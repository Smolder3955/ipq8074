/*
 * Copyright (c) 2011,2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * Notifications and licenses are retained for attribution purposes only.
 */
/*
 * Copyright (c) 2002-2005 Sam Leffler, Errno Consulting
 * Copyright (c) 2002-2005 Atheros Communications, Inc.
 * Copyright (c) 2008-2010, Atheros Communications Inc. 
 * 
 * Redistribution and use in source and binary forms are permitted
 * provided that the following conditions are met:
 * 1. The materials contained herein are unmodified and are used
 *    unmodified.
 * 2. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following NO
 *    ''WARRANTY'' disclaimer below (''Disclaimer''), without
 *    modification.
 * 3. Redistributions in binary form must reproduce at minimum a
 *    disclaimer similar to the Disclaimer below and any redistribution
 *    must be conditioned upon including a substantially similar
 *    Disclaimer requirement for further binary redistribution.
 * 4. Neither the names of the above-listed copyright holders nor the
 *    names of any contributors may be used to endorse or promote
 *    product derived from this software without specific prior written
 *    permission.
 * 
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ''AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT,
 * MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE
 * FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGES.
 * 
 */

#ifndef _ATH_AR5416_H_
#define _ATH_AR5416_H_

#include "ah_internal.h"
#include "ah_eeprom.h"
#include "ah_devid.h"
#include "ar5416eep.h"  /* For Eeprom definitions */
#include "opt_ah.h"

#define AR5416_MAGIC            0x19641014

#if defined(AH_SUPPORT_HOWL)
/*
 * The code is compiled to support Howl.
 * Check the stored silicon rev to see if the HW is a Howl chip.
 */
#define AR_SREV_HOWL(ah) \
    ((AH_PRIVATE(ah)->ah_mac_version) == AR_SREV_VERSION_HOWL)
#else
/*
 * The code is not compiled to support Howl.
 * To reduce code size, don't even check the stored silicon revision.
 */
#define AR_SREV_HOWL(ah) 0
#endif

/* DCU Transmit Filter macros */
#define CALC_MMR(dcu, idx) \
    ( (4 * dcu) + (idx < 32 ? 0 : (idx < 64 ? 1 : (idx < 96 ? 2 : 3))) )
#define TXBLK_FROM_MMR(mmr) \
    (AR_D_TXBLK_BASE + ((mmr & 0x1f) << 6) + ((mmr & 0x20) >> 3))
#define CALC_TXBLK_ADDR(dcu, idx)   (TXBLK_FROM_MMR(CALC_MMR(dcu, idx)))
#define CALC_TXBLK_VALUE(idx)       (1 << (idx & 0x1f))

/* MAC register values */

#define INIT_BEACON_CONTROL \
    ((INIT_RESET_TSF << 24)  | (INIT_BEACON_EN << 23) | \
      (INIT_TIM_OFFSET << 16) | INIT_BEACON_PERIOD)

#define INIT_CONFIG_STATUS  0x00000000
#define INIT_RSSI_THR       0x00000700  /* Missed beacon counter initialized to 0x7 (max is 0xff) */
#define INIT_BCON_CNTRL_REG 0x00000000

/*
 * Various fifo fill before Tx start, in 64-byte units
 * i.e. put the frame in the air while still DMAing
 */
#define MIN_TX_FIFO_THRESHOLD           0x1
#define MAX_TX_FIFO_THRESHOLD_DEFAULT   (( 4096 / 64) - 1)
#define MAX_TX_FIFO_THRESHOLD_KITE      (( 2048 / 64) - 1)
#define INIT_TX_FIFO_THRESHOLD          MIN_TX_FIFO_THRESHOLD

#define IS_SPUR_CHAN(_chan) \
    ( (((_chan)->channel % 32) != 0) && \
        ((((_chan)->channel % 32) < 10) || (((_chan)->channel % 32) > 22)) )

/*
 * Gain support.
 */
#define NUM_CORNER_FIX_BITS_2133    7
#define CCK_OFDM_GAIN_DELTA         15
#define MERLIN_TX_GAIN_TABLE_SIZE   22

enum GAIN_PARAMS {
    GP_TXCLIP,
    GP_PD90,
    GP_PD84,
    GP_GSEL
};

enum GAIN_PARAMS_2133 {
    GP_MIXGAIN_OVR,
    GP_PWD_138,
    GP_PWD_137,
    GP_PWD_136,
    GP_PWD_132,
    GP_PWD_131,
    GP_PWD_130,
};

enum {
    HAL_RESET_POWER_ON,
    HAL_RESET_WARM,
    HAL_RESET_COLD,
};

typedef struct _gainOptStep {
    int16_t paramVal[NUM_CORNER_FIX_BITS_2133];
    int32_t stepGain;
    int8_t  stepName[16];
} GAIN_OPTIMIZATION_STEP;

typedef struct {
    u_int32_t   numStepsInLadder;
    u_int32_t   defaultStepNum;
    GAIN_OPTIMIZATION_STEP optStep[10];
} GAIN_OPTIMIZATION_LADDER;

typedef struct {
    u_int32_t   currStepNum;
    u_int32_t   currGain;
    u_int32_t   targetGain;
    u_int32_t   loTrig;
    u_int32_t   hiTrig;
    u_int32_t   gainFCorrection;
    u_int32_t   active;
    const GAIN_OPTIMIZATION_STEP *currStep;
} GAIN_VALUES;

typedef struct {
    u_int16_t   synth_center;
    u_int16_t   ctl_center;
    u_int16_t   ext_center;
} CHAN_CENTERS;

/* RF HAL structures */
typedef struct RfHalFuncs {
    void      (*rfDetach)(struct ath_hal *ah);
    void      (*writeRegs)(struct ath_hal *,
            u_int modeIndex, u_int freqIndex, int regWrites);
    HAL_BOOL  (*setChannel)(struct ath_hal *, HAL_CHANNEL_INTERNAL *);
    HAL_BOOL  (*setRfRegs)(struct ath_hal *,
              HAL_CHANNEL_INTERNAL *, u_int16_t modesIndex);
    HAL_BOOL  (*getChipPowerLim)(struct ath_hal *ah, HAL_CHANNEL *chans,
                       u_int32_t nchancs);
    void      (*decreaseChainPower)(struct ath_hal *ah, HAL_CHANNEL *chans);
} RF_HAL_FUNCS;

/*
 * Per-channel ANI state private to the driver.
 */
struct ar5416AniState {
    HAL_CHANNEL c;
    u_int8_t    noiseImmunityLevel;
    u_int8_t    spurImmunityLevel;
    u_int8_t    firstepLevel;
    u_int8_t    ofdmWeakSigDetectOff;
    u_int8_t    cckWeakSigThreshold;

    /* Thresholds */
    u_int32_t   listenTime;
    u_int32_t   ofdmTrigHigh;
    u_int32_t   ofdmTrigLow;
    int32_t     cckTrigHigh;
    int32_t     cckTrigLow;
    int32_t     rssiThrLow;
    int32_t     rssiThrHigh;

    u_int32_t   noiseFloor; /* The current noise floor */
    u_int32_t   txFrameCount;   /* Last txFrameCount */
    u_int32_t   rxFrameCount;   /* Last rx Frame count */
    u_int32_t   cycleCount; /* Last cycleCount (can detect wrap-around) */
    u_int32_t   ofdmPhyErrCount;/* OFDM err count since last reset */
    u_int32_t   cckPhyErrCount; /* CCK err count since last reset */
    u_int32_t   ofdmPhyErrBase; /* Base value for ofdm err counter */
    u_int32_t   cckPhyErrBase;  /* Base value for cck err counters */
    int16_t     pktRssi[2]; /* Average rssi of pkts for 2 antennas */
    int16_t     ofdmErrRssi[2]; /* Average rssi of ofdm phy errs for 2 ant */
    int16_t     cckErrRssi[2];  /* Average rssi of cck phy errs for 2 ant */
    HAL_BOOL    phyNoiseSpur; /* based on OFDM/CCK Phy errors */
};

#define AR5416_ANI_POLLINTERVAL    100    /* 100 milliseconds between ANI poll */

#define AR5416_CHANNEL_SWITCH_TIME_USEC  3000 /* 3 milliseconds to change channels */

#define HAL_PROCESS_ANI     0x00000001  /* ANI state setup */
#define HAL_RADAR_EN        0x80000000  /* Radar detect is capable */
#define HAL_AR_EN       0x40000000  /* AR detect is capable */

#define DO_ANI(ah) \
    ((AH5416(ah)->ah_procPhyErr & HAL_PROCESS_ANI))

struct ar5416Stats {
    u_int32_t   ast_ani_niup;   /* ANI increased noise immunity */
    u_int32_t   ast_ani_nidown; /* ANI decreased noise immunity */
    u_int32_t   ast_ani_spurup; /* ANI increased spur immunity */
    u_int32_t   ast_ani_spurdown;/* ANI descreased spur immunity */
    u_int32_t   ast_ani_ofdmon; /* ANI OFDM weak signal detect on */
    u_int32_t   ast_ani_ofdmoff;/* ANI OFDM weak signal detect off */
    u_int32_t   ast_ani_cckhigh;/* ANI CCK weak signal threshold high */
    u_int32_t   ast_ani_ccklow; /* ANI CCK weak signal threshold low */
    u_int32_t   ast_ani_stepup; /* ANI increased first step level */
    u_int32_t   ast_ani_stepdown;/* ANI decreased first step level */
    u_int32_t   ast_ani_ofdmerrs;/* ANI cumulative ofdm phy err count */
    u_int32_t   ast_ani_cckerrs;/* ANI cumulative cck phy err count */
    u_int32_t   ast_ani_reset;  /* ANI parameters zero'd for non-STA */
    u_int32_t   ast_ani_lzero;  /* ANI listen time forced to zero */
    u_int32_t   ast_ani_lneg;   /* ANI listen time calculated < 0 */
    HAL_MIB_STATS   ast_mibstats;   /* MIB counter stats */
    HAL_NODE_STATS  ast_nodestats;  /* Latest rssi stats from driver */
};

struct ar5416RadReader {
    u_int16_t   rd_index;
    u_int16_t   rd_expSeq;
    u_int32_t   rd_resetVal;
    u_int8_t    rd_start;
};

struct ar5416RadWriter {
    u_int16_t   wr_index;
    u_int16_t   wr_seq;
};

struct ar5416RadarEvent {
    u_int32_t   re_ts;      /* 32 bit time stamp */
    u_int8_t    re_rssi;    /* rssi of radar event */
    u_int8_t    re_dur;     /* duration of radar pulse */
    u_int8_t    re_chanIndex;   /* Channel of event */
};

struct ar5416RadarQElem {
    u_int32_t   rq_seqNum;
    u_int32_t   rq_busy;        /* 32 bit to insure atomic read/write */
    struct ar5416RadarEvent rq_event;   /* Radar event */
};

struct ar5416RadarQInfo {
    u_int16_t   ri_qsize;       /* q size */
    u_int16_t   ri_seqSize;     /* Size of sequence ring */
    struct ar5416RadReader ri_reader;   /* State for the q reader */
    struct ar5416RadWriter ri_writer;   /* state for the q writer */
};

