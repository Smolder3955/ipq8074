// This is an auto-generated file from input/cmdHwCalHandler.s
#include "tlv2Inc.h"
#include "cmdHwCalHandler.h"

void* initHWCALOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict)
{
    int i; 	//for initializing array parameter
    CMD_HWCAL_PARMS  *pHWCALParms = (CMD_HWCAL_PARMS *)pParmsCommon;

    if (pParmsCommon == NULL) return (NULL);

    i = 0;	//assign a number to avoid warning in case i is not used

    // Populate the parm structure with initial values
    pHWCALParms->radioId = pParmDict[PARM_RADIOID].v.valU8;
    pHWCALParms->calOp = pParmDict[PARM_CALOP].v.valU8;
    pHWCALParms->loopCnt = pParmDict[PARM_LOOPCNT].v.valU8;
    pHWCALParms->chainMask = pParmDict[PARM_CHAINMASK].v.valU8;
    pHWCALParms->loopBack = pParmDict[PARM_LOOPBACK].v.valU8;
    pHWCALParms->saveCal = pParmDict[PARM_SAVECAL].v.valU8;
    pHWCALParms->noiseFloorCal = pParmDict[PARM_NOISEFLOORCAL].v.valU8;

    // Make up ParmOffsetTbl
    resetParmOffsetFields();
    fillParmOffsetTbl((A_UINT32)PARM_RADIOID, (A_UINT32)(((A_UINT32)&(pHWCALParms->radioId)) - (A_UINT32)pHWCALParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_CALOP, (A_UINT32)(((A_UINT32)&(pHWCALParms->calOp)) - (A_UINT32)pHWCALParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_LOOPCNT, (A_UINT32)(((A_UINT32)&(pHWCALParms->loopCnt)) - (A_UINT32)pHWCALParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_CHAINMASK, (A_UINT32)(((A_UINT32)&(pHWCALParms->chainMask)) - (A_UINT32)pHWCALParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_LOOPBACK, (A_UINT32)(((A_UINT32)&(pHWCALParms->loopBack)) - (A_UINT32)pHWCALParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_SAVECAL, (A_UINT32)(((A_UINT32)&(pHWCALParms->saveCal)) - (A_UINT32)pHWCALParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_NOISEFLOORCAL, (A_UINT32)(((A_UINT32)&(pHWCALParms->noiseFloorCal)) - (A_UINT32)pHWCALParms), pParmsOffset);
    return((void*) pHWCALParms);
}

static HWCAL_OP_FUNC HWCALOpFunc = NULL;

TLV2_API void registerHWCALHandler(HWCAL_OP_FUNC fp)
{
    HWCALOpFunc = fp;
}

A_BOOL HWCALOp(void *pParms)
{
    CMD_HWCAL_PARMS *pHWCALParms = (CMD_HWCAL_PARMS *)pParms;

#if 0 //for debugging, comment out this line, and uncomment the line below
//#ifdef _DEBUG
    int i; 	//for initializing array parameter
    i = 0;	//assign a number to avoid warning in case i is not used

    A_PRINTF("HWCALOp: radioId %u\n", pHWCALParms->radioId);
    A_PRINTF("HWCALOp: calOp %u\n", pHWCALParms->calOp);
    A_PRINTF("HWCALOp: loopCnt %u\n", pHWCALParms->loopCnt);
    A_PRINTF("HWCALOp: chainMask 0x%x\n", pHWCALParms->chainMask);
    A_PRINTF("HWCALOp: loopBack %u\n", pHWCALParms->loopBack);
    A_PRINTF("HWCALOp: saveCal %u\n", pHWCALParms->saveCal);
    A_PRINTF("HWCALOp: noiseFloorCal %u\n", pHWCALParms->noiseFloorCal);
#endif //_DEBUG

    if (NULL != HWCALOpFunc) {
        (*HWCALOpFunc)(pHWCALParms);
    }
    return(TRUE);
}
