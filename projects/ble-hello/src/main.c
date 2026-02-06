/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * BLE Hello World for nRF54L15-DK
 * Advertises as a connectable peripheral with LED status indication
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/bluetooth/gatt.h>

LOG_MODULE_REGISTER(ble_hello, LOG_LEVEL_INF);

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

/* Advertising data */
static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

/* Connection callbacks */
static void connected(struct bt_conn *conn, uint8_t err)
{
    if (err) {
        LOG_ERR("Connection failed (err %u)", err);
        return;
    }

    LOG_INF("Connected");
    is_connected = true;
    gpio_pin_set_dt(&led0, 1);  /* Solid LED when connected */
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    LOG_INF("Disconnected (reason %u)", reason);
    is_connected = false;
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

    /* TODO: Send BLE notification here */
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

static void led_blink_work_handler(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(led_blink_work, led_blink_work_handler);

static void led_blink_work_handler(struct k_work *work)
{
    if (!is_connected) {
        gpio_pin_toggle_dt(&led0);
        k_work_schedule(&led_blink_work, K_MSEC(500));
    }
}

int main(void)
{
    int err;

    LOG_INF("BLE Hello World - nRF54L15");

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

    err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err) {
        LOG_ERR("Advertising failed to start (err %d)", err);
        return err;
    }

    LOG_INF("Advertising started");

    /* Start LED blinking while not connected */
    k_work_schedule(&led_blink_work, K_NO_WAIT);

    return 0;
}
