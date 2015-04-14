/* linux/arch/arm/mach-exynos/setup-fimc-sensor.c
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/clk.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif
#include <plat/gpio-cfg.h>

#include <mach/exynos-fimc-is.h>
#include <mach/exynos-fimc-is-sensor.h>
#if defined(CONFIG_SOC_EXYNOS5430)
#include <mach/regs-clock-exynos5430.h>
#endif

static int exynos_fimc_is_sensor_pin_control(struct platform_device *pdev,
	int pin, u32 value, char *name, u32 act, u32 channel)
{
	int ret = 0;
	char ch_name[30];

	snprintf(ch_name, sizeof(ch_name), "%s%d", name, channel);
	pr_info("%s(pin(%d), act(%d), ch(%s))\n", __func__, pin, act, ch_name);

	switch (act) {
	case PIN_PULL_NONE:
		break;
	case PIN_OUTPUT_HIGH:
		if (gpio_is_valid(pin)) {
			gpio_request_one(pin, GPIOF_OUT_INIT_HIGH, "CAM_GPIO_OUTPUT_HIGH");
			gpio_free(pin);
		}
		break;
	case PIN_OUTPUT_LOW:
		if (gpio_is_valid(pin)) {
			gpio_request_one(pin, GPIOF_OUT_INIT_LOW, "CAM_GPIO_OUTPUT_LOW");
			gpio_free(pin);
		}
		break;
	case PIN_INPUT:
		if (gpio_is_valid(pin)) {
			gpio_request_one(pin, GPIOF_IN, "CAM_GPIO_INPUT");
			gpio_free(pin);
		}
		break;
	case PIN_RESET:
		if (gpio_is_valid(pin)) {
			gpio_request_one(pin, GPIOF_OUT_INIT_HIGH, "CAM_GPIO_RESET");
			usleep_range(1000, 1000);
			__gpio_set_value(pin, 0);
			usleep_range(1000, 1000);
			__gpio_set_value(pin, 1);
			usleep_range(1000, 1000);
			gpio_free(pin);
		}
		break;
	case PIN_FUNCTION:
		s3c_gpio_cfgpin(pin, value);
		s3c_gpio_setpull(pin, S3C_GPIO_PULL_NONE);
		usleep_range(1000, 1000);
		break;
	case PIN_REGULATOR_ON:
		{
			struct regulator *regulator;

			regulator = regulator_get(&pdev->dev, name);
			if (IS_ERR(regulator)) {
				pr_err("%s : regulator_get(%s) fail\n", __func__, name);
				return PTR_ERR(regulator);
			}

			ret = regulator_enable(regulator);
			if (ret) {
				pr_err("%s : regulator_enable(%s) fail\n", __func__, name);
				regulator_put(regulator);
				return ret;
			}

			regulator_put(regulator);
		}
		break;
	case PIN_REGULATOR_OFF:
		{
			struct regulator *regulator;

			regulator = regulator_get(&pdev->dev, name);
			if (IS_ERR(regulator)) {
				pr_err("%s : regulator_get(%s) fail\n", __func__, name);
				return PTR_ERR(regulator);
			}

			if (!regulator_is_enabled(regulator)) {
				pr_warning("%s regulator is already disabled\n", name);
				regulator_put(regulator);
				return 0;
			}

			ret = regulator_disable(regulator);
			if (ret) {
				pr_err("%s : regulator_disable(%s) fail\n", __func__, name);
				regulator_put(regulator);
				return ret;
			}

			regulator_put(regulator);
		}
		break;
	case PIN_DELAY:
		usleep_range(1000, 1000);
		break;
	default:
		pr_err("unknown act for pin\n");
		break;
	}

	return ret;
}

int exynos_fimc_is_sensor_pins_cfg(struct platform_device *pdev,
	u32 scenario,
	u32 enable)
{
	int ret = 0;
	u32 i = 0;
	struct exynos_sensor_pin (*pin_ctrls)[2][GPIO_CTRL_MAX];
	struct exynos_platform_fimc_is_sensor *pdata;

	BUG_ON(!pdev);
	BUG_ON(!pdev->dev.platform_data);
	BUG_ON(enable > 1);
	BUG_ON(scenario > SENSOR_SCENARIO_MAX);

	pdata = pdev->dev.platform_data;
	pin_ctrls = pdata->pin_ctrls;

	for (i = 0; i < GPIO_CTRL_MAX; ++i) {
		if (pin_ctrls[scenario][enable][i].act == PIN_END) {
			pr_info("gpio cfg is end(%d)\n", i);
			break;
		}

		ret = exynos_fimc_is_sensor_pin_control(pdev,
			pin_ctrls[scenario][enable][i].pin,
			pin_ctrls[scenario][enable][i].value,
			pin_ctrls[scenario][enable][i].name,
			pin_ctrls[scenario][enable][i].act,
			pdata->csi_ch);

		if (ret) {
			pr_err("exynos5_fimc_is_sensor_gpio(%d, %d, %s, %d, %d) is fail(%d)",
				pin_ctrls[scenario][enable][i].pin,
				pin_ctrls[scenario][enable][i].value,
				pin_ctrls[scenario][enable][i].name,
				pin_ctrls[scenario][enable][i].act,
				pdata->csi_ch,
				ret);
			goto p_err;
		}
	}

p_err:
	return ret;

}

#if defined(CONFIG_SOC_EXYNOS5260)
/* EXYNOS5260 */
int exynos5260_fimc_is_sensor_iclk_cfg(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
	struct clk *sclk_mediatop_pll = NULL;
	struct clk *aclk_gscl_fimc = NULL;
	struct clk *aclk_gscl_fimc_user = NULL;
	struct clk *sclk_csis = NULL;

	struct clk *cam_src = NULL;
	struct clk *cam_clk = NULL;

	pr_info("%s\n", __func__);

	/* camif part */
	sclk_mediatop_pll = clk_get(&pdev->dev, "sclk_mediatop_pll");
	if (IS_ERR(sclk_mediatop_pll))
		return PTR_ERR(sclk_mediatop_pll);

	aclk_gscl_fimc = clk_get(&pdev->dev, "aclk_gscl_fimc");
	if (IS_ERR(aclk_gscl_fimc)) {
		clk_put(sclk_mediatop_pll);
		return PTR_ERR(aclk_gscl_fimc);
	}

	aclk_gscl_fimc_user = clk_get(&pdev->dev, "aclk_gscl_fimc_user");
	if (IS_ERR(aclk_gscl_fimc_user)) {
		clk_put(sclk_mediatop_pll);
		clk_put(aclk_gscl_fimc);
		return PTR_ERR(aclk_gscl_fimc_user);
	}

	sclk_csis = clk_get(&pdev->dev, "sclk_csis");
	if (IS_ERR(sclk_csis)) {
		clk_put(sclk_mediatop_pll);
		clk_put(aclk_gscl_fimc);
		clk_put(aclk_gscl_fimc_user);
		return PTR_ERR(sclk_csis);
	}

	clk_set_parent(aclk_gscl_fimc, sclk_mediatop_pll);
	clk_set_rate(aclk_gscl_fimc, 334 * 1000000);
	clk_set_parent(aclk_gscl_fimc_user, aclk_gscl_fimc);
	clk_set_parent(sclk_csis, aclk_gscl_fimc_user);

	clk_put(sclk_mediatop_pll);
	clk_put(aclk_gscl_fimc);
	clk_put(aclk_gscl_fimc_user);
	clk_put(sclk_csis);

	/* sensor part */
	cam_src = clk_get(&pdev->dev, "ext_xtal");
	if (IS_ERR(cam_src))
		return PTR_ERR(cam_src);

	/** #0 */
	cam_clk = clk_get(&pdev->dev, "dout_isp1_sensor0_a");
	if (IS_ERR(cam_clk)) {
		clk_put(cam_src);
		return PTR_ERR(cam_clk);
	}

	clk_set_parent(cam_clk, cam_src);
	clk_set_rate(cam_clk, 24 * 1000000);
	clk_put(cam_clk);

	/** #1 */
	cam_clk = clk_get(&pdev->dev, "dout_isp1_sensor1_a");
	if (IS_ERR(cam_clk)) {
		clk_put(cam_src);
		return PTR_ERR(cam_clk);
	}

	clk_set_parent(cam_clk, cam_src);
	clk_set_rate(cam_clk, 24 * 1000000);
	clk_put(cam_clk);

	/** #2 */
	cam_clk = clk_get(&pdev->dev, "dout_isp1_sensor2_a");
	if (IS_ERR(cam_clk)) {
		clk_put(cam_src);
		return PTR_ERR(cam_clk);
	}

	clk_set_parent(cam_clk, cam_src);
	clk_set_rate(cam_clk, 24 * 1000000);
	clk_put(cam_clk);

	clk_put(cam_src);

	return 0;
}

