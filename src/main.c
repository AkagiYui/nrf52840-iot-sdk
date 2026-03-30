/*
 * Copyright (c) 2024 Your Name
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * BLE RGB LED Demo
 *
 * 通过 BLE GATT 自定义服务控制 RGB LED：
 *   - 写特征值 Red   (UUID ...0001)：0x00 熄灭 / 0x01 点亮红色 LED
 *   - 写特征值 Green (UUID ...0002)：0x00 熄灭 / 0x01 点亮绿色 LED
 *   - 写特征值 Blue  (UUID ...0003)：0x00 熄灭 / 0x01 点亮蓝色 LED
 *   - Notify 特征值  (UUID ...0004)：状态改变时主动推送 [R, G, B] 3字节
 *
 * 硬件：Seeed XIAO BLE (nRF52840)
 *   红 LED -> P0.26 (led0)
 *   绿 LED -> P0.30 (led1)
 *   蓝 LED -> P0.06 (led2)
 *
 * 服务 UUID: 12340000-0000-1000-8000-00805F9B34FB
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

/* ============ LED 定义 ============ */

#define LED0_NODE DT_ALIAS(led0)  /* 红 */
#define LED1_NODE DT_ALIAS(led1)  /* 绿 */
#define LED2_NODE DT_ALIAS(led2)  /* 蓝 */

static const struct gpio_dt_spec led_red   = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct gpio_dt_spec led_green = GPIO_DT_SPEC_GET(LED1_NODE, gpios);
static const struct gpio_dt_spec led_blue  = GPIO_DT_SPEC_GET(LED2_NODE, gpios);

/* 当前 LED 状态：0=灭, 1=亮 */
static uint8_t led_state[3]; /* [0]=Red, [1]=Green, [2]=Blue */

/* ============ BLE UUID 定义 ============ */
/* 基础 UUID: 12340000-0000-1000-8000-00805F9B34FB */
#define BT_UUID_RGB_SVC_VAL \
	BT_UUID_128_ENCODE(0x12340000, 0x0000, 0x1000, 0x8000, 0x00805F9B34FB)

#define BT_UUID_RGB_RED_VAL \
	BT_UUID_128_ENCODE(0x12340001, 0x0000, 0x1000, 0x8000, 0x00805F9B34FB)

#define BT_UUID_RGB_GREEN_VAL \
	BT_UUID_128_ENCODE(0x12340002, 0x0000, 0x1000, 0x8000, 0x00805F9B34FB)

#define BT_UUID_RGB_BLUE_VAL \
	BT_UUID_128_ENCODE(0x12340003, 0x0000, 0x1000, 0x8000, 0x00805F9B34FB)

#define BT_UUID_RGB_NOTIFY_VAL \
	BT_UUID_128_ENCODE(0x12340004, 0x0000, 0x1000, 0x8000, 0x00805F9B34FB)

static struct bt_uuid_128 uuid_svc    = BT_UUID_INIT_128(BT_UUID_RGB_SVC_VAL);
static struct bt_uuid_128 uuid_red    = BT_UUID_INIT_128(BT_UUID_RGB_RED_VAL);
static struct bt_uuid_128 uuid_green  = BT_UUID_INIT_128(BT_UUID_RGB_GREEN_VAL);
static struct bt_uuid_128 uuid_blue   = BT_UUID_INIT_128(BT_UUID_RGB_BLUE_VAL);
static struct bt_uuid_128 uuid_notify = BT_UUID_INIT_128(BT_UUID_RGB_NOTIFY_VAL);

/* ============ 连接管理 ============ */

static struct bt_conn *current_conn;

static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		printk("[BLE] 连接失败, err=%u\n", err);
		return;
	}
	current_conn = bt_conn_ref(conn);
	printk("[BLE] 已连接\n");
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	ARG_UNUSED(reason);
	if (current_conn) {
		bt_conn_unref(current_conn);
		current_conn = NULL;
	}
	printk("[BLE] 已断开, reason=0x%02x，重新开始广播...\n", reason);

	int ret = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, NULL, 0, NULL, 0);

	if (ret) {
		printk("[BLE] 广播启动失败: %d\n", ret);
	}
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected    = connected,
	.disconnected = disconnected,
};

/* ============ GATT Write 函数前置声明 ============ */

static ssize_t write_red(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			 const void *buf, uint16_t len,
			 uint16_t offset, uint8_t flags);

static ssize_t write_green(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			   const void *buf, uint16_t len,
			   uint16_t offset, uint8_t flags);

static ssize_t write_blue(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			  const void *buf, uint16_t len,
			  uint16_t offset, uint8_t flags);

/* ============ Notify CCC 前置声明 ============ */

static void notify_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value);

/* ============ GATT 服务定义 ============ */
/*
 * 属性索引（0-based）：
 *  0: Primary Service 声明
 *  1: Red 特征值声明 (CHRC)
 *  2: Red 特征值
 *  3: Green 特征值声明 (CHRC)
 *  4: Green 特征值
 *  5: Blue 特征值声明 (CHRC)
 *  6: Blue 特征值
 *  7: Notify 特征值声明 (CHRC)
 *  8: Notify 特征值      <-- bt_gatt_notify 指向此属性
 *  9: Notify CCC 描述符
 */
