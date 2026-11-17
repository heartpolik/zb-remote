#include "oled.c"
static void dump(const char *name, bool rot) {
	FILE *f = fopen(name, "w");
	fprintf(f, "P1\n%d %d\n", rot ? PH : PW, rot ? PW : PH);
	if (!rot) for (int y = 0; y < PH; y++) { for (int x = 0; x < PW; x++) fprintf(f, "%d ", (fb[(y/8)*PW+x] >> (y%8)) & 1); fputc('\n', f); }
	else for (int ly = 0; ly < PW; ly++) { for (int lx = 0; lx < PH; lx++) { int X = ly, Y = PH-1-lx; fprintf(f, "%d ", (fb[(Y/8)*PW+X] >> (Y%8)) & 1);} fputc('\n', f); }
	fclose(f);
}
static void render(const char *n) { memset(fb,0,sizeof fb); if (alert.active) draw_alert(); else draw_home(); dump(n, portrait); }
/* Renders the real src/oled.c into PBM files (see tools/preview/render.sh). */
int main(void) {
	ui_set_status(76, true);
	ui_set_tile(0, "temp|23.4°");
	ui_set_tile(1, "bulb|Кухня");
	ui_set_tile(2, "lock|Зачинено");
	render("home_l.pbm");
	portrait = true; render("home_p.pbm"); portrait = false;
	ui_set_alert("bell|Домофон|Хтось біля|дверей");
	render("alert_l.pbm");
	portrait = true; render("alert_p.pbm");
	return 0;
}
