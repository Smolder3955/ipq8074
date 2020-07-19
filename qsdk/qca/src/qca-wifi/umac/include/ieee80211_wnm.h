/*
 * Copyright (c) 2011, 2017-2018 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * Copyright (c) 2009 Atheros Communications Inc.  All rights reserved.
 */


#include <ieee80211_options.h>

#ifndef _IEEE80211_WNM_H_
#define _IEEE80211_WNM_H_

#if UMAC_SUPPORT_WNM
#define MAX_BSSID_STORE         32 /* Maximum number of bssid-pref pairs AP can store */

#define FMS_MAX_STREAM_IDS      (255)
#define FMS_MAX_COUNTERS        (8)
/* Max FMS IE len in the Beacon Frame.
 * This will limit the Max FMS streams we support due to the FMSIDs field
 * in the IE */

#define FMS_MAX_IE (3 + (FMS_MAX_COUNTERS * 1) + FMS_MAX_STREAM_IDS)
#define IEEE80211_BSSLOAD_WAIT                  1000  /* inactivity interval in msec */
#define IEEE80211_BSS_IDLE_PERIOD_DEFAULT       10


enum ieee80211_wnm_msg {
    IEEE80211_WNM_SLEEP_ENTER_CONFIRM,
    IEEE80211_WNM_SLEEP_ENTER_FAIL,
    IEEE80211_WNM_SLEEP_EXIT_CONFIRM,
    IEEE80211_WNM_SLEEP_EXIT_FAIL,
    IEEE80211_WNM_SLEEP_TFS_REQ_IE_ADD,
    IEEE80211_WNM_SLEEP_TFS_REQ_IE_NONE,
    IEEE80211_WNM_SLEEP_TFS_REQ_IE_SET,
    IEEE80211_WNM_SLEEP_TFS_RESP_IE_ADD,
    IEEE80211_WNM_SLEEP_TFS_RESP_IE_NONE,
    IEEE80211_WNM_SLEEP_TFS_RESP_IE_SET,
    IEEE80211_WNM_SLEEP_TFS_IE_DEL,
};

struct ieee80211_fms_counters {
#if _BYTE_ORDER == _BIG_ENDIAN
    u_int8_t    count:5;
    u_int8_t    id:3;
#else
    u_int8_t    id:3;
    u_int8_t    count:5;
#endif
} __packed;

struct ieee80211_fms_desc_ie {
    u_int8_t    id;         /* IEEE80211_ELEMID_FMS_DESCRIPTOR */
    u_int8_t    len;
    u_int8_t    numcounters;
    struct   ieee80211_fms_counters    fmscounters[1]; /* 0 or more counters */
    /* 0 or more FMSIDs follow */
} __packed;

#define WNM_BSS_IDLE_PROTECTED_KEEPALIVE    BIT(0)
#define WNM_BSS_IDLE_PROT_ENABLE_MASK       0x1
#define WNM_BSS_IDLE_PROT_FORCE_MASK        0x2

struct ieee80211_wnm
{
    osdev_t                 wnm_osdev;
    wlan_if_t               wnm_vap;
    struct ieee80211_node   *ni;
    u_int16_t               wnm_bss_max_idle_period;  /* storing idle period */
    u_int8_t                wnm_bss_idle_option;      /* proctedted and non protected option */
    u_int8_t                wnm_timbcast_counter;
    u_int8_t                wnm_timbcast_interval;
    u_int32_t               wnm_timbcast_offset;
    u_int8_t                wnm_timbcast_enable;
    u_int16_t               wnm_timbcast_highrate;
    u_int16_t               wnm_timbcast_lowrate;
    u_int8_t                wnm_check_beacon;
    u_int8_t                wnm_bss:1,
                            wnm_tfs:1,
                            wnm_tim:1,
                            wnm_sleep:1,
                            wnm_fms:1;
    struct sock             *wnm_nl_sock;
    u_int8_t                wnm_fms_desc_ie[FMS_MAX_IE];
    struct ieee80211_fms    *wnm_fms_data;
    u_int64_t               wnm_bssterm_tsf;      /* BSS Termination TSF in the future */
    u_int16_t               wnm_bssterm_duration; /* BSS Termination Duration */
    u_int8_t                wnm_bss_pref;         /* BSS Preference */
    /*
     * To store in AP all the mac-pref_val pairs entered by user
     */
    struct ieee80211_user_bssid_pref      store_userbssidpref_in_ap[MAX_BSSID_STORE];
    int count; /* count of bssid-pref_value pairs stored till now */
};

#define WNM_FLAG_FUNCS(xyz) \
     static INLINE int ieee80211_wnm_##xyz##_is_set (struct ieee80211_wnm *_wnm) { \
        return (_wnm->wnm_##xyz == 1); \
     } \
     static INLINE int ieee80211_wnm_##xyz##_is_clear (struct ieee80211_wnm *_wnm) { \
        return (_wnm->wnm_##xyz == 0); \
     } \
     static INLINE void ieee80211_wnm_##xyz##_set (struct ieee80211_wnm *_wnm) { \
        _wnm->wnm_##xyz =1; \
     } \
     static INLINE void  ieee80211_wnm_##xyz##_clear (struct ieee80211_wnm *_wnm) { \
        _wnm->wnm_##xyz = 0; \
     } 

