/*
 * Copyright (c) 2010, Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */

#ifndef _SPECTRAL_RAW_ADC_IOCTL_H_
#define _SPECTRAL_RAW_ADC_IOCTL_H_

#define SPECTRAL_ADC_API_VERSION                20100315

#define SPECTRAL_ADC_IOCTL_BEGIN                100
#define SPECTRAL_ADC_ENABLE_TEST_ADDAC_MODE     100
#define SPECTRAL_ADC_DISABLE_TEST_ADDAC_MODE    101
#define SPECTRAL_ADC_BEGIN_CAPTURE              102
#define SPECTRAL_ADC_RETRIEVE_DATA              103
#define SPECTRAL_ADC_GET_CHAINS                 104
#define SPECTRAL_ADC_IOCTL_END                  SPECTRAL_ADC_GET_CHAINS

#define SPECTRAL_RAW_ADC_MAX_CHAINS             3       	// max chains (2 for Merlin, 3 for Osprey)
#define SPECTRAL_RAW_ADC_SAMPLES_PER_CALL       1024    	// number of samples returned per raw ADC capture call
#define SPECTRAL_RAW_ADC_SAMPLES_FREQUENCY_MHZ  44   		// always 44Mhz

#define AR5416_SPECTRAL_RAW_ADC_SAMPLE_MIN		-256
#define AR5416_SPECTRAL_RAW_ADC_SAMPLE_MAX		255
#define AR9300_SPECTRAL_RAW_ADC_SAMPLE_MIN		-512
#define AR9300_SPECTRAL_RAW_ADC_SAMPLE_MAX		511

typedef struct {
    u_int16_t           num_chains;     /* number of chains (output only, ignored on input) */
    u_int16_t           chain_mask;     /* chain mask (lsb=chain0) - 1 or 2 for Merlin, 1,2,4 or 7 for Osprey */
} SPECTRAL_CHAIN_INFO;

#define SPECTRAL_ADC_CAPTURE_FLAG_UNUSED		        0x1     /* this flag is no longer used */
#define SPECTRAL_ADC_CAPTURE_FLAG_AGC_AUTO		        0x2     /* set AGC gain to auto */
#define SPECTRAL_ADC_CAPTURE_FLAG_DISABLE_DC_FILTER		0x4     /* disable DC filter */
typedef struct {
    u_int32_t           version;      /* capture API version - must match SPECTRAL_ADC_API_VERSION */
    u_int16_t           freq;         /* setting in Mhz */
    u_int16_t           ieee;         /* IEEE channel number (output only, ignored on input) */
    u_int32_t           chan_flags;   /* channel flags (output only, ignored on input) */
	u_int32_t			capture_flags;/* see SPECTRAL_ADC_CAPTURE_FLAG_xxxx */
    SPECTRAL_CHAIN_INFO chain_info;   /* desired chains to sample */
} SPECTRAL_ADC_CAPTURE_PARAMS;

typedef struct {
    int16_t sampleI;    /* data is signed, 10 bit for Osprey, 9 bit for Merlin */
    int16_t sampleQ;    /* data is signed, 10 bit for Osprey, 9 bit for Merlin */
} SPECTRAL_ADC_SAMPLE;

typedef struct {
    SPECTRAL_ADC_CAPTURE_PARAMS cap;        	                        /* info on capture inc valid chains in the data */
    u_int32_t                   duration;   	                        /* total duration of samples in nanoseconds */
	int16_t 					min_sample_val;                         /* min sample value */
	int16_t 					max_sample_val;                         /* max sample value */
    int32_t                     ref_power[SPECTRAL_RAW_ADC_MAX_CHAINS]; /* reference power in dBm */
    u_int32_t                   sample_len; 	                        /* = number samples * num_chains */
    SPECTRAL_ADC_SAMPLE         data[0];    	                        /* len is sample_len */
} SPECTRAL_ADC_DATA;

#endif // _SPECTRAL_RAW_ADC_IOCTL_H_