int exynos5260_fimc_is_sensor_iclk_on(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
	int ch;
	char mipi[20];
	char flite[20];
	struct clk *mipi_ctrl = NULL;
	struct clk *flite_ctrl = NULL;

	pr_info("%s\n", __func__);

	for (ch = FLITE_ID_A; ch < 2; ch++) {
		snprintf(mipi, sizeof(mipi), "gscl_wrap%d", ch);
		snprintf(flite, sizeof(flite), "gscl_flite.%d", ch);

		mipi_ctrl = clk_get(&pdev->dev, mipi);
		if (IS_ERR(mipi_ctrl)) {
			pr_err("%s : clk_get(%s) failed\n",
				__func__, mipi);
			return  PTR_ERR(mipi_ctrl);
		}

		clk_enable(mipi_ctrl);
		clk_put(mipi_ctrl);

		flite_ctrl = clk_get(&pdev->dev, flite);
		if (IS_ERR(flite_ctrl)) {
			pr_err("%s : clk_get(%s) failed\n",
				__func__, flite);
			return PTR_ERR(flite_ctrl);
		}

		clk_enable(flite_ctrl);
		clk_put(flite_ctrl);
	}

	return 0;
}

int exynos5260_fimc_is_sensor_iclk_off(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
	int ch;
	char mipi[20];
	char flite[20];
	struct clk *mipi_ctrl = NULL;
	struct clk *flite_ctrl = NULL;

	pr_info("%s\n", __func__);

	for (ch = FLITE_ID_A; ch < 2; ch++) {
		snprintf(mipi, sizeof(mipi), "gscl_wrap%d", ch);
		snprintf(flite, sizeof(flite), "gscl_flite.%d", ch);

		mipi_ctrl = clk_get(&pdev->dev, mipi);
		if (IS_ERR(mipi_ctrl)) {
			pr_err("%s : clk_get(%s) failed\n",
				__func__, mipi);
			return  PTR_ERR(mipi_ctrl);
		}

		/* FIXME */
		clk_enable(mipi_ctrl);
		clk_put(mipi_ctrl);

		flite_ctrl = clk_get(&pdev->dev, flite);
		if (IS_ERR(flite_ctrl)) {
			pr_err("%s : clk_get(%s) failed\n",
				__func__, flite);
			return PTR_ERR(flite_ctrl);
		}

		/* FIXME */
		clk_enable(flite_ctrl);
		clk_put(flite_ctrl);
	}

	return 0;
}

