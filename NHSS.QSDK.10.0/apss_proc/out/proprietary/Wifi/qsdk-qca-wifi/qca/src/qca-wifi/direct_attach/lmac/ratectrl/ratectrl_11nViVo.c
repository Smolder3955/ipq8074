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
 * This file contains the data structures and routines for transmit rate
 * control operating on the linearized rates of different phys
 *
 */


#include <osdep.h>

#include "ah.h"
#include "ratectrl.h"
#include "ratectrl11n.h"

#include "ath_internal.h" /* ath_node */

#ifndef REMOVE_PKT_LOG
#include "pktlog_rc.h"
extern struct ath_pktlog_rcfuncs *g_pktlog_rcfuncs;
#endif

#if ATH_SUPPORT_IQUE

#define MULTI_RATE_RETRY_ENABLE 1

#define MIN_PROBE_INTERVAL 2

#if ATH_SUPPORT_VOWEXT /* for RCA */
/* Following is the table of constants required to choose the aggregate size of
 * the next packet. Condition required to drop the aggr size: 
 * 		goodput of full packet < goodput of half packet
 * where,goodput of the full packet =MAC throughput of a packet of size nFrames
 *              *( 1- ( #head error + #tail error)/nFrames)
 * goodput of first half of the packet=MAC throughput of a packet of size 
 *              nFrames_by_2 *( 1- #head error/ (nFrames/2))
 * #head error : # errors in first half of the pkt, 
 * #tail errors: #error in second half of the packet
 * after simplification, (#tail errors * 100) > 
 *              ( thp_ratio*nFrames + ( 100- 2*thp_ratio)*#head errors)
 * where thp_ratio is a function of rate and aggr size.
 * thp_ratio = 100 - 100*MAC_thp_half_pkt/MAC_thp_full packet. 
 * Given below is the thp_ratio for different rates and aggregate sizes.
 * Each row corresponds to a (HT)rate indexed by rateCode of the rate. First 
 * column is for aggr size 1 to 8, second column is for aggr size 9 to 16 and 
 * third column indicates the value for aggr size greater than 16.
 */
#define THP_RATIO_MAX_INDEX 23
static A_UINT8 thp_ratio[24][3] = { 
{1,     1,      1}, // MCS0
{5,	    1,	    1}, // MCS1
{5,     1,      1}, // MCS2
{5,	    5,	    1}, // MCS3
{10,    5,      5}, // MCS4
{10,    5,      5}, // MCS5
{10,    10,     5}, // MCS6
{10,    10,     5}, // MCS7
{5,     1,      1}, // MCS8
{5,     5,      1}, // MCS9
{10,    5,      5}, // MCS10
{10,    5,      5}, // MCS11
{15,    10,     10},// MCS12
{15,    15,     10},// MCS13
{15,    15,     10},// MCS14
{20,    15,     10},// MCS15
{5,     1,      1}, // MCS16
{10,    5,      5}, // MCS17
{10,    10,     5}, // MCS18
{15,    10,     10},// MCS19
{15,    15,     10},// MCS20
{30,    25,     13},// MCS21
{30,    25,     14},// MCS22
{30,    25,     15},// MCS23
};
#endif

/* Dump all the valid rate index */

