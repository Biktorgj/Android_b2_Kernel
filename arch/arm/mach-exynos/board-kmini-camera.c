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

#include "board-universal222ap.h"

#if defined(CONFIG_VIDEO_EXYNOS_FIMC_IS)
static struct exynos_platform_fimc_is exynos_fimc_is_data;

struct exynos_fimc_is_clk_gate_info gate_info = {
	.groups = {
		[FIMC_IS_GRP_3A0] = {
			.mask_clk_on_org	= 0,
			.mask_clk_on_mod	= 0,
			.mask_clk_off_self_org	= 0,
			.mask_clk_off_self_mod	= 0,
			.mask_clk_off_depend	= 0,
			.mask_cond_for_depend	= 0,
		},
		[FIMC_IS_GRP_3A1] = {
			.mask_clk_on_org	= 0,
			.mask_clk_on_mod	= 0,
			.mask_clk_off_self_org	= 0,
			.mask_clk_off_self_mod	= 0,
			.mask_clk_off_depend	= 0,
			.mask_cond_for_depend	= 0,
		},
		[FIMC_IS_GRP_ISP] = {
			.mask_clk_on_org	=
				(1 << FIMC_IS_GATE_ISP_IP) |
				(1 << FIMC_IS_GATE_DRC_IP) |
				(1 << FIMC_IS_GATE_SCC_IP) |
				(1 << FIMC_IS_GATE_SCP_IP) |
				(1 << FIMC_IS_GATE_FD_IP),
			.mask_clk_on_mod	= 0,
			.mask_clk_off_self_org	=
				(1 << FIMC_IS_GATE_ISP_IP) |
				(1 << FIMC_IS_GATE_DRC_IP) |
				(1 << FIMC_IS_GATE_SCC_IP) |
				(1 << FIMC_IS_GATE_SCP_IP) |
				(1 << FIMC_IS_GATE_FD_IP),
			.mask_clk_off_self_mod	= 0,
			.mask_clk_off_depend	= 0,
			.mask_cond_for_depend	= 0,
		},
		[FIMC_IS_GRP_DIS] = {
			.mask_clk_on_org	= 0,
			.mask_clk_on_mod	= 0,
			.mask_clk_off_self_org	= 0,
			.mask_clk_off_self_mod	= 0,
			.mask_clk_off_depend	= 0,
			.mask_cond_for_depend	= 0,
		},
	},
};

static struct exynos_fimc_is_subip_info subip_info = {
	._mcuctl = {
		.valid		= 1,
		.full_bypass	= 0,
		.version	= 161,
		.base_addr	= 0,
	},
	._3a0 = {
		.valid		= 0,
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
		.valid		= 1,
		.full_bypass	= 0,
		.version	= 0,
		.base_addr	= 0,
	},
	._drc = {
		.valid		= 1,
		.full_bypass	= 0,
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
		.valid		= 1,
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
		.base_addr	= 0x12160000,
	},
};
#endif

static struct exynos_platform_fimc_is_sensor s5k4h5 = {
	.scenario	= SENSOR_SCENARIO_NORMAL,
	.mclk_ch	= 0,
	.csi_ch		= CSI_ID_A,
	.flite_ch	= FLITE_ID_A,
	.i2c_ch		= SENSOR_CONTROL_I2C0,
	.i2c_addr	= 0x6E,
	.is_bns         = 0,
	.flash_first_gpio	= 8,
	.flash_second_gpio	= 6,
};

static struct exynos_platform_fimc_is_sensor s5k6a3 = {
	.scenario	= SENSOR_SCENARIO_NORMAL,
	.mclk_ch	= 1,
	.csi_ch		= CSI_ID_B,
	.flite_ch	= FLITE_ID_B,
	.i2c_ch		= SENSOR_CONTROL_I2C1,
	.i2c_addr	= 0x20,
	.is_bns         = 0,
};

static struct platform_device *camera_devices[] __initdata = {
#if defined(CONFIG_VIDEO_EXYNOS_FIMC_IS)
	&exynos_device_fimc_is_sensor0,
	&exynos_device_fimc_is_sensor1,
	&exynos4_device_fimc_is,
#endif
};

