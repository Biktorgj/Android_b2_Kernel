/* linux/drivers/video/exynos/s6e63j0x03.c
 *
 * MIPI-DSI based S6E63J0X03 AMOLED lcd 1.63 inch panel driver.
 *
 * Inki Dae, <inki.dae@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/ctype.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/lcd.h>
#include <linux/fb.h>
#include <linux/backlight.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>
#include <linux/sysfs.h>
#include <linux/pm_runtime.h>
#include <linux/lcd-property.h>
#include <video/mipi_display.h>
#include <video/exynos_mipi_dsim.h>
#include <plat/gpio-cfg.h>

#include "s6e63j0x03_dimming.h"
#include "exynos_smies.h"

#define MIN_BRIGHTNESS		0
#define MAX_BRIGHTNESS		100
#define DEFAULT_BRIGHTNESS	80

#define POWER_IS_ON(pwr)	((pwr) == FB_BLANK_UNBLANK)
#define POWER_IS_OFF(pwr)	((pwr) == FB_BLANK_POWERDOWN)
#define POWER_IS_NRM(pwr)	((pwr) == FB_BLANK_NORMAL)

#define lcd_to_master(a)	(a->dsim_dev->master)
#define lcd_to_master_ops(a)	((lcd_to_master(a))->master_ops)

#define LDI_ID_REG		0xD3
#define LDI_MTP_SET3		0xD4
#define LDI_CASET_REG		0x2A
#define LDI_PASET_REG		0x2B
#define LDI_ID_LEN		6
#define LDI_MTP_SET3_LEN	1
#define MAX_MTP_CNT		27
#define GAMMA_CMD_CNT		28
#define ACL_CMD_CNT		66
#define MIN_ACL			0
#define MAX_ACL			3
#define MAX_GAMMA_CNT		11

#define CANDELA_10		10
#define CANDELA_30		30
#define CANDELA_60		60
#define CANDELA_90		90
#define CANDELA_120		120
#define CANDELA_150		150
#define CANDELA_200		200
#define CANDELA_240		240
#define CANDELA_300		300

#define REFRESH_30HZ		30
#define REFRESH_60HZ		60

#define PANEL_DBG_MSG(on, dev, fmt, ...)				\
{									\
	if (on)								\
		dev_err(dev, "%s: "fmt, __func__, ##__VA_ARGS__);	\
}

enum {
	DSIM_NONE_STATE,
	DSIM_RESUME_COMPLETE,
	DSIM_FRAME_DONE,
};

struct s6e63j0x03 {
	struct device			*dev;
	struct lcd_device		*ld;
	struct backlight_device		*bd;
	struct mipi_dsim_lcd_device	*dsim_dev;
	struct lcd_platform_data	*pd;
	struct lcd_property	*property;
	struct smart_dimming		*dimming;
	struct regulator		*vdd3;
	struct regulator		*vci;
	struct work_struct		det_work;
	struct mutex			lock;
	unsigned int			reset_gpio;
	unsigned int			det_gpio;
	unsigned int			esd_irq;
	unsigned int			power;
	unsigned int			gamma;
	unsigned int			acl;
	unsigned int			cur_acl;
	unsigned int			refresh;
	unsigned char			id[LDI_ID_LEN];
	unsigned char			elvss_hbm;
	unsigned char			default_hbm;
	unsigned char			*gamma_tbl[MAX_GAMMA_CNT];
	bool				hbm_on;
	bool				alpm_on;
	bool				lp_mode;
	bool				boot_power_on;
	bool				compensation_on;
};

#ifdef CONFIG_LCD_ESD
static struct class *esd_class;
static struct device *esd_dev;
#endif
static unsigned int dbg_mode;
static const unsigned char TEST_KEY_ON_1[] = {
	0xF0,
	0x5A, 0x5A
};

static const unsigned char TEST_KEY_ON_2[] = {
	0xF1,
	0x5A, 0x5A
};

static const unsigned char PORCH_ADJUSTMENT[] = {
	0xF2,
	0x1C, 0x28
};

static const unsigned char FRAME_FREQ_60HZ_SET[] = {
	0xB5,
	0x00, 0x01, 0x00
};

static const unsigned char FRAME_FREQ_30HZ_SET[] = {
	0xB5,
	0x00, 0x02, 0x00
};

static const unsigned char MEM_ADDR_SET_0[] = {
	0x2A,
	0x00, 0x14, 0x01, 0x53
};

static const unsigned char MEM_ADDR_SET_1[] = {
	0x2B,
	0x00, 0x00, 0x01, 0x3F
};

static const unsigned char LPTS_TIMMING_SET_60HZ_0[] = {
	0xF8,
	0x08, 0x08, 0x08, 0x17,
	0x00, 0x2A, 0x02, 0x26,
	0x00, 0x00, 0x02, 0x00,
	0x00
};

static const unsigned char LPTS_TIMMING_SET_30HZ_0[] = {
	0xF8,
	0x08, 0x08, 0x08, 0x17,
	0x00, 0x2A, 0x02, 0x13,
	0x00, 0x00, 0x02, 0x00,
	0x00
};

static const unsigned char LPTS_TIMMING_SET_1[] = {
	0xF7,
	0x02
};

static const unsigned char TE_RISING_EDGE[] = {
	0xE2,
	0x0F
};

static const unsigned char SLEEP_OUT[] = {
	0x11,
	0x00, 0x00
};

static const unsigned char ELVSS_COND[] = {
	0xB1,
	0x00, 0x09,
};

static const unsigned char ALPM_FRM_CON_15HZ[] = {
	0xD3,
	0x41
};

static const unsigned char ALPM_FRM_CON_30HZ[] = {
	0xD3,
	0x21
};

static const unsigned char WHITE_BRIGHTNESS_DEFAULT[] = {
	0x51,
	0xFF
};

static const unsigned char WHITE_CTRL_DEFAULT[] = {
	0x53,
	0x20
};

static const unsigned char WHITE_BRIGHTNESS_10_NIT[] = {
	0x51,
	0x26,
};

static const unsigned char WHITE_CTRL_10_NIT[] = {
	0x53,
	0x20
};

static const unsigned char WHITE_BRIGHTNESS_30_NIT[] = {
	0x51,
	0x13,
};

static const unsigned char WHITE_CTRL_30_NIT[] = {
	0x53,
	0x20
};

static const unsigned char HBM_WHITE_BRIGHTNESS[] = {
	0x51,
	0xEA
};

static const unsigned char HBM_WHITE_CTRL[] = {
	0x53,
	0x20
};

static const unsigned char ACL_40_PERCENT[] = {
	0x55,
	0x12
};

static const unsigned char ACL_OFF[] = {
	0x55,
	0x00
};

static const unsigned char TEST_KEY_OFF_2[] = {
	0xF1,
	0xA5, 0xA5
};

static const unsigned char TE_ON[] = {
	0x35,
	0x00, 0x00
};

static const unsigned char SET_POS[] = {
	0x36,
	0x40
};

static const unsigned char DISPLAY_ON[] = {
	0x29,
	0x00, 0x00
};

static const unsigned char DISPLAY_OFF[] = {
	0x28,
	0x00, 0x00
};

static const unsigned char SLEEP_IN[] = {
	0x10,
	0x00, 0x00
};

static const unsigned char PARTIAL_AREA_SET[] = {
	0x30,
	0x00, 0x00
};

static const unsigned char PARTIAL_MODE_ON[] = {
	0x12,
	0x00, 0x00
};

static const unsigned char IDLE_MODE_ON[] = {
	0x39,
	0x00, 0x00
};

static const unsigned char IDLE_MODE_OFF[] = {
	0x38,
	0x00, 0x00
};

static const unsigned char NORMAL_MODE_ON[] = {
	0x13,
	0x00, 0x00
};

static const unsigned char PARAM_POS_HBM_ELVSS[] = {
	0xb0,
	0x4c, 0x00
};

static const unsigned char PARAM_POS_DEF_ELVSS[] = {
	0xb0,
	0x20, 0x00,
};

static const unsigned char PARAM_POS_DEFAULT[] = {
	0xb0,
	0x00, 0x00,
};

static const unsigned char PARAM_POS_TE_EDGE[] = {
	0xB0,
	0x01, 0x00
};

static const unsigned char PARAM_POS_ALPM_FRM[] = {
	0xB0,
	0x21
};

static const unsigned char ACL_SEL_LOOK[] = {
	0xC0,
	0x13,
};

static const unsigned char ACL_ON[] = {
	0x55,
	0x10,
};

static const unsigned char ACL_GLOBAL_PARAM[] = {
	0xB0,
	0x1B,
};

static const unsigned char ACL_LUT_ENABLE[] = {
	0xD3,
	0x83,
};

static const unsigned char ACL_16[] = {
	0xC1,
	0x13, 0x13, 0x13, 0x13, 0x13,
	0x13, 0x13, 0x13, 0x13, 0x13,
	0x13, 0x13, 0x13, 0x13, 0x13,
	0x13, 0x13, 0x13, 0x13, 0x13,
	0x13, 0x13, 0x13, 0x13, 0x13,
	0x13, 0x13, 0x13, 0x13, 0x13,
	0x13, 0x13, 0x13, 0x13, 0x13,
	0x13, 0x13, 0x13, 0x14, 0x15,
	0x16, 0x17, 0x18, 0x19, 0x1A,
	0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
	0x20, 0x21, 0x22, 0x23, 0x24,
	0x25, 0x26, 0x27, 0x28, 0x29,
	0x2A, 0x2B, 0x2C, 0x2D, 0x2E
};

static const unsigned char ACL_21[] = {
	0xC1,
	0x13, 0x13, 0x13, 0x13, 0x13,
	0x13, 0x13, 0x13, 0x13, 0x13,
	0x13, 0x13, 0x13, 0x13, 0x13,
	0x13, 0x13, 0x13, 0x13, 0x13,
	0x13, 0x13, 0x13, 0x13, 0x13,
	0x14, 0x15, 0x16, 0x17, 0x18,
	0x19, 0x1A, 0x1B, 0x1C, 0x1D,
	0x1E, 0x1F, 0x20, 0x21, 0x22,
	0x23, 0x24, 0x25, 0x26, 0x27,
	0x28, 0x29, 0x2A, 0x2B, 0x2C,
	0x2D, 0x2E, 0x2F, 0x30, 0x31,
	0x32, 0x33, 0x34, 0x35, 0x36,
	0x37, 0x38, 0x39, 0x3A, 0x3B,
};

static const unsigned char ACL_25[] = {
	0xC1,
	0x13, 0x13, 0x13, 0x13, 0x13,
	0x13, 0x13, 0x13, 0x13, 0x13,
	0x13, 0x13, 0x13, 0x13, 0x14,
	0x15, 0x16, 0x17, 0x18, 0x19,
	0x1A, 0x1B, 0x1C, 0x1D, 0x1E,
	0x1F, 0x20, 0x21, 0x22, 0x23,
	0x24, 0x25, 0x26, 0x27, 0x28,
	0x29, 0x2A, 0x2B, 0x2C, 0x2D,
	0x2E, 0x2F, 0x30, 0x31, 0x32,
	0x33, 0x34, 0x35, 0x36, 0x37,
	0x38, 0x39, 0x3A, 0x3B, 0x3C,
	0x3D, 0x3E, 0x3F, 0x40, 0x41,
	0x42, 0x43, 0x44, 0x45, 0x46
};

static const unsigned char *acl_tbl[MAX_ACL] = {
	ACL_16,
	ACL_21,
	ACL_25,
};

static const short candela_tbl[MAX_GAMMA_CNT] = {
	CANDELA_10,
	CANDELA_30,
	CANDELA_60,
	CANDELA_90,
	CANDELA_120,
	CANDELA_150,
	CANDELA_200,
	CANDELA_200,
	CANDELA_240,
	CANDELA_300,
	CANDELA_300,
};

static struct regulator_bulk_data supplies[] = {
	{ .supply = "vcc_lcd_3.0", },
	{ .supply = "vcc_lcd_1.8", },
};

static void panel_regulator_enable(struct s6e63j0x03 *lcd)
{
	int ret = 0;
	struct lcd_platform_data *pd = NULL;

	pd = lcd->pd;
	PANEL_DBG_MSG(dbg_mode, lcd->dev, "S\n");

	mutex_lock(&lcd->lock);

	/* enable vdd3 (1.8v). */
	ret = regulator_enable(supplies[1].consumer);
	if (ret) {
		dev_err(lcd->dev, "failed to enable vdd3 regulator.\n");
		goto out;
	}

	/* enable vci (3.3v). */
	ret = regulator_enable(supplies[0].consumer);
	if (ret) {
		dev_err(lcd->dev, "failed to enable vci regulator.\n");
		ret = regulator_disable(supplies[1].consumer);
		goto out;
	}

	usleep_range(pd->power_on_delay * 1000, pd->power_on_delay * 1000);
