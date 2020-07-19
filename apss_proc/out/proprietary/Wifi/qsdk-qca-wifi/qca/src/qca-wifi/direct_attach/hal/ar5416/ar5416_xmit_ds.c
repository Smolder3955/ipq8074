/*
* Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * Notifications and licenses are retained for attribution purposes only.
 */
/*
 * Copyright (c) 2002-2005 Sam Leffler, Errno Consulting
 * Copyright (c) 2002-2005 Atheros Communications, Inc.
 * Copyright (c) 2010, Atheros Communications Inc. 
 * 
 * Redistribution and use in source and binary forms are permitted
 * provided that the following conditions are met:
 * 1. The materials contained herein are unmodified and are used
 *    unmodified.
 * 2. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following NO
 *    ''WARRANTY'' disclaimer below (''Disclaimer''), without
 *    modification.
 * 3. Redistributions in binary form must reproduce at minimum a
 *    disclaimer similar to the Disclaimer below and any redistribution
 *    must be conditioned upon including a substantially similar
 *    Disclaimer requirement for further binary redistribution.
 * 4. Neither the names of the above-listed copyright holders nor the
 *    names of any contributors may be used to endorse or promote
 *    product derived from this software without specific prior written
 *    permission.
 * 
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ''AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT,
 * MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE
 * FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGES.
 * 
 */

#include "opt_ah.h"

#ifdef AH_SUPPORT_AR5416

#include "ah.h"
#include "ah_desc.h"
#include "ah_internal.h"

#include "ar5416/ar5416.h"
#include "ar5416/ar5416reg.h"
#include "ar5416/ar5416desc.h"
#include "ar5416/ar5416phy.h"

#ifdef AH_NEED_DESC_SWAP
static void ar5416SwapTxDesc(void *ds);
#endif

void
ar5416TxReqIntrDesc(struct ath_hal *ah, void *ds)
{
    struct ar5416_desc *ads = AR5416DESC(ds);
    ads->ds_ctl0 |= AR_TxIntrReq;
}

HAL_BOOL
ar5416SetupTxDesc(struct ath_hal *ah, struct ath_desc *ds,
        u_int pktLen,
        u_int hdrLen,
        HAL_PKT_TYPE type,
        u_int txPower,
        u_int txRate0, u_int txTries0,
        u_int key_ix,
        u_int antMode,
        u_int flags,
        u_int rts_cts_rate,
        u_int rts_cts_duration,
        u_int compicvLen,
        u_int compivLen,
        u_int comp)
{
#define RTSCTS  (HAL_TXDESC_RTSENA|HAL_TXDESC_CTSENA)
        struct ar5416_desc *ads = AR5416DESC(ds);

        (void) hdrLen;

        ads->ds_txstatus9 &= ~AR_TxDone;

        HALASSERT(txTries0 != 0);
        HALASSERT(isValidPktType(type));
        HALASSERT(isValidTxRate(txRate0));
        HALASSERT((flags & RTSCTS) != RTSCTS);
        /* XXX validate antMode */

        /*
         * If descriptor-based tpc, tx power is controlled by
         * ar5416Set11nRateScenario().
         * If not, what you put in the descriptor is ignored anyway.
         */
        txPower = 0;

        ads->ds_ctl0 = (pktLen & AR_FrameLen)
                     | (txPower << AR_XmitPower0_S)
                     | (flags & HAL_TXDESC_VEOL ? AR_VEOL : 0)
                     | (flags & HAL_TXDESC_CLRDMASK ? AR_ClrDestMask : 0)
                     | (flags & HAL_TXDESC_INTREQ ? AR_TxIntrReq : 0)
                     ;
        ads->ds_ctl1 = (type << AR_FrameType_S)
                     | (flags & HAL_TXDESC_NOACK ? AR_NoAck : 0);
        ads->ds_ctl2 = SM(txTries0, AR_XmitDataTries0)
                     ;
        ads->ds_ctl3 = (txRate0 << AR_XmitRate0_S)
                     ;

        ads->ds_ctl7 = SM(AR5416_LEGACY_CHAINMASK, AR_ChainSel0)
                     | SM(AR5416_LEGACY_CHAINMASK, AR_ChainSel1)
                     | SM(AR5416_LEGACY_CHAINMASK, AR_ChainSel2)
                     | SM(AR5416_LEGACY_CHAINMASK, AR_ChainSel3)
                     ;

        if (key_ix != HAL_TXKEYIX_INVALID) {
                /* XXX validate key index */
                ads->ds_ctl1 |= SM(key_ix, AR_DestIdx);
                ads->ds_ctl0 |= AR_DestIdxValid;
        }
        if (flags & RTSCTS) {
                if (!isValidTxRate(rts_cts_rate)) {
                        HDPRINTF(ah, HAL_DBG_TXDESC, "%s: invalid rts/cts rate 0x%x\n",
                                __func__, rts_cts_rate);
                        return AH_FALSE;
                }
                /* XXX validate rts_cts_duration */
                ads->ds_ctl0 |= (flags & HAL_TXDESC_CTSENA ? AR_CTSEnable : 0)
                             | (flags & HAL_TXDESC_RTSENA ? AR_RTSEnable : 0)
                             ;
                ads->ds_ctl2 |= SM(rts_cts_duration, AR_BurstDur);
                ads->ds_ctl3 |= (rts_cts_rate << AR_RTSCTSRate_S);
        }
        return AH_TRUE;
#undef RTSCTS
}

