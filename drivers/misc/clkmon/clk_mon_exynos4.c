
/*
 * driver/misc/clkmon/clk_mon.c
 *
 * Copyright (C) 2012 Samsung Electronics co. ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/io.h>

#include <linux/clk_mon.h>
#if defined(CONFIG_ARCH_EXYNOS4)
#include "clk_mon_s5p.h"
#endif
#include "clk_mon_ioctl.h"

struct power_domain_mask power_domain_masks[] = {
	{"CAM ", 0x7, CLK_MON_PMU_CAM_CONF},
	{"TV  ", 0x7, CLK_MON_PMU_TV_CONF},
	{"MFC ", 0x7, CLK_MON_PMU_MFC_CONF},
	{"G3D ", 0x7, CLK_MON_PMU_G3D_CONF},
	{"LCD0", 0x7, CLK_MON_PMU_LCD0_CONF},
	{"MAU ", 0x7, CLK_MON_PMU_MAU_CONF},
	{"ISP ", 0x7, CLK_MON_PMU_ISP_CONF},
	{"GPS ", 0x7, CLK_MON_PMU_GPS_CONF},
	{"GPSA", 0x7, CLK_MON_PMU_GPS_ALIVE_CONF},
	/*  End */
	{{0}, 0, NULL},
};

static struct clk_gate_mask clk_gate_masks[] = {
	/* CLK_GATE_IP_LEFTBUS */
	{"CLK_ASYNC_G3D", (0x1 << 6), CLK_MON_CGATE_IP_LEFTBUS},
	{"CLK_ASYNC_MFCL", (0x1 << 4), CLK_MON_CGATE_IP_LEFTBUS},
	{"CLK_ASYNC_TVX", (0x1 << 3), CLK_MON_CGATE_IP_LEFTBUS},
	{"CLK_PPMULEFT", (0x1 << 1), CLK_MON_CGATE_IP_LEFTBUS},
	{"CLK_GPIO_LEFT", (0x1), CLK_MON_CGATE_IP_LEFTBUS},
	/* CLK_GATE_IP_IMAGE */
	{"CLK_PPMUIMAGE", (0x1 << 8), CLK_MON_CGATE_IP_IMAGE},
	{"CLK_SMMUMDMA", (0x1 << 5), CLK_MON_CGATE_IP_IMAGE},
	{"CLK_SMMUROTATOR", (0x1 << 4), CLK_MON_CGATE_IP_IMAGE},
	{"CLK_MDMA", (0x1 << 2), CLK_MON_CGATE_IP_IMAGE},
	{"CLK_ROTATOR", (0x1 << 1), CLK_MON_CGATE_IP_IMAGE},
	/* CLK_GATE_IP_RIGHTBUS */
	{"CLK_ASYNC_ISPMX", (0x1 << 9), CLK_MON_CGATE_IP_RIGHTBUS},
	{"CLK_ASYNC_MAUDIOX", (0x1 << 7), CLK_MON_CGATE_IP_RIGHTBUS},
	{"CLK_ASYNC_MFCR", (0x1 << 6), CLK_MON_CGATE_IP_RIGHTBUS},
	{"CLK_ASYNC_FSYSD", (0x1 << 5), CLK_MON_CGATE_IP_RIGHTBUS},
	{"CLK_ASYNC_LCD0X", (0x1 << 3), CLK_MON_CGATE_IP_RIGHTBUS},
	{"CLK_ASYNC_CAMX", (0x1 << 2), CLK_MON_CGATE_IP_RIGHTBUS},
	{"CLK_PPMURIGHT", (0x1 << 1), CLK_MON_CGATE_IP_RIGHTBUS},
	{"CLK_GPIO_RIGHT", (0x1), CLK_MON_CGATE_IP_RIGHTBUS},
	/* CLK_GATE_IP_PERIR */
	{"CLK_CMU_ISPPART", (0x1 << 18), CLK_MON_CGATE_IP_PERIR},
	{"CLK_TMU_APBIF", (0x1 << 17), CLK_MON_CGATE_IP_PERIR},
	{"CLK_KEYIF", (0x1 << 16), CLK_MON_CGATE_IP_PERIR},
	{"CLK_RTC", (0x1 << 15), CLK_MON_CGATE_IP_PERIR},
	{"CLK_WDT", (0x1 << 14), CLK_MON_CGATE_IP_PERIR},
	{"CLK_MCT", (0x1 << 13), CLK_MON_CGATE_IP_PERIR},
	{"CLK_SECKEY", (0x1 << 12), CLK_MON_CGATE_IP_PERIR},
	{"CLK_HDMI_CEC", (0x1 << 11), CLK_MON_CGATE_IP_PERIR},
	{"CLK_TZPC5", (0x1 << 10), CLK_MON_CGATE_IP_PERIR},
	{"CLK_TZPC4", (0x1 << 9), CLK_MON_CGATE_IP_PERIR},
	{"CLK_TZPC3", (0x1 << 8), CLK_MON_CGATE_IP_PERIR},
	{"CLK_TZPC2", (0x1 << 7), CLK_MON_CGATE_IP_PERIR},
	{"CLK_TZPC1", (0x1 << 6), CLK_MON_CGATE_IP_PERIR},
	{"CLK_TZPC0", (0x1 << 5), CLK_MON_CGATE_IP_PERIR},
	{"CLK_CMU_COREPART", (0x1 << 4), CLK_MON_CGATE_IP_PERIR},
	{"CLK_CMU_TOPPART", (0x1 << 3), CLK_MON_CGATE_IP_PERIR},
	{"CLK_PMU_APBIF", (0x1 << 2), CLK_MON_CGATE_IP_PERIR},
	{"CLK_SYSREG", (0x1 << 1), CLK_MON_CGATE_IP_PERIR},
	{"CLK_CHIP_ID", (0x1), CLK_MON_CGATE_IP_PERIR},
	/* CLK_GATE_IP_CAM */
	{"CLK_PIXELASYNCM1", (0x1 << 18), CLK_MON_CGATE_IP_CAM},
	{"CLK_PIXELASYNCM0", (0x1 << 17), CLK_MON_CGATE_IP_CAM},
	{"CLK_PPMUCAMIF", (0x1 << 16), CLK_MON_CGATE_IP_CAM},
	{"CLK_SMMUJPEG", (0x1 << 11), CLK_MON_CGATE_IP_CAM},
	{"CLK_SMMUFIMC3", (0x1 << 10), CLK_MON_CGATE_IP_CAM},
	{"CLK_SMMUFIMC2", (0x1 << 9), CLK_MON_CGATE_IP_CAM},
	{"CLK_SMMUFIMC1", (0x1 << 8), CLK_MON_CGATE_IP_CAM},
	{"CLK_SMMUFIMC0", (0x1 << 7), CLK_MON_CGATE_IP_CAM},
	{"CLK_JPEG", (0x1 << 6), CLK_MON_CGATE_IP_CAM},
	{"CLK_CSIS1", (0x1 << 5), CLK_MON_CGATE_IP_CAM},
	{"CLK_CSIS0", (0x1 << 4), CLK_MON_CGATE_IP_CAM},
	{"CLK_FIMC3", (0x1 << 3), CLK_MON_CGATE_IP_CAM},
	{"CLK_FIMC2", (0x1 << 2), CLK_MON_CGATE_IP_CAM},
	{"CLK_FIMC1", (0x1 << 1), CLK_MON_CGATE_IP_CAM},
	{"CLK_FIMC0", (0x1), CLK_MON_CGATE_IP_CAM},
	/* CLK_GATE_IP_TV */
	{"CLK_PPMUTV", (0x1 << 5), CLK_MON_CGATE_IP_TV},
	{"CLK_SMMUTV", (0x1 << 4), CLK_MON_CGATE_IP_TV},
	{"CLK_HDMI", (0x1 << 3), CLK_MON_CGATE_IP_TV},
	{"CLK_MIXER", (0x1 << 1), CLK_MON_CGATE_IP_TV},
	{"CLK_VP", (0x1), CLK_MON_CGATE_IP_TV},
	/* CLK_GATE_IP_MFC */
	{"CLK_PPMUMFC_R", (0x1 << 4), CLK_MON_CGATE_IP_MFC},
	{"CLK_PPMUMFC_L", (0x1 << 3), CLK_MON_CGATE_IP_MFC},
	{"CLK_SMMUMFC_R", (0x1 << 2), CLK_MON_CGATE_IP_MFC},
	{"CLK_SMMUMFC_L", (0x1 << 1), CLK_MON_CGATE_IP_MFC},
	{"CLK_MFC", (0x1 << 0), CLK_MON_CGATE_IP_MFC},
	/* CLK_GATE_IP_G3D */
	{"CLK_PPMUG3D", (0x1 << 1), CLK_MON_CGATE_IP_G3D},
	{"CLK_G3D", (0x1), CLK_MON_CGATE_IP_G3D},
	/* CLK_GATE_IP_LCD */
	{"CLK_PPMULCD0", (0x1 << 5), CLK_MON_CGATE_IP_LCD0},
	{"CLK_SMMUFIMD0", (0x1 << 4), CLK_MON_CGATE_IP_LCD0},
	{"CLK_DSIM0", (0x1 << 3), CLK_MON_CGATE_IP_LCD0},
	{"CLK_MDNIE0", (0x1 << 2), CLK_MON_CGATE_IP_LCD0},
	{"CLK_MIE0", (0x1 << 1), CLK_MON_CGATE_IP_LCD0},
	{"CLK_FIMD0", (0x1), CLK_MON_CGATE_IP_LCD0},
	/* CLK_GATE_IP_ISP */
	{"CLK_UART_ISP_SCLK", (0x1 << 3), CLK_MON_CGATE_IP_ISP},
	{"CLK_SPI1_ISP_SCLK", (0x1 << 2), CLK_MON_CGATE_IP_ISP},
	{"CLK_SPI0_ISP_SCLK", (0x1 << 1), CLK_MON_CGATE_IP_ISP},
	{"CLK_PWM_ISP_SCLK", (0x1), CLK_MON_CGATE_IP_ISP},
	/* CLK_GATE_IP_FSYS */
	{"CLK_PPMUFILE", (0x1 << 17), CLK_MON_CGATE_IP_FSYS},
	{"CLK_NFCON", (0x1 << 16), CLK_MON_CGATE_IP_FSYS},
	{"CLK_ONENAND", (0x1 << 15), CLK_MON_CGATE_IP_FSYS},
	{"CLK_USBDEVICE", (0x1 << 13), CLK_MON_CGATE_IP_FSYS},
	{"CLK_USBHOST", (0x1 << 12), CLK_MON_CGATE_IP_FSYS},
	{"CLK_SROMC", (0x1 << 11), CLK_MON_CGATE_IP_FSYS},
	{"CLK_MIPIHSI", (0x1 << 10), CLK_MON_CGATE_IP_FSYS},
	{"CLK_SDMMC4", (0x1 << 9), CLK_MON_CGATE_IP_FSYS},
	{"CLK_SDMMC3", (0x1 << 8), CLK_MON_CGATE_IP_FSYS},
	{"CLK_SDMMC2", (0x1 << 7), CLK_MON_CGATE_IP_FSYS},
	{"CLK_SDMMC1", (0x1 << 6), CLK_MON_CGATE_IP_FSYS},
	{"CLK_SDMMC0", (0x1 << 5), CLK_MON_CGATE_IP_FSYS},
	{"CLK_TSI", (0x1 << 4), CLK_MON_CGATE_IP_FSYS},
	{"CLK_PDMA1", (0x1 << 1), CLK_MON_CGATE_IP_FSYS},
	{"CLK_PDMA0", (0x1), CLK_MON_CGATE_IP_FSYS},
	/* CLK_GATE_IP_GPS */
	{"CLK_PPMUGPS", (0x1 << 2), CLK_MON_CGATE_IP_GPS},
	{"CLK_SMMUGPS", (0x1 << 1), CLK_MON_CGATE_IP_GPS},
	{"CLK_GPS", (0x1), CLK_MON_CGATE_IP_GPS},
	/* CLK_GATE_IP_PERIL */
	{"CLK_AC97", (0x1 << 27), CLK_MON_CGATE_IP_PERIL},
	{"CLK_SPDIF", (0x1 << 26), CLK_MON_CGATE_IP_PERIL},
	{"CLK_SLIMBUS", (0x1 << 25), CLK_MON_CGATE_IP_PERIL},
	{"CLK_PWM", (0x1 << 24), CLK_MON_CGATE_IP_PERIL},
	{"CLK_PCM2", (0x1 << 23), CLK_MON_CGATE_IP_PERIL},
	{"CLK_PCM1", (0x1 << 22), CLK_MON_CGATE_IP_PERIL},
	{"CLK_I2S2", (0x1 << 21), CLK_MON_CGATE_IP_PERIL},
	{"CLK_I2S1", (0x1 << 20), CLK_MON_CGATE_IP_PERIL},
	{"CLK_SPI2", (0x1 << 18), CLK_MON_CGATE_IP_PERIL},
	{"CLK_SPI1", (0x1 << 17), CLK_MON_CGATE_IP_PERIL},
	{"CLK_SPI0", (0x1 << 16), CLK_MON_CGATE_IP_PERIL},
	{"CLK_I2CHDMI", (0x1 << 14), CLK_MON_CGATE_IP_PERIL},
	{"CLK_I2C7", (0x1 << 13), CLK_MON_CGATE_IP_PERIL},
	{"CLK_I2C6", (0x1 << 12), CLK_MON_CGATE_IP_PERIL},
	{"CLK_I2C5", (0x1 << 11), CLK_MON_CGATE_IP_PERIL},
	{"CLK_I2C4", (0x1 << 10), CLK_MON_CGATE_IP_PERIL},
	{"CLK_I2C3", (0x1 << 9), CLK_MON_CGATE_IP_PERIL},
	{"CLK_I2C2", (0x1 << 8), CLK_MON_CGATE_IP_PERIL},
	{"CLK_I2C1", (0x1 << 7), CLK_MON_CGATE_IP_PERIL},
	{"CLK_I2C0", (0x1 << 6), CLK_MON_CGATE_IP_PERIL},
	{"CLK_UART4", (0x1 << 4), CLK_MON_CGATE_IP_PERIL},
	{"CLK_UART3", (0x1 << 3), CLK_MON_CGATE_IP_PERIL},
	{"CLK_UART2", (0x1 << 2), CLK_MON_CGATE_IP_PERIL},
	{"CLK_UART1", (0x1 << 1), CLK_MON_CGATE_IP_PERIL},
	{"CLK_UART0", (0x1), CLK_MON_CGATE_IP_PERIL},
	/* CLK_GATE_BLOCK */
	{"CLK_BLK_GPS", (0x1 << 7), CLK_MON_CGATE_BLOCK},
	{"CLK_BLK_LCD", (0x1 << 4), CLK_MON_CGATE_BLOCK},
	{"CLK_BLK_G3D", (0x1 << 3), CLK_MON_CGATE_BLOCK},
	{"CLK_BLK_MFC", (0x1 << 2), CLK_MON_CGATE_BLOCK},
	{"CLK_BLK_TV", (0x1 << 1), CLK_MON_CGATE_BLOCK},
	{"CLK_BLK_CAM", (0x1), CLK_MON_CGATE_BLOCK},
	/* CLK_GATE_IP_DMC */
	{"CLK_GPIOC2C", (0x1 << 31), CLK_MON_CGATE_IP_DMC},
	{"CLK_ASYNC_CPU_XIUR", (0x1 << 28), CLK_MON_CGATE_IP_DMC},
	{"CLK_ASYNC_C2C_XIUL", (0x1 << 27), CLK_MON_CGATE_IP_DMC},
	{"CLK_C2C", (0x1 << 26), CLK_MON_CGATE_IP_DMC},
	{"CLK_SMMUG2D_ACP", (0x1 << 24), CLK_MON_CGATE_IP_DMC},
	{"CLK_G2D_ACP", (0x1 << 23), CLK_MON_CGATE_IP_DMC},
	{"CLK_ASYNC_GDR", (0x1 << 22), CLK_MON_CGATE_IP_DMC},
	{"CLK_ASYNC_GDL", (0x1 << 21), CLK_MON_CGATE_IP_DMC},
	{"CLK_GIC", (0x1 << 20), CLK_MON_CGATE_IP_DMC},
	{"CLK_IEM_IEC", (0x1 << 18), CLK_MON_CGATE_IP_DMC},
	{"CLK_IEM_APC", (0x1 << 17), CLK_MON_CGATE_IP_DMC},
	{"CLK_PPMUACP", (0x1 << 16), CLK_MON_CGATE_IP_DMC},
	{"CLK_ID_REMAPPER", (0x1 << 13), CLK_MON_CGATE_IP_DMC},
	{"CLK_SMMUSSS", (0x1 << 12), CLK_MON_CGATE_IP_DMC},
	{"CLK_PPMUCPU", (0x1 << 10), CLK_MON_CGATE_IP_DMC},
	{"CLK_PPMUDMC1", (0x1 << 9), CLK_MON_CGATE_IP_DMC},
	{"CLK_PPMUDMC0", (0x1 << 8), CLK_MON_CGATE_IP_DMC},
	{"CLK_FBMDMC1", (0x1 << 6), CLK_MON_CGATE_IP_DMC},
	{"CLK_FBMDMC0", (0x1 << 5), CLK_MON_CGATE_IP_DMC},
	{"CLK_SSS", (0x1 << 4), CLK_MON_CGATE_IP_DMC},
	{"CLK_INT_COMB", (0x1 << 2), CLK_MON_CGATE_IP_DMC},
	{"CLK_DREX2", (0x1), CLK_MON_CGATE_IP_DMC},
	/* CLK_GATE_IP_CPU */
	{"CLK_CSSYS", (0x1 << 1), CLK_MON_CGATE_IP_CPU},
	{"CLK_HPM", (0x1), CLK_MON_CGATE_IP_CPU},
	/*  End */
	{{0}, 0, NULL},
};


