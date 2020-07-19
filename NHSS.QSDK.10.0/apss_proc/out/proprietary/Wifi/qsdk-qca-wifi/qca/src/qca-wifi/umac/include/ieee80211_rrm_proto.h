/*
 *  Copyright (c) 2014,2017 Qualcomm Innovation Center, Inc.
 *  All Rights Reserved.
 *  Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 *  2014 Qualcomm Atheros, Inc.  All rights reserved.
 *
 *  Qualcomm is a trademark of Qualcomm Technologies Incorporated, registered in the United
 *  States and other countries.  All Qualcomm Technologies Incorporated trademarks are used with
 *  permission.  Atheros is a trademark of Qualcomm Atheros, Inc., registered in
 *  the United States and other countries.  Other products and brand names may be
 *  trademarks or registered trademarks of their respective owners.
 */

#include <qdf_defer.h>
#ifndef _IEEE80211_RRM_PROTO_H_
#define _IEEE80211_RRM_PROTO_H_ 

#define IEEE80211_ACTION_CAT_RM    5   /* Radio measurement */
#define IEEE80211_COLOCATED_BSSID_MAX_LEN         196

struct ieee80211_nrresp_info
{
    u_int8_t channum;
    u_int8_t phytype;
    u_int8_t regclass;
    u_int32_t capinfo;
    u_int8_t bssid[6];
    u_int8_t preference;
    u_int8_t meas_count;
    u_int8_t meas_token[2];
    u_int8_t meas_req_mode[2];
    u_int8_t meas_type[2];
    u_int8_t loc_sub[2];
    u_int8_t chwidth;
    u_int8_t cf_s1;
    u_int8_t cf_s2;
    u_int8_t is_ht;
    u_int8_t is_vht;
    u_int8_t is_ftm;
    u_int8_t is_he;
    u_int8_t is_he_er_su;
    u_int8_t colocated_bss[IEEE80211_COLOCATED_BSSID_MAX_LEN];
};

#if UMAC_SUPPORT_RRM

#define IEEE80211_ELEMID_NEIGHBOR_REPORT          52

/* rrm call back struct */
struct ieee80211_rrm_cbinfo {
    u_int8_t* frm;
    u_int8_t ssid[IEEE80211_NWID_LEN];
    u_int8_t  ssid_len;
    u_int8_t* essid;
    u_int8_t  essid_len;
    u_int8_t  bssid[6];
    u_int16_t duration;
    u_int16_t chnum;
    u_int8_t dialogtoken;
    u_int8_t preference;
    u_int8_t meas_count;
    u_int8_t meas_token[2];
    u_int8_t meas_req_mode[2];
    u_int8_t meas_type[2];
    u_int8_t loc_sub[2];
    int32_t max_pktlen;
};

#define SLIDING_WINDOW   120
#define GRANUALITY_TIMER  1
#define STATS_TIMER       10
#define SLIDING_WINDOW_SIZE SLIDING_WINDOW/STATS_TIMER
#define RSSI_CLASS_MAX 7
#define TIME_SEC_TO_MSEC(sec)   (sec*1000)

/* Noise floor value to use when converting from RSSI in dB to RSSI in dBm */
#define RRM_DEFAULT_NOISE_FLOOR -95

struct ieee80211_beacon_report {
    u_int8_t reg_class;
    u_int8_t ch_num;
    u_int8_t ms_time[8];
    u_int16_t mduration;
    u_int8_t frame_info;
    int8_t rcpi;
    u_int8_t rsni;
    u_int8_t bssid[6];
    u_int8_t ant_id;
    u_int8_t parent_tsf[4];
    u_int8_t ies[1];
}__packed;

struct ieee80211_beacon_entry {
    TAILQ_ENTRY(ieee80211_beacon_entry) blist;
    struct ieee80211_beacon_report report;
};

struct ieee80211_beacon_report_table
{
    spinlock_t                              lock;     /* on scan table */
    TAILQ_HEAD(, ieee80211_beacon_entry)    entry;    /* head all entries */
};

