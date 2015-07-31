/* linux/drivers/video/backlight/s6e8aa4_mipi_lcd.c
 *
 * Samsung SoC MIPI LCD driver.
 *
 * Copyright (c) 2012 Samsung Electronics
 *
 * Haowei Li, <haowei.li@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/delay.h>
#include <linux/gpio.h>
#include <video/mipi_display.h>

#include <plat/dsim.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <plat/mipi_dsi.h>
#include <plat/gpio-cfg.h>

#include <linux/proc_fs.h>
#define PROC_DIR	"lcdsettings/"
struct proc_dir_entry *display_dir;

#define GAMMA_PARAM_SIZE 26
#define MAX_BRIGHTNESS 255
#define MIN_BRIGHTNESS 0
#define DEFAULT_BRIGHTNESS 0

static struct mipi_dsim_device *dsim_base;
static struct backlight_device *bd;
int alpmMode=0;
int procfsAmbientBrightness=2;
int procfsPartialMode=0;
int procfsIdleMode=0;


#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend    s6e63j0x03_early_suspend;
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
	0x02, 0x00
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
	0xFF, 0x00
};

static const unsigned char DEFAULT_WHITE_CTRL[] = {
	0x53,
	0x20, 0x00
};

static const unsigned char ACL_OFF[] = {
	0x55,
	0x00, 0x00
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
	0x40, 0x00
};


static const unsigned char DISPLAY_ON[] = {
	0x29,
	0x00, 0x00
};

/*** My amazing brightness levels ;) ***/
int SavedBrightness=128; // default to 128...
#define MCS_MTP_SET3		0xd4
static const unsigned char GAMMA_10[] = {
	MCS_MTP_SET3,
	0x00, 0x00, 0x00, 0x7f, 0x7f, 0x7f, 0x52, 0x6b, 0x6f, 0x26, 0x28, 0x2d,
	0x28, 0x26, 0x27, 0x33, 0x34, 0x32, 0x36, 0x36, 0x35, 0x00, 0xab, 0x00,
	0xae, 0x00, 0xbf
};

static const unsigned char GAMMA_30[] = {
	MCS_MTP_SET3,
	0x00, 0x00, 0x00, 0x70, 0x7f, 0x7f, 0x4e, 0x64, 0x69, 0x26, 0x27, 0x2a,
	0x28, 0x29, 0x27, 0x31, 0x32, 0x31, 0x35, 0x34, 0x35, 0x00, 0xc4, 0x00,
	0xca, 0x00, 0xdc
};

static const unsigned char GAMMA_60[] = {
	MCS_MTP_SET3,
	0x00, 0x00, 0x00, 0x65, 0x7b, 0x7d, 0x5f, 0x67, 0x68, 0x2a, 0x28, 0x29,
	0x28, 0x2a, 0x27, 0x31, 0x2f, 0x30, 0x34, 0x33, 0x34, 0x00, 0xd9, 0x00,
	0xe4, 0x00, 0xf5
};

static const unsigned char GAMMA_90[] = {
	MCS_MTP_SET3,
	0x00, 0x00, 0x00, 0x4d, 0x6f, 0x71, 0x67, 0x6a, 0x6c, 0x29, 0x28, 0x28,
	0x28, 0x29, 0x27, 0x30, 0x2e, 0x30, 0x32, 0x31, 0x31, 0x00, 0xea, 0x00,
	0xf6, 0x01, 0x09
};

static const unsigned char GAMMA_120[] = {
	MCS_MTP_SET3,
	0x00, 0x00, 0x00, 0x3d, 0x66, 0x68, 0x69, 0x69, 0x69, 0x28, 0x28, 0x27,
	0x28, 0x28, 0x27, 0x30, 0x2e, 0x2f, 0x31, 0x31, 0x30, 0x00, 0xf9, 0x01,
	0x05, 0x01, 0x1b
};

static const unsigned char GAMMA_150[] = {
	MCS_MTP_SET3,
	0x00, 0x00, 0x00, 0x31, 0x51, 0x53, 0x66, 0x66, 0x67, 0x28, 0x29, 0x27,
	0x28, 0x27, 0x27, 0x2e, 0x2d, 0x2e, 0x31, 0x31, 0x30, 0x01, 0x04, 0x01,
	0x11, 0x01, 0x29
};

