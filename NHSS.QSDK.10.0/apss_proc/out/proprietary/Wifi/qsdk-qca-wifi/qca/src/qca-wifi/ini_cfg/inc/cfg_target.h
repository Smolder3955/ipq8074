/*
 *
 * Copyright (c) 2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 */

#ifndef _CFG_TGT_TYPE_H_
#define _CFG_TGT_TYPE_H_

#include "cfg_define.h"

#define CFG_TGT_MPDU_DENSITY \
	CFG_INI_UINT("mpdu_density", \
	0, 9, 6, \
	CFG_VALUE_OR_DEFAULT, "mpdu density value")

#define CFG_TGT_VHT_AMSDU \
	CFG_INI_UINT("vht_amsdu", \
	0, 4, 3, \
	CFG_VALUE_OR_DEFAULT, "VHT AMSDU value")

#define CFG_TGT_MULTIQ_SUPPORT \
	CFG_INI_BOOL("multiq_support_enabled", \
	false, "multi queue support enabled")

#define CFG_TGT_DBDC_TIME_DELAY \
	CFG_INI_UINT("delay_stavap_connection", \
	0, 10, 0, \
	CFG_VALUE_OR_DEFAULT, "DBDC time delay for stavap connection")

#define CFG_TGT_NAC_MAX_BSSID \
	CFG_INI_UINT("nac_max_bssid", \
	0, 8, 3, \
	CFG_VALUE_OR_DEFAULT, "NAC max BSSID")

#define CFG_TGT_NAC_MAX_CLIENT \
	CFG_INI_UINT("nac_max_client", \
	0, 24, 8, \
	CFG_VALUE_OR_DEFAULT, "NAC max client")

#define CFG_TGT_NUM_CLIENT \
	CFG_INI_UINT("num_clients", \
	0, 512, 512, \
	CFG_VALUE_OR_DEFAULT, "number of  clients")

#define CFG_TGT_DYN_GROUPING_SUPPORT \
	CFG_INI_BOOL("dynamic_grouping_support", \
	false, "if dynamic grouping support is enabled")

#define CFG_TGT_DPD_SUPPORT \
	CFG_INI_BOOL("dpd_support", \
	false, "if dpd support is enabled")

#define CFG_TGT_AGGR_BURST_SUPPORT \
	CFG_INI_BOOL("aggr_burst_support", \
	false, "if aggr burst support is enabled")

#define CFG_TGT_QBOOST_SUPPORT \
	CFG_INI_BOOL("qboost_support", \
	false, "if qboost support is enabled")

#define CFG_TGT_SIFS_FRAME_SUPPORT \
	CFG_INI_BOOL("sifs_frame_support", \
	false, "if sifs frame support is enabled")

#define CFG_TGT_BLK_INTERBSS_SUPPORT \
	CFG_INI_BOOL("block_interbss_support", \
	false, "if block interbss support is enabled")

#define CFG_TGT_DIS_RESET_SUPPORT \
	CFG_INI_BOOL("disable_reset_support", \
	false, "if disable reset support is enabled")

#define CFG_TGT_MSDU_TTL_SUPPORT \
	CFG_INI_BOOL("msdu_ttl_support", \
	false, "if msdu ttl support is enabled")

#define CFG_TGT_PPDU_DUR_SUPPORT \
	CFG_INI_BOOL("ppdu_duration_support", \
	false, "if ppdu duration support is enabled")

#define CFG_TGT_BURST_MODE_SUPPORT \
	CFG_INI_BOOL("burst_mode_support", \
	false, "if burst mode support is enabled")

#define CFG_TGT_WDS_SUPPORT \
	CFG_INI_BOOL("wds_support", \
	false, "if WDS support is enabled")

#define CFG_TGT_NO_BFEE_LIMIT_SUPPORT \
	CFG_INI_BOOL("no_bfee_limit", \
	false, "if no bfee limit support is enabled")

#define CFG_TGT_HE_TGT_SUPPORT \
	CFG_INI_BOOL("he_target", \
	false, "if HE target support is enabled")

#define CFG_TARGET \
	CFG(CFG_TGT_MPDU_DENSITY) \
	CFG(CFG_TGT_VHT_AMSDU) \
	CFG(CFG_TGT_MULTIQ_SUPPORT) \
	CFG(CFG_TGT_DBDC_TIME_DELAY) \
	CFG(CFG_TGT_NAC_MAX_BSSID) \
	CFG(CFG_TGT_NAC_MAX_CLIENT) \
	CFG(CFG_TGT_NUM_CLIENT) \
	CFG(CFG_TGT_DYN_GROUPING_SUPPORT) \
	CFG(CFG_TGT_DPD_SUPPORT) \
	CFG(CFG_TGT_AGGR_BURST_SUPPORT) \
	CFG(CFG_TGT_QBOOST_SUPPORT) \
	CFG(CFG_TGT_SIFS_FRAME_SUPPORT) \
	CFG(CFG_TGT_BLK_INTERBSS_SUPPORT) \
	CFG(CFG_TGT_DIS_RESET_SUPPORT) \
	CFG(CFG_TGT_MSDU_TTL_SUPPORT) \
	CFG(CFG_TGT_PPDU_DUR_SUPPORT) \
	CFG(CFG_TGT_BURST_MODE_SUPPORT) \
	CFG(CFG_TGT_WDS_SUPPORT) \
	CFG(CFG_TGT_NO_BFEE_LIMIT_SUPPORT) \
	CFG(CFG_TGT_HE_TGT_SUPPORT) \

#endif
