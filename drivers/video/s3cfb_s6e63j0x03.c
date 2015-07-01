/* linux/drivers/video/samsung/s3cfb_s6e8aa0.c
 *
 * MIPI-DSI based AMS529HA01 AMOLED lcd panel driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
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
#include <linux/gpio.h>
#include <linux/workqueue.h>
#include <linux/backlight.h>
#include <linux/lcd.h>
#include <plat/gpio-cfg.h>
#include <plat/regs-dsim.h>
#include <mach/dsim.h>
#include <mach/mipi_ddi.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#include "s5p-dsim.h"
#include "s3cfb.h"

#ifndef CONFIG_S6E63J0X03_SMART_DIMMING
#define TEMP_GAMMA
#endif

#define LOW_BRIGHTNESS_COMP

#ifdef CONFIG_S6E63J0X03_SMART_DIMMING
#include "ssd.h"
#endif

#define SUPPORT_DCS_READ
#define SUPPORT_HBM
#define SUPPORT_ESD_DET
#define SUPPORT_ACL

#define POWER_IS_ON(pwr)	((pwr) <= FB_BLANK_NORMAL)

#define MIN_BRIGHTNESS		0
#define MAX_BRIGHTNESS		255
#define DEFAULT_BRIGHTNESS	100

#define LDI_ID_REG			0xD3
#define LDI_ID_LEN			3

#ifdef CONFIG_S6E63J0X03_SMART_DIMMING
#define LDI_MTP_ADDR		0xD4
#define MTP_MAX_CNT			27
#endif

#define MAX_GAMMA_CNT			12
#define GAMMA_ELEMENT_CNT		28

struct lcd_info {
	unsigned int			bl;
	unsigned int			auto_brightness;
	unsigned int			acl_enable;
	unsigned int			siop_enable;
	unsigned int			current_acl;
	unsigned int			current_bl;
	unsigned int			current_elvss;

	unsigned int			ldi_enable;
	unsigned int			power;
	struct mutex			lock;
	struct mutex			bl_lock;

	struct device			*dev;
	struct lcd_device		*ld;
	struct backlight_device		*bd;
	struct lcd_platform_data	*lcd_pd;
	struct early_suspend		early_suspend;

	unsigned char			id[LDI_ID_LEN];
#if 0 
	unsigned char			**gamma_table;
	unsigned char			**elvss_table;
#endif
	unsigned int			irq;
	unsigned int			connected;

	struct dsim_global		*dsim;

#ifdef CONFIG_FB_SUPPORT_ALPM
	unsigned int alpm;
	unsigned int status;
#endif

#ifdef SUPPORT_HBM
	unsigned char elvss_hbm;
	unsigned char default_hbm;
	unsigned char hbm;
#endif

#ifdef CONFIG_S6E63J0X03_SMART_DIMMING
	struct dim_data *dimming;
	unsigned char *gamma_tbl[MAX_GAMMA_CNT];
#endif

#ifdef SUPPORT_ESD_DET
	struct work_struct oled_work;
#endif

#ifdef FIMD_OVE_TUNE
	unsigned int ove_mode;
#endif

#ifdef LOW_BRIGHTNESS_COMP
	unsigned int comp;
#endif
#ifdef SUPPORT_ACL
	unsigned int acl;
#endif
};

#ifdef CONFIG_FB_SUPPORT_ALPM
#define PANEL_STATUS_LCDON	0
#define PANEL_STATUS_LCDOFF	1
#define PANEL_STATUS_ALPM_LCD_ON  3
#define PANEL_STATUS_ALPM_LCD_OFF 4
#define PANEL_STATUS_RESET	5
#endif
struct lcd_info *g_lcd;

extern void (*lcd_early_suspend)(void);
extern void (*lcd_late_resume)(void);

static int update_brightness(struct lcd_info *lcd, u8 force);

static int set_hbm_mode(struct lcd_info *lcd);
static int exit_hbm_mode(struct lcd_info *lcd);

#ifdef CONFIG_FB_SUPPORT_ALPM
int get_alpm_mode(void);
#endif


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

static const unsigned char FRAME_FREQ_SET[] = {
	0xB5,
	0x00, 0x01, 0x00
};

static const unsigned char MEM_ADDR_SET_0[] = {
	0x2A,
	0x00, 0x14, 0x01, 0x53
};

static const unsigned char MEM_ADDR_SET_1[] = {
	0x2B,
	0x00, 0x00, 0x01, 0x3F
};

static const unsigned char LPTS_TIMMING_SET_0[] = {
	0xF8,
	0x08, 0x08, 0x08, 0x17,
	0x00, 0x2A, 0x02, 0x26,
	0x00, 0x00, 0x02, 0x00,
	0x00
};

static const unsigned char LPTS_TIMMING_SET_1[] = {
	0xF7,
	0x02
};

static const unsigned char SLEEP_OUT[] = {
	0x11,
	0x00, 0x00
};

static const unsigned char ELVSS_COND[] = {
	0xB1, 
	0x00, 0x09,
};

static const unsigned char DEFAULT_WHITE_BRIGHTNESS[] = {
	0x51,
	0xFF
};

static const unsigned char DEFAULT_WHITE_CTRL[] = {
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

static const unsigned char WHITE_BRIGHTNESS_10_NIT[] = {
	0x51,
	0x26
};

static const unsigned char WHITE_CTRL_10_NIT[] = {
	0x53,
	0x20,
};

static const unsigned char WHITE_BRIGHTNESS_30_NIT[] = {
	0x51,
	0x13,
};

static const unsigned char WHITE_CTRL_30_NIT[] = {
	0x53,
	0x20,
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
#ifdef SUPPORT_ACL
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

#endif


#if defined (TEMP_GAMMA)
const unsigned char gamma_10[] =  {
	0xD4,
	0x00, 0x00, 0x00, 0x30,
	0x30, 0x30, 0x2E, 0x3D,
	0x3D, 0x0C, 0x25, 0x27,
	0x1E, 0x23, 0x26, 0x2E,
	0x2D, 0x2F, 0x33, 0x32,
	0x33, 0x00, 0x96, 0x00,
	0x9E, 0x00, 0xAE
};

const unsigned char gamma_14[] =  {
	0xD4,
	0x00, 0x00, 0x00, 0x1C,
	0x1C, 0x1C, 0x40, 0x63,
	0x66, 0x12, 0x23, 0x25,
	0x22, 0x22, 0x24, 0x2E,
	0x2C, 0x2E, 0x32, 0x31,
	0x32, 0x00, 0xAE, 0x00,
	0xB8, 0x00, 0xC9
};

const unsigned char gamma_44[] = {
	0xD4,
	0x00, 0x00, 0x00, 0x30,
	0x36, 0x36, 0x39, 0x5C,
	0x5E, 0x21, 0x26, 0x28,
	0x22, 0x21, 0x24, 0x2C,
	0x2B, 0x2D, 0x31, 0x30,
	0x31, 0x00, 0xC4, 0x00,
	0xD0, 0x00, 0xE2
};

const unsigned char gamma_64[] = {
	0xD4,
	0x00, 0x00, 0x00, 0x29,
	0x33, 0x33, 0x3E, 0x5C,
	0x5F, 0x21, 0x22, 0x25,
	0x23, 0x21, 0x24, 0x2D,
	0x2C, 0x2D, 0x2F, 0x2F,
	0x2F, 0x00, 0xD8, 0x00,
	0xE6, 0x00, 0xFA
};

const unsigned char gamma_93[] = {
	0xD4,
	0x00, 0x00, 0x00, 0x30,
	0x45, 0x46, 0x4E, 0x62,
	0x64, 0x22, 0x22, 0x25,
	0x22, 0x20, 0x22, 0x2B,
	0x2B, 0x2B, 0x2F, 0x2F,
	0x2F, 0x00, 0xE9, 0x00,
	0xF9, 0x01, 0x0D
};

const unsigned char gamma_119[] = {
	0xD4,
	0x00, 0x00, 0x00, 0x2B,
	0x42, 0x43, 0x55, 0x63,
	0x65, 0x22, 0x22, 0x24,
	0x21, 0x1F, 0x22, 0x2A,
	0x29, 0x2A, 0x30, 0x2F,
	0x30, 0x00, 0xF5, 0x01,
	0x06, 0x01, 0x1B
};

const unsigned char gamma_143[] = {
	0xD4,
	0x00, 0x00, 0x00, 0x36,
	0x56, 0x58, 0x57, 0x62,
	0x65, 0x22, 0x21, 0x24,
	0x20, 0x1F, 0x21, 0x2A,
	0x29, 0x2A, 0x2F, 0x2F,
	0x2F, 0x00, 0xFF, 0x01,
	0x12, 0x01, 0x27
};

const unsigned char gamma_195[] = {
	0xD4,	
	0x00, 0x00, 0x00, 0x33,
	0x53, 0x55, 0x58, 0x60,
	0x63, 0x21, 0x20, 0x23,
	0x20, 0x1F, 0x20, 0x29,
	0x29, 0x29, 0x2E, 0x2E,
	0x2E, 0x01, 0x12, 0x01,
	0x28, 0x01, 0x3E
};

const unsigned char gamma_220[] = {
	0xD4,
	0x00, 0x00, 0x00, 0x31,
	0x51, 0x53, 0x5A, 0x5F,
	0x62, 0x22, 0x21, 0x24,
	0x20, 0x1F, 0x20, 0x29,
	0x28, 0x29, 0x2E, 0x2E,
	0x2E, 0x01, 0x1A, 0x01,
	0x30, 0x01, 0x47
};

const unsigned char gamma_249[] = {
	0xD4,
	0x00, 0x00, 0x00, 0x3A, 
	0x63, 0x66, 0x5D, 0x61, 
	0x63, 0x22, 0x20, 0x23, 
	0x1F, 0x1E, 0x1F, 0x2A,
	0x29, 0x2A, 0x2E, 0x2D,
	0x2D, 0x01, 0x22, 0x01, 
	0x3A, 0x01, 0x52
};

const unsigned char gamma_300[] = {
	0xD4,
	0x00, 0x00, 0x00, 0x38,
	0x61, 0x64, 0x5E, 0x5F,
	0x62, 0x23, 0x21, 0x24, 
	0x1F, 0x1E, 0x1F, 0x2A,
	0x29, 0x2A, 0x2E, 0x2D, 
	0x2D, 0x01, 0x31, 0x01, 
	0x4B, 0x01, 0x64
};


const unsigned char *gamma_tbl[MAX_GAMMA_CNT] = {
	gamma_10,
	gamma_14,
	gamma_44,
	gamma_64,
	gamma_93,
	gamma_119,
	gamma_143,
	gamma_195,
	gamma_220,
	gamma_249,
	gamma_300,
	gamma_300,
};
#endif

const unsigned char br_tbl[MAX_BRIGHTNESS+1] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
	9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
	10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
	11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11
};


#ifdef CONFIG_S6E63J0X03_SMART_DIMMING
const unsigned char dft_ref_gamma[MTP_MAX_CNT] = {
	0x00, 0x00, 0x00, 0x38,
	0x61, 0x64, 0x5E, 0x5F,
	0x62, 0x23, 0x21, 0x24, 
	0x1F, 0x1E, 0x1F, 0x2A,
	0x29, 0x2A, 0x2E, 0x2D, 
	0x2D, 0x01, 0x31, 0x01, 
	0x4B, 0x01, 0x64
};

unsigned short cd_tbl[MAX_GAMMA_CNT] = {
	10, 30, 60, 90, 120, 150, 200, 220, 240, 300, 300, 300,
};
#endif

static int s6e63j0x03_write(struct lcd_info *lcd, const unsigned char *seq, int len)
{
	int size;
	unsigned char wr_ret;
	const unsigned char *wbuf;
	int retry_cnt = 5;

	if (!lcd->connected)
		return 0;

	mutex_lock(&lcd->lock);

	size = len;
	wbuf = seq;

write_cmd:

	if (size == 1)
		wr_ret = lcd->dsim->ops->cmd_write(lcd->dsim, DCS_WR_NO_PARA, wbuf[0], 0);
	else if (size == 2)
		wr_ret = lcd->dsim->ops->cmd_write(lcd->dsim, DCS_WR_1_PARA, wbuf[0], wbuf[1]);
	else
		wr_ret = lcd->dsim->ops->cmd_write(lcd->dsim, DCS_LONG_WR, (unsigned int)wbuf, size);

	if (!wr_ret) {
		if (retry_cnt--) {
			goto write_cmd;
		}
		dev_err(&lcd->ld->dev, "%s : write failed\n", __func__);
	}

	mutex_unlock(&lcd->lock);
	return 0;
}

static int _s6e63j0x03_read(struct lcd_info *lcd, const u8 addr, u16 count, u8 *buf)
{
	int ret = 0;

	if (!lcd->connected)
		return ret;

	mutex_lock(&lcd->lock);

	if (lcd->dsim->ops->cmd_read)
#ifdef SUPPORT_DCS_READ
		ret = lcd->dsim->ops->cmd_dcs_read(lcd->dsim, addr, count, buf);
#else
		ret = lcd->dsim->ops->cmd_read(lcd->dsim, addr, count, buf);
#endif
	mutex_unlock(&lcd->lock);

	return ret;
}

static int s6e63j0x03_read(struct lcd_info *lcd, const u8 addr, u16 count, u8 *buf, u8 retry_cnt)
{
	int ret = 0;

	if (!lcd->connected)
		return 0;
	
read_retry:
	ret = _s6e63j0x03_read(lcd, addr, count, buf);
	if (ret != count) {
		if (retry_cnt) {
			printk(KERN_WARNING "[WARN:LCD] %s : retry cnt : %d\n", __func__, retry_cnt);
			retry_cnt--;
			goto read_retry;
		} else
			printk(KERN_ERR "[ERROR:LCD] %s : 0x%02x read failed\n", __func__, addr);
	}

	return ret;
}


static int s6e63j0x03_get_id(struct lcd_info *lcd, u8 *buf)
{
	int ret = 0;
	int retry_cnt = 3;
	u8 temp_id[6];


	s6e63j0x03_write(lcd, TEST_KEY_ON_2, ARRAY_SIZE(TEST_KEY_ON_2));

read_id:
	ret = s6e63j0x03_read(lcd, LDI_ID_REG, 6, temp_id, 2);
	if (ret != 6) {
		if (!retry_cnt--) {
			lcd->connected = 0;
			dev_err(&lcd->ld->dev, "panel is not connected well\n");
			return -1;
		}
		goto read_id;
	}

	buf[0] = temp_id[3];
	buf[1] = temp_id[2];
	buf[2] = temp_id[5];

	s6e63j0x03_write(lcd, TEST_KEY_OFF_2, ARRAY_SIZE(TEST_KEY_OFF_2));

#ifdef LOW_BRIGHTNESS_COMP
	if (buf[2] >= 0x02) {
		dev_info(&lcd->ld->dev, "enable low brightness compensation\n");
		lcd->comp = 1;
	} else {
		lcd->comp = 0;
	}
#endif

	return 0;
}


#ifdef SUPPORT_HBM
static int s6e63j0x03_set_hbm_param(struct lcd_info *lcd)
{
	int ret;
	s6e63j0x03_write(lcd, TEST_KEY_ON_2, ARRAY_SIZE(TEST_KEY_ON_2));

	s6e63j0x03_write(lcd, PARAM_POS_HBM_ELVSS, sizeof(PARAM_POS_HBM_ELVSS));
	ret = s6e63j0x03_read(lcd, 0xd4, 1, &lcd->elvss_hbm, 2);
	dev_info(&lcd->ld->dev, "HBM ELVSS : %x\n", lcd->elvss_hbm);
	
	s6e63j0x03_write(lcd, PARAM_POS_DEF_ELVSS, sizeof(PARAM_POS_DEF_ELVSS));
	s6e63j0x03_read(lcd, 0xd3, 1, &lcd->default_hbm, 2);
	dev_info(&lcd->ld->dev, "DEF ELVSS : %x\n", lcd->default_hbm);

	s6e63j0x03_write(lcd, PARAM_POS_DEFAULT, sizeof(PARAM_POS_DEFAULT));

	s6e63j0x03_write(lcd, TEST_KEY_OFF_2, ARRAY_SIZE(TEST_KEY_OFF_2));

	lcd->hbm = 0;

	return ret;
}
#endif


#ifdef CONFIG_S6E63J0X03_SMART_DIMMING
static int s6e63j0x03_get_mtp(struct lcd_info *lcd)
{
	int i, j;
	int ret;
	u8 gamma[27];

	s6e63j0x03_write(lcd, TEST_KEY_ON_2, ARRAY_SIZE(TEST_KEY_ON_2));

	ret = s6e63j0x03_read(lcd, LDI_MTP_ADDR, MTP_MAX_CNT, gamma, 2);
	if (!ret) {
		dev_info(&lcd->ld->dev, "failed to read mtp value\n");
	}
	
	dev_info(&lcd->ld->dev, "read gamma : ");
	for (i = 0; i < MTP_MAX_CNT; i++) {
		dev_info(&lcd->ld->dev, "%02x ", gamma[i]);
	}
	//dev_info(&lcd->ld->dev, "\n");

	s6e63j0x03_write(lcd, TEST_KEY_OFF_2, ARRAY_SIZE(TEST_KEY_OFF_2));

	read_mtp(lcd->dimming, gamma);

	return ret;
}
#endif

extern void s3cfb_trigger(void);

static int s6e63j0x03_ldi_init(struct lcd_info *lcd)
{
	int ret = 0;

	//usleep_range(10000, 10000);

	s6e63j0x03_write(lcd, TEST_KEY_ON_1, ARRAY_SIZE(TEST_KEY_ON_1));
	s6e63j0x03_write(lcd, TEST_KEY_ON_2, ARRAY_SIZE(TEST_KEY_ON_2));

	/* Setting for panel condition */
	s6e63j0x03_write(lcd, PORCH_ADJUSTMENT, ARRAY_SIZE(PORCH_ADJUSTMENT));
	s6e63j0x03_write(lcd, FRAME_FREQ_SET, ARRAY_SIZE(FRAME_FREQ_SET));
	s6e63j0x03_write(lcd, MEM_ADDR_SET_0, ARRAY_SIZE(MEM_ADDR_SET_0));
	s6e63j0x03_write(lcd, MEM_ADDR_SET_1, ARRAY_SIZE(MEM_ADDR_SET_1));
	s6e63j0x03_write(lcd, LPTS_TIMMING_SET_0, ARRAY_SIZE(LPTS_TIMMING_SET_0));
	s6e63j0x03_write(lcd, LPTS_TIMMING_SET_1, ARRAY_SIZE(LPTS_TIMMING_SET_1));

	s6e63j0x03_write(lcd, SLEEP_OUT, ARRAY_SIZE(SLEEP_OUT));

	usleep_range(120000, 120000);
	
	/* Setting for brightness control */
	s6e63j0x03_write(lcd, DEFAULT_WHITE_BRIGHTNESS, ARRAY_SIZE(DEFAULT_WHITE_BRIGHTNESS));
	s6e63j0x03_write(lcd, DEFAULT_WHITE_CTRL, ARRAY_SIZE(DEFAULT_WHITE_CTRL));
	s6e63j0x03_write(lcd, ACL_OFF, ARRAY_SIZE(ACL_OFF));
	
	s6e63j0x03_write(lcd, ELVSS_COND, ARRAY_SIZE(ELVSS_COND));
	s6e63j0x03_write(lcd, SET_POS, ARRAY_SIZE(SET_POS));
	/* tearing effect on */
	s6e63j0x03_write(lcd, TE_ON, ARRAY_SIZE(TE_ON));

	//usleep_range(120000, 120000);

	s6e63j0x03_write(lcd, TEST_KEY_OFF_2, ARRAY_SIZE(TEST_KEY_OFF_2));

	return ret;
}


