/*
 * Copyright (c) 2011,2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * Copyright (c) 2001-2003, Atheros Communications Inc.
 * All Rights Reserved.
 */

/*
 * DESCRIPTION
 *
 * This file contains the data structures and routines for transmit rate
 * control operating on the linearized rates of different phys
 */

#include <osdep.h>

#include "ah.h"
#include "ratectrl.h"
#include "ratectrl11n.h"
#include "ath_internal.h"

#ifndef REMOVE_PKT_LOG
#include "pktlog_rc.h"
extern struct ath_pktlog_rcfuncs *g_pktlog_rcfuncs;
#endif

#ifdef QCA_SUPPORT_CP_STATS
#include <wlan_cp_stats_ic_utils_api.h>
#endif

#define MULTI_RATE_RETRY_ENABLE 1

#ifdef ATH_SUPPORT_TxBF
/* sound rate swap table for mcs0~mcsf at A/G band and 40/20 bandwith*/
const u_int8_t sounding_swap_table_A_40[SWAP_TABLE_SIZE]= {0x80, 0x80, 0x80, 0x80, 0x81, 0x83, 0x84, 0x84,
                                                            0x80, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8c, 0x8d};
const u_int8_t sounding_swap_table_A_20[SWAP_TABLE_SIZE]= {0x80, 0x80, 0x80, 0x80, 0x81, 0x83, 0x84, 0x84,
                                                            0x80, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8c, 0x8d};
const u_int8_t sounding_swap_table_G_40[SWAP_TABLE_SIZE]= {0x80, 0x80, 0x80, 0x80, 0x81, 0x83, 0x84, 0x84,
                                                            0x80, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8c, 0x8c};
const u_int8_t sounding_swap_table_G_20[SWAP_TABLE_SIZE]= {0x80, 0x80, 0x80, 0x80, 0x81, 0x83, 0x84, 0x84,
                                                            0x80, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8c, 0x8c};
#endif
/*
 * The purpose of this function is to sort valid rate into sequence 
 * that corresponds to rate table. But here was a corner case when 
 * two rates have same kbps, like MCS 22h has same kbps as MCS 23. 
 * The following complicated sorting condition can avoid MCS 23 
 * being put ahead of MCS 22h. Refer to EV#71550.
 */
static void
rcSortValidRates(const RATE_TABLE_11N *pRateTable, TX_RATE_CTRL *pRc)
{
    A_UINT8 i,j;

    if (!pRc->maxValidRate) {
        return;
    }

    for (i = pRc->maxValidRate - 1; i > 0; i--) {
        for (j = 0; j <= i - 1; j++) {
             if ((pRateTable->info[pRc->validRateIndex[j]].rateKbps >
                 pRateTable->info[pRc->validRateIndex[j+1]].rateKbps) ||
                 ((pRateTable->info[pRc->validRateIndex[j]].rateKbps ==
                 pRateTable->info[pRc->validRateIndex[j+1]].rateKbps) && 
                 (pRc->validRateIndex[j]>pRc->validRateIndex[j+1])))
             {
                 A_UINT8 tmp=0;
                 tmp = pRc->validRateIndex[j];
                 pRc->validRateIndex[j] = pRc->validRateIndex[j+1];
                 pRc->validRateIndex[j+1] = tmp;
             }
        }
    }

    return;
}

/* Iterators for validTxRateMask */
static INLINE int
rcGetNextValidTxRate(const RATE_TABLE_11N *pRateTable, TX_RATE_CTRL *pRc,
                     A_UINT8 curValidTxRate, A_UINT8 *pNextIndex)
{
    A_UINT8     i;

    for (i = 0; i < pRc->maxValidRate-1; i++) {
        if (pRc->validRateIndex[i] == curValidTxRate) {
            *pNextIndex = pRc->validRateIndex[i+1];
            return TRUE;
        }
    }
    /* No more valid rates */
    *pNextIndex = 0;
    return FALSE;
}

static INLINE int
rcGetNextLowerValidTxRate(const RATE_TABLE_11N *pRateTable, TX_RATE_CTRL *pRc,
                          A_UINT8 curValidTxRate, A_UINT8 *pNextIndex,int txbfsounding)
{
    A_INT8     i;

    for (i = 1; i < pRc->maxValidRate ; i++) {
#ifdef ATH_SUPPORT_TxBF
        if (txbfsounding){
            if ((pRc->SoundRateIndex[i] == curValidTxRate)
                && (pRateTable->info[curValidTxRate].rateCode != MCS0))
                /* no last lower rate than mcs0 at sounding*/ 
            {
                *pNextIndex = pRc->SoundRateIndex[i-1];
                return TRUE;
            }
        } else if (pRc->validRateIndex[i] == curValidTxRate) {
            *pNextIndex = pRc->validRateIndex[i-1];
            return TRUE;
        }           
#else
        if (pRc->validRateIndex[i] == curValidTxRate) {
            *pNextIndex = pRc->validRateIndex[i-1];
            return TRUE;
        }
#endif
    }

    return FALSE;
}


/*
 * Initialize the Valid Rate Index from Rate Set
 */
static A_UINT8
rcSibSetValidRates(struct atheros_node *pSib, const RATE_TABLE_11N *pRateTable,
                   struct ieee80211_rateset *pRateSet, A_UINT32 capflag)
{
    A_UINT8      i, j, hi = 0;
    TX_RATE_CTRL *pRc  = (TX_RATE_CTRL *)(pSib);

    /* Use intersection of working rates and valid rates */
    for (i = 0; i < pRateSet->rs_nrates; i++) {
        for (j = 0; j < pRateTable->rateCount; j++) {
            A_UINT32 phy = pRateTable->info[j].phy;
            A_UINT32 valid;

            valid = getValidFlags(pSib, pRateTable, j);

        /* We allow a rate only if its valid and the capflag matches one of
         * the validity (TRUE/TRUE_20/TRUE_40) flags */

            if (((pRateSet->rs_rates[i] & 0x7F) == (pRateTable->info[j].dot11Rate & 0x7F))
           && ((valid & WLAN_RC_CAP_MODE(capflag)) ==
           WLAN_RC_CAP_MODE(capflag)) && !WLAN_RC_PHY_HT(phy))
            {

                if (!rcIsValidPhyRate(phy, capflag, FALSE))
                    continue;

#ifdef ATH_SUPPORT_UAPSD_RATE_CONTROL
                if ((pSib->uapsd_rate_ctrl) && (pRateTable->info[j].validUAPSD  != TRUE)) {
                    continue;
                }
#endif /* ATH_SUPPORT_UAPSD_RATE_CONTROL */

                pRc->validPhyRateIndex[phy][pRc->validPhyRateCount[phy]] = j;
                pRc->validPhyRateCount[phy] += 1;
                hi = A_MAX(hi, j);
            }
        }
    }
    return hi;
}