static int clk_mon_ioc_check_reg(struct clk_mon_ioc_buf __user *uarg)
{
	struct clk_mon_ioc_buf *karg = NULL;
	void __iomem *v_addr = NULL;
	int size = sizeof(struct clk_mon_ioc_buf);
	int ret = -EFAULT;
	int i;

	if (!access_ok(VERIFY_WRITE, uarg, size))
		return -EFAULT;

	karg = kzalloc(size, GFP_KERNEL);

	if (!karg)
		return -ENOMEM;

	if (copy_from_user(karg, uarg, size)) {
		ret = -EFAULT;
		goto out;
	}

	for (i = 0; i < karg->nr_addrs; i++) {
		v_addr = ioremap((unsigned int)karg->reg[i].addr, 4);
		karg->reg[i].value = ioread32(v_addr);
		iounmap(v_addr);
	}

	if (copy_to_user(uarg, karg, size)) {
		ret = -EFAULT;
		goto out;
	}

	ret = 0;

out:
	kfree(karg);
	return ret;
}

static void clk_mon_set_power_info(struct clk_mon_reg_info *buf,
		char *name, void *reg_addr)
{
	unsigned int tmp = 0;

	tmp = __raw_readl(reg_addr);
	buf->addr = (void *)vaddr_to_paddr(reg_addr, PWR_REG);
	buf->value = (tmp & CLK_MON_PWR_EN);
	sprintf(buf->name, "%-5s", name);
}

