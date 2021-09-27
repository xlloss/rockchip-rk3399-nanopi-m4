// SPDX-License-Identifier: GPL-2.0-only
/*
 * mcp4725 Backlight Driver
 * Slash Huang <slash.linux.c@gmail.com>
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/of.h>

#define DEFAULT_BL_NAME "mcp4725-backlight"
#define MAX_BRIGHTNESS 10
#define DEFAULT_BL_VALE 10
#define MCP4725_BL_ON 1
#define MCP4725_BL_OFF 0

struct mcp4725 {
	u8 initial_brightness;
	int current_brightness;
	unsigned int dac_value;
	struct i2c_client *client;
	struct backlight_device *bl;
	struct device *dev;
	struct gpio_desc *enable_gpio;
};

struct mcp4725;

/*
 * ADC_4096 / brightness_10
 */

static int mcp4725_set_value(struct mcp4725 *mcp4725_dev, int val)
{
	struct i2c_client *cl = mcp4725_dev->client;
	u8 outbuf[2];
	int ret, dac_value;
	int backlight_table[MAX_BRIGHTNESS] =
		{0x0FFF, 0x0AFF, 0x09FF, 0x08FF, 0x0855,
		 0x0800, 0x07EE, 0x0780, 0x0750, 0x0700};

	/*
	 * INPUT DATA CODING             Output
	 * ----------------------------------------
	 * 111111111111 (FFFh)         V DD - 1 LSB
	 * 111111111110 (FFEh)         V DD - 2 LSB
	 * 000000000010 (002h)         2 LSB
	 * 000000000001 (001h)         1 LSB
	 * 000000000000 (000h)         0
	 */
	if (val == MAX_BRIGHTNESS)
		dac_value = (1 << 12) - 1;
	else if (val == 0)
		dac_value = 0;
	else
		dac_value = backlight_table[MAX_BRIGHTNESS - val];

	outbuf[0] = (dac_value >> 8) & 0x0f;
	outbuf[1] = dac_value & 0xff;

	ret = i2c_master_send(cl, outbuf, 2);
	if (ret < 0)
		return ret;
	else if (ret != 2)
		return -EIO;
	else
		return 0;
}

static int mcp4725_get_value(struct mcp4725 *mcp4725_dev)
{
	struct i2c_client *client = mcp4725_dev->client;
	u8 inbuf[4];
	unsigned int dac_value;
	int err;

	/* read current DAC value and settings */
	err = i2c_master_recv(client, inbuf, 3);
	if (err < 0) {
		dev_err(&client->dev, "failed to read DAC value");
		return -1;
	}

	dac_value = (inbuf[1] << 8) | (inbuf[2] >> 4);
	mcp4725_dev->dac_value = dac_value;

	return 0;
}

static int mcp4725_bl_on_off(struct mcp4725 *mcp4725_dev, unsigned char onoff)
{
	int ret;

	if (onoff == MCP4725_BL_OFF) {
		ret = mcp4725_set_value(mcp4725_dev, 0);
	} else {
		ret = mcp4725_get_value(mcp4725_dev);
		if (mcp4725_dev->dac_value > 0)
			return 0;

		ret = mcp4725_set_value(mcp4725_dev, MAX_BRIGHTNESS);
		mcp4725_dev->current_brightness = MAX_BRIGHTNESS;
	}

	return ret;
}

static int mcp4725_bl_update_status(struct backlight_device *bl)
{
	struct mcp4725 *mcp4725_dev = bl_get_data(bl);
	int brightness = bl->props.brightness;
	int ret;

	if (bl->props.state & (BL_CORE_SUSPENDED | BL_CORE_FBBLANK))
		brightness = 0;

	if (bl->props.power == MCP4725_BL_OFF) {
		ret = mcp4725_bl_on_off(mcp4725_dev, MCP4725_BL_OFF);
		return ret;
	} else
		ret = mcp4725_bl_on_off(mcp4725_dev, MCP4725_BL_ON);

	mcp4725_dev->current_brightness = brightness;
	mcp4725_set_value(mcp4725_dev, brightness);

	return ret;
}

static int mcp4725_bl_get_brightness(struct backlight_device *dev)
{
	struct mcp4725 *mcp4725_dev = bl_get_data(dev);

	return mcp4725_dev->current_brightness;
}

