/*===========================================================================
  test-sensor.c

  DESCRIPTION
  Test sensor device. This exposes a file in TZ_TEMP filesystem path which
  should be populated with a temperature in Celsius for testing.

  ---------------------------------------------------------------------------
  Copyright (c) 2011 Qualcomm Technologies, Inc. All Rights Reserved.
  Qualcomm Technologies Proprietary and Confidential.
  ---------------------------------------------------------------------------

===========================================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "thermal.h"

#define TEST_TEMP "/data/local/tmp/test%dtemp"

int test_sensors_setup(sensor_setting_t *setting, sensor_t *sensor)
{
	int test_fd = -1;
	int sensor_count = 0;
	char name[MAX_PATH] = {0};

	snprintf(name, MAX_PATH, TEST_TEMP, sensor->tzn);

	/* create initial sensor test file if does not exist */
	test_fd = open(name, O_RDONLY|O_CREAT);
	if (test_fd > 0) {
		setting->disabled = 0;
		setting->chan_idx = test_fd;
		sensor_count++;
	} else {
		msg("test: Error opening %s\n", name);
	}

	return sensor_count;
}

void test_sensors_shutdown(sensor_setting_t *setting)
{
	if (setting->chan_idx > 0)
		close(setting->chan_idx);
}

int test_sensor_get_temperature(sensor_setting_t *setting)
{
	static char buf[10];
	int temp = 0;

	if (read(setting->chan_idx, buf, sizeof(buf)) != -1) {
		temp = atoi(buf);
	}
	lseek(setting->chan_idx, 0, SEEK_SET);

	return CONV(temp);
}
