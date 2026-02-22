/*
 * SleepyTime — main.c
 *
 * Entry point for the nRF54L15 Zephyr target.
 *
 * Responsibilities (and nothing more):
 *   1. Decode reset cause to a watch_boot_reason_t.
 *   2. Initialise services and wire interface structs.
 *   3. Read restored time and boot the state machine.
 *   4. Pump events from the ISR/service queue to the state machine.
 *
 * Wiring rules:
 *   - To swap the wakeup timer (GRTC → PCF8563): change k_wakeup_iface.
 *   - To swap the motion sensor (LIS3DH → BMI270): change k_motion_iface.
 *   - No other file needs to change.
 *
 * No business logic lives here. If you find yourself adding conditionals
 * that are not about wiring or init sequencing, they belong in watch_sm.c
 * or a service module instead.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/hwinfo.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>

/* Service headers */
#include "ble_service.h"
#include "display_service.h"
#include "motion_service.h"
#include "power_service.h"
#include "time_service.h"
#include "wakeup_grtc.h"

/* Pure-logic library headers */
#include "watch_sm.h"
#include "watch_time.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

/* ── Event queue ────────────────────────────────────────────────────────────
 */

#define EVENT_QUEUE_DEPTH 8

K_MSGQ_DEFINE(g_event_queue, sizeof(watch_event_t), EVENT_QUEUE_DEPTH,
              sizeof(watch_event_t));

/* ── Global state machine instance ───────────────────────────────────────── */

static watch_sm_t g_sm;

/* ── Event posting ──────────────────────────────────────────────────────────
 */

void sleepytime_post_event(watch_event_t event) {
  if (k_msgq_put(&g_event_queue, &event, K_NO_WAIT) != 0) {
    LOG_WRN("Event queue full — dropped %s", watch_sm_event_name(event));
  }
}

/* ── watch_sm_iface_t implementations ────────────────────────────────────── */

static void iface_display_time(const watch_time_t *t, uint8_t battery_pct) {
  display_service_show_watch_face(t, battery_pct);
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
  CODE_UNREACHABLE;
}

static void iface_log(const char *fmt, ...) {
  /* Zephyr logging macros do not accept va_list — no-op stub. */
  (void)fmt;
}

static const watch_sm_iface_t k_sm_iface = {
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

  int rc = hwinfo_get_reset_cause(&cause);
  if (rc != 0) {
    LOG_WRN("hwinfo_get_reset_cause failed (%d) — cold boot", rc);
    return WATCH_BOOT_COLD;
  }

  hwinfo_clear_reset_cause();
  LOG_DBG("Reset cause: 0x%08x", cause);

  /*
   * Errata [48]: RESETREAS wakeup field is unreliable when a hard reset
   * bit is also set. Treat the combination as a cold boot.
   */
  const uint32_t hard_reset = RESET_PIN | RESET_WATCHDOG | RESET_DEBUG;
  const uint32_t wakeup = RESET_CLOCK | RESET_LOW_POWER_WAKE;

  if ((cause & hard_reset) && (cause & wakeup)) {
    LOG_WRN("Errata [48]: hard+wakeup bits set (0x%08x) — cold boot", cause);
    return WATCH_BOOT_COLD;
  }

  if (cause & RESET_CLOCK) {
    /* GRTC alarm fired — periodic minute tick */
    LOG_DBG("Wakeup: GRTC alarm");
    return WATCH_BOOT_RTC_ALARM;
  }

  if (cause & RESET_LOW_POWER_WAKE) {
    /* GPIO sense fired — delegated to power_service */
    LOG_DBG("Wakeup: GPIO sense");
    return power_service_wakeup_source();
  }

  if (cause & RESET_WATCHDOG) {
    LOG_ERR("Watchdog reset");
    return WATCH_BOOT_WATCHDOG;
  }

  return WATCH_BOOT_COLD;
}

/* ── BLE time-sync callback ─────────────────────────────────────────────────
 */

void ble_service_on_time_sync(const watch_time_t *new_time) {
  watch_sm_on_time_sync(&g_sm, new_time);
}

/* ── main ───────────────────────────────────────────────────────────────────
 */

int main(void) {
  LOG_INF("SleepyTime starting — board: %s", CONFIG_BOARD);

  /* 1. Decode reset reason before any service init disturbs the register */
  watch_boot_reason_t boot_reason = decode_reset_reason();

  /* 2. Initialise services
   *
   * Wiring: swap wakeup timer or motion sensor here by changing which
   * iface() function is passed to power_service_init().
   *
   *   GRTC wakeup:    wakeup_grtc_iface()      ← current
   *   PCF8563 wakeup: wakeup_pcf8563_iface()   ← future alternative
   *
   *   LIS3DH motion:  motion_service_iface()   ← current
   *   BMI270 motion:  bmi270_service_iface()   ← future alternative
   */
  motion_service_init();
  power_service_init(wakeup_grtc_iface(), motion_service_iface());

  display_service_init();
  time_service_init();
  ble_service_init();

  /* 3. Read restored time from RTC / GPREGRET */
  watch_time_t restored_time = {0};
  bool time_valid = time_service_read(&restored_time);
  (void)time_valid; /* SM handles invalid time gracefully */

  /* 4. Initialise and boot the state machine */
  watch_sm_init(&g_sm, &k_sm_iface);
  watch_sm_on_boot(&g_sm, boot_reason, &restored_time);

  /* 5. Event loop */
  watch_event_t event;
  while (true) {
    if (k_msgq_get(&g_event_queue, &event, K_FOREVER) == 0) {
      watch_sm_event(&g_sm, event);
    }
  }

  return 0;
}
