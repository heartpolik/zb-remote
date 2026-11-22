#pragma once
/* Button N (1..14) = pair of keypad lines (0-based index into kp-gpios).
 * Lines: 0 = 106 (IC GND), 1 = 104, 2 = 009, 3 = 010, 4 = 111, 5 = 011.
 * Learned 2026-11-22 on the second board (buttons on line 4/111 derived from the PCB layout).
 * Pair 3-5 (0-based) is unused on this PCB. Front-panel labels: see README. */
static const signed char keymap[14][2] = {
	{3, 4}, /*  1: 010-111  ⏻ power */
	{2, 3}, /*  2: 009-010  ⟳ сцени */
	{2, 5}, /*  3: 009-011  (blank) домофон */
	{2, 4}, /*  4: 009-111  ☼ up */
	{1, 3}, /*  5: 104-010  K- → room ◀ */
	{1, 4}, /*  6: 104-111  ☾ night */
	{1, 5}, /*  7: 104-011  K+ → room ▶ */
	{4, 5}, /*  8: 111-011  ☼ down */
	{0, 3}, /*  9: 106-010  20% */
	{0, 4}, /* 10: 106-111  50% */
	{0, 5}, /* 11: 106-011  100% */
	{0, 1}, /* 12: 106-104  3000K → hot 1 */
	{0, 2}, /* 13: 106-009  6500K → hot 2 */
	{1, 2}, /* 14: 104-009  4000K → hot 3 */
};
