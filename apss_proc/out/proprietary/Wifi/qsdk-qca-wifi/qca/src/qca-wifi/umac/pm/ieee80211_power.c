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

#include <osdep.h>

#include <wlan_utility.h>
#include "ieee80211_power_priv.h"

#define PM_ARBITER_REQUESTOR_ID_MASK          0x0fff
#define PM_ARBITER_REQUESTOR_ID_MAGIC_PREFIX  0x2fedc000
#define PM_ARBITER_MAX_REQUESTORS  12
#define PM_ARBITER_MAX_REQ_NAME   16

struct ieee80211_power_arbiter_requestor_info {
    u_int8_t    id_name[PM_ARBITER_MAX_REQ_NAME]; /* names of the requestors */
    IEEE80211_POWER_ARBITER_TYPE arbiter_type;
} ;

struct ieee80211_power_arbiter_info {
    u_int32_t   id_bitmap;       /* power arbiter control id bitmaps */
    u_int32_t   disable_bitmap;  /* power arbiter control id enable bitmaps */
    u_int32_t   sleep_bitmap;    /* power arbiter sleep requests bitmap */
    struct ieee80211_power_arbiter_requestor_info requestor[PM_ARBITER_MAX_REQUESTORS];
};

struct ieee80211_power {
    spinlock_t          pm_state_lock;  /* lock for the data */
    u_int8_t            pm_sleep_count; /* how many modules want to put the vap to network sleep */
    u_int8_t            pm_nwsleep_event_sent; /* flag indicating network sleep event sent */
    struct ieee80211_power_arbiter_info pm_arbiter;
};

static void ieee80211_power_vap_event_handler(ieee80211_vap_t     vap, 
                                              ieee80211_vap_event *event, 
                                              void                *arg);

static int __ieee80211_power_enter_nwsleep(struct ieee80211vap *vap);
static int __ieee80211_power_exit_nwsleep(struct ieee80211vap *vap);

static void ieee80211_power_arbiter_vattach(struct ieee80211vap *vap)
{
    vap->iv_power->pm_arbiter.id_bitmap = 0x0;
    vap->iv_power->pm_arbiter.disable_bitmap = 0xffffffff;
    vap->iv_power->pm_arbiter.sleep_bitmap = 0x0;
    return;
}

static void ieee80211_power_arbiter_vdetach(struct ieee80211vap *vap)
{
    vap->iv_power->pm_arbiter.id_bitmap = 0x0;
    vap->iv_power->pm_arbiter.disable_bitmap = 0xffffffff;
    vap->iv_power->pm_arbiter.sleep_bitmap = 0x0;
    return;
}

void _ieee80211_power_detach(struct ieee80211com *ic)
{
    ic->ic_power_sta_set_pspoll = NULL;
    ic->ic_power_sta_set_pspoll_moredata_handling = NULL;
    ic->ic_power_sta_get_pspoll = NULL;
    ic->ic_power_sta_get_pspoll_moredata_handling = NULL;
    ic->ic_power_set_mode = NULL;
    ic->ic_power_get_mode = NULL;
    ic->ic_power_get_apps_state = NULL;
    ic->ic_power_set_inactive_time = NULL;
    ic->ic_power_get_inactive_time = NULL;
    ic->ic_power_force_sleep = NULL;
    ic->ic_power_set_ips_pause_notif_timeout = NULL;
    ic->ic_power_get_ips_pause_notif_timeout = NULL;
    ic->ic_power_detach = NULL;
    ic->ic_power_vattach = NULL;
    ic->ic_power_vdetach = NULL;
}