static int s6e63j0x03_ldi_enable(struct lcd_info *lcd)
{
	int ret = 0;

	s6e63j0x03_write(lcd, DISPLAY_ON, ARRAY_SIZE(DISPLAY_ON));

	return ret;
}


static int s6e63j0x03_ldi_disable(struct lcd_info *lcd)
{
	int ret = 0;
	
	s6e63j0x03_write(lcd, DISPLAY_OFF, ARRAY_SIZE(DISPLAY_OFF));

	s6e63j0x03_write(lcd, SLEEP_IN, ARRAY_SIZE(SLEEP_IN));

	msleep(120);
	
	return ret;
}

#ifdef CONFIG_FB_SUPPORT_ALPM
static int s6e63j0x03_ldi_enter_alpm(struct lcd_info *lcd)
{
	int ret = 0;

	s6e63j0x03_write(lcd, DISPLAY_OFF, ARRAY_SIZE(DISPLAY_OFF));
	s3cfb_trigger();
	usleep_range(17000, 17000);
	s6e63j0x03_write(lcd, PARTIAL_AREA_SET, ARRAY_SIZE(PARTIAL_AREA_SET));
	s6e63j0x03_write(lcd, PARTIAL_MODE_ON, ARRAY_SIZE(PARTIAL_MODE_ON));
	s6e63j0x03_write(lcd, IDLE_MODE_ON, ARRAY_SIZE(IDLE_MODE_ON));
	s6e63j0x03_write(lcd, DISPLAY_ON, ARRAY_SIZE(DISPLAY_ON));

	return ret;
}

