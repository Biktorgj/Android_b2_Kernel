/*
 *  Copyright (C) 2012, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */
#include "ssp.h"

/*************************************************************************/
/* SSP Kernel -> HAL input evnet function                                */
/*************************************************************************/
void convert_acc_data(s16 *iValue)
{
	if (*iValue > MAX_ACCEL_2G)
		*iValue = ((MAX_ACCEL_4G - *iValue)) * (-1);
}

void report_acc_data(struct ssp_data *data, struct sensor_value *accdata)
{
	data->buf[ACCELEROMETER_SENSOR].x = accdata->x;
	data->buf[ACCELEROMETER_SENSOR].y = accdata->y;
	data->buf[ACCELEROMETER_SENSOR].z = accdata->z;

	input_report_rel(data->acc_input_dev, REL_X,
		data->buf[ACCELEROMETER_SENSOR].x);
	input_report_rel(data->acc_input_dev, REL_Y,
		data->buf[ACCELEROMETER_SENSOR].y);
	input_report_rel(data->acc_input_dev, REL_Z,
		data->buf[ACCELEROMETER_SENSOR].z);
	input_sync(data->acc_input_dev);
}

void report_gyro_data(struct ssp_data *data, struct sensor_value *gyrodata)
{
	long lTemp[3] = {0,};
	data->buf[GYROSCOPE_SENSOR].x = gyrodata->x;
	data->buf[GYROSCOPE_SENSOR].y = gyrodata->y;
	data->buf[GYROSCOPE_SENSOR].z = gyrodata->z;

	if (data->uGyroDps == GYROSCOPE_DPS500) {
		lTemp[0] = (long)data->buf[GYROSCOPE_SENSOR].x;
		lTemp[1] = (long)data->buf[GYROSCOPE_SENSOR].y;
		lTemp[2] = (long)data->buf[GYROSCOPE_SENSOR].z;
	} else if (data->uGyroDps == GYROSCOPE_DPS250)	{
		lTemp[0] = (long)data->buf[GYROSCOPE_SENSOR].x >> 1;
		lTemp[1] = (long)data->buf[GYROSCOPE_SENSOR].y >> 1;
		lTemp[2] = (long)data->buf[GYROSCOPE_SENSOR].z >> 1;
	} else if (data->uGyroDps == GYROSCOPE_DPS2000)	{
		lTemp[0] = (long)data->buf[GYROSCOPE_SENSOR].x << 2;
		lTemp[1] = (long)data->buf[GYROSCOPE_SENSOR].y << 2;
		lTemp[2] = (long)data->buf[GYROSCOPE_SENSOR].z << 2;
	} else {
		lTemp[0] = (long)data->buf[GYROSCOPE_SENSOR].x;
		lTemp[1] = (long)data->buf[GYROSCOPE_SENSOR].y;
		lTemp[2] = (long)data->buf[GYROSCOPE_SENSOR].z;
	}

	input_report_rel(data->gyro_input_dev, REL_RX, lTemp[0]);
	input_report_rel(data->gyro_input_dev, REL_RY, lTemp[1]);
	input_report_rel(data->gyro_input_dev, REL_RZ, lTemp[2]);
	input_sync(data->gyro_input_dev);
}

#ifdef CONFIG_SENSORS_SSP_ADPD142
void report_hrm_raw_data(struct ssp_data *data, struct sensor_value *hrmdata)
{
	data->buf[BIO_HRM_RAW].ch_a = hrmdata->ch_a;
	data->buf[BIO_HRM_RAW].ch_b = hrmdata->ch_b;
	input_report_rel(data->hrm_raw_input_dev, REL_X, data->buf[BIO_HRM_RAW].ch_a + 1);
	input_report_rel(data->hrm_raw_input_dev, REL_Y, data->buf[BIO_HRM_RAW].ch_b + 1);
	input_report_rel(data->hrm_raw_input_dev, REL_Z, 1);	// Dummy code
	input_sync(data->hrm_raw_input_dev);
}

