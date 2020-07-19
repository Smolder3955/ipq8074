/*
 * Copyright (c) 2011,2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * Copyright (c) 2008 Atheros Communications Inc.
 * All Rights Reserved.

 */

#ifndef _IEEE80211_MLME_H
#define _IEEE80211_MLME_H


#define IEEE80211_DELIVER_EVENT_MLME_ASSOC_REQ( _vap,_evt)  do {                              \
        int i;                                                                                \
        for(i=0;i<IEEE80211_MAX_VAP_MLME_EVENT_HANDLERS; ++i) {                               \
            if ((_vap)->iv_mlme_evtable[i] &&                                                 \
            (_vap)->iv_mlme_evtable[i]->mlme_assoc_req) {                                     \
                (* (_vap)->iv_mlme_evtable[i]->mlme_assoc_req)                                \
                                           ((_vap)->iv_mlme_arg[i], _evt);                    \
             }                                                                                \
        }                                                                                     \
    } while(0)

#define IEEE80211_DELIVER_EVENT_MLME_AUTH_COMPLETE( _vap,macaddr,_evt)  do {                          \
        int i;                                                                                \
        for(i=0;i<IEEE80211_MAX_VAP_MLME_EVENT_HANDLERS; ++i) {                               \
            if ((_vap)->iv_mlme_evtable[i] &&                                                 \
            (_vap)->iv_mlme_evtable[i]->mlme_auth_complete) {                                 \
                (* (_vap)->iv_mlme_evtable[i]->mlme_auth_complete)                            \
                                           ((_vap)->iv_mlme_arg[i],macaddr, _evt);                    \
             }                                                                                \
        }                                                                                     \
    } while(0)

#define IEEE80211_DELIVER_EVENT_MLME_DEAUTH_COMPLETE( _vap,macaddr,_evt)  do {                        \
        int i;                                                                                \
        for(i=0;i<IEEE80211_MAX_VAP_MLME_EVENT_HANDLERS; ++i) {                               \
            if ((_vap)->iv_mlme_evtable[i] &&                                                 \
            (_vap)->iv_mlme_evtable[i]->mlme_deauth_complete) {                               \
                (* (_vap)->iv_mlme_evtable[i]->mlme_deauth_complete)                          \
                                           ((_vap)->iv_mlme_arg[i],macaddr, _evt);                    \
             }                                                                                \
        }                                                                                     \
    } while(0)

#define IEEE80211_DELIVER_EVENT_MLME_ASSOC_COMPLETE( _vap,_ieeeStatus,_aid,_evt)  do {        \
        int i;                                                                                \
        for(i=0;i<IEEE80211_MAX_VAP_MLME_EVENT_HANDLERS; ++i) {                               \
            if ((_vap)->iv_mlme_evtable[i] &&                                                 \
            (_vap)->iv_mlme_evtable[i]->mlme_assoc_complete) {                                \
                (* (_vap)->iv_mlme_evtable[i]->mlme_assoc_complete)                           \
                                           ((_vap)->iv_mlme_arg[i], _ieeeStatus, _aid,_evt);  \
             }                                                                                \
        }                                                                                     \
    } while(0)

#define IEEE80211_DELIVER_EVENT_MLME_REASSOC_COMPLETE( _vap,_ieeeStatus,_aid,_evt)  do {      \
        int i;                                                                                \
        for(i=0;i<IEEE80211_MAX_VAP_MLME_EVENT_HANDLERS; ++i) {                               \
            if ((_vap)->iv_mlme_evtable[i] &&                                                 \
            (_vap)->iv_mlme_evtable[i]->mlme_reassoc_complete) {                              \
                (* (_vap)->iv_mlme_evtable[i]->mlme_reassoc_complete)                         \
                                           ((_vap)->iv_mlme_arg[i], _ieeeStatus, _aid,_evt);  \
             }                                                                                \
        }                                                                                     \
    } while(0)

#define IEEE80211_DELIVER_EVENT_MLME_DISASSOC_COMPLETE( _vap,_macaddr,_reason,_evt)  do {     \
        int i;                                                                                \
        for(i=0;i<IEEE80211_MAX_VAP_MLME_EVENT_HANDLERS; ++i) {                               \
            if ((_vap)->iv_mlme_evtable[i] &&                                                 \
            (_vap)->iv_mlme_evtable[i]->mlme_disassoc_complete) {                             \
                (* (_vap)->iv_mlme_evtable[i]->mlme_disassoc_complete)                        \
                                           ((_vap)->iv_mlme_arg[i], _macaddr, _reason,_evt);  \
             }                                                                                \
        }                                                                                     \
    } while(0)

#define IEEE80211_DELIVER_EVENT_MLME_TXCHANSWITCH_COMPLETE( _vap,_evt)                 do {   \
        int i;                                                                                \
        for(i=0;i<IEEE80211_MAX_VAP_MLME_EVENT_HANDLERS; ++i) {                               \
            if ((_vap)->iv_mlme_evtable[i] &&                                                 \
            (_vap)->iv_mlme_evtable[i]->mlme_txchanswitch_complete) {                         \
                (* (_vap)->iv_mlme_evtable[i]->mlme_txchanswitch_complete)                    \
                                           ((_vap)->iv_mlme_arg[i],_evt);                     \
             }                                                                                \
        }                                                                                     \
    } while(0)

#define IEEE80211_DELIVER_EVENT_MLME_REPEATER_CAC_COMPLETE( _vap,_evt)                 do {   \
        int i;                                                                                \
        for(i=0;i<IEEE80211_MAX_VAP_MLME_EVENT_HANDLERS; ++i) {                               \
            if ((_vap)->iv_mlme_evtable[i] &&                                                 \
            (_vap)->iv_mlme_evtable[i]->mlme_repeater_cac_complete) {                         \
                (* (_vap)->iv_mlme_evtable[i]->mlme_repeater_cac_complete)                    \
                                           ((_vap)->iv_mlme_arg[i],_evt);                     \
             }                                                                                \
        }                                                                                     \
    } while(0)

#define IEEE80211_DELIVER_EVENT_MLME_AUTH_INDICATION( _vap,_macaddr,_indication_status)  do {        \
        int i;                                                                                       \
        for(i=0;i<IEEE80211_MAX_VAP_MLME_EVENT_HANDLERS; ++i) {                                      \
            if ((_vap)->iv_mlme_evtable[i] &&                                                        \
            (_vap)->iv_mlme_evtable[i]->mlme_auth_indication) {                                      \
                (* (_vap)->iv_mlme_evtable[i]->mlme_auth_indication)                                 \
                                           ((_vap)->iv_mlme_arg[i], _macaddr, _indication_status);   \
             }                                                                                       \
        }                                                                                            \
    } while(0)