void
_ieee80211_power_vattach(struct ieee80211vap *vap,
                        int fullsleepEnable,
                        u_int32_t sleepTimerPwrSaveMax,
                        u_int32_t sleepTimerPwrSave,
                        u_int32_t sleepTimePerf,
                        u_int32_t inactTimerPwrsaveMax,
                        u_int32_t inactTimerPwrsave,
                        u_int32_t inactTimerPerf,
                        u_int32_t smpsDynamic,
                        u_int32_t pspollEnabled)
{
    osdev_t                  os_handle = vap->iv_ic->ic_osdev;

    vap->iv_power = (ieee80211_power_t)OS_MALLOC(os_handle,sizeof(struct ieee80211_power),0);

    if (!vap->iv_power) {
        return;
    }

    OS_MEMZERO(vap->iv_power, sizeof(struct ieee80211_power));
    spin_lock_init(&vap->iv_power->pm_state_lock);
    switch (vap->iv_opmode) {
    case IEEE80211_M_HOSTAP:
        ieee80211_ap_power_vattach(vap);
        vap->iv_pwrsave_sta = NULL;
        vap->iv_pwrsave_smps = NULL; 
        break;
    case IEEE80211_M_STA:
        if( (!IEEE80211_VAP_IS_WDS_ENABLED(vap)) || ( vap->iv_ic->ic_opmode == IEEE80211_M_STA ) )  {
            vap->iv_pwrsave_sta = ieee80211_sta_power_vattach(vap,
                                                          fullsleepEnable,
                                                          sleepTimerPwrSaveMax,
                                                          sleepTimerPwrSave,
                                                          sleepTimePerf,
                                                          inactTimerPwrsaveMax,
                                                          inactTimerPwrsave,
                                                          inactTimerPerf,
                                                          pspollEnabled);
            vap->iv_pwrsave_smps =  ieee80211_pwrsave_smps_attach(vap,smpsDynamic);
        } else {
            vap->iv_pwrsave_sta = NULL;
            vap->iv_pwrsave_smps = NULL; 
        }
        break;
    default:
        vap->iv_pwrsave_sta = NULL;
        vap->iv_pwrsave_smps = NULL; 
        break;
    }
    ieee80211_vap_register_event_handler(vap,ieee80211_power_vap_event_handler,(void *)NULL );
    ieee80211_power_arbiter_vattach(vap);
}

void
_ieee80211_power_vdetach(struct ieee80211vap *vap)
{
    ieee80211_vap_unregister_event_handler(vap,ieee80211_power_vap_event_handler,(void *)NULL );
    switch (vap->iv_opmode) {
    case IEEE80211_M_HOSTAP:
        ieee80211_ap_power_vdetach(vap);
        break;
    case IEEE80211_M_STA:
        if(vap->iv_pwrsave_sta ) {
            ieee80211_sta_power_vdetach(vap->iv_pwrsave_sta);
        }
        if(vap->iv_pwrsave_smps) {
            ieee80211_pwrsave_smps_detach(vap->iv_pwrsave_smps);
        }
        vap->iv_pwrsave_sta = NULL;
        vap->iv_pwrsave_smps = NULL;
        break;
    default:
        break;
    }

    if (vap->iv_power) {
        spin_lock_destroy(&vap->iv_power->pm_state_lock);
        ieee80211_power_arbiter_vdetach(vap);
        OS_FREE(vap->iv_power);
    }

}

