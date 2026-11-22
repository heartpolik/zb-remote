/* zb-remote: 14-button sleepy Zigbee remote (nRF52840 Pro Micro) for Zigbee2MQTT.
 *
 * Endpoint 1, HA profile, device "Remote Control":
 *   Basic, Identify, Power Config (battery), Multistate Input (button actions).
 * Button action -> Report Attributes genMultistateInput.presentValue = btn*10 + action
 *   action: 1 single, 2 double, 3 hold.
 */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <stdio.h>

#include <zboss_api.h>
#include <zboss_api_addons.h>
#include <zigbee/zigbee_error_handler.h>
#include <zigbee/zigbee_app_utils.h>
#include <zb_nrf_platform.h>
#include <ram_pwrdn.h>

#include "buttons.h"
#include "keypad.h"
#include "keymap.h"
#include <zephyr/sys/reboot.h>
#include "battery.h"
#include "led.h"
#include "oled.h"
#include "display_cluster.h"
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/fatal.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/reboot.h>

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

/* last fatal error survives the software reset (noinit RAM) and is logged on the next boot */
static __noinit struct {
	uint32_t magic, reason, pc, lr, thread;
} crash;
#define CRASH_MAGIC 0xC0FFEE11

void k_sys_fatal_error_handler(unsigned int reason, const struct arch_esf *esf)
{
	crash.magic = CRASH_MAGIC;
	crash.reason = reason;
	crash.pc = esf ? esf->basic.pc : 0;
	crash.lr = esf ? esf->basic.lr : 0;
	crash.thread = (uint32_t)k_current_get();
	sys_reboot(SYS_REBOOT_COLD);
}

/* ZBOSS internal assert (linked with --wrap=zb_assert) -> remembered across the reset */
extern void __real_zb_assert(zb_uint16_t file_id, zb_int_t line);
void __wrap_zb_assert(zb_uint16_t file_id, zb_int_t line)
{
	crash.magic = CRASH_MAGIC;
	crash.reason = 0xA55E;
	crash.pc = file_id;
	crash.lr = (uint32_t)line;
	crash.thread = 0;
	if (!k_is_in_isr()) {
		LOG_ERR("ZBOSS assert file_id %u line %d, rebooting in 1 s", file_id, line);
		k_msleep(1000); /* let the USB console flush the log */
	}
	__real_zb_assert(file_id, line);
}


/* file_id 122 = zb_nvram.c: corrupted NVRAM makes ZBOSS assert on every boot.
 * Wipe the partition once so the stack starts clean (network has to be re-joined). */
#define ZB_FILE_ID_NVRAM 122
static int erase_partition(uint8_t id, const char *name)
{
	const struct flash_area *fa;
	int err = flash_area_open(id, &fa);
	if (!err) {
		err = flash_area_erase(fa, 0, fa->fa_size);
		flash_area_close(fa);
	}
	LOG_WRN("%s erased: %d", name, err);
	return err;
}

/* NVRAM datasets are encrypted with a PSA master key kept in trusted storage (settings/NVS
 * in the "storage" partition); if either is corrupted the stack asserts while saving keys.
 * Wipe both together: the device leaves the network and has to be re-joined. */
static void nvram_self_heal(void)
{
	erase_partition(FIXED_PARTITION_ID(zboss_nvram), "zboss nvram");
	erase_partition(FIXED_PARTITION_ID(storage_partition), "settings storage (PSA ITS)");
	/* settings/NVS may already be mounted on the old content -> restart clean */
	crash.magic = 0;
	k_msleep(500);
	sys_reboot(SYS_REBOOT_COLD);
}


#define EP                 1
#define DEVICE_ID          0x0006 /* HA Remote Control */
#define LONG_POLL_MS       5000
#define TURBO_POLL_MS      10000
#define BATTERY_PERIOD     K_HOURS(1)


/* ---------- attributes ---------- */
static struct {
	zb_uint8_t zcl_version;
	zb_uint8_t app_version;
	zb_uint8_t stack_version;
	zb_uint8_t hw_version;
	zb_char_t mf_name[16];
	zb_char_t model_id[16];
	zb_char_t date_code[10];
	zb_uint8_t power_source;
	zb_char_t location_id[2];
	zb_uint8_t ph_env;
	zb_char_t sw_build_id[12];
} basic;

static zb_uint16_t identify_time;