HAL_BOOL
ar5416FillTxDesc(struct ath_hal *ah, void *ds, dma_addr_t *bufAddr,
        u_int32_t *seg_len, u_int desc_id, u_int qcu, HAL_KEY_TYPE keyType, HAL_BOOL first_seg,
        HAL_BOOL last_seg, const void *ds0)
{
        struct ar5416_desc *ads = AR5416DESC(ds);

        HALASSERT((seg_len[0] &~ AR_BufLen) == 0);
		OS_MEMZERO(&(ads->u.tx.tx_status), sizeof(ads->u.tx.tx_status));
        /* Set the buffer addresses */
        ads->ds_data = bufAddr[0];

        if (first_seg) {
                /*
                 * First descriptor, don't clobber xmit control data
                 * setup by ar5416SetupTxDesc.
                 */
                ads->ds_ctl1 |= seg_len[0] | (last_seg ? 0 : AR_TxMore);
        } else if (last_seg) {           /* !first_seg && last_seg */
                /*
                 * Last descriptor in a multi-descriptor frame,
                 * copy the multi-rate transmit parameters from
                 * the first frame for processing on completion.
                 */
                ads->ds_ctl0 = 0;
                ads->ds_ctl1 = seg_len[0];
#ifdef AH_NEED_DESC_SWAP
                ads->ds_ctl2 = __bswap32(AR5416DESC_CONST(ds0)->ds_ctl2);
                ads->ds_ctl3 = __bswap32(AR5416DESC_CONST(ds0)->ds_ctl3);
#else
                ads->ds_ctl2 = AR5416DESC_CONST(ds0)->ds_ctl2;
                ads->ds_ctl3 = AR5416DESC_CONST(ds0)->ds_ctl3;
#endif
        } else {                        /* !first_seg && !last_seg */
                /*
                 * Intermediate descriptor in a multi-descriptor frame.
                 */
                ads->ds_ctl0 = 0;
                ads->ds_ctl1 = seg_len[0] | AR_TxMore;
                ads->ds_ctl2 = 0;
                ads->ds_ctl3 = 0;
        }
        return AH_TRUE;
}

void
ar5416SetDescLink(struct ath_hal *ah, void *ds, u_int32_t link)
{
    struct ar5416_desc *ads = AR5416DESC(ds);

    ads->ds_link = link;
}

void
ar5416GetDescLinkPtr(struct ath_hal *ah, void *ds, u_int32_t **link)
{
    struct ar5416_desc *ads = AR5416DESC(ds);

    *link = &ads->ds_link;
}

void
ar5416ClearTxDescStatus(struct ath_hal *ah, void *ds)
{
    struct ar5416_desc *ads = AR5416DESC(ds);
	OS_MEMZERO(&(ads->u.tx.tx_status), sizeof(ads->u.tx.tx_status));
}

#ifdef ATH_SWRETRY
void 
ar5416ClearDestMask(struct ath_hal *ah, void *ds)
{
    struct ar5416_desc *ads = AR5416DESC(ds);
    ads->ds_ctl0 |= AR_ClrDestMask;
}
#endif

#ifdef AH_NEED_DESC_SWAP
/* Swap transmit descriptor */
static __inline void
ar5416SwapTxDesc(void *ds)
{
        ds->ds_data = __bswap32(ds->ds_data);
        ds->ds_ctl0 = __bswap32(ds->ds_ctl0);
        ds->ds_ctl1 = __bswap32(ds->ds_ctl1);
        ds->ds_hw[0] = __bswap32(ds->ds_hw[0]);
        ds->ds_hw[1] = __bswap32(ds->ds_hw[1]);
        ds->ds_hw[2] = __bswap32(ds->ds_hw[2]);
        ds->ds_hw[3] = __bswap32(ds->ds_hw[3]);
}
#endif