static int clk_mon_ioc_check_power_domain(struct clk_mon_ioc_buf __user *uarg)
{
	struct clk_mon_ioc_buf *karg = NULL;
	int size = sizeof(struct clk_mon_ioc_buf);
	int ret = -EFAULT;
	int idx = 0;

	if (!access_ok(VERIFY_WRITE, uarg, size))
		return -EFAULT;

	karg = kzalloc(size, GFP_KERNEL);

	if (!karg)
		return -ENOMEM;

	clk_mon_set_power_info(&(karg->reg[idx++]),
			"CAM", CLK_MON_PMU_CAM_CONF);

	clk_mon_set_power_info(&(karg->reg[idx++]),
			"TV", CLK_MON_PMU_TV_CONF);

	clk_mon_set_power_info(&(karg->reg[idx++]),
			"MFC", CLK_MON_PMU_MFC_CONF);

	clk_mon_set_power_info(&(karg->reg[idx++]),
			"G3D", CLK_MON_PMU_G3D_CONF);

	clk_mon_set_power_info(&(karg->reg[idx++]),
			"LCD0", CLK_MON_PMU_LCD0_CONF);

	clk_mon_set_power_info(&(karg->reg[idx++]),
			"ISP", CLK_MON_PMU_ISP_CONF);

	clk_mon_set_power_info(&(karg->reg[idx++]),
			"GPS", CLK_MON_PMU_GPS_CONF);

	karg->nr_addrs = idx;

	if (copy_to_user(uarg, karg, size)) {
		ret = -EFAULT;
		goto out;
	}

	ret = 0;

out:
	kfree(karg);
	return ret;
}

