/*
* Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 *  Copyright (c) 2009 Atheros Communications Inc.  All rights reserved.
 */
/*
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

/* */

#ifndef _IEEE80211_RESMGR_OC_SCHEDULER_API_H_
#define _IEEE80211_RESMGR_OC_SCHEDULER_API_H_

#include "ieee80211_resmgr_oc_scheduler.h"

#define  IEEE80211_RESMGR_OC_SCHED_SCAN_DWELL_PRIORITY_MSEC  24

typedef enum {
    OFF_CHAN_SCHED_TSF_TIMER_EXPIRE = 1, 
    OFF_CHAN_SCHED_TSF_TIMER_CHANGE,
    OFF_CHAN_SCHED_START_REQUEST,
    OFF_CHAN_SCHED_STOP_REQUEST,    
    OFF_CHAN_SCHED_VAP_TRANSITION,
    OFF_CHAN_SCHED_HALT_SCHEDULER,
    OFF_CHAN_SCHED_TSF_EVENT_MAX
} ieee80211_resmgr_oc_sched_tsftimer_event_t;

typedef enum {
    OFF_CHAN_SCHED_VAP_START = 1,
    OFF_CHAN_SCHED_VAP_UP,
    OFF_CHAN_SCHED_VAP_STOPPED,
    OFF_CHAN_SCHED_VAP_EVENT_MAX
} ieee80211_resmgr_oc_sched_vap_event_type;

typedef enum {
    OC_SCHED_OP_NONE = 0,
    OC_SCHED_OP_VAP,
    OC_SCHED_OP_SCAN,
    OC_SCHED_OP_SLEEP,
    OC_SCHED_OP_NOP,
    OC_SCHED_OP_MAX
} ieee80211_resmgr_oc_sched_operation_t;

typedef struct ieee80211_resmgr_oc_scheduler_operation {
    ieee80211_resmgr_oc_sched_operation_t operation_type;
    ieee80211_vap_t                       vap;  
} ieee80211_resmgr_oc_scheduler_operation_data_t;

/*
 * resource manager off-channel scheduler is not compiled in by default.
 */ 
#ifndef UMAC_SUPPORT_RESMGR_OC_SCHED
#define UMAC_SUPPORT_RESMGR_OC_SCHED 0
#endif

#if UMAC_SUPPORT_RESMGR_OC_SCHED

/*
**
**  Name:  ieee80211_resmgr_oc_scheduler
**
**  Parameters:
** 
**          Resmgr      - Pointer to resmgr data structure
**
**      Event_type:
**          
**          OFF_CHAN_SCHED_TSF_TIMER_EXPIRE
**              A TSF timer has expired and the scheduler may need
**              to transition to a new scheduled operation.
**
**          OFF_CHAN_SCHED_TSF_TIMER_CHANGE
**              A TSF timer has encountered a non-linear change
**              in value. This condition may occur during STA 
**              join and association operations. It may also
**              occur across transitions between STA and AP
**              MAC modes.
**
**          OFF_CHAN_SCHED_START_REQUEST
**              A new scheduler operation has been added to the
**              list of active requests.
**              
**          OFF_CHAN_SCHED_STOP_REQUEST
**              A scheduler operation has been removed from the
**              list of active requests.
**
**          OFF_CHAN_SCHED_VAP_TRANSITION
**              A Virtual Access Point (VAP) has changed its
**              operational state, i.e. starting, up, or stopped.
**
**  Return value:
**
**      EOK     -   Successful
**      EINVAL  -   Invalid error encountered
**
**  Description:
**
**      From a high-level perspective, this routine fetches the current value of the
**      TSF clock and performs a lookup to find the next scheduled operation within
**      the current schedule window. If the end of the schedule window has been 
**      encounted, then it computes a new schedule window based on currently
**      active requests in the system. It initiates or starts a new TSF timer to fire
**      at the completion of the current operation.
**  
*/

int
ieee80211_resmgr_oc_scheduler(
    ieee80211_resmgr_t resmgr,
    ieee80211_resmgr_oc_sched_tsftimer_event_t event_type
    );

