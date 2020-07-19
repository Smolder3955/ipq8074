/*
* Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 * Copyright (c) 2009, Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */

#ifndef _IEEE80211_BTAMP_API_H_
#define _IEEE80211_BTAMP_API_H_

#include "ieee80211_api.h"

/* BTAMP SM Specific APIs */

struct _wlan_btamp_conn_sm;
typedef struct _wlan_btamp_conn_sm *wlan_btamp_conn_sm_t;

#define AMP_MAX_PHY_LINKS    7

typedef enum _wlan_btamp_conn_sm_event_type {
     WLAN_BTAMP_CONN_SM_CONNECTION_UP = 1,  /* the connection/link is up and running */ 
     WLAN_BTAMP_CONN_SM_CONNECTION_FAILED,  /* the connection request failed */ 
     WLAN_BTAMP_CONN_SM_CONNECTION_DOWN,    /* the connection/link is down */ 
} wlan_btamp_conn_sm_event_type;
					
typedef enum _wlan_btamp_conn_sm_event_reason {
     WLAN_BTAMP_CONN_SM_REASON_NONE,             /* No special reason */ 
     WLAN_BTAMP_CONN_SM_CONNECT_TIMEDOUT,        /* connection timed out */ 
     WLAN_BTAMP_CONN_SM_DISCONNECT_REQUEST,      /* disconnection requested */ 
     WLAN_BTAMP_CONN_SM_PEER_DISCONNECTED,       /* peer node disconnected */ 
     WLAN_BTAMP_CONN_SM_CHANNEL_CHANGE,          /* Hw channel changed */ 
     WLAN_BTAMP_CONN_SM_RESET_START,             /* Port reset due to internal reasons - Ndis-Reset/RFKill/Adhoc-Start */ 
     WLAN_BTAMP_CONN_SM_POWER_STATE_CHANGE,      /* Power State Change - S3/S4 */ 
} wlan_btamp_conn_sm_event_reason;

typedef struct _wlan_btamp_conn_sm_event {
     wlan_btamp_conn_sm_event_type    event; 
     wlan_btamp_conn_sm_event_reason  reason;  
} wlan_btamp_conn_sm_event;

typedef void (*wlan_btamp_conn_sm_event_handler) (wlan_btamp_conn_sm_t smhandle, u_int8_t *peer,
                                                  os_if_t vaphandle, wlan_btamp_conn_sm_event *smevent);   
/**
 * @creates a handle for btamp connection state machine for a given peer.
 * ARGS :
 *  wlan_if_t    : handle to the vap .
 *  linkHandle   : pointer to array of physical link state machines.
 *  peer         : btamp peer node.
 *  initiate     : Boolean indicating whether the local node should start auth.
 * RETURNS:
 *  on success return an opaque handle to newly created sm.
 *  on failure returns NULL.
 */
wlan_btamp_conn_sm_t wlan_btamp_conn_sm_create(wlan_if_t vaphandle,
                                               wlan_btamp_conn_sm_t *linkHandle,
                                               u_int8_t *peer, bool initiate);

/**
 * @delete  a handle for btamp connection state machine.
 * ARGS :
 *  wlan_btamp_conn_sm_t    : handle to the state machine .
 * RETURNS:
 */
void  wlan_btamp_conn_sm_delete(wlan_btamp_conn_sm_t smhandle);

/** 
 * @register event handler table with sm.
 * ARGS :
 *  wlan_btamp_conn_sm_t     : handle to the state machine .
 *  os_if_t                  : handle to os network interface.
 *  wlan_event_handler_table : table of handlers to receive 802.11 state machine events.
 *                             not set the MLME handlers part of the event_handler_table. 
 * RETURNS:
 *  on success return 0.
 *  on failure returns -1.
 */
void wlan_btamp_conn_sm_register_event_handler(wlan_btamp_conn_sm_t smhandle, os_if_t osif,
					                           wlan_btamp_conn_sm_event_handler sm_evhandler); 

/**
 * @start a state machine.
 * ARGS :
 *  wlan_btamp_conn_sm_t : handle to the state machine.
 *
 * RETURNS:
 *  on success returns 0.
 *  on failure returns a negative value.
 */
int wlan_btamp_conn_sm_start(wlan_btamp_conn_sm_t smhandle);

/**
 * @stop a state machine.
 * ARGS :
 *  wlan_btamp_conn_sm_t : handle to the state machine .
 *  wlan_btamp_conn_sm_event_reason : reason for disconnection
 *
 * RETURNS:
 *  on success returns 0.
 *  on failure returns a negative value.
 */
int wlan_btamp_conn_sm_stop(wlan_btamp_conn_sm_t smhandle, wlan_btamp_conn_sm_event_reason reason);

u_int8_t *wlan_btamp_conn_sm_get_peer(wlan_btamp_conn_sm_t smhandle);

bool wlan_btamp_conn_sm_is_idle(wlan_btamp_conn_sm_t smhandle);

#endif