out:
	mutex_unlock(&lcd->lock);
	PANEL_DBG_MSG(dbg_mode, lcd->dev, "E\n");
}

static void panel_regulator_disable(struct s6e63j0x03 *lcd)
{
	int ret = 0;

	PANEL_DBG_MSG(dbg_mode, lcd->dev, "S\n");

	mutex_lock(&lcd->lock);

	/* disable vci (3.3v). */
	ret = regulator_disable(supplies[0].consumer);
	if (ret) {
		dev_err(lcd->dev, "failed to disable vci regulator.\n");
		goto out;
	}

	/* disable vdd3 (1.8v). */
	ret = regulator_disable(supplies[1].consumer);
	if (ret) {
		dev_err(lcd->dev, "failed to disable vdd3 regulator.\n");
		ret = regulator_enable(supplies[0].consumer);
		goto out;
	}

out:
	mutex_unlock(&lcd->lock);

	PANEL_DBG_MSG(dbg_mode, lcd->dev, "E\n");
}

static int panel_enter_hbm_mode(struct s6e63j0x03 *lcd)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	struct mipi_dsim_device *master = lcd_to_master(lcd);
	u8 buf[3];

	buf[0] = LDI_ID_REG;
	buf[1] = lcd->elvss_hbm;
	buf[2] = 0;

	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TEST_KEY_ON_2, ARRAY_SIZE(TEST_KEY_ON_2));

	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			PARAM_POS_DEF_ELVSS, ARRAY_SIZE(PARAM_POS_DEF_ELVSS));

	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE, buf, ARRAY_SIZE(buf));

	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TEST_KEY_OFF_2, ARRAY_SIZE(TEST_KEY_OFF_2));

	ops->cmd_write(master, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			HBM_WHITE_BRIGHTNESS, ARRAY_SIZE(HBM_WHITE_BRIGHTNESS));
	ops->cmd_write(master, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			HBM_WHITE_CTRL, ARRAY_SIZE(HBM_WHITE_CTRL));

	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			PARAM_POS_DEFAULT, ARRAY_SIZE(PARAM_POS_DEFAULT));

	return 0;
}

