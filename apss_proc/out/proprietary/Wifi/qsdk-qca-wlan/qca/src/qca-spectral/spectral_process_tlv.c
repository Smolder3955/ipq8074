/*
 * Copyright (c) 2002-2009 Atheros Communications, Inc.
 * All Rights Reserved.
 *
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 */

#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <netdb.h>
#include <errno.h>
#include <errno.h>
#include <signal.h>
#include <sys/ioctl.h>
#include "logspectral.h"
#include "if_athioctl.h"
#include "ah.h"
#include "ah_internal.h"

#define TLV_TAG_SPECTRAL_SUMMARY_REPORT     0xF9
#define TLV_TAG_ADC_REPORT                  0xFA
#define TLV_TAG_SEARCH_FFT_REPORT           0xFB

#ifdef WIN32
/* Not really necessary to define this,
   but we put it in just to ease the 
   task of porting to Win32, if ever required */
#pragma pack(push, spectral, 1)
#define __ATTRIB_PACK
#else
#ifndef __ATTRIB_PACK
#define __ATTRIB_PACK __attribute__ ((packed))
#endif
#endif

#ifdef BIG_ENDIAN_HOST
typedef  struct spectral_phyerr_tlv {
    u_int8_t signature;
    u_int8_t tag;
    u_int16_t length;
} __ATTRIB_PACK SPECTRAL_PHYERR_TLV;
#else
typedef  struct spectral_phyerr_tlv {
    u_int16_t length;
    u_int8_t tag;
    u_int8_t signature;
} __ATTRIB_PACK SPECTRAL_PHYERR_TLV;
#endif /* BIG_ENDIAN_HOST */

typedef struct spectral_phyerr_hdr {
    u_int32_t hdr_a;
    u_int32_t hdr_b;
}SPECTRAL_PHYERR_HDR;

typedef struct spectral_phyerr_fft {
    u_int8_t buf[0];
}SPECTRAL_PHYERR_FFT;

/*
 * Function     : print_buf
 * Description  : Prints given buffer for given length
 * Input        : Pointer to buffer and length
 * Output       : Void
 *
 */
void print_buf(u_int8_t* pbuf, int len)
{
    int i = 0;
    for (i = 0; i < len; i++) {
        printf("%02X ", pbuf[i]);
        if (i % 32 == 31) {
            printf("\n");
        }
    }
}


/*
 * Function     : process_adc_report
 * Description  : Process ADC Reports
 * Input        : Pointer to Spectral Phyerr TLV and Length
 * Output       : Success/Failure
 *
 */
