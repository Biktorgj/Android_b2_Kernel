/*
 * Gadget Driver for SLP based on Android
 *
 * Copyright (C) 2008 Google, Inc.
 * Author: Mike Lockwood <lockwood@android.com>
 * Modified : Yongsul Oh <yongsul96.oh@samsung.com>
 *
 * Heavily based on android.c
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

/* #define DEBUG */
/* #define VERBOSE_DEBUG */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/sched.h>

#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/utsname.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>

#include <linux/usb/ch9.h>
#include <linux/usb/composite.h>
#include <linux/usb/gadget.h>
#include <linux/usb/slp_multi.h>
#include <asm/system_info.h>
#include <linux/pm_qos.h>
#include <linux/of.h>
#include <linux/workqueue.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/idr.h>
#include <linux/spinlock.h>

#include "gadget_chips.h"

/*
 * When the feature "CONFIG_USB_G_SLP" is selected, we cannot choose
 * "CONFIG_USB_DUN_SUPPORT" feature. Because that feature is tightly
 * linked to "CONFIG_USB_G_ANDROID" feature in this project.
 * But we have to use that feature, so we just defined it in this code.
 * That feature is only for SAMSUNG, and it works only in "f_acm.c"
 */
#define CONFIG_USB_DUN_SUPPORT	/* for data-router */

/*
 * Kbuild is not very cooperative with respect to linking separately
 * compiled library objects into one module.  So for now we won't use
 * separate compilation ... ensuring init/exit sections work to shrink
 * the runtime footprint, and giving us at least some parts of what
 * a "gcc --combine ... part1.c part2.c part3.c ... " build would.
 */
#include "usbstring.c"
#include "config.c"
#include "epautoconf.c"
#include "composite.c"

#ifdef CONFIG_ARCH_MSM
#include "f_qdss.c"
#include "f_rmnet.c"
#include "u_bam.c"
#include "u_rmnet_ctrl_smd.c"
#include "u_rmnet_ctrl_qti.c"
#include "u_ctrl_hsic.c"
#include "u_data_hsic.c"
#include "u_ctrl_hsuart.c"
#include "u_data_hsuart.c"
#endif

#include "f_mass_storage.c"
#include "u_serial.c"
#ifdef CONFIG_USB_DUN_SUPPORT
#include "serial_acm_slp.c"
#endif
#include "f_sdb.c"
#include "f_acm.c"
#include "f_mtp_slp.c"
#ifdef CONFIG_USB_ANDROID_ACCESSORY_SUPPORT
#include "f_accessory.c"
#endif
#include "f_diag.c"
#define USB_ETH_RNDIS y
#include "f_rndis.c"
#include "rndis.c"
#include "u_ether.c"

#define USB_MODE_VERSION	"1.1"

MODULE_AUTHOR("Yongsul Oh <yongsul96.oh@samsung.com>");
MODULE_DESCRIPTION("SLP Composite USB Driver similar to Android Compiste");
MODULE_LICENSE("GPL");
MODULE_VERSION(USB_MODE_VERSION);

static const char slp_longname[] = "Gadget SLP";
static const char *const ustate_string[] = {
	[USB_STATE_NOTATTACHED] = "NOTATTACHED",
	[USB_STATE_ATTACHED] = "ATTACHED",
	[USB_STATE_POWERED] = "EOPNOTSUPP",		/* not support */
	[USB_STATE_RECONNECTING] = "EOPNOTSUPP",	/* not support */
	[USB_STATE_UNAUTHENTICATED] = "EOPNOTSUPP",	/* not support */
	[USB_STATE_DEFAULT] = "EOPNOTSUPP",		/* not support */
	[USB_STATE_ADDRESS] = "EOPNOTSUPP",		/* not support */
	[USB_STATE_CONFIGURED] = "CONFIGURED",
	[USB_STATE_SUSPENDED] = "EOPNOTSUPP"	/* not support */
};

/* Default vendor and product IDs, overridden by userspace */
#define VENDOR_ID		0x04E8	/* Samsung VID */
#define PRODUCT_ID		0x6860	/* KIES mode PID */

/* DM_PORT NUM : /dev/ttyGS* port number */
#define DM_PORT_NUM            1

struct slp_multi_usb_function {
	char *name;
	void *config;

	struct device *dev;
	char *dev_name;
	struct device_attribute **attributes;

	/* for slp_multi_dev.funcs_fconf */
	struct list_head fconf_list;

	/* for slp_multi_dev.funcs_sconf */
	struct list_head sconf_list;

	/* for slp_multi_dev.available_functions */
	struct list_head available_list;

	/* Manndatory: initialization during gadget bind */
	int (*init) (struct slp_multi_usb_function *,
					struct usb_composite_dev *);
	/* Optional: cleanup during gadget unbind */
	void (*cleanup) (struct slp_multi_usb_function *);
	/* Mandatory: called when the usb enabled */
	int (*bind_config) (struct slp_multi_usb_function *,
			    struct usb_configuration *);
	/* Optional: called when the configuration is removed */
	void (*unbind_config) (struct slp_multi_usb_function *,
			       struct usb_configuration *);
	/* Optional: handle ctrl requests before the device is configured */
	int (*ctrlrequest) (struct slp_multi_usb_function *,
			    struct usb_composite_dev *,
			    const struct usb_ctrlrequest *);

	/* to check used or not */
	char used;
};

struct slp_multi_dev {
	struct list_head available_functions;

	/* for each configuration control */
	struct list_head funcs_fconf;
	struct list_head funcs_sconf;

	struct usb_composite_dev *cdev;
	struct device dev;

	bool enabled;
	bool dual_config;
	bool attached;

	/* current USB state */
	enum usb_device_state ustate;

	/* to control DMA QOS */
	char pm_qos[5];
	s32 swfi_latency;
	s32 curr_latency;
	struct pm_qos_request pm_qos_req_dma;

	struct work_struct evt_work;

	struct list_head list;

	/* to check it's id */
	int id;

	/* asserted events */
	struct list_head evt_list;
	struct mutex evt_mutex;
	struct mutex enable_lock;
	spinlock_t evt_lock;
};

static unsigned slp_multi_nluns;
static struct class *slp_multi_class;
static int slp_multi_bind_config(struct usb_configuration *c);
static void slp_multi_unbind_config(struct usb_configuration *c);

static DEFINE_IDA(slp_multi_ida);
static LIST_HEAD(smdev_list);

/* string IDs are assigned dynamically */
#define STRING_MANUFACTURER_IDX		0
#define STRING_PRODUCT_IDX		1
#define STRING_SERIAL_IDX		2

static char manufacturer_string[256];
static char product_string[256];
static char serial_string[256];

/* String Table */
static struct usb_string strings_dev[] = {
	[STRING_MANUFACTURER_IDX].s = manufacturer_string,
	[STRING_PRODUCT_IDX].s = product_string,
	[STRING_SERIAL_IDX].s = serial_string,
	{}			/* end of list */
};

static struct usb_gadget_strings stringtab_dev = {
	.language = 0x0409,	/* en-us */
	.strings = strings_dev,
};

static struct usb_gadget_strings *slp_dev_strings[] = {
	&stringtab_dev,
	NULL,
};

static struct usb_device_descriptor device_desc = {
	.bLength = sizeof(device_desc),
	.bDescriptorType = USB_DT_DEVICE,
	.bcdUSB = __constant_cpu_to_le16(0x0200),
	.idVendor = __constant_cpu_to_le16(VENDOR_ID),
	.idProduct = __constant_cpu_to_le16(PRODUCT_ID),
	.bcdDevice = __constant_cpu_to_le16(0xffff),
};

static struct usb_configuration first_config_driver = {
	.label = "slp_first_config",
	.unbind = slp_multi_unbind_config,
	.bConfigurationValue = USB_CONFIGURATION_1,
	.bmAttributes = USB_CONFIG_ATT_ONE | USB_CONFIG_ATT_SELFPOWER,
	.bMaxPower = 0x30,	/* 96ma */
};

static struct usb_configuration second_config_driver = {
	.label = "slp_second_config",
	.unbind = slp_multi_unbind_config,
	.bConfigurationValue = USB_CONFIGURATION_2,
	.bmAttributes = USB_CONFIG_ATT_ONE | USB_CONFIG_ATT_SELFPOWER,
	.bMaxPower = 0x30,	/* 96ma */
};

/*-------------------------------------------------------------------------*/
/* Supported functions initialization */

static int sdb_function_init(struct slp_multi_usb_function *f,
			     struct usb_composite_dev *cdev)
{
	return sdb_setup(cdev);
}

static void sdb_function_cleanup(struct slp_multi_usb_function *f)
{
	sdb_cleanup();
}

static int sdb_function_bind_config(struct slp_multi_usb_function *f,
				    struct usb_configuration *c)
{
	return sdb_bind_config(c);
}

static struct slp_multi_usb_function sdb_function = {
	.name = "sdb",
	.init = sdb_function_init,
	.cleanup = sdb_function_cleanup,
	.bind_config = sdb_function_bind_config,
};

#define MAX_ACM_INSTANCES 4
struct acm_function_config {
	int instances;
};

