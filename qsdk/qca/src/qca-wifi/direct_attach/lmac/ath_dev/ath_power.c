/*
 * Copyright (c) 2009, Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */

#include "ath_internal.h"
#include "ath_power.h"

#ifdef QCA_SUPPORT_CP_STATS
#include <wlan_cp_stats_ic_utils_api.h>
#endif

void
ath_pcie_pwrsave_enable_on_phystate_change(struct ath_softc *sc, int enable)
{
    if (!sc->sc_config.pcieDisableAspmOnRfWake)
        return;

    ath_pcie_pwrsave_enable(sc, enable);
}

void
ath_pcie_pwrsave_enable(struct ath_softc *sc, int enable)
{
    u_int8_t    byte = 0;
    u_int8_t    pcieLcrOffset = PCIE_CAP_LINK_CTRL;

    if (sc->sc_config.pcieLcrOffset)
        pcieLcrOffset = sc->sc_config.pcieLcrOffset;

    if (ath_hal_isPcieEntSyncEnabled(sc->sc_ah) && ath_hal_hasPciePwrsave(sc->sc_ah)) {
        OS_PCI_READ_CONFIG(sc->sc_osdev, pcieLcrOffset, &byte, 1);
        /* Set Extended Sycn bit for L0s WAR on some chips */
        byte |= PCIE_CAP_LINK_EXT_SYNC;
        OS_PCI_WRITE_CONFIG(sc->sc_osdev, pcieLcrOffset, &byte, 1);
    }

    if (!ath_hal_isPciePwrsaveEnabled(sc->sc_ah) || !ath_hal_hasPciePwrsave(sc->sc_ah))
        return;

#ifdef ATH_BT_COEX
    if (sc->sc_hasbtcoex && ath_hal_bt_aspm_war(sc->sc_ah) && ath_bt_coex_get_info(sc, ATH_BT_COEX_INFO_SCHEME, NULL)) {
        /* BT coex scheme is enabled. */
        sc->sc_btinfo.wlan_aspm = enable ? sc->sc_config.pcieAspm : 0;
        return;
    }
#endif
  
    OS_PCI_READ_CONFIG(sc->sc_osdev, pcieLcrOffset, &byte, 1);

    /* Clear the bits if they are set, then or in the desired bits */
    byte &= ~(PCIE_CAP_LINK_L0S | PCIE_CAP_LINK_L1);
    if (enable) {
        byte |= sc->sc_config.pcieAspm;
    }

    OS_PCI_WRITE_CONFIG(sc->sc_osdev, pcieLcrOffset, &byte, 1);
}

/*
 * Notification of device suprise removal event
 */
void
ath_notify_device_removal(ath_dev_t dev)
{
    ATH_DEV_TO_SC(dev)->sc_invalid = 1;
    ATH_DEV_TO_SC(dev)->sc_removed = 1;
}

/*
 * Detect card present
 */
int
ath_detect_card_present(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;

    return ath_hal_detectcardpresent(ah);
}


/*
 * To query software PHY state
 */
int
ath_get_sw_phystate(ath_dev_t dev)
{
    return ATH_DEV_TO_SC(dev)->sc_sw_phystate;
}

/*
 * To query hardware PHY state
 */
int
ath_get_hw_phystate(ath_dev_t dev)
{
    return ATH_DEV_TO_SC(dev)->sc_hw_phystate;
}

/*
 * To set software PHY state
 * NB: this is just to set the software state. To turn on/off
 * radio, ath_radio_enable()/ath_radio_disable() has to be
 * explicitly called.
 */
void
ath_set_sw_phystate(ath_dev_t dev, int swstate)
{
    ATH_DEV_TO_SC(dev)->sc_sw_phystate = swstate;
}

/*
 * To disable PHY (radio off)
 */