#define IEEE80211_DELIVER_EVENT_MLME_DEAUTH_INDICATION( _vap,_macaddr,_associd, _reason)  do {          \
        int i;                                                                                \
        for(i=0;i<IEEE80211_MAX_VAP_MLME_EVENT_HANDLERS; ++i) {                               \
            if ((_vap)->iv_mlme_evtable[i] &&                                                 \
            (_vap)->iv_mlme_evtable[i]->mlme_deauth_indication) {                             \
                (* (_vap)->iv_mlme_evtable[i]->mlme_deauth_indication)                        \
                                           ((_vap)->iv_mlme_arg[i], _macaddr, _associd, _reason); \
             }                                                                                \
        }                                                                                     \
    } while(0)

#define IEEE80211_DELIVER_EVENT_MLME_UNPROTECTED_DEAUTH_INDICATION( _vap,_macaddr,_associd, _reason)  do {    \
        int i;                                                                                                \
        for(i=0;i<IEEE80211_MAX_VAP_MLME_EVENT_HANDLERS; ++i) {                                               \
            if ((_vap)->iv_mlme_evtable[i] &&                                                                 \
            (_vap)->iv_mlme_evtable[i]->mlme_unprotected_deauth_indication) {                                 \
                (* (_vap)->iv_mlme_evtable[i]->mlme_unprotected_deauth_indication)                            \
                                           ((_vap)->iv_mlme_arg[i], _macaddr, _associd, _reason);             \
             }                                                                                                \
        }                                                                                                     \
    } while(0)

#define IEEE80211_DELIVER_EVENT_MLME_ASSOC_INDICATION( _vap,_macaddr,_assocstatus,_wbuf,_resp_wbuf)  do {    \
        int i;                                                                                               \
        for(i=0;i<IEEE80211_MAX_VAP_MLME_EVENT_HANDLERS; ++i) {                                              \
            if ((_vap)->iv_mlme_evtable[i] &&                                                                \
            (_vap)->iv_mlme_evtable[i]->mlme_assoc_indication) {                                             \
                (* (_vap)->iv_mlme_evtable[i]->mlme_assoc_indication)                                        \
                                           ((_vap)->iv_mlme_arg[i], _macaddr, _assocstatus,_wbuf,_resp_wbuf, 0);\
             }                                                                                               \
        }                                                                                                    \
    } while(0)

#define IEEE80211_DELIVER_EVENT_MLME_REASSOC_INDICATION( _vap,_macaddr,_assocstatus,_wbuf,_resp_wbuf)  do {    \
        int i;                                                                                                 \
        for(i=0;i<IEEE80211_MAX_VAP_MLME_EVENT_HANDLERS; ++i) {                                                \
            if ((_vap)->iv_mlme_evtable[i] &&                                                                  \
            (_vap)->iv_mlme_evtable[i]->mlme_reassoc_indication) {                                             \
                (* (_vap)->iv_mlme_evtable[i]->mlme_reassoc_indication)                                        \
                                           ((_vap)->iv_mlme_arg[i], _macaddr, _assocstatus,_wbuf,_resp_wbuf, 1);  \
             }                                                                                                 \
        }                                                                                                      \
    } while(0)

#if ATH_SUPPORT_DFS && ATH_SUPPORT_STA_DFS
#define IEEE80211_DELIVER_EVENT_MLME_DISASSOC_INDICATION( _vap,_macaddr,_associd,_reason)  do {        \
        int i;                                                                                \
        mlme_set_stacac_valid((_vap),0);                                                      \
        for(i=0;i<IEEE80211_MAX_VAP_MLME_EVENT_HANDLERS; ++i) {                               \
            if ((_vap)->iv_mlme_evtable[i] &&                                                 \
            (_vap)->iv_mlme_evtable[i]->mlme_disassoc_indication) {                           \
                (* (_vap)->iv_mlme_evtable[i]->mlme_disassoc_indication)                      \
                                           ((_vap)->iv_mlme_arg[i], _macaddr, _associd, _reason);       \
             }                                                                                \
        }                                                                                     \
    } while(0)
#else
#define IEEE80211_DELIVER_EVENT_MLME_DISASSOC_INDICATION( _vap,_macaddr,_associd,_reason)  do {        \
        int i;                                                                                \
        for(i=0;i<IEEE80211_MAX_VAP_MLME_EVENT_HANDLERS; ++i) {                               \
            if ((_vap)->iv_mlme_evtable[i] &&                                                 \
            (_vap)->iv_mlme_evtable[i]->mlme_disassoc_indication) {                           \
                (* (_vap)->iv_mlme_evtable[i]->mlme_disassoc_indication)                      \
                                           ((_vap)->iv_mlme_arg[i], _macaddr, _associd, _reason);       \
             }                                                                                \
        }                                                                                     \
    } while(0)
#endif

#define IEEE80211_DELIVER_EVENT_MLME_RADAR_DETECTED( _vap,_evt)  do {                         \
        int i;                                                                                \
        for(i=0;i<IEEE80211_MAX_VAP_MLME_EVENT_HANDLERS; ++i) {                               \
            if ((_vap)->iv_mlme_evtable[i] &&                                                 \
            (_vap)->iv_mlme_evtable[i]->wlan_radar_detected) {                                \
                (* (_vap)->iv_mlme_evtable[i]->wlan_radar_detected)                           \
                                           ((_vap)->iv_mlme_arg[i], _evt);                    \
             }                                                                                \
        }                                                                                     \
    } while(0)

#define IEEE80211_DELIVER_EVENT_MLME_NODE_AUTHORIZED_INDICATION( _vap,_macaddr)  do {         \
        int i;                                                                                \
        for(i=0;i<IEEE80211_MAX_VAP_MLME_EVENT_HANDLERS; ++i) {                               \
            if ((_vap)->iv_mlme_evtable[i] &&                                                 \
            (_vap)->iv_mlme_evtable[i]->wlan_node_authorized_indication) {                    \
                (* (_vap)->iv_mlme_evtable[i]->wlan_node_authorized_indication)               \
                                           ((_vap)->iv_mlme_arg[i], _macaddr);                \
             }                                                                                \
        }                                                                                     \
    } while(0)

#define IEEE80211_DELIVER_EVENT_MLME_JOIN_COMPLETE_SET_COUNTRY( _vap,_evt)  do {                    \
        int i;                                                                                \
        for(i=0;i<IEEE80211_MAX_VAP_MLME_EVENT_HANDLERS; ++i) {                               \
            if ((_vap)->iv_mlme_evtable[i] &&                                                 \
            (_vap)->iv_mlme_evtable[i]->mlme_join_complete_set_country) {                           \
                (* (_vap)->iv_mlme_evtable[i]->mlme_join_complete_set_country)                      \
                                           ((_vap)->iv_mlme_arg[i], _evt);                    \
             }                                                                                \
        }                                                                                     \
    } while(0)

