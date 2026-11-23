#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <zboss_api.h>
#include "display_cluster.h"
#include "oled.h"

LOG_MODULE_REGISTER(dispcl, LOG_LEVEL_INF);

static zb_uint8_t tile_attr[3][1 + DISPLAY_SPEC_BYTES];
static zb_uint8_t alert_attr[1 + DISPLAY_SPEC_BYTES];
static zb_uint8_t title_attr[1 + DISPLAY_SPEC_BYTES];
static zb_uint8_t wake_attr, flip_attr, charging_attr, focus_attr;
static zb_int8_t xoff_attr, yoff_attr;
static zb_uint16_t cluster_rev = 1;

#define RW ZB_ZCL_ATTR_ACCESS_READ_WRITE
#define NM ZB_ZCL_NON_MANUFACTURER_SPECIFIC
zb_zcl_attr_t display_attr_list[] = {
	{ ZB_ZCL_ATTR_GLOBAL_CLUSTER_REVISION_ID, ZB_ZCL_ATTR_TYPE_U16, ZB_ZCL_ATTR_ACCESS_READ_ONLY, NM,
	  &cluster_rev },
	{ 0x0000, ZB_ZCL_ATTR_TYPE_OCTET_STRING, RW, NM, tile_attr[0] },
	{ 0x0001, ZB_ZCL_ATTR_TYPE_OCTET_STRING, RW, NM, tile_attr[1] },
	{ 0x0002, ZB_ZCL_ATTR_TYPE_OCTET_STRING, RW, NM, tile_attr[2] },
	{ 0x0003, ZB_ZCL_ATTR_TYPE_U8, RW, NM, &wake_attr },
	{ 0x0004, ZB_ZCL_ATTR_TYPE_OCTET_STRING, RW, NM, alert_attr },
	{ 0x0005, ZB_ZCL_ATTR_TYPE_U8, RW, NM, &flip_attr },
	{ 0x0006, ZB_ZCL_ATTR_TYPE_S8, RW, NM, &xoff_attr },
	{ 0x0007, ZB_ZCL_ATTR_TYPE_S8, RW, NM, &yoff_attr },
	{ 0x0009, ZB_ZCL_ATTR_TYPE_OCTET_STRING, RW, NM, title_attr },
	{ DISPLAY_ATTR_CHARGING, ZB_ZCL_ATTR_TYPE_U8, ZB_ZCL_ATTR_ACCESS_READ_ONLY, NM, &charging_attr },
	{ 0x000B, ZB_ZCL_ATTR_TYPE_U8, RW, NM, &focus_attr },
	{ ZB_ZCL_NULL_ID, 0, 0, NM, NULL },
};
BUILD_ASSERT(ARRAY_SIZE(display_attr_list) == DISPLAY_ATTR_COUNT);

static bool is_str(zb_uint16_t id)
{
	return id <= 0x0002 || id == 0x0004 || id == 0x0009;
}

static zb_ret_t check_value(zb_uint16_t attr_id, zb_uint8_t ep, zb_uint8_t *value)
{
	if (is_str(attr_id) && value[0] > DISPLAY_SPEC_BYTES) {
		return RET_ERROR;
	}
	return RET_OK;
}

static void write_hook(zb_uint8_t ep, zb_uint16_t attr_id, zb_uint8_t *v, zb_uint16_t manuf)
{
	char tmp[DISPLAY_SPEC_BYTES + 1];

	if (is_str(attr_id)) {
		uint8_t n = MIN(v[0], DISPLAY_SPEC_BYTES);
		memcpy(tmp, &v[1], n);
		tmp[n] = 0;
		LOG_INF("attr 0x%04x = \"%s\"", attr_id, tmp);
		if (attr_id == 0x0009) {
			ui_set_title(tmp);
		} else if (attr_id == 0x0004) {
			ui_set_alert(tmp);
		} else {
			extern bool ha_tiles;
			ha_tiles = true;
			ui_set_tile(attr_id, tmp);
		}
	} else if (attr_id == 0x0003 && v[0]) {
		oled_wake(v[0] * 1000);
	} else if (attr_id == 0x000B) {
		ui_set_focus(v[0]);
	} else if (attr_id == 0x0005) {
		ui_set_flip(v[0] != 0);
	} else if (attr_id == 0x0006) {
		ui_set_offset((int8_t)v[0], yoff_attr);
	} else if (attr_id == 0x0007) {
		ui_set_offset(xoff_attr, (int8_t)v[0]);
	}
}

void display_set_charging(bool c)
{
	charging_attr = c ? 1 : 0;
}

void display_cluster_init(void)
{
	zb_zcl_add_cluster_handlers(ZB_ZCL_CLUSTER_ID_REMOTE_DISPLAY, ZB_ZCL_CLUSTER_SERVER_ROLE,
				    check_value, write_hook, NULL);
}
