/*
 * Copyright (c) 2011, 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * Copyright (c) 2011, Atheros Communications
 *
 * Routines for P2P Protocol Module for Client.
 */

#include "ieee80211P2P_api.h"
#include "ieee80211_p2p_client.h"
#include "ieee80211_p2p_client_power.h"
#include "ieee80211_p2p_client_priv.h"
#include "ieee80211_p2p_prot_client.h"
#include "ieee80211_p2p_ie.h"
#include "ieee80211_sm.h"
#include "ieee80211_p2p_prot_utils.h"
#include "ieee80211_p2p_prot_api.h"

#define BUFSIZE_TMP_P2P_IE      1024

/* Max. amount of time to try to find the GO and wait for the right IEs */
#define P2P_PROT_CLIENT_MAX_FIND_TIME   (120 * 1000)

typedef struct ieee80211_p2p_prot_client *wlan_p2p_prot_client_t;

/* Macros to help to print the mac address */
#define MAC_ADDR_FMT        "%x:%x:%x:%x:%x:%x"
#define SPLIT_MAC_ADDR(addr)                    \
    ((addr)[0])&0x0FF,((addr)[1])&0x0FF,        \
    ((addr)[2])&0x0FF,((addr)[3])&0x0FF,        \
    ((addr)[4])&0x0FF,((addr)[5])&0x0FF         \

/********************** Start of State Machine support routines *************************/

#define MAX_QUEUED_EVENTS   4   /* max. number of queued events in state machine */

/* List of States for the P2P Device Scanner when doing Discovery */
typedef enum {
    P2P_PROT_CLIENT_STATE_OFF,              /* Uninitialized or attached */
    P2P_PROT_CLIENT_STATE_IDLE,             /* Idle */
    P2P_PROT_CLIENT_STATE_FIND_GO_OPCHAN,   /* Find the GO on Operational channel. */
    P2P_PROT_CLIENT_STATE_GO_FOUND,         /* GO is found and waiting for IE's to be acceptable. */
    P2P_PROT_CLIENT_STATE_DO_FULLSCAN,      /* Find the GO using full scans. Actually, it is using partial scans and Op-Chan scans. */
} ieee80211_connection_state; 

/* 
 * List of Events used in the P2P Protcol Discovery State machine.
 * NOTE: please update p2p_prot_client_sm_event_name[]
 * when this enum is changed (even if the order is changed).
 */ 
typedef enum {
    P2P_PROT_CLIENT_EVENT_NULL = 0,             /* Invalid event. A placeholder. */
    P2P_PROT_CLIENT_EVENT_RESET,                /* Reset everything. Disable all timers, cancel all scans and remain_on_channels, and return back to IDLE state. */
    P2P_PROT_CLIENT_EVENT_CONNECT_TO,           /* Connect to Grp Owner. */
    P2P_PROT_CLIENT_EVENT_SCAN_START,           /* start of channel scan */
    P2P_PROT_CLIENT_EVENT_SCAN_END,             /* End of channel scan */
    P2P_PROT_CLIENT_EVENT_SCAN_DEQUEUE,         /* Scan request rejected and dequeued. */
    P2P_PROT_CLIENT_EVENT_RX_MGMT,              /* Receive a management frame like Probe Response or beacon. */
    P2P_PROT_CLIENT_EVENT_UPDATE_JOIN_PARAM,    /* Join parameter are updated */
    P2P_PROT_CLIENT_EVENT_UPDATE_GROUP_ID,      /* Group ID parameter are updated */

    /* List of timer events */
    P2P_PROT_CLIENT_EVENT_TMR_CONNECT_EXPIRE,   /* Timeout for the CONNECT_TO request. */
    P2P_PROT_CLIENT_EVENT_TMR_NEXT_SCAN,        /* Timer to start scan to find GO or to check the GO's IE. */

} ieee80211_connection_event; 

/* NOTE: please update ieee80211_connection_event when this array is changed */ 
static const char* p2p_prot_client_sm_event_name[] = {
    "NULL", /* P2P_PROT_CLIENT_EVENT_NULL = 0 */
    "RESET", /* P2P_PROT_CLIENT_EVENT_RESET */
    "CONNECT_TO", /* P2P_PROT_CLIENT_EVENT_CONNECT_TO */
    "SCAN_START", /* P2P_PROT_CLIENT_EVENT_SCAN_START */
    "SCAN_END", /* P2P_PROT_CLIENT_EVENT_SCAN_END */
    "SCAN_DEQUEUE", /* P2P_PROT_CLIENT_EVENT_SCAN_DEQUEUE */
    "RX_MGMT", /* P2P_PROT_CLIENT_EVENT_RX_MGMT */
    "UPDATE_JOIN_PARAM", /* P2P_PROT_CLIENT_EVENT_UPDATE_JOIN_PARAM */
    "UPDATE_GROUP_ID", /* P2P_PROT_CLIENT_EVENT_UPDATE_GROUP_ID */

    /* List of timer events */
    "TMR_CONNECT_EXPIRE", /* P2P_PROT_CLIENT_EVENT_TMR_CONNECT_EXPIRE */
    "TMR_NEXT_SCAN", /* P2P_PROT_CLIENT_EVENT_TMR_NEXT_SCAN */
};

static const char* event_name(ieee80211_connection_event event)
{
    return (p2p_prot_client_sm_event_name[event]);
}

/* Additional information when event is P2P_PROT_CLIENT_EVENT_RX_MGMT */
typedef struct _rx_mgmt_event_info {
    ieee80211_p2p_client_event_type     event_type;
    ieee80211_p2p_client_ie_bit_state   in_group_formation;
    ieee80211_p2p_client_ie_bit_state   wps_selected_registrar;
    u_int8_t                            ta[IEEE80211_ADDR_LEN];
} rx_mgmt_event_info;

/* This data structure are used for the event_data field when processing a state machine event */
typedef struct _sm_event_info {
    ieee80211_connection_event  event_type;
    union {
        rx_mgmt_event_info              rx_mgmt;    /* event_type==P2P_PROT_CLIENT_EVENT_RX_MGMT */
        wlan_p2p_prot_client_join_param join_param; /* P2P_PROT_CLIENT_EVENT_UPDATE_JOIN_PARAM */
        wlan_p2p_prot_grp_id            group_id;   /* P2P_PROT_CLIENT_EVENT_UPDATE_GROUP_ID */
        ieee80211_scan_event            scan_cb;    /* event_type is scan related */
    };
} sm_event_info;

#define TIMER_INFO_ENTRY(_event) {P2P_PROT_CLIENT_EVENT_TMR_##_event, #_event}

/* List of timer used for this state machine. Please update prot_client_timer[] when this list is changed */
typedef enum {
    TIMER_CONNECT_EXPIRE = 0,       /* Timeout for the CONNECT_TO request. */
    TIMER_NEXT_SCAN,                /* Timer to start scan to find GO or to check the GO's IE. */
    MAX_NUM_P2P_PROT_CLIENT_TIMER   /* NOTE: Make sure this is the last entry */
} P2P_PROT_CLIENT_TIMER_TYPE;

/* Structure to store the Timer information */
typedef struct {
    wlan_p2p_prot_client_t      prot_handle;        /* P2P Protocol client module handle */
    P2P_PROT_CLIENT_TIMER_TYPE  type;               /* Timer type */
    os_timer_t                  timer_handle;       /* OS timer object handle */
    ieee80211_connection_event  event;              /* Event to post when timer triggers */
} p2p_prot_client_timer_info;

struct timer_info {
    const ieee80211_connection_event    event;
    const char                          *name;
};

/* List of timer event to post when timer triggers. */
const struct timer_info     prot_client_timer[MAX_NUM_P2P_PROT_CLIENT_TIMER] = {
    TIMER_INFO_ENTRY(CONNECT_EXPIRE),
    TIMER_INFO_ENTRY(NEXT_SCAN),
};

struct ieee80211_p2p_prot_client {

    spinlock_t              lock;
    ieee80211_vap_t         vap;
    struct wlan_mlme_app_ie *app_ie_handle;
    osdev_t                 os_handle;
    wlan_p2p_client_t       p2p_client_handle;

    u_int8_t                tmp_ie_buf[BUFSIZE_TMP_P2P_IE];     /* Temporary IE buffer */

    /* For now, do not send the listen timing on client port. The difficulty is
     * that this information is at the p2p device VAP. */
    u_int16_t               ext_listen_period;
    u_int16_t               ext_listen_interval;

    char                    p2p_dev_name[IEEE80211_MAX_P2P_DEV_NAME_LEN+1]; /* add 1 for NULL termination */
    u_int16_t               p2p_dev_name_len;
    u_int8_t                dev_cap;                    /* device capability bitmap */
    u_int8_t                wpsVersionEnabled;          /* WPS versions that are currently enabled */
    u_int16_t               configMethods;              /* WSC Methods supported - P2P Device Info Attribute */
    u_int8_t                primaryDeviceType[IEEE80211_P2P_DEVICE_TYPE_LEN];
    u_int8_t                numSecDeviceTypes;
    u_int8_t                *secondaryDeviceTypes;
    u_int8_t                p2pDeviceAddr[IEEE80211_ADDR_LEN];

    /* List of timers used by state machine */
    p2p_prot_client_timer_info
                            sm_timer[MAX_NUM_P2P_PROT_CLIENT_TIMER];

    wlan_p2p_prot_grp_id    group_id;
    ieee80211_hsm_t         hsm_handle;
    p2p_prot_scan_params    curr_scan;                      /* Current scan context information */
    wlan_p2p_prot_client_join_param
                            join_param;                     /* Join parameters */
    ieee80211_p2p_client_ie_bit_state
                            in_group_formation;
    ieee80211_p2p_client_ie_bit_state
                            wps_selected_registrar;
    bool                    have_connect_to_req;            /* have outstanding connect to request */

    /* Various state related fields */
    systime_t               scan_end_time;                  /* time when the last scan ends. */
    systime_t               connect_start_time;             /* connect request start time */
    int                     st_GO_FOUND_scan_cnt;           /* Number of scans completed */
    u_int32_t               st_DO_FULLSCAN_scan_interval;   /* inter-scans interval */
    bool                    wait_for_scan_end_before_ind;

    bool                    client_evt_handler_registered;  /* p2p client event handler registered? */

    ieee80211_p2p_prot_client_event_handler     
                            callback_handler;               /* Callback handler */
    void                    *callback_arg;

};

static void
proc_event_reset(ieee80211_p2p_prot_client_t prot_handle);

static void
proc_event_update_group_id(
    ieee80211_p2p_prot_client_t prot_handle,
    wlan_p2p_prot_grp_id        *new_group_id);

static void
proc_event_update_join_param(
    ieee80211_p2p_prot_client_t     prot_handle,
    wlan_p2p_prot_client_join_param *new_join_param);

static void
p2p_prot_client_complete_connect_to_req(
    ieee80211_p2p_prot_client_t prot_handle,
    int                         status);

static void
ieee80211_p2p_prot_client_p2p_event_handler(
    wlan_p2p_client_t p2p_client_handle, ieee80211_p2p_client_event *event, void *arg);

static void
update_client_group_id(wlan_p2p_client_t p2p_client_handle, wlan_p2p_prot_grp_id *param);

/******************* Start of Timer support functions ********************/

static const char *
timer_name(P2P_PROT_CLIENT_TIMER_TYPE timer_type)
{
    return(prot_client_timer[timer_type].name);
}

/* Timer callback for OS Timer. It is a single function for all timers used in this state machine */
static OS_TIMER_FUNC(p2p_prot_client_disc_timer_cb)
{
    /*struct ieee80211com         *ic;*/
    p2p_prot_client_timer_info  *timer_info;
    wlan_p2p_prot_client_t      prot_handle;

    OS_GET_TIMER_ARG(timer_info, p2p_prot_client_timer_info *);

    prot_handle = timer_info->prot_handle;

    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
        "%s: OS Timer %s(%d) triggered. Posting event %s, 0x%x\n",
        __func__, timer_name(timer_info->type), timer_info->type, event_name(timer_info->event), timer_info->event);

    /* Post a message */
    ieee80211_sm_dispatch(prot_handle->hsm_handle, timer_info->event,
                          0, NULL);
}

