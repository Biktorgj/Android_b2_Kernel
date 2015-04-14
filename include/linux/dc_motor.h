/* include/linux/dc_motor.h
 *
 * Copyright (C) 2011 Samsung Electronics Co. Ltd. All Rights Reserved.
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

#ifndef _LINUX_DC_MOTOR_H
#define _LINUX_DC_MOTOR_H

#include <linux/hrtimer.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/wakelock.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/timed_output.h>

#if 0//!defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
#define DC_MOTOR_LOG
#endif

#define DC_MOTOR_VOL_MAX 28
#define DC_MOTOR_VOL_MIN 12
#define DC_MOTOR_VOL_LIMIT 27

struct dc_motor_platform_data {
	int max_timeout;
	void (*power) (bool on);
	void (*set_voltage) (int val);
};

struct dc_motor_drvdata {
	struct timed_output_dev dev;
	struct hrtimer timer;
	struct work_struct work;
	void (*power) (bool on);
	void (*set_voltage) (int val);
	spinlock_t lock;
	bool running;
	int timeout;
	int max_timeout;
};

#if defined(CONFIG_VIBETONZ)
extern void vibtonz_enable(bool en);
extern void vibtonz_set_voltage(int val);
#endif

#endif	/* _LINUX_DC_MOTOR_H */
