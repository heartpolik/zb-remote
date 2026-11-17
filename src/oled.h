#pragma once
#include <stdbool.h>
#include <stdint.h>

/* 128x64. Status bar + 3 tiles (landscape: in a row, portrait: stacked). Alert = full screen, 2x icon. */
#define UI_TILES 3
#define UI_TEXT_MAX 48

int oled_init(void);
/* "icon|text" (icon name from icons.c, text UTF-8). Empty clears the tile. */
void ui_set_tile(int idx, const char *spec);
/* "icon|line1|line2|line3"; empty string clears the alert. Wakes the screen. */
void ui_set_alert(const char *spec);
void ui_clear_alert(void);
bool ui_alert_active(void);
void ui_set_status(int battery_pct, bool linked);
void ui_set_flip(bool flip);
void ui_set_portrait(bool portrait);
/* Shift the whole picture (pixels); shows a calibration frame for 20 s */
void ui_set_offset(int x, int y);
/* Screen on for ms (0 = default) */
void oled_wake(int ms);
