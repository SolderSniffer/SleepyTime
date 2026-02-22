/*
 * SleepyTime — watch_sm.h
 *
 * Event-driven state machine for the SleepyTime watch.
 *
 * Design principles:
 *   - Zero Zephyr / hardware dependencies. Compiles on host for unit testing.
 *   - All hardware interactions are mediated through watch_sm_iface_t function
 *     pointers, injected at init time. Tests stub these; main.c wires real
 *     driver calls.
 *   - State transitions are deterministic given (current_state, event) pairs
 *     and are fully enumerable in unit tests.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef WATCH_SM_H
#define WATCH_SM_H

#include "watch_time.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── States ─────────────────────────────────────────────────────────────────
 */

typedef enum {
  WATCH_STATE_BOOTING = 0,    /**< Entry state: decode RESETREAS, load boot state */
  WATCH_STATE_DISPLAY_UPDATE, /**< Render current time to EPD */
  WATCH_STATE_ACTIVE,         /**< Display is current; waiting for next event   */
  WATCH_STATE_BLE_SYNC,       /**< BLE connection open, waiting for CTS write   */
  WATCH_STATE_SLEEP_PENDING,  /**< EPD refresh complete; prepare for System Off
                               */
  WATCH_STATE_SLEEPING,       /**< Terminal: System Off entry called             */
  WATCH_STATE_LOW_BATTERY,    /**< Battery too low for normal operation          */

  WATCH_STATE_COUNT,
} watch_state_t;

/* ── Events ─────────────────────────────────────────────────────────────────
 */

typedef enum {
  WATCH_EVT_BOOT_DONE = 0,    /**< Boot state loaded, init complete              */
  WATCH_EVT_DISPLAY_READY,    /**< EPD refresh complete (BUSY line deasserted) */
  WATCH_EVT_WRIST_RAISED,     /**< LIS3DH activity interrupt fired               */
  WATCH_EVT_WRIST_LOWERED,    /**< Inactivity timeout elapsed                    */
  WATCH_EVT_BLE_CONNECTED,    /**< BLE link established                          */
  WATCH_EVT_BLE_TIME_SYNCED,  /**< CTS write received and validated  */
  WATCH_EVT_BLE_DISCONNECTED, /**< BLE link lost or timed out */
  WATCH_EVT_TICK,             /**< Periodic 1-minute tick (from RTC or soft timer)*/
  WATCH_EVT_LOW_BATTERY,      /**< Fuel gauge threshold crossed                  */
  WATCH_EVT_BATTERY_OK,       /**< Battery level recovered (charging)            */
  WATCH_EVT_SYNC_REQUESTED,   /**< User or system requests a BLE sync */

  WATCH_EVT_COUNT,
} watch_event_t;

/* ── Boot reason ────────────────────────────────────────────────────────────
 */

/**
 * Decoded wakeup / reset cause, populated from RESETREAS by the boot service
 * and passed in via watch_sm_on_boot().
 */
typedef enum {
  WATCH_BOOT_COLD = 0,    /**< Power-on or hard reset                        */
  WATCH_BOOT_WRIST_RAISE, /**< GPIO sense wakeup from LIS3DH INT1            */
  WATCH_BOOT_RTC_ALARM,   /**< RTC alarm wakeup (PCF8563 or MCU RTC)         */
  WATCH_BOOT_WATCHDOG,    /**< Watchdog reset — log and treat as cold        */
} watch_boot_reason_t;

/* ── Hardware interface ─────────────────────────────────────────────────────
 */

/**
 * All hardware interactions the state machine may initiate.
 * Inject real implementations in main.c; inject stubs in unit tests.
 *
 * Every function pointer must be non-NULL at watch_sm_init() time.
 */
typedef struct {
  /**
   * Render the watch face for @p t with @p battery_pct% charge.
   * Asynchronous: implementation should start an EPD refresh and return.
   * Fires WATCH_EVT_DISPLAY_READY when BUSY deasserts.
   */
  void (*display_time)(const watch_time_t *t, uint8_t battery_pct);

  /**
   * Render the BLE sync / pairing screen.
   */
  void (*display_sync_screen)(void);

  /**
   * Render the low-battery warning screen with @p battery_pct%.
   */
  void (*display_low_battery)(uint8_t battery_pct);

  /**
   * Return the current battery percentage (0–100).
   * Called synchronously inside state transitions.
   */
  uint8_t (*get_battery_pct)(void);

  /**
   * Start BLE advertising and attempt to connect to a bonded peer.
   * Fires WATCH_EVT_BLE_CONNECTED or WATCH_EVT_BLE_DISCONNECTED.
   */
  void (*ble_start)(void);

  /**
   * Stop BLE and release radio resources.
   */
  void (*ble_stop)(void);

  /**
   * Persist boot state (time + display state) to GPREGRET / retained RAM.
   * Called before entering sleep.
   */
  void (*save_boot_state)(const watch_time_t *t);

  /**
   * Configure GPIO sense for LIS3DH INT1 and RTC alarm, then enter
   * System Off. Does not return.
   */
  void (*enter_sleep)(void);

  /**
   * Log a message (maps to LOG_INF on target, printf on host/sim).
   * @p fmt is a printf-style format string.
   */
  void (*log)(const char *fmt, ...);
} watch_sm_iface_t;

/* ── State machine context ──────────────────────────────────────────────────
 */

/**
 * Opaque state machine context. Embed statically — do not heap-allocate.
 * Initialised by watch_sm_init().
 */
typedef struct {
  watch_state_t state;
  watch_time_t time;
  uint8_t battery_pct;
  watch_boot_reason_t boot_reason;
  bool ble_sync_pending;
  const watch_sm_iface_t *iface;
} watch_sm_t;

/* ── Public API ─────────────────────────────────────────────────────────────
 */

/**
 * @brief Initialise the state machine.
 *
 * Must be called once before any other watch_sm_* function.
 * @p iface must have all function pointers populated.
 */
void watch_sm_init(watch_sm_t *sm, const watch_sm_iface_t *iface);

/**
 * @brief Notify the state machine of the boot reason and restored time.
 *
 * Call immediately after watch_sm_init(). This drives the first
 * WATCH_EVT_BOOT_DONE transition internally.
 *
 * @param reason  Decoded RESETREAS value.
 * @param restored_time  Time restored from GPREGRET / RTC. May be invalid
 *                       (watch_time_is_valid() == false) on a cold boot.
 */
void watch_sm_on_boot(watch_sm_t *sm, watch_boot_reason_t reason,
                      const watch_time_t *restored_time);

/**
 * @brief Deliver an event to the state machine.
 *
 * This is the main entry point for all runtime events. Safe to call from
 * task context; not ISR-safe (post events via a queue from ISR context).
 */
void watch_sm_event(watch_sm_t *sm, watch_event_t event);

/**
 * @brief Deliver a time-sync event with the new time value.
 *
 * Convenience wrapper: validates @p new_time, applies it, and fires
 * WATCH_EVT_BLE_TIME_SYNCED (or WATCH_EVT_BLE_DISCONNECTED on invalid time).
 */
void watch_sm_on_time_sync(watch_sm_t *sm, const watch_time_t *new_time);

/**
 * @brief Return the current state (for logging / diagnostics).
 */
watch_state_t watch_sm_state(const watch_sm_t *sm);

/**
 * @brief Return a human-readable name for a state (for logging).
 */
const char *watch_sm_state_name(watch_state_t state);

/**
 * @brief Return a human-readable name for an event (for logging).
 */
const char *watch_sm_event_name(watch_event_t event);

#ifdef __cplusplus
}
#endif

#endif /* WATCH_SM_H */
