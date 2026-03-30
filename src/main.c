/*
 * Copyright (c) 2024 Your Name
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Zephyr 多任务示例：RGB LED 换色 + 串口定时输出
 *
 * 演示 Zephyr 内核的两种定时机制：
 *   1. k_thread — 独立线程（RGB LED 换色，500ms 周期）
 *   2. k_timer  — 定时器回调 + 信号量（串口输出，1000ms 周期）
 *
 * 硬件：Seeed XIAO BLE (nRF52840)
 *   红 LED -> P0.26 (led0)
 *   绿 LED -> P0.30 (led1)
 *   蓝 LED -> P0.06 (led2)
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/gpio.h>

/* ============ LED 定义 ============ */

#define LED0_NODE DT_ALIAS(led0)  /* 红 */
#define LED1_NODE DT_ALIAS(led1)  /* 绿 */
#define LED2_NODE DT_ALIAS(led2)  /* 蓝 */

static const struct gpio_dt_spec led_red   = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct gpio_dt_spec led_green = GPIO_DT_SPEC_GET(LED1_NODE, gpios);
static const struct gpio_dt_spec led_blue  = GPIO_DT_SPEC_GET(LED2_NODE, gpios);

struct rgb_color {
	const char *name;
	bool red;
	bool green;
	bool blue;
};

static const struct rgb_color colors[] = {
	{ "红 (Red)",     true,  false, false },
	{ "绿 (Green)",   false, true,  false },
	{ "蓝 (Blue)",    false, false, true  },
	{ "黄 (Yellow)",  true,  true,  false },
	{ "青 (Cyan)",    false, true,  true  },
	{ "品红 (Magenta)", true, false, true  },
	{ "白 (White)",   true,  true,  true  },
	{ "灭 (Off)",     false, false, false },
};

#define NUM_COLORS ARRAY_SIZE(colors)
#define LED_STEP_MS 500

/* ============ 串口定时器 ============ */

static struct k_timer status_timer;
static struct k_sem status_sem;

/* 定时器回调：在中断上下文中执行，仅释放信号量 */
static void status_timer_handler(struct k_timer *timer_id)
{
	ARG_UNUSED(timer_id);
	k_sem_give(&status_sem);
}

/* ============ LED 线程 ============ */

#define LED_THREAD_STACK_SIZE 512
#define LED_THREAD_PRIORITY   5

static K_THREAD_STACK_DEFINE(led_stack, LED_THREAD_STACK_SIZE);
static struct k_thread led_thread_data;

static int configure_led(const struct gpio_dt_spec *led)
{
	if (!gpio_is_ready_dt(led)) {
		printk("错误: LED GPIO 设备未就绪\n");
		return -ENODEV;
	}
	return gpio_pin_configure_dt(led, GPIO_OUTPUT_INACTIVE);
}

static void set_rgb(const struct rgb_color *c)
{
	gpio_pin_set_dt(&led_red,   c->red);
	gpio_pin_set_dt(&led_green, c->green);
	gpio_pin_set_dt(&led_blue,  c->blue);
}

static void led_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	printk("[LED 线程] 已启动，每 %d ms 切换颜色\n", LED_STEP_MS);

	while (1) {
		for (int i = 0; i < NUM_COLORS; i++) {
			set_rgb(&colors[i]);
			printk("[LED] 颜色: %s\n", colors[i].name);
			k_msleep(LED_STEP_MS);
		}
	}
}

/* ============ 主线程 ============ */

int main(void)
{
	int ret;

	printk("=== Zephyr 多任务示例 ===\n");
	printk("开发板: %s\n", CONFIG_BOARD_TARGET);
	printk("共有 %d 种颜色，LED 每 %d ms 切换，串口每 1000 ms 输出状态\n\n",
	       NUM_COLORS, LED_STEP_MS);

	/* 配置 LED GPIO */
	ret = configure_led(&led_red);
	if (ret < 0) {
		return ret;
	}
	ret = configure_led(&led_green);
	if (ret < 0) {
		return ret;
	}
	ret = configure_led(&led_blue);
	if (ret < 0) {
		return ret;
	}

	/* 初始化信号量（初始值为 0） */
	k_sem_init(&status_sem, 0, 1);

	/* 初始化定时器：1000ms 周期，无一次性延迟 */
	k_timer_init(&status_timer, status_timer_handler, NULL);
	k_timer_start(&status_timer, K_MSEC(1000), K_MSEC(1000));

	/* 启动 LED 线程 */
	k_thread_create(&led_thread_data, led_stack,
			LED_THREAD_STACK_SIZE,
			led_thread, NULL, NULL, NULL,
			LED_THREAD_PRIORITY, 0, K_NO_WAIT);

	/* 主线程：等待信号量，定时输出串口信息 */
	uint32_t counter = 0;

	while (1) {
		/* 阻塞等待信号量（由定时器释放） */
		k_sem_take(&status_sem, K_FOREVER);
		counter++;

		printk("[串口] 第 %u 次心跳 | 运行时间: %d ms | 开发板: %s\n",
		       counter, (int)k_uptime_get(), CONFIG_BOARD_TARGET);
	}

	return 0;
}