/*
 * Initialization of all timers used by this state machine. This function is called when this module loads.
 */
static void
p2p_prot_client_timer_init(wlan_p2p_prot_client_t prot_handle)
{
    int     i;

    for (i = 0; i < MAX_NUM_P2P_PROT_CLIENT_TIMER; i++) {
        p2p_prot_client_timer_info  *timer;

        timer = &(prot_handle->sm_timer[i]);
        timer->prot_handle = prot_handle;
        timer->type = i;
        timer->event = prot_client_timer[i].event;

        OS_INIT_TIMER(prot_handle->os_handle, &(timer->timer_handle), p2p_prot_client_disc_timer_cb, (void *)timer, QDF_TIMER_TYPE_WAKE_APPS);
    }
}

/*
 * Cleanup of all timers used by this state machine. This function is called when this module unloads.
 */
static void
p2p_prot_client_timer_cleanup(wlan_p2p_prot_client_t prot_handle)
{
    int     i;

    for (i = 0; i < MAX_NUM_P2P_PROT_CLIENT_TIMER; i++) {
        p2p_prot_client_timer_info  *timer;

        timer = &(prot_handle->sm_timer[i]);

        OS_CANCEL_TIMER(&(timer->timer_handle));

        OS_FREE_TIMER(&(timer->timer_handle));
    }
}

/*
 * Support timer routine to stop a Timer.
 * Note that this timer may not have started.
 */
static void
p2p_prot_client_timer_stop(wlan_p2p_prot_client_t prot_handle, P2P_PROT_CLIENT_TIMER_TYPE timer_type)
{
    p2p_prot_client_timer_info     *timer;

    ASSERT(timer_type < MAX_NUM_P2P_PROT_CLIENT_TIMER);
    timer = &(prot_handle->sm_timer[timer_type]);

    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
        "%s: OS Timer %s(%d) stopped.\n",
        __func__, timer_name(timer->type), timer->type);

    OS_CANCEL_TIMER(&(timer->timer_handle));
}

/*
 * Routine to cancel all timers.
 */

/*
static void
p2p_prot_client_timer_stop_all(wlan_p2p_prot_client_t prot_handle)
{
    int     i;

    for (i = 0; i < MAX_NUM_P2P_PROT_CLIENT_TIMER; i++) {
        p2p_prot_client_timer_info     *timer;

        timer = &(prot_handle->sm_timer[i]);
        p2p_prot_client_timer_stop(prot_handle, timer->type);
    }
}
*/

/*
 * Support timer routine to start a P2P Timer.
 * Note that the timeout duration, duration, is in terms of milliseconds.
 */
static void
p2p_prot_client_timer_start(wlan_p2p_prot_client_t prot_handle, P2P_PROT_CLIENT_TIMER_TYPE timer_type, u_int32_t duration)
{
    p2p_prot_client_timer_info     *timer;

    ASSERT(timer_type < MAX_NUM_P2P_PROT_CLIENT_TIMER);

    timer = &(prot_handle->sm_timer[timer_type]);

    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
        "%s: OS Timer %s(%d) started. Duration=%d millisec\n",
        __func__, timer_name(timer->type), timer->type, duration);

    OS_SET_TIMER(&(timer->timer_handle), duration);
}
/******************* End of Timer support functions ********************/
/*
 * ****************************** State Machine common helper functions ******************************
 */

/*
 * Process an event using the state machine common handlers.
 * Returns true if this event is handled. Otherwise, return false.
 */
static bool
proc_event_common(
    ieee80211_p2p_prot_client_t prot_handle,
    u_int16_t                   eventtype,
    sm_event_info               *event,
    u_int16_t                   event_data_len, 
    ieee80211_connection_state  *ret_newstate)
{
    ieee80211_connection_state  curr_state;
    ieee80211_connection_state  newstate = P2P_PROT_CLIENT_STATE_OFF;

    curr_state = (ieee80211_connection_state)ieee80211_sm_get_curstate(prot_handle->hsm_handle);

    switch(eventtype)
    {
    case P2P_PROT_CLIENT_EVENT_RESET:
    {
        proc_event_reset(prot_handle);
        newstate = P2P_PROT_CLIENT_STATE_IDLE;
        break;
    }

    case P2P_PROT_CLIENT_EVENT_TMR_CONNECT_EXPIRE:
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
            "%s: Timeout, do a reset. Outstanding have_connect_to_req=%d\n",
            __func__, prot_handle->have_connect_to_req);
        proc_event_reset(prot_handle);
        newstate = P2P_PROT_CLIENT_STATE_IDLE;
        break;

    case P2P_PROT_CLIENT_EVENT_UPDATE_GROUP_ID:
        if (curr_state != P2P_PROT_CLIENT_STATE_IDLE) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: Warning: we should not be updating GroupID unless in IDLE state. curr_state=%d\n",
                __func__, curr_state);
        }
        proc_event_update_group_id(prot_handle, &event->group_id);
        break;

    case P2P_PROT_CLIENT_EVENT_UPDATE_JOIN_PARAM:
        proc_event_update_join_param(prot_handle, &event->join_param);
        break;

    default:
        return false;   /* event not handled here */
    }

    if (newstate != P2P_PROT_CLIENT_STATE_OFF) {
        if (curr_state == newstate) {
            /* Effectively, there is no change in state */
        }
        else {
            *ret_newstate = newstate;
        }
    }

    return true;        /* event handled here */
}

/*
 * Check that the scan event has the same ID as my current request.
 * Return true if the ID matched. false if unmatched.
 */
static inline bool
check_scan_event_id_matched(p2p_prot_scan_params *prot_scan_param, ieee80211_scan_event *event)
{
    if (prot_scan_param->request_id == event->scan_id) {
        return true;
    }
    else {
        IEEE80211_DPRINTF_VB(prot_scan_param->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
            "%s: Rejected scan event type=%d due to mismatched ID. scan=%d, mine=%d\n",
            __func__, event->type, event->scan_id, prot_scan_param->request_id);

        return false;
    }
}

static void
proc_event_reset(ieee80211_p2p_prot_client_t prot_handle)
{
    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
        "%s: Reseting. have_connect_to_req=%d, wait_for_scan_end_before_ind=%d\n",
        __func__, prot_handle->have_connect_to_req, prot_handle->wait_for_scan_end_before_ind);

    p2p_prot_client_timer_stop(prot_handle, TIMER_CONNECT_EXPIRE);

    if (prot_handle->have_connect_to_req) {
        prot_handle->wait_for_scan_end_before_ind = false;
        p2p_prot_client_complete_connect_to_req(prot_handle, -EINVAL);
    }

    /* Clear all the Join Parameters */
    OS_MEMSET(&prot_handle->join_param, 0, sizeof(wlan_p2p_prot_client_join_param));
}

static void
p2p_prot_client_complete_connect_to_req(
    ieee80211_p2p_prot_client_t prot_handle,
    int                         status)
{
    p2p_prot_client_timer_stop(prot_handle, TIMER_CONNECT_EXPIRE);

    /* Do a callback */
    if (prot_handle->callback_handler) {
        (*prot_handle->callback_handler)(prot_handle->p2p_client_handle,
                                         IEEE80211_P2P_PROT_CLIENT_EVENT_CONNECT_TO_COMPLETE,
                                         prot_handle->callback_arg,
                                         status);
    }

    prot_handle->have_connect_to_req = false;
}

/*
 * Process the event P2P_PROT_CLIENT_EVENT_UPDATE_GROUP_ID
 */
static void
proc_event_update_group_id(
    ieee80211_p2p_prot_client_t prot_handle,
    wlan_p2p_prot_grp_id        *new_group_id)
{
    int     retval = EOK;

    /* Let the P2P client knows the new SSID */
    retval = wlan_p2p_client_set_desired_ssid(prot_handle->p2p_client_handle, &new_group_id->ssid);
    if (retval != EOK) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
            "%s: Error: wlan_p2p_client_set_desired_ssidlist ret=0x%x\n",
            __func__, retval);
    }
    else {
        /* Store this information */
        OS_MEMCPY(&prot_handle->group_id, new_group_id, sizeof(wlan_p2p_prot_grp_id));
    }
}

/*
 * Routine to record the end of scan end time.
 */
void
record_scan_end_time(ieee80211_p2p_prot_client_t prot_handle)
{
    prot_handle->scan_end_time = OS_GET_TIMESTAMP();
}

/*
 * Process the event P2P_PROT_CLIENT_EVENT_UPDATE_JOIN_PARAM
 */
static void
proc_event_update_join_param(
    ieee80211_p2p_prot_client_t     prot_handle,
    wlan_p2p_prot_client_join_param *new_join_param)
{
    /*int     retval = EOK;*/

    /* STNG TODO: we should not be updating unless we are in IDLE state */

    /* Store this information */
    OS_MEMCPY(&prot_handle->join_param, new_join_param, sizeof(wlan_p2p_prot_client_join_param));
}

/* Debug routine to display the current state */
static char *
dbg_str_ie_bit_state(ieee80211_p2p_client_ie_bit_state state)
{
    switch(state) {
    case IEEE80211_P2P_CLIENT_IE_BIT_NOT_PRESENT:
        return("not present");
    case IEEE80211_P2P_CLIENT_IE_BIT_CLR:
        return("0");
    case IEEE80211_P2P_CLIENT_IE_BIT_SET:
        return("1");
    default:
        return("invalid");
    }
}

struct find_GO_param {
    ieee80211_p2p_prot_client_t prot_handle;
    wlan_p2p_prot_grp_id        *group_id;

    /* returned fields */
    bool                        match_found;
    u_int32_t                   rx_freq;
};

/* Note that the output buffer must be of size (IEEE80211_NWID_LEN + 1) */
static char *
get_str_ssid(char *outbuf, u_int8_t *ssid_str, int ssid_len)
{
    ASSERT(ssid_len <= IEEE80211_NWID_LEN);
    OS_MEMCPY(outbuf, ssid_str, ssid_len);
    outbuf[ssid_len] = 0;
    return outbuf;
}

/*
 * Callback function to iterate the Scan list based on mac addresss 
 * and find a suitable peer that is on this channel.
 * Called by wlan_scan_macaddr_iterate().
 * Returns EOK if not found. 
 * Return -EALREADY if a peer is found and the iteration stops.
 */
static int
p2p_disc_find_peer_GO(void *ctx, wlan_scan_entry_t entry)
{
    struct find_GO_param    *param;
    u_int8_t                *entry_mac_addr;
    int                     retval;
    ieee80211_scan_p2p_info scan_p2p_info;
    u_int8_t                entry_ssid_len = 0;
    u_int8_t                *entry_ssid;

    param = (struct find_GO_param *)ctx;
    entry_mac_addr = wlan_scan_entry_macaddr(entry);

    ASSERT(param->match_found == false);

    /* Check that this peer is a p2p device. Reject it otherwise. */
    retval = wlan_scan_entry_p2p_info(entry, &scan_p2p_info);
    if (retval != EOK) {
        IEEE80211_DPRINTF_VB(param->prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                             "%s: wlan_scan_entry_p2p_info ret err=0x%x, entry=%pK\n",
                             __func__, retval, entry);
        return EOK;     /* continue to the next one */
    }

    IEEE80211_DPRINTF_VB(param->prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                     "%s: scan_p2p_info.is_p2p=%d, param->rx_freq=%d, ic_freq=%d, p2p_dev_addr="MAC_ADDR_FMT"\n",
                     __func__, scan_p2p_info.is_p2p,
                     (scan_p2p_info.listen_chan? scan_p2p_info.listen_chan->ic_freq : 0),
                     SPLIT_MAC_ADDR(scan_p2p_info.p2p_dev_addr));

    if (!scan_p2p_info.is_p2p) {
        /* Not a P2P, skip this entry and go to the next entry. */
        return EOK; 
    }

    ASSERT(param->group_id != NULL);
    /* Make sure the SSID matches */
    entry_ssid = wlan_scan_entry_ssid(entry, &entry_ssid_len);
    if ((entry_ssid_len != param->group_id->ssid.len) ||
        (OS_MEMCMP(param->group_id->ssid.ssid, entry_ssid, entry_ssid_len) != 0))
    {
        /* SSID does not match */
        if (entry_ssid) {
            char    tmpbuf[IEEE80211_NWID_LEN + 1];

            IEEE80211_DPRINTF_VB(param->prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                                 "%s: Error: SSID mismatched. Entry: ssid=%s, len=%d. Desired: ssid=%s, len=%d\n",
                                 __func__, get_str_ssid(tmpbuf, entry_ssid, entry_ssid_len), entry_ssid_len);
            IEEE80211_DPRINTF_VB(param->prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                                 "\tDesired: ssid=%s, len=%d\n",
                                 get_str_ssid(tmpbuf, param->group_id->ssid.ssid, param->group_id->ssid.len),
                                 param->group_id->ssid.len);
        }
        else {
            IEEE80211_DPRINTF_VB(param->prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                             "%s: Error: scan entry %pK has no SSID.\n", __func__, entry);
        }
        return EOK;
    }

    if ((scan_p2p_info.listen_chan == NULL) ||
        (scan_p2p_info.listen_chan->ic_freq == 0))
    {
        IEEE80211_DPRINTF_VB(param->prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                         "%s: Error: missing listen in scan entry %pK or zero frequency.\n", __func__, entry);
        return EOK;
    }

    /* Found the GO */
    IEEE80211_DPRINTF_VB(param->prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                         "%s: Found the GO in device ID address="MAC_ADDR_FMT" scan entry=0x%x.\n",
                         __func__, SPLIT_MAC_ADDR(entry_mac_addr), entry);

    param->match_found = true;

    param->rx_freq = scan_p2p_info.listen_chan->ic_freq;

    /* Return an error so that we stop iterating the list */
    return -EALREADY;
}

