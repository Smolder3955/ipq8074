/*
 * Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * Copyright (c) 2011, Atheros Communications
 *
 * This file contains the private definitions for P2P Discovery. 
 * It should be used outside of this module.
 *
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */


#ifndef _IEEE80211_P2P_PROT_PRIV_H
#define _IEEE80211_P2P_PROT_PRIV_H

#include "ieee80211_p2p_prot_utils.h"
#include <ieee80211_p2p_prot_api.h>

/* List of States for the P2P Device Scanner when doing Discovery */
typedef enum {
    P2P_DISC_STATE_OFF,         /* Uninitialized or attached */
    P2P_DISC_STATE_IDLE,        /* Idle and waiting for the next event. The GO/Client VAP could be operational. */
    P2P_DISC_STATE_PRE_LISTEN,  /* Waiting for hardware to switch to specified channel before transiting to ST_Listen state. */
    P2P_DISC_STATE_LISTEN,      /* Listen on this channel and reply to probe request with probe responses. May also transact action frames. */
    P2P_DISC_STATE_PRE_ONCHAN,  /* Waiting for hardware to switch to specified channel before transiting to ST_OnChan state. */
    P2P_DISC_STATE_ONCHAN,      /* Remain on this channel for certain action frames transaction like 3-way handshake during Group Formation phase, 2-way handshake during Invitation phase, and 2-way handshake during Provisioning phase. Note that the hardware is on the actual channel. */
    P2P_DISC_STATE_DISCSCAN,    /* Doing a full scan to find Group Owners and P2P Devices. Sends Probe Req frames if active scan. Note that the actual scan may not have started yet. */
} ieee80211_connection_state; 

/* 
 * List of Events used in the P2P Protcol Discovery State machine.
 * NOTE: please update p2p_prot_disc_sm_event_name[]
 * when this enum is changed (even if the order is changed).
 */ 
typedef enum {
    P2P_DISC_EVENT_NULL = 0,            /* Invalid event. A placeholder. */
    P2P_DISC_EVENT_START,               /* Start of state machine. */
    P2P_DISC_EVENT_RESET,               /* Reset everything. Disable discoverabality timer, cancel all scans and remain_on_channels, and return back to IDLE state. */
    P2P_DISC_EVENT_END,                 /* End of state machine. Disable all timers, free structures, cancel all scans and remain_on_channels, deregisters the callbacks, etc. */
    P2P_DISC_EVENT_TX_GO_NEG_REQ,       /* Command to send GO Negotiation Request frame. */
    P2P_DISC_EVENT_TX_GO_NEG_RESP,      /* command to send GO Negotiation Response frame. */
    P2P_DISC_EVENT_TX_GO_NEG_CONF,      /* command to send GO Negotiation Confirm frame. */
    P2P_DISC_EVENT_TX_INVITE_REQ,       /* command to send Invitation Request frame. */
    P2P_DISC_EVENT_TX_INVITE_RESP,      /* command to send Invitation Response frame. */
    P2P_DISC_EVENT_TX_PROV_DISC_REQ,    /* command to send Provisioning Discovery Request frame. */
    P2P_DISC_EVENT_TX_PROV_DISC_RESP,   /* command to send Provisioning Discovery Response frame. */
    P2P_DISC_EVENT_START_GO,            /* command to start Group Owner */
    P2P_DISC_EVENT_START_CLIENT,        /* command to start P2P Client */
    P2P_DISC_EVENT_SCAN_START,          /* start of channel scan */
    P2P_DISC_EVENT_SCAN_END,            /* End of channel scan */
    P2P_DISC_EVENT_SCAN_DEQUEUE,        /* Scan request rejected and dequeued. */
    P2P_DISC_EVENT_ON_CHAN_START,       /* start of remain on channel */
    P2P_DISC_EVENT_ON_CHAN_END,         /* End of remain on channel */
    P2P_DISC_EVENT_TX_COMPLETE,         /* completion status for an action frame. */
    P2P_DISC_EVENT_RX_MGMT,             /* Receive a management frame like Action, Probe Req/Resp, etc. */
    P2P_DISC_EVENT_DISC_SCAN_INIT,      /* To start the scanning in discovery scan state */
    P2P_DISC_EVENT_DISC_SCAN_REQ,       /* Request to start a device discovery scan */
    P2P_DISC_EVENT_LISTEN_ST_DISC_REQ,  /* Indicate that the Listen State Discoverability mode has changed */
    P2P_DISC_EVENT_START_PEND_DISCSCAN, /* Start the pended discovery scan */
    P2P_DISC_EVENT_APP_IE_UPDATE,       /* There is an update of the Probe Request Application IE */
    P2P_DISC_EVENT_DISC_SCAN_STOP,      /* Stop Discovery scan (if any) */

    /* List of timer events */
    P2P_DISC_EVENT_TMR_DISC_LISTEN,     /* Timer for next Discovery Listen. Not specific in any state. */
    P2P_DISC_EVENT_TMR_SET_CHAN_SWITCH, /* Timer for channel switch timeout during state Pre_OnChan or Pre_Listen */
    P2P_DISC_EVENT_TMR_TX_RETRY,        /* Timer to retry the Tx Action frame during state OnChan or Listen */
    P2P_DISC_EVENT_TMR_RETRY_SET_CHAN,  /* Timer to retry the Set Channel. Only used in PRE_ONCHAN and PRE_LISTEN state */
    P2P_DISC_EVENT_TMR_RETRY_SET_SCAN,  /* Timer to retry the set scan request (could be set chan too) Only used in DISCSCAN state. */
    P2P_DISC_EVENT_TMR_DISC_SCAN_EXPIRE,/* Timer to timeout the device discovery scan request. */
    P2P_DISC_EVENT_TMR_TX_REQ_EXPIRE,   /* Timer to timeout the Transmit Action Frame request. */

} ieee80211_connection_event; 

