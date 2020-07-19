/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

// This is an auto-generated file from input/cmdAgcHistoryDump.s
#include "tlv2Inc.h"
#include "cmdAgcHistoryDump.h"

void* initAGCHISTORYDUMPOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict)
{
    int i, j; 	//for initializing array parameter
    CMD_AGCHISTORYDUMP_PARMS  *pAGCHISTORYDUMPParms = (CMD_AGCHISTORYDUMP_PARMS *)pParmsCommon;

    if (pParmsCommon == NULL) return (NULL);

    i = j = 0;	//assign a number to avoid warning in case i and j are not used

    // Populate the parm structure with initial values
    pAGCHISTORYDUMPParms->AGCHistorydumpOption = pParmDict[PARM_AGCHISTORYDUMPOPTION].v.valU32;
    pAGCHISTORYDUMPParms->phyId = pParmDict[PARM_PHYID].v.valU8;

    // Make up ParmOffsetTbl
    resetParmOffsetFields();
    fillParmOffsetTbl((A_UINT32)PARM_AGCHISTORYDUMPOPTION, (A_UINT32)(((A_UINT32)&(pAGCHISTORYDUMPParms->AGCHistorydumpOption)) - (A_UINT32)pAGCHISTORYDUMPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_PHYID, (A_UINT32)(((A_UINT32)&(pAGCHISTORYDUMPParms->phyId)) - (A_UINT32)pAGCHISTORYDUMPParms), pParmsOffset);
    return((void*) pAGCHISTORYDUMPParms);
}

static AGCHISTORYDUMP_OP_FUNC AGCHISTORYDUMPOpFunc = NULL;

TLV2_API void registerAGCHISTORYDUMPHandler(AGCHISTORYDUMP_OP_FUNC fp)
{
    AGCHISTORYDUMPOpFunc = fp;
}

A_BOOL AGCHISTORYDUMPOp(void *pParms)
{
    CMD_AGCHISTORYDUMP_PARMS *pAGCHISTORYDUMPParms = (CMD_AGCHISTORYDUMP_PARMS *)pParms;

#if 0 //for debugging, comment out this line, and uncomment the line below
//#ifdef _DEBUG
    int i; 	//for initializing array parameter
    i = 0;	//assign a number to avoid warning in case i is not used

    A_PRINTF("AGCHISTORYDUMPOp: AGCHistorydumpOption %u\n", pAGCHISTORYDUMPParms->AGCHistorydumpOption);
    A_PRINTF("AGCHISTORYDUMPOp: phyId %u\n", pAGCHISTORYDUMPParms->phyId);
#endif //_DEBUG

    if (NULL != AGCHISTORYDUMPOpFunc) {
        (*AGCHISTORYDUMPOpFunc)(pAGCHISTORYDUMPParms);
    }
    return(TRUE);
}

void* initAGCHISTORYDUMPRSPOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict)
{
    int i, j; 	//for initializing array parameter
    CMD_AGCHISTORYDUMPRSP_PARMS  *pAGCHISTORYDUMPRSPParms = (CMD_AGCHISTORYDUMPRSP_PARMS *)pParmsCommon;

    if (pParmsCommon == NULL) return (NULL);

    i = j = 0;	//assign a number to avoid warning in case i and j are not used

    // Populate the parm structure with initial values
    memset(pAGCHISTORYDUMPRSPParms->AGCHistoryData, 0, sizeof(pAGCHISTORYDUMPRSPParms->AGCHistoryData));
    pAGCHISTORYDUMPRSPParms->status = pParmDict[PARM_STATUS].v.valU8;
    pAGCHISTORYDUMPRSPParms->phyId = pParmDict[PARM_PHYID].v.valU8;

    // Make up ParmOffsetTbl
    resetParmOffsetFields();
    fillParmOffsetTbl((A_UINT32)PARM_AGCHISTORYDATA, (A_UINT32)(((A_UINT32)&(pAGCHISTORYDUMPRSPParms->AGCHistoryData)) - (A_UINT32)pAGCHISTORYDUMPRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_STATUS, (A_UINT32)(((A_UINT32)&(pAGCHISTORYDUMPRSPParms->status)) - (A_UINT32)pAGCHISTORYDUMPRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_PHYID, (A_UINT32)(((A_UINT32)&(pAGCHISTORYDUMPRSPParms->phyId)) - (A_UINT32)pAGCHISTORYDUMPRSPParms), pParmsOffset);
    return((void*) pAGCHISTORYDUMPRSPParms);
}

static AGCHISTORYDUMPRSP_OP_FUNC AGCHISTORYDUMPRSPOpFunc = NULL;

TLV2_API void registerAGCHISTORYDUMPRSPHandler(AGCHISTORYDUMPRSP_OP_FUNC fp)
{
    AGCHISTORYDUMPRSPOpFunc = fp;
}

A_BOOL AGCHISTORYDUMPRSPOp(void *pParms)
{
    CMD_AGCHISTORYDUMPRSP_PARMS *pAGCHISTORYDUMPRSPParms = (CMD_AGCHISTORYDUMPRSP_PARMS *)pParms;

#if 0 //for debugging, comment out this line, and uncomment the line below
//#ifdef _DEBUG
    int i; 	//for initializing array parameter
    i = 0;	//assign a number to avoid warning in case i is not used

    for (i = 0; i < 8 ; i++) // can be modified to print up to 320 entries
    {
        A_PRINTF("AGCHISTORYDUMPRSPOp: AGCHistoryData 0x%x\n", pAGCHISTORYDUMPRSPParms->AGCHistoryData[i]);
    }
    A_PRINTF("AGCHISTORYDUMPRSPOp: status %u\n", pAGCHISTORYDUMPRSPParms->status);
    A_PRINTF("AGCHISTORYDUMPRSPOp: phyId %u\n", pAGCHISTORYDUMPRSPParms->phyId);
#endif //_DEBUG

    if (NULL != AGCHISTORYDUMPRSPOpFunc) {
        (*AGCHISTORYDUMPRSPOpFunc)(pAGCHISTORYDUMPRSPParms);
    }
    return(TRUE);
}
