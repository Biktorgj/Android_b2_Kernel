/*
 * arch/arm/mach-exynos/sec-switch.c
 *
 * c source file supporting MUIC common platform device register
 *
 * Copyright (C) 2010 Samsung Electronics
 * Seung-Jin Hahn <sjin.hahn@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/power_supply.h>
#include <linux/module.h>

#include <linux/max14577-muic.h>
#include <linux/muic.h>

/* switch device header */
#ifdef CONFIG_SWITCH
#include <linux/switch.h>
#endif

/* USB header */
#include <plat/udc-hs.h>
#include <plat/devs.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>

#ifdef CONFIG_SWITCH
static struct switch_dev switch_dock = {
	.name = "dock",
};

static struct switch_dev switch_usb = {
	.name = "usb_cable",
};
#endif

extern struct class *sec_class;

struct device *switch_device;

/* charger cable state */
#if defined(CONFIG_MACH_B2)
bool is_cable_attached;
#else
extern bool is_cable_attached;
#endif
bool is_jig_attached;

static void muic_init_cb(void)
{
#ifdef CONFIG_SWITCH
	int ret;

	/* for CarDock, DeskDock */
	ret = switch_dev_register(&switch_dock);
	if (ret < 0)
		pr_err("%s:%s Failed to register dock switch(%d)\n",
			MUIC_DEV_NAME, __func__, ret);

	/* for usb client mode */
	ret = switch_dev_register(&switch_usb);
	if (ret < 0)
		pr_err("%s:%s Failed to register usb switch(%d)\n",
			MUIC_DEV_NAME, __func__, ret);
	else
		pr_info("%s:%s: switch_usb switch_dev registered\n",
			MUIC_DEV_NAME, __func__);
#endif
}

/* usb cable call back function */
static void muic_usb_cb(u8 usb_mode)
{
#ifdef CONFIG_USB_GADGET_SWITCH
	struct usb_gadget *gadget = platform_get_drvdata(&s3c_device_usb_hsotg);
	int g_state;
#endif

	pr_info("%s:%s MUIC usb_cb:%d\n", MUIC_DEV_NAME, __func__, usb_mode);

	switch (usb_mode) {
	case USB_CABLE_DETACHED:
		pr_info("usb: muic: USB_CABLE_DETACHED(%d)\n", usb_mode);
		#ifdef CONFIG_SWITCH
		switch_set_state(&switch_usb, usb_mode);
		#endif
#ifdef CONFIG_USB_GADGET_SWITCH
		if (gadget) {
			g_state = usb_gadget_vbus_disconnect(gadget);
			if (g_state < 0)
				pr_err("%s:gadget_vbus dis-connect failed(%d)\n",
					__func__, g_state);
		}
#endif
		break;
	case USB_CABLE_ATTACHED:
		pr_info("usb: muic: USB_CABLE_ATTACHED(%d)\n", usb_mode);
#ifdef CONFIG_USB_GADGET_SWITCH
		if (gadget) {
			g_state = usb_gadget_vbus_connect(gadget);
			if (g_state < 0) {
				pr_err("%s:gadget_vbus dis-connect failed(%d)\n",
					__func__, g_state);
				return;
			}
		}
#endif
		#ifdef CONFIG_SWITCH
		switch_set_state(&switch_usb, usb_mode);
		#endif
		break;
	default:
		pr_info("usb: muic: invalid mode%d\n", usb_mode);
	}
}
EXPORT_SYMBOL(muic_usb_cb);
#ifdef CONFIG_TOUCHSCREEN_MELFAS_W
extern void tsp_charger_infom(bool en);
#endif
extern int current_cable_type;
static int muic_charger_cb(enum muic_attached_dev cable_type)
{
	struct power_supply *psy = power_supply_get_by_name("battery");
	union power_supply_propval value;

	pr_info("%s:%s %d\n", MUIC_DEV_NAME, __func__, cable_type);

	switch (cable_type) {
	case ATTACHED_DEV_NONE_MUIC:
	case ATTACHED_DEV_OTG_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
		is_cable_attached = false;
		break;
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_CDP_MUIC:
#if defined(CONFIG_MUIC_SUPPORT_POGO_USB)
	case ATTACHED_DEV_POGO_USB_MUIC:
#endif /* CONFIG_MUIC_SUPPORT_POGO_USB */
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_USB;
		is_cable_attached = true;
		break;
	case ATTACHED_DEV_TA_MUIC:
#if defined(CONFIG_MUIC_SUPPORT_POGO_TA)
	case ATTACHED_DEV_POGO_TA_MUIC:
#endif /* CONFIG_MUIC_SUPPORT_POGO_TA */
#if defined(CONFIG_MUIC_ADC_ADD_PLATFORM_DEVICE)
	case ATTACHED_DEV_2A_TA_MUIC:
	case ATTACHED_DEV_UNKNOWN_TA_MUIC:
#endif /* CONFIG_MUIC_ADC_ADD_PLATFORM_DEVICE */
	case ATTACHED_DEV_DESKDOCK_MUIC:
	case ATTACHED_DEV_CARDOCK_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_MAINS;
		is_cable_attached = true;
		break;
	default:
		pr_err("%s:%s invalid type:%d\n", MUIC_DEV_NAME, __func__, cable_type);
		return -EINVAL;
	}

#if defined(CONFIG_TOUCHSCREEN_MELFAS_W)
	tsp_charger_infom(current_cable_type != POWER_SUPPLY_TYPE_BATTERY ? true : false );
#endif

	if (!psy || !psy->set_property)
		pr_err("%s: fail to get battery psy\n", __func__);
	else {
		value.intval = current_cable_type;
		psy->set_property(psy, POWER_SUPPLY_PROP_ONLINE, &value);
	}
	return 0;
}

static void muic_dock_cb(int type)
{
	pr_info("%s:%s MUIC dock type=%d\n", MUIC_DEV_NAME, __func__, type);
#ifdef CONFIG_SWITCH
	switch_set_state(&switch_dock, type);
#endif
}

int muic_get_jig_state(void)
{
	return is_jig_attached;
}

static void muic_set_jig_state(bool jig_attached)
{
	pr_info("%s:%s jig attach: %d\n", MUIC_DEV_NAME, __func__, jig_attached);
	is_jig_attached = jig_attached;
}

struct sec_switch_data sec_switch_data = {
	.init_cb = muic_init_cb,
	.usb_cb = muic_usb_cb,
	.charger_cb = muic_charger_cb,
	.dock_cb = muic_dock_cb,
	.set_jig_state_cb = muic_set_jig_state,
};

static int __init sec_switch_init(void)
{
	int ret = 0;
	switch_device = device_create(sec_class, NULL, 0, NULL, "switch");
	if (IS_ERR(switch_device)) {
		pr_err("%s:%s Failed to create device(switch)!\n",
				MUIC_DEV_NAME, __func__);
		return -ENODEV;
	}

	return ret;
};

device_initcall(sec_switch_init);
