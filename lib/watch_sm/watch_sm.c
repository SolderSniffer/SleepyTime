/*
 * SleepyTime — watch_sm.c
 *
 * Event-driven state machine. No Zephyr / hardware dependencies.
 *
 * Transition table summary:
 *
 *  BOOTING
 *    BOOT_DONE           → DISPLAY_UPDATE  (always)
 *
 *  DISPLAY_UPDATE
 *    DISPLAY_READY       → ACTIVE          (normal wakeup)
 *    DISPLAY_READY       → SLEEP_PENDING   (RTC tick wakeup, no interaction)
 *    LOW_BATTERY         → LOW_BATTERY     (render warning instead)
 *
 *  ACTIVE
 *    WRIST_LOWERED       → SLEEP_PENDING
 *    TICK                → DISPLAY_UPDATE
 *    SYNC_REQUESTED      → BLE_SYNC
 *    BLE_CONNECTED       → BLE_SYNC
 *    LOW_BATTERY         → LOW_BATTERY
 *
 *  BLE_SYNC
 *    BLE_TIME_SYNCED     → DISPLAY_UPDATE
 *    BLE_DISCONNECTED    → DISPLAY_UPDATE  (use existing time)
 *
 *  SLEEP_PENDING
 *    DISPLAY_READY       → SLEEPING
 *
 *  LOW_BATTERY
 *    DISPLAY_READY       → SLEEPING        (immediately sleep after warning)
 *    BATTERY_OK          → DISPLAY_UPDATE  (charging detected)
 *
 *  SLEEPING
 *    (terminal — enter_sleep() does not return)
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "watch_sm.h"
#include "watch_time.h"

#include <stddef.h>
#include <string.h>

/* ── Internal forward declarations ───────────────────────────────────────── */

static void enter_state(watch_sm_t *sm, watch_state_t next);
static void handle_booting(watch_sm_t *sm, watch_event_t event);
static void handle_display_update(watch_sm_t *sm, watch_event_t event);
static void handle_active(watch_sm_t *sm, watch_event_t event);
static void handle_ble_sync(watch_sm_t *sm, watch_event_t event);
static void handle_sleep_pending(watch_sm_t *sm, watch_event_t event);
static void handle_sleeping(watch_sm_t *sm, watch_event_t event);
static void handle_low_battery(watch_sm_t *sm, watch_event_t event);

/* ── Default fallback time (cold boot with no valid RTC) ─────────────────── */

static const watch_time_t k_default_time = {
    .year = 2025,
    .month = 1,
    .day = 1,
    .hour = 0,
    .minute = 0,
    .second = 0,
    .weekday = 3, /* Wednesday */
};

/* ── Battery threshold ──────────────────────────────────────────────────────
 */

#define BATTERY_LOW_PCT 10U
#define BATTERY_CRIT_PCT 5U

/* ── Public API ─────────────────────────────────────────────────────────────
 */

void watch_sm_init(watch_sm_t *sm, const watch_sm_iface_t *iface) {
  if (sm == NULL || iface == NULL) {
    return;
  }
  memset(sm, 0, sizeof(*sm));
  sm->iface = iface;
  sm->state = WATCH_STATE_BOOTING;
  sm->battery_pct = 100U;
}

void watch_sm_on_boot(watch_sm_t *sm, watch_boot_reason_t reason,
                      const watch_time_t *restored_time) {
  if (sm == NULL) {
    return;
  }

  sm->boot_reason = reason;

  if (restored_time != NULL && watch_time_is_valid(restored_time)) {
    sm->time = *restored_time;
  } else {
    sm->time = k_default_time;
    sm->iface->log(
        "watch_sm: cold boot — no valid restored time, using default");
  }

  sm->iface->log("watch_sm: boot reason=%s time=%04u-%02u-%02u %02u:%02u",
                 (reason == WATCH_BOOT_WRIST_RAISE) ? "WRIST_RAISE"
                 : (reason == WATCH_BOOT_RTC_ALARM) ? "RTC_ALARM"
                 : (reason == WATCH_BOOT_WATCHDOG)  ? "WATCHDOG"
                                                    : "COLD",
                 sm->time.year, sm->time.month, sm->time.day, sm->time.hour,
                 sm->time.minute);

  watch_sm_event(sm, WATCH_EVT_BOOT_DONE);
}

