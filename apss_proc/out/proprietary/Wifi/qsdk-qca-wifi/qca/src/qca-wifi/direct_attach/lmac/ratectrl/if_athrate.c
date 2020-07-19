/*
 * Copyright (c) 2013, 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */
/*
 * 2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * Notifications and licenses are retained for attribution purposes only.
 */

/*-
 * Copyright (c) 2004 Video54 Technologies, Inc.
 * Copyright (c) 2004 Atheros Communications, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Atheros rate control algorithm
 */
 /* 
 *QCA chooses to take this file subject to the terms of the BSD license. 
 */

#include <osdep.h>

#if !NO_HAL
#include "opt_ah.h"
#endif
#include "ah.h"
#include "ratectrl.h"
#include "ratectrl11n.h"
#include "ah_desc.h"
#include "ath_internal.h"

#include <_ieee80211.h>

#ifndef REMOVE_PKT_LOG
#include "pktlog_rc.h"
extern struct ath_pktlog_rcfuncs *g_pktlog_rcfuncs;
#endif

static int ath_rate_newassoc_11n(struct ath_softc *sc, struct ath_node *an, int isnew,
                                 unsigned int capflag, 
                                 struct ieee80211_rateset *negotiated_rates,
                                 struct ieee80211_rateset *negotiated_htrates);
static void ath_rate_findrate_11n(struct ath_softc *sc,
                                  struct ath_node *an,
                                  size_t frameLen,
                                  int numTries,
                                  int numRates,
                                  unsigned int rcflag,
                                  u_int8_t ac,
                                  struct ath_rc_series series[],
                                  int *isProbe,
                                  int isretry,
                                  u_int32_t bf_flags,
				                  struct ath_rc_pp *rc_pp);

/*
 * Attach to a device instance.  Setup the public definition
 * of how much per-node space we need and setup the private
 * phy tables that have rate control parameters.  These tables
 * are normally part of the Atheros hal but are not included
 * in our hal as the rate control data was not being used and
 * was considered proprietary (at the time).
 */
struct atheros_softc *
ath_rate_attach(struct ath_hal *ah)
{
    struct atheros_softc *asc;

    RC_OS_MALLOC(&asc, sizeof(struct atheros_softc), RATECTRL_MEMTAG);
    if (asc == NULL)
        return NULL;

    OS_MEMZERO(asc, sizeof(struct atheros_softc));
    asc->ah_magic = ath_hal_get_device_info(ah, HAL_MAC_MAGIC);

    /*
     * Use the magic number to figure out the chip type.
     * There's probably a better way to do this but for
     * now this suffices.
     *
     * NB: We don't have a separate set of tables for the
     *     5210; treat it like a 5211 since it has the same
     *     tx descriptor format and (hopefully) sufficiently
     *     similar operating characteristics to work ok.
     */
    switch (asc->ah_magic) {
#ifdef AH_SUPPORT_AR5212
    case 0x19570405:
    case 0x19541014:    /* 5212 */
        ar5212AttachRateTables(asc);
        asc->prate_maprix = ar5212_rate_maprix;
        break;
#endif
#ifdef AH_SUPPORT_AR5416
    case 0x19641014:    /* 5416 */
        ar5416AttachRateTables(asc);
        asc->prate_maprix = ar5416_rate_maprix;
        break;
#endif
#ifdef AH_SUPPORT_AR9300
    case 0x19741014:    /* 9300 */
        ar9300AttachRateTables(asc);
        asc->prate_maprix = ar9300_rate_maprix;
        break;
#endif
    default:
        ASSERT(0);
        break;
    }

    /* Save Maximum TX Trigger Level (used for 11n) */
    ath_hal_getcapability(ah, HAL_CAP_TX_TRIG_LEVEL_MAX, 0, &asc->txTrigLevelMax);

    /*  return alias for atheros_softc * */
    return asc;
}

struct atheros_vap *
ath_rate_create_vap(struct atheros_softc *asc, struct ath_vap *athdev_vap)
{
    struct atheros_vap *avp;

    RC_OS_MALLOC(&avp, sizeof(struct atheros_vap), RATECTRL_MEMTAG);
    if (avp == NULL)
        return NULL;

    OS_MEMZERO(avp, sizeof(struct atheros_vap));
    avp->asc = asc;
    avp->athdev_vap = athdev_vap;
    return avp;
}

struct atheros_node *
ath_rate_node_alloc(struct atheros_vap *avp)
{
    struct atheros_node *anode;

    RC_OS_MALLOC(&anode, sizeof(struct atheros_node), RATECTRL_MEMTAG);
    if (anode == NULL)
        return NULL;

    OS_MEMZERO(anode, sizeof(struct atheros_node));
    anode->avp = avp;
    anode->asc = avp->asc;
#if ATH_SUPPORT_IQUE
    anode->rcFunc[WME_AC_VI].rcUpdate = &rcUpdate_11nViVo;
    anode->rcFunc[WME_AC_VO].rcUpdate = &rcUpdate_11nViVo;
    anode->rcFunc[WME_AC_BE].rcUpdate = &rcUpdate_11n;
    anode->rcFunc[WME_AC_BK].rcUpdate = &rcUpdate_11n;

    anode->rcFunc[WME_AC_VI].rcFind = &rcRateFind_11nViVo;
    anode->rcFunc[WME_AC_VO].rcFind = &rcRateFind_11nViVo;
    anode->rcFunc[WME_AC_BE].rcFind = &rcRateFind_11n;
    anode->rcFunc[WME_AC_BK].rcFind = &rcRateFind_11n;
#endif
    return anode;
}

void
ath_rate_node_free(struct atheros_node *anode)
{
    if (anode != NULL)
        RC_OS_FREE(anode, sizeof(*anode));
}

void
ath_rate_free_vap(struct atheros_vap *avp)
{
    if (avp != NULL)
        RC_OS_FREE(avp, sizeof(*avp));
}

void
ath_rate_detach(struct atheros_softc *asc)
{
    if (asc != NULL)
        RC_OS_FREE(asc, sizeof(*asc));
}

/*
 * Initialize per-node rate control state.
 */
void
ath_rate_node_init(struct atheros_node *pSib)
{
    rcSibInit(pSib);
}

/*
 * Cleanup per-node rate control state.
 */
void
ath_rate_node_cleanup(struct atheros_node *an)
{
    /* NB: nothing to do */
    UNREFERENCED_PARAMETER(an);
}

/*
 * Return the next series 0 transmit rate and setup for a callback
 * to install the multi-rate transmit data if appropriate.  We cannot
 * install the multi-rate transmit data here because the caller is
 * going to initialize the tx descriptor and so would clobber whatever
 * we write. Note that we choose an arbitrary series 0 try count to
 * insure we get called back; this permits us to defer calculating
 * the actual number of tries until the callback at which time we
 * can just copy the pre-calculated series data.
 */
void
ath_rate_findrate(struct ath_softc *sc, struct ath_node *an,
                  int shortPreamble, u_int32_t frameLen, int numTries,
                  unsigned int rcflag, u_int8_t ac, struct ath_rc_series rcs[],
                  int *isProbe, int isretry, u_int32_t bf_flags, 
                  struct ath_rc_pp *rc_pp)
{
    struct atheros_node *oan = an->an_rc_node;
    struct atheros_softc *asc = oan->asc;
    struct atheros_vap *avap = oan->avp;
    struct ath_vap *avp = oan->avp->athdev_vap;
    const RATE_TABLE *pRateTable = avap->rateTable;
    u_int8_t *retrySched;
    int       i;
    u_int8_t fixedratecode;
    u_int32_t old_av_fixed_rateset;
    u_int32_t old_av_fixed_retryset;
    ASSERT(rcs != NULL);

    if (sc->sc_rc->ah_magic == 0x19641014 ||
        sc->sc_rc->ah_magic == 0x19741014)
    {
#ifdef ATH_SUPPORT_TxBF     // set calibration and sounding indicator
#define MS(_v, _f)  (((_v) & _f) >> _f##_S)
        sc->sc_txbfsounding = 0;
        oan->txbf_sounding = 0;
        sc->sc_txbfcalibrating = 0;
        if ((MS(bf_flags,HAL_TXDESC_TXBF_SOUND)==HAL_TXDESC_STAG_SOUND)
            ||(MS(bf_flags,HAL_TXDESC_TXBF_SOUND)== HAL_TXDESC_SOUND)){
            sc->sc_txbfsounding = 1;
            oan->txbf_sounding = 1;
        }            
        if (bf_flags & HAL_TXDESC_CAL) {
            sc->sc_txbfcalibrating = 1;
        }
#endif
            /* store the previous values */
            old_av_fixed_rateset = avp->av_config.av_fixed_rateset;
            old_av_fixed_retryset = avp->av_config.av_fixed_retryset;

            /* If fixed node rate is enabled, we fixed the link rate */
            if (an->an_fixedrate_enable) {
                fixedratecode = an->an_fixedratecode;
                avp->av_config.av_fixed_rateset = 0;
                for (i=0; i<4; i++) {
                    avp->av_config.av_fixed_rateset <<= 8;
                    avp->av_config.av_fixed_rateset |= fixedratecode; 
                }
                avp->av_config.av_fixed_retryset = 0x03030303;
            }

            ath_rate_findrate_11n(sc, an, frameLen, numTries, 4,
                                  rcflag, ac, rcs, isProbe, isretry, bf_flags, rc_pp);

            /* restore the previous values */
            avp->av_config.av_fixed_rateset = old_av_fixed_rateset;
            avp->av_config.av_fixed_retryset = old_av_fixed_retryset;
            return;
    }