static int s6e63j0x03_ldi_exit_alpm(struct lcd_info *lcd)
{
	int ret = 0;
	dev_info(&lcd->ld->dev, "%s was called\n", __func__);
	s6e63j0x03_write(lcd, DISPLAY_OFF, ARRAY_SIZE(DISPLAY_OFF));
	s3cfb_trigger();
	usleep_range(17000, 17000);
	s6e63j0x03_write(lcd, IDLE_MODE_OFF, ARRAY_SIZE(IDLE_MODE_OFF));
	s6e63j0x03_write(lcd, NORMAL_MODE_ON, ARRAY_SIZE(NORMAL_MODE_ON));
	s6e63j0x03_write(lcd, DISPLAY_ON, ARRAY_SIZE(DISPLAY_ON));

	return ret;
}

#endif
static int s6e63j0x03_ldi_power_on(struct lcd_info *lcd)
{
	int ret = 0;
	struct lcd_platform_data *pd = NULL;
	unsigned char lcd_id[3] = {0, };

	pd = lcd->lcd_pd;
	
	dev_info(&lcd->ld->dev, "%s\n", __func__);

#ifdef CONFIG_FB_SUPPORT_ALPM
	if ((lcd->status == PANEL_STATUS_LCDOFF) || 
			(lcd->status == PANEL_STATUS_RESET))
		goto panel_turn_on;

	else if (lcd->status == PANEL_STATUS_ALPM_LCD_OFF) {
		if (lcd->alpm) {
			dev_info(&lcd->ld->dev, "prev status : ALPM, current : ALPM\n");
			lcd->status = PANEL_STATUS_ALPM_LCD_ON;
			goto out;
		} else {
			s6e63j0x03_ldi_exit_alpm(lcd);
			goto update_br;
		}
	}

	else if (lcd->status == PANEL_STATUS_LCDON)
		goto update_br;

#endif

panel_turn_on:
	ret = s6e63j0x03_ldi_init(lcd);
	if (ret) {
		dev_err(&lcd->ld->dev, "failed to initialize ldi.\n");
		goto out;
	}

	s3cfb_trigger();

	usleep_range(20000, 20000);

	ret = s6e63j0x03_ldi_enable(lcd);
	if (ret) {
		dev_err(&lcd->ld->dev, "failed to enable ldi.\n");
		goto out;
	}

update_br:
	lcd->status = PANEL_STATUS_LCDON;
	lcd->ldi_enable = 1;

	update_brightness(lcd, 1);
out:
	return ret;
}