void watch_sm_event(watch_sm_t *sm, watch_event_t event) {
  if (sm == NULL || sm->iface == NULL) {
    return;
  }
  if (event >= WATCH_EVT_COUNT) {
    sm->iface->log("watch_sm: unknown event %d ignored", (int)event);
    return;
  }

  sm->iface->log("watch_sm: [%s] + %s", watch_sm_state_name(sm->state),
                 watch_sm_event_name(event));

  switch (sm->state) {
  case WATCH_STATE_BOOTING:
    handle_booting(sm, event);
    break;
  case WATCH_STATE_DISPLAY_UPDATE:
    handle_display_update(sm, event);
    break;
  case WATCH_STATE_ACTIVE:
    handle_active(sm, event);
    break;
  case WATCH_STATE_BLE_SYNC:
    handle_ble_sync(sm, event);
    break;
  case WATCH_STATE_SLEEP_PENDING:
    handle_sleep_pending(sm, event);
    break;
  case WATCH_STATE_SLEEPING:
    handle_sleeping(sm, event);
    break;
  case WATCH_STATE_LOW_BATTERY:
    handle_low_battery(sm, event);
    break;
  default:
    sm->iface->log("watch_sm: unhandled state %d", (int)sm->state);
    break;
  }
}

void watch_sm_on_time_sync(watch_sm_t *sm, const watch_time_t *new_time) {
  if (sm == NULL || new_time == NULL) {
    watch_sm_event(sm, WATCH_EVT_BLE_DISCONNECTED);
    return;
  }

  watch_sync_result_t result = watch_time_apply_sync(&sm->time, new_time);

  if (result == WATCH_SYNC_OK) {
    sm->iface->log("watch_sm: time sync accepted %04u-%02u-%02u %02u:%02u:%02u",
                   sm->time.year, sm->time.month, sm->time.day, sm->time.hour,
                   sm->time.minute, sm->time.second);
    watch_sm_event(sm, WATCH_EVT_BLE_TIME_SYNCED);
  } else {
    sm->iface->log("watch_sm: time sync rejected (result=%d)", (int)result);
    watch_sm_event(sm, WATCH_EVT_BLE_DISCONNECTED);
  }
}

watch_state_t watch_sm_state(const watch_sm_t *sm) {
  if (sm == NULL) {
    return WATCH_STATE_BOOTING;
  }
  return sm->state;
}

const char *watch_sm_state_name(watch_state_t state) {
  switch (state) {
  case WATCH_STATE_BOOTING:
    return "BOOTING";
  case WATCH_STATE_DISPLAY_UPDATE:
    return "DISPLAY_UPDATE";
  case WATCH_STATE_ACTIVE:
    return "ACTIVE";
  case WATCH_STATE_BLE_SYNC:
    return "BLE_SYNC";
  case WATCH_STATE_SLEEP_PENDING:
    return "SLEEP_PENDING";
  case WATCH_STATE_SLEEPING:
    return "SLEEPING";
  case WATCH_STATE_LOW_BATTERY:
    return "LOW_BATTERY";
  default:
    return "UNKNOWN";
  }
}

const char *watch_sm_event_name(watch_event_t event) {
  switch (event) {
  case WATCH_EVT_BOOT_DONE:
    return "BOOT_DONE";
  case WATCH_EVT_DISPLAY_READY:
    return "DISPLAY_READY";
  case WATCH_EVT_WRIST_RAISED:
    return "WRIST_RAISED";
  case WATCH_EVT_WRIST_LOWERED:
    return "WRIST_LOWERED";
  case WATCH_EVT_BLE_CONNECTED:
    return "BLE_CONNECTED";
  case WATCH_EVT_BLE_TIME_SYNCED:
    return "BLE_TIME_SYNCED";
  case WATCH_EVT_BLE_DISCONNECTED:
    return "BLE_DISCONNECTED";
  case WATCH_EVT_TICK:
    return "TICK";
  case WATCH_EVT_LOW_BATTERY:
    return "LOW_BATTERY";
  case WATCH_EVT_BATTERY_OK:
    return "BATTERY_OK";
  case WATCH_EVT_SYNC_REQUESTED:
    return "SYNC_REQUESTED";
  default:
    return "UNKNOWN";
  }
}

/* ── State entry actions ────────────────────────────────────────────────────
 */

static void enter_state(watch_sm_t *sm, watch_state_t next) {
  sm->iface->log("watch_sm: → %s", watch_sm_state_name(next));
  sm->state = next;

  switch (next) {

  case WATCH_STATE_DISPLAY_UPDATE:
    sm->battery_pct = sm->iface->get_battery_pct();
    sm->iface->display_time(&sm->time, sm->battery_pct);
    break;

  case WATCH_STATE_BLE_SYNC:
    sm->iface->display_sync_screen();
    sm->iface->ble_start();
    break;

  case WATCH_STATE_SLEEP_PENDING:
    sm->iface->save_boot_state(&sm->time);
    sm->iface->ble_stop();
    /*
     * Final EPD refresh was already requested by whatever state preceded
     * this one. We wait for DISPLAY_READY before calling enter_sleep().
     */
    break;

  case WATCH_STATE_SLEEPING:
    sm->iface->log("watch_sm: entering System Off");
    sm->iface->enter_sleep(); /* does not return */
    break;

  case WATCH_STATE_LOW_BATTERY:
    sm->battery_pct = sm->iface->get_battery_pct();
    sm->iface->display_low_battery(sm->battery_pct);
    break;

  case WATCH_STATE_ACTIVE:
    /*
     * No entry action — display is already showing current time from the
     * preceding DISPLAY_UPDATE state.
     */
    break;

  default:
    break;
  }
}

