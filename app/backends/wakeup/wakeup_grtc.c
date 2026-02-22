/*
 * SleepyTime — wakeup_grtc.c
 *
 * GRTC implementation of wakeup_timer_iface_t.
 *
 * z_nrf_grtc_wakeup_prepare() accepts a relative delay in microseconds —
 * pass (seconds * USEC_PER_SEC) directly. Do NOT add the current counter
 * value; the driver handles the absolute conversion internally.
 *
 * z_nrf_grtc_wakeup_prepare() is only compiled when both
 * CONFIG_POWEROFF=y and CONFIG_NRF_GRTC_START_SYSCOUNTER=y are set.
 * The prj.conf must enable both.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "wakeup_grtc.h"

#include <zephyr/drivers/timer/nrf_grtc_timer.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(wakeup_grtc, LOG_LEVEL_DBG);

/* ── wakeup_timer_iface_t implementation ─────────────────────────────────── */

static int grtc_arm(uint32_t seconds) {
  uint64_t delay_us = (uint64_t)seconds * USEC_PER_SEC;

  /*
   * z_nrf_grtc_wakeup_prepare() takes a relative delay in microseconds.
   * It is only available when CONFIG_POWEROFF &&
   * CONFIG_NRF_GRTC_START_SYSCOUNTER.
   */
  int rc = z_nrf_grtc_wakeup_prepare(delay_us);
  if (rc < 0) {
    LOG_ERR("z_nrf_grtc_wakeup_prepare failed (%d)", rc);
    return rc;
  }

  LOG_DBG("GRTC armed: wakeup in %u s (%" PRIu64 " us)", seconds, delay_us);
  return 0;
}

static void grtc_disarm(void) {
  /*
   * No explicit cancel API exists in NCS v3.2. Push the wakeup far into
   * the future — 24 hours is effectively a disarm for our use case.
   * In practice disarm() is never called in normal operation; this is
   * purely defensive.
   */
  uint64_t far_us = (uint64_t)24U * 3600U * USEC_PER_SEC;

  (void)z_nrf_grtc_wakeup_prepare(far_us);
  LOG_DBG("GRTC disarmed (pushed to +24h)");
}

static const wakeup_timer_iface_t k_grtc_iface = {
    .arm = grtc_arm,
    .disarm = grtc_disarm,
};

/* ── Public accessor ────────────────────────────────────────────────────────
 */

const wakeup_timer_iface_t *wakeup_grtc_iface(void) { return &k_grtc_iface; }