#define IEEE80211_DELIVER_EVENT_MLME_JOIN_COMPLETE_INFRA( _vap,_evt)  do {                    \
        int i;                                                                                \
        for(i=0;i<IEEE80211_MAX_VAP_MLME_EVENT_HANDLERS; ++i) {                               \
            if ((_vap)->iv_mlme_evtable[i] &&                                                 \
            (_vap)->iv_mlme_evtable[i]->mlme_join_complete_infra) {                           \
                (* (_vap)->iv_mlme_evtable[i]->mlme_join_complete_infra)                      \
                                           ((_vap)->iv_mlme_arg[i], _evt);                    \
             }                                                                                \
        }                                                                                     \
    } while(0)

#define IEEE80211_DELIVER_EVENT_MLME_JOIN_COMPLETE_ADHOC( _vap,_evt)  do {                    \
        int i;                                                                                \
        for(i=0;i<IEEE80211_MAX_VAP_MLME_EVENT_HANDLERS; ++i) {                               \
            if ((_vap)->iv_mlme_evtable[i] &&                                                 \
            (_vap)->iv_mlme_evtable[i]->mlme_join_complete_adhoc) {                           \
                (* (_vap)->iv_mlme_evtable[i]->mlme_join_complete_adhoc)                      \
                                           ((_vap)->iv_mlme_arg[i], _evt);                    \
             }                                                                                \
        }                                                                                     \
    } while(0)

#define IEEE80211_DELIVER_EVENT_MLME_IBSS_MERGE_START_INDICATION( _vap,_bssid)  do {          \
        int i;                                                                                \
        for(i=0;i<IEEE80211_MAX_VAP_MLME_EVENT_HANDLERS; ++i) {                               \
            if ((_vap)->iv_mlme_evtable[i] &&                                                 \
            (_vap)->iv_mlme_evtable[i]->mlme_ibss_merge_start_indication) {                   \
                (* (_vap)->iv_mlme_evtable[i]->mlme_ibss_merge_start_indication)              \
                                           ((_vap)->iv_mlme_arg[i], _bssid);                  \
             }                                                                                \
        }                                                                                     \
    } while(0)

#define IEEE80211_DELIVER_EVENT_MLME_IBSS_MERGE_COMPLETE_INDICATION( _vap,_bssid)  do {       \
        int i;                                                                                \
        for(i=0;i<IEEE80211_MAX_VAP_MLME_EVENT_HANDLERS; ++i) {                               \
            if ((_vap)->iv_mlme_evtable[i] &&                                                 \
            (_vap)->iv_mlme_evtable[i]->mlme_ibss_merge_completion_indication) {              \
                (* (_vap)->iv_mlme_evtable[i]->mlme_ibss_merge_completion_indication)         \
                                           ((_vap)->iv_mlme_arg[i], _bssid);                  \
             }                                                                                \
        }                                                                                     \
    } while(0)

#define IEEE80211_DELIVER_EVENT_CHANNEL_CHANGE( _vap,_evt)  do {                              \
        int i;                                                                                \
        for(i=0;i<IEEE80211_MAX_MISC_EVENT_HANDLERS; ++i) {                                   \
            if ((_vap)->iv_misc_evtable[i] &&                                                 \
                            (_vap)->iv_misc_evtable[i]->wlan_channel_change) {                \
                (* (_vap)->iv_misc_evtable[i]->wlan_channel_change)                           \
                                           ((_vap)->iv_misc_arg[i], _evt);                    \
             }                                                                                \
        }                                                                                     \
    } while(0)

#define IEEE80211_DELIVER_EVENT_COUNTRY_CHANGE( _vap,_evt)  do {                              \
        int i;                                                                                \
        for(i=0;i<IEEE80211_MAX_MISC_EVENT_HANDLERS; ++i) {                                   \
            if ((_vap)->iv_misc_evtable[i] &&                                                 \
                            (_vap)->iv_misc_evtable[i]->wlan_country_changed) {               \
                (* (_vap)->iv_misc_evtable[i]->wlan_country_changed)                          \
                                           ((_vap)->iv_misc_arg[i], _evt);                    \
             }                                                                                \
        }                                                                                     \
    } while(0)


#define IEEE80211_DELIVER_EVENT_LINK_SPEED( _vap,_rxspeed,_txspeed)  do {                              \
        int i;                                                                                \
        for(i=0;i<IEEE80211_MAX_MISC_EVENT_HANDLERS; ++i) {                                   \
            if ((_vap)->iv_misc_evtable[i] &&                                                 \
                            (_vap)->iv_misc_evtable[i]->wlan_linkspeed) {                     \
                (* (_vap)->iv_misc_evtable[i]->wlan_linkspeed)                                \
                                           ((_vap)->iv_misc_arg[i], _rxspeed, _txspeed);                    \
             }                                                                                \
        }                                                                                     \
    } while(0)

#define IEEE80211_DELIVER_EVENT_MIC_FAILURE( _vap,_frm,_idx)  do {                            \
        int i;                                                                                \
        for(i=0;i<IEEE80211_MAX_MISC_EVENT_HANDLERS; ++i) {                                   \
            if ((_vap)->iv_misc_evtable[i] &&                                                 \
                            (_vap)->iv_misc_evtable[i]->wlan_michael_failure_indication) {    \
                (* (_vap)->iv_misc_evtable[i]->wlan_michael_failure_indication)               \
                    ((_vap)->iv_misc_arg[i], _frm, _idx);                                     \
             }                                                                                \
        }                                                                                     \
    } while(0)
#define IEEE80211_DELIVER_EVENT_BLKLST_STA_AUTH_INDICATION( _vap,_macaddr,_status)  do {                            \
        int i;                                                                                \
        for(i=0;i<IEEE80211_MAX_MISC_EVENT_HANDLERS; ++i) {                                   \
            if ((_vap)->iv_misc_evtable[i] &&                                                 \
                            (_vap)->iv_misc_evtable[i]->wlan_blklst_sta_auth_indication) {    \
                (* (_vap)->iv_misc_evtable[i]->wlan_blklst_sta_auth_indication)               \
                    ((_vap)->iv_misc_arg[i], _macaddr, _status);                                     \
             }                                                                                \
        }                                                                                     \
    } while(0)

