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

#if WLAN_SPECTRAL_ENABLE
#include "spectral.h" 
#include "spectralscan_classifier.h" 

void init_classifier(struct ath_softc *sc)
{
  struct ath_spectral *spectral=sc->sc_spectral;
  struct ss *lwrband, *uprband;

  lwrband = &spectral->bd_lower;
  uprband = &spectral->bd_upper;

  OS_MEMZERO(lwrband, sizeof(struct ss));
  OS_MEMZERO(uprband, sizeof(struct ss));

 if (spectral->sc_spectral_20_40_mode) {

    /* AP is in 20-40 mode so we will need to process both extension and primary channel spectral data */
    if (sc->sc_extchan.channel < sc->sc_curchan.channel) { 
        lwrband->b_chan_in_mhz = sc->sc_extchan.channel;
        uprband->b_chan_in_mhz = sc->sc_curchan.channel;
        uprband->dc_in_mhz = (uprband->b_chan_in_mhz - 10);
        uprband->lwr_band_data = 1;
        lwrband->lwr_band_data = 0;
    } else {
        lwrband->b_chan_in_mhz = sc->sc_curchan.channel;
        uprband->b_chan_in_mhz = sc->sc_extchan.channel;
        uprband->dc_in_mhz = (uprband->b_chan_in_mhz + 10);
        uprband->lwr_band_data = 0;
        lwrband->lwr_band_data = 1;
    }

    uprband->dynamic_ht2040_en = spectral->sc_spectral_20_40_mode;
    uprband->dc_index = spectral->spectral_dc_index;
    SPECTRAL_DPRINTK(sc, ATH_DEBUG_SPECTRAL1,"uprband->b_chan_in_mhz = %d\n", uprband->b_chan_in_mhz);
  SPECTRAL_DPRINTK(sc, ATH_DEBUG_SPECTRAL1,"uprband->dc_in_mhz = %d\n", uprband->dc_in_mhz);

  lwrband->dc_in_mhz = uprband->dc_in_mhz;

 } else {
        /* AP is in 20MHz mode so only primary channel spectral data counts*/
        lwrband->b_chan_in_mhz = sc->sc_curchan.channel;
        lwrband->dc_in_mhz = lwrband->b_chan_in_mhz;
        lwrband->lwr_band_data = 1;
    }

  lwrband->dynamic_ht2040_en = spectral->sc_spectral_20_40_mode;
  lwrband->dc_index = spectral->spectral_dc_index;
  SPECTRAL_DPRINTK(sc, ATH_DEBUG_SPECTRAL1,"lwrband->b_chan_in_mhz = %d\n", lwrband->b_chan_in_mhz);
  SPECTRAL_DPRINTK(sc, ATH_DEBUG_SPECTRAL1,"lwrband->dc_in_mhz = %d\n", lwrband->dc_in_mhz);
}


void classifier_initialize(struct ss *spectral_lower, struct ss *spectral_upper)
{
  
  struct ss *lwrband, *uprband;
  lwrband = spectral_lower;
  uprband = spectral_upper;

 lwrband->print_to_screen=1;
 uprband->print_to_screen=1;

  lwrband->max_bin = INT_MIN;
  lwrband->min_bin = INT_MAX;
  uprband->max_bin = INT_MIN;
  uprband->min_bin = INT_MAX;

  lwrband->count_vbr = 0;
  uprband->count_vbr = 0;
  lwrband->count_bth = 0;
  uprband->count_bth = 0;
  lwrband->count_bts = 0;
  uprband->count_bts = 0;
  lwrband->count_cwa = 0;
  uprband->count_cwa = 0;
  lwrband->count_wln = 0;
  uprband->count_wln = 0;
  lwrband->count_mwo = 0;
  uprband->count_mwo = 0;
  lwrband->count_cph = 0;
  uprband->count_cph = 0; 
  lwrband->num_identified_short_bursts = 0;
  uprband->num_identified_short_bursts = 0;
  lwrband->num_identified_bursts = 0;
  uprband->num_identified_bursts = 0;
  lwrband->previous_nb = 0;
  uprband->previous_nb = 0;
  lwrband->previous_wb = 0;
  uprband->previous_wb = 0;
  lwrband->prev_burst_stop = 0;
  uprband->prev_burst_stop = 0;
  lwrband->average_rssi_total = 0;
  uprband->average_rssi_total = 0;
  lwrband->average_rssi_numbr = 1;
  uprband->average_rssi_numbr = 1;

  reset_vbr(lwrband);
  reset_vbr(uprband);
  reset_bth(lwrband);
  reset_bth(uprband);
  reset_bts(lwrband);
  reset_bts(uprband);
  reset_cph(lwrband);
  reset_cph(uprband);
  reset_cwa(lwrband);
  reset_cwa(uprband);
  reset_mwo(lwrband);
  reset_mwo(uprband);
  reset_wln(lwrband);
  reset_wln(uprband);

  return;
}
#endif