void _ieee80211_power_attach(struct ieee80211com *ic)
{
#if UMAC_SUPPORT_STA_POWERSAVE
    ic->ic_power_sta_set_pspoll = ieee80211_sta_power_set_pspoll;
    ic->ic_power_sta_set_pspoll_moredata_handling = ieee80211_sta_power_set_pspoll_moredata_handling;
    ic->ic_power_sta_get_pspoll = ieee80211_sta_power_get_pspoll;
    ic->ic_power_sta_get_pspoll_moredata_handling = ieee80211_sta_power_get_pspoll_moredata_handling;
    ic->ic_power_set_mode = ieee80211_set_powersave;
    ic->ic_power_get_mode = ieee80211_get_powersave;
    ic->ic_power_get_apps_state = ieee80211_get_apps_powersave_state;
    ic->ic_power_set_inactive_time = ieee80211_set_powersave_inactive_time;
    ic->ic_power_get_inactive_time = ieee80211_get_powersave_inactive_time;
    ic->ic_power_force_sleep = ieee80211_pwrsave_force_sleep;
    ic->ic_power_set_ips_pause_notif_timeout = ieee80211_power_set_ips_pause_notif_timeout;
    ic->ic_power_get_ips_pause_notif_timeout = ieee80211_power_get_ips_pause_notif_timeout;

    ic->ic_power_sta_tx_start = _ieee80211_sta_power_tx_start;
    ic->ic_power_sta_tx_end = _ieee80211_sta_power_tx_end;
    ic->ic_power_sta_pause = _ieee80211_sta_power_pause;
    ic->ic_power_sta_unpause = _ieee80211_sta_power_unpause;
    ic->ic_power_sta_send_keepalive = _ieee80211_sta_power_send_keepalive;
    ic->ic_power_sta_register_event_handler = _ieee80211_sta_power_register_event_handler;
    ic->ic_power_sta_unregister_event_handler = _ieee80211_sta_power_unregister_event_handler;
    ic->ic_power_sta_event_tim = _ieee80211_sta_power_event_tim;
    ic->ic_power_sta_event_dtim = _ieee80211_sta_power_event_dtim;
#endif

    ic->ic_power_detach = _ieee80211_power_detach;
    ic->ic_power_vattach = _ieee80211_power_vattach;
    ic->ic_power_vdetach = _ieee80211_power_vdetach;
}

void ieee80211_power_class_attach(struct ieee80211com *ic)
{
    ic->ic_power_attach = _ieee80211_power_attach;
}

void
ieee80211_set_uapsd_flags(struct ieee80211vap *vap, u_int8_t flags)
{
    vap->iv_uapsd = (u_int8_t) (flags & WME_CAPINFO_UAPSD_ALL);
}

u_int8_t
ieee80211_get_uapsd_flags(struct ieee80211vap *vap)
{
    return (u_int8_t) vap->iv_uapsd;
}

void
ieee80211_set_wmm_power_save(struct ieee80211vap *vap, u_int8_t enable)
{
    vap->iv_wmm_power_save = enable;
}

u_int8_t
ieee80211_get_wmm_power_save(struct ieee80211vap *vap)
{
    return (u_int8_t) vap->iv_wmm_power_save;
}


/*
 * allows multiple modules call this API.
 * to enter network sleep.
 */
static int
__ieee80211_power_enter_nwsleep(struct ieee80211vap *vap)
{
    ieee80211_power_t power_handle = vap->iv_power;
    ieee80211_vap_event    evt;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_POWER, "%s sleep_count %d \n", __func__,power_handle->pm_sleep_count );
    /*
     * ignore any requests while the vap is not ready.
     */
    spin_lock(&power_handle->pm_state_lock);
    if (wlan_vdev_is_up(vap->vdev_obj) != QDF_STATUS_SUCCESS) {
        spin_unlock(&power_handle->pm_state_lock);
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_POWER, "%s vap is not ready \n", __func__);
        return EINVAL;
    }

    power_handle->pm_sleep_count++;
    /* if this is the first request put the vap to network sleep */
    if (power_handle->pm_sleep_count == 1) {
        spin_unlock(&power_handle->pm_state_lock);
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_POWER, "%s \n", __func__);
        evt.type = IEEE80211_VAP_NETWORK_SLEEP;
        ieee80211_vap_deliver_event(vap, &evt);
        if (!ieee80211_resmgr_exists(vap->iv_ic)) {
            vap->iv_ic->ic_pwrsave_set_state(vap->iv_ic, IEEE80211_PWRSAVE_NETWORK_SLEEP);
        }
    } else {
        spin_unlock(&power_handle->pm_state_lock);
    }

    return EOK;

}


/*
 * allows multiple modules call this API.
 * to exit network sleep.
 */
