/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * BLE HID Consumer Control (Media Remote) for nRF54L15-DK
 * NFC-triggered BLE advertising: tap NFC to advertise until connected
 * Buttons: Vol+, Vol-, Play/Pause, Next Track
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <bluetooth/services/hids.h>

#include <nfc_t2t_lib.h>
#include <nfc/ndef/msg.h>
#include <nfc/ndef/text_rec.h>

LOG_MODULE_REGISTER(ble_hello, LOG_LEVEL_INF);

/* HID Consumer Control report configuration */
#define REPORT_ID_CONSUMER_CTRL   1
#define REPORT_IDX_CONSUMER_CTRL  0
#define REPORT_SIZE_CONSUMER_CTRL 2

/* Consumer Control usage IDs */
#define USAGE_VOL_UP     0x00E9
#define USAGE_VOL_DOWN   0x00EA
#define USAGE_PLAY_PAUSE 0x00CD
#define USAGE_NEXT_TRACK 0x00B5

/* HID report descriptor: single 16-bit consumer control input */
static const uint8_t report_map[] = {
	0x05, 0x0C,       /* Usage Page (Consumer Control) */
	0x09, 0x01,       /* Usage (Consumer Control) */
	0xA1, 0x01,       /* Collection (Application) */
	0x85, REPORT_ID_CONSUMER_CTRL, /* Report ID */
	0x15, 0x00,       /* Logical Minimum (0) */
	0x26, 0xFF, 0x03, /* Logical Maximum (0x03FF) */
	0x19, 0x00,       /* Usage Minimum (0) */
	0x2A, 0xFF, 0x03, /* Usage Maximum (0x03FF) */
	0x75, 0x10,       /* Report Size (16 bits) */
	0x95, 0x01,       /* Report Count (1) */
	0x81, 0x00,       /* Input (Data, Array, Absolute) */
	0xC0,             /* End Collection */
};

BT_HIDS_DEF(hids_obj, REPORT_SIZE_CONSUMER_CTRL);

/* LED definitions */
#define LED0_NODE DT_ALIAS(led0)
#define LED1_NODE DT_ALIAS(led1)

static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(LED1_NODE, gpios);

/* Button definitions: 4 buttons for media control */
#define BTN0_NODE DT_ALIAS(sw0)
#define BTN1_NODE DT_ALIAS(sw1)
#define BTN2_NODE DT_ALIAS(sw2)
#define BTN3_NODE DT_ALIAS(sw3)

static const struct gpio_dt_spec buttons[] = {
	GPIO_DT_SPEC_GET(BTN0_NODE, gpios),
	GPIO_DT_SPEC_GET(BTN1_NODE, gpios),
	GPIO_DT_SPEC_GET(BTN2_NODE, gpios),
	GPIO_DT_SPEC_GET(BTN3_NODE, gpios),
};

#define NUM_BUTTONS ARRAY_SIZE(buttons)

/* Map button index to consumer control usage ID */
static const uint16_t button_usage[] = {
	USAGE_VOL_UP,     /* Button1 */
	USAGE_VOL_DOWN,   /* Button2 */
	USAGE_PLAY_PAUSE, /* Button3 */
	USAGE_NEXT_TRACK, /* Button4 */
};

static struct gpio_callback btn_cb_port0;
static struct gpio_callback btn_cb_port1;
static struct bt_conn *current_conn;
static bool is_connected;
static bool is_advertising;

/* Pending button press to send via work item */
static volatile uint16_t pending_usage;

/* NFC NDEF message buffer */
#define NDEF_MSG_BUF_SIZE 128
static uint8_t ndef_msg_buf[NDEF_MSG_BUF_SIZE];

/* NDEF text record payload */
static const uint8_t en_code[] = "en";
static const uint8_t en_payload[] = "nRF54L15 BLE Hello";

NFC_NDEF_TEXT_RECORD_DESC_DEF(nfc_text_rec,
			      UTF_8,
			      en_code,
			      sizeof(en_code),
			      en_payload,
			      sizeof(en_payload));

