/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

// This is an auto-generated file from input\cmdNFCalGroupHandler.s
#ifndef _CMDNFCALGROUPHANDLER_H_
#define _CMDNFCALGROUPHANDLER_H_

#if defined(__cplusplus) || defined(__cplusplus__)
extern "C" {
#endif

#if defined(WIN32) || defined(WIN64)
#pragma pack (push, 1)
#endif //WIN32 || WIN64

#ifndef TLV2_MAX_CHAIN
#define TLV2_MAX_CHAIN 16
#endif

typedef struct nfcalgroup_parms {
    A_UINT8	cmdIdNFCalGroup;
    A_UINT8	phyId;
    A_UINT8	bandCode;
    A_UINT8	bwCode;
    A_UINT16	freq;
    A_UINT16	txChMask;
    A_UINT16	rxChMask;
    A_UINT8	concMode;
    A_UINT8	pad1;
    A_UINT32	userParm1;
    A_UINT32	userParm2;
    A_UINT32	nfCalResults[TLV2_MAX_CHAIN];
} __ATTRIB_PACK CMD_NFCALGROUP_PARMS;

typedef struct nfcalgrouprsp_parms {
    A_UINT8	phyId;
    A_UINT8	status;
    A_UINT8	miscFlags;
    A_UINT8	pad1;
    A_UINT32	nfCalResults[TLV2_MAX_CHAIN];
    A_UINT32	executionTime;
} __ATTRIB_PACK CMD_NFCALGROUPRSP_PARMS;

typedef void (*NFCALGROUP_OP_FUNC)(void *pParms);
typedef void (*NFCALGROUPRSP_OP_FUNC)(void *pParms);

// Exposed functions

void* initNFCALGROUPOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict);
A_BOOL NFCALGROUPOp(void *pParms);

void* initNFCALGROUPRSPOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict);
A_BOOL NFCALGROUPRSPOp(void *pParms);

#if defined(WIN32) || defined(WIN64)
#pragma pack(pop)
#endif //WIN32 || WIN64


#if defined(__cplusplus) || defined(__cplusplus__)
}
#endif

#endif //_CMDNFCALGROUPHANDLER_H_
