/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
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
#include <linux/io.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi_gpio.h>

#include <plat/gpio-cfg.h>
#include <plat/devs.h>
#include <plat/s3c64xx-spi.h>
#include <plat/iic.h>
#include <plat/clock.h>
#include <plat/s5p-clock.h>
#include <plat/clock-clksrc.h>

#include <mach/irqs.h>
#include <mach/map.h>
#include <mach/regs-pmu.h>
#include <mach/gpio.h>
#include <mach/spi-clocks.h>
#include "../../../sound/soc/codecs/ymu831/ymu831_priv.h"

#include "board-universal3250.h"

#define GPIO_CODEC_LDO_EN EXYNOS3_GPX3(6)

struct platform_device b2_audio_device = {
	.name = "b2-audio",
	.id = -1,
};

#ifdef CONFIG_SND_SOC_YAMAHA_YMU831
static void ymu831_set_codec_ldod(int status)
{
	pr_info("%s %d\n", __func__, status);
	gpio_set_value(GPIO_CODEC_LDO_EN, status);
}

static struct mc_asoc_platform_data mc_asoc_pdata = {
	.set_ext_micbias = NULL,
	.set_codec_ldod = ymu831_set_codec_ldod,
};
#endif

#ifdef CONFIG_S3C64XX_DEV_SPI1
static struct s3c64xx_spi_csinfo spi1_csi[] = {
        [0] = {
		.line           = EXYNOS3_GPB(5),
		.set_level      = gpio_set_value,
		.fb_delay       = 0x2,
        },
};
static struct spi_board_info spi1_board_info[] __initdata = {
	{
		.modalias               = "ymu831",
		.platform_data          = &mc_asoc_pdata,
		.max_speed_hz           = 15*1000*1000,
		.bus_num                = 1,
		.chip_select            = 0,
		.mode                   = SPI_MODE_0,
		.controller_data        = &spi1_csi[0],
	}
};
#endif

static struct platform_device *b2_audio_devices[] __initdata = {
#ifdef CONFIG_S3C64XX_DEV_SPI1
	&s3c64xx_device_spi1,
#endif
	&b2_audio_device,
#ifdef CONFIG_SND_SAMSUNG_I2S
	&exynos4_device_i2s2,
#endif
	&samsung_asoc_dma,
};

static void exynos3_b2_audio_gpio_init(void)
{
	int err;

	pr_info("%s\n", __func__);
	err = gpio_request(GPIO_CODEC_LDO_EN, "GPIO_CODEC_LDO_EN");
	if (err) {
		pr_err("GPIO_CODEC_LDO_EN set error\n");
		return;
	}
	gpio_direction_output(GPIO_CODEC_LDO_EN, 1);
	gpio_set_value(GPIO_CODEC_LDO_EN, 1);
	gpio_free(GPIO_CODEC_LDO_EN);
}

static void exynos3_b2_audio_clock_setup(void)
{
#ifdef CONFIG_SND_SOC_SAMSUNG_B2_USE_CLK_FW
	struct clk *xusbxti = NULL;
	struct clk *clkout = NULL;
#endif

	pr_info("%s\n", __func__);

	/* TODO: need to check usage of clk_fout_epll */
	clk_set_rate(&clk_fout_epll, 49152000);
	clk_enable(&clk_fout_epll);

#ifdef CONFIG_SND_SOC_SAMSUNG_B2_USE_CLK_FW
	xusbxti = clk_get(NULL, "xusbxti");
	if (IS_ERR(xusbxti)) {
		pr_err("%s: Can't get xusbxti clk %ld\n",
					__func__, PTR_ERR(xusbxti));
		return;
	}

	clkout = clk_get(NULL, "clk_out");
	if (IS_ERR(clkout)) {
		pr_err("%s: Can't get clk_out %ld\n",
					__func__, PTR_ERR(clkout));
		return;
	}

	clk_set_parent(clkout, xusbxti);
	clk_put(clkout);
	clk_put(xusbxti);
#endif
}

void __init exynos3_b2_audio_init(void)
{
	int ret;

	/* Codec LDO GPIO setup */
	exynos3_b2_audio_gpio_init();

	/* audio clock setup */
	exynos3_b2_audio_clock_setup();

	/* SPI1 setup */
#ifdef CONFIG_S3C64XX_DEV_SPI1
	exynos_spi_clock_setup(&s3c64xx_device_spi1.dev, 1);
	ret = exynos_spi_cfg_cs(spi1_csi[0].line, 1);
	if (!ret) {
		s3c64xx_spi1_set_platdata(&s3c64xx_spi1_pdata,
				EXYNOS_SPI_SRCCLK_SCLK, ARRAY_SIZE(spi1_csi));
		spi_register_board_info(spi1_board_info,
				ARRAY_SIZE(spi1_board_info));
	} else {
		pr_err(KERN_ERR "Error requesting gpio for SPI-CH1 CS\n");
	}
#endif

	platform_add_devices(b2_audio_devices,
			ARRAY_SIZE(b2_audio_devices));
}
