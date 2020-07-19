/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

// This is an auto-generated file from input/cmdRxGainCtrl.s
#ifndef _CMDRXGAINCTRL_H_
#define _CMDRXGAINCTRL_H_

#if defined(__cplusplus) || defined(__cplusplus__)
extern "C" {
#endif

#if defined(WIN32) || defined(WIN64)
#pragma pack (push, 1)
#endif //WIN32 || WIN64

typedef struct rxgainctrl_parms {
    A_UINT8	phyId;
    A_UINT8	xlnaCtrl;
    A_UINT8	gainCtrlMode;
    A_UINT8	band;
    A_UINT8	gainIdx;
    A_UINT8	pad[3];
} __ATTRIB_PACK CMD_RXGAINCTRL_PARMS;

typedef struct rxgainctrlrsp_parms {
    A_UINT8	phyId;
    A_UINT8	status;
    A_UINT8	pad[2];
} __ATTRIB_PACK CMD_RXGAINCTRLRSP_PARMS;

typedef void (*RXGAINCTRL_OP_FUNC)(void *pParms);
typedef void (*RXGAINCTRLRSP_OP_FUNC)(void *pParms);

// Exposed functions

void* initRXGAINCTRLOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict);
A_BOOL RXGAINCTRLOp(void *pParms);

void* initRXGAINCTRLRSPOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict);
A_BOOL RXGAINCTRLRSPOp(void *pParms);

#if defined(WIN32) || defined(WIN64)
#pragma pack(pop)
#endif //WIN32 || WIN64


#if defined(__cplusplus) || defined(__cplusplus__)
}
#endif

#endif //_CMDRXGAINCTRL_H_
