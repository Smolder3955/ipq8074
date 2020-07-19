/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

// This is an auto-generated file from input/cmdTxGainCtrl.s
#ifndef _CMDTXGAINCTRL_H_
#define _CMDTXGAINCTRL_H_

#if defined(__cplusplus) || defined(__cplusplus__)
extern "C" {
#endif

#if defined(WIN32) || defined(WIN64)
#pragma pack (push, 1)
#endif //WIN32 || WIN64

typedef struct txgainctrl_parms {
    A_UINT8	phyId;
    A_UINT8	gainIdx;
    A_UINT16	chainMask;
    A_INT8	dacGain;
    A_UINT8	forceCal;
    A_UINT8	pad[2];
} __ATTRIB_PACK CMD_TXGAINCTRL_PARMS;

typedef struct txgainctrlrsp_parms {
    A_UINT8	phyId;
    A_UINT8	status;
    A_UINT8	pad[2];
} __ATTRIB_PACK CMD_TXGAINCTRLRSP_PARMS;

typedef void (*TXGAINCTRL_OP_FUNC)(void *pParms);
typedef void (*TXGAINCTRLRSP_OP_FUNC)(void *pParms);

// Exposed functions

void* initTXGAINCTRLOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict);
A_BOOL TXGAINCTRLOp(void *pParms);

void* initTXGAINCTRLRSPOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict);
A_BOOL TXGAINCTRLRSPOp(void *pParms);

#if defined(WIN32) || defined(WIN64)
#pragma pack(pop)
#endif //WIN32 || WIN64


#if defined(__cplusplus) || defined(__cplusplus__)
}
#endif

#endif //_CMDTXGAINCTRL_H_
