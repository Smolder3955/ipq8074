/*
 * Copyright (c) 2011, 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
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

#ifndef _IEEE80211_TARGET_H_
#define _IEEE80211_TARGET_H_

#define IEEE80211_CHK_VAP_TARGET(_ic)                   0
#define IEEE80211_CHK_NODE_TARGET(_ic)                  0

#define IEEE80211_ADD_VAP_TARGET(vap)
#define IEEE80211_ADD_NODE_TARGET(ni, vap, is_vap_node)
#define IEEE80211_DELETE_VAP_TARGET(vap)
#define IEEE80211_DELETE_NODE_TARGET(ni, ic, vap, is_reset_bss)
#define IEEE80211_UPDATE_TARGET_IC(ni)
#define ieee80211_update_vap_target(_vap)                 

#endif /* #ifndef _IEEE80211_TARGET_H_ */
