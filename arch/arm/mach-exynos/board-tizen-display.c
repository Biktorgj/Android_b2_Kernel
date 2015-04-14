/*
 *  linux/arch/arm/mach-exynos/board-tizen-display.c
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS - DISPLAY setting in set board
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/platform_device.h>
#include <plat/devs.h>
#include <linux/lcd-property.h>
#ifdef CONFIG_DRM_EXYNOS_FIMD
#include <linux/fb.h>
#include <plat/regs-fb.h>
#include <drm/exynos_drm.h>
#endif
#ifdef CONFIG_EXYNOS_MIPI_DSI
#include <plat/fb.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/lcd.h>
#include <video/exynos_mipi_dsim.h>
#endif
#include <linux/dma-mapping.h>
#include <mach/map.h>
#ifdef CONFIG_EXYNOS_SMIES
#include <plat/smies.h>
#endif

#define TIZEN3250_HBP		1
#define TIZEN3250_HFP		1
#define TIZEN3250_VBP		1
#define TIZEN3250_VFP		500
#define TIZEN3250_HSP		1
#define TIZEN3250_VSW		1
#define TIZEN3250_XRES		320
#define TIZEN3250_YRES		320
#define TIZEN3250_VIRTUAL_X	320
#define TIZEN3250_VIRTUAL_Y	(320 * 2)
#define TIZEN3250_WIDTH		60
#define TIZEN3250_HEIGHT	60
#define TIZEN3250_MAX_BPP	32
#define TIZEN3250_DEFAULT_BPP	24

#ifdef CONFIG_DRM_EXYNOS
static struct resource exynos_drm_resource[] = {
	[0] = {
		.start = IRQ_FIMD0_SYSTEM,
		.end   = IRQ_FIMD0_SYSTEM,
		.flags = IORESOURCE_IRQ,
	},
};

struct platform_device exynos_drm_device = {
	.name	= "exynos-drm",
	.id	= -1,
	.num_resources	  = ARRAY_SIZE(exynos_drm_resource),
	.resource	  = exynos_drm_resource,
	.dev	= {
		.dma_mask = &exynos_drm_device.dev.coherent_dma_mask,
		.coherent_dma_mask = 0xffffffffUL,
	}
};
#endif

#ifdef CONFIG_EXYNOS_MIPI_DSI
int s5p_dsim_phy_enable(struct platform_device *pdev, bool on);

static void reset_lcd_low(void)
{
	gpio_request_one(EXYNOS3_GPE0(1), GPIOF_OUT_INIT_LOW, "GPE0");
	gpio_set_value(EXYNOS3_GPE0(1), 0);
	usleep_range(1000, 1000);
	gpio_free(EXYNOS3_GPE0(1));
	usleep_range(6000, 6000);
}

static int reset_lcd(struct lcd_device *ld)
{
	/* Reset */
	gpio_request_one(EXYNOS3_GPE0(1), GPIOF_OUT_INIT_HIGH, "GPE0");
	gpio_set_value(EXYNOS3_GPE0(1), 0);
	usleep_range(1000, 1000);
	gpio_set_value(EXYNOS3_GPE0(1), 1);
	gpio_free(EXYNOS3_GPE0(1));
	usleep_range(6000, 6000);

	gpio_set_value(EXYNOS3_GPE0(1), 0);
	usleep_range(1000, 1000);
	gpio_set_value(EXYNOS3_GPE0(1), 1);
	gpio_free(EXYNOS3_GPE0(1));
	usleep_range(6000, 6000);
	dev_info(&ld->dev, "reset completed.\n");

	return 0;
}

static struct lcd_property s6e63j0x03_property = {
	.reset_low = reset_lcd_low,
#ifdef CONFIG_LCD_ESD
	.det_gpio = EXYNOS3_GPX1(7),
#endif
};

static struct lcd_platform_data s6e63j0x03_pdata = {
	.reset			= reset_lcd,
	.reset_delay		= 5,
	.power_off_delay		= 130,
	.power_on_delay		= 25,
	.lcd_enabled		= 1,
	.pdata	= &s6e63j0x03_property,
};

/*
 * ===========================================
 * |    P    |    M    |    S    |    MHz    |
 * -------------------------------------------
 * |    3    |   100   |    3    |    100    |
 * |    3    |   100   |    2    |    200    |
 * |    3    |    63   |    1    |    252    |
 * |    4    |   100   |    1    |    300    |
 * |    4    |   110   |    1    |    330    |
 * |   12    |   350   |    1    |    350    |
 * |    3    |   100   |    1    |    400    |
 * |    4    |   150   |    1    |    450    |
 * |    3    |   120   |    1    |    480    |
 * |   12    |   250   |    0    |    500    |
 * |    4    |   100   |    0    |    600    |
 * |    3    |    81   |    0    |    648    |
 * |    3    |    88   |    0    |    704    |
 * |    3    |    90   |    0    |    720    |
 * |    3    |   100   |    0    |    800    |
 * |   12    |   425   |    0    |    850    |
 * |    4    |   150   |    0    |    900    |
 * |   12    |   475   |    0    |    950    |
 * |    6    |   250   |    0    |   1000    |
 * -------------------------------------------
 */

