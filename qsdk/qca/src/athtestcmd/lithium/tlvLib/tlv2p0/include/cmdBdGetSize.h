/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

// This is an auto-generated file from input\cmdBdGetSize.s
#ifndef _CMDBDGETSIZE_H_
#define _CMDBDGETSIZE_H_

#if defined(__cplusplus) || defined(__cplusplus__)
extern "C" {
#endif

#if defined(WIN32) || defined(WIN64)
#pragma pack (push, 1)
#endif //WIN32 || WIN64

typedef struct bdgetsizersp_parms {
    A_UINT8	status;
    A_UINT8	pad3[3];
    A_UINT32	bdSize;
} __ATTRIB_PACK CMD_BDGETSIZERSP_PARMS;

typedef void (*BDGETSIZE_OP_FUNC)(void *pParms);
typedef void (*BDGETSIZERSP_OP_FUNC)(void *pParms);

// Exposed functions

void* initBDGETSIZEOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict);
A_BOOL BDGETSIZEOp(void *pParms);

void* initBDGETSIZERSPOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict);
A_BOOL BDGETSIZERSPOp(void *pParms);

#if defined(WIN32) || defined(WIN64)
#pragma pack(pop)
#endif //WIN32 || WIN64


#if defined(__cplusplus) || defined(__cplusplus__)
}
#endif

#endif //_CMDBDGETSIZE_H_
