// This is an auto-generated file from input\cmdBasicRspHandler.s
#include "tlv2Inc.h"
#include "cmdBasicRspHandler.h"

//SRAM_TEXT
void* initBASICRSPOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict)
{
    int i; 	//for initializing array parameter
    CMD_BASICRSP_PARMS  *pBASICRSPParms = (CMD_BASICRSP_PARMS *)pParmsCommon;

    if (pParmsCommon == NULL) return (NULL);

    i = 0;	//assign a number to avoid warning in case i is not used

    // Populate the parm structure with initial values
    pBASICRSPParms->cmdId = pParmDict[PARM_CMDID].v.valU16;
    pBASICRSPParms->radioId = pParmDict[PARM_RADIOID].v.valU8;
    pBASICRSPParms->status = pParmDict[PARM_STATUS].v.valU8;

    // Make up ParmOffsetTbl
    resetParmOffsetFields();
    fillParmOffsetTbl((A_UINT32)PARM_CMDID, (A_UINT32)(((A_UINT32)&(pBASICRSPParms->cmdId)) - (A_UINT32)pBASICRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_RADIOID, (A_UINT32)(((A_UINT32)&(pBASICRSPParms->radioId)) - (A_UINT32)pBASICRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_STATUS, (A_UINT32)(((A_UINT32)&(pBASICRSPParms->status)) - (A_UINT32)pBASICRSPParms), pParmsOffset);
    return((void*) pBASICRSPParms);
}

//SRAM_DATA
static BASICRSP_OP_FUNC BASICRSPOpFunc = NULL;

//SRAM_TEXT
TLV2_API void registerBASICRSPHandler(BASICRSP_OP_FUNC fp)
{
    BASICRSPOpFunc = fp;
}

//SRAM_TEXT
A_BOOL BASICRSPOp(void *pParms)
{
    CMD_BASICRSP_PARMS *pBASICRSPParms = (CMD_BASICRSP_PARMS *)pParms;
#if 0
//#ifdef _DEBUG
    int i; 	//for initializing array parameter
    i = 0;	//assign a number to avoid warning in case i is not used

    A_PRINTF("BASICRSPOp: cmdId %u\n", pBASICRSPParms->cmdId);
    A_PRINTF("BASICRSPOp: radioId %u\n", pBASICRSPParms->radioId);
    A_PRINTF("BASICRSPOp: status %u\n", pBASICRSPParms->status);
#endif //_DEBUG

    if (NULL != BASICRSPOpFunc) {
        (*BASICRSPOpFunc)(pBASICRSPParms);
    }
    return(TRUE);
}