struct vodelay
{
    u_int8_t  sliding_vo[SLIDING_WINDOW_SIZE];
    u_int8_t  min;
    u_int8_t  max;
    u_int8_t  avg;
    u_int8_t  last;
    u_int8_t index;
    u_int8_t valid_entries;
};
struct anpi
{
    int8_t  sliding_anpi[SLIDING_WINDOW_SIZE];
    int8_t  min;
    int8_t  max;
    int8_t  avg;
    int8_t  last;
    int8_t index;
    int8_t valid_entries;
};

struct bedelay
{
    u_int8_t  sliding_be[SLIDING_WINDOW_SIZE];
    u_int8_t  min;
    u_int8_t  max;
    u_int8_t  avg;
    u_int8_t  last;
    u_int8_t index;
    u_int8_t valid_entries;
};

struct frmcnt
{
    u_int32_t  sliding_frmcnt[SLIDING_WINDOW_SIZE];
    u_int32_t  min;
    u_int32_t  max;
    u_int32_t  avg;
    u_int32_t  last;
    u_int32_t index;
    u_int32_t valid_entries;
};
struct ackfail
{
    u_int32_t  sliding_ackfail[SLIDING_WINDOW_SIZE];
    u_int32_t  min;
    u_int32_t  max;
    u_int32_t  avg;
    u_int32_t  last;
    u_int32_t index;
    u_int32_t valid_entries;
};
struct per
{
    u_int32_t  min;
    u_int32_t  max;
    u_int32_t  avg;
    u_int32_t  last;
};
struct stcnt{
    u_int8_t  sliding_stcnt[SLIDING_WINDOW_SIZE];
    u_int8_t  min;
    u_int8_t  max;
    u_int8_t  avg;
    u_int8_t  last;
    u_int8_t index;
    u_int8_t valid_entries;
};
struct chload
{
    u_int8_t  sliding_chload[SLIDING_WINDOW_SIZE];
    u_int8_t  min;
    u_int8_t  max;
    u_int8_t  avg;
    u_int8_t  last;
    u_int8_t index;
    u_int8_t valid_entries;
    u_int8_t pre_trap_val;
};

struct sliding_window
{
    struct chload    chload_window;
    struct bedelay   be_window;
    struct vodelay   vo_window;
    struct anpi      anpi_window;
    struct frmcnt    frmcnt_window;
    struct ackfail   ackfail_window;
    struct per       per_window;
    struct stcnt     stcnt_window;
};
struct ieee80211_mbo_info {
     u_int8_t* frm;
     u_int8_t* ssid;
     u_int8_t  ssid_len;
     u_int8_t  num_of_channels;
     u_int8_t  supported_channels[MAX_NUM_CHANNEL_SUPPORTED];
     u_int8_t  channels[MAX_NUM_CHANNEL_SUPPORTED];
     u_int8_t  channels_preference;
};