static int s6e63j0x03_ldi_power_off(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "%s\n", __func__);

	lcd->ldi_enable = 0;
#ifdef CONFIG_FB_SUPPORT_ALPM
	if (lcd->alpm) {
		if (lcd->status == PANEL_STATUS_LCDON) {
			ret = s6e63j0x03_ldi_enter_alpm(lcd);
			lcd->status = PANEL_STATUS_ALPM_LCD_OFF;
		} 
		else if (lcd->status == PANEL_STATUS_ALPM_LCD_ON) {
			lcd->status = PANEL_STATUS_ALPM_LCD_OFF;
		}
	} else {
		ret = s6e63j0x03_ldi_disable(lcd);
		lcd->status = PANEL_STATUS_LCDOFF;
	}
#else
	ret = s6e63j0x03_ldi_disable(lcd);
#endif
	return ret;
}


static int s6e63j0x03_ldi_power(struct lcd_info *lcd, int power)
{
	int ret = 0;

	printk("%s was called : power : %d\n",__func__, power);

	if (POWER_IS_ON(power) && !POWER_IS_ON(lcd->power))
		ret = s6e63j0x03_ldi_power_on(lcd);
	else if (!POWER_IS_ON(power) && POWER_IS_ON(lcd->power))
		ret = s6e63j0x03_ldi_power_off(lcd);

	printk("%s ret : %d\n", __func__, ret);

	if (!ret)
		lcd->power = power;

	return ret;
}


static int s6e63j0x03_set_power(struct lcd_device *ld, int power)
{
	struct lcd_info *lcd = lcd_get_data(ld);

	if (power != FB_BLANK_UNBLANK && power != FB_BLANK_POWERDOWN &&
		power != FB_BLANK_NORMAL) {
		dev_err(&lcd->ld->dev, "power value should be 0, 1 or 4.\n");
		return -EINVAL;
	}

	return s6e63j0x03_ldi_power(lcd, power);
}



static int s6e63j0x03_get_power(struct lcd_device *ld)
{
	struct lcd_info *lcd = lcd_get_data(ld);

	return lcd->power;
}

static int s6e63j0x03_check_fb(struct lcd_device *ld, struct fb_info *fb)
{
	struct lcd_info *lcd = lcd_get_data(ld);

	dev_info(&lcd->ld->dev, "%s, fb%d\n", __func__, fb->node);

	return 0;
}

#if defined (TEMP_GAMMA)
static int update_gamma(struct lcd_info *lcd, int br)
{
	int i;
	int ret = 0;
	unsigned char **tbl;

	if (br >= MAX_GAMMA_CNT)
		br = MAX_GAMMA_CNT - 1;

	if (gamma_tbl[br] == NULL) {
		dev_err(&lcd->ld->dev, "ERR:%s:faied to get gamma tbl\n", __func__);
		return -EINVAL;
	}
	
	s6e63j0x03_write(lcd, TEST_KEY_ON_2, ARRAY_SIZE(TEST_KEY_ON_2));

	switch (br) { 
		case 0 : 
			s6e63j0x03_write(lcd, gamma_10, GAMMA_ELEMENT_CNT);
			break;
		case 1 :
			s6e63j0x03_write(lcd, gamma_14, GAMMA_ELEMENT_CNT);
			break;
		case 2 :
			s6e63j0x03_write(lcd, gamma_44, GAMMA_ELEMENT_CNT);
			break;
		case 3 :
			s6e63j0x03_write(lcd, gamma_64, GAMMA_ELEMENT_CNT);
			break;
		case 4 :
			s6e63j0x03_write(lcd, gamma_93, GAMMA_ELEMENT_CNT);
			break;
		case 5 :
			s6e63j0x03_write(lcd, gamma_119, GAMMA_ELEMENT_CNT);
			break;	
		case 6 :
			s6e63j0x03_write(lcd, gamma_143, GAMMA_ELEMENT_CNT);
			break;
		case 7 :
			s6e63j0x03_write(lcd, gamma_195, GAMMA_ELEMENT_CNT);
			break;
		case 8 :
			s6e63j0x03_write(lcd, gamma_220, GAMMA_ELEMENT_CNT);
			break;
		case 9 :
			s6e63j0x03_write(lcd, gamma_249, GAMMA_ELEMENT_CNT);
			break;
		case 10 :
			s6e63j0x03_write(lcd, gamma_300, GAMMA_ELEMENT_CNT);
			break;
	}
	
	s6e63j0x03_write(lcd, TEST_KEY_OFF_2, ARRAY_SIZE(TEST_KEY_OFF_2));

	return ret;
}
#endif

#ifdef FIMD_OVE_TUNE
#define OVE_MODE_1_BR	10
#define OVE_MODE_2_BR	11
extern int s3cfb_enable_ove_tune(int enable);
#endif

