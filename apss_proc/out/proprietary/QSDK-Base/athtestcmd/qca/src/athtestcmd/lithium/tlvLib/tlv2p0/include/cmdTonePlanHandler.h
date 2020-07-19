/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

// This is an auto-generated file from input/cmdTonePlanHandler.s
#ifndef _CMDTONEPLANHANDLER_H_
#define _CMDTONEPLANHANDLER_H_

#if defined(__cplusplus) || defined(__cplusplus__)
extern "C" {
#endif

#if defined(WIN32) || defined(WIN64)
#pragma pack (push, 1)
#endif //WIN32 || WIN64

#ifndef TLV2_MAX_USERS
#define TLV2_MAX_USERS 9
#endif
#ifndef TLV2_MAX_20MHZ_BANDWIDTHS
#define TLV2_MAX_20MHZ_BANDWIDTHS 8
#endif
#ifndef TLV2_MAX_BANDWIDTHS_USERS
#define TLV2_MAX_BANDWIDTHS_USERS 72
#endif

typedef struct toneplan_parms {
    A_UINT8	phyId;
    A_UINT8	bandwidth;
    A_UINT8	version;
    A_UINT8	pad1;
    A_UINT32	userEntries;
    A_UINT32	dataRates[TLV2_MAX_BANDWIDTHS_USERS];
    A_UINT32	powerdBm[TLV2_MAX_BANDWIDTHS_USERS];
    A_UINT32	fec[TLV2_MAX_BANDWIDTHS_USERS];
    A_UINT32	dcm[TLV2_MAX_BANDWIDTHS_USERS];
} __ATTRIB_PACK CMD_TONEPLAN_PARMS;

typedef void (*TONEPLAN_OP_FUNC)(void *pParms);

// Exposed functions

void* initTONEPLANOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict);
A_BOOL TONEPLANOp(void *pParms);

#if defined(WIN32) || defined(WIN64)
#pragma pack(pop)
#endif //WIN32 || WIN64


#if defined(__cplusplus) || defined(__cplusplus__)
}
#endif

#endif //_CMDTONEPLANHANDLER_H_
