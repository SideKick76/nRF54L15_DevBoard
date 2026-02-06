/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * BLE Hello World for nRF54L15-DK
 * NFC-triggered BLE advertising: tap NFC to start 30s advertising window
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/bluetooth/gatt.h>

#include <nfc_t2t_lib.h>
#include <nfc/ndef/msg.h>
#include <nfc/ndef/text_rec.h>

LOG_MODULE_REGISTER(ble_hello, LOG_LEVEL_INF);

/* Advertising timeout in seconds */
#define ADV_TIMEOUT_SEC 30

/* LED and Button definitions */
#define LED0_NODE DT_ALIAS(led0)
#define LED1_NODE DT_ALIAS(led1)
#define BTN0_NODE DT_ALIAS(sw0)

static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(LED1_NODE, gpios);
static const struct gpio_dt_spec btn0 = GPIO_DT_SPEC_GET(BTN0_NODE, gpios);

static struct gpio_callback btn_cb_data;
static uint32_t button_press_count;
static bool is_connected;
static bool is_advertising;

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
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

/* Forward declarations */
static void advertising_start(void);
static void adv_start_work_handler(struct k_work *work);
static void adv_timeout_work_handler(struct k_work *work);
static void led_blink_work_handler(struct k_work *work);
static void nfc_led_blink_work_handler(struct k_work *work);

K_WORK_DEFINE(adv_start_work, adv_start_work_handler);
K_WORK_DELAYABLE_DEFINE(adv_timeout_work, adv_timeout_work_handler);
K_WORK_DELAYABLE_DEFINE(led_blink_work, led_blink_work_handler);
K_WORK_DELAYABLE_DEFINE(nfc_led_blink_work, nfc_led_blink_work_handler);

/* NFC LED1 blink: 5 blinks = 10 toggles */
#define NFC_LED_BLINK_COUNT 10
static int nfc_led_blink_remaining;

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
		/* Already advertising: restart the 30s timer */
		LOG_INF("Restarting advertising timeout");
		k_work_reschedule(&adv_timeout_work, K_SECONDS(ADV_TIMEOUT_SEC));
		return;
	}

	err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		LOG_ERR("Advertising failed to start (err %d)", err);
		return;
	}

	is_advertising = true;
	LOG_INF("Advertising started (30s timeout)");

	/* Start LED0 blink */
	k_work_schedule(&led_blink_work, K_NO_WAIT);

	/* Schedule advertising stop after timeout */
	k_work_schedule(&adv_timeout_work, K_SECONDS(ADV_TIMEOUT_SEC));
}

static void advertising_stop(void)
{
	int err;

	if (!is_advertising) {
		return;
	}

	err = bt_le_adv_stop();
	if (err) {
		LOG_ERR("Advertising failed to stop (err %d)", err);
		return;
	}

	is_advertising = false;
	LOG_INF("Advertising stopped");

	/* Cancel blink and turn LED0 off */
	k_work_cancel_delayable(&led_blink_work);
	gpio_pin_set_dt(&led0, 0);
}

static void adv_timeout_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	LOG_INF("Advertising timeout reached");
	advertising_stop();
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

	/* Cancel advertising timeout and blink */
	k_work_cancel_delayable(&adv_timeout_work);
	k_work_cancel_delayable(&led_blink_work);

	/* LED0 solid when connected */
	gpio_pin_set_dt(&led0, 1);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	LOG_INF("Disconnected (reason %u)", reason);
	is_connected = false;

	/* LED0 off when disconnected (no auto-readvertise) */
	gpio_pin_set_dt(&led0, 0);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

/* Button callback */
static void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	button_press_count++;
	LOG_INF("Button pressed (count: %u)", button_press_count);
	gpio_pin_toggle_dt(&led1);
}

static int init_gpio(void)
{
	int ret;

	if (!gpio_is_ready_dt(&led0) || !gpio_is_ready_dt(&led1) || !gpio_is_ready_dt(&btn0)) {
		LOG_ERR("GPIO devices not ready");
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&led0, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) return ret;

	ret = gpio_pin_configure_dt(&led1, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) return ret;

	ret = gpio_pin_configure_dt(&btn0, GPIO_INPUT);
	if (ret < 0) return ret;

	ret = gpio_pin_interrupt_configure_dt(&btn0, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret < 0) return ret;

	gpio_init_callback(&btn_cb_data, button_pressed, BIT(btn0.pin));
	gpio_add_callback(btn0.port, &btn_cb_data);

	return 0;
}

static int nfc_init(void)
{
	int err;
	uint32_t len = sizeof(ndef_msg_buf);

	/* Setup NFC T2T with callback */
	err = nfc_t2t_setup(nfc_callback, NULL);
	if (err) {
		LOG_ERR("NFC T2T setup failed (err %d)", err);
		return err;
	}

	/* Build NDEF message with text record */
	err = nfc_ndef_msg_record_add(&NFC_NDEF_MSG(nfc_text_msg),
				      &NFC_NDEF_TEXT_RECORD_DESC(nfc_text_rec));
	if (err) {
		LOG_ERR("Failed to add NDEF record (err %d)", err);
		return err;
	}

	err = nfc_ndef_msg_encode(&NFC_NDEF_MSG(nfc_text_msg), ndef_msg_buf, &len);
	if (err) {
		LOG_ERR("Failed to encode NDEF message (err %d)", err);
		return err;
	}

	/* Set NDEF payload */
	err = nfc_t2t_payload_set(ndef_msg_buf, len);
	if (err) {
		LOG_ERR("Failed to set NFC payload (err %d)", err);
		return err;
	}

	/* Start NFC field sensing */
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

	LOG_INF("BLE Hello World - nRF54L15 (NFC-triggered)");

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

	err = nfc_init();
	if (err) {
		LOG_ERR("NFC init failed (err %d)", err);
		return err;
	}

	LOG_INF("Waiting for NFC tap to start advertising...");

	return 0;
}
