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


#ifndef IEEE80211_CRYPTO_PMF_PRIV_H
#define IEEE80211_CRYPTO_PMF_PRIV_H

#ifndef WLAN_CONV_CRYPTO_SUPPORTED

#include <osdep.h>
#include <ieee80211_var.h>
#include "rijndael.h"

#define AES_BLOCK_SIZE 16
#define AES_CMAC_KEY_LEN 16
#define CMAC_TLEN 8 /* CMAC TLen = 64 bits (8 octets) */
#define AAD_LEN 20
#endif
#endif /* IEEE80211_CRYPTO_PMF_PRIV_H */
