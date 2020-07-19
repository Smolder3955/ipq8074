/*
 * Copyright (c) 2017-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 */
#ifndef _IEEE80211_CBS_H
#define _IEEE80211_CBS_H

#define DEBUG_CBS 1

#if DEBUG_CBS

#define cbs_trace(data...)  do {          \
                    printk("%s ",__func__);        \
                    printk (data) ;                \
} while (0)

#else
#define cbs_trace(data)
#endif

#define CBS_DEFAULT_RESTTIME 500 /* msec */
#define CBS_DEFAULT_DWELL_TIME 50 /* msec */
#define CBS_DEFAULT_WAIT_TIME 1000 /* 1 sec */
#define CBS_DEFAULT_DWELL_SPLIT_TIME 50 /* msec */
#define CBS_DEFAULT_DWELL_REST_TIME 500 /* msec */
#define CBS_DEFAULT_MIN_REST_TIME 50 /* msec */

#define IEEE80211_CBS_ENABLE 0x1  /* Command to enable/disable CBS */
#define IEEE80211_CBS_DWELL_SPLIT_TIME 0x2 /* Command to configure Dwell split time */
#define IEEE80211_CBS_DWELL_REST_TIME 0x3 /* command to configure Dwell rest time */
#define IEEE80211_CBS_REST_TIME 0x4 /* command to configure rest time between chans */
#define IEEE80211_CBS_WAIT_TIME 0x5 /* command to configure wait time after a scan */
#define IEEE80211_CBS_CSA_ENABLE 0x6 /* command to enable channel switching in cbs */

typedef struct ieee80211_cbs    *ieee80211_cbs_t;

enum ieee80211_cbs_state {
    IEEE80211_CBS_INIT=0,
    IEEE80211_CBS_SCAN,
    IEEE80211_CBS_REST,
    IEEE80211_CBS_RANK,
    IEEE80211_CBS_WAIT,
    IEEE80211_CBS_STATS,
};

enum ieee80211_cbs_event {
    IEEE80211_CBS_SCAN_START=1,
    IEEE80211_CBS_SCAN_NEXT,
    IEEE80211_CBS_SCAN_CONTINUE,
    IEEE80211_CBS_DWELL_SPLIT,
    IEEE80211_CBS_SCAN_CANCEL_SYNC,
    IEEE80211_CBS_SCAN_CANCEL_ASYNC,
    IEEE80211_CBS_SCAN_COMPLETE,
    IEEE80211_CBS_RANK_START,
    IEEE80211_CBS_RANK_COMPLETE,
    IEEE80211_CBS_DCS_INTERFERENCE,
    IEEE80211_CBS_STATS_COLLECT,
    IEEE80211_CBS_STATS_COMPLETE,
};

struct ieee80211_cbs {
    wlan_dev_t                          cbs_ic;
    wlan_if_t                           cbs_vap;
    osdev_t                             cbs_osdev;

    spinlock_t                          cbs_lock;
    os_timer_t                          cbs_timer;

    enum ieee80211_cbs_state            cbs_state;

    IEEE80211_SCAN_REQUESTOR            cbs_scan_requestor;    /* requestor id assigned by scan module */
    IEEE80211_SCAN_ID                   cbs_scan_id;           /* scan id assigned by scan scheduler */

    uint32_t                            dwell_time;
    uint32_t                            rest_time;
    uint32_t                            wait_time;
    uint32_t                            scan_intvl_time;

    struct scan_start_request           scan_params;
    uint32_t                            chan_list[IEEE80211_CHAN_MAX];
    uint32_t                            nchans;
    uint32_t                            chan_list_idx;

    uint8_t                             cbs_enable;
    uint8_t                             cbs_csa;

    uint8_t                             max_dwell_split_cnt;
    int8_t                              dwell_split_cnt;
    uint32_t                            scan_offset[8];      /*Array of offset times in ms*/
    uint32_t                            scan_dwell_rest[8];  /*Array of dwell rest times in ms*/
    uint32_t                            min_dwell_rest_time;  /*Minimum time between two successive scans in same channel in integral multiples of 100 ms*/
    uint32_t                            dwell_split_time;
    uint8_t                             max_arr_size_used;    /*Maximum array index used in both offset and dwell rest time arrays*/

    qdf_work_t                          cbs_work;         /* work queue for processing events */
    enum ieee80211_cbs_event            cbs_event;
};


int ieee80211_cbs_attach(ieee80211_cbs_t *cbs, wlan_dev_t devhandle, osdev_t osdev);
int ieee80211_cbs_detach(ieee80211_cbs_t *cbs);

int wlan_bk_scan(struct ieee80211com *ic);
void wlan_bk_scan_stop(struct ieee80211com *ic);
void wlan_bk_scan_stop_async(struct ieee80211com *ic);
int ieee80211_cbs_set_param(ieee80211_cbs_t cbs, int param , int val);
int ieee80211_cbs_get_param(ieee80211_cbs_t cbs, int param );
int ieee80211_cbs_api_change_home_channel(ieee80211_cbs_t cbs);
#endif /* _IEEE80211_CBS_H */
