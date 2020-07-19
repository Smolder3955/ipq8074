/*
* Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 *  Copyright (c) 2010 Atheros Communications Inc.  All rights reserved.
 */
/*
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include <ieee80211_var.h>
#include <ieee80211_vap_ath_info.h>

#if ATH_SUPPORT_ATHVAP_INFO

/* We ran out of bits for the iv_debug field. For now, use the STATE module. */
#define IEEE80211_MSG_VAP_ATH_INFO      IEEE80211_MSG_STATE

#define MAX_VAP_ATH_INFO_NOTIFY     4

struct ieee80211_vap_ath_info_notify {
    ath_vap_infotype                    interested_types;
    bool                                used;
    ieee80211_vap_ath_info_t            h_mgr;
    ieee80211_vap_ath_info_notify_func  callback;
    void                                *callback_arg;
    bool                                callback_inprog;
};

/* 
 * Private structure for this module's info.
 * Pointer resides in VAP
 */
struct ieee80211_vap_ath_info {
    wlan_if_t                               vap;
    ath_vap_infotype                        all_interested_types;
    int                                     num_registered;
    int                                     active_callbks;
    spinlock_t                              lock;

    struct ieee80211_vap_ath_info_notify    notify[MAX_VAP_ATH_INFO_NOTIFY];
};

/*
 * Get the current value for this VAP_ATH information.
 */
int
ieee80211_vap_ath_info_get(
    wlan_if_t                   vap,
    ath_vap_infotype            infotype,
    u_int32_t                   *param1,
    u_int32_t                   *param2)
{
    int                         ret_val = 0;
    ieee80211_vap_ath_info_t    handle;

    if (vap->iv_vap_ath_info_handle == NULL) {
        printk("%s: Error: not attached.\n", __func__);
        return EINVAL;
    }
    handle = vap->iv_vap_ath_info_handle;

    ret_val = vap->iv_vap_ath_info_get(vap, infotype, param1, param2);
    if (ret_val != 0) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_VAP_ATH_INFO, 
                "%s: Error calling iv_vap_info_get. ret=%d\n", __func__, ret_val);
        return EINVAL;
    }
    return EOK;
}

static void
ieee80211_vap_ath_info_callback(
    void *arg, 
    ath_vap_infotype type,
    u_int32_t param1,
    u_int32_t param2)
{
    ieee80211_vap_ath_info_t    h_mgr;
    int                         i;

    ASSERT(type != 0);
    ASSERT(arg);
    h_mgr = (ieee80211_vap_ath_info_t)arg;

    /* 
     * This callback may be overlapped by different types.
     * i.e. the ATH layer may call this function in different threads.
     * This spinlock will serialized all these calls.
     */
    spin_lock(&(h_mgr->lock));

    /* Multiple parties could have registered. Check who is interested in this info. */
    for (i = 0; i < MAX_VAP_ATH_INFO_NOTIFY; i++) {
        if (!h_mgr->notify[i].used) {
            break;
        }
        if ((h_mgr->notify[i].interested_types & type) != 0) {
            /* This one is interested. Do the notify callback */
            ieee80211_vap_ath_info_notify_func  cb_func;
            void                                *cb_arg;
            ieee80211_vap_ath_info_notify_t     h_notify = &h_mgr->notify[i];

            ASSERT(h_notify->callback != NULL);
            ASSERT(!h_notify->callback_inprog);
            h_notify->callback_inprog = true;
            cb_func = h_notify->callback;
            cb_arg = h_notify->callback_arg;

            /* Do the callback with the lock */
            (*cb_func)(cb_arg, type, param1, param2);

            h_notify->callback_inprog = false;
        }
    }
    spin_unlock(&(h_mgr->lock));
}

/*
 * Register a callback whenever the interested ATH information changes.
 */
