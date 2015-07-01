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
#include "../ssp.h"

/*************************************************************************/
/* factory Sysfs                                                         */
/*************************************************************************/

#define MODEL_NAME			"STM32F401CCY6B"
#define SMART_ALERT_MOTION	8


ssize_t mcu_revision_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "ST01%u,ST01%u\n", data->uCurFirmRev,
		get_module_rev(data));
}

ssize_t mcu_model_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", MODEL_NAME);
}

ssize_t mcu_update_kernel_bin_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	bool bSuccess = false;
	int iRet = 0;
	struct ssp_data *data = dev_get_drvdata(dev);

	ssp_dbg("[SSP]: %s - mcu binany update!\n", __func__);

	iRet = forced_to_download_binary(data, UMS_BINARY);
	if (iRet == SUCCESS) {
		bSuccess = true;
		goto out;
	}

	iRet = forced_to_download_binary(data, KERNEL_BINARY);
	if (iRet == SUCCESS)
		bSuccess = true;
	else
		bSuccess = false;
out:
	return sprintf(buf, "%s\n", (bSuccess ? "OK" : "NG"));
}

ssize_t mcu_update_kernel_crashed_bin_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	bool bSuccess = false;
	int iRet = 0;
	struct ssp_data *data = dev_get_drvdata(dev);

	ssp_dbg("[SSP]: %s - mcu binany update!\n", __func__);

	iRet = forced_to_download_binary(data, UMS_BINARY);
	if (iRet == SUCCESS) {
		bSuccess = true;
		goto out;
	}

	iRet = forced_to_download_binary(data, KERNEL_CRASHED_BINARY);
	if (iRet == SUCCESS)
		bSuccess = true;
	else
		bSuccess = false;
out:
	return sprintf(buf, "%s\n", (bSuccess ? "OK" : "NG"));
}

ssize_t mcu_update_ums_bin_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	bool bSuccess = false;
	int iRet = 0;
	struct ssp_data *data = dev_get_drvdata(dev);

	ssp_dbg("[SSP]: %s - mcu binany update!\n", __func__);

	iRet = forced_to_download_binary(data, UMS_BINARY);
	if (iRet == SUCCESS)
		bSuccess = true;
	else
		bSuccess = false;

	return sprintf(buf, "%s\n", (bSuccess ? "OK" : "NG"));
}

ssize_t mcu_reset_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	reset_mcu(data);

	return sprintf(buf, "OK\n");
}

ssize_t mcu_dump_show(struct device *dev, struct device_attribute *attr,
		char *buf) {
	struct ssp_data *data = dev_get_drvdata(dev);
	int status = 1, iDelaycnt = 0;

	data->bDumping = true;
	set_big_data_start(data, BIG_TYPE_DUMP, 0);
	msleep(300);
	while (data->bDumping) {
		mdelay(10);
		if (iDelaycnt++ > 1000) {
			status = 0;
			break;
		}
	}
	return sprintf(buf, "%s\n", status ? "OK" : "NG");
}

static char buffer[FACTORY_DATA_MAX];

ssize_t mcu_factorytest_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	int iRet = 0;
	struct ssp_msg *msg;

	if (sysfs_streq(buf, "1")) {
		msg = kzalloc(sizeof(*msg), GFP_KERNEL);
		if (msg == NULL) {
			pr_err("[SSP] %s, failed to alloc memory for ssp_msg\n", __func__);
			return -ENOMEM;
		}
		msg->cmd = MCU_FACTORY;
		msg->length = 5;
		msg->options = AP2HUB_READ;
		msg->buffer = buffer;
		msg->free_buffer = 0;

		memset(msg->buffer, 0, 5);

		iRet = ssp_spi_async(data, msg);

	} else {
		pr_err("[SSP]: %s - invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	ssp_dbg("[SSP]: MCU Factory Test Start! - %d\n", iRet);

	return size;
}

ssize_t mcu_factorytest_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	bool bMcuTestSuccessed = false;
	struct ssp_data *data = dev_get_drvdata(dev);

	if (data->bSspShutdown == true) {
		ssp_dbg("[SSP]: %s - MCU Bin is crashed\n", __func__);
		return sprintf(buf, "NG,NG,NG\n");
	}

	ssp_dbg("[SSP] MCU Factory Test Data : %u, %u, %u, %u, %u\n", buffer[0],
			buffer[1], buffer[2], buffer[3], buffer[4]);

		/* system clock, RTC, I2C Master, I2C Slave, externel pin */
	if ((buffer[0] == SUCCESS)
			&& (buffer[1] == SUCCESS)
			&& (buffer[2] == SUCCESS)
			&& (buffer[3] == SUCCESS)
			&& (buffer[4] == SUCCESS))
		bMcuTestSuccessed = true;

	ssp_dbg("[SSP]: MCU Factory Test Result - %s, %s, %s\n", MODEL_NAME,
		(bMcuTestSuccessed ? "OK" : "NG"), "OK");

	return sprintf(buf, "%s,%s,%s\n", MODEL_NAME,
		(bMcuTestSuccessed ? "OK" : "NG"), "OK");
}

