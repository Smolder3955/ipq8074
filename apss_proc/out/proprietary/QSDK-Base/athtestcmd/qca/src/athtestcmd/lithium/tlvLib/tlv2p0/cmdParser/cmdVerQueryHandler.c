/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

// This is an auto-generated file from input\cmdVerQueryHandler.s
#include "tlv2Inc.h"
#include "cmdVerQueryHandler.h"

void* initVERQUERYOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict)
{
    return(NULL);
}

static VERQUERY_OP_FUNC VERQUERYOpFunc = NULL;

TLV2_API void registerVERQUERYHandler(VERQUERY_OP_FUNC fp)
{
    VERQUERYOpFunc = fp;
}

A_BOOL VERQUERYOp(void *pParms)
{
    if (NULL != VERQUERYOpFunc) {
        (*VERQUERYOpFunc)(NULL);
    }
    return(TRUE);
}

void* initVERQUERYRSPOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict)
{
    int i, j; 	//for initializing array parameter
    CMD_VERQUERYRSP_PARMS  *pVERQUERYRSPParms = (CMD_VERQUERYRSP_PARMS *)pParmsCommon;

    if (pParmsCommon == NULL) return (NULL);

    i = j = 0;	//assign a number to avoid warning in case i and j are not used

    // Populate the parm structure with initial values
    memset(pVERQUERYRSPParms->ver_info, 0, sizeof(pVERQUERYRSPParms->ver_info));

    // Make up ParmOffsetTbl
    resetParmOffsetFields();
    fillParmOffsetTbl((A_UINT32)PARM_VER_INFO, (A_UINT32)(((A_UINT32)&(pVERQUERYRSPParms->ver_info)) - (A_UINT32)pVERQUERYRSPParms), pParmsOffset);
    return((void*) pVERQUERYRSPParms);
}

static VERQUERYRSP_OP_FUNC VERQUERYRSPOpFunc = NULL;

TLV2_API void registerVERQUERYRSPHandler(VERQUERYRSP_OP_FUNC fp)
{
    VERQUERYRSPOpFunc = fp;
}

A_BOOL VERQUERYRSPOp(void *pParms)
{
    CMD_VERQUERYRSP_PARMS *pVERQUERYRSPParms = (CMD_VERQUERYRSP_PARMS *)pParms;

#if 0 //for debugging, comment out this line, and uncomment the line below
//#ifdef _DEBUG
    int i; 	//for initializing array parameter
    i = 0;	//assign a number to avoid warning in case i is not used

    for (i = 0; i < 8 ; i++) // can be modified to print up to 256 entries
    {
        A_PRINTF("VERQUERYRSPOp: ver_info %u\n", pVERQUERYRSPParms->ver_info[i]);
    }
#endif //_DEBUG

    if (NULL != VERQUERYRSPOpFunc) {
        (*VERQUERYRSPOpFunc)(pVERQUERYRSPParms);
    }
    return(TRUE);
}