/* List of timer used for this state machine. Please update disc_timer[] when this list is changed */
typedef enum {
    TIMER_DISC_LISTEN = 0,      /* Timer for next Discovery Listen. Specific in IDLE state. */
    TIMER_SET_CHAN_SWITCH,      /* Timer for channel switch timeout during state Pre_OnChan and Pre_Listen*/
    TIMER_TX_RETRY,             /* Timer to retry the Tx Action frame during state OnChan or Listen */
    TIMER_RETRY_SET_CHAN,       /* Timer to retry the Set Channel. Only used in PRE_ONCHAN and PRE_LISTEN state */
    TIMER_RETRY_SET_SCAN,       /* Timer to retry the set scan request (could be set chan too) Only used in DISCSCAN state. */
    TIMER_DISC_SCAN_EXPIRE,     /* Timer to timeout the device discovery scan request. */
    TIMER_TX_REQ_EXPIRE,        /* Timer to timeout the Transmit Action Frame request. */
    MAX_NUM_P2P_DISC_TIMER      /* NOTE: Make sure this is the last entry */
} P2P_DISC_TIMER_TYPE;

/* Structure to store the Timer information */
typedef struct {
    wlan_p2p_prot_t             prot_handle;        /* P2P Protocol module handle */
    P2P_DISC_TIMER_TYPE         type;               /* Timer type */
    os_timer_t                  timer_handle;       /* OS timer object handle */
    ieee80211_connection_event  event;              /* Event to post when timer triggers */
} p2p_disc_timer_info;

typedef enum {
    SCAN_CANCEL_REASON_NULL = 0,
    SCAN_CANCEL_REASON_FAILED,
    SCAN_CANCEL_REASON_GO_CLIENT_STARTED,
    SCAN_CANCEL_REASON_TIMEOUT
} SCAN_CANCEL_REASON;

/* Time in millisec to dwell on a single social channel */
#define P2P_PROT_SOCIAL_SCAN_DWELL_TIME     30
/* Time in ms to dwell on a single active channel for full/partial scan */
#define P2P_PROT_ACTIVE_SCAN_DWELL_TIME     30
/* Time in ms to dwell on a single passive channel for full/partial scan */
#define P2P_PROT_PASSIVE_SCAN_DWELL_TIME    110
/* Time in ms between transmitting two probe Request frames during scan */
#define P2P_PROT_SCAN_REPEAT_PROBE_REQUEST_INTVAL   20

/* Number of repeated probe request during a scan to find a device */
#define P2P_PROT_DEV_FIND_NUM_REPEATED_PROBE        3

/* The earlier scan request failed and this is the time in millisec to wait before retry */
#define DEF_TIMEOUT_RETRY_SET_SCAN          10

