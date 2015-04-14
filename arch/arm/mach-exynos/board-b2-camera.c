/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>

#include <mach/exynos-fimc-is.h>
#include <mach/exynos-fimc-is-sensor.h>
#include <plat/devs.h>

#include <plat/gpio-cfg.h>
#include <plat/devs.h>
#include <plat/mipi_csis.h>
#include <media/exynos_camera.h>

#include "board-universal3250.h"

#define B2_REV00_BTYPE	0x01
extern unsigned int system_rev;
#if defined(CONFIG_VIDEO_EXYNOS_FIMC_IS)
static struct exynos_platform_fimc_is exynos_fimc_is_data;

static struct exynos_fimc_is_subip_info subip_info = {
	._mcuctl = {
		.valid		= 1,
		.full_bypass	= 0,
		.version	= 190,
		.base_addr	= 0,
	},
	._3a0 = {
		.valid		= 1,
		.full_bypass	= 0,
		.version	= 0,
		.base_addr	= 0,
	},
	._3a1 = {
		.valid		= 0,
		.full_bypass	= 0,
		.version	= 0,
		.base_addr	= 0,
	},
	._isp = {
		.valid		= 0,
		.full_bypass	= 0,
		.version	= 0,
		.base_addr	= 0,
	},
	._drc = {
		.valid		= 1,
		.full_bypass	= 1,
		.version	= 0,
		.base_addr	= 0,
	},
	._scc = {
		.valid		= 1,
		.full_bypass	= 0,
		.version	= 0,
		.base_addr	= 0,
	},
	._odc = {
		.valid		= 0,
		.full_bypass	= 0,
		.version	= 0,
		.base_addr	= 0,
	},
	._dis = {
		.valid		= 0,
		.full_bypass	= 0,
		.version	= 0,
		.base_addr	= 0,
	},
	._dnr = {
		.valid		= 0,
		.full_bypass	= 0,
		.version	= 0,
		.base_addr	= 0,
	},
	._scp = {
		.valid		= 0,
		.full_bypass	= 0,
		.version	= 0,
		.base_addr	= 0,
	},
	._fd = {
		.valid		= 1,
		.full_bypass	= 0,
		.version	= 0,
		.base_addr	= 0,
	},
	._pwm = {
		.valid		= 1,
		.full_bypass	= 0,
		.version	= 0,
		.base_addr	= 0,
	},
};
#endif

static struct exynos_platform_fimc_is_sensor s5k8b1 = {
	.scenario		= SENSOR_SCENARIO_NORMAL,
	.mclk_ch		= 1,
	.csi_ch			= CSI_ID_A,
	.flite_ch		= FLITE_ID_A,
	.i2c_ch			= SENSOR_CONTROL_I2C0,
	.i2c_addr		= 0x6A,
	.is_bns			= 0,
};


static struct platform_device *camera_devices[] __initdata = {
#if defined(CONFIG_VIDEO_EXYNOS_FIMC_IS)
	&exynos3_device_fimc_is_sensor0,
	&exynos3_device_fimc_is_sensor1,
	&exynos3_device_fimc_is,
#endif
};

