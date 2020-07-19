/*
 * Copyright (c) 2011-2016, 2017-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 */

/*
 * Copyright (c) 2008 Atheros Communications Inc.
 * All Rights Reserved.
 *
 * 2011-2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include "ieee80211_var.h"
#include "ieee80211_btamp_api.h"
#include "ieee80211_sm.h"
#include "ieee80211_btamp_conn_private.h"

#if UMAC_SUPPORT_BTAMP
/*
 * default max assoc and auth attemtps.
 */
#define MAX_ASSOC_ATTEMPTS    3
#define MAX_AUTH_ATTEMPTS     3
#define MAX_MGMT_TIME         9000   /* msec */
#define AUTH_RETRY_TIME       30
#define ASSOC_RETRY_TIME      30
#define DISASSOC_WAIT_TIME    100    /* msec */ 
#define MAX_QUEUED_EVENTS     16

static OS_TIMER_FUNC(btamp_conn_sm_timer_handler);

struct _wlan_btamp_conn_sm {
    osdev_t                             os_handle;
    wlan_if_t                           vap_handle;
    ieee80211_hsm_t                     hsm_handle; 
    wlan_btamp_conn_sm_event_handler    sm_evhandler;
    os_if_t                             osif;
    u_int8_t                            peer[IEEE80211_ADDR_LEN];
    os_timer_t                          sm_timer;           /* generic timer */
    u_int8_t                            max_assoc_attempts; /* maximum assoc attempts */
    u_int8_t                            cur_assoc_attempts; /* current assoc attempt */
    u_int8_t                            max_auth_attempts;  /* maximum auth attempts */
    u_int8_t                            cur_auth_attempts;  /* current auth attempt */
    u_int32_t                           max_mgmt_time;      /* maximum time to wait for response */
    u_int8_t                            last_failure;
    u_int8_t                            last_reason;
    u_int32_t                           initiating:1,
                                        is_stop_requested:1,
                                        is_running:1,
                                        is_join:1;
};

static const char*    btamp_event_name[] = {
    /* IEEE80211_BTAMP_CONN_EVENT_CONNECT_REQUEST    */ "CONNECT_REQUEST",
    /* IEEE80211_BTAMP_CONN_EVENT_DISCONNECT_REQUEST */ "DISCONNECT_REQUEST",
    /* IEEE80211_BTAMP_CONN_EVENT_JOIN_COMPLETE      */ "JOIN_COMPLETE",
    /* IEEE80211_BTAMP_CONN_EVENT_AUTH_SUCCESS       */ "AUTH_SUCCESS",
    /* IEEE80211_BTAMP_CONN_EVENT_AUTH_FAIL          */ "AUTH_FAIL",
    /* IEEE80211_BTAMP_CONN_EVENT_ASSOC_SUCCESS      */ "ASSOC_SUCCESS",
    /* IEEE80211_BTAMP_CONN_EVENT_ASSOC_FAIL         */ "ASSOC_FAIL",
    /* IEEE80211_BTAMP_CONN_EVENT_TIMEOUT            */ "TIMEOUT",
    /* IEEE80211_BTAMP_CONN_EVENT_DEAUTH             */ "DEAUTH",
    /* IEEE80211_BTAMP_CONN_EVENT_DISASSOC           */ "DISASSOC",
    /* IEEE80211_BTAMP_CONN_EVENT_KEYPLUMB           */ "KEYPLUMB",
};
    
static void ieee80211_btamp_conn_sm_debug_print (void *ctx,const char *fmt,...) 
{
    wlan_btamp_conn_sm_t    sm = (wlan_btamp_conn_sm_t) ctx;
    char                    tmp_buf[256];
    va_list                 ap;
     
    va_start(ap, fmt);
    vsnprintf (tmp_buf,256, fmt, ap);
    va_end(ap);
    //IEEE80211_DPRINTF(sm->vap_handle, IEEE80211_MSG_STATE, "%s(%s)", tmp_buf, ether_sprintf(sm->peer));
    printk("%s(%s)", tmp_buf, ether_sprintf(sm->peer));
}

/*
 * 802.11 station association state machine implementation.
 */
static void ieee80211_send_event(wlan_btamp_conn_sm_t sm, wlan_btamp_conn_sm_event_type type,
                                 wlan_btamp_conn_sm_event_reason reason)
{
    wlan_btamp_conn_sm_event    sm_event;
    
    sm_event.event = type;
    sm_event.reason = reason;
    (* sm->sm_evhandler) (sm, sm->peer, sm->osif, &sm_event);
}

/* 
 * different state related functions.
 */


/*
 * BEACONING 
 */
static void ieee80211_btamp_conn_state_beaconing_entry(void *ctx) 
{
    wlan_btamp_conn_sm_t    sm = (wlan_btamp_conn_sm_t) ctx;

    if (sm->is_join || sm->cur_auth_attempts) {
        sm->is_join = 0;
        sm->cur_auth_attempts = 0; 
        sm->cur_assoc_attempts = 0; 
	    ieee80211_send_event(sm, sm->last_failure, sm->last_reason);
        sm->is_running = 0;
    }
}

