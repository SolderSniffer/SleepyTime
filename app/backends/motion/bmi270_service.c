/*
 * SleepyTime — bmi270_service.c
 *
 * Placeholder BMI270 accelerometer backend.
 *
 * TODO:
 *   - Configure any-motion / wrist-raise interrupt on BMI270.
 *   - Ensure interrupt latch semantics work with GPIO sense wakeup.
 *   - Implement clear() by reading/clearing interrupt status registers.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bmi270_service.h"

#include <errno.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(bmi270_service, LOG_LEVEL_DBG);

static int bmi270_configure_wakeup(void) {
  LOG_WRN("BMI270 backend selected but not implemented (configure_wakeup)");
  return -ENOTSUP;
}

static int bmi270_clear(void) {
  LOG_WRN("BMI270 backend selected but not implemented (clear)");
  return 0;
}

static const motion_iface_t k_bmi270_iface = {
    .configure_wakeup = bmi270_configure_wakeup,
    .clear = bmi270_clear,
};

void bmi270_service_init(void) {
  LOG_WRN("bmi270_service: selected but placeholder implementation only");
}

const motion_iface_t *bmi270_service_iface(void) { return &k_bmi270_iface; }
