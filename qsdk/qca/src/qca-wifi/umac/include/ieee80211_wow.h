/*
* Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 * Copyright (c) 2010 Atheros Communications Inc.  All rights reserved. 
 * 
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */


#ifndef _IEEE80211_WOW_H
#define _IEEE80211_WOW_H

#include <osdep.h>
#include <ath_dev.h>
#include <ieee80211_defines.h>


#define IEEE80211_WOW_MAGIC_PKTLEN      102     /* total magic pattern size */
#define IEEE80211_WOW_MAGIC_DUPLEN      96      /* the length of 16 duplications of the IEEE address */
#define IEEE80211_WOW_MAGIC_DUPCNT      16      /* the duplication counts */
#define IEEE80211_WOW_KEEPALIVE_TIMER   60000   /* 1 min */


void ieee80211_wow_magic_parser(struct ieee80211_node *ni, wbuf_t wbuf);
int wlan_get_wow(wlan_if_t vap);
int wlan_set_wow(wlan_if_t vap, uint32_t value);

#endif /* WOW_H */
