/*
* Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 * Copyright (c) 2009-2010, Atheros Communications
 * implements P2P protpcol layer.
 * WARNING: synchronization issues are not handled yet.
 */
/*
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */


#ifndef _IEEE80211_P2P_DEVICE_PRIV_H_
#define _IEEE80211_P2P_DEVICE_PRIV_H_

#include "ieee80211_api.h"
#include "ieee80211P2P_api.h"
#include "ieee80211_var.h"

#define IEEE80211_MAX_P2P_EVENT_HANDLERS 3
#define P2P_MIN_DWELL_TIME 0
#define P2P_MAX_DWELL_TIME 1000
#define P2P_DWELL_TIME_ACTIVE 20
#define P2P_DWELL_TIME_PASSIVE 100
#define P2P_REST_TIME       100
#define IEEE80211_P2P_NUM_SOCIAL_CHANNELS 3
#define IEEE80211_P2P_SOCIAL_CHANNELS 1,6,11
#define IEEE80211_P2P_MAX_IE_LEN   256
#define MAX_DEV_NAME_LEN           32
#define IEEE80211_P2P_MAX_BUF_LEN  2000  /* Max buffer size to fit the action frame in wbuf of 2k size */
#define IEEE80211_P2P_MAX_ACTION_FRAME  5 /* maximum number of action frames that can be queued */
#define P2P_SEND_ACTION_DWELL_TIME 20
#define IEEE80211_P2P_MAX_SEQ_TRACK 10  /* how many unassociated devices to track action frame sequence numbers from */
#define IEEE80211_P2P_SEQ_TRACK_MS 2000  /* how long to keep track of incoming action frames for hw retries */

typedef enum {
    IEEE80211_P2P_SCAN_REQ_NONE=0,
    IEEE80211_P2P_SCAN_REQ_EXTERNAL_SCAN,       /* scan requested from external layer (osshim)*/
    IEEE80211_P2P_SCAN_REQ_EXTERNAL_LISTEN,     /* listen requested from external layer (osshim) */
    IEEE80211_P2P_SCAN_REQ_EXTERNAL_SEARCH,     /* Search State requested from external layer (osshim) */
    IEEE80211_P2P_SCAN_REQ_SEND_ACTION,         /* scan to send action frames */
} ieee80211_p2p_scan_request;


struct _p2p_act_frame;
/*
 * structure to keep track of the send action frame request.
 */
typedef struct _p2p_act_frame {
    TAILQ_ENTRY(_p2p_act_frame) act_frame_list;
    u_int32_t                   freq;
    u_int32_t                   dwell_time; /* dwell time in msec */
    u_int8_t                    src[IEEE80211_ADDR_LEN];
    u_int8_t                    dst[IEEE80211_ADDR_LEN];
    u_int8_t                    bssid[IEEE80211_ADDR_LEN];
    u_int8_t                    buf[IEEE80211_P2P_MAX_BUF_LEN];
    size_t                      len;
    wlan_action_frame_complete_handler frame_complete_handler;
    u_int32_t                   p2p_act_frame_seq;
    void                        *arg;
    u_int16_t                   pad;
} ieee80211_p2p_act_frame;

typedef struct _p2p_act_frm_seq_ctr {
    u_int16_t                   last_seq;
    u_int8_t                    src[IEEE80211_ADDR_LEN];
    u_int32_t                   last_sec;
} ieee80211_p2p_act_frm_seq_ctr;

struct ieee80211p2p {
    wlan_dev_t                             p2p_devhandle;     /* dev handle, (com struct ) */
    osdev_t                                p2p_oshandle;
    char                                   p2p_dev_name[IEEE80211_MAX_P2P_DEV_NAME_LEN];
    os_mesg_queue_t                        p2p_mesg_q;
    spinlock_t                             p2p_lock;           /* lock to access this structure */

    /* P2P device vap */
    wlan_if_t                              p2p_vap;

