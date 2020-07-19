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


#ifndef _IEEE80211_RESMGR_SCHEDULER_PRIV_H_
#define _IEEE80211_RESMGR_SCHEDULER_PRIV_H_

#include <ieee80211_mlme.h>
#include <ieee80211_vap_tsf_offset.h>
#include <ieee80211_resmgr.h>
#include <ieee80211_tsftimer.h>
#include <ieee80211_resmgr_oc_scheduler.h>

#define  MAX_RUN_QUEUES  2
#define  MAX_TIMESLOT_ENTRIES_PER_RUNQ 256
#define  MAX_SCHED_POLICY_ENTRIES_PER_RUNQ 32
#define  MAX_SCHEDULING_TIME_MSEC  2000
#define  MINIMUM_GAP_DURATION_USEC  16000
#define  MAX_REQUESTS_PER_VAP  OC_SCHED_REQ_INDEX_MAX
#define  INITIAL_REQUEST_ID  1
#define  OC_SCHED_NULL_ACK_DELAY 3500
#define  SWBA_INTR_DELAY_BEFORE_TBTT_USEC  10000
#define  NOA_EVENT_SCHEDULING_DELAY_FOR_NOTIFICATION 256000

#define  OC_SCHED_DEBUG  0
#define  OC_SCHED_DEFAULT_DEBUG_LEVEL  0
#define  OC_SCHED_DEFAULT_MULTI_CHAN_ENABLE  true

#if OC_SCHED_DEBUG
#define  DBG_PRINTK(_fmt, ...)  printk(_fmt, __VA_ARGS__);
#else
#define  DBG_PRINTK(_fmt, ...)
#endif /* OC_SCHED_DEBUG */

/* 
 * ** NOTE **
 *
 * There are no more free message debug bits. Thus, reusing
 * the WDS bit for now. This issue will be addressed in the
 * future.
 */
#define  IEEE80211_MSG_RESMGR_OC_SCHED  IEEE80211_MSG_WDS
#define  MS_TO_US(val) ieee80211_rm_ocs_msec_to_usec(val)
#define  TIMESLOTS_PENDING(sched, run_q_ndx) ieee80211_rm_ocs_pending_timeslots(sched, run_q_ndx)

typedef enum {
    OC_SCHEDULER_OFF = 1,
    OC_SCHEDULER_ON,
    OC_SCHEDULER_STATE_MAX
} ieee80211_resmgr_oc_sched_state_t;

typedef enum {
    INVALID = 0,
    ACTIVE,     /* Actively being scheduled */
    INACTIVE,       /* Inactive or not being scheduled */
    RESET,          /* Reset scheduling state */
    STOPPED,        /* Stop the scheduling of request */
    COMPLETED,      /* Completed scheduling of request */
    STATE_MAX       /* Maximum number of priorities */
} ieee80211_resmgr_oc_sched_req_state_t;

typedef enum {
    OC_SCHEDULER_INVALID_REQTYPE = 0,
    OC_SCHEDULER_VAP_REQTYPE,
    OC_SCHEDULER_SCAN_REQTYPE,
    OC_SCHEDULER_NONE_REQTYPE,
    OC_SCHEDULER_MAX_REQTYPE
} ieee80211_resmgr_oc_sched_reqtype_t;

/*
 * State machine states
 */
typedef enum {
    IEEE80211_OC_SCHED_STATE_IDLE = 0,
    IEEE80211_OC_SCHED_STATE_CALC,
    IEEE80211_OC_SCHED_STATE_RUN
} ieee80211_rm_oc_sched_state;

/*
 * State machine events.
 * Posted as part of API calls to the Off-Channel Scheduler
 */
typedef enum {
    IEEE80211_OC_SCHED_START_REQ = 1,
    IEEE80211_OC_SCHED_STOP_REQ,
    IEEE80211_OC_SCHED_TSFTIMER
} ieee80211_rm_oc_sched_event;

