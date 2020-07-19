/*
 * Copyright (c) 2011, 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc. 
 *
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * Copyright (c) 2010, Atheros Communications Inc. 
 * All Rights Reserved.
 *
 */

#include "ieee80211_sm.h"
#include "ieee80211_var.h"

/*
 * To automatically include debugging history, define ENABLE_HSM_HISTORY=1 in compiler options
 */
#ifndef ENABLE_HSM_HISTORY
#define ENABLE_HSM_HISTORY            0
#endif

#if ENABLE_HSM_HISTORY
#define IEEE80211_HSM_HISTORY_SIZE              10

typedef enum hsm_trace_type {
    HSM_EVENT_STATE_TRANSITION = 1,
    HSM_EVENT_MSG_PROCESSING,
    HSM_EVENT_MSG_QUEUEING
} hsm_trace_type_t;

typedef struct hsm_history_info {
    hsm_trace_type_t    trace_type;
    int                 event_type;
    u_int8_t            initial_state;
    u_int8_t            final_state;
} hsm_history_info_t;

typedef struct hsm_history {
    spinlock_t            hh_lock;
    int                   index;
    hsm_history_info_t    data[IEEE80211_HSM_HISTORY_SIZE];
} hsm_history_t;
#endif    /* ENABLE_HISTORY */

typedef struct _ieee80211_hsm {
    u_int8_t                      name[IEEE80211_HSM_MAX_NAME];  /* name of the state machine */
    u_int8_t                      cur_state;
    u_int8_t                      next_state; /* is different from cur_state in the middle of ieee80211_transition_to */
    u_int8_t                      event_state; /* holds the current state to which  the event is delivered to */
    u_int8_t                      num_states;
    u_int8_t                      last_event;  /* last event */
    osdev_t                       oshandle;
    const ieee80211_state_info    *state_info;
    void                          *ctx;                    /* context specific to the caller */
    u_int32_t                     in_state_transition : 1; /* in state transition */
    os_mesg_queue_t               mesg_q;
    const char                    **event_names;   /* for debug printing */
    u_int32_t                     num_event_names; /* for debug printing */
#if ENABLE_HSM_HISTORY
    hsm_history_t                 history;
#endif    /* ENABLE_HISTORY */

    void (*ieee80211_hsm_debug_print) (void *ctx, const char *fmt,...);     
} ieee80211_hsm; 

#define IEEE80211_HSM_DPRINTF(hsm, fmt, ...)  do {                          \
        if (hsm->ieee80211_hsm_debug_print) {                               \
            (* hsm->ieee80211_hsm_debug_print) (hsm->ctx, fmt,__VA_ARGS__); \
        }                                                                   \
    } while (0)
        

#if ENABLE_HSM_HISTORY
static void hsm_save_memento(ieee80211_hsm_t  hsm,
                             hsm_trace_type_t trace_type,
                             u_int8_t         initial_state,
                             u_int8_t         final_state,
                             int              event_type)
{
    hsm_history_t         *p_hsm_history = &(hsm->history);
    hsm_history_info_t    *p_memento;

    /*
     * History saved in circular buffer. 
     * Save a pointer to next write location and increment pointer.
     */
    spin_lock(&p_hsm_history->hh_lock);
    p_memento = &(p_hsm_history->data[p_hsm_history->index]);
    p_hsm_history->index++;
    if (p_hsm_history->index >= IEEE80211_N(p_hsm_history->data)) {
        p_hsm_history->index = 0;
    }
    spin_unlock(&p_hsm_history->hh_lock);
    
    OS_MEMZERO(p_memento, sizeof(*p_memento));
    p_memento->trace_type    = trace_type;
    p_memento->initial_state = initial_state;
    p_memento->final_state   = final_state;
    p_memento->event_type    = event_type;
}                          

static void hsm_history_init(ieee80211_hsm_t  hsm)
{
    spin_lock_init(&(hsm->history.hh_lock));
}

static void hsm_history_delete(ieee80211_hsm_t  hsm)
{
    spin_lock_destroy(&(hsm->history.hh_lock));
}

#endif    /* ENABLE_HISTORY */

static void  ieee80211_sm_dispatch_sync_internal(void *ctx,u_int16_t event, 
                                                 u_int16_t event_data_len, void *event_data);

/*
 * create a HSM and return a handle to it.
 */
