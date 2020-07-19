/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

// This is an auto-generated file from input\cmdNewXtalCalAndRespHandler.s
#include "tlv2Inc.h"
#include "cmdNewXtalCalAndRespHandler.h"

void* initNEWXTALCALPROCOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict)
{
    int i, j; 	//for initializing array parameter
    CMD_NEWXTALCALPROC_PARMS  *pNEWXTALCALPROCParms = (CMD_NEWXTALCALPROC_PARMS *)pParmsCommon;

    if (pParmsCommon == NULL) return (NULL);

    i = j = 0;	//assign a number to avoid warning in case i and j are not used

    // Populate the parm structure with initial values
    pNEWXTALCALPROCParms->newCapIn = pParmDict[PARM_NEWCAPIN].v.valU16;
    pNEWXTALCALPROCParms->newCapOut = pParmDict[PARM_NEWCAPOUT].v.valU16;
    pNEWXTALCALPROCParms->ctrlFlag = pParmDict[PARM_CTRLFLAG].v.valU8;
    pNEWXTALCALPROCParms->phyId = pParmDict[PARM_PHYID].v.valU8;
    pNEWXTALCALPROCParms->band = pParmDict[PARM_BAND].v.valU8;
    pNEWXTALCALPROCParms->pad1 = pParmDict[PARM_PAD1].v.valU8;

    // Make up ParmOffsetTbl
    resetParmOffsetFields();
    fillParmOffsetTbl((A_UINT32)PARM_NEWCAPIN, (A_UINT32)(((A_UINT32)&(pNEWXTALCALPROCParms->newCapIn)) - (A_UINT32)pNEWXTALCALPROCParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_NEWCAPOUT, (A_UINT32)(((A_UINT32)&(pNEWXTALCALPROCParms->newCapOut)) - (A_UINT32)pNEWXTALCALPROCParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_CTRLFLAG, (A_UINT32)(((A_UINT32)&(pNEWXTALCALPROCParms->ctrlFlag)) - (A_UINT32)pNEWXTALCALPROCParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_PHYID, (A_UINT32)(((A_UINT32)&(pNEWXTALCALPROCParms->phyId)) - (A_UINT32)pNEWXTALCALPROCParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_BAND, (A_UINT32)(((A_UINT32)&(pNEWXTALCALPROCParms->band)) - (A_UINT32)pNEWXTALCALPROCParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_PAD1, (A_UINT32)(((A_UINT32)&(pNEWXTALCALPROCParms->pad1)) - (A_UINT32)pNEWXTALCALPROCParms), pParmsOffset);
    return((void*) pNEWXTALCALPROCParms);
}

static NEWXTALCALPROC_OP_FUNC NEWXTALCALPROCOpFunc = NULL;

TLV2_API void registerNEWXTALCALPROCHandler(NEWXTALCALPROC_OP_FUNC fp)
{
    NEWXTALCALPROCOpFunc = fp;
}

A_BOOL NEWXTALCALPROCOp(void *pParms)
{
    CMD_NEWXTALCALPROC_PARMS *pNEWXTALCALPROCParms = (CMD_NEWXTALCALPROC_PARMS *)pParms;

#if 0 //for debugging, comment out this line, and uncomment the line below
//#ifdef _DEBUG
    int i; 	//for initializing array parameter
    i = 0;	//assign a number to avoid warning in case i is not used

    A_PRINTF("NEWXTALCALPROCOp: newCapIn %u\n", pNEWXTALCALPROCParms->newCapIn);
    A_PRINTF("NEWXTALCALPROCOp: newCapOut %u\n", pNEWXTALCALPROCParms->newCapOut);
    A_PRINTF("NEWXTALCALPROCOp: ctrlFlag %u\n", pNEWXTALCALPROCParms->ctrlFlag);
    A_PRINTF("NEWXTALCALPROCOp: phyId %u\n", pNEWXTALCALPROCParms->phyId);
    A_PRINTF("NEWXTALCALPROCOp: band %u\n", pNEWXTALCALPROCParms->band);
    A_PRINTF("NEWXTALCALPROCOp: pad1 %u\n", pNEWXTALCALPROCParms->pad1);
#endif //_DEBUG

    if (NULL != NEWXTALCALPROCOpFunc) {
        (*NEWXTALCALPROCOpFunc)(pNEWXTALCALPROCParms);
    }
    return(TRUE);
}

void* initNEWXTALCALPROCRSPOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict)
{
    int i, j; 	//for initializing array parameter
    CMD_NEWXTALCALPROCRSP_PARMS  *pNEWXTALCALPROCRSPParms = (CMD_NEWXTALCALPROCRSP_PARMS *)pParmsCommon;

    if (pParmsCommon == NULL) return (NULL);

    i = j = 0;	//assign a number to avoid warning in case i and j are not used

    // Populate the parm structure with initial values
    pNEWXTALCALPROCRSPParms->status = pParmDict[PARM_STATUS].v.valU8;
    pNEWXTALCALPROCRSPParms->pllLocked = pParmDict[PARM_PLLLOCKED].v.valU8;
    pNEWXTALCALPROCRSPParms->phyId = pParmDict[PARM_PHYID].v.valU8;
    pNEWXTALCALPROCRSPParms->band = pParmDict[PARM_BAND].v.valU8;
    pNEWXTALCALPROCRSPParms->capInValMin = pParmDict[PARM_CAPINVALMIN].v.valU16;
    pNEWXTALCALPROCRSPParms->capInValMax = pParmDict[PARM_CAPINVALMAX].v.valU16;
    pNEWXTALCALPROCRSPParms->capOutValMin = pParmDict[PARM_CAPOUTVALMIN].v.valU16;
    pNEWXTALCALPROCRSPParms->capOutValMax = pParmDict[PARM_CAPOUTVALMAX].v.valU16;
    pNEWXTALCALPROCRSPParms->newCapIn = pParmDict[PARM_NEWCAPIN].v.valU16;
    pNEWXTALCALPROCRSPParms->newCapOut = pParmDict[PARM_NEWCAPOUT].v.valU16;

    // Make up ParmOffsetTbl
    resetParmOffsetFields();
    fillParmOffsetTbl((A_UINT32)PARM_STATUS, (A_UINT32)(((A_UINT32)&(pNEWXTALCALPROCRSPParms->status)) - (A_UINT32)pNEWXTALCALPROCRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_PLLLOCKED, (A_UINT32)(((A_UINT32)&(pNEWXTALCALPROCRSPParms->pllLocked)) - (A_UINT32)pNEWXTALCALPROCRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_PHYID, (A_UINT32)(((A_UINT32)&(pNEWXTALCALPROCRSPParms->phyId)) - (A_UINT32)pNEWXTALCALPROCRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_BAND, (A_UINT32)(((A_UINT32)&(pNEWXTALCALPROCRSPParms->band)) - (A_UINT32)pNEWXTALCALPROCRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_CAPINVALMIN, (A_UINT32)(((A_UINT32)&(pNEWXTALCALPROCRSPParms->capInValMin)) - (A_UINT32)pNEWXTALCALPROCRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_CAPINVALMAX, (A_UINT32)(((A_UINT32)&(pNEWXTALCALPROCRSPParms->capInValMax)) - (A_UINT32)pNEWXTALCALPROCRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_CAPOUTVALMIN, (A_UINT32)(((A_UINT32)&(pNEWXTALCALPROCRSPParms->capOutValMin)) - (A_UINT32)pNEWXTALCALPROCRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_CAPOUTVALMAX, (A_UINT32)(((A_UINT32)&(pNEWXTALCALPROCRSPParms->capOutValMax)) - (A_UINT32)pNEWXTALCALPROCRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_NEWCAPIN, (A_UINT32)(((A_UINT32)&(pNEWXTALCALPROCRSPParms->newCapIn)) - (A_UINT32)pNEWXTALCALPROCRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_NEWCAPOUT, (A_UINT32)(((A_UINT32)&(pNEWXTALCALPROCRSPParms->newCapOut)) - (A_UINT32)pNEWXTALCALPROCRSPParms), pParmsOffset);
    return((void*) pNEWXTALCALPROCRSPParms);
}

static NEWXTALCALPROCRSP_OP_FUNC NEWXTALCALPROCRSPOpFunc = NULL;

TLV2_API void registerNEWXTALCALPROCRSPHandler(NEWXTALCALPROCRSP_OP_FUNC fp)
{
    NEWXTALCALPROCRSPOpFunc = fp;
}

A_BOOL NEWXTALCALPROCRSPOp(void *pParms)
{
    CMD_NEWXTALCALPROCRSP_PARMS *pNEWXTALCALPROCRSPParms = (CMD_NEWXTALCALPROCRSP_PARMS *)pParms;

#if 0 //for debugging, comment out this line, and uncomment the line below
//#ifdef _DEBUG
    int i; 	//for initializing array parameter
    i = 0;	//assign a number to avoid warning in case i is not used

    A_PRINTF("NEWXTALCALPROCRSPOp: status %u\n", pNEWXTALCALPROCRSPParms->status);
    A_PRINTF("NEWXTALCALPROCRSPOp: pllLocked %u\n", pNEWXTALCALPROCRSPParms->pllLocked);
    A_PRINTF("NEWXTALCALPROCRSPOp: phyId %u\n", pNEWXTALCALPROCRSPParms->phyId);
    A_PRINTF("NEWXTALCALPROCRSPOp: band %u\n", pNEWXTALCALPROCRSPParms->band);
    A_PRINTF("NEWXTALCALPROCRSPOp: capInValMin %u\n", pNEWXTALCALPROCRSPParms->capInValMin);
    A_PRINTF("NEWXTALCALPROCRSPOp: capInValMax %u\n", pNEWXTALCALPROCRSPParms->capInValMax);
    A_PRINTF("NEWXTALCALPROCRSPOp: capOutValMin %u\n", pNEWXTALCALPROCRSPParms->capOutValMin);
    A_PRINTF("NEWXTALCALPROCRSPOp: capOutValMax %u\n", pNEWXTALCALPROCRSPParms->capOutValMax);
    A_PRINTF("NEWXTALCALPROCRSPOp: newCapIn %u\n", pNEWXTALCALPROCRSPParms->newCapIn);
    A_PRINTF("NEWXTALCALPROCRSPOp: newCapOut %u\n", pNEWXTALCALPROCRSPParms->newCapOut);
#endif //_DEBUG

    if (NULL != NEWXTALCALPROCRSPOpFunc) {
        (*NEWXTALCALPROCRSPOpFunc)(pNEWXTALCALPROCRSPParms);
    }
    return(TRUE);
}