typedef enum {
    IEEE80211_OC_SCHED_KEEP_INSPT = 1,
    IEEE80211_OC_SCHED_REMOVE_INSPT
} ieee80211_rm_oc_sched_insert_pt_policy;

typedef enum {
    OC_SCHED_STATE_INIT = 1,    /* Initial or starting condition */
    OC_SCHED_STATE_RUN,         /* Actively scheduling */
    OC_SCHED_STATE_HALT,        /* Stop or halt scheduling activities */
    OC_SCHED_STATE_RESTART,     /* Restart scheduling activities */
    OC_SCHED_STATE_MAX          /* Maximum number of priorities */
} ieee80211_resmgr_oc_sched_run_state;

typedef enum {
    OC_SCHED_HIGH_PRIO_1SHOT = 0,
    OC_SCHED_HIGH_PRIO_PERIODIC,
    OC_SCHED_LOW_PRIO_1SHOT,
    OC_SCHED_LOW_PRIO_PERIODIC,
    OC_SCHED_MLME_1SHOT,
    OC_SCHED_REQ_INDEX_MAX
} ieee80211_resmgr_oc_sched_req_index;

struct ieee80211_resmgr_oc_sched_noa_event_vap {
    ieee80211_vap_t                         vap;
    ieee80211_resmgr_noa_event_handler      handler;
    void                                    *arg;
    TAILQ_ENTRY(ieee80211_resmgr_oc_sched_noa_event_vap) nev_te;
};
typedef struct ieee80211_resmgr_oc_sched_noa_event_vap *ieee80211_resmgr_oc_sched_noa_event_vap_t;

struct ieee80211_resmgr_oc_sched_req {
    ieee80211_vap_t                         vap;
    u_int32_t                               duration_usec; 
    u_int32_t                               interval_usec;
    ieee80211_resmgr_oc_sched_prio_t        priority;
    bool                                    periodic;
    bool                                    schedule_asap;
    ieee80211_resmgr_oc_sched_req_state_t   state;
    TAILQ_ENTRY(ieee80211_resmgr_oc_sched_req) req_te;
    u_int32_t                               previous_pending_timeslot_tsf;
    u_int32_t                               req_id;
    ieee80211_resmgr_oc_sched_reqtype_t     req_type;
};
typedef struct ieee80211_resmgr_oc_sched_req *ieee80211_resmgr_oc_sched_req_t;

struct ieee80211_resmgr_oc_sched_vap_data {
    ieee80211_resmgr_oc_sched_req_t         req_array[MAX_REQUESTS_PER_VAP];
    u_int32_t                               scheduled_air_time_limit;
    u_int32_t                               high_prio_air_time_percent;
};
typedef struct ieee80211_resmgr_oc_sched_vap_data *ieee80211_resmgr_oc_sched_vap_data_t;

struct ieee80211_rm_oc_sched_timeslot {
    u_int32_t start_tsf;
    u_int32_t stop_tsf;
    ieee80211_resmgr_oc_sched_req_t req;
    TAILQ_ENTRY(ieee80211_rm_oc_sched_timeslot) ts_te;
};
typedef struct ieee80211_rm_oc_sched_timeslot *ieee80211_rm_oc_sched_timeslot_t;

typedef TAILQ_HEAD(timeslot_head_s, ieee80211_rm_oc_sched_timeslot) oc_sched_timeslot_head;

struct ieee80211_rm_oc_sched_policy_entry {
    ieee80211_resmgr_oc_sched_req_t req;
    TAILQ_HEAD(, ieee80211_rm_oc_sched_timeslot) collision_q_head;
    TAILQ_ENTRY(ieee80211_rm_oc_sched_policy_entry) pe_te;
};
typedef struct ieee80211_rm_oc_sched_policy_entry *ieee80211_rm_oc_sched_policy_entry_t;

