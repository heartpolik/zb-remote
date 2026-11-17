#pragma once
#include <stdint.h>
#include <stdbool.h>

#define BTN_MAX 14

enum btn_action { BTN_SINGLE = 1, BTN_DOUBLE = 2, BTN_HOLD = 3 };

/* Called from the system work queue. btn is 1-based. */
typedef void (*btn_cb_t)(uint8_t btn, enum btn_action action);

void buttons_init(btn_cb_t cb);
/* Feed debounced physical state of button (1-based) */
void buttons_feed(uint8_t btn, bool pressed);
bool buttons_any_pressed(void);
