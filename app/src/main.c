/*
 * SleepyTime — Hello World
 *
 * Blinks the onboard LED at 1 Hz and logs a counter over RTT.
 *
 * Viewing RTT output:
 *   - nRF Connect for Desktop → Programmer → RTT Viewer (easiest)
 *   - Or from a terminal: JLinkExe → connect → then JLinkRTTClient
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

#define LED0_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

int main(void) {
  int ret;
  uint32_t count = 0;

  if (!gpio_is_ready_dt(&led)) {
    LOG_ERR("LED GPIO device not ready");
    return -ENODEV;
  }

  ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
  if (ret < 0) {
    LOG_ERR("Failed to configure LED GPIO: %d", ret);
    return ret;
  }

  LOG_INF("SleepyTime hello world — board: %s", CONFIG_BOARD);

  while (1) {
    ret = gpio_pin_toggle_dt(&led);
    if (ret < 0) {
      LOG_ERR("Failed to toggle LED: %d", ret);
      return ret;
    }

    LOG_INF("Blink #%u — LED is %s", count++,
            gpio_pin_get_dt(&led) ? "ON" : "OFF");

    k_msleep(100);
  }

  return 0;
}