NFC_NDEF_MSG_DEF(nfc_text_msg, 1);

/* Advertising data */
static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL,
		      BT_UUID_16_ENCODE(BT_UUID_HIDS_VAL),
		      BT_UUID_16_ENCODE(BT_UUID_BAS_VAL)),
};

static const struct bt_data sd[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME,
		sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

/* Forward declarations */
static void advertising_start(void);
static void adv_start_work_handler(struct k_work *work);
static void led_blink_work_handler(struct k_work *work);
static void nfc_led_blink_work_handler(struct k_work *work);
static void hid_report_work_handler(struct k_work *work);

K_WORK_DEFINE(adv_start_work, adv_start_work_handler);
K_WORK_DELAYABLE_DEFINE(led_blink_work, led_blink_work_handler);
K_WORK_DELAYABLE_DEFINE(nfc_led_blink_work, nfc_led_blink_work_handler);
K_WORK_DEFINE(hid_report_work, hid_report_work_handler);

/* NFC LED1 blink: 5 blinks = 10 toggles */
#define NFC_LED_BLINK_COUNT 10
static int nfc_led_blink_remaining;

static void send_consumer_ctrl(uint16_t usage_id)
{
	int err;
	uint8_t buf[REPORT_SIZE_CONSUMER_CTRL];

	if (!current_conn) {
		return;
	}

	sys_put_le16(usage_id, buf);
	err = bt_hids_inp_rep_send(&hids_obj, current_conn,
				   REPORT_IDX_CONSUMER_CTRL,
				   buf, sizeof(buf), NULL);
	if (err) {
		LOG_ERR("HID report send failed (err %d)", err);
	}
}

static void hid_report_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);
	uint16_t usage = pending_usage;

	if (!is_connected || !usage) {
		return;
	}

	LOG_INF("Sending HID consumer ctrl: 0x%04X", usage);

	/* Key press */
	send_consumer_ctrl(usage);

	/* Brief delay then key release */
	k_sleep(K_MSEC(50));
	send_consumer_ctrl(0x0000);
}

static void adv_start_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);
	advertising_start();
}

static void advertising_start(void)
{
	int err;

	if (is_connected) {
		LOG_INF("Already connected, ignoring NFC tap");
		return;
	}

	if (is_advertising) {
		/* Stop and restart advertising so iOS rescans */
		LOG_INF("Restarting advertising");
		bt_le_adv_stop();
		is_advertising = false;
	}

	err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad),
			      sd, ARRAY_SIZE(sd));
	if (err) {
		LOG_ERR("Advertising failed to start (err %d)", err);
		return;
	}

	is_advertising = true;
	LOG_INF("Advertising started (until connected)");

	/* Start LED0 blink */
	k_work_schedule(&led_blink_work, K_NO_WAIT);
}

static void led_blink_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	if (is_advertising && !is_connected) {
		gpio_pin_toggle_dt(&led0);
		k_work_schedule(&led_blink_work, K_MSEC(500));
	}
}

static void nfc_led_blink_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	if (nfc_led_blink_remaining > 0) {
		gpio_pin_toggle_dt(&led1);
		nfc_led_blink_remaining--;
		k_work_schedule(&nfc_led_blink_work, K_MSEC(100));
	} else {
		gpio_pin_set_dt(&led1, 0);
	}
}

/* NFC callback - called from ISR context */
static void nfc_callback(void *context,
			 nfc_t2t_event_t event,
			 const uint8_t *data,
			 size_t data_length)
{
	ARG_UNUSED(context);
	ARG_UNUSED(data);
	ARG_UNUSED(data_length);

	switch (event) {
	case NFC_T2T_EVENT_FIELD_ON:
		LOG_INF("NFC field detected");
		nfc_led_blink_remaining = NFC_LED_BLINK_COUNT;
		k_work_schedule(&nfc_led_blink_work, K_NO_WAIT);
		k_work_submit(&adv_start_work);
		break;
	case NFC_T2T_EVENT_FIELD_OFF:
		LOG_INF("NFC field removed");
		break;
	default:
		break;
	}
}

