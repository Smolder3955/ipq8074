/* Copyright (c) 2011, 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */
/*
 * Copyright (c) 2010, Atheros Communications Inc.
 * All Rights Reserved.
 *
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 */

#include "spectral.h"

#if WLAN_SPECTRAL_ENABLE

u_int32_t
spectral_round(int32_t val)
{
    u_int32_t ival,rem;

    if (val < 0)
        return 0;

    ival = val/100;
    rem = val-(ival*100);

    if (rem <50)
        return ival;
    else
        return(ival+1);
}

int8_t fix_rssi_inv_only (u_int8_t rssi_val)
{
    int8_t temp = rssi_val;
    int8_t inv  = 0;

    if (rssi_val > 127) {
        temp = rssi_val & 0x7f;
        inv = (~(temp) + 1) & 0x7f;
        temp = 0 - inv;
    }

    return temp;
}

int8_t adjust_rssi_with_nf_noconv_dbm (struct ath_spectral *spectral, int8_t rssi, int upper, int lower)
{
    return adjust_rssi_with_nf_dbm (spectral, rssi, upper, lower, 0);
}

int8_t adjust_rssi_with_nf_dbm (struct ath_spectral* spectral, int8_t rssi, int upper, int lower, int convert_to_dbm)
{
    int16_t nf = -110;
    int8_t temp;
    SPECTRAL_OPS* p_sops = GET_SPECTRAL_OPS(spectral);

    if (upper) {
        nf = p_sops->get_ctl_noisefloor(spectral);
    }
    if (lower) {
        nf = p_sops->get_ext_noisefloor(spectral);
    }
    temp = 110 + (nf) + (rssi);
    if (convert_to_dbm)
        temp -= 96;
    return temp;
}

int8_t fix_rssi_for_classifier (struct ath_spectral* spectral, u_int8_t rssi_val, int upper, int lower)
{
    int8_t temp = rssi_val;
    int8_t inv  = 0;

    if (rssi_val > 127) {
        temp = rssi_val & 0x7f;
        inv = (~(temp) + 1) & 0x7f;
        temp = 0 - inv;
    }
    temp = adjust_rssi_with_nf_noconv_dbm(spectral, temp, upper, lower);

    return temp;
}


/*
 * Function     : ath_process_spectraldata
 * Description  : process spectral data from hardware till Peregrine
 * Input        :
 * Output       :
 *
 */

