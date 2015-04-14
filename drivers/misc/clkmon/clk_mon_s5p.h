/* linux/arch/arm/mach-exynos/include/mach/regs-pmu.h
 *
 * Copyright (c) 2010-2011 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * EXYNOS4 - Power management unit definition
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef _CLK_MON_S5P_H_
#define _CLK_MON_S5P_H_

#include <mach/regs-pmu.h>
#include <mach/regs-clock.h>
#include <linux/clk_mon.h>

#define CLK_MON_PWR_EN                          S5P_INT_LOCAL_PWR_EN

#define CLK_MON_PMU_CAM_CONF            S5P_PMU_CAM_CONF
#define CLK_MON_PMU_TV_CONF                     S5P_PMU_TV_CONF
#define CLK_MON_PMU_MFC_CONF            S5P_PMU_MFC_CONF
#define CLK_MON_PMU_G3D_CONF            S5P_PMU_G3D_CONF
#define CLK_MON_PMU_LCD0_CONF           S5P_PMU_LCD0_CONF
#define CLK_MON_PMU_MAU_CONF            S5P_PMU_MAUDIO_CONF
#define CLK_MON_PMU_ISP_CONF            S5P_PMU_ISP_CONF
#define CLK_MON_PMU_GPS_CONF            S5P_PMU_GPS_CONF
#define CLK_MON_PMU_GPS_ALIVE_CONF      S5P_PMU_GPS_ALIVE_CONF

#define CLK_MON_CGATE_IP_LEFTBUS    EXYNOS4_CLKGATE_IP_LEFTBUS
#define CLK_MON_CGATE_IP_IMAGE      EXYNOS4_CLKGATE_IP_IMAGE_4212
#define CLK_MON_CGATE_IP_RIGHTBUS   EXYNOS4_CLKGATE_IP_RIGHTBUS
#define CLK_MON_CGATE_IP_PERIR      EXYNOS4_CLKGATE_IP_PERIR_4212
#define CLK_MON_CGATE_IP_CAM        EXYNOS4_CLKGATE_IP_CAM
#define CLK_MON_CGATE_IP_TV         EXYNOS4_CLKGATE_IP_TV
#define CLK_MON_CGATE_IP_MFC        EXYNOS4_CLKGATE_IP_MFC
#define CLK_MON_CGATE_IP_G3D        EXYNOS4_CLKGATE_IP_G3D
#define CLK_MON_CGATE_IP_LCD0       EXYNOS4_CLKGATE_IP_LCD0
#define CLK_MON_CGATE_IP_ISP        EXYNOS4_CLKGATE_IP_ISP
#define CLK_MON_CGATE_IP_FSYS       EXYNOS4_CLKGATE_IP_FSYS
#define CLK_MON_CGATE_IP_GPS        EXYNOS4_CLKGATE_IP_GPS
#define CLK_MON_CGATE_IP_PERIL      EXYNOS4_CLKGATE_IP_PERIL
#define CLK_MON_CGATE_BLOCK         EXYNOS4_CLKGATE_BLOCK
#define CLK_MON_CGATE_IP_DMC        EXYNOS4_CLKGATE_IP_DMC
#define CLK_MON_CGATE_IP_CPU        EXYNOS4_CLKGATE_IP_CPU


static inline unsigned int vaddr_to_paddr(void *vaddr, int mode)
{
        unsigned int paddr = (unsigned int)vaddr;

        if (mode == PWR_REG) {
                paddr &= 0x0000ffff;
                paddr |= 0x10020000;
        } else if (mode == CLK_REG) {
                unsigned int tmp_high, tmp_low;
                tmp_low = paddr & 0x0000ffff;
                tmp_high = paddr & 0xffff0000;
                tmp_high -= 0xE80D0000;
                paddr = tmp_high | tmp_low;
        }

        return paddr;
}

#endif