void report_hrm_raw_fac_data(struct ssp_data *data, struct sensor_value *hrmdata)
{
	data->buf[BIO_HRM_RAW_FAC].ch_a = hrmdata->ch_a;
	data->buf[BIO_HRM_RAW_FAC].ch_b = hrmdata->ch_b;
	data->buf[BIO_HRM_RAW_FAC].frequency = hrmdata->frequency;
	data->buf[BIO_HRM_RAW_FAC].noise_value = hrmdata->noise_value;
	data->buf[BIO_HRM_RAW_FAC].dc_value = hrmdata->dc_value;
	data->buf[BIO_HRM_RAW_FAC].ac_value = hrmdata->ac_value;
	data->buf[BIO_HRM_RAW_FAC].perfusion_rate = hrmdata->perfusion_rate;
	data->buf[BIO_HRM_RAW_FAC].snrac = hrmdata->snrac;
	data->buf[BIO_HRM_RAW_FAC].snrdc = hrmdata->snrdc;
	input_report_rel(data->hrm_raw_input_dev, REL_X, data->buf[BIO_HRM_RAW_FAC].ch_a + 1);
	input_report_rel(data->hrm_raw_input_dev, REL_Y, data->buf[BIO_HRM_RAW_FAC].ch_b + 1);
	input_report_rel(data->hrm_raw_input_dev, REL_Z, 1);	// Dummy code
	input_sync(data->hrm_raw_input_dev);
}

void report_hrm_lib_data(struct ssp_data *data, struct sensor_value *hrmdata)
{
	data->buf[BIO_HRM_LIB].hr = hrmdata->hr;
	data->buf[BIO_HRM_LIB].rri = hrmdata->rri;
	data->buf[BIO_HRM_LIB].snr = hrmdata->snr;

	input_report_rel(data->hrm_lib_input_dev, REL_X, data->buf[BIO_HRM_LIB].hr + 1);
	input_report_rel(data->hrm_lib_input_dev, REL_Y, data->buf[BIO_HRM_LIB].rri + 1);
	input_report_rel(data->hrm_lib_input_dev, REL_Z, data->buf[BIO_HRM_LIB].snr + 1);
	input_sync(data->hrm_lib_input_dev);
}
#endif

int initialize_event_symlink(struct ssp_data *data)
{
	int iRet = 0;

	iRet = sensors_create_symlink(data->acc_input_dev);
	if (iRet < 0)
		goto iRet_acc_sysfs_create_link;

	iRet = sensors_create_symlink(data->gyro_input_dev);
	if (iRet < 0)
		goto iRet_gyro_sysfs_create_link;

#ifdef CONFIG_SENSORS_SSP_ADPD142
	iRet = sensors_create_symlink(data->hrm_raw_input_dev);
	if (iRet < 0)
		goto iRet_hrm_raw_sysfs_create_link;

	iRet = sensors_create_symlink(data->hrm_lib_input_dev);
	if (iRet < 0)
		goto iRet_hrm_lib_sysfs_create_link;
#endif

	return SUCCESS;

#ifdef CONFIG_SENSORS_SSP_ADPD142
iRet_hrm_lib_sysfs_create_link:
	sensors_remove_symlink(data->hrm_raw_input_dev);
iRet_hrm_raw_sysfs_create_link:
	sensors_remove_symlink(data->gyro_input_dev);
#endif
iRet_gyro_sysfs_create_link:
	sensors_remove_symlink(data->acc_input_dev);
iRet_acc_sysfs_create_link:
	pr_err("[SSP]: %s - could not create event symlink\n", __func__);
	return FAIL;
}

void remove_event_symlink(struct ssp_data *data)
{
	sensors_remove_symlink(data->acc_input_dev);
	sensors_remove_symlink(data->gyro_input_dev);
#ifdef CONFIG_SENSORS_SSP_ADPD142
	sensors_remove_symlink(data->hrm_raw_input_dev);
	sensors_remove_symlink(data->hrm_lib_input_dev);
#endif
}

