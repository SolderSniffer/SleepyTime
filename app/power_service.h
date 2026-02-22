/*
 * SleepyTime — power_service.h
 *
 * Manages System Off entry and battery reads.
 *
 * power_service is hardware-agnostic with respect to wakeup sources.
 * It calls through two interface structs injected at init time:
 *
 *   wakeup_timer_iface_t  — arm/disarm the periodic wakeup timer.
 *                           Default impl: wakeup_grtc.c (GRTC).
 *                           Alternative:  wakeup_pcf8563.c (PCF8563 /INT).
 *                           Swap by changing one pointer in main.c.
 *
 *   motion_iface_t        — configure accelerometer GPIO sense wakeup.
 *                           Default impl: motion_service.c (LIS3DH).
 *                           Swap by changing one pointer in main.c.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef POWER_SERVICE_H
#define POWER_SERVICE_H

#include "motion_service.h"
#include "watch_sm.h"
#include <stdint.h>

/*
 * wakeup_timer_iface_t — hardware abstraction for the periodic wakeup timer.
 *
 * Decouples power_service from the specific timer peripheral (GRTC, PCF8563,
 * etc.). Implemented in wakeup_grtc.c for the nRF54L15 internal GRTC.
 */
typedef struct {
  /**
   * arm() - schedule a wakeup @seconds from now.
   *
   * Called immediately before sys_poweroff(). Must translate the relative
   * delay into whatever absolute timestamp the underlying hardware requires.
   *
   * Return: 0 on success, negative errno on failure.
   */
  int (*arm)(uint32_t seconds);

  /**
   * disarm() - cancel any pending wakeup.
   *
   * Called if sleep is cancelled (e.g. wrist raise during SLEEP_PENDING).
   * Implementations that lack an explicit cancel may push the wakeup time
   * far into the future as an approximation.
   */
  void (*disarm)(void);
} wakeup_timer_iface_t;

/* ── Service API ────────────────────────────────────────────────────────────
 */

/**
 * power_service_init() - initialise the power service.
 *
 * @wakeup: wakeup timer interface (e.g. wakeup_grtc_iface()). Must not
 *          be NULL.
 * @motion: motion/accelerometer interface (e.g. motion_service_iface()).
 *          May be NULL — GRTC-only wakeup will be used.
 *
 * Stores both interface pointers for use by power_service_enter_system_off().
 * Does not arm either wakeup source — that is deferred to sleep entry.
 */
void power_service_init(const wakeup_timer_iface_t *wakeup,
                        const motion_iface_t *motion);

/**
 * power_service_battery_pct() - return battery level 0–100.
 *
 * TODO: implement ADC fuel gauge read.
 */
uint8_t power_service_battery_pct(void);

/**
 * power_service_wakeup_source() - identify which source caused a System Off
 * wakeup.
 *
 * Called from main.c when hwinfo reports RESET_LOW_POWER_WAKE. Returns the
 * appropriate watch_boot_reason_t for the SM.
 */
watch_boot_reason_t power_service_wakeup_source(void);

/**
 * power_service_enter_system_off() - arm wakeup sources and enter System Off.
 *
 * Arms the wakeup timer and configures GPIO sense via the injected interfaces,
 * then calls sys_poweroff(). Does not return.
 */
void power_service_enter_system_off(void);

#endif /* POWER_SERVICE_H */