ieee80211_hsm_t ieee80211_sm_create(osdev_t                       oshandle, 
                                    const char                    *name,
                                    void                          *ctx, 
                                    u_int8_t                      init_state, 
                                    const ieee80211_state_info    *state_info,
                                    u_int8_t                      num_states,
                                    u_int8_t                      max_queued_events, 
                                    u_int16_t                     event_data_len, 
                                    mesgq_priority_t              priority,
                                    u_int32_t                     flags,
                                    void (*ieee80211_debug_print) (void *ctx,const char *fmt,...), 
                                    const char                    **event_names, 
                                    u_int32_t                     num_event_names)
{
    ieee80211_hsm_t              hsm;
    u_int32_t                    i;
    mesgq_event_delivery_type    mq_type;

    if (num_states > IEEE80211_HSM_MAX_STATES) {
        return NULL;
    }

    /*
     * validate the state_info table.
     * the entries need to be valid and also  
     * need to be in order.
     */
    for (i = 0; i < num_states; ++i) {
        u_int8_t state_visited[IEEE80211_HSM_MAX_STATES] = {0};
        u_int8_t state,next_state;
        /*
         * make sure that the state definitions are in order
         */
        if ((state_info[i].state >= IEEE80211_HSM_MAX_STATES) || (state_info[i].state != i)) {
            if (ieee80211_debug_print)
                ieee80211_debug_print(ctx,  "HSM: %s : entry %d has invalid state %d \n", name, i,state_info[i].state); 

            return NULL;
        }
        if ((state_info[i].has_substates) && (state_info[i].initial_substate == IEEE80211_HSM_STATE_NONE)) {
            if (ieee80211_debug_print)
                ieee80211_debug_print(ctx,  "HSM: %s : entry %d is marked as super state but has no initial sub state \n", name, i); 

            return NULL;
        }
        if ((!state_info[i].has_substates) && (state_info[i].initial_substate != IEEE80211_HSM_STATE_NONE)) {
            if (ieee80211_debug_print)
                ieee80211_debug_print(ctx,  "HSM: %s : entry %d is not a super state but has initial sub state \n", name, i); 

            return NULL;
        }
        if ((state_info[i].has_substates) && (state_info[state_info[i].initial_substate].parent_state != i)) {
            if (ieee80211_debug_print)
                ieee80211_debug_print(ctx,  "HSM: %s : entry %d initial sub state is not a sub state \n", name, i); 

            return NULL;
        }
        /* detect if there is any loop in the hierarichy */
        state = state_info[i].state;
        while(state != IEEE80211_HSM_STATE_NONE) {
            if (state_visited[state]) {
                if (ieee80211_debug_print)
                    ieee80211_debug_print(ctx,  "HSM: %s : detected a loop with entry %d \n", name, i); 

                return NULL;
            }
            
            state_visited[state] = 1;
            next_state = state_info[state].parent_state;

            if (next_state != IEEE80211_HSM_STATE_NONE) {
                if (!state_info[next_state].has_substates) {
                    if (ieee80211_debug_print)
                        ieee80211_debug_print(ctx,  "HSM: %s : state %d is marked as perent of %d but is not a super state \n", 
                                              name,next_state,state); 

                    return NULL;
                }
            }
            state = next_state;
        }               
    }
    
    hsm = (ieee80211_hsm_t) OS_MALLOC(oshandle, sizeof(ieee80211_hsm), 0);
    if (hsm == NULL) {
        if (ieee80211_debug_print)
            ieee80211_debug_print(ctx, "HSM: %s : hsm allocation failed \n", name); 

        return NULL;
    }
    /* Clear hsm structure */
    OS_MEMZERO(hsm, sizeof(ieee80211_hsm));
    if(flags & IEEE80211_HSM_SYNCHRONOUS) {
        mq_type = MESGQ_SYNCHRONOUS_EVENT_DELIVERY;
    } else {
        mq_type = MESGQ_ASYNCHRONOUS_EVENT_DELIVERY;
    }
    if (OS_MESGQ_INIT(oshandle, &hsm->mesg_q, event_data_len,
                      max_queued_events, ieee80211_sm_dispatch_sync_internal, hsm, priority, mq_type) != 0) {
        if (ieee80211_debug_print)
            ieee80211_debug_print(ctx, "HSM: %s : OS_MESGQ_INIT  failed \n", name); 

        OS_FREE(hsm);

        return NULL;
    }

#if ENABLE_HSM_HISTORY
    hsm_history_init(hsm);
#endif    /* ENABLE_HISTORY */

    hsm->cur_state                 = init_state;
    hsm->num_states                = num_states;
    hsm->oshandle                  = oshandle;
    hsm->state_info                = state_info;
    hsm->ctx                       = ctx;
    hsm->last_event                = IEEE80211_HSM_EVENT_NONE;
    hsm->ieee80211_hsm_debug_print = ieee80211_debug_print;
    hsm->in_state_transition       = false;
    hsm->event_names               = event_names;
    hsm->num_event_names           = num_event_names;

    /* strncpy - don't forget the string terminator */
    i = 0;
    while ((name[i] != '\0') && (i < IEEE80211_HSM_MAX_NAME)) {
        hsm->name[i] = name[i];
        ++i;
    }
    if (i < IEEE80211_HSM_MAX_NAME) {
        hsm->name[i] = '\0';
    }
    
    return hsm;    
}

