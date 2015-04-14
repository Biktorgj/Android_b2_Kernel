/* drivers/gpu/mali400/mali/platform/exynos3250/exynos3.c
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
 * @file exynos3.c
 * Platform specific Mali driver functions for the exynos 3XXX based platforms
 */
#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/mali/mali_utgard.h>
#include "mali_kernel_common.h"

#include <linux/irq.h>
#include <plat/devs.h>

#include "exynos3_pmm.h"

#define MALI_GP_IRQ		EXYNOS3_IRQ_GP_3D
#define MALI_PP0_IRQ		EXYNOS3_IRQ_PP0_3D
#define MALI_PP1_IRQ		EXYNOS3_IRQ_PP1_3D
#define MALI_GP_MMU_IRQ		EXYNOS3_IRQ_GPMMU_3D
#define MALI_PP0_MMU_IRQ	EXYNOS3_IRQ_PPMMU0_3D
#define MALI_PP1_MMU_IRQ	EXYNOS3_IRQ_PPMMU1_3D

static struct resource mali_gpu_resources[] = {
	MALI_GPU_RESOURCES_MALI400_MP2(0x13000000,
						MALI_GP_IRQ, MALI_GP_MMU_IRQ,
						MALI_PP0_IRQ, MALI_PP0_MMU_IRQ,
						MALI_PP1_IRQ, MALI_PP1_MMU_IRQ)
};

static struct mali_gpu_device_data mali_gpu_data = {
	.shared_mem_size = 256 * 1024 * 1024, /* 256MB */
	.fb_start = 0x40000000,
	.fb_size = 0xb1000000,
	.utilization_interval = 100, /* 100ms */
	.utilization_handler = mali_exynos_update_dvfs,
};

int mali_platform_device_register(void)
{
	int err;

	MALI_DEBUG_PRINT(4, ("mali_platform_device_register() called\n"));

	/* Connect resources to the device */
	err = platform_device_add_resources(&exynos4_device_g3d,
						mali_gpu_resources,
						sizeof(mali_gpu_resources) /
						sizeof(mali_gpu_resources[0]));
	if (0 == err) {
		err = platform_device_add_data(&exynos4_device_g3d,
						&mali_gpu_data,
						sizeof(mali_gpu_data));
		if (0 == err) {
			mali_platform_init(&(exynos4_device_g3d.dev));

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37))
			pm_runtime_set_autosuspend_delay(&(exynos4_device_g3d.dev), 200);
			pm_runtime_use_autosuspend(&(exynos4_device_g3d.dev));
#endif
			pm_runtime_enable(&(exynos4_device_g3d.dev));
			return 0;
		}

	}
	return err;
}

void mali_platform_device_unregister(void)
{
	MALI_DEBUG_PRINT(4, ("mali_platform_device_unregister() called\n"));

	mali_platform_deinit(&(exynos4_device_g3d.dev));
}
