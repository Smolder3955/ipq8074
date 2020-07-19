/*
 * Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * Copyright (c) 2010, Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */

#include "ieee80211_var.h"
#include <ieee80211_channel.h>
#include "ieee80211_sme_api.h"
#include "ieee80211_sm.h"
#include <ieee80211_resmgr_priv.h>

/* State transition table
------------------------------------------------------------------------------------------------------------------------------------------------
EVENTS                                                                  CURRENT STATE ->

                        IDLE         VAP_PRESTART     VAP_STARTING    BSSCHAN        CAN_PAUSE      PAUSING       OFFCHAN        RESUMING 
------------------------------------------------------------------------------------------------------------------------------------------------
VAP_START_REQUEST    VAP_PRESTART /       X                X        VAP_PRESTART     BSSCHAN         STORE        RESUMING        STORE
                     VAP_STARTING
VAP_UP                    X               X             BSSCHAN          X              X              X             X               X
VAP_STOPPED               X       VAP_STARTING/IDLE        X        BSSCHAN/IDLE  CAN_PAUSE/IDLE   PAUSING/IDLE  OFFCHAN/IDLE   RESUMING/IDLE
OFFCHAN_REQUEST        OFFCHAN          STORE            STORE       CAN_PAUSE         X               X             X               X
VAP_PAUSE_COMPLETE        X          VAP_STARTING          X             X             X            OFFCHAN          X               X
BSSCHAN_REQUEST           X             STORE            STORE           X             X        RESUMING/BSSCHAN BSSCHAN/RESUMING  RESUMING
VAP_RESUME_COMPLETE       X               X             BSSCHAN          X             X               X             X             BSSCHAN
VAP_CANPAUSE_TIMER        X               X                X             X       CAN_PAUSE/PAUSING     X             X               X
------------------------------------------------------------------------------------------------------------------------------------------------ 
*/

#if UMAC_SUPPORT_RESMGR_SM

static const char* resmgr_event_name[] = {
    /* IEEE80211_RESMGR_EVENT_NONE                */                 "NONE",
    /* IEEE80211_RESMGR_EVENT_VAP_START_REQUEST   */    "VAP_START_REQUEST",
    /* IEEE80211_RESMGR_EVENT_VAP_UP              */               "VAP_UP",
    /* IEEE80211_RESMGR_EVENT_VAP_STOPPED         */          "VAP_STOPPED",
    /* IEEE80211_RESMGR_EVENT_VAP_PAUSE_COMPLETE  */   "VAP_PAUSE_COMPLETE",
    /* IEEE80211_RESMGR_EVENT_VAP_PAUSE_FAIL      */       "VAP_PAUSE_FAIL",
    /* IEEE80211_RESMGR_EVENT_VAP_RESUME_COMPLETE */  "VAP_RESUME_COMPLETE",
    /* IEEE80211_RESMGR_EVENT_OFFCHAN_REQUEST     */      "OFFCHAN_REQUEST",
    /* IEEE80211_RESMGR_EVENT_BSSCHAN_REQUEST     */      "BSSCHAN_REQUEST",
    /* IEEE80211_RESMGR_EVENT_VAP_CANPAUSE_TIMER  */   "VAP_CANPAUSE_TIMER",
    /* IEEE80211_RESMGR_EVENT_CHAN_SWITCH_REQUEST */  "CHAN_SWITCH_REQUEST",
    /* IEEE80211_RESMGR_EVENT_CSA_COMPLETE        */         "CSA_COMPLETE",
    /* IEEE80211_RESMGR_EVENT_SCHED_BSS_REQUEST   */    "SCHED_BSS_REQUEST",
    /* IEEE80211_RESMGR_EVENT_TSF_TIMER_EXPIRED   */    "TSF_TIMER_EXPIRED",
    /* IEEE80211_RESMGR_EVENT_TSF_TIMER_CHANGED   */    "TSF_TIMER_CHANGED",
    /* IEEE80211_RESMGR_EVENT_OFF_CHAN_SCHED_START_REQUEST */   "OC_SCHED_START_REQUEST",
    /* IEEE80211_RESMGR_EVENT_OFF_CHAN_SCHED_STOP_REQUEST */    "OC_SCHED_STOP_REQUEST",
    /* IEEE80211_RESMGR_EVENT_OFF_CHAN_SCHED_SWITCH_CHAN */    "OC_SCHED_SWITCH_CHANNEL",
    /* IEEE80211_RESMGR_EVENT_VAP_AUTH_COMPLETE   */    "VAP_AUTH_COMPLETE",
};

#if 0
static void ieee80211_vap_iter_start_scheduling_vaps(void *arg, struct ieee80211vap *vap);
static void ieee80211_resmgr_oc_scheduler_change_state(ieee80211_resmgr_t resmgr, bool state);
#endif

static bool ieee80211_resmgr_needs_scheduler(ieee80211_resmgr_t resmgr,u_int16_t event_type, ieee80211_resmgr_sm_event_t event);
static bool ieee80211_resmgr_off_chan_scheduler_bsschan_event_handler(ieee80211_resmgr_t resmgr,
                                         u_int16_t event_type, u_int16_t event_data_len, void *event_data); 
static void ieee80211_resmgr_state_chanswitch_entry(void *ctx); 
static bool ieee80211_resmgr_state_chanswitch_event(void *ctx, u_int16_t event_type, u_int16_t event_data_len, void *event_data); 
static void ieee80211_resmgr_state_oc_chanswitch_entry(void *ctx); 
static bool ieee80211_resmgr_state_oc_chanswitch_event(void *ctx, u_int16_t event_type, 
             u_int16_t event_data_len, void *event_data);
static void ieee80211_resmgr_state_oc_chanswitch_exit(void *ctx); 
struct ieee80211_ath_channel* find_proper_chan(struct ieee80211com *ic, struct ieee80211_ath_channel *c1, struct ieee80211_ath_channel *c2);
static int _ieee80211_resmgr_setmode(ieee80211_resmgr_t resmgr, ieee80211_resmgr_mode mode);
static ieee80211_resmgr_mode _ieee80211_resmgr_getmode(ieee80211_resmgr_t resmgr);
static void ieee80211_vap_iter_get_vap_live(void *arg, struct ieee80211vap *vap);

#define MAX_QUEUED_EVENTS  16

#define IS_CHAN_EQUAL(_c1, _c2)  (((_c1)->ic_freq == (_c2)->ic_freq) && \
                                  (((_c1)->ic_flags & IEEE80211_CHAN_ALLTURBO) == ((_c2)->ic_flags & IEEE80211_CHAN_ALLTURBO)))

#define IS_FREQ_EQUAL(_c1, _c2)  ((_c1)->ic_freq == (_c2)->ic_freq) 

static void ieee80211_resmgr_sm_debug_print (void *ctx, const char *fmt,...) 
{
    char tmp_buf[256];
    va_list ap;
    ieee80211_resmgr_t resmgr = (ieee80211_resmgr_t) ctx;
     
    va_start(ap, fmt);
    vsnprintf (tmp_buf,256, fmt, ap);
    va_end(ap);
    IEEE80211_DPRINTF_IC(resmgr->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                         "%s",tmp_buf);
}

static void ieee80211_notify_vap_start_complete(ieee80211_resmgr_t resmgr,
                             struct ieee80211vap *vap,  ieee80211_resmgr_notification_status status)
{
    ieee80211_resmgr_notification notif;

    /* Intimate start completion to VAP module */
    notif.type = IEEE80211_RESMGR_VAP_START_COMPLETE;
    notif.req_id = IEEE80211_RESMGR_REQID;
    notif.status = status;
    notif.vap = vap;
    vap->iv_rescan = 0;
    IEEE80211_RESMGR_NOTIFICATION(resmgr, &notif);
}

static bool is_chan_superset(struct ieee80211_ath_channel *c1, struct ieee80211_ath_channel *c2)
{

    /* 11AX TODO - Add 11ax processing if required in the future. Currently, 11ac
     * too is not processed.
     */

    if (IEEE80211_IS_CHAN_11N(c1) && !IEEE80211_IS_CHAN_11N(c2)) {
        return true;
    }
    
    if (IEEE80211_IS_CHAN_11N_HT40(c1) && !IEEE80211_IS_CHAN_11N_HT40(c2)) {
        return true;
    }
    
    return false;
}


/*
* called when new vap is starting.
* make sure that with the new vap coming up
* the vap cobination is supported. if supported return true, if not
* return false. also adjust the beacon interval if a new  AP vap is coming up
* while a sta vap is already up.
*/ 
static bool  ieee80211_resmgr_handle_vap_start_request(ieee80211_resmgr_t resmgr, 
                                            ieee80211_vap_t starting_vap)
{
   struct vap_iter_check_state_params params;

   struct ieee80211com *ic = resmgr->ic;
   ieee80211_resmgr_vap_priv_t resmgr_starting_vap = NULL;

    if (!starting_vap) {
        return true;
    }

   resmgr_starting_vap = ieee80211vap_get_resmgr(starting_vap);

   resmgr_starting_vap->state = VAP_STARTING;
   OS_MEMZERO(&params, sizeof(struct vap_iter_check_state_params));
   wlan_iterate_vap_list(ic, ieee80211_vap_iter_check_state, &params);

   /* 
    * for now only one IBSS vap is allowed, when one IBSS vap is active no othr  VAP is allowed
    * to be active including a new IBSS vap, if atleast one non IBSS VAP is active a new IBSS vap 
    * not allowed to become active.
    * this may be allowed with new chipsets.
    */ 

    if (params.num_ibss_vaps_active && (params.num_ap_vaps_active || 
                                               params.num_sta_vaps_active )) {
        resmgr_starting_vap->state = VAP_STOPPED;
        ieee80211_notify_vap_start_complete(resmgr, starting_vap, IEEE80211_RESMGR_STATUS_NOT_SUPPORTED);
        IEEE80211_DPRINTF_IC(resmgr->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                                 "%s: can not bringup vap-%d,  active ibss vaps %d ap vaps %d sta vaps %d \n ",__func__,
                                 starting_vap->iv_unit, 
                                 params.num_ibss_vaps_active, params.num_ap_vaps_active, params.num_sta_vaps_active ); 
        return false;
    } 

    if (params.num_ibss_vaps_active > 1 ) {
        resmgr_starting_vap->state = VAP_STOPPED;
        IEEE80211_DPRINTF_IC(resmgr->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                                 "%s: can not bringup vap %d active ibss vaps %d ap vaps %d sta vaps %d \n ",__func__,
                                 starting_vap->iv_unit, 
                                 params.num_ibss_vaps_active, params.num_ap_vaps_active, params.num_sta_vaps_active ); 
        ieee80211_notify_vap_start_complete(resmgr, starting_vap, IEEE80211_RESMGR_STATUS_NOT_SUPPORTED);
        return false;
    } 

    resmgr_starting_vap->state = VAP_STOPPED;

    /*
     * if the starting vap is an AP/BT amp vap and another VAP already active, then
     * check and set the becaon interval of the starting vap as same as  the alreay active
     * vap. our HW can not support different beacon intervals for different vaps on the same radio.
     */
    if ( (starting_vap->iv_opmode == IEEE80211_M_BTAMP || starting_vap->iv_opmode == IEEE80211_M_HOSTAP) && 
                (params.num_ap_vaps_active || params.num_sta_vaps_active )) {
         ieee80211_vap_t live_vap = NULL;
         wlan_iterate_vap_list(ic, ieee80211_vap_iter_get_vap_live, &live_vap);
         if (live_vap && (ieee80211_vap_get_beacon_interval(live_vap) != ieee80211_vap_get_beacon_interval(starting_vap) ) )  {
               /* 
                * Beacon interval of starting AP/BT AMP vap is different from the an exisitng/active VAP, so force
                *  the beacon interval of the starting vap to be the same as that of the existing/active vap.
                */
               IEEE80211_DPRINTF_IC(resmgr->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                                 "%s: forcing the  vap %d beacon interval to %d (to match with vap %d) from original value of %d \n ",__func__,
                                 starting_vap->iv_unit, ieee80211_vap_get_beacon_interval(live_vap) , live_vap->iv_unit,
                                 ieee80211_vap_get_beacon_interval(starting_vap) );
                ieee80211_vap_set_beacon_interval(starting_vap, ieee80211_vap_get_beacon_interval(live_vap)); 
         }
     }
    return true;
}

/* VAP Iteration Routines */

/* Get VAP deferred events (if any) */
static void ieee80211_vap_iter_get_def_event(void *arg, struct ieee80211vap *vap)
{
    ieee80211_vap_t *event_vap = (ieee80211_vap_t *)arg;
    ieee80211_resmgr_vap_priv_t resmgr_vap = NULL;

    if ((*event_vap) == NULL) {
        resmgr_vap = ieee80211vap_get_resmgr(vap);
        if (resmgr_vap->def_event_type) {
            *event_vap = vap;
         }
    }
}

/* Send Pause Request to all active VAPs */
static void ieee80211_vap_iter_pause_request(void *arg, struct ieee80211vap *vap)
{
    ieee80211_resmgr_vap_priv_t resmgr_vap = ieee80211vap_get_resmgr(vap);

    if (resmgr_vap->state == VAP_ACTIVE) {
        ieee80211_vap_pause(vap, IEEE80211_RESMGR_REQID);
    }
}

static void ieee80211_vap_iter_standby_request(void *arg, struct ieee80211vap *vap)
{
    ieee80211_resmgr_vap_priv_t resmgr_vap = ieee80211vap_get_resmgr(vap);

    if (resmgr_vap->state == VAP_ACTIVE) {
        ieee80211_vap_standby_bss(vap);

        if (vap->iv_opmode == IEEE80211_M_BTAMP) {
            /* AMP Port goes down during standby operation. However, it still needs a resume notification.
             * Set a special flag for this case.
             */
            resmgr_vap->resume_notify = 1;
        }
    }
}

struct vap_iter_unpause_params {
    struct ieee80211_ath_channel *chan;
    bool vaps_unpausing;
};

/* Send Unpause Request to all paused VAPs, on matching channel if required */
static void ieee80211_vap_iter_unpause_request(void *arg, struct ieee80211vap *vap)
{
    ieee80211_resmgr_vap_priv_t resmgr_vap = ieee80211vap_get_resmgr(vap);
    struct vap_iter_unpause_params *params = (struct vap_iter_unpause_params *)arg;

    if (resmgr_vap->state == VAP_PAUSED) {
        if (!params || !params->chan ||
            ((NULL != resmgr_vap->chan) && params->chan->ic_freq == resmgr_vap->chan->ic_freq)) {
            ieee80211_vap_unpause(vap, IEEE80211_RESMGR_REQID);
            ieee80211_resgmr_lmac_vap_pause_control(vap, false);
            if (params) {
                params->vaps_unpausing = true;
            }
        }
    }
}

static void ieee80211_vap_iter_resume_request(void *arg, struct ieee80211vap *vap)
{
    ieee80211_resmgr_vap_priv_t resmgr_vap = ieee80211vap_get_resmgr(vap);
    struct vap_iter_unpause_params *params = (struct vap_iter_unpause_params *)arg;

    if (resmgr_vap->state == VAP_PAUSED) {
        if (!params || !params->chan ||
            ((resmgr_vap->chan != NULL) && params->chan->ic_freq == resmgr_vap->chan->ic_freq)) {
            vap->iv_bsschan = resmgr_vap->chan;
            ieee80211_vap_resume_bss(vap);
            if (params) {
                params->vaps_unpausing = true;
            }
        }
    } else if (resmgr_vap->resume_notify) {
        ieee80211_vap_resume_bss(vap);
        resmgr_vap->resume_notify = 0;
    }
}

/* Send Channel Switch Request to all active VAPs on different channel */
static void ieee80211_vap_iter_chanswitch_request(void *arg, struct ieee80211vap *vap)
{
    ieee80211_resmgr_vap_priv_t resmgr_vap = ieee80211vap_get_resmgr(vap);
    struct ieee80211_ath_channel *chan = (struct ieee80211_ath_channel *)arg;

    if (resmgr_vap->state == VAP_ACTIVE &&
        (chan->ic_freq != resmgr_vap->chan->ic_freq)) {
        //ieee80211_vap_switch_chan(vap, chan, IEEE80211_RESMGR_REQID);
        //resmgr_vap->state = VAP_SWITCHING;
        resmgr_vap->chan = chan;
    }
}

/* Pick BSS channel */
static void ieee80211_vap_iter_bss_channel(void *arg, struct ieee80211vap *vap)
{
    ieee80211_resmgr_vap_priv_t resmgr_vap = ieee80211vap_get_resmgr(vap);
    struct ieee80211_ath_channel **chan = (struct ieee80211_ath_channel **)arg;

    if (resmgr_vap->state == VAP_PAUSED) {
        if (*chan) {
            /* Channel already picked up from first VAP, update if current VAP has same channel
             * frequency and a wider PHY mode.
             */
            if (IS_FREQ_EQUAL(resmgr_vap->chan, (*chan)) &&
                is_chan_superset(resmgr_vap->chan, (*chan))) {
                *chan = resmgr_vap->chan;
            }
        } else {
            *chan = resmgr_vap->chan;
        }
    }
}

/* Check if all VAPs can pause */
static void ieee80211_vap_iter_canpause(void *arg, struct ieee80211vap *vap)
{
    bool *can_pause = (bool *)arg;

    if ((*can_pause) && (ieee80211_vap_can_pause(vap) == false)) {
        *can_pause = false;
    }
}

/* Find the starting vap */
static void ieee80211_vap_iter_get_vap_starting(void *arg, struct ieee80211vap *vap)
{
    ieee80211_vap_t *starting_vap = (ieee80211_vap_t *)arg;
    ieee80211_resmgr_vap_priv_t resmgr_vap  = NULL;

    if (!(*starting_vap)) {
        resmgr_vap = ieee80211vap_get_resmgr(vap);
        if (resmgr_vap->state == VAP_STARTING) {
            *starting_vap = vap;
         }
    }
}

/* Find the switching vap */
static void ieee80211_vap_iter_get_vap_switching(void *arg, struct ieee80211vap *vap)
{
    ieee80211_vap_t *switching_vap = (ieee80211_vap_t *)arg;
    ieee80211_resmgr_vap_priv_t resmgr_vap  = NULL;

    if (!(*switching_vap)) {
        resmgr_vap = ieee80211vap_get_resmgr(vap);
        if (resmgr_vap->state == VAP_SWITCHING) {
            *switching_vap = vap;
         }
    }
}

/*
 * Use the chan frequency and flags from c1 and "add some flag from c2" to find a proper chan
 * In most of cases, c1 is the chan from STA(honored) VAP and c2 is the chan from other VAPs which is switching
 * to the chan frequency of honored VAP.
 */
