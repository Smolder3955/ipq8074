/*
* Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 * Copyright (c) 2011, Atheros Communications
 *
 * Private Header file for P2P Client Module.
 *
 */
/*
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#ifndef _IEEE80211_P2P_CLIENT_PRIV_H
#define _IEEE80211_P2P_CLIENT_PRIV_H

typedef struct ieee80211_p2p_prot_client *ieee80211_p2p_prot_client_t;

#define IEEE80211_MAX_P2P_CLIENT_EVENT_HANDLERS 4

struct ieee80211_p2p_client {
    wlan_dev_t                                 devhandle;
    wlan_if_t                                  vap;
    ieee80211_p2p_client_power_t               p2p_client_power;
    ieee80211_p2p_go_schedule_t                go_schedule;
    struct ieee80211_p2p_sub_element_noa       noa_ie;
    /* registered p2p event handler */
    wlan_p2p_event_handler                     p2p_event_handler;
    void                                       *p2p_event_arg;
    u_int32_t                                  p2p_req_id;   /* id of a perticular request */
    /* registered p2p client-specific event handlers */
    ieee80211_p2p_client_event_handler         p2p_client_event_handlers[IEEE80211_MAX_P2P_CLIENT_EVENT_HANDLERS];
    void                                       *p2p_client_event_handler_args[IEEE80211_MAX_P2P_CLIENT_EVENT_HANDLERS];
    u_int32_t                                  p2p_client_event_filters[IEEE80211_MAX_P2P_CLIENT_EVENT_HANDLERS];
    u_int32_t                                  p2p_client_event_filter;
    spinlock_t                                 p2p_client_lock;
    u_int32_t                                  noa_initialized:1,
                                               go_schedule_needs_update:1;
    os_timer_t                                 schedule_update_timer;        /*schedule needs to be update every 30mins to handl tsf wrap around issue  */
    ieee80211_vap_tsf_offset                   tsf_offset_info; /* tsf offset between GO and our HW tsf */
    wlan_mlme_app_ie_t                         p2p_client_app_ie;

    ieee80211_p2p_client_ie_bit_state          in_group_formation;
    ieee80211_p2p_client_ie_bit_state          wps_selected_registrar;

    ieee80211_ssid                             ssid;    /* STNG TODO: for the p2p client code path, also needs to init. this field */

	ieee80211_p2p_prot_client_t                prot_info;       /* Protocol module instance information */
#if TEST_VAP_PAUSE
    os_timer_t              pause_timer;                   /* to monitor vap activity */
    u_int32_t               pause_state;
#endif
};

#endif  //_IEEE80211_P2P_CLIENT_PRIV_H

