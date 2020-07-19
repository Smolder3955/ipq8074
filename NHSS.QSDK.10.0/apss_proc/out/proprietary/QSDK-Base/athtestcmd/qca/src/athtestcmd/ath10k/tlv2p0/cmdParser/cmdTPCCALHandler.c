#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#if !defined(_HOST_SIM_TESTING)
#include "osapi.h"
#endif
#include "wlantype.h"
#include "tsCommon.h"
#include "cmdRspParmsDict.h"
#include "cmdRspParmsInternal.h"
#include "cmdTPCCALHandler.h"
#include "tlv2Api.h"

SRAM_TEXT
void* initTPCCALOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict)
{
    // Populate the parm structure with initial values
    CMD_TPCCAL_PARMS  *pTPCCALParms=(CMD_TPCCAL_PARMS *)pParmsCommon;
    pTPCCALParms->radioId = pParmDict[PARM_RADIOID].v.valU8;

    // Make up ParmOffsetTbl
    resetParmOffsetFields();
    fillParmOffsetTbl((A_UINT32)PARM_RADIOID, (A_UINT32)(((A_UINT32)&(pTPCCALParms->radioId)) - (A_UINT32)pTPCCALParms), pParmsOffset);

    return((void*) pTPCCALParms);
}

SRAM_DATA
static TPCCAL_OP_FUNC TPCCALOpFunc=NULL;
SRAM_TEXT
void registerTPCCALHandler(TPCCAL_OP_FUNC fp)
{
    TPCCALOpFunc = fp;
}

SRAM_TEXT
A_BOOL TPCCALOp(void *pParms)
{
    CMD_TPCCAL_PARMS *pTPCCALParms=(CMD_TPCCAL_PARMS *)pParms;

    //A_PRINTF_ALWAYS("TPCCALOp: radioId %d\n", (int)pTPCCALParms->radioId);

    if (NULL != TPCCALOpFunc) {
        (*TPCCALOpFunc)(pTPCCALParms);
    }

    return(TRUE);
}