#ifdef CONFIG_S6E63J0X03_SMART_DIMMING
static int write_gamma(struct lcd_info *lcd, unsigned char *gamma)
{
	int ret = 0;

	s6e63j0x03_write(lcd, TEST_KEY_ON_2, ARRAY_SIZE(TEST_KEY_ON_2));
	s6e63j0x03_write(lcd, gamma, GAMMA_ELEMENT_CNT);
	s6e63j0x03_write(lcd, TEST_KEY_OFF_2, ARRAY_SIZE(TEST_KEY_OFF_2));
	
	return ret;
}


static int update_gamma(struct lcd_info *lcd, int br)
{
	int i;
	int ret = 0;
	int prev_cd;

	dev_info(&lcd->ld->dev, "gamma cd : %d\n", cd_tbl[br]);
	
	if (br == OVE_MODE_2_BR) {
		set_hbm_mode(lcd);
		lcd->hbm = 1;
	}
	else {
		if (lcd->hbm == 1) {
			exit_hbm_mode(lcd);
			lcd->hbm = 0;
		}
#ifdef LOW_BRIGHTNESS_COMP
		if (lcd->comp) {
			prev_cd = cd_tbl[lcd->current_bl];
			if (cd_tbl[br] == 10) {
				s6e63j0x03_write(lcd, WHITE_BRIGHTNESS_10_NIT, ARRAY_SIZE(WHITE_BRIGHTNESS_10_NIT));
				s6e63j0x03_write(lcd, WHITE_CTRL_10_NIT, ARRAY_SIZE(WHITE_CTRL_10_NIT));
			}
			else if (cd_tbl[br] == 30) {
				s6e63j0x03_write(lcd, WHITE_BRIGHTNESS_30_NIT, ARRAY_SIZE(WHITE_BRIGHTNESS_30_NIT));
				s6e63j0x03_write(lcd, WHITE_CTRL_30_NIT, ARRAY_SIZE(WHITE_CTRL_30_NIT));
			}
			else {
				if ((prev_cd == 10) || (prev_cd == 30)) {
					s6e63j0x03_write(lcd, DEFAULT_WHITE_BRIGHTNESS, ARRAY_SIZE(DEFAULT_WHITE_BRIGHTNESS));
					s6e63j0x03_write(lcd, DEFAULT_WHITE_CTRL, ARRAY_SIZE(DEFAULT_WHITE_CTRL));
				}
			}	
		}
		ret = write_gamma(lcd, lcd->gamma_tbl[br]);
#else
		ret = write_gamma(lcd, lcd->gamma_tbl[br]);
#endif
	}
	return ret;
}

#endif


static int update_brightness(struct lcd_info *lcd, u8 force)
{
	int br, ret;	
	u32 brightness;

	mutex_lock(&lcd->bl_lock);

	brightness = lcd->bd->props.brightness;

	if (brightness > MAX_BRIGHTNESS)
		brightness = MAX_BRIGHTNESS;

	br = br_tbl[brightness];

	dev_info(&lcd->ld->dev, "calculated br : %d force : %d\n\n", br, force);

	if (!force) {
		if (br == lcd->current_bl) {
			goto exit;
		}
	}
#ifdef FIMD_OVE_TUNE
	if ((br == OVE_MODE_1_BR) || (br == OVE_MODE_2_BR)) {
		if (!lcd->ove_mode)
			s3cfb_enable_ove_tune(1);
		dev_info(&lcd->ld->dev, "ove mode : %d\n", br-(OVE_MODE_1_BR-1));
		lcd->ove_mode = br - (OVE_MODE_1_BR - 1); 
	} else {
		if (lcd->ove_mode)
			s3cfb_enable_ove_tune(0);
		lcd->ove_mode = 0;
	}	
#endif

	ret = update_gamma(lcd, br);
	if (ret) {
		dev_err(&lcd->ld->dev, "failed to update brightness \n");
	}

	lcd->current_bl = br;

exit:
	mutex_unlock(&lcd->bl_lock);

	return 0;
}


static int s6e63j0x03_set_brightness(struct backlight_device *bd)
{
	int ret = 0;
	int brightness = bd->props.brightness;
	struct lcd_info *lcd = bl_get_data(bd);

	dev_info(&lcd->ld->dev, "%s: brightness=%d\n", __func__, brightness);

	if (brightness <= MIN_BRIGHTNESS ||
		brightness > bd->props.max_brightness) {
		dev_err(&bd->dev, "lcd brightness should be %d to %d. now %d\n",
			MIN_BRIGHTNESS, MAX_BRIGHTNESS, brightness);
		return -EINVAL;
	}

	if (lcd->ldi_enable) {
		ret = update_brightness(lcd, 0);
		if (ret < 0) {
			dev_err(lcd->dev, "err in %s\n", __func__);
			return -EINVAL;
		}
	}

	return ret;
}

static int s6e63j0x03_get_brightness(struct backlight_device *bd)
{
	struct lcd_info *lcd = bl_get_data(bd);

	return bd->props.brightness;
}

static struct lcd_ops s6e63j0x03_lcd_ops = {
	.set_power = s6e63j0x03_set_power,
	.get_power = s6e63j0x03_get_power,
	.check_fb  = s6e63j0x03_check_fb,
};

static const struct backlight_ops s6e63j0x03_backlight_ops  = {
	.get_brightness = s6e63j0x03_get_brightness,
	.update_status = s6e63j0x03_set_brightness,
};

static ssize_t power_reduce_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	char temp[3];

	sprintf(temp, "%d\n", lcd->acl_enable);
	strcpy(buf, temp);

	return strlen(buf);
}

static ssize_t power_reduce_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	int value;
	int rc;

	rc = strict_strtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;
	else {
		if (lcd->acl_enable != value) {
			dev_info(dev, "%s - %d, %d\n", __func__, lcd->acl_enable, value);
			mutex_lock(&lcd->bl_lock);
			lcd->acl_enable = value;
			mutex_unlock(&lcd->bl_lock);
			if (lcd->ldi_enable)
				update_brightness(lcd, 1);
		}
	}
	return size;
}

static DEVICE_ATTR(power_reduce, 0664, power_reduce_show, power_reduce_store);

static ssize_t siop_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	char temp[3];

	sprintf(temp, "%d\n", lcd->siop_enable);
	strcpy(buf, temp);

	return strlen(buf);
}

static ssize_t siop_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	int value;
	int rc;

	rc = strict_strtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;
	else {
		if (lcd->siop_enable != value) {
			dev_info(dev, "%s - %d, %d\n", __func__, lcd->siop_enable, value);
			mutex_lock(&lcd->bl_lock);
			lcd->siop_enable = value;
			mutex_unlock(&lcd->bl_lock);
			if (lcd->ldi_enable)
				update_brightness(lcd, 1);
		}
	}
	return size;
}

static DEVICE_ATTR(siop_enable, 0664, siop_enable_show, siop_enable_store);

static ssize_t lcd_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	char temp[15];

	sprintf(temp, "SMD_AMS163AX01\n");

	strcat(buf, temp);
	return strlen(buf);
}

static DEVICE_ATTR(lcd_type, 0444, lcd_type_show, NULL);

static ssize_t gamma_table_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	return strlen(buf);
}
static DEVICE_ATTR(gamma_table, 0444, gamma_table_show, NULL);

static ssize_t auto_brightness_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	char temp[3];

	sprintf(temp, "%d\n", lcd->auto_brightness);
	strcpy(buf, temp);

	return strlen(buf);
}

