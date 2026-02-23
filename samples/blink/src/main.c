#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(blink_sample, LOG_LEVEL_INF);

#define BLINK_PERIOD_MS 500

#if !DT_NODE_HAS_STATUS(DT_ALIAS(led0), okay)
#error "This sample requires an LED alias named led0 in the board devicetree."
#endif

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

int main(void) {
  if (!gpio_is_ready_dt(&led)) {
    LOG_ERR("LED GPIO device not ready");
    return -ENODEV;
  }

  int rc = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
  if (rc != 0) {
    LOG_ERR("Failed to configure LED pin (%d)", rc);
    return rc;
  }

  LOG_INF("Blink sample started (period=%d ms)", BLINK_PERIOD_MS);

  while (true) {
    rc = gpio_pin_toggle_dt(&led);
    if (rc != 0) {
      LOG_ERR("Failed to toggle LED pin (%d)", rc);
      return rc;
    }

    LOG_INF("LED toggled");

    k_msleep(BLINK_PERIOD_MS);
  }

  return 0;
}