/*
 * Process the event P2P_PROT_CLIENT_EVENT_RX_MGMT. Return EOK if successful.
 * This function will check the IE's state and set the parameter, matched, if the
 * IE's state matches what we wanted.
 */
static int
proc_event_rx_mgmt(
    ieee80211_p2p_prot_client_t prot_handle,
    sm_event_info               *event,
    bool                        *matched
    )
{
    int         retval = EOK;

    *matched = false;

    if (event->rx_mgmt.in_group_formation != prot_handle->in_group_formation) {
        /* A change in this bit */
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                             "%s: in_group_formation bit changed from %s to %s\n",
                             __func__, dbg_str_ie_bit_state(prot_handle->in_group_formation),
                             dbg_str_ie_bit_state(event->rx_mgmt.in_group_formation));
    }
    if (event->rx_mgmt.wps_selected_registrar != prot_handle->wps_selected_registrar) {
        /* A change in this bit */
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                             "%s: wps_selected_registrar bit changed from %s to %s\n",
                             __func__, dbg_str_ie_bit_state(prot_handle->wps_selected_registrar),
                             dbg_str_ie_bit_state(event->rx_mgmt.wps_selected_registrar));
    }
    prot_handle->in_group_formation = event->rx_mgmt.in_group_formation;
    prot_handle->wps_selected_registrar = event->rx_mgmt.wps_selected_registrar;

    /* Check if the state of these 2 bits are what we wanted */
    do {

        if (prot_handle->join_param.p2p_ie_in_group_formation) {
            /* The group formation bit must be '1' */
            if (prot_handle->in_group_formation != IEEE80211_P2P_CLIENT_IE_BIT_SET) {
                /* mismatch */
                break;
            }
        }
        else {
            /* The group formation bit must be '0' */
            if (prot_handle->in_group_formation != IEEE80211_P2P_CLIENT_IE_BIT_CLR) {
                /* mismatch */
                break;
            }
        }

        if (prot_handle->join_param.wps_ie_wait_for_wps_ready) {
            /* The Selected Registrar must be set to '1' */
            if (prot_handle->wps_selected_registrar != IEEE80211_P2P_CLIENT_IE_BIT_SET) {
                /* mismatch */
                break;
            }
        }
        /* Else do not need to compare the selected registrar */

        *matched = true;

    } while ( false );

    if (prot_handle->curr_scan.device_freq == 0) {
        /* We do not know the frequency of this device. Hence, we should update the freq.
         * Search the scan table for this entry. Make sure that we have an scan entry
         * for this GO.
         */
        struct find_GO_param    param;

        OS_MEMZERO(&param , sizeof(struct find_GO_param));

        param.prot_handle = prot_handle;
        param.group_id = &prot_handle->group_id;

        wlan_scan_macaddr_iterate(prot_handle->vap, event->rx_mgmt.ta, p2p_disc_find_peer_GO, &param);

        if (!param.match_found) {
            /* Error: no match in scan table */
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                                 "%s: Error: no match scan entry for GO.\n",
                                 __func__);
            retval = -EINVAL;
        }
        else {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                                 "%s: Update the device frequency to %d.\n",
                                 __func__, param.rx_freq);
            ASSERT(param.rx_freq != 0);
            prot_handle->curr_scan.device_freq = param.rx_freq;
        }
    }

    return retval;
}

/*
 * ****************************** State P2P_PROT_CLIENT_STATE_OFF ******************************
 * Description: Unused state
 */

/* Exit function for this state */
static void
p2p_prot_client_state_OFF_exit(void *ctx)
{
}

/* Entry function for this state */
static void
p2p_prot_client_state_OFF_entry(void *ctx)
{
}

static bool
p2p_prot_client_state_OFF_event(void *ctx, u_int16_t eventtype, u_int16_t event_data_len, void *event_data) 
{
    return false;
}
/*
 * ****************************** State P2P_PROT_CLIENT_STATE_IDLE ******************************
 * Description: Idle and waiting for the connect_to event.
 */

/*
 * We have already check that all the Group Owner parameters are available and valid.
 * Returns true if all parameters are valid.
 */
static bool
check_and_setup_join_params(ieee80211_p2p_prot_client_t prot_handle)
{
    int             retval = EOK;
    ieee80211_ssid  *my_ssid;
    int             freq;

    /* Make sure the SSID is already initialized */
    retval = ieee80211_get_desired_ssid(prot_handle->vap, 0, &my_ssid);
    if (retval != EOK) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
            "%s: Invalid desired SSID. retval=0x%x.\n",
            __func__, retval);
        return false;
    }

    if (prot_handle->join_param.go_op_chan_number == 0) {
        /* It is possible for the channel number to be invalid. For this case,
         * we do not know the Operation channel of this GO. Therefore, we need
         * to do a scan for this GO.
         */
        prot_handle->curr_scan.device_freq = 0;
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
            "%s: Unknown GO frequency. op_class=%d, chan_number=%d\n",
            __func__, prot_handle->join_param.go_op_chan_op_class,
            prot_handle->join_param.go_op_chan_number,
            prot_handle->join_param.go_op_chan_country_code[0],
            prot_handle->join_param.go_op_chan_country_code[1],
            prot_handle->join_param.go_op_chan_country_code[2]);
    }
    else {
        /* Calculate the frequency of GO's Operational Channel */
        freq = wlan_p2p_channel_to_freq((const char *)prot_handle->join_param.go_op_chan_country_code,
                                        prot_handle->join_param.go_op_chan_op_class,
                                        prot_handle->join_param.go_op_chan_number);
        if (freq == -1) {
            prot_handle->curr_scan.device_freq = 0;
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: Unable to convert GO frequency. op_class=%d, chan_number=%d\n",
                __func__, prot_handle->join_param.go_op_chan_op_class,
                prot_handle->join_param.go_op_chan_number,
                prot_handle->join_param.go_op_chan_country_code[0],
                prot_handle->join_param.go_op_chan_country_code[1],
                prot_handle->join_param.go_op_chan_country_code[2]);
        }
        else {
            prot_handle->curr_scan.device_freq = freq;
        }
    }
    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
        "%s: GO's op channel freq=%d.\n", __func__, prot_handle->curr_scan.device_freq);

    prot_handle->curr_scan.probe_req_ssid = &prot_handle->group_id.ssid;

    return true;
}

/* Exit function for this state */
static void
p2p_prot_client_state_IDLE_exit(void *ctx)
{
    ieee80211_p2p_prot_client_t prot_handle = (ieee80211_p2p_prot_client_t)ctx;

    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_PROT,
        "%s: called. ctx=0x%x.\n", __func__, ctx);
}

/* Entry function for this state */
static void
p2p_prot_client_state_IDLE_entry(void *ctx)
{
    ieee80211_p2p_prot_client_t prot_handle = (ieee80211_p2p_prot_client_t)ctx;

    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_PROT,
        "%s: called. ctx=0x%x.\n", __func__, ctx);

    ASSERT(prot_handle->have_connect_to_req == false);
    if (prot_handle->have_connect_to_req) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
            "%s: Error: Outstanding Connect_To request.\n", __func__);
        prot_handle->have_connect_to_req = false;
        p2p_prot_client_complete_connect_to_req(prot_handle, EPERM);
    }

    if (prot_handle->client_evt_handler_registered) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
            "%s: Unregister p2p client event handler.\n", __func__);

        ieee80211_p2p_client_unregister_event_handler(
            prot_handle->p2p_client_handle, ieee80211_p2p_prot_client_p2p_event_handler, prot_handle);
        prot_handle->client_evt_handler_registered = false;
    }
}

static bool
p2p_prot_client_state_IDLE_event(void *ctx, u_int16_t eventtype, u_int16_t event_data_len, void *event_data) 
{
    ieee80211_p2p_prot_client_t prot_handle = (ieee80211_p2p_prot_client_t)ctx;
    sm_event_info               *event;
    ieee80211_connection_state  newstate = P2P_PROT_CLIENT_STATE_OFF;
    ieee80211_connection_event  event_type = (ieee80211_connection_event)eventtype;
    bool                        retval = true;

    event = (sm_event_info *)event_data;

    switch (event_type) {
    case P2P_PROT_CLIENT_EVENT_CONNECT_TO:
    {
        ASSERT(prot_handle->have_connect_to_req == false);

        /* We have already check that all the Group Owner parameters are available */
        if (check_and_setup_join_params(prot_handle)) {
            /* Register the client event callback for the beacon and probe response frames */
            int retval_local;

            ASSERT(prot_handle->client_evt_handler_registered == false);
            retval_local = ieee80211_p2p_client_register_event_handler(
                         prot_handle->p2p_client_handle,
                         ieee80211_p2p_prot_client_p2p_event_handler,
                         prot_handle,
                         IEEE80211_P2P_CLIENT_EVENT_BEACON_BY_SSID | IEEE80211_P2P_CLIENT_EVENT_PROBE_RESP_BY_SSID);
            if (retval_local != EOK) {
                IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                    "%s: Error=0x%x from ieee80211_p2p_client_register_event_handler()\n", __func__, retval_local);
                p2p_prot_client_complete_connect_to_req(prot_handle, EPERM);
                break;
            }
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                "%s: Register p2p client event handler.\n", __func__);
            prot_handle->client_evt_handler_registered = true;

            prot_handle->connect_start_time = OS_GET_TIMESTAMP();
            p2p_prot_client_timer_start(prot_handle, TIMER_CONNECT_EXPIRE, P2P_PROT_CLIENT_MAX_FIND_TIME);

            prot_handle->have_connect_to_req = true;

            if (prot_handle->curr_scan.device_freq == 0) {
                /* The GO operational freq is unknown, then we need to do
                 * a full scan and cannot go to FIND_GO_OPCHAN state.
                 */
                IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                    "%s: Unknown GO's op channel, do a full scan.\n",
                    __func__);
                newstate = P2P_PROT_CLIENT_STATE_DO_FULLSCAN;
            }
            else {
                newstate = P2P_PROT_CLIENT_STATE_FIND_GO_OPCHAN;
            }
        }
        else {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                "%s: Error: got event CONNECT_TO but join params are invalid.\n", __func__);
            p2p_prot_client_complete_connect_to_req(prot_handle, EPERM);
            /* Remain at IDLE state */
        }
        break;
    }

    default:
        /* Try to process it using the state machine common handlers */
        if (proc_event_common(prot_handle, eventtype, event, event_data_len, &newstate)) {
            /* This event is processed by the common handler */
            break;
        }
        /* Else no one process this event */
        retval = false;
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                             "%s: called. rejected event_type=%s. ctx=0x%x.\n",
                             __func__, event_name(event_type), ctx);
        break;
    }

    if (newstate != P2P_PROT_CLIENT_STATE_OFF) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                             "%s: end of function and switching state to %s(%d).\n",
                             __func__, 
                             ieee80211_sm_get_state_name(prot_handle->hsm_handle, newstate),
                             newstate);
        ieee80211_sm_transition_to(prot_handle->hsm_handle, newstate);
    }

    return retval;
}

