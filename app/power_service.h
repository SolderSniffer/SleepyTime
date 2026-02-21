/*
 * SleepyTime — power_service.h
 *
 * Manages System Off entry, GPIO sense configuration for LIS3DH INT1 and
 * RTC alarm wakeup, and battery fuel gauge reads.
 *
 * TODO: implement in power_service.c
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef POWER_SERVICE_H
#define POWER_SERVICE_H

#include "watch_sm.h"
#include <stdint.h>

void power_service_init(void);

/** Return battery percentage 0–100. */
uint8_t power_service_battery_pct(void);

/**
 * Inspect which GPIO sense line fired to distinguish wrist-raise from
 * RTC alarm after a RESET_LOW_POWER_WAKE reset.
 */
watch_boot_reason_t power_service_wakeup_source(void);

/** Configure GPIO sense and enter System Off. Does not return. */
void power_service_enter_system_off(void);

#endif /* POWER_SERVICE_H */