static int
__ieee80211_power_exit_nwsleep(struct ieee80211vap *vap)
{
    ieee80211_power_t power_handle = vap->iv_power;
    ieee80211_vap_event    evt;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_POWER, "%s sleep_count %d \n", __func__,power_handle->pm_sleep_count );
    /*
     * ignore any requests while the vap is not ready.
     */
    spin_lock(&power_handle->pm_state_lock);
    if (wlan_vdev_is_up(vap->vdev_obj) != QDF_STATUS_SUCCESS) {
        spin_unlock(&power_handle->pm_state_lock);
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_POWER, "%s vap is not ready \n", __func__);
        return EINVAL;
    }

    power_handle->pm_sleep_count--;
    /* if this is the first request put the vap to network sleep */
    if (power_handle->pm_sleep_count == 0) {
        power_handle->pm_nwsleep_event_sent = 0;
        spin_unlock(&power_handle->pm_state_lock);
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_POWER, "%s \n", __func__);
        evt.type = IEEE80211_VAP_ACTIVE;
        ieee80211_vap_deliver_event(vap, &evt);
        if (!ieee80211_resmgr_exists(vap->iv_ic)) {
            vap->iv_ic->ic_pwrsave_set_state(vap->iv_ic, IEEE80211_PWRSAVE_AWAKE);
        }
    } else {
        spin_unlock(&power_handle->pm_state_lock);
    }

    return EOK;

}

static void ieee80211_power_vap_event_handler(ieee80211_vap_t     vap, 
                                              ieee80211_vap_event *event, 
                                              void                *arg)
{
    ieee80211_power_t power_handle = vap->iv_power;

    switch(event->type) {

    case IEEE80211_VAP_FULL_SLEEP:
    case IEEE80211_VAP_DOWN:
        spin_lock(&power_handle->pm_state_lock);
        power_handle->pm_sleep_count=0;
        spin_unlock(&power_handle->pm_state_lock);
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_POWER, "%s VAP_DOWN , so reset the state \n", __func__);
        break;
    default:
        break;
    }
    if (vap->iv_pwrsave_sta) {
        ieee80211_sta_power_vap_event_handler(vap->iv_pwrsave_sta, event); 
    }

}

#if UMAC_SUPPORT_STA_POWERSAVE

static INLINE void
ieee80211_pwr_arbiter_set_sleep(struct ieee80211vap *vap, u_int32_t requestor_id)
{
    struct ieee80211_power      *pm_priv = vap->iv_power;
    struct ieee80211_power_arbiter_info *pm_arbiter = &pm_priv->pm_arbiter;
    u_int32_t id = (requestor_id & PM_ARBITER_REQUESTOR_ID_MASK);

    pm_arbiter->sleep_bitmap |= (1<<id);

    return;
}

static INLINE void
ieee80211_pwr_arbiter_clear_sleep(struct ieee80211vap *vap, u_int32_t requestor_id)
{
    struct ieee80211_power      *pm_priv = vap->iv_power;
    struct ieee80211_power_arbiter_info *pm_arbiter = &pm_priv->pm_arbiter;
    u_int32_t id = (requestor_id & PM_ARBITER_REQUESTOR_ID_MASK);

    pm_arbiter->sleep_bitmap &= ~(1<<id);

    return;
}

static INLINE bool
ieee80211_pwr_arbiter_requestor_id_enabled(struct ieee80211_power_arbiter_info *pm_arbiter,
        u_int32_t requestor_id)
{
    u_int32_t id = (requestor_id & PM_ARBITER_REQUESTOR_ID_MASK);

    if (((1<<id) & pm_arbiter->disable_bitmap) == 0) {
        return true;
    }
    else {
        return false;
    }
}

static INLINE bool
ieee80211_pwr_arbiter_can_sleep(struct ieee80211vap *vap, ieee80211_power_t power_handle)
{
    return true;
}

/**
 * requestor ready to enter network sleep
 * @param vap           : vap handle
 * @param requestor_id  : id of requestor
 * @return EOK if success and error code if failed.
 */
