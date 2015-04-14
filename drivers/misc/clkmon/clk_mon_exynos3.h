/*
 * drivers/misc/clkmon/clk_mon_exynos3.h
 *
 * Register address based on Exynos3 CPU
 */

#ifndef _CLK_MON_S5P_H_
#define _CLK_MON_S5P_H_

#include <mach/regs-pmu.h>
#include <mach/regs-clock.h>
#include <linux/clk_mon.h>

//Power Domain
#define CLK_MON_PMU_CAM_CONF            EXYNOS3_CAM_CONFIGURATION
#define CLK_MON_PMU_MFC_CONF            EXYNOS3_MFC_CONFIGURATION
#define CLK_MON_PMU_G3D_CONF            EXYNOS3_G3D_CONFIGURATION
#define CLK_MON_PMU_LCD0_CONF		EXYNOS3_LCD0_CONFIGURATION
#define CLK_MON_PMU_ISP_CONF		EXYNOS3_ISP_CONFIGURATION
#define CLK_MON_PWR_EN			EXYNOS_INT_LOCAL_PWR_EN
//Clock Gate
#define CLK_MON_CGATE_IP_LEFTBUS    EXYNOS3_CLKGATE_IP_LEFTBUS
#define CLK_MON_CGATE_IP_RIGHTBUS   EXYNOS3_CLKGATE_IP_RIGHTBUS
#define CLK_MON_CGATE_BLOCK         EXYNOS3_CLKGATE_BLOCK
#define CLK_MON_CGATE_IP_PERIR      EXYNOS3_CLKGATE_IP_PERIR
#define CLK_MON_CGATE_IP_PERIL      EXYNOS3_CLKGATE_IP_PERIL
#define CLK_MON_CGATE_IP_FSYS       EXYNOS3_CLKGATE_IP_FSYS
#define CLK_MON_CGATE_IP_G3D        EXYNOS3_CLKGATE_IP_G3D
#define CLK_MON_CGATE_IP_CAM        EXYNOS3_CLKGATE_IP_CAM
#define CLK_MON_CGATE_IP_MFC        EXYNOS3_CLKGATE_IP_MFC
#define CLK_MON_CGATE_IP_ISP        EXYNOS3_CLKGATE_IP_ISP
#define CLK_MON_CGATE_IP_CPU        EXYNOS3_CLKGATE_IP_CPU
#define CLK_MON_CGATE_IP_LCD        EXYNOS3_CLKGATE_IP_LCD
#define CLK_MON_CGATE_IP_DMC0       EXYNOS3_CLKGATE_IP_DMC0


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
