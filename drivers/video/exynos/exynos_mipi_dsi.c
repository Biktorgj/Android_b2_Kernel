/* linux/drivers/video/exynos/exynos_mipi_dsi.c
 *
 * Samsung SoC MIPI-DSIM driver.
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd
 *
 * InKi Dae, <inki.dae@samsung.com>
 * Donghwa Lee, <dh09.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/fb.h>
#include <linux/ctype.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/memory.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/notifier.h>
#include <linux/regulator/consumer.h>
#include <linux/pm_runtime.h>
#include <drm/exynos_drm.h>

#include <video/exynos_mipi_dsim.h>

#include <plat/fb.h>

#include "exynos_mipi_dsi_common.h"
#include "exynos_mipi_dsi_lowlevel.h"

struct mipi_dsim_ddi {
	int				bus_id;
	struct list_head		list;
	struct mipi_dsim_lcd_device	*dsim_lcd_dev;
	struct mipi_dsim_lcd_driver	*dsim_lcd_drv;
};

struct mipi_dsim_partial_region {
	unsigned int x;
	unsigned int y;
	unsigned int w;
	unsigned int h;
};

static LIST_HEAD(dsim_ddi_list);
static DEFINE_MUTEX(mipi_dsim_lock);
#define dev_to_dsim(a)	platform_get_drvdata(to_platform_device(a))
static struct mipi_dsim_platform_data *to_dsim_plat(struct platform_device
							*pdev)
{
	return pdev->dev.platform_data;
}

static int exynos_mipi_regulator_enable(struct mipi_dsim_device *dsim)
{
	struct mipi_dsim_driverdata *ddata = dsim->ddata;

	int ret;

	mutex_lock(&dsim->lock);
	ret = regulator_bulk_enable(ddata->num_supply, ddata->supplies);
	mutex_unlock(&dsim->lock);

	dev_dbg(dsim->dev, "MIPI regulator enable success.\n");
	return ret;
}

static int exynos_mipi_regulator_disable(struct mipi_dsim_device *dsim)
{
	struct mipi_dsim_driverdata *ddata = dsim->ddata;
	int ret;

	mutex_lock(&dsim->lock);
	ret = regulator_bulk_disable(ddata->num_supply, ddata->supplies);
	mutex_unlock(&dsim->lock);

	return ret;
}

/* update all register settings to MIPI DSI controller. */
static void exynos_mipi_update_cfg(struct mipi_dsim_device *dsim)
{

	exynos_mipi_dsi_init_dsim(dsim);
	exynos_mipi_dsi_init_link(dsim);

	usleep_range(10000, 10000);

	exynos_mipi_dsi_set_hs_enable(dsim, 0);

	/* set display timing. */
	exynos_mipi_dsi_set_display_mode(dsim, dsim->dsim_config);

	exynos_mipi_dsi_init_interrupt(dsim);

}

static int exynos_mipi_dsi_early_blank_mode(struct mipi_dsim_device *dsim,
		int power)
{
	int ret = 0;

	pr_info("[mipi]pw_down:cur_dpms[%d]", dsim->dpms);

	switch (power) {
	case FB_BLANK_POWERDOWN:
		if (dsim->dpms == FB_BLANK_UNBLANK)
			pm_runtime_put_sync(dsim->dev);
		else {
			pr_err("%s:invalid dpms state.\n", __func__);
			ret = -EINVAL;
		}
		break;
	default:
		pr_err("%s:invalid power mode[%d].\n", __func__, power);
		ret = -EINVAL;
		break;
	}

	pr_info("[mipi]pw_down:done:cur_dpms[%d]ret[%d]", dsim->dpms, ret);

	return ret;
}

static int exynos_mipi_dsi_blank_mode(struct mipi_dsim_device *dsim, int power)
{
	int ret = 0;

	pr_info("[mipi]blank:cur_dpms[%d]", dsim->dpms);

	switch (power) {
	case FB_BLANK_UNBLANK:
		if (dsim->dpms == FB_BLANK_POWERDOWN) {
			atomic_set(&dsim->dpms_on, 1);
			pm_runtime_get_sync(dsim->dev);
			atomic_set(&dsim->dpms_on, 0);
		} else {
			pr_err("%s:invalid dpms state.\n", __func__);
			ret = -EINVAL;
		}
		break;
	case FB_BLANK_NORMAL:
	default:
		pr_err("%s:invalid power mode[%d].\n", __func__, power);
		ret = -EINVAL;
		break;
	}

	pr_info("[mipi]blank:done:cur_dpms[%d]ret[%d]", dsim->dpms, ret);

	return ret;
}