    if (asc->fixedrix == IEEE80211_FIXED_RATE_NONE) {
        rcs[0].rix = rcRateFind(sc, oan, frameLen, pRateTable, &sc->sc_curchan, isretry);
	/* multi-rate support implied */
        rcs[0].tries = ATH_TXMAXTRY-1;	/* anything != ATH_TXMAXTRY */
    } else {
        rcs[0].rix = asc->fixedrix;
        rcs[0].tries = ATH_TXMAXTRY;
    }

	/* If fixed node rate is enabled, we fixed the link rate */
    if (an->an_fixedrate_enable) {
        rcs[0].rix = an->an_fixedrix;
        rcs[0].tries = ATH_TXMAXTRY;
    }

    ASSERT(rcs[0].rix != (u_int8_t)-1);
    /* get the corresponding index for the choosen rate in txRateCtrl */
    for (i = 0; oan->txRateCtrl.validRateIndex[i] != rcs[0].rix 
       && i < oan->txRateCtrl.maxValidRate ; i++);

    ASSERT(asc->fixedrix != IEEE80211_FIXED_RATE_NONE || i != oan->txRateCtrl.maxValidRate);

    if (rcs[0].tries != ATH_TXMAXTRY) {

        /* NB: only called for data frames */
        if (oan->txRateCtrl.probeRate) {
            retrySched = shortPreamble ?
                (u_int8_t *)&(oan->txRateCtrl.validRateSeries[i][24]) :
                (u_int8_t *)&(oan->txRateCtrl.validRateSeries[i][16]);
        } else {
            retrySched = shortPreamble ?
                (u_int8_t *)&(oan->txRateCtrl.validRateSeries[i][8]) :
                (u_int8_t *)&(oan->txRateCtrl.validRateSeries[i][0]);
        }
#ifdef ATH_SUPPORT_UAPSD_RATE_CONTROL
        if (oan->uapsd_rate_ctrl) {
            retrySched = shortPreamble ?
                (u_int8_t *)&(oan->txRateCtrl.validRateSeries[i][40]) :
                (u_int8_t *)&(oan->txRateCtrl.validRateSeries[i][32]);
        }
#endif /* ATH_SUPPORT_UAPSD_RATE_CONTROL */
        /* Update rate seies based on retry schedule */
        rcs[0].tries = retrySched[0];
        rcs[1].tries = retrySched[1];
        rcs[2].tries = retrySched[2];
        rcs[3].tries = retrySched[3];
        
        if (!IS_CHAN_TURBO(&sc->sc_curchan))
            ASSERT(rcs[0].rix == pRateTable->rateCodeToIndex[retrySched[4]]);

        /* 
         * Retry scheduler assumes that all the rates below max rate in the
         * intersection rate table are present and so it takes the next possible
         * lower rates for the retries, which is not correct. So check them
         * before filling the retry rates.
         */
        rcs[1].rix = pRateTable->rateCodeToIndex[retrySched[5]];
        rcs[2].rix = pRateTable->rateCodeToIndex[retrySched[6]];
        rcs[3].rix = pRateTable->rateCodeToIndex[retrySched[7]];

        if (sc->sc_curchan.priv_flags & CHANNEL_4MS_LIMIT) {
            u_int32_t  prevrix = rcs[0].rix;

            for (i=1; i<4; i++) {
                if (rcs[i].tries) {
                    if (pRateTable->info[rcs[i].rix].max4msFrameLen < frameLen) {
                        rcs[i].rix = prevrix;
                    } else {
                        prevrix = rcs[i].rix;
                    }
                } else {
                    /* Retries are 0 from here */
                    break;
                }
            }
        }

    }

    if (sc->sc_ieee_ops->update_txrate) {
        sc->sc_ieee_ops->update_txrate(an->an_node, oan->rixMap[rcs[0].rix]);
    }
    
#if ATH_SUPERG_DYNTURBO
    /* XXX map from merged table to split for driver */
    if (IS_CHAN_TURBO(&sc->sc_curchan) && rcs[0].rix >=
        (pRateTable->rateCount - pRateTable->numTurboRates))
    {
        u_int32_t numCCKRates = 5;
        u_int32_t i;

         rcs[0].rix -= (pRateTable->rateCount-pRateTable->numTurboRates);
         if (IS_CHAN_2GHZ(&sc->sc_curchan)) {
            for (i=1;i<=3;i++) { /*Mapping for retry rates from merged table to split table*/
                 if (rcs[i].rix >= numCCKRates) {
                     rcs[i].rix -= numCCKRates;
                 } else { /* For 6Mbps Turbo rate */
                     rcs[i].rix -= numCCKRates-1;
                 }                                
            }
        }

    }
#endif
    if (oan->txRateCtrl.consecRtsFailCount > MAX_CONSEC_RTS_FAILED_FRAMES) {
        rcs[0].flags |= ATH_RC_RTSCTS_FLAG;
        rcs[1].rix = rcs[2].rix = rcs[3].rix = 0;
        rcs[1].tries = rcs[2].tries = rcs[3].tries = 0;
    }
}


u_int8_t
ath_rate_findrateix(struct ath_softc *sc, struct ath_node *an, u_int8_t dot11Rate)
{
    struct atheros_node *oan = an->an_rc_node;
    struct atheros_vap *avap = oan->avp;
    const RATE_TABLE *pRateTable;
    const RATE_TABLE_11N *pRateTable_11n;
    int i;

    if (sc->sc_rc->ah_magic == 0x19641014 ||
        sc->sc_rc->ah_magic == 0x19741014)
    {
            pRateTable_11n = (const RATE_TABLE_11N *)avap->rateTable;
            for (i=0 ; i < pRateTable_11n->rateCount; i++) {
                if ((pRateTable_11n->info[i].dot11Rate & 0x7f) ==
                                    (dot11Rate & 0x7f)) {
                    return i;
                }
            }
            return 0;
    }

    pRateTable = (const RATE_TABLE *)avap->rateTable;

    for (i=0 ; i < pRateTable->rateCount; i++) {
        if ((pRateTable->info[i].dot11Rate & 0x7f) == (dot11Rate & 0x7f)) {
            return i;
        }
    }

    return 0;
}

#ifdef AH_SUPPORT_AR5212
#include <ar5212/ar5212desc.h>
#endif

#define    MS(_v, _f)    (((_v) & _f) >> _f##_S)

/*
 * Process a tx descriptor for a completed transmit (success or failure).
 */
#ifdef ATH_SUPPORT_VOWEXT
void
ath_rate_tx_complete(struct ath_softc *sc, struct ath_node *an,
                     struct ath_desc *ds, struct ath_rc_series rcs[],
                     u_int8_t ac, int nframes, int nbad, int nHeadFail, 
                     int nTailFail, int rts_retry_limit, struct ath_rc_pp *pp_rc)
#else
void
ath_rate_tx_complete(struct ath_softc *sc, struct ath_node *an,
                     struct ath_desc *ds, struct ath_rc_series rcs[],
                     u_int8_t ac, int nframes, int nbad, int rts_retry_limit,
                     struct ath_rc_pp *pp_rc)
