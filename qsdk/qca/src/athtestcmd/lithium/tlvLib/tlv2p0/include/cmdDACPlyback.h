/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

// This is an auto-generated file from input/cmdDACPlyback.s
#ifndef _CMDDACPLYBACK_H_
#define _CMDDACPLYBACK_H_

#if defined(__cplusplus) || defined(__cplusplus__)
extern "C" {
#endif

#if defined(WIN32) || defined(WIN64)
#pragma pack (push, 1)
#endif //WIN32 || WIN64

typedef struct dacplybck_parms {
    A_UINT8	phyId;
    A_UINT8	DACPlybckMode;
    A_UINT8	DACPlybckOptions;
    A_UINT8	DACPlybckRadarOnLen;
    A_UINT8	DACPlybckRadarOffLen;
    A_UINT8	pad3[3];
    A_UINT16	chainMask;
    A_UINT16	DACPlybckCount;
    A_UINT8	data4k[4096];
} __ATTRIB_PACK CMD_DACPLYBCK_PARMS;

typedef struct dacplybckrsp_parms {
    A_UINT8	phyId;
    A_UINT8	status;
    A_UINT8	pad[2];
} __ATTRIB_PACK CMD_DACPLYBCKRSP_PARMS;

typedef void (*DACPLYBCK_OP_FUNC)(void *pParms);
typedef void (*DACPLYBCKRSP_OP_FUNC)(void *pParms);

// Exposed functions

void* initDACPLYBCKOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict);
A_BOOL DACPLYBCKOp(void *pParms);

void* initDACPLYBCKRSPOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict);
A_BOOL DACPLYBCKRSPOp(void *pParms);

#if defined(WIN32) || defined(WIN64)
#pragma pack(pop)
#endif //WIN32 || WIN64


#if defined(__cplusplus) || defined(__cplusplus__)
}
#endif

#endif //_CMDDACPLYBACK_H_
