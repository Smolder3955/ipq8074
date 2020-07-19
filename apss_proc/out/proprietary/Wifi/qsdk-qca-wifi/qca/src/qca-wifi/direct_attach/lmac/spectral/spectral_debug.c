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

OS_TIMER_FUNC(spectral_classify_scan);

void disable_beacons(struct ath_softc *sc)
{
    SPECTRAL_DPRINTK(sc, DEBUG_SPECTRAL2,"Turning off SWBA and BMISS interrupts before imask = 0x%x\n", ath_hal_intrget(sc->sc_ah));
    sc->sc_imask &= ~(HAL_INT_SWBA | HAL_INT_BMISS);
    ath_hal_intrset(sc->sc_ah, sc->sc_imask);
    SPECTRAL_DPRINTK(sc, DEBUG_SPECTRAL2,"AFTER imask = 0x%x\n", ath_hal_intrget(sc->sc_ah));
}
void enable_beacons(struct ath_softc *sc)
{
    SPECTRAL_DPRINTK(sc, DEBUG_SPECTRAL2,"Turning ON SWBA and BMISS interrupts before imask = 0x%x\n", ath_hal_intrget(sc->sc_ah));
    sc->sc_imask |= (HAL_INT_SWBA | HAL_INT_BMISS);
    ath_hal_intrset(sc->sc_ah, sc->sc_imask);
    SPECTRAL_DPRINTK(sc, DEBUG_SPECTRAL2,"AFTER imask = 0x%x\n", ath_hal_intrget(sc->sc_ah));
}

void print_classifier_counts(struct ath_softc *sc, struct ss *bd, const char *print_str)
{
    if(bd->count_mwo || bd->count_bts || bd->count_bth || bd->count_cwa || bd->count_wln)
        SPECTRAL_DPRINTK(sc, DEBUG_SPECTRAL1,"%s\n", print_str);
    if(bd->count_mwo)
        SPECTRAL_DPRINTK(sc, DEBUG_SPECTRAL1, "count_mwo=%d\n",bd->count_mwo);
    if(bd->count_bts) {
        SPECTRAL_DPRINTK(sc, DEBUG_SPECTRAL2, "count_bts=%d ",bd->count_bts);
        SPECTRAL_DPRINTK(sc, DEBUG_SPECTRAL2, "bts_min_freq=%d ",bd->bts_min_freq);
        SPECTRAL_DPRINTK(sc, DEBUG_SPECTRAL2, "bts_max_freq=%d\n",bd->bts_max_freq);
    }
    if(bd->count_bth)
    SPECTRAL_DPRINTK(sc, DEBUG_SPECTRAL2, "count_bth=%d\n",bd->count_bth);

    if(bd->count_cph) {
        SPECTRAL_DPRINTK(sc, DEBUG_SPECTRAL2, "count_cph=%d ",bd->count_cph);
        SPECTRAL_DPRINTK(sc, DEBUG_SPECTRAL2, "cph_min_freq=%d ",bd->cph_min_freq);
        SPECTRAL_DPRINTK(sc, DEBUG_SPECTRAL2, "cph_max_freq=%d\n",bd->cph_max_freq);
    }
    if(bd->count_cwa){
        SPECTRAL_DPRINTK(sc, DEBUG_SPECTRAL1, "count_cwa=%d\n",bd->count_cwa);
        SPECTRAL_DPRINTK(sc, DEBUG_SPECTRAL1, "cwa_min_freq=%d ",bd->cwa_min_freq);
        SPECTRAL_DPRINTK(sc, DEBUG_SPECTRAL1, "cwa_max_freq=%d\n",bd->cwa_max_freq);
    }
    if(bd->count_wln)
    SPECTRAL_DPRINTK(sc, DEBUG_SPECTRAL1, "count_wln=%d\n",bd->count_wln);
    if(bd->count_vbr)
    SPECTRAL_DPRINTK(sc, DEBUG_SPECTRAL1, "count_vbr=%d\n",bd->count_vbr);

}