static struct {
	zb_uint8_t voltage;       /* 100 mV units */
	zb_uint8_t size;
	zb_uint8_t quantity;
	zb_uint8_t rated_voltage;
	zb_uint8_t alarm_mask;
	zb_uint8_t voltage_min_th;
	zb_uint8_t remaining;     /* 0.5 % units */
	zb_uint8_t th1, th2, th3, min_th, pth1, pth2, pth3;
	zb_uint32_t alarm_state;
} batt;

static struct {
	zb_char_t description[2];
	zb_uint16_t num_states;
	zb_bool_t out_of_service;
	zb_uint16_t present_value;
	zb_uint8_t reliability;
	zb_uint8_t status_flags;
	zb_uint32_t app_type;
} msi;

ZB_ZCL_DECLARE_BASIC_ATTRIB_LIST_EXT(basic_attr_list, &basic.zcl_version, &basic.app_version,
	&basic.stack_version, &basic.hw_version, basic.mf_name, basic.model_id, basic.date_code,
	&basic.power_source, basic.location_id, &basic.ph_env, basic.sw_build_id);

ZB_ZCL_DECLARE_IDENTIFY_ATTRIB_LIST(identify_attr_list, &identify_time);

ZB_ZCL_START_DECLARE_ATTRIB_LIST_CLUSTER_REVISION(power_attr_list, ZB_ZCL_POWER_CONFIG)
ZB_ZCL_POWER_CONFIG_BATTERY_ATTRIB_LIST_EXT(, &batt.voltage, &batt.size,
	&batt.quantity, &batt.rated_voltage, &batt.alarm_mask, &batt.voltage_min_th, &batt.remaining,
	&batt.th1, &batt.th2, &batt.th3, &batt.min_th, &batt.pth1, &batt.pth2, &batt.pth3,
	&batt.alarm_state)
ZB_ZCL_FINISH_DECLARE_ATTRIB_LIST;

ZB_ZCL_DECLARE_MULTI_INPUT_ATTRIB_LIST(msi_attr_list, msi.description, &msi.num_states,
	&msi.out_of_service, &msi.present_value, &msi.reliability, &msi.status_flags, &msi.app_type);

static zb_zcl_cluster_desc_t clusters[] = {
	ZB_ZCL_CLUSTER_DESC(ZB_ZCL_CLUSTER_ID_BASIC, ZB_ZCL_ARRAY_SIZE(basic_attr_list, zb_zcl_attr_t),
			    basic_attr_list, ZB_ZCL_CLUSTER_SERVER_ROLE, ZB_ZCL_MANUF_CODE_INVALID),
	ZB_ZCL_CLUSTER_DESC(ZB_ZCL_CLUSTER_ID_IDENTIFY,
			    ZB_ZCL_ARRAY_SIZE(identify_attr_list, zb_zcl_attr_t), identify_attr_list,
			    ZB_ZCL_CLUSTER_SERVER_ROLE, ZB_ZCL_MANUF_CODE_INVALID),
	ZB_ZCL_CLUSTER_DESC(ZB_ZCL_CLUSTER_ID_POWER_CONFIG,
			    ZB_ZCL_ARRAY_SIZE(power_attr_list, zb_zcl_attr_t), power_attr_list,
			    ZB_ZCL_CLUSTER_SERVER_ROLE, ZB_ZCL_MANUF_CODE_INVALID),
	ZB_ZCL_CLUSTER_DESC(ZB_ZCL_CLUSTER_ID_MULTI_INPUT,
			    ZB_ZCL_ARRAY_SIZE(msi_attr_list, zb_zcl_attr_t), msi_attr_list,
			    ZB_ZCL_CLUSTER_SERVER_ROLE, ZB_ZCL_MANUF_CODE_INVALID),
	ZB_ZCL_CLUSTER_DESC(ZB_ZCL_CLUSTER_ID_REMOTE_DISPLAY, DISPLAY_ATTR_COUNT, display_attr_list,
			    ZB_ZCL_CLUSTER_SERVER_ROLE, ZB_ZCL_MANUF_CODE_INVALID),
};

ZB_DECLARE_SIMPLE_DESC(5, 0);
ZB_AF_SIMPLE_DESC_TYPE(5, 0) simple_desc_remote = {
	EP, ZB_AF_HA_PROFILE_ID, DEVICE_ID, 0, 0, 5, 0,
	{ ZB_ZCL_CLUSTER_ID_BASIC, ZB_ZCL_CLUSTER_ID_IDENTIFY, ZB_ZCL_CLUSTER_ID_POWER_CONFIG,
	  ZB_ZCL_CLUSTER_ID_MULTI_INPUT, ZB_ZCL_CLUSTER_ID_REMOTE_DISPLAY }
};

