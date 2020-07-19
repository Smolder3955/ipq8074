/*===========================================================================
  mpctl-test-app.c

  DESCRIPTION
  Test App to check thermal client socket functionality.

  Copyright (c) 2013 Qualcomm Technologies, Inc.  All Rights Reserved.
  Qualcomm Technologies Proprietary and Confidential.

===========================================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <dlfcn.h>

int main(int argc, char **argv)
{
	int ret = 0;
	int rc = 0;
	char *error;
	int i = 5;
	int value = 0;
	void *handle = NULL;
	int (*request)(char *, int);

	handle = dlopen("/vendor/lib/libthermalclient.so", RTLD_NOW);
	if (!handle) {
		fputs (dlerror(), stderr);
		return -1;
	}

	request = dlsym(handle, "thermal_client_request");
	error = (char *)dlerror();
	if (error != NULL) {
		fputs(error, stderr);
		return -1;
	}

	while(i) {
		printf("mpctl: Request %d, value %d\n", i, value);
		rc = request("override", value);
		sleep(10);
		value = (value)?(0):(1);
		i--;
	}

	printf("test app shutting down ......\n");
	dlclose(handle);
	return ret;
}
