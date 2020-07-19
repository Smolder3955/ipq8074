/*
 * Copyright (c) 2018 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#ifndef NL_UTILS_H
#define NL_UTILS_H

#include <errno.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include <netlink/genl/genl.h>
#include "qca-vendor_copy.h"
#include "nl80211_copy.h"
#include "wifilearner.h"

#ifndef BIT
#define BIT(x) (1U << (x))
#endif

#define NL_STATS_HAS_RX_RATE BIT(0)
#define NL_STATS_HAS_TX_RATE BIT(1)

struct station_info {
	uint64_t filled;
	uint32_t current_rx_rate;
	uint32_t current_tx_rate;
};

#define NL_SURVEY_HAS_NOISE BIT(0)
#define NL_SURVEY_HAS_CHAN_TIME BIT(1)
#define NL_SURVEY_HAS_CHAN_TIME_BUSY BIT(2)
#define NL_SURVEY_HAS_CHAN_TIME_EXT_BUSY BIT(3)
#define NL_SURVEY_HAS_CHAN_TIME_RX BIT(4)
#define NL_SURVEY_HAS_CHAN_TIME_TX BIT(5)
#define NL_SURVEY_HAS_CHAN_TIME_SCAN BIT(6)

#define MAX_SURVEY_CHANNELS 50

struct survey_info {
	uint32_t filled[MAX_SURVEY_CHANNELS];
	uint32_t freq[MAX_SURVEY_CHANNELS];
	int8_t noise[MAX_SURVEY_CHANNELS];
	uint8_t in_use[MAX_SURVEY_CHANNELS];
	uint64_t time[MAX_SURVEY_CHANNELS];
	uint64_t time_busy[MAX_SURVEY_CHANNELS];
	uint64_t time_ext_busy[MAX_SURVEY_CHANNELS];
	uint64_t time_rx[MAX_SURVEY_CHANNELS];
	uint64_t time_tx[MAX_SURVEY_CHANNELS];
	uint64_t time_scan[MAX_SURVEY_CHANNELS];
	uint32_t count;
};

struct iface_info {
	const char *name;
	uint32_t index;
	uint32_t found;
};


int nl80211_init(struct wifilearner_ctx *wlc);
void nl80211_deinit(struct wifilearner_ctx *wlc);
int get_iface_stats(struct wifilearner_ctx *wlc, const char *ifname,
		    const unsigned char *peer_mac,
		    struct station_info *sta_data);
int get_survey_dump_results(struct wifilearner_ctx *wlc, const char *ifname,
			    struct survey_info *survey_data);

#endif  //NL_UTILS_H
