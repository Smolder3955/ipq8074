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
#include <osdep.h>
#include "ah.h"

#if WLAN_SPECTRAL_ENABLE

#include "spectral.h"

void send_fake_ht40_data(struct ath_softc *sc)
{
    struct ath_spectral *spectral=sc->sc_spectral;
    u_int8_t *bin_data_ptr;
    HT40_FFT_PACKET fake_fft_40;
    struct samp_msg_params fake_params;

    OS_MEMZERO(&fake_params, sizeof(fake_params));
    fake_ht40_data_packet(&fake_fft_40,sc);
    //spectral_debug_level=ATH_DEBUG_SPECTRAL3;
    //print_fft_ht40_packet(&fake_fft_40, sc);
    //spectral_debug_level=ATH_DEBUG_SPECTRAL;

    bin_data_ptr=(u_int8_t*)&fake_fft_40.lower_bins.bin_magnitude[0];

    fake_params.pwr_count=spectral->spectral_numbins; 

    fake_params.sc=sc;
    fake_params.bin_pwr_data=&bin_data_ptr;
    fake_params.freq=sc->sc_curchan.channel; 
    fake_params.interf_list.count=0; 

    //Thu Nov  6 14:01:17 PST 2008 - add interference report to faked data
    spectral_add_interf_samp_msg(&fake_params, sc);
    //spectral_debug_level=ATH_DEBUG_SPECTRAL3;
    //print_samp_msg_params(sc, &fake_params);
    spectral_create_samp_msg(&fake_params);
    //spectral_debug_level=ATH_DEBUG_SPECTRAL;
    SPECTRAL_DPRINTK(sc, ATH_DEBUG_SPECTRAL2,"===================%s %d freq=%u =====================\n", __func__, __LINE__, fake_params.freq); 
    return;
}

void spectral_add_interf_samp_msg(struct samp_msg_params *params, struct ath_softc *sc)
{
  struct ath_spectral *spectral=sc->sc_spectral;
  struct ss *lwrband, *uprband;
  struct interf_src_rsp *interf_src_rsp = &params->interf_list;
  int interf_index=0;

  lwrband = &spectral->bd_lower;
  uprband = &spectral->bd_upper;

  //printk("%s %d\n", __func__, __LINE__);
  OS_MEMZERO(interf_src_rsp, sizeof(struct interf_src_rsp));
  interf_src_rsp->count=0; 

   if(lwrband->count_cph) {
        interf_index=interf_src_rsp->count;
        interf_src_rsp->interf[interf_index].interf_type=INTERF_DECT;
        interf_src_rsp->interf[interf_index].interf_min_freq=(lwrband->cph_min_freq/1000);
 interf_src_rsp->interf[interf_index].interf_max_freq=(lwrband->cph_max_freq/1000);
        interf_src_rsp->count++;
        SPECTRAL_DPRINTK(sc, ATH_DEBUG_SPECTRAL2,"%s %d interf=%d\n", __func__, __LINE__, interf_src_rsp->interf[interf_index].interf_type);
   }
   if(uprband->count_cph) {
        interf_index=interf_src_rsp->count;
        interf_src_rsp->interf[interf_index].interf_type=INTERF_DECT;
        interf_src_rsp->interf[interf_index].interf_min_freq=(lwrband->cph_min_freq/1000);
        interf_src_rsp->interf[interf_index].interf_max_freq=(lwrband->cph_max_freq/1000);
        interf_src_rsp->count++;
         SPECTRAL_DPRINTK(sc, ATH_DEBUG_SPECTRAL2,"%s %d interf=%d\n", __func__, __LINE__, interf_src_rsp->interf[interf_index].interf_type);
    }
   if(lwrband->count_cwa) {
        interf_index=interf_src_rsp->count;
        interf_src_rsp->interf[interf_index].interf_type=INTERF_TONE;
        interf_src_rsp->interf[interf_index].interf_min_freq=(lwrband->cwa_min_freq/1000);
        interf_src_rsp->interf[interf_index].interf_max_freq=(lwrband->cwa_max_freq/1000);
        interf_src_rsp->count++;
        SPECTRAL_DPRINTK(sc, ATH_DEBUG_SPECTRAL2,"%s %d interf=%d\n", __func__, __LINE__, interf_src_rsp->interf[interf_index].interf_type);
   }
   if(uprband->count_cwa) {
        interf_index=interf_src_rsp->count;
        interf_src_rsp->interf[interf_index].interf_type=INTERF_TONE;
        interf_src_rsp->interf[interf_index].interf_min_freq=lwrband->cwa_min_freq/1000;
        interf_src_rsp->interf[interf_index].interf_max_freq=lwrband->cwa_max_freq/1000;
        interf_src_rsp->count++;
        SPECTRAL_DPRINTK(sc, ATH_DEBUG_SPECTRAL2,"%s %d interf=%d\n", __func__, __LINE__, interf_src_rsp->interf[interf_index].interf_type);
    }
   if(lwrband->count_bts) {
        interf_index=interf_src_rsp->count;
        interf_src_rsp->interf[interf_index].interf_type=INTERF_BT;
        interf_src_rsp->interf[interf_index].interf_min_freq=lwrband->bts_min_freq/1000;
        interf_src_rsp->interf[interf_index].interf_max_freq=lwrband->bts_max_freq/1000;
        interf_src_rsp->count++;
        SPECTRAL_DPRINTK(sc, ATH_DEBUG_SPECTRAL2,"%s %d interf=%d\n", __func__, __LINE__, interf_src_rsp->interf[interf_index].interf_type);
   }
   if(uprband->count_bts) {
        interf_index=interf_src_rsp->count;
        interf_src_rsp->interf[interf_index].interf_type=INTERF_BT;
        interf_src_rsp->interf[interf_index].interf_min_freq=lwrband->bts_min_freq/1000;
        interf_src_rsp->interf[interf_index].interf_max_freq=lwrband->bts_max_freq/1000;
        interf_src_rsp->count++;
        SPECTRAL_DPRINTK(sc, ATH_DEBUG_SPECTRAL2,"%s %d interf=%d\n", __func__, __LINE__, interf_src_rsp->interf[interf_index].interf_type);
}
   if(lwrband->count_bth) {
        interf_index=interf_src_rsp->count;
        interf_src_rsp->interf[interf_index].interf_type=INTERF_BT;
        interf_src_rsp->interf[interf_index].interf_min_freq=lwrband->bth_min_freq/1000;
        interf_src_rsp->interf[interf_index].interf_max_freq=lwrband->bth_max_freq/1000;
        interf_src_rsp->count++;
        SPECTRAL_DPRINTK(sc, ATH_DEBUG_SPECTRAL2,"%s %d interf=%d\n", __func__, __LINE__, interf_src_rsp->interf[interf_index].interf_type);
   }
   if(uprband->count_bth) {
        interf_index=interf_src_rsp->count;
        interf_src_rsp->interf[interf_index].interf_type=INTERF_BT;
        interf_src_rsp->interf[interf_index].interf_min_freq=lwrband->bth_min_freq/1000;
        interf_src_rsp->interf[interf_index].interf_max_freq=lwrband->bth_max_freq/1000;
        interf_src_rsp->count++;
        SPECTRAL_DPRINTK(sc, ATH_DEBUG_SPECTRAL2,"%s %d interf=%d\n", __func__, __LINE__, interf_src_rsp->interf[interf_index].interf_type);
    }
   if(lwrband->count_mwo) {
        interf_index=interf_src_rsp->count;
        interf_src_rsp->interf[interf_index].interf_type=INTERF_MW;
        interf_src_rsp->interf[interf_index].interf_min_freq=lwrband->mwo_min_freq/1000;
        interf_src_rsp->interf[interf_index].interf_max_freq=lwrband->mwo_max_freq/1000;
        interf_src_rsp->count++;
        SPECTRAL_DPRINTK(sc, ATH_DEBUG_SPECTRAL2,"%s %d interf=%d\n", __func__, __LINE__, interf_src_rsp->interf[interf_index].interf_type);
   }
   if(uprband->count_mwo) {
        interf_index=interf_src_rsp->count;
        interf_src_rsp->interf[interf_index].interf_type=INTERF_MW;
        interf_src_rsp->interf[interf_index].interf_min_freq=lwrband->mwo_min_freq/1000;
        interf_src_rsp->interf[interf_index].interf_max_freq=lwrband->mwo_max_freq/1000;
        interf_src_rsp->count++;
        SPECTRAL_DPRINTK(sc, ATH_DEBUG_SPECTRAL2,"%s %d interf=%d\n", __func__, __LINE__, interf_src_rsp->interf[interf_index].interf_type);
}
  //printk("%s %d\n", __func__, __LINE__);
}