void
ath_process_spectraldata(struct ath_spectral *spectral, struct ath_buf *bf, struct ath_rx_status *rxs, u_int64_t fulltsf)
{
#define EXT_CH_RADAR_FOUND          0x02
#define PRI_CH_RADAR_FOUND          0x01
#define EXT_CH_RADAR_EARLY_FOUND    0x04
#define SPECTRAL_SCAN_DATA          0x10
#define DEFAULT_CHAN_NOISE_FLOOR    -110

    int i = 0;
    struct samp_msg_params params;

    u_int8_t rssi             = 0;
    u_int8_t control_rssi     = 0;
    u_int8_t extension_rssi   = 0;
    u_int8_t combined_rssi    = 0;
    u_int8_t max_exp          = 0;
    u_int8_t max_scale        = 0;
    u_int8_t dc_index         = 0;
    u_int8_t lower_dc         = 0;
    u_int8_t upper_dc         = 0;
    u_int8_t ext_rssi         = 0;

    int8_t inv_control_rssi     = 0;
    int8_t inv_combined_rssi    = 0;
    int8_t inv_extension_rssi   = 0;
    int8_t temp_nf              = 0;


    u_int8_t pulse_bw_info      = 0;
    u_int8_t pulse_length_ext   = 0;
    u_int8_t pulse_length_pri   = 0;

    u_int32_t tstamp            = 0;

    u_int16_t datalen           = 0;
    u_int16_t max_mag           = 0;
    u_int16_t newdatalen        = 0;
    u_int16_t already_copied    = 0;
    u_int16_t maxmag_upper      = 0;

    u_int8_t maxindex_upper     = 0;
    u_int8_t max_index          = 0;

    int bin_pwr_count   = 0;

    u_int32_t *last_word_ptr        = NULL;
    u_int32_t *secondlast_word_ptr  = NULL;

    u_int8_t *byte_ptr          = NULL;
    u_int8_t *fft_data_end_ptr  = NULL;

    u_int8_t last_byte_0 = 0;
    u_int8_t last_byte_1 = 0;
    u_int8_t last_byte_2 = 0;
    u_int8_t last_byte_3 = 0;

    u_int8_t secondlast_byte_0 = 0;
    u_int8_t secondlast_byte_1 = 0;
    u_int8_t secondlast_byte_2 = 0;
    u_int8_t secondlast_byte_3 = 0;

    HT20_FFT_PACKET fft_20;
    HT40_FFT_PACKET fft_40;

    u_int8_t *fft_data_ptr  = NULL;
    u_int8_t *fft_src_ptr   = NULL;
    u_int8_t *data_ptr      = NULL;

    int8_t cmp_rssi = -110;
    int8_t rssi_up  = 0;
    int8_t rssi_low = 0;

    static u_int64_t last_tsf       = 0;
    static u_int64_t send_tstamp    = 0;

    int8_t nfc_control_rssi     = 0;
    int8_t nfc_extension_rssi   = 0;

    int peak_freq   = 0;
    int nb_lower    = 0;
    int nb_upper    = 0;

    int8_t ctl_chan_noise_floor = DEFAULT_CHAN_NOISE_FLOOR;
    int8_t ext_chan_noise_floor = DEFAULT_CHAN_NOISE_FLOOR;
    struct ath_softc* sc        = NULL;
    SPECTRAL_OPS* p_sops        = NULL;

    qdf_mem_zero(&params, sizeof(params));

    if (((rxs->rs_phyerr != HAL_PHYERR_RADAR)) &&
        ((rxs->rs_phyerr != HAL_PHYERR_FALSE_RADAR_EXT)) &&
        ((rxs->rs_phyerr != HAL_PHYERR_SPECTRAL))) {
        printk("%s : Unknown PHY error (0x%x)\n", __func__, rxs->rs_phyerr);
        return;
    }

    if (spectral == NULL) {
        printk("Spectral Module not attached\n");
        return;
    }

    sc     = GET_SPECTRAL_ATHSOFTC(spectral);
    p_sops = GET_SPECTRAL_OPS(spectral);

    spectral->ath_spectral_stats.total_phy_errors++;

    /* Get pointer to data & timestamp*/
    datalen = rxs->rs_datalen;
    tstamp  = (rxs->rs_tstamp & SPECTRAL_TSMASK);

    /* check for valid data length */
    if ((!datalen) || (datalen < spectral->spectral_data_len - 1))  {
        spectral->ath_spectral_stats.datalen_discards++;
        return;
    }


    /* WAR: Never trust combined RSSI on radar pulses for <=
     * OWL2.0. For short pulses only the chain 0 rssi is present
     * and remaining descriptor data is all 0x80, for longer
     * pulses the descriptor is present, but the combined value is
     * inaccurate. This HW capability is queried in spectral_attach and stored in
     * the sc_spectral_combined_rssi_ok flag.*/

    if (spectral->sc_spectral_combined_rssi_ok) {
        rssi = (u_int8_t) rxs->rs_rssi;
    } else {
        rssi = (u_int8_t) rxs->rs_rssi_ctl0;
    }

    /* get rssi values */
    combined_rssi   = rssi;
    control_rssi    = (u_int8_t) rxs->rs_rssi_ctl0;
    extension_rssi  = (u_int8_t) rxs->rs_rssi_ext0;
    ext_rssi        = (u_int8_t) rxs->rs_rssi_ext0;

    /*
     * If the combined RSSI is less than a particular threshold, this pulse is of no
     * interest to the classifier, so discard it here itself
     * except when noise power cal is required (then we want all rssi values)
     */
    inv_combined_rssi = fix_rssi_inv_only(combined_rssi);

    if (inv_combined_rssi < 5 && !spectral->sc_spectral_noise_pwr_cal) {
        return;
    }

    last_word_ptr       = (u_int32_t *)(((u_int8_t*)bf->bf_vdata) + datalen - (datalen%4));
    secondlast_word_ptr = last_word_ptr-1;

    byte_ptr    =   (u_int8_t*)last_word_ptr;
    last_byte_0 =   (*(byte_ptr)   & 0xff);
    last_byte_1 =   (*(byte_ptr+1) & 0xff);
    last_byte_2 =   (*(byte_ptr+2) & 0xff);
    last_byte_3 =   (*(byte_ptr+3) & 0xff);

    byte_ptr            =   (u_int8_t*)secondlast_word_ptr;
    secondlast_byte_0   =   (*(byte_ptr)   & 0xff);
    secondlast_byte_1   =   (*(byte_ptr+1) & 0xff);
    secondlast_byte_2   =   (*(byte_ptr+2) & 0xff);
    secondlast_byte_3   =   (*(byte_ptr+3) & 0xff);


    switch((datalen & 0x3)) {
        case 0:
            pulse_bw_info       = secondlast_byte_3;
            pulse_length_ext    = secondlast_byte_2;
            pulse_length_pri    = secondlast_byte_1;
            byte_ptr            = (u_int8_t*)secondlast_word_ptr;
            fft_data_end_ptr    = (byte_ptr);
            break;
        case 1:
            pulse_bw_info       = last_byte_0;
            pulse_length_ext    = secondlast_byte_3;
            pulse_length_pri    = secondlast_byte_2;
            byte_ptr            = (u_int8_t*)secondlast_word_ptr;
            fft_data_end_ptr    = (byte_ptr+2);
            break;
        case 2:
            pulse_bw_info       = last_byte_1;
            pulse_length_ext    = last_byte_0;
            pulse_length_pri    = secondlast_byte_3;
            byte_ptr            = (u_int8_t*)secondlast_word_ptr;
            fft_data_end_ptr    = (byte_ptr+3);
            break;
        case 3:
            pulse_bw_info       = last_byte_2;
            pulse_length_ext    = last_byte_1;
            pulse_length_pri    = last_byte_0;
            byte_ptr            = (u_int8_t*)last_word_ptr;
            fft_data_end_ptr    = (byte_ptr);
            break;
        default:
             printk(  "datalen mod4=%d spectral_data_len=%d\n", (datalen%4), spectral->spectral_data_len);
    }

    /*
     * Only the last 3 bits of the BW info are relevant,
     * they indicate which channel the radar was detected in.
     */
    pulse_bw_info &= 0x17;

    if (pulse_bw_info & SPECTRAL_SCAN_DATA) {

        if (datalen > spectral->spectral_data_len + 2) {
            //printk("Invalid spectral scan datalen = %d\n", datalen);
            return;
        }

        if (spectral->num_spectral_data == 0) {
            spectral->first_tstamp = tstamp;
            spectral->max_rssi = -110;
            //printk( "First FFT data tstamp = %u rssi=%d\n", tstamp, fix_rssi_inv_only(combined_rssi));
        }

        spectral->num_spectral_data++;

        OS_MEMZERO(&fft_40, sizeof (fft_40));
        OS_MEMZERO(&fft_20, sizeof (fft_20));

        if (spectral->sc_spectral_20_40_mode) {
            fft_data_ptr = (u_int8_t*)&fft_40.lower_bins.bin_magnitude[0];
        } else {
            fft_data_ptr = (u_int8_t*)&fft_20.lower_bins.bin_magnitude[0];
        }

        byte_ptr = fft_data_ptr;

        if (datalen == spectral->spectral_data_len) {

            if (spectral->sc_spectral_20_40_mode) {
                // HT40 packet, correct length
                OS_MEMCPY(&fft_40, (u_int8_t*)(bf->bf_vdata), MIN(datalen,sizeof(fft_40)));
            } else {
                // HT20 packet, correct length
                OS_MEMCPY(&fft_20, (u_int8_t*)(bf->bf_vdata), MIN(datalen,sizeof(fft_20)));
            }

        }

      /* This happens when there is a missing byte because CCK is enabled.
       * This may happen with or without the second bug of the MAC inserting
       * 2 bytes
       */
      if ((datalen == (spectral->spectral_data_len - 1)) ||
          (datalen == (spectral->spectral_data_len + 1))) {

          printk(  "%s %d missing 1 byte datalen=%d expected=%d\n", __func__, __LINE__, datalen, spectral->spectral_data_len);

          already_copied++;

          if (spectral->sc_spectral_20_40_mode) {
              // HT40 packet, missing 1 byte
              // Use the beginning byte as byte 0 and byte 1
              fft_40.lower_bins.bin_magnitude[0]=*(u_int8_t*)(bf->bf_vdata);
              // Now copy over the rest
              fft_data_ptr = (u_int8_t*)&fft_40.lower_bins.bin_magnitude[1];
              /* The first byte in destination buffer is already populated with duplicated bin,
               * hence available destination  buffer space is reduced by one.
               */
              OS_MEMCPY(fft_data_ptr, (u_int8_t*)(bf->bf_vdata), MIN(datalen,(sizeof(fft_40)-1)));
          } else {
              // HT20 packet, missing 1 byte
              // Use the beginning byte as byte 0 and byte 1
              fft_20.lower_bins.bin_magnitude[0]=*(u_int8_t*)(bf->bf_vdata);
              // Now copy over the rest
              fft_data_ptr = (u_int8_t*)&fft_20.lower_bins.bin_magnitude[1];
              /* The first byte in destination buffer is already populated with duplicated bin,
               *  hence available destination buffer space is reduced by one.
               */
              OS_MEMCPY(fft_data_ptr, (u_int8_t*)(bf->bf_vdata), MIN(datalen,(sizeof(fft_20)-1)));
          }
      }

      if ((datalen == (spectral->spectral_data_len + 1)) ||
          (datalen == (spectral->spectral_data_len + 2))) {

          //printk(  "%s %d extra bytes datalen=%d expected=%d\n", __func__, __LINE__, datalen, spectral->spectral_data_len);

          if (spectral->sc_spectral_20_40_mode) {// HT40 packet, MAC added 2 extra bytes
              fft_src_ptr = (u_int8_t*)(bf->bf_vdata);

              fft_data_ptr = (u_int8_t*)&fft_40.lower_bins.bin_magnitude[already_copied];

              for( i = 0, newdatalen=0; (i < datalen) && (newdatalen < (sizeof(fft_40)- already_copied)); i++) {
                if (i == 30 || i == 32) {
                  continue;
                }

                *(fft_data_ptr+newdatalen)=*(fft_src_ptr+i);
                newdatalen++;
               }
          } else { //HT20 packet, MAC added 2 extra bytes
              fft_src_ptr = (u_int8_t*)(bf->bf_vdata);
              fft_data_ptr = (u_int8_t*)&fft_20.lower_bins.bin_magnitude[already_copied];

              for(i=0, newdatalen=0; (i < datalen) && (newdatalen < (sizeof(fft_20) - already_copied)); i++) {
                if (i == 30 || i == 32)
                  continue;

                *(fft_data_ptr+newdatalen)=*(fft_src_ptr+i);
                newdatalen++;
              }
          }
      }

      spectral->total_spectral_data++;

      dc_index = spectral->spectral_dc_index;

      if (spectral->sc_spectral_20_40_mode) {
            max_exp     = (fft_40.max_exp & 0x07);
            byte_ptr    = (u_int8_t*)&fft_40.lower_bins.bin_magnitude[0];

             /* Correct the DC bin value */
            lower_dc = *(byte_ptr+dc_index-1);
            upper_dc = *(byte_ptr+dc_index+1);
            *(byte_ptr+dc_index)=((upper_dc + lower_dc)/2);

       } else {
             max_exp    = (fft_20.max_exp & 0x07);
             byte_ptr   = (u_int8_t*)&fft_20.lower_bins.bin_magnitude[0];

            /* Correct the DC bin value */
            lower_dc = *(byte_ptr+dc_index-1);
            upper_dc = *(byte_ptr+dc_index+1);
            *(byte_ptr+dc_index)=((upper_dc + lower_dc)/2);

       }

        if (p_sops->get_ent_spectral_mask(spectral)) {
            *(byte_ptr+dc_index) &=
                ~((1 << p_sops->get_ent_spectral_mask(spectral)) - 1);
        }

        if (max_exp < 1) {
            max_scale = 1;
        } else {
            max_scale = (2) << (max_exp - 1);
        }

        bin_pwr_count = spectral->spectral_numbins;

        if ((last_tsf  > fulltsf) && (!spectral->classify_scan)) {
            spectral->send_single_packet = 1;
            last_tsf = fulltsf;
        }

        inv_combined_rssi   = fix_rssi_inv_only(combined_rssi);
        inv_control_rssi    = fix_rssi_inv_only(control_rssi);
        inv_extension_rssi  = fix_rssi_inv_only(extension_rssi);

        {
            if (spectral->upper_is_control)
              rssi_up = control_rssi;
            else
              rssi_up = extension_rssi;

            if (spectral->lower_is_control)
              rssi_low = control_rssi;
            else
              rssi_low = extension_rssi;

            nfc_control_rssi = get_nfc_ctl_rssi(spectral, inv_control_rssi, &temp_nf);
            ctl_chan_noise_floor = temp_nf;
            rssi_low = fix_rssi_for_classifier(spectral, rssi_low, spectral->lower_is_control, spectral->lower_is_extension);

            if (spectral->sc_spectral_20_40_mode) {
              rssi_up = fix_rssi_for_classifier(spectral, rssi_up,spectral->upper_is_control,spectral->upper_is_extension);
              nfc_extension_rssi = get_nfc_ext_rssi(spectral, inv_extension_rssi, &temp_nf);
              ext_chan_noise_floor = temp_nf;
            }
        }
        if(sc->sc_ieee_ops->spectral_eacs_update) {
            sc->sc_ieee_ops->spectral_eacs_update(sc->sc_ieee, nfc_control_rssi,
                                                  nfc_extension_rssi, ctl_chan_noise_floor,
                                                                       ext_chan_noise_floor);
        }

        if (!spectral->sc_spectral_20_40_mode) {
            rssi_up             = 0;
            extension_rssi      = 0;
            inv_extension_rssi  = 0;
            nb_upper            = 0;
            maxindex_upper      = 0;
            maxmag_upper        = 0;
        }

        params.rssi         = inv_combined_rssi;
        params.lower_rssi   = rssi_low;
        params.upper_rssi   = rssi_up;

        if (spectral->sc_spectral_noise_pwr_cal) {
            params.chain_ctl_rssi[0] = fix_rssi_inv_only(rxs->rs_rssi_ctl0);
            params.chain_ctl_rssi[1] = fix_rssi_inv_only(rxs->rs_rssi_ctl1);
            params.chain_ctl_rssi[2] = fix_rssi_inv_only(rxs->rs_rssi_ctl2);
            params.chain_ext_rssi[0] = fix_rssi_inv_only(rxs->rs_rssi_ext0);
            params.chain_ext_rssi[1] = fix_rssi_inv_only(rxs->rs_rssi_ext1);
            params.chain_ext_rssi[2] = fix_rssi_inv_only(rxs->rs_rssi_ext2);
        }

        params.bwinfo       = pulse_bw_info;
        params.tstamp       = tstamp;
        params.max_mag      = max_mag;
        params.max_index    = max_index;
        params.max_exp      = max_scale;
        params.peak         = peak_freq;
        params.pwr_count    = bin_pwr_count;
        params.bin_pwr_data = &byte_ptr;
        params.freq         = p_sops->get_current_channel(spectral);
        if(sc->sc_ieee_ops->spectral_get_freq_loading) {
            params.freq_loading = sc->sc_ieee_ops->spectral_get_freq_loading(sc->sc_ieee);
        }
        else {
            params.freq_loading = 0;
        }
        params.interf_list.count = 0;
        params.max_lower_index   = 0;//maxindex_lower;
        params.max_upper_index   = 0;//maxindex_upper;
        params.nb_lower          = nb_lower;
        params.nb_upper          = nb_upper;
        params.last_tstamp       = spectral->last_tstamp;

        get_nfc_ctl_rssi(spectral, inv_control_rssi, &temp_nf);

        params.noise_floor = (int16_t)temp_nf;

        OS_MEMCPY(&params.classifier_params, &spectral->classifier_params, sizeof(struct spectral_classifier_params));

        cmp_rssi = fix_rssi_inv_only (combined_rssi);
        spectral->send_single_packet = 0;

#ifdef SPECTRAL_DEBUG_TIMER
        OS_CANCEL_TIMER(&spectral->debug_timer);
#endif
        spectral->spectral_sent_msg++;
        params.datalen = datalen;

        if (spectral->sc_spectral_20_40_mode) {
            data_ptr = (u_int8_t*)&fft_40.lower_bins.bin_magnitude;
        } else {
            data_ptr = (u_int8_t*)&fft_20.lower_bins.bin_magnitude;
        }

        byte_ptr = (u_int8_t*)data_ptr;
        params.bin_pwr_data = &byte_ptr;

        send_tstamp = p_sops->get_tsf64(spectral);
        spectral_create_samp_msg(spectral, &params);

#ifdef SPECTRAL_CLASSIFIER_IN_KERNEL

        if (spectral->classify_scan && maxindex_lower != (SPECTRAL_HT20_DC_INDEX + 1)) {

            classifier(&spectral->bd_lower, tstamp, spectral->last_tstamp, rssi_low, nb_lower, maxindex_lower);

            if (spectral->sc_spectral_20_40_mode && maxindex_upper != (SPECTRAL_HT40_DC_INDEX)) {
                classifier(&spectral->bd_upper, tstamp, spectral->last_tstamp, rssi_up, nb_upper, maxindex_upper);
            }

            print_detection(sc);
        }

#endif /* SPECTRAL_CLASSIFIER_IN_KERNEL */

        spectral->last_tstamp=tstamp;

        return;
  } else {

      /*
       *  Add a HAL capability that tests if chip is capable of spectral scan.
       *  Probably just a check if its a Merlin and above.
       */
      spectral->ath_spectral_stats.owl_phy_errors++;
  }
#undef EXT_CH_RADAR_FOUND
#undef PRI_CH_RADAR_FOUND
#undef EXT_CH_RADAR_EARLY_FOUND
#undef SPECTRAL_SCAN_DATA
}
#endif