/*
 * ****************************** State P2P_PROT_CLIENT_STATE_FIND_GO_OPCHAN ******************************
 * Description: Find the GO on Operational channel.
 */

/* Used in State FIND_GO_OPCHAN only. Time intervals between scans (in millisecs) */
#define ST_FIND_GO_OPCHAN_LONG_INTERVAL     500             /* long interval */
#define ST_FIND_GO_OPCHAN_SHORT_INTERVAL    100             /* short interval */
#define ST_FIND_GO_OPCHAN_DO_FULL_SCAN      ((u_int32_t)-1) /* abort and use full scan */

/* Used in State FIND_GO_OPCHAN only. Time from CONNECT start used to determine the scan intervals  */
#define ST_FIND_GO_OPCHAN_PRE_CONFIGTIME_START_TIME         500     /* Time before the GO Config Time */
#define ST_FIND_GO_OPCHAN_POST_CONFIGTIME_CONT_SCAN_TIME    500     /* Time after GO Config Time to do continuous scan */
#define ST_FIND_GO_OPCHAN_POST_CONFIGTIME_LIMIT_TIME        3000    /* Time after GO Config Time to give up and do full scan */

/* 
 * For the FIND_GO_OPCHAN state only, calculate the time to wait for the next scan request.
 * It is assumed that this function is called at the end of a scan.
 */
void
st_FIND_GO_OPCHAN_scan_next_step(
    ieee80211_p2p_prot_client_t prot_handle, u_int32_t *next_scan_delay)
{
    u_int32_t   elapsed_time;

    /* If elapsed time is less than Go Config Time minus GO CONFIG TIME */
    elapsed_time = CONVERT_SYSTEM_TIME_TO_MS(prot_handle->scan_end_time - prot_handle->connect_start_time);

    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
        "%s: elapsed_time=%d, go_config_time=%d\n",
        __func__, elapsed_time, prot_handle->join_param.go_config_time);

    /* Long time before the GO config time */
    if ((elapsed_time + ST_FIND_GO_OPCHAN_PRE_CONFIGTIME_START_TIME) < prot_handle->join_param.go_config_time) {
        /* Do scans with long interval in between them */
        *next_scan_delay = ST_FIND_GO_OPCHAN_LONG_INTERVAL;
        return;
    }

    /* Just before the GO config time */
    if (elapsed_time < prot_handle->join_param.go_config_time) {
        /* Do scans with short interval in between them */
        *next_scan_delay = ST_FIND_GO_OPCHAN_SHORT_INTERVAL;
        return;
    }

    /* Elapsed time is just after GO Config Time, scan aggressively */
    if (elapsed_time < (prot_handle->join_param.go_config_time + ST_FIND_GO_OPCHAN_POST_CONFIGTIME_CONT_SCAN_TIME)) {
        /* Do scans with zero interval in between them */
        *next_scan_delay = 0;
        return;
    }

    /* Elapsed time is sometime after after GO Config Time, scan with short interval */
    if (elapsed_time < (prot_handle->join_param.go_config_time + ST_FIND_GO_OPCHAN_POST_CONFIGTIME_LIMIT_TIME)) {
        /* Do scans with zero interval in between them */
        *next_scan_delay = ST_FIND_GO_OPCHAN_SHORT_INTERVAL;
        return;
    }

    /* When reached time limit, give up and switch to Full scan. */
    *next_scan_delay = ST_FIND_GO_OPCHAN_DO_FULL_SCAN;
    return;
}

/* Exit function for this state */
static void
p2p_prot_client_state_FIND_GO_OPCHAN_exit(void *ctx)
{
    ieee80211_p2p_prot_client_t prot_handle = (ieee80211_p2p_prot_client_t)ctx;
    /*int                         retval;*/

    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_PROT,
        "%s: called. ctx=0x%x.\n", __func__, ctx);

    p2p_prot_client_timer_stop(prot_handle, TIMER_NEXT_SCAN);

    if (prot_handle->wait_for_scan_end_before_ind) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
            "%s: do the delayed CONNECT_TO indication.\n", __func__);
        prot_handle->curr_scan.scan_requested = false;
        prot_handle->wait_for_scan_end_before_ind = false;
        p2p_prot_client_complete_connect_to_req(prot_handle, EOK);
    }

    if (prot_handle->curr_scan.scan_requested)
    {
        if (ieee80211_sm_get_nextstate(prot_handle->hsm_handle) == P2P_PROT_CLIENT_STATE_IDLE)
        {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                "%s: cancel scan since exiting this state and back to IDLE.\n", __func__);
            ieee80211_p2p_prot_cancel_scan(&prot_handle->curr_scan);
        }
        else {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                "%s: active scan but let it continue.\n", __func__);
        }
    }
}

/* Entry function for this state */
static void
p2p_prot_client_state_FIND_GO_OPCHAN_entry(void *ctx)
{
    ieee80211_p2p_prot_client_t prot_handle = (ieee80211_p2p_prot_client_t)ctx;

    ASSERT(prot_handle->curr_scan.device_freq != 0);
    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_PROT,
        "%s: called. ctx=0x%x. Freq=%d\n", __func__, ctx, 
        prot_handle->curr_scan.device_freq);

    prot_handle->wait_for_scan_end_before_ind = false;

    /* STNG TODO: We can find the GO's scan entry and see if it recently updated and assumed that the GO is found. */

    /* Goto GO's operation channel and find it */
    ieee80211_p2p_prot_request_scan(&prot_handle->curr_scan, P2P_PROT_SCAN_TYPE_FIND_DEV);
}

static bool
p2p_prot_client_state_FIND_GO_OPCHAN_event(void *ctx, u_int16_t eventtype, u_int16_t event_data_len, void *event_data) 
{
    ieee80211_p2p_prot_client_t prot_handle = (ieee80211_p2p_prot_client_t)ctx;
    sm_event_info               *event;
    ieee80211_connection_state  newstate = P2P_PROT_CLIENT_STATE_OFF;
    ieee80211_connection_event  event_type = (ieee80211_connection_event)eventtype;
    bool                        retval = true;

    event = (sm_event_info *)event_data;

    switch (event_type) {
    case P2P_PROT_CLIENT_EVENT_SCAN_START:         /* start of channel scan */
    {
        ieee80211_scan_event    *scan_event;

        ASSERT(event_data_len >= sizeof(sm_event_info));
        event = (sm_event_info *)event_data;

        scan_event = &event->scan_cb;

        /* check the ID to match. */
        if (!check_scan_event_id_matched(&prot_handle->curr_scan, scan_event)) {
            break;
        }

        if (!prot_handle->curr_scan.scan_requested) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: Error: unexpected scan start.\n", __func__);
        }
        break;
    }

    case P2P_PROT_CLIENT_EVENT_SCAN_END:           /* End of channel scan */
    case P2P_PROT_CLIENT_EVENT_SCAN_DEQUEUE:       /* Scan request rejected and dequeued. */
    {
        ieee80211_scan_event    *scan_event;
        u_int32_t               next_scan_delay;

        ASSERT(event_data_len >= sizeof(sm_event_info));
        event = (sm_event_info *)event_data;
        scan_event = &event->scan_cb;

        /* Check for cancelled scan confirmation */
        if (event_scan_end_from_cancel_scan(&prot_handle->curr_scan, event->scan_cb.scan_id)) {
            /* Don't do anything special */
        }

        /* check the ID to match. */
        if (!check_scan_event_id_matched(&prot_handle->curr_scan, scan_event)) {
            break;
        }

        if (prot_handle->wait_for_scan_end_before_ind) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                "%s: do the delayed CONNECT_TO indication.\n", __func__);
            prot_handle->curr_scan.scan_requested = false;
            prot_handle->wait_for_scan_end_before_ind = false;
            p2p_prot_client_complete_connect_to_req(prot_handle, EOK);
            newstate = P2P_PROT_CLIENT_STATE_IDLE;
            break;
        }

        if (!prot_handle->curr_scan.scan_requested) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: Error: unexpected SCAN end.\n", __func__);
            /* STNG TODO: how to handle this error condition? Do we let it continue? */
        }
        prot_handle->curr_scan.scan_requested = false;

        if (event_type == P2P_PROT_CLIENT_EVENT_SCAN_DEQUEUE) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                "%s: scan dequeue. Repeat scan request again.\n", __func__);

            next_scan_delay = 0;
        }
        else {
            ASSERT(event_type == P2P_PROT_CLIENT_EVENT_SCAN_END);
            /* Record the time of this scan end */
            record_scan_end_time(prot_handle);

            /* Calculate the next step to do in terms of scanning request. */
            st_FIND_GO_OPCHAN_scan_next_step(prot_handle, &next_scan_delay);
        }

        if (next_scan_delay == 0) {
            /* Start the next scan now */
            ieee80211_p2p_prot_request_scan(&prot_handle->curr_scan, P2P_PROT_SCAN_TYPE_FIND_DEV);
        }
        else if (next_scan_delay == ST_FIND_GO_OPCHAN_DO_FULL_SCAN) {
            /* Give up and do a full scan */
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                "%s: Give up and do a full scan.\n",
                __func__);
            newstate = P2P_PROT_CLIENT_STATE_DO_FULLSCAN;
        }
        else {
            /* Setup a timer to wake up for the next scan */
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                "%s: scheduling next scan in %d msec.\n",
                __func__, next_scan_delay);
            p2p_prot_client_timer_start(prot_handle, TIMER_NEXT_SCAN, next_scan_delay);
        }
        break;
    }

    case P2P_PROT_CLIENT_EVENT_TMR_NEXT_SCAN:
    {
        /* Start the next scan now */
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
            "%s: Start find device Scan.\n",
            __func__);
        ieee80211_p2p_prot_request_scan(&prot_handle->curr_scan, P2P_PROT_SCAN_TYPE_FIND_DEV);
        break;
    }

    case P2P_PROT_CLIENT_EVENT_RX_MGMT:
    {
        bool    matched;
        /*int     retval;*/

        if (prot_handle->wait_for_scan_end_before_ind) {
            /* Already completed the CONNECT_TO */
            break;
        }

        
        if (proc_event_rx_mgmt(prot_handle, event, &matched) != EOK) {
            /* Some problem with this Received management frame. Ignore it */
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: warning: ignore this RX_MGMT.\n", __func__);
        }
        else if (matched) {
            /* (retval == EOK) and the IE states matches */

            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: rx IE bits matched. Complete the CONNECT_TO.\n", __func__);

            if (prot_handle->curr_scan.scan_requested)
            {
                IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                                     "%s: Cancel scan and wait for SCAN_END before indication\n", __func__);
                prot_handle->wait_for_scan_end_before_ind = true;
                ieee80211_p2p_prot_cancel_scan(&prot_handle->curr_scan);
            }
            else {
                p2p_prot_client_complete_connect_to_req(prot_handle, EOK);
                newstate = P2P_PROT_CLIENT_STATE_IDLE;
            }
        }
        else {
            /* We have found the GO but still waiting for the IE bits to change. 
             * Switch state to P2P_PROT_CLIENT_STATE_GO_FOUND */
            newstate = P2P_PROT_CLIENT_STATE_GO_FOUND;
        }
        break;
    }
    
    default:
        /* Try to process it using the state machine common handlers */
        if (proc_event_common(prot_handle, eventtype, event, event_data_len, &newstate)) {
            /* This event is processed by the common handler */
            break;
        }
        /* Else no one process this event */
        retval = false;
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                             "%s: called. rejected event_type=%s. ctx=0x%x.\n",
                             __func__, event_name(event_type), ctx);
        break;
    }

    if (newstate != P2P_PROT_CLIENT_STATE_OFF) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                             "%s: end of function and switching state to %s(%d).\n",
                             __func__, 
                             ieee80211_sm_get_state_name(prot_handle->hsm_handle, newstate),
                             newstate);
        ieee80211_sm_transition_to(prot_handle->hsm_handle, newstate);
    }

    return retval;
}

