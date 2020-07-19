/*
 * Copyright (c) 2011,2017 Qualcomm Innovation Center, Inc.
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

/*
 * This module provide certain run-time information about the ATH-VAP
 * including callbacks when the particular data changes.
 */
#include "ath_internal.h"
#ifdef UNINET
#include <uninet_lmac_common.h>
#else
#include <umac_lmac_common.h>
#endif

#if ATH_SUPPORT_ATHVAP_INFO

struct ath_vap_info {
    spinlock_t                      lock;
    struct ath_softc                *sc;
    u_int32_t                       info_req_mask;
    ath_vap_info_notify_func        change_callback;
    void                            *change_callback_arg;
    atomic_t                        callback_inprog;

    /* Used for the ATH_VAP_INFOTYPE_SLOT_OFFSET information */
    atomic_t                        bslot_info_notify;

};

void
ath_vap_info_attach(struct ath_softc *sc, struct ath_vap *avp)
{
    ath_vap_info_t  h_module;

    ASSERT(avp->av_vap_info == NULL);

    h_module = (struct ath_vap_info *) OS_MALLOC(sc->sc_osdev,
                                               sizeof(struct ath_vap_info),
                                               GFP_KERNEL);
    if (h_module == NULL) {
        printk("%s: can't alloc module object. Size=%d\n", __func__, (int)sizeof(struct ath_vap_info));
        return;
    }
    OS_MEMZERO(h_module, sizeof(struct ath_vap_info));

    spin_lock_init(&h_module->lock);

    avp->av_vap_info = h_module;
    h_module->sc = sc;
}

void
ath_vap_info_detach(struct ath_softc *sc, struct ath_vap *avp)
{
    ath_vap_info_t    h_module = NULL;

    if (avp->av_vap_info == NULL) {
        printk("%s: sc->sc_vap_info is already NULL\n", __func__);
        return;
    }
    h_module = avp->av_vap_info;

    ASSERT(atomic_read(&h_module->callback_inprog) == 0);
    spin_lock_destroy(&h_module->lock);

    avp->av_vap_info = NULL;
    OS_FREE(h_module);
}

/* Get the current information about the VAP */
int
ath_vap_info_get(
    ath_dev_t                   dev,
    int                         vap_if_id,
    ath_vap_infotype            vap_info_type,
    u_int32_t                   *ret_param1,
    u_int32_t                   *ret_param2)
{
    struct ath_vap      *avp;
    struct ath_softc    *sc = ATH_DEV_TO_SC(dev);

    if ((vap_if_id == ATH_IF_ID_ANY) || (sc->sc_vaps[vap_if_id] == NULL)) {
        printk("%s: invalid if_id=%d or vap is NULL.\n", __func__,
               vap_if_id);
        return -EINVAL;     /* nothing to do since current interface is not valid */
    }
    ASSERT(vap_if_id < ATH_VAPSIZE);

    avp = sc->sc_vaps[vap_if_id];
    if (avp->av_vap_info == NULL) {
        printk("%s: avp->av_vap_info is NULL. vap_if_id=%d\n", __func__, vap_if_id);
        return -EINVAL;      /* VAP is not valid */
    }

    switch(vap_info_type) {
    case ATH_VAP_INFOTYPE_SLOT_OFFSET:
        ASSERT(ret_param1);
        *ret_param1 = le64_to_cpu(avp->av_tsfadjust);
        break;
    default:
        printk("%s: unknown VAP info type=%d\n", __func__, vap_info_type);
        return -EINVAL;
    }

    return 0;
}

/* Inform the VAP that its information has changed or assigned */
static void
ath_vap_info_notify_change(
    ath_vap_info_t      h_module,
    ath_vap_infotype    infotype,
    u_int32_t           param1,
    u_int32_t           param2)
{
    ath_vap_info_notify_func    callback_func;
    void                        *callback_arg;

    spin_lock(&h_module->lock);

    if ((infotype & h_module->info_req_mask) == 0) {
        /* Not interested in this info. */
        spin_unlock(&h_module->lock);
        return;
    }

    ASSERT(h_module->change_callback);
    callback_func = h_module->change_callback;
    callback_arg = h_module->change_callback_arg;
    atomic_inc(&h_module->callback_inprog);
    spin_unlock(&h_module->lock);

    /* Do the callback without the lock */
    (*callback_func)(callback_arg, infotype, param1, param2);

    atomic_dec(&h_module->callback_inprog);
}

