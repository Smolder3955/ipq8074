/*
 * Copyright (c) 2011, 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */
/*
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
#include <ieee80211_var.h>
#include <ieee80211_notify_tx_bcn.h>

#if UMAC_SUPPORT_P2P

/* We ran out of bits for the ic->ic_debug field. For now, use the STATE module. */
#define IEEE80211_MSG_NOTIFY_TX_BCN IEEE80211_MSG_STATE

#define MAX_TX_BCN_NOTIFY   4

struct ieee80211_tx_bcn_notify {
    bool                            used;
    int                             beacon_id;
    ieee80211_notify_tx_bcn_mgr_t   h_mgr;
    tx_bcn_notify_func              callback;
    void                            *callback_arg;
    bool                            callback_inprog;

    /* 
       For the split driver, beacon done is delivered by work queue and it may cause race oncdition with htc_thread
       which is trying to dereg notify becaon done callback but callback is in process. Add a flag to defer it.
    */
    bool                            delete_req;
};

/* 
 * Private structure to store the Tx Beacon Notify Manager info.
 * Pointer resides in struct ieee80211com
 */
struct ieee80211_notify_tx_bcn_mgr {
    struct ieee80211com             *ic;
    int                             num_registered;
    int                             active_callbks;
    spinlock_t                      lock;

    struct ieee80211_tx_bcn_notify  notify[MAX_TX_BCN_NOTIFY];
};

/*
 * Callback function from ATH layer when a beacon is transmitted.
 */
static void
ieee80211_notify_tx_bcn_callback(
    void                    *arg,
    int                     beacon_id,
    u_int32_t               tx_status)
{
    ieee80211_notify_tx_bcn_mgr_t   h_mgr = (ieee80211_notify_tx_bcn_mgr_t)arg;
    int                             i, ret_val = 0;

    ASSERT(h_mgr->ic->ic_notify_tx_bcn_mgr == h_mgr);

    spin_lock(&(h_mgr->lock));

    for (i = 0; i < MAX_TX_BCN_NOTIFY; i++) {
        ieee80211_tx_bcn_notify_t   h_notify = &h_mgr->notify[i];
        
        if (h_notify->used && (h_notify->beacon_id == beacon_id)) {
            /* Found a match. Do the callback. */
            tx_bcn_notify_func  cb_func;
            void                *cb_arg;

            ASSERT(h_notify->callback != NULL);
            ASSERT(!h_notify->callback_inprog);
            h_notify->callback_inprog = true;
            cb_func = h_notify->callback;
            cb_arg = h_notify->callback_arg;

            spin_unlock(&(h_mgr->lock));
            /* Callback without the lock */
            (*cb_func)(h_notify, beacon_id, tx_status, cb_arg);
            spin_lock(&(h_mgr->lock));
            h_notify->callback_inprog = false;

            if(h_notify->delete_req) {
                IEEE80211_DPRINTF_IC(h_mgr->ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_NOTIFY_TX_BCN,
						"%s: Warning: Delete request while callback in process\n",__func__);
                h_mgr->num_registered--;

                OS_MEMZERO(h_notify, sizeof(struct ieee80211_tx_bcn_notify));

                if (h_mgr->num_registered == 0) {
                    /* Last one, deregistered with the ATH callbacks. */
                    ret_val = h_mgr->ic->ic_dereg_notify_tx_bcn(h_mgr->ic);
                    if (ret_val < 0) {
                    IEEE80211_DPRINTF_IC(h_mgr->ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_NOTIFY_TX_BCN,
                            "%s: ERROR: Unabled to deregister callback from ATH. ret=%d\n", __func__, ret_val);
                    }
                    else if (ret_val == 1) {
                        IEEE80211_DPRINTF_IC(h_mgr->ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_NOTIFY_TX_BCN,
                            "%s: Warning: Callback in progress. ret=%d\n", __func__, ret_val);
                    }
                }   
            }
        }
    }

    spin_unlock(&(h_mgr->lock));
}