#define HAL_MAX_ACK_RADAR_DUR   511
#define HAL_MAX_NUM_PEAKS   3
#define HAL_ARQ_SIZE        4096        /* 8K AR events for buffer size */
#define HAL_ARQ_SEQSIZE     4097        /* Sequence counter wrap for AR */
#define HAL_RADARQ_SIZE     1024        /* 1K radar events for buffer size */
#define HAL_RADARQ_SEQSIZE  1025        /* Sequence counter wrap for radar */
#define HAL_NUMRADAR_STATES 64      /* Number of radar channels we keep state for */

struct ar5416ArState {
    u_int16_t   ar_prevTimeStamp;
    u_int32_t   ar_prevWidth;
    u_int32_t   ar_phyErrCount[HAL_MAX_ACK_RADAR_DUR];
    u_int32_t   ar_ackSum;
    u_int16_t   ar_peakList[HAL_MAX_NUM_PEAKS];
    u_int32_t   ar_packetThreshold; /* Thresh to determine traffic load */
    u_int32_t   ar_parThreshold;    /* Thresh to determine peak */
    u_int32_t   ar_radarRssi;       /* Rssi threshold for AR event */
};

struct ar5416RadarState {
    HAL_CHANNEL_INTERNAL *rs_chan;      /* Channel info */
    u_int8_t    rs_chanIndex;       /* Channel index in radar structure */
    u_int32_t   rs_numRadarEvents;  /* Number of radar events */
    int32_t     rs_firpwr;      /* Thresh to check radar sig is gone */
    u_int32_t   rs_radarRssi;       /* Thresh to start radar det (dB) */
    u_int32_t   rs_height;      /* Thresh for pulse height (dB)*/
    u_int32_t   rs_pulseRssi;       /* Thresh to check if pulse is gone (dB) */
    u_int32_t   rs_inband;      /* Thresh to check if pusle is inband (0.5 dB) */
};

#define AR5416_OPFLAGS_11A           0x01   /* if set, allow 11a */
#define AR5416_OPFLAGS_11G           0x02   /* if set, allow 11g */
#define AR5416_OPFLAGS_N_5G_HT40     0x04   /* if set, disable 5G HT40 */
#define AR5416_OPFLAGS_N_2G_HT40     0x08   /* if set, disable 2G HT40 */
#define AR5416_OPFLAGS_N_5G_HT20     0x10   /* if set, disable 5G HT20 */
#define AR5416_OPFLAGS_N_2G_HT20     0x20   /* if set, disable 2G HT20 */

#define AR5416_MAX_CHAINS            3
#define FREQ2FBIN(x,y) ((y) ? ((x) - 2300) : (((x) - 4800) / 5))

#define AR5416_RATES_OFDM_OFFSET    0
#define AR5416_RATES_CCK_OFFSET     8
#define AR5416_RATES_HT20_OFFSET    16
#define AR5416_RATES_HT40_OFFSET    24


/* Support for multiple INIs */
struct ar5416IniArray {
    u_int32_t *ia_array;
    u_int32_t ia_rows;
    u_int32_t ia_columns;
};
#define INIT_INI_ARRAY(iniarray, array, columns) \
    INIT_INI_ARRAY_BASE( \
        (iniarray), (array), sizeof(array)/sizeof((array)[0]), (columns))
#define INIT_INI_ARRAY_BASE(iniarray, array, rows, columns) do {             \
    (iniarray)->ia_array = (u_int32_t *)(array);    \
    (iniarray)->ia_rows = (rows);       \
    (iniarray)->ia_columns = (columns); \
} while (0)
#define INI_RA(iniarray, row, column) (((iniarray)->ia_array)[(row) * ((iniarray)->ia_columns) + (column)])

#define INIT_CAL(_perCal)   \
    (_perCal)->calState = CAL_WAITING;  \
    (_perCal)->calNext = AH_NULL;

#define INSERT_CAL(_ahp, _perCal)   \
do {                    \
    if ((_ahp)->ah_cal_list_last == AH_NULL) {  \
        (_ahp)->ah_cal_list = (_ahp)->ah_cal_list_last = (_perCal); \
        ((_ahp)->ah_cal_list_last)->calNext = (_perCal);    \
    } else {    \
        ((_ahp)->ah_cal_list_last)->calNext = (_perCal);    \
        (_ahp)->ah_cal_list_last = (_perCal);   \
        (_perCal)->calNext = (_ahp)->ah_cal_list;   \
    }   \
} while (0)
 
typedef enum cal_types {
    ADC_DC_INIT_CAL = 0x1,
    ADC_GAIN_CAL    = 0x2,
    ADC_DC_CAL      = 0x4,
    IQ_MISMATCH_CAL = 0x8
} HAL_CAL_TYPES;

typedef enum cal_state {
    CAL_INACTIVE,
    CAL_WAITING,
    CAL_RUNNING,
    CAL_DONE
} HAL_CAL_STATE;            /* Calibrate state */

#define MIN_CAL_SAMPLES     1
#define MAX_CAL_SAMPLES    64
#define INIT_LOG_COUNT      5
#define PER_MIN_LOG_COUNT   2
#define PER_MAX_LOG_COUNT  10

/* Per Calibration data structure */
typedef struct per_cal_data {
    HAL_CAL_TYPES calType;           // Type of calibration
    u_int32_t     calNumSamples;     // Number of SW samples to collect
    u_int32_t     calCountMax;       // Number of HW samples to collect
    void (*calCollect)(struct ath_hal *);  // Accumulator func
    void (*calPostProc)(struct ath_hal *, u_int8_t); // Post-processing func
} HAL_PERCAL_DATA;

/* List structure for calibration data */
typedef struct cal_list {
    const HAL_PERCAL_DATA  *calData;
    HAL_CAL_STATE          calState;
    struct cal_list        *calNext;
} HAL_CAL_LIST;

#define AR5416_NUM_CAL_TYPES 1

struct ath_hal_5416 {
    struct ath_hal_private_tables  ah_priv;    /* base class */

    /*
     * Information retrieved from EEPROM.
     */
    ar5416_eeprom_t  ah_eeprom;

    GAIN_VALUES ah_gainValues;

    u_int8_t    ah_macaddr[IEEE80211_ADDR_LEN];
    u_int8_t    ah_bssid[IEEE80211_ADDR_LEN];
    u_int8_t    ah_bssidmask[IEEE80211_ADDR_LEN];
    u_int16_t   ah_assocId;

    int16_t     ah_curchanRadIndex; /* cur. channel radar index */

    /*
     * Runtime state.
     */
    u_int32_t   ah_maskReg;     /* copy of AR_IMR */
    struct ar5416Stats ah_stats;        /* various statistics */
    RF_HAL_FUNCS    ah_rfHal;
    u_int32_t   ah_txDescMask;      /* mask for TXDESC */
    u_int32_t   ah_txOkInterruptMask;
    u_int32_t   ah_txErrInterruptMask;
    u_int32_t   ah_txDescInterruptMask;
    u_int32_t   ah_txEolInterruptMask;
    u_int32_t   ah_txUrnInterruptMask;
    HAL_TX_QUEUE_INFO ah_txq[HAL_NUM_TX_QUEUES];
    HAL_POWER_MODE  ah_powerMode;
    HAL_SMPS_MODE   ah_smPowerMode;
    HAL_BOOL    ah_chipFullSleep;
    u_int32_t   ah_atimWindow;
    HAL_ANT_SETTING ah_diversityControl;    /* antenna setting */
    u_int16_t   ah_antennaSwitchSwap;       /* Controls mapping of OID request */
    u_int8_t    ah_tx_chainmask_cfg;        /* chain mask config */
    u_int8_t    ah_rx_chainmask_cfg;
    /* Calibration related fields */
    HAL_CAL_TYPES ah_suppCals;
    HAL_CAL_LIST  ah_iqCalData;         /* IQ Cal Data */
    HAL_CAL_LIST  ah_adcGainCalData;    /* ADC Gain Cal Data */
    HAL_CAL_LIST  ah_adcDcCalInitData;  /* Init ADC DC Offset Cal Data */
    HAL_CAL_LIST  ah_adcDcCalData;      /* Periodic ADC DC Offset Cal Data */
    HAL_CAL_LIST  *ah_cal_list;         /* ptr to first cal in list */
    HAL_CAL_LIST  *ah_cal_list_last;    /* ptr to last cal in list */
    HAL_CAL_LIST  *ah_cal_list_curr;    /* ptr to current cal */
// IQ Cal aliases
#define ah_totalPowerMeasI ah_Meas0.unsign
#define ah_totalPowerMeasQ ah_Meas1.unsign
#define ah_totalIqCorrMeas ah_Meas2.sign
// Adc Gain Cal aliases
#define ah_totalAdcIOddPhase  ah_Meas0.unsign
#define ah_totalAdcIEvenPhase ah_Meas1.unsign
#define ah_totalAdcQOddPhase  ah_Meas2.unsign
#define ah_totalAdcQEvenPhase ah_Meas3.unsign
// Adc DC Offset Cal aliases
#define ah_totalAdcDcOffsetIOddPhase  ah_Meas0.sign
#define ah_totalAdcDcOffsetIEvenPhase ah_Meas1.sign
#define ah_totalAdcDcOffsetQOddPhase  ah_Meas2.sign
#define ah_totalAdcDcOffsetQEvenPhase ah_Meas3.sign
    union {
        u_int32_t   unsign[AR5416_MAX_CHAINS];
        int32_t     sign[AR5416_MAX_CHAINS];
    } ah_Meas0;
    union {
        u_int32_t   unsign[AR5416_MAX_CHAINS];
        int32_t     sign[AR5416_MAX_CHAINS];
    } ah_Meas1;
    union {
        u_int32_t   unsign[AR5416_MAX_CHAINS];
        int32_t     sign[AR5416_MAX_CHAINS];
    } ah_Meas2;
    union {
        u_int32_t   unsign[AR5416_MAX_CHAINS];
        int32_t     sign[AR5416_MAX_CHAINS];
    } ah_Meas3;
    u_int16_t   ah_CalSamples;
    /* end - Calibration related fields */
    u_int32_t   ah_tx6PowerInHalfDbm;   /* power output for 6Mb tx */
    u_int32_t   ah_staId1Defaults;  /* STA_ID1 default settings */
    u_int32_t   ah_miscMode;        /* MISC_MODE settings */
    HAL_BOOL    ah_getPlcpHdr;      /* setting about MISC_SEL_EVM */
    enum {
        AUTO_32KHZ,     /* use it if 32kHz crystal present */
        USE_32KHZ,      /* do it regardless */
        DONT_USE_32KHZ,     /* don't use it regardless */
    } ah_enable32kHzClock;          /* whether to sleep at 32kHz */
    u_int32_t   *ah_analogBank0Data;/* RF register banks */
    u_int32_t   *ah_analogBank1Data;
    u_int32_t   *ah_analogBank2Data;
    u_int32_t   *ah_analogBank3Data;
    u_int32_t   *ah_analogBank6Data;
    u_int32_t   *ah_analogBank6TPCData;
    u_int32_t   *ah_analogBank7Data;

    u_int32_t   *ah_addacOwl21;     /* temporary register arrays */
    u_int32_t   *ah_bank6Temp;

    u_int32_t   ah_ofdmTxPower;
    int16_t     ah_txPowerIndexOffset;

