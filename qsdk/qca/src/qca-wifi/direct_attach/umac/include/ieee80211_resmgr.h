/*
 * Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * Copyright (c) 2008 Atheros Communications Inc.
 * All Rights Reserved.
 * 
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */

#ifndef _IEEE80211_RESMGR_H_ 
#define _IEEE80211_RESMGR_H_

struct ieee80211_resmgr;
typedef struct ieee80211_resmgr *ieee80211_resmgr_t;

struct ieee80211_resmgr_vap_priv;
typedef struct ieee80211_resmgr_vap_priv *ieee80211_resmgr_vap_priv_t;

/* Resource manager modes */
typedef enum {
    IEEE80211_RESMGR_MODE_SINGLE_CHANNEL,
    IEEE80211_RESMGR_MODE_MULTI_CHANNEL,
} ieee80211_resmgr_mode;

/* Notifications from Resource Manager to registered modules */
typedef enum {
    IEEE80211_RESMGR_BSSCHAN_SWITCH_COMPLETE,
    IEEE80211_RESMGR_OFFCHAN_SWITCH_COMPLETE,
    IEEE80211_RESMGR_OFFCHAN_ABORTED,
    IEEE80211_RESMGR_VAP_START_COMPLETE,
    IEEE80211_RESMGR_VAP_RESTART_COMPLETE,
    IEEE80211_RESMGR_CHAN_SWITCH_COMPLETE,
    IEEE80211_RESMGR_SCHEDULE_BSS_COMPLETE,

    IEEE80211_RESMGR_NOTIFICATION_TYPE_COUNT,
} ieee80211_resmgr_notification_type;

typedef enum {
IEEE80211_RESMGR_STATUS_SUCCESS,
IEEE80211_RESMGR_STATUS_NO_ACTIVE_VAP, /* no active vap exists */
IEEE80211_RESMGR_STATUS_NOT_SUPPORTED,  /* can not support */
} ieee80211_resmgr_notification_status;

typedef struct _ieee80211_resmgr_notification {
    ieee80211_resmgr_notification_type    type;
    u_int16_t                             req_id; /* ID of the request causing this event*/ 
    ieee80211_resmgr_notification_status  status;
    ieee80211_vap_t                       vap;    /* VAP associated with notification (if any) */
} ieee80211_resmgr_notification;

typedef void (*ieee80211_resmgr_notification_handler) (ieee80211_resmgr_t resmgr,
                                                       ieee80211_resmgr_notification *notif,
                                                       void *arg);
/*
 * device events for resource manager.
 */
#define IEEE80211_DEVICE_START 1
#define IEEE80211_DEVICE_STOP  2

#define IEEE80211_RESMGR_MAX_NOA_SCHEDULES  2

#include "ieee80211_resmgr_oc_scheduler.h"

typedef enum {
    IEEE80211_RESMGR_NOA_INVALID_PRIO = 0,
    IEEE80211_RESMGR_NOA_HIGH_PRIO,
    IEEE80211_RESMGR_NOA_MEDIUM_PRIO,
    IEEE80211_RESMGR_NOA_LOW_PRIO,
    IEEE80211_RESMGR_NOA_MAX_PRIO
} ieee80211_resmgr_noa_sched_priority_t;

typedef struct ieee80211_resmgr_noa_schedule {
    u_int8_t                                type_count; /* 1: one_shot, 255: periodic, 1 - 254 (number of intervals) */
    ieee80211_resmgr_noa_sched_priority_t   priority;
    u_int32_t                               start_tsf;  /* usec */
    u_int32_t                               duration;   /* usec */
    u_int32_t                               interval;   /* usec */
} *ieee80211_resmgr_noa_schedule_t;

typedef void (*ieee80211_resmgr_noa_event_handler) (void *arg,
                                                       ieee80211_resmgr_noa_schedule_t noa_schedules,
                                                       u_int8_t num_schedules);