#if 0
static void
dump_valid_rate_index(RATE_TABLE_11N *pRateTable, A_UINT8 *validRateIndex)
{
    A_UINT32 i=0;

    printk("Valid Rate Table:-\n");
    for(i=0; i<MAX_TX_RATE_TBL; i++)
    printk(" Index:%d, value:%d, code:%x, rate:%d, flag:%x\n", i, (int)validRateIndex[i],
           pRateTable->info[(int)validRateIndex[i]].rateCode,
           pRateTable->info[(int)validRateIndex[i]].rateKbps,
           WLAN_RC_PHY_40(pRateTable->info[(int)validRateIndex[i]].phy));
}
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
            if (*pNextIndex > MAX_TX_RATE_TBL) {
                pNextIndex = 0;
                return FALSE;
            }
            else
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
    A_INT8    i;

    for (i = 1; i < pRc->maxValidRate ; i++) {
#ifdef ATH_SUPPORT_TxBF
        if (txbfsounding){
            if ((pRc->SoundRateIndex[i] == curValidTxRate)
                && (pRateTable->info[curValidTxRate].rateCode != MCS0))  /* no last lower rate than mcs0 at sounding*/
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
    TX_RATE_CTRL *pRc  = (TX_RATE_CTRL *)(&pSib->txRateCtrlViVo);

    /* Use intersection of working rates and valid rates */
    for (i = 0; i < pRateSet->rs_nrates; i++) {
        for (j = 0; j < pRateTable->rateCount; j++) {
            A_UINT32 phy = pRateTable->info[j].phy;
            A_UINT32 valid;

            valid = getValidFlags(pSib, pRateTable, j);

            /* We allow a rate only if its valid and the capflag matches one of
             * the validity (TRUE/TRUE_20/TRUE_40) flags
             */
            if (((pRateSet->rs_rates[i] & 0x7F) == (pRateTable->info[j].dot11Rate & 0x7F))
                && ((valid & WLAN_RC_CAP_MODE(capflag)) ==
                    WLAN_RC_CAP_MODE(capflag)) && !WLAN_RC_PHY_HT(phy))
            {

                if (!rcIsValidPhyRate(phy, capflag, FALSE))
                    continue;

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
                     A_UINT8 *pMcsSet, A_UINT32 capflag,
                     const MAX_RATES *maxRates)
{
    A_UINT8     i, j, hi = 0;
    TX_RATE_CTRL *pRc  = (TX_RATE_CTRL *)(&pSib->txRateCtrlViVo);
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
    RATE_TABLE_11N            *pRateTable;
    struct atheros_node	      *pSib	  = ATH_NODE_ATHEROS(an);
    struct atheros_softc      *asc 	  = (struct atheros_softc*)sc->sc_rc;
    struct ieee80211_rateset  *pRateSet = negotiated_rates;
    A_UINT8 *phtMcs = (A_UINT8 *)negotiated_htrates;
    TX_RATE_CTRL *pRc  = (TX_RATE_CTRL *)(&pSib->txRateCtrlViVo);
    A_UINT8 i, j, k, hi = 0, htHi = 0;
    
#ifdef ATH_SUPPORT_TxBF    
    const u_int8_t  (*sounding_swap_table);
#endif

    pRateTable = (RATE_TABLE_11N*)asc->hwRateTable[sc->sc_curmode];

    /* Initial rate table size. Will change depending on the working rate set */
    pRc->rateTableSize = MAX_TX_RATE_TBL;

#if ATH_SUPPORT_VOWEXT /* for RCA */
    pRc->nHeadFail = 0;
    pRc->nTailFail = 0;
    pRc->nAggrSize = 0;
    pRc->aggrLimit = MAX_AGGR_LIMIT; 
#endif

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
            htHi = rcSibSetValidHtRates(pSib, pRateTable, phtMcs, capflag,
                                        maxRates);
        }
        hi = A_MAX(hi, htHi);
    }

    pRc->rateTableSize = hi + 1;
    ASSERT(pRc->rateTableSize <= MAX_TX_RATE_TBL);

    for (i = 0, k = 0; i < WLAN_RC_PHY_MAX; i++) {
        for (j = 0; j < pRc->validPhyRateCount[i]; j++) {
            pRc->validRateIndex[k++] = pRc->validPhyRateIndex[i][j];
        }

        if (!rcIsValidPhyRate(i, pRateTable->initialRateMax, TRUE) || !pRc->validPhyRateCount[i])
           continue;

        //pRc->rateMaxPhy = pRc->validPhyRateIndex[i][j-1];
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
	
	/* Modify for static analysis, protect if k < 4 */
	if (k >= 4) {
		pRc->rateMaxPhy = pRc->validRateIndex[k-4];
	} else {
		pRc->rateMaxPhy = pRc->validRateIndex[0];
	}
    pRc->probeInterval = sc->sc_rc_params[1].probe_interval;

#ifdef ATH_SUPPORT_TxBF
    /*  Set up sounding rate table. Sounding rate table is used to swap rate while TxBF sounding
     **/ 
    //DPRINTF(sc, ATH_DEBUG_ANY,"==>%s:set up vivo rate table:\n",__func__);
    for (i=0;i<pRc->maxValidRate;i++)
    {
        //DPRINTF(sc, ATH_DEBUG_ANY," %d:MCS %x,",pRc->validRateIndex[i],pRateTable->info[pRc->validRateIndex[i]].rateCode);
    }         
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
                    
                    if (ath_hal_support_ts_sounding(ah))
                        swapratecode = ratecode;                
                    else
                        swapratecode = TS_SOUNDING_SWAP_RATE;    // force swap (two stream rate)
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
           //printk("vivo org idx %d,codate %x; swap idx %d, coderate %x \n",pRc->validRateIndex[i],pRateTable->info[pRc->validRateIndex[i]].rateCode,
                   // pRc->SoundRateIndex[i],pRateTable->info[pRc->SoundRateIndex[i]].rateCode);
        }
    }     
#endif
    //dump_valid_rate_index(pRateTable, pRc->validRateIndex);
    //printk("RateTable:%d, maxvalidrate:%d, ratemax:%d\n", pRc->rateTableSize,k,pRc->rateMaxPhy);

}

void
rcSibUpdate_11nViVo( struct ath_softc *sc, struct ath_node *pSib,
                      A_UINT32 capflag, int keepState,
                      struct ieee80211_rateset *negotiated_rates,
                      struct ieee80211_rateset *negotiated_htrates,
                      const MAX_RATES *maxRates)
{
    rcSibUpdate_ht(sc, pSib, capflag,
                   keepState, negotiated_rates, negotiated_htrates, maxRates);
}


static A_UINT8
rcRateFind_ht(struct ath_softc *sc, struct ath_node *an, struct atheros_node *pSib, A_UINT8 ac,
		  const RATE_TABLE_11N *pRateTable, int probeAllowed, int *isProbing, int isretry)
{
    A_UINT32      bestThruput, thisThruput;
    A_UINT32      nowMsec;
    A_UINT8       rate, nextRate, thisRate, minRate, prevRate;
    A_INT8        index, rc_index;
    TX_RATE_CTRL  *pRc;
#ifdef ATH_SUPPORT_TxBF
    A_INT8        rtidx=0;
#endif
    ASSERT (pSib && pSib->an);
    pRc  = (TX_RATE_CTRL *)(&pSib->txRateCtrlViVo);

    *isProbing = FALSE;

    nowMsec = A_MS_TICKGET();

    pRc->rptGoodput = 0;
    bestThruput = 0;
    rate = 0;
    minRate = 0;
    /*
     * Try the higher rate first. It will reduce memory moving time
     * if we have very good channel characteristics.
     */

    for (index = pRc->maxValidRate-1; index >= 0 ; index--) {
        A_UINT8 per;

        thisRate = pRc->validRateIndex[index];

        /* since higher rate index might have lower data rate, And we want to 
         *  sort out the rate index with higher data rate than pRc->rateMaxPhy
         *  , it should use pRateTable->info[rate].userRateKbps instead of use 
         *  rate index itself directly.
         */

        /*if (thisRate > pRc->rateMaxPhy) {
            continue;
        }*/
        if (pRateTable->info[thisRate].userRateKbps > 
            pRateTable->info[pRc->rateMaxPhy].userRateKbps) {
            continue;
        }

        if (pRateTable->info[thisRate].userRateKbps >
            sc->sc_ac_params[ac].min_kbps)
        {
            minRate = thisRate;
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
        per = pRc->state[thisRate].per;
#if ATH_SUPPORT_VOWEXT /* RCA */
        /* 12% Per threshold will bias the algorithm to choose a higher rate. 
         * It is possible that even if the current rate has Per of around 20% 
         * and lower rate has better goodput, it will operate at higher rate 
         * only. So, for better reliability, disabling this feature for video.
         * Also if the Per at lower rate later, increases due to collision, 
         * it will come back to the higher rate.
         */
        if ( (!ATH_IS_VOWEXT_RCA_ENABLED(sc)) || \
			(IS_LEGACY_RATE(pRateTable->info[thisRate].rateCode)) ){
        	if (per < ATH_RATE_TGT_PER_PCT_VIVO) {
            		per = ATH_RATE_TGT_PER_PCT_VIVO;
        	}	
        }
#else
        if (per < ATH_RATE_TGT_PER_PCT_VIVO) {
            per = ATH_RATE_TGT_PER_PCT_VIVO;
        }
#endif

        thisThruput = pRateTable->info[thisRate].userRateKbps *
                      (100 - per);

        if (bestThruput <= thisThruput) {
            bestThruput = thisThruput;
            rate = thisRate;
#ifdef ATH_SUPPORT_TxBF
            rtidx = index;
#endif
        }
    }
    
    pRc->rptGoodput = bestThruput;
    if (minRate > rate) {
        rate = minRate;
    }

	/*
	 * For the headline block removal feature, if 
	 * the minRate for a node is MCS2, and PER >= 55 
	 * (for video/voice), then the data to be 
	 * transmitted to this node maight block 
	 * xmission to other nodes with higher rates 
	 * and lower PER. Then the headline block 
	 * removal state machine will be triggered, 
	 * by blocking further xmission to this node 
	 * untill the probing to this node succeedes.
	 * Probing means checking the status of the 
	 * link to the blocked node by sending out QoS 
	 * Null frames. Either N continuous DoSNull 
	 * frames been sent successfully, or the PER 
	 * drops below 35, the "probing" succeeds, and the 
	 * transmission to this node resumes.
	 */
	/* Modify for static analysis, prevent pSib->an is NULL */
	if (pSib->an) {
		pSib->an->an_minRate[ac] = minRate;
	}
	if (sc->sc_hbr_params[ac].hbr_enable && 
		rate == minRate &&
		pRc->state[rate].per >= sc->sc_hbr_per_high &&
        sc->sc_ieee_ops->hbr_settrigger && pSib->an) /* PER default as 25 by default */
	{
		sc->sc_ieee_ops->hbr_settrigger(pSib->an->an_node, HBR_EVENT_BACK);
	}
#if 0
    /* if we are retrying for more than half the number
     * of max retries, use the min rate for the next retry
     */
    if (isretry)
        rate = pRc->validRateIndex[minIndex];
#endif

    /*
     * If we are not picking rateMax for more than 10ms,
     * we should reduce rateMax.
     */

    if (pRateTable->info[rate].userRateKbps  < pRateTable->info[pRc->rateMaxPhy].userRateKbps) {
        if ((nowMsec - pRc->rateMaxLastUsed) > 10) {
            pRc->rateMaxPhy = rate;
            pRc->rateMaxLastUsed = nowMsec;
        }
    } else {
        pRc->rateMaxLastUsed = nowMsec;
    }
    if ( (pRateTable->info[rate].userRateKbps >= pRateTable->info[pRc->rateMaxPhy].userRateKbps)
         && probeAllowed && (!(ATH_TXBF_SOUNDING(sc))) ) {
        /*
         * attempt to probe only if this rate is lower than the max valid rate
         */
        if ( !rcGetNextLowerValidTxRate(pRateTable, pRc, rate, &prevRate, 0) ){
            prevRate=rate;
        }
        if ((pRateTable->info[rate].userRateKbps < 
            pRateTable->info[pRc->validRateIndex[pRc->maxValidRate-1]].userRateKbps)
            && ( (pRc->state[rate].per <= ATH_RATE_TGT_PER_PCT_VIVO) || 
               (pRc->state[prevRate].per >= ATH_RATE_TGT_PER_PCT_VIVO ) ) ){ 
            /* Probe the next allowed phy state only when good enough.
             * Excluding the case, when all the rates have high PER 
             */
            if (rcGetNextValidTxRate(pRateTable, pRc, rate, &nextRate) &&
                (nowMsec - pRc->probeTime > pRc->probeInterval) &&
                (pRc->hwMaxRetryPktCnt >= 1)){
                rate                  = nextRate;
                pRc->probeRate        = rate;
                pRc->probeTime        = nowMsec;
                pRc->hwMaxRetryPktCnt = 0;
                *isProbing            = TRUE;         
            }
        } else {
            /* Reset Probe interval to configured value */
            rc_index = (ac >= WME_AC_VI) ? 1 : 0;
            pRc->probeInterval = sc->sc_rc_params[rc_index].probe_interval;
        }
    }

    /*
     * Make sure rate is not higher than the allowed maximum.
     */
    ASSERT (rate <= (pRc->rateTableSize - 1));

#ifndef REMOVE_PKT_LOG
    if (!sc->sc_txaggr || (sc->sc_txaggr && sc->sc_log_rcfind))
    {
        struct log_rcfind log_data = {0};
        log_data.rc = pRc;
        log_data.rateCode = pRateTable->info[rate].rateCode;
        log_data.rate = rate;
        log_data.rssiReduce = 0;
        log_data.misc[0] = pRc->state[rate].per;
        log_data.misc[1] = 0;
        log_data.ac = ac;
        log_data.isProbing = *isProbing;
        ath_log_rcfind(sc, &log_data, 0);
        sc->sc_log_rcfind = 0;
    }
#endif

    /* record selected rate, which is used to decide if we want to do fast
     * frame
     */
    if (!(*isProbing)) {
        pSib->lastRateKbps = pRateTable->info[rate].rateKbps;
    }

     ASSERT(VALID_RATE(pRateTable,pSib,rate));

#ifdef ATH_SUPPORT_TxBF
    if ( sc->sc_txbfsounding ) {    // swap rate when TxBF sounding
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
                && (asn->lastTxmcs0cnt > MCS0_80_PERCENT)
        && !(asc->sc_txbf_disable_flag[pRateTable->info[rix].baseIndex][txchains - 1])))
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
                A_UINT8 ac, const RATE_TABLE_11N *pRateTable,
                A_UINT8 rix, A_UINT16 stepDown, A_UINT16 minRate)
{
    A_UINT32 j;
    A_UINT8 nextIndex;
    struct atheros_node *pSib;
    TX_RATE_CTRL *pRc;
    A_UINT8                 opt;

#ifdef ATH_SUPPORT_TxBF
    opt = sc->sc_txbfsounding;
#else
    opt = 0;
#endif

    pSib = ATH_NODE_ATHEROS(an);
    pRc = (TX_RATE_CTRL *)(&pSib->txRateCtrlViVo);

	if (minRate) {
		for (j = RATE_TABLE_SIZE_11N; j > 0; j-- ) {
			if (rcGetNextLowerValidTxRate(pRateTable, pRc, rix, &nextIndex,opt)) {
				rix = nextIndex;
			} else {
				break;
			}
		}
	} else {
		for (j = stepDown; j > 0; j-- ) {
			if (rcGetNextLowerValidTxRate(pRateTable, pRc, rix, &nextIndex,opt)) {
                if (pRateTable->info[nextIndex].userRateKbps > sc->sc_ac_params[ac].min_kbps) {
				    rix = nextIndex;
                } else {
				    break;
                }
			} else {
				break;
			}
		}
	}
#ifdef ATH_SUPPORT_TxBF
    if (sc->sc_txbfcalibrating) {// calibration frame
        if ((sc->sc_curmode == WIRELESS_MODE_11NG_HT20) || (sc->sc_curmode == WIRELESS_MODE_11NG_HT40PLUS) 
            || (sc->sc_curmode == WIRELESS_MODE_11NG_HT40MINUS) ) // g-band rate
            rix = 0xc;
        else     
            rix = 0x8;         // force mcs0 when not HT rate.
    }
#endif
	return rix;
}

void
rcRateFind_11nViVo(struct ath_softc *sc, struct ath_node *an, A_UINT8 ac,
              int numTries, int numRates, unsigned int rcflag,
              struct ath_rc_series series[], int *isProbe, int isretry)
{
    A_UINT8 i=0;
    A_UINT8 tryPerRate = 0;
    A_UINT8 rix, nrix, use_rts = 0;
    struct atheros_softc  *asc = (struct atheros_softc*)sc->sc_rc;
    RATE_TABLE_11N        *pRateTable = (RATE_TABLE_11N *)asc->hwRateTable[sc->sc_curmode];
    struct atheros_node   *asn = ATH_NODE_ATHEROS(an);
    TX_RATE_CTRL  *pRc;
#if ATH_SUPPORT_VOWEXT
    A_UINT8  aggr_limit = MAX_AGGR_LIMIT;
    A_UINT8 nHeadFail, nTailFail, nAggrSize,last_aggrlimit, lastRate;
    A_UINT8 isLegacy = 0;
    A_UINT32 nowMsec; 
#endif

    ASSERT (asn);

    asn->an = an;

    rix = rcRateFind_ht(sc, an, asn, ac, pRateTable, (rcflag & ATH_RC_PROBE_ALLOWED) ? 1 : 0, isProbe, isretry);
    nrix = rix;

    use_rts = sc->sc_ac_params[ac].use_rts;

    pRc  = (TX_RATE_CTRL *)(&asn->txRateCtrlViVo);
#if ATH_SUPPORT_VOWEXT /* RCA */
    if (pRateTable->info[rix].rateCode < 0x80) {
        isLegacy = 1;
    }
    nHeadFail = pRc->nHeadFail;
    nTailFail = pRc->nTailFail;
    nAggrSize = pRc->nAggrSize;
    last_aggrlimit = pRc->aggrLimit;
    lastRate = pRc->lastRate;
    if ((ATH_IS_VOWEXT_RCA_ENABLED(sc)) && (!(*isProbe)) && (!isLegacy) &&
             ( !(ATH_TXBF_SOUNDING(sc)) )) { 
        A_UINT8 nextRate;
	/* constants required to compare the goodput of full packet and first 
	 * half of the packet for the given rate and aggr size. 
	 */
        if (IF_ABOVE_MIN_RCARATE(pRateTable->info[lastRate].rateCode)) {
                 
            A_UINT8 column_ind = (nAggrSize > 16)? 2:(nAggrSize > 8) ? 1 : 0; 
            A_UINT8 row_ind = pRateTable->info[lastRate].rateCode - 0x80 ;
            A_UINT32 ratio =0;
            ratio = thp_ratio [ MIN(row_ind,THP_RATIO_MAX_INDEX )][column_ind];
            nowMsec = A_MS_TICKGET(); 

            if (((nHeadFail+nTailFail)*2 >= nAggrSize) ){ 
	            /* if error >= 50% */
                pRc->badPerTime = nowMsec; 
                an->throttle = 10;
                /* reduce the rate */
                if( (rix >= lastRate)) { 
                    if(rcGetNextLowerValidTxRate(pRateTable, pRc, (A_UINT8) lastRate, &nextRate, 0)){
                        rix = nextRate;
                    }
                } 
                aggr_limit = pRc->state[rix].maxAggrSize;
	            /*reducing the max size */
                pRc->state[lastRate].maxAggrSize = MAX( (pRc->state[lastRate].maxAggrSize - \
		               (pRc->state[lastRate].maxAggrSize >> sc->excretry_aggr_red_factor)) ,MIN_AGGR_LIMIT ); 
                pRc->nHeadFail = pRc->nTailFail = 0;
            } else if (  (nTailFail >= 4) && ((nTailFail*100) >= \
	            ((ratio)*nAggrSize + (100 - 2*ratio)*(MAX(nHeadFail, 1 )))) ) {  
	            if (rix > lastRate ) {
                    rix = lastRate;
                }
	            /* reduce aggr size by half */
                if (nAggrSize>>1) { 
                    aggr_limit = (nAggrSize>>1) + 1;
                } else {
                    aggr_limit = nAggrSize;
                }
 		        pRc->badPerTime = nowMsec; 
                an->throttle = 10;
                pRc->state[lastRate].maxAggrSize = MAX(pRc->state[lastRate].maxAggrSize - \
						   sc->badper_aggr_red_factor , MIN_AGGR_LIMIT ); 
                
                pRc->nHeadFail = pRc->nTailFail = 0;
            } else if ((nowMsec - pRc->badPerTime) < CONSTANT_RATE_DURATION_MSEC) { 
	            /* and if ukbps is more than ingress rate - not implemented */
	            /* do not increase the rate for next 10 msec after hitting the
	             * bad per condition 
                 */
                if ( rix > lastRate) {
                    rix = lastRate;
                }
                aggr_limit = last_aggrlimit + 8; 

            } else if ( (pRc->aggrLimit <= 4) && (rix > lastRate) ){
	            /* don't increase the rate. First increase the aggr size and then rate */
                rix = lastRate; 
                aggr_limit = last_aggrlimit + 8; 
            } else if (rix > lastRate){
	            aggr_limit = MIN (16,last_aggrlimit ); 
#if !WAR_TX_UNDERRUN                
    	        use_rts = 1; 
#endif                
            } else if (rix == lastRate){
                aggr_limit = last_aggrlimit + 8; 
	            /*define stability point */
            } else {
                aggr_limit = last_aggrlimit;
            }
        }
	/* minimum aggr limit */
        if (aggr_limit <3 ) {
            aggr_limit = 3;
        } 
	/* max aggr limit */
        aggr_limit = MIN( pRc->state[rix].maxAggrSize, aggr_limit ); 
        if ( an->throttle > 0 ) { 
#if !WAR_TX_UNDERRUN            
            use_rts = 1;
#endif            
        }
    }
    nrix = rix;
#endif

    if ((rcflag & ATH_RC_PROBE_ALLOWED) && (*isProbe)) {
        /* set one try for probe rates. For the probes don't enable rts */

        rcRateSetseries(&series[0], pRateTable, &series[i++], 1, nrix, (use_rts) ? TRUE:FALSE, asn,sc->sc_tx_numchains);

        tryPerRate = (numTries/numRates);
        /* Get the next tried/allowed rate. No RTS for the next series
         * after the probe rate
         */
        nrix = rcRateGetIndex( sc, an, ac, pRateTable, nrix, 1, FALSE);

        rcRateSetseries(&series[0], pRateTable, &series[i++], tryPerRate, nrix, (use_rts) ? TRUE:FALSE, asn, sc->sc_tx_numchains);
        /* Enable RTS for probe rate if user say so */
        if (pRateTable->info[nrix].rateKbps <= sc->sc_limitrate)
        {
            if (sc->sc_en_rts_cts)
                series[0].flags |= ATH_RC_RTSCTS_FLAG ;
        }	
    } else {
        tryPerRate = (numTries/numRates);
        /* Set the choosen rate. No RTS for first series entry. */
        rcRateSetseries(&series[0], pRateTable, &series[i++], tryPerRate, nrix, (use_rts) ? TRUE:FALSE, asn, sc->sc_tx_numchains);
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

        nrix = rcRateGetIndex(sc, an, ac, pRateTable, nrix, 1, minRate);
        /* All other rates in the series have RTS enabled */

        rcRateSetseries(&series[0], pRateTable, &series[i], tryNum, nrix, TRUE, asn, sc->sc_tx_numchains);

    }

#if ATH_SUPPORT_VOWEXT /* RCA */
#ifdef ATH_SUPPORT_TxBF
    if ((!(*isProbe)) && (ATH_IS_VOWEXT_RCA_ENABLED(sc)) && (!sc->sc_txbfsounding) ) {
#else
    if ((!(*isProbe)) && (ATH_IS_VOWEXT_RCA_ENABLED(sc))) {
#endif
	    if(an->throttle) {
            an->throttle--;
	    }
	    pRc->aggrLimit = aggr_limit;  
   }
#endif

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


/*
 * Calculate PER updates
 * new_PER = 7/8*old_PER + 1/8*(currentPER)
 */
static inline A_UINT8
perCal(A_UINT8 per, int retries, A_UINT16 nFrames, A_UINT16 nBad, int if_usedrts)  
{
    A_UINT8 newPer;
    int i;

    newPer = per; 

    /* Update per for all intermediate failure attempts. 
     * If rts is not used, reduce the per weightage.
     */
    for (i = 0; i < retries; i++) {
        if (!if_usedrts ) {
            newPer = ((7*newPer) + 50) >> 3;
            break;
        }
        else{
            newPer = ((7*newPer) + 100) >> 3;
        } 

    }

    /* Update per for the final successful attempt, if any */
    if (nBad < nFrames) {
        newPer = ((7*newPer) + (100 * nBad)/nFrames) >> 3;
    }
    else if ( (!nFrames) && (!retries) ) {
        newPer = (7*newPer) >> 3;
    }

    return newPer;
}
static void
rcUpdate_ht(struct ath_softc *sc, struct ath_node *an, A_UINT8 ac, int txRate,
            int Xretries, int retries, A_RSSI rssiAck, A_UINT32 nowMsec,
            A_UINT16 nFrames, A_UINT16 nBad, int isProbe, int short_retry_fail, int if_usedrts)
{
    TX_RATE_CTRL *pRc;
    A_UINT8      lastPer, lastminRatePer, rc_index;
    int          rate;
    struct atheros_node *pSib;
    struct atheros_softc *asc;
    RATE_TABLE_11N *pRateTable;

    pSib = ATH_NODE_ATHEROS(an);
    asc = (struct atheros_softc*)sc->sc_rc;
    rc_index = (ac >= WME_AC_VI) ? 1 : 0;

    if (!pSib || !asc) {
        return;
    }

    pRc = (TX_RATE_CTRL *)(&pSib->txRateCtrlViVo);
    pRateTable = (RATE_TABLE_11N *)asc->hwRateTable[sc->sc_curmode];

    ASSERT(txRate >= 0);

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
        goto out;
    }
	/* To compensate for some imbalance between ctrl and ext. channel */

    if (WLAN_RC_PHY_40(pRateTable->info[txRate].phy))
		rssiAck = rssiAck < 3? 0: rssiAck - 3;

    lastPer = pRc->state[txRate].per;
    lastminRatePer = pRc->state[an->an_minRate[ac]].per;

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

#if ATH_SUPPORT_VOWEXT /* RCA */ 
    if (ATH_IS_VOWEXT_RCA_ENABLED(sc)){
        if ( (nFrames > 3) ){
		/* If the nFrames is less than 3, then we don't update RCA stats ( nHeadFail,nTailFail etc) 
		 * and hence the aggr table for this packet. So, we should update PER for those packets. 
         */
            if ( nBad < nFrames){ 
			/* if transmission is successful, update the per. */ 
                pRc->state[txRate].per = perCal(pRc->state[txRate].per, 0 , nFrames, nBad, if_usedrts);
            }else if ( (if_usedrts) || (pRc->state[txRate].maxAggrSize <= sc->per_aggr_thresh) ){ 
			/* do not update the per for all excessive retries, as sometimes 
             * excessive retry doesn't mean 100% per 
             */
                pRc->state[txRate].per = perCal(pRc->state[txRate].per, retries, nFrames, nBad, if_usedrts);
            }
            if ( retries && (!if_usedrts) ) { 
                an->throttle = MAX (5, an->throttle) ;
            }

        }
        else if (isProbe ) {  
            if ( (nFrames == nBad) || retries || (txRate == pRc->probeRate) ){  
			/* for probe packets update the per only if the packet fails.*/
                pRc->state[txRate].per = perCal(pRc->state[txRate].per, retries, nFrames, nBad, if_usedrts);
            }
        }else {
            pRc->state[txRate].per = perCal(pRc->state[txRate].per, retries, nFrames, nBad, if_usedrts);
        }
    }
    else {
        pRc->state[txRate].per = perCal(pRc->state[txRate].per, retries, nFrames, nBad, 1);
    }
#else
    pRc->state[txRate].per = perCal(pRc->state[txRate].per, retries, nFrames, nBad, 1);
#endif		

    if (!Xretries) {
        /*
         * If we got at most one retry then increase the max rate if
         * this was a probe.  Otherwise, ignore the probe.
         */
        if (pRc->probeRate && pRc->probeRate == txRate) {
            /*
             * Since we probed with just a single attempt,
             * any retries means the probe failed.  Also,
             * if the attempt worked, but more than half
             * the subframes were bad then also consider
             * the probe a failure.
             */
            if (retries == 0 && nBad <= nFrames/2) {
                A_UINT8 prevRate;
                A_UINT32 interval;

                pRc->rateMaxPhy = pRc->probeRate;

                if (rcGetNextLowerValidTxRate(pRateTable, pRc, (A_UINT8)pRc->probeRate, &prevRate,0)) {
                        if (pRc->state[pRc->probeRate].per < pRc->state[prevRate].per) {
                            pRc->state[pRc->probeRate].per = pRc->state[prevRate].per;
                        }
                        else if (pRc->state[pRc->probeRate].per > 30) {
                            pRc->state[pRc->probeRate].per = MAX (30, pRc->state[prevRate].per );
                        }
                }

                /*
                 * Since this probe succeeded, we allow the next probe
                 * twice as soon.  This allows the maxRate to move up
                 * faster if the probes are succesful. Dont let the
                 * probe interval fall below MIN_PROBE_INTERVAL ms.
                 */
                interval = pRc->probeInterval/2;
                pRc->probeInterval = (interval > MIN_PROBE_INTERVAL) ? interval : MIN_PROBE_INTERVAL;
                pRc->probeTime = nowMsec;
            } else {
                /* Reset Probe interval to configured value  on probe failure */
                pRc->probeInterval = sc->sc_rc_params[rc_index].probe_interval;
            }
        }

        if (retries > 0) {
            /*
             * Don't update anything.  We don't know if this was because
             * of collisions or poor signal.
             *
             */
            pRc->hwMaxRetryPktCnt = 0;
        } else {
            /* XXX */
            if (txRate == pRc->rateMaxPhy && pRc->hwMaxRetryPktCnt < 255) {

		        if ( (nFrames > 1) && ( nBad > nFrames/2) ){ 
                    pRc->hwMaxRetryPktCnt = 0;
                    /* Reset Probe interval on probe failure */
                    pRc->probeInterval = sc->sc_rc_params[rc_index].probe_interval;
              	} else {
		            pRc->hwMaxRetryPktCnt++;
		        }
            }
        }
    }

    if (pRc->probeRate == txRate) {
        pRc->probeRate = 0;
    }

	/* For all cases */

    /*
     * If this rate looks bad (high PER) then stop using it for
     * a while (except if we are probing).
     */
    if (pRc->state[txRate].per >= sc->sc_rc_params[rc_index].per_threshold && txRate > 0 &&
        pRateTable->info[txRate].rateKbps <=
				pRateTable->info[pRc->rateMaxPhy].rateKbps)
    {
        A_UINT8 nextRate;

        if (rcGetNextLowerValidTxRate(pRateTable, pRc, (A_UINT8) txRate, &nextRate,0)) {
            if (pRateTable->info[nextRate].userRateKbps > sc->sc_ac_params[ac].min_kbps) {
                pRc->rateMaxPhy = nextRate;
            }
        }

        /* Don't probe for a little while. */
        pRc->probeTime = nowMsec;
    }
    
    /* When at max allow rate, if current rate expected throuhgput is lower 
      then next lower rate expected throughput ,then drop one rate*/    
    if ( (pRc->rateMaxPhy == txRate) && (!isProbe)){        
        A_UINT8 nextrate = 0;
        if (rcGetNextLowerValidTxRate(pRateTable, pRc, (A_UINT8) txRate, &nextrate,0)){
            if ( (pRc->state[nextrate].per <  ATH_RATE_TGT_PER_PCT_VIVO ) &&
                 ((pRateTable->info[txRate].userRateKbps *(100 - pRc->state[txRate].per))
                < (pRateTable->info[nextrate].userRateKbps *(100 -ATH_RATE_TGT_PER_PCT_VIVO)))
                && (pRateTable->info[nextrate].userRateKbps > sc->sc_ac_params[ac].min_kbps)) {
                pRc->rateMaxPhy = nextrate;
            }
        }             
    }

    /* Make sure the rates below this have lower PER */
    /* Monotonicity is kept only for rates below the current rate. */
    /*if (pRc->state[txRate].per < lastPer) {
        for (rate = txRate - 1; rate >= 0; rate--) {
            if (pRateTable->info[rate].phy != pRateTable->info[txRate].phy) {
                break;
            }

            if (pRc->state[rate].per > pRc->state[rate+1].per) {
                pRc->state[rate].per = pRc->state[rate+1].per;
            }
        }
    }*/
    /* Make sure the rates below this have lower PER */
    /* Monotonicity is kept only for rates below the current rate. */
    /* Maintain monotonicity for rates above the current rate*/
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
#if ATH_SUPPORT_VOWEXT /* RCA */
if ( !ATH_IS_VOWEXT_RCA_ENABLED(sc)) 
#endif
{
    if (pRc->state[txRate].per > lastPer) {
        A_UINT8 nextRate;
        rate = txRate;
        /*
         * If PER of current rate is higher than the next upper valid rate
         * the upper rate PER data is likely stale. Reduce rateMaxPhy.
         */
        if (rcGetNextValidTxRate(pRateTable, pRc, rate, &nextRate)) {
            if ((pRc->state[nextRate].per < pRc->state[rate].per) &&
                (pRc->rateMaxPhy > rate))
            {
                pRc->rateMaxPhy = rate;
            }
        }
    }
}

    /* Every so often, we reduce the thresholds and PER (different for CCK and OFDM). */

    if (nowMsec - pRc->perDownTime >= pRateTable->rssiReduceInterval) {
        for (rate = 0; rate < pRc->rateTableSize; rate++) {
            pRc->state[rate].per = ((7*pRc->state[rate].per) >> 3);
        }
        pRc->perDownTime = nowMsec;
    }

#if ATH_SUPPORT_VOWEXT /* RCA */  
    /* Every so often, we increase the aggr size (different for CCK and OFDM). */
    if (ATH_IS_VOWEXT_RCA_ENABLED(sc)){
     	if (  (nowMsec - pRc->aggrUpTime) >= sc->rca_aggr_uptime ){
            for (rate = 0; rate < pRc->rateTableSize; rate++) {
        	pRc->state[rate].maxAggrSize = MIN( MAX_AGGR_LIMIT, \
				(pRc->state[rate].maxAggrSize)+ sc->aggr_aging_factor );
            }
            pRc->aggrUpTime = nowMsec;
    	}	
    }
#endif

	/*
	 * Send signal back to headline block removal state machine: hey, the PER
	 * is lower than the threshold to go back to ACTIVE state
	 */
	if (sc->sc_hbr_params[ac].hbr_enable &&
//	 	lastminRatePer > sc->sc_hbr_per_low && 
//		((struct ieee80211_node*)(an->an_node))->ni_hbr_block &&
		pRc->state[an->an_minRate[ac]].per <= sc->sc_hbr_per_low &&
        sc->sc_ieee_ops->hbr_settrigger) 
	{
		sc->sc_ieee_ops->hbr_settrigger(an->an_node, HBR_EVENT_FORWARD);
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

static inline A_UINT32
getRateIndex(RATE_TABLE_11N * pRateTable, A_UINT32 index, A_UINT8 flags)
{
    A_UINT32 rix;

    if ((flags & ATH_RC_CW40_FLAG) && (flags & ATH_RC_SGI_FLAG)) {
        rix = pRateTable->info[index].htIndex;
    } else if (flags & ATH_RC_SGI_FLAG) {
        rix = pRateTable->info[index].sgiIndex;
    } else if (flags & ATH_RC_CW40_FLAG) {
        rix = pRateTable->info[index].cw40Index;
    } else {
        rix = index; //pRateTable->info[index].baseIndex;
    }

    return rix;
}

/*
 * This routine is called by the Tx interrupt service routine to give
 * the status of previous frames.
 */
#if ATH_SUPPORT_VOWEXT
void
rcUpdate_11nViVo(struct ath_softc *sc,
                struct ath_node *an,
                A_RSSI rssiAck,
                A_UINT8 ac,
                int finalTSIdx,
                int Xretries,
                struct ath_rc_series rcs[],
                int nFrames,
                int  nBad,
                int long_retry,
                int short_retry_fail,
                int nHeadFail,
                int nTailFail)
#else
void
rcUpdate_11nViVo(struct ath_softc *sc,
                struct ath_node *an,
                A_RSSI rssiAck,
                A_UINT8 ac,
                int finalTSIdx,
                int Xretries,
                struct ath_rc_series rcs[],
                int nFrames,
                int  nBad,
                int long_retry,
                int short_retry_fail)
#endif
{
    A_UINT32 series, tries;
    A_UINT32 rix, index;
    A_UINT32 nowMsec;
    struct atheros_softc *asc;
    RATE_TABLE_11N *pRateTable;
    struct atheros_node *pSib;
    TX_RATE_CTRL *pRc;
    A_UINT8 flags; 
    int total_frames, bad_frames;
    static const A_UINT8       translation_table_5g [] = 
                   {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,17,18,19,20,21,22,23,24,26,27,28,29,30,32,34,36};
	static const A_UINT8       translation_table_2g [] = 
				   {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,21,22,23,24,25,26,27,28,30,31,32,33,34,36,38,40};

#if ATH_SUPPORT_VOWEXT /* RCA */
    int head_fail, tail_fail;
#endif
    int isProbe = 0;
    A_UINT8 if_usedrts;  

    ASSERT(an);

    /*don't update status at sounding frame. Since sounding frame is without steering
     , it is forced to use lower rate at sounding frame. And it is better not to use at
     rate control algorithm */
#ifdef ATH_SUPPORT_TxBF
    if (rcs[0].flags & ATH_RC_SOUNDING_FLAG) {
        return;
    }
#endif
    pSib = ATH_NODE_ATHEROS(an);
    pRc = (TX_RATE_CTRL *)(&pSib->txRateCtrlViVo);
    asc = (struct atheros_softc*)sc->sc_rc;
    pRateTable = (RATE_TABLE_11N *)asc->hwRateTable[sc->sc_curmode];
    nowMsec = A_MS_TICKGET();
    ASSERT (rcs[0].tries != 0);

    /* Process the entire rate series */
    for (series = 0; series <= finalTSIdx ; series++) {
		  switch(sc->sc_curmode) {
		  case WIRELESS_MODE_11NA_HT20:
		  case WIRELESS_MODE_11NA_HT40PLUS:
		  case WIRELESS_MODE_11NA_HT40MINUS:
			  index = translation_table_5g[rcs[series].rix];
              break;
		  case WIRELESS_MODE_11NG_HT20:
		  case WIRELESS_MODE_11NG_HT40PLUS:
		  case WIRELESS_MODE_11NG_HT40MINUS:
			  index = translation_table_2g[rcs[series].rix];
              break;
		  default:
			  index = rcs[series].rix;
		  }

        if (rcs[series].tries != 0) {
            flags = rcs[series].flags;

            /* If HT40 and we have switched mode from 40 to 20 => don't update */
            if ((flags & ATH_RC_CW40_FLAG) &&
                (pRc->rcPhyMode != (flags & ATH_RC_CW40_FLAG)))
            {
                return;
            }

            rix = getRateIndex(pRateTable, index, flags);

            if_usedrts = (flags & ATH_RC_RTSCTS_FLAG );   
            if ( (!series) && (pRateTable->info[rix].rateKbps > pRateTable->info[pRc->rateMaxPhy].rateKbps) ){
            	isProbe = 1;
            }

            if ((series == finalTSIdx) && !Xretries && !short_retry_fail) {
                tries = long_retry;
                total_frames = nFrames;
                bad_frames = nBad;

#if ATH_SUPPORT_VOWEXT /* RCA */  
                head_fail = nHeadFail;
                tail_fail = nTailFail;
                /* if the last series is successful after more than one retries, 
                 * update the aggr table. 
                 * don't update the aggr table if RTS is disabled */
                if ( if_usedrts && long_retry && (nFrames > 1) ) { 
			        pRc->state[rix].maxAggrSize = MAX( MIN_AGGR_LIMIT, 
                            (pRc->state[rix].maxAggrSize - sc->badper_aggr_red_factor ) );
		        }
#endif

            } else {
                /* transmit failed completely */
                tries = rcs[series].tries;
                total_frames = nFrames;
                bad_frames = nFrames;

#if ATH_SUPPORT_VOWEXT /* RCA */  
                head_fail = tail_fail = (nFrames >> 1);
                if ((series < finalTSIdx) && (nFrames > 1) &&
                        !short_retry_fail)
                {
                    pRc->state[rix].maxAggrSize = MAX(MIN_AGGR_LIMIT,
                                (pRc->state[rix].maxAggrSize -
                                   sc->badper_aggr_red_factor));
                } 
#endif

            }

#if ATH_SUPPORT_VOWEXT /* RCA */
        if (!short_retry_fail && (series == finalTSIdx)){
           if (((ATH_IS_VOWEXT_AGGRSIZE_ENABLED(sc)) &&
                       (!ATH_IS_VOWEXT_RCA_ENABLED(sc)) &&
                       ((total_frames+1) >= ((pRc->nAggrSize)>>1)))
                    ||
                 ((ATH_IS_VOWEXT_RCA_ENABLED(sc))&&
                    (((total_frames+1) >= pRc->aggrLimit) || (nFrames >= 4))))
           {

               /* last condition is to avoid the RTS failure case.*/
               pRc->nHeadFail = head_fail;
               pRc->nTailFail = tail_fail;
               pRc->nAggrSize = total_frames;
               pRc->lastRate = rix;
           }
           else if ((Xretries) && (nFrames > 1)) {
               pRc->state[rix].maxAggrSize = MAX( MIN_AGGR_LIMIT, pRc->state[rix].maxAggrSize - 2 ) ; 
           }
        }
#endif
        rcUpdate_ht(sc, an, ac, rix, Xretries, tries, rssiAck, nowMsec, total_frames, bad_frames, isProbe, short_retry_fail, if_usedrts);

#if ATH_SUPPORT_HYFI_ENHANCEMENTS
        pRc->lastRateIndex = rix;
#endif

        }
    }

    if (short_retry_fail) {
        pRc->consecRtsFailCount++;
    } else {
        pRc->consecRtsFailCount = 0;
    }

}
#endif /* ATH_SUPPORT_IQUE */
