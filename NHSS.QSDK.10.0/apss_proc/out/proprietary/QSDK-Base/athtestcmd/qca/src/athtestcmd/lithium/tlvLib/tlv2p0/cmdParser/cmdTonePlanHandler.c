/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

// This is an auto-generated file from input/cmdTonePlanHandler.s
#include "tlv2Inc.h"
#include "cmdTonePlanHandler.h"

void* initTONEPLANOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict)
{
    int i, j; 	//for initializing array parameter
    CMD_TONEPLAN_PARMS  *pTONEPLANParms = (CMD_TONEPLAN_PARMS *)pParmsCommon;

    if (pParmsCommon == NULL) return (NULL);

    i = j = 0;	//assign a number to avoid warning in case i and j are not used

    // Populate the parm structure with initial values
    pTONEPLANParms->phyId = pParmDict[PARM_PHYID].v.valU8;
    pTONEPLANParms->bandwidth = pParmDict[PARM_BANDWIDTH].v.valU8;
    pTONEPLANParms->version = pParmDict[PARM_VERSION].v.valU8;
    pTONEPLANParms->pad1 = pParmDict[PARM_PAD1].v.valU8;
    pTONEPLANParms->userEntries = pParmDict[PARM_USERENTRIES].v.valU32;
    memset(pTONEPLANParms->dataRates, 0, sizeof(pTONEPLANParms->dataRates));
    memset(pTONEPLANParms->powerdBm, 0, sizeof(pTONEPLANParms->powerdBm));
    memset(pTONEPLANParms->fec, 0, sizeof(pTONEPLANParms->fec));
    memset(pTONEPLANParms->dcm, 0, sizeof(pTONEPLANParms->dcm));

    // Make up ParmOffsetTbl
    resetParmOffsetFields();
    fillParmOffsetTbl((A_UINT32)PARM_PHYID, (A_UINT32)(((A_UINT32)&(pTONEPLANParms->phyId)) - (A_UINT32)pTONEPLANParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_BANDWIDTH, (A_UINT32)(((A_UINT32)&(pTONEPLANParms->bandwidth)) - (A_UINT32)pTONEPLANParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_VERSION, (A_UINT32)(((A_UINT32)&(pTONEPLANParms->version)) - (A_UINT32)pTONEPLANParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_PAD1, (A_UINT32)(((A_UINT32)&(pTONEPLANParms->pad1)) - (A_UINT32)pTONEPLANParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_USERENTRIES, (A_UINT32)(((A_UINT32)&(pTONEPLANParms->userEntries)) - (A_UINT32)pTONEPLANParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_DATARATES, (A_UINT32)(((A_UINT32)&(pTONEPLANParms->dataRates)) - (A_UINT32)pTONEPLANParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_POWERDBM, (A_UINT32)(((A_UINT32)&(pTONEPLANParms->powerdBm)) - (A_UINT32)pTONEPLANParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_FEC, (A_UINT32)(((A_UINT32)&(pTONEPLANParms->fec)) - (A_UINT32)pTONEPLANParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_DCM, (A_UINT32)(((A_UINT32)&(pTONEPLANParms->dcm)) - (A_UINT32)pTONEPLANParms), pParmsOffset);
    return((void*) pTONEPLANParms);
}

static TONEPLAN_OP_FUNC TONEPLANOpFunc = NULL;

TLV2_API void registerTONEPLANHandler(TONEPLAN_OP_FUNC fp)
{
    TONEPLANOpFunc = fp;
}

A_BOOL TONEPLANOp(void *pParms)
{
    CMD_TONEPLAN_PARMS *pTONEPLANParms = (CMD_TONEPLAN_PARMS *)pParms;

#if 0 //for debugging, comment out this line, and uncomment the line below
//#ifdef _DEBUG
    int i; 	//for initializing array parameter
    i = 0;	//assign a number to avoid warning in case i is not used

    A_PRINTF("TONEPLANOp: phyId %u\n", pTONEPLANParms->phyId);
    A_PRINTF("TONEPLANOp: bandwidth %u\n", pTONEPLANParms->bandwidth);
    A_PRINTF("TONEPLANOp: version %u\n", pTONEPLANParms->version);
    A_PRINTF("TONEPLANOp: pad1 %u\n", pTONEPLANParms->pad1);
    A_PRINTF("TONEPLANOp: userEntries %u\n", pTONEPLANParms->userEntries);
    for (i = 0; i < 8 ; i++) // can be modified to print up to 72 entries
    {
        A_PRINTF("TONEPLANOp: dataRates %u\n", pTONEPLANParms->dataRates[i]);
    }
    for (i = 0; i < 8 ; i++) // can be modified to print up to 72 entries
    {
        A_PRINTF("TONEPLANOp: powerdBm %u\n", pTONEPLANParms->powerdBm[i]);
    }
    for (i = 0; i < 8 ; i++) // can be modified to print up to 72 entries
    {
        A_PRINTF("TONEPLANOp: fec %u\n", pTONEPLANParms->fec[i]);
    }
    for (i = 0; i < 8 ; i++) // can be modified to print up to 72 entries
    {
        A_PRINTF("TONEPLANOp: dcm %u\n", pTONEPLANParms->dcm[i]);
    }
#endif //_DEBUG

    if (NULL != TONEPLANOpFunc) {
        (*TONEPLANOpFunc)(pTONEPLANParms);
    }
    return(TRUE);
}