struct ieee80211_resmgr_func_table {
    void (* resmgr_create_complete) (ieee80211_resmgr_t);
    void (* resmgr_delete_prepare )(ieee80211_resmgr_t resmgr);
    void (* resmgr_delete) (ieee80211_resmgr_t);
    int  (* resmgr_register_notification_handler)(ieee80211_resmgr_t resmgr, 
                                                   ieee80211_resmgr_notification_handler notificationhandler,
                                                   void  *arg);
    int  (* resmgr_unregister_notification_handler)(ieee80211_resmgr_t resmgr, 
                                                     ieee80211_resmgr_notification_handler notificationhandler,
                                                     void  *arg);
    int  (* resmgr_request_offchan)(ieee80211_resmgr_t resmgr, struct ieee80211_ath_channel *chan, 
                                     u_int16_t reqid, u_int16_t max_bss_chan, u_int32_t max_dwell_time);
    int  (* resmgr_request_bsschan)(ieee80211_resmgr_t resmgr, u_int16_t reqid);
    int  (* resmgr_request_chanswitch)(ieee80211_resmgr_t resmgr, ieee80211_vap_t vap, 
                                        struct ieee80211_ath_channel *chan, u_int16_t reqid);
    int  (* resmgr_vap_start) (ieee80211_resmgr_t resmgr, ieee80211_vap_t vap, 
                                        struct ieee80211_ath_channel *chan, u_int16_t reqid, u_int16_t max_start_time, u_int8_t restart);
    void (* resmgr_vattach) (ieee80211_resmgr_t resmgr, ieee80211_vap_t vap);
    void (* resmgr_vdetach) (ieee80211_resmgr_t resmgr, ieee80211_vap_t vap);
    const char* (* resmgr_get_notification_type_name) (ieee80211_resmgr_t resmgr, ieee80211_resmgr_notification_type type);
    int  (* resmgr_register_noa_event_handler)(ieee80211_resmgr_t resmgr,
                                                         ieee80211_vap_t vap, ieee80211_resmgr_noa_event_handler handler, void *arg);
    int  (* resmgr_unregister_noa_event_handler)(ieee80211_resmgr_t resmgr,
                                                         ieee80211_vap_t vap);
    int  (* resmgr_off_chan_sched_set_air_time_limit) (ieee80211_resmgr_t resmgr,
    ieee80211_vap_t vap, u_int32_t scheduled_air_time_limit );
    u_int32_t  (* resmgr_off_chan_sched_get_air_time_limit) (ieee80211_resmgr_t resmgr,
    ieee80211_vap_t vap);
    int  (* resmgr_setmode) (ieee80211_resmgr_t resmgr, ieee80211_resmgr_mode mode);
    ieee80211_resmgr_mode (* resmgr_getmode)(ieee80211_resmgr_t resmgr);
    int  (* resmgr_vap_stop) (ieee80211_resmgr_t resmgr, ieee80211_vap_t vap, 
                                                               u_int16_t reqid);
};

typedef struct ieee80211_resmgr_func_table *resmgr_func_t;

#ifndef UMAC_SUPPORT_RESMGR
#define UMAC_SUPPORT_RESMGR 0
#endif

#if UMAC_SUPPORT_RESMGR

/*
 * attach routine for direct attach solution.
 * need to move out once resmgr move down into
 * a separate directory.
 */
#if DA_SUPPORT
void ieee80211_resmgr_attach(struct ieee80211com *ic);
#else
#define ieee80211_resmgr_attach(ic) /**/
#endif

/*
 * API to create a Resource manager.
 */
#define ieee80211_resmgr_create(_ic, _mode) _ic->ic_resmgr_create(_ic,_mode)

/*
* API to delete Resource manager.
*/
static INLINE void ieee80211_resmgr_delete(ieee80211_resmgr_t resmgr)
{
    ((resmgr_func_t)resmgr)->resmgr_delete(resmgr);   
}

/*
 * Portion of the Resource Manager initialization that requires that all other
 * objects be also initialized
 */
static INLINE void ieee80211_resmgr_create_complete(ieee80211_resmgr_t resmgr)
{
    ((resmgr_func_t)resmgr)->resmgr_create_complete(resmgr);   
}


/*
 * Portion of the Resource Manager deletion that requires that all other
 * objects be still initialized
 */
static INLINE void ieee80211_resmgr_delete_prepare(ieee80211_resmgr_t resmgr)
{
    ((resmgr_func_t)resmgr)->resmgr_delete_prepare(resmgr);   
}

/**
 * @register a resmgr event handler.
 * ARGS :
 *  ieee80211_resmgr_event_handler : resmgr event handler
 *  arg                            : argument passed back via the evnt handler
 * RETURNS:
 *  on success returns 0.
 *  on failure returns a negative value.
 * allows more than one event handler to be registered.
 */

