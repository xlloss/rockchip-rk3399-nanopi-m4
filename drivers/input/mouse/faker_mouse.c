/*
 *  fake mouse driver for Linux
 *  slash.linux.c@gmail.com
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation
 */

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/iio/iio.h>
#include <linux/iio/machine.h>
#include <linux/iio/driver.h>
#include <linux/iio/consumer.h>
#include <linux/input-polldev.h>
#include <linux/delay.h>

#define FAKE_MOUSE

struct faker_mouse_data {
	struct input_polled_dev *poll_dev;
	struct iio_channel *chan[4];
	struct device *faker_mouse_dev;
};

int raw_lastx, raw_lasty;

static void faker_mouse_poll(struct input_polled_dev *dev)
{
	int i, raw, ret;
	int aix_y, aix_x, btn;
	int dx, dy;
	struct faker_mouse_data *_faker_mouse_data;

	_faker_mouse_data = dev->private;

	for (i = 0; i < 3; i++) {
		ret = iio_read_channel_raw(_faker_mouse_data->chan[i], &raw);
		if (ret < 0)
			pr_err("read channel() error: %d\n", ret);

		switch (i) {
			case 0:
				aix_y = (raw - 8000) >> 10;
			break;

			case 1:
				aix_x = (raw - 8000) >> 10;
			break;

			case 2:
				btn = (raw - 3000) >> 5;
			break;

			default:
				aix_y = aix_x = btn = 0;
				pr_err("read fail AIN port\n");
			break;
		};
	}

	dx = raw_lastx - aix_x;
	if (dx < 0)
		dx = 0 - dx;

	dy = raw_lasty - aix_y;
	if (dy < 0)
		dy = 0 - dy;

	if (dx > 5) {
		if (aix_x < 0) {
			#ifdef FAKE_MOUSE
			input_report_rel(dev->input, REL_X, 100);
			#else
			input_report_abs(dev->input, ABS_X, 1);
			#endif
		} else {
				#ifdef FAKE_MOUSE
				input_report_rel(dev->input, REL_X, -100);
				#else
				input_report_abs(dev->input, ABS_X, -1);
				#endif
		}
	}

	if (dy > 5) {
		if (aix_y > 0) {
			#ifdef FAKE_MOUSE
			input_report_rel(dev->input, REL_Y, 100);
			#else
			input_report_abs(dev->input, ABS_Y, 1);
			#endif
		} else {
			#ifdef FAKE_MOUSE
			input_report_rel(dev->input, REL_Y, -100);
			#else
			input_report_abs(dev->input, ABS_Y, -1);
			#endif
		}
	}

	input_sync(dev->input);

#ifdef DEBUG
		pr_info("AIX-Y %d \n", aix_y);
		pr_info("AIX-X %d \n", aix_x);
		pr_info("BTN   %d \n", btn);
#endif


}

static int faker_mouse_open(struct input_dev *dev)
{
	return 0;
}

static void faker_mouse_close(struct input_dev *dev)
{
}

int faker_mouse_parse_dt(struct faker_mouse_data *_faker_mouse_data)
{
	struct iio_channel *chan;
	enum iio_chan_type type;
	struct device_node *np = _faker_mouse_data->faker_mouse_dev->of_node;
	int ret, i;
	char chan_name[10];

	if (!np) {
		pr_err("np is NULL\n");
		return -EINVAL;
	}
	for (i = 0; i < 4; i++) {
		sprintf(chan_name, "AIN%d", i); 
		chan = iio_channel_get(_faker_mouse_data->faker_mouse_dev, chan_name);
		if (IS_ERR(chan)) {
			pr_err("iio_channel_get fail chan_name %s\n", chan_name);
			return -EINVAL;
		}
		_faker_mouse_data->chan[i] = chan;

		ret = iio_get_channel_type(chan, &type);
		if (ret < 0) {
			pr_err("iio_get_channel_type fail chan_name %s\n", chan_name);
			return -EINVAL;
		}
	}

	return 0;
}