static int acm_function_init(struct slp_multi_usb_function *f,
			     struct usb_composite_dev *cdev)
{
	struct acm_function_config *config;
	int status;

	config = kzalloc(sizeof(struct acm_function_config), GFP_KERNEL);
	if (!config)
		return -ENOMEM;

	/* default setting */
	config->instances = 1;

#ifdef CONFIG_USB_DUN_SUPPORT
	status = modem_misc_register();
	if (status) {
		kfree(config);
		return status;
	}
#endif

	/* maybe allocate device-global string IDs, and patch descriptors */
	if (acm_string_defs[ACM_CTRL_IDX].id == 0) {
		status = usb_string_id(cdev);
		if (status < 0)
			goto acm_init_error;
		acm_string_defs[ACM_CTRL_IDX].id = status;

		acm_control_interface_desc.iInterface = status;

		status = usb_string_id(cdev);
		if (status < 0)
			goto acm_init_error;
		acm_string_defs[ACM_DATA_IDX].id = status;

		acm_data_interface_desc.iInterface = status;

		status = usb_string_id(cdev);
		if (status < 0)
			goto acm_init_error;
		acm_string_defs[ACM_IAD_IDX].id = status;

		acm_iad_descriptor.iFunction = status;
	}

	status = gserial_setup(cdev->gadget, config->instances);
	if (status)
		goto acm_init_error;

	f->config = config;
	return 0;

 acm_init_error:
	kfree(config);
	return status;
}

static void acm_function_cleanup(struct slp_multi_usb_function *f)
{
	gserial_cleanup();
	kfree(f->config);
	f->config = NULL;
#ifdef CONFIG_USB_DUN_SUPPORT
	modem_misc_unregister();
#endif
}

static int acm_function_bind_config(struct slp_multi_usb_function *f,
				    struct usb_configuration *c)
{
	int i;
	int ret = 0;
	struct acm_function_config *config = f->config;

	for (i = 0; i < config->instances; i++) {
		ret = acm_bind_config(c, i);
		if (ret) {
			dev_err(f->dev, "Could not bind acm%u config\n", i);
			break;
		}
	}

	return ret;
}

static ssize_t acm_instances_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct slp_multi_usb_function *f = dev_get_drvdata(dev);
	struct acm_function_config *config = f->config;
	return snprintf(buf, PAGE_SIZE, "%d\n", config->instances);
}

static ssize_t acm_instances_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	struct slp_multi_usb_function *f = dev_get_drvdata(dev);
	struct acm_function_config *config = f->config;
	int value;

	sscanf(buf, "%d", &value);
	if (value > MAX_ACM_INSTANCES)
		value = MAX_ACM_INSTANCES;
	config->instances = value;
	return size;
}

static DEVICE_ATTR(instances, S_IRUGO | S_IWUSR,
		   acm_instances_show, acm_instances_store);
static struct device_attribute *acm_function_attributes[] = {
				&dev_attr_instances, NULL };

static struct slp_multi_usb_function acm_function = {
	.name = "acm",
	.init = acm_function_init,
	.cleanup = acm_function_cleanup,
	.bind_config = acm_function_bind_config,
	.attributes = acm_function_attributes,
};

static int mtp_function_init(struct slp_multi_usb_function *f,
			     struct usb_composite_dev *cdev)
{
	return mtp_setup(cdev);
}

static void mtp_function_cleanup(struct slp_multi_usb_function *f)
{
	mtp_cleanup();
}

static int mtp_function_bind_config(struct slp_multi_usb_function *f,
				    struct usb_configuration *c)
{
	return mtp_bind_config(c);
}

#define IS_GET_MS_DESC(_ctrl, _w_index)	((_ctrl->bRequest == 0x01 \
			|| _ctrl->bRequest == 0x54 || _ctrl->bRequest == 0x6F \
			|| _ctrl->bRequest == 0xFE)	\
		&& (_ctrl->bRequestType & USB_DIR_IN)	\
		&& (_w_index == 4 || _w_index == 5))

static int mtp_function_ctrlrequest(struct slp_multi_usb_function *f,
				    struct usb_composite_dev *cdev,
				    const struct usb_ctrlrequest *ctrl)
{
	struct usb_request *req = cdev->req;
	struct usb_gadget *gadget = cdev->gadget;
	int value = -EOPNOTSUPP;
	u16 w_length = le16_to_cpu(ctrl->wLength);
	u16	w_index = le16_to_cpu(ctrl->wIndex);
	struct usb_string_descriptor *os_func_desc = req->buf;
	char ms_descriptor[38] = {
		/* Header section */
		/* Upper 2byte of dwLength */
		0x00, 0x00,
		/* bcd Version */
		0x00, 0x01,
		/* wIndex, Extended compatID index */
		0x04, 0x00,
		/* bCount, we use only 1 function(MTP) */
		0x01,
		/* RESERVED */
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

		/* First Function section for MTP */
		/* bFirstInterfaceNumber,
		 * we always use it by 0 for MTP
		 */
		0x00,
		/* RESERVED, fixed value 1 */
		0x01,
		/* CompatibleID for MTP */
		0x4D, 0x54, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00,
		/* Sub-compatibleID for MTP */
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		/* RESERVED */
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};

	/* Added handler to respond to host about MS OS Descriptors.
	 * Below handler is requirement if you use MTP.
	 * So, If you set composite included MTP,
	 * you have to respond to host about 0x01, 0x54, 0x64, 0xFE
	 * request refer to following site.
	 * http://msdn.microsoft.com/en-us/windows/hardware/gg463179
	 */

	if (IS_GET_MS_DESC(ctrl, w_index)) {
		os_func_desc->bLength = 0x28;
		os_func_desc->bDescriptorType = 0x00;
		value = min(w_length, (u16) (sizeof(ms_descriptor) + 2));
		memcpy(os_func_desc->wData, &ms_descriptor, value);

		if (value >= 0) {
			req->length = value;
			req->zero = value < w_length;
			value = usb_ep_queue(gadget->ep0, req, GFP_ATOMIC);
			if (value < 0) {
				req->status = 0;
				composite_setup_complete(gadget->ep0, req);
			}
		}
	}

	return value;
}

static struct slp_multi_usb_function mtp_function = {
	.name = "mtp",
	.init = mtp_function_init,
	.cleanup = mtp_function_cleanup,
	.bind_config = mtp_function_bind_config,
	.ctrlrequest = mtp_function_ctrlrequest,
};

struct rndis_function_config {
	u8 ethaddr[ETH_ALEN];
	u32 vendorID;
	char manufacturer[256];
	bool wceis;
	u8 rndis_string_defs0_id;
};
static char host_addr_string[18];

static int rndis_function_init(struct slp_multi_usb_function *f,
			       struct usb_composite_dev *cdev)
{
	struct rndis_function_config *config;
	int status, i;

	config = kzalloc(sizeof(struct rndis_function_config), GFP_KERNEL);
	if (!config)
		return -ENOMEM;

	/* maybe allocate device-global string IDs */
	if (rndis_string_defs[0].id == 0) {

		/* control interface label */
		status = usb_string_id(cdev);
		if (status < 0)
			goto rndis_init_error;
		config->rndis_string_defs0_id = status;
		rndis_string_defs[0].id = status;
		rndis_control_intf.iInterface = status;

		/* data interface label */
		status = usb_string_id(cdev);
		if (status < 0)
			goto rndis_init_error;
		rndis_string_defs[1].id = status;
		rndis_data_intf.iInterface = status;

		/* IAD iFunction label */
		status = usb_string_id(cdev);
		if (status < 0)
			goto rndis_init_error;
		rndis_string_defs[2].id = status;
		rndis_iad_descriptor.iFunction = status;
	}

	/* create a fake MAC address from our serial number. */
	for (i = 0; (i < 256) && serial_string[i]; i++) {
		/* XOR the USB serial across the remaining bytes */
		config->ethaddr[i % (ETH_ALEN - 1) + 1] ^= serial_string[i];
	}
	config->ethaddr[0] &= 0xfe;	/* clear multicast bit */
	config->ethaddr[0] |= 0x02;	/* set local assignment bit (IEEE802) */

	snprintf(host_addr_string, sizeof(host_addr_string),
		"%02x:%02x:%02x:%02x:%02x:%02x",
		config->ethaddr[0],	config->ethaddr[1],
		config->ethaddr[2],	config->ethaddr[3],
		config->ethaddr[4], config->ethaddr[5]);

	/* The "host_addr" is a module_param variable of u_ether.c
	 *  for "host ethernet address". So we use it directly.
	 */
	host_addr = host_addr_string;

	f->config = config;
	return 0;

 rndis_init_error:
	kfree(config);
	return status;
}

static void rndis_function_cleanup(struct slp_multi_usb_function *f)
{
	kfree(f->config);
	f->config = NULL;
}

static int rndis_function_bind_config(struct slp_multi_usb_function *f,
				      struct usb_configuration *c)
{
	int ret = -EINVAL;
	struct rndis_function_config *rndis = f->config;

	if (!rndis) {
		dev_err(f->dev, "error rndis_pdata is null\n");
		return ret;
	}

	ret = gether_setup(c->cdev->gadget, rndis->ethaddr);
	if (ret) {
		dev_err(f->dev, "gether_setup failed(ret:%d)\n", ret);
		return ret;
	}