    u_int       ah_slottime;        /* user-specified slot time */
    u_int       ah_acktimeout;      /* user-specified ack timeout */
    /*
     * XXX
     * 11g-specific stuff; belongs in the driver.
     */
    u_int8_t    ah_gBeaconRate;     /* fixed rate for G beacons */
    u_int32_t   ah_gpioMask;        /* copy of enabled GPIO mask */
    /*
     * RF Silent handling; setup according to the EEPROM.
     */
    u_int32_t   ah_gpioSelect;      /* GPIO pin to use */
    u_int32_t   ah_polarity;        /* polarity to disable RF */
    u_int32_t   ah_gpioBit;     /* after init, prev value */
    HAL_BOOL    ah_eepEnabled;      /* EEPROM bit for capability */

#ifdef ATH_BT_COEX
    /*
     * Bluetooth coexistence static setup according to the registry
     */
    HAL_BT_MODULE ah_btModule;           /* Bluetooth module identifier */
    u_int8_t    ah_btCoexConfigType;     /* BT coex configuration */
    u_int8_t    ah_btActiveGpioSelect;   /* GPIO pin for BT_ACTIVE */
    u_int8_t    ah_btPriorityGpioSelect; /* GPIO pin for BT_PRIORITY */
    u_int8_t    ah_wlanActiveGpioSelect; /* GPIO pin for WLAN_ACTIVE */
    u_int8_t    ah_btActivePolarity;     /* Polarity of BT_ACTIVE */
    HAL_BOOL    ah_btCoexSingleAnt;      /* Single or dual antenna configuration */
    u_int8_t    ah_btWlanIsolation;      /* Isolation between BT and WLAN in dB */

    /*
     * Bluetooth coexistence runtime settings
     */
    HAL_BOOL    ah_btCoexEnabled;        /* If Bluetooth coexistence is enabled */
    u_int32_t   ah_btCoexMode;           /* Register setting for AR_BT_COEX_MODE */
    u_int32_t   ah_btCoexBTWeight;       /* Register setting for AR_BT_COEX_WEIGHT */
    u_int32_t   ah_btCoexWLANWeight;     /* Register setting for AR_BT_COEX_WEIGHT */
    u_int32_t   ah_btCoexMode2;          /* Register setting for AR_BT_COEX_MODE2 */
    u_int32_t   ah_btCoexFlag;           /* Special tuning flags for BT coex */
#endif

    /*
     * Generic timer support
     */
    u_int32_t   ah_availGenTimers;       /* mask of available timers */
    u_int32_t   ah_intrGenTimerTrigger;  /* generic timer trigger interrupt state */
    u_int32_t   ah_intrGenTimerThresh;   /* generic timer trigger interrupt state */
    HAL_BOOL    ah_enableTSF2;           /* enable TSF2 for gen timer 8-15. (Kiwi) */


    /*
     * ANI & Radar support.
     */
    u_int32_t   ah_procPhyErr;      /* Process Phy errs */
    HAL_BOOL    ah_hasHwPhyCounters;    /* Hardware has phy counters */
    u_int32_t   ah_aniPeriod;       /* ani update list period */
    struct ar5416AniState   *ah_curani; /* cached last reference */
    struct ar5416AniState   ah_ani[255]; /* per-channel state */
    struct ar5416RadarState ah_radar[HAL_NUMRADAR_STATES];  /* Per-Channel Radar detector state */
    struct ar5416RadarQElem *ah_radarq; /* radar event queue */
    struct ar5416RadarQInfo ah_radarqInfo;  /* radar event q read/write state */
    struct ar5416ArState    ah_ar;      /* AR detector state */
    struct ar5416RadarQElem *ah_arq;    /* AR event queue */
    struct ar5416RadarQInfo ah_arqInfo; /* AR event q read/write state */

    /*
     * Ani tables that change between the 5416 and 5312.
     * These get set at attach time.
     * XXX don't belong here
     * XXX need better explanation
     */
        int     ah_totalSizeDesired[5];
        int     ah_coarseHigh[5];
        int     ah_coarseLow[5];
        int     ah_firpwr[5];

    /*
     * Transmit power state.  Note these are maintained
     * here so they can be retrieved by diagnostic tools.
     */
    /* This is used to store transmit power settings configured dynamically through 
     * the athconf power utility. If ah_dynconf is set, these values will
     * overide the eeprom settings.
     */
    u_int16_t   ah_ratesArray[Ar5416RateSize];
    HAL_BOOL    ah_dynconf; 
     /*
     * Tx queue interrupt state.
     */
    u_int32_t   ah_intrTxqs;

    HAL_BOOL    ah_intrMitigationRx; /* rx Interrupt Mitigation Settings */
    HAL_BOOL    ah_intrMitigationTx; /* tx Interrupt Mitigation Settings */

    /*
     * Extension Channel Rx Clear State
     */
    u_int32_t   ah_cycleCount;
    u_int32_t   ah_ctlBusy;
    u_int32_t   ah_extBusy;
    u_int32_t   ah_Rf;
    u_int32_t   ah_Tf;

    /* HT CWM state */
    HAL_HT_EXTPROTSPACING ah_extprotspacing;
    u_int8_t    ah_txchainmask; /* tx chain mask */
    u_int8_t    ah_rxchainmask; /* rx chain mask */

    int         ah_hwp;
    void        *ah_cal_mem;
    HAL_BOOL    ah_emu_eeprom;

    HAL_ANI_CMD ah_ani_function;
    HAL_BOOL    ah_rifs_enabled;
    u_int32_t   ah_rifs_reg[11];
    u_int32_t   ah_rifs_sec_cnt;

    /* open-loop power control */
    u_int32_t originalGain[22];
    int32_t   initPDADC;
    int32_t   PDADCdelta;

    /* cycle counts for beacon stuck diagnostics */
    u_int32_t   ah_cycles;          
    u_int32_t   ah_rx_clear;
    u_int32_t   ah_rx_frame;
    u_int32_t   ah_tx_frame;

#define BB_HANG_SIG1 0 
#define BB_HANG_SIG2 1 
#define BB_HANG_SIG3 2 
#define BB_HANG_SIG4 3 
#define MAC_HANG_SIG1 4 
#define MAC_HANG_SIG2 5 
    /* bb hang detection */
    int		ah_hang[6];
    hal_hw_hangs_t  ah_hang_wars;
    /*
     * Support for ar5416 multiple INIs
     */
    struct ar5416IniArray ah_iniModes;
    struct ar5416IniArray ah_iniCommon;
    struct ar5416IniArray ah_iniBank0;
    struct ar5416IniArray ah_iniBB_RfGain;
    struct ar5416IniArray ah_iniBank1;
    struct ar5416IniArray ah_iniBank2;
    struct ar5416IniArray ah_iniBank3;
    struct ar5416IniArray ah_iniBank6;
    struct ar5416IniArray ah_iniBank6TPC;
    struct ar5416IniArray ah_iniBank7;
    struct ar5416IniArray ah_iniAddac;
    struct ar5416IniArray ah_iniPcieSerdes;
    struct ar5416IniArray ah_iniModesAdditional;
#ifdef AH_SUPPORT_K2
    struct ar5416IniArray ah_iniModes_K2_1_0_only;
    struct ar5416IniArray ah_iniModes_K2_ANI_reg;
    struct ar5416IniArray ah_iniCommon_normal_cck_fir_coeff_K2;
    struct ar5416IniArray ah_iniCommon_japan_2484_cck_fir_coeff_K2;
    struct ar5416IniArray ah_iniModes_high_power_tx_gain_K2;
    struct ar5416IniArray ah_iniModes_normal_power_tx_gain_K2;
#endif    
    struct ar5416IniArray ah_iniModesRxgain;
    struct ar5416IniArray ah_iniModesTxgain;
#ifdef ATH_WOW
    struct ar5416IniArray ah_iniPcieSerdesWow;  /* SerDes values during WOW sleep */
#endif
    struct ar5416IniArray ah_iniCckfirNormal;
    struct ar5416IniArray ah_iniCckfirJapan2484;

    /* To indicate EEPROM mapping used */
    HAL_5416_EEP_MAP ah_eep_map;

    /* filled out Vpd table for all pdGains (chanL) */
    u_int8_t   ah_vpdTableL[AR5416_EEPDEF_NUM_PD_GAINS][AR5416_MAX_PWR_RANGE_IN_HALF_DB];
    /* filled out Vpd table for all pdGains (chanR) */
    u_int8_t   ah_vpdTableR[AR5416_EEPDEF_NUM_PD_GAINS][AR5416_MAX_PWR_RANGE_IN_HALF_DB];
    /* filled out Vpd table for all pdGains (interpolated) */
    u_int8_t   ah_vpdTableI[AR5416_EEPDEF_NUM_PD_GAINS][AR5416_MAX_PWR_RANGE_IN_HALF_DB];
    u_int32_t ah_immunity_vals[6];
    HAL_BOOL ah_immunity_on;
    /*
     * snap shot of counter register for debug purposes
     */
#ifdef AH_DEBUG
    u_int32_t lasttf;
    u_int32_t lastrf;
    u_int32_t lastrc;
    u_int32_t lastcc;
#endif

#ifdef QCA_PARTNER_PLATFORM
    u_int32_t analog_reg_shadow[5]; /* bug 34632 */
#endif
    HAL_BOOL    ah_dma_stuck; /* Set to true when RX/TX DMA failed to stop. */
    u_int32_t   nf_tsf32; /* timestamp for NF calibration duration */

    /* adjusted power for descriptor-based TPC for 1, 2, or 3 chains */
    int16_t txPower[Ar5416RateSize][AR5416_MAX_CHAINS];
    u_int32_t  txPowerRecord[3][9];
    u_int32_t  PALRecord;

	/*
	 * This is where we found the calibration data.
	 */
	int calibration_data_source;
	int calibration_data_source_address;
	/*
	 * This is where we look for the calibration data. must be set before ath_attach() is called
	 */
	int calibration_data_try;
	int calibration_data_try_address;

};

#define AH5416(_ah) ((struct ath_hal_5416 *)(_ah))

#define IS_5416_EMU(ah) \
    ((AH_PRIVATE(ah)->ah_devid == AR5416_DEVID_EMU) || \
     (AH_PRIVATE(ah)->ah_devid == AR5416_DEVID_EMU_PCIE))

#define ar5416RfDetach(ah) do {             \
    if (AH5416(ah)->ah_rfHal.rfDetach != AH_NULL)   \
        AH5416(ah)->ah_rfHal.rfDetach(ah);  \
} while (0)

#define ar5416EepDataInFlash(_ah) \
    (!(AH_PRIVATE(_ah)->ah_flags & AH_USE_EEPROM))

#define IS_5GHZ_FAST_CLOCK_EN(_ah, _c)  (IS_CHAN_5GHZ(_c) && \
                                           ((AH_PRIVATE(_ah))->ah_config.ath_hal_fastClockEnable) && \
                                           ((ar5416EepromGet(AH5416(_ah), EEP_MINOR_REV) <= AR5416_EEP_MINOR_VER_16) || \
                                           (ar5416EepromGet(AH5416(_ah), EEP_FSTCLK_5G))))

