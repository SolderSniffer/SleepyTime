/*
 * SleepyTime — motion_service.c
 *
 * LIS3DH implementation of motion_iface_t using the Zephyr st,lis3dh
 * (lis2dh) sensor driver.
 *
 * The driver owns the INT1 GPIO pin and all sensor register configuration.
 * This service sits above the driver and programs wrist-raise detection
 * parameters via the standard Zephyr sensor API:
 *
 *   sensor_attr_set()    — SENSOR_ATTR_SLOPE_TH / SLOPE_DUR
 *   sensor_trigger_set() — SENSOR_TRIG_DELTA (any-motion)
 *   pm_device_action_run() — SUSPEND before sys_poweroff()
 *
 * Swapping the LIS3DH for another sensor means:
 *   1. New compatible string in the overlay
 *   2. New motion_service.c implementing the same motion_iface_t
 *   power_service.c and main.c are untouched.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "motion_service.h"

#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>

LOG_MODULE_REGISTER(motion_service, LOG_LEVEL_DBG);

/*
 * Any-motion (slope) detection threshold in m/s².
 * The lis2dh driver maps SENSOR_ATTR_SLOPE_TH to INT1_THS.
 * At ±2g range, 1 LSB ≈ 16 mg ≈ 0.157 m/s².
 *
 * Start with ~250 mg (≈2.45 m/s²) and tune empirically on the wrist.
 * Too low → false wakes from table vibration.
 * Too high → misses gentle wrist raises.
 */
#define WRIST_RAISE_TH_M_S2 2        /* val1: integer part  */
#define WRIST_RAISE_TH_M_S2_2 450000 /* val2: micro part    */

/*
 * Duration filter: number of consecutive samples that must exceed the
 * threshold before the interrupt fires. At ODR=10Hz, 1 sample = 100ms.
 * 2 samples = 200ms minimum gesture duration — rejects single-sample spikes.
 */
#define WRIST_RAISE_DUR 2

/* ── Device handle ────────────────────────────────────────────────────────── */

#if DT_NODE_HAS_STATUS(DT_NODELABEL(lis3dh), okay)
static const struct device *const g_lis3dh =
    DEVICE_DT_GET(DT_NODELABEL(lis3dh));
#define LIS3DH_AVAILABLE 1
#else
#define LIS3DH_AVAILABLE 0
#endif

/* ── motion_iface_t implementation ───────────────────────────────────────── */

