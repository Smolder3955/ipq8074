/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

// This is an auto-generated file from input\cmdCombCalGroupHandler.s
#ifndef _CMDCOMBCALGROUPHANDLER_H_
#define _CMDCOMBCALGROUPHANDLER_H_

#if defined(__cplusplus) || defined(__cplusplus__)
extern "C" {
#endif

#if defined(WIN32) || defined(WIN64)
#pragma pack (push, 1)
#endif //WIN32 || WIN64

#ifndef CALDB_RX_IQ_NUM_TAPS
#define CALDB_RX_IQ_NUM_TAPS            9
#endif
#ifndef CALDB_RX_IQ_NUM_GAINS
#define CALDB_RX_IQ_NUM_GAINS          16
#endif
#ifndef CALDB_TX_IQ_NUM_TAPS
#define CALDB_TX_IQ_NUM_TAPS            7
#endif
#ifndef CALDB_TX_IQ_NUM_GAINS
#define CALDB_TX_IQ_NUM_GAINS          16
#endif
#ifndef CALDB_TXLO_NORMAL_NUM_TAPS
#define CALDB_TXLO_NORMAL_NUM_TAPS      1
#endif
#ifndef CALDB_TXLO_NORMAL_NUM_GAINS
#define CALDB_TXLO_NORMAL_NUM_GAINS    64
#endif
#ifndef CALDB_MAX_CAL_GAINS
#define CALDB_MAX_CAL_GAINS    16
#endif

typedef struct combcalgroup_parms {
    A_UINT8	cmdIdCombCalGroup;
    A_UINT8	phyId;
    A_UINT8	bandCode;
    A_UINT8	bwCode;
    A_UINT16	freq;
    A_UINT8	dyn_bw;
    A_UINT8	chain;
    A_UINT8	num_plybck_tone;
    A_UINT8	lenPlybckTones;
    A_UINT8	plybckToneIdx;
    A_UINT8	playBackLen;
    A_UINT8	plybckTones[CALDB_MAX_CAL_GAINS];
    A_INT16	calToneList[16];
    A_UINT8	initCal;
    A_UINT8	iteration;
    A_UINT8	clLoc;
    A_UINT8	isRunCalTx;
    A_UINT8	combine;
    A_UINT8	homeCh;
    A_UINT8	synth;
    A_UINT8	verbose;
    A_UINT8	max_offset;
    A_UINT8	DBS;
    A_UINT8	iqIter;
    A_UINT8	clIter;
    A_UINT8	numTxGains;
    A_UINT8	dbStep;
    A_UINT8	calMode;
    A_UINT8	havePrevResults;
    A_INT32	prevResults_magTx[CALDB_MAX_CAL_GAINS];
    A_INT32	prevResults_magRx[CALDB_MAX_CAL_GAINS];
    A_INT32	prevResults_phaseTx[CALDB_MAX_CAL_GAINS];
    A_INT32	prevResults_phaseRx[CALDB_MAX_CAL_GAINS];
    A_UINT32	clOffset[CALDB_MAX_CAL_GAINS];
    A_INT16	clCorr_real[CALDB_TXLO_NORMAL_NUM_GAINS*CALDB_TXLO_NORMAL_NUM_TAPS];
    A_INT16	clCorr_imag[CALDB_TXLO_NORMAL_NUM_GAINS*CALDB_TXLO_NORMAL_NUM_TAPS];
    A_INT16	txCorr_real[CALDB_TX_IQ_NUM_GAINS*CALDB_TX_IQ_NUM_TAPS];
    A_INT16	txCorr_imag[CALDB_TX_IQ_NUM_GAINS*CALDB_TX_IQ_NUM_TAPS];
    A_INT16	rxCorr_real[CALDB_RX_IQ_NUM_GAINS*CALDB_RX_IQ_NUM_TAPS];
    A_INT16	rxCorr_imag[CALDB_RX_IQ_NUM_GAINS*CALDB_RX_IQ_NUM_TAPS];
} __ATTRIB_PACK CMD_COMBCALGROUP_PARMS;

typedef struct combcalgrouprsp_parms {
    A_UINT8	phyId;
    A_UINT8	status;
    A_UINT8	pad2[2];
    A_INT16	clCorr_real[CALDB_TXLO_NORMAL_NUM_GAINS*CALDB_TXLO_NORMAL_NUM_TAPS];
    A_INT16	clCorr_imag[CALDB_TXLO_NORMAL_NUM_GAINS*CALDB_TXLO_NORMAL_NUM_TAPS];
    A_INT16	txCorr_real[CALDB_TX_IQ_NUM_GAINS*CALDB_TX_IQ_NUM_TAPS];
    A_INT16	txCorr_imag[CALDB_TX_IQ_NUM_GAINS*CALDB_TX_IQ_NUM_TAPS];
    A_INT16	rxCorr_real[CALDB_RX_IQ_NUM_GAINS*CALDB_RX_IQ_NUM_TAPS];
    A_INT16	rxCorr_imag[CALDB_RX_IQ_NUM_GAINS*CALDB_RX_IQ_NUM_TAPS];
    A_INT32	trxGainPair_txGainIdx[CALDB_MAX_CAL_GAINS];
    A_INT32	trxGainPair_txDacGain[CALDB_MAX_CAL_GAINS];
    A_INT32	trxGainPair_rxGainIdx[CALDB_MAX_CAL_GAINS];
    A_INT32	prevResults_magTx[CALDB_MAX_CAL_GAINS];
    A_INT32	prevResults_magRx[CALDB_MAX_CAL_GAINS];
    A_INT32	prevResults_phaseTx[CALDB_MAX_CAL_GAINS];
    A_INT32	prevResults_phaseRx[CALDB_MAX_CAL_GAINS];
    A_UINT32	executionTime;
} __ATTRIB_PACK CMD_COMBCALGROUPRSP_PARMS;

typedef void (*COMBCALGROUP_OP_FUNC)(void *pParms);
typedef void (*COMBCALGROUPRSP_OP_FUNC)(void *pParms);

// Exposed functions

void* initCOMBCALGROUPOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict);
A_BOOL COMBCALGROUPOp(void *pParms);

void* initCOMBCALGROUPRSPOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict);
A_BOOL COMBCALGROUPRSPOp(void *pParms);

#if defined(WIN32) || defined(WIN64)
#pragma pack(pop)
#endif //WIN32 || WIN64


#if defined(__cplusplus) || defined(__cplusplus__)
}
#endif

#endif //_CMDCOMBCALGROUPHANDLER_H_
