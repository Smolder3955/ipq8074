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
 * P2P client API.
 */

#ifndef IEEE80211_P2P_CLIENT_H
#define IEEE80211_P2P_CLIENT_H

typedef enum {
    IEEE80211_P2P_CLIENT_EVENT_BEACON=0x1,           /* received beacon from GO */ 
    IEEE80211_P2P_CLIENT_EVENT_PROBE_RESP=0x2,       /* received probe resp from GO */ 
    IEEE80211_P2P_CLIENT_EVENT_UPDATE_CTWINDOW=0x4,  /* ctwindow changed  */ 
    IEEE80211_P2P_CLIENT_EVENT_UPDATE_OPPPS=0x8,     /* GO oppPS tate changed  */ 
    IEEE80211_P2P_CLIENT_EVENT_BEACON_BY_SSID=0x10,      /* received beacon from GO, filtered by SSID and even before VAP is active */ 
    IEEE80211_P2P_CLIENT_EVENT_PROBE_RESP_BY_SSID=0x20,  /* received probe resp from GO, filtered by SSID and even before VAP is active */ 
} ieee80211_p2p_client_event_type;

typedef enum {
    IEEE80211_P2P_CLIENT_IE_BIT_NOT_PRESENT,
    IEEE80211_P2P_CLIENT_IE_BIT_CLR,
    IEEE80211_P2P_CLIENT_IE_BIT_SET,
} ieee80211_p2p_client_ie_bit_state;

typedef struct _ieee80211_p2p_client_event {
    ieee80211_p2p_client_event_type         type;
    union {
        u_int8_t oppPS;
        u_int8_t ctwindow;
        /* This struc is filled for IEEE80211_P2P_CLIENT_EVENT_BEACON_BY_SSID and 
         * IEEE80211_P2P_CLIENT_EVENT_PROBE_RESP_BY_SSID */
        struct {
            ieee80211_p2p_client_ie_bit_state   in_group_formation;
            ieee80211_p2p_client_ie_bit_state   wps_selected_registrar;
            u_int8_t                            ta[IEEE80211_ADDR_LEN];
        } filter_by_ssid_info;
    } u;
} ieee80211_p2p_client_event;

typedef void (*ieee80211_p2p_client_event_handler) (wlan_p2p_client_t, ieee80211_p2p_client_event *event, void *arg);

int ieee80211_p2p_client_register_event_handler(wlan_p2p_client_t p2p_client_handle,ieee80211_p2p_client_event_handler evhandler, void *arg, u_int32_t event_filter);

int ieee80211_p2p_client_unregister_event_handler(wlan_p2p_client_t p2p_client_handle,ieee80211_p2p_client_event_handler evhandler, void *arg);

u_int32_t ieee80211_p2p_client_get_next_tbtt_tsf_32(wlan_if_t vap);

#endif /* IEEE80211_P2P_CLIENT_H */
