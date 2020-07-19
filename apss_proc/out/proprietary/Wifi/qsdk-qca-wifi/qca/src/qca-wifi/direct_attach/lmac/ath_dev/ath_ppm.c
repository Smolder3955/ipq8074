/*
 * Copyright (c) 2009, Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 *
 *  Implementation of LED control module.
 */


#include "wlan_opts.h"
#include "ath_internal.h"

#if ATH_SUPPORT_FORCE_PPM

/* Bit values for debugging */
#define ATH_DEBUG_PPM_ENABLE        0x0001  /* any bit set will enable function */
#define ATH_DEBUG_PPM_PRINT         0x0002  /* print update mesages to console */
#define ATH_DEBUG_PPM_LOG           0x0004  /* log activity in packet log */
#define ATH_DEBUG_PPM_NOUPDATE      0x0008  /* do not actually force the new value */

#ifdef DEBUG
/* TEMP DEBUG ONLY */
int dumpForcePpmFlag = 0;
#endif

/* Internal states */
enum {
    ST_FORCE_PPM_INIT,
    ST_FORCE_PPM_ARMED,
    ST_FORCE_PPM_SEARCH,
    ST_FORCE_PPM_SCAN,
    ST_FORCE_PPM_IDLE,
    ST_FORCE_PPM_NEXT,

    FORCE_PPM_NUMBER_STATES,
};

#if ATH_DEBUG
static const char* stateName[] = {
    "INIT",
    "ARMED",
    "SEARCH",
    "SCAN",
    "IDLE",
    "NEXT",
};

static const char* eventName[] = {
    "DISABLE",
    "ENABLE",
    "SUSPEND",
    "RESUME",
};
#endif /* ATH_DEBUG */

/*
 * Reset force ppm tracking state and variables.
 * This function maintains some state information:
 *     -whether ForcePPM is running and/or suspended;
 *     -the current BSSI.
 */
static void
ath_force_ppm_restart (ath_force_ppm_t *afp)
{
    afp->timerStart1 = 0;
    afp->timerCount1 = 0;
    afp->timerThrsh1 = afp->force_ppm_update_timeout / ATH_FORCE_PPM_PERIOD;
    afp->timerStart2 = 0;
    afp->timerCount2 = 0;
    afp->timerThrsh2 = afp->force_ppm_inactivity_timeout / ATH_FORCE_PPM_PERIOD;
    afp->forceState  = ST_FORCE_PPM_INIT;
    afp->lastTsf1    = 0;
    afp->lastTsf2    = 0;
    afp->latchedRssi = 0x00808080;

    DPRINTF(afp->sc, ATH_DEBUG_PPM, "%s: active=%d timerThrsh1=%d timerThrsh2=%d\n", 
        __func__, 
        afp->ppmActive,
        afp->timerThrsh1,
        afp->timerThrsh2);

    /* remove force */
    ath_hal_ppmUnForce(afp->ah);

#ifdef DEBUG
    /* DEBUG */
    dumpForcePpmFlag = 0;
#endif
}

/*
 * Initialize force ppm tracking state and variables
 */
static void
ath_force_ppm_init (ath_force_ppm_t *afp)
{
    /*
     * Reset variables not affected by the restart operation.
     */
    afp->isConnected = 0;
    afp->isSuspended = 0;

    OS_MEMSET(afp->bssid, 0, sizeof(afp->bssid));

    ath_force_ppm_restart(afp);

    DPRINTF(afp->sc, ATH_DEBUG_PPM, "%s: active=%d timerThrsh1=%d timerThrsh2=%d\n", 
        __func__, 
        afp->ppmActive,
        afp->timerThrsh1,
        afp->timerThrsh2);
}