/*
**
**  Name:  ieee80211_resmgr_oc_sched_get_scheduled_operation
**
**  Parameters:
** 
**          Resmgr      - Pointer to resmgr data structure
**
**      Op_data     - Pointer to structure of scheduler operation data
**
**          Operation type:
**
**              OC_SCHED_OP_NONE
**                  No operation is scheduled at present
**
**              OC_SCHED_OP_VAP
**                  A VAP operation is ready to run. The VAP
**                  pointer is valid for this type of operation.
**
**              OC_SCHED_OP_SCAN
**                  A SCAN operation is ready to run.
**
**              OC_SCHED_OP_SLEEP
**                  This operation is not currently supported.
**
**              OC_SCHED_OP_NOP
**                  No operation is scheduled at present. 
**
**          ieee80211_vap_t     - Pointer to a VAP data structure
**          
**
**  Return value:
**
**      None
**
**  Description:
**
**      This routine returns the next scheduled operation based on the 
**      TSF timer value.
**  
*/

void
ieee80211_resmgr_oc_sched_get_scheduled_operation(
    ieee80211_resmgr_t resmgr,
    ieee80211_resmgr_oc_scheduler_operation_data_t *op_data
    );

/*
**
**  Name:  ieee80211_resmgr_oc_scheduler_create
**
**  Parameters:
** 
**          Resmgr      - Pointer to resmgr data structure
**
**      Policy      - Timeslot conflict resolution policy
**
**          OFF_CHAN_SCHED_POLICY_ROUND_ROBIN
**              Timeslot conflicts are resolved by a simple
**              round-robin algorithm
**
**          OFF_CHAN_SCHED_POLICY_MRU_THRUPUT
**              Not supported currently; Most Recently Used
**              Throughput style algorithm to handle timeslot
**              conflicts.      
**
**  Return value:
**
**      EOK     -  Successful
**      ENOMEM  -  Insufficient memory needed to support scheduler
**      
**  Description:
**
**      This routine performs all initialization needed to enable
**      proper operation of the off-channel scheduling engine.
**      Working memory is allocated as well as TSF timer
**      resources utilized by the scheduler itself.
**  
*/

int 
ieee80211_resmgr_oc_scheduler_create(
    ieee80211_resmgr_t resmgr,
    ieee80211_resmgr_oc_sched_policy_t policy
    );

/*
**
**  Name:  ieee80211_resmgr_oc_scheduler_delete
**
**  Parameters:
** 
**          Resmgr      - Pointer to resmgr data structure
**
**  Return value:
**
**      None
**
**  Description:
**
**      Release all working memory and TSF timer
**      resources.
**  
*/

void 
ieee80211_resmgr_oc_scheduler_delete(
    ieee80211_resmgr_t resmgr
    );

/*
**
**  Name:  ieee80211_resmgr_oc_sched_req_alloc
**
**  Parameters:
** 
**          Resmgr      - Pointer to resmgr data structure
**
**      Vap         - Pointer to a VAP data structure
**
**      Duration        - Duration of the operation in millisecond units
**
**      Interval        - Interval of the operation in millisecond units
**
**      Priority        - Request priority
**
**          OFF_CHAN_SCHED_PRIO_LOW
**              Periodic request:
**                  Duration and interval are ignored; Scheduler
**                  allocates across whole gaps in the timeslot window.
**                  It will round-robin if there are more than one
**                  PRIO_LOW periodic request.

**              Non-periodic request:
**                  Duration is honored if there is a timeslot gap
**                  large enough to accomodate. It does
**                  NOT get a timeslot if no gap is found. However,
**                  the non-periodic PRIO_LOW has precedence
**                  over the PRIO_LOW periodic requests.
**
**          OFF_CHAN_SCHED_PRIO_HIGH
**              Periodic request:
**                  Duration and interval are honored; Scheduler
**                  will apply the scheduling policy, i.e. round-robin,
**                  in the event of a timeslot conflict.
**
**              Non-periodic request:
**                  Duration is honored; Scheduler will fulfill
**                  one-shot requests based on the first-in
**                  order when the requests are started in 
**                  the scheduler.
**          
**
**      Periodic        - Request is reoccurring or not; true or false
**
**      Request Type:   
**
**          OC_SCHEDULER_VAP_REQTYPE            
**              Request is associated with a VAP object
**
**          OC_SCHEDULER_SCAN_REQTYPE
**              Request is associated with the Scanner object
**
**          OC_SCHEDULER_NONE_REQTYPE
**              Request is not associated with any external objects
**
**  Return value:
**
**      Request Handle  - Successful
**      NULL            - Failure to allocate memory resources
**
**  Description:
**
**          Allocate and initialize a scheduler request object.
*/

