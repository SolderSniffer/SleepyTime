/*
 * SleepyTime — display_service.c  (stub)
 *
 * TODO: implement SPI EPD driver integration, epd_canvas rendering,
 * and BUSY line interrupt → WATCH_EVT_DISPLAY_READY event posting.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "display_service.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(display_service, LOG_LEVEL_DBG);

void display_service_init(void) { LOG_INF("display_service: init (stub)"); }

void display_service_show_watch_face(const watch_time_t *t,
                                     uint8_t battery_pct) {
  (void)t;
  (void)battery_pct;
  LOG_INF("display_service: show_watch_face (stub)");
}

void display_service_show_sync_screen(void) {
  LOG_INF("display_service: show_sync_screen (stub)");
}

void display_service_show_low_battery(uint8_t battery_pct) {
  (void)battery_pct;
  LOG_INF("display_service: show_low_battery (stub)");
}
