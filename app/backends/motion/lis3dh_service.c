/*
 * SleepyTime — lis3dh_service.c
 *
 * LIS3DH implementation of motion_iface_t using the Zephyr st,lis3dh
 * (lis2dh) sensor driver.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "lis3dh_service.h"

#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>

LOG_MODULE_REGISTER(lis3dh_service, LOG_LEVEL_DBG);

#define WRIST_RAISE_TH_M_S2 2
#define WRIST_RAISE_TH_M_S2_2 450000
#define WRIST_RAISE_DUR 2

#if DT_NODE_HAS_STATUS(DT_NODELABEL(lis3dh), okay)
static const struct device *const g_lis3dh = DEVICE_DT_GET(DT_NODELABEL(lis3dh));
#define LIS3DH_AVAILABLE 1
#else
#define LIS3DH_AVAILABLE 0
#endif

static int lis3dh_configure_wakeup(void) {
#if LIS3DH_AVAILABLE
  if (!device_is_ready(g_lis3dh)) {
    LOG_ERR("LIS3DH device not ready");
    return -ENODEV;
  }

  struct sensor_value th = {
      .val1 = WRIST_RAISE_TH_M_S2,
      .val2 = WRIST_RAISE_TH_M_S2_2,
  };
  int rc = sensor_attr_set(g_lis3dh, SENSOR_CHAN_ACCEL_XYZ, SENSOR_ATTR_SLOPE_TH, &th);
  if (rc < 0) {
    LOG_ERR("Failed to set slope threshold (%d)", rc);
    return rc;
  }

  struct sensor_value dur = {.val1 = WRIST_RAISE_DUR, .val2 = 0};
  rc = sensor_attr_set(g_lis3dh, SENSOR_CHAN_ACCEL_XYZ, SENSOR_ATTR_SLOPE_DUR, &dur);
  if (rc < 0) {
    LOG_ERR("Failed to set slope duration (%d)", rc);
    return rc;
  }

  struct sensor_trigger trig = {
      .type = SENSOR_TRIG_DELTA,
      .chan = SENSOR_CHAN_ACCEL_XYZ,
  };
  rc = sensor_trigger_set(g_lis3dh, &trig, NULL);
  if (rc < 0) {
    LOG_ERR("Failed to set any-motion trigger (%d)", rc);
    return rc;
  }

  rc = pm_device_action_run(g_lis3dh, PM_DEVICE_ACTION_SUSPEND);
  if (rc < 0 && rc != -ENOSYS && rc != -EALREADY) {
    LOG_WRN("LIS3DH PM suspend failed (%d) — continuing", rc);
  }

  LOG_DBG("LIS3DH any-motion armed (th=%d.%06d m/s², dur=%d samples)", WRIST_RAISE_TH_M_S2,
          WRIST_RAISE_TH_M_S2_2, WRIST_RAISE_DUR);
  return 0;
#else
  LOG_WRN("LIS3DH not available in DTS — skipping GPIO wakeup");
  return 0;
#endif
}

static int lis3dh_clear(void) {
#if LIS3DH_AVAILABLE
  if (!device_is_ready(g_lis3dh)) {
    return -ENODEV;
  }

  int rc = pm_device_action_run(g_lis3dh, PM_DEVICE_ACTION_RESUME);
  if (rc < 0 && rc != -ENOSYS && rc != -EALREADY) {
    LOG_WRN("LIS3DH PM resume failed (%d)", rc);
  }

  rc = sensor_sample_fetch(g_lis3dh);
  if (rc < 0) {
    LOG_WRN("sensor_sample_fetch failed (%d) — INT1 latch may persist", rc);
  } else {
    LOG_DBG("LIS3DH INT1 latch cleared");
  }
#endif
  return 0;
}

static const motion_iface_t k_lis3dh_iface = {
    .configure_wakeup = lis3dh_configure_wakeup,
    .clear = lis3dh_clear,
};

void lis3dh_service_init(void) {
#if LIS3DH_AVAILABLE
  if (!device_is_ready(g_lis3dh)) {
    LOG_ERR("LIS3DH not ready — check I2C wiring and overlay address");
    return;
  }
  LOG_INF("lis3dh_service: LIS3DH ready");
#else
  LOG_WRN("lis3dh_service: lis3dh node not in DTS — wrist-raise wakeup disabled");
  LOG_WRN("Add st,lis3dh node to app/boards/*.overlay");
#endif
}

const motion_iface_t *lis3dh_service_iface(void) { return &k_lis3dh_iface; }