static const unsigned char GAMMA_200[] = {
	MCS_MTP_SET3,
	0x00, 0x00, 0x00, 0x2f, 0x4f, 0x51, 0x67, 0x65, 0x65, 0x29, 0x2a, 0x28,
	0x27, 0x25, 0x26, 0x2d, 0x2c, 0x2c, 0x30, 0x30, 0x30, 0x01, 0x14, 0x01,
	0x23, 0x01, 0x3b
};

static const unsigned char GAMMA_240[] = {
	MCS_MTP_SET3,
	0x00, 0x00, 0x00, 0x2c, 0x4d, 0x50, 0x65, 0x63, 0x64, 0x2a, 0x2c, 0x29,
	0x26, 0x24, 0x25, 0x2c, 0x2b, 0x2b, 0x30, 0x30, 0x30, 0x01, 0x1e, 0x01,
	0x2f, 0x01, 0x47
};

static const unsigned char GAMMA_300[] = {
	MCS_MTP_SET3,
	0x00, 0x00, 0x00, 0x38, 0x61, 0x64, 0x65, 0x63, 0x64, 0x28, 0x2a, 0x27,
	0x26, 0x23, 0x25, 0x2b, 0x2b, 0x2a, 0x30, 0x2f, 0x30, 0x01, 0x2d, 0x01,
	0x3f, 0x01, 0x57
};
static const unsigned char WHITE_CTRL[] = {
	0x53,
	0x20, 0x00
};

/** Amoled Low Power Mode Enable **/
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
static const unsigned char FRAME_FREQ_60HZ_SET[] = {
	0xB5,
	0x00, 0x01, 0x00
};

static const unsigned char FRAME_FREQ_30HZ_SET[] = {
	0xB5,
	0x00, 0x02, 0x00
};
/************** DISPLAY CODE *************/

static int s6e63j0x03_get_brightness(struct backlight_device *bd)
{
	return bd->props.brightness;
}

static int get_backlight_level(int brightness)
{

	return brightness;
}

/** ALPM Enable - Disable **/
void s6e63j0x03_alpm_control(struct mipi_dsim_device *dsim, bool state)
{
	if (state)	{
	if (procfsPartialMode){
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,	PARTIAL_AREA_SET, ARRAY_SIZE(PARTIAL_AREA_SET)) == -1)
		printk ("s6e63j0x03 Suspend: Error in Partial Area Set\n");
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,	PARTIAL_MODE_ON, ARRAY_SIZE(PARTIAL_MODE_ON)) == -1)
		printk ("s6e63j0x03 Suspend: Error activating partial mode\n");
	}
	if (procfsIdleMode){
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,	IDLE_MODE_ON, ARRAY_SIZE(IDLE_MODE_ON)) == -1)
		printk ("s6e63j0x03 Suspend: Error setting IDLE mode to ON\n");
	}
	switch (procfsAmbientBrightness)
	{
		case 1:
		if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,	GAMMA_10, ARRAY_SIZE(GAMMA_30)) == -1)
				dev_err(dsim->dev, "fail to send panel_condition_set command.\n");
		break;
		case 2:
		if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,	GAMMA_30, ARRAY_SIZE(GAMMA_30)) == -1)
				dev_err(dsim->dev, "fail to send panel_condition_set command.\n");
		break;	
		case 3:
		if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,	GAMMA_60, ARRAY_SIZE(GAMMA_30)) == -1)
				dev_err(dsim->dev, "fail to send panel_condition_set command.\n");
		break;
	}
	
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,	FRAME_FREQ_30HZ_SET,ARRAY_SIZE(FRAME_FREQ_30HZ_SET)) == -1)
		printk ("s6e63j0x03 Suspend: Error Setting Freq to 30Hz");
	printk ("s6e63j0x03 - ALPM mode enabled\n");
	alpmMode=1;
	}
	else{
	if (SavedBrightness>10)
		{
		if (procfsIdleMode){
			if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,	IDLE_MODE_OFF, ARRAY_SIZE(IDLE_MODE_OFF)) == -1)
				printk ("s6e63j0x03 Resume: Error deactivating idle mode\n");
		}
		if (procfsPartialMode){
			if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,	NORMAL_MODE_ON, ARRAY_SIZE(NORMAL_MODE_ON)) == -1)
				printk ("s6e63j0x03 Resume: Error reactivating normal mode\n");
		}
		if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,	FRAME_FREQ_60HZ_SET,ARRAY_SIZE(FRAME_FREQ_30HZ_SET)) == -1)
			printk ("s6e63j0x03 Resume: Error resetting frame freq to 60hz\n");
		
		printk ("s6e63j0x03 - ALPM mode disabled\n");
		alpmMode=0;
		}
	}
}