#endif
{
    struct atheros_node    *atn = an->an_rc_node;
    struct atheros_vap     *avap = atn->avp;
    const RATE_TABLE       *pRateTable = avap->rateTable;
    u_int8_t               txRateCode = ds->ds_txstat.ts_ratecode;
    u_int8_t               totalTries = 0;
#if 0
    int                    short_retry_fail = ds->ds_txstat.ts_shortretry ;
#endif

    if (sc->sc_rc->ah_magic == 0x19641014 ||
        sc->sc_rc->ah_magic == 0x19741014)
    {
    
#ifdef ATH_SUPPORT_VOWEXT
        ath_rate_tx_complete_11n(sc, an, &ds->ds_txstat, rcs, ac, nframes, nbad,
                                 nHeadFail, nTailFail, rts_retry_limit, pp_rc);
#else
        ath_rate_tx_complete_11n(sc, an, &ds->ds_txstat, rcs, ac, nframes, nbad, 
                                 rts_retry_limit, pp_rc);
#endif
        return;
    }

    if (pRateTable->rateCodeToIndex[txRateCode] == (u_int8_t) -1) {
        /*
         * This can happen, for example, when switching bands
         * and pending tx's are processed before the queue
         * is flushed (should fix mode switch to ensure this
         * does not happen).
         */
        // DPRINTF(sc, "%s: no mapping for rate code 0x%x",
        //         __func__, txRate);
        return;
    }
#ifdef AH_SUPPORT_AR5212
    if (ds->ds_txstat.ts_rateindex != 0) {
        const struct ar5212_desc *ads = AR5212DESC(ds);
        int finalTSIdx = MS(ads->ds_txstatus1, AR_FinalTSIndex);
        int series;
        
        /*
         * Process intermediate rates that failed.
         */
        for (series = 0; series < finalTSIdx; series++) {
            int rate, tries;

            /* NB: we know series <= 2 */
            switch (series) {
            case 0:
                rate = MS(ads->ds_ctl3, AR_XmitRate0);
                tries = MS(ads->ds_ctl2, AR_XmitDataTries0);
                break;
            case 1:
                rate = MS(ads->ds_ctl3, AR_XmitRate1);
                tries = MS(ads->ds_ctl2, AR_XmitDataTries1);
                break;
            default:
                rate = MS(ads->ds_ctl3, AR_XmitRate2);
                tries = MS(ads->ds_ctl2, AR_XmitDataTries2);
                break;
            }

            if (pRateTable->rateCodeToIndex[rate] != (u_int8_t) -1) {
                /*
                 * This can happen, for example, when switching bands
                 * and pending tx's are processed before the queue
                 * is flushed (should fix mode switch to ensure this
                 * does not happen).
                 */
                // DPRINTF(sc, "%s: no mapping for rate code 0x%x",
                //         __func__, txRate);
                rcUpdate(atn
                         , 2 // Huh? Indicates an intermediate rate failure. Should use a macro instead.
                         , pRateTable->rateCodeToIndex[rate]
                         , tries
                         , ds->ds_txstat.ts_rssi
                         , ds->ds_txstat.ts_antenna
                         , pRateTable
                         , sc->sc_opmode 
                         , sc->sc_diversity
                         , sc->sc_setdefantenna
                         , (void *)sc
                         , 0
                    );
            }

            /* Account for retries on intermediate rates */
            totalTries += tries;
        }
    }
#endif

    /* 
     * Exclude intermediate rate retries, or the last rate, which may have 
     * succeeded, will incur a penalty higher than the intermediate rates that
     * failed.
     */
    rcUpdate(atn
             , (ds->ds_txstat.ts_status & HAL_TXERR_XRETRY) != 0
             , pRateTable->rateCodeToIndex[txRateCode]
             , ds->ds_txstat.ts_longretry - totalTries
             , ds->ds_txstat.ts_rssi
             , ds->ds_txstat.ts_antenna
             , pRateTable
             , sc->sc_opmode
             , sc->sc_diversity
             , sc->sc_setdefantenna
             , (void *)sc
             , 0
    );

#if 0
    if (short_retry_fail == rts_retry_limit) {
        atn->txRateCtrl.consecRtsFailCount++;
    } else {
        atn->txRateCtrl.consecRtsFailCount = 0;
    }
#else
    atn->txRateCtrl.consecRtsFailCount = 0;
#endif
}

/*
 * Return current transmit rate.
 */
u_int32_t
ath_rate_node_gettxrate(struct ath_node *an)
{
    struct atheros_node *oan = ATH_NODE_ATHEROS(an);
    return oan->lastRateKbps;
}

/*
 * Return the max phy rate while in IBSS.
 */
static u_int32_t
ath_rate_getIBSSmaxphyrate(const RATE_TABLE_11N *lp_tbl, const u_int8_t tx_streams, const WIRELESS_MODE mode)
{
	int i = 0;
	u_int32_t max_rate = 1000;
	for(i = 0; i < lp_tbl->rateCount; i++){
		u_int8_t valid = lp_tbl->info[i].validSS;
		u_int8_t mode_filter = ~0;

		/* get the valid seaching scope with tx_stream number */
		switch(tx_streams){
		case 2:
			valid = lp_tbl->info[i].validDS;
			break;
		case 3:
			valid = lp_tbl->info[i].validTS;
			break;
		default:
			break;
		}

		/* get the result valid scope by wireless mode */
		switch(mode){
		case WIRELESS_MODE_11NA_HT20:
		case WIRELESS_MODE_11NG_HT20:
		case WIRELESS_MODE_XR:
			mode_filter &= ~TRUE_40;
			break;
		default:
			break;
		}
		valid &=  mode_filter;

		/* get the max rate in the legal valid scope */
		if(valid){
			if(lp_tbl->info[i].rateKbps > max_rate)
				max_rate = lp_tbl->info[i].rateKbps;
		}
	}
	return max_rate;
}
 
u_int32_t
ath_rate_getmaxphyrate(struct ath_softc *sc, struct ath_node *an)
{
    struct atheros_node *oan = an->an_rc_node;
    struct atheros_softc *asc = oan->asc;
    struct ath_vap         *avp = oan->avp->athdev_vap;
    TX_RATE_CTRL *pRc  = (TX_RATE_CTRL *)(oan);
    int ratecode = IEEE80211_RATE_IDX_ENTRY(avp->av_config.av_fixed_rateset, 3);
    WIRELESS_MODE bss_curmode;
    int i = 0 ;
    const RATE_TABLE_11N *pRateTable;

    /* choose the proper rate table */
    if (sc->sc_rc->ah_magic == 0x19641014 ||
        sc->sc_rc->ah_magic == 0x19741014){
        pRateTable = asc->hwRateTable[sc->sc_curmode];

    } else {
        struct atheros_vap *avap = oan->avp;
        pRateTable = avap->rateTable;
    }
    if (pRc->maxValidRate == 0) 
       return 0;

    /* IBSS: the STA just indicates link rate from its capability */
    if (an->an_avp->av_opmode == HAL_M_IBSS) {
        return ath_rate_getIBSSmaxphyrate(pRateTable,sc->sc_tx_numchains,sc->sc_curmode);
    }

    /* Get the current mode of the bss from the umac. This could be different
     * for different vaps. Like in repeater mode, the AP vap could be in 11n
     * and STA vap can be in 11g mode.
     */
    if (sc->sc_ieee_ops->get_vap_bss_mode) {
        bss_curmode = sc->sc_ieee_ops->get_vap_bss_mode(an->an_sc->sc_ieee,
                                                        an->an_node);
    } else {
        bss_curmode = sc->sc_curmode;
    }

    for (i = pRc->validRateIndex[pRc->maxValidRate-1]; i > 0; i--) {
        /*
         * If fixed rate is used, make sure the ratecode match.
         * If the node doesn't support Short GI (SGI), don't return
         * an SGI rate as the max PHY rate.
         */
        if (((avp->av_config.av_fixed_rateset != IEEE80211_FIXED_RATE_NONE) &&
             (pRateTable->info[i].rateCode != ratecode)) ||
            (WLAN_RC_PHY_SGI(pRateTable->info[i].phy) &&
             !avp->av_config.av_short_gi))
        {
            continue;
        }

        /* Also consider the user chain mask setting in HOSTAP mode */
        if ((an->an_avp->av_opmode == HAL_M_HOSTAP) && (bss_curmode >= WIRELESS_MODE_11NA_HT20) && (avp->av_config.av_fixed_rateset == IEEE80211_FIXED_RATE_NONE)) {
            if ((sc->sc_tx_numchains == 1 && !WLAN_RC_PHY_SS(pRateTable->info[i].phy)) ||
                (sc->sc_tx_numchains == 2 && !WLAN_RC_PHY_DS(pRateTable->info[i].phy)) ||
                (sc->sc_tx_numchains == 3 && !WLAN_RC_PHY_TS(pRateTable->info[i].phy)))
                    continue;
        } else {
            /* 
             * In STA mode the tx_numchains depends on the AP it is connected to.
             * So simply check for the short gi 
             */
            if ((WLAN_RC_PHY_SGI(pRateTable->info[i].phy) &&
                 !avp->av_config.av_short_gi))
                continue;
        }

        break;
    }

    return (pRateTable->info[i].rateKbps);
}

#if QCA_AIRTIME_FAIRNESS