int
ath_radio_disable(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    HAL_STATUS status;
    HAL_HT_MACMODE ht_macmode = sc->sc_ieee_ops->cwm_macmode(sc->sc_ieee);

    if (sc->sc_invalid)
        return -EIO;

#if ATH_RESET_SERIAL
    ATH_RESET_ACQUIRE_MUTEX(sc);
    ATH_RESET_LOCK(sc);
    atomic_inc(&sc->sc_hold_reset);
    ath_reset_wait_tx_rx_finished(sc);
    ATH_RESET_UNLOCK(sc);
#endif

    ATH_PS_WAKEUP(sc);
    
    /*
     * notify LED module radio has been turned off
     * This function will access the hw, so we must call it before 
     * the power save function.
     */
    ath_led_disable(&sc->sc_led_control);

    ath_hal_intrset(ah, 0);    /* disable interrupts */

    ath_reset_draintxq(sc, AH_FALSE, 0); /* stop xmit side */
    ATH_USB_TX_STOP(sc->sc_osdev);
    ATH_HTC_DRAINTXQ(sc);          /* stop target xmit */
    ATH_STOPRECV(sc, 0);        /* stop recv side */
    ath_flushrecv(sc);      /* flush recv queue */

#if ATH_C3_WAR
    STOP_C3_WAR_TIMER(sc);
#endif
#if !ATH_RESET_SERIAL
    ATH_LOCK_PCI_IRQ(sc);
#endif
#ifdef QCA_SUPPORT_CP_STATS
    pdev_lmac_cp_stats_ast_halresets_inc(sc->sc_pdev, 1);
#else
    sc->sc_stats.ast_halresets++;
#endif
    if (!ath_hal_reset(ah, sc->sc_opmode, &sc->sc_curchan,
                       ht_macmode,
                       sc->sc_tx_chainmask, sc->sc_rx_chainmask,
                       sc->sc_ht_extprotspacing, AH_FALSE, &status,
                       sc->sc_scanning)) {
        printk("%s: unable to reset hardware; hal status %u\n",
               __func__, status);
    }
    ATH_HTC_ABORTTXQ(sc);
#if !ATH_RESET_SERIAL
    ATH_UNLOCK_PCI_IRQ(sc);
#endif

    ATH_USB_TX_START(sc->sc_osdev);
    ath_hal_phydisable(ah);

#ifdef ATH_RFKILL
    if (sc->sc_hasrfkill) {
        ath_hal_intrset(ah, HAL_INT_GLOBAL | HAL_INT_GPIO);    /* Enable interrupt to capture GPIO event */
    }
#endif
    
    /* Turn on PCIE ASPM during RF Silence */
    ath_pcie_pwrsave_enable_on_phystate_change(sc, 1);
    /*
     * XXX TODO: We should put chip to forced sleep when radio is disabled. 
     */

    ath_pwrsave_fullsleep(sc);
    ATH_PS_SLEEP(sc);
#if ATH_RESET_SERIAL
    atomic_dec(&sc->sc_hold_reset);
    ATH_RESET_RELEASE_MUTEX(sc);
#endif

    return 0;
}

/*
 * To enable PHY (radio on)
 */
int
ath_radio_enable(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    HAL_STATUS status;
    HAL_HT_MACMODE ht_macmode = sc->sc_ieee_ops->cwm_macmode(sc->sc_ieee);

    if (sc->sc_invalid)
        return -EIO;
    
#if ATH_RESET_SERIAL
    ATH_RESET_ACQUIRE_MUTEX(sc);
#endif    
    ATH_PS_WAKEUP(sc);

    ath_pwrsave_awake(sc);

    /* Turn off PCIE ASPM when card is active */
    ath_pcie_pwrsave_enable_on_phystate_change(sc, 0);

    ATH_USB_TX_STOP(sc->sc_osdev);
#if ATH_C3_WAR
    STOP_C3_WAR_TIMER(sc);
#endif
#if !ATH_RESET_SERIAL
    ATH_LOCK_PCI_IRQ(sc);
#endif
#ifdef QCA_SUPPORT_CP_STATS
    pdev_lmac_cp_stats_ast_halresets_inc(sc->sc_pdev, 1);
#else
    sc->sc_stats.ast_halresets++;
#endif
    if (!ath_hal_reset(ah, sc->sc_opmode, &sc->sc_curchan,
                       ht_macmode,
                       sc->sc_tx_chainmask, sc->sc_rx_chainmask,
                       sc->sc_ht_extprotspacing, AH_FALSE, &status,
                       sc->sc_scanning)) {
        printk("%s: unable to reset hardware; hal status %u\n",
               __func__, status);
    }
#if !ATH_RESET_SERIAL
    ATH_UNLOCK_PCI_IRQ(sc);
#endif

    ath_update_txpow(sc, sc->tx_power); /* update tx power state */

    ath_beacon_config(sc, ATH_BEACON_CONFIG_REASON_RESET, ATH_IF_ID_ANY);   /* restart beacons */
    ath_hal_intrset(ah, sc->sc_imask);

    ath_wmi_start_recv(sc);
    if (ATH_STARTRECV(sc) != 0)    { /* restart recv */
        printk("%s: unable to start recv logic\n",
               __func__);
    }

    ATH_USB_TX_START(sc->sc_osdev);
    /*
     * notify LED module radio has been turned on
     * This function will access the hw, so we must call it after 
     * the power save function.
     */
    ath_led_enable(&sc->sc_led_control);

    ATH_PS_SLEEP(sc);
#if ATH_RESET_SERIAL
    ATH_RESET_RELEASE_MUTEX(sc);
#endif

    return 0;
}