/*
 * ****************************** State P2P_PROT_CLIENT_STATE_GO_FOUND ******************************
 * Description: GO is found and waiting for the IE bits to change.
 */

/* Number of scans to do for this phase */
#define ST_GO_FOUND_NUM_CONT_SCAN           4   /* first phase is continuous scan */
#define ST_GO_FOUND_NUM_SHORT_INTERVAL_SCAN 10  /* 2nd phase is short interval scan */

#define ST_GO_FOUND_NUM_SHORT_INTERVAL  300
#define ST_GO_FOUND_NUM_LONG_INTERVAL   600
#define ST_GO_FOUND_DO_FULLSCAN         ((u_int32_t)-1)
/* 
 * For the GO_FOUND state only, calculate the time to wait for the next scan request.
 * It is assumed that this function is called at the end of a scan.
 */
void
st_GO_FOUND_scan_next_step(
    ieee80211_p2p_prot_client_t prot_handle, u_int32_t *next_scan_delay)
{
    /* Do the continuous scan */
    if (prot_handle->st_GO_FOUND_scan_cnt <= ST_GO_FOUND_NUM_CONT_SCAN) {
        *next_scan_delay = 0;
        return;
    }

    /* Do the short interval scan */
    if (prot_handle->st_GO_FOUND_scan_cnt <= 
        (ST_GO_FOUND_NUM_CONT_SCAN + ST_GO_FOUND_NUM_SHORT_INTERVAL_SCAN))
    {
        *next_scan_delay = ST_GO_FOUND_NUM_SHORT_INTERVAL;
        return;
    }

    /* Else the long interval scan */
    *next_scan_delay = ST_GO_FOUND_NUM_LONG_INTERVAL;
    return;
}

/* Exit function for this state */
static void
p2p_prot_client_state_GO_FOUND_exit(void *ctx)
{
    ieee80211_p2p_prot_client_t prot_handle = (ieee80211_p2p_prot_client_t)ctx;
    /*int                         retval;*/

    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_PROT,
        "%s: called. ctx=0x%x.\n", __func__, ctx);

    if (prot_handle->wait_for_scan_end_before_ind) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
            "%s: do the delayed CONNECT_TO indication.\n", __func__);
        prot_handle->curr_scan.scan_requested = false;
        prot_handle->wait_for_scan_end_before_ind = false;
        p2p_prot_client_complete_connect_to_req(prot_handle, EOK);
    }

    p2p_prot_client_timer_stop(prot_handle, TIMER_NEXT_SCAN);

    if (prot_handle->curr_scan.scan_requested) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
            "%s: cancel scan since exiting this state.\n", __func__);
        ieee80211_p2p_prot_cancel_scan(&prot_handle->curr_scan);
    }
}

/* Entry function for this state */
static void
p2p_prot_client_state_GO_FOUND_entry(void *ctx)
{
    ieee80211_p2p_prot_client_t prot_handle = (ieee80211_p2p_prot_client_t)ctx;

    ASSERT(prot_handle->curr_scan.device_freq != 0);
    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_PROT,
        "%s: called. ctx=0x%x. Freq=%d, scan_requested=%d\n", __func__, ctx, 
        prot_handle->curr_scan.device_freq, prot_handle->curr_scan.scan_requested);

    prot_handle->wait_for_scan_end_before_ind = false;
    prot_handle->st_GO_FOUND_scan_cnt = 0;

    /* Normally, we should already be scanning on the GO's op channel. If not, then schedule
     * a scan. */
    if (!prot_handle->curr_scan.scan_requested)
    {
        /* Goto GO's operation channel and find it */
        ieee80211_p2p_prot_request_scan(&prot_handle->curr_scan, P2P_PROT_SCAN_TYPE_FIND_DEV);
    }
}

static bool
p2p_prot_client_state_GO_FOUND_event(void *ctx, u_int16_t eventtype, u_int16_t event_data_len, void *event_data) 
{
    ieee80211_p2p_prot_client_t prot_handle = (ieee80211_p2p_prot_client_t)ctx;
    sm_event_info               *event;
    ieee80211_connection_state  newstate = P2P_PROT_CLIENT_STATE_OFF;
    ieee80211_connection_event  event_type = (ieee80211_connection_event)eventtype;
    bool                        retval = true;

    event = (sm_event_info *)event_data;

    switch (event_type) {
    case P2P_PROT_CLIENT_EVENT_SCAN_START:         /* start of channel scan */
    {
        ieee80211_scan_event    *scan_event;

        ASSERT(event_data_len >= sizeof(sm_event_info));
        event = (sm_event_info *)event_data;

        scan_event = &event->scan_cb;

        /* check the ID to match. */
        if (!check_scan_event_id_matched(&prot_handle->curr_scan, scan_event)) {
            break;
        }

        if (!prot_handle->curr_scan.scan_requested) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: Error: unexpected scan start.\n", __func__);
        }

        break;
    }

    case P2P_PROT_CLIENT_EVENT_SCAN_DEQUEUE:       /* Scan request rejected and dequeued. */
    case P2P_PROT_CLIENT_EVENT_SCAN_END:           /* End of channel scan */
    {
        ieee80211_scan_event    *scan_event;
        u_int32_t               next_scan_delay;

        ASSERT(event_data_len >= sizeof(sm_event_info));
        event = (sm_event_info *)event_data;
        scan_event = &event->scan_cb;

        /* Check for cancelled scan confirmation */
        if (event_scan_end_from_cancel_scan(&prot_handle->curr_scan, event->scan_cb.scan_id)) {
            /* Don't do anything special */
        }

        /* check the ID to match. */
        if (!check_scan_event_id_matched(&prot_handle->curr_scan, scan_event)) {
            break;
        }

        if (prot_handle->wait_for_scan_end_before_ind) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                "%s: do the delayed CONNECT_TO indication.\n", __func__);
            prot_handle->curr_scan.scan_requested = false;
            prot_handle->wait_for_scan_end_before_ind = false;
            p2p_prot_client_complete_connect_to_req(prot_handle, EOK);
            newstate = P2P_PROT_CLIENT_STATE_IDLE;
            break;
        }

        if (!prot_handle->curr_scan.scan_requested) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: Error: unexpected SCAN end.\n", __func__);
            /* STNG TODO: how to handle this error condition? Do we let it continue? */
        }

        prot_handle->curr_scan.scan_requested = false;

        if (event_type == P2P_PROT_CLIENT_EVENT_SCAN_DEQUEUE) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                "%s: scan dequeue. Repeat scan request again.\n", __func__);

            next_scan_delay = 0;
        }
        else {
            ASSERT(event_type == P2P_PROT_CLIENT_EVENT_SCAN_END);

            prot_handle->st_GO_FOUND_scan_cnt++;

            /* Calculate the next step to do in terms of scanning request. */
            st_GO_FOUND_scan_next_step(prot_handle, &next_scan_delay);
        }

        if (next_scan_delay == 0) {
            /* Start the next scan now */
            ieee80211_p2p_prot_request_scan(&prot_handle->curr_scan, P2P_PROT_SCAN_TYPE_FIND_DEV);
        }
        else if (next_scan_delay == ST_GO_FOUND_DO_FULLSCAN) {
            /* Give up and do a full scan */
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                "%s: Give up and do a full scan.\n",
                __func__);
            newstate = P2P_PROT_CLIENT_STATE_DO_FULLSCAN;
        }
        else {
            /* Setup a timer to wake up for the next scan */
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                "%s: scheduling next scan in %d msec.\n",
                __func__, next_scan_delay);
            p2p_prot_client_timer_start(prot_handle, TIMER_NEXT_SCAN, next_scan_delay);
        }
        break;
    }

    case P2P_PROT_CLIENT_EVENT_TMR_NEXT_SCAN:
    {
        /* Start the next scan now */
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
            "%s: Start find device Scan.\n",
            __func__);
        ieee80211_p2p_prot_request_scan(&prot_handle->curr_scan, P2P_PROT_SCAN_TYPE_FIND_DEV);
        break;
    }

    case P2P_PROT_CLIENT_EVENT_RX_MGMT:
    {
        bool    matched;
        /*int     retval;*/

        if (prot_handle->wait_for_scan_end_before_ind) {
            /* Already completed the CONNECT_TO */
            break;
        }

        if (proc_event_rx_mgmt(prot_handle, event, &matched)!= EOK) {
            /* Some problem with this Received management frame. Ignore it */
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: warning: ignore this RX_MGMT.\n", __func__);
        }
        else if (matched) {

            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: rx IE bits matched. Complete the CONNECT_TO.\n", __func__);

            if (prot_handle->curr_scan.scan_requested)
            {
                IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                                     "%s: Cancel scan and wait for SCAN_END before indication\n", __func__);
                prot_handle->wait_for_scan_end_before_ind = true;
                ieee80211_p2p_prot_cancel_scan(&prot_handle->curr_scan);
            }
            else {
                p2p_prot_client_complete_connect_to_req(prot_handle, EOK);
                newstate = P2P_PROT_CLIENT_STATE_IDLE;
            }
        }
        /* Else still need to wait for the GO's IE to change */
        break;
    }
    
    default:
        /* Try to process it using the state machine common handlers */
        if (proc_event_common(prot_handle, eventtype, event, event_data_len, &newstate)) {
            /* This event is processed by the common handler */
            break;
        }
        /* Else no one process this event */
        retval = false;
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                             "%s: called. rejected event_type=%s. ctx=0x%x.\n",
                             __func__, event_name(event_type), ctx);
        break;
    }

    if (newstate != P2P_PROT_CLIENT_STATE_OFF) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                             "%s: end of function and switching state to %s(%d).\n",
                             __func__, 
                             ieee80211_sm_get_state_name(prot_handle->hsm_handle, newstate),
                             newstate);
        ieee80211_sm_transition_to(prot_handle->hsm_handle, newstate);
    }

    return retval;
}

/*
 * ********************** State P2P_PROT_CLIENT_STATE_DO_FULLSCAN ***************************
 * Description: GO is not found and operational channel unknown. Do a full scan to find it.
 */

/* Amount of time to increment between each full scan */
#define ST_DO_FULLSCAN_DELAY_INCREMENT  500
/* Max amount of scan interval */
#define ST_DO_FULLSCAN_MAX_DELAY        4000

/* 
 * For the DO_FULLSCAN state only, calculate the time to wait for the next scan request.
 * It is assumed that this function is called at the end of a scan.
 */
void
st_DO_FULLSCAN_scan_next_step(
    ieee80211_p2p_prot_client_t prot_handle, u_int32_t *next_scan_delay)
{
    if (prot_handle->st_DO_FULLSCAN_scan_interval < ST_DO_FULLSCAN_MAX_DELAY) {
        prot_handle->st_DO_FULLSCAN_scan_interval += ST_DO_FULLSCAN_DELAY_INCREMENT;
    }
    *next_scan_delay = prot_handle->st_DO_FULLSCAN_scan_interval;
    return;
}

/* Exit function for this state */
static void
p2p_prot_client_state_DO_FULLSCAN_exit(void *ctx)
{
    ieee80211_p2p_prot_client_t prot_handle = (ieee80211_p2p_prot_client_t)ctx;
    /* int                         retval;*/

    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_PROT,
        "%s: called. ctx=0x%x.\n", __func__, ctx);

    p2p_prot_client_timer_stop(prot_handle, TIMER_NEXT_SCAN);

    if (prot_handle->wait_for_scan_end_before_ind) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
            "%s: do the delayed CONNECT_TO indication.\n", __func__);
        prot_handle->curr_scan.scan_requested = false;
        prot_handle->wait_for_scan_end_before_ind = false;
        p2p_prot_client_complete_connect_to_req(prot_handle, EOK);
    }

    if (prot_handle->curr_scan.scan_requested) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
            "%s: cancel scan since exiting this state.\n", __func__);
        ieee80211_p2p_prot_cancel_scan(&prot_handle->curr_scan);
    }
}