struct ieee80211_rm_oc_sched_run_q {
    ieee80211_rm_oc_sched_timeslot_t curr_ts;
    TAILQ_HEAD(, ieee80211_rm_oc_sched_timeslot) run_q_head;
    TAILQ_HEAD(, ieee80211_rm_oc_sched_policy_entry) sched_policy_entry_head;   
    ieee80211_rm_oc_sched_policy_entry_t next_scheduled_policy_entry;
    u_int32_t starting_tsf;
    u_int32_t ending_tsf;
    u_int32_t current_1shot_start_tsf;
};
typedef struct ieee80211_rm_oc_sched_run_q *ieee80211_rm_oc_sched_run_q_t;

struct ieee80211_rm_oc_sched_statistics {
    u_int32_t   scheduler_run_count;
    u_int32_t   tsf_timer_expire_count;
    u_int32_t   tsf_time_changed_count;
    u_int32_t   request_start_count;
    u_int32_t   request_stop_count;
    u_int32_t   vap_transition_count;
    u_int32_t   halt_count;
    u_int32_t   mlme_bmiss_count;
    u_int32_t   mlme_bmiss_clear_count;
    u_int32_t   one_shot_start_count;
    u_int32_t   noa_schedule_updates;
};
typedef struct ieee80211_rm_oc_sched_statistics ieee80211_rm_oc_sched_statistics_t;

struct ieee80211_rm_oc_sched_noa_update_cache {
    bool        cache_valid;
    u_int8_t    valid_schedules[IEEE80211_RESMGR_MAX_NOA_SCHEDULES];
    u_int32_t   duration_continuous;
    u_int32_t   interval_continuous;
    u_int32_t   duration_one_shot;
    u_int32_t   start_tsf_one_shot;
    u_int32_t   last_periodic_noa_update_tsf;
};
typedef struct ieee80211_rm_oc_sched_noa_update_cache ieee80211_rm_oc_sched_noa_update_cache_t;

struct ieee80211_resmgr_oc_scheduler {
    ieee80211_resmgr_oc_sched_lock_t                scheduler_lock;
    ieee80211_resmgr_t                              resmgr;
    ieee80211_resmgr_oc_sched_policy_t              policy;
    TAILQ_HEAD(, ieee80211_resmgr_oc_sched_req)     high_prio_list;
    TAILQ_HEAD(, ieee80211_resmgr_oc_sched_req)     low_prio_list;
    ieee80211_rm_oc_sched_timeslot_t                timeslot_base;
    TAILQ_HEAD(, ieee80211_rm_oc_sched_timeslot)    timeslot_free_q;
    u_int32_t                                       timeslots_inuse_count;
    u_int32_t                                       max_timeslots_inuse_count;
    ieee80211_rm_oc_sched_policy_entry_t            sched_policy_entry_base;
    TAILQ_HEAD(, ieee80211_rm_oc_sched_policy_entry) sched_policy_entry_free_q;
    u_int8_t                                        current_run_q;
    u_int32_t                                       current_tsf;
    u_int32_t                                       previous_tsf;
    u_int32_t                                       next_tsf_sched_ev;
    struct ieee80211_rm_oc_sched_run_q              run_queues[MAX_RUN_QUEUES];
    ieee80211_hsm_t                                 hsm_handle;                    /* HSM Handle */
    u_int32_t                                       current_req_id;
    ieee80211_resmgr_oc_sched_run_state             run_state;
    tsftimer_handle_t                               tsftimer_hdl;
    bool                                            transition_run_q;
    ieee80211_resmgr_oc_sched_req_t                 current_active_req;
    ieee80211_resmgr_oc_sched_state_t               scheduler_state;
    u_int32_t                                       device_pause_delay_usec;
    u_int32_t                                       swba_delay_before_tbtt_usec;
    TAILQ_HEAD(,ieee80211_resmgr_oc_sched_noa_event_vap)    noa_event_vap_list;
    u_int32_t                                       noa_event_scheduling_delay;
    ieee80211_rm_oc_sched_statistics_t              stats;
    ieee80211_rm_oc_sched_noa_update_cache_t        noa_update_cache;
};
typedef struct ieee80211_resmgr_oc_scheduler *ieee80211_resmgr_oc_scheduler_t;