#if ATH_SUPPORT_POWER

/*
 * handles power save state transitions.
 */

#ifdef ATH_USB
static inline int
ath_pwrsave_always_awake(struct ath_softc *sc)
{
    /* Under acceptable timing delay, keep always awake if P2P related functions works. */
    if (sc->sc_nvaps > 1){
        int slot, awake = 0;
        struct ath_vap *avp;
            
        for (slot = 0; slot < ATH_VAPSIZE; slot++){
            avp = sc->sc_vaps[slot];
            if (avp &&
                ((avp->av_opmode == HAL_M_STA) || (avp->av_opmode == HAL_M_HOSTAP)))
                 awake++;
        }
        
        if (awake) return 1;
    }
    
    return 0;
}
#endif

int
ath_pwrsave_get_state(ath_dev_t dev)
{
    return ATH_DEV_TO_SC(dev)->sc_pwrsave.ps_pwrsave_state;
}

static void
ath_pwrsave_set_state_sync(struct ath_softc *sc)
{
    struct ath_hal *ah = sc->sc_ah;
    ATH_PWRSAVE_STATE newstate = (sc)->sc_pwrsave.ps_set_state;

    if(sc->sc_pwrsave.ps_pwrsave_state == newstate) 
        return;
    if (sc->sc_removed) 
        return;

    switch (sc->sc_pwrsave.ps_pwrsave_state) {

    case ATH_PWRSAVE_NETWORK_SLEEP:
        switch (newstate) {
        case ATH_PWRSAVE_AWAKE:
            if (!ATH_PS_ALWAYS_AWAKE(sc)){
                ath_hal_setpower(ah, HAL_PM_AWAKE);

                /* 
                 * Must clear RxAbort bit manually if hardware does not support 
                 * automatic sleep after waking up for TIM.
                 */
                if (! sc->sc_hasautosleep) {
                    u_int32_t    imask;

            

                    ath_hal_setrxabort(ah, 0);
                

                    /* Disable TIM_TIMER interrupt */
                    imask = ath_hal_intrget(ah);

                    if ((imask & HAL_INT_TIM_TIMER) ||
                        ((imask & HAL_INT_TSFOOR) == 0)) {
                        sc->sc_imask &= ~HAL_INT_TIM_TIMER;
                        imask &= ~HAL_INT_TIM_TIMER;

                        imask |= HAL_INT_TSFOOR;
                        sc->sc_imask |= HAL_INT_TSFOOR;


                        ath_hal_intrset(ah, imask);

                    }

                }

            }
#if ATH_TX_DUTY_CYCLE
            if (sc->sc_tx_dc_enable) {
                u_int32_t duration;
                /* re-arm tx duty cycle */
                DPRINTF(sc, ATH_DEBUG_ANY, "%s: %d=>%d: re-enable quiet time: %u%% active\n", 
                        __func__, sc->sc_pwrsave.ps_pwrsave_state, newstate, sc->sc_tx_dc_active_pct);
                if (sc->sc_opmode == HAL_M_HOSTAP && sc->sc_nbcnvaps != 0) {
                    ath_beacon_config(sc, ATH_BEACON_CONFIG_REASON_RESET, ATH_IF_ID_ANY);
                }else  if (sc->sc_nvaps){
                    duration = ((ATH_TX_DUTY_CYCLE_MAX-sc->sc_tx_dc_active_pct)*sc->sc_tx_dc_period)/ATH_TX_DUTY_CYCLE_MAX;
                    ath_hal_setQuiet(sc->sc_ah, sc->sc_tx_dc_period, duration, 0, AH_TRUE);
                }
            }
#endif
            break;
        case ATH_PWRSAVE_FULL_SLEEP:
            /*
             * Stop both receive PCU and DMA before going full sleep to prevent deaf mute.
             */
            ath_hal_setrxabort(ah, 1);
            ath_hal_stopdmarecv(ah, 0); 
            ath_hal_setpower(ah, HAL_PM_FULL_SLEEP);
            break;

        default:
            break;
        }
        break;

    case ATH_PWRSAVE_AWAKE:
        switch (newstate) {
        case ATH_PWRSAVE_NETWORK_SLEEP:
	        if (!ATH_PS_ALWAYS_AWAKE(sc)){
                /* 
                 * Chips that do not support automatic sleep after waking up to 
                 * receive TIM must make sure at least one beacon is received 
                 * before reentering network sleep.
                 */
#if ATH_TX_DUTY_CYCLE
                if (sc->sc_tx_dc_enable) {
                    /* disarm tx duty cycle */
                    DPRINTF(sc, ATH_DEBUG_ANY, "%s: %d=>%d: disable quiet time: %u%% active\n", 
                            __func__, sc->sc_pwrsave.ps_pwrsave_state, newstate, sc->sc_tx_dc_active_pct);
                    ath_hal_setQuiet(sc->sc_ah, 0, 0, 0, AH_FALSE);
                }
#endif
                if (! sc->sc_hasautosleep) {
                    /* 
                     * Do not enter network sleep if no beacon received
                     */
                    if (! sc->sc_waitbeacon) {
                        u_int32_t    imask;
                        
                        /* Enable TIM_TIMER interrupt */
                        imask = ath_hal_intrget(ah);
                        
                        if (((imask & HAL_INT_TIM_TIMER) == 0) ||
                            (imask & HAL_INT_TSFOOR)){
                            sc->sc_imask |= HAL_INT_TIM_TIMER;
                            imask |= HAL_INT_TIM_TIMER;

                            imask &= ~HAL_INT_TSFOOR;
                            sc->sc_imask &= ~HAL_INT_TSFOOR;

                            ath_hal_intrset(ah, imask);
                        } 
                        
                        /* Stop RX state machine */
                        if (ath_hal_setrxabort(ah, 1)) {
                            ath_hal_setpower(ah, HAL_PM_NETWORK_SLEEP);
                        }
                    }
                }
                else {
                    ath_hal_setpower(ah, HAL_PM_NETWORK_SLEEP);
                }
            }
            break;
        case ATH_PWRSAVE_FULL_SLEEP:
            /*
             *  Must abort rx prior to full sleep for 2 reasons:
             *
             * 1. Hardware does not support automatic sleep (sc_hasautosleep=0)
             *    after waking up for TIM.
             * 2. WAR for EV68448 - card disappearance.
             *    Ideally, rx would be stopped at a higher level
             *    but the code was removed because it broke RFKILL (see EV66300).
             * 3. Stop both receive PCU and DMA before going full sleep to prevent deaf mute.
             */
#if ATH_TX_DUTY_CYCLE
            if (sc->sc_tx_dc_enable) {
                /* disarm tx duty cycle */
                DPRINTF(sc, ATH_DEBUG_ANY, "%s: %d=>%d: disable quiet time: %u%% active\n", 
                        __func__, sc->sc_pwrsave.ps_pwrsave_state, newstate, sc->sc_tx_dc_active_pct);
                ath_hal_setQuiet(sc->sc_ah, 0, 0, 0, AH_FALSE);
            }
#endif
            ath_hal_setrxabort(ah, 1);
            ath_hal_stopdmarecv(ah, 0); 
            ath_hal_setpower(ah, HAL_PM_FULL_SLEEP);
            break;

        default:
            break;
        }
        break;

    case ATH_PWRSAVE_FULL_SLEEP:
        switch (newstate) {
        case ATH_PWRSAVE_AWAKE:
            ath_hal_setpower(ah, HAL_PM_AWAKE);

            /* 
             * Must clear RxAbort bit manually if hardware does not support 
             * automatic sleep after waking up for TIM.
             */
            if (! sc->sc_hasautosleep) {
                u_int32_t    imask;
            
                ath_hal_setrxabort(ah, 0);
                
                /* Disable TIM_TIMER interrupt */
                imask = ath_hal_intrget(ah);
                if (imask & HAL_INT_TIM_TIMER) {
                    sc->sc_imask &= ~HAL_INT_TIM_TIMER;
                    ath_hal_intrset(ah, imask & ~HAL_INT_TIM_TIMER);
                }
            }
#if ATH_TX_DUTY_CYCLE
            if (sc->sc_tx_dc_enable) {
                u_int32_t duration;
                /* re-arm tx duty cycle */
                DPRINTF(sc, ATH_DEBUG_ANY, "%s: %d=>%d: re-enable quiet time: %u%% active\n", 
                        __func__, sc->sc_pwrsave.ps_pwrsave_state, newstate, sc->sc_tx_dc_active_pct);
                if (sc->sc_opmode == HAL_M_HOSTAP && sc->sc_nbcnvaps != 0) {
                    ath_beacon_config(sc, ATH_BEACON_CONFIG_REASON_RESET, ATH_IF_ID_ANY);
                }else if (sc->sc_nvaps){
                    duration = ((100-sc->sc_tx_dc_active_pct)*sc->sc_tx_dc_period)/100;
                    ath_hal_setQuiet(sc->sc_ah, sc->sc_tx_dc_period, duration, 0, AH_TRUE);
                }
            }
#endif
            break;
        default:
            break;
        }
    default:
        break;

    }
    sc->sc_pwrsave.ps_pwrsave_state = newstate;

    /* If chip has been put in full sleep, make sure full reset is called */
    if (sc->sc_pwrsave.ps_pwrsave_state == ATH_PWRSAVE_FULL_SLEEP)
        sc->sc_full_reset = 1;
}

