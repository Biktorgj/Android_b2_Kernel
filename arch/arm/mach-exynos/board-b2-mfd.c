/* 
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/init.h>
#include <linux/export.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/platform_device.h>
#include <linux/regulator/machine.h>
#include <linux/io.h>
#ifdef CONFIG_MFD_MAX77803
#include <linux/mfd/max77803.h>
#include <linux/mfd/max77803-private.h>
#endif /* CONFIG_MFD_MAX77803 */
#ifdef CONFIG_MFD_MAX77836
#include <linux/mfd/max14577.h>
#include <linux/mfd/max14577-private.h>
#if defined(CONFIG_CHARGER_MAX14577)
#include <linux/battery/sec_charging_common.h>
#endif
#ifdef CONFIG_REGULATOR_MAX77836
#include <linux/regulator/max77836-regulator.h>
#endif /* CONFIG_REGULATOR_MAX77836 */
#endif /* CONFIG_MFD_MAX77836 */
#include <mach/map.h>
#include <plat/devs.h>
#include <plat/iic.h>
#include <plat/gpio-cfg.h>
#include "board-universal3250.h"

#define GPIO_IF_PMIC_SDA	EXYNOS3_GPD0(2)
#define GPIO_IF_PMIC_SCL	EXYNOS3_GPD0(3)
#define GPIO_IF_PMIC_IRQ	EXYNOS3_GPX1(5)
#define I2C_IF_PMIC_ID		7

#ifdef CONFIG_MFD_MAX77803
#define MFD_DEV_NAME 		"max77803"
#define MFD_DEV_ADDR		(0x46 >> 1)

extern struct max77803_muic_data max77803_muic;

static struct regulator_consumer_supply safeout_supply[] = {
	REGULATOR_SUPPLY("safeout", NULL),
};

//static struct regulator_consumer_supply charger_supply[] = {
//	REGULATOR_SUPPLY("vinchg1", "charger-manager.0"),
//};