int
ieee80211_power_arbiter_enter_nwsleep(struct ieee80211vap *vap, u_int32_t requestor_id)
{
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_POWER, "%s - requestor_id = 0x%08x\n", __func__, requestor_id);
    return __ieee80211_power_enter_nwsleep(vap);
}

/**
 * requestor ready to exit network sleep
 * @param vap           : vap handle
 * @param requestor_id  : id of requestor
 * @return EOK if success and error code if failed.
 */
int ieee80211_power_arbiter_exit_nwsleep(struct ieee80211vap *vap, u_int32_t requestor_id)
{
    return __ieee80211_power_exit_nwsleep(vap);
}

/**
 * allocate a requestor id to enter/exit network sleep
 * @param vap        : vap handle
 * @param name       :  friendly name of the requestor(for debugging).
 * @return unique id for the requestor if success , returns 0 if failed.
 */
u_int32_t
ieee80211_power_arbiter_alloc_id(struct ieee80211vap *vap,
        const char *requestor_name, IEEE80211_POWER_ARBITER_TYPE arbiter_type)
{
    struct ieee80211_power      *pm_priv = vap->iv_power;
    struct ieee80211_power_arbiter_info *pm_arbiter = &pm_priv->pm_arbiter;
    int i;

    if (arbiter_type <= IEEE80211_PWR_ARBITER_TYPE_INVALID ||
            arbiter_type >= IEEE80211_PWR_ARBITER_TYPE_MAX) {
        return 0;
    }

    spin_lock(&vap->iv_lock);
    for(i=1;i<PM_ARBITER_MAX_REQUESTORS;++i) {
        if (((1<<i) & pm_arbiter->id_bitmap) == 0) {
            pm_arbiter->id_bitmap |=  (1<<i);
            snprintf((char *)&pm_arbiter->requestor[i].id_name[0],
                    PM_ARBITER_MAX_REQ_NAME, "%s", requestor_name);
            pm_arbiter->requestor[i].arbiter_type = arbiter_type;
            spin_unlock(&vap->iv_lock);
            return (i | PM_ARBITER_REQUESTOR_ID_MAGIC_PREFIX);
        }
    }
    spin_unlock(&vap->iv_lock);

    return 0;
}

/**
 * free a requestor id to enter/exit network sleep
 * @param vap           : vap handle
 * @param requestor_id  : id of requestor
 * @return EOK if success and error code if failed.
 */
int
ieee80211_power_arbiter_free_id(struct ieee80211vap *vap, u_int32_t requestor_id)
{
    struct ieee80211_power      *pm_priv = vap->iv_power;
    struct ieee80211_power_arbiter_info *pm_arbiter = &pm_priv->pm_arbiter;
    u_int32_t id;

    if ( (requestor_id & ~PM_ARBITER_REQUESTOR_ID_MASK) != PM_ARBITER_REQUESTOR_ID_MAGIC_PREFIX) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
                          "%s: invalid id 0x%x \n",__func__, requestor_id);
        return EINVAL;
    }
    id = (requestor_id & PM_ARBITER_REQUESTOR_ID_MASK);
    if ( id == 0 || id >= PM_ARBITER_MAX_REQUESTORS) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
                          "%s: id out of range 0x%x \n",__func__, requestor_id);
        return EINVAL;
    }
    spin_lock(&vap->iv_lock);
    pm_arbiter->id_bitmap &= ~(1<<id);
    pm_arbiter->requestor[id].id_name[0] = 0;
    pm_arbiter->requestor[id].arbiter_type = IEEE80211_PWR_ARBITER_TYPE_INVALID;
    spin_unlock(&vap->iv_lock);

    return EOK;
}

/**
 * enable arbitration for this requestor
 * @param vap           : vap handle
 * @param requestor_id  : id of requestor
 * @return EOK if success and error code if failed.
 */