/* Inform the VAP that its beacon slot has changed or assigned */
void
ath_bslot_info_notify_change(struct ath_vap *avp, u_int64_t tsf_offset)
{
    ath_vap_info_t      h_module;

    h_module = avp->av_vap_info;

    if (h_module == NULL) {
        return;
    }

    if (!atomic_read(&h_module->bslot_info_notify)) {
        /* 
         * Due to overhead of spinlock, I do the first check without locking since most vaps
         * will not use this feature.
         */
        return;     /* No notify required */
    }

    ASSERT((tsf_offset >> 32) == 0); /* assumed that TSF adjust is only 32 bits */
    ath_vap_info_notify_change(h_module, ATH_VAP_INFOTYPE_SLOT_OFFSET, (u_int32_t)tsf_offset, 0);
}

/* Assumed that the lock is held */
static void
update_infotype_mask(
    ath_vap_info_t      h_module,
    ath_vap_infotype    infotype_mask)
{
    h_module->info_req_mask = infotype_mask;

    if (infotype_mask & ATH_VAP_INFOTYPE_SLOT_OFFSET) {
        atomic_set(&h_module->bslot_info_notify, 1);
    }
    else {
        atomic_set(&h_module->bslot_info_notify, 0);
    }
}

/*
 * Register a notification when the specified ATH information changes.
*/
int
ath_reg_vap_info_notify(
    ath_dev_t                   dev,
    int                         vap_if_id,
    ath_vap_infotype            infotype_mask,
    ath_vap_info_notify_func    callback,
    void                        *arg)
{
    struct ath_softc    *sc = ATH_DEV_TO_SC(dev);
    ath_vap_info_t      h_module = NULL;
    struct ath_vap      *avp;

    ASSERT(vap_if_id < ATH_VAPSIZE);
    avp = sc->sc_vaps[vap_if_id];
    if (avp == NULL) {
        printk("%s: invalid VAP. if_id=%d\n", __func__, vap_if_id);
        return -EINVAL;      /* VAP is not valid */
    }

    if (avp->av_vap_info == NULL) {
        printk("%s: avp->av_vap_info is NULL\n", __func__);
        return -EINVAL;
    }
    h_module = avp->av_vap_info;

    /* Make sure that we have not register before */
    if ((h_module->info_req_mask != 0) || (h_module->change_callback)) {
        printk("%s: Error: Already registered.\n", __func__);
        return -EINVAL;
    }

    h_module->change_callback = callback;
    h_module->change_callback_arg = arg;
    update_infotype_mask(h_module, infotype_mask);

    return 0;
}

/*
 * Update the list of interested ATH information for callback.
*/
int
ath_vap_info_update_notify(
    ath_dev_t                   dev,
    int                         vap_if_id,
    ath_vap_infotype            infotype_mask)
{
    struct ath_softc    *sc = ATH_DEV_TO_SC(dev);
    ath_vap_info_t      h_module = NULL;
    struct ath_vap      *avp;

    ASSERT(vap_if_id < ATH_VAPSIZE);
    avp = sc->sc_vaps[vap_if_id];
    if (avp == NULL) {
        printk("%s: invalid VAP. if_id=%d\n", __func__, vap_if_id);
        return -EINVAL;      /* VAP is not valid */
    }

    if (avp->av_vap_info == NULL) {
        printk("%s: avp->av_vap_info is NULL\n", __func__);
        return -EINVAL;
    }
    h_module = avp->av_vap_info;

    /* Make sure that we have register before */
    ASSERT(h_module->change_callback != NULL);

    update_infotype_mask(h_module, infotype_mask);

    return 0;
}

/*
 * Called to De-registered the VAP information changed callback.
 * Return 0 if successful. -1 if unsuccessful. 1 if success but callback in progress.
 * Note: This routine may be called multiple times to check if the callback is
 *       still "in progress".
 */
int
ath_dereg_vap_info_notify(
    ath_dev_t   dev,
    int         vap_if_id)
{
    struct ath_softc    *sc = ATH_DEV_TO_SC(dev);
    ath_vap_info_t      h_module = NULL;
    struct ath_vap      *avp;
    int                 in_progress = 0;

    ASSERT(vap_if_id < ATH_VAPSIZE);
    avp = sc->sc_vaps[vap_if_id];
    if (avp == NULL) {
        printk("%s: invalid VAP. if_id=%d\n", __func__, vap_if_id);
        return -EINVAL;      /* VAP is not valid */
    }

    if (avp->av_vap_info == NULL) {
        printk("%s: avp->av_vap_info is NULL\n", __func__);
        return -EINVAL;
    }
    h_module = avp->av_vap_info;

    update_infotype_mask(h_module, 0);  /* stop further callbacks */

    spin_lock(&h_module->lock);

    if (atomic_read(&h_module->callback_inprog) != 0) {
        /* A callback is still in progress */
        printk("%s: Note: callback still in progress (%d)\n", 
               __func__, atomic_read(&h_module->callback_inprog));
        in_progress = 1;
    }
    h_module->change_callback = NULL;

    spin_unlock(&h_module->lock);

    return in_progress;
}

#endif //ATH_SUPPORT_ATHVAP_INFO

