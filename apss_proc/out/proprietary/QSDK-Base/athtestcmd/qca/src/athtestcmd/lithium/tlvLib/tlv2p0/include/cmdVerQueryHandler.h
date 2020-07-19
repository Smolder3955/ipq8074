/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

// This is an auto-generated file from input\cmdVerQueryHandler.s
#ifndef _CMDVERQUERYHANDLER_H_
#define _CMDVERQUERYHANDLER_H_

#if defined(__cplusplus) || defined(__cplusplus__)
extern "C" {
#endif

#if defined(WIN32) || defined(WIN64)
#pragma pack (push, 1)
#endif //WIN32 || WIN64

#define MAX_VER_INFO_SIZE          1024
typedef struct verqueryrsp_parms {
    A_UINT8	ver_info[MAX_VER_INFO_SIZE];
} __ATTRIB_PACK CMD_VERQUERYRSP_PARMS;

typedef void (*VERQUERY_OP_FUNC)(void *pParms);
typedef void (*VERQUERYRSP_OP_FUNC)(void *pParms);

// Exposed functions

void* initVERQUERYOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict);
A_BOOL VERQUERYOp(void *pParms);

void* initVERQUERYRSPOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict);
A_BOOL VERQUERYRSPOp(void *pParms);

#if defined(WIN32) || defined(WIN64)
#pragma pack(pop)
#endif //WIN32 || WIN64


#if defined(__cplusplus) || defined(__cplusplus__)
}
#endif

#endif //_CMDVERQUERYHANDLER_H_
