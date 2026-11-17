/* "Pair matrix" keypad scanner for a reused IR-remote PCB.
 * Every key shorts two of N lines (no diodes). Each scan drives one line low at a time and
 * reads the others (pull-ups). Pairs that are connected at boot (power rails, IR driver,
 * caps) are learned as baseline and ignored. Scan period 25 ms, debounce = 2 scans. */
#include <zephyr/kernel.h>
#include <string.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include "keypad.h"
#include "keymap.h"
#include "buttons.h"

LOG_MODULE_REGISTER(keypad, LOG_LEVEL_INF);

#define USER_NODE DT_PATH(zephyr_user)
#define NL DT_PROP_LEN(USER_NODE, kp_gpios)
#define SCAN_MS 25
#define SETTLE_US 30

#define KP_SPEC(node, prop, idx) GPIO_DT_SPEC_GET_BY_IDX(node, prop, idx),
static const struct gpio_dt_spec lines[NL] = { DT_FOREACH_PROP_ELEM(USER_NODE, kp_gpios, KP_SPEC) };

typedef uint8_t conn_t[NL]; /* conn[i] bit j = line j low while i driven low */
static conn_t baseline, last, stable;
static kp_raw_cb_t raw_cb;
static void scan_fn(struct k_work *w);
static K_WORK_DELAYABLE_DEFINE(scan_work, scan_fn);

static void scan(conn_t out)
{
	for (int i = 0; i < NL; i++) {
		out[i] = 0;
		gpio_pin_configure_dt(&lines[i], GPIO_OUTPUT_LOW);
		k_busy_wait(SETTLE_US);
		for (int j = 0; j < NL; j++) {
			if (j != i && gpio_pin_get_raw(lines[j].port, lines[j].pin) == 0) {
				out[i] |= BIT(j);
			}
		}
		gpio_pin_configure_dt(&lines[i], GPIO_INPUT);
	}
}

/* symmetric pair state, minus baseline */
static bool pair_on(const conn_t c, int a, int b)
{
	bool on = (c[a] & BIT(b)) || (c[b] & BIT(a));
	bool base = (baseline[a] & BIT(b)) || (baseline[b] & BIT(a));
	return on && !base;
}

static int key_of_pair(int a, int b)
{
	for (int k = 0; k < 14; k++) {
		if ((keymap[k][0] == a && keymap[k][1] == b) || (keymap[k][0] == b && keymap[k][1] == a)) {
			return k + 1;
		}
	}
	return 0;
}

static void scan_fn(struct k_work *w)
{
	conn_t now;

	scan(now);
	for (int a = 0; a < NL; a++) {
		for (int b = a + 1; b < NL; b++) {
			bool n = pair_on(now, a, b);
			bool l = pair_on(last, a, b);
			bool s = pair_on(stable, a, b);
			if (n == l && n != s) { /* debounced change */
				int key = key_of_pair(a, b);
				LOG_INF("pads %d-%d %s%s", a + 1, b + 1, n ? "DOWN" : "up",
					key ? "" : " (unmapped)");
				if (key) {
					buttons_feed(key, n);
				}
				if (raw_cb) {
					raw_cb(a, b, n);
				}
			}
		}
	}
	/* commit: stable follows debounced view */
	for (int i = 0; i < NL; i++) {
		uint8_t m = 0;
		for (int j = 0; j < NL; j++) {
			if (j == i) {
				continue;
			}
			bool n = (now[i] & BIT(j)) || (now[j] & BIT(i));
			bool l = (last[i] & BIT(j)) || (last[j] & BIT(i));
			bool s = (stable[i] & BIT(j)) || (stable[j] & BIT(i));
			if ((n == l) ? n : s) {
				m |= BIT(j);
			}
		}
		stable[i] = m;
	}
	memcpy(last, now, sizeof(conn_t));
	k_work_reschedule(&scan_work, K_MSEC(SCAN_MS));
}

int keypad_init(kp_raw_cb_t cb)
{
	raw_cb = cb;
	for (int i = 0; i < NL; i++) {
		if (!gpio_is_ready_dt(&lines[i])) {
			return -ENODEV;
		}
		gpio_pin_configure_dt(&lines[i], GPIO_INPUT);
	}
	k_msleep(20);
	/* baseline = what is connected with no key pressed (rails, IR driver, caps) */
	conn_t a, b;
	scan(a);
	k_msleep(5);
	scan(b);
	for (int i = 0; i < NL; i++) {
		baseline[i] = a[i] | b[i];
		if (baseline[i]) {
			LOG_INF("baseline: pad %d connected to mask 0x%02x (ignored)", i + 1, baseline[i]);
		}
	}
	memcpy(last, baseline, sizeof(conn_t));
	memcpy(stable, baseline, sizeof(conn_t));
	k_work_reschedule(&scan_work, K_MSEC(SCAN_MS));
	LOG_INF("keypad: %d lines, scanning every %d ms", NL, SCAN_MS);
	return 0;
}
