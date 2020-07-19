/*
* Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 *  Copyright (c) 2008 Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */

/*
 * public API for VAP object.
 */
#ifndef _IEEE80211_VAP_H_
#define _IEEE80211_VAP_H_

#include <_ieee80211.h>
#include <ieee80211.h>
#include <ieee80211_api.h>
#include <ieee80211_options.h>

struct ieee80211vap;
typedef struct ieee80211vap *ieee80211_vap_t;

#include "ieee80211_resmgr.h"

typedef enum {
IEEE80211_VAP_UP,            /* vap is up and fully configured */ 
IEEE80211_VAP_DOWN,          /* vap is disconnected  */ 
IEEE80211_VAP_ACTIVE,        /* vap is in active state */
IEEE80211_VAP_NETWORK_SLEEP, /* vap in network sleep and has limited activity */
IEEE80211_VAP_FULL_SLEEP,    /* vap is in full sleep and is not active */
IEEE80211_VAP_PAUSED,        /* vap entered paused state in response to pause request */   
IEEE80211_VAP_PAUSE_FAIL,    /* vap pause failed because of failure to send out ps frame */   
IEEE80211_VAP_UNPAUSED,      /* vap entered unpaused state in response to unpause request */     
IEEE80211_VAP_CSA_COMPLETE,  /* CSA is complete in response to CSA request */
IEEE80211_VAP_STANDBY,       /* vap is being temporarily stopped */
IEEE80211_VAP_RESUMED,       /* vap is re-started after being in standby mode */
IEEE80211_VAP_STOPPING,      /* vap is being stopped  */
IEEE80211_VAP_AUTH_COMPLETE, /* vap upper layer auth is complete */
} ieee80211_vap_event_type;

 
typedef struct _ieee80211_vap_event {
    ieee80211_vap_event_type         type;
} ieee80211_vap_event;

typedef void (*ieee80211_vap_event_handler) (ieee80211_vap_t, ieee80211_vap_event *event, void *arg);

/*
 * function to deliver vap events.
 * ideally should be in a vap private header.
 */
void ieee80211_vap_deliver_event(struct ieee80211vap *vap, ieee80211_vap_event *event);

typedef struct _ieee80211_vap_activity {
    u_int32_t data_q_len;    /* number of frames queueed up currently due to pause operation */
    u_int32_t data_q_bytes;  /* number of bytes of queued data currently due to pause operation */
    u_int32_t msec_from_last_data;  /* number msec passes since a last data sent (or) received */
    u_int32_t tx_data_frame_len;      /* total data in bytes transmitted on the vap  */
    u_int32_t rx_data_frame_len;      /* total data in bytes received on the vap  */
    u_int32_t tx_data_frame_count;    /* total number of data frmes transmitted on the vap  */
    u_int32_t rx_data_frame_count;    /* total number of data frmes received on the vap  */
} ieee80211_vap_activity;

typedef struct _ieee80211_vap_tsf_offset {
    bool        offset_negative;
    u_int64_t   offset;
} ieee80211_vap_tsf_offset;

/**
* @ get opmode
* ARGS:
*  vap    : handle to vap.
* RETURNS: returns opmode of the vap.
*/
enum ieee80211_opmode ieee80211_vap_get_opmode(ieee80211_vap_t vap);

const char* ieee80211_opmode2string( enum ieee80211_opmode opmode);

/**
* @ get resource manager private pointer in vap. 
* ARGS:
*  vap      : handle to vap.
*/  
ieee80211_resmgr_vap_priv_t ieee80211vap_get_resmgr(ieee80211_vap_t vap);

/**
* @ set resource manager private pointer in vap. 
* ARGS:
*  vap          : handle to vap.
*  resmgr_priv  : handle t resource manager private data.
*/  
void ieee80211vap_set_resmgr(ieee80211_vap_t vap, ieee80211_resmgr_vap_priv_t resmgr_priv);

/**
 * @register a vap event handler.
 * ARGS :
 *  ieee80211_vap_event_handler : vap event handler
 *  arg                         : argument passed back via the evnt handler
 * RETURNS:
 *  on success returns 0.
 *  on failure returns a negative value.
 * allows more than one event handler to be registered.
 */
int ieee80211_vap_register_event_handler(ieee80211_vap_t,ieee80211_vap_event_handler evhandler, void *arg);

/**
 * @unregister a vap event handler.
 * ARGS :
 *  ieee80211_vap_event_handler : vap event handler
 *  arg                         : argument passed back via the evnt handler
 * RETURNS:
 *  on success returns 0.
 *  on failure returns a negative value.
 */
int ieee80211_vap_unregister_event_handler(ieee80211_vap_t,ieee80211_vap_event_handler evhandler, void *arg);


/**
 * @set vap beacon interval.
 * ARGS :
 *  ieee80211_vap_event_handler : vap event handler
 *  intval                      : beacon interval. 
 * RETURNS:
 *  on success returns 0.
 */
int ieee80211_vap_set_beacon_interval(ieee80211_vap_t vap, u_int16_t intval);

/**
 * @get vap beacon interval.
 * ARGS :
 *  ieee80211_vap_event_handler : vap event handler
 * RETURNS:
 *   returns beacon interval.
 */
u_int16_t ieee80211_vap_get_beacon_interval(ieee80211_vap_t vap);

/*
 * @find if any VAP is in running state.
 * ARGS :
 *  struct ieee80211com *ic: global device handler.
 *
 * RETURNS:
 *  Return true if any VAP is in RUNNING state
 */
int
ieee80211_vap_is_any_running(struct ieee80211com *ic);

/*
 * @find number of AP VAP(s) in running state.
 * ARGS :
 *  struct ieee80211com *ic: global device handler.
 *
 * RETURNS:
 *  Return number of AP VAP(s) in RUNNING state
 */
int
ieee80211_num_apvap_running(struct ieee80211com *ic);

/*
 * @ check if vap is in running state
 * ARGS:
 *  vap    : handle to vap.
 * RETURNS:
 *  TRUE if current state of the vap is IEE80211_S_RUN.
 *  FALSE otherwise.
 */
bool ieee80211_is_vap_state_running(ieee80211_vap_t vap);

/*
 * @find maximum channel width allowed for this VAP.
 * ARGS :
 *  struct ieee80211vap *vap: handle to vap.
 *
 * RETURNS:
 *  Return ieee80211_cwm_width
 */
enum ieee80211_cwm_width ieee80211_get_vap_max_chwidth (struct ieee80211vap *vap);

/**
 * ieee80211_is_vap_state_stopping() - function to check vap stopping state
 * @vap: ieee80211vap object
 *
 * API, function to check if vap state machine is in stopping state
 *
 * Return: true if vap state machine is in stopping state,
 *         false otherwise
 */
bool ieee80211_is_vap_state_stopping (struct ieee80211vap *vap);

/**
 * ieee80211_vap_is_connected() - function to check vap connected state
 * @vap: ieee80211vap object
 *
 * API, function to check if STA vap connection sm is in connected state
 *
 * Return: true if vap connection sm is in connected state
 *         false otherwise
 */
bool ieee80211_vap_is_connected(struct ieee80211vap *vap);

/**
 * ieee80211_vap_is_connecting() - function to check vap connecting state
 * @vap: ieee80211vap object
 *
 * API, function to check if STA vap connection sm is in connecting state
 *
 * Return: true if vap connection sm is in connecting state
 *         false otherwise
 */
bool ieee80211_vap_is_connecting(struct ieee80211vap *vap);
bool wlan_vap_delete_in_progress(struct ieee80211vap *vap);

#endif
