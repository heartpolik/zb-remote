#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
int main(void)
{
	gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	for (int n = 0;; n++) {
		gpio_pin_toggle_dt(&led);
		printk("blinky alive %d\n", n);
		k_msleep(500);
	}
}
