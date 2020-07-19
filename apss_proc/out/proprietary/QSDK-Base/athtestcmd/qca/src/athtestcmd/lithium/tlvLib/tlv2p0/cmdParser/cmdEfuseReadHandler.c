/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

// This is an auto-generated file from input\cmdEfuseReadHandler.s
#include "tlv2Inc.h"
#include "cmdEfuseReadHandler.h"

void* initEFUSEREADOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict)
{
    int i, j; 	//for initializing array parameter
    CMD_EFUSEREAD_PARMS  *pEFUSEREADParms = (CMD_EFUSEREAD_PARMS *)pParmsCommon;

    if (pParmsCommon == NULL) return (NULL);

    i = j = 0;	//assign a number to avoid warning in case i and j are not used

    // Populate the parm structure with initial values
    pEFUSEREADParms->offset = pParmDict[PARM_OFFSET].v.valU32;
    pEFUSEREADParms->numBytes = pParmDict[PARM_NUMBYTES].v.valU16;

    // Make up ParmOffsetTbl
    resetParmOffsetFields();
    fillParmOffsetTbl((A_UINT32)PARM_OFFSET, (A_UINT32)(((A_UINT32)&(pEFUSEREADParms->offset)) - (A_UINT32)pEFUSEREADParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_NUMBYTES, (A_UINT32)(((A_UINT32)&(pEFUSEREADParms->numBytes)) - (A_UINT32)pEFUSEREADParms), pParmsOffset);
    return((void*) pEFUSEREADParms);
}

static EFUSEREAD_OP_FUNC EFUSEREADOpFunc = NULL;

TLV2_API void registerEFUSEREADHandler(EFUSEREAD_OP_FUNC fp)
{
    EFUSEREADOpFunc = fp;
}

A_BOOL EFUSEREADOp(void *pParms)
{
    CMD_EFUSEREAD_PARMS *pEFUSEREADParms = (CMD_EFUSEREAD_PARMS *)pParms;

#if 0 //for debugging, comment out this line, and uncomment the line below
//#ifdef _DEBUG
    int i; 	//for initializing array parameter
    i = 0;	//assign a number to avoid warning in case i is not used

    A_PRINTF("EFUSEREADOp: offset 0x%x\n", pEFUSEREADParms->offset);
    A_PRINTF("EFUSEREADOp: numBytes %u\n", pEFUSEREADParms->numBytes);
#endif //_DEBUG

    if (NULL != EFUSEREADOpFunc) {
        (*EFUSEREADOpFunc)(pEFUSEREADParms);
    }
    return(TRUE);
}

void* initEFUSEREADRSPOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict)
{
    int i, j; 	//for initializing array parameter
    CMD_EFUSEREADRSP_PARMS  *pEFUSEREADRSPParms = (CMD_EFUSEREADRSP_PARMS *)pParmsCommon;

    if (pParmsCommon == NULL) return (NULL);

    i = j = 0;	//assign a number to avoid warning in case i and j are not used

    // Populate the parm structure with initial values
    pEFUSEREADRSPParms->numBytes = pParmDict[PARM_NUMBYTES].v.valU16;
    memset(pEFUSEREADRSPParms->efuseData, 0, sizeof(pEFUSEREADRSPParms->efuseData));
    pEFUSEREADRSPParms->status = pParmDict[PARM_STATUS].v.valU8;

    // Make up ParmOffsetTbl
    resetParmOffsetFields();
    fillParmOffsetTbl((A_UINT32)PARM_NUMBYTES, (A_UINT32)(((A_UINT32)&(pEFUSEREADRSPParms->numBytes)) - (A_UINT32)pEFUSEREADRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_EFUSEDATA, (A_UINT32)(((A_UINT32)&(pEFUSEREADRSPParms->efuseData)) - (A_UINT32)pEFUSEREADRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_STATUS, (A_UINT32)(((A_UINT32)&(pEFUSEREADRSPParms->status)) - (A_UINT32)pEFUSEREADRSPParms), pParmsOffset);
    return((void*) pEFUSEREADRSPParms);
}

static EFUSEREADRSP_OP_FUNC EFUSEREADRSPOpFunc = NULL;

TLV2_API void registerEFUSEREADRSPHandler(EFUSEREADRSP_OP_FUNC fp)
{
    EFUSEREADRSPOpFunc = fp;
}

A_BOOL EFUSEREADRSPOp(void *pParms)
{
    CMD_EFUSEREADRSP_PARMS *pEFUSEREADRSPParms = (CMD_EFUSEREADRSP_PARMS *)pParms;

#if 0 //for debugging, comment out this line, and uncomment the line below
//#ifdef _DEBUG
    int i; 	//for initializing array parameter
    i = 0;	//assign a number to avoid warning in case i is not used

    A_PRINTF("EFUSEREADRSPOp: numBytes %u\n", pEFUSEREADRSPParms->numBytes);
    for (i = 0; i < 8 ; i++) // can be modified to print up to 256 entries
    {
        A_PRINTF("EFUSEREADRSPOp: efuseData 0x%x\n", pEFUSEREADRSPParms->efuseData[i]);
    }
    A_PRINTF("EFUSEREADRSPOp: status %u\n", pEFUSEREADRSPParms->status);
#endif //_DEBUG

    if (NULL != EFUSEREADRSPOpFunc) {
        (*EFUSEREADRSPOpFunc)(pEFUSEREADRSPParms);
    }
    return(TRUE);
}
