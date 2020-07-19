/*
 * Copyright (c) 2011,2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 * Copyright (c) 2010, Atheros Communications Inc.
 * All Rights Reserved.
 */

#include "ieee80211_mlme_priv.h"    /* Private to MLME module */
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_scan_ucfg_api.h>
#include <ieee80211_objmgr_priv.h>
#include <wlan_utility.h>

#if UMAC_SUPPORT_SW_BMISS

/*
* requestor ID is 1 to 255.
* 0 is allocated for scanner.
* the requestor id occupies lower 8 bits and the remaining 24 bits are for
* filled with a magic number used for sanity check.
* allows multiple modules enable/disable SW bmiss timer.
* allocates abit for each module (requestor) and
* maintains a bit map of disable requests.
*/  

#define REQUESTOR_ID_MASK          0xff  
#define REQUESTOR_ID_MAGIC_PREFIX  0x0abcde00
#define REQUESTOR_ID_SCAN          0x0  
#define REQUESTOR_ID_SCAN_NAME     "SCANNER" 

static void mlme_sta_calculate_sleep_timer_val(struct ieee80211vap *vap)
{
    struct ieee80211com           *ic = vap->iv_ic;
    struct ieee80211_mlme_priv    *mlme_priv = vap->iv_mlme_priv;

    mlme_priv->im_sleep_mode = wlan_get_powersave(vap);
    if (mlme_priv->im_sleep_mode == IEEE80211_PWRSAVE_NONE) {
       mlme_priv->im_sw_bmiss_timer_interval = mlme_priv->im_sw_bmiss_timer_interval_normal;
    } else {
       mlme_priv->im_sw_bmiss_timer_interval_sleep = mlme_priv->im_sw_bmiss_timer_interval_normal * ic->ic_lintval;
    }
}

static void mlme_sta_scan_event_handler(struct wlan_objmgr_vdev *vdev, struct scan_event *event, void *arg)
{

    struct ieee80211vap *originator = wlan_vdev_get_mlme_ext_obj(vdev);
    /*
     * stop sw bmiss timer when off channel and restart 
     * it when back on to bss channel.
     */
    struct ieee80211vap *vap = (struct ieee80211vap *) arg;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
                      "%s: orig=%08p vap=%08p scan_id %d event %d reason %d\n", 
                      __func__, 
                      originator, vap,
                      event->scan_id, event->type, event->reason);

    /*
     * Handle scan events regardless of which module requested the scan.
     * SCAN_EVENT_TYPE_DEQUEUED can be safely ignored.
     */
    switch(event->type) {
    case SCAN_EVENT_TYPE_COMPLETED:
    case SCAN_EVENT_TYPE_BSS_CHANNEL:
        if (wlan_vdev_is_up(vap->vdev_obj) == QDF_STATUS_SUCCESS) {
            mlme_sta_swbmiss_timer_enable(vap,
                REQUESTOR_ID_MAGIC_PREFIX | REQUESTOR_ID_SCAN ); 
        }
        break;
    case SCAN_EVENT_TYPE_FOREIGN_CHANNEL:
        if (wlan_vdev_is_up(vap->vdev_obj) == QDF_STATUS_SUCCESS) {
           mlme_sta_swbmiss_timer_disable(vap,
                REQUESTOR_ID_MAGIC_PREFIX | REQUESTOR_ID_SCAN ); 
        }
    default:
        break;
    }
}

void mlme_sta_swbmiss_timer_restart(struct ieee80211vap *vap)
{
    struct ieee80211_mlme_priv    *mlme_priv = vap->iv_mlme_priv;

    if (!wlan_is_hwbeaconproc_active(vap) && ieee80211_vap_sw_bmiss_is_set(vap) &&
            mlme_priv->im_sw_bmiss_disable_bitmap  == 0 ) {
        OS_SET_TIMER(&mlme_priv->im_sw_bmiss_timer, mlme_priv->im_sw_bmiss_timer_interval);
    }
}