struct ieee80211_ath_channel* find_proper_chan(struct ieee80211com *ic, struct ieee80211_ath_channel *c1, struct ieee80211_ath_channel *c2)
{
#define CWMASK  (IEEE80211_CHAN_HT40MINUS | IEEE80211_CHAN_HT40PLUS | IEEE80211_CHAN_HT20 | IEEE80211_CHAN_OFDM)
    struct ieee80211_ath_channel *channel = NULL;
    int counter = 0;
    u_int64_t c1flags = c1->ic_flags;
    u_int64_t c2flags = c2->ic_flags;
    u_int32_t addflag;

    /* 11AX TODO - Add 11ax processing here if required. Currently, 11ac too is
     * not processed.
     */

    //Igonre the channel width of c1, but keep other info(5GHz, passive, radar, HT40 intolerant and etc)
    c1flags &= ~CWMASK;

    //Check if the original channel width of c2 can run on c1 frequency
    addflag = c2flags & CWMASK;
    channel = ieee80211_find_channel(ic, c1->ic_freq, c1->ic_vhtop_ch_freq_seg2, c1flags | addflag);

    while (channel == NULL) {
        //find alternate phymode
        if ((c2flags & (IEEE80211_CHAN_HT40PLUS | IEEE80211_CHAN_HT40MINUS)) != 0) {
            if (counter == 0){
                //First time, switch (HT40+ to HT40-) or (HT40- to HT40+)
                addflag = c2flags & (IEEE80211_CHAN_HT40PLUS | IEEE80211_CHAN_HT40MINUS);
                addflag ^= (IEEE80211_CHAN_HT40PLUS | IEEE80211_CHAN_HT40MINUS);
                counter++;
            }
            else {
                //Second time, downgrade HT40 to HT20
                addflag = IEEE80211_CHAN_HT20;
                //Don't modify c2->ic_flags, but it's ok to modify c2flags
                c2flags &= ~IEEE80211_CHAN_HT40PLUS;
                c2flags &= ~IEEE80211_CHAN_HT40MINUS;
                c2flags |= IEEE80211_CHAN_HT20;
            }
        }
        else if ((c2flags & IEEE80211_CHAN_HT20) != 0) {
            //downgrade HT20 to OFDM
            addflag = IEEE80211_CHAN_OFDM;
            //Don't modify c2->ic_flags, but it's ok to modify c2flags
            c2flags &= ~IEEE80211_CHAN_HT20;
            c2flags |= IEEE80211_CHAN_OFDM;
        }
        else
            break;

        channel = ieee80211_find_channel(ic, c1->ic_freq, c1->ic_vhtop_ch_freq_seg2, c1flags | addflag);
    }

    return channel;
}

/* Event Deferring and Processing */
static bool ieee80211_resmgr_defer_event(ieee80211_resmgr_t resmgr,
                                  ieee80211_resmgr_vap_priv_t resmgr_vap,
                                  ieee80211_resmgr_sm_event_t event,
                                  u_int16_t event_type)
{
    if (resmgr_vap) {
        /* VAP related event */
        if (!resmgr_vap->def_event_type) {
            if (event->vap) {
                event->ni_bss_node=ieee80211_try_ref_bss_node(event->vap);
            }
            OS_MEMCPY(&resmgr_vap->def_event, event, sizeof(struct ieee80211_resmgr_sm_event));
            resmgr_vap->def_event_type = event_type;
            IEEE80211_DPRINTF_IC(resmgr->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                                 "%s: defer event %d vap %d \n ",__func__,event_type,event->vap?event->vap->iv_unit:-1 );
        }
    } else {
        /* SCAN related event */
        if ((resmgr->scandata.def_event_type == IEEE80211_RESMGR_EVENT_OFFCHAN_REQUEST) &&
            (event_type == IEEE80211_RESMGR_EVENT_BSSCHAN_REQUEST)) {
            if (resmgr->scandata.def_event.ni_bss_node) {
                ieee80211_free_node(resmgr->scandata.def_event.ni_bss_node);
            }
            OS_MEMZERO(&resmgr->scandata.def_event, sizeof(struct ieee80211_resmgr_sm_event));
            resmgr->scandata.def_event_type = 0;
            IEEE80211_DPRINTF_IC(resmgr->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                                 "%s: clear deferred OFFCHAN Request \n ",__func__);
            return false;

        } else if (!resmgr->scandata.def_event_type) {
            OS_MEMCPY(&resmgr->scandata.def_event, event, sizeof(struct ieee80211_resmgr_sm_event));
            resmgr->scandata.def_event_type = event_type;
            if (event->vap) {
                event->ni_bss_node=ieee80211_try_ref_bss_node(event->vap);
            }
            IEEE80211_DPRINTF_IC(resmgr->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                                 "%s: defer event %d \n ",__func__,event_type);
        }
    }

    return true;
}

static void ieee80211_resmgr_clear_event(ieee80211_resmgr_t resmgr,
                                          ieee80211_resmgr_vap_priv_t resmgr_vap)
{
    if (resmgr_vap) {
        /* VAP related event */
       if (resmgr_vap->def_event.ni_bss_node) {
           ieee80211_free_node(resmgr_vap->def_event.ni_bss_node);
       }
       IEEE80211_DPRINTF_IC(resmgr->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                                 "%s: clear deferred event %d \n ",__func__,resmgr_vap->def_event_type);
        OS_MEMZERO(&resmgr_vap->def_event, sizeof(struct ieee80211_resmgr_sm_event));
        resmgr_vap->def_event_type = 0;
    } else {
       if (resmgr->scandata.def_event.ni_bss_node) {
           ieee80211_free_node(resmgr->scandata.def_event.ni_bss_node);
       }
        OS_MEMZERO(&resmgr->scandata.def_event, sizeof(struct ieee80211_resmgr_sm_event));
        IEEE80211_DPRINTF_IC(resmgr->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                                 "%s: clear deferred event %d \n ",__func__,resmgr->scandata.def_event_type);
        resmgr->scandata.def_event_type = 0;
    }
}

static void
ieee80211_resmgr_process_deferred_events(ieee80211_resmgr_t resmgr)
{
    ieee80211_vap_t vap = NULL;
    ieee80211_resmgr_sm_event_t event = NULL;
    u_int16_t event_type = 0;
    ieee80211_resmgr_vap_priv_t resmgr_vap = NULL;
    struct ieee80211com *ic = resmgr->ic;
    bool dispatch_event = false;

    /*
     * This is a stable state, process deferred events if any ... 
     * Preference goes to deferred events from VAP first and then to SCAn
     */
    wlan_iterate_vap_list(ic, ieee80211_vap_iter_get_def_event, &vap);
    if (vap) {
        resmgr_vap = ieee80211vap_get_resmgr(vap);
        event = &resmgr_vap->def_event;
        event_type  = resmgr_vap->def_event_type;
    }

    if (event_type) {
        switch(event_type) {
        case IEEE80211_RESMGR_EVENT_VAP_START_REQUEST:
        case IEEE80211_RESMGR_EVENT_CHAN_SWITCH_REQUEST:
        {
            dispatch_event = true;
            break;
        }

        default:
            break;
        }

        if (dispatch_event) {
			struct ieee80211_node* save_bss_node = event->ni_bss_node;
            ieee80211_resmgr_sm_dispatch(resmgr, event_type,event);
			event->ni_bss_node = save_bss_node;
            dispatch_event = false;
        }

        /* Clear deferred event */
        ieee80211_resmgr_clear_event(resmgr, resmgr_vap);
    }

    if (resmgr->scandata.def_event_type) {    
        event = &resmgr->scandata.def_event;
        event_type = resmgr->scandata.def_event_type;

        switch(event_type) {
        case IEEE80211_RESMGR_EVENT_OFFCHAN_REQUEST:
        case IEEE80211_RESMGR_EVENT_BSSCHAN_REQUEST:
            dispatch_event = true;
            break;

        default:
            break;
        }

        if (dispatch_event) {
            IEEE80211_DPRINTF_IC(resmgr->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                                 "%s: dispatch deferred event %d \n ",__func__,event_type);
            ieee80211_resmgr_sm_dispatch(resmgr, event_type, event);
        }
        ieee80211_resmgr_clear_event(resmgr, NULL);
    }
}


static void
ieee80211_resmgr_offchan_common_handler(ieee80211_resmgr_t resmgr, struct ieee80211_ath_channel *chan)
{
    struct ieee80211com *ic = resmgr->ic;
    ieee80211_resmgr_notification notif;            

    /* Set required channel */
    ic->ic_curchan = chan;
#ifdef SLOW_ANT_DIV
        ic->ic_antenna_diversity_suspend(ic);
#endif
    ic->ic_scan_start(ic);
    ic->ic_set_channel(ic);

    /* Intimate request completion to SCAN module */
    notif.type = IEEE80211_RESMGR_OFFCHAN_SWITCH_COMPLETE;
    notif.req_id = IEEE80211_RESMGR_REQID;
    notif.status = EOK;
    notif.vap = NULL;
    IEEE80211_RESMGR_NOTIFICATION(resmgr, &notif);
}

static void
ieee80211_resmgr_chanswitch_common_handler(ieee80211_resmgr_t resmgr,
                                           ieee80211_vap_t vap,
                                           struct ieee80211_ath_channel *chan,
                                           bool change_state)
{
    ieee80211_resmgr_vap_priv_t         resmgr_vap = ieee80211vap_get_resmgr(vap);
    struct ieee80211com                 *ic = resmgr->ic;
    struct vap_iter_check_state_params  params;
    ieee80211_resmgr_notification       notif;
    int                                 status;

    OS_MEMZERO(&params, sizeof(struct vap_iter_check_state_params));
    wlan_iterate_vap_list(ic, ieee80211_vap_iter_check_state, &params);

    if (params.vaps_active == 1) {
        /* If this is the only VAP active in the system, the channel can be switched right away */

        /* Set desired channel */
        status = ieee80211_set_channel(ic, chan);

        /* Update new channel setting */
        resmgr_vap->chan = chan;
        vap->iv_bsschan = chan;

        /* Intimate request completion */
        notif.type = IEEE80211_RESMGR_CHAN_SWITCH_COMPLETE;
        notif.req_id = IEEE80211_RESMGR_REQID;
        notif.status = status;
        notif.vap = vap;
        IEEE80211_RESMGR_NOTIFICATION(resmgr, &notif);

        if (change_state) {
            ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_BSSCHAN);
        }
    } else if (params.vaps_active > 1) {

        /* Multiple VAPs active in the system, other VAPs on the current channel need to be paused */
        resmgr_vap->state = VAP_SWITCHING;
        resmgr_vap->chan = chan;

        /* Single Channel Operation - Switch other vaps to STA VAP channel. */
        if (vap->iv_opmode == IEEE80211_M_STA) {
            wlan_iterate_vap_list(ic, ieee80211_vap_iter_chanswitch_request, resmgr_vap->chan);
        }
        ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_CHANSWITCH);
    }
}

/* 
 * different state related functions.
 */
/*
 * State IDLE
 */
static void ieee80211_resmgr_state_idle_entry(void *ctx) 
{
    ieee80211_resmgr_t resmgr = (ieee80211_resmgr_t) ctx;

    /* This is a stable state, process deferred events if any ...  */
    ieee80211_resmgr_process_deferred_events(resmgr);
}

static bool ieee80211_resmgr_state_idle_event(void *ctx, u_int16_t event_type, u_int16_t event_data_len, void *event_data) 
{
    ieee80211_resmgr_t resmgr = (ieee80211_resmgr_t)ctx;
    ieee80211_resmgr_sm_event_t event = (ieee80211_resmgr_sm_event_t)event_data;
    ieee80211_vap_t vap = event->vap;
    ieee80211_resmgr_vap_priv_t resmgr_vap = NULL;
    bool retVal = true;
    struct ieee80211com *ic = resmgr->ic;

    if (vap) {
        resmgr_vap = ieee80211vap_get_resmgr(vap);
    }

    switch (event_type) {
    case IEEE80211_RESMGR_EVENT_VAP_START_REQUEST:
    {
        ieee80211_resmgr_notification notif;

        if (!resmgr_vap)
            break;

        resmgr->current_scheduled_operation.operation_type = OC_SCHED_OP_VAP;
        resmgr->current_scheduled_operation.vap = vap;

        /* Abort Scan operation if it is in progress */ 
        if (resmgr->scandata.chan) {
            notif.type = IEEE80211_RESMGR_OFFCHAN_ABORTED;
            notif.req_id = IEEE80211_RESMGR_REQID;
            notif.status = 0;
            notif.vap = NULL;
            IEEE80211_RESMGR_NOTIFICATION(resmgr, &notif);
        }

        resmgr_vap->state = VAP_STARTING;
        resmgr_vap->chan = event->chan;
        ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_VAP_STARTING);
        break;
    }

    case IEEE80211_RESMGR_EVENT_BSSCHAN_REQUEST:
    {
        ieee80211_resmgr_notification notif;

        /* Set home channel */
        if (resmgr->scandata.chan) {
            ic->ic_curchan = resmgr->scandata.chan;
            ic->ic_set_channel(ic);
        }
        resmgr->scandata.chan = NULL;

        /* Intimate request completion */
        notif.type = IEEE80211_RESMGR_BSSCHAN_SWITCH_COMPLETE;
        notif.req_id = IEEE80211_RESMGR_REQID;
        notif.status = 0;
        notif.vap = NULL;
        IEEE80211_RESMGR_NOTIFICATION(resmgr, &notif);
        break;
    }

    case IEEE80211_RESMGR_EVENT_OFFCHAN_REQUEST:
    {
        /* Store current channel as home channel */
        resmgr->scandata.chan = ic->ic_curchan;

        /* Set desired channel and complete request */
        ieee80211_resmgr_offchan_common_handler(resmgr, event->chan);
        break;
    }

    case IEEE80211_RESMGR_EVENT_TSF_TIMER_EXPIRED:
    case IEEE80211_RESMGR_EVENT_TSF_TIMER_CHANGED:
    case IEEE80211_RESMGR_EVENT_OFF_CHAN_SCHED_START_REQUEST:
    case IEEE80211_RESMGR_EVENT_OFF_CHAN_SCHED_STOP_REQUEST:
        ieee80211_resmgr_off_chan_scheduler(resmgr, event_type);
        break;
        
    default:
        retVal = false;
        break;
    }
    if (event->ni_bss_node) {
           ieee80211_free_node(event->ni_bss_node);
    }

    return retVal;
}

/*
 * State VAP_PRESTART
 */
static void ieee80211_resmgr_state_vapprestart_entry(void *ctx) 
{
    ieee80211_resmgr_t resmgr = (ieee80211_resmgr_t)ctx;
    struct ieee80211com *ic = resmgr->ic;

    /* Send Pause request to all active VAPs */
    wlan_iterate_vap_list(ic, ieee80211_vap_iter_standby_request, NULL);
}

static bool ieee80211_resmgr_state_vapprestart_event(void *ctx, u_int16_t event_type, u_int16_t event_data_len, void *event_data) 
{
    ieee80211_resmgr_t resmgr = (ieee80211_resmgr_t)ctx;
    ieee80211_resmgr_sm_event_t event =  (ieee80211_resmgr_sm_event_t)event_data;
    ieee80211_vap_t vap = event->vap;
    ieee80211_resmgr_vap_priv_t resmgr_vap = NULL;
    bool retVal = true;
    struct ieee80211com *ic = resmgr->ic;

    if (vap) {
        resmgr_vap = ieee80211vap_get_resmgr(vap);
    }
    
    switch (event_type) {

    case IEEE80211_RESMGR_EVENT_VAP_START_REQUEST:
        ieee80211_resmgr_defer_event(resmgr, resmgr_vap, event, event_type);
        break;

    case IEEE80211_RESMGR_EVENT_VAP_STOPPED:
    {
        struct vap_iter_check_state_params params;
        u_int16_t next_state = 0;

        if (!resmgr_vap)
            break;
        resmgr_vap->state = VAP_STOPPED;

        
        /* If currently starting VAP has been stopped, unpause other vaps */
        OS_MEMZERO(&params, sizeof(struct vap_iter_check_state_params));
        wlan_iterate_vap_list(ic, ieee80211_vap_iter_check_state, &params);

        if (params.vap_starting) {
            if (!params.vaps_active) {
                next_state = IEEE80211_RESMGR_STATE_VAP_STARTING;
            }
        } else {
            if (!params.vaps_running) {
                next_state = IEEE80211_RESMGR_STATE_IDLE;
            } else if (params.vaps_active) {
                /* VAPs still in the process of pausing, unpause them when they are done */
                resmgr->cancel_pause = true;
            } else {
                struct vap_iter_unpause_params unpause_params;
                
                OS_MEMZERO(&unpause_params, sizeof(struct vap_iter_unpause_params));
                unpause_params.chan = resmgr_vap->chan;
                wlan_iterate_vap_list(ic, ieee80211_vap_iter_resume_request, &unpause_params);
            }
        }

        /* TBD - Check chip mode and set appropriately */

        if (next_state)
            ieee80211_sm_transition_to(resmgr->hsm_handle, next_state);
        break;
    }

    case IEEE80211_RESMGR_EVENT_OFFCHAN_REQUEST:
    {
        ieee80211_resmgr_defer_event(resmgr, NULL, event, event_type);
        break;
    }

    case IEEE80211_RESMGR_EVENT_VAP_PAUSE_COMPLETE:
    {
        struct vap_iter_check_state_params params;
        resmgr_vap->state = VAP_PAUSED;

        /* Check if all the VAPs are paused */
        OS_MEMZERO(&params, sizeof(struct vap_iter_check_state_params));
        wlan_iterate_vap_list(ic, ieee80211_vap_iter_check_state, &params);

        if (!params.vaps_active && !params.vaps_switching) {
            if (resmgr->cancel_pause) {
                /* Pending Pause cancel */
                struct vap_iter_unpause_params unpause_params;
                
                OS_MEMZERO(&unpause_params, sizeof(struct vap_iter_unpause_params));
                unpause_params.chan = resmgr_vap->chan;
                wlan_iterate_vap_list(ic, ieee80211_vap_iter_resume_request, &unpause_params);
                resmgr->cancel_pause = false;
            } else {
                ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_VAP_STARTING);
            }
        }
        break;
    }

    case IEEE80211_RESMGR_EVENT_BSSCHAN_REQUEST:
    {
        ieee80211_resmgr_notification notif;

        if (ieee80211_resmgr_defer_event(resmgr, NULL, event, event_type)) {
           /* The event is deferred, we do not want it to derfer and we want to complete it here */
           /* Clear the deferred event */ 
           ieee80211_resmgr_clear_event(resmgr, NULL);
        }
        /*  Bss-Chan Req, we are already on bss channel of the vap complete it immediately */
        notif.type = IEEE80211_RESMGR_BSSCHAN_SWITCH_COMPLETE;
        notif.req_id = IEEE80211_RESMGR_REQID;
        notif.status = EOK;
        notif.vap = NULL;
        IEEE80211_RESMGR_NOTIFICATION(resmgr, &notif);
        break;
    }

    case IEEE80211_RESMGR_EVENT_VAP_RESUME_COMPLETE:
    {
        struct vap_iter_check_state_params params;
        resmgr_vap->state = VAP_ACTIVE;

        /* Check if all VAPs on the channel are unpaused */
        OS_MEMZERO(&params, sizeof(struct vap_iter_check_state_params));
        params.chan = resmgr_vap->chan;
        wlan_iterate_vap_list(ic, ieee80211_vap_iter_check_state, &params);

        if (!params.vaps_paused) {
            ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_BSSCHAN);
        }
        break;
    }

    case IEEE80211_RESMGR_EVENT_CSA_COMPLETE:
    {
        struct vap_iter_check_state_params params;
        resmgr_vap->state = VAP_ACTIVE;

        OS_MEMZERO(&params, sizeof(struct vap_iter_check_state_params));
        wlan_iterate_vap_list(ic, ieee80211_vap_iter_check_state, &params);

        /* Vap has completed channel switch request, pause it */
        if (!params.vaps_switching)
            ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_VAP_STARTING);
        break;
    }

    case IEEE80211_RESMGR_EVENT_CHAN_SWITCH_REQUEST:
        ieee80211_resmgr_defer_event(resmgr, resmgr_vap, event, event_type);
        break;

    case IEEE80211_RESMGR_EVENT_TSF_TIMER_EXPIRED:
    case IEEE80211_RESMGR_EVENT_TSF_TIMER_CHANGED:
    case IEEE80211_RESMGR_EVENT_OFF_CHAN_SCHED_START_REQUEST:
    case IEEE80211_RESMGR_EVENT_OFF_CHAN_SCHED_STOP_REQUEST:
        ieee80211_resmgr_off_chan_scheduler(resmgr, event_type);
        break;
        
    default:
        retVal = false;
        break;
    }

    if (event->ni_bss_node) {
           ieee80211_free_node(event->ni_bss_node);
    }
    return retVal;
}