static INLINE int ieee80211_resmgr_register_notification_handler(ieee80211_resmgr_t resmgr, 
                                                   ieee80211_resmgr_notification_handler notificationhandler,
                                                   void  *arg)
{
    return ((resmgr_func_t)resmgr)->resmgr_register_notification_handler(resmgr,notificationhandler,arg);   
}




/**
 * @unregister a resmgr event handler.
 * ARGS :
 *  ieee80211_resmgr_event_handler : resmgr event handler
 *  
 * RETURNS:
 *  on success returns 0.
 *  on failure returns a negative value.
 * 
 */

static INLINE int ieee80211_resmgr_unregister_notification_handler(ieee80211_resmgr_t resmgr, 
                                                     ieee80211_resmgr_notification_handler notificationhandler,
                                                     void  *arg)
{
    return ((resmgr_func_t)resmgr)->resmgr_unregister_notification_handler(resmgr,notificationhandler,arg);   
}

/**
 * @ off channel request.
 * ARGS :
 *  resmgr  : handle to resource manager.
 *  chan    : off channel to switch to. 
 *  reqid   : id of the requestor.
 *  max_bss_chan   : timeout on the bss chan, if 0 the vaps can stay 
 *                   on the bss chan as long as they want(background scan). 
 *                   if non zero then the resource manager completes 
 *                   the off channel request in max_bss_chan by forcing the vaps to pause 
 *                   if the vaps are active at the end of max_bss_chan timeout.
 * RETURNS:
 *  returns EOK if the caller can change the channel synchronously and 
 *  returns EBUSY if the resource manager processes the request asynchronously and 
 *    at the end of processing the request the Resource Manager sends the 
 *    IEEE80211_RESMGR_OFFCHAN_SWITCH_COMPLETE notification to all the registred handlers.
 *  returns  EPERM if there is traffic .
 *  returns -ve for all other failure cases.
 */
static INLINE int ieee80211_resmgr_request_offchan(ieee80211_resmgr_t resmgr, struct ieee80211_ath_channel *chan, 
                                     u_int16_t reqid, u_int16_t max_bss_chan, u_int32_t max_dwell_time)
{
    return ((resmgr_func_t)resmgr)->resmgr_request_offchan(resmgr,chan,reqid,max_bss_chan,max_dwell_time);   
}

/**
 * @ bss channel request.
 * ARGS :
 *  resmgr  : handle to resource manager.
 *  reqid   : id of the requestor.
 * RETURNS:
 *  returns EOK if the caller can change the channel synchronously and 
 *  returns EBUSY if the resource manager processes the request asynchronously and 
 *     at the end of processing the request the Resource Manager posts the 
 *     IEEE80211_RESMGR_BSSCHAN_SWITCH_COMPLETE event to all the registred event handlers.
 */
static INLINE int ieee80211_resmgr_request_bsschan(ieee80211_resmgr_t resmgr, u_int16_t reqid)
{
    return ((resmgr_func_t)resmgr)->resmgr_request_bsschan(resmgr,reqid);   
}

/**
 * @ channel siwth request.
 * ARGS :
 *  resmgr  : handle to resource manager.
 *  vap     : handle to the vap.
 *  chan    : off channel  to switch to. 
 * RETURNS:
 *  returns EOK if the caller can change the channel synchronously and 
 *  returns EBUSY if the resource manager processes the request asynchronously and 
 *     at the end of processing the request the Resource Manager posts the 
 *     IEEE80211_RESMGR_CHAN_SWITCH_COMPLETE event to all the registred event handlers.
 *  return EPERM if this request is from a non primary VAP and
 *     it does not support multiple vaps on different channels yet.
 *  This is primarily used by STA vap when it wants to change channel after it is associated
 *  (one of the reasons a STA vap could switch channel is CSA from its AP). 
 */
static INLINE int ieee80211_resmgr_request_chanswitch(ieee80211_resmgr_t resmgr, ieee80211_vap_t vap, 
                                        struct ieee80211_ath_channel *chan, u_int16_t reqid)
{
    return ((resmgr_func_t)resmgr)->resmgr_request_chanswitch(resmgr,vap,chan,reqid);   
}

