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

#if ATH_SUPPORT_WIRESHARK
#include "ah_radiotap.h" /* ah_rx_radiotap_header */
#endif /* ATH_SUPPORT_WIRESHARK */

/*
 * Initialize RX descriptor, by clearing the status and setting
 * the size (and any other flags).
 */

HAL_BOOL
ar5416SetupRxDesc(struct ath_hal *ah, struct ath_desc *ds,
    u_int32_t size, u_int flags)
{
    struct ar5416_desc *ads = AR5416DESC(ds);
    HAL_CAPABILITIES *pCap = &AH_PRIVATE(ah)->ah_caps;

    HALASSERT((size &~ AR_BufLen) == 0);

    ads->ds_ctl1 = size & AR_BufLen;
    if (flags & HAL_RXDESC_INTREQ)
        ads->ds_ctl1 |= AR_RxIntrReq;

    /* this should be enough */
    ads->ds_rxstatus8 &= ~AR_RxDone;
    if (! pCap->hal_auto_sleep_support) {
        /* If AutoSleep not supported, clear all fields. */
        OS_MEMZERO(&(ads->u), sizeof(ads->u));
    }
    return AH_TRUE;
}

/*
 * Process an RX descriptor, and return the status to the caller.
 * Copy some hardware specific items into an rx status struct.
 */
static HAL_STATUS
ar5416ProcRxDescBase(struct ath_hal *ah, struct ath_desc *ds,
    struct ath_rx_status *rx_stats)
{
    struct ar5416_desc ads;
    struct ar5416_desc *adsp = AR5416DESC(ds);

    if ((adsp->ds_rxstatus8 & AR_RxDone) == 0) {
        rx_stats->rs_status = HAL_RXERR_INCOMP;
        return HAL_EINPROGRESS;
    }

    /*
     * Now we need to get the stats from the descriptor. Since desc are
     * uncached, lets make a copy of the stats first. Note that, since we
     * touch most of the rx stats, a memcpy would always be more efficient
     *
     * Next we fill in all values in a caller passed stack variable.
     * This reduces the number of uncached accesses.
     * Do this copy here, after the check so that when the checks fail, we
     * dont end up copying the entire stats uselessly.
     */
    ads.u.rx = adsp->u.rx;

    rx_stats->rs_status = 0;
    rx_stats->rs_flags =  0;

    rx_stats->rs_datalen = ads.ds_rxstatus1 & AR_DataLen;
    rx_stats->rs_tstamp =  ads.AR_RcvTimestamp;

    /* If post delim CRC error happens,  */
    if (ads.ds_rxstatus8 & AR_PostDelimCRCErr) {
        rx_stats->rs_rssi      = HAL_RSSI_BAD;
        rx_stats->rs_rssi_ctl0 = HAL_RSSI_BAD;
        rx_stats->rs_rssi_ctl1 = HAL_RSSI_BAD;
        rx_stats->rs_rssi_ctl2 = HAL_RSSI_BAD;
        rx_stats->rs_rssi_ext0 = HAL_RSSI_BAD;
        rx_stats->rs_rssi_ext1 = HAL_RSSI_BAD;
        rx_stats->rs_rssi_ext2 = HAL_RSSI_BAD;
    } else {
        rx_stats->rs_rssi      = MS(ads.ds_rxstatus4, AR_RxRSSICombined);
        rx_stats->rs_rssi_ctl0 = MS(ads.ds_rxstatus0, AR_RxRSSIAnt00);
        rx_stats->rs_rssi_ctl1 = MS(ads.ds_rxstatus0, AR_RxRSSIAnt01);
        rx_stats->rs_rssi_ctl2 = MS(ads.ds_rxstatus0, AR_RxRSSIAnt02);
        rx_stats->rs_rssi_ext0 = MS(ads.ds_rxstatus4, AR_RxRSSIAnt10);
        rx_stats->rs_rssi_ext1 = MS(ads.ds_rxstatus4, AR_RxRSSIAnt11);
        rx_stats->rs_rssi_ext2 = MS(ads.ds_rxstatus4, AR_RxRSSIAnt12);
    }
    rx_stats->evm0 = adsp->ds_rxstatus5;
    rx_stats->evm1 = adsp->ds_rxstatus6;
    rx_stats->evm2 = adsp->ds_rxstatus7;
    /* XXX what about KeyCacheMiss? */
    if (ads.ds_rxstatus8 & AR_RxKeyIdxValid) {
        rx_stats->rs_keyix = MS(ads.ds_rxstatus8, AR_KeyIdx);
    } else {
        rx_stats->rs_keyix = HAL_RXKEYIX_INVALID;
    }
    /* NB: caller expected to do rate table mapping */
    rx_stats->rs_rate = RXSTATUS_RATE(ah, (&ads));
    rx_stats->rs_more = (ads.ds_rxstatus1 & AR_RxMore) ? 1 : 0;