#if AH_NEED_TX_DATA_SWAP
#if AH_NEED_RX_DATA_SWAP
#define ar5416_init_cfg_reg(ah) OS_REG_WRITE(ah, AR_CFG, AR_CFG_SWTB | AR_CFG_SWRB)
#else
#define ar5416_init_cfg_reg(ah) OS_REG_WRITE(ah, AR_CFG, AR_CFG_SWTB)
#endif
#elif AH_NEED_RX_DATA_SWAP
#define ar5416_init_cfg_reg(ah) OS_REG_WRITE(ah, AR_CFG, AR_CFG_SWRB)
#else
#define ar5416_init_cfg_reg(ah) OS_REG_WRITE(ah, AR_CFG, AR_CFG_SWTD | AR_CFG_SWRD)
#endif


/*
 * WAR for bug 6773.  OS_DELAY() does a PIO READ on the PCI bus which allows
 * other cards' DMA reads to complete in the middle of our reset.
 */
#ifdef ATH_USB
#define WAR_6773(x)
#else
#define WAR_6773(x) do {                \
        if ((++(x) % 64) == 0)          \
                OS_DELAY(1);            \
} while (0)
#endif

/*
 *  There are two methods to fix PCI write unreliable to analog register
 *    space (0x7800- 0x7900):
 *    1. Use analog long shift register.
 *    2. Use delay.
 *    but, the new value can not be written correcly by using method 1. So
 *      we select method 2 to work around this issue. (Reported Bug:32840)
 */
#define WAR_32840(_ah, _reg) do {     \
        if ((AR_SREV_MERLIN_10_OR_LATER(_ah)) &&  \
            ((_reg) >= 0x00007800 &&  \
             (_reg) < 0x00007900) )  \
            OS_DELAY(100);            \
} while (0)

#define REG_WRITE_ARRAY(iniarray, column, regWr) do {                   \
        int r;                                                          \
		ENABLE_REG_WRITE_BUFFER                                         \
        for (r = 0; r < ((iniarray)->ia_rows); r++) {    \
                OS_REG_WRITE(ah, INI_RA((iniarray), (r), 0), INI_RA((iniarray), r, (column)));\
                WAR_6773(regWr);                                        \
                WAR_32840(ah, INI_RA((iniarray), (r), 0));              \
        }                                                               \
        OS_REG_WRITE_FLUSH(ah);                                         \
		DISABLE_REG_WRITE_BUFFER                                        \
} while (0)


#define ar5416_send_txpower_to_target(ah, mode)



/******************************************************************************/
/*!
**  \brief Analog Shift Register Read-Modify-Write
**
**  This routine will either perform the standard RMW procedure, or apply the
**  Merlin specific write process depending on the setting of the configuration
**  parameter.  For AP it's on by default, for Station it's controlled by
**  registry parameters.
**
**  \param ah   HAL instance variable
**  \param reg  register offset to program
**  \param val  value to insert into the register.
**  \return N/A
*/
static inline void
analogShiftRegRMW(struct ath_hal *ah, u_int reg, u_int32_t mask,
                  u_int32_t shift, u_int32_t val)
{
    u_int32_t               regVal;
    struct ath_hal_private  *ap = AH_PRIVATE(ah);

    /*
    ** Code Begins
    ** Get the register value, and mask the appropriate bits
    */

     regVal = OS_REG_READ(ah,reg) & ~mask;

     /*
     ** Insert the value into the appropriate slot
     */

     regVal |= (val << shift) & mask;

     /*
     ** Determine if we use standard write or the "magic" write
     */

     OS_REG_WRITE(ah,reg,regVal);

     if (ap->ah_config.ath_hal_analogShiftRegDelay) {
        OS_DELAY(ap->ah_config.ath_hal_analogShiftRegDelay);
     }

    return;
}

/******************************************************************************/
/*!
  **  \brief Analog Shift Register Write
  **
  **  This routine will either perform the standard Write procedure, or apply the
  **  Merlin specific write process depending on the setting of the configuration
  **  parameter.  For AP it's on by default, for Station it's controlled by
  **  registry parameters.
  ** 
  **  \param ah      HAL instance variable
  **  \param reg     register offset to program
  **  \param val     value to insert into the register.
  */

static inline void 
analogShiftRegWrite(struct ath_hal *ah, u_int reg, u_int32_t val)
{
        struct ath_hal_private  *ap = AH_PRIVATE(ah);
            
        OS_REG_WRITE(ah, reg, val);
                 
        if (ap->ah_config.ath_hal_analogShiftRegDelay) {
            OS_DELAY(ap->ah_config.ath_hal_analogShiftRegDelay);
        }
}

/* 
 * For Kite and later chipsets, the following bits are not being programmed in EEPROM
 * and so need to be enabled always.
 * Bit 0: en_fcc_mid,  Bit 1: en_jap_mid,      Bit 2: en_fcc_dfs_ht40
 * Bit 3: en_jap_ht40, Bit 4: en_jap_dfs_ht40
 */
#define AR9285_RDEXT_DEFAULT  0x1F

/*
 * If the code tries at run-time to use a feature
 * that was removed during compilation, complain.
 */
#ifdef AH_ASSERT
    #define ar5416FeatureNotSupported(feature, ah, func)    \
        ath_hal_printf(ah, # feature                        \
            " not supported but called from %s\n", (func)), \
        hal_assert(0)
#else
    #define ar5416FeatureNotSupported(feature, ah, func)    \
        ath_hal_printf(ah, # feature                        \
            " not supported but called from %s\n", (func))
#endif /* AH_ASSERT */

extern  HAL_BOOL ar2133RfAttach(struct ath_hal *, HAL_STATUS *);

struct ath_hal;

extern  HAL_STATUS ar5416RadioAttach(struct ath_hal *ah);
extern  struct ath_hal_5416 * ar5416NewState(u_int16_t devid,
        HAL_ADAPTER_HANDLE osdev, HAL_SOFTC sc,
        HAL_BUS_TAG st, HAL_BUS_HANDLE sh, HAL_BUS_TYPE bustype,
        struct hal_reg_parm *hal_conf_parm, HAL_STATUS *status);
extern  struct ath_hal * ar5416Attach(u_int16_t devid,
        HAL_ADAPTER_HANDLE osdev, HAL_SOFTC sc,
        HAL_BUS_TAG st, HAL_BUS_HANDLE sh, HAL_BUS_TYPE bustype, 
        struct hal_reg_parm *hal_conf_parm,
        HAL_STATUS *status);
extern  void ar5416Detach(struct ath_hal *ah);
extern void ar5416ReadRevisions(struct ath_hal *ah);
#ifdef AH_PRIVATE_DIAG
extern  HAL_BOOL ar5416ChipTest(struct ath_hal *ah);
#else
#define ar5416ChipTest(_ah) AH_TRUE
#endif /* AH_PRIVATE_DIAG */
extern  HAL_BOOL ar5416GetChannelEdges(struct ath_hal *ah,
                u_int16_t flags, u_int16_t *low, u_int16_t *high);
extern  HAL_BOOL ar5416FillCapabilityInfo(struct ath_hal *ah);

void ar5416BeaconInit(struct ath_hal *ah,
                      u_int32_t next_beacon, u_int32_t beacon_period, 
                      u_int32_t beacon_period_fraction, HAL_OPMODE opmode);
extern  void ar5416SetStaBeaconTimers(struct ath_hal *ah,
        const HAL_BEACON_STATE *);

extern  HAL_BOOL ar5416IsInterruptPending(struct ath_hal *ah);
extern  HAL_BOOL ar5416GetPendingInterrupts(struct ath_hal *ah, HAL_INT *,
                                            HAL_INT_TYPE, u_int8_t, HAL_BOOL);
extern  HAL_INT ar5416GetInterrupts(struct ath_hal *ah);
extern  HAL_INT ar5416SetInterrupts(struct ath_hal *ah, HAL_INT ints, HAL_BOOL);
extern  void ar5416SetIntrMitigationTimer(struct ath_hal* ah,
        HAL_INT_MITIGATION reg, u_int32_t value);
extern  u_int32_t ar5416GetIntrMitigationTimer(struct ath_hal* ah,
        HAL_INT_MITIGATION reg);

extern  u_int32_t ar5416GetKeyCacheSize(struct ath_hal *);
extern  HAL_BOOL ar5416IsKeyCacheEntryValid(struct ath_hal *, u_int16_t entry);
extern  HAL_BOOL ar5416ResetKeyCacheEntry(struct ath_hal *ah, u_int16_t entry);
extern  HAL_BOOL ar5416SetKeyCacheEntryMac(struct ath_hal *,
            u_int16_t entry, const u_int8_t *mac);
extern  HAL_BOOL ar5416SetKeyCacheEntry(struct ath_hal *ah, u_int16_t entry,
                       const HAL_KEYVAL *k, const u_int8_t *mac, int xorKey);

#if ATH_SUPPORT_KEYPLUMB_WAR
extern  HAL_BOOL ar5416CheckKeyCacheEntry(struct ath_hal *ah, u_int16_t entry,
                       const HAL_KEYVAL *k, int xorKey);
#endif
extern  void ar5416GetMacAddress(struct ath_hal *ah, u_int8_t *mac);
extern  HAL_BOOL ar5416SetMacAddress(struct ath_hal *ah, const u_int8_t *);
extern  void ar5416GetBssIdMask(struct ath_hal *ah, u_int8_t *mac);
extern  HAL_BOOL ar5416SetBssIdMask(struct ath_hal *, const u_int8_t *);
extern  HAL_STATUS ar5416SelectAntConfig(struct ath_hal *ah, u_int32_t cfg);
extern  void ar5416OpenLoopPowerControlTempCompensation(struct ath_hal *ah);
extern  HAL_BOOL ar5416InterferenceIsPresent(struct ath_hal *ah);
extern  HAL_BOOL ar5416SetRegulatoryDomain(struct ath_hal *ah,
                                    u_int16_t regDomain, HAL_STATUS *stats);
extern  u_int ar5416GetWirelessModes(struct ath_hal *ah);
extern  void ar5416EnableRfKill(struct ath_hal *);
extern  HAL_BOOL ar5416GpioCfgOutput(struct ath_hal *, u_int32_t gpio, HAL_GPIO_OUTPUT_MUX_TYPE signalType);
extern  HAL_BOOL ar5416GpioCfgOutputLEDoff(struct ath_hal *, u_int32_t gpio, HAL_GPIO_OUTPUT_MUX_TYPE signalType);
extern  HAL_BOOL ar5416GpioCfgInput(struct ath_hal *, u_int32_t gpio);
extern  HAL_BOOL ar5416GpioSet(struct ath_hal *, u_int32_t gpio, u_int32_t val);
extern  u_int32_t ar5416GpioGet(struct ath_hal *ah, u_int32_t gpio);
extern  void ar5416GpioSetIntr(struct ath_hal *ah, u_int, u_int32_t ilevel);
extern  void ar5416SetLedState(struct ath_hal *ah, HAL_LED_STATE state);
extern  void ar5416SetPowerLedState(struct ath_hal *ah, u_int8_t enable);
extern  void ar5416SetNetworkLedState(struct ath_hal *ah, u_int8_t enable);
extern  void ar5416WriteAssocid(struct ath_hal *ah, const u_int8_t *bssid,
        u_int16_t assocId);
