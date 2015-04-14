/*
 * Exynos Generic power domain support.
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * Implementation of Exynos specific power domain control which is used in
 * conjunction with runtime-pm. Support for both device-tree and non-device-tree
 * based power domain support is included.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <mach/pm_domains.h>
#include <mach/bts.h>

/* exynos4 power domain */
EXYNOS_COMMON_GPD(exynos4_pd_mfc, EXYNOS4_MFC_CONFIGURATION, "pd-mfc");
EXYNOS_COMMON_GPD(exynos4_pd_g3d, EXYNOS4_G3D_CONFIGURATION, "pd-g3d");
EXYNOS_COMMON_GPD(exynos4_pd_lcd0, EXYNOS4_LCD0_CONFIGURATION, "pd-lcd0");
#ifdef CONFIG_S5P_DEV_FIMD0
EXYNOS_COMMON_GPD(exynos4_spd_fimd0, NULL, "pd-fimd0");
EXYNOS_COMMON_GPD(exynos4_spd_dsim0, NULL, "pd-dsim0");
#endif
EXYNOS_COMMON_GPD(exynos4_pd_cam, EXYNOS4_CAM_CONFIGURATION, "pd-cam");
#ifdef CONFIG_S5P_DEV_FIMC0
EXYNOS_COMMON_GPD(exynos4_spd_fimc0, NULL, "pd-fimc0");
#endif
#ifdef CONFIG_S5P_DEV_FIMC1
EXYNOS_COMMON_GPD(exynos4_spd_fimc1, NULL, "pd-fimc1");
#endif
#ifdef CONFIG_S5P_DEV_FIMC2
EXYNOS_COMMON_GPD(exynos4_spd_fimc2, NULL, "pd-fimc2");
#endif
#ifdef CONFIG_S5P_DEV_FIMC3
EXYNOS_COMMON_GPD(exynos4_spd_fimc3, NULL, "pd-fimc3");
#endif
#ifdef CONFIG_VIDEO_EXYNOS_MIPI_CSIS
EXYNOS_COMMON_GPD(exynos4_spd_csis0, NULL, "pd-csis0");
EXYNOS_COMMON_GPD(exynos4_spd_csis1, NULL, "pd-csis1");
#endif
#if defined(CONFIG_EXYNOS4_DEV_JPEG) || defined(CONFIG_EXYNOS5_DEV_JPEG)
EXYNOS_COMMON_GPD(exynos4_spd_jpeg, NULL, "pd-jpeg");
#endif
EXYNOS_COMMON_GPD(exynos4_pd_tv, EXYNOS4_TV_CONFIGURATION, "pd-tv");
#ifdef CONFIG_S5P_DEV_TV
EXYNOS_COMMON_GPD(exynos4_spd_hdmi, NULL, "pd-hdmi");
EXYNOS_COMMON_GPD(exynos4_spd_mixer, NULL, "pd-mixer");
#endif
EXYNOS_COMMON_GPD(exynos4_pd_isp, EXYNOS4_ISP_CONFIFURATION, "pd-isp");

/* exynos4 bts initialize function */
static int exynos4_pm_domain_bts_on(struct exynos_pm_domain *domain)
{
	bts_initialize(domain->pd.name, true);

	return 0;
}

static void __iomem *exynos4_mfc_pwr_reg[] = {
	EXYNOS4_CMU_CLKSTOP_MFC_LOWPWR,
	EXYNOS4_CMU_RESET_MFC_LOWPWR,
};

static void __iomem *exynos4_g3d_pwr_reg[] = {
	EXYNOS4_CMU_CLKSTOP_G3D_LOWPWR,
	EXYNOS4_CMU_RESET_G3D_LOWPWR,
};

static void __iomem *exynos4_lcd0_pwr_reg[] = {
	EXYNOS4_CMU_CLKSTOP_LCD0_LOWPWR,
	EXYNOS4_CMU_RESET_LCD0_LOWPWR,
};

static void __iomem *exynos4_cam_pwr_reg[] = {
	EXYNOS4_CMU_CLKSTOP_CAM_LOWPWR,
	EXYNOS4_CMU_RESET_CAM_LOWPWR,
};

static void __iomem *exynos4_tv_pwr_reg[] = {
	EXYNOS4_CMU_CLKSTOP_TV_LOWPWR,
	EXYNOS4_CMU_RESET_TV_LOWPWR,
};