static int
ath_force_ppm_timertimeout (void *context)
{
    ath_force_ppm_t    *afp = (ath_force_ppm_t *) context;

    DPRINTF(afp->sc, ATH_DEBUG_PPM, "%s: St=%s Act=%d,%d,%d T1=%d cnt=%d/%d T2=%d cnt=%d/%d\n", 
        __func__, 
        stateName[afp->forceState],
        afp->ppmActive,
        afp->isConnected,
        afp->isSuspended,
        afp->timerStart1,
        afp->timerCount1,
        afp->timerThrsh1,
        afp->timerStart2,
        afp->timerCount2,
        afp->timerThrsh2);

    /* Synchronize access to PPM routine */
    ATH_PPM_LOCK(afp);

    if (afp->ppmActive && afp->isConnected && (! afp->isSuspended))
    {
        /*
         * timer1, one-shot, time when next sample window is to be started
         */
        if (afp->timerStart1)
        {
            afp->timerCount1++;
            if (afp->timerCount1 >= afp->timerThrsh1)
            {
                afp->timerStart1 = 0;
                afp->timerCount1 = 0;
            }
        }

        /*
         * timer2, retrig, times out on lack of receive activity
         */
        if (afp->timerStart2)
        {
            afp->timerStart2 = 0;
            afp->timerCount2 = 0;
        } 
        else 
        {
            afp->timerCount2++;
            if (afp->timerCount2 >= afp->timerThrsh2)
            {
                afp->timerCount2 = 0;
                /* re-start */
                ath_force_ppm_restart(afp);
            }
        }
    }

    /* Release spin lock */
    ATH_PPM_UNLOCK(afp);

    return (0);
}

#ifdef DEBUG_PKTLOG
/* 12 bit 2's complement */
static int
getTwosComplement12 (int val)
{
    int tmpval = val;
    int invval;

    if (val > 0x7ff)
    {
        tmpval = val & 0x7ff;
        invval = (~tmpval + 1) & 0x7ff;
        tmpval = 0 - invval;
    }
    return tmpval;
}
#endif

static int
ath_PpmIsInAggr (ath_force_ppm_t      *afp, 
                 struct ath_rx_status *rx_stats)
{
    /*
     * Check if frame is part of an aggregate but not the last one.
     * This result is used later since only the last frame in an aggregate
     * contains the RSSI value.
     */
    return ((rx_stats->rs_isaggr && rx_stats->rs_moreaggr) ? 1 : 0);
}

static int
ath_PpmIsRssi (ath_force_ppm_t *afp, 
               u_int32_t       latched_val, 
               u_int32_t       desc_val)
{
    struct ath_softc    *sc = afp->sc;
    u_int32_t           cmpMask;
    HAL_BOOL            useSwap = 0;

//  TODO: pre-calculate comp mask  
//    if (OS_REG_READ(ah, PHY_ANALOG_SWAP) & PHY_SWAP_ALT_CHAIN) {
    if ((sc->sc_rx_chainmask == 5) || (sc->sc_tx_chainmask == 5))
    {
        useSwap = 1;
    }

    switch (sc->sc_rx_chainmask)
    {
    case 1:
        cmpMask = 0x000000ff;
        break;
    case 3:
    case 5:
        cmpMask = useSwap ? 0x00ff00ff : 0x0000ffff;
        break;
    case 7:
        cmpMask = 0x00ffffff;
        break;
    default:
        cmpMask = 0x00000000;
        break;
    }
    /* return 1 only when cmpMask not 0 and the masked values equal;
     * return 0 otherwise 
     */
    return (cmpMask && ((latched_val & cmpMask) == (desc_val & cmpMask)));
}

/*
 * Try a comparison of the rx timestamp against the tsf of the trigger arm.
 * Rx timestamp is 32 bits, tsf is 64 bits at 1 uSec resolution.
 * If timestamp
 * If no frames received within xxx, external timer resets state
 */
static int
ath_PpmCheckTsf (u_int32_t test, u_int32_t rxTimestamp)
{
    if ((rxTimestamp >= test) || 
        ((rxTimestamp < test) && ((rxTimestamp - test) < 0x80000000)))
    {
        return 1;
    }
    return 0;
}

#ifdef DEBUG_PKTLOG
int32_t pktlog_misc[8];
#endif

/*
 * State machine to monitor ppm tracking.
 * Clocked on rx frame activity.
 */
int
ath_force_ppm_logic (ath_force_ppm_t      *afp, 
                     struct ath_buf       *bf, 
                     HAL_STATUS           status, 
                     struct ath_rx_status *rx_stats)
{
    int                       ppmIsTrig, ppmIsInAggr, ppmIsTsf, ppmIsRssi, ppmIsBssOk, ppmIsTsf2;
    unsigned int              ppmDescRssi;
    unsigned int              lastTsfLog;
    int                       forcePpmStateCur;
    u_int32_t                 thistsf;
    u_int32_t                 tempFine = -2047;
    int                       type;
    struct ath_softc          *sc = afp->sc;
    struct ath_hal            *ah = afp->ah;
    struct ieee80211_frame    *wh;

