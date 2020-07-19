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

#include "ath_internal.h"

#if WLAN_SPECTRAL_ENABLE

#include "spectral.h"

void get_ht20_bmap_maxindex(struct ath_softc *sc, u_int8_t *fft_20_data, int datalen, u_int8_t *bmap_wt, u_int8_t *max_index);

int8_t fix_maxindex_inv_only(u_int8_t val)
{
    int8_t temp = val;
    int8_t inv  = 0;
    if (val > 31) {
        temp = val & 0x1f;
        inv = ((~(temp) & 0x1F)+ 1) & 0x1f;
        temp = 0 - inv;
    }
    return temp;
}

#ifdef OLD_MAGDATA_DEF
void process_ht20_mag_data(MAX_MAG_INDEX_DATA *imag, u_int16_t *mmag, u_int8_t *bmap_wt, u_int8_t *max_index)
{
    u_int16_t max_mag=0;
    u_int16_t temp=0;
    u_int8_t  maxindex;


    *bmap_wt= imag->bmap_wt;

    *max_index = imag->max_index_bits05;
    maxindex = *max_index;
    *max_index=fix_maxindex_inv_only (maxindex);
    *max_index += 29;

    max_mag = imag->max_mag_bits01;
    temp = imag->max_mag_bits29;
    max_mag = ((temp << 2) |  max_mag);

    temp = imag->max_mag_bits1110;
    max_mag = ((temp << 10) | max_mag);
    *mmag = max_mag;
}
#endif

void process_fft_ht20_packet(HT20_FFT_PACKET *fft_20, struct ath_softc *sc, u_int16_t *max_mag, u_int8_t* max_index, int *narrowband, int8_t *rssi, int *bmap)
{
    HT20_BIN_MAG_DATA *bmag=NULL;
    MAX_MAG_INDEX_DATA *imag=NULL;

    u_int8_t maxval=0, maxindex=0, *bindata=NULL, bmapwt;
    u_int16_t maxmag;
    int small_bitmap=0, high_rssi=0;
    int8_t minimum_rssi=5;
    u_int8_t  calc_maxindex=0, calc_strongbins=0, calc_maxval=0; 
    int one_over_wideband_min_large_bin_ratio=7, one_side_length = SPECTRAL_HT20_NUM_BINS;

    bmag = &(fft_20->lower_bins);
    bindata = bmag->bin_magnitude;

    maxval = return_max_value(bindata, SPECTRAL_HT20_NUM_BINS, &maxindex, sc, &calc_strongbins);

    calc_maxval = return_max_value(bindata, SPECTRAL_HT20_NUM_BINS, &calc_maxindex, sc, &calc_strongbins);

    imag = &(fft_20->lower_bins_max);
    //process_ht20_mag_data(imag, &maxmag, &bmapwt, &maxindex);
    process_mag_data(imag, &maxmag, &bmapwt, &maxindex);

    /* Do this only for HT20 mode */
    maxindex = fix_maxindex_inv_only(maxindex);
    maxindex += 29;

    *max_mag = maxmag;
    *max_index = maxindex;
    *bmap = bmapwt;

    SPECTRAL_DPRINTK(sc, ATH_DEBUG_SPECTRAL2,"strongbins=%d bmapwt=%d calc_maxval=%d maxmag=%d calc_maxindex=%d maxindex=%d\n",calc_strongbins, bmapwt, calc_maxval, maxmag, calc_maxindex, maxindex);

    small_bitmap = (bmapwt < 2);

    high_rssi = (*rssi > minimum_rssi);

    *narrowband = 0;       

    if((bmapwt * one_over_wideband_min_large_bin_ratio) < (one_side_length - 1)){
           *narrowband = 1;       
            SPECTRAL_DPRINTK(sc, ATH_DEBUG_SPECTRAL3,"%s nb=%d \n",__func__, *narrowband);
    } else {
            SPECTRAL_DPRINTK(sc, ATH_DEBUG_SPECTRAL3,"%s nb=%d \n",__func__, *narrowband);
    }
}

void get_ht20_bmap_maxindex(struct ath_softc *sc, u_int8_t *fft_20_data, int datalen, u_int8_t *bmap_wt, u_int8_t *max_index) 
{

    int offset = (SPECTRAL_HT20_TOTAL_DATA_LEN - datalen);
    u_int8_t temp_maxindex=0, temp_bmapwt=0;

    SPECTRAL_DPRINTK(sc, ATH_DEBUG_SPECTRAL1,"%s datalen=%d\n",__func__, datalen);    

    temp_maxindex = fft_20_data[SPECTRAL_HT20_MAXINDEX_INDEX + offset];

    temp_maxindex=(temp_maxindex & 0xFC);
    temp_maxindex=(temp_maxindex >> 2);

   //temp_maxindex = ((temp_maxindex >> 2) & 0x3F);
    temp_maxindex = fix_maxindex_inv_only(temp_maxindex);
    temp_maxindex += 29;

    temp_bmapwt=fft_20_data[SPECTRAL_HT20_BMAPWT_INDEX + offset];
    temp_bmapwt &= 0x3F;

    *bmap_wt = temp_bmapwt;
    *max_index = temp_maxindex;

}

#endif