    /* Primary and secondary device types */
    u_int8_t                               pri_dev_type[IEEE80211_P2P_DEVICE_TYPE_LEN];
    int                                    num_sec_dev_types;
    u_int8_t                               *sec_dev_type;   /* need to free this structure if allocated */

    /* registered event handlers */
    wlan_p2p_event_handler                 event_handler;
    void                                   *event_arg;
    u_int32_t                              p2p_req_id;   /* id of a perticular request */

   /* protection mode */
    u_int8_t                               p2p_protection_mode; /* protection mode to be used while sending frames */

    /* request id used with scan operations */
    IEEE80211_SCAN_REQUESTOR               p2p_scan_requestor;
    IEEE80211_SCAN_ID                      p2p_scan_id;
    ieee80211_p2p_scan_request             p2p_inprogress_scan_req_type;   /* type of the current scan request that is in progress */

    /* the following 2 fields are only valid  if p2p_inprogress_scan_req_type is LISTEN */
    u_int32_t                              p2p_cur_req_id;
    u_int32_t                              p2p_cur_listen_freq;
    u_int32_t                              p2p_cur_listen_duration;

    /* pending scan request type */
    ieee80211_p2p_scan_request             p2p_pending_scan_req_type;     /* type of the pending scan request that is in progress */
    u_int32_t                              p2p_scan_channels[IEEE80211_P2P_NUM_SOCIAL_CHANNELS];
    ieee80211_scan_params                  p2p_scan_params;

    /* the following 2 fields are only valid  if p2p_pending_scan_req_type is LISTEN */
    u_int32_t                              p2p_listen_freq;
    u_int32_t                              p2p_listen_duration;

    /* P2P Device address */
    u_int8_t                               p2p_device_address_valid;
    u_int8_t                               p2p_device_address[IEEE80211_ADDR_LEN];

    /* WPS Primary Device Type */
    u_int8_t                               wps_primary_device_type_valid;
    u_int8_t                               wps_primary_device_type[WPS_DEV_TYPE_LEN];

    /* list of pending p2p action frames */
    TAILQ_HEAD(,_p2p_act_frame)            p2p_act_frame_head;      /* list of p2p action frames pending */
    TAILQ_HEAD(,_p2p_act_frame)            p2p_act_frame_pool;      /* free list of p2p action frame structs */
    TAILQ_HEAD(,_p2p_act_frame)            p2p_act_frame_sent;      /* list of p2p action frame sent out and waiting for response*/
    u_int32_t                              p2p_act_frame_seq;       /* seq # to be used with action frame */

    ieee80211_p2p_act_frm_seq_ctr      p2p_seq_tracker[IEEE80211_P2P_MAX_SEQ_TRACK]; /* MAC / seq # array for dup. detection */

};

/*
 * internal API exposed by p2p device module.
 */
int ieee80211_p2p_scan(wlan_p2p_t p2p_handle, int full_scan, int freq, ieee80211_p2p_scan_request type);
int ieee80211_p2p_start_listen(wlan_p2p_t p2p_handle, unsigned int freq,u_int32_t req_id,
                               unsigned int duration, ieee80211_p2p_scan_request type,
                               IEEE80211_SCAN_PRIORITY scan_priority);
void ieee80211_p2p_stop_listen(wlan_p2p_t p2p_handle, u_int32_t flags);
void ieee80211_p2p_deliver_event(wlan_p2p_t p2p_handle, wlan_p2p_event *event);
void ieee80211_p2p_stop(wlan_p2p_t p2p_handle, u_int32_t flags);
void ieee80211_p2p_stop_send_action_scan(wlan_p2p_t p2p_handle);
int ieee80211_p2p_send_action_frame(wlan_p2p_t p2p_handle, u_int32_t freq, void *arg,
                               const u_int8_t *dst_addr, const u_int8_t *src_addr,const u_int8_t *bssid,
                                    const u_int8_t *data, u_int32_t data_len);

#endif