/**
 * @ vap start request.
 * ARGS :
 *  resmgr  : handle to resource manager.
 *  vap     : handle to the vap.
 *  chan    : off channel  to switch to. 
 *  reqid   : id of the requestor.
 *  max_start_time : time requested by the VAP to complete start operating
 *                    during start the resource manager/shceduler will try to 
 *                    to prevent the channel switch for max_start_time to allow
 *                    Vap to complete start.  
 * RETURNS:
 *  returns EOK if the caller can change the channel synchronously and 
 *  returns EBUSY if the resource manager processes the request asynchronously and 
 *     at the end of processing the request the Resource Manager posts the 
 *     IEEE80211_RESMGR_VAP_START_COMPLETE event to all the registred event handlers.
 *  return EPERM if this request is from a non primary VAP and
 *     it does not support multiple vaps on different channels yet.
 */
static INLINE int ieee80211_resmgr_vap_start(ieee80211_resmgr_t resmgr, ieee80211_vap_t vap, 
                                        struct ieee80211_ath_channel *chan, u_int16_t reqid, u_int16_t max_start_time, u_int8_t restart)
{
    return ((resmgr_func_t)resmgr)->resmgr_vap_start(resmgr,vap,chan,reqid,max_start_time,restart);   
}
/*
 * @resmgr vap attach.
 * ARGS :
 *  resmgr  : handle to resource manager.
 *  vap     : handle to the vap.
 */
static INLINE void ieee80211_resmgr_vattach(ieee80211_resmgr_t resmgr, ieee80211_vap_t vap)
{
    ((resmgr_func_t)resmgr)->resmgr_vattach(resmgr,vap);   
}

/*
 * @notify vap delete.
 * ARGS :
 *  resmgr  : handle to resource manager.
 *  vap     : handle to the vap.
 */
static INLINE void ieee80211_resmgr_vdetach(ieee80211_resmgr_t resmgr, ieee80211_vap_t vap)
{
    ((resmgr_func_t)resmgr)->resmgr_vdetach(resmgr,vap);   
}

/*
 * @return an ASCII string describing the notification type
 * ARGS :
 *  resmgr  : handle to resource manager.
 *  type    : notification type.
 */
static INLINE const char *ieee80211_resmgr_get_notification_type_name(ieee80211_resmgr_t resmgr, ieee80211_resmgr_notification_type type)
{
    return ((resmgr_func_t)resmgr)->resmgr_get_notification_type_name(resmgr,type);   

}

/*
 *  @Register a Notice Of Absence (NOA) handler
 *  ARGS:
 *      resmgr          :   handle to resource manager.
 *    vap           :    handle to VAP structure.
 *    handler           :       Function pointer for processing NOA schedules
 *      arg             :   Opaque parameter for handler
 *  
 * RETURNS:
 *  on success returns 0.
 *  on failure returns a negative value.
 * 
 */
static INLINE int ieee80211_resmgr_register_noa_event_handler(ieee80211_resmgr_t resmgr,
    ieee80211_vap_t vap, ieee80211_resmgr_noa_event_handler handler, void *arg)
{
    return ((resmgr_func_t)resmgr)->resmgr_register_noa_event_handler(resmgr,vap,handler,arg);   
}

/*
 *  @Unregister a Notice Of Absence (NOA) handler
 *  ARGS:
 *      resmgr          :   handle to resource manager.
 *    vap           :    handle to VAP structure.
 *  
 * RETURNS:
 *  on success returns 0.
 *  on failure returns a negative value.
 * 
 */
static INLINE int ieee80211_resmgr_unregister_noa_event_handler(ieee80211_resmgr_t resmgr,
    ieee80211_vap_t vap)
{
    return ((resmgr_func_t)resmgr)->resmgr_unregister_noa_event_handler(resmgr,vap);   
}

#define     ieee80211_resmgr_exists(ic) true 

/*
 *  @Get the air-time limit when using off-channel scheduling
 *  ARGS:
 *      resmgr          :   handle to resource manager.
 *    vap           :    handle to VAP structure.
 *  
 * RETURNS:
 *   returns current air-time limit.
 * 
 */
static INLINE u_int32_t ieee80211_resmgr_off_chan_sched_get_air_time_limit(ieee80211_resmgr_t resmgr,
    ieee80211_vap_t vap
    )
{
    return ((resmgr_func_t)resmgr)->resmgr_off_chan_sched_get_air_time_limit(resmgr,vap);   
}