static A_UINT8
rcSibSetValidHtRates(struct atheros_node *pSib, const RATE_TABLE_11N *pRateTable,
                     A_UINT8 *pMcsSet, A_UINT32 capflag
                     , const MAX_RATES *maxRates)
{
    A_UINT8      i, j, hi = 0;
    TX_RATE_CTRL *pRc  = (TX_RATE_CTRL *)(pSib);
    A_UINT32     maxRate;        /* max rate in Kbps */

    /* Use intersection of working rates and valid rates */
    for (i = 0; i <  ((struct ieee80211_rateset *)pMcsSet)->rs_nrates; i++) {
        for (j = 0; j < pRateTable->rateCount; j++) {
            A_UINT32 phy = pRateTable->info[j].phy;
            A_UINT32 valid;

            valid = getValidFlags(pSib, pRateTable, j);
#ifdef ATH_SUPPORT_TxBF
            if (capflag & WLAN_RC_TxBF_FLAG) {
                if (((((struct ieee80211_rateset *) pMcsSet)->rs_rates[i] & 0x7F)
                    != (pRateTable->info[j].dot11Rate & 0x7F)) || !WLAN_RC_PHY_HT(phy)) {
                    continue; 
                } else {
                    if ((!(rcIsValidTxBFPhyRat(pSib,pRateTable->info[j].validTxBF))) ||
                        (!WLAN_RC_PHY_TxBF_HT_VALID(pRateTable->info[j].validTxBF, capflag)))
                        continue;
                }
            } else {
                if (((((struct ieee80211_rateset *)pMcsSet)->rs_rates[i] & 0x7F)
                    != (pRateTable->info[j].dot11Rate & 0x7F)) ||
                    !WLAN_RC_PHY_HT(phy) || !WLAN_RC_PHY_HT_VALID(valid, capflag)) { 
                    continue;
                }
            }
#else
            if (((((struct ieee80211_rateset *)pMcsSet)->rs_rates[i] & 0x7F)
                != (pRateTable->info[j].dot11Rate & 0x7F)) ||
                !WLAN_RC_PHY_HT(phy) || !WLAN_RC_PHY_HT_VALID(valid, capflag)) 
                    continue;
#endif

                if (!rcIsValidPhyRate(phy, capflag, FALSE))
                    continue;

#ifdef ATH_SUPPORT_UAPSD_RATE_CONTROL
                if ((pSib->uapsd_rate_ctrl) && (pRateTable->info[j].validUAPSD  != TRUE)) {
                    continue;
                }
#endif /* ATH_SUPPORT_UAPSD_RATE_CONTROL */

                /* 
                 * We may have mixed 20/40 rates. Pick the maxrate based on phy type.
                 */
                if (valid == TRUE_20) {
                    maxRate = maxRates->max_ht20_tx_rateKbps;
                } else if (valid == TRUE_40) {
                    maxRate = maxRates->max_ht40_tx_rateKbps;
                } else {
                    maxRate = (capflag & WLAN_RC_40_FLAG)? 
                              maxRates->max_ht40_tx_rateKbps :
                              maxRates->max_ht20_tx_rateKbps;
                }

                if (maxRate && (pRateTable->info[j].rateKbps > maxRate))
                    continue;

            pRc->validPhyRateIndex[phy][pRc->validPhyRateCount[phy]] = j;
            pRc->validPhyRateCount[phy] += 1;
            hi = A_MAX(hi, j);
        }
    }
    return hi;
}

/*
 *  Update the SIB's rate control information
 *
 *  This should be called when the supported rates change
 *  (e.g. SME operation, wireless mode change)
 *
 *  It will determine which rates are valid for use.
 */
static void
rcSibUpdate_ht(struct ath_softc *sc, struct ath_node *an, A_UINT32 capflag, int keepState,
               struct ieee80211_rateset *negotiated_rates,
               struct ieee80211_rateset *negotiated_htrates,
               const MAX_RATES *maxRates)
{
    const RATE_TABLE_11N                *pRateTable = NULL;
    struct atheros_node          *pSib      = ATH_NODE_ATHEROS(an);
    struct atheros_softc      *asc       = (struct atheros_softc*)sc->sc_rc;
    struct ieee80211_rateset  *pRateSet = negotiated_rates;
    A_UINT8              *phtMcs = (A_UINT8 *)negotiated_htrates;
    TX_RATE_CTRL *pRc  = (TX_RATE_CTRL *)(pSib);
    A_UINT8                 i, j, k, hi = 0, htHi = 0;

#ifdef ATH_SUPPORT_TxBF    
    const u_int8_t  *sounding_swap_table;
#endif

#if ATH_SUPPORT_VOWEXT
    pRc->nHeadFail = 0;
    pRc->nTailFail = 0;
    pRc->nAggrSize = 0;

    pRc->aggrLimit = MAX_AGGR_LIMIT; /* for RCA */
#endif
    pRateTable = (const RATE_TABLE_11N*)asc->hwRateTable[sc->sc_curmode];

    /* Initial rate table size. Will change depending on the working rate set */
    pRc->rateTableSize = MAX_TX_RATE_TBL;

    pRc->switchCount = 0;
    /* Initialize thresholds according to the global rate table */
    for (i = 0 ; (i < pRc->rateTableSize) && (!keepState); i++) {
		if (i < pRateTable->rateCount) {
                pRc->state[i].rssiThres = pRateTable->info[i].rssiAckValidMin;
		}
        pRc->state[i].per       = 0;
#if ATH_SUPPORT_VOWEXT /* for RCA */
        pRc->state[i].maxAggrSize = MAX_AGGR_LIMIT;
#endif
    }

    /* Determine the valid rates */
    rcInitValidTxRate(pRc);

    for (i = 0; i < WLAN_RC_PHY_MAX; i++) {
        for (j = 0; j < MAX_TX_RATE_PHY; j++) {
            pRc->validPhyRateIndex[i][j] = 0;
        }
        pRc->validPhyRateCount[i] = 0;
    }
    pRc->rcPhyMode = (capflag & WLAN_RC_40_FLAG);

    /* Set stream capability */
    pSib->singleStream = 0;
    pSib->dualStream = 0;
    pSib->tripleStream = 0;

    if (capflag & WLAN_RC_TS_FLAG) {
        pSib->tripleStream = 1;
    } else if (capflag & WLAN_RC_DS_FLAG) {
        pSib->dualStream = 1;
    } else {
        pSib->singleStream = 1;
    }

#ifdef ATH_SUPPORT_TxBF  
    
   /*   Decide its final Nss of Beamforming 
    *   By Own Tx stream number, Opposite Rx stream number & Opposite Channel Estimation Capability(0: one-stream ; 1: two-stream)
    *
    *   usedNss = min(Own_Tx_Stream-1,Opposite_Rx_Stream,Opposite_CEC+1)
    *  
    *   Own_Tx_Stream   Opposite_Rx_Stream   Opposite_CEC       usedNss    Steer_allowed_stream    Map_to_RateTable
    *           3               3               >=2(1)             2  (TS)         (2,1)               Type_A
    *           3               2               >=2(1)             2  (DS)         (2,1)               Type_B
    *           3               1               >=1(0)             1  (SS)           (1)               Type_C
    *           2               2               >=1(0)             1  (DS)           (1)               Type_D
    *           2               1               >=1(0)             1  (SS)           (1)               Type_C
    *
    *           3               3               ==1(0)             1  (TS)           (1)               Type_E
    *           3               2               ==1(0)             1  (DS)           (1)               Type_D  
    *
    */

    pSib->usedNss = 0;
    if (pSib->txbf) {
        pSib->usedNss = MIN(MIN((sc->sc_tx_numchains-1), TX_STREAM_USED_NUM(pSib)), OPPOSITE_CEC_NUM(capflag));
    }
 
#endif //#ifdef  ATH_SUPPORT_TxBF


    if (!pRateSet->rs_nrates) {
        /* No working rate, just initialize valid rates */
        hi = rcSibInitValidRates(pSib, pRateTable, capflag, maxRates);
    } else {
        /* Use intersection of working rates and valid rates */
        hi = rcSibSetValidRates(pSib, pRateTable, pRateSet, capflag);
        if (capflag & WLAN_RC_HT_FLAG) {
            htHi = rcSibSetValidHtRates(pSib, pRateTable, phtMcs, capflag
                , maxRates);
        }
        hi = A_MAX(hi, htHi);
    }

    pRc->rateTableSize = hi + 1;
    pRc->rateMaxPhy = 0;
    ASSERT(pRc->rateTableSize <= MAX_TX_RATE_TBL);

    for (i = 0, k = 0; i < WLAN_RC_PHY_MAX; i++) {
        for (j = 0; j < pRc->validPhyRateCount[i]; j++) {
            pRc->validRateIndex[k++] = pRc->validPhyRateIndex[i][j];
        }

        if (!rcIsValidPhyRate(i, pRateTable->initialRateMax, TRUE) || !pRc->validPhyRateCount[i])
               continue;

        pRc->rateMaxPhy = pRc->validPhyRateIndex[i][j-1];
    }
    ASSERT(pRc->rateTableSize <= MAX_TX_RATE_TBL);
    ASSERT(k <= MAX_TX_RATE_TBL);

    pRc->maxValidRate = k;
    /*
     * Some third party vendors don't send the supported rate series in order. So sorting to
     * make sure its in order, otherwise our RateFind Algo will select wrong rates
     */
    rcSortValidRates(pRateTable, pRc);
    rcSkipLGIRates(pRateTable, pRc);

    /* 
     * we do not want to start using the maximum supported rate for the node right away.
     * start max phy rate  4 rates below the maximum supported rate of the node. 
     * if the number of supported rates is less thatn 8 then start from the rate
     * which is in the middle of the supported rates.
     */
    if ( k > 7) {
        pRc->rateMaxPhy = pRc->validRateIndex[k-4];
    } else {
        pRc->rateMaxPhy = pRc->validRateIndex[k-(k/2)];
    }

#ifdef ATH_SUPPORT_TxBF
    /*  Set up sounding rate table. Sounding rate table is used to swap rate while TxBF sounding
     **/ 
    
    if (capflag & WLAN_RC_TxBF_FLAG){    // setup rate table for sounding
        if ((sc->sc_curmode == WIRELESS_MODE_11NA_HT20) ||(sc->sc_curmode == WIRELESS_MODE_11NA_HT40PLUS) 
            ||(sc->sc_curmode == WIRELESS_MODE_11NA_HT40MINUS)){    //A_band
            if (capflag && WLAN_RC_40_FLAG){  
                sounding_swap_table = sounding_swap_table_A_40;
            } else {
                sounding_swap_table = sounding_swap_table_A_20;
            }
        } else {
            if (capflag && WLAN_RC_40_FLAG){  
                sounding_swap_table = sounding_swap_table_G_40;
            } else {
                sounding_swap_table = sounding_swap_table_G_20; // g band table
            }
        }
        for (i=0 ; i < pRc->maxValidRate ; i++){
            u_int8_t    ratecode, swapratecode = MCS0, rtidx;
            
            ratecode = pRateTable->info[pRc->validRateIndex[i]].rateCode; // orignal rate code
            if (LEGACY_RATE(ratecode)) {
                pRc->SoundRateIndex[i] = pRc->validRateIndex[i];     // no txbf rate, keep original
            } else {
                // before osprey 2.2 , it has a bug that will cause sounding invalid problem at 3 stream
                // (sounding bug when NESS=0), so it should swap to  2 stream rate
                if (TS_RATE(ratecode)) {                                // three stream rate
                    struct ath_hal *ah = sc->sc_ah;
                    
                    if (ath_hal_support_ts_sounding(ah)) {
                        swapratecode = ratecode;                
                    } else {
                        swapratecode = TS_SOUNDING_SWAP_RATE;    // force swap to (two stream rate)
                    }
                } else {
                    //find current rate in swap table
                    swapratecode = sounding_swap_table[(ratecode & SWAP_RATE)]; //swap rate
                }
                if (ratecode == swapratecode){
                    pRc->SoundRateIndex[i] = pRc->validRateIndex[i]; //don't need to swap
                } else {
                    rtidx = pRc->validRateIndex[i];
                    if (ratecode > swapratecode){             // search swapped rate index
                        while ( rtidx >0){
                            rtidx --;
                            if ( pRateTable -> info[rtidx].rateCode == swapratecode){
                                pRc->SoundRateIndex[i] = rtidx;
                                break;
                            }
                        }
                    } else {
                        while (rtidx < MAX_TX_RATE_TBL-1){
                            rtidx ++;
                            if (pRateTable->info[rtidx].rateCode == swapratecode) {
                                pRc->SoundRateIndex[i] = rtidx;
                                break;
                            }
                        }
                    }
                }
            }
           //printk("org idx %d,codate %x; swap idx %d, coderate %x \n",pRc->validRateIndex[i],pRateTable->info[pRc->validRateIndex[i]].rateCode,
           //         pRc->SoundRateIndex[i],pRateTable->info[pRc->SoundRateIndex[i]].rateCode);
        }
    }
#endif
    //dump_valid_rate_index(pRateTable, pRc->validRateIndex);
    //printk("RateTable:%d, maxvalidrate:%d, ratemax:%d\n", pRc->rateTableSize,k,pRc->rateMaxPhy);

}

