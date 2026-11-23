/* SSD1306 128x64 (two-colour: yellow rows 0..15, blue below): UI with pixel-art icons. */
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include "oled.h"
#include "font.h"
#include "icons.h"

LOG_MODULE_REGISTER(oled, LOG_LEVEL_INF);

#define PW 128 /* physical */
#define PH 64
/* logical size depends on orientation */
#define W PW
#define H PH
#define OLED_ON_MS 10000
#define ALERT_ON_MS 30000
#define ANIM_MS 300

static const struct device *disp = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
static const struct gpio_dt_spec vcc = GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), vcc_ctrl_gpios);

static uint8_t fb[PW * PH / 8];
static struct {
	int icon;
	char text[UI_TEXT_MAX];
} tiles[UI_TILES] = { { -1 }, { -1 }, { -1 } };
static struct {
	bool active;
	int icon;
	char lines[3][UI_TEXT_MAX];
	int phase;
} alert;
static int batt_pct = -1;
static bool charging; /* USB power present */
static int focus; /* 1..3: tile whose value the d-pad changes (drawn inverted), 0 = none */
static char title[UI_TEXT_MAX]; /* header text (yellow band), e.g. selected room */
static bool linked, flip, ready, on;
static int xoff, yoff;

static int64_t frame_until;
static K_MUTEX_DEFINE(lock);

static void off_fn(struct k_work *w);
static K_WORK_DELAYABLE_DEFINE(off_work, off_fn);
static void draw_fn(struct k_work *w);
static K_WORK_DEFINE(draw_work, draw_fn);
static void anim_fn(struct k_work *w);
static K_WORK_DELAYABLE_DEFINE(anim_work, anim_fn);

/* ---------- drawing primitives ---------- */
static void px(int x, int y, bool v)
{
	x += xoff;
	y += yoff;
	if (x < 0 || x >= W || y < 0 || y >= H) {
		return;
	}
	int X = flip ? (PW - 1 - x) : x;
	int Y = flip ? (PH - 1 - y) : y;
	uint8_t *b = &fb[(Y / 8) * PW + X];
	if (v) {
		*b |= BIT(Y % 8);
	} else {
		*b &= ~BIT(Y % 8);
	}
}

static void px_toggle(int x, int y)
{
	x += xoff;
	y += yoff;
	if (x < 0 || x >= W || y < 0 || y >= H) {
		return;
	}
	int X = flip ? (PW - 1 - x) : x;
	int Y = flip ? (PH - 1 - y) : y;
	fb[(Y / 8) * PW + X] ^= BIT(Y % 8);
}

static void rect(int x0, int y0, int x1, int y1, bool fill)
{
	for (int y = y0; y <= y1; y++) {
		for (int x = x0; x <= x1; x++) {
			if (fill || y == y0 || y == y1 || x == x0 || x == x1) {
				px(x, y, true);
			}
		}
	}
}

static void disc(int cx, int cy, int r, bool fill)
{
	for (int y = -r; y <= r; y++) {
		for (int x = -r; x <= r; x++) {
			int d = x * x + y * y;
			if (d <= r * r && (fill || d >= (r - 1) * (r - 1))) {
				px(cx + x, cy + y, true);
			}
		}
	}
}

static void draw_icon_s(int idx, int x0, int y0, int s)
{
	if (idx < 0 || idx >= icons_count) {
		return;
	}
	for (int y = 0; y < 24 * s; y++) {
		uint32_t r = icons[idx].rows[y / s];
		for (int x = 0; x < 24 * s; x++) {
			if (r & BIT(x / s)) {
				px(x0 + x, y0 + y, true);
			}
		}
	}
}

static void draw_icon(int idx, int x0, int y0)
{
	draw_icon_s(idx, x0, y0, 1);
}

/* horizontal clip window for text (logical coords) */
static int clip_l = -10000, clip_r = 10000;

static const struct font_glyph *glyph(uint16_t cp)
{
	int lo = 0, hi = font_main.count - 1;
	while (lo <= hi) {
		int m = (lo + hi) / 2;
		if (font_main.glyphs[m].cp == cp) {
			return &font_main.glyphs[m];
		}
		if (font_main.glyphs[m].cp < cp) {
			lo = m + 1;
		} else {
			hi = m - 1;
		}
	}
	return NULL;
}