/* Connection callbacks */
static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		LOG_ERR("Connection failed (err %u)", err);
		return;
	}

	LOG_INF("Connected");
	is_connected = true;
	is_advertising = false;
	current_conn = bt_conn_ref(conn);

	/* Cancel blink */
	k_work_cancel_delayable(&led_blink_work);

	/* LED0 solid when connected */
	gpio_pin_set_dt(&led0, 1);

	int ret = bt_hids_connected(&hids_obj, conn);

	if (ret) {
		LOG_ERR("HIDS connected notify failed (err %d)", ret);
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	LOG_INF("Disconnected (reason %u)", reason);

	int ret = bt_hids_disconnected(&hids_obj, conn);

	if (ret) {
		LOG_ERR("HIDS disconnected notify failed (err %d)", ret);
	}

	if (current_conn) {
		bt_conn_unref(current_conn);
		current_conn = NULL;
	}

	is_connected = false;

	/* LED0 off when disconnected (no auto-readvertise) */
	gpio_pin_set_dt(&led0, 0);
}

static void security_changed(struct bt_conn *conn, bt_security_t level,
			     enum bt_security_err err)
{
	if (err) {
		LOG_ERR("Security failed: level %u err %d", level, err);
	} else {
		LOG_INF("Security changed: level %u", level);
	}
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
	.security_changed = security_changed,
};

/* Button callback - ISR context */
static void button_pressed(const struct device *dev, struct gpio_callback *cb,
			   uint32_t pins)
{
	for (int i = 0; i < NUM_BUTTONS; i++) {
		if (pins & BIT(buttons[i].pin)) {
			LOG_INF("Button%d pressed", i + 1);
			pending_usage = button_usage[i];
			k_work_submit(&hid_report_work);
			return;
		}
	}
}

static int init_gpio(void)
{
	int ret;
	uint32_t mask_port0 = 0;
	uint32_t mask_port1 = 0;
	const struct device *port0_dev = NULL;
	const struct device *port1_dev = NULL;

	if (!gpio_is_ready_dt(&led0) || !gpio_is_ready_dt(&led1)) {
		LOG_ERR("LED GPIO devices not ready");
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&led0, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		return ret;
	}

	ret = gpio_pin_configure_dt(&led1, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		return ret;
	}

	/* Configure all 4 buttons and sort by port */
	for (int i = 0; i < NUM_BUTTONS; i++) {
		if (!gpio_is_ready_dt(&buttons[i])) {
			LOG_ERR("Button%d GPIO not ready", i + 1);
			return -ENODEV;
		}

		ret = gpio_pin_configure_dt(&buttons[i], GPIO_INPUT);
		if (ret < 0) {
			return ret;
		}

		ret = gpio_pin_interrupt_configure_dt(&buttons[i],
						      GPIO_INT_EDGE_TO_ACTIVE);
		if (ret < 0) {
			return ret;
		}

		/* Buttons span gpio0 and gpio1 on nRF54L15-DK.
		 * We need separate callbacks per port.
		 */
		if (port0_dev == NULL) {
			port0_dev = buttons[i].port;
			mask_port0 |= BIT(buttons[i].pin);
		} else if (buttons[i].port == port0_dev) {
			mask_port0 |= BIT(buttons[i].pin);
		} else {
			port1_dev = buttons[i].port;
			mask_port1 |= BIT(buttons[i].pin);
		}
	}

	/* Register callback for first port */
	if (port0_dev && mask_port0) {
		gpio_init_callback(&btn_cb_port0, button_pressed, mask_port0);
		ret = gpio_add_callback(port0_dev, &btn_cb_port0);
		if (ret < 0) {
			LOG_ERR("Failed to add callback for port0 (err %d)", ret);
			return ret;
		}
	}

	/* Register callback for second port (if buttons span two ports) */
	if (port1_dev && mask_port1) {
		gpio_init_callback(&btn_cb_port1, button_pressed, mask_port1);
		ret = gpio_add_callback(port1_dev, &btn_cb_port1);
		if (ret < 0) {
			LOG_ERR("Failed to add callback for port1 (err %d)", ret);
			return ret;
		}
	}

	return 0;
}