	if (rndis->wceis) {
		/* "Wireless" RNDIS; auto-detected by Windows */
		rndis_iad_descriptor.bFunctionClass =
		    USB_CLASS_WIRELESS_CONTROLLER;
		rndis_iad_descriptor.bFunctionSubClass = 0x01;
		rndis_iad_descriptor.bFunctionProtocol = 0x03;
		rndis_control_intf.bInterfaceClass =
		    USB_CLASS_WIRELESS_CONTROLLER;
		rndis_control_intf.bInterfaceSubClass = 0x01;
		rndis_control_intf.bInterfaceProtocol = 0x03;
	}

	/* ... and setup RNDIS itself */
	ret = rndis_init();
	if (ret < 0) {
		dev_err(f->dev, "rndis_init failed(ret:%d)\n", ret);
		gether_cleanup();
		return ret;
	}

	/* Android team reset "rndis_string_defs[0].id" when RNDIS unbinded
	 * in f_rndis.c but, that makes failure of rndis_bind_config() by
	 * the overflow of "next_string_id" value in usb_string_id().
	 * So, Android team also reset "next_string_id" value in android.c
	 * but SLP does not reset "next_string_id" value. And we decided to
	 * re-update "rndis_string_defs[0].id" by old value.
	 * 20120224 yongsul96.oh@samsung.com
	 */
	if (rndis_string_defs[0].id == 0)
		rndis_string_defs[0].id = rndis->rndis_string_defs0_id;

	ret = rndis_bind_config_vendor(c, rndis->ethaddr, rndis->vendorID,
				 rndis->manufacturer);
	if (ret) {
		rndis_exit();
		gether_cleanup();
		dev_err(f->dev, "rndis_bind_config failed(ret:%d)\n", ret);
	}

	return ret;
}

static void rndis_function_unbind_config(struct slp_multi_usb_function *f,
					 struct usb_configuration *c)
{
	gether_cleanup();
}

static ssize_t rndis_manufacturer_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct slp_multi_usb_function *f = dev_get_drvdata(dev);
	struct rndis_function_config *config = f->config;
	return snprintf(buf, PAGE_SIZE, "%s\n", config->manufacturer);
}

static ssize_t rndis_manufacturer_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	struct slp_multi_usb_function *f = dev_get_drvdata(dev);
	struct rndis_function_config *config = f->config;

	if ((size >= sizeof(config->manufacturer)) ||
		(sscanf(buf, "%s", config->manufacturer) != 1))
		return -EINVAL;

	return size;
}

static DEVICE_ATTR(manufacturer, S_IRUGO | S_IWUSR, rndis_manufacturer_show,
		   rndis_manufacturer_store);

static ssize_t rndis_wceis_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct slp_multi_usb_function *f = dev_get_drvdata(dev);
	struct rndis_function_config *config = f->config;
	return snprintf(buf, PAGE_SIZE, "%d\n", config->wceis);
}

static ssize_t rndis_wceis_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t size)
{
	struct slp_multi_usb_function *f = dev_get_drvdata(dev);
	struct rndis_function_config *config = f->config;
	int value;

	if (sscanf(buf, "%d", &value) == 1) {
		config->wceis = value;
		return size;
	}
	return -EINVAL;
}

static DEVICE_ATTR(wceis, S_IRUGO | S_IWUSR, rndis_wceis_show,
		   rndis_wceis_store);

static ssize_t rndis_ethaddr_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct slp_multi_usb_function *f = dev_get_drvdata(dev);
	struct rndis_function_config *rndis = f->config;
	return snprintf(buf, PAGE_SIZE, "%02x:%02x:%02x:%02x:%02x:%02x\n",
		       rndis->ethaddr[0], rndis->ethaddr[1], rndis->ethaddr[2],
		       rndis->ethaddr[3], rndis->ethaddr[4], rndis->ethaddr[5]);
}

static DEVICE_ATTR(ethaddr, S_IRUGO | S_IWUSR, rndis_ethaddr_show,
		   NULL);

static ssize_t rndis_vendorID_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct slp_multi_usb_function *f = dev_get_drvdata(dev);
	struct rndis_function_config *config = f->config;
	return snprintf(buf, PAGE_SIZE, "%04x\n", config->vendorID);
}

static ssize_t rndis_vendorID_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t size)
{
	struct slp_multi_usb_function *f = dev_get_drvdata(dev);
	struct rndis_function_config *config = f->config;
	int value;

	if (sscanf(buf, "%04x", &value) == 1) {
		config->vendorID = value;
		return size;
	}
	return -EINVAL;
}

static DEVICE_ATTR(vendorID, S_IRUGO | S_IWUSR, rndis_vendorID_show,
		   rndis_vendorID_store);

static struct device_attribute *rndis_function_attributes[] = {
	&dev_attr_manufacturer,
	&dev_attr_wceis,
	&dev_attr_ethaddr,
	&dev_attr_vendorID,
	NULL
};

static struct slp_multi_usb_function rndis_function = {
	.name = "rndis",
	.init = rndis_function_init,
	.cleanup = rndis_function_cleanup,
	.bind_config = rndis_function_bind_config,
	.unbind_config = rndis_function_unbind_config,
	.attributes = rndis_function_attributes,
};

struct mass_storage_function_config {
	struct fsg_config fsg;
	struct fsg_common *common;
};

static int mass_storage_function_init(struct slp_multi_usb_function *f,
				      struct usb_composite_dev *cdev)
{
	struct mass_storage_function_config *config;
	struct fsg_common *common;
	int err, i;

	config = kzalloc(sizeof(struct mass_storage_function_config),
			 GFP_KERNEL);
	if (!config)
		return -ENOMEM;

	if (slp_multi_nluns != 0) {
		/* Some device use sd card or not.
		 * If you want to modify nluns,
		 * please change nluns of standard slp_multi platform data
		 * Please do not modify nluns directly in this function.
		 * Every model uses same slp.c file.
		 */
		dev_dbg(f->dev, "usb: %s nluns=%d\n", __func__,
		       slp_multi_nluns);
		config->fsg.nluns = slp_multi_nluns;
		for (i = 0; i < slp_multi_nluns; i++)
			config->fsg.luns[i].removable = 1;

		common = fsg_common_init(NULL, cdev, &config->fsg);
		if (IS_ERR(common)) {
			kfree(config);
			return PTR_ERR(common);
		}

		for (i = 0; i < slp_multi_nluns; i++) {
			char luns[5];
			err = snprintf(luns, 5, "lun%d", i);
			if (err == 0) {
				dev_err(f->dev, "usb: %s snprintf error\n",
				       __func__);
				kfree(config);
				return err;
			}
			err = sysfs_create_link(&f->dev->kobj,
						&common->luns[i].dev.kobj,
						luns);
			if (err) {
				kfree(config);
				return err;
			}
		}
	} else {
		/* original mainline code */
		dev_dbg(f->dev, "usb: %s pdata is not available. nluns=1\n",
		       __func__);
		config->fsg.nluns = 1;
		config->fsg.luns[0].removable = 1;

		common = fsg_common_init(NULL, cdev, &config->fsg);
		if (IS_ERR(common)) {
			kfree(config);
			return PTR_ERR(common);
		}

		err = sysfs_create_link(&f->dev->kobj,
					&common->luns[0].dev.kobj, "lun");
		if (err) {
			kfree(config);
			return err;
		}
	}

	config->common = common;
	f->config = config;
	return 0;
}

static void mass_storage_function_cleanup(struct slp_multi_usb_function *f)
{
	kfree(f->config);
	f->config = NULL;
}

static int mass_storage_function_bind_config(struct slp_multi_usb_function *f,
					     struct usb_configuration *c)
{
	struct mass_storage_function_config *config = f->config;
	return fsg_bind_config(c->cdev, c, config->common);
}

static ssize_t mass_storage_inquiry_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct slp_multi_usb_function *f = dev_get_drvdata(dev);
	struct mass_storage_function_config *config = f->config;
	return snprintf(buf, PAGE_SIZE, "%s\n", config->common->inquiry_string);
}

static ssize_t mass_storage_inquiry_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t size)
{
	struct slp_multi_usb_function *f = dev_get_drvdata(dev);
	struct mass_storage_function_config *config = f->config;
	if (size >= sizeof(config->common->inquiry_string))
		return -EINVAL;
	if (sscanf(buf, "%s", config->common->inquiry_string) != 1)
		return -EINVAL;
	return size;
}

static DEVICE_ATTR(inquiry_string, S_IRUGO | S_IWUSR,
		   mass_storage_inquiry_show, mass_storage_inquiry_store);

static struct device_attribute *mass_storage_function_attributes[] = {
	&dev_attr_inquiry_string,
	NULL
};

static struct slp_multi_usb_function mass_storage_function = {
	.name = "mass_storage",
	.init = mass_storage_function_init,
	.cleanup = mass_storage_function_cleanup,
	.bind_config = mass_storage_function_bind_config,
	.attributes = mass_storage_function_attributes,
};

#ifdef CONFIG_USB_ANDROID_ACCESSORY_SUPPORT
static int accessory_function_init(struct slp_multi_usb_function *f,
					struct usb_composite_dev *cdev)
{
	int ret;

