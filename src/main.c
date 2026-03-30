/*
 * Copyright (c) 2024 Your Name
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * RGB LED color cycling demo for Seeed XIAO BLE (nRF52840).
 *
 * Board LED definitions (GPIO_ACTIVE_LOW):
 *   Red   -> led0 -> P0.26
 *   Green -> led1 -> P0.30
 *   Blue  -> led2 -> P0.06
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/gpio.h>

/* LED device tree aliases from xiao_ble_common.dtsi */
#define LED0_NODE DT_ALIAS(led0)  /* Red   */
#define LED1_NODE DT_ALIAS(led1)  /* Green */
#define LED2_NODE DT_ALIAS(led2)  /* Blue  */

static const struct gpio_dt_spec led_red   = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct gpio_dt_spec led_green = GPIO_DT_SPEC_GET(LED1_NODE, gpios);
static const struct gpio_dt_spec led_blue  = GPIO_DT_SPEC_GET(LED2_NODE, gpios);

/* Color states: each color is a combination of R, G, B */
struct rgb_color {
	const char *name;
	bool red;
	bool green;
	bool blue;
};

static const struct rgb_color colors[] = {
	{ "Red",        true,  false, false },
	{ "Green",      false, true,  false },
	{ "Blue",       false, false, true  },
	{ "Yellow",     true,  true,  false },
	{ "Cyan",       false, true,  true  },
	{ "Magenta",    true,  false, true  },
	{ "White",      true,  true,  true  },
	{ "Off",        false, false, false },
};

#define NUM_COLORS ARRAY_SIZE(colors)
#define STEP_MS    500

static int configure_led(const struct gpio_dt_spec *led)
{
	int ret;

	if (!gpio_is_ready_dt(led)) {
		printk("LED GPIO device not ready\n");
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(led, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		printk("Failed to configure LED pin (%d)\n", ret);
		return ret;
	}

	return 0;
}

static void set_rgb(const struct rgb_color *c)
{
	/* LEDs are ACTIVE_LOW, so GPIO_ACTIVE = LED on */
	gpio_pin_set_dt(&led_red,   c->red);
	gpio_pin_set_dt(&led_green, c->green);
	gpio_pin_set_dt(&led_blue,  c->blue);
}

int main(void)
{
	int ret;

	printk("RGB LED Color Cycling on %s\n", CONFIG_BOARD_TARGET);

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

	printk("Cycling through %d colors, %d ms per step\n", NUM_COLORS, STEP_MS);

	while (1) {
		for (int i = 0; i < NUM_COLORS; i++) {
			set_rgb(&colors[i]);
			printk("Color: %s\n", colors[i].name);
			k_msleep(STEP_MS);
		}
	}

	return 0;
}