static int panel_exit_hbm_mode(struct s6e63j0x03 *lcd)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	struct mipi_dsim_device *master = lcd_to_master(lcd);
	u8 buf[3];

	buf[0] = LDI_ID_REG;
	buf[1] = lcd->default_hbm;
	buf[2] = 0;

	ops->cmd_write(master, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			WHITE_BRIGHTNESS_DEFAULT,
			ARRAY_SIZE(WHITE_BRIGHTNESS_DEFAULT));
	ops->cmd_write(master, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			WHITE_CTRL_DEFAULT, ARRAY_SIZE(WHITE_CTRL_DEFAULT));

	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TEST_KEY_ON_2, ARRAY_SIZE(TEST_KEY_ON_2));

	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			PARAM_POS_DEF_ELVSS, ARRAY_SIZE(PARAM_POS_DEF_ELVSS));
	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE, buf, ARRAY_SIZE(buf));

	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TEST_KEY_OFF_2, ARRAY_SIZE(TEST_KEY_OFF_2));

	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			PARAM_POS_DEFAULT, ARRAY_SIZE(PARAM_POS_DEFAULT));

	return 0;
}

static int panel_get_power(struct lcd_device *ld)
{
	struct s6e63j0x03 *lcd = lcd_get_data(ld);

	return lcd->power;
}

static int panel_set_power(struct lcd_device *ld, int power)
{
	struct s6e63j0x03 *lcd = lcd_get_data(ld);
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	int ret = 0;

	if (power != FB_BLANK_UNBLANK && power != FB_BLANK_POWERDOWN &&
	    power != FB_BLANK_NORMAL) {
		dev_err(lcd->dev, "%s: power value should be 0, 1 or 4.\n",
			__func__);
		return -EINVAL;
	}

	if ((power == FB_BLANK_UNBLANK) && ops->set_blank_mode) {
		/* LCD power on */
		if ((POWER_IS_ON(power) && POWER_IS_OFF(lcd->power))
			|| (POWER_IS_ON(power) && POWER_IS_NRM(lcd->power))) {
			ret = ops->set_blank_mode(lcd_to_master(lcd), power);
			if (!ret && lcd->power != power)
				lcd->power = power;
		}
	} else if ((power == FB_BLANK_POWERDOWN) && ops->set_early_blank_mode) {
		/* LCD power off */
		if ((POWER_IS_OFF(power) && POWER_IS_ON(lcd->power)) ||
		(POWER_IS_ON(lcd->power) && POWER_IS_NRM(power))) {
			ret = ops->set_early_blank_mode(lcd_to_master(lcd),
							power);
			if (!ret && lcd->power != power)
				lcd->power = power;
		}
	}

	dev_info(lcd->dev, "%s: lcd power: mode[%d]\n", __func__, power);

	return ret;
}


static struct lcd_ops s6e63j0x03_lcd_ops = {
	.get_power = panel_get_power,
	.set_power = panel_set_power,
};

static void panel_compensate_brightness(struct s6e63j0x03 *lcd,
							unsigned int brightness)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	struct mipi_dsim_device *master = lcd_to_master(lcd);

	if (lcd->id[5] < 0x02)
		return;

	/* compensate for low brightness */
	switch (candela_tbl[brightness]) {
	case CANDELA_10:
		ops->cmd_write(master, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
					WHITE_BRIGHTNESS_10_NIT,
					ARRAY_SIZE(WHITE_BRIGHTNESS_10_NIT));
		ops->cmd_write(master, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
					WHITE_CTRL_10_NIT,
					ARRAY_SIZE(WHITE_CTRL_10_NIT));

		lcd->compensation_on = true;
		break;
	case CANDELA_30:
		ops->cmd_write(master, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
					WHITE_BRIGHTNESS_30_NIT,
					ARRAY_SIZE(WHITE_BRIGHTNESS_30_NIT));
		ops->cmd_write(master, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
					WHITE_CTRL_30_NIT,
					ARRAY_SIZE(WHITE_CTRL_30_NIT));

		lcd->compensation_on = true;
		break;
	default:
		if (lcd->compensation_on) {
			ops->cmd_write(master, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
					WHITE_BRIGHTNESS_DEFAULT,
					ARRAY_SIZE(WHITE_BRIGHTNESS_DEFAULT));
			ops->cmd_write(master, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
					WHITE_CTRL_DEFAULT,
					ARRAY_SIZE(WHITE_CTRL_DEFAULT));

			lcd->compensation_on = false;
		}
		break;
	}
}

static int panel_update_gamma(struct s6e63j0x03 *lcd, unsigned int brightness)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	struct mipi_dsim_device *master = lcd_to_master(lcd);

	panel_compensate_brightness(lcd, brightness);

	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TEST_KEY_ON_2, ARRAY_SIZE(TEST_KEY_ON_2));

	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			lcd->gamma_tbl[brightness], GAMMA_CMD_CNT);

	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TEST_KEY_OFF_2, ARRAY_SIZE(TEST_KEY_OFF_2));

	return 0;
}