void
rcSibUpdate_11n( struct ath_softc *sc, struct ath_node *pSib,
                      A_UINT32 capflag, int keepState,
                      struct ieee80211_rateset *negotiated_rates,
                      struct ieee80211_rateset *negotiated_htrates
                      , const MAX_RATES *maxRates)
{
    rcSibUpdate_ht(sc, pSib, capflag,
                   keepState, negotiated_rates, negotiated_htrates, maxRates);
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

static A_UINT8
rcRateFind_ht(struct ath_softc *sc, struct ath_node *an, struct atheros_node *pSib, A_UINT8 ac,
          const RATE_TABLE_11N *pRateTable, int probeAllowed, int *isProbing, int isretry)
{
    A_UINT32             dt;
    A_UINT32             bestThruput, thisThruput;
    A_UINT32             nowMsec;
    A_UINT8              rate, nextRate, bestRate;
    A_RSSI               rssiLast, rssiReduce=0;
    A_UINT8              maxIndex, minIndex;
    A_INT8               index;
    TX_RATE_CTRL         *pRc = NULL;
#ifdef ATH_SUPPORT_TxBF
    A_INT8               rtidx=0;
#endif
    ASSERT (pSib);
    pRc = (TX_RATE_CTRL *)pSib;
    *isProbing = FALSE;

    rssiLast   = median(pRc->rssiLast, pRc->rssiLastPrev, pRc->rssiLastPrev2);

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
    pRc->rptGoodput = 0;
    bestThruput = 0;
    maxIndex    = pRc->maxValidRate-1;

    /* FIXME: XXX */
    minIndex    = 0;
    bestRate    = minIndex;
    /*
     * Try the higher rate first. It will reduce memory moving time
     * if we have very good channel characteristics.
     */

    for (index = maxIndex; index >= minIndex ; index--) {
        A_UINT8 perThres;

        rate = pRc->validRateIndex[index];

        /* since higher rate index might have lower data rate, And we want to 
         *  sort out the rate index with higher data rate than pRc->rateMaxPhy
         *  , it should use pRateTable->info[rate].userRateKbps instead of use 
         *  rate index itself directly.
         */
        /*if (rate > pRc->rateMaxPhy) {
            continue;
        }*/
        if (pRateTable->info[rate].userRateKbps > 
            pRateTable->info[pRc->rateMaxPhy].userRateKbps) {
            continue;
        }

        /*
         * For TCP the average collision rate is around 11%,
         * so we ignore PERs less than this.  This is to
         * prevent the rate we are currently using (whose
         * PER might be in the 10-15 range because of TCP
         * collisions) looking worse than the next lower
         * rate whose PER has decayed close to 0.  If we
         * used to next lower rate, its PER would grow to
         * 10-15 and we would be worse off then staying
         * at the current rate.
         */
        perThres = pRc->state[rate].per;
        if ( perThres < ATH_RATE_TGT_PER_PCT ) {
            perThres = ATH_RATE_TGT_PER_PCT;
        }

        thisThruput = pRateTable->info[rate].userRateKbps *
                      (100 - perThres);

        if (bestThruput <= thisThruput) {
            bestThruput = thisThruput;
            bestRate    = rate;
#ifdef ATH_SUPPORT_TxBF
            rtidx = index;
#endif
        }
    }

    rate = bestRate;
    pRc->rptGoodput = bestThruput;
#if 0
    /* if we are retrying for more than half the number
     * of max retries, use the min rate for the next retry
     */
    if (isretry)
        rate = pRc->validRateIndex[minIndex];
#endif

    pRc->rssiLastLkup = rssiLast;

    /* probe only when current rate is max phy and not sounding 
       and current PER is lower enough (less or equal to target PER)
    */
#if ATH_ANI_NOISE_SPUR_OPT
	/* Ignore low PER if its just a noise spur */
    if ((rate == pRc->rateMaxPhy) && probeAllowed && (!(ATH_TXBF_SOUNDING(sc)))
	&&((pRc->state[rate].per <= ATH_RATE_TGT_PER_PCT)
	   || (ath_hal_is_ani_noise_spur(sc->sc_ah)
	       && sc->sc_config.ath_noise_spur_opt)))
#else
    if ((rate == pRc->rateMaxPhy) && probeAllowed && (!(ATH_TXBF_SOUNDING(sc)))
	&&((pRc->state[rate].per <= ATH_RATE_TGT_PER_PCT)))
#endif
    {
            /* Probe the next allowed phy state */
        if (rcGetNextValidTxRate( pRateTable, pRc, rate, &nextRate) &&
            (nowMsec - pRc->probeTime > pRateTable->probeInterval) &&
            (pRc->hwMaxRetryPktCnt >= 1)){
            rate                  = nextRate;
            pRc->probeRate        = rate;
            pRc->probeTime        = nowMsec;
            pRc->hwMaxRetryPktCnt = 0;
            *isProbing            = TRUE;
        }
    }
    /*
     * Make sure rate is not higher than the allowed maximum.
     * We should also enforce the min, but I suspect the min is
     * normally 1 rather than 0 because of the rate 9 vs 6 issue
     * in the old code.
     */
    if (rate > (pRc->rateTableSize - 1)) {
        rate = pRc->rateTableSize - 1;
    }

#ifndef REMOVE_PKT_LOG
    if (!sc->sc_txaggr || (sc->sc_txaggr && sc->sc_log_rcfind))
    {
        struct log_rcfind log_data = {0};
        log_data.rc = pRc;
        log_data.rateCode = pRateTable->info[rate].rateCode;
        log_data.rate = rate;
        log_data.rssiReduce = rssiReduce;
        log_data.misc[0] = pRc->state[rate].per;
        log_data.misc[1] = pRc->state[rate].rssiThres;
        log_data.ac = ac;
        log_data.isProbing = *isProbing;
        ath_log_rcfind(sc, &log_data, 0);
        sc->sc_log_rcfind = 0;
    }
#endif

    /* record selected rate, which is used to decide if we want to do fast 
     * frame
     */
    if (!(*isProbing) && pSib) {
        pSib->lastRateKbps = pRateTable->info[rate].rateKbps;
    }
#define VALID_RATE_FOR_MODE(_pRateTable,_pSib,_rate) \
        VALID_RATE(_pRateTable,_pSib,_rate)

    if (!(VALID_RATE_FOR_MODE(pRateTable,pSib,rate))) {
           int rateindex;
           for(rateindex=0;rateindex<pRateTable->rateCount;++rateindex) {
              if (VALID_RATE_FOR_MODE(pRateTable,pSib,rateindex)) {
                 break;
              }
           }
           if (rateindex <pRateTable->rateCount ) {
               rate = rateindex;
           }
    }
#ifdef ATH_SUPPORT_TxBF
    if (sc->sc_txbfsounding) {    // swap rate when TxBF sounding
        //printk("org idx %d %d,codate %x %x; swap idx %d, coderate %x \n",rate, pRc->validRateIndex[rtidx],
        //    pRateTable->info[rate].rateCode,pRateTable->info[pRc->validRateIndex[rtidx]].rateCode,
        //            pRc->SoundRateIndex[rtidx],pRateTable->info[pRc->SoundRateIndex[rtidx]].rateCode);
        rate = pRc->SoundRateIndex[rtidx];
    }
#endif
    return rate;
}

static void
rcRateSetseries(struct ath_rc_series *rcs0, const RATE_TABLE_11N *pRateTable ,
                struct ath_rc_series *series, A_UINT8 tries, A_UINT8 rix,
                int rtsctsenable, struct atheros_node *asn, u_int8_t txchains)
{
#ifdef ATH_SUPPORT_TxBF
    struct atheros_softc *asc = asn->asc;
#endif

    series->tries = tries;
    series->flags = (rtsctsenable? ATH_RC_RTSCTS_FLAG : 0) |
            (WLAN_RC_PHY_DS(pRateTable->info[rix].phy) ? ATH_RC_DS_FLAG : 0) |
            (WLAN_RC_PHY_TS(pRateTable->info[rix].phy) ? ATH_RC_TS_FLAG : 0) |
            (WLAN_RC_PHY_40(pRateTable->info[rix].phy) ? ATH_RC_CW40_FLAG : 0) |
            (WLAN_RC_PHY_SGI(pRateTable->info[rix].phy) ? ATH_RC_SGI_FLAG : 0);
    series->rix = pRateTable->info[rix].baseIndex;
    series->max4msframelen = pRateTable->info[rix].max4msframelen;

    if (asn->stbc) {

        /* For now, only single stream STBC is supported */
        if (pRateTable->info[rix].rateCode >= 0x80 && 
            pRateTable->info[rix].rateCode <= 0x87) {
            series->flags |= ATH_RC_TX_STBC_FLAG; 
        }
    }
    series->rix = pRateTable->info[rix].baseIndex;
    series->max4msframelen = pRateTable->info[rix].max4msframelen;
    /* Only single and dual stream TxBF is supported */
#ifdef ATH_SUPPORT_TxBF
    if (asn->txbf && VALID_TXBF_RATE(pRateTable->info[rix].rateCode, asn->usedNss)
        /* disable BF when current BW is 20M and rcs0 BW is 40 M */
        && (!((!(series->flags & ATH_RC_CW40_FLAG)) && (rcs0->flags & ATH_RC_CW40_FLAG)))
        /* disable BF when rcs0 is mcs0 and > 80% last Tx frame's rate is mcs0*/
        && !((pRateTable->info[rcs0->rix].rateCode == MCS0)
                && (asn->lastTxmcs0cnt > MCS0_80_PERCENT))
        && !(asc->sc_txbf_disable_flag[pRateTable->info[rix].baseIndex][txchains - 1]))
    {
        series->flags |= ATH_RC_TXBF_FLAG;
        series->flags &= ~(ATH_RC_TX_STBC_FLAG); /* disable STBC at TxBF mode*/
    }
    if (asn->txbf_sounding) { 
            series->flags &= ~(ATH_RC_SGI_FLAG);     /*disable SGI at sounding*/ 
            series->flags |= ATH_RC_SOUNDING_FLAG;
            asn->txbf_rpt_received = 0;
    }
#endif

}

static A_UINT8
rcRateGetIndex(struct ath_softc *sc, struct ath_node *an,
               const RATE_TABLE_11N *pRateTable,
           A_UINT8 rix, A_UINT16 stepDown, A_UINT16 minRate)
{
    A_UINT32                j;
    A_UINT8                 nextIndex;
    struct atheros_node     *pSib = ATH_NODE_ATHEROS(an);
    TX_RATE_CTRL     *pRc = (TX_RATE_CTRL *)(pSib);
    A_UINT8                 opt;

#ifdef ATH_SUPPORT_TxBF
    opt = sc->sc_txbfsounding;
#else
    opt = 0;
#endif

    if (minRate) {
        for (j = RATE_TABLE_SIZE_11N; j > 0; j-- ) {
            if (rcGetNextLowerValidTxRate(pRateTable, pRc, rix, &nextIndex, opt)) {
                rix = nextIndex;
            } else {
                break;
            }
        }
    } else {
        for (j = stepDown; j > 0; j-- ) {
            if (rcGetNextLowerValidTxRate(pRateTable, pRc, rix, &nextIndex, opt)) {
                rix = nextIndex;
            } else {
                break;
            }
        }
    }
#ifdef ATH_SUPPORT_TxBF
    if (sc->sc_txbfcalibrating) // calibration frame
    {
        if ((sc->sc_curmode==WIRELESS_MODE_11NG_HT20) || (sc->sc_curmode==WIRELESS_MODE_11NG_HT40PLUS) 
            || (sc->sc_curmode==WIRELESS_MODE_11NG_HT40MINUS) ) // g-band rate
            rix = 0xc;
        else     
            rix = 0x8;         // force mcs0 when not HT rate.
    }
#endif
    return rix;
}

void
rcRateFind_11n(struct ath_softc *sc, struct ath_node *an, A_UINT8 ac,
              int numTries, int numRates, unsigned int rcflag,
              struct ath_rc_series series[], int *isProbe, int isretry)
{
    A_UINT8               i = 0;
    A_UINT8               tryPerRate = 0;
    struct atheros_softc  *asc = (struct atheros_softc*)sc->sc_rc;
    const RATE_TABLE_11N        *pRateTable = (const RATE_TABLE_11N *)asc->hwRateTable[sc->sc_curmode];
    struct atheros_node   *asn = ATH_NODE_ATHEROS(an);
    A_UINT8               rix, nrix;
    TX_RATE_CTRL          *pRc = (TX_RATE_CTRL*)&(asn->txRateCtrl);

    rix = rcRateFind_ht(sc, an, asn, ac, pRateTable, (rcflag & ATH_RC_PROBE_ALLOWED) ? 1 : 0, isProbe, isretry);
    nrix = rix;

#if ATH_ANT_DIV_COMB
    if (sc->sc_smart_antenna && !(*isProbe) && !isretry)
        ath_smartant_tx_scan(sc->sc_sascan, pRateTable->info[rix].rateKbps);
#endif
    
    if ((rcflag & ATH_RC_PROBE_ALLOWED) && (*isProbe)) {
        /* set one try for probe rates. For the probes don't enable rts */

        rcRateSetseries(&series[0], pRateTable, &series[i++], 1, nrix, FALSE, asn, sc->sc_tx_numchains);

        tryPerRate = (numTries/numRates);
        /* Get the next tried/allowed rate. No RTS for the next series
         * after the probe rate
         */
        nrix = rcRateGetIndex( sc, an, pRateTable, nrix, 1, FALSE);

        rcRateSetseries(&series[0], pRateTable, &series[i++], tryPerRate, nrix, 0, asn, sc->sc_tx_numchains);

        if (pRateTable->info[nrix].rateKbps <= sc->sc_limitrate)
        {
            if (sc->sc_en_rts_cts) 
                series[0].flags |= ATH_RC_RTSCTS_FLAG ;
        }	

    } else {
        tryPerRate = (numTries/numRates);
        /* Set the choosen rate. No RTS for first series entry. */
        rcRateSetseries(&series[0], pRateTable, &series[i++], tryPerRate, nrix, FALSE, asn, sc->sc_tx_numchains);

        if (pRateTable->info[nrix].rateKbps <= sc->sc_limitrate)
        {
            if (sc->sc_en_rts_cts)
                series[0].flags |= ATH_RC_RTSCTS_FLAG ;
        }	
    }

    /* Fill in the other rates for multirate retry */
    for ( ; i < numRates; i++ ) {
        A_UINT8 tryNum;
        A_UINT8 minRate;

        tryNum  = ((i + 1) == numRates) ? numTries - (tryPerRate * i) : tryPerRate ;
        minRate = (((i + 1) == numRates) && (rcflag & ATH_RC_MINRATE_LASTRATE)) ? 1 : 0;

        nrix =rcRateGetIndex(sc, an, pRateTable, nrix, 1, minRate);
        
        /* All other rates in the series have RTS enabled */
        rcRateSetseries(&series[0], pRateTable, &series[i], tryNum, nrix, TRUE, asn, sc->sc_tx_numchains);
    }

#ifdef ATH_SUPPORT_UAPSD_RATE_CONTROL
    /*
   * If the node is a UAPSD node, i.e. its primary traffic is voice traffic,
   * set the multirate retries to perform an aggresive drop down to the lowest rate.
   */
    if(asn->uapsd_rate_ctrl) {
        A_UINT8 j=0;

        for (i=0; i < numRates; i++) {
            if (series[i].rix == 0) {
                for (j=0; j < numRates; j++) {
                    if (j == i) {
                        series[j].tries = 8;
                    } else if (j < i) {
                        series[j].tries = 1;
                    } else {
                        series[j].tries = 0;
                    }
                }
                break;
            } else if (i == (numRates-1)) {
                for (j=0; j <= i; j++) {
                    if (j == i) {
                        series[j].tries = 8;
                    } else {
                        series[j].tries = 1;
                    }
                }
                break;
            }
        }
    }
#endif /* ATH_SUPPORT_UAPSD_RATE_CONTROL */

    /*
     * BUG 26545:
     * Change rate series to enable aggregation when operating at lower MCS rates.
     * When first rate in series is MCS2 in HT40 @ 2.4GHz, series should look like:
     *    {MCS2, MCS1, MCS0, MCS0}.
     * When first rate in series is MCS3 in HT20 @ 2.4GHz, series should look like:
     *    {MCS3, MCS2, MCS1, MCS1}
     * So, set fourth rate in series to be same as third one for above conditions.
     */
    if ((sc->sc_curmode == WIRELESS_MODE_11NG_HT20) ||
        (sc->sc_curmode == WIRELESS_MODE_11NG_HT40PLUS) ||
        (sc->sc_curmode == WIRELESS_MODE_11NG_HT40MINUS))
    {
        A_UINT8  dot11Rate = pRateTable->info[rix].dot11Rate;
        WLAN_PHY phy = pRateTable->info[rix].phy;
        if (i == 4 &&
            ((dot11Rate == 2 && phy == WLAN_RC_PHY_HT_40_SS) ||
             (dot11Rate == 3 && phy == WLAN_RC_PHY_HT_20_SS)))
        {
            series[3].rix = series[2].rix;
            series[3].flags = series[2].flags;
            series[3].max4msframelen = series[2].max4msframelen;
        }
    }

    /* override the rate control chosen series if there are
     * 10 consecutive RTS retry failures. Choose the following
     * tries=[1,0,0,0], rts/cts=[1,1,1,1]. This avoids the overhead
     * flooding RTS too many times.
     */
    if (pRc->consecRtsFailCount > MAX_CONSEC_RTS_FAILED_FRAMES) {
        int si = 0;

        series[0].tries = 2;
        series[1].tries = series[2].tries = series[3].tries = 0;
        for (si = 0;  si < 4; si++) {
            series[si].flags |= ATH_RC_RTSCTS_FLAG;
        }
    }

#ifdef ATH_SUPPORT_LINUX_STA
    if (sc->sc_ieee_ops->update_txrate) {
        sc->sc_ieee_ops->update_txrate(an->an_node, rix | IEEE80211_RATE_MCS);
    }	
#endif

#if ATH_SUPPORT_HYFI_ENHANCEMENTS
    pRc->TxRateInMbps += pRateTable->info[rix].rateKbps / 1000;
    //printk("the rate is set to %d\n", pRateTable->info[rix].rateKbps);
    pRc->Max4msFrameLen += pRateTable->info[rix].max4msframelen;
    pRc->TxRateCount += 1;
#endif
}
#ifdef ATH_SUPPORT_VOWEXT
static void
rcUpdate_ht(struct ath_softc *sc, struct ath_node *an,  A_UINT8 ac,
            int txRate, int Xretries, int retries, A_RSSI rssiAck,
            A_UINT16 nFrames, A_UINT16 nBad, int isProbe, int short_retry_fail)
#else
static void
rcUpdate_ht(struct ath_softc *sc, struct ath_node *an,  A_UINT8 ac,
            int txRate, int Xretries, int retries, A_RSSI rssiAck,
            A_UINT16 nFrames, A_UINT16 nBad, int short_retry_fail)
#endif
{
    TX_RATE_CTRL   *pRc;
    A_UINT32              nowMsec     = A_MS_TICKGET();
    int                stateChange = FALSE;
    A_UINT8               lastPer;
    int                   rate,count;
    //struct ieee80211com   *ic         = &sc->sc_ic;
    struct atheros_node   *pSib       = ATH_NODE_ATHEROS(an);
    struct atheros_softc  *asc        = (struct atheros_softc*)sc->sc_rc;
    const RATE_TABLE_11N            *pRateTable = (const RATE_TABLE_11N *)asc->hwRateTable[sc->sc_curmode];
#if ATH_ANI_NOISE_SPUR_OPT
    bool ani_noise_spur = 0;
#endif     
    static const A_UINT32 nRetry2PerLookup[10] = {
        100 * 0 / 1,
        100 * 1 / 4,
        100 * 1 / 2,
        100 * 3 / 4,
        100 * 4 / 5,
        100 * 5 / 6,
        100 * 6 / 7,
        100 * 7 / 8,
        100 * 8 / 9,
        100 * 9 / 10
    };

    if (!pSib) {
        return;
    }

    pRc        = (TX_RATE_CTRL *)(pSib);
    
    // ASSERT(retries >= 0 && retries < MAX_TX_RETRIES);
    ASSERT(txRate >= 0);
    if (txRate < 0) {
            //printk("%s: txRate value of 0x%x is bad.\n", __FUNCTION__, txRate);
            return;
    }

#ifdef ATH_SUPPORT_TxBF
    /* calulate mcs0 percentage for last 255 frame*/
    if  (pRateTable->info[txRate].rateCode == MCS0){
        pSib->Txmcs0cnt++;        
    }
    pSib->pktcount++;
    if (pSib->pktcount == 255){
        pSib->pktcount = 0;
        pSib->lastTxmcs0cnt = pSib->Txmcs0cnt;
        pSib->Txmcs0cnt = 0;
    }    
#endif

    /* if tx failed due to RTS retry, do not update the PER, but 
     * log entry
     */
    if (short_retry_fail) {
        if (an->an_rc_node->txRateCtrl.rate_fast_drop_en) {
#ifdef QCA_SUPPORT_CP_STATS
            if (sc->sc_rc_rx_mgmt_num &&
                    (pdev_cp_stats_rx_num_mgmt_get(sc->sc_pdev) > sc->sc_rc_rx_mgmt_num))
#endif
            {
                /*  Chip still receive mgmt frames from peer point. 
                 *  The RTS fail should trigger Tx Rate down.
                 */
            } else {
#ifdef QCA_SUPPORT_CP_STATS
                sc->sc_rc_rx_mgmt_num = pdev_cp_stats_rx_num_mgmt_get(sc->sc_pdev);
#endif
                goto out;
            }
        } else {
            goto out;
        }
    }
    
    
    /* To compensate for some imbalance between ctrl and ext. channel */

    if (WLAN_RC_PHY_40(pRateTable->info[txRate].phy))
        rssiAck = rssiAck < 3? 0: rssiAck - 3;

    lastPer = pRc->state[txRate].per;

#if QCA_AIRTIME_FAIRNESS
    if (sc->sc_atf_enable == ATF_TPUT_BASED) {
        int n;
        unsigned char nframe_to_aggr_bucket[32] = { 0, 0, 0, 0, 1, 1, 1, 1,
                                                    2, 2, 2, 2, 3, 3, 3, 3,
                                                    4, 4, 4, 4, 5, 5, 5, 5,
                                                    6, 6, 6, 6, 7, 7, 7, 7 };

        if (pRc->state[txRate].used < 5000)
            pRc->state[txRate].used++;

        if (nFrames == 0)
            n = 1;
        else if (nFrames > 32)
            n = 32;
        else
            n = nFrames;
        pRc->state[txRate].aggr |= (0x1u << (n - 1));

        if (pRc->state[txRate].aggr_count[nframe_to_aggr_bucket[n - 1]] < 5000)
            pRc->state[txRate].aggr_count[nframe_to_aggr_bucket[n - 1]]++;
    }
#endif

    if (Xretries) {
        /* Update the PER. */
        if (Xretries == 1) {
            pRc->state[txRate].per += 30;
            if (pRc->state[txRate].per > 100) {
                pRc->state[txRate].per = 100;
            }

        } else {
            /* Xretries == 2 */
#ifdef MULTI_RATE_RETRY_ENABLE
            count = sizeof(nRetry2PerLookup) / sizeof(nRetry2PerLookup[0]);
            if (retries >= count) {
                retries = count - 1;
            }
            /* new_PER = 7/8*old_PER + 1/8*(currentPER) */
            pRc->state[txRate].per = (A_UINT8)(pRc->state[txRate].per - (pRc->state[txRate].per >> 3) +
                                     ((100) >> 3));
#endif
        }

        /* Xretries == 1 or 2 */

        if (pRc->probeRate == txRate)
            pRc->probeRate = 0;

    } else {    /* Xretries == 0 */

        /* Update the PER. */
        /* Make sure it doesn't index out of array's bounds. */
        count = sizeof(nRetry2PerLookup) / sizeof(nRetry2PerLookup[0]);
        if (retries >= count) {
            retries = count - 1;
        }
        if (nBad) {
            /* new_PER = 7/8*old_PER + 1/8*(currentPER)  */
        /*
             * Assuming that nFrames is not 0.  The current PER
             * from the retries is 100 * retries / (retries+1),
             * since the first retries attempts failed, and the
             * next one worked.  For the one that worked, nBad
             * subframes out of nFrames wored, so the PER for
             * that part is 100 * nBad / nFrames, and it contributes
             * 100 * nBad / (nFrames * (retries+1)) to the above
             * PER.  The expression below is a simplified version
             * of the sum of these two terms.
             */
            if (nFrames > 0){
        
                pRc->state[txRate].per = (A_UINT8)(pRc->state[txRate].per -
                                     (pRc->state[txRate].per >> 3) +
                ((100*(retries*nFrames + nBad)/(nFrames*(retries+1))) >> 3));
            }
        } else {
            /* new_PER = 7/8*old_PER + 1/8*(currentPER) */

            pRc->state[txRate].per = (A_UINT8)(pRc->state[txRate].per -
            (pRc->state[txRate].per >> 3) + (nRetry2PerLookup[retries] >> 3));
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
            if (retries > 0 || 2 * nBad > nFrames) {
                /*
                 * Since we probed with just a single attempt,
                 * any retries means the probe failed.  Also,
                 * if the attempt worked, but more than half
                 * the subframes were bad then also consider
                 * the probe a failure.
                 */
                pRc->probeRate = 0;
            } else {

                pRc->rateMaxPhy = pRc->probeRate;

                if (pRc->state[pRc->probeRate].per > 30) {
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
            pRc->hwMaxRetryPktCnt = 0;
        } else {
            /*
             * It worked with no retries.  First ignore bogus (small)
             * rssiAck values.
             */
            if (txRate == pRc->rateMaxPhy && pRc->hwMaxRetryPktCnt < 255) {
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

                if (pRc->rssiSumCnt > 4) {
                    A_RSSI32 rssiAckAvg = (pRc->rssiSum + 2) / 4;

                    pRc->rssiSum = pRc->rssiSumCnt = 0;

                    /* Now reduce the current rssi threshold. */
                    if ((rssiAckAvg < pRc->state[txRate].rssiThres + 2) &&
                        (pRc->state[txRate].rssiThres >
                    pRateTable->info[txRate].rssiAckValidMin))
                    {
                        pRc->state[txRate].rssiThres--;
                    }

                    stateChange = TRUE;
                }
            }
        }
    }


    /* For all cases */

    // ASSERT((pRc->rateMaxPhy >= 0 && pRc->rateMaxPhy <= pRc->rateTableSize && pRc->rateMaxPhy != INVALID_RATE_MAX));

#if ATH_ANI_NOISE_SPUR_OPT
    ani_noise_spur = (sc->sc_config.ath_noise_spur_opt && ath_hal_is_ani_noise_spur(sc->sc_ah));
#endif
    /*
     * If this rate looks bad (high PER) then stop using it for
     * a while (except if we are probing).
     */
#if ATH_ANI_NOISE_SPUR_OPT
    /* If it is just a spur, continue to use the rate */
    if (ani_noise_spur == 0 && pRc->state[txRate].per >= 55 && txRate > 0 &&
#else
    if (pRc->state[txRate].per >= 55 && txRate > 0 &&
#endif
        pRateTable->info[txRate].rateKbps <=
                pRateTable->info[pRc->rateMaxPhy].rateKbps)
    {
        rcGetNextLowerValidTxRate(pRateTable, pRc, (A_UINT8) txRate,
                                &pRc->rateMaxPhy,0);

        /* Don't probe for a little while. */
        pRc->probeTime = nowMsec;
    }


    /* When at max allow rate, if current rate expected throuhgput is lower 
      then next lower rate expected throughput ,then drop one rate*/
#if ATH_ANI_NOISE_SPUR_OPT
    /* Don't drop rates if it is just a spur */
    if (ani_noise_spur == 0 && pRc->rateMaxPhy == txRate){
#else    
    if (pRc->rateMaxPhy == txRate){
#endif
        A_UINT8 nextrate = 0 ;

        if (rcGetNextLowerValidTxRate(pRateTable, pRc, (A_UINT8) txRate, &nextrate,0)){
            if ((pRateTable->info[txRate].userRateKbps *(100 - pRc->state[txRate].per))
                < (pRateTable->info[nextrate].userRateKbps *(100 -ATH_RATE_TGT_PER_PCT))) {
                pRc->rateMaxPhy = nextrate;
            }

#ifdef ATH_SUPPORT_TxBF            
            /* send sounding if per if large*/
            if ((pRc->state[txRate].per >= pSib->avp->athdev_vap->av_config.av_cvupdate_per)
                && (IS_TXBF_RATE_CONTROL(pSib)))
            {
                /* send sounding if report for previous sounding is received*/
                if (pSib->txbf_rpt_received){
                    pSib->txbf_sounding_request = 1;
                    pSib->txbf_rpt_received = 0;
                    //DbgPrint("==>%s: per >%d sound requested",__func__, pSib->avp->athdev_vap->av_config.av_cvupdate_per);
                }
            }
#endif
        }              
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

    /* since higher rate index might have lower data rate, it should use 
        userRateKbps instead of use index directly */
    /* Make sure the rates below this have lower PER */
    /* Monotonicity is kept only for rates below the current rate. */
    /*  if (pRc->state[txRate].per < lastPer) {
        for (rate = txRate - 1; rate >= 0; rate--) {
            if (pRc->state[rate].per > pRc->state[rate+1].per){
                pRc->state[rate].per = pRc->state[rate+1].per;
            }
        }
    }*/

    /* Maintain monotonicity for rates above the current rate*/
    /* for (rate = txRate; rate < pRc->rateTableSize - 1; rate++) {
        if (pRc->state[rate+1].per < pRc->state[rate].per) {
            pRc->state[rate+1].per = pRc->state[rate].per;
        }
    }*/ 

    for (rate=0; rate <pRc->rateTableSize - 1; rate++){
        if (pRateTable->info[txRate].userRateKbps  <
            pRateTable->info[rate].userRateKbps) {            
            if (pRc->state[rate].per < pRc->state[txRate].per) {
                pRc->state[rate].per = pRc->state[txRate].per;
            }
        } else {
            if (pRc->state[rate].per > pRc->state[txRate].per){
                pRc->state[rate].per = pRc->state[txRate].per;
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
    if (nowMsec - pRc->perDownTime >= pRateTable->rssiReduceInterval) {
        for (rate = 0; rate < pRc->rateTableSize; rate++) {
            pRc->state[rate].per = 7*pRc->state[rate].per/8;
        }

        pRc->perDownTime = nowMsec;
    }
out:
#ifndef REMOVE_PKT_LOG
    {
        struct log_rcupdate log_data = {0};

        log_data.rc = pRc;
        log_data.txRate = txRate;
        log_data.rateCode = pRateTable->info[txRate].rateCode;
        log_data.Xretries = Xretries;
        log_data.currentBoostState = 0;
        log_data.useTurboPrime = 0;
        log_data.retries = retries;
        log_data.rssiAck = rssiAck;
        log_data.ac = ac;
        ath_log_rcupdate(sc, &log_data, 0);
   }
#else

    return;
#endif
}

/*
 * This routine is called by the Tx interrupt service routine to give
 * the status of previous frames.
 */
#ifdef ATH_SUPPORT_VOWEXT
void
rcUpdate_11n(struct ath_softc *sc, struct ath_node *an, A_RSSI rssiAck, A_UINT8 ac,
               int finalTSIdx, int Xretries, struct ath_rc_series rcs[], int nFrames,
           int  nBad, int long_retry, int short_retry_fail, int nHeadFail, int nTailFail)
#else
void
rcUpdate_11n(struct ath_softc *sc, struct ath_node *an, A_RSSI rssiAck, A_UINT8 ac,
               int finalTSIdx, int Xretries, struct ath_rc_series rcs[], int nFrames,
           int  nBad, int long_retry, int short_retry_fail)
#endif
{
    A_UINT32              series = 0;
    A_UINT32              rix;
    struct atheros_softc  *asc        = (struct atheros_softc*)sc->sc_rc;
    const RATE_TABLE_11N            *pRateTable = (const RATE_TABLE_11N *)asc->hwRateTable[sc->sc_curmode];
    struct atheros_node   *pSib;
    TX_RATE_CTRL          *pRc;
    A_UINT16              flags;
    static const A_UINT8       translation_table_5g [] = 
                   {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,17,18,19,20,21,22,23,24,26,27,28,29,30,32,34,36};
	static const A_UINT8       translation_table_2g [] = 
				   {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,21,22,23,24,25,26,27,28,30,31,32,33,34,36,38,40};
    A_UINT8              idx;

	/* Modify for static analysis, prevent an is NULL */
    if (!an) {
    // panic ("rcUpdate an is NULL");
        return;
    }
	
    pSib = ATH_NODE_ATHEROS(an);
    pRc = (TX_RATE_CTRL *)(pSib);

	pSib = ATH_NODE_ATHEROS(an);
	pRc  = (TX_RATE_CTRL *)(pSib);

    /*don't update status at sounding frame. Since sounding frame is without steering
     , it is forced to use lower rate at sounding frame. And it is better not to use at
     rate control algorithm */
#ifdef ATH_SUPPORT_TxBF
    if (rcs[0].flags & ATH_RC_SOUNDING_FLAG) { 
        return;
    }
#endif
    ASSERT (rcs[0].tries != 0);

    /*
     * If the first rate is not the final index, there are intermediate rate failures
     * to be processed.
     */
    if (finalTSIdx != 0) {

    /* Process intermediate rates that failed.*/
        for (series = 0; series < finalTSIdx ; series++) {
            switch(sc->sc_curmode) {
		      case WIRELESS_MODE_11NA_HT20:
		      case WIRELESS_MODE_11NA_HT40PLUS:
		      case WIRELESS_MODE_11NA_HT40MINUS:
			    idx = translation_table_5g[rcs[series].rix];
                break;
		      case WIRELESS_MODE_11NG_HT20:
		      case WIRELESS_MODE_11NG_HT40PLUS:
		      case WIRELESS_MODE_11NG_HT40MINUS:
		        idx = translation_table_2g[rcs[series].rix];
                break;
		      default:
                idx = rcs[series].rix;
            }

            if (rcs[series].tries != 0) {
                flags = rcs[series].flags;
                /* If HT40 and we have switched mode from 40 to 20 => don't update */
                if ((flags & ATH_RC_CW40_FLAG) && (pRc->rcPhyMode != 
                    (flags & ATH_RC_CW40_FLAG)))
                {
                    return;
                }
#ifdef ATH_SUPPORT_TxBF
                if (flags & ATH_RC_TXBF_FLAG){
                    pSib->txbf_steered = 1;
                } else {
                    pSib->txbf_steered = 0;
                }                    
#endif
                if ((flags & ATH_RC_CW40_FLAG) && (flags & ATH_RC_SGI_FLAG)) {
                    rix = pRateTable->info[idx].htIndex;
                } else if (flags & ATH_RC_SGI_FLAG) {
                    rix = pRateTable->info[idx].sgiIndex;
                } else if (flags & ATH_RC_CW40_FLAG) {
                    rix = pRateTable->info[idx].cw40Index;
                } else {
                    rix = idx;   //pRateTable->info[idx].baseIndex;
                }
                /* FIXME:XXXX, too many args! */
#ifdef ATH_SUPPORT_VOWEXT
                rcUpdate_ht(sc, an, ac, rix, Xretries? 1 : 2, rcs[series].tries, rssiAck,
                        nFrames, nFrames, 0, short_retry_fail);
#else
                rcUpdate_ht(sc, an, ac, rix, Xretries? 1 : 2, rcs[series].tries, rssiAck,
                        nFrames, nFrames, short_retry_fail);
#endif
            }
        }
    } else {
        /*
         * Handle the special case of MIMO PS burst, where the second aggregate is sent
         *  out with only one rate and one try. Treating it as an excessive retry penalizes
         * the rate inordinately.
         */
        if (rcs[0].tries == 1 && Xretries == 1) {
            Xretries = 2;
        }
    }

#if ATH_SUPPORT_VOWEXT /* RCA */
    if (!short_retry_fail && (ATH_IS_VOWEXT_AGGRSIZE_ENABLED(sc)) &&
        ((nFrames+1) >= ((pRc->nAggrSize)>>1)))
    {
        pRc->nHeadFail = nHeadFail;
        pRc->nTailFail = nTailFail;
        pRc->nAggrSize = nFrames;
    }
#endif

    flags = rcs[series].flags;

	switch(sc->sc_curmode) {
	case WIRELESS_MODE_11NA_HT20:
	case WIRELESS_MODE_11NA_HT40PLUS:
	case WIRELESS_MODE_11NA_HT40MINUS:
		idx = translation_table_5g[rcs[series].rix];
		break;
	case WIRELESS_MODE_11NG_HT20:
	case WIRELESS_MODE_11NG_HT40PLUS:
	case WIRELESS_MODE_11NG_HT40MINUS:
		idx = translation_table_2g[rcs[series].rix];
		break;
	default:
		idx = rcs[series].rix;
	}
    /* If HT40 and we have switched mode from 40 to 20 => don't update */
    if ((flags & ATH_RC_CW40_FLAG) && (pRc->rcPhyMode != (flags & ATH_RC_CW40_FLAG))) {
     return;
    }
    if ((flags & ATH_RC_CW40_FLAG) && (flags & ATH_RC_SGI_FLAG)) {
        rix = pRateTable->info[idx].htIndex;
    } else if (flags & ATH_RC_SGI_FLAG) {
        rix = pRateTable->info[idx].sgiIndex;
    } else if (flags & ATH_RC_CW40_FLAG) {
        rix = pRateTable->info[idx].cw40Index;
    } else {
        rix = idx;  //pRateTable->info[idx].baseIndex;
    }

#if ATH_SUPPORT_HYFI_ENHANCEMENTS
    pRc->lastRateIndex = rix;
#endif
	
    /* FIXME:XXXX, too many args! */
#if ATH_SUPPORT_VOWEXT
    rcUpdate_ht(sc, an, ac, rix, Xretries, long_retry, rssiAck, nFrames, nBad, 0, short_retry_fail);
#else
    rcUpdate_ht(sc, an, ac, rix, Xretries, long_retry, rssiAck, nFrames, nBad, short_retry_fail);
#endif

    if (short_retry_fail) {
        pRc->consecRtsFailCount++;
    } else {
        pRc->consecRtsFailCount = 0;
    }

}

#if ATH_CCX
u_int8_t
rcRateValueToPer_11n(struct ath_softc *sc, struct ath_node *an, int txRateKbps)
{
    const RATE_TABLE_11N *pRateTable;
    u_int8_t             rate  = 0;
    u_int8_t             index = 0;
    struct atheros_node *oan = an->an_rc_node;
    struct TxRateCtrl_s *pRc = &oan->txRateCtrl;
    HAL_BOOL turboFlag = IS_CHAN_TURBO(&(sc->sc_curchan));

    pRateTable = sc->sc_rc->hwRateTable[sc->sc_curmode];

    if(pRateTable){
        while (rate < pRateTable->rateCount) {
            A_UINT32 valid;

            /* Check for single stream, to avoid dual stream rates for
             * single stream device */
            valid = getValidFlags(oan, pRateTable, rate);

            if (valid && txRateKbps == pRateTable->info[rate].rateKbps &&
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

#if UNIFIED_SMARTANTENNA
/* Copying the rate series from source to destination */
 void
rcRate_copyseries(struct ath_rc_series *series_d,struct ath_rc_series *series_s)
{
    series_d->rix             = series_s->rix;
    series_d->tries           = series_s->tries;
    series_d->flags           = series_s->flags;
    series_d->max4msframelen  = series_s->max4msframelen;
}


void
rcSmartAnt_SetRate_11n (struct ath_softc *sc, struct ath_node *an,
                      struct ath_rc_series series[], u_int8_t *rate_array_index, u_int32_t *antenna_array, u_int32_t rc_flags)

{
    struct atheros_softc  *asc = (struct atheros_softc*)sc->sc_rc;
    const RATE_TABLE_11N        *pRateTable = (const RATE_TABLE_11N *)asc->hwRateTable[sc->sc_curmode];
    struct atheros_node   *asn = ATH_NODE_ATHEROS(an);

    if (sc->sc_smart_ant_mode) { /* parallel mode */
        /* copy rates to one index down */
        rcRate_copyseries(&series[3],&series[2]);
        rcRate_copyseries(&series[2],&series[1]);
        rcRate_copyseries(&series[1],&series[0]);

        rcRateSetseries(&series[0], pRateTable, &series[0], series[0].tries, rate_array_index[0], FALSE, asn, sc->sc_tx_numchains);

#ifdef ATH_SUPPORT_TxBF
        series[0].flags &= ~(ATH_RC_TXBF_FLAG);
#endif        
        series[0].flags |= ATH_RC_TRAINING_FLAG;
        series[0].flags |= ATH_RC_RTSCTS_FLAG;

    } else {
        /* Set the choosen rate index */
        rcRateSetseries(&series[0], pRateTable, &series[0], series[0].tries, rate_array_index[0], FALSE, asn, sc->sc_tx_numchains);
        rcRateSetseries(&series[0], pRateTable, &series[1], series[1].tries, rate_array_index[1], FALSE, asn, sc->sc_tx_numchains);
        rcRateSetseries(&series[0], pRateTable, &series[2], series[2].tries, rate_array_index[2], FALSE, asn, sc->sc_tx_numchains);
        rcRateSetseries(&series[0], pRateTable, &series[3], series[3].tries, rate_array_index[3], FALSE, asn, sc->sc_tx_numchains);

        series[0].flags |= ATH_RC_TRAINING_FLAG;
        series[1].flags |= ATH_RC_TRAINING_FLAG;
        series[2].flags |= ATH_RC_TRAINING_FLAG;
        series[3].flags |= ATH_RC_TRAINING_FLAG;
    }

}
#endif
