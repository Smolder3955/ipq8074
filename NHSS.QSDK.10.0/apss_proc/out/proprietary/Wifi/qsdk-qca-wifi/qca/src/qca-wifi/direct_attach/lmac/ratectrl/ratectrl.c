/* Copyright (c) 2011, 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */
/*
 * Copyright (c) 2001-2003, Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 *
 * This file contains the data structures and routines for transmit rate
 * control.
 *
 * Releases prior to 2.0.1 used a different algorithm that was based on
 * an active of unicast packets at different rate.  Starting in 2.0.1
 * a new algorithm was introduced.
 *
 * Basic algorithm:
 *   - Keep track of the ackRssi from each successful transmit.
 *   - For each user (SIB entry) maintain a table of rssi thresholds, one
 *     for each rate (eg: 8 or 4). This is the minimum rssi required for
 *     each rate for this user.
 *   - Pick the rate by lookup in the rssi table.
 *   - Update the rssi thresholds based on number of retries and Xretries.
 *     Also, slowly reduce the rssi thresholds over time towards their
 *     theoretical minimums.  In this manner the thresholds will adapt
 *     to the channel characteristics, like delay spread etc, while not
 *     staying abnormally high.
 *
 * More specific details:
 *  - The lkupRssi we use to lookup is computed as follows:
 *  - Use the median of the last three ackRssis.
 *  - Reduce the lkupRssi if the last ackRssi is older than 32msec.
 *    Specifically, reduce it linearly by up to 10dB for ages between
 *    25msec and 185msec, and a fixed 10dB for ages more than 185msec.
 *  - Reduce the lkupRssi by 1 or 2 dB if the packet is long or very long
 *    (this tweaks the rate for longer versus short packets).
 *  - When we get Xretries, reduce lkupRssi by 10dB, unless this was
 *    a probe (see below).
 *  - Maintain a maxRate (really an index, not an actual rate).  We don't
 *    pick rates above the maxRate.  The rssi lookup is free to choose any
 *    rate between [0,maxRate] on each packet.
 *
 *  The maxRate is increased by periodically probing:
 *    - No more than once every 100msec, and only if lkupRssi is high
 *      enough, we try a rate one higher than the current max rate.  If
 *      it succeeds with no retries then we increase the maxRate by one.
 *
 *  The rssi lkup is then free to use this new rate whenever the
 *  rssi warrants.
 *
 *  The maxRate is decreased if a rate looks poor (way too many
 *  retries or a couple of Xretries without many good packets in
 *  between):
 *    - We maintain a packet error rate estimate (per) for each rate.
 *      Each time we get retries or Xretries we increase the PER.
 *      Each time we get no retries we decrease the PER.  If the
 *      PER ever exceeds X (eg: an Xretry will increase the PER by Y)
 *      we set the maxRate to one below this rate.
 */


#include <osdep.h>

#if !NO_HAL
#include "opt_ah.h"
#endif
#include "ah.h"
#include "ratectrl.h"
#include "ratectrl11n.h"
#include "ath_internal.h"

#ifndef REMOVE_PKT_LOG
#include "pktlog_rc.h"
extern struct ath_pktlog_rcfuncs *g_pktlog_rcfuncs;
#endif

#ifdef AH_SUPPORT_AR5212

/* Access functions for validTxRateMask */

static void
rcInitValidTxMask(struct TxRateCtrl_s *pRc)
{
    pRc->validTxRateMask = 0;
}

static void
rcSetValidTxMask(struct TxRateCtrl_s *pRc, A_UINT8 index, int validTxRate)
{
    ASSERT(index < pRc->rateTableSize);

    if (validTxRate) {
        pRc->validTxRateMask |= (1 << index);
    } else {
        pRc->validTxRateMask &= ~(1 << index);
    }
}

/* Iterators for validTxRateMask */

static INLINE int
rcGetFirstValidTxRate(struct TxRateCtrl_s *pRc, A_UINT8 *pIndex, const RATE_TABLE *pRateTable, 
                      A_UINT8 turboPhyOnly, WLAN_PHY turboPhy)
{
    A_UINT32    mask = pRc->validTxRateMask;
    A_UINT8     i;

    for (i = 0; i < pRc->rateTableSize; i++) {
        if (mask & (1 << i)) {
            if ((!turboPhyOnly) || (pRateTable->info[i].phy == turboPhy)) {
                *pIndex = i;
                return TRUE;
            }
        }
    }

    /*
     * No valid rates - this should not happen.
     *
     * Note - Removed an assert here because the dummy sib
     * was causing the assert to trigger.
     */
    *pIndex = 0;

    return FALSE;
}

static INLINE int
rcGetNextValidTxRate(struct TxRateCtrl_s *pRc, A_UINT8 curValidTxRate,
                     A_UINT8 *pNextIndex, 
                     const RATE_TABLE *pRateTable,
                     A_UINT8 excludeTurboPhy, WLAN_PHY turboPhy)
{
    A_UINT32    mask = pRc->validTxRateMask;
    A_UINT8     i;

    for (i = curValidTxRate + 1; i < pRc->rateTableSize; i++) {
        if (mask & (1 << i) &&
            (!excludeTurboPhy || pRateTable->info[i].phy != turboPhy))
        {
            *pNextIndex = i;
            return TRUE;
        }
    }

    /* No more valid rates */
    *pNextIndex = 0;

    return FALSE;
}