ieee80211_tx_bcn_notify_t
ieee80211_reg_notify_tx_bcn(
    ieee80211_notify_tx_bcn_mgr_t   h_mgr,
    tx_bcn_notify_func              callback,
    int                             beacon_id,
    void                            *arg)
{
    struct ieee80211com     *ic;
    int                     i, ret_val = 0;

    if (h_mgr == NULL) {
        printk("%s: ERROR: ieee80211_notify_tx_bcn_mgr is NULL.\n", __func__);
        return NULL;
    }

    ic = h_mgr->ic;
    spin_lock(&(h_mgr->lock));

    if (h_mgr->num_registered >= MAX_TX_BCN_NOTIFY) {
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_NOTIFY_TX_BCN,
            "%s: ERROR: max. number of notifies registered.\n", __func__);
        spin_unlock(&(h_mgr->lock));
        return NULL;
    }

    /* Check notify_tx_bcn existed before register it. */
    for (i = 0; i < MAX_TX_BCN_NOTIFY; i++) {
        if (h_mgr->notify[i].used &&
            h_mgr->notify[i].callback == callback &&
            h_mgr->notify[i].beacon_id == beacon_id &&
            h_mgr->notify[i].callback_arg == arg &&
            h_mgr->notify[i].h_mgr == h_mgr &&
            h_mgr->notify[i].delete_req == false) {
            IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_NOTIFY_TX_BCN, 
                                  "%s: already registered.\n", __func__);
	    spin_unlock(&(h_mgr->lock));
	    return &(h_mgr->notify[i]);
        }
    }
    
    /* Find an unused one */
    for (i = 0; i < MAX_TX_BCN_NOTIFY; i++) {
        if (!h_mgr->notify[i].used) {
            break;
        }
    }
    ASSERT(i < MAX_TX_BCN_NOTIFY);

    h_mgr->notify[i].used = true;
    h_mgr->notify[i].callback = callback;
    h_mgr->notify[i].beacon_id = beacon_id;
    h_mgr->notify[i].callback_arg = arg;
    h_mgr->notify[i].h_mgr = h_mgr;

    h_mgr->num_registered++;

    if (h_mgr->num_registered == 1) {
        /* First one, registered with the ATH callbacks. */
        ret_val = ic->ic_reg_notify_tx_bcn(ic, ieee80211_notify_tx_bcn_callback, h_mgr);
        if (ret_val != 0) {
            IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_NOTIFY_TX_BCN,
                "%s: ERROR: Unabled to register callback from ATH. ret=%d\n", __func__, ret_val);

            /* Release this notify */
            h_mgr->num_registered--;
            OS_MEMZERO(&(h_mgr->notify[i]), sizeof(struct ieee80211_tx_bcn_notify));

            spin_unlock(&(h_mgr->lock));
            return NULL;
        }
    }

    spin_unlock(&(h_mgr->lock));

    return &(h_mgr->notify[i]);
}

void
ieee80211_dereg_notify_tx_bcn(
    ieee80211_tx_bcn_notify_t   h_notify)
{
    ieee80211_notify_tx_bcn_mgr_t   h_mgr;
    int                             ret_val = 0;
    
    ASSERT(h_notify);
    h_mgr = h_notify->h_mgr;
    ASSERT(h_mgr);

    spin_lock(&(h_mgr->lock));

    ASSERT(h_notify->used);
    ASSERT(h_mgr->num_registered > 0);

    ASSERT(!h_notify->callback_inprog);

    h_mgr->num_registered--;

    OS_MEMZERO(h_notify, sizeof(struct ieee80211_tx_bcn_notify));

    if (h_mgr->num_registered == 0) {
        /* Last one, deregistered with the ATH callbacks. */
        ret_val = h_mgr->ic->ic_dereg_notify_tx_bcn(h_mgr->ic);
        if (ret_val < 0) {
            IEEE80211_DPRINTF_IC(h_mgr->ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_NOTIFY_TX_BCN,
                "%s: ERROR: Unabled to deregister callback from ATH. ret=%d\n", __func__, ret_val);
        }
        else if (ret_val == 1) {
            IEEE80211_DPRINTF_IC(h_mgr->ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_NOTIFY_TX_BCN,
                "%s: Warning: Callback in progress. ret=%d\n", __func__, ret_val);
        }
    }

    spin_unlock(&(h_mgr->lock));
}

/*
 * Attach the "Notify Tx Beacon" manager.
 */
ieee80211_notify_tx_bcn_mgr_t ieee80211_notify_tx_bcn_attach(struct ieee80211com *ic)
{
    ieee80211_notify_tx_bcn_mgr_t   h_mgr;

    if (ic->ic_notify_tx_bcn_mgr) {
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_NOTIFY_TX_BCN,
                             "%s : notify_tx_bcn manager already exists \n", __func__); 
        return NULL;
    }

    /* Allocate ResMgr data structures */
    h_mgr = (ieee80211_notify_tx_bcn_mgr_t) OS_MALLOC(ic->ic_osdev, 
                                            sizeof(struct ieee80211_notify_tx_bcn_mgr),
                                            0);
    if (h_mgr == NULL) {
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_NOTIFY_TX_BCN,
                             "%s : notify_tx_bcn manager memory alloction failed\n", __func__); 
        return NULL;
    }

    OS_MEMZERO(h_mgr, sizeof(struct ieee80211_notify_tx_bcn_mgr));
    h_mgr->ic = ic;

    spin_lock_init(&(h_mgr->lock));

    h_mgr->active_callbks = 0;
    h_mgr->num_registered = 0;

    return h_mgr;
}

/*
 * Delete Tsf Timer manager.
 */
void ieee80211_notify_tx_bcn_detach(ieee80211_notify_tx_bcn_mgr_t h_mgr)
{
    struct ieee80211com *ic;

    if (!h_mgr) {
        /* Not allocated. Return */
        return;
    }

    ic = h_mgr->ic;
    ASSERT(ic->ic_notify_tx_bcn_mgr != NULL);

    ASSERT(h_mgr->num_registered == 0);

    spin_lock_destroy(&(h_mgr->lock));

    /* Free data structures */
    OS_FREE(h_mgr);
    ic->ic_notify_tx_bcn_mgr = NULL;
}

#endif  //UMAC_SUPPORT_P2P

