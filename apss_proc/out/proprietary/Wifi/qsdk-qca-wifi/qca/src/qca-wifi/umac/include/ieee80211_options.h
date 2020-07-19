/*
* Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/


/*
 *  Copyright (c) 2008 Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */
#include <osdep.h>

#ifndef IEEE80211_OPTS_H
#define IEEE80211_OPTS_H

#include <wlan_opts.h>

/*
 * default definitions for optional module compiletime inclusion flags.
 */

/*
 * support Station mode 802.11 powersave as well
 * as fake sleep.
 */
#ifndef UMAC_SUPPORT_STA_POWERSAVE
#define UMAC_SUPPORT_STA_POWERSAVE 0
#endif

/*
 * support AP mode powersave. i.e AP supporting
 * stations power save stations. 
 */
#ifndef UMAC_SUPPORT_AP_POWERSAVE
#define UMAC_SUPPORT_AP_POWERSAVE 0
#endif

/*
 * support Station mode 802.11n  spacial mimo powersave
 */
#ifndef UMAC_SUPPORT_STA_SMPS
#define UMAC_SUPPORT_STA_SMPS 0
#endif

/*
 * support CCMP encrypt/decrypt  in software
 */
#ifndef UMAC_SUPPORT_CCMP_SW_CRYPTO
#define UMAC_SUPPORT_CCMP_SW_CRYPTO 0
#endif

/* 
 * support TKIP encrypt/decrypt  in software
 */
#ifndef UMAC_SUPPORT_TKIP_SW_CRYPTO
#define UMAC_SUPPORT_TKIP_SW_CRYPTO 0
#endif

/* 
 * support rijndael encryption  in software.
 */
#ifndef UMAC_SUPPORT_RIJNDAEL
#define UMAC_SUPPORT_RIJNDAEL 0
#endif

/*
 * resource manager .
 */
#ifndef UMAC_SUPPORT_RESMGR
#define UMAC_SUPPORT_RESMGR 0
#endif

#if UMAC_SUPPORT_RESMGR
#ifndef UMAC_SUPPORT_VAP_PAUSE
#define UMAC_SUPPORT_VAP_PAUSE 1
#endif
#endif

#ifndef UMAC_SUPPORT_VAP_PAUSE
#define UMAC_SUPPORT_VAP_PAUSE 0
#endif

/*
 * resource manager State machine.
 * it is invalid to set UMAC_SUPPORT_RESMGR and  
 */
#if UMAC_SUPPORT_RESMGR 

#ifndef UMAC_SUPPORT_RESMGR_SM
#define UMAC_SUPPORT_RESMGR_SM 0
#endif

#else /* UMAC_SUPPORT_RESMGR */

/* 
 * cannot include resource manager SM without including resource manager.
 */
#if UMAC_SUPPORT_RESMGR_SM
#error option UMAC_SUPPORT_RESMGR_SM  requires UMAC_SUPPORT_RESMGR 
#endif

#endif /* UMAC_SUPPORT_RESMGR */

/*
 * scan module 
 */
#ifndef UMAC_SUPPORT_SCAN
#define UMAC_SUPPORT_SCAN 1
#endif

/*
 * aplist module 
 */
#ifndef UMAC_SUPPORT_APLIST
#define UMAC_SUPPORT_APLIST 1
#endif

/*
 * TX fragmentation support .
 */
#ifndef UMAC_SUPPORT_TX_FRAG
#define UMAC_SUPPORT_TX_FRAG 0
#endif 

/*
 * HT (11n) support .
 */
#ifndef UMAC_SUPPORT_HT
#define UMAC_SUPPORT_HT 1
#endif 

/*
 * IBSS mode support.
 */
#ifndef UMAC_SUPPORT_IBSS
#define UMAC_SUPPORT_IBSS 1
#endif 

#ifndef UMAC_SUPPORT_NAWDS
#define UMAC_SUPPORT_NAWDS 0
#else
#ifndef UMAC_MAX_NAWDS_REPEATER
#define UMAC_MAX_NAWDS_REPEATER 37
#endif
#endif 

/*
 * SW becon miss handling  
 */
#ifndef UMAC_SUPPORT_SW_BMISS
#define UMAC_SUPPORT_SW_BMISS  1
#endif 

/*
 * P2P protocol support.
 */
#ifndef UMAC_SUPPORT_P2P
#define UMAC_SUPPORT_P2P 0
#endif 

/*
 * BT AMP
 */
#ifndef UMAC_SUPPORT_BTAMP
#define UMAC_SUPPORT_BTAMP 0
#endif

/*
 * support TSF timers 
 * Support Repeater Placement Feature
 */
#ifndef UMAC_SUPPORT_TSF_TIMER
#define UMAC_SUPPORT_TSF_TIMER 0
#endif 


#ifndef UMAC_SUPPORT_VAP_TSF_OFFSET
#define UMAC_SUPPORT_VAP_TSF_OFFSET 0
#endif 

/*
 * Set AP as the only supported opmode,
 * this flag is for CPU/memory optimization for 
 * retail/carrier/enterprise platforms.
 * False by default, meaning that all modes are 
 * supported in UMAC.  
 */
#ifndef UMAC_SUPPORT_OPMODE_APONLY
#define UMAC_SUPPORT_OPMODE_APONLY 0
#endif 

#endif 
