/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

// This is an auto-generated file from input/cmdAgcHistoryConfig.s
#ifndef _CMDAGCHISTORYCONFIG_H_
#define _CMDAGCHISTORYCONFIG_H_

#if defined(__cplusplus) || defined(__cplusplus__)
extern "C" {
#endif

#if defined(WIN32) || defined(WIN64)
#pragma pack (push, 1)
#endif //WIN32 || WIN64

typedef struct agchistoryconfig_parms {
    A_UINT8	phyId;
    A_UINT8	agcControl;
    A_UINT8	pad[2];
} __ATTRIB_PACK CMD_AGCHISTORYCONFIG_PARMS;

typedef struct agchistoryconfigrsp_parms {
    A_UINT8	phyId;
    A_UINT8	status;
    A_UINT8	pad[2];
} __ATTRIB_PACK CMD_AGCHISTORYCONFIGRSP_PARMS;

typedef void (*AGCHISTORYCONFIG_OP_FUNC)(void *pParms);
typedef void (*AGCHISTORYCONFIGRSP_OP_FUNC)(void *pParms);

// Exposed functions

void* initAGCHISTORYCONFIGOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict);
A_BOOL AGCHISTORYCONFIGOp(void *pParms);

void* initAGCHISTORYCONFIGRSPOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict);
A_BOOL AGCHISTORYCONFIGRSPOp(void *pParms);

#if defined(WIN32) || defined(WIN64)
#pragma pack(pop)
#endif //WIN32 || WIN64


#if defined(__cplusplus) || defined(__cplusplus__)
}
#endif

#endif //_CMDAGCHISTORYCONFIG_H_
