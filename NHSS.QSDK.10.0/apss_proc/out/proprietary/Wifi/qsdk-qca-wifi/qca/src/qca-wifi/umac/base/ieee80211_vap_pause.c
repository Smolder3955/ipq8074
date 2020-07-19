/*
 * Copyright (c) 2011,2017,2019 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * Copyright (c) 2008 Atheros Communications Inc.
 * All Rights Reserved.
 */

#include <ieee80211_var.h>
#include <ieee80211_channel.h>
#include <ieee80211_mlme.h>
#include <ieee80211_target.h>
#include <ieee80211_vap.h>
#include <ieee80211_vap_pause.h>
#include <wlan_mlme_dp_dispatcher.h>
#include <ieee80211_cfg80211.h>
#include <wlan_utility.h>
#include <wlan_vdev_mlme_api.h>
#include <wlan_mlme_vdev_mgmt_ops.h>

#if UMAC_SUPPORT_VAP_PAUSE

#define IEEE80211_MAX_PAUSE_TIMEOUT 40

#define  ieee80211_vap_set_pause(vap)    (vap->iv_pause_info.iv_pause=1)
#define  ieee80211_vap_clear_pause(vap)  (vap->iv_pause_info.iv_pause=0)
#define  ieee80211_vap_set_force_pause(vap)    (vap->iv_pause_info.iv_force_pause=1)
#define  ieee80211_vap_clear_force_pause(vap)  (vap->iv_pause_info.iv_force_pause=0)
#define VAP_DEFAULT_IDLE_TIMEOUT           100
#define VAP_DEFAULT_MIN_PAUSE_BEACON_COUNT   2


static void ieee80211_vap_power_pause_event_handler (struct ieee80211vap *vap, ieee80211_sta_power_event *event, void *arg);

static void ieee80211_vap_resmgr_notification_handler (ieee80211_resmgr_t resmgr,
                                                       ieee80211_resmgr_notification *notification, void *arg)
{
    struct ieee80211vap *vap = (struct ieee80211vap *) arg;
    struct ieee80211com *ic = vap->iv_ic;


    /* Handle notification only if meant for this VAP */
    if (notification->vap == vap) {
        switch(notification->type) {
        case IEEE80211_RESMGR_VAP_START_COMPLETE:
            switch(vap->iv_opmode) {
            case IEEE80211_M_STA:
#if ATH_SUPPORT_DFS && ATH_SUPPORT_STA_DFS
                if(notification->status == IEEE80211_RESMGR_STATUS_SUCCESS) {
                    /* 11AX TODO - Add DFS support for 11ax */

                    if(mlme_is_stacac_needed(vap)) {
                        printk("STACAC_start chan %d timeout %d sec, curr time: %d sec\n",
                            ic->ic_curchan->ic_freq,
                            ieee80211_dfs_get_cac_timeout(ic, ic->ic_curchan),
                            (qdf_system_ticks_to_msecs(qdf_system_ticks()) / 1000));
                        mlme_set_stacac_running(vap,1);
                        mlme_set_stacac_timer(vap,1000*ieee80211_dfs_get_cac_timeout(ic, ic->ic_curchan));
                        mlme_reset_mlme_req(vap);
                        wlan_cfg80211_dfs_cac_start(vap, ieee80211_dfs_get_cac_timeout(ic, ic->ic_curchan));
                        IEEE80211_DELIVER_EVENT_CAC_STARTED(vap, ic->ic_curchan->ic_freq, ieee80211_dfs_get_cac_timeout(ic, ic->ic_curchan));
                    } else {
                        ieee80211_mlme_join_infra_continue(vap,EOK);
                    }
                } else {
                    ieee80211_mlme_join_infra_continue(vap,EINVAL);
                }
#else
                ieee80211_mlme_join_infra_continue(vap,
                  notification->status ==  IEEE80211_RESMGR_STATUS_SUCCESS ? EOK : EINVAL);
#endif
                vap->iv_quick_reconnect = false;
                break;
            case IEEE80211_M_BTAMP:
            case IEEE80211_M_HOSTAP:
                ieee80211_mlme_create_infra_continue_async(vap,
                  notification->status ==  IEEE80211_RESMGR_STATUS_SUCCESS ? EOK : EINVAL);
                break;
            case IEEE80211_M_MONITOR:
                break;
#if UMAC_SUPPORT_IBSS
            case IEEE80211_M_IBSS:
                ieee80211_mlme_create_join_ibss_continue(vap,
                  notification->status ==  IEEE80211_RESMGR_STATUS_SUCCESS ? EOK : EINVAL);
                break;
#endif
            default:
                ASSERT(0);
                break;
            }
            break;

        case IEEE80211_RESMGR_CHAN_SWITCH_COMPLETE:
            ieee80211_mlme_chanswitch_continue(vap->iv_bss, notification->status);
            break;
        case IEEE80211_RESMGR_VAP_RESTART_COMPLETE:
            mlme_ext_vap_up(vap,true);

            /* Send peer authorize command to FW after channel switch announcement */
            if(vap->iv_opmode == IEEE80211_M_STA) {
                   if(vap->iv_root_authorize(vap, 1)) {
                          qdf_print("%s:Unable to authorize peer", __func__);
                   }
            }

            /* Check if VAP restart was due to channel switch triggered for
             * repeater move, and force beacon miss for faster SM transition
             * to disconnect state. Also, change repeater move state after
             * forced beacon miss to IN_PROGRESS
             */
            if ((vap->iv_opmode == IEEE80211_M_STA) && (ic->ic_repeater_move.state == REPEATER_MOVE_START)) {
                IEEE80211_DELIVER_EVENT_BEACON_MISS(vap);
                ic->ic_repeater_move.state = REPEATER_MOVE_IN_PROGRESS;
            }
            break;

        default:
            break;
        }
    }
}

