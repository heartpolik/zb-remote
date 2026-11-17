/* Status LED (P0.15, board led0) */
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include "led.h"

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static struct k_work_delayable blink_work, search_work;
static int blinks_left, on_t, off_t;
static bool lit, searching;

static void blink_fn(struct k_work *w)
{
	if (lit) {
		gpio_pin_set_dt(&led, 0);
		lit = false;
		if (--blinks_left > 0) {
			k_work_reschedule(&blink_work, K_MSEC(off_t));
		}
	} else if (blinks_left > 0) {
		gpio_pin_set_dt(&led, 1);
		lit = true;
		k_work_reschedule(&blink_work, K_MSEC(on_t));
	}
}

static void search_fn(struct k_work *w)
{
	if (!searching) {
		return;
	}
	led_blink(1, 60, 0);
	k_work_reschedule(&search_work, K_SECONDS(1));
}

void led_init(void)
{
	gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
	k_work_init_delayable(&blink_work, blink_fn);
	k_work_init_delayable(&search_work, search_fn);
}

void led_blink(int count, int on_ms, int off_ms)
{
	blinks_left = count;
	on_t = on_ms;
	off_t = off_ms;
	lit = false;
	k_work_reschedule(&blink_work, K_NO_WAIT);
}

void led_searching(bool on)
{
	searching = on;
	if (on) {
		k_work_reschedule(&search_work, K_NO_WAIT);
	} else {
		k_work_cancel_delayable(&search_work);
	}
}
