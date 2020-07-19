/*
 * Copyright (c) 2011,2018-2019 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * Copyright (c) 2008 Atheros Communications Inc.
 * All Rights Reserved.
 */

#ifndef _IEEE80211_MLME_PRIV_H
#define _IEEE80211_MLME_PRIV_H

#include <osdep.h>

#if ATH_SUPPORT_WRAP
#include <if_athvar.h>
#endif

#include <ieee80211_var.h>
#include <ieee80211_channel.h>
#include <ieee80211_rateset.h>
#include <ieee80211_regdmn.h>
#include <ieee80211_mlme.h>         /* Public within UMAC */
#include <ieee80211_power.h>         /* Public within UMAC */
#include <ieee80211_proto.h>         /* Public within UMAC */
#include <ieee80211_vap.h>         /* Public within UMAC */
#include <ieee80211_vap_tsf_offset.h>
#ifdef QCA_SUPPORT_CP_STATS
#include <wlan_cp_stats_ic_utils_api.h>
#endif

/* Defines */
#define MLME_WAIT_FOR_BSS_JOIN(_mp)     atomic_set(&((_mp)->im_join_wait_beacon_to_synchronize), 1)
#define MLME_STOP_WAITING_FOR_JOIN(_mp)      \
    (OS_ATOMIC_CMPXCHG(&((_mp)->im_join_wait_beacon_to_synchronize), 1, 0) == 1)

#define MLME_DEAUTH_WAITING_THRESHOLD          3       /* 3 ticks, or 6 seconds */
#define MLME_PROBE_REQUEST_LIMIT               1       /* 1 probe request */
#define MLME_NODE_EXPIRE_TIME                  75000   /* msec */
#define MLME_IBSS_WATCHDOG_INTERVAL            500     /* msec */
#define MLME_IBSS_BEACON_MISS_ALERT            1000    /* msec */
#if ATH_SUPPORT_IBSS_WPA2
#define MLME_DEFAULT_DISASSOCIATION_TIMEOUT    20000    /* msec */
#else
#define MLME_DEFAULT_DISASSOCIATION_TIMEOUT    2000    /* msec */
#endif

#define MLME_IBSS_CANDIDATE_LIST_MAX_COUNT     32
#define MLME_IBSS_ASSOC_LIST_MAX_COUNT         32
#define MLME_REQ_ID                            0x1234

/* Type of mlme request */
enum mlme_req {
    MLME_REQ_NONE = 0,
    MLME_REQ_JOIN_INFRA,
    MLME_REQ_JOIN_ADHOC,
    MLME_REQ_AUTH,
    MLME_REQ_ASSOC,
    MLME_REQ_REASSOC,
    MLME_REQ_TXCSA,
    MLME_REQ_REPEATER_CAC,
};


/* Verify if the addresses match */
#ifdef QCA_SUPPORT_CP_STATS
#define IEEE80211_VERIFY_ELEMENT(__elem, __maxlen) do {     \
    if ((__elem) == NULL) {                                 \
        IEEE80211_DISCARD(vap, IEEE80211_MSG_ELEMID,        \
        wh, ieee80211_mgt_subtype_name[subtype >>           \
        IEEE80211_FC0_SUBTYPE_SHIFT],                       \
        "%s", "no " #__elem);                               \
        vdev_cp_stats_rx_elem_missing_inc(vap->vdev_obj, 1); \
        return -EINVAL;                                     \
    }                                                       \
    if ((__elem)[1] > (__maxlen)) {                         \
        IEEE80211_DISCARD(vap, IEEE80211_MSG_ELEMID,        \
        wh, ieee80211_mgt_subtype_name[subtype >>           \
        IEEE80211_FC0_SUBTYPE_SHIFT],                       \
        "bad " #__elem " len %d", (__elem)[1]);             \
        vdev_cp_stats_rx_elem_too_big_inc(vap->vdev_obj, 1); \
        return -EINVAL;                                     \
    }                                                       \
} while (0)

#define IEEE80211_VERIFY_LENGTH(_len, _minlen) do {         \
    if ((_len) < (_minlen)) {                               \
        IEEE80211_DISCARD(vap, IEEE80211_MSG_ELEMID,        \
        wh, ieee80211_mgt_subtype_name[subtype >>           \
        IEEE80211_FC0_SUBTYPE_SHIFT],                       \
        "%s", "ie too short\n");                            \
        vdev_cp_stats_rx_mgmt_discard_inc(vap->vdev_obj, 1); \
        return -EINVAL;                                     \
    }                                                       \
} while (0)