#define IEEE80211_DELIVER_EVENT_KEYSET_DONE_INDICATION( _vap,_macaddr,_status)  do {                            \
        int i;                                                                                \
        for(i=0;i<IEEE80211_MAX_MISC_EVENT_HANDLERS; ++i) {                                   \
            if ((_vap)->iv_misc_evtable[i] &&                                                 \
                            (_vap)->iv_misc_evtable[i]->wlan_keyset_done_indication) {    \
                (* (_vap)->iv_misc_evtable[i]->wlan_keyset_done_indication)               \
                    ((_vap)->iv_misc_arg[i], _macaddr, _status);                                     \
             }                                                                                \
        }                                                                                     \
    } while(0)
#define IEEE80211_DELIVER_EVENT_REPLAY_FAILURE( _vap,_frm,_idx)  do {                         \
        int i;                                                                                \
        for(i=0;i<IEEE80211_MAX_MISC_EVENT_HANDLERS; ++i) {                                   \
            if ((_vap)->iv_misc_evtable[i] &&                                                 \
                            (_vap)->iv_misc_evtable[i]->wlan_replay_failure_indication) {     \
                (* (_vap)->iv_misc_evtable[i]->wlan_replay_failure_indication)                \
                    ((_vap)->iv_misc_arg[i], _frm, _idx);                                     \
             }                                                                                \
        }                                                                                     \
    } while(0)

#define IEEE80211_DELIVER_EVENT_BEACON_MISS( _vap)  do {                                      \
        int i;                                                                                \
        for(i=0;i<IEEE80211_MAX_MISC_EVENT_HANDLERS; ++i) {                                   \
            if ((_vap)->iv_misc_evtable[i] &&                                                 \
                            (_vap)->iv_misc_evtable[i]->wlan_beacon_miss_indication) {        \
                (* (_vap)->iv_misc_evtable[i]->wlan_beacon_miss_indication)                   \
                    ((_vap)->iv_misc_arg[i]);                                                 \
             }                                                                                \
        }                                                                                     \
    } while(0)

#if ATH_SUPPORT_HYFI_ENHANCEMENTS
#define IEEE80211_DELIVER_EVENT_BUFFULL( _vap)  do {                                      \
        int i;                                                                                \
        for(i=0;i<IEEE80211_MAX_MISC_EVENT_HANDLERS; ++i) {                                   \
            if ((_vap)->iv_misc_evtable[i] &&                                                 \
                            (_vap)->iv_misc_evtable[i]->wlan_buffull) {        \
                (* (_vap)->iv_misc_evtable[i]->wlan_buffull)                   \
                    ((_vap)->iv_misc_arg[i]);                                                 \
             }                                                                                \
        }                                                                                     \
    } while(0)

#endif
#define IEEE80211_DELIVER_EVENT_BEACON_RSSI( _vap)  do {                                      \
        int i;                                                                                \
        for(i=0;i<IEEE80211_MAX_MISC_EVENT_HANDLERS; ++i) {                                   \
            if ((_vap)->iv_misc_evtable[i] &&                                                 \
                            (_vap)->iv_misc_evtable[i]->wlan_beacon_rssi_indication) {        \
                (* (_vap)->iv_misc_evtable[i]->wlan_beacon_rssi_indication)                   \
                    ((_vap)->iv_misc_arg[i]);                                                 \
             }                                                                                \
        }                                                                                     \
    } while(0)

#define IEEE80211_DELIVER_EVENT_DEVICE_ERROR( _vap)  do {                                     \
        int _i;                                                                               \
        for(_i=0;_i<IEEE80211_MAX_MISC_EVENT_HANDLERS; ++_i) {                                \
            if ((_vap)->iv_misc_evtable[_i] &&                                                \
                            (_vap)->iv_misc_evtable[_i]->wlan_device_error_indication) {      \
                (* (_vap)->iv_misc_evtable[_i]->wlan_device_error_indication)                 \
                    ((_vap)->iv_misc_arg[_i]);                                                \
             }                                                                                \
        }                                                                                     \
    } while(0)

#define IEEE80211_DELIVER_EVENT_STA_CLONEMAC( _vap)  do {                              \
         int i;                                                                             \
         for(i=0;i<IEEE80211_MAX_MISC_EVENT_HANDLERS; ++i) {                                \
             if (_vap->iv_misc_evtable[i] && _vap->iv_misc_evtable[i]->wlan_sta_clonemac_indication ) {      \
                 (* _vap->iv_misc_evtable[i]->wlan_sta_clonemac_indication)                                 \
                                            (_vap->iv_misc_arg[i]);                   \
              }                                                                             \
         }                                                                                  \
     } while(0)

#define IEEE80211_DELIVER_EVENT_STA_SCAN_ENTRY_UPDATE( _vap,_scan_entry, _new)  do {                \
        int i;                                                                                \
        for(i=0;i<IEEE80211_MAX_MISC_EVENT_HANDLERS; ++i) {                                   \
            if ((_vap)->iv_misc_evtable[i] &&                                                 \
                            (_vap)->iv_misc_evtable[i]->wlan_sta_scan_entry_update) {         \
                (* (_vap)->iv_misc_evtable[i]->wlan_sta_scan_entry_update)                    \
                                           ((_vap)->iv_misc_arg[i], _scan_entry, _new);             \
             }                                                                                \
        }                                                                                     \
    } while(0)

#define IEEE80211_DELIVER_EVENT_STA_REKEYING( _vap,_mac)  do {                \
        int i;                                                                                \
        for(i=0;i<IEEE80211_MAX_MISC_EVENT_HANDLERS; ++i) {                                   \
            if ((_vap)->iv_misc_evtable[i] &&                                                 \
                            (_vap)->iv_misc_evtable[i]->wlan_sta_rekey_indication) {         \
                (* (_vap)->iv_misc_evtable[i]->wlan_sta_rekey_indication)                    \
                                           ((_vap)->iv_misc_arg[i], _mac);             \
             }                                                                                \
        }                                                                                     \
    } while(0)

#define IEEE80211_DELIVER_EVENT_AP_STOPPED( _vap,_evt)  do {                                  \
        int i;                                                                                \
        for(i=0;i<IEEE80211_MAX_MISC_EVENT_HANDLERS; ++i) {                                   \
            if ((_vap)->iv_misc_evtable[i] &&                                                 \
                            (_vap)->iv_misc_evtable[i]->wlan_ap_stopped) {                    \
                (* (_vap)->iv_misc_evtable[i]->wlan_ap_stopped)                               \
                                           ((_vap)->iv_misc_arg[i], _evt);                    \
             }                                                                                \
        }                                                                                     \
    } while(0)

