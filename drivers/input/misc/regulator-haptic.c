/*
 * Regulator haptic driver
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 * Author: Sanghyeon Lee <sirano06.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <config/regulator//s2mps14.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/of_regulator.h>
#include <linux/of.h>

#define MAX_MAGNITUDE		0xffff
#define DEFAULT_MIN_MICROVOLT	1100000
#define DEFAULT_MAX_MICROVOLT	2700000

#define ERROR_NAME "error"

/*
 * struct regulator
 *
 * One for each consumer device.
 */
struct regulator_haptic {
	struct device *dev;
	struct input_dev *input_dev;
	struct work_struct work;
	bool enabled;
	struct regulator *regulator;
	struct mutex mutex;
	int max_mV;
	int min_mV;
	int intensity; /* mV */
	int level;
};

extern unsigned int system_rev;

enum {
	B2_BT_EMUL = 0x0,
	B2_BT_REV00 = 0x1,
	B2_NEO_REV00 = 0x8,
	B2_NEO_REV01 = 0x9,
};

static char * get_regulator_name(void) {
	switch (system_rev) {
		case B2_BT_EMUL: return "regulator-haptic";
		case B2_BT_REV00:
		case B2_NEO_REV00:
		case B2_NEO_REV01:
		default : return "vdd_ldo1";
	}
	return "error";
}

static void set_haptic_data(struct regulator_haptic* pHapticData)
{
	if(pHapticData)
	{
		pHapticData->min_mV = DEFAULT_MIN_MICROVOLT/1000;
		pHapticData->max_mV = DEFAULT_MAX_MICROVOLT/1000;
	}else
		printk("regulator-haptic : Not defien regulator_haptic structure\n");
}

static void regulator_haptic_enable(struct regulator_haptic *haptic, bool enable)
{
	int ret;
	mutex_lock(&haptic->mutex);
	if (enable && !haptic->enabled)
	{
		haptic->enabled = true;
		ret = regulator_enable(haptic->regulator);
		if (ret)
			printk("regulator-haptic: failed to enable regulator\n");
	} else if (!enable && haptic->enabled)
	{
		haptic->enabled = false;
		ret = regulator_disable(haptic->regulator);
		if (ret)
			printk("regulator-haptic: failed to disable regulator\n");
	}

	if (haptic->enabled)
	{
		regulator_set_voltage(haptic->regulator,
			haptic->intensity * 1000, haptic->max_mV * 1000);
	}

	mutex_unlock(&haptic->mutex);
}

static void regulator_haptic_work(struct work_struct *work)
{
	struct regulator_haptic *haptic = container_of(work,
						       struct regulator_haptic,
						       work);
	if (haptic->level)
		regulator_haptic_enable(haptic, true);
	else
		regulator_haptic_enable(haptic, false);
}

static int regulator_haptic_play(struct input_dev *input, void *data,
				struct ff_effect *effect)
{
	struct regulator_haptic *haptic = input_get_drvdata(input);

	haptic->level = effect->u.rumble.strong_magnitude;
	if (!haptic->level)
		haptic->level = effect->u.rumble.weak_magnitude;

	haptic->intensity =
		(haptic->max_mV - haptic->min_mV) * haptic->level /
		MAX_MAGNITUDE;
	haptic->intensity = haptic->intensity + haptic->min_mV;

	if (haptic->intensity > haptic->max_mV)
		haptic->intensity = haptic->max_mV;
	if (haptic->intensity < haptic->min_mV)
		haptic->intensity = haptic->min_mV;

	schedule_work(&haptic->work);

	return 0;
}

static void regulator_haptic_close(struct input_dev *input)
{
	struct regulator_haptic *haptic = input_get_drvdata(input);

	cancel_work_sync(&haptic->work);
	regulator_haptic_enable(haptic, false);
}

static int regulator_haptic_probe(struct platform_device *pdev)
{
	struct regulator_haptic *haptic;
	struct input_dev *input_dev;
	char *select_regulator_name;
	int error;

	haptic = kzalloc(sizeof(*haptic), GFP_KERNEL);
	if (!haptic) {
		printk("regulator-haptic : unable to allocate memory for haptic\n");
		return -ENOMEM;
	}

	input_dev = input_allocate_device();
	if (!input_dev) {
		printk("regulator-haptic : unalbe to allocate memory\n");
		error =  -ENOMEM;
		goto err_kfree_mem;
	}

	INIT_WORK(&haptic->work, regulator_haptic_work);
	mutex_init(&haptic->mutex);
	haptic->input_dev = input_dev;
	haptic->dev = &pdev->dev;
	select_regulator_name = get_regulator_name();


	if(!strcmp(select_regulator_name, ERROR_NAME)){
		pr_info("%s : error regulator_name\n", __func__);
		error = -ENOMEM;
		goto err_kfree_mem;
	}

	haptic->regulator = regulator_get(&pdev->dev, select_regulator_name);
	if (IS_ERR(haptic->regulator)) {
		error = PTR_ERR(haptic->regulator);
		printk("regulator-haptic : unable to get regulator, err : %d\n", error);
		goto err_ifree_mem;
	}
	set_haptic_data(haptic);
	haptic->input_dev->name = "regulator-haptic";
	haptic->input_dev->dev.parent = &pdev->dev;
	haptic->input_dev->close = regulator_haptic_close;
	haptic->enabled = false;
	input_set_drvdata(haptic->input_dev, haptic);
	input_set_capability(haptic->input_dev, EV_FF, FF_RUMBLE);

	error = input_ff_create_memless(input_dev, NULL,
				      regulator_haptic_play);
	if (error) {
		printk("regulator-haptic : input_ff_create_memless() failed : %d\n", error);
		goto err_put_regulator;
	}
	error = input_register_device(haptic->input_dev);
	if (error) {
		printk("regulator-haptic : couldn't register input device : %d\n", error);
		goto err_destroy_ff;
	}
	platform_set_drvdata(pdev, haptic);
	return 0;

err_destroy_ff:
	input_ff_destroy(haptic->input_dev);
err_put_regulator:
	regulator_put(haptic->regulator);
err_ifree_mem:
	input_free_device(haptic->input_dev);
err_kfree_mem:
	kfree(haptic);

	return error;
}

static int regulator_haptic_remove(struct platform_device *pdev)
{
	struct regulator_haptic *haptic = platform_get_drvdata(pdev);

	input_unregister_device(haptic->input_dev);

	return 0;
}

static struct of_device_id regulator_haptic_dt_match[] = {
	{ .compatible = "linux,regulator-haptic" },
	{},
};

static struct platform_driver regulator_haptic_driver = {
	.driver		= {
		.name	= "regulator-haptic",
		.owner	= THIS_MODULE,
		.of_match_table = regulator_haptic_dt_match,
	},
	.probe		= regulator_haptic_probe,
	.remove		= regulator_haptic_remove,
};
module_platform_driver(regulator_haptic_driver);

MODULE_ALIAS("platform:regulator-haptic");
MODULE_DESCRIPTION("Regulator haptic driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sanghyeon Lee <sirano06.lee@samsung.com>");