static int panel_get_brightness_index(unsigned int brightness)
{

	switch (brightness) {
	case 0 ... 20:
		brightness = 0;
		break;
	case 21 ... 40:
		brightness = 2;
		break;
	case 41 ... 60:
		brightness = 4;
		break;
	case 61 ... 80:
		brightness = 6;
		break;
	default:
		brightness = 10;
		break;
	}

	return brightness;
}

static int panel_set_brightness(struct backlight_device *bd)
{
	int ret = 0;
	unsigned int brightness = bd->props.brightness;
	struct s6e63j0x03 *lcd = bl_get_data(bd);
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	struct mipi_dsim_device *master = lcd_to_master(lcd);

	PANEL_DBG_MSG(dbg_mode, lcd->dev, "S\n");

	if (brightness < MIN_BRIGHTNESS ||
		brightness > bd->props.max_brightness) {
		dev_err(lcd->dev, "lcd brightness should be %d to %d.\n",
			MIN_BRIGHTNESS, MAX_BRIGHTNESS);
		return -EINVAL;
	}

	if (lcd->power > FB_BLANK_UNBLANK) {
		dev_warn(lcd->dev, "enable lcd panel.\n");
		return -EPERM;
	}

	if (lcd->alpm_on)
		return 0;

	dev_info(lcd->dev, "brightness[%d]dpms[%d]hbm[%d]\n",
		brightness, atomic_read(&master->dpms_on), lcd->hbm_on);

	brightness = panel_get_brightness_index(brightness);

	if (atomic_read(&master->dpms_on)) {
		if (lcd->hbm_on) {
#ifdef CONFIG_EXYNOS_SMIES_DEFALUT_ENABLE
			ops->set_smies_mode(master, OUTDOOR_MODE);
#else
			ops->set_smies_active(master, lcd->hbm_on);
#endif
			ret = panel_enter_hbm_mode(lcd);
		} else
			ret = panel_update_gamma(lcd, brightness);
	} else {
		if (ops->set_runtime_active(master)) {
			dev_warn(lcd->dev,
				"failed to set_runtime_active:power[%d]\n",
				lcd->power);

			if (lcd->power > FB_BLANK_UNBLANK)
				return -EPERM;
		}

		ops->standby(master, 0, 1);
		ret = panel_update_gamma(lcd, brightness);
		ops->standby(master, 1, 1);
		ops->fimd_trigger(master);
	}

	if (ret) {
		dev_err(lcd->dev, "failed gamma setting.\n");
		return -EIO;
	}

	PANEL_DBG_MSG(dbg_mode, lcd->dev, "E\n");

	return ret;
}

static int panel_get_brightness(struct backlight_device *bd)
{
	return bd->props.brightness;
}

static const struct backlight_ops s6e63j0x03_backlight_ops = {
	.get_brightness = panel_get_brightness,
	.update_status = panel_set_brightness,
};

static void panel_power_on(struct mipi_dsim_lcd_device *dsim_dev, int power)
{
	struct s6e63j0x03 *lcd = dev_get_drvdata(&dsim_dev->dev);

	PANEL_DBG_MSG(dbg_mode, lcd->dev, "S(power = %d)\n", power);

	/* lcd power on */
	if (power) {
		if (lcd->lp_mode)
			return;
		panel_regulator_enable(lcd);

		/* Do not reset at booting time if enabled. */
		if (lcd->boot_power_on) {
			lcd->boot_power_on = false;
			return;
		}
		/* lcd reset */
		if (lcd->pd->reset)
			lcd->pd->reset(lcd->ld);
	} else {

	/* lcd reset low */
		if (lcd->property->reset_low)
			lcd->property->reset_low();

		panel_regulator_disable(lcd);
	}

	PANEL_DBG_MSG(dbg_mode, lcd->dev, "E(power = %d)\n", power);
}

static void panel_get_gamma_tbl(struct s6e63j0x03 *lcd,
						const unsigned char *data)
{
	int i;

	panel_read_gamma(lcd->dimming, data);

	panel_generate_volt_tbl(lcd->dimming);

	for (i = 0; i < MAX_GAMMA_CNT; i++) {
		lcd->gamma_tbl[i][0] = LDI_MTP_SET3;
		panel_get_gamma(lcd->dimming, candela_tbl[i],
							&lcd->gamma_tbl[i][1]);
	}
}

static int panel_check_mtp(struct mipi_dsim_lcd_device *dsim_dev)
{
	struct s6e63j0x03 *lcd = dev_get_drvdata(&dsim_dev->dev);
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	struct mipi_dsim_device *master = lcd_to_master(lcd);
	unsigned char mtp_data[MAX_MTP_CNT];

	/* ID */
	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TEST_KEY_ON_2, ARRAY_SIZE(TEST_KEY_ON_2));

	ops->cmd_read(master, MIPI_DSI_DCS_READ,
			LDI_ID_REG, LDI_ID_LEN, lcd->id);

	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TEST_KEY_OFF_2, ARRAY_SIZE(TEST_KEY_OFF_2));

	dev_info(lcd->dev, "Ver = %d.%d.%d\n", lcd->id[3], lcd->id[2],
						lcd->id[5]);

	/* HBM */
	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TEST_KEY_ON_2, ARRAY_SIZE(TEST_KEY_ON_2));

	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			PARAM_POS_HBM_ELVSS, ARRAY_SIZE(PARAM_POS_HBM_ELVSS));
	ops->cmd_read(master, MIPI_DSI_DCS_READ,
			LDI_MTP_SET3, LDI_MTP_SET3_LEN, &lcd->elvss_hbm);
	dev_info(lcd->dev, "HBM ELVSS: 0x%x\n", lcd->elvss_hbm);

	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			PARAM_POS_DEF_ELVSS, ARRAY_SIZE(PARAM_POS_DEF_ELVSS));
	ops->cmd_read(master, MIPI_DSI_DCS_READ,
			LDI_ID_REG, LDI_MTP_SET3_LEN, &lcd->default_hbm);
	dev_info(lcd->dev, "DEFAULT ELVSS: 0x%x\n", lcd->default_hbm);

	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			PARAM_POS_DEFAULT, ARRAY_SIZE(PARAM_POS_DEFAULT));

	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TEST_KEY_OFF_2, ARRAY_SIZE(TEST_KEY_OFF_2));

	/* GAMMA */
	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TEST_KEY_ON_2, ARRAY_SIZE(TEST_KEY_ON_2));

	ops->cmd_read(master, MIPI_DSI_DCS_READ, LDI_MTP_SET3,
			MAX_MTP_CNT, mtp_data);

	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TEST_KEY_OFF_2, ARRAY_SIZE(TEST_KEY_OFF_2));

	panel_get_gamma_tbl(lcd, mtp_data);

	return 0;
}

