/*
 * Copyright (c) 2018 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#ifndef WIFILEARNER_H
#define WIFILEARNER_H

#include <stdint.h>

#ifndef ETH_ALEN
#define ETH_ALEN 6
#endif

struct nl80211_ctx {
	struct nl_sock *sock;
	int32_t netlink_familyid;
	int32_t nlctrl_familyid;
	size_t sock_buf_size;
};

struct wifilearner_ctx {
	int stdout_debug;
	struct nl80211_ctx *nl_ctx;
};

#endif //WIFILEARNER_H