static int clk_mon_ioc_check_clock_gating(struct clk_mon_ioc_buf __user *uarg)
{
	struct clk_mon_ioc_buf *karg = NULL;
	unsigned int val = 0;
	int size = sizeof(struct clk_mon_ioc_buf);
	int ret = -EFAULT;
	int i;

	if (!access_ok(VERIFY_WRITE, uarg, size))
		return -EFAULT;

	karg = kzalloc(size, GFP_KERNEL);

	if (!karg)
		return -ENOMEM;

	for (i = 0; clk_gate_masks[i].addr != NULL; i++) {
		val = __raw_readl(clk_gate_masks[i].addr);
		karg->reg[i].value = val & clk_gate_masks[i].mask;
		karg->reg[i].addr =
			(void *)vaddr_to_paddr(clk_gate_masks[i].addr, CLK_REG);
		strcpy(karg->reg[i].name, clk_gate_masks[i].name);
		karg->nr_addrs++;
	}

	if (copy_to_user(uarg, karg, size)) {
		ret = -EFAULT;
		goto out;
	}

	ret = 0;

out:
	kfree(karg);
	return ret;
}

static int clk_mon_ioc_set_reg(struct clk_mon_reg_info __user *uarg)
{
	struct clk_mon_reg_info *karg = NULL;
	void __iomem *v_addr = NULL;
	int size = sizeof(struct clk_mon_reg_info);
	int ret = 0;

	if (!access_ok(VERIFY_READ, uarg, size))
		return -EFAULT;

	karg = kzalloc(size, GFP_KERNEL);

	if (!karg)
		return -ENOMEM;

	if (copy_from_user(karg, uarg, size)) {
		ret = -EFAULT;
		goto out;
	}

	v_addr = ioremap((unsigned int)karg->addr, 4);
	iowrite32(karg->value, v_addr);
	iounmap(v_addr);

	ret = 0;

out:
	kfree(karg);
	return ret;
}

