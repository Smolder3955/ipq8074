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


#ifndef _IEEE80211_RESMGR_OC_SCHEDULER_H_
#define _IEEE80211_RESMGR_OC_SCHEDULER_H_

typedef enum {
    OFF_CHAN_SCHED_POLICY_ROUND_ROBIN = 1,
    OFF_CHAN_SCHED_POLICY_MRU_THRUPUT,
    OFF_CHAN_SCHED_POLICY_MAX
} ieee80211_resmgr_oc_sched_policy_t;

typedef enum {
    OFF_CHAN_SCHED_PRIO_LOW = 1, /* Low priority scheduling request */
    OFF_CHAN_SCHED_PRIO_HIGH,    /* High priority scheduling request */
    OFF_CHAN_SCHED_PRIO_MAX      /* Maximum number of priorities */
} ieee80211_resmgr_oc_sched_prio_t;

struct ieee80211_resmgr_oc_sched_req;
typedef struct ieee80211_resmgr_oc_sched_req *ieee80211_resmgr_oc_sched_req_handle_t;


#if UMAC_SUPPORT_RESMGR_OC_SCHED

bool ieee80211_resmgr_oc_scheduler_is_active(ieee80211_resmgr_t resmgr);


#else

#define ieee80211_resmgr_oc_scheduler_is_active(resmgr)  false

#endif



#endif  /* _IEEE80211_RESMGR_OC_SCHEDULER_H_ */
