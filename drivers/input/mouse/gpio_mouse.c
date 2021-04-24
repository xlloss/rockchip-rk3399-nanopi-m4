/*
 * Driver for simulating a mouse on GPIO lines.
 *
 * Copyright (C) 2007 Atmel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif
#include <linux/input-polldev.h>
#include <linux/gpio.h>
#include <linux/gpio_mouse.h>


#include <linux/slab.h>
#define GPIO_MOUSE_PIN_MAX_LIMIT 5
/*
 * Timer function which is run every scan_ms ms when the device is opened.
 * The dev input variable is set to the the input_dev pointer.
 */
static void gpio_mouse_scan(struct input_polled_dev *dev)
{
	struct gpio_mouse_platform_data *gpio = dev->private;
	struct input_dev *input = dev->input;
	int x, y;

	if (gpio->bleft >= 0)
		input_report_key(input, BTN_LEFT,
				gpio_get_value(gpio->bleft) ^ gpio->polarity);
	if (gpio->bmiddle >= 0)
		input_report_key(input, BTN_MIDDLE,
				gpio_get_value(gpio->bmiddle) ^ gpio->polarity);
	if (gpio->bright >= 0)
		input_report_key(input, BTN_RIGHT,
				gpio_get_value(gpio->bright) ^ gpio->polarity);

	x = (gpio_get_value(gpio->right) ^ gpio->polarity)
		- (gpio_get_value(gpio->left) ^ gpio->polarity);
	y = (gpio_get_value(gpio->down) ^ gpio->polarity)
		- (gpio_get_value(gpio->up) ^ gpio->polarity);

	input_report_rel(input, REL_X, x);
	input_report_rel(input, REL_Y, y);
	input_sync(input);
}

static int gpio_mouse_probe(struct platform_device *pdev)
{
	struct gpio_mouse_platform_data *pdata = dev_get_platdata(&pdev->dev);
	struct input_polled_dev *input_poll;
	struct input_dev *input;
	int pin, i, gpio_value[GPIO_MOUSE_PIN_MAX_LIMIT], ret;
	int error, scan_ms;
	const char *gpio_name[GPIO_MOUSE_PIN_MAX_LIMIT] = {
		"btn-up", "btn-down", "btn-left", "btn-right", "btn-btnleft"};

	dev_info(&pdev->dev, "%s\n", __func__);

#ifdef CONFIG_OF
	pdata = kmalloc(sizeof(struct gpio_mouse_platform_data), GFP_KERNEL);
	if (!pdata) {
		dev_err(&pdev->dev, "kmalloc fail\n");
		error = -ENXIO;
		goto out;
	}

	memset(pdata->pins, -1, sizeof(pdata->pins));
	for (i = 0; i < GPIO_MOUSE_PIN_MAX_LIMIT; i++) {
		ret = of_property_read_u32(pdev->dev.of_node,
							gpio_name[i],
							&gpio_value[i]);
		if (ret) {
			dev_err(&pdev->dev, "get gpio pin fial from dts\n");
			error = -ENXIO;
			goto out;
		}
		pdata->pins[i] = gpio_value[i];
		dev_info(&pdev->dev, "pdata->pins[%d] %d\n", i, pdata->pins[i]);
	}

	ret = of_property_read_u32(pdev->dev.of_node,
						"scan_ms",
						&scan_ms);
	if (ret) {
		dev_info(&pdev->dev, "scan_ms read fial from dts\n");
		scan_ms = 10;
	}
	pdata->scan_ms = scan_ms;
	pdata->polarity = 1;
#endif

	if (!pdata) {
		dev_err(&pdev->dev, "no platform data\n");
		error = -ENXIO;
		goto out;
	}

	if (pdata->scan_ms < 0) {
		dev_err(&pdev->dev, "invalid scan time\n");
		error = -EINVAL;
		goto out;
	}

	for (i = 0; i < GPIO_MOUSE_PIN_MAX_LIMIT; i++) {
		pin = pdata->pins[i];

		if (pin < 0) {

			if (i <= GPIO_MOUSE_PIN_RIGHT) {
				/* Mouse direction is required. */
				dev_err(&pdev->dev,
					"missing GPIO for directions\n");
				error = -EINVAL;
				goto out_free_gpios;
			}

			if (i == GPIO_MOUSE_PIN_BLEFT)
				dev_dbg(&pdev->dev, "no left button defined\n");

		} else {
			error = gpio_request(pin, "gpio_mouse");
			if (error) {
				dev_err(&pdev->dev, "fail %d pin (%d idx)\n",
					pin, i);
				goto out_free_gpios;
			}

			gpio_direction_input(pin);
		}
	}

	input_poll = input_allocate_polled_device();
	if (!input_poll) {
		dev_err(&pdev->dev, "not enough memory for input device\n");
		error = -ENOMEM;
		goto out_free_gpios;
	}

	platform_set_drvdata(pdev, input_poll);

	/* set input-polldev handlers */
	input_poll->private = pdata;
	input_poll->poll = gpio_mouse_scan;
	input_poll->poll_interval = pdata->scan_ms;

	input = input_poll->input;
	input->name = pdev->name;
	input->id.bustype = BUS_HOST;
	input->dev.parent = &pdev->dev;

	input_set_capability(input, EV_REL, REL_X);
	input_set_capability(input, EV_REL, REL_Y);
	if (pdata->bleft >= 0)
		input_set_capability(input, EV_KEY, BTN_LEFT);
	if (pdata->bmiddle >= 0)
		input_set_capability(input, EV_KEY, BTN_MIDDLE);
	if (pdata->bright >= 0)
		input_set_capability(input, EV_KEY, BTN_RIGHT);

	error = input_register_polled_device(input_poll);
	if (error) {
		dev_err(&pdev->dev, "could not register input device\n");
		goto out_free_polldev;
	}

	dev_dbg(&pdev->dev, "%d ms scan time, buttons: %s%s%s\n",
			pdata->scan_ms,
			pdata->bleft < 0 ? "" : "left ",
			pdata->bmiddle < 0 ? "" : "middle ",
			pdata->bright < 0 ? "" : "right");

	return 0;

 out_free_polldev:
	input_free_polled_device(input_poll);

 out_free_gpios:
	while (--i >= 0) {
		pin = pdata->pins[i];
		if (pin)
			gpio_free(pin);
	}
 out:
	return error;
}

static int gpio_mouse_remove(struct platform_device *pdev)
{
	struct input_polled_dev *input = platform_get_drvdata(pdev);
	struct gpio_mouse_platform_data *pdata = input->private;
	int pin, i;

	input_unregister_polled_device(input);
	input_free_polled_device(input);

	for (i = 0; i < GPIO_MOUSE_PIN_MAX_LIMIT; i++) {
		pin = pdata->pins[i];
		if (pin >= 0)
			gpio_free(pin);
	}

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id gpio_mouse_dt_ids[] = {
	{ .compatible = "gpio,mouse" },
	{}
};
MODULE_DEVICE_TABLE(of, gpio_mouse_dt_ids);
#endif

static struct platform_driver gpio_mouse_device_driver = {
	.probe		= gpio_mouse_probe,
	.remove		= gpio_mouse_remove,
	.driver		= {
		.name	= "gpio_mouse",
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(gpio_mouse_dt_ids),
#endif
	}
};
module_platform_driver(gpio_mouse_device_driver);

MODULE_AUTHOR("Hans-Christian Egtvedt <egtvedt@samfundet.no>");
MODULE_DESCRIPTION("GPIO mouse driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:gpio_mouse"); /* work with hotplug and coldplug */