static ssize_t auto_brightness_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	int value;
	int rc;

	rc = strict_strtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;
	else {
		if (lcd->auto_brightness != value) {
			dev_info(dev, "%s - %d, %d\n", __func__, lcd->auto_brightness, value);
			mutex_lock(&lcd->bl_lock);
			lcd->auto_brightness = value;
			mutex_unlock(&lcd->bl_lock);
			if (lcd->ldi_enable)
				update_brightness(lcd, 1);
		}
	}
	return size;
}

static DEVICE_ATTR(auto_brightness, 0644, auto_brightness_show, auto_brightness_store);

#ifdef CONFIG_FB_SUPPORT_ALPM
int get_alpm_mode(void)
{
	int ret = 0;
	if (g_lcd)
		ret = g_lcd->alpm;

	return ret;
}
EXPORT_SYMBOL(get_alpm_mode);

void set_alpm_mode_by_force(int value)
{
	g_lcd->alpm = value;
	
	return;
}
EXPORT_SYMBOL(set_alpm_mode_by_force);

static ssize_t alpm_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret;
	struct lcd_info *lcd = dev_get_drvdata(dev);

	ret = sprintf(buf, "%d\n", lcd->alpm);

	return ret;
}

static ssize_t alpm_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int rc;
	int value;
	struct lcd_info *lcd = dev_get_drvdata(dev);
	
	rc = strict_strtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;
	
	dev_info(dev, "set alpm to %d\n", value);

	switch(value) {
		case 0:
			if (lcd->alpm) {
				lcd->alpm = value;
				if (lcd->status == PANEL_STATUS_ALPM_LCD_ON) {
					s6e63j0x03_ldi_exit_alpm(lcd);
					lcd->status = PANEL_STATUS_LCDON;
					lcd->ldi_enable = 1;
					update_brightness(lcd, 1);
				}
			}
			break;
		case 1:
			lcd->alpm = value;
			break;
		default: 
			dev_err(dev, "failed to set alpm mode : %d\n", value);
			return -EINVAL;
	}

	return size;
}

static DEVICE_ATTR(alpm, 0644, alpm_show, alpm_store);
#endif


#ifdef SUPPORT_HBM
#define ELVSS_BUF_SIZE	3

static int set_hbm_mode(struct lcd_info *lcd)
{
	int ret = 0;
	u8 buf[ELVSS_BUF_SIZE];
	
	dev_info(&lcd->bd->dev, "%s", __func__);	

	buf[0] = 0xD3;
	buf[1] = lcd->elvss_hbm;
	buf[2] = 0;

	s6e63j0x03_write(lcd, TEST_KEY_ON_2, ARRAY_SIZE(TEST_KEY_ON_2));

	s6e63j0x03_write(lcd, PARAM_POS_DEF_ELVSS, sizeof(PARAM_POS_DEF_ELVSS));
	s6e63j0x03_write(lcd, buf, ELVSS_BUF_SIZE);
	s6e63j0x03_write(lcd, TEST_KEY_OFF_2, ARRAY_SIZE(TEST_KEY_OFF_2));	

	s6e63j0x03_write(lcd, HBM_WHITE_BRIGHTNESS, sizeof(HBM_WHITE_BRIGHTNESS));
	s6e63j0x03_write(lcd, HBM_WHITE_CTRL, sizeof(HBM_WHITE_CTRL));

	s6e63j0x03_write(lcd, PARAM_POS_DEFAULT, sizeof(PARAM_POS_DEFAULT));

	lcd->hbm = 1;
	return ret;
}

static int exit_hbm_mode(struct lcd_info *lcd)
{
	int ret = 0;
	u8 buf[ELVSS_BUF_SIZE];

	dev_info(&lcd->bd->dev, "%s", __func__);	

	buf[0] = 0xD3;
	buf[1] = lcd->default_hbm;
	buf[2] = 0;

	s6e63j0x03_write(lcd, DEFAULT_WHITE_BRIGHTNESS, sizeof(DEFAULT_WHITE_BRIGHTNESS));
	s6e63j0x03_write(lcd, DEFAULT_WHITE_CTRL, sizeof(DEFAULT_WHITE_CTRL));

	s6e63j0x03_write(lcd, TEST_KEY_ON_2, ARRAY_SIZE(TEST_KEY_ON_2));

	s6e63j0x03_write(lcd, PARAM_POS_DEF_ELVSS, sizeof(PARAM_POS_DEF_ELVSS));
	s6e63j0x03_write(lcd, buf, ELVSS_BUF_SIZE);
	s6e63j0x03_write(lcd, TEST_KEY_OFF_2, ARRAY_SIZE(TEST_KEY_OFF_2));	

	s6e63j0x03_write(lcd, PARAM_POS_DEFAULT, sizeof(PARAM_POS_DEFAULT));	
	
	lcd->hbm = 0;
	return ret;
}


static ssize_t hbm_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret = 0;
	struct lcd_info *lcd = dev_get_drvdata(dev);

	return ret;
}

static ssize_t hbm_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int rc;
	int value;
	struct lcd_info *lcd = dev_get_drvdata(dev);
	
	rc = strict_strtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;

	dev_info(dev, "set hbm to %d\n", value);
	
	if (value)
		set_hbm_mode(lcd);
	else 
		exit_hbm_mode(lcd);

	return size;
}

static DEVICE_ATTR(hbm, 0644, hbm_show, hbm_store);
#endif

#ifdef SUPPORT_ACL
#define MAX_ACL_CNT 3

const unsigned char *acl_tbl[MAX_ACL_CNT] = {
	ACL_16,
	ACL_21,
	ACL_25,
};

static int update_acl(struct lcd_info *lcd, int acl)
{

	if (acl == 0) {
		s6e63j0x03_write(lcd, ACL_OFF, sizeof(ACL_OFF));
	} 
    else if (acl == 1){
		printk("set acl 16 \n");
		s6e63j0x03_write(lcd, TEST_KEY_ON_2, ARRAY_SIZE(TEST_KEY_ON_2));
		s6e63j0x03_write(lcd, ACL_SEL_LOOK, sizeof(ACL_SEL_LOOK));
		s6e63j0x03_write(lcd, ACL_16, sizeof(ACL_16));
		s6e63j0x03_write(lcd, ACL_GLOBAL_PARAM, sizeof(ACL_GLOBAL_PARAM));
		s6e63j0x03_write(lcd, ACL_LUT_ENABLE, sizeof(ACL_LUT_ENABLE));
		s6e63j0x03_write(lcd, ACL_ON, sizeof(ACL_ON));
		s6e63j0x03_write(lcd, TEST_KEY_OFF_2, ARRAY_SIZE(TEST_KEY_OFF_2));
	} else if (acl == 2) {
		printk("set acl 21 \n");
		s6e63j0x03_write(lcd, TEST_KEY_ON_2, ARRAY_SIZE(TEST_KEY_ON_2));
		s6e63j0x03_write(lcd, ACL_SEL_LOOK, sizeof(ACL_SEL_LOOK));
		s6e63j0x03_write(lcd, ACL_21, sizeof(ACL_21));
		s6e63j0x03_write(lcd, ACL_GLOBAL_PARAM, sizeof(ACL_GLOBAL_PARAM));
		s6e63j0x03_write(lcd, ACL_LUT_ENABLE, sizeof(ACL_LUT_ENABLE));	
		s6e63j0x03_write(lcd, ACL_ON, sizeof(ACL_ON));
		s6e63j0x03_write(lcd, TEST_KEY_OFF_2, ARRAY_SIZE(TEST_KEY_OFF_2));	

	} else if (acl == 3) {
		printk("set acl 25 \n");
		s6e63j0x03_write(lcd, TEST_KEY_ON_2, ARRAY_SIZE(TEST_KEY_ON_2));
		s6e63j0x03_write(lcd, ACL_SEL_LOOK, sizeof(ACL_SEL_LOOK));
		s6e63j0x03_write(lcd, ACL_25, sizeof(ACL_25));
		s6e63j0x03_write(lcd, ACL_GLOBAL_PARAM, sizeof(ACL_GLOBAL_PARAM));
		s6e63j0x03_write(lcd, ACL_LUT_ENABLE, sizeof(ACL_LUT_ENABLE));
		s6e63j0x03_write(lcd, ACL_ON, sizeof(ACL_ON));
		s6e63j0x03_write(lcd, TEST_KEY_OFF_2, ARRAY_SIZE(TEST_KEY_OFF_2));	
	} else {
		dev_err(&lcd->bd->dev, "%s exceed acl count : %d\n", __func__ ,acl);
		return -1;
	}

	return 0;
}

