#pragma once
#include <stdbool.h>
#include <stdint.h>

/* 128x64 two-colour. Status bar (yellow) + 3 tiles in a row (blue). Alert: title in yellow, 2x icon + 2 lines in blue. */
#define UI_TILES 3
#define UI_TEXT_MAX 48

int oled_init(void);
/* "icon|text" (icon name from icons.c, text UTF-8). Empty clears the tile. */
void ui_set_tile(int idx, const char *spec);
/* "icon|line1|line2|line3"; empty string clears the alert. Wakes the screen. */
void ui_set_alert(const char *spec);
/* Header text in the yellow band (e.g. selected room); empty = battery percent instead. */
void ui_set_title(const char *s);
void ui_clear_alert(void);
bool ui_alert_active(void);
void ui_set_status(int battery_pct, bool linked);
void ui_set_charging(bool charging); /* USB power present: bolt instead of the level */
void ui_set_flip(bool flip);
/* Shift the whole picture (pixels); shows a calibration frame for 20 s */
void ui_set_offset(int x, int y);
/* Screen on for ms (0 = default) */
void oled_wake(int ms);