/*
 * State VAP_STARTING
 */
static void ieee80211_resmgr_state_vapstarting_entry(void *ctx) 
{
    ieee80211_resmgr_t resmgr = (ieee80211_resmgr_t)ctx;
    ieee80211_vap_t vap = NULL;
    ieee80211_resmgr_vap_priv_t resmgr_vap = NULL;
    struct ieee80211com *ic = resmgr->ic;
    struct vap_iter_check_state_params params;

    /* Check if this is the first VAP starting */
    OS_MEMZERO(&params, sizeof(struct vap_iter_check_state_params));
    wlan_iterate_vap_list(ic, ieee80211_vap_iter_check_state, &params);

    /* Set the requested channel */
    wlan_iterate_vap_list(ic, ieee80211_vap_iter_get_vap_starting, &vap);
    if (vap) {
        resmgr_vap = ieee80211vap_get_resmgr(vap);
    }

    IEEE80211_DPRINTF_IC(resmgr->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                         "%s : Entry vap 0x%x \n",__func__,vap);
    if (resmgr_vap ) {
       IEEE80211_DPRINTF_IC(resmgr->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                            "%s: vap-%d bss chan %d mode %s' \n",__func__,vap->iv_unit,
                                    wlan_channel_ieee(resmgr_vap->chan),
                                    ieee80211_phymode_to_name(wlan_channel_phymode(resmgr_vap->chan)));
       if (!params.vaps_active || !IS_FREQ_EQUAL(ic->ic_curchan, resmgr_vap->chan)) {
           IEEE80211_DPRINTF_IC(resmgr->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                                "%s: vap-%d seting radio to vap bss chan ' \n",__func__,vap->iv_unit);
           ic->ic_curchan = resmgr_vap->chan;
           vap->iv_bsschan = ic->ic_curchan;
           ic->ic_set_channel(ic);
           ieee80211_reset_erp(ic, ic->ic_curmode, vap->iv_opmode);
           ieee80211_wme_initparams(vap);
        } else {
           vap->iv_bsschan = resmgr_vap->chan;
           if (is_chan_superset(resmgr_vap->chan, ic->ic_curchan)) {
               IEEE80211_DPRINTF_IC(resmgr->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                                    "%s: bss chan %d upgrade from  '%s' to mode %s' \n",__func__,
                                    wlan_channel_ieee(ic->ic_curchan),
                                    ieee80211_phymode_to_name(wlan_channel_phymode(ic->ic_curchan)),
                                    ieee80211_phymode_to_name(wlan_channel_phymode(resmgr_vap->chan)));
               ic->ic_curchan = resmgr_vap->chan;
               ic->ic_set_channel(ic);
               ieee80211_reset_erp(ic, ic->ic_curmode, vap->iv_opmode);
               ieee80211_wme_initparams(vap);
           }
        }
    }

    /* TBD - Check chip mode and set appropriately */

    /* Intimate start completion to VAP module */
    ieee80211_notify_vap_start_complete(resmgr, vap, IEEE80211_RESMGR_STATUS_SUCCESS);
}

static bool ieee80211_resmgr_state_vapstarting_event(void *ctx, u_int16_t event_type, u_int16_t event_data_len, void *event_data) 
{
    ieee80211_resmgr_t resmgr = (ieee80211_resmgr_t)ctx;
    ieee80211_resmgr_sm_event_t event =  (ieee80211_resmgr_sm_event_t)event_data;
    ieee80211_vap_t vap = event->vap;
    ieee80211_resmgr_vap_priv_t resmgr_vap = NULL;
    bool retVal = true;
    struct ieee80211com *ic = resmgr->ic;

    if (vap) {
        resmgr_vap = ieee80211vap_get_resmgr(vap);
    }

    switch (event_type) {

    case IEEE80211_RESMGR_EVENT_VAP_START_REQUEST:
        if (!resmgr_vap)
            break;

        if (resmgr_vap->state == VAP_STARTING) {
            /* Same VAP being re-started, make sure that the appropriate notification goes out */
            ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_VAP_STARTING);
        } else {
            ieee80211_resmgr_defer_event(resmgr, resmgr_vap, event, event_type);
        }
        break;

    case IEEE80211_RESMGR_EVENT_VAP_UP:
    {
        struct vap_iter_unpause_params params = {0, 0};

        resmgr_vap->state = VAP_ACTIVE;

        /* Unpause VAP's on same channel */
        params.chan = resmgr_vap->chan;
        wlan_iterate_vap_list(ic, ieee80211_vap_iter_resume_request, &params);

        if (!params.vaps_unpausing) {
            ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_BSSCHAN);
        }
        break;
    }

    case IEEE80211_RESMGR_EVENT_VAP_STOPPED:
    {
        struct vap_iter_check_state_params params;

        resmgr_vap->state = VAP_STOPPED;

        
        OS_MEMZERO(&params, sizeof(struct vap_iter_check_state_params));
        wlan_iterate_vap_list(ic, ieee80211_vap_iter_check_state, &params);
 
        if (!params.vap_starting) {
            if (!params.vaps_running) {
                ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_IDLE);
            } else {
                struct vap_iter_unpause_params unpause_params = {0, 0};

                unpause_params.chan = resmgr_vap->chan;
                wlan_iterate_vap_list(ic, ieee80211_vap_iter_resume_request, &unpause_params);

                if (!unpause_params.vaps_unpausing) {
                    ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_BSSCHAN);
                }
            }
        }

        /* TBD - Check chip mode and set appropriately */

        break;
    }

    case IEEE80211_RESMGR_EVENT_OFFCHAN_REQUEST:
    {
        ieee80211_resmgr_defer_event(resmgr, NULL, event, event_type);
        break;
    }

    case IEEE80211_RESMGR_EVENT_BSSCHAN_REQUEST:
    {
        ieee80211_resmgr_notification notif;

        if (ieee80211_resmgr_defer_event(resmgr, NULL, event, event_type)) {
           /* the event is deferred ,  we do not want it to derfer and we want to complete it here */
           /* clear the deferred event */ 
           ieee80211_resmgr_clear_event(resmgr, NULL);
        }
        /*  Bss-Chan Req, we are already on bss channel of the vap complete it immediately */
        notif.type = IEEE80211_RESMGR_BSSCHAN_SWITCH_COMPLETE;
        notif.req_id = IEEE80211_RESMGR_REQID;
        notif.status = EOK;
        notif.vap = NULL;
        IEEE80211_RESMGR_NOTIFICATION(resmgr, &notif);
        break;
    }

    case IEEE80211_RESMGR_EVENT_VAP_RESUME_COMPLETE:
    {
        struct vap_iter_check_state_params params;
        resmgr_vap->state = VAP_ACTIVE;

        /* Check if all VAPs on the channel are unpaused */
        OS_MEMZERO(&params, sizeof(struct vap_iter_check_state_params));
        params.chan = resmgr_vap->chan;
        wlan_iterate_vap_list(ic, ieee80211_vap_iter_check_state, &params);

        if (!params.vaps_paused) {
            ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_BSSCHAN);
        }
        break;
    }

    case IEEE80211_RESMGR_EVENT_CHAN_SWITCH_REQUEST:
        ieee80211_resmgr_defer_event(resmgr, resmgr_vap, event, event_type);
        break;

    case IEEE80211_RESMGR_EVENT_TSF_TIMER_EXPIRED:
    case IEEE80211_RESMGR_EVENT_TSF_TIMER_CHANGED:
    case IEEE80211_RESMGR_EVENT_OFF_CHAN_SCHED_START_REQUEST:
    case IEEE80211_RESMGR_EVENT_OFF_CHAN_SCHED_STOP_REQUEST:
        ieee80211_resmgr_off_chan_scheduler(resmgr, event_type);
        break;
        
    default:
        retVal = false;
        break;
    }

    if (event->ni_bss_node) {
           ieee80211_free_node(event->ni_bss_node);
    }
    return retVal;
}

/*
 * State BSSCHAN
 */
static void ieee80211_resmgr_state_bsschan_entry(void *ctx) 
{
    ieee80211_resmgr_t resmgr = (ieee80211_resmgr_t) ctx;

    /* This is a stable state, process deferred events if any ...  */
    ieee80211_resmgr_process_deferred_events(resmgr);
}

static bool ieee80211_resmgr_state_bsschan_event(void *ctx, u_int16_t event_type, u_int16_t event_data_len, void *event_data) 
{
    ieee80211_resmgr_t resmgr = (ieee80211_resmgr_t) ctx;
    ieee80211_resmgr_sm_event_t event =  (ieee80211_resmgr_sm_event_t)event_data;
    ieee80211_vap_t vap = event->vap;
    ieee80211_resmgr_vap_priv_t resmgr_vap = NULL;
    struct ieee80211com *ic = resmgr->ic;
    struct ieee80211_ath_channel *channel = NULL;
    bool retVal = true;

    if (vap) {
        resmgr_vap = ieee80211vap_get_resmgr(vap);
    }

    /* let the scheduler handle the event first. if schedulder does not, then handle here */
    if ( ieee80211_resmgr_off_chan_scheduler_bsschan_event_handler(resmgr,event_type,event_data_len,event_data) ) {
       return true;
    } 
    switch (event_type) {

    case IEEE80211_RESMGR_EVENT_VAP_START_REQUEST:
    {
        struct vap_iter_check_state_params params;
        if (!resmgr_vap)
            break;

        if (!ieee80211_resmgr_handle_vap_start_request(resmgr, vap)) {
              break;
        }

        resmgr_vap->state = VAP_STARTING;       
        resmgr_vap->chan = event->chan;


        /* Check if any other vaps exist */
        OS_MEMZERO(&params, sizeof(struct vap_iter_check_state_params));
        wlan_iterate_vap_list(ic, ieee80211_vap_iter_check_state, &params);
        if (params.vaps_running == 0) {
            /* no other vaps are running, the same vap is restarting */
            ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_VAP_STARTING);
            break;
        }


        /* Single Channel Operation
         * New STA Vap - Switch other vaps to its channel (if diff from operating channel)
         * New non-STA Vap - Start on channel being used
         */
        if (vap->iv_opmode == IEEE80211_M_STA) {
            if (IS_FREQ_EQUAL(ic->ic_curchan, resmgr_vap->chan)) {
                /* STA starting on operating channel */
                ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_VAP_STARTING);
                break;
            } else {

               IEEE80211_DPRINTF_IC(resmgr->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                                    "%s: vap-%d cur chan %d vap bss chan %d \n",__func__,vap->iv_unit,
                                    wlan_channel_ieee(ic->ic_curchan),
                                    wlan_channel_ieee(resmgr_vap->chan));
                /* 
                 * if this is a second STA vap coming up, fail the VAP START , we can not support
                 * 2 STA vaps in different channels
                 */
                if (params.num_sta_vaps_active > 1) { 
                    ieee80211_notify_vap_start_complete(resmgr, vap, IEEE80211_RESMGR_STATUS_NOT_SUPPORTED);
                    break;
                }
                /* Send channel switch request to active VAPs on different channels */
                /* When the chan width of AP and STA VAP is different, search a proper chan(freq & flags) for AP VAP*/
                if (is_chan_superset(ic->ic_curchan, resmgr_vap->chan) || is_chan_superset(resmgr_vap->chan, ic->ic_curchan)) {
                    channel = find_proper_chan(ic, resmgr_vap->chan, ic->ic_curchan);
                }
                if (channel == NULL) {   // it can cover the same channel width and channel cannot be found
                    // Use STA VAP channel directly
                    channel = resmgr_vap->chan;
                }
                wlan_iterate_vap_list(ic, ieee80211_vap_iter_chanswitch_request, channel);
            }
        } else {
            if (!IS_FREQ_EQUAL(ic->ic_curchan, resmgr_vap->chan)) {
                /* requested channel frequency is not same as current chan freq */
                /* force the new AP vap coming up to the same as current channel */
                /* When the chan width of AP and STA VAP is different, search a proper chan(freq & flags) for AP VAP*/
                if (is_chan_superset(ic->ic_curchan, resmgr_vap->chan) || is_chan_superset(resmgr_vap->chan, ic->ic_curchan)) {
                    channel = find_proper_chan(ic, ic->ic_curchan, resmgr_vap->chan);
                }
                if (channel == NULL) {    // it can cover the same channel width and channel cannot be found
                    // Use STA VAP channel directly
                    channel = ic->ic_curchan;
                }
                resmgr_vap->chan = channel;
            }
            ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_VAP_STARTING);
            break;
        }           
        ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_VAP_PRESTART);
        break;
    }

    case IEEE80211_RESMGR_EVENT_VAP_STOPPED:
    {
        struct vap_iter_check_state_params params;

        if (!resmgr_vap)
            break;

        /* Stop associated VAP */
        resmgr_vap->state = VAP_STOPPED;

        /* Check if any other vaps exist */
        OS_MEMZERO(&params, sizeof(struct vap_iter_check_state_params));
        wlan_iterate_vap_list(ic, ieee80211_vap_iter_check_state, &params);

        if (!params.vaps_running) {
            ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_IDLE);
        } else if (params.vaps_paused) {
            /* TBD - Multi Channel operation, need to move to new bss channel & unpause associated vaps */
        }
        break;
    }

    case IEEE80211_RESMGR_EVENT_BSSCHAN_REQUEST:
    {
        ieee80211_resmgr_notification notif;
        struct ieee80211_ath_channel *chan = NULL;
    
        /* check wheter any vap is still paused */
        /* Pick channel from first VAP (single channel operation) */
        wlan_iterate_vap_list(ic, ieee80211_vap_iter_bss_channel, &chan);
        if(chan) {
            resmgr->scandata.chan = chan;
            ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_RESUMING);
            break;
        }

        /* Already in bss channel, intimate request completion */
        notif.type = IEEE80211_RESMGR_BSSCHAN_SWITCH_COMPLETE;
        notif.req_id = IEEE80211_RESMGR_REQID;
        notif.status = 0;
        notif.vap = NULL;
        IEEE80211_RESMGR_NOTIFICATION(resmgr, &notif);
        break;
    }

    case IEEE80211_RESMGR_EVENT_OFFCHAN_REQUEST:
    {
        /* Setup channel to be set after VAPs are paused */
        resmgr->scandata.chan = event->chan;
        resmgr->scandata.canpause_timeout = event->max_time ? event->max_time: CANPAUSE_INFINITE_WAIT;
        ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_CANPAUSE);
        break;
    }

    case IEEE80211_RESMGR_EVENT_CHAN_SWITCH_REQUEST:
    {
        if (!resmgr_vap)
            break;

        ieee80211_resmgr_chanswitch_common_handler(resmgr, vap, event->chan, false);
        break;
    }

    default:
        retVal = false;
        break;
    }

    if (event->ni_bss_node) {
       ieee80211_free_node(event->ni_bss_node);
    }
    return retVal;
}

/*
 * State CAN_PAUSE 
 */
static void ieee80211_resmgr_state_canpause_entry(void *ctx) 
{
    ieee80211_resmgr_t resmgr = (ieee80211_resmgr_t)ctx;

    /* Start timer */
    OS_SET_TIMER(&resmgr->scandata.canpause_timer, 0);
}

static void ieee80211_resmgr_state_canpause_exit(void *ctx) 
{
    /* NONE */
}