u_int32_t userRateKbps[8][MAX_TX_RATE_TBL] = { // userRateKbps[Aggr Buckets][Rate Table Size];
{ 336500, 309900, 309900, 284200, 281300, 258400, 220900, 202000, 140600, 107400, 73300, 37500,
  243100, 222100, 202900, 182700, 141000, 96800,  73500,  49800,  25200,
  131600, 119400, 108200, 96900,  73700,  49900,  37600,  25300,  12700,
  182300, 166100, 166100, 151100, 149500, 135700, 114700, 104000, 70900,  53700,  36200, 18300,
  126900, 115000, 104300, 93400,  71000,  48100,  36300,  24400,  12300,
  66000,  59700,  53900,  48100,  36300,  24400,  18300,  12300,  6200,
  0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,     0,   },
{ 325700, 299500, 299500, 276000, 273900, 251400, 216000, 197300, 138500, 106000, 72700, 37300,
  237100, 217400, 198400, 179300, 139000, 95800,  72800,  49500,  25200,
  129800, 117900, 107000, 95900,  73100,  49600,  37500,  25200,  12700,
  178800, 163100, 163100, 148700, 147100, 133700, 113300, 102900, 70400,  53400,  36100, 18300,
  125200, 113700, 103200, 92500,  70500,  47800,  36100,  24300,  12200,
  65600,  59300,  53600,  47800,  36200,  24300,  18300,  12300,  6200,
  0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,     0,   },
{ 310100, 287400, 287400, 265300, 262900, 242300, 209500, 191800, 135600, 104300, 71900, 37100,
  230000, 211000, 193000, 175200, 136200, 94600,  72100,  49100,  25100,
  127700, 116100, 105000, 94700,  72400,  49300,  37300,  25100,  12700,
  174200, 159500, 159500, 145700, 143900, 131400, 111400, 101400, 69700,  52900,  35900, 18200,
  123100, 111900, 101800, 91400,  69900,  47500,  35900,  24200,  12200,
  65000,  58800,  53200,  47500,  36000,  24300,  18300,  12200,  6200,
  0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,     0,   },
{ 291000, 269400, 269400, 250700, 248100, 229900, 199600, 184000, 131500, 101900, 70700, 36700,
  219200, 201300, 185500, 168800, 132300, 92700,  70900,  48600,  24900,
  124200, 113200, 103200, 92800,  71300,  48800,  37000,  25000,  12600,
  167600, 153900, 153900, 141000, 139300, 127600, 108700, 99100,  68600,  52300,  35600, 18100,
  119800, 109200, 99500,  89500,  68800,  47000,  35600,  24100,  12200,
  64100,  58100,  52600,  47100,  35700,  24100,  18200,  12200,  6100,
  0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,     0,   },
{ 262100, 245600, 245600, 229700, 228200, 211700, 186100, 172400, 125700, 97800,  68800, 36100,
  203200, 188000, 174100, 159200, 126600, 89600,  69100,  47700,  24600,
  118900, 109000, 99500,  89900,  69600,  48000,  36500,  24800,  12600,
  157800, 145400, 145400, 133800, 132800, 121800, 104500, 95700,  66900,  51200,  35100, 18000,
  115100, 105100, 96200,  86800,  67100,  46300,  35200,  23900,  12100,
  62700,  56900,  51700,  46300,  35300,  23900,  18100,  12200,  6100,
  0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,     0,   },
{ 220600, 207000, 207000, 196600, 195000, 182900, 163800, 153100, 114900, 90900,  65199, 35000,
  177300, 166100, 155100, 142900, 116100, 84400,  65600,  46100,  24100,
  110100, 101100, 93100,  84700,  66300,  46400,  35500,  24300,  12500,
  141200, 131700, 131700, 122100, 120800, 111700, 96900,  89200,  63800,  49200,  34200, 17700,
  106100, 97700,  89900,  81800,  64200,  44700,  34300,  23400,  12000,
  60100,  54700,  49900,  44800,  34500,  23500,  17800,  12000,  6100,
  0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,     0,   },
{ 147700, 142300, 142300, 137300, 135800, 129800, 120500, 114600, 91400,  75100,  56600, 32099,
  128300, 123000, 116900, 109300, 92900,  71400,  57200,  41800,  22800,
  89300,  83600,  78100,  71900,  58300,  42400,  32900,  23100,  12100,
  107300, 101700, 101700, 95900,  95100,  89300,  79700,  74600,  55800,  44100,  31600, 16900,
  86700,  80800,  75600,  69700,  56400,  40900,  31800,  22200,  11600,
  53300,  49100,  45100,  41000,  32200,  22400,  17200,  11700,  6000,
  0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,     0,   },
{ 51000,  50100,  50100,  49300,  49300,  48500,  46900,  46200,  41600,  37900,  32400, 22300,
  49300,  48500,  47700,  46200,  42800,  37900,  33200,  27300,  17500,
  42800,  41600,  39900,  38400,  33900,  27800,  23400,  18000,  10500,
  48000,  44100,  44100,  42800,  42800,  41600,  39400 , 37900,  32400,  28100,  22300, 13800,
  41600,  40500,  38900,  37400,  33200,  27100,  22700,  17400,  10100,
  32400,  30700,  29200,  27300,  23000,  17600,  14200,  10300,  5600,
  28900,  27100,  22700,  17000,  13600,  9700,   7500,   5200,   8100,   4900,   1900,  900, },
};

/*
 * Get estimate of airtime using PER and MCS (and NSS and Channel BW)
 */
u_int32_t
ath_rate_atf_airtime_estimate(struct ath_softc *sc,
                              struct ath_node *an, u_int32_t tput, u_int32_t *possible_tput)
{
    struct atheros_node *oan = an->an_rc_node;
    struct atheros_softc *asc = oan->asc;
    TX_RATE_CTRL *pRc = (TX_RATE_CTRL *)(oan);
    const RATE_TABLE_11N *pRateTable;
    struct atheros_vap *avap;
    unsigned int i, j, weighted_rate = 0, rate_count = 0, used, aggr, aggr_count[8], rate, tot_rate, tot_used;
    u_int32_t adjust;

    if (sc->sc_rc->ah_magic == 0x19641014 || sc->sc_rc->ah_magic == 0x19741014) {
        pRateTable = asc->hwRateTable[sc->sc_curmode];
    } else {
        avap = oan->avp;
        pRateTable = avap->rateTable;
    }

    for (i = 0; i < MAX_TX_RATE_TBL; i++) {
        used = pRc->state[i].used;
        pRc->state[i].used = 0;
        aggr = pRc->state[i].aggr;
        pRc->state[i].aggr = 0;
        for (j = 0; j < 8; j++) {
            aggr_count[j] = pRc->state[i].aggr_count[j];
            pRc->state[i].aggr_count[j] = 0;
        }

        rate = pRateTable->info[i].userRateKbps;

        if (sc->sc_use_aggr_for_possible_tput == 1) {
            rate = 0;
            for (j = 0; j < 8; j++) {
                if (aggr & (0xf << ((8 - 1 - j) * 4))) {
                    if ((sc->sc_curmode == WIRELESS_MODE_11NA_HT20) ||
                        (sc->sc_curmode == WIRELESS_MODE_11NA_HT40PLUS) ||
                        (sc->sc_curmode == WIRELESS_MODE_11NA_HT40MINUS) ||
                        (sc->sc_curmode == WIRELESS_MODE_11a)) {
                        rate = userRateKbps[j][MAX_TX_RATE_TBL - 4 - 1 - i];
                    } else if ((sc->sc_curmode == WIRELESS_MODE_11NG_HT20) ||
                        (sc->sc_curmode == WIRELESS_MODE_11NG_HT40PLUS) ||
                        (sc->sc_curmode == WIRELESS_MODE_11NG_HT40MINUS) ||
                        (sc->sc_curmode == WIRELESS_MODE_11g) ||
                        (sc->sc_curmode == WIRELESS_MODE_11b)) {
                        rate = userRateKbps[j][MAX_TX_RATE_TBL - 1 - i];
                    }
                    break;
                }
            }
            if (!rate) {
                rate = pRateTable->info[i].userRateKbps;
            }
        } else if (sc->sc_use_aggr_for_possible_tput == 2) {
            rate = 0;
            tot_rate = 0;
            tot_used = 0;
            for (j = 0; j < 8; j++) {
                if (aggr_count[j]) {
                    if ((sc->sc_curmode == WIRELESS_MODE_11NA_HT20) ||
                        (sc->sc_curmode == WIRELESS_MODE_11NA_HT40PLUS) ||
                        (sc->sc_curmode == WIRELESS_MODE_11NA_HT40MINUS) ||
                        (sc->sc_curmode == WIRELESS_MODE_11a)) {
                        rate = userRateKbps[8 - 1 - j][MAX_TX_RATE_TBL - 4 - 1 - i];
                    } else if ((sc->sc_curmode == WIRELESS_MODE_11NG_HT20) ||
                        (sc->sc_curmode == WIRELESS_MODE_11NG_HT40PLUS) ||
                        (sc->sc_curmode == WIRELESS_MODE_11NG_HT40MINUS) ||
                        (sc->sc_curmode == WIRELESS_MODE_11g) ||
                        (sc->sc_curmode == WIRELESS_MODE_11b)) {
                        rate = userRateKbps[8 - 1 - j][MAX_TX_RATE_TBL - 1 - i];
                    }
                    tot_rate += rate * aggr_count[j];
                    tot_used += aggr_count[j];
                }
            }
            if (tot_used) {
                rate = tot_rate / tot_used;
            }
            if (!rate) {
                rate = pRateTable->info[i].userRateKbps;
            }
        }

        weighted_rate += ((rate * (100 - pRc->state[i].per)) / 100) * used;
        rate_count += used;
    }