extern  u_int32_t ar5416PpmGetRssiDump(struct ath_hal *);
extern  u_int32_t ar5416PpmArmTrigger(struct ath_hal *);
extern  int ar5416PpmGetTrigger(struct ath_hal *);
extern  u_int32_t ar5416PpmForce(struct ath_hal *);
extern  void ar5416PpmUnForce(struct ath_hal *);
extern  u_int32_t ar5416PpmGetForceState(struct ath_hal *);
extern  u_int32_t ar5416PpmGetForceState(struct ath_hal *);
extern  void ar5416SetDcsMode(struct ath_hal *ah, u_int32_t);
extern  u_int32_t ar5416GetDcsMode(struct ath_hal *ah);
extern  u_int32_t ar5416GetTsf32(struct ath_hal *ah);
extern  u_int64_t ar5416GetTsf64(struct ath_hal *ah);
extern  u_int32_t ar5416GetTsf2_32(struct ath_hal *ah);
extern  void ar5416SetTsf64(struct ath_hal *ah, u_int64_t tsf);
extern  void ar5416ResetTsf(struct ath_hal *ah);
extern  void ar5416SetBasicRate(struct ath_hal *ah, HAL_RATE_SET *pSet);
extern  u_int32_t ar5416GetRandomSeed(struct ath_hal *ah);
extern  HAL_BOOL ar5416DetectCardPresent(struct ath_hal *ah);
extern  void ar5416UpdateMibMacStats(struct ath_hal *ah);
extern  void ar5416GetMibMacStats(struct ath_hal *ah, HAL_MIB_STATS* stats);
extern  HAL_BOOL ar5416IsJapanChannelSpreadSupported(struct ath_hal *ah);
extern  u_int32_t ar5416GetCurRssi(struct ath_hal *ah);
extern  u_int ar5416GetDefAntenna(struct ath_hal *ah);
extern  void ar5416SetDefAntenna(struct ath_hal *ah, u_int antenna);
extern  HAL_BOOL ar5416SetAntennaSwitch(struct ath_hal *ah,
        HAL_ANT_SETTING settings, HAL_CHANNEL *chan, u_int8_t *, u_int8_t *, u_int8_t *);
extern  HAL_BOOL ar5416IsSleepAfterBeaconBroken(struct ath_hal *ah);
extern  HAL_BOOL ar5416SetSlotTime(struct ath_hal *, u_int);
extern  HAL_BOOL ar5416SetAckTimeout(struct ath_hal *, u_int);
extern  u_int ar5416GetAckTimeout(struct ath_hal *);
extern  HAL_STATUS ar5416SetQuiet(struct ath_hal *ah, u_int32_t period, u_int32_t duration, 
        u_int32_t nextStart, HAL_QUIET_FLAG flag);
extern  void ar5416SetPCUConfig(struct ath_hal *);
extern  HAL_STATUS ar5416GetCapability(struct ath_hal *, HAL_CAPABILITY_TYPE,
        u_int32_t, u_int32_t *);
extern  HAL_BOOL ar5416SetCapability(struct ath_hal *, HAL_CAPABILITY_TYPE,
        u_int32_t, u_int32_t, HAL_STATUS *);
extern  HAL_BOOL ar5416GetDiagState(struct ath_hal *ah, int request,
        const void *args, u_int32_t argsize,
        void **result, u_int32_t *resultsize);
#ifdef AR5416_EMULATION
extern  void ar5416InitMacTrace(struct ath_hal *ah);
extern  void ar5416StopMacTrace(struct ath_hal *ah);
#endif /* AR5416_EMULATION */
extern  void ar5416GetDescInfo(struct ath_hal *ah, HAL_DESC_INFO *desc_info);
extern  int8_t ar5416Get11nExtBusy(struct ath_hal *ah);
extern  u_int32_t ar5416GetChBusyPct(struct ath_hal *ah);
extern  void ar5416Set11nMac2040(struct ath_hal *ah, HAL_HT_MACMODE mode);
extern  HAL_HT_RXCLEAR ar5416Get11nRxClear(struct ath_hal *ah);
extern  void ar5416Set11nRxClear(struct ath_hal *ah, HAL_HT_RXCLEAR rxclear);
extern  HAL_BOOL ar5416SetPowerMode(struct ath_hal *ah, HAL_POWER_MODE mode,
        int set_chip);
extern  HAL_BOOL ar5416SetPowerModeAwake(struct ath_hal *ah, int set_chip);
extern  void ar5416SetSmPowerMode(struct ath_hal *ah, HAL_SMPS_MODE mode);

extern void ar5416ConfigPciPowerSave(struct ath_hal *ah, int restore, int powerOff);

extern  void ar5416ForceTSFSync(struct ath_hal *ah, const u_int8_t *bssid, 
                                u_int16_t assocId);

#if ATH_WOW
extern  void ar5416WowApplyPattern(struct ath_hal *ah, u_int8_t *p_ath_pattern,
        u_int8_t *p_ath_mask, int32_t pattern_count, u_int32_t athPatternLen);
//extern  u_int32_t ar5416WowWakeUp(struct ath_hal *ah,u_int8_t  *chipPatternBytes);
extern  u_int32_t ar5416WowWakeUp(struct ath_hal *ah, bool offloadEnable);
extern  HAL_BOOL ar5416WowEnable(struct ath_hal *ah, u_int32_t pattern_enable, u_int32_t timeoutInSeconds, int clearbssid,
                                                                        bool offloadEnable);
#endif

extern  HAL_BOOL ar5416Reset(struct ath_hal *ah, HAL_OPMODE opmode,
        HAL_CHANNEL *chan, HAL_HT_MACMODE macmode, u_int8_t txchainmask,
        u_int8_t rxchainmask, HAL_HT_EXTPROTSPACING extprotspacing,
        HAL_BOOL bChannelChange, HAL_STATUS *status, int is_scan);

extern  HAL_BOOL ar5416SetResetReg(struct ath_hal *ah, u_int32_t type);
extern  void ar5416InitPLL(struct ath_hal *ah, HAL_CHANNEL *chan);
extern  void ar5416GreenApPsOnOff( struct ath_hal *ah, u_int16_t rxMask);
extern  u_int16_t ar5416IsSingleAntPowerSavePossible(struct ath_hal *ah);
extern  void ar5416SetOperatingMode(struct ath_hal *ah, int opmode);
extern  HAL_BOOL ar5416PhyDisable(struct ath_hal *ah);
extern  HAL_BOOL ar5416Disable(struct ath_hal *ah);
extern  HAL_BOOL ar5416ChipReset(struct ath_hal *ah, HAL_CHANNEL *);
extern  HAL_BOOL ar5416Calibration(struct ath_hal *ah,  HAL_CHANNEL *chan, 
        u_int8_t rxchainmask, HAL_BOOL longcal, HAL_BOOL *isIQdone,
        int is_scan, u_int32_t *sched_cals);
extern  void ar5416ResetCalValid(struct ath_hal *ah,  HAL_CHANNEL *chan,
                                 HAL_BOOL *isIQdone, u_int32_t calType);
extern  void ar5416IQCalCollect(struct ath_hal *ah);
extern  void ar5416IQCalibration(struct ath_hal *ah, u_int8_t numChains);
extern  void ar5416AdcGainCalCollect(struct ath_hal *ah);
extern  void ar5416AdcGainCalibration(struct ath_hal *ah, u_int8_t numChains);
extern  void ar5416AdcDcCalCollect(struct ath_hal *ah);
extern  void ar5416AdcDcCalibration(struct ath_hal *ah, u_int8_t numChains);
extern  int16_t ar5416GetMinCCAPwr(struct ath_hal *ah);
extern  void ar5416UploadNoiseFloor(struct ath_hal *ah, int16_t nfarray[]);

extern  HAL_BOOL ar5416SetTxPowerLimit(struct ath_hal *ah, u_int32_t limit,
        u_int16_t extra_txpow, u_int16_t tpcInDb);
extern  void ar5416GetChipMinAndMaxPowers(struct ath_hal *ah, int8_t *max_tx_power,
                                          int8_t *min_tx_power);
extern  void ar5416FillHalChansFromRegdb(struct ath_hal *ah,
                                         uint16_t freq,
                                         int8_t maxregpower,
                                         int8_t maxpower,
                                         int8_t minpower,
                                         uint32_t flags,
                                         int index);
extern void ar5416Setsc(struct ath_hal *ah, HAL_SOFTC sc);
extern void ar5416ChainNoiseFloor(struct ath_hal *ah, int16_t *nfBuf, HAL_CHANNEL *chan, int is_scan);
extern HAL_BOOL ar5416IsAniNoiseSpur(struct ath_hal *ah);

extern  HAL_RFGAIN ar5416GetRfgain(struct ath_hal *ah);
extern  const HAL_RATE_TABLE *ar5416GetRateTable(struct ath_hal *, u_int mode);
extern  int16_t ar5416GetRateTxPower(struct ath_hal *ah, u_int mode,
                                     u_int8_t rate_index, u_int8_t chainmask);
extern  void ar5416InitRateTxPower(struct ath_hal *ah, u_int mode,
                                   HAL_CHANNEL_INTERNAL *chan,
                                   int ht40Correction, int16_t powerPerRate[],
                                   u_int8_t chainmask);
#ifdef AH_DEBUG
extern  void ar5416DumpRateTxPower(struct ath_hal *ah, u_int mode);
#else
#define ar5416DumpRateTxPower(ahp, mode)
#endif

extern  void ar5416EnableMIBCounters(struct ath_hal *);
extern  void ar5416DisableMIBCounters(struct ath_hal *);
extern  void ar5416AniAttach(struct ath_hal *);
extern  void ar5416AniDetach(struct ath_hal *);
extern  struct ar5416AniState *ar5416AniGetCurrentState(struct ath_hal *);
extern  struct ar5416Stats *ar5416AniGetCurrentStats(struct ath_hal *);
extern  HAL_BOOL ar5416AniControl(struct ath_hal *, HAL_ANI_CMD cmd, int param, HAL_BOOL inISR);
struct ath_rx_status;
extern  void ar5416AniPhyErrReport(struct ath_hal *ah,
        const struct ath_rx_status *rs);

extern  void ar5416ProcessMibIntr(struct ath_hal *, const HAL_NODE_STATS *);
extern  void ar5416AniArPoll(struct ath_hal *, const HAL_NODE_STATS *,
                 HAL_CHANNEL *, HAL_ANISTATS *);
extern  void ar5416AniReset(struct ath_hal *);
extern  void ar5416EnableTPC(struct ath_hal *);
#if ATH_ANT_DIV_COMB
extern  HAL_BOOL ar5416_ant_ctrl_set_lna_div_use_bt_ant(struct ath_hal * ah, HAL_BOOL enable, HAL_CHANNEL * chan);
#endif /* ATH_ANT_DIV_COMB */

/* DFS declarations */
#ifdef ATH_SUPPORT_DFS
extern  void ar5416EnableDfs(struct ath_hal *ah, HAL_PHYERR_PARAM *pe); 
extern  void ar5416GetDfsThresh(struct ath_hal *ah, HAL_PHYERR_PARAM *pe);
extern  HAL_BOOL ar5416RadarWait(struct ath_hal *ah, HAL_CHANNEL *chan);
extern void ar5416_adjust_difs(struct ath_hal *ah, u_int32_t val);
extern u_int32_t ar5416_dfs_config_fft(struct ath_hal *ah, bool is_enable);
extern void ar5416_dfs_cac_war(struct ath_hal *ah, u_int32_t start);
#endif