void print_detection(struct ath_softc *sc)
{
    struct ath_spectral *spectral=sc->sc_spectral;

    if(spectral->lower_is_control)
        print_classifier_counts(sc, &spectral->bd_lower, "==CONTROL==");
    if(spectral->lower_is_extension)
        print_classifier_counts(sc, &spectral->bd_lower, "==EXTENSION==");

    if(spectral->upper_is_control)
        print_classifier_counts(sc, &spectral->bd_upper, "==CONTROL==");
    if(spectral->upper_is_extension)
        print_classifier_counts(sc, &spectral->bd_upper, "==EXTENSION==");
}

void fake_ht40_data_packet(HT40_FFT_PACKET *ht40pkt, struct ath_softc *sc)
{
    int i;
    HT40_BIN_MAG_DATA *lower_bmag= &(ht40pkt->lower_bins);
    HT40_BIN_MAG_DATA *upper_bmag= &(ht40pkt->upper_bins);

    OS_MEMZERO(ht40pkt, sizeof(HT40_FFT_PACKET));
    for (i=0; i < SPECTRAL_HT40_NUM_BINS;i++) {
         lower_bmag->bin_magnitude[i]=255;
         upper_bmag->bin_magnitude[i]=255;
    }
    return;
}

void print_fft_ht40_bytes(HT40_FFT_PACKET *fft_40, struct ath_softc *sc)
{
    int i, size_fft = sizeof(HT40_FFT_PACKET);

    for(i=0;i< size_fft; i++) {
        SPECTRAL_DPRINTK(sc,DEBUG_SPECTRAL3, "%3x ",*(((u_int8_t*)fft_40) + i));
    }
    SPECTRAL_DPRINTK(sc,DEBUG_SPECTRAL3, "%s","\n");
}    

void print_phy_err_stats(struct ath_softc *sc)
{
    struct ath_phy_stats *phy_stats = &sc->sc_phy_stats[sc->sc_curmode];
    struct ath_spectral *spectral = sc->sc_spectral;
    
    int i;

    SPECTRAL_DPRINTK(sc, DEBUG_SPECTRAL, "Total PHYERR = %llu\n", (long long unsigned int) phy_stats->ast_rx_phyerr);
    for (i=0; i < 32; i++) {
        if (phy_stats->ast_rx_phy[i] != 0)
        SPECTRAL_DPRINTK(sc, DEBUG_SPECTRAL, "rx_phy[%d] = %llu\n", i, (long long unsigned int) phy_stats->ast_rx_phy[i]);
    }
    SPECTRAL_DPRINTK(sc, DEBUG_SPECTRAL, "%s total_phy_errors=%d\n", __func__, spectral->ath_spectral_stats.total_phy_errors);
}

void print_config_regs(struct ath_softc *sc)
{
#ifdef DBG
#define NUM_REGS 9
    struct ath_hal *ah = sc->sc_ah;
    int i,reg_addr[NUM_REGS]={0x806c, 0x9840, 0x9858, 0x9864,0x9910,0x9924, 0x9940,0x9954,0x9970};

    for(i=0; i< NUM_REGS; i++) {
    SPECTRAL_DPRINTK(sc, DEBUG_SPECTRAL, "0x%x=0x%x\n",
                        reg_addr[i], 
                        ath_hal_readRegister(ah, reg_addr[i]));
    }
#undef NUM_REGS
#endif
}