ieee80211_vap_ath_info_notify_t
ieee80211_vap_ath_info_reg_notify(
    wlan_if_t                           vap,
    ath_vap_infotype                    infotype_mask,
    ieee80211_vap_ath_info_notify_func  vap_ath_info_callback,
    void                                *arg)
{
    int                         ret_val = EOK;
    int                         i;
    ieee80211_vap_ath_info_t    h_mgr;

    if (vap->iv_vap_ath_info_handle == NULL) {
        printk("%s: Error: not attached.\n", __func__);
        return NULL;
    }

    h_mgr = vap->iv_vap_ath_info_handle;

    spin_lock(&(h_mgr->lock));
    ASSERT(infotype_mask != 0);             /* Must have some interested bits */

    if (h_mgr->num_registered >= MAX_VAP_ATH_INFO_NOTIFY) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_VAP_ATH_INFO, 
            "%s: ERROR: max. number of notifies registered.\n", __func__);
        spin_unlock(&(h_mgr->lock));
        return NULL;
    }

    /* Find an unused one */
    for (i = 0; i < MAX_VAP_ATH_INFO_NOTIFY; i++) {
        if (!h_mgr->notify[i].used) {
            break;
        }
    }
    ASSERT(i < MAX_VAP_ATH_INFO_NOTIFY);

    h_mgr->notify[i].used = true;
    h_mgr->notify[i].callback = vap_ath_info_callback;
    h_mgr->notify[i].callback_arg = arg;
    h_mgr->notify[i].interested_types = infotype_mask;
    h_mgr->notify[i].h_mgr = h_mgr;

    h_mgr->num_registered++;

    if (h_mgr->num_registered == 1) {
        /* This is the first one. Register with the ATH layer. */
        ret_val = vap->iv_reg_vap_ath_info_notify(vap, infotype_mask, ieee80211_vap_ath_info_callback, h_mgr);
        if (ret_val != 0) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_VAP_ATH_INFO, 
                "%s: ERROR: Unabled to register callback from ATH. ret=%d\n", __func__, ret_val);

            /* Release this notify */
            h_mgr->num_registered--;
            OS_MEMZERO(&(h_mgr->notify[i]), sizeof(struct ieee80211_vap_ath_info_notify));

            spin_unlock(&(h_mgr->lock));
            return NULL;
        }
    }
    else {
        if ((h_mgr->all_interested_types | infotype_mask) != h_mgr->all_interested_types) {
            /* There are some new bits */
            ret_val = vap->iv_vap_ath_info_update_notify(vap, (h_mgr->all_interested_types | infotype_mask));
            if (ret_val != 0) {
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_VAP_ATH_INFO, 
                    "%s: ERROR: Unabled to register callback from ATH. ret=%d\n", __func__, ret_val);

                /* Release this notify */
                h_mgr->num_registered--;
                OS_MEMZERO(&(h_mgr->notify[i]), sizeof(struct ieee80211_vap_ath_info_notify));

                spin_unlock(&(h_mgr->lock));
                return NULL;
            }
        }
    }
    h_mgr->all_interested_types |= infotype_mask;  /* Gather all the interested items */

    spin_unlock(&(h_mgr->lock));

    return &(h_mgr->notify[i]);
}

/*
 * Deregister a callback.
 */