/*
 * Processing of HW TX descriptor.
 */
HAL_STATUS
ar5416ProcTxDesc(struct ath_hal *ah, void *desc)
{
        struct ar5416_desc *ads = AR5416DESC(desc);
        struct ath_desc *ds = (struct ath_desc *)desc;
		struct ath_tx_status *ntxstat;
#ifdef AH_NEED_DESC_SWAP
        if ((ads->ds_txstatus9 & __bswap32(AR_TxDone)) == 0)
                return HAL_EINPROGRESS;

        ar5416SwapTxDesc(ds);
#else
        if ((ads->ds_txstatus9 & AR_TxDone) == 0)
                return HAL_EINPROGRESS;
#endif
		/*
		 * Use a local copy of ds/ads in the cacheable memory to
		 * improve the d-cache efficiency
		 */
		ntxstat = &(ds->ds_txstat);
        /* Update software copies of the HW status */
        ntxstat->ts_seqnum = MS(ads->ds_txstatus9, AR_SeqNum);
        ntxstat->ts_tstamp = ads->AR_SendTimestamp;
        ntxstat->ts_status = 0;
        ntxstat->ts_flags  = 0;

        if (ads->ds_txstatus1 & AR_ExcessiveRetries)
            ntxstat->ts_status |= HAL_TXERR_XRETRY;
        if (ads->ds_txstatus1 & AR_Filtered)
            ntxstat->ts_status |= HAL_TXERR_FILT;
        if (ads->ds_txstatus1 & AR_FIFOUnderrun) {
            ntxstat->ts_status |= HAL_TXERR_FIFO;
	        ar5416UpdateTxTrigLevel(ah, AH_TRUE);
        }
        if (ads->ds_txstatus9 & AR_TxOpExceeded)
            ntxstat->ts_status |= HAL_TXERR_XTXOP;
        if (ads->ds_txstatus1 & AR_TxTimerExpired)
            ntxstat->ts_status |= HAL_TXERR_TIMER_EXPIRED;

        if (ads->ds_txstatus1 & AR_DescCfgErr)
            ntxstat->ts_flags |= HAL_TX_DESC_CFG_ERR;
        if (ads->ds_txstatus1 & AR_TxDataUnderrun) {
            ntxstat->ts_flags |= HAL_TX_DATA_UNDERRUN;
            ar5416UpdateTxTrigLevel(ah, AH_TRUE);
        }
        if (ads->ds_txstatus1 & AR_TxDelimUnderrun) {
            ntxstat->ts_flags |= HAL_TX_DELIM_UNDERRUN;
            ar5416UpdateTxTrigLevel(ah, AH_TRUE);
        }
        if (ads->ds_txstatus0 & AR_TxBaStatus) {
            ntxstat->ts_flags |= HAL_TX_BA;
            ntxstat->ba_low = ads->AR_BaBitmapLow;
            ntxstat->ba_high = ads->AR_BaBitmapHigh;
        }

        ntxstat->tid = (ads->ds_txstatus9 & 0xf0000000) >>28;
        /*
         * Extract the transmit rate.
         */
        ntxstat->ts_rateindex = MS(ads->ds_txstatus9, AR_FinalTxIdx);
        switch (ntxstat->ts_rateindex) {
        case 0:
            ntxstat->ts_ratecode = MS(ads->ds_ctl3, AR_XmitRate0);
            break;
        case 1:
            ntxstat->ts_ratecode = MS(ads->ds_ctl3, AR_XmitRate1);
            break;
        case 2:
            ntxstat->ts_ratecode = MS(ads->ds_ctl3, AR_XmitRate2);
            break;
        case 3:
            ntxstat->ts_ratecode = MS(ads->ds_ctl3, AR_XmitRate3);
            break;
        }

        ntxstat->ts_rssi =      MS(ads->ds_txstatus5, AR_TxRSSICombined);
        ntxstat->ts_rssi_ctl0 = MS(ads->ds_txstatus0, AR_TxRSSIAnt00);
        ntxstat->ts_rssi_ctl1 = MS(ads->ds_txstatus0, AR_TxRSSIAnt01);
        ntxstat->ts_rssi_ctl2 = MS(ads->ds_txstatus0, AR_TxRSSIAnt02);
        ntxstat->ts_rssi_ext0 = MS(ads->ds_txstatus5, AR_TxRSSIAnt10);
        ntxstat->ts_rssi_ext1 = MS(ads->ds_txstatus5, AR_TxRSSIAnt11);
        ntxstat->ts_rssi_ext2 = MS(ads->ds_txstatus5, AR_TxRSSIAnt12);
        ntxstat->evm0 = ads->AR_TxEVM0;
        ntxstat->evm1 = ads->AR_TxEVM1;
        ntxstat->evm2 = ads->AR_TxEVM2;
        ntxstat->ts_shortretry = MS(ads->ds_txstatus1, AR_RTSFailCnt);
        ntxstat->ts_longretry = MS(ads->ds_txstatus1, AR_DataFailCnt);
        ntxstat->ts_virtcol = MS(ads->ds_txstatus1, AR_VirtRetryCnt);
        ntxstat->ts_antenna = 0;

        return HAL_OK;
}

