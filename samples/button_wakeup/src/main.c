#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <hal/nrf_gpio.h>
#include <zephyr/sys/poweroff.h>

#define SLEEP_DELAY_MS 5000
#define BLINK_INTERVAL_MS 500

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
static struct gpio_callback button_cb_data;

static volatile uint32_t last_activity_time;

void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
  // Reset the activity timer on every press
  last_activity_time = k_uptime_get_32();
  printk("Activity detected! Timer reset.\n");
}

int main(void) {
  // 2-second startup delay so RTT has time to re-attach after wake-up reset
  // k_msleep(2000);
  printk("XIAO nRF54L15 Booted. Press button to stay awake.\n");

  gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
  gpio_pin_configure_dt(&button, GPIO_INPUT);

  gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));
  gpio_add_callback(button.port, &button_cb_data);
  gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);

  last_activity_time = k_uptime_get_32();

  while (1) {
    uint32_t now = k_uptime_get_32();

    if (now - last_activity_time > SLEEP_DELAY_MS) {
      printk("5s Inactivity. Entering System OFF...\n");
      gpio_pin_set_dt(&led, 1); // Ensure LED is off before sleeping
      k_msleep(100);            // Flush logs

      // CRITICAL: Configure P0.00 to wake up the system
      // We use nRF HAL because Zephyr's standard GPIO API often doesn't
      // expose the SENSE field needed for System OFF wakeup.
      nrf_gpio_cfg_input(NRF_GPIO_PIN_MAP(0, 0), NRF_GPIO_PIN_PULLUP);
      nrf_gpio_cfg_sense_set(NRF_GPIO_PIN_MAP(0, 0), NRF_GPIO_PIN_SENSE_LOW);

      // Trigger System OFF
      sys_poweroff();
    }

    gpio_pin_toggle_dt(&led);
    k_msleep(BLINK_INTERVAL_MS);
  }
  return 0;
}