static int update_brightness(int brightness)
{
	int backlightlevel;
	struct mipi_dsim_device *dsim;
	dsim = dsim_base;
	backlightlevel = get_backlight_level(brightness);
	// First we activate the test mode for the LCD, then we send the command
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,	TEST_KEY_ON_2, ARRAY_SIZE(TEST_KEY_ON_2)) == -1) 
			dev_err(dsim->dev, "fail to send panel_condition_set command.\n");
	if (SavedBrightness==10 && brightness>10)
		{
		SavedBrightness=brightness;
		s6e63j0x03_alpm_control(dsim,0);
		}
	else
		SavedBrightness=brightness; 
	switch (backlightlevel)	{
		case 0 ... 51:
		if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,	GAMMA_30, ARRAY_SIZE(GAMMA_30)) == -1)
					dev_err(dsim->dev, "fail to send panel_condition_set command.\n");
		break;
		case 52 ... 103:
		if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,	GAMMA_60, ARRAY_SIZE(GAMMA_60)) == -1)
					dev_err(dsim->dev, "fail to send panel_condition_set command.\n");
		break;
		case 104 ... 155:
		if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,	GAMMA_120, ARRAY_SIZE(GAMMA_120)) == -1)
					dev_err(dsim->dev, "fail to send panel_condition_set command.\n");
		break;
		case 156 ... 207:
		if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,	GAMMA_200, ARRAY_SIZE(GAMMA_200)) == -1)
					dev_err(dsim->dev, "fail to send panel_condition_set command.\n");
		break;
		case 208 ... 255:
		if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,	GAMMA_300, ARRAY_SIZE(GAMMA_300)) == -1)
					dev_err(dsim->dev, "fail to send panel_condition_set command.\n");		
		break;
	} // switch close
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE, TEST_KEY_OFF_2, ARRAY_SIZE(TEST_KEY_OFF_2)) == -1)
					dev_err(dsim->dev, "fail to send panel_condition_set command.\n");
	return 1;
}

static int s6e63j0x03_set_brightness(struct backlight_device *bd)
{
	int brightness = bd->props.brightness;
	if (brightness < MIN_BRIGHTNESS || brightness > MAX_BRIGHTNESS) {
		printk(KERN_ALERT "Brightness should be in the range of 0 ~ 255\n");
		return -EINVAL;
	}
	update_brightness(brightness);
	return 1;
}

static const struct backlight_ops s6e63j0x03_backlight_ops = {
	.get_brightness = s6e63j0x03_get_brightness,
	.update_status = s6e63j0x03_set_brightness,
};

/* PROCFS ALPM MODE */
static ssize_t s6e63j0x03_read_proc_ambientbrightness(struct file *file, char __user *userbuf,size_t bytes, loff_t *off)
{
	return procfsAmbientBrightness;
}