typedef struct ieee80211_rrm
{
    u_int32_t                           rrm_in_progress; /* to know whether rrm is busy or not*/
    u_int32_t                           rrm_last_scan;/* last scan time */
    wlan_scan_requester                 rrm_scan_requestor;
    wlan_scan_id                        rrm_scan_id;     /* scan id assigned by scan scheduler */
    osdev_t                             rrm_osdev;
    wlan_if_t                           rrm_vap;/* vap struct */
    int8_t                              rrm_noisefloor[IEEE80211_RRM_CHAN_MAX]; /* calulation of noise */
    int8_t                              u_chload[IEEE80211_RRM_CHAN_MAX]; /*channl load storage to feed to user layer */
    ieee80211_rrm_noise_data_t          user_nhist_resp[IEEE80211_RRM_CHAN_MAX]; /* user layer feed after report received on ap side */
    ieee80211_rrm_cca_data_t            user_cca_resp[IEEE80211_RRM_CHAN_MAX]; /* user layer feed after report received on ap side */
    ieee80211_rrm_rpi_data_t            user_rpihist_resp[IEEE80211_RRM_CHAN_MAX]; /* user layer feed after report received on ap side */
    u_int32_t                           rrm_chan_load[IEEE80211_RRM_CHAN_MAX];  /*channel load calculation through scan module */
    u_int32_t                           rrm_cycle_count[IEEE80211_RRM_CHAN_MAX];
    u_int8_t                            pending_report;
    u_int8_t                            pending_report_type;
    void                                *rrmcb;
    u_int8_t                            req_chan;
    os_timer_t                          rrm_sliding_window_timer;
    uint8_t                             pre_gid; /*required in case of sta stats in timer */
    qdf_work_t                          rrm_work;
    struct sliding_window               window; /* window to mantain stats  */
    atomic_t                            timer_running;
    atomic_t                            rrm_detach;
    u_int8_t                            windowstats; /* following varaibles are for sending various traps from work queue */
    u_int8_t                            nonerptrap;
    u_int8_t                            bgjointrap;
    u_int8_t                            chloadtrap;
    u_int32_t                           rrmstats;
    u_int32_t                           slwindow;
    u_int8_t                            cochannel;
    u_int8_t                            precochannel;
    u_int8_t                            cochanneltrap;
    struct ieee80211_beacon_report_table *beacon_table;
    u_int8_t                            rrm_macaddr[IEEE80211_ADDR_LEN];    /* MAC address */
#if UMAC_SUPPORT_RRM_DEBUG
    u_int32_t                           rrm_dbg_lvl;
#endif
    qdf_spinlock_t                      rrm_lock;
}*ieee80211_rrm_t;


/**
 * rrm action frame recv handler
 * @param vaphandle         : handle to the VAP
 * @param ni                : handle to node pointer
 * @param action            : rrm action type 
 * @param frm               : pointer to action frame 
 * @param frm_len           : action frame length 
 *
 * @return on success return 0.
 *         on failure returns -ve value.
 */
int ieee80211_rrm_recv_action(wlan_if_t vap, wlan_node_t ni, 
                            u_int8_t action, u_int8_t *frm, u_int32_t frm_len);
/**
 * Add RRM capability IE
 * @param frm               : pointer to frame 
 * @param ni                : handle to node pointer
 *
 * @return : pointer to frame after adding ie
 */
u_int8_t *ieee80211_add_rrm_cap_ie(u_int8_t *frm, struct ieee80211_node *ni);

/**
 * Go through the scan list and fill 
 * beacon request response. 
 * @param arg               : opaque pointer to ieee80211_rrm_cbinfo
 * @param se                : scan entry 
 *
 * @return on success return 0.
 *         on failure returns -ve value.
 */
QDF_STATUS ieee80211_fill_bcnreq_info(void *arg, wlan_scan_entry_t se);
/**
 * Go through the scan list and fill the neighbor
 * AP's information
 * @param arg               : opaque pointer to ieee80211_rrm_cbinfo
 * @param se                : scan entry 
 *
 * @return on success return 0.
 *         on failure returns -ve value.
 */
QDF_STATUS ieee80211_fill_nrinfo(void *arg, wlan_scan_entry_t se);
int ieee80211_mbo_fill_nrinfo(void *arg, wlan_scan_entry_t se);
/**
 * Get Phy Type (as per 802.11mc draft spec) from Phy mode
 *
 * @param phy_mode          :phy mode (enumeration different from phy type)
 *
 * @return PHT TYPE enum
 */
u_int8_t ieee80211_rrm_get_phy_type(u_int16_t phy_mode);

/**
 * Get Channel width for Wide Bandwidth Channel sub element
 *
 * @param ic_flags          :flags from channel info
 *
 * @return Channel width enum
 */
u_int8_t ieee80211_rrm_get_chwidth_for_wbc(u_int64_t ic_flags);

/**
 * Add a Neighbor report IE to the frame
 * @param frm               : Pointer to position in frame at 
 *                          which to add IE
 * @param nr_info           : Neighbor info to add
 *
 * @return Updated pointer to current position in frame.
 */
u_int8_t *ieee80211_add_nr_ie(u_int8_t *frm, 
                              struct ieee80211_nrresp_info* nr_info);

/**
 * Attach function for rrm module 
 * param ic               : pointer to ic structure 
 * param vap              :pointer to vap structure 
 * @return on success return 0.
 *         on failure returns -ve value.
 */