    if (rate_count == 0 || weighted_rate == 0) {
        *possible_tput = an->an_old_max_tput;
    } else {
        *possible_tput = (weighted_rate / rate_count);
        an->an_old_max_tput = *possible_tput;
    }

    if (sc->sc_adjust_possible_tput) {
        if (sc->sc_adjust_possible_tput >= 1000 && sc->sc_adjust_possible_tput <= 350000)
            adjust = sc->sc_adjust_possible_tput;
        else if (sc->sc_adjust_possible_tput <= 100)
            adjust = (*possible_tput * sc->sc_adjust_possible_tput) / 100;
        else
            adjust = 0;
        *possible_tput -= (*possible_tput > adjust ? adjust : *possible_tput);
    }

    if (sc->sc_adjust_tput_more_thrs && *possible_tput < sc->sc_adjust_tput_more_thrs) {
        tput += (sc->sc_adjust_tput_more * tput) / 100;
    }
    if (sc->sc_adjust_tput) {
        tput += sc->sc_adjust_tput;
    }

    if (tput < *possible_tput) {
        return (tput * 100) / *possible_tput;
    }

    return 0;
}
#endif

/*
 * Update rate-control state on station associate/reassociate.
 */
int
ath_rate_newassoc(struct ath_softc *sc, struct ath_node *an,
                  int isnew, unsigned int capflag,
                  struct ieee80211_rateset *negotiated_rates,
                  struct ieee80211_rateset *negotiated_htrates)
{
    struct ieee80211_rateset *pRates = negotiated_rates;
    struct atheros_node *oan = an->an_rc_node;
    struct atheros_softc *asc = oan->asc;
    int txrate = 0;

    oan->uapsd_rate_ctrl = 0;

    if( (capflag & ATH_RC_UAPSD_FLAG) && 
        !((an->an_avp->av_opmode == HAL_M_STA) && 
          (sc->sc_ieee_ops->wds_is_enabled(sc->sc_ieee))) ){
       /*
        * This condition is added to prevent WDS STA to use UAPSD rate ctrl.
        * WDS station is setting this capflag based on the Association response frame.
        * UAPSD rate ctrl is only meant for AP and normal stations.
        *
        * CAUTION:: Here we are using HAL_OP mode. it has direct relation to nostabeacons flag.
        * Previous fix for removing the nosbeacons is required to support this fix. Else HAL mode
        * will be in AP_MODE for WDS station and will break this fix. 
        */
        oan->uapsd_rate_ctrl = 1;
    }

    if (sc->sc_rc->ah_magic == 0x19641014 ||
        sc->sc_rc->ah_magic == 0x19741014)
    {
        return (ath_rate_newassoc_11n(sc, an, isnew, capflag, negotiated_rates, negotiated_htrates));
    }

    if (isnew) {
        rcSibUpdate(asc, oan, 0, pRates, sc->sc_curmode);
        if(oan->txRateCtrl.maxValidRate) {
            /* Update minimum rate index in sc */
            sc->sc_minrateix = oan->txRateCtrl.validRateIndex[0];
        }
        /*
         * Set an initial tx rate for the net80211 layer.
         * Even though noone uses it, it wants to validate
         * the setting before entering RUN state so if there
         * was a pervious setting from a different node it
         * may be invalid.
         */
        if (asc->fixedrix != IEEE80211_FIXED_RATE_NONE) {
            if (sc->sc_rixmap[asc->fixedrix] != 0xff)
                txrate = sc->sc_rixmap[asc->fixedrix];
        }
        if (sc->sc_ieee_ops->update_txrate) {
            sc->sc_ieee_ops->update_txrate(an->an_node, txrate);
    }
    }

    return 0;
}

void
ath_rate_node_update(struct ath_node *an)
{
    /* notify net80211 layer */
    an->an_sc->sc_ieee_ops->rate_node_update(an->an_sc->sc_ieee, an->an_node, 1);
}

/*
 * Update rate-control state on a device state change.  When
 * operating as a station this includes associate/reassociate
 * with an AP.  Otherwise this gets called, for example, when
 * the we transition to run state when operating as an AP.
 */
void
ath_rate_newstate(struct ath_softc *sc, struct ath_vap *avp, int up)
{
    struct atheros_softc *asc = sc->sc_rc;
    struct atheros_vap *avap = avp->av_atvp;

    switch (asc->ah_magic) {
#ifdef AH_SUPPORT_AR5212
    case 0x19541014:    /* 5212 */
        /* For half and quarter rate channles use different
         * rate tables
         */
        if (sc->sc_curchan.channel_flags & CHANNEL_HALF) {
            ar5212SetHalfRateTable(asc);
        } else if (sc->sc_curchan.channel_flags & CHANNEL_QUARTER) {
            ar5212SetQuarterRateTable(asc);
        } else { /* full rate */
            ar5212SetFullRateTable(asc);
        }
        break;
#endif
#ifdef AH_SUPPORT_AR5416
    case 0x19641014:
        /* For half and quarter rate channles use different
         * rate tables
         */
#ifndef ATH_NO_5G_SUPPORT
        if (sc->sc_curchan.channel_flags & CHANNEL_HALF) {
            ar5416SetHalfRateTable(asc);
        } else if (sc->sc_curchan.channel_flags & CHANNEL_QUARTER) {
            ar5416SetQuarterRateTable(asc);
        } else { /* full rate */
            ar5416SetFullRateTable(asc);
        }
#endif  /* #ifndef ATH_NO_5G_SUPPORT */
        break;
#endif
#ifdef AH_SUPPORT_AR9300
    case 0x19741014:    /* 9300 */
        /* For half and quarter rate channles use different
         * rate tables
         */
#ifndef ATH_NO_5G_SUPPORT
        if (sc->sc_curchan.channel_flags & CHANNEL_HALF) {
            ar9300SetHalfRateTable(asc);
        } else if (sc->sc_curchan.channel_flags & CHANNEL_QUARTER) {
            ar9300SetQuarterRateTable(asc);
        } else { /* full rate */
            ar9300SetFullRateTable(asc);
        }
#endif  /* #ifndef ATH_NO_5G_SUPPORT */
        break;
#endif
    default:        /* XXX 5210 */
        break;
    }

    /*
     * Calculate index of any fixed rate configured.  It is safe
     * to do this only here as changing/setting the fixed rate
     * causes the 802.11 state machine to transition (which causes
     * us to be notified).
     */
    avap->rateTable = asc->hwRateTable[sc->sc_curmode];

    if (avp->av_config.av_fixed_rateset != IEEE80211_FIXED_RATE_NONE) {
        asc->fixedrix = sc->sc_rixmap[avp->av_config.av_fixed_rateset & 0xff];
        /* NB: check the fixed rate exists */
        if (asc->fixedrix == 0xff)
            asc->fixedrix = IEEE80211_FIXED_RATE_NONE;
    } else
        asc->fixedrix = IEEE80211_FIXED_RATE_NONE;

    if (up) {
        /* Notify net80211 layer */
        sc->sc_ieee_ops->rate_newstate(sc->sc_ieee, avp->av_if_data);
    }
}

#ifdef AH_SUPPORT_AR5212
void
atheros_setuptable(RATE_TABLE *rt)
{
    int i;

    for (i = 0; i < RATE_TABLE_SIZE; i++)
        rt->rateCodeToIndex[i] = (u_int8_t) -1;
    for (i = 0; i < rt->rateCount; i++) {
        u_int8_t code = rt->info[i].rateCode;

        /* Do not re-initialize rateCodeToIndex when using combined
         * base + turbo rate tables. i.e the rateCodeToIndex should
         * always point to base rate index. The ratecontrol module
         * adjusts the index based on turbo mode.
         */
        if(rt->rateCodeToIndex[code] == (u_int8_t) -1) {
            rt->rateCodeToIndex[code] = i;
            rt->rateCodeToIndex[ code | rt->info[i].shortPreamble] = i;
        }
    }
}
#endif

int
ath_rate_table_init(void)
{
#ifdef AH_SUPPORT_AR5212
    ar5212SetupRateTables();
    /* ar5416SetupRateTables(); */
#endif
    return 0;
}

/* 11N speicific routines */

/*
 * Return the next series 0 transmit rate and setup for a callback
 * to install the multi-rate transmit data if appropriate.  We cannot
 * install the multi-rate transmit data here because the caller is
 * going to initialize the tx descriptor and so would clobber whatever
 * we write. Note that we choose an arbitrary series 0 try count to
 * insure we get called back; this permits us to defer calculating
 * the actual number of tries until the callback at which time we
 * can just copy the pre-calculated series data.
 */