void
printNoiseFloor(struct ath_softc *sc)
{
#ifdef DBG
#define SM(_v, _f)  (((_v) << _f##_S) & _f)
#define MS(_v, _f)  (((_v) & _f) >> _f##_S)

#define AR_PHY_CCA              0x9864
#define AR_PHY_MINCCA_PWR       0x0FF80000
#define AR_PHY_MINCCA_PWR_S     19
#define AR_PHY_CCA_THRESH62     0x0007F000
#define AR_PHY_CCA_THRESH62_S   12
#define AR9280_PHY_MINCCA_PWR       0x1FF00000
#define AR9280_PHY_MINCCA_PWR_S     20
#define AR_PHY_EXT_CCA          0x99bc
#define AR_PHY_EXT_MINCCA_PWR   0xFF800000
#define AR_PHY_EXT_MINCCA_PWR_S 23
#define AR9280_PHY_EXT_MINCCA_PWR       0x01FF0000
#define AR9280_PHY_EXT_MINCCA_PWR_S     16
#define AR9280_PHY_CCA_THRESH62     0x000FF000
#define AR9280_PHY_CCA_THRESH62_S   12
    int16_t nf, orig_nf;
    SPECTRAL_DPRINTK(sc, DEBUG_SPECTRAL2,"\nthresh62=0x%x\n",
                     (u_int8_t)MS(ath_hal_readRegister(sc->sc_ah, AR_PHY_CCA),
                                  AR9280_PHY_CCA_THRESH62));
    nf = MS(ath_hal_readRegister(sc->sc_ah, AR_PHY_CCA), AR9280_PHY_MINCCA_PWR);
    orig_nf=nf;
    if (nf & 0x100)
            nf = 0 - ((nf ^ 0x1ff) + 1);
    SPECTRAL_DPRINTK(sc, DEBUG_SPECTRAL2,"NF calibrated [ctl]"
                     " [chain 0] is %d orig_nf=%d\n", nf, orig_nf);
    nf = MS(ath_hal_readRegister(sc->sc_ah, AR_PHY_EXT_CCA),
            AR9280_PHY_EXT_MINCCA_PWR);
    orig_nf=nf;
    if (nf & 0x100)
            nf = 0 - ((nf ^ 0x1ff) + 1);
    SPECTRAL_DPRINTK(sc, DEBUG_SPECTRAL2,"NF calibrated [ext] [chain 0] is %d orig_nf=%d\n", nf, orig_nf);
#undef SM
#undef MS
#undef AR_PHY_CCA
#undef AR_PHY_MINCCA_PWR
#undef AR_PHY_MINCCA_PWR_S
#undef AR_PHY_CCA_THRESH62
#undef AR_PHY_CCA_THRESH62_S
#undef AR9280_PHY_MINCCA_PWR
#undef AR9280_PHY_MINCCA_PWR_S
#undef AR_PHY_EXT_CCA
#undef AR_PHY_EXT_MINCCA_PWR
#undef AR_PHY_EXT_MINCCA_PWR_S
#undef AR9280_PHY_EXT_MINCCA_PWR
#undef AR9280_PHY_CCA_THRESH62
#undef AR9280_PHY_CCA_THRESH62_S
#endif
}