static int hid_init(void)
{
	int err;
	struct bt_hids_init_param hids_init_param = { 0 };
	struct bt_hids_inp_rep *rep;

	hids_init_param.rep_map.data = report_map;
	hids_init_param.rep_map.size = sizeof(report_map);

	hids_init_param.info.bcd_hid = 0x0101; /* USB HID spec version 1.01 */
	hids_init_param.info.b_country_code = 0x00;
	hids_init_param.info.flags = (BT_HIDS_REMOTE_WAKE |
				      BT_HIDS_NORMALLY_CONNECTABLE);

	/* Single input report: consumer control */
	rep = &hids_init_param.inp_rep_group_init.reports[REPORT_IDX_CONSUMER_CTRL];
	rep->id = REPORT_ID_CONSUMER_CTRL;
	rep->size = REPORT_SIZE_CONSUMER_CTRL;
	hids_init_param.inp_rep_group_init.cnt = 1;

	err = bt_hids_init(&hids_obj, &hids_init_param);
	if (err) {
		LOG_ERR("HIDS init failed (err %d)", err);
		return err;
	}

	LOG_INF("HID service initialized");
	return 0;
}

static int nfc_init(void)
{
	int err;
	uint32_t len = sizeof(ndef_msg_buf);

	err = nfc_t2t_setup(nfc_callback, NULL);
	if (err) {
		LOG_ERR("NFC T2T setup failed (err %d)", err);
		return err;
	}

	err = nfc_ndef_msg_record_add(&NFC_NDEF_MSG(nfc_text_msg),
				      &NFC_NDEF_TEXT_RECORD_DESC(nfc_text_rec));
	if (err) {
		LOG_ERR("Failed to add NDEF record (err %d)", err);
		return err;
	}

	err = nfc_ndef_msg_encode(&NFC_NDEF_MSG(nfc_text_msg),
				  ndef_msg_buf, &len);
	if (err) {
		LOG_ERR("Failed to encode NDEF message (err %d)", err);
		return err;
	}

	err = nfc_t2t_payload_set(ndef_msg_buf, len);
	if (err) {
		LOG_ERR("Failed to set NFC payload (err %d)", err);
		return err;
	}

	err = nfc_t2t_emulation_start();
	if (err) {
		LOG_ERR("Failed to start NFC emulation (err %d)", err);
		return err;
	}

	LOG_INF("NFC T2T emulation started");
	return 0;
}

int main(void)
{
	int err;

	LOG_INF("BLE HID Media Remote - nRF54L15 (NFC-triggered)");

	err = init_gpio();
	if (err) {
		LOG_ERR("GPIO init failed (err %d)", err);
		return err;
	}

	err = bt_enable(NULL);
	if (err) {
		LOG_ERR("Bluetooth init failed (err %d)", err);
		return err;
	}

	LOG_INF("Bluetooth initialized");

	err = settings_load();
	if (err) {
		LOG_ERR("Settings load failed (err %d)", err);
		return err;
	}

	/* Hold Button4 at boot to clear all bond keys */
	if (gpio_pin_get_dt(&buttons[3]) == 1) {
		LOG_INF("Button4 held at boot - clearing all bonds!");
		bt_unpair(BT_ID_DEFAULT, BT_ADDR_LE_ANY);

		/* Blink LED1 3 times to confirm */
		for (int i = 0; i < 6; i++) {
			gpio_pin_toggle_dt(&led1);
			k_sleep(K_MSEC(200));
		}
		gpio_pin_set_dt(&led1, 0);
		LOG_INF("All bonds cleared");
	} else {
		LOG_INF("Settings loaded (bonds restored)");
	}

	err = hid_init();
	if (err) {
		LOG_ERR("HID init failed (err %d)", err);
		return err;
	}

	err = nfc_init();
	if (err) {
		LOG_ERR("NFC init failed (err %d)", err);
		return err;
	}

	LOG_INF("Waiting for NFC tap to start advertising...");

	return 0;
}