#define IEEE80211_DELIVER_EVENT_IBSS_RSSI_MONITOR( _vap,_macaddr,_rssiclass)  do {	\
        int i;                                                                                \
        for(i=0;i<IEEE80211_MAX_MISC_EVENT_HANDLERS; ++i) {                                   \
            if ((_vap)->iv_misc_evtable[i] &&                                                 \
                            (_vap)->iv_misc_evtable[i]->wlan_ibss_rssi_monitor) {                    \
                (* (_vap)->iv_misc_evtable[i]->wlan_ibss_rssi_monitor)                               \
		  ((_vap)->iv_misc_arg[i],(u_int8_t *) _macaddr, _rssiclass);	\
             }                                                                                \
        }                                                                                     \
} while(0)
#define IEEE80211_DELIVER_EVENT_COCHANNEL_AP( _vap,_val)  do {                                \
        int i;                                                                                \
        for(i=0;i<IEEE80211_MAX_MISC_EVENT_HANDLERS; ++i) {                                   \
            if ((_vap)->iv_misc_evtable[i] &&                                                 \
                            (_vap)->iv_misc_evtable[i]->wlan_cochannelap_cnt) {               \
                (* (_vap)->iv_misc_evtable[i]->wlan_cochannelap_cnt)                          \
		  ((_vap)->iv_misc_arg[i],_val);                                              \
             }                                                                                \
        }                                                                                     \
    } while(0)
#define IEEE80211_DELIVER_EVENT_CHLOAD( _vap,_chload)  do {                                   \
        int i;                                                                                \
        for(i=0;i<IEEE80211_MAX_MISC_EVENT_HANDLERS; ++i) {                                   \
            if ((_vap)->iv_misc_evtable[i] &&                                                 \
                            (_vap)->iv_misc_evtable[i]->wlan_channel_load) {                  \
                (* (_vap)->iv_misc_evtable[i]->wlan_channel_load)                             \
		  ((_vap)->iv_misc_arg[i],_chload);                                           \
             }                                                                                \
        }                                                                                     \
    } while(0)
#define IEEE80211_DELIVER_EVENT_CH_HOP_CHANNEL_CHANGE( _vap,_ch)  do {                                   \
    int i;                                                                                \
    for (i=0; i<IEEE80211_MAX_MISC_EVENT_HANDLERS; ++i) {                                   \
        if ((_vap)->iv_misc_evtable[i] &&                                                 \
                (_vap)->iv_misc_evtable[i]->wlan_ch_hop_channel_change) { \
            (* (_vap)->iv_misc_evtable[i]->wlan_ch_hop_channel_change) \
            ((_vap)->iv_misc_arg[i],_ch);                   \
        }                                                                                \
    }                                                                                     \
} while(0)

#define IEEE80211_DELIVER_EVENT_RECV_PROBE_REQ( _vap,_mac_addr,_ssid_element)  do {                           \
    int i;                                                                                \
    for (i=0; i<IEEE80211_MAX_MISC_EVENT_HANDLERS; ++i) {                                 \
        if ((_vap)->iv_misc_evtable[i] &&                                                 \
                (_vap)->iv_misc_evtable[i]->wlan_recv_probereq ) {                        \
            (* (_vap)->iv_misc_evtable[i]->wlan_recv_probereq)                            \
            ((_vap)->iv_misc_arg[i],_mac_addr,_ssid_element);                             \
        }                                                                                 \
    }                                                                                     \
} while(0)

#define IEEE80211_DELIVER_EVENT_NONERP_JOINED( _vap,_nonerpcnt)  do {	                      \
        int i;                                                                                \
        for(i=0;i<IEEE80211_MAX_MISC_EVENT_HANDLERS; ++i) {                                   \
            if ((_vap)->iv_misc_evtable[i] &&                                                 \
                            (_vap)->iv_misc_evtable[i]->wlan_nonerpcnt) {                     \
                (* (_vap)->iv_misc_evtable[i]->wlan_nonerpcnt)                                \
		  ((_vap)->iv_misc_arg[i],_nonerpcnt);                                        \
             }                                                                                \
        }                                                                                     \
    } while(0)
#define IEEE80211_DELIVER_EVENT_BGJOINED( _vap,_val)  do {	                              \
        int i;                                                                                \
        for(i=0;i<IEEE80211_MAX_MISC_EVENT_HANDLERS; ++i) {                                   \
            if ((_vap)->iv_misc_evtable[i] &&                                                 \
                            (_vap)->iv_misc_evtable[i]->wlan_bgjoin) {                        \
                (* (_vap)->iv_misc_evtable[i]->wlan_bgjoin)                                   \
		  ((_vap)->iv_misc_arg[i],_val);                                              \
             }                                                                                \
        }                                                                                     \
    } while(0)

#define IEEE80211_DELIVER_EVENT_SESSION_TIMEOUT( _vap,_macaddr)  do {	                              \
        int i;                                                                                \
        for(i=0;i<IEEE80211_MAX_MISC_EVENT_HANDLERS; ++i) {                                   \
            if ((_vap)->iv_misc_evtable[i] &&                                                 \
                            (_vap)->iv_misc_evtable[i]->wlan_session_timeout) {                        \
                (* (_vap)->iv_misc_evtable[i]->wlan_session_timeout)                                   \
		  ((_vap)->iv_misc_arg[i],_macaddr);                                              \
             }                                                                                \
        }                                                                                     \
    } while(0)

#if ATH_SUPPORT_MGMT_TX_STATUS
#define IEEE80211_DELIVER_EVENT_MGMT_TX_STATUS(_vap, _status,_wbuf)  do {                     \
        int _i;                                                                                \
        for(_i=0;_i<IEEE80211_MAX_MISC_EVENT_HANDLERS; ++_i) {                                   \
            if ((_vap)->iv_misc_evtable[_i] &&                                                 \
                (_vap)->iv_misc_evtable[_i]->wlan_mgmt_tx_status) {                            \
                (* (_vap)->iv_misc_evtable[_i]->wlan_mgmt_tx_status)                           \
                                           ((_vap)->iv_misc_arg[_i], _status, _wbuf);            \
             }                                                                                \
        }                                                                                     \
    } while(0)
#endif
#if ATH_SSID_STEERING
#define IEEE80211_DELIVER_SSID_EVENT(_vap,_macaddr)  do { \
    int i;                                                          \
    for(i = 0; i < IEEE80211_MAX_MISC_EVENT_HANDLERS; ++i) {        \
        if ((_vap)->iv_misc_evtable[i] &&                           \
                (_vap)->iv_misc_evtable[i]->ssid_event) {          \
            (* (_vap)->iv_misc_evtable[i]->ssid_event)         \
            ((_vap)->iv_misc_arg[i],(u_int8_t *) _macaddr); \
        }                                                           \
    }                                                               \
} while(0)
#endif

