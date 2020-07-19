/*
 * Copyright (c) 2011, 2017 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary . Qualcomm Innovation Center, Inc.
 *
 * Copyright (c) 2010, Atheros Communications Inc.
 * All Rights Reserved.
 *
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 */

#ifndef _SPECTRAL_DATA_H_
#define _SPECTRAL_DATA_H_

#include "spec_msg_proto.h"

#ifndef MAX_SPECTRAL_MSG_ELEMS
#define MAX_SPECTRAL_MSG_ELEMS 10
#endif

#define SPECTRAL_SIGNATURE  0xdeadbeef

#ifndef NETLINK_ATHEROS
#define NETLINK_ATHEROS              (NETLINK_GENERIC + 1)
#endif

#ifdef WIN32
#pragma pack(push, spectral_data, 1)
#define __ATTRIB_PACK
#else
#ifndef __ATTRIB_PACK
#define __ATTRIB_PACK __attribute__ ((packed))
#endif
#endif



/* 11ac chipsets will have a max of 520 bins in each 80 MHz segment.
 *
 * For 11ac chipsets prior to AR900B version 2.0, a max of 512 bins are delivered.
 * However, there can be additional bins reported for AR900B version 2.0 and
 * QCA9984 as described next:
 *
 * AR900B version 2.0: An additional tone is processed on the right hand side in
 * order to facilitate detection of radar pulses out to the extreme band-edge of
 * the channel frequency. Since the HW design processes four tones at a time,
 * this requires one additional Dword to be added to the search FFT report.
 *
 * QCA9984: When spectral_scan_rpt_mode=2, i.e 2-dword summary + 1x-oversampled
 * bins (in-band) per FFT, then 8 more bins (4 more on left side and 4 more on
 * right side)are added.
 */

typedef struct spectral_data {
    int16_t        spectral_data_len;
    int16_t        spectral_rssi;
    int16_t        spectral_bwinfo;
    int32_t        spectral_tstamp;
    int16_t        spectral_max_index;
    int16_t        spectral_max_mag;
} __ATTRIB_PACK SPECTRAL_DATA;

struct spectral_scan_data {
    u_int16_t chanMag[128];
    u_int8_t  chanExp;
    int16_t   primRssi;
    int16_t   extRssi;
    u_int16_t dataLen;
    u_int32_t timeStamp;
    int16_t   filtRssi;
    u_int32_t numRssiAboveThres;
    int16_t   noiseFloor;
    u_int32_t center_freq;
};

typedef struct spectral_msg {
        int16_t      num_elems;
        SPECTRAL_DATA data_elems[MAX_SPECTRAL_MSG_ELEMS];
} SPECTRAL_MSG;





/* noise power cal support */
#define NOISE_PWR_SCAN_IOCTL            200
#define NOISE_PWR_SCAN_SET_DEBUG_IOCTL  201

#define NOISE_PWR_MAX_CHAINS            3
#define MAX_NOISE_PWR_REPORTS           10000

/*
 * units are: 4 x dBm - NOISE_PWR_DATA_OFFSET (e.g. -25 = (-25/4 - 90) = -96.25 dBm)
 * range (for 6 signed bits) is (-32 to 31) + offset => -122dBm to -59dBm
 * resolution (2 bits) is 0.25dBm
 */
typedef signed char pwr_dBm;

typedef struct {
    int rptcount;                       /* count of reports in pwr array */
    pwr_dBm un_cal_nf;                  /* uncalibrated noise floor */
    pwr_dBm factory_cal_nf;             /* noise floor as calibrated at the factory for module */
    pwr_dBm median_pwr;                 /* median power (median of pwr array)  */
    pwr_dBm pwr[];                      /* power reports */
} __ATTRIB_PACK CHAIN_NOISE_PWR_INFO;

typedef struct {
    pwr_dBm cal;
    pwr_dBm pwr;
} __ATTRIB_PACK CHAIN_NOISE_PWR_CAL;

typedef struct {
    uint8_t valid_chain_mask;           /* which chain cals to override (0x1 = chain 0, etc) */
    CHAIN_NOISE_PWR_CAL chain[NOISE_PWR_MAX_CHAINS];
} __ATTRIB_PACK NOISE_PWR_CAL;

typedef struct {
    uint16_t rptcount;                  /* count of reports required */
    uint8_t ctl_chain_mask;             /* ctl chains required (0x1 = chain 0, etc) */
    uint8_t ext_chain_mask;             /* ext chains required (0x1 = chain 0, etc) */
    NOISE_PWR_CAL cal_override;         /* override cal values - valid_chain_mask should be 0 for no override */
} __ATTRIB_PACK CHAIN_NOISE_PWR_IOCTL_REQ;


#ifdef WIN32
#pragma pack(pop, spectral_data)
#endif
#ifdef __ATTRIB_PACK
#undef __ATTRIB_PACK
#endif

#endif