SRAM_TEXT
void* initTPCCALRSPOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict)
{
    // Populate the parm structure with initial values
    CMD_TPCCALRSP_PARMS  *pTPCCALRSPParms=(CMD_TPCCALRSP_PARMS *)pParmsCommon;
    pTPCCALRSPParms->radioId = pParmDict[PARM_RADIOID].v.valU8;
    pTPCCALRSPParms->numFreq2G = pParmDict[PARM_NUMFREQ2G].v.valU8;
    pTPCCALRSPParms->freq2G[0] = pParmDict[PARM_FREQ2G].v.ptU8[0];
    pTPCCALRSPParms->numFreq5G = pParmDict[PARM_NUMFREQ5G].v.valU8;
    pTPCCALRSPParms->freq5G[0] = pParmDict[PARM_FREQ5G].v.ptU8[0];
    pTPCCALRSPParms->numChain = pParmDict[PARM_NUMCHAIN].v.valU8;
    pTPCCALRSPParms->chainMasks[0] = pParmDict[PARM_CHAINMASKS].v.ptU8[0];
    pTPCCALRSPParms->numCALPt2G = pParmDict[PARM_NUMCALPT2G].v.valU8;
    pTPCCALRSPParms->tgtPwr2G[0] = pParmDict[PARM_TGTPWR2G].v.ptS16[0];
    pTPCCALRSPParms->CALPt2G[0] = pParmDict[PARM_CALPT2G].v.ptU8[0];
    pTPCCALRSPParms->txGains2G[0] = pParmDict[PARM_TXGAINS2G].v.ptU8[0];
    pTPCCALRSPParms->dacGains2G[0] = pParmDict[PARM_DACGAINS2G].v.ptU8[0];
    pTPCCALRSPParms->paCfg2G[0] = pParmDict[PARM_PACFG2G].v.ptU8[0];
    pTPCCALRSPParms->numCALPt5G = pParmDict[PARM_NUMCALPT5G].v.valU8;
    pTPCCALRSPParms->CALPt5G[0] = pParmDict[PARM_CALPT5G].v.ptU8[0];
    pTPCCALRSPParms->tgtPwr5G[0] = pParmDict[PARM_TGTPWR5G].v.ptS16[0];
    pTPCCALRSPParms->txGains5G[0] = pParmDict[PARM_TXGAINS5G].v.ptU8[0];
    pTPCCALRSPParms->dacGains5G[0] = pParmDict[PARM_DACGAINS5G].v.ptU8[0];
    pTPCCALRSPParms->paCfg5G[0] = pParmDict[PARM_PACFG5G].v.ptU8[0];
    pTPCCALRSPParms->miscFlags = pParmDict[PARM_MISCFLAGS].v.valU8;

    // Make up ParmOffsetTbl
    resetParmOffsetFields();
    fillParmOffsetTbl((A_UINT32)PARM_RADIOID, (A_UINT32)(((A_UINT32)&(pTPCCALRSPParms->radioId)) - (A_UINT32)pTPCCALRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_NUMFREQ2G, (A_UINT32)(((A_UINT32)&(pTPCCALRSPParms->numFreq2G)) - (A_UINT32)pTPCCALRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_FREQ2G, (A_UINT32)(((A_UINT32)&(pTPCCALRSPParms->freq2G)) - (A_UINT32)pTPCCALRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_NUMFREQ5G, (A_UINT32)(((A_UINT32)&(pTPCCALRSPParms->numFreq5G)) - (A_UINT32)pTPCCALRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_FREQ5G, (A_UINT32)(((A_UINT32)&(pTPCCALRSPParms->freq5G)) - (A_UINT32)pTPCCALRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_NUMCHAIN, (A_UINT32)(((A_UINT32)&(pTPCCALRSPParms->numChain)) - (A_UINT32)pTPCCALRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_CHAINMASKS, (A_UINT32)(((A_UINT32)&(pTPCCALRSPParms->chainMasks)) - (A_UINT32)pTPCCALRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_NUMCALPT2G, (A_UINT32)(((A_UINT32)&(pTPCCALRSPParms->numCALPt2G)) - (A_UINT32)pTPCCALRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_TGTPWR2G, (A_UINT32)(((A_UINT32)&(pTPCCALRSPParms->tgtPwr2G)) - (A_UINT32)pTPCCALRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_CALPT2G, (A_UINT32)(((A_UINT32)&(pTPCCALRSPParms->CALPt2G)) - (A_UINT32)pTPCCALRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_TXGAINS2G, (A_UINT32)(((A_UINT32)&(pTPCCALRSPParms->txGains2G)) - (A_UINT32)pTPCCALRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_DACGAINS2G, (A_UINT32)(((A_UINT32)&(pTPCCALRSPParms->dacGains2G)) - (A_UINT32)pTPCCALRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_PACFG2G, (A_UINT32)(((A_UINT32)&(pTPCCALRSPParms->paCfg2G)) - (A_UINT32)pTPCCALRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_NUMCALPT5G, (A_UINT32)(((A_UINT32)&(pTPCCALRSPParms->numCALPt5G)) - (A_UINT32)pTPCCALRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_TGTPWR5G, (A_UINT32)(((A_UINT32)&(pTPCCALRSPParms->tgtPwr5G)) - (A_UINT32)pTPCCALRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_CALPT5G, (A_UINT32)(((A_UINT32)&(pTPCCALRSPParms->CALPt5G)) - (A_UINT32)pTPCCALRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_TXGAINS5G, (A_UINT32)(((A_UINT32)&(pTPCCALRSPParms->txGains5G)) - (A_UINT32)pTPCCALRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_DACGAINS5G, (A_UINT32)(((A_UINT32)&(pTPCCALRSPParms->dacGains5G)) - (A_UINT32)pTPCCALRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_PACFG5G, (A_UINT32)(((A_UINT32)&(pTPCCALRSPParms->paCfg5G)) - (A_UINT32)pTPCCALRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_MISCFLAGS, (A_UINT32)(((A_UINT32)&(pTPCCALRSPParms->miscFlags)) - (A_UINT32)pTPCCALRSPParms), pParmsOffset);

    return((void*) pTPCCALRSPParms);
}

SRAM_DATA
static TPCCALRSP_OP_FUNC TPCCALRSPOpFunc=NULL;
SRAM_TEXT
TLV2_API void registerTPCCALRSPHandler(TPCCALRSP_OP_FUNC fp)
{
    TPCCALRSPOpFunc = fp;
}

SRAM_TEXT
A_BOOL TPCCALRSPOp(void *pParms)
{
    CMD_TPCCALRSP_PARMS  *pTPCCALRSPParms=(CMD_TPCCALRSP_PARMS *)pParms;

#if 0
    A_PRINTF_ALWAYS("TPCCALRSPOp: radioId %d numFreq2G %d freq2G %d numFreq5G %d freq5G %d numChain %d numCALPt %d \n",
        (int)pTPCCALRSPParms->radioId,
        (int)pTPCCALRSPParms->numFreq2G,
        (int)pTPCCALRSPParms->freq2G[0],
        (int)pTPCCALRSPParms->numFreq5G,
        (int)pTPCCALRSPParms->freq5G[0],
        (int)pTPCCALRSPParms->numChain,
        (int)pTPCCALRSPParms->numCALPt);
#endif

    if (NULL != TPCCALRSPOpFunc) {
        (*TPCCALRSPOpFunc)(pTPCCALRSPParms);
    }

    return(TRUE);
}


