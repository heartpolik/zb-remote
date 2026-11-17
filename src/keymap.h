#pragma once
/* Button N (1..14, numbering from the annotated PCB photo) = pair of keypad lines (0-based
 * index into kp-gpios). Learned 2026-11-17 with 6 wires (line 0 = 006 = IC GND).
 * Pair 3-5 (1-based) is unused on this PCB. */
static const signed char keymap[14][2] = {
	{3, 4}, /*  1: 024-100 */
	{4, 5}, /*  2: 100-011 */
	{2, 5}, /*  3: 022-011 */
	{3, 5}, /*  4: 024-011 */
	{1, 4}, /*  5: 008-100 */
	{1, 3}, /*  6: 008-024 */
	{1, 2}, /*  7: 008-022 */
	{2, 3}, /*  8: 022-024 */
	{0, 4}, /*  9: 006-100 */
	{0, 3}, /* 10: 006-024 */
	{0, 2}, /* 11: 006-022 */
	{0, 1}, /* 12: 006-008 */
	{0, 5}, /* 13: 006-011 */
	{1, 5}, /* 14: 008-011 */
};
