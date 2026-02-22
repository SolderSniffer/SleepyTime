/*
 * SleepyTime — power_service.c
 *
 * Manages System Off entry and wakeup source identification.
 *
 * Wakeup sources on the nRF54L15:
 *   RESET_CLOCK          — GRTC alarm (periodic minute tick)
 *   RESET_LOW_POWER_WAKE — GPIO sense (LIS3DH INT1, wrist raise)
 *
 * This service has no knowledge of which timer or sensor is in use.
 * It calls through wakeup_timer_iface_t and motion_iface_t, both
 * injected by main.c at init time.
 *
 * Errata [48]: RESETREAS wakeup field can be incorrect when a wakeup
 * source is active during reset. Handled in main.c — if a hard reset
 * bit (RESET_PIN, RESET_WATCHDOG) is set alongside a wakeup bit the
 * whole cause is treated as a cold boot.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "power_service.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/sys/poweroff.h>

LOG_MODULE_REGISTER(power_service, LOG_LEVEL_DBG);

/*
 * Wakeup interval passed to wakeup_timer_iface_t.arm().
 * Tune to trade display freshness against wake frequency.
 * At ~1.2 µA System Off current a 60 s interval costs ~0.02 µAh/day.
 */
#define WAKEUP_INTERVAL_S 60U

/* ── Module state ───────────────────────────────────────────────────────────
 */

static const wakeup_timer_iface_t *g_wakeup;
static const motion_iface_t *g_motion;
static bool g_initialised;

/* ── Public API ─────────────────────────────────────────────────────────────
 */

void power_service_init(const wakeup_timer_iface_t *wakeup,
                        const motion_iface_t *motion) {
  __ASSERT_NO_MSG(wakeup != NULL);
  __ASSERT_NO_MSG(wakeup->arm != NULL);
  __ASSERT_NO_MSG(wakeup->disarm != NULL);

  g_wakeup = wakeup;
  g_motion = motion; /* NULL is valid — GRTC-only wakeup */
  g_initialised = true;

  /*
   * If this is a wrist-raise wakeup the LIS3DH INT1 line may still be
   * asserted from the previous sleep cycle. Clear it now so the GPIO
   * sense latch does not fire again on the next sleep entry.
   */
  if (g_motion && g_motion->clear) {
    int rc = g_motion->clear();
    if (rc < 0) {
      LOG_WRN("motion clear failed (%d)", rc);
    }
  }

  LOG_INF("power_service: ready (wakeup interval %u s)", WAKEUP_INTERVAL_S);
}

uint8_t power_service_battery_pct(void) {
  /*
   * TODO: implement ADC fuel gauge read.
   *
   * Outline:
   *   1. Enable load-switch GPIO (check XIAO schematic for pin).
   *   2. Wait ~1 ms for voltage divider to settle.
   *   3. Sample the ADC channel.
   *   4. Disable load switch.
   *   5. Convert raw → mV using ADC reference/gain config.
   *   6. Map voltage → capacity via LiPo curve (3.0V=0%, 4.2V=100%).
   */
  return 100U;
}

watch_boot_reason_t power_service_wakeup_source(void) {
  /*
   * Called by main.c when RESET_LOW_POWER_WAKE is set without RESET_CLOCK.
   * That means GPIO sense fired — LIS3DH INT1 (wrist raise).
   *
   * If multiple GPIO sense sources are added in future (e.g. charge
   * detect), read and compare pin states here to disambiguate.
   */
  LOG_DBG("Wakeup source: GPIO sense (wrist raise)");
  return WATCH_BOOT_WRIST_RAISE;
}

void power_service_enter_system_off(void) {
  __ASSERT(g_initialised, "power_service_init() not called");

  LOG_INF("Entering System Off (timer +%u s)", WAKEUP_INTERVAL_S);

  /*
   * Flush RTT before powering off so the last log messages are visible.
   * LOG_PANIC() switches to synchronous output and blocks until drained.
   *
   * NOTE: on a standalone device without a debug probe this will spin
   * until the RTT timeout expires. Guard with IS_ENABLED(CONFIG_DEBUG)
   * once the production build configuration is defined.
   */
  LOG_PANIC();

  /*
   * Step 1: Configure accelerometer GPIO sense for wrist-raise wakeup.
   * Must complete before sys_poweroff() — the nRF54L15 latches GPIO
   * sense configuration on System Off entry.
   */
  if (g_motion && g_motion->configure_wakeup) {
    int rc = g_motion->configure_wakeup();
    if (rc < 0) {
      LOG_WRN("motion configure_wakeup failed (%d) — timer-only wakeup", rc);
    }
  } else {
    LOG_DBG("No motion interface — skipping GPIO sense wakeup");
  }

  /*
   * Step 2: Arm the periodic wakeup timer.
   * Called immediately before sys_poweroff() to minimise drift between
   * the armed timestamp and actual sleep entry.
   */
  int rc = g_wakeup->arm(WAKEUP_INTERVAL_S);
  if (rc < 0) {
    /*
     * Timer arming failure is not fatal — the device will enter
     * System Off and can still wake via GPIO sense. Without a timer
     * wakeup the display will not update until the user raises their
     * wrist. Log the error and proceed.
     */
    LOG_ERR("wakeup arm failed (%d) — GPIO-only wakeup", rc);
  }

  /*
   * Step 3: Enter System Off.
   *
   * sys_poweroff() does not return. The SoC resets on the next wakeup
   * and execution restarts from main().
   *
   * Zephyr power-off sequence:
   *   1. PM_DEVICE_ACTION_TURN_OFF on all PM-managed devices.
   *   2. arch_system_off() → NRF_POWER->SYSTEMOFF.
   */
  sys_poweroff();

  CODE_UNREACHABLE;
}