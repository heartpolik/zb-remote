#pragma once
#include <stdint.h>

struct font_glyph {
	uint16_t cp;     /* unicode code point */
	uint8_t adv;     /* advance width */
	uint8_t w;       /* bitmap columns */
	uint16_t off;    /* offset into data */
};

struct font {
	uint8_t height;  /* rows; each column = ceil(height/8) bytes, LSB = top */
	uint8_t ascent;
	const struct font_glyph *glyphs;
	uint16_t count;
	const uint8_t *data;
};

extern const struct font font_main;
