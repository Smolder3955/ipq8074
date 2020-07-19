/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

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

# cmd
CMD= CombCalGroup

# cmd parm
PARM_START:
UINT8:cmdIdCombCalGroup:1:u:0
UINT8:phyId:1:u:0
UINT8:bandCode:1:u:0
UINT8:bwCode:1:u:0

UINT16:freq:1:u:5180
UINT8:dyn_bw:1:u:1
UINT8:chain:1:u:0

UINT8:num_plybck_tone:1:u:4
UINT8:lenPlybckTones:1:u:0
UINT8:plybckToneIdx:1:u:0
UINT8:playBackLen:1:u:0

UINT8:plybckTones:CALDB_MAX_CAL_GAINS:16:u
INT16:calToneList:16:x

UINT8:initCal:1:u:1
UINT8:iteration:1:u:4
UINT8:clLoc:1:u:0
UINT8:isRunCalTx:1:u:1

UINT8:combine:1:u:1
UINT8:homeCh:1:u:0
UINT8:synth:1:u:0
UINT8:verbose:1:u:0

UINT8:max_offset:1:u:0
UINT8:DBS:1:u:0
UINT8:iqIter:1:u:0
UINT8:clIter:1:u:0

UINT8:numTxGains:1:u:0
UINT8:dbStep:1:u:0
UINT8:calMode:1:u:0
UINT8:havePrevResults:1:u:0

INT32:prevResults_magTx:CALDB_MAX_CAL_GAINS:16:x
INT32:prevResults_magRx:CALDB_MAX_CAL_GAINS:16:x
INT32:prevResults_phaseTx:CALDB_MAX_CAL_GAINS:16:x
INT32:prevResults_phaseRx:CALDB_MAX_CAL_GAINS:16:x

UINT32:clOffset:CALDB_MAX_CAL_GAINS:16:u

INT16:clCorr_real:CALDB_TXLO_NORMAL_NUM_GAINS*CALDB_TXLO_NORMAL_NUM_TAPS:32:x
INT16:clCorr_imag:CALDB_TXLO_NORMAL_NUM_GAINS*CALDB_TXLO_NORMAL_NUM_TAPS:32:x
INT16:txCorr_real:CALDB_TX_IQ_NUM_GAINS*CALDB_TX_IQ_NUM_TAPS:112:x
INT16:txCorr_imag:CALDB_TX_IQ_NUM_GAINS*CALDB_TX_IQ_NUM_TAPS:112:x
INT16:rxCorr_real:CALDB_RX_IQ_NUM_GAINS*CALDB_RX_IQ_NUM_TAPS:112:x
INT16:rxCorr_imag:CALDB_RX_IQ_NUM_GAINS*CALDB_RX_IQ_NUM_TAPS:112:x

PARM_END:


# rsp
RSP= CombCalGroupRsp
# rsp param
PARM_START:
UINT8:phyId:1:u:0
UINT8:status:1:u:0
UINT8:pad2:2:u

INT16:clCorr_real:CALDB_TXLO_NORMAL_NUM_GAINS*CALDB_TXLO_NORMAL_NUM_TAPS:32:x
INT16:clCorr_imag:CALDB_TXLO_NORMAL_NUM_GAINS*CALDB_TXLO_NORMAL_NUM_TAPS:32:x
INT16:txCorr_real:CALDB_TX_IQ_NUM_GAINS*CALDB_TX_IQ_NUM_TAPS:112:x
INT16:txCorr_imag:CALDB_TX_IQ_NUM_GAINS*CALDB_TX_IQ_NUM_TAPS:112:x
INT16:rxCorr_real:CALDB_RX_IQ_NUM_GAINS*CALDB_RX_IQ_NUM_TAPS:112:x
INT16:rxCorr_imag:CALDB_RX_IQ_NUM_GAINS*CALDB_RX_IQ_NUM_TAPS:112:x

INT32:trxGainPair_txGainIdx:CALDB_MAX_CAL_GAINS:16:x
INT32:trxGainPair_txDacGain:CALDB_MAX_CAL_GAINS:16:x
INT32:trxGainPair_rxGainIdx:CALDB_MAX_CAL_GAINS:16:x

INT32:prevResults_magTx:CALDB_MAX_CAL_GAINS:16:x
INT32:prevResults_magRx:CALDB_MAX_CAL_GAINS:16:x
INT32:prevResults_phaseTx:CALDB_MAX_CAL_GAINS:16:x
INT32:prevResults_phaseRx:CALDB_MAX_CAL_GAINS:16:x

UINT32:executionTime:1:u:0

PARM_END: