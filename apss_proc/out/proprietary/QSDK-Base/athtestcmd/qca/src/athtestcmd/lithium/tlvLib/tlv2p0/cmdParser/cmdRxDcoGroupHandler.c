/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

// This is an auto-generated file from input\cmdRxDcoGroupHandler.s
#include "tlv2Inc.h"
#include "cmdRxDcoGroupHandler.h"

void* initRXDCOGROUPOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict)
{
#if 0  // Helium code which is not applicable to Lithum codebase
    int i, j; 	//for initializing array parameter
    CMD_RXDCOGROUP_PARMS  *pRXDCOGROUPParms = (CMD_RXDCOGROUP_PARMS *)pParmsCommon;

    if (pParmsCommon == NULL) return (NULL);

    i = j = 0;	//assign a number to avoid warning in case i and j are not used

    // Populate the parm structure with initial values
    pRXDCOGROUPParms->cmdIdRxDcoGroup = pParmDict[PARM_CMDIDRXDCOGROUP].v.valU8;
    pRXDCOGROUPParms->phyId = pParmDict[PARM_PHYID].v.valU8;
    pRXDCOGROUPParms->bandCode = pParmDict[PARM_BANDCODE].v.valU8;
    pRXDCOGROUPParms->bwCode = pParmDict[PARM_BWCODE].v.valU8;
    pRXDCOGROUPParms->chainMask = pParmDict[PARM_CHAINMASK].v.valU16;
    pRXDCOGROUPParms->rxdco_cal_mode = pParmDict[PARM_RXDCO_CAL_MODE].v.valU16;
    pRXDCOGROUPParms->homeCh = pParmDict[PARM_HOMECH].v.valU8;
    pRXDCOGROUPParms->initLUT = pParmDict[PARM_INITLUT].v.valU8;
    pRXDCOGROUPParms->verbose = pParmDict[PARM_VERBOSE].v.valU8;
    pRXDCOGROUPParms->pad1 = pParmDict[PARM_PAD1].v.valU8;
    pRXDCOGROUPParms->bbOvrdI = pParmDict[PARM_BBOVRDI].v.valU16;
    pRXDCOGROUPParms->bbOvrdQ = pParmDict[PARM_BBOVRDQ].v.valU16;
    pRXDCOGROUPParms->rfOvrdI = pParmDict[PARM_RFOVRDI].v.valU16;
    pRXDCOGROUPParms->rfOvrdQ = pParmDict[PARM_RFOVRDQ].v.valU16;
    pRXDCOGROUPParms->userParm1 = pParmDict[PARM_USERPARM1].v.valU32;
    pRXDCOGROUPParms->userParm2 = pParmDict[PARM_USERPARM2].v.valU32;
    pRXDCOGROUPParms->userParm3 = pParmDict[PARM_USERPARM3].v.valU32;
    pRXDCOGROUPParms->userParm4 = pParmDict[PARM_USERPARM4].v.valU32;
    pRXDCOGROUPParms->bbDcocRange0I = pParmDict[PARM_BBDCOCRANGE0I].v.valU8;
    pRXDCOGROUPParms->bbDcocRange0Q = pParmDict[PARM_BBDCOCRANGE0Q].v.valU8;
    pRXDCOGROUPParms->rfDcocRange0I = pParmDict[PARM_RFDCOCRANGE0I].v.valU8;
    pRXDCOGROUPParms->rfDcocRange0Q = pParmDict[PARM_RFDCOCRANGE0Q].v.valU8;
    memset(pRXDCOGROUPParms->corrRF_DCO_LUT_i, 0, sizeof(pRXDCOGROUPParms->corrRF_DCO_LUT_i));
    memset(pRXDCOGROUPParms->corrRF_DCO_LUT_q, 0, sizeof(pRXDCOGROUPParms->corrRF_DCO_LUT_q));
    memset(pRXDCOGROUPParms->corrBB_DCO_LUT_i, 0, sizeof(pRXDCOGROUPParms->corrBB_DCO_LUT_i));
    memset(pRXDCOGROUPParms->corrBB_DCO_LUT_q, 0, sizeof(pRXDCOGROUPParms->corrBB_DCO_LUT_q));

    // Make up ParmOffsetTbl
    resetParmOffsetFields();
    fillParmOffsetTbl((A_UINT32)PARM_CMDIDRXDCOGROUP, (A_UINT32)(((A_UINT32)&(pRXDCOGROUPParms->cmdIdRxDcoGroup)) - (A_UINT32)pRXDCOGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_PHYID, (A_UINT32)(((A_UINT32)&(pRXDCOGROUPParms->phyId)) - (A_UINT32)pRXDCOGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_BANDCODE, (A_UINT32)(((A_UINT32)&(pRXDCOGROUPParms->bandCode)) - (A_UINT32)pRXDCOGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_BWCODE, (A_UINT32)(((A_UINT32)&(pRXDCOGROUPParms->bwCode)) - (A_UINT32)pRXDCOGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_CHAINMASK, (A_UINT32)(((A_UINT32)&(pRXDCOGROUPParms->chainMask)) - (A_UINT32)pRXDCOGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_RXDCO_CAL_MODE, (A_UINT32)(((A_UINT32)&(pRXDCOGROUPParms->rxdco_cal_mode)) - (A_UINT32)pRXDCOGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_HOMECH, (A_UINT32)(((A_UINT32)&(pRXDCOGROUPParms->homeCh)) - (A_UINT32)pRXDCOGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_INITLUT, (A_UINT32)(((A_UINT32)&(pRXDCOGROUPParms->initLUT)) - (A_UINT32)pRXDCOGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_VERBOSE, (A_UINT32)(((A_UINT32)&(pRXDCOGROUPParms->verbose)) - (A_UINT32)pRXDCOGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_PAD1, (A_UINT32)(((A_UINT32)&(pRXDCOGROUPParms->pad1)) - (A_UINT32)pRXDCOGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_BBOVRDI, (A_UINT32)(((A_UINT32)&(pRXDCOGROUPParms->bbOvrdI)) - (A_UINT32)pRXDCOGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_BBOVRDQ, (A_UINT32)(((A_UINT32)&(pRXDCOGROUPParms->bbOvrdQ)) - (A_UINT32)pRXDCOGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_RFOVRDI, (A_UINT32)(((A_UINT32)&(pRXDCOGROUPParms->rfOvrdI)) - (A_UINT32)pRXDCOGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_RFOVRDQ, (A_UINT32)(((A_UINT32)&(pRXDCOGROUPParms->rfOvrdQ)) - (A_UINT32)pRXDCOGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_USERPARM1, (A_UINT32)(((A_UINT32)&(pRXDCOGROUPParms->userParm1)) - (A_UINT32)pRXDCOGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_USERPARM2, (A_UINT32)(((A_UINT32)&(pRXDCOGROUPParms->userParm2)) - (A_UINT32)pRXDCOGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_USERPARM3, (A_UINT32)(((A_UINT32)&(pRXDCOGROUPParms->userParm3)) - (A_UINT32)pRXDCOGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_USERPARM4, (A_UINT32)(((A_UINT32)&(pRXDCOGROUPParms->userParm4)) - (A_UINT32)pRXDCOGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_BBDCOCRANGE0I, (A_UINT32)(((A_UINT32)&(pRXDCOGROUPParms->bbDcocRange0I)) - (A_UINT32)pRXDCOGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_BBDCOCRANGE0Q, (A_UINT32)(((A_UINT32)&(pRXDCOGROUPParms->bbDcocRange0Q)) - (A_UINT32)pRXDCOGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_RFDCOCRANGE0I, (A_UINT32)(((A_UINT32)&(pRXDCOGROUPParms->rfDcocRange0I)) - (A_UINT32)pRXDCOGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_RFDCOCRANGE0Q, (A_UINT32)(((A_UINT32)&(pRXDCOGROUPParms->rfDcocRange0Q)) - (A_UINT32)pRXDCOGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_CORRRF_DCO_LUT_I, (A_UINT32)(((A_UINT32)&(pRXDCOGROUPParms->corrRF_DCO_LUT_i)) - (A_UINT32)pRXDCOGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_CORRRF_DCO_LUT_Q, (A_UINT32)(((A_UINT32)&(pRXDCOGROUPParms->corrRF_DCO_LUT_q)) - (A_UINT32)pRXDCOGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_CORRBB_DCO_LUT_I, (A_UINT32)(((A_UINT32)&(pRXDCOGROUPParms->corrBB_DCO_LUT_i)) - (A_UINT32)pRXDCOGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_CORRBB_DCO_LUT_Q, (A_UINT32)(((A_UINT32)&(pRXDCOGROUPParms->corrBB_DCO_LUT_q)) - (A_UINT32)pRXDCOGROUPParms), pParmsOffset);
    return((void*) pRXDCOGROUPParms);
#else
	 return((void*) NULL);
#endif
}

static RXDCOGROUP_OP_FUNC RXDCOGROUPOpFunc = NULL;

TLV2_API void registerRXDCOGROUPHandler(RXDCOGROUP_OP_FUNC fp)
{
    RXDCOGROUPOpFunc = fp;
}

A_BOOL RXDCOGROUPOp(void *pParms)
{
#if 0 // Helium code which is not compatible to Lithium code component.
    CMD_RXDCOGROUP_PARMS *pRXDCOGROUPParms = (CMD_RXDCOGROUP_PARMS *)pParms;

#if 0 //for debugging, comment out this line, and uncomment the line below
//#ifdef _DEBUG
    int i; 	//for initializing array parameter
    i = 0;	//assign a number to avoid warning in case i is not used

    A_PRINTF("RXDCOGROUPOp: cmdIdRxDcoGroup %u\n", pRXDCOGROUPParms->cmdIdRxDcoGroup);
    A_PRINTF("RXDCOGROUPOp: phyId %u\n", pRXDCOGROUPParms->phyId);
    A_PRINTF("RXDCOGROUPOp: bandCode %u\n", pRXDCOGROUPParms->bandCode);
    A_PRINTF("RXDCOGROUPOp: bwCode %u\n", pRXDCOGROUPParms->bwCode);
    A_PRINTF("RXDCOGROUPOp: chainMask 0x%x\n", pRXDCOGROUPParms->chainMask);
    A_PRINTF("RXDCOGROUPOp: rxdco_cal_mode 0x%x\n", pRXDCOGROUPParms->rxdco_cal_mode);
    A_PRINTF("RXDCOGROUPOp: homeCh %u\n", pRXDCOGROUPParms->homeCh);
    A_PRINTF("RXDCOGROUPOp: initLUT %u\n", pRXDCOGROUPParms->initLUT);
    A_PRINTF("RXDCOGROUPOp: verbose %u\n", pRXDCOGROUPParms->verbose);
    A_PRINTF("RXDCOGROUPOp: pad1 %u\n", pRXDCOGROUPParms->pad1);
    A_PRINTF("RXDCOGROUPOp: bbOvrdI %u\n", pRXDCOGROUPParms->bbOvrdI);
    A_PRINTF("RXDCOGROUPOp: bbOvrdQ %u\n", pRXDCOGROUPParms->bbOvrdQ);
    A_PRINTF("RXDCOGROUPOp: rfOvrdI %u\n", pRXDCOGROUPParms->rfOvrdI);
    A_PRINTF("RXDCOGROUPOp: rfOvrdQ %u\n", pRXDCOGROUPParms->rfOvrdQ);
    A_PRINTF("RXDCOGROUPOp: userParm1 %u\n", pRXDCOGROUPParms->userParm1);
    A_PRINTF("RXDCOGROUPOp: userParm2 %u\n", pRXDCOGROUPParms->userParm2);
    A_PRINTF("RXDCOGROUPOp: userParm3 %u\n", pRXDCOGROUPParms->userParm3);
    A_PRINTF("RXDCOGROUPOp: userParm4 %u\n", pRXDCOGROUPParms->userParm4);
    A_PRINTF("RXDCOGROUPOp: bbDcocRange0I %u\n", pRXDCOGROUPParms->bbDcocRange0I);
    A_PRINTF("RXDCOGROUPOp: bbDcocRange0Q %u\n", pRXDCOGROUPParms->bbDcocRange0Q);
    A_PRINTF("RXDCOGROUPOp: rfDcocRange0I %u\n", pRXDCOGROUPParms->rfDcocRange0I);
    A_PRINTF("RXDCOGROUPOp: rfDcocRange0Q %u\n", pRXDCOGROUPParms->rfDcocRange0Q);
    for (i = 0; i < 8 ; i++) // can be modified to print up to 256 entries
    {
        A_PRINTF("RXDCOGROUPOp: corrRF_DCO_LUT_i 0x%x\n", pRXDCOGROUPParms->corrRF_DCO_LUT_i[i]);
    }
    for (i = 0; i < 8 ; i++) // can be modified to print up to 256 entries
    {
        A_PRINTF("RXDCOGROUPOp: corrRF_DCO_LUT_q 0x%x\n", pRXDCOGROUPParms->corrRF_DCO_LUT_q[i]);
    }
    for (i = 0; i < 8 ; i++) // can be modified to print up to 256 entries
    {
        A_PRINTF("RXDCOGROUPOp: corrBB_DCO_LUT_i 0x%x\n", pRXDCOGROUPParms->corrBB_DCO_LUT_i[i]);
    }
    for (i = 0; i < 8 ; i++) // can be modified to print up to 256 entries
    {
        A_PRINTF("RXDCOGROUPOp: corrBB_DCO_LUT_q 0x%x\n", pRXDCOGROUPParms->corrBB_DCO_LUT_q[i]);
    }
#endif //_DEBUG

    if (NULL != RXDCOGROUPOpFunc) {
        (*RXDCOGROUPOpFunc)(pRXDCOGROUPParms);
    }
#endif
    return(TRUE);
}

void* initRXDCOGROUPRSPOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict)
{
    int i, j; 	//for initializing array parameter
    CMD_RXDCOGROUPRSP_PARMS  *pRXDCOGROUPRSPParms = (CMD_RXDCOGROUPRSP_PARMS *)pParmsCommon;

    if (pParmsCommon == NULL) return (NULL);

    i = j = 0;	//assign a number to avoid warning in case i and j are not used

    // Populate the parm structure with initial values
    pRXDCOGROUPRSPParms->phyId = pParmDict[PARM_PHYID].v.valU8;
    memset(pRXDCOGROUPRSPParms->pad3, 0, sizeof(pRXDCOGROUPRSPParms->pad3));
    memset(pRXDCOGROUPRSPParms->calStatus, 0, sizeof(pRXDCOGROUPRSPParms->calStatus));
    pRXDCOGROUPRSPParms->bbDcocRange0I = pParmDict[PARM_BBDCOCRANGE0I].v.valU8;
    pRXDCOGROUPRSPParms->bbDcocRange0Q = pParmDict[PARM_BBDCOCRANGE0Q].v.valU8;
    pRXDCOGROUPRSPParms->rfDcocRange0I = pParmDict[PARM_RFDCOCRANGE0I].v.valU8;
    pRXDCOGROUPRSPParms->rfDcocRange0Q = pParmDict[PARM_RFDCOCRANGE0Q].v.valU8;
    memset(pRXDCOGROUPRSPParms->corrRF_DCO_LUT_i, 0, sizeof(pRXDCOGROUPRSPParms->corrRF_DCO_LUT_i));
    memset(pRXDCOGROUPRSPParms->corrRF_DCO_LUT_q, 0, sizeof(pRXDCOGROUPRSPParms->corrRF_DCO_LUT_q));
    memset(pRXDCOGROUPRSPParms->corrBB_DCO_LUT_i, 0, sizeof(pRXDCOGROUPRSPParms->corrBB_DCO_LUT_i));
    memset(pRXDCOGROUPRSPParms->corrBB_DCO_LUT_q, 0, sizeof(pRXDCOGROUPRSPParms->corrBB_DCO_LUT_q));

    // Make up ParmOffsetTbl
    resetParmOffsetFields();
    fillParmOffsetTbl((A_UINT32)PARM_PHYID, (A_UINT32)(((A_UINT32)&(pRXDCOGROUPRSPParms->phyId)) - (A_UINT32)pRXDCOGROUPRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_PAD3, (A_UINT32)(((A_UINT32)&(pRXDCOGROUPRSPParms->pad3)) - (A_UINT32)pRXDCOGROUPRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_CALSTATUS, (A_UINT32)(((A_UINT32)&(pRXDCOGROUPRSPParms->calStatus)) - (A_UINT32)pRXDCOGROUPRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_BBDCOCRANGE0I, (A_UINT32)(((A_UINT32)&(pRXDCOGROUPRSPParms->bbDcocRange0I)) - (A_UINT32)pRXDCOGROUPRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_BBDCOCRANGE0Q, (A_UINT32)(((A_UINT32)&(pRXDCOGROUPRSPParms->bbDcocRange0Q)) - (A_UINT32)pRXDCOGROUPRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_RFDCOCRANGE0I, (A_UINT32)(((A_UINT32)&(pRXDCOGROUPRSPParms->rfDcocRange0I)) - (A_UINT32)pRXDCOGROUPRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_RFDCOCRANGE0Q, (A_UINT32)(((A_UINT32)&(pRXDCOGROUPRSPParms->rfDcocRange0Q)) - (A_UINT32)pRXDCOGROUPRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_CORRRF_DCO_LUT_I, (A_UINT32)(((A_UINT32)&(pRXDCOGROUPRSPParms->corrRF_DCO_LUT_i)) - (A_UINT32)pRXDCOGROUPRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_CORRRF_DCO_LUT_Q, (A_UINT32)(((A_UINT32)&(pRXDCOGROUPRSPParms->corrRF_DCO_LUT_q)) - (A_UINT32)pRXDCOGROUPRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_CORRBB_DCO_LUT_I, (A_UINT32)(((A_UINT32)&(pRXDCOGROUPRSPParms->corrBB_DCO_LUT_i)) - (A_UINT32)pRXDCOGROUPRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_CORRBB_DCO_LUT_Q, (A_UINT32)(((A_UINT32)&(pRXDCOGROUPRSPParms->corrBB_DCO_LUT_q)) - (A_UINT32)pRXDCOGROUPRSPParms), pParmsOffset);
    return((void*) pRXDCOGROUPRSPParms);
}

static RXDCOGROUPRSP_OP_FUNC RXDCOGROUPRSPOpFunc = NULL;

TLV2_API void registerRXDCOGROUPRSPHandler(RXDCOGROUPRSP_OP_FUNC fp)
{
    RXDCOGROUPRSPOpFunc = fp;
}

A_BOOL RXDCOGROUPRSPOp(void *pParms)
{
    CMD_RXDCOGROUPRSP_PARMS *pRXDCOGROUPRSPParms = (CMD_RXDCOGROUPRSP_PARMS *)pParms;

#if 0 //for debugging, comment out this line, and uncomment the line below
//#ifdef _DEBUG
    int i; 	//for initializing array parameter
    i = 0;	//assign a number to avoid warning in case i is not used

    A_PRINTF("RXDCOGROUPRSPOp: phyId %u\n", pRXDCOGROUPRSPParms->phyId);
    for (i = 0; i < 3 ; i++)
    {
        A_PRINTF("RXDCOGROUPRSPOp: pad3 %u\n", pRXDCOGROUPRSPParms->pad3[i]);
    }
    for (i = 0; i < 8 ; i++) // can be modified to print up to 16 entries
    {
        A_PRINTF("RXDCOGROUPRSPOp: calStatus %u\n", pRXDCOGROUPRSPParms->calStatus[i]);
    }
    A_PRINTF("RXDCOGROUPRSPOp: bbDcocRange0I %u\n", pRXDCOGROUPRSPParms->bbDcocRange0I);
    A_PRINTF("RXDCOGROUPRSPOp: bbDcocRange0Q %u\n", pRXDCOGROUPRSPParms->bbDcocRange0Q);
    A_PRINTF("RXDCOGROUPRSPOp: rfDcocRange0I %u\n", pRXDCOGROUPRSPParms->rfDcocRange0I);
    A_PRINTF("RXDCOGROUPRSPOp: rfDcocRange0Q %u\n", pRXDCOGROUPRSPParms->rfDcocRange0Q);
    for (i = 0; i < 8 ; i++) // can be modified to print up to 256 entries
    {
        A_PRINTF("RXDCOGROUPRSPOp: corrRF_DCO_LUT_i 0x%x\n", pRXDCOGROUPRSPParms->corrRF_DCO_LUT_i[i]);
    }
    for (i = 0; i < 8 ; i++) // can be modified to print up to 256 entries
    {
        A_PRINTF("RXDCOGROUPRSPOp: corrRF_DCO_LUT_q 0x%x\n", pRXDCOGROUPRSPParms->corrRF_DCO_LUT_q[i]);
    }
    for (i = 0; i < 8 ; i++) // can be modified to print up to 256 entries
    {
        A_PRINTF("RXDCOGROUPRSPOp: corrBB_DCO_LUT_i 0x%x\n", pRXDCOGROUPRSPParms->corrBB_DCO_LUT_i[i]);
    }
    for (i = 0; i < 8 ; i++) // can be modified to print up to 256 entries
    {
        A_PRINTF("RXDCOGROUPRSPOp: corrBB_DCO_LUT_q 0x%x\n", pRXDCOGROUPRSPParms->corrBB_DCO_LUT_q[i]);
    }
#endif //_DEBUG

    if (NULL != RXDCOGROUPRSPOpFunc) {
        (*RXDCOGROUPRSPOpFunc)(pRXDCOGROUPRSPParms);
    }
    return(TRUE);
}