static int panel_set_refresh_rate(struct mipi_dsim_lcd_device *dsim_dev,
						int refresh)
{
	struct s6e63j0x03 *lcd = dev_get_drvdata(&dsim_dev->dev);

	PANEL_DBG_MSG(dbg_mode, lcd->dev, "S(enable = %d)\n", refresh);

	/* TODO: support various refresh rate */

	PANEL_DBG_MSG(dbg_mode, lcd->dev, "E(enable = %d)\n", refresh);

	return 0;
}

static int panel_set_win_update_region(struct mipi_dsim_lcd_device *dsim_dev,
						int offset_x, int offset_y,
						int width, int height)
{
	struct s6e63j0x03 *lcd = dev_get_drvdata(&dsim_dev->dev);
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	struct mipi_dsim_device *master = lcd_to_master(lcd);
	unsigned char buf[5];

	/* ToDo: need to check condition. */
	/*
		if (offset_y >= vm->vactive || height > vm->vactive)
			return -EINVAL;
	*/

	offset_x += 20;

	buf[0] = LDI_CASET_REG;
	buf[1] = (offset_x & 0xff00) >> 8;
	buf[2] = offset_x & 0x00ff;
	buf[3] = ((offset_x + width - 1) & 0xff00) >> 8;
	buf[4] = (offset_x + width - 1) & 0x00ff;

	ops->atomic_cmd_write(master, MIPI_DSI_DCS_LONG_WRITE, buf,
				ARRAY_SIZE(buf));

	buf[0] = LDI_PASET_REG;
	buf[1] = (offset_y & 0xff00) >> 8;
	buf[2] = offset_y & 0x00ff;
	buf[3] = ((offset_y + height - 1) & 0xff00) >> 8;
	buf[4] = (offset_y + height - 1) & 0x00ff;

	ops->atomic_cmd_write(master, MIPI_DSI_DCS_LONG_WRITE, buf,
				ARRAY_SIZE(buf));

	return 0;
}

static void panel_display_on(struct mipi_dsim_lcd_device *dsim_dev,
				unsigned int enable)
{
	struct s6e63j0x03 *lcd = dev_get_drvdata(&dsim_dev->dev);
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	struct mipi_dsim_device *master = lcd_to_master(lcd);

	if (lcd->lp_mode && enable) {
		lcd->lp_mode = false;
		return;
	}
	if (enable)
		ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
				DISPLAY_ON, ARRAY_SIZE(DISPLAY_ON));
	else
		ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
				DISPLAY_OFF, ARRAY_SIZE(DISPLAY_OFF));

	dev_info(lcd->dev, "%s[%d]\n", __func__, enable);
}

static void panel_sleep_in(struct s6e63j0x03 *lcd)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	struct mipi_dsim_device *master = lcd_to_master(lcd);

	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			SLEEP_IN, ARRAY_SIZE(SLEEP_IN));
}

static void panel_alpm_on(struct s6e63j0x03 *lcd, unsigned int enable)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	struct mipi_dsim_device *master = lcd_to_master(lcd);

	PANEL_DBG_MSG(dbg_mode, lcd->dev, "S\n");

	ops->wait_for_frame_done(master);

	if (enable) {
		ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
				PARTIAL_AREA_SET, ARRAY_SIZE(PARTIAL_AREA_SET));
		ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
				PARTIAL_MODE_ON, ARRAY_SIZE(PARTIAL_MODE_ON));
		ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
				IDLE_MODE_ON, ARRAY_SIZE(IDLE_MODE_ON));
		ops->update_panel_refresh(master, 30);

		dev_info(lcd->dev, "Entered into ALPM mode\n");
	} else {
		struct backlight_device *bd = lcd->bd;
		unsigned int brightness = bd->props.brightness;

		ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
				IDLE_MODE_OFF, ARRAY_SIZE(IDLE_MODE_OFF));
		ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
				NORMAL_MODE_ON, ARRAY_SIZE(NORMAL_MODE_ON));
		ops->update_panel_refresh(master, 60);

		brightness = panel_get_brightness_index(brightness);
		panel_update_gamma(lcd, brightness);

		dev_info(lcd->dev, "Exited from ALPM mode\n");
	}

	PANEL_DBG_MSG(dbg_mode, lcd->dev, "E\n");
}

static void panel_pm_check(struct mipi_dsim_lcd_device *dsim_dev,
						bool *pm_skip)
{
	struct s6e63j0x03 *lcd = dev_get_drvdata(&dsim_dev->dev);

	if (lcd->lp_mode) {
		if (lcd->alpm_on) {
			*pm_skip = true;
			lcd->power = FB_BLANK_UNBLANK;
		} else
			*pm_skip = false;
	} else
		*pm_skip = false;

	return;
}

static void panel_display_init(struct s6e63j0x03 *lcd)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	struct mipi_dsim_device *master = lcd_to_master(lcd);

	/* Test key enable */
	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TEST_KEY_ON_1, ARRAY_SIZE(TEST_KEY_ON_1));
	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TEST_KEY_ON_2, ARRAY_SIZE(TEST_KEY_ON_2));

	/* Panel condition setting */
	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			PORCH_ADJUSTMENT, ARRAY_SIZE(PORCH_ADJUSTMENT));

	if (lcd->refresh == REFRESH_30HZ)
		ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			FRAME_FREQ_30HZ_SET,
			ARRAY_SIZE(FRAME_FREQ_30HZ_SET));
	else
		ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			FRAME_FREQ_60HZ_SET,
			ARRAY_SIZE(FRAME_FREQ_60HZ_SET));

	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			MEM_ADDR_SET_0, ARRAY_SIZE(MEM_ADDR_SET_0));

	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			MEM_ADDR_SET_1, ARRAY_SIZE(MEM_ADDR_SET_1));

	if (lcd->refresh == REFRESH_30HZ)
		ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			LPTS_TIMMING_SET_30HZ_0,
			ARRAY_SIZE(LPTS_TIMMING_SET_30HZ_0));
	else
		ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			LPTS_TIMMING_SET_60HZ_0,
			ARRAY_SIZE(LPTS_TIMMING_SET_60HZ_0));

	ops->cmd_write(master, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			LPTS_TIMMING_SET_1, ARRAY_SIZE(LPTS_TIMMING_SET_1));

	/* sleep out */
	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			SLEEP_OUT, ARRAY_SIZE(SLEEP_OUT));

	usleep_range(120000, 120000);

	/* brightness setting */
	ops->cmd_write(master, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			WHITE_BRIGHTNESS_DEFAULT,
			ARRAY_SIZE(WHITE_BRIGHTNESS_DEFAULT));
	ops->cmd_write(master, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			WHITE_CTRL_DEFAULT,
			ARRAY_SIZE(WHITE_CTRL_DEFAULT));
	ops->cmd_write(master, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			ACL_OFF, ARRAY_SIZE(ACL_OFF));

	/* etc condition setting */
	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			ELVSS_COND, ARRAY_SIZE(ELVSS_COND));
	ops->cmd_write(master, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			SET_POS, ARRAY_SIZE(SET_POS));

	/* tearing effect on */
	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TE_ON, ARRAY_SIZE(TE_ON));

	/* Test key disable */
	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TEST_KEY_OFF_2, ARRAY_SIZE(TEST_KEY_OFF_2));
}

