/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		 http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/platform_device.h>
#include <linux/usb/slp_multi.h>

#include <plat/devs.h>
#include <plat/udc-hs.h>
#include <mach/regs-usb-phy.h>

#ifdef CONFIG_USB_G_SLP
static const char *tizen_multi_funcs[] = {
	[0] = "mtp",
	[1] = "acm",
	[2] = "sdb",
	[3] = "diag",
};

static struct slp_multi_platform_data tizen_multi_pdata = {
	.nluns = 2,
	.enable_funcs = tizen_multi_funcs,
	.nfuncs = ARRAY_SIZE(tizen_multi_funcs),
};

static struct platform_device tizen_usb_multi = {
	.name = "slp_multi",
	.id = -1,
	.dev = {
		.platform_data = &tizen_multi_pdata,
	},
};
#endif

static struct s3c_hsotg_plat universal3250_hsotg_pdata __initdata;

static void __init universal3250_hsotg_init(void)
{
#if defined(CONFIG_MACH_B2)
	/* sqrxtune [13:11] 3b110 : -15%
	 * txvreftune [3:0] 4b1001 : +12%
	 */
	universal3250_hsotg_pdata.phy_tune_mask =
		OTG_TUNE_SQRXTUNE(0x7) | OTG_TUNE_TXVREFTUNE(0xf);
	universal3250_hsotg_pdata.phy_tune =
		OTG_TUNE_SQRXTUNE(0x6) | OTG_TUNE_TXVREFTUNE(0x9);
#endif
	s3c_hsotg_set_platdata(&universal3250_hsotg_pdata);
}

static struct platform_device *universal3250_usb_devices[] __initdata = {
	&s3c_device_usb_hsotg,
#ifdef CONFIG_USB_G_SLP
	&tizen_usb_multi,
#endif
};

void __init exynos3_universal3250_usb_init(void)
{
	universal3250_hsotg_init();

	platform_add_devices(universal3250_usb_devices,
			ARRAY_SIZE(universal3250_usb_devices));
}