static void __iomem *exynos4_isp_pwr_reg[] = {
	EXYNOS4X12_CMU_CLKSTOP_ISP_LOWPWR,
	EXYNOS4X12_CMU_SYSCLK_ISP_LOWPWR,
	EXYNOS4X12_CMU_RESET_ISP_LOWPWR,
};

int __init exynos4_pm_domain_init(void)
{
	int i;

	exynos_pm_powerdomain_init(&exynos4_pd_mfc);
	exynos_pm_powerdomain_init(&exynos4_pd_g3d);
	exynos_pm_powerdomain_init(&exynos4_pd_lcd0);
	exynos_pm_powerdomain_init(&exynos4_pd_cam);
	exynos_pm_powerdomain_init(&exynos4_pd_tv);
	exynos_pm_powerdomain_init(&exynos4_pd_isp);

	exynos_pm_add_reg(&exynos4_pd_mfc, EXYNOS_PROCESS_BEFORE,
			EXYNOS_PROCESS_ONOFF, EXYNOS4_MFC_OPTION, 2);
	for (i = 0; i < ARRAY_SIZE(exynos4_mfc_pwr_reg); ++i)
		exynos_pm_add_reg(&exynos4_pd_mfc, EXYNOS_PROCESS_BEFORE,
				EXYNOS_PROCESS_ONOFF, exynos4_mfc_pwr_reg[i],
				0);

	exynos_pm_add_reg(&exynos4_pd_g3d, EXYNOS_PROCESS_BEFORE,
			EXYNOS_PROCESS_ONOFF, EXYNOS4_G3D_OPTION, 2);
	for (i = 0; i < ARRAY_SIZE(exynos4_g3d_pwr_reg); ++i)
		exynos_pm_add_reg(&exynos4_pd_g3d, EXYNOS_PROCESS_BEFORE,
				EXYNOS_PROCESS_ONOFF, exynos4_g3d_pwr_reg[i],
				0);

	exynos_pm_add_reg(&exynos4_pd_lcd0, EXYNOS_PROCESS_BEFORE,
			EXYNOS_PROCESS_ONOFF, EXYNOS4_LCD0_OPTION, 2);
	for (i = 0; i < ARRAY_SIZE(exynos4_lcd0_pwr_reg); ++i)
		exynos_pm_add_reg(&exynos4_pd_lcd0, EXYNOS_PROCESS_BEFORE,
				EXYNOS_PROCESS_ONOFF, exynos4_lcd0_pwr_reg[i],
				0);

	exynos_pm_add_reg(&exynos4_pd_cam, EXYNOS_PROCESS_BEFORE,
			EXYNOS_PROCESS_ONOFF, EXYNOS4_CAM_OPTION, 2);
	for (i = 0; i < ARRAY_SIZE(exynos4_cam_pwr_reg); ++i)
		exynos_pm_add_reg(&exynos4_pd_cam, EXYNOS_PROCESS_BEFORE,
				EXYNOS_PROCESS_ONOFF, exynos4_cam_pwr_reg[i],
				0);

	exynos_pm_add_reg(&exynos4_pd_tv, EXYNOS_PROCESS_BEFORE,
			EXYNOS_PROCESS_ONOFF, EXYNOS4_TV_OPTION, 2);
	for (i = 0; i < ARRAY_SIZE(exynos4_tv_pwr_reg); ++i)
		exynos_pm_add_reg(&exynos4_pd_tv, EXYNOS_PROCESS_BEFORE,
				EXYNOS_PROCESS_ONOFF, exynos4_tv_pwr_reg[i],
				0);

	exynos_pm_add_reg(&exynos4_pd_isp, EXYNOS_PROCESS_BEFORE,
			EXYNOS_PROCESS_ONOFF, EXYNOS4_ISP_OPTION, 2);
	for (i = 0; i < ARRAY_SIZE(exynos4_isp_pwr_reg); ++i)
		exynos_pm_add_reg(&exynos4_pd_isp, EXYNOS_PROCESS_BEFORE,
				EXYNOS_PROCESS_ONOFF, exynos4_isp_pwr_reg[i],
				0);

#ifdef CONFIG_S5P_DEV_MFC
	exynos_pm_add_platdev(&exynos4_pd_mfc, &s5p_device_mfc);
	exynos_pm_add_clk(&exynos4_pd_mfc, &s5p_device_mfc.dev, "mfc");
	exynos_pm_add_platdev(&exynos4_pd_mfc, &SYSMMU_PLATDEV(mfc_lr));
	exynos_pm_add_clk(&exynos4_pd_mfc, &SYSMMU_PLATDEV(mfc_lr).dev, "sysmmu");
#endif
	exynos_pm_add_platdev(&exynos4_pd_g3d, &exynos4_device_g3d);
	exynos_pm_add_callback(&exynos4_pd_g3d, EXYNOS_PROCESS_AFTER, EXYNOS_PROCESS_ON,
				true, exynos4_pm_domain_bts_on);
#ifdef CONFIG_S5P_DEV_FIMD0
	exynos_pm_powerdomain_init(&exynos4_spd_fimd0);
	exynos_pm_add_platdev(&exynos4_spd_fimd0, &s5p_device_fimd0);
	exynos_pm_add_clk(&exynos4_spd_fimd0, &s5p_device_fimd0.dev, "fimd");
	exynos_pm_add_platdev(&exynos4_spd_fimd0, &SYSMMU_PLATDEV(fimd0));
	exynos_pm_add_clk(&exynos4_spd_fimd0, &SYSMMU_PLATDEV(fimd0).dev, "sysmmu");
	exynos_pm_add_subdomain(&exynos4_pd_lcd0, &exynos4_spd_fimd0, true);

	exynos_pm_powerdomain_init(&exynos4_spd_dsim0);
#ifdef CONFIG_FB_MIPI_DSIM
	exynos_pm_add_platdev(&exynos4_spd_dsim0, &s5p_device_mipi_dsim0);
#endif
	exynos_pm_add_clk(&exynos4_spd_dsim0, NULL, "dsim0");
	exynos_pm_add_subdomain(&exynos4_pd_lcd0, &exynos4_spd_dsim0, true);
#endif
#ifdef CONFIG_S5P_DEV_FIMC0
	exynos_pm_powerdomain_init(&exynos4_spd_fimc0);
	exynos_pm_add_platdev(&exynos4_spd_fimc0, &s5p_device_fimc0);
	exynos_pm_add_clk(&exynos4_spd_fimc0, &s5p_device_fimc0.dev, "fimc");
	exynos_pm_add_platdev(&exynos4_spd_fimc0, &SYSMMU_PLATDEV(fimc0));
	exynos_pm_add_clk(&exynos4_spd_fimc0, &SYSMMU_PLATDEV(fimc0).dev, "sysmmu");
	exynos_pm_add_subdomain(&exynos4_pd_cam, &exynos4_spd_fimc0, true);
#endif
#ifdef CONFIG_S5P_DEV_FIMC1
	exynos_pm_powerdomain_init(&exynos4_spd_fimc1);
	exynos_pm_add_platdev(&exynos4_spd_fimc1, &s5p_device_fimc1);
	exynos_pm_add_clk(&exynos4_spd_fimc1, &s5p_device_fimc1.dev, "fimc");
	exynos_pm_add_platdev(&exynos4_spd_fimc1, &SYSMMU_PLATDEV(fimc1));
	exynos_pm_add_clk(&exynos4_spd_fimc1, &SYSMMU_PLATDEV(fimc1).dev, "sysmmu");
	exynos_pm_add_subdomain(&exynos4_pd_cam, &exynos4_spd_fimc1, true);
#endif
#ifdef CONFIG_S5P_DEV_FIMC2
	exynos_pm_powerdomain_init(&exynos4_spd_fimc2);
	exynos_pm_add_platdev(&exynos4_spd_fimc2, &s5p_device_fimc2);
	exynos_pm_add_clk(&exynos4_spd_fimc2, &s5p_device_fimc2.dev, "fimc");
	exynos_pm_add_platdev(&exynos4_spd_fimc2, &SYSMMU_PLATDEV(fimc2));
	exynos_pm_add_clk(&exynos4_spd_fimc2, &SYSMMU_PLATDEV(fimc2).dev, "sysmmu");
	exynos_pm_add_subdomain(&exynos4_pd_cam, &exynos4_spd_fimc2, true);
#endif
#ifdef CONFIG_S5P_DEV_FIMC3
	exynos_pm_powerdomain_init(&exynos4_spd_fimc3);
	exynos_pm_add_platdev(&exynos4_spd_fimc3, &s5p_device_fimc3);
	exynos_pm_add_clk(&exynos4_spd_fimc3, &s5p_device_fimc3.dev, "fimc");
	exynos_pm_add_platdev(&exynos4_spd_fimc3, &SYSMMU_PLATDEV(fimc3));
	exynos_pm_add_clk(&exynos4_spd_fimc3, &SYSMMU_PLATDEV(fimc3).dev, "sysmmu");
	exynos_pm_add_subdomain(&exynos4_pd_cam, &exynos4_spd_fimc3, true);
#endif
#ifdef CONFIG_VIDEO_EXYNOS_MIPI_CSIS
	exynos_pm_powerdomain_init(&exynos4_spd_csis0);
	exynos_pm_add_platdev(&exynos4_spd_csis0, &s5p_device_mipi_csis0);
	exynos_pm_add_clk(&exynos4_spd_csis0, &s5p_device_mipi_csis0.dev, "csis");
	exynos_pm_add_subdomain(&exynos4_pd_cam, &exynos4_spd_csis0, true);

	exynos_pm_powerdomain_init(&exynos4_spd_csis1);
	exynos_pm_add_platdev(&exynos4_spd_csis1, &s5p_device_mipi_csis1);
	exynos_pm_add_clk(&exynos4_spd_csis1, &s5p_device_mipi_csis1.dev, "csis");
	exynos_pm_add_subdomain(&exynos4_pd_cam, &exynos4_spd_csis1, true);
#endif
#if defined(CONFIG_EXYNOS4_DEV_JPEG) || defined(CONFIG_EXYNOS5_DEV_JPEG)
	exynos_pm_powerdomain_init(&exynos4_spd_jpeg);
	exynos_pm_add_platdev(&exynos4_spd_jpeg, &s5p_device_jpeg);
	exynos_pm_add_clk(&exynos4_spd_jpeg, &s5p_device_jpeg.dev, "jpeg");
	exynos_pm_add_platdev(&exynos4_spd_jpeg, &SYSMMU_PLATDEV(jpeg));
	exynos_pm_add_clk(&exynos4_spd_jpeg, &SYSMMU_PLATDEV(jpeg).dev, "sysmmu");
	exynos_pm_add_subdomain(&exynos4_pd_cam, &exynos4_spd_jpeg, true);
#endif
#ifdef CONFIG_S5P_DEV_TV
	exynos_pm_add_platdev(&exynos4_pd_tv, &SYSMMU_PLATDEV(tv));
	exynos_pm_add_clk(&exynos4_pd_tv, &SYSMMU_PLATDEV(tv).dev, "sysmmu");

	exynos_pm_powerdomain_init(&exynos4_spd_hdmi);
	exynos_pm_add_platdev(&exynos4_spd_hdmi, &s5p_device_hdmi);
	exynos_pm_add_clk(&exynos4_spd_hdmi, &s5p_device_hdmi.dev, "hdmi");
	exynos_pm_add_subdomain(&exynos4_pd_tv, &exynos4_spd_hdmi, true);

	exynos_pm_powerdomain_init(&exynos4_spd_mixer);
	exynos_pm_add_platdev(&exynos4_spd_mixer, &s5p_device_mixer);
	exynos_pm_add_clk(&exynos4_spd_mixer, &s5p_device_mixer.dev, "mixer");
	exynos_pm_add_subdomain(&exynos4_pd_tv, &exynos4_spd_mixer, true);
#endif
#ifdef CONFIG_EXYNOS_DEV_FIMC_IS
	if (soc_is_exynos3470()) {
		exynos_pm_add_platdev(&exynos4_pd_isp,
				&exynos4_device_fimc_is);

		exynos_pm_add_platdev(&exynos4_pd_isp, &SYSMMU_PLATDEV(isp0));
		exynos_pm_add_platdev(&exynos4_pd_isp, &SYSMMU_PLATDEV(isp1));

		exynos_pm_add_subdomain(&exynos4_pd_cam, &exynos4_pd_isp,
					false);

		exynos_pm_add_platdev(&exynos4_pd_isp, &SYSMMU_PLATDEV(camif0));
		exynos_pm_add_platdev(&exynos4_pd_isp, &SYSMMU_PLATDEV(camif1));
	}
#endif
#ifdef CONFIG_EXYNOS4_DEV_FIMC_LITE
	if (soc_is_exynos3470()) {
		exynos_pm_add_platdev(&exynos4_pd_isp, &SYSMMU_PLATDEV(camif0));
		exynos_pm_add_platdev(&exynos4_pd_isp, &SYSMMU_PLATDEV(camif1));
		exynos_pm_add_platdev(&exynos4_pd_isp, &SYSMMU_PLATDEV(camif2));
	}
#endif

	bts_initialize("pd-g3d", true);
	bts_initialize("pd-g2d", true);

	return 0;
}
arch_initcall(exynos4_pm_domain_init);