int exynos_mipi_dsi_register_lcd_device(struct mipi_dsim_lcd_device *lcd_dev)
{
	struct mipi_dsim_ddi *dsim_ddi;

	if (!lcd_dev->name) {
		pr_err("dsim_lcd_device name is NULL.\n");
		return -EFAULT;
	}

	dsim_ddi = kzalloc(sizeof(struct mipi_dsim_ddi), GFP_KERNEL);
	if (!dsim_ddi) {
		pr_err("failed to allocate dsim_ddi object.\n");
		return -ENOMEM;
	}

	dsim_ddi->dsim_lcd_dev = lcd_dev;

	mutex_lock(&mipi_dsim_lock);
	list_add_tail(&dsim_ddi->list, &dsim_ddi_list);
	mutex_unlock(&mipi_dsim_lock);

	return 0;
}

struct mipi_dsim_ddi *exynos_mipi_dsi_find_lcd_device
		(struct mipi_dsim_lcd_driver *lcd_drv)
{
	struct mipi_dsim_ddi *dsim_ddi, *next;
	struct mipi_dsim_lcd_device *lcd_dev;

	mutex_lock(&mipi_dsim_lock);

	list_for_each_entry_safe(dsim_ddi, next, &dsim_ddi_list, list) {
		if (!dsim_ddi)
			goto out;

		lcd_dev = dsim_ddi->dsim_lcd_dev;
		if (!lcd_dev)
			continue;

		if ((strcmp(lcd_drv->name, lcd_dev->name)) == 0) {
			/**
			 * bus_id would be used to identify
			 * connected bus.
			 */
			dsim_ddi->bus_id = lcd_dev->bus_id;
			mutex_unlock(&mipi_dsim_lock);

			return dsim_ddi;
		}

		list_del(&dsim_ddi->list);
		kfree(dsim_ddi);
	}

out:
	mutex_unlock(&mipi_dsim_lock);

	return NULL;
}

int exynos_mipi_dsi_register_lcd_driver(struct mipi_dsim_lcd_driver *lcd_drv)
{
	struct mipi_dsim_ddi *dsim_ddi;

	if (!lcd_drv->name) {
		pr_err("dsim_lcd_driver name is NULL.\n");
		return -EFAULT;
	}

	dsim_ddi = exynos_mipi_dsi_find_lcd_device(lcd_drv);
	if (!dsim_ddi) {
		pr_err("mipi_dsim_ddi object not found.\n");
		return -EFAULT;
	}

	dsim_ddi->dsim_lcd_drv = lcd_drv;

	pr_info("registered panel driver(%s) to mipi-dsi driver.\n",
		lcd_drv->name);

	return 0;

}

struct mipi_dsim_ddi *exynos_mipi_dsi_bind_lcd_ddi
		(struct mipi_dsim_device *dsim, const char *name)
{
	struct mipi_dsim_ddi *dsim_ddi, *next;
	struct mipi_dsim_lcd_driver *lcd_drv;
	struct mipi_dsim_lcd_device *lcd_dev;
	int ret;

	mutex_lock(&dsim->lock);

	list_for_each_entry_safe(dsim_ddi, next, &dsim_ddi_list, list) {
		lcd_drv = dsim_ddi->dsim_lcd_drv;
		lcd_dev = dsim_ddi->dsim_lcd_dev;
		if (!lcd_drv || !lcd_dev ||
			(dsim->id != dsim_ddi->bus_id))
				continue;

		dev_dbg(dsim->dev, "lcd_drv->id = %d, lcd_dev->id = %d\n",
				lcd_drv->id, lcd_dev->id);
		dev_dbg(dsim->dev, "lcd_dev->bus_id = %d, dsim->id = %d\n",
				lcd_dev->bus_id, dsim->id);

		if ((strcmp(lcd_drv->name, name) == 0)) {
			lcd_dev->master = dsim;

			lcd_dev->dev.parent = dsim->dev;
			dev_set_name(&lcd_dev->dev, "%s", lcd_drv->name);

			ret = device_register(&lcd_dev->dev);
			if (ret < 0) {
				dev_err(dsim->dev,
					"can't register %s, status %d\n",
					dev_name(&lcd_dev->dev), ret);
				mutex_unlock(&dsim->lock);

				return NULL;
			}

			dsim->dsim_lcd_dev = lcd_dev;
			dsim->dsim_lcd_drv = lcd_drv;

			mutex_unlock(&dsim->lock);

			return dsim_ddi;
		}
	}

	mutex_unlock(&dsim->lock);

	return NULL;
}