static bool ieee80211_btamp_conn_state_beaconing_event(void *ctx, u_int16_t event, u_int16_t event_data_len, void *event_data) 
{
    wlan_btamp_conn_sm_t    sm = (wlan_btamp_conn_sm_t) ctx;
    bool                    retVal = true;

    switch(event) {
    case IEEE80211_BTAMP_CONN_EVENT_CONNECT_REQUEST:
        if (sm->initiating) {
            ieee80211_sm_transition_to(sm->hsm_handle, IEEE80211_BTAMP_CONN_STATE_JOIN); 
        } else {
            ieee80211_sm_transition_to(sm->hsm_handle, IEEE80211_BTAMP_CONN_STATE_AUTH); 
        }
        break;

    case IEEE80211_BTAMP_CONN_EVENT_DISCONNECT_REQUEST:
        break;

    default:
        retVal = false;
        break;
    }
    return retVal;
}

/*
 * JOIN 
 */
static void ieee80211_btamp_conn_state_join_entry(void *ctx) 
{
    wlan_btamp_conn_sm_t    sm = (wlan_btamp_conn_sm_t) ctx;
    struct ieee80211_node   *ni = NULL;
    struct ieee80211vap     *vap = sm->vap_handle; 
    
    sm->is_join = 1;
    ni = ieee80211_vap_find_node(vap, sm->peer);

    OS_SET_TIMER(&sm->sm_timer, sm->max_mgmt_time);

    if (ni) {
        ieee80211_send_probereq(ni, vap->iv_myaddr, sm->peer, sm->peer,
                                ni->ni_essid, ni->ni_esslen,
                                NULL, 0);
        ieee80211_free_node(ni);
        IEEE80211_DPRINTF(sm->vap_handle,IEEE80211_MSG_STATE,"%s: probe request",
                          __func__);
    }
}

static void ieee80211_btamp_conn_state_join_exit(void *ctx) 
{
    wlan_btamp_conn_sm_t    sm = (wlan_btamp_conn_sm_t) ctx;
    
    wlan_mlme_cancel(sm->vap_handle); /* cancel any pending mlme auth req*/
    OS_CANCEL_TIMER(&sm->sm_timer);
}

static bool ieee80211_btamp_conn_state_join_event(void *ctx, u_int16_t event, u_int16_t event_data_len, void *event_data) 
{
    wlan_btamp_conn_sm_t    sm = (wlan_btamp_conn_sm_t) ctx;
    bool                    retVal = true;

    switch(event) {

    case IEEE80211_BTAMP_CONN_EVENT_JOIN_COMPLETE:
        ieee80211_sm_transition_to(sm->hsm_handle, IEEE80211_BTAMP_CONN_STATE_AUTH); 
        break;

    case IEEE80211_BTAMP_CONN_EVENT_TIMEOUT:
        sm->last_failure = WLAN_BTAMP_CONN_SM_CONNECTION_FAILED;
        sm->last_reason = WLAN_BTAMP_CONN_SM_CONNECT_TIMEDOUT;
        ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_BTAMP_CONN_STATE_BEACONING); 
        break;

    case IEEE80211_BTAMP_CONN_EVENT_DISCONNECT_REQUEST:
        sm->last_failure = WLAN_BTAMP_CONN_SM_CONNECTION_DOWN;
        sm->last_reason = *((wlan_btamp_conn_sm_event_reason *)event_data);
        ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_BTAMP_CONN_STATE_BEACONING); 
        break;

    default:
        retVal = false;
        break;
    }

    return retVal;
}

/*
 * AUTH 
 */
static void ieee80211_btamp_conn_state_auth_entry(void *ctx) 
{
    wlan_btamp_conn_sm_t    sm = (wlan_btamp_conn_sm_t) ctx;
    
    ++sm->cur_auth_attempts; 
    
    if (!sm->initiating) {
        OS_SET_TIMER(&sm->sm_timer, sm->max_mgmt_time);
        return;
    }

    if (ieee80211_mlme_auth_request_btamp(sm->vap_handle, sm->peer, AUTH_RETRY_TIME) != 0 ) {
        IEEE80211_DPRINTF(sm->vap_handle,IEEE80211_MSG_STATE,"%s: auth_request failed retrying ...\n",__func__);
        OS_SET_TIMER(&sm->sm_timer, AUTH_RETRY_TIME);
    }
}

static void ieee80211_btamp_conn_state_auth_exit(void *ctx) 
{
    wlan_btamp_conn_sm_t    sm = (wlan_btamp_conn_sm_t) ctx;
    
    wlan_mlme_cancel(sm->vap_handle); /* cancel any pending mlme auth req*/
    OS_CANCEL_TIMER(&sm->sm_timer);
}

