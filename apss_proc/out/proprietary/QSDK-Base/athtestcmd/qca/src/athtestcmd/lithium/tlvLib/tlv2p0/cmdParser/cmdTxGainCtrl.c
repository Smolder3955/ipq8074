/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

// This is an auto-generated file from input/cmdTxGainCtrl.s
#include "tlv2Inc.h"
#include "cmdTxGainCtrl.h"

void* initTXGAINCTRLOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict)
{
    int i, j; 	//for initializing array parameter
    CMD_TXGAINCTRL_PARMS  *pTXGAINCTRLParms = (CMD_TXGAINCTRL_PARMS *)pParmsCommon;

    if (pParmsCommon == NULL) return (NULL);

    i = j = 0;	//assign a number to avoid warning in case i and j are not used

    // Populate the parm structure with initial values
    pTXGAINCTRLParms->phyId = pParmDict[PARM_PHYID].v.valU8;
    pTXGAINCTRLParms->gainIdx = pParmDict[PARM_GAINIDX].v.valU8;
    pTXGAINCTRLParms->chainMask = pParmDict[PARM_CHAINMASK].v.valU16;
    pTXGAINCTRLParms->dacGain = pParmDict[PARM_DACGAIN].v.valS8;
    pTXGAINCTRLParms->forceCal = pParmDict[PARM_FORCECAL].v.valU8;

    // Make up ParmOffsetTbl
    resetParmOffsetFields();
    fillParmOffsetTbl((A_UINT32)PARM_PHYID, (A_UINT32)(((A_UINT32)&(pTXGAINCTRLParms->phyId)) - (A_UINT32)pTXGAINCTRLParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_GAINIDX, (A_UINT32)(((A_UINT32)&(pTXGAINCTRLParms->gainIdx)) - (A_UINT32)pTXGAINCTRLParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_CHAINMASK, (A_UINT32)(((A_UINT32)&(pTXGAINCTRLParms->chainMask)) - (A_UINT32)pTXGAINCTRLParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_DACGAIN, (A_UINT32)(((A_UINT32)&(pTXGAINCTRLParms->dacGain)) - (A_UINT32)pTXGAINCTRLParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_FORCECAL, (A_UINT32)(((A_UINT32)&(pTXGAINCTRLParms->forceCal)) - (A_UINT32)pTXGAINCTRLParms), pParmsOffset);
    return((void*) pTXGAINCTRLParms);
}

static TXGAINCTRL_OP_FUNC TXGAINCTRLOpFunc = NULL;

TLV2_API void registerTXGAINCTRLHandler(TXGAINCTRL_OP_FUNC fp)
{
    TXGAINCTRLOpFunc = fp;
}

A_BOOL TXGAINCTRLOp(void *pParms)
{
    CMD_TXGAINCTRL_PARMS *pTXGAINCTRLParms = (CMD_TXGAINCTRL_PARMS *)pParms;

#if 0 //for debugging, comment out this line, and uncomment the line below
//#ifdef _DEBUG
    int i; 	//for initializing array parameter
    i = 0;	//assign a number to avoid warning in case i is not used

    A_PRINTF("TXGAINCTRLOp: phyId %u\n", pTXGAINCTRLParms->phyId);
    A_PRINTF("TXGAINCTRLOp: gainIdx %u\n", pTXGAINCTRLParms->gainIdx);
    A_PRINTF("TXGAINCTRLOp: chainMask 0x%x\n", pTXGAINCTRLParms->chainMask);
    A_PRINTF("TXGAINCTRLOp: dacGain %u\n", pTXGAINCTRLParms->dacGain);
    A_PRINTF("TXGAINCTRLOp: forceCal %u\n", pTXGAINCTRLParms->forceCal);
#endif //_DEBUG

    if (NULL != TXGAINCTRLOpFunc) {
        (*TXGAINCTRLOpFunc)(pTXGAINCTRLParms);
    }
    return(TRUE);
}

void* initTXGAINCTRLRSPOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict)
{
    int i, j; 	//for initializing array parameter
    CMD_TXGAINCTRLRSP_PARMS  *pTXGAINCTRLRSPParms = (CMD_TXGAINCTRLRSP_PARMS *)pParmsCommon;

    if (pParmsCommon == NULL) return (NULL);

    i = j = 0;	//assign a number to avoid warning in case i and j are not used

    // Populate the parm structure with initial values
    pTXGAINCTRLRSPParms->phyId = pParmDict[PARM_PHYID].v.valU8;
    pTXGAINCTRLRSPParms->status = pParmDict[PARM_STATUS].v.valU8;

    // Make up ParmOffsetTbl
    resetParmOffsetFields();
    fillParmOffsetTbl((A_UINT32)PARM_PHYID, (A_UINT32)(((A_UINT32)&(pTXGAINCTRLRSPParms->phyId)) - (A_UINT32)pTXGAINCTRLRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_STATUS, (A_UINT32)(((A_UINT32)&(pTXGAINCTRLRSPParms->status)) - (A_UINT32)pTXGAINCTRLRSPParms), pParmsOffset);
    return((void*) pTXGAINCTRLRSPParms);
}

static TXGAINCTRLRSP_OP_FUNC TXGAINCTRLRSPOpFunc = NULL;

TLV2_API void registerTXGAINCTRLRSPHandler(TXGAINCTRLRSP_OP_FUNC fp)
{
    TXGAINCTRLRSPOpFunc = fp;
}

A_BOOL TXGAINCTRLRSPOp(void *pParms)
{
    CMD_TXGAINCTRLRSP_PARMS *pTXGAINCTRLRSPParms = (CMD_TXGAINCTRLRSP_PARMS *)pParms;

#if 0 //for debugging, comment out this line, and uncomment the line below
//#ifdef _DEBUG
    int i; 	//for initializing array parameter
    i = 0;	//assign a number to avoid warning in case i is not used

    A_PRINTF("TXGAINCTRLRSPOp: phyId %u\n", pTXGAINCTRLRSPParms->phyId);
    A_PRINTF("TXGAINCTRLRSPOp: status %u\n", pTXGAINCTRLRSPParms->status);
#endif //_DEBUG

    if (NULL != TXGAINCTRLRSPOpFunc) {
        (*TXGAINCTRLRSPOpFunc)(pTXGAINCTRLRSPParms);
    }
    return(TRUE);
}
