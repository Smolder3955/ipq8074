/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

// This is an auto-generated file from input\cmdEfuseWriteHandler.s
#include "tlv2Inc.h"
#include "cmdEfuseWriteHandler.h"

void* initEFUSEWRITEOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict)
{
    int i, j; 	//for initializing array parameter
    CMD_EFUSEWRITE_PARMS  *pEFUSEWRITEParms = (CMD_EFUSEWRITE_PARMS *)pParmsCommon;

    if (pParmsCommon == NULL) return (NULL);

    i = j = 0;	//assign a number to avoid warning in case i and j are not used

    // Populate the parm structure with initial values
    pEFUSEWRITEParms->offset = pParmDict[PARM_OFFSET].v.valU32;
    pEFUSEWRITEParms->numBytes = pParmDict[PARM_NUMBYTES].v.valU16;
    memset(pEFUSEWRITEParms->efuseData, 0, sizeof(pEFUSEWRITEParms->efuseData));

    // Make up ParmOffsetTbl
    resetParmOffsetFields();
    fillParmOffsetTbl((A_UINT32)PARM_OFFSET, (A_UINT32)(((A_UINT32)&(pEFUSEWRITEParms->offset)) - (A_UINT32)pEFUSEWRITEParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_NUMBYTES, (A_UINT32)(((A_UINT32)&(pEFUSEWRITEParms->numBytes)) - (A_UINT32)pEFUSEWRITEParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_EFUSEDATA, (A_UINT32)(((A_UINT32)&(pEFUSEWRITEParms->efuseData)) - (A_UINT32)pEFUSEWRITEParms), pParmsOffset);
    return((void*) pEFUSEWRITEParms);
}

static EFUSEWRITE_OP_FUNC EFUSEWRITEOpFunc = NULL;

TLV2_API void registerEFUSEWRITEHandler(EFUSEWRITE_OP_FUNC fp)
{
    EFUSEWRITEOpFunc = fp;
}

A_BOOL EFUSEWRITEOp(void *pParms)
{
    CMD_EFUSEWRITE_PARMS *pEFUSEWRITEParms = (CMD_EFUSEWRITE_PARMS *)pParms;

#if 0 //for debugging, comment out this line, and uncomment the line below
//#ifdef _DEBUG
    int i; 	//for initializing array parameter
    i = 0;	//assign a number to avoid warning in case i is not used

    A_PRINTF("EFUSEWRITEOp: offset 0x%x\n", pEFUSEWRITEParms->offset);
    A_PRINTF("EFUSEWRITEOp: numBytes %u\n", pEFUSEWRITEParms->numBytes);
    for (i = 0; i < 8 ; i++) // can be modified to print up to 256 entries
    {
        A_PRINTF("EFUSEWRITEOp: efuseData 0x%x\n", pEFUSEWRITEParms->efuseData[i]);
    }
#endif //_DEBUG

    if (NULL != EFUSEWRITEOpFunc) {
        (*EFUSEWRITEOpFunc)(pEFUSEWRITEParms);
    }
    return(TRUE);
}

void* initEFUSEWRITERSPOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict)
{
    int i, j; 	//for initializing array parameter
    CMD_EFUSEWRITERSP_PARMS  *pEFUSEWRITERSPParms = (CMD_EFUSEWRITERSP_PARMS *)pParmsCommon;

    if (pParmsCommon == NULL) return (NULL);

    i = j = 0;	//assign a number to avoid warning in case i and j are not used

    // Populate the parm structure with initial values
    pEFUSEWRITERSPParms->status = pParmDict[PARM_STATUS].v.valU8;

    // Make up ParmOffsetTbl
    resetParmOffsetFields();
    fillParmOffsetTbl((A_UINT32)PARM_STATUS, (A_UINT32)(((A_UINT32)&(pEFUSEWRITERSPParms->status)) - (A_UINT32)pEFUSEWRITERSPParms), pParmsOffset);
    return((void*) pEFUSEWRITERSPParms);
}

static EFUSEWRITERSP_OP_FUNC EFUSEWRITERSPOpFunc = NULL;

TLV2_API void registerEFUSEWRITERSPHandler(EFUSEWRITERSP_OP_FUNC fp)
{
    EFUSEWRITERSPOpFunc = fp;
}

A_BOOL EFUSEWRITERSPOp(void *pParms)
{
    CMD_EFUSEWRITERSP_PARMS *pEFUSEWRITERSPParms = (CMD_EFUSEWRITERSP_PARMS *)pParms;

#if 0 //for debugging, comment out this line, and uncomment the line below
//#ifdef _DEBUG
    int i; 	//for initializing array parameter
    i = 0;	//assign a number to avoid warning in case i is not used

    A_PRINTF("EFUSEWRITERSPOp: status %u\n", pEFUSEWRITERSPParms->status);
#endif //_DEBUG

    if (NULL != EFUSEWRITERSPOpFunc) {
        (*EFUSEWRITERSPOpFunc)(pEFUSEWRITERSPParms);
    }
    return(TRUE);
}