/* Number of times to check for the cancelled scan event. For the workaround where
 * the scan module needs to wait for the scan stop before starting a new scan. */
#define WAR_NUM_CHECK_FOR_CANCEL_SCAN_EVENT 3

/* The default time to wait for a Channel switch started by wlan_p2p_set_channel() */
#define DEF_TIMEOUT_CHANNEL_SWITCH          5000

/* Minimum time in millisec to send an action frame. */
#define P2P_PROT_MIN_TIME_TO_TX_ACT_FR      15

/*
 * Number of transmission retries to transmit an Action frame
 * We set this to a huge number so that the timeout is primarily dictated by the
 * request timeout value.
 */
#define DEF_NUM_TX_ACTION_RETRY         (2000/DEF_TIMEOUT_TX_RETRY)

/* Default timeout to retry the transmission of action frame during State OnChan */
#define DEF_TIMEOUT_TX_RETRY            25      /* millisec */

/* Default remain_on_channel time, in terms of milliseconds. */
#define P2P_DISC_DEF_ON_CHAN_TIME       200


/* Structure of Event Information to transmit an action frame */
typedef struct _tx_event_info {
    u_int32_t                           alloc_size;     /* allocated size of this structure */
    wlan_p2p_prot_tx_act_fr_param       orig_req_param; /* Copy of the original request param */
    wlan_p2p_prot_event                 cb_event_param; /* Param buf to be used for event callback */
    u_int8_t                            dialog_token;   /* Assigned dialog token */
    u_int32_t                           tx_req_id;      /* unique transmit ID to differentiate from other tx request */
    u_int32_t                           timeout;        /* Timeout in millisec for this request to complete */
    bool                                need_mini_find; /* indicates that we need to find this peer. */
    systime_t                           minifind_start; /* Start time of minifind */
    bool                                have_group_id;
    wlan_p2p_prot_grp_id                group_id;       /* Optional group ID. Valid when have_group_id==true */
    u_int8_t                            peer_addr[IEEE80211_ADDR_LEN];
    u_int8_t                            rx_addr[IEEE80211_ADDR_LEN];
                                                        /* Receiver Address used when transmitting this frame. Only valid when
                                                         * sending Prov Disc Req or Invitation Req frames */
    u_int16_t                           tx_freq;        /* Channel frequency to transmit this frame, valid when (!need_mini_find) */
    bool                                is_GO;          /* true if we are the GO based on GO_INTENT */
    u_int32_t                           in_ie_offset;   /* offset of buffer containing additional IE to append to frame */
    u_int32_t                           in_ie_buf_len;  /* additional IE buf size to append to frame */
    u_int32_t                           out_ie_offset;  /* offset of buffer containing final IE's used to send frame */
    u_int32_t                           out_ie_buf_len; /* size of buffer containing final IE's used to send frame */

    /* The following section contains information from the previous frame */
    wlan_p2p_prot_frame_type            previous_frtype;/* Previous frame type. Used for sanity checks */
    /* Received Channel list from the previous frame; only when prev. frame is GO_NEG_REQ or GO_NEG_RESP 
     * or INVITE_REQ */
    bool                                rx_valid_chan_list;
    struct p2p_channels                 rx_chan_list;
    u_int8_t                            rx_country[3];
    struct p2p_channels                 intersected_chan_list;
    /* Info. from previous frame; valid when only when previous frame is Invitation Request frame */
    bool                                prev_fr_we_are_GO;
    bool                                prev_fr_start_new_group;

    int                                 frame_len;      /* Length of frame */
    u_int8_t                            frame_buf[IEEE80211_RTS_MAX];    /* Frame buffer for the action frame */
} tx_event_info;

typedef struct _wlan_p2p_prot_event_tx_compl {
    int                             status;
    tx_event_info                   *orig_tx_param;
} wlan_p2p_prot_event_tx_compl;