/*
 * Resource Manager Off-Channel Scheduler is not compiled in by default.
 */ 
#ifndef UMAC_SUPPORT_RESMGR_OC_SCHED
#define UMAC_SUPPORT_RESMGR_OC_SCHED 0
#endif

#if UMAC_SUPPORT_RESMGR_OC_SCHED

/* Get the beacon interval in microseconds */
static INLINE 
u_int32_t get_beacon_interval(wlan_if_t vap) 
{
    /* The beacon interval is in terms of Time Units */
    return(vap->iv_bss->ni_intval * TIME_UNIT_TO_MICROSEC);
}

#define MIN_TSF_TIMER_TIME  5000 /* 5 msec */

/* Calculate the next TBTT time in terms of microseconds */
static INLINE 
u_int32_t get_next_tbtt_tsf_32(wlan_if_t vap) 
{
   struct ieee80211com *ic = vap->iv_ic;
   u_int64_t           curr_tsf64, nexttbtt_tsf64;
   u_int32_t           bintval; /* beacon interval in micro seconds */
   ieee80211_vap_tsf_offset    vap_tsf_offset;

   ieee80211_vap_get_tsf_offset(vap, &vap_tsf_offset);
   
   curr_tsf64 = ic->ic_get_TSF64(ic);

   /* Adjust or normalize the TSF to the native TSF of the VAP for the TBTT computation */
   if (vap_tsf_offset.offset_negative == true) {
        curr_tsf64 -= vap_tsf_offset.offset;
   }
   else {
        curr_tsf64 += vap_tsf_offset.offset;
   }
   
   /* calculate the next tbtt */ 

   bintval = get_beacon_interval(vap);
    
   nexttbtt_tsf64 =  curr_tsf64 + bintval;
   nexttbtt_tsf64  = nexttbtt_tsf64 - OS_MOD64_TBTT_OFFSET(nexttbtt_tsf64, bintval);
   if ((nexttbtt_tsf64 - curr_tsf64) < MIN_TSF_TIMER_TIME ) {  /* if the immediate next tbtt is too close go to next one */
       nexttbtt_tsf64 += bintval;
   }

   /* Adjust TSF back to the primary STA TSF value */
   if (vap_tsf_offset.offset_negative == true) {
        nexttbtt_tsf64 += vap_tsf_offset.offset;
   }
   else {
        nexttbtt_tsf64 -= vap_tsf_offset.offset;
   }
   
   return (u_int32_t) nexttbtt_tsf64;
}

static INLINE u_int32_t 
ieee80211_rm_ocs_msec_to_usec(
    u_int32_t val
    )
{
    return(val * 1000);
}

static INLINE bool
ieee80211_rm_ocs_pending_timeslots(    
    ieee80211_resmgr_oc_scheduler_t sched,
    u_int8_t run_q_ndx
    )
{
    return(! TAILQ_EMPTY(&(sched->run_queues[run_q_ndx].run_q_head)));
}

void
ieee80211_rm_ocs_mlme_event_handler(
    ieee80211_vap_t vap, 
    ieee80211_mlme_event *event, 
    void *arg);

void
ieee80211_rm_ocs_cleanup_request_lists(
    ieee80211_resmgr_oc_scheduler_t sched
    );
#ifdef SCHDEULER_TEST
void 
ieee80211_rm_ocs_scheduler_test(
    ieee80211_resmgr_t resmgr,
    u_int32_t prev_pending_timeslot_tsf
    );
#endif

