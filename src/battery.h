#pragma once
#include <stdint.h>
/* Returns battery millivolts (VDDH) or <0 on error */
int battery_mv(void);
/* LiPo 0..100 % estimate */
uint8_t battery_percent(int mv);