/* Structure of Event Information to do device discovery scan */
typedef struct _disc_scan_event_info {
    u_int32_t                           alloc_size;                 /* allocated size of this structure */
    wlan_p2p_prot_disc_scan_param       orig_req_param;             /* Copy of the original request param */
    wlan_p2p_prot_event                 cb_event_param;             /* Param buf to be used for event callback */

    bool                                is_minifind;                /* Indicates that this scan is due to a minifind */
    /* Minifind related info. Valid when is_minifind is true. */
    struct {
        ieee80211_connection_event          tx_event_type;          /* Tx event type */
        tx_event_info                       *tx_fr_request;         /* Current Tx request. */
        bool                                aborted;                /* Tx request aborted */
    } minifind;

    p2p_prot_scan_type                  curr_scan_type;             /* Current requested scan type */
    bool                                remove_add_ie;              /* Should remove the default probereq IE during this discovery? */
    int                                 num_find_cycle;             /* Count of number of Find/Scan's left */
    bool                                curr_scan_started;          /* Current scan has started */
    bool                                on_chan_started;            /* On_Channel scan has started */
    systime_t                           time_start;                 /* Start time of this discovery scan */
    u_int32_t                           curr_timeout;               /* Current timeout. May be different from original timeout due to scan retries. */
    bool                                desired_peer_found;         /* desired peer is found */
    u_int32_t                           desired_peer_freq;          /* Freq where desired peer is found. Only valid when this peer is in scan table */
    u_int32_t                           in_ie_offset;               /* offset of buffer containing additional IE for probe req frame */
    u_int32_t                           in_ie_buf_len;              /* additional IE buf size to append to frame */

    bool                                in_social_chans_only;       /* Only scan on the social channels */
    bool                                in_stop_when_discovered;    /* Stop scan when the devices specified by filters are found. */
    u_int32_t                           in_dev_filter_list_offset;  /* offset to array of wlan_p2p_prot_disc_dev_filter containing device filters */
    u_int32_t                           in_dev_filter_list_num;     /* no. of device filters */

    u_int32_t                           in_p2p_wldcd_ssid_offset;   /* offset to P2P wildcard SSID. 
                                                                     * It is guaranteed that this SSID is just before the Legacy SSID list */

} disc_scan_event_info;

/* Structure of Event Information for P2P_DISC_EVENT_APP_IE_UPDATE - set the IE */
typedef struct _set_app_ie_event_info {
    u_int8_t                            *probe_req_add_ie;
    int                                 probe_req_add_ie_len;
} set_app_ie_event_info;

/* This data structure are used for the event_data field when processing a state machine event */
typedef struct _sm_event_info {
    ieee80211_connection_event  event_type;
    union {
        tx_event_info                   *tx_info;       /* event_type==P2P_DISC_EVENT_TX_GO_NEG_REQ */
        disc_scan_event_info            *disc_scan_info;/* event_type==P2P_DISC_EVENT_DISC_SCAN_REQ */
        wlan_p2p_prot_event_tx_compl    tx_complete;    /* if event_type is P2P_DISC_EVENT_TX_COMPLETE */
        ieee80211_scan_event            scan_cb;        /* event_type is scan related */
        wlan_p2p_event                  p2p_dev_cb;     /* event from P2P Device */
        set_app_ie_event_info           set_app_ie;     /* event_type==P2P_DISC_EVENT_APP_IE_UPDATE */
    };
} sm_event_info;

/* Number of cycles in find phase to alternate between social scan and listen */
#define WLAN_P2P_PROT_DISC_NUM_FIND_PHASE_CYCLES        5

/* Structure for a single channel */
struct p2p_channel {
    u_int8_t                            reg_class;
    u_int8_t                            ieee_num;
};

/*
 * The following constants are used to program Listen Period and Intervals
 * based on the current VAP usage and OS desired discoverability level.
 * Note that all values are in milliseconds.
 */