int initialize_input_dev(struct ssp_data *data)
{
	int iRet = 0;

	data->acc_input_dev = input_allocate_device();
	if (data->acc_input_dev == NULL)
		goto err_initialize_acc_input_dev;

	data->acc_input_dev->name = "accelerometer_sensor";
	input_set_capability(data->acc_input_dev, EV_REL, REL_X);
	input_set_capability(data->acc_input_dev, EV_REL, REL_Y);
	input_set_capability(data->acc_input_dev, EV_REL, REL_Z);

	iRet = input_register_device(data->acc_input_dev);
	if (iRet < 0) {
		input_free_device(data->acc_input_dev);
		goto err_initialize_acc_input_dev;
	}
	input_set_drvdata(data->acc_input_dev, data);

	data->gyro_input_dev = input_allocate_device();
	if (data->gyro_input_dev == NULL)
		goto err_initialize_gyro_input_dev;

	data->gyro_input_dev->name = "gyro_sensor";
	input_set_capability(data->gyro_input_dev, EV_REL, REL_RX);
	input_set_capability(data->gyro_input_dev, EV_REL, REL_RY);
	input_set_capability(data->gyro_input_dev, EV_REL, REL_RZ);

	iRet = input_register_device(data->gyro_input_dev);
	if (iRet < 0) {
		input_free_device(data->gyro_input_dev);
		goto err_initialize_gyro_input_dev;
	}
	input_set_drvdata(data->gyro_input_dev, data);

	data->key_input_dev = input_allocate_device();
	if (data->key_input_dev == NULL)
		goto err_initialize_key_input_dev;

	data->key_input_dev->name = "LPM_MOTION";
	input_set_capability(data->key_input_dev, EV_KEY, KEY_HOMEPAGE);

	iRet = input_register_device(data->key_input_dev);
	if (iRet < 0) {
		input_free_device(data->key_input_dev);
		goto err_initialize_key_input_dev;
	}
	input_set_drvdata(data->key_input_dev, data);

#ifdef CONFIG_SENSORS_SSP_ADPD142
	data->hrm_raw_input_dev = input_allocate_device();
	if (data->hrm_raw_input_dev == NULL)
		goto err_initialize_hrm_raw_input_dev;

	data->hrm_raw_input_dev->name = "hrm_raw_sensor";
	input_set_capability(data->hrm_raw_input_dev, EV_REL, REL_X);
	input_set_capability(data->hrm_raw_input_dev, EV_REL, REL_Y);
	input_set_capability(data->hrm_raw_input_dev, EV_REL, REL_Z);

	iRet = input_register_device(data->hrm_raw_input_dev);
	if (iRet < 0) {
		input_free_device(data->hrm_raw_input_dev);
		goto err_initialize_hrm_raw_input_dev;
	}
	input_set_drvdata(data->hrm_raw_input_dev, data);

	data->hrm_lib_input_dev = input_allocate_device();
	if (data->hrm_lib_input_dev == NULL)
		goto err_initialize_hrm_lib_input_dev;

	data->hrm_lib_input_dev->name = "hrm_lib_sensor";
	input_set_capability(data->hrm_lib_input_dev, EV_REL, REL_X);
	input_set_capability(data->hrm_lib_input_dev, EV_REL, REL_Y);
	input_set_capability(data->hrm_lib_input_dev, EV_REL, REL_Z);

	iRet = input_register_device(data->hrm_lib_input_dev);
	if (iRet < 0) {
		input_free_device(data->hrm_lib_input_dev);
		goto err_initialize_hrm_lib_input_dev;
	}
	input_set_drvdata(data->hrm_lib_input_dev, data);
#endif

	return SUCCESS;

#ifdef CONFIG_SENSORS_SSP_ADPD142
err_initialize_hrm_lib_input_dev:
	pr_err("[SSP]: %s - could not allocate hrm lib input device\n", __func__);
	input_unregister_device(data->hrm_raw_input_dev);
err_initialize_hrm_raw_input_dev:
	pr_err("[SSP]: %s - could not allocate hrm raw input device\n", __func__);
	input_unregister_device(data->key_input_dev);
#endif
err_initialize_key_input_dev:
	pr_err("[SSP]: %s - could not allocate key input device\n", __func__);
	input_unregister_device(data->gyro_input_dev);
err_initialize_gyro_input_dev:
	pr_err("[SSP]: %s - could not allocate gyro input device\n", __func__);
	input_unregister_device(data->acc_input_dev);
err_initialize_acc_input_dev:
	pr_err("[SSP]: %s - could not allocate acc input device\n", __func__);
	return ERROR;
}

void remove_input_dev(struct ssp_data *data)
{
	input_unregister_device(data->acc_input_dev);
	input_unregister_device(data->gyro_input_dev);
}