void __init exynos4_universal222ap_camera_init(void)
{
#if defined(CONFIG_VIDEO_EXYNOS_FIMC_IS)
	dev_set_name(&exynos4_device_fimc_is.dev, "s5p-mipi-csis.0");
	clk_add_alias("sclk_csis0", FIMC_IS_DEV_NAME,
			"sclk_csis", &exynos4_device_fimc_is.dev);
	clk_add_alias("csis0", FIMC_IS_DEV_NAME,
			"csis", &exynos4_device_fimc_is.dev);

	dev_set_name(&exynos4_device_fimc_is.dev, "s5p-mipi-csis.1");
	clk_add_alias("sclk_csis1", FIMC_IS_DEV_NAME,
			"sclk_csis", &exynos4_device_fimc_is.dev);
	clk_add_alias("csis1", FIMC_IS_DEV_NAME,
			"csis", &exynos4_device_fimc_is.dev);

	dev_set_name(&exynos4_device_fimc_is.dev, FIMC_IS_DEV_NAME);

	exynos_fimc_is_data.subip_info = &subip_info;

	SET_QOS(exynos_fimc_is_data.dvfs_data, FIMC_IS_SN_DEFAULT,
			200000, 400000, 108000000, 0, 0, 0);
	SET_QOS(exynos_fimc_is_data.dvfs_data, FIMC_IS_SN_MAX,
			200000, 400000, 108000000, 0, 0, 0);

	exynos_fimc_is_data.gate_info = &gate_info;

	exynos_fimc_is_set_platdata(&exynos_fimc_is_data);

	/* s5k4h5: normal: on */
	SET_PIN(&s5k4h5, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 0,
		0, 0, "main_cam_io_1v8", PIN_REGULATOR_ON);
	SET_PIN(&s5k4h5, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 1,
		0, 0, "main_cam_af_2v8", PIN_REGULATOR_ON);
	SET_PIN(&s5k4h5, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 2,
		0, 0, "cam_sensor_core_1v2", PIN_REGULATOR_ON);
	SET_PIN(&s5k4h5, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 3,
		0, 0, "cam_sensor_a2v8", PIN_REGULATOR_ON);
	SET_PIN(&s5k4h5, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 4,
		EXYNOS4_GPM2(2), (3 << 8), "GPM2.2 (CAM_MCLK)", PIN_FUNCTION);
	SET_PIN(&s5k4h5, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 5,
		EXYNOS4_GPM4(0), (2 << 0), "GPM4.0 (CAM_I2C0_SCL)", PIN_FUNCTION);
	SET_PIN(&s5k4h5, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 6,
		EXYNOS4_GPM4(1), (2 << 4), "GPM4.1 (CAM_I2C0_SDA)", PIN_FUNCTION);
	SET_PIN(&s5k4h5, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 7,
		EXYNOS4_GPM3(4), (2 << 16), "GPM3.4 (CAM_FLASH_EN)", PIN_FUNCTION);
	SET_PIN(&s5k4h5, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 8,
		EXYNOS4_GPM3(6), (2 << 24), "GPM3.6 (CAM_FLASH_TORCH)", PIN_FUNCTION);
	SET_PIN(&s5k4h5, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 9,
		EXYNOS4_GPX1(2), 1, "GPX1.2 (MAIN_CAM_RST)", PIN_RESET);
	SET_PIN(&s5k4h5, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 10,
		0, 0, "", PIN_END);

	/* s5k4h5: normal: off */
	SET_PIN(&s5k4h5, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 0,
		EXYNOS4_GPX1(2), 1, "GPX1.2 (MAIN_CAM_RST)", PIN_RESET);
	SET_PIN(&s5k4h5, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 1,
		EXYNOS4_GPX1(2), 1, "GPX1.2 (MAIN_CAM_RST)", PIN_INPUT);
	SET_PIN(&s5k4h5, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 2,
		0, 0, "main_cam_io_1v8", PIN_REGULATOR_OFF);
	SET_PIN(&s5k4h5, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 3,
		0, 0, "main_cam_af_2v8", PIN_REGULATOR_OFF);
	SET_PIN(&s5k4h5, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 4,
		0, 0, "cam_sensor_core_1v2", PIN_REGULATOR_ON);
	SET_PIN(&s5k4h5, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 5,
		0, 0, "cam_sensor_a2v8", PIN_REGULATOR_ON);
	SET_PIN(&s5k4h5, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 6,
		EXYNOS4_GPM3(4), 1, "GPM3.4 (CAM_FLASH_EN)", PIN_INPUT);
	SET_PIN(&s5k4h5, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 7,
		EXYNOS4_GPM3(6), 1, "GPM3.6 (CAM_FLASH_TORCH)", PIN_INPUT);
	SET_PIN(&s5k4h5, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 8,
		0, 0, "", PIN_END);

	/* s5k6a3: normal: on */
	SET_PIN(&s5k6a3, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 0,
		0, 0, "vt_cam_io_1v8", PIN_REGULATOR_ON);
	SET_PIN(&s5k6a3, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 1,
		0, 0, "cam_sensor_a2v8", PIN_REGULATOR_ON);
	SET_PIN(&s5k6a3, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 2,
		EXYNOS4_GPM2(2), (3 << 8), "GPM2.2 (CAM_MCLK)", PIN_FUNCTION);
	SET_PIN(&s5k6a3, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 3,
		EXYNOS4_GPM4(2), (2 << 8), "GPM4.2 (CAM_I2C1_SCL)", PIN_FUNCTION);
	SET_PIN(&s5k6a3, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 4,
		EXYNOS4_GPM4(3), (2 << 12), "GPM4.3 (CAM_I2C1_SDA)", PIN_FUNCTION);
	SET_PIN(&s5k6a3, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 5,
		EXYNOS4_GPM1(2), 1, "GPM1.2 (VT_CAM_RST)", PIN_RESET);
	SET_PIN(&s5k6a3, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 6,
		0, 0, "", PIN_END);

	/* s5k6a3: normal: off */
	SET_PIN(&s5k6a3, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 0,
		EXYNOS4_GPM1(2), 1, "GPM1.2 (VT_CAM_RST)", PIN_RESET);
	SET_PIN(&s5k6a3, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 1,
		EXYNOS4_GPE0(4), 1, "GPE0.4 (CAM_VT_nRST)", PIN_INPUT);
	SET_PIN(&s5k6a3, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 2,
		0, 0, "vt_cam_io_1v8", PIN_REGULATOR_OFF);
	SET_PIN(&s5k6a3, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 3,
		0, 0, "cam_sensor_a2v8", PIN_REGULATOR_ON);
	SET_PIN(&s5k6a3, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 4,
		0, 0, "", PIN_END);

	exynos_fimc_is_sensor_set_platdata(&s5k4h5, &exynos_device_fimc_is_sensor0);
	exynos_fimc_is_sensor_set_platdata(&s5k6a3, &exynos_device_fimc_is_sensor1);
#endif

	platform_add_devices(camera_devices, ARRAY_SIZE(camera_devices));
}
