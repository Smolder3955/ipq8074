/*
* Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

/*
* P2P state machine implementation.
*/

#ifndef _IEEE80211_P2P_SM_H_
#define _IEEE80211_P2P_SM_H_

typedef enum {
    IEEE80211_P2P_STATE_SCAN,                   /* scan state */  
    IEEE80211_P2P_STATE_SEARCH,                 /* search state */
    IEEE80211_P2P_STATE_LISTEN,                 /* listen state */
    IEEE80211_P2P_STATE_GO_NEGO_LISTEN_CHANNEL, /* GO negotiation state on listen channel */
    IEEE80211_P2P_STATE_GO_NEGO_PEER_CHANNEL,   /* GO negotiation state */
} ieee80211_p2p_state; 


typedef enum {
    IEEE80211_P2P_EVENT_SCAN_END,                /* end of scan event from scanner */  
    IEEE80211_P2P_EVENT_CHANNEL_TIMEOUT,         /* end of channel time in SEARCH state  */
    IEEE80211_P2P_LISTEN_TIMEOUT,                /* end of listen time */
    IEEE80211_P2P_FOUND_MATCHING_ENTRY,          /* found matching entry */
} ieee80211_p2p_event; 


#endif

