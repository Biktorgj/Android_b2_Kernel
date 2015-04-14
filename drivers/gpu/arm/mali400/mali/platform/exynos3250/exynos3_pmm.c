/* drivers/gpu/mali400/mali/platform/exynos3250/exynos3_pmm.c
 *
 * Copyright 2011 by S.LSI. Samsung Electronics Inc.
 * San#24, Nongseo-Dong, Giheung-Gu, Yongin, Korea
 *
 * Samsung SoC Mali400 DVFS driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software FoundatIon.
 */

/**
 * @file exynos3_pmm.c
 * Platform specific Mali driver functions for the exynos 4XXX based platforms
 */
#include "mali_kernel_common.h"
#include "mali_osk.h"
#include "exynos3_pmm.h"
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <mach/regs-pmu.h>

#ifdef CONFIG_MALI400_PROFILING
#include "mali_osk_profiling.h"
#endif

#define MPLLCLK_NAME	"sclk_mpll_pre_div"
#define MOUT0_NAME	"mout_g3d0"
#define GPUCLK_NAME      "sclk_g3d"

struct mali_exynos_dvfs_step {
	unsigned int	rate;
	unsigned int	downthreshold;
	unsigned int	upthreshold;
};

struct mali_exynos_drvdata {
	struct device				*dev;

	const struct mali_exynos_dvfs_step	*steps;
	unsigned int				nr_steps;

	struct clk				*mpll;
	struct clk				*parent;
	struct clk				*sclk;

	mali_power_mode				power_mode;
	unsigned int				dvfs_step;
	unsigned int				load;

	struct work_struct			dvfs_work;
	struct workqueue_struct			*dvfs_workqueue;
};

static struct mali_exynos_drvdata *mali;

#define MALI_DVFS_STEP(freq, down, up) \
	{freq * 1000000, (255 * down) / 100, (255 * up) / 100}

static const struct mali_exynos_dvfs_step mali_exynos_dvfs_step_tbl[] = {
	MALI_DVFS_STEP(134, 0, 255),
	MALI_DVFS_STEP(134, 0, 255)
};

/* PegaW1 */
int mali_gpu_clk;
unsigned int mali_dvfs_utilization;