static bool ieee80211_resmgr_state_canpause_event(void *ctx, u_int16_t event_type, u_int16_t event_data_len, void *event_data) 
{
    ieee80211_resmgr_t resmgr = (ieee80211_resmgr_t) ctx;
    ieee80211_resmgr_sm_event_t event =  (ieee80211_resmgr_sm_event_t)event_data;
    ieee80211_vap_t vap = event->vap;
    ieee80211_resmgr_vap_priv_t resmgr_vap = NULL;
    struct ieee80211com *ic = resmgr->ic;
    struct ieee80211_ath_channel *channel = NULL;
    bool retVal=true;

    if (vap) {
        resmgr_vap = ieee80211vap_get_resmgr(vap);
    }

    switch (event_type) {

    case IEEE80211_RESMGR_EVENT_VAP_START_REQUEST:
    {
        ieee80211_resmgr_notification notif;            

        if (!resmgr_vap)
            break;

        /* check if the vap combination is allowed */
        if (!ieee80211_resmgr_handle_vap_start_request(resmgr, vap)) {
              break;
        }

        if (ieee80211_resmgr_needs_scheduler(resmgr,event_type,event) ){
            /*
             * scheduler is needed (requires off channel operation)
             * defer it until we reach BSSCHAN state where the scheduler will be turned on .
             */
            ieee80211_resmgr_defer_event(resmgr, resmgr_vap, event, event_type);
            retVal=true;
            break;
        }
        /* VAP start takes more priority. Send failure notification to Scan and continue with VAP start */ 
        OS_CANCEL_TIMER(&resmgr->scandata.canpause_timer);
        notif.type = IEEE80211_RESMGR_OFFCHAN_SWITCH_COMPLETE;
        notif.req_id = IEEE80211_RESMGR_REQID;
        notif.status = EBUSY;
        notif.vap = NULL;
        IEEE80211_RESMGR_NOTIFICATION(resmgr, &notif);


        resmgr_vap->state = VAP_STARTING;
        resmgr_vap->chan = event->chan;
        
        /* Single Channel Operation
         * New STA Vap - Switch other vaps to its channel (if diff from operating channel)
         * New non-STA Vap - Start on channel being used
         */
        if (vap->iv_opmode == IEEE80211_M_STA) {
            if (IS_FREQ_EQUAL(ic->ic_curchan, resmgr_vap->chan)) {
                /* STA starting on operating channel */
                ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_VAP_STARTING);
                break;
            } else {
                struct vap_iter_check_state_params params;

                /* Check if any other vaps exist */
                OS_MEMZERO(&params, sizeof(struct vap_iter_check_state_params));
                wlan_iterate_vap_list(ic, ieee80211_vap_iter_check_state, &params);
                /* 
                 * if this is a second STA vap coming up, fail the VAP START , we can not support
                 * 2 STA vaps in different channels
                 */
                if (params.num_sta_vaps_active > 1) { 
                    ieee80211_notify_vap_start_complete(resmgr, vap, EINVAL);
                    break;
                }
                
                /* Send channel switch request to active VAPs on different channels */
                /* When the chan width of AP and STA VAP is different, search a proper chan(freq & flags) for AP VAP*/
                if (is_chan_superset(ic->ic_curchan, resmgr_vap->chan) || is_chan_superset(resmgr_vap->chan, ic->ic_curchan)) {
                    channel = find_proper_chan(ic, resmgr_vap->chan, ic->ic_curchan);
                }
                if (channel == NULL) {   // it can cover the same channel width and channel cannot be found
                    // Use STA VAP channel directly
                    channel = resmgr_vap->chan;
                }
                wlan_iterate_vap_list(ic, ieee80211_vap_iter_chanswitch_request, channel);
            }
        } else {
            if (!IS_FREQ_EQUAL(ic->ic_curchan, resmgr_vap->chan)) {
                /* requested channel frequency is not same as current chan freq */
                /* force the new AP vap coming up to the same as current channel */
                /* When the chan width of AP and STA VAP is different, search a proper chan(freq & flags) for AP VAP*/
                if (is_chan_superset(ic->ic_curchan, resmgr_vap->chan) || is_chan_superset(resmgr_vap->chan, ic->ic_curchan)) {
                    channel = find_proper_chan(ic, ic->ic_curchan, resmgr_vap->chan);
                }
                if (channel == NULL) {    // it can cover the same channel width and channel cannot be found
                    // Use STA VAP channel directly
                    channel = ic->ic_curchan;
                }
                resmgr_vap->chan = channel;
            }
            ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_VAP_STARTING);
            break;
        }           
        ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_VAP_PRESTART);

        break;
    }

    case IEEE80211_RESMGR_EVENT_VAP_STOPPED:
    {
        struct vap_iter_check_state_params params;

        if (!resmgr_vap)
            break;

        /* Stop associated VAP */
        resmgr_vap->state = VAP_STOPPED;

        OS_MEMZERO(&params, sizeof(struct vap_iter_check_state_params));
        wlan_iterate_vap_list(ic, ieee80211_vap_iter_check_state, &params);

        if (!params.vaps_running) {
            /* Set requested channel and complete request */
            ieee80211_resmgr_offchan_common_handler(resmgr, resmgr->scandata.chan);
            resmgr->scandata.chan = NULL;

            ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_IDLE);
        }
        break;
    }

    case IEEE80211_RESMGR_EVENT_BSSCHAN_REQUEST:
    {
        ieee80211_resmgr_notification notif;

        /* Equivalent to cancelling the off channel request */
        resmgr->scandata.chan = NULL;

        /* Intimate request completion */
        notif.type = IEEE80211_RESMGR_BSSCHAN_SWITCH_COMPLETE;
        notif.req_id = IEEE80211_RESMGR_REQID;
        notif.status = 0;
        notif.vap = NULL;
        IEEE80211_RESMGR_NOTIFICATION(resmgr, &notif);

        ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_BSSCHAN);
        break;
    }

    case IEEE80211_RESMGR_EVENT_VAP_CANPAUSE_TIMER:
    {
        bool vaps_canpause = true;

        /* Check if VAPs can pause */
        wlan_iterate_vap_list(ic, ieee80211_vap_iter_canpause, &vaps_canpause);

        if (!vaps_canpause) {
            /* Rearm timer */
            if (resmgr->scandata.canpause_timeout == CANPAUSE_INFINITE_WAIT) {
                OS_SET_TIMER(&resmgr->scandata.canpause_timer, CANPAUSE_CHECK_INTERVAL);
                break;
            } else if (resmgr->scandata.canpause_timeout > 0) {
                resmgr->scandata.canpause_timeout -= CANPAUSE_CHECK_INTERVAL;
                OS_SET_TIMER(&resmgr->scandata.canpause_timer, CANPAUSE_CHECK_INTERVAL);
                break;
            }
        }

        /* All the vaps can pause or max wait time is over */
        ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_PAUSING);
        break;
    }

    case IEEE80211_RESMGR_EVENT_CHAN_SWITCH_REQUEST:
    {
        ieee80211_resmgr_notification notif;            

        if (!resmgr_vap)
            break;

        /* VAP Channel Switch takes more priority. Send failure notification to Scan and continue */ 
        OS_CANCEL_TIMER(&resmgr->scandata.canpause_timer);
        notif.type = IEEE80211_RESMGR_OFFCHAN_SWITCH_COMPLETE;
        notif.req_id = IEEE80211_RESMGR_REQID;
        notif.status = EBUSY;
        notif.vap = NULL;
        IEEE80211_RESMGR_NOTIFICATION(resmgr, &notif);

        if (ieee80211_resmgr_needs_scheduler(resmgr,event_type,event) ){
            /*
             * scheduler is needed (requires off channel operation)
             * defer it until we reach BSSCHAN state where the scheduler will be turned on .
             */
            ieee80211_resmgr_defer_event(resmgr, resmgr_vap, event, event_type);
            retVal=true;
            break;
        } else {
            ieee80211_resmgr_chanswitch_common_handler(resmgr, vap, event->chan, true);
        }
        break;
    }

    case IEEE80211_RESMGR_EVENT_TSF_TIMER_EXPIRED:
    case IEEE80211_RESMGR_EVENT_TSF_TIMER_CHANGED:
    case IEEE80211_RESMGR_EVENT_OFF_CHAN_SCHED_START_REQUEST:
    case IEEE80211_RESMGR_EVENT_OFF_CHAN_SCHED_STOP_REQUEST:
        ieee80211_resmgr_off_chan_scheduler(resmgr, event_type);
        break;
        
    default:
        retVal = false;
        break;
    }

    if (event->ni_bss_node) {
       ieee80211_free_node(event->ni_bss_node);
    }
    return retVal;
}

static void ieee80211_vap_iter_pause_requested_to_active(void *arg, struct ieee80211vap *vap)
{
    ieee80211_resmgr_vap_priv_t resmgr_vap = ieee80211vap_get_resmgr(vap);

    if (resmgr_vap->state == VAP_PAUSE_REQUESTED) {
       /* 
        * pause has already requested . this is happens when oc channel scheduler 
        * is turned off and we enter from OC_CHANSWITCH (or) BSSCHAN state
        * from the function ieee80211_resmgr_vap_stopped_off_chan_scheduler_turned_off.
        * the PAUSING state and the rest of the Resmgr do not understand the PAUSE_EQUESTED state
        * and they leave the state to VAP_ACTIVE until the pause complete notification is received.
        * to match the expectation change the VAP_PAUSE_REQUESTED to VAP_ACTIVE.
        */    
        resmgr_vap->state = VAP_ACTIVE;
    }
}

/*
 * State PAUSING
 */
static void ieee80211_resmgr_state_pausing_entry(void *ctx) 
{
    ieee80211_resmgr_t resmgr = (ieee80211_resmgr_t) ctx;
    struct ieee80211com *ic = resmgr->ic;

    /* Send Pause request to all active VAPs */
    wlan_iterate_vap_list(ic, ieee80211_vap_iter_pause_request, NULL);

    /* chnage the VAP_PAUSE_REQUESTED to VAP_ACTIVE as expected by the the PAUSING state */
    wlan_iterate_vap_list(ic, ieee80211_vap_iter_pause_requested_to_active, NULL);
}

static bool ieee80211_resmgr_state_pausing_event(void *ctx, u_int16_t event_type, u_int16_t event_data_len, void *event_data) 
{
    ieee80211_resmgr_t resmgr = (ieee80211_resmgr_t) ctx;
    ieee80211_resmgr_sm_event_t event =  (ieee80211_resmgr_sm_event_t)event_data;
    ieee80211_vap_t vap = event->vap;
    ieee80211_resmgr_vap_priv_t resmgr_vap = NULL;
    struct ieee80211com *ic = resmgr->ic;
    bool retVal=true;
    ieee80211_resmgr_notification notif;         

    if (vap) {
        resmgr_vap = ieee80211vap_get_resmgr(vap);
    }

    switch (event_type) {
    case IEEE80211_RESMGR_EVENT_VAP_START_REQUEST:
    {
        if (!resmgr_vap)
            break;

        /* check if the vap combination is allowed */
        if (!ieee80211_resmgr_handle_vap_start_request(resmgr, vap)) {
              break;
        }
        /* Defer event till SM reaches stable state */
        ieee80211_resmgr_defer_event(resmgr, resmgr_vap, event, event_type);

        if (!resmgr->cancel_pause && !resmgr->pause_failed) {
           /* 
            * VAP start takes more priority. Send failure notification to Scan and cancel pause operation.
            * SM will go back to BSS chan state where VAP start will be handled.
            */ 
            notif.type = IEEE80211_RESMGR_OFFCHAN_SWITCH_COMPLETE;
            notif.req_id = IEEE80211_RESMGR_REQID;
            notif.status = EBUSY;
            notif.vap = NULL;
            IEEE80211_RESMGR_NOTIFICATION(resmgr, &notif);
            resmgr->cancel_pause = true;
        }
        break;
    }

    case IEEE80211_RESMGR_EVENT_VAP_STOPPED:
    {
        struct vap_iter_check_state_params params;

        if (!resmgr_vap)
            break;

        /* Stop associated VAP */
        resmgr_vap->state = VAP_STOPPED;

        OS_MEMZERO(&params, sizeof(struct vap_iter_check_state_params));
        wlan_iterate_vap_list(ic, ieee80211_vap_iter_check_state, &params);
        if (!params.vaps_running) {
            if (resmgr->cancel_pause) {
                /* Pause Cancel Pending - BSS Chan Notification */
                notif.type = IEEE80211_RESMGR_BSSCHAN_SWITCH_COMPLETE;
                notif.req_id = IEEE80211_RESMGR_REQID;
                notif.status = 0;
                notif.vap = NULL;
                IEEE80211_RESMGR_NOTIFICATION(resmgr, &notif);
                resmgr->cancel_pause = false;
            } else {
                /* Set requested channel and complete request */
                ieee80211_resmgr_offchan_common_handler(resmgr, resmgr->scandata.chan);
                resmgr->scandata.chan = NULL;
            }
            ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_IDLE);
        }
        break;
    }

    case IEEE80211_RESMGR_EVENT_VAP_PAUSE_COMPLETE:
    {
        struct vap_iter_check_state_params params;

        if (resmgr_vap->state != VAP_STOPPED) {
            /* If VAP is already stopped, don't update state */
            resmgr_vap->state = VAP_PAUSED;
        }

        /* Check if all the VAPs are paused */
        OS_MEMZERO(&params, sizeof(struct vap_iter_check_state_params));
        wlan_iterate_vap_list(ic, ieee80211_vap_iter_check_state, &params);

        if (!params.vaps_active) {
            if (resmgr->cancel_pause) {
                /* Pending pause cancel */
                resmgr->scandata.chan = ic->ic_curchan;
                resmgr->cancel_pause = false;
                ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_RESUMING);
            } else {
                ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_OFFCHAN);
            }
        } else if (params.vaps_active == 1 && resmgr->pause_failed) {
            if (resmgr->cancel_pause) {
                /* Pending pause cancel */
                resmgr->scandata.chan = ic->ic_curchan;
                resmgr->cancel_pause = false;
                ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_RESUMING);
            } else {
                struct vap_iter_unpause_params unpause_params = {0, 0};
                /* Unpause VAPs on channel */
                unpause_params.chan = ic->ic_curchan;
                wlan_iterate_vap_list(ic, ieee80211_vap_iter_unpause_request, &unpause_params);
                resmgr->pause_failed = false;
            }
        }
        break;
    }

    case IEEE80211_RESMGR_EVENT_VAP_PAUSE_FAIL:
    {
        struct vap_iter_check_state_params params;

        /* Unpause VAPs operating on channel */
        OS_MEMZERO(&params, sizeof(struct vap_iter_check_state_params));
        params.chan = ic->ic_curchan; 
        wlan_iterate_vap_list(ic, ieee80211_vap_iter_check_state, &params);

        if (params.vaps_active > 1) {
            /* VAPs still in the process of pausing, need to unpaused them after they are paused */
            resmgr->pause_failed = true;

        } else if (params.vaps_paused) {
            if (resmgr->cancel_pause) {
                /* Pending pause cancel */
                resmgr->scandata.chan = ic->ic_curchan;
                resmgr->cancel_pause = false;
                ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_RESUMING);
            } else {
                struct vap_iter_unpause_params unpause_params = {0, 0};
                /* Unpause VAPs on channel */
                unpause_params.chan = ic->ic_curchan;
                wlan_iterate_vap_list(ic, ieee80211_vap_iter_unpause_request, &unpause_params);
            }
        } else {
            /* Check if there is a cancel pending */
            if (resmgr->cancel_pause) {
                resmgr->cancel_pause = false;
                notif.type = IEEE80211_RESMGR_BSSCHAN_SWITCH_COMPLETE;
                notif.status = EOK;
            } else {
                notif.type = IEEE80211_RESMGR_OFFCHAN_SWITCH_COMPLETE;
                notif.status = EIO;
            }
            notif.req_id = IEEE80211_RESMGR_REQID;
            notif.vap = NULL;
            IEEE80211_RESMGR_NOTIFICATION(resmgr, &notif);

            ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_BSSCHAN);
        }
        break;
    }

    case IEEE80211_RESMGR_EVENT_BSSCHAN_REQUEST:
    {
        /* Equivalent to cancelling the off channel request */
        struct vap_iter_check_state_params params;

        /* Unpause VAPs operating on channel */
        OS_MEMZERO(&params, sizeof(struct vap_iter_check_state_params));
        params.chan = ic->ic_curchan; 
        wlan_iterate_vap_list(ic, ieee80211_vap_iter_check_state, &params);

        if (params.vaps_active) {
            /* VAPs still in the process of pausing, unpause them after pause completion */
            resmgr->cancel_pause = true;

        } else if (params.vaps_paused) {
            resmgr->scandata.chan = ic->ic_curchan;
            ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_RESUMING);
        } else {
            resmgr->scandata.chan = NULL;

            /* Intimate request completion */
            notif.type = IEEE80211_RESMGR_BSSCHAN_SWITCH_COMPLETE;
            notif.req_id = IEEE80211_RESMGR_REQID;
            notif.status = 0;
            notif.vap = NULL;
            IEEE80211_RESMGR_NOTIFICATION(resmgr, &notif);

            ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_BSSCHAN);
        }
        break;
    }

    case IEEE80211_RESMGR_EVENT_VAP_RESUME_COMPLETE:
    {
        struct vap_iter_check_state_params params;
        resmgr_vap->state = VAP_ACTIVE;

        /* Check if all the VAPs on the channel have been unpaused */
        OS_MEMZERO(&params, sizeof(struct vap_iter_check_state_params));
        params.chan = ic->ic_curchan;
        wlan_iterate_vap_list(ic, ieee80211_vap_iter_check_state, &params);

        /* Intimate Pause failure to Scan Module */
        if (!params.vaps_paused) {
            if (resmgr->cancel_pause) {
                resmgr->cancel_pause = false;
                notif.type = IEEE80211_RESMGR_BSSCHAN_SWITCH_COMPLETE;
                notif.status = EOK;
            } else {
                notif.type = IEEE80211_RESMGR_OFFCHAN_SWITCH_COMPLETE;
                notif.status = EIO;
            }
            notif.req_id = IEEE80211_RESMGR_REQID;
            notif.vap = NULL;
            IEEE80211_RESMGR_NOTIFICATION(resmgr, &notif);

            ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_BSSCHAN);
        }
        break;
    }

    case IEEE80211_RESMGR_EVENT_CHAN_SWITCH_REQUEST:
        ieee80211_resmgr_defer_event(resmgr, resmgr_vap, event, event_type);
        break;

    
    case IEEE80211_RESMGR_EVENT_TSF_TIMER_EXPIRED:
    case IEEE80211_RESMGR_EVENT_TSF_TIMER_CHANGED:
    case IEEE80211_RESMGR_EVENT_OFF_CHAN_SCHED_START_REQUEST:
    case IEEE80211_RESMGR_EVENT_OFF_CHAN_SCHED_STOP_REQUEST:
        ieee80211_resmgr_off_chan_scheduler(resmgr, event_type);
        break;
        
    default:
        retVal = false;
        break;
    }

    if (event->ni_bss_node) {
       ieee80211_free_node(event->ni_bss_node);
    }
    return retVal;
}

static void ieee80211_resmgr_state_pausing_exit(void *ctx) 
{
    ieee80211_resmgr_t resmgr = (ieee80211_resmgr_t) ctx;

    /* Make sure the lingering flags are cleared */
    resmgr->cancel_pause = false;
    resmgr->pause_failed = false;
}

/*
 * State OFFCHAN
 */
static void ieee80211_resmgr_state_offchan_entry(void *ctx) 
{
    ieee80211_resmgr_t resmgr = (ieee80211_resmgr_t) ctx;

    /* Set requested channel and complete request */
    ieee80211_resmgr_offchan_common_handler(resmgr, resmgr->scandata.chan);
    resmgr->scandata.chan = NULL;
}