int exynos5260_fimc_is_sensor_mclk_on(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
	struct clk *sensor_ctrl = NULL;

	pr_info("%s\n", __func__);

	sensor_ctrl = clk_get(&pdev->dev, "isp1_sensor");
	if (IS_ERR(sensor_ctrl)) {
		pr_err("%s : clk_get(isp1_sensor) failed\n", __func__);
		return PTR_ERR(sensor_ctrl);
	}

	clk_enable(sensor_ctrl);
	clk_put(sensor_ctrl);

	return 0;
}

int exynos5260_fimc_is_sensor_mclk_off(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
	struct clk *sensor_ctrl = NULL;

	pr_info("%s\n", __func__);

	sensor_ctrl = clk_get(&pdev->dev, "isp1_sensor");
	if (IS_ERR(sensor_ctrl)) {
		pr_err("%s : clk_get(isp1_sensor) failed\n", __func__);
		return PTR_ERR(sensor_ctrl);
	}

	clk_disable(sensor_ctrl);
	clk_put(sensor_ctrl);

	return 0;
}
#elif defined(CONFIG_SOC_EXYNOS3470)
int exynos3470_fimc_is_sensor_iclk_cfg(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
	struct clk *mout_mpll = NULL;
	struct clk *sclk_csis0 = NULL;
	struct clk *sclk_csis1 = NULL;
	struct clk *sclk_cam_src = NULL;
	struct clk *sclk_cam1 = NULL;

	pr_debug("(%s) ++++\n", __func__);

	/* 1. MIPI-CSI */
	mout_mpll = clk_get(&pdev->dev, "mout_mpll_user_top");
	if (IS_ERR(mout_mpll)) {
		pr_err("fail to get 'mout_mpll'\n");
		return PTR_ERR(mout_mpll);
	}
	sclk_csis0 = clk_get(&pdev->dev, "sclk_csis0");
	if (IS_ERR(sclk_csis0)) {
		clk_put(mout_mpll);
		pr_err("fail to get 'sclk_csis0'\n");
		return PTR_ERR(sclk_csis0);
	}
	clk_set_parent(sclk_csis0, mout_mpll);
	clk_set_rate(sclk_csis0, 267 * 1000000);

	sclk_csis1 = clk_get(&pdev->dev, "sclk_csis1");
	if (IS_ERR(sclk_csis1)) {
		clk_put(sclk_csis0);
		clk_put(mout_mpll);
		pr_err("fail to get 'sclk_csis1'\n");
		return PTR_ERR(sclk_csis1);
	}
	clk_set_parent(sclk_csis1, mout_mpll);
	clk_set_rate(sclk_csis1, 267 * 1000000);

	pr_info("sclk_csis0 : %ld\n", clk_get_rate(sclk_csis0));
	pr_info("sclk_csis1 : %ld\n", clk_get_rate(sclk_csis1));
	clk_put(mout_mpll);
	clk_put(sclk_csis0);
	clk_put(sclk_csis1);

	/* 2. Sensor clock */
	sclk_cam_src = clk_get(&pdev->dev, "xusbxti");

	if (IS_ERR(sclk_cam_src)) {
		pr_err("fail to get 'mout_epll'\n");
		return PTR_ERR(sclk_cam_src);
	}

	sclk_cam1 = clk_get(&pdev->dev, "sclk_cam1");
	if (IS_ERR(sclk_cam1)) {
		clk_put(sclk_cam_src);
		pr_err("fail to get 'sclk_cam1'\n");
		return PTR_ERR(sclk_cam1);
	}
	clk_set_parent(sclk_cam1, sclk_cam_src);
	clk_set_rate(sclk_cam1, 24 * 1000000);

	pr_debug("sclk_cam_src : %ld\n", clk_get_rate(sclk_cam_src));
	pr_debug("sclk_cam1 : %ld\n", clk_get_rate(sclk_cam1));
	clk_put(sclk_cam_src);
	clk_put(sclk_cam1);

	pr_debug("(%s) ----\n", __func__);
	return 0;
}

