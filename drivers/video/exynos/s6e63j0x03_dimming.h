/* linux/drivers/video/exynos/s6e63j0x03_dimming.h
 *
 * MIPI-DSI based S6E63J0X03 AMOLED lcd 1.63 inch panel driver.
 *
 * YoungJun Cho, <yj44.cho@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef	_S6E63J0X03_DIMMING_H
#define	_S6E63J0x03_DIMMING_H

#define	MAX_COLOR_CNT	3
#define	MAX_GRADATION	256

enum {
	V0,
	V5,
	V15,
	V31,
	V63,
	V127,
	V191,
	V255,
	VMAX,
};

struct smart_dimming {
	int	gamma[VMAX][MAX_COLOR_CNT];
	int	volt[MAX_GRADATION][MAX_COLOR_CNT];
	int	volt_vt[MAX_COLOR_CNT];
	int	look_volt[VMAX][MAX_COLOR_CNT];
};

void panel_read_gamma(struct smart_dimming *dimming, const unsigned char *data);
void panel_generate_volt_tbl(struct smart_dimming *dimming);
void panel_get_gamma(struct smart_dimming *dimming, int candela,
							unsigned char *target);
#endif	/* _S6E63J0x03_DIMMING_H */