void
ieee80211_rm_ocs_verify_schedule_window(
    ieee80211_resmgr_oc_scheduler_t sched   
    );

/*
 * Set next tsf timer value
 */
int
ieee80211_rm_ocs_set_next_tsftimer(
    ieee80211_resmgr_oc_scheduler_t sched,    
    struct ieee80211_resmgr_oc_sched_req **sched_req
    );

void
ieee80211_rm_ocs_tsf_timeout(
    tsftimer_handle_t h_tsftime, 
    int arg1, 
    void *arg2, 
    u_int32_t curr_tsf
    );

void
ieee80211_rm_ocs_tsf_changed(
    tsftimer_handle_t h_tsftime, 
    int arg1, 
    void *arg2, 
    u_int32_t curr_tsf
    );

/*
 * Allocate a scheduler timeslot element
 */
ieee80211_rm_oc_sched_timeslot_t 
ieee80211_rm_ocs_timeslot_alloc(
    ieee80211_resmgr_oc_scheduler_t sched
    );

/*
 * Release or free a previously allocated scheduler timeslot element
 */
void 
ieee80211_rm_ocs_timeslot_free(
    ieee80211_resmgr_oc_scheduler_t sched, 
    ieee80211_rm_oc_sched_timeslot_t ts
    );

/*
 * Get next tsf timer event
 */
int
ieee80211_rm_ocs_get_next_tsftimer(
    ieee80211_resmgr_oc_scheduler_t sched,
    u_int32_t *next_tsf,
    struct ieee80211_resmgr_oc_sched_req **sched_req
    );

/*
 * Compute the Notice-Of-Absence for each registered VAP
 */
void
ieee80211_rm_ocs_compute_notice_of_absence(
    ieee80211_resmgr_oc_scheduler_t sched, 
    ieee80211_rm_oc_sched_run_q_t run_q
    );

/*
 * Calculate a new schedule based on the latest off-channel air-time requests
 */
int 
ieee80211_rm_ocs_calc_schedule(
    ieee80211_resmgr_oc_scheduler_t sched,
    u_int8_t new_runq_ndx
    );

/*
 * Populate or fill-out a new scheduler run queue
 */
int 
ieee80211_rm_ocs_populate_high_prio_timeslots(
    ieee80211_resmgr_oc_scheduler_t sched, 
    ieee80211_rm_oc_sched_run_q_t run_q, 
    ieee80211_resmgr_oc_sched_req_t req
    );
/*
 * Calculate the scheduling delay needed in order
 * successfully notify all clients via the beacon IEs
 * of a new NOA schedule.
 */
u_int32_t
ieee80211_rm_ocs_compute_noa_scheduling_delay(
    ieee80211_resmgr_oc_scheduler_t sched,
    ieee80211_rm_oc_sched_run_q_t run_q,
    ieee80211_resmgr_oc_sched_req_index req_type
);

/*
 * Find the insertion point for a new timeslot object
 */
int 
ieee80211_rm_ocs_find_timeslot_insertion_pt(
    ieee80211_resmgr_oc_scheduler_t sched, 
    ieee80211_rm_oc_sched_run_q_t run_q, 
    ieee80211_rm_oc_sched_timeslot_t ts, 
    u_int32_t *new_tsf
    );

/*
 * Resolve a timeslot collision between two scheduling requests
 */
int 
ieee80211_rm_ocs_resolve_collision(
    ieee80211_resmgr_oc_scheduler_t sched, 
    ieee80211_rm_oc_sched_run_q_t run_q, 
    ieee80211_rm_oc_sched_timeslot_t new_ts, 
    ieee80211_rm_oc_sched_timeslot_t conflict_ts, 
    u_int32_t *new_tsf
    );

/*
 * Cleanup all of the colliding timeslots
 */
