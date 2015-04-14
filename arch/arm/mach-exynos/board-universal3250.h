/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __MACH_EXYNOS_BOARD_UNIVERSAL3250_H
#define __MACH_EXYNOS_BOARD_UNIVERSAL3250_H

#if defined(CONFIG_BATTERY_SAMSUNG_P1X)
#include <linux/battery/sec_charging_common.h>
extern int current_cable_type;
#endif

void exynos3_universal3250_clock_init(void);
void exynos3_universal3250_mmc_init(void);
void exynos3_universal3250_battery_init(void);
void exynos3_universal3250_input_init(void);
void exynos3_universal3250_display_init(void);
void exynos3_b2_mfd_init(void);
void exynos3_universal3250_power_init(void);
void exynos3_universal3250_usb_init(void);
void exynos3_universal3250_media_init(void);
void exynos3_b2_audio_init(void);
void exynos3_b2_sensor_init(void);
void exynos3_b2_fpga_init(void);
void exynos3_universal3250_camera_init(void);
void exynos3_b2_thermistor_init(void);
void exynos3_universal3250_gpio_init(void);
void exynos3_universal3250_vibrator_init(void);
void config_init_gpio_tables(void);
#endif