    if ((! afp->ppmActive) || (! afp->isConnected) || afp->isSuspended)
    {
        return (0);
    }

    ppmIsTrig        = 0;
    ppmIsTsf         = 0;
    ppmIsInAggr      = 0;
    ppmIsRssi        = 0;
    ppmIsBssOk       = 0;
    ppmDescRssi      = 0x00808080;
    ppmIsTsf2        = 0;
    lastTsfLog       = afp->lastTsf1;
    forcePpmStateCur = afp->forceState;

    /* Synchronize access to PPM routine */
    ATH_PPM_LOCK(afp);

    afp->timerStart2 = 1;  /* kick rx timeout */

    wh = (struct ieee80211_frame *)wbuf_raw_data(bf->bf_mpdu);
    type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;

    thistsf = ath_hal_gettsf32(ah);

    switch (forcePpmStateCur)
    {
    case ST_FORCE_PPM_INIT:
        /* INIT */
        afp->forceState = ST_FORCE_PPM_ARMED;
        afp->lastTsf1   = ath_hal_ppmArmTrigger(ah);
        break;

    case ST_FORCE_PPM_ARMED:
        /* ARMED */
        if (ath_hal_ppmGetTrigger(ah))
        {
            afp->lastTsf2 = thistsf;
            ppmIsTrig = 1;
            afp->latchedRssi = ath_hal_ppmGetRssiDump(ah);
            ppmDescRssi = ((rx_stats->rs_rssi_ctl0 & 0xff) <<  0) |
                          ((rx_stats->rs_rssi_ctl1 & 0xff) <<  8) |
                          ((rx_stats->rs_rssi_ctl2 & 0xff) << 16);
            ppmIsRssi   = ath_PpmIsRssi(afp, afp->latchedRssi, ppmDescRssi);
            ppmIsTsf    = ath_PpmCheckTsf(afp->lastTsf1, rx_stats->rs_tstamp);
            ppmIsInAggr = ath_PpmIsInAggr(afp, rx_stats);

            /*
             * Verify that the RX frame whose descriptor is passed to this 
             * function is the same frame on which the Base Band latched. 
             * This comparison is done by validating the RSSI, TSF and 
             * Aggregation Status.
             *
             * RSSI: If the descriptor matches the frame on which the Base Band
             *     then the RSSI must be exactly the same for the 2 samples.
             * Aggregation Status: Only the last frame in an aggregation has a
             *     valid RSSI, so we disregard the others.
             * Tsf: The frame being analyzed must have a TSF equal or greater 
             *     than the time when the trigger was armed.
             */
            if (ppmIsRssi && ppmIsTsf && !ppmIsInAggr)
            { /* FOO */

                if (((type == IEEE80211_FC0_TYPE_DATA) || (type == IEEE80211_FC0_TYPE_MGT)) &&
                    (ATH_ADDR_EQ(wh->i_addr3, afp->bssid)) &&
                    (status == HAL_OK) )
                {
                    /* Frame originated from our AP, is OK, and is Data or Mgmt type */
                    ppmIsBssOk = 1;
                }

                if (ppmIsBssOk)
                {
                    afp->forceState = ST_FORCE_PPM_NEXT;
                    tempFine = ath_hal_ppmForce(ah);

                    #ifdef DEBUG
                    /* TEMP DEBUG ONLY */
                    dumpForcePpmFlag = 1;
                    #endif
                } 
                else
                {
                    /* match but not us, restart */
                    afp->forceState = ST_FORCE_PPM_ARMED;
                    afp->lastTsf1 = ath_hal_ppmArmTrigger(ah);
                }
            } 
            else 
            {
                /* 
                 * If RSSI doesn't match  or tsf not reached, or in aggr then 
                 * the descriptor does not correspond to the frame on which 
                 * the Base Band latched. 
                 * Continue checking frames until timeout, then retrigger
                 */
                afp->forceState = ST_FORCE_PPM_SEARCH;
            }
        }
        /* else not triggered, stay in this state */
        break;

    case ST_FORCE_PPM_SEARCH:
        /* SEARCHING */
        ppmDescRssi = ((rx_stats->rs_rssi_ctl0 & 0xff) <<  0) |
                      ((rx_stats->rs_rssi_ctl1 & 0xff) <<  8) |
                      ((rx_stats->rs_rssi_ctl2 & 0xff) << 16);
        ppmIsRssi   = ath_PpmIsRssi(afp, afp->latchedRssi, ppmDescRssi);
        ppmIsTsf    = ath_PpmCheckTsf(afp->lastTsf1, rx_stats->rs_tstamp);
        ppmIsInAggr = ath_PpmIsInAggr(afp, rx_stats);
        ppmIsTsf2   = ath_PpmCheckTsf(afp->force_ppm_recovery_timeout + afp->lastTsf2, rx_stats->rs_tstamp);
        /*
         * Verify that the RX frame whose descriptor is passed to this 
         * function is the same frame on which the Base Band latched. 
         * This comparison is done by validating the RSSI, TSF and 
         * Aggregation Status.
         *
         * RSSI: If the descriptor matches the frame on which the Base Band
         *     then the RSSI must be exactly the same for the 2 samples.
         * Aggregation Status: Only the last frame in an aggregation has a
         *     valid RSSI, so we disregard the others.
         * Tsf: The frame being analyzed must have a TSF equal or greater 
         *     than the time when the trigger was armed.
         */
        if (ppmIsRssi && ppmIsTsf && !ppmIsInAggr)
        { /* FOO */
                
            if (((type == IEEE80211_FC0_TYPE_DATA) || (type == IEEE80211_FC0_TYPE_MGT)) &&
                (ATH_ADDR_EQ(wh->i_addr3, sc->sc_curbssid)) &&
                (status == HAL_OK) )
            {
                /* Frame originated from our AP, is OK, and is Data or Mgmt type */
                ppmIsBssOk = 1;
            }

            if (ppmIsBssOk)
            {
                afp->forceState = ST_FORCE_PPM_NEXT;
                tempFine = ath_hal_ppmForce(ah);

                #ifdef DEBUG
                /* TEMP DEBUG ONLY */
                dumpForcePpmFlag = 1;
                #endif
            } 
            else
            {
                /* match but not us, restart */
                afp->forceState = ST_FORCE_PPM_ARMED;
                afp->lastTsf1   = ath_hal_ppmArmTrigger(ah);
            }
        } else if (ppmIsTsf2 && !ppmIsInAggr) 
        {
            /*
             * do not need to try any more as these frames are at
             * least 12 ms newer than the most recent trigger
             * event, can restart
             */
            afp->forceState = ST_FORCE_PPM_ARMED;
            afp->lastTsf1   = ath_hal_ppmArmTrigger(ah);
        }
        /* else stay in this state */
        break;

    case ST_FORCE_PPM_IDLE:
        /* IDLE */
        if (!afp->timerStart1)
        {
            afp->forceState = ST_FORCE_PPM_ARMED;
            afp->lastTsf1   = ath_hal_ppmArmTrigger(ah);
        }
        break;

    case ST_FORCE_PPM_NEXT:
        /* START NEXT TIMER */
        afp->forceState = ST_FORCE_PPM_IDLE;
        afp->timerStart1 = 1;
        break;

    default:
        printk("PPM STATE ILLEGAL %x %x\n", forcePpmStateCur, afp->forceState);
        ASSERT(0);
        ath_force_ppm_restart(afp);
        break;
    }