static void
ath_rate_findrate_11n(struct ath_softc *sc,
                      struct ath_node *an,
                      size_t frameLen,
                      int numTries,
                      int numRates,
                      unsigned int rcflag,
                      u_int8_t ac,
                      struct ath_rc_series series[],
                      int *isProbe,
                      int isretry,
                      u_int32_t bf_flags,
                      struct ath_rc_pp *rc_pp)
{
    struct atheros_node    *oan = ATH_NODE_ATHEROS(an);
    struct ath_vap         *avp = oan->avp->athdev_vap;
#if ATH_SUPPORT_WIFIPOS
    ieee80211_wifipos_reqdata_t *data = (ieee80211_wifipos_reqdata_t *)an->an_wifipos_data;
    int rateset = 0;
    int retryset = 0;
#endif

    if (!numRates || !numTries) {
        return;
    }

#if ATH_SUPPORT_WIFIPOS
    if (data) {
        rateset = data->rateset;
        retryset = data->retryset;
    }
#endif

    if (avp->av_config.av_fixed_rateset == IEEE80211_FIXED_RATE_NONE
#if ATH_SUPPORT_WIFIPOS
	    && (!(bf_flags & HAL_TXDESC_POS))
#endif
        && (!rc_pp || (rc_pp && !rc_pp->rate))
    	) {
#if ATH_SUPPORT_IQUE
        oan->rcFunc[ac].rcFind(sc, an, ac, numTries, numRates, rcflag, series, isProbe, isretry);
#else
        rcRateFind_11n(sc, an, ac, numTries, numRates, rcflag, series, isProbe, isretry);
#endif
    } else {
        /* Fixed rate for node or fixed rate per packet */
        int idx;
        A_UINT16 flags;
        A_UINT32 rix;
        A_UINT8 num_series = 4;
        struct atheros_softc *asc = oan->asc;
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
                TX_RATE_CTRL          *pRc = (TX_RATE_CTRL*)&(oan->txRateCtrl);
 #endif        
        const RATE_TABLE_11N *pRateTable = (const RATE_TABLE_11N *)asc->hwRateTable[sc->sc_curmode];

        if (rc_pp && rc_pp->rate)
               num_series = 1;

        for (idx = 0; idx < num_series; idx++) {
            unsigned int    mcs;
            unsigned int    maxRateCode;

#if ATH_SUPPORT_WIFIPOS
            if (rateset && (bf_flags & HAL_TXDESC_POS)) {
                series[idx].tries =
                    IEEE80211_RATE_IDX_ENTRY(data->retryset, idx);
                mcs = IEEE80211_RATE_IDX_ENTRY(data->rateset, idx);
            } else {
#endif

            	if(rc_pp && rc_pp->rate) {
             	/* Only one rate is passed per packet if rate is passed from UMAC  
               	   So set only one entry in rate series */
                	mcs = rc_pp->rate;
                	series[idx].tries = rc_pp->tries;
            	} else {
                	series[idx].tries =
                    	IEEE80211_RATE_IDX_ENTRY(avp->av_config.av_fixed_retryset, idx);
                	mcs = IEEE80211_RATE_IDX_ENTRY(avp->av_config.av_fixed_rateset, idx);
            	}
#if ATH_SUPPORT_WIFIPOS
            }
#endif
 #ifdef ATH_SUPPORT_TxBF
                //swap rate at sounding frame
            if ((sc->sc_txbfsounding) && (VALID_TXBF_RATE(mcs, oan->usedNss))){
                const u_int8_t  *sounding_swap_table;
                if ((sc->sc_curmode == WIRELESS_MODE_11NA_HT20) 
                    ||(sc->sc_curmode == WIRELESS_MODE_11NA_HT40PLUS) 
                    ||(sc->sc_curmode == WIRELESS_MODE_11NA_HT40MINUS))
                {    //A_band
                    if (series[0].flags & ATH_RC_CW40_FLAG ){
                        // 40M 
                        sounding_swap_table = sounding_swap_table_A_40;
                    } else {
                        sounding_swap_table = sounding_swap_table_A_20;
                    }
                } else {
                    // g band table
                    if (series[0].flags & ATH_RC_CW40_FLAG ){
                        // 40M 
                        sounding_swap_table = sounding_swap_table_G_40;
                    } else {
                        sounding_swap_table = sounding_swap_table_G_20; 
                    }
                }
                // swap rate
                mcs = sounding_swap_table[(mcs & SWAP_RATE)];       
            }
#endif

            if (idx == 3 && (mcs & 0xf0) == 0x70)
                mcs = (mcs & ~0xf0)|0x80;

            if (!(mcs & 0x80)) {
                flags = 0;
            } else {
                int i, SGIenable = 0;
                
                /*
                 * Enable SGI according to PHY state of current mcs rate
                 * and user configuration.
                 */
                for (i = 0; i< pRateTable->rateCount; i++) {
                    if (mcs == pRateTable->info[i].rateCode &&
                        WLAN_RC_PHY_SGI(pRateTable->info[i].phy) &&
                        avp->av_config.av_short_gi)
                    {
                        SGIenable = 1; //enable SGI
                        break;
                    }
                }
                
                flags = ((oan->htcap & WLAN_RC_DS_FLAG) ? ATH_RC_DS_FLAG : 0) |
                        ((oan->htcap & WLAN_RC_TS_FLAG) ? ATH_RC_TS_FLAG : 0) |
                        ((oan->htcap & WLAN_RC_40_FLAG) ? ATH_RC_CW40_FLAG : 0) |
                        ((oan->htcap & WLAN_RC_SGI_FLAG) && SGIenable ? ATH_RC_SGI_FLAG : 0);

                if (oan->stbc) {
                    /* For now, only single stream STBC is supported */
                    if (mcs >= 0x80 && mcs <= 0x87)
                        flags |= ATH_RC_TX_STBC_FLAG;
                }
#ifdef ATH_SUPPORT_TxBF
                rix = sc->sc_rixmap[mcs];
                if (oan->txbf && VALID_TXBF_RATE(mcs, oan->usedNss) &&
                    !(asc->sc_txbf_disable_flag[rix][sc->sc_tx_numchains - 1])) {
                    flags |= ATH_RC_TXBF_FLAG;
                    flags &= ~(ATH_RC_TX_STBC_FLAG);                    
                }
                if (sc->sc_txbfsounding) {
                    flags &=~(ATH_RC_SGI_FLAG);     //disable SGI at sounding
                    flags |= ATH_RC_SOUNDING_FLAG;
                }
#endif
            }

            series[idx].rix = sc->sc_rixmap[mcs];
            /* Rate count cannot be zero */
            KASSERT(pRateTable->rateCount,("Rate count cannot be zero"));
            /*
             * Check if the fixed rate is valid for this mode.
             * If invalid then set the rate index to max rate code index
             */
            maxRateCode = pRateTable->info[pRateTable->rateCount-1].rateCode;
            if(series[idx].rix == 0xFF) {
                mcs = maxRateCode;
                series[idx].rix = sc->sc_rixmap[mcs];
            }

            if ((flags & ATH_RC_CW40_FLAG) && (flags & ATH_RC_SGI_FLAG)) {
                rix = pRateTable->info[series[idx].rix].htIndex;
            } else if (flags & ATH_RC_SGI_FLAG) {
                rix = pRateTable->info[series[idx].rix].sgiIndex;
            } else if (flags & ATH_RC_CW40_FLAG) {
                rix = pRateTable->info[series[idx].rix].cw40Index;
            } else {
                rix = pRateTable->info[series[idx].rix].baseIndex;
            }
            series[idx].max4msframelen = pRateTable->info[rix].max4msframelen;
            series[idx].flags = flags;
        }
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
        pRc->TxRateInMbps += pRateTable->info[rix].rateKbps / 1000;
		pRc->Max4msFrameLen += pRateTable->info[rix].max4msframelen;
		pRc->TxRateCount += 1;
#endif
    }
}

/*
 * Process a tx descriptor for a completed transmit (success or failure).
 */
#ifdef ATH_SUPPORT_VOWEXT
void
ath_rate_tx_complete_11n(struct ath_softc *sc,
                         struct ath_node *an,
                         struct ath_tx_status *ts,
                         struct ath_rc_series rcs[],
                         u_int8_t ac,
                         int nframes,
                         int nbad,
                         int nHeadFail,
                         int nTailFail,
                         int rts_retry_limit,
                         struct ath_rc_pp *pp_rcs)
#else
void
ath_rate_tx_complete_11n(struct ath_softc *sc,
                         struct ath_node *an,
                         struct ath_tx_status *ts,
                         struct ath_rc_series rcs[],
                         u_int8_t ac,
                         int nframes,
                         int nbad,
                         int rts_retry_limit,
                         struct ath_rc_pp *pp_rcs)