/*
 * Calculate air time of a transmit packet
 */
u_int32_t
ar5416CalcTxAirtime(struct ath_hal *ah, void *ds, struct ath_tx_status *ts,
        HAL_BOOL comp_wastedt, u_int8_t nbad, u_int8_t nframes)
{
    struct ar5416_desc *ads = AR5416DESC(ds);
    int finalindex_tries;
	int status9;
    /*
     * Number of attempts made on the final index
     * Note: If no BA was recv, then the data_fail_cnt is the number of tries
     * made on the final index.  If BA was recv, then add 1 to account for the
     * successful attempt.
     */
    finalindex_tries = ts->ts_longretry + (ts->ts_flags & HAL_TX_BA)? 1 : 0;

	/*
	 * Use a local copy of the ads in the cacheable memory to
	 * improve the d-cache efficiency.
	 */
	status9 = ads->ds_txstatus9;
	status9 = MS(status9, AR_FinalTxIdx);
    /*
     * Calculate time of transmit on air for packet including retries
     * at different rates.
     */
	if (status9 == 0) {
        return  MS(ads->ds_ctl4, AR_PacketDur0) * finalindex_tries;
	} else if (status9 == 1) {
        return (MS(ads->ds_ctl4, AR_PacketDur1) * finalindex_tries) +
               (MS(ads->ds_ctl2, AR_XmitDataTries0) * MS(ads->ds_ctl4, AR_PacketDur0));
	} else if (status9 == 2) {
        return (MS(ads->ds_ctl5, AR_PacketDur2) * finalindex_tries) +
               (MS(ads->ds_ctl2, AR_XmitDataTries1) * MS(ads->ds_ctl4, AR_PacketDur1)) +
               (MS(ads->ds_ctl2, AR_XmitDataTries0) * MS(ads->ds_ctl4, AR_PacketDur0));
	} else if (status9 == 3) {
        return (MS(ads->ds_ctl5, AR_PacketDur3) * finalindex_tries) +
               (MS(ads->ds_ctl2, AR_XmitDataTries2) * MS(ads->ds_ctl5, AR_PacketDur2)) +
               (MS(ads->ds_ctl2, AR_XmitDataTries1) * MS(ads->ds_ctl4, AR_PacketDur1)) +
               (MS(ads->ds_ctl2, AR_XmitDataTries0) * MS(ads->ds_ctl4, AR_PacketDur0));
	} else {
        HALASSERT(0);
        return 0;
    }
}