ieee80211_resmgr_oc_sched_req_handle_t 
ieee80211_resmgr_oc_sched_req_alloc(
    ieee80211_resmgr_t resmgr,
    ieee80211_vap_t vap,
    u_int32_t duration, 
    u_int32_t interval,
    ieee80211_resmgr_oc_sched_prio_t priority, 
    bool periodic,
    ieee80211_resmgr_oc_sched_reqtype_t req_type
    );

/*
**
**  Name:  ieee80211_resmgr_oc_sched_req_start
**
**  Parameters:
** 
**          Resmgr      - Pointer to resmgr data structure
**
**      Req_handle  - Handle to scheduler request
**
**  Return value:
**
**      EOK     - Successful
**      EINVAL  - Invalid request handle
**
**  Description:
**
**      This routine places the request for timeslots into the active list.
**      The request does not take affect until the next scheduling window
**      takes effect. If the request needs to be honored immediately, then
**      ieee80211_resmgr_oc_scheduler(resmgr, OFF_CHAN_SCHED_START_REQUEST) 
**      must be invoked in order to compute a new schedule window.
**  
*/

int 
ieee80211_resmgr_oc_sched_req_start(
                                     ieee80211_resmgr_t resmgr,
                                     ieee80211_resmgr_oc_sched_req_handle_t req_handle
                                     );

/*
**
**  Name:  ieee80211_resmgr_oc_sched_req_stop
**
**  Parameters:
** 
**          Resmgr      - Pointer to resmgr data structure
**
**      Req_handle  - Handle to scheduler request
**
**  Return value:
**
**      EOK     - Successful
**      EINVAL  - Invalid request handle
**
**  Description:
**
**      This routine removes the request for timeslots from the active list.
**      The request does not take affect until the next scheduling window
**      takes effect. If the request needs to be honored immediately, then
**      ieee80211_resmgr_oc_scheduler(resmgr, OFF_CHAN_SCHED_STOP_REQUEST) 
**      must be invoked in order to compute a new schedule window.
**  
*/

int 
ieee80211_resmgr_oc_sched_req_stop(
                                    ieee80211_resmgr_t resmgr,
                                    ieee80211_resmgr_oc_sched_req_handle_t req_handle
                                    );

/*
**
**  Name:  ieee80211_resmgr_oc_sched_req_free
**
**  Parameters:
** 
**          Resmgr      - Pointer to resmgr data structure
**
**      Req_handle  - Handle to scheduler request
**
**  Return value:
**
**      None
**
**  Description:
**
**          Releases all resources associated with request.
*/

void 
ieee80211_resmgr_oc_sched_req_free(
    ieee80211_resmgr_t resmgr,
    ieee80211_resmgr_oc_sched_req_handle_t req_handle
    );

/*
**
**  Name:  ieee80211_resmgr_oc_sched_req_duration_usec_set
**
**  Parameters:
** 
**      Req_handle      - Handle to scheduler request
**
**      Duration_msec   - Duration of request in millisecond units
**
**  Return value:
**
**      None
**
**  Description:
**
**          Modify the duration of a scheduler request; The request
**      should be stopped before changing the duration.
*/

void 
ieee80211_resmgr_oc_sched_req_duration_usec_set(
    ieee80211_resmgr_oc_sched_req_handle_t req_handle,
    u_int32_t duration_msec
    );

/*
**
**  Name:  ieee80211_resmgr_oc_sched_req_interval_usec_set
**
**  Parameters:
** 
**      Req_handle      - Handle to scheduler request
**
**      Interval_msec       - Interval of request in millisecond units
**
**  Return value:
**
**      None
**
**  Description:
**
**          Modify the interval of a scheduler request. The request
**      should be stopped before changing the interval.
*/