#define IEEE80211_VERIFY_SSID(_ni, _ssid) do {              \
    if ((_ssid)[1] != 0 &&                                  \
        ((_ssid)[1] != (_ni)->ni_esslen ||                  \
        OS_MEMCMP((_ssid) + 2, (_ni)->ni_essid, (_ssid)[1]) != 0)) {\
        vdev_cp_stats_rx_ssid_mismatch_inc(vap->vdev_obj, 1); \
        return -EINVAL;                                     \
    }                                                       \
} while (0)
#else
#define IEEE80211_VERIFY_ELEMENT(__elem, __maxlen) do {     \
    if ((__elem) == NULL) {                                 \
        IEEE80211_DISCARD(vap, IEEE80211_MSG_ELEMID,        \
        wh, ieee80211_mgt_subtype_name[subtype >>           \
        IEEE80211_FC0_SUBTYPE_SHIFT],                       \
        "%s", "no " #__elem);                               \
        return -EINVAL;                                     \
    }                                                       \
    if ((__elem)[1] > (__maxlen)) {                         \
        IEEE80211_DISCARD(vap, IEEE80211_MSG_ELEMID,        \
        wh, ieee80211_mgt_subtype_name[subtype >>           \
        IEEE80211_FC0_SUBTYPE_SHIFT],                       \
        "bad " #__elem " len %d", (__elem)[1]);             \
        return -EINVAL;                                     \
    }                                                       \
} while (0)

#define IEEE80211_VERIFY_LENGTH(_len, _minlen) do {         \
    if ((_len) < (_minlen)) {                               \
        IEEE80211_DISCARD(vap, IEEE80211_MSG_ELEMID,        \
        wh, ieee80211_mgt_subtype_name[subtype >>           \
        IEEE80211_FC0_SUBTYPE_SHIFT],                       \
        "%s", "ie too short\n");                            \
        return -EINVAL;                                     \
    }                                                       \
} while (0)

#define IEEE80211_VERIFY_SSID(_ni, _ssid) do {              \
    if ((_ssid)[1] != 0 &&                                  \
        ((_ssid)[1] != (_ni)->ni_esslen ||                  \
        OS_MEMCMP((_ssid) + 2, (_ni)->ni_essid, (_ssid)[1]) != 0)) {\
        return -EINVAL;                                     \
    }                                                       \
} while (0)

#define IEEE80211_VERIFY_LENGTH(_len, _minlen) do {         \
    if ((_len) < (_minlen)) {                               \
        IEEE80211_DISCARD(vap, IEEE80211_MSG_ELEMID,        \
        wh, ieee80211_mgt_subtype_name[subtype >>           \
        IEEE80211_FC0_SUBTYPE_SHIFT],                       \
        "%s", "ie too short\n");                            \
        return -EINVAL;                                     \
    }                                                       \
} while (0)

#endif /* QCA_SUPPORT_CP_STATS */

#define IEEE80211_VERIFY_ADDR(_ni) do {                     \
    if (!IEEE80211_ADDR_EQ(wh->i_addr1, vap->iv_myaddr) ||  \
        !IEEE80211_ADDR_EQ(wh->i_addr3, (_ni)->ni_bssid)) { \
        IEEE80211_DISCARD(vap, IEEE80211_MSG_INPUT,         \
        wh, ieee80211_mgt_subtype_name[subtype >>           \
        IEEE80211_FC0_SUBTYPE_SHIFT],                       \
        "%s", "frame not for me");                          \
        return -EINVAL;                                     \
    }                                                       \
} while (0)

#define IEEE80211_CHECK_SSID(_ssid, hidden_ssid) do {           \
    hidden_ssid = FALSE;                                        \
    if ((_ssid)[1] != 0) {                                      \
        u_int8_t zero_ssid[IEEE80211_NWID_LEN];                 \
        OS_MEMZERO(zero_ssid, IEEE80211_NWID_LEN);              \
        if (OS_MEMCMP((_ssid) + 2, zero_ssid, (_ssid)[1]) == 0){\
            hidden_ssid = TRUE;                                 \
        }                                                       \
    }                                                           \
} while (0)

#define IEEE80211_VERIFY_FRAMELEN(_pFrm, _pEfrm) do {       \
    if ((_pFrm) > (_pEfrm)) {                               \
        IEEE80211_DISCARD(vap, IEEE80211_MSG_ELEMID,        \
        wh, ieee80211_mgt_subtype_name[subtype >>           \
        IEEE80211_FC0_SUBTYPE_SHIFT],                       \
        "%s", "frame too short");                           \
        return -EINVAL;                                     \
    }                                                       \
} while (0)