static int exynos_mipi_dsi_set_refresh_rate(struct mipi_dsim_device *dsim,
	int refresh)
{
	struct mipi_dsim_lcd_driver *client_drv = dsim->dsim_lcd_drv;
	struct mipi_dsim_lcd_device *client_dev = dsim->dsim_lcd_dev;

	if (client_drv && client_drv->set_refresh_rate)
		client_drv->set_refresh_rate(client_dev, refresh);

	return 0;
}

static int exynos_mipi_dsi_set_partial_region(struct mipi_dsim_device *dsim,
	void *pos)
{
	struct mipi_dsim_lcd_driver *client_drv = dsim->dsim_lcd_drv;
	struct mipi_dsim_lcd_device *client_dev = dsim->dsim_lcd_dev;
	struct mipi_dsim_partial_region *region = pos;

	if (client_drv && client_drv->set_partial_region) {
		exynos_mipi_dsi_set_main_disp_resol(dsim, region->w, region->h);
		client_drv->set_partial_region(client_dev, region->x, region->y,
						region->w, region->h);
	}

	return 0;
}

static int exynos_mipi_dsi_wait_for_frame_done(struct mipi_dsim_device *dsim)
{
	struct exynos_drm_fimd_pdata *src_pd;
	struct platform_device *src_pdev;

	src_pdev = dsim->src_pdev;
	if (!src_pdev)
		return -EINVAL;

	src_pd = src_pdev->dev.platform_data;
	if (src_pd && src_pd->wait_for_frame_done)
		src_pd->wait_for_frame_done(&src_pdev->dev);

	return 0;
}

static void exynos_mipi_dsi_stop_trigger(struct mipi_dsim_device *dsim,
						unsigned int stop)
{
	struct exynos_drm_fimd_pdata *src_pd;
	struct platform_device *src_pdev;

	src_pdev = dsim->src_pdev;
	if (!src_pdev)
		return;

	src_pd = src_pdev->dev.platform_data;
	if (src_pd && src_pd->stop_trigger)
		src_pd->stop_trigger(&src_pdev->dev, stop);
}

int exynos_mipi_dsi_runtime_active(struct mipi_dsim_device *dsim, bool enable)
{
	int ret = 0;

	pr_info("[mipi]gate[%d]cur_dpms[%d]", enable, dsim->dpms);

	atomic_set(&dsim->pwr_gate, 1);

	if (enable) {
		if (dsim->dpms == FB_BLANK_VSYNC_SUSPEND)
			pm_runtime_get_sync(dsim->dev);
		else {
			pr_err("%s:invalid dpms state.\n", __func__);
			ret = -EINVAL;
			goto out;
		}
	} else {
		if (dsim->dpms == FB_BLANK_UNBLANK)
			pm_runtime_put_sync(dsim->dev);
		else {
			pr_err("%s:invalid dpms state.\n", __func__);
			ret = -EINVAL;
			goto out;
		}
	}

out:
	atomic_set(&dsim->pwr_gate, 0);
	pr_info("[mipi]gate:done[%d]cur_dpms[%d]ret[%d]",
		enable, dsim->dpms, ret);

	return ret;
}

static int exynos_mipi_dsi_set_runtime_active
		(struct mipi_dsim_device *dsim)
{
	struct exynos_drm_fimd_pdata *src_pd;
	struct platform_device *src_pdev;
	int ret = 0;

	src_pdev = dsim->src_pdev;
	if (!src_pdev)
		return -ENOMEM;

	src_pd = src_pdev->dev.platform_data;
	if (src_pd && src_pd->set_runtime_activate)
		ret = src_pd->set_runtime_activate(&src_pdev->dev);

	return ret;
}

