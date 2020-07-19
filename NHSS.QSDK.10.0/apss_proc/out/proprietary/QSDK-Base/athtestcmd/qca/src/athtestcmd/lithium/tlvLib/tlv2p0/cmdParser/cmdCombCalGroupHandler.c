/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

// This is an auto-generated file from input\cmdCombCalGroupHandler.s
#include "tlv2Inc.h"
#include "cmdCombCalGroupHandler.h"

void* initCOMBCALGROUPOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict)
{
    int i, j; 	//for initializing array parameter
    CMD_COMBCALGROUP_PARMS  *pCOMBCALGROUPParms = (CMD_COMBCALGROUP_PARMS *)pParmsCommon;

    if (pParmsCommon == NULL) return (NULL);

    i = j = 0;	//assign a number to avoid warning in case i and j are not used

    // Populate the parm structure with initial values
    pCOMBCALGROUPParms->cmdIdCombCalGroup = pParmDict[PARM_CMDIDCOMBCALGROUP].v.valU8;
    pCOMBCALGROUPParms->phyId = pParmDict[PARM_PHYID].v.valU8;
    pCOMBCALGROUPParms->bandCode = pParmDict[PARM_BANDCODE].v.valU8;
    pCOMBCALGROUPParms->bwCode = pParmDict[PARM_BWCODE].v.valU8;
    pCOMBCALGROUPParms->freq = pParmDict[PARM_FREQ].v.valU16;
    pCOMBCALGROUPParms->dyn_bw = pParmDict[PARM_DYN_BW].v.valU8;
    pCOMBCALGROUPParms->chain = pParmDict[PARM_CHAIN].v.valU8;
    pCOMBCALGROUPParms->num_plybck_tone = pParmDict[PARM_NUM_PLYBCK_TONE].v.valU8;
    pCOMBCALGROUPParms->lenPlybckTones = pParmDict[PARM_LENPLYBCKTONES].v.valU8;
    pCOMBCALGROUPParms->plybckToneIdx = pParmDict[PARM_PLYBCKTONEIDX].v.valU8;
    pCOMBCALGROUPParms->playBackLen = pParmDict[PARM_PLAYBACKLEN].v.valU8;
    memset(pCOMBCALGROUPParms->plybckTones, 0, sizeof(pCOMBCALGROUPParms->plybckTones));
    memset(pCOMBCALGROUPParms->calToneList, 0, sizeof(pCOMBCALGROUPParms->calToneList));
    pCOMBCALGROUPParms->initCal = pParmDict[PARM_INITCAL].v.valU8;
    pCOMBCALGROUPParms->iteration = pParmDict[PARM_ITERATION].v.valU8;
    pCOMBCALGROUPParms->clLoc = pParmDict[PARM_CLLOC].v.valU8;
    pCOMBCALGROUPParms->isRunCalTx = pParmDict[PARM_ISRUNCALTX].v.valU8;
    pCOMBCALGROUPParms->combine = pParmDict[PARM_COMBINE].v.valU8;
    pCOMBCALGROUPParms->homeCh = pParmDict[PARM_HOMECH].v.valU8;
    pCOMBCALGROUPParms->synth = pParmDict[PARM_SYNTH].v.valU8;
    pCOMBCALGROUPParms->verbose = pParmDict[PARM_VERBOSE].v.valU8;
    pCOMBCALGROUPParms->max_offset = pParmDict[PARM_MAX_OFFSET].v.valU8;
    pCOMBCALGROUPParms->DBS = pParmDict[PARM_DBS].v.valU8;
    pCOMBCALGROUPParms->iqIter = pParmDict[PARM_IQITER].v.valU8;
    pCOMBCALGROUPParms->clIter = pParmDict[PARM_CLITER].v.valU8;
    pCOMBCALGROUPParms->numTxGains = pParmDict[PARM_NUMTXGAINS].v.valU8;
    pCOMBCALGROUPParms->dbStep = pParmDict[PARM_DBSTEP].v.valU8;
    pCOMBCALGROUPParms->calMode = pParmDict[PARM_CALMODE].v.valU8;
    pCOMBCALGROUPParms->havePrevResults = pParmDict[PARM_HAVEPREVRESULTS].v.valU8;
    memset(pCOMBCALGROUPParms->prevResults_magTx, 0, sizeof(pCOMBCALGROUPParms->prevResults_magTx));
    memset(pCOMBCALGROUPParms->prevResults_magRx, 0, sizeof(pCOMBCALGROUPParms->prevResults_magRx));
    memset(pCOMBCALGROUPParms->prevResults_phaseTx, 0, sizeof(pCOMBCALGROUPParms->prevResults_phaseTx));
    memset(pCOMBCALGROUPParms->prevResults_phaseRx, 0, sizeof(pCOMBCALGROUPParms->prevResults_phaseRx));
    memset(pCOMBCALGROUPParms->clOffset, 0, sizeof(pCOMBCALGROUPParms->clOffset));
    memset(pCOMBCALGROUPParms->clCorr_real, 0, sizeof(pCOMBCALGROUPParms->clCorr_real));
    memset(pCOMBCALGROUPParms->clCorr_imag, 0, sizeof(pCOMBCALGROUPParms->clCorr_imag));
    memset(pCOMBCALGROUPParms->txCorr_real, 0, sizeof(pCOMBCALGROUPParms->txCorr_real));
    memset(pCOMBCALGROUPParms->txCorr_imag, 0, sizeof(pCOMBCALGROUPParms->txCorr_imag));
    memset(pCOMBCALGROUPParms->rxCorr_real, 0, sizeof(pCOMBCALGROUPParms->rxCorr_real));
    memset(pCOMBCALGROUPParms->rxCorr_imag, 0, sizeof(pCOMBCALGROUPParms->rxCorr_imag));

    // Make up ParmOffsetTbl
    resetParmOffsetFields();
    fillParmOffsetTbl((A_UINT32)PARM_CMDIDCOMBCALGROUP, (A_UINT32)(((A_UINT32)&(pCOMBCALGROUPParms->cmdIdCombCalGroup)) - (A_UINT32)pCOMBCALGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_PHYID, (A_UINT32)(((A_UINT32)&(pCOMBCALGROUPParms->phyId)) - (A_UINT32)pCOMBCALGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_BANDCODE, (A_UINT32)(((A_UINT32)&(pCOMBCALGROUPParms->bandCode)) - (A_UINT32)pCOMBCALGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_BWCODE, (A_UINT32)(((A_UINT32)&(pCOMBCALGROUPParms->bwCode)) - (A_UINT32)pCOMBCALGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_FREQ, (A_UINT32)(((A_UINT32)&(pCOMBCALGROUPParms->freq)) - (A_UINT32)pCOMBCALGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_DYN_BW, (A_UINT32)(((A_UINT32)&(pCOMBCALGROUPParms->dyn_bw)) - (A_UINT32)pCOMBCALGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_CHAIN, (A_UINT32)(((A_UINT32)&(pCOMBCALGROUPParms->chain)) - (A_UINT32)pCOMBCALGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_NUM_PLYBCK_TONE, (A_UINT32)(((A_UINT32)&(pCOMBCALGROUPParms->num_plybck_tone)) - (A_UINT32)pCOMBCALGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_LENPLYBCKTONES, (A_UINT32)(((A_UINT32)&(pCOMBCALGROUPParms->lenPlybckTones)) - (A_UINT32)pCOMBCALGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_PLYBCKTONEIDX, (A_UINT32)(((A_UINT32)&(pCOMBCALGROUPParms->plybckToneIdx)) - (A_UINT32)pCOMBCALGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_PLAYBACKLEN, (A_UINT32)(((A_UINT32)&(pCOMBCALGROUPParms->playBackLen)) - (A_UINT32)pCOMBCALGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_PLYBCKTONES, (A_UINT32)(((A_UINT32)&(pCOMBCALGROUPParms->plybckTones)) - (A_UINT32)pCOMBCALGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_CALTONELIST, (A_UINT32)(((A_UINT32)&(pCOMBCALGROUPParms->calToneList)) - (A_UINT32)pCOMBCALGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_INITCAL, (A_UINT32)(((A_UINT32)&(pCOMBCALGROUPParms->initCal)) - (A_UINT32)pCOMBCALGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_ITERATION, (A_UINT32)(((A_UINT32)&(pCOMBCALGROUPParms->iteration)) - (A_UINT32)pCOMBCALGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_CLLOC, (A_UINT32)(((A_UINT32)&(pCOMBCALGROUPParms->clLoc)) - (A_UINT32)pCOMBCALGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_ISRUNCALTX, (A_UINT32)(((A_UINT32)&(pCOMBCALGROUPParms->isRunCalTx)) - (A_UINT32)pCOMBCALGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_COMBINE, (A_UINT32)(((A_UINT32)&(pCOMBCALGROUPParms->combine)) - (A_UINT32)pCOMBCALGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_HOMECH, (A_UINT32)(((A_UINT32)&(pCOMBCALGROUPParms->homeCh)) - (A_UINT32)pCOMBCALGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_SYNTH, (A_UINT32)(((A_UINT32)&(pCOMBCALGROUPParms->synth)) - (A_UINT32)pCOMBCALGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_VERBOSE, (A_UINT32)(((A_UINT32)&(pCOMBCALGROUPParms->verbose)) - (A_UINT32)pCOMBCALGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_MAX_OFFSET, (A_UINT32)(((A_UINT32)&(pCOMBCALGROUPParms->max_offset)) - (A_UINT32)pCOMBCALGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_DBS, (A_UINT32)(((A_UINT32)&(pCOMBCALGROUPParms->DBS)) - (A_UINT32)pCOMBCALGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_IQITER, (A_UINT32)(((A_UINT32)&(pCOMBCALGROUPParms->iqIter)) - (A_UINT32)pCOMBCALGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_CLITER, (A_UINT32)(((A_UINT32)&(pCOMBCALGROUPParms->clIter)) - (A_UINT32)pCOMBCALGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_NUMTXGAINS, (A_UINT32)(((A_UINT32)&(pCOMBCALGROUPParms->numTxGains)) - (A_UINT32)pCOMBCALGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_DBSTEP, (A_UINT32)(((A_UINT32)&(pCOMBCALGROUPParms->dbStep)) - (A_UINT32)pCOMBCALGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_CALMODE, (A_UINT32)(((A_UINT32)&(pCOMBCALGROUPParms->calMode)) - (A_UINT32)pCOMBCALGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_HAVEPREVRESULTS, (A_UINT32)(((A_UINT32)&(pCOMBCALGROUPParms->havePrevResults)) - (A_UINT32)pCOMBCALGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_PREVRESULTS_MAGTX, (A_UINT32)(((A_UINT32)&(pCOMBCALGROUPParms->prevResults_magTx)) - (A_UINT32)pCOMBCALGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_PREVRESULTS_MAGRX, (A_UINT32)(((A_UINT32)&(pCOMBCALGROUPParms->prevResults_magRx)) - (A_UINT32)pCOMBCALGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_PREVRESULTS_PHASETX, (A_UINT32)(((A_UINT32)&(pCOMBCALGROUPParms->prevResults_phaseTx)) - (A_UINT32)pCOMBCALGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_PREVRESULTS_PHASERX, (A_UINT32)(((A_UINT32)&(pCOMBCALGROUPParms->prevResults_phaseRx)) - (A_UINT32)pCOMBCALGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_CLOFFSET, (A_UINT32)(((A_UINT32)&(pCOMBCALGROUPParms->clOffset)) - (A_UINT32)pCOMBCALGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_CLCORR_REAL, (A_UINT32)(((A_UINT32)&(pCOMBCALGROUPParms->clCorr_real)) - (A_UINT32)pCOMBCALGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_CLCORR_IMAG, (A_UINT32)(((A_UINT32)&(pCOMBCALGROUPParms->clCorr_imag)) - (A_UINT32)pCOMBCALGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_TXCORR_REAL, (A_UINT32)(((A_UINT32)&(pCOMBCALGROUPParms->txCorr_real)) - (A_UINT32)pCOMBCALGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_TXCORR_IMAG, (A_UINT32)(((A_UINT32)&(pCOMBCALGROUPParms->txCorr_imag)) - (A_UINT32)pCOMBCALGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_RXCORR_REAL, (A_UINT32)(((A_UINT32)&(pCOMBCALGROUPParms->rxCorr_real)) - (A_UINT32)pCOMBCALGROUPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_RXCORR_IMAG, (A_UINT32)(((A_UINT32)&(pCOMBCALGROUPParms->rxCorr_imag)) - (A_UINT32)pCOMBCALGROUPParms), pParmsOffset);
    return((void*) pCOMBCALGROUPParms);
}

static COMBCALGROUP_OP_FUNC COMBCALGROUPOpFunc = NULL;

TLV2_API void registerCOMBCALGROUPHandler(COMBCALGROUP_OP_FUNC fp)
{
    COMBCALGROUPOpFunc = fp;
}

A_BOOL COMBCALGROUPOp(void *pParms)
{
    CMD_COMBCALGROUP_PARMS *pCOMBCALGROUPParms = (CMD_COMBCALGROUP_PARMS *)pParms;

#if 0 //for debugging, comment out this line, and uncomment the line below
//#ifdef _DEBUG
    int i; 	//for initializing array parameter
    i = 0;	//assign a number to avoid warning in case i is not used

    A_PRINTF("COMBCALGROUPOp: cmdIdCombCalGroup %u\n", pCOMBCALGROUPParms->cmdIdCombCalGroup);
    A_PRINTF("COMBCALGROUPOp: phyId %u\n", pCOMBCALGROUPParms->phyId);
    A_PRINTF("COMBCALGROUPOp: bandCode %u\n", pCOMBCALGROUPParms->bandCode);
    A_PRINTF("COMBCALGROUPOp: bwCode %u\n", pCOMBCALGROUPParms->bwCode);
    A_PRINTF("COMBCALGROUPOp: freq %u\n", pCOMBCALGROUPParms->freq);
    A_PRINTF("COMBCALGROUPOp: dyn_bw %u\n", pCOMBCALGROUPParms->dyn_bw);
    A_PRINTF("COMBCALGROUPOp: chain %u\n", pCOMBCALGROUPParms->chain);
    A_PRINTF("COMBCALGROUPOp: num_plybck_tone %u\n", pCOMBCALGROUPParms->num_plybck_tone);
    A_PRINTF("COMBCALGROUPOp: lenPlybckTones %u\n", pCOMBCALGROUPParms->lenPlybckTones);
    A_PRINTF("COMBCALGROUPOp: plybckToneIdx %u\n", pCOMBCALGROUPParms->plybckToneIdx);
    A_PRINTF("COMBCALGROUPOp: playBackLen %u\n", pCOMBCALGROUPParms->playBackLen);
    for (i = 0; i < 8 ; i++) // can be modified to print up to 16 entries
    {
        A_PRINTF("COMBCALGROUPOp: plybckTones %u\n", pCOMBCALGROUPParms->plybckTones[i]);
    }
    for (i = 0; i < 8 ; i++) // can be modified to print up to 16 entries
    {
        A_PRINTF("COMBCALGROUPOp: calToneList 0x%x\n", pCOMBCALGROUPParms->calToneList[i]);
    }
    A_PRINTF("COMBCALGROUPOp: initCal %u\n", pCOMBCALGROUPParms->initCal);
    A_PRINTF("COMBCALGROUPOp: iteration %u\n", pCOMBCALGROUPParms->iteration);
    A_PRINTF("COMBCALGROUPOp: clLoc %u\n", pCOMBCALGROUPParms->clLoc);
    A_PRINTF("COMBCALGROUPOp: isRunCalTx %u\n", pCOMBCALGROUPParms->isRunCalTx);
    A_PRINTF("COMBCALGROUPOp: combine %u\n", pCOMBCALGROUPParms->combine);
    A_PRINTF("COMBCALGROUPOp: homeCh %u\n", pCOMBCALGROUPParms->homeCh);
    A_PRINTF("COMBCALGROUPOp: synth %u\n", pCOMBCALGROUPParms->synth);
    A_PRINTF("COMBCALGROUPOp: verbose %u\n", pCOMBCALGROUPParms->verbose);
    A_PRINTF("COMBCALGROUPOp: max_offset %u\n", pCOMBCALGROUPParms->max_offset);
    A_PRINTF("COMBCALGROUPOp: DBS %u\n", pCOMBCALGROUPParms->DBS);
    A_PRINTF("COMBCALGROUPOp: iqIter %u\n", pCOMBCALGROUPParms->iqIter);
    A_PRINTF("COMBCALGROUPOp: clIter %u\n", pCOMBCALGROUPParms->clIter);
    A_PRINTF("COMBCALGROUPOp: numTxGains %u\n", pCOMBCALGROUPParms->numTxGains);
    A_PRINTF("COMBCALGROUPOp: dbStep %u\n", pCOMBCALGROUPParms->dbStep);
    A_PRINTF("COMBCALGROUPOp: calMode %u\n", pCOMBCALGROUPParms->calMode);
    A_PRINTF("COMBCALGROUPOp: havePrevResults %u\n", pCOMBCALGROUPParms->havePrevResults);
    for (i = 0; i < 8 ; i++) // can be modified to print up to 16 entries
    {
        A_PRINTF("COMBCALGROUPOp: prevResults_magTx 0x%x\n", pCOMBCALGROUPParms->prevResults_magTx[i]);
    }
    for (i = 0; i < 8 ; i++) // can be modified to print up to 16 entries
    {
        A_PRINTF("COMBCALGROUPOp: prevResults_magRx 0x%x\n", pCOMBCALGROUPParms->prevResults_magRx[i]);
    }
    for (i = 0; i < 8 ; i++) // can be modified to print up to 16 entries
    {
        A_PRINTF("COMBCALGROUPOp: prevResults_phaseTx 0x%x\n", pCOMBCALGROUPParms->prevResults_phaseTx[i]);
    }
    for (i = 0; i < 8 ; i++) // can be modified to print up to 16 entries
    {
        A_PRINTF("COMBCALGROUPOp: prevResults_phaseRx 0x%x\n", pCOMBCALGROUPParms->prevResults_phaseRx[i]);
    }
    for (i = 0; i < 8 ; i++) // can be modified to print up to 16 entries
    {
        A_PRINTF("COMBCALGROUPOp: clOffset %u\n", pCOMBCALGROUPParms->clOffset[i]);
    }
    for (i = 0; i < 8 ; i++) // can be modified to print up to 32 entries
    {
        A_PRINTF("COMBCALGROUPOp: clCorr_real 0x%x\n", pCOMBCALGROUPParms->clCorr_real[i]);
    }
    for (i = 0; i < 8 ; i++) // can be modified to print up to 32 entries
    {
        A_PRINTF("COMBCALGROUPOp: clCorr_imag 0x%x\n", pCOMBCALGROUPParms->clCorr_imag[i]);
    }
    for (i = 0; i < 8 ; i++) // can be modified to print up to 112 entries
    {
        A_PRINTF("COMBCALGROUPOp: txCorr_real 0x%x\n", pCOMBCALGROUPParms->txCorr_real[i]);
    }
    for (i = 0; i < 8 ; i++) // can be modified to print up to 112 entries
    {
        A_PRINTF("COMBCALGROUPOp: txCorr_imag 0x%x\n", pCOMBCALGROUPParms->txCorr_imag[i]);
    }
    for (i = 0; i < 8 ; i++) // can be modified to print up to 112 entries
    {
        A_PRINTF("COMBCALGROUPOp: rxCorr_real 0x%x\n", pCOMBCALGROUPParms->rxCorr_real[i]);
    }
    for (i = 0; i < 8 ; i++) // can be modified to print up to 112 entries
    {
        A_PRINTF("COMBCALGROUPOp: rxCorr_imag 0x%x\n", pCOMBCALGROUPParms->rxCorr_imag[i]);
    }
#endif //_DEBUG

    if (NULL != COMBCALGROUPOpFunc) {
        (*COMBCALGROUPOpFunc)(pCOMBCALGROUPParms);
    }
    return(TRUE);
}

void* initCOMBCALGROUPRSPOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict)
{
    int i, j; 	//for initializing array parameter
    CMD_COMBCALGROUPRSP_PARMS  *pCOMBCALGROUPRSPParms = (CMD_COMBCALGROUPRSP_PARMS *)pParmsCommon;

    if (pParmsCommon == NULL) return (NULL);

    i = j = 0;	//assign a number to avoid warning in case i and j are not used

    // Populate the parm structure with initial values
    pCOMBCALGROUPRSPParms->phyId = pParmDict[PARM_PHYID].v.valU8;
    pCOMBCALGROUPRSPParms->status = pParmDict[PARM_STATUS].v.valU8;
    memset(pCOMBCALGROUPRSPParms->pad2, 0, sizeof(pCOMBCALGROUPRSPParms->pad2));
    memset(pCOMBCALGROUPRSPParms->clCorr_real, 0, sizeof(pCOMBCALGROUPRSPParms->clCorr_real));
    memset(pCOMBCALGROUPRSPParms->clCorr_imag, 0, sizeof(pCOMBCALGROUPRSPParms->clCorr_imag));
    memset(pCOMBCALGROUPRSPParms->txCorr_real, 0, sizeof(pCOMBCALGROUPRSPParms->txCorr_real));
    memset(pCOMBCALGROUPRSPParms->txCorr_imag, 0, sizeof(pCOMBCALGROUPRSPParms->txCorr_imag));
    memset(pCOMBCALGROUPRSPParms->rxCorr_real, 0, sizeof(pCOMBCALGROUPRSPParms->rxCorr_real));
    memset(pCOMBCALGROUPRSPParms->rxCorr_imag, 0, sizeof(pCOMBCALGROUPRSPParms->rxCorr_imag));
    memset(pCOMBCALGROUPRSPParms->trxGainPair_txGainIdx, 0, sizeof(pCOMBCALGROUPRSPParms->trxGainPair_txGainIdx));
    memset(pCOMBCALGROUPRSPParms->trxGainPair_txDacGain, 0, sizeof(pCOMBCALGROUPRSPParms->trxGainPair_txDacGain));
    memset(pCOMBCALGROUPRSPParms->trxGainPair_rxGainIdx, 0, sizeof(pCOMBCALGROUPRSPParms->trxGainPair_rxGainIdx));
    memset(pCOMBCALGROUPRSPParms->prevResults_magTx, 0, sizeof(pCOMBCALGROUPRSPParms->prevResults_magTx));
    memset(pCOMBCALGROUPRSPParms->prevResults_magRx, 0, sizeof(pCOMBCALGROUPRSPParms->prevResults_magRx));
    memset(pCOMBCALGROUPRSPParms->prevResults_phaseTx, 0, sizeof(pCOMBCALGROUPRSPParms->prevResults_phaseTx));
    memset(pCOMBCALGROUPRSPParms->prevResults_phaseRx, 0, sizeof(pCOMBCALGROUPRSPParms->prevResults_phaseRx));
    pCOMBCALGROUPRSPParms->executionTime = pParmDict[PARM_EXECUTIONTIME].v.valU32;

    // Make up ParmOffsetTbl
    resetParmOffsetFields();
    fillParmOffsetTbl((A_UINT32)PARM_PHYID, (A_UINT32)(((A_UINT32)&(pCOMBCALGROUPRSPParms->phyId)) - (A_UINT32)pCOMBCALGROUPRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_STATUS, (A_UINT32)(((A_UINT32)&(pCOMBCALGROUPRSPParms->status)) - (A_UINT32)pCOMBCALGROUPRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_PAD2, (A_UINT32)(((A_UINT32)&(pCOMBCALGROUPRSPParms->pad2)) - (A_UINT32)pCOMBCALGROUPRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_CLCORR_REAL, (A_UINT32)(((A_UINT32)&(pCOMBCALGROUPRSPParms->clCorr_real)) - (A_UINT32)pCOMBCALGROUPRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_CLCORR_IMAG, (A_UINT32)(((A_UINT32)&(pCOMBCALGROUPRSPParms->clCorr_imag)) - (A_UINT32)pCOMBCALGROUPRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_TXCORR_REAL, (A_UINT32)(((A_UINT32)&(pCOMBCALGROUPRSPParms->txCorr_real)) - (A_UINT32)pCOMBCALGROUPRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_TXCORR_IMAG, (A_UINT32)(((A_UINT32)&(pCOMBCALGROUPRSPParms->txCorr_imag)) - (A_UINT32)pCOMBCALGROUPRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_RXCORR_REAL, (A_UINT32)(((A_UINT32)&(pCOMBCALGROUPRSPParms->rxCorr_real)) - (A_UINT32)pCOMBCALGROUPRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_RXCORR_IMAG, (A_UINT32)(((A_UINT32)&(pCOMBCALGROUPRSPParms->rxCorr_imag)) - (A_UINT32)pCOMBCALGROUPRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_TRXGAINPAIR_TXGAINIDX, (A_UINT32)(((A_UINT32)&(pCOMBCALGROUPRSPParms->trxGainPair_txGainIdx)) - (A_UINT32)pCOMBCALGROUPRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_TRXGAINPAIR_TXDACGAIN, (A_UINT32)(((A_UINT32)&(pCOMBCALGROUPRSPParms->trxGainPair_txDacGain)) - (A_UINT32)pCOMBCALGROUPRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_TRXGAINPAIR_RXGAINIDX, (A_UINT32)(((A_UINT32)&(pCOMBCALGROUPRSPParms->trxGainPair_rxGainIdx)) - (A_UINT32)pCOMBCALGROUPRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_PREVRESULTS_MAGTX, (A_UINT32)(((A_UINT32)&(pCOMBCALGROUPRSPParms->prevResults_magTx)) - (A_UINT32)pCOMBCALGROUPRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_PREVRESULTS_MAGRX, (A_UINT32)(((A_UINT32)&(pCOMBCALGROUPRSPParms->prevResults_magRx)) - (A_UINT32)pCOMBCALGROUPRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_PREVRESULTS_PHASETX, (A_UINT32)(((A_UINT32)&(pCOMBCALGROUPRSPParms->prevResults_phaseTx)) - (A_UINT32)pCOMBCALGROUPRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_PREVRESULTS_PHASERX, (A_UINT32)(((A_UINT32)&(pCOMBCALGROUPRSPParms->prevResults_phaseRx)) - (A_UINT32)pCOMBCALGROUPRSPParms), pParmsOffset);
    fillParmOffsetTbl((A_UINT32)PARM_EXECUTIONTIME, (A_UINT32)(((A_UINT32)&(pCOMBCALGROUPRSPParms->executionTime)) - (A_UINT32)pCOMBCALGROUPRSPParms), pParmsOffset);
    return((void*) pCOMBCALGROUPRSPParms);
}

static COMBCALGROUPRSP_OP_FUNC COMBCALGROUPRSPOpFunc = NULL;

TLV2_API void registerCOMBCALGROUPRSPHandler(COMBCALGROUPRSP_OP_FUNC fp)
{
    COMBCALGROUPRSPOpFunc = fp;
}

A_BOOL COMBCALGROUPRSPOp(void *pParms)
{
    CMD_COMBCALGROUPRSP_PARMS *pCOMBCALGROUPRSPParms = (CMD_COMBCALGROUPRSP_PARMS *)pParms;

#if 0 //for debugging, comment out this line, and uncomment the line below
//#ifdef _DEBUG
    int i; 	//for initializing array parameter
    i = 0;	//assign a number to avoid warning in case i is not used

    A_PRINTF("COMBCALGROUPRSPOp: phyId %u\n", pCOMBCALGROUPRSPParms->phyId);
    A_PRINTF("COMBCALGROUPRSPOp: status %u\n", pCOMBCALGROUPRSPParms->status);
    for (i = 0; i < 2 ; i++)
    {
        A_PRINTF("COMBCALGROUPRSPOp: pad2 %u\n", pCOMBCALGROUPRSPParms->pad2[i]);
    }
    for (i = 0; i < 8 ; i++) // can be modified to print up to 32 entries
    {
        A_PRINTF("COMBCALGROUPRSPOp: clCorr_real 0x%x\n", pCOMBCALGROUPRSPParms->clCorr_real[i]);
    }
    for (i = 0; i < 8 ; i++) // can be modified to print up to 32 entries
    {
        A_PRINTF("COMBCALGROUPRSPOp: clCorr_imag 0x%x\n", pCOMBCALGROUPRSPParms->clCorr_imag[i]);
    }
    for (i = 0; i < 8 ; i++) // can be modified to print up to 112 entries
    {
        A_PRINTF("COMBCALGROUPRSPOp: txCorr_real 0x%x\n", pCOMBCALGROUPRSPParms->txCorr_real[i]);
    }
    for (i = 0; i < 8 ; i++) // can be modified to print up to 112 entries
    {
        A_PRINTF("COMBCALGROUPRSPOp: txCorr_imag 0x%x\n", pCOMBCALGROUPRSPParms->txCorr_imag[i]);
    }
    for (i = 0; i < 8 ; i++) // can be modified to print up to 112 entries
    {
        A_PRINTF("COMBCALGROUPRSPOp: rxCorr_real 0x%x\n", pCOMBCALGROUPRSPParms->rxCorr_real[i]);
    }
    for (i = 0; i < 8 ; i++) // can be modified to print up to 112 entries
    {
        A_PRINTF("COMBCALGROUPRSPOp: rxCorr_imag 0x%x\n", pCOMBCALGROUPRSPParms->rxCorr_imag[i]);
    }
    for (i = 0; i < 8 ; i++) // can be modified to print up to 16 entries
    {
        A_PRINTF("COMBCALGROUPRSPOp: trxGainPair_txGainIdx 0x%x\n", pCOMBCALGROUPRSPParms->trxGainPair_txGainIdx[i]);
    }
    for (i = 0; i < 8 ; i++) // can be modified to print up to 16 entries
    {
        A_PRINTF("COMBCALGROUPRSPOp: trxGainPair_txDacGain 0x%x\n", pCOMBCALGROUPRSPParms->trxGainPair_txDacGain[i]);
    }
    for (i = 0; i < 8 ; i++) // can be modified to print up to 16 entries
    {
        A_PRINTF("COMBCALGROUPRSPOp: trxGainPair_rxGainIdx 0x%x\n", pCOMBCALGROUPRSPParms->trxGainPair_rxGainIdx[i]);
    }
    for (i = 0; i < 8 ; i++) // can be modified to print up to 16 entries
    {
        A_PRINTF("COMBCALGROUPRSPOp: prevResults_magTx 0x%x\n", pCOMBCALGROUPRSPParms->prevResults_magTx[i]);
    }
    for (i = 0; i < 8 ; i++) // can be modified to print up to 16 entries
    {
        A_PRINTF("COMBCALGROUPRSPOp: prevResults_magRx 0x%x\n", pCOMBCALGROUPRSPParms->prevResults_magRx[i]);
    }
    for (i = 0; i < 8 ; i++) // can be modified to print up to 16 entries
    {
        A_PRINTF("COMBCALGROUPRSPOp: prevResults_phaseTx 0x%x\n", pCOMBCALGROUPRSPParms->prevResults_phaseTx[i]);
    }
    for (i = 0; i < 8 ; i++) // can be modified to print up to 16 entries
    {
        A_PRINTF("COMBCALGROUPRSPOp: prevResults_phaseRx 0x%x\n", pCOMBCALGROUPRSPParms->prevResults_phaseRx[i]);
    }
    A_PRINTF("COMBCALGROUPRSPOp: executionTime %u\n", pCOMBCALGROUPRSPParms->executionTime);
#endif //_DEBUG

    if (NULL != COMBCALGROUPRSPOpFunc) {
        (*COMBCALGROUPRSPOpFunc)(pCOMBCALGROUPRSPParms);
    }
    return(TRUE);
}