#if 0
static int
rcGetPrevValidTxRate(struct TxRateCtrl_s *pRc, A_UINT8 curValidTxRate,
                     A_UINT8 *pPrevIndex )
{
    A_UINT32    mask = pRc->validTxRateMask;
    int         i;

    if (curValidTxRate == 0) {
        *pPrevIndex = 0;
        return FALSE;
    }

    for (i = curValidTxRate - 1; i >= 0; i--) {
        if (mask & (1 << i)) {
            *pPrevIndex = (A_UINT8)i;
            return TRUE;
        }
    }

    /* No more valid rates */
    *pPrevIndex = 0;

    return FALSE;
}
#endif

static int
rcGetNextLowerValidTxRate(const RATE_TABLE *pTable, struct TxRateCtrl_s *pRc, 
                          A_UINT8 curValidTxRate, A_UINT8 *pNextIndex )
{
    A_UINT32    mask = pRc->validTxRateMask;
    A_UINT8     i;

    *pNextIndex = 0;
    for (i = 1; i < pRc->rateTableSize; i++) {
        if ((mask & (1 << i)) && 
            (pTable->info[i].rateKbps < pTable->info[curValidTxRate].rateKbps))
        {
            if (*pNextIndex) {
                if (pTable->info[i].rateKbps > pTable->info[*pNextIndex].rateKbps) {
                    *pNextIndex = i;
                }
            } else {
                *pNextIndex = i;
            }
        }
    }

    if (*pNextIndex) {
        return TRUE;
    } else {
        *pNextIndex = curValidTxRate;
        return FALSE;
    }
}

/*
 *  Update the SIB's rate control information
 *
 *  This should be called when the supported rates change
 *  (e.g. SME operation, wireless mode change)
 *
 *  It will determine which rates are valid for use.
 */
void
rcSibUpdate(struct atheros_softc *asc, struct atheros_node *pSib,
            int keepState, struct ieee80211_rateset *pRateSet,
            enum ieee80211_phymode curmode)
{
#define N(a)    (sizeof(a)/sizeof(a[0]))
    const RATE_TABLE    *pRateTable;
    struct TxRateCtrl_s *pRc        = &pSib->txRateCtrl;
    A_UINT8             i, j, hi = 0, count;
    A_INT8              k;
    int                 rateCount, numInfo;
    HAL_BOOL            bFoundDot11Rate_11, bFoundDot11Rate_22;

    pRateTable = asc->hwRateTable[curmode];

    /* Initial rate table size. Will change depending on the working rate set */
    pRc->rateTableSize = MAX_TX_RATE_TBL;	
    numInfo = N(pRateTable->info);

    /* Initialize thresholds according to the global rate table */
    for (i = 0 ; (i < pRc->rateTableSize) && (!keepState) && (i < pRateTable->rateCount); i++) {
		if (i < numInfo) {
			pRc->state[i].rssiThres = pRateTable->info[i].rssiAckValidMin;
		}
			pRc->state[i].per       = 0;
#if ATH_SUPPORT_VOWEXT /* for RCA */
        pRc->state[i].maxAggrSize = MAX_AGGR_LIMIT;
#endif
    }

    /* Determine the valid rates */
    rcInitValidTxMask(pRc);
    rateCount = pRateTable->rateCount;