static struct mipi_dsim_config dsim_config = {
	.e_interface		= DSIM_COMMAND,
	.e_pixel_format		= DSIM_24BPP_888,
	/* main frame fifo auto flush at VSYNC pulse */
	.auto_flush		= false,
	.eot_disable		= false,
	.auto_vertical_cnt	= false,

	.hse = false,
	.hfp = false,
	.hbp = false,
	.hsa = false,

	.e_no_data_lane = DSIM_DATA_LANE_1,
	.e_byte_clk	= DSIM_PLL_OUT_DIV8,
	.e_burst_mode	= DSIM_BURST,

	.p = 3,
	.m = 125,
	.s = 1,

	.esc_clk	= 20 * 1000000, /* escape clk : 20MHz */

	/* stop state holding counter after bta change count 0 ~ 0xfff */
	.stop_holding_cnt	= 0xfff,
	.bta_timeout		= 0xff,		/* bta timeout 0 ~ 0xff */
	.rx_timeout		= 0xffff,	/* lp rx timeout 0 ~ 0xffff */
	.pll_stable_time	= 500,
};

static struct mipi_dsim_platform_data dsim_platform_data = {
	/* already enabled at boot loader. FIXME!!! */
	.enabled		= true,
	.phy_enable		= s5p_dsim_phy_enable,
	.dsim_config		= &dsim_config,
	.src_pdev = &s5p_device_fimd0,
};

static struct mipi_dsim_lcd_device mipi_lcd_device = {
	.name			= "s6e63j0x03",
	.id			= -1,
	.bus_id			= 0,
	.platform_data		= (void *)&s6e63j0x03_pdata,
};
#endif

#ifdef CONFIG_DRM_EXYNOS_FIMD
static struct exynos_drm_mode_info drm_fimd_modes[] = {
	[0] = {
		.refresh = 30,
		.vmode = FB_VMODE_NONINTERLACED,
	},
};

static struct exynos_drm_fimd_pdata drm_fimd_pdata = {
	.panel	= {
		.timing	= {
			.xres		= TIZEN3250_XRES,
			.yres		= TIZEN3250_YRES,
			.hsync_len	= TIZEN3250_HSP,
			.left_margin	= TIZEN3250_HBP,
			.right_margin	= TIZEN3250_HFP,
			.vsync_len	= TIZEN3250_VSW,
			.upper_margin	= TIZEN3250_VBP,
			.lower_margin	= TIZEN3250_VFP,
			.refresh	= 30,
			.cs_setup_time = 0,
			.wr_setup_time = 0,
			.wr_act_time = 1,
			.wr_hold_time = 0,
			.rs_pol = 0,
			.i80en = 1,
		},
		.self_refresh	= 60,
		.width_mm	= 29,
		.height_mm	= 29,
		.mode_count = ARRAY_SIZE(drm_fimd_modes),
		.mode = drm_fimd_modes,
	},

	/*
	 * Enable DSI_EN if this board has MIPI and i80 interface
	 * based lcd panel.
	 */
	.vidcon0		= VIDCON0_PNRMODE_RGB | VIDCON0_VIDOUT_I80_LDI0 |
		VIDCON0_DSI_EN | VIDCON0_CLKVALUP,
	.default_win		= 3,
	.bpp			= 32,
	.disp_bus_pdev = &s5p_device_mipi_dsim0,
	.te_gpio = EXYNOS3_GPX0(6),
	.max_refresh = 60,
	.min_refresh = 30,
#ifdef CONFIG_EXYNOS_SMIES
	.smies_on	= smies_enable_by_fimd,
	.smies_off	= smies_disable_by_fimd,
	.smies_mode	= smies_runtime_mode,
	.smies_device	= &s5p_device_smies.dev,
#endif
};
#endif

#ifdef CONFIG_EXYNOS_SMIES
static struct s5p_smies_platdata rinato_smies_pdata __initdata = {
	.sae_on			= 0,
	.scr_on			= 0,
#ifdef CONFIG_EXYNOS_SMIES_DEFALUT_ENABLE
	.gamma_on		= 0,
#else
	.gamma_on		= 1,
#endif
	.dither_on		= 0,
	.sae_skin_check		= 0,
	.sae_gain		= 0,
	.scr_r			= 0xff0000,
	.scr_g			= 0x00ff00,
	.scr_b			= 0x0000ff,
	.scr_c			= 0x00ffff,
	.scr_m			= 0xff00ff,
	.scr_y			= 0xffff00,
	.scr_white		= 0xffffff,
	.scr_black		= 0x000000,
	.width			= TIZEN3250_XRES,
	.height			= TIZEN3250_YRES,
};
#endif