    /* Release spin lock */
    ATH_PPM_UNLOCK(afp);

    if (afp->isConnected && (! afp->isSuspended) && (forcePpmStateCur != afp->forceState))
    {
        DPRINTF(afp->sc, ATH_DEBUG_PPM, "%s: St=%s->%s Act=%d,%d,%d type=%02X Trig=%d Tsf=%d Aggr=%d Rssi=%d BssOk=%d Tsf2=%d rssi=%d,%d,%d addr3=%02X:%02X:%02X:%02X:%02X:%02X\n",
            __func__, 
            stateName[forcePpmStateCur], stateName[afp->forceState],
            afp->ppmActive,
            afp->isConnected,
            afp->isSuspended,
            type,
            ppmIsTrig,
            ppmIsTsf,
            ppmIsInAggr,
            ppmIsRssi,
            ppmIsBssOk,
            ppmIsTsf2,
            rx_stats->rs_rssi_ctl0,
            rx_stats->rs_rssi_ctl1,
            rx_stats->rs_rssi_ctl2,
            wh->i_addr3[0], wh->i_addr3[1], wh->i_addr3[2],
            wh->i_addr3[3], wh->i_addr3[4], wh->i_addr3[5]);
    }

#ifdef DEBUG_PKTLOG
//                if(ah->staConfig.pktLogEnable & ATH_PKTLOG_RX)
    pktlog_misc[0] = thistsf.low;
    pktlog_misc[1] = lastTsfLog;
    pktlog_misc[2] = afp->lastTsf2;
    pktlog_misc[3] = (ppmIsTrig  << 31)                               | ((afp->forceState == ST_FORCE_PPM_IDLE) << 30) |
                     (ath_timer_is_active(&afp->timer) ? 1 << 29 : 0) | (ppmIsRssi   << 28) | 
                     (ppmIsTsf   << 27)                               | (ppmIsInAggr << 26) |
                     (afp->timerStart1 << 25)                         | (ppmIsBssOk  << 24);
    pktlog_misc[3] |= afp->latchedRssi;
    pktlog_misc[4] = (forcePpmStateCur << 28) | (afp->forceState << 24);
    pktlog_misc[4] |= (pStatusX->rssi & 0xff) |
                      (pStatusX->rssi_ctl_chain1 & 0xff) << 8 |
                      (pStatusX->rssi_ctl_chain2 & 0xff) << 16;
    pktlog_misc[5] = getTwosComplement12(tempFine);
    pktlog_misc[6] = (pHeaderX->frameControl.fType << 28) |
                     (pSrcAddr->st.word & 0xff000000) >> 8  |
                     (pSrcAddr->st.half & 0x00ff) << 8 | 
                     (pSrcAddr->st.half & 0xff00) >> 8;
#endif
#ifdef DEBUG
#ifdef DEBUG_PKTLOG
/* TEMP DEBUG ONLY */
    if (dumpForcePpmFlag)
    {
        dumpForcePpmFlag = 0;
        /* tempFine_2C, st_desc_rssi, src, thisTSF, armTSF, fine_reg, check_reg */
        printk("FORCE_PPM %4d %6.6x %8.8x %8.8x %8.8x %3.3x %4.4x\n",
            pktlog_misc[5],
            pktlog_misc[4],
            pktlog_misc[6],
            pktlog_misc[0],
            pktlog_misc[1],
            tempFine,
            ath_hal_ppmGetForceState(ah));
    }
#endif
#endif

