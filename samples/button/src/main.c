#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(button_sample, LOG_LEVEL_INF);

#if !DT_NODE_HAS_STATUS(DT_ALIAS(sw0), okay)
#error "This sample requires a button alias named sw0 in the board devicetree."
#endif

static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);

static struct gpio_callback button_cb;

static void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
  int val = gpio_pin_get_dt(&button);

  if (val < 0) {
    LOG_ERR("Failed to read button pin (%d)", val);
    return;
  }

  if (val == 0) {
    LOG_INF("Button pressed");
  } else {
    LOG_INF("Button released");
  }
}

int main(void) {
  int rc;

  if (!gpio_is_ready_dt(&button)) {
    LOG_ERR("Button GPIO device not ready");
    return -ENODEV;
  }

  rc = gpio_pin_configure_dt(&button, GPIO_INPUT);
  if (rc != 0) {
    LOG_ERR("Failed to configure button pin (%d)", rc);
    return rc;
  }

  rc = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_BOTH);
  if (rc != 0) {
    LOG_ERR("Failed to configure button interrupt (%d)", rc);
    return rc;
  }

  gpio_init_callback(&button_cb, button_pressed, BIT(button.pin));
  gpio_add_callback(button.port, &button_cb);

  LOG_INF("Button sample started — press the user button (sw0)");

  /* Nothing to poll; all work is done in the ISR callback. */
  return 0;
}