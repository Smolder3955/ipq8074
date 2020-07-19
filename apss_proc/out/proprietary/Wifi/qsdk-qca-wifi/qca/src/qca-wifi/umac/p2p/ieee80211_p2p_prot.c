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
 * This file contains the routine for P2P Discovery. It is used in the new higher API for win8 Wifi-Direct.
 */


#include "ieee80211_p2p_device_priv.h"
#include "ieee80211_ie_utils.h"
#include "ieee80211_channel.h"
#include "ieee80211_p2p_proto.h"
#include "ieee80211_p2p_ie.h"
#include "ieee80211_defines.h"
#include "ieee80211_rateset.h"
#include "ieee80211_sm.h"
#include "ieee80211_p2p_prot_priv.h"
#include "ieee80211_p2p_prot_utils.h"

/* This module is only applicable when UMAC P2P and the Lower P2P Protocol modules are enabled */
#if UMAC_SUPPORT_P2P && UMAC_SUPPORT_P2P_PROT

#ifndef MAC2STR
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#endif

/* Minimum Time in millisec to find a desired peer using discovery scan */
#define P2P_PROT_MIN_TIME_TO_FIND_PEER  500

/* Minimum Duration in millisec to do a general full scan to find any p2p devices */
#define P2P_PROT_MIN_DURATION_FOR_FULLSCAN  4000

#define MAX_QUEUED_EVENTS   6   /* max. number of queued events in state machine */

/* NOTE: please update ieee80211_connection_event when this array is changed */ 
static const char* p2p_prot_disc_sm_event_name[] = {
    "NULL", /* P2P_DISC_EVENT_NULL = 0 */
    "START", /* P2P_DISC_EVENT_START */
    "RESET", /* P2P_DISC_EVENT_RESET */
    "END", /* P2P_DISC_EVENT_END */
    "TX_GO_NEG_REQ", /* P2P_DISC_EVENT_TX_GO_NEG_REQ */
    "TX_GO_NEG_RESP", /* P2P_DISC_EVENT_TX_GO_NEG_RESP */
    "TX_GO_NEG_CONF", /* P2P_DISC_EVENT_TX_GO_NEG_CONF */
    "TX_INVITE_REQ", /* P2P_DISC_EVENT_TX_INVITE_REQ */
    "TX_INVITE_RESP", /* P2P_DISC_EVENT_TX_INVITE_RESP */
    "TX_PROV_DISC_REQ", /* P2P_DISC_EVENT_TX_PROV_DISC_REQ */
    "TX_PROV_DISC_RESP", /* P2P_DISC_EVENT_TX_PROV_DISC_RESP */
    "START_GO", /* P2P_DISC_EVENT_START_GO */
    "START_CLIENT", /* P2P_DISC_EVENT_START_CLIENT */
    "SCAN_START", /* P2P_DISC_EVENT_SCAN_START */
    "SCAN_END", /* P2P_DISC_EVENT_SCAN_END */
    "SCAN_DEQUEUE", /* P2P_DISC_EVENT_SCAN_DEQUEUE */
    "ON_CHAN_START", /* P2P_DISC_EVENT_ON_CHAN_START */
    "ON_CHAN_END", /* P2P_DISC_EVENT_ON_CHAN_END */
    "TX_COMPLETE", /* P2P_DISC_EVENT_TX_COMPLETE */
    "RX_MGMT", /* P2P_DISC_EVENT_RX_MGMT */
    "SCAN_INIT", /* P2P_DISC_EVENT_DISC_SCAN_INIT */
    "DISC_SCAN_REQ", /* P2P_DISC_EVENT_DISC_SCAN_REQ */
    "LISTEN_ST_DISC_REQ", /* P2P_DISC_EVENT_LISTEN_ST_DISC_REQ */
    "START_PEND_DISCSCAN", /* P2P_DISC_EVENT_START_PEND_DISCSCAN */
    "APP_IE_UPDATE", /* P2P_DISC_EVENT_APP_IE_UPDATE */
    "DISC_SCAN_STOP", /* P2P_DISC_EVENT_DISC_SCAN_STOP */

    "TMR_DISC_LISTEN", /* P2P_DISC_EVENT_TMR_DISC_LISTEN */
    "TMR_SET_CHAN_SWITCH", /* P2P_DISC_EVENT_TMR_SET_CHAN_SWITCH */
    "TMR_TX_RETRY", /* P2P_DISC_EVENT_TMR_TX_RETRY */
    "TMR_RETRY_SET_CHAN", /* P2P_DISC_EVENT_TMR_RETRY_SET_CHAN */
    "TMR_RETRY_SET_SCAN", /* P2P_DISC_EVENT_TMR_RETRY_SET_SCAN */
    "TMR_DISC_SCAN_EXPIRE", /* P2P_DISC_EVENT_TMR_DISC_SCAN_EXPIRE */
    "TMR_TX_REQ_EXPIRE", /* P2P_DISC_EVENT_TMR_TX_REQ_EXPIRE */
};

/* This define is used inside a switch() for all the transmit Action frame cases */
#define    CASE_ALL_TX_ACTION_FRAMES                                                            \
    case P2P_DISC_EVENT_TX_GO_NEG_REQ:      /* send GO Negotiation Request frame. */            \
    case P2P_DISC_EVENT_TX_GO_NEG_RESP:     /* send GO Negotiation Response frame. */           \
    case P2P_DISC_EVENT_TX_GO_NEG_CONF:     /* send GO Negotiation Confirm frame. */            \
    case P2P_DISC_EVENT_TX_INVITE_REQ:      /* send Invitation Request frame. */                \
    case P2P_DISC_EVENT_TX_INVITE_RESP:     /* send Invitation Response frame. */               \
    case P2P_DISC_EVENT_TX_PROV_DISC_REQ:   /* send Provisioning Discovery Request frame. */    \
    case P2P_DISC_EVENT_TX_PROV_DISC_RESP:  /* send Provisioning Discovery Response frame. */

static const char* event_name(ieee80211_connection_event event)
{
    return (p2p_prot_disc_sm_event_name[event]);
}

static int create_and_add_probe_resp_ie(wlan_p2p_prot_t prot_handle);

static void
p2p_discoverability_listen_stop(wlan_p2p_prot_t prot_handle);

static int 
p2p_prot_build_group_neg_req_frame(
    wlan_p2p_prot_t prot_handle, 
    tx_event_info   *tx_fr_info);

static int 
p2p_prot_build_group_neg_resp_frame(
    wlan_p2p_prot_t prot_handle, 
    tx_event_info   *tx_fr_info);

static int 
p2p_prot_build_group_neg_conf_frame(
    wlan_p2p_prot_t prot_handle, 
    tx_event_info   *tx_fr_info);

static int 
p2p_prot_build_prov_disc_req_frame(
    wlan_p2p_prot_t prot_handle, 
    tx_event_info   *tx_fr_info);

static int 
p2p_prot_build_prov_disc_resp_frame(
    wlan_p2p_prot_t prot_handle, 
    tx_event_info   *tx_fr_info);

static void
complete_tx_action_frame_event(wlan_p2p_prot_t prot_handle, tx_event_info *tx_event, int ret_status);

static int
setup_tx_action_frame_request(
    wlan_p2p_prot_t prot_handle, 
    u_int16_t       event_type, 
    tx_event_info   *tx_fr_info);

static void
p2p_disc_scan_abort(wlan_p2p_prot_t prot_handle);

static void
proc_event_tx_frame(
    wlan_p2p_prot_t             prot_handle, 
    u_int16_t                   event_type, 
    tx_event_info               *tx_fr_info,
    ieee80211_connection_state  *newstate);

static int
p2p_disc_find_peer(void *ctx, wlan_scan_entry_t entry);

static int
p2p_disc_find_peer_in_grp_owner(void *ctx, wlan_scan_entry_t entry);

static int
p2p_prot_add_probe_req_ie(
    wlan_p2p_prot_t prot_handle, 
    wlan_p2p_prot_disc_scan_param *disc_scan_param,
    u_int8_t *frm, int buf_len,
    int *ret_ie_len);

static void
p2p_prot_start_disc_scan(wlan_p2p_prot_t prot_handle, ieee80211_connection_state *newstate);

static void
complete_disc_scan_req_event(wlan_p2p_prot_t prot_handle, 
                             disc_scan_event_info *disc_scan_info, 
                             int status, ieee80211_connection_state *newstate);

static void
clear_desired_peer(wlan_p2p_prot_t prot_handle);

static void
set_device_filters(wlan_p2p_prot_t prot_handle);

static void
reselect_op_chan(struct p2p_channels *chan_list, u8 *op_reg_class, u8 *op_chan);

/******************* Start of Timer support functions ********************/

struct timer_info {
    const ieee80211_connection_event    event;
    const char                          *name;
};

#define TIMER_INFO_ENTRY(_event) {P2P_DISC_EVENT_TMR_##_event, #_event}

/* List of timer event to post when timer triggers. */
const struct timer_info     disc_timer[MAX_NUM_P2P_DISC_TIMER] = {
    TIMER_INFO_ENTRY(DISC_LISTEN),      /* Timer for next Discovery Listen. Not specific in any state. */
    TIMER_INFO_ENTRY(SET_CHAN_SWITCH),  /* Timer for channel switch timeout during state PRE_OnChan or PRE_LISTEN */
    TIMER_INFO_ENTRY(TX_RETRY),         /* Timer to retry the Tx Action frame during state OnChan or Listen */
    TIMER_INFO_ENTRY(RETRY_SET_CHAN),   /* Timer to retry the Set Channel. Only used in PRE_ONCHAN or PRE_LISTEN state */
    TIMER_INFO_ENTRY(RETRY_SET_SCAN),   /* Timer to retry the set scan request (could be set chan too) Only used in DISCSCAN state. */
    TIMER_INFO_ENTRY(DISC_SCAN_EXPIRE), /* Timer to timeout the device discovery scan request. Used in all states */
    TIMER_INFO_ENTRY(TX_REQ_EXPIRE),    /* Timer to timeout the Transmit Action Frame request. Used in all states */
};

static const char *
timer_name(P2P_DISC_TIMER_TYPE timer_type)
{
    return(disc_timer[timer_type].name);
}

/* Timer callback for OS Timer. It is a single function for all timers used in this state machine */
static OS_TIMER_FUNC(p2p_disc_timer_cb)
{
    struct ieee80211com *ic;
    p2p_disc_timer_info *timer_info;
    wlan_p2p_prot_t     prot_handle;

    OS_GET_TIMER_ARG(timer_info, p2p_disc_timer_info *);

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
p2p_disc_timer_init(wlan_p2p_prot_t prot_handle)
{
    int     i;

    for (i = 0; i < MAX_NUM_P2P_DISC_TIMER; i++) {
        p2p_disc_timer_info     *timer;

        timer = &(prot_handle->sm_timer[i]);
        timer->prot_handle = prot_handle;
        timer->type = i;
        timer->event = disc_timer[i].event;

        OS_INIT_TIMER(prot_handle->os_handle, &(timer->timer_handle), p2p_disc_timer_cb, (void *)timer, QDF_TIMER_TYPE_WAKE_APPS);
    }
}

/*
 * Cleanup of all timers used by this state machine. This function is called when this module unloads.
 */
static void
p2p_disc_timer_cleanup(wlan_p2p_prot_t prot_handle)
{
    int     i;

    for (i = 0; i < MAX_NUM_P2P_DISC_TIMER; i++) {
        p2p_disc_timer_info     *timer;

        timer = &(prot_handle->sm_timer[i]);

        OS_CANCEL_TIMER(&(timer->timer_handle));

        OS_FREE_TIMER(&(timer->timer_handle));
    }
}

/*
 * Support timer routine to stop a P2P Discovery Timer.
 * Note that this timer may not have started.
 */
static void
p2p_disc_timer_stop(wlan_p2p_prot_t prot_handle, P2P_DISC_TIMER_TYPE timer_type)
{
    p2p_disc_timer_info     *timer;

    ASSERT(timer_type < MAX_NUM_P2P_DISC_TIMER);
    timer = &(prot_handle->sm_timer[timer_type]);

    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
        "%s: OS Timer %s(%d) stopped.\n",
        __func__, timer_name(timer->type), timer->type);

    OS_CANCEL_TIMER(&(timer->timer_handle));
}

/*
 * Routine to cancel all timers.
 */
static void
p2p_disc_timer_stop_all(wlan_p2p_prot_t prot_handle)
{
    int     i;

    for (i = 0; i < MAX_NUM_P2P_DISC_TIMER; i++) {
        p2p_disc_timer_info     *timer;

        timer = &(prot_handle->sm_timer[i]);
        p2p_disc_timer_stop(prot_handle, timer->type);
    }
}

/*
 * Support timer routine to start a P2P Discovery Timer.
 * Note that the timeout duration, duration, is in terms of milliseconds.
 */
static void
p2p_disc_timer_start(wlan_p2p_prot_t prot_handle, P2P_DISC_TIMER_TYPE timer_type, u_int32_t duration)
{
    p2p_disc_timer_info     *timer;

    ASSERT(timer_type < MAX_NUM_P2P_DISC_TIMER);

    timer = &(prot_handle->sm_timer[timer_type]);

    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
        "%s: OS Timer %s(%d) started. Duration=%d millisec\n",
        __func__, timer_name(timer->type), timer->type, duration);

    OS_SET_TIMER(&(timer->timer_handle), duration);
}
/******************* End of Timer support functions ********************/

/*
 * ****************************** Support Functions ******************************
 */

/* Stub Entry function for State Entry and Exit functions */
#define STATE_ENTRY_FN(_state)                                                      \
static void p2p_disc_state_##_state##_entry(void *ctx)                              \
{                                                                                   \
    wlan_p2p_prot_t         prot_handle = (wlan_p2p_prot_t)ctx;                     \
    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_PROT,   \
        "%s: called. ctx=0x%x.\n", __func__, ctx);                                  \
}                                                                                   \

/* Stub Exit function for State */
#define STATE_EXIT_FN(_state)                                                       \
static void p2p_disc_state_##_state##_exit(void *ctx)                               \
{                                                                                   \
    wlan_p2p_prot_t         prot_handle = (wlan_p2p_prot_t)ctx;                     \
    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_PROT,   \
        "%s: called. ctx=0x%x.\n", __func__, ctx);                                  \
}                                                                                   \

/* Macros to help to print the mac address */
#define MAC_ADDR_FMT        "%02x:%02x:%02x:%02x:%02x:%02x"
#define SPLIT_MAC_ADDR(addr)                    \
    ((addr)[0])&0x0FF,((addr)[1])&0x0FF,        \
    ((addr)[2])&0x0FF,((addr)[3])&0x0FF,        \
    ((addr)[4])&0x0FF,((addr)[5])&0x0FF         \

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
 * Check that the scan event has the same ID as my current request.
 * Return true if the ID matched. false if unmatched.
 */
static inline bool
check_scan_event_id_matched(wlan_p2p_prot_t prot_handle, ieee80211_scan_event *event)
{
    if (prot_handle->curr_scan.request_id == event->scan_id) {
        return true;
    }
    return false;
}

/*
 * Routine to check if we are able to transmit a frame now in this state.
 * This routine should be called from LISTEN or ONCHAN states.
 */
static bool
p2p_prot_can_tx_action_frame(wlan_p2p_prot_t prot_handle, tx_event_info *tx_fr_info)
{
    u_int32_t   time_elapsed, time_remaining;

    /* make sure that we are already on channel. Only true for LISTEN and ONCHAN states */
    ASSERT(prot_handle->curr_scan.set_chan_started == true);

    if (prot_handle->curr_tx_info) {
        /* Cannot accept new transmit of action frame */
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
            "%s: Unable to accept new tx since prot_handle->curr_tx_info (0x%x) is not NULL.\n", 
            __func__, prot_handle->curr_tx_info);
        return false;
    }
    /* make sure we are not in the middle of transmitting a frame */
    ASSERT(prot_handle->curr_tx_info == NULL);

    if (tx_fr_info->tx_freq != prot_handle->curr_scan.set_chan_freq) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
            "%s: Note: on_chan freq (%d) != tx freq (%d). Need to reschedule ONCHAN\n", 
            __func__, prot_handle->curr_scan.set_chan_freq, tx_fr_info->tx_freq);

        return false;
    }

    /* Check if there is enough time to transmit this frame now */
    time_elapsed = CONVERT_SYSTEM_TIME_TO_MS(OS_GET_TIMESTAMP() - prot_handle->curr_scan.set_chan_startime);

    if (prot_handle->curr_scan.set_chan_duration < time_elapsed) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                             "%s: warning: scan_duration (%d) < time_elapsed (%d).\n",
                             __func__, prot_handle->curr_scan.set_chan_duration, time_elapsed);
        time_remaining = 0;
    }
    else {
        time_remaining = prot_handle->curr_scan.set_chan_duration - time_elapsed;
    }

    /* Optimization TODO: Need to check if there is sufficient time based on the frame type.
     * For example, a GO_NEG_REQ will need enough time for the 3-way handshake while
     * a GO_NEG_CONF will only need a shorter time to transmit and get back an ACK. */
    if (time_remaining < P2P_PROT_MIN_TIME_TO_TX_ACT_FR) {
        /* there is insufficient time to send this frame. */
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
             "%s: warning: insufficient time to tx frame, schedule a new ON_CHAN, time_remaining (%d).\n",
             __func__, time_remaining);

        return false;
    }
    return true;
}   /* end of p2p_prot_can_tx_action_frame() */

/* 
 * This callback function is called when the action frame is transmitted or completed with errors.
 * The function type is wlan_action_frame_complete_handler.
 */
static void
p2p_prot_action_frame_complete_handler(wlan_if_t vaphandle, wbuf_t wbuf, void *arg, 
                                       u_int8_t *dst_addr, u_int8_t *src_addr, u_int8_t *bssid, 
                                       ieee80211_xmit_status *ts)
{
    sm_event_info                   new_event;
    tx_event_info                   *tx_fr_info;
    wlan_p2p_prot_t                 prot_handle;

    tx_fr_info = (tx_event_info *)arg;
    prot_handle = tx_fr_info->orig_req_param.prot_handle;

    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
        "%s: tx frame completed. status ts_flags=0x%x\n", 
        __func__, ts->ts_flags);

    /* post an error from transmit */
    OS_MEMZERO(&new_event, sizeof(sm_event_info));
    new_event.event_type = P2P_DISC_EVENT_TX_COMPLETE;
    if (ts->ts_flags == 0) {
        /* Send successful */
        new_event.tx_complete.status = EOK;
        prot_handle->num_tx_frame_retries = 0; /* Must sure this failure is not retried */
    }
    else {
        new_event.tx_complete.status = EIO;
    }
    new_event.tx_complete.orig_tx_param = tx_fr_info;

    ieee80211_sm_dispatch(prot_handle->hsm_handle, P2P_DISC_EVENT_TX_COMPLETE,
                          sizeof(sm_event_info), &new_event);
}

/*
 * Routine to resend the action frame that was previously prepared.
 */
static void
p2p_prot_send_action_frame(wlan_p2p_prot_t prot_handle, tx_event_info *tx_fr_info)
{
    int             retval;
    const u_int8_t  *dst_addr = NULL;
    const u_int8_t  *bssid = NULL;

    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
        "%s: send tx frame. num_tx_frame_retries=%d.\n", 
        __func__, prot_handle->num_tx_frame_retries);

    switch (tx_fr_info->orig_req_param.frtype) {
    case IEEE80211_P2P_PROT_FTYPE_GO_NEG_REQ:
        dst_addr = tx_fr_info->orig_req_param.go_neg_req.peer_addr;
        bssid = tx_fr_info->orig_req_param.go_neg_req.peer_addr;
        break;
    case IEEE80211_P2P_PROT_FTYPE_GO_NEG_RESP:
        dst_addr = tx_fr_info->orig_req_param.go_neg_resp.peer_addr;
        bssid = prot_handle->vap->iv_myaddr;
        break;
    case IEEE80211_P2P_PROT_FTYPE_GO_NEG_CONF:
        dst_addr = tx_fr_info->orig_req_param.go_neg_conf.peer_addr;
        bssid = tx_fr_info->orig_req_param.go_neg_conf.peer_addr;
        break;
    case IEEE80211_P2P_PROT_FTYPE_PROV_DISC_REQ:
        dst_addr = tx_fr_info->orig_req_param.prov_disc_req.peer_addr;
        bssid = tx_fr_info->orig_req_param.prov_disc_req.peer_addr;
        break;
    case IEEE80211_P2P_PROT_FTYPE_PROV_DISC_RESP:
        dst_addr = tx_fr_info->orig_req_param.prov_disc_resp.rx_addr;
        bssid = prot_handle->vap->iv_myaddr;
        break;
    case IEEE80211_P2P_PROT_FTYPE_INVIT_REQ:
        dst_addr = tx_fr_info->orig_req_param.invit_req.peer_addr;
        bssid = tx_fr_info->orig_req_param.invit_req.peer_addr;
        break;
    case IEEE80211_P2P_PROT_FTYPE_INVIT_RESP:
        dst_addr = tx_fr_info->orig_req_param.invit_resp.rx_addr;
        bssid = prot_handle->vap->iv_myaddr;
        break;
    default:
        ASSERT(false);
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
            "%s: Error: Unsupported frame type=%d\n", 
            __func__, tx_fr_info->orig_req_param.frtype);
        return;
    }

    retval = wlan_vap_send_action_frame(
                prot_handle->vap, tx_fr_info->tx_freq,
                p2p_prot_action_frame_complete_handler, tx_fr_info,
                dst_addr,                               /* dest addr */
                prot_handle->vap->iv_myaddr,            /* source addr */
                bssid,                                  /* BSSID addr */
                tx_fr_info->frame_buf, tx_fr_info->frame_len);
    if (retval != EOK)
    {
        /* Error in sending action frame */
        sm_event_info       new_event;

        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
            "%s: Err: wlan_vap_send_action_frame ret=%d\n", 
            __func__, retval);

        /* post an error from transmit */
        OS_MEMZERO(&new_event, sizeof(sm_event_info));
        new_event.event_type = P2P_DISC_EVENT_TX_COMPLETE;
        new_event.tx_complete.status = -EIO;
        new_event.tx_complete.orig_tx_param = tx_fr_info;

        ieee80211_sm_dispatch(prot_handle->hsm_handle, P2P_DISC_EVENT_TX_COMPLETE,
                              sizeof(sm_event_info), &new_event);
    }
}

/*
 * This helper function prepares the event_info structure to do a discovery scan.
 * It returns the allocated and filled in event_info structure.
 */
static disc_scan_event_info *
prepare_disc_scan_event_info(wlan_p2p_prot_t prot_handle, 
                             wlan_p2p_prot_disc_scan_param *disc_scan_param)
{
    int                     param_size, add_ie_size;
    disc_scan_event_info    *req_param;
    u_int8_t                *add_ie_buf;
    int                     curr_dest_buf_offset;
    int                     legacy_ssid_list_len, p2p_ie_len;
    int                     dev_filter_list_len;
    bool                    add_wildcard_p2p_ssid;
    int                     retval;

    /* Assumed that the P2P IE is single  */
    p2p_ie_len = MAX_SINGLE_IE_LEN;

    /* Alloc the scatch buffer for this tx request (includes the frame buffer and additional IE) */
    param_size = sizeof(disc_scan_event_info);
    param_size += disc_scan_param->add_ie_buf_len;  /* add the OS's IE's for probe requests */
    param_size += p2p_ie_len;                       /* add the P2P IE's for probe requests */
    dev_filter_list_len = disc_scan_param->dev_filter_list_num * sizeof(wlan_p2p_prot_disc_dev_filter);
    param_size += dev_filter_list_len;      /* add memory for device filters */

    param_size += sizeof(ieee80211_ssid);  /* add space for one P2P wildcard SSID or group ssid */

    req_param = (disc_scan_event_info *)OS_MALLOC(prot_handle->os_handle, param_size, 0);
    if (req_param == NULL) {
        IEEE80211_DPRINTF(prot_handle->vap, IEEE80211_MSG_P2P_PROT, 
                          "%s: failed to alloc param buf, size=%d\n",
                          __func__, param_size);
        return NULL;
    }
    req_param->alloc_size = param_size;
    OS_MEMZERO(req_param, param_size);

    /* Make a copy of the original parameters */
    OS_MEMCPY(&req_param->orig_req_param, disc_scan_param, sizeof(wlan_p2p_prot_disc_scan_param));

    //STNG TODO: these IE's replaces the OID_DOT11_WFD_ADDITIONAL_IE if an IE is specified.

    /* Copy the probe req's IE to the end of disc_scan_event_info buffer */
    {
        // Copy the IE buffer.
        u_int8_t                *dest_buf;

        curr_dest_buf_offset = sizeof(disc_scan_event_info);
        dest_buf = &(((u_int8_t *)req_param)[curr_dest_buf_offset]);

        if (disc_scan_param->add_ie_buf_len > 0) {
            OS_MEMCPY(dest_buf, disc_scan_param->add_ie_buf, disc_scan_param->add_ie_buf_len);
            /* Note that we will replace the existing IE with this new one during the discovery */
            req_param->remove_add_ie = true;
        }
        else {
            /* Do not need to replace the existing IE during the discovery */
            req_param->remove_add_ie = false;
        }

        req_param->in_ie_offset = curr_dest_buf_offset;
        req_param->in_ie_buf_len = disc_scan_param->add_ie_buf_len;

        curr_dest_buf_offset += disc_scan_param->add_ie_buf_len;

        //STNG TODO: consider to move the IE filling inside the statemachine so that the prot_handle's fields are synchronized.
        /* Add the P2P IE */
        /* Note: the P2P IE should be the last one after the WSC (also call WPS) IE */
        dest_buf = &(((u_int8_t *)req_param)[curr_dest_buf_offset]);
        retval = p2p_prot_add_probe_req_ie(prot_handle, disc_scan_param, dest_buf,
                                     p2p_ie_len, &p2p_ie_len);
        req_param->in_ie_buf_len += p2p_ie_len;
        curr_dest_buf_offset += p2p_ie_len;
    }

    /* Copy the device filter list */
    if (disc_scan_param->dev_filter_list_num > 0)
    {
        int         i;
        u_int8_t    *dest_buf;
        u_int8_t    all_bitmask;

        req_param->in_stop_when_discovered = true;

        /* Process the Device Filter List */
        all_bitmask = 0;
        for (i = 0; i < disc_scan_param->dev_filter_list_num; i++) {
            ieee80211_ssid  *grp_ssid;
            u_int8_t        curr_bitmask;

            /* collect all the bitmask */
            curr_bitmask = disc_scan_param->dev_filter_list[i].bitmask;
            all_bitmask |= curr_bitmask;

            /* check if group SSID is a P2P wildcard SSID */
            grp_ssid = &(disc_scan_param->dev_filter_list[i].grp_ssid);
            if ((grp_ssid->len == IEEE80211_P2P_WILDCARD_SSID_LEN) &&
                (OS_MEMCMP(grp_ssid->ssid, IEEE80211_P2P_WILDCARD_SSID, IEEE80211_P2P_WILDCARD_SSID_LEN) == 0))
            {
                disc_scan_param->dev_filter_list[i].grp_ssid_is_p2p_wildcard = true;

                if (curr_bitmask == WLAN_P2P_PROT_DISC_DEV_FILTER_BITMASK_GO)
                {
                    /* When the filter is to find all GO for this device ID, then this scan
                     * will not stop until all channels are found. Therefore, no need for a
                     * device filter(s). */
                    req_param->in_stop_when_discovered = false;
                    break;
                }
            }
        }
        if ((all_bitmask == WLAN_P2P_PROT_DISC_DEV_FILTER_BITMASK_DEVICE) &&
            (req_param->in_stop_when_discovered))
        {
            /* Only scanning for P2P device on social channels */
            req_param->in_social_chans_only = true;
        }

        dest_buf = &(((u_int8_t *)req_param)[curr_dest_buf_offset]);
        OS_MEMCPY(dest_buf, disc_scan_param->dev_filter_list, dev_filter_list_len);
        req_param->in_dev_filter_list_num = disc_scan_param->dev_filter_list_num;
        req_param->in_dev_filter_list_offset = curr_dest_buf_offset;
        curr_dest_buf_offset += dev_filter_list_len;

        if ((disc_scan_param->dev_filter_list_num == 1) &&
            (disc_scan_param->dev_filter_list[0].bitmask == WLAN_P2P_PROT_DISC_DEV_FILTER_BITMASK_GO))
        {
            /* Only scanning for GO. Add the group SSID */
            ieee80211_ssid      *src_ssid_buf;
            ieee80211_ssid      *dst_ssid_buf;

            add_wildcard_p2p_ssid = false;

            /* Copy the GO's SSID */
            src_ssid_buf = &(disc_scan_param->dev_filter_list[0].grp_ssid);
            req_param->in_p2p_wldcd_ssid_offset = curr_dest_buf_offset;
            dst_ssid_buf = (ieee80211_ssid *)&(((u_int8_t *)req_param)[curr_dest_buf_offset]);
            OS_MEMCPY(dst_ssid_buf, src_ssid_buf, sizeof(ieee80211_ssid));
            curr_dest_buf_offset += sizeof(ieee80211_ssid);
        }
        else {
            /* Add a wildcard P2P ssid */
            add_wildcard_p2p_ssid = true;
        }
    }
    else {
        /* Add a wildcard P2P ssid */
        add_wildcard_p2p_ssid = true;
        req_param->in_dev_filter_list_num = 0;
        req_param->in_dev_filter_list_offset = 0;
    }

    if (add_wildcard_p2p_ssid) {
        ieee80211_ssid      *ssid_buf;

        /* Leave space for one SSID that is reserved for the P2P wildcard SSID */
        /* Make sure that this SSID is just before the legacy SSID list */
        req_param->in_p2p_wldcd_ssid_offset = curr_dest_buf_offset;

        ssid_buf = (ieee80211_ssid *)&(((u_int8_t *)req_param)[curr_dest_buf_offset]);
        ssid_buf->len = IEEE80211_P2P_WILDCARD_SSID_LEN;
        OS_MEMCPY(ssid_buf->ssid, IEEE80211_P2P_WILDCARD_SSID, IEEE80211_P2P_WILDCARD_SSID_LEN);
        curr_dest_buf_offset += sizeof(ieee80211_ssid);
    }

    req_param->time_start = OS_GET_TIMESTAMP();
    req_param->curr_timeout = req_param->orig_req_param.timeout;

    return(req_param);
}

/*
 * Routine to create a discovery scan request for Mini-find.
 */
static disc_scan_event_info *
create_minifind_disc_scan_request(wlan_p2p_prot_t prot_handle, ieee80211_connection_event tx_event_type,
                                  tx_event_info *tx_fr_info)
{
    wlan_p2p_prot_disc_scan_param   *disc_scan_param;
    int                             param_size, add_ie_size;
    disc_scan_event_info            *req_param;
    u_int8_t                        *add_ie_buf;
    int                             curr_dest_buf_offset;
    int                             dev_id_list_len, legacy_ssid_list_len, p2p_ie_len;
    size_t                          buf_size;

    buf_size = sizeof(wlan_p2p_prot_disc_scan_param) + sizeof(wlan_p2p_prot_disc_dev_filter);
    disc_scan_param = (wlan_p2p_prot_disc_scan_param *) OS_MALLOC(prot_handle->os_handle,
                                                                  buf_size,
                                                                  0);
    if (disc_scan_param == NULL)
    {
        IEEE80211_DPRINTF(prot_handle->vap, IEEE80211_MSG_P2P_PROT, 
                          "%s: failed to alloc param buf, size=%d\n",
                          __func__, buf_size);
        return NULL;
    }
    /* Prepare the original scan parameter structure*/
    OS_MEMZERO(disc_scan_param, buf_size);

    disc_scan_param->disc_type = wlan_p2p_prot_disc_type_auto;
    disc_scan_param->disc_scan_forced = true;
    disc_scan_param->scan_type = wlan_p2p_prot_disc_scan_type_auto;
    disc_scan_param->timeout = tx_fr_info->timeout;
    disc_scan_param->scan_legy_netwk = false;

    {
        wlan_p2p_prot_disc_dev_filter   *dev_filter;
        u_int8_t                        *tmp_buf = (u_int8_t *)disc_scan_param;

        /* Setup a single device filter for this P2P device */
        disc_scan_param->dev_filter_list_num = 1;
        disc_scan_param->dev_filter_list = (wlan_p2p_prot_disc_dev_filter *)
                                            &(tmp_buf[sizeof(wlan_p2p_prot_disc_scan_param)]);

        dev_filter = &disc_scan_param->dev_filter_list[0];

#if DBG
        dev_filter->dbg_mem_marker = WLAN_P2P_PROT_DBG_MEM_MARKER;
#endif

        /* We will find either the P2P device or the GO */
        dev_filter->bitmask = WLAN_P2P_PROT_DISC_DEV_FILTER_BITMASK_DEVICE | 
                                WLAN_P2P_PROT_DISC_DEV_FILTER_BITMASK_GO;
        IEEE80211_ADDR_COPY(dev_filter->device_id, tx_fr_info->peer_addr);
        if (tx_fr_info->have_group_id) {
            dev_filter->grp_ssid.len = tx_fr_info->group_id.ssid.len;
            OS_MEMCPY(dev_filter->grp_ssid.ssid, tx_fr_info->group_id.ssid.ssid, tx_fr_info->group_id.ssid.len);
        }
        else {
            /* Use the P2P wildcard SSID */
            dev_filter->grp_ssid.len = IEEE80211_P2P_WILDCARD_SSID_LEN;
            OS_MEMCPY(dev_filter->grp_ssid.ssid, IEEE80211_P2P_WILDCARD_SSID, IEEE80211_P2P_WILDCARD_SSID_LEN);
        }
    }

    req_param = prepare_disc_scan_event_info(prot_handle, disc_scan_param);
    if (req_param == NULL) {
        IEEE80211_DPRINTF(prot_handle->vap, IEEE80211_MSG_P2P_PROT, 
                          "%s: failed to prepare event info structure for discovery scan.\n",
                          __func__);
        return NULL;
    }

    req_param->is_minifind = true;
    req_param->minifind.tx_fr_request = tx_fr_info;
    req_param->minifind.tx_event_type  = tx_event_type;

    return(req_param);
}

/* structure to store the parameters when calling p2p_disc_find_peer() */
typedef struct find_peer_param {
    wlan_p2p_prot_t         prot_handle;
    const u_int8_t          *mac_addr;
    wlan_scan_entry_t       match_entry;
    bool                    only_GO;
    ieee80211_ssid          *ssid_info;
    ieee80211_scan_p2p_info *p2p_info;
    u_int8_t                tmp_buf[IEEE80211_MAX_MPDU_LEN];
};

/*
 * This function is called to check if the p2p operational channel is restricted
 * due to other reasons like the Infra-Station VAP is active and multiple-channel
 * support is unavailable.
 *
 * Returns true if the op channel is restricted and must be set to the returned
 * frequency, op_chan_freq.
 * Returns false if the op channel is not restricted. But if there is a preferred op channel,
 * then have_preferred_op_chan is set to true and op_chan_freq contains the preferred op chan.
 * The op_chan_freq is checked with the given channel list, chan_list. If the operational channel
 * is not found inside the channel list, then op_chan_freq is set to 0.
 * When the op_chan_freq is returned, then the corresponding p2p channel fields,
 * op_chan_reg_class and op_chan_channel, are also returned.
 */
static bool
p2p_prot_is_op_chan_restricted(
    wlan_p2p_prot_t     prot_handle,
    struct p2p_channels *chan_list,
    bool                *have_preferred_op_chan,
    u_int32_t           *op_chan_freq,
    u8                  *op_chan_reg_class, 
    u8                  *op_chan_channel
    )
{
#define BSS_FREQ_MAX_NUM    8
    bool        op_chan_restricted;
    int         retval;
    u_int32_t   bss_freq[BSS_FREQ_MAX_NUM];
    int         num_bss_freq;
    bool        have_multi_chan;

    *have_preferred_op_chan = false;
    *op_chan_freq = 0;
    *op_chan_reg_class = *op_chan_channel = 0;

    /*
     * Have to consider other VAPs who are connected and cannot move to another channel.
     * Specifically, when the infra-station or/and p2p client is associated and 
     * off-channel support is not enabled, then Operational channel should be equal to 
     * the station/client channel.
     */
    num_bss_freq = BSS_FREQ_MAX_NUM;
    retval = wlan_get_bss_chan_list(wlan_vap_get_devhandle(prot_handle->vap), bss_freq, &num_bss_freq);
    if (retval != EOK) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                             "%s: Error: wlan_get_bss_chan_list rets error (0x%x).\n",
                             __func__, retval);
        /* No good way to handle this error. */
        return false;
    }

    if (num_bss_freq == 0) {
        /* No other active BSS. No channel restrictions */
        op_chan_restricted = false;

        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                             "%s: No Op chan restriction since no other BSSes are active.\n",
                             __func__);
        return op_chan_restricted;
    }

    have_multi_chan = (wlan_get_device_param(wlan_vap_get_devhandle(prot_handle->vap), IEEE80211_DEVICE_MULTI_CHANNEL) != 0);
    if (have_multi_chan)
    {
        /*
         * Have multiple channel support.
         * We have choose a op channel frequency that is within the given channel list
         */
        int         i;
        u_int32_t   matched_op_chan_freq = 0;

        for (i = 0; i < num_bss_freq; i++)
        {
            int     retval;

            retval = wlan_p2p_supported_chan_on_freq((const char *)prot_handle->country,
                                                     chan_list,
                                                     bss_freq[i],
                                                     op_chan_reg_class, op_chan_channel);
            if (retval == 1) {
                /* This BSS channel is within our channel list. choose this one */
                matched_op_chan_freq = bss_freq[i];
                break;
            }
        }

        /* Note: assumed that we can only do 2 unrestricted channels when multiple channel is enabled. */
        if (num_bss_freq == 1) {
            /* No channel restriction but have a preferred op channel */
            op_chan_restricted = false;
            *have_preferred_op_chan = true;
            *op_chan_freq = matched_op_chan_freq;
            ASSERT((*op_chan_reg_class != 0) && (*op_chan_channel != 0));
        }
        else if (matched_op_chan_freq == 0) {
            /* No channel found within the channel list */
            op_chan_restricted = true;
            *op_chan_freq = 0;
            *op_chan_reg_class = *op_chan_channel = 0;
        }
        else {
            /* Else Found a match and (num_bss_freq>1) */
            ASSERT(num_bss_freq > 1);
            op_chan_restricted = true;
            *op_chan_freq = matched_op_chan_freq;
            ASSERT((*op_chan_reg_class != 0) && (*op_chan_channel != 0));
        }

        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                             "%s: Have multi-chan supp. restricted=%d. op chan freq=%d, have_preferred=%d, num_bss_freq=%d\n",
                             __func__, op_chan_restricted, *op_chan_freq, *have_preferred_op_chan, num_bss_freq);

        return op_chan_restricted;
    }
    else {
        /* Else no multiple channel support. Our p2p must have the same BSS channel as the
         * existing one */
        int     retval;

        ASSERT(num_bss_freq > 0);

        op_chan_restricted = true;

        retval = wlan_p2p_supported_chan_on_freq((const char *)prot_handle->country,
                                                 chan_list,
                                                 bss_freq[0],
                                                 op_chan_reg_class, op_chan_channel);
        if (retval == 1) {
            /* This BSS channel is within our channel list. choose this one */
            *op_chan_freq = bss_freq[0];
            ASSERT((*op_chan_reg_class != 0) && (*op_chan_channel != 0));
        }
        else {
            /* We don't have an valid operational channel */
            *op_chan_freq = 0;
            *op_chan_reg_class = *op_chan_channel = 0;
        }

        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                             "%s: No multi-chan supp. Proposed Op chan freq=%d. num_bss_freq=%d\n",
                             __func__, bss_freq[0], num_bss_freq);

        return op_chan_restricted;
    }

#undef BSS_FREQ_MAX_NUM
}

/*
 * This routine will check and set the channel related information during group negotiation for
 * the GO NEG Response or Confirmation frames.
 *
 * The return status of this frame may be changed if the channel restrictions do not permit
 * a successful group formation. When the returned status inside the tx_fr_info parameter is 
 * changed, the return value of status_changed is set to true.
 * The Operational channel, op_chan_reg_class and op_chan_channel, 
 * and channel list, ret_chan_list, are returned.
 */
void
p2p_prot_check_and_set_chan_info(
    wlan_p2p_prot_t     prot_handle, 
    tx_event_info       *tx_fr_info,
    bool                resp_frame_type,
    bool                *status_changed,
    struct p2p_channels **ret_chan_list,
    u8                  *op_chan_reg_class,
    u8                  *op_chan_channel
    )
{
    int     *ret_status;
    bool    sta_chan_restricted = false;
    u8      sta_chan_reg_class = 0, sta_chan_channel = 0;
    u8      go_op_chan_reg_class = 0, go_op_chan_channel = 0;
    bool    we_are_GO = false;
    bool    prev_frame_parsed_ie_ok;
    bool    have_preferred_op_chan = false;
    u8      pref_chan_reg_class = 0, pref_chan_channel = 0;

    if (resp_frame_type) {
        /* Processing the GO NEG Response frame */
        wlan_p2p_prot_tx_go_neg_resp_param  *resp_param;

        ASSERT(tx_fr_info->previous_frtype == IEEE80211_P2P_PROT_FTYPE_GO_NEG_REQ);

        resp_param = &tx_fr_info->orig_req_param.go_neg_resp;
        ret_status = &resp_param->status;
        we_are_GO = resp_param->use_grp_id;
        prev_frame_parsed_ie_ok = resp_param->request_fr_parsed_ie;
    }
    else {
        /* Processing the GO NEG Confirmation frame */
        wlan_p2p_prot_tx_go_neg_conf_param  *conf_param;

        ASSERT(tx_fr_info->previous_frtype == IEEE80211_P2P_PROT_FTYPE_GO_NEG_RESP);

        conf_param = &tx_fr_info->orig_req_param.go_neg_conf;
        ret_status = &conf_param->status;
        we_are_GO = conf_param->use_grp_id;
        prev_frame_parsed_ie_ok = conf_param->response_fr_parsed_ie;

        if (!we_are_GO) {
            /* If we are client, then the GO must have op channel in its response frame */
            if ((!conf_param->resp_have_op_chan) && prev_frame_parsed_ie_ok) {
                /* Error: */
                IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                                     "%s: Error: set parsing error. GON Resp frame for GO must have Op Chan.\n",
                                     __func__);
                prev_frame_parsed_ie_ok = false;
            }
            else {
                /* In rx_resp_op_chan[], the first 3 octets are country code, followed
                 * by regulatory class and then the IEEE channel number. */
                go_op_chan_reg_class = conf_param->rx_resp_op_chan[3];
                go_op_chan_channel = conf_param->rx_resp_op_chan[4];
            }
        }
    }
    *status_changed = false;
    *ret_chan_list = NULL;
    *op_chan_reg_class = *op_chan_channel = 0;

    /* Check if we need to adjust the status of this Grp Negotiation to failure */
    do {
        u_int32_t   op_chan_freq = 0;
        u8          tmp_reg_class, tmp_chan_num;

        if (*ret_status != P2P_SC_SUCCESS) {
            /* status is already a failure anyway */
            break;
        }

        if (!prev_frame_parsed_ie_ok) {
            /* Some error in parsing the received Grp Neg Request/Response frame */
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                                 "%s: Change status from SUCCESS to FAIL_INVALID_PARAMS "
                                 "due to bad IE in prev. frame.\n",
                                 __func__);
            *ret_status = P2P_SC_FAIL_INVALID_PARAMS;
            *status_changed = true;
            break;
        }

        if ((!tx_fr_info->rx_valid_chan_list) ||
            (tx_fr_info->intersected_chan_list.reg_classes == 0))
        {
            /* No common channels */
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                                 "%s: Change status from SUCCESS to FAIL_NO_COMMON_CHANNELS."
                                 " rx_valid_chan_list=%d, no reg_classes=%d\n",
                                 __func__, tx_fr_info->rx_valid_chan_list,
                                 tx_fr_info->intersected_chan_list.reg_classes);
            *ret_status = P2P_SC_FAIL_NO_COMMON_CHANNELS;
            *status_changed = true;
            break;
        }

        /*
         * Consider the channel restrictions due to connections on other VAP who
         * are unable move to another channel.
         * Specifically, when the infra-station or/and p2p client is associated and 
         * off-channel support is not enabled, then Operational channel should be exactly 
         * the station/client channel.
         */
        if (p2p_prot_is_op_chan_restricted(prot_handle, &tx_fr_info->intersected_chan_list,
                                           &have_preferred_op_chan, &op_chan_freq,
                                           &tmp_reg_class, &tmp_chan_num))
        {
            /*
             * We need to restrict the Op Channel due to other VAPs.
             */
            int     retval;

            if (op_chan_freq == 0) {
                IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                                     "%s: restricted but no compatible freq, Change status from SUCCESS to FAIL_NO_COMMON_CHANNELS.\n",
                                     __func__);
                *ret_status = P2P_SC_FAIL_NO_COMMON_CHANNELS;
                *status_changed = true;
                break;
            }
            /* Else there is a restricted channel to use */

            ASSERT((tmp_reg_class != 0) && (tmp_chan_num != 0));
            sta_chan_reg_class = tmp_reg_class;
            sta_chan_channel = tmp_chan_num;
            sta_chan_restricted = true;

            /* The new intersected chan list is just this one channel */
            tx_fr_info->intersected_chan_list.reg_classes = 1;
            tx_fr_info->intersected_chan_list.reg_class[0].reg_class = sta_chan_reg_class;
            tx_fr_info->intersected_chan_list.reg_class[0].channels = 1;
            tx_fr_info->intersected_chan_list.reg_class[0].channel[0] = sta_chan_channel;

            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                                 "%s: changing op channel and chan list to regclass=%d chan=%d.\n",
                                 __func__, sta_chan_reg_class, sta_chan_channel);
        }
        else if (have_preferred_op_chan) {
            /* No op channel restriction but have a preferred channel */

            pref_chan_reg_class = tmp_reg_class;
            pref_chan_channel = tmp_chan_num;
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                                 "%s: Have pref op chan freq=%d and is in intersect chanlist. regclass=%d, chan=%d\n",
                                 __func__, op_chan_freq, pref_chan_reg_class, pref_chan_channel);
        }

    } while ( false );

    if (*ret_status == 0) {
        /* OS has succeeded this GON */
        if (we_are_GO) {
            /* Note that we have already check the channel list is OK and have common channels */

            /* The channel list to use the intersected channel list */
            *ret_chan_list = &tx_fr_info->intersected_chan_list;

            if (sta_chan_restricted) {
                /* We are restricted to the single channel that station uses */
                ASSERT(sta_chan_channel != 0);
                *op_chan_reg_class = sta_chan_reg_class;
                *op_chan_channel = sta_chan_channel;
            }
            else {
                /* need to chose an op channel within the intersected list */

                if (have_preferred_op_chan) {
                    /* We have a preferred op channel. Note: this chan is from intersected list. */
                    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                                         "%s: we are GO: Using pref op channel. regclass=%d chan=%d.\n",
                                         __func__, pref_chan_reg_class, pref_chan_channel);

                    *op_chan_reg_class = pref_chan_reg_class;
                    *op_chan_channel = pref_chan_channel;
                }
                else {
                    /* Use the default op channel */
                    int     retval;
                    retval = wlan_p2p_channels_includes(&tx_fr_info->intersected_chan_list,
                                                        prot_handle->op_chan.reg_class,
                                                        prot_handle->op_chan.ieee_num);
                    if (retval == 0) {
                        /* Our desired Op chan is not found in the intersected list */
                        u8      new_reg_class = 0, new_chan = 0;
    
                        reselect_op_chan(&tx_fr_info->intersected_chan_list, &new_reg_class, &new_chan);
                        ASSERT((new_reg_class != 0) && (new_chan != 0));
    
                        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                                             "%s: we are GO: selected op channel. regclass=%d chan=%d.\n",
                                             __func__, new_reg_class, new_chan);
    
                        *op_chan_reg_class = new_reg_class;
                        *op_chan_channel = new_chan;
                    }
                    else {
                        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                                             "%s: we are GO: op channel unchanged. regclass=%d chan=%d.\n",
                                             __func__, prot_handle->op_chan.reg_class, prot_handle->op_chan.ieee_num);
    
                        *op_chan_reg_class = prot_handle->op_chan.reg_class;
                        *op_chan_channel = prot_handle->op_chan.ieee_num;
                    }
                }
            }
        }
        else {
            /* We are the client. Channel list is what our hw can support */

            if (sta_chan_restricted) {
                /* We are restricted to the single channel that station uses */
                IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                                     "%s: we are client: op channel restrict to sta chan. regclass=%d chan=%d.\n",
                                     __func__, sta_chan_reg_class, sta_chan_channel);

                *op_chan_reg_class = sta_chan_reg_class;
                *op_chan_channel = sta_chan_channel;

                /* Send the intersected channel list */
                *ret_chan_list = &tx_fr_info->intersected_chan_list;
            }
            else {
                if (resp_frame_type) {
                    /* for GO_NEG_RESP frame only and use our hw list */
                    *ret_chan_list = &prot_handle->chanlist;

                    /* This is optional. But it shows our preferred Op Chan but
                     * must be within the chan list from resp frame */
                    if (have_preferred_op_chan) {
                        /* We have a preferred op channel. Note: this chan is from intersected list. */
                        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                                             "%s: we are client: Using pref op channel. regclass=%d chan=%d.\n",
                                             __func__, pref_chan_reg_class, pref_chan_channel);

                        *op_chan_reg_class = pref_chan_reg_class;
                        *op_chan_channel = pref_chan_channel;
                    }
                    else {
                        *op_chan_reg_class = 0;
                        *op_chan_channel = 0;
                    }
                }
                else {
                    /* for GO_NEG_CONF frame, use intersected chan list */
                    *ret_chan_list = &tx_fr_info->intersected_chan_list;

                    /* Use the operational channel is that received in the GO_NEG_RESP frame */
                    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                                         "%s: Use peer's op chan=%d %d\n",
                                         __func__, go_op_chan_reg_class, go_op_chan_channel);
                    ASSERT(go_op_chan_channel != 0);
                    *op_chan_reg_class = go_op_chan_reg_class;
                    *op_chan_channel = go_op_chan_channel;
                }
            }
        }
    }
    else {
        /* Failed Group Negotiation */
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                             "%s: OS has failed this GON. status=%d\n",
                             __func__, *ret_status);

        *ret_chan_list = &prot_handle->chanlist;

        *op_chan_reg_class = prot_handle->op_chan.reg_class;
        *op_chan_channel = prot_handle->op_chan.ieee_num;
    }
}

/*
 * To find the scan entry for this P2P Device that has the given mac address and SSID (if given).
 * When parameter only_GO is set, then the peer must be a P2P GO and the SSID must matched.
 * When parameter only_GO is not set, then the peer can be a p2p device or GO. But if it is a GO and
 * the grp_id_ssid is non-null, then the SSID must match.
 * If the grp_id_ssid is null, then the SSID check is skipped.
 * Return true if peer is found and scan_entry contains the scan entry. This scan entry
 * will be Ref-Counted and hence, the Ref-count needs to be release when done.
 * Return false if this peer is either not found or not matching SSID or suitable for P2P.
 */
static bool
find_p2p_peer(wlan_p2p_prot_t prot_handle, const u_int8_t *mac_addr,
              bool only_GO, ieee80211_ssid  *grp_id_ssid,
              wlan_scan_entry_t *scan_entry, ieee80211_scan_p2p_info *entry_p2p_info)
{
    struct find_peer_param  *param;

    /* Allocate the memory for param structure */
    param = (struct find_peer_param *)OS_MALLOC(prot_handle->os_handle, sizeof(struct find_peer_param), 0);
    if (param == NULL) {
        IEEE80211_DPRINTF(prot_handle->vap, IEEE80211_MSG_P2P_PROT, 
                          "%s: Error: failed to alloc param buf, size=%d\n",
                          __func__, sizeof(struct find_peer_param));
        return false;
    }

    // 
    // Examine each entry in the driver's BSS list. 
    // Function StaBuildBSSList populates the list to be returned to the OS.
    //
    OS_MEMZERO(param, sizeof(struct find_peer_param));
    param->prot_handle = prot_handle;
    param->mac_addr = mac_addr;
    if (grp_id_ssid != NULL) {
        /* Check if this SSID is a p2p wildcard SSID */
        if ((grp_id_ssid->len == IEEE80211_P2P_WILDCARD_SSID_LEN) &&
            (OS_MEMCMP(grp_id_ssid->ssid, IEEE80211_P2P_WILDCARD_SSID, IEEE80211_P2P_WILDCARD_SSID_LEN) == 0))
        {
            /* This is a p2p wildcard ssid. Assumed that ssid is NULL. */
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                 "%s: Find p2p device="MAC_ADDR_FMT". grp_id_ssid is a p2p wildcard ssid. only_GO=%d\n",
                 __func__, SPLIT_MAC_ADDR(mac_addr), only_GO);
        }
        else {
            param->ssid_info = grp_id_ssid;
        }
    }
    param->match_entry = NULL;
    param->p2p_info = entry_p2p_info;

    /* Look for the P2P Device but not into the GO's group info */
    wlan_scan_table_iterate(prot_handle->vap, p2p_disc_find_peer, param);

    if (param->match_entry == NULL) {
        /* Did not find the P2P device. Try looking into the GO's group info */

        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
             "%s: Did not find p2p device="MAC_ADDR_FMT". Try looking at the client table.\n",
             __func__, SPLIT_MAC_ADDR(mac_addr));

        wlan_scan_table_iterate(prot_handle->vap, p2p_disc_find_peer_in_grp_owner, param);
    }

    *scan_entry = param->match_entry;

    OS_FREE(param);

    return(*scan_entry != NULL);
}

/*
 * This routine is called after the successful completion of a minifind
 * and to start the transmission of the action frame.
 */
static void
post_minifind_start_tx_action(wlan_p2p_prot_t               prot_handle,
                              disc_scan_event_info          *disc_scan_info, 
                              wlan_scan_entry_t             peer, 
                              ieee80211_scan_p2p_info       *entry_p2p_info,
                              ieee80211_connection_state    *newstate)
{
    tx_event_info   *tx_request;
    u_int16_t       dest_freq;
    u_int8_t        *peer_macaddr;

    ASSERT(peer != NULL);

    tx_request = disc_scan_info->minifind.tx_fr_request;
    disc_scan_info->minifind.tx_fr_request = NULL;

    dest_freq = entry_p2p_info->listen_chan->ic_freq;
    tx_request->tx_freq = dest_freq;
    tx_request->need_mini_find = false;

    peer_macaddr = ieee80211_scan_entry_macaddr(peer);

    /* Store the peer MAC address as the Receiver Address */
    IEEE80211_ADDR_COPY(tx_request->rx_addr, peer_macaddr);

    /* Adjust the timeout by subtracting the amount of time spend during minifind */
    {
        u_int32_t   time_elapsed_msec;

        time_elapsed_msec = CONVERT_SYSTEM_TIME_TO_MS(OS_GET_TIMESTAMP() - tx_request->minifind_start);

        if (time_elapsed_msec >= tx_request->timeout) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                                 "%s: Error: too much time has elapsed=%d. Timeout=%d\n",
                                 __func__, time_elapsed_msec, tx_request->timeout);
            tx_request->timeout = 1;    /* Set to a small value so that the later code does not assert */
        }
        else {
            tx_request->timeout -= time_elapsed_msec;

            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                                 "%s: Time for mini-find=%d, Re-adjusting timeout=%d\n",
                                 __func__, time_elapsed_msec, tx_request->timeout);
        }
    }

    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                         "%s: After mini-find, start tx action frame for peer " MAC_ADDR_FMT ". freq=%d\n",
                         __func__, SPLIT_MAC_ADDR(peer_macaddr), dest_freq);

    /* We need to re-schedule a new ONCHAN */
    proc_event_tx_frame(prot_handle, disc_scan_info->minifind.tx_event_type, tx_request, newstate);
}

/* Complete the Discovery Scan event which was started due to a minifind. */
static void
complete_minifind_tx_fr_req_event(wlan_p2p_prot_t               prot_handle,
                                  disc_scan_event_info          *disc_scan_info,
                                  int                           status,
                                  ieee80211_connection_state    *newstate)
{
    tx_event_info           *old_tx_request;
    bool                    peer_is_found;
    tx_event_info           *tx_fr_request;
    ieee80211_scan_p2p_info entry_p2p_info;
    wlan_scan_entry_t       peer = NULL;
    u_int8_t                *peer_macaddr = NULL;

    /* This is a Discovery Scan from a minifind due to a Tx Action frame request. */
    /* First, check for the previous Minifind which includes a Tx Request */

    ASSERT(disc_scan_info->minifind.tx_fr_request != NULL);
    OS_MEMZERO(&entry_p2p_info, sizeof(ieee80211_scan_p2p_info));

    if (disc_scan_info->minifind.aborted) {
        /* Abort this tx request and do not retry */
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
             "%s: Discovery scan for mini-find aborted.\n", __func__);
        peer_is_found = false;
    }
    else {
        ieee80211_ssid  *grp_id_ssid;

        if (status != 0) {
            /* Error in discovery scan. But still continue to find the peer. */
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                 "%s: Discovery scan for mini-find failed (0x%x). Still try to find peer.\n",
                 __func__, status);
        }

        tx_fr_request = disc_scan_info->minifind.tx_fr_request;

        if (tx_fr_request->have_group_id) {
            grp_id_ssid = &(tx_fr_request->group_id.ssid);
        }
        else {
            grp_id_ssid = NULL;
        }
        peer_macaddr = tx_fr_request->peer_addr;

        /* Done with discovery scan. Try to find the peer in the scan table */
        peer_is_found = find_p2p_peer(prot_handle, peer_macaddr, false, grp_id_ssid, &peer, &entry_p2p_info);
    }

    if (!peer_is_found) {
        /* We do not know where the peer is. failed this tx action frame request. */
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                             "%s: Even after mini-find, cannot find peer " MAC_ADDR_FMT ". Failed this tx. num_outstanding_tx=%d\n",
                             __func__, SPLIT_MAC_ADDR(peer_macaddr),
                             prot_handle->num_outstanding_tx);

        ASSERT(peer == NULL);

        /* Complete this request with error */
        old_tx_request = disc_scan_info->minifind.tx_fr_request;
        ASSERT(old_tx_request->tx_req_id == 0);
        disc_scan_info->minifind.tx_fr_request = NULL;
        complete_tx_action_frame_event(prot_handle, old_tx_request, -EIO);
    }
    else {
        /* Start the transmission of this action frame. */
        post_minifind_start_tx_action(prot_handle, disc_scan_info, peer, &entry_p2p_info, newstate);

        /* No need for this scan entry */
        ieee80211_scan_entry_remove_reference(peer);

        return;
    }
}

/*
 * Complete the current (and only) device discovery scan request
 */
static void
st_disc_scan_complete_req(wlan_p2p_prot_t prot_handle, int status, ieee80211_connection_state *newstate)
{
    wlan_p2p_prot_event     *ind_event;
    disc_scan_event_info    *disc_scan_info;

    /* Stop the timer TIMER_DISC_SCAN_EXPIRE (may have already expired) */
    p2p_disc_timer_stop(prot_handle, TIMER_DISC_SCAN_EXPIRE);

    if (prot_handle->curr_disc_scan_info == NULL) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
            "%s: called but no current scan parameters.\n", __func__);
        return;
    }

    if (status != EOK) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
            "%s: called. Some error in status=0x%x\n", __func__, status);
    }
    else {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
            "%s: Discovery Scan complete. status=0x%x, arg=%pK.\n",
            __func__, status, prot_handle->event_arg);

    }

    disc_scan_info = prot_handle->curr_disc_scan_info;
    prot_handle->curr_disc_scan_info = NULL;
    prot_handle->disc_scan_active = false;

    complete_disc_scan_req_event(prot_handle, disc_scan_info, status, newstate);
}

/*
 * This routine is called when trying to send an action but the peer is
 * not yet found. Hence, we need to start a mini-find.
 */
static void
start_minifind(wlan_p2p_prot_t              prot_handle,
               ieee80211_connection_event   tx_event_type,
               tx_event_info                *tx_fr_info, 
               bool                         in_idle_state,
               ieee80211_connection_state   *newstate)
{
    disc_scan_event_info            *req_param;

    ASSERT(tx_fr_info->need_mini_find);

    /* Make sure that we not the middle of mini-find or transmitting an action frame.
     * If yes, then complete them.
     */
    /* First, check for the previous Transmit request */
    if (prot_handle->curr_tx_info) {
        /* Complete this request with error */
        tx_event_info   *old_tx_request;

        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
             "%s: Warning: Two Tx request. Failed the first one (%pK) with error.\n",
             __func__, prot_handle->curr_tx_info);

        old_tx_request = prot_handle->curr_tx_info;
        prot_handle->curr_tx_info = NULL;
        complete_tx_action_frame_event(prot_handle, old_tx_request, -EIO);
    }
    ASSERT(prot_handle->retry_tx_info == NULL);
    ASSERT(prot_handle->curr_tx_info == NULL);
    ASSERT(prot_handle->disc_scan_active == false);

    /* Make sure that there is no pending discovery scan */
    if (prot_handle->curr_disc_scan_info) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
            "%s:fails pending discovery scan request since mini-find is starting.\n", __func__);

        p2p_disc_scan_abort(prot_handle);
    }
    ASSERT(prot_handle->curr_disc_scan_info == NULL);

    /* Start a discovery scan to find this peer address */

    /* Prepare the scan parameters */
    req_param = create_minifind_disc_scan_request(prot_handle, tx_event_type, tx_fr_info);
    if (req_param == NULL) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
            "%s: abort tx request since create_minifind_disc_scan_request() failed.\n", __func__);

        complete_tx_action_frame_event(prot_handle, tx_fr_info, -EIO);
        return;
    }

    prot_handle->curr_disc_scan_info = req_param;

    ASSERT(prot_handle->def_probereq_ie_removed == false);
    ASSERT(req_param->remove_add_ie == false);

    set_device_filters(prot_handle);

    tx_fr_info->minifind_start = OS_GET_TIMESTAMP();

    /* Set the timeout for this Tx action frame request and mini-find discovery scan.
     * We set the discovery scan to be 1 unit lessor so that it will trigger first. */
    p2p_disc_timer_start(prot_handle, TIMER_TX_REQ_EXPIRE, tx_fr_info->timeout);
    p2p_disc_timer_start(prot_handle, TIMER_DISC_SCAN_EXPIRE, tx_fr_info->timeout - 1);

    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
         "%s: Start the mini-find. Timeout is %d\n", __func__, tx_fr_info->timeout);

    /* Switch to the DISC_SCAN state (via the IDLE_STATE) */
    if (in_idle_state) {
        /* Since we already in IDLE state, just start this discovery scan */
        p2p_prot_start_disc_scan(prot_handle, newstate);
    }
    else {
        /* Request to goto IDLE state and the Discovery scan will start from there */
        ASSERT(*newstate == P2P_DISC_STATE_OFF);
        *newstate = P2P_DISC_STATE_IDLE;
    }
}

/*
 * Common routine used in LISTEN and ONCHAN states to process the 
 * P2P_DISC_EVENT_TMR_TX_RETRY event.
 */
static void
st_listen_n_onchan_proc_tx_retry_event(wlan_p2p_prot_t prot_handle, sm_event_info *event)
{
    tx_event_info       *tx_fr_info;

    if (prot_handle->retry_tx_info) {
        tx_fr_info = prot_handle->retry_tx_info;
        prot_handle->retry_tx_info = NULL;

        if (prot_handle->curr_tx_req_id != tx_fr_info->tx_req_id) {
            /* Error: this should not happen. */
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: ERROR! got timer_tx_retry but not latest request (0x%x!=0x%x). No retry.\n",
                __func__, prot_handle->curr_tx_req_id, tx_fr_info->tx_req_id);
            ASSERT(false);
            complete_tx_action_frame_event(prot_handle, tx_fr_info, -EIO);
        }
        else {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                "%s: Retry the action frame transmit.\n", __func__);

            p2p_prot_send_action_frame(prot_handle, tx_fr_info);
        }
    }
    else {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
            "%s: Note: no retry frame available. Ignored this event. \n", __func__);
    }
}

/*
 * Common routine used in LISTEN and ONCHAN states to process all the transmit action frame,
 * P2P_DISC_EVENT_TX_*, events.
 */
static void
st_listen_n_onchan_proc_tx_action_fr_event(wlan_p2p_prot_t              prot_handle, 
                                           sm_event_info                *event, 
                                           ieee80211_connection_state   *newstate)
{
    /* Complete any previous Tx request that are waiting for retransmission */
    if (prot_handle->retry_tx_info) {
        /* Except GO_NEG_CONF, we will complete this previous request with error. */
        /* For GO_NEG_CONF, it is possible that the GO_NEG_REQ frame was
         * successfully transmited but the status is failure. But since we are sending
         * the GO_NEG_CONF, I assumed that the prior GO_NEG_REQ is successful. */
        int             retval;
        tx_event_info   *old_tx_request;

        if (event->event_type == P2P_DISC_EVENT_TX_GO_NEG_CONF) {
            retval = EOK;
        }
        else {
            retval = -EIO;
        }

        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
             "%s: Warning: New Tx request but pending retry frame. Completed the first one (%pK) with status=0x%x.\n",
             __func__, prot_handle->retry_tx_info, retval);

        p2p_disc_timer_stop(prot_handle, TIMER_TX_RETRY);   /* cancel the retry timer */

        old_tx_request = prot_handle->retry_tx_info;
        prot_handle->retry_tx_info = NULL;
        complete_tx_action_frame_event(prot_handle, old_tx_request, retval);
    }

    if (event->tx_info->need_mini_find) {
        /* We do not know where this peer is. Needs to do a mini-find */
        start_minifind(prot_handle, event->event_type, event->tx_info, false, newstate);
        event->tx_info = NULL;
    }
    else {
        // Else we know which channel to send action frame.
        if (p2p_prot_can_tx_action_frame(prot_handle, event->tx_info)) {
            /* We can transmit the frame now */
            int     retval;

            ASSERT(prot_handle->curr_tx_info == NULL);

            /* Setup this request */
            retval = setup_tx_action_frame_request(prot_handle, event->event_type, event->tx_info);
            if (retval != 0) {
                /* Some error in setup. Abort this transmit */
                complete_tx_action_frame_event(prot_handle, event->tx_info, retval);
                event->tx_info = NULL;
                return;
            }

            /* Send the action frame */
            p2p_prot_send_action_frame(prot_handle, event->tx_info);
            event->tx_info = NULL;
        }
        else {
            /* But first, abort the discovery scan (if any). */
            if (prot_handle->disc_scan_active) {
                /* abort this discovery scan */
                p2p_disc_scan_abort(prot_handle);
            }

            /* We need to re-schedule a new ONCHAN */
            proc_event_tx_frame(prot_handle, event->event_type, event->tx_info, newstate);
            event->tx_info = NULL;
        }
    }
}

/*
 * Process the TX_COMPLETE event that is used in states LISTEN and ONCHAN.
 */
static void
st_listen_n_onchan_proc_tx_complete(wlan_p2p_prot_t prot_handle, sm_event_info *event)
{
    bool            complete_tx = false;
    int             retstatus = EOK;
    tx_event_info   *tx_event = event->tx_complete.orig_tx_param;

    /* Note that the Tx complete could be from an earlier aborted transmit */
    if (event->tx_complete.status != EOK) {
        /* Fail to send. Check if we need to retry */

        if (prot_handle->curr_tx_req_id != tx_event->tx_req_id) {
            /* Since this is not the latest request, we will not retry */
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: err tx status=0x%x but not latest request (0x%x!=0x%x). No retry.\n",
                __func__, event->tx_complete.status, prot_handle->curr_tx_req_id, tx_event->tx_req_id);
            complete_tx = true;
            retstatus = -EIO;
        }
        else if (prot_handle->num_tx_frame_retries > 0) {
            /* Retry this transmission */

            if (event->tx_complete.orig_tx_param->tx_freq != prot_handle->curr_scan.set_chan_freq) {
                IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                    "%s: Retry tx but listen freq (%d) != tx freq (%d). Do later. id=0x%x\n", 
                    __func__, prot_handle->curr_scan.set_chan_freq,
                    event->tx_complete.orig_tx_param->tx_freq, tx_event->tx_req_id);
                ASSERT(prot_handle->retry_tx_info == NULL);
                prot_handle->retry_tx_info = event->tx_complete.orig_tx_param;
                event->tx_complete.orig_tx_param = NULL;
                complete_tx = false;
            }
            else {
                prot_handle->num_tx_frame_retries--;

                /* Store this tx param in prot_handle */
                ASSERT(prot_handle->retry_tx_info == NULL);
                prot_handle->retry_tx_info = event->tx_complete.orig_tx_param;
                event->tx_complete.orig_tx_param = NULL;

                complete_tx = false;

                /* Schedule a resend timer. We don't resend immediately */
                p2p_disc_timer_start(prot_handle, TIMER_TX_RETRY, DEF_TIMEOUT_TX_RETRY);
            }
        }
        else {
            /* Already at max. retry. Abort this Tx of action frame and 
             * indicate to caller and revert to Idle state */
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: Error retstatus: already at Max retry. Fail this tx. id=0x%x\n",
                __func__, tx_event->tx_req_id);

            complete_tx = true;
            retstatus = -EIO;
        }
    }
    else {
        /* Transmission is successful. Indicate status to caller. */
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
            "%s: tx completed successfully. id=0x%x\n", __func__, tx_event->tx_req_id);
        complete_tx = true;
        retstatus = EOK;
    }

    if (complete_tx) {
        complete_tx_action_frame_event(prot_handle, event->tx_complete.orig_tx_param, retstatus);
        event->tx_complete.orig_tx_param = NULL;
    }
}

/*
 * Routine is called in IDLE state to start a Discovery Scan.
 */
static void
p2p_prot_start_disc_scan(wlan_p2p_prot_t prot_handle, ieee80211_connection_state *newstate)
{
    disc_scan_event_info            *disc_info;

    ASSERT(prot_handle->curr_disc_scan_info != NULL);

    /* Init. the scan type to NONE (first step) */
    disc_info = prot_handle->curr_disc_scan_info;
    disc_info->curr_scan_type = P2P_PROT_SCAN_TYPE_NONE;
    prot_handle->disc_scan_active = true;

    ASSERT(*newstate == P2P_DISC_STATE_OFF);
    *newstate = P2P_DISC_STATE_DISCSCAN;
}

/*
 * Routine called before returning to IDLE state to see if there is
 * a pending Device Discovery Scan. Returns TRUE if there is a pending discovery.
 */
static inline bool
have_pending_disc_scan(wlan_p2p_prot_t prot_handle)
{
    if (prot_handle->curr_disc_scan_info != NULL) {
        return true;
    }
    else {
        return false;
    }
}

/*
 * This routine is called when at the end of channel and returning back to IDLE state
 * but may need to divert to another state due to pending request(s).
 * Called from LISTEN and ONCHAN states.
 */
static void
st_listen_n_onchan_proc_on_chan_end(wlan_p2p_prot_t prot_handle, ieee80211_connection_state *newstate)
{
    if (prot_handle->num_outstanding_tx > 0) {
        /* We have outstanding Transmit Action frame request */

        if (prot_handle->curr_tx_nextstate == P2P_DISC_STATE_PRE_ONCHAN) {
            prot_handle->curr_tx_nextstate = P2P_DISC_STATE_PRE_LISTEN;    /* flip it */

            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                "%s: Note: have outstanding tx=%d. Switch to PRE_ONCHAN to retry. dest_freq=%d\n",
                __func__, prot_handle->num_outstanding_tx, prot_handle->on_chan_dest_freq);

            ASSERT(*newstate == P2P_DISC_STATE_OFF);
            *newstate = P2P_DISC_STATE_PRE_ONCHAN;
        }
        else {
            ASSERT(prot_handle->curr_tx_nextstate == P2P_DISC_STATE_PRE_LISTEN);

            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                "%s: Note: have outstanding tx=%d. Switch to PRE_LISTEN to wait for awhile before next retry. dest_freq=%d\n",
                __func__, prot_handle->num_outstanding_tx, prot_handle->on_chan_dest_freq);

            prot_handle->curr_tx_nextstate = P2P_DISC_STATE_PRE_ONCHAN;    /* flip it */
            ASSERT(*newstate == P2P_DISC_STATE_OFF);
            *newstate = P2P_DISC_STATE_PRE_LISTEN;
        }
    }
    /* Else no outstanding tx and check if there is a pending discovery scan. */
    else if (have_pending_disc_scan(prot_handle)) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
            "%s: Have pended Discovery Scan request.\n", __func__);

        if (prot_handle->disc_scan_active) {
            /* Discovery scan has already started */
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                "%s: end of remain_on_channel, goto DISCSCAN state since disc_scan_active is true.\n",
                __func__);
            ASSERT(*newstate == P2P_DISC_STATE_OFF);
            *newstate = P2P_DISC_STATE_DISCSCAN;
        }
        else {
            /* Else start the discovery scan */
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                "%s: end of remain_on_channel, starts discovery scan.\n",
                __func__);
            p2p_prot_start_disc_scan(prot_handle, newstate);
        }
    }
    else {
        /* switch to IDLE state since no pending requests */
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
            "%s: end of remain_on_channel, switch back to IDLE since no pending requests.\n",
            __func__);
        ASSERT(*newstate == P2P_DISC_STATE_OFF);
        *newstate = P2P_DISC_STATE_IDLE;
    }
}

/*
 * Complete the Tx action frame event and indicate the return status code back to caller.
 */
static void
complete_tx_action_frame_event(wlan_p2p_prot_t prot_handle, tx_event_info *tx_event, int ret_status)
{
    wlan_p2p_prot_event     *ind_event;
    u_int32_t               out_ie_offset;

    ASSERT(prot_handle->ev_handler);

    ind_event = &tx_event->cb_event_param;
    ind_event->event_type = IEEE80211_P2P_PROT_EVENT_TX_ACT_FR_COMPL;
    ind_event->tx_act_fr_compl.frame_type = tx_event->orig_req_param.frtype;

    /* Cancel the timeout timer (if it still exist) */
    if (prot_handle->curr_tx_req_id == tx_event->tx_req_id) {
        p2p_disc_timer_stop(prot_handle, TIMER_TX_REQ_EXPIRE);
        prot_handle->curr_tx_req_id = 0;
    }
    else {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
            "%s: did not cancel TX_REQ_EXPIRE timer since ID don't match (0x%x!=0x%x)\n",
            __func__, prot_handle->curr_tx_req_id, tx_event->tx_req_id);
    }

    if (tx_event->out_ie_buf_len > 0) {
        /* Need to adjust the offset since the new offset will be relative
         * to the tx_act_fr_compl field that is inside the cb_event_param field */
        ASSERT(tx_event->out_ie_offset >= (offsetof(tx_event_info, cb_event_param) + 
                                          offsetof(wlan_p2p_prot_event, tx_act_fr_compl)));
        out_ie_offset = tx_event->out_ie_offset - offsetof(tx_event_info, cb_event_param)
                        - offsetof(wlan_p2p_prot_event, tx_act_fr_compl);
    }
    else {
        out_ie_offset = 0;
    }

    switch(tx_event->orig_req_param.frtype) {
    case IEEE80211_P2P_PROT_FTYPE_GO_NEG_REQ:
    {
        wlan_p2p_prot_event_tx_go_neg_req_compl *go_neg_req_param;

        go_neg_req_param = &(tx_event->cb_event_param.tx_act_fr_compl.go_neg_req);

        IEEE80211_ADDR_COPY(go_neg_req_param->peer_addr, tx_event->orig_req_param.go_neg_req.peer_addr);
        go_neg_req_param->dialog_token = tx_event->dialog_token;
        go_neg_req_param->status = ret_status;

        /* Setup the location of IE's used to send frame */
        go_neg_req_param->out_ie_buf_len = tx_event->out_ie_buf_len;
        go_neg_req_param->out_ie_offset = out_ie_offset;

        break;
    }
    case IEEE80211_P2P_PROT_FTYPE_GO_NEG_RESP:
    {
        wlan_p2p_prot_event_tx_go_neg_resp_compl *go_neg_resp_param;

        go_neg_resp_param = &(tx_event->cb_event_param.tx_act_fr_compl.go_neg_resp);

        IEEE80211_ADDR_COPY(go_neg_resp_param->peer_addr, tx_event->orig_req_param.go_neg_resp.peer_addr);
        go_neg_resp_param->dialog_token = tx_event->dialog_token;
        go_neg_resp_param->status = ret_status;

        /* Setup the location of IE's used to send frame */
        go_neg_resp_param->out_ie_buf_len = tx_event->out_ie_buf_len;
        go_neg_resp_param->out_ie_offset = out_ie_offset;

        break;
    }
    case IEEE80211_P2P_PROT_FTYPE_GO_NEG_CONF:
    {
        wlan_p2p_prot_event_tx_go_neg_conf_compl *go_neg_conf_param;

        go_neg_conf_param = &(tx_event->cb_event_param.tx_act_fr_compl.go_neg_conf);

        IEEE80211_ADDR_COPY(go_neg_conf_param->peer_addr, tx_event->orig_req_param.go_neg_conf.peer_addr);
        go_neg_conf_param->dialog_token = tx_event->dialog_token;
        go_neg_conf_param->status = ret_status;

        /* Setup the location of IE's used to send frame */
        go_neg_conf_param->out_ie_buf_len = tx_event->out_ie_buf_len;
        go_neg_conf_param->out_ie_offset = out_ie_offset;

        break;
    }
    case IEEE80211_P2P_PROT_FTYPE_PROV_DISC_REQ:
    {
        wlan_p2p_prot_event_tx_prov_disc_req_compl *prov_disc_req_param;

        prov_disc_req_param = &(tx_event->cb_event_param.tx_act_fr_compl.prov_disc_req);

        IEEE80211_ADDR_COPY(prov_disc_req_param->peer_addr, tx_event->orig_req_param.prov_disc_req.peer_addr);
        /* Copy the Receiver Address used when transmitting this frame */
        IEEE80211_ADDR_COPY(prov_disc_req_param->rx_addr, tx_event->rx_addr);
        prov_disc_req_param->dialog_token = tx_event->dialog_token;
        prov_disc_req_param->status = ret_status;

        /* Setup the location of IE's used to send frame */
        prov_disc_req_param->out_ie_buf_len = tx_event->out_ie_buf_len;
        prov_disc_req_param->out_ie_offset = out_ie_offset;

        break;
    }
    case IEEE80211_P2P_PROT_FTYPE_PROV_DISC_RESP:
    {
        wlan_p2p_prot_event_tx_prov_disc_resp_compl *prov_disc_resp_param;

        prov_disc_resp_param = &(tx_event->cb_event_param.tx_act_fr_compl.prov_disc_resp);

        IEEE80211_ADDR_COPY(prov_disc_resp_param->rx_addr, tx_event->orig_req_param.prov_disc_resp.rx_addr);
        prov_disc_resp_param->dialog_token = tx_event->dialog_token;
        prov_disc_resp_param->status = ret_status;

        /* Setup the location of IE's used to send frame */
        prov_disc_resp_param->out_ie_buf_len = tx_event->out_ie_buf_len;
        prov_disc_resp_param->out_ie_offset = out_ie_offset;
        break;
    }
    case IEEE80211_P2P_PROT_FTYPE_INVIT_REQ:
    {
        wlan_p2p_prot_event_tx_invit_req_compl *invit_req_param;

        invit_req_param = &(tx_event->cb_event_param.tx_act_fr_compl.invit_req);

        IEEE80211_ADDR_COPY(invit_req_param->peer_addr, tx_event->orig_req_param.invit_req.peer_addr);
        /* Copy the Receiver Address used when transmitting this frame */
        IEEE80211_ADDR_COPY(invit_req_param->rx_addr, tx_event->rx_addr);
        invit_req_param->dialog_token = tx_event->dialog_token;
        invit_req_param->status = ret_status;

        /* Setup the location of IE's used to send frame */
        invit_req_param->out_ie_buf_len = tx_event->out_ie_buf_len;
        invit_req_param->out_ie_offset = out_ie_offset;

        break;
    }
    case IEEE80211_P2P_PROT_FTYPE_INVIT_RESP:
    {
        wlan_p2p_prot_event_tx_invit_resp_compl *invit_resp_param;

        invit_resp_param = &(tx_event->cb_event_param.tx_act_fr_compl.invit_resp);

        IEEE80211_ADDR_COPY(invit_resp_param->rx_addr, tx_event->orig_req_param.invit_resp.rx_addr);
        invit_resp_param->dialog_token = tx_event->dialog_token;
        invit_resp_param->status = ret_status;

        /* Setup the location of IE's used to send frame */
        invit_resp_param->out_ie_buf_len = tx_event->out_ie_buf_len;
        invit_resp_param->out_ie_offset = out_ie_offset;
        break;
    }
    default:
        ASSERT(false);
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
            "%s: Unsupported frame type. frtype=%d, arg=%pK, tx_event_info=%pK.\n",
            __func__, tx_event->orig_req_param.frtype, prot_handle->event_arg, tx_event);
        break;
    }

    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
        "%s: Tx callback. frtype=%d, arg=%pK, tx_event_info=%pK, ret_status=0x%x, num_outstanding_tx=%d.\n",
        __func__, tx_event->orig_req_param.frtype, prot_handle->event_arg,
        tx_event, ret_status, prot_handle->num_outstanding_tx);

    if (tx_event->tx_req_id != 0) {
        ASSERT(prot_handle->num_outstanding_tx > 0);
        prot_handle->num_outstanding_tx--;
    }

    prot_handle->ev_handler(prot_handle->event_arg, ind_event);

    /* We can free this tx_event_info structure now */
    OS_FREE(tx_event);
}

/*
 * Common routine to process the event P2P_DISC_EVENT_APP_IE_UPDATE.
 * Note that normally, the Application IE for probe request frames are
 * send down from the upper stack. But during Discover Request scans and
 * if the discover request has its own IE, then we will overwrite this IE
 * during the duration of the discovery scan.
 */
static void
proc_event_app_ie_update(wlan_p2p_prot_t prot_handle, sm_event_info *event)
{
    int     retval;

    if (prot_handle->probe_req_add_ie != NULL) {
        /* free the old ie buffer which is not null. */
        OS_FREE(prot_handle->probe_req_add_ie);
        prot_handle->probe_req_add_ie = NULL;
        prot_handle->probe_req_add_ie_len = 0;
    }

    prot_handle->probe_req_add_ie = event->set_app_ie.probe_req_add_ie;
    prot_handle->probe_req_add_ie_len = event->set_app_ie.probe_req_add_ie_len;

    if (!prot_handle->def_probereq_ie_removed) {
        /* Default ProbeReq IE is not removed. Plump this new IE. */
        retval = wlan_mlme_set_appie(prot_handle->vap, IEEE80211_FRAME_TYPE_PROBEREQ,
                                     prot_handle->probe_req_add_ie, prot_handle->probe_req_add_ie_len);
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                             "%s: set new ProbeReq IE. len=%d, retval=0x%x\n",
                             __func__, prot_handle->probe_req_add_ie_len, retval);
    }
    else {
        /* If discovery scan has started, then this IE will be plump when the scan completes. */
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                             "%s: delay setting new ProbeReq IE. len=%d because discovery is ON\n",
                             __func__, prot_handle->probe_req_add_ie_len);
    }
}

/*
 * Function to check that the event data parameter is correct.
 * Note that different event types may have event data structures.
 */
static int
validate_event_param(wlan_p2p_prot_t prot_handle, u_int16_t event_type, void *event_data, u_int16_t event_data_len)
{
#define CHK_EVENT_PARAM(_type, _param_size)                                     \
    case _type:                                                                 \
    {                                                                           \
        if ((event_data == NULL) || (event_data_len < _param_size)) {           \
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS,   \
                IEEE80211_MSG_P2P_PROT,                                         \
                "%s: invalid event data. ptr=%pK, len=%d, min size=%d.\n",       \
                __func__, event_data, event_data_len, _param_size);             \
        }                                                                       \
        break;                                                                  \
    }

    switch(event_type) {
        CHK_EVENT_PARAM(P2P_DISC_EVENT_TX_GO_NEG_REQ, sizeof(sm_event_info));
        CHK_EVENT_PARAM(P2P_DISC_EVENT_TX_GO_NEG_RESP, sizeof(sm_event_info));
        CHK_EVENT_PARAM(P2P_DISC_EVENT_TX_GO_NEG_CONF, sizeof(sm_event_info));
        CHK_EVENT_PARAM(P2P_DISC_EVENT_TX_INVITE_REQ, sizeof(sm_event_info));
        CHK_EVENT_PARAM(P2P_DISC_EVENT_TX_INVITE_RESP, sizeof(sm_event_info));
        CHK_EVENT_PARAM(P2P_DISC_EVENT_TX_PROV_DISC_REQ, sizeof(sm_event_info));
        CHK_EVENT_PARAM(P2P_DISC_EVENT_TX_PROV_DISC_RESP, sizeof(sm_event_info));
        CHK_EVENT_PARAM(P2P_DISC_EVENT_SCAN_START, sizeof(sm_event_info));
        CHK_EVENT_PARAM(P2P_DISC_EVENT_SCAN_END, sizeof(sm_event_info));
        CHK_EVENT_PARAM(P2P_DISC_EVENT_SCAN_DEQUEUE, sizeof(sm_event_info));
        CHK_EVENT_PARAM(P2P_DISC_EVENT_ON_CHAN_START, sizeof(sm_event_info));
        CHK_EVENT_PARAM(P2P_DISC_EVENT_ON_CHAN_END, sizeof(sm_event_info));
        CHK_EVENT_PARAM(P2P_DISC_EVENT_RX_MGMT, sizeof(sm_event_info));
        CHK_EVENT_PARAM(P2P_DISC_EVENT_TX_COMPLETE, sizeof(sm_event_info));
        CHK_EVENT_PARAM(P2P_DISC_EVENT_DISC_SCAN_REQ, sizeof(sm_event_info));
        CHK_EVENT_PARAM(P2P_DISC_EVENT_APP_IE_UPDATE, sizeof(sm_event_info));

        case P2P_DISC_EVENT_START:
        case P2P_DISC_EVENT_END:
        case P2P_DISC_EVENT_RESET:
        case P2P_DISC_EVENT_START_GO:
        case P2P_DISC_EVENT_START_CLIENT:
        case P2P_DISC_EVENT_DISC_SCAN_INIT:
        case P2P_DISC_EVENT_TMR_DISC_LISTEN:
        case P2P_DISC_EVENT_TMR_SET_CHAN_SWITCH:
        case P2P_DISC_EVENT_TMR_TX_RETRY:
        case P2P_DISC_EVENT_TMR_RETRY_SET_CHAN:
        case P2P_DISC_EVENT_TMR_RETRY_SET_SCAN:
        case P2P_DISC_EVENT_TMR_DISC_SCAN_EXPIRE:
        case P2P_DISC_EVENT_LISTEN_ST_DISC_REQ:
        case P2P_DISC_EVENT_DISC_SCAN_STOP:
            /* No parameters required */
            break;

        default:
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                "%s: Unhandled event %s, 0x%x. Please add to this function.\n", __func__, event_name(event_type), event_type);
    }
    return EOK;

#undef CHK_EVENT_PARAM
}

/*
 * Callback function to iterate the Scan list and find a suitable peer. 
 * Called by wlan_scan_table_iterate().
 * Returns EOK if not found. 
 * Return -EALREADY if a peer is found and the iteration stops.
 */
static int
p2p_disc_find_peer(void *ctx, wlan_scan_entry_t entry)
{
    struct find_peer_param  *param = (struct find_peer_param *)ctx;
    u_int8_t                *entry_mac_addr;
    int                     retval;
    ieee80211_scan_p2p_info scan_p2p_info;
    bool                    peer_matched;
    u_int8_t                entry_ssid_len = 0;
    u_int8_t                *entry_ssid_ptr = NULL;
    bool                    entry_ssid_is_wildcard = true;
    bool                    ssid_matched = false;
    bool                    mac_addr_matched;
    bool                    p2p_device_addr_matched;

    param = (struct find_peer_param *)ctx;
    entry_mac_addr = wlan_scan_entry_macaddr(entry);

    ASSERT(param->match_entry == NULL);

    /* Check that this peer is a p2p device. Reject it otherwise. */
    retval = wlan_scan_entry_p2p_info(entry, &scan_p2p_info);
    if (retval != EOK) {
        IEEE80211_DPRINTF_VB(param->prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                             "%s: wlan_scan_entry_p2p_info ret err=0x%x, entry=%pK\n",
                             __func__, retval, entry);
        return EOK;     /* continue to the next one */
    }

    if (!scan_p2p_info.is_p2p) {
        /* Not a P2P, skip this entry and go to the next entry. */
        return EOK; 
    }

    do {
        peer_matched = false;

        mac_addr_matched = (OS_MEMCMP(param->mac_addr, entry_mac_addr, IEEE80211_ADDR_LEN) == 0);
        p2p_device_addr_matched = (OS_MEMCMP(param->mac_addr, scan_p2p_info.p2p_dev_addr, IEEE80211_ADDR_LEN) == 0);

        if ((!param->only_GO) && (param->ssid_info == NULL))
        {
            if ((mac_addr_matched) || (p2p_device_addr_matched)) {
                peer_matched = true;
            }
            break;
        }

        entry_ssid_ptr = wlan_scan_entry_ssid(entry, &entry_ssid_len);
        if ((param->ssid_info != NULL) && (entry_ssid_ptr != NULL)) {
            if ((param->ssid_info->len == entry_ssid_len) &&
                (OS_MEMCMP(entry_ssid_ptr, param->ssid_info->ssid, param->ssid_info->len) == 0))
            {
                ssid_matched = true;
            }
        }

        if ((!param->only_GO) && (param->ssid_info != NULL))
        {
            if (mac_addr_matched) {
                peer_matched = true;
            }
            else if ((p2p_device_addr_matched) && (ssid_matched)) {
                peer_matched = true;
            }
            break;
        }

        /* Check the incoming SSID to make sure that it is from a GO */
        entry_ssid_is_wildcard = true;
        if (entry_ssid_ptr != NULL) {
            if ((entry_ssid_len != IEEE80211_P2P_WILDCARD_SSID_LEN) ||
                (OS_MEMCMP(entry_ssid_ptr, IEEE80211_P2P_WILDCARD_SSID, IEEE80211_P2P_WILDCARD_SSID_LEN) != 0))
            {
                entry_ssid_is_wildcard = false;
            }
        }

        if ((param->only_GO) && (param->ssid_info == NULL))
        {
            if ((!entry_ssid_is_wildcard) && (p2p_device_addr_matched)) {
                peer_matched = true;
            }
            break;
        }
        if ((param->only_GO) && (param->ssid_info != NULL))
        {
            if ((!entry_ssid_is_wildcard) && (p2p_device_addr_matched) && (ssid_matched)) {
                peer_matched = true;
            }
            break;
        }

    } while ( false );

    if (mac_addr_matched || p2p_device_addr_matched) {
        IEEE80211_DPRINTF_VB(param->prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                             "%s: peer_matched=%d, addr=" MAC_ADDR_FMT ", mac_addr_matched=%d, p2p_device_addr_matched=%d,\n"
                             "\tparam->only_GO=%d, param->ssid_info=%pK, entry_ssid_ptr=%pK, ssid_matched=%d, entry_ssid_is_wildcard=%d\n",
                             __func__, peer_matched, SPLIT_MAC_ADDR(entry_mac_addr),
                             mac_addr_matched, p2p_device_addr_matched, param->only_GO,
                             param->ssid_info, entry_ssid_ptr, ssid_matched, entry_ssid_is_wildcard);
    }

    if (!peer_matched) {
        /* no match */
        return EOK;
    }

    param->match_entry = entry;

    ASSERT(param->p2p_info != NULL);
    OS_MEMCPY(param->p2p_info, &scan_p2p_info, sizeof(ieee80211_scan_p2p_info));

    /* Add refcnt so that this entry don't get freed. */
    ieee80211_scan_entry_add_reference(entry);

    /* Return an error so that we stop iterating the list */
    return -EALREADY;
}

/*
 * Callback function to iterate the Scan list and find a suitable peer that are
 * associated with the Group Owner. 
 * Called by wlan_scan_table_iterate().
 * Returns EOK if not found. 
 * Return -EALREADY if a peer is found and the iteration stops.
 */
static int
p2p_disc_find_peer_in_grp_owner(void *ctx, wlan_scan_entry_t entry)
{
    struct find_peer_param  *param = (struct find_peer_param *)ctx;
    u_int8_t                *entry_mac_addr = wlan_scan_entry_macaddr(entry);
    struct p2p_parsed_ie    pie;
    u8                      *ie_data;
    u_int16_t               ie_len;
    int                     retval = EOK;
    ieee80211_scan_p2p_info scan_p2p_info;
    wlan_if_t               vap;

    param = (struct find_peer_param *)ctx;
    entry_mac_addr = wlan_scan_entry_macaddr(entry);

    ASSERT(param->match_entry == NULL);

    vap = param->prot_handle->vap;

    /* Check that this peer is a p2p device. Reject it otherwise. */
    retval = wlan_scan_entry_p2p_info(entry, &scan_p2p_info);
    if (retval != EOK) {
        IEEE80211_DPRINTF_VB(param->prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                             "%s: wlan_scan_entry_p2p_info ret err=0x%x, entry=%pK\n",
                             __func__, retval, entry);
        return EOK;     /* continue to the next one */
    }
    if (!scan_p2p_info.is_p2p) {
        /* Not a P2P, skip this entry and go to the next entry. */
        return EOK; 
    }

    /* Parse the Probe Response frame for the client info. */
    ie_data = param->tmp_buf;
    ie_len = IEEE80211_MAX_MPDU_LEN;
    retval = wlan_scan_specific_frame_copy_ie_data(entry, IEEE80211_FC0_SUBTYPE_PROBE_RESP,
                                                        ie_data, &ie_len);
    if ((retval != 0) || (ie_len == 0)) {
        /* Unable to get the Probe Response IE */
        IEEE80211_DPRINTF_VB(param->prot_handle->vap, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_PROT,
                             "%s: Skip addr="MAC_ADDR_FMT" since No Probe Resp IE. scan entry=0x%x. retval=0x%x\n",
                             __func__, SPLIT_MAC_ADDR(entry_mac_addr), entry, retval);
        return EOK; 
    }

    OS_MEMZERO(&pie, sizeof(pie));

    if (ieee80211_p2p_parse_ies(param->prot_handle->os_handle, param->prot_handle->vap, ie_data, ie_len, &pie) != 0) {
        /* Some error in parsing */
        IEEE80211_DPRINTF_VB(param->prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                             "%s: Error in parsing IE. Addr=" MACSTR "\n",
                             __func__, MAC2STR(entry_mac_addr));
        return EOK;
    }

    do {
        
        if ((pie.group_info == NULL) || (pie.group_info_len == 0)) {
            /* No Group Info subattribute. Skip */
            retval = EOK;
            break;
        }

        IEEE80211_DPRINTF_VB(vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                             "%s: Have Group IE, len=%d. Addr=" MACSTR "\n",
                             __func__, pie.group_info_len, MAC2STR(entry_mac_addr));

        /* STNG TODO: INCOMPLETE CODE: we need to add code to parse and extract the client info. */
        retval = EOK;
        break;

    } while ( false );

    if (retval == -EALREADY) {
        /* Found the P2P device address */
        IEEE80211_DPRINTF_VB(param->prot_handle->vap, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_PROT,
                             "%s: Found a match in MAC address="MAC_ADDR_FMT" scan entry=0x%x.\n",
                             __func__, SPLIT_MAC_ADDR(entry_mac_addr), entry);

        param->match_entry = entry;

        ASSERT(param->p2p_info != NULL);
        OS_MEMCPY(param->p2p_info, &scan_p2p_info, sizeof(ieee80211_scan_p2p_info));

        /* Add refcnt so that this entry don't get freed. */
        ieee80211_scan_entry_add_reference(entry);
    }

    ieee80211_p2p_parse_free(&pie);

    return retval;
}

/*
 * This function will add a channel list sub-attribute that contains
 * only a single channel.
 * Return EOK if success. Else return <0 if error.
 */
static int
p2p_prot_add_single_channel_list(wlan_p2p_prot_t prot_handle, p2pbuf *ies, u8 reg_class, u8 chan_num)
{
    struct p2p_channels *new_chanlist;

    /* Allocate the memory for channel structure since it is too big for stack */
    new_chanlist = (struct p2p_channels *)OS_MALLOC(prot_handle->os_handle, 
                                                    sizeof(struct p2p_channels), 0);
    if (new_chanlist == NULL) {
        IEEE80211_DPRINTF(prot_handle->vap, IEEE80211_MSG_P2P_PROT, 
                          "%s: Error: failed to alloc param buf, size=%d\n",
                          __func__, sizeof(struct p2p_channels));
        return -ENOMEM;
    }

    new_chanlist->reg_classes = 1;
    new_chanlist->reg_class[0].reg_class = reg_class;
    new_chanlist->reg_class[0].channels = 1;
    new_chanlist->reg_class[0].channel[0] = chan_num;

    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                         "%s: channel list is restrict to single chan. regclass=%d chan=%d.\n",
                         __func__, reg_class, chan_num);

    p2pbuf_add_channel_list(ies, prot_handle->country, new_chanlist);

    OS_FREE(new_chanlist);

    return EOK;
}

/*
 * Build the payload for a GO NEGOTIATION REQUEST frame.
 * The created payload is in tx_fr_info->frame_buf and length is tx_fr_info->frame_len.
 */
static int p2p_prot_build_group_neg_req_frame(
    wlan_p2p_prot_t prot_handle, 
    tx_event_info   *tx_fr_info)
{
    p2pbuf      p2p_buf, *ies;
    u_int8_t    *frm;
    int         buf_len;
    int         ie_len;
    u_int8_t    *ielen_ptr;
    u_int8_t    go_intent;
    u_int32_t   op_chan_freq = 0;
    bool        op_chan_restricted = false;
    u8          op_chan_reg_class = 0, op_chan_channel = 0;
    bool        have_preferred_op_chan = false;
    u8          pref_chan_reg_class = 0, pref_chan_channel = 0;
    u8          tmp_reg_class, tmp_chan_num;

    ies = &p2p_buf;
    frm = tx_fr_info->frame_buf;
    buf_len = IEEE80211_RTS_MAX;
    p2pbuf_init(ies, frm, buf_len);

    p2pbuf_add_public_action_hdr(ies, P2P_GO_NEG_REQ, tx_fr_info->dialog_token);

    ASSERT((p2pbuf_get_bufptr(ies) - tx_fr_info->frame_buf) == IEEE80211_P2P_ACTION_FR_IE_OFFSET);

    ielen_ptr = p2pbuf_add_ie_hdr(ies);

    spin_lock(&(prot_handle->set_param_lock));  /* mutual exclusion with OS updating the parameters */

    /* Note that the group capability is from the input parameter */
    p2pbuf_add_capability(ies, prot_handle->dev_capab, 
                          tx_fr_info->orig_req_param.go_neg_req.grp_capab);

    go_intent = tx_fr_info->orig_req_param.go_neg_req.go_intent.intent;
    go_intent = go_intent << 1;
    go_intent |= tx_fr_info->orig_req_param.go_neg_req.go_intent.tie_breaker;
    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                         "%s: dialog_token=%d, group intent=0x%x, val=%d, tiebreaker=%d\n",
                         __func__, tx_fr_info->dialog_token, go_intent,
                         tx_fr_info->orig_req_param.go_neg_req.go_intent.intent,
                         tx_fr_info->orig_req_param.go_neg_req.go_intent.tie_breaker);
    p2pbuf_add_go_intent(ies, go_intent);
    /* Note that we let the OS flip the tie-breaker in its next request */
    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                         "%s: min config time (units of 10 millisec): go=%d, client=%d\n",
                         __func__, tx_fr_info->orig_req_param.go_neg_req.min_config_go_timeout,
                         tx_fr_info->orig_req_param.go_neg_req.min_config_client_timeout);
    p2pbuf_add_config_timeout(ies, tx_fr_info->orig_req_param.go_neg_req.min_config_go_timeout,
                              tx_fr_info->orig_req_param.go_neg_req.min_config_client_timeout);
    p2pbuf_add_listen_channel(ies, prot_handle->country,
                   prot_handle->listen_chan.reg_class,
                   prot_handle->listen_chan.ieee_num);
    if (prot_handle->ext_listen_interval) {
        p2pbuf_add_ext_listen_timing(ies, prot_handle->ext_listen_period,
                          prot_handle->ext_listen_interval);
    }
    p2pbuf_add_intended_addr(ies, tx_fr_info->orig_req_param.go_neg_req.own_interface_addr);

    /*
     * Have to consider other stations who are connected and cannot move to another channel.
     * Specifically, when the infra-station or/and p2p client is associated and 
     * off-channel support is not enabled, then Operational channel should be exactly 
     * the station/client channel.
     */
    op_chan_restricted = p2p_prot_is_op_chan_restricted(prot_handle, &prot_handle->chanlist, 
                                                        &have_preferred_op_chan, &op_chan_freq,
                                                        &tmp_reg_class, &tmp_chan_num);

    if (op_chan_restricted) {
        /* Restrict the Op channel to this frequency */
        int                 retval;

        if (op_chan_freq == 0) {
            /* Error: cannot find a compatible Operational Channel */
            IEEE80211_DPRINTF(prot_handle->vap, IEEE80211_MSG_P2P_PROT, 
                              "%s: Error: op chan is restricted but no compatible freq.\n",
                              __func__);
            spin_unlock(&(prot_handle->set_param_lock));
            return -EINVAL;
        }

        op_chan_reg_class = tmp_reg_class;
        op_chan_channel = tmp_chan_num;

        /* Add the channel list sub-attribute that has one channel */
        retval = p2p_prot_add_single_channel_list(prot_handle, ies, tmp_reg_class, tmp_chan_num);
        if (retval != EOK) {
            spin_unlock(&(prot_handle->set_param_lock));
            return retval;
        }
    }
    else {
        /* Else use the default channel list without restrictions */
        p2pbuf_add_channel_list(ies, prot_handle->country, &prot_handle->chanlist);

        if (have_preferred_op_chan) {
            /* No op channel restriction but have a preferred channel */
            ASSERT((tmp_reg_class != 0) && (tmp_chan_num != 0));
            pref_chan_reg_class = tmp_reg_class;
            pref_chan_channel = tmp_chan_num;
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                                 "%s: Have pref op chan freq=%d and is in intersect chanlist. regclass=%d, chan=%d\n",
                                 __func__, op_chan_freq, pref_chan_reg_class, pref_chan_channel);
        }
    }

    /* P2P Device Info */
    p2pbuf_add_device_info(ies, prot_handle->vap->iv_myaddr, 
                           (u_int8_t *)prot_handle->p2p_dev_name, prot_handle->p2p_dev_name_len, 
                           prot_handle->configMethods, prot_handle->primaryDeviceType,
                           prot_handle->numSecDeviceTypes, prot_handle->secondaryDeviceTypes);

    if (op_chan_restricted) {
        ASSERT(op_chan_channel != 0);
        ASSERT((op_chan_reg_class != 0) && (op_chan_channel != 0));
        p2pbuf_add_operating_channel(ies, prot_handle->country,
                                     op_chan_reg_class, op_chan_channel);
    }
    else if (have_preferred_op_chan) {
        /* No op channel restriction but have a preferred channel */
        ASSERT((pref_chan_reg_class != 0) && (pref_chan_channel != 0));
        p2pbuf_add_operating_channel(ies, prot_handle->country,
                       pref_chan_reg_class, pref_chan_channel);
    }
    else {
        /* No channel restrictions. Use the default Op channel */
        p2pbuf_add_operating_channel(ies, prot_handle->country,
                       prot_handle->op_chan.reg_class,
                       prot_handle->op_chan.ieee_num);
    }

    spin_unlock(&(prot_handle->set_param_lock));

    p2pbuf_update_ie_hdr(ies, ielen_ptr, &ie_len);  /* last thing to do is update the IE length */

    /* Note: we assumed the WPS IE with Device Password ID attribute is in the additional IE */
    /* Add the IE's from the OS */
    if (tx_fr_info->in_ie_buf_len > 0) {
        // Copy the IE buffer.
        u_int8_t        *src_buf = &(((u_int8_t *)tx_fr_info)[tx_fr_info->in_ie_offset]);

        p2pbuf_put_data(ies, src_buf, tx_fr_info->in_ie_buf_len);
    }
    /* calculate the entire frame length */
    tx_fr_info->frame_len = p2pbuf_get_bufptr(ies) - tx_fr_info->frame_buf;

    return EOK;
}

/*
 * Reselect a Op channel from a channel list.
 */
static void
reselect_op_chan(struct p2p_channels *chan_list, u8 *op_reg_class, u8 *op_chan)
{
    /* For now, we choose the first one.
     * TODO: we should have a better algorithm */
    ASSERT(chan_list->reg_classes >= 1);
    ASSERT(chan_list->reg_class[0].channels >= 1);
    *op_reg_class = chan_list->reg_class[0].reg_class;
    *op_chan = chan_list->reg_class[0].channel[0];
}

/*
 * Build the payload for a GO NEGOTIATION RESPONSE frame.
 * The created payload is in tx_fr_info->frame_buf and length is tx_fr_info->frame_len.
 */
static int p2p_prot_build_group_neg_resp_frame(
    wlan_p2p_prot_t prot_handle, 
    tx_event_info   *tx_fr_info)
{
    p2pbuf                              p2p_buf, *ies;
    u_int8_t                            *frm;
    int                                 buf_len;
    int                                 ie_len;
    u_int8_t                            *ielen_ptr;
    u_int8_t                            go_intent;
    wlan_p2p_prot_tx_go_neg_resp_param  *param;
    bool                                we_are_GO = false;
    bool                                sta_chan_restricted = false;
    u8                                  sta_chan_reg_class = 0, sta_chan_channel = 0;
    bool                                status_changed = false;
    struct p2p_channels                 *chan_list;
    u8                                  op_chan_reg_class;
    u8                                  op_chan_channel;
    const bool                          processing_resp_frame = true;

    param = &tx_fr_info->orig_req_param.go_neg_resp;

    /* Check and set the channel information to use */
    p2p_prot_check_and_set_chan_info(prot_handle, tx_fr_info, processing_resp_frame,
                                     &status_changed, &chan_list, 
                                     &op_chan_reg_class, &op_chan_channel);

    ies = &p2p_buf;
    frm = tx_fr_info->frame_buf;
    buf_len = IEEE80211_RTS_MAX;
    p2pbuf_init(ies, frm, buf_len);

    p2pbuf_add_public_action_hdr(ies, P2P_GO_NEG_RESP, tx_fr_info->dialog_token);

    ielen_ptr = p2pbuf_add_ie_hdr(ies);

    spin_lock(&(prot_handle->set_param_lock));  /* mutual exclusion with OS updating the parameters */

    /* Status attribute ID */
    p2pbuf_add_status(ies, param->status);

    /* Note that the group capability is from the input parameter */
    p2pbuf_add_capability(ies, prot_handle->dev_capab, param->grp_capab);

    go_intent = param->go_intent.intent;
    go_intent = go_intent << 1;
    go_intent |= param->go_intent.tie_breaker;
    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                         "%s: group intent=0x%x, val=%d, tiebreaker=%d\n",
                         __func__, go_intent,
                         param->go_intent.intent,
                         param->go_intent.tie_breaker);
    p2pbuf_add_go_intent(ies, go_intent);
    /* Note that we let the OS flip the tie-breaker in its next request */

    /* Note: assumed that use_grp_id is equal to "We are GO" */
    we_are_GO = param->use_grp_id;

    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                         "%s: min config time (units of 10 millisec): go=%d, client=%d, we_are_GO=%d\n",
                         __func__, param->min_config_go_timeout,
                         param->min_config_client_timeout, we_are_GO);
    p2pbuf_add_config_timeout(ies, param->min_config_go_timeout,
                              param->min_config_client_timeout);

    if (op_chan_channel != 0) {
        p2pbuf_add_operating_channel(ies, prot_handle->country, op_chan_reg_class, op_chan_channel);
    }
    /* Else skip adding the Op channel IE */

    p2pbuf_add_intended_addr(ies, param->own_interface_addr);

    p2pbuf_add_channel_list(ies, prot_handle->country, chan_list);

    /* P2P Device Info */
    p2pbuf_add_device_info(ies, prot_handle->vap->iv_myaddr, 
                           (u_int8_t *)prot_handle->p2p_dev_name, prot_handle->p2p_dev_name_len, 
                           prot_handle->configMethods, prot_handle->primaryDeviceType,
                           prot_handle->numSecDeviceTypes, prot_handle->secondaryDeviceTypes);

    if (param->use_grp_id) {
        p2pbuf_add_group_id(ies, param->grp_id.device_addr, param->grp_id.ssid.ssid, param->grp_id.ssid.len);
    }
    else {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                             "%s: No group ID and we_are_GO or use_grp_id=%d\n",
                             __func__, param->use_grp_id);
    }

    spin_unlock(&(prot_handle->set_param_lock));

    p2pbuf_update_ie_hdr(ies, ielen_ptr, &ie_len);  /* last thing to do is update the IE length */

    /* Note: we assumed the WPS IE with Device Password ID attribute is in the additional IE */
    /* Add the IE's from the OS */
    if (tx_fr_info->in_ie_buf_len > 0) {
        // Copy the IE buffer.
        u_int8_t        *src_buf = &(((u_int8_t *)tx_fr_info)[tx_fr_info->in_ie_offset]);

        p2pbuf_put_data(ies, src_buf, tx_fr_info->in_ie_buf_len);
    }

    /* calculate the entire frame length */
    tx_fr_info->frame_len = p2pbuf_get_bufptr(ies) - tx_fr_info->frame_buf;

    return EOK;
}

/*
 * Build the payload for a GO NEGOTIATION CONFIRM frame.
 * The created payload is in tx_fr_info->frame_buf and length is tx_fr_info->frame_len.
 */
static int p2p_prot_build_group_neg_conf_frame(
    wlan_p2p_prot_t prot_handle, 
    tx_event_info   *tx_fr_info)
{
    p2pbuf                              p2p_buf, *ies;
    u_int8_t                            *frm;
    int                                 buf_len;
    int                                 ie_len;
    u_int8_t                            *ielen_ptr;
    u_int8_t                            go_intent;
    wlan_p2p_prot_tx_go_neg_conf_param  *param;
    bool                                status_changed = false;
    struct p2p_channels                 *chan_list;
    u8                                  op_chan_reg_class;
    u8                                  op_chan_channel;
    const bool                          processing_resp_frame = false;

    /* Check and set the channel information to use */
    p2p_prot_check_and_set_chan_info(prot_handle, tx_fr_info, processing_resp_frame,
                                     &status_changed, &chan_list, 
                                     &op_chan_reg_class, &op_chan_channel);

    param = &tx_fr_info->orig_req_param.go_neg_conf;

    ies = &p2p_buf;
    frm = tx_fr_info->frame_buf;
    buf_len = IEEE80211_RTS_MAX;
    p2pbuf_init(ies, frm, buf_len);

    p2pbuf_add_public_action_hdr(ies, P2P_GO_NEG_CONF, tx_fr_info->dialog_token);

    ielen_ptr = p2pbuf_add_ie_hdr(ies);

    spin_lock(&(prot_handle->set_param_lock));  /* mutual exclusion with OS updating the parameters */

    /* Status attribute ID */
    p2pbuf_add_status(ies, param->status);

    /* Note that the group capability is from the input parameter */
    p2pbuf_add_capability(ies, prot_handle->dev_capab, param->grp_capab);

    if (op_chan_channel != 0) {
        p2pbuf_add_operating_channel(ies, prot_handle->country, op_chan_reg_class, op_chan_channel);
    }
    /* Else skip adding the Op channel IE */

    p2pbuf_add_channel_list(ies, prot_handle->country, chan_list);

    if (param->use_grp_id) {
        p2pbuf_add_group_id(ies, param->grp_id.device_addr, param->grp_id.ssid.ssid, param->grp_id.ssid.len);
    }

    spin_unlock(&(prot_handle->set_param_lock));

    p2pbuf_update_ie_hdr(ies, ielen_ptr, &ie_len);  /* last thing to do is update the IE length */

    /* Note: we assumed the WPS IE with Device Password ID attribute is in the additional IE */
    /* Add the IE's from the OS */
    if (tx_fr_info->in_ie_buf_len > 0) {
        // Copy the IE buffer.
        u_int8_t        *src_buf = &(((u_int8_t *)tx_fr_info)[tx_fr_info->in_ie_offset]);

        p2pbuf_put_data(ies, src_buf, tx_fr_info->in_ie_buf_len);
    }

    /* calculate the entire frame length */
    tx_fr_info->frame_len = p2pbuf_get_bufptr(ies) - tx_fr_info->frame_buf;

    return EOK;
}

/*
 * Build the payload for a PROVISION DISCOVERY REQUEST frame.
 * The created payload is in tx_fr_info->frame_buf and length is tx_fr_info->frame_len.
 */
static int p2p_prot_build_prov_disc_req_frame(
    wlan_p2p_prot_t prot_handle, 
    tx_event_info   *tx_fr_info)
{
    p2pbuf                                  p2p_buf, *ies;
    u_int8_t                                *frm;
    int                                     buf_len;
    int                                     ie_len;
    u_int8_t                                *ielen_ptr;
    u_int8_t                                go_intent;
    wlan_p2p_prot_tx_prov_disc_req_param    *param;

    param = &tx_fr_info->orig_req_param.prov_disc_req;

    ies = &p2p_buf;
    frm = tx_fr_info->frame_buf;
    buf_len = IEEE80211_RTS_MAX;
    p2pbuf_init(ies, frm, buf_len);

    p2pbuf_add_public_action_hdr(ies, P2P_PROV_DISC_REQ, tx_fr_info->dialog_token);

    ielen_ptr = p2pbuf_add_ie_hdr(ies);

    spin_lock(&(prot_handle->set_param_lock));  /* mutual exclusion with OS updating the parameters */

    /* Note that the group capability is from the input parameter */
    p2pbuf_add_capability(ies, prot_handle->dev_capab, 
                          tx_fr_info->orig_req_param.prov_disc_req.grp_capab);

    p2pbuf_add_device_info(ies, prot_handle->vap->iv_myaddr, 
                           (u_int8_t *)prot_handle->p2p_dev_name, prot_handle->p2p_dev_name_len, 
                           prot_handle->configMethods, prot_handle->primaryDeviceType,
                           prot_handle->numSecDeviceTypes, prot_handle->secondaryDeviceTypes);

    if (param->use_grp_id) {
        /* Add the group ID only when this flag is true */
        p2pbuf_add_group_id(ies, param->grp_id.device_addr, param->grp_id.ssid.ssid, param->grp_id.ssid.len);
    }

    spin_unlock(&(prot_handle->set_param_lock));

    p2pbuf_update_ie_hdr(ies, ielen_ptr, &ie_len);  /* last thing to do is update the IE length */

    /* Note: we assumed the WPS IE with Device Password ID attribute is in the additional IE */
    /* Add the IE's from the OS */
    if (tx_fr_info->in_ie_buf_len > 0) {
        // Copy the IE buffer.
        u_int8_t        *src_buf = &(((u_int8_t *)tx_fr_info)[tx_fr_info->in_ie_offset]);

        p2pbuf_put_data(ies, src_buf, tx_fr_info->in_ie_buf_len);
    }
    /* calculate the entire frame length */
    tx_fr_info->frame_len = p2pbuf_get_bufptr(ies) - tx_fr_info->frame_buf;

    return EOK;
}

/*
 * Build the payload for a PROVISION DISCOVERY RESPONSE frame.
 * The created payload is in tx_fr_info->frame_buf and length is tx_fr_info->frame_len.
 */
static int p2p_prot_build_prov_disc_resp_frame(
    wlan_p2p_prot_t prot_handle, 
    tx_event_info   *tx_fr_info)
{
    p2pbuf                                  p2p_buf, *ies;
    u_int8_t                                *frm;
    int                                     buf_len;
    wlan_p2p_prot_tx_prov_disc_resp_param   *param;

    param = &tx_fr_info->orig_req_param.prov_disc_resp;

    ies = &p2p_buf;
    frm = tx_fr_info->frame_buf;
    buf_len = IEEE80211_RTS_MAX;
    p2pbuf_init(ies, frm, buf_len);

    p2pbuf_add_public_action_hdr(ies, P2P_PROV_DISC_RESP, tx_fr_info->dialog_token);

    /* Note: we assumed the WPS IE with Device Password ID attribute is in the additional IE */
    /* Add the IE's from the OS */
    if (tx_fr_info->in_ie_buf_len > 0) {
        // Copy the IE buffer.
        u_int8_t        *src_buf = &(((u_int8_t *)tx_fr_info)[tx_fr_info->in_ie_offset]);

        p2pbuf_put_data(ies, src_buf, tx_fr_info->in_ie_buf_len);
    }
    /* calculate the entire frame length */
    tx_fr_info->frame_len = p2pbuf_get_bufptr(ies) - tx_fr_info->frame_buf;

    return EOK;
}

/*
 * Build the payload for an INVITATION REQUEST frame.
 * The created payload is in tx_fr_info->frame_buf and length is tx_fr_info->frame_len.
 */
static int p2p_prot_build_invit_req_frame(
    wlan_p2p_prot_t prot_handle, 
    tx_event_info   *tx_fr_info)
{
    p2pbuf                                  p2p_buf, *ies;
    u_int8_t                                *frm;
    int                                     buf_len;
    int                                     ie_len;
    u_int8_t                                *ielen_ptr;
    u_int8_t                                go_intent;
    wlan_p2p_prot_tx_invit_req_param        *param;
    bool                                    we_are_GO, start_new_group;
    bool                                    op_chan_is_restricted = false;
    u8                                      op_chan_reg_class = 0;
    u8                                      op_chan_channel = 0;

    param = &tx_fr_info->orig_req_param.invit_req;

    ies = &p2p_buf;
    frm = tx_fr_info->frame_buf;
    buf_len = IEEE80211_RTS_MAX;
    p2pbuf_init(ies, frm, buf_len);

    p2pbuf_add_public_action_hdr(ies, P2P_INVITATION_REQ, tx_fr_info->dialog_token);

    ielen_ptr = p2pbuf_add_ie_hdr(ies);

    spin_lock(&(prot_handle->set_param_lock));  /* mutual exclusion with OS updating the parameters */

    if (IEEE80211_ADDR_EQ(param->grp_id.device_addr, prot_handle->vap->iv_myaddr)) {
        /* We are going to be the Group Owner */
        we_are_GO = true;
    }
    else {
        we_are_GO = false;
    }

    p2pbuf_add_config_timeout(ies, param->min_config_go_timeout, param->min_config_client_timeout);

    {
        u_int8_t    flags;

        flags = param->invitation_flags;
        flags &= WLAN_P2P_PROT_INVIT_TYPE_MASK;
        if (flags == WLAN_P2P_PROT_INVIT_TYPE_REINVOKE) {
            /* Start a group */
            p2pbuf_add_invitation_flags(ies, P2P_INVITATION_FLAGS_TYPE_REINVOKE_PERSISTENT_GRP);
            start_new_group = true;

            if (we_are_GO && (!param->use_grp_bssid)) {
                /* Error: we must have the group BSSID when we are the group owner */
                IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                    "%s: Warning: we are group owner but grp bssid is not given.\n", __func__);
            }
        }
        else {
            /* Join an existing group */
            start_new_group = false;
            p2pbuf_add_invitation_flags(ies, P2P_INVITATION_FLAGS_TYPE_JOIN_ACTIVE_GRP);

            /* Extra checking */
            if (!param->use_grp_bssid) {
                /* Error: we must have the group BSSID when joining an existing group */
                IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                    "%s: Warning: join active group but grp bssid is not given.\n", __func__);
            }
        }
    }

    if (!start_new_group) {
        /* Join existing group, then Op Chan. should be specified */
        if (!param->use_specified_op_chan) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: Error: join our grp but no op chan parameter.\n", __func__);
        }
    }
    else {
        /* Start new group. Choose our Op channel while taking into consideration
         * of the existing channel usage. */
        bool        have_preferred_op_chan = false;
        u_int32_t   op_chan_freq = 0;

        op_chan_is_restricted = p2p_prot_is_op_chan_restricted(prot_handle, &prot_handle->chanlist,
                                                               &have_preferred_op_chan, &op_chan_freq,
                                                               &op_chan_reg_class, &op_chan_channel);
        if (op_chan_is_restricted) {
            /* Op channel is restricted */
            if ((op_chan_freq == 0) ||
                (op_chan_reg_class == 0) || (op_chan_channel == 0))
            {
                IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                    "%s: Error: Op Chan restricted and no valid chan. freq=%d, reg_class=%d, chan=%d\n",
                    __func__, op_chan_freq, op_chan_reg_class, op_chan_channel);

                op_chan_reg_class = 0;
                op_chan_channel = 0;
            }
            else {
                IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                    "%s: Note: Op Chan restricted. freq=%d, reg_class=%d, chan=%d\n",
                    __func__, op_chan_freq, op_chan_reg_class, op_chan_channel);
            }
        }
        else {
            if (have_preferred_op_chan) {
                /* Op channel is not restricted but have Op chan */
                IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                    "%s: No op chan restriction but have preferred reg_class=%d, chan=%d.\n",
                    __func__, op_chan_reg_class, op_chan_channel);
            }
            else {
                /* No op channel restriction, use our default Op channel */
                op_chan_reg_class = prot_handle->op_chan.reg_class;
                op_chan_channel = prot_handle->op_chan.ieee_num;

                IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                    "%s: No op chan restriction and no preferred. reg_class=%d, chan=%d.\n",
                    __func__, op_chan_reg_class, op_chan_channel);
            }
        }
    }

    if (param->use_specified_op_chan) {
        /* Used the specified Operating Channel */
        p2pbuf_add_operating_channel(ies, prot_handle->country,
                                     param->op_chan_op_class,
                                     param->op_chan_chan_num);

        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
            "%s: Use the specified Op chan: class=%d, num=%d\n",
            __func__, param->op_chan_op_class, param->op_chan_chan_num);
    }
    else {
        if ((op_chan_reg_class == 0) || (op_chan_channel == 0))
        {
            /* Invalid Op channel */
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: Error: Empty Op Chan. Skip this sub-attribute.\n", __func__);
        }
        else {
            p2pbuf_add_operating_channel(ies, prot_handle->country, op_chan_reg_class, op_chan_channel);
        }
    }

    if (param->use_grp_bssid) {
        p2pbuf_add_group_bssid(ies, param->grp_bssid);
    }

    if (start_new_group) {
        if (op_chan_is_restricted) {
            /* Use the single channel list based on the channel restriction */
            if ((op_chan_reg_class != 0) && (op_chan_channel != 0))
            {
                int retval;

                /* Add the channel list sub-attribute that has one channel */
                retval = p2p_prot_add_single_channel_list(prot_handle, ies, op_chan_reg_class, op_chan_channel);
                if (retval != EOK) {
                    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                        "%s: Error: p2p_prot_add_single_channel_list ret 0x%x.\n", __func__, retval);
                    return retval;
                }
            }
            else {
                IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                    "%s: Error: Empty Op Chan. Skip this sub-attribute.\n", __func__);
            }
        }
        else {
            /* Add our current hardware channel list */
            p2pbuf_add_channel_list(ies, prot_handle->country, &prot_handle->chanlist);
        }
    }
    else {
        /* Add our current hardware channel list */
        p2pbuf_add_channel_list(ies, prot_handle->country, &prot_handle->chanlist);
    }

    p2pbuf_add_group_id(ies, param->grp_id.device_addr,
                        param->grp_id.ssid.ssid,
                        param->grp_id.ssid.len);

    p2pbuf_add_device_info(ies, prot_handle->vap->iv_myaddr, 
                           (u_int8_t *)prot_handle->p2p_dev_name, prot_handle->p2p_dev_name_len, 
                           prot_handle->configMethods, prot_handle->primaryDeviceType,
                           prot_handle->numSecDeviceTypes, prot_handle->secondaryDeviceTypes);

    spin_unlock(&(prot_handle->set_param_lock));

    p2pbuf_update_ie_hdr(ies, ielen_ptr, &ie_len);  /* last thing to do is update the IE length */

    /* Note: we assumed the WPS IE with Device Password ID attribute is in the additional IE */
    /* Add the IE's from the OS */
    if (tx_fr_info->in_ie_buf_len > 0) {
        // Copy the IE buffer.
        u_int8_t        *src_buf = &(((u_int8_t *)tx_fr_info)[tx_fr_info->in_ie_offset]);

        p2pbuf_put_data(ies, src_buf, tx_fr_info->in_ie_buf_len);
    }
    /* calculate the entire frame length */
    tx_fr_info->frame_len = p2pbuf_get_bufptr(ies) - tx_fr_info->frame_buf;

    {
        char    tmpbuf[IEEE80211_NWID_LEN + 1];
    
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
            "%s: we_are_GO=%d. start_new_group=%d, ssid=%s, "
            "min_config_go_timeout=%d, min_config_client_timeout=%d\n",
            __func__, we_are_GO, start_new_group,
            get_str_ssid(tmpbuf, param->grp_id.ssid.ssid, param->grp_id.ssid.len),
            param->min_config_go_timeout, param->min_config_client_timeout);
    }

    return EOK;
}

/*
 * Build the payload for an INVITATION RESPONSE frame.
 * The created payload is in tx_fr_info->frame_buf and length is tx_fr_info->frame_len.
 */
static int p2p_prot_build_invit_resp_frame(
    wlan_p2p_prot_t prot_handle, 
    tx_event_info   *tx_fr_info)
{
    p2pbuf                                  p2p_buf, *ies;
    u_int8_t                                *frm;
    int                                     buf_len;
    int                                     ie_len;
    u_int8_t                                *ielen_ptr;
    u_int8_t                                go_intent;
    wlan_p2p_prot_tx_invit_resp_param       *param;
    bool                                    we_are_client, start_new_group;
    bool                                    op_chan_is_restricted;
    u8                                      op_chan_reg_class = 0;
    u8                                      op_chan_channel = 0;

    param = &tx_fr_info->orig_req_param.invit_resp;

    ies = &p2p_buf;
    frm = tx_fr_info->frame_buf;
    buf_len = IEEE80211_RTS_MAX;
    p2pbuf_init(ies, frm, buf_len);

    p2pbuf_add_public_action_hdr(ies, P2P_INVITATION_RESP, tx_fr_info->dialog_token);

    ielen_ptr = p2pbuf_add_ie_hdr(ies);

    spin_lock(&(prot_handle->set_param_lock));  /* mutual exclusion with OS updating the parameters */

    ASSERT(tx_fr_info->previous_frtype == IEEE80211_P2P_PROT_FTYPE_INVIT_REQ);

    if (tx_fr_info->prev_fr_we_are_GO) {
        /* Since the sender of previous frame is GO, we must be client */
        we_are_client = true;
    }
    else {
        /* The sender of previous frame is client */
        if (tx_fr_info->prev_fr_start_new_group) {
            we_are_client = false;  /* We are the GO in this new group */
        }
        else {
            we_are_client = true;
        }
    }

    if (((param->status == P2P_SC_SUCCESS) || (param->status == P2P_SC_FAIL_INFO_CURRENTLY_UNAVAILABLE))
        && (!param->request_fr_parsed_ie)) {
        /* OS returned status is OK but there are parsing error in previous frame */
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
            "%s: Note: OS returned status is OK but there are parsing error in previous frame.\n"
            "Change status to INCOMPATIBLE_PARAMS\n", __func__);

        param->status = P2P_SC_FAIL_INCOMPATIBLE_PARAMS;
    }

    op_chan_is_restricted = false;

    if ((param->status == P2P_SC_SUCCESS) || (param->status == P2P_SC_FAIL_INFO_CURRENTLY_UNAVAILABLE))
    {
        /* Check the channel list */
        ASSERT(tx_fr_info->rx_valid_chan_list);

        if (tx_fr_info->intersected_chan_list.reg_classes == 0) {
            /* No common channels */
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                                 "%s: Change status from SUCCESS to FAIL_NO_COMMON_CHANNELS."
                                 " rx_valid_chan_list=%d, no reg_classes=%d\n",
                                 __func__, tx_fr_info->rx_valid_chan_list,
                                 tx_fr_info->intersected_chan_list.reg_classes);
            op_chan_reg_class = 0;
            op_chan_channel = 0;
            param->status = P2P_SC_FAIL_NO_COMMON_CHANNELS;
        }
        else {
            /* Check if we have channel restrictions */
            bool        have_preferred_op_chan = false;
            u_int32_t   op_chan_freq = 0;

            /* Check if we can find an Operation Channel from the intersected channel list */
            op_chan_is_restricted = p2p_prot_is_op_chan_restricted(prot_handle, &tx_fr_info->intersected_chan_list,
                                                                   &have_preferred_op_chan, &op_chan_freq,
                                                                   &op_chan_reg_class, &op_chan_channel);
            if (op_chan_is_restricted) {
                /* Op channel is restricted */
                if ((op_chan_freq == 0) ||
                    (op_chan_reg_class == 0) || (op_chan_channel == 0))
                {
                    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                        "%s: Error: Op Chan restricted and no valid chan. freq=%d, reg_class=%d, chan=%d\n",
                        __func__, op_chan_freq, op_chan_reg_class, op_chan_channel);

                    op_chan_reg_class = 0;
                    op_chan_channel = 0;
                    param->status = P2P_SC_FAIL_NO_COMMON_CHANNELS;
                }
                else {
                    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                        "%s: Note: Op Chan restricted. freq=%d, reg_class=%d, chan=%d\n",
                        __func__, op_chan_freq, op_chan_reg_class, op_chan_channel);

                    if (!tx_fr_info->prev_fr_start_new_group) {
                        /* Joining an existing group. Make sure that the received Op channel is at the
                         * same frequency as our restricted channel. */
                        int     rx_op_chan_freq;
                        ASSERT(param->req_have_op_chan == true);

                        rx_op_chan_freq = wlan_p2p_channel_to_freq((const char *)param->rx_req_op_chan,       /* country code */
                                                                   param->rx_req_op_chan[3],    /* reg class */
                                                                   param->rx_req_op_chan[4]);   /* channel number */
                        if (rx_op_chan_freq <= 0) {
                            /* Error */
                            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                                "%s: Error: wlan_p2p_channel_to_freq failed. Set status to INCOMP_PARAMS. reg_class=%d, chan=%d, country=%x %x %x\n",
                                __func__, param->rx_req_op_chan[3], param->rx_req_op_chan[4],
                                param->rx_req_op_chan[0], param->rx_req_op_chan[1], param->rx_req_op_chan[2]);

                            param->status = P2P_SC_FAIL_INCOMPATIBLE_PARAMS;
                        }
                        else {
                            if (op_chan_freq != rx_op_chan_freq) {
                                /* Different channel supported */
                                IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                                    "%s: Error: Set status to NO_COMM_CHAN. rx op chan freq(%d) != our own restricted freq(%d)\n",
                                    __func__, rx_op_chan_freq, op_chan_freq);
                                param->status = P2P_SC_FAIL_NO_COMMON_CHANNELS;
                            }
                            else {
                                /* We can support this */
                            }
                        }
                    }
                }
            }
            else {
                /* Else No restrictions on Op channel */
                if (have_preferred_op_chan) {
                    /* Op channel is not restricted but have preferred Op chan */
                    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                        "%s: No op chan restriction but have preferred reg_class=%d, chan=%d.\n",
                        __func__, op_chan_reg_class, op_chan_channel);
                }
                else {
                    /* No op channel restriction, check if we can use our default Op channel */
                    int     retval;
                    retval = wlan_p2p_channels_includes(&tx_fr_info->intersected_chan_list,
                                                        prot_handle->op_chan.reg_class,
                                                        prot_handle->op_chan.ieee_num);
                    if (retval == 0) {
                        /* Our desired Op chan is not found in the intersected list */
                        u8      new_reg_class = 0, new_chan = 0;

                        reselect_op_chan(&tx_fr_info->intersected_chan_list, &new_reg_class, &new_chan);
                        ASSERT((new_reg_class != 0) && (new_chan != 0));

                        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                                             "%s: No chan restrictions but reselected op channel. regclass=%d chan=%d.\n",
                                             __func__, new_reg_class, new_chan);

                        op_chan_reg_class = new_reg_class;
                        op_chan_channel = new_chan;
                    }
                    else {
                        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                                             "%s: No chan restrictions: op channel unchanged. regclass=%d chan=%d.\n",
                                             __func__, prot_handle->op_chan.reg_class, prot_handle->op_chan.ieee_num);

                        op_chan_reg_class = prot_handle->op_chan.reg_class;
                        op_chan_channel = prot_handle->op_chan.ieee_num;
                    }
                }

                if (!tx_fr_info->prev_fr_start_new_group) {
                    /* Joining an existing group. Make sure that the received Op channel is supported. */
                    int     rx_op_chan_freq;
                    int     retval;
                    ASSERT(param->req_have_op_chan == true);

                    retval = wlan_p2p_channels_includes(&tx_fr_info->intersected_chan_list,
                                                        param->rx_req_op_chan[3],
                                                        param->rx_req_op_chan[4]);
                    if (retval == 1) {
                        /* received op channel is within our intersected chan list */
                        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                            "%s: rx op chan is in intersected chan list. reg_class=%d, chan=%d\n",
                            __func__, param->rx_req_op_chan[3], param->rx_req_op_chan[4]);
                    }
                    else {
                        /* Error: received op channel not within our intersected chan list */
                        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                            "%s: Error: rx op chan not in intersected chan list. Set status to INCOMP_PARAMS. reg_class=%d, chan=%d\n",
                            __func__, param->rx_req_op_chan[3], param->rx_req_op_chan[4]);

                        param->status = P2P_SC_FAIL_NO_COMMON_CHANNELS;
                    }
                }
            }
        }
    }

    /* Status attribute ID */
    if (param->status == P2P_SC_FAIL_INFO_CURRENTLY_UNAVAILABLE) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                             "%s: Note: status is P2P_SC_FAIL_INFO_CURRENTLY_UNAVAILABLE.\n",
                             __func__, param->status);
    }
    else if (param->status != P2P_SC_SUCCESS) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                             "%s: Note: status is non-success=%d.\n",
                             __func__, param->status);
    }
    p2pbuf_add_status(ies, param->status);

    p2pbuf_add_config_timeout(ies, param->min_config_go_timeout, param->min_config_client_timeout);

    if (param->use_specified_op_chan) {
        /* Used the specified Operating Channel */
        p2pbuf_add_operating_channel(ies, prot_handle->country,
                                     param->op_chan_op_class,
                                     param->op_chan_chan_num);

        if (param->status == P2P_SC_FAIL_NO_COMMON_CHANNELS) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                                 "%s: Warn: use_specified_op_chan but status is FAIL_NO_COMMON_CHANNELS. regclass=%d chan=%d.\n",
                                 __func__, param->op_chan_op_class, param->op_chan_chan_num);
        }
    }
    else {
        /* Check if we need to add our own op channel */
        if (!we_are_client) {
            /* We are the GO, we need to give an Op channel */
            if ((op_chan_reg_class != 0) && (op_chan_channel != 0)) {
                p2pbuf_add_operating_channel(ies, prot_handle->country,
                                             op_chan_reg_class,
                                             op_chan_channel);
                IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                     "%s: We are GO, use_specified_op_chan is false. Add own Op chan. regclass=%d chan=%d.\n",
                     __func__, op_chan_reg_class, op_chan_channel);
            }
            else if (param->status == P2P_SC_SUCCESS) {
                /* Error */
                IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                     "%s: Error: we are GO, use_specified_op_chan is false, status is OK, but no valid Op chan.\n",
                     __func__);
            }
        }
    }

    if (param->use_grp_bssid) {
        p2pbuf_add_group_bssid(ies, param->grp_bssid);
    }
    else {
        /* If we are GO, then this field is mandatory */
        if ((!we_are_client) && (param->status == P2P_SC_SUCCESS)) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                 "%s: Error: we are GO, status is OK but use_grp_bssid is false.\n",
                 __func__);
        }
    }

    if ((param->status == P2P_SC_SUCCESS) || (param->status == P2P_SC_FAIL_INFO_CURRENTLY_UNAVAILABLE))
    {
        ASSERT(tx_fr_info->intersected_chan_list.reg_classes != 0);

        if (op_chan_is_restricted)
        {
            int retval;

            ASSERT(op_chan_reg_class != 0);
            ASSERT(op_chan_channel != 0);

            /* Add the channel list sub-attribute that has one channel */
            retval = p2p_prot_add_single_channel_list(prot_handle, ies, op_chan_reg_class, op_chan_channel);
            if (retval != EOK) {
                IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                    "%s: Error: p2p_prot_add_single_channel_list ret 0x%x.\n", __func__, retval);
                return retval;
            }
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                "%s: Add the single restricted channel as chan list. reg_class=%d, chan=%d\n",
                __func__, op_chan_reg_class, op_chan_channel);
        }
        else {
            /* Add the intersected channel list */
            p2pbuf_add_channel_list(ies, prot_handle->country, &tx_fr_info->intersected_chan_list);

            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                "%s: Add the intersected channel list as chan list. num reg_classes=%d\n",
                __func__, tx_fr_info->intersected_chan_list.reg_classes);
        }
    }

    spin_unlock(&(prot_handle->set_param_lock));

    p2pbuf_update_ie_hdr(ies, ielen_ptr, &ie_len);  /* last thing to do is update the IE length */

    /* Note: we assumed the WPS IE with Device Password ID attribute is in the additional IE */
    /* Add the IE's from the OS */
    if (tx_fr_info->in_ie_buf_len > 0) {
        // Copy the IE buffer.
        u_int8_t        *src_buf = &(((u_int8_t *)tx_fr_info)[tx_fr_info->in_ie_offset]);

        p2pbuf_put_data(ies, src_buf, tx_fr_info->in_ie_buf_len);
    }
    /* calculate the entire frame length */
    tx_fr_info->frame_len = p2pbuf_get_bufptr(ies) - tx_fr_info->frame_buf;

    return EOK;
}

/********************Start of common routines to process events ********************/

/*
 * This routine is called to complete all outstanding Tx Request. But those
 * requests that are send to UMAC and still awaiting their completion callback
 * will not be completed now. Instead we will wait for their completion callback
 * before completion this tx requests back to the caller.
 */
static void
complete_outstanding_tx_req(wlan_p2p_prot_t prot_handle)
{
    if (prot_handle->retry_tx_info) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
            "%s: Going down but has pending tx action frame retries.\n", __func__);

        complete_tx_action_frame_event(prot_handle, prot_handle->retry_tx_info, -EIO);
        prot_handle->retry_tx_info = NULL;
    }

    if (prot_handle->curr_tx_info) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
            "%s: Going down but has pending tx action frame request.\n", __func__);

        complete_tx_action_frame_event(prot_handle, prot_handle->curr_tx_info, -EIO);
        prot_handle->curr_tx_info = NULL;
    }

    /* Check if there is a minifind discovery scan going on. */
    if (prot_handle->curr_disc_scan_info != NULL) {
        if (prot_handle->curr_disc_scan_info->is_minifind) {
            /* We have a minifind */
            ASSERT(prot_handle->curr_disc_scan_info->minifind.tx_fr_request != NULL);

            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: Going down but has pending minifind and tx action frame request.\n", __func__);

            p2p_disc_scan_abort(prot_handle);
        }
    }

    if (prot_handle->num_outstanding_tx > 0) {
        /* We have outstanding Transmit Action frame request */
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
            "%s: Note: num_outstanding_tx=%d. We will complete these request when tx completes\n",
            __func__, prot_handle->num_outstanding_tx);
    }

    /* Clear current Tx Request ID so that subsequent TX_COMPLETEs will not retry */
    prot_handle->curr_tx_req_id = 0;
}

/*
 * Common routine to set the Transmit Action Frame Request.
 * If there is a prior request, then it will be completed with error.
 * If there is any errors in preparing this current request, then this tx request
 * will be completed.
 */
static int
setup_tx_action_frame_request(
    wlan_p2p_prot_t prot_handle, 
    u_int16_t       event_type, 
    tx_event_info   *tx_fr_info)
{
    sm_event_info           *event;
    wlan_scan_entry_t       peer;
    int                     retval = EOK;
    u_int8_t                *peer_addr;

    /* First, check for the previous Transmit request */
    if (prot_handle->curr_tx_info) {
        /* Complete this request with error */
        tx_event_info   *old_tx_request;

        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
             "%s: Warning: Two Tx request. Failed the first one (%pK) with error.\n",
             __func__, prot_handle->curr_tx_info);

        old_tx_request = prot_handle->curr_tx_info;
        prot_handle->curr_tx_info = NULL;
        complete_tx_action_frame_event(prot_handle, old_tx_request, -EIO);
    }
    ASSERT(prot_handle->retry_tx_info == NULL);

    //STNG TODO: you can get peer_addr from tx_fr_info->peer_addr

    //create the action frame
    switch (tx_fr_info->orig_req_param.frtype) {
    case IEEE80211_P2P_PROT_FTYPE_GO_NEG_REQ:
        retval = p2p_prot_build_group_neg_req_frame(prot_handle, tx_fr_info);
        peer_addr = tx_fr_info->orig_req_param.go_neg_req.peer_addr;
        break;
    case IEEE80211_P2P_PROT_FTYPE_GO_NEG_RESP:
        retval = p2p_prot_build_group_neg_resp_frame(prot_handle, tx_fr_info);
        peer_addr = tx_fr_info->orig_req_param.go_neg_resp.peer_addr;
        break;
    case IEEE80211_P2P_PROT_FTYPE_GO_NEG_CONF:
        retval = p2p_prot_build_group_neg_conf_frame(prot_handle, tx_fr_info);
        peer_addr = tx_fr_info->orig_req_param.go_neg_conf.peer_addr;
        break;
    case IEEE80211_P2P_PROT_FTYPE_PROV_DISC_REQ:
        retval = p2p_prot_build_prov_disc_req_frame(prot_handle, tx_fr_info);
        peer_addr = tx_fr_info->orig_req_param.prov_disc_req.peer_addr;
        break;
    case IEEE80211_P2P_PROT_FTYPE_PROV_DISC_RESP:
        retval = p2p_prot_build_prov_disc_resp_frame(prot_handle, tx_fr_info);
        peer_addr = tx_fr_info->orig_req_param.prov_disc_resp.rx_addr;
        break;
    case IEEE80211_P2P_PROT_FTYPE_INVIT_REQ:
        retval = p2p_prot_build_invit_req_frame(prot_handle, tx_fr_info);
        peer_addr = tx_fr_info->orig_req_param.invit_req.peer_addr;
        break;
    case IEEE80211_P2P_PROT_FTYPE_INVIT_RESP:
        retval = p2p_prot_build_invit_resp_frame(prot_handle, tx_fr_info);
        peer_addr = tx_fr_info->orig_req_param.invit_resp.rx_addr;
        break;
    default:
        ASSERT(false);
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
            "%s: Error: Unsupported frame type=%d. Abort this tx\n", 
            __func__, tx_fr_info->orig_req_param.frtype);
        return -EIO;
    }

    if (retval != 0) {
        /* Error in building the frame itself */
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
            "%s: Error=0x%x when building the tx frame (type=%d). Abort this tx\n", 
            __func__, retval, tx_fr_info->orig_req_param.frtype);
        return retval;
    }

    //STNG TODO: can remove peer_addr and use tx_fr_info->peer_addr
    ASSERT(OS_MEMCMP(peer_addr, tx_fr_info->peer_addr, 6) == 0);

    ASSERT(prot_handle == tx_fr_info->orig_req_param.prot_handle);
    /* Scheduled the timeout for this event. Create an unique ID to associate this timer with this request */
    ASSERT(tx_fr_info->timeout != 0);
    prot_handle->curr_tx_req_id = CREATE_UNIQUE_TIMER_ID(tx_fr_info->dialog_token, 
                                                    tx_fr_info->orig_req_param.frtype, 
                                                    CREATE_ADDR_HASH(peer_addr));
    /* When we assign a tx_req_id to this Tx request, we will increment
     * the num_outstanding_tx count */
    tx_fr_info->tx_req_id = prot_handle->curr_tx_req_id;
    prot_handle->num_outstanding_tx++;

    /* Calc. the IE offset and Length */
    tx_fr_info->out_ie_offset = offsetof(tx_event_info, frame_buf) + IEEE80211_P2P_ACTION_FR_IE_OFFSET;
    ASSERT(tx_fr_info->frame_len >= IEEE80211_P2P_ACTION_FR_IE_OFFSET);
    if (tx_fr_info->frame_len >= IEEE80211_P2P_ACTION_FR_IE_OFFSET) {
        tx_fr_info->out_ie_buf_len = tx_fr_info->frame_len - IEEE80211_P2P_ACTION_FR_IE_OFFSET;
    }

    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
        "%s: tx action frame to " MAC_ADDR_FMT " on freq=%d, payload_len=%d, id=0x%x, ie_len=%d, ie_offset=%d\n", 
        __func__, SPLIT_MAC_ADDR(peer_addr),
        tx_fr_info->tx_freq, tx_fr_info->frame_len, prot_handle->curr_tx_req_id,
        tx_fr_info->out_ie_buf_len, tx_fr_info->out_ie_offset);

    /* Init. the number of retries */
    prot_handle->num_tx_frame_retries = DEF_NUM_TX_ACTION_RETRY;

    /* Schedule a remain_on_channel at this peer's listen channel */
    ASSERT(tx_fr_info->tx_freq != 0);
    prot_handle->on_chan_dest_freq = tx_fr_info->tx_freq;

    prot_handle->curr_tx_nextstate = P2P_DISC_STATE_PRE_LISTEN;
    p2p_disc_timer_start(prot_handle, TIMER_TX_REQ_EXPIRE, tx_fr_info->timeout);

    return 0;
}

/*
 * Common routine to process the event to transmit an action frame.
 * This routine is called when we are not on the correct channel and
 * needs to reschedule a ONCHAN. But we know which channel to send this
 * action frame.
 * If there is any errors, then this tx request will be completed.
 */
static void
proc_event_tx_frame(
    wlan_p2p_prot_t             prot_handle, 
    u_int16_t                   event_type, 
    tx_event_info               *tx_fr_info,
    ieee80211_connection_state  *newstate)
{
    sm_event_info           *event;
    wlan_scan_entry_t       peer;
    u_int32_t               timeout = 0;
    int                     retval;

    /* We should know which channel this peer is on */
    ASSERT(tx_fr_info->need_mini_find == false);

    /* Setup this request */
    retval = setup_tx_action_frame_request(prot_handle, event_type, tx_fr_info);
    if (retval != 0) {
        /* Some error in setup. Abort this transmit */
        complete_tx_action_frame_event(prot_handle, tx_fr_info, retval);
        return;
    }

    if ((event_type != P2P_DISC_EVENT_TX_GO_NEG_REQ) && 
        (event_type != P2P_DISC_EVENT_TX_PROV_DISC_REQ) &&
        (event_type != P2P_DISC_EVENT_TX_INVITE_REQ))
    {
        /* Note: only Request frames have enough time to switch channel */
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                             "%s: Note: got %s but not on channel yet. May not have enough time to switch but try anyway.\n",
                             __func__, event_name(event_type));
    }

    /* Store the event parameter */
    ASSERT(prot_handle->curr_tx_info == NULL);
    prot_handle->curr_tx_info = tx_fr_info;
    ASSERT(tx_fr_info->tx_freq != 0);

    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                         "%s: schedule on_channel. type=%s, chan=%d.\n",
                         __func__, event_name(event_type), prot_handle->on_chan_dest_freq);

    /* Request to goto ONCHAN state (via the PRE_ONCHAN state) */
    ASSERT(*newstate == P2P_DISC_STATE_OFF);
    *newstate = P2P_DISC_STATE_PRE_ONCHAN;
}

/*
 * Process the P2P_DISC_EVENT_TX_COMPLETE event when no retries are wanted.
 */
static void
proc_event_tx_complete_no_retry(wlan_p2p_prot_t prot_handle, sm_event_info *event)
{
    ASSERT(event->event_type == P2P_DISC_EVENT_TX_COMPLETE);
    ASSERT(event->tx_complete.orig_tx_param != NULL);

    /* Just complete it */
    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
        "%s: got event %s. status=0x%x, Just complete this tx.\n",
        __func__, event_name(event->event_type), event->tx_complete.status);

    complete_tx_action_frame_event(prot_handle, event->tx_complete.orig_tx_param, event->tx_complete.status);
    event->tx_complete.orig_tx_param = NULL;
}

/*
 * Process the P2P_DISC_EVENT_TX_COMPLETE event. Used in states IDLE, PRE_LISTEN, PRE_ONCHAN and DISCSCAN.
 */
static void
proc_event_tx_complete(wlan_p2p_prot_t prot_handle, sm_event_info *event, ieee80211_connection_state *newstate)
{
    tx_event_info   *tx_event;

    tx_event = event->tx_complete.orig_tx_param;
    if ((event->tx_complete.status != EOK) &&
        (prot_handle->curr_tx_req_id == tx_event->tx_req_id) &&
        (prot_handle->num_tx_frame_retries > 0))
    {
       /* Since this is the latest request and failed, we retry this tx */
        ASSERT(prot_handle->retry_tx_info == NULL);

        prot_handle->num_tx_frame_retries--;
        prot_handle->retry_tx_info = tx_event;
        event->tx_complete.orig_tx_param = NULL;

        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
            "%s: err tx status=0x%x. num_tx_frame_retries=%d, Retry.\n",
            __func__, event->tx_complete.status, prot_handle->num_tx_frame_retries);

        if (prot_handle->curr_tx_nextstate == P2P_DISC_STATE_PRE_ONCHAN) {
            prot_handle->on_chan_dest_freq = tx_event->tx_freq;
            ASSERT(*newstate == P2P_DISC_STATE_OFF);
            *newstate = P2P_DISC_STATE_PRE_ONCHAN;
        }
        else {
            ASSERT(prot_handle->curr_tx_nextstate == P2P_DISC_STATE_PRE_LISTEN);
            ASSERT(*newstate == P2P_DISC_STATE_OFF);
            *newstate = P2P_DISC_STATE_PRE_LISTEN;
        }
    }
    else {
        /* Since this is not the latest request or success, we will not retry */
        proc_event_tx_complete_no_retry(prot_handle, event);
    }
}

static void
dispatch_disc_scan_req(wlan_p2p_prot_t prot_handle, disc_scan_event_info *req_param)
{
    sm_event_info           sm_event;

    OS_MEMZERO(&sm_event, sizeof(sm_event));
    sm_event.event_type = P2P_DISC_EVENT_DISC_SCAN_REQ;

    /* Note: the state handler has to free this allocated memory */
    sm_event.disc_scan_info = req_param;

    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
        "%s: posting event P2P_DISC_EVENT_DISC_SCAN_REQ.\n", __func__);

    ieee80211_sm_dispatch(prot_handle->hsm_handle, P2P_DISC_EVENT_DISC_SCAN_REQ,
                          sizeof(sm_event_info), &sm_event);
}

/* Reset the discovery scan parameters */
static void
reset_disc_scan_params(wlan_p2p_prot_t            prot_handle,
                       disc_scan_event_info       *disc_scan)
{
    int                             i;
    wlan_p2p_prot_disc_dev_filter   *dev_filter;

    /* If the discovery type is "FIND DEVICE", then switch to AUTO */
    if (disc_scan->orig_req_param.disc_type == wlan_p2p_prot_disc_type_find_dev)
    {
        disc_scan->orig_req_param.disc_type = wlan_p2p_prot_disc_type_auto;
    }
    disc_scan->desired_peer_found = false;
    disc_scan->desired_peer_freq = 0;

    dev_filter = (wlan_p2p_prot_disc_dev_filter *)
                    &(((u_int8_t *)disc_scan)[disc_scan->in_dev_filter_list_offset]);

    for (i = 0; i < disc_scan->in_dev_filter_list_num; i++)
    {
        dev_filter[i].discovered = false;
        #if DBG
        ASSERT(dev_filter[i].dbg_mem_marker == WLAN_P2P_PROT_DBG_MEM_MARKER);
        #endif //DBG
    }
}

/*
 * Routine to complete the Device Discovery Scan request, indicate the completion to caller and cleanup.
 */
static void
complete_disc_scan_req_event(wlan_p2p_prot_t            prot_handle, 
                             disc_scan_event_info       *disc_scan_info, 
                             int                        status,
                             ieee80211_connection_state *newstate)
{
    clear_desired_peer(prot_handle);

    if (disc_scan_info->is_minifind) {
        /* Since this discovery scan is due to a minifind, complete the Tx frame request instead. */
        ASSERT(prot_handle->def_probereq_ie_removed == false);
        complete_minifind_tx_fr_req_event(prot_handle, disc_scan_info, status, newstate);
    }
    else {
        wlan_p2p_prot_event     *ind_event;

        if (prot_handle->def_probereq_ie_removed) {
            /* Restore the default probe request additional IE that was removed at
             * the start of this discovery scan. */
            int     retval;

            ASSERT(disc_scan_info->remove_add_ie == true);

            prot_handle->def_probereq_ie_removed = false;
            retval = wlan_mlme_set_appie(prot_handle->vap, IEEE80211_FRAME_TYPE_PROBEREQ,
                                         prot_handle->probe_req_add_ie, prot_handle->probe_req_add_ie_len);
            if (retval != EOK) {
                IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                                     "%s: Error: wlan_mlme_set_appie(IEEE80211_FRAME_TYPE_PROBEREQ) ret=0x%x\n",
                                     __func__, retval);
            }
            else {
                IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                                     "%s: restore the default ProbeReq IE\n",
                                     __func__);
            }
        }

        /* If this is a directed discovery scan, then make sure that we have
         * found the desired peer. Otherwise, we should do another discovery
         * scan if there is sufficient time left.
         */
        do {
            u_int32_t   time_elapsed_msec;

            if (status != EOK) {
                /* Some error occured during this discovery scan */
                break;  /* continue to complete this call */
            }

            if ((disc_scan_info->in_stop_when_discovered) &&
                (disc_scan_info->desired_peer_found))
            {
                /* A targetted scan and the desired peer(s) are found. */
                break;
            }

            /* get current time and calc. elapsed time. */
            time_elapsed_msec = CONVERT_SYSTEM_TIME_TO_MS(OS_GET_TIMESTAMP() - disc_scan_info->time_start);

            if (disc_scan_info->in_stop_when_discovered)
            {
                /* A targetted scan. */
                if (disc_scan_info->orig_req_param.timeout < 
                    (time_elapsed_msec + P2P_PROT_MIN_TIME_TO_FIND_PEER))
                {
                    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                        "%s: not sure if desired peer is found but anyway, no time(%d) for retry.\n",
                        __func__, time_elapsed_msec);
                    break;  /* continue to complete this call */
                }
                /* Else we do another scan */
            }
            else {
                /* A general discovery scan. Make sure that we have spend enough time scanning.
                 * Otherwise, we may spend too little time scanning and did not discover some devices.
                 * Eg. a single band device will take only 1.5 seconds to complete a full scan which
                 * is not enough time to discover all the P2P devices. */
                if (time_elapsed_msec < P2P_PROT_MIN_DURATION_FOR_FULLSCAN)
                {
                    if (disc_scan_info->orig_req_param.timeout < 
                        (time_elapsed_msec + P2P_PROT_MIN_TIME_TO_FIND_PEER))
                    {
                        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                            "%s: Did not spend enough time scanning but anyway, no time(%d) for retry.\n",
                            __func__, time_elapsed_msec);
                        break;  /* continue to complete this call */
                    }
                    /* Else we do another scan */
                }
                else {
                    break;  /* continue to complete this call */
                }
            }

            /* Start another discovery */

            /* Reset the discovery scan parameters */
            reset_disc_scan_params(prot_handle, disc_scan_info);

            /* Adjust the timeout value down. */
            ASSERT(disc_scan_info->orig_req_param.timeout >= time_elapsed_msec);
            disc_scan_info->curr_timeout = disc_scan_info->orig_req_param.timeout - time_elapsed_msec;

            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                "%s: desired peer not found or not enough time scanning. Start another scan. time_elapsed_msec=%d, orig timeout=%d, curr timeout=%d\n",
                __func__, time_elapsed_msec, disc_scan_info->orig_req_param.timeout, disc_scan_info->curr_timeout);

            /* Dispatch a new Discovery scan but reusing the existing param struc */
            dispatch_disc_scan_req(prot_handle, disc_scan_info);

            return;

        } while ( false );

        ind_event = &(disc_scan_info->cb_event_param);
        ind_event->event_type = IEEE80211_P2P_PROT_EVENT_DISC_SCAN_COMPL;
        ind_event->disc_scan_compl.status = status;

        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
            "%s: called. status=0x%x, called event handler with event=IEEE80211_P2P_PROT_EVENT_DISC_SCAN_COMPL\n",
            __func__, status);

        prot_handle->ev_handler(prot_handle->event_arg, ind_event);
    }

    /* We can free this discovery scan request structure now */
    OS_FREE(disc_scan_info);
}

struct find_device_param_listen_freq {
    wlan_p2p_prot_t prot_handle;
    u_int8_t        mac_addr[IEEE80211_ADDR_LEN];

    /* output */
    bool            match_found;
    u_int32_t       listen_freq;
};

/*
 * Callback function to iterate the Scan list based on mac addresss to
 * find the p2p device. 
 * Called by wlan_scan_macaddr_iterate().
 * Returns EOK if not found. 
 * Return -EALREADY if a p2p device is found and its listen frequency returns.
 */
static int
p2p_disc_find_device_listen_freq(void *ctx, wlan_scan_entry_t entry)
{
    struct find_device_param_listen_freq    *param;
    u_int8_t                                *entry_mac_addr;
    int                                     retval;
    ieee80211_scan_p2p_info                 scan_p2p_info;

    param = (struct find_device_param_listen_freq *)ctx;
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
                     "%s: scan_p2p_info.is_p2p=%d, param->listen_freq=%d, ic_freq=%d, p2p_dev_addr="MAC_ADDR_FMT"\n",
                     __func__, scan_p2p_info.is_p2p, param->listen_freq,
                     (scan_p2p_info.listen_chan? scan_p2p_info.listen_chan->ic_freq : 0),
                     SPLIT_MAC_ADDR(scan_p2p_info.p2p_dev_addr));

    if (!scan_p2p_info.is_p2p) {
        /* Not a P2P, skip this entry and go to the next entry. */
        return EOK; 
    }

    IEEE80211_DPRINTF_VB(param->prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                         "%s: Found a match in device ID address="MAC_ADDR_FMT" scan entry=0x%x.\n",
                         __func__, SPLIT_MAC_ADDR(entry_mac_addr), entry);

    if (scan_p2p_info.listen_chan) {
        param->match_found = true;
        param->listen_freq = scan_p2p_info.listen_chan->ic_freq;

        /* Return an error so that we stop iterating the list */
        return -EALREADY;
    }
    else {
        /* Error: no listen channel, skip */
        IEEE80211_DPRINTF_VB(param->prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                             "%s: Error: p2p device address="MAC_ADDR_FMT" scan entry=0x%x have no listen channel.\n",
                             __func__, SPLIT_MAC_ADDR(entry_mac_addr), entry);
        return EOK; 
    }
}

/*
 * Find the listen frequency of this P2P device entry in the scan table
 */
static bool
find_p2p_device_listen_freq(wlan_p2p_prot_t prot_handle, u_int8_t *mac_addr, u_int32_t *listen_freq)
{
    struct find_device_param_listen_freq      param;

    OS_MEMZERO(&param, sizeof(struct find_device_param_listen_freq));
    param.prot_handle = prot_handle;
    IEEE80211_ADDR_COPY(param.mac_addr, mac_addr);

    /* Look for the P2P Device but not into the GO's group info */
    wlan_scan_macaddr_iterate(prot_handle->vap, mac_addr, p2p_disc_find_device_listen_freq, &param);

    if (param.match_found) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
             "%s: Found p2p device="MAC_ADDR_FMT" and listen freq=%d\n",
             __func__, SPLIT_MAC_ADDR(mac_addr), param.listen_freq);
        *listen_freq = param.listen_freq;
        return true;
    }
    return false;
}


/*
 * Find the listen frequency of this P2P GO in the scan table
 */
static bool
find_p2p_GO_listen_freq(wlan_p2p_prot_t prot_handle, u_int8_t *mac_addr, ieee80211_ssid *grp_ssid, u_int32_t *listen_freq)
{
    ieee80211_scan_p2p_info entry_p2p_info;
    wlan_scan_entry_t       peer = NULL;
    bool                    is_found;

    is_found = find_p2p_peer(prot_handle, mac_addr, true, grp_ssid, &peer, &entry_p2p_info);

    if (is_found) {
        /* We got the listen channel to send the action frame */
        *listen_freq = entry_p2p_info.listen_chan->ic_freq;

        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
             "%s: Found p2p GO="MAC_ADDR_FMT" and listen freq=%d\n",
             __func__, SPLIT_MAC_ADDR(mac_addr), *listen_freq);

        /* No need for this scan entry */
        ieee80211_scan_entry_remove_reference(peer);
    }
    /* else did not find anything */
    return is_found;
}

/*
 * To process the event P2P_DISC_EVENT_DISC_SCAN_REQ in all states except OFF.
 * Return EOK to indicate that this new discovery scan can start. Returns error if this
 * discovery scan is rejected.
 */
static int
proc_event_disc_scan_req(wlan_p2p_prot_t            prot_handle,
                         sm_event_info              *event,
                         ieee80211_connection_state *newstate)
{
    disc_scan_event_info        *req_param;
    ieee80211_connection_state  curr_state = (ieee80211_connection_state)ieee80211_sm_get_curstate(prot_handle->hsm_handle);

    ASSERT(curr_state != P2P_DISC_STATE_OFF);
    ASSERT(event->event_type == P2P_DISC_EVENT_DISC_SCAN_REQ);
    req_param = event->disc_scan_info;

    if (prot_handle->curr_disc_scan_info) {
        /* Only one device discovery scan at one time. Fail this call. */
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
            "%s: Error: already have discovery scan request. Is current scan a minifind=%d\n",
            __func__, prot_handle->curr_disc_scan_info->is_minifind);

        /* Failed this request immediately */
        complete_disc_scan_req_event(prot_handle, event->disc_scan_info, -EIO, newstate);
        event->disc_scan_info = NULL;
        return -EINVAL;
    }
    ASSERT(!prot_handle->disc_scan_active);

    /* Set a timeout for this device discovery scan. */
    p2p_disc_timer_start(prot_handle, TIMER_DISC_SCAN_EXPIRE, req_param->curr_timeout);

    prot_handle->curr_disc_scan_info = event->disc_scan_info;

    ASSERT(prot_handle->def_probereq_ie_removed == false);
    if (req_param->remove_add_ie) {
        /* Remove the default probe request additional IE since
         * we will use the one from this discovery scan. */
        int     retval;

        prot_handle->def_probereq_ie_removed = true;
        retval = wlan_mlme_set_appie(prot_handle->vap, IEEE80211_FRAME_TYPE_PROBEREQ, NULL, 0);
        if (retval != EOK) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                                 "%s: Error: wlan_mlme_set_appie(IEEE80211_FRAME_TYPE_PROBEREQ) ret=0x%x\n",
                                 __func__, retval);
        }
        else {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                                 "%s: clear the default ProbeReq IE\n",
                                 __func__);
        }
    }

    /* store this request */
    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
        "%s: Discovery Scan request in %s state. Dev_filter_num=%d. soc_chans_only=%d, stop_when_disc=%d, Queue and do later.\n",
        __func__,
        ieee80211_sm_get_state_name(prot_handle->hsm_handle, curr_state),
        prot_handle->curr_disc_scan_info->in_dev_filter_list_num,
        prot_handle->curr_disc_scan_info->in_social_chans_only, 
        prot_handle->curr_disc_scan_info->in_stop_when_discovered);

    /* This discovery scan will stop when all the devices specified in filters are found */
    if (prot_handle->curr_disc_scan_info->in_stop_when_discovered) {
        /* Have Device Filters. */
        disc_scan_event_info    *disc_scan;
        disc_scan = prot_handle->curr_disc_scan_info;

        set_device_filters(prot_handle);

        ASSERT(disc_scan->in_dev_filter_list_num > 0);

        /* If discovery type is AUTO, then try to optimize the search */
        if (disc_scan->orig_req_param.disc_type == wlan_p2p_prot_disc_type_auto)
        {
            /* Check the scan table to see if this P2P device's listen/op channel is known */
            int                             i;
            wlan_p2p_prot_disc_dev_filter   *dev_filter;

            dev_filter = (wlan_p2p_prot_disc_dev_filter *)
                            &(((u_int8_t *)disc_scan)[disc_scan->in_dev_filter_list_offset]);
            for (i = 0; i < disc_scan->in_dev_filter_list_num; i++)
            {
                u_int32_t   listen_freq = 0;
                bool        is_found = false;

                if (dev_filter[i].bitmask == WLAN_P2P_PROT_DISC_DEV_FILTER_BITMASK_DEVICE) {
                    /* check scan table to see if we seen this p2p device */
                    is_found = find_p2p_device_listen_freq(prot_handle, dev_filter[i].device_id, &listen_freq);
                }
                else if (dev_filter[i].bitmask == WLAN_P2P_PROT_DISC_DEV_FILTER_BITMASK_GO) {
                    /* check scan table to see if we seen this GO */
                    ASSERT(!dev_filter[i].grp_ssid_is_p2p_wildcard);
                    /* Find this GO with this SSID */
                    is_found = find_p2p_GO_listen_freq(prot_handle, dev_filter[i].device_id,
                                                       &dev_filter[i].grp_ssid, &listen_freq);
                }
                else if (dev_filter[i].bitmask == 
                         (WLAN_P2P_PROT_DISC_DEV_FILTER_BITMASK_GO|WLAN_P2P_PROT_DISC_DEV_FILTER_BITMASK_DEVICE))
                {
                    is_found = find_p2p_device_listen_freq(prot_handle, dev_filter[i].device_id, &listen_freq);
                    if (!is_found) {
                        is_found = find_p2p_GO_listen_freq(prot_handle, dev_filter[i].device_id,
                                                           &dev_filter[i].grp_ssid, &listen_freq);
                    }
                }

                if ((!is_found) || (listen_freq == 0)) {
                    /* We do not know where the peer is. We do a generic discovery scan. */
                    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                                         "%s: did not find peer " MAC_ADDR_FMT " not in scan table. listen_freq=%d\n",
                                         __func__, SPLIT_MAC_ADDR(dev_filter[i].device_id), listen_freq);
                }
                else {
                    /* We got the listen channel to send the action frame */
                    disc_scan->desired_peer_freq = listen_freq;

                    /* Switch the disc type from AUTO to FIND_DEV */
                    disc_scan->orig_req_param.disc_type = wlan_p2p_prot_disc_type_find_dev;

                    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                                         "%s: will try to discover at dest_freq=%d for peer " MAC_ADDR_FMT ". DiscType is FIND_DEV.\n",
                                         __func__, disc_scan->desired_peer_freq, SPLIT_MAC_ADDR(dev_filter[i].device_id));

                    break;  /* get out of this for-loop */
                }
            }
        }
    }

    /* Note: If we are not IDLE state, then we will wait till we are back at IDLE state
     * before starting this scan */

    return EOK;
}

static void
proc_event_disc_scan_stop(wlan_p2p_prot_t prot_handle,
                          ieee80211_connection_state *newstate)
{
    if ((prot_handle->curr_disc_scan_info != NULL) &&
        (!prot_handle->curr_disc_scan_info->is_minifind))
    {
        /* There is an existing discovery scan and not due to minifind (tx action frame) */
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
            "%s: Stopping existing discovery scan.\n",
            __func__);

        p2p_disc_scan_abort(prot_handle);
    }
    else {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
            "%s: No outstanding disc scan to stop. prot_handle->curr_disc_scan_info=%pK\n",
            __func__, prot_handle->curr_disc_scan_info);
        if ((prot_handle->curr_disc_scan_info != NULL) &&
            (prot_handle->curr_disc_scan_info->is_minifind))
        {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                "%s: current disc scan is for minifind\n",
                __func__);
        }
    }
}

/*
 * This routine is called when the timer for Device Discovery expired.
 * We have to fail this request and indicate to the caller.
 */
static void
disc_scan_timer_expired(wlan_p2p_prot_t prot_handle)
{
    /* Timer expired while waiting for this device discovery to complete */
    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
        "%s: Timeout while waiting for device discovery to complete. Abort.\n",
        __func__);

    ASSERT(prot_handle->curr_disc_scan_info != NULL);

    p2p_disc_scan_abort(prot_handle);
}

/*
 * Request a Listen "remain_on_channel" scan.
 * Parameter duration_tu gives the duration in Time Units. If duration_tu is 0, then a
 * random value is chosen.
 */
static int
p2p_prot_request_listen_scan(wlan_p2p_prot_t prot_handle, u_int32_t duration_tu)
{
    int         retval = EOK;
    bool        in_listen_state = true;
    u_int32_t   random, tu;

    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
        "%s: called. Listen scan. My listen Freq=%d\n", 
        __func__, prot_handle->my_listen_chan->ic_freq);

    prot_handle->scan_cancel_reason = SCAN_CANCEL_REASON_NULL;

    ASSERT(!prot_handle->curr_scan.set_chan_requested);
    prot_handle->curr_scan.set_chan_requested = false;

    /* Update the P2P IE in the Probe Response frame */
    retval = create_and_add_probe_resp_ie(prot_handle);
    if (retval != EOK)
    {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
            "%s: Err: create_and_add_probe_resp_ie returns=%x\n", __func__, retval);
        /* Note: No graceful way to recover from this error */
    }

    if (duration_tu == 0) {
        /* Choose a random listen period in multiples of 100 millisecs */

        /* Calculate the listen dwell time */
        OS_GET_RANDOM_BYTES(&random, sizeof(random));
        /* tu is in terms of milliseconds. */
        tu = (random % ((prot_handle->max_disc_int - prot_handle->min_disc_int) + 1) +
              prot_handle->min_disc_int) * 100;
        tu = 1024 * tu / 1000;
    }
    else {
        tu = duration_tu;
    }

    /* Schedule a remain_on_channel for the designated channel, prot_handle->on_chan_dest */
    prot_handle->curr_scan.set_chan_req_id++;  /* Increment to get current ID */
    retval = wlan_p2p_set_channel(prot_handle->p2p_device_handle, 
                                  in_listen_state,
                                  prot_handle->my_listen_chan->ic_freq,
                                  prot_handle->curr_scan.set_chan_req_id,
                                  tu, IEEE80211_P2P_DEFAULT_SCAN_PRIORITY);
    if (retval != EOK)
    {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
            "%s: Err: wlan_p2p_set_channel ret=%d. duration=%d, duration_tu=%d, Freq=%d\n", __func__, retval, 
            tu, duration_tu, prot_handle->my_listen_chan->ic_freq);
    }
    else {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
            "%s: wlan_p2p_set_channel OK. given set_chan_req_id=%d, duration=%d, duration_tu=%d, Freq=%d\n",
            __func__, prot_handle->curr_scan.set_chan_req_id, tu, duration_tu,
            prot_handle->my_listen_chan->ic_freq);

        prot_handle->curr_scan.set_chan_requested = true;
        prot_handle->curr_scan.set_chan_duration = tu;
        prot_handle->curr_scan.set_chan_freq = prot_handle->my_listen_chan->ic_freq;
    }
    return retval;
}

/*
 * This routine is called to process the P2P_DISC_EVENT_RESET event.
 * Make sure all outstanding requests are completed.
 */
static void
proc_event_reset(wlan_p2p_prot_t prot_handle)
{
    if (prot_handle->curr_disc_scan_info) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
            "%s: Going down. Abort discovery scan request.\n", __func__);

        p2p_disc_scan_abort(prot_handle);
    }

    /* Try to complete the outstanding Tx requests */
    complete_outstanding_tx_req(prot_handle);

    if (prot_handle->is_discoverable) {
        /* Stop the Discoverability Listening */
        p2p_discoverability_listen_stop(prot_handle);
    }
}

/*
 * This routine is called to process the P2P_DISC_EVENT_END event.
 * Make sure all outstanding requests are completed.
 */
static void
proc_event_end(wlan_p2p_prot_t prot_handle)
{
    proc_event_reset(prot_handle);

    /* cancel all timers */
    p2p_disc_timer_stop_all(prot_handle);
}

/*
 * Common routine to process the P2P_DISC_EVENT_RX_MGMT event.
 */
static void
proc_event_rx_mgmt(wlan_p2p_prot_t              prot_handle, 
                   struct wlan_p2p_rx_frame     *rx_frame,
                   ieee80211_connection_state   *newstate)
{
    if (prot_handle->curr_disc_scan_info == NULL) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
            "%s: called but no current scan parameters. Ignore this matched peer frame.\n", __func__);
        return;
    }

    if (prot_handle->curr_disc_scan_info->is_minifind) {
        /* We are doing a discovery scan to do a minifind */
        tx_event_info *tx_fr_info;
        wlan_scan_entry_t scan_entry;
        ieee80211_scan_p2p_info entry_p2p_info;
        bool peer_found = false;
        ieee80211_ssid  *grp_id_ssid;

        tx_fr_info = prot_handle->curr_disc_scan_info->minifind.tx_fr_request;

        /* If there is a group ID, then make sure that the SSID matches */
        if (tx_fr_info->have_group_id) {
            grp_id_ssid = &tx_fr_info->group_id.ssid;
        }
        else {
            grp_id_ssid = NULL;
        }

        peer_found = find_p2p_peer(prot_handle, tx_fr_info->peer_addr, false, grp_id_ssid, 
                                   &scan_entry, &entry_p2p_info);

        if (peer_found) {
            disc_scan_event_info *disc_scan_info;

            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                "%s: desired device is found.\n",
                __func__);

            /* Stop the timer TIMER_DISC_SCAN_EXPIRE (may have already expired) */
            p2p_disc_timer_stop(prot_handle, TIMER_DISC_SCAN_EXPIRE);

            disc_scan_info = prot_handle->curr_disc_scan_info;
            prot_handle->curr_disc_scan_info = NULL;
            prot_handle->disc_scan_active = false;

            /* Start the transmission of this action frame. */
            post_minifind_start_tx_action(prot_handle, disc_scan_info, scan_entry, &entry_p2p_info, newstate);

            /* No need for this scan entry */
            ieee80211_scan_entry_remove_reference(scan_entry);

            /* We can free this discovery scan request structure now */
            OS_FREE(disc_scan_info);

            return;
        }
        else {
            /* Error: this should not happen since we have received a
             * suitable beacon/probe resp frame. Stop this minifind.
             */
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: Error: desired device is not found, abort minifind.\n",
                __func__);

            st_disc_scan_complete_req(prot_handle, -EBUSY, newstate);
        }

    }
    else {
        /* Else if discovery scan, then complete this request and return the results. */
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
            "%s: complete a discovery scan since desired device is found.\n",
            __func__);

        prot_handle->curr_disc_scan_info->desired_peer_found = true;

        st_disc_scan_complete_req(prot_handle, EOK, newstate);
        return;
    }

    return;
}

/*
 * Common routine in all states to process the transmit action frame event as
 * defined by CASE_ALL_TX_ACTION_FRAMES. Return 0 if OK and the state machine
 * can process this event further. Returns non zero if this event is already
 * processed.
 */
static int
p2p_prot_proc_common_tx_action_frame_event(
    wlan_p2p_prot_t prot_handle, ieee80211_connection_event event_type, sm_event_info *event)
{
    if (event_type == P2P_DISC_EVENT_TX_GO_NEG_REQ) {
        /* For GON Request frame, we have to make sure that we have a valid channel
         * to form a group */
        u_int32_t   op_chan_freq;
        bool        have_preferred_op_chan = false;
        u8          tmp_reg_class, tmp_chan_num;

        if (p2p_prot_is_op_chan_restricted(prot_handle, &prot_handle->chanlist, &have_preferred_op_chan,
                                           &op_chan_freq, &tmp_reg_class, &tmp_chan_num))
        {
            /* Op channel is restricted. Check that we can support this channel. */

            if (op_chan_freq == 0) {
                /* No valid operational channel */
                IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                                     "%s: restricted op chan unsupported in chan list. Failed this tx act frame\n",
                                     __func__);

                /* Complete this event with error */
                complete_tx_action_frame_event(prot_handle, event->tx_info, -EBUSY);
                event->tx_info = NULL;

                return -1;
            }
            else {
                IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                                     "%s: OK, restricted freq=%d and is in supported chan list.\n",
                                     __func__, op_chan_freq);
            }
        }
        /* Else no op channel restrictions */
    }

    return 0;
}

static void
p2p_prot_cancel_set_channel(wlan_p2p_prot_t prot_handle)
{
    if (prot_handle->curr_scan.set_chan_requested) {
        if (prot_handle->curr_scan.set_chan_cancelled) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                "%s: warning: already have pending cancelled set_channel request. id=%d.\n",
                __func__, prot_handle->curr_scan.set_chan_cancel_req_id);
        }
    
        /* 
         * Cancel present remain_on_channel. Note that we are not waiting
         * for the cancel to complete but wait for SCAN_DEQUEUED or SCAN_COMPLETED events.
         */
        wlan_p2p_cancel_channel(prot_handle->p2p_device_handle, IEEE80211_SCAN_CANCEL_ASYNC);
        prot_handle->curr_scan.set_chan_requested = false;
    
        prot_handle->curr_scan.set_chan_cancelled = true;
        prot_handle->curr_scan.set_chan_cancel_req_id = prot_handle->curr_scan.set_chan_req_id;
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
            "%s: Pending cancelled set_channel request. id=%d.\n",
            __func__, prot_handle->curr_scan.set_chan_cancel_req_id);
    }
}

/* 
 * Check for cancelled set_on_channel confirmation
 * Returns true if this P2P_DISC_EVENT_ON_CHAN_END event is for a previously cancelled set_on_channel.
 */
static bool
event_on_chan_end_is_from_cancel_scan(
    wlan_p2p_prot_t     prot_handle, 
    int                 req_id
    )
{
    if (prot_handle->curr_scan.set_chan_cancelled) {
        if (req_id == prot_handle->curr_scan.set_chan_cancel_req_id) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                "%s: Got ON_CHAN_END for previously cancelled set_on_channel, id=%d\n",
                __func__, req_id);
            prot_handle->curr_scan.set_chan_cancelled = false;
            return true;
        }
    }
    return false;
}

/*
 * Check if there is a pending cancelled scan or set_on_channel requests.
 */
static bool
p2p_prot_have_pending_cancel_scan(wlan_p2p_prot_t prot_handle)
{
    if (prot_handle->curr_scan.set_chan_cancelled) {
        /* We have a pending cancelled set_chan request */
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
            "%s: have pending cancelled set_on_channel, id=%d\n",
            __func__, prot_handle->curr_scan.set_chan_cancel_req_id);
        return true;
    }
    if (prot_handle->curr_scan.cancel_scan_requested) {
        /* We have a pending cancelled scan request */
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
            "%s: have pending cancelled scan, id=%d\n",
            __func__, prot_handle->curr_scan.cancel_scan_request_id);
        return true;
    }
    return false;
}

/******************** End of common routines to process events ********************/

/*
 * ****************************** State P2P_DISC_STATE_OFF ******************************
 * Description:  - Uninitialized or unattached
 */
STATE_ENTRY_FN(OFF);
STATE_EXIT_FN(OFF);

static bool
p2p_disc_state_OFF_event(void *ctx, u_int16_t eventtype, u_int16_t event_data_len, void *event_data) 
{
    wlan_p2p_prot_t             prot_handle = (wlan_p2p_prot_t)ctx;
    bool                        retval = true;
    ieee80211_connection_event  event_type = (ieee80211_connection_event)eventtype;
    ieee80211_connection_state  newstate = P2P_DISC_STATE_OFF;
    sm_event_info               *event;

    event = (sm_event_info *)event_data;

    switch (event_type) {
    case P2P_DISC_EVENT_START:
    {
        if (prot_handle->num_outstanding_tx > 0) {
            /* We have outstanding Transmit Action frame request */
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: Error: have outstanding tx=%d. Switch to IDLE\n",
                __func__, prot_handle->num_outstanding_tx, prot_handle->on_chan_dest_freq);
        }

        newstate = P2P_DISC_STATE_IDLE;
        break;
    }

    case P2P_DISC_EVENT_TX_COMPLETE:
    {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
            "%s: Late completion of Tx frame.\n", __func__);

        proc_event_tx_complete_no_retry(prot_handle, event);
        break;
    }

    case P2P_DISC_EVENT_APP_IE_UPDATE:
        proc_event_app_ie_update(prot_handle, event);
        break;

    case P2P_DISC_EVENT_SCAN_END:           /* End of channel scan */
    case P2P_DISC_EVENT_SCAN_DEQUEUE:       /* Scan request rejected and dequeued. */
        /* Check for cancelled scan confirmation */
        if (event_scan_end_from_cancel_scan(&prot_handle->curr_scan, event->scan_cb.scan_id)) {
            /* Don't do anything special */
        }
        break;

    default:
        retval = false;
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
            "%s: called. rejected event_type=%s. ctx=0x%x.\n", __func__, event_name(event_type), ctx);
        break;
    }

    if (newstate != P2P_DISC_STATE_OFF) {
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
 * ****************************** State P2P_DISC_STATE_IDLE ******************************
 * Description: Idle and waiting for the next event. The GO/Client VAP could be operational.
 */

/*
 * Stop any discoverability listen scan (if one is scheduled or active) and
 * cancel any timer(s).
 */
static void
p2p_discoverability_listen_stop(wlan_p2p_prot_t prot_handle)
{
    if (!prot_handle->is_discoverable) {
        /* Not enabled. Therefore nothing to do */
        return;
    }
    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
        "%s: call. Stop listening.\n",__func__);

    prot_handle->is_discoverable = false;
    p2p_disc_timer_stop(prot_handle, TIMER_DISC_LISTEN);
    p2p_disc_timer_stop(prot_handle, TIMER_RETRY_SET_CHAN);

    /* If a scan has being scheduled, then cancel it */
    p2p_prot_cancel_set_channel(prot_handle);
}

/*
 * Calculate the listen interval and period to schedule the discoverability
 * listen scans.
 */
static void
calc_listen_period_n_interval(wlan_p2p_prot_t prot_handle, 
                              u_int16_t *listen_period, u_int16_t *listen_interval)
{
    struct ieee80211_vap_opmode_count   vap_opmode_count;
    int                                 num_active_ap_vaps;
    int                                 num_active_sta_vaps;


    *listen_period = *listen_period = 0;

    if (prot_handle->listen_state_disc == IEEE80211_P2P_PROT_LISTEN_STATE_DISC_NOT) {
        /* No discoverability */
        return;
    }

    OS_MEMZERO(&(vap_opmode_count), sizeof(vap_opmode_count));

    /*
     * Get total number of VAPs of each type supported by the IC.
     */
    wlan_get_vap_opmode_count(prot_handle->vap->iv_ic, &vap_opmode_count);

    num_active_ap_vaps = 0;
    num_active_sta_vaps = 0;

    /* Find out which VAPs are active */
    do {
        /* subtract AP by one since the P2P Device is an AP vap */
        vap_opmode_count.ap_count--;

        if ((vap_opmode_count.ap_count > 0) || (vap_opmode_count.btamp_count > 0)
            || (vap_opmode_count.ibss_count > 0))
        {
            /* An AP-like VAP is active */
            num_active_ap_vaps += ieee80211_vaps_ready(prot_handle->vap->iv_ic, IEEE80211_M_HOSTAP);
            if (num_active_ap_vaps) {
                break;
            }

            num_active_ap_vaps += ieee80211_vaps_ready(prot_handle->vap->iv_ic, IEEE80211_M_BTAMP);
            if (num_active_ap_vaps) {
                break;
            }

            num_active_ap_vaps += ieee80211_vaps_ready(prot_handle->vap->iv_ic, IEEE80211_M_IBSS);
            if (num_active_ap_vaps) {
                break;
            }
        }
        else if (vap_opmode_count.sta_count > 0)
        {
            num_active_sta_vaps += ieee80211_vaps_ready(prot_handle->vap->iv_ic, IEEE80211_M_STA);
            if (num_active_sta_vaps) {
                break;
            }
        }

    } while ( false );

    if (num_active_ap_vaps > 0)
    {
        /* An AP-like VAP is active */
        if (prot_handle->listen_state_disc == IEEE80211_P2P_PROT_LISTEN_STATE_DISC_AUTO) {
            /* STNG TODO: We are counting the AP VAP as active even though it may not be really active.
             * We need a better to check for these VAPs. For now, I am still going to do LISTEN scan
             * when APs are active. Normally, LISTEN scans are switched off when there is an active AP.
             * But I need to turn on this listen scan so that we are discoverable and we need to fix this. */
            *listen_period = P2P_PROT_LISTEN_PERIOD_AP_VAP_AUTO;
            *listen_interval = P2P_PROT_LISTEN_INTRVL_AP_VAP_AUTO;
        }
        else {
            ASSERT(IEEE80211_P2P_PROT_LISTEN_STATE_DISC_HIGH == prot_handle->listen_state_disc);

            *listen_period = P2P_PROT_LISTEN_PERIOD_AP_VAP_HIGH;
            *listen_interval = P2P_PROT_LISTEN_INTRVL_AP_VAP_HIGH;
        }
    }
    else if (num_active_sta_vaps)
    {
        /* An Station-like VAP is active */
        if (prot_handle->listen_state_disc == IEEE80211_P2P_PROT_LISTEN_STATE_DISC_AUTO) {
            *listen_period = P2P_PROT_LISTEN_PERIOD_STA_VAP_AUTO;
            *listen_interval = P2P_PROT_LISTEN_INTRVL_STA_VAP_AUTO;
        }
        else {
            ASSERT(IEEE80211_P2P_PROT_LISTEN_STATE_DISC_HIGH == prot_handle->listen_state_disc);

            *listen_period = P2P_PROT_LISTEN_PERIOD_STA_VAP_HIGH;
            *listen_interval = P2P_PROT_LISTEN_INTRVL_STA_VAP_HIGH;
        }
    }
    else {
        /* Assumed that we can freely go off-channel */
        if (prot_handle->listen_state_disc == IEEE80211_P2P_PROT_LISTEN_STATE_DISC_AUTO) {
            *listen_period = P2P_PROT_LISTEN_PERIOD_NO_VAP_AUTO;
            *listen_interval = P2P_PROT_LISTEN_INTRVL_NO_VAP_AUTO;
        }
        else {
            ASSERT(IEEE80211_P2P_PROT_LISTEN_STATE_DISC_HIGH == prot_handle->listen_state_disc);

            *listen_period = P2P_PROT_LISTEN_PERIOD_NO_VAP_HIGH;
            *listen_interval = P2P_PROT_LISTEN_INTRVL_NO_VAP_HIGH;
        }
    }
    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                         "%s: num_active_ap_vaps=%d, num_active_sta_vaps=%d, listen_period=%d, listen_interval=%d.\n",
                         __func__, num_active_ap_vaps, num_active_sta_vaps,
                         *listen_period, *listen_interval);

    ASSERT(*listen_period <= *listen_interval);
}


/*
 * Start the discoverability listen scan and schedule the next timer.
 */
static void
p2p_discoverability_listen_start_timer(wlan_p2p_prot_t prot_handle)
{
    struct ieee80211_vap_opmode_count   vap_opmode_count;
    u_int16_t                           listen_period = 0;
    u_int16_t                           listen_interval = 0;

    calc_listen_period_n_interval(prot_handle, &listen_period, &listen_interval);

    if ((listen_period == 0) || (listen_interval == 0)) {
        prot_handle->is_discoverable = false;
        return;
    }
    /* else we want to have Discoverability Listen scans. */
    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
        "%s: start discoverable. New Listen: prd=%d, itvl=%d. Old Listen: prd=%d, itvl=%d.\n",
        __func__, listen_period, listen_interval,
        prot_handle->ext_listen_period, prot_handle->ext_listen_interval);
    prot_handle->is_discoverable = true;
    prot_handle->ext_listen_period = listen_period;
    prot_handle->ext_listen_interval = listen_interval;

    /* we will start a scan later in time = (listen_period - listen_interval) */

    if (listen_period == listen_interval) {
        ASSERT(listen_period != 0);
        /* always in listening. Start the listen scan now. */
        p2p_disc_timer_start(prot_handle, TIMER_DISC_LISTEN, 0);
        return;
    }

    /* schedule the next listen scan */
    ASSERT(listen_period <= listen_interval);
    p2p_disc_timer_start(prot_handle, TIMER_DISC_LISTEN, (listen_interval - listen_period));
}

/*
 * indicates that the OS has change the discoverability listen state parameter
 */
static void
p2p_discoverability_listen_timer_param_changed(wlan_p2p_prot_t prot_handle)
{
    if (prot_handle->listen_state_disc == IEEE80211_P2P_PROT_LISTEN_STATE_DISC_NOT) {
        /* No discoverability */
        if (prot_handle->is_discoverable) {
            /* disable the discoverability listening */
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                "%s: some disc listen param has changed. Stop listening.\n",__func__);
            p2p_discoverability_listen_stop(prot_handle);
        }
        return;
    }
    // Else start the discoverability listen
    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
        "%s: some disc listen param has changed. Start listening.\n",__func__);

    p2p_discoverability_listen_start_timer(prot_handle);
}

/*
 * Called when the timer to schedule the next discoverability listen
 * triggers, TIMER_DISC_LISTEN.
 */
static void
st_idle_discoverability_listen_timer_event(wlan_p2p_prot_t prot_handle, ieee80211_connection_state *newstate)
{
    int     retval = EOK;

    if (!prot_handle->is_discoverable) {
        /* No discoverable. Do nothing */
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
            "%s: called but is_discoverable is false. Ignored.\n", __func__);
        return;
    }

    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
        "%s: called. Request a listen scan of duration=%d.\n",
        __func__, prot_handle->ext_listen_period);

    /* Switch to pre_listen to schedule a SET_CHAN set */
    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                         "%s: switch to pre_listen.\n",
                         __func__);

    ASSERT(*newstate == P2P_DISC_STATE_OFF);
    *newstate = P2P_DISC_STATE_PRE_LISTEN;
}

/* Exit function for this state */
static void
p2p_disc_state_IDLE_exit(void *ctx)
{
    wlan_p2p_prot_t         prot_handle = (wlan_p2p_prot_t)ctx;

    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_PROT,
        "%s: called. ctx=0x%x.\n", __func__, ctx);

    /* Stop the timer for Listen schedules */
    p2p_discoverability_listen_stop(prot_handle);
}

/* Entry function for this state */
static void
p2p_disc_state_IDLE_entry(void *ctx)
{
    wlan_p2p_prot_t         prot_handle = (wlan_p2p_prot_t)ctx;
    int                     retval = 0;

    if (prot_handle->num_outstanding_tx > 0) {
        /* We have outstanding Transmit Action frame request */
        ASSERT(have_pending_disc_scan(prot_handle) == false);

        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
            "%s: ERROR: should not have outstanding tx=%d here.\n",
            __func__, prot_handle->num_outstanding_tx);
    }

    /* Check if there is a pending discovery scan. */
    if (have_pending_disc_scan(prot_handle)) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
            "%s: Have pended Discovery Scan request.\n", __func__);

        /* Post a message to this state machine to initiate the discovery listen timers */
        ieee80211_sm_dispatch(prot_handle->hsm_handle, P2P_DISC_EVENT_START_PEND_DISCSCAN,
                              0, NULL);
    }
    else {
        /* Post a message to this state machine to initiate the discovery listen timers */
        ieee80211_sm_dispatch(prot_handle->hsm_handle, P2P_DISC_EVENT_LISTEN_ST_DISC_REQ,
                              0, NULL);
    }
}

static bool
p2p_disc_state_IDLE_event(void *ctx, u_int16_t eventtype, u_int16_t event_data_len, void *event_data) 
{
    wlan_p2p_prot_t             prot_handle = (wlan_p2p_prot_t)ctx;
    bool                        retval = true;
    sm_event_info               *event;
    ieee80211_connection_event  event_type = (ieee80211_connection_event)eventtype;
    ieee80211_connection_state  newstate = P2P_DISC_STATE_OFF;

    if (validate_event_param(prot_handle, event_type, event_data, event_data_len) != EOK) {
        return -EINVAL;
    }
    event = (sm_event_info *)event_data;

    switch (event_type) {
    case P2P_DISC_EVENT_RX_MGMT:
    {
        /* Indicates that a desired peer is found. */
        wlan_p2p_event              *p2p_dev_event;     /* event from P2P Device */

        ASSERT(event_data_len >= sizeof(sm_event_info));
        p2p_dev_event = &event->p2p_dev_cb;

        proc_event_rx_mgmt(prot_handle, &p2p_dev_event->u.rx_frame, &newstate);
        break;
    }

    case P2P_DISC_EVENT_LISTEN_ST_DISC_REQ:
        /* indicates that the OS has change the discoverability listen state parameter */
        p2p_discoverability_listen_timer_param_changed(prot_handle);
        break;

    case P2P_DISC_EVENT_TMR_DISC_LISTEN:    /* Timeout for Discovery Listen state timer */
        st_idle_discoverability_listen_timer_event(prot_handle, &newstate);
        break;

    case P2P_DISC_EVENT_ON_CHAN_START:  /* start of set channel */
    {
        /* Unexpected ON_CHAN_START: ignore it */
        wlan_p2p_event              *p2p_dev_event;     /* event from P2P Device */

        ASSERT(event_data_len >= sizeof(sm_event_info));
        p2p_dev_event = &event->p2p_dev_cb;

        /* check the ID to match. */
        if (p2p_dev_event->req_id != prot_handle->curr_scan.set_chan_req_id) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: Event O_CHAN_START but unmatched ID: %d != %d\n",
                __func__, p2p_dev_event->req_id, prot_handle->curr_scan.set_chan_req_id);
            break;
        }

        ASSERT(prot_handle->curr_scan.set_chan_requested);

        /* Make sure that the channel is correct */
        if (p2p_dev_event->u.chan_start.freq != prot_handle->my_listen_chan->ic_freq) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: My ON CHAN started but freq=%d is wrong. Exp=%d\n",
                __func__, p2p_dev_event->u.chan_start.freq, prot_handle->my_listen_chan->ic_freq);
        }

        if (prot_handle->curr_scan.set_chan_started) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: Got P2P_DISC_EVENT_ON_CHAN_START event but prot_handle->set_chan_started is already true\n",
                __func__);
        }

        /* Note down the details of this ON_CHAN */
        prot_handle->curr_scan.set_chan_freq = p2p_dev_event->u.chan_start.freq;
        prot_handle->curr_scan.set_chan_started = true;
        prot_handle->curr_scan.set_chan_startime = OS_GET_TIMESTAMP();

        if (!prot_handle->is_discoverable) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                "%s: My ON CHAN started but unexpected since is_discoverable is false\n",
                __func__);
        }

        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
            "%s: Unexpected on chan start: freq=%d, dur=%d, in Listening. Ignored\n", 
            __func__, p2p_dev_event->u.chan_start.freq, p2p_dev_event->u.chan_start.duration);
        break;
    }

    case P2P_DISC_EVENT_ON_CHAN_END:        /* End of on channel */
    {
        /* unexpected ON_CHAN_END */
        wlan_p2p_event              *p2p_dev_event;     /* event from P2P Device */

        ASSERT(event_data_len >= sizeof(sm_event_info));
        p2p_dev_event = &event->p2p_dev_cb;

        /* Check for cancelled set_on_channel confirmation */
        if (event_on_chan_end_is_from_cancel_scan(prot_handle, event->p2p_dev_cb.req_id)) {
            /* Don't do anything special */
        }

        /* check the ID to match. */
        if (p2p_dev_event->req_id != prot_handle->curr_scan.set_chan_req_id) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                "%s: Event O_CHAN_END but unmatched ID: %d != %d\n",
                __func__, p2p_dev_event->req_id, prot_handle->curr_scan.set_chan_req_id);
            break;
        }

        prot_handle->curr_scan.set_chan_started = false;
        prot_handle->curr_scan.set_chan_requested = false;

        if (!prot_handle->is_discoverable) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: My ON CHAN ended but unexpected since is_discoverable is false\n",
                __func__);
            break;
        }

        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
            "%s: Unexpected on chan end: freq=%d, dur=%d, in Listening. Ignored\n", 
            __func__, p2p_dev_event->u.chan_start.freq, p2p_dev_event->u.chan_start.duration);
        break;
    }

    CASE_ALL_TX_ACTION_FRAMES
    {
        ASSERT(event_data_len >= sizeof(sm_event_info));
        if (p2p_prot_proc_common_tx_action_frame_event(prot_handle, event_type, event) != 0) {
            break;
        }

        /* Call the common function to send action frame while in Idle state */
        if (event->tx_info->need_mini_find) {
            /* We do not know where this peer is. Needs to do a mini-find */
            start_minifind(prot_handle, event->event_type, event->tx_info, true, &newstate);
        }
        else {
            // Else we know which channel to send action frame.
            proc_event_tx_frame(prot_handle, event_type, event->tx_info, &newstate);
        }
        event->tx_info = NULL;
        break;
    }

    case P2P_DISC_EVENT_RESET:
        proc_event_reset(prot_handle);
        break;

    case P2P_DISC_EVENT_END:
        proc_event_end(prot_handle);
        ASSERT(newstate == P2P_DISC_STATE_OFF);
        newstate = P2P_DISC_STATE_OFF;
        break;

    case P2P_DISC_EVENT_SCAN_END:           /* End of channel scan */
    case P2P_DISC_EVENT_SCAN_DEQUEUE:       /* Scan request rejected and dequeued. */
        /* Check for cancelled scan confirmation */
        if (event_scan_end_from_cancel_scan(&prot_handle->curr_scan, event->scan_cb.scan_id)) {
            /* Don't do anything special */
        }
        break;

    case P2P_DISC_EVENT_START_GO:           /* command to start Group Owner */
    case P2P_DISC_EVENT_START_CLIENT:       /* command to start P2P Client */
    case P2P_DISC_EVENT_SCAN_START:         /* start of channel scan */
        /* STNG TODO: add code to monitor the other VAP state changes and 
         * adjust the Listen State Discoverability parameters */
        /* Can ignore */
        break;

    case P2P_DISC_EVENT_START_PEND_DISCSCAN:
        ASSERT(prot_handle->curr_disc_scan_info != NULL);

        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
            "%s: Pended Discovery Scan request. Switching to DISC_SCAN state. Timeout=%d\n",
            __func__, prot_handle->curr_disc_scan_info->orig_req_param.timeout);

        ASSERT(!prot_handle->disc_scan_active);

        p2p_prot_start_disc_scan(prot_handle, &newstate);
        break;

    case P2P_DISC_EVENT_DISC_SCAN_REQ:      /* Device Discovery Scan request */
        /* If in IDLE state, then can start the Device Discovery Scan now */
        ASSERT(prot_handle->curr_disc_scan_info == NULL);

        ASSERT(event_data_len >= sizeof(sm_event_info));

        if (proc_event_disc_scan_req(prot_handle, event, &newstate) != EOK) {
            /* Some error in processing this event */
            ASSERT(newstate == P2P_DISC_STATE_OFF);
            break;
        }
        /* Else continue to start this discovery scan */

        p2p_prot_start_disc_scan(prot_handle, &newstate);
        break;

    case P2P_DISC_EVENT_DISC_SCAN_STOP:      /* Device Discovery Scan Stop */
    {
        proc_event_disc_scan_stop(prot_handle, &newstate);
        break;
    }

    case P2P_DISC_EVENT_TX_COMPLETE:
        ASSERT(event_data_len >= sizeof(sm_event_info));
        proc_event_tx_complete(prot_handle, event, &newstate);
        break;

    case P2P_DISC_EVENT_TMR_DISC_SCAN_EXPIRE:
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
            "%s: Error: should not have this event %s in IDLE state.\n",
            __func__, event_name(event_type));
        /* Failed the current discovery request */
        disc_scan_timer_expired(prot_handle);
        break;

    case P2P_DISC_EVENT_TMR_TX_REQ_EXPIRE:
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
            "%s: Error: should not have this event %s in IDLE state.\n",
            __func__, event_name(event_type));
        ASSERT(false);
        complete_outstanding_tx_req(prot_handle);
        break;

    case P2P_DISC_EVENT_APP_IE_UPDATE:
        proc_event_app_ie_update(prot_handle, event);
        break;

    default:
        retval = false;
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                             "%s: called. rejected event_type=%s. ctx=0x%x.\n",
                             __func__, event_name(event_type), ctx);
        break;
    }

    if (newstate != P2P_DISC_STATE_OFF) {
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
 * ****************************** State P2P_DISC_STATE_PRE_LISTEN ******************************
 * Description: Waiting for hardware to switch to specified channel before transiting to ST_Listen state.
 */

/* Exit function for this state */
static void
p2p_disc_state_PRE_LISTEN_exit(void *ctx)
{
    wlan_p2p_prot_t         prot_handle = (wlan_p2p_prot_t)ctx;

    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_PROT,
        "%s: called. ctx=0x%x.\n", __func__, ctx);

    /* Cancel timer (if any) */
    p2p_disc_timer_stop(prot_handle, TIMER_SET_CHAN_SWITCH);
    p2p_disc_timer_stop(prot_handle, TIMER_RETRY_SET_CHAN);

    if (prot_handle->st_pre_listen_abort_listen &&
        prot_handle->curr_scan.set_chan_requested)
    {
        /* 
         * Cancel present remain_on_channel. Note that we are not waiting
         * for the cancel to complete but wait for SCAN_DEQUEUED or SCAN_COMPLETED events.
         */
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
            "%s: abort this on_channel.\n", __func__, ctx);

        /* Abort this Listen scan */
        p2p_prot_cancel_set_channel(prot_handle);
    }
}

/*
 * Routine to request a Listen scan. Used in state PRE_LISTEN.
 */
static void
st_pre_listen_req_scan(wlan_p2p_prot_t prot_handle)
{
    int                     retval = 0;

    ASSERT(!prot_handle->curr_scan.set_chan_requested);

    retval = p2p_prot_request_listen_scan(prot_handle, prot_handle->ext_listen_period);
    if (retval != EOK)
    {
        /* Error: Retry later */
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
            "%s: Err: p2p_prot_request_listen_scan ret=%d. Retry later\n", __func__, retval);

        p2p_disc_timer_start(prot_handle, TIMER_RETRY_SET_CHAN, DEF_TIMEOUT_RETRY_SET_SCAN);

        return;
    }
    else {
        /* Set a timeout for this channel switch, Tmr_ChanSwitch. */
        p2p_disc_timer_start(prot_handle, TIMER_SET_CHAN_SWITCH, DEF_TIMEOUT_CHANNEL_SWITCH);
    }
}

/*
 * Abort the discovery scan. If this discovery scan is started due to a minifind, then
 * abort that transmit action frame request too.
 */
static void
p2p_disc_scan_abort(wlan_p2p_prot_t prot_handle)
{
    ieee80211_connection_state  newstate = P2P_DISC_STATE_OFF;

    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
        "%s: called.\n", __func__);

    if (prot_handle->curr_disc_scan_info != NULL) {
        if (prot_handle->curr_disc_scan_info->is_minifind) {
            /* Abort this tx action frame request */

            prot_handle->curr_disc_scan_info->minifind.aborted = true;

            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                "%s: Disc scan is due to minifind. Abort this tx frame too.\n", __func__);
        }
    }

    st_disc_scan_complete_req(prot_handle, -EBUSY, &newstate);

    /* Note: There should not be a change in state unless it is due to minifind. But we have
     * already abort the tx request. */
    if (newstate != P2P_DISC_STATE_OFF) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
            "%s: Ignore state change to %s(%d) after calling st_listen_abort_disc_scan().\n",
            __func__, ieee80211_sm_get_state_name(prot_handle->hsm_handle, newstate),
            newstate);
    }
    ASSERT(newstate == P2P_DISC_STATE_OFF);
}

/* Entry function for this state */
static void
p2p_disc_state_PRE_LISTEN_entry(void *ctx)
{
    wlan_p2p_prot_t         prot_handle = (wlan_p2p_prot_t)ctx;
    int                     retval = 0;
    u_int16_t               listen_freq;

    ASSERT(prot_handle->curr_tx_info == NULL);

    listen_freq = prot_handle->my_listen_chan->ic_freq;

    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_PROT,
        "%s: called. ctx=0x%x. Freq=%d, disc_scan_active=%d\n", 
        __func__, ctx, listen_freq, prot_handle->disc_scan_active);

    prot_handle->scan_cancel_reason = SCAN_CANCEL_REASON_NULL;
    prot_handle->st_pre_listen_abort_listen = false;

    if (prot_handle->curr_scan.set_chan_requested) {
        /* This should not happen since the earlier state should have clean up */
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
            "%s: Error: there is a pending set_channel requested.\n", __func__);
        p2p_prot_cancel_set_channel(prot_handle);
    }

    /* Workaround for scan bug. If the last scan is cancelled and we start a new scan too
     * fast, then we may lose (very small chance though) the scan end/completion message. */
    if (p2p_prot_have_pending_cancel_scan(prot_handle)) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
            "%s: Warning: there is a pending cancel scan. wait for the cancel.\n", __func__);
        prot_handle->curr_scan.cancel_scan_recheck_cnt = WAR_NUM_CHECK_FOR_CANCEL_SCAN_EVENT;
        p2p_disc_timer_start(prot_handle, TIMER_RETRY_SET_CHAN, DEF_TIMEOUT_RETRY_SET_SCAN);
    }
    else {
        st_pre_listen_req_scan(prot_handle);
    }
}

/*
 * Note: If the current Tx Action event needs to be aborted, then call complete_tx_action_frame_event()
 *       to inform the caller about the status.
 */
static bool
p2p_disc_state_PRE_LISTEN_event(void *ctx, u_int16_t eventtype, u_int16_t event_data_len, void *event_data) 
{
    wlan_p2p_prot_t             prot_handle = (wlan_p2p_prot_t)ctx;
    bool                        retval = true;
    sm_event_info               *event;
    ieee80211_connection_event  event_type = (ieee80211_connection_event)eventtype;
    ieee80211_connection_state  newstate = P2P_DISC_STATE_OFF;

    if (validate_event_param(prot_handle, event_type, event_data, event_data_len) != EOK) {
        return -EINVAL;
    }
    event = (sm_event_info *)event_data;

    switch (event_type) {
    case P2P_DISC_EVENT_RX_MGMT:
    {
        /* Indicates that a desired peer is found. */
        wlan_p2p_event              *p2p_dev_event;     /* event from P2P Device */

        ASSERT(event_data_len >= sizeof(sm_event_info));
        p2p_dev_event = &event->p2p_dev_cb;

        proc_event_rx_mgmt(prot_handle, &p2p_dev_event->u.rx_frame, &newstate);
        break;
    }

    case P2P_DISC_EVENT_TMR_RETRY_SET_CHAN:
        /* Retry to set channel again since the previous one failed */
        if (p2p_prot_have_pending_cancel_scan(prot_handle)) {

            prot_handle->curr_scan.cancel_scan_recheck_cnt--;

            if (prot_handle->curr_scan.cancel_scan_recheck_cnt > 0) {
                /* continue to wait */
                IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                    "%s: Note: waiting for pending cancel scan to complete (times=%d).\n",
                    __func__, prot_handle->curr_scan.cancel_scan_recheck_cnt);
                /*Setup another timer to wait for the last scan to complete*/
                p2p_disc_timer_start(prot_handle, TIMER_RETRY_SET_CHAN, DEF_TIMEOUT_RETRY_SET_SCAN);
                break;
            }
            else {
                IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                    "%s: Error: there is a pending cancel scan even after the wait. Ignore it\n",
                    __func__, prot_handle->curr_scan.cancel_scan_recheck_cnt);
            }
        }

        st_pre_listen_req_scan(prot_handle);
        break;

    case P2P_DISC_EVENT_START_GO:           /* command to start Group Owner */
    case P2P_DISC_EVENT_START_CLIENT:       /* command to start P2P Client */
    {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
            "%s: Warning: unexpect event that the GO/Client is starting. Ignored!\n", __func__);
        break;
    }

    case P2P_DISC_EVENT_SCAN_END:           /* End of channel scan */
    case P2P_DISC_EVENT_SCAN_DEQUEUE:       /* Scan request rejected and dequeued. */
        /* Check for cancelled scan confirmation */
        if (event_scan_end_from_cancel_scan(&prot_handle->curr_scan, event->scan_cb.scan_id)) {
            /* Don't do anything special */
        }
        break;

    case P2P_DISC_EVENT_SCAN_START:         /* start of channel scan */
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
            "%s: Unexpected scan event=%s on PRE_ONCHAN state. Ignore!\n", __func__, event_name(event_type));
        break;
    
    case P2P_DISC_EVENT_ON_CHAN_START:  /* start of set channel */
    {
        wlan_p2p_event              *p2p_dev_event;     /* event from P2P Device */

        p2p_dev_event = &event->p2p_dev_cb;

        /* check the ID to match. */
        if (p2p_dev_event->req_id != prot_handle->curr_scan.set_chan_req_id) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: Event O_CHAN_START but unmatched ID: %d != %d\n",
                __func__, p2p_dev_event->req_id, prot_handle->curr_scan.set_chan_req_id);
            break;
        }

        ASSERT(prot_handle->curr_scan.set_chan_requested);

        /* Make sure that the channel is correct */
        if (p2p_dev_event->u.chan_start.freq != prot_handle->curr_scan.set_chan_freq) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: My ON CHAN started but freq=%d is wrong. Exp=%d\n",
                __func__, p2p_dev_event->u.chan_start.freq, prot_handle->curr_scan.set_chan_freq);
        }

        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
            "%s: on chan start: freq=%d, dur=%d, goto ONCHAN state.\n", 
            __func__, p2p_dev_event->u.chan_start.freq, p2p_dev_event->u.chan_start.duration);

        /* Note down the details of this ON_CHAN */
        prot_handle->curr_scan.set_chan_started = true;
        prot_handle->curr_scan.set_chan_startime = OS_GET_TIMESTAMP();

        /* Goto State Listen. */
        ASSERT(newstate == P2P_DISC_STATE_OFF);
        newstate = P2P_DISC_STATE_LISTEN;
        break;
    }

    case P2P_DISC_EVENT_ON_CHAN_END:        /* End of on channel */
    {
        wlan_p2p_event              *p2p_dev_event;     /* event from P2P Device */

        p2p_dev_event = &event->p2p_dev_cb;

        /* Check for cancelled set_on_channel confirmation */
        if (event_on_chan_end_is_from_cancel_scan(prot_handle, event->p2p_dev_cb.req_id)) {
            /* Don't do anything special */
        }

        /* check the ID to match. */
        if (p2p_dev_event->req_id != prot_handle->curr_scan.set_chan_req_id) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                "%s: Event O_CHAN_END but unmatched ID: %d != %d\n",
                __func__, p2p_dev_event->req_id, prot_handle->curr_scan.set_chan_req_id);
            break;
        }

        prot_handle->curr_scan.set_chan_started = false;
        prot_handle->curr_scan.set_chan_requested = false;

        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
            "%s: ERROR: premature end to on_channel, most likely due to abort scan, retry later.\n",
            __func__);

        p2p_disc_timer_start(prot_handle, TIMER_RETRY_SET_CHAN, DEF_TIMEOUT_RETRY_SET_SCAN);
        break;
    }

    case P2P_DISC_EVENT_TMR_SET_CHAN_SWITCH:
    {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
            "%s: ERROR: Aborting Pre_Listen since channel switch is too slow. reset too!\n", __func__);

        prot_handle->scan_cancel_reason = SCAN_CANCEL_REASON_TIMEOUT;

        /* Abort this listen */
        prot_handle->st_pre_listen_abort_listen = true;
        proc_event_reset(prot_handle);
        ASSERT(newstate == P2P_DISC_STATE_OFF);
        newstate = P2P_DISC_STATE_IDLE;
        break;
    }

    CASE_ALL_TX_ACTION_FRAMES
    {
        int     ret_val;

        if (p2p_prot_proc_common_tx_action_frame_event(prot_handle, event_type, event) != 0) {
            break;
        }

        /* Abort this PRE-Listen. Process this transmit action frame request */
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
            "%s: Request to transmit action frame (%s) while on state Pre_Listen. Aborting listen\n",
            __func__, event_name(event_type));

        /* We need to re-schedule a new ONCHAN */

        /* abort this listen */
        prot_handle->st_pre_listen_abort_listen = true;

        /* But first, abort this discovery scan (if any). */
        if (prot_handle->disc_scan_active) {
            /* abort this discovery scan */
            p2p_disc_scan_abort(prot_handle);
        }

        if (event->tx_info->need_mini_find) {
            /* We do not know where this peer is. Needs to do a mini-find */
            start_minifind(prot_handle, event->event_type, event->tx_info, false, &newstate);
        }
        else {
            // Else we know which channel to send action frame.
            proc_event_tx_frame(prot_handle, event_type, event->tx_info, &newstate);
        }
        event->tx_info = NULL;
        break;
    }

    case P2P_DISC_EVENT_RESET:
    {
        /* Abort this listen */
        prot_handle->st_pre_listen_abort_listen = true;
        proc_event_reset(prot_handle);
        ASSERT(newstate == P2P_DISC_STATE_OFF);
        newstate = P2P_DISC_STATE_IDLE;
        break;
    }

    case P2P_DISC_EVENT_END:
    {
        prot_handle->st_pre_listen_abort_listen = true;
        proc_event_end(prot_handle);
        ASSERT(newstate == P2P_DISC_STATE_OFF);
        newstate = P2P_DISC_STATE_OFF;
        break;
    }

    case P2P_DISC_EVENT_DISC_SCAN_REQ:      /* Device Discovery Scan request */
        proc_event_disc_scan_req(prot_handle, event, &newstate);
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
            "%s: Going to do listen. Starts discovery scan Request after listen state.\n",
            __func__);
        break;

    case P2P_DISC_EVENT_DISC_SCAN_STOP:      /* Device Discovery Scan Stop */
    {
        proc_event_disc_scan_stop(prot_handle, &newstate);
        break;
    }

    case P2P_DISC_EVENT_TX_COMPLETE:
    {
        ASSERT(event_data_len >= sizeof(sm_event_info));
        proc_event_tx_complete(prot_handle, event, &newstate);
        break;
    }

    case P2P_DISC_EVENT_TMR_DISC_SCAN_EXPIRE:
        disc_scan_timer_expired(prot_handle);
        break;

    case P2P_DISC_EVENT_LISTEN_ST_DISC_REQ:
        /* Ignore this event and process it in IDLE state entry function */
        break;

    case P2P_DISC_EVENT_TMR_TX_REQ_EXPIRE:
    {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
            "%s: Tx Request Timer Expired. Abort tx action frame request. num_outstanding_tx=%d\n",
            __func__, prot_handle->num_outstanding_tx);

        ASSERT(prot_handle->num_outstanding_tx > 0);

        complete_outstanding_tx_req(prot_handle);
        break;
    }

    case P2P_DISC_EVENT_APP_IE_UPDATE:
        proc_event_app_ie_update(prot_handle, event);
        break;

    default:
        retval = false;
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                             "%s: called. rejected event_type=%s. ctx=0x%x.\n",
                             __func__, event_name(event_type), ctx);
        break;
    }

    if (newstate != P2P_DISC_STATE_OFF) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                             "%s: end of function and switching state to %s(%d).\n",
                             __func__, 
                             ieee80211_sm_get_state_name(prot_handle->hsm_handle, newstate),
                             newstate);
        ieee80211_sm_transition_to(prot_handle->hsm_handle, newstate);
    }

    return retval;
}   /* end of fn p2p_disc_state_PRE_LISTEN_event() */

/*
 * ****************************** State P2P_DISC_STATE_LISTEN ******************************
 * Description: Remain on this channel to listen for probe request frame and reply with probe responses.
 *              We may also do certain action frames transaction like 
 *              3-way handshake during Group Formation phase, 2-way handshake during
 *              Invitation phase, and 2-way handshake during Provisioning phase.
 *              Note that the hardware is already on the actual channel.
 */

/* Exit function for this state */
static void
p2p_disc_state_LISTEN_exit(void *ctx)
{
    wlan_p2p_prot_t         prot_handle = (wlan_p2p_prot_t)ctx;

    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_PROT,
        "%s: called. ctx=0x%x. num_outstanding_tx=%d\n", __func__, ctx, prot_handle->num_outstanding_tx);

    p2p_disc_timer_stop(prot_handle, TIMER_TX_RETRY);

    /* If a scan has being scheduled, then cancel it */
    p2p_prot_cancel_set_channel(prot_handle);
}

/* Entry function for this state */
static void
p2p_disc_state_LISTEN_entry(void *ctx)
{
    wlan_p2p_prot_t                 prot_handle = (wlan_p2p_prot_t)ctx;
    int                             retval = 0;
    tx_event_info                   *tx_fr_info;

    ASSERT(prot_handle->curr_tx_info == NULL);

    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_PROT,
        "%s: called. ctx=0x%x. Freq=%d\n", __func__, ctx, 
        prot_handle->on_chan_dest_freq);

    ASSERT(prot_handle->curr_scan.set_chan_requested);

    /* Check if we need to continue the tx action frame */
    if (prot_handle->retry_tx_info) {
        if (prot_handle->on_chan_dest_freq == prot_handle->retry_tx_info->tx_freq) {
            /* The Tx frame freq equals to our listen freq. Can continue to transmit. */

            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                "%s: tx freq==listen freq. can continue to retry tx. retry_tx_info=%pK\n",
                __func__, prot_handle->retry_tx_info);

            ieee80211_sm_dispatch(prot_handle->hsm_handle, P2P_DISC_EVENT_TMR_TX_RETRY, 0, NULL);
        }
    }
}

/*
 * State event handling for LISTEN state.
 */
static bool
p2p_disc_state_LISTEN_event(void *ctx, u_int16_t eventtype, u_int16_t event_data_len, void *event_data) 
{
    wlan_p2p_prot_t             prot_handle = (wlan_p2p_prot_t)ctx;
    bool                        retval = true;
    sm_event_info               *event;
    ieee80211_connection_event  event_type = (ieee80211_connection_event)eventtype;
    ieee80211_connection_state  newstate = P2P_DISC_STATE_OFF;

    if (validate_event_param(prot_handle, event_type, event_data, event_data_len) != EOK) {
        return -EINVAL;
    }
    event = (sm_event_info *)event_data;
    
    switch (event_type) {
    case P2P_DISC_EVENT_RX_MGMT:
    {
        /* Indicates that a desired peer is found. */
        wlan_p2p_event              *p2p_dev_event;     /* event from P2P Device */

        ASSERT(event_data_len >= sizeof(sm_event_info));
        p2p_dev_event = &event->p2p_dev_cb;

        proc_event_rx_mgmt(prot_handle, &p2p_dev_event->u.rx_frame, &newstate);
        break;
    }

    case P2P_DISC_EVENT_TX_COMPLETE:
    {
        ASSERT(event_data_len >= sizeof(sm_event_info));
        st_listen_n_onchan_proc_tx_complete(prot_handle, event);
        break;
    }

    case P2P_DISC_EVENT_TMR_TX_RETRY:   /* Time to resend the action frame */
    {
        st_listen_n_onchan_proc_tx_retry_event(prot_handle, event);
        break;
    }

    CASE_ALL_TX_ACTION_FRAMES
        ASSERT(event_data_len >= sizeof(sm_event_info));
        if (p2p_prot_proc_common_tx_action_frame_event(prot_handle, event_type, event) != 0) {
            break;
        }
        st_listen_n_onchan_proc_tx_action_fr_event(prot_handle, event, &newstate);
        break;

    case P2P_DISC_EVENT_START_GO:           /* command to start Group Owner */
    case P2P_DISC_EVENT_START_CLIENT:       /* command to start P2P Client */
    {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
            "%s: Aborting Pre_OnChan since the GO/Client is starting.\n", __func__);

        prot_handle->scan_cancel_reason = SCAN_CANCEL_REASON_GO_CLIENT_STARTED;

        /* 
         * Cancel present remain_on_channel. Note that we are not waiting
         * for the cancel to complete but wait for SCAN_DEQUEUED or SCAN_COMPLETED events.
         */
        p2p_prot_cancel_set_channel(prot_handle);
        break;
    }

    case P2P_DISC_EVENT_SCAN_END:           /* End of channel scan */
    case P2P_DISC_EVENT_SCAN_DEQUEUE:       /* Scan request rejected and dequeued. */
        /* Check for cancelled scan confirmation */
        if (event_scan_end_from_cancel_scan(&prot_handle->curr_scan, event->scan_cb.scan_id)) {
            /* Don't do anything special */
        }
        break;

    case P2P_DISC_EVENT_SCAN_START:         /* start of channel scan */
    {
        /* Ignore all the scan events since we use wlan_p2p_set_channel() to switch channel */
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
            "%s: Ignore the scan event=%s on PRE_ONCHAN state.\n", __func__, event_name(event_type));
        break;
    }

    case P2P_DISC_EVENT_ON_CHAN_START:
    {
        wlan_p2p_event              *p2p_dev_event;     /* event from P2P Device */

        ASSERT(event_data_len >= sizeof(sm_event_info));
        p2p_dev_event = &event->p2p_dev_cb;

        /* check the ID to match. */
        if (p2p_dev_event->req_id != prot_handle->curr_scan.set_chan_req_id) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                "%s: Event O_CHAN_START but unmatched ID: %d != %d\n",
                __func__, p2p_dev_event->req_id, prot_handle->curr_scan.set_chan_req_id);
            break;
        }

        /* Note down the details of this ON_CHAN */
        prot_handle->curr_scan.set_chan_freq = p2p_dev_event->u.chan_start.freq;
        prot_handle->curr_scan.set_chan_started = true;
        prot_handle->curr_scan.set_chan_startime = OS_GET_TIMESTAMP();

        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
            "%s: Warn: unexpected OnChan_Start, ignored.\n", __func__);
        break;
    }

    case P2P_DISC_EVENT_ON_CHAN_END:
    {
        wlan_p2p_event              *p2p_dev_event;     /* event from P2P Device */

        ASSERT(event_data_len >= sizeof(sm_event_info));
        p2p_dev_event = &event->p2p_dev_cb;

        /* Check for cancelled set_on_channel confirmation */
        if (event_on_chan_end_is_from_cancel_scan(prot_handle, event->p2p_dev_cb.req_id)) {
            /* Don't do anything special */
        }

        /* check the ID to match. */
        if (p2p_dev_event->req_id != prot_handle->curr_scan.set_chan_req_id) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                "%s: Event O_CHAN_END but unmatched ID: %d != %d\n",
                __func__, p2p_dev_event->req_id, prot_handle->curr_scan.set_chan_req_id);
            break;
        }

        prot_handle->curr_scan.set_chan_started = false;

        ASSERT(prot_handle->curr_scan.set_chan_requested);
        prot_handle->curr_scan.set_chan_requested = false;

        st_listen_n_onchan_proc_on_chan_end(prot_handle, &newstate);
        break;
    }

    case P2P_DISC_EVENT_RESET:
    {
        proc_event_reset(prot_handle);
        ASSERT(newstate == P2P_DISC_STATE_OFF);
        newstate = P2P_DISC_STATE_IDLE;
        break;
    }

    case P2P_DISC_EVENT_END:
    {
        proc_event_end(prot_handle);
        ASSERT(newstate == P2P_DISC_STATE_OFF);
        newstate = P2P_DISC_STATE_OFF;
        break;
    }

    case P2P_DISC_EVENT_DISC_SCAN_REQ:      /* Device Discovery Scan request */
    {
        int     retval;

        ASSERT(event_data_len >= sizeof(sm_event_info));
        retval = proc_event_disc_scan_req(prot_handle, event, &newstate);
        if (retval != EOK) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                                 "%s: Warning: get new event %s while the previous has not ended yet.\n",
                                 __func__, event_name(event_type));
        }
        break;
    }
    case P2P_DISC_EVENT_DISC_SCAN_STOP:      /* Device Discovery Scan Stop */
    {
        proc_event_disc_scan_stop(prot_handle, &newstate);
        break;
    }

    case P2P_DISC_EVENT_TMR_DISC_SCAN_EXPIRE:
        disc_scan_timer_expired(prot_handle);
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                             "%s: Disc scan time expired. still continue this listen\n",
                             __func__, event_name(event_type), ctx);
        break;

    case P2P_DISC_EVENT_LISTEN_ST_DISC_REQ:
        /* Ignore this event and process when just before going into IDLE state */
        break;

    case P2P_DISC_EVENT_TMR_TX_REQ_EXPIRE:
    {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
            "%s: Tx Request Timer Expired. Abort tx action frame request. num_outstanding_tx=%d\n",
            __func__, prot_handle->num_outstanding_tx);

        ASSERT(prot_handle->num_outstanding_tx > 0);

        complete_outstanding_tx_req(prot_handle);
        break;
    }

    case P2P_DISC_EVENT_APP_IE_UPDATE:
        proc_event_app_ie_update(prot_handle, event);
        break;

    default:
        retval = false;
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                             "%s: called. rejected event_type=%s. ctx=0x%x.\n",
                             __func__, event_name(event_type), ctx);
        break;
    }

    if (newstate != P2P_DISC_STATE_OFF) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                             "%s: end of function and switching state to %s(%d).\n",
                             __func__, 
                             ieee80211_sm_get_state_name(prot_handle->hsm_handle, newstate),
                             newstate);
        ieee80211_sm_transition_to(prot_handle->hsm_handle, newstate);
    }

    return retval;
} //end of function p2p_disc_state_LISTEN_event()

/*
 * ****************************** State P2P_DISC_STATE_PRE_ONCHAN ******************************
 * Description: Waiting for hardware to switch to specified channel before transiting to ST_OnChan state.
 */

/* Exit function for this state */
static void
p2p_disc_state_PRE_ONCHAN_exit(void *ctx)
{
    wlan_p2p_prot_t         prot_handle = (wlan_p2p_prot_t)ctx;

    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_PROT,
        "%s: called. ctx=0x%x.\n", __func__, ctx);

    /* Cancel timer (if any) */
    p2p_disc_timer_stop(prot_handle, TIMER_SET_CHAN_SWITCH);
    p2p_disc_timer_stop(prot_handle, TIMER_RETRY_SET_CHAN);
    
    if ((prot_handle->curr_scan.set_chan_requested) &&
        (prot_handle->st_pre_onchan_abort))
    {
        /* 
         * Cancel present remain_on_channel. Note that we are not waiting
         * for the cancel to complete but wait for SCAN_DEQUEUED or SCAN_COMPLETED events.
         */
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
            "%s: abort this ON_CHAN.\n", __func__);

        /* If a scan has being scheduled, then cancel it */
        p2p_prot_cancel_set_channel(prot_handle);
    }
}

static void
st_pre_onchan_req_set_chan(wlan_p2p_prot_t prot_handle)
{
    int     retval;

    /* Schedule a remain_on_channel for the designated channel, prot_handle->on_chan_dest */
    prot_handle->curr_scan.set_chan_req_id++;  /* Increment to get current ID */
    retval = wlan_p2p_set_channel(prot_handle->p2p_device_handle, 
                                  false /* Not in listen state */, 
                                  prot_handle->on_chan_dest_freq, 
                                  prot_handle->curr_scan.set_chan_req_id,
                                  P2P_DISC_DEF_ON_CHAN_TIME,
                                  IEEE80211_P2P_SCAN_PRIORITY_HIGH  /* makes a high priority scan */
                                  );
    if (retval != EOK)
    {
        /* Error: Retry later */

        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
            "%s: Err: wlan_p2p_set_channel ret=%d. Freq=%d. Retry later\n", __func__, retval, 
            prot_handle->on_chan_dest_freq);

        p2p_disc_timer_start(prot_handle, TIMER_RETRY_SET_CHAN, DEF_TIMEOUT_RETRY_SET_SCAN);

        return;
    }
    else {
        prot_handle->curr_scan.set_chan_requested = true;
        prot_handle->curr_scan.set_chan_freq = prot_handle->on_chan_dest_freq;
        prot_handle->curr_scan.set_chan_duration = P2P_DISC_DEF_ON_CHAN_TIME;

        /* Set a timeout for this channel switch, Tmr_ChanSwitch. */
        p2p_disc_timer_start(prot_handle, TIMER_SET_CHAN_SWITCH,  DEF_TIMEOUT_CHANNEL_SWITCH);
    }
}

/* Entry function for this state */
static void
p2p_disc_state_PRE_ONCHAN_entry(void *ctx)
{
    wlan_p2p_prot_t         prot_handle = (wlan_p2p_prot_t)ctx;
    int                     retval = 0;

    ASSERT(prot_handle->num_outstanding_tx > 0);

    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_PROT,
        "%s: called. ctx=0x%x. Freq=%d\n", __func__, ctx, 
        prot_handle->on_chan_dest_freq);

    prot_handle->scan_cancel_reason = SCAN_CANCEL_REASON_NULL;
    prot_handle->st_pre_onchan_abort = false;

    if (prot_handle->curr_scan.set_chan_requested) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
            "%s: Warning: there is a pending set_channel requested.\n", __func__);
        p2p_prot_cancel_set_channel(prot_handle);
    }

    /* Workaround for scan bug. If the last scan is cancelled and we start a new scan too
    * fast, then we may lose (very small chance though) the scan end/completion message. */
    if (p2p_prot_have_pending_cancel_scan(prot_handle)) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
            "%s: Warning: there is a pending cancel scan. wait and retry.\n", __func__);
        prot_handle->curr_scan.cancel_scan_recheck_cnt = WAR_NUM_CHECK_FOR_CANCEL_SCAN_EVENT;
        p2p_disc_timer_start(prot_handle, TIMER_RETRY_SET_CHAN, DEF_TIMEOUT_RETRY_SET_SCAN);
    }
    else {
        st_pre_onchan_req_set_chan(prot_handle);
    }
}

/*
 * Note: If the current Tx Action event needs to be aborted, then call complete_tx_action_frame_event()
 *       to inform the caller about the status.
 */
static bool
p2p_disc_state_PRE_ONCHAN_event(void *ctx, u_int16_t eventtype, u_int16_t event_data_len, void *event_data) 
{
    wlan_p2p_prot_t             prot_handle = (wlan_p2p_prot_t)ctx;
    bool                        retval = true;
    sm_event_info               *event;
    ieee80211_connection_event  event_type = (ieee80211_connection_event)eventtype;
    ieee80211_connection_state  newstate = P2P_DISC_STATE_OFF;

    if (validate_event_param(prot_handle, event_type, event_data, event_data_len) != EOK) {
        return -EINVAL;
    }
    event = (sm_event_info *)event_data;

    switch (event_type) {
    case P2P_DISC_EVENT_RX_MGMT:
    {
        /* Indicates that a desired peer is found. */
        wlan_p2p_event              *p2p_dev_event;     /* event from P2P Device */

        ASSERT(event_data_len >= sizeof(sm_event_info));
        p2p_dev_event = &event->p2p_dev_cb;

        proc_event_rx_mgmt(prot_handle, &p2p_dev_event->u.rx_frame, &newstate);
        break;
    }

    case P2P_DISC_EVENT_TMR_RETRY_SET_CHAN:
        if (p2p_prot_have_pending_cancel_scan(prot_handle)) {

            prot_handle->curr_scan.cancel_scan_recheck_cnt--;

            if (prot_handle->curr_scan.cancel_scan_recheck_cnt > 0) {
                /* continue to wait */
                IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                    "%s: Note: waiting for pending cancel scan to complete (times=%d).\n",
                    __func__, prot_handle->curr_scan.cancel_scan_recheck_cnt);
                /*Setup another timer to wait for the last scan to complete */
                p2p_disc_timer_start(prot_handle, TIMER_RETRY_SET_CHAN, DEF_TIMEOUT_RETRY_SET_SCAN);
                break;
            }
            else {
                IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                    "%s: Error: there is a pending cancel scan even after the wait. Ignore it\n",
                    __func__, prot_handle->curr_scan.cancel_scan_recheck_cnt);
            }
        }

        st_pre_onchan_req_set_chan(prot_handle);
        break;

    case P2P_DISC_EVENT_START_GO:           /* command to start Group Owner */
    case P2P_DISC_EVENT_START_CLIENT:       /* command to start P2P Client */
    {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
            "%s: Warning: unexpect event that the GO/Client is starting. Ignored!\n", __func__);
        break;
    }

    case P2P_DISC_EVENT_SCAN_END:           /* End of channel scan */
    case P2P_DISC_EVENT_SCAN_DEQUEUE:       /* Scan request rejected and dequeued. */
        /* Check for cancelled scan confirmation */
        if (event_scan_end_from_cancel_scan(&prot_handle->curr_scan, event->scan_cb.scan_id)) {
            /* Don't do anything special */
        }
        break;

    case P2P_DISC_EVENT_SCAN_START:         /* start of channel scan */
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
            "%s: Unexpected scan event=%s on PRE_ONCHAN state. Ignore!\n", __func__, event_name(event_type));
        break;
    
    case P2P_DISC_EVENT_ON_CHAN_START:  /* start of set channel */
    {
        wlan_p2p_event              *p2p_dev_event;     /* event from P2P Device */

        p2p_dev_event = &event->p2p_dev_cb;

        /* check the ID to match. */
        if (p2p_dev_event->req_id != prot_handle->curr_scan.set_chan_req_id) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: Event O_CHAN_START but unmatched ID: %d != %d\n",
                __func__, p2p_dev_event->req_id, prot_handle->curr_scan.set_chan_req_id);
            break;
        }

        ASSERT(prot_handle->curr_scan.set_chan_requested);

        /* Make sure that the channel is correct */
        if (p2p_dev_event->u.chan_start.freq != prot_handle->on_chan_dest_freq) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: My ON CHAN started but freq=%d is wrong. Exp=%d\n",
                __func__, p2p_dev_event->u.chan_start.freq, prot_handle->on_chan_dest_freq);
        }

        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
            "%s: on chan start: freq=%d, dur=%d, goto ONCHAN state.\n", 
            __func__, p2p_dev_event->u.chan_start.freq, p2p_dev_event->u.chan_start.duration);

        /* Note down the details of this ON_CHAN */
        prot_handle->curr_scan.set_chan_freq = p2p_dev_event->u.chan_start.freq;
        prot_handle->curr_scan.set_chan_started = true;
        prot_handle->curr_scan.set_chan_startime = OS_GET_TIMESTAMP();

        /* Goto State OnChan. The current tx param is stored in prot_handle->curr_tx_param */
        ASSERT(newstate == P2P_DISC_STATE_OFF);
        newstate = P2P_DISC_STATE_ONCHAN;
        break;
    }

    case P2P_DISC_EVENT_ON_CHAN_END:        /* End of on channel */
    {
        wlan_p2p_event              *p2p_dev_event;     /* event from P2P Device */

        p2p_dev_event = &event->p2p_dev_cb;

        /* Check for cancelled set_on_channel confirmation */
        if (event_on_chan_end_is_from_cancel_scan(prot_handle, event->p2p_dev_cb.req_id)) {
            /* Don't do anything special */
        }

        /* check the ID to match. */
        if (p2p_dev_event->req_id != prot_handle->curr_scan.set_chan_req_id) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                "%s: Event O_CHAN_END but unmatched ID: %d != %d\n",
                __func__, p2p_dev_event->req_id, prot_handle->curr_scan.set_chan_req_id);
            break;
        }

        prot_handle->curr_scan.set_chan_started = false;
        prot_handle->curr_scan.set_chan_requested = false;

        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
            "%s: Error: premature end to on_channel, most likely due to scan abort. Retry this scan\n",
            __func__);

        p2p_disc_timer_start(prot_handle, TIMER_RETRY_SET_CHAN, DEF_TIMEOUT_RETRY_SET_SCAN);
        break;
    }

    case P2P_DISC_EVENT_TMR_SET_CHAN_SWITCH:
    {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
            "%s: Aborting Pre_OnChan since channel switch is too slow. Reset too!\n", __func__);

        prot_handle->st_pre_onchan_abort = true;
        proc_event_reset(prot_handle);
        ASSERT(newstate == P2P_DISC_STATE_OFF);
        newstate = P2P_DISC_STATE_IDLE;
        break;
    }

    CASE_ALL_TX_ACTION_FRAMES
    {
        /* Call the common function to send action frame */
        ASSERT(event_data_len >= sizeof(sm_event_info));
        event = (sm_event_info *)event_data;

        if (p2p_prot_proc_common_tx_action_frame_event(prot_handle, event_type, event) != 0) {
            break;
        }

        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
            "%s: Error: unexpected event %s while on state Pre_OnChan.\n",
            __func__, event_name(event_type));

        if (event->tx_info->need_mini_find) {
            /* We do not know where this peer is. Needs to do a mini-find */
            start_minifind(prot_handle, event->event_type, event->tx_info, false, &newstate);
        }
        else {
            // Else we know which channel to send action frame.
            proc_event_tx_frame(prot_handle, event_type, event->tx_info, &newstate);
        }
        event->tx_info = NULL;
        break;
    }

    case P2P_DISC_EVENT_RESET:
    {
        proc_event_reset(prot_handle);
        prot_handle->st_pre_onchan_abort = true;
        ASSERT(newstate == P2P_DISC_STATE_OFF);
        newstate = P2P_DISC_STATE_IDLE;
        break;
    }

    case P2P_DISC_EVENT_END:
    {
        proc_event_end(prot_handle);
        prot_handle->st_pre_onchan_abort = true;
        ASSERT(newstate == P2P_DISC_STATE_OFF);
        newstate = P2P_DISC_STATE_OFF;
        break;
    }

    case P2P_DISC_EVENT_DISC_SCAN_REQ:      /* Device Discovery Scan request */
        proc_event_disc_scan_req(prot_handle, event, &newstate);
        break;

    case P2P_DISC_EVENT_DISC_SCAN_STOP:      /* Device Discovery Scan Stop */
    {
        proc_event_disc_scan_stop(prot_handle, &newstate);
        break;
    }

    case P2P_DISC_EVENT_TX_COMPLETE:
    {
        ASSERT(event_data_len >= sizeof(sm_event_info));
        proc_event_tx_complete(prot_handle, event, &newstate);
        break;
    }

    case P2P_DISC_EVENT_TMR_DISC_SCAN_EXPIRE:
        disc_scan_timer_expired(prot_handle);
        break;

    case P2P_DISC_EVENT_LISTEN_ST_DISC_REQ:
        /* Ignore this event and process when just before going into IDLE state */
        break;

    case P2P_DISC_EVENT_TMR_TX_REQ_EXPIRE:
    {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
            "%s: Tx Request Timer Expired. Abort tx action frame request. num_outstanding_tx=%d\n",
            __func__, prot_handle->num_outstanding_tx);

        ASSERT(prot_handle->num_outstanding_tx > 0);

        complete_outstanding_tx_req(prot_handle);
        break;
    }

    case P2P_DISC_EVENT_APP_IE_UPDATE:
        proc_event_app_ie_update(prot_handle, event);
        break;

    default:
        retval = false;
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                             "%s: called. rejected event_type=%s. ctx=0x%x.\n",
                             __func__, event_name(event_type), ctx);
        break;
    }

    if (newstate != P2P_DISC_STATE_OFF) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                             "%s: end of function and switching state to %s(%d).\n",
                             __func__, 
                             ieee80211_sm_get_state_name(prot_handle->hsm_handle, newstate),
                             newstate);
        ieee80211_sm_transition_to(prot_handle->hsm_handle, newstate);
    }

    return retval;
}   /* end of p2p_disc_state_PRE_ONCHAN_event() */

/*
 * ****************************** State P2P_DISC_STATE_ONCHAN ******************************
 * Description: Remain on this channel for certain action frames transaction like 
 *              3-way handshake during Group Formation phase, 2-way handshake during
 *              Invitation phase, and 2-way handshake during Provisioning phase.
 *              Note that the hardware is already on the actual channel.
 */

/* Exit function for this state */
static void
p2p_disc_state_ONCHAN_exit(void *ctx)
{
    wlan_p2p_prot_t         prot_handle = (wlan_p2p_prot_t)ctx;

    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_PROT,
        "%s: called. ctx=0x%x. num_outstanding_tx=%d\n", __func__, ctx, prot_handle->num_outstanding_tx);

    p2p_disc_timer_stop(prot_handle, TIMER_TX_RETRY);

    /* If a scan has being scheduled, then cancel it */
    p2p_prot_cancel_set_channel(prot_handle);
}

/* Entry function for this state */
static void
p2p_disc_state_ONCHAN_entry(void *ctx)
{
    wlan_p2p_prot_t                 prot_handle = (wlan_p2p_prot_t)ctx;
    int                             retval = 0;
    tx_event_info                   *tx_fr_info;

    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_PROT,
        "%s: called. ctx=0x%x. Freq=%d\n", __func__, ctx, 
        prot_handle->on_chan_dest_freq);

    ASSERT(prot_handle->curr_scan.set_chan_requested);

    if (prot_handle->retry_tx_info) {
        /* Check if we need to continue the tx action frame */
        ASSERT(prot_handle->curr_tx_info == NULL);
        if (prot_handle->on_chan_dest_freq == prot_handle->retry_tx_info->tx_freq) {
            /* We are on the right frequency */

            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                "%s: continue to retry tx. retry_tx_info=%pK\n", __func__, prot_handle->retry_tx_info);

            ieee80211_sm_dispatch(prot_handle->hsm_handle, P2P_DISC_EVENT_TMR_TX_RETRY, 0, NULL);
        }
        else {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                "%s: ERROR: retry tx has wrong freq (%d!=%d). retry_tx_info=%pK\n",
                __func__, prot_handle->on_chan_dest_freq, 
                prot_handle->retry_tx_info->tx_freq, prot_handle->retry_tx_info);
        }
    }
    else if (prot_handle->curr_tx_info != NULL) {

        /* Create and Send the action frame */
        tx_fr_info = prot_handle->curr_tx_info;
        prot_handle->curr_tx_info = NULL;

        if (tx_fr_info->tx_freq != prot_handle->on_chan_dest_freq) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: Error: on_chan freq (%d) != tx freq (%d)\n", 
                __func__, prot_handle->on_chan_dest_freq, tx_fr_info->tx_freq);
        }

        /* Send the action frame */
        p2p_prot_send_action_frame(prot_handle, tx_fr_info);
    }
    else {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
            "%s: Error: both prot_handle->curr_tx_info and ->retry_tx_info are NULL. Nothing to do in this state.\n", 
            __func__);
    }
}

/*
 * Note: We must call complete_tx_action_frame_event() to inform the caller about the Tx status.
 */
static bool
p2p_disc_state_ONCHAN_event(void *ctx, u_int16_t eventtype, u_int16_t event_data_len, void *event_data) 
{
    wlan_p2p_prot_t             prot_handle = (wlan_p2p_prot_t)ctx;
    bool                        retval = true;
    sm_event_info               *event;
    ieee80211_connection_event  event_type = (ieee80211_connection_event)eventtype;
    ieee80211_connection_state  newstate = P2P_DISC_STATE_OFF;

    if (validate_event_param(prot_handle, event_type, event_data, event_data_len) != EOK) {
        return -EINVAL;
    }
    event = (sm_event_info *)event_data;
    
    switch (event_type) {
    case P2P_DISC_EVENT_RX_MGMT:
    {
        /* Indicates that a desired peer is found. */
        wlan_p2p_event              *p2p_dev_event;     /* event from P2P Device */

        ASSERT(event_data_len >= sizeof(sm_event_info));
        p2p_dev_event = &event->p2p_dev_cb;

        proc_event_rx_mgmt(prot_handle, &p2p_dev_event->u.rx_frame, &newstate);
        break;
    }

    case P2P_DISC_EVENT_TX_COMPLETE:
    {
        st_listen_n_onchan_proc_tx_complete(prot_handle, event);
        break;
    }

    case P2P_DISC_EVENT_TMR_TX_RETRY:   /* Time to resend the action frame */
    {
        st_listen_n_onchan_proc_tx_retry_event(prot_handle, event);
        break;
    }

    CASE_ALL_TX_ACTION_FRAMES
    {
        ASSERT(event_data_len >= sizeof(sm_event_info));
        if (p2p_prot_proc_common_tx_action_frame_event(prot_handle, event_type, event) != 0) {
            break;
        }

        st_listen_n_onchan_proc_tx_action_fr_event(prot_handle, event, &newstate);
        break;
    }

    case P2P_DISC_EVENT_START_GO:           /* command to start Group Owner */
    case P2P_DISC_EVENT_START_CLIENT:       /* command to start P2P Client */
    {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
            "%s: Aborting Pre_OnChan since the GO/Client is starting.\n", __func__);

        prot_handle->scan_cancel_reason = SCAN_CANCEL_REASON_GO_CLIENT_STARTED;

        /* 
         * Cancel present remain_on_channel. Note that we are not waiting
         * for the cancel to complete but wait for SCAN_DEQUEUED or SCAN_COMPLETED events.
         */
        p2p_prot_cancel_set_channel(prot_handle);
        break;
    }

    case P2P_DISC_EVENT_SCAN_END:           /* End of channel scan */
    case P2P_DISC_EVENT_SCAN_DEQUEUE:       /* Scan request rejected and dequeued. */
        /* Check for cancelled scan confirmation */
        if (event_scan_end_from_cancel_scan(&prot_handle->curr_scan, event->scan_cb.scan_id)) {
            /* Don't do anything special */
        }
        break;

    case P2P_DISC_EVENT_SCAN_START:         /* start of channel scan */
    {
        /* Ignore all the scan events since we use wlan_p2p_set_channel() to switch channel */
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
            "%s: Ignore the scan event=%s on PRE_ONCHAN state.\n", __func__, event_name(event_type));
        break;
    }

    case P2P_DISC_EVENT_ON_CHAN_START:
    {
        wlan_p2p_event              *p2p_dev_event;     /* event from P2P Device */

        ASSERT(event_data_len >= sizeof(sm_event_info));
        p2p_dev_event = &event->p2p_dev_cb;

        /* check the ID to match. */
        if (p2p_dev_event->req_id != prot_handle->curr_scan.set_chan_req_id) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                "%s: Event O_CHAN_START but unmatched ID: %d != %d\n",
                __func__, p2p_dev_event->req_id, prot_handle->curr_scan.set_chan_req_id);
            break;
        }

        /* Note down the details of this ON_CHAN */
        prot_handle->curr_scan.set_chan_freq = p2p_dev_event->u.chan_start.freq;
        prot_handle->curr_scan.set_chan_started = true;
        prot_handle->curr_scan.set_chan_startime = OS_GET_TIMESTAMP();

        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
            "%s: Warn: unexpected OnChan_Start, ignored.\n", __func__);
        break;
    }

    case P2P_DISC_EVENT_ON_CHAN_END:
    {
        wlan_p2p_event              *p2p_dev_event;     /* event from P2P Device */

        ASSERT(event_data_len >= sizeof(sm_event_info));
        p2p_dev_event = &event->p2p_dev_cb;

        /* Check for cancelled set_on_channel confirmation */
        if (event_on_chan_end_is_from_cancel_scan(prot_handle, event->p2p_dev_cb.req_id)) {
            /* Don't do anything special */
        }

        /* check the ID to match. */
        if (p2p_dev_event->req_id != prot_handle->curr_scan.set_chan_req_id) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                "%s: Event O_CHAN_END but unmatched ID: %d != %d\n",
                __func__, p2p_dev_event->req_id, prot_handle->curr_scan.set_chan_req_id);
            break;
        }

        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
            "%s: end of remain_on_channel, goto IDLE state.\n",
            __func__);

        prot_handle->curr_scan.set_chan_started = false;

        ASSERT(prot_handle->curr_scan.set_chan_requested);
        prot_handle->curr_scan.set_chan_requested = false;

        st_listen_n_onchan_proc_on_chan_end(prot_handle, &newstate);
        break;
    }

    case P2P_DISC_EVENT_RESET:
    {
        proc_event_reset(prot_handle);
        ASSERT(newstate == P2P_DISC_STATE_OFF);
        newstate = P2P_DISC_STATE_IDLE;
        break;
    }

    case P2P_DISC_EVENT_END:
    {
        proc_event_end(prot_handle);
        ASSERT(newstate == P2P_DISC_STATE_OFF);
        newstate = P2P_DISC_STATE_OFF;
        break;
    }

    case P2P_DISC_EVENT_DISC_SCAN_REQ:      /* Device Discovery Scan request */
        ASSERT(event_data_len >= sizeof(sm_event_info));
        proc_event_disc_scan_req(prot_handle, event, &newstate);
        break;

    case P2P_DISC_EVENT_DISC_SCAN_STOP:      /* Device Discovery Scan Stop */
    {
        proc_event_disc_scan_stop(prot_handle, &newstate);
        break;
    }

    case P2P_DISC_EVENT_TMR_DISC_SCAN_EXPIRE:
        disc_scan_timer_expired(prot_handle);
        break;

    case P2P_DISC_EVENT_LISTEN_ST_DISC_REQ:
        /* Ignore this event and process when just before going into IDLE state */
        break;

    case P2P_DISC_EVENT_TMR_TX_REQ_EXPIRE:
    {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
            "%s: Tx Request Timer Expired. Abort tx action frame request. num_outstanding_tx=%d\n",
            __func__, prot_handle->num_outstanding_tx);

        ASSERT(prot_handle->num_outstanding_tx > 0);

        complete_outstanding_tx_req(prot_handle);
        break;
    }

    case P2P_DISC_EVENT_APP_IE_UPDATE:
        proc_event_app_ie_update(prot_handle, event);
        break;

    default:
        retval = false;
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                             "%s: called. rejected event_type=%s. ctx=0x%x.\n",
                             __func__, event_name(event_type), ctx);
        break;
    }

    if (newstate != P2P_DISC_STATE_OFF) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                             "%s: end of function and switching state to %s(%d).\n",
                             __func__, 
                             ieee80211_sm_get_state_name(prot_handle->hsm_handle, newstate),
                             newstate);
        ieee80211_sm_transition_to(prot_handle->hsm_handle, newstate);
    }

    return retval;
} //end of function p2p_disc_state_ONCHAN_event()

/*
 * ****************************** State P2P_DISC_STATE_DISCSCAN ******************************
 * Description: This state combines the Full Scan, Social Scan, and Listen into one state.
 * We will track the scan using current OperationScanType. At the end of each scan, we will 
 * figure out the next scan to start.
 */

static int
st_disc_scan_request_scan(
    wlan_p2p_prot_t             prot_handle, 
    p2p_prot_scan_type          curr_scan_type,
    disc_scan_event_info        *disc_info,
    ieee80211_connection_state  *newstate)
{
    int     retval = EOK;

    switch(curr_scan_type) {
    case P2P_PROT_SCAN_TYPE_FULL:
    case P2P_PROT_SCAN_TYPE_FIND_DEV:
    case P2P_PROT_SCAN_TYPE_PARTIAL:
    case P2P_PROT_SCAN_TYPE_SOCIAL:
        retval = ieee80211_p2p_prot_request_scan(&prot_handle->curr_scan, curr_scan_type);
        break;
    case P2P_PROT_SCAN_TYPE_LISTEN:
    {
        /* Request a listen state scan but still remains in DISCSCAN state */
        retval = p2p_prot_request_listen_scan(prot_handle, 0);
        if (retval != EOK) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                "%s listen state scan req failed\n", __func__);
            break;
        }
        else {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                "%s P2P_PROT_SCAN_TYPE_LISTEN : wlan_scan_start OK. returned set_chan_req_id=%d\n",
                __func__, prot_handle->curr_scan.set_chan_req_id);
        }
        break;
    }
    default:
        ASSERT(false);
        retval = -EPERM;
    }

    return retval;
}

/*
 * This function will check if a Listen scan is required during the discovery scan.
 * Return true if a listen state is required.
 */
static bool
st_disc_need_listen_state(wlan_p2p_prot_t         prot_handle,
                          wlan_p2p_prot_disc_type disc_type,
                          int                     num_find_cycle)
{
    switch(prot_handle->listen_state_disc)
    {
    case IEEE80211_P2P_PROT_LISTEN_STATE_DISC_NOT:
        /* Not discoverable mode */
        return false;
    case IEEE80211_P2P_PROT_LISTEN_STATE_DISC_AUTO:
        /* Auto mode: Only do listen state for every other cycle */
        /* STNG TODO: if there is a GO running, then we should not be discoverable. But for now, we do listen state */
        if (num_find_cycle & 0x01) {
            return true;
        }
        else {
            return false;
        }
    case IEEE80211_P2P_PROT_LISTEN_STATE_DISC_HIGH:
        /* Highly discoverable mode */
        return true;
    default:
        ASSERT(false);
        return false;
    }
}

/*
 * This function is called to start the next scan or on_channel request. It assumes
 * that the last scan stated in disc_info->curr_scan_type has completed.
 * Return true if this discovery scan has completed.
 * Note that this routine should be called from the DISCSCAN state only.
 *
 * If Discovery Type is scan-only, then just schedule a full scan and then stop. 
 * If discovery Type is find-only, then schedule a find phase which consist of 
 * alternating between Socal Scan and Listen until the timeout or 
 * NUM_FIND_PHASE_CYCLE is reached. 
 * If Discovery Type is auto, then schedule a full scan, followed by the find phase
 * until timeout or NUM_FIND_PHASE_CYCLE is reached.
 *
 */
static void
st_disc_scan_next_step(wlan_p2p_prot_t                  prot_handle, 
                       wlan_p2p_prot_disc_scan_param    *req_param, 
                       ieee80211_connection_state       *newstate)
{
    int                     retval = EOK;
    disc_scan_event_info    *disc_info;
    p2p_prot_scan_type      orig_scan_type;
    bool                    do_another_scan = true;

    ASSERT(prot_handle->curr_disc_scan_info != NULL);
    disc_info = prot_handle->curr_disc_scan_info;

    orig_scan_type = disc_info->curr_scan_type;

    if (disc_info->curr_scan_type == P2P_PROT_SCAN_TYPE_NONE) {
        /* Initialization some of the parameters in prot_handle->curr_scan */
        p2p_prot_scan_params    *prot_scan_param;
        u_int8_t                *buf;

        prot_scan_param = &prot_handle->curr_scan;

        buf = (u_int8_t *)disc_info;

        ASSERT(prot_scan_param->vap == prot_handle->vap);
        ASSERT(prot_scan_param->my_scan_requestor == prot_handle->my_scan_requestor);

        /* First step: do some initialization of the prot_handle->curr_scan structure */
        prot_scan_param->device_freq = disc_info->desired_peer_freq;
        prot_scan_param->partial_scan_chan_index = 0;

        prot_scan_param->scan_legy_netwk = disc_info->orig_req_param.scan_legy_netwk;

        /* Copy the IE's (if any) */
        if (disc_info->in_ie_buf_len > 0) {
            /* Note: We did not alloc the buffer for IE but reused the existing buffer inside req_param */
            prot_scan_param->probe_req_ie_len = disc_info->in_ie_buf_len;
            prot_scan_param->probe_req_ie_buf = &buf[disc_info->in_ie_offset];
        }
        else {
            prot_scan_param->probe_req_ie_len = 0;
        }

        if ((disc_info->orig_req_param.scan_type == wlan_p2p_prot_disc_scan_type_active) ||
            (disc_info->orig_req_param.scan_type == wlan_p2p_prot_disc_scan_type_auto))
        {
            prot_scan_param->active_scan = true;
        }
        else prot_scan_param->active_scan = false;

        prot_scan_param->probe_req_ssid = (ieee80211_ssid *)&buf[disc_info->in_p2p_wldcd_ssid_offset];
    }

    /* Figure out the next step to do */
    switch(req_param->disc_type) {
    case wlan_p2p_prot_disc_type_scan_only:
    {
        if (disc_info->curr_scan_type == P2P_PROT_SCAN_TYPE_NONE) {
            /* discovery scan using full scan only. Request a bunch of partial scans
             * and intermix with (possible) listen state. */

            /* Calculate the number of partial scan to do for cover all channels */
            disc_info->num_find_cycle = 1 + ((prot_handle->curr_scan.total_num_all_chans - 1)/P2P_PROT_PARTIAL_SCAN_NUM_CHAN);

            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                "%s: disc_type_scan_only: Just started. request P2P_PROT_SCAN_TYPE_PARTIAL next, num_find_cycle=%d\n",
                 __func__, disc_info->num_find_cycle);

            disc_info->curr_scan_type = P2P_PROT_SCAN_TYPE_PARTIAL;
        }
        else if (disc_info->curr_scan_type == P2P_PROT_SCAN_TYPE_PARTIAL)
        {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                "%s: disc_type_scan_only: Done the Partial scan. num_find_cycle=%d\n",
                 __func__, disc_info->num_find_cycle);

            disc_info->num_find_cycle--;

            if (disc_info->num_find_cycle > 0) {
                /* Do more cycles */
                if (st_disc_need_listen_state(prot_handle, req_param->disc_type, disc_info->num_find_cycle)) {
                    /* Do the listen state so that we are still discoverable */
                    disc_info->curr_scan_type = P2P_PROT_SCAN_TYPE_LISTEN; /* do the listen state next before repeating again */
                }
                else {
                    /* Repeat the partial scan again */
                    disc_info->curr_scan_type = P2P_PROT_SCAN_TYPE_PARTIAL; /* start with partial scan first */
                }
            }
            else {
                do_another_scan = false;
            }
        }
        else if (disc_info->curr_scan_type == P2P_PROT_SCAN_TYPE_LISTEN)
        {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                "%s: disc_type_scan_only: Done the Listen state. Repeat cycle. num_find_cycle=%d\n",
                 __func__, disc_info->num_find_cycle);

            /* Repeat the partial scan again */
            disc_info->curr_scan_type = P2P_PROT_SCAN_TYPE_PARTIAL; /* start with partial scan first */
        }
        else {
            ASSERT(false);  /* Invalid disc_info->curr_scan_type */
        }
        break;
    }
    case wlan_p2p_prot_disc_type_find_only:
    {
        /* discovery scan using find phase only. */
        if (disc_info->curr_scan_type == P2P_PROT_SCAN_TYPE_NONE) {
            /* discovery scan using find phase only. */
            disc_info->num_find_cycle = WLAN_P2P_PROT_DISC_NUM_FIND_PHASE_CYCLES;

            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                "%s: disc_type_find_only: Just started. request P2P_PROT_SCAN_TYPE_SOCIAL, num_cycle=%d\n",
                 __func__, disc_info->num_find_cycle);

            /* Request a 3-chan social scan */
            disc_info->curr_scan_type = P2P_PROT_SCAN_TYPE_SOCIAL;
        }
        else if (disc_info->curr_scan_type == P2P_PROT_SCAN_TYPE_SOCIAL) {
            /* switch to the listen */

            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                "%s: disc_type_find_only: finished social, requesting P2P_PROT_SCAN_TYPE_LISTEN, num_cycle=%d\n",
                 __func__, disc_info->num_find_cycle);

            disc_info->curr_scan_type = P2P_PROT_SCAN_TYPE_LISTEN;
        }
        else if (disc_info->curr_scan_type == P2P_PROT_SCAN_TYPE_LISTEN) {
            /* Check more many iterations we have done */

            if (disc_info->num_find_cycle > 0) {
                /* Do any cycle */
                disc_info->num_find_cycle--;

                IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                    "%s: disc_type_find_only: finished listen, requesting P2P_PROT_SCAN_TYPE_SOCIAL, num_cycle=%d\n",
                     __func__, disc_info->num_find_cycle);

                /* Request a 3-chan social scan */
                disc_info->curr_scan_type = P2P_PROT_SCAN_TYPE_SOCIAL;
            }
            else {

                IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                    "%s: disc_type_find_only: all completed. complete request\n",
                     __func__);

                do_another_scan = false;  /* discovery is completed */
            }
        }
        else {
            ASSERT(false);  /* Invalid disc_info->curr_scan_type */
        }
        break;
    }

    case wlan_p2p_prot_disc_type_auto:
    {
        /* discovery scan using automatic. We will start with a social scan (multiple times), 
         * then full scan, then social-listen scans (multiple times). */
        if (disc_info->curr_scan_type == P2P_PROT_SCAN_TYPE_NONE) {

            /* Calculate the number of partial scan to do for cover all channels */
            disc_info->num_find_cycle = 1 + ((prot_handle->curr_scan.total_num_all_chans - 1)/P2P_PROT_PARTIAL_SCAN_NUM_CHAN);

            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                "%s: disc_type_auto: Just started. request P2P_PROT_SCAN_TYPE_SOCIAL next, num_find_cycle=%d\n",
                 __func__, disc_info->num_find_cycle);

            disc_info->curr_scan_type = P2P_PROT_SCAN_TYPE_SOCIAL; /* start with social scan first */
        }
        else if (disc_info->curr_scan_type == P2P_PROT_SCAN_TYPE_SOCIAL)
        {
            if (disc_info->in_social_chans_only) {
                /* We scan social channels only and skip the other non social channels. */
                IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                    "%s: disc_type_auto: Done the social scan and in_social_chans_only==true. num_find_cycle=%d\n",
                     __func__, disc_info->num_find_cycle);

                disc_info->num_find_cycle--;

                if (disc_info->num_find_cycle > 0) {
                    /* Repeat the entire scan pattern again */
                    if (st_disc_need_listen_state(prot_handle, req_param->disc_type, disc_info->num_find_cycle)) {
                        /* Do the listen state so that we are still discoverable */
                        disc_info->curr_scan_type = P2P_PROT_SCAN_TYPE_LISTEN; /* do the listen state next before repeating again */
                    }
                    else {
                        /* Repeat the social scan again */
                        disc_info->curr_scan_type = P2P_PROT_SCAN_TYPE_SOCIAL; /* start with social scan first */
                    }
                }
                else {
                    do_another_scan = false;
                }
            }
            else {
                /* Else goto the next step of doing partial scans */
                IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                    "%s: disc_type_auto: Done social scan. Do the Partial scan. num_find_cycle=%d\n",
                     __func__, disc_info->num_find_cycle);

                disc_info->curr_scan_type = P2P_PROT_SCAN_TYPE_PARTIAL;
            }
        }
        else if (disc_info->curr_scan_type == P2P_PROT_SCAN_TYPE_PARTIAL)
        {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                "%s: disc_type_auto: Done the Partial scan. num_find_cycle=%d\n",
                 __func__, disc_info->num_find_cycle);

            disc_info->num_find_cycle--;

            if (disc_info->num_find_cycle > 0) {
                /* Do more cycles */
                if (st_disc_need_listen_state(prot_handle, req_param->disc_type, disc_info->num_find_cycle)) {
                    /* Do the listen state so that we are still discoverable */
                    disc_info->curr_scan_type = P2P_PROT_SCAN_TYPE_LISTEN; /* do the listen state next before repeating again */
                }
                else {
                    /* Repeat the social scan again */
                    disc_info->curr_scan_type = P2P_PROT_SCAN_TYPE_SOCIAL; /* start with social scan first */
                }
            }
            else {
                do_another_scan = false;
            }
        }
        else if (disc_info->curr_scan_type == P2P_PROT_SCAN_TYPE_LISTEN)
        {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                "%s: disc_type_auto: Done the Listen state. Repeat cycle. num_find_cycle=%d\n",
                 __func__, disc_info->num_find_cycle);

            /* Repeat the social scan again */
            disc_info->curr_scan_type = P2P_PROT_SCAN_TYPE_SOCIAL; /* start with social scan first */
        }
        else {
            ASSERT(false);  /* Invalid disc_info->curr_scan_type */
        }
        break;
    }   /* case wlan_p2p_prot_disc_type_auto */

    case wlan_p2p_prot_disc_type_find_dev:
    {
        /* discovery scan to find a device with a known listen frequency.
         * We will start with scan on the listen frequency, then social scan, then
         * partial scan. We repeat these steps multiple times until all the channels
         * are scanned.
         */
        if (disc_info->curr_scan_type == P2P_PROT_SCAN_TYPE_NONE) {

            ASSERT(disc_info->desired_peer_freq != 0);

            /* Calculate the number of partial scan to do for cover all channels */
            disc_info->num_find_cycle = 1 + ((prot_handle->curr_scan.total_num_all_chans - 1)/P2P_PROT_PARTIAL_SCAN_NUM_CHAN);

            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                "%s: disc_type_find_dev: Just started. request P2P_PROT_SCAN_TYPE_FIND_DEV next, num_find_cycle=%d\n",
                 __func__, disc_info->num_find_cycle);

            disc_info->curr_scan_type = P2P_PROT_SCAN_TYPE_FIND_DEV; /* start with scan to find device first */
        }
        else if (disc_info->curr_scan_type == P2P_PROT_SCAN_TYPE_FIND_DEV)
        {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                "%s: disc_type_find_dev: Done find device scan. Do the Social scan. num_find_cycle=%d\n",
                 __func__, disc_info->num_find_cycle);

            disc_info->curr_scan_type = P2P_PROT_SCAN_TYPE_SOCIAL;
        }
        else if (disc_info->curr_scan_type == P2P_PROT_SCAN_TYPE_SOCIAL)
        {
            if (disc_info->in_social_chans_only) {
                /* We scan social channels only and skip the other non social channels. */
                IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                    "%s: disc_type_find_dev: Done the social scan and in_social_chans_only==true. num_find_cycle=%d\n",
                     __func__, disc_info->num_find_cycle);

                disc_info->num_find_cycle--;

                if (disc_info->num_find_cycle > 0) {
                    /* Repeat the entire scan pattern again */
                    if (st_disc_need_listen_state(prot_handle, req_param->disc_type, disc_info->num_find_cycle)) {
                        /* Do the listen state so that we are still discoverable */
                        disc_info->curr_scan_type = P2P_PROT_SCAN_TYPE_LISTEN; /* do the listen state next before repeating again */
                    }
                    else {
                        /* Repeat the find device scan again */
                        disc_info->curr_scan_type = P2P_PROT_SCAN_TYPE_FIND_DEV; /* start with find device scan first */
                    }
                }
                else {
                    do_another_scan = false;
                }
            }
            else {
                /* Else goto the next step of doing partial scans */
                IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                    "%s: disc_type_find_dev: Done social scan. Do the Partial scan. num_find_cycle=%d\n",
                     __func__, disc_info->num_find_cycle);

                disc_info->curr_scan_type = P2P_PROT_SCAN_TYPE_PARTIAL;
            }
        }
        else if (disc_info->curr_scan_type == P2P_PROT_SCAN_TYPE_PARTIAL)
        {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                "%s: disc_type_find_dev: Done the Partial scan. num_find_cycle=%d\n",
                 __func__, disc_info->num_find_cycle);

            disc_info->num_find_cycle--;

            if (disc_info->num_find_cycle > 0) {
                /* Repeat the entire scan pattern again */
                if (st_disc_need_listen_state(prot_handle, req_param->disc_type, disc_info->num_find_cycle)) {
                    /* Do the listen state so that we are still discoverable */
                    disc_info->curr_scan_type = P2P_PROT_SCAN_TYPE_LISTEN; /* do the listen state next before repeating again */
                }
                else {
                    /* Repeat the find device scan again */
                    disc_info->curr_scan_type = P2P_PROT_SCAN_TYPE_FIND_DEV; /* start with find device scan first */
                }
            }
            else {
                do_another_scan = false;
            }
        }
        else if (disc_info->curr_scan_type == P2P_PROT_SCAN_TYPE_LISTEN)
        {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                "%s: disc_type_find_dev: Done the Listen state. Repeat cycle. num_find_cycle=%d\n",
                 __func__, disc_info->num_find_cycle);

            /* Repeat the find device scan again */
            disc_info->curr_scan_type = P2P_PROT_SCAN_TYPE_FIND_DEV; /* start with find device scan first */
        }
        else {
            ASSERT(false);  /* Invalid disc_info->curr_scan_type */
        }
        break;
    }   /* case wlan_p2p_prot_disc_type_find_dev */

    default:
        ASSERT(false);
    }

    if (do_another_scan) {
        retval = st_disc_scan_request_scan(prot_handle, disc_info->curr_scan_type, disc_info, newstate);
        if (retval != EOK) {
            /* Error: could not request the scan. Retry later */
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: Warning: st_disc_scan_request_scan(%d) failed (ret=0x%x).  Orig ScanType=%d, disc_type=%d, Retry later.\n",
                 __func__, disc_info->curr_scan_type, retval, orig_scan_type, req_param->disc_type);

            disc_info->curr_scan_type = orig_scan_type;
            p2p_disc_timer_start(prot_handle, TIMER_RETRY_SET_SCAN, DEF_TIMEOUT_RETRY_SET_SCAN);
        }
    }
    else {
        /* We are done */
        /* Complete this request and goto IDLE */
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
            "%s: disc_type_auto: all completed. complete request\n",
             __func__);

        /* discovery is completed */
        ASSERT(*newstate == P2P_DISC_STATE_OFF);

        /* Complete this request to the caller */
        st_disc_scan_complete_req(prot_handle, EOK, newstate);

        if (*newstate == P2P_DISC_STATE_OFF) {
            /* Unless the state is changed, we should switch back to state IDLE
             * when discovery scan is completed. */
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                "%s: Disc scan done. Switch back to IDLE state.\n",
                 __func__);

            ASSERT(prot_handle->num_outstanding_tx == 0);

            *newstate = P2P_DISC_STATE_IDLE;
        }
        else {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                "%s: Disc scan done. But switch back to %s(%d) state.\n",
                 __func__, ieee80211_sm_get_state_name(prot_handle->hsm_handle, *newstate), *newstate);
        }
    }

    return;
}

/*
 * Exit function for this state. Note that we could be exiting this state
 * to go to the LISTEN state OR that the discovery scan has completed.
 */
static void
p2p_disc_state_DISCSCAN_exit(void *ctx)
{
    wlan_p2p_prot_t             prot_handle = (wlan_p2p_prot_t)ctx;
    int                         retval;

    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_PROT,
        "%s: called. ctx=0x%x.\n", __func__, ctx);

    /* Stop the timer TIMER_DISC_SCAN_EXPIRE (may have already expired) */
    p2p_disc_timer_stop(prot_handle, TIMER_DISC_SCAN_EXPIRE);

    /* Stop the timer TIMER_RETRY_SET_SCAN */
    p2p_disc_timer_stop(prot_handle, TIMER_RETRY_SET_SCAN);

    if (prot_handle->curr_scan.scan_requested) {
        /* Cancel this scan request but do not wait for it to finish */
        ASSERT(prot_handle->curr_scan.my_scan_requestor == prot_handle->my_scan_requestor);
        retval = ieee80211_p2p_prot_cancel_scan(&prot_handle->curr_scan);
        if (retval != EOK) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                "%s: warning: ieee80211_p2p_prot_cancel_scan() returns 0x%x.\n", __func__, retval);
        }
        prot_handle->curr_scan.scan_requested = false;
    }

    /* If a scan has being scheduled, then cancel it */
    p2p_prot_cancel_set_channel(prot_handle);

    /* Complete the current device discovery request (if any) */
    if (prot_handle->curr_disc_scan_info) {
        /* Complete this request with errors */
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
            "%s: warning: should have completed this disc scan earlier. Abort this scan.\n",
            __func__);
        p2p_disc_scan_abort(prot_handle);
    }
}

/* 
 * Entry function for this state. Note that this could be the start of discovery scan or
 * returning from the LISTEN state.
 */
static void
p2p_disc_state_DISCSCAN_entry(void *ctx)
{
    wlan_p2p_prot_t                 prot_handle = (wlan_p2p_prot_t)ctx;

    ASSERT(prot_handle->num_outstanding_tx == 0);
    ASSERT(prot_handle->curr_disc_scan_info);

    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_PROT,
        "%s: called. curr_scan_type =%d\n", __func__, prot_handle->curr_disc_scan_info->curr_scan_type);

    /* Post a message to this state machine to initiate the scan */
    ieee80211_sm_dispatch(prot_handle->hsm_handle, P2P_DISC_EVENT_DISC_SCAN_INIT,
                          0, NULL);
}

/*
 * Note: We must call complete_tx_action_frame_event() to inform the caller about the Tx status.
 */
static bool
p2p_disc_state_DISCSCAN_event(void *ctx, u_int16_t eventtype, u_int16_t event_data_len, void *event_data) 
{
    wlan_p2p_prot_t             prot_handle = (wlan_p2p_prot_t)ctx;
    bool                        retval = true;
    sm_event_info               *event = NULL;
    ieee80211_connection_event  event_type = (ieee80211_connection_event)eventtype;
    ieee80211_connection_state  newstate = P2P_DISC_STATE_OFF;

    if (validate_event_param(prot_handle, event_type, event_data, event_data_len) != EOK) {
        return -EINVAL;
    }
    event = (sm_event_info *)event_data;
    
    switch (event_type) {
    case P2P_DISC_EVENT_RX_MGMT:
    {
        /* Indicates that a desired peer is found. */
        wlan_p2p_event              *p2p_dev_event;     /* event from P2P Device */

        ASSERT(event_data_len >= sizeof(sm_event_info));
        p2p_dev_event = &event->p2p_dev_cb;

        proc_event_rx_mgmt(prot_handle, &p2p_dev_event->u.rx_frame, &newstate);

        if ((prot_handle->curr_tx_info == NULL) &&
            (prot_handle->curr_disc_scan_info == NULL))
        {
            /* No outstanding transmit or discovery scan. Return back to IDLE state */
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                "%s: EVENT_RX_MGMT: no outstanding disc scan or tx, return back to IDLE state.\n",
                __func__);
            ieee80211_sm_transition_to(prot_handle->hsm_handle, P2P_DISC_STATE_IDLE);
        }
        break;
    }

    case P2P_DISC_EVENT_TX_COMPLETE:
    {
        ASSERT(event_data_len >= sizeof(sm_event_info));
        event = (sm_event_info *)event_data;
        proc_event_tx_complete(prot_handle, event, &newstate);
        break;
    }

    CASE_ALL_TX_ACTION_FRAMES
    {
        /* Call the common function to send action frame */
        ASSERT(event_data_len >= sizeof(sm_event_info));
        event = (sm_event_info *)event_data;

        if (p2p_prot_proc_common_tx_action_frame_event(prot_handle, event_type, event) != 0) {
            break;
        }

        if (event->tx_info->need_mini_find) {
            /* We do not know where this peer is. Needs to do a mini-find */
            start_minifind(prot_handle, event->event_type, event->tx_info, false, &newstate);
        }
        else {
            // Else we know which channel to send action frame.
            proc_event_tx_frame(prot_handle, event_type, event->tx_info, &newstate);
        }
        event->tx_info = NULL;
        break;
    }

    case P2P_DISC_EVENT_START_GO:           /* command to start Group Owner */
    case P2P_DISC_EVENT_START_CLIENT:       /* command to start P2P Client */
    {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
            "%s: Note: the GO/Client is starting.\n", __func__);
#if 0
        prot_handle->scan_cancel_reason = SCAN_CANCEL_REASON_GO_CLIENT_STARTED;

        /* 
         * Cancel present remain_on_channel. Note that we are not waiting
         * for the cancel to complete but wait for SCAN_DEQUEUED or SCAN_COMPLETED events.
         */
        p2p_prot_cancel_set_channel(prot_handle);
        break;
#endif //0
    }

    case P2P_DISC_EVENT_SCAN_START:         /* start of channel scan */
    {
        ieee80211_scan_event    *scan_event;
        disc_scan_event_info    *disc_info;

        ASSERT(event_data_len >= sizeof(sm_event_info));
        event = (sm_event_info *)event_data;

        scan_event = &event->scan_cb;

        /* check the ID to match. */
        if (!check_scan_event_id_matched(prot_handle, scan_event)) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                "%s: Rejected event=%s due to mismatched ID. scan=%d, mine=%d\n",
                __func__, event_name(event_type), scan_event->scan_id, prot_handle->curr_scan.request_id);
            break;
        }

        if (!prot_handle->curr_scan.scan_requested) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: Error: unexpected scan start.\n", __func__);
        }

        disc_info = prot_handle->curr_disc_scan_info;

        if (disc_info->curr_scan_started) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: Warning: Scan has already started, type=%d.\n", __func__, disc_info->curr_scan_type);
        }
        else {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                "%s: Scan_Start for type=%d.\n", __func__, disc_info->curr_scan_type);
        }
        disc_info->curr_scan_started = true;

        break;
    }

    case P2P_DISC_EVENT_ON_CHAN_START:
    {
        wlan_p2p_event              *p2p_dev_event;     /* event from P2P Device */
        disc_scan_event_info        *disc_info;

        ASSERT(event_data_len >= sizeof(sm_event_info));
        event = (sm_event_info *)event_data;
        p2p_dev_event = &event->p2p_dev_cb;

        /* check the ID to match. */
        if (p2p_dev_event->req_id != prot_handle->curr_scan.set_chan_req_id) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                "%s: Event O_CHAN_END but unmatched ID: %d != %d\n",
                __func__, p2p_dev_event->req_id, prot_handle->curr_scan.set_chan_req_id);
            break;
        }

        if (!prot_handle->curr_scan.set_chan_requested) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: Error: unexpected ON_CHAN start.\n", __func__);
            /* STNG TODO: should we cancel this scan so that it does not waste off-channel time? */
        }

        prot_handle->curr_scan.set_chan_freq = p2p_dev_event->u.chan_start.freq;
        prot_handle->curr_scan.set_chan_started = true;
        prot_handle->curr_scan.set_chan_startime = OS_GET_TIMESTAMP();

        disc_info = prot_handle->curr_disc_scan_info;

        if (disc_info->on_chan_started) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: Warning: ON_CHAN has already started, type=%d.\n", __func__, disc_info->curr_scan_type);
        }
        else {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                "%s: ON_CHAN_Start for type=%d.\n", __func__, disc_info->curr_scan_type);
        }
        disc_info->on_chan_started = true;

        break;
    }

    case P2P_DISC_EVENT_ON_CHAN_END:
    {
        wlan_p2p_event                  *p2p_dev_event;     /* event from P2P Device */
        wlan_p2p_prot_disc_scan_param   *req_param;
        disc_scan_event_info            *disc_info;

        ASSERT(event_data_len >= sizeof(sm_event_info));
        event = (sm_event_info *)event_data;
        p2p_dev_event = &event->p2p_dev_cb;

        /* Check for cancelled set_on_channel confirmation */
        if (event_on_chan_end_is_from_cancel_scan(prot_handle, event->p2p_dev_cb.req_id)) {
            /* Don't do anything special */
        }

        /* check the ID to match. */
        if (p2p_dev_event->req_id != prot_handle->curr_scan.set_chan_req_id) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                "%s: Event O_CHAN_END but unmatched ID: %d != %d\n",
                __func__, p2p_dev_event->req_id, prot_handle->curr_scan.set_chan_req_id);
            break;
        }

        if (!prot_handle->curr_scan.set_chan_requested) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: Error: unexpected ON_CHAN end.\n", __func__);
        }
        
        prot_handle->curr_scan.set_chan_started = false;

        disc_info = prot_handle->curr_disc_scan_info;

        if (!disc_info->on_chan_started) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: Warning: Unexpected ON_CHAN_END since we did not received the O_CHAN_START.\n",
                __func__);
        }

        prot_handle->curr_scan.set_chan_requested = false;
        disc_info->on_chan_started = false;
        req_param = &prot_handle->curr_disc_scan_info->orig_req_param;

        st_disc_scan_next_step(prot_handle, req_param, &newstate);
        break;
    }

    case P2P_DISC_EVENT_SCAN_END:           /* End of channel scan */
    case P2P_DISC_EVENT_SCAN_DEQUEUE:       /* Scan request rejected and dequeued. */
    {
        ieee80211_scan_event            *scan_event;
        wlan_p2p_prot_disc_scan_param   *req_param;
        disc_scan_event_info            *disc_info;

        ASSERT(event_data_len >= sizeof(sm_event_info));
        event = (sm_event_info *)event_data;
        scan_event = &event->scan_cb;

        /* Check for cancelled scan confirmation */
        if (event_scan_end_from_cancel_scan(&prot_handle->curr_scan, event->scan_cb.scan_id)) {
            /* Don't do anything special */
        }

        /* check the ID to match. */
        if (!check_scan_event_id_matched(prot_handle, scan_event)) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                "%s: Rejected event=%s due to mismatched ID. scan=%d, mine=%d\n",
                __func__, event_name(event_type), scan_event->scan_id, prot_handle->curr_scan.request_id);
            break;
        }

        if (!prot_handle->curr_scan.scan_requested) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: Error: unexpected ON_CHAN end.\n", __func__);
        }
        
        disc_info = prot_handle->curr_disc_scan_info;

        if (!disc_info->curr_scan_started) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: Warning: Scan has not started, type=%d. But got scan end\n", __func__, disc_info->curr_scan_type);
        }
        else {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                "%s: Scan_End for type=%d.\n", __func__, disc_info->curr_scan_type);
        }
        ASSERT(prot_handle->curr_disc_scan_info);
        disc_info = prot_handle->curr_disc_scan_info;

        if (!disc_info->curr_scan_started) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: Warning: Unexpected ON_SCAN_END/DEQUEUE since we did not received the SCAN_START.\n",
                __func__);
        }

        disc_info->curr_scan_started = false;
        prot_handle->curr_scan.scan_requested = false;

        disc_info = prot_handle->curr_disc_scan_info;
        req_param = &prot_handle->curr_disc_scan_info->orig_req_param;

        st_disc_scan_next_step(prot_handle, req_param, &newstate);

        break;
    }

    case P2P_DISC_EVENT_RESET:
    {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
            "%s: reset, abort this scan.\n",
            __func__);
        proc_event_reset(prot_handle);
        ASSERT(newstate == P2P_DISC_STATE_OFF);
        newstate = P2P_DISC_STATE_IDLE;
        break;
    }

    case P2P_DISC_EVENT_END:
    {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
            "%s: shut down, abort this scan.\n",
            __func__);
        proc_event_end(prot_handle);
        ASSERT(newstate == P2P_DISC_STATE_OFF);
        newstate = P2P_DISC_STATE_OFF;
        break;
    }

    case P2P_DISC_EVENT_DISC_SCAN_REQ:      /* Device Discovery Scan request */
    {
        ASSERT(event_data_len >= sizeof(sm_event_info));
        event = (sm_event_info *)event_data;

        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
            "%s: Error: got Discovery Scan request even though we are already in DISCSCAN state.\n",
            __func__);
        proc_event_disc_scan_req(prot_handle, event, &newstate);
        break;
    }

    case P2P_DISC_EVENT_DISC_SCAN_STOP:      /* Device Discovery Scan Stop */
    {
        proc_event_disc_scan_stop(prot_handle, &newstate);
        break;
    }

    case P2P_DISC_EVENT_DISC_SCAN_INIT:
    case P2P_DISC_EVENT_TMR_RETRY_SET_SCAN:
    {
        /* Earlier attempts to request a scan or remain_on_channel failed. Retry now */
        wlan_p2p_prot_disc_scan_param   *req_param;

        ASSERT(prot_handle->curr_disc_scan_info != NULL);
        req_param = &prot_handle->curr_disc_scan_info->orig_req_param;

        st_disc_scan_next_step(prot_handle, req_param, &newstate);

        break;
    }

    case P2P_DISC_EVENT_TMR_DISC_SCAN_EXPIRE:
        /* Failed the current discovery request and return back to IDLE */
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
            "%s: Discovery took too long. Timeout event=%s arrives\n",
            __func__, event_name(event_type));
        disc_scan_timer_expired(prot_handle);
        ASSERT(prot_handle->num_outstanding_tx == 0);
        /* Unless the next state is specified, we should go back to IDLE */
        newstate = P2P_DISC_STATE_IDLE;
        break;

    case P2P_DISC_EVENT_LISTEN_ST_DISC_REQ:
        /* Ignore this event and process when just before going into IDLE state */
        break;

    case P2P_DISC_EVENT_TMR_TX_REQ_EXPIRE:
    {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
            "%s: ERROR: should not have this P2P_DISC_EVENT_TMR_TX_REQ_EXPIRE event in this state. num_outstanding_tx=%d\n",
            __func__, prot_handle->num_outstanding_tx);
        ASSERT(false);
        complete_outstanding_tx_req(prot_handle);
        break;
    }

    case P2P_DISC_EVENT_APP_IE_UPDATE:
        proc_event_app_ie_update(prot_handle, event);
        break;

    default:
        retval = false;
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                             "%s: called. rejected event_type=%s. ctx=0x%x.\n",
                             __func__, event_name(event_type), ctx);
        break;
    }

    if (newstate != P2P_DISC_STATE_OFF) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                             "%s: end of function and switching state to %s(%d).\n",
                             __func__, 
                             ieee80211_sm_get_state_name(prot_handle->hsm_handle, newstate),
                             newstate);
        ieee80211_sm_transition_to(prot_handle->hsm_handle, newstate);
    }

    return retval;
} /* end of function p2p_disc_state_DISCSCAN_event() */

/************************* End of DISCSCAN state ******************************/

/********************** Start of State Machine support routines *************************/

/* State Machine for the P2P Protocol Discovery */
ieee80211_state_info p2p_prot_disc_sm_info[] = {
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
        (u_int8_t) P2P_DISC_STATE_##st_name,        \
        (u_int8_t) IEEE80211_HSM_STATE_NONE,        \
        (u_int8_t) IEEE80211_HSM_STATE_NONE,        \
        false,                                      \
        "P2P_DISC_" #st_name,                       \
        p2p_disc_state_##st_name##_entry,           \
        p2p_disc_state_##st_name##_exit,            \
        p2p_disc_state_##st_name##_event            \
    }
    
    STATE_INFO(OFF),
    STATE_INFO(IDLE),
    STATE_INFO(PRE_LISTEN),
    STATE_INFO(LISTEN),
    STATE_INFO(PRE_ONCHAN),
    STATE_INFO(ONCHAN),
    STATE_INFO(DISCSCAN),

#undef STATE_INFO
};

static void p2p_prot_disc_sm_debug_print (void *ctx, const char *fmt, ...) 
{
    char                tmp_buf[256];
    va_list             ap;
    wlan_p2p_prot_t     prot_handle = (wlan_p2p_prot_t)ctx;
     
    va_start(ap, fmt);
    vsnprintf (tmp_buf, 256, fmt, ap);
    va_end(ap);

    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT, tmp_buf);
}

/********************** End of State Machine support routines *************************/

/********************** Start of routines that called from external *************************/
/* Note: since these functions are called from external sources, there is no synchronization. */

/*
 * Registered Scan Event Handler.
 */
static VOID
p2p_prot_scan_evhandler(wlan_if_t vaphandle, ieee80211_scan_event *event, void *arg)
{
    wlan_p2p_prot_t     prot_handle = (wlan_p2p_prot_t)arg;
    sm_event_info       sm_event;

    ASSERT(prot_handle != NULL);

    /* Check that the request ID, event->requestor, is from our device */
    if (prot_handle->my_scan_requestor != event->requestor) {
        /* Scan request is not from me. Ignore it. */
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                          "%s: Scan event ignored, mismatch Request ID. given=%d, mine=%d\n",
                          __func__, event->requestor, prot_handle->my_scan_requestor);
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
    sm_event.event_type = P2P_DISC_EVENT_NULL;

    switch (event->type)
    {
    case IEEE80211_SCAN_STARTED:
        sm_event.event_type = P2P_DISC_EVENT_SCAN_START;
        ieee80211_sm_dispatch(prot_handle->hsm_handle, sm_event.event_type, sizeof(sm_event_info), &sm_event);
        break;

    case IEEE80211_SCAN_COMPLETED:
        sm_event.event_type = P2P_DISC_EVENT_SCAN_END;
        ieee80211_sm_dispatch(prot_handle->hsm_handle, sm_event.event_type, sizeof(sm_event_info), &sm_event);
        break;

    case IEEE80211_SCAN_DEQUEUED:
        sm_event.event_type = P2P_DISC_EVENT_SCAN_DEQUEUE;
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
 * Common routine to process the event to receive an action frame.  
 */

/*
 * Validate the P2P GO Negotiation Request frame.
 * ieee80211_p2p_parse_free() when the parsed data is not needed anymore.
 */
static bool validate_p2p_go_neg_req(
    wlan_p2p_prot_t prot_handle, struct p2p_parsed_ie *msg, 
    u_int8_t *data, const u_int8_t *sa, u_int32_t len)
{
    if (!msg->capability) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
            "%s: Mandatory Capability attribute missing from GO Neg Req from " MACSTR "\n", 
            __func__, MAC2STR(sa));
        /* We still let it continue */
    }
    
    if (!msg->go_intent) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
            "%s: Mandatory Intent attribute missing from GO Neg Req from " MACSTR "\n", 
            __func__, MAC2STR(sa));
        /* We still let it continue */
    }

    if ((*msg->go_intent >> 1) > P2P_MAX_GO_INTENT) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
            "%s: Invalid GO Intent value (%u) from GO Neg Req from " MACSTR "\n", 
            __func__, *msg->go_intent >> 1, MAC2STR(sa));
        goto fail;
    }
    
    if (!msg->config_timeout) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
            "%s: Mandatory Configuration Timeout attribute missing from GO Neg Req from " MACSTR "\n", 
            __func__, MAC2STR(sa));
        /* We still let it continue */
    }
    
    if (!msg->listen_channel) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
            "%s: No Listen Channel attribute from GO Neg Req from " MACSTR ". Drop req.\n", 
            __func__, MAC2STR(sa));
        goto fail;
    }

    if (!msg->operating_channel) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
            "%s: No Operating Channel attribute from GO Neg Req from " MACSTR ". Drop req.\n", 
            __func__, MAC2STR(sa));
        goto fail;
    }

    if (!msg->channel_list) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
            "%s: No Channel List attribute from GO Neg Req from " MACSTR ". Drop req.\n", 
            __func__, MAC2STR(sa));
        goto fail;
    }

    if (!msg->intended_addr) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
            "%s: No Intended P2P Interface Address attribute from GO Neg Req from " MACSTR ". Drop req.\n", 
            __func__, MAC2STR(sa));
        goto fail;
    }

    if (!msg->p2p_device_info) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
            "%s: No P2P Device Info attribute from GO Neg Req from " MACSTR ". Drop req.\n", 
            __func__, MAC2STR(sa));
        goto fail;
    }
    
    if (OS_MEMCMP(msg->p2p_device_addr, sa, IEEE80211_ADDR_LEN) != 0) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
            "%s: Unexpected GO Negotiation Request SA=" MACSTR " != dev_addr=" MACSTR " Drop req.\n", 
            __func__, MAC2STR(sa),  MAC2STR(msg->p2p_device_addr));
        goto fail;
    }

    if (msg->status && *msg->status) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
            "%s: Unexpected Status attribute (%d) in GO Neg Req\n",
             __func__, *msg->status);
        goto fail;
    }

    return true;

fail:
    /* This received GO Negotiation Req frame is rejected by the driver. But we
     * will still indicate to the OS stack. */
    return false;
}

/*
 * Validate the P2P GO Negotiation Response frame.
 * ieee80211_p2p_parse_free() when the parsed data is not needed anymore.
 */
static bool validate_p2p_go_neg_resp(
    wlan_p2p_prot_t prot_handle, struct p2p_parsed_ie *msg, 
    u_int8_t *data, const u_int8_t *sa, u_int32_t len)
{
    bool    ok_status = false;

    do {
        if (!msg->status) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: P2P: No Status attribute received\n", __func__);
            break;
        }
        if (*msg->status) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: P2P: GO Negotiation rejected: status %d\n",
                __func__, *msg->status);
            ok_status = true;   /* The parsing of IE is OK */
            break;
        }

        if (!msg->capability) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: Mandatory Capability attribute missing from GO Neg Resp from " MACSTR "\n", 
                __func__, MAC2STR(sa));
            /* We still let it continue */
        }

        if (!msg->p2p_device_info) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: No P2P Device Info attribute from GO Neg Resp from " MACSTR ".\n", 
                __func__, MAC2STR(sa));
            /* We still let it continue */
        }

        if (!msg->intended_addr) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: No Intended P2P Interface Address attribute from GO Neg Resp from " MACSTR ". Drop req.\n", 
                __func__, MAC2STR(sa));
            break;
        }

        if (!msg->go_intent) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: Mandatory Intent attribute missing from GO Neg Resp from " MACSTR "\n", 
                __func__, MAC2STR(sa));
            break;
        }

        if ((*msg->go_intent >> 1) > P2P_MAX_GO_INTENT) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: Invalid GO Intent value (%u) from GO Neg Resp from " MACSTR "\n", 
                __func__, *msg->go_intent >> 1, MAC2STR(sa));
            break;
        }

        if (!msg->config_timeout) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: Mandatory Configuration Timeout attribute missing from GO Neg Resp from " MACSTR "\n", 
                __func__, MAC2STR(sa));
            /* We still let it continue */
        }

        if (!msg->operating_channel) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: No Operating Channel attribute from GO Neg Resp from " MACSTR ".\n", 
                __func__, MAC2STR(sa));
            /* Not a problem if we are going to be a client. But a problem if we are GO.
             * But since we did not have GO Intent from our GO_NEG_REQ frame, we cannot determine
             * if we are going to be client or GO here. Let it continue. */
        }

        if (!msg->channel_list) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: No Channel List attribute from GO Neg Req from " MACSTR ". Drop req.\n", 
                __func__, MAC2STR(sa));
            break;
        }

        ok_status = true;      /* everything checked out OK */

    } while ( false );

    return ok_status;
}

/*
 * Validate the P2P GO Negotiation Confirmation frame.
 * ieee80211_p2p_parse_free() when the parsed data is not needed anymore.
 */
static bool validate_p2p_go_neg_conf(
    wlan_p2p_prot_t prot_handle, struct p2p_parsed_ie *msg, 
    u_int8_t *data, const u_int8_t *sa, u_int32_t len)
{
    bool    ok_status = false;

    do {

        if (!msg->status) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: P2P: No Status attribute received", __func__);
            break;
        }
        if (*msg->status) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: P2P: GO Negotiation Confirm rejected: status %d",
                __func__, *msg->status);
            ok_status = true;   /* The parsing of IE is OK */
            break;
        }

        if (!msg->capability) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: Mandatory Capability attribute missing from GO Neg Conf from " MACSTR "\n", 
                __func__, MAC2STR(sa));
            /* We still let it continue */
        }

        if (!msg->operating_channel) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: No Operating Channel attribute from GO Neg Conf from " MACSTR ".\n", 
                __func__, MAC2STR(sa));
            /* Not a problem if we are going to be a client. But a problem if we are GO.
             * But since we did not have GO Intent from our GO_NEG_REQ frame, we cannot determine
             * if we are going to be client or GO here. Let it continue. */
        }
        else {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                "%s: Operating Channel=%d attribute from GO Neg Conf from " MACSTR ".\n", 
                __func__, msg->operating_channel[4], MAC2STR(sa));
        }

        if (!msg->channel_list) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: No Channel List attribute from GO Neg Conf from " MACSTR ".\n", 
                __func__, MAC2STR(sa));
            /* We still let it continue */
        }

        if (!msg->group_id) {
            /* No group ID. If the other peer is tht GO, then this field is mandatory.
             * But we don't have enough information on who is the GO here. */
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: No group ID attribute from GO Neg Conf from " MACSTR ".\n", 
                __func__, MAC2STR(sa));
        }

        ok_status = true;      /* everything checked out OK */

    } while ( false );

    return ok_status;
}

/*
 * Validate the P2P Provision Discovery Request frame.
 * ieee80211_p2p_parse_free() when the parsed data is not needed anymore.
 */
static bool validate_p2p_prov_disc_req(
    wlan_p2p_prot_t prot_handle, struct p2p_parsed_ie *msg, 
    u_int8_t *data, const u_int8_t *sa, u_int32_t len)
{
    bool    ok_status = false;

    do {

        if (!msg->capability) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: Capability attribute missing from Prov Disc Req from " MACSTR "\n", 
                __func__, MAC2STR(sa));
            /* We still let it continue */
        }

        if (!msg->group_id) {
            /* No group ID. If the other peer is tht GO, then this field is mandatory.
             * But we don't have enough information on who is the GO here. */
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: No group ID attribute from Prov Disc Req from " MACSTR ".\n", 
                __func__, MAC2STR(sa));
        }

        ok_status = true;      /* everything checked out OK */

    } while ( false );

    return ok_status;
}

/*
 * Validate the P2P Invitation Request frame.
 * ieee80211_p2p_parse_free() when the parsed data is not needed anymore.
 */
static bool validate_p2p_invit_req(
    wlan_p2p_prot_t prot_handle, struct p2p_parsed_ie *msg, 
    u_int8_t *data, const u_int8_t *sa, u_int32_t len)
{
    bool    ok_status = false;

    do {

        if (!msg->group_id) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: Error: Missing Mandatory Group ID from Invitation Req from " MACSTR "\n", 
                __func__, MAC2STR(sa));
            break;
        }
    
        if (!msg->channel_list) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: Error: Missing Mandatory channel list from Invitation Req from " MACSTR "\n", 
                __func__, MAC2STR(sa));
            break;
        }
    
        if (msg->invitation_flags) {
            if (msg->invitation_flags[0] & P2P_INVITATION_FLAGS_TYPE_REINVOKE_PERSISTENT_GRP) {
                /* Reinvoke persistent group */
                IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                    "%s: Invitation Flags is REINVOKE_PERSISTENT_GRP\n", __func__);
            }
            else {
                /* Join active group */
                IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                    "%s: Invitation Flags is JOIN_ACTIVE_GRP\n", __func__);
            }
        }
        else {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: Error: missing Invitation Flags in invitation request frame\n", __func__);
            break;
        }

        ok_status = true;      /* everything checked out OK */

    } while ( false );

    return ok_status;
}

/* 
 * Parse and Process the Channel List attribute. Extract the country code and
 * channel list. Also calculate the intersection of the channel list and our
 * hardware support list.
 */
static void
proc_channel_list_attrib(wlan_p2p_prot_t prot_handle, wlan_p2p_prot_rx_context *rx_context)
{
    int retval;

    ASSERT((rx_context->previous_frtype == IEEE80211_P2P_PROT_FTYPE_GO_NEG_REQ) ||
           (rx_context->previous_frtype == IEEE80211_P2P_PROT_FTYPE_GO_NEG_RESP) ||
           (rx_context->previous_frtype == IEEE80211_P2P_PROT_FTYPE_INVIT_REQ));

    retval = wlan_p2p_parse_channel_list(prot_handle->vap, rx_context->msg.channel_list,
                                         rx_context->msg.channel_list_len,
                                         &rx_context->rx_chan_list, rx_context->rx_country);
    if (retval != 0) {
        /* Some error in parsing the channel list attribute */
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                          "%s: received invalid channel list attribute.\n",__func__);
        rx_context->parsed_ie_ok = false;
        rx_context->rx_valid_chan_list = false;
        return;
    }
    /* Else the channel list attribute is valid */
    rx_context->rx_valid_chan_list = true;

    /* Check the received country code */
    if ((rx_context->rx_country[2] != 0x04) &&
        (OS_MEMCMP(rx_context->rx_country, prot_handle->country, 2) != 0))
    {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                          "%s: diff. country code (rx=%x %x %x) in chan list. Ours=%x %x %x\n",
                             __func__, rx_context->rx_country[0],
                             rx_context->rx_country[1],
                             rx_context->rx_country[2],
                             prot_handle->country[0], prot_handle->country[1],
                             prot_handle->country[2]);
        rx_context->rx_valid_chan_list = false;
        return;
    }
}

/* 
 * Process the P2P Public Action Frame for Group Owner Negotiation Request.
 * We will indicate this frame to the upper stack.
 * Note that data is pointing to the Dialog Token field of the frame.
 */
static void
proc_rx_p2p_go_neg_req(
    wlan_p2p_prot_t prot_handle, wlan_p2p_event *p2p_dev_event, 
    const u_int8_t *sa, u_int8_t *data, u_int32_t len)
{
    u_int32_t                               rx_freq = p2p_dev_event->u.rx_frame.freq;
    wlan_p2p_prot_rx_context                *rx_context;
    u_int8_t                                *frame_ie_buf;
    u_int32_t                               ie_len, alloc_bufsize;
    u_int8_t                                *alloc_buf;
    wlan_p2p_prot_event                     *event;
    wlan_p2p_prot_event_rx_act_fr_ind       *rx_indicate;
    wlan_p2p_prot_event_rx_go_neg_req_ind   *rx_go_neg_req_ind;

    /* Parse the IE's in the frame */

    /* The IE is everything after the Dialog Token field */
    ie_len = len - 1;

    /* Alloc the buffer to store the event + received IE's + RxContextInfo */
    alloc_bufsize = sizeof(wlan_p2p_prot_rx_context) + sizeof(wlan_p2p_prot_event) + ie_len;

    alloc_buf = (u_int8_t *) OS_MALLOC(prot_handle->os_handle, alloc_bufsize, 0);
    if (alloc_buf == NULL) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                          "%s: OS_MALLOC failed, size=%d\n",__func__, alloc_bufsize);
        return;
    }
    OS_MEMZERO(alloc_buf, alloc_bufsize);

    /* Cut up the buffer into 3 sections */
    event = (wlan_p2p_prot_event *)alloc_buf;
    rx_context = (wlan_p2p_prot_rx_context *)(alloc_buf + sizeof(wlan_p2p_prot_event));
    frame_ie_buf = alloc_buf + sizeof(wlan_p2p_prot_event) + sizeof(wlan_p2p_prot_rx_context);

    /* Note: must to free the IE info by calling ieee80211_p2p_parse_free() */
    if (ieee80211_p2p_parse(prot_handle->os_handle, prot_handle->vap, data, len, &rx_context->msg)) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                          "%s: invalid frame. Ignored.\n",__func__);
        ieee80211_p2p_parse_free(&rx_context->msg);
        OS_FREE(alloc_buf);
        return;
    }

    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
        "%s: Received GO Negotiation Request from " MACSTR "(freq=%d), dialog_token=%d\n", 
        __func__, MAC2STR(sa), rx_freq, rx_context->msg.dialog_token);

    if (!validate_p2p_go_neg_req(prot_handle, &rx_context->msg, data, sa, len)) {
        /* There is something that we don't like about this frame but we will still
         * indicate this frame to upper layers. */
        rx_context->parsed_ie_ok = false;
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                          "%s: error from validate_p2p_go_neg_req.\n",__func__);
    }
    else {
        rx_context->parsed_ie_ok = true;
    }

    rx_context->p2p_dev_event = p2p_dev_event;
    rx_context->rx_freq = rx_freq;
    if (rx_context->msg.go_intent) {
        rx_context->rx_go_intent = *rx_context->msg.go_intent;
    }

    rx_context->previous_frtype = IEEE80211_P2P_PROT_FTYPE_GO_NEG_REQ;

    /* Copy over the channel list (if any) */
    if (rx_context->msg.channel_list) {
        proc_channel_list_attrib(prot_handle, rx_context);
    }

    /* Copy over the frame IE's. Skip the Dialog Token at the begining. */
    OS_MEMCPY(frame_ie_buf, data + 1, len - 1);

    rx_indicate = &event->rx_act_fr_ind;
    rx_go_neg_req_ind = &rx_indicate->go_neg_req;

    /* Fill in the wlan_p2p_prot_event_rx_go_neg_req_ind indication structure */
    OS_MEMCPY(rx_go_neg_req_ind->peer_addr, sa, IEEE80211_ADDR_LEN);
    rx_go_neg_req_ind->dialog_token = rx_context->msg.dialog_token;
    rx_go_neg_req_ind->request_context = rx_context;
    rx_go_neg_req_ind->add_ie_buf = frame_ie_buf;
    rx_go_neg_req_ind->add_ie_buf_len = ie_len;

    rx_indicate->frame_type = IEEE80211_P2P_PROT_FTYPE_GO_NEG_REQ;
    ASSERT(p2p_dev_event->u.rx_frame.frame_type == IEEE80211_FC0_SUBTYPE_ACTION);

    event->event_type = IEEE80211_P2P_PROT_EVENT_RX_ACT_FR_IND;

    /* Send this indication to the upper stack. Note that we may get a callback to transmit the
     * response frame */
    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
        "%s: Rx GO Neg Req frame.\n",
        __func__);

    prot_handle->ev_handler(prot_handle->event_arg, event);

    /* free the structures. */
    ieee80211_p2p_parse_free(&rx_context->msg);
    OS_FREE(alloc_buf);
}

/* 
 * Process the P2P Public Action Frame for GO Negotiation Response.
 * We will indicate this frame to the upper stack.
 * Note that data is pointing to the Dialog Token field of the frame.
 */
static void
proc_rx_p2p_go_neg_resp(
    wlan_p2p_prot_t prot_handle, wlan_p2p_event *p2p_dev_event, 
    const u_int8_t *sa, u_int8_t *data, u_int32_t len)
{
    u_int32_t                               rx_freq = p2p_dev_event->u.rx_frame.freq;
    wlan_p2p_prot_rx_context                *rx_context;
    u_int8_t                                *frame_ie_buf;
    u_int32_t                               ie_len, alloc_bufsize;
    u_int8_t                                *alloc_buf;
    wlan_p2p_prot_event                     *event;
    wlan_p2p_prot_event_rx_act_fr_ind       *rx_indicate;
    wlan_p2p_prot_event_rx_go_neg_resp_ind  *rx_go_neg_resp_ind;

    /* Parse the IE's in the frame */

    /* The IE is everything after the Dialog Token field */
    ie_len = len - 1;

    /* Alloc the buffer to store the event + received IE's + RxContextInfo */
    alloc_bufsize = sizeof(wlan_p2p_prot_rx_context) + sizeof(wlan_p2p_prot_event) + ie_len;

    alloc_buf = (u_int8_t *) OS_MALLOC(prot_handle->os_handle, alloc_bufsize, 0);
    if (alloc_buf == NULL) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                          "%s: OS_MALLOC failed, size=%d\n",__func__, alloc_bufsize);
        return;
    }
    OS_MEMZERO(alloc_buf, alloc_bufsize);

    /* Cut up the buffer into 3 sections */
    event = (wlan_p2p_prot_event *)alloc_buf;
    rx_context = (wlan_p2p_prot_rx_context *)(alloc_buf + sizeof(wlan_p2p_prot_event));
    frame_ie_buf = alloc_buf + sizeof(wlan_p2p_prot_event) + sizeof(wlan_p2p_prot_rx_context);

    /* Note: must to free the IE info by calling ieee80211_p2p_parse_free() */
    if (ieee80211_p2p_parse(prot_handle->os_handle, prot_handle->vap, data, len, &rx_context->msg)) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                          "%s: invalid frame. Ignored.\n",__func__);
        ieee80211_p2p_parse_free(&rx_context->msg);
        OS_FREE(alloc_buf);
        return;
    }

    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
        "%s: Received GO Negotiation Response from " MACSTR "(freq=%d). dialog_token=%d\n", 
        __func__, MAC2STR(sa), rx_freq, rx_context->msg.dialog_token);

    if (!validate_p2p_go_neg_resp(prot_handle, &rx_context->msg, data, sa, len)) {
        /* There is something that we don't like about this frame but we will still
         * indicate this frame to upper layers. */
        rx_context->parsed_ie_ok = false;
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                          "%s: validate_p2p_go_neg_resp ret error. parsed_ie failed.\n",__func__);
    }
    else {
        rx_context->parsed_ie_ok = true;
    }
    rx_context->p2p_dev_event = p2p_dev_event;
    rx_context->rx_freq = rx_freq;
    if (rx_context->msg.go_intent) {
        rx_context->rx_go_intent = *rx_context->msg.go_intent;
    }
    if (rx_context->msg.operating_channel != NULL) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                            "%s: received Op Chan=%d.\n", __func__, rx_context->msg.operating_channel[4]);
    }

    rx_context->previous_frtype = IEEE80211_P2P_PROT_FTYPE_GO_NEG_RESP;

    /* Copy over the channel list (if any) */
    if (rx_context->msg.channel_list) {
        proc_channel_list_attrib(prot_handle, rx_context);
    }

    /* Copy over the frame IE's. Skip the Dialog Token at the begining. */
    OS_MEMCPY(frame_ie_buf, data + 1, len - 1);

    rx_indicate = &event->rx_act_fr_ind;
    rx_go_neg_resp_ind = &rx_indicate->go_neg_resp;

    /* Fill in the wlan_p2p_prot_event_rx_go_neg_resp_ind indication structure */
    OS_MEMCPY(rx_go_neg_resp_ind->peer_addr, sa, IEEE80211_ADDR_LEN);
    rx_go_neg_resp_ind->dialog_token = rx_context->msg.dialog_token;
    rx_go_neg_resp_ind->request_context = rx_context;
    rx_go_neg_resp_ind->add_ie_buf = frame_ie_buf;
    rx_go_neg_resp_ind->add_ie_buf_len = ie_len;

    rx_indicate->frame_type = IEEE80211_P2P_PROT_FTYPE_GO_NEG_RESP;
    ASSERT(p2p_dev_event->u.rx_frame.frame_type == IEEE80211_FC0_SUBTYPE_ACTION);

    event->event_type = IEEE80211_P2P_PROT_EVENT_RX_ACT_FR_IND;

    /* Send this indication to the upper stack. Note that we may get a callback to transmit the
     * response frame */
    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
        "%s: Rx GO Neg Resp frame.\n",
        __func__);

    prot_handle->ev_handler(prot_handle->event_arg, event);

    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
        "%s: back from event handler call back.\n",
        __func__);

    /* free the structures. */
    ieee80211_p2p_parse_free(&rx_context->msg);
    OS_FREE(alloc_buf);
}

/* 
 * Process the P2P Public Action Frame for GO Negotiation Confirmation.
 * We will indicate this frame to the upper stack.
 * Note that data is pointing to the Dialog Token field of the frame.
 */
static void
proc_rx_p2p_go_neg_conf(
    wlan_p2p_prot_t prot_handle, wlan_p2p_event *p2p_dev_event, 
    const u_int8_t *sa, u_int8_t *data, u_int32_t len)
{
    u_int32_t                               rx_freq = p2p_dev_event->u.rx_frame.freq;
    u_int8_t                                *frame_ie_buf;
    u_int32_t                               ie_len, alloc_bufsize;
    u_int8_t                                *alloc_buf;
    wlan_p2p_prot_event                     *event;
    wlan_p2p_prot_event_rx_act_fr_ind       *rx_indicate;
    wlan_p2p_prot_event_rx_go_neg_conf_ind  *rx_go_neg_conf_ind;
    struct p2p_parsed_ie                    msg;

    /* Parse the IE's in the frame */

    /* The IE is everything after the Dialog Token field */
    ie_len = len - 1;

    /* Alloc the buffer to store the event + received IE's */
    alloc_bufsize = sizeof(wlan_p2p_prot_event) + ie_len;

    alloc_buf = (u_int8_t *) OS_MALLOC(prot_handle->os_handle, alloc_bufsize, 0);
    if (alloc_buf == NULL) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                          "%s: OS_MALLOC failed, size=%d\n",__func__, alloc_bufsize);
        return;
    }
    OS_MEMZERO(alloc_buf, alloc_bufsize);

    /* Cut up the buffer into 2 sections */
    event = (wlan_p2p_prot_event *)alloc_buf;
    frame_ie_buf = alloc_buf + sizeof(wlan_p2p_prot_event);

    /* Note: must to free the IE info by calling ieee80211_p2p_parse_free() */
    if (ieee80211_p2p_parse(prot_handle->os_handle, prot_handle->vap, data, len, &msg)) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                          "%s: invalid frame. Ignored.\n",__func__);
        ieee80211_p2p_parse_free(&msg);
        OS_FREE(alloc_buf);
        return;
    }

    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
        "%s: Received GO Negotiation Confirm from " MACSTR "(freq=%d). dialog_token=%d\n", 
         __func__, MAC2STR(sa), rx_freq, msg.dialog_token);

    if (!validate_p2p_go_neg_conf(prot_handle, &msg, data, sa, len)) {
        /* There is something that we don't like about this frame but we will still
         * indicate this frame to upper layers. */
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                          "%s: validate_frame returns failure.\n",__func__);
    }

    /* Copy over the frame IE's. Skip the Dialog Token at the begining. */
    OS_MEMCPY(frame_ie_buf, data + 1, len - 1);

    rx_indicate = &event->rx_act_fr_ind;
    rx_go_neg_conf_ind = &rx_indicate->go_neg_conf;

    /* Fill in the wlan_p2p_prot_event_rx_go_neg_conf_ind indication structure */
    OS_MEMCPY(rx_go_neg_conf_ind->peer_addr, sa, IEEE80211_ADDR_LEN);
    rx_go_neg_conf_ind->dialog_token = msg.dialog_token;
    rx_go_neg_conf_ind->status = *msg.status;
    rx_go_neg_conf_ind->add_ie_buf = frame_ie_buf;
    rx_go_neg_conf_ind->add_ie_buf_len = ie_len;

    rx_indicate->frame_type = IEEE80211_P2P_PROT_FTYPE_GO_NEG_CONF;
    ASSERT(p2p_dev_event->u.rx_frame.frame_type == IEEE80211_FC0_SUBTYPE_ACTION);

    event->event_type = IEEE80211_P2P_PROT_EVENT_RX_ACT_FR_IND;

    /* Send this indication to the upper stack. Note that we may get a callback to transmit the
     * response frame */
    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
        "%s: Rx GO Neg Conf frame.\n",
        __func__);

    prot_handle->ev_handler(prot_handle->event_arg, event);

    /* free the structures. */
    ieee80211_p2p_parse_free(&msg);
    OS_FREE(alloc_buf);
}

/* 
 * Process the P2P Public Action Frame for Provision Discovery Request.
 * We will indicate this frame to the upper stack.
 * Note that data is pointing to the Dialog Token field of the frame.
 */
static void
proc_rx_p2p_prov_disc_req(
    wlan_p2p_prot_t prot_handle, wlan_p2p_event *p2p_dev_event, 
    const u_int8_t *sa, u_int8_t *data, u_int32_t len)
{
    u_int32_t                                   rx_freq = p2p_dev_event->u.rx_frame.freq;
    wlan_p2p_prot_rx_context                    *rx_context;
    u_int8_t                                    *frame_ie_buf;
    u_int32_t                                   ie_len, alloc_bufsize;
    u_int8_t                                    *alloc_buf;
    wlan_p2p_prot_event                         *event;
    wlan_p2p_prot_event_rx_act_fr_ind           *rx_indicate;
    wlan_p2p_prot_event_rx_prov_disc_req_ind    *rx_prov_disc_req_ind;

    /* Parse the IE's in the frame */

    /* The IE is everything after the Dialog Token field */
    ie_len = len - 1;

    /* Alloc the buffer to store the event + received IE's + RxContextInfo */
    alloc_bufsize = sizeof(wlan_p2p_prot_rx_context) + sizeof(wlan_p2p_prot_event) + ie_len;

    alloc_buf = (u_int8_t *) OS_MALLOC(prot_handle->os_handle, alloc_bufsize, 0);
    if (alloc_buf == NULL) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                          "%s: OS_MALLOC failed, size=%d\n",__func__, alloc_bufsize);
        return;
    }
    OS_MEMZERO(alloc_buf, alloc_bufsize);

    /* Cut up the buffer into 3 sections */
    event = (wlan_p2p_prot_event *)alloc_buf;
    rx_context = (wlan_p2p_prot_rx_context *)(alloc_buf + sizeof(wlan_p2p_prot_event));
    frame_ie_buf = alloc_buf + sizeof(wlan_p2p_prot_event) + sizeof(wlan_p2p_prot_rx_context);

    /* Note: must to free the IE info by calling ieee80211_p2p_parse_free() */
    if (ieee80211_p2p_parse(prot_handle->os_handle, prot_handle->vap, data, len, &rx_context->msg)) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                          "%s: invalid frame. Ignored.\n",__func__);
        ieee80211_p2p_parse_free(&rx_context->msg);
        OS_FREE(alloc_buf);
        return;
    }

    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
        "%s: Received Provision Discovery Request from " MACSTR "(freq=%d), dialog_token=%d\n", 
        __func__, MAC2STR(sa), rx_freq, rx_context->msg.dialog_token);

    if (!validate_p2p_prov_disc_req(prot_handle, &rx_context->msg, data, sa, len)) {
        /* There is something that we don't like about this frame but we will still
         * indicate this frame to upper layers. */
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                          "%s: validate_frame returns failure.\n", __func__);
        rx_context->parsed_ie_ok = false;
    }
    else {
        rx_context->parsed_ie_ok = true;
    }
    rx_context->p2p_dev_event = p2p_dev_event;
    rx_context->rx_freq = rx_freq;

    /* Copy over the frame IE's. Skip the Dialog Token at the begining. */
    OS_MEMCPY(frame_ie_buf, data + 1, len - 1);

    rx_indicate = &event->rx_act_fr_ind;
    rx_prov_disc_req_ind = &rx_indicate->prov_disc_req;

    /* Fill in the wlan_p2p_prot_event_rx_prov_disc_req_ind indication structure */
    OS_MEMCPY(rx_prov_disc_req_ind->ta, sa, IEEE80211_ADDR_LEN);
    {
        struct ieee80211_frame *wh;
        wh = (struct ieee80211_frame *)p2p_dev_event->u.rx_frame.frame_buf;
        OS_MEMCPY(rx_prov_disc_req_ind->bssid, wh->i_addr3, IEEE80211_ADDR_LEN);
    }
    rx_prov_disc_req_ind->dialog_token = rx_context->msg.dialog_token;
    rx_prov_disc_req_ind->request_context = rx_context;
    rx_prov_disc_req_ind->add_ie_buf = frame_ie_buf;
    rx_prov_disc_req_ind->add_ie_buf_len = ie_len;

    rx_indicate->frame_type = IEEE80211_P2P_PROT_FTYPE_PROV_DISC_REQ;
    ASSERT(p2p_dev_event->u.rx_frame.frame_type == IEEE80211_FC0_SUBTYPE_ACTION);

    event->event_type = IEEE80211_P2P_PROT_EVENT_RX_ACT_FR_IND;

    /* Send this indication to the upper stack. Note that we may get a callback to transmit the
     * response frame */
    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
        "%s: Rx Prov Disc Req frame.\n",
        __func__);

    prot_handle->ev_handler(prot_handle->event_arg, event);

    /* free the structures. */
    ieee80211_p2p_parse_free(&rx_context->msg);
    OS_FREE(alloc_buf);
}

/* 
 * Process the P2P Public Action Frame for Provision Discovery Response.
 * We will indicate this frame to the upper stack.
 * Note that data is pointing to the Dialog Token field of the frame.
 */
static void
proc_rx_p2p_prov_disc_resp(
    wlan_p2p_prot_t prot_handle, wlan_p2p_event *p2p_dev_event, 
    const u_int8_t *sa, u_int8_t *data, u_int32_t len)
{
    u_int32_t                                   rx_freq = p2p_dev_event->u.rx_frame.freq;
    u_int8_t                                    *frame_ie_buf;
    u_int32_t                                   ie_len, alloc_bufsize;
    u_int8_t                                    *alloc_buf;
    wlan_p2p_prot_event                         *event;
    wlan_p2p_prot_event_rx_act_fr_ind           *rx_indicate;
    wlan_p2p_prot_event_rx_prov_disc_resp_ind   *rx_prov_disc_resp_ind;
    struct p2p_parsed_ie                        msg;

    /* Parse the IE's in the frame */

    /* The IE is everything after the Dialog Token field */
    ie_len = len - 1;

    /* Alloc the buffer to store the event + received IE's */
    alloc_bufsize = sizeof(wlan_p2p_prot_event) + ie_len;

    alloc_buf = (u_int8_t *) OS_MALLOC(prot_handle->os_handle, alloc_bufsize, 0);
    if (alloc_buf == NULL) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                          "%s: OS_MALLOC failed, size=%d\n",__func__, alloc_bufsize);
        return;
    }
    OS_MEMZERO(alloc_buf, alloc_bufsize);

    /* Cut up the buffer into 2 sections */
    event = (wlan_p2p_prot_event *)alloc_buf;
    frame_ie_buf = alloc_buf + sizeof(wlan_p2p_prot_event);

    /* Note: must to free the IE info by calling ieee80211_p2p_parse_free() */
    if (ieee80211_p2p_parse(prot_handle->os_handle, prot_handle->vap, data, len, &msg)) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                          "%s: invalid frame. Ignored.\n",__func__);
        ieee80211_p2p_parse_free(&msg);
        OS_FREE(alloc_buf);
        return;
    }

    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
        "%s: Received Provision Discovery Response from " MACSTR "(freq=%d). dialog_token=%d\n", 
        __func__, MAC2STR(sa), rx_freq, msg.dialog_token);

    /* Copy over the frame IE's. Skip the Dialog Token at the begining. */
    OS_MEMCPY(frame_ie_buf, data + 1, len - 1);

    rx_indicate = &event->rx_act_fr_ind;
    rx_prov_disc_resp_ind = &rx_indicate->prov_disc_resp;

    /* Fill in the wlan_p2p_prot_event_rx_prov_disc_resp_ind indication structure */
    OS_MEMCPY(rx_prov_disc_resp_ind->ta, sa, IEEE80211_ADDR_LEN);
    {
        struct ieee80211_frame *wh;
        wh = (struct ieee80211_frame *)p2p_dev_event->u.rx_frame.frame_buf;
        OS_MEMCPY(rx_prov_disc_resp_ind->bssid, wh->i_addr3, IEEE80211_ADDR_LEN);
    }
    rx_prov_disc_resp_ind->dialog_token = msg.dialog_token;
    rx_prov_disc_resp_ind->add_ie_buf = frame_ie_buf;
    rx_prov_disc_resp_ind->add_ie_buf_len = ie_len;

    rx_indicate->frame_type = IEEE80211_P2P_PROT_FTYPE_PROV_DISC_RESP;
    ASSERT(p2p_dev_event->u.rx_frame.frame_type == IEEE80211_FC0_SUBTYPE_ACTION);

    event->event_type = IEEE80211_P2P_PROT_EVENT_RX_ACT_FR_IND;

    /* Send this indication to the upper stack. Note that we may get a callback to transmit the
     * response frame */
    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
        "%s: Rx Prov Disc Resp frame.\n",
        __func__);

    prot_handle->ev_handler(prot_handle->event_arg, event);

    /* free the structures. */
    ieee80211_p2p_parse_free(&msg);
    OS_FREE(alloc_buf);
}

/* 
 * Process the P2P Public Action Frame for Invitation Request.
 * We will indicate this frame to the upper stack.
 * Note that data is pointing to the Dialog Token field of the frame.
 */
static void
proc_rx_p2p_invit_req(
    wlan_p2p_prot_t prot_handle, wlan_p2p_event *p2p_dev_event, 
    const u_int8_t *sa, u_int8_t *data, u_int32_t len)
{
    u_int32_t                                   rx_freq = p2p_dev_event->u.rx_frame.freq;
    wlan_p2p_prot_rx_context                    *rx_context;
    u_int8_t                                    *frame_ie_buf;
    u_int32_t                                   ie_len, alloc_bufsize;
    u_int8_t                                    *alloc_buf;
    wlan_p2p_prot_event                         *event;
    wlan_p2p_prot_event_rx_act_fr_ind           *rx_indicate;
    wlan_p2p_prot_event_rx_invit_req_ind        *rx_invit_req_ind;

    /* Parse the IE's in the frame */

    /* The IE is everything after the Dialog Token field */
    ie_len = len - 1;

    /* Alloc the buffer to store the event + received IE's + RxContextInfo */
    alloc_bufsize = sizeof(wlan_p2p_prot_rx_context) + sizeof(wlan_p2p_prot_event) + ie_len;

    alloc_buf = (u_int8_t *) OS_MALLOC(prot_handle->os_handle, alloc_bufsize, 0);
    if (alloc_buf == NULL) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                          "%s: OS_MALLOC failed, size=%d\n",__func__, alloc_bufsize);
        return;
    }
    OS_MEMZERO(alloc_buf, alloc_bufsize);

    /* Cut up the buffer into 3 sections */
    event = (wlan_p2p_prot_event *)alloc_buf;
    rx_context = (wlan_p2p_prot_rx_context *)(alloc_buf + sizeof(wlan_p2p_prot_event));
    frame_ie_buf = alloc_buf + sizeof(wlan_p2p_prot_event) + sizeof(wlan_p2p_prot_rx_context);

    /* Note: must to free the IE info by calling ieee80211_p2p_parse_free() */
    if (ieee80211_p2p_parse(prot_handle->os_handle, prot_handle->vap, data, len, &rx_context->msg)) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                          "%s: invalid frame. Ignored.\n",__func__);
        ieee80211_p2p_parse_free(&rx_context->msg);
        OS_FREE(alloc_buf);
        return;
    }

    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
        "%s: Received Invitation Request from " MACSTR "(freq=%d). dialog_token=%d\n", 
        __func__, MAC2STR(sa), rx_freq, rx_context->msg.dialog_token);

    if (!validate_p2p_invit_req(prot_handle, &rx_context->msg, data, sa, len)) {
        /* There is something that we don't like about this frame but we will still
         * indicate this frame to upper layers. */
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                          "%s: validate_frame returns failure.\n", __func__);
        rx_context->parsed_ie_ok = false;
    }
    else {
        rx_context->parsed_ie_ok = true;
    }
    rx_context->p2p_dev_event = p2p_dev_event;
    rx_context->rx_freq = rx_freq;

    /* Copy over the frame IE's. Skip the Dialog Token at the begining. */
    OS_MEMCPY(frame_ie_buf, data + 1, len - 1);

    rx_indicate = &event->rx_act_fr_ind;
    rx_invit_req_ind = &rx_indicate->invit_req;

    /* Fill in the wlan_p2p_prot_event_rx_invit_req_ind indication structure */
    OS_MEMCPY(rx_invit_req_ind->ta, sa, IEEE80211_ADDR_LEN);
    {
        struct ieee80211_frame *wh;
        wh = (struct ieee80211_frame *)p2p_dev_event->u.rx_frame.frame_buf;
        OS_MEMCPY(rx_invit_req_ind->bssid, wh->i_addr3, IEEE80211_ADDR_LEN);
    }
    rx_invit_req_ind->dialog_token = rx_context->msg.dialog_token;
    rx_invit_req_ind->request_context = rx_context;
    rx_invit_req_ind->add_ie_buf = frame_ie_buf;
    rx_invit_req_ind->add_ie_buf_len = ie_len;

    rx_context->previous_frtype = IEEE80211_P2P_PROT_FTYPE_INVIT_REQ;

    /* Copy over the channel list (if any) */
    if (rx_context->msg.channel_list) {
        proc_channel_list_attrib(prot_handle, rx_context);
    }

    if (rx_context->parsed_ie_ok)
    {
        const u8    *dev_addr;
        const u8    *ssid;
        u_int16_t   ssid_len;
        int         retval;

        /* Store some information to be used when sending the response frame */
        ASSERT(rx_context->msg.invitation_flags != NULL);
        if (rx_context->msg.invitation_flags[0] & P2P_INVITATION_FLAGS_TYPE_REINVOKE_PERSISTENT_GRP) {
            /* Reinvoke persistent group */
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                "%s: Invitation Flags is REINVOKE_PERSISTENT_GRP\n", __func__);
            rx_context->start_new_group = true;
        }
        else {
            /* Join active group */
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                "%s: Invitation Flags is JOIN_ACTIVE_GRP\n", __func__);
            rx_context->start_new_group = false;
        }

        ASSERT(rx_context->msg.group_id != NULL);

        retval = wlan_p2p_parse_group_id(prot_handle->vap, rx_context->msg.group_id,
                                         rx_context->msg.group_id_len, &dev_addr, &ssid, &ssid_len);
        if (retval != 0) {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: Error: wlan_p2p_parse_group_id ret 0x%x\n", __func__, retval);
        }
        else {
            if (IEEE80211_ADDR_EQ(dev_addr, prot_handle->vap->iv_myaddr)) {
                /* We are going to be the Group Owner */
                rx_context->we_are_GO = true;
            }
            else {
                rx_context->we_are_GO = false;
            }
        }
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
            "%s: we_are_GO=%d, start_new_group=%d\n", __func__,
            rx_context->we_are_GO, rx_context->start_new_group);

        if (!rx_context->start_new_group) {
            /* Join existing group, Make sure that we have op chan attribute */
            if (rx_context->msg.operating_channel == NULL) {
                /* Error: when joining existing group, we must have op channel attribute */
                IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                    "%s: Error: joining group but no op channel attribute.\n", __func__, retval);
                rx_context->parsed_ie_ok = false;
            }
        }
    }

    rx_indicate->frame_type = IEEE80211_P2P_PROT_FTYPE_INVIT_REQ;
    ASSERT(p2p_dev_event->u.rx_frame.frame_type == IEEE80211_FC0_SUBTYPE_ACTION);

    event->event_type = IEEE80211_P2P_PROT_EVENT_RX_ACT_FR_IND;

    /* Send this indication to the upper stack. Note that we may get a callback to transmit the
     * response frame */
    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
        "%s: Rx Invitation Req frame.\n",
        __func__);

    prot_handle->ev_handler(prot_handle->event_arg, event);

    /* free the structures. */
    ieee80211_p2p_parse_free(&rx_context->msg);
    OS_FREE(alloc_buf);
}

/* 
 * Process the P2P Public Action Frame for Invitation Response.
 * We will indicate this frame to the upper stack.
 * Note that data is pointing to the Dialog Token field of the frame.
 */
static void
proc_rx_p2p_invit_resp(
    wlan_p2p_prot_t prot_handle, wlan_p2p_event *p2p_dev_event, 
    const u_int8_t *sa, u_int8_t *data, u_int32_t len)
{
    u_int32_t                                   rx_freq = p2p_dev_event->u.rx_frame.freq;
    u_int8_t                                    *frame_ie_buf;
    u_int32_t                                   ie_len, alloc_bufsize;
    u_int8_t                                    *alloc_buf;
    wlan_p2p_prot_event                         *event;
    wlan_p2p_prot_event_rx_act_fr_ind           *rx_indicate;
    wlan_p2p_prot_event_rx_invit_resp_ind       *rx_invit_resp_ind;
    struct p2p_parsed_ie                        msg;

    /* Parse the IE's in the frame */

    /* The IE is everything after the Dialog Token field */
    ie_len = len - 1;

    /* Alloc the buffer to store the event + received IE's */
    alloc_bufsize = sizeof(wlan_p2p_prot_event) + ie_len;

    alloc_buf = (u_int8_t *) OS_MALLOC(prot_handle->os_handle, alloc_bufsize, 0);
    if (alloc_buf == NULL) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                          "%s: OS_MALLOC failed, size=%d\n",__func__, alloc_bufsize);
        return;
    }
    OS_MEMZERO(alloc_buf, alloc_bufsize);

    /* Cut up the buffer into 2 sections */
    event = (wlan_p2p_prot_event *)alloc_buf;
    frame_ie_buf = alloc_buf + sizeof(wlan_p2p_prot_event);

    /* Note: must to free the IE info by calling ieee80211_p2p_parse_free() */
    if (ieee80211_p2p_parse(prot_handle->os_handle, prot_handle->vap, data, len, &msg)) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                          "%s: invalid frame. Ignored.\n",__func__);
        ieee80211_p2p_parse_free(&msg);
        OS_FREE(alloc_buf);
        return;
    }

    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
        "%s: Received Invitation Response from " MACSTR "(freq=%d). dialog_token=%d\n", 
        __func__, MAC2STR(sa), rx_freq, msg.dialog_token);

    /* Copy over the frame IE's. Skip the Dialog Token at the begining. */
    OS_MEMCPY(frame_ie_buf, data + 1, len - 1);

    rx_indicate = &event->rx_act_fr_ind;
    rx_invit_resp_ind = &rx_indicate->invit_resp;

    /* Fill in the wlan_p2p_prot_event_rx_invit_resp_ind indication structure */
    OS_MEMCPY(rx_invit_resp_ind->ta, sa, IEEE80211_ADDR_LEN);
    {
        struct ieee80211_frame *wh;
        wh = (struct ieee80211_frame *)p2p_dev_event->u.rx_frame.frame_buf;
        OS_MEMCPY(rx_invit_resp_ind->bssid, wh->i_addr3, IEEE80211_ADDR_LEN);
    }
    rx_invit_resp_ind->dialog_token = msg.dialog_token;
    rx_invit_resp_ind->add_ie_buf = frame_ie_buf;
    rx_invit_resp_ind->add_ie_buf_len = ie_len;

    rx_indicate->frame_type = IEEE80211_P2P_PROT_FTYPE_INVIT_RESP;
    ASSERT(p2p_dev_event->u.rx_frame.frame_type == IEEE80211_FC0_SUBTYPE_ACTION);

    event->event_type = IEEE80211_P2P_PROT_EVENT_RX_ACT_FR_IND;

    /* Send this indication to the upper stack. Note that we may get a callback to transmit the
     * response frame */
    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
        "%s: Rx Invitation Resp frame.\n",
        __func__);

    prot_handle->ev_handler(prot_handle->event_arg, event);

    /* free the structures. */
    ieee80211_p2p_parse_free(&msg);
    OS_FREE(alloc_buf);
}

/* 
 * Process the P2P Public Action Frame
 */
static void
proc_rx_p2p_public_action_frame(
    wlan_p2p_prot_t prot_handle, wlan_p2p_event *p2p_dev_event,
    const u_int8_t *sa, u_int8_t *data, u_int32_t len)
{
    ASSERT(len >= 1);

    /* check the OUI Subtype */
    switch (data[0]) {
    case P2P_GO_NEG_REQ:
        proc_rx_p2p_go_neg_req(prot_handle, p2p_dev_event, sa, data + 1, len - 1);
        break;
    case P2P_PROV_DISC_REQ:
        proc_rx_p2p_prov_disc_req(prot_handle, p2p_dev_event, sa, data + 1, len - 1);
        break;
    case P2P_INVITATION_REQ:
        proc_rx_p2p_invit_req(prot_handle, p2p_dev_event, sa, data + 1, len - 1);
        break;
    case P2P_INVITATION_RESP:
        proc_rx_p2p_invit_resp(prot_handle, p2p_dev_event, sa, data + 1, len - 1);
        break;
    case P2P_PROV_DISC_RESP:
        proc_rx_p2p_prov_disc_resp(prot_handle, p2p_dev_event, sa, data + 1, len - 1);
        break;
    case P2P_GO_NEG_RESP:
        proc_rx_p2p_go_neg_resp(prot_handle, p2p_dev_event, sa, data + 1, len - 1);
        break;
    case P2P_GO_NEG_CONF:
        proc_rx_p2p_go_neg_conf(prot_handle, p2p_dev_event, sa, data + 1, len - 1);
        break;
#if 0   /* STNG TODO: add the support for these frames */
    case P2P_DEV_DISC_REQ:
        p2p_process_dev_disc_req(p2p, sa, data + 1, len - 1, rx_freq);
        break;
    case P2P_DEV_DISC_RESP:
        p2p_process_dev_disc_resp(p2p, sa, data + 1, len - 1);
        break;
#endif
    default:
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                             "%s: Unsupported P2P Public Action frame type %d\n",
                             __func__, data[0]);
        break;
    }

}

/*
 * Routine to process the Rx of an public action frame.
 * Note that the data is pointer to the start of the payload (after the 802.11 header).
 */
static void
proc_rx_public_action_frame(wlan_p2p_prot_t prot_handle, wlan_p2p_event *p2p_dev_event, u_int8_t *data, u_int32_t len)
{
    u_int8_t        category;
    const u_int8_t  *sa;        /* source address */

    sa = p2p_dev_event->u.rx_frame.src_addr;

    /* make sure that frame contains at least the category (1), action (1), 
     * OUI (3), OUI type (1), and OUT subtype (1) fields */
    if (len < (1+1+3+1+1)) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                             "%s: public action frame too short, len=%d\n",
                             __func__, len);
        return;
    }

    /* skip the category octet */
    data++;
    len--;

    /* check the action field */
    switch (data[0]) {
        case WLAN_PA_VENDOR_SPECIFIC:
            data++;
            len--;
            if (P2PIE_GET_BE24(data) != OUI_WFA) {
                IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                                     "%s: public action frame invalid OUI=%x %x %x\n",
                                     __func__, data[0], data[1], data[2]);
                return;
            }
            
            data += 3;      /* skip the OUI */
            len -= 3;
            if (*data != P2P_OUI_TYPE) {
                IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                                     "%s: public action frame invalid, OUI Type=%x\n",
                                     __func__, data[0]);
                return;
            }
            /* skip the OUI type */
            data++;
            len--;

            /* Process the P2P Public Action Frame */
            proc_rx_p2p_public_action_frame(prot_handle, p2p_dev_event, sa, data, len);
            break;
#if 0   /* STNG TODO: add support for these public action frames (if needed) */
        case WLAN_PA_GAS_INITIAL_REQ:
            p2p_rx_gas_initial_req(p2p, sa, data + 1, len - 1, freq);
            break;
        case WLAN_PA_GAS_INITIAL_RESP:
            p2p_rx_gas_initial_resp(p2p, sa, data + 1, len - 1, freq);
            break;
        case WLAN_PA_GAS_COMEBACK_REQ:
            p2p_rx_gas_comeback_req(p2p, sa, data + 1, len - 1, freq);
            break;
        case WLAN_PA_GAS_COMEBACK_RESP:
            p2p_rx_gas_comeback_resp(p2p, sa, data + 1, len - 1, freq);
            break;
#endif
        default:
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                                    "%s: Unsupported public action frame, action field=%x\n",
                                    __func__, data[0]);
            break;
    }
}

/*
 * Routine to process the Rx of an action frame.
 */
static void
proc_rx_action_frame(wlan_p2p_prot_t prot_handle, wlan_p2p_event *p2p_dev_event)
{
    u_int8_t        *frm;
    u_int8_t        category;
    u_int32_t       len;

    frm = p2p_dev_event->u.rx_frame.frame_buf;
    len = p2p_dev_event->u.rx_frame.frame_len;
    frm += sizeof(struct ieee80211_frame);      /* skip the header */
    len -= sizeof(struct ieee80211_frame);      /* skip the header */

    /* make sure that frame contains at least the category and action fields */
    if (len < 2) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                             "%s: action frame too short, len=%d\n",
                             __func__, p2p_dev_event->u.rx_frame.frame_len);
        return;
    }

    /* Check the first octet of payload which is the category */
    category = frm[0];
    switch(category) {
    case WLAN_ACTION_PUBLIC:
        proc_rx_public_action_frame(prot_handle, p2p_dev_event, frm, len);
        break;
#if 0   //STNG TODO: add this frame support.
    case WLAN_ACTION_VENDOR_SPECIFIC:
        proc_rx_vendor_specific_action_frame(prot_handle, p2p_dev_event);
        break;
#endif
    default:
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                          "%s: unsupport action frame category=%d\n",__func__, category);
        break;
    }
}

struct find_GO_param_with_chan {
    wlan_p2p_prot_t prot_handle;
    bool            match_found;
    u_int32_t       rx_freq;
    u_int8_t        mac_addr[IEEE80211_ADDR_LEN];
    ieee80211_ssid  *grp_ssid;
    u_int8_t        ta_addr[IEEE80211_ADDR_LEN];
};

/*
 * Callback function to iterate the Scan list based on mac addresss 
 * and find a suitable GO that is on this channel.
 * Called by wlan_scan_macaddr_iterate().
 * Returns EOK if not found. 
 * Return -EALREADY if a GO is found and the iteration stops.
 */
static int
p2p_disc_find_GO_on_same_freq(void *ctx, wlan_scan_entry_t entry)
{
    struct find_GO_param_with_chan      *param;
    u_int8_t                            *entry_mac_addr;
    int                                 retval;
    ieee80211_scan_p2p_info             scan_p2p_info;

    param = (struct find_GO_param_with_chan *)ctx;
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
                     __func__, scan_p2p_info.is_p2p, param->rx_freq,
                     (scan_p2p_info.listen_chan? scan_p2p_info.listen_chan->ic_freq : 0),
                     SPLIT_MAC_ADDR(scan_p2p_info.p2p_dev_addr));

    if (!scan_p2p_info.is_p2p) {
        /* Not a P2P, skip this entry and go to the next entry. */
        return EOK; 
    }

    /* Check that the frequency matches */
    if (param->rx_freq != scan_p2p_info.listen_chan->ic_freq) {
        /* Frequency mismatch. Go to the next entry. */
        return EOK; 
    }

    /* Maybe this is a Group Owner entry. Then check its device address */
    if (!IEEE80211_ADDR_EQ(param->mac_addr, scan_p2p_info.p2p_dev_addr)) {
        /* It's P2P Device is not what we want */

        return EOK;
    }
    /* Else the P2P Device matches  but check the SSID first */
    ASSERT(param->grp_ssid != NULL);

    if (param->grp_ssid) {
        /* Make sure the SSID matches */
        u_int8_t entry_ssid_len = 0;
        u_int8_t *entry_ssid;

        entry_ssid = wlan_scan_entry_ssid(entry, &entry_ssid_len);
        if ((entry_ssid_len != param->grp_ssid->len) ||
            (OS_MEMCMP(param->grp_ssid->ssid, entry_ssid, entry_ssid_len) != 0))
        {
            /* SSID does not match */
            if (entry_ssid) {
                char    tmpbuf[IEEE80211_NWID_LEN + 1];

                IEEE80211_DPRINTF_VB(param->prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                                     "%s: Error: SSID mismatched. Entry: ssid=%s, len=%d. Desired: ssid=%s, len=%d\n",
                                     __func__, get_str_ssid(tmpbuf, entry_ssid, entry_ssid_len), entry_ssid_len);
                IEEE80211_DPRINTF_VB(param->prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                                     "\tDesired: ssid=%s, len=%d\n",
                                     get_str_ssid(tmpbuf, param->grp_ssid->ssid, param->grp_ssid->len),
                                     param->grp_ssid->len);
            }
            else {
                IEEE80211_DPRINTF_VB(param->prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                                 "%s: Error: scan entry %pK has no SSID.\n", __func__, entry);
            }
            return EOK;
        }
        else {
            char    tmpbuf[IEEE80211_NWID_LEN + 1];

            IEEE80211_DPRINTF_VB(param->prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                                 "%s: Found a match in device ID address="MAC_ADDR_FMT", ssid=%s, scan entry=0x%x.\n",
                                 __func__, SPLIT_MAC_ADDR(entry_mac_addr), 
                                 get_str_ssid(tmpbuf, param->grp_ssid->ssid, param->grp_ssid->len),
                                 entry);
        }
    }

    param->match_found = true;
    IEEE80211_ADDR_COPY(param->ta_addr, entry_mac_addr);

    /* Return an error so that we stop iterating the list */
    return -EALREADY;
}

/*
 * Check whether this frame satisfy one of the device filters.
 * Return true if all device filters are satisfied.
 */
static bool
check_for_desired_peers(wlan_p2p_prot_t prot_handle, struct wlan_p2p_rx_frame *frame)
{
    int     i;
    bool    matched;

    matched = false;
    for (i = 0; i < prot_handle->desired_peer.num_dev_filters; i++) {
        wlan_p2p_prot_disc_dev_filter   *curr_filter;

        curr_filter = &(prot_handle->desired_peer.dev_filter[i]);
#if DBG
        ASSERT(curr_filter->dbg_mem_marker == WLAN_P2P_PROT_DBG_MEM_MARKER);
#endif //DBG
        if (curr_filter->discovered) {
            /* Already discovered and account for. */
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                 "%s: This filter for address="MAC_ADDR_FMT" is already discovered.\n",
                 __func__, SPLIT_MAC_ADDR(curr_filter->device_id));
            continue;
        }

        if (curr_filter->bitmask & WLAN_P2P_PROT_DISC_DEV_FILTER_BITMASK_DEVICE) {
            /* Check the sender MAC address to look for the P2P device */
            if (IEEE80211_ADDR_EQ(curr_filter->device_id, frame->src_addr)) {
                /* There is a match */
                IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                     "%s: Matched src address="MAC_ADDR_FMT" and p2p device ID.\n",
                     __func__, SPLIT_MAC_ADDR(curr_filter->device_id));
                matched = true;
                curr_filter->discovered = true;
                continue;
            }
        }

        if (curr_filter->bitmask & WLAN_P2P_PROT_DISC_DEV_FILTER_BITMASK_GO) {
            /* Check if this frame is from a GO with desired device ID and SSID */
            struct find_GO_param_with_chan  param;

            OS_MEMZERO(&param, sizeof(struct find_GO_param_with_chan));
            param.prot_handle = prot_handle;

            IEEE80211_ADDR_COPY(param.mac_addr, curr_filter->device_id);
            param.rx_freq = frame->freq;
            if (curr_filter->grp_ssid_is_p2p_wildcard) {
                /* Any SSID */
                param.grp_ssid = NULL;
            }
            else {
                param.grp_ssid = &(curr_filter->grp_ssid);
            }

            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                 "%s: Find p2p device="MAC_ADDR_FMT" on freq=%d with frame from "MAC_ADDR_FMT".\n",
                 __func__, SPLIT_MAC_ADDR(curr_filter->device_id), frame->freq,
                 SPLIT_MAC_ADDR(frame->src_addr));

            /* Look for the P2P Device but not into the GO's group info */
            wlan_scan_macaddr_iterate(prot_handle->vap, (u_int8_t *)frame->src_addr, p2p_disc_find_GO_on_same_freq, &param);

            if (param.match_found) {
                IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                     "%s: Found p2p device="MAC_ADDR_FMT" in ta="MAC_ADDR_FMT"\n",
                     __func__, SPLIT_MAC_ADDR(curr_filter->device_id), SPLIT_MAC_ADDR(param.ta_addr));
                matched = true;
                curr_filter->discovered = true;
                continue;
            }
        }
    }

    if (matched) {
        /* Make sure that all filters are matched */
        for (i = 0; i < prot_handle->desired_peer.num_dev_filters; i++) {
            wlan_p2p_prot_disc_dev_filter   *curr_filter;

            curr_filter = &(prot_handle->desired_peer.dev_filter[i]);
            if (!curr_filter->discovered) {
                /* One filter not discovered yet. */
                IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                     "%s: Filter %d not discovered yet. Do not indicate.\n", i);
                matched = false;
                break;
            }
        }
    }

    return(matched);
}

static void
update_dev_filters(wlan_p2p_prot_t prot_handle)
{
    /* Check if we need to update the desired peer information. */
    if (atomic_read(&prot_handle->desired_peer.pend_update)) {
        /* We will need to update the desire peer information */
        spin_lock(&(prot_handle->desired_peer.lock)); 

        /* read the pend_update flag again in case it is change in another thread */
        if (atomic_read(&prot_handle->desired_peer.pend_update) != 0) {
            /* copy over the peer information */

            if (prot_handle->desired_peer.pend_have_dev_id) {
                /* Have pending device filters. Copy them over to the current filters */
                prot_handle->desired_peer.num_dev_filters = prot_handle->desired_peer.pend_num_dev_filters;
                OS_MEMCPY(prot_handle->desired_peer.dev_filter, prot_handle->desired_peer.pending_dev_filter,
                          prot_handle->desired_peer.pend_num_dev_filters * sizeof(wlan_p2p_prot_disc_dev_filter));

                IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                     "%s: set %d desired peers.\n", __func__, prot_handle->desired_peer.num_dev_filters);
                if (prot_handle->desired_peer.num_dev_filters > 0) {
                    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                         "\tFirst filter for address="MAC_ADDR_FMT". bitmask=%d.\n",
                         SPLIT_MAC_ADDR(prot_handle->desired_peer.dev_filter[0].device_id), 
                         prot_handle->desired_peer.dev_filter[0].bitmask);

                    ASSERT(prot_handle->desired_peer.dev_filter[0].discovered == false);
                }

                atomic_set(&prot_handle->desired_peer.avail, 1);
            }
            else {
                /* No desired device ID */

                IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                     "%s: clear desired peers.\n", __func__);

                atomic_set(&prot_handle->desired_peer.avail, 0);
            }

            /* Note: make sure this flag is the last line before releasing the lock. */
            atomic_set(&prot_handle->desired_peer.pend_update, 0);
        }
        else {
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                 "%s: Note: prot_handle->desired_peer.pend_update changes during locking\n", __func__);
        }

        /* Else it is possible that another thread has done the switch */
        spin_unlock(&(prot_handle->desired_peer.lock)); 
    }
}

/*
 * This function will check the received Probe Response or Beacon frame to
 * check if it is from a desired peer. Note that we are looking for the
 * p2p device address which could be inside the P2P IE attributes.
 * Returns true if the desired peer is found.
 * Note: For performance reasons, this routine will not need to acquire the spinlock
 * unless there is change in the desired peer information.
 */
static bool
proc_rx_probe_resp_fr(wlan_p2p_prot_t prot_handle, struct wlan_p2p_rx_frame *frame)
{
    /* For performance reasons, we do not want to hold a spinlock
     * to do the address check. */
    bool        all_peers_found = false;

    /* For synchronization reasons, check if we need to update the device filters */
    update_dev_filters(prot_handle);

    if (atomic_read(&prot_handle->desired_peer.avail) != 0) {
        /* Check for the P2P device address */
        all_peers_found = check_for_desired_peers(prot_handle, frame);
    }

    return(all_peers_found);
}

/*
 * This function is called to set the device filters during discovery scan.
 * This routine is called from the statemachine and should not be re-entrant.
 */
static void
set_device_filters(wlan_p2p_prot_t prot_handle)
{
    disc_scan_event_info            *disc_scan;
    wlan_p2p_prot_disc_dev_filter   *dev_filter;

    ASSERT(prot_handle->curr_disc_scan_info != NULL);
    disc_scan = prot_handle->curr_disc_scan_info;

    if (disc_scan->in_dev_filter_list_num == 0) {
        /* No device filter. Nothing to do */
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
             "%s: Error: no device filter. Should not call this function.\n",
             __func__);
        return;
    }

    ASSERT(disc_scan->in_dev_filter_list_num <= WLAN_P2P_PROT_MAX_NUM_DISC_DEVICE_FILTER);

    spin_lock(&(prot_handle->desired_peer.lock));

    /* Copy to a holding area first */
    dev_filter = (wlan_p2p_prot_disc_dev_filter *)&(((u_int8_t *)disc_scan)[disc_scan->in_dev_filter_list_offset]);

    prot_handle->desired_peer.pend_num_dev_filters = disc_scan->in_dev_filter_list_num;
    OS_MEMCPY(prot_handle->desired_peer.pending_dev_filter, dev_filter,
                disc_scan->in_dev_filter_list_num * sizeof(wlan_p2p_prot_disc_dev_filter));

    prot_handle->desired_peer.pend_have_dev_id = true;

    /* Note: we will do the update later in update_desired_peers() */
    atomic_set(&prot_handle->desired_peer.pend_update, 1);

    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
         "%s: Set pending desired peers. First one="MAC_ADDR_FMT".\n",
         __func__, SPLIT_MAC_ADDR(dev_filter->device_id));

    spin_unlock(&(prot_handle->desired_peer.lock)); 
}

/*
 * This function is called to clear the desired peer.
 * This routine is called from the statemachine and should not be re-entrant.
 */
static void
clear_desired_peer(wlan_p2p_prot_t prot_handle)
{
    spin_lock(&(prot_handle->desired_peer.lock));

    if (prot_handle->desired_peer.pend_have_dev_id) {
        prot_handle->desired_peer.pend_have_dev_id = false;
        atomic_set(&prot_handle->desired_peer.pend_update, 1);
    }

    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
         "%s: pending clear desired peer.\n", __func__);

    spin_unlock(&(prot_handle->desired_peer.lock)); 
}

/*
 * Routine to process the Rx frame callback from the lower P2P Device driver.
 * Will return true if this frame is to be indicated. Return false if this
 * frame is to be dropped.
 */
static bool
proc_rx_frame(wlan_p2p_prot_t prot_handle, wlan_p2p_event *p2p_dev_event)
{
    bool    do_indicate = false;
    wlan_p2p_prot_event *event = NULL;
    wlan_p2p_prot_event_rx_act_fr_ind   *rx_indicate;

    switch(p2p_dev_event->u.rx_frame.frame_type)
    {
    case IEEE80211_FC0_SUBTYPE_ACTION:
        proc_rx_action_frame(prot_handle, p2p_dev_event);
        do_indicate = false;    /* Action frames are processed directly without going through the state machine */
        break;
    case IEEE80211_FC0_SUBTYPE_PROBE_REQ:
        /* No further processing of Probe Request are needed for now */
        break;
    case IEEE80211_FC0_SUBTYPE_PROBE_RESP:
    case IEEE80211_FC0_SUBTYPE_BEACON:
        /*
         * During discovery scan that is target or mini-find, then search for
         * MAC address of the targeted peer.
         */
        if (proc_rx_probe_resp_fr(prot_handle, &(p2p_dev_event->u.rx_frame))) {
            /* Found a desired peer. Indicate it to the state machine. */
            do_indicate = true;
        }
        break;
    }

    if (!do_indicate) {
        /* Nothing further to do since not indicating to upper layers */
        return do_indicate;
    }
    /* Else we need to make a copy of the frame buffer */

    return do_indicate;
}

/* 
 * Registered Handler for events from the P2P Device
 */
static VOID
p2p_prot_device_evhandler(void *arg, wlan_p2p_event *p2p_dev_event)
{
    wlan_p2p_prot_t                 prot_handle = (wlan_p2p_prot_t)arg;
    sm_event_info                   sm_event;
    ieee80211_connection_event      event_type = P2P_DISC_EVENT_NULL;
    bool                            do_indicate = false;

    switch (p2p_dev_event->type) {
    case WLAN_P2PDEV_CHAN_START:
        event_type = P2P_DISC_EVENT_ON_CHAN_START;
        break;

    case WLAN_P2PDEV_CHAN_END:
        event_type = P2P_DISC_EVENT_ON_CHAN_END;
        break;

    case WLAN_P2PDEV_RX_FRAME:
        do_indicate = proc_rx_frame(prot_handle, p2p_dev_event);
        if (do_indicate) {
            /* Note that contents of the received frame will be dealloc after this call.
             * Therefore, these fields src_addr, wbuf, ie_buf, frame_buf, etc are invalid
             * when this function returns. */
            event_type = P2P_DISC_EVENT_RX_MGMT;
        }
        break;

    default:
        break;
    }

    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                      "%s: P2P Device event: evt=%d req_id=%d, event=%s\n", 
                      __func__, p2p_dev_event->type, p2p_dev_event->req_id, event_name(event_type));

    if (event_type != P2P_DISC_EVENT_NULL) {
        /* Send this P2P Device event to this state machine */
        OS_MEMZERO(&sm_event, sizeof(sm_event_info));
        OS_MEMCPY(&sm_event.p2p_dev_cb, p2p_dev_event, sizeof(wlan_p2p_event));

        sm_event.event_type = event_type;

        if (event_type == P2P_DISC_EVENT_RX_MGMT) {
            /* Since the memory buffer for the received frame will be freed after this
             * call, we will NULL the appropriate fields. If we need them, then we need
             * to allocate space for the fields.
             */
            sm_event.p2p_dev_cb.u.rx_frame.frame_buf = NULL;
            sm_event.p2p_dev_cb.u.rx_frame.ie_buf = NULL;
            sm_event.p2p_dev_cb.u.rx_frame.src_addr = NULL;
            sm_event.p2p_dev_cb.u.rx_frame.wbuf = NULL;
        }

        ieee80211_sm_dispatch(prot_handle->hsm_handle, event_type, sizeof(sm_event_info), &sm_event);
    }
}

/**
 * return vaphandle associated with underlying p2p protocol device.
 * @param p2p_prot_handle         : handle to the p2p protocol object .
 * @return vaphandle.
 */
wlan_if_t
wlan_p2p_prot_get_vap_handle(wlan_p2p_prot_t  prot_handle)
{
    return (prot_handle->vap);
}

/*
 * Returns true if P2P is supported on this channel.
 */
static bool
p2p_prot_p2p_supported_chan(wlan_chan_t chan)
{
    if (wlan_channel_is_dfs(chan, true)) {
        /* Remove this DFS channel */
        return(false);
    }

    if (wlan_channel_is_passive(chan)) {
        /* Remove this passive channel */
        return(false);
    }
    return(true);
}

/*
 * Add the P2P channel into the channel list. We assumed that the channel numbers in the
 * existing channel lists are ordered in ascending order.
 * Return 1 if channel is already inside the current list.
 * Return 0 if new channel is added. Else return <0 if error.
 */
int
p2p_channels_add_new(struct p2p_channels *channels, u8 reg_class, u8 channel)
{
    size_t i, j, k;
    for (i = 0; i < channels->reg_classes; i++) {
        struct p2p_reg_class *reg = &channels->reg_class[i];
        if (reg->reg_class != reg_class)
            continue;
        /* Else there is already an existing reg_class */
        ASSERT(reg->channels >= 1);
        for (j = 0; j < reg->channels; j++) {
            if (reg->channel[j] == channel)
                /* A match! */
                return 1;
            if (reg->channel[j] > channel) {
                /* Need to squeeze this new channel inside this slot */
                break;
            }
        }
        /* We will need to insert this new channel into reg->channel[j] */
        if (reg->channels >= P2P_MAX_REG_CLASS_CHANNELS) {
            /* Error: already max. number of channels */
            return -1;
        }
        /* Move the rest of channel numbers back by one index */
        for (k = (reg->channels - 1); k >= j; k--) {
            reg->channel[k + 1] = reg->channel[k];
        }
        reg->channel[j] = channel;
        reg->channels++;
        return 0;
    }
    /* Add a new reg_class */
    if (channels->reg_classes >= P2P_MAX_REG_CLASSES) {
        /* Error: already max. number of reg_class */
        return -1;
    }

    {
        struct p2p_reg_class *new_reg_class;

        new_reg_class = &channels->reg_class[channels->reg_classes];

        new_reg_class->reg_class = reg_class;
        ASSERT(new_reg_class->channels == 0);
        new_reg_class->channel[0] = channel;
        new_reg_class->channels = 1;
        channels->reg_classes++;
    }
    return 0;
}

/*
 * Routine to get the supported channel list and initialize the chanlist field.
 */
static void
p2p_prot_get_chan_list(wlan_p2p_prot_t prot_handle)
{
    const int       max_num_channels =  (IEEE80211_CHAN_MAX + 1);
    wlan_chan_t     *chan_list = NULL;
    int             succeed = false;
    int             num_p2p_chans = 0;

    do {
        size_t      alloc_buf_size;
        int         num_chans;
        int         i;

        //
        // Allocate memory for channel list
        //
        alloc_buf_size = max_num_channels * sizeof(wlan_chan_t);
        chan_list = (wlan_chan_t *)OS_MALLOC(prot_handle->os_handle, alloc_buf_size, 0);
        if (chan_list == NULL) {
            IEEE80211_DPRINTF(prot_handle->vap, IEEE80211_MSG_P2P_PROT, 
                              "%s: Error: failed to alloc param buf, size=%d\n",
                              __func__, alloc_buf_size);
            break;
        }
        OS_MEMZERO(chan_list, alloc_buf_size);

        num_chans = wlan_get_channel_list(wlan_vap_get_devhandle(prot_handle->vap),
                                          IEEE80211_MODE_AUTO,
                                          chan_list,
                                          max_num_channels);
        if (num_chans < 0) {
            IEEE80211_DPRINTF(prot_handle->vap, IEEE80211_MSG_P2P_PROT, 
                              "%s: Error: wlan_get_channel_list ret=0x%x\n",
                              __func__, num_chans);
            break;
        }

        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                             "%s: wlan_get_channel_list returns num_chans=%d\n", __func__, num_chans);

        /* GO through each channel and remove the unsupported channels (DFS channels and passive) */
        for (i = 0; i < num_chans; i++) {

            u_int32_t   freq = wlan_channel_frequency(chan_list[i]);
            u_int32_t   flags = wlan_channel_flags(chan_list[i]);
            u_int32_t   ieee = wlan_channel_ieee(chan_list[i]);
            int         retval;
            u8          p2p_reg_class;
            u8          p2p_channel;

            if (!p2p_prot_p2p_supported_chan(chan_list[i])) {
                /* This channel is not supported by P2P */
                IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                                     "%s: remove unsupported channel, freq=%d, ieee=%d, flags=0x%x\n",
                                     __func__, freq, ieee, flags);
                continue;
            }

            /* Convert from UMAC channel to P2P channel */
            ASSERT(prot_handle->country[2] == 0x04);
            retval = wlan_p2p_chan_convert_to_p2p_channel(chan_list[i], (const char *)prot_handle->country,
                                                          &p2p_reg_class, &p2p_channel);
            if (retval != 0) {
                /* Cannot find the equivalent P2P channel */
                IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                                     "%s: cannot find p2p reg_class and chan num, freq=%d, ieee=%d, flags=0x%x\n",
                                     __func__, freq, ieee, flags);
                continue;
            }

            /* Add this channel to the list */
            retval = p2p_channels_add_new(&prot_handle->chanlist, p2p_reg_class, p2p_channel);
            if (retval < 0) {
                IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                                     "%s: Error in adding new p2p channel, reg_class=%d, chan=%d, freq=%d, ieee=%d, flags=0x%x\n",
                                     __func__, p2p_reg_class, p2p_channel, freq, ieee, flags);
                continue;
            }
            else if (retval == 0) {
                IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                                        "%s: Adding new p2p channel, reg_class=%d, chan=%d, freq=%d, ieee=%d, flags=0x%x\n",
                                        __func__, p2p_reg_class, p2p_channel, freq, ieee, flags);
                num_p2p_chans++;
            }
        }

        succeed = true;

    } while (FALSE);

    if (chan_list != NULL) {
        OS_FREE(chan_list);
        chan_list = NULL;
    }

    if (!succeed) {
        /* Some error. Replace with the default list. */
        int     i;

        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                             "%s: replace with default channel list of channel 1-11\n", __func__);

        prot_handle->chanlist.reg_classes = 1;
        prot_handle->chanlist.reg_class[0].reg_class = 81;  /* hardcoded to be 2.4 GHz */
        prot_handle->chanlist.reg_class[0].channels = 11;
        for (i = 0; i < prot_handle->chanlist.reg_class[0].channels; i++) {
            prot_handle->chanlist.reg_class[0].channel[i] = i + 1;
        }
    }
    else {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                                "%s: Added %d p2p channels\n",
                                __func__, num_p2p_chans);
    }
}

/*
 * To create the P2P Protocol Device.
 * Return P2P Protocol Device handle if succeed. Else return NULL if error.
 */
wlan_p2p_prot_t wlan_p2p_prot_create(wlan_dev_t dev_handle, wlan_p2p_params *p2p_params,
                                     void *event_arg, ieee80211_p2p_prot_event_handler ev_handler)
{
    osdev_t                 oshandle;
    wlan_p2p_prot_t         prot_handle = NULL;
    int                     status = EOK;
    wlan_if_t               vap = NULL;
    bool                    scan_registered = false;
    ieee80211_hsm_t         hsm_handle = NULL;
    bool                    got_requestor_id = false;
    bool                    p2p_event_registered = false;
    wlan_p2p_t              p2pdev_handle = NULL;
    struct wlan_mlme_app_ie *h_app_ie = NULL;

    /* Check that P2P is supported */
    if (wlan_get_device_param(dev_handle, IEEE80211_DEVICE_P2P) == 0) {
        ieee80211com_note(dev_handle, IEEE80211_MSG_P2P_PROT,
            "%s: Error: P2P unsupported.\n", __func__);
        return NULL;
    }

    if (ev_handler == NULL) {
        /* Must have an event handler to callback */
        ieee80211com_note(dev_handle, IEEE80211_MSG_P2P_PROT,
             "%s: Error: No event handler callback parameter.\n", __func__);
        return NULL;
    }

    oshandle = ieee80211com_get_oshandle(dev_handle);
    prot_handle = (wlan_p2p_prot_t) OS_MALLOC(oshandle, sizeof(struct ieee80211_p2p_prot), 0);
    if (prot_handle == NULL) {
        ieee80211com_note(dev_handle, IEEE80211_MSG_P2P_PROT,
            "%s p2p protocol device allocation failed\n", __func__);
        return NULL;
    }
    OS_MEMZERO(prot_handle, sizeof(struct ieee80211_p2p_prot));
    prot_handle->os_handle = oshandle;

    do {
        /* Create the P2P Device */

        p2pdev_handle = prot_handle->p2p_device_handle = wlan_p2p_create(dev_handle, p2p_params);
        if (p2pdev_handle == NULL) {
            ieee80211com_note(dev_handle, IEEE80211_MSG_P2P_PROT,
                 "%s p2p device creation failed\n", __func__);
            status = -EIO;
            break;
        }

        vap = wlan_p2p_get_vap_handle(p2pdev_handle);
        if (vap == NULL) {
            break;
        }
        prot_handle->vap = vap;
        prot_handle->event_arg = event_arg;
        prot_handle->ev_handler = ev_handler;
        prot_handle->curr_scan.vap = prot_handle->vap;

        /* Create the state machine */
        /* Create State Machine and start */
        hsm_handle = ieee80211_sm_create(oshandle, 
                         "P2PPROTDISC",
                         (void *)prot_handle, 
                         P2P_DISC_STATE_OFF,
                         p2p_prot_disc_sm_info,
                         sizeof(p2p_prot_disc_sm_info)/sizeof(ieee80211_state_info),
                         MAX_QUEUED_EVENTS,
                         sizeof(sm_event_info), /* size of event data */
                         MESGQ_PRIORITY_HIGH,
                         /* Note for darwin, we get more accurate timings for the NOA start and stop when synchronous */
                         OS_MESGQ_CAN_SEND_SYNC() ? IEEE80211_HSM_SYNCHRONOUS :
                                                    IEEE80211_HSM_ASYNCHRONOUS,
                         p2p_prot_disc_sm_debug_print,
                         p2p_prot_disc_sm_event_name,
                         IEEE80211_N(p2p_prot_disc_sm_event_name)); 
        if (!hsm_handle) {
            IEEE80211_DPRINTF(prot_handle->vap, IEEE80211_MSG_P2P_PROT, 
                "%s : ieee80211_sm_create failed for p2p protocol discovery state machine.\n", __func__);
            status = -ENOMEM;
            break;
        }

        prot_handle->hsm_handle = hsm_handle;

        /* Register an Application IE handle so that we can append to the frames' IE's */
        h_app_ie = wlan_mlme_app_ie_create(vap);
        if (h_app_ie == NULL) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, 
                "%s: Error in allocating Application IE\n", __func__);
            status = -ENOMEM;
            break;
        }
        prot_handle->h_app_ie = h_app_ie;

        /* Register event handler with Scanner */
        status = wlan_scan_register_event_handler(vap, p2p_prot_scan_evhandler, prot_handle);
        if (status != EOK) {
            IEEE80211_DPRINTF(prot_handle->vap, IEEE80211_MSG_P2P_PROT, 
                              "%s: wlan_scan_register_event_handler() failed handler=%08p,%08p status=%08X\n",
                              __func__, p2p_prot_scan_evhandler, prot_handle, status);
            break;
        }
        scan_registered = true;

        /* Register P2P event handler */
        status = wlan_p2p_register_event_handlers(p2pdev_handle, prot_handle, p2p_prot_device_evhandler);
        if (status != EOK) {
            IEEE80211_DPRINTF(prot_handle->vap, IEEE80211_MSG_P2P_PROT, 
                              "%s: wlan_p2p_register_event_handlers() failed handler=%08p,%08p status=%08X\n",
                              __func__, p2p_prot_device_evhandler, prot_handle, status);
            break;
        }
        p2p_event_registered = true;

        /* Get my unique scan requestor ID so that I can tagged all my scan requests */
        status = wlan_scan_get_requestor_id(prot_handle->vap, (u_int8_t *) "P2P_PROT", 
                                            &(prot_handle->my_scan_requestor));
        if (status != EOK) {
            IEEE80211_DPRINTF(prot_handle->vap, IEEE80211_MSG_P2P_PROT, 
                              "%s: wlan_scan_get_requestor_id() failed status=%08X\n",
                              __func__, status);
            break;
        }
        got_requestor_id = true;

        spin_lock_init(&prot_handle->set_param_lock);

        p2p_disc_timer_init(prot_handle);

        /* Start initialization of this structure */
        prot_handle->min_disc_int = 1;
        prot_handle->max_disc_int = 3;

        {
            /* Initialize the channels for social scan */
            int         i;
            u_int32_t   social_channels[IEEE80211_P2P_NUM_SOCIAL_CHANNELS] = {IEEE80211_P2P_SOCIAL_CHANNELS};

            for (i = 0; i < IEEE80211_P2P_NUM_SOCIAL_CHANNELS; i++) {
                prot_handle->curr_scan.social_chans_list[i] = social_channels[i];
            }
        }

        /* Init. my Listen channel */
        //STNG TODO: we need a way to configure our listen channel. Maybe by registry?
        prot_handle->my_listen_chan = ieee80211_find_dot11_channel(dev_handle, P2P_PROT_DEFAULT_LISTEN_CHAN, 0, IEEE80211_MODE_AUTO);
        if (prot_handle->my_listen_chan == NULL) {
            IEEE80211_DPRINTF(prot_handle->vap, IEEE80211_MSG_P2P_PROT,
                              "%s: Error: cannot init. listen channel=%d.\n",
                              __func__, P2P_PROT_DEFAULT_LISTEN_CHAN);
            status = -EPERM;
            break;
        }

        /* Get the country code */
        ieee80211_getCurrentCountryISO(dev_handle, (char *)prot_handle->country);

        IEEE80211_DPRINTF(prot_handle->vap, IEEE80211_MSG_P2P_PROT, 
                          "%s: got country code: 0x%x 0x%x 0x%x but 3rd octet changed to 0x04\n",
                          __func__, prot_handle->country[0], prot_handle->country[1], prot_handle->country[2]);

        /* The third octet of the Country String field is set to 0x04 to indicate that Table J-4 is used. */
        prot_handle->country[2] = 0x04;

        /* Get the support channels list and initialize the prot_handle->chanlist field. */
        p2p_prot_get_chan_list(prot_handle);

        /* Prepare the scan list - all channels to scan. We will extract the unique channel
         * channel number from the P2P supported channel list.  */
        {
            size_t      i, j, k;
            u_int32_t   chan_count;

            prot_handle->curr_scan.total_num_all_chans = chan_count = 0;

            for (i = 0; i < prot_handle->chanlist.reg_classes; i++) {
                const struct p2p_reg_class *reg = &prot_handle->chanlist.reg_class[i];

                for (j = 0; j < reg->channels; j++) {

                    bool    match_found = false;
                    u8      curr_chan_num = reg->channel[j];

                    /* GO through the scan chan list to find a match */
                    for (k = 0; k < chan_count; k++) {
                        if (prot_handle->curr_scan.all_chans_list[k] == curr_chan_num) {
                            match_found = true;
                            break;
                        }
                    }

                    if (!match_found) {
                        if (chan_count >= IEEE80211_CHAN_MAX) {
                            /* Error: exceed the list size */
                            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                                                 "%s: Error: too many scan channels, max size=%d\n",
                                                 __func__, IEEE80211_CHAN_MAX);
                            break;
                        }
                        else {
                            prot_handle->curr_scan.all_chans_list[chan_count] = curr_chan_num;
                            chan_count++;
                        }
                    }
                }
            }
            prot_handle->curr_scan.total_num_all_chans = chan_count;
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                              "%s: Found %d unique channel numbers\n",__func__, chan_count);
        }

        /* STNG TODO: we need registry keys to initialize the Listen and Operational Channels */
        prot_handle->listen_chan.ieee_num = P2P_PROT_DEFAULT_LISTEN_CHAN;
        prot_handle->listen_chan.reg_class = 81;        /* hardcoded to be 2.4 GHz */
        prot_handle->op_chan.ieee_num = P2P_PROT_DEFAULT_OP_CHAN;
        prot_handle->op_chan.reg_class = 81;        /* hardcoded to be 2.4 GHz */

        prot_handle->ext_listen_interval = 0;
        //STNG TODO: needs to set prot_handle->ext_listen_period and prot_handle->ext_listen_interval

        prot_handle->curr_scan.my_scan_requestor = prot_handle->my_scan_requestor;

        IEEE80211_DPRINTF(prot_handle->vap, IEEE80211_MSG_P2P_PROT, 
                          "%s: success. p2pdevice=%pK, vap=%pK, my_scan_requestor=%d\n",
                          __func__, p2pdev_handle, vap, prot_handle->my_scan_requestor);

        /* Tell the state machine to start */
        ieee80211_sm_dispatch(prot_handle->hsm_handle, P2P_DISC_EVENT_START, 0, NULL);

    } while ( false );

    if (status != EOK) {
        /* Error. Clean up in reverse order. */

        if (got_requestor_id) {
            p2p_disc_timer_cleanup(prot_handle);

            wlan_scan_clear_requestor_id(vap, prot_handle->my_scan_requestor);

            spin_lock_destroy(&prot_handle->set_param_lock);
        }

        /* Deregister P2P event handler */
        if (p2p_event_registered) {
            wlan_p2p_register_event_handlers(p2pdev_handle, prot_handle, NULL);
        }

        if (scan_registered) {
            status = wlan_scan_unregister_event_handler(vap, p2p_prot_scan_evhandler, prot_handle);
            if (status != EOK) {
                IEEE80211_DPRINTF(prot_handle->vap, IEEE80211_MSG_P2P_PROT, 
                                  "%s: wlan_scan_unregister_event_handler() failed handler=%08p,%08p status=%08X\n",
                                  __func__, p2p_prot_scan_evhandler, prot_handle, status);
            }
        }

        if (h_app_ie) {
            wlan_mlme_remove_ie_list(h_app_ie);
            h_app_ie = NULL;
        }

        /* Delete the State Machine */
        if (hsm_handle) {
            ieee80211_sm_delete(hsm_handle);
        }

        if (p2pdev_handle) {
            wlan_p2p_delete(p2pdev_handle);
        }

        if (prot_handle) {
            OS_FREE(prot_handle);
            prot_handle = NULL;
        }

        IEEE80211_DPRINTF(prot_handle->vap, IEEE80211_MSG_P2P_PROT, 
                          "%s: Failed.\n", __func__);

        return NULL;
    }
    else {
        ASSERT(prot_handle);
        return prot_handle;
    }
} /* end of function wlan_p2p_prot_create() */

/**
 * stop p2p protocol device object.
 * @param p2p_prot_handle : handle to the p2p protocol object .
 *
 * @return on success returns 0.
 *         on failure returns a negative value.
 */
int
wlan_p2p_prot_stop(wlan_p2p_prot_t prot_handle, u_int32_t flags)
{
    int             status;

    if (prot_handle->p2p_device_handle) {
        wlan_p2p_stop(prot_handle->p2p_device_handle, flags);
    }

    /* Tell the state machine to go back to IDLE state */
    ieee80211_sm_dispatch(prot_handle->hsm_handle, P2P_DISC_EVENT_RESET, 0, NULL);

    /* IMPORTANT STNG TODO: add code to disable the P2P Protocol module. Also need to make sure all is quiet. */

    return EOK;
}

void
wlan_p2p_prot_reset_start(wlan_p2p_prot_t prot_handle, ieee80211_reset_request *reset_req)
{
    wlan_p2p_reset_start(prot_handle->p2p_device_handle);
}

void
wlan_p2p_prot_reset_end(wlan_p2p_prot_t prot_handle, ieee80211_reset_request *reset_req)
{
    wlan_p2p_reset_end(prot_handle->p2p_device_handle);
}

/**
 * delete p2p protocol device object.
 * @param p2p_prot_handle : handle to the p2p protocol object .
 *
 * @return on success returns 0.
 *         on failure returns a negative value.
 */
int
wlan_p2p_prot_delete(wlan_p2p_prot_t prot_handle)
{
    int             status;

    /* Tell the state machine to go back to OFF state */
    ieee80211_sm_dispatch(prot_handle->hsm_handle, P2P_DISC_EVENT_END, 0, NULL);

    p2p_disc_timer_cleanup(prot_handle);

    wlan_scan_clear_requestor_id(prot_handle->vap, prot_handle->my_scan_requestor);

    /* Deregister P2P event handler */
    if (prot_handle->p2p_device_handle) {
        wlan_p2p_register_event_handlers(prot_handle->p2p_device_handle, prot_handle, NULL);
    }

    status = wlan_scan_unregister_event_handler(prot_handle->vap, p2p_prot_scan_evhandler, prot_handle);
    if (status != EOK) {
        IEEE80211_DPRINTF(prot_handle->vap, IEEE80211_MSG_P2P_PROT, 
                          "%s: wlan_scan_unregister_event_handler() failed handler=%08p,%08p status=%08X\n",
                          __func__, p2p_prot_scan_evhandler, prot_handle, status);
    }

    if (prot_handle->h_app_ie) {
        wlan_mlme_remove_ie_list(prot_handle->h_app_ie);
        prot_handle->h_app_ie = NULL;
    }

    /* Delete the State Machine */
    if (prot_handle->hsm_handle) {
        ieee80211_sm_delete(prot_handle->hsm_handle);
    }

    if (prot_handle->secondaryDeviceTypes) {
        OS_FREE(prot_handle->secondaryDeviceTypes);
    }

    if (prot_handle->probe_req_add_ie) {
        OS_FREE(prot_handle->probe_req_add_ie);
    }

    if (prot_handle->p2p_device_handle) {
        wlan_p2p_delete(prot_handle->p2p_device_handle);
    }

    spin_lock_destroy(&prot_handle->set_param_lock);

    OS_FREE(prot_handle);
    prot_handle = NULL;

    return EOK;
}

/*
 * Routine to assign a new dialog token. Note that this function may be called outside
 * the state machine and needs synchronization.
 */
u_int8_t
wlan_p2p_prot_generate_dialog_token(wlan_p2p_prot_t prot_handle)
{
    atomic_t        token;

    token = (atomic_inc(&prot_handle->dialog_token)) & 0x0FF;

    if (token == 0) {
        /* Redo since dialog token cannot be zero */
        token = (atomic_inc(&prot_handle->dialog_token)) & 0x0FF;
    }
    return((u_int8_t)token);
}

/*
 * Routine to send an Action Frame of the "request" type.
 * If successful, the new Dialog Token is returned.
 */
int
wlan_p2p_prot_tx_action_request_fr(wlan_p2p_prot_t prot_handle, 
                                   wlan_p2p_prot_tx_act_fr_param *tx_param
                                   )
{
    ieee80211_connection_event          event;
    int                                 param_size, add_ie_size;
    tx_event_info                       *req_param;
    u_int8_t                            *add_ie_buf;
    sm_event_info                       sm_event;
    wlan_scan_entry_t                   peer = NULL;
    ieee80211_scan_p2p_info             entry_p2p_info;
    u_int16_t                           dest_freq = 0;
    u_int8_t                            *peer_macaddr = NULL;
    u_int32_t                           timeout = 0;
    u_int8_t                            dtoken = 0;
    wlan_p2p_prot_grp_id                *group_id = NULL;
    bool                                peer_is_found;

    switch(tx_param->frtype) {
    case IEEE80211_P2P_PROT_FTYPE_GO_NEG_REQ:
        event = P2P_DISC_EVENT_TX_GO_NEG_REQ;
        add_ie_size = tx_param->go_neg_req.add_ie_buf_len;
        add_ie_buf = tx_param->go_neg_req.add_ie_buf;
        peer_macaddr = tx_param->go_neg_req.peer_addr;
        timeout = tx_param->go_neg_req.send_timeout;
        dtoken = tx_param->go_neg_req.dialog_token;
        break;
    case IEEE80211_P2P_PROT_FTYPE_PROV_DISC_REQ:
        event = P2P_DISC_EVENT_TX_PROV_DISC_REQ;
        add_ie_size = tx_param->prov_disc_req.add_ie_buf_len;
        add_ie_buf = tx_param->prov_disc_req.add_ie_buf;
        /* 
         * The logic for the destination address is that if the GroupID is specified,
         * then we assumed that the GO has started. Since it is easier to find the GO,
         * we will send this Prov Discovery Request frame to the operational channel
         * of this GO. We need to compare SSID so that we get the right GO since each device
         * may have multiple GOs started.
         */
        if (tx_param->prov_disc_req.use_grp_id) {
            /* Have Group ID */
            group_id = &tx_param->prov_disc_req.grp_id;
        }
        peer_macaddr = tx_param->prov_disc_req.peer_addr;

        timeout = tx_param->prov_disc_req.send_timeout;
        dtoken = tx_param->prov_disc_req.dialog_token;
        break;
    case IEEE80211_P2P_PROT_FTYPE_INVIT_REQ:
        event = P2P_DISC_EVENT_TX_INVITE_REQ;
        add_ie_size = tx_param->invit_req.add_ie_buf_len;
        add_ie_buf = tx_param->invit_req.add_ie_buf;
        peer_macaddr = tx_param->invit_req.peer_addr;
        timeout = tx_param->invit_req.send_timeout;
        dtoken = tx_param->invit_req.dialog_token;
        break;
    default:
        /* Error: unsuppported frame type for this function */
        IEEE80211_DPRINTF(prot_handle->vap, IEEE80211_MSG_P2P_PROT, 
                          "%s: Error: unsuppported frame type=%d for this function\n",
                          __func__, tx_param->frtype);
        return -EINVAL;
    }
    ASSERT(peer_macaddr != NULL);

    /* Search for this peer */
    {
        ieee80211_ssid  *grp_id_ssid;

        if (group_id) {
            grp_id_ssid = &(group_id->ssid);
        }
        else {
            grp_id_ssid = NULL;
        }
        peer_is_found = find_p2p_peer(prot_handle, peer_macaddr, false, grp_id_ssid, &peer, &entry_p2p_info);
    }

    if (!peer_is_found) {
        /* We do not know where the peer is. We need to start a mini-find */
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                             "%s: start mini-find for " MAC_ADDR_FMT " since we don't have scan entry.\n",
                             __func__, SPLIT_MAC_ADDR(peer_macaddr));
        dest_freq = 0;
    }
    else {
        /* We got the channel to send the action frame */
        dest_freq = entry_p2p_info.listen_chan->ic_freq;
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                             "%s: will try to tx at dest_freq=%d for " MAC_ADDR_FMT ".\n",
                             __func__, dest_freq, SPLIT_MAC_ADDR(peer_macaddr));
    }

    /* Alloc the scatch buffer for this tx request (includes the frame buffer and additional IE) */
    param_size = sizeof(tx_event_info) + add_ie_size;
    req_param = (tx_event_info *)OS_MALLOC(prot_handle->os_handle, param_size, 0);
    if (req_param == NULL) {
        IEEE80211_DPRINTF(prot_handle->vap, IEEE80211_MSG_P2P_PROT, 
                          "%s: failed to alloc param buf, size=%d\n",
                          __func__, param_size);

        if (peer) {
            ieee80211_scan_entry_remove_reference(peer);
        }
        return -ENOMEM;
    }
    OS_MEMZERO(req_param, param_size);
    req_param->alloc_size = param_size;

    if (!peer_is_found) {
        req_param->need_mini_find = true;   /* needs mini-find */
    }
    else {
        ASSERT(peer != NULL);
        ASSERT(entry_p2p_info.is_p2p == true);
        ASSERT(dest_freq != 0);     /* We must know the destination frequency in order to proceed */

        req_param->tx_freq = dest_freq;

        /* Store the peer MAC address as the Receiver Address */
        IEEE80211_ADDR_COPY(req_param->rx_addr, ieee80211_scan_entry_macaddr(peer));

        ieee80211_scan_entry_remove_reference(peer);
    }

    if (group_id != NULL) {
        req_param->have_group_id = true;
        OS_MEMCPY(&req_param->group_id, group_id, sizeof(wlan_p2p_prot_grp_id));
    }
    OS_MEMCPY(req_param->peer_addr, peer_macaddr, IEEE80211_ADDR_LEN);

    ASSERT(timeout != 0);
    req_param->timeout = timeout;

    ASSERT(dtoken != 0);
    req_param->dialog_token = dtoken;

    /* Make a copy of the original parameters */
    OS_MEMCPY(&req_param->orig_req_param, tx_param, sizeof(wlan_p2p_prot_tx_act_fr_param));

    req_param->orig_req_param.prot_handle = prot_handle;

    /* Copy the additional IE to the end of tx_event_info buffer */
    {
        // Copy the IE buffer.
        u_int8_t        *dest_buf = &(((u_int8_t *)req_param)[sizeof(tx_event_info)]);
        OS_MEMCPY(dest_buf, add_ie_buf, add_ie_size);

        req_param->in_ie_offset = sizeof(tx_event_info);
        req_param->in_ie_buf_len = add_ie_size;
    }

    sm_event.event_type = event;
    /* Note: the state handler has to free this allocated memory */
    sm_event.tx_info = req_param;

    ieee80211_sm_dispatch(prot_handle->hsm_handle, event,
                          sizeof(sm_event), &sm_event);

    return EOK;        
}

/*
 * Determine who is the Group Owner. Return 1 if we are the GO and
 * 0 if the peer is the GO, and -1 if both sides want to be GO or error.
 */
static int p2p_go_det(u8 own_intent, u8 peer_value)
{
    u8 peer_intent = peer_value >> 1;
    if (own_intent == peer_intent) {
        if (own_intent == P2P_MAX_GO_INTENT)
            return -1; /* both devices want to become GO */
        
        /* Use tie breaker bit to determine GO */
        return (peer_value & 0x01) ? 0 : 1;
    }
    
    return own_intent > peer_intent;
}

/*
 * Routine to send an Action Frame of the "response/confirm" type.
 */
int
wlan_p2p_prot_tx_action_reply_fr(wlan_p2p_prot_t prot_handle, 
                                 wlan_p2p_prot_tx_act_fr_param *tx_param)
{
    ieee80211_connection_event          event;
    int                                 param_size, add_ie_size;
    tx_event_info                       *req_param;
    u_int8_t                            *add_ie_buf;
    sm_event_info                       sm_event;
    wlan_p2p_prot_rx_context            *rx_context = NULL;
    u_int16_t                           dest_freq = 0;
    u_int8_t                            dialog_token;
    u_int8_t                            *peer_macaddr = NULL;
    bool                                we_are_GO = false;
    u_int32_t                           timeout = 0;
    bool                                prev_fr_valid_chan_list = false;
    struct p2p_channels                 *prev_fr_chan_list = NULL;
    u_int8_t                            *prev_fr_country = NULL;
    bool                                prev_fr_invite_req = false;
    bool                                prev_fr_we_are_GO = false;
    bool                                prev_fr_start_new_group = false;
    wlan_p2p_prot_frame_type            prev_frtype = 0;

    ASSERT(tx_param->prot_handle == prot_handle);

    switch(tx_param->frtype) {
    case IEEE80211_P2P_PROT_FTYPE_GO_NEG_RESP:
        event = P2P_DISC_EVENT_TX_GO_NEG_RESP;
        add_ie_size = tx_param->go_neg_resp.add_ie_buf_len;
        add_ie_buf = tx_param->go_neg_resp.add_ie_buf;
        peer_macaddr = tx_param->go_neg_resp.peer_addr;

        /* Retrieve the destination freq from the Rx Context of the last rx frame */
        ASSERT(tx_param->go_neg_resp.request_context != NULL);
        rx_context = (wlan_p2p_prot_rx_context *)tx_param->go_neg_resp.request_context;
        dest_freq = rx_context->rx_freq;
        dialog_token = tx_param->go_neg_resp.dialog_token;
        timeout = tx_param->go_neg_resp.send_timeout;

        /* Determine who is GO based on the go_intents */
        we_are_GO = p2p_go_det(tx_param->go_neg_resp.go_intent.intent,
                                      rx_context->rx_go_intent);

        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
             "%s: Send GO_NEG_RESP to peer=" MAC_ADDR_FMT ", is_GO=%d, dest_freq=%d, dialog_token=%d, status=%d.\n",
             __func__, SPLIT_MAC_ADDR(peer_macaddr), we_are_GO, dest_freq, dialog_token,
                             tx_param->go_neg_resp.status);
        tx_param->go_neg_resp.request_fr_parsed_ie = rx_context->parsed_ie_ok;

        ASSERT(rx_context->previous_frtype == IEEE80211_P2P_PROT_FTYPE_GO_NEG_REQ);

        /* The channel list information from the previous Request frame */
        prev_fr_valid_chan_list = rx_context->rx_valid_chan_list;
        prev_fr_chan_list = &rx_context->rx_chan_list;
        prev_fr_country = rx_context->rx_country;

        if ((tx_param->go_neg_resp.status == 0) &&
            !rx_context->parsed_ie_ok)
        {
            /* Error: we should not send Success error code if we detect a problem
             * in the original frame.
             */
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                 "%s: Error: respond status is success but errors in original received frame.\n",
                 __func__);
            /* STNG TODO: we need to overwrite the status to failure. */
        }
        break;
    case IEEE80211_P2P_PROT_FTYPE_GO_NEG_CONF:
        event = P2P_DISC_EVENT_TX_GO_NEG_CONF;
        add_ie_size = tx_param->go_neg_conf.add_ie_buf_len;
        add_ie_buf = tx_param->go_neg_conf.add_ie_buf;
        peer_macaddr = tx_param->go_neg_conf.peer_addr;

        /* Retrieve the destination freq from the Rx Context of the last rx frame */
        ASSERT(tx_param->go_neg_conf.request_context != NULL);
        rx_context = (wlan_p2p_prot_rx_context *)tx_param->go_neg_conf.request_context;
        dest_freq = rx_context->rx_freq;
        dialog_token = tx_param->go_neg_conf.dialog_token;
        timeout = tx_param->go_neg_conf.send_timeout;

        if (rx_context->msg.operating_channel != NULL) {
            tx_param->go_neg_conf.resp_have_op_chan = true;
            OS_MEMCPY(tx_param->go_neg_conf.rx_resp_op_chan, rx_context->msg.operating_channel,
                        sizeof(tx_param->go_neg_conf.rx_resp_op_chan));
        }

        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
             "%s: Send GO_NEG_CONF to peer=" MAC_ADDR_FMT ", dest_freq=%d, dialog_token=%d, status=%d.\n",
             __func__, SPLIT_MAC_ADDR(peer_macaddr), dest_freq, dialog_token,
                             tx_param->go_neg_conf.status);

        tx_param->go_neg_conf.response_fr_parsed_ie = rx_context->parsed_ie_ok;

        /* The channel list information from the previous Response frame */
        ASSERT(rx_context->previous_frtype == IEEE80211_P2P_PROT_FTYPE_GO_NEG_RESP);

        prev_fr_valid_chan_list = rx_context->rx_valid_chan_list;
        prev_fr_chan_list = &rx_context->rx_chan_list;
        prev_fr_country = rx_context->rx_country;

        if ((tx_param->go_neg_conf.status == 0) &&
            !rx_context->parsed_ie_ok)
        {
            /* Error: we should not send Success error code if we detect a problem
             * in the original frame.
             */
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                 "%s: Error: respond status is success but errors in original received frame.\n",
                 __func__);
            /* STNG TODO: we need to overwrite the status to failure. */
        }
        break;
    case IEEE80211_P2P_PROT_FTYPE_PROV_DISC_RESP:
        event = P2P_DISC_EVENT_TX_PROV_DISC_RESP;
        add_ie_size = tx_param->prov_disc_resp.add_ie_buf_len;
        add_ie_buf = tx_param->prov_disc_resp.add_ie_buf;
        peer_macaddr = tx_param->prov_disc_resp.rx_addr;

        /* Retrieve the destination freq from the Rx Context of the last rx frame */
        ASSERT(tx_param->prov_disc_resp.request_context != NULL);
        rx_context = (wlan_p2p_prot_rx_context *)tx_param->prov_disc_resp.request_context;
        dest_freq = rx_context->rx_freq;
        dialog_token = tx_param->prov_disc_resp.dialog_token;
        timeout = tx_param->prov_disc_resp.send_timeout;

        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
             "%s: Send PROV_DISC_RESP to peer=" MAC_ADDR_FMT ".\n",
             __func__, SPLIT_MAC_ADDR(tx_param->prov_disc_resp.rx_addr));
        if (!rx_context->parsed_ie_ok)
        {
            /* Error: we should not send Success error code if we detect a problem
             * in the original frame.
             */
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                 "%s: Warning: there is errors in original received request frame.\n",
                 __func__);
        }
        break;
    case IEEE80211_P2P_PROT_FTYPE_INVIT_RESP:
        event = P2P_DISC_EVENT_TX_INVITE_RESP;
        add_ie_size = tx_param->invit_resp.add_ie_buf_len;
        add_ie_buf = tx_param->invit_resp.add_ie_buf;
        peer_macaddr = tx_param->invit_resp.rx_addr;

        /* Retrieve the destination freq from the Rx Context of the last rx frame */
        ASSERT(tx_param->invit_resp.request_context != NULL);
        rx_context = (wlan_p2p_prot_rx_context *)tx_param->invit_resp.request_context;
        dest_freq = rx_context->rx_freq;
        dialog_token = tx_param->invit_resp.dialog_token;
        timeout = tx_param->invit_resp.send_timeout;

        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
             "%s: Send INVITATION_RESP to peer=" MAC_ADDR_FMT ".\n",
             __func__, SPLIT_MAC_ADDR(tx_param->invit_resp.rx_addr));

        tx_param->invit_resp.request_fr_parsed_ie = rx_context->parsed_ie_ok;

        /* The channel list information from the previous Request frame */
        prev_fr_valid_chan_list = rx_context->rx_valid_chan_list;
        prev_fr_chan_list = &rx_context->rx_chan_list;
        prev_fr_country = rx_context->rx_country;

        ASSERT(rx_context->previous_frtype == IEEE80211_P2P_PROT_FTYPE_INVIT_REQ);

        prev_fr_invite_req = true;
        prev_fr_we_are_GO = rx_context->we_are_GO;
        prev_fr_start_new_group = rx_context->start_new_group;
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
             "%s: copy over from previous frame: start_new_group=%d, we_are_GO=%d.\n",
             __func__, rx_context->start_new_group, rx_context->we_are_GO);

        if (rx_context->msg.operating_channel != NULL) {
            tx_param->invit_resp.req_have_op_chan = true;
            OS_MEMCPY(tx_param->invit_resp.rx_req_op_chan, rx_context->msg.operating_channel,
                        sizeof(tx_param->invit_resp.rx_req_op_chan));

            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                 "%s: Op chan from previous invite req frame: reg_class=%d, chan=%d, country=%x %x %x.\n",
                 __func__, tx_param->invit_resp.rx_req_op_chan[3], tx_param->invit_resp.rx_req_op_chan[4],
                 tx_param->invit_resp.rx_req_op_chan[0], tx_param->invit_resp.rx_req_op_chan[1],
                 tx_param->invit_resp.rx_req_op_chan[2]);
        }

        if (!rx_context->parsed_ie_ok)
        {
            /* Error: we should not send Success error code if we detect a problem
             * in the original frame.
             */
            IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                 "%s: Warning: there is errors in original received request frame.\n",
                 __func__);
        }
        break;
    default:
        /* Error: unsuppported frame type for this function */
        IEEE80211_DPRINTF(prot_handle->vap, IEEE80211_MSG_P2P_PROT, 
                          "%s: Error: unsuppported frame type=%d for this function\n",
                          __func__, tx_param->frtype);
        return -EINVAL;
    }
    ASSERT(rx_context != NULL);
    ASSERT(dest_freq != 0);

    /* Alloc the scatch buffer for this tx request (includes the frame buffer and additional IE) */
    param_size = sizeof(tx_event_info) + add_ie_size;
    req_param = (tx_event_info *)OS_MALLOC(prot_handle->os_handle, param_size, 0);
    if (req_param == NULL) {
        IEEE80211_DPRINTF(prot_handle->vap, IEEE80211_MSG_P2P_PROT, 
                          "%s: failed to alloc param buf, size=%d\n",
                          __func__, param_size);
        return -ENOMEM;
    }
    OS_MEMZERO(req_param, param_size);
    req_param->alloc_size = param_size;
    req_param->tx_freq = dest_freq;
    req_param->is_GO = we_are_GO;
    OS_MEMCPY(req_param->peer_addr, peer_macaddr, IEEE80211_ADDR_LEN);

    ASSERT(timeout != 0);
    req_param->timeout = timeout;

    /* Make a copy of the original parameters */
    OS_MEMCPY(&req_param->orig_req_param, tx_param, sizeof(wlan_p2p_prot_tx_act_fr_param));

    /* Fill in the assigned dialog token */
    req_param->orig_req_param.prot_handle = prot_handle;
    req_param->dialog_token = dialog_token;

    req_param->previous_frtype = prev_frtype = rx_context->previous_frtype;

    /* Copy over the information from the previous Invite_Req frame (if any) */
    if (prev_fr_invite_req) {
        ASSERT(prev_frtype == IEEE80211_P2P_PROT_FTYPE_INVIT_REQ);
        req_param->prev_fr_we_are_GO = prev_fr_we_are_GO;
        req_param->prev_fr_start_new_group = prev_fr_start_new_group;
    }

    /* Copy the previous frame channel list (if valid) */
    if (prev_fr_valid_chan_list) {
        ASSERT(prev_fr_country != NULL);
        ASSERT(prev_fr_chan_list != NULL);
        ASSERT(prev_frtype != 0);
        req_param->rx_valid_chan_list = prev_fr_valid_chan_list;
        OS_MEMCPY(req_param->rx_country, prev_fr_country, 3);
        OS_MEMCPY(&req_param->rx_chan_list, prev_fr_chan_list, sizeof(struct p2p_channels));

        /* Get the intersected channel list */
        wlan_p2p_channels_intersect(&prot_handle->chanlist, prev_fr_chan_list,
                                    &req_param->intersected_chan_list);
    }

    /* Copy the additional IE to the end of tx_event_info buffer */
    {
        // Copy the IE buffer.
        u_int8_t        *dest_buf = &(((u_int8_t *)req_param)[sizeof(tx_event_info)]);
        OS_MEMCPY(dest_buf, add_ie_buf, add_ie_size);

        req_param->in_ie_offset = sizeof(tx_event_info);
        req_param->in_ie_buf_len = add_ie_size;
    }

    sm_event.event_type = event;
    /* Note: the state handler has to free this allocated memory */
    sm_event.tx_info = req_param;

    ieee80211_sm_dispatch(prot_handle->hsm_handle, event,
                          sizeof(sm_event), &sm_event);

    return EOK;
}

/*
 * Add the P2P IE for the Probe Request frame.
 * parameter ret_ie_len return the total length of this P2P IE.
 */
static int
p2p_prot_add_probe_req_ie(
    wlan_p2p_prot_t prot_handle, 
    wlan_p2p_prot_disc_scan_param *disc_scan_param,
    u_int8_t *frm, int buf_len,
    int *ret_ie_len)
{
    p2pbuf      p2p_buf, *ies;
    int         ie_len;
    u_int8_t    *ielen_ptr;

    ies = &p2p_buf;

    p2pbuf_init(ies, frm, buf_len);
    ielen_ptr = p2pbuf_add_ie_hdr(ies);

    spin_lock(&(prot_handle->set_param_lock));

    p2pbuf_add_capability(ies, prot_handle->dev_capab, prot_handle->grp_capab);

    spin_unlock(&(prot_handle->set_param_lock));

    if (disc_scan_param->dev_filter_list_num == 1) {
        p2pbuf_add_device_id(ies, disc_scan_param->dev_filter_list[0].device_id);
    }
    else if (disc_scan_param->dev_filter_list_num > 1) {
        /* Purposely skip the Device ID in the probe request so that all P2P device
         * can respond. The alternative is to send 2 probe request frames. */
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                             "%s: Multiple device filter list=%d. Do not add device ID in probe req IE\n",
                             __func__, disc_scan_param->dev_filter_list_num);
    }

    p2pbuf_add_listen_channel(ies, prot_handle->country,
                   prot_handle->listen_chan.reg_class,
                   prot_handle->listen_chan.ieee_num);

    if (prot_handle->ext_listen_interval) {
        p2pbuf_add_ext_listen_timing(ies, prot_handle->ext_listen_period,
                          prot_handle->ext_listen_interval);
    }

    /* TODO: p2pbuf_add_operating_channel() if GO */

    p2pbuf_update_ie_hdr(ies, ielen_ptr, &ie_len);  /* last thing to do is update the IE length */

    *ret_ie_len = ie_len + 2;

    return EOK;
}

/*
 * Add the P2P IE for the Probe Response frame.
 * parameter ret_ie_len return the total length of this P2P IE.
 */
int p2p_prot_add_probe_resp_ie(
    wlan_p2p_prot_t prot_handle, 
    u_int8_t *frm, int buf_len,
    int *ret_ie_len)
{
    p2pbuf      p2p_buf, *ies;
    int         ie_len;
    u_int8_t    *ielen_ptr;

    ies = &p2p_buf;

    p2pbuf_init(ies, frm, buf_len);
    ielen_ptr = p2pbuf_add_ie_hdr(ies);

    spin_lock(&(prot_handle->set_param_lock));

    p2pbuf_add_capability(ies, prot_handle->dev_capab, prot_handle->grp_capab);

    if (prot_handle->ext_listen_interval) {
        p2pbuf_add_ext_listen_timing(ies, prot_handle->ext_listen_period,
                          prot_handle->ext_listen_interval);
    }

    /* Add the Notice of Absence (if any). Note: we will let the lower P2P device add this NOA */

    /* P2P Device Info */
    p2pbuf_add_device_info(ies, prot_handle->vap->iv_myaddr, 
                           (u_int8_t *)prot_handle->p2p_dev_name, prot_handle->p2p_dev_name_len, 
                           prot_handle->configMethods, prot_handle->primaryDeviceType,
                           prot_handle->numSecDeviceTypes, prot_handle->secondaryDeviceTypes);

    spin_unlock(&(prot_handle->set_param_lock));

    /* P2P Group Info - only for GO and not necessary for P2P Device */

    p2pbuf_update_ie_hdr(ies, ielen_ptr, &ie_len);  /* last thing to do is update the IE length */

    *ret_ie_len = ie_len + 2;

    return EOK;
}

/*
 * Create the Probe Response IE and add this IE.
 */
static int create_and_add_probe_resp_ie(wlan_p2p_prot_t prot_handle)
{
    int         retval = EOK;
    u_int8_t    *iebuf;
    int         ret_ie_len = 0;

    /* Malloc the largest IE. We assumed that the P2P IE does not exceed on IE */
    iebuf = OS_MALLOC(prot_handle->os_handle, MAX_SINGLE_IE_LEN, 0);
    if (iebuf == NULL) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
            "%s: Error: OS_MALLOC failed. Size=%d.\n", __func__, MAX_SINGLE_IE_LEN);
        return -ENOMEM;
    }

    /* Create the IE data */
    retval = p2p_prot_add_probe_resp_ie(prot_handle, iebuf, MAX_SINGLE_IE_LEN, &ret_ie_len);
    if (retval != EOK) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
            "%s: Error: p2p_prot_add_probe_resp_ie ret=0x%x.\n", __func__, retval);
        return retval;
    }

    /* Add this Application IE to the probe response frame */
    ASSERT(prot_handle->h_app_ie != NULL);
    retval = wlan_mlme_app_ie_set(prot_handle->h_app_ie, IEEE80211_FRAME_TYPE_PROBERESP,
                                  iebuf, ret_ie_len);
    if (retval != EOK) {
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
            "%s: Error: wlan_mlme_app_ie_set ret=0x%x.\n", __func__, retval);
        return retval;
    }
    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
        "%s: set App IE for Probe Resp. Len=%d, buf=%x %x %x %x %x %x %x %x\n",
        __func__, ret_ie_len, 
        iebuf[0], iebuf[1], iebuf[2], iebuf[3], 
        iebuf[4], iebuf[5], iebuf[6], iebuf[7]);

    /* Free the IE memory */
    OS_FREE(iebuf);

    return EOK;
}

/*
 * Routine to request a device discovery scan.
 */
int
wlan_p2p_prot_disc_scan_request(wlan_p2p_prot_t prot_handle, 
                                wlan_p2p_prot_disc_scan_param *disc_scan_param)
{
    disc_scan_event_info    *req_param;

    if (prot_handle->curr_disc_scan_info) {
        /* Only one device discovery scan at one time. Fail this call. */
        IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
            "%s: Error: already have discovery scan request.\n", __func__);
        return -EINPROGRESS;
    }

    req_param = prepare_disc_scan_event_info(prot_handle, disc_scan_param);
    if (req_param == NULL) {
        IEEE80211_DPRINTF(prot_handle->vap, IEEE80211_MSG_P2P_PROT, 
                          "%s: failed to prepare event info structure for discovery scan.\n",
                          __func__);
        return -ENOMEM;
    }

    dispatch_disc_scan_req(prot_handle, req_param);

    return EOK;        
}

/*
 * Routine to stop an existing device discovery scan (if any).
 */
int
wlan_p2p_prot_disc_scan_stop(wlan_p2p_prot_t prot_handle)
{
    sm_event_info           sm_event;

    OS_MEMZERO(&sm_event, sizeof(sm_event));
    sm_event.event_type = P2P_DISC_EVENT_DISC_SCAN_STOP;

    IEEE80211_DPRINTF_VB(prot_handle->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
        "%s: posting event P2P_DISC_EVENT_DISC_SCAN_STOP.\n", __func__);

    ieee80211_sm_dispatch(prot_handle->hsm_handle, P2P_DISC_EVENT_DISC_SCAN_STOP,
                          sizeof(sm_event_info), &sm_event);
    return EOK;        
}

/**
 * set simple p2p configuration parameter.
 *
 *  @param p2p_handle     : handle to the vap.
 *  @param param         : simple config paramaeter.
 *  @param val           : value of the parameter.
 *  @return 0  on success and -ve on failure.
 */
int
wlan_p2p_prot_set_param(wlan_p2p_prot_t prot_handle, wlan_p2p_prot_param param, u_int32_t val)
{
    spin_lock(&(prot_handle->set_param_lock));

    switch (param) {

    case WLAN_P2P_PROT_DEVICE_CAPABILITY:
        prot_handle->dev_capab = val;
        break;

#if 0
    case WLAN_P2P_PROT_SUPPORT_SERVICE_DISCOVERY:
        if (val)
            prot_handle->dev_capab |= P2P_DEV_CAPAB_SERVICE_DISCOVERY;
        else 
            prot_handle->dev_capab &= ~P2P_DEV_CAPAB_SERVICE_DISCOVERY;
        break;

    case WLAN_P2P_PROT_SUPPORT_CLIENT_DISCOVERABILITY:
        if (val)
            prot_handle->dev_capab |= P2P_DEV_CAPAB_CLIENT_DISCOVERABILITY;
        else 
            prot_handle->dev_capab &= ~P2P_DEV_CAPAB_CLIENT_DISCOVERABILITY;
        break;

    case WLAN_P2P_PROT_SUPPORT_CONCURRENT_OPERATION:
        if (val)
            prot_handle->dev_capab |= P2P_DEV_CAPAB_CONCURRENT_OPER;
        else 
            prot_handle->dev_capab &= ~P2P_DEV_CAPAB_CONCURRENT_OPER;
        break;

    case WLAN_P2P_PROT_INFRASTRUCTURE_MANAGED:
        if (val)
            prot_handle->dev_capab |= P2P_DEV_CAPAB_INFRA_MANAGED;
        else 
            prot_handle->dev_capab &= ~P2P_DEV_CAPAB_INFRA_MANAGED;
        break;

    case WLAN_P2P_PROT_DEVICE_LIMIT:
        if (val)
            prot_handle->dev_capab |= P2P_DEV_CAPAB_DEVICE_LIMIT;
        else 
            prot_handle->dev_capab &= ~P2P_DEV_CAPAB_DEVICE_LIMIT;
        break;

    case WLAN_P2P_PROT_SUPPORT_INVITATION_PROCEDURE:
        if (val)
            prot_handle->dev_capab |= P2P_DEV_CAPAB_INVITATION_PROCEDURE;
        else 
            prot_handle->dev_capab &= ~P2P_DEV_CAPAB_INVITATION_PROCEDURE;
        break;
#endif

    case WLAN_P2P_PROT_WPS_VERSION_ENABLED:
        prot_handle->wpsVersionEnabled = (u_int8_t)val;
        break;

    case WLAN_P2P_PROT_GROUP_CAPABILITY:
        prot_handle->grp_capab = val;
        break;

#if 0
    case WLAN_P2P_PROT_GROUP_PERSISTENT:
        if (val)
            prot_handle->grp_capab |= P2P_GROUP_CAPAB_PERSISTENT_GROUP;
        else 
            prot_handle->grp_capab &= ~P2P_GROUP_CAPAB_PERSISTENT_GROUP;
        break;

    case WLAN_P2P_PROT_GROUP_LIMIT:
        if (val)
            prot_handle->grp_capab |= P2P_GROUP_CAPAB_GROUP_LIMIT;
        else 
            prot_handle->grp_capab &= ~P2P_GROUP_CAPAB_GROUP_LIMIT;
        break;

    case WLAN_P2P_PROT_GROUP_INTRA_BSS:
        if (val)
            prot_handle->grp_capab |= P2P_GROUP_CAPAB_INTRA_BSS_DIST;
        else 
            prot_handle->grp_capab &= ~P2P_GROUP_CAPAB_INTRA_BSS_DIST;
        break;

    case WLAN_P2P_PROT_GROUP_CROSS_CONNECTION:
        if (val)
            prot_handle->grp_capab |= P2P_GROUP_CAPAB_CROSS_CONN;
        else 
            prot_handle->grp_capab &= ~P2P_GROUP_CAPAB_CROSS_CONN;
        break;

    case WLAN_P2P_PROT_GROUP_PERSISTENT_RECONNECT:
        if (val)
            prot_handle->grp_capab |= P2P_GROUP_CAPAB_PERSISTENT_RECONN;
        else 
            prot_handle->grp_capab &= ~P2P_GROUP_CAPAB_PERSISTENT_RECONN;
        break;

    case WLAN_P2P_PROT_GROUP_FORMATION:
        if (val)
            prot_handle->grp_capab |= P2P_GROUP_CAPAB_GROUP_FORMATION;
        else 
            prot_handle->grp_capab &= ~P2P_GROUP_CAPAB_GROUP_FORMATION;
        break;
#endif

    case WLAN_P2P_PROT_MAX_GROUP_LIMIT:
        prot_handle->maxGroupLimit = (u_int8_t)val;
        break;

    case WLAN_P2P_PROT_DEVICE_CONFIG_METHODS:
        prot_handle->configMethods = (u_int16_t)val;
        break;

    case WLAN_P2P_PROT_LISTEN_ST_DISC:
        prot_handle->listen_state_disc = (wlan_p2p_prot_listen_state_disc)val;

        spin_unlock(&(prot_handle->set_param_lock));

        /* Post a message to the state machine to let it know that this parameter has changed */
        ieee80211_sm_dispatch(prot_handle->hsm_handle, P2P_DISC_EVENT_LISTEN_ST_DISC_REQ,
                              0, NULL);

        spin_lock(&(prot_handle->set_param_lock));
        break;

    default:
        break;
    }

    spin_unlock(&(prot_handle->set_param_lock));

    return 0;
}

/**
 * set simple p2p configuration parameter.
 *
 *  @param p2p_handle     : handle to the vap.
 *  @param param         : simple config paramaeter.
 *  @param val           : value of the parameter.
 *  @return 0  on success and -ve on failure.
 */
int
wlan_p2p_prot_set_param_array(wlan_p2p_prot_t prot_handle, wlan_p2p_prot_param param, u_int8_t *byte_arr, u_int32_t len)
{
    int     retval = EOK;

    spin_lock(&(prot_handle->set_param_lock));

    switch (param) {

    case WLAN_P2P_PROT_DEVICE_NAME:
        if (byte_arr) {
            OS_MEMCPY(prot_handle->p2p_dev_name, (const char *)byte_arr, len);
            /* ensure that p2p_dev_name is null-terminated */
            prot_handle->p2p_dev_name[len] = '\0';
            prot_handle->p2p_dev_name_len = len;
        }
        else {
            prot_handle->p2p_dev_name_len = 0;
        }
        break;

    case WLAN_P2P_PROT_PRIMARY_DEVICE_TYPE:
        if (len != IEEE80211_P2P_DEVICE_TYPE_LEN) {
            IEEE80211_DPRINTF(prot_handle->vap, IEEE80211_MSG_P2P_PROT, 
                              "%s: Error: set WLAN_P2P_PROT_PRIMARY_DEVICE_TYPE: bad len=%d, exp=%d\n",
                              __func__, len, IEEE80211_P2P_DEVICE_TYPE_LEN);
            return -EINVAL;
        }

        /* Also let the P2P device knows about the primary device type */
        retval = wlan_p2p_set_primary_dev_type(prot_handle->p2p_device_handle, byte_arr);
        if (retval != EOK) {
            IEEE80211_DPRINTF(prot_handle->vap, IEEE80211_MSG_P2P_PROT, 
                              "%s: Error: wlan_p2p_set_primary_dev_type failed, ret=0x%x\n",
                              __func__, retval);
            return -EINVAL;
        }

        OS_MEMCPY(&prot_handle->primaryDeviceType, byte_arr, len);

        break;

    case WLAN_P2P_PROT_SECONDARY_DEVICE_TYPES:
    {
        int     num_dev_types;

        num_dev_types = len/IEEE80211_P2P_DEVICE_TYPE_LEN;

        if (len != (num_dev_types * IEEE80211_P2P_DEVICE_TYPE_LEN)) {
            IEEE80211_DPRINTF(prot_handle->vap, IEEE80211_MSG_P2P_PROT, 
                              "%s: Error: set WLAN_P2P_PROT_SECONDARY_DEVICE_TYPES: bad len=%d, exp=%d\n",
                              __func__, len, (num_dev_types * IEEE80211_P2P_DEVICE_TYPE_LEN));
            return -EINVAL;
        }

        /* Also let the P2P device knows about the secondary device type */
        retval = wlan_p2p_set_secondary_dev_type(prot_handle->p2p_device_handle, num_dev_types, byte_arr);
        if (retval != EOK) {
            IEEE80211_DPRINTF(prot_handle->vap, IEEE80211_MSG_P2P_PROT, 
                              "%s: Error: wlan_p2p_set_secondary_dev_type failed, ret=0x%x\n",
                              __func__, retval);
            return -EINVAL;
        }

        if (prot_handle->secondaryDeviceTypes) {
            /* Free the old list */
            OS_FREE(prot_handle->secondaryDeviceTypes);
        }
        prot_handle->secondaryDeviceTypes = byte_arr;
        prot_handle->numSecDeviceTypes = num_dev_types;
        break;
    }

    default:
        break;
    }

    spin_unlock(&(prot_handle->set_param_lock));

    return 0;
}

/*
 * Extract the P2P Device address from this beacon or probe response frame.
 * Return true if the p2p device address is found and copied into p2p_dev_addr[]
 */
bool
wlan_p2p_prot_get_dev_addr_from_ie(struct ieee80211vap  *vap,
                           const u8             *ie_data,
                           size_t               ie_len,
                           int                  subtype, 
                           osdev_t              st_osdev,
                           u_int8_t             *p2p_dev_addr)
{
    struct p2p_parsed_ie    pie;
    bool                    ret_status = false;

    OS_MEMZERO(&pie, sizeof(pie));

    if (ieee80211_p2p_parse_ies(st_osdev, vap, ie_data, ie_len, &pie) != 0) {
        /* Some error in parsing */
        IEEE80211_DPRINTF_VB(vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                             "%s: Error in parsing IE. SubType=0x%x\n",
                             __func__, subtype);
        return false;
    }

    do {
        /* We want to extract the P2P device address */
        if (subtype == IEEE80211_FC0_SUBTYPE_BEACON) {
            /* The P2P Device is in the P2P Device ID attribute of beacon frame */

            if (pie.device_id == NULL) {
                /* Error: beacon must have Device ID.*/
                IEEE80211_DPRINTF_VB(vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                                     "%s: Error, beacon missing Device ID\n",
                                     __func__);
                break;
            }

            IEEE80211_DPRINTF_VB(vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                                 "%s: Rx beacon: found P2P device addr= "MACSTR"\n",
                                 __func__, MAC2STR(pie.device_id));

            /* Copy over the device ID as P2P Device Address */
            IEEE80211_ADDR_COPY(p2p_dev_addr, pie.device_id);
            ret_status = true;

            break;
        }
        else if (subtype == IEEE80211_FC0_SUBTYPE_PROBE_RESP) {
            /* The P2P Device is in the P2P Device Info attribute of probe response frame */

            if (pie.p2p_device_addr == NULL) {
                /* Error: beacon must have Device Info.*/
                IEEE80211_DPRINTF_VB(vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                                     "%s: Error, Probe Resp missing Device Info.\n",
                                     __func__);
                break;
            }

            IEEE80211_DPRINTF_VB(vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                                 "%s: Rx Probe Resp, found P2P device addr= "MACSTR"\n",
                                 __func__, MAC2STR(pie.p2p_device_addr));

            /* Copy over the device ID as P2P Device Address */
            IEEE80211_ADDR_COPY(p2p_dev_addr, pie.p2p_device_addr);
            ret_status = true;

            break;
        }

    } while ( false );

    ieee80211_p2p_parse_free(&pie);

    return ret_status;
}

/* Set application defined IEs for Probe Request frame */
static int
p2p_prot_set_probe_req_appie(
    wlan_p2p_prot_t prot_handle,
    u_int8_t *buf,
    u_int16_t buflen)
{
    int         retval = EOK;
    u_int8_t    *new_appie = NULL;

    if (buflen) {
        new_appie = (u_int8_t *)OS_MALLOC(prot_handle->os_handle, buflen, 0);
        if (new_appie == NULL) {
            IEEE80211_DPRINTF(prot_handle->vap, IEEE80211_MSG_P2P_PROT, 
                              "%s: Error: failed to alloc buf for probe req IE, size=%d\n",
                              __func__, buflen);
            retval = -ENOMEM;
        }
        else {
             OS_MEMCPY(new_appie, buf, buflen);
        }
    }

    if (retval == EOK) {
        sm_event_info                   new_event;
    
        OS_MEMZERO(&new_event, sizeof(sm_event_info));
        new_event.event_type = P2P_DISC_EVENT_APP_IE_UPDATE;

        new_event.set_app_ie.probe_req_add_ie = new_appie;
        new_event.set_app_ie.probe_req_add_ie_len = buflen;
    
        /* Post a message to the state machine to let it know that this parameter has changed */
        ieee80211_sm_dispatch(prot_handle->hsm_handle, P2P_DISC_EVENT_APP_IE_UPDATE,
                              sizeof(sm_event_info), &new_event);
    }

    return retval;
}

/* Set application defined IEs for various frames */
int
wlan_p2p_prot_set_appie(
    wlan_p2p_prot_t prot_handle,
    ieee80211_frame_type ftype,
    u_int8_t *buf,
    u_int16_t buflen)
{
    int     retval;

    switch(ftype)
    {
    case IEEE80211_FRAME_TYPE_BEACON:
        retval = -EINVAL;
        break;
    case IEEE80211_FRAME_TYPE_PROBERESP:
        retval = wlan_mlme_set_appie(prot_handle->vap, ftype, buf, buflen);
        break;
    case IEEE80211_FRAME_TYPE_PROBEREQ:
        /* Additional IEs for Probe Request needs special handling since
         * during discovery scan, the upper stack may overwrite it. */
        retval = p2p_prot_set_probe_req_appie(prot_handle, buf, buflen);
        break;
    default:
        ASSERT(false);
        retval = -EINVAL;
        break;
    }
    return retval;
}

/********************** End of routines that called from external *************************/

#endif //UMAC_SUPPORT_P2P && UMAC_SUPPORT_P2P_PROT