    rx_stats->rs_isaggr = (ads.ds_rxstatus8 & AR_RxAggr) ? 1 : 0;
    rx_stats->rs_moreaggr = (ads.ds_rxstatus8 & AR_RxMoreAggr) ? 1 : 0;
    rx_stats->rs_antenna = MS(ads.ds_rxstatus3, AR_RxAntenna);
    rx_stats->rs_flags  = (ads.ds_rxstatus3 & AR_GI) ? HAL_RX_GI : 0;
    rx_stats->rs_flags  |= (ads.ds_rxstatus3 & AR_2040) ? HAL_RX_2040 : 0;

    if (ads.ds_rxstatus8 & AR_PreDelimCRCErr) {
        rx_stats->rs_flags |= HAL_RX_DELIM_CRC_PRE;
    }
    if (ads.ds_rxstatus8 & AR_PostDelimCRCErr) {
        rx_stats->rs_flags |= HAL_RX_DELIM_CRC_POST;
    }
    if (ads.ds_rxstatus8 & AR_DecryptBusyErr) {
        rx_stats->rs_flags |= HAL_RX_DECRYPT_BUSY;
    }
    if (ads.ds_rxstatus8 & AR_HiRxChain) {
        rx_stats->rs_flags |= HAL_RX_HI_RX_CHAIN;
    }
    if (ads.ds_rxstatus8 & AR_KeyMiss) {
        rx_stats->rs_status |= HAL_RXERR_KEYMISS;
    }

    if ((ads.ds_rxstatus8 & AR_RxFrameOK) == 0) {
        /*
         * These four bits should not be set together.  The
         * 5416 spec states a Michael error can only occur if
         * DecryptCRCErr not set (and TKIP is used).  Experience
         * indicates however that you can also get Michael errors
         * when a CRC error is detected, but these are specious.
         * Consequently we filter them out here so we don't
         * confuse and/or complicate drivers.
         */
        if (ads.ds_rxstatus8 & AR_CRCErr) {
            rx_stats->rs_status |= HAL_RXERR_CRC;
        } else if (ads.ds_rxstatus8 & AR_PHYErr) {
            u_int phyerr;

            rx_stats->rs_status |= HAL_RXERR_PHY;
            phyerr = MS(ads.ds_rxstatus8, AR_PHYErrCode);
            rx_stats->rs_phyerr = phyerr;
        } else if (ads.ds_rxstatus8 & AR_DecryptCRCErr)
            rx_stats->rs_status |= HAL_RXERR_DECRYPT;
        else if (ads.ds_rxstatus8 & AR_MichaelErr)
            rx_stats->rs_status |= HAL_RXERR_MIC;
    }
#if ATH_SUPPORT_CFEND
    /* count number of good subframes */
    if (((rx_stats->rs_status & HAL_RXERR_CRC) == 0) &&
        ((rx_stats->rs_status & HAL_RXERR_PHY) == 0) && (rx_stats->rs_isaggr)){
        if (!(rx_stats->rs_moreaggr)) {
            AH_PRIVATE(ah)->ah_rx_num_cur_aggr_good=0;
        } else {
            AH_PRIVATE(ah)->ah_rx_num_cur_aggr_good++;
        }
    }
    /* make sure we reset good subframe count when we recieve non-aggr*/
    if (!rx_stats->rs_isaggr) {
        AH_PRIVATE(ah)->ah_rx_num_cur_aggr_good=0;
    }
#endif
    return HAL_OK;
}

