/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

// This is an auto-generated file from input\cmdBdRead.s
#include "tlv2Inc.h"
#include "cmdBdRead.h"

void* initBDREADOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict)
{
    int i, j; 	//for initializing array parameter
    CMD_BDREAD_PARMS  *pBDREADParms = (CMD_BDREAD_PARMS *)pParmsCommon;

    if (pParmsCommon == NULL) return (NULL);

    i = j = 0;	//assign a number to avoid warning in case i and j are not used

    // Populate the parm structure with initial values
    pBDREADParms->bdSize = pParmDict[PARM_BDSIZE].v.valU32;
    pBDREADParms->offset = pParmDict[PARM_OFFSET].v.valU32;
    pBDREADParms->size = pParmDict[PARM_SIZE].v.valU32;

    // Make up ParmOffsetTbl
    resetParmOffsetFields();
    fillParmOffsetTbl((A_UINT32)PARM_BDSIZE, (A_UINT32)(((A_UINT32)&(pBDREADParms->bdSize)) - (A_UINT32)pBDREADParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_OFFSET, (A_UINT32)(((A_UINT32)&(pBDREADParms->offset)) - (A_UINT32)pBDREADParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_SIZE, (A_UINT32)(((A_UINT32)&(pBDREADParms->size)) - (A_UINT32)pBDREADParms), pParmsOffset);
    return((void*) pBDREADParms);
}

static BDREAD_OP_FUNC BDREADOpFunc = NULL;

TLV2_API void registerBDREADHandler(BDREAD_OP_FUNC fp)
{
    BDREADOpFunc = fp;
}

A_BOOL BDREADOp(void *pParms)
{
    CMD_BDREAD_PARMS *pBDREADParms = (CMD_BDREAD_PARMS *)pParms;

#if 0 //for debugging, comment out this line, and uncomment the line below
//#ifdef _DEBUG
    int i; 	//for initializing array parameter
    i = 0;	//assign a number to avoid warning in case i is not used

    A_PRINTF("BDREADOp: bdSize %u\n", pBDREADParms->bdSize);
    A_PRINTF("BDREADOp: offset %u\n", pBDREADParms->offset);
    A_PRINTF("BDREADOp: size %u\n", pBDREADParms->size);
#endif //_DEBUG

    if (NULL != BDREADOpFunc) {
        (*BDREADOpFunc)(pBDREADParms);
    }
    return(TRUE);
}

void* initBDREADRSPOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict)
{
    int i, j; 	//for initializing array parameter
    CMD_BDREADRSP_PARMS  *pBDREADRSPParms = (CMD_BDREADRSP_PARMS *)pParmsCommon;

    if (pParmsCommon == NULL) return (NULL);

    i = j = 0;	//assign a number to avoid warning in case i and j are not used

    // Populate the parm structure with initial values
    pBDREADRSPParms->status = pParmDict[PARM_STATUS].v.valU8;
    memset(pBDREADRSPParms->pad3, 0, sizeof(pBDREADRSPParms->pad3));
    pBDREADRSPParms->offset = pParmDict[PARM_OFFSET].v.valU32;
    pBDREADRSPParms->size = pParmDict[PARM_SIZE].v.valU32;
    memset(pBDREADRSPParms->data4k, 0, sizeof(pBDREADRSPParms->data4k));

    // Make up ParmOffsetTbl
    resetParmOffsetFields();
    fillParmOffsetTbl((A_UINT32)PARM_STATUS, (A_UINT32)(((A_UINT32)&(pBDREADRSPParms->status)) - (A_UINT32)pBDREADRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_PAD3, (A_UINT32)(((A_UINT32)&(pBDREADRSPParms->pad3)) - (A_UINT32)pBDREADRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_OFFSET, (A_UINT32)(((A_UINT32)&(pBDREADRSPParms->offset)) - (A_UINT32)pBDREADRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_SIZE, (A_UINT32)(((A_UINT32)&(pBDREADRSPParms->size)) - (A_UINT32)pBDREADRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_DATA4K, (A_UINT32)(((A_UINT32)&(pBDREADRSPParms->data4k)) - (A_UINT32)pBDREADRSPParms), pParmsOffset);
    return((void*) pBDREADRSPParms);
}

static BDREADRSP_OP_FUNC BDREADRSPOpFunc = NULL;

TLV2_API void registerBDREADRSPHandler(BDREADRSP_OP_FUNC fp)
{
    BDREADRSPOpFunc = fp;
}

A_BOOL BDREADRSPOp(void *pParms)
{
    CMD_BDREADRSP_PARMS *pBDREADRSPParms = (CMD_BDREADRSP_PARMS *)pParms;

#if 0 //for debugging, comment out this line, and uncomment the line below
//#ifdef _DEBUG
    int i; 	//for initializing array parameter
    i = 0;	//assign a number to avoid warning in case i is not used

    A_PRINTF("BDREADRSPOp: status %u\n", pBDREADRSPParms->status);
    for (i = 0; i < 3 ; i++)
    {
        A_PRINTF("BDREADRSPOp: pad3 %u\n", pBDREADRSPParms->pad3[i]);
    }
    A_PRINTF("BDREADRSPOp: offset %u\n", pBDREADRSPParms->offset);
    A_PRINTF("BDREADRSPOp: size %u\n", pBDREADRSPParms->size);
    for (i = 0; i < 8 ; i++) // can be modified to print up to 4096 entries
    {
        A_PRINTF("BDREADRSPOp: data4k 0x%x\n", pBDREADRSPParms->data4k[i]);
    }
#endif //_DEBUG

    if (NULL != BDREADRSPOpFunc) {
        (*BDREADRSPOpFunc)(pBDREADRSPParms);
    }
    return(TRUE);
}