static ssize_t s6e63j0x03_write_proc_ambientbrightness(struct file *file, const char __user *buffer,size_t count, loff_t *pos)
{
	printk ("s6e63j0x03: brightness mode change called, %c\n", buffer[0]);
	if (buffer[0]=='1')
		procfsAmbientBrightness=1;
	else if (buffer[0]=='2')
		procfsAmbientBrightness=2;
	else if (buffer[0]=='3')
		procfsAmbientBrightness=3;
	return count;
}
static const struct file_operations proc_fops_alpm = {
	.owner = THIS_MODULE,
	.read = s6e63j0x03_read_proc_ambientbrightness,
	.write = s6e63j0x03_write_proc_ambientbrightness,
};
/* IDLE MODE */
static ssize_t s6e63j0x03_read_proc_idlemode(struct file *file, char __user *userbuf,size_t bytes, loff_t *off)
{
	return procfsIdleMode;
}

static ssize_t s6e63j0x03_write_proc_idlemode(struct file *file, const char __user *buffer,size_t count, loff_t *pos)
{
	printk ("s6e63j0x03: Idle mode change called, %c\n", buffer[0]);
	if (buffer[0]=='1')
		procfsIdleMode=1;
	else
		procfsIdleMode=0;
	
	return count;
}

static const struct file_operations proc_fops_idlemode = {
	.owner = THIS_MODULE,
	.read = s6e63j0x03_read_proc_idlemode,
	.write = s6e63j0x03_write_proc_idlemode,
};
/* PARTIAL MODE */
static ssize_t s6e63j0x03_read_proc_partialmode(struct file *file, char __user *userbuf,size_t bytes, loff_t *off)
{
	return procfsPartialMode;
}

static ssize_t s6e63j0x03_write_proc_partialmode	(struct file *file, const char __user *buffer,size_t count, loff_t *pos)
{
	printk ("s6e63j0x03: Partial mode change called, %c\n", buffer[0]);
	if (buffer[0]=='1')
		procfsPartialMode=1;
	else
		procfsPartialMode=0;
	return count;
}

static const struct file_operations proc_fops_partialmode = {
	.owner = THIS_MODULE,
	.read = s6e63j0x03_read_proc_partialmode,
	.write = s6e63j0x03_write_proc_partialmode,
};
static int s6e63j0x03_probe(struct mipi_dsim_device *dsim)
{
	int retval;
	struct proc_dir_entry *ent;
	dsim_base = dsim;
	printk("%s was called\n", __func__);
	
	display_dir = proc_mkdir("lcdsettings", NULL);
	if (display_dir == NULL) {
		printk("s6e63j0x03 Error:Unable to create /proc/lcdsettings directory");
		return -ENOMEM;
	}
	ent = proc_create("ambient_brightness", 0, display_dir, &proc_fops_alpm);
	if (ent == NULL) {
		printk("[BT] Error:Unable to create /proc/%s/ambient_brightness entry", PROC_DIR);
		retval = -ENOMEM;
		goto fail;
	}
	ent = proc_create("idlemode", 0, display_dir, &proc_fops_idlemode);
	if (ent == NULL) {
		printk("s6e63j0x03 Error:Unable to create /proc/%s/idlemode entry", PROC_DIR);
		retval = -ENOMEM;
		goto fail;
	}
	ent = proc_create("partialmode", 0, display_dir, &proc_fops_partialmode);
	if (ent == NULL) {
		printk("s6e63j0x03 Error:Unable to create /proc/%s/partialmode entry", PROC_DIR);
		retval = -ENOMEM;
		goto fail;
	}


	bd = backlight_device_register("panel", NULL,
		NULL, &s6e63j0x03_backlight_ops, NULL);
	if (IS_ERR(bd))
		printk(KERN_ALERT "failed to register backlight device!\n");

	bd->props.max_brightness = MAX_BRIGHTNESS;
	bd->props.brightness = DEFAULT_BRIGHTNESS;

	return 1;
fail:
	remove_proc_entry("alpm", display_dir);
	remove_proc_entry("idlemode", display_dir);
	remove_proc_entry("partialmode", display_dir);
	remove_proc_entry("lcdsettings", 0);
	return retval;
	}

