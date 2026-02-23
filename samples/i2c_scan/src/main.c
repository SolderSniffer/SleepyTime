#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(i2c_scan_sample, LOG_LEVEL_INF);

#define SCAN_PERIOD_MS 5000

#if DT_NODE_HAS_STATUS(DT_NODELABEL(i2c22), okay)
#define I2C_BUS_NODE DT_NODELABEL(i2c22)
#elif DT_NODE_HAS_STATUS(DT_NODELABEL(i2c30), okay)
#define I2C_BUS_NODE DT_NODELABEL(i2c30)
#elif DT_NODE_HAS_STATUS(DT_NODELABEL(i2c21), okay)
#define I2C_BUS_NODE DT_NODELABEL(i2c21)
#elif DT_NODE_HAS_STATUS(DT_NODELABEL(i2c20), okay)
#define I2C_BUS_NODE DT_NODELABEL(i2c20)
#elif DT_NODE_HAS_STATUS(DT_NODELABEL(i2c1), okay)
#define I2C_BUS_NODE DT_NODELABEL(i2c1)
#elif DT_NODE_HAS_STATUS(DT_NODELABEL(i2c0), okay)
#define I2C_BUS_NODE DT_NODELABEL(i2c0)
#else
#error "No enabled I2C controller found. Provide a board overlay that enables an I2C node."
#endif

static const struct device *const i2c_bus = DEVICE_DT_GET(I2C_BUS_NODE);

static int probe_address(const struct device *bus, uint16_t addr) {
  uint8_t dummy = 0U;

  return i2c_write(bus, &dummy, 0U, addr);
}

int main(void) {
  if (!device_is_ready(i2c_bus)) {
    LOG_ERR("I2C controller not ready: %s", i2c_bus->name);
    return -ENODEV;
  }

  LOG_INF("I2C scan sample started on bus: %s", i2c_bus->name);
  LOG_INF("Scanning 7-bit addresses 0x08..0x77 every %d ms", SCAN_PERIOD_MS);
  LOG_INF("If your accelerometer is connected, it should appear (LIS3DH often at 0x18 or 0x19)");

  while (true) {
    int found = 0;

    for (uint16_t addr = 0x08U; addr <= 0x77U; addr++) {
      int rc = probe_address(i2c_bus, addr);
      if (rc == 0) {
        LOG_INF("I2C device found at 0x%02x", addr);
        found++;
      }
    }

    if (found == 0) {
      LOG_WRN("No I2C devices detected");
    } else {
      LOG_INF("Scan complete, %d device(s) responded", found);
    }

    k_msleep(SCAN_PERIOD_MS);
  }

  return 0;
}
