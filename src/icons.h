#pragma once
#include <stdint.h>
struct icon {
	const char *name;
	const uint32_t *rows; /* 24 rows, bit x = column x */
};
extern const struct icon icons[];
extern const int icons_count;
