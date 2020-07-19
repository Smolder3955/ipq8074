/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */

/*
 * Copyright (c) 2011, 2014-2015 The Linux Foundation. All rights reserved.
 */
#ifndef _DBGLOG_COMMON_H_
#define _DBGLOG_COMMON_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "dbglog_id.h"
#include "dbglog.h"

enum cnss_diag_type {
	DIAG_TYPE_FW_EVENT,           /* send fw event- to diag */
	DIAG_TYPE_FW_LOG,             /* send log event- to diag */
	DIAG_TYPE_FW_DEBUG_MSG,       /* send dbg message- to diag */
	DIAG_TYPE_INIT_REQ,           /* cnss_diag initialization- from diag */
	DIAG_TYPE_FW_MSG,             /* fw msg command-to diag */
	DIAG_TYPE_HOST_MSG,           /* host command-to diag */
	DIAG_TYPE_CRASH_INJECT,       /*crash inject-from diag */
	DIAG_TYPE_DBG_LEVEL,          /* DBG LEVEL-from diag */
};

enum wlan_diag_config_type {
	DIAG_VERSION_INFO,
};

enum wlan_diag_frame_type {
	WLAN_DIAG_TYPE_CONFIG,
	WLAN_DIAG_TYPE_EVENT,
	WLAN_DIAG_TYPE_LOG,
	WLAN_DIAG_TYPE_MSG,
	WLAN_DIAG_TYPE_LEGACY_MSG,
};

struct dbglog_slot {
	unsigned int diag_type;
	unsigned int timestamp;
	unsigned int length;
	unsigned int dropped;
	/* max ATH6KL_FWLOG_PAYLOAD_SIZE bytes */
	uint8_t payload[0];
} __packed;

#endif /* _DBGLOG_COMMON_H_ */