static long clk_mon_ioctl(struct file *filep, unsigned int cmd,
		unsigned long arg)
{
	struct clk_mon_ioc_buf __user *uarg = NULL;
	int ret = 0;

	pr_info("%s\n", __func__);

	if (!arg)
		return -EINVAL;

	uarg = (struct clk_mon_ioc_buf __user *)arg;

	switch (cmd) {
	case CLK_MON_IOC_CHECK_REG:
		ret = clk_mon_ioc_check_reg(uarg);
		break;
	case CLK_MON_IOC_CHECK_POWER_DOMAIN:
		ret = clk_mon_ioc_check_power_domain(uarg);
		break;
	case CLK_MON_IOC_CHECK_CLOCK_DOMAIN:
		ret = clk_mon_ioc_check_clock_gating(uarg);
		break;
	case CLK_MON_IOC_SET_REG:
		ret = clk_mon_ioc_set_reg(
				(struct clk_mon_reg_info __user *)arg);
		break;
	default:
		pr_err("%s:Invalid ioctl\n", __func__);
		ret = -EINVAL;
	}

	return ret;
}

static unsigned int g_reg_addr;
static unsigned int g_reg_value;

static ssize_t clk_mon_store_check_reg(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int reg_addr = 0;
	char *cur = NULL;
	int ret = 0;

	if (!buf)
		return -EINVAL;

	cur = strstr(buf, "0x");

	if (cur && cur + 2)
		ret = sscanf(cur + 2, "%x", &reg_addr);

	if (!ret)
		return -EINVAL;

	g_reg_addr = reg_addr;

	return size;
}

