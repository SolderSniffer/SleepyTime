/*
 * SleepyTime — main.c
 *
 * Entry point for the nRF54L15 Zephyr target.
 *
 * Responsibilities (and nothing more):
 *   1. Decode reset cause to a watch_boot_reason_t.
 *   2. Read restored time from PCF8563 RTC or GPREGRET.
 *   3. Wire real driver/service calls into watch_sm_iface_t.
 *   4. Initialise the state machine and call watch_sm_on_boot().
 *   5. Pump events from the ISR/service event queue to watch_sm_event().
 *
 * No business logic lives here. If you find yourself adding conditionals
 * that aren't about wiring or init sequencing, they belong in watch_sm.c
 * or a service module instead.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/hwinfo.h> /* hwinfo_get_reset_cause() / hwinfo_clear_reset_cause() */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>

/* Service / driver headers */
#include "ble_service.h"
#include "display_service.h"
#include "power_service.h"
#include "time_service.h"

/* Pure-logic library headers */
#include "watch_sm.h"
#include "watch_time.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

/* ── Event queue ────────────────────────────────────────────────────────────
 */
/*
 * ISRs and service callbacks post watch_event_t values here.
 * main() drains this queue and calls watch_sm_event() from task context.
 * Queue depth of 8 is conservative — only a handful of events can be
 * outstanding at any time in normal operation.
 */
#define EVENT_QUEUE_DEPTH 8

K_MSGQ_DEFINE(g_event_queue, sizeof(watch_event_t), EVENT_QUEUE_DEPTH,
              sizeof(watch_event_t));

/* ── Global state machine instance ───────────────────────────────────────── */

static watch_sm_t g_sm;

/* ── Helpers ────────────────────────────────────────────────────────────────
 */

/**
 * Post an event from any context (task or ISR).
 * Drops the event and logs a warning if the queue is full.
 */
void sleepytime_post_event(watch_event_t event) {
  if (k_msgq_put(&g_event_queue, &event, K_NO_WAIT) != 0) {
    LOG_WRN("Event queue full — dropped event %s", watch_sm_event_name(event));
  }
}

/* ── watch_sm_iface_t implementations ────────────────────────────────────── */

static void iface_display_time(const watch_time_t *t, uint8_t battery_pct) {
  display_service_show_watch_face(t, battery_pct);
  /* display_service posts WATCH_EVT_DISPLAY_READY when BUSY deasserts */
}

static void iface_display_sync_screen(void) {
  display_service_show_sync_screen();
}

static void iface_display_low_battery(uint8_t battery_pct) {
  display_service_show_low_battery(battery_pct);
}

static uint8_t iface_get_battery_pct(void) {
  return power_service_battery_pct();
}

static void iface_ble_start(void) { ble_service_start(); }

static void iface_ble_stop(void) { ble_service_stop(); }

static void iface_save_boot_state(const watch_time_t *t) {
  time_service_save_boot_state(t);
}

static void iface_enter_sleep(void) {
  power_service_enter_system_off();
  /* Does not return. */
  CODE_UNREACHABLE;
}

static void iface_log(const char *fmt, ...) {
  /*
   * Zephyr's logging macros don't accept va_list directly so this is a
   * no-op stub — the state machine's log calls are informational only.
   * Each service uses LOG_INF/LOG_DBG directly for its own messages.
   */
  (void)fmt;
}

static const watch_sm_iface_t k_iface = {
    .display_time = iface_display_time,
    .display_sync_screen = iface_display_sync_screen,
    .display_low_battery = iface_display_low_battery,
    .get_battery_pct = iface_get_battery_pct,
    .ble_start = iface_ble_start,
    .ble_stop = iface_ble_stop,
    .save_boot_state = iface_save_boot_state,
    .enter_sleep = iface_enter_sleep,
    .log = iface_log,
};

/* ── Reset cause decode ─────────────────────────────────────────────────────
 */

static watch_boot_reason_t decode_reset_reason(void) {
  uint32_t cause = 0;

  /*
   * hwinfo_get_reset_cause() is the Zephyr-portable way to read reset
   * reason — it maps to RESETREAS on nRF targets without requiring a
   * direct MDK register include.
   *
   * Relevant bits (zephyr/drivers/hwinfo.h):
   *   RESET_PIN        — RESETPIN / cold boot
   *   RESET_WATCHDOG   — watchdog
   *   RESET_LOW_POWER_WAKE — woke from System Off via GPIO sense / RTC
   */
  int rc = hwinfo_get_reset_cause(&cause);
  if (rc != 0) {
    LOG_WRN("hwinfo_get_reset_cause failed (%d) — treating as cold boot", rc);
    return WATCH_BOOT_COLD;
  }

  hwinfo_clear_reset_cause();

  LOG_DBG("Reset cause register: 0x%08x", cause);

  if (cause & RESET_LOW_POWER_WAKE) {
    /*
     * Woke from System Off. Distinguish LIS3DH INT1 (wrist raise)
     * from RTC alarm by checking which GPIO sense line fired —
     * delegated to power_service which knows the pin assignments.
     */
    return power_service_wakeup_source();
  }
  if (cause & RESET_WATCHDOG) {
    LOG_ERR("Watchdog reset detected");
    return WATCH_BOOT_WATCHDOG;
  }

  /* RESET_PIN, power-on, or unrecognised — treat as cold boot */
  return WATCH_BOOT_COLD;
}

/* ── BLE time-sync callback (called from ble_service on CTS write) ──────────
 */

void ble_service_on_time_sync(const watch_time_t *new_time) {
  /*
   * Called from BLE stack callback context — hand the new time to the SM
   * which validates it and fires the appropriate event internally.
   */
  watch_sm_on_time_sync(&g_sm, new_time);
}

/* ── main ───────────────────────────────────────────────────────────────────
 */

int main(void) {
  LOG_INF("SleepyTime starting — board: %s", CONFIG_BOARD);

  /* 1. Decode reset reason before any service init can disturb it */
  watch_boot_reason_t boot_reason = decode_reset_reason();

  /* 2. Initialise services (does not start BLE or EPD refresh yet) */
  display_service_init();
  time_service_init();
  power_service_init();
  ble_service_init();

  /* 3. Read restored time from RTC / GPREGRET */
  watch_time_t restored_time = {0};
  bool time_valid = time_service_read(&restored_time);
  (void)time_valid; /* watch_sm_on_boot() handles invalid time gracefully */

  /* 4. Wire interface and initialise state machine */
  watch_sm_init(&g_sm, &k_iface);

  /* 5. Drive first transition — this starts the EPD refresh */
  watch_sm_on_boot(&g_sm, boot_reason, &restored_time);

  /* 6. Event loop — drain the queue and pump the state machine */
  watch_event_t event;
  while (true) {
    int rc = k_msgq_get(&g_event_queue, &event, K_FOREVER);
    if (rc == 0) {
      watch_sm_event(&g_sm, event);
    }
  }

  /* Unreachable — System Off is entered inside the state machine */
  return 0;
}