/*
 *  @Set the air-time limit when using off-channel scheduling
 *  ARGS:
 *      resmgr          :   handle to resource manager.
 *    vap           :    handle to VAP structure.
 *      scheduled_air_time_limit    :   unsigned integer percentage with a range of 1 to 100
 *  
 * RETURNS:
 *  on success returns 0.
 *  on failure returns a negative value.
 * 
 */
static INLINE int ieee80211_resmgr_off_chan_sched_set_air_time_limit(ieee80211_resmgr_t resmgr,
    ieee80211_vap_t vap,
    u_int32_t scheduled_air_time_limit
    )
{
    return ((resmgr_func_t)resmgr)->resmgr_off_chan_sched_set_air_time_limit(resmgr,vap, scheduled_air_time_limit);   
}

/*
 *  @Get the NOA scheduling delay when using off-channel scheduling
 *  ARGS:
 *      resmgr          :   handle to resource manager.
 *  
 * RETURNS:
 *   returns current NOA scheduling delay.
 * 
 */
u_int32_t ieee80211_resmgr_off_chan_sched_get_noa_scheduling_delay(ieee80211_resmgr_t resmgr
    );

/*
 *  @Set the NOA scheduling delay when using off-channel scheduling
 *  ARGS:
 *      resmgr          :   handle to resource manager.
 *      noa_scheduling_delay_msec    :   unsigned integer in the range of 0 to 2000 msec.
 *  
 * RETURNS:
 *  on success returns EOK.
 *  returns EIO if scheduler isn't enabled
 *  returns EINVAL if noa_scheduling_delay_msec is out of range.
 * 
 */
int ieee80211_resmgr_off_chan_sched_set_noa_scheduling_delay(ieee80211_resmgr_t resmgr,
    u_int32_t noa_scheduling_delay_msec
    );
/*
*  pause/unpause the lmac queues for the vap
*  if pause is true then perform pause operation
*  if pause is false then perform unpause operation
*  ARGS:
*  vap           :    handle to VAP structure.
*  pause         :    boolean value that controls the lmac queue pausing/unpausing  
* 
*/

#if DA_SUPPORT
void ieee80211_resgmr_lmac_vap_pause_control(ieee80211_vap_t vap,
	                                       bool pause);
#else
#define ieee80211_resgmr_lmac_vap_pause_control(vap, pause) /**/
#endif
#if UMAC_SUPPORT_RESMGR_SM
/*
*  set resource manager mode.
*  @param    resmgr          :   handle to resource manager.
*  @param    mode            :   operating mode of the resource manager.
*  @ return EOK if succesfull. EPERM if the operation is not permitted.
     EINVAL if it is not supported
*/
static INLINE int ieee80211_resmgr_setmode(ieee80211_resmgr_t resmgr, ieee80211_resmgr_mode mode)
{
    return ((resmgr_func_t)resmgr)->resmgr_setmode(resmgr,mode);   
}

/*
*  get resource manager mode.
*  @param    resmgr          :   handle to resource manager.
*  @ return operating mode of the resource manager.
*/
static INLINE ieee80211_resmgr_mode ieee80211_resmgr_getmode(ieee80211_resmgr_t resmgr)
{
    return ((resmgr_func_t)resmgr)->resmgr_getmode(resmgr);   
}



#define     ieee80211_resmgr_active(ic) true 

#else /*(!UMAC_SUPPORT_RESMGR_SM)*/

#define     ieee80211_resmgr_active(ic) false
#define     ieee80211_resmgr_setmode(_resmgr,_mode)  EINVAL
#define     ieee80211_resmgr_getmode(_resmgr) IEEE80211_RESMGR_MODE_SINGLE_CHANNEL

#endif /*(UMAC_SUPPORT_RESMGR_SM)*/
/**
 * @ vap start request.
 * ARGS :
 *  resmgr  : handle to resource manager.
 *  vap     : handle to the vap.
 *  reqid   : id of the requestor.
 *
 * RETURNS:
 *  returns EOK
 */

static INLINE int ieee80211_resmgr_vap_stop(ieee80211_resmgr_t resmgr, ieee80211_vap_t vap,
                                            u_int16_t reqid)
{
    return ((resmgr_func_t)resmgr)->resmgr_vap_stop(resmgr,vap,reqid);
}


