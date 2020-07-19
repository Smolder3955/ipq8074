/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

// This is an auto-generated file from input\cmdBdGetSize.s
#include "tlv2Inc.h"
#include "cmdBdGetSize.h"

void* initBDGETSIZEOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict)
{
    return(NULL);
}

static BDGETSIZE_OP_FUNC BDGETSIZEOpFunc = NULL;

TLV2_API void registerBDGETSIZEHandler(BDGETSIZE_OP_FUNC fp)
{
    BDGETSIZEOpFunc = fp;
}

A_BOOL BDGETSIZEOp(void *pParms)
{
    if (NULL != BDGETSIZEOpFunc) {
        (*BDGETSIZEOpFunc)(NULL);
    }
    return(TRUE);
}

void* initBDGETSIZERSPOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict)
{
    int i, j; 	//for initializing array parameter
    CMD_BDGETSIZERSP_PARMS  *pBDGETSIZERSPParms = (CMD_BDGETSIZERSP_PARMS *)pParmsCommon;

    if (pParmsCommon == NULL) return (NULL);

    i = j = 0;	//assign a number to avoid warning in case i and j are not used

    // Populate the parm structure with initial values
    pBDGETSIZERSPParms->status = pParmDict[PARM_STATUS].v.valU8;
    memset(pBDGETSIZERSPParms->pad3, 0, sizeof(pBDGETSIZERSPParms->pad3));
    pBDGETSIZERSPParms->bdSize = pParmDict[PARM_BDSIZE].v.valU32;

    // Make up ParmOffsetTbl
    resetParmOffsetFields();
    fillParmOffsetTbl((A_UINT32)PARM_STATUS, (A_UINT32)(((A_UINT32)&(pBDGETSIZERSPParms->status)) - (A_UINT32)pBDGETSIZERSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_PAD3, (A_UINT32)(((A_UINT32)&(pBDGETSIZERSPParms->pad3)) - (A_UINT32)pBDGETSIZERSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_BDSIZE, (A_UINT32)(((A_UINT32)&(pBDGETSIZERSPParms->bdSize)) - (A_UINT32)pBDGETSIZERSPParms), pParmsOffset);
    return((void*) pBDGETSIZERSPParms);
}

static BDGETSIZERSP_OP_FUNC BDGETSIZERSPOpFunc = NULL;

TLV2_API void registerBDGETSIZERSPHandler(BDGETSIZERSP_OP_FUNC fp)
{
    BDGETSIZERSPOpFunc = fp;
}

A_BOOL BDGETSIZERSPOp(void *pParms)
{
    CMD_BDGETSIZERSP_PARMS *pBDGETSIZERSPParms = (CMD_BDGETSIZERSP_PARMS *)pParms;

#if 0 //for debugging, comment out this line, and uncomment the line below
//#ifdef _DEBUG
    int i; 	//for initializing array parameter
    i = 0;	//assign a number to avoid warning in case i is not used

    A_PRINTF("BDGETSIZERSPOp: status %u\n", pBDGETSIZERSPParms->status);
    for (i = 0; i < 3 ; i++)
    {
        A_PRINTF("BDGETSIZERSPOp: pad3 %u\n", pBDGETSIZERSPParms->pad3[i]);
    }
    A_PRINTF("BDGETSIZERSPOp: bdSize %u\n", pBDGETSIZERSPParms->bdSize);
#endif //_DEBUG

    if (NULL != BDGETSIZERSPOpFunc) {
        (*BDGETSIZERSPOpFunc)(pBDGETSIZERSPParms);
    }
    return(TRUE);
}