void 
ieee80211_resmgr_oc_sched_req_interval_usec_set(
    ieee80211_resmgr_oc_sched_req_handle_t req_handle,
    u_int32_t interval_msec
    );

/*
**
**  Name:  ieee80211_resmgr_oc_sched_req_priority_set
**
**  Parameters:
** 
**      Req_handle      - Handle to scheduler request
**
**      Duration_msec   - Duration of request in millisecond units
**
**  Return value:
**
**      None
**
**  Description:
**
**          Modify the duration of a scheduler request; The request
**      should be stopped before changing the duration.
*/

void 
ieee80211_resmgr_oc_sched_req_priority_set(
    ieee80211_resmgr_oc_sched_req_handle_t req_handle,
    ieee80211_resmgr_oc_sched_prio_t priority
    );

/*
**
**  Name:  ieee80211_resmgr_oc_sched_vattach
**
**  Parameters:
** 
**          Resmgr      - Pointer to resmgr data structure
**
**      Vap         - Pointer to a VAP data structure
**
**      Resmgr_vap  - Pointer to private area of the VAP
**                     structure used by the Resource 
**                     manager and the off-channel
**                     scheduler.
**
**  Return value:
**
**      None
**
**  Description:
**
**          Allocates scheduler requests on behalf of a VAP
**      during the VAP attach invocation.
*/

void 
ieee80211_resmgr_oc_sched_vattach(
    ieee80211_resmgr_t resmgr,
    ieee80211_vap_t vap,
    ieee80211_resmgr_vap_priv_t resmgr_vap
    );

/*
**
**  Name:  ieee80211_resmgr_oc_sched_vdetach
**
**  Parameters:
** 
**          Resmgr      - Pointer to resmgr data structure
**
**      Resmgr_vap  - Pointer to private area of the VAP
**                     structure used by the Resource 
**                     manager and the off-channel
**                     scheduler.
**
**  Return value:
**
**      None
**
**  Description:
**
**          Releases scheduler requests on behalf of a 
**      VAP deattach invocation.
*/

void 
ieee80211_resmgr_oc_sched_vdetach(
    ieee80211_resmgr_t resmgr,
    ieee80211_resmgr_vap_priv_t resmgr_vap
    );

/*
**
**  Name:  ieee80211_resmgr_oc_sched_vap_transition
**
**  Parameters:
** 
**          Resmgr      - Pointer to resmgr data structure
**
**      Vap         - Pointer to a VAP data structure
**
**      Resmgr_vap  - Pointer to private area of the VAP
**                     structure used by the Resource 
**                     manager and the off-channel
**                     scheduler.
**
**      Event_type:
**
**          OFF_CHAN_SCHED_VAP_START
**              A new VAP is starting execution
**
**          OFF_CHAN_SCHED_VAP_UP
**              The VAP has completed its
**              startup processing and is
**              ready to use
**
**          OFF_CHAN_SCHED_VAP_STOPPED
**              VAP has stopped and doesn't
**              need scheduler timeslots.
**
**  Return value:
**
**      None
**
**  Description:
**
**      Based on the event_type, the requests associated with a VAP are 
**      started and/or stopped. In addition, the ieee80211_resmgr_oc_scheduler(resmgr,
**      OFF_CHAN_SCHED_VAP_TRANSITION) maybe invoked if required to 
**      update the schedule window.
*/

void 
ieee80211_resmgr_oc_sched_vap_transition(
    ieee80211_resmgr_t resmgr,
    ieee80211_vap_t vap,
    ieee80211_resmgr_vap_priv_t resmgr_vap,
    ieee80211_resmgr_oc_sched_vap_event_type event_type
    );

/*
**
**  Name:  ieee80211_resmgr_oc_sched_get_state
**
**  Parameters:
** 
**          Resmgr      - Pointer to resmgr data structure
**
**  Return value:
**
**      Scheduler state:
**
**          OC_SCHEDULER_OFF
**
**          OC_SCHEDULER_ON
**
**  Description:
**
**          Returns the state of the off-channel scheduler
**      either ON or OFF.
*/

