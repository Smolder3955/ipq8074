/*
* Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 * Copyright (c) 2010, Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */

#if ATH_SUPPORT_WAPI
#ifndef __CRYPTO_WPI_SMS4__H
#define __CRYPTO_WPI_SMS4__H

#include "osdep.h"
#include "ieee80211_var.h"

#ifndef WLAN_CONV_CRYPTO_SUPPORTED
#define SMS4_BLOCK_LEN		16

struct sms4_ctx {
    struct ieee80211vap *sms4c_vap;	/* for diagnostics + statistics */
    struct ieee80211com *sms4c_ic;
};
#endif
#endif /*__CRYPTO_WPI_SMS4__H*/
#endif /*ATH_SUPPORT_WAPI*/