int process_adc_report(SPECTRAL_PHYERR_TLV* ptlv, int tlvlen)
{
    int i;
    uint32_t *pdata;
    uint32_t data;

    /* For simplicity, everything is defined as uint32_t (except one). Proper code will later use the right sizes. */
    uint32_t samp_fmt;
    uint32_t chn_idx;
    uint32_t recent_rfsat;
    uint32_t agc_mb_gain;
    uint32_t agc_total_gain;

    uint32_t adc_summary;

    u_int8_t* ptmp = (u_int8_t*)ptlv;

    printf("SPECTRAL : ADC REPORT\n");

    /* Relook this */ 
    if (tlvlen < 4) {
        printf("Unexpected TLV length %d for ADC Report! Hexdump follows\n", tlvlen);
        print_buf((u_int8_t*)ptlv, tlvlen + 4);
        return -1;
    }

    memcpy(&adc_summary, (u_int8_t*)(ptlv + 4), sizeof(int));

    samp_fmt= ((adc_summary >> 28) & 0x1);
    chn_idx= ((adc_summary >> 24) & 0x3);
    recent_rfsat = ((adc_summary >> 23) & 0x1);
    agc_mb_gain = ((adc_summary >> 16) & 0x7f);
    agc_total_gain = adc_summary & 0x3ff;

    printf("samp_fmt= %u, chn_idx= %u, recent_rfsat= %u, agc_mb_gain=%u agc_total_gain=%u\n", samp_fmt, chn_idx, recent_rfsat, agc_mb_gain, agc_total_gain);

    for (i=0; i<(tlvlen/4); i++) {
        pdata = (uint32_t*)(ptmp + 4 + i*4); 
        data = *pdata;

        /* Interpreting capture format 1 */
        if (1) {
            uint8_t i1;
            uint8_t q1;
            uint8_t i2;
            uint8_t q2;
            int8_t si1;
            int8_t sq1;
            int8_t si2;
            int8_t sq2;


            i1 = data & 0xff;
            q1 = (data >> 8 ) & 0xff;
            i2 = (data >> 16 ) & 0xff;
            q2 = (data >> 24 ) & 0xff;

            if (i1 > 127) {
                si1 = i1 - 256;
            } else {
                si1 = i1;
            }

            if (q1 > 127) {
                sq1 = q1 - 256;
            } else {
                sq1 = q1;
            }

            if (i2 > 127) {
                si2 = i2 - 256;
            } else {
                si2 = i2;
            }

            if (q2 > 127) {
                sq2 = q2 - 256;
            } else {
                sq2 = q2;
            }

            printf("SPECTRAL ADC : Interpreting capture format 1\n");
            printf("adc_data_format_1 # %d %d %d\n", 2*i, si1, sq1);
            printf("adc_data_format_1 # %d %d %d\n", 2*i+1, si2, sq2);
        }

        /* Interpreting capture format 0 */
        if (1) {
            uint16_t i1;
            uint16_t q1;
            int16_t si1;
            int16_t sq1;
            i1 = data & 0xffff;
            q1 = (data >> 16 ) & 0xffff;
            if (i1 > 32767) {
                si1 = i1 - 65536;
            } else {
                si1 = i1;
            }

            if (q1 > 32767) {
                sq1 = q1 - 65536;
            } else {
                sq1 = q1;
            }
            printf("SPECTRAL ADC : Interpreting capture format 0\n");
            printf("adc_data_format_2 # %d %d %d\n", i, si1, sq1);
        }
    }

    printf("\n");

    return 0;
}

/*
 * Function     : process_search_fft_report
 * Description  : Process Search FFT Report
 * Input        : Pointer to Spectral Phyerr TLV and Length
 * Output       : Success/Failure
 *
 */
int process_search_fft_report(SPECTRAL_PHYERR_TLV* ptlv, int tlvlen)
{
    int i;
    uint32_t fft_mag;

    /* For simplicity, everything is defined as uint32_t (except one). Proper code will later use the right sizes. */
    /* For easy comparision between MDK team and OS team, the MDK script variable names have been used */
    uint32_t relpwr_db;    
    uint32_t num_str_bins_ib; 
    uint32_t base_pwr; 
    uint32_t total_gain_info;

    uint32_t fft_chn_idx;  
    int16_t peak_inx;
    uint32_t avgpwr_db;
    uint32_t peak_mag; 

    uint32_t fft_summary_A;
    uint32_t fft_summary_B;
    u_int8_t *tmp = (u_int8_t*)ptlv;
    SPECTRAL_PHYERR_HDR* phdr = (SPECTRAL_PHYERR_HDR*)(tmp + sizeof(SPECTRAL_PHYERR_TLV));

    printf("SPECTRAL : SEARCH FFT REPORT\n");
   
    /* Relook this */ 
    if (tlvlen < 8) {
        printf("SPECTRAL : Unexpected TLV length %d for Spectral Summary Report! Hexdump follows\n", tlvlen);
        print_buf((u_int8_t*)ptlv, tlvlen + 4);
        return -1;
    }


    /* Doing copy as the contents may not be aligned */
    memcpy(&fft_summary_A, (u_int8_t*)phdr, sizeof(int));
    memcpy(&fft_summary_B, (u_int8_t*)((u_int8_t*)phdr + sizeof(int)), sizeof(int));

    relpwr_db= ((fft_summary_B >>26) & 0x3f);
    num_str_bins_ib=fft_summary_B & 0xff;
    base_pwr = ((fft_summary_A >> 14) & 0x1ff);
    total_gain_info = ((fft_summary_A >> 23) & 0x1ff);
   
    fft_chn_idx= ((fft_summary_A >>12) & 0x3);
    peak_inx=fft_summary_A & 0xfff;
    
    if (peak_inx > 2047) {
        peak_inx = peak_inx - 4096;
    }
    
    avgpwr_db = ((fft_summary_B >> 18) & 0xff);
    peak_mag = ((fft_summary_B >> 8) & 0x3ff);

    printf("HA = 0x%x HB = 0x%x\n", phdr->hdr_a, phdr->hdr_b);
    printf("Base Power= 0x%x, Total Gain= %d, relpwr_db=%d, num_str_bins_ib=%d fft_chn_idx=%d peak_inx=%d avgpwr_db=%d peak_mag=%d\n", base_pwr, total_gain_info, relpwr_db, num_str_bins_ib, fft_chn_idx, peak_inx, avgpwr_db, peak_mag);

    for (i = 0; i < (tlvlen-8); i++){
        fft_mag = ((u_int8_t*)ptlv)[12 + i];
        printf("%d %d, ", i, fft_mag);
    }

    printf("\n");

    return 0;
}

