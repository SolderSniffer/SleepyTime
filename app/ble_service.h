/*
 * SleepyTime — ble_service.h
 *
 * Manages the BLE stack lifecycle and Current Time Service (CTS) client.
 * Calls ble_service_on_time_sync() (defined in main.c) on a successful
 * CTS write, and posts WATCH_EVT_BLE_CONNECTED / WATCH_EVT_BLE_DISCONNECTED
 * to the event queue.
 *
 * TODO: implement in ble_service.c
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BLE_SERVICE_H
#define BLE_SERVICE_H

#include "watch_time.h"

void ble_service_init(void);
void ble_service_start(void);
void ble_service_stop(void);

/** Implemented in main.c — called by ble_service on CTS write. */
void ble_service_on_time_sync(const watch_time_t *new_time);

#endif /* BLE_SERVICE_H */
