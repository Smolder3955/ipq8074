/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

// This is an auto-generated file from input\cmdNFCalGroupHandler.s
#include "tlv2Inc.h"
#include "cmdNFCalGroupHandler.h"

void* initNFCALGROUPOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict)
{
    int i, j; 	//for initializing array parameter
    CMD_NFCALGROUP_PARMS  *pNFCALGROUPParms = (CMD_NFCALGROUP_PARMS *)pParmsCommon;

    if (pParmsCommon == NULL) return (NULL);

    i = j = 0;	//assign a number to avoid warning in case i and j are not used

    // Populate the parm structure with initial values
    pNFCALGROUPParms->cmdIdNFCalGroup = pParmDict[PARM_CMDIDNFCALGROUP].v.valU8;
    pNFCALGROUPParms->phyId = pParmDict[PARM_PHYID].v.valU8;
    pNFCALGROUPParms->bandCode = pParmDict[PARM_BANDCODE].v.valU8;
    pNFCALGROUPParms->bwCode = pParmDict[PARM_BWCODE].v.valU8;
    pNFCALGROUPParms->freq = pParmDict[PARM_FREQ].v.valU16;
    pNFCALGROUPParms->txChMask = pParmDict[PARM_TXCHMASK].v.valU16;
    pNFCALGROUPParms->rxChMask = pParmDict[PARM_RXCHMASK].v.valU16;
    pNFCALGROUPParms->concMode = pParmDict[PARM_CONCMODE].v.valU8;
    pNFCALGROUPParms->pad1 = pParmDict[PARM_PAD1].v.valU8;
    pNFCALGROUPParms->userParm1 = pParmDict[PARM_USERPARM1].v.valU32;
    pNFCALGROUPParms->userParm2 = pParmDict[PARM_USERPARM2].v.valU32;
    memset(pNFCALGROUPParms->nfCalResults, 0, sizeof(pNFCALGROUPParms->nfCalResults));

    // Make up ParmOffsetTbl
    resetParmOffsetFields();
    fillParmOffsetTbl((A_UINT32)PARM_CMDIDNFCALGROUP, (A_UINT32)(((A_UINT32)&(pNFCALGROUPParms->cmdIdNFCalGroup)) - (A_UINT32)pNFCALGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_PHYID, (A_UINT32)(((A_UINT32)&(pNFCALGROUPParms->phyId)) - (A_UINT32)pNFCALGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_BANDCODE, (A_UINT32)(((A_UINT32)&(pNFCALGROUPParms->bandCode)) - (A_UINT32)pNFCALGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_BWCODE, (A_UINT32)(((A_UINT32)&(pNFCALGROUPParms->bwCode)) - (A_UINT32)pNFCALGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_FREQ, (A_UINT32)(((A_UINT32)&(pNFCALGROUPParms->freq)) - (A_UINT32)pNFCALGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_TXCHMASK, (A_UINT32)(((A_UINT32)&(pNFCALGROUPParms->txChMask)) - (A_UINT32)pNFCALGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_RXCHMASK, (A_UINT32)(((A_UINT32)&(pNFCALGROUPParms->rxChMask)) - (A_UINT32)pNFCALGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_CONCMODE, (A_UINT32)(((A_UINT32)&(pNFCALGROUPParms->concMode)) - (A_UINT32)pNFCALGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_PAD1, (A_UINT32)(((A_UINT32)&(pNFCALGROUPParms->pad1)) - (A_UINT32)pNFCALGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_USERPARM1, (A_UINT32)(((A_UINT32)&(pNFCALGROUPParms->userParm1)) - (A_UINT32)pNFCALGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_USERPARM2, (A_UINT32)(((A_UINT32)&(pNFCALGROUPParms->userParm2)) - (A_UINT32)pNFCALGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_NFCALRESULTS, (A_UINT32)(((A_UINT32)&(pNFCALGROUPParms->nfCalResults)) - (A_UINT32)pNFCALGROUPParms), pParmsOffset);
    return((void*) pNFCALGROUPParms);
}

static NFCALGROUP_OP_FUNC NFCALGROUPOpFunc = NULL;

TLV2_API void registerNFCALGROUPHandler(NFCALGROUP_OP_FUNC fp)
{
    NFCALGROUPOpFunc = fp;
}

A_BOOL NFCALGROUPOp(void *pParms)
{
    CMD_NFCALGROUP_PARMS *pNFCALGROUPParms = (CMD_NFCALGROUP_PARMS *)pParms;

#if 0 //for debugging, comment out this line, and uncomment the line below
//#ifdef _DEBUG
    int i; 	//for initializing array parameter
    i = 0;	//assign a number to avoid warning in case i is not used

    A_PRINTF("NFCALGROUPOp: cmdIdNFCalGroup %u\n", pNFCALGROUPParms->cmdIdNFCalGroup);
    A_PRINTF("NFCALGROUPOp: phyId %u\n", pNFCALGROUPParms->phyId);
    A_PRINTF("NFCALGROUPOp: bandCode %u\n", pNFCALGROUPParms->bandCode);
    A_PRINTF("NFCALGROUPOp: bwCode %u\n", pNFCALGROUPParms->bwCode);
    A_PRINTF("NFCALGROUPOp: freq %u\n", pNFCALGROUPParms->freq);
    A_PRINTF("NFCALGROUPOp: txChMask 0x%x\n", pNFCALGROUPParms->txChMask);
    A_PRINTF("NFCALGROUPOp: rxChMask 0x%x\n", pNFCALGROUPParms->rxChMask);
    A_PRINTF("NFCALGROUPOp: concMode %u\n", pNFCALGROUPParms->concMode);
    A_PRINTF("NFCALGROUPOp: pad1 %u\n", pNFCALGROUPParms->pad1);
    A_PRINTF("NFCALGROUPOp: userParm1 %u\n", pNFCALGROUPParms->userParm1);
    A_PRINTF("NFCALGROUPOp: userParm2 %u\n", pNFCALGROUPParms->userParm2);
    for (i = 0; i < 8 ; i++) // can be modified to print up to 16 entries
    {
        A_PRINTF("NFCALGROUPOp: nfCalResults %u\n", pNFCALGROUPParms->nfCalResults[i]);
    }
#endif //_DEBUG

    if (NULL != NFCALGROUPOpFunc) {
        (*NFCALGROUPOpFunc)(pNFCALGROUPParms);
    }
    return(TRUE);
}

void* initNFCALGROUPRSPOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict)
{
    int i, j; 	//for initializing array parameter
    CMD_NFCALGROUPRSP_PARMS  *pNFCALGROUPRSPParms = (CMD_NFCALGROUPRSP_PARMS *)pParmsCommon;

    if (pParmsCommon == NULL) return (NULL);

    i = j = 0;	//assign a number to avoid warning in case i and j are not used

    // Populate the parm structure with initial values
    pNFCALGROUPRSPParms->phyId = pParmDict[PARM_PHYID].v.valU8;
    pNFCALGROUPRSPParms->status = pParmDict[PARM_STATUS].v.valU8;
    pNFCALGROUPRSPParms->miscFlags = pParmDict[PARM_MISCFLAGS].v.valU8;
    pNFCALGROUPRSPParms->pad1 = pParmDict[PARM_PAD1].v.valU8;
    memset(pNFCALGROUPRSPParms->nfCalResults, 0, sizeof(pNFCALGROUPRSPParms->nfCalResults));
    pNFCALGROUPRSPParms->executionTime = pParmDict[PARM_EXECUTIONTIME].v.valU32;

    // Make up ParmOffsetTbl
    resetParmOffsetFields();
    fillParmOffsetTbl((A_UINT32)PARM_PHYID, (A_UINT32)(((A_UINT32)&(pNFCALGROUPRSPParms->phyId)) - (A_UINT32)pNFCALGROUPRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_STATUS, (A_UINT32)(((A_UINT32)&(pNFCALGROUPRSPParms->status)) - (A_UINT32)pNFCALGROUPRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_MISCFLAGS, (A_UINT32)(((A_UINT32)&(pNFCALGROUPRSPParms->miscFlags)) - (A_UINT32)pNFCALGROUPRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_PAD1, (A_UINT32)(((A_UINT32)&(pNFCALGROUPRSPParms->pad1)) - (A_UINT32)pNFCALGROUPRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_NFCALRESULTS, (A_UINT32)(((A_UINT32)&(pNFCALGROUPRSPParms->nfCalResults)) - (A_UINT32)pNFCALGROUPRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_EXECUTIONTIME, (A_UINT32)(((A_UINT32)&(pNFCALGROUPRSPParms->executionTime)) - (A_UINT32)pNFCALGROUPRSPParms), pParmsOffset);
    return((void*) pNFCALGROUPRSPParms);
}

static NFCALGROUPRSP_OP_FUNC NFCALGROUPRSPOpFunc = NULL;

TLV2_API void registerNFCALGROUPRSPHandler(NFCALGROUPRSP_OP_FUNC fp)
{
    NFCALGROUPRSPOpFunc = fp;
}

A_BOOL NFCALGROUPRSPOp(void *pParms)
{
    CMD_NFCALGROUPRSP_PARMS *pNFCALGROUPRSPParms = (CMD_NFCALGROUPRSP_PARMS *)pParms;

#if 0 //for debugging, comment out this line, and uncomment the line below
//#ifdef _DEBUG
    int i; 	//for initializing array parameter
    i = 0;	//assign a number to avoid warning in case i is not used

    A_PRINTF("NFCALGROUPRSPOp: phyId %u\n", pNFCALGROUPRSPParms->phyId);
    A_PRINTF("NFCALGROUPRSPOp: status %u\n", pNFCALGROUPRSPParms->status);
    A_PRINTF("NFCALGROUPRSPOp: miscFlags %u\n", pNFCALGROUPRSPParms->miscFlags);
    A_PRINTF("NFCALGROUPRSPOp: pad1 %u\n", pNFCALGROUPRSPParms->pad1);
    for (i = 0; i < 8 ; i++) // can be modified to print up to 16 entries
    {
        A_PRINTF("NFCALGROUPRSPOp: nfCalResults %u\n", pNFCALGROUPRSPParms->nfCalResults[i]);
    }
    A_PRINTF("NFCALGROUPRSPOp: executionTime %u\n", pNFCALGROUPRSPParms->executionTime);
#endif //_DEBUG

    if (NULL != NFCALGROUPRSPOpFunc) {
        (*NFCALGROUPRSPOpFunc)(pNFCALGROUPRSPParms);
    }
    return(TRUE);
}