/* Entry function for this state */
static void
p2p_prot_client_state_DO_FULLSCAN_entry(void *ctx)
{
    ieee80211_p2p_prot_client_t prot_handle = (ieee80211_p2p_prot_client_t)ctx;

    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_PROT,
        "%s: called. ctx=0x%x. Freq=%d, scan_requested=%d\n", __func__, ctx, 
        prot_handle->curr_scan.device_freq, prot_handle->curr_scan.scan_requested);

    prot_handle->wait_for_scan_end_before_ind = false;

    prot_handle->st_DO_FULLSCAN_scan_interval = 0;

    /* Normally, we should already be scanning on the GO's op channel. If not, then schedule
     * a scan. */
    if (!prot_handle->curr_scan.scan_requested)
    {
        /* Goto GO's operation channel and find it */
        ieee80211_p2p_prot_request_scan(&prot_handle->curr_scan, P2P_PROT_SCAN_TYPE_FULL);
    }
}

static bool
p2p_prot_client_state_DO_FULLSCAN_event(void *ctx, u_int16_t eventtype, u_int16_t event_data_len, void *event_data) 
{
    ieee80211_p2p_prot_client_t prot_handle = (ieee80211_p2p_prot_client_t)ctx;
    sm_event_info               *event;
    ieee80211_connection_state  newstate = P2P_PROT_CLIENT_STATE_OFF;
    ieee80211_connection_event  event_type = (ieee80211_connection_event)eventtype;
    bool                        retval = true;

    event = (sm_event_info *)event_data;

    switch (event_type) {
    case P2P_PROT_CLIENT_EVENT_SCAN_START:         /* start of channel scan */
    {
        ieee80211_scan_event    *scan_event;

        ASSERT(event_data_len >= sizeof(sm_event_info));
        event = (sm_event_info *)event_data;

        scan_event = &event->scan_cb;

        /* check the ID to match. */
        if (!check_scan_event_id_matched(&prot_handle->curr_scan, scan_event)) {
            break;
        }

        if (!prot_handle->curr_scan.scan_requested) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: Error: unexpected scan start.\n", __func__);
        }

        break;
    }

    case P2P_PROT_CLIENT_EVENT_SCAN_DEQUEUE:       /* Scan request rejected and dequeued. */
    case P2P_PROT_CLIENT_EVENT_SCAN_END:           /* End of channel scan */
    {
        ieee80211_scan_event    *scan_event;
        u_int32_t               next_scan_delay;

        ASSERT(event_data_len >= sizeof(sm_event_info));
        event = (sm_event_info *)event_data;
        scan_event = &event->scan_cb;

        /* Check for cancelled scan confirmation */
        if (event_scan_end_from_cancel_scan(&prot_handle->curr_scan, event->scan_cb.scan_id)) {
            /* Don't do anything special */
        }

        /* check the ID to match. */
        if (!check_scan_event_id_matched(&prot_handle->curr_scan, scan_event)) {
            break;
        }

        if (prot_handle->wait_for_scan_end_before_ind) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                "%s: do the delayed CONNECT_TO indication.\n", __func__);
            prot_handle->curr_scan.scan_requested = false;
            prot_handle->wait_for_scan_end_before_ind = false;
            p2p_prot_client_complete_connect_to_req(prot_handle, EOK);
            newstate = P2P_PROT_CLIENT_STATE_IDLE;
            break;
        }

        if (!prot_handle->curr_scan.scan_requested) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: Error: unexpected SCAN end.\n", __func__);
            /* STNG TODO: how to handle this error condition? Do we let it continue? */
        }
        prot_handle->curr_scan.scan_requested = false;

        if (event_type == P2P_PROT_CLIENT_EVENT_SCAN_DEQUEUE) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                "%s: scan dequeue. Repeat scan request again.\n", __func__);

            next_scan_delay = 0;
        }
        else {
            ASSERT(event_type == P2P_PROT_CLIENT_EVENT_SCAN_END);

            prot_handle->st_GO_FOUND_scan_cnt++;

            /* Calculate the next step to do in terms of scanning request. */
            st_GO_FOUND_scan_next_step(prot_handle, &next_scan_delay);
        }

        if (next_scan_delay == 0) {
            /* Start the next scan now */
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                "%s: Start Full Scan now. scan_cnt=%d.\n",
                __func__, prot_handle->st_GO_FOUND_scan_cnt);
            ieee80211_p2p_prot_request_scan(&prot_handle->curr_scan, P2P_PROT_SCAN_TYPE_FULL);
        }
        else {
            /* Setup a timer to wake up for the next scan */
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                "%s: scheduling next scan in %d msec.\n",
                __func__, next_scan_delay);
            p2p_prot_client_timer_start(prot_handle, TIMER_NEXT_SCAN, next_scan_delay);
        }
        break;
    }

    case P2P_PROT_CLIENT_EVENT_TMR_NEXT_SCAN:
    {
        /* Start the next scan now */
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
            "%s: Start Full Scan. scan_cnt=%d.\n",
            __func__, prot_handle->st_GO_FOUND_scan_cnt);
        ieee80211_p2p_prot_request_scan(&prot_handle->curr_scan, P2P_PROT_SCAN_TYPE_FULL);
        break;
    }

    case P2P_PROT_CLIENT_EVENT_RX_MGMT:
    {
        bool    matched;
        /*int     retval;*/

        if (prot_handle->wait_for_scan_end_before_ind) {
            /* Already completed the CONNECT_TO */
            break;
        }

        
        if (proc_event_rx_mgmt(prot_handle, event, &matched) != EOK) {
            /* Some problem with this Received management frame. Ignore it */
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: warning: ignore this RX_MGMT.\n", __func__);
        }
        else if (matched) {

            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: rx IE bits matched. Complete the CONNECT_TO.\n", __func__);

            if (prot_handle->curr_scan.scan_requested)
            {
                IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                                     "%s: Cancel scan and wait for SCAN_END before indication\n", __func__);
                prot_handle->wait_for_scan_end_before_ind = true;
                ieee80211_p2p_prot_cancel_scan(&prot_handle->curr_scan);
            }
            else {
                p2p_prot_client_complete_connect_to_req(prot_handle, EOK);
                newstate = P2P_PROT_CLIENT_STATE_IDLE;
            }
        }
        else {
            /* We have found the GO but still waiting for the IE bits to change. 
             * Switch state to P2P_PROT_CLIENT_STATE_GO_FOUND */
            newstate = P2P_PROT_CLIENT_STATE_GO_FOUND;
        }
        break;
    }
    
    default:
        /* Try to process it using the state machine common handlers */
        if (proc_event_common(prot_handle, eventtype, event, event_data_len, &newstate)) {
            /* This event is processed by the common handler */
            break;
        }
        /* Else no one process this event */
        retval = false;
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                             "%s: called. rejected event_type=%s. ctx=0x%x.\n",
                             __func__, event_name(event_type), ctx);
        break;
    }

    if (newstate != P2P_PROT_CLIENT_STATE_OFF) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                             "%s: end of function and switching state to %s(%d).\n",
                             __func__, 
                             ieee80211_sm_get_state_name(prot_handle->hsm_handle, newstate),
                             newstate);
        ieee80211_sm_transition_to(prot_handle->hsm_handle, newstate);
    }

    return retval;
}

/**********************************************************/

/* State Machine for the P2P Protocol Discovery */
ieee80211_state_info p2p_prot_client_sm_info[] = {
/*
    Fields of ieee80211_state_info:
    u_int8_t  state;                - the state id
    u_int8_t  parent_state;         - valid for sub states 
    u_int8_t  initial_substate;     - initial substate for the state 
    u_int8_t  has_substates;        - true if this state has sub states 
    const char      *name;          - name of the state 
    void (*ieee80211_hsm_entry);    - entry action routine 
    void (*ieee80211_hsm_exit);     - exit action routine  
    bool (*ieee80211_hsm_event)     - event handler. returns true if event is handled
*/
#define STATE_INFO(st_name)                         \
    {                                               \
        (u_int8_t) P2P_PROT_CLIENT_STATE_##st_name, \
        (u_int8_t) IEEE80211_HSM_STATE_NONE,        \
        (u_int8_t) IEEE80211_HSM_STATE_NONE,        \
        false,                                      \
        "P2P_CLIENT_" #st_name,                     \
        p2p_prot_client_state_##st_name##_entry,    \
        p2p_prot_client_state_##st_name##_exit,     \
        p2p_prot_client_state_##st_name##_event     \
    }
    STATE_INFO(OFF),
    STATE_INFO(IDLE),
    STATE_INFO(FIND_GO_OPCHAN),
    STATE_INFO(GO_FOUND),
    STATE_INFO(DO_FULLSCAN),

#undef STATE_INFO
};

static void p2p_prot_client_sm_debug_print (void *ctx, const char *fmt, ...) 
{
    char                        tmp_buf[256];
    va_list                     ap;
    ieee80211_p2p_prot_client_t prot_info = (ieee80211_p2p_prot_client_t)ctx;
     
    va_start(ap, fmt);
    vsnprintf (tmp_buf, 256, fmt, ap);
    va_end(ap);

    IEEE80211_DPRINTF_VB(prot_info->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT, 
                         "%s",tmp_buf);
}

/********************** End of State Machine support routines *************************/

static void
wlan_p2p_prot_client_update_assoc_req_ie(ieee80211_p2p_prot_client_t prot_info);

/********************** Start of routines that called from external *************************/
/* Note: since these functions are called from external sources, there is no synchronization. */

/*
 * Registered Scan Event Handler.
 */
static void
p2p_prot_client_scan_evhandler(wlan_if_t vaphandle, ieee80211_scan_event *event, void *arg)
{
    ieee80211_p2p_prot_client_t prot_handle = (ieee80211_p2p_prot_client_t)arg;
    sm_event_info               sm_event;

    ASSERT(prot_handle != NULL);

    /* Check that the request ID, event->requestor, is from our device */
    if (prot_handle->curr_scan.my_scan_requestor != event->requestor) {
        /* Scan request is not from me. Ignore it. */
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                          "%s: Scan event ignored, mismatch Request ID. given=%d, mine=%d\n",
                          __func__, event->requestor, prot_handle->curr_scan.my_scan_requestor);
        return;
    }

#if DBG
    /* Note: the prot_handle->scan_context fields are read without state machine lock. So, it might be wrong. */
    if ((event->type == IEEE80211_SCAN_STARTED)   ||
        (event->type == IEEE80211_SCAN_RESTARTED) ||
        (event->type == IEEE80211_SCAN_COMPLETED) ||
        (event->type == IEEE80211_SCAN_DEQUEUED)  ||
        (event->type == IEEE80211_SCAN_PREEMPTED))
    {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                          "%s: EvtScanId=%d type=%s reason=%s chan=%3d\n",
                          __func__,
                          event->scan_id,
                          wlan_scan_notification_event_name(event->type),
                          wlan_scan_notification_reason_name(event->reason),
                          (event->chan != NULL) ? wlan_channel_ieee(event->chan) : 0);
    }
#endif  //DBG

    OS_MEMZERO(&sm_event, sizeof(sm_event_info));
    OS_MEMCPY(&sm_event.scan_cb, event, sizeof(ieee80211_scan_event));
    sm_event.event_type = P2P_PROT_CLIENT_EVENT_NULL;

    switch (event->type)
    {
    case IEEE80211_SCAN_STARTED:
        sm_event.event_type = P2P_PROT_CLIENT_EVENT_SCAN_START;
        ieee80211_sm_dispatch(prot_handle->hsm_handle, sm_event.event_type, sizeof(sm_event_info), &sm_event);
        break;

    case IEEE80211_SCAN_COMPLETED:
        sm_event.event_type = P2P_PROT_CLIENT_EVENT_SCAN_END;
        ieee80211_sm_dispatch(prot_handle->hsm_handle, sm_event.event_type, sizeof(sm_event_info), &sm_event);
        break;

    case IEEE80211_SCAN_DEQUEUED:
        sm_event.event_type = P2P_PROT_CLIENT_EVENT_SCAN_DEQUEUE;
        ieee80211_sm_dispatch(prot_handle->hsm_handle, sm_event.event_type, sizeof(sm_event_info), &sm_event);
        break;

    /* We don't handle Scan Pre-empted and Restart for now. */
    case IEEE80211_SCAN_RESTARTED:
        break;
    case IEEE80211_SCAN_PREEMPTED:
        break;

    case IEEE80211_SCAN_RADIO_MEASUREMENT_START:
    case IEEE80211_SCAN_RADIO_MEASUREMENT_END:
    case IEEE80211_SCAN_HOME_CHANNEL:
    case IEEE80211_SCAN_FOREIGN_CHANNEL:
    case IEEE80211_SCAN_BSSID_MATCH :
    case IEEE80211_SCAN_FOREIGN_CHANNEL_GET_NF:
        break;

    default:
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                          "%s: scan_id=%d UNKNOWN EVENT=%02d reason=%02d chan=%3d\n",
                          __func__,
                          event->scan_id,
                          event->type,
                          event->reason,
                          (event->chan != NULL) ? wlan_channel_ieee(event->chan) : 0);
        break;
    }
}

