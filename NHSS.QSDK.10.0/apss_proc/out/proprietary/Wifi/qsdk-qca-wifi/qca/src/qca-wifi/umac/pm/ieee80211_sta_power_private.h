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


/*
* 802.11 station sleep state machine implementation.
*/

#ifndef _IEEE80211_STA_SLEEP_PRIVATE_H_
#define _IEEE80211_STA_SLEEP_PRIVATE_H_


typedef _ieee80211_popwer_sta_smps { 
    u_int32_t               ips_smDynamicPwrSaveEnable;/* Dynamic MIMO power save enabled/disabled */
    IEEE80211_SM_PWRSAVE_STATE  ips_smPowerSaveState;  /* Current dynamic MIMO power save state */
    u_int16_t               ips_smpsDataHistory[IEEE80211_SMPS_DATAHIST_NUM]; /* Throughput history buffer used for enabling MIMO ps */
    u_int8_t                ips_smpsCurrDataIndex;     /* Index in throughput history buffer to be updated */
} ieee80211_popwer_sta_smps;

#endif