#define IEEE80211_MATCH_SSID(_ni, _ssid)               \
    ((_ssid)[1] != 0 &&                                  \
        ((_ssid)[1] != (_ni)->ni_esslen ||                  \
        OS_MEMCMP((_ssid) + 2, (_ni)->ni_essid, (_ssid)[1]) != 0))

/*
 * Private APIs
 *      - private to mlme implementation
 *      - called by mgmt frame processing (ieee80211_mgmt_input.c)
 */

/* Confirmations */
void ieee80211_mlme_join_complete_infra(struct ieee80211_node *ni);
int ieee80211_mlme_recv_auth(struct ieee80211_node *ni,
                              u_int16_t algo, u_int16_t seq, u_int16_t status_code,
                              u_int8_t *challenge, u_int8_t challenge_length,wbuf_t wbuf,
                              const struct ieee80211_rx_status *rx_status);
void ieee80211_mlme_recv_assoc_response(struct ieee80211_node *ni, int subtype,
    u_int16_t capability, u_int16_t status_code, u_int16_t aid,
    u_int8_t *ie_data, u_int32_t ie_length, wbuf_t wbuf);
int ieee80211_mlme_recv_assoc_request(struct ieee80211_node *ni,
    u_int8_t reassoc,u_int8_t *vendor_ie, wbuf_t wbuf);

bool ieee80211_is_mmie_valid(struct ieee80211vap *vap, struct ieee80211_node *ni, u_int8_t* frm, u_int8_t* efrm);

/* Indications */
void ieee80211_mlme_recv_deauth(struct ieee80211_node *ni, u_int16_t reason_code);
void ieee80211_mlme_recv_disassoc(struct ieee80211_node *ni, u_int32_t reason_code);
void ieee80211_mlme_recv_csa(struct ieee80211_node *ni, u_int32_t csa_delay, bool disconnect);

u_int32_t mlme_dot11rate_to_bps(u_int8_t  rate);



#if UMAC_SUPPORT_IBSS
void ieee80211_mlme_adhoc_join_indication(struct ieee80211_node *ni, wbuf_t wbuf);
void mlme_adhoc_vattach(struct ieee80211vap *vap);
void mlme_adhoc_vdetach(struct ieee80211vap *vap);
int  mlme_create_adhoc_bss(struct ieee80211vap *vap);
void mlme_stop_adhoc_bss(struct ieee80211vap *vap,int flags);
int  mlme_resume_adhoc_bss(struct ieee80211vap *vap);
void mlme_pause_adhoc_bss(struct ieee80211vap *vap);
void ieee80211_mlme_join_complete_adhoc(struct ieee80211_node *ni);
void ieee80211_mlme_adhoc_leave_indication(struct ieee80211_node *ni, u_int16_t reason_code);
void ieee80211_mlme_adhoc_merge_start(struct ieee80211_node *ni);
void ieee80211_mlme_adhoc_merge_completion(struct ieee80211_node *ni);
int mlme_recv_auth_ibss(struct ieee80211_node *ni,
                         u_int16_t algo, u_int16_t seq, u_int16_t status_code,
                         u_int8_t *challenge, u_int8_t challenge_length, wbuf_t wbuf);
void ieee80211_recv_beacon_ibss(struct ieee80211_node *ni, wbuf_t wbuf, int subtype,
                               struct ieee80211_rx_status *rs, ieee80211_scan_entry_t  scan_entry);
#define    IEEE80211_MLME_IBSS_PRIVATE                                                                                 \
    os_timer_t                      im_ibss_timer;               /* IBSS watchdog timer */                             \
    u_int32_t                       im_ibss_timer_interval;      /* IBSS watchdog timer interval in ms */              \
    u_int32_t                       im_ibss_beacon_miss_alert;   /* in ms */                                           \
    u_int8_t                        im_ibss_timer_cancel_flag;   /* indicates timer must be cancelled */               \
    atomic_t                        im_ibss_timer_lock;          /* indicate timer handler is currently executing */   \
    struct ieee80211_node *         im_ibss_candidate_list[MLME_IBSS_CANDIDATE_LIST_MAX_COUNT];                        \
    u_int32_t                       im_ibss_candidate_count;