WNM_FLAG_FUNCS(bss);
WNM_FLAG_FUNCS(tfs);
WNM_FLAG_FUNCS(tim);
WNM_FLAG_FUNCS(sleep);
WNM_FLAG_FUNCS(fms);

/* BSS transition management query */
typedef struct ieee80211_wnm    *ieee80211_wnm_t;

struct ieee80211_action_bstm_query {
    struct ieee80211_action             header;
    u_int8_t                            dialogtoken;
    u_int16_t                           reason;
    u_int8_t                            opt_ies[0];
} __packed;

#define WNM_BSSTM_REQMODE_CANDIDATE_LIST    BIT(0)
#define WNM_BSSTM_REQMODE_ABRIDGED          BIT(1)
#define WNM_BSSTM_REQMODE_DISASSOC_IMM      BIT(2)
#define WNM_BSSTM_REQMODE_BSSTERM_INC       BIT(3)
#define WNM_BSSTM_REQMODE_ESS_DISASSOC_IMM  BIT(4)

/* BSS transition management request */
struct ieee80211_action_bstm_req {
    struct ieee80211_action             header;
    u_int8_t                            dialogtoken;
#if _BYTE_ORDER == _BIG_ENDIAN
    u_int8_t                            mode_reserved:3;      /* B5 -B7 */
    u_int8_t                            mode_ess_disassoc:1;  /* B4 */
    u_int8_t                            mode_term_inc:1;      /* B3 */
    u_int8_t                            mode_disassoc:1;      /* B2 */
    u_int8_t                            mode_abridged:1;      /* B1 */
    u_int8_t                            mode_candidate_list:1;/* B0 */
#else
    u_int8_t                            mode_candidate_list:1;/* B0 */
    u_int8_t                            mode_abridged:1;      /* B1 */
    u_int8_t                            mode_disassoc:1;      /* B2 */
    u_int8_t                            mode_term_inc:1;      /* B3 */
    u_int8_t                            mode_ess_disassoc:1;  /* B4 */
    u_int8_t                            mode_reserved:3;      /* B5 -B7 */
#endif
    u_int16_t                           disassoc_timer;
    u_int8_t                            validity_itvl;
    u_int8_t                            opt_ies[0];
} __packed;

struct ieee80211_action_bstm_resp {
    struct ieee80211_action             header;
    u_int8_t                            dialogtoken;
    u_int8_t                            status;
    u_int8_t                            bssterm_delay;
    u_int8_t                            opt_ies[0];
} __packed;

/**
 * Attach function for wnm module 
 * param ic               : pointer to ic structure 
 * param vap              :pointer to vap structure 
 * @return on success return 0.
 *         on failure returns -ve value.
 */
int ieee80211_wnm_vattach (struct ieee80211com *ic,ieee80211_vap_t vap);

/**
 * Detach  function for wnm module 
 * param vap              :pointer to vap structure 
 * @return on success return 0.
 *         on failure returns -ve value.
 */

int ieee80211_wnm_vdetach(ieee80211_vap_t vap);

/* Function to Receive and Parse WNM Action Frames
 * param vap        : Pointer to vap structure
 * ni               : POinter to node from which Action frame received
 * actionCode       : Action Code
 * frm              : Pointer to start of Action Element
 * frm_len          : Length of Element
 */ 
int ieee80211_wnm_recv_action(wlan_if_t vap, wlan_node_t ni, u_int8_t action,
                                  u_int8_t *frm, u_int8_t frm_len);

/**
   caluates inactivity of the station with based on there 
    last recevid packet **/

int ieee80211_wnm_bss_validate_inactivity(struct ieee80211com *ic);

int ieee80211_tfs_filter(wlan_if_t vap, wbuf_t);


/** updates receive packet time based on protection **/

void ieee80211_wnm_bssmax_updaterx(struct ieee80211_node *ni, int secured);

/* bss max element created for sending it in assoc resp */

u_int8_t *ieee80211_add_bssmax(u_int8_t *frm, struct ieee80211vap *vap, int prot);


int wnm_netlink_init(ieee80211_wnm_t wnm);

int wnm_netlink_send(struct ieee80211vap *vap, wnm_netlink_event_t *event);

int wnm_netlink_delete(ieee80211_wnm_t wnm);

void ieee80211_wnm_parse_bssmax_ie(struct ieee80211_node *ni, u_int8_t *frm);

u_int8_t *ieee80211_add_timreq_ie(wlan_if_t vap, wlan_node_t ni,
                                               u_int8_t *frm);
u_int8_t *ieee80211_add_timresp_ie(wlan_if_t vap, wlan_node_t ni,
                                               u_int8_t *frm);
