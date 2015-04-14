/*
 * SAMSUNG UNIVERSAL3250 machine file
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/platform_device.h>
#include <linux/serial_core.h>
#include <linux/io.h>
#include <linux/memblock.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/i2c.h>
#include <linux/cma.h>
#include <linux/ion.h>
#include <asm/mach/arch.h>
#include <asm/hardware/gic.h>
#include <asm/mach-types.h>
#include <asm/setup.h>
#include <mach/map.h>
#include <mach/memory.h>

#include <plat/cpu.h>
#include <plat/devs.h>
#include <plat/regs-serial.h>
#include <plat/clock.h>
#include <plat/iic.h>
#include <plat/adc.h>
#include <plat/watchdog.h>
#include <mach/regs-pmu.h>
#include <mach/gpio.h>
#include <mach/pmu.h>
#include <plat/gpio-cfg.h>

#include "common.h"
#include "board-universal3250.h"
#ifdef CONFIG_BT_BCM4334W
#include "include/board-bluetooth-bcm.h"

/*rfkill device registeration*/
static struct platform_device bcm4334w_bluetooth_device = {
	.name = "bcm4334w_bluetooth",
	.id = -1,
};
#endif

static struct platform_device ramconsole_device = {
	.name	= "ram_console",
	.id	= -1,
};
#define UNIVERSAL3250_UCON_DEFAULT	(S3C2410_UCON_TXILEVEL |	\
				 S3C2410_UCON_RXILEVEL |	\
				 S3C2410_UCON_TXIRQMODE |	\
				 S3C2410_UCON_RXIRQMODE |	\
				 S3C2410_UCON_RXFIFO_TOI |	\
				 S3C2443_UCON_RXERR_IRQEN)

#define UNIVERSAL3250_ULCON_DEFAULT	S3C2410_LCON_CS8

#define UNIVERSAL3250_UFCON_DEFAULT	(S3C2410_UFCON_FIFOMODE |	\
				 S5PV210_UFCON_TXTRIG4 |	\
				 S5PV210_UFCON_RXTRIG4)

#ifdef CONFIG_INPUT_REGULATOR_HAPTIC
static struct platform_device regulator_haptic_device = {
	.name = "regulator-haptic",
	.id = 30,
};
#endif


static struct s3c2410_uartcfg universal3250_uartcfgs[] __initdata = {
	[0] = {
		.hwport		= 0,
		.flags		= 0,
		.ucon		= UNIVERSAL3250_UCON_DEFAULT,
		.ulcon		= UNIVERSAL3250_ULCON_DEFAULT,
		.ufcon		= UNIVERSAL3250_UFCON_DEFAULT,
#ifdef CONFIG_BT_BCM4334W
		.wake_peer	= bcm_bt_lpm_exit_lpm_locked,
#endif
	},
	[1] = {
		.hwport		= 1,
		.flags		= 0,
		.ucon		= UNIVERSAL3250_UCON_DEFAULT,
		.ulcon		= UNIVERSAL3250_ULCON_DEFAULT,
		.ufcon		= UNIVERSAL3250_UFCON_DEFAULT,
	},
	[2] = {
		.hwport		= 2,
		.flags		= 0,
		.ucon		= UNIVERSAL3250_UCON_DEFAULT,
		.ulcon		= UNIVERSAL3250_ULCON_DEFAULT,
		.ufcon		= UNIVERSAL3250_UFCON_DEFAULT,
	},
	[3] = {
		.hwport		= 3,
		.flags		= 0,
		.ucon		= UNIVERSAL3250_UCON_DEFAULT,
		.ulcon		= UNIVERSAL3250_ULCON_DEFAULT,
		.ufcon		= UNIVERSAL3250_UFCON_DEFAULT,
	},
};

static void __init universal3250_map_io(void)
{
	exynos_init_io(NULL, 0);
}

static inline void exynos_reserve_mem(void)
{
	
	exynos3_universal3250_gpio_init();

}

#ifdef CONFIG_SAMSUNG_DEV_ADC
static struct s3c_adc_platdata universal3250_adc_data __initdata = {
	.phy_init	= s3c_adc_phy_init,
	.phy_exit	= s3c_adc_phy_exit,
};
#endif

#ifdef CONFIG_S3C_DEV_WDT
/* WDT */
static struct s3c_watchdog_platdata smdk5410_watchdog_platform_data = {
	exynos_pmu_wdt_control,
	PMU_WDT_RESET_TYPE0,
};
#endif

static struct platform_device *universal3250_devices[] __initdata = {
#ifdef CONFIG_SAMSUNG_DEV_ADC
	&s3c_device_adc,
#endif
#ifdef CONFIG_S3C_DEV_WDT
	&s3c_device_wdt,
#endif
#ifdef CONFIG_MALI400
	&exynos4_device_g3d,
#endif

#ifdef CONFIG_BT_BCM4334W
	&bcm4334w_bluetooth_device,
#endif
#ifdef CONFIG_INPUT_REGULATOR_HAPTIC
	&regulator_haptic_device,
#endif
};

static void __init universal3250_machine_init(void)
{
#ifdef CONFIG_SAMSUNG_DEV_ADC
	s3c_adc_set_platdata(&universal3250_adc_data);
#endif
#ifdef CONFIG_S3C_DEV_WDT
	s3c_watchdog_set_platdata(&smdk5410_watchdog_platform_data);
#endif

	exynos3_universal3250_clock_init();
	exynos3_universal3250_mmc_init();
	exynos3_universal3250_power_init();
	exynos3_universal3250_battery_init();
	exynos3_universal3250_input_init();
	exynos3_b2_mfd_init();
	exynos3_universal3250_usb_init();
	exynos3_universal3250_display_init();
#ifdef CONFIG_SND_SOC_SAMSUNG_B2_YMU831
	exynos3_b2_audio_init();
#endif
	exynos3_b2_sensor_init();
#ifdef CONFIG_ICE4_FPGA
	exynos3_b2_fpga_init();
#endif

#ifdef CONFIG_DC_MOTOR
	exynos3_universal3250_vibrator_init();
#endif
	exynos3_universal3250_media_init();
	exynos3_universal3250_camera_init();
	exynos3_b2_thermistor_init();
	platform_add_devices(universal3250_devices, ARRAY_SIZE(universal3250_devices));
}

static void __init universal3250_init_early(void)
{
	s3c24xx_init_clocks(24000000);
	s3c24xx_init_uarts(universal3250_uartcfgs, ARRAY_SIZE(universal3250_uartcfgs));
}
MACHINE_START(B2, "B2")
	.init_irq	= exynos3_init_irq,
	.init_early	= universal3250_init_early,
	.map_io		= universal3250_map_io,
	.handle_irq	= gic_handle_irq,
	.init_machine	= universal3250_machine_init,
	.timer		= &exynos4_timer,
	.restart	= exynos3_restart,
	.reserve	= exynos_reserve_mem,
MACHINE_END