static bool ieee80211_resmgr_state_offchan_event(void *ctx, u_int16_t event_type, u_int16_t event_data_len, void *event_data) 
{
    ieee80211_resmgr_t resmgr = (ieee80211_resmgr_t )ctx;
    struct ieee80211com *ic = resmgr->ic;
    ieee80211_resmgr_sm_event_t event =  (ieee80211_resmgr_sm_event_t)event_data;
    ieee80211_vap_t vap = event->vap;
    ieee80211_resmgr_vap_priv_t resmgr_vap = NULL;
    bool retVal=true;

    if (vap) {
        resmgr_vap = ieee80211vap_get_resmgr(vap);
    }

    switch (event_type) {
    case IEEE80211_RESMGR_EVENT_VAP_START_REQUEST:
    {
        /* Inform scan module and start VAP */
        ieee80211_resmgr_notification notif;
        struct ieee80211_ath_channel *chan = NULL;

        if (!resmgr_vap)
            break;

        /* check if the vap combination is allowed */
        if (!ieee80211_resmgr_handle_vap_start_request(resmgr, vap)) {
              break;
        }

        if (ieee80211_resmgr_needs_scheduler(resmgr,event_type,event) ){
            /*
             * scheduler is needed (requires off channel operation)
             * defer it until we reach BSSCHAN state where the scheduler will be turned on .
             */
            ieee80211_resmgr_defer_event(resmgr, resmgr_vap, event, event_type);
            retVal=true;
            break;
        }

        /* Defer event till SM reaches stable state */
        ieee80211_resmgr_defer_event(resmgr, resmgr_vap, event, event_type);

        /* 
         * VAP start takes more priority. Send abort notification to Scan and go back to BSS chan where
         * VAP start will be handled.
         */ 
        notif.type = IEEE80211_RESMGR_OFFCHAN_ABORTED;
        notif.req_id = IEEE80211_RESMGR_REQID;
        notif.status = 0;
        notif.vap = NULL;
        IEEE80211_RESMGR_NOTIFICATION(resmgr, &notif);

        /* Pick channel from a paused VAP (Single channel operation) */
        wlan_iterate_vap_list(ic, ieee80211_vap_iter_bss_channel, &chan);
        resmgr->scandata.chan = chan;
        ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_RESUMING);
        break;
    }

    case IEEE80211_RESMGR_EVENT_VAP_STOPPED:
    {
        struct vap_iter_check_state_params params;

        if (!resmgr_vap)
            break;

        /* Stop associated VAP */
        resmgr_vap->state = VAP_STOPPED;

        OS_MEMZERO(&params, sizeof(struct vap_iter_check_state_params));
        wlan_iterate_vap_list(ic, ieee80211_vap_iter_check_state, &params);

        if (!params.vaps_running) {
            ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_IDLE);
        }
        break;
    }

    case IEEE80211_RESMGR_EVENT_BSSCHAN_REQUEST:
    {
        struct ieee80211_ath_channel *chan = NULL;

        /* Pick channel 
         *   Multi-Channel case it would be first VAP's channel
         *   Single-Channel case, channel with widest PHY mode has to be picked up
         */
        wlan_iterate_vap_list(ic, ieee80211_vap_iter_bss_channel, &chan);
        /* if no vap is paused, transit to BSSCHAN state */
        if (!chan) {
            ieee80211_resmgr_notification notif;
            
            notif.type = IEEE80211_RESMGR_BSSCHAN_SWITCH_COMPLETE;
            notif.req_id = IEEE80211_RESMGR_REQID;
            notif.status = 0;
            notif.vap = NULL;
            IEEE80211_RESMGR_NOTIFICATION(resmgr, &notif);
            ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_BSSCHAN);
            break;
        }
        resmgr->scandata.chan = chan;

        ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_RESUMING);
        break;
    }

    case IEEE80211_RESMGR_EVENT_OFFCHAN_REQUEST:
    {
        /* Set requested channel and complete request */
        ieee80211_resmgr_offchan_common_handler(resmgr, event->chan);

        break;
    }

    case IEEE80211_RESMGR_EVENT_CHAN_SWITCH_REQUEST:
        ieee80211_resmgr_defer_event(resmgr, resmgr_vap, event, event_type);
        break;

    case IEEE80211_RESMGR_EVENT_TSF_TIMER_EXPIRED:
    case IEEE80211_RESMGR_EVENT_TSF_TIMER_CHANGED:
    case IEEE80211_RESMGR_EVENT_OFF_CHAN_SCHED_START_REQUEST:
    case IEEE80211_RESMGR_EVENT_OFF_CHAN_SCHED_STOP_REQUEST:
        ieee80211_resmgr_off_chan_scheduler(resmgr, event_type);
        break;
        
    default:
        retVal = false;
        break;
    }

    if (event->ni_bss_node) {
       ieee80211_free_node(event->ni_bss_node);
    }
    return retVal;
}

/*
 * State RESUMING
 */
static void ieee80211_resmgr_state_resuming_entry(void *ctx) 
{
    ieee80211_resmgr_t resmgr = (ieee80211_resmgr_t) ctx;
    struct ieee80211com *ic = resmgr->ic;
    struct vap_iter_unpause_params params = {0, 0};

    /* Set required channel */
    if (resmgr->scandata.chan) { 
	    ic->ic_curchan = resmgr->scandata.chan;
    	ic->ic_scan_end(ic);
    	ic->ic_set_channel(ic);
#if ATH_SLOW_ANT_DIV
    	ic->ic_antenna_diversity_resume(ic);
#endif

    	/* Unpause VAPs operating on selected BSS channel */
    	params.chan = ic->ic_curchan;
    	wlan_iterate_vap_list(ic, ieee80211_vap_iter_unpause_request, &params);
    }
}

static bool ieee80211_resmgr_state_resuming_event(void *ctx, u_int16_t event_type, u_int16_t event_data_len, void *event_data) 
{
    ieee80211_resmgr_t resmgr = (ieee80211_resmgr_t )ctx;
    struct ieee80211com *ic = resmgr->ic;
    ieee80211_resmgr_vap_priv_t resmgr_vap = NULL;
    ieee80211_resmgr_sm_event_t event =  (ieee80211_resmgr_sm_event_t)event_data;
    ieee80211_vap_t vap = event->vap;
    ieee80211_resmgr_notification notif;
    bool retVal = true;

    if (vap) {
        resmgr_vap = ieee80211vap_get_resmgr(vap);
    }

    switch (event_type) {
    case IEEE80211_RESMGR_EVENT_VAP_START_REQUEST:
    {
        if (!resmgr_vap)
            break;

        /* check if the vap combination is allowed */
        if (!ieee80211_resmgr_handle_vap_start_request(resmgr, vap)) {
              break;
        }

        ieee80211_resmgr_defer_event(resmgr, resmgr_vap, event, event_type);
        break;
    }

    case IEEE80211_RESMGR_EVENT_VAP_STOPPED:
    {
        struct vap_iter_check_state_params params;

        if (!resmgr_vap)
            break;

        /* Stop associated VAP */
        resmgr_vap->state = VAP_STOPPED;

        OS_MEMZERO(&params, sizeof(struct vap_iter_check_state_params));
        params.chan = ic->ic_curchan;
        wlan_iterate_vap_list(ic, ieee80211_vap_iter_check_state, &params);


        /* Intimate request completion to scan module */
        if (!params.vaps_paused) {
            /* if all vaps are unpaused then send bss chan switch event to scanner */ 
            notif.type = IEEE80211_RESMGR_BSSCHAN_SWITCH_COMPLETE;
            notif.req_id = IEEE80211_RESMGR_REQID;
            notif.status = 0;
            notif.vap = NULL;
            IEEE80211_RESMGR_NOTIFICATION(resmgr, &notif);
        }

        if (!params.vaps_running) {
            /* if no vaps are running then move to INIT state */
            ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_IDLE);
        } else if (!params.vaps_paused) {
            /* if there is atleast one vap running and all vaps are unpaused then move to BSS chan state */ 
            ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_BSSCHAN);
        }
        break;
    }

    case IEEE80211_RESMGR_EVENT_OFFCHAN_REQUEST:
        /* Store ? */
        if (ieee80211_resmgr_needs_scheduler(resmgr,event_type,event) ){
            /*
             * scheduler is needed (requires off channel operation)
             * defer it until we reach BSSCHAN state where the scheduler will be turned on .
             */
            ieee80211_resmgr_defer_event(resmgr, resmgr_vap, event, event_type);
            retVal=true;
            break;
        }
        break;

    case IEEE80211_RESMGR_EVENT_VAP_RESUME_COMPLETE:
    {
        struct vap_iter_check_state_params params;
        resmgr_vap->state = VAP_ACTIVE;

        /* Check if all the VAPs on the channel have been unpaused */
        OS_MEMZERO(&params, sizeof(struct vap_iter_check_state_params));
        params.chan = ic->ic_curchan;
        wlan_iterate_vap_list(ic, ieee80211_vap_iter_check_state, &params);

        /* Intimate request completion to VAP module */
        if (!params.vaps_paused) {
            notif.type = IEEE80211_RESMGR_BSSCHAN_SWITCH_COMPLETE;
            notif.req_id = IEEE80211_RESMGR_REQID;
            notif.status = 0;
            notif.vap = NULL;
            IEEE80211_RESMGR_NOTIFICATION(resmgr, &notif);
            ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_BSSCHAN);
        }
        break;
    }

    case IEEE80211_RESMGR_EVENT_CHAN_SWITCH_REQUEST:
        ieee80211_resmgr_defer_event(resmgr, resmgr_vap, event, event_type);
        break;

    case IEEE80211_RESMGR_EVENT_TSF_TIMER_EXPIRED:
    case IEEE80211_RESMGR_EVENT_TSF_TIMER_CHANGED:
    case IEEE80211_RESMGR_EVENT_OFF_CHAN_SCHED_START_REQUEST:
    case IEEE80211_RESMGR_EVENT_OFF_CHAN_SCHED_STOP_REQUEST:
        ieee80211_resmgr_off_chan_scheduler(resmgr, event_type);
        break;

    default:
        retVal = false;
        break;
    }

    if (event->ni_bss_node) {
       ieee80211_free_node(event->ni_bss_node);
    }
    return retVal;
}

/*
 * State CHANSWITCH 
 */
static void ieee80211_resmgr_state_chanswitch_entry(void *ctx) 
{
    ieee80211_resmgr_t resmgr = (ieee80211_resmgr_t)ctx;
    struct ieee80211com *ic = resmgr->ic;

    /* Send Pause request to all active VAPs on the same channel */
    wlan_iterate_vap_list(ic, ieee80211_vap_iter_standby_request, NULL);
}

static bool ieee80211_resmgr_state_chanswitch_event(void *ctx, u_int16_t event_type, u_int16_t event_data_len, void *event_data) 
{
    ieee80211_resmgr_t resmgr = (ieee80211_resmgr_t )ctx;
    struct ieee80211com *ic = resmgr->ic;
    ieee80211_resmgr_vap_priv_t resmgr_vap = NULL;
    ieee80211_resmgr_sm_event_t event =  (ieee80211_resmgr_sm_event_t)event_data;
    ieee80211_vap_t vap = event->vap, tmpvap = NULL;
    ieee80211_resmgr_notification notif;
    bool retVal = true;
    int status;

    if (vap) {
        resmgr_vap = ieee80211vap_get_resmgr(vap);
    }

    switch (event_type) {
    case IEEE80211_RESMGR_EVENT_VAP_START_REQUEST:
    {
        if (!resmgr_vap)
            break;

        /* check if the vap combination is allowed */
        if (!ieee80211_resmgr_handle_vap_start_request(resmgr, vap)) {
              break;
        }

        ieee80211_resmgr_defer_event(resmgr, resmgr_vap, event, event_type);
        break;
    }

    case IEEE80211_RESMGR_EVENT_VAP_STOPPED:
    {
        struct vap_iter_check_state_params params;

        if (!resmgr_vap)
            break;

        /* Stop associated VAP */
        resmgr_vap->state = VAP_STOPPED;

        /* Check if switching vap still exists */
        wlan_iterate_vap_list(ic, ieee80211_vap_iter_get_vap_switching, &tmpvap);

        if (tmpvap) {
            /* Switching vap still exists */
            resmgr_vap = ieee80211vap_get_resmgr(tmpvap);

            OS_MEMZERO(&params, sizeof(struct vap_iter_check_state_params));
            params.chan = resmgr_vap->chan;
            wlan_iterate_vap_list(ic, ieee80211_vap_iter_check_state, &params);

            if (!params.vaps_active) {
                /* Set desired channel */
                status = ieee80211_set_channel(ic, resmgr_vap->chan);

                /* VAP has completed channel switch */
                resmgr_vap->state = VAP_ACTIVE;

                if ((status == EOK) && params.vaps_paused) {
                    /* Unpause VAPs on channel */
                    params.chan = ic->ic_curchan;
                    wlan_iterate_vap_list(ic, ieee80211_vap_iter_resume_request, &params);

                } else {
                    /* Intimate request completion */
                    notif.type = IEEE80211_RESMGR_CHAN_SWITCH_COMPLETE;
                    notif.req_id = IEEE80211_RESMGR_REQID;
                    notif.status = status;
                    notif.vap = tmpvap;
                    IEEE80211_RESMGR_NOTIFICATION(resmgr, &notif);
                    ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_BSSCHAN);
                }
            }
        } else {
            /* Switching VAP stopped */
            OS_MEMZERO(&params, sizeof(struct vap_iter_check_state_params));
            params.chan = resmgr_vap->chan;
            wlan_iterate_vap_list(ic, ieee80211_vap_iter_check_state, &params);

            if (!params.vaps_active) {
                if (params.vaps_paused) {
                    /* Unpause VAPs on channel */
                    params.chan = ic->ic_curchan;
                    wlan_iterate_vap_list(ic, ieee80211_vap_iter_unpause_request, &params);

                } else {
                    /* Only VAP on channel no longer exists */
                    /* TBD - Multi VAP case, another paused VAP needs to be scheduled */
                    ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_IDLE);
                }
            }
        }
        break;
    }

    case IEEE80211_RESMGR_EVENT_OFFCHAN_REQUEST:
        ieee80211_resmgr_defer_event(resmgr, NULL, event, event_type);
        break;

    case IEEE80211_RESMGR_EVENT_VAP_PAUSE_COMPLETE:
    {
        struct vap_iter_check_state_params params;
        resmgr_vap->state = VAP_PAUSED;

        /* Check if all the VAPs are paused */
        OS_MEMZERO(&params, sizeof(struct vap_iter_check_state_params));
        wlan_iterate_vap_list(ic, ieee80211_vap_iter_check_state, &params);

        if (!params.vaps_active) {
            /* Set the requested channel */
            wlan_iterate_vap_list(ic, ieee80211_vap_iter_get_vap_switching, &tmpvap);
            if (tmpvap) {
                resmgr_vap = ieee80211vap_get_resmgr(tmpvap);
            }

            if (resmgr_vap) {
                /* Set desired channel */
                ieee80211_set_channel(ic, resmgr_vap->chan);

                /* VAP has completed channel switch, update state only after resuming other VAPs on the channel */
                //resmgr_vap->state = VAP_ACTIVE;
            }

            /* Unpause VAPs on channel */
            params.chan = ic->ic_curchan;
            wlan_iterate_vap_list(ic, ieee80211_vap_iter_resume_request, &params);
        }
        break;
    }

    case IEEE80211_RESMGR_EVENT_VAP_RESUME_COMPLETE:
    {
        struct vap_iter_check_state_params params;
        resmgr_vap->state = VAP_ACTIVE;

        /* Check if all VAPs on the channel are unpaused */
        OS_MEMZERO(&params, sizeof(struct vap_iter_check_state_params));
        params.chan = resmgr_vap->chan;
        wlan_iterate_vap_list(ic, ieee80211_vap_iter_check_state, &params);

        if (!params.vaps_paused) {
            wlan_iterate_vap_list(ic, ieee80211_vap_iter_get_vap_switching, &tmpvap);
            if (tmpvap) {
                resmgr_vap = ieee80211vap_get_resmgr(tmpvap);

                /* Channel switch fully done, update state on switching VAP now */
                resmgr_vap->state = VAP_ACTIVE;

                /* Intimate request completion */
                notif.type = IEEE80211_RESMGR_CHAN_SWITCH_COMPLETE;
                notif.req_id = IEEE80211_RESMGR_REQID;
                notif.status = EOK;
                notif.vap = tmpvap;
                IEEE80211_RESMGR_NOTIFICATION(resmgr, &notif);
                ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_BSSCHAN);
            }
        }
        break;
    }

    default:
        retVal = false;
        break;
    }

    if (event->ni_bss_node) {
       ieee80211_free_node(event->ni_bss_node);
    }

    return retVal;
}

ieee80211_state_info ieee80211_resmgr_sm_info[] = {
    { 
        (u_int8_t) IEEE80211_RESMGR_STATE_IDLE, 
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        false,
        "IDLE",
        ieee80211_resmgr_state_idle_entry,
        NULL,
        ieee80211_resmgr_state_idle_event
    },
    { 
        (u_int8_t) IEEE80211_RESMGR_STATE_VAP_PRESTART, 
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        false,
        "VAPPRESTART",
        ieee80211_resmgr_state_vapprestart_entry,
        NULL,
        ieee80211_resmgr_state_vapprestart_event
    },
    { 
        (u_int8_t) IEEE80211_RESMGR_STATE_VAP_STARTING, 
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        false,
        "VAPSTARTING",
        ieee80211_resmgr_state_vapstarting_entry,
        NULL,
        ieee80211_resmgr_state_vapstarting_event
    },
    { 
        (u_int8_t) IEEE80211_RESMGR_STATE_BSSCHAN, 
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        false,
        "BSSCHAN",
        ieee80211_resmgr_state_bsschan_entry,
        NULL,
        ieee80211_resmgr_state_bsschan_event
    },
    { 
        (u_int8_t) IEEE80211_RESMGR_STATE_CANPAUSE, 
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        false,
        "CANPAUSE",
        ieee80211_resmgr_state_canpause_entry,
        ieee80211_resmgr_state_canpause_exit,
        ieee80211_resmgr_state_canpause_event
    },
    { 
        (u_int8_t) IEEE80211_RESMGR_STATE_PAUSING, 
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        false,
        "PAUSING",
        ieee80211_resmgr_state_pausing_entry,
        ieee80211_resmgr_state_pausing_exit,
        ieee80211_resmgr_state_pausing_event
    },
    { 
        (u_int8_t) IEEE80211_RESMGR_STATE_OFFCHAN, 
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        false,
        "OFFCHAN",
        ieee80211_resmgr_state_offchan_entry,
        NULL,
        ieee80211_resmgr_state_offchan_event
    },
    { 
        (u_int8_t) IEEE80211_RESMGR_STATE_RESUMING, 
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        false,
        "RESUMING",
        ieee80211_resmgr_state_resuming_entry,
        NULL,
        ieee80211_resmgr_state_resuming_event
    },
    { 
        (u_int8_t) IEEE80211_RESMGR_STATE_CHANSWITCH, 
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        false,
        "CHANSWITCH",
        ieee80211_resmgr_state_chanswitch_entry,
        NULL,
        ieee80211_resmgr_state_chanswitch_event
    },
    { 
        (u_int8_t) IEEE80211_RESMGR_STATE_OC_CHANSWITCH, 
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        false,
        "OC_CHANSWITCH",
        ieee80211_resmgr_state_oc_chanswitch_entry,
        ieee80211_resmgr_state_oc_chanswitch_exit,
        ieee80211_resmgr_state_oc_chanswitch_event
    }
};