#define IEEE80211_DELIVER_ASSOC_DENIAL_EVENT(_vap,_macaddr)  do {   \
    int i;                                                          \
    for(i = 0; i < IEEE80211_MAX_MISC_EVENT_HANDLERS; ++i) {        \
        if ((_vap)->iv_misc_evtable[i] &&                           \
                (_vap)->iv_misc_evtable[i]->assocdeny_event) {      \
            (* (_vap)->iv_misc_evtable[i]->assocdeny_event)         \
            ((_vap)->iv_misc_arg[i],(u_int8_t *) _macaddr);         \
        }                                                           \
    }                                                               \
} while(0)

#define IEEE80211_DELIVER_EVENT_TX_OVERFLOW(_vap)  do {                                   \
        int i;                                                                            \
        for(i=0;i<IEEE80211_MAX_MISC_EVENT_HANDLERS; ++i) {                               \
         	if ((_vap)->iv_misc_evtable[i] &&                                         \
                            (_vap)->iv_misc_evtable[i]->wlan_tx_overflow) {               \
                	(*(_vap)->iv_misc_evtable[i]->wlan_tx_overflow)                   \
                                           ((_vap)->iv_misc_arg[i]);                      \
		}									  \
        }                                                                                 \
    } while(0)

#if DBDC_REPEATER_SUPPORT
#define IEEE80211_DELIVER_EVENT_STA_VAP_CONNECTION_UPDATE(_vap)  do {   \
    int i;                                                          \
    for(i = 0; i < IEEE80211_MAX_MISC_EVENT_HANDLERS; ++i) {        \
        if ((_vap)->iv_misc_evtable[i] &&                           \
                (_vap)->iv_misc_evtable[i]->stavap_connection) {      \
            (* (_vap)->iv_misc_evtable[i]->stavap_connection)         \
            ((_vap)->iv_misc_arg[i]);         \
        }                                                           \
    }                                                               \
} while(0)
#endif

#if ATH_SUPPORT_DFS && ATH_SUPPORT_STA_DFS
#define IEEE80211_DELIVER_EVENT_CAC_STARTED( _vap,freq,_timeout)  do {                            \
    int i;                                                                                \
    for(i=0;i<IEEE80211_MAX_MISC_EVENT_HANDLERS; ++i) {                                   \
        if ((_vap)->iv_misc_evtable[i] &&                                                 \
                (_vap)->iv_misc_evtable[i]->wlan_sta_cac_started) {    \
            (* (_vap)->iv_misc_evtable[i]->wlan_sta_cac_started)               \
            ((_vap)->iv_misc_arg[i], freq, _timeout);                                     \
        }                                                                                \
    }                                                                                     \
} while(0)
#endif

#define IEEE80211_DELIVER_EVENT_BSS_NODE_FREED( _vap)  do {                   \
    int i;                                                                    \
    for(i=0; i < IEEE80211_MAX_MISC_EVENT_HANDLERS; ++i) {                    \
        if ((_vap)->iv_misc_evtable[i] &&                                     \
                (_vap)->iv_misc_evtable[i]->wlan_bss_node_freed_indication) { \
            (*(_vap)->iv_misc_evtable[i]->wlan_bss_node_freed_indication)(_vap);\
        }                                                                     \
    }                                                                         \
} while(0)

#if ATH_PARAMETER_API
#define IEEE80211_DELIVER_PAPI_EVENT(_vap,_event,_eventlen,_data)  do { \
        int i;                                                          \
        for(i = 0; i < IEEE80211_MAX_MISC_EVENT_HANDLERS; ++i) {        \
            if ((_vap)->iv_misc_evtable[i] &&                           \
                (_vap)->iv_misc_evtable[i]->papi_event) {               \
                (* (_vap)->iv_misc_evtable[i]->papi_event)         \
                    ((_vap)->iv_misc_arg[i],_event,_eventlen,_data); \
            }                                                           \
        }                                                               \
    } while(0)
#else
#define IEEE80211_DELIVER_PAPI_EVENT( _vap,_event,_eventlen,_data) do { \
    }    while(0)
#endif

struct ieee80211_mlme_priv;
typedef struct ieee80211_mlme_priv *ieee80211_mlme_priv_t;

int ieee80211_mlme_attach(struct ieee80211com *ic);
int ieee80211_mlme_detach(struct ieee80211com *ic);
int ieee80211_mlme_vattach(struct ieee80211vap *vap);
int ieee80211_mlme_vdetach(struct ieee80211vap *vap);

void ieee80211_mlme_adhoc_merge_prepare(struct ieee80211vap *vap);

/*
 * Returns MLME's current BSS entry.
 */
ieee80211_scan_entry_t ieee80211_mlme_get_bss_entry(struct ieee80211vap *vap);

/*
 * to be called when the resmgr completes the vap_start request.
 */
void ieee80211_mlme_join_infra_continue(struct ieee80211vap *vap, int32_t status);
void ieee80211_mlme_create_infra_continue_async(struct ieee80211vap *vap,int32_t status);
void ieee80211_mlme_create_join_ibss_continue(struct ieee80211vap *vap, int32_t status);
void mlme_inact_timeout(struct ieee80211vap *vap);
void ieee80211_update_erp_info(struct ieee80211vap *vap);

void ieee80211_mlme_chanswitch_continue(struct ieee80211_node *ni, int err);

/**
 * wlan_pdev_mlme_vdev_sm_notify_radar_ind - Notify RADAR indication to all
 *                                           VDEVs
 * @pdev:  PDEV object
 * @new_chan:  New channel for the operation
 *
 * API to notify radar indication to all VDEVs of PDEV
 *
 * Return: QDF_STATUS_SUCCESS on successful assignment
 *         QDF_STATUS_E_FAILURE, if it fails due to any
 */
void wlan_pdev_mlme_vdev_sm_notify_radar_ind(struct wlan_objmgr_pdev *pdev,
					struct ieee80211_ath_channel *new_chan);
void wlan_pdev_mlme_vdev_sm_notify_chan_failure(struct wlan_objmgr_pdev *pdev,
					struct ieee80211_ath_channel *new_chan);
void wlan_pdev_mlme_vdev_sm_csa_restart(struct wlan_objmgr_pdev *pdev,
				   struct ieee80211_ath_channel *new_chan);
void wlan_pdev_mlme_vdev_sm_chan_change(struct wlan_objmgr_pdev *pdev,
				   struct ieee80211_ath_channel *new_chan);
void wlan_pdev_mlme_vdev_sm_seamless_chan_change
				(struct wlan_objmgr_pdev *pdev,
				 struct wlan_objmgr_vdev *vdev,
				 struct ieee80211_ath_channel *new_chan);