	/* pre-allocate a string ID for f-accessory interface */
	if (acc_string_defs[INTERFACE_STRING_INDEX].id == 0) {
		ret = usb_string_id(cdev);
		if (ret < 0)
			return ret;
		acc_string_defs[INTERFACE_STRING_INDEX].id = ret;
		acc_interface_desc.iInterface = ret;
	}

	return acc_setup();
}

static void accessory_function_cleanup(struct slp_multi_usb_function *f)
{
	acc_cleanup();
}

static int accessory_function_bind_config(struct slp_multi_usb_function *f,
						struct usb_configuration *c)
{
	return acc_bind_config(c);
}

static int accessory_function_ctrlrequest(struct slp_multi_usb_function *f,
						struct usb_composite_dev *cdev,
						const struct usb_ctrlrequest *c)
{
	return acc_ctrlrequest(cdev, c);
}

static struct slp_multi_usb_function accessory_function = {
	.name		= "accessory",
	.init		= accessory_function_init,
	.cleanup	= accessory_function_cleanup,
	.bind_config	= accessory_function_bind_config,
	.ctrlrequest	= accessory_function_ctrlrequest,
};
#endif

/* DIAG : enabled DIAG clients- "diag[,diag_mdm]" */
static char diag_clients[32];
static ssize_t clients_store(
		struct device *device, struct device_attribute *attr,
		const char *buff, size_t size)
{
	strlcpy(diag_clients, buff, sizeof(diag_clients));

	return size < (sizeof(diag_clients)-1) ?
				size : sizeof(diag_clients)-1;
}

static DEVICE_ATTR(clients, S_IWUSR, NULL, clients_store);
static struct device_attribute *diag_function_attributes[] = {
				&dev_attr_clients, NULL };

static int diag_function_init(struct slp_multi_usb_function *f,
				 struct usb_composite_dev *cdev)
{
	return diag_setup();
}

static void diag_function_cleanup(struct slp_multi_usb_function *f)
{
	diag_cleanup();
}

static int diag_function_bind_config(struct slp_multi_usb_function *f,
					struct usb_configuration *c)
{
	char *name;
	char buf[32], *b;
	int err = -1;
	int (*notify)(uint32_t, const char *) = NULL;

	strlcpy(buf, diag_clients, sizeof(buf));
	b = strim(buf);
	while (b) {
		notify = NULL;
		name = strsep(&b, ",");

		if (name) {
			err = diag_function_add(c, name, notify);
			if (err)
				dev_err(f->dev, "usb: diag: Cannot open channel %s\n",
						 name);
		}
	}
	return err;
}

static struct slp_multi_usb_function diag_function = {
	.name		= "diag",
	.init		= diag_function_init,
	.cleanup	= diag_function_cleanup,
	.bind_config	= diag_function_bind_config,
	.attributes	= diag_function_attributes,
};

#ifdef CONFIG_ARCH_MSM
static int qdss_function_init(struct slp_multi_usb_function *f,
	struct usb_composite_dev *cdev)
{
	return qdss_setup();
}

static void qdss_function_cleanup(struct slp_multi_usb_function *f)
{
	qdss_cleanup();
}

static int qdss_function_bind_config(struct slp_multi_usb_function *f,
					struct usb_configuration *c)
{
	int  err = -1;

	err = qdss_bind_config(c, "qdss");
	if (err)
		dev_err(f->dev, "qdss: Cannot open channel qdss\n");

	return err;
}

static struct slp_multi_usb_function qdss_function = {
	.name		= "qdss",
	.init		= qdss_function_init,
	.cleanup	= qdss_function_cleanup,
	.bind_config	= qdss_function_bind_config,
};

/*rmnet transport string format(per port):"ctrl0,data0,ctrl1,data1..." */
#define MAX_XPORT_STR_LEN 50
static char rmnet_transports[MAX_XPORT_STR_LEN];

static void rmnet_function_cleanup(struct slp_multi_usb_function *f)
{
	frmnet_cleanup();
}

static int rmnet_function_bind_config(struct slp_multi_usb_function *f,
					 struct usb_configuration *c)
{
	int i;
	int err = 0;
	char *ctrl_name;
	char *data_name;
	char buf[MAX_XPORT_STR_LEN], *b;
	static int rmnet_initialized, ports;

	if (!rmnet_initialized) {
		rmnet_initialized = 1;
		strlcpy(buf, rmnet_transports, sizeof(buf));
		b = strim(buf);
		while (b) {
			ctrl_name = strsep(&b, ",");
			data_name = strsep(&b, ",");
			if (ctrl_name && data_name) {
				err = frmnet_init_port(ctrl_name, data_name);
				if (err) {
					pr_err("rmnet: Cannot open ctrl port:'%s' data port:'%s'\n",
						ctrl_name, data_name);
					goto out;
				}
				ports++;
			}
		}

		err = rmnet_gport_setup();
		if (err) {
			pr_err("rmnet: Cannot setup transports");
			goto out;
		}
	}

	for (i = 0; i < ports; i++) {
		err = frmnet_bind_config(c, i);
		if (err) {
			pr_err("Could not bind rmnet%u config\n", i);
			break;
		}
	}
out:
	return err;
}

static ssize_t rmnet_transports_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", rmnet_transports);
}

static ssize_t rmnet_transports_store(
		struct device *device, struct device_attribute *attr,
		const char *buff, size_t size)
{
	strlcpy(rmnet_transports, buff, sizeof(rmnet_transports));

	return size;
}

static struct device_attribute dev_attr_rmnet_transports =
					__ATTR(transports, S_IRUGO | S_IWUSR,
						rmnet_transports_show,
						rmnet_transports_store);
static struct device_attribute *rmnet_function_attributes[] = {
					&dev_attr_rmnet_transports,
					NULL };

static struct slp_multi_usb_function rmnet_function = {
	.name		= "rmnet",
	.cleanup	= rmnet_function_cleanup,
	.bind_config	= rmnet_function_bind_config,
	.attributes	= rmnet_function_attributes,
};
#endif

static struct slp_multi_usb_function *supported_functions[] = {
	&sdb_function,
	&acm_function,
	&mtp_function,
	&rndis_function,
	&mass_storage_function,
#ifdef CONFIG_USB_ANDROID_ACCESSORY_SUPPORT
	&accessory_function,
#endif
	&diag_function,
#ifdef CONFIG_ARCH_MSM
	&qdss_function,
	&rmnet_function,
#endif
	NULL
};

static inline
struct slp_multi_dev *cdev_to_smdev(struct usb_composite_dev *cdev)
{
	struct slp_multi_dev *smdev;

	if (list_empty(&smdev_list))
		return NULL;

	list_for_each_entry(smdev, &smdev_list, list)
		if (smdev->cdev == cdev)
			return smdev;

	return NULL;
}

static void slp_multi_evt_emit(struct slp_multi_dev *smdev,
				struct slp_multi_evt *evt)
{
	int err;

	switch (evt->evt_type) {
	case SMDEV_EVT_QOS_CHANGE:
		if (smdev->curr_latency != evt->qos) {
			smdev->curr_latency = evt->qos;
			pm_qos_update_request(&smdev->pm_qos_req_dma,
					evt->qos);
			dev_info(&smdev->dev, "PM QOS value changed to %s\n",
				(evt->qos != PM_QOS_DEFAULT_VALUE) ?
					"high" : "low");
		}
		break;

	case SMDEV_EVT_STATE_CHANGE:
		if (smdev->ustate != evt->ustate) {
			dev_info(&smdev->dev, "usb_state changed from %d to %d\n",
				smdev->ustate, evt->ustate);
			smdev->ustate = evt->ustate;
			err = kobject_uevent(&smdev->dev.kobj, KOBJ_CHANGE);
			if (err < 0)
				dev_err(&smdev->dev, "can't send usb_state[%d], err(%d)\n",
					evt->ustate, err);
			else
				dev_info(&smdev->dev, "sended uevent usb_state [%s]\n",
					ustate_string[smdev->ustate]);
		}
		break;

	default:
		dev_err(&smdev->dev, "not supported type(%d)\n", evt->evt_type);
		break;
	}
}

static void slp_multi_evt_thread(struct work_struct *data)
{
	struct slp_multi_dev *smdev =
		container_of(data, struct slp_multi_dev, evt_work);
	LIST_HEAD(evt_list);

	mutex_lock(&smdev->evt_mutex);
	if (!smdev->enabled)
		goto evt_thread_done;

	while (1) {
		struct slp_multi_evt *evt;
		struct list_head *this, *tmp;
		unsigned long flags;

		spin_lock_irqsave(&smdev->evt_lock, flags);
		list_splice_init(&smdev->evt_list, &evt_list);
		spin_unlock_irqrestore(&smdev->evt_lock, flags);

		if (list_empty(&evt_list))
			break;

		list_for_each_safe(this, tmp, &evt_list) {
			evt = list_entry(this, struct slp_multi_evt, node);
			list_del(&evt->node);
			slp_multi_evt_emit(smdev, evt);
			kfree(evt);
		}
	}

evt_thread_done:
	mutex_unlock(&smdev->evt_mutex);
}

