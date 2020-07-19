/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

// This is an auto-generated file from input/cmdPHYDBGdump.s
#include "tlv2Inc.h"
#include "cmdPHYDBGdump.h"

void* initPHYDBGDUMPOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict)
{
    int i, j; 	//for initializing array parameter
    CMD_PHYDBGDUMP_PARMS  *pPHYDBGDUMPParms = (CMD_PHYDBGDUMP_PARMS *)pParmsCommon;

    if (pParmsCommon == NULL) return (NULL);

    i = j = 0;	//assign a number to avoid warning in case i and j are not used

    // Populate the parm structure with initial values
    pPHYDBGDUMPParms->PHYDBGdumpOption = pParmDict[PARM_PHYDBGDUMPOPTION].v.valU32;
    pPHYDBGDUMPParms->phyId = pParmDict[PARM_PHYID].v.valU8;

    // Make up ParmOffsetTbl
    resetParmOffsetFields();
    fillParmOffsetTbl((A_UINT32)PARM_PHYDBGDUMPOPTION, (A_UINT32)(((A_UINT32)&(pPHYDBGDUMPParms->PHYDBGdumpOption)) - (A_UINT32)pPHYDBGDUMPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_PHYID, (A_UINT32)(((A_UINT32)&(pPHYDBGDUMPParms->phyId)) - (A_UINT32)pPHYDBGDUMPParms), pParmsOffset);
    return((void*) pPHYDBGDUMPParms);
}

static PHYDBGDUMP_OP_FUNC PHYDBGDUMPOpFunc = NULL;

TLV2_API void registerPHYDBGDUMPHandler(PHYDBGDUMP_OP_FUNC fp)
{
    PHYDBGDUMPOpFunc = fp;
}

A_BOOL PHYDBGDUMPOp(void *pParms)
{
    CMD_PHYDBGDUMP_PARMS *pPHYDBGDUMPParms = (CMD_PHYDBGDUMP_PARMS *)pParms;

#if 0 //for debugging, comment out this line, and uncomment the line below
//#ifdef _DEBUG
    int i; 	//for initializing array parameter
    i = 0;	//assign a number to avoid warning in case i is not used

    A_PRINTF("PHYDBGDUMPOp: PHYDBGdumpOption %u\n", pPHYDBGDUMPParms->PHYDBGdumpOption);
    A_PRINTF("PHYDBGDUMPOp: phyId %u\n", pPHYDBGDUMPParms->phyId);
#endif //_DEBUG

    if (NULL != PHYDBGDUMPOpFunc) {
        (*PHYDBGDUMPOpFunc)(pPHYDBGDUMPParms);
    }
    return(TRUE);
}

void* initPHYDBGDUMPRSPOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict)
{
    int i, j; 	//for initializing array parameter
    CMD_PHYDBGDUMPRSP_PARMS  *pPHYDBGDUMPRSPParms = (CMD_PHYDBGDUMPRSP_PARMS *)pParmsCommon;

    if (pParmsCommon == NULL) return (NULL);

    i = j = 0;	//assign a number to avoid warning in case i and j are not used

    // Populate the parm structure with initial values
    memset(pPHYDBGDUMPRSPParms->data4k, 0, sizeof(pPHYDBGDUMPRSPParms->data4k));
    pPHYDBGDUMPRSPParms->status = pParmDict[PARM_STATUS].v.valU8;
    pPHYDBGDUMPRSPParms->phyId = pParmDict[PARM_PHYID].v.valU8;

    // Make up ParmOffsetTbl
    resetParmOffsetFields();
    fillParmOffsetTbl((A_UINT32)PARM_DATA4K, (A_UINT32)(((A_UINT32)&(pPHYDBGDUMPRSPParms->data4k)) - (A_UINT32)pPHYDBGDUMPRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_STATUS, (A_UINT32)(((A_UINT32)&(pPHYDBGDUMPRSPParms->status)) - (A_UINT32)pPHYDBGDUMPRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_PHYID, (A_UINT32)(((A_UINT32)&(pPHYDBGDUMPRSPParms->phyId)) - (A_UINT32)pPHYDBGDUMPRSPParms), pParmsOffset);
    return((void*) pPHYDBGDUMPRSPParms);
}

static PHYDBGDUMPRSP_OP_FUNC PHYDBGDUMPRSPOpFunc = NULL;

TLV2_API void registerPHYDBGDUMPRSPHandler(PHYDBGDUMPRSP_OP_FUNC fp)
{
    PHYDBGDUMPRSPOpFunc = fp;
}

A_BOOL PHYDBGDUMPRSPOp(void *pParms)
{
    CMD_PHYDBGDUMPRSP_PARMS *pPHYDBGDUMPRSPParms = (CMD_PHYDBGDUMPRSP_PARMS *)pParms;

#if 0 //for debugging, comment out this line, and uncomment the line below
//#ifdef _DEBUG
    int i; 	//for initializing array parameter
    i = 0;	//assign a number to avoid warning in case i is not used

    for (i = 0; i < 8 ; i++) // can be modified to print up to 4096 entries
    {
        A_PRINTF("PHYDBGDUMPRSPOp: data4k 0x%x\n", pPHYDBGDUMPRSPParms->data4k[i]);
    }
    A_PRINTF("PHYDBGDUMPRSPOp: status %u\n", pPHYDBGDUMPRSPParms->status);
    A_PRINTF("PHYDBGDUMPRSPOp: phyId %u\n", pPHYDBGDUMPRSPParms->phyId);
#endif //_DEBUG

    if (NULL != PHYDBGDUMPRSPOpFunc) {
        (*PHYDBGDUMPRSPOpFunc)(pPHYDBGDUMPRSPParms);
    }
    return(TRUE);
}