#else /* UMAC_SUPPORT_IBSS */
#define mlme_adhoc_vattach(vap)                              /**/
#define mlme_adhoc_vdetach(vap)                              /**/
#define mlme_create_adhoc_bss(vap)                          -EINVAL
#define mlme_stop_adhoc_bss(vap,flags)                      -EINVAL
#define mlme_resume_adhoc_bss(vap)                          -EINVAL
#define mlme_pause_adhoc_bss(vap)                           -EINVAL
#define ieee80211_mlme_adhoc_join_indication(ni,wbuf)        /**/
#define ieee80211_mlme_join_complete_adhoc(ni)               /**/
#define ieee80211_mlme_adhoc_leave_indication(ni,reason_code)/**/
#define ieee80211_mlme_adhoc_merge_start(ni)                 /**/
#define ieee80211_mlme_adhoc_merge_completion(ni)            /**/
#define    IEEE80211_MLME_IBSS_PRIVATE                       /**/
#define mlme_recv_auth_ibss(ni,algo,seq,status_code,challenge,challenge_length,wbuf)  (0)
#define ieee80211_recv_beacon_ibss(ni,wbuf,subtype,rs,scan_entry) /**/
#endif /* UMAC_SUPPORT_IBSS */

#define IEEE80211_INACT_NONERP 10 /* Aging period for non ERP bss (secs)*/
#define IEEE80211_INACT_HT 10    /* Aging period for HT protection (secs)*/

u_int16_t mlme_auth_shared(struct ieee80211_node *ni, u_int16_t seq,
            u_int16_t status, u_int8_t *challenge,u_int16_t challenge_len);
int mlme_create_infra_bss(struct ieee80211vap *vap, u_int8_t restart);
int ieee80211_mlme_create_infra_continue(struct ieee80211vap *vap);
int mlme_recv_auth_ap(struct ieee80211_node *ni,
                       u_int16_t algo, u_int16_t seq, u_int16_t status_code,
                       u_int8_t *challenge, u_int8_t challenge_length, wbuf_t wbuf,
                       const struct ieee80211_rx_status *rx_status);
int mlme_recv_auth_ap_defer(struct ieee80211_node *ni, bool create_new_node,
        struct recv_auth_params_defer *auth_params, int peer_delete_inprogress);
int mlme_auth_peer_delete_handler(struct ieee80211vap *vap, struct ieee80211_node *ni);
#ifdef AST_HKV1_WORKAROUND
int mlme_auth_wds_delete_resp_handler(void *psoc_obj,
                                      struct recv_auth_params_defer *auth_params);
#endif
int
ieee80211_recv_asreq(struct ieee80211_node *ni, wbuf_t wbuf, int subtype);
void ieee80211_recv_beacon_ap(struct ieee80211_node *ni, wbuf_t wbuf, int subtype,
                               struct ieee80211_rx_status *rs, ieee80211_scan_entry_t  scan_entry);
void ieee80211_recv_ctrl_ap(struct ieee80211_node *ni, wbuf_t wbuf,
						int subtype);
void ieee80211_inact_timeout_ap(struct ieee80211vap *vap);
void ieee80211_mlme_node_pwrsave_ap(struct ieee80211_node *ni, int enable);
void ieee80211_mlme_node_leave_ap(struct ieee80211_node *ni);
int ieee80211_mlme_ap_set_beacon_suspend_state(struct ieee80211vap *vap, bool en_suspend);

    int ieee80211_recv_probereq(struct ieee80211_node *ni, wbuf_t wbuf, int subtype,
                                struct ieee80211_rx_status *rs);

void mlme_sta_vattach(struct ieee80211vap *vap);
void mlme_sta_vdetach(struct ieee80211vap *vap);
void mlme_get_linkrate(struct ieee80211_node *ni, u_int32_t* rxlinkspeed, u_int32_t* txlinkspeed);
int mlme_recv_auth_sta(struct ieee80211_node *ni,
                         u_int16_t algo, u_int16_t seq, u_int16_t status_code,
                         u_int8_t *challenge, u_int8_t challenge_length, wbuf_t wbuf);
void ieee80211_recv_beacon_sta(struct ieee80211_node *ni, wbuf_t wbuf, int subtype,
                               struct ieee80211_rx_status *rs, ieee80211_scan_entry_t  scan_entry);
void ieee80211_process_csa(struct ieee80211_node *ni,
                           struct ieee80211_ath_channel *chan,
                           struct ieee80211_ath_channelswitch_ie *chanie,
                           struct ieee80211_extendedchannelswitch_ie *echanie, u_int8_t *update_beacon);