static int exynos_mipi_dsi_set_smies_active
		(struct mipi_dsim_device *dsim, bool enable)
{
	struct exynos_drm_fimd_pdata *src_pd;
	struct platform_device *src_pdev;
	int ret = 0;

	src_pdev = dsim->src_pdev;
	if (!src_pdev)
		return -ENOMEM;

	src_pd = src_pdev->dev.platform_data;
	if (src_pd && src_pd->set_smies_activate)
		ret = src_pd->set_smies_activate(&src_pdev->dev, enable);

	return ret;
}

static int exynos_mipi_dsi_set_smies_mode
		(struct mipi_dsim_device *dsim, int mode)
{
	struct exynos_drm_fimd_pdata *src_pd;
	struct platform_device *src_pdev;
	int ret = 0;

	src_pdev = dsim->src_pdev;
	if (!src_pdev)
		return -ENOMEM;

	src_pd = src_pdev->dev.platform_data;
	if (src_pd && src_pd->set_smies_mode)
		ret = src_pd->set_smies_mode(&src_pdev->dev, mode);

	return ret;
}

static int exynos_mipi_dsi_set_fimd_trigger(struct mipi_dsim_device *dsim)
{
	struct platform_device *src_pdev;
	struct exynos_drm_fimd_pdata *src_pdata;
	int ret = 0;

	src_pdev = dsim->src_pdev;
	if (!src_pdev)
		return -ENOMEM;

	src_pdata = src_pdev->dev.platform_data;

	if (src_pdata->trigger)
		src_pdata->trigger(&src_pdev->dev);

	return ret;
}

static void exynos_mipi_dsi_notify_panel_self_refresh
		(struct mipi_dsim_device *dsim, unsigned int rate)
{
	struct exynos_drm_fimd_pdata *src_pd;
	struct platform_device *src_pdev;

	src_pdev = dsim->src_pdev;
	if (!src_pdev)
		return;

	src_pd = src_pdev->dev.platform_data;
	if (src_pd && src_pd->update_panel_refresh)
		src_pd->update_panel_refresh(&src_pdev->dev, rate);

}

static void exynos_mipi_dsi_power_gate(struct mipi_dsim_device *dsim,
		bool enable)
{
	struct platform_device *pdev = to_platform_device(dsim->dev);

	if (enable) {
		exynos_mipi_regulator_enable(dsim);

		if (dsim->pd->phy_enable)
			dsim->pd->phy_enable(pdev, true);
		clk_enable(dsim->clock);

		usleep_range(10000, 10000);

		exynos_mipi_update_cfg(dsim);

		exynos_mipi_dsi_standby(dsim, 1, 1);

	} else {
		exynos_mipi_dsi_stop_trigger(dsim, true);
		exynos_mipi_dsi_standby(dsim, 0, 0);

		if (dsim->pd->phy_enable)
			dsim->pd->phy_enable(pdev, false);
		clk_disable(dsim->clock);

		exynos_mipi_regulator_disable(dsim);
	}
}

/* define MIPI-DSI Master operations. */
static struct mipi_dsim_master_ops master_ops = {
	.cmd_read			= exynos_mipi_dsi_rd_data,
	.cmd_write			= exynos_mipi_dsi_wr_data,
	.atomic_cmd_write		= exynos_mipi_dsi_atomic_wr_data,
	.wait_for_frame_done		= exynos_mipi_dsi_wait_for_frame_done,
	.stop_trigger			= exynos_mipi_dsi_stop_trigger,
	.standby			= exynos_mipi_dsi_standby,
	.set_early_blank_mode		= exynos_mipi_dsi_early_blank_mode,
	.set_blank_mode			= exynos_mipi_dsi_blank_mode,
	.set_refresh_rate	= exynos_mipi_dsi_set_refresh_rate,
	.update_panel_refresh	= exynos_mipi_dsi_notify_panel_self_refresh,
	.set_clock_mode			= exynos_mipi_dsi_set_hs_enable,
	.runtime_active			= exynos_mipi_dsi_runtime_active,
	.set_runtime_active		= exynos_mipi_dsi_set_runtime_active,
	.set_partial_region		= exynos_mipi_dsi_set_partial_region,
	.set_smies_active		= exynos_mipi_dsi_set_smies_active,
	.set_smies_mode		= exynos_mipi_dsi_set_smies_mode,
	.fimd_trigger		= exynos_mipi_dsi_set_fimd_trigger,
};