static void slp_multi_evt_send(struct slp_multi_dev *smdev,
				struct slp_multi_evt *evt)
{
	unsigned long flags;

	spin_lock_irqsave(&smdev->evt_lock, flags);
	list_add_tail(&evt->node, &smdev->evt_list);
	schedule_work(&smdev->evt_work);
	spin_unlock_irqrestore(&smdev->evt_lock, flags);
}

static void slp_multi_qos_evt(struct slp_multi_dev *smdev,
				s32 new_value)
{
	struct slp_multi_evt *evt;

	evt = kzalloc(sizeof(struct slp_multi_evt), GFP_KERNEL);
	if (!evt) {
		dev_err(&smdev->dev, "can't queue qos(%d) by ENOMEM\n",
			new_value);
		return;
	}

	evt->evt_type = SMDEV_EVT_QOS_CHANGE;
	INIT_LIST_HEAD(&evt->node);
	evt->qos = new_value;

	slp_multi_evt_send(smdev, evt);
}

static void slp_multi_state_evt(struct slp_multi_dev *smdev,
				enum usb_device_state new_state)
{
	struct slp_multi_evt *evt;

	evt = kzalloc(sizeof(struct slp_multi_evt), GFP_KERNEL);
	if (!evt) {
		dev_err(&smdev->dev, "can't queue [%d] evt by ENOMEM\n",
			new_state);
		return;
	}

	evt->evt_type = SMDEV_EVT_STATE_CHANGE;
	INIT_LIST_HEAD(&evt->node);
	evt->ustate = new_state;

	slp_multi_evt_send(smdev, evt);
}

static int slp_multi_init_functions(struct slp_multi_dev *smdev,
				  struct usb_composite_dev *cdev)
{
	struct slp_multi_usb_function *f;
	struct device_attribute **attrs;
	struct device_attribute *attr;
	int err = 0;
	int index = 0;

	list_for_each_entry(f, &smdev->available_functions, available_list) {
		f->dev_name = kasprintf(GFP_KERNEL, "f_%s", f->name);
		f->dev = device_create(slp_multi_class, &smdev->dev,
				       MKDEV(0, index++), f, f->dev_name);
		if (IS_ERR(f->dev)) {
			dev_err(&smdev->dev,
				"Failed to create dev %s", f->dev_name);
			err = PTR_ERR(f->dev);
			goto init_func_err_create;
		}

		if (f->init) {
			err = f->init(f, cdev);
			if (err) {
				dev_err(&smdev->dev,
					"Failed to init %s", f->name);
				goto init_func_err_out;
			}
		}

		attrs = f->attributes;
		if (attrs) {
			while ((attr = *attrs++) && !err)
				err = device_create_file(f->dev, attr);
		}
		if (err) {
			dev_err(f->dev, "Failed to create function %s attributes",
			       f->name);
			goto init_func_err_out;
		}
	}
	return 0;

 init_func_err_out:
	device_destroy(slp_multi_class, f->dev->devt);
 init_func_err_create:
	kfree(f->dev_name);
	return err;
}

static void slp_multi_cleanup_functions(struct slp_multi_dev *smdev)
{
	struct slp_multi_usb_function *f;

	list_for_each_entry(f, &smdev->available_functions, available_list) {
		if (f->dev) {
			device_destroy(slp_multi_class, f->dev->devt);
			kfree(f->dev_name);
		}

		if (f->cleanup)
			f->cleanup(f);
	}
}

static int
slp_multi_bind_enabled_functions(struct slp_multi_dev *smdev,
			       struct usb_configuration *c)
{
	struct slp_multi_usb_function *f;
	int ret;

	if (c->bConfigurationValue == USB_CONFIGURATION_1) {
		list_for_each_entry(f, &smdev->funcs_fconf, fconf_list) {
			dev_dbg(&smdev->dev, "usb_bind_conf(1st) f:%s\n",
				f->name);
			ret = f->bind_config(f, c);
			if (ret) {
				dev_err(&smdev->dev, "%s bind_conf(1st) failed\n",
					f->name);
				return ret;
			}
		}
	} else if (c->bConfigurationValue == USB_CONFIGURATION_2) {
		list_for_each_entry(f, &smdev->funcs_sconf, sconf_list) {
			dev_dbg(&smdev->dev, "usb_bind_conf(2nd) f:%s\n",
				f->name);
			ret = f->bind_config(f, c);
			if (ret) {
				dev_err(&smdev->dev, "%s bind_conf(2nd) failed\n",
					f->name);
				return ret;
			}
		}
	} else {
		dev_err(&smdev->dev, "Not supported configuraton(%d)\n",
			c->bConfigurationValue);
		return -EINVAL;
	}
	return 0;
}

static void
slp_multi_unbind_enabled_functions(struct slp_multi_dev *smdev,
				 struct usb_configuration *c)
{
	struct slp_multi_usb_function *f;

	if (c->bConfigurationValue == USB_CONFIGURATION_1) {
		list_for_each_entry(f, &smdev->funcs_fconf, fconf_list) {
			if (f->unbind_config)
				f->unbind_config(f, c);
		}
	} else if (c->bConfigurationValue == USB_CONFIGURATION_2) {
		list_for_each_entry(f, &smdev->funcs_sconf, sconf_list) {
			if (f->unbind_config)
				f->unbind_config(f, c);
		}
	}
}