void ieee80211_vap_pause_vattach(struct ieee80211com *ic,ieee80211_vap_t vap)
{
    ieee80211_resmgr_register_notification_handler(ic->ic_resmgr, ieee80211_vap_resmgr_notification_handler, vap);
    vap->iv_pause_info.iv_idle_timeout = VAP_DEFAULT_IDLE_TIMEOUT;
    vap->iv_pause_info.iv_min_pause_beacon_count = VAP_DEFAULT_MIN_PAUSE_BEACON_COUNT;
    ieee80211_vap_clear_pause(vap);
    vap->iv_pause_info.iv_pause_count = 0;
    atomic_set(&vap->iv_pause_info.iv_pause_beacon_count, 0);
}

void ieee80211_vap_pause_late_vattach(struct ieee80211com *ic,ieee80211_vap_t vap)
{
    ieee80211_sta_power_register_event_handler(vap, ieee80211_vap_power_pause_event_handler,vap);
}

void ieee80211_vap_pause_vdetach(struct ieee80211com *ic,ieee80211_vap_t vap)
{
    (void)ieee80211_resmgr_unregister_notification_handler(ic->ic_resmgr, ieee80211_vap_resmgr_notification_handler,vap);
    (void)ieee80211_sta_power_unregister_event_handler(vap, ieee80211_vap_power_pause_event_handler,vap);
}

u_int32_t ieee80211_vap_pause_get_param(wlan_if_t vap, ieee80211_param param)
{
    u_int32_t val = 0;
    switch(param) {
    case IEEE80211_MIN_BEACON_COUNT:
        val = vap->iv_pause_info.iv_min_pause_beacon_count ;
        break;

    case IEEE80211_IDLE_TIME:
        val = vap->iv_pause_info.iv_idle_timeout ;
        break;

    default:
        break;

    }

    return val;

}

u_int32_t ieee80211_vap_pause_set_param(wlan_if_t vap, ieee80211_param param,u_int32_t val)
{
    switch(param) {
    case IEEE80211_MIN_BEACON_COUNT:
        vap->iv_pause_info.iv_min_pause_beacon_count = val ;
        break;

    case IEEE80211_IDLE_TIME:
        vap->iv_pause_info.iv_idle_timeout = val ;
        break;

    default:
        break;

    }

    return 0;

}


/**
 * @ can pause query.
 * ARGS :
 *  vap    : handle to vap.
 * RETURNS:
 *  returns  true if the vap can pause else returns false.
 */