ssize_t mcu_sleep_factorytest_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	int iRet = 0;
	struct ssp_msg *msg;

	if (sysfs_streq(buf, "1")) {
		msg = kzalloc(sizeof(*msg), GFP_KERNEL);
		if (msg == NULL) {
			pr_err("[SSP] %s, failed to alloc memory for ssp_msg\n", __func__);
			return -ENOMEM;
		}
		msg->cmd = MCU_SLEEP_FACTORY;
		msg->length = FACTORY_DATA_MAX;
		msg->options = AP2HUB_READ;
		msg->buffer = buffer;
		msg->free_buffer = 0;

		iRet = ssp_spi_async(data, msg);

	} else {
		pr_err("[SSP]: %s - invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	ssp_dbg("[SSP]: MCU Sleep Factory Test Start! - %d\n", iRet);

	return size;
}

ssize_t mcu_sleep_factorytest_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int iDataIdx, iSensorData = 0;
	struct ssp_data *data = dev_get_drvdata(dev);
	struct sensor_value *fsb;
	u16 chLength = 0;
	int ret = 0;
	
	fsb = kzalloc(sizeof(struct sensor_value)*SENSOR_MAX, GFP_KERNEL);

	memcpy(&chLength, buffer, 2);
	memset(fsb, 0, sizeof(struct sensor_value) * SENSOR_MAX);

	for (iDataIdx = 2; iDataIdx < chLength + 2;) {
		iSensorData = (int)buffer[iDataIdx++];

		if ((iSensorData < 0) ||
			(iSensorData >= (SENSOR_MAX - 1))) {
			pr_err("[SSP]: %s - Mcu data frame error %d\n",
				__func__, iSensorData);
			goto exit;
		}

		data->get_sensor_data[iSensorData]((char *)buffer,
			&iDataIdx, &(fsb[iSensorData]));
	}

exit:
	ssp_dbg("[SSP]: %s Result\n"
		"accel %d,%d,%d\n"
		"gyro %d,%d,%d\n"
#ifdef CONFIG_SENSORS_SSP_ADPD142
		"hrm_raw %d,%d\n"
		"hrm_lib %d,%d,%d\n"
#endif
		, __func__,
		fsb[ACCELEROMETER_SENSOR].x, fsb[ACCELEROMETER_SENSOR].y,
		fsb[ACCELEROMETER_SENSOR].z, fsb[GYROSCOPE_SENSOR].x,
		fsb[GYROSCOPE_SENSOR].y, fsb[GYROSCOPE_SENSOR].z
#ifdef CONFIG_SENSORS_SSP_ADPD142
		, fsb[BIO_HRM_RAW].ch_a, fsb[BIO_HRM_RAW].ch_b
		, fsb[BIO_HRM_LIB].hr, fsb[BIO_HRM_LIB].rri, fsb[BIO_HRM_RAW].snr
#endif
		);

	ret = sprintf(buf, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%u,"
		"%u,%u,%u,%u,%d,%d,%d,%d,%d,%d"
#ifdef CONFIG_SENSORS_SSP_ADPD142
		",%d, %d, %d, %d, %d"
#endif
		"\n",
		fsb[ACCELEROMETER_SENSOR].x, fsb[ACCELEROMETER_SENSOR].y,
		fsb[ACCELEROMETER_SENSOR].z, fsb[GYROSCOPE_SENSOR].x,
		fsb[GYROSCOPE_SENSOR].y, fsb[GYROSCOPE_SENSOR].z,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
#ifdef CONFIG_SENSORS_SSP_ADPD142
		, fsb[BIO_HRM_RAW].ch_a, fsb[BIO_HRM_RAW].ch_b
		, fsb[BIO_HRM_LIB].hr, fsb[BIO_HRM_LIB].rri, fsb[BIO_HRM_RAW].snr
#endif
		);

	kfree(fsb);

	return ret;
}

int ssp_charging_motion(struct ssp_data *data, int iEnable)
{
	u8 uBuf[2] = {0, 0};

	if (iEnable == 1) {
		send_instruction(data, ADD_LIBRARY,
			SMART_ALERT_MOTION, uBuf, 2);
	} else {
		send_instruction(data, REMOVE_LIBRARY,
			SMART_ALERT_MOTION, uBuf, 2);
	}

	return 0;
}

#if 0
void report_key_event(struct ssp_data *data)
{
	input_event(data->key_input_dev, EV_KEY, KEY_HOMEPAGE, 1);
	input_sync(data->key_input_dev);
	ssp_charging_motion(data, 0);

	msleep(10);

	input_event(data->key_input_dev, EV_KEY, KEY_HOMEPAGE, 0);
	input_sync(data->key_input_dev);
	ssp_charging_motion(data, 1);

	wake_lock_timeout(&data->ssp_wake_lock, 3 * HZ);
}
#endif

int ssp_parse_motion(struct ssp_data *data, char *dataframe, int start, int end)
{
	int length = end - start;
	char *buf = dataframe + start;;

	if (length != 4)
		return FAIL;

	if ((buf[0] == 1) && (buf[1] == 1) && (buf[2] == SMART_ALERT_MOTION)) {
		ssp_dbg("[SSP]: %s - LP MODE WAKEUP\n", __func__);
		queue_work(data->lpm_motion_wq, &data->work_lpm_motion);
		//report_key_event(data);
		return SUCCESS;
	}

	return FAIL;
}

static void lpm_motion_work_func(struct work_struct *work)
{
	struct ssp_data *data =
		container_of(work, struct ssp_data, work_lpm_motion);

	input_event(data->key_input_dev, EV_KEY, KEY_HOMEPAGE, 1);
	input_sync(data->key_input_dev);
	ssp_charging_motion(data, 0);

	msleep(10);

	input_event(data->key_input_dev, EV_KEY, KEY_HOMEPAGE, 0);
	input_sync(data->key_input_dev);
	ssp_charging_motion(data, 1);

	wake_lock_timeout(&data->ssp_wake_lock, 3 * HZ);

}

int intialize_lpm_motion(struct ssp_data *data)
{
	data->lpm_motion_wq = create_singlethread_workqueue("ssp_lpm_motion_wq");
	if (!data->lpm_motion_wq)
		return ERROR;

	INIT_WORK(&data->work_lpm_motion, lpm_motion_work_func);
	return SUCCESS;
}

