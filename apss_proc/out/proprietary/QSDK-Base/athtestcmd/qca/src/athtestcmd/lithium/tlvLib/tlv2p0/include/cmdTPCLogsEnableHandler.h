/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

// This is an auto-generated file from input\cmdTPCLogsEnableHandler.s
#ifndef _CMDTPCLOGSENABLEHANDLER_H_
#define _CMDTPCLOGSENABLEHANDLER_H_

#if defined(__cplusplus) || defined(__cplusplus__)
extern "C" {
#endif

#if defined(WIN32) || defined(WIN64)
#pragma pack (push, 1)
#endif //WIN32 || WIN64

typedef struct tpclogsenable_parms {
    A_UINT8	phyId;
    A_UINT8	enable;
    A_UINT8	pad[2];
} __ATTRIB_PACK CMD_TPCLOGSENABLE_PARMS;

typedef struct tpclogsenablersp_parms {
    A_UINT8	phyId;
    A_UINT8	status;
    A_UINT8	pad[2];
} __ATTRIB_PACK CMD_TPCLOGSENABLERSP_PARMS;

typedef void (*TPCLOGSENABLE_OP_FUNC)(void *pParms);
typedef void (*TPCLOGSENABLERSP_OP_FUNC)(void *pParms);

// Exposed functions

void* initTPCLOGSENABLEOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict);
A_BOOL TPCLOGSENABLEOp(void *pParms);

void* initTPCLOGSENABLERSPOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict);
A_BOOL TPCLOGSENABLERSPOp(void *pParms);

#if defined(WIN32) || defined(WIN64)
#pragma pack(pop)
#endif //WIN32 || WIN64


#if defined(__cplusplus) || defined(__cplusplus__)
}
#endif

#endif //_CMDTPCLOGSENABLEHANDLER_H_