static int exynos_mipi_dsi_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct mipi_dsim_device *dsim;
	struct mipi_dsim_config *dsim_config;
	struct mipi_dsim_platform_data *dsim_pd;
	struct mipi_dsim_driverdata *ddata;
	struct mipi_dsim_ddi *dsim_ddi;
	int ret = -EINVAL;

	dsim = kzalloc(sizeof(struct mipi_dsim_device), GFP_KERNEL);
	if (!dsim) {
		dev_err(&pdev->dev, "failed to allocate dsim object.\n");
		return -ENOMEM;
	}

	dsim->pd = to_dsim_plat(pdev);
	dsim->dev = &pdev->dev;
	dsim->id = pdev->id;
	dsim->src_pdev = dsim->pd->src_pdev;

	/* get mipi_dsim_drvierdata. */
	ddata = (struct mipi_dsim_driverdata *)
			platform_get_device_id(pdev)->driver_data;
	if (ddata == NULL) {
		dev_err(&pdev->dev, "failed to get driver data for dsim.\n");
		goto err_clock_get;
	}
	dsim->ddata = ddata;

	/* get mipi_dsim_platform_data. */
	dsim_pd = (struct mipi_dsim_platform_data *)dsim->pd;
	if (dsim_pd == NULL) {
		dev_err(&pdev->dev, "failed to get platform data for dsim.\n");
		goto err_clock_get;
	}
	/* get mipi_dsim_config. */
	dsim_config = dsim_pd->dsim_config;
	if (dsim_config == NULL) {
		dev_err(&pdev->dev, "failed to get dsim config data.\n");
		goto err_clock_get;
	}

	dsim->dsim_config = dsim_config;
	dsim->master_ops = &master_ops;

	mutex_init(&dsim->lock);

	ret = regulator_bulk_get(&pdev->dev, ddata->num_supply,
			ddata->supplies);
	if (ret) {
		dev_err(&pdev->dev, "Failed to get regulators: %d\n", ret);
		goto err_clock_get;
	}

	dsim->clock = clk_get(&pdev->dev, ddata->clk_name);
	if (IS_ERR(dsim->clock)) {
		dev_err(&pdev->dev, "failed to get dsim clock source\n");
		goto err_clock_get;
	}

	clk_enable(dsim->clock);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "failed to get io memory region\n");
		goto err_platform_get;
	}

	dsim->res = request_mem_region(res->start, resource_size(res),
					dev_name(&pdev->dev));
	if (!dsim->res) {
		dev_err(&pdev->dev, "failed to request io memory region\n");
		ret = -ENOMEM;
		goto err_mem_region;
	}

	dsim->reg_base = ioremap(res->start, resource_size(res));
	if (!dsim->reg_base) {
		dev_err(&pdev->dev, "failed to remap io region\n");
		ret = -ENOMEM;
		goto err_ioremap;
	}

	/* bind lcd ddi matched with panel name. */
	dsim_ddi = exynos_mipi_dsi_bind_lcd_ddi(dsim, dsim_pd->lcd_panel_name);
	if (!dsim_ddi) {
		dev_err(&pdev->dev, "mipi_dsim_ddi object not found.\n");
		goto err_bind;
	}

	dsim->irq = platform_get_irq(pdev, 0);
	if (dsim->irq < 0) {
		dev_err(&pdev->dev, "failed to request dsim irq resource\n");
		ret = -EINVAL;
		goto err_platform_get_irq;
	}

	init_completion(&dsim->wr_comp);
	init_completion(&dsim->rd_comp);
	init_completion(&dsim->fd_comp);

	ret = request_irq(dsim->irq, exynos_mipi_dsi_interrupt_handler,
			IRQF_SHARED, pdev->name, dsim);
	if (ret != 0) {
		dev_err(&pdev->dev, "failed to request dsim irq\n");
		ret = -EINVAL;
		goto err_bind;
	}

	/* enable interrupt */
	exynos_mipi_dsi_init_interrupt(dsim);

	/* initialize mipi-dsi client(lcd panel). */
	if (dsim_ddi->dsim_lcd_drv && dsim_ddi->dsim_lcd_drv->probe)
		dsim_ddi->dsim_lcd_drv->probe(dsim_ddi->dsim_lcd_dev);

	platform_set_drvdata(pdev, dsim);
	/* in case that mipi got enabled at bootloader. */
	if (dsim_pd->enabled) {
		/* lcd panel power on. */
		if (dsim_ddi->dsim_lcd_drv && dsim_ddi->dsim_lcd_drv->power_on)
			dsim_ddi->dsim_lcd_drv->power_on(dsim_ddi->dsim_lcd_dev, 1);

		exynos_mipi_regulator_enable(dsim);

		if (dsim_ddi->dsim_lcd_drv && dsim_ddi->dsim_lcd_drv->check_mtp)
			dsim_ddi->dsim_lcd_drv->check_mtp(dsim_ddi->dsim_lcd_dev);

		exynos_mipi_update_cfg(dsim);
	} else {
		/* TODO:
		 * add check_mtp callback function
		 * if mipi dsim is off on bootloader, it causes kernel panic */
		dsim->dpms = FB_BLANK_POWERDOWN;
	}

	pm_runtime_enable(&pdev->dev);
	pm_runtime_get_sync(&pdev->dev);

	dev_info(&pdev->dev, "mipi-dsi driver(%s mode) has been probed.\n",
		(dsim_config->e_interface == DSIM_COMMAND) ?
			"CPU" : "RGB");

	return 0;

