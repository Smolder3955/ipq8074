/*===========================================================================
  camera_camcorder_test_app.c

  DESCRIPTION
  Test App to check camera/camcorder client socket functionality.

  Copyright (c) 2013 Qualcomm Technologies, Inc.  All Rights Reserved.
  Qualcomm Technologies Proprietary and Confidential.

===========================================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>
#include <fcntl.h>
#include <time.h>
#include <getopt.h>
#include <linux/types.h>
#include <dlfcn.h>

int thermal_callback_camera(int value, void *user_data, void *reserved_data)
{
	int *data;
	data = (int *)user_data;
	printf("camera callback: action level %d user_data %d\n", value, *data);
	return 0;
}

int thermal_callback_camcorder(int value, void *user_data, void *reserved_data)
{
	int *data;
	data = (int *)user_data;
	printf("camcorder callback: action level %d user_data %d\n", value, *data);
	return 0;
}

int main(int argc, char **argv)
{
	int ret = 0;
	int handle_camera;
	int handle_camcorder;
	char *error;
	void *handle = NULL;
	int user_data = 500;
	int (*thermal_reg)(char *, void *, void *);
	void (*thermal_unreg)(int);
	int (*callback_camera)(int, void *, void *);
	int (*callback_camcorder)(int, void *, void *);

	callback_camera = thermal_callback_camera;
	callback_camcorder = thermal_callback_camcorder;

	handle = dlopen("/vendor/lib/libthermalclient.so", RTLD_NOW);
	if (!handle) {
		fputs (dlerror(), stderr);
		return -1;
	}

	thermal_reg = dlsym(handle, "thermal_client_register_callback");
	error = (char *)dlerror();
	if (error != NULL) {
		fputs(error, stderr);
		return -1;
	}

	handle_camera = thermal_reg("camera", callback_camera, &user_data);
	if (handle_camera == 0) {
		printf("registration error for camera\n");
		return -1;
	}
	printf("registration success for camera\n");

	handle_camcorder = thermal_reg("camcorder", callback_camcorder, &user_data);
	if (handle_camcorder == 0) {
		printf("registration error for camcorder\n");
		return -1;
	}
	printf("registration success for camcorder\n");

	sleep(300);

	thermal_unreg = dlsym(handle, "thermal_client_unregister_callback");
	error = (char *)dlerror();
	if (error != NULL) {
		fputs(error, stderr);
		return -1;
	}
	thermal_unreg(handle_camcorder);
	thermal_unreg(handle_camera);

	printf("test app shutting down ......\n");
	dlclose(handle);
	return ret;
}