#define ADD_FUNCS_LIST(head, member)	\
static inline int add_##member(struct slp_multi_dev *smdev, char *name)	\
{	\
	struct slp_multi_usb_function *av_f, *en_f;	\
	\
	dev_dbg(&smdev->dev, "usb: name=%s\n", name);	\
	list_for_each_entry(av_f, &smdev->available_functions,	\
			available_list) {	\
		if (!strcmp(name, av_f->name)) {	\
			list_for_each_entry(en_f, &smdev->head,	\
					member) {	\
				if (av_f == en_f) {	\
					dev_info(&smdev->dev, \
						"usb:%s already enabled!\n", \
						name);	\
					return 0;	\
				}	\
			}	\
			list_add_tail(&av_f->member, &smdev->head);	\
			return 0;	\
		}	\
	}	\
	return -EINVAL;	\
}	\
static ssize_t	show_##head(struct device *pdev,	\
			struct device_attribute *attr, char *buf)	\
{	\
	struct slp_multi_dev *smdev = dev_get_drvdata(pdev);	\
	struct slp_multi_usb_function *f;	\
	char *buff = buf;	\
	\
	list_for_each_entry(f, &smdev->head, member) {	\
		dev_dbg(pdev, "usb: enabled_func=%s\n",	\
		       f->name);	\
		buff += snprintf(buff, PAGE_SIZE, "%s,", f->name);	\
	}	\
	if (buff != buf)	\
		*(buff - 1) = '\n';	\
	\
	return buff - buf;	\
}	\
static ssize_t store_##head(struct device *pdev,	\
		struct device_attribute *attr,	\
		const char *buff, size_t size)	\
{	\
	struct slp_multi_dev *smdev = dev_get_drvdata(pdev);	\
	char *name;	\
	char buf[256], *b;	\
	int err;	\
	\
	if (smdev->enabled) {	\
		dev_info(pdev, "can't change usb functions"	\
			"(already enabled)!!\n");	\
		return -EBUSY;	\
	}	\
	\
	INIT_LIST_HEAD(&smdev->head);	\
	\
	dev_dbg(pdev, "usb: buff=%s\n", buff);	\
	strlcpy(buf, buff, sizeof(buf));	\
	b = strim(buf);	\
	\
	while (b) {	\
		name = strsep(&b, ",");	\
		if (name) {	\
			err = add_##member(smdev, name);	\
			if (err)	\
				dev_err(pdev, \
					"slp_multi_usb: Cannot enable '%s'", \
					name); \
		}	\
	}	\
	\
	return size;	\
}	\
static DEVICE_ATTR(head, S_IRUGO | S_IWUSR, show_##head, store_##head);

ADD_FUNCS_LIST(funcs_fconf, fconf_list)
ADD_FUNCS_LIST(funcs_sconf, sconf_list)

/*-------------------------------------------------------------------------*/
/* /sys/class/usb_mode/usb%d/ interface */

static ssize_t pm_qos_show(struct device *pdev,
			   struct device_attribute *attr, char *buf)
{
	struct slp_multi_dev *smdev = dev_get_drvdata(pdev);

	return snprintf(buf, PAGE_SIZE, "%s\n", smdev->pm_qos);
}

static ssize_t pm_qos_store(struct device *pdev,
			   struct device_attribute *attr,
			   const char *buff, size_t size)
{
	struct slp_multi_dev *smdev = dev_get_drvdata(pdev);

	if (smdev->enabled) {
		dev_info(pdev, "Already usb enabled, can't change qos\n");
		return -EBUSY;
	}

	if (!(strncmp(buff, "high", 4)) || !(strncmp(buff, "low", 3))) {
		dev_err(pdev, "not supported cmd, can't set it\n");
		return -EINVAL;
	}

	strlcpy(smdev->pm_qos, buff, sizeof(smdev->pm_qos));
	return size;
}

static DEVICE_ATTR(pm_qos, S_IRUGO | S_IWUSR, pm_qos_show, pm_qos_store);

static ssize_t enable_show(struct device *pdev,
			   struct device_attribute *attr, char *buf)
{
	struct slp_multi_dev *smdev = dev_get_drvdata(pdev);
	dev_dbg(pdev, "usb: smdev->enabled=%d\n", smdev->enabled);
	return snprintf(buf, PAGE_SIZE, "%d\n", smdev->enabled);
}

static ssize_t enable_store(struct device *pdev, struct device_attribute *attr,
			    const char *buff, size_t size)
{
	struct slp_multi_dev *smdev = dev_get_drvdata(pdev);
	struct usb_composite_dev *cdev = smdev->cdev;
	struct list_head *this, *tmp;
	int enabled;
	int ret = 0;

	if (sysfs_streq(buff, "1"))
		enabled = 1;
	else if (sysfs_streq(buff, "0"))
		enabled = 0;
	else {
		dev_err(pdev, "Invalid cmd %c%c..", *buff, *(buff+1));
		return -EINVAL;
	}

	dev_info(pdev, "try to enabled=%d by %s(%d), smdev->enabled=%d(current)\n",
	       enabled, current->comm, current->pid, smdev->enabled);

	mutex_lock(&smdev->enable_lock);

	if (enabled && !smdev->enabled) {
		struct slp_multi_usb_function *f;

		/* update values in composite driver's
		 * copy of device descriptor
		 */
		cdev->desc.idVendor = device_desc.idVendor;
		cdev->desc.idProduct = device_desc.idProduct;
		cdev->desc.bcdDevice = device_desc.bcdDevice;

		list_for_each_entry(f, &smdev->funcs_fconf, fconf_list) {
			if (!strcmp(f->name, "acm"))
				cdev->desc.bcdDevice =
					cpu_to_le16(0x0400);
		}

		list_for_each_entry(f, &smdev->funcs_sconf, sconf_list) {
			if (!strcmp(f->name, "acm"))
				cdev->desc.bcdDevice =
					cpu_to_le16(0x0400);
			smdev->dual_config = true;
		}

		cdev->desc.bDeviceClass = device_desc.bDeviceClass;
		cdev->desc.bDeviceSubClass = device_desc.bDeviceSubClass;
		cdev->desc.bDeviceProtocol = device_desc.bDeviceProtocol;

		dev_dbg(pdev, "usb: %s vendor=%x,product=%x,bcdDevice=%x",
		       __func__, cdev->desc.idVendor,
		       cdev->desc.idProduct, cdev->desc.bcdDevice);
		dev_dbg(pdev, ",Class=%x,SubClass=%x,Protocol=%x\n",
		       cdev->desc.bDeviceClass,
		       cdev->desc.bDeviceSubClass, cdev->desc.bDeviceProtocol);
		dev_dbg(pdev, "usb: %s next cmd : usb_add_config\n",
		       __func__);

		ret = usb_add_config(cdev,
				&first_config_driver, slp_multi_bind_config);
		if (ret < 0) {
			dev_err(pdev,
				"usb_add_config fail-1st(%d)\n", ret);
			smdev->dual_config = false;
			goto done;
		}

		if (smdev->dual_config) {
			ret = usb_add_config(cdev, &second_config_driver,
				       slp_multi_bind_config);
			if (ret < 0) {
				dev_err(pdev,
					"usb_add_config fail-2nd(%d)\n", ret);
				smdev->dual_config = false;
				goto enable_conf_err;
			}
		}

		if ((smdev->swfi_latency != PM_QOS_DEFAULT_VALUE) &&
			!(strncmp(smdev->pm_qos, "high", 4)) &&
				(smdev->curr_latency == PM_QOS_DEFAULT_VALUE)) {
			smdev->curr_latency = smdev->swfi_latency;
			pm_qos_update_request(&smdev->pm_qos_req_dma,
				smdev->swfi_latency);
			dev_info(pdev, "PM QOS changed to HIGH\n");
		}

		smdev->enabled = true;

		ret = usb_gadget_connect(cdev->gadget);
		if (ret < 0) {
			dev_err(pdev, "can't connected gadget(%d)\n", ret);
			smdev->enabled = false;
			goto enable_conf_err;
		}

	} else if (!enabled && smdev->enabled) {
		usb_gadget_disconnect(cdev->gadget);

		smdev->enabled = false;

		/* Cancel pending control requests if it available */
		usb_ep_dequeue(cdev->gadget->ep0, cdev->req);

		usb_remove_config(cdev, &first_config_driver);
		if (smdev->dual_config)
			usb_remove_config(cdev, &second_config_driver);
		smdev->dual_config = false;

		/* remove all evt if it remained */
		mutex_lock(&smdev->evt_mutex);
		if (!list_empty(&smdev->evt_list)) {
			list_for_each_safe(this, tmp, &smdev->evt_list) {
				struct slp_multi_evt *evt;

				evt = list_entry(this,
					struct slp_multi_evt, node);
				list_del(&evt->node);
				kfree(evt);
			}
		}
		mutex_unlock(&smdev->evt_mutex);

		if (smdev->curr_latency != PM_QOS_DEFAULT_VALUE) {
			smdev->curr_latency = PM_QOS_DEFAULT_VALUE;
			pm_qos_update_request(&smdev->pm_qos_req_dma,
				PM_QOS_DEFAULT_VALUE);
			dev_info(pdev, "PM QOS changed to DEFAULT\n");
		}

		if (smdev->ustate != USB_STATE_NOTATTACHED) {
			dev_info(pdev, "forcely send disconnect uevent\n");
			smdev->ustate = USB_STATE_NOTATTACHED;
			kobject_uevent(&smdev->dev.kobj, KOBJ_CHANGE);
		}

	} else {
		dev_info(pdev, "slp_multi_usb: already %s\n",
		       smdev->enabled ? "enabled" : "disabled");
	}

	goto done;

enable_conf_err:
	if (smdev->dual_config) {
		usb_remove_config(cdev, &second_config_driver);
		smdev->dual_config = false;
	}
	usb_remove_config(cdev, &first_config_driver);

	mutex_lock(&smdev->evt_mutex);
	if (!list_empty(&smdev->evt_list)) {
		list_for_each_safe(this, tmp, &smdev->evt_list) {
			struct slp_multi_evt *evt;

			evt = list_entry(this, struct slp_multi_evt, node);
			list_del(&evt->node);
			kfree(evt);
		}
	}
	mutex_unlock(&smdev->evt_mutex);

	if (smdev->ustate != USB_STATE_NOTATTACHED) {
		dev_info(pdev, "forcely send disconnect uevent\n");
		smdev->ustate = USB_STATE_NOTATTACHED;
		kobject_uevent(&smdev->dev.kobj, KOBJ_CHANGE);
	}

done:
	mutex_unlock(&smdev->enable_lock);
	return (ret < 0 ? ret : size);
}

static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR, enable_show, enable_store);