extern HAL_CHANNEL* ar5416GetExtensionChannel(struct ath_hal *ah);
extern void ar5416MarkPhyInactive(struct ath_hal *ah);
extern HAL_BOOL ar5416IsFastClockEnabled(struct ath_hal *ah);
extern u_int16_t ar5416GetAHDevid(struct ath_hal *ah);

/* Spectral scan declarations */
extern void ar5416ConfigureSpectralScan(struct ath_hal *ah, HAL_SPECTRAL_PARAM *ss);
extern void ar5416SetCcaThreshold(struct ath_hal *ah, u_int8_t thresh62);
extern void ar5416GetSpectralParams(struct ath_hal *ah, HAL_SPECTRAL_PARAM *ss);
extern void ar5416StartSpectralScan(struct ath_hal *ah);
extern void ar5416StopSpectralScan(struct ath_hal *ah);
extern HAL_BOOL ar5416IsSpectralEnabled(struct ath_hal *ah);
extern HAL_BOOL ar5416IsSpectralActive(struct ath_hal *ah);
extern u_int32_t ar5416GetSpectralConfig(struct ath_hal *ah);
extern void ar5416RestoreSpectralConfig(struct ath_hal *ah, u_int32_t restoreval);
int16_t ar5416GetCtlChanNF(struct ath_hal *ah);
int16_t ar5416GetExtChanNF(struct ath_hal *ah);
int16_t ar5416GetNominalNF(struct ath_hal *ah, HAL_FREQ_BAND band);
/* End spectral scan declarations */

/* Raw ADC capture functions */
extern void ar5416EnableTestAddacMode(struct ath_hal *ah);
extern void ar5416DisableTestAddacMode(struct ath_hal *ah);
extern void ar5416BeginAdcCapture(struct ath_hal *ah, int auto_agc_gain);
extern HAL_STATUS ar5416RetrieveCaptureData(struct ath_hal *ah, u_int16_t chain_mask, int disable_dc_filter, void *sample_buf, u_int32_t *max_samples);
extern HAL_STATUS ar5416CalculateADCRefPowers(struct ath_hal *ah, int freq_mhz, int16_t *sample_min, int16_t *sample_max, int32_t *chain_ref_pwr, int num_chain_ref_pwr);
extern HAL_STATUS ar5416GetMinAGCGain(struct ath_hal *ah, int freq_mhz, int32_t *chain_gain, int num_chain_gain);

extern  HAL_BOOL ar5416Reset11n(struct ath_hal *ah, HAL_OPMODE opmode,
        HAL_CHANNEL *chan, HAL_BOOL bChannelChange, HAL_STATUS *status);
extern void ar5416SetCoverageClass(struct ath_hal *ah, u_int8_t coverageclass, int now);

extern void ar5416GetChannelCenters(struct ath_hal *ah,
                                    HAL_CHANNEL_INTERNAL *chan,
                                    CHAN_CENTERS *centers);
extern u_int16_t ar5416GetCtlCenter(struct ath_hal *ah,
                                        HAL_CHANNEL_INTERNAL *chan);
extern u_int16_t ar5416GetExtCenter(struct ath_hal *ah,
                                        HAL_CHANNEL_INTERNAL *chan);
extern u_int32_t ar5416GetMibCycleCountsPct(struct ath_hal *, u_int32_t*, u_int32_t*, u_int32_t*);

extern void ar5416DmaRegDump(struct ath_hal *);
extern HAL_BOOL ar5416Set11nRxRifs(struct ath_hal *ah, HAL_BOOL enable);
extern HAL_BOOL ar5416SetRifsDelay(struct ath_hal *ah, HAL_BOOL enable);
extern HAL_BOOL ar5416DetectBbHang(struct ath_hal *ah);
extern HAL_BOOL ar5416DetectMacHang(struct ath_hal *ah);

#ifdef ATH_BT_COEX
extern void ar5416SetBTCoexInfo(struct ath_hal *ah, HAL_BT_COEX_INFO *btinfo);
extern void ar5416BTCoexConfig(struct ath_hal *ah, HAL_BT_COEX_CONFIG *btconf);
extern void ar5416BTCoexSetQcuThresh(struct ath_hal *ah, int qnum);
extern void ar5416BTCoexSetWeights(struct ath_hal *ah, u_int32_t stompType);
extern void ar5416BTCoexSetupBmissThresh(struct ath_hal *ah, u_int32_t thresh);
extern void ar5416BTCoexSetParameter(struct ath_hal *ah, u_int32_t type, u_int32_t value);
extern void ar5416BTCoexDisable(struct ath_hal *ah);
extern int ar5416BTCoexEnable(struct ath_hal *ah);
extern void ar5416InitBTCoex(struct ath_hal *ah);
extern u_int32_t ar5416GetBTActiveGpio(struct ath_hal *ah, u_int32_t reg);
extern u_int32_t ar5416GetWlanActiveGpio(struct ath_hal *ah, u_int32_t reg,u_int32_t bOn);
#endif

extern int ar5416AllocGenericTimer(struct ath_hal *ah, HAL_GEN_TIMER_DOMAIN tsf);
extern void ar5416FreeGenericTimer(struct ath_hal *ah, int index);
extern void ar5416StartGenericTimer(struct ath_hal *ah, int index, u_int32_t timer_next, u_int32_t timer_period);
extern void ar5416StopGenericTimer(struct ath_hal *ah, int index);
extern void ar5416GetGenTimerInterrupts(struct ath_hal *ah, u_int32_t *trigger, u_int32_t *thresh);
extern void ar5416StartTsf2(struct ath_hal *ah);

extern void ar5416ChkRSSIUpdateTxPwr(struct ath_hal *ah, int rssi);
extern HAL_BOOL ar5416_is_skip_paprd_by_greentx(struct ath_hal *ah);
extern void ar5416_paprd_dec_tx_pwr(struct ath_hal *ah);

extern int ar5416_getSpurInfo(struct ath_hal * ah, int *enable, int len, u_int16_t *freq);
extern int ar5416_setSpurInfo(struct ath_hal * ah, int enable, int len, u_int16_t *freq);
extern void ar5416WowSetGpioResetLow(struct ath_hal * ah);

extern void ar5416GetMibCycleCounts(struct ath_hal *, HAL_COUNTERS*);
extern void ar5416GetVowStats(struct ath_hal *, HAL_VOWSTATS*, u_int8_t);
#ifdef ATH_CCX
extern u_int8_t ar5416GetCcaThreshold(struct ath_hal *ah);
#endif
#if ATH_TxBF_DYNAMIC_LOF_ON_N_CHAIN_MASK
extern HAL_BOOL ar5416_txbf_loforceon_update(struct ath_hal *ah,HAL_BOOL loforcestate);
#endif
/* EEPROM interface functions */
/* Common Interface functions */
extern  HAL_STATUS ar5416EepromAttach(struct ath_hal *);
extern  u_int32_t ar5416EepromGet(struct ath_hal_5416 *ahp, EEPROM_PARAM param);

extern  u_int32_t ar5416INIFixup(struct ath_hal *ah,
                                    ar5416_eeprom_t *pEepData,
                                    u_int32_t reg, 
                                    u_int32_t val);

extern  HAL_STATUS ar5416EepromSetTransmitPower(struct ath_hal *ah,
                     ar5416_eeprom_t *pEepData, HAL_CHANNEL_INTERNAL *chan,
                     u_int16_t cfgCtl, u_int16_t twiceAntennaReduction,
                     u_int16_t twiceMaxRegulatoryPower, u_int16_t powerLimit);
extern  void ar5416EepromSetAddac(struct ath_hal *, HAL_CHANNEL_INTERNAL *);
extern  HAL_BOOL ar5416EepromSetParam(struct ath_hal *ah, EEPROM_PARAM param, u_int32_t value);
extern  HAL_BOOL ar5416EepromSetBoardValues(struct ath_hal *, HAL_CHANNEL_INTERNAL *);
extern  HAL_BOOL ar5416EepromRead(struct ath_hal *, u_int off, u_int16_t *data);
extern  HAL_BOOL ar5416FlashRead(struct ath_hal *, u_int off, u_int16_t *data);
#if AH_SUPPORT_WRITE_EEPROM
extern  HAL_BOOL ar5416EepromWrite(struct ath_hal *, u_int off, u_int16_t data);
#endif
extern  HAL_BOOL ar5416FlashWrite(struct ath_hal *, u_int off, u_int16_t data);
extern  u_int ar5416EepromDumpSupport(struct ath_hal *ah, void **pp_e);
extern  u_int8_t ar5416EepromGetNumAntConfig(struct ath_hal_5416 *ahp, HAL_FREQ_BAND freq_band);
extern  HAL_STATUS ar5416EepromGetAntCfg(struct ath_hal_5416 *ahp, HAL_CHANNEL_INTERNAL *chan,
                                     u_int8_t index, u_int32_t *config);
extern u_int8_t* ar5416EepromGetCustData(struct ath_hal_5416 *ahp);

/* The HAL may support just one kind of EEPROM map, or both.
 */
#if (!defined(AH_SUPPORT_EEPROM_DEF)) && (!defined(AH_SUPPORT_EEPROM_4K))
#error Must define AH_SUPPORT_EEPROM_DEF, AH_SUPPORT_EEPROM_4K, or both
#endif


/* EEPROM Default MAP - interface functions */ 
#ifdef AH_SUPPORT_EEPROM_DEF
extern  u_int32_t ar5416EepromDefGet(struct ath_hal_5416 *ahp,
        EEPROM_PARAM param);
extern  HAL_BOOL ar5416EepromDefSetParam(struct ath_hal *ah,
        EEPROM_PARAM param, u_int32_t value);
extern  HAL_BOOL ar5416EepromDefSetBoardValues(struct ath_hal *,
        HAL_CHANNEL_INTERNAL *);
extern  u_int32_t ar5416EepromDefINIFixup(struct ath_hal *ah,
                                    ar5416_eeprom_def_t *pEepData,
                                    u_int32_t reg, 
                                    u_int32_t val);
extern  HAL_STATUS ar5416EepromDefSetTransmitPower(struct ath_hal *ah,
                     ar5416_eeprom_def_t *pEepData, HAL_CHANNEL_INTERNAL *chan,
                     u_int16_t cfgCtl, u_int16_t twiceAntennaReduction,
                     u_int16_t twiceMaxRegulatoryPower, u_int16_t powerLimit);
extern  void ar5416EepromDefSetAddac(struct ath_hal *, HAL_CHANNEL_INTERNAL *);
extern  u_int8_t ar5416EepromDefGetNumAntConfig(struct ath_hal_5416 *ahp,
        HAL_FREQ_BAND freq_band);
extern  HAL_STATUS ar5416EepromDefGetAntCfg(struct ath_hal_5416 *ahp,
        HAL_CHANNEL_INTERNAL *chan, u_int8_t index, u_int32_t *config);