#ifdef AH_PRIVATE_DIAG
void
ar5416_ContTxMode(struct ath_hal *ah, void *ds, int mode)
{
        static int qnum =0;
        int i;
        unsigned int qbits;
#if AH_DEBUG
        struct ar5416_desc *ads = AR5416DESC(ds);
        unsigned int val, val1, val2;
#endif
        if(mode == 10) return;

    if (mode==7) {                      // print status from the cont tx desc
#if AH_DEBUG
                if (ads) {
                        val1 = ads->ds_txstatus0;
                        val2 = ads->ds_txstatus1;
                        HDPRINTF(ah, HAL_DBG_TXDESC, "s0(%x) s1(%x)\n",
                                                   (unsigned)val1, (unsigned)val2);
                }
#endif
                HDPRINTF(ah, HAL_DBG_TXDESC, "txe(%x) txd(%x)\n",
                                           OS_REG_READ(ah, AR_Q_TXE),
                                           OS_REG_READ(ah, AR_Q_TXD)
                        );
#if AH_DEBUG
                for(i=0;i<HAL_NUM_TX_QUEUES; i++) {
                        val = OS_REG_READ(ah, AR_QTXDP(i));
                        val2 = OS_REG_READ(ah, AR_QSTS(i)) & AR_Q_STS_PEND_FR_CNT;
                        HDPRINTF(ah, HAL_DBG_TXDESC, "[%d] %x %d\n", i, val, val2);
                }
#endif
                return;
    }
    if (mode==8) {                      // set TXE for qnum
                OS_REG_WRITE(ah, AR_Q_TXE, 1<<qnum);
                return;
    }
    if (mode==9) {
                return;
    }

    if (mode >= 1) {                    // initiate cont tx operation
                /* Disable AGC to A2 */
		qnum = (int)ds;

                OS_REG_WRITE(ah, AR_PHY_TEST,
                                         (OS_REG_READ(ah, AR_PHY_TEST) | PHY_AGC_CLR) );

                OS_REG_WRITE(ah, 0x9864, OS_REG_READ(ah, 0x9864) | 0x7f000);
                OS_REG_WRITE(ah, 0x9924, OS_REG_READ(ah, 0x9924) | 0x7f00fe);
                OS_REG_WRITE(ah, AR_DIAG_SW,
                                         (OS_REG_READ(ah, AR_DIAG_SW) | (AR_DIAG_FORCE_RX_CLEAR+AR_DIAG_IGNORE_VIRT_CS)) );


                OS_REG_WRITE(ah, AR_CR, AR_CR_RXD);     // set receive disable

                if (mode == 3 || mode == 4) {
                        int txcfg;

                        if (mode == 3) {
                                OS_REG_WRITE(ah, AR_DLCL_IFS(qnum), 0);
                                OS_REG_WRITE(ah, AR_DRETRY_LIMIT(qnum), 0xffffffff);
                                OS_REG_WRITE(ah, AR_D_GBL_IFS_SIFS, 100);
                                OS_REG_WRITE(ah, AR_D_GBL_IFS_EIFS, 100);
                                OS_REG_WRITE(ah, AR_TIME_OUT, 2);
                                OS_REG_WRITE(ah, AR_D_GBL_IFS_SLOT, 100);
                        }
                        OS_REG_WRITE(ah, AR_DRETRY_LIMIT(qnum), 0xffffffff);
                        OS_REG_WRITE(ah, AR_D_FPCTL, 0x10|qnum);        // enable prefetch on qnum
                        txcfg = 5 | (6<<AR_FTRIG_S);
                        OS_REG_WRITE(ah, AR_TXCFG, txcfg);

                        OS_REG_WRITE(ah, AR_QMISC(qnum),        // set QCU modes
                                                 AR_Q_MISC_DCU_EARLY_TERM_REQ
                                                 +AR_Q_MISC_FSP_ASAP
                                                 +AR_Q_MISC_CBR_INCR_DIS1
                                                 +AR_Q_MISC_CBR_INCR_DIS0
                                );

                        /* stop tx dma all all except qnum */
                        qbits = 0x3ff;
                        qbits &= ~(1<<qnum);
                        for (i=0; i<10; i++) {
                                if (i==qnum) continue;
                                OS_REG_WRITE(ah, AR_Q_TXD, 1<<i);
                        }
                        OS_REG_WRITE(ah, AR_Q_TXD, qbits);

                        /* clear and freeze MIB counters */
                        OS_REG_WRITE(ah, AR_MIBC, AR_MIBC_CMC);
                        OS_REG_WRITE(ah, AR_MIBC, AR_MIBC_FMC);

                        OS_REG_WRITE(ah, AR_DMISC(qnum),
                                                 (AR_D_MISC_ARB_LOCKOUT_CNTRL_GLOBAL << AR_D_MISC_ARB_LOCKOUT_CNTRL_S)
                                                 +(AR_D_MISC_ARB_LOCKOUT_IGNORE)
                                                 +(AR_D_MISC_POST_FR_BKOFF_DIS)
                                                 +(AR_D_MISC_VIR_COL_HANDLING_IGNORE << AR_D_MISC_VIR_COL_HANDLING_S)
                                );

                        for(i=0; i<HAL_NUM_TX_QUEUES+2; i++) {  // disconnect QCUs
                                if (i==qnum) continue;
                                OS_REG_WRITE(ah, AR_DQCUMASK(i), 0);
                        }
                }
    }
    if (mode == 0) {

                OS_REG_WRITE(ah, AR_PHY_TEST,
                                         (OS_REG_READ(ah, AR_PHY_TEST) & ~PHY_AGC_CLR) );
                OS_REG_WRITE(ah, AR_DIAG_SW,
                                         (OS_REG_READ(ah, AR_DIAG_SW) & ~(AR_DIAG_FORCE_RX_CLEAR+AR_DIAG_IGNORE_VIRT_CS)) );
    }
}
#endif