#ifdef CONFIG_DRM_EXYNOS_VIDI
struct platform_device exynos_drm_vidi_device = {
	.name	= "exynos-drm-vidi",
};
#endif

#ifdef CONFIG_DRM_EXYNOS_IPP
struct platform_device exynos_drm_ipp_device = {
	.name	= "exynos-drm-ipp",
};
#endif

#ifdef CONFIG_DRM_EXYNOS_GSC
static u64 exynos_gsc_dma_mask = DMA_BIT_MASK(32);

static struct resource exynos3_gsc0_resource[] = {
	[0] = {
		.start	= EXYNOS3_PA_GSC0,
		.end	= EXYNOS3_PA_GSC0 + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_GSC0,
		.end	= IRQ_GSC0,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device exynos3_device_gsc0 = {
	.name		= "exynos-gsc",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(exynos3_gsc0_resource),
	.resource	= exynos3_gsc0_resource,
	.dev		= {
		.dma_mask		= &exynos_gsc_dma_mask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	},
};

static struct resource exynos3_gsc1_resource[] = {
	[0] = {
		.start	= EXYNOS3_PA_GSC1,
		.end	= EXYNOS3_PA_GSC1 + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_GSC1,
		.end	= IRQ_GSC1,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device exynos3_device_gsc1 = {
	.name		= "exynos-gsc",
	.id		= 1,
	.num_resources	= ARRAY_SIZE(exynos3_gsc1_resource),
	.resource	= exynos3_gsc1_resource,
	.dev		= {
		.dma_mask		= &exynos_gsc_dma_mask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	},
};
#endif

#ifdef CONFIG_DRM_EXYNOS_SC
static struct resource exynos5_scaler0_resource[] = {
	[0] = DEFINE_RES_MEM(EXYNOS5_PA_MSCL0, SZ_4K),
	[1] = DEFINE_RES_IRQ(EXYNOS5_IRQ_MSCL0),
};

struct platform_device exynos5_device_scaler0 = {
	.name		= "exynos5-scaler",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(exynos5_scaler0_resource),
	.resource	= exynos5_scaler0_resource,
};
#endif

#ifdef CONFIG_DRM_EXYNOS_DBG
struct platform_device exynos_drm_dbg_device = {
	.name	= "exynos-drm-dbg",
};
#endif

static struct platform_device *display_devices[] __initdata = {
#ifdef CONFIG_DRM_EXYNOS
	&exynos_drm_device,
#endif
#ifdef CONFIG_EXYNOS_MIPI_DSI
	&s5p_device_mipi_dsim0,
#endif
#ifdef CONFIG_DRM_EXYNOS_FIMD
	&s5p_device_fimd0,
#endif
#ifdef CONFIG_DRM_EXYNOS_VIDI
	&exynos_drm_vidi_device,
#endif
#ifdef CONFIG_DRM_EXYNOS_IPP
	&exynos_drm_ipp_device,
#endif
#ifdef CONFIG_DRM_EXYNOS_GSC
	&exynos3_device_gsc0,
	&exynos3_device_gsc1,
#endif
#ifdef CONFIG_DRM_EXYNOS_SC
	&exynos5_device_scaler0,
#endif
#ifdef CONFIG_EXYNOS_SMIES
	&s5p_device_smies,
#endif
#ifdef CONFIG_DRM_EXYNOS_DBG
	&exynos_drm_dbg_device,
#endif
};

void __init tizen_display_init(void)
{
	struct mipi_dsim_platform_data *dsim_pd;

	printk(KERN_INFO "%s\n", __func__);

#ifdef CONFIG_EXYNOS_MIPI_DSI
	s5p_device_mipi_dsim0.name = "exynos3-mipi";
	s5p_device_mipi_dsim0.dev.platform_data = (void *)&dsim_platform_data;

	dsim_pd = (struct mipi_dsim_platform_data *)&dsim_platform_data;
	strcpy(dsim_pd->lcd_panel_name, "s6e63j0x03");
	dsim_pd->lcd_panel_info = (void *)&drm_fimd_pdata.panel.timing;
	exynos_mipi_dsi_register_lcd_device(&mipi_lcd_device);
#endif
#ifdef CONFIG_DRM_EXYNOS_FIMD
	s5p_device_fimd0.name = "exynos3-fb";
	s5p_device_fimd0.dev.platform_data = &drm_fimd_pdata;
	exynos4_fimd_setup_clock(&s5p_device_fimd0.dev, "sclk_fimd", \
				"sclk_lcd_blk", 50 * MHZ);
#endif
#ifdef CONFIG_EXYNOS_SMIES
	s5p_smies_set_platdata(&rinato_smies_pdata);
#endif

	platform_add_devices(display_devices, ARRAY_SIZE(display_devices));
}