/*
 * Event handler callback function from p2p client module.
 */
static void
ieee80211_p2p_prot_client_p2p_event_handler(
    wlan_p2p_client_t p2p_client_handle, ieee80211_p2p_client_event *event, void *arg)
{
    ieee80211_p2p_prot_client_t h_prot_client = (ieee80211_p2p_prot_client_t)arg;

    switch(event->type) {
    case IEEE80211_P2P_CLIENT_EVENT_BEACON_BY_SSID:
    case IEEE80211_P2P_CLIENT_EVENT_PROBE_RESP_BY_SSID:
    {
        sm_event_info                   sm_event;

        /* Send a message to the P2P Protocol Client state machine */
        OS_MEMZERO(&sm_event, sizeof(sm_event_info));

        sm_event.event_type = P2P_PROT_CLIENT_EVENT_RX_MGMT;
        sm_event.rx_mgmt.event_type = event->type;
        sm_event.rx_mgmt.in_group_formation = event->u.filter_by_ssid_info.in_group_formation;
        sm_event.rx_mgmt.wps_selected_registrar = event->u.filter_by_ssid_info.wps_selected_registrar;
        IEEE80211_ADDR_COPY(sm_event.rx_mgmt.ta, event->u.filter_by_ssid_info.ta);

        IEEE80211_DPRINTF_VB(p2p_client_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                             "%s: event type=%d, grpform=%d, sel_reg=%d\n",
                             __func__, event->type, event->u.filter_by_ssid_info.in_group_formation,
                             event->u.filter_by_ssid_info.wps_selected_registrar);

        ieee80211_sm_dispatch(h_prot_client->hsm_handle, sm_event.event_type, sizeof(sm_event_info), &sm_event);
        break;
    }
    default:
        break;
    }
}

/*
 * Routine to attach the P2P Protocol module for the client.
 * Returns EOK if success. Else some non-zero error code.
 */
int
wlan_p2p_prot_client_attach(
    wlan_p2p_client_t                       p2p_client_handle, 
    ieee80211_p2p_prot_client_event_handler evhandler,
    void                                    *callback_arg)
{
    ieee80211_p2p_prot_client_t h_prot_client = NULL;
    osdev_t                     oshandle;
    int                         retval = EOK;
    struct wlan_mlme_app_ie     *h_app_ie = NULL;
    bool                        scan_registered = false;
    bool                        got_requestor_id = false;
    ieee80211_hsm_t             hsm_handle = NULL;

    if (p2p_client_handle->prot_info) {
        /* Already attached. */
        IEEE80211_DPRINTF_VB(p2p_client_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                          "%s: Error, already attached. handle=%pK\n",
                          __func__, p2p_client_handle->prot_info);
        return -EINVAL;
    }

    oshandle = ieee80211com_get_oshandle(p2p_client_handle->devhandle);
    h_prot_client = (ieee80211_p2p_prot_client_t)OS_MALLOC(oshandle, sizeof(struct ieee80211_p2p_prot_client), 0);
    if (h_prot_client == NULL) {
        IEEE80211_DPRINTF_VB(p2p_client_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                          "%s: unable to alloc memory. size=%d\n",
                          __func__, sizeof(struct ieee80211_p2p_prot_client));
        retval = -ENOMEM;
        goto failed_client_attach;
    }
    OS_MEMZERO(h_prot_client, sizeof(struct ieee80211_p2p_prot_client));

    h_prot_client->vap = p2p_client_handle->vap;
    h_prot_client->os_handle = oshandle;
    h_prot_client->p2p_client_handle = p2p_client_handle;
    spin_lock_init(&h_prot_client->lock);

    /* Create State Machine and start */
    hsm_handle = ieee80211_sm_create(oshandle, 
                     "P2PPROTCLIENT",
                     (void *)h_prot_client, 
                     P2P_PROT_CLIENT_STATE_IDLE,
                     p2p_prot_client_sm_info,
                     sizeof(p2p_prot_client_sm_info)/sizeof(ieee80211_state_info),
                     MAX_QUEUED_EVENTS,
                     sizeof(sm_event_info), /* size of event data */
                     MESGQ_PRIORITY_NORMAL,
                     /* Note for darwin, we get more accurate timings for the NOA start and stop when synchronous */
                     OS_MESGQ_CAN_SEND_SYNC() ? IEEE80211_HSM_SYNCHRONOUS :
                                                IEEE80211_HSM_ASYNCHRONOUS,
                     p2p_prot_client_sm_debug_print,
                     p2p_prot_client_sm_event_name,
                     IEEE80211_N(p2p_prot_client_sm_event_name)); 
    if (!hsm_handle) {
        IEEE80211_DPRINTF(h_prot_client->vap, IEEE80211_MSG_P2P_PROT, 
            "%s : ieee80211_sm_create failed for p2p protocol client state machine.\n", __func__);
        retval = -ENOMEM;
        goto failed_client_attach;
    }

    h_prot_client->hsm_handle = hsm_handle;

    /* Create an instance of Application IE so that we can add P2P IE */
    h_app_ie = wlan_mlme_app_ie_create(h_prot_client->vap);
    if (h_app_ie == NULL) {
        IEEE80211_DPRINTF(h_prot_client->vap, IEEE80211_MSG_ANY, 
            "%s: Error in allocating Application IE\n", __func__);
        retval = -ENOMEM;
        goto failed_client_attach;
    }
    h_prot_client->app_ie_handle = h_app_ie;

    /* Register event handler with Scanner */
    retval = wlan_scan_register_event_handler(p2p_client_handle->vap,
                                              p2p_prot_client_scan_evhandler,
                                              h_prot_client);
    if (retval != EOK) {
        IEEE80211_DPRINTF(p2p_client_handle->vap, IEEE80211_MSG_P2P_PROT, 
                          "%s: wlan_scan_register_event_handler() failed handler=%08p,%08p status=%08X\n",
                          __func__, p2p_prot_client_scan_evhandler, h_prot_client, retval);
        goto failed_client_attach;
    }
    scan_registered = true;

    /* Get my unique scan requestor ID so that I can tagged all my scan requests */
    retval = wlan_scan_get_requestor_id(p2p_client_handle->vap, (u_int8_t *) "P2P_PROT_CLIENT", 
                                        &(h_prot_client->curr_scan.my_scan_requestor));
    if (retval != EOK) {
        IEEE80211_DPRINTF(p2p_client_handle->vap, IEEE80211_MSG_P2P_PROT, 
                          "%s: wlan_scan_get_requestor_id() failed status=%08X\n",
                          __func__, retval);
        retval = -ENOMEM;
        goto failed_client_attach;
    }
    got_requestor_id = true;

    p2p_prot_client_timer_init(h_prot_client);

    p2p_client_handle->prot_info = h_prot_client;

    h_prot_client->callback_handler = evhandler;
    h_prot_client->callback_arg = callback_arg;

    /* Do some initialization of the prot_handle->curr_scan structure */
    h_prot_client->curr_scan.vap = h_prot_client->vap;
    h_prot_client->curr_scan.partial_scan_chan_index = 0;

    return EOK;

failed_client_attach:

    /* Some failure in attaching this module */

    if (got_requestor_id) {
        p2p_prot_client_timer_cleanup(h_prot_client);

        wlan_scan_clear_requestor_id(p2p_client_handle->vap, h_prot_client->curr_scan.my_scan_requestor);
    }

    if (scan_registered) {
        int status;

        status = wlan_scan_unregister_event_handler(p2p_client_handle->vap,
                                                    p2p_prot_client_scan_evhandler,
                                                    h_prot_client);
        if (status != EOK) {
            IEEE80211_DPRINTF(p2p_client_handle->vap, IEEE80211_MSG_P2P_PROT, 
                              "%s: wlan_scan_unregister_event_handler() failed handler=%08p,%08p status=%08X\n",
                              __func__, p2p_prot_client_scan_evhandler, h_prot_client, status);
        }
    }

    if (h_app_ie != NULL) {
        wlan_mlme_remove_ie_list(h_app_ie);
        h_app_ie = NULL;
    }

    /* Delete the State Machine */
    if (hsm_handle) {
        ieee80211_sm_delete(hsm_handle);
    }

    if (h_prot_client) {
        OS_FREE(h_prot_client);
    }

    return retval;
}

/*
 * Routine to detach the P2P Protocol module for the client.
 */
void
wlan_p2p_prot_client_detach(wlan_p2p_client_t p2p_client_handle)
{
    ieee80211_p2p_prot_client_t h_prot_client;
    int                         retval;

    if (p2p_client_handle->prot_info == NULL) {
        /* Already detached. */
        IEEE80211_DPRINTF_VB(p2p_client_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                          "%s: Error, not attached. Nothing to do\n", __func__);
        return;
    }
    h_prot_client = p2p_client_handle->prot_info;

    p2p_prot_client_timer_cleanup(h_prot_client);
    wlan_scan_clear_requestor_id(p2p_client_handle->vap, h_prot_client->curr_scan.my_scan_requestor);

    retval = wlan_scan_unregister_event_handler(p2p_client_handle->vap,
                                                p2p_prot_client_scan_evhandler,
                                                h_prot_client);
    if (retval != EOK) {
        IEEE80211_DPRINTF(p2p_client_handle->vap, IEEE80211_MSG_P2P_PROT, 
                          "%s: wlan_scan_unregister_event_handler() failed handler=%08p,%08p status=%08X\n",
                          __func__, p2p_prot_client_scan_evhandler, h_prot_client, retval);
    }

    if (h_prot_client->secondaryDeviceTypes) {
        OS_FREE(h_prot_client->secondaryDeviceTypes);
    }

    /* Delete the State Machine */
    if (h_prot_client->hsm_handle) {
        ieee80211_sm_delete(h_prot_client->hsm_handle);
    }

    wlan_mlme_remove_ie_list(h_prot_client->app_ie_handle);

    p2p_client_handle->prot_info = NULL;

    OS_FREE(h_prot_client);
}

/**
 * To set a parameter in the P2P Protocol client module.
 * @param p2p_client_handle     : handle to the p2p
 *                              clientobject.
 * @param param_type            : type of parameter to set.
 * @param param                 : new parameter.
 * @return Error Code. Equal 0 if success.
 */
int wlan_p2p_prot_client_set_param(wlan_p2p_client_t p2p_client_handle,
                                   wlan_p2p_client_param param, u_int32_t val)
{
    int                         ret = 0;
    ieee80211_p2p_prot_client_t prot_info;

    if (p2p_client_handle->prot_info == NULL) {
        IEEE80211_DPRINTF_VB(p2p_client_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                          "%s: Error, protocol module not attached.\n",
                          __func__);
        return EINVAL;
    }
    prot_info = p2p_client_handle->prot_info;

    switch(param) {
    case WLAN_P2P_CLIENT_DEVICE_CAPABILITY:
        prot_info->dev_cap = val;
        wlan_p2p_prot_client_update_assoc_req_ie(prot_info);
        break;

    case WLAN_P2P_CLIENT_WPS_VERSION_ENABLED:
        prot_info->wpsVersionEnabled = (u_int8_t)val;
        break;

    case WLAN_P2P_CLIENT_DEVICE_CONFIG_METHODS:
        prot_info->configMethods = (u_int16_t)val;
        wlan_p2p_prot_client_update_assoc_req_ie(prot_info);
        break;

    default:
        ret = EINVAL;
    }

    return ret;
}