void
ar5416Set11nTxDesc(struct ath_hal *ah, void *ds,
                                   u_int pktLen, HAL_PKT_TYPE type, u_int txPower,
                                   u_int key_ix, HAL_KEY_TYPE keyType,
                                   u_int flags)
{
        struct ar5416_desc *ads = AR5416DESC(ds);
		u_int32_t ctl0, ctl1, ctl6;
        HALASSERT(isValidPktType(type));
        HALASSERT(isValidKeyType(keyType));

        /*
         * If descriptor-based tpc, tx power is controlled by
         * ar5416Set11nRateScenario().
         * If not, what you put in the descriptor is ignored anyway.
         */
        txPower = 0;

        ctl0 = (pktLen & AR_FrameLen)
                                 | (flags & HAL_TXDESC_VMF ? AR_VirtMoreFrag : 0)
                                 | SM(txPower, AR_XmitPower0)
                                 | (flags & HAL_TXDESC_VEOL ? AR_VEOL : 0)
                                 | (flags & HAL_TXDESC_CLRDMASK ? AR_ClrDestMask : 0)
                                 | (flags & HAL_TXDESC_INTREQ ? AR_TxIntrReq : 0)
                                 | (key_ix != HAL_TXKEYIX_INVALID ? AR_DestIdxValid : 0)
                                 | (flags & HAL_TXDESC_LOWRXCHAIN ? AR_LowRxChain : 0);

        ctl1 = (key_ix != HAL_TXKEYIX_INVALID ? SM(key_ix, AR_DestIdx) : 0)
                                 | SM(type, AR_FrameType)
                                 | (flags & HAL_TXDESC_NOACK ? AR_NoAck : 0)
                                 | (flags & HAL_TXDESC_EXT_ONLY ? AR_ExtOnly : 0)
                                 | (flags & HAL_TXDESC_EXT_AND_CTL ? AR_ExtAndCtl : 0);

        ctl6 = SM(keyType, AR_EncrType);
		ads->ds_ctl0 = ctl0;
		ads->ds_ctl1 = ctl1;
		ads->ds_ctl6 = ctl6;

		/* For Kite set the tx antenna selection */
		if (AR_SREV_KITE(ah) || AR_SREV_K2(ah)) {
			/* FIXME: For now set the tx antenna to always 0,
			 * till tx selection diverisity is implementated
			 */
		    ads->ds_ctl8 = 0;
		    ads->ds_ctl9 = 0;
		    ads->ds_ctl10 = 0;
		    ads->ds_ctl11 = 0;
		}
}

#define HT_RC_2_MCS(_rc)        ((_rc) & 0x0f)
static const u_int8_t baDurationDelta[] = {
    24,     //  0: BPSK
    12,     //  1: QPSK 1/2
    12,     //  2: QPSK 3/4
     4,     //  3: 16-QAM 1/2
     4,     //  4: 16-QAM 3/4
     4,     //  5: 64-QAM 2/3
     4,     //  6: 64-QAM 3/4
     4,     //  7: 64-QAM 5/6
    24,     //  8: BPSK
    12,     //  9: QPSK 1/2
    12,     // 10: QPSK 3/4
     4,     // 11: 16-QAM 1/2
     4,     // 12: 16-QAM 3/4
     4,     // 13: 64-QAM 2/3
     4,     // 14: 64-QAM 3/4
     4,     // 15: 64-QAM 5/6
};

#if UNIFIED_SMARTANTENNA
void
ar5416Set11nRateScenario(struct ath_hal *ah, void *ds,
                             void *lastds,
                             u_int dur_update_en, u_int rts_cts_rate, u_int rts_cts_duration,
                             HAL_11N_RATE_SERIES series[], u_int nseries,
                             u_int flags, u_int32_t smart_ant_status, u_int32_t antenna_array[])
#else
void
ar5416Set11nRateScenario(struct ath_hal *ah, void *ds,
                             void *lastds,
                             u_int dur_update_en, u_int rts_cts_rate, u_int rts_cts_duration,
                             HAL_11N_RATE_SERIES series[], u_int nseries,
                             u_int flags, u_int32_t smartAntenna)