int ieee80211_resmgr_singlechan_sm_create(ieee80211_resmgr_t resmgr)
{
    struct ieee80211com *ic = resmgr->ic;

    /* Create State Machine and start */
    resmgr->hsm_handle = ieee80211_sm_create(ic->ic_osdev, 
                                             "ResMgr",
                                             (void *)resmgr, 
                                             IEEE80211_RESMGR_STATE_IDLE,
                                             ieee80211_resmgr_sm_info,
                                             sizeof(ieee80211_resmgr_sm_info)/sizeof(ieee80211_state_info),
                                             MAX_QUEUED_EVENTS,
                                             sizeof(struct ieee80211_resmgr_sm_event), /* size of event data */
                                             ic->ic_reg_parm.resmgrLowPriority ? MESGQ_PRIORITY_LOW : MESGQ_PRIORITY_HIGH,
                                             ic->ic_reg_parm.resmgrSyncSm ? IEEE80211_HSM_SYNCHRONOUS : IEEE80211_HSM_ASYNCHRONOUS, 
                                             ieee80211_resmgr_sm_debug_print,
                                             resmgr_event_name,
                                             IEEE80211_N(resmgr_event_name)); 
    if (!resmgr->hsm_handle) {
        printk("%s : ieee80211_sm_create failed \n", __func__);
        return ENOMEM;
    }

    resmgr->resmgr_func_table.resmgr_setmode = _ieee80211_resmgr_setmode; 
    resmgr->resmgr_func_table.resmgr_getmode = _ieee80211_resmgr_getmode; 

    return EOK;
}

void ieee80211_resmgr_singlechan_sm_delete(ieee80211_resmgr_t resmgr)
{
    ieee80211_sm_delete(resmgr->hsm_handle);
}

static int _ieee80211_resmgr_setmode(ieee80211_resmgr_t resmgr, ieee80211_resmgr_mode mode)
{
    struct vap_iter_check_state_params params;
    if ( resmgr->mode == mode) {
        return EOK;
    }

    if ( mode == IEEE80211_RESMGR_MODE_MULTI_CHANNEL) {
        if (resmgr->oc_scheduler == NULL ) {
            /* can not support multi channel operation if no scheduler */
        IEEE80211_DPRINTF_IC(resmgr->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                             "%s: resmgr can not support multi channel operation with no scheduler  \n", __func__);
            return EINVAL;
        }
    }

    /* mode can only be switched when all vaps are in stopped state */

    OS_MEMZERO(&params, sizeof(struct vap_iter_check_state_params));
    wlan_iterate_vap_list(resmgr->ic, ieee80211_vap_iter_check_state, &params);

    if (params.vaps_paused || params.vaps_active || 
         params.vap_starting || params.vaps_switching ||
        params.vaps_running ) {

        IEEE80211_DPRINTF_IC(resmgr->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                             "%s: can not change resource manager mode while vaps are active \n",__func__);
        return EBUSY;
    }

    resmgr->mode = mode;
    
    return EOK;
}

static ieee80211_resmgr_mode _ieee80211_resmgr_getmode(ieee80211_resmgr_t resmgr)
{
    return resmgr->mode;
}

/* scheduler/resmgr integration code, needs to be moved in to a separate component */
#if UMAC_SUPPORT_RESMGR_OC_SCHED

/* Starting off-channel scheduling requests */
static void ieee80211_vap_iter_start_scheduling_vaps(void *arg, struct ieee80211vap *vap)
{
    ieee80211_resmgr_vap_priv_t resmgr_vap  = NULL;
    ieee80211_resmgr_t resmgr = NULL;
    
    resmgr_vap = ieee80211vap_get_resmgr(vap);

    
    if (resmgr_vap != NULL) {
        resmgr = resmgr_vap->resmgr;
        
        switch (resmgr_vap->state) {
        case VAP_STARTING:
            IEEE80211_DPRINTF_IC(resmgr->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                                 "%s : VAP_STARTING - vap=0x%08x  resmgr_vap=0x%08x\n",__func__, vap, resmgr_vap);
            ieee80211_resmgr_oc_sched_vap_transition(resmgr, vap,
                resmgr_vap, OFF_CHAN_SCHED_VAP_START);        
            break;
        case VAP_ACTIVE:
        case VAP_PAUSED:
        case VAP_PAUSE_REQUESTED:
        case VAP_SWITCHING:
            IEEE80211_DPRINTF_IC(resmgr->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                                 "%s : VAP_ACTIVE - vap=0x%08x  resmgr_vap=0x%08x\n",__func__, vap, resmgr_vap);
            ieee80211_resmgr_oc_sched_vap_transition(resmgr, vap, 
                resmgr_vap, OFF_CHAN_SCHED_VAP_UP);
            break;
        case VAP_STOPPED:
        case VAP_SLEEP:
            IEEE80211_DPRINTF_IC(resmgr->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                                 "%s : VAP_STOPPED - vap=0x%08x  resmgr_vap=0x%08x\n",__func__, vap, resmgr_vap);
            ieee80211_resmgr_oc_sched_vap_transition(resmgr, vap, 
                resmgr_vap, OFF_CHAN_SCHED_VAP_STOPPED);
            break;
        default:
            IEEE80211_DPRINTF_IC(resmgr->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                                 "%s : INVALID state - vap=0x%08x  resmgr_vap=0x%08x\n",__func__, vap, resmgr_vap);
            break;  
        }   
    }
}

/* Find any vap that is not in stopped state */
static void ieee80211_vap_iter_get_vap_live(void *arg, struct ieee80211vap *vap)
{
    ieee80211_vap_t *live_vap = (ieee80211_vap_t *)arg;
    ieee80211_resmgr_vap_priv_t resmgr_vap  = NULL;

    if (!(*live_vap)) {
        resmgr_vap = ieee80211vap_get_resmgr(vap);
        if (resmgr_vap->state != VAP_STOPPED) {
            *live_vap = vap;
         }
    }
}

/*
 * get the state of the scheduler
 */
bool ieee80211_resmgr_oc_scheduler_is_active(ieee80211_resmgr_t resmgr)
{
    return resmgr->oc_scheduler_active;
}

/*
 * change the on/off state of the scheduler
 */
static bool ieee80211_resmgr_oc_scheduler_change_state(ieee80211_resmgr_t resmgr, bool state)
{
    if (resmgr->oc_scheduler_active == state) {
        return false; 
    }
    IEEE80211_DPRINTF_IC(resmgr->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                         "%s: turn scheduler %s  \n", __func__, state ? "ON":"OFF");
   
    resmgr->oc_scheduler_active = state;
    ieee80211_resmgr_oc_sched_set_state(resmgr, state ? OC_SCHEDULER_ON : OC_SCHEDULER_OFF);
    return true;
}


#define MAX_OP_STR_LEN 32

/*
 * convert  scheduler operation into human readable form.
 */
static void  get_op_string(ieee80211_resmgr_oc_scheduler_operation_data_t  *curop, char *msg_buf)
{
    if (curop->vap) {
        snprintf(msg_buf,MAX_OP_STR_LEN-1, "%s vap-%d",ieee80211_resmgr_oc_sched_get_op_name(curop->operation_type), curop->vap->iv_unit); 
    } else {
        snprintf(msg_buf,MAX_OP_STR_LEN-1, "%s",ieee80211_resmgr_oc_sched_get_op_name(curop->operation_type)); 
    }
}
 
/*
 * check the scheduler needs for new operation and cache the 
 * new operation and also determine if the new operation
 * requires channel change. if it requires new channel then
 * return true else return false.
 */
static bool ieee80211_resmgr_oc_needs_channel_change(ieee80211_resmgr_t resmgr)
{
    ieee80211_resmgr_oc_scheduler_operation_data_t  new_scheduled_operation;
    ieee80211_resmgr_oc_scheduler_operation_data_t  *curop, *newop;
    char cur_op_str[MAX_OP_STR_LEN],new_op_str[MAX_OP_STR_LEN];
    
    curop = &resmgr->current_scheduled_operation;
    
    if (!resmgr->oc_scheduler_active) {
            /* scheduler is not active, no  need for channel change */
            return (false);
    }

    /* get the new scheduled operation and then excute it */
    ieee80211_resmgr_oc_sched_get_scheduled_operation(resmgr,&new_scheduled_operation); 
    newop = &new_scheduled_operation;

    /* print current and new op info */
    get_op_string(curop, cur_op_str);
    get_op_string(newop, new_op_str);
    IEEE80211_DPRINTF_IC(resmgr->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                         "%s: curop %s newop %s\n", __func__, cur_op_str,new_op_str);
#if 0
    if (curop->operation_type == newop->operation_type &&
        curop->vap == newop->vap ) {
        /* if current and new operation are the same , then no need to switch channel */
        return(false);
    }
#endif
    /*
     * if the new operation is scan then we always need to switch channel.
     */ 
    if (newop->operation_type == OC_SCHED_OP_SCAN) {
        
        IEEE80211_DPRINTF_IC(resmgr->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                             "%s: newop is SCAN\n", __func__);
        return(true);
    }

    /*
     * if the old operation is scan then we alwasy need to switch channel.
     */ 
    if (curop->operation_type == OC_SCHED_OP_SCAN && newop->operation_type == OC_SCHED_OP_VAP) {
        IEEE80211_DPRINTF_IC(resmgr->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                             "%s: curop is SCAN and newop is VAP\n", __func__);
        return(true);
    }

    if (newop->operation_type == OC_SCHED_OP_VAP) {
        /* from one vap to another vap */
        struct ieee80211_ath_channel *bsschan1, *bsschan2;
        bsschan1 = resmgr->ic->ic_curchan;
        bsschan2 = ieee80211_get_home_channel(newop->vap);
        if (!bsschan1 || !bsschan2) {
            IEEE80211_DPRINTF_IC(resmgr->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                                 "%s: !bsschan1 || !bsschan2\n", __func__);
            return(false);  /* can it happen ? */
        }
        /* if the channels are different then we need to switch */
        if (wlan_channel_frequency(bsschan1) != wlan_channel_frequency(bsschan2)) {
            IEEE80211_DPRINTF_IC(resmgr->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                                 "%s: needs chan switch curchan %d newchan %d  \n", __func__,
                                 wlan_channel_ieee(bsschan1) ,  wlan_channel_ieee(bsschan2));
            return(true);
        }
    }
    return(false);
}

/* Get current state of all VAPs */
static INLINE void ieee80211_vap_iter_check_channel(void *arg, struct ieee80211vap *vap)
{
    struct vap_iter_check_channel_params *params = (struct vap_iter_check_channel_params *)arg;
    ieee80211_resmgr_vap_priv_t resmgr_vap = ieee80211vap_get_resmgr(vap);
    struct ieee80211_ath_channel *bsschan;

    if (!params || (resmgr_vap->state == VAP_STOPPED))
        return;

    /* if a vap needs scheduler during scan (usually a GO vap) */ 
    if (ieee80211_vap_needs_scheduler_is_set(vap)) {
        params->vap_needs_scheduler = true;
    }

    /* no need to turn on scheduler for multi channel if  multi channel operation is disabled */ 
    if (!params->oc_multichannel_enabled ) {
        return;
    }

    /* if a vap wants to be slave vap and does not want multi channel (usually a GO vap) */ 
    if (ieee80211_vap_no_multichannel_is_set(vap)) {
        return;
    }
    /* if channel is not initialized, initialize it */
    if (params->chan == NULL) {
        params->chan = ieee80211_get_home_channel(vap);
        return;
    }

    /* check if the new vaps bss channel is same as previous vaps bss channel */
    bsschan = ieee80211_get_home_channel(vap);
    if (!bsschan) {
        return;  /* can it happen ? */
    }
    if (wlan_channel_frequency(bsschan) != wlan_channel_frequency(params->chan)) {
        params->oc_scheduler_state = true;
    }

}

typedef enum _oc_state_change {
    IEEE80211_OC_SCHEDULER_NO_STATE_CHANGE,
    IEEE80211_OC_SCHEDULER_TURNED_ON,
    IEEE80211_OC_SCHEDULER_TURNED_OFF,
} ieee80211_oc_scheduler_state_change;

/*
 * check for the current state of the vaps and scheduler and see if the   
 * scheduler needs to be turned on/off.
 */

static ieee80211_oc_scheduler_state_change ieee80211_resmgr_oc_check_and_change_state(ieee80211_resmgr_t resmgr, u_int16_t event_type)
{
    bool oc_scheduler_state = false; /* default state of scheduler is off */
    struct ieee80211com *ic = resmgr->ic;
    struct vap_iter_check_channel_params params;

    /* Check if all the vaps have the same channel (frequency)  */
    OS_MEMZERO(&params, sizeof(struct vap_iter_check_channel_params));
    params.oc_multichannel_enabled = ( resmgr->mode == IEEE80211_RESMGR_MODE_MULTI_CHANNEL); 

    wlan_iterate_vap_list(ic, ieee80211_vap_iter_check_channel, &params);
    

    switch(event_type) {
      case IEEE80211_RESMGR_EVENT_VAP_START_REQUEST:
      case IEEE80211_RESMGR_EVENT_VAP_STOPPED:
      case IEEE80211_RESMGR_EVENT_BSSCHAN_REQUEST:
      case IEEE80211_RESMGR_EVENT_CHAN_SWITCH_REQUEST:
         oc_scheduler_state = params.oc_scheduler_state;
         break;         
      case IEEE80211_RESMGR_EVENT_OFFCHAN_REQUEST:
          IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                             "%s oc_scheduler_state %d vap_needs_scheduler %d  \n",__func__ , params.oc_scheduler_state, 
                              params.vap_needs_scheduler);
          oc_scheduler_state = params.oc_scheduler_state;
          /* turn on scheduler even if one vap needs it */
          if (!oc_scheduler_state) {
              oc_scheduler_state = params.vap_needs_scheduler;
          }
         break;         
    }
     
    if (ieee80211_resmgr_oc_scheduler_change_state(resmgr, oc_scheduler_state)) {
        /* scheduler state changed */
       if (resmgr->oc_scheduler_active) {
         return IEEE80211_OC_SCHEDULER_TURNED_ON;
       } else {
         return IEEE80211_OC_SCHEDULER_TURNED_OFF;
       }
    } 
    return IEEE80211_OC_SCHEDULER_NO_STATE_CHANGE;

}

/*
 * check if scheduler needs to  be turned on.
 */
static bool ieee80211_resmgr_needs_scheduler(ieee80211_resmgr_t resmgr, u_int16_t event_type, ieee80211_resmgr_sm_event_t event)
{
    bool oc_scheduler_state = false; /* default state of scheduler is off */
    struct ieee80211com *ic = resmgr->ic;
    ieee80211_vap_t vap = event->vap;
    struct vap_iter_check_channel_params params;
    ieee80211_resmgr_vapstate_t vap_state;
    struct ieee80211_ath_channel *vap_chan,*bss_chan;
    ieee80211_resmgr_vap_priv_t resmgr_vap = NULL;


    if (vap) {
        resmgr_vap = ieee80211vap_get_resmgr(vap);
    } else {
        return oc_scheduler_state; /* use default if event data is invalid */
    }

    /* Check if all the vaps have the same channel (frequency)  */
    OS_MEMZERO(&params, sizeof(struct vap_iter_check_channel_params));

    params.oc_multichannel_enabled = ( resmgr->mode == IEEE80211_RESMGR_MODE_MULTI_CHANNEL); 

    switch(event_type) {
      case IEEE80211_RESMGR_EVENT_VAP_START_REQUEST:
      case IEEE80211_RESMGR_EVENT_CHAN_SWITCH_REQUEST:
          /* save the vap params */
          vap_state = resmgr_vap->state;
          vap_chan = resmgr_vap->chan;
          bss_chan = vap->iv_bsschan;

          resmgr_vap->state = VAP_STARTING;
          resmgr_vap->chan = event->chan;
          vap->iv_bsschan = event->chan;
          wlan_iterate_vap_list(ic, ieee80211_vap_iter_check_channel, &params);
          /* restore the vap params */
          resmgr_vap->state = vap_state;
          resmgr_vap->chan = vap_chan;
          vap->iv_bsschan = bss_chan;
          oc_scheduler_state = params.oc_scheduler_state;
          break;         
      case IEEE80211_RESMGR_EVENT_OFFCHAN_REQUEST:
          oc_scheduler_state = params.oc_scheduler_state;
          /* turn on scheduler even if one vap needs it */
          if (!oc_scheduler_state) {
              oc_scheduler_state = params.vap_needs_scheduler;
          }
    }

    return oc_scheduler_state;
}