    count = 0;
    if (!pRateSet->rs_nrates) {
        /* No working rate, use valid rates */
        for (i = 0; i < rateCount; i++) {
            /*
             * If the rate is:
             * 1. not valid, or 
             * 2. this is an uapsd node but the rate is not an uapsd rate
             * skip it.
             */ 
            if (pRateTable->info[i].valid != TRUE) {
                continue;
            }
#ifdef ATH_SUPPORT_UAPSD_RATE_CONTROL
            if ((pSib->uapsd) && (pRateTable->info[i].validUAPSD != TRUE)) {
                continue;
            }
#endif /* ATH_SUPPORT_UAPSD_RATE_CONTROL */

            pRc->validRateIndex[count] = i;
            /* 
             * Copy the static rate series for the correspondig rate
             * from static rate table to TxRateCtrl_s.
             */
            OS_MEMCPY(pRc->validRateSeries[count],
                &(pRateTable->info[i].normalSched), MAX_SCHED_TBL);
            count ++;
            rcSetValidTxMask(pRc, i, TRUE);
            hi = A_MAX(hi, i);
            pSib->rixMap[i] = 0;
        }

        pRc->maxValidRate = count;
        pRc->maxValidTurboRate = pRateTable->numTurboRates;
    } else {
        A_UINT8  turboCount;
        A_UINT64 mask;

        bFoundDot11Rate_11 = FALSE;
        bFoundDot11Rate_22 = FALSE;
        for(k = 0; k < pRateSet->rs_nrates; k++) {
            // Look for 5.5 mbps
            if((pRateSet->rs_rates[k] & 0x7F) == 11) {
                bFoundDot11Rate_11 = TRUE;
            }
            // Look for 11 mbps
            if((pRateSet->rs_rates[k] & 0x7F) == 22) {
                bFoundDot11Rate_22 = TRUE;
            }
        }

        /*
         * Use intersection of working rates and valid rates.
         * if working rates has 6 OFDM and does not have 5.5 and 11 CCKM, use 6.
         * if working rates has 9 OFDM and does not has 11 CCKM, use 9.
         */
        turboCount = 0;
        for (i = 0; i < pRateSet->rs_nrates; i++) {
            for (j = 0; j < rateCount; j++) {
                if ((pRateSet->rs_rates[i] & 0x7F) ==
                    (pRateTable->info[j].dot11Rate & 0x7F)) {
#ifdef ATH_SUPPORT_UAPSD_RATE_CONTROL
                    /*
                     * If this is an uapsd node but the rate is not an uapsd rate
                     * skip it.
                     */ 
                    if ((pSib->uapsd) && (pRateTable->info[j].validUAPSD != TRUE)) {
                        continue;
                    }
#endif /* ATH_SUPPORT_UAPSD_RATE_CONTROL */

                    if(pRateTable->info[j].valid == TRUE) {
                        rcSetValidTxMask(pRc, j, TRUE);
                        hi = A_MAX(hi, j);
                        pSib->rixMap[j] = i;
                    } else {
                        //To keep rate monotonicity
                        if(pRateTable->info[j].phy == WLAN_PHY_OFDM &&
                            ((((pRateSet->rs_rates[i] & 0x7F) == 12) && 
                               (!bFoundDot11Rate_11) && (!bFoundDot11Rate_22)) ||
                            (((pRateSet->rs_rates[i] & 0x7F) == 18) && !bFoundDot11Rate_22)))
                        {
                            rcSetValidTxMask(pRc, j, TRUE);
                            hi = A_MAX(hi, j);
                            pSib->rixMap[j] = i;
                        }
                    }
                }
            }
        }

        /* Get actually valid rate index, previous we get it from rate table,
         * now get rate table which include all working rate, so we need make
         * sure our valid rate table align with working rate */
        mask = pRc->validTxRateMask;
        for (i = 0; i < pRc->rateTableSize; i ++) {
            if (mask & ((A_UINT64)1 << i)) {
                pRc->validRateIndex[count] = i;
                /* 
                 * Copy the static rate series for the correspondig rate
                 * from static rate table to TxRateCtrl_s.
                 */
                OS_MEMCPY(pRc->validRateSeries[count],
                   &(pRateTable->info[i].normalSched), MAX_SCHED_TBL);
                count ++;
                if (pRateTable->info[i].phy == WLAN_PHY_TURBO) {
                    turboCount ++;
                }
            }
        }
        pRc->maxValidRate = count;
        pRc->maxValidTurboRate = turboCount;
    }
    /*
     * Modify the static rate series in TxRateCtrl_s with respect to intersection
     * rate table.
     */
    for(i = 0 ; i < pRc->maxValidRate; i ++) {
        k = i;
        for (j = 5; j < MAX_SCHED_TBL; j ++) {
            if(j % 8 == 0) {
                k = i;
                j = j + 4;
                continue;
            }
            for (; k >= 0; k --) {
                if(pRc->validRateSeries[i][j] == pRateTable->info[k].rateCode) {
                    break;
                }
            }
            if (k == -1) {
                pRc->validRateSeries[i][j] = pRc->validRateSeries[i][j-1];
            }
        }
    }
    pRc->rateTableSize = hi + 1;
    pRc->rateMax = A_MIN(hi, pRateTable->initialRateMax);

    ASSERT(pRc->rateTableSize <= MAX_TX_RATE_TBL);
#undef N
}

/*
 *  This routine is called to initialize the rate control parameters
 *  in the SIB. It is called initially during system initialization
 *  or when a station is associated with the AP.
 */
void
rcSibInit(struct atheros_node *pSib)
{
    struct TxRateCtrl_s *pRc        = &pSib->txRateCtrl;

#if 0
    /* NB: caller assumed to zero state */
    A_MEM_ZERO((char *)pSib, sizeof(*pSib));
#endif
    pRc->rssiDownTime = A_MS_TICKGET();
}


/*
 * Return the median of three numbers
 */
static INLINE A_RSSI
median(A_RSSI a, A_RSSI b, A_RSSI c)
{
    if (a >= b) {
        if (b >= c) {
            return b;
        } else if (a > c) {
            return c;
        } else {
            return a;
        }
    } else {
        if (a >= c) {
            return a;
        } else if (b >= c) {
            return c;
        } else {
            return b;
        }
    }
}

/*
 * Determines and returns the new Tx rate index.
 */
