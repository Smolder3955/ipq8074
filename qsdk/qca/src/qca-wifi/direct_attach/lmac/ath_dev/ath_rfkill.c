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
#include "ath_rfkill.h"

#ifdef ATH_RFKILL

#define	ATH_RFKILL_POLL_INTERVAL    2000    /* 2 second */

static void ath_reset_rfkill(struct ath_softc *sc);

int
ath_rfkill_attach(struct ath_softc *sc)
{
    u_int32_t rfsilent = 0;
    struct ath_hal *ah = sc->sc_ah;
    struct ath_rfkill_info *rfkill = &sc->sc_rfkill;

    OS_MEMZERO(rfkill, sizeof(struct ath_rfkill_info));
    
    ath_hal_getrfkillinfo(ah, &rfsilent);
    rfkill->rf_gpio_select = rfsilent & 0xffff;
    rfkill->rf_gpio_polarity = (rfsilent >> 16) & 0xffff;

    rfkill->rf_hasint = ath_hal_hasrfkillInt(ah) ? AH_TRUE : AH_FALSE;

    ath_hal_enable_rfkill(ah, AH_TRUE);
    ath_initialize_timer(sc->sc_osdev, &rfkill->rf_timer,
                         ATH_RFKILL_POLL_INTERVAL,
                         (timer_handler_function)ath_rfkill_poll,
                         sc);

    /*
     * This flag is used by the WAR for RkFill Delay during power resume.
     * This flag is used to skip the initial check after system boot.
     */
    rfkill->delay_chk_start = AH_FALSE;

    return 0;
}

void
ath_rfkill_detach(struct ath_softc *sc)
{
    ath_free_timer(&sc->sc_rfkill.rf_timer);
}

void
ath_rfkill_start_polling(struct ath_softc *sc)
{
    ath_start_timer(&sc->sc_rfkill.rf_timer);
}

void
ath_rfkill_stop_polling(struct ath_softc *sc)
{
    ath_cancel_timer(&sc->sc_rfkill.rf_timer, CANCEL_NO_SLEEP);
}

int
ath_rfkill_poll(struct ath_softc *sc)
{
     HAL_BOOL radioOn;

    if (!sc->sc_rfkillpollenabled || sc->sc_removed)
        return -EIO;
    
    if (!ath_rfkill_hasint(sc)) { 
        /* rfkill has problem to generate interrupt */
        radioOn = !ath_get_rfkill(sc);

        if (ath_get_hw_phystate(sc) != radioOn) {
            DPRINTF(sc, ATH_DEBUG_ANY, 
                    "%s: Change in hw phy state. New radioOn=%d\n", 
                    __func__, radioOn);
            ath_hw_phystate_change(sc, radioOn);
        }
    } else {
        /* The spin and wait for interrupt method ... */
        ATH_PS_WAKEUP(sc);
        OS_DELAY(2);
        ATH_PS_SLEEP(sc);
    }
    
    return 0;
}

/*
 * Handle GPIO interrupt
 */
HAL_BOOL
ath_rfkill_gpio_isr(struct ath_softc *sc)
{
    HAL_BOOL radioOn;
    
    /* Don't support GPIO interrupt at all */
    if (!ath_rfkill_hasint(sc))
        return AH_FALSE;

    radioOn = !ath_get_rfkill(sc);

    /* If RfKill status changed, it could be RfKill GPIO interrupt */
    if (ath_get_hw_phystate(sc) != radioOn) {
        ath_reset_rfkill(sc);
        sc->sc_rfkill.rf_phystatechange = AH_TRUE;
        return AH_TRUE;
    }
    return AH_FALSE;
}

void
ath_rfkill_gpio_intr(struct ath_softc *sc)
{
    /* Don't support GPIO interrupt at all */
    if (!ath_rfkill_hasint(sc))
        return;
    
    if (sc->sc_rfkill.rf_phystatechange) {
        sc->sc_rfkill.rf_phystatechange = AH_FALSE;
        ath_hw_phystate_change(sc, !ath_get_hw_phystate(sc));
    }
}

/*
 * Get RfKill status. If it's not enabled in the EEPROM, always return FALSE.
 */
HAL_BOOL
ath_get_rfkill(struct ath_softc *sc)
{
    struct ath_hal *ah = sc->sc_ah;
    int select = sc->sc_rfkill.rf_gpio_select;
    int polarity = sc->sc_rfkill.rf_gpio_polarity;

    if (ath_hal_isrfkillenabled(ah))
        return (ath_hal_gpioGet(ah, select) == polarity);
    else
        return AH_FALSE;
}