ieee80211_resmgr_oc_sched_state_t 
ieee80211_resmgr_oc_sched_get_state(
    ieee80211_resmgr_t resmgr
    );

/*
**
**  Name:  ieee80211_resmgr_oc_sched_set_state
**
**  Parameters:
** 
**          Resmgr      - Pointer to resmgr data structure
**
**      Scheduler state:
**
**          OC_SCHEDULER_OFF
**
**          OC_SCHEDULER_ON
**
**
**  Return value:
**
**      None
**
**  Description:
**
**          Sets the state of the off-channel scheduler either
**      ON or OFF.
*/

void 
ieee80211_resmgr_oc_sched_set_state(
    ieee80211_resmgr_t resmgr,
    ieee80211_resmgr_oc_sched_state_t new_state
    );

/*
**
**  Name:  ieee80211_resmgr_oc_sched_register_noa_event_handler
**
**  Parameters:
** 
**          Resmgr      - Pointer to resmgr data structure
**      Vap         - Pointer to VAP structure
**      Handler     - Function pointer to NOA event handler
**      Arg         - Opaque parameter that is passed to the event handler
**
**  Return value:
**
**      EOK     - Successful
**      EINVAL  - Invalid request handle
**
**  Description:
**
**          Register or store the Notice Of Absence event handler, which
**      the off-channel scheduler invokes when a schedule change occurs.
*/

int ieee80211_resmgr_oc_sched_register_noa_event_handler(ieee80211_resmgr_t resmgr,
    ieee80211_vap_t vap, ieee80211_resmgr_noa_event_handler handler, void *arg);

/*
**
**  Name:  ieee80211_resmgr_oc_sched_unregister_noa_event_handler
**
**  Parameters:
** 
**          Resmgr      - Pointer to resmgr data structure
**      Vap         - Pointer to VAP structure
**
**  Return value:
**
**      EOK     - Successful
**      EINVAL  - Invalid request handle
**
**  Description:
**
**          Unregister or remove the Notice Of Absence (NOA) event handler.
*/

int ieee80211_resmgr_oc_sched_unregister_noa_event_handler(ieee80211_resmgr_t resmgr,
    ieee80211_vap_t vap);



/*
**
**  Name:  ieee80211_resmgr_oc_sched_get_air_time_limit
**
**  Parameters:
** 
**          Resmgr      - Pointer to resmgr data structure
**          Resmgr_vap  - Pointer to private area of the VAP
**                     structure used by the Resource 
**                     manager and the off-channel
**                     scheduler.
**
**
**  Return value:
**
**      current air-time limit as percentage (0 to 100).
**
**  Description:
**
**         Get the air-time limit used by the off-channel scheduler.
*/

u_int32_t 
ieee80211_resmgr_oc_sched_get_air_time_limit(
    ieee80211_resmgr_t resmgr,
    ieee80211_resmgr_vap_priv_t resmgr_vap
    );

/*
**
**  Name:  ieee80211_resmgr_oc_sched_set_air_time_limit
**
**  Parameters:
** 
**          Resmgr      - Pointer to resmgr data structure
**          Resmgr_vap  - Pointer to private area of the VAP
**                     structure used by the Resource 
**                     manager and the off-channel
**                     scheduler.
**          Scheduled_air_time_limit - The percentage of air-time
**                              to be allocated by the off-channel
**                              scheduler for a particular VAP.
**
**
**  Return value:
**
**      EOK     - Successful
**      EINVAL  - Air time limit (Range is 1 to 100 percent)
**
**  Description:
**
**         Set the air-time limit used by the off-channel scheduler.
*/

int 
ieee80211_resmgr_oc_sched_set_air_time_limit(
    ieee80211_resmgr_t resmgr,
    ieee80211_resmgr_vap_priv_t resmgr_vap,
    u_int32_t scheduled_air_time_limit
    );
    
/*
**
**  Name:  ieee80211_resmgr_oc_sched_get_op_name
**
**  Parameters:
**      operation_type         - type of operation. 
**
**  Return value:
**      return human readable name for the op.
**
**  Description:
**             return human readable name for the op.
**
*/