    return 0;
}

/*
 * Indicate whether to enable or disable Force PPM.
 * Force PPM runs while in connected state and is momentarily disabled 
 * when scanning a foreign channel.
 */
void
ath_force_ppm_notify (ath_force_ppm_t *afp, int event, u_int8_t *bssid)
{
    if (! afp->ppmActive)
    {
        return;
    }

    /* Synchronize access to PPM routine */
    ATH_PPM_LOCK(afp);

    /* DEBUG */
    DPRINTF(afp->sc, ATH_DEBUG_PPM, "%s: %s\n",
        __func__, 
        eventName[event]);

    switch (event)
    {
    case ATH_FORCE_PPM_DISABLE:
        ath_force_ppm_init(afp);
        break;

    case ATH_FORCE_PPM_ENABLE:
        ASSERT(bssid != NULL);

        afp->isConnected = 1;
        OS_MEMCPY(afp->bssid, bssid, sizeof(afp->bssid));
        break;

    case ATH_FORCE_PPM_SUSPEND:
        afp->isSuspended = 1;

        ath_force_ppm_restart(afp);
        break;

    case ATH_FORCE_PPM_RESUME:
        afp->isSuspended = 0;
        break;

    /* Invalid event */
    default:
        ASSERT(0);
        break;
    }

    /* Release spin lock */
    ATH_PPM_UNLOCK(afp);
}


