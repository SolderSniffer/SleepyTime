/*
 * SleepyTime — display_service.h
 *
 * Owns the SPI EPD driver. Renders via epd_canvas and manages the BUSY line.
 * Posts WATCH_EVT_DISPLAY_READY to the event queue when refresh completes.
 *
 * TODO: implement in display_service.c
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef DISPLAY_SERVICE_H
#define DISPLAY_SERVICE_H

#include "watch_time.h"
#include <stdint.h>

void display_service_init(void);
void display_service_show_watch_face(const watch_time_t *t, uint8_t battery_pct);
void display_service_show_sync_screen(void);
void display_service_show_low_battery(uint8_t battery_pct);

#endif /* DISPLAY_SERVICE_H */