void 
ieee80211_rm_ocs_cleanup_collisions(
    ieee80211_resmgr_oc_scheduler_t sched,
    ieee80211_rm_oc_sched_run_q_t run_q,
    ieee80211_rm_oc_sched_timeslot_t new_ts,
    ieee80211_rm_oc_sched_timeslot_t conflict_ts
    );

/*
 * Collect all colliding timeslots onto a list
 */
void 
ieee80211_rm_ocs_find_all_collisions(
    ieee80211_resmgr_oc_scheduler_t sched,
    ieee80211_rm_oc_sched_run_q_t run_q,
    ieee80211_rm_oc_sched_timeslot_t new_ts,
    ieee80211_rm_oc_sched_timeslot_t inspt_ts,
    oc_sched_timeslot_head *collide_head
    );
    
/*
 * Insert a timeslot entry into the scheduler run queue
 */
int 
ieee80211_rm_ocs_insert_high_prio_timeslot(
    ieee80211_resmgr_oc_scheduler_t sched, 
    ieee80211_rm_oc_sched_run_q_t run_q,
    ieee80211_rm_oc_sched_timeslot_t new_ts, 
    ieee80211_rm_oc_sched_timeslot_t inspt_ts,
    ieee80211_rm_oc_sched_insert_pt_policy inspt_policy
    );

/*
 * Return the next scheduling policy entry
 */
ieee80211_rm_oc_sched_policy_entry_t 
ieee80211_rm_ocs_get_next_pe(
    ieee80211_resmgr_oc_scheduler_t sched, 
    ieee80211_rm_oc_sched_run_q_t run_q, 
    ieee80211_rm_oc_sched_policy_entry_t cur_pe
    );

/*
 * Save the colliding timeslot on the policy entry collision list
 */
void 
ieee80211_rm_ocs_insert_collision_list(
    ieee80211_resmgr_oc_scheduler_t sched, 
    ieee80211_rm_oc_sched_policy_entry_t cnflt_pe, 
    ieee80211_rm_oc_sched_timeslot_t conflict_ts
    );

/*
 * Selectively remove the appropriate timeslot from a policy entry collision list
 */
ieee80211_rm_oc_sched_timeslot_t 
ieee80211_rm_ocs_remove_collision_list(
    ieee80211_resmgr_oc_scheduler_t sched,
    ieee80211_rm_oc_sched_policy_entry_t sched_pe, 
    u_int32_t prev_stop_tsf, u_int32_t next_start_tsf
    );

/*
 * Select between conflicting timeslots using a round robin algorithm
 */
ieee80211_rm_oc_sched_timeslot_t 
ieee80211_rm_ocs_round_robin_scheduler(
    ieee80211_resmgr_oc_scheduler_t sched, 
    ieee80211_rm_oc_sched_run_q_t run_q, 
    ieee80211_rm_oc_sched_timeslot_t new_ts, 
    ieee80211_rm_oc_sched_timeslot_t conflict_ts
    );

/*
 * Select between conflicting timeslots using a Most Recently Used Throughput algorithm
 */
ieee80211_rm_oc_sched_timeslot_t 
ieee80211_rm_ocs_mru_thruput_scheduler(
    ieee80211_resmgr_oc_scheduler_t sched, 
    ieee80211_rm_oc_sched_run_q_t run_q, 
    ieee80211_rm_oc_sched_timeslot_t new_ts, 
    ieee80211_rm_oc_sched_timeslot_t conflict_ts
    );

/*
 * Print or dump the computed schedule
 */
void 
ieee80211_rm_ocs_print_schedule(
    ieee80211_resmgr_oc_scheduler_t sched, 
    int8_t run_q_ndx
    );

/*
 * Allocate a scheduler policy entry
 */
ieee80211_rm_oc_sched_policy_entry_t 
ieee80211_rm_ocs_policy_entry_alloc(
    ieee80211_resmgr_oc_scheduler_t sched
    );

/*
 * Release or free a previously allocated scheduler policy entry element
 */