void
ath_force_ppm_attach (ath_force_ppm_t     *afp, 
                                 struct ath_softc    *sc, 
                                 osdev_t             sc_osdev,
                                 struct ath_hal      *ah,
                                 struct ath_reg_parm *pRegParam)
{
    DPRINTF(sc, ATH_DEBUG_PPM, "%s: forcePpmEnable=%d capability=%d\n",
        __func__, 
        pRegParam->forcePpmEnable,
        (ath_hal_getcapability(ah, HAL_CAP_FORCE_PPM, 0, NULL) == HAL_OK));

    /* Verify debug arrays have the correct length */
    ASSERT(ARRAY_LENGTH(stateName) == FORCE_PPM_NUMBER_STATES);
    ASSERT(ARRAY_LENGTH(eventName) == ATH_FORCE_PPM_NUMBER_EVENTS);

    /*
     * Start Force PPM module ONLY if it's enabled from the registry AND
     * chip supports it.
     * We also add a check for timerThrsh1. The algorithm cannot run if 
     * timerThrsh1==0.
     */

    /* First check if customer enabled/disabled feature. */
    if (pRegParam->forcePpmEnable)
    {
        /* Now check whether the hardware supports it. */
        if (ath_hal_getcapability(ah, HAL_CAP_FORCE_PPM, 0, NULL) == HAL_OK)
        {
            /* One-time initialization: */

            /* Initialize Spin Lock */
            ATH_PPM_LOCK_INIT(afp);

            /* Synchronize access to PPM routine */
            ATH_PPM_LOCK(afp);

            /* Save backpointers. */
            afp->sc    = sc;
            afp->ah    = ah;
            afp->osdev = sc_osdev;

            afp->forcePpmEnable = pRegParam->forcePpmEnable;

            afp->force_ppm_update_timeout     = ATH_FORCE_PPM_UPDATE_TIMEOUT;
            afp->force_ppm_inactivity_timeout = ATH_FORCE_PPM_INACTIVITY_TIMEOUT;
            afp->force_ppm_recovery_timeout   = ATH_FORCE_PPM_RECOVERY_TIMEOUT;

            /* Initialize timer object/ */
            ath_initialize_timer(sc->sc_osdev, &afp->timer, ATH_FORCE_PPM_PERIOD, ath_force_ppm_timertimeout, afp);

            /* Initialize timer period from registry. Use default value if not set. */
            if (pRegParam->forcePpmUpdateTimeout != 0)
            {
                afp->force_ppm_update_timeout = pRegParam->forcePpmUpdateTimeout;
            }
            
            /*
             * Initialize remaining fields. Those are initialized several 
             * times during normal driver execution.
             */
            ath_force_ppm_init(afp);

            /* Force PPM now active */
            afp->ppmActive = 1;

            /* Release spin lock */
            ATH_PPM_UNLOCK(afp);
        }
    }
    else
    {
        ath_hal_ppmUnForce(ah);
    }
}

void
ath_force_ppm_start (ath_force_ppm_t *afp)
{
    if(afp->forcePpmEnable)
    {
        afp->ppmActive = 1;
    }
    /* Start timer if Force PPM is enabled. */
    if (afp->ppmActive)
    {
        if (ath_timer_is_initialized(&afp->timer))
        {
            if (! ath_timer_is_active(&afp->timer))
            {
                ath_start_timer(&afp->timer);
            }
        }

        DPRINTF(afp->sc, ATH_DEBUG_PPM, "%s\n", __func__);
    }
}

void
ath_force_ppm_halt (ath_force_ppm_t *afp)
{

    /*
     * Stop timer used for force ppm
     */

    if (afp->ppmActive)
    {
        /* Synchronize access to PPM routine */
        ATH_PPM_LOCK(afp);

        DPRINTF(afp->sc, ATH_DEBUG_PPM, "%s: timer_active=%d\n",
            __func__,
            ath_timer_is_active(&afp->timer));

        if (ath_timer_is_initialized(&afp->timer))
        {
            /* cancel timer. Use busy wait so that function can be called from any IRQL. */
            ath_cancel_timer(&afp->timer, CANCEL_NO_SLEEP);
        }
        ath_free_timer(&afp->timer);

        /* Force PPM deactivated */
        afp->ppmActive = 0;

        /* Release spin lock */
        ATH_PPM_UNLOCK(afp);
    }
}

#endif /* ATH_SUPPORT_FORCE_PPM */