int ieee80211_get_avg_rssi(ieee80211_scan_entry_t  scan_entry, struct ieee80211vap *vap);
int ieee80211_recv_asresp(struct ieee80211_node *ni, wbuf_t wbuf, int subtype);
void ieee80211_inact_timeout_sta(struct ieee80211vap *vap);
void mlme_sta_connection_reset(struct ieee80211vap *vap);
bool ieee80211_check_wpaie(struct ieee80211vap *vap, u_int8_t *iebuf, u_int32_t length);
void mlme_sta_reset_bmiss(struct ieee80211vap *vap);
void ieee80211_update_noderates(struct ieee80211_node *ni);
int ieee80211_recv_addts_req(struct ieee80211_node *ni, struct ieee80211_wme_tspec *tspec, int dialog_token);
void ieee80211_update_ni_chwidth(uint8_t chwidth, struct ieee80211_node *ni, struct ieee80211vap *vap);
void ieee80211_update_ht_vht_he_phymode(struct ieee80211com *ic, struct ieee80211_node *ni);
void ieee80211_update_vap_shortgi(struct ieee80211_node *ni);
void ieee80211_readjust_chwidth(struct ieee80211_node *ni,
        struct ieee80211_ie_vhtop *ap_vhtop,
        struct ieee80211_ie_htinfo_cmn *ap_htinfo,
        struct ieee80211_ie_vhtcap *ap_vhtcap);

int ieee80211_recv_delts_req(struct ieee80211_node *ni, struct ieee80211_wme_tspec *tspec);

#define MLME_SWBMISS_MAX_REQUESTORS  4
#define MLME_SWBMISS_MAX_REQ_NAME   16
#define    IEEE80211_MLME_STA_PRIVATE                                                                                          \
    os_timer_t                      im_sw_bmiss_timer;                   /* STA swbmiss timer */                               \
    u_int32_t                       im_sw_bmiss_timer_interval;          /* swbmiss timer interval in ms */                    \
    u_int32_t                       im_sw_bmiss_timer_interval_normal;   /* swbmiss timer interval in ms when sta is sleep */  \
    u_int32_t                       im_sw_bmiss_timer_interval_sleep;    /* swbmiss timer interval in ms */                    \
    u_int32_t                       im_sw_bmiss_id_bitmap;               /* swbmiss timer control id bitmaps */                \
    u_int32_t                       im_sw_bmiss_disable_bitmap;               /* swbmiss timer control id enable bitmaps */         \
    u_int8_t                        im_sw_bmiss_id_name[MLME_SWBMISS_MAX_REQUESTORS][MLME_SWBMISS_MAX_REQ_NAME]; /* names of the requestorsi */                      \
    ieee80211_pwrsave_mode          im_sleep_mode;                       /* sta sleep mode    */                               \


void mlme_sta_bmiss_ind(wlan_if_t vap);
void ieee80211_vap_iter_beacon_miss(void *arg, wlan_if_t vap);

#if UMAC_SUPPORT_SW_BMISS
void mlme_sta_swbmiss_timer_restart(struct ieee80211vap *vap);
void mlme_sta_swbmiss_timer_start(struct ieee80211vap *vap);
void mlme_sta_swbmiss_timer_stop(struct ieee80211vap *vap);
void mlme_sta_swbmiss_timer_attach(struct ieee80211vap *vap);
void mlme_sta_swbmiss_timer_detach(struct ieee80211vap *vap);
#define mlme_sta_swbmiss_active(vap) ieee80211_vap_sw_bmiss_is_set(vap)
u_int32_t mlme_sta_swbmiss_timer_alloc_id(struct ieee80211vap *vap, int8_t *requestor_name);
int mlme_sta_swbmiss_timer_free_id(struct ieee80211vap *vap, u_int32_t id);
int mlme_sta_swbmiss_timer_enable(struct ieee80211vap *vap, u_int32_t id);
int mlme_sta_swbmiss_timer_disable(struct ieee80211vap *vap, u_int32_t id);
void mlme_sta_swbmiss_timer_print_status(struct ieee80211vap *vap);
#else
#define mlme_sta_swbmiss_timer_restart(vap)                /**/
#define mlme_sta_swbmiss_timer_start(vap)                  /**/
#define mlme_sta_swbmiss_timer_stop(vap);                  /**/
#define mlme_sta_swbmiss_timer_attach(vap)                 /**/
#define mlme_sta_swbmiss_timer_detach(vap)                 /**/
#define mlme_sta_swbmiss_active(vap)                       0
#define mlme_sta_swbmiss_timer_alloc_id(vap)               0
#define mlme_sta_swbmiss_timer_free_id(vap)                EINVAL
#define mlme_sta_swbmiss_timer_enable(vap,id)              EINVAL
#define mlme_sta_swbmiss_timer_disable(vap,id)             EINVAL
#define mlme_sta_swbmiss_timer_print_status(vap)           /**/
#endif