bool ieee80211_vap_can_pause(ieee80211_vap_t vap)
{
    u_int32_t msec_from_last_data, last_data_time;
    /*
     * vap can always pause if it is not active.
     */
    if (wlan_vdev_mlme_is_active(vap->vdev_obj) != QDF_STATUS_SUCCESS)
        return true;
    last_data_time = (u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(vap->iv_lastdata);
    msec_from_last_data = (u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(OS_GET_TIMESTAMP());
    msec_from_last_data -= last_data_time;

    /*
     * can pause only if minumum number of beacons have not been sent/received.
     */
    if (vap->iv_opmode == IEEE80211_M_STA &&
        (atomic_read(&vap->iv_pause_info.iv_pause_beacon_count) < vap->iv_pause_info.iv_min_pause_beacon_count)) {
        return false;
    }

    /*
     * if the vap is idle for some time.
     */
    if (msec_from_last_data < vap->iv_pause_info.iv_idle_timeout )
        return false;

    return true;

}

struct ieee80211_pause_sta_arg {
    ieee80211_vap_t vap;
};

static void ieee80211_pause_sta_iter(void *arg, struct ieee80211_node *ni)
{
    struct ieee80211_pause_sta_arg *itr_arg = (struct ieee80211_pause_sta_arg *)arg;
    if (itr_arg->vap == ni->ni_vap) {
        ieee80211node_pause(ni);
    }
}

/**
 * force pause request.synchronously pause the active traffic in the ath
 * and umac layers. no fake sleep is performed.
 * @param vap    : handle to vap.
 * @param req_id : id of the requestor.
 * @return  EOK if request is processed.
 *  returns EEXIST if the if vap is already in forced pause state.
 */
int ieee80211_vap_force_pause(ieee80211_vap_t vap, u_int16_t reqid )
{
    struct ieee80211_pause_sta_arg itr_arg;
    itr_arg.vap = vap;

    IEEE80211_VAP_LOCK(vap);
    ++vap->iv_pause_info.iv_force_pause_count;
    if (vap->iv_pause_info.iv_force_pause_count == 1) {
        ieee80211_vap_set_force_pause(vap);
        IEEE80211_VAP_UNLOCK(vap);
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_POWER, "%s \n", __func__);
        if (vap->iv_opmode  == IEEE80211_M_STA) {
            ieee80211node_pause(vap->iv_bss);
        } else {
            ieee80211_iterate_node(vap->iv_ic,ieee80211_pause_sta_iter,(void *)&itr_arg);
        }
        ieee80211_resgmr_lmac_vap_pause_control(vap, true);
        return EOK;
    }
    IEEE80211_VAP_UNLOCK(vap);
    return EEXIST;
}


/**
 * pause request.
 * @param vap    : handle to vap.
 * @param req_id : id of the requestor.
 * @return  EOK if it accepted the request and processes the request asynchronously and
 *    at the end of processing the request the VAP posts the
 *    IEEE80211_VAP_PAUSED event to all the registred event handlers.
 *  returns EEXIST if the VAP is in FULL SLEEP state (or) if vap is already in pause  state.
 */
int ieee80211_vap_pause(ieee80211_vap_t vap, u_int16_t reqid )
{
    bool request_pause=FALSE;
    struct ieee80211_pause_sta_arg itr_arg;
    itr_arg.vap = vap;

    ASSERT(vap->iv_opmode == IEEE80211_M_HOSTAP || vap->iv_opmode == IEEE80211_M_STA ||
           vap->iv_opmode == IEEE80211_M_BTAMP || vap->iv_opmode == IEEE80211_M_IBSS||
           vap->iv_opmode == IEEE80211_M_MONITOR);

    if (wlan_vdev_is_up(vap->vdev_obj) != QDF_STATUS_SUCCESS) {
        /* if vap is not up, you can not pause the vap */
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_POWER, "%s: can not pause the vap when it is down \n", __func__);
        return EINVAL;
    }
    IEEE80211_VAP_LOCK(vap);
    ++vap->iv_pause_info.iv_pause_count;
    if (vap->iv_pause_info.iv_pause_count == 1) {
        request_pause=TRUE;
        ieee80211_vap_set_pause(vap);
    }
    IEEE80211_VAP_UNLOCK(vap);

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_POWER, "%s: pause count %d \n", __func__,vap->iv_pause_info.iv_pause_count);
    if (request_pause == FALSE) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_POWER, "%s: already in paused state \n", __func__);
        return EEXIST;
    }
    if (vap->iv_opmode  == IEEE80211_M_STA) {
        if (ieee80211_sta_power_pause(vap, IEEE80211_MAX_PAUSE_TIMEOUT) != EOK) {
            /* failed, force pause complete ?*/
            ieee80211_vap_pause_complete(vap, EINVAL);
        }
    } else {
        /* To-do send  Quiet IE part of the AP pause */
        ieee80211_iterate_node(vap->iv_ic,ieee80211_pause_sta_iter,(void *)&itr_arg);
        ieee80211_vap_pause_complete(vap, EOK);
    }
    return EOK;
}