/*
 * Function     : process_spectral_summary_report
 * Description  : Process Spectral Summary Report
 * Input        : Pointer to Spectral Phyerr TLV and Length
 * Output       : Success/Failure
 *
 */
int process_spectral_summary_report(SPECTRAL_PHYERR_TLV* ptlv, int tlvlen)
{
    /* For simplicity, everything is defined as uint32_t (except one). Proper code will later use the right sizes. */

    /* For easy comparision between MDK team and OS team, the MDK script variable names have been used */

    uint32_t agc_mb_gain;    
    uint32_t sscan_gidx; 
    uint32_t agc_total_gain; 
    uint32_t recent_rfsat;
    uint32_t ob_flag;  
    uint32_t nb_mask;
    uint32_t peak_mag; 
    int16_t peak_inx;

    uint32_t ss_summary_A;
    uint32_t ss_summary_B;
    SPECTRAL_PHYERR_HDR* phdr = (SPECTRAL_PHYERR_HDR*)((u_int8_t*)ptlv + sizeof(SPECTRAL_PHYERR_TLV));

    printf("SPECTRAL : SPECTRAL SUMMARY REPORT\n");

    if (tlvlen != 8) {
        printf("SPECTRAL : Unexpected TLV length %d for Spectral Summary Report! Hexdump follows\n", tlvlen);
        print_buf((u_int8_t*)ptlv, tlvlen + 4);
        return -1;
    }

    /* Doing copy as the contents may not be aligned */
    memcpy(&ss_summary_A, (u_int8_t*)phdr, sizeof(int));
    memcpy(&ss_summary_B, (u_int8_t*)((u_int8_t*)phdr + sizeof(int)), sizeof(int));

    nb_mask = ((ss_summary_B >> 22) & 0xff);
    ob_flag = ((ss_summary_B >> 30) & 0x1);
    peak_inx = (ss_summary_B  & 0xfff);

    if (peak_inx > 2047) {
        peak_inx = peak_inx - 4096;
    }

    peak_mag = ((ss_summary_B >> 12) & 0x3ff);
    agc_mb_gain = ((ss_summary_A >> 24)& 0x7f);
    agc_total_gain = (ss_summary_A  & 0x3ff);
    sscan_gidx = ((ss_summary_A >> 16) & 0xff);
    recent_rfsat = ((ss_summary_B >> 31) & 0x1);  

    printf("nb_mask=0x%.2x, ob_flag=%d, peak_index=%d, peak_mag=%d, agc_mb_gain=%d, agc_total_gain=%d, sscan_gidx=%d, recent_rfsat=%d\n", nb_mask, ob_flag, peak_inx, peak_mag, agc_mb_gain, agc_total_gain, sscan_gidx, recent_rfsat);

    return 0;
}

/*
 * Function     : spectral_process_tlv
 * Description  : Process Spectral TLV
 * Input        : Pointer to Spectral Phyerr TLV
 * Output       : Success/Failure
 *
 */