void mlme_vdev_sm_notify_peers_disconnected(struct ieee80211vap *vap);
void mlme_vdev_sm_notify_conn_sm_init_state(struct ieee80211vap *vap);


/*
 * Query all VAPs for a connection involving the specified BSS entry.
 */
bool ieee80211_mlme_is_connected(struct ieee80211com *ic, struct scan_cache_entry *bss_entry);

/*
 * station powersave state change handler.
 * for AP mode only.
 */
void ieee80211_mlme_node_pwrsave(struct ieee80211_node *ni, int enable);

/*
 * send an action frame.
 * @param vap      : vap pointer.
 * @param dst_addr : destination address.
 * @param src_addr : source address.
 * @param bssid    : bssid.
 * @param data     : data buffer conataining the action frame including action category  and type.
 * @param data_len : length of the data buffer.
 * @param handler  : hanlder called when the frame transmission completes.
 * @param arg      : opaque pointer passed back via the handler.
 * @ returns 0 if success, -ve if failed.
 */   
int ieee80211_vap_send_action_frame(struct ieee80211vap *vap,const u_int8_t *dst_addr,const  u_int8_t *src_addr, const u_int8_t *bssid,
                                    const u_int8_t *data, u_int32_t data_len, ieee80211_vap_complete_buf_handler handler, void *arg);


void ieee80211_send_setup( struct ieee80211vap *vap, struct ieee80211_node *ni,
    struct ieee80211_frame *wh, u_int8_t type, const u_int8_t *sa, const u_int8_t *da,
                           const u_int8_t *bssid);
/*
 * Parameters supplied when adding/updating an entry in a
 * scan cache.  Pointer variables should be set to NULL
 * if no data is available.  Pointer references can be to
 * local data; any information that is saved will be copied.
 * All multi-byte values must be in host byte order.
 */
struct ieee80211_ie_list {
    u_int8_t    *tim;
    u_int8_t    *country;
    u_int8_t    *ssid;
    u_int8_t    *rates;
    u_int8_t    *xrates;
    u_int8_t    *csa;
    u_int8_t    *xcsa;
    u_int8_t    *wpa;
    u_int8_t    *wcn;
    u_int8_t    *rsn;
    u_int8_t    *wps;
    u_int8_t    *wmeinfo;
    u_int8_t    *wmeparam;
    u_int8_t    *quiet;
    u_int8_t    *htcap;
    u_int8_t    *htinfo;
    u_int8_t    *athcaps;
    u_int8_t    *athextcaps;
    u_int8_t    *sfa;
    u_int8_t    *vendor;
    u_int8_t    *qbssload;
#if ATH_SUPPORT_WAPI
    u_int8_t    *wapi;
#endif
    u_int8_t    *p2p;
    u_int8_t    *alt_wcn;   /* alternate WCN from the "other" beacon or probe resp frame */
    u_int8_t    *extcaps;
#if ATH_SUPPORT_IBSS_DFS
    u_int8_t    *ibssdfs;
#endif /* ATH_SUPPORT_IBSS_DFS */
    u_int8_t    *sonadv;
    u_int8_t    *vhtcap;
    u_int8_t    *vhtop;
    u_int8_t    *opmode;
    u_int8_t    *cswrp;
    u_int8_t    *widebw;
    u_int8_t    *txpwrenvlp;
    u_int8_t    *bwnss_map;
    u_int8_t    *secchanoff;  /* secondary channel offset */
    u_int8_t    *hecap;
    u_int8_t    *heop;
    u_int8_t    *muedca;
    u_int8_t    *mboie;
#if DBDC_REPEATER_SUPPORT
    u_int8_t    *extender_ie;  /* extender ie offset */
#endif
};

/*
 * All relevant information about a received Beacon or Probe Response.
 */
struct ieee80211_scanentry_params {
    u_int16_t                   capinfo;        /* 802.11 capabilities */
    u_int16_t                   fhdwell;        /* FHSS dwell interval */
    u_int8_t                    fhindex;
    u_int8_t                    erp;
    u_int16_t                   bintval;
    u_int8_t                    timoff;
    u_int32_t                   phy_mode;
    struct ieee80211_ath_channel    *chan;
    u_int8_t                    *tsf;
    bool                        channel_mismatch;
    struct ieee80211_ie_list    ie_list;
#if QCA_LTEU_SUPPORT
    u_int16_t                   sequence_num;
#endif
    bool                        oce_cap;
};

void ieee80211_mlme_node_leave(struct ieee80211_node *ni);
bool ieee80211_mlme_check_all_nodes_asleep(ieee80211_vap_t vap);

int ieee80211_mlme_get_num_assoc_sta(ieee80211_vap_t vap);

/*
 * Suspend or Resume the transmission of beacon for this SoftAP VAP.
 * @param vap           : vap pointer.
 * @param en_suspend    : boolean flag to enable or disable suspension.
 * @ returns 0 if success, others if failed.
 */   
int
ieee80211_mlme_set_beacon_suspend_state(
    struct ieee80211vap *vap, 
    bool en_suspend);

bool ieee80211_mlme_beacon_suspend_state(struct ieee80211vap *vap);

#if DYNAMIC_BEACON_SUPPORT
/*
 * Suspend or Resume the transmission of beacon for DFS or non DFS channel.
 * @param vap           : vap pointer.
 * @param en_suspend    : boolean flag to enable or disable suspension.
 */
void ieee80211_mlme_set_dynamic_beacon_suspend(struct ieee80211vap *vap, bool suspend_beacon);
#endif
/**
 * allocate a requestor id to enable/disable sw bmiss 
 * @param vap        : vap handle
 * @param name       :  friendly name of the requestor(for debugging). 
 * @return unique id for the requestor if success , returns 0 if failed. 
 */
u_int32_t ieee80211_mlme_sta_swbmiss_timer_alloc_id(struct ieee80211vap *vap, int8_t *requestor_name);

/**
 * free a requestor id to enable/disable sw bmiss 
 * @param vap           : vap handle
 * @param requestor_id  : id of requestor 
 * @return EOK if success and error code if failed. 
 */
int ieee80211_mlme_sta_swbmiss_timer_free_id(struct ieee80211vap *vap, u_int32_t id);

/**
 * enable sw bmiss timer
 * @param vap           : vap handle
 * @param requestor_id  : id of requestor 
 * @return EOK if success and error code if failed. 
 */
int ieee80211_mlme_sta_swbmiss_timer_enable(struct ieee80211vap *vap, u_int32_t id);

/**
 * disable sw bmiss timer
 * @param vap           : vap handle
 * @param requestor_id  : id of requestor 
 * @return EOK if success and error code if failed. 
 */