static struct regulator_init_data safeout_init_data = {
	.constraints	= {
		.name		= "safeout range",
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.always_on	= 0,
		.boot_on	= 1,
		.state_mem	= {
			.enabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(safeout_supply),
	.consumer_supplies	= safeout_supply,
};


//static struct regulator_init_data charger_init_data = {
//	.constraints	= {
//		.name		= "CHARGER",
//		.valid_ops_mask = REGULATOR_CHANGE_STATUS |
//		REGULATOR_CHANGE_CURRENT,
//		.boot_on	= 1,
//		.min_uA		= 60000,
//		.max_uA		= 2580000,
//	},
//	.num_consumer_supplies	= ARRAY_SIZE(charger_supply),
//	.consumer_supplies	= charger_supply,
//};

struct max77803_regulator_data max77803_regulators[] = {
	{MAX77803_ESAFEOUT1, &safeout_init_data,},
//	{MAX77803_CHARGER, &charger_init_data,},
};


static bool is_muic_default_uart_path_cp(void)
{
	return false;
}

static struct max77803_platform_data b2_mfd_pdata = {
	.irq_base = IRQ_BOARD_IFIC_START,
	.irq_gpio = GPIO_IF_PMIC_IRQ,
	.wakeup		= 1,
	.muic = &max77803_muic,
	.is_default_uart_path_cp = is_muic_default_uart_path_cp,
	.regulators = max77803_regulators,
	.num_regulators = MAX77803_REG_MAX,
};
#endif /* CONFIG_MFD_MAX77803 */

#ifdef CONFIG_MFD_MAX77836
#define MFD_DEV_ADDR		(0x4a >> 1)
#define SYSREG_I2C_MUX		(S3C_VA_SYS + 0x0228)

#if defined(CONFIG_CHARGER_MAX14577)
extern sec_battery_platform_data_t sec_battery_pdata;
#endif

#ifdef CONFIG_REGULATOR_MAX77836
static struct regulator_consumer_supply __initdata ldo1_consumer =
	REGULATOR_SUPPLY("vdd_ldo1", NULL);

static struct regulator_consumer_supply __initdata ldo2_consumer =
	REGULATOR_SUPPLY("vdd_ldo2", NULL);

static struct regulator_init_data __initdata max77836_ldo1_data = {
	.constraints	= {
		.name		= "vdd_ldo1 range",
		.min_uV		= 800000,
		.max_uV		= 3950000,
		.apply_uV	= 1,
		.always_on	= 0,
		.state_mem	= {
			.enabled	= 0,
		},
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &ldo1_consumer,
};

static struct regulator_init_data __initdata max77836_ldo2_data = {
	.constraints	= {
		.name		= "vdd_ldo2 range",
		.min_uV		= 800000,
		.max_uV		= 3950000,
		.apply_uV	= 1,
		.always_on	= 0,
		.state_mem	= {
			.enabled	= 0,
		},
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &ldo2_consumer,
};

static struct max77836_reg_platform_data __initdata max77836_reglator_pdata[] = {
	{
		.reg_id = MAX77836_LDO1,
		.init_data = &max77836_ldo1_data,
		.overvoltage_clamp_enable = 0,
		.auto_low_power_mode_enable = 0,
		.compensation_ldo = 0,
		.active_discharge_enable = 0,
		.soft_start_slew_rate = 0,
		.enable_mode = REG_NORMAL_MODE,

	},
	{
		.reg_id = MAX77836_LDO2,
		.init_data = &max77836_ldo2_data,
		.overvoltage_clamp_enable = 0,
		.auto_low_power_mode_enable = 0,
		.compensation_ldo = 0,
		.active_discharge_enable = 0,
		.soft_start_slew_rate = 0,
		.enable_mode = REG_NORMAL_MODE,

	},
};

static int enable_mode[] = {
	[MAX77836_LDO1] = REG_NORMAL_MODE,
	[MAX77836_LDO2] = REG_NORMAL_MODE,
};
#endif /* CONFIG_REGULATOR_MAX77836 */

static struct max14577_platform_data b2_mfd_pdata = {
	.irq_base		= IRQ_BOARD_IFIC_START,
	.irq_gpio		= GPIO_IF_PMIC_IRQ,
	.wakeup			= true,

#if defined(CONFIG_CHARGER_MAX14577)
	.charger_data		= &sec_battery_pdata,
#endif
#ifdef CONFIG_REGULATOR_MAX77836
	.regulators = max77836_reglator_pdata,
	.num_regulators = ARRAY_SIZE(max77836_reglator_pdata),
	.opmode_data = enable_mode,
#endif /* CONFIG_REGULATOR_MAX77836 */
};
#endif /* CONFIG_MFD_MAX77836 */

#if defined(CONFIG_MFD_MAX77803) || defined(CONFIG_MFD_MAX77836)
static struct i2c_board_info __initdata i2c_devs_if_pmic[] = {
	{
		I2C_BOARD_INFO(MFD_DEV_NAME, MFD_DEV_ADDR),
		.platform_data = &b2_mfd_pdata,
	}
};

#ifdef CONFIG_MUIC_I2C_USE_S3C_DEV_I2C7
static struct s3c2410_platform_i2c __initdata i2c_data_if_pmic = {
	.bus_num	= I2C_IF_PMIC_ID,
	.flags		= 0,
	.slave_addr	= MFD_DEV_ADDR,
	.frequency	= 400*1000,
	.sda_delay	= 100,
};
#else
static struct i2c_gpio_platform_data gpio_i2c_if_pmic = {
	.sda_pin = GPIO_IF_PMIC_SDA,
	.scl_pin = GPIO_IF_PMIC_SCL,
};

static struct platform_device device_i2c_if_pmic = {
	.name = "i2c-gpio",
	.id = I2C_IF_PMIC_ID,
	.dev.platform_data = &gpio_i2c_if_pmic,
};
#endif /* CONFIG_MUIC_I2C_USE_S3C_DEV_I2C7 */
#endif /* CONFIG_MFD_MAX77803 || CONFIG_MFD_MAX77836 */

void __init exynos3_b2_mfd_init(void)
{
#if defined(CONFIG_MFD_MAX77803) || defined(CONFIG_MFD_MAX77836)

#ifdef CONFIG_MUIC_I2C_USE_S3C_DEV_I2C7
	pr_info("%s: Use hw i2c 7\n", __func__);

	/*
	 * S3C_I2C7 dedicated to using internal block
	 * If want to use external I2C, set 0x0 to I2C_MUX register
	 */
	__raw_writel(0x0, SYSREG_I2C_MUX);
	s3c_i2c7_set_platdata(&i2c_data_if_pmic);
	i2c_register_board_info(I2C_IF_PMIC_ID,
		i2c_devs_if_pmic, ARRAY_SIZE(i2c_devs_if_pmic));
	platform_device_register(&s3c_device_i2c7);
#else
	pr_info("%s: Use sw i2c gpio\n", __func__);
	s3c_gpio_cfgpin(GPIO_IF_PMIC_SDA, S3C_GPIO_INPUT);
	s3c_gpio_cfgpin(GPIO_IF_PMIC_SCL, S3C_GPIO_INPUT);

	s3c_gpio_setpull(GPIO_IF_PMIC_SDA, S3C_GPIO_PULL_UP);
	s3c_gpio_setpull(GPIO_IF_PMIC_SCL, S3C_GPIO_PULL_UP);

	i2c_register_board_info(I2C_IF_PMIC_ID,
		i2c_devs_if_pmic, ARRAY_SIZE(i2c_devs_if_pmic));

	platform_device_register(&device_i2c_if_pmic);
#endif /* CONFIG_MUIC_I2C_USE_S3C_DEV_I2C7 */

#endif /* CONFIG_MFD_MAX77803 || CONFIG_MFD_MAX77836 */
}