static void panel_set_sequence(struct mipi_dsim_lcd_device *dsim_dev)
{
	struct s6e63j0x03 *lcd = dev_get_drvdata(&dsim_dev->dev);

	PANEL_DBG_MSG(dbg_mode, lcd->dev, "S\n");

	if (lcd->lp_mode) {
		panel_alpm_on(lcd, lcd->alpm_on);
		lcd->power = FB_BLANK_UNBLANK;
		return;
	}

	panel_display_init(lcd);

	lcd->power = FB_BLANK_UNBLANK;
	panel_set_brightness(lcd->bd);

	PANEL_DBG_MSG(dbg_mode, lcd->dev, "E(refresh = %d)\n", lcd->refresh);
}

static void panel_frame_freq_set(struct s6e63j0x03 *lcd)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	struct mipi_dsim_device *master = lcd_to_master(lcd);

	PANEL_DBG_MSG(dbg_mode, lcd->dev, "S(refresh = %d)\n", lcd->refresh);

	if (lcd->refresh == REFRESH_30HZ) {
		ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TEST_KEY_ON_2, ARRAY_SIZE(TEST_KEY_ON_2));
		ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			FRAME_FREQ_30HZ_SET, ARRAY_SIZE(FRAME_FREQ_30HZ_SET));
		ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			LPTS_TIMMING_SET_30HZ_0,
			ARRAY_SIZE(LPTS_TIMMING_SET_30HZ_0));
		ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TEST_KEY_OFF_2, ARRAY_SIZE(TEST_KEY_OFF_2));
	} else {
		ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TEST_KEY_ON_2, ARRAY_SIZE(TEST_KEY_ON_2));
		ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			FRAME_FREQ_60HZ_SET, ARRAY_SIZE(FRAME_FREQ_60HZ_SET));
		ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			LPTS_TIMMING_SET_60HZ_0,
			ARRAY_SIZE(LPTS_TIMMING_SET_60HZ_0));
		ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TEST_KEY_OFF_2, ARRAY_SIZE(TEST_KEY_OFF_2));
	}

	PANEL_DBG_MSG(dbg_mode, lcd->dev, "E(refresh = %d)\n", lcd->refresh);
}


static ssize_t panel_lcd_type_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e63j0x03 *lcd = dev_get_drvdata(dev);
	char temp[15];

	sprintf(temp, "SDC_%02d%02d%02d\n",
			lcd->id[3], lcd->id[2], lcd->id[5]);

	strcat(buf, temp);

	return strlen(buf);
}

static ssize_t panel_alpm_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e63j0x03 *lcd = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n", lcd->alpm_on ? "on" : "off");
}

static ssize_t panel_alpm_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct s6e63j0x03 *lcd = dev_get_drvdata(dev);

	if (!strncmp(buf, "on", 2))
		lcd->alpm_on = true;
	else if (!strncmp(buf, "off", 3))
		lcd->alpm_on = false;
	else
		dev_warn(dev, "invalid command.\n");

	return size;
}

static void panel_acl_update(struct s6e63j0x03 *lcd, unsigned int value)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	struct mipi_dsim_device *master = lcd_to_master(lcd);

	if (value == MIN_ACL) {
		ops->cmd_write(master, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
				ACL_OFF, ARRAY_SIZE(ACL_OFF));
		return;
	}

	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TEST_KEY_ON_2, ARRAY_SIZE(TEST_KEY_ON_2));

	ops->cmd_write(master, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			ACL_SEL_LOOK, ARRAY_SIZE(ACL_SEL_LOOK));
	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE, acl_tbl[value - 1],
			ACL_CMD_CNT);
	ops->cmd_write(master, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			ACL_GLOBAL_PARAM, ARRAY_SIZE(ACL_GLOBAL_PARAM));
	ops->cmd_write(master, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			ACL_LUT_ENABLE, ARRAY_SIZE(ACL_LUT_ENABLE));
	ops->cmd_write(master, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			ACL_ON, ARRAY_SIZE(ACL_ON));

	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TEST_KEY_OFF_2, ARRAY_SIZE(TEST_KEY_OFF_2));
}

static ssize_t panel_acl_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e63j0x03 *lcd = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", lcd->acl);
}

static ssize_t panel_acl_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct s6e63j0x03 *lcd = dev_get_drvdata(dev);
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	struct mipi_dsim_device *master = lcd_to_master(lcd);
	unsigned long value;
	int rc;

	if (lcd->power > FB_BLANK_UNBLANK) {
		dev_warn(lcd->dev, "acl control before lcd enable.\n");
		return -EPERM;
	}

	rc = kstrtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;

	if (value < MIN_ACL || value > MAX_ACL) {
		dev_warn(dev, "invalid acl value[%ld]\n", value);
		return size;
	}

	if (ops->set_runtime_active(master)) {
		dev_warn(lcd->dev,
			"failed to set_runtime_active:power[%d]\n",
			lcd->power);

		if (lcd->power > FB_BLANK_UNBLANK)
			return -EPERM;
	}

	ops->standby(master, 0, 1);
	panel_acl_update(lcd, value);
	ops->standby(master, 1, 1);

	lcd->acl = value;

	dev_info(lcd->dev, "acl control[%d]\n", lcd->acl);

	return size;
}

static ssize_t panel_hbm_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e63j0x03 *lcd = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n", lcd->hbm_on ? "on" : "off");
}

static ssize_t panel_hbm_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct s6e63j0x03 *lcd = dev_get_drvdata(dev);
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	struct mipi_dsim_device *master = lcd_to_master(lcd);

	if (!strncmp(buf, "on", 2))
		lcd->hbm_on = 1;
	else if (!strncmp(buf, "off", 3))
		lcd->hbm_on = 0;
	else {
		dev_warn(dev, "invalid comman (use on or off)d.\n");
		return size;
	}

	if (lcd->power > FB_BLANK_UNBLANK) {
		/* let the fimd know the smies status
		 *  before DPMS ON
		 */
		ops->set_smies_active(master, lcd->hbm_on);
		dev_warn(lcd->dev, "hbm control before lcd enable.\n");
		return -EPERM;
	}

	if (ops->set_runtime_active(master)) {
		dev_warn(lcd->dev,
			"failed to set_runtime_active:power[%d]\n",
			lcd->power);

		if (lcd->power > FB_BLANK_UNBLANK)
			return -EPERM;
	}

	ops->standby(master, 0, 1);
