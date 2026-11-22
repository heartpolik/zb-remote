/* Renders the real src/oled.c into PBM files, one per scene (see tools/preview/render.sh). */
#include "oled.c"

static void dump(const char *name)
{
	char path[64];
	snprintf(path, sizeof(path), "%s.pbm", name);
	FILE *f = fopen(path, "w");
	fprintf(f, "P1\n%d %d\n", PW, PH);
	for (int y = 0; y < PH; y++) {
		for (int x = 0; x < PW; x++) {
			fprintf(f, "%d ", (fb[(y / 8) * PW + x] >> (y % 8)) & 1);
		}
		fputc('\n', f);
	}
	fclose(f);
}

/* title, tile1..3, alert ("" = none), battery % */
static void scene(const char *name, const char *ttl, const char *t1, const char *t2, const char *t3,
		  const char *al, int batt)
{
	ui_set_status(batt, true);
	ui_set_title(ttl);
	ui_set_tile(0, t1);
	ui_set_tile(1, t2);
	ui_set_tile(2, t3);
	alert.active = false;
	if (al[0]) {
		ui_set_alert(al);
	}
	memset(fb, 0, sizeof(fb));
	if (alert.active) {
		draw_alert();
	} else {
		draw_home();
	}
	dump(name);
}

int main(void)
{
	scene("overview", "", "power|2.5kW", "battery|100%", "temp|-0.3°", "", 76);
	scene("room", "Вітальня", "bulb|ON", "sun|60%", "temp|24.1°", "", 76);
	scene("room_nodim", "Коридор", "bulb_off|OFF", "sun|—", "temp|—", "", 76);
	scene("alert", "", "", "", "", "bell|Домофон|Хтось біля|дверей", 76);
	scene("alert_title", "", "", "", "", "power|Світло зникло", 76);
	return 0;
}
