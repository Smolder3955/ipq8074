/*
 * Copyright (c) 2008-2010 Atheros Communications Inc.
 * All Rights Reserved.
 *
 * Copyright (c) 2015, 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2015 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */

#include "opt_ah.h"

#ifdef AH_SUPPORT_AR9300

#include "ah.h"
#include "ah_desc.h"
#include "ah_internal.h"

#include "ar9300/ar9300.h"
#include "ar9300/ar9300reg.h"
#include "ar9300/ar9300desc.h"

#if ATH_SUPPORT_WIRESHARK
#include "ah_radiotap.h" /* ah_rx_radiotap_header */
#endif /* ATH_SUPPORT_WIRESHARK */


/*
 * Process an RX descriptor, and return the status to the caller.
 * Copy some hardware specific items into the software portion
 * of the descriptor.
 *
 * NB: the caller is responsible for validating the memory contents
 *     of the descriptor (e.g. flushing any cached copy).
 */
HAL_STATUS
ar9300_proc_rx_desc_fast(struct ath_hal *ah, struct ath_desc *ds,
    u_int32_t pa, struct ath_desc *nds, struct ath_rx_status *rxs,
    void *buf_addr)
{
    struct ar9300_rxs *rxsp = AR9300RXS(buf_addr);

    /*
    ath_hal_printf(ah,"CHH=RX: ds_info 0x%x  status1: 0x%x  status11: 0x%x\n",
                        rxsp->ds_info,rxsp->status1,rxsp->status11);
     */

    if ((rxsp->status11 & AR_rx_done) == 0) {
        return HAL_EINPROGRESS;
    }

    if (MS(rxsp->ds_info, AR_desc_id) != 0x168c) {
#if __PKT_SERIOUS_ERRORS__
       /*BUG: 63564-HT */
        HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE, "%s: Rx Descriptor error 0x%x\n",
                 __func__, rxsp->ds_info);
#endif
        return HAL_EINVAL;
    }

    if ((rxsp->ds_info & (AR_tx_rx_desc | AR_ctrl_stat)) != 0) {
#if __PKT_SERIOUS_ERRORS__
        HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
            "%s: Rx Descriptor wrong info 0x%x\n", __func__, rxsp->ds_info);
#endif
        return HAL_EINPROGRESS;
    }

    rxs->rs_status = 0;
    rxs->rs_flags =  0;
    rxs->rs_phyerr = 0;
#if ATH_SUPPORT_WIFIPOS
    rxs->hdump = NULL;
#endif

    rxs->rs_datalen = rxsp->status2 & AR_data_len;
    rxs->rs_tstamp =  rxsp->status3;

    /* XXX what about key_cache_miss? */
    rxs->rs_rssi = MS(rxsp->status5, AR_rx_rssi_combined);
    rxs->rs_rssi_ctl0 = MS(rxsp->status1, AR_rx_rssi_ant00);
    rxs->rs_rssi_ctl1 = MS(rxsp->status1, AR_rx_rssi_ant01);
    rxs->rs_rssi_ctl2 = MS(rxsp->status1, AR_rx_rssi_ant02);
    rxs->rs_rssi_ext0 = MS(rxsp->status5, AR_rx_rssi_ant10);
    rxs->rs_rssi_ext1 = MS(rxsp->status5, AR_rx_rssi_ant11);
    rxs->rs_rssi_ext2 = MS(rxsp->status5, AR_rx_rssi_ant12);
    if (rxsp->status11 & AR_rx_key_idx_valid) {
        rxs->rs_keyix = MS(rxsp->status11, AR_key_idx);
    } else {
        rxs->rs_keyix = HAL_RXKEYIX_INVALID;
    }
    /* NB: caller expected to do rate table mapping */
    rxs->rs_rate = MS(rxsp->status1, AR_rx_rate);
    rxs->rs_more = (rxsp->status2 & AR_rx_more) ? 1 : 0;

    rxs->rs_isaggr = (rxsp->status11 & AR_rx_aggr) ? 1 : 0;
    rxs->rs_moreaggr = (rxsp->status11 & AR_rx_more_aggr) ? 1 : 0;