const char *ieee80211_resmgr_oc_sched_get_op_name(ieee80211_resmgr_oc_sched_operation_t operation_type);

/*
**
**  Name:  ieee80211_resmgr_oc_sched_get_noa_schedule_delay_msec
**
**  Parameters:
** 
**          sched      - Pointer to off-channel scheduler data structure
**
**  Return value:
**
**      current Notice-Of-Absence scheduling delay in msec.
**
**  Description:
**
**         Get the NOA scheduling delay used by the off-channel scheduler.
*/

u_int32_t
ieee80211_resmgr_oc_sched_get_noa_schedule_delay_msec(
    ieee80211_resmgr_oc_scheduler_t sched
    );

/*
**
**  Name:  ieee80211_resmgr_oc_sched_set_noa_schedule_delay_msec
**
**  Parameters:
** 
**          sched      - Pointer to off-channel scheduler data structure
**          noa_schedule_delay_msec - Notice-Of-Absence (NOA) scheduling delay in msec.
**
**
**  Return value:
**
**      EOK     - Successful
**      EIO      - Either resource manager or off-channel scheduler doesn't exist.
**      EINVAL  - noa scheduling delay must be less than 2000 msec.
**
**  Description:
**
**         Set the NOA scheduling delay used by the off-channel scheduler.
*/

int
ieee80211_resmgr_oc_sched_set_noa_schedule_delay_msec(
    ieee80211_resmgr_oc_scheduler_t sched,
    u_int32_t noa_schedule_delay_msec
    );

#else

#define  ieee80211_resmgr_oc_sched_get_state(resmgr) (0)
#define  ieee80211_resmgr_oc_sched_set_state(resmgr, new_state) /**/
#define  ieee80211_resmgr_oc_sched_vap_transition(resmgr, vap, resmgr_vap, event_type) /**/
#define  ieee80211_resmgr_oc_sched_vattach(resmgr, vap, resmgr_vap) /**/
#define  ieee80211_resmgr_oc_sched_vdetach(resmgr, resmgr_vap) /**/
#define  ieee80211_resmgr_oc_scheduler(resmgr, event_type) (EOK)
#define  ieee80211_resmgr_oc_sched_get_scheduled_operation(resmgr, op_data) /**/
#define  ieee80211_resmgr_oc_scheduler_create(resmgr, policy) (EOK)
#define  ieee80211_resmgr_oc_scheduler_delete(resmgr) /**/
#define  ieee80211_resmgr_oc_sched_req_alloc(resmgr, vap, duration, interval, priority, one_shot, req_type) (NULL)
#define  ieee80211_resmgr_oc_sched_req_free(resmgr, req) /**/
#define  ieee80211_resmgr_oc_sched_req_start(resmgr, req) (EOK)
#define  ieee80211_resmgr_oc_sched_req_stop(resmgr, req)  (EOK)
#define  ieee80211_resmgr_oc_sched_req_duration_usec_set(req_handle, duration_msec) /**/
#define  ieee80211_resmgr_oc_sched_req_interval_usec_set(req_handle, interval_msec) /**/
#define  ieee80211_resmgr_oc_sched_req_priority_set(req_handle, priority) /**/
#define  ieee80211_resmgr_oc_sched_register_noa_event_handler(resmgr, vap, handler, arg) (EOK)
#define  ieee80211_resmgr_oc_sched_unregister_noa_event_handler(resmgr, vap) (EOK)
#define  ieee80211_resmgr_oc_sched_get_air_time_limit(resmgr, resmgr_vap) (0)
#define  ieee80211_resmgr_oc_sched_set_air_time_limit(resmgr, resmgr_vap, scheduled_air_time_limit)  (EOK)
#define  ieee80211_resmgr_oc_sched_get_op_name(operation_type)  NULL
#define  ieee80211_resmgr_oc_sched_get_noa_schedule_delay_msec(sched) (0)
#define  ieee80211_resmgr_oc_sched_set_noa_schedule_delay_msec(sched, noa_scheduling_delay) (EOK)

#endif /* UMAC_SUPPORT_RESMGR_OC_SCHED */
#endif /* _IEEE80211_RESMGR_OC_SCHEDULER_API_H_ */