void mlme_sta_swbmiss_timer_start(struct ieee80211vap *vap)
{
    struct ieee80211_mlme_priv    *mlme_priv = vap->iv_mlme_priv;

    if (!wlan_is_hwbeaconproc_active(vap) && ieee80211_vap_sw_bmiss_is_set(vap) &&
            mlme_priv->im_sw_bmiss_disable_bitmap  == 0 ) {
        OS_SET_TIMER(&mlme_priv->im_sw_bmiss_timer, mlme_priv->im_sw_bmiss_timer_interval);
    }
}

void mlme_sta_swbmiss_timer_stop(struct ieee80211vap *vap)
{
    struct ieee80211_mlme_priv    *mlme_priv = vap->iv_mlme_priv;
    OS_CANCEL_TIMER(&mlme_priv->im_sw_bmiss_timer);
}

static OS_TIMER_FUNC(mlme_sta_swbmiss_timer_handler)
{
    struct ieee80211vap            *vap;

    OS_GET_TIMER_ARG(vap, struct ieee80211vap *);
    /*
     * call the common beacon miss handler with non NULL arg (first param)
     * to indicate that the function is being called from SW beacon miss timer.
     */
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s: SW Beacon miss!!\n", __func__);
    ieee80211_vap_iter_beacon_miss((void *)vap,vap); 
}

static void mlme_power_event_handler (struct ieee80211vap *vap, ieee80211_sta_power_event *event, void *arg)
{
    struct ieee80211_mlme_priv    *mlme_priv = vap->iv_mlme_priv;
  
    switch(event->type) 
    {
    case IEEE80211_POWER_STA_SLEEP: 
        /* re calculate the bmiss timer and restart the timer */
        if (wlan_get_powersave(vap)  != mlme_priv->im_sleep_mode) {
            mlme_sta_calculate_sleep_timer_val(vap);
        }
        if (mlme_priv->im_sw_bmiss_timer_interval != mlme_priv->im_sw_bmiss_timer_interval_sleep) {
            mlme_priv->im_sw_bmiss_timer_interval = mlme_priv->im_sw_bmiss_timer_interval_sleep;
            mlme_sta_swbmiss_timer_restart(vap);
        }
        break;
    case IEEE80211_POWER_STA_AWAKE: 
        if (mlme_priv->im_sw_bmiss_timer_interval != mlme_priv->im_sw_bmiss_timer_interval_normal) {
            mlme_priv->im_sw_bmiss_timer_interval = mlme_priv->im_sw_bmiss_timer_interval_normal;
            mlme_sta_swbmiss_timer_restart(vap);
        }
        break;
    case IEEE80211_POWER_STA_PAUSE_COMPLETE:
        break;
    case IEEE80211_POWER_STA_UNPAUSE_COMPLETE:
        break;
    default:
        break;
    }

}

void mlme_sta_swbmiss_timer_attach(struct ieee80211vap *vap)
{
    struct ieee80211com           *ic = vap->iv_ic;
    struct ieee80211_mlme_priv    *mlme_priv = vap->iv_mlme_priv;
    QDF_STATUS status;
    
    OS_INIT_TIMER(ic->ic_osdev, &mlme_priv->im_sw_bmiss_timer, 
                  mlme_sta_swbmiss_timer_handler, vap, QDF_TIMER_TYPE_WAKE_APPS);
    /*
     * convert SW bmiss time out from TUs to msec 
     */
    mlme_priv->im_sw_bmiss_timer_interval = (ic->ic_bmisstimeout << 10)/1000;
    mlme_priv->im_sw_bmiss_timer_interval_normal =  mlme_priv->im_sw_bmiss_timer_interval;
    status = ucfg_scan_register_event_handler(wlan_vap_get_pdev(vap),
            mlme_sta_scan_event_handler,(void *) vap);
    if (status) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
                          "%s: wlan_scan_register_event_handler() failed handler=%08p,%08p rc=%08X\n",
                          __func__, mlme_sta_scan_event_handler, vap, status);
        scan_err("scan_register_event_handler failed with status: %d", status);
    }
    ieee80211_sta_power_register_event_handler(vap, mlme_power_event_handler,vap);

    mlme_priv->im_sw_bmiss_id_bitmap |=  1; /* scanner takes id 0 (bit 1) of the id bitmap */
    snprintf((char *)&mlme_priv->im_sw_bmiss_id_name[0][0],
               MLME_SWBMISS_MAX_REQ_NAME,"%s","SCANNER");   
}