static bool ieee80211_btamp_conn_state_auth_event(void *ctx, u_int16_t event, u_int16_t event_data_len, void *event_data) 
{
    wlan_btamp_conn_sm_t    sm = (wlan_btamp_conn_sm_t) ctx;
    bool                    retVal = true;

    switch(event) {

    case IEEE80211_BTAMP_CONN_EVENT_AUTH_SUCCESS:
        ieee80211_sm_transition_to(sm->hsm_handle, IEEE80211_BTAMP_CONN_STATE_ASSOC); 
        break;

    case IEEE80211_BTAMP_CONN_EVENT_AUTH_FAIL: 
        sm->last_failure = WLAN_BTAMP_CONN_SM_CONNECTION_FAILED;
        sm->last_reason = WLAN_BTAMP_CONN_SM_REASON_NONE;
        ieee80211_sm_transition_to(sm->hsm_handle, IEEE80211_BTAMP_CONN_STATE_BEACONING); 
        break;

    case IEEE80211_BTAMP_CONN_EVENT_TIMEOUT:
        if (!sm->initiating) {
            sm->last_failure = WLAN_BTAMP_CONN_SM_CONNECTION_FAILED;
            sm->last_reason = WLAN_BTAMP_CONN_SM_CONNECT_TIMEDOUT;
            ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_BTAMP_CONN_STATE_BEACONING); 
            break;
        }
        if (sm->cur_auth_attempts >= sm->max_auth_attempts) {
            IEEE80211_DPRINTF(sm->vap_handle,IEEE80211_MSG_STATE,"%s: max auth attempts reached \n",__func__);
            sm->last_failure = WLAN_BTAMP_CONN_SM_CONNECTION_FAILED;
            sm->last_reason = WLAN_BTAMP_CONN_SM_CONNECT_TIMEDOUT;
            ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_BTAMP_CONN_STATE_BEACONING); 
        } else {
            IEEE80211_DPRINTF(sm->vap_handle,IEEE80211_MSG_STATE,"%s: Auth timeout, retry\n",__func__);
            ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_BTAMP_CONN_STATE_AUTH); 
        }
        break;

    case IEEE80211_BTAMP_CONN_EVENT_DISCONNECT_REQUEST:
        sm->last_failure = WLAN_BTAMP_CONN_SM_CONNECTION_DOWN;
        sm->last_reason = *((wlan_btamp_conn_sm_event_reason *)event_data);
        ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_BTAMP_CONN_STATE_BEACONING); 
        break;

    case IEEE80211_BTAMP_CONN_EVENT_DEAUTH:
        sm->last_failure = WLAN_BTAMP_CONN_SM_CONNECTION_DOWN;
        sm->last_reason = WLAN_BTAMP_CONN_SM_PEER_DISCONNECTED;
        ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_BTAMP_CONN_STATE_BEACONING); 
        break;

    default:
        retVal = false;
        break;
    }

    return retVal;
}

/*
 * ASSOC
 */
static void ieee80211_btamp_conn_state_assoc_entry(void *ctx) 
{
    wlan_btamp_conn_sm_t    sm = (wlan_btamp_conn_sm_t) ctx;
    
    ++sm->cur_assoc_attempts; 

    if (!sm->initiating) {
        OS_SET_TIMER(&sm->sm_timer, sm->max_mgmt_time);
        return;
    }

    if (ieee80211_mlme_assoc_request_btamp(sm->vap_handle, sm->peer, ASSOC_RETRY_TIME) != 0 ) {
        IEEE80211_DPRINTF(sm->vap_handle,IEEE80211_MSG_STATE,"%s: assoc request failed retrying ...\n",__func__);
        OS_SET_TIMER(&sm->sm_timer, ASSOC_RETRY_TIME);
    }
}

static void ieee80211_btamp_conn_state_assoc_exit(void *ctx) 
{
    wlan_btamp_conn_sm_t    sm = (wlan_btamp_conn_sm_t) ctx;
    
    wlan_mlme_cancel(sm->vap_handle); /* cancel any pending mlme assoc req */
    OS_CANCEL_TIMER(&sm->sm_timer);
}

static bool ieee80211_btamp_conn_state_assoc_event(void *ctx, u_int16_t event, u_int16_t event_data_len, void *event_data) 
{
    wlan_btamp_conn_sm_t    sm = (wlan_btamp_conn_sm_t) ctx;
    bool                    retVal = true;

    switch(event) {
    case IEEE80211_BTAMP_CONN_EVENT_ASSOC_SUCCESS:
        ieee80211_sm_transition_to(sm->hsm_handle, IEEE80211_BTAMP_CONN_STATE_CONNECTED); 
        break;

    case IEEE80211_BTAMP_CONN_EVENT_ASSOC_FAIL:
        sm->last_failure = WLAN_BTAMP_CONN_SM_CONNECTION_FAILED;
        sm->last_reason = WLAN_BTAMP_CONN_SM_REASON_NONE;
        ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_BTAMP_CONN_STATE_BEACONING); 
        break;

    case IEEE80211_BTAMP_CONN_EVENT_TIMEOUT:
        if (!sm->initiating) {
            sm->last_failure = WLAN_BTAMP_CONN_SM_CONNECTION_FAILED;
            sm->last_reason = WLAN_BTAMP_CONN_SM_CONNECT_TIMEDOUT;
            ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_BTAMP_CONN_STATE_BEACONING); 
            break;
        }
        if (sm->cur_assoc_attempts >= sm->max_assoc_attempts) {
            IEEE80211_DPRINTF(sm->vap_handle,IEEE80211_MSG_STATE,"%s: max assoc attempts reached \n",__func__);
            sm->last_failure = WLAN_BTAMP_CONN_SM_CONNECTION_FAILED;
            sm->last_reason = WLAN_BTAMP_CONN_SM_CONNECT_TIMEDOUT;
            ieee80211_sm_transition_to(sm->hsm_handle, IEEE80211_BTAMP_CONN_STATE_BEACONING); 
        } else {
            IEEE80211_DPRINTF(sm->vap_handle,IEEE80211_MSG_STATE,"%s: Assoc timeout, retry ...\n",__func__);
            ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_BTAMP_CONN_STATE_ASSOC); 
        }
        break;

    case IEEE80211_BTAMP_CONN_EVENT_DISCONNECT_REQUEST:
        sm->last_failure = WLAN_BTAMP_CONN_SM_CONNECTION_DOWN;
        sm->last_reason = *((wlan_btamp_conn_sm_event_reason *)event_data);
        wlan_mlme_deauth_request(sm->vap_handle, sm->peer, IEEE80211_REASON_AUTH_LEAVE);
        ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_BTAMP_CONN_STATE_BEACONING); 
        break;

    case IEEE80211_BTAMP_CONN_EVENT_DEAUTH:
        sm->last_failure = WLAN_BTAMP_CONN_SM_CONNECTION_DOWN;
        sm->last_reason = WLAN_BTAMP_CONN_SM_PEER_DISCONNECTED;
        ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_BTAMP_CONN_STATE_BEACONING); 
        break;

    default:
        retVal = false;
        break;
    }

    return retVal;
}