#endif
{
        struct ath_hal_private *ap = AH_PRIVATE(ah);
        struct ar5416_desc *ads = AR5416DESC(ds);
        struct ar5416_desc *last_ads = AR5416DESC(lastds);
        u_int32_t ds_ctl0;
        u_int mode;

        HALASSERT(nseries == 4);
        (void)nseries;
        (void)rts_cts_duration;   /* use H/W to calculate RTSCTSDuration */

        /*
         * Rate control settings override
         */
        ds_ctl0 = ads->ds_ctl0;
        if (flags & (HAL_TXDESC_RTSENA | HAL_TXDESC_CTSENA)) {
            if (flags & HAL_TXDESC_RTSENA) {
                ds_ctl0 &= ~AR_CTSEnable;
                ds_ctl0 |= AR_RTSEnable;
            } else {
                ds_ctl0 &= ~AR_RTSEnable;
                ds_ctl0 |= AR_CTSEnable;
            }
        } else {
            ds_ctl0 = (ds_ctl0 & ~(AR_RTSEnable | AR_CTSEnable));
        }

        mode = ath_hal_get_curmode(ah, ap->ah_curchan);
        if (ap->ah_config.ath_hal_desc_tpc) {
            int16_t txpower;

            txpower = ar5416GetRateTxPower(ah, mode, series[0].rate_index,
                                           series[0].ch_sel);

            if(series[0].tx_power_cap == 0)
            {  
                /*For short range mode, set txpower to MAX to put series[0].TxPowerCap into the descriptor*/
                txpower = HAL_TXPOWER_MAX;
            }

            ds_ctl0 &= ~AR_XmitPower0;
            if (AR_SREV_MERLIN_10_OR_LATER(ah)) {
                u_int count;
                for (count=0; count < nseries; count++) {
                    series[count].tx_power_cap -= AR5416_PWR_TABLE_OFFSET_DB * 2;
                }
            }
            ds_ctl0 |= set11nTxPower(0, AH_MIN(txpower, series[0].tx_power_cap));
        }

        ads->ds_ctl0 = ds_ctl0;

        ads->ds_ctl2 = set11nTries(series, 0)
                                 |  set11nTries(series, 1)
                                 |  set11nTries(series, 2)
                                 |  set11nTries(series, 3)
                                 |  (dur_update_en ? AR_DurUpdateEna : 0)
                                 |  SM(0, AR_BurstDur);

        ads->ds_ctl3 = set11nRate(series, 0)
                                 |  set11nRate(series, 1)
                                 |  set11nRate(series, 2)
                                 |  set11nRate(series, 3);

        ads->ds_ctl4 = set11nPktDurRTSCTS(series, 0)
                                 |  set11nPktDurRTSCTS(series, 1);

        ads->ds_ctl5 = set11nPktDurRTSCTS(series, 2)
                                 |  set11nPktDurRTSCTS(series, 3);

        ads->ds_ctl7 = set11nRateFlags(series, 0)
                                 |  set11nRateFlags(series, 1)
                                 |  set11nRateFlags(series, 2)
                                 |  set11nRateFlags(series, 3)
                                 | SM(rts_cts_rate, AR_RTSCTSRate);

        if (ap->ah_config.ath_hal_desc_tpc && AR_SREV_OWL_20_OR_LATER(ah)) {
            int16_t txpower;
            txpower = ar5416GetRateTxPower(
                ah, mode, series[1].rate_index, series[1].ch_sel);
            ads->ds_ctl9 =
                set11nTxPower(1, AH_MIN(txpower, series[1].tx_power_cap));
            txpower = ar5416GetRateTxPower(
                ah, mode, series[2].rate_index, series[2].ch_sel);
            ads->ds_ctl10 =
                set11nTxPower(2, AH_MIN(txpower, series[2].tx_power_cap));
            txpower = ar5416GetRateTxPower(
                ah, mode, series[3].rate_index, series[3].ch_sel);
            ads->ds_ctl11 =
                set11nTxPower(3, AH_MIN(txpower, series[3].tx_power_cap));
        }

#ifdef AH_NEED_DESC_SWAP
        last_ads->ds_ctl2 = __bswap32(ads->ds_ctl2);
        last_ads->ds_ctl3 = __bswap32(ads->ds_ctl3);
#else
        last_ads->ds_ctl2 = ads->ds_ctl2;
        last_ads->ds_ctl3 = ads->ds_ctl3;
#endif
}