void print_ht40_bin_mag_data(HT40_BIN_MAG_DATA *bmag, struct ath_softc *sc)
{
    int i;
    for (i=0; i< SPECTRAL_HT40_NUM_BINS; i++) {
        SPECTRAL_DPRINTK(sc, DEBUG_SPECTRAL3, "[%d]=0x%x ", i,bmag->bin_magnitude[i]);
    }
}
void print_max_mag_index_data(MAX_MAG_INDEX_DATA *imag, struct ath_softc *sc)
{
#ifdef OLD_MAXMAG_DATA_DEF
    u_int16_t max_mag=0;
    u_int16_t temp=0;

    SPECTRAL_DPRINTK(sc, DEBUG_SPECTRAL3, "max_mag_bits01=0x%x\n", imag->max_mag_bits01);
    max_mag = imag->max_mag_bits01;
    SPECTRAL_DPRINTK(sc, DEBUG_SPECTRAL3, "max_mag=0x%x\n", max_mag);
    //SPECTRAL_DPRINTK(sc, DEBUG_SPECTRAL3, "MASK max_mag_bits01=0x%x max_mag=0x%x\n", (imag->max_mag_bits01 & 0xC0), max_mag);
    SPECTRAL_DPRINTK(sc, DEBUG_SPECTRAL3, "bmap_wt=0x%x\n", imag->bmap_wt);
    SPECTRAL_DPRINTK(sc, DEBUG_SPECTRAL3, "MASK bmap_wt=0x%x\n", (imag->bmap_wt & 0x3F));
    SPECTRAL_DPRINTK(sc, DEBUG_SPECTRAL3, "max_mag_bits29=0x%x\n", imag->max_mag_bits29);
    temp = imag->max_mag_bits29;
    max_mag = ((temp << 2) |  max_mag);
    SPECTRAL_DPRINTK(sc, DEBUG_SPECTRAL3, "max_mag=0x%x\n", max_mag);
    SPECTRAL_DPRINTK(sc, DEBUG_SPECTRAL3, "max_index_bits05=0x%x\n", imag->max_index_bits05);
    //SPECTRAL_DPRINTK(sc, DEBUG_SPECTRAL3, "MASK max_index_bits05=0x%x\n", (imag->max_index_bits05 & 0xFC));
    SPECTRAL_DPRINTK(sc, DEBUG_SPECTRAL3, "max_max_bits1110=0x%x\n", imag->max_mag_bits1110);
    //SPECTRAL_DPRINTK(sc, DEBUG_SPECTRAL3, "MASK max_max_bits1110=0x%x\n", (imag->max_mag_bits1110 & 0x03));

    temp = imag->max_mag_bits1110;
    max_mag = ((temp << 10) | max_mag);
    SPECTRAL_DPRINTK(sc, DEBUG_SPECTRAL3, "max_mag=0x%x\n", max_mag);
#else

    int i=0, total_bytes=0;
    u_int8_t* print_ptr = (u_int8_t*)imag;

    SPECTRAL_DPRINTK(sc,DEBUG_SPECTRAL2, "%s\n","MAGNITUDE");
    for(i=0; i<sizeof(MAX_MAG_INDEX_DATA); i++, total_bytes++) {
        SPECTRAL_DPRINTK(sc,DEBUG_SPECTRAL2, "[%d]=%x\n",i,*print_ptr++);
    }

    SPECTRAL_DPRINTK(sc,DEBUG_SPECTRAL2, "MAX_EXP print_ptr=%pK\n",print_ptr);
    for(i=0; i<sizeof(u_int8_t); i++, total_bytes++) {
        SPECTRAL_DPRINTK(sc,DEBUG_SPECTRAL2, "[%d]=%x\n",i,*print_ptr++);
    }
#endif
}
void print_fft_ht40_packet(HT40_FFT_PACKET *fft_40, struct ath_softc *sc)
{
    HT40_BIN_MAG_DATA *bmag=NULL;
    MAX_MAG_INDEX_DATA *imag=NULL;
    u_int8_t maxval=0, maxindex=0, *bindata=NULL;

    SPECTRAL_DPRINTK(sc,DEBUG_SPECTRAL3, "%s LOWER BINS\n", __func__);
    bmag = &(fft_40->lower_bins);
    print_ht40_bin_mag_data(bmag, sc);

    bindata = bmag->bin_magnitude;
    //maxval = return_max_value(bindata, SPECTRAL_HT40_NUM_BINS, &maxindex, sc);
    SPECTRAL_DPRINTK(sc,DEBUG_SPECTRAL3, "\nLOWER BINS max_mag=0x%x max_index=%d\n",maxval, maxindex);

    SPECTRAL_DPRINTK(sc,DEBUG_SPECTRAL3, "\n%s UPPER BINS\n", __func__);
    bmag = &(fft_40->upper_bins);
    print_ht40_bin_mag_data(bmag, sc);

    bindata = bmag->bin_magnitude;
    //maxval = return_max_value(bindata, SPECTRAL_HT40_NUM_BINS, &maxindex, sc);
    SPECTRAL_DPRINTK(sc,DEBUG_SPECTRAL3, "\nUPPER BINS max_mag=0x%x max_index=%d\n",maxval, maxindex);

    SPECTRAL_DPRINTK(sc,DEBUG_SPECTRAL3, "\n%s LOWER BINS MAGNITUDE\n", __func__);
    imag = &(fft_40->lower_bins_max);
    print_max_mag_index_data(imag, sc);

    SPECTRAL_DPRINTK(sc,DEBUG_SPECTRAL3, "\n%s UPPER BINS MAGNITUDE\n", __func__);
    imag = &(fft_40->upper_bins_max);
    print_max_mag_index_data(imag, sc);

    SPECTRAL_DPRINTK(sc,DEBUG_SPECTRAL3, "\nmax_exp=%d\n",fft_40->max_exp);
    SPECTRAL_DPRINTK(sc,DEBUG_SPECTRAL3, "MASK max_exp=%d\n",(fft_40->max_exp & 0x0F));
}