extern  HAL_BOOL ar5416FillEepromDef(struct ath_hal *ah);
extern  HAL_STATUS ar5416CheckEepromDef(struct ath_hal *ah);
extern HAL_BOOL ar5416ForceVCS( struct ath_hal *ah);
extern HAL_BOOL ar5416SetDfs3StreamFix(struct ath_hal *ah, u_int32_t val);
extern HAL_BOOL ar5416Get3StreamSignature( struct ath_hal *ah);

#ifdef AH_PRIVATE_DIAG
extern  void ar5416FillEmuEepromDef(struct ath_hal_5416 *ahp);
#else
#define ar5416FillEmuEepromDef(_ahp)
#endif /* AH_PRIVATE_DIAG */

#else
/* The EEPROM default map type is not supported if AH_SUPPORT_EEPROM_DEF
   is undefined.  Complain if the driver tries to use it anyway.
 */
#define ar5416EepromDefNotSupported(ah, func) \
    ar5416FeatureNotSupported(EepromDef, ah, func)

#define ar5416EepromDefGet(ahp, param)                                       \
    ( ar5416EepromDefNotSupported((struct ath_hal *) (ahp), __func__), 0 )
#define ar5416EepromDefSetParam(ah, param, value)                            \
    ( ar5416EepromDefNotSupported(ah, __func__), 0 )
#define ar5416EepromDefSetBoardValues(ah, chan)                              \
    ( ar5416EepromDefNotSupported(ah, __func__), 0 )
#define ar5416EepromDefINIFixup(ah, pEepData, reg, val)                      \
    ( ar5416EepromDefNotSupported(ah, __func__), 0 )
#define ar5416EepromDefSetTransmitPower(                                     \
    ah, pEepData, chan, cfgCtl, twiceAntennaReduction,                       \
    twiceMaxRegulatoryPower, powerLimit)                                     \
    ( ar5416EepromDefNotSupported(ah, __func__), 0 )
#define ar5416EepromDefSetAddac(ah, chan)                                    \
    ( ar5416EepromDefNotSupported(ah, __func__) )
#define ar5416EepromDefGetNumAntConfig(ahp, freq_band)                       \
    ( ar5416EepromDefNotSupported((struct ath_hal *) (ahp), __func__), 0 )
#define ar5416EepromDefGetAntCfg(ahp, chan, index, config)                   \
    ( ar5416EepromDefNotSupported((struct ath_hal *) (ahp), __func__), 0 )
#define ar5416CheckEepromDef(ah)                                             \
    ( ar5416EepromDefNotSupported(ah, __func__), 0 )
#define ar5416FillEmuEepromDef(ahp)                                          \
    ( ar5416EepromDefNotSupported((struct ath_hal *) (ahp), __func__) )
#define ar5416FillEepromDef(ah)                                              \
    ( ar5416EepromDefNotSupported(ah, __func__), 0 )

#endif /* AH_SUPPORT_EEPROM_DEF */


/* EEPROM  4KBits MAP - interface functions */  
#ifdef AH_SUPPORT_EEPROM_4K
extern  u_int32_t ar5416Eeprom4kGet(struct ath_hal_5416 *ahp,
        EEPROM_PARAM param);
extern  HAL_BOOL ar5416Eeprom4kSetParam(struct ath_hal *ah, EEPROM_PARAM param,
        u_int32_t value);
extern  HAL_BOOL ar5416Eeprom4kSetBoardValues(struct ath_hal *,
        HAL_CHANNEL_INTERNAL *);
extern  u_int32_t ar5416Eeprom4kINIFixup(struct ath_hal *ah,
                                    ar5416_eeprom_4k_t *pEepData,
                                    u_int32_t reg, 
                                    u_int32_t val);
extern  HAL_STATUS ar5416Eeprom4kSetTransmitPower(struct ath_hal *ah,
                     ar5416_eeprom_4k_t *pEepData, HAL_CHANNEL_INTERNAL *chan,
                     u_int16_t cfgCtl, u_int16_t twiceAntennaReduction,
                     u_int16_t twiceMaxRegulatoryPower, u_int16_t powerLimit);
extern  void ar5416Eeprom4kSetAddac(struct ath_hal *, HAL_CHANNEL_INTERNAL *);
extern  u_int8_t ar5416Eeprom4kGetNumAntConfig(struct ath_hal_5416 *ahp,
        HAL_FREQ_BAND freq_band);
extern  HAL_STATUS ar5416Eeprom4kGetAntCfg(struct ath_hal_5416 *ahp,
        HAL_CHANNEL_INTERNAL *chan, u_int8_t index, u_int32_t *config);
extern  HAL_STATUS ar5416CheckEeprom4k(struct ath_hal *ah);
extern  HAL_BOOL ar5416FillEeprom4k(struct ath_hal *ah);

#ifdef AH_PRIVATE_DIAG
extern  void ar5416FillEmuEeprom4k(struct ath_hal_5416 *ahp);
#else
#define ar5416FillEmuEeprom4k(_ahp)
#endif /* AH_PRIVATE_DIAG */

#else
/* The EEPROM 4KBits map type is not supported if AH_SUPPORT_EEPROM_4K
   is undefined.  Complain if the driver tries to use it anyway.
 */
#define ar5416Eeprom4kNotSupported(ah, func) \
    ar5416FeatureNotSupported(Eeprom4k, ah, func)

#define ar5416Eeprom4kGet(ahp, param)                                       \
    ( ar5416Eeprom4kNotSupported((struct ath_hal *) (ahp), __func__), 0 )
#define ar5416Eeprom4kSetParam(ah, param, value)                            \
    ( ar5416Eeprom4kNotSupported(ah, __func__), 0 )
#define ar5416Eeprom4kSetBoardValues(ah, chan)                              \
    ( ar5416Eeprom4kNotSupported(ah, __func__), 0 )
#define ar5416Eeprom4kINIFixup(ah, pEepData, reg, val)                      \
    ( ar5416Eeprom4kNotSupported(ah, __func__), 0 )
#define ar5416Eeprom4kSetTransmitPower(                                     \
    ah, pEepData, chan, cfgCtl, twiceAntennaReduction,                      \
    twiceMaxRegulatoryPower, powerLimit)                                    \
    ( ar5416Eeprom4kNotSupported(ah, __func__), 0 )
#define ar5416Eeprom4kSetAddac(ah, chan)                                    \
    ( ar5416Eeprom4kNotSupported(ah, __func__) )
#define ar5416Eeprom4kGetNumAntConfig(ahp, freq_band)                       \
    ( ar5416Eeprom4kNotSupported((struct ath_hal *) (ahp), __func__), 0 )
#define ar5416Eeprom4kGetAntCfg(ahp, chan, index, config)                   \
    ( ar5416Eeprom4kNotSupported((struct ath_hal *) (ahp), __func__), 0 )
#define ar5416CheckEeprom4k(ah)                                             \
    ( ar5416Eeprom4kNotSupported(ah, __func__), 0 )
#define ar5416FillEmuEeprom4k(ahp)                                          \
    ( ar5416Eeprom4kNotSupported((struct ath_hal *) (ahp), __func__) )
#define ar5416FillEeprom4k(ah)                                              \
    ( ar5416Eeprom4kNotSupported(ah, __func__), 0 )

#endif /* AH_SUPPORT_EEPROM_4K */


/* EEPROM  AR9287 MAP - interface functions */  
#ifdef AH_SUPPORT_EEPROM_AR9287
extern  u_int32_t ar9287EepromGet(struct ath_hal_5416 *ahp,
        EEPROM_PARAM param);
extern  HAL_BOOL ar9287EepromSetParam(struct ath_hal *ah,
        EEPROM_PARAM param, u_int32_t value);
extern  HAL_BOOL ar9287EepromSetBoardValues(struct ath_hal *,
        HAL_CHANNEL_INTERNAL *);
extern  u_int32_t ar9287EepromINIFixup(struct ath_hal *ah,
                                    ar9287_eeprom_t *pEepData,
                                    u_int32_t reg, 
                                    u_int32_t val);
extern  HAL_STATUS ar9287EepromSetTransmitPower(struct ath_hal *ah,
                     ar9287_eeprom_t *pEepData, HAL_CHANNEL_INTERNAL *chan,
                     u_int16_t cfgCtl, u_int16_t twiceAntennaReduction,
                     u_int16_t twiceMaxRegulatoryPower, u_int16_t powerLimit);
extern  void ar9287EepromSetAddac(struct ath_hal *, HAL_CHANNEL_INTERNAL *);
extern  u_int8_t ar9287EepromGetNumAntConfig(struct ath_hal_5416 *ahp,
        HAL_FREQ_BAND freq_band);
extern  HAL_STATUS ar9287EepromGetAntCfg(struct ath_hal_5416 *ahp,
        HAL_CHANNEL_INTERNAL *chan, u_int8_t index, u_int32_t *config);
extern  HAL_STATUS ar9287CheckEeprom(struct ath_hal *ah);
extern  HAL_BOOL ar9287FillEeprom(struct ath_hal *ah);

#ifdef AH_PRIVATE_DIAG
extern  void ar9287FillEmuEeprom(struct ath_hal_5416 *ahp);
#else
#define ar9287FillEmuEeprom(_ahp)
#endif /* AH_PRIVATE_DIAG */
#ifdef ART_BUILD
extern void ar9287EepromLoadDefaults(struct ath_hal *ah);
#else
#define ar9287EepromLoadDefaults(ah)
#endif /* ART_BUILD */
extern u_int16_t *ar9287RegulatoryDomainGet(struct ath_hal *ah);
#else
/* The EEPROM Ar9287 map type is not supported if AH_SUPPORT_EEPROM_4K
   is undefined.  Complain if the driver tries to use it anyway.
 */
#define ar9287EepromNotSupported(ah, func) \
    ar5416FeatureNotSupported(EepromAr9287, ah, func)

#define ar9287EepromGet(ahp, param)                                       \
    ( ar9287EepromNotSupported((struct ath_hal *) (ahp), __func__), 0 )
#define ar9287EepromSetParam(ah, param, value)                            \
    ( ar9287EepromNotSupported(ah, __func__), 0 )
#define ar9287EepromSetBoardValues(ah, chan)                              \
    ( ar9287EepromNotSupported(ah, __func__), 0 )
#define ar9287EepromINIFixup(ah, pEepData, reg, val)                      \
    ( ar9287EepromNotSupported(ah, __func__), 0 )
#define ar9287EepromSetTransmitPower(                                     \
    ah, pEepData, chan, cfgCtl, twiceAntennaReduction,                      \
    twiceMaxRegulatoryPower, powerLimit)                                    \
    ( ar9287EepromNotSupported(ah, __func__), 0 )
#define ar9287EepromSetAddac(ah, chan)                                    \
    ( ar9287EepromNotSupported(ah, __func__) )
#define ar9287EepromGetNumAntConfig(ahp, freq_band)                       \
    ( ar9287EepromNotSupported((struct ath_hal *) (ahp), __func__), 0 )
#define ar9287EepromGetAntCfg(ahp, chan, index, config)                   \
    ( ar9287EepromNotSupported((struct ath_hal *) (ahp), __func__), 0 )
#define ar9287CheckEeprom(ah)                                             \
    ( ar9287EepromNotSupported(ah, __func__), 0 )