/*
 * CONNECTED
 */
static void ieee80211_btamp_conn_state_connected_entry(void *ctx) 
{
    wlan_btamp_conn_sm_t     sm = (wlan_btamp_conn_sm_t) ctx;
    struct ieee80211vap      *vap = (struct ieee80211vap *)sm->vap_handle;
    struct ieee80211com      *ic = vap->iv_ic;
    struct ieee80211_node    *ni = NULL;

    /* Authorize peer node */
    wlan_node_authorize(sm->vap_handle, true, sm->peer);

    ni = ieee80211_find_node(&ic->ic_sta, sm->peer);
    if (ni) {

        /* Set up node tx rate */
        if (ic->ic_newassoc)
            ic->ic_newassoc(ni, TRUE);

        ieee80211_free_node(ni);
    }

    ieee80211_send_event(sm, WLAN_BTAMP_CONN_SM_CONNECTION_UP, WLAN_BTAMP_CONN_SM_REASON_NONE);
}

static void ieee80211_btamp_conn_state_connected_exit(void *ctx) 
{
    wlan_btamp_conn_sm_t    sm = (wlan_btamp_conn_sm_t) ctx;

    /* De-authorize peer node */
    wlan_node_authorize(sm->vap_handle, false, sm->peer);

   /* delete any unicast key installed */
   wlan_del_key(sm->vap_handle, IEEE80211_KEYIX_NONE, sm->peer); 
}

static bool ieee80211_btamp_conn_state_connected_event(void *ctx, u_int16_t event, u_int16_t event_data_len, void *event_data) 
{
    wlan_btamp_conn_sm_t    sm = (wlan_btamp_conn_sm_t) ctx;
    bool                    retVal = true;

    switch(event) {

    case IEEE80211_BTAMP_CONN_EVENT_KEYPLUMB:
        ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_BTAMP_CONN_STATE_SECURED); 
        break;

    case IEEE80211_BTAMP_CONN_EVENT_DEAUTH:
    case IEEE80211_BTAMP_CONN_EVENT_DISASSOC:
        sm->last_failure = WLAN_BTAMP_CONN_SM_CONNECTION_DOWN;
        sm->last_reason = WLAN_BTAMP_CONN_SM_PEER_DISCONNECTED;
        ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_BTAMP_CONN_STATE_BEACONING); 
        break;

    case IEEE80211_BTAMP_CONN_EVENT_DISCONNECT_REQUEST:
        sm->last_failure = WLAN_BTAMP_CONN_SM_CONNECTION_DOWN;
        sm->last_reason = *((wlan_btamp_conn_sm_event_reason *)event_data);
        wlan_mlme_deauth_request(sm->vap_handle, sm->peer, IEEE80211_REASON_AUTH_LEAVE);
        ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_BTAMP_CONN_STATE_BEACONING); 
        break;

    default:
        retVal = false;
        break;
    }
    return retVal;
}

/*
 * SECURED 
 */
static void ieee80211_btamp_conn_state_secured_entry(void *ctx) 
{
    wlan_btamp_conn_sm_t    sm = (wlan_btamp_conn_sm_t) ctx;

    /* Authorize peer node */
    wlan_node_authorize(sm->vap_handle, true, sm->peer);
}

static void ieee80211_btamp_conn_state_secured_exit(void *ctx) 
{
    wlan_btamp_conn_sm_t    sm = (wlan_btamp_conn_sm_t) ctx;

    /* De-authorize peer node */
    wlan_node_authorize(sm->vap_handle, false, sm->peer);

   /* delete any unicast key installed */
   wlan_del_key(sm->vap_handle, IEEE80211_KEYIX_NONE, sm->peer); 
}