ZB_AF_DECLARE_ENDPOINT_DESC(remote_ep, EP, ZB_AF_HA_PROFILE_ID, 0, NULL,
	ZB_ZCL_ARRAY_SIZE(clusters, zb_zcl_cluster_desc_t), clusters,
	(zb_af_simple_desc_1_1_t *)&simple_desc_remote, 0, NULL, 0, NULL);

ZBOSS_DECLARE_DEVICE_CTX_1_EP(remote_ctx, remote_ep);

static void attrs_init(void)
{
	basic.zcl_version = ZB_ZCL_VERSION;
	basic.app_version = 1;
	basic.stack_version = 1;
	basic.hw_version = 1;
	ZB_ZCL_SET_STRING_VAL(basic.mf_name, "heartpolik", ZB_ZCL_STRING_CONST_SIZE("heartpolik"));
	ZB_ZCL_SET_STRING_VAL(basic.model_id, "zb-remote-14", ZB_ZCL_STRING_CONST_SIZE("zb-remote-14"));
	ZB_ZCL_SET_STRING_VAL(basic.date_code, "20261117", ZB_ZCL_STRING_CONST_SIZE("20261117"));
	ZB_ZCL_SET_STRING_VAL(basic.sw_build_id, "0.1.0", ZB_ZCL_STRING_CONST_SIZE("0.1.0"));
	basic.power_source = ZB_ZCL_BASIC_POWER_SOURCE_BATTERY;
	basic.ph_env = ZB_ZCL_BASIC_PHYSICAL_ENVIRONMENT_DEFAULT_VALUE;

	identify_time = ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE;

	batt.size = ZB_ZCL_POWER_CONFIG_BATTERY_SIZE_OTHER;
	batt.quantity = 1;
	batt.rated_voltage = 37;
	batt.voltage = 0xff;
	batt.remaining = 0xff;

	msi.num_states = 14 * 10 + 3;
	msi.out_of_service = ZB_FALSE;
	msi.reliability = ZB_ZCL_MULTI_INPUT_RELIABILITY_NO_FAULT_DETECTED;
	msi.status_flags = ZB_ZCL_MULTI_INPUT_STATUS_FLAG_NORMAL;
}

/* ---------- manual attribute reports to the coordinator ---------- */
struct report_req {
	zb_uint16_t cluster;
	zb_uint8_t n;
	struct { zb_uint16_t id; zb_uint8_t type; zb_uint16_t val; } a[2];
};

#define REPORT_Q_LEN 16
K_MSGQ_DEFINE(report_q, sizeof(struct report_req), REPORT_Q_LEN, 4);

static void send_report_buf(zb_cb_param_t bufid)
{
	zb_bufid_t buf = (zb_bufid_t)bufid;
	struct report_req r;
	zb_uint16_t dst = 0x0000;
	if (k_msgq_get(&report_q, &r, K_NO_WAIT)) {
		zb_buf_free(buf);
		return;
	}

	zb_uint8_t *ptr = ZB_ZCL_START_PACKET(buf);
	ZB_ZCL_CONSTRUCT_GENERAL_COMMAND_RESP_FRAME_CONTROL(ptr);
	ZB_ZCL_CONSTRUCT_COMMAND_HEADER(ptr, ZB_ZCL_GET_SEQ_NUM(), ZB_ZCL_CMD_REPORT_ATTRIB);
	for (int i = 0; i < r.n; i++) {
		ZB_ZCL_PACKET_PUT_DATA16_VAL(ptr, r.a[i].id);
		ZB_ZCL_PACKET_PUT_DATA8(ptr, r.a[i].type);
		if (r.a[i].type == ZB_ZCL_ATTR_TYPE_U16) {
			ZB_ZCL_PACKET_PUT_DATA16_VAL(ptr, r.a[i].val);
		} else {
			ZB_ZCL_PACKET_PUT_DATA8(ptr, (zb_uint8_t)r.a[i].val);
		}
	}
	ZB_ZCL_FINISH_N_SEND_PACKET(buf, ptr, dst, ZB_APS_ADDR_MODE_16_ENDP_PRESENT, 1, EP,
				    ZB_AF_HA_PROFILE_ID, r.cluster, NULL);
}