int ieee80211_parse_timreq_ie(u_int8_t *frm, u_int8_t *efrm, wlan_node_t ni);
int ieee80211_parse_timresp_ie(u_int8_t *frm, u_int8_t *efrm, wlan_node_t ni);
int ieee80211_timbcast_get_highrate(wlan_if_t vap);
int ieee80211_timbcast_get_lowrate(wlan_if_t vap);
int ieee80211_timbcast_lowrateenable(wlan_if_t vap);
int ieee80211_timbcast_highrateenable(wlan_if_t vap);
int ieee80211_wnm_tim_incr_checkbeacon(wlan_if_t vap);

void ieee80211_fms_filter(struct ieee80211_node *ni, wbuf_t wbuf, 
                          int ether_type, struct ip_header *ip,int hdrsize);
int ieee80211_wnm_setup_fmsdesc_ie(struct ieee80211_node *ni, int dtim, 
                                   uint8_t **fmsie, u_int8_t *fmsie_len, 
                                   u_int32_t *fms_counter_mask);

#define BSSTRANS_LIST_VALID_ITVL  100
#define IEEE80211_TCLAS_SUB_ELEMID  1
void
ieee80211_wnm_add_extcap(struct ieee80211_node *ni, u_int32_t *ext_capflags);

wbuf_t ieee80211_timbcast_alloc(struct ieee80211_node *ni);
int
ieee80211_timbcast_update(struct ieee80211_node *ni,
                        struct ieee80211_beacon_offsets *bo, wbuf_t wbuf);
int ieee80211_wnm_timbcast_cansend(wlan_if_t vap);

int ieee80211_wnm_timbcast_enabled(wlan_if_t vap);

/* WNM Sleep related */
int ieee80211_wnm_forward_action_app(wlan_if_t vap, wlan_node_t ni, wbuf_t wbuf,
                                int subtype, struct ieee80211_rx_status *rs);
int ieee80211_wnm_set_appie(wlan_if_t vap, u8 *buf, int len);
int ieee80211_wnm_get_appie(wlan_if_t vap, u_int8_t *buf,
                            u_int32_t *ielen, u_int32_t buflen);
int ieee80211_wnm_sleepreq_to_app(wlan_if_t vap, u_int8_t action, u_int16_t intval);

int ieee80211_wnm_fms_enabled(wlan_if_t vap);
#else

static inline
u_int8_t *ieee80211_add_bssmax(u_int8_t *frm, struct ieee80211vap *vap, int prot)
{
    return frm;
}

static inline 
int ieee80211_wnm_recv_action(wlan_if_t vap, wlan_node_t ni, u_int8_t action,
                                  u_int8_t *frm, u_int8_t frm_len) {
    return 0;
}

static inline 
u_int8_t *ieee80211_add_timreq_ie(wlan_if_t vap, wlan_node_t ni,
                                               u_int8_t *frm)
{
    return frm;
}

static inline
u_int8_t *ieee80211_add_timresp_ie(wlan_if_t vap, wlan_node_t ni,
                                               u_int8_t *frm)
{
    return frm;
}

static inline
int ieee80211_parse_timreq_ie(u_int8_t *frm, u_int8_t *efrm, wlan_node_t ni)
{
    return 0;
}

static inline
int ieee80211_parse_timresp_ie(u_int8_t *frm, u_int8_t *efrm, wlan_node_t ni)
{
    return 0;
}

static inline 
int ieee80211_tfs_filter(wlan_if_t vap, wbuf_t wbuf)
{
    return 1;
}

/* WNM Sleep related */
static inline
int ieee80211_wnm_forward_action_app(wlan_if_t vap, wlan_node_t ni, wbuf_t wbuf,
                                int subtype, struct ieee80211_rx_status *rs)
{
    return 0;
}

static inline
int ieee80211_wnm_set_appie(wlan_if_t vap, u8 *buf, int len)
{
    return 0;
}

static inline
int ieee80211_wnm_get_appie(wlan_if_t vap, u_int8_t *buf,
                            u_int32_t *ielen, u_int32_t buflen)
{
    return 0;
}

static inline
int ieee80211_wnm_sleepreq_to_app(wlan_if_t vap, u_int8_t action, u_int16_t intval)
{
    return 0;
}

#define ieee80211_parse_bssmax_ie(ni, frm)                  (NULL)


#define ieee80211_wnm_add_extcap(ni, ext_capflags)
#define ieee80211_timbcast_get_highrate(vap)      (NULL)
#define ieee80211_timbcast_get_lowrate(vap)       (NULL)
#define ieee80211_timbcast_lowrateenable(vap)     (NULL)
#define ieee80211_timbcast_highrateenable(vap)    (NULL)
#define ieee80211_wnm_tim_incr_checkbeacon(vap)   (NULL)




#define ieee80211_timbcast_alloc(ni) (NULL)


#define ieee80211_timbcast_update(ni, bo, wbuf) (-1)
#define ieee80211_wnm_timbcast_cansend(vap) (-1)
#define ieee80211_wnm_timbcast_enabled(vap) (0)
#define ieee80211_wnm_fms_enabled(vap) (0)
#define ieee80211_fms_filter(ni, wbuf, ether_type, ip, hdrsize) 
#define ieee80211_wnm_vattach(ic, vap)
#define ieee80211_wnm_vdetach(vap)
#endif
#endif /* _IEEE80211_WNM_H_ */
