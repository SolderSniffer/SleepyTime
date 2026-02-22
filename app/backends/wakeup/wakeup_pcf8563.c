/*
 * SleepyTime — wakeup_pcf8563.c
 *
 * Placeholder PCF8563 wakeup backend.
 *
 * TODO:
 *   - Program PCF8563 alarm over I2C for +seconds relative delay.
 *   - Clear alarm flags before System Off entry.
 *   - Ensure /INT line is configured for nRF54L15 GPIO sense wakeup.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "wakeup_pcf8563.h"

#include <errno.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(wakeup_pcf8563, LOG_LEVEL_DBG);

static int pcf8563_arm(uint32_t seconds) {
  LOG_WRN("PCF8563 backend selected but not implemented (arm +%u s)", seconds);
  return -ENOTSUP;
}

static void pcf8563_disarm(void) {
  LOG_WRN("PCF8563 backend selected but not implemented (disarm)");
}

static const wakeup_timer_iface_t k_pcf8563_iface = {
    .arm = pcf8563_arm,
    .disarm = pcf8563_disarm,
};

const wakeup_timer_iface_t *wakeup_pcf8563_iface(void) { return &k_pcf8563_iface; }
