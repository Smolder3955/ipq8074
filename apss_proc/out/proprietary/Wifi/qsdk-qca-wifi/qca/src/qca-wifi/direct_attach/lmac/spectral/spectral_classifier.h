/*
 * Copyright (c) 2010, Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */

#ifndef _SPECTRAL_CLASSIFIER_H_
#define _SPECTRAL_CLASSIFIER_H_

#ifdef WIN32
#pragma pack(push, spectral_classifier, 1)
#define __ATTRIB_PACK
#else
#ifndef __ATTRIB_PACK
#define __ATTRIB_PACK __attribute__ ((packed))
#endif
#endif

typedef struct spectral_burst {

    u_int8_t prev_narrowb;
    u_int8_t prev_wideb;
    u_int8_t max_bin;
    u_int8_t min_bin;
    u_int16_t burst_width;
    u_int16_t burst_rssi;
    u_int64_t burst_start;
    u_int64_t burst_stop;
    u_int16_t burst_wideb_width;
    u_int16_t burst_wideb_rssi;

} __ATTRIB_PACK SPECTRAL_BURST;

#ifdef WIN32
#pragma pack(pop, spectral_classifier)
#endif
#ifdef __ATTRIB_PACK
#undef __ATTRIB_PACK
#endif

#endif
