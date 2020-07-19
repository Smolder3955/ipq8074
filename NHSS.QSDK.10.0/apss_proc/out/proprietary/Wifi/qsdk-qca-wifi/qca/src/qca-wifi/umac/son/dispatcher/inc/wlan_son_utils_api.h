/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*/
#include "wlan_son_types.h"

u_int16_t son_SNRToPhyRateTablePerformLookup(
	u_int8_t snr, u_int8_t nss,
	wlan_phymode_e phyMode,
	wlan_chwidth_e chwidth);