/* ── State handlers ─────────────────────────────────────────────────────────
 */

static void handle_booting(watch_sm_t *sm, watch_event_t event) {
  switch (event) {
  case WATCH_EVT_BOOT_DONE:
    sm->battery_pct = sm->iface->get_battery_pct();
    if (sm->battery_pct <= BATTERY_CRIT_PCT) {
      enter_state(sm, WATCH_STATE_LOW_BATTERY);
    } else {
      enter_state(sm, WATCH_STATE_DISPLAY_UPDATE);
    }
    break;

  default:
    /* All other events are ignored during boot. */
    break;
  }
}

static void handle_display_update(watch_sm_t *sm, watch_event_t event) {
  switch (event) {
  case WATCH_EVT_DISPLAY_READY:
    /*
     * If we woke from a wrist raise, stay ACTIVE so the user can
     * interact. For an RTC tick wakeup with no interaction expected,
     * go straight back to sleep.
     */
    if (sm->boot_reason == WATCH_BOOT_RTC_ALARM && !sm->ble_sync_pending) {
      enter_state(sm, WATCH_STATE_SLEEP_PENDING);
    } else {
      enter_state(sm, WATCH_STATE_ACTIVE);
    }
    break;

  case WATCH_EVT_LOW_BATTERY:
    enter_state(sm, WATCH_STATE_LOW_BATTERY);
    break;

  default:
    break;
  }
}

static void handle_active(watch_sm_t *sm, watch_event_t event) {
  switch (event) {
  case WATCH_EVT_WRIST_LOWERED:
    enter_state(sm, WATCH_STATE_SLEEP_PENDING);
    break;

  case WATCH_EVT_TICK:
    watch_time_add_seconds(&sm->time, 60U);
    enter_state(sm, WATCH_STATE_DISPLAY_UPDATE);
    break;

  case WATCH_EVT_SYNC_REQUESTED:
  case WATCH_EVT_BLE_CONNECTED:
    sm->ble_sync_pending = true;
    enter_state(sm, WATCH_STATE_BLE_SYNC);
    break;

  case WATCH_EVT_LOW_BATTERY:
    enter_state(sm, WATCH_STATE_LOW_BATTERY);
    break;

  case WATCH_EVT_WRIST_RAISED:
    /*
     * Already active — re-render to acknowledge the gesture and
     * reset any inactivity timer in the service layer.
     */
    enter_state(sm, WATCH_STATE_DISPLAY_UPDATE);
    break;

  default:
    break;
  }
}

static void handle_ble_sync(watch_sm_t *sm, watch_event_t event) {
  switch (event) {
  case WATCH_EVT_BLE_TIME_SYNCED:
    sm->ble_sync_pending = false;
    sm->iface->ble_stop();
    enter_state(sm, WATCH_STATE_DISPLAY_UPDATE);
    break;

  case WATCH_EVT_BLE_DISCONNECTED:
    sm->ble_sync_pending = false;
    sm->iface->ble_stop();
    /*
     * Sync failed — update display with existing time, go back to active
     * so the user can see the watch face.
     */
    enter_state(sm, WATCH_STATE_DISPLAY_UPDATE);
    break;

  case WATCH_EVT_LOW_BATTERY:
    sm->iface->ble_stop();
    enter_state(sm, WATCH_STATE_LOW_BATTERY);
    break;

  default:
    break;
  }
}

static void handle_sleep_pending(watch_sm_t *sm, watch_event_t event) {
  switch (event) {
  case WATCH_EVT_DISPLAY_READY:
    enter_state(sm, WATCH_STATE_SLEEPING);
    break;

  case WATCH_EVT_WRIST_RAISED:
    /*
     * User raised wrist while we were preparing to sleep.
     * Cancel sleep and go back to showing the time.
     */
    sm->boot_reason = WATCH_BOOT_WRIST_RAISE;
    enter_state(sm, WATCH_STATE_DISPLAY_UPDATE);
    break;

  default:
    break;
  }
}

static void handle_sleeping(watch_sm_t *sm, watch_event_t event) {
  /*
   * This state should be terminal — enter_sleep() does not return.
   * If somehow we end up here again, attempt re-entry.
   */
  (void)event;
  sm->iface->log("watch_sm: re-entering sleep from SLEEPING state");
  sm->iface->enter_sleep();
}

static void handle_low_battery(watch_sm_t *sm, watch_event_t event) {
  switch (event) {
  case WATCH_EVT_DISPLAY_READY:
    /* Warning shown — sleep immediately to protect the battery */
    enter_state(sm, WATCH_STATE_SLEEP_PENDING);
    break;

  case WATCH_EVT_BATTERY_OK:
    /* Charging detected */
    enter_state(sm, WATCH_STATE_DISPLAY_UPDATE);
    break;

  default:
    break;
  }
}
