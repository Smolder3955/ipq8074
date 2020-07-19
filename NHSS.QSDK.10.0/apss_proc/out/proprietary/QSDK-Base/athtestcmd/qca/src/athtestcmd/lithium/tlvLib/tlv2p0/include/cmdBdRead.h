/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

// This is an auto-generated file from input\cmdBdRead.s
#ifndef _CMDBDREAD_H_
#define _CMDBDREAD_H_

#if defined(__cplusplus) || defined(__cplusplus__)
extern "C" {
#endif

#if defined(WIN32) || defined(WIN64)
#pragma pack (push, 1)
#endif //WIN32 || WIN64

typedef struct bdread_parms {
    A_UINT32	bdSize;
    A_UINT32	offset;
    A_UINT32	size;
} __ATTRIB_PACK CMD_BDREAD_PARMS;

typedef struct bdreadrsp_parms {
    A_UINT8	status;
    A_UINT8	pad3[3];
    A_UINT32	offset;
    A_UINT32	size;
    A_UINT8	data4k[4096];
} __ATTRIB_PACK CMD_BDREADRSP_PARMS;

typedef void (*BDREAD_OP_FUNC)(void *pParms);
typedef void (*BDREADRSP_OP_FUNC)(void *pParms);

// Exposed functions

void* initBDREADOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict);
A_BOOL BDREADOp(void *pParms);

void* initBDREADRSPOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict);
A_BOOL BDREADRSPOp(void *pParms);

#if defined(WIN32) || defined(WIN64)
#pragma pack(pop)
#endif //WIN32 || WIN64


#if defined(__cplusplus) || defined(__cplusplus__)
}
#endif

#endif //_CMDBDREAD_H_
