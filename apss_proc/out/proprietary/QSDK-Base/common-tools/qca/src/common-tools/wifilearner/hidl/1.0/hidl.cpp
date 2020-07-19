/*
 * Copyright (c) 2018 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include <android-base/logging.h>
#include <hidl/HidlTransportSupport.h>

#include "wifistats.h"

extern "C"
{
#include "hidl.h"
#include "wifilearner.h"
}

using android::hardware::configureRpcThreadpool;
using android::hardware::joinRpcThreadpool;
using vendor::qti::hardware::wifi::wifilearner::V1_0::IWifiStats;
using vendor::qti::hardware::wifi::wifilearner::V1_0::implementation::WifiStats;

/*
 * Function     : wifilearner_hidl_process
 * Description  : registers as server instance for IWifiStats service
 * Input params : pointer to wifilearner_ctx
 * Return       : SUCCESS/FAILURE
 *
 */
int wifilearner_hidl_process(struct wifilearner_ctx *wlc)
{
	android::base::InitLogging(NULL,
			android::base::LogdLogger(android::base::SYSTEM));
	LOG(INFO) << "Wifi Learner Hal is booting up...";

	android::sp<IWifiStats> service_wifistats = new WifiStats(wlc);
	configureRpcThreadpool(1, true /* callerWillJoin */);
	if (service_wifistats->registerAsService("wifiStats") != android::OK) {
		LOG(ERROR) << "Cannot register WifiStats HAL service";
		return 1;
	}

	joinRpcThreadpool();
	LOG(INFO) << "wifilearner is terminating...";

	return 0;
}