int
ieee80211_power_arbiter_enable(struct ieee80211vap *vap, u_int32_t requestor_id)
{
    struct ieee80211_power      *pm_priv = vap->iv_power;
    struct ieee80211_power_arbiter_info *pm_arbiter = &pm_priv->pm_arbiter;
    u_int32_t id;
    u_int32_t cur_disable_mask;

    if ( (requestor_id & ~PM_ARBITER_REQUESTOR_ID_MASK) != PM_ARBITER_REQUESTOR_ID_MAGIC_PREFIX) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
                          "%s: invalid id 0x%x \n",__func__, requestor_id);
        return EINVAL;
    }
    id = (requestor_id & PM_ARBITER_REQUESTOR_ID_MASK);
    if (id >= PM_ARBITER_MAX_REQUESTORS) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
                          "%s: id out of range 0x%x \n",__func__, requestor_id);
        return EINVAL;
    }
    spin_lock(&vap->iv_lock);
    cur_disable_mask = pm_arbiter->disable_bitmap;
    if ( (cur_disable_mask & (1<<id)) == 0 ) {
        /* already enabled */
        spin_unlock(&vap->iv_lock);
        return EOK;
    }
    pm_arbiter->disable_bitmap &= ~(1<<id);
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s: pm_arbiter enabled by %s curmask 0x%x  \n",
                          __func__, pm_arbiter->requestor[id].id_name, pm_arbiter->disable_bitmap);
    spin_unlock(&vap->iv_lock);

    return EOK;
}

/**
 * disable arbitration for this requestor
 * @param vap           : vap handle
 * @param requestor_id  : id of requestor
 * @return EOK if success and error code if failed.
 */
int
ieee80211_power_arbiter_disable(struct ieee80211vap *vap, u_int32_t requestor_id)
{
    struct ieee80211_power      *pm_priv = vap->iv_power;
    struct ieee80211_power_arbiter_info *pm_arbiter = &pm_priv->pm_arbiter;
    u_int32_t id;
    u_int32_t cur_disable_mask;

    if ( (requestor_id & ~PM_ARBITER_REQUESTOR_ID_MASK) != PM_ARBITER_REQUESTOR_ID_MAGIC_PREFIX) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
                          "%s: invalid id 0x%x \n",__func__, requestor_id);
        return EINVAL;
    }
    id = (requestor_id & PM_ARBITER_REQUESTOR_ID_MASK);
    if (id >= PM_ARBITER_MAX_REQUESTORS) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
                          "%s: id out of range 0x%x \n",__func__, requestor_id);
        return EINVAL;
    }
    spin_lock(&vap->iv_lock);
    cur_disable_mask = pm_arbiter->disable_bitmap;
    if ( (cur_disable_mask & (1<<id))) {
        /* already disabled */
        spin_unlock(&vap->iv_lock);
        return EOK;
    }
    pm_arbiter->disable_bitmap |= (1<<id);
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s: pm_arbiter disabled by %s curmask 0x%x  \n",
                          __func__, pm_arbiter->requestor[id].id_name, pm_arbiter->disable_bitmap);
    spin_unlock(&vap->iv_lock);

    return EOK;
}

/*
* print the power arbitration status.
*/
void
ieee80211_power_arbiter_print_status(struct ieee80211vap *vap)
{
    int i;
    struct ieee80211_power      *pm_priv = vap->iv_power;
    struct ieee80211_power_arbiter_info *pm_arbiter = &pm_priv->pm_arbiter;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s: pm_arbiter disable_bitmap 0x%x  \n",
                          __func__,pm_arbiter->disable_bitmap);
    for(i=0;i<PM_ARBITER_MAX_REQUESTORS;++i) {
        if ((1<<i)& pm_arbiter->id_bitmap ) {
          IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s: pm_arbiter %s by '%s' \n",__func__,
                           (pm_arbiter->disable_bitmap) & (1 << i) ? "disabled" : "enabled",
                            pm_arbiter->requestor[i].id_name);
        }
    }
}