void print_samp_msg (struct spectral_samp_msg *samp, struct ath_softc *sc)
{
    int i=0;

    struct spectral_samp_data *data = &(samp->samp_data);
    SPECTRAL_DPRINTK(sc, ATH_DEBUG_SPECTRAL2, "%s struct spectral_samp_msg\n", __func__);

    SPECTRAL_DPRINTK(sc, ATH_DEBUG_SPECTRAL2,"freq=%d \n", samp->freq);

    SPECTRAL_DPRINTK(sc, ATH_DEBUG_SPECTRAL2,"datalen=%d\n", data->spectral_data_len);
    SPECTRAL_DPRINTK(sc, ATH_DEBUG_SPECTRAL2,"rssi=%d\n", data->spectral_rssi);
    SPECTRAL_DPRINTK(sc, ATH_DEBUG_SPECTRAL2, "comb_rssi=%d\n", data->spectral_combined_rssi);
    SPECTRAL_DPRINTK(sc, ATH_DEBUG_SPECTRAL2, "max_scale=%d\n", data->spectral_max_scale);
    SPECTRAL_DPRINTK(sc, ATH_DEBUG_SPECTRAL2, "bwinfo=%x\n", data->spectral_bwinfo);
    SPECTRAL_DPRINTK(sc, ATH_DEBUG_SPECTRAL2, "maxindex=%d\n", data->spectral_max_index);
    SPECTRAL_DPRINTK(sc, ATH_DEBUG_SPECTRAL2,"maxmag=%d\n", data->spectral_max_mag);
    SPECTRAL_DPRINTK(sc, ATH_DEBUG_SPECTRAL2,"maxexp=%d\n", data->spectral_max_exp);
    SPECTRAL_DPRINTK(sc, ATH_DEBUG_SPECTRAL2,"pwr_count=%d\n", data->bin_pwr_count);
    for (i=0; i< data->bin_pwr_count; i++) {
        SPECTRAL_DPRINTK(sc, ATH_DEBUG_SPECTRAL2,"(%d)=(0x%x) ", i, data->bin_pwr[i]);
    }
}

#undef SPECTRAL_DPRINTK 
#undef SPECTRAL_MIN
#undef SPECTRAL_MAX
#undef SPECTRAL_DIFF
#endif