/*
 * Process an RX descriptor, and return the status to the caller.
 * Copy some hardware specific items into a rx status object
 * provided by the caller.
 *
 * NB: the caller is responsible for validating the memory contents
 *     of the descriptor (e.g. flushing any cached copy).
 */
HAL_STATUS
ar5416ProcRxDescFast(struct ath_hal *ah, struct ath_desc *ds,
    u_int32_t pa, struct ath_desc *nds, struct ath_rx_status *rx_stats, void *buf_addr)
{
    struct ar5416_desc *ands = AR5416DESC(nds);

    /*
     * Given the use of a self-linked tail be very sure that the hw is
     * done with this descriptor; the hw may have done this descriptor
     * once and picked it up again...make sure the hw has moved on.
     */
    if ((ands->ds_rxstatus8 & AR_RxDone) == 0
        && OS_REG_READ(ah, AR_RXDP) == pa)
        return HAL_EINPROGRESS;

    return ar5416ProcRxDescBase(ah, ds, rx_stats);
}

/*
 * Process an RX descriptor, and return the status to the caller.
 * Copy some hardware specific items into the software portion
 * of the descriptor.
 *
 * NB: the caller is responsible for validating the memory contents
 *     of the descriptor (e.g. flushing any cached copy).
 */
HAL_STATUS
ar5416ProcRxDesc(struct ath_hal *ah, struct ath_desc *ds,
    u_int32_t pa, struct ath_desc *nds, u_int64_t tsf, struct ath_rx_status *rxs)
{
    HAL_STATUS retval;
    retval = ar5416ProcRxDescBase(ah, ds, rxs);
    return retval;
}

HAL_STATUS
ar5416GetRxKeyIdx(struct ath_hal *ah, struct ath_desc *ds, 
    u_int8_t *keyix, u_int8_t *status)
{
    struct ar5416_desc *adsp = AR5416DESC(ds);
    u_int32_t status8;

    status8 = adsp->ds_rxstatus8;
    if ((status8 & AR_RxDone) == 0) {
        *status = HAL_RXERR_INCOMP;
        return HAL_EINPROGRESS;
    }

    /*
     * Now we only need to get the key index from the descriptor. 
     */
    *status = 0;
    if (status8 & AR_RxKeyIdxValid) {
        *keyix = MS(status8, AR_KeyIdx);
    } else {
        *keyix = HAL_RXKEYIX_INVALID;
    }
    if ((status8 & AR_RxFrameOK) && (status8 & (AR_CRCErr|AR_PHYErr|AR_DecryptCRCErr|AR_MichaelErr))) {
        *status = HAL_RXERR_INCOMP;
    }
    return HAL_OK;
}