/* Listen Period and Interval when no other vap is active and AUTO level */
#define P2P_PROT_LISTEN_PERIOD_NO_VAP_AUTO      500
#define P2P_PROT_LISTEN_INTRVL_NO_VAP_AUTO      1000
/* Listen Period and Interval when no other vap is active and HIGH level */
#define P2P_PROT_LISTEN_PERIOD_NO_VAP_HIGH      500
#define P2P_PROT_LISTEN_INTRVL_NO_VAP_HIGH      500
/* Listen Period and Interval when Station or client is active and AUTO level */
#define P2P_PROT_LISTEN_PERIOD_STA_VAP_AUTO     300
#define P2P_PROT_LISTEN_INTRVL_STA_VAP_AUTO     1200
/* Listen Period and Interval when Station or client is active and HIGH level */
#define P2P_PROT_LISTEN_PERIOD_STA_VAP_HIGH     350
#define P2P_PROT_LISTEN_INTRVL_STA_VAP_HIGH     450
/* Listen Period and Interval when SoftAP, BTAmp, or GO is active and AUTO level */
#define P2P_PROT_LISTEN_PERIOD_AP_VAP_AUTO      175
#define P2P_PROT_LISTEN_INTRVL_AP_VAP_AUTO      400
/* Listen Period and Interval when SoftAP, BTAmp, or GO is active and HIGH level */
#define P2P_PROT_LISTEN_PERIOD_AP_VAP_HIGH      175
#define P2P_PROT_LISTEN_INTRVL_AP_VAP_HIGH      300

/* Context information of Rx of action frame. This structure will be passed back the
 * driver from the upper stack when sending the response frame. */
typedef struct _wlan_p2p_prot_rx_context {
    wlan_p2p_prot_t         prot_handle;
    struct p2p_parsed_ie    msg;
    bool                    parsed_ie_ok;
    u_int16_t               rx_freq;
    u_int8_t                rx_go_intent;       /* received group intent field from GO_NEG_REQ or GO_NEG_RESP */

    /* Received Channel list; only from GO_NEG_REQ or GO_NEG_RESP or INVITE_REQ */
    wlan_p2p_prot_frame_type    previous_frtype;
    bool                    rx_valid_chan_list;
    struct p2p_channels     rx_chan_list;
    u_int8_t                rx_country[3];
    /* Some info. from the Received Invitation Req frame. Only for INVITE_REQ */
    bool                    start_new_group;        /* In the INVITE_REQ params, is a new group starting? */
    bool                    we_are_GO;              /* Is the sender of INVITE_REQ frame a GO? */

    /* back pointer to the original receive event */
    wlan_p2p_event          *p2p_dev_event;
} wlan_p2p_prot_rx_context;

/* Definition of P2P Protocol Device instance data */
struct ieee80211_p2p_prot {
    osdev_t                             os_handle;
    wlan_p2p_t                          p2p_device_handle;
    wlan_if_t                           vap;
    spinlock_t                          set_param_lock;     /* lock when accessing the set parameters */
    ieee80211_hsm_t                     hsm_handle;         /* state machine handle */
    struct wlan_mlme_app_ie             *h_app_ie;
    IEEE80211_SCAN_REQUESTOR            my_scan_requestor;  /* scan requester ID. */
    p2p_prot_scan_params                curr_scan;          /* Current scan context information */

    struct ieee80211_ath_channel            *my_listen_chan;    /* My listen channel */
    u_int8_t                            country[3];
    struct p2p_channel                  listen_chan;        /* listen channel */
    struct p2p_channel                  op_chan;            /* Operational Channel */

    struct p2p_channels                 chanlist;           /* channel list that are supported */

    SCAN_CANCEL_REASON                  scan_cancel_reason; /* Reason for why scan is cancelled */

    bool                                disc_scan_active;   /* Currently doing the discovery scans */
    bool                                st_pre_listen_abort_listen; /* Used in state PRE_LISTEN to indicate that the listen is aborted. */
    bool                                st_pre_onchan_abort;/* Used in state PRE_ONCHAN to indicate that the ON_CHAN is aborted. */

    /* The following parameters are passed to the P2P_DISC_STATE_ONCHAN state */
    u_int16_t                           on_chan_dest_freq;  /* On Channel destination's frequency */

    /* Callback Event handler parameters */
    void                                *event_arg;
    ieee80211_p2p_prot_event_handler    ev_handler;

    /* Current Tx Action frame parameters. We only support one action frame (plus one retry) at one time. This
       data structure must be completed and freed when done. */
    tx_event_info                       *curr_tx_info;
    u_int32_t                           curr_tx_req_id;     /* unique ID of the current tx request. */
    int                                 num_outstanding_tx; /* no. of outstanding transmit requests */
    ieee80211_connection_state          curr_tx_nextstate;  /* Used when Tx retry. Indicate to do PRE_LISTEN or PRE_ONCHAN next */
    /* Current Retry Tx Action frame parameters. This
       data structure must be completed and freed when done. */
    tx_event_info                       *retry_tx_info;