#if UMAC_SUPPORT_BTAMP
void ieee80211_recv_beacon_btamp(struct ieee80211_node *ni, wbuf_t wbuf, int subtype,
                                 struct ieee80211_rx_status *rs, ieee80211_scan_entry_t  scan_entry);
int mlme_recv_auth_btamp(struct ieee80211_node *ni,
                          u_int16_t algo, u_int16_t seq, u_int16_t status_code,
                          u_int8_t *challenge, u_int8_t challenge_length, wbuf_t wbuf);
void ieee80211_mlme_join_complete_btamp(struct ieee80211_node *ni);
#else
#define ieee80211_recv_beacon_btamp(_ni, _wbuf, _subtype, _rs, _scan_entry)    /**/
#define mlme_recv_auth_btamp(ni,algo,seq,status_code,challenge,challenge_length,wbuf)  (0)
#endif

#define IEEE80211_MAX_MLME_EVENT_HANDLERS 5

void ieee80211_mlme_deliver_event(struct ieee80211_mlme_priv *mlme_priv, ieee80211_mlme_event *event);

struct ieee80211_ath_channel* ieee80211_derive_chan(struct ieee80211vap *vap);
void ieee80211_process_beacon_for_ru26_update(struct ieee80211vap *vap,
                                           wlan_scan_entry_t scan_entry);

void ieee80211_mlme_frame_complete_handler(wlan_if_t vap, wbuf_t wbuf,void *arg,
        u_int8_t *dst_addr, u_int8_t *src_addr, u_int8_t *bssid,
        ieee80211_xmit_status *ts);

struct ieee80211_mlme_priv {
    struct ieee80211vap             *im_vap;
    osdev_t                         im_osdev;

    os_timer_t                      im_timeout_timer;

#if ATH_SUPPORT_DFS && ATH_SUPPORT_STA_DFS
    os_timer_t                      im_stacac_timeout_timer;
    u_int8_t                        im_is_stacac_running;
#endif

    u_int32_t                       im_timeout;
    enum mlme_req                   im_request_type;    /* type of mlme request in progress */
    u_int8_t                        im_connection_up;   /* connection state (IBSS/AP) */

    /* Join related variables */
    atomic_t                        im_join_wait_beacon_to_synchronize;

    /* Authentication related variables */
    u_int16_t                       im_expected_auth_seq_number;

    /* event handlers data */
    void*                           im_event_handler_arg[IEEE80211_MAX_MLME_EVENT_HANDLERS];
    ieee80211_mlme_event_handler    im_event_handler[IEEE80211_MAX_MLME_EVENT_HANDLERS];

    int                             im_beacon_tx_suspend; /* suspend beacon transmission */
    /* Association related variables */

    /* IBSS related variables */
    IEEE80211_MLME_IBSS_PRIVATE

    /* STA related variables */
    IEEE80211_MLME_STA_PRIVATE

    /* Configuration */
    u_int32_t                       im_disassoc_timeout;    /* in  ms */
#if DBDC_REPEATER_SUPPORT
    u_int8_t                        im_sta_connection_up;   /* connection state (DBDC STA) */
    os_timer_t                      im_drop_mcast_timer;
#endif
    u_int8_t                        im_join_is_timeout;
};

/*
 * function to start offchannel mgmt tx/rx
 * @param vap_handle  : handle to the vap
 * @param dst_addr    : destination mac address
 * @param src_addr    : src mac address
 * @param bssid       : bssid
 * @param data        : frame pointer
 * @param data_len    : frame len
 * @param offchan_req : offchannel request
 */
int wlan_offchan_mgmt_tx_start(wlan_if_t vap_handle, const u_int8_t *dst_addr, const u_int8_t *src_addr,
                               const u_int8_t *bssid, const u_int8_t *data, u_int32_t data_len,
                               struct ieee80211_offchan_req *offchan_req);
int wlan_remain_on_channel(wlan_if_t vap,
                           struct ieee80211_offchan_req *offchan_req);
int wlan_cancel_remain_on_channel(wlan_if_t vap);
#endif /* end of _IEEE80211_MLME_PRIV_H */