#else  /* UMAC_SUPPORT_STA_POWERSAVE */

/**
 * requestor ready to enter network sleep
 * @param vap           : vap handle
 * @param requestor_id  : id of requestor
 * @return EOK if success and error code if failed.
 */
int
ieee80211_power_arbiter_enter_nwsleep(struct ieee80211vap *vap, u_int32_t requestor_id)
{
    return __ieee80211_power_enter_nwsleep(vap);
}

/**
 * requestor ready to exit network sleep
 * @param vap           : vap handle
 * @param requestor_id  : id of requestor
 * @return EOK if success and error code if failed.
 */
int ieee80211_power_arbiter_exit_nwsleep(struct ieee80211vap *vap, u_int32_t requestor_id)
{
    return __ieee80211_power_exit_nwsleep(vap);
}

/**
 * allocate a requestor id to enter/exit network sleep
 * @param vap        : vap handle
 * @param name       :  friendly name of the requestor(for debugging).
 * @return unique id for the requestor if success , returns 0 if failed.
 */
u_int32_t
ieee80211_power_arbiter_alloc_id(struct ieee80211vap *vap,
        const char *requestor_name, IEEE80211_POWER_ARBITER_TYPE arbiter_type)
{
    return 0;
}

/**
 * free a requestor id to enter/exit network sleep
 * @param vap           : vap handle
 * @param requestor_id  : id of requestor
 * @return EOK if success and error code if failed.
 */
int
ieee80211_power_arbiter_free_id(struct ieee80211vap *vap, u_int32_t requestor_id)
{
    return EOK;
}

/**
 * enable arbitration for this requestor
 * @param vap           : vap handle
 * @param requestor_id  : id of requestor
 * @return EOK if success and error code if failed.
 */
int
ieee80211_power_arbiter_enable(struct ieee80211vap *vap, u_int32_t requestor_id)
{
    return EOK;
}

/**
 * disable arbitration for this requestor
 * @param vap           : vap handle
 * @param requestor_id  : id of requestor
 * @return EOK if success and error code if failed.
 */
int
ieee80211_power_arbiter_disable(struct ieee80211vap *vap, u_int32_t requestor_id)
{
    return EOK;
}

/*
* print the power arbitration status.
*/
void
ieee80211_power_arbiter_print_status(struct ieee80211vap *vap)
{
    return;
}

#endif /* ! UMAC_SUPPORT_STA_POWERSAVE */

void ieee80211_power_attach(struct ieee80211com *ic)
{
    if (ic->ic_power_attach) {
        ic->ic_power_attach(ic);
    }
}

void ieee80211_power_detach(struct ieee80211com *ic)
{
    if (ic->ic_power_detach) {
        ic->ic_power_detach(ic);
    }
}

void ieee80211_power_vattach(
        struct ieee80211vap *vap, 
        int fullsleep_enable, 
        u_int32_t sleepTimerPwrSaveMax, 
        u_int32_t sleepTimerPwrSave, 
        u_int32_t sleepTimePerf, 
        u_int32_t inactTimerPwrsaveMax, 
        u_int32_t inactTimerPwrsave, 
        u_int32_t inactTimerPerf, 
        u_int32_t smpsDynamic, 
        u_int32_t pspollEnabled)
{
    struct ieee80211com *ic = vap->iv_ic;

    if (ic->ic_power_vattach) {
        ic->ic_power_vattach(vap, fullsleep_enable,
                sleepTimerPwrSaveMax, sleepTimerPwrSave, sleepTimePerf,
                inactTimerPwrsaveMax, inactTimerPwrsave, inactTimerPerf,
                smpsDynamic, pspollEnabled);
    }
}

void ieee80211_power_latevattach(struct ieee80211vap *vap)
{
}

void ieee80211_power_vdetach(struct ieee80211vap * vap)
{
    struct ieee80211com *ic = vap->iv_ic;

    if (ic->ic_power_vdetach) {
        ic->ic_power_vdetach(vap);
    }
}
