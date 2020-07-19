/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

// This is an auto-generated file from input\cmdEfuseWriteHandler.s
#ifndef _CMDEFUSEWRITEHANDLER_H_
#define _CMDEFUSEWRITEHANDLER_H_

#if defined(__cplusplus) || defined(__cplusplus__)
extern "C" {
#endif

#if defined(WIN32) || defined(WIN64)
#pragma pack (push, 1)
#endif //WIN32 || WIN64

#ifndef CMD_EFUSE_NUMBYTES_MAX
#define CMD_EFUSE_NUMBYTES_MAX 256
#endif

typedef struct efusewrite_parms {
    A_UINT32	offset;
    A_UINT16	numBytes;
    A_UINT8	efuseData[CMD_EFUSE_NUMBYTES_MAX];
    A_UINT8	pad[2];
} __ATTRIB_PACK CMD_EFUSEWRITE_PARMS;

typedef struct efusewritersp_parms {
    A_UINT8	status;
    A_UINT8	pad[3];
} __ATTRIB_PACK CMD_EFUSEWRITERSP_PARMS;

typedef void (*EFUSEWRITE_OP_FUNC)(void *pParms);
typedef void (*EFUSEWRITERSP_OP_FUNC)(void *pParms);

// Exposed functions

void* initEFUSEWRITEOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict);
A_BOOL EFUSEWRITEOp(void *pParms);

void* initEFUSEWRITERSPOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict);
A_BOOL EFUSEWRITERSPOp(void *pParms);

#if defined(WIN32) || defined(WIN64)
#pragma pack(pop)
#endif //WIN32 || WIN64


#if defined(__cplusplus) || defined(__cplusplus__)
}
#endif

#endif //_CMDEFUSEWRITEHANDLER_H_