/*
 * pause complete notification.
 * to be called by power save module.
 */
void ieee80211_vap_pause_complete(ieee80211_vap_t vap, IEEE80211_STATUS status)
{
    ieee80211_vap_event evt;
    bool unpause_bss_node=false;
    struct ieee80211_node *bss_node;
/* No more data can go out from the ath layer for this vap */
    if (ieee80211_ic_enh_ind_rpt_is_set(vap->iv_ic)) {
    vap->iv_ic->ic_vap_pause_control(vap->iv_ic,vap,true);
    }
    /* Notify based on status */
    if (status == EOK) {
        evt.type = IEEE80211_VAP_PAUSED;
    } else {
        evt.type = IEEE80211_VAP_PAUSE_FAIL;

        /* Clear VAP pause flags before reporting pause failure.
         * Ideally these flags shouldn't have been set. Hang around with this code until the correct behavior is decided.
         * - Do we ignore the power save NULL data frame Tx failure. OR
         * - Do we pass the failure information to the requesting module and give it a chance to retry.
         */
        IEEE80211_VAP_LOCK(vap);
        if (vap->iv_pause_info.iv_pause_count != 0) {
            --vap->iv_pause_info.iv_pause_count;

            if (vap->iv_pause_info.iv_pause_count == 0) {
                ieee80211_vap_clear_pause(vap);
                atomic_set(&vap->iv_pause_info.iv_pause_beacon_count, 0);

                if (vap->iv_opmode  == IEEE80211_M_STA &&
                    (wlan_vdev_is_up(vap->vdev_obj) ==
                                       QDF_STATUS_SUCCESS)) {
                    unpause_bss_node=true;
                }
            }
        }
        IEEE80211_VAP_UNLOCK(vap);
        /* unpause the node the node */
        if (unpause_bss_node && vap->iv_bss) {
            bss_node =  ieee80211_try_ref_bss_node(vap);
            ieee80211node_unpause(bss_node);
            ieee80211_free_node(bss_node);
        }

    }
    ieee80211_vap_deliver_event( vap, &evt);
}

struct ieee80211_unpause_sta_arg {
    ieee80211_vap_t vap;
};

static void ieee80211_unpause_sta_iter(void *arg, struct ieee80211_node *ni)
{
    struct ieee80211_unpause_sta_arg *itr_arg = (struct ieee80211_unpause_sta_arg *)arg;
    if (itr_arg->vap == ni->ni_vap) {
         ieee80211node_unpause(ni);
    }
}

/*
 * unpause complete
 * to be called by power save module.
 */
void ieee80211_vap_unpause_complete(ieee80211_vap_t vap)
{
    ieee80211_vap_event evt;

    if (ieee80211_ic_enh_ind_rpt_is_set(vap->iv_ic)) {
        vap->iv_ic->ic_vap_pause_control(vap->iv_ic,vap,false);
    }
    IEEE80211_VAP_LOCK(vap);
    atomic_set(&vap->iv_pause_info.iv_pause_beacon_count, 0);
    IEEE80211_VAP_UNLOCK(vap);
    evt.type = IEEE80211_VAP_UNPAUSED;
    ieee80211_vap_deliver_event(vap, &evt);

}

static void ieee80211_vap_power_pause_event_handler (struct ieee80211vap *vap, ieee80211_sta_power_event *event, void *arg)
{
  if (event->type == IEEE80211_POWER_STA_PAUSE_COMPLETE) {
      if (event->status == IEEE80211_POWER_STA_STATUS_SUCCESS) {
          ieee80211_vap_pause_complete(vap, EOK);
      } else {
          ieee80211_vap_pause_complete(vap, EIO);
      }
  }
  if (event->type == IEEE80211_POWER_STA_UNPAUSE_COMPLETE) {
      ieee80211_vap_unpause_complete(vap);
  }

}
/**
 * unpause request.
 * @param vap    : handle to vap.
 * @param req_id : id of the requestor.
 * @return  returns EOK if it accepted the request and processes the request asynchronously and
 *    at the end of processing the request the VAP posts the
 *    IEEE80211_VAP_UNPAUSED event to all the registred event handlers.
 *    returns EINVAL if the VAP is in FULL SLEEP state (or) if vap is not  in pause (or)
 *    if no pause request is pending currently .
 *    returns EBUSY if the pause count not 0 (other modules still wat to keep it paused)
 */
