/* 
 * Copyright (c)2013 Maxim Integrated Products, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include "../ssp.h"

/*************************************************************************/
/* factory Sysfs                                                         */
/*************************************************************************/

#define VENDOR		"ADI"
#define CHIP_ID		"ADPD142"

static ssize_t hrm_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", VENDOR);
}

static ssize_t hrm_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", CHIP_ID);
}

static ssize_t hrm_eol_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int iCount = 0;
	struct ssp_data *data = dev_get_drvdata(dev);

	iCount = sprintf(buf,"%d %d %d %d %d %d %d\n",
			data->buf[BIO_HRM_RAW_FAC].frequency,
			data->buf[BIO_HRM_RAW_FAC].noise_value,
			data->buf[BIO_HRM_RAW_FAC].dc_value,
			data->buf[BIO_HRM_RAW_FAC].ac_value,
			data->buf[BIO_HRM_RAW_FAC].perfusion_rate,
			data->buf[BIO_HRM_RAW_FAC].snrac,
			data->buf[BIO_HRM_RAW_FAC].snrdc);

	return iCount;
}

static ssize_t hrm_eol_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int iRet;
	int64_t dEnable;
	struct ssp_data *data = dev_get_drvdata(dev);

	iRet = kstrtoll(buf, 10, &dEnable);
	if (iRet < 0)
		return iRet;

	if (dEnable)
		atomic_set(&data->eol_enable, 1);
	else
		atomic_set(&data->eol_enable, 0);

	return size;
}

static ssize_t hrm_raw_data_read(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d,%d\n",
		data->buf[BIO_HRM_RAW].ch_a,
		data->buf[BIO_HRM_RAW].ch_b);
}

static ssize_t hrm_lib_data_read(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n",
		data->buf[BIO_HRM_LIB].hr,
		data->buf[BIO_HRM_LIB].rri,
		data->buf[BIO_HRM_LIB].snr);
}

static DEVICE_ATTR(name, S_IRUGO, hrm_name_show, NULL);
static DEVICE_ATTR(vendor, S_IRUGO, hrm_vendor_show, NULL);
static DEVICE_ATTR(hrm_eol, S_IRUGO | S_IWUSR | S_IWGRP, hrm_eol_show, hrm_eol_store);
static DEVICE_ATTR(hrm_raw, S_IRUGO, hrm_raw_data_read, NULL);
static DEVICE_ATTR(hrm_lib, S_IRUGO, hrm_lib_data_read, NULL);

static struct device_attribute *hrm_attrs[] = {
	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_hrm_eol,
	&dev_attr_hrm_raw,
	&dev_attr_hrm_lib,
	NULL,
};

void initialize_hrm_factorytest(struct ssp_data *data)
{
	sensors_register(data->hrm_device, data, hrm_attrs,
		"hrm_sensor");
}

void remove_hrm_factorytest(struct ssp_data *data)
{
	sensors_unregister(data->hrm_device, hrm_attrs);
}
