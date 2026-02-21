/*
 * SleepyTime — power_service.c  (stub)
 *
 * TODO: implement GPIO sense configuration for LIS3DH INT1 and RTC alarm,
 * System Off entry, and battery fuel gauge ADC read.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "power_service.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(power_service, LOG_LEVEL_DBG);

void power_service_init(void) { LOG_INF("power_service: init (stub)"); }

uint8_t power_service_battery_pct(void) {
  return 100U; /* stub — always full */
}

watch_boot_reason_t power_service_wakeup_source(void) {
  /* stub — assume wrist raise until GPIO sense is implemented */
  return WATCH_BOOT_WRIST_RAISE;
}

void power_service_enter_system_off(void) {
  LOG_INF("power_service: enter_system_off (stub) — halting");
  /* TODO: configure GPIO sense pins then call pm_state_force() /
   * sys_poweroff(). For now just spin so the stub is safe on hardware. */
  while (true) {
    /* intentional infinite loop — replace with real System Off entry */
  }
}