/*
 * delete HSM.
 */
void ieee80211_sm_delete(ieee80211_hsm_t hsm)
{
#if ENABLE_HSM_HISTORY
    hsm_history_delete(hsm);
#endif    /* ENABLE_HISTORY */

    OS_MESGQ_DESTROY(&(hsm->mesg_q));

    OS_FREE(hsm);
}

u_int8_t ieee80211_sm_get_lastevent(ieee80211_hsm_t hsm)
{
    return (hsm->last_event);
}

u_int8_t ieee80211_sm_get_curstate(ieee80211_hsm_t hsm)
{
    return (hsm->cur_state);
}

u_int8_t ieee80211_sm_get_nextstate(ieee80211_hsm_t hsm)
{
    return (hsm->next_state);
}

const char *ieee80211_sm_get_state_name(ieee80211_hsm_t hsm, u_int8_t state)
{
    return (hsm->state_info[state].name);
}

const char *ieee80211_sm_get_current_state_name(ieee80211_hsm_t hsm)
{
    return (hsm->state_info[hsm->cur_state].name);
}

/*
 * event into the HSM.
 * all the events should be dispatched via this function.
 */
void ieee80211_sm_dispatch(ieee80211_hsm_t hsm,u_int16_t event, 
                           u_int16_t event_data_len, void *event_data)
{
    if(hsm) {
#if ENABLE_HSM_HISTORY
        hsm_save_memento(hsm,
                         HSM_EVENT_MSG_QUEUEING,
                         hsm->cur_state,
                         hsm->cur_state,
                         event);
#endif    /* ENABLE_HISTORY */

        if (OS_MESGQ_SEND(&hsm->mesg_q, event, event_data_len, event_data) != 0) {
            qdf_alert("HSM: %s : FAILED to deliver event %d !!!", hsm->name, event);
        }
    }
}


static void  ieee80211_sm_dispatch_sync_internal(void      *ctx,
                                                 u_int16_t event, 
                                                 u_int16_t event_data_len, 
                                                 void      *event_data)
{
    ieee80211_hsm_t    hsm           = (ieee80211_hsm_t) ctx;
    bool               event_handled = false;
    u_int8_t           state         = hsm->cur_state;

    if (event == IEEE80211_HSM_EVENT_NONE) {
        IEEE80211_HSM_DPRINTF(hsm, "HSM: %s : invalid event %d\n", hsm->name, event); 
        return;
    }
    hsm->last_event = event;

#if ENABLE_HSM_HISTORY
    hsm_save_memento(hsm,
                     HSM_EVENT_MSG_PROCESSING,
                     hsm->cur_state,
                     hsm->cur_state,
                     event);
#endif    /* ENABLE_HISTORY */

    if (hsm->event_names) {
        const char    *event_name = NULL;
        
        if (event < hsm->num_event_names) {
            event_name = hsm->event_names[event];
        }
        IEEE80211_HSM_DPRINTF(hsm, "HSM: %s :current state %s event %s[%d] \n",
                              hsm->name, hsm->state_info[hsm->cur_state].name, event_name?event_name:"UNKOWN_EVENT", event); 
    } else {
        IEEE80211_HSM_DPRINTF(hsm, "HSM: %s :current state %s event %d \n",
                              hsm->name, hsm->state_info[hsm->cur_state].name, event); 
    }
    /*
     * go through the state hierarchy and  until the 
     * event is handled.
     */
    while (!event_handled && (state != IEEE80211_HSM_STATE_NONE)) {
        hsm->event_state = state;
        event_handled = (*hsm->state_info[state].ieee80211_hsm_event) (hsm->ctx, event, event_data_len, event_data);
        state = hsm->state_info[state].parent_state;
    }
    if (!event_handled) {
        IEEE80211_HSM_DPRINTF(hsm, "HSM: %s : event %d not handled in state %s \n", hsm->name, 
                          event, hsm->state_info[hsm->cur_state].name); 
    }
}