A_UINT16
rcRateFind(struct ath_softc *sc, struct atheros_node *pSib,
           A_UINT32 frameLen,
           const RATE_TABLE *pRateTable,
           HAL_CHANNEL *curchan, int isretry)
{
    struct TxRateCtrl_s  *pRc;
    A_UINT32             dt;
    A_UINT32             bestThruput,thisThruput = 0;
    A_UINT32             nowMsec;
    A_UINT8              rate, nextRate, bestRate;
    A_RSSI               rssiLast, rssiReduce;
#if ATH_SUPERG_DYNTURBO
    A_UINT8              currentPrimeState = IS_CHAN_TURBO(curchan);
    /* 0 = regular; 1 = turbo */
    A_UINT8              primeInUse        = sc->sc_dturbo;
#else
    A_UINT8              primeInUse        = 0;
    A_UINT8              currentPrimeState = 0;
#endif
    int               isChanTurbo      = FALSE;
    A_UINT8              maxIndex, minIndex;
    A_INT8               index;
    int               isProbing = FALSE;

    /* have the real rate control logic kick in */
    pRc = &pSib->txRateCtrl;

#if ATH_SUPERG_DYNTURBO
    /*
     * Reset primeInUse state, if we are currently using XR 
     * rate tables or if we have any clients associated in XR mode
     */

    /* make sure that rateMax is correct when using TURBO_PRIME tables */
    if (currentPrimeState)
    {
        pRc->rateMax = pRateTable->rateCount - 1;
    } else
    {
        pRc->rateMax = pRateTable->rateCount - 1 - pRateTable->numTurboRates;
    }
#endif /* ATH_SUPERG_DYNTURBO */

    rssiLast   = median(pRc->rssiLast, pRc->rssiLastPrev, pRc->rssiLastPrev2);
    rssiReduce = 0;

    /*
     * Age (reduce) last ack rssi based on how old it is.
     * The bizarre numbers are so the delta is 160msec,
     * meaning we divide by 16.
     *   0msec   <= dt <= 25msec:   don't derate
     *   25msec  <= dt <= 185msec:  derate linearly from 0 to 10dB
     *   185msec <= dt:             derate by 10dB
     */
    nowMsec = A_MS_TICKGET();
    dt = nowMsec - pRc->rssiTime;

    if (dt >= 185) {
        rssiReduce = 10;
    } else if (dt >= 25) {
        rssiReduce = (A_UINT8)((dt - 25) >> 4);
    }

    /* Reduce rssi for long packets */
    if (frameLen > 800) {
        rssiReduce += 1;    /* need 1 more dB for packets > 800 bytes */
    }

    /* Now reduce rssiLast by rssiReduce */
    if (rssiLast < rssiReduce) {
        rssiLast = 0;
    } else {
        rssiLast -= rssiReduce;
    }

    /*
     * Now look up the rate in the rssi table and return it.
     * If no rates match then we return 0 (lowest rate)
     */

    bestThruput = 0;
    maxIndex    = pRc->maxValidRate - 1;
    minIndex    = 0;
    isChanTurbo = IS_CHAN_TURBO(curchan);

    /* 
     * If we use TURBO_PRIME table, but we aren't in turbo mode,
     * we need to reduce maxIndex to actual size
     */
    if (pRc->maxValidTurboRate) {
        if (!isChanTurbo) {
            maxIndex -= pRc->maxValidTurboRate;
            /* 
             * This shouldn't happen unless we use wrong rate table.
             * if rate table has only turbo rates and currently we are not in turbo mode,
             * maxIndex wraps to 0xFF, so send the packet at minimum rate possible. This
             * was wrongly compared with 0 previously.
             */
            if (maxIndex == 0xFF) {
                pSib->lastRateKbps = pRateTable->info[0].rateKbps;
                return 0;
            }
        } else {
            if (pRc->maxValidRate > pRc->maxValidTurboRate) {
                minIndex = pRc->maxValidRate - pRc->maxValidTurboRate;
            }
        }
    }

    bestRate =  pRc->validRateIndex[minIndex];
    /*
     * Try the higher rate first. It will reduce memory moving time
     * if we have very good channel characteristics.
     */
    for (index = maxIndex; index >= minIndex ; index--) {
        rate = pRc->validRateIndex[index];

        if (rssiLast < pRc->state[rate].rssiThres) {
            continue;
        }

        thisThruput = pRateTable->info[rate].userRateKbps * 
            (100 - pRc->state[rate].per);

        if (bestThruput <= thisThruput) {
            bestThruput = thisThruput;
            bestRate    = rate;
        }
    }

    rate = bestRate;

    /* Following are recorded as a part of pktLog feature */
    pRc->misc[4]  = primeInUse;
    pRc->misc[5]  = currentPrimeState;
    pRc->misc[9]  = pRc->rateTableSize;
    pRc->misc[8]  = rcGetNextValidTxRate(pRc, rate, &pRc->misc[7], pRateTable,
                                         !isChanTurbo, WLAN_PHY_TURBO);
    pRc->misc[10] = rate;

    pRc->rssiLastLkup = rssiLast;

    /*
     * When we have exhausted half of our max retries, we should force to use
     * the lowest rate in the current table
     */
    if (isretry) {
        rate = pRc->validRateIndex[minIndex];
    }
 
    /*
     * Must check the actual rate (rateKbps) to account for non-monoticity of
     * 11g's rate table
     */

    if (rate > pRc->rateMax) {
        rate = pRc->rateMax;

        /*
         * Always probe the next rate in the rate Table (ignoring monotonicity).
         * Reason:  If OFDM is broken, when rateMax = 5.5, it will probe
         *          11 Mbps first.
         */
        if (rcGetNextValidTxRate(pRc, rate, &nextRate, pRateTable,
                                 !isChanTurbo, WLAN_PHY_TURBO) &&
            (nowMsec - pRc->probeTime > pRateTable->probeInterval) &&
            (pRc->hwMaxRetryPktCnt >= 4))
        {
            rate                  = nextRate;
            pRc->probeRate        = (A_UINT8)rate;
            pRc->probeTime        = nowMsec;
            pRc->hwMaxRetryPktCnt = 0;

            isProbing = TRUE;
        }
    }

    /*
     * Make sure rate is not higher than the allowed maximum.
     * We should also enforce the min, but I suspect the min is
     * normally 1 rather than 0 because of the rate 9 vs 6 issue
     * in the old code.
     */
    if (rate > (pSib->txRateCtrl.rateTableSize - 1)) {
        rate = pSib->txRateCtrl.rateTableSize - 1;
    }

#if ATH_SUPERG_DYNTURBO
    /* turboPrime - make mode switch recommendation */
    if (primeInUse && (sc->sc_opmode == HAL_M_HOSTAP)) {
        if (currentPrimeState == 0) { /* regular mode */
            pRc->recommendedPrimeState = 0;   /* default:  don't switch */
            if (rate > pRateTable->regularToTurboThresh) {
                pRc->switchCount++;
                if (pRc->switchCount >= pRateTable->pktCountThresh) {
                    A_UINT8 targetRate = pRateTable->regularToTurboThresh;

                    pRc->switchCount = pRateTable->pktCountThresh;
                    /* make sure threshold rate is valid; if not, find the closest valid rate */
                    if ((pRc->validTxRateMask & (1 << targetRate))
                        || (rcGetNextValidTxRate(pRc, targetRate, &targetRate, pRateTable, 
                                                 FALSE, WLAN_PHY_TURBO)))
                    {
                        if (pRc->state[targetRate].per <= 20) {
                            pRc->recommendedPrimeState = 1;   /* recommend switch to Turbo mode */
                        }
                    }
                }
            } else {
                pRc->switchCount = 0;
            }
        } else {  /* turbo mode */
            pRc->recommendedPrimeState = 1;  /* default: don't switch */
            if (rate < pRateTable->turboToRegularThresh) {
                pRc->switchCount++;
                if (pRc->switchCount >= pRateTable->pktCountThresh) {
                    pRc->switchCount = pRateTable->pktCountThresh;
                    /* Assumption: regular is always better than Turbo in terms of sensitivity */
                    pRc->recommendedPrimeState = 0;   /* recommend switch to regular mode */
                }
            } else {
                pRc->switchCount = 0;
            }
        }
        /* Use latest recommendation.  TODO: vote */
        sc->sc_rate_recn_state = pRc->recommendedPrimeState;
    }
#endif /* ATH_SUPERG_DYNTURBO */


    if (sc->sc_curchan.priv_flags & CHANNEL_4MS_LIMIT) {
        A_UINT32 limitRate;
        for (limitRate = minIndex;
            (pRateTable->info[limitRate].max4msFrameLen < frameLen && 
            limitRate < maxIndex); limitRate++);
  
        if (pRateTable->info[rate].max4msFrameLen < frameLen) {
            /* Best rate has come down below limitRateKbps */
            rate = limitRate;
        }
    }

#ifndef REMOVE_PKT_LOG
    {
        struct log_rcfind log_data;
        log_data.rc = pRc;
        log_data.rate = rate;
        log_data.rssiReduce = rssiReduce;
        log_data.isProbing = isProbing;
        log_data.primeInUse = primeInUse;
        log_data.currentPrimeState = currentPrimeState;

        ath_log_rcfind(sc, &log_data, 0);
    }
#endif

    /* record selected rate, which is used to decide if we want to do fast frame */
    if (!isProbing) {
        pSib->lastRateKbps = pRateTable->info[rate].rateKbps;
    }
    return rate;
}