void 
ieee80211_rm_ocs_policy_entry_free( 
    ieee80211_resmgr_oc_scheduler_t sched, 
    ieee80211_rm_oc_sched_policy_entry_t pe
    );

/*
 * Add a schedule policy entry to the run queue
 */
ieee80211_rm_oc_sched_policy_entry_t 
ieee80211_rm_ocs_add_pe(
    ieee80211_resmgr_oc_scheduler_t sched, 
    ieee80211_rm_oc_sched_run_q_t run_q,
    ieee80211_resmgr_oc_sched_req_t req
    );

/*
 * Find an existing schedule policy entry in the run queue
 */
ieee80211_rm_oc_sched_policy_entry_t 
ieee80211_rm_ocs_find_policy_entry( 
    ieee80211_resmgr_oc_scheduler_t sched,
    ieee80211_rm_oc_sched_run_q_t run_q, 
    ieee80211_resmgr_oc_sched_req_t req
    );

void 
ieee80211_rm_ocs_dump_timeslot(
    ieee80211_resmgr_oc_scheduler_t sched, 
    ieee80211_rm_oc_sched_timeslot_t ts
    );

/*
 * Populate one-shot and periodic low priority timeslots into new schedule
 */
int 
ieee80211_rm_ocs_populate_low_prio_timeslots(
    ieee80211_resmgr_oc_scheduler_t sched, 
    ieee80211_rm_oc_sched_run_q_t run_q
    );

/*
 * Can a request be scheduled for air-time
 */
bool 
ieee80211_rm_ocs_can_schedule_request(
    ieee80211_resmgr_oc_sched_req_t req
    );
    
/*
 * Insert a low priority non-periodic or one-shot request into schedule
 */
int
ieee80211_rm_ocs_insert_low_prio_one_shot_request(
    ieee80211_resmgr_oc_scheduler_t sched, 
    ieee80211_rm_oc_sched_run_q_t run_q,
    ieee80211_resmgr_oc_sched_req_t req
    );

/*
 * Walk a timeslot list and free each timeslot element
 */
void
ieee80211_rm_ocs_cleanup_timeslot_list(
    ieee80211_resmgr_oc_scheduler_t sched, 
    oc_sched_timeslot_head *timeslot_head
    );

void
ieee80211_rm_ocs_walk_sched_policy_list(
    ieee80211_resmgr_oc_scheduler_t sched, 
    ieee80211_rm_oc_sched_run_q_t run_q
    );

/*
 * Walk a schedule policy list and free each schedule policy element
 */
void
ieee80211_rm_ocs_cleanup_sched_policy_list(
    ieee80211_resmgr_oc_scheduler_t sched, 
    ieee80211_rm_oc_sched_run_q_t run_q
    );

/*
 * Get next low priority periodic request
 */
ieee80211_resmgr_oc_sched_req_t
ieee80211_rm_ocs_get_next_lowprio_periodic_request(
    ieee80211_resmgr_oc_scheduler_t sched, 
    ieee80211_rm_oc_sched_run_q_t run_q,
    ieee80211_resmgr_oc_sched_req_t cur_req
    );

/*
 * Fill in periodic low prority requests into appropriately sized timeslots
 */
int
ieee80211_rm_ocs_fill_in_avail_timeslots(
    ieee80211_resmgr_oc_scheduler_t sched, 
    ieee80211_rm_oc_sched_run_q_t run_q
    );

#else

#define ieee80211_resmgr_oc_scheduler_sm_create(sched)   (EOK)
#define ieee80211_resmgr_oc_scheduler_sm_delete(sched)    /**/
#define ieee80211_resmgr_oc_scheduler_sm_dispatch(sched,event,event_data_len,event_data)  (EINVAL) 

#endif /* UMAC_SUPPORT_RESMGR_OC_SCHED */

#endif /* _IEEE80211_RESMGR_SCHEDULER_PRIV_H_ */