#if UNIFIED_SMARTANTENNA
    rxs->rs_antenna = MS(rxsp->status4, AR_rx_antenna);
#else    
    rxs->rs_antenna = (MS(rxsp->status4, AR_rx_antenna) & 0x7);
#endif    
    rxs->rs_isapsd = (rxsp->status11 & AR_apsd_trig) ? 1 : 0;
    rxs->rs_flags  = (rxsp->status4 & AR_gi) ? HAL_RX_GI : 0;
    if (!((rxsp->status11 & AR_rx_aggr) && ((rxsp->status11 & AR_rx_first_aggr) || rxsp->status11 & AR_rx_more_aggr)))
    {
        rxs->rs_flags |= (rxsp->status4 & AR_2040) ? HAL_RX_2040 : 0;
    }

#ifdef ATH_SUPPORT_TxBF
    rxs->rx_hw_upload_data = (rxsp->status2 & AR_hw_upload_data) ? 1 : 0;
    rxs->rx_not_sounding = (rxsp->status4 & AR_rx_not_sounding) ? 1 : 0;
    rxs->rx_Ness = MS(rxsp->status4, AR_rx_ness);
    rxs->rx_hw_upload_data_valid =
        (rxsp->status4 & AR_hw_upload_data_valid) ? 1 : 0;  
    /*
     * Due to HW timing issue, rx_hw_upload_data_type field not work-able
     * EV [69462] Chip::Osprey Wrong value at "hw_upload_data_type" of RXS
     * when uploading V/CV report
     */
    rxs->rx_hw_upload_data_type = MS(rxsp->status11, AR_hw_upload_data_type);
#if 0
    if (((rxsp->status11 & AR_rx_frame_ok) == 0) &&
        ((rxsp->status4 & AR_rx_not_sounding) == 0))
    {
        HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
            "=>%s:sounding, Ness= %d,Rx_2:%x,Rx4:%x, Rx_11:%x,rate %d\n",
            __func__, MS(rxsp->status4, AR_rx_ness),
            rxsp->status2, rxsp->status4, rxsp->status11, rxs->rs_rate);
        HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
            "===>%s:data length %d, Rx rate %d\n",
            __func__, rxs->rs_datalen, rxs->rs_rate);
    }
#endif
#endif
    /* Copy EVM information */
    rxs->evm0 = rxsp->status6;
    rxs->evm1 = rxsp->status7;
    rxs->evm2 = rxsp->status8;
    rxs->evm3 = rxsp->status9;
    rxs->evm4 = (rxsp->status10 & 0xffff);

    if (rxsp->status11 & AR_pre_delim_crc_err) {
        rxs->rs_flags |= HAL_RX_DELIM_CRC_PRE;
    }
    if (rxsp->status11 & AR_post_delim_crc_err) {
        rxs->rs_flags |= HAL_RX_DELIM_CRC_POST;
    }
    if (rxsp->status11 & AR_decrypt_busy_err) {
        rxs->rs_flags |= HAL_RX_DECRYPT_BUSY;
    }
    if (rxsp->status11 & AR_hi_rx_chain) {
        rxs->rs_flags |= HAL_RX_HI_RX_CHAIN;
    }
    if (rxsp->status11 & AR_key_miss) {
        rxs->rs_status |= HAL_RXERR_KEYMISS;
    }

    if ((rxsp->status11 & AR_rx_frame_ok) == 0) {
        /*
         * These four bits should not be set together.  The
         * 9300 spec states a Michael error can only occur if
         * decrypt_crc_err not set (and TKIP is used).  Experience
         * indicates however that you can also get Michael errors
         * when a CRC error is detected, but these are specious.
         * Consequently we filter them out here so we don't
         * confuse and/or complicate drivers.
         */

        if (rxsp->status11 & AR_crc_err) {
            rxs->rs_status |= HAL_RXERR_CRC;
            /* 
			 * ignore CRC flag for phy reports
			 */
            if (rxsp->status11 & AR_phyerr) {
                u_int phyerr = MS(rxsp->status11, AR_phy_err_code);
                rxs->rs_status |= HAL_RXERR_PHY;
                rxs->rs_phyerr = phyerr;
            }
        } else if (rxsp->status11 & AR_phyerr) {
            u_int phyerr;

            /*
             * Packets with OFDM_RESTART on post delimiter are CRC OK and
             * usable and MAC ACKs them.
             * To avoid packet from being lost, we remove the PHY Err flag
             * so that lmac layer does not drop them.
             * (EV 70071)
             */
            phyerr = MS(rxsp->status11, AR_phy_err_code);
            if ((phyerr == HAL_PHYERR_OFDM_RESTART) && 
                    (rxsp->status11 & AR_post_delim_crc_err)) {
                rxs->rs_phyerr = 0;
            } else {
                rxs->rs_status |= HAL_RXERR_PHY;
                rxs->rs_phyerr = phyerr;
            }
        } else if (rxsp->status11 & AR_decrypt_crc_err) {
            rxs->rs_status |= HAL_RXERR_DECRYPT;
        } else if (rxsp->status11 & AR_michael_err) {
            rxs->rs_status |= HAL_RXERR_MIC;
        }
    }