#define ar9287FillEmuEeprom(ahp)                                          \
    ( ar9287EepromNotSupported((struct ath_hal *) (ahp), __func__) )
#define ar9287FillEeprom(ah)                                              \
    ( ar9287EepromNotSupported(ah, __func__), 0 )
#define ar9287EepromLoadDefaults(ah)                                      \
    (ar9287EepromNotSupported(ah, __func__))
#define  ar9287RegulatoryDomainGet(ah)                                  \
    (ar9287EepromNotSupported(ah, __func__), 0)
#endif /* AH_SUPPORT_EEPROM_AR9287 */


/* Common EEPROM Help function */
extern void ar5416GetTargetPowers(struct ath_hal *ah,
    HAL_CHANNEL_INTERNAL *chan, CAL_TARGET_POWER_HT *powInfo,
    u_int16_t numChannels, CAL_TARGET_POWER_HT *pNewPower,
    u_int16_t numRates, HAL_BOOL isHt40Target);
extern void ar5416GetTargetPowersLeg(struct ath_hal *ah,
    HAL_CHANNEL_INTERNAL *chan, CAL_TARGET_POWER_LEG *powInfo,
    u_int16_t numChannels, CAL_TARGET_POWER_LEG *pNewPower,
    u_int16_t numRates, HAL_BOOL isExtTarget);
extern void ar5416SetImmunity(struct ath_hal *ah, HAL_BOOL enable);
extern void ar5416GetHwHangs(struct ath_hal *ah, hal_hw_hangs_t *hangs);
#if ATH_ANT_DIV_COMB
extern void ar5416AntDivCombGetConfig(struct ath_hal *ah, HAL_ANT_COMB_CONFIG* divCombConf);
extern void ar5416AntDivCombSetConfig(struct ath_hal *ah, HAL_ANT_COMB_CONFIG* divCombConf);
#endif
#if ATH_SUPPORT_WIRESHARK
#include "ah_radiotap.h"
extern void ar5416FillRadiotapHdr(struct ath_hal *ah,
    struct ah_rx_radiotap_header *rh, struct ah_ppi_data *ppi, struct ath_desc *ds, void *buf_addr);
#endif /* ATH_SUPPORT_WIRESHARK */

        /* TX common functions */

extern  HAL_BOOL ar5416UpdateTxTrigLevel(struct ath_hal *,
        HAL_BOOL IncTrigLevel);
extern  u_int16_t ar5416GetTxTrigLevel(struct ath_hal *);
extern  HAL_BOOL ar5416SetTxQueueProps(struct ath_hal *ah, int q,
        const HAL_TXQ_INFO *q_info);
extern  HAL_BOOL ar5416GetTxQueueProps(struct ath_hal *ah, int q,
        HAL_TXQ_INFO *q_info);
extern  int ar5416SetupTxQueue(struct ath_hal *ah, HAL_TX_QUEUE type,
        const HAL_TXQ_INFO *q_info);
extern  HAL_BOOL ar5416ReleaseTxQueue(struct ath_hal *ah, u_int q);
extern  HAL_BOOL ar5416ResetTxQueue(struct ath_hal *ah, u_int q);
extern  u_int32_t ar5416GetTxDP(struct ath_hal *ah, u_int q);
extern  HAL_BOOL ar5416SetTxDP(struct ath_hal *ah, u_int q, u_int32_t txdp);
extern  HAL_BOOL ar5416StartTxDma(struct ath_hal *ah, u_int q);
extern  u_int32_t ar5416NumTxPending(struct ath_hal *ah, u_int q);
extern  HAL_BOOL ar5416StopTxDma(struct ath_hal *ah, u_int q, u_int timeout);
extern  HAL_BOOL ar5416AbortTxDma(struct ath_hal *ah);
extern  void ar5416GetTxIntrQueue(struct ath_hal *ah, u_int32_t *);

extern  void ar5416TxReqIntrDesc(struct ath_hal *ah, void *ds);
extern  HAL_BOOL ar5416SetupTxDesc(struct ath_hal *ah, struct ath_desc *ds,
        u_int pktLen, u_int hdrLen, HAL_PKT_TYPE type, u_int txPower,
        u_int txRate0, u_int txTries0,
        u_int key_ix, u_int antMode, u_int flags,
        u_int rts_cts_rate, u_int rts_cts_duration,
        u_int compicvLen, u_int compivLen, u_int comp);
extern  HAL_BOOL ar5416SetupXTxDesc(struct ath_hal *, struct ath_desc *,
        u_int txRate1, u_int txRetries1,
        u_int txRate2, u_int txRetries2,
        u_int txRate3, u_int txRetries3);
extern  HAL_BOOL ar5416FillTxDesc(struct ath_hal *ah, void *ds, dma_addr_t *bufAddr,
        u_int32_t *seg_len, u_int desc_id, u_int qcu, HAL_KEY_TYPE keyType, HAL_BOOL first_seg,
        HAL_BOOL last_seg, const void *ds0);
extern  void ar5416SetDescLink(struct ath_hal *, void *ds, u_int32_t link);
extern  void ar5416GetDescLinkPtr(struct ath_hal *ah, void *ds, u_int32_t **link);
extern  void ar5416ClearTxDescStatus(struct ath_hal *ah, void *ds);
#ifdef ATH_SWRETRY
extern void ar5416ClearDestMask(struct ath_hal *ah, void *ds);
#endif
extern  HAL_STATUS ar5416ProcTxDesc(struct ath_hal *ah, void *);
extern  u_int32_t ar5416CalcTxAirtime(struct ath_hal *ah, void *, struct ath_tx_status *,
        HAL_BOOL comp_wastedt, u_int8_t nbad, u_int8_t nframes);
extern void ar5416Set11nTxDesc(struct ath_hal *ah, void *ds,
       u_int pktLen, HAL_PKT_TYPE type, u_int txPower,
       u_int key_ix, HAL_KEY_TYPE keyType, u_int flags);
#if UNIFIED_SMARTANTENNA
extern void ar5416Set11nRateScenario(struct ath_hal *ah, void *ds,
        void *lastds, u_int dur_update_en, u_int rts_cts_rate, u_int rts_cts_duration, HAL_11N_RATE_SERIES series[],
       u_int nseries, u_int flags, u_int32_t smart_ant_status, u_int32_t antenna_array[]);
#else
extern void ar5416Set11nRateScenario(struct ath_hal *ah, void *ds,
        void *lastds, u_int dur_update_en, u_int rts_cts_rate, u_int rts_cts_duration, HAL_11N_RATE_SERIES series[],
       u_int nseries, u_int flags, u_int32_t smartAntenna);
#endif

extern void ar5416Set11nAggrFirst(struct ath_hal *ah, void *ds,
       u_int aggr_len);
extern void ar5416Set11nAggrMiddle(struct ath_hal *ah, void *ds,
       u_int num_delims);
extern void ar5416Set11nAggrLast(struct ath_hal *ah, void *ds);
extern void ar5416Clr11nAggr(struct ath_hal *ah, void *ds);
extern void ar5416Set11nBurstDuration(struct ath_hal *ah, void *ds,
       u_int burst_duration);
extern void ar5416Set11nRifsBurstMiddle(struct ath_hal *ah, void *ds);
extern void ar5416Set11nRifsBurstLast(struct ath_hal *ah, void *ds);
extern void ar5416Clr11nRifsBurst(struct ath_hal *ah, void *ds);
extern void ar5416Set11nAggrRifsBurst(struct ath_hal *ah, void *ds);
extern void ar5416Set11nVirtualMoreFrag(struct ath_hal *ah, void *ds,
       u_int vmf);
#ifdef AH_PRIVATE_DIAG
extern void ar5416_ContTxMode(struct ath_hal *ah, void *ds, int mode);
#endif

	/* RX common functions */

extern  u_int32_t ar5416GetRxDP(struct ath_hal *ath, HAL_RX_QUEUE qtype);
extern  void ar5416SetRxDP(struct ath_hal *ah, u_int32_t rxdp, HAL_RX_QUEUE qtype);
extern  void ar5416EnableReceive(struct ath_hal *ah);
extern  HAL_BOOL ar5416StopDmaReceive(struct ath_hal *ah, u_int timeout);
extern  void ar5416StartPcuReceive(struct ath_hal *ah, HAL_BOOL);
extern  void ar5416StopPcuReceive(struct ath_hal *ah);
extern  void ar5416SetMulticastFilter(struct ath_hal *ah,
        u_int32_t filter0, u_int32_t filter1);
extern  u_int32_t ar5416GetRxFilter(struct ath_hal *ah);
extern  void ar5416SetRxFilter(struct ath_hal *ah, u_int32_t bits);
extern  HAL_BOOL ar5416SetRxSelEvm(struct ath_hal *ah, HAL_BOOL, HAL_BOOL);
extern	HAL_BOOL ar5416SetRxAbort(struct ath_hal *ah, HAL_BOOL);
extern  HAL_BOOL ar5416SetupRxDesc(struct ath_hal *,
        struct ath_desc *, u_int32_t size, u_int flags);
extern  HAL_STATUS ar5416ProcRxDesc(struct ath_hal *ah,
        struct ath_desc *, u_int32_t, struct ath_desc *, u_int64_t, struct ath_rx_status *);
extern  HAL_STATUS ar5416GetRxKeyIdx(struct ath_hal *ah,
        struct ath_desc *, u_int8_t *, u_int8_t *);
extern  HAL_STATUS ar5416ProcRxDescFast(struct ath_hal *ah, struct ath_desc *,
        u_int32_t, struct ath_desc *, struct ath_rx_status *, void *);

extern  void ar5416PromiscMode(struct ath_hal *ah, HAL_BOOL enable);
extern  void ar5416ReadPktlogReg(struct ath_hal *ah, u_int32_t *, u_int32_t *, u_int32_t *, u_int32_t *);
extern  void ar5416WritePktlogReg(struct ath_hal *ah, HAL_BOOL , u_int32_t , u_int32_t , u_int32_t , u_int32_t );
extern  HAL_STATUS ar5416SetProxySTA(struct ath_hal *ah, HAL_BOOL enable);
extern void ar5416DisablePhyRestart(struct ath_hal *ah, int disable_phy_restart);
extern  HAL_BOOL ar5416RegulatoryDomainOverride(struct ath_hal *ah, u_int16_t regdmn);
extern void ar5416_enable_keysearch_always(struct ath_hal *ah, int enable);
#ifdef ATH_TX99_DIAG
extern void ar5416_tx99_chainmsk_setup(struct ath_hal *ah, int tx_chainmask);
extern void ar5416_tx99_set_single_carrier(struct ath_hal *ah, int tx_chain_mask, int chtype);
extern void ar5416_tx99_channel_pwr_update(struct ath_hal *ah, HAL_CHANNEL *c, u_int32_t txpower);
extern void ar5416_tx99_start(struct ath_hal *ah, u_int8_t *data);
extern void ar5416_tx99_stop(struct ath_hal *ah);
#endif /* ATH_TX99_DIAG */
extern void ar5416ClearMibCounters(struct ath_hal *ah);
extern void ar5416_reset_nav(struct ath_hal *ah);
extern void ar5416_factory_defaults(struct ath_hal *ah);
#endif  /* _ATH_AR5416_H_ */