static bool ieee80211_btamp_conn_state_secured_event(void *ctx, u_int16_t event, u_int16_t event_data_len, void *event_data) 
{
    wlan_btamp_conn_sm_t    sm = (wlan_btamp_conn_sm_t) ctx;
    bool                    retVal = true;

    switch(event) {
    case IEEE80211_BTAMP_CONN_EVENT_DISASSOC:
    case IEEE80211_BTAMP_CONN_EVENT_DEAUTH:
        sm->last_failure = WLAN_BTAMP_CONN_SM_CONNECTION_DOWN;
        sm->last_reason = WLAN_BTAMP_CONN_SM_PEER_DISCONNECTED;
        ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_BTAMP_CONN_STATE_BEACONING); 
        break;

    case IEEE80211_BTAMP_CONN_EVENT_DISCONNECT_REQUEST:
        sm->last_failure = WLAN_BTAMP_CONN_SM_CONNECTION_DOWN;
        sm->last_reason = *((wlan_btamp_conn_sm_event_reason *)event_data);
        wlan_mlme_deauth_request(sm->vap_handle, sm->peer, IEEE80211_REASON_AUTH_LEAVE);
        ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_BTAMP_CONN_STATE_BEACONING); 
        break;

    default:
        retVal = false;
        break;
    }

    return retVal;
}

ieee80211_state_info ieee80211_btamp_conn_sm_info[] = {
    { 
        (u_int8_t) IEEE80211_BTAMP_CONN_STATE_BEACONING, 
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        false,
        "BEACONING",
        ieee80211_btamp_conn_state_beaconing_entry,
        NULL,
        ieee80211_btamp_conn_state_beaconing_event
    },
    { 
        (u_int8_t) IEEE80211_BTAMP_CONN_STATE_JOIN, 
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        false,
        "JOIN",
        ieee80211_btamp_conn_state_join_entry,
        ieee80211_btamp_conn_state_join_exit,
        ieee80211_btamp_conn_state_join_event
    },
    { 
        (u_int8_t) IEEE80211_BTAMP_CONN_STATE_AUTH, 
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        false,
        "AUTH",
        ieee80211_btamp_conn_state_auth_entry,
        ieee80211_btamp_conn_state_auth_exit,
        ieee80211_btamp_conn_state_auth_event
    },
    { 
        (u_int8_t) IEEE80211_BTAMP_CONN_STATE_ASSOC, 
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        false,
        "ASSOC",
        ieee80211_btamp_conn_state_assoc_entry,
        ieee80211_btamp_conn_state_assoc_exit,
        ieee80211_btamp_conn_state_assoc_event
    },
    { 
        (u_int8_t) IEEE80211_BTAMP_CONN_STATE_CONNECTED, 
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        false,
        "CONNECTED",
        ieee80211_btamp_conn_state_connected_entry,
        ieee80211_btamp_conn_state_connected_exit,
        ieee80211_btamp_conn_state_connected_event
    },
    { 
        (u_int8_t) IEEE80211_BTAMP_CONN_STATE_SECURED, 
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        false,
        "SECURED",
        ieee80211_btamp_conn_state_secured_entry,
        ieee80211_btamp_conn_state_secured_exit,
        ieee80211_btamp_conn_state_secured_event
    },
};

static OS_TIMER_FUNC(btamp_conn_sm_timer_handler)
{
    wlan_btamp_conn_sm_t    sm;

    OS_GET_TIMER_ARG(sm, wlan_btamp_conn_sm_t);
    IEEE80211_DPRINTF(sm->vap_handle,IEEE80211_MSG_STATE,"%s: timed out cur state %s \n",
                      __func__, ieee80211_btamp_conn_sm_info[ieee80211_sm_get_curstate(sm->hsm_handle)].name);
    ieee80211_sm_dispatch(sm->hsm_handle, IEEE80211_BTAMP_CONN_EVENT_TIMEOUT, 0, NULL); 

}

/*
 * mlme event handlers.
 */

static wlan_btamp_conn_sm_t ieee80211_btamp_get_smhandle(wlan_btamp_conn_sm_t *smhandle,
                                                         u_int8_t *peer_addr, int cur_state)
{
    u_int8_t                phyLink;
    wlan_btamp_conn_sm_t    sm = NULL;

    for (phyLink = 0; phyLink < AMP_MAX_PHY_LINKS; phyLink++) {

        if (!smhandle[phyLink])
            continue;

        sm = smhandle[phyLink];

        if (peer_addr && IEEE80211_ADDR_EQ(sm->peer, peer_addr))
            break;

        if (!peer_addr && ieee80211_sm_get_curstate(sm->hsm_handle) == cur_state)
            break;
    }

    if (phyLink >= AMP_MAX_PHY_LINKS)
        sm = NULL;

    return sm;
}

static void sm_join_complete(os_handle_t osif, IEEE80211_STATUS status)
{
    wlan_btamp_conn_sm_t    sm;
    
    sm = ieee80211_btamp_get_smhandle((wlan_btamp_conn_sm_t *)osif, NULL,
                                      IEEE80211_BTAMP_CONN_STATE_JOIN); 
    if (!sm || sm->is_stop_requested) {
        return;
    }

    if (status == IEEE80211_STATUS_SUCCESS) {
        ieee80211_sm_dispatch(sm->hsm_handle, IEEE80211_BTAMP_CONN_EVENT_JOIN_COMPLETE,
                              0, NULL);
    }
}

