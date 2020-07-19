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
#include "cmdTPCCALPWRHandler.h"
#include "tlv2Api.h"

SRAM_TEXT
void* initTPCCALPWROpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict)
{
    // Populate the parm structure with initial values
    CMD_TPCCALPWR_PARMS  *pTPCCALPWRParms=(CMD_TPCCALPWR_PARMS *)pParmsCommon;

    pTPCCALPWRParms->radioId = pParmDict[PARM_RADIOID].v.valU8;
    memcpy((void*)&(pTPCCALPWRParms->measuredPwr[0]), (void*)&(pParmDict[PARM_MEASUREDPWR].v.ptS16[0]), sizeof(pTPCCALPWRParms->measuredPwr)); 
    pTPCCALPWRParms->numMeasuredPwr = pParmDict[PARM_NUMMEASUREDPWR].v.valU8;

    // Make up ParmOffsetTbl
    resetParmOffsetFields();
    fillParmOffsetTbl((A_UINT32)PARM_RADIOID, (A_UINT32)(((A_UINT32)&(pTPCCALPWRParms->radioId)) - (A_UINT32)pTPCCALPWRParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_MEASUREDPWR, (A_UINT32)(((A_UINT32)&(pTPCCALPWRParms->measuredPwr)) - (A_UINT32)pTPCCALPWRParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_NUMMEASUREDPWR, (A_UINT32)(((A_UINT32)&(pTPCCALPWRParms->numMeasuredPwr)) - (A_UINT32)pTPCCALPWRParms), pParmsOffset);

    return((void*) pTPCCALPWRParms);
}

SRAM_DATA
static TPCCALPWR_OP_FUNC TPCCALPWROpFunc=NULL;
SRAM_TEXT
void registerTPCCALPWRHandler(TPCCALPWR_OP_FUNC fp)
{
    TPCCALPWROpFunc = fp;
}

SRAM_TEXT
A_BOOL TPCCALPWROp(void *pParms)
{
    //int i;
    CMD_TPCCALPWR_PARMS *pTPCCALPWRParms=(CMD_TPCCALPWR_PARMS *)pParms;

//    A_PRINTF_ALWAYS("TPCCALPWROp: radioId %d numMeasuredPwr %d\n", (int)pTPCCALPWRParms->radioId, pTPCCALPWRParms->numMeasuredPwr);
#if 0
    for (i=0;i<pTPCCALPWRParms->numMeasuredPwr;i++) {
        printf("%d ", pTPCCALPWRParms->measuredPwr[i]);
    }
    printf("\n");
#endif

    if (NULL != TPCCALPWROpFunc) {
        (*TPCCALPWROpFunc)(pTPCCALPWRParms);
    }

    return(TRUE);
}
    

SRAM_TEXT
void* initTPCCALDATAOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict)
{
    // Populate the parm structure with initial values
    CMD_TPCCALDATA_PARMS  *pTPCCALDATAParms=(CMD_TPCCALDATA_PARMS *)pParmsCommon;
    pTPCCALDATAParms->radioId = pParmDict[PARM_RADIOID].v.valU8;
    pTPCCALDATAParms->calData2GOffset = pParmDict[PARM_TPCCALDATA2GOFFSET].v.valU16; 
    pTPCCALDATAParms->calData2GLength = pParmDict[PARM_TPCCALDATA2GLENGTH].v.valU16; 
    memcpy((void*)&(pTPCCALDATAParms->calData2G[0]), (void*)&(pParmDict[PARM_TPCCALDATA2G].v.ptU8[0]), sizeof(pTPCCALDATAParms->calData2G)); 
    pTPCCALDATAParms->calData5GOffset = pParmDict[PARM_TPCCALDATA5GOFFSET].v.valU16; 
    pTPCCALDATAParms->calData5GLength = pParmDict[PARM_TPCCALDATA5GLENGTH].v.valU16; 
    memcpy((void*)&(pTPCCALDATAParms->calData5G[0]), (void*)&(pParmDict[PARM_TPCCALDATA5G].v.ptU8[0]), sizeof(pTPCCALDATAParms->calData5G)); 
    pTPCCALDATAParms->numFreq2G = pParmDict[PARM_NUMFREQ2G].v.valU8; 
    pTPCCALDATAParms->freq2G[0] = pParmDict[PARM_FREQ2G].v.ptU8[0]; 
    pTPCCALDATAParms->numFreq5G = pParmDict[PARM_NUMFREQ5G].v.valU8; 
    pTPCCALDATAParms->freq5G[0] = pParmDict[PARM_FREQ5G].v.ptU8[0]; 
    pTPCCALDATAParms->numChain = pParmDict[PARM_NUMCHAIN].v.valU8; 
    pTPCCALDATAParms->chainMasks[0] = pParmDict[PARM_CHAINMASKS].v.ptU8[0]; 
    pTPCCALDATAParms->numCALPt2G = pParmDict[PARM_NUMCALPT2G].v.valU8; 
    pTPCCALDATAParms->CALPt2G[0] = pParmDict[PARM_CALPT2G].v.ptU8[0]; 
    pTPCCALDATAParms->tgtPwr2G[0] = pParmDict[PARM_TGTPWR2G].v.ptS16[0]; 
    pTPCCALDATAParms->txGains2G[0] = pParmDict[PARM_TXGAINS2G].v.ptU8[0]; 
    pTPCCALDATAParms->dacGains2G[0] = pParmDict[PARM_DACGAINS2G].v.ptU8[0]; 
    pTPCCALDATAParms->paCfg2G[0] = pParmDict[PARM_PACFG2G].v.ptU8[0]; 
    pTPCCALDATAParms->numCALPt5G = pParmDict[PARM_NUMCALPT5G].v.valU8; 
    pTPCCALDATAParms->CALPt5G[0] = pParmDict[PARM_CALPT5G].v.ptU8[0]; 
    pTPCCALDATAParms->tgtPwr5G[0] = pParmDict[PARM_TGTPWR5G].v.ptS16[0]; 
    pTPCCALDATAParms->txGains5G[0] = pParmDict[PARM_TXGAINS5G].v.ptU8[0]; 
    pTPCCALDATAParms->dacGains5G[0] = pParmDict[PARM_DACGAINS5G].v.ptU8[0]; 
    pTPCCALDATAParms->paCfg5G[0] = pParmDict[PARM_PACFG5G].v.ptU8[0]; 
    pTPCCALDATAParms->miscFlags = pParmDict[PARM_MISCFLAGS].v.valU8; 
    pTPCCALDATAParms->calData2GOffset_clpc = pParmDict[PARM_TPCCALDATA2GOFFSET_CLPC].v.valU16; 
    pTPCCALDATAParms->calData2GLength_clpc = pParmDict[PARM_TPCCALDATA2GLENGTH_CLPC].v.valU16; 
    memcpy((void*)&(pTPCCALDATAParms->calData2G_clpc[0]), (void*)&(pParmDict[PARM_TPCCALDATA2G_CLPC].v.ptU8[0]), sizeof(pTPCCALDATAParms->calData2G_clpc)); 
    pTPCCALDATAParms->calData5GOffset_clpc = pParmDict[PARM_TPCCALDATA5GOFFSET_CLPC].v.valU16; 
    pTPCCALDATAParms->calData5GLength_clpc = pParmDict[PARM_TPCCALDATA5GLENGTH_CLPC].v.valU16; 
    memcpy((void*)&(pTPCCALDATAParms->calData5G_clpc[0]), (void*)&(pParmDict[PARM_TPCCALDATA5G_CLPC].v.ptU8[0]), sizeof(pTPCCALDATAParms->calData5G_clpc)); 

    // Make up ParmOffsetTbl
    resetParmOffsetFields();
    fillParmOffsetTbl((A_UINT32)PARM_RADIOID, (A_UINT32)(((A_UINT32)&(pTPCCALDATAParms->radioId)) - (A_UINT32)pTPCCALDATAParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_TPCCALDATA2GOFFSET, (A_UINT32)(((A_UINT32)&(pTPCCALDATAParms->calData2GOffset)) - (A_UINT32)pTPCCALDATAParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_TPCCALDATA2GLENGTH, (A_UINT32)(((A_UINT32)&(pTPCCALDATAParms->calData2GLength)) - (A_UINT32)pTPCCALDATAParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_TPCCALDATA2G, (A_UINT32)(((A_UINT32)&(pTPCCALDATAParms->calData2G)) - (A_UINT32)pTPCCALDATAParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_TPCCALDATA5GOFFSET, (A_UINT32)(((A_UINT32)&(pTPCCALDATAParms->calData5GOffset)) - (A_UINT32)pTPCCALDATAParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_TPCCALDATA5GLENGTH, (A_UINT32)(((A_UINT32)&(pTPCCALDATAParms->calData5GLength)) - (A_UINT32)pTPCCALDATAParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_TPCCALDATA5G, (A_UINT32)(((A_UINT32)&(pTPCCALDATAParms->calData5G)) - (A_UINT32)pTPCCALDATAParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_NUMFREQ2G, (A_UINT32)(((A_UINT32)&(pTPCCALDATAParms->numFreq2G)) - (A_UINT32)pTPCCALDATAParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_FREQ2G, (A_UINT32)(((A_UINT32)&(pTPCCALDATAParms->freq2G)) - (A_UINT32)pTPCCALDATAParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_NUMFREQ5G, (A_UINT32)(((A_UINT32)&(pTPCCALDATAParms->numFreq5G)) - (A_UINT32)pTPCCALDATAParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_FREQ5G, (A_UINT32)(((A_UINT32)&(pTPCCALDATAParms->freq5G)) - (A_UINT32)pTPCCALDATAParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_NUMCHAIN, (A_UINT32)(((A_UINT32)&(pTPCCALDATAParms->numChain)) - (A_UINT32)pTPCCALDATAParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_CHAINMASKS, (A_UINT32)(((A_UINT32)&(pTPCCALDATAParms->chainMasks)) - (A_UINT32)pTPCCALDATAParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_NUMCALPT2G, (A_UINT32)(((A_UINT32)&(pTPCCALDATAParms->numCALPt2G)) - (A_UINT32)pTPCCALDATAParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_CALPT2G, (A_UINT32)(((A_UINT32)&(pTPCCALDATAParms->CALPt2G)) - (A_UINT32)pTPCCALDATAParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_TXGAINS2G, (A_UINT32)(((A_UINT32)&(pTPCCALDATAParms->txGains2G)) - (A_UINT32)pTPCCALDATAParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_DACGAINS2G, (A_UINT32)(((A_UINT32)&(pTPCCALDATAParms->dacGains2G)) - (A_UINT32)pTPCCALDATAParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_PACFG2G, (A_UINT32)(((A_UINT32)&(pTPCCALDATAParms->paCfg2G)) - (A_UINT32)pTPCCALDATAParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_NUMCALPT5G, (A_UINT32)(((A_UINT32)&(pTPCCALDATAParms->numCALPt5G)) - (A_UINT32)pTPCCALDATAParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_CALPT5G, (A_UINT32)(((A_UINT32)&(pTPCCALDATAParms->CALPt5G)) - (A_UINT32)pTPCCALDATAParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_TXGAINS5G, (A_UINT32)(((A_UINT32)&(pTPCCALDATAParms->txGains5G)) - (A_UINT32)pTPCCALDATAParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_DACGAINS5G, (A_UINT32)(((A_UINT32)&(pTPCCALDATAParms->dacGains5G)) - (A_UINT32)pTPCCALDATAParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_PACFG5G, (A_UINT32)(((A_UINT32)&(pTPCCALDATAParms->paCfg5G)) - (A_UINT32)pTPCCALDATAParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_MISCFLAGS, (A_UINT32)(((A_UINT32)&(pTPCCALDATAParms->miscFlags)) - (A_UINT32)pTPCCALDATAParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_TPCCALDATA2GOFFSET_CLPC, (A_UINT32)(((A_UINT32)&(pTPCCALDATAParms->calData2GOffset_clpc)) - (A_UINT32)pTPCCALDATAParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_TPCCALDATA2GLENGTH_CLPC, (A_UINT32)(((A_UINT32)&(pTPCCALDATAParms->calData2GLength_clpc)) - (A_UINT32)pTPCCALDATAParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_TPCCALDATA2G_CLPC, (A_UINT32)(((A_UINT32)&(pTPCCALDATAParms->calData2G_clpc)) - (A_UINT32)pTPCCALDATAParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_TPCCALDATA5GOFFSET_CLPC, (A_UINT32)(((A_UINT32)&(pTPCCALDATAParms->calData5GOffset_clpc)) - (A_UINT32)pTPCCALDATAParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_TPCCALDATA5GLENGTH_CLPC, (A_UINT32)(((A_UINT32)&(pTPCCALDATAParms->calData5GLength_clpc)) - (A_UINT32)pTPCCALDATAParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_TPCCALDATA5G_CLPC, (A_UINT32)(((A_UINT32)&(pTPCCALDATAParms->calData5G_clpc)) - (A_UINT32)pTPCCALDATAParms), pParmsOffset);

    return((void*) pTPCCALDATAParms);
}

SRAM_DATA
static TPCCALDATA_OP_FUNC TPCCALDATAOpFunc=NULL;
SRAM_TEXT
TLV2_API void registerTPCCALDATAHandler(TPCCALDATA_OP_FUNC fp)
{
    TPCCALDATAOpFunc = fp;
}

SRAM_TEXT
A_BOOL TPCCALDATAOp(void *pParms)
{
    CMD_TPCCALDATA_PARMS  *pTPCCALDATAParms=(CMD_TPCCALDATA_PARMS *)pParms;
 
#if 0
    printf("TPCCALPWROp: radioId %d calData2G %d calData2GOffset %d calData2GLen %d calData5G %d callData5GOffset %d calData5GLen %d \n", 
        (int)pTPCCALDATAParms->radioId,
        (int)pTPCCALDATAParms->calData2G[0],
        (int)pTPCCALDATAParms->calData2GOffset,
        (int)pTPCCALDATAParms->calData2GLength,
        (int)pTPCCALDATAParms->calData5G[0],
        (int)pTPCCALDATAParms->calData5GOffset,
        (int)pTPCCALDATAParms->calData5GLength);
#endif

    if (NULL != TPCCALDATAOpFunc) {
        (*TPCCALDATAOpFunc)(pTPCCALDATAParms);
    }

    return(TRUE);
}


