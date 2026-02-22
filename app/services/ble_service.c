/*
 * SleepyTime — ble_service.c  (stub)
 *
 * TODO: implement BLE stack init, CTS client, bond management,
 * and WATCH_EVT_BLE_CONNECTED / WATCH_EVT_BLE_DISCONNECTED event posting.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ble_service.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ble_service, LOG_LEVEL_DBG);

void ble_service_init(void) { LOG_INF("ble_service: init (stub)"); }

void ble_service_start(void) { LOG_INF("ble_service: start (stub)"); }

void ble_service_stop(void) { LOG_INF("ble_service: stop (stub)"); }