static void sm_auth_complete(os_handle_t osif, IEEE80211_STATUS status)
{
    wlan_btamp_conn_sm_t    sm;
    
    sm = ieee80211_btamp_get_smhandle((wlan_btamp_conn_sm_t *)osif, NULL,
                                      IEEE80211_BTAMP_CONN_STATE_AUTH); 
    if (!sm || sm->is_stop_requested) {
        return;
    }

    if (status == IEEE80211_STATUS_SUCCESS) {
        ieee80211_sm_dispatch(sm->hsm_handle,IEEE80211_BTAMP_CONN_EVENT_AUTH_SUCCESS,0,NULL);
    } else if (status == IEEE80211_STATUS_TIMEOUT) {
        ieee80211_sm_dispatch(sm->hsm_handle,IEEE80211_BTAMP_CONN_EVENT_TIMEOUT,0,NULL);
    } else {
        ieee80211_sm_dispatch(sm->hsm_handle,IEEE80211_BTAMP_CONN_EVENT_AUTH_FAIL,0,NULL);
    }
}

static void sm_assoc_complete(os_handle_t osif, IEEE80211_STATUS status, u_int16_t aid, wbuf_t wbuf)
{
    wlan_btamp_conn_sm_t      sm;
    struct ieee80211_frame    *wh = NULL;
    
    if (wbuf)
        wh = (struct ieee80211_frame *)wbuf_header(wbuf);

    sm = ieee80211_btamp_get_smhandle((wlan_btamp_conn_sm_t *)osif,
                                      (wh?wh->i_addr2:NULL),
                                      IEEE80211_BTAMP_CONN_STATE_ASSOC); 
    if (!sm || sm->is_stop_requested) {
        return;
    }

    if (status == IEEE80211_STATUS_SUCCESS) {
        ieee80211_sm_dispatch(sm->hsm_handle,IEEE80211_BTAMP_CONN_EVENT_ASSOC_SUCCESS,0,NULL);
    } else if (status == IEEE80211_STATUS_TIMEOUT) {
        ieee80211_sm_dispatch(sm->hsm_handle,IEEE80211_BTAMP_CONN_EVENT_TIMEOUT,0,NULL);
    } else {
        ieee80211_sm_dispatch(sm->hsm_handle,IEEE80211_BTAMP_CONN_EVENT_ASSOC_FAIL,0,NULL);
    }
}

static void sm_reassoc_complete(os_handle_t osif, IEEE80211_STATUS status, u_int16_t aid, wbuf_t wbuf)
{
    wlan_btamp_conn_sm_t      sm;
    struct ieee80211_frame    *wh = NULL;
    
    if (wbuf)
        wh = (struct ieee80211_frame *)wbuf_header(wbuf);

    sm = ieee80211_btamp_get_smhandle((wlan_btamp_conn_sm_t *)osif, wh->i_addr2,
                                      IEEE80211_BTAMP_CONN_STATE_ASSOC); 
    if (!sm || sm->is_stop_requested) {
        return;
    }

    if (status == IEEE80211_STATUS_SUCCESS) {
        ieee80211_sm_dispatch(sm->hsm_handle,IEEE80211_BTAMP_CONN_EVENT_ASSOC_SUCCESS,0,NULL);
    } else {
        ieee80211_sm_dispatch(sm->hsm_handle,IEEE80211_BTAMP_CONN_EVENT_ASSOC_FAIL,0,NULL);
    }
}

static void sm_auth_indication(os_handle_t osif, u_int8_t *macaddr, u_int16_t result)
{
    wlan_btamp_conn_sm_t    sm;

    sm = ieee80211_btamp_get_smhandle((wlan_btamp_conn_sm_t *)osif, macaddr,
                                      IEEE80211_BTAMP_CONN_STATE_AUTH); 
    if (!sm || sm->is_stop_requested) {
        return;
    }

    if (result == IEEE80211_STATUS_SUCCESS) {
        ieee80211_sm_dispatch(sm->hsm_handle,IEEE80211_BTAMP_CONN_EVENT_AUTH_SUCCESS,0,NULL);
    } else {
        ieee80211_sm_dispatch(sm->hsm_handle,IEEE80211_BTAMP_CONN_EVENT_AUTH_FAIL,0,NULL);
    }
}

static void sm_assoc_indication(os_handle_t osif, u_int8_t *macaddr, u_int16_t result,
                                wbuf_t wbuf, wbuf_t resp_wbuf)
{
    wlan_btamp_conn_sm_t     sm;
    struct ieee80211vap      *vap = NULL;
    struct ieee80211_node    *ni = NULL;

    sm = ieee80211_btamp_get_smhandle((wlan_btamp_conn_sm_t *)osif, macaddr,
                                      IEEE80211_BTAMP_CONN_STATE_ASSOC); 
    if (!sm || sm->is_stop_requested) {
        return;
    }

    vap = sm->vap_handle;
    ni = ieee80211_vap_find_node(vap, sm->peer);
    if (ni) {
       ni->ni_assocstatus = result;
       ieee80211_free_node(ni);
    }

    if (result == IEEE80211_STATUS_SUCCESS) {
        ieee80211_sm_dispatch(sm->hsm_handle,IEEE80211_BTAMP_CONN_EVENT_ASSOC_SUCCESS, 0, NULL);
    } else {
        ieee80211_sm_dispatch(sm->hsm_handle,IEEE80211_BTAMP_CONN_EVENT_ASSOC_FAIL, 0, NULL);
    }
}