static uint32_t utf8_next(const char **s)
{
	const uint8_t *p = (const uint8_t *)*s;
	uint32_t c = *p++;
	if (c >= 0xc0 && c < 0xe0 && (*p & 0xc0) == 0x80) {
		c = ((c & 0x1f) << 6) | (*p++ & 0x3f);
	} else if (c >= 0xe0 && c < 0xf0 && (p[0] & 0xc0) == 0x80 && (p[1] & 0xc0) == 0x80) {
		c = ((c & 0x0f) << 12) | ((p[0] & 0x3f) << 6) | (p[1] & 0x3f);
		p += 2;
	} else if (c >= 0xf0) {
		while ((*p & 0xc0) == 0x80) {
			p++;
		}
		c = '?';
	}
	*s = (const char *)p;
	return c;
}

static const struct font_glyph *glyph_or_q(uint32_t cp)
{
	const struct font_glyph *g = cp <= 0xffff ? glyph(cp) : NULL;
	return g ? g : glyph('?');
}

static int text_width(const char *s)
{
	int w = 0;
	while (*s) {
		w += glyph_or_q(utf8_next(&s))->adv;
	}
	return w;
}

static void draw_text(int x, int y, const char *s)
{
	int nb = (font_main.height + 7) / 8;
	while (*s) {
		const struct font_glyph *g = glyph_or_q(utf8_next(&s));
		const uint8_t *col = &font_main.data[g->off];
		for (int cx = 0; cx < g->w; cx++, col += nb) {
			uint32_t bits = 0;
			for (int k = 0; k < nb; k++) {
				bits |= (uint32_t)col[k] << (8 * k);
			}
			if (x + cx < clip_l || x + cx > clip_r) {
				continue;
			}
			for (int r = 0; r < font_main.height; r++) {
				if (bits & BIT(r)) {
					px(x + cx, y + r, true);
				}
			}
		}
		x += g->adv;
	}
}

/* text centred in [x0, x0+w), clipped to it when too long */
static void draw_text_box(int x0, int w, int y, const char *s)
{
	int tw = text_width(s);
	clip_l = x0;
	clip_r = x0 + w - 1;
	draw_text(tw >= w ? x0 : x0 + (w - tw) / 2, y, s);
	clip_l = -10000;
	clip_r = 10000;
}

static void draw_text_centered(int y, const char *s)
{
	draw_text_box(0, W, y, s);
}

/* ---------- screens ---------- */
/* lightning bolt inside the battery outline (x0..x0+15, y 2..9) */
static void draw_bolt(int x0)
{
	static const char *b[] = {
		"......###...",
		"....####....",
		"..#########.",
		".#########..",
		"....####....",
		"...###......",
	};
	for (int y = 0; y < 6; y++) {
		for (int x = 0; b[y][x]; x++) {
			if (b[y][x] == '#') {
				px(x0 + 2 + x, 3 + y, true);
			}
		}
	}
}

static void draw_battery_icon(int x0)
{
	rect(x0, 2, x0 + 15, 9, false);
	rect(x0 + 16, 4, x0 + 17, 7, true);
	if (charging) {
		draw_bolt(x0);
	} else if (batt_pct >= 0) {
		int fill = (batt_pct * 12 + 50) / 100;
		if (fill > 0) {
			rect(x0 + 2, 4, x0 + 1 + fill, 7, true);
		}
	}
}

static void draw_status_bar(void)
{
	if (title[0]) {
		/* title (left), battery + link dot (right), separator at y=12 */
		clip_l = 1;
		clip_r = W - 37;
		draw_text(1, 0, title);
		clip_l = -10000;
		clip_r = 10000;
		draw_battery_icon(W - 32);
		disc(W - 6, 6, 3, linked);
		for (int x = 0; x < W; x++) {
			px(x, 12, true);
		}
		return;
	}
	/* battery + percent (left), link dot (right), separator at y=12 */
	draw_battery_icon(1);
	if (charging) {
		draw_text(22, 0, "USB");
	} else if (batt_pct >= 0) {
		char t[8];
		snprintk(t, sizeof(t), "%d%%", batt_pct);
		draw_text(22, 0, t);
	}
	disc(W - 6, 6, 3, linked);
	for (int x = 0; x < W; x++) {
		px(x, 12, true);
	}
}