void
ath_pwrsave_set_state(ath_dev_t dev, int newstateint)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    ATH_PWRSAVE_STATE newstate = (ATH_PWRSAVE_STATE)newstateint;
	HAL_BOOL    is_disable_intr = (((newstate == ATH_PWRSAVE_FULL_SLEEP))
	                        && sc->sc_msi_enable);
	HAL_BOOL    is_enable_intr = ((newstate == ATH_PWRSAVE_AWAKE 
                            && sc->sc_pwrsave.ps_pwrsave_state == ATH_PWRSAVE_FULL_SLEEP)
                            && sc->sc_msi_enable);
#ifdef ATH_BT_COEX
    HAL_BOOL exit_ns = (sc->sc_pwrsave.ps_pwrsave_state == ATH_PWRSAVE_NETWORK_SLEEP) &&
                        (newstate == ATH_PWRSAVE_AWAKE);
    HAL_BOOL enter_ns = (newstate == ATH_PWRSAVE_NETWORK_SLEEP);
#endif

#ifdef ATH_BT_COEX
    if (sc->sc_hasbtcoex && enter_ns) {
        u_int32_t sleep = 1;

        ATH_PS_UNLOCK(sc);
        ath_bt_coex_event(sc, ATH_COEX_EVENT_WLAN_NETWORK_SLEEP, &sleep);
        ATH_PS_LOCK(sc);
    }