/*
 * This routine is called by the Tx interrupt service routine to give
 * the status of previous frames.
 */
void
rcUpdate(struct atheros_node *pSib,
         int Xretries, int txRate, int retries, A_RSSI rssiAck,
         A_UINT8 curTxAnt,
         const RATE_TABLE *pRateTable,
         enum ieee80211_opmode opmode,
         int diversity,
         SetDefAntenna_callback setDefAntenna, void *context, int short_retry_fail)
{
#define N(a)    (sizeof(a)/sizeof(a[0]))
    struct TxRateCtrl_s *pRc;
    A_UINT32            nowMsec     = A_MS_TICKGET();
    int              stateChange = FALSE;
    A_UINT8             lastPer;
    int                 rate, count;
#if ATH_SUPERG_DYNTURBO
    A_UINT8             currentPrimeState, primeInUse;
#endif
    int                 numInfo;
    struct ath_softc *sc = (struct ath_softc *)context;

    static const A_UINT32 nRetry2PerLookup[10] = {
        100 * 0 / 1,
        100 * 1 / 8,
        100 * 1 / 2,
        100 * 3 / 4,
        100 * 4 / 5,
        100 * 5 / 6,
        100 * 6 / 7,
        100 * 7 / 8,
        100 * 8 / 9,
        100 * 9 / 10
    };

    pRc                             = &pSib->txRateCtrl;

    /* if RTS failed on all rates, do not drop the rate here,
     * stay at the same rate and do not update the PER for this rate
     * next try should happen at the same highest rate
     * Quickly dropping the rate when RTS fails, may not be correct
     * as it takes long time to recover. The RTS failure may be due to multiple
     * reasons, station may be experiencing too much noise, or it is in some
     * kind of reset procedure or AP failed to detect power save state quicker.
     */
    if (short_retry_fail) {
        goto out;
    }
#if ATH_SUPERG_DYNTURBO
    {
    
    /* TURBO_PRIME - choose the correct rate index when in turbo mode */
    if (sc->sc_curchan.channel_flags & CHANNEL_TURBO)
    {
        if (txRate < pRateTable->initialRateMax + 1) {
            rate = txRate + pRateTable->numTurboRates;

            if (pRateTable->info[txRate].rateCode != pRateTable->info[rate].rateCode) {
                        int i;

                rate++;

                for (i = rate; i < pRateTable->rateCount; i ++) {
                    if (pRateTable->info[txRate].rateCode ==
                        pRateTable->info[i].rateCode)
                    {
                        txRate = i;
                        break;
                    }
                }
            } else {
                txRate = rate;
            }
        }
    }

    primeInUse        = sc->sc_dturbo;
    currentPrimeState = IS_CHAN_TURBO(&sc->sc_curchan);
    }
#endif /* ATH_SUPERG_DYNTURBO */

    lastPer = pRc->state[txRate].per;

#if QCA_AIRTIME_FAIRNESS
    if (sc->sc_atf_enable == ATF_TPUT_BASED) {
        if (pRc->state[txRate].used < 5000)
            pRc->state[txRate].used++;

        pRc->state[txRate].aggr |= 0x1u;

        if (pRc->state[txRate].aggr_count[0] < 5000)
            pRc->state[txRate].aggr_count[0]++;
    }
#endif

    ASSERT(retries >= 0 && retries <= ATH_TXMAXTRY);
    ASSERT(txRate >= 0 && txRate < pRc->rateTableSize);

    if (Xretries) {
        /* Update the PER. */
        if (Xretries == 1) {
            pRc->state[txRate].per += 35;
            if (pRc->state[txRate].per > 100) {
                pRc->state[txRate].per = 100;
            }
        } else {
            count = sizeof(nRetry2PerLookup) / sizeof(nRetry2PerLookup[0]);
            if (retries >= count) {
                retries = count - 1;
            }
            /* new_PER = 3/4*old_PER + 1/4*(currentPER) */
            pRc->state[txRate].per = (A_UINT8)(pRc->state[txRate].per -
                                               (pRc->state[txRate].per >> 2) +
                                               (nRetry2PerLookup[retries] >> 2));
        }
		numInfo = N(pRateTable->info);
        for (rate = txRate + 1; rate < pRc->rateTableSize; rate++) {
			/* Modify for static analysis, sizeof pRateTable->info[txRate] is max 32? */
            if ((rate >= numInfo) || 
                (pRateTable->info[rate].phy != pRateTable->info[txRate].phy)) {
                break;
            }

            if (pRc->state[rate].per < pRc->state[txRate].per) {
                pRc->state[rate].per = pRc->state[txRate].per;
            }
        }

        /* Update the RSSI threshold */
        /*
         * Oops, it didn't work at all.  Set the required rssi
         * to the rssiAck we used to lookup the rate, plus 4dB.
         * The immediate effect is we won't try this rate again
         * until we get an rssi at least 4dB higher.
         */
        if (txRate > 0) {
            A_RSSI rssi = A_MAX(pRc->rssiLastLkup, pRc->rssiLast - 2);

            if (pRc->state[txRate].rssiThres + 2 < rssi) {
                pRc->state[txRate].rssiThres += 4;
            } else if (pRc->state[txRate].rssiThres < rssi + 2) {
                pRc->state[txRate].rssiThres = rssi + 2;
            } else {
                pRc->state[txRate].rssiThres += 2;
            }

            pRc->hwMaxRetryRate = (A_UINT8)txRate;
            stateChange         = TRUE;
        }

        /*
         * Also, if we are not doing a probe, we force a significant
         * backoff by reducing rssiLast.  This doesn't have a big
         * impact since we will bump the rate back up as soon as we
         * get any good ACK.  RateMax will also be set to the current
         * txRate if the failed frame is not a probe.
         */
        if (pRc->probeRate == 0 || pRc->probeRate != txRate) {
            pRc->rssiLast      = 10 * pRc->state[txRate].rssiThres / 16;
            pRc->rssiLastPrev  = pRc->rssiLast;
            pRc->rssiLastPrev2 = pRc->rssiLast;
        }

        pRc->probeRate = 0;
    } else {
        /* Update the PER. */
        /* Make sure it doesn't index out of array's bounds. */
        count = sizeof(nRetry2PerLookup) / sizeof(nRetry2PerLookup[0]);
        if (retries >= count) {
            retries = count - 1;
        }

        /* new_PER = 7/8*old_PER + 1/8*(currentPER) */
        pRc->state[txRate].per = (A_UINT8)(pRc->state[txRate].per -
                                           (pRc->state[txRate].per >> 3) +
                                           (nRetry2PerLookup[retries] >> 3));

        /* Update the RSSI threshold */
        if (pSib->antTx != curTxAnt) {
            /*
             * Hw does AABBAA on transmit attempts, and has flipped on this transmit.
             */
            pSib->antTx = curTxAnt;     /* 0 or 1 */
// XXX need API to push stats
//          sc->sc_stats.ast_ant_txswitch++;
            pRc->antFlipCnt = 1;

            if (opmode != IEEE80211_M_HOSTAP && !diversity) {
                /*
                 * Update rx ant (default) to this transmit antenna if:
                 *   1. The very first try on the other antenna succeeded and
                 *      with a very good ack rssi.
                 *   2. Or if we find ourselves succeeding for RX_FLIP_THRESHOLD
                 *      consecutive transmits on the other antenna;
                 * NOTE that the default antenna is preserved across a chip
                 *      reset by the hal software
                 */
                if (retries == 2 && rssiAck >= pRc->rssiLast + 2) {
                    curTxAnt = curTxAnt ? 2 : 1;
                    setDefAntenna(context, curTxAnt);
                }
            }
        } else if (opmode != IEEE80211_M_HOSTAP) {
            if (!diversity && pRc->antFlipCnt < RX_FLIP_THRESHOLD) {
                pRc->antFlipCnt++;
                if (pRc->antFlipCnt == RX_FLIP_THRESHOLD) {
                    curTxAnt = curTxAnt ? 2 : 1;
                    setDefAntenna(context, curTxAnt);
                }
            }
        }

        pRc->rssiLastPrev2 = pRc->rssiLastPrev;
        pRc->rssiLastPrev  = pRc->rssiLast;
        pRc->rssiLast      = rssiAck;
        pRc->rssiTime      = nowMsec;

        /*
         * If we got at most one retry then increase the max rate if
         * this was a probe.  Otherwise, ignore the probe.
         */

        if (pRc->probeRate && pRc->probeRate == txRate) {
            if (retries > 1) {
                pRc->probeRate = 0;
            } else {
                pRc->rateMax = pRc->probeRate;

                if (pRc->state[pRc->probeRate].per > 45) {
                    pRc->state[pRc->probeRate].per = 20;
                }

                pRc->probeRate = 0;

                /*
                 * Since this probe succeeded, we allow the next probe
                 * twice as soon.  This allows the maxRate to move up
                 * faster if the probes are succesful.
                 */
                pRc->probeTime = nowMsec - pRateTable->probeInterval / 2;
            }
        }

        if (retries > 0) {
            /*
             * Don't update anything.  We don't know if this was because
             * of collisions or poor signal.
             *
             * Later: if rssiAck is close to pRc->state[txRate].rssiThres
             * and we see lots of retries, then we could increase
             * pRc->state[txRate].rssiThres.
             */
        } else {
            /*
             * It worked with no retries.  First ignore bogus (small)
             * rssiAck values.
             */
            if (pRc->hwMaxRetryPktCnt < 255) {
                pRc->hwMaxRetryPktCnt++;
            }

            if (rssiAck >= pRateTable->info[txRate].rssiAckValidMin) {
                /* Average the rssi */
                if (txRate != pRc->rssiSumRate) {
                    pRc->rssiSumRate = txRate;
                    pRc->rssiSum     = pRc->rssiSumCnt = 0;
                }

                pRc->rssiSum += rssiAck;
                pRc->rssiSumCnt++;

                if (pRc->rssiSumCnt >= 4) {
                    A_RSSI32 rssiAckAvg = (pRc->rssiSum + 2) / 4;

                    pRc->rssiSum = pRc->rssiSumCnt = 0;

                    /* Now reduce the current rssi threshold. */
                    if ((rssiAckAvg < pRc->state[txRate].rssiThres + 2) &&
                        (pRc->state[txRate].rssiThres > pRateTable->info[txRate].rssiAckValidMin))
                    {
                        pRc->state[txRate].rssiThres--;
                    }

                    stateChange = TRUE;
                }
            }
        }
    }

    /*
     * If this rate looks bad (high PER) then stop using it for
     * a while (except if we are probing).
     */
    if (pRc->state[txRate].per > 60 && txRate > 0 &&
        pRateTable->info[txRate].rateKbps <= pRateTable->info[pRc->rateMax].rateKbps)
    {
        rcGetNextLowerValidTxRate(pRateTable, pRc, (A_UINT8) txRate, &pRc->rateMax);

        /* Don't probe for a little while. */
        pRc->probeTime = nowMsec;
    }

    if (stateChange) {
        /*
         * Make sure the rates above this have higher rssi thresholds.
         * (Note:  Monotonicity is kept within the OFDM rates and within the CCK rates.
         *         However, no adjustment is made to keep the rssi thresholds monotonically
         *         increasing between the CCK and OFDM rates.)
         */
        for (rate = txRate; rate < pRc->rateTableSize - 1; rate++) {
            if (pRateTable->info[rate+1].phy != pRateTable->info[txRate].phy) {
                break;
            }

            if (pRc->state[rate].rssiThres + pRateTable->info[rate].rssiAckDeltaMin >
                pRc->state[rate+1].rssiThres)
            {
                pRc->state[rate+1].rssiThres =
                    pRc->state[rate].rssiThres + pRateTable->info[rate].rssiAckDeltaMin;
            }
        }

        /* Make sure the rates below this have lower rssi thresholds. */
        for (rate = txRate - 1; rate >= 0; rate--) {
            if (pRateTable->info[rate].phy != pRateTable->info[txRate].phy) {
                break;
            }

            if (pRc->state[rate].rssiThres + pRateTable->info[rate].rssiAckDeltaMin >
                pRc->state[rate+1].rssiThres)
            {
                if (pRc->state[rate+1].rssiThres < pRateTable->info[rate].rssiAckDeltaMin) {
                    pRc->state[rate].rssiThres = 0;
                } else {
                    pRc->state[rate].rssiThres =
                        pRc->state[rate+1].rssiThres - pRateTable->info[rate].rssiAckDeltaMin;
                }

                if (pRc->state[rate].rssiThres < pRateTable->info[rate].rssiAckValidMin) {
                    pRc->state[rate].rssiThres = pRateTable->info[rate].rssiAckValidMin;
                }
            }
        }
    }

    /* Make sure the rates below this have lower PER */
    /* Monotonicity is kept only for rates below the current rate. */
    if (pRc->state[txRate].per < lastPer) {
        for (rate = txRate - 1; rate >= 0; rate--) {
            if (pRateTable->info[rate].phy != pRateTable->info[txRate].phy) {
                break;
            }

            if (pRc->state[rate].per > pRc->state[rate+1].per) {
                pRc->state[rate].per = pRc->state[rate+1].per;
            }
        }
    }

    /* Every so often, we reduce the thresholds and PER (different for CCK and OFDM). */
    if (nowMsec - pRc->rssiDownTime >= pRateTable->rssiReduceInterval) {
        for (rate = 0; rate < pRc->rateTableSize; rate++) {
            if (pRc->state[rate].rssiThres > pRateTable->info[rate].rssiAckValidMin) {
                pRc->state[rate].rssiThres -= 1;
            }
//            pRc->state[rate].per = 7*pRc->state[rate].per/8;
        }

        pRc->rssiDownTime = nowMsec;
    }
    /* Every so often, we reduce the thresholds and PER (different for CCK and OFDM). */
    if (nowMsec - pRc->perDownTime >= (pRateTable->rssiReduceInterval/2)) {
        for (rate = 0; rate < pRc->rateTableSize; rate++) {
            if (pRc->state[rate].per != 0) {
                pRc->state[rate].per = 7*pRc->state[rate].per/8;
            }
        }

        pRc->perDownTime = nowMsec;
    }

out:
#ifndef REMOVE_PKT_LOG
    {
        struct log_rcupdate log_data;
		struct ath_softc *sc = context;

        log_data.rc = pRc;
        log_data.txRate = txRate;
        log_data.Xretries = Xretries;
#ifdef ATH_SUPERG_DYNTURBO
        log_data.currentBoostState = IS_CHAN_TURBO(&sc->sc_curchan)? 1 : 0;
        log_data.useTurboPrime = sc->sc_dturbo;        
#else
        log_data.currentBoostState = 0;
        log_data.useTurboPrime = 0;        
#endif         
        log_data.retries = retries;
        log_data.rssiAck = rssiAck;

        ath_log_rcupdate(sc, &log_data, 0);
    }
#else
    return;
#endif
#undef N
}

