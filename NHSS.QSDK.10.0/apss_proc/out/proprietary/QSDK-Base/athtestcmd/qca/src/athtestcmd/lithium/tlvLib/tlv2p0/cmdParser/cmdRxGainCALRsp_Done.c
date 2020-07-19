/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

// This is an auto-generated file from input\cmdRxGainCALRsp_Done.s
#include "tlv2Inc.h"
#include "cmdRxGainCALRsp_Done.h"

void* initRXGAINCAL_SIGL_DONEOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict)
{
    int i, j; 	//for initializing array parameter
    CMD_RXGAINCAL_SIGL_DONE_PARMS  *pRXGAINCAL_SIGL_DONEParms = (CMD_RXGAINCAL_SIGL_DONE_PARMS *)pParmsCommon;

    if (pParmsCommon == NULL) return (NULL);

    i = j = 0;	//assign a number to avoid warning in case i and j are not used

    // Populate the parm structure with initial values
    pRXGAINCAL_SIGL_DONEParms->phyId = pParmDict[PARM_PHYID].v.valU8;
    pRXGAINCAL_SIGL_DONEParms->status = pParmDict[PARM_STATUS].v.valU8;
    pRXGAINCAL_SIGL_DONEParms->band = pParmDict[PARM_BAND].v.valU8;
    pRXGAINCAL_SIGL_DONEParms->pad1 = pParmDict[PARM_PAD1].v.valU8;

    // Make up ParmOffsetTbl
    resetParmOffsetFields();
    fillParmOffsetTbl((A_UINT32)PARM_PHYID, (A_UINT32)(((A_UINT32)&(pRXGAINCAL_SIGL_DONEParms->phyId)) - (A_UINT32)pRXGAINCAL_SIGL_DONEParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_STATUS, (A_UINT32)(((A_UINT32)&(pRXGAINCAL_SIGL_DONEParms->status)) - (A_UINT32)pRXGAINCAL_SIGL_DONEParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_BAND, (A_UINT32)(((A_UINT32)&(pRXGAINCAL_SIGL_DONEParms->band)) - (A_UINT32)pRXGAINCAL_SIGL_DONEParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_PAD1, (A_UINT32)(((A_UINT32)&(pRXGAINCAL_SIGL_DONEParms->pad1)) - (A_UINT32)pRXGAINCAL_SIGL_DONEParms), pParmsOffset);
    return((void*) pRXGAINCAL_SIGL_DONEParms);
}

static RXGAINCAL_SIGL_DONE_OP_FUNC RXGAINCAL_SIGL_DONEOpFunc = NULL;

TLV2_API void registerRXGAINCAL_SIGL_DONEHandler(RXGAINCAL_SIGL_DONE_OP_FUNC fp)
{
    RXGAINCAL_SIGL_DONEOpFunc = fp;
}

A_BOOL RXGAINCAL_SIGL_DONEOp(void *pParms)
{
    CMD_RXGAINCAL_SIGL_DONE_PARMS *pRXGAINCAL_SIGL_DONEParms = (CMD_RXGAINCAL_SIGL_DONE_PARMS *)pParms;

#if 0 //for debugging, comment out this line, and uncomment the line below
//#ifdef _DEBUG
    int i; 	//for initializing array parameter
    i = 0;	//assign a number to avoid warning in case i is not used

    A_PRINTF("RXGAINCAL_SIGL_DONEOp: phyId %u\n", pRXGAINCAL_SIGL_DONEParms->phyId);
    A_PRINTF("RXGAINCAL_SIGL_DONEOp: status %u\n", pRXGAINCAL_SIGL_DONEParms->status);
    A_PRINTF("RXGAINCAL_SIGL_DONEOp: band %u\n", pRXGAINCAL_SIGL_DONEParms->band);
    A_PRINTF("RXGAINCAL_SIGL_DONEOp: pad1 %u\n", pRXGAINCAL_SIGL_DONEParms->pad1);
#endif //_DEBUG

    if (NULL != RXGAINCAL_SIGL_DONEOpFunc) {
        (*RXGAINCAL_SIGL_DONEOpFunc)(pRXGAINCAL_SIGL_DONEParms);
    }
    return(TRUE);
}

void* initRXGAINCALRSP_DONEOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict)
{
    int i, j; 	//for initializing array parameter
    CMD_RXGAINCALRSP_DONE_PARMS  *pRXGAINCALRSP_DONEParms = (CMD_RXGAINCALRSP_DONE_PARMS *)pParmsCommon;

    if (pParmsCommon == NULL) return (NULL);

    i = j = 0;	//assign a number to avoid warning in case i and j are not used

    // Populate the parm structure with initial values
    pRXGAINCALRSP_DONEParms->phyId = pParmDict[PARM_PHYID].v.valU8;
    pRXGAINCALRSP_DONEParms->status = pParmDict[PARM_STATUS].v.valU8;
    pRXGAINCALRSP_DONEParms->band = pParmDict[PARM_BAND].v.valU8;
    pRXGAINCALRSP_DONEParms->pad1 = pParmDict[PARM_PAD1].v.valU8;

    // Make up ParmOffsetTbl
    resetParmOffsetFields();
    fillParmOffsetTbl((A_UINT32)PARM_PHYID, (A_UINT32)(((A_UINT32)&(pRXGAINCALRSP_DONEParms->phyId)) - (A_UINT32)pRXGAINCALRSP_DONEParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_STATUS, (A_UINT32)(((A_UINT32)&(pRXGAINCALRSP_DONEParms->status)) - (A_UINT32)pRXGAINCALRSP_DONEParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_BAND, (A_UINT32)(((A_UINT32)&(pRXGAINCALRSP_DONEParms->band)) - (A_UINT32)pRXGAINCALRSP_DONEParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_PAD1, (A_UINT32)(((A_UINT32)&(pRXGAINCALRSP_DONEParms->pad1)) - (A_UINT32)pRXGAINCALRSP_DONEParms), pParmsOffset);
    return((void*) pRXGAINCALRSP_DONEParms);
}

static RXGAINCALRSP_DONE_OP_FUNC RXGAINCALRSP_DONEOpFunc = NULL;

TLV2_API void registerRXGAINCALRSP_DONEHandler(RXGAINCALRSP_DONE_OP_FUNC fp)
{
    RXGAINCALRSP_DONEOpFunc = fp;
}

A_BOOL RXGAINCALRSP_DONEOp(void *pParms)
{
    CMD_RXGAINCALRSP_DONE_PARMS *pRXGAINCALRSP_DONEParms = (CMD_RXGAINCALRSP_DONE_PARMS *)pParms;

#if 0 //for debugging, comment out this line, and uncomment the line below
//#ifdef _DEBUG
    int i; 	//for initializing array parameter
    i = 0;	//assign a number to avoid warning in case i is not used

    A_PRINTF("RXGAINCALRSP_DONEOp: phyId %u\n", pRXGAINCALRSP_DONEParms->phyId);
    A_PRINTF("RXGAINCALRSP_DONEOp: status %u\n", pRXGAINCALRSP_DONEParms->status);
    A_PRINTF("RXGAINCALRSP_DONEOp: band %u\n", pRXGAINCALRSP_DONEParms->band);
    A_PRINTF("RXGAINCALRSP_DONEOp: pad1 %u\n", pRXGAINCALRSP_DONEParms->pad1);
#endif //_DEBUG

    if (NULL != RXGAINCALRSP_DONEOpFunc) {
        (*RXGAINCALRSP_DONEOpFunc)(pRXGAINCALRSP_DONEParms);
    }
    return(TRUE);
}