int spectral_process_tlv(SPECTRAL_PHYERR_TLV* ptlv)
{
    if (ptlv->signature != 0xbb) {
        printf(" APP : Invalid signature 0x%x!\n", ptlv->signature);
        return -1;
    }

    /* TODO : Do not delete the following print
     *        The scripts used to validate Spectral depend on this Print
     */
    printf("SPECTRAL : TLV Length is 0x%x (%d)\n", ptlv->length, ptlv->length);

    switch(ptlv->tag) {
        case TLV_TAG_SPECTRAL_SUMMARY_REPORT:
        process_spectral_summary_report(ptlv, ptlv->length);
        break;

        case TLV_TAG_SEARCH_FFT_REPORT:
        process_search_fft_report(ptlv, ptlv->length);
        break;

        case TLV_TAG_ADC_REPORT:
        process_adc_report(ptlv, ptlv->length);
        break;

        default:
        printf("SPECTRAL : INVALID TLV\n");
        break;
    }
    return 0;
}

/*
 * Function     : spectral_process_header
 * Description  : Process Spectral header
 * Input        : Pointer to Spectral Phyerr Header
 * Output       : Success/Failure
 *
 */
int spectral_process_header(SPECTRAL_PHYERR_HDR* phdr)
{

    u_int32_t a;
    u_int32_t b;

    memcpy(&a, (u_int8_t*)phdr, sizeof(int));
    memcpy(&b, (u_int8_t*)((u_int8_t*)phdr + sizeof(int)), sizeof(int));

    printf("SPECTRAL : HEADER A 0x%x (%d)\n", a, a);
    printf("SPECTRAL : HEADER B 0x%x (%d)\n", b, b);
    return 0;
}

/*
 * Function     : spectral_process_fft
 * Description  : Process Spectral FFT
 * Input        : Pointer to Spectral Phyerr FFT
 * Output       : Success/Failure
 *
 */
int spectral_process_fft(SPECTRAL_PHYERR_FFT* pfft, int fftlen)
{
    int i = 0;

    /* TODO : Do not delete the following print
     *        The scripts used to validate Spectral depend on this Print
     */
    printf("SPECTRAL : FFT Length is 0x%x (%d)\n", fftlen, fftlen);

    printf("fft_data # ");
    for (i = 0; i < fftlen; i++) {
        printf("%d ", pfft->buf[i]);
#if 0
        if (i % 32 == 31)
            printf("\n");
#endif
    }
    printf("\n");
    return 0;
}


/*
 * Function     : spectral_process_phyerr_data
 * Description  : Process PHY Error
 * Input        : Pointer to buffer
 * Output       : Success/Failure
 *
 */
int spectral_process_phyerr_data(u_int8_t* data)
{
    SPECTRAL_PHYERR_TLV* ptlv = (SPECTRAL_PHYERR_TLV*)data;
    SPECTRAL_PHYERR_HDR* phdr = (SPECTRAL_PHYERR_HDR*)(data + sizeof(SPECTRAL_PHYERR_TLV));
    SPECTRAL_PHYERR_FFT* pfft = (SPECTRAL_PHYERR_FFT*)(data + sizeof(SPECTRAL_PHYERR_TLV) + sizeof(SPECTRAL_PHYERR_HDR));
    int fftlen  = ptlv->length - sizeof(SPECTRAL_PHYERR_HDR);
    u_int8_t* next_tlv;

    if (spectral_process_tlv(ptlv) == -1) {
        printf(" APP : Invalid signature 0x%x!\n", ptlv->signature);
        return -1;
    }
    next_tlv = (u_int8_t*)ptlv + ptlv->length + sizeof(int);

    /*
     * Make a more correct check. 
     * Summary Report always follows Spectral Report
     */
    if (ptlv->length > 128) {
        spectral_process_tlv((SPECTRAL_PHYERR_TLV*)next_tlv);
    }

    spectral_process_header(phdr);

#if 0
    if (fftlen) {
        spectral_process_fft(pfft, fftlen);
    }
#endif

    return 0;

}

