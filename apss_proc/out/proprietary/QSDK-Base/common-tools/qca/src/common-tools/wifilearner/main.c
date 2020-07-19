/*
 * Copyright (c) 2018 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */


#include "wifilearner.h"
#include "nl_utils.h"
#include "utils.h"
#include "hidl.h"

int main(int argc, char *argv[])
{
	struct wifilearner_ctx wlc;
	int status = 0;

	wlc.stdout_debug = WL_MSG_INFO;
	if (nl80211_init(&wlc)) {
		wl_print(&wlc, WL_MSG_ERROR, "netlink initialization failed.");
		return -1;
	}

	if (wifilearner_hidl_process(&wlc)) {
		wl_print(&wlc, WL_MSG_ERROR, "hidl initialization failed.");
		status = -1;
		goto out;
	}

out:
	nl80211_deinit(&wlc);
	return status;
}