static ssize_t clk_mon_show_check_reg(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	void __iomem *v_addr = NULL;
	unsigned int p_addr = 0;
	unsigned int value = 0;

	if (!g_reg_addr)
		return -EINVAL;

	p_addr = g_reg_addr;
	v_addr = ioremap(p_addr, 4);

	value = ioread32(v_addr);
	iounmap(v_addr);

	return sprintf(buf, "[0x%x] 0x%x\n", p_addr, value) + 1;
}

static ssize_t clk_mon_store_set_reg(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int reg_addr = 0;
	unsigned int reg_value = 0;
	void __iomem *v_addr = NULL;
	char tmp_addr[9] = {0};
	char *cur = NULL;

	if (!buf)
		return -EINVAL;

	cur = strstr(buf, "0x");

	if (!cur || !(cur + 2))
		return -EINVAL;

	strncpy(tmp_addr, cur + 2, 8);

	if (!sscanf(tmp_addr, "%x", &reg_addr))
		return -EFAULT;

	cur = strstr(&cur[2], "0x");

	if (!cur || !(cur + 2))
		return -EINVAL;

	if (!sscanf(cur + 2, "%x", &reg_value))
		return -EFAULT;

	g_reg_addr  = reg_addr;
	g_reg_value = reg_value;

	v_addr = ioremap(g_reg_addr, 4);
	iowrite32(g_reg_value, v_addr);
	iounmap(v_addr);

	return size;
}