#if ATH_SUPPORT_WIFIPOS
    else {
    	rxs->rx_position_bit = (rxsp->status11 & AR_position_bit) ? 1 : 0;
#ifdef ATH_SUPPORT_TxBF
        if (rxs->rx_position_bit && rxs->rx_hw_upload_data && 
                rxs->rx_hw_upload_data_valid && rxs->rx_hw_upload_data_type == 1) {
            rxs->hdump = (u_int8_t *)((char *)rxsp + sizeof(struct ar9300_rxs));
        }
#endif
    }
#endif
    rxs->rs_channel = AH_PRIVATE(ah)->ah_curchan->channel;
    return HAL_OK;
}

HAL_STATUS
ar9300_proc_rx_desc(struct ath_hal *ah, struct ath_desc *ds,
    u_int32_t pa, struct ath_desc *nds, u_int64_t tsf, 
    struct ath_rx_status *rxs)
{
    return HAL_ENOTSUPP;
}

/*
 * rx path in ISR is different for ar9300 from ar5416, and
 * ath_rx_proc_descfast will not be called if edmasupport is true.
 * So this function ath_hal_get_rxkeyidx will not be 
 * called for ar9300.
 * This function in ar9300's HAL is just a stub one because we need 
 * to link something to the callback interface of the HAL module.
 */
HAL_STATUS
ar9300_get_rx_key_idx(struct ath_hal *ah, struct ath_desc *ds, u_int8_t *keyix,
    u_int8_t *status)
{
    *status = 0;
    *keyix = HAL_RXKEYIX_INVALID;    
    return HAL_ENOTSUPP;
}

#if ATH_SUPPORT_WIRESHARK