void print_hex_fft_ht20_packet(HT20_FFT_PACKET *fft_20, struct ath_softc *sc)
{
    int i=0, total_bytes=0;    
    u_int8_t *print_ptr=(u_int8_t*)&(fft_20->lower_bins);

    SPECTRAL_DPRINTK(sc,DEBUG_SPECTRAL2, "BINS print_ptr=%pK\n",print_ptr);
    for(i=0; i<SPECTRAL_HT20_NUM_BINS; i++, total_bytes++) {
        SPECTRAL_DPRINTK(sc,DEBUG_SPECTRAL2, "[%d]=%x\n",i, *print_ptr++);
    }

    SPECTRAL_DPRINTK(sc,DEBUG_SPECTRAL2, "MAGNITUDE print_ptr=%pK\n",print_ptr);
    for(i=0; i<sizeof(MAX_MAG_INDEX_DATA); i++, total_bytes++) {
        SPECTRAL_DPRINTK(sc,DEBUG_SPECTRAL2, "[%d]=%x\n",i,*print_ptr++);
    }

    SPECTRAL_DPRINTK(sc,DEBUG_SPECTRAL2, "MAX_EXP print_ptr=%pK\n",print_ptr);
    for(i=0; i<sizeof(u_int8_t); i++, total_bytes++) {
        SPECTRAL_DPRINTK(sc,DEBUG_SPECTRAL2, "[%d]=%x\n",i,*print_ptr++);
    }
    SPECTRAL_DPRINTK(sc,DEBUG_SPECTRAL2, "DONE print_ptr=%pK, sizeof(HT20_FFT_PACKET)=%d total_bytes=%d\n",print_ptr, sizeof(HT20_FFT_PACKET), total_bytes);
}

#ifdef SPECTRAL_DEBUG_TIMER
/*Start this timer when issuing a start scan command. This timer will 
only go off if no data has been received and no packet has been sent after the initial start scan command. If this timer times out that means, we may have hit a spectral scan hang*/
OS_TIMER_FUNC(spectral_debug_timeout)
{
#define AR_OBS_BUS_1 0x806c

        struct ath_softc *sc;
        struct ath_spectral *spectral;
        OS_GET_TIMER_ARG(sc, struct ath_softc *);

        spectral=sc->sc_spectral;

        OS_CANCEL_TIMER(&spectral->debug_timer);
        spectral_get_thresholds(sc, &sc->sc_spectral->params);
#ifdef DBG
        SPECTRAL_DPRINTK(sc, DEBUG_SPECTRAL, "==============SPECTRAL DEBUG TIMEOUT============== channel %d\n", sc->sc_curchan.channel);
        SPECTRAL_DPRINTK(sc, DEBUG_SPECTRAL, "0x%x=0x%x\n", AR_OBS_BUS_1, ath_hal_readRegister(sc->sc_ah, AR_OBS_BUS_1));
        SPECTRAL_DPRINTK(sc, DEBUG_SPECTRAL, "0x%x=0x%x\n", 0x0008, ath_hal_readRegister(sc->sc_ah, 0x0008));
#endif
        print_config_regs(sc);
        printNoiseFloor(sc->sc_ah);
        SPECTRAL_DPRINTK(sc, DEBUG_SPECTRAL, "send_single=%d sent_msg=%d spectral_scan=%d\n",
                spectral->send_single_packet,
                spectral->spectral_sent_msg,
                spectral->sc_spectral_scan);
        SPECTRAL_DPRINTK(sc, DEBUG_SPECTRAL2,"total_recv=%d this_scan=%d last_tstamp=0x%x\n", sc->sc_spectral->total_spectral_data, sc->sc_spectral->num_spectral_data, sc->sc_spectral->last_tstamp);
        print_phy_err_stats(sc);
        if (sc->sc_spectral->num_spectral_data < spectral->params.ss_count || spectral->params.ss_count == 128) {
            SPECTRAL_DPRINTK(sc, DEBUG_SPECTRAL2,"Expected %d reports got %d RESTART scan\n", spectral->params.ss_count, sc->sc_spectral->num_spectral_data);
            ATH_PS_WAKEUP(sc);
            ath_hal_stop_spectral_scan(sc->sc_ah);
            spectral->send_single_packet = 1;
            spectral->spectral_sent_msg = 0;
            spectral->num_spectral_data=0;
            spectral->scan_start_tstamp = ath_hal_gettsf64(sc->sc_ah);
            SPECTRAL_DPRINTK(sc, DEBUG_SPECTRAL2,"Start scan at %llu\n", (long long unsigned int) spectral->scan_start_tstamp);
            ath_hal_start_spectral_scan(sc->sc_ah);
            ATH_PS_SLEEP(sc);
            OS_SET_TIMER(&spectral->debug_timer, DEBUG_TIMEOUT_MS);
        }
        SPECTRAL_DPRINTK(sc, DEBUG_SPECTRAL2,"%s\n", "==============END SPECTRAL DEBUG TIMEOUT==============");
#undef AR_OBS_BUS_1
}
#endif