int ieee80211_rrm_vattach (struct ieee80211com *ic,ieee80211_vap_t vap); 

/**
 * Detach  function for rrm module 
 * param vap              :pointer to vap structure 
 * @return on success return 0.
 *         on failure returns -ve value.
 */

int ieee80211_rrm_vdetach(ieee80211_vap_t vap);

/**
 *  Go through all node list and find node is rrm 
 *  capable or not.
 * param vap:pointer to vap structure 
 * param mac:pointer to mac address of node 
 * @return on success non zero value .
 *         on failure returns zero value.
 */
int ieee80211_node_is_rrm(wlan_if_t vap,char *mac);

/**
 * mark node as rrm capable  
 * param ni:pointer node structure
 * param flag:enable or disable 
 * @return :- void 
 *   
 */

void ieee80211_set_node_rrm(struct ieee80211_node *ni,uint8_t flag);

/* set rrmstats :- if enabled we should send various events to 
 * upper application(athadhoc) *
 */

#else
#define ieee80211_recv_link_measurement_req(_ni,_wbuf,_subtype)  (0)

#define ieee80211_recv_link_measurement_rsp(_ni,_wbuf,_subtype)  (0)

#define ieee80211_recv_neighbor_req(_ni, _wbuf, _subtype)        (0)

#define ieee80211_recv_neighbor_rsp(_ni,_wbuf,_subtype)          (0)

#define ieee80211_rrm_vattach(ic, vap) 

#define ieee80211_rrm_vdetach(vap) 

static inline u_int8_t *ieee80211_add_rrm_cap_ie(u_int8_t *frm, struct ieee80211_node *ni)
{ 
    return frm;
}
static inline int ieee80211_rrm_recv_action(wlan_if_t vap, wlan_node_t ni, 
                            u_int8_t action, u_int8_t *frm, u_int32_t frm_len)
{
     return EOK;
}

static inline void ieee80211_set_node_rrm(struct ieee80211_node *ni,uint8_t flag)
{
    return;
}
#endif /* UMAC_SUPPORT_RRM */

#if UMAC_SUPPORT_RRM_MISC
int ieee80211_set_rrmstats(wlan_if_t vap, int val);

/* get  rrmstats :- to get rrmstats value */
int ieee80211_get_rrmstats(wlan_if_t vap);

/* set sliding window  :- if enabled we should start internal sliding window */

int ieee80211_rrm_set_slwindow(wlan_if_t vap, int val);

/* to get vlaue of sliding window */
int ieee80211_rrm_get_slwindow(wlan_if_t vap);
#else
static inline int ieee80211_set_rrmstats(wlan_if_t vap, int val) 
{
    return EINVAL;
}
static inline int ieee80211_get_rrmstats(wlan_if_t vap) 
{
    return EINVAL;
}
static inline int ieee80211_rrm_set_slwindow(wlan_if_t vap, int val)
{
    return EINVAL;
}
static inline int ieee80211_rrm_get_slwindow(wlan_if_t vap) 
{ 
    return EINVAL;

}
#endif /*UMAC_SUPPORT_RRM_MISC*/

#if UMAC_SUPPORT_RRM_DEBUG
/**
 * @brief 
 *
 * @param vap
 * @param val
 *
 * @return 
 */
int ieee80211_rrmdbg_get(wlan_if_t vap);

/**
 * @brief 
 *
 * @param vap
 *
 * @return 
 */
int ieee80211_rrmdbg_set(wlan_if_t vap, u_int32_t val);
#else

/**
 * @brief 
 *
 * @param vap
 *
 * @return 
 */
static inline int ieee80211_rrmdbg_set(wlan_if_t vap, u_int32_t val) 
{
    return -EINVAL;
}

/**
 * @brief 
 *
 * @param vap
 *
 * @return 
 */
static inline int ieee80211_rrmdbg_get(wlan_if_t vap)
{
    return -EINVAL;
}
#endif /*UMAC_SUPPORT_RRM_DEBUG*/
#endif /* _IEEE80211_RRM_H_ */