#define DESCRIPTOR_ATTR(field, format_string)				\
static ssize_t								\
field ## _show(struct device *dev, struct device_attribute *attr,	\
		char *buf)						\
{									\
	return snprintf(buf, PAGE_SIZE, format_string, device_desc.field);\
}									\
static ssize_t								\
field ## _store(struct device *dev, struct device_attribute *attr,	\
		const char *buf, size_t size)	\
{									\
	int value;	\
	if (sscanf(buf, format_string, &value) == 1) {			\
		device_desc.field = value;				\
		return size;						\
	}								\
	return -EINVAL;							\
}									\
static DEVICE_ATTR(field, S_IRUGO | S_IWUSR, field ## _show, field ## _store);

#define DESCRIPTOR_STRING_ATTR(field, buffer)	\
static ssize_t	\
field ## _show(struct device *dev, struct device_attribute *attr,	\
		char *buf)	\
{	\
	return snprintf(buf, PAGE_SIZE, "%s", buffer);	\
}	\
static ssize_t	\
field ## _store(struct device *dev, struct device_attribute *attr,	\
		const char *buf, size_t size)	\
{	\
	if ((size >= sizeof(buffer)) ||	\
		(sscanf(buf, "%s", buffer) != 1)) {	\
		return -EINVAL;	\
	}	\
	return size;	\
}	\
static DEVICE_ATTR(field, S_IRUGO | S_IWUSR, field ## _show, field ## _store);

DESCRIPTOR_ATTR(idVendor, "%04x\n")
DESCRIPTOR_ATTR(idProduct, "%04x\n")
DESCRIPTOR_ATTR(bcdDevice, "%04x\n")
DESCRIPTOR_ATTR(bDeviceClass, "%d\n")
DESCRIPTOR_ATTR(bDeviceSubClass, "%d\n")
DESCRIPTOR_ATTR(bDeviceProtocol, "%d\n")
DESCRIPTOR_STRING_ATTR(iManufacturer, manufacturer_string)
DESCRIPTOR_STRING_ATTR(iProduct, product_string)
DESCRIPTOR_STRING_ATTR(iSerial, serial_string)

static struct attribute *slp_multi_attributes[] = {
	&dev_attr_idVendor.attr,
	&dev_attr_idProduct.attr,
	&dev_attr_bcdDevice.attr,
	&dev_attr_bDeviceClass.attr,
	&dev_attr_bDeviceSubClass.attr,
	&dev_attr_bDeviceProtocol.attr,
	&dev_attr_iManufacturer.attr,
	&dev_attr_iProduct.attr,
	&dev_attr_iSerial.attr,
	&dev_attr_funcs_fconf.attr,
	&dev_attr_funcs_sconf.attr,
	&dev_attr_enable.attr,
	&dev_attr_pm_qos.attr,
	NULL
};

static const struct attribute_group slp_multi_attr_group = {
	.attrs = slp_multi_attributes,
};

static const struct attribute_group *slp_multi_attr_groups[] = {
	&slp_multi_attr_group,
	NULL,
};

/*-------------------------------------------------------------------------*/
/* Composite driver */

static int slp_multi_bind_config(struct usb_configuration *c)
{
	struct slp_multi_dev *smdev;
	int ret = 0;

	smdev = cdev_to_smdev(c->cdev);
	if (!smdev) {
		pr_err("%s: can't get smdev by cdev\n", __func__);
		return -ENODEV;
	}

	ret = slp_multi_bind_enabled_functions(smdev, c);

	return ret;
}

static void slp_multi_unbind_config(struct usb_configuration *c)
{
	struct slp_multi_dev *smdev;

	smdev = cdev_to_smdev(c->cdev);
	if (!smdev) {
		pr_err("%s: can't get smdev by cdev\n", __func__);
		return;
	}

	slp_multi_unbind_enabled_functions(smdev, c);
}

static int slp_multi_bind(struct usb_composite_dev *cdev)
{
	struct slp_multi_dev *smdev;
	struct usb_gadget *gadget = cdev->gadget;
	int gcnum, id, ret;

	/* match free smdev for cdev */
	smdev = cdev_to_smdev(NULL);
	if (!smdev) {
		pr_err("%s: can't find free smdev\n", __func__);
		return -ENODEV;
	}

	dev_dbg(&smdev->dev, "usb: %s disconnect\n", __func__);
	usb_gadget_disconnect(gadget);

	/* Allocate string descriptor numbers ... note that string
	 * contents can be overridden by the composite_dev glue.
	 */
	id = usb_string_id(cdev);
	if (id < 0)
		return id;
	strings_dev[STRING_MANUFACTURER_IDX].id = id;
	device_desc.iManufacturer = id;

	id = usb_string_id(cdev);
	if (id < 0)
		return id;
	strings_dev[STRING_PRODUCT_IDX].id = id;
	device_desc.iProduct = id;

	/* Default strings - should be updated by userspace */
	strlcpy(manufacturer_string, "Samsung\0",
		sizeof(manufacturer_string) - 1);
	strlcpy(product_string, "SLP\0", sizeof(product_string) - 1);
	snprintf(serial_string, sizeof(serial_string),
		 "%08x%08x", system_serial_high, system_serial_low);

	id = usb_string_id(cdev);
	if (id < 0)
		return id;
	strings_dev[STRING_SERIAL_IDX].id = id;
	device_desc.iSerialNumber = id;

	ret = slp_multi_init_functions(smdev, cdev);
	if (ret)
		return ret;

	gcnum = usb_gadget_controller_number(gadget);
	if (gcnum >= 0)
		device_desc.bcdDevice = cpu_to_le16(0x0200 + gcnum);
	else {
		/* gadget zero is so simple (for now, no altsettings) that
		 * it SHOULD NOT have problems with bulk-capable hardware.
		 * so just warn about unrcognized controllers -- don't panic.
		 *
		 * things like configuration and altsetting numbering
		 * can need hardware-specific attention though.
		 */
		dev_warn(&smdev->dev, "%s: controller '%s' not recognized\n",
			   slp_longname, gadget->name);
		device_desc.bcdDevice = __constant_cpu_to_le16(0x9999);
	}

	usb_gadget_set_selfpowered(gadget);
	smdev->cdev = cdev;

	return 0;
}

static int slp_multi_usb_unbind(struct usb_composite_dev *cdev)
{
	struct slp_multi_dev *smdev;

	smdev = cdev_to_smdev(cdev);
	if (!smdev) {
		pr_err("%s: can't get smdev by cdev\n", __func__);
		return -ENODEV;
	}

	dev_dbg(&smdev->dev, "usb: %s\n", __func__);
	slp_multi_cleanup_functions(smdev);

	smdev->cdev = ERR_PTR(-ENODEV);

	return 0;
}

static void slp_multi_usb_disconnect(struct usb_composite_dev *cdev)
{
	struct slp_multi_dev *smdev;

	smdev = cdev_to_smdev(cdev);
	if (!smdev) {
		pr_err("%s: can't get smdev by cdev\n", __func__);
		return;
	}

	/* to prevent evt queuing during dis/enable control*/
	if (list_empty(&cdev->configs))
		return;

	if (!strncmp(smdev->pm_qos, "high", 4)) {
		dev_info(&smdev->dev, "queue default qos evt\n");
		slp_multi_qos_evt(smdev, PM_QOS_DEFAULT_VALUE);
	}

	if (smdev->attached) {
		smdev->attached = false;
		dev_info(&smdev->dev, "queue disconnect evt\n");
		slp_multi_state_evt(smdev, USB_STATE_NOTATTACHED);
	}
}

static void slp_multi_usb_resume(struct usb_composite_dev *cdev)
{
	struct slp_multi_dev *smdev = cdev_to_smdev(cdev);

	if (!smdev) {
		pr_err("%s: can't get smdev by cdev\n", __func__);
		return;
	}

	/* to prevent evt queuing during dis/enable control*/
	if (list_empty(&cdev->configs))
		return;

	if (!strncmp(smdev->pm_qos, "high", 4)) {
		dev_info(&smdev->dev, "queue high qos evt\n");
		slp_multi_qos_evt(smdev, smdev->swfi_latency);
	}
}

static void slp_multi_usb_suspend(struct usb_composite_dev *cdev)
{
	struct slp_multi_dev *smdev = cdev_to_smdev(cdev);

	if (!smdev) {
		pr_err("%s: can't get smdev by cdev\n", __func__);
		return;
	}

	/* to prevent evt queuing during dis/enable control*/
	if (list_empty(&cdev->configs))
		return;

	if (!strncmp(smdev->pm_qos, "high", 4)) {
		dev_info(&smdev->dev, "queue default qos evt\n");
		slp_multi_qos_evt(smdev, PM_QOS_DEFAULT_VALUE);
	}
}

static struct usb_composite_driver slp_multi_composite = {
	.name = "slp_multi_composite",
	.dev = &device_desc,
	.strings = slp_dev_strings,
	.unbind = slp_multi_usb_unbind,
	.disconnect = slp_multi_usb_disconnect,
	.max_speed = USB_SPEED_SUPER,
	.resume = slp_multi_usb_resume,
	.suspend = slp_multi_usb_suspend,
};

static int
slp_multi_setup(struct usb_gadget *gadget, const struct usb_ctrlrequest *ctrl)
{
	struct usb_composite_dev *cdev = get_gadget_data(gadget);
	struct slp_multi_dev *smdev;
	u8 b_requestType = ctrl->bRequestType;
	struct slp_multi_usb_function *f;
	int value = -EOPNOTSUPP;

	smdev = cdev_to_smdev(cdev);

	if (!smdev->attached) {
		/* Set attached when device got a first USB command */
		smdev->attached = true;
		slp_multi_state_evt(smdev, USB_STATE_ATTACHED);
	}

	if (smdev && ((b_requestType & USB_TYPE_MASK) == USB_TYPE_VENDOR)) {
		struct usb_request *req = cdev->req;

		req->zero = 0;
		req->complete = composite_setup_complete;
		req->length = 0;
		gadget->ep0->driver_data = cdev;

		/* To check & report it to platform , we check it all */
		list_for_each_entry(f, &smdev->available_functions,
			available_list) {
			if (f->ctrlrequest) {
				value = f->ctrlrequest(f, cdev, ctrl);
				if (value >= 0)
					break;
			}
		}
	}

	if (value < 0)
		value = composite_setup(gadget, ctrl);

	if (smdev && (value >= 0) && (cdev->config)
		&& (ctrl->bRequest == USB_REQ_SET_CONFIGURATION))
		slp_multi_state_evt(smdev, USB_STATE_CONFIGURED);

	return value;
}

static void slp_multi_release(struct device *dev)
{
	struct slp_multi_dev *smdev = dev_get_drvdata(dev);

	dev_dbg(dev, "releasing '%s'\n", dev_name(dev));
	dev_set_drvdata(dev, NULL);
	ida_simple_remove(&slp_multi_ida, smdev->id);
	kfree(smdev);
}

static struct slp_multi_dev *slp_multi_create_device(struct device *pdev,
				struct slp_multi_platform_data *pdata)
{
	struct slp_multi_dev *smdev;
	struct slp_multi_usb_function *f;
	struct slp_multi_usb_function **functions = supported_functions;

	int i, err;

	smdev = kzalloc(sizeof(*smdev), GFP_KERNEL);
	if (!smdev) {
		dev_err(pdev, "usb_mode: can't alloc for smdev\n");
		return ERR_PTR(-ENOMEM);
	}

	INIT_LIST_HEAD(&smdev->available_functions);
	INIT_LIST_HEAD(&smdev->funcs_fconf);
	INIT_LIST_HEAD(&smdev->funcs_sconf);
	INIT_LIST_HEAD(&smdev->evt_list);
	mutex_init(&smdev->evt_mutex);
	mutex_init(&smdev->enable_lock);
	spin_lock_init(&smdev->evt_lock);
	INIT_WORK(&smdev->evt_work, slp_multi_evt_thread);

	err = ida_simple_get(&slp_multi_ida, 0, 0, GFP_KERNEL);
	if (err < 0) {
		dev_err(pdev, "failed to gat idr for smdev\n");
		goto err_ida_get;
	}
	smdev->id = err;
	smdev->ustate = USB_STATE_NOTATTACHED;

	smdev->dev.class = slp_multi_class;
	smdev->dev.groups = slp_multi_attr_groups;
	smdev->dev.parent = pdev;
	smdev->dev.release = slp_multi_release;
	dev_set_drvdata(&smdev->dev, smdev);

	err = dev_set_name(&smdev->dev, "%s%d", "usb", smdev->id);
	if (err) {
		dev_err(pdev, "failed to set smdev name(%d)\n", err);
		goto err_set_name;
	}

	err = device_register(&smdev->dev);
	if (err) {
		dev_err(pdev, "failed to device_add(%d)\n", err);
		put_device(&smdev->dev);
		return ERR_PTR(err);
	}

	while ((f = *functions++)) {
		for (i = 0; i < pdata->nfuncs; i++)
			if ((!f->used) &&
				(!strcmp(pdata->enable_funcs[i], f->name))) {
				list_add_tail(&f->available_list,
					      &smdev->available_functions);
				f->used = 1;
			}
	}

	return smdev;

err_set_name:
	dev_set_drvdata(&smdev->dev, NULL);
	ida_simple_remove(&slp_multi_ida, smdev->id);
err_ida_get:
	kfree(smdev);

	return ERR_PTR(err);
}

static void slp_multi_destroy_device(struct slp_multi_dev *smdev)
{
	struct slp_multi_usb_function *f;
	struct list_head *this, *tmp;

	while (!list_empty(&smdev->available_functions)) {
		f = list_first_entry(&smdev->available_functions,
				struct slp_multi_usb_function, available_list);
		list_del(&f->available_list);
		f->used = 0;
	}

	cancel_work_sync(&smdev->evt_work);

	if (!list_empty(&smdev->evt_list)) {
		list_for_each_safe(this, tmp, &smdev->evt_list) {
			struct slp_multi_evt *evt;

			evt = list_entry(this, struct slp_multi_evt, node);
			list_del(&evt->node);
			kfree(evt);
		}
	}

	device_unregister(&smdev->dev);
}

static int __devinit slp_multi_probe(struct platform_device *pdev)
{
	struct slp_multi_dev *smdev;
	struct device_node *np = pdev->dev.of_node;
	struct slp_multi_platform_data *pdata;
	const char **enable_funcs;
	int i, err;

	if (np) {
		dev_dbg(&pdev->dev, "device tree enabled\n");
		pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata) {
			dev_err(&pdev->dev, "unable to allocate platform data\n");
			return -ENOMEM;
		}

		if (of_find_property(np, "slp,slp_mult-nluns", NULL))
			of_property_read_u32(np, "slp,slp_mult-nluns",
				&pdata->nluns);

		if (of_find_property(np, "slp,slp_mult-swfi-latency", NULL))
			of_property_read_u32(np, "slp,slp_mult-swfi-latency",
				&pdata->swfi_latency);

		if (of_find_property(np, "slp,slp_mult-enable-funcs", NULL)) {
			pdata->nfuncs = of_property_count_strings(np,
				"slp,slp_mult-enable-funcs");

			if (pdata->nfuncs < 0) {
				dev_err(&pdev->dev,
					"fail to count enable-funcs(%d)\n",
						pdata->nfuncs);
				return -EINVAL;
			}

			enable_funcs = devm_kzalloc(&pdev->dev,
				pdata->nfuncs * sizeof(**enable_funcs),
					GFP_KERNEL);
			if (!enable_funcs) {
				dev_err(&pdev->dev, "unable to allocate platform data\n");
				return -ENOMEM;
			}

			for (i = 0; i < pdata->nfuncs; i++)
				of_property_read_string_index(pdev->dev.of_node,
					"slp,slp_mult-enable-funcs",
					i, &enable_funcs[i]);
			pdata->enable_funcs = enable_funcs;
		} else {
			dev_err(&pdev->dev, "There is no enable-functions\n");
			return -EINVAL;
		}
	} else {
		pdata = pdev->dev.platform_data;
	}

	smdev = slp_multi_create_device(&pdev->dev, pdata);
	if (IS_ERR(smdev)) {
		dev_err(&pdev->dev, "usb_mode: can't create device\n");
		return PTR_ERR(smdev);
	}

	slp_multi_nluns = pdata->nluns;

	if (pdata->swfi_latency) {
		smdev->swfi_latency = pdata->swfi_latency + 1;
		pm_qos_add_request(&smdev->pm_qos_req_dma,
			PM_QOS_CPU_DMA_LATENCY, PM_QOS_DEFAULT_VALUE);
		strlcpy(smdev->pm_qos, "high", sizeof(smdev->pm_qos));
	} else {
		smdev->swfi_latency = PM_QOS_DEFAULT_VALUE;
		strlcpy(smdev->pm_qos, "NONE", sizeof(smdev->pm_qos));
	}
	smdev->curr_latency = PM_QOS_DEFAULT_VALUE;

	platform_set_drvdata(pdev, smdev);
	list_add_tail(&smdev->list, &smdev_list);


	/* Override composite driver functions */
	composite_driver.setup = slp_multi_setup;

	err = usb_composite_probe(&slp_multi_composite, slp_multi_bind);
	if (err) {
		dev_err(&pdev->dev, "usb_mode: can't probe composite\n");
		goto err_comp_prb;
	}

	dev_info(&pdev->dev, "usb_mode driver, version:" USB_MODE_VERSION
	       "," " init Ok\n");

	return 0;

 err_comp_prb:
	list_del(&smdev->list);
	platform_set_drvdata(pdev, NULL);
	slp_multi_destroy_device(smdev);

	return err;
}