static void request_send(zb_cb_param_t unused)
{
	ZVUNUSED(unused);
	zb_buf_get_out_delayed_ext(send_report_buf, 0, 0);
}

static void queue_report(const struct report_req *r)
{
	if (k_msgq_put(&report_q, r, K_NO_WAIT)) {
		LOG_WRN("report queue full");
		return;
	}
	if (ZB_JOINED()) {
		zigbee_schedule_callback(request_send, 0);
	} else {
		LOG_INF("not joined yet, report queued");
	}
}

static void flush_reports(void)
{
	for (uint32_t i = k_msgq_num_used_get(&report_q); i > 0; i--) {
		zigbee_schedule_callback(request_send, 0);
	}
}

/* ---------- battery ---------- */
static int last_pct = -1;
bool ha_tiles; /* set once HA wrote any tile */
static void battery_update(void)
{
	int mv = battery_mv();
	if (mv < 0) {
		LOG_WRN("battery read failed: %d", mv);
		return;
	}
	uint8_t pct = battery_percent(mv);
	batt.voltage = MIN(mv / 100, 0xfe);
	batt.remaining = MIN(pct * 2, 200);
	LOG_INF("battery %d mV, %d %%", mv, pct);
	last_pct = pct;
	ui_set_status(pct, ZB_JOINED());


	struct report_req r = {
		.cluster = ZB_ZCL_CLUSTER_ID_POWER_CONFIG, .n = 2,
		.a = {
			{ ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_ID, ZB_ZCL_ATTR_TYPE_U8, batt.voltage },
			{ ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID, ZB_ZCL_ATTR_TYPE_U8,
			  batt.remaining },
		},
	};
	queue_report(&r);
}

static void battery_work_fn(struct k_work *w);
static K_WORK_DELAYABLE_DEFINE(battery_work, battery_work_fn);
static int64_t last_battery_ms = -1;

static void battery_work_fn(struct k_work *w)
{
	battery_update();
	last_battery_ms = k_uptime_get();
	k_work_reschedule(&battery_work, BATTERY_PERIOD);
}

/* ---------- buttons ---------- */
static void turbo_poll(zb_cb_param_t unused)
{
	ZVUNUSED(unused);
	zb_zdo_pim_start_turbo_poll_continuous(TURBO_POLL_MS);
}

static void on_button(uint8_t btn, enum btn_action action)
{
	static const char *names[] = { "", "single", "double", "hold" };
	uint16_t value = btn * 10 + action;

	LOG_INF("button %u %s -> %u", btn, names[action], value);
	led_blink(1, 30, 0);
	if (!ha_tiles) {
		char b[24];
		snprintf(b, sizeof(b), "button|%u%s", btn, action == BTN_DOUBLE ? " x2" : action == BTN_HOLD ? " д" : "");
		ui_set_tile(1, b);
	}
	if (ui_alert_active()) {
		ui_clear_alert(); /* any button dismisses the alert locally */
	}
	oled_wake(0);
	msi.present_value = value;

	struct report_req r = {
		.cluster = ZB_ZCL_CLUSTER_ID_MULTI_INPUT, .n = 1,
		.a = { { ZB_ZCL_ATTR_MULTI_INPUT_PRESENT_VALUE_ID, ZB_ZCL_ATTR_TYPE_U16, value } },
	};
	queue_report(&r);
	/* stay responsive for a while so HA can answer (display text etc.) */
	zigbee_schedule_callback(turbo_poll, 0);

	if (last_battery_ms < 0 || k_uptime_get() - last_battery_ms > 10 * 60 * 1000) {
		k_work_reschedule(&battery_work, K_MSEC(500));
	}
}

/* ---------- keypad raw pairs: learn view + factory reset ---------- */
static bool keymap_empty(void)
{
	for (int k = 0; k < 14; k++) {
		if (keymap[k][0] >= 0) {
			return false;
		}
	}
	return true;
}

static void factory_reset_fn(struct k_work *w)
{
	LOG_WRN("factory reset: leaving network");
	zigbee_erase_persistent_storage(ZB_TRUE);
	k_msleep(200);
	sys_reboot(SYS_REBOOT_COLD);
}
static K_WORK_DELAYABLE_DEFINE(factory_reset_work, factory_reset_fn);