static void draw_home(void)
{
	draw_status_bar();
	/* 3 tiles side by side, 42 px each: icon on top, value below */
	for (int i = 0; i < UI_TILES; i++) {
		int x0 = i * 43;
		if (i) {
			for (int y = 16; y < H - 2; y += 2) {
				px(x0 - 1, y, true);
			}
		}
		draw_icon(tiles[i].icon, x0 + 9, 16);
		draw_text_box(x0 + 1, 41, 44, tiles[i].text);
		if (focus == i + 1) {
			/* inverted value box with rounded corners */
			for (int y = 42; y <= 57; y++) {
				for (int x = x0 + 1; x <= x0 + 40; x++) {
					bool corner = (y == 42 || y == 57) && (x == x0 + 1 || x == x0 + 40);
					if (!corner) {
						px_toggle(x, y);
					}
				}
			}
		}
	}
}

/* Two-colour panel: physical rows 0..15 are yellow, 16..63 blue (with a gap between).
 * Alert layout keeps every element inside one colour zone. */
#define BAND_H 16

static void draw_alert(void)
{
	int shake = (alert.phase % 2) ? 2 : -2;
	/* yellow band: title (line 1); blue body: 2x icon + lines 2-3 */
	int band_y = flip ? H - BAND_H : 0;
	int body_y = flip ? 0 : BAND_H;
	draw_text_centered(band_y + 2, alert.lines[0]);
	int m = (alert.lines[1][0] ? 1 : 0) + (alert.lines[2][0] ? 1 : 0);
	if (m == 0) {
		draw_icon_s(alert.icon, (W - 48) / 2 + shake, body_y, 2);
	} else {
		draw_icon_s(alert.icon, 4 + shake, body_y, 2);
		int y = body_y + (H - BAND_H - m * 14) / 2 + 1;
		for (int i = 1; i < 3; i++) {
			if (alert.lines[i][0]) {
				draw_text_box(58, W - 58, y, alert.lines[i]);
				y += 14;
			}
		}
	}
	if (alert.phase % 4 >= 2) { /* blink: invert */
		for (size_t i = 0; i < sizeof(fb); i++) {
			fb[i] ^= 0xff;
		}
	}
}

static void draw_fn(struct k_work *w)
{
	if (!ready) {
		return;
	}
	k_mutex_lock(&lock, K_FOREVER);
	memset(fb, 0, sizeof(fb));
	if (alert.active) {
		draw_alert();
	} else {
		draw_home();
	}
	if (k_uptime_get() < frame_until) {
		/* calibration frame: logical border + centre line */
		int ox = xoff, oy = yoff;
		xoff = yoff = 0;
		rect(0, 0, W - 1, H - 1, false);
		for (int y = 0; y < H; y += 4) {
			px(W / 2, y, true);
		}
		xoff = ox;
		yoff = oy;
	}
	k_mutex_unlock(&lock);

	struct display_buffer_descriptor d = {
		.buf_size = sizeof(fb), .width = PW, .height = PH, .pitch = PW,
	};
	display_write(disp, 0, 0, &d, fb);
	if (!on) {
		display_blanking_off(disp);
		on = true;
	}
}

static void anim_fn(struct k_work *w)
{
	if (!alert.active || !on) {
		return;
	}
	alert.phase++;
	k_work_submit(&draw_work);
	k_work_reschedule(&anim_work, K_MSEC(ANIM_MS));
}

static void off_fn(struct k_work *w)
{
	if (ready && on) {
		display_blanking_on(disp);
		on = false;
	}
}

/* ---------- API ---------- */
static int icon_by_name(const char *name, size_t len)
{
	for (int i = 0; i < icons_count; i++) {
		if (strlen(icons[i].name) == len && !strncmp(icons[i].name, name, len)) {
			return i;
		}
	}
	return -1;
}