err_bind:
	iounmap(dsim->reg_base);

err_ioremap:
	release_mem_region(dsim->res->start, resource_size(dsim->res));

err_mem_region:
	release_resource(dsim->res);

err_platform_get:
	clk_disable(dsim->clock);
	clk_put(dsim->clock);
err_clock_get:
	kfree(dsim);

err_platform_get_irq:
	return ret;
}

static int __devexit exynos_mipi_dsi_remove(struct platform_device *pdev)
{
	struct mipi_dsim_device *dsim = platform_get_drvdata(pdev);
	struct mipi_dsim_ddi *dsim_ddi, *next;
	struct mipi_dsim_lcd_driver *dsim_lcd_drv;
	struct mipi_dsim_driverdata *ddata = dsim->ddata;

	iounmap(dsim->reg_base);

	clk_disable(dsim->clock);
	clk_put(dsim->clock);

	release_resource(dsim->res);
	release_mem_region(dsim->res->start, resource_size(dsim->res));

	list_for_each_entry_safe(dsim_ddi, next, &dsim_ddi_list, list) {
		if (dsim_ddi) {
			if (dsim->id != dsim_ddi->bus_id)
				continue;

			dsim_lcd_drv = dsim_ddi->dsim_lcd_drv;

			if (dsim_lcd_drv->remove)
				dsim_lcd_drv->remove(dsim_ddi->dsim_lcd_dev);

			kfree(dsim_ddi);
		}
	}

	regulator_bulk_free(ddata->num_supply, ddata->supplies);
	kfree(dsim);

	return 0;
}

