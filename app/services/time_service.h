/*
 * SleepyTime — time_service.h
 *
 * Owns the PCF8563 RTC driver. Reads/writes time and persists boot state
 * to GPREGRET for recovery after System Off.
 *
 * TODO: implement in time_service.c
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TIME_SERVICE_H
#define TIME_SERVICE_H

#include "watch_time.h"
#include <stdbool.h>

void time_service_init(void);

/** Read current time from RTC. Returns true if time is valid. */
bool time_service_read(watch_time_t *t);

/** Persist time to GPREGRET before System Off. */
void time_service_save_boot_state(const watch_time_t *t);

#endif /* TIME_SERVICE_H */
