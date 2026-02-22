/*
 * SleepyTime — motion_service.h
 *
 * Owns accelerometer configuration and exposes motion_iface_t for
 * dependency inversion. power_service calls configure_wakeup() through
 * this interface before entering System Off — it never touches sensor
 * registers directly.
 *
 * Swapping LIS3DH for another sensor means:
 *   1. New driver in drivers/
 *   2. New motion_iface_t implementation in motion_service.c
 *   3. One pointer change in main.c
 *   power_service.c is untouched.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef MOTION_SERVICE_H
#define MOTION_SERVICE_H

#include <stdbool.h>

/*
 * motion_iface_t — hardware abstraction for wrist-raise wakeup config.
 *
 * Implemented by motion_service.c (LIS3DH) on real hardware.
 * Stubbed in tests. Alternative implementations (e.g. BMI270) drop in
 * without changing power_service.c.
 */
typedef struct {
  /*
   * configure_wakeup() — arm the sensor to assert its interrupt line
   * when wrist-raise activity is detected.
   *
   * Called immediately before sys_poweroff(). The sensor's INT line
   * must remain asserted while the wrist stays raised so the nRF54L15
   * GPIO sense latch fires.
   *
   * Return: 0 on success, negative errno on failure.
   */
  int (*configure_wakeup)(void);

  /*
   * clear() — deassert the interrupt line and reset the sensor's
   * activity latch after a wakeup.
   *
   * Called during power_service_init() on a wrist-raise boot so that
   * the INT line is low before any new sleep cycle is armed.
   *
   * Return: 0 on success, negative errno on failure.
   */
  int (*clear)(void);
} motion_iface_t;

/* ── Service API ────────────────────────────────────────────────────────────
 */

/**
 * motion_service_init() - initialise the accelerometer driver.
 *
 * Configures the sensor for activity detection but does NOT arm the
 * interrupt yet — that is deferred to configure_wakeup() just before
 * System Off to avoid spurious wakeups during active use.
 */
void motion_service_init(void);

/**
 * motion_service_iface() - return pointer to the motion_iface_t
 * implementation for this sensor.
 *
 * main.c passes this to power_service_init() during wiring.
 */
const motion_iface_t *motion_service_iface(void);

#endif /* MOTION_SERVICE_H */