#else

#if DA_SUPPORT
void ieee80211_resmgr_attach(struct ieee80211com *ic);
#else
#define ieee80211_resmgr_attach(ic) /**/
#endif

#define     ieee80211_resmgr_create(_ic, _mode) _ic->ic_resmgr_create(_ic,_mode)

#define     ieee80211_resmgr_setmode(_resmgr,_mode)  EINVAL
#define     ieee80211_resmgr_getmode(_resmgr) IEEE80211_RESMGR_MODE_SINGLE_CHANNEL
#define     ieee80211_resmgr_create_complete(_resmgr) /* */ 
#define     ieee80211_resmgr_delete_prepare(_resmgr) /* */ 
#define     ieee80211_resmgr_vattach(_resmgr,_ap)  /**/
#define     ieee80211_resmgr_vdetach(_resmgr, _vap) /**/
/* To keep the compiler happy add a dummy access to _handler */
#define     ieee80211_resmgr_request_offchan(resmgr,chan,reqid,max_bss_chan,max_dwell_time) EOK
#define     ieee80211_resmgr_request_bsschan(resmgr,reqid) EOK
#define     ieee80211_resmgr_request_chanswitch(resmgr,vap,chan,reqid) EOK
#define     ieee80211_resmgr_exists(ic) false 
#define     ieee80211_resmgr_active(ic) false
#define     ieee80211_resmgr_get_notification_type_name(resmgr,type) "unknown"
#define     ieee80211_resmgr_off_chan_sched_req_alloc(resmgr, vap, duration_msec, interval_msec, priority, periodic) (NULL)
#define     ieee80211_resmgr_off_chan_sched_req_start(resmgr, req_handle) (EOK)
#define     ieee80211_resmgr_off_chan_sched_req_stop(resmgr, req_handle)  (EOK)
#define     ieee80211_resmgr_off_chan_sched_req_free(resmgr, req_handle)  /**/
#define     ieee80211_resmgr_register_noa_event_handler(resmgr, vap, handler, arg) (EOK)
#define     ieee80211_resmgr_unregister_noa_event_handler(resmgr, vap)  (EOK)
#define     ieee80211_resmgr_off_chan_sched_get_air_time_limit(resmgr,vap) (100)
#define     ieee80211_resmgr_off_chan_sched_set_air_time_limit(resmgr,vap,val) (EINVAL) 
#define     ieee80211_resgmr_lmac_vap_pause_control(vap, pause)  /**/
#define     ieee80211_resmgr_off_chan_sched_get_noa_scheduling_delay(resmgr) (256)
#define     ieee80211_resmgr_off_chan_sched_set_noa_scheduling_delay(resmgr, noa_scheduling_delay_msec)  (EINVAL)

static INLINE void ieee80211_resmgr_delete(ieee80211_resmgr_t resmgr)
{
    ((resmgr_func_t)resmgr)->resmgr_delete(resmgr);
}

static INLINE int ieee80211_resmgr_register_notification_handler(ieee80211_resmgr_t resmgr, ieee80211_resmgr_notification_handler notificationhandler, void  *arg)
{
    return ((resmgr_func_t)resmgr)->resmgr_register_notification_handler(resmgr,notificationhandler,arg);   
}

static INLINE int ieee80211_resmgr_unregister_notification_handler(ieee80211_resmgr_t resmgr, 
                                                     ieee80211_resmgr_notification_handler notificationhandler,
                                                     void  *arg)
{
    return ((resmgr_func_t)resmgr)->resmgr_unregister_notification_handler(resmgr,notificationhandler,arg);   
}

static INLINE int ieee80211_resmgr_vap_start(ieee80211_resmgr_t resmgr, ieee80211_vap_t vap, 
                                        struct ieee80211_ath_channel *chan, u_int16_t reqid, u_int16_t max_start_time, u_int8_t restart)
{
    return ((resmgr_func_t)resmgr)->resmgr_vap_start(resmgr,vap,chan,reqid,max_start_time,restart);   
}

static INLINE int ieee80211_resmgr_vap_stop(ieee80211_resmgr_t resmgr, ieee80211_vap_t vap, 
                                                                             u_int16_t reqid)
{
    return ((resmgr_func_t)resmgr)->resmgr_vap_stop(resmgr,vap,reqid);   
}
#endif
#endif


