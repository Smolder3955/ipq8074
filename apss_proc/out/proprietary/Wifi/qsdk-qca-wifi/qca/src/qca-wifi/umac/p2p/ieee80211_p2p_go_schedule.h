/*
* Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
/*
 * module to determine when GO is present and absent given
 * different NOA schedules and CTWINDOW interface.
 * It also generates GO PRESENT/ ABSENT events when event 
 * GO changes its state.  
 */

#ifndef _IEEE80211_P2P_GO_SCHEDULE_

#define _IEEE80211_P2P_GO_SCHEDULE_

#include "ieee80211_p2p_proto.h"

struct ieee80211_p2p_go_schedule_t;
typedef struct ieee80211_p2p_go_schedule *ieee80211_p2p_go_schedule_t;

typedef enum _ieee80211_p2p_go_schedule_type {
    IEEE80211_P2P_GO_ABSENT_SCHEDULE,
    IEEE80211_P2P_GO_PRESENT_SCHEDULE
} ieee80211_p2p_go_schedule_type;

typedef enum _ieee80211_p2p_go_schedule_event_type {
    IEEE80211_P2P_GO_SCHEDULE_EVENT_GO_ABSENT,
    IEEE80211_P2P_GO_SCHEDULE_EVENT_GO_PRESENT
} ieee80211_p2p_go_schedule_event_type;

/* Maximum number of NOA schedules that we can process. */
#define P2P_GO_PS_MAX_NUM_SCHEDULE_REQ          4

typedef struct _ieee80211_p2p_go_schedule_req {
    ieee80211_p2p_go_schedule_type                type; /* type of the scedule */
    u_int8_t                                      pri;  /* priority :0 (highest) ,1,2 (lowest) */
    struct ieee80211_p2p_noa_descriptor noa_descriptor; /* scedule info */
} ieee80211_p2p_go_schedule_req;

typedef struct _ieee80211_p2p_go_schedule_event {
    ieee80211_p2p_go_schedule_event_type type;
} ieee80211_p2p_go_schedule_event;

/*
 * crete an instance of the p2p schedule module.
 */
ieee80211_p2p_go_schedule_t ieee80211_p2p_go_schedule_create(osdev_t os_handle, wlan_if_t vap);

/*
 * delete the p2p schedule module.
 */
int ieee80211_p2p_go_schedule_delete(ieee80211_p2p_go_schedule_t go_scheduler);

typedef void (*ieee80211_go_schedule_event_handler) (ieee80211_p2p_go_schedule_t go_schedule, 
                                                     ieee80211_p2p_go_schedule_event *event, void *arg,
                                                     bool one_shot_noa);

/*
 * register event handler with go scheduler.
 */
int ieee80211_p2p_go_schedule_register_event_handler(ieee80211_p2p_go_schedule_t go_schedule, 
                                                ieee80211_go_schedule_event_handler evhandler, void *Arg);
/*
 * unregister event handler with go scheduler.
 */
int ieee80211_p2p_go_schedule_unregister_event_handler(ieee80211_p2p_go_schedule_t go_schedule, 
                                                ieee80211_go_schedule_event_handler evhandler, void *Arg);

/*
 * set up p2p go schedules. will start generating the events.  
 * if there was an old schedule in place, it will restart with the new schedule.
 * if num_schdules is 0 then will stop.   
 */
int ieee80211_p2p_go_schedule_setup(ieee80211_p2p_go_schedule_t go_schedule,
                                    u_int8_t num_schedules, ieee80211_p2p_go_schedule_req *schedules );

/*
 * pause P2p GO scheduler. the scheduler should cancel all timers and pause.
 */
void ieee80211_p2p_go_schedule_pause(ieee80211_p2p_go_schedule_t go_schedule);

/*
 * unpause P2p GO scheduler. the scheduler should restart all the timers.
 * it should also generate a go event idicating the current state of the GO.
 */
void ieee80211_p2p_go_schedule_unpause(ieee80211_p2p_go_schedule_t go_schedule);

#endif /* _IEEE80211_P2P_GO_SCHEDULE_ */