    /* Current Discovery Scan Request parameters. We only support one discovery scan at one time. This
       data structure must be completed and freed when done. */
    disc_scan_event_info                *curr_disc_scan_info;

    /* Desired Peer information to be used during discovery scan */
    struct {
        atomic_t	pend_update;                        /* pending update for peer? */
        atomic_t    avail;                              /* Any desired peer to look for? */
        bool		pend_have_dev_id;                   /* Any pending device ID? */
        spinlock_t	lock;                               /* lock when updating this structure */

        int                             pend_num_dev_filters;
        wlan_p2p_prot_disc_dev_filter   pending_dev_filter[WLAN_P2P_PROT_MAX_NUM_DISC_DEVICE_FILTER];
        int                             num_dev_filters;
        wlan_p2p_prot_disc_dev_filter   dev_filter[WLAN_P2P_PROT_MAX_NUM_DISC_DEVICE_FILTER];
    } desired_peer;

    int                                 num_tx_frame_retries;   /* Number of retries left to transmit action frame */

    atomic_t                            dialog_token;       /* last assigned dialog token value */

	/**
	 * min_disc_int - minDiscoverableInterval
	 */
	int                                 min_disc_int;

	/**
	 * max_disc_int - maxDiscoverableInterval
	 */
	int                                 max_disc_int;

    p2p_disc_timer_info                 sm_timer[MAX_NUM_P2P_DISC_TIMER];   /* List of timers used by Discovery state machine */

    /* Discoverable fields */
    bool                                is_discoverable;
    /* Listen parameters that are inserted in the probe req and resp frames and
     * visible to the external world. We will use these parameters to setup the
     * listen intervals. */
    u_int16_t                           ext_listen_period;
    u_int16_t                           ext_listen_interval;

    /* Parameters that configured by OS shim */
    char                                p2p_dev_name[IEEE80211_MAX_P2P_DEV_NAME_LEN+1]; /* add 1 for NULL termination */
    u_int16_t                           p2p_dev_name_len;
    u_int8_t                            dev_capab;                  /* device capability bitmap */
    u_int8_t                            grp_capab;                  /* group capability bitmap */
    u_int8_t                            wpsVersionEnabled;          /* WPS versions that are currently enabled */
    u_int8_t                            maxGroupLimit;              /* Maximum no. of P2P clients that GO should allow */
    u_int16_t                           configMethods;              /* WSC Methods supported - P2P Device Info Attribute */
    u_int8_t                            primaryDeviceType[IEEE80211_P2P_DEVICE_TYPE_LEN];
    u_int8_t                            numSecDeviceTypes;
    u_int8_t                            *secondaryDeviceTypes;
    wlan_p2p_prot_listen_state_disc     listen_state_disc;          /* Listen state discoverability */
    bool                                def_probereq_ie_removed;    /* Is the default ProbeReq IE removed? */
    u_int8_t                            *probe_req_add_ie;          /* Probe Request additional IE */
    int                                 probe_req_add_ie_len;
};

#define MAX_SINGLE_IE_LEN                               0x00FF  /* max. length of a single IE */

#define P2P_PROT_SCAN_MIN_REST_TIME_MULTIPLE_PORTS      200 /* min time in msec on the BSS channel */
#define P2P_PROT_SCAN_AP_PRESENT_MAX_OFFCHANNEL_TIME    250 /* max time away from BSS channel, in ms */
#define P2P_PROT_PARTIAL_SCAN_NUM_CHAN                  6   /* Number of channels to scan per Partial Scan */
#define P2P_PROT_DEFAULT_LISTEN_CHAN                    11  /* default listen channel for our device */
#define P2P_PROT_DEFAULT_OP_CHAN                        11  /* default Operational channel for our device */

#define CREATE_ADDR_HASH(_addr)     (_addr[0] ^ _addr[1] ^ _addr[2] ^ _addr[3] ^ _addr[4] ^ _addr[5])

#define CREATE_UNIQUE_TIMER_ID(_dialog_token, _frtype, _addr_hash)              \
        ((0x000000FF & (u_int32_t)_dialog_token) | ((u_int32_t)_frtype<<8) |    \
        ((u_int32_t)_addr_hash<<16))

#endif //_IEEE80211_P2P_PROT_PRIV_H