int ieee80211_mlme_sta_swbmiss_timer_disable(struct ieee80211vap *vap, u_int32_t id);

/**
 * indicate beacon miss detected for this VAP
 * @param vap           : vap handle
 */
void ieee80211_mlme_sta_bmiss_ind(struct ieee80211vap *vap);

/* MLME event API */
typedef enum {
  IEEE80211_MLME_EVENT_BEACON_MISS,         /* beacon miss detected */
  IEEE80211_MLME_EVENT_BEACON_MISS_CLEAR,   /* beacon miss was cleared */
  IEEE80211_MLME_EVENT_STA_JOIN,            /* station join */
  IEEE80211_MLME_EVENT_STA_LEAVE,           /* station leave */
  IEEE80211_MLME_EVENT_STA_ENTER_PS,        /* sta enters power save */
  IEEE80211_MLME_EVENT_STA_EXIT_PS,         /* sta exits power save */
} ieee80211_mlme_event_type;

typedef struct _ieee80211_mlme_event_sta {
  struct ieee80211_node *ni; /* sta causing event */
  u_int32_t   sta_count;     /* number of stations associated now , accounting for the sta causing event */
  u_int32_t   sta_ps_count;  /* number of stations in power save, accounting for the sta causing event  */
} ieee80211_mlme_event_sta;

typedef struct _ieee80211_mlme_event_bmiss {
  u_int32_t cur_bmiss_count; /* current beacon miss count */
  u_int32_t max_bmiss_count; /* max beacon miss count, when cur_bmiss_count == max_bmiss_count OS will be notified */
} ieee80211_mlme_event_bmiss;

typedef struct _ieee80211_mlme_event {
    ieee80211_mlme_event_type  type;
    union {
         ieee80211_mlme_event_sta event_sta;     /* IEEE80211_MLME_EVENT_STA_*  */
         ieee80211_mlme_event_bmiss event_bmiss; /* IEEE80211_MLME_EVENT_BEACON_MISS */
    } u;
} ieee80211_mlme_event;

typedef void (*ieee80211_mlme_event_handler) (ieee80211_vap_t, ieee80211_mlme_event *event, void *arg);

/**
 * register a mlme  event handler.
 * @param vap        : handle to vap object
 * @param evhandler  : event handler function.
 * @param arg        : argument passed back via the event handler
 * @return EOK if success, EINVAL if failed, ENOMEM if runs out of memory.
 * allows more than one event handler to be registered.
 */
int ieee80211_mlme_register_event_handler(ieee80211_vap_t vap,ieee80211_mlme_event_handler evhandler, void *arg);

/**
 * unregister a mlme  event handler.
 * @param vap        : handle to vap object
 * @param evhandler  : event handler function.
 * @param arg        : argument passed back via the evnt handler
 * @return EOK if success, EINVAL if failed.
 */
int ieee80211_mlme_unregister_event_handler(ieee80211_vap_t vap,ieee80211_mlme_event_handler evhandler, void *arg);

void ieee80211_mlme_reset_bmiss(struct ieee80211vap *vap);

#if UMAC_SUPPORT_BTAMP
int ieee80211_mlme_auth_request_btamp(wlan_if_t vaphandle, u_int8_t *peer_addr, u_int32_t timeout);
int ieee80211_mlme_assoc_request_btamp(wlan_if_t vaphandle, u_int8_t *peer, u_int32_t timeout);
int ieee80211_mlme_reassoc_request_btamp(wlan_if_t vaphandle, u_int8_t *peer, u_int8_t *prev_bssid, u_int32_t timeout);
#endif

#if ATH_SUPPORT_DEFERRED_NODE_CLEANUP
void ieee80211_mlme_frame_complete_handler(wlan_if_t vap, wbuf_t wbuf,void *arg,
        u_int8_t *dst_addr, u_int8_t *src_addr, u_int8_t *bssid,
        ieee80211_xmit_status *ts);
#endif

/*
 * Auxiliary function to calculate the maximum allowable scan_entry age.
 */
u_int32_t ieee80211_mlme_maximum_scan_entry_age(wlan_if_t vaphandle, 
                                                systime_t reference_time);

bool
ieee80211_is_pmf_enabled(struct ieee80211vap *vap, struct ieee80211_node *ni);

void ieee80211_scs_vattach(struct ieee80211vap *vap);
void ieee80211_scs_vdetach(struct ieee80211vap *vap);

#if ATH_SUPPORT_DFS && ATH_SUPPORT_STA_DFS
void mlme_set_stacac_timer(struct ieee80211vap *vap, u_int32_t expire_ms);
void mlme_cancel_stacac_timer(struct ieee80211vap *vap);
bool mlme_is_stacac_running(struct ieee80211vap *vap);
void mlme_set_stacac_running(struct ieee80211vap *vap, u_int8_t set);
bool mlme_is_stacac_valid(struct ieee80211vap *vap);
void mlme_set_stacac_valid(struct ieee80211vap *vap, u_int8_t set);
void mlme_reset_mlme_req(struct ieee80211vap *vap);
void mlme_stacac_restore_defaults(struct ieee80211vap *vap);
bool mlme_is_stacac_needed(struct ieee80211vap *vap);

#define wlan_mlme_cancel_stacac_timer mlme_cancel_stacac_timer
#define wlan_mlme_reset_mlme_req      mlme_reset_mlme_req
#define wlan_mlme_set_stacac_running  mlme_set_stacac_running
#define wlan_mlme_set_stacac_valid    mlme_set_stacac_valid
#define wlan_mlme_stacac_restore_defaults  mlme_stacac_restore_defaults
#endif

void ieee80211_indicate_sta_radar_detect(struct ieee80211_node *ni);
int channel_switch_set_channel(struct ieee80211vap *vap, struct ieee80211com *ic);
bool is_subset_channel_for_cac(struct ieee80211com *ic, struct ieee80211_ath_channel *curchan, struct ieee80211_ath_channel *prevchan);

void ieee80211_mgmt_sta_send_csa_rx_nl_msg(
    struct ieee80211com                         *ic,
    struct ieee80211_node                       *ni,
    struct ieee80211_ath_channel                *chan,
    struct ieee80211_ath_channelswitch_ie       *chanie,
    struct ieee80211_extendedchannelswitch_ie   *echanie,
    struct ieee80211_ie_sec_chan_offset         *secchanoffsetie,
    struct ieee80211_ie_wide_bw_switch          *widebwie);

void ieee80211_mlme_event_callback(ieee80211_vap_t vap,
                                   ieee80211_mlme_event *event,
                                   void *arg);
void ieee80211_estimate_dynamic_muedca_params(ieee80211_vap_t vap);

#endif /* _IEEE80211_MLME_H */