static void sm_deauth_indication(os_handle_t osif,u_int8_t *macaddr, u_int16_t reason)
{
    wlan_btamp_conn_sm_t    sm;

    sm = ieee80211_btamp_get_smhandle((wlan_btamp_conn_sm_t *)osif, macaddr,
                                      IEEE80211_BTAMP_CONN_STATE_CONNECTED); 
    if (!sm || sm->is_stop_requested) {
        return;
    }

    ieee80211_sm_dispatch(sm->hsm_handle, IEEE80211_BTAMP_CONN_EVENT_DEAUTH, 0, NULL);
}

static void sm_disassoc_indication(os_handle_t osif, u_int8_t *macaddr, u_int32_t reason)
{
    wlan_btamp_conn_sm_t    sm;

    sm = ieee80211_btamp_get_smhandle((wlan_btamp_conn_sm_t *)osif, macaddr,
                                      IEEE80211_BTAMP_CONN_STATE_CONNECTED); 
    if (!sm || sm->is_stop_requested) {
        return;
    }

    ieee80211_sm_dispatch(sm->hsm_handle, IEEE80211_BTAMP_CONN_EVENT_DISASSOC, 0, NULL);
}

static wlan_mlme_event_handler_table btamp_mlme_evt_handler = {
    /* MLME confirmation handler */
    sm_join_complete,           /* mlme_join_complete_infra */
    NULL,                       /* mlme_join_complete_adhoc */
    sm_auth_complete,           /* mlme_auth_complete */
    NULL,                       /* mlme_assoc_req */
    sm_assoc_complete,          /* mlme_assoc_complete */
    sm_reassoc_complete,        /* mlme_reassoc_complete */
    NULL,                       /* mlme_deauth_complete */
    NULL,                       /* mlme_disassoc_complete */
    NULL,                       /* mlme_txchanswitch_complete */
 
    /* MLME indication handler */
    sm_auth_indication,         /* mlme_auth_indication */
    sm_deauth_indication,       /* mlme_deauth_indication */
    sm_assoc_indication,        /* mlme_assoc_indication */
    NULL,                       /* mlme_reassoc_indication */
    sm_disassoc_indication,     /* mlme_disassoc_indication */
    NULL,                       /* mlme_ibss_merge_start_indication */
    NULL,                       /* mlme_ibss_merge_completion_indication */
};


wlan_btamp_conn_sm_t
wlan_btamp_conn_sm_create(wlan_if_t            vaphandle,
                          wlan_btamp_conn_sm_t *linkHandle,
                          u_int8_t             *peer_addr, 
                          bool                 initiating)
{
    wlan_btamp_conn_sm_t     sm;
    struct ieee80211vap      *vap = vaphandle;
    osdev_t                  oshandle = vap->iv_ic->ic_osdev;
    struct ieee80211_node    *ni;

    sm = (wlan_btamp_conn_sm_t) OS_MALLOC(oshandle, sizeof(struct _wlan_btamp_conn_sm), 0);
    if (!sm) {
        return NULL;
    }
    OS_MEMZERO(sm, sizeof(struct _wlan_btamp_conn_sm));

    sm->os_handle = oshandle;
    sm->vap_handle = vaphandle;
    IEEE80211_ADDR_COPY(sm->peer, peer_addr);
    sm->initiating = initiating;
    sm->hsm_handle = ieee80211_sm_create(oshandle,
                                         "btamp_conn",
                                         (void *) sm,
                                         IEEE80211_BTAMP_CONN_STATE_BEACONING,
                                         ieee80211_btamp_conn_sm_info,
                                         sizeof(ieee80211_btamp_conn_sm_info)/sizeof(ieee80211_state_info),
                                         MAX_QUEUED_EVENTS,
                                         sizeof(u_int32_t) /* event data of size int */, 
                                         MESGQ_PRIORITY_HIGH,
                                         IEEE80211_HSM_ASYNCHRONOUS, /* run the SM asynchronously */
                                         ieee80211_btamp_conn_sm_debug_print,
                                         btamp_event_name,
                                         IEEE80211_N(btamp_event_name));
    if (!sm->hsm_handle) {
        OS_FREE(sm);
        IEEE80211_DPRINTF(vaphandle, IEEE80211_MSG_STATE,
            "%s : ieee80211_sm_create failed\n", __func__);
        return NULL;
    }

    sm->max_assoc_attempts = MAX_ASSOC_ATTEMPTS;
    sm->max_auth_attempts =  MAX_AUTH_ATTEMPTS;
    sm->max_mgmt_time =  MAX_MGMT_TIME;
    OS_INIT_TIMER(oshandle, &(sm->sm_timer), btamp_conn_sm_timer_handler, (void *)sm, QDF_TIMER_TYPE_WAKE_APPS);

    /* Register MLME event handlers */
    wlan_vap_register_mlme_event_handlers(vaphandle, (os_if_t)linkHandle, &btamp_mlme_evt_handler);

    /* Create a node for the peer mac address */
    ni = ieee80211_dup_bss(vap, peer_addr);
    if (ni) {
        if (initiating) {
            IEEE80211_ADDR_COPY(ni->ni_bssid, peer_addr);

            /* Set up SSID */
            snprintf((char *)ni->ni_essid, 22, "AMP-%02x-%02x-%02x-%02x-%02x-%02x",
                     peer_addr[0], peer_addr[1], peer_addr[2],
                     peer_addr[3], peer_addr[4], peer_addr[5]);
            ni->ni_esslen = 21;
            ieee80211node_set_flag(ni, IEEE80211_NODE_ERP); // Mark the node as ERP
        }
        ieee80211_free_node(ni); /* Decrement the extra ref count */
    }

    return sm;
}