void __init exynos3_universal3250_camera_init(void)
{
#if defined(CONFIG_VIDEO_EXYNOS_FIMC_IS)
	int ret;
/*
	dev_set_name(&exynos3_device_fimc_is.dev, "s5p-mipi-csis.0");
	clk_add_alias("gscl_wrap0", FIMC_IS_DEV_NAME,
			"gscl_wrap0", &exynos3_device_fimc_is.dev);
	dev_set_name(&exynos3_device_fimc_is.dev, "s5p-mipi-csis.1");
	clk_add_alias("gscl_wrap1", FIMC_IS_DEV_NAME,
			"gscl_wrap1", &exynos3_device_fimc_is.dev);
	dev_set_name(&exynos3_device_fimc_is.dev, FIMC_IS_DEV_NAME);
*/
	exynos_fimc_is_data.subip_info = &subip_info;

	SET_QOS(exynos_fimc_is_data.dvfs_data, FIMC_IS_SN_DEFAULT,
			135000, 400000, 0, 0, 100000, 0);
	SET_QOS(exynos_fimc_is_data.dvfs_data, FIMC_IS_SN_MAX,
			135000, 400000, 0, 0, 100000, 0);

	exynos_fimc_is_set_platdata(&exynos_fimc_is_data);

	/* s5k8b1: normal: on */
	SET_PIN(&s5k8b1, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 0,
		0, 0, "cam_io_1.8", PIN_REGULATOR_ON);
	SET_PIN(&s5k8b1, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 1,
		0, 0, "cam_vdd_2.8v", PIN_REGULATOR_ON);
	SET_PIN(&s5k8b1, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 2,
		0, 0, "cam_vdd_1.2", PIN_REGULATOR_ON);
	SET_PIN(&s5k8b1, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 3,
		0, 0, "", PIN_DELAY); /* add delay before RESET_HIGH */
	SET_PIN(&s5k8b1, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 4,
		EXYNOS3_GPM2(3), (1 << 8), "GPM2.3 (CAM_nRST)", PIN_OUTPUT_HIGH);
	SET_PIN(&s5k8b1, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 5,
		EXYNOS3_GPM2(2), (3 << 8), "GPM2.2 (CAM_MCLK)", PIN_FUNCTION);
	SET_PIN(&s5k8b1, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 6,
		EXYNOS3_GPM4(0), (2), "GPM4.0 (CAM_I2C0_SCL)", PIN_FUNCTION);
	SET_PIN(&s5k8b1, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 7,
		EXYNOS3_GPM4(1), (2<< 4), "GPM4.1 (CAM_I2C0_SDA)", PIN_FUNCTION);
	SET_PIN(&s5k8b1, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 8,
		EXYNOS3_GPM4(2), (2<<8), "GPM4.2 (CAM_I2C1_SCL)", PIN_FUNCTION);
	SET_PIN(&s5k8b1, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 9,
		EXYNOS3_GPM4(3), (2<< 12), "GPM4.3 (CAM_I2C1_SDA)", PIN_FUNCTION);
	SET_PIN(&s5k8b1, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 10,
		0, 0, "cam_af_2.8", PIN_REGULATOR_ON);
	SET_PIN(&s5k8b1, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 11,
		0, 0, "", PIN_END);

	/* s5k8b1: normal: off */
	SET_PIN(&s5k8b1, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 0,
		0, 0, "cam_af_2.8", PIN_REGULATOR_OFF);
	SET_PIN(&s5k8b1, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 1,
		EXYNOS3_GPM2(2), (3 << 8), "GPM2.2 (CAM_MCLK)", PIN_INPUT);
	SET_PIN(&s5k8b1, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 2,
		EXYNOS3_GPM2(3), (1 << 8), "GPM2.3 (CAM_nRST)", PIN_OUTPUT_LOW);
	SET_PIN(&s5k8b1, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 3,
		0, 0, "cam_vdd_1.2", PIN_REGULATOR_OFF);
	SET_PIN(&s5k8b1, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 4,
		0, 0, "cam_io_1.8", PIN_REGULATOR_OFF);
	SET_PIN(&s5k8b1, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 5,
		0, 0, "cam_vdd_2.8v", PIN_REGULATOR_OFF);
	SET_PIN(&s5k8b1, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 6,
		0, 0, "", PIN_END);

	if (system_rev == B2_REV00_BTYPE) {
		ret = gpio_request(EXYNOS3_GPA1(2), "AF_SDA_REV00");
		if (ret)
			pr_err("failed to request gpio(AF_SDA_REV00)(%d)\n", ret);
		s3c_gpio_cfgpin(EXYNOS3_GPA1(2), S3C_GPIO_INPUT);
		s3c_gpio_setpull(EXYNOS3_GPA1(2), S3C_GPIO_PULL_NONE);
		gpio_free(EXYNOS3_GPA1(2));

		ret = gpio_request(EXYNOS3_GPA1(3), "AF_SCL_REV00");
		if (ret)
			pr_err("failed to request gpio(AF_SCL_REV00)(%d)\n", ret);
		s3c_gpio_cfgpin(EXYNOS3_GPA1(3), S3C_GPIO_INPUT);
		s3c_gpio_setpull(EXYNOS3_GPA1(3), S3C_GPIO_PULL_NONE);
		gpio_free(EXYNOS3_GPA1(3));
	}

	exynos_fimc_is_sensor_set_platdata(&s5k8b1, &exynos3_device_fimc_is_sensor0);
	exynos_fimc_is_sensor_set_platdata(&s5k8b1, &exynos3_device_fimc_is_sensor1);
#endif

	platform_add_devices(camera_devices, ARRAY_SIZE(camera_devices));
}
