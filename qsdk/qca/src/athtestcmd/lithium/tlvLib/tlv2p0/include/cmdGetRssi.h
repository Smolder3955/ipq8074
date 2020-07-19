/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

// This is an auto-generated file from input/cmdGetRssi.s
#ifndef _CMDGETRSSI_H_
#define _CMDGETRSSI_H_

#if defined(__cplusplus) || defined(__cplusplus__)
extern "C" {
#endif

#if defined(WIN32) || defined(WIN64)
#pragma pack (push, 1)
#endif //WIN32 || WIN64

typedef struct getrssi_parms {
    A_UINT8	phyId;
    A_UINT8	chain;
    A_UINT8	pad[2];
} __ATTRIB_PACK CMD_GETRSSI_PARMS;

typedef struct getrssirsp_parms {
    A_UINT8	phyId;
    A_UINT8	status;
    A_INT32	rssi;
    A_UINT8	pad[2];
} __ATTRIB_PACK CMD_GETRSSIRSP_PARMS;

typedef void (*GETRSSI_OP_FUNC)(void *pParms);
typedef void (*GETRSSIRSP_OP_FUNC)(void *pParms);

// Exposed functions

void* initGETRSSIOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict);
A_BOOL GETRSSIOp(void *pParms);

void* initGETRSSIRSPOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict);
A_BOOL GETRSSIRSPOp(void *pParms);

#if defined(WIN32) || defined(WIN64)
#pragma pack(pop)
#endif //WIN32 || WIN64


#if defined(__cplusplus) || defined(__cplusplus__)
}
#endif

#endif //_CMDGETRSSI_H_