void ar9300_fill_radiotap_hdr(struct ath_hal *ah,
                           struct ah_rx_radiotap_header *rh,
                           struct ah_ppi_data *ppi,
                           struct ath_desc *ds,
                           void *buf_addr)
{
    struct ar9300_rxs *rxsp = AR9300RXS(buf_addr);
    int i, j;

    if (rh != NULL) {
        /* Fill out Radiotap header */
        OS_MEMZERO(rh, sizeof(struct ah_rx_radiotap_header));

        rh->tsf = ar9300_get_tsf64(ah);

        rh->wr_rate = MS(rxsp->status1, AR_rx_rate);
        rh->wr_chan_freq = (AH_PRIVATE(ah)->ah_curchan) ?
            AH_PRIVATE(ah)->ah_curchan->channel : 0;
        rh->wr_xchannel.freq = rh->wr_chan_freq;
		/* Modify for static analysis, prevent AH_PRIVATE(ah)->ah_curchan is NULL */
        rh->wr_xchannel.flags = (AH_PRIVATE(ah)->ah_curchan) ? 
			AH_PRIVATE(ah)->ah_curchan->channel_flags : 0;
        rh->wr_xchannel.maxpower = (AH_PRIVATE(ah)->ah_curchan) ?
            AH_PRIVATE(ah)->ah_curchan->max_reg_tx_power : 0;
        rh->wr_xchannel.chan =
            ath_hal_mhz2ieee(ah, rh->wr_chan_freq, rh->wr_xchannel.flags);

        rh->wr_antsignal = MS(rxsp->status5, AR_rx_rssi_combined);

        /* Radiotap Rx flags */
        if (AH9300(ah)->ah_get_plcp_hdr) {
            rh->wr_rx_flags |= AH_RADIOTAP_11NF_RX_PLCP;
        }
        if (rxsp->status11 & AR_crc_err) {
            /* for now set both */
            rh->wr_flags |= AH_RADIOTAP_F_BADFCS;
            rh->wr_rx_flags |= AH_RADIOTAP_F_RX_BADFCS;
        }
        if (rxsp->status4 & AR_gi) {
            rh->wr_rx_flags |= AH_RADIOTAP_11NF_RX_HALFGI;
            rh->wr_mcs.flags |= RADIOTAP_MCS_FLAGS_SHORT_GI;
        }
        if (rxsp->status4 & AR_2040) {
            rh->wr_rx_flags |= AH_RADIOTAP_11NF_RX_40;
            rh->wr_mcs.flags |= RADIOTAP_MCS_FLAGS_40MHZ;
        }
        if (rxsp->status4 & AR_parallel40) {
            rh->wr_rx_flags |= AH_RADIOTAP_11NF_RX_40P;
        }
        if (rxsp->status11 & AR_rx_aggr) {
            rh->wr_rx_flags |= AH_RADIOTAP_11NF_RX_AGGR;
        }
        if (rxsp->status11 & AR_rx_first_aggr) {
            rh->wr_rx_flags |= AH_RADIOTAP_11NF_RX_FIRSTAGGR;
        }
        if (rxsp->status11 & AR_rx_more_aggr) {
            rh->wr_rx_flags |= AH_RADIOTAP_11NF_RX_MOREAGGR;
        }
        if (rxsp->status2 & AR_rx_more) {
            rh->wr_rx_flags |= AH_RADIOTAP_11NF_RX_MORE;
        }
        if (rxsp->status11 & AR_pre_delim_crc_err) {
            rh->wr_rx_flags |= AH_RADIOTAP_11NF_RX_PREDELIM_CRCERR;
        }
        if (rxsp->status11 & AR_post_delim_crc_err) {
            rh->wr_rx_flags |= AH_RADIOTAP_11NF_RX_POSTDELIM_CRCERR;
        }
        if (rxsp->status11 & AR_decrypt_crc_err) {
            rh->wr_rx_flags |= AH_RADIOTAP_11NF_RX_DECRYPTCRCERR;
        }
        if (rxsp->status4 & AR_rx_stbc) {
            rh->wr_rx_flags |= AH_RADIOTAP_11NF_RX_STBC;
        }
        if (rxsp->status11 & AR_phyerr) {
            rh->wr_rx_flags |= AH_RADIOTAP_11NF_RX_PHYERR;
            rh->ath_ext.phyerr_code = MS(rxsp->status11, AR_phy_err_code);
        }

        /* add the extension part */
        rh->vendor_ext.oui[0] = (ATH_OUI & 0xff);
        rh->vendor_ext.oui[1] = ((ATH_OUI >> 8) & 0xff);
        rh->vendor_ext.oui[2] = ((ATH_OUI >> 16) & 0xff);
        rh->vendor_ext.skip_length =
            sizeof(struct ah_rx_vendor_radiotap_header);

        /* sub_namespace -
         * simply set to 0 since we don't use multiple namespaces
         */
        rh->vendor_ext.sub_namespace = 0;

        rh->ath_ext.version = AH_RADIOTAP_VERSION;

        rh->ath_ext.ch_info.num_chains =
            AR9300_NUM_CHAINS(AH9300(ah)->ah_rx_chainmask);

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
        if (rxsp->status11 & AR_post_delim_crc_err) {
            for (i = 0; i < RADIOTAP_MAX_CHAINS ; i++) {
                rh->ath_ext.ch_info.rssi_ctl[i] = HAL_RSSI_BAD;
                rh->ath_ext.ch_info.rssi_ext[i] = HAL_RSSI_BAD;
            }
        } else {
            rh->ath_ext.ch_info.rssi_ctl[0] =
                MS(rxsp->status1, AR_rx_rssi_ant00);
            rh->ath_ext.ch_info.rssi_ctl[1] =
                MS(rxsp->status1, AR_rx_rssi_ant01);
            rh->ath_ext.ch_info.rssi_ctl[2] =
                MS(rxsp->status1, AR_rx_rssi_ant02);

            rh->ath_ext.ch_info.rssi_ext[0] =
                MS(rxsp->status5, AR_rx_rssi_ant10);
            rh->ath_ext.ch_info.rssi_ext[1] =
                MS(rxsp->status5, AR_rx_rssi_ant11);
            rh->ath_ext.ch_info.rssi_ext[2] =
                MS(rxsp->status5, AR_rx_rssi_ant12);

            rh->ath_ext.ch_info.evm[0] = rxsp->AR_rx_evm0;
            rh->ath_ext.ch_info.evm[1] = rxsp->AR_rx_evm1;
            rh->ath_ext.ch_info.evm[2] = rxsp->AR_rx_evm2;
        }

        for (i = 0, j = 0; i < RADIOTAP_MAX_CHAINS ; i++) {
            if (AH9300(ah)->ah_rx_chainmask & (0x1 << i)) {
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
                     * EVM fields only need to be shifted if
                     * PCU_SEL_EVM bit is set.
                     * Otherwise EVM fields hold the value of PLCP headers
                     * which are not related to chainmask.
                     */
                    if (!(AH9300(ah)->ah_get_plcp_hdr)) {
                        rh->ath_ext.ch_info.evm[j] = rh->ath_ext.ch_info.evm[i];
                    }
                }
                j++;
            }
        }

        rh->ath_ext.n_delims = MS(rxsp->status2, AR_num_delim);
    } /* if rh != NULL */

    if (ppi != NULL) {        
        struct ah_ppi_pfield_common             *pcommon;
        struct ah_ppi_pfield_mac_extensions     *pmac;
        struct ah_ppi_pfield_macphy_extensions  *pmacphy;
        
        /* Fill out PPI Data */
        pcommon = ppi->ppi_common;
        pmac = ppi->ppi_mac_ext;
        pmacphy = ppi->ppi_macphy_ext;

        /* Zero out all fields */
        OS_MEMZERO(pcommon, sizeof(struct ah_ppi_pfield_common));

        /* Common Fields */
        /* Grab the tsf */
        pcommon->common_tsft =  ar9300_get_tsf64(ah);
        /* Fill out common flags */
        if (rxsp->status11 & AR_crc_err) {
            pcommon->common_flags |= 4;
        }
        if (rxsp->status11 & AR_phyerr) {
            pcommon->common_flags |= 8;
        }
        // Set the rate in the calling layer - it's already translated etc there.        
        // Channel frequency
        if (AH_PRIVATE(ah)->ah_curchan) {
            pcommon->common_chan_freq = AH_PRIVATE(ah)->ah_curchan->channel;
            pcommon->common_chan_flags = AH_PRIVATE(ah)->ah_curchan->channel_flags;
        }
        pcommon->common_dbm_ant_signal = MS(rxsp->status5, AR_rx_rssi_combined) - 96;

        /* Mac Extensions */

        if (pmac != NULL) {            
            OS_MEMZERO(pmac, sizeof(struct ah_ppi_pfield_mac_extensions));
            /* Mac Flag bits:
             * 0 = Greenfield
             * 1 = HT20[0]/40[1]
             * 2 = SGI
             * 3 = dup_rx
             * 4 = Aggregate
             */
            if (rxsp->status4 & AR_2040) {
                pmac->mac_flags |= 1 << 1;            
            }
            if (rxsp->status4 & AR_gi) {            
                pmac->mac_flags |= 1 << 2;
            }
            if (rxsp->status11 & AR_rx_aggr) {
                pmac->mac_flags |= 1 << 4;
            }

            if (rxsp->status11 & AR_rx_more_aggr) {            
                pmac->mac_flags |= 1 << 5;
            }

            if (rxsp->status11 & AR_post_delim_crc_err) {
                pmac->mac_flags |= 1 << 6;
            }
            
            /* pmac->ampdu_id -
             * get this in the caller where we have all the prototypes.
             */
            pmac->mac_delimiters = MS(rxsp->status2, AR_num_delim);

        } /* if pmac != NULL */

        if (pmacphy != NULL) {
            OS_MEMZERO(pmacphy, sizeof(struct ah_ppi_pfield_macphy_extensions));
            /* mac_phy Flag bits:
             * 0 = green_field
             * 1 = HT20[0] / HT40[1]
             * 2 = SGI
             * 3 = dup_rx
             * 4 = Aggr
             * 5 = more_aggr
             * 6 = post_delim_error
             */
            if (rxsp->status4 & AR_2040) {
                pmacphy->macphy_flags |= 1 << 1;
            }
            if (rxsp->status4 & AR_gi) {            
                pmacphy->macphy_flags |= 1 << 2;
            }
            if (rxsp->status11 & AR_rx_aggr) {
                pmacphy->macphy_flags |= 1 << 4;
            }

            if (rxsp->status11 & AR_rx_more_aggr) {            
                pmacphy->macphy_flags |= 1 << 5;
            }

            if (rxsp->status11 & AR_post_delim_crc_err) {
                pmacphy->macphy_flags |= 1 << 6;
            }

            pmacphy->macphy_delimiters = MS(rxsp->status2, AR_num_delim);

            /* macphy_ampdu_id: Get the AMPDU ID at the upper layer */
            /* macphy_mcs:
             * Get in calling function - it's available in the ieee layer.
             */
            /* Fill macphy_num_streams based on mcs rate: do in caller. */
            
            pmacphy->macphy_rssi_combined =
                MS(rxsp->status5, AR_rx_rssi_combined);
            pmacphy->macphy_rssi_ant0_ctl = MS(rxsp->status1, AR_rx_rssi_ant00);
            pmacphy->macphy_rssi_ant1_ctl = MS(rxsp->status1, AR_rx_rssi_ant01);
            pmacphy->macphy_rssi_ant2_ctl = MS(rxsp->status1, AR_rx_rssi_ant02);
            pmacphy->macphy_rssi_ant0_ext = MS(rxsp->status5, AR_rx_rssi_ant10);
            pmacphy->macphy_rssi_ant1_ext = MS(rxsp->status5, AR_rx_rssi_ant11);
            pmacphy->macphy_rssi_ant2_ext = MS(rxsp->status5, AR_rx_rssi_ant12);

            if (AH_PRIVATE(ah)->ah_curchan) {
                pmacphy->macphy_chan_freq = AH_PRIVATE(ah)->ah_curchan->channel;
                pmacphy->macphy_chanFlags =  AH_PRIVATE(ah)->ah_curchan->channel_flags; 
            }
            pmacphy->macphy_ant0_signal =
                MS(rxsp->status1, AR_rx_rssi_ant00) - 96;
            pmacphy->macphy_ant1_signal =
                MS(rxsp->status1, AR_rx_rssi_ant01) - 96;
            pmacphy->macphy_ant2_signal =
                MS(rxsp->status1, AR_rx_rssi_ant02) - 96;
            
            pmacphy->macphy_ant0_noise = -96;
            pmacphy->macphy_ant1_noise = -96;
            pmacphy->macphy_ant2_noise = -96;
            
            pmacphy->macphy_evm0 = rxsp->AR_rx_evm0;
            pmacphy->macphy_evm1 = rxsp->AR_rx_evm1;
            pmacphy->macphy_evm2 = rxsp->AR_rx_evm2;
        }
    }
}
#endif /* ATH_SUPPORT_WIRESHARK */

#endif