int
ieee80211_vap_ath_info_dereg_notify(
    ieee80211_vap_ath_info_notify_t h_notify)
{
    int                         ret_val = EOK;
    int                         i;
    wlan_if_t                   vap;
    ieee80211_vap_ath_info_t    h_mgr;

    if (h_notify == NULL) {
        printk("%s: Error: not attached.\n", __func__);
        return EINVAL;
    }

    h_mgr = h_notify->h_mgr;
    ASSERT(h_mgr);
    vap = h_mgr->vap;

    spin_lock(&(h_mgr->lock));

    ASSERT(h_notify->used);
    ASSERT(h_mgr->num_registered > 0);

    ASSERT(!h_notify->callback_inprog);

    h_mgr->num_registered--;

    OS_MEMZERO(h_notify, sizeof(struct ieee80211_vap_ath_info_notify));

    /* Re-calculate the interested information */
    h_mgr->all_interested_types = 0;
    for (i = 0; i < MAX_VAP_ATH_INFO_NOTIFY; i++) {
        if (!h_mgr->notify[i].used) {
            break;
        }
        h_mgr->all_interested_types |= h_mgr->notify[i].interested_types;
    }

    if (h_mgr->num_registered == 0) {
        /* Last one, deregistered with the ATH callbacks. */
        #define MAX_NUM_TRIES       20
        int     sanity_count = 0;

        ASSERT(h_mgr->all_interested_types == 0);
        while (true) {
            ret_val = vap->iv_dereg_vap_ath_info_notify(vap);
            if (ret_val == 1) {
                /* Interface is busy. Try again. */
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_VAP_ATH_INFO, 
                        "%s: Interface Busy. Try %d of %d\n", __func__, sanity_count, MAX_NUM_TRIES);
                OS_DELAY(10);   // wait for 10 microseconds
                sanity_count++;
                if (sanity_count >= MAX_NUM_TRIES) {
                    IEEE80211_DPRINTF(vap, IEEE80211_MSG_VAP_ATH_INFO, 
                                      "%s: Error: waiting too long for "
                                      "iv_dereg_vap_ath_info_notify to complete.\n", __func__);
                    break;
                }
                continue;
            }
            else if (ret_val != 0) {
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_VAP_ATH_INFO, 
                        "%s: Error calling iv_vap_info_get. ret=%d\n", __func__, ret_val);
                ret_val = EINVAL;
            }
            break;
        }
        #undef  MAX_NUM_TRIES
    }
    else {
        ret_val = vap->iv_vap_ath_info_update_notify(vap, h_mgr->all_interested_types);
        if (ret_val != 0) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_VAP_ATH_INFO, 
                "%s: ERROR: Unabled to update registration callback from ATH. ret=%d\n", __func__, ret_val);
            ret_val = EINVAL;
        }
    }
    spin_unlock(&(h_mgr->lock));

    return ret_val;
}

/*
 * Attach the "VAP_ATH_INFO" manager.
 */
ieee80211_vap_ath_info_t
ieee80211_vap_ath_info_attach(
    wlan_if_t           vap)
{
    ieee80211_vap_ath_info_t    h_mgr;

    if (vap->iv_vap_ath_info_handle) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_VAP_ATH_INFO, 
                "%s: Error: already attached.\n", __func__);
        return NULL;
    }

    /* Allocate ResMgr data structures */
    h_mgr = (ieee80211_vap_ath_info_t) OS_MALLOC(vap->iv_ic->ic_osdev, 
                                            sizeof(struct ieee80211_vap_ath_info),
                                            0);
    if (h_mgr == NULL) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_VAP_ATH_INFO, 
             "%s : memory alloction failed. size=%d\n", __func__, 
              sizeof(struct ieee80211_vap_ath_info));
        return NULL;
    }

    OS_MEMZERO(h_mgr, sizeof(struct ieee80211_vap_ath_info));
    h_mgr->vap = vap;

    spin_lock_init(&(h_mgr->lock));

    return h_mgr;
}

/*
 * Delete "VAP_ATH_INFO" manager.
 */
void
ieee80211_vap_ath_info_detach(ieee80211_vap_ath_info_t h_mgr)
{
    wlan_if_t           vap;

    if (!h_mgr) {
        /* Not allocated. Return */
        printk("%s : Not even attached.\n", __func__);
        return;
    }
    vap = h_mgr->vap;

    if (h_mgr->all_interested_types != 0) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_VAP_ATH_INFO, 
             "%s : Warning: did not de-registered callbacks=0x%x.\n",
                          __func__, h_mgr->all_interested_types);
    }

    ASSERT(vap->iv_vap_ath_info_handle != NULL);

    spin_lock_destroy(&(h_mgr->lock));

    /* Free data structures */
    OS_FREE(h_mgr);
    vap->iv_vap_ath_info_handle = NULL;
}

#endif  //ATH_SUPPORT_ATHVAP_INFO