static ssize_t acl_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret = 0;
	struct lcd_info *lcd = dev_get_drvdata(dev);

	return lcd->acl;
}

static ssize_t acl_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int rc;
	int value, ret;
	struct lcd_info *lcd = dev_get_drvdata(dev);
	
	rc = strict_strtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;

	dev_info(dev, "set acl to %d\n", value);
	
	update_acl(lcd, value);

	return size;
}

static DEVICE_ATTR(acl, 0644, acl_show, acl_store);
#endif

#ifdef FIMD_OVE_TUNE
static ssize_t ove_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret = 0;
	struct lcd_info *lcd = dev_get_drvdata(dev);

	return ret;
}

static ssize_t ove_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int rc;
	int value;
	struct lcd_info *lcd = dev_get_drvdata(dev);
	


	rc = strict_strtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;

	dev_info(dev, "set ove to %d\n", value);
	
	if (value)
		s3cfb_enable_ove_tune(1);
	else 
		s3cfb_enable_ove_tune(0);

	return size;
}

static DEVICE_ATTR(ove, 0644, ove_show, ove_store);
#endif


#ifdef CONFIG_HAS_EARLYSUSPEND

void s6e63j0x03_early_suspend(void)
{
	struct lcd_info *lcd = g_lcd;
	int err = 0;

#ifdef SUPPORT_ESD_DET
	s3c_gpio_cfgpin(GPIO_OLED_DET, S3C_GPIO_SFN(0x00));
#endif

#ifdef FIMD_OVE_TUNE
	lcd->ove_mode = 0;
#endif

	set_dsim_lcd_enabled(0);

	dev_info(&lcd->ld->dev, "+%s\n", __func__);

	s6e63j0x03_ldi_power(lcd, FB_BLANK_POWERDOWN);

	dev_info(&lcd->ld->dev, "-%s\n", __func__);

	return ;
}

void s6e63j0x03_late_resume(void)
{
	struct lcd_info *lcd = g_lcd;

	dev_info(&lcd->ld->dev, "+%s\n", __func__);
	
	s6e63j0x03_ldi_power(lcd, FB_BLANK_UNBLANK);

	set_dsim_lcd_enabled(1);

#ifdef SUPPORT_ESD_DET
	s3c_gpio_cfgpin(GPIO_OLED_DET, S3C_GPIO_SFN(0xf));
#endif

	dev_info(&lcd->ld->dev, "-%s\n", __func__);
	
	return ;
}
#endif

int get_panel_status(void)
{
	if (!g_lcd)
		return 0;

#ifdef SUPPORT_ESD_DET	
	if ((g_lcd->status == PANEL_STATUS_RESET) || 
			(g_lcd->status == PANEL_STATUS_LCDOFF))
		return 0;
	else
#endif
		return 1;
}

EXPORT_SYMBOL(get_panel_status);



#ifdef SUPPORT_ESD_DET
extern void s5p_dsim_clear_frame_req(void);
extern void s3cfb_clear_draw_req(void);

void panel_reset(void)
{
	struct lcd_info *lcd;

	lcd = g_lcd;
	if (!lcd)
		return;

	dev_info(&lcd->ld->dev, "%s was called\n", __func__);

	lcd->status = PANEL_STATUS_RESET;

	s5p_dsim_clear_frame_req();
	s3cfb_clear_draw_req();

	s6e63j0x03_early_suspend();
	s5p_dsim_early_suspend();

	msleep(300);

	s5p_dsim_late_resume();
	s6e63j0x03_late_resume();
}
EXPORT_SYMBOL(panel_reset);


static void oled_detect_work(struct work_struct *work)
{
	struct lcd_info *lcd = container_of(work, struct lcd_info, oled_work);

	dev_info(&lcd->ld->dev, "%s was called\n", __func__);
	
	lcd->status = PANEL_STATUS_RESET;

	s5p_dsim_clear_frame_req();
	s3cfb_clear_draw_req();

	s6e63j0x03_early_suspend();
	s5p_dsim_early_suspend();

	msleep(300);

	s5p_dsim_late_resume();
	s6e63j0x03_late_resume();
	
	return;
}

static irqreturn_t oled_det_irq(int irq, void *param)
{
	struct lcd_info *lcd = (struct lcd_info *)param;

	dev_info(&lcd->ld->dev, "%s\n", __func__);
	
	s3c_gpio_cfgpin(GPIO_OLED_DET, S3C_GPIO_SFN(0x00));

	if (lcd->ldi_enable == 1) {
		lcd->ldi_enable = 0;
		schedule_work(&lcd->oled_work);
	}

	return IRQ_HANDLED;
}

static ssize_t det_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret = 0;
	struct lcd_info *lcd = dev_get_drvdata(dev);

	return ret;
}


static ssize_t det_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int rc;
	int value;
	struct lcd_info *lcd = dev_get_drvdata(dev);
	
	rc = strict_strtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;

	dev_info(dev, "test oled det to %d\n", value);

	if (lcd->status == PANEL_STATUS_LCDON) {
		lcd->status = PANEL_STATUS_RESET;

		s5p_dsim_clear_frame_req();
		s3cfb_clear_draw_req();

		s6e63j0x03_early_suspend();
		s5p_dsim_early_suspend();

		msleep(300);

		s5p_dsim_late_resume();
		s6e63j0x03_late_resume();

	}
	return size;
}

static DEVICE_ATTR(det, 0644, det_show, det_store);
#endif