/*
 * Enable RfKill by flip the polarity of the GPIO interrupt.
 */
static void
ath_reset_rfkill(struct ath_softc *sc)
{
    struct ath_hal *ah = sc->sc_ah;
    int select = sc->sc_rfkill.rf_gpio_select;
    int polarity = sc->sc_rfkill.rf_gpio_polarity;

    if (!ath_rfkill_hasint(sc))
        return;
    
    ath_hal_gpioSetIntr(ah, select,
        (ath_hal_gpioGet(ah, select) == polarity) ?
        !polarity : polarity);
}

/*
 * WAR for Bug 33276: In certain systems, the RfKill signal is slow to
 * stabilize when waking up from power suspend. The RfKill is an
 * active-low GPIO signal that could be slow to rise from 0V to VCC.
 * Due to this slow transition after a power resume, the driver might 
 * wrongly read the RfKill is active.
 * For this workaround, when the power resumes and the RfKill is found to be
 * active and its previous state before sleep is inactive, then we will
 * delayed implementing the new RfKill state and poll for up to 1.0 secs.
 *
 * WARNING: Be careful when calling this routine since it will stall the
 * CPU for up to 1 seconds. Ideally, you should call this routine at the
 * PASSIVE level.
 */
void
ath_rfkill_delay_detect(struct ath_softc *sc)
{
    int wait_time = 0;
    HAL_BOOL first_time_after_attach;

    /* 
     * MAX_RFKILL_DELAY_WAIT_TIME is max. time to wait for the RfKill to 
     * transition from ON to OFF. Units is microsec.
     */
    #define MAX_RFKILL_DELAY_WAIT_TIME      1000000
    /* Time interval to check RfKill. Units is microsec. */
    #define RFKILL_PERIODIC_CHK_TIME        20000

    first_time_after_attach = !sc->sc_rfkill.delay_chk_start;
    sc->sc_rfkill.delay_chk_start = AH_TRUE;
    
    /*
     * Workaround not possible or required for any of the following reasons:
     * 1) The hardware can generate the RfKill interrupt,
     * 2) First time after system boot,
     * 3) Last known hardware state is already OFF,
     * 4) Current hardware state is ON (RfKill is disabled).
     */
    if (ath_rfkill_hasint(sc) ||
        first_time_after_attach ||
        !sc->sc_hw_phystate ||
        !ath_get_rfkill(sc)) {
        return;
    }

    /* Assumed that the RfKill Poll is not active */
    ASSERT(!ath_timer_is_initialized(&sc->sc_rfkill.rf_timer) || 
           !ath_timer_is_active(&sc->sc_rfkill.rf_timer));

    /*
     * Wait for the RfKill to switch back to active
     */
    while (ath_get_rfkill(sc)) {
        /*
         * Bad! We are stalling the CPU for long period of time. Please make
         * sure your OSes can handle this.
         */
        OS_DELAY(RFKILL_PERIODIC_CHK_TIME);
        wait_time += RFKILL_PERIODIC_CHK_TIME;
        if (wait_time >= MAX_RFKILL_DELAY_WAIT_TIME) {
            /*
             * Waited too long. Assumed that the RfKill has stabilized.
             */
            DPRINTF(sc, ATH_DEBUG_ANY, 
                    "%s: RfKill state did not changed after delay of %d millisec.\n",
                   __func__, (wait_time/1000));
            return;
        }
    }
    /*
     * The RfKill has changed. Update the sw phy state.
     */
    DPRINTF(sc, ATH_DEBUG_ANY, 
            "%s: Warning: RfKill state has switch to OFF after delay of %d millisec.\n",
           __func__, (wait_time/1000));

    return;

#undef MAX_RFKILL_DELAY_WAIT_TIME
#undef RFKILL_PERIODIC_CHK_TIME
}

void
ath_enable_RFKillPolling(ath_dev_t dev, int enable)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    if (!sc->sc_hasrfkill) {
		return;
    }

	if (enable) {
		if (!sc->sc_rfkillpollenabled) {
			sc->sc_rfkillpollenabled = 1;
		    ath_rfkill_poll(sc);
		    ath_rfkill_start_polling(sc);
		}
	}
	else if (sc->sc_rfkillpollenabled) {
		sc->sc_rfkillpollenabled = 0;
		ath_rfkill_stop_polling(sc);
	}
}

#endif /* ATH_RFKILL */