static const int NR_BIT = 8 * sizeof(unsigned int);
static const int IDX_SHIFT = 5;

int clk_mon_power_domain(unsigned int *pm_status)
{
	volatile unsigned int val = 0;
	int i;

	if (!pm_status)
		return -EINVAL;

	*pm_status = 0;

	for (i = 0; power_domain_masks[i].addr != NULL; i++) {
		if (i > NR_BIT) {
			pr_err("%s: Error Exceed storage size %d(%d)\n",
					__func__, i, NR_BIT);
			break;
		}

		val = __raw_readl(power_domain_masks[i].addr);
		val &= power_domain_masks[i].mask;

		if (val)
			*pm_status |= (0x1 << i);
	}

	return i;
}

int clk_mon_get_power_info(unsigned int *pm_status, char *buf)
{
	int i, val, size = 0;

	if (!pm_status || !buf)
		return -EINVAL;

	for (i = 0; power_domain_masks[i].addr != NULL; i++) {
		if (i > NR_BIT) {
			pr_err("%s: Error Exceed storage size %d(%d)\n",
					__func__, i, NR_BIT);
			break;
		}

		val = *pm_status & (0x1 << i);

		size += sprintf(buf + size, "[%-5s] %-3s\n",
				power_domain_masks[i].name, (val) ? "on" : "off");
	}

	return size + 1;
}

int clk_mon_clock_gate(unsigned int *clk_status)
{
	int bit_max = NR_BIT * CLK_GATES_NUM;
	volatile unsigned int val = 0;
	int i, bit_shift, idx;

	if (!clk_status || bit_max < 0)
		return -EINVAL;

	memset(clk_status, 0, sizeof(unsigned int) * CLK_GATES_NUM);

	for (i = 0; clk_gate_masks[i].addr != NULL; i++) {
		if (i >= bit_max) {
			pr_err("%s: Error Exceed storage size %d(%d)\n",
					__func__, i, bit_max);
			break;
		}

		val = __raw_readl(clk_gate_masks[i].addr);
		val &= clk_gate_masks[i].mask;

		idx = i >> IDX_SHIFT;
		bit_shift = i % NR_BIT;

		if (val)
			clk_status[idx] |= (0x1 << bit_shift);
		else
			clk_status[idx] &= ~(0x1 << bit_shift);
	}

	return i;
}

int clk_mon_get_clock_info(unsigned int *clk_status, char *buf)
{
	void *addr = NULL;
	int bit_max = NR_BIT * CLK_GATES_NUM;
	int bit_shift, idx, size = 0;
	int val, i;

	if (!clk_status || !buf)
		return -EINVAL;

	for (i = 0; clk_gate_masks[i].addr != NULL; i++) {
		if (i >= bit_max) {
			pr_err("%s: Error Exceed storage size %d(%d)\n",
					__func__, i, bit_max);
			break;
		}

		if (addr != clk_gate_masks[i].addr) {
			addr = clk_gate_masks[i].addr;
			size += snprintf(buf + size, CLK_MON_BUF_SIZE,
					"\n[0x%x]\n", vaddr_to_paddr(addr, CLK_REG));
		}

		bit_shift = i % NR_BIT;
		idx = i >> IDX_SHIFT;

		val = clk_status[idx] & (0x1 << bit_shift);

		size += snprintf(buf + size, CLK_MON_BUF_SIZE,
				" %-15s\t: %s\n", clk_gate_masks[i].name,
				(val) ? "on" : "off");
	}

	return size;
}