#endif

    sc->sc_pwrsave.ps_set_state = newstate;
    if (is_disable_intr)
        ATH_INTR_DISABLE(sc);
    OS_EXEC_INTSAFE(sc->sc_osdev, ath_pwrsave_set_state_sync, sc);
    if (is_enable_intr)
        ATH_INTR_ENABLE(sc);

#ifdef ATH_BT_COEX
    if (sc->sc_hasbtcoex && exit_ns) {
        u_int32_t sleep = 0;

        ATH_PS_UNLOCK(sc);
        ath_bt_coex_event(sc, ATH_COEX_EVENT_WLAN_NETWORK_SLEEP, &sleep);
        ATH_PS_LOCK(sc);
    }
#endif

}

void    
ath_pwrsave_awake(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    ATH_PS_LOCK(sc);
    sc->sc_pwrsave.ps_restore_state = ATH_PWRSAVE_AWAKE;
    if (sc->sc_pwrsave.ps_pwrsave_state != ATH_PWRSAVE_AWAKE) { 
        ath_pwrsave_set_state(sc, ATH_PWRSAVE_AWAKE); 
    }
    ATH_PS_UNLOCK(sc);
}

void
ath_pwrsave_fullsleep(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    ATH_PS_LOCK(sc);

    /*
     * if nobody is using hal, then put it back to
     * sleep. 
     */
    if (sc->sc_pwrsave.ps_hal_usecount) 
        sc->sc_pwrsave.ps_restore_state = ATH_PWRSAVE_FULL_SLEEP;
    else {
        ath_pwrsave_set_state(sc, ATH_PWRSAVE_FULL_SLEEP);
    }
    ATH_PS_UNLOCK(sc);
}

