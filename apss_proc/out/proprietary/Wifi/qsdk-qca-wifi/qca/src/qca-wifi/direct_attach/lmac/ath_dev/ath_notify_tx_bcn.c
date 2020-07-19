/*
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include "ath_internal.h"

#if ATH_SUPPORT_P2P

struct ath_notify_tx_bcn {
    spinlock_t                      notify_tx_beacon_lock;
    ath_notify_tx_beacon_function   notify_tx_bcn_callback;
    void                            *notify_tx_bcn_callback_arg;
    bool                            notify_tx_bcn_callback_inprog;
};

void
ath_notify_tx_beacon_attach(struct ath_softc *sc)
{
    ath_notify_tx_bcn_t     h_module = NULL;
    
    ASSERT(sc->sc_notify_tx_bcn == NULL);

    h_module = (struct ath_notify_tx_bcn *) OS_MALLOC(sc->sc_osdev,
                                               sizeof(struct ath_notify_tx_bcn),
                                               GFP_KERNEL);
    if (h_module == NULL) {
        printk("%s: can't alloc module object. Size=%d\n", __func__, (int)sizeof(struct ath_notify_tx_bcn));
        return;
    }
    OS_MEMZERO(h_module, sizeof(struct ath_notify_tx_bcn));

    sc->sc_notify_tx_bcn = h_module;
    spin_lock_init(&h_module->notify_tx_beacon_lock);
}

void
ath_notify_tx_beacon_detach(struct ath_softc *sc)
{
    ath_notify_tx_bcn_t     h_module = NULL;

    if (sc->sc_notify_tx_bcn == NULL) {
        return;
    }
    h_module = sc->sc_notify_tx_bcn;

    ASSERT(h_module->notify_tx_bcn_callback_inprog == false);

    spin_lock_destroy(&h_module->notify_tx_beacon_lock);

    sc->sc_notify_tx_bcn = NULL;
    OS_FREE(h_module);
}

int
ath_reg_notify_tx_bcn(ath_dev_t dev, 
                      ath_notify_tx_beacon_function callback,
                      void *arg)
{
    struct ath_softc        *sc = ATH_DEV_TO_SC(dev);
    ath_notify_tx_bcn_t     h_module = NULL;

    if (sc->sc_notify_tx_bcn == NULL) {
        printk("%s: Error: module not init.\n", __func__);
        return -1;
    }
    h_module = sc->sc_notify_tx_bcn;

    spin_lock(&h_module->notify_tx_beacon_lock);

    ASSERT(atomic_read(&sc->sc_has_tx_bcn_notify) == 0);
    ASSERT(h_module->notify_tx_bcn_callback == NULL);

    h_module->notify_tx_bcn_callback = callback;
    h_module->notify_tx_bcn_callback_arg = arg;
    atomic_set(&sc->sc_has_tx_bcn_notify, 1);

    spin_unlock(&h_module->notify_tx_beacon_lock);

    return 0;
}

/*
 * Called to De-registered the "Notify Tx Beacon" callback.
 * Return 0 if successful. -1 if unsuccessful. 1 if success but callback in progress.
 * Note: This routine may be called multiple times to check if the callback is
 *       still "in progress".
 */
int
ath_dereg_notify_tx_bcn(ath_dev_t dev)
{
    struct ath_softc        *sc = ATH_DEV_TO_SC(dev);
    bool                    in_progress = 0;
    ath_notify_tx_bcn_t     h_module = NULL;

    if (sc->sc_notify_tx_bcn == NULL) {
        printk("%s: Error: module not init.\n", __func__);
        return -1;
    }
    h_module = sc->sc_notify_tx_bcn;

    spin_lock(&h_module->notify_tx_beacon_lock);

    atomic_set(&sc->sc_has_tx_bcn_notify, 0);
    h_module->notify_tx_bcn_callback = NULL;

    in_progress = h_module->notify_tx_bcn_callback_inprog;

    spin_unlock(&h_module->notify_tx_beacon_lock);

    return in_progress;
}

/* 
 * Notify that a beacon has completed
 * Note: This is called from DPC.
 */
void
ath_tx_bcn_notify(struct ath_softc *sc)
{
    int                             beacon_id = 0;  /* TODO, we need to figure which beacon has just transmitted. */
    u_int32_t                       tx_status = 0;  /* TODO, we need to figure tx completion status */
    ath_notify_tx_beacon_function   callback_func;
    void                            *callback_arg;
    ath_notify_tx_bcn_t             h_module = NULL;

    ASSERT(sc->sc_notify_tx_bcn);
    h_module = sc->sc_notify_tx_bcn;

    spin_lock(&h_module->notify_tx_beacon_lock);
    ASSERT(beacon_id < ATH_BCBUF);

    callback_func = h_module->notify_tx_bcn_callback;
    callback_arg = h_module->notify_tx_bcn_callback_arg;
    if ((atomic_read(&sc->sc_has_tx_bcn_notify)) && (callback_func != NULL)) {
        h_module->notify_tx_bcn_callback_inprog = true;
        spin_unlock(&h_module->notify_tx_beacon_lock);

        /* Do the callback without the lock */
        (*callback_func)(callback_arg, beacon_id, tx_status);

        h_module->notify_tx_bcn_callback_inprog = false;
    }
    else {
        spin_unlock(&h_module->notify_tx_beacon_lock);
    }
}

#endif  //ATH_SUPPORT_P2P

