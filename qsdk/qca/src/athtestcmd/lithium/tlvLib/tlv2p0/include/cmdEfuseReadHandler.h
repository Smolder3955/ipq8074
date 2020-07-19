/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

// This is an auto-generated file from input\cmdEfuseReadHandler.s
#ifndef _CMDEFUSEREADHANDLER_H_
#define _CMDEFUSEREADHANDLER_H_

#if defined(__cplusplus) || defined(__cplusplus__)
extern "C" {
#endif

#if defined(WIN32) || defined(WIN64)
#pragma pack (push, 1)
#endif //WIN32 || WIN64

#ifndef CMD_EFUSE_NUMBYTES_MAX
#define CMD_EFUSE_NUMBYTES_MAX 256
#endif

typedef struct efuseread_parms {
    A_UINT32	offset;
    A_UINT16	numBytes;
    A_UINT8	pad[2];
} __ATTRIB_PACK CMD_EFUSEREAD_PARMS;

typedef struct efusereadrsp_parms {
    A_UINT16	numBytes;
    A_UINT8	efuseData[CMD_EFUSE_NUMBYTES_MAX];
    A_UINT8	status;
    A_UINT8	pad[1];
} __ATTRIB_PACK CMD_EFUSEREADRSP_PARMS;

typedef void (*EFUSEREAD_OP_FUNC)(void *pParms);
typedef void (*EFUSEREADRSP_OP_FUNC)(void *pParms);

// Exposed functions

void* initEFUSEREADOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict);
A_BOOL EFUSEREADOp(void *pParms);

void* initEFUSEREADRSPOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict);
A_BOOL EFUSEREADRSPOp(void *pParms);

#if defined(WIN32) || defined(WIN64)
#pragma pack(pop)
#endif //WIN32 || WIN64


#if defined(__cplusplus) || defined(__cplusplus__)
}
#endif

#endif //_CMDEFUSEREADHANDLER_H_