static int exynos_mipi_dsi_power_on(struct mipi_dsim_device *dsim, bool enable)
{
	struct mipi_dsim_lcd_driver *client_drv = dsim->dsim_lcd_drv;
	struct mipi_dsim_lcd_device *client_dev = dsim->dsim_lcd_dev;
	struct platform_device *pdev = to_platform_device(dsim->dev);
	struct platform_device *src_pdev;
	struct exynos_drm_fimd_pdata *src_pdata;
	int ret = 0;

	pr_info("[mipi]pm[%d]cur_dpms[%d][%s]\n",
		enable, dsim->dpms,
		atomic_read(&dsim->pwr_gate) ? "pwr_gate" : "dpms");

	if (enable) {
		if (atomic_read(&dsim->pwr_gate)) {
			if (dsim->dpms != FB_BLANK_VSYNC_SUSPEND) {
				pr_err("%s:invalid dpms state.\n", __func__);
				ret = -EINVAL;
				goto out;
			}

			exynos_mipi_dsi_power_gate(dsim, enable);
			dsim->dpms = FB_BLANK_UNBLANK;
			goto out;
		}

		if (dsim->dpms != FB_BLANK_POWERDOWN) {
			pr_info("%s:bypass.\n", __func__);
			goto out;
		}

		exynos_mipi_regulator_enable(dsim);

		/* enable MIPI-DSI PHY. */
		if (dsim->pd->phy_enable)
			dsim->pd->phy_enable(pdev, true);

		clk_enable(dsim->clock);

		exynos_mipi_dsi_standby(dsim, 0, 0);

		exynos_mipi_update_cfg(dsim);

		if (client_drv && client_drv->panel_pm_check) {
			bool pm_skip;

			client_drv->panel_pm_check(client_dev, &pm_skip);
			if (pm_skip) {
				exynos_mipi_dsi_standby(dsim, 1, 1);
				dsim->dpms = FB_BLANK_UNBLANK;
				return 0;
			}
		}
		/* lcd panel power on. */
		if (client_drv && client_drv->power_on)
			client_drv->power_on(client_dev, 1);

		exynos_mipi_dsi_stop_trigger(dsim, true);
		/* set lcd panel sequence commands. */
		if (client_drv && client_drv->set_sequence)
			client_drv->set_sequence(client_dev);

		exynos_mipi_dsi_standby(dsim, 1, 1);

		src_pdev = dsim->src_pdev;
		src_pdata = src_pdev->dev.platform_data;

		if (src_pdata->trigger)
			src_pdata->trigger(&src_pdev->dev);

		exynos_mipi_dsi_stop_trigger(dsim, true);

		INIT_COMPLETION(dsim->fd_comp);
		if (!wait_for_completion_timeout(&dsim->fd_comp,
			msecs_to_jiffies(50)))
			dev_warn(dsim->dev, "timeout for frame done\n");

		if (client_drv->display_on)
			client_drv->display_on(client_dev, 1);

		exynos_mipi_dsi_standby(dsim, 1, 1);

		if (client_drv && client_drv->resume)
			client_drv->resume(client_dev);

		dsim->dpms = FB_BLANK_UNBLANK;
	} else {
		if (dsim->dpms != FB_BLANK_UNBLANK) {
			pr_err("%s:invalid dpms state.\n", __func__);
			ret = -EINVAL;
			goto out;
		}

		if (atomic_read(&dsim->pwr_gate)) {
			exynos_mipi_dsi_power_gate(dsim, enable);
			dsim->dpms = FB_BLANK_VSYNC_SUSPEND;
			goto out;
		}

		exynos_mipi_dsi_standby(dsim, 0, 1);

		if (client_drv && client_drv->suspend)
			client_drv->suspend(client_dev);

		/* disable MIPI-DSI PHY. */
		if (dsim->pd->phy_enable)
			dsim->pd->phy_enable(pdev, false);

		clk_disable(dsim->clock);

		exynos_mipi_regulator_disable(dsim);

		dsim->dpms = FB_BLANK_POWERDOWN;
	}

out:
	pr_info("[mipi]pm[%d]done:cur_dpms[%d][%s]ret[%d]\n",
		enable, dsim->dpms,
		atomic_read(&dsim->pwr_gate) ? "pwr_gate" : "dpms",
		ret);

	return ret;
}

#ifdef CONFIG_PM
static int exynos_mipi_dsi_suspend(struct device *dev)
{
	struct mipi_dsim_device *dsim = dev_to_dsim(dev);

	if (pm_runtime_suspended(dev))
		return 0;

	dev_info(dsim->dev, "%s\n", __func__);

	/*
	 * do not use pm_runtime_suspend(). if pm_runtime_suspend() is
	 * called here, an error would be returned by that interface
	 * because the usage_count of pm runtime is more than 1.
	 */
	return exynos_mipi_dsi_power_on(dsim, false);
}