int ieee80211_vap_unpause(ieee80211_vap_t vap, u_int16_t reqid )
{
    bool request_unpause=FALSE;

    ASSERT(vap->iv_opmode == IEEE80211_M_HOSTAP || vap->iv_opmode == IEEE80211_M_STA ||
           vap->iv_opmode == IEEE80211_M_BTAMP || vap->iv_opmode == IEEE80211_M_IBSS ||
           vap->iv_opmode == IEEE80211_M_MONITOR);

    IEEE80211_VAP_LOCK(vap);
    if (vap->iv_pause_info.iv_pause_count == 0) {
        IEEE80211_VAP_UNLOCK(vap);
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_POWER, "%s: not in paused state \n", __func__);
        return EINVAL;
    }
    ASSERT(ieee80211_vap_is_paused(vap));
    --vap->iv_pause_info.iv_pause_count;
    if (vap->iv_pause_info.iv_pause_count == 0) {
        request_unpause=TRUE;
        ieee80211_vap_clear_pause(vap);
    }
    IEEE80211_VAP_UNLOCK(vap);

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_POWER, "%s: pause count %d \n", __func__,vap->iv_pause_info.iv_pause_count);
    if (request_unpause == FALSE) {
        return EBUSY;
    }
    if (vap->iv_opmode  == IEEE80211_M_STA) {
        if (ieee80211_sta_power_unpause(vap) != EOK) {
            ieee80211_vap_unpause_complete(vap);
        }
    } else {
        struct ieee80211_unpause_sta_arg itr_arg;
        itr_arg.vap = vap;
        ieee80211_vap_unpause_complete(vap);
        /*
         * flush all the frames queued in to the node save queue
         */
        ieee80211_iterate_node(vap->iv_ic,ieee80211_unpause_sta_iter,(void *)&itr_arg);
    }
    return EOK;
}

/**
 * force unpause request.synchronously unpause the active traffic in the ath
 * and umac layers. no fake wakeup is performed.
 * @param vap    : handle to vap.
 * @param req_id : id of the requestor.
 * @return  EOK if request is processed.
 *    returns EBUSY if the force pause count not 0 (other modules still wat to keep it force paused)
 *    returns EINVAL if it is not in force paused state.
 */
int ieee80211_vap_force_unpause(ieee80211_vap_t vap, u_int16_t reqid )
{
    struct ieee80211_pause_sta_arg itr_arg;
    itr_arg.vap = vap;

    IEEE80211_VAP_LOCK(vap);
    if (vap->iv_pause_info.iv_force_pause_count == 0) {
        IEEE80211_VAP_UNLOCK(vap);
        return EINVAL;
    }
    ASSERT(ieee80211_vap_is_force_paused(vap));
    --vap->iv_pause_info.iv_force_pause_count;
    if ((vap->iv_pause_info.iv_force_pause_count) == 0) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_POWER, "%s \n", __func__);
        ieee80211_vap_clear_force_pause(vap);
        IEEE80211_VAP_UNLOCK(vap);
        if (vap->iv_opmode  == IEEE80211_M_STA) {
            ieee80211node_unpause(vap->iv_bss);
        } else {
            /*
             * flush all the frames queued in to the node save queue
             */
            ieee80211_iterate_node(vap->iv_ic,ieee80211_unpause_sta_iter,(void *)&itr_arg);
        }
        ieee80211_resgmr_lmac_vap_pause_control(vap, false);
        return EOK;
    }
    IEEE80211_VAP_UNLOCK(vap);
    return EBUSY;
}

/**
 * @ reset VAP pause state/counters.
 * ARGS :
 *  vap    : handle to vap.
 */
void ieee80211_vap_pause_reset(ieee80211_vap_t vap)
{
    IEEE80211_VAP_LOCK(vap);
    if (vap->iv_pause_info.iv_pause_count) {
        vap->iv_pause_info.iv_pause_count = 0;
        ieee80211_vap_clear_pause(vap);
        atomic_set(&vap->iv_pause_info.iv_pause_beacon_count, 0);
    }
    IEEE80211_VAP_UNLOCK(vap);
}

