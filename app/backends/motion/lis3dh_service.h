/*
 * SleepyTime — lis3dh_service.h
 *
 * LIS3DH accelerometer backend implementing motion_iface_t.
 *
 * This service owns LIS3DH-specific configuration and exposes
 * motion_iface_t for dependency inversion. power_service calls
 * configure_wakeup() through this interface before entering System Off.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LIS3DH_SERVICE_H
#define LIS3DH_SERVICE_H

#include <stdbool.h>

/*
 * motion_iface_t — hardware abstraction for wrist-raise wakeup config.
 *
 * Implemented by lis3dh_service.c (LIS3DH) on real hardware.
 * Alternative implementations (e.g. BMI270) drop in without changing
 * power_service.c.
 */
typedef struct {
  int (*configure_wakeup)(void);
  int (*clear)(void);
} motion_iface_t;

void lis3dh_service_init(void);
const motion_iface_t *lis3dh_service_iface(void);

#endif /* LIS3DH_SERVICE_H */