#if ATH_CCX
u_int8_t
rcRateValueToPer(struct ath_softc *sc, struct ath_node *an, int txRateKbps)
{
    const RATE_TABLE    *pRateTable;
    u_int8_t             rate  = 0;
    u_int8_t             index = 0;
    struct atheros_node *oan = an->an_rc_node;
    struct TxRateCtrl_s *pRc        = &oan->txRateCtrl;
    HAL_BOOL turboFlag = IS_CHAN_TURBO(&(sc->sc_curchan));

    pRateTable = sc->sc_rc->hwRateTable[sc->sc_curmode];
    if(pRateTable){
        while (rate < pRateTable->rateCount) {
            if (pRateTable->info[rate].valid &&
                txRateKbps == pRateTable->info[rate].rateKbps && 
                ((turboFlag && pRateTable->info[rate].phy == WLAN_PHY_TURBO) || 
                (!turboFlag && pRateTable->info[rate].phy != WLAN_PHY_TURBO ))) {

                index = rate;
                break;
            }
            rate++;
        }
	} else {
        return (100);
    }
    if (pRc == NULL || index >= MAX_TX_RATE_TBL) {
        return (100);
    } else {
        return (pRc->state[index].per);
    }
}
#endif
#endif /* AH_SUPPORT_AR5212 */