static int lis3dh_configure_wakeup(void)
{
#if LIS3DH_AVAILABLE
  if (!device_is_ready(g_lis3dh))
  {
    LOG_ERR("LIS3DH device not ready");
    return -ENODEV;
  }

  /*
   * Program any-motion threshold.
   * The driver writes this to INT1_THS via SENSOR_ATTR_SLOPE_TH.
   */
  struct sensor_value th = {
      .val1 = WRIST_RAISE_TH_M_S2,
      .val2 = WRIST_RAISE_TH_M_S2_2,
  };
  int rc = sensor_attr_set(g_lis3dh, SENSOR_CHAN_ACCEL_XYZ,
                           SENSOR_ATTR_SLOPE_TH, &th);
  if (rc < 0)
  {
    LOG_ERR("Failed to set slope threshold (%d)", rc);
    return rc;
  }

  /*
   * Program duration filter.
   * The driver writes this to INT1_DUR via SENSOR_ATTR_SLOPE_DUR.
   */
  struct sensor_value dur = {.val1 = WRIST_RAISE_DUR, .val2 = 0};
  rc = sensor_attr_set(g_lis3dh, SENSOR_CHAN_ACCEL_XYZ,
                       SENSOR_ATTR_SLOPE_DUR, &dur);
  if (rc < 0)
  {
    LOG_ERR("Failed to set slope duration (%d)", rc);
    return rc;
  }

  /*
   * Arm the any-motion trigger.
   *
   * SENSOR_TRIG_DELTA is the Zephyr standard trigger for any-motion /
   * slope detection. The driver routes this to INT1 (configured in DTS
   * via int1-gpio-config and anym-mode) and programs CTRL_REG3/5 and
   * INT1_CFG accordingly.
   *
   * We pass NULL as the handler — we don't want a callback during active
   * operation, just the GPIO line held high after System Off wakeup.
   * The nRF54L15 GPIO sense latch fires on the level, not the edge.
   *
   * Note: setting a NULL handler arms the hardware interrupt but does
   * not register a thread/ISR callback. The line stays asserted until
   * lis3dh_clear() reads INT1_SRC.
   */
  struct sensor_trigger trig = {
      .type = SENSOR_TRIG_DELTA,
      .chan = SENSOR_CHAN_ACCEL_XYZ,
  };
  rc = sensor_trigger_set(g_lis3dh, &trig, NULL);
  if (rc < 0)
  {
    LOG_ERR("Failed to set any-motion trigger (%d)", rc);
    return rc;
  }

  /*
   * Suspend the sensor driver before sys_poweroff().
   *
   * This is required to avoid a kernel panic. Without it, the Zephyr PM
   * subsystem tries to suspend the sensor from the idle thread — which
   * cannot perform I2C operations. Suspending explicitly from task
   * context here is the correct pattern (confirmed by zephyr#46126).
   *
   * PM_DEVICE_ACTION_SUSPEND puts the LIS3DH into low-power mode while
   * keeping the INT1 latch armed — the sensor continues to assert INT1
   * in suspend, which is exactly what we need for GPIO sense wakeup.
   */
  rc = pm_device_action_run(g_lis3dh, PM_DEVICE_ACTION_SUSPEND);
  if (rc < 0 && rc != -ENOSYS && rc != -EALREADY)
  {
    LOG_WRN("LIS3DH PM suspend failed (%d) — continuing", rc);
  }

  LOG_DBG("LIS3DH any-motion armed (th=%d.%06d m/s², dur=%d samples)",
          WRIST_RAISE_TH_M_S2, WRIST_RAISE_TH_M_S2_2, WRIST_RAISE_DUR);
  return 0;
#else
  LOG_WRN("LIS3DH not available in DTS — skipping GPIO wakeup");
  return 0;
#endif
}

static int lis3dh_clear(void)
{
#if LIS3DH_AVAILABLE
  if (!device_is_ready(g_lis3dh))
  {
    return -ENODEV;
  }

  /*
   * Resume the driver before attempting I2C communication.
   * The sensor was suspended in configure_wakeup() before System Off.
   */
  int rc = pm_device_action_run(g_lis3dh, PM_DEVICE_ACTION_RESUME);
  if (rc < 0 && rc != -ENOSYS && rc != -EALREADY)
  {
    LOG_WRN("LIS3DH PM resume failed (%d)", rc);
  }

  /*
   * Reading INT1_SRC via sensor_sample_fetch() clears the any-motion
   * latch inside the LIS3DH and deasserts INT1. This prevents the GPIO
   * sense from re-firing on the next sleep entry if the wrist is still
   * raised when power_service_init() runs.
   *
   * The lis2dh driver reads INT1_SRC internally when handling the
   * SENSOR_TRIG_DELTA trigger — sample_fetch is the cleanest way to
   * trigger that path without a live interrupt handler.
   */
  rc = sensor_sample_fetch(g_lis3dh);
  if (rc < 0)
  {
    LOG_WRN("sensor_sample_fetch failed (%d) — INT1 latch may persist",
            rc);
  }
  else
  {
    LOG_DBG("LIS3DH INT1 latch cleared");
  }
#endif
  return 0;
}

static const motion_iface_t k_lis3dh_iface = {
    .configure_wakeup = lis3dh_configure_wakeup,
    .clear = lis3dh_clear,
};

/* ── Service API ──────────────────────────────────────────────────────────── */

void motion_service_init(void)
{
#if LIS3DH_AVAILABLE
  if (!device_is_ready(g_lis3dh))
  {
    LOG_ERR("LIS3DH not ready — check I2C wiring and overlay address");
    return;
  }
  LOG_INF("motion_service: LIS3DH ready");
#else
  LOG_WRN("motion_service: lis3dh node not in DTS — wrist-raise wakeup disabled");
  LOG_WRN("Add st,lis3dh node to app/boards/*.overlay");
#endif
}

const motion_iface_t *motion_service_iface(void)
{
  return &k_lis3dh_iface;
}