int exynos3470_fimc_is_sensor_mclk_on(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
	struct clk *sclk_cam1 = NULL;
	struct clk *cam1 = NULL;

	pr_debug("(%s) ++++\n", __func__);

	/* Sensor */
	sclk_cam1 = clk_get(&pdev->dev, "sclk_cam1");
	if (IS_ERR(sclk_cam1)) {
		pr_err("fail to get 'sclk_cam1'\n");
		return PTR_ERR(sclk_cam1);
	}
	clk_enable(sclk_cam1);
	clk_put(sclk_cam1);

	cam1 = clk_get(&pdev->dev, "cam1");
	if (IS_ERR(cam1)) {
		pr_err("fail to get 'cam1'\n");
		return PTR_ERR(cam1);
	}
	clk_enable(cam1);
	clk_put(cam1);

	pr_debug("(%s) ----\n", __func__);
	return 0;
}

int exynos3470_fimc_is_sensor_mclk_off(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
	struct clk *sclk_cam1 = NULL;
	struct clk *cam1 = NULL;

	pr_debug("(%s) ++++\n", __func__);

	/* Sensor */
	sclk_cam1 = clk_get(&pdev->dev, "sclk_cam1");
	if (IS_ERR(sclk_cam1))
		return PTR_ERR(sclk_cam1);
	clk_disable(sclk_cam1);
	clk_put(sclk_cam1);

	cam1 = clk_get(&pdev->dev, "cam1");
	if (IS_ERR(cam1)) {
		pr_err("fail to get 'cam1'\n");
		return PTR_ERR(cam1);
	}
	clk_disable(cam1);
	clk_put(cam1);

	pr_debug("(%s) ----\n", __func__);
	return 0;
}

