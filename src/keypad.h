#pragma once
#include <stdint.h>
/* Called (work queue) when a pair of lines becomes connected that is not in keymap */
typedef void (*kp_raw_cb_t)(int a, int b, bool pressed);
int keypad_init(kp_raw_cb_t raw_cb);