#endif
{
    int    finalTSIdx = ts->ts_rateindex;
    int    tx_status = 0, is_underrun = 0;
    struct atheros_node    *oan = ATH_NODE_ATHEROS(an);
    struct ath_vap         *avp = oan->avp->athdev_vap;
    
#if UNIFIED_SMARTANTENNA                   
    if ((rcs[0].flags & ATH_RC_TRAINING_FLAG)) { 
         /* do not update RC stats for this */
        return;
    }
#endif


#ifdef ATH_SUPPORT_TxBF
    if (rcs[0].flags & ATH_RC_SOUNDING_FLAG) {
      /* indicate sounding sent*/ 
      ts->ts_txbfstatus |= TxBF_STATUS_Sounding_Complete;
    }
    if (oan->lastTxmcs0cnt > MCS0_80_PERCENT ){
        /* clear TxBF HW status to avoid trigger sounding*/
        ts->ts_txbfstatus &= ~(AR_TxBF_Valid_HW_Status);
    }
#endif

    if(unlikely(!sc->sc_nodebug)){
    RcUpdate_TxBF_STATS(sc, rcs, ts);
    }

    if ((avp->av_config.av_fixed_rateset != IEEE80211_FIXED_RATE_NONE)
            || ts->ts_status & HAL_TXERR_FILT 
            || (pp_rcs && pp_rcs->rate)) {
        return;
    }

#ifdef ATH_CHAINMASK_SELECT
    if (ts->ts_rssi > 0) {
        ATH_RSSI_LPF(an->an_chainmask_sel.tx_avgrssi,
                             ts->ts_rssi);
    }
#endif

    /*
     * If underrun error is seen assume it as an excessive retry only if prefetch
     * trigger level have reached the max (0x3f for 5416)
     * Adjust the long retry as if the frame was tried ATH_11N_TXMAXTRY times. This
     * affects how ratectrl updates PER for the failed rate.
     */
    if ((HAL_IS_TX_UNDERRUN(ts)) &&
        (ath_hal_gettxtriglevel(sc->sc_ah) >= sc->sc_rc->txTrigLevelMax)) {
        tx_status = 1;
        is_underrun = 1;
    }

    if (ts->ts_status & (HAL_TXERR_XRETRY |HAL_TXERR_FIFO))
    {
        tx_status = 1;
    }
    
#ifdef ATH_SUPPORT_VOWEXT
#if ATH_SUPPORT_IQUE
    oan->rcFunc[ac].rcUpdate(sc, an, ts->ts_rssi, ac,
                        finalTSIdx, tx_status, rcs, nframes , nbad,
                        (is_underrun) ? ATH_11N_TXMAXTRY:ts->ts_longretry, ts->ts_shortretry, nHeadFail, nTailFail);
#else
    rcUpdate_11n(sc, an, ts->ts_rssi , ac,
                    finalTSIdx, tx_status, rcs, nframes , nbad,
                    (is_underrun) ? ATH_11N_TXMAXTRY:ts->ts_longretry, ts->ts_shortretry, nHeadFail, nTailFail);
#endif
#else
#if ATH_SUPPORT_IQUE
    oan->rcFunc[ac].rcUpdate(sc, an, ts->ts_rssi, ac,
                        finalTSIdx, tx_status, rcs, nframes , nbad,
                        (is_underrun) ? ATH_11N_TXMAXTRY:ts->ts_longretry, ts->ts_shortretry);
#else
    rcUpdate_11n(sc, an, ts->ts_rssi , ac,
                    finalTSIdx, tx_status, rcs, nframes , nbad,
                    (is_underrun) ? ATH_11N_TXMAXTRY:ts->ts_longretry, ts->ts_shortretry);
#endif
#endif

#ifdef ATH_SUPPORT_TxBF
    if (oan->txbf_sounding_request){
        ts->ts_txbfstatus |= TxBF_STATUS_Sounding_Request;
        oan->txbf_sounding_request = 0;
    } else {
        ts->ts_txbfstatus &= ~(TxBF_STATUS_Sounding_Request);
    }
#endif
}

/*
 * Update rate-control state on station associate/reassociate.
 */
static int
ath_rate_newassoc_11n(struct ath_softc *sc, struct ath_node *an, int isnew,
                      unsigned int capflag,
                      struct ieee80211_rateset *negotiated_rates,
                      struct ieee80211_rateset *negotiated_htrates)
{
    struct atheros_node    *oan = ATH_NODE_ATHEROS(an);

    if (isnew) {
        u_int32_t node_maxRate = (u_int32_t) (-1);
        MAX_RATES maxRates;
        
        OS_MEMZERO(&maxRates, sizeof(maxRates));

        /* FIX ME:XXXX Looks like this not used at all.    */
        oan->htcap =
                    ((capflag & ATH_RC_DS_FLAG) ? WLAN_RC_DS_FLAG : 0) |
                    ((capflag & ATH_RC_TS_FLAG) ? WLAN_RC_TS_FLAG : 0) |
                    ((capflag & ATH_RC_SGI_FLAG) ? WLAN_RC_SGI_FLAG : 0) |
                    ((capflag & ATH_RC_HT_FLAG)  ? WLAN_RC_HT_FLAG : 0) |
#ifdef ATH_SUPPORT_TxBF
                    ((capflag & ATH_RC_TXBF_FLAG) ? WLAN_RC_TxBF_FLAG:0)|     
#endif
                    ((capflag & ATH_RC_CW40_FLAG) ? WLAN_RC_40_FLAG : 0);

        /* Rx STBC is a 2-bit mask. Needs to convert from ath definition to wlan definition. */
        oan->htcap |= (((capflag & ATH_RC_RX_STBC_FLAG) >> ATH_RC_RX_STBC_FLAG_S)
                       << WLAN_RC_STBC_FLAG_S);

        /* Enable stbc only for more than one tx chain */
        if (sc->sc_txstbcsupport && (sc->sc_tx_chainmask != 1)) {
            oan->stbc = (capflag & ATH_RC_RX_STBC_FLAG) >> ATH_RC_RX_STBC_FLAG_S;
        } else {
            oan->stbc = 0;
        }

        /* Enable txbf if supported */
#ifdef ATH_SUPPORT_TxBF
        oan->txbf = (capflag & ATH_RC_TXBF_FLAG) ? 1 : 0;
        oan->htcap |= (((capflag & ATH_RC_CEC_FLAG) >> ATH_RC_CEC_FLAG_S)
                       << WLAN_RC_CEC_FLAG_S);
#endif

        if (sc->sc_ieee_ops->get_htmaxrate) {
            node_maxRate = 1000 * sc->sc_ieee_ops->get_htmaxrate(an->an_node);
            if (!node_maxRate)
                node_maxRate = (u_int32_t) (-1);
        }

        if (capflag & ATH_RC_WEP_TKIP_FLAG) {
            maxRates.max_ht20_tx_rateKbps = (sc->sc_max_wep_tkip_ht20_tx_rateKbps < node_maxRate) ?
                                            sc->sc_max_wep_tkip_ht20_tx_rateKbps : node_maxRate;
            maxRates.max_ht40_tx_rateKbps = (sc->sc_max_wep_tkip_ht40_tx_rateKbps < node_maxRate) ?
                                            sc->sc_max_wep_tkip_ht40_tx_rateKbps : node_maxRate;
        } else {
            maxRates.max_ht20_tx_rateKbps = node_maxRate;
            maxRates.max_ht40_tx_rateKbps = node_maxRate;
        }

        rcSibUpdate_11n(sc, an, oan->htcap, 0, negotiated_rates,
                negotiated_htrates, &maxRates);

#if ATH_SUPPORT_IQUE
        rcSibUpdate_11nViVo(sc, an, oan->htcap, 0, negotiated_rates,
                negotiated_htrates, &maxRates);
#endif

        /*
         * Set an initial tx rate for the net80211 layer.
         * Even though noone uses it, it wants to validate
         * the setting before entering RUN state so if there
         * was a pervious setting from a different node it
         * may be invalid.
         */

        if (sc->sc_ieee_ops->update_txrate) {
            sc->sc_ieee_ops->update_txrate(an->an_node, 0);
        }
    }

    return 0;
}

u_int8_t ath_get_tx_chainmask(struct ath_softc *sc)
{
    return sc->sc_tx_chainmask;
}