static int __devexit slp_multi_remove(struct platform_device *pdev)
{
	struct slp_multi_platform_data *pdata = pdev->dev.platform_data;
	struct slp_multi_dev *smdev = platform_get_drvdata(pdev);

	if (smdev->enabled) {
		dev_info(&pdev->dev, "start remove usb_mode driver without disabled\n");
		smdev->enabled = false;
	}

	usb_composite_unregister(&slp_multi_composite);

	list_del(&smdev->list);
	platform_set_drvdata(pdev, NULL);

	if (pdata->swfi_latency != PM_QOS_DEFAULT_VALUE)
		pm_qos_remove_request(&smdev->pm_qos_req_dma);

	slp_multi_destroy_device(smdev);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id of_slp_multi_match[] = {
	{
		.compatible = "slp,slp_multi",
	},
	{ },
};
MODULE_DEVICE_TABLE(of, of_slp_multi_match);
#endif

static struct platform_driver slp_multi_driver = {
	.probe = slp_multi_probe,
	.remove = __devexit_p(slp_multi_remove),
	.driver = {
		   .name = "slp_multi",
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(of_slp_multi_match),
	},
};

static CLASS_ATTR_STRING(version, S_IRUSR | S_IRGRP | S_IROTH,
			 USB_MODE_VERSION);

static int slp_multi_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	struct slp_multi_dev *smdev =
			container_of(dev, struct slp_multi_dev, dev);
	int ret;

	if (!smdev) {
		dev_info(dev, "not yet created smdev!\n");
		return -ENODEV;
	}

	ret = add_uevent_var(env, "USB_STATE=%s", ustate_string[smdev->ustate]);
	if (ret)
		dev_err(dev, "failed to add uevent USB_STATE\n");

	return ret;
}

static int __init slp_multi_init(void)
{
	int err;

	slp_multi_class = class_create(THIS_MODULE, "usb_mode");
	if (IS_ERR(slp_multi_class)) {
		pr_err("failed to create slp_multi class --> %ld\n",
				PTR_ERR(slp_multi_class));
		return PTR_ERR(slp_multi_class);
	}

	slp_multi_class->dev_uevent = slp_multi_uevent;

	err = class_create_file(slp_multi_class, &class_attr_version.attr);
	if (err) {
		pr_err("usb_mode: can't create sysfs version file\n");
		goto err_class;
	}

	err = platform_driver_register(&slp_multi_driver);
	if (err) {
		pr_err("usb_mode: can't register driver\n");
		goto err_attr;
	}

	return 0;

err_attr:
	class_remove_file(slp_multi_class, &class_attr_version.attr);
err_class:
	class_destroy(slp_multi_class);

	return err;
}
module_init(slp_multi_init);

static void __exit slp_multi_exit(void)
{
	platform_driver_unregister(&slp_multi_driver);
	class_remove_file(slp_multi_class, &class_attr_version.attr);
	class_destroy(slp_multi_class);
}
module_exit(slp_multi_exit);