#if ATH_SUPPORT_WIRESHARK
void ar5416FillRadiotapHdr(struct ath_hal *ah,
                           struct ah_rx_radiotap_header *rh,                           
                           struct ah_ppi_data *ppi,
                           struct ath_desc *ds,
                           void *buf_addr)
{
    struct ath_rx_status *rx_stats = &ds->ds_rxstat;
    struct ar5416_desc *adsp = AR5416DESC(ds);
    int i, j;

    if (rh != NULL) {
        OS_MEMZERO(rh, sizeof(struct ah_rx_radiotap_header));

        rh->tsf = ar5416GetTsf64(ah); /* AR5416 specific */

        rh->wr_rate = RXSTATUS_RATE(ah, adsp);
        rh->wr_chan_freq = (AH_PRIVATE(ah)->ah_curchan) ?
    		AH_PRIVATE(ah)->ah_curchan->channel : 0;
        rh->wr_antsignal = MS(adsp->ds_rxstatus4, AR_RxRSSICombined);

        // Fill out extended channel info.
        rh->wr_xchannel.freq = rh->wr_chan_freq;
		/* Modify for static analysis, prevent AH_PRIVATE(ah)->ah_curchan is NULL */
        rh->wr_xchannel.flags = (AH_PRIVATE(ah)->ah_curchan) ?
			AH_PRIVATE(ah)->ah_curchan->channel_flags : 0;
        rh->wr_xchannel.maxpower = (AH_PRIVATE(ah)->ah_curchan) ?
			AH_PRIVATE(ah)->ah_curchan->max_reg_tx_power : 0;
        rh->wr_xchannel.chan = ath_hal_mhz2ieee(ah, rh->wr_chan_freq, rh->wr_xchannel.flags);

        /* Radiotap Rx flags */
        if (AH5416(ah)->ah_getPlcpHdr) {
            rh->wr_rx_flags |= AH_RADIOTAP_11NF_RX_PLCP;
        }
        if (adsp->ds_rxstatus8 & AR_CRCErr) {
            /* for now, set both values */
            rh->wr_flags |= AH_RADIOTAP_F_BADFCS;
            rh->wr_rx_flags |= AH_RADIOTAP_F_RX_BADFCS;
        }
        if (adsp->ds_rxstatus3 & AR_GI) {
            rh->wr_rx_flags |= AH_RADIOTAP_11NF_RX_HALFGI;
            rh->wr_mcs.flags |= RADIOTAP_MCS_FLAGS_SHORT_GI;
        }
        if (adsp->ds_rxstatus3 & AR_2040 ) {
            rh->wr_rx_flags |= AH_RADIOTAP_11NF_RX_40;
            rh->wr_mcs.flags |= RADIOTAP_MCS_FLAGS_40MHZ;
        }
        if (adsp->ds_rxstatus3 & AR_Parallel40 ) {
            rh->wr_rx_flags |= AH_RADIOTAP_11NF_RX_40P;
        }
        if (adsp->ds_rxstatus8 & AR_RxAggr ) {
            rh->wr_rx_flags |= AH_RADIOTAP_11NF_RX_AGGR;
        }
        if (adsp->ds_rxstatus8 & AR_RxFirstAggr ) {
            rh->wr_rx_flags |= AH_RADIOTAP_11NF_RX_FIRSTAGGR;
        }
        if (adsp->ds_rxstatus8 & AR_RxMoreAggr ) {
            rh->wr_rx_flags |= AH_RADIOTAP_11NF_RX_MOREAGGR;
        }
        if (adsp->ds_rxstatus1 & AR_RxMore ) {
            rh->wr_rx_flags |= AH_RADIOTAP_11NF_RX_MORE;
        }
        if (adsp->ds_rxstatus8 & AR_PreDelimCRCErr) {
            rh->wr_rx_flags |= AH_RADIOTAP_11NF_RX_PREDELIM_CRCERR;
        }
        if (adsp->ds_rxstatus8 & AR_PostDelimCRCErr) {
            rh->wr_rx_flags |= AH_RADIOTAP_11NF_RX_POSTDELIM_CRCERR;
        }
        if (adsp->ds_rxstatus8 & AR_DecryptCRCErr) {
            rh->wr_rx_flags |= AH_RADIOTAP_11NF_RX_DECRYPTCRCERR;
        }
        if (adsp->ds_rxstatus3 & AR_STBC ) {
            rh->wr_rx_flags |= AH_RADIOTAP_11NF_RX_STBC;
        }
        if (adsp->ds_rxstatus8 & AR_PHYErr) {
            rh->wr_rx_flags |= AH_RADIOTAP_11NF_RX_PHYERR;
            rh->ath_ext.phyerr_code = rx_stats->rs_phyerr;
        }

        /* add the extension part */
        rh->vendor_ext.oui[0] = (ATH_OUI & 0xff);
        rh->vendor_ext.oui[1] = ((ATH_OUI >> 8) & 0xff);
        rh->vendor_ext.oui[2] = ((ATH_OUI >> 16) & 0xff);
        rh->vendor_ext.skip_length = sizeof(struct ah_rx_vendor_radiotap_header);
        /* sub_namespace - simply set to 0 since we don't use multiple namespaces */
        rh->vendor_ext.sub_namespace = 0;

        rh->ath_ext.version = AH_RADIOTAP_VERSION;

        rh->ath_ext.ch_info.num_chains =
            owl_get_ntxchains(AH5416(ah)->ah_rxchainmask);

        /*
         * Two-pass strategy to fill in RSSI and EVM:
         * First pass: copy the info from each chain from the
         * descriptor into the corresponding index of the
         * radiotap header's ch_info array.
         * Second pass: use the rxchainmask to determine
         * which chains are valid.  Shift ch_info array
         * elements down to fill in indices that were off
         * in the rxchainmask.
         */
        rh->ath_ext.ch_info.rssi_ctl[0] = rx_stats->rs_rssi_ctl0;
        rh->ath_ext.ch_info.rssi_ext[0] = rx_stats->rs_rssi_ext0;
        rh->ath_ext.ch_info.evm[0] = rx_stats->evm0;

        rh->ath_ext.ch_info.rssi_ctl[1] = rx_stats->rs_rssi_ctl1;
        rh->ath_ext.ch_info.rssi_ext[1] = rx_stats->rs_rssi_ext1;
        rh->ath_ext.ch_info.evm[1] = rx_stats->evm1;

        rh->ath_ext.ch_info.rssi_ctl[2] = rx_stats->rs_rssi_ctl2;
        rh->ath_ext.ch_info.rssi_ext[2] = rx_stats->rs_rssi_ext2;
        rh->ath_ext.ch_info.evm[2] = rx_stats->evm2;

        for (i = 0, j = 0; i < RADIOTAP_MAX_CHAINS ; i++ ) {
            if (AH5416(ah)->ah_rxchainmask & (0x1 << i)) {
                /*
                 * This chain is enabled.  Does its data need
                 * to be shifted down into a lower array index?
                 */
                if (i != j) {
                    rh->ath_ext.ch_info.rssi_ctl[j] =
                        rh->ath_ext.ch_info.rssi_ctl[i];
                    rh->ath_ext.ch_info.rssi_ext[j] =
                        rh->ath_ext.ch_info.rssi_ext[i];
                    /*
                     * EVM fields only need to be shifted if PCU_SEL_EVM bit is set.
                     * Otherwise EVM fields hold the value of PLCP headers which
                     * are not related to chainmask.
                     */
                    if (!(AH5416(ah)->ah_getPlcpHdr)) {
                        rh->ath_ext.ch_info.evm[j] = rh->ath_ext.ch_info.evm[i];
                    }
                }
                j++;
            }
        }
        rh->ath_ext.n_delims = MS(adsp->ds_rxstatus1, AR_NumDelim);
    }
    if (ppi != NULL) 
    {        
        struct ah_ppi_pfield_common             *pcommon;
        struct ah_ppi_pfield_mac_extensions     *pmac;
        struct ah_ppi_pfield_macphy_extensions  *pmacphy;
        
        // Fill out PPI Data
        pcommon = ppi->ppi_common;
        pmac = ppi->ppi_mac_ext;
        pmacphy = ppi->ppi_macphy_ext;

        // Zero out all fields
        OS_MEMZERO(pcommon, sizeof(struct ah_ppi_pfield_common));

        /* Common Fields */
        // Grab the tsf
        pcommon->common_tsft =  ar5416GetTsf64(ah);
        // Fill out common flags
        if (adsp->ds_rxstatus8 & AR_CRCErr) {
            pcommon->common_flags |= 4;
        }
        if (adsp->ds_rxstatus8 & AR_PHYErr) {
            pcommon->common_flags |= 8;
        }
        // Set the rate in the calling layer - it's already translated etc there.        
        // Channel frequency
        if (AH_PRIVATE(ah)->ah_curchan) {
        	pcommon->common_chan_freq = AH_PRIVATE(ah)->ah_curchan->channel;
        	// Channel flags
        	pcommon->common_chan_flags = AH_PRIVATE(ah)->ah_curchan->channel_flags;
        }
        pcommon->common_dbm_ant_signal = MS(adsp->ds_rxstatus4, AR_RxRSSICombined)-96;

        /* Mac Extensions */

        if (pmac != NULL) {
            OS_MEMZERO(pmac, sizeof(struct ah_ppi_pfield_mac_extensions));
            // Mac Flag bits : 0 = Greenfield, 1 = HT20[0]/40[1],  2 = SGI, 3 = DupRx, 4 = Aggregate
        
            if (adsp->ds_rxstatus3 & AR_2040) {
                pmac->mac_flags |= 1 << 1;            
            }
            if (adsp->ds_rxstatus3 & AR_GI) {            
                pmac->mac_flags |= 1 << 2;
            }
            if (adsp->ds_rxstatus8 & AR_RxAggr) {
                pmac->mac_flags |= 1 << 4;
            }

            if (adsp->ds_rxstatus8 & AR_RxMoreAggr) {            
                pmac->mac_flags |= 1 << 5;
            }

            if (adsp->ds_rxstatus8 & AR_PostDelimCRCErr) {
                pmac->mac_flags |= 1 << 6;
            }
            
            // pmac->amduId - get this in the caller where we have all the prototypes.
            pmac->mac_delimiters = MS(adsp->ds_rxstatus2, AR_NumDelim);

        } // if pmac != NULL

        if (pmacphy != NULL) {            
            OS_MEMZERO(pmacphy, sizeof(struct ah_ppi_pfield_macphy_extensions));
            // MacPhy Flag bits : 0=GreenField, 1=HT20[0]/HT40[1], 2=SGI, 3=DupRX, 4=Aggr, 5=MoreAggr, 6=PostDelimError            
            if (adsp->ds_rxstatus3 & AR_2040) {
                pmacphy->macphy_flags |= 1 << 1;
            }
            if (adsp->ds_rxstatus3 & AR_GI) {            
                pmacphy->macphy_flags |= 1 << 2;
            }
            if (adsp->ds_rxstatus8 & AR_RxAggr) {
                pmacphy->macphy_flags |= 1 << 4;
            }

            if (adsp->ds_rxstatus8 & AR_RxMoreAggr) {            
                pmacphy->macphy_flags |= 1 << 5;
            }

            if (adsp->ds_rxstatus8 & AR_PostDelimCRCErr) {
                pmacphy->macphy_flags |= 1 << 6;
            }

            pmacphy->macphy_delimiters = MS(adsp->ds_rxstatus1, AR_NumDelim);

            // macphy_ampduId : Get the AMPDU ID at the upper layer
            // macphy_mcs : Get in calling function - it's available in the ieee layer.
            // Fill macphy_numStreams based on mcs rate : do in caller.
            
            pmacphy->macphy_rssi_combined = MS(adsp->ds_rxstatus4, AR_RxRSSICombined);
            pmacphy->macphy_rssi_ant0_ctl = MS(adsp->ds_rxstatus0, AR_RxRSSIAnt00);
            pmacphy->macphy_rssi_ant1_ctl = MS(adsp->ds_rxstatus0, AR_RxRSSIAnt01);
            pmacphy->macphy_rssi_ant2_ctl = MS(adsp->ds_rxstatus0, AR_RxRSSIAnt02);
            pmacphy->macphy_rssi_ant0_ext = MS(adsp->ds_rxstatus4, AR_RxRSSIAnt10);
            pmacphy->macphy_rssi_ant1_ext = MS(adsp->ds_rxstatus4, AR_RxRSSIAnt11);
            pmacphy->macphy_rssi_ant2_ext = MS(adsp->ds_rxstatus4, AR_RxRSSIAnt12);
            if (AH_PRIVATE(ah)->ah_curchan) {
                pmacphy->macphy_chan_freq = AH_PRIVATE(ah)->ah_curchan->channel;
                pmacphy->macphy_chanFlags =  AH_PRIVATE(ah)->ah_curchan->channel_flags;
            }
            // Until we are able to combine CTL and EXT rssi's indicate control.
            
            pmacphy->macphy_ant0_signal = MS(adsp->ds_rxstatus0, AR_RxRSSIAnt00)-96;
            pmacphy->macphy_ant1_signal = MS(adsp->ds_rxstatus0, AR_RxRSSIAnt01)-96;
            pmacphy->macphy_ant2_signal = MS(adsp->ds_rxstatus0, AR_RxRSSIAnt02)-96;
            
            pmacphy->macphy_ant0_noise = -96;
            pmacphy->macphy_ant1_noise = -96;
            pmacphy->macphy_ant2_noise = -96; 
            
            pmacphy->macphy_evm0 = adsp->AR_RxEVM0;
            pmacphy->macphy_evm1 = adsp->AR_RxEVM1;
            pmacphy->macphy_evm2 = adsp->AR_RxEVM2;
        }
     }

 }
#endif /* ATH_SUPPORT_WIRESHARK */

#endif /* AH_SUPPORT_AR5416 */