void mlme_sta_swbmiss_timer_detach(struct ieee80211vap *vap)
{
    int i;
    struct ieee80211_mlme_priv    *mlme_priv = vap->iv_mlme_priv;
    
    mlme_sta_swbmiss_timer_stop(vap);
    ucfg_scan_unregister_event_handler(wlan_vap_get_pdev(vap), mlme_sta_scan_event_handler,(void *) vap);
    (void)ieee80211_sta_power_unregister_event_handler(vap,mlme_power_event_handler,vap);
    OS_FREE_TIMER(&((struct ieee80211_mlme_priv *)vap->iv_mlme_priv)->im_sw_bmiss_timer);

    if (mlme_priv->im_sw_bmiss_disable_bitmap) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
                          "%s:disable bitmap is 0x%x \n",__func__,
                          mlme_priv->im_sw_bmiss_disable_bitmap );
    }
    for(i=0;i<MLME_SWBMISS_MAX_REQUESTORS;++i) {
       if (((1<<i)& mlme_priv->im_sw_bmiss_id_bitmap)) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
                          "%s:id %d name (%s) is still allocated \n",__func__, i,
                          mlme_priv->im_sw_bmiss_id_name[i] );
       }
    }
}


 
/**
 * allocate a requestor id to enable/disable sw bmiss 
 * @param vap        : vap handle
 * @param name       :  friendly name of the requestor(for debugging). 
 * @return unique id for the requestor if success , returns 0 if failed. 
 */
u_int32_t mlme_sta_swbmiss_timer_alloc_id(struct ieee80211vap *vap, int8_t *name)
{
    struct ieee80211_mlme_priv    *mlme_priv = vap->iv_mlme_priv;
    int i;
    IEEE80211_VAP_LOCK(vap);
    for(i=1;i<MLME_SWBMISS_MAX_REQUESTORS;++i) {
        if (((1<<i)& mlme_priv->im_sw_bmiss_id_bitmap) == 0) {
            mlme_priv->im_sw_bmiss_id_bitmap |=  (1<<i);
            snprintf((char *)&mlme_priv->im_sw_bmiss_id_name[i][0],
               MLME_SWBMISS_MAX_REQ_NAME,"%s",name);   
            IEEE80211_VAP_UNLOCK(vap);
            return (i | REQUESTOR_ID_MAGIC_PREFIX);
        }
    }
    IEEE80211_VAP_UNLOCK(vap);
    return 0;
}

/**
 * free a requestor id to enable/disable sw bmiss 
 * @param vap           : vap handle
 * @param requestor_id  : id of requestor 
 * @return EOK if success and error code if failed. 
 */
int mlme_sta_swbmiss_timer_free_id(struct ieee80211vap *vap, u_int32_t requestor_id)
{
    struct ieee80211_mlme_priv    *mlme_priv = vap->iv_mlme_priv;
    u_int32_t id;
    if ( (requestor_id & ~REQUESTOR_ID_MASK) != REQUESTOR_ID_MAGIC_PREFIX) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
                          "%s: invalid id 0x%x \n",__func__, requestor_id);
        return EINVAL;
    }
    id = (requestor_id & REQUESTOR_ID_MASK);
    if ( id == 0 || id >= MLME_SWBMISS_MAX_REQUESTORS) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
                          "%s: id out of range 0x%x \n",__func__, requestor_id);
        return EINVAL;
    }
    IEEE80211_VAP_LOCK(vap);
    mlme_priv->im_sw_bmiss_id_bitmap &=  ~(1<<id);
    mlme_priv->im_sw_bmiss_id_name[id][0] = 0;
    IEEE80211_VAP_UNLOCK(vap);

    return EOK;
}

/**
 * enable sw bmiss timer
 * @param vap           : vap handle
 * @param requestor_id  : id of requestor 
 * @return EOK if success and error code if failed. 
 */