/*
* common function to handle the case where a VAP is stopped and 
* scheduler is turned off due to the vap stop.
* this function returns to the right state and restores all the channels/states 
* to a consistant value before returning back to non oc channel scheduler mode
* of operation.
*/
static void ieee80211_resmgr_vap_stopped_off_chan_scheduler_turned_off(ieee80211_resmgr_t resmgr, ieee80211_vap_t vap  ) 
{

    ieee80211_resmgr_vap_priv_t resmgr_vap = NULL;
    struct ieee80211com *ic = resmgr->ic;

    if (vap) {
        resmgr_vap = ieee80211vap_get_resmgr(vap);
    }
    /* if the scheduler is turned off, update the scheduler_data with current vap */
    if (!resmgr->oc_scheduler_active) {

        /*
         * vap is stopped and scheduler is turned off we need to move to the right resource manager state.
         * assumsion: only one other active vap exists and a potential pending scan request.
         *   Active VP state       Scan Req Pending      action     New State  
         *   ---------------       ----------------      ------     ---------
         *   PAUSE_REQUESTED           YES                none       PAUSING  
         *   PAUSED                    YES                none       OFFCHAN  
         *   ACTIVE                    YES                none       PAUSING  
         *   PAUSED                    NO                unpause     BSSCHAN  
         *   PAUSE_REQUESTED           NO                unpause     BSSCHAN  
         *   ACTIVE                    NO                 none       BSSCHAN  
         */
         
        ieee80211_vap_t live_vap = NULL;
         
         OS_MEMCPY(&resmgr->previous_scheduled_operation, &resmgr->current_scheduled_operation, 
             sizeof(ieee80211_resmgr_oc_scheduler_operation_data_t));

         /* check if another active vap is active then use the vap info */
         wlan_iterate_vap_list(ic, ieee80211_vap_iter_get_vap_live, &live_vap);

         /*
          * if the scheduler tutned off with a current VAP operation in progress and the 
          * current vap (vap in focus ) is also the vap going down then we need to
          * one last channel switch to another active vap.
          */ 
         if (live_vap) {
             resmgr->current_scheduled_operation.operation_type = OC_SCHED_OP_VAP;
             resmgr->current_scheduled_operation.vap = live_vap;
         } else {
             resmgr->current_scheduled_operation.operation_type = OC_SCHED_OP_NONE;
             resmgr->current_scheduled_operation.vap = NULL;
         }

         if ( resmgr->oc_scan_req_pending) { /* pending off chan request or already in off channel */ 
             /*
              * vap down while a pending offchannel request (or) we are off channel and scheduler is turned off. 
              * check if other vaps are active and running.
              *  if active and running then move to pausing state. 
              *  if no vaps are active  but atleast one vap is running ( all vaps are paused) then move to off channel state. 
              *  if no vaps are running then move to idle state.
              */
             struct vap_iter_check_state_params params;
             
             OS_MEMZERO(&params, sizeof(struct vap_iter_check_state_params));
             wlan_iterate_vap_list(ic, ieee80211_vap_iter_check_state, &params);
             if (params.vaps_running ) {
                 /* this will send an off channel event to scanner, is this ok ? */
                 IEEE80211_DPRINTF_IC(resmgr->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                      "%s: vap-%d  VAP_STOPPED pending offchan request and vaps are running \n",__func__,vap->iv_unit);
                 /*
                  *  If all the vaps are paused  then go to OFFCHAN state and if atleast
                  *  one vap is in PAUSING state then go to PAUSING state.
                  */
                 if (!params.vaps_active && !params.vaps_pause_requested) {
                     /* the active vap is paused */
                     ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_OFFCHAN);
                 } else {
                     /* the active vap is either pausing  or active */
                     ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_PAUSING);
                 }
             } else {
                 IEEE80211_DPRINTF_IC(resmgr->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                      "%s: vap-%d  VAP_STOPPED pending offchan request and no vaps are running \n",__func__,vap->iv_unit);
                 resmgr->current_scheduled_operation.operation_type = OC_SCHED_OP_NONE;
                 resmgr->current_scheduled_operation.vap = NULL;
                 ieee80211_resmgr_offchan_common_handler(resmgr, resmgr->scandata.chan);
                 resmgr->scandata.chan = NULL;
                 ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_IDLE);
             }
             resmgr->oc_scan_req_pending = false; /* no pending off chan request */ 
         } else { 
             /* no scan request is pending, select a live vap and switch to its channel  */
             if (live_vap) {
                 ieee80211_resmgr_vap_priv_t  live_resmgr_vap = ieee80211vap_get_resmgr(live_vap);
                 IEEE80211_DPRINTF_IC(resmgr->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                      "%s: vap-%d  VAP_STOPPED, no scan request pending and there is a live vap \n",__func__,vap->iv_unit);
                 switch (live_resmgr_vap->state )   
                 {
                     case VAP_PAUSED: 
                     case VAP_ACTIVE:
                     case VAP_PAUSE_REQUESTED:
                         if (resmgr->previous_scheduled_operation.operation_type == OC_SCHED_OP_SCAN) {
                             /* Whenever ic_scan_end is called, set ic_curchan to the channel of live_vap, 
                              * even if the two channels are the same. 
                              */
                             IEEE80211_DPRINTF_IC(resmgr->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                                                  "%s : [CHAN CHANGE SCAN CMPLT] invoking ic_scan_end\n", __func__);
                             ic->ic_curchan = live_vap->iv_bsschan;
                             ic->ic_scan_end(ic);
                             ic->ic_set_channel(ic);
                         }
        
                         if (ic->ic_curchan != live_vap->iv_bsschan) {     
                             ic->ic_curchan = live_vap->iv_bsschan;
                             ic->ic_set_channel(ic);
                         } 
                         
                         /*
                          * if the newly selected live vap is either in paused state (or)
                          * a pause request has already been issued to the vap in the oc_chanswitch_entry function 
                          * then unpause it.
                          */
                         
                         if ( live_resmgr_vap->state == VAP_PAUSED ||
                             live_resmgr_vap->state == VAP_PAUSE_REQUESTED) {
                             /* unpause the vap and move to BSS chan state*/
                             ieee80211_vap_unpause(live_vap,0);
                             ieee80211_resgmr_lmac_vap_pause_control(live_vap, false);
                             live_resmgr_vap->state = VAP_ACTIVE;
                         }
                         
                         ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_BSSCHAN);
                         break; 
                         
                     case VAP_STARTING: 
                         ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_VAP_STARTING);
                         break;
                         
                     default:
                         IEEE80211_DPRINTF_IC(resmgr->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                                              "%s : ASSERT: unexpected vap state \n", __func__);
                         break;
                         
                         
                 }
            }
         }
     }
}

/*
 * common event handler when scheduler is present.
 * this is called from every state in the resource manager.
 * if the event is handled by the event handler it returns true
 * else it returns false.
 */

static bool ieee80211_resmgr_off_chan_scheduler_bsschan_event_handler(ieee80211_resmgr_t resmgr,
                          u_int16_t event_type, u_int16_t event_data_len, void *event_data) 
{
    bool retval=false;
    ieee80211_resmgr_sm_event_t event =  (ieee80211_resmgr_sm_event_t)event_data;
    ieee80211_vap_t vap = event->vap;
    ieee80211_resmgr_vap_priv_t resmgr_vap = NULL;
    struct ieee80211com *ic = resmgr->ic;
    u_int32_t current_tsf = 0;
    ieee80211_resmgr_notification notif;
    ieee80211_oc_scheduler_state_change oc_state_change = IEEE80211_OC_SCHEDULER_NO_STATE_CHANGE;
    bool  needs_oc_chan_switch=false;
    struct ieee80211_node *tmp_bss = event->ni_bss_node;
    bool  check_for_channel_change=true;
     
    if (vap) {
        resmgr_vap = ieee80211vap_get_resmgr(vap);
    }

    IEEE80211_DPRINTF_IC(resmgr->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                         "%s : Entry\n",__func__);
    
    switch(event_type) {

    case IEEE80211_RESMGR_EVENT_VAP_START_REQUEST:
        /* check if vap combination is  allowed */
        if (!ieee80211_resmgr_handle_vap_start_request(resmgr, vap)) {
              break;
        }

        /* 
         * check if the scheduler needs to be turned on and
         * if yes then turn it on and pass the event to  the scheduler. 
         * return true if scheduler is turned on.
         */
         if (!vap || !resmgr_vap) {
            return false; /* not handled due to invalid event_data arg */
         }
         resmgr_vap->state = VAP_STARTING;       
         resmgr_vap->chan = event->chan;
         vap->iv_bsschan = event->chan;
         oc_state_change = ieee80211_resmgr_oc_check_and_change_state(resmgr, event_type); 
         if (oc_state_change == IEEE80211_OC_SCHEDULER_TURNED_ON) {
             /*
              * if scheduler is turned on just now, we need to setup 
              *  schedules for all existing vaps .
              */
              wlan_iterate_vap_list(ic, ieee80211_vap_iter_start_scheduling_vaps, NULL);
              retval=true;
         } 
         
         /*
          * if the scheduler is on , return true so that the single
          * channel resmgr does not handle the event. 
          */ 
         if (resmgr->oc_scheduler_active) {
            retval=true;
         }
        break;

    case IEEE80211_RESMGR_EVENT_VAP_UP:
         if (!vap || !resmgr_vap) {
            return false; /* not handled due to invalid event_data arg */
         }
        resmgr_vap->state = VAP_ACTIVE;
        
        /*
         * if the scheduler is on , return true so that the single
         * channel resmgr does not handle the event. 
         */ 
         if (resmgr->oc_scheduler_active) {
            retval=true;
         }
        break;
            
    case IEEE80211_RESMGR_EVENT_VAP_AUTH_COMPLETE:
            
        /*
         * if the scheduler is on , return true so that the single
         * channel resmgr does not handle the event. 
         */ 
        if (resmgr->oc_scheduler_active) {
            ieee80211_resmgr_oc_sched_vap_transition(resmgr, vap, 
                 resmgr_vap, OFF_CHAN_SCHED_VAP_UP);
            retval=true;
        }
       break;


    case IEEE80211_RESMGR_EVENT_VAP_STOPPED:
        /* 
         * check if the scheduler needs to be turned off and
         * if yes then turn it off and return false. 
         * also pass the event to scheduler.
         */
         if (!vap || !resmgr_vap) {
            return false; /* not handled due to invalid event_data arg */
         }
        resmgr_vap->state = VAP_STOPPED;
        oc_state_change = ieee80211_resmgr_oc_check_and_change_state(resmgr, event_type);

        
        /* update the scheduler_data with current vap */
        if ( oc_state_change == IEEE80211_OC_SCHEDULER_TURNED_OFF) { 
            ieee80211_resmgr_vap_stopped_off_chan_scheduler_turned_off(resmgr,vap); 
        } else if (!resmgr->oc_scheduler_active) {
            ieee80211_vap_t live_vap = NULL;
            /* check if another active vap is active then use the vap info */
            if (resmgr->current_scheduled_operation.operation_type == OC_SCHED_OP_VAP ) {
                wlan_iterate_vap_list(ic, ieee80211_vap_iter_get_vap_live, &live_vap);
                /*
                 * if the scheduler tutned off with a current VAP operation in progress and the 
                 * current vap (vap in focus ) is also the vap going down then we need to
                 * one last channel switch to another active vap.
                 */ 
                if (live_vap) {
                    resmgr->current_scheduled_operation.operation_type = OC_SCHED_OP_VAP;
                    resmgr->current_scheduled_operation.vap = live_vap;
                } else {
                    resmgr->current_scheduled_operation.operation_type = OC_SCHED_OP_NONE;
                    resmgr->current_scheduled_operation.vap = NULL;
                }
#if 0
            } else if (resmgr->current_scheduled_operation.operation_type == OC_SCHED_OP_SCAN ) {
                /*
                 * vap down while scanner is in off channel, scheduler is off.
                 * check if other vaps are active, if active then move to off channel state otherwise 
                 * move to idle state.
                 */
                struct vap_iter_check_state_params params;
                    
                OS_MEMZERO(&params, sizeof(struct vap_iter_check_state_params));
                wlan_iterate_vap_list(ic, ieee80211_vap_iter_check_state, &params);
                if (params.vaps_running ) {
                    /*
                     * if the current operation is SCAN then 
                     * the radio should already on the off channel and scandata.chan will be NULL.
                     * set the scandata.chan to  the current channel so that when the offchan_entry
                     * function tries to set the channel the channel is valid (not NULL).
                     * the ath layer should ignore the channel change request (changing to the same channel).
                     * ideally we need to enter  OFFCHAN state without executing the entry fucntion. 
                     */ 
                    if (!resmgr->scandata.chan ) {
                       resmgr->scandata.chan = resmgr->ic->ic_curchan;
                    } 
                    /* this will send an off channel event to scanner, scanner ignores it */
                    ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_OFFCHAN);
                } else {
                    resmgr->scandata.chan = NULL;
                    ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_IDLE);
                }
#endif
            }
        }
         /*
          * if the scheduler is on , return true so that the single
          * channel resmgr does not handle the event. 
          */ 
         if (resmgr->oc_scheduler_active) {
            ieee80211_resmgr_oc_sched_vap_transition(resmgr, vap, 
                     resmgr_vap, OFF_CHAN_SCHED_VAP_STOPPED);
            retval=true;
         }
        break;

    case IEEE80211_RESMGR_EVENT_OFFCHAN_REQUEST:
        /* Setup channel to be set after VAPs are paused */
        resmgr->scandata.chan = event->chan;
        resmgr->scandata.canpause_timeout = event->max_time ? event->max_time: CANPAUSE_INFINITE_WAIT;
        
        /* send the request to sceheduler and continue with chan switch*/
        oc_state_change = ieee80211_resmgr_oc_check_and_change_state(resmgr, event_type); 
        if (oc_state_change == IEEE80211_OC_SCHEDULER_TURNED_ON) {
                 /*
                  * if scheduler is turned on just now, we need to setup 
                  *  schedules for all existing vaps .
                  */
              wlan_iterate_vap_list(ic, ieee80211_vap_iter_start_scheduling_vaps, NULL);                 
        }
             /*
              * if the scheduler is on , return true so that the single
              * channel resmgr does not handle the event. 
              */ 
         if (resmgr->oc_scheduler_active) {
            ieee80211_resmgr_oc_sched_req_duration_usec_set(resmgr->scandata.oc_sched_req, 
                               event->max_dwell_time * 1000);
           
            if (event->max_dwell_time < IEEE80211_RESMGR_OC_SCHED_SCAN_DWELL_PRIORITY_MSEC) {
                ieee80211_resmgr_oc_sched_req_priority_set(resmgr->scandata.oc_sched_req,
                    OFF_CHAN_SCHED_PRIO_LOW);
            }
            else {
                ieee80211_resmgr_oc_sched_req_priority_set(resmgr->scandata.oc_sched_req,
                    OFF_CHAN_SCHED_PRIO_HIGH);
            }
            
            resmgr->oc_scan_req_pending = true; /* pending off chan request */ 
            ieee80211_resmgr_oc_sched_req_start(resmgr, resmgr->scandata.oc_sched_req);
             
            ieee80211_resmgr_off_chan_scheduler(resmgr, IEEE80211_RESMGR_EVENT_OFF_CHAN_SCHED_START_REQUEST);           
            retval=true;
         }
        break;

    case IEEE80211_RESMGR_EVENT_CHAN_SWITCH_REQUEST:
        /* CSA on STA vap , turn on/off scheduler if required */
         if (!vap || !resmgr_vap) {
            return false; /* not handled due to invalid event_data arg */
         }
         resmgr_vap->chan = event->chan;
         vap->iv_bsschan = event->chan;
         oc_state_change = ieee80211_resmgr_oc_check_and_change_state(resmgr, event_type);
         if (oc_state_change == IEEE80211_OC_SCHEDULER_TURNED_ON) {
             /*
              * if scheduler is turned on just now, we need to setup 
              *  schedules for all existing vaps .
              */
              wlan_iterate_vap_list(ic, ieee80211_vap_iter_start_scheduling_vaps, NULL);
         } 

        /* Intimate request completion */
        notif.type = IEEE80211_RESMGR_CHAN_SWITCH_COMPLETE;
        notif.req_id = IEEE80211_RESMGR_REQID;
        notif.status = 0;
        notif.vap = vap;
        IEEE80211_RESMGR_NOTIFICATION(resmgr, &notif);
         
        if (resmgr->oc_scheduler_active) {
            retval=true;
        }

        break;

    case IEEE80211_RESMGR_EVENT_BSSCHAN_REQUEST:
         oc_state_change = ieee80211_resmgr_oc_check_and_change_state(resmgr, event_type);
         /*
          * if the scheduler is on , return true so that the single
          * channel resmgr does not handle the event. 
          */ 
         if (resmgr->oc_scheduler_active) {
             
             ieee80211_resmgr_oc_sched_req_stop(resmgr, resmgr->scandata.oc_sched_req);
             
             ieee80211_resmgr_off_chan_scheduler(resmgr, IEEE80211_RESMGR_EVENT_OFF_CHAN_SCHED_STOP_REQUEST);
             
             /* Already in bss channel, intimate request completion */
             notif.type = IEEE80211_RESMGR_BSSCHAN_SWITCH_COMPLETE;
             notif.req_id = IEEE80211_RESMGR_REQID;
             notif.status = 0;
             notif.vap = NULL;
             IEEE80211_RESMGR_NOTIFICATION(resmgr, &notif);
             
             retval=true;
         }
         resmgr->oc_scan_req_pending = false; /* pending off chan request */ 

        break;

    case IEEE80211_RESMGR_EVENT_VAP_PAUSE_COMPLETE:
        /* ignore */
         if (!vap || !resmgr_vap) {
            return false; /* not handled due to invalid event_data arg */
         }
        break;
        
    case IEEE80211_RESMGR_EVENT_VAP_RESUME_COMPLETE:
         if (!vap || !resmgr_vap) {
            return false; /* not handled due to invalid event_data arg */
         }
        if (resmgr->oc_scheduler_active) {
            current_tsf = resmgr->ic->ic_get_TSF32(resmgr->ic);    
            IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                                 "[CHAN CHANGE RESUME CMPLT] current_tsf = 0x%08x\n", current_tsf);
            retval=true;
        }
        break;

    case IEEE80211_RESMGR_EVENT_TSF_TIMER_EXPIRED:
    case IEEE80211_RESMGR_EVENT_TSF_TIMER_CHANGED:
    case IEEE80211_RESMGR_EVENT_OFF_CHAN_SCHED_START_REQUEST:
    case IEEE80211_RESMGR_EVENT_OFF_CHAN_SCHED_STOP_REQUEST:
        ieee80211_resmgr_off_chan_scheduler(resmgr, event_type);
        retval=true;
        break;
    default:
        check_for_channel_change = false;
        break;
    }


    /*
     * check with the sceduler  and transition to CHANSWITCH state 
     * if the new scheduler operation requires channel change.
     * 2 cases where channel change is required.
     * 1) scheduler just turned off due to the current vap is  
     *    going down and we need to one last chan switch to get the focus from 
     *     the vap going down to one of the existing active vaps.
           (needs_oc_chan_switch is set to true for this case )
     * 2) scheduler is active and wants to change channel.
     */

    if ( check_for_channel_change == true ) {
        if(needs_oc_chan_switch ||  ieee80211_resmgr_oc_needs_channel_change(resmgr)) {
            current_tsf = ic->ic_get_TSF32(ic);    
            IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                                 "[CHAN CHANGE BEGIN] current_tsf = 0x%08x\n", current_tsf);
           ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_OC_CHANSWITCH);
        } 
    }
    
    if (!retval) {
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                             "%s let the normal bsschan event handler handle the event \n",__func__ );
    } 
    else {
        if (tmp_bss) {
           ieee80211_free_node(tmp_bss);
        }
    }

    return(retval);
}


/* OC_CHANSWITCH state */
/* 
 * in this state the channel is switched from one vap to another vap 
 *  (or) vap to scanner (or) scanner to vap.
 */