/* split "a|b|c" into fields (in place copy) */
static int split(const char *spec, char out[][UI_TEXT_MAX], int max)
{
	int n = 0;
	const char *p = spec;
	while (n < max) {
		const char *e = strchr(p, '|');
		size_t len = e ? (size_t)(e - p) : strlen(p);
		len = MIN(len, UI_TEXT_MAX - 1);
		memcpy(out[n], p, len);
		out[n][len] = 0;
		n++;
		if (!e) {
			break;
		}
		p = e + 1;
	}
	return n;
}

void ui_set_tile(int idx, const char *spec)
{
	char f[2][UI_TEXT_MAX] = { 0 };
	if (idx < 0 || idx >= UI_TILES) {
		return;
	}
	int n = split(spec ? spec : "", f, 2);
	k_mutex_lock(&lock, K_FOREVER);
	tiles[idx].icon = icon_by_name(f[0], strlen(f[0]));
	strcpy(tiles[idx].text, n > 1 ? f[1] : "");
	k_mutex_unlock(&lock);
	if (on) {
		k_work_submit(&draw_work);
	}
}

void ui_set_title(const char *s)
{
	k_mutex_lock(&lock, K_FOREVER);
	strncpy(title, s ? s : "", sizeof(title) - 1);
	title[sizeof(title) - 1] = 0;
	k_mutex_unlock(&lock);
	if (on) {
		k_work_submit(&draw_work);
	}
}

void ui_set_alert(const char *spec)
{
	char f[4][UI_TEXT_MAX] = { 0 };
	if (!spec || !spec[0]) {
		ui_clear_alert();
		return;
	}
	split(spec, f, 4);
	k_mutex_lock(&lock, K_FOREVER);
	alert.icon = icon_by_name(f[0], strlen(f[0]));
	for (int i = 0; i < 3; i++) {
		strcpy(alert.lines[i], f[i + 1]);
	}
	alert.phase = 0;
	alert.active = true;
	k_mutex_unlock(&lock);
	oled_wake(ALERT_ON_MS);
	k_work_reschedule(&anim_work, K_MSEC(ANIM_MS));
}

void ui_clear_alert(void)
{
	if (!alert.active) {
		return;
	}
	alert.active = false;
	k_work_cancel_delayable(&anim_work);
	if (on) {
		k_work_submit(&draw_work);
	}
}

bool ui_alert_active(void)
{
	return alert.active;
}

void ui_set_status(int battery_pct, bool is_linked)
{
	batt_pct = battery_pct;
	linked = is_linked;
	if (on) {
		k_work_submit(&draw_work);
	}
}

void ui_set_focus(int f)
{
	if (f < 0 || f > UI_TILES) {
		f = 0;
	}
	if (focus == f) {
		return;
	}
	focus = f;
	if (on) {
		k_work_submit(&draw_work);
	}
}

void ui_set_charging(bool c)
{
	if (charging == c) {
		return;
	}
	charging = c;
	if (on) {
		k_work_submit(&draw_work);
	}
}

void ui_set_flip(bool f)
{
	flip = f;
	if (on) {
		k_work_submit(&draw_work);
	}
}

void ui_set_offset(int x, int y)
{
	xoff = CLAMP(x, -32, 32);
	yoff = CLAMP(y, -32, 32);
	frame_until = k_uptime_get() + 20000;
	oled_wake(20000);
}

void oled_wake(int ms)
{
	if (!ready) {
		return;
	}
	k_work_submit(&draw_work);
	k_work_reschedule(&off_work, K_MSEC(ms > 0 ? ms : OLED_ON_MS));
	if (alert.active) {
		k_work_reschedule(&anim_work, K_MSEC(ANIM_MS));
	}
}

int oled_init(void)
{
	int err = gpio_pin_configure_dt(&vcc, GPIO_OUTPUT_ACTIVE);
	if (err) {
		return err;
	}
	k_msleep(50);
	err = device_init(disp);
	if (err && err != -EALREADY) {
		LOG_ERR("display init failed: %d", err);
		return err;
	}
	if (!device_is_ready(disp)) {
		return -ENODEV;
	}
	display_set_contrast(disp, 0x60);
	ready = true;
	LOG_INF("oled ready");
	return 0;
}
