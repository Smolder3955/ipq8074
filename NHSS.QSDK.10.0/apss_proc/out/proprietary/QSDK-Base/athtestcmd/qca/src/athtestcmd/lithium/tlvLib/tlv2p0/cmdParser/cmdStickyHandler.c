/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

// This is an auto-generated file from input\cmdStickyHandler.s
#include "tlv2Inc.h"
#include "cmdStickyHandler.h"

void* initSTICKYOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict)
{
    int i, j; 	//for initializing array parameter
    CMD_STICKY_PARMS  *pSTICKYParms = (CMD_STICKY_PARMS *)pParmsCommon;

    if (pParmsCommon == NULL) return (NULL);

    i = j = 0;	//assign a number to avoid warning in case i and j are not used

    // Populate the parm structure with initial values
    pSTICKYParms->regAddress = pParmDict[PARM_REGADDRESS].v.valU32;
    pSTICKYParms->regMask = pParmDict[PARM_REGMASK].v.valU32;
    pSTICKYParms->stickyFlags = pParmDict[PARM_STICKYFLAGS].v.valU32;
    pSTICKYParms->regValue = pParmDict[PARM_REGVALUE].v.valU32;

    // Make up ParmOffsetTbl
    resetParmOffsetFields();
    fillParmOffsetTbl((A_UINT32)PARM_REGADDRESS, (A_UINT32)(((A_UINT32)&(pSTICKYParms->regAddress)) - (A_UINT32)pSTICKYParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_REGMASK, (A_UINT32)(((A_UINT32)&(pSTICKYParms->regMask)) - (A_UINT32)pSTICKYParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_STICKYFLAGS, (A_UINT32)(((A_UINT32)&(pSTICKYParms->stickyFlags)) - (A_UINT32)pSTICKYParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_REGVALUE, (A_UINT32)(((A_UINT32)&(pSTICKYParms->regValue)) - (A_UINT32)pSTICKYParms), pParmsOffset);
    return((void*) pSTICKYParms);
}

static STICKY_OP_FUNC STICKYOpFunc = NULL;

TLV2_API void registerSTICKYHandler(STICKY_OP_FUNC fp)
{
    STICKYOpFunc = fp;
}

A_BOOL STICKYOp(void *pParms)
{
    CMD_STICKY_PARMS *pSTICKYParms = (CMD_STICKY_PARMS *)pParms;

#if 0 //for debugging, comment out this line, and uncomment the line below
//#ifdef _DEBUG
    int i; 	//for initializing array parameter
    i = 0;	//assign a number to avoid warning in case i is not used

    A_PRINTF("STICKYOp: regAddress 0x%x\n", pSTICKYParms->regAddress);
    A_PRINTF("STICKYOp: regMask 0x%x\n", pSTICKYParms->regMask);
    A_PRINTF("STICKYOp: stickyFlags 0x%x\n", pSTICKYParms->stickyFlags);
    A_PRINTF("STICKYOp: regValue 0x%x\n", pSTICKYParms->regValue);
#endif //_DEBUG

    if (NULL != STICKYOpFunc) {
        (*STICKYOpFunc)(pSTICKYParms);
    }
    return(TRUE);
}
