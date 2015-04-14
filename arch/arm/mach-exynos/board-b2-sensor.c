#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/delay.h>
#include <plat/gpio-cfg.h>
#include <plat/iic.h>
#include <plat/devs.h>
#include <mach/hs-iic.h>
#include <mach/regs-gpio.h>
#include <mach/gpio.h>
#include <asm/system_info.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#include <linux/spi/spi.h>
#include <plat/s3c64xx-spi.h>
#include <mach/spi-clocks.h>
#include <linux/regulator/consumer.h>

#include "board-universal3250.h"

#if defined(CONFIG_SENSORS_SSP)
#include <linux/ssp_platformdata.h>
#endif

#define ss_info(str, args...)\
	pr_info("[SSP:%s] " str, __func__, ##args)
#define ss_err(str, args...)\
	pr_err("[SSP:%s] " str, __func__, ##args)

#ifdef CONFIG_SENSORS_SSP_MAX86900
static int hrm_sensor_power(int on);
#endif

#if defined(CONFIG_SENSORS_SSP_STM)
#define	GPIO_SHUB_SPI_SCK	EXYNOS3_GPB(0)
#define	GPIO_SHUB_SPI_SSN	EXYNOS3_GPB(1)
#define	GPIO_SHUB_SPI_MISO	EXYNOS3_GPB(2)
#define	GPIO_SHUB_SPI_MOSI	EXYNOS3_GPB(3)
#endif

#if defined(CONFIG_SENSORS_SSP)
#define GPIO_AP_MCU_INT		EXYNOS3_GPX0(0)
#define GPIO_MCU_AP_INT_1	EXYNOS3_GPX0(2)
#define GPIO_MCU_AP_INT_2	EXYNOS3_GPX0(4)
#define GPIO_MCU_NRST		EXYNOS3_GPX0(5)

u8 ssp_magnetic_pdc[] = {110, 85, 171, 71, 203, 195, 0, 67,\
			208, 56, 175, 244, 206, 213, 0, 92, 250, 0,\
			55, 48, 189, 252, 171, 243, 13, 45, 250};

static int set_mcu_reset(int on);
static int check_ap_rev(void);
static int ssp_check_lpmode(void);
static void ssp_get_positions(int *acc, int *mag);
extern unsigned int lpcharge;
#endif

#if defined(CONFIG_SENSORS_SSP)
static struct ssp_platform_data ssp_pdata = {
	.set_mcu_reset = set_mcu_reset,
	.check_ap_rev = check_ap_rev,
	.check_lpmode = ssp_check_lpmode,
	.get_positions = ssp_get_positions,
	.mag_matrix_size = ARRAY_SIZE(ssp_magnetic_pdc),
	.mag_matrix = ssp_magnetic_pdc,
	.irq = IRQ_EINT(2),
#if defined(CONFIG_SENSORS_SSP_STM)
	.rst = GPIO_MCU_NRST,
	.ap_int = GPIO_AP_MCU_INT,
	.mcu_int1 = GPIO_MCU_AP_INT_1,
	.mcu_int2 = GPIO_MCU_AP_INT_2,
#endif
};
#endif

#if defined(CONFIG_SENSORS_SSP)
static int initialize_ssp_gpio(void)
{
	int ret;

	ss_info("is called\n");

	ret = gpio_request(GPIO_AP_MCU_INT, "AP_MCU_INT_PIN");
	if (ret)
		pr_err("%s, failed to request AP_MCU_INT for SSP\n", __func__);

	s3c_gpio_cfgpin(GPIO_AP_MCU_INT, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_AP_MCU_INT, S3C_GPIO_PULL_NONE);
	gpio_direction_output(GPIO_AP_MCU_INT, 1);
	gpio_free(GPIO_AP_MCU_INT);

	ret = gpio_request(GPIO_MCU_AP_INT_2, "MCU_AP_INT_PIN2");
	if (ret)
		pr_err("%s, failed to request MCU_AP_INT2 for SSP\n", __func__);

	s3c_gpio_cfgpin(GPIO_MCU_AP_INT_2, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_MCU_AP_INT_2, S3C_GPIO_PULL_NONE);
	gpio_free(GPIO_MCU_AP_INT_2);

	ret = gpio_request(GPIO_MCU_NRST, "AP_MCU_RESET");
	if (ret)
		pr_err("%s, failed to request AP_MCU_RESET for SSP\n",
			__func__);

	s3c_gpio_cfgpin(GPIO_MCU_NRST, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_MCU_NRST, S3C_GPIO_PULL_NONE);
	gpio_direction_output(GPIO_MCU_NRST, 0);
//	gpio_set_value(GPIO_MCU_NRST, 0);
	msleep(200);
	gpio_set_value(GPIO_MCU_NRST, 1);
//	msleep(50);
	gpio_free(GPIO_MCU_NRST);

	ret = gpio_request(GPIO_MCU_AP_INT_1, "MCU_AP_INT_PIN");
	if (ret)
		pr_err("%s, failed to request MCU_AP_INT for SSP\n", __func__);
	s3c_gpio_cfgpin(GPIO_MCU_AP_INT_1, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_MCU_AP_INT_1, S3C_GPIO_PULL_NONE);
	gpio_free(GPIO_MCU_AP_INT_1);
	
	return ret;
}

static int set_mcu_reset(int on)
{
	if (on == 0)
		gpio_set_value(GPIO_MCU_NRST, 0);
	else
		gpio_set_value(GPIO_MCU_NRST, 1);

	return 0;
}

static int check_ap_rev(void)
{
	return system_rev;
}

static int ssp_check_lpmode(void)
{
	if (lpcharge == 0x01)
		return true;
	else
		return false;
}

/********************************************************
 * Sensors
 * top/upper-left => top/upper-right ... ... =>	bottom/lower-left
*/
static void ssp_get_positions(int *acc, int *mag)
{
	*acc = 7;
	*mag = 0;

	ss_info("position acc : %d, mag = %d\n", *acc, *mag);
}
#endif

#if defined(CONFIG_SENSORS_SSP_STM)
int initialize_ssp_spi_gpio(struct platform_device *dev)
{
	int gpio;

	s3c_gpio_cfgpin(GPIO_SHUB_SPI_SCK, S3C_GPIO_SFN(2));
	s3c_gpio_setpull(GPIO_SHUB_SPI_SCK, S3C_GPIO_PULL_UP);

	s3c_gpio_cfgpin(GPIO_SHUB_SPI_SSN, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_SHUB_SPI_SSN, S3C_GPIO_PULL_UP);

	s3c_gpio_cfgpin(GPIO_SHUB_SPI_MISO, S3C_GPIO_SFN(2));
	s3c_gpio_setpull(GPIO_SHUB_SPI_MISO, S3C_GPIO_PULL_DOWN);
	s3c_gpio_cfgpin(GPIO_SHUB_SPI_MOSI, S3C_GPIO_SFN(2));
	s3c_gpio_setpull(GPIO_SHUB_SPI_MOSI, S3C_GPIO_PULL_NONE);

	for (gpio = GPIO_SHUB_SPI_SCK;
		gpio <= GPIO_SHUB_SPI_MOSI; gpio++)
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV3);

	return 0;
}

void __init ssp_spi0_set_platdata(struct s3c64xx_spi_info *pd,
				      int src_clk_nr, int num_cs)
{
	if (!pd) {
		ss_err("Need to pass platform data\n");
		return;
	}

	/* Reject invalid configuration */
	if (!num_cs || src_clk_nr < 0) {
		ss_err("Invalid SPI configuration\n");
		return;
	}

	pd->num_cs = num_cs;
	pd->src_clk_nr = src_clk_nr;
	if (!pd->cfg_gpio)
		pd->cfg_gpio = initialize_ssp_spi_gpio;

	if (pd->dma_mode != PIO_MODE)
		pd->dma_mode = HYBRID_MODE;

	s3c_set_platdata(pd, sizeof(*pd), &s3c64xx_device_spi0);
}

static struct s3c64xx_spi_csinfo spi0_csi[] = {
	[0] = {
		.line		= GPIO_SHUB_SPI_SSN,
		.set_level	= gpio_set_value,
		.fb_delay	= 0x0,
		.cs_mode	= AUTO_CS_MODE,
	},
};

static struct spi_board_info spi0_board_info[] __initdata = {
	{
		.modalias			= "ssp-spi",
		.max_speed_hz	= 5 * MHZ,
		.bus_num		= 0,
		.chip_select		= 0,
		.mode			= SPI_MODE_0,
		.irq				= IRQ_EINT(0),
		.controller_data	= &spi0_csi[0],
		.platform_data	= &ssp_pdata,
	}
};
#endif

void __init exynos3_b2_sensor_init(void)
{
#if defined(CONFIG_SENSORS_SSP) || defined(CONFIG_SENSORS_SSP_STM)
	int ret = 0;
#endif

	ss_info("is called\n");

#if defined(CONFIG_SENSORS_SSP)
	ret = initialize_ssp_gpio();
	if (ret < 0)
		ss_err("initialize_ssp_gpio fail(err=%d)\n", ret);
#endif

#if defined(CONFIG_SENSORS_SSP_STM)
	ss_info("SSP_SPI_SETUP\n");

	exynos_spi_clock_setup(&s3c64xx_device_spi0.dev, 0);

	if (!exynos_spi_cfg_cs(spi0_csi[0].line, 0)) {
		ss_info("spi0_set_platdata ...\n");
		#if 0
		s3c64xx_spi0_set_platdata(&s3c64xx_spi0_pdata,
			EXYNOS_SPI_SRCCLK_SCLK, ARRAY_SIZE(spi0_csi));
		#else
		ssp_spi0_set_platdata(&s3c64xx_spi0_pdata,
			EXYNOS_SPI_SRCCLK_SCLK, ARRAY_SIZE(spi0_csi));
		#endif
		spi_register_board_info(spi0_board_info,
			ARRAY_SIZE(spi0_board_info));
	} else {
		ss_err("Error requesting gpio for SPI-CH%d CS",
			spi0_board_info->bus_num);
	}

	ret = platform_device_register(&s3c64xx_device_spi0);
	if (ret < 0)
		ss_err("Failed to register spi0 plaform devices(err=%d)\n", ret);
#endif
}

