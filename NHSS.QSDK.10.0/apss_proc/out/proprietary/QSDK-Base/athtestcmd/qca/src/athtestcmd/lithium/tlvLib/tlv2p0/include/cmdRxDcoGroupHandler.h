/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

// This is an auto-generated file from input\cmdRxDcoGroupHandler.s
#ifndef _CMDRXDCOGROUPHANDLER_H_
#define _CMDRXDCOGROUPHANDLER_H_

#if defined(__cplusplus) || defined(__cplusplus__)
extern "C" {
#endif

#if defined(WIN32) || defined(WIN64)
#pragma pack (push, 1)
#endif //WIN32 || WIN64

#ifndef TLV2_MAX_CHAIN
#define TLV2_MAX_CHAIN 16
#endif

typedef struct rxdcogroup_parms {
    A_UINT8	cmdIdRxDcoGroup;
    A_UINT8	phyId;
    A_UINT8	bandCode;
    A_UINT8	bwCode;
    A_UINT16	chainMask;
    A_UINT16	rxdco_cal_mode;
    A_UINT8	homeCh;
    A_UINT8	initLUT;
    A_UINT8	verbose;
    A_UINT8	pad1;
    A_UINT16	bbOvrdI;
    A_UINT16	bbOvrdQ;
    A_UINT16	rfOvrdI;
    A_UINT16	rfOvrdQ;
    A_UINT32	userParm1;
    A_UINT32	userParm2;
    A_UINT32	userParm3;
    A_UINT32	userParm4;
    A_UINT8	bbDcocRange0I;
    A_UINT8	bbDcocRange0Q;
    A_UINT8	rfDcocRange0I;
    A_UINT8	rfDcocRange0Q;
    A_UINT8	corrRF_DCO_LUT_i[256];
    A_UINT8	corrRF_DCO_LUT_q[256];
    A_UINT8	corrBB_DCO_LUT_i[256];
    A_UINT8	corrBB_DCO_LUT_q[256];
} __ATTRIB_PACK CMD_RXDCOGROUP_PARMS;

typedef struct rxdcogrouprsp_parms {
    A_UINT8	phyId;
    A_UINT8	pad3[3];
    A_UINT8	calStatus[TLV2_MAX_CHAIN];
    A_UINT8	bbDcocRange0I;
    A_UINT8	bbDcocRange0Q;
    A_UINT8	rfDcocRange0I;
    A_UINT8	rfDcocRange0Q;
    A_UINT8	corrRF_DCO_LUT_i[256];
    A_UINT8	corrRF_DCO_LUT_q[256];
    A_UINT8	corrBB_DCO_LUT_i[256];
    A_UINT8	corrBB_DCO_LUT_q[256];
} __ATTRIB_PACK CMD_RXDCOGROUPRSP_PARMS;

typedef void (*RXDCOGROUP_OP_FUNC)(void *pParms);
typedef void (*RXDCOGROUPRSP_OP_FUNC)(void *pParms);

// Exposed functions

void* initRXDCOGROUPOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict);
A_BOOL RXDCOGROUPOp(void *pParms);

void* initRXDCOGROUPRSPOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict);
A_BOOL RXDCOGROUPRSPOp(void *pParms);

#if defined(WIN32) || defined(WIN64)
#pragma pack(pop)
#endif //WIN32 || WIN64


#if defined(__cplusplus) || defined(__cplusplus__)
}
#endif

#endif //_CMDRXDCOGROUPHANDLER_H_