static const struct backlight_ops mcp4725_bl_ops = {
	.options = BL_CORE_SUSPENDRESUME,
	.update_status = mcp4725_bl_update_status,
	.get_brightness = mcp4725_bl_get_brightness,
};

static int mcp4725_backlight_register(struct mcp4725 *mcp4725_dev)
{
	struct backlight_device *bl;
	struct backlight_properties props;
	const char *name = DEFAULT_BL_NAME;

	memset(&props, 0, sizeof(props));
	props.type = BACKLIGHT_PLATFORM;
	props.max_brightness = MAX_BRIGHTNESS;

	if (mcp4725_dev->initial_brightness > props.max_brightness)
		mcp4725_dev->initial_brightness = props.max_brightness;

	props.brightness = mcp4725_dev->initial_brightness;

	bl = devm_backlight_device_register(mcp4725_dev->dev, name,
		mcp4725_dev->dev, mcp4725_dev, &mcp4725_bl_ops, &props);
	if (IS_ERR(bl))
		return PTR_ERR(bl);

	mcp4725_dev->bl = bl;

	return 0;
}

static int mcp4725_probe(struct i2c_client *cl, const struct i2c_device_id *id)
{
	struct mcp4725 *mcp4725_dev;
	int ret;

	if (!i2c_check_functionality(cl->adapter,
		I2C_FUNC_SMBUS_BYTE_DATA | I2C_FUNC_SMBUS_WORD_DATA))
		return -EIO;

	mcp4725_dev = devm_kzalloc(&cl->dev, sizeof(*mcp4725_dev), GFP_KERNEL);
	if (!mcp4725_dev)
		return -ENOMEM;

	mcp4725_dev->client = cl;
	mcp4725_dev->dev = &cl->dev;

	i2c_set_clientdata(cl, mcp4725_dev);

	mcp4725_dev->initial_brightness = DEFAULT_BL_VALE;
	ret = mcp4725_backlight_register(mcp4725_dev);
	if (ret) {
		dev_err(mcp4725_dev->dev,
			"failed to register backlight. err: %d\n", ret);
		return ret;
	}

	mcp4725_dev->bl->props.state &= ~(BL_CORE_SUSPENDED | BL_CORE_FBBLANK);
	mcp4725_dev->bl->props.power = MCP4725_BL_ON;

	backlight_update_status(mcp4725_dev->bl);
	return 0;
}

static void mcp4725_shutdown(struct i2c_client *cl)
{
	struct mcp4725 *mcp4725_dev = i2c_get_clientdata(cl);

	mcp4725_dev->bl->props.brightness = 0;
	backlight_update_status(mcp4725_dev->bl);
	mcp4725_bl_on_off(mcp4725_dev, MCP4725_BL_OFF);
}

static int mcp4725_remove(struct i2c_client *cl)
{
	struct mcp4725 *mcp4725_dev = i2c_get_clientdata(cl);

	mcp4725_dev->bl->props.brightness = 0;
	backlight_update_status(mcp4725_dev->bl);
	mcp4725_bl_on_off(mcp4725_dev, MCP4725_BL_OFF);
	return 0;
}

static const struct of_device_id mcp4725_dt_ids[] = {
	{ .compatible = "mcp4725,bl", },
	{ }
};
MODULE_DEVICE_TABLE(of, mcp4725_dt_ids);

static const struct i2c_device_id mcp4725_ids[] = {
	{"mcp4725", 0},
	{ }
};
MODULE_DEVICE_TABLE(i2c, mcp4725_ids);

static struct i2c_driver mcp4725_driver = {
	.driver = {
		   .name = "mcp4725-bl",
		   .of_match_table = of_match_ptr(mcp4725_dt_ids),
	},
	.probe = mcp4725_probe,
	.remove = mcp4725_remove,
	.shutdown = mcp4725_shutdown,
	.id_table = mcp4725_ids,
};

module_i2c_driver(mcp4725_driver);

MODULE_DESCRIPTION("mcp4725 Backlight driver");
MODULE_AUTHOR("Slash Huang <slash.linux.c@gmail.com>");
MODULE_LICENSE("GPL");