BT_GATT_SERVICE_DEFINE(rgb_svc,
	BT_GATT_PRIMARY_SERVICE(&uuid_svc),

	/* Red LED 写特征值 */
	BT_GATT_CHARACTERISTIC(&uuid_red.uuid,
		BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
		BT_GATT_PERM_WRITE,
		NULL, write_red, NULL),

	/* Green LED 写特征值 */
	BT_GATT_CHARACTERISTIC(&uuid_green.uuid,
		BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
		BT_GATT_PERM_WRITE,
		NULL, write_green, NULL),

	/* Blue LED 写特征值 */
	BT_GATT_CHARACTERISTIC(&uuid_blue.uuid,
		BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
		BT_GATT_PERM_WRITE,
		NULL, write_blue, NULL),

	/* RGB 状态 Notify 特征值 */
	BT_GATT_CHARACTERISTIC(&uuid_notify.uuid,
		BT_GATT_CHRC_NOTIFY,
		BT_GATT_PERM_NONE,
		NULL, NULL, NULL),
	BT_GATT_CCC(notify_ccc_changed,
		BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

/* ============ Notify 实现 ============ */

static bool notify_enabled;

static void notify_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	ARG_UNUSED(attr);
	notify_enabled = (value == BT_GATT_CCC_NOTIFY);
	printk("[BLE] Notify %s\n", notify_enabled ? "已启用" : "已禁用");
}

/* 发送当前 RGB 状态通知，attrs[8] 为 Notify 特征值属性 */
static void send_rgb_notify(void)
{
	if (!notify_enabled || !current_conn) {
		return;
	}

	int ret = bt_gatt_notify(current_conn, &rgb_svc.attrs[8],
				 led_state, sizeof(led_state));

	if (ret) {
		printk("[BLE] Notify 发送失败: %d\n", ret);
	} else {
		printk("[BLE] Notify -> R=%u G=%u B=%u\n",
		       led_state[0], led_state[1], led_state[2]);
	}
}

/* ============ GATT Write 回调实现 ============ */

static ssize_t write_led_common(struct bt_conn *conn,
				const struct bt_gatt_attr *attr,
				const void *buf, uint16_t len,
				uint16_t offset, uint8_t flags,
				uint8_t led_index,
				const struct gpio_dt_spec *led_spec,
				const char *led_name)
{
	ARG_UNUSED(conn);
	ARG_UNUSED(attr);
	ARG_UNUSED(offset);
	ARG_UNUSED(flags);

	if (len != 1) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}

	uint8_t val = ((const uint8_t *)buf)[0];

	if (val > 1) {
		return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
	}

	led_state[led_index] = val;
	gpio_pin_set_dt(led_spec, val);
	printk("[LED] %s -> %s\n", led_name, val ? "亮" : "灭");

	send_rgb_notify();
	return len;
}

static ssize_t write_red(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			 const void *buf, uint16_t len,
			 uint16_t offset, uint8_t flags)
{
	return write_led_common(conn, attr, buf, len, offset, flags,
				0, &led_red, "Red");
}

static ssize_t write_green(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			   const void *buf, uint16_t len,
			   uint16_t offset, uint8_t flags)
{
	return write_led_common(conn, attr, buf, len, offset, flags,
				1, &led_green, "Green");
}

static ssize_t write_blue(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			  const void *buf, uint16_t len,
			  uint16_t offset, uint8_t flags)
{
	return write_led_common(conn, attr, buf, len, offset, flags,
				2, &led_blue, "Blue");
}

/* ============ BLE 广播数据 ============ */

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_RGB_SVC_VAL),
};

static const struct bt_data sd[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE,
		CONFIG_BT_DEVICE_NAME,
		sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

/* ============ BLE 初始化回调 ============ */

static void bt_ready(int err)
{
	if (err) {
		printk("[BLE] 初始化失败: %d\n", err);
		return;
	}

	printk("[BLE] 初始化完成\n");

	err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad),
			      sd, ARRAY_SIZE(sd));
	if (err) {
		printk("[BLE] 广播启动失败: %d\n", err);
		return;
	}

	printk("[BLE] 广播已开始，设备名: \"%s\"\n", CONFIG_BT_DEVICE_NAME);
	printk("[BLE] 等待手机连接...\n");
	printk("[BLE] 写 Red   UUID 末段 0x0001: 0x01=亮, 0x00=灭\n");
	printk("[BLE] 写 Green UUID 末段 0x0002: 0x01=亮, 0x00=灭\n");
	printk("[BLE] 写 Blue  UUID 末段 0x0003: 0x01=亮, 0x00=灭\n");
	printk("[BLE] 订阅 Notify UUID 末段 0x0004: 接收 [R,G,B] 状态\n");
}

/* ============ 主函数 ============ */

int main(void)
{
	int ret;

	printk("=== BLE RGB LED Demo ===\n");
	printk("开发板: %s\n", CONFIG_BOARD_TARGET);

	/* 配置 LED GPIO */
	if (!gpio_is_ready_dt(&led_red) ||
	    !gpio_is_ready_dt(&led_green) ||
	    !gpio_is_ready_dt(&led_blue)) {
		printk("错误: LED GPIO 设备未就绪\n");
		return -ENODEV;
	}

	ret  = gpio_pin_configure_dt(&led_red,   GPIO_OUTPUT_INACTIVE);
	ret |= gpio_pin_configure_dt(&led_green, GPIO_OUTPUT_INACTIVE);
	ret |= gpio_pin_configure_dt(&led_blue,  GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		printk("错误: LED GPIO 配置失败\n");
		return ret;
	}

	printk("[LED] GPIO 配置完成，初始状态：全灭\n");

	/* 启动 BLE */
	ret = bt_enable(bt_ready);
	if (ret) {
		printk("[BLE] bt_enable 失败: %d\n", ret);
		return ret;
	}

	/* 主循环：每 10 秒打印一次心跳和当前 LED 状态 */
	while (1) {
		k_sleep(K_SECONDS(10));
		printk("[心跳] 运行时间: %lld ms | R=%u G=%u B=%u\n",
		       k_uptime_get(),
		       led_state[0], led_state[1], led_state[2]);
	}

	return 0;
}