static ssize_t clk_mon_show_power_domain(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned int val = 0;
	ssize_t size = 0;

	val = __raw_readl(CLK_MON_PMU_CAM_CONF);
	size += sprintf(buf + size, "[0x%x] %-5s : %-3s\n",
			vaddr_to_paddr(CLK_MON_PMU_CAM_CONF, PWR_REG),
			"CAM", (val & CLK_MON_PWR_EN) ? "on" : "off");

	val = __raw_readl(CLK_MON_PMU_TV_CONF);
	size += sprintf(buf + size, "[0x%x] %-5s : %-3s\n",
			vaddr_to_paddr(CLK_MON_PMU_TV_CONF, PWR_REG),
			"TV", (val & CLK_MON_PWR_EN) ? "on" : "off");

	val = __raw_readl(CLK_MON_PMU_MFC_CONF);
	size += sprintf(buf + size, "[0x%x] %-5s : %-3s\n",
			vaddr_to_paddr(CLK_MON_PMU_MFC_CONF, PWR_REG),
			"MFC", (val & CLK_MON_PWR_EN) ? "on" : "off");

	val = __raw_readl(CLK_MON_PMU_G3D_CONF);
	size += sprintf(buf + size, "[0x%x] %-5s : %-3s\n",
			vaddr_to_paddr(CLK_MON_PMU_G3D_CONF, PWR_REG),
			"G3D", (val & CLK_MON_PWR_EN) ? "on" : "off");

	val = __raw_readl(CLK_MON_PMU_LCD0_CONF);
	size += sprintf(buf + size, "[0x%x] %-5s : %-3s\n",
			vaddr_to_paddr(CLK_MON_PMU_LCD0_CONF, PWR_REG),
			"LCD0", (val & CLK_MON_PWR_EN) ? "on" : "off");

	val = __raw_readl(CLK_MON_PMU_ISP_CONF);
	size += sprintf(buf + size, "[0x%x] %-5s : %-3s\n",
			vaddr_to_paddr(CLK_MON_PMU_ISP_CONF, PWR_REG),
			"ISP", (val & CLK_MON_PWR_EN) ? "on" : "off");

	val = __raw_readl(CLK_MON_PMU_GPS_CONF);
	size += sprintf(buf + size, "[0x%x] %-5s : %-3s\n",
			vaddr_to_paddr(CLK_MON_PMU_GPS_CONF, PWR_REG),
			"GPS", (val & CLK_MON_PWR_EN) ? "on" : "off");

	return size + 1;
}

static ssize_t clk_mon_show_clock_gating(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	void *addr = NULL;
	unsigned int val  = 0;
	ssize_t size = 0;
	int i;

	for (i = 0; clk_gate_masks[i].addr != NULL; i++) {
		val = __raw_readl(clk_gate_masks[i].addr);

		if (addr != clk_gate_masks[i].addr) {
			addr = clk_gate_masks[i].addr;
			size += sprintf(buf + size, "\n[0x%x] 0x%x\n",
					vaddr_to_paddr(addr, CLK_REG), val);
		}

		val &= clk_gate_masks[i].mask;

		size += sprintf(buf + size, " %-15s\t: %s\n",
				clk_gate_masks[i].name, (val) ? "on" : "off");
	}

	return size + 1;
}


static DEVICE_ATTR(check_reg, S_IRUSR | S_IWUSR,
		clk_mon_show_check_reg, clk_mon_store_check_reg);
static DEVICE_ATTR(set_reg, S_IWUSR, NULL, clk_mon_store_set_reg);
static DEVICE_ATTR(power_domain, S_IRUSR, clk_mon_show_power_domain, NULL);
static DEVICE_ATTR(clock_gating, S_IRUSR, clk_mon_show_clock_gating, NULL);

static struct attribute *clk_mon_attributes[] = {
	&dev_attr_check_reg.attr,
	&dev_attr_set_reg.attr,
	&dev_attr_power_domain.attr,
	&dev_attr_clock_gating.attr,
	NULL,
};

static struct attribute_group clk_mon_attr_group = {
	.attrs = clk_mon_attributes,
	.name  = "check",
};

static const struct file_operations clk_mon_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = clk_mon_ioctl,
};

static struct miscdevice clk_mon_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = "clk_mon",
	.fops  = &clk_mon_fops,
};

static int __init clk_mon_init(void)
{
	int ret = 0;

	pr_info("%s\n", __func__);

	ret = misc_register(&clk_mon_device);

	if (ret) {
		pr_err("%s: Unable to register clk_mon_device\n", __func__);
		goto err_misc_register;
	}

	ret = sysfs_create_group(&clk_mon_device.this_device->kobj,
			&clk_mon_attr_group);

	if (ret) {
		pr_err("%s: Unable to Create sysfs node\n", __func__);
		goto err_create_group;
	}

	return 0;

err_create_group:
	misc_deregister(&clk_mon_device);
err_misc_register:
	return ret;
}

static void __exit clk_mon_exit(void)
{
	misc_deregister(&clk_mon_device);
}

module_init(clk_mon_init);
module_exit(clk_mon_exit);

MODULE_AUTHOR("TaeYoung Kim <tyung.kim@samsung.com>");
MODULE_DESCRIPTION("Clock Gate Monitor");
MODULE_LICENSE("GPL");

