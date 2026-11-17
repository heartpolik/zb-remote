#include <zephyr/kernel.h>
#include <zephyr/drivers/adc.h>
#include "battery.h"

static const struct adc_dt_spec ch = ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 0);

int battery_mv(void)
{
	int16_t raw;
	struct adc_sequence seq = { .buffer = &raw, .buffer_size = sizeof(raw) };
	static bool inited;

	if (!inited) {
		if (!adc_is_ready_dt(&ch) || adc_channel_setup_dt(&ch)) {
			return -ENODEV;
		}
		inited = true;
	}
	adc_sequence_init_dt(&ch, &seq);
	if (adc_read_dt(&ch, &seq)) {
		return -EIO;
	}
	int32_t mv = raw;
	adc_raw_to_millivolts_dt(&ch, &mv);
	return mv * 5; /* VDDH/5 */
}

uint8_t battery_percent(int mv)
{
	/* rough LiPo discharge curve */
	static const struct { int mv; uint8_t pct; } t[] = {
		{4200, 100}, {4100, 90}, {4000, 80}, {3900, 65}, {3800, 50},
		{3700, 30}, {3600, 15}, {3500, 5}, {3300, 0},
	};
	if (mv >= t[0].mv) {
		return 100;
	}
	for (int i = 1; i < ARRAY_SIZE(t); i++) {
		if (mv >= t[i].mv) {
			return t[i].pct + (mv - t[i].mv) * (t[i - 1].pct - t[i].pct) /
					  (t[i - 1].mv - t[i].mv);
		}
	}
	return 0;
}