static int faker_mouse_probe(struct platform_device *pdev)
{
	int err;
	struct input_dev *_input_dev;
	struct input_polled_dev *poll_dev;
	struct faker_mouse_data *_faker_mouse_data;

	pr_info("%s\n", __func__);

	_faker_mouse_data = kmalloc(sizeof(struct faker_mouse_data), GFP_KERNEL);
	_faker_mouse_data->faker_mouse_dev = &pdev->dev;
	err = faker_mouse_parse_dt(_faker_mouse_data);
	if (err) {
		pr_err("faker_mouse_parse_dt fail\n");
		return -EINVAL;
	}

	poll_dev = devm_input_allocate_polled_device(&pdev->dev);
	if (!poll_dev) {
		dev_err(&pdev->dev, "failed to allocate input device\n");
		return -ENOMEM;
	}

	_faker_mouse_data->poll_dev = poll_dev;
	poll_dev->poll = faker_mouse_poll;
	poll_dev->private = _faker_mouse_data;
	poll_dev->poll_interval = 5;
	
	_input_dev = poll_dev->input;

#ifdef FAKE_MOUSE
	_input_dev->name = pdev->name;
	_input_dev->id.bustype = BUS_I2C;
	_input_dev->phys = "faker_mouse/input0";
	_input_dev->id.vendor = 0x0001;
	_input_dev->id.product = 0x0002;
	_input_dev->id.version = 0x0100;
	_input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_REL);
	_input_dev->relbit[0] = BIT_MASK(REL_X) | BIT_MASK(REL_Y);
	_input_dev->keybit[BIT_WORD(BTN_LEFT)] = BIT_MASK(BTN_LEFT) |
		BIT_MASK(BTN_MIDDLE) | BIT_MASK(BTN_RIGHT);
#else
	_input_dev->phys = "faker_joy/input0";
	_input_dev->id.vendor = 0x0001;
	_input_dev->id.product = 0x0003;
	_input_dev->id.version = 0x0100;

	_input_dev->evbit[0] = BIT_MASK(EV_ABS);
	_input_dev->absbit[0] = BIT_MASK(ABS_X) | BIT_MASK(ABS_Y);

	input_set_abs_params(_input_dev, ABS_X, -1, 1, 0, 0);
	input_set_abs_params(_input_dev, ABS_Y, -1, 1, 0, 0);
#endif

	_input_dev->open = faker_mouse_open;
	_input_dev->close = faker_mouse_close;
	_input_dev->dev.parent = &pdev->dev;

	err = input_register_polled_device(poll_dev);
	if (err) {
		input_free_device(_input_dev);
		return err;
	}

	raw_lastx = raw_lasty = 0;
	platform_set_drvdata(pdev, _faker_mouse_data);

	return 0;
}

static int faker_mouse_remove(struct platform_device *pdev)
{
	struct faker_mouse_data *_faker_mouse_data;
	struct input_polled_dev *poll_dev; 

	_faker_mouse_data = platform_get_drvdata(pdev);
	poll_dev = _faker_mouse_data->poll_dev;

	input_unregister_polled_device(poll_dev);
	input_free_polled_device(poll_dev);
	kfree(_faker_mouse_data);
	return 0;
}

static const struct of_device_id faker_mouse_dt_ids[] = {
	{ .compatible = "test,fakermouse", },
	{ }
};
MODULE_DEVICE_TABLE(of, faker_mouse_dt_ids);

static struct platform_driver faker_mouse_driver = {
	.driver = {
		.name = "fakermouse_driver",
		.of_match_table = of_match_ptr(faker_mouse_dt_ids),
	},
	.probe	= faker_mouse_probe,
	.remove	= faker_mouse_remove,
};
module_platform_driver(faker_mouse_driver);

MODULE_AUTHOR("slash.huang <slash.linux.c@gmail.com>");
MODULE_DESCRIPTION("faker mouse driver");
MODULE_LICENSE("GPL");