void
ar5416Set11nAggrFirst(struct ath_hal *ah, void *ds, u_int aggr_len)
{
    struct ar5416_desc *ads = AR5416DESC(ds);

    ads->ds_ctl1 |= (AR_IsAggr | AR_MoreAggr);

    ads->ds_ctl6 &= ~AR_AggrLen;
    ads->ds_ctl6 |= SM(aggr_len, AR_AggrLen);
}

void
ar5416Set11nAggrMiddle(struct ath_hal *ah, void *ds, u_int num_delims)
{
    struct ar5416_desc *ads = AR5416DESC(ds);
    unsigned int ctl6;

    ads->ds_ctl1 |= (AR_IsAggr | AR_MoreAggr);

    /*
     * We use a stack variable to manipulate ctl6 to reduce uncached
     * read modify, modfiy, write.
     */
    ctl6 = ads->ds_ctl6;
    ctl6 &= ~AR_PadDelim;
    ctl6 |= SM(num_delims, AR_PadDelim);
    ads->ds_ctl6 = ctl6;
}

void
ar5416Set11nAggrLast(struct ath_hal *ah, void *ds)
{
    struct ar5416_desc *ads = AR5416DESC(ds);

    ads->ds_ctl1 |= AR_IsAggr;
    ads->ds_ctl1 &= ~AR_MoreAggr;
    ads->ds_ctl6 &= ~AR_PadDelim;
}

void
ar5416Clr11nAggr(struct ath_hal *ah, void *ds)
{
    struct ar5416_desc *ads = AR5416DESC(ds);

    ads->ds_ctl1 &= (~AR_IsAggr & ~AR_MoreAggr);
}

void
ar5416Set11nBurstDuration(struct ath_hal *ah, void *ds,
                                                  u_int burst_duration)
{
    struct ar5416_desc *ads = AR5416DESC(ds);

    ads->ds_ctl2 &= ~AR_BurstDur;
    ads->ds_ctl2 |= SM(burst_duration, AR_BurstDur);
}

void
ar5416Set11nRifsBurstMiddle(struct ath_hal *ah, void *ds)
{
    struct ar5416_desc *ads = AR5416DESC(ds);

    ads->ds_ctl1 |= AR_MoreRifs | AR_NoAck;
    ads->ds_ctl0 &= ~AR_TxIntrReq;
}

void
ar5416Set11nRifsBurstLast(struct ath_hal *ah, void *ds)
{
    struct ar5416_desc *ads = AR5416DESC(ds);

    ads->ds_ctl1 &= (~AR_MoreAggr & ~AR_MoreRifs);
}

void
ar5416Clr11nRifsBurst(struct ath_hal *ah, void *ds)
{
    struct ar5416_desc *ads = AR5416DESC(ds);

    ads->ds_ctl1 &= (~AR_MoreRifs & ~AR_NoAck);
    ads->ds_ctl0 |= AR_TxIntrReq;
}

void
ar5416Set11nAggrRifsBurst(struct ath_hal *ah, void *ds)
{
    struct ar5416_desc *ads = AR5416DESC(ds);

    ads->ds_ctl1 |= AR_NoAck;
    ads->ds_ctl1 &= ~AR_MoreRifs;
    ads->ds_ctl0 &= ~AR_TxIntrReq;
}

void
ar5416Set11nVirtualMoreFrag(struct ath_hal *ah, void *ds,
                                                  u_int vmf)
{
    struct ar5416_desc *ads = AR5416DESC(ds);

    if (vmf)
    {
        ads->ds_ctl0 |=  AR_VirtMoreFrag;
    }
    else
    {
        ads->ds_ctl0 &= ~AR_VirtMoreFrag;
    }
}

void
ar5416GetDescInfo(struct ath_hal *ah, HAL_DESC_INFO *desc_info)
{
    desc_info->txctl_numwords = TXCTL_NUMWORDS(ah);
    desc_info->txctl_offset = TXCTL_OFFSET(ah);
    desc_info->txstatus_numwords = TXSTATUS_NUMWORDS(ah);
    desc_info->txstatus_offset = TXSTATUS_OFFSET(ah);

    desc_info->rxctl_numwords = RXCTL_NUMWORDS(ah);
    desc_info->rxctl_offset = RXCTL_OFFSET(ah);
    desc_info->rxstatus_numwords = RXSTATUS_NUMWORDS(ah);
    desc_info->rxstatus_offset = RXSTATUS_OFFSET(ah);
}

#endif /* AH_SUPPORT_AR5416 */