void  ieee80211_sm_dispatch_sync(ieee80211_hsm_t hsm,
                                 u_int16_t       event,
                                 u_int16_t       event_data_len, 
                                 void            *event_data, 
                                 bool            flush)
{
    if (OS_MESGQ_SEND_SYNC(&hsm->mesg_q, event, event_data_len, event_data, flush) != 0) {
        IEEE80211_HSM_DPRINTF(hsm, "HSM: %s : FAILED to deliver event %d !!! \n", hsm->name, event);
    }
}

/*
 * function to set the new state. 
 */
void  ieee80211_sm_transition_to(ieee80211_hsm_t hsm, u_int8_t state)
{
    const ieee80211_state_info    *state_info = hsm->state_info;
    u_int8_t                      cur_state   = hsm->cur_state;
    u_int8_t                      event_state = hsm->event_state; /* the state handling the event that caused this transition */

    /* cannot change state from state entry/exit routines */
    KASSERT((hsm->in_state_transition == 0),("can not call state transition from entry/exit routines"));

    hsm->in_state_transition = 1; /* in state transition */

#if ENABLE_HSM_HISTORY
    hsm_save_memento(hsm,
                     HSM_EVENT_STATE_TRANSITION,
                     hsm->cur_state,
                     state,
                     0xFF);
#endif    /* ENABLE_HISTORY */

    if (state_info[event_state].parent_state != state_info[state].parent_state) {
        IEEE80211_HSM_DPRINTF(hsm, "HSM: %s : invalid transition from %d to %d .do not have the same parent \n", 
                          hsm->name, event_state, state); 
        return;
    }

    if ((state == IEEE80211_HSM_STATE_NONE) || (state >= IEEE80211_HSM_MAX_STATES)) {
        IEEE80211_HSM_DPRINTF(hsm, "HSM: %s : to state %d needs to be a valid state current_state=%d\n", hsm->name, event_state, state); 
        return;
    }
    IEEE80211_HSM_DPRINTF(hsm, "HSM: %s: transition %s => %s \n", 
        hsm->name, state_info[cur_state].name, state_info[state].name); 

    hsm->next_state = state;
    /*
     * call the exit function(s) of the current state hierarchy
     * starting from substate.
     */
    while(cur_state != IEEE80211_HSM_STATE_NONE) {
        if (state_info[cur_state].ieee80211_hsm_exit) {
            state_info[cur_state].ieee80211_hsm_exit(hsm->ctx);
        }
        cur_state = state_info[cur_state].parent_state;
    }       
    /*
     * call the entry function(s) of the current state hierarchy
     * starting from superstate.
     */
    cur_state = state;
    while (cur_state != IEEE80211_HSM_STATE_NONE) {
        if (state_info[cur_state].ieee80211_hsm_entry) {
            state_info[cur_state].ieee80211_hsm_entry(hsm->ctx);
        }
        hsm->cur_state = cur_state;
        cur_state = state_info[cur_state].initial_substate;
        if (cur_state != IEEE80211_HSM_STATE_NONE) {
            IEEE80211_HSM_DPRINTF(hsm, "HSM: %s: Entering Initial sub state %s \n", 
                hsm->name, state_info[cur_state].name); 
        }
    }       
    hsm->next_state = hsm->cur_state;
    hsm->in_state_transition = 0; /* out of state transition */
}

/*
 * Reset SM and transition to INIT state.
 */
void ieee80211_sm_reset(ieee80211_hsm_t hsm, u_int8_t init_state, os_mesg_handler_t msg_handler)
{
    OS_MESGQ_DRAIN(&hsm->mesg_q, msg_handler);
    hsm->cur_state = init_state;
}

/*
 * Drain SM queue.
 */
void ieee80211_sm_msgq_drain(ieee80211_hsm_t hsm, os_mesg_handler_t msg_handler)
{
    OS_MESGQ_DRAIN(&hsm->mesg_q, msg_handler);
}

EXPORT_SYMBOL(ieee80211_sm_dispatch);
EXPORT_SYMBOL(ieee80211_sm_get_current_state_name);
EXPORT_SYMBOL(ieee80211_sm_transition_to);
EXPORT_SYMBOL(ieee80211_sm_create);
EXPORT_SYMBOL(ieee80211_sm_delete);
EXPORT_SYMBOL(ieee80211_sm_get_curstate);