static int exynos_mipi_dsi_resume(struct device *dev)
{
	struct mipi_dsim_device *dsim = dev_to_dsim(dev);

	/*
	 * if entering to sleep when lcd panel is on, the usage_count
	 * of pm runtime would still be 1 so in this case, mipi dsi driver
	 * should be on directly not drawing on pm runtime interface.
	 */
	if (!pm_runtime_suspended(dev)) {
		dev_info(dsim->dev, "%s\n", __func__);
		return exynos_mipi_dsi_power_on(dsim, true);
	}

	return 0;
}
#endif
#ifdef CONFIG_PM_RUNTIME
static int exynos_mipi_dsi_runtime_suspend(struct device *dev)
{
	struct mipi_dsim_device *dsim = dev_to_dsim(dev);

	dev_dbg(dsim->dev, "%s\n", __func__);

	return exynos_mipi_dsi_power_on(dsim, false);
}

static int exynos_mipi_dsi_runtime_resume(struct device *dev)
{
	struct mipi_dsim_device *dsim = dev_to_dsim(dev);

	dev_dbg(dsim->dev, "%s\n", __func__);

	return exynos_mipi_dsi_power_on(dsim, true);
}
#endif

static const struct dev_pm_ops exynos_mipi_dsi_pm_ops = {
	.suspend		= exynos_mipi_dsi_suspend,
	.resume			= exynos_mipi_dsi_resume,
	.runtime_suspend	= exynos_mipi_dsi_runtime_suspend,
	.runtime_resume		= exynos_mipi_dsi_runtime_resume,
};

static struct regulator_bulk_data exynos3_supplies[] = {
	{ .supply = "vap_mipi_1.0v", },
};

static struct mipi_dsim_driverdata exynos3_ddata = {
	.clk_name = "dsim0",
	.num_supply = ARRAY_SIZE(exynos3_supplies),
	.supplies = exynos3_supplies,
};

static struct regulator_bulk_data exynos4_supplies[] = {
	{ .supply = "vdd10", },
	{ .supply = "vdd18", },
};

static struct mipi_dsim_driverdata exynos4_ddata = {
	.clk_name = "dsim0",
	.num_supply = ARRAY_SIZE(exynos4_supplies),
	.supplies = exynos4_supplies,
};

static struct mipi_dsim_driverdata exynos5210_ddata = {
	.clk_name = "dsim0",
	.num_supply = ARRAY_SIZE(exynos4_supplies),
	.supplies = exynos4_supplies,
};

static struct mipi_dsim_driverdata exynos5410_ddata = {
	.clk_name = "dsim0",
	.num_supply = ARRAY_SIZE(exynos4_supplies),
	.supplies = exynos4_supplies,
};

static struct platform_device_id exynos_mipi_driver_ids[] = {
	{
		.name		= "exynos3-mipi",
		.driver_data	= (unsigned long)&exynos3_ddata,
	}, {
		.name		= "exynos4-mipi",
		.driver_data	= (unsigned long)&exynos4_ddata,
	}, {
		.name		= "exynos5250-mipi",
		.driver_data	= (unsigned long)&exynos5210_ddata,
	}, {
		.name		= "exynos5410-mipi",
		.driver_data	= (unsigned long)&exynos5410_ddata,
	},
	{},
};
MODULE_DEVICE_TABLE(platform, exynos_mipi_driver_ids);

static struct platform_driver exynos_mipi_dsi_driver = {
	.probe = exynos_mipi_dsi_probe,
	.remove = __devexit_p(exynos_mipi_dsi_remove),
	.id_table	= exynos_mipi_driver_ids,
	.driver = {
		   .name = "exynos-mipi-dsim",
		   .owner = THIS_MODULE,
		   .pm = &exynos_mipi_dsi_pm_ops,
	},
};

static int exynos_mipi_dsi_register(void)
{
	platform_driver_register(&exynos_mipi_dsi_driver);
	return 0;
}

static void exynos_mipi_dsi_unregister(void)
{
	platform_driver_unregister(&exynos_mipi_dsi_driver);
}

late_initcall(exynos_mipi_dsi_register);
module_exit(exynos_mipi_dsi_unregister);

MODULE_AUTHOR("InKi Dae <inki.dae@samsung.com>");
MODULE_DESCRIPTION("Samusung SoC MIPI-DSI driver");
MODULE_LICENSE("GPL");
