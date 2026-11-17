/* Click detection (single / double / hold) on top of any debounced key source. */
#include <zephyr/kernel.h>
#include "buttons.h"

#define HOLD_MS   600
#define DOUBLE_MS 300

struct btn_state {
	struct k_work_delayable timer;
	bool pressed;
	bool hold_fired;
	uint8_t clicks;
};

static struct btn_state st[BTN_MAX];
static btn_cb_t user_cb;

static void timer_fn(struct k_work *w)
{
	struct btn_state *s = CONTAINER_OF(k_work_delayable_from_work(w), struct btn_state, timer);
	int i = s - st;

	if (s->pressed) {
		s->hold_fired = true;
		s->clicks = 0;
		user_cb(i + 1, BTN_HOLD);
	} else if (s->clicks) {
		user_cb(i + 1, s->clicks >= 2 ? BTN_DOUBLE : BTN_SINGLE);
		s->clicks = 0;
	}
}

void buttons_feed(uint8_t btn, bool now)
{
	if (btn < 1 || btn > BTN_MAX) {
		return;
	}
	struct btn_state *s = &st[btn - 1];

	if (now == s->pressed) {
		return;
	}
	s->pressed = now;
	if (now) {
		s->hold_fired = false;
		k_work_reschedule(&s->timer, K_MSEC(HOLD_MS));
	} else if (s->hold_fired) {
		k_work_cancel_delayable(&s->timer);
	} else if (++s->clicks >= 2) {
		k_work_cancel_delayable(&s->timer);
		s->clicks = 0;
		user_cb(btn, BTN_DOUBLE);
	} else {
		k_work_reschedule(&s->timer, K_MSEC(DOUBLE_MS));
	}
}

bool buttons_any_pressed(void)
{
	for (int i = 0; i < BTN_MAX; i++) {
		if (st[i].pressed) {
			return true;
		}
	}
	return false;
}

void buttons_init(btn_cb_t cb)
{
	user_cb = cb;
	for (int i = 0; i < BTN_MAX; i++) {
		k_work_init_delayable(&st[i].timer, timer_fn);
	}
}
