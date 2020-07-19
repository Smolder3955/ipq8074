/*
 * Copyright (c) 2011, 2017 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary . Qualcomm Innovation Center, Inc.
 *
 * Copyright (c) 2010 Atheros Communications, Inc. 
 * All Rights Reserved.
 * 
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */

#ifndef _SPECTRAL_MSG_PROTO_H_
#define _SPECTRAL_MSG_PROTO_H_

#ifdef WIN32
#pragma pack(push, spec_msg_proto, 1)
#define __ATTRIB_PACK
#else
#ifndef __ATTRIB_PACK
#define __ATTRIB_PACK __attribute__ ((packed))
#endif
#endif


#include <ah_osdep.h>

typedef enum {

SAMP_FAST_REQUEST=1,
SAMP_RESPONSE=2,
SAMP_CLASSIFY_REQUEST=3,
SAMP_REQUEST,
LAST_SAMP_TAG_TYPE
}eSAMP_TAG;

typedef enum {
    BW_20=0,
    BW_40
} eBW;

typedef enum {
    INTERF_NONE=0,
    INTERF_MW,
    INTERF_BT,
    INTERF_DECT,
    INTERF_TONE,
    INTERF_OTHER,
} eINTERF_TYPE;

struct TLV{
    u_int8_t tag;
    u_int16_t len;
    u_int8_t value[1];
}__ATTRIB_PACK;

struct FREQ_BW_REQ{
    u_int16_t freq;
    u_int8_t bw;
}__ATTRIB_PACK;

struct DATA_REQ_VAL{
    u_int16_t count;
    struct FREQ_BW_REQ data[1];
} __ATTRIB_PACK;

struct FREQ_PWR_RSP{
    u_int8_t pwr;
} __ATTRIB_PACK;

struct SAMPLE_RSP {
    u_int16_t freq;
    u_int16_t sample_count;
    struct FREQ_PWR_RSP samples[1];
} __ATTRIB_PACK;



#ifdef WIN32
#pragma pack(pop, spec_msg_proto)
#endif
#ifdef __ATTRIB_PACK
#undef __ATTRIB_PACK
#endif

#endif