/**
 * @ request CSA on AP VAP.
 * ARGS :
 *  vap    : handle to vap.
 *  req_id : ID of the requesting module.
 * RETURNS:
 *  returns 0 if it accepted the request and processes the request asynchronously and
 *    at the end of processing the request the VAP posts the
 *    IEEE80211_VAP_UNPAUSED event to all the registred event handlers.
 *    The VAP will wait for configured number of beacons (configured via IEEE80211_CSA_BEACON_COUNT)
 *    to go out with CSA before sending the
 *    IEEE80211_VAP_CSA_COMPLETE event.
 *  returns EINVAL if the VAP is in FULL SLEEP state (or) if vap is in pauseed state.
 */
int ieee80211_vap_request_csa(ieee80211_vap_t vap, wlan_chan_t chan )
{
    return EINVAL;
}

struct ieee80211_saveq_iter_sta_arg {
    ieee80211_vap_t vap;
    ieee80211_vap_activity *activity;
};


static void ieee80211_saveq_info_sta_iter(void *arg, struct ieee80211_node *ni)
{
    struct ieee80211_saveq_iter_sta_arg *itr_arg =  (struct ieee80211_saveq_iter_sta_arg *)arg;
    ieee80211_vap_activity *activity = itr_arg->activity;
    ieee80211_node_saveq_info nodeq_info;
    if (itr_arg->vap == ni->ni_vap) {
        ieee80211_node_saveq_get_info(ni,&nodeq_info);
        activity->data_q_len += nodeq_info.data_count;
        activity->data_q_bytes += nodeq_info.data_len;
    }
}

/**
 * @ get activity info
 * ARGS:
 *  vap      : handle to vap.
 *  activity : info about the data transfer activity of a vap.
 *
 * RETURNS:
 */
void ieee80211_vap_get_activity(ieee80211_vap_t vap, ieee80211_vap_activity *activity, u_int8_t flags)
{
    ieee80211_node_saveq_info  nodeq_info;
    u_int32_t                  last_data_time = (u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(vap->iv_lastdata);
    u_int32_t                  now = (u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(OS_GET_TIMESTAMP());

    activity->msec_from_last_data = now - last_data_time;

    if (vap->iv_opmode  == IEEE80211_M_STA) {
        ieee80211_node_saveq_get_info(vap->iv_bss,&nodeq_info);
        activity->data_q_len = nodeq_info.data_count;
        activity->data_q_bytes = nodeq_info.data_len;
    }
    if (vap->iv_opmode  == IEEE80211_M_HOSTAP) {
        struct ieee80211_saveq_iter_sta_arg itr_arg;
        activity->data_q_len = 0;
        activity->data_q_bytes = 0;
        itr_arg.vap = vap;
        itr_arg.activity = activity;
        ieee80211_iterate_node(vap->iv_ic,ieee80211_saveq_info_sta_iter,&itr_arg);
    }
    wlan_vdev_get_txrx_activity(vap->vdev_obj, activity);
}

int
ieee80211_vap_standby_bss(ieee80211_vap_t vap)
{
    /* Notify channel change start */
    IEEE80211_DELIVER_EVENT_CHANNEL_CHANGE(vap, NULL);

    if (vap->iv_opmode != IEEE80211_M_HOSTAP)
        return 0;

    return wlan_mlme_stop_bss(vap,
                              WLAN_MLME_STOP_BSS_F_SEND_DEAUTH         |
                              WLAN_MLME_STOP_BSS_F_CLEAR_ASSOC_STATE   |
                              WLAN_MLME_STOP_BSS_F_STANDBY             |
                              WLAN_MLME_STOP_BSS_F_NO_RESET);
}

 void
 ieee80211_ic_unpause(struct ieee80211com *ic)
 {
    struct ieee80211vap *vap;

      /* FIXME: Force pause/unpause feature is currently disabled for DCS path
       * Need to verify before enabling it for DCS path */
      if (ic->cw_inter_found) {
        return ;
      }
     TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
     if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
             ieee80211_vap_force_unpause(vap, 0);
         }
        }
    }


 int
 ieee80211_ic_pause(struct ieee80211com *ic)
 {
     struct ieee80211vap *vap;

      /* FIXME: Force pause/unpause feature is currently disabled for DCS path
       * Need to verify before enabling it for DCS path */
      if (ic->cw_inter_found) {
        return 0;
      }
     TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
         if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
             if  (ieee80211_vap_can_pause(vap) == false) {
                 return  -1;
             }
             if (!ieee80211_vap_is_force_paused(vap)) {
                 ieee80211_vap_force_pause(vap, 0);
             }
         }

     }
     return 0;
    }


#endif