/* Send event to inform statemachine that the join parameters are updated. */
static void
update_client_join_param(wlan_p2p_client_t p2p_client_handle, wlan_p2p_prot_client_join_param *param)
{
    sm_event_info               sm_event;
    ieee80211_p2p_prot_client_t prot_handle;
    /*
        *int                         ret = EOK;
    */
    if (p2p_client_handle->prot_info == NULL) {
        IEEE80211_DPRINTF_VB(p2p_client_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                          "%s: Error, protocol module not attached.\n",
                          __func__);
        return;
    }
    prot_handle = p2p_client_handle->prot_info;

    IEEE80211_DPRINTF_VB(p2p_client_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                         "%s: grpform=%d, sel_reg=%d\n",
                         __func__, param->p2p_ie_in_group_formation,
                         param->wps_ie_wait_for_wps_ready);

    OS_MEMZERO(&sm_event, sizeof(sm_event_info));

    sm_event.event_type = P2P_PROT_CLIENT_EVENT_UPDATE_JOIN_PARAM;

    OS_MEMCPY(&sm_event.join_param, param, sizeof(wlan_p2p_prot_client_join_param));

    ieee80211_sm_dispatch(prot_handle->hsm_handle, sm_event.event_type, sizeof(sm_event_info), &sm_event);
}

/* Send event to inform statemachine that the group ID parameters are updated. */
static void
update_client_group_id(wlan_p2p_client_t p2p_client_handle, wlan_p2p_prot_grp_id *param)
{
    sm_event_info               sm_event;
    ieee80211_p2p_prot_client_t prot_handle;

#if DBG
    char    ssid_str[IEEE80211_NWID_LEN + 1];


    ASSERT(param->ssid.len <= IEEE80211_NWID_LEN);
    OS_MEMCPY(ssid_str, param->ssid.ssid, param->ssid.len);
    ssid_str[param->ssid.len] = 0;

    IEEE80211_DPRINTF_VB(p2p_client_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                         "%s: ssid=%s, dev addr="MAC_ADDR_FMT"\n",
                         __func__, ssid_str, SPLIT_MAC_ADDR(param->device_addr));
#endif

    if (p2p_client_handle->prot_info == NULL) {
        IEEE80211_DPRINTF_VB(p2p_client_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                          "%s: Error, protocol module not attached.\n",
                          __func__);
        return;
    }
    prot_handle = p2p_client_handle->prot_info;

    OS_MEMZERO(&sm_event, sizeof(sm_event_info));

    sm_event.event_type = P2P_PROT_CLIENT_EVENT_UPDATE_GROUP_ID;

    OS_MEMCPY(&sm_event.group_id, param, sizeof(wlan_p2p_prot_grp_id));

    ieee80211_sm_dispatch(prot_handle->hsm_handle, sm_event.event_type, sizeof(sm_event_info), &sm_event);
}

int
wlan_p2p_prot_client_connect_to(wlan_p2p_client_t p2p_client_handle)
{
    ieee80211_p2p_prot_client_t prot_handle;
    /*
        int                         ret = EOK;
    */ 
    sm_event_info               sm_event;

    if (p2p_client_handle->prot_info == NULL) {
        IEEE80211_DPRINTF_VB(p2p_client_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                          "%s: Error, protocol module not attached.\n",
                          __func__);
        return -EINVAL;
    }
    prot_handle = p2p_client_handle->prot_info;

    OS_MEMZERO(&sm_event, sizeof(sm_event_info));

    sm_event.event_type = P2P_PROT_CLIENT_EVENT_CONNECT_TO;

    IEEE80211_DPRINTF_VB(p2p_client_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                         "%s: Send CONNECT_TO event.\n",
                         __func__);

    ieee80211_sm_dispatch(prot_handle->hsm_handle, sm_event.event_type, sizeof(sm_event_info), &sm_event);

    return EOK;
}

int wlan_p2p_prot_client_reset(wlan_p2p_client_t p2p_client_handle)
{
    ieee80211_p2p_prot_client_t prot_handle;
    /*
       * int                         ret = EOK;
    */
    sm_event_info               sm_event;

    if (p2p_client_handle->prot_info == NULL) {
        IEEE80211_DPRINTF_VB(p2p_client_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                          "%s: Error, protocol module not attached.\n",
                          __func__);
        return -EINVAL;
    }
    prot_handle = p2p_client_handle->prot_info;

    OS_MEMZERO(&sm_event, sizeof(sm_event_info));

    sm_event.event_type = P2P_PROT_CLIENT_EVENT_RESET;

    IEEE80211_DPRINTF_VB(p2p_client_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                         "%s: Send Reset event.\n",
                         __func__);

    ieee80211_sm_dispatch(prot_handle->hsm_handle, sm_event.event_type, sizeof(sm_event_info), &sm_event);

    return EOK;
}

/*
 * Get the BSS channel information. Return 0 if successful.
 */
int
wlan_p2p_prot_get_chan_info(wlan_dev_t devhandle, wlan_if_t vap,
                            u_int8_t *CountryCode, u_int8_t *OpClass,
                            u_int8_t *OpChan)
{
    wlan_chan_t         bss_channel;
    u_int32_t           bss_frequency;
    int                 retval;

    /* Get the current country code */
    ieee80211_getCurrentCountryISO(dev_handle, (char *)CountryCode);

    /* The third octet of the Country String field is set to 0x04 to indicate that Table J-4 is used. */
    IEEE80211_DPRINTF_VB(vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                         "%s: Overwrite country code 3rd octet from 0x%x to 0x%x.\n",
                         __func__, CountryCode[2], 0x04);
    CountryCode[2] = 0x04;

    bss_channel = wlan_get_bss_channel(vap);
    if (bss_channel == 0) {
        /* Error: this VAP is not associated */
        IEEE80211_DPRINTF_VB(vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                             "%s: Error: wlan_get_bss_channel ret 0 indicating that VAP is not associated.\n",
                             __func__);
        return -EIO;
    }
    bss_frequency = wlan_channel_frequency(bss_channel);
    if (bss_frequency == IEEE80211_CHAN_ANY) {
        /* Error: this VAP is not associated */
        IEEE80211_DPRINTF_VB(vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                             "%s: Error: wlan_channel_frequency ret error.\n",
                             __func__);
        return -EIO;
    }

    retval = wlan_p2p_freq_to_channel((const char *)CountryCode, bss_frequency, OpClass, OpChan);
    if (retval != 0) {
        /* Error: this VAP is not associated */
        IEEE80211_DPRINTF_VB(vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                             "%s: Error: wlan_p2p_freq_to_channel ret=%d. bss_frequency=%d\n",
                             __func__, retval, bss_frequency);
        return -EIO;
    }
    return EOK;
}

/**
 * To set a parameter array in the P2P Protocol client module.
 *  @param p2p_client_handle : handle to the p2p client object.
 *  @param param         : config paramaeter.
 *  @param byte_arr      : byte array .
 *  @param len           : length of byte array.
 *  @return 0  on success and -ve on failure.
 */
int
wlan_p2p_prot_client_set_param_array(wlan_p2p_client_t p2p_client_handle, 
                                     wlan_p2p_client_param param,
                                     u_int8_t *byte_arr, u_int32_t len)
{
    ieee80211_p2p_prot_client_t prot_info;
    int                         ret = EOK;

    if (p2p_client_handle->prot_info == NULL) {
        IEEE80211_DPRINTF_VB(p2p_client_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                          "%s: Error, protocol module not attached.\n",
                          __func__);
        return EINVAL;
    }
    prot_info = p2p_client_handle->prot_info;

    switch (param) {
    /* We should use the BSSID of the GO to optimize our find. The BSSID is from the
     * OID_DOT11_DESIRED_BSSID_LIST */ 

    case WLAN_P2P_CLIENT_DEVICE_NAME:
        if (byte_arr) {
            OS_MEMCPY(prot_info->p2p_dev_name, (const char *)byte_arr, len);
            /* ensure that p2p_dev_name is null-terminated */
            prot_info->p2p_dev_name[len] = '\0';
            prot_info->p2p_dev_name_len = len;
            wlan_p2p_prot_client_update_assoc_req_ie(prot_info);
        }
        else {
            prot_info->p2p_dev_name_len = 0;
        }
        break;

    case WLAN_P2P_CLIENT_PRIMARY_DEVICE_TYPE:
        OS_MEMCPY(&prot_info->primaryDeviceType, byte_arr, len);
        wlan_p2p_prot_client_update_assoc_req_ie(prot_info);
        break;

    case WLAN_P2P_CLIENT_SECONDARY_DEVICE_TYPES:
        if (prot_info->secondaryDeviceTypes) {
            OS_FREE(prot_info->secondaryDeviceTypes);
        }
        prot_info->secondaryDeviceTypes = byte_arr;
        prot_info->numSecDeviceTypes = len/IEEE80211_P2P_DEVICE_TYPE_LEN;
        wlan_p2p_prot_client_update_assoc_req_ie(prot_info);
        break;

    case WLAN_P2P_CLIENT_P2P_DEVICE_ADDR:
        ASSERT(len == sizeof(prot_info->p2pDeviceAddr));
        OS_MEMCPY(prot_info->p2pDeviceAddr, byte_arr, len);

        wlan_p2p_prot_client_update_assoc_req_ie(prot_info);
        break;

    case WLAN_P2P_PROT_CLIENT_GROUP_ID:
        if (len != sizeof(wlan_p2p_prot_grp_id)) {
            IEEE80211_DPRINTF_VB(p2p_client_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                              "%s: Error, Set Group ID, len=%d incorrect. expected=%d\n",
                              __func__, len, sizeof(wlan_p2p_prot_grp_id));
            return EINVAL;
        }
        update_client_group_id(p2p_client_handle, (wlan_p2p_prot_grp_id *)byte_arr);
        break;

    case WLAN_P2P_PROT_CLIENT_JOIN_PARAM:
        if (len != sizeof(wlan_p2p_prot_client_join_param)) {
            IEEE80211_DPRINTF_VB(p2p_client_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                              "%s: Error, Set JOIN_PARAM, len=%d incorrect. expected=%d\n",
                              __func__, len, sizeof(wlan_p2p_prot_client_join_param));
            return EINVAL;
        }
        update_client_join_param(p2p_client_handle, (wlan_p2p_prot_client_join_param *)byte_arr);
        break;

    default:
        ret = EINVAL;
        break;
    }

    return ret;
}

/*
 * This routine is called to update the P2P IE for the Association Request frame.
 */
static void
wlan_p2p_prot_client_update_assoc_req_ie(ieee80211_p2p_prot_client_t prot_info)
{
    p2pbuf                  p2p_buf, *ies;
    /*
        u_int8_t                *frm;
    */
    u_int8_t                *ielen_ptr;
    int                     ie_len = 0;

    spin_lock(&(prot_info->lock));  /* mutual exclusion with OS updating the parameters */

    ies = &p2p_buf;
    /* Use the temporary buffer to store the IE */
    p2pbuf_init(ies, prot_info->tmp_ie_buf, sizeof(prot_info->tmp_ie_buf));
    ielen_ptr = p2pbuf_add_ie_hdr(ies);     /* Add the P2P IE */

    /* Add P2P Capability */
    /* Journi's code also put 0 for group capability */
    p2pbuf_add_capability(ies, prot_info->dev_cap, 0);

    /* Add Extended Listen Timing */
    if (prot_info->ext_listen_interval) {
        p2pbuf_add_ext_listen_timing(ies, prot_info->ext_listen_period, prot_info->ext_listen_interval);
    }

    /* Add P2P Device Info */
    p2pbuf_add_device_info(ies, prot_info->p2pDeviceAddr,
                           (u_int8_t *)prot_info->p2p_dev_name, prot_info->p2p_dev_name_len, 
                           prot_info->configMethods, prot_info->primaryDeviceType,
                           prot_info->numSecDeviceTypes, prot_info->secondaryDeviceTypes);

    p2pbuf_update_ie_hdr(ies, ielen_ptr, &ie_len);  /* last thing to do is update the IE length */

    ie_len = ie_len + 2;         /* plus 2 bytes for IE header */

    if (wlan_mlme_app_ie_set(prot_info->app_ie_handle, IEEE80211_FRAME_TYPE_ASSOCREQ,
                             prot_info->tmp_ie_buf, ie_len) != 0) {
        IEEE80211_DPRINTF(prot_info->vap, IEEE80211_MSG_ANY,
                          "%s wlan_mlme_app_ie_set failed \n",__func__);
    }

    spin_unlock(&(prot_info->lock));  /* mutual exclusion with OS updating the parameters */

}

