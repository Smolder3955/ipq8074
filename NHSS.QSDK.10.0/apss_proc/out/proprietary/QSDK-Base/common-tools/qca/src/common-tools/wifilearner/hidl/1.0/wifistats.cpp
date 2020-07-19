/*
 * Copyright (c) 2018 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */


#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/stringprintf.h>

#include "wifistats.h"
#include "hidl_return_util.h"

extern "C"
{

}

namespace {

using vendor::qti::hardware::wifi::wifilearner::V1_0::IWifiStats;

}  // namespace

namespace vendor {
namespace qti {
namespace hardware {
namespace wifi {
namespace wifilearner {
namespace V1_0 {
namespace implementation {
using hidl_return_util::call;


WifiStats::WifiStats(struct wifilearner_ctx* wlc) : wlc_(wlc)
{}

Return<void> WifiStats::getPeerStats(const hidl_string& iface_name,
				     const hidl_array<uint8_t, 6>& peer_mac,
				     getPeerStats_cb _hidl_cb)
{
	return call(this, &WifiStats::getPeerStatsInternal, _hidl_cb,
		    iface_name, peer_mac);
}

Return<void> WifiStats::getSurveyDumpResults(const hidl_string& iface_name,
					     getSurveyDumpResults_cb _hidl_cb)
{
	return call(this, &WifiStats::getSurveyDumpResultsInternal, _hidl_cb,
		    iface_name);
}

static void copyStats(IfaceStats *stats, station_info *data)
{
	if (data->filled & NL_STATS_HAS_TX_RATE) {
		stats->txRate = data->current_tx_rate;
		stats->filled |= IfaceStatsFilledEnum::HIDL_STATS_HAS_TX_RATE;
	}
	if (data->filled & NL_STATS_HAS_RX_RATE) {
		stats->rxRate = data->current_rx_rate;
		stats->filled |= IfaceStatsFilledEnum::HIDL_STATS_HAS_RX_RATE;
	}
}

std::pair<WifiLearnerStatus, IfaceStats> WifiStats::getPeerStatsInternal(const std::string& iface_name,
        const std::array<uint8_t, 6>& peer_mac)
{
	station_info sta_data;
	IfaceStats peer_stats = {};
	int ret;

	ret = get_iface_stats(wlc_, iface_name.c_str(), peer_mac.data(), &sta_data);
	if (ret) {
		return {{WifiLearnerStatusCode::FAILURE_UNKNOWN, ""}, std::move(peer_stats)};
	}
	copyStats(&peer_stats, &sta_data);

	return {{WifiLearnerStatusCode::SUCCESS, ""}, std::move(peer_stats)};
}

static void copySurveyInfo(ChannelSurveyInfo *info, survey_info *data, uint32_t data_index)
{
	info->freq = data->freq[data_index];

	if (data->filled[data_index] & NL_SURVEY_HAS_CHAN_TIME) {
		info->channelTime = data->time[data_index];
		info->filled |= ChannelSurveyInfoFilledEnum::HIDL_SURVEY_HAS_CHAN_TIME;
	}
	if (data->filled[data_index] & NL_SURVEY_HAS_CHAN_TIME_BUSY) {
		info->channelTimeBusy = data->time_busy[data_index];
		info->filled |=
			ChannelSurveyInfoFilledEnum::HIDL_SURVEY_HAS_CHAN_TIME_BUSY;
	}
}

std::pair<WifiLearnerStatus, hidl_vec<ChannelSurveyInfo>> WifiStats::getSurveyDumpResultsInternal(const std::string& iface_name)
{
	hidl_vec<ChannelSurveyInfo> survey_results;
	survey_info survey_data;
	int ret;

	ret = get_survey_dump_results(wlc_, iface_name.c_str(), &survey_data);
	if (ret)
		return {{WifiLearnerStatusCode::FAILURE_UNKNOWN, ""},
			std::move(survey_results)};

	survey_results.resize(survey_data.count);
	ChannelSurveyInfo defaultSurvey = {};
	std::fill(survey_results.begin(), survey_results.end(), defaultSurvey);
	for(uint32_t i = 0; i < survey_data.count; i++)
		copySurveyInfo(&survey_results[i], &survey_data, i);

	return {{WifiLearnerStatusCode::SUCCESS, ""},
		std::move(survey_results)};
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace wifilearner
}  // namespace wifi
}  // namespace hardware
}  // namespace qti
}  // namespace vendor