static void init_lcd(struct mipi_dsim_device *dsim)
{
	printk("%s was called\n", __func__);

	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,	TEST_KEY_ON_1, ARRAY_SIZE(TEST_KEY_ON_1)) == -1)
	dev_err(dsim->dev, "fail to send panel_condition_set command.\n");

	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,	TEST_KEY_ON_2, ARRAY_SIZE(TEST_KEY_ON_2)) == -1)
	dev_err(dsim->dev, "fail to send panel_condition_set command.\n");

	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,	PORCH_ADJUSTMENT, ARRAY_SIZE(PORCH_ADJUSTMENT)) == -1)
	dev_err(dsim->dev, "fail to send panel_condition_set command.\n");

	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,	FRAME_FREQ_SET, ARRAY_SIZE(FRAME_FREQ_SET)) == -1)
	dev_err(dsim->dev, "fail to send panel_condition_set command.\n");
	
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,	MEM_ADDR_SET_0, ARRAY_SIZE(MEM_ADDR_SET_0)) == -1)
	dev_err(dsim->dev, "fail to send panel_condition_set command.\n");
	
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,	MEM_ADDR_SET_1, ARRAY_SIZE(MEM_ADDR_SET_1)) == -1)
	dev_err(dsim->dev, "fail to send panel_condition_set command.\n");

	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,	LPTS_TIMMING_SET_0, ARRAY_SIZE(LPTS_TIMMING_SET_0)) == -1)
	dev_err(dsim->dev, "fail to send panel_condition_set command.\n");
	
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,	LPTS_TIMMING_SET_1, ARRAY_SIZE(LPTS_TIMMING_SET_1)) == -1)
	dev_err(dsim->dev, "fail to send panel_condition_set command.\n");

	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,	SLEEP_OUT, ARRAY_SIZE(SLEEP_OUT)) == -1)
	dev_err(dsim->dev, "fail to send panel_condition_set command.\n");

	usleep_range(120000, 120000);

		if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,	DEFAULT_WHITE_BRIGHTNESS, ARRAY_SIZE(DEFAULT_WHITE_BRIGHTNESS)) == -1)
			dev_err(dsim->dev, "fail to send panel_condition_set command.\n");
		if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,	DEFAULT_WHITE_CTRL, ARRAY_SIZE(DEFAULT_WHITE_CTRL)) == -1)
			dev_err(dsim->dev, "fail to send panel_condition_set command.\n");
		if (update_brightness(SavedBrightness)== 0)
			printk ("s6e63j0x03 - DisplayON: Set brightness to saved value");

	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,	ACL_OFF, ARRAY_SIZE(ACL_OFF)) == -1)
	dev_err(dsim->dev, "fail to send panel_condition_set command.\n");

	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,	ELVSS_COND, ARRAY_SIZE(ELVSS_COND)) == -1)
	dev_err(dsim->dev, "fail to send panel_condition_set command.\n");
	
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,	SET_POS, ARRAY_SIZE(SET_POS)) == -1)
	dev_err(dsim->dev, "fail to send panel_condition_set command.\n");

	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,	TE_ON, ARRAY_SIZE(TE_ON)) == -1)
	dev_err(dsim->dev, "fail to send panel_condition_set command.\n");

	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,	TEST_KEY_OFF_2, ARRAY_SIZE(TEST_KEY_OFF_2)) == -1)
	dev_err(dsim->dev, "fail to send panel_condition_set command.\n");

	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,	DISPLAY_ON, ARRAY_SIZE(DISPLAY_ON)) == -1)
	dev_err(dsim->dev, "fail to send panel_condition_set command.\n");

	}

static int s6e63j0x03_displayon(struct mipi_dsim_device *dsim)
{
	printk("%s was called\n", __func__);
	init_lcd(dsim);
	return 1; 
}
static int s6e63j0x03_suspend(struct mipi_dsim_device *dsim)
{
	return 1;
}

static int s6e63j0x03_resume(struct mipi_dsim_device *dsim)
{
	return 1;
}

struct mipi_dsim_lcd_driver s6e63j0x03_mipi_lcd_driver = {
	.probe		= s6e63j0x03_probe,
	.displayon	= s6e63j0x03_displayon,
	.suspend	= s6e63j0x03_suspend,
	.resume		= s6e63j0x03_resume,
	.alpm		= s6e63j0x03_alpm_control,
	};