void
ath_pwrsave_netsleep(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    ATH_PS_LOCK(sc);
    /*
     * if nobody is using hal, then put it back to
     * sleep. 
     */
    if (sc->sc_pwrsave.ps_hal_usecount) {
        if (sc->sc_pwrsave.ps_restore_state != ATH_PWRSAVE_FULL_SLEEP)
            sc->sc_pwrsave.ps_restore_state = ATH_PWRSAVE_NETWORK_SLEEP;
    } else {
        if (sc->sc_pwrsave.ps_pwrsave_state != ATH_PWRSAVE_FULL_SLEEP) {
            ath_pwrsave_set_state(sc, ATH_PWRSAVE_NETWORK_SLEEP);
        }
    }
    ATH_PS_UNLOCK(sc);
}

void
ath_pwrsave_proc_intdone(struct ath_softc *sc, u_int32_t intrStatus)
{
    ATH_PWRSAVE_STATE new_state;
    ATH_PS_LOCK(sc);

    new_state = sc->sc_pwrsave.ps_pwrsave_state;
    /* 
     * any interrupt atomatically wakens the HW. 
     * so push the state machine to AWAKE state
     * to keep the HW and SW in sync.
     */
    if (sc->sc_pwrsave.ps_hal_usecount) {
        sc->sc_pwrsave.ps_restore_state = new_state;
    } else {
        sc->sc_pwrsave.ps_pwrsave_state = ATH_PWRSAVE_AWAKE;
        ath_pwrsave_set_state(sc, new_state);
    }
            
    ATH_PS_UNLOCK(sc);
}

void
ath_pwrsave_init(struct ath_softc *sc)
{
    u_int8_t    pcieLcrOffset = PCIE_CAP_LINK_CTRL;

    if (sc->sc_config.pcieLcrOffset)
        pcieLcrOffset = sc->sc_config.pcieLcrOffset;
    /* init the HW and SW state to awake */
    sc->sc_pwrsave.ps_hal_usecount  = 0;
    sc->sc_pwrsave.ps_restore_state = ATH_PWRSAVE_AWAKE;
    sc->sc_pwrsave.ps_pwrsave_state = ATH_PWRSAVE_AWAKE;
    ath_hal_setpower(sc->sc_ah, HAL_PM_AWAKE);
    /* 
     * Must clear RxAbort bit manually if hardware does not support 
     * automatic sleep after waking up for TIM.
     */
    if (! sc->sc_hasautosleep) {
        ath_hal_setrxabort(sc->sc_ah, 0);
    }

#ifdef ATH_BT_COEX
    if (sc->sc_hasbtcoex && ath_hal_bt_aspm_war(sc->sc_ah)) {
        OS_PCI_READ_CONFIG(sc->sc_osdev, pcieLcrOffset, &sc->sc_btinfo.wlan_aspm, 1);

        sc->sc_btinfo.wlan_aspm &= (PCIE_CAP_LINK_L0S | PCIE_CAP_LINK_L1);
    }
#endif
}

#endif

#ifdef ATH_BT_COEX
void
ath_pcie_pwrsave_btcoex_enable(struct ath_softc *sc, int enable)
{
    u_int8_t    byte = 0;
    u_int8_t    pcieLcrOffset = PCIE_CAP_LINK_CTRL;

    if (sc->sc_config.pcieLcrOffset)
        pcieLcrOffset = sc->sc_config.pcieLcrOffset;
    if (ath_hal_bt_aspm_war(sc->sc_ah)) {
        OS_PCI_READ_CONFIG(sc->sc_osdev, pcieLcrOffset, &byte, 1);

        /* Clear the bits if they are set, then or in the desired bits */
        byte &= ~(PCIE_CAP_LINK_L0S | PCIE_CAP_LINK_L1);

        if (enable) {
            /* BT coex is enabled. Disable ASPM. */
        }
        else {
            /* Restore the saved value. */
            byte |= sc->sc_btinfo.wlan_aspm;
        }

        OS_PCI_WRITE_CONFIG(sc->sc_osdev, pcieLcrOffset, &byte, 1);
    }
}
#endif