u_int8_t
ath_rate_max_tx_chains(struct ath_softc *sc, u_int8_t rix, u_int16_t bfrcsflags)
{
    if (sc->sc_rc->ah_magic == 0x19641014 ||
        sc->sc_rc->ah_magic == 0x19741014)
    {
        struct atheros_softc *asc = (struct atheros_softc*)sc->sc_rc;
        const RATE_TABLE_11N *pRateTable;

        pRateTable = (const RATE_TABLE_11N*)asc->hwRateTable[sc->sc_curmode];

       if (sc->sc_cddLimitingEnabled) {
            /*
             * EV 66771:
             * Use single tx chain for CDD channels (36,40,44,48)
             * at legacy rates
             */
            if (!(pRateTable->info[rix].rateCode & 0x80)) {
                return 1;
            }
        }
#if ATH_SUPPORT_DYN_TX_CHAINMASK
        /*
         * For the dynamic tx chainmask feature, the number of chains
         * used for 64-QAM rates should not exceed the rate's number
         * of spatial streams.
         * This limit is stored in the rate table's maxTxChains column.
         * dynamic tx chainmask should be exclusive to txbf and stbc
         */
        if(sc->sc_dyn_txchainmask                 &&
#ifdef ATH_SUPPORT_TxBF
           !(bfrcsflags & ATH_RC_TXBF_FLAG)       &&
           !(bfrcsflags & ATH_RC_SOUNDING_FLAG)   &&
#endif
           !(bfrcsflags & ATH_RC_TX_STBC_FLAG))
        {
            return pRateTable->info[rix].maxTxChains;
        }
#endif /* ATH_SUPPORT_DYN_TX_CHAINMASK */
    }
    /*
     * No restriction on the number of chains for legacy chips,
     * or if the restrictions above don't apply.
     */
    return sc->sc_tx_numchains;
}

#ifdef __linux__
#ifndef ATH_WLAN_COMBINE
/*
 * Linux module glue
 */
static char *dev_info = "ath_rate_atheros";

MODULE_AUTHOR("Atheros Communications, Inc.");
MODULE_DESCRIPTION("Rate control support for Atheros devices");
#ifdef MODULE_LICENSE
MODULE_LICENSE("Proprietary");
#endif

static int __init
init_ath_rate_atheros(void)
{
    /* XXX version is a guess; where should it come from? */
    printk(KERN_INFO "%s: "
           "Copyright (c) 2001-2005 Atheros Communications, Inc, "
           "All Rights Reserved\n", dev_info);
    return 0;
}
module_init(init_ath_rate_atheros);

static void __exit
exit_ath_rate_atheros(void)
{
    printk(KERN_INFO "%s: driver unloaded\n", dev_info);
}
module_exit(exit_ath_rate_atheros);

#ifndef EXPORT_SYMTAB
#define    EXPORT_SYMTAB
#endif

EXPORT_SYMBOL(ath_rate_maprix);
EXPORT_SYMBOL(ath_rate_set_mcast_rate);
EXPORT_SYMBOL(ath_rate_table_init);
EXPORT_SYMBOL(ath_rate_attach);
EXPORT_SYMBOL(ath_rate_detach);
EXPORT_SYMBOL(ath_rate_create_vap);
EXPORT_SYMBOL(ath_rate_free_vap);
EXPORT_SYMBOL(ath_rate_node_alloc);
EXPORT_SYMBOL(ath_rate_node_free);
EXPORT_SYMBOL(ath_rate_node_init);
EXPORT_SYMBOL(ath_rate_node_cleanup);
EXPORT_SYMBOL(ath_rate_newassoc);
#if QCA_AIRTIME_FAIRNESS
EXPORT_SYMBOL(ath_rate_atf_airtime_estimate);
#endif
EXPORT_SYMBOL(ath_rate_node_gettxrate);
EXPORT_SYMBOL(ath_rate_getmaxphyrate);
EXPORT_SYMBOL(ath_rate_newstate);
EXPORT_SYMBOL(ath_rate_findrate);
EXPORT_SYMBOL(ath_rate_node_update);
EXPORT_SYMBOL(ath_rate_findrateix);
EXPORT_SYMBOL(ath_rate_tx_complete);
EXPORT_SYMBOL(ath_rate_tx_complete_11n);
EXPORT_SYMBOL(ath_get_tx_chainmask);
EXPORT_SYMBOL(ath_rate_max_tx_chains);
#endif /* #ifndef ATH_WLAN_COMBINE */

#endif

#ifdef ATH_CCX
u_int8_t
ath_rcRateValueToPer(ath_dev_t dev, struct ath_node *an, int txRateKbps)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    switch (sc->sc_rc->ah_magic) {
    case 0x19570405:
    case 0x19541014:    /* 5212 */
        return(rcRateValueToPer(sc, an, txRateKbps));
    case 0x19641014:    /* 5416 */
    case 0x19741014:    /* 9300 */
        return(rcRateValueToPer_11n(sc, an, txRateKbps));
    default:
        ASSERT(0);
        return(100);
    }

}
#endif

/*
 * This routine is called to map the baseIndex to the rate in the RATE_TABLE
 */
u_int32_t
ath_rate_maprix(struct ath_softc *sc, u_int16_t curmode, int isratecode)
{
    return (((struct atheros_softc *)sc->sc_rc)->prate_maprix)((struct atheros_softc *)sc->sc_rc, 
              curmode,  sc->sc_lastdatarix, sc->sc_lastrixflags, isratecode);
}

int32_t
ath_rate_set_mcast_rate(struct ath_softc *sc, u_int32_t req_rate)
{
    const HAL_RATE_TABLE *rt;
    int i, mcast_rate;

    rt = sc->sc_currates;
    for (i = 0; i < rt->rateCount; i++) {
        if (req_rate == (rt->info[i].rateKbps/1000)) {
            /*
             * Requested rate is a valid rate
             */
            mcast_rate = rt->info[i].dot11Rate & IEEE80211_RATE_VAL;
            return (mcast_rate);
        }
    }

    return -1;
}

#if UNIFIED_SMARTANTENNA
void
ath_smart_ant_set_fixedrate(struct ath_softc *sc, struct ath_node *an,
                   int shortPreamble, u_int32_t *rate_array, u_int32_t *antenna_array,
                   unsigned int rcflag, u_int8_t ac, struct ath_rc_series series[],
                   int *isProbe, int isretry, u_int32_t bf_flags)
{
    int i = 0,j = 0 ;
    u_int8_t rate_array_index[4];
    struct atheros_node *oan = an->an_rc_node;
    struct atheros_softc *asc = oan->asc;
    TX_RATE_CTRL *pRc  = (TX_RATE_CTRL *)(oan);
    const RATE_TABLE_11N *pRateTable;
    pRateTable = (const RATE_TABLE_11N *)asc->hwRateTable[sc->sc_curmode];

    ASSERT(series != NULL);

    if (sc->sc_rc->ah_magic == 0x19641014 ||
        sc->sc_rc->ah_magic == 0x19741014)
    {
        
        /* TODO: each node should contain valid RateTable for SA Train, SA module should send rateIndex, Below code can be removed after that */
        for (j = 0; j < 4; j++) {
            for(i = 0; i < pRc->maxValidRate; i++) {
                if ( rate_array[j] == pRateTable->info[pRc->validRateIndex[i]].rateCode) {
                    break;
                }
            }

            rate_array_index[j]=  pRc->validRateIndex[i];
        }

        rcSmartAnt_SetRate_11n(sc, an, series, rate_array_index, antenna_array, bf_flags);
        return;

    }

}
EXPORT_SYMBOL(ath_smart_ant_set_fixedrate);

/**  
 *   ath_smart_ant_rate_prepare_rateset: This function prepares valid rate index array i.e negotiated rates at the time of association for a particular node and fill the
 *      valid rate index array from the caller.
 *   @sc: handle to ath_softc object.
     @an: handle to node.
     @prate_info: pointer to ath_smartant_rate info.
 *   @valid_ratearray: pionter to valid rateindex array.
 *
 *   There is no return value 
 */
void ath_smart_ant_rate_prepare_rateset(struct ath_softc *sc, struct ath_node *an, void *prate_info)
{
    struct atheros_node *oan = an->an_rc_node;
    struct atheros_softc *asc = oan->asc;
    struct sa_rate_info *rate_info = (struct sa_rate_info *)prate_info;
    TX_RATE_CTRL *pRc  = (TX_RATE_CTRL *)(oan);
    const RATE_TABLE_11N *pRateTable;
    u_int8_t prev_rtcode = 0;
    int i = 0 ;
    pRateTable = (const RATE_TABLE_11N *)asc->hwRateTable[sc->sc_curmode];

    if (sc->sc_rc->ah_magic == 0x19641014 ||
            sc->sc_rc->ah_magic == 0x19741014)
    {
        rate_info->num_of_rates = 0;

        for(i = 0; i < pRc->maxValidRate;i++) {
            /* if same rate code exists for two different index */
            if ((i) && (prev_rtcode == pRateTable->info[pRc->validRateIndex[i]].rateCode)) {
                continue;
            }

	    prev_rtcode = pRateTable->info[pRc->validRateIndex[i]].rateCode;
            rate_info->rates[rate_info->num_of_rates].ratecode =  pRateTable->info[pRc->validRateIndex[i]].rateCode;
            rate_info->rates[rate_info->num_of_rates].rateindex = pRc->validRateIndex[i];
            rate_info->num_of_rates++;
        }
    } else {
        return ;
    }
}
EXPORT_SYMBOL(ath_smart_ant_rate_prepare_rateset);
#endif