static int s6e63j0x03_probe(struct device *dev)
{
	int ret = 0;
	struct lcd_info *lcd;
	u8 temp_buf;
	int i, j;

	lcd = kzalloc(sizeof(struct lcd_info), GFP_KERNEL);
	if (!lcd) {
		pr_err("failed to allocate for lcd\n");
		ret = -ENOMEM;
		goto err_alloc;
	}

	g_lcd = lcd;

#ifdef CONFIG_S6E63J0X03_SMART_DIMMING
	lcd->dimming = kzalloc(sizeof(struct dim_data), GFP_KERNEL);
	if (!lcd->dimming) {
		pr_err("failed to allocate for dimming\n");
		ret = -ENOMEM;
		goto out_free_lcd;
	} 
	for (i = 0; i < MAX_GAMMA_CNT; i++) {
		lcd->gamma_tbl[i] = 
			(unsigned char *)kzalloc(sizeof(unsigned char) * GAMMA_ELEMENT_CNT, GFP_KERNEL);
		if (!lcd->gamma_tbl[i]) {
			pr_err("failed to allocate for gamma_tbl\n");
			ret = -ENOMEM;
			goto out_free_lcd;
		}
	}
#endif

	lcd->ld = lcd_device_register("panel", dev, lcd, &s6e63j0x03_lcd_ops);
	if (IS_ERR(lcd->ld)) {
		pr_err("failed to register lcd device\n");
		ret = PTR_ERR(lcd->ld);
		goto out_free_lcd;
	}

	lcd->bd = backlight_device_register("panel", dev, lcd, &s6e63j0x03_backlight_ops, NULL);
	if (IS_ERR(lcd->bd)) {
		pr_err("failed to register backlight device\n");
		ret = PTR_ERR(lcd->bd);
		goto out_free_backlight;
	}

	lcd->dev = dev;
	lcd->dsim = (struct dsim_global *)dev_get_drvdata(dev->parent);
	lcd->bd->props.max_brightness = MAX_BRIGHTNESS;
	lcd->bd->props.brightness = DEFAULT_BRIGHTNESS;
	//lcd->bl = DEFAULT_GAMMA_LEVEL;
	lcd->current_bl = lcd->bl;

	lcd->acl_enable = 0;
	lcd->siop_enable = 0;
	lcd->current_acl = 0;

	lcd->power = FB_BLANK_UNBLANK;
	lcd->ldi_enable = 0;
	lcd->connected = 1;
	lcd->auto_brightness = 0;

#ifdef CONFIG_FB_SUPPORT_ALPM
	lcd->alpm = 0;
	lcd->status = PANEL_STATUS_LCDON;
#endif

#ifdef SUPPORT_ESD_DET
	ret = gpio_request(GPIO_OLED_DET, "OLED_DET");
	if (ret) {
		pr_err("failed to request OLED_DET gpio\n");
		goto out_free_backlight;
	}
	
	ret = request_irq(gpio_to_irq(GPIO_OLED_DET), oled_det_irq,
						IRQF_TRIGGER_FALLING, "oled_det", lcd);
	if (ret) {
		pr_err("failed to request irq for oled_det\n");
		goto out_free_backlight;
	}

	INIT_WORK(&lcd->oled_work, oled_detect_work);
	s3c_gpio_cfgpin(GPIO_OLED_DET, S3C_GPIO_SFN(0xf));
	s3c_gpio_setpull(GPIO_OLED_DET, S3C_GPIO_PULL_DOWN);

#endif
	ret = device_create_file(&lcd->ld->dev, &dev_attr_power_reduce);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->ld->dev, &dev_attr_siop_enable);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->ld->dev, &dev_attr_lcd_type);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->ld->dev, &dev_attr_gamma_table);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->bd->dev, &dev_attr_auto_brightness);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

#ifdef CONFIG_FB_SUPPORT_ALPM
	ret = device_create_file(&lcd->ld->dev, &dev_attr_alpm);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);
#endif

#ifdef SUPPORT_HBM
	ret = device_create_file(&lcd->ld->dev, &dev_attr_hbm);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);
#endif

#ifdef FIMD_OVE_TUNE
	ret = device_create_file(&lcd->ld->dev, &dev_attr_ove);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);
#endif

#ifdef SUPPORT_ACL
	ret = device_create_file(&lcd->ld->dev, &dev_attr_acl);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

#endif
	dev_set_drvdata(dev, lcd);

	mutex_init(&lcd->lock);
	mutex_init(&lcd->bl_lock);
	lcd->ldi_enable = 1;

	
	ret = s6e63j0x03_get_id(lcd, lcd->id);
	if (!ret)
		dev_info(&lcd->ld->dev, "ID: %x, %x, %x\n", lcd->id[0], lcd->id[1], lcd->id[2]);

#ifdef SUPPORT_HBM
	if (lcd->connected)
		s6e63j0x03_set_hbm_param(lcd);
#endif

#ifdef CONFIG_S6E63J0X03_SMART_DIMMING
	if (lcd->connected)
		s6e63j0x03_get_mtp(lcd);
	else 
		read_mtp(lcd->dimming, dft_ref_gamma);
	
	ret = generate_volt_table(lcd->dimming);
	if (ret) {
		dev_err(&lcd->ld->dev, "%s:failed to generate volt table\n", __func__);
		goto out_free_backlight;
	}

	for (i = 0; i < MAX_GAMMA_CNT; i++) {
		lcd->gamma_tbl[i][0] = LDI_MTP_ADDR;
		get_gamma(lcd->dimming, cd_tbl[i], &lcd->gamma_tbl[i][1]);
	}
#endif

	dev_info(&lcd->ld->dev, "s6e8aa0 lcd panel driver has been probed.\n");

	lcd_early_suspend = s6e63j0x03_early_suspend;
	lcd_late_resume = s6e63j0x03_late_resume;

	/*
	s6e63j0x03_ldi_power_on(lcd);
	*/
	return 0;

out_free_backlight:
	lcd_device_unregister(lcd->ld);

out_free_lcd:
#ifdef CONFIG_S6E63J0X03_SMART_DIMMING
	if (lcd->dimming != NULL)
		kfree(lcd->dimming);
	for(i = 0; i < MAX_GAMMA_CNT; i++)
		if (lcd->gamma_tbl[i])
			kfree(lcd->gamma_tbl[i]);
#endif
	if (lcd != NULL)
		kfree(lcd);
err_alloc:
	return ret;
}

static int __devexit s6e63j0x03_remove(struct device *dev)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	s6e63j0x03_ldi_power(lcd, FB_BLANK_POWERDOWN);
	lcd_device_unregister(lcd->ld);
	backlight_device_unregister(lcd->bd);
	kfree(lcd);

	return 0;
}

/* Power down all displays on reboot, poweroff or halt. */
static void s6e63j0x03_shutdown(struct device *dev)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	dev_info(&lcd->ld->dev, "%s\n", __func__);

#ifdef SUPPORT_ESD_DET
	s3c_gpio_cfgpin(GPIO_OLED_DET, S3C_GPIO_SFN(0x0));
#endif

	s6e63j0x03_ldi_power(lcd, FB_BLANK_POWERDOWN);
}

static struct mipi_lcd_driver s6e63j0x03_mipi_driver = {
	.name 			= "s6e63j0x03",
	.probe			= s6e63j0x03_probe,
	.remove			= __devexit_p(s6e63j0x03_remove),
	.shutdown		= s6e63j0x03_shutdown,
};

static int s6e63j0x03_init(void)
{
	return s5p_dsim_register_lcd_driver(&s6e63j0x03_mipi_driver);
}

static void s6e63j0x03_exit(void)
{
	return;
}

module_init(s6e63j0x03_init);
module_exit(s6e63j0x03_exit);

MODULE_DESCRIPTION("MIPI-DSI S6E8AA0: AMS529HA01 (800x1280) / AMS480GYXX (720x1280) Panel Driver");
MODULE_LICENSE("GPL");