int exynos3470_fimc_is_sensor_iclk_on(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
	int ch;
	char mipi[20];
	char flite[20];
	struct clk *mipi_ctrl = NULL;
	struct clk *flite_ctrl = NULL;

	pr_info("%s\n", __func__);

	for (ch = FLITE_ID_A; ch < 2; ch++) {
		snprintf(mipi, sizeof(mipi), "csis%d", ch);
		snprintf(flite, sizeof(flite), "fimc-lite%d", ch);

		mipi_ctrl = clk_get(&pdev->dev, mipi);
		if (IS_ERR(mipi_ctrl)) {
			pr_err("%s : clk_get(%s) failed\n",
				__func__, mipi);
			return  PTR_ERR(mipi_ctrl);
		}

		/* FIXME */
		clk_enable(mipi_ctrl);
		clk_put(mipi_ctrl);

		flite_ctrl = clk_get(&pdev->dev, flite);
		if (IS_ERR(flite_ctrl)) {
			pr_err("%s : clk_get(%s) failed\n",
				__func__, flite);
			return PTR_ERR(flite_ctrl);
		}

		/* FIXME */
		clk_enable(flite_ctrl);
		clk_put(flite_ctrl);
	}
	pr_debug("(%s) ----\n", __func__);

	return 0;
}

int exynos3470_fimc_is_sensor_iclk_off(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
	int ch;
	char mipi[20];
	char flite[20];
	struct clk *mipi_ctrl = NULL;
	struct clk *flite_ctrl = NULL;

	pr_info("%s\n", __func__);

	for (ch = FLITE_ID_A; ch < 2; ch++) {
		snprintf(mipi, sizeof(mipi), "csis%d", ch);
		snprintf(flite, sizeof(flite), "fimc-lite%d", ch);

		mipi_ctrl = clk_get(&pdev->dev, mipi);
		if (IS_ERR(mipi_ctrl)) {
			pr_err("%s : clk_get(%s) failed\n",
				__func__, mipi);
			return  PTR_ERR(mipi_ctrl);
		}

		/* FIXME */
		clk_disable(mipi_ctrl);
		clk_put(mipi_ctrl);

		flite_ctrl = clk_get(&pdev->dev, flite);
		if (IS_ERR(flite_ctrl)) {
			pr_err("%s : clk_get(%s) failed\n",
				__func__, flite);
			return PTR_ERR(flite_ctrl);
		}

		/* FIXME */
		clk_disable(flite_ctrl);
		clk_put(flite_ctrl);
	}

	pr_debug("(%s) ----\n", __func__);

	return 0;
}
#elif defined(CONFIG_ARCH_EXYNOS3)
int exynos3_fimc_is_sensor_iclk_cfg(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
	return 0;
}

