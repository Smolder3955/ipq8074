/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

// This is an auto-generated file from input/cmdDACPlyback.s
#include "tlv2Inc.h"
#include "cmdDACPlyback.h"

void* initDACPLYBCKOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict)
{
    int i, j; 	//for initializing array parameter
    CMD_DACPLYBCK_PARMS  *pDACPLYBCKParms = (CMD_DACPLYBCK_PARMS *)pParmsCommon;

    if (pParmsCommon == NULL) return (NULL);

    i = j = 0;	//assign a number to avoid warning in case i and j are not used

    // Populate the parm structure with initial values
    pDACPLYBCKParms->phyId = pParmDict[PARM_PHYID].v.valU8;
    pDACPLYBCKParms->DACPlybckMode = pParmDict[PARM_DACPLYBCKMODE].v.valU8;
    pDACPLYBCKParms->DACPlybckOptions = pParmDict[PARM_DACPLYBCKOPTIONS].v.valU8;
    pDACPLYBCKParms->DACPlybckRadarOnLen = pParmDict[PARM_DACPLYBCKRADARONLEN].v.valU8;
    pDACPLYBCKParms->DACPlybckRadarOffLen = pParmDict[PARM_DACPLYBCKRADAROFFLEN].v.valU8;
    memset(pDACPLYBCKParms->pad3, 0, sizeof(pDACPLYBCKParms->pad3));
    pDACPLYBCKParms->chainMask = pParmDict[PARM_CHAINMASK].v.valU16;
    pDACPLYBCKParms->DACPlybckCount = pParmDict[PARM_DACPLYBCKCOUNT].v.valU16;
    memset(pDACPLYBCKParms->data4k, 0, sizeof(pDACPLYBCKParms->data4k));

    // Make up ParmOffsetTbl
    resetParmOffsetFields();
    fillParmOffsetTbl((A_UINT32)PARM_PHYID, (A_UINT32)(((A_UINT32)&(pDACPLYBCKParms->phyId)) - (A_UINT32)pDACPLYBCKParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_DACPLYBCKMODE, (A_UINT32)(((A_UINT32)&(pDACPLYBCKParms->DACPlybckMode)) - (A_UINT32)pDACPLYBCKParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_DACPLYBCKOPTIONS, (A_UINT32)(((A_UINT32)&(pDACPLYBCKParms->DACPlybckOptions)) - (A_UINT32)pDACPLYBCKParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_DACPLYBCKRADARONLEN, (A_UINT32)(((A_UINT32)&(pDACPLYBCKParms->DACPlybckRadarOnLen)) - (A_UINT32)pDACPLYBCKParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_DACPLYBCKRADAROFFLEN, (A_UINT32)(((A_UINT32)&(pDACPLYBCKParms->DACPlybckRadarOffLen)) - (A_UINT32)pDACPLYBCKParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_PAD3, (A_UINT32)(((A_UINT32)&(pDACPLYBCKParms->pad3)) - (A_UINT32)pDACPLYBCKParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_CHAINMASK, (A_UINT32)(((A_UINT32)&(pDACPLYBCKParms->chainMask)) - (A_UINT32)pDACPLYBCKParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_DACPLYBCKCOUNT, (A_UINT32)(((A_UINT32)&(pDACPLYBCKParms->DACPlybckCount)) - (A_UINT32)pDACPLYBCKParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_DATA4K, (A_UINT32)(((A_UINT32)&(pDACPLYBCKParms->data4k)) - (A_UINT32)pDACPLYBCKParms), pParmsOffset);
    return((void*) pDACPLYBCKParms);
}

static DACPLYBCK_OP_FUNC DACPLYBCKOpFunc = NULL;

TLV2_API void registerDACPLYBCKHandler(DACPLYBCK_OP_FUNC fp)
{
    DACPLYBCKOpFunc = fp;
}

A_BOOL DACPLYBCKOp(void *pParms)
{
    CMD_DACPLYBCK_PARMS *pDACPLYBCKParms = (CMD_DACPLYBCK_PARMS *)pParms;

#if 0 //for debugging, comment out this line, and uncomment the line below
//#ifdef _DEBUG
    int i; 	//for initializing array parameter
    i = 0;	//assign a number to avoid warning in case i is not used

    A_PRINTF("DACPLYBCKOp: phyId %u\n", pDACPLYBCKParms->phyId);
    A_PRINTF("DACPLYBCKOp: DACPlybckMode %u\n", pDACPLYBCKParms->DACPlybckMode);
    A_PRINTF("DACPLYBCKOp: DACPlybckOptions %u\n", pDACPLYBCKParms->DACPlybckOptions);
    A_PRINTF("DACPLYBCKOp: DACPlybckRadarOnLen %u\n", pDACPLYBCKParms->DACPlybckRadarOnLen);
    A_PRINTF("DACPLYBCKOp: DACPlybckRadarOffLen %u\n", pDACPLYBCKParms->DACPlybckRadarOffLen);
    for (i = 0; i < 3 ; i++)
    {
        A_PRINTF("DACPLYBCKOp: pad3 %u\n", pDACPLYBCKParms->pad3[i]);
    }
    A_PRINTF("DACPLYBCKOp: chainMask 0x%x\n", pDACPLYBCKParms->chainMask);
    A_PRINTF("DACPLYBCKOp: DACPlybckCount %u\n", pDACPLYBCKParms->DACPlybckCount);
    for (i = 0; i < 8 ; i++) // can be modified to print up to 4096 entries
    {
        A_PRINTF("DACPLYBCKOp: data4k 0x%x\n", pDACPLYBCKParms->data4k[i]);
    }
#endif //_DEBUG

    if (NULL != DACPLYBCKOpFunc) {
        (*DACPLYBCKOpFunc)(pDACPLYBCKParms);
    }
    return(TRUE);
}

void* initDACPLYBCKRSPOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict)
{
    int i, j; 	//for initializing array parameter
    CMD_DACPLYBCKRSP_PARMS  *pDACPLYBCKRSPParms = (CMD_DACPLYBCKRSP_PARMS *)pParmsCommon;

    if (pParmsCommon == NULL) return (NULL);

    i = j = 0;	//assign a number to avoid warning in case i and j are not used

    // Populate the parm structure with initial values
    pDACPLYBCKRSPParms->phyId = pParmDict[PARM_PHYID].v.valU8;
    pDACPLYBCKRSPParms->status = pParmDict[PARM_STATUS].v.valU8;

    // Make up ParmOffsetTbl
    resetParmOffsetFields();
    fillParmOffsetTbl((A_UINT32)PARM_PHYID, (A_UINT32)(((A_UINT32)&(pDACPLYBCKRSPParms->phyId)) - (A_UINT32)pDACPLYBCKRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_STATUS, (A_UINT32)(((A_UINT32)&(pDACPLYBCKRSPParms->status)) - (A_UINT32)pDACPLYBCKRSPParms), pParmsOffset);
    return((void*) pDACPLYBCKRSPParms);
}

static DACPLYBCKRSP_OP_FUNC DACPLYBCKRSPOpFunc = NULL;

TLV2_API void registerDACPLYBCKRSPHandler(DACPLYBCKRSP_OP_FUNC fp)
{
    DACPLYBCKRSPOpFunc = fp;
}

A_BOOL DACPLYBCKRSPOp(void *pParms)
{
    CMD_DACPLYBCKRSP_PARMS *pDACPLYBCKRSPParms = (CMD_DACPLYBCKRSP_PARMS *)pParms;

#if 0 //for debugging, comment out this line, and uncomment the line below
//#ifdef _DEBUG
    int i; 	//for initializing array parameter
    i = 0;	//assign a number to avoid warning in case i is not used

    A_PRINTF("DACPLYBCKRSPOp: phyId %u\n", pDACPLYBCKRSPParms->phyId);
    A_PRINTF("DACPLYBCKRSPOp: status %u\n", pDACPLYBCKRSPParms->status);
#endif //_DEBUG

    if (NULL != DACPLYBCKRSPOpFunc) {
        (*DACPLYBCKRSPOpFunc)(pDACPLYBCKRSPParms);
    }
    return(TRUE);
}
