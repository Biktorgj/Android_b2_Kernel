/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * EXYNOS5260 - PLL support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef EXYNOS5260_DISP_CLOCK_REINIT
#define EXYNOS5260_DISP_CLOCK_REINIT
int s3c_fb_clock_reinit(void);
int mipi_dsi_clock_reinit(void);
int s5p_dp_clock_reinit(void);
#endif