static void ieee80211_resmgr_state_oc_chanswitch_entry(void *ctx) 
{
    ieee80211_resmgr_t resmgr = (ieee80211_resmgr_t) ctx;
    ieee80211_vap_t vap = resmgr->current_scheduled_operation.vap;

    if (resmgr->current_scheduled_operation.operation_type == OC_SCHED_OP_VAP && resmgr->oc_scheduler_active) {
        int          ret;
        ieee80211_resmgr_vap_priv_t resmgr_vap = NULL;        
        /* 
         * vap_pause will fail with return value of EINVAL if vap can not be paused
         * (vap is not active . i.e either in stopped or still trying to come up ).
         * in which case we need to move back to bss chan.
         */
        ret = ieee80211_vap_pause(vap, 0);

        /* Force pause the currently active VAP */
        ieee80211_resgmr_lmac_vap_pause_control(vap, true);

        if ( ret == EINVAL)
        {
           struct ieee80211_resmgr_sm_event event;
           event.vap = NULL;
           IEEE80211_DPRINTF_IC(resmgr->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                             "%s vap pause failed on vap %d move to bss channel \n",__func__,vap->iv_unit );
           /* Post event to ResMgr SM */
           ieee80211_resmgr_sm_dispatch(resmgr, IEEE80211_RESMGR_EVENT_OFF_CHAN_SCHED_SWITCH_CHAN, &event);
        } else {
            resmgr_vap = ieee80211vap_get_resmgr(vap);
            resmgr_vap->state  = VAP_PAUSE_REQUESTED;  
        }
    }
    else {
        struct ieee80211_resmgr_sm_event event;
        event.vap = NULL;
        
        /* Post event to ResMgr SM */
        ieee80211_resmgr_sm_dispatch(resmgr, IEEE80211_RESMGR_EVENT_OFF_CHAN_SCHED_SWITCH_CHAN, &event);
    }
}

static bool ieee80211_resmgr_state_oc_chanswitch_event(void *ctx, u_int16_t event_type, 
             u_int16_t event_data_len, void *event_data)
{
    ieee80211_resmgr_t resmgr = (ieee80211_resmgr_t )ctx;
    bool retval=true;
    ieee80211_resmgr_vap_priv_t resmgr_vap = NULL;
    ieee80211_resmgr_sm_event_t event =  (ieee80211_resmgr_sm_event_t)event_data;
    ieee80211_vap_t vap = event->vap;

    if (vap) {
        resmgr_vap = ieee80211vap_get_resmgr(vap);
    }

    switch(event_type) {
        
    case IEEE80211_RESMGR_EVENT_VAP_START_REQUEST:
        if (vap) {
            IEEE80211_DPRINTF_IC(resmgr->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                             "%s: vap-%d  VAP_START_REQUEST\n",__func__,vap->iv_unit);
            /* check if the vap combination is allowed */
            if (!ieee80211_resmgr_handle_vap_start_request(resmgr, vap)) {
              break;
            }

            resmgr_vap->state = VAP_STARTING;       
            resmgr_vap->chan = event->chan;
            ieee80211_resmgr_oc_sched_vap_transition(resmgr, vap,
                resmgr_vap, OFF_CHAN_SCHED_VAP_START);  
        }
        break;
        
    case IEEE80211_RESMGR_EVENT_VAP_UP:
        if (vap) {
            /* send the request to sceheduler and continue with chan switch*/
            IEEE80211_DPRINTF_IC(resmgr->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                             "%s: vap-%d  VAP_UP\n",__func__,vap->iv_unit);
            resmgr_vap->state = VAP_ACTIVE;
        }
        break;
            
    case IEEE80211_RESMGR_EVENT_VAP_AUTH_COMPLETE:
        if (vap) {
            /* send the request to sceheduler and continue with chan switch*/
            IEEE80211_DPRINTF_IC(resmgr->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                                "%s: vap-%d  VAP_AUTH_COMPLETE\n",__func__,vap->iv_unit);
            ieee80211_resmgr_oc_sched_vap_transition(resmgr, vap, 
                                                         resmgr_vap, OFF_CHAN_SCHED_VAP_UP);
        }
        break;
            

    case IEEE80211_RESMGR_EVENT_VAP_STOPPED:
        if (vap) {
            ieee80211_oc_scheduler_state_change oc_state_change = IEEE80211_OC_SCHEDULER_NO_STATE_CHANGE;

            IEEE80211_DPRINTF_IC(resmgr->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                             "%s: vap-%d  VAP_STOPPED\n",__func__,vap->iv_unit);
            resmgr_vap->state = VAP_STOPPED;
            /* 
             * check if the scheduler needs to be turned off and
             * if yes then turn it off and return false. 
             * also pass the event to scheduler.
             */
            ieee80211_resmgr_oc_sched_vap_transition(resmgr, vap, 
                resmgr_vap, OFF_CHAN_SCHED_VAP_STOPPED);
            oc_state_change = ieee80211_resmgr_oc_check_and_change_state(resmgr, event_type);

            ieee80211_resmgr_vap_stopped_off_chan_scheduler_turned_off(resmgr,vap); 
#if 0
            /* if the scheduler is turned off, update the scheduler_data with current vap */
            if (!resmgr->oc_scheduler_active) {
                ieee80211_vap_t live_vap = NULL;
                 
                OS_MEMCPY(&resmgr->previous_scheduled_operation, &resmgr->current_scheduled_operation, 
                    sizeof(ieee80211_resmgr_oc_scheduler_operation_data_t));

                /* check if another active vap is active then use the vap info */
                wlan_iterate_vap_list(ic, ieee80211_vap_iter_get_vap_live, &live_vap);

                /*
                 * if the scheduler tutned off with a current VAP operation in progress and the 
                 * current vap (vap in focus ) is also the vap going down then we need to
                 * one last channel switch to another active vap.
                 */ 
                if (live_vap) {
                    resmgr->current_scheduled_operation.operation_type = OC_SCHED_OP_VAP;
                    resmgr->current_scheduled_operation.vap = live_vap;
                } else {
                    resmgr->current_scheduled_operation.operation_type = OC_SCHED_OP_NONE;
                    resmgr->current_scheduled_operation.vap = NULL;
                }

                if ( resmgr->scandata.chan) { /* pending off chan request */ 
                    /*
                     * vap down while a pending offchannel request and scheduler is turned off. 
                     * check if other vaps are active and running.
                     *  if active and running then move to pausing state. 
                     *  if no vaps are active  but atleast one vap is running ( all vaps are paused) then move to off channel state. 
                     *  if no vaps are running then move to idle state.
                     */
                    struct vap_iter_check_state_params params;
                    
                    OS_MEMZERO(&params, sizeof(struct vap_iter_check_state_params));
                    wlan_iterate_vap_list(ic, ieee80211_vap_iter_check_state, &params);
                    if (params.vaps_running ) {
                        /* this will send an off channel event to scanner, is this ok ? */
                        IEEE80211_DPRINTF_IC(resmgr->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                             "%s: vap-%d  VAP_STOPPED pending offchan request and vaps are running \n",__func__,vap->iv_unit);
                        if (!params.vaps_active) {
                            ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_OFFCHAN);
                        } else {
                            ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_PAUSING);
                        }
                    } else {
                        IEEE80211_DPRINTF_IC(resmgr->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                             "%s: vap-%d  VAP_STOPPED pending offchan request and no vaps are running \n",__func__,vap->iv_unit);
                        resmgr->current_scheduled_operation.operation_type = OC_SCHED_OP_NONE;
                        resmgr->current_scheduled_operation.vap = NULL;
                        ieee80211_resmgr_offchan_common_handler(resmgr, resmgr->scandata.chan);
                        resmgr->scandata.chan = NULL;
                        ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_IDLE);
                    }
                } else { 
                    /* no scan request is pending, select a live vap and switch its channel  */
                    if (live_vap) {
                        ieee80211_resmgr_vap_priv_t  live_resmgr_vap = ieee80211vap_get_resmgr(live_vap);
                        IEEE80211_DPRINTF_IC(resmgr->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                             "%s: vap-%d  VAP_STOPPED, no scan request pending and there is a live vap \n",__func__,vap->iv_unit);
                        switch (live_resmgr_vap->state )   
                        {
                        case VAP_PAUSED: 
                        case VAP_PAUSE_REQUESTED: 
                        case VAP_ACTIVE:                           
                            ic->ic_curchan = live_vap->iv_bsschan;
                            if (resmgr->previous_scheduled_operation.operation_type == OC_SCHED_OP_SCAN) {
                                IEEE80211_DPRINTF_IC(resmgr->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                                             "%s : [CHAN CHANGE SCAN CMPLT] invoking ic_scan_end\n", __func__);
                        
                                ic->ic_scan_end(ic);
                            }                   
                            ic->ic_set_channel(ic);
                                
                            /*
                             * if the newly selected live vap is either in paused state (or)
                             * a pause request has already been issued to the vap in the oc_chanswitch_entry function 
                             * then unpause it.
                             */
                            if ( live_resmgr_vap->state == VAP_PAUSED ||
                                live_resmgr_vap->state == VAP_PAUSE_REQUESTED) {
                                    
                                /* unpause the vap and move to BSS chan state*/
                                ieee80211_vap_unpause(live_vap,0);
                                ieee80211_resgmr_lmac_vap_pause_control(live_vap, false);
                            }

                            ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_BSSCHAN);
                            break; 

                        case VAP_STARTING: 
                            ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_VAP_STARTING);
                            break;

                        default:
                            IEEE80211_DPRINTF_IC(resmgr->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                                             "%s : ASSERT: unexpected vap state \n", __func__);
                            break;
        

                        }
                    } else {
                        IEEE80211_DPRINTF_IC(resmgr->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                             "%s: vap-%d  VAP_STOPPED, no scan request pending and there is no live vap \n",__func__,vap->iv_unit);
                            if (resmgr->previous_scheduled_operation.operation_type == OC_SCHED_OP_SCAN) {
                                IEEE80211_DPRINTF_IC(resmgr->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                                             "%s : [CHAN CHANGE SCAN CMPLT] invoking ic_scan_end\n", __func__);
                        
                                ic->ic_scan_end(ic);
                            }                   
                            resmgr->scandata.chan = NULL;
                            ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_IDLE);
                    }
                }
            }
#endif
        }
        break;

    case IEEE80211_RESMGR_EVENT_OFFCHAN_REQUEST:
        /* Setup channel to be set after VAPs are paused */
        resmgr->scandata.chan = event->chan;
        resmgr->scandata.canpause_timeout = event->max_time ? event->max_time: CANPAUSE_INFINITE_WAIT;
        
        /* send the request to sceheduler and continue with chan switch*/
        ieee80211_resmgr_oc_sched_req_duration_usec_set(resmgr->scandata.oc_sched_req, event->max_dwell_time * 1000);
        ieee80211_resmgr_oc_sched_req_start(resmgr, resmgr->scandata.oc_sched_req);
        
        ieee80211_resmgr_off_chan_scheduler(resmgr, IEEE80211_RESMGR_EVENT_OFF_CHAN_SCHED_START_REQUEST);           
        resmgr->oc_scan_req_pending = true; /* pending off chan request */ 
        break;

    case IEEE80211_RESMGR_EVENT_BSSCHAN_REQUEST:
        ieee80211_resmgr_defer_event(resmgr, NULL, event, event_type);
        break;

    case IEEE80211_RESMGR_EVENT_VAP_PAUSE_COMPLETE:
    case IEEE80211_RESMGR_EVENT_VAP_PAUSE_FAIL:
    {
        u_int32_t current_tsf;
        
        /* 
         * check if all the vaps have paused on  
         * the current channel (current operation)
         * then transition to BSSCHAN state.
         * the exit routine will execute the new scheduler operation.
         */

         if (!vap || !resmgr_vap) {
            return false; /* not handled due to invalid event_data arg */
         }

        
        if (resmgr_vap->state != VAP_STOPPED) {
            /* If VAP is already stopped, don't update state */
            resmgr_vap->state = VAP_PAUSED;
        }

        current_tsf = resmgr->ic->ic_get_TSF32(resmgr->ic);    
        IEEE80211_DPRINTF_IC(resmgr->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                             "%s: vap-%d  VAP_PAUSE_COMPLETE/FAIL current_tsf = 0x%08x\n",__func__,vap->iv_unit, current_tsf);
        
        ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_BSSCHAN);
    }
        break;
        
    case IEEE80211_RESMGR_EVENT_VAP_RESUME_COMPLETE:
        /* ignore */
        if (!vap || !resmgr_vap) {
            return false; /* not handled due to invalid event_data arg */
        }
        break;


    case IEEE80211_RESMGR_EVENT_TSF_TIMER_EXPIRED:
    case IEEE80211_RESMGR_EVENT_TSF_TIMER_CHANGED:
    case IEEE80211_RESMGR_EVENT_OFF_CHAN_SCHED_START_REQUEST:
    case IEEE80211_RESMGR_EVENT_OFF_CHAN_SCHED_STOP_REQUEST:
        ieee80211_resmgr_off_chan_scheduler(resmgr, event_type);

        break;
        
    case IEEE80211_RESMGR_EVENT_OFF_CHAN_SCHED_SWITCH_CHAN:
        ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_BSSCHAN);     
        break;
            
    default:
        retval=false;
        break;
    }
    if (event->ni_bss_node) {
       ieee80211_free_node(event->ni_bss_node);
    }

    return(retval);
}

static void ieee80211_resmgr_state_oc_chanswitch_exit(void *ctx) 
{
    ieee80211_resmgr_t resmgr = (ieee80211_resmgr_t )ctx;
    struct ieee80211com *ic = resmgr->ic;
    ieee80211_resmgr_vap_priv_t resmgr_vap  = NULL;
    ieee80211_vap_t vap;
    ieee80211_resmgr_oc_scheduler_operation_data_t  *prevop, *curop;
    char prev_op_str[MAX_OP_STR_LEN],cur_op_str[MAX_OP_STR_LEN];

    if (!resmgr->oc_scheduler_active) {
       return;
    }
    OS_MEMCPY(&resmgr->previous_scheduled_operation, &resmgr->current_scheduled_operation, 
         sizeof(ieee80211_resmgr_oc_scheduler_operation_data_t));
    
    /* get the new scheduled operation and then excute it */
    ieee80211_resmgr_oc_sched_get_scheduled_operation(resmgr,&resmgr->current_scheduled_operation); 

    prevop = &resmgr->previous_scheduled_operation; 
    curop = &resmgr->current_scheduled_operation; 

    /* print prev and cur op info */
    get_op_string(prevop, prev_op_str);
    get_op_string(curop, cur_op_str);
    IEEE80211_DPRINTF_IC(resmgr->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                         "%s: prevop %s curop %s\n", __func__, prev_op_str,cur_op_str);
    /* 
     * change the channel and
     * unpause new scheduler operation.
     */

    switch (resmgr->current_scheduled_operation.operation_type) {
        
        case OC_SCHED_OP_VAP:
            vap = resmgr->current_scheduled_operation.vap;
            resmgr_vap = ieee80211vap_get_resmgr(vap);
            
            switch (resmgr_vap->state) {
            case VAP_STARTING:
            IEEE80211_DPRINTF_IC(resmgr->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                         "%s: curop %s VAP_STARTING\n", __func__,cur_op_str);
                {
                    struct vap_iter_check_state_params params;
                    
                    /* Check if this is the first VAP starting */
                    OS_MEMZERO(&params, sizeof(struct vap_iter_check_state_params));
                    wlan_iterate_vap_list(ic, ieee80211_vap_iter_check_state, &params);
                    
                    /* Set the requested channel */
                    if (ic->ic_curchan != vap->iv_bsschan) {
                        /* Set home channel */
                        IEEE80211_DPRINTF_IC(resmgr->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                                             "%s: channel change %d --> %d\n", __func__,
                                             wlan_channel_ieee(ic->ic_curchan) ,  wlan_channel_ieee(vap->iv_bsschan));
                        ic->ic_curchan = vap->iv_bsschan;                       
                        if (resmgr->previous_scheduled_operation.operation_type == OC_SCHED_OP_SCAN) {
                            IEEE80211_DPRINTF_IC(resmgr->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                                                 "%s : [CHAN CHANGE SCAN CMPLT] invoking ic_scan_end\n", __func__);
                            
                            ic->ic_scan_end(ic);
                        }                       
                        ic->ic_set_channel(ic);
                    }
                    
                    /* Intimate start completion to VAP module */
                    ieee80211_notify_vap_start_complete(resmgr, vap, IEEE80211_RESMGR_STATUS_SUCCESS);
                }
                break;
                
            case VAP_PAUSED:                
            case VAP_PAUSE_REQUESTED:                
            IEEE80211_DPRINTF_IC(resmgr->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                         "%s: curop %s VAP_PAUSED\n", __func__,cur_op_str);
                if (ic->ic_curchan != vap->iv_bsschan) {
                    /* Set home channel */
                    IEEE80211_DPRINTF_IC(resmgr->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                                             "%s: channel change %d --> %d\n", __func__,
                                             wlan_channel_ieee(ic->ic_curchan) ,  wlan_channel_ieee(vap->iv_bsschan));
                    ic->ic_curchan = vap->iv_bsschan;
                    if (resmgr->previous_scheduled_operation.operation_type == OC_SCHED_OP_SCAN) {
                        IEEE80211_DPRINTF_IC(resmgr->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                                             "%s : [CHAN CHANGE SCAN CMPLT] invoking ic_scan_end\n", __func__);
                        
                        ic->ic_scan_end(ic);
                    }                   
                    ic->ic_set_channel(ic);
                }
                /* unpause umac first and then unpas ath */ 
                ieee80211_vap_unpause(vap,0);
                ieee80211_resgmr_lmac_vap_pause_control(vap, false);
                
                resmgr_vap->state = VAP_ACTIVE;
                
                break;
                
            default:
                break;
            }
            
            break;
            
        case OC_SCHED_OP_SCAN:
            {
	    if (resmgr->scandata.chan == NULL) {
		ASSERT(FALSE);
   	    }

            ieee80211_resmgr_offchan_common_handler(resmgr, resmgr->scandata.chan);
            resmgr->scandata.chan = NULL;
            resmgr->oc_scan_req_pending = false; /* no pending off chan request */ 
            }
            break;
            
        case OC_SCHED_OP_NONE:
        case OC_SCHED_OP_SLEEP:
        case OC_SCHED_OP_NOP:
        default:    
            break;
    }
    
}

#else /* UMAC_SUPPORT_RESMGR_OC_SCHED */
static void ieee80211_resmgr_state_oc_chanswitch_entry(void *ctx) 
{
}
static bool ieee80211_resmgr_state_oc_chanswitch_event(void *ctx, u_int16_t event_type, 
             u_int16_t event_data_len, void *event_data)
{
        return false;
}
static void ieee80211_resmgr_state_oc_chanswitch_exit(void *ctx) 
{
}
static bool ieee80211_resmgr_off_chan_scheduler_bsschan_event_handler(ieee80211_resmgr_t resmgr,
                          u_int16_t event_type, u_int16_t event_data_len, void *event_data) 
{
    return false;
}
static void ieee80211_resmgr_oc_scheduler_change_state(ieee80211_resmgr_t resmgr, bool state)
{
}
static void ieee80211_vap_iter_start_scheduling_vaps(void *arg, struct ieee80211vap *vap)
{
}
static bool ieee80211_resmgr_needs_scheduler(ieee80211_resmgr_t resmgr,u_int16_t event_type, ieee80211_resmgr_sm_event_t event)
{
    return false;
}
#endif
#else

#endif
