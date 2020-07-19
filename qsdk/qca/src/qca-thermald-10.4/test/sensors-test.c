/*===========================================================================
  sensors-test.c

  DESCRIPTION
  Test sensor setup.

  ---------------------------------------------------------------------------
  Copyright (c) 2011 Qualcomm Technologies, Inc. All Rights Reserved.
  Qualcomm Technologies Proprietary and Confidential.
  ---------------------------------------------------------------------------

===========================================================================*/

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "thermal.h"

char *sensor_names[] =
{
	"test0",
};

extern int test_sensors_setup(sensor_setting_t *settings, sensor_t *sensor);
extern void test_sensors_shutdown(sensor_setting_t *setting);
extern int test_sensor_get_temperature(sensor_setting_t *setting);

static sensor_t g_sensors[] = {
	{
		.name = "test0",
		.setup = test_sensors_setup,
		.shutdown = test_sensors_shutdown,
		.get_temperature = test_sensor_get_temperature,
		.setting = NULL,
		.tzn = 0,
		.data = NULL,
	},
};

int sensors_setup(thermal_setting_t *settings)
{
	int i = 0;
	int j = 0;
	int sensor_count = 0;

	if (!settings)
		return sensor_count;

	for (i = 0; i < ARRAY_SIZE(g_sensors); i++) {
		for (j = 0; j < SENSOR_IDX_MAX; j++) {
			if (settings->sensors[j].desc && !strcmp(settings->sensors[j].desc, g_sensors[i].name)) {
				info("Sensor setup:[%s]\n", g_sensors[i].name);
				g_sensors[i].setting = &settings->sensors[j];
				sensor_count += g_sensors[i].setup(&settings->sensors[j], (sensor_t *)&g_sensors[i]);
				break;
			}
		}
	}

	return sensor_count;
}

void sensors_shutdown()
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(g_sensors); i++) {
		info("Sensor shutdown:[%s] \n", g_sensors[i].name);
		g_sensors[i].shutdown(g_sensors[i].setting);
	}
}

int sensor_get_temperature(sensor_setting_t *setting)
{
	int i;
	int temp = 0;

	if (!setting)
		return -EFAULT;

	for (i = 0; i < ARRAY_SIZE(g_sensors); i++) {
		if (setting == g_sensors[i].setting) {
			temp = g_sensors[i].get_temperature(setting);
			break;
		}
	}

	dbgmsg("Sensor[%s] Temperature : %2.1f\n", setting->desc, RCONV(temp));

	return temp;
}

void sensor_wait(sensor_setting_t *setting)
{
	static int is_first_poll = 1;

	if (setting == NULL) {
		msg("%s: Unexpected NULL", __func__);
		return;
	}

	if (!is_first_poll) {
		usleep(setting->sampling_period_us);
	} else {
		is_first_poll = 0;
	}
}

void sensor_update_thresholds(sensor_setting_t *setting,
				int threshold_triggered, int level)
{
	return;
}