/* export GPU frequency as a read-only parameter so that it can be read in /sys */
module_param(mali_gpu_clk, int, S_IRUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(mali_gpu_clk, "Mali Current Clock");

#ifdef CONFIG_MALI400_PROFILING
static inline void _mali_osk_profiling_add_gpufreq_event(int rate)
{
	_mali_osk_profiling_add_event(MALI_PROFILING_EVENT_TYPE_SINGLE |
		 MALI_PROFILING_EVENT_CHANNEL_GPU |
		 MALI_PROFILING_EVENT_REASON_SINGLE_GPU_FREQ_VOLT_CHANGE,
		 rate, 0, 0, 0, 0);
}
#else
static inline void _mali_osk_profiling_add_gpufreq_event(int rate)
{
}
#endif

static void mali_exynos_set_dvfs_step(struct mali_exynos_drvdata *mali,
							unsigned int step)
{
	const struct mali_exynos_dvfs_step *next = &mali->steps[step];

	if (mali->dvfs_step == step)
		return;

	clk_set_rate(mali->sclk, next->rate);

	mali_gpu_clk = (int)(clk_get_rate(mali->sclk) / 1000000);

	_mali_osk_profiling_add_gpufreq_event(mali_gpu_clk);

	mali->dvfs_step = step;
}

static void mali_exynos_dvfs_work(struct work_struct *work)
{
	struct mali_exynos_drvdata *mali = container_of(work,
					struct mali_exynos_drvdata, dvfs_work);
	unsigned int step = mali->dvfs_step;
	const struct mali_exynos_dvfs_step *cur = &mali->steps[step];

	if (mali->load > cur->upthreshold)
		++step;
	else if (mali->load < cur->downthreshold)
		--step;

	BUG_ON(step >= mali->nr_steps);

	if (step != mali->dvfs_step)
		mali_exynos_set_dvfs_step(mali, step);
}

void mali_exynos_update_dvfs(unsigned int load)
{
	if (load > 255)
		load = 255;

	mali->load = load;
	mali_dvfs_utilization = load;

	queue_work(mali->dvfs_workqueue, &mali->dvfs_work);
}

_mali_osk_errcode_t mali_platform_power_mode_change(struct device *dev,
						mali_power_mode power_mode)
{
	if (mali->power_mode == power_mode)
		MALI_SUCCESS;
	/* to avoid multiple clk_disable() call */
	else if ((mali->power_mode > MALI_POWER_MODE_ON) &&
					(power_mode > MALI_POWER_MODE_ON)) {
		mali->power_mode = power_mode;
		MALI_SUCCESS;
	}

	switch (power_mode) {
	case MALI_POWER_MODE_ON:
		mali_exynos_set_dvfs_step(mali, 1);
		clk_enable(mali->sclk);
		break;
	case MALI_POWER_MODE_DEEP_SLEEP:
	case MALI_POWER_MODE_LIGHT_SLEEP:
		clk_disable(mali->sclk);
		mali_exynos_set_dvfs_step(mali, 0);
		mali_gpu_clk = 0;
		break;
	}

	mali->power_mode = power_mode;

	MALI_SUCCESS;
}

static mali_bool mali_clk_get(struct mali_exynos_drvdata *mali)
{
	mali->mpll = clk_get(NULL, MPLLCLK_NAME);
	if (IS_ERR(mali->mpll)) {
		MALI_PRINT_ERROR(("failed to get sclk_mpll_pre_div clock"));
		return MALI_FALSE;
	}

	mali->parent = clk_get(NULL, MOUT0_NAME);
	if (IS_ERR(mali->parent)) {
		MALI_PRINT_ERROR(("failed to get parent_clock"));
		return MALI_FALSE;
	}

	mali->sclk = clk_get(NULL, GPUCLK_NAME);
	if (IS_ERR(mali->sclk)) {
		MALI_PRINT_ERROR(("failed to get sclk_clock"));
		return MALI_FALSE;
	}

	return MALI_TRUE;
}

static void mali_clk_put(struct mali_exynos_drvdata *mali,
						mali_bool binc_mali_clock)
{
	if (mali->parent) {
		clk_put(mali->parent);
		mali->parent = NULL;
	}

	if (mali->mpll) {
		clk_put(mali->mpll);
		mali->mpll = NULL;
	}

	if (binc_mali_clock && mali->sclk) {
		clk_put(mali->sclk);
		mali->sclk = NULL;
	}
}

_mali_osk_errcode_t mali_platform_init(struct device *dev)
{
	mali = kzalloc(sizeof(*mali), GFP_KERNEL);
	if (WARN_ON(!mali))
		MALI_ERROR(_MALI_OSK_ERR_NOMEM);

	mali->steps = mali_exynos_dvfs_step_tbl;
	mali->nr_steps = ARRAY_SIZE(mali_exynos_dvfs_step_tbl);

	if (!mali_clk_get(mali)) {
		MALI_PRINT_ERROR(("Failed to get Mali clocks"));
		goto err_clk_put;
	}

	clk_set_parent(mali->parent, mali->mpll);
	clk_set_parent(mali->sclk, mali->parent);

	mali->dvfs_workqueue = create_singlethread_workqueue("mali_dvfs");
	if (WARN_ON(!mali->dvfs_workqueue)) {
		MALI_PRINT_ERROR(("failed to create workqueue"));
		goto err_clk_put;
	}

	mali->power_mode = MALI_POWER_MODE_DEEP_SLEEP;

	INIT_WORK(&mali->dvfs_work, mali_exynos_dvfs_work);

	mali_exynos_set_dvfs_step(mali, 1);

	mali_clk_put(mali, MALI_FALSE);

	MALI_SUCCESS;

err_clk_put:
	mali_clk_put(mali, MALI_TRUE);
	kfree(mali);
	MALI_ERROR(_MALI_OSK_ERR_FAULT);
}

_mali_osk_errcode_t mali_platform_deinit(struct device *dev)
{
	mali_clk_put(mali, MALI_TRUE);

	kfree(mali);

	MALI_SUCCESS;
}
