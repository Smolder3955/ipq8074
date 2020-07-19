/*
 * Copyright (c) 2018 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#ifndef WIFISTATS_HIDL_H
#define WIFISTATS_HIDL_H

#include <string>

#include <android-base/macros.h>

#include <vendor/qti/hardware/wifi/wifilearner/1.0/IWifiStats.h>

extern "C"
{
#include "wifilearner.h"
#include "nl_utils.h"
}

namespace vendor {
namespace qti {
namespace hardware {
namespace wifi {
namespace wifilearner {
namespace V1_0 {
namespace implementation {

using namespace android::hardware;
using namespace vendor::qti::hardware::wifi::wifilearner::V1_0;

class WifiStats : public V1_0::IWifiStats
{
public:
	WifiStats(struct wifilearner_ctx *wlc);
	~WifiStats() override = default;

	// Hidl methods exposed.
	Return<void> getPeerStats(const hidl_string& iface_name,
				  const hidl_array<uint8_t, 6>& peer_mac,
				  getPeerStats_cb _hidl_cb) override;

	Return<void> getSurveyDumpResults(const hidl_string& iface_name,
					  getSurveyDumpResults_cb _hidl_cb)
					  override;

private:
	// Corresponding worker functions for the HIDL methods.
	std::pair<WifiLearnerStatus, IfaceStats>
	getPeerStatsInternal(const std::string& iface_name,
			     const std::array<uint8_t, 6>& peer_mac);

	std::pair<WifiLearnerStatus, hidl_vec<ChannelSurveyInfo>>
	getSurveyDumpResultsInternal(const std::string& iface_name);

	struct wifilearner_ctx* wlc_;
	DISALLOW_COPY_AND_ASSIGN(WifiStats);
};
}  // namespace implementation
}  // namespace V1_0
}  // namespace wifilearner
}  // namespace wifi
}  // namespace hardware
}  // namespace qti
}  // namespace vendor

#endif  // WIFISTATS_HIDL_H
