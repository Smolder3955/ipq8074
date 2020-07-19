/*
* Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 * Copyright (c) 2010, Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */

/*
 * general HSM (hierarchical state machine) framework.
 */
#ifndef _IEEE80211_HSM_H_
#define _IEEE80211_HSM_H_

#include "osdep.h"

struct _ieee80211_hsm;

#define IEEE80211_HSM_STATE_NONE 255 /* invalid state */
#define IEEE80211_HSM_EVENT_NONE 255 /* invalid event */
#define IEEE80211_HSM_MAX_STATE_NAME 128 
/*
 * structure describing a state.
 */
typedef struct _ieee80211_state_info {
    u_int8_t  state;                                             /* the state id */
    u_int8_t  parent_state;                                      /* valid for sub states */ 
    u_int8_t  initial_substate;                                  /* initial substate for the state */ 
    u_int8_t  has_substates;                                     /* true if this state has sub states */
    const char      *name;                                       /* name of the state */
    void (*ieee80211_hsm_entry) (void *ctx);                     /* entry action routine */
    void (*ieee80211_hsm_exit)  (void *ctx);                     /* exit action routine  */
    bool (*ieee80211_hsm_event) (void *ctx, u_int16_t event,
                                 u_int16_t event_data_len, void *event_data);     /* event handler. returns true if event is handled */
} ieee80211_state_info;


#define IEEE80211_HSM_MAX_NAME   64
#define IEEE80211_HSM_MAX_STATES 200 
#define IEEE80211_HSM_MAX_EVENTS 200 

typedef struct _ieee80211_hsm *ieee80211_hsm_t; 

#define IEEE80211_HSM_ENTRY(name,state,parent,initsubstate,has_substates) \
    { state,parent,initsubstate,has_substates, \
            "##name", ieee80211_hsm_##name_entry, ieee80211_hsm_##name_exit, ieee80211_##name_event }


/*
 * flag definitions
 */
#define IEEE80211_HSM_ASYNCHRONOUS  0x0  /* run SM asynchronously */
#define IEEE80211_HSM_SYNCHRONOUS   0x1  /* run SM synchronously */

/*
 * create a HSM and return a handle to it.
 */
ieee80211_hsm_t ieee80211_sm_create(osdev_t                    oshandle, 
                                    const char                 *name, 
                                    void                       *ctx, 
                                    u_int8_t                   init_state,
                                    const ieee80211_state_info *state_info, 
                                    u_int8_t                   num_states, 
                                    u_int8_t                   max_queued_events,
                                    u_int16_t                  event_data_len, 
                                    mesgq_priority_t           priority,
                                    u_int32_t                  flags,
                                    void (*ieee80211_debug_print) (void *ctx, const char *fmt,...), 
                                    const char **event_names,   /* array of  event name strings. */
                                    u_int32_t num_event_names   /* length of the array*/
                                    );
/*
 * delete HSM.
 */
void ieee80211_sm_delete(ieee80211_hsm_t hsm); 

/*
 * event into the HSM.
 * all the events should be directed via this function.
 */
void  ieee80211_sm_dispatch(ieee80211_hsm_t hsm,u_int16_t event, 
                            u_int16_t event_data_len, void *event_data); 

/*
 * event into the HSM.
 * function to set the new state. 
 */
void  ieee80211_sm_transition_to(ieee80211_hsm_t hsm,u_int8_t state); 

/*
 * return the last dispatched event.
 */
u_int8_t  ieee80211_sm_get_lastevent(ieee80211_hsm_t hsm);

/*
 * return the current state.
 */
u_int8_t ieee80211_sm_get_curstate(ieee80211_hsm_t hsm);

/*
 * return the next state.
 *  if called while the SM is transitioning to a new state, it will return the new state,
 *  else returs the current state.
 */
u_int8_t ieee80211_sm_get_nextstate(ieee80211_hsm_t hsm);

/*
 * return the current state's name.
 */
const char *ieee80211_sm_get_current_state_name(ieee80211_hsm_t hsm);

/*
 * return a state's name.
 */
const char *ieee80211_sm_get_state_name(ieee80211_hsm_t hsm, u_int8_t cur_state);

/*
 * dispatch synchronously.
 * WARNING: This function should be used with care.
 * this function dispatches event synchronously (in the context of the calling thread).
 * should only be used on systems that are single threaded . usually to stop
 * the sm synchronously.
 * if flush flag is true, it will flush the curent messgaes on the queue before delivering 
 * the new message.
 */
void  ieee80211_sm_dispatch_sync(ieee80211_hsm_t hsm,u_int16_t event, 
                                 u_int16_t event_data_len, void *event_data, bool flush);

/* Reset SM and transition to INIT state */
void ieee80211_sm_reset(ieee80211_hsm_t hsm, u_int8_t init_state, os_mesg_handler_t msg_handler);

/* Drain SM message queue */
void ieee80211_sm_msgq_drain(ieee80211_hsm_t hsm, os_mesg_handler_t msg_handler);
#endif
