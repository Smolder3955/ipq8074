/*
 * Copyright (c) 2010, Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */

#ifdef ATH_USB

#include "ath_internal.h"

extern void ath_wmi_beacon_stuck(struct ath_softc *sc);
extern void ath_wmi_beacon_immunity(struct ath_softc *sc);
extern void ath_wmi_beacon_setslottime(struct ath_softc *sc);

void
ath_usb_suspend(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    sc->sc_usb_invalid = 1;
}

int 
ath_usb_set_tx(struct ath_softc *sc)
{
    /* Workaround : use TxQ-0 for BE for K2 since Q0 is transmistting faster than Q1 under same backoff parameters */

    if (!ath_tx_setup(sc, HAL_WME_AC_BE)) {
        printk("unable to setup xmit queue for BE traffic!\n");

        return 0;
    }

    if (!ath_tx_setup(sc, HAL_WME_AC_BK) ||
        !ath_tx_setup(sc, HAL_WME_AC_VI) ||
        !ath_tx_setup(sc, HAL_WME_AC_VO)) {
        /*
         * Not enough hardware tx queues to properly do WME;
         * just punt and assign them all to the same h/w queue.
         * We could do a better job of this if, for example,ath_hal_getmcastkeysearch(ah)
         * we allocate queues when we switch from station to
         * AP mode.
         */
        if (sc->sc_haltype2q[HAL_WME_AC_VI] != -1)
            ath_tx_cleanupq(sc, &sc->sc_txq[sc->sc_haltype2q[HAL_WME_AC_VI]]);
        if (sc->sc_haltype2q[HAL_WME_AC_BK] != -1)
            ath_tx_cleanupq(sc, &sc->sc_txq[sc->sc_haltype2q[HAL_WME_AC_BK]]);
        sc->sc_haltype2q[HAL_WME_AC_BK] = sc->sc_haltype2q[HAL_WME_AC_BE];
        sc->sc_haltype2q[HAL_WME_AC_VI] = sc->sc_haltype2q[HAL_WME_AC_BE];
        sc->sc_haltype2q[HAL_WME_AC_VO] = sc->sc_haltype2q[HAL_WME_AC_BE];
    } else {
        /*
         * Mark WME capability since we have sufficient
         * hardware queues to do proper priority scheduling.
         */
        sc->sc_haswme = 1;
#ifdef ATH_SUPPORT_UAPSD
        sc->sc_uapsdq = ath_txq_setup(sc, HAL_TX_QUEUE_UAPSD, 0);
        if (sc->sc_uapsdq == NULL) {
            DPRINTF(sc, ATH_DEBUG_UAPSD, "%s: unable to setup UAPSD xmit queue!\n",
                    __func__);
        }
        else {
            /*
             * default UAPSD on if HW capable
             */
            sc->sc_uapsdsupported = 1;
        }
#endif
    }

    return 1;
}

#ifdef __linux__
/* Thread Related functions */
u_int32_t smp_kevent_Lock = 0;

#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,20))
void kevent(struct work_struct *work)
#else
void kevent(void *data)
#endif
{
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,20))
    struct ath_softc *sc =
               container_of(work, struct ath_softc, kevent);
#else
    struct ath_softc *sc = (struct ath_softc *) data;
#endif

    if (sc == NULL)
    {
        return;
    }

    if (test_and_set_bit(0, (void *)&smp_kevent_Lock))
    {
        return;
    }

    down(&sc->ioctl_sem);

    if (test_and_clear_bit(KEVENT_BEACON_STUCK, &sc->kevent_flags))
    {
//        struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(sc->sc_ieee);
//        scn->sc_ops->ath_wmi_beacon_stuck(scn->sc_dev);
        ath_wmi_beacon_stuck(sc);
    }

    if (test_and_clear_bit(KEVENT_BEACON_IMMUNITY, &sc->kevent_flags))
    {
        ath_wmi_beacon_immunity(sc);
    }

    if (test_and_clear_bit(KEVENT_BEACON_SETSLOTTIME, &sc->kevent_flags))
    {
        ath_wmi_beacon_setslottime(sc);
    }

    if (test_and_clear_bit(KEVENT_WMM_UPDATE, &sc->kevent_flags))
    {
        /*
         * To update wmm atomic, avoid HW and SW state reset.
         */
        ATH_PS_INIT_LOCK(sc);
        sc->sc_ieee_ops->ath_htc_wmm_update(sc->sc_ieee);
        ATH_PS_INIT_UNLOCK(sc);
    }

    if (test_and_clear_bit(KEVENT_BEACON_DONE, &sc->kevent_flags))
    {
        ath_tx_bcn_notify(sc);
    }
    
    clear_bit(0, (void *)&smp_kevent_Lock);
    up(&sc->ioctl_sem);
}

int ath_usb_create_thread(void *p)
{
    struct ath_softc *sc = (struct ath_softc *)p;

    /* Create Mutex and keventd */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20))
    INIT_WORK(&sc->kevent, kevent, sc);
#else
    INIT_WORK(&sc->kevent, kevent);
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,37)
    init_MUTEX(&sc->ioctl_sem);
#else
    sema_init(&sc->ioctl_sem, 1);
#endif

    return 0;
}

void ath_usb_schedule_thread(void *p, int flag)
{
    struct ath_softc *sc = (struct ath_softc *)p;

    if (sc == NULL)
    {
        printk("sc is NULL\n");
        return;
    }

//    if (0 && macp->kevent_ready != 1)
//    {
//        printk("Kevent not ready\n");
//        return;
//    }

    set_bit(flag, &sc->kevent_flags);

    if (!schedule_work(&sc->kevent))
    {
        //Fails is Normal
        //printk(KERN_ERR "schedule_task failed, flag = %x\n", flag);
    }
}
#else
int ath_usb_create_thread(void *p)
{
    struct ath_softc *sc = (struct ath_softc *)p;
    return 0;
}
void ath_usb_schedule_thread(void *p, int flag)
{
    struct ath_softc *sc = (struct ath_softc *)p;
    switch (flag) {
        case KEVENT_BEACON_STUCK:
            ath_wmi_beacon_stuck(sc);
            break;
        case KEVENT_BEACON_IMMUNITY:
            ath_wmi_beacon_immunity(sc);
            break;  
        case KEVENT_BEACON_SETSLOTTIME:
            ath_wmi_beacon_setslottime(sc);
            break;                                   
        case KEVENT_BEACON_DONE:
            ath_tx_bcn_notify(sc);
            break;
        default:
            break;        
    }
}
#endif
#endif /* ATH_USB */

