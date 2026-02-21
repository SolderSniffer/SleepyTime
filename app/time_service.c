/*
 * SleepyTime — time_service.c  (stub)
 *
 * TODO: implement PCF8563 I2C RTC driver integration and GPREGRET
 * boot state persistence.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "time_service.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(time_service, LOG_LEVEL_DBG);

void time_service_init(void) { LOG_INF("time_service: init (stub)"); }

bool time_service_read(watch_time_t *t) {
  (void)t;
  LOG_INF("time_service: read (stub) — returning invalid");
  return false;
}

void time_service_save_boot_state(const watch_time_t *t) {
  (void)t;
  LOG_INF("time_service: save_boot_state (stub)");
}