int exynos3_fimc_is_sensor_mclk_on(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
	return 0;
}

int exynos3_fimc_is_sensor_mclk_off(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
	return 0;
}

int exynos3_fimc_is_sensor_iclk_on(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
	return 0;
}

int exynos3_fimc_is_sensor_iclk_off(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
	return 0;
}
#endif

/* COMMON INTERFACE */
int exynos_fimc_is_sensor_iclk_cfg(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
#if defined(CONFIG_SOC_EXYNOS5260)
	return exynos5260_fimc_is_sensor_iclk_cfg(pdev, scenario, channel);
#elif defined(CONFIG_SOC_EXYNOS3470)
	return exynos3470_fimc_is_sensor_iclk_cfg(pdev, scenario, channel);
#elif defined(CONFIG_ARCH_EXYNOS3)
	return exynos3_fimc_is_sensor_iclk_cfg(pdev, scenario, channel);
#else
	pr_info("%s : can't find!! \n", __func__);
	return 0;
#endif
}

int exynos_fimc_is_sensor_iclk_on(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
#if defined(CONFIG_SOC_EXYNOS5260)
	return exynos5260_fimc_is_sensor_iclk_on(pdev, scenario, channel);
#elif defined(CONFIG_SOC_EXYNOS3470)
	return exynos3470_fimc_is_sensor_iclk_on(pdev, scenario, channel);
#elif defined(CONFIG_ARCH_EXYNOS3)
	return exynos3_fimc_is_sensor_iclk_on(pdev, scenario, channel);
#else
	pr_info("%s : can't find!! \n", __func__);
	return 0;
#endif
}

int exynos_fimc_is_sensor_iclk_off(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
#if defined(CONFIG_SOC_EXYNOS5260)
	return exynos5260_fimc_is_sensor_iclk_off(pdev, scenario, channel);
#elif defined(CONFIG_SOC_EXYNOS3470)
	return exynos3470_fimc_is_sensor_iclk_off(pdev, scenario, channel);
#elif defined(CONFIG_ARCH_EXYNOS3)
	return exynos3_fimc_is_sensor_iclk_off(pdev, scenario, channel);
#else
	pr_info("%s : can't find!! \n", __func__);
	return 0;
#endif
}

int exynos_fimc_is_sensor_mclk_on(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
#if defined(CONFIG_SOC_EXYNOS5260)
	return exynos5260_fimc_is_sensor_mclk_on(pdev, scenario, channel);
#elif defined(CONFIG_SOC_EXYNOS3470)
	return exynos3470_fimc_is_sensor_mclk_on(pdev, scenario, channel);
#elif defined(CONFIG_ARCH_EXYNOS3)
	return exynos3_fimc_is_sensor_mclk_on(pdev, scenario, channel);
#else
	pr_info("%s : can't find!! \n", __func__);
	return 0;
#endif
}

int exynos_fimc_is_sensor_mclk_off(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
#if defined(CONFIG_SOC_EXYNOS5260)
	return exynos5260_fimc_is_sensor_mclk_off(pdev, scenario, channel);
#elif defined(CONFIG_SOC_EXYNOS3470)
	return exynos3470_fimc_is_sensor_mclk_off(pdev, scenario, channel);
#elif defined(CONFIG_ARCH_EXYNOS3)
	return exynos3_fimc_is_sensor_mclk_off(pdev, scenario, channel);
#else
	pr_info("%s : can't find!! \n", __func__);
	return 0;
#endif
}
