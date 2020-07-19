/*
* Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 * Copyright (c) 2011, Atheros Communications
 *
 * Private Header file for P2P Group Owner Module.
 *
 */
/*
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#ifndef _IEEE80211_P2P_GO_PRIV_H
#define _IEEE80211_P2P_GO_PRIV_H

#define IEEE80211_MAX_P2P_IE 1024

typedef struct ieee80211_p2p_prot_go *ieee80211_p2p_prot_go_t;

struct ieee80211_p2p_go {
    u_int32_t               specialmarker;  /* Unique marker for this structure. For debugging */
    wlan_dev_t              devhandle;
    wlan_if_t               vap;

    ieee80211_p2p_go_ps_t   p2p_go_ps;      /* opaque handle to P2P GO PS info */
    spinlock_t              lock;           /* lock to access this structure */

    /** Id used to set and get Application IE's */
    struct wlan_mlme_app_ie *app_ie_handle;

    /* registered event handlers */
    wlan_p2p_event_handler                 event_handler;
    void                                   *event_arg;
    u_int32_t                              p2p_req_id;   /* id of a perticular request */

    /* NOA IE sub-attributes blob */
    u_int8_t                noa_sub_ie[2 + IEEE80211_NOA_IE_SIZE(IEEE80211_MAX_NOA_DESCRIPTORS)];
    u_int16_t               noa_sub_ie_len;  /* Size of NOA IE blob */

    /* Beacon Frame P2P IE sub-attributes blob */
    u_int8_t                *beacon_sub_ie;
    u_int16_t               beacon_sub_ie_len;

    /* P2P Module's IE sub-attributes blob */
    u_int8_t                *p2p_module_sub_ie;
    u_int16_t               p2p_module_sub_ie_len;

    /* Probe Response P2P IE (used when replying to P2P devices and GO) */
    struct {
        spinlock_t          lock;           /* lock to access this structure */
        u_int8_t            *curr_ie;       /* current p2p ie */
        u_int16_t           curr_ie_len;
        u_int8_t            *new_ie;        /* new p2p ie */
        u_int16_t           new_ie_len;
    } proberesp_p2p_ie;

    bool                    en_suppress_beacon;     /* Enable feature of suppress beacon */
    bool                    beacon_is_suppressed;   /* beacon is currently suppressed */
    bool                    use_scheduler_for_scan; /* use scheduler for scan */
    bool                    p2p_go_allow_p2p_clients_only; /* only P2P-aware clients can associate */
    u_int16_t               p2p_go_max_clients;            /* maximum number of clients that can associate*/

    /* P2P Device address */
    u_int8_t                p2p_device_address_valid;
    u_int8_t                p2p_device_address[IEEE80211_ADDR_LEN];

    /* P2P client Device address */
    u_int8_t                p2p_client_device_addr_num;
    u_int8_t                p2p_client_device_addr[P2P_MAX_GROUP_ENTRIES][IEEE80211_ADDR_LEN];

    /* WPS Primary Device Type */
    u_int8_t                wps_primary_device_type_valid;
    u_int8_t                wps_primary_device_type[WPS_DEV_TYPE_LEN];

    ieee80211_p2p_prot_go_t prot_info;              /* Protocol Module Information */

#if TEST_VAP_PAUSE
    os_timer_t              pause_timer;                   /* to monitor vap activity */
    u_int32_t               pause_state;
    ieee80211_tx_bcn_notify_t
                        h_tx_bcn_notify;
#endif
};

#endif  //_IEEE80211_P2P_GO_PRIV_H