OS_TIMER_FUNC(spectral_classify_scan)
{
        struct ath_softc *sc;
        struct ath_spectral *spectral;
        static int prev_recv=0;

        OS_GET_TIMER_ARG(sc, struct ath_softc *);
        spectral=sc->sc_spectral;

        OS_CANCEL_TIMER(&spectral->classify_timer);
        SPECTRAL_DPRINTK(sc, DEBUG_SPECTRAL2,"%s\n", "SPECTRAL CLASSIFY TIMEOUT");

        if(prev_recv == spectral->num_spectral_data) {
            SPECTRAL_DPRINTK(sc, DEBUG_SPECTRAL2,"this_scan_recv=%d prev_recv=%d\n", spectral->num_spectral_data, prev_recv);
            send_fake_ht40_data(sc);
        } else {
            print_detection(sc);
            SPECTRAL_DPRINTK(sc, DEBUG_SPECTRAL2,"this_scan_recv=%d total_recv=%d\n", spectral->num_spectral_data, spectral->total_spectral_data);
        }
        /* Now that the timer has gone off, make sure we send the next 
           bit of data*/
        prev_recv = spectral->num_spectral_data;
        spectral->send_single_packet = 1;
        spectral->spectral_sent_msg = 0;
        OS_SET_TIMER(&spectral->classify_timer, CLASSIFY_TIMEOUT_MS);

}

#if 0
void print_compare_fft_data(struct ath_softc *sc, u_int8_t *fft_data_ptr1, u_int8_t *fft_data_ptr2, int datalen)
{
    int i;

    for (i=0; i<datalen; i++) {
        SPECTRAL_DPRINTK(sc, DEBUG_SPECTRAL1, "fft_data1[%d]=0x%x fft_data2[%d]=0x%x\n",
                                    i, fft_data_ptr1[i],
                                    i, fft_data_ptr2[i]);
    }
}

void print_fft_data(struct ath_softc *sc, u_int8_t *fft_data_ptr, int datalen)
{
    int i;

    for (i=0; i<datalen; i++) {
        SPECTRAL_DPRINTK(sc, DEBUG_SPECTRAL1, "fft_data[%d]=0x%x\n", i, fft_data_ptr[i]);
    }
}

u_int8_t *ret_byte_copied_fft_data(struct ath_softc *sc, struct ath_desc *ds, struct ath_buf *bf)
{
#define ALLOC_FFT_DATA_SIZE 256

        struct ath_spectral *spectral=sc->sc_spectral;
        u_int16_t datalen = ds->ds_rxstat.rs_datalen;
        u_int8_t  *pfft_data=NULL, *byte_ptr=NULL, bmapwt=0,maxindex=0;
        int i=0;

    pfft_data = (u_int8_t*)OS_MALLOC(sc->sc_osdev, ALLOC_FFT_DATA_SIZE, 0); 

        if ((!pfft_data) || (datalen > spectral->spectral_data_len + 2))
            return NULL;
        
        OS_MEMZERO(pfft_data, ALLOC_FFT_DATA_SIZE);


        for (i=0; i<datalen; i++) {
            byte_ptr = (u_int8_t*)(((u_int8_t*)bf->bf_vdata + i));
            pfft_data[i]=((*byte_ptr) & 0xFF);
        }
        get_ht20_bmap_maxindex(sc,pfft_data,datalen,&bmapwt, &maxindex);

        SPECTRAL_DPRINTK(sc, DEBUG_SPECTRAL1, "%s %d HT20 bmap=%d maxindex=%d\n", __func__, __LINE__, bmapwt, maxindex);
        return pfft_data;
#undef ALLOC_FFT_DATA_SIZE
}

#endif
#endif //WLAN_SPECTRAL_ENABLE

