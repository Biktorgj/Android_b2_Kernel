/*
 * Copyright (C) 2011 Samsung Electronics
 * Yongsul Oh <yongsul96.oh@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#ifndef __LINUX_USB_SLP_MULTI_H
#define __LINUX_USB_SLP_MULTI_H

#include <linux/usb/ch9.h>

enum slp_multi_config_id {
	USB_CONFIGURATION_1 = 1,
	USB_CONFIGURATION_2 = 2,
	USB_CONFIGURATION_DUAL = 0xFF,
};

struct slp_multi_platform_data {
	/* for mass_storage nluns, optional */
	unsigned nluns;
	/* for QOS control, optional */
	u32 swfi_latency;

	unsigned nfuncs;
	const char **enable_funcs;
};

enum slp_multi_dev_evt {
	SMDEV_EVT_QOS_CHANGE = 1,
	SMDEV_EVT_STATE_CHANGE,

	SMDEV_EVT_LAST = SMDEV_EVT_STATE_CHANGE,
	SMDEV_EVT_MAXBITS = SMDEV_EVT_LAST + 1
};

struct slp_multi_evt {
	enum slp_multi_dev_evt evt_type;
	struct list_head node;

	union {
		enum usb_device_state ustate;
		s32 qos;
	};
};

#endif /* __LINUX_USB_SLP_MULTI_H */