static void on_raw_pair(int a, int b, bool pressed)
{
	/* hold key 1 (or any pair while the keymap is empty) for 10 s -> factory reset */
	bool is_key1 = (keymap[0][0] == a && keymap[0][1] == b) || (keymap[0][0] == b && keymap[0][1] == a);
	if (is_key1 || keymap_empty()) {
		if (pressed) {
			k_work_reschedule(&factory_reset_work, K_SECONDS(10));
		} else {
			k_work_cancel_delayable(&factory_reset_work);
		}
	}
	if (pressed && keymap_empty()) {
		char l[24];
		snprintf(l, sizeof(l), "пади %d-%d", a + 1, b + 1);
		ui_set_tile(1, "");
		char spec[40];
		snprintf(spec, sizeof(spec), "button|%s", l);
		ui_set_tile(0, spec);
		oled_wake(0);
		led_blink(1, 30, 0);
	}
}

/* ---------- zigbee signals ---------- */
void zboss_signal_handler(zb_cb_param_t bufid)
{
	zb_zdo_app_signal_hdr_t *hdr = NULL;
	zb_zdo_app_signal_type_t sig = zb_get_app_signal(bufid, &hdr);
	zb_ret_t status = ZB_GET_APP_SIGNAL_STATUS(bufid);

	if (sig != ZB_COMMON_SIGNAL_CAN_SLEEP) {
		LOG_INF("zb signal %d status %d", sig, status);
	}

	switch (sig) {
	case ZB_BDB_SIGNAL_DEVICE_REBOOT:
	case ZB_BDB_SIGNAL_STEERING:
		if (status == RET_OK && ZB_JOINED()) {
			LOG_INF("joined network, PAN 0x%04x", ZB_PIBCACHE_PAN_ID());
			led_searching(false);
			led_blink(3, 100, 150);
			ui_set_status(last_pct, true);
			if (!ha_tiles) {
				ui_set_tile(0, "link|OK");
			}
			flush_reports();
			zb_zdo_pim_set_long_poll_interval(LONG_POLL_MS);
			k_work_reschedule(&battery_work, K_SECONDS(3));
		}
		if (status != RET_OK) {
			led_searching(true);
		}
		break;
	case ZB_ZDO_SIGNAL_LEAVE:
		LOG_INF("left network");
		ui_set_status(last_pct, false);
		led_searching(true);
		break;
	default:
		break;
	}

	ZB_ERROR_CHECK(zigbee_default_signal_handler(bufid));
	if (bufid) {
		zb_buf_free(bufid);
	}
}

static void wait_for_host(void)
{
#if defined(CONFIG_UART_LINE_CTRL)
	const struct device *con = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
	uint32_t dtr = 0;
	for (int i = 0; i < 200 && !dtr; i++) {
		uart_line_ctrl_get(con, UART_LINE_CTRL_DTR, &dtr);
		k_msleep(100);
	}
#endif
}

int main(void)
{
	if (IS_ENABLED(CONFIG_APP_WAIT_HOST)) {
		wait_for_host();
	}
	uint32_t rc = 0;
	hwinfo_get_reset_cause(&rc);
	hwinfo_clear_reset_cause();
	LOG_INF("zb-remote starting, reset cause 0x%08x", rc);
	if (crash.magic == CRASH_MAGIC) {
		if (crash.reason == 0xA55E) {
			LOG_ERR("previous ZBOSS assert: file_id %u line %u", crash.pc, crash.lr);
			if (crash.pc == ZB_FILE_ID_NVRAM) {
				nvram_self_heal();
			}
		} else {
			LOG_ERR("previous crash: reason %u pc 0x%08x lr 0x%08x thread 0x%08x",
				crash.reason, crash.pc, crash.lr, crash.thread);
		}
		crash.magic = 0;
	}
	led_init();
	led_blink(1, 1000, 0);
	if (oled_init() == 0) {
		ui_set_tile(0, "search|…");
		ui_set_tile(1, "button|—");
		oled_wake(15000);
	}

	buttons_init(on_button);
	int kerr = keypad_init(on_raw_pair);
	if (kerr) {
		LOG_ERR("keypad init failed: %d", kerr);
	}

	zigbee_configure_sleepy_behavior(true);
	ZB_AF_REGISTER_DEVICE_CTX(&remote_ctx);
	attrs_init();

	if (IS_ENABLED(CONFIG_APP_RAM_PWRDN)) {
		power_down_unused_ram();
	}

	led_searching(true);
	zigbee_enable();
	LOG_INF("zigbee enabled");
	return 0;
}