#ifdef CONFIG_EXYNOS_SMIES_DEFALUT_ENABLE
	if (lcd->hbm_on)
		ops->set_smies_mode(master, OUTDOOR_MODE);
	else
		ops->set_smies_mode(master, DEFAULT_MODE);
#else
	ops->set_smies_active(master, lcd->hbm_on);
#endif
	if (lcd->hbm_on) {
		dev_info(lcd->dev, "HBM ON.\n");
		panel_enter_hbm_mode(lcd);
		ops->standby(master, 1, 1);
		ops->fimd_trigger(master);
	} else {
		dev_info(lcd->dev, "HBM OFF.\n");
		panel_exit_hbm_mode(lcd);
		ops->standby(master, 1, 1);
	}

	return size;
}

static ssize_t panel_refresh_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e63j0x03 *lcd = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", lcd->refresh);
}

static ssize_t panel_refresh_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct s6e63j0x03 *lcd = dev_get_drvdata(dev);
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	struct mipi_dsim_device *master = lcd_to_master(lcd);

	if (strncmp(buf, "30", 2) && strncmp(buf, "60", 2)) {
		dev_warn(dev, "invalid comman (use on or off)d.\n");
		return size;
	}

	if (ops->set_runtime_active(master)) {
		dev_warn(lcd->dev,
			"failed to set_runtime_active:power[%d]\n",
			lcd->power);

		if (lcd->power > FB_BLANK_UNBLANK)
			return -EPERM;
	}

	ops->standby(master, 0, 1);

	if (!strncmp(buf, "30", 2)) {
		lcd->refresh = 30;
		panel_frame_freq_set(lcd);
	} else {
		lcd->refresh = 60;
		panel_frame_freq_set(lcd);
	}
	ops->standby(master, 1, 1);

	return size;
}
DEVICE_ATTR(lcd_type, 0444, panel_lcd_type_show, NULL);
DEVICE_ATTR(alpm, 0644, panel_alpm_show, panel_alpm_store);
DEVICE_ATTR(acl, 0644, panel_acl_show, panel_acl_store);
DEVICE_ATTR(hbm, 0644, panel_hbm_show, panel_hbm_store);
DEVICE_ATTR(refresh, 0644, panel_refresh_show, panel_refresh_store);

#ifdef CONFIG_LCD_ESD
static void panel_esd_detect_work(struct work_struct *work)
{
	struct s6e63j0x03 *lcd = container_of(work,
						struct s6e63j0x03, det_work);
	char *event_string = "LCD_ESD=ON";
	char *envp[] = { event_string, NULL };

	if (!POWER_IS_OFF(lcd->power)) {
		kobject_uevent_env(&esd_dev->kobj,
			KOBJ_CHANGE, envp);
		dev_info(lcd->dev,
			"Send uevent. ESD DETECTED\n");
	}
}

irqreturn_t panel_det_interrupt(int irq, void *dev_id)
{
	struct s6e63j0x03 *lcd = dev_id;

	s3c_gpio_cfgpin(lcd->det_gpio, S3C_GPIO_SFN(0x00));

	if (!work_busy(&lcd->det_work)) {
		schedule_work(&lcd->det_work);
		dev_dbg(lcd->dev, "add esd schedule_work.\n");
	}

	return IRQ_HANDLED;
}
#endif