void wlan_btamp_conn_sm_delete(wlan_btamp_conn_sm_t handle)
{
    wlan_btamp_conn_sm_t     smhandle = (wlan_btamp_conn_sm_t)handle;
    struct ieee80211vap      *vap = NULL;
    struct ieee80211_node    *ni = NULL;

    if (!smhandle)
        return;

    vap = smhandle->vap_handle;

    if (smhandle->is_running) {
        IEEE80211_DPRINTF(smhandle->vap_handle, IEEE80211_MSG_STATE,
                          "%s: Deleting state machine while it is still runing \n", __func__); 
    }
    ASSERT(! smhandle->is_running);
    
    OS_CANCEL_TIMER(&(smhandle->sm_timer));
    OS_FREE_TIMER(&(smhandle->sm_timer));

    ieee80211_sm_delete(smhandle->hsm_handle);

    /* Delete node for the peer mac address */
    ni = ieee80211_vap_find_node(vap, smhandle->peer);
    if (ni) {
        IEEE80211_NODE_LEAVE(ni);
        ieee80211_free_node(ni); /* Decrement the extra ref count from find */
    }

    OS_FREE(smhandle);
}

void wlan_btamp_conn_sm_register_event_handler(wlan_btamp_conn_sm_t handle, os_if_t osif,
                                               wlan_btamp_conn_sm_event_handler sm_evhandler)
{
    wlan_btamp_conn_sm_t    smhandle = (wlan_btamp_conn_sm_t)handle;

    if (!smhandle)
        return;

    smhandle->osif = osif;
    smhandle->sm_evhandler = sm_evhandler;
}

/*
 * start the state machine and handling the events.
 */
int wlan_btamp_conn_sm_start(wlan_btamp_conn_sm_t handle)
{
    wlan_btamp_conn_sm_t    smhandle = (wlan_btamp_conn_sm_t)handle;

    if (!smhandle)
        return -EINVAL;

    if ( smhandle->is_running ) {
        IEEE80211_DPRINTF(smhandle->vap_handle, IEEE80211_MSG_STATE,
                          "%s: BTAMP Connection SM is already running\n", __func__);
        return -EINPROGRESS;
    }

    if (smhandle->is_stop_requested) {
        IEEE80211_DPRINTF(smhandle->vap_handle, IEEE80211_MSG_STATE,
                          "%s: BTAMP Connection SM is being stopped\n",__func__);
        return -EINVAL;
    }
  
    /* mark it as running */
    smhandle->is_running = 1;

    ieee80211_sm_dispatch(smhandle->hsm_handle,
                          IEEE80211_BTAMP_CONN_EVENT_CONNECT_REQUEST, 0, NULL); 
    return EOK;
}

/*
 * stop handling the events.
 */
int wlan_btamp_conn_sm_stop(wlan_btamp_conn_sm_t handle, wlan_btamp_conn_sm_event_reason reason)
{
    wlan_btamp_conn_sm_t    smhandle = (wlan_btamp_conn_sm_t)handle;

    if (!smhandle)
        return -EINVAL;

    /*
     * return an error if it is already stopped (or)
     * there is a stop request is pending.
     */
    if (!smhandle->is_running ) {
        IEEE80211_DPRINTF(smhandle->vap_handle, IEEE80211_MSG_STATE,
                          "%s: BTAMP Connection SM is already stopped\n",__func__);
        return -EALREADY;
    }
    if (smhandle->is_stop_requested) {
        IEEE80211_DPRINTF(smhandle->vap_handle, IEEE80211_MSG_STATE,
                          "%s: BTAMP Connection SM is already being stopped\n",__func__);
        return -EALREADY;
    }
    smhandle->is_stop_requested = 1;

    OS_CANCEL_TIMER(&(smhandle->sm_timer));

    ieee80211_sm_dispatch(smhandle->hsm_handle,
                          IEEE80211_BTAMP_CONN_EVENT_DISCONNECT_REQUEST,
                          sizeof(u_int32_t),
                          &reason);
                          
    return EOK;
}

u_int8_t *wlan_btamp_conn_sm_get_peer(wlan_btamp_conn_sm_t handle)
{
    wlan_btamp_conn_sm_t    smhandle = (wlan_btamp_conn_sm_t)handle;
    
    return (smhandle->peer);
}

bool wlan_btamp_conn_sm_is_idle(wlan_btamp_conn_sm_t handle)
{
    wlan_btamp_conn_sm_t smhandle = (wlan_btamp_conn_sm_t)handle;
    return ((smhandle->is_running) ? false : true);
}

#endif