int mlme_sta_swbmiss_timer_enable(struct ieee80211vap *vap, u_int32_t requestor_id)
{
    struct ieee80211_mlme_priv    *mlme_priv = vap->iv_mlme_priv;
    u_int32_t id;
    u_int32_t cur_disable_mask;
    if ( (requestor_id & ~REQUESTOR_ID_MASK) != REQUESTOR_ID_MAGIC_PREFIX) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
                          "%s: invalid id 0x%x \n",__func__, requestor_id);
        return EINVAL;
    }
    id = (requestor_id & REQUESTOR_ID_MASK);
    if (id >= MLME_SWBMISS_MAX_REQUESTORS) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
                          "%s: id out of range 0x%x \n",__func__, requestor_id);
        return EINVAL;
    }
    IEEE80211_VAP_LOCK(vap);
    cur_disable_mask = mlme_priv->im_sw_bmiss_disable_bitmap;
    if ( (cur_disable_mask & (1<<id)) == 0 ) {
        /* already enabled */
        IEEE80211_VAP_UNLOCK(vap);
        return EOK;
    }
    mlme_priv->im_sw_bmiss_disable_bitmap &= ~(1<<id);
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s: swbmiss enabled by %s curmask 0x%x  \n",
                          __func__, mlme_priv->im_sw_bmiss_id_name[id], mlme_priv->im_sw_bmiss_disable_bitmap);
    if (mlme_priv->im_sw_bmiss_disable_bitmap  == 0 ) {
        /* nobody wants it to be disabled */
       mlme_sta_swbmiss_timer_restart(vap);
    }
    IEEE80211_VAP_UNLOCK(vap);

    return EOK;
}

/**
 * disable sw bmiss timer
 * @param vap           : vap handle
 * @param requestor_id  : id of requestor 
 * @return EOK if success and error code if failed. 
 */
int mlme_sta_swbmiss_timer_disable(struct ieee80211vap *vap, u_int32_t requestor_id)
{
    struct ieee80211_mlme_priv    *mlme_priv = vap->iv_mlme_priv;
    u_int32_t id;
    u_int32_t cur_disable_mask;
    if ( (requestor_id & ~REQUESTOR_ID_MASK) != REQUESTOR_ID_MAGIC_PREFIX) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
                          "%s: invalid id 0x%x \n",__func__, requestor_id);
        return EINVAL;
    }
    id = (requestor_id & REQUESTOR_ID_MASK);
    if (id >= MLME_SWBMISS_MAX_REQUESTORS) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
                          "%s: id out of range 0x%x \n",__func__, requestor_id);
        return EINVAL;
    }
    IEEE80211_VAP_LOCK(vap);
    cur_disable_mask = mlme_priv->im_sw_bmiss_disable_bitmap;
    if ( (cur_disable_mask & (1<<id))) {
        /* already disabled */
        IEEE80211_VAP_UNLOCK(vap);
        return EOK;
    }
    mlme_priv->im_sw_bmiss_disable_bitmap |= (1<<id);
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s: swbmiss disabled by %s curmask 0x%x  \n",
                          __func__, mlme_priv->im_sw_bmiss_id_name[id], mlme_priv->im_sw_bmiss_disable_bitmap);
    if (cur_disable_mask  == 0 ) {
        /* currently it is enabled, so disable it */
       mlme_sta_swbmiss_timer_stop(vap);
    }
    IEEE80211_VAP_UNLOCK(vap);

    return EOK;
}


/*
* print the SW bmiss timer status.
*/
void mlme_sta_swbmiss_timer_print_status(struct ieee80211vap *vap)
{
    int i;
    struct ieee80211_mlme_priv    *mlme_priv = vap->iv_mlme_priv;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s: swbmiss disable_bitmap 0x%x  \n",
                          __func__,mlme_priv->im_sw_bmiss_disable_bitmap);
    for(i=0;i<MLME_SWBMISS_MAX_REQUESTORS;++i) {
        if ((1<<i)& mlme_priv->im_sw_bmiss_id_bitmap ) {
    	  IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s: swbmiss %s by '%s' \n",__func__,
                           (mlme_priv->im_sw_bmiss_disable_bitmap) & (1 << i) ? "disabled" : "enabled",
                            mlme_priv->im_sw_bmiss_id_name[i]);
        }
    }
}

#endif