static int panel_probe(struct mipi_dsim_lcd_device *dsim_dev)
{
	struct s6e63j0x03 *lcd;
	struct mipi_dsim_platform_data *dsim_pd;
	struct fb_videomode *timing;
	int ret, i;

	lcd = devm_kzalloc(&dsim_dev->dev, sizeof(struct s6e63j0x03),
				GFP_KERNEL);
	if (!lcd) {
		dev_err(&dsim_dev->dev,
				"failed to allocate s6e63j0x03 structure.\n");
		return -ENOMEM;
	}

	lcd->dsim_dev = dsim_dev;
	lcd->dev = &dsim_dev->dev;
	lcd->pd = (struct lcd_platform_data *)dsim_dev->platform_data;
	lcd->boot_power_on = lcd->pd->lcd_enabled;
	lcd->dimming = devm_kzalloc(&dsim_dev->dev, sizeof(*lcd->dimming),
								GFP_KERNEL);
	if (!lcd->dimming) {
		dev_err(&dsim_dev->dev, "failed to allocate dimming.\n");
		ret = -ENOMEM;
		goto err_free_lcd;
	}

	for (i = 0; i < MAX_GAMMA_CNT; i++) {
		lcd->gamma_tbl[i] = (unsigned char *)
			kzalloc(sizeof(unsigned char) * GAMMA_CMD_CNT,
								GFP_KERNEL);
		if (!lcd->gamma_tbl[i]) {
			dev_err(&dsim_dev->dev,
					"failed to allocate gamma_tbl\n");
			ret = -ENOMEM;
			goto err_free_dimming;
		}
	}

	mutex_init(&lcd->lock);

	ret = devm_regulator_bulk_get(lcd->dev, ARRAY_SIZE(supplies), supplies);
	if (ret) {
		dev_err(lcd->dev, "Failed to get regulator: %d\n", ret);
		return ret;
	}

	lcd->bd = backlight_device_register("s6e63j0x03-bl", lcd->dev, lcd,
			&s6e63j0x03_backlight_ops, NULL);
	if (IS_ERR(lcd->bd)) {
		dev_err(lcd->dev, "failed to register backlight ops.\n");
		ret = PTR_ERR(lcd->bd);
		goto err_free_gamma_tbl;
	}

	lcd->ld = lcd_device_register("s6e63j0x03", lcd->dev, lcd,
					&s6e63j0x03_lcd_ops);
	if (IS_ERR(lcd->ld)) {
		dev_err(lcd->dev, "failed to register lcd ops.\n");
		ret = PTR_ERR(lcd->ld);
		goto err_unregister_bd;
	}

	if (lcd->pd)
		lcd->property = lcd->pd->pdata;

#ifdef CONFIG_LCD_ESD
	lcd->det_gpio = lcd->property->det_gpio;
	lcd->esd_irq = gpio_to_irq(lcd->det_gpio);

	ret = devm_request_irq(lcd->dev, lcd->esd_irq,
				panel_det_interrupt,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT, "esd",
				lcd);
	if (ret < 0) {
		dev_err(lcd->dev, "failed to request det irq.\n");
		goto err_unregister_lcd;
	}

	s3c_gpio_cfgpin(lcd->det_gpio, S3C_GPIO_SFN(0xf));
	s3c_gpio_setpull(lcd->det_gpio, S3C_GPIO_PULL_DOWN);
	INIT_WORK(&lcd->det_work, panel_esd_detect_work);

	esd_class = class_create(THIS_MODULE, "lcd_event");
	if (IS_ERR(esd_class)) {
		dev_err(lcd->dev, "Failed to create class(lcd_event)!\n");
		ret = PTR_ERR(esd_class);
		goto err_unregister_lcd;
	}

	esd_dev = device_create(esd_class, NULL, 0, NULL, "esd");
#endif

	ret = device_create_file(&lcd->ld->dev, &dev_attr_lcd_type);
	if (ret < 0) {
		dev_err(&lcd->ld->dev, " failed to create sysfs.\n");
		goto err_unregister_lcd;
	}

	ret = device_create_file(lcd->dev, &dev_attr_alpm);
	if (ret < 0) {
		dev_err(lcd->dev, " failed to create sysfs.\n");
		goto err_remove_lcd_type_file;
	}

	ret = device_create_file(lcd->dev, &dev_attr_acl);
	if (ret < 0) {
		dev_err(lcd->dev, " failed to create sysfs.\n");
		goto err_remove_alpm_file;
	}

	ret = device_create_file(lcd->dev, &dev_attr_hbm);
	if (ret < 0) {
		dev_err(lcd->dev, " failed to create sysfs.\n");
		goto err_remove_acl_file;
	}

	ret = device_create_file(lcd->dev, &dev_attr_refresh);
	if (ret < 0) {
		dev_err(lcd->dev, " failed to create sysfs.\n");
		goto err_remove_hbm_file;
	}

	lcd->power = FB_BLANK_UNBLANK;
	lcd->bd->props.max_brightness = MAX_BRIGHTNESS;
	lcd->bd->props.brightness = DEFAULT_BRIGHTNESS;
	dsim_pd = (struct mipi_dsim_platform_data *)lcd_to_master(lcd)->pd;
	timing = (struct fb_videomode *)dsim_pd->lcd_panel_info;
#if 0
	lcd->refresh = timing->refresh;
#else
	lcd->refresh = REFRESH_60HZ;
#endif

	dev_set_drvdata(&dsim_dev->dev, lcd);

	dev_info(lcd->dev, "probed s6e63j0x03 panel driver.\n");

	return 0;
err_remove_hbm_file:
	device_remove_file(lcd->dev, &dev_attr_hbm);
err_remove_acl_file:
	device_remove_file(lcd->dev, &dev_attr_acl);
err_remove_alpm_file:
	device_remove_file(lcd->dev, &dev_attr_alpm);
err_remove_lcd_type_file:
	device_remove_file(&lcd->ld->dev, &dev_attr_lcd_type);
err_unregister_lcd:
	lcd_device_unregister(lcd->ld);
err_unregister_bd:
	backlight_device_unregister(lcd->bd);
err_free_gamma_tbl:
	for (i = 0; i < MAX_GAMMA_CNT; i++)
		if (lcd->gamma_tbl[i])
			devm_kfree(&dsim_dev->dev, lcd->gamma_tbl[i]);
err_free_dimming:
	devm_kfree(&dsim_dev->dev, lcd->dimming);
err_free_lcd:
	devm_kfree(&dsim_dev->dev, lcd);

	return ret;
}

#ifdef CONFIG_PM
static int panel_suspend(struct mipi_dsim_lcd_device *dsim_dev)
{
	struct s6e63j0x03 *lcd = dev_get_drvdata(&dsim_dev->dev);

	PANEL_DBG_MSG(dbg_mode, lcd->dev, "S\n");

	if (lcd->alpm_on) {
		if (lcd->lp_mode)
			lcd->power = FB_BLANK_POWERDOWN;
		else {
			panel_alpm_on(lcd, lcd->alpm_on);
			lcd->power = FB_BLANK_POWERDOWN;
			lcd->lp_mode = true;
		}

		return 0;
	}
#ifdef CONFIG_LCD_ESD
	s3c_gpio_cfgpin(lcd->det_gpio, S3C_GPIO_SFN(0x00));
#endif

	panel_display_on(dsim_dev, 0);
	panel_sleep_in(lcd);

	usleep_range(lcd->pd->power_off_delay * 1000,
			lcd->pd->power_off_delay * 1000);

	panel_power_on(dsim_dev, 0);

	lcd->power = FB_BLANK_POWERDOWN;

	PANEL_DBG_MSG(dbg_mode, lcd->dev, "E\n");

	return 0;
}

static int panel_resume(struct mipi_dsim_lcd_device *dsim_dev)
{
	struct s6e63j0x03 *lcd = dev_get_drvdata(&dsim_dev->dev);

	PANEL_DBG_MSG(dbg_mode, lcd->dev, "S\n");

#ifdef CONFIG_LCD_ESD
	s3c_gpio_cfgpin(lcd->det_gpio, S3C_GPIO_SFN(0xf));
#endif

	PANEL_DBG_MSG(dbg_mode, lcd->dev, "E\n");

	return 0;
}
#else
#define panel_suspend	NULL
#define panel_resume	NULL
#endif

static struct mipi_dsim_lcd_driver panel_dsim_ddi_driver = {
	.name = "s6e63j0x03",
	.id = -1,
	.if_type = DSIM_COMMAND,
	.power_on = panel_power_on,
	.set_sequence = panel_set_sequence,
	.display_on = panel_display_on,
	.probe = panel_probe,
	.suspend = panel_suspend,
	.resume = panel_resume,
	.check_mtp = panel_check_mtp,
	.set_refresh_rate = panel_set_refresh_rate,
	.set_partial_region = panel_set_win_update_region,
	.panel_pm_check = panel_pm_check,
};

static int panel_init(void)
{
	int ret;

	ret = exynos_mipi_dsi_register_lcd_driver(&panel_dsim_ddi_driver);
	if (ret < 0) {
		pr_err("failed to register mipi lcd driver.\n");
		/* TODO - unregister lcd device. */
		return ret;
	}

	return 0;
}

static void panel_exit(void)
{
}

module_init(panel_init);
module_exit(panel_exit);

MODULE_AUTHOR("Inki Dae <inki.dae@samsung.com>");
MODULE_DESCRIPTION("MIPI-DSI based S6E63J0X03 AMOLED LCD Panel Driver");
MODULE_LICENSE("GPL");
