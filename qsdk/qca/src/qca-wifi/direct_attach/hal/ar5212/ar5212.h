/*
* Copyright (c) 2011,2017-2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
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
 */

#ifndef _ATH_AR5212_H_
#define _ATH_AR5212_H_

#include "ah_eeprom.h"
#include "ar5212/ar5212radar.h"

#define	AR5212_MAGIC	0x19541014

/* DCU Transmit Filter macros */
#define CALC_MMR(dcu, idx) \
	( (4 * dcu) + (idx < 32 ? 0 : (idx < 64 ? 1 : (idx < 96 ? 2 : 3))) )
#define TXBLK_FROM_MMR(mmr) \
	(AR_D_TXBLK_BASE + ((mmr & 0x1f) << 6) + ((mmr & 0x20) >> 3))
#define CALC_TXBLK_ADDR(dcu, idx)	(TXBLK_FROM_MMR(CALC_MMR(dcu, idx)))
#define CALC_TXBLK_VALUE(idx)		(1 << (idx & 0x1f))

/* MAC register values */

#define INIT_BEACON_CONTROL \
	((INIT_RESET_TSF << 24)  | (INIT_BEACON_EN << 23) | \
	  (INIT_TIM_OFFSET << 16) | INIT_BEACON_PERIOD)

#define INIT_CONFIG_STATUS	0x00000000
#define INIT_RSSI_THR		0x00000781	/* Missed beacon counter initialized to 0x7 (max is 0xff) */
#define INIT_IQCAL_LOG_COUNT_MAX	0xF
#define INIT_BCON_CNTRL_REG	0x00000000

#define INIT_USEC		40
#define HALF_RATE_USEC		19 /* ((40 / 2) - 1 ) */
#define QUARTER_RATE_USEC	9  /* ((40 / 4) - 1 ) */

#define RX_NON_FULL_RATE_LATENCY	63
#define TX_HALF_RATE_LATENCY		108
#define TX_QUARTER_RATE_LATENCY		216

#define IFS_SLOT_FULL_RATE	0x168 /* 9 us half, 40 MHz core clock (9*40) */
#define IFS_SLOT_HALF_RATE	0x104 /* 13 us half, 20 MHz core clock (13*20) */
#define IFS_SLOT_QUARTER_RATE	0xD2 /* 21 us quarter, 10 MHz core clock (21*10) */
#define IFS_EIFS_FULL_RATE	0xE60 /* (74 + (2 * 9)) * 40MHz core clock */
#define IFS_EIFS_HALF_RATE	0xDAC /* (149 + (2 * 13)) * 20MHz core clock */
#define IFS_EIFS_QUARTER_RATE	0xD48 /* (298 + (2 * 21)) * 10MHz core clock */

#define ACK_CTS_TIMEOUT_11A	0x3E8 /* ACK timeout in 11a core clocks */

/* Tx frame start to tx data start delay */
#define TX_FRAME_D_START_HALF_RATE 	0xc
#define TX_FRAME_D_START_QUARTER_RATE 	0xd

/*
 * Various fifo fill before Tx start, in 64-byte units
 * i.e. put the frame in the air while still DMAing
 */
#define MIN_TX_FIFO_THRESHOLD	0x1
#define MAX_TX_FIFO_THRESHOLD	((IEEE80211_MAX_LEN / 64) + 1)
#define INIT_TX_FIFO_THRESHOLD	MIN_TX_FIFO_THRESHOLD

/* compression definitions */
#define HAL_DECOMP_MASK_SIZE            128

/*
 * Gain support.
 */
#define	NUM_CORNER_FIX_BITS		4
#define	NUM_CORNER_FIX_BITS_5112	7
#define	DYN_ADJ_UP_MARGIN		15
#define	DYN_ADJ_LO_MARGIN		20
#define	PHY_PROBE_CCK_CORRECTION	5
#define	CCK_OFDM_GAIN_DELTA		15

enum GAIN_PARAMS {
	GP_TXCLIP,
	GP_PD90,
	GP_PD84,
	GP_GSEL,
};

enum GAIN_PARAMS_5112 {
	GP_MIXGAIN_OVR,
	GP_PWD_138,
	GP_PWD_137,
	GP_PWD_136,
	GP_PWD_132,
	GP_PWD_131,
	GP_PWD_130,
};

typedef struct _gainOptStep {
	int16_t	paramVal[NUM_CORNER_FIX_BITS_5112];
	int32_t	stepGain;
	int8_t	stepName[16];
} GAIN_OPTIMIZATION_STEP;

typedef struct {
	u_int32_t	numStepsInLadder;
	u_int32_t	defaultStepNum;
	GAIN_OPTIMIZATION_STEP optStep[10];
} GAIN_OPTIMIZATION_LADDER;

typedef struct {
	u_int32_t	currStepNum;
	u_int32_t	currGain;
	u_int32_t	targetGain;
	u_int32_t	loTrig;
	u_int32_t	hiTrig;
	u_int32_t	gainFCorrection;
	u_int32_t	active;
	const GAIN_OPTIMIZATION_STEP *currStep;
} GAIN_VALUES;

/* RF HAL structures */
typedef struct RfHalFuncs {
	void	  (*rfDetach)(struct ath_hal *ah);
	void	  (*writeRegs)(struct ath_hal *,
			u_int modeIndex, u_int freqIndex, int regWrites);
	u_int32_t *(*getRfBank)(struct ath_hal *ah, int bank);
	HAL_BOOL  (*setChannel)(struct ath_hal *, HAL_CHANNEL_INTERNAL *);
	HAL_BOOL  (*setRfRegs)(struct ath_hal *,
		      HAL_CHANNEL_INTERNAL *, u_int16_t modesIndex,
		      u_int16_t *rfXpdGain);
	HAL_BOOL  (*setPowerTable)(struct ath_hal *ah,
		      int16_t *minPower, int16_t *maxPower,
		      HAL_CHANNEL_INTERNAL *, u_int16_t *rfXpdGain);
	HAL_BOOL  (*getChipPowerLim)(struct ath_hal *ah, HAL_CHANNEL *chans,
				       u_int32_t nchancs);
	int16_t	  (*getNfAdjust)(struct ath_hal *, const HAL_CHANNEL_INTERNAL*);
} RF_HAL_FUNCS;

/*
 * Per-channel ANI state private to the driver.
 */
struct ar5212AniState {
	HAL_CHANNEL	c;

	/*
	 * Max spur immunity level needs to be set to different levels
	 * depending on whether we have a venice chip or something later
	 * like Griffin or Cobra, Spider, etc... Not sure if Eagle should
	 * follow venice values or Griffin values....
	 *
	 */

	u_int8_t	maxSpurImmunity;
	u_int8_t	noiseImmunityLevel;
	u_int8_t	spurImmunityLevel;
	u_int8_t	firstepLevel;
	u_int8_t	ofdmWeakSigDetectOff;
	u_int8_t	cckWeakSigThreshold;

	/* Thresholds */
	u_int32_t	listenTime;
	u_int32_t	ofdmTrigHigh;
	u_int32_t	ofdmTrigLow;
	int32_t		cckTrigHigh;
	int32_t		cckTrigLow;
	int32_t		rssiThrLow;
	int32_t		rssiThrHigh;

    /* Current RSSI */
	int32_t		rssi;

	u_int32_t	noiseFloor;	/* The current noise floor */
	u_int32_t	txFrameCount;	/* Last txFrameCount */
	u_int32_t	rxFrameCount;	/* Last rx Frame count */
	u_int32_t	cycleCount;	/* Last cycleCount (can detect wrap-around) */
	u_int32_t	ofdmPhyErrCount;/* OFDM err count since last reset */
	u_int32_t	cckPhyErrCount;	/* CCK err count since last reset */
	u_int32_t	ofdmPhyErrBase;	/* Base value for ofdm err counter */
	u_int32_t	cckPhyErrBase;	/* Base value for cck err counters */
	int16_t		pktRssi[2];	/* Average rssi of pkts for 2 antennas */
	int16_t		ofdmErrRssi[2];	/* Average rssi of ofdm phy errs for 2 ant */
	int16_t		cckErrRssi[2];	/* Average rssi of cck phy errs for 2 ant */
};

#define AR5212_ANI_POLLINTERVAL    100    /* 100 milliseconds between ANI poll */

#define AR5212_CHANNEL_SWITCH_TIME_USEC  4000 /* Typical channel change time in microseconds */

#define	HAL_PROCESS_ANI		0x00000001	/* ANI state setup */
#define HAL_RADAR_EN		0x80000000	/* Radar detect is capable */
#define HAL_AR_EN		0x40000000	/* AR detect is capable */

#define DO_ANI(ah) \
	((AH5212(ah)->ah_procPhyErr & HAL_PROCESS_ANI))

struct ar5212Stats {
	u_int32_t	ast_ani_niup;	/* ANI increased noise immunity */
	u_int32_t	ast_ani_nidown;	/* ANI decreased noise immunity */
	u_int32_t	ast_ani_spurup;	/* ANI increased spur immunity */
	u_int32_t	ast_ani_spurdown;/* ANI descreased spur immunity */
	u_int32_t	ast_ani_ofdmon;	/* ANI OFDM weak signal detect on */
	u_int32_t	ast_ani_ofdmoff;/* ANI OFDM weak signal detect off */
	u_int32_t	ast_ani_cckhigh;/* ANI CCK weak signal threshold high */
	u_int32_t	ast_ani_ccklow;	/* ANI CCK weak signal threshold low */
	u_int32_t	ast_ani_stepup;	/* ANI increased first step level */
	u_int32_t	ast_ani_stepdown;/* ANI decreased first step level */
	u_int32_t	ast_ani_ofdmerrs;/* ANI cumulative ofdm phy err count */
	u_int32_t	ast_ani_cckerrs;/* ANI cumulative cck phy err count */
	u_int32_t	ast_ani_reset;	/* ANI parameters zero'd for non-STA */
	u_int32_t	ast_ani_lzero;	/* ANI listen time forced to zero */
	u_int32_t	ast_ani_lneg;	/* ANI listen time calculated < 0 */
	HAL_MIB_STATS	ast_mibstats;	/* MIB counter stats */
	HAL_NODE_STATS	ast_nodestats;	/* Latest rssi stats from driver */
};

struct ath_hal_5212 {
	struct ath_hal_private_tables  ah_priv; /* base class */

	/*
	 * Information retrieved from EEPROM.
	 */
	HAL_EEPROM	ah_eeprom;

	GAIN_VALUES	ah_gainValues;

	u_int8_t	ah_macaddr[IEEE80211_ADDR_LEN];
	u_int8_t	ah_bssid[IEEE80211_ADDR_LEN];
	u_int8_t	ah_bssidmask[IEEE80211_ADDR_LEN];
        u_int16_t       ah_assocId;
	
	int16_t		ah_curchanRadIndex;	/* cur. channel radar index */

	/*
	 * Runtime state.
	 */
	u_int32_t	ah_maskReg;		/* copy of AR_IMR */
	struct ar5212Stats ah_stats;		/* various statistics */
	RF_HAL_FUNCS	ah_rfHal;
	u_int32_t	ah_txDescMask;		/* mask for TXDESC */
	u_int32_t	ah_txOkInterruptMask;
	u_int32_t	ah_txErrInterruptMask;
	u_int32_t	ah_txDescInterruptMask;
	u_int32_t	ah_txEolInterruptMask;
	u_int32_t	ah_txUrnInterruptMask;
	HAL_TX_QUEUE_INFO ah_txq[HAL_NUM_TX_QUEUES];
	HAL_POWER_MODE	ah_powerMode;
	HAL_BOOL	ah_chipFullSleep;
	u_int32_t	ah_atimWindow;
	HAL_ANT_SETTING ah_diversityControl;	/* antenna setting */
	enum {
		IQ_CAL_INACTIVE,
		IQ_CAL_RUNNING,
		IQ_CAL_DONE
	} ah_bIQCalibration;			/* IQ calibrate state */
	HAL_RFGAIN	ah_rfgainState;		/* RF gain calibrartion state */
	u_int32_t	ah_tx6PowerInHalfDbm;	/* power output for 6Mb tx */
	u_int32_t	ah_staId1Defaults;	/* STA_ID1 default settings */
	u_int32_t	ah_miscMode;		/* MISC_MODE settings */
	u_int32_t	ah_macTPC;		/* tpc register */
	enum {
		AUTO_32KHZ,		/* use it if 32kHz crystal present */
		USE_32KHZ,		/* do it regardless */
		DONT_USE_32KHZ,		/* don't use it regardless */
	} ah_enable32kHzClock;			/* whether to sleep at 32kHz */
	void		*ah_analogBanks;	/* RF register banks */
	u_int32_t	ah_ofdmTxPower;
	int16_t		ah_txPowerIndexOffset;

	u_int		ah_slottime;		/* user-specified slot time */
	u_int		ah_acktimeout;		/* user-specified ack timeout */
	/*
	 * XXX
	 * 11g-specific stuff; belongs in the driver.
	 */
	u_int8_t	ah_gBeaconRate;		/* fixed rate for G beacons */
	u_int8_t	ah_compressSupport;  	/* SuperG compression support */

	/*
	 * RF Silent handling; setup according to the EEPROM.
	 */
	u_int32_t	ah_gpioSelect;		/* GPIO pin to use */
	u_int32_t	ah_polarity;		/* polarity to disable RF */
	u_int32_t	ah_gpioBit;		/* after init, prev value */
	HAL_BOOL	ah_eepEnabled;		/* EEPROM bit for capability */
	/*
	 * ANI & Radar support.
	 */
	u_int32_t	ah_procPhyErr;		/* Process Phy errs */
	HAL_BOOL	ah_hasHwPhyCounters;	/* Hardware has phy counters */
	u_int32_t	ah_aniPeriod;		/* ani update list period */
	struct ar5212AniState	*ah_curani;	/* cached last reference */
	struct ar5212AniState	ah_ani[64];	/* per-channel state */
	struct ar5212RadarState	ah_radar[HAL_NUM_RADAR_STATES];	/* Per-Channel Radar detector state */
	struct ar5212ArState	ah_ar;		/* AR detector state */
	struct ar5212RadarQElem	*ah_arq;	/* AR event queue */
	struct ar5212RadarQInfo	ah_arqInfo;	/* AR event q read/write state */
#ifdef AH_SUPPORT_DFS
	struct ar5212RadarQElem	*ah_radarq;	/* radar event queue */
	struct ar5212RadarQInfo	ah_radarqInfo;	/* radar event q read/write state */
	struct ar5212RadarFilterType *ah_radarf[HAL_MAX_RADAR_TYPES]; /* One filter for each radar pulse type */
	struct ar5212RadarInfo ah_rinfo;	/* State variables for radar processing */
	struct ar5212Bin5Radars *ah_b5radars;   /* array of bin5 radars events*/
	int8_t **ah_radarTable;                 /* Mapping of radar durations to filter types*/	
	struct ar5212RadarNolElem *ah_radarNol;	/* Non occupancy list for radar */
	u_int8_t	ah_dfsDisabled;		/* Is the HAL disabled b/c of lack of dfs processing */
#define	HAL_DFS_DISABLE		0x01		/* Disable HAL */
#define	HAL_DFS_TIMEMARK	0x02		/* Time of first dfs event already marked */

	u_int64_t	ah_dfsMarkTime;
#endif

	/*
	 * Ani tables that change between the 5212 and 5312.
	 * These get set at attach time.
	 * XXX don't belong here
	 * XXX need better explanation
	 */
        int		ah_totalSizeDesired[5];
        int		ah_coarseHigh[5];
        int		ah_coarseLow[5];
        int		ah_firpwr[5];

	/*
	 * Transmit power state.  Note these are maintained
	 * here so they can be retrieved by diagnostic tools.
	 */
	u_int16_t	*ah_pcdacTable;
	u_int		ah_pcdacTableSize;
	u_int16_t	ah_ratesArray[16];
    void        *ah_vpdTable;

	/*
	 * Tx queue interrupt state.
	 */
	u_int32_t	ah_intrTxqs;

    void        *ah_cal_mem;
};

#define	AH5212(_ah)	((struct ath_hal_5212 *)(_ah))

#define	IS_5112(ah) \
	((AH_PRIVATE(ah)->ah_analog_5ghz_rev&0xf0) >= AR_RAD5112_SREV_MAJOR \
	 && (AH_PRIVATE(ah)->ah_analog_5ghz_rev&0xf0) < AR_RAD2316_SREV_MAJOR )
#define IS_5212(ah) \
    (AH_PRIVATE(ah)->ah_mac_version == AR_SREV_VERSION_VENICE)
/* Hainan card and up */
/* XXX: This macro, ported from release 5.2, is comparing ah_mac_rev against */
/*      version constant AR_SREV_VERSION_VENICE. Is this correct?? */
#define IS_5112_REV5_UP(ah) \
    (((AH_PRIVATE(ah)->ah_mac_version) > AR_SREV_VERSION_VENICE) || \
    (IS_5212(ah) && ((AH_PRIVATE(ah)->ah_mac_rev) >= AR_SREV_VERSION_VENICE)))
#define	IS_RAD5112_REV1(ah) \
	(((AH_PRIVATE(ah)->ah_analog_5ghz_rev&0xf0) == AR_RAD5112_SREV_MAJOR) && \
	((AH_PRIVATE(ah)->ah_analog_5ghz_rev&0x0f) < (AR_RAD5112_SREV_2_0&0x0f)))
#define IS_RADX112_REV2(ah) \
        (IS_5112(ah) && \
	  ((AH_PRIVATE(ah)->ah_analog_5ghz_rev == AR_RAD5112_SREV_2_0) || \
	   (AH_PRIVATE(ah)->ah_analog_5ghz_rev == AR_RAD2112_SREV_2_0) || \
	   (AH_PRIVATE(ah)->ah_analog_5ghz_rev == AR_RAD2112_SREV_2_1) || \
	   (AH_PRIVATE(ah)->ah_analog_5ghz_rev == AR_RAD5112_SREV_2_1)))
#define	IS_5312_2_X(ah) \
	(((AH_PRIVATE(ah)->ah_mac_version) == AR_SREV_VERSION_VENICE) && \
	 (((AH_PRIVATE(ah)->ah_mac_rev) == 2) || ((AH_PRIVATE(ah)->ah_mac_rev) == 7)))
#define	IS_2316(ah) \
	((AH_PRIVATE(ah)->ah_mac_version) == AR_SREV_2415)
#define	IS_2413(ah) \
	((AH_PRIVATE(ah)->ah_mac_version) == AR_SREV_2413 || IS_2316(ah))
#define IS_5424(ah) \
 	( (AH_PRIVATE(ah)->ah_mac_version) == AR_SREV_5424 || \
 	( (AH_PRIVATE(ah)->ah_mac_version) == AR_SREV_5413 && \
 	  (AH_PRIVATE(ah)->ah_mac_rev) <=3 && \
 	  (AH_PRIVATE(ah)->ah_is_pci_express) == AH_TRUE) )
#define IS_5413(ah) \
	(((AH_PRIVATE(ah)->ah_mac_version) == AR_SREV_5413) || IS_5424(ah))
#define IS_2425(ah) \
	((AH_PRIVATE(ah)->ah_mac_version) == AR_SREV_2425)
#define IS_HB63(ah)	\
    ((AH_PRIVATE(ah)->ah_flags) & AH_IS_HB63)
#define IS_2417(ah) \
        ((AH_PRIVATE(ah)->ah_mac_version) == AR_SREV_2417)

#define IS_PCIE(ah) (IS_5424((ah)) || IS_2425((ah)))

#define	ar5212RfDetach(ah) do {				\
	if (AH5212(ah)->ah_rfHal.rfDetach != AH_NULL)	\
		AH5212(ah)->ah_rfHal.rfDetach(ah);	\
} while (0)
#define	ar5212GetRfBank(ah, b) \
	AH5212(ah)->ah_rfHal.getRfBank(ah, b)

#define CHECK_CCK(_ah, _chan, _flag) do {                       \
    if ((AH_PRIVATE(_ah)->ah_mac_version) == AR_SREV_2425 ||     \
        (AH_PRIVATE(_ah)->ah_mac_version) == AR_SREV_2417) {     \
            if (((_chan)->channel_flags) & CHANNEL_CCK)  {       \
                    ((_chan)->channel_flags) &= (~CHANNEL_CCK);  \
                    ((_chan)->channel_flags) |= CHANNEL_OFDM;    \
                    (_flag) = AH_TRUE;                          \
            }                                                   \
   }                                                            \
} while (0)

#define UNCHECK_CCK(_ah, _chan, _flag) do {                     \
    if ((AH_PRIVATE(_ah)->ah_mac_version) == AR_SREV_2425 ||     \
        (AH_PRIVATE(_ah)->ah_mac_version) == AR_SREV_2417) {     \
            if ((_flag) == AH_TRUE) {                           \
                    ((_chan)->channel_flags) &= (~CHANNEL_OFDM); \
                    ((_chan)->channel_flags) |= CHANNEL_CCK;     \
                    (_flag) = AH_FALSE;                         \
            }                                                   \
   }                                                            \
} while (0)


#define ar5212EepDataInFlash(_ah) \
    (!AH_PRIVATE(_ah)->ah_flags & AH_USE_EEPROM)


extern	HAL_BOOL ar5111RfAttach(struct ath_hal *, HAL_STATUS *);
extern	HAL_BOOL ar5112RfAttach(struct ath_hal *, HAL_STATUS *);
extern	HAL_BOOL ar2413RfAttach(struct ath_hal *, HAL_STATUS *);
extern  HAL_BOOL ar5413RfAttach(struct ath_hal *, HAL_STATUS *);
extern	HAL_BOOL ar2316RfAttach(struct ath_hal *, HAL_STATUS *);
extern	HAL_BOOL ar2317RfAttach(struct ath_hal *, HAL_STATUS *);
extern	HAL_BOOL ar2425RfAttach(struct ath_hal *, HAL_STATUS *);

struct ath_hal;

extern	u_int32_t ar5212GetRadioRev(struct ath_hal *ah);
extern	struct ath_hal_5212 * ar5212NewState(u_int16_t devid,
        HAL_ADAPTER_HANDLE osdev, HAL_SOFTC sc,
		HAL_BUS_TAG st, HAL_BUS_HANDLE sh, HAL_BUS_TYPE bustype,
		struct hal_reg_parm *hal_conf_parm, HAL_STATUS *status);
extern struct ath_hal * ar5212Attach(u_int16_t devid,
        HAL_ADAPTER_HANDLE osdev, HAL_SOFTC sc,
		HAL_BUS_TAG st, HAL_BUS_HANDLE sh, HAL_BUS_TYPE bustype,
		struct hal_reg_parm *hal_conf_parm, HAL_STATUS *status);
extern	void ar5212Detach(struct ath_hal *ah);
extern  HAL_BOOL ar5212ChipTest(struct ath_hal *ah);
extern  HAL_BOOL ar5212GetChannelEdges(struct ath_hal *ah,
                u_int16_t flags, u_int16_t *low, u_int16_t *high);
extern	HAL_BOOL ar5212FillCapabilityInfo(struct ath_hal *ah);

extern void ar5212BeaconInit(struct ath_hal *ah,
                             u_int32_t next_beacon, u_int32_t beacon_period,
                             u_int32_t beacon_period_fraction, HAL_OPMODE opmode);
extern	void ar5212SetStaBeaconTimers(struct ath_hal *ah,
		const HAL_BEACON_STATE *);

extern  HAL_BOOL ar5212IsInterruptPending(struct ath_hal *ah);
extern  HAL_BOOL ar5212GetPendingInterrupts(struct ath_hal *ah, HAL_INT *,
                                              HAL_INT_TYPE type, u_int8_t, HAL_BOOL);
extern  HAL_INT ar5212GetInterrupts(struct ath_hal *ah);
extern  HAL_INT ar5212SetInterrupts(struct ath_hal *ah, HAL_INT ints, HAL_BOOL);

extern	u_int32_t ar5212GetKeyCacheSize(struct ath_hal *);
extern	HAL_BOOL ar5212IsKeyCacheEntryValid(struct ath_hal *, u_int16_t entry);
extern	HAL_BOOL ar5212ResetKeyCacheEntry(struct ath_hal *ah, u_int16_t entry);
extern	HAL_BOOL ar5212SetKeyCacheEntryMac(struct ath_hal *,
			u_int16_t entry, const u_int8_t *mac);
extern	HAL_BOOL ar5212SetKeyCacheEntry(struct ath_hal *ah, u_int16_t entry,
                       const HAL_KEYVAL *k, const u_int8_t *mac, int xorKey);
#if ATH_SUPPORT_KEYPLUMB_WAR
extern  HAL_BOOL ar5212CheckKeyCacheEntry(struct ath_hal *ah, u_int16_t entry,
                               const HAL_KEYVAL *k, int xorKey);
#endif
extern	void ar5212GetMacAddress(struct ath_hal *ah, u_int8_t *mac);
extern	HAL_BOOL ar5212SetMacAddress(struct ath_hal *ah, const u_int8_t *);
extern	void ar5212GetBssIdMask(struct ath_hal *ah, u_int8_t *mac);
extern	HAL_BOOL ar5212SetBssIdMask(struct ath_hal *, const u_int8_t *);
extern	HAL_BOOL ar5212EepromRead(struct ath_hal *, u_int off, u_int16_t *data);
extern	HAL_BOOL ar5212EepromWrite(struct ath_hal *, u_int off, u_int16_t data);
extern  HAL_BOOL ar5212FlashRead(struct ath_hal *ah, u_int off,
        u_int16_t *data);
extern  HAL_BOOL ar5212FlashWrite(struct ath_hal *ah, u_int off,
        u_int16_t data);
extern	HAL_BOOL ar5212SetRegulatoryDomain(struct ath_hal *ah,
		u_int16_t regDomain, HAL_STATUS *stats);
extern	u_int ar5212GetWirelessModes(struct ath_hal *ah);
extern	void ar5212EnableRfKill(struct ath_hal *);
extern	HAL_BOOL ar5212GpioCfgOutput(struct ath_hal *, u_int32_t gpio, HAL_GPIO_OUTPUT_MUX_TYPE signalType);
extern	HAL_BOOL ar5212GpioCfgOutputLEDoff(struct ath_hal *, u_int32_t gpio, HAL_GPIO_OUTPUT_MUX_TYPE signalType);
extern	HAL_BOOL ar5212GpioCfgInput(struct ath_hal *, u_int32_t gpio);
extern	HAL_BOOL ar5212GpioSet(struct ath_hal *, u_int32_t gpio, u_int32_t val);
extern	u_int32_t ar5212GpioGet(struct ath_hal *ah, u_int32_t gpio);
extern	void ar5212GpioSetIntr(struct ath_hal *ah, u_int, u_int32_t ilevel);
extern	void ar5212SetLedState(struct ath_hal *ah, HAL_LED_STATE state);
extern	void ar5212SetPowerLedState(struct ath_hal *ah, u_int8_t enable);
extern	void ar5212SetNetworkLedState(struct ath_hal *ah, u_int8_t enable);
extern	void ar5212WriteAssocid(struct ath_hal *ah, const u_int8_t *bssid,
		u_int16_t assocId);
extern  void ar5212ForceTSFSync(struct ath_hal *ah, const u_int8_t *bssid, 
                                u_int16_t assocId);
extern  void ar5212SetDcsMode(struct ath_hal *ah, u_int32_t);
extern  u_int32_t ar5212GetDcsMode(struct ath_hal *ah);
extern	u_int32_t ar5212GetTsf32(struct ath_hal *ah);
extern	u_int64_t ar5212GetTsf64(struct ath_hal *ah);
extern	u_int32_t ar5212GetTsf2_32(struct ath_hal *ah);
extern	void ar5212ResetTsf(struct ath_hal *ah);
extern	void ar5212SetBasicRate(struct ath_hal *ah, HAL_RATE_SET *pSet);
extern	u_int32_t ar5212GetRandomSeed(struct ath_hal *ah);
extern	HAL_BOOL ar5212DetectCardPresent(struct ath_hal *ah);
extern	void ar5212UpdateMibMacStats(struct ath_hal *ah);
extern  void ar5212GetMibMacStats(struct ath_hal *ah, HAL_MIB_STATS* stats);
extern	HAL_BOOL ar5212IsJapanChannelSpreadSupported(struct ath_hal *ah);
extern	u_int32_t ar5212GetCurRssi(struct ath_hal *ah);
extern	u_int ar5212GetDefAntenna(struct ath_hal *ah);
extern	void ar5212SetDefAntenna(struct ath_hal *ah, u_int antenna);
extern	HAL_BOOL ar5212SetAntennaSwitch(struct ath_hal *ah,
		HAL_ANT_SETTING settings, HAL_CHANNEL *chan, u_int8_t *, u_int8_t *, u_int8_t *);
extern	HAL_BOOL ar5212IsSleepAfterBeaconBroken(struct ath_hal *ah);
extern	HAL_BOOL ar5212SetSlotTime(struct ath_hal *, u_int);
extern	HAL_BOOL ar5212SetAckTimeout(struct ath_hal *, u_int);
extern	u_int ar5212GetAckTimeout(struct ath_hal *);
void 	ar5212SetCoverageClass(struct ath_hal *, u_int8_t, int);
extern  HAL_STATUS ar5212SetQuiet(struct ath_hal *ah, u_int32_t period, u_int32_t duration, u_int32_t nextStart, HAL_QUIET_FLAG flag);
extern	void ar5212SetPCUConfig(struct ath_hal *);
extern	HAL_BOOL ar5212Use32KHzclock(struct ath_hal *ah, HAL_OPMODE opmode);
extern	void ar5212SetupClock(struct ath_hal *ah, HAL_OPMODE opmode);
extern	void ar5212RestoreClock(struct ath_hal *ah, HAL_OPMODE opmode);
extern	int16_t ar5212GetNfAdjust(struct ath_hal *,
		const HAL_CHANNEL_INTERNAL *);
extern	HAL_STATUS ar5212GetCapability(struct ath_hal *, HAL_CAPABILITY_TYPE,
		u_int32_t, u_int32_t *);
extern	HAL_BOOL ar5212SetCapability(struct ath_hal *, HAL_CAPABILITY_TYPE,
		u_int32_t, u_int32_t, HAL_STATUS *);
extern	HAL_BOOL ar5212GetDiagState(struct ath_hal *ah, int request,
		const void *args, u_int32_t argsize,
		void **result, u_int32_t *resultsize);
extern void ar5212GetDescInfo(struct ath_hal *ah, HAL_DESC_INFO *desc_info);
extern  HAL_STATUS ar5212SelectAntConfig(struct ath_hal *ah, u_int32_t cfg);

extern	HAL_BOOL ar5212SetPowerMode(struct ath_hal *ah, HAL_POWER_MODE mode,
		int set_chip);
extern	void ar5212SetSmPowerMode(struct ath_hal *ah, HAL_SMPS_MODE mode);
#if ATH_WOW
extern  void ar5212WowApplyPattern(struct ath_hal *ah, u_int8_t *p_ath_pattern, u_int8_t *p_ath_mask, int32_t pattern_count, u_int32_t athPatternLen);
//extern  u_int32_t ar5212WowWakeUp(struct ath_hal *ah, u_int8_t  *chipPatternBytes);
extern  u_int32_t ar5212WowWakeUp(struct ath_hal *ah, bool offloadEnable);
extern  HAL_BOOL ar5212WowEnable(struct ath_hal *ah, u_int32_t pattern_enable, u_int32_t timeoutInSeconds, int clearbssid, bool offloadEnable);
#endif
extern	HAL_BOOL ar5212GetPowerStatus(struct ath_hal *ah);

extern  u_int32_t ar5212GetRxDP(struct ath_hal *ath, HAL_RX_QUEUE qtype);
extern  void ar5212SetRxDP(struct ath_hal *ah, u_int32_t rxdp, HAL_RX_QUEUE qtype);
extern  void ar5212EnableReceive(struct ath_hal *ah);
extern  HAL_BOOL ar5212StopDmaReceive(struct ath_hal *ah, u_int timeout);
extern  void ar5212StartPcuReceive(struct ath_hal *ah, HAL_BOOL);
extern  void ar5212StopPcuReceive(struct ath_hal *ah);
extern  void ar5212SetMulticastFilter(struct ath_hal *ah,
                u_int32_t filter0, u_int32_t filter1);
extern  u_int32_t ar5212GetRxFilter(struct ath_hal *ah);
extern  void ar5212SetRxFilter(struct ath_hal *ah, u_int32_t bits);
extern  HAL_BOOL ar5212SetRxAbort(struct ath_hal *ah, HAL_BOOL);
extern  HAL_BOOL ar5212SetupRxDesc(struct ath_hal *,
        struct ath_desc *, u_int32_t size, u_int flags);
extern  HAL_STATUS ar5212ProcRxDesc(struct ath_hal *ah, struct ath_desc *,
                u_int32_t, struct ath_desc *, u_int64_t, struct ath_rx_status *);
extern  HAL_STATUS ar5212GetRxKeyIdx(struct ath_hal *ah, struct ath_desc *,
                u_int8_t *, u_int8_t *);
extern  HAL_STATUS ar5212ProcRxDescFast(struct ath_hal *ah, struct ath_desc *,
        u_int32_t, struct ath_desc *, struct ath_rx_status *, void *);

extern HAL_BOOL ar5212Reset(struct ath_hal *ah, HAL_OPMODE opmode, HAL_CHANNEL *chan, HAL_HT_MACMODE macmode, u_int8_t txchainmask, u_int8_t rxchainmask, HAL_HT_EXTPROTSPACING extprotspacing, HAL_BOOL bChannelChange, HAL_STATUS *status, int is_scan);

extern	void ar5212SetOperatingMode(struct ath_hal *ah, int opmode);
extern	HAL_BOOL ar5212PhyDisable(struct ath_hal *ah);
extern	HAL_BOOL ar5212Disable(struct ath_hal *ah);
extern	HAL_BOOL ar5212ChipReset(struct ath_hal *ah, HAL_CHANNEL *);
extern  HAL_BOOL ar5212PerCalibration(struct ath_hal *ah,  HAL_CHANNEL *chan,
        u_int8_t rxchainmask, HAL_BOOL longcal, HAL_BOOL *isIQdone,
        int is_scan, u_int32_t *sched_cals);
extern  void ar5212ResetCalValid(struct ath_hal *ah,  HAL_CHANNEL *chan,
                                 HAL_BOOL *isIQdone, u_int32_t calType);
extern	int16_t ar5212GetNoiseFloor(struct ath_hal *ah);
extern	HAL_BOOL ar5212SetTxPowerLimit(struct ath_hal *ah, u_int32_t limit,
        u_int16_t extra_txpow, u_int16_t tpcInDb);
extern void ar5212GetChipMinAndMaxPowers(struct ath_hal *ah, int8_t *max_tx_power,
                                         int8_t *min_tx_power);
extern void ar5212FillHalChansFromRegdb(struct ath_hal *ah,
                                        uint16_t freq,
                                        int8_t maxregpower,
                                        int8_t maxpower,
                                        int8_t minpower,
                                        uint32_t flags,
                                        int index);
extern void ar5212Setsc(struct ath_hal *ah, HAL_SOFTC sc);
extern	void ar5212ChainNoiseFloor(struct ath_hal *ah, int16_t *nfBuf, HAL_CHANNEL *chan, int is_scan);
extern	void ar5212InitializeGainValues(struct ath_hal *);
extern	HAL_RFGAIN ar5212GetRfgain(struct ath_hal *ah);

extern	HAL_BOOL ar5212UpdateTxTrigLevel(struct ath_hal *,
		HAL_BOOL IncTrigLevel);
extern  u_int16_t ar5212GetTxTrigLevel(struct ath_hal *);
extern  HAL_BOOL ar5212SetTxQueueProps(struct ath_hal *ah, int q,
		const HAL_TXQ_INFO *q_info);
extern	HAL_BOOL ar5212GetTxQueueProps(struct ath_hal *ah, int q,
		HAL_TXQ_INFO *q_info);
extern	int ar5212SetupTxQueue(struct ath_hal *ah, HAL_TX_QUEUE type,
		const HAL_TXQ_INFO *q_info);
extern	HAL_BOOL ar5212ReleaseTxQueue(struct ath_hal *ah, u_int q);
extern	HAL_BOOL ar5212ResetTxQueue(struct ath_hal *ah, u_int q);
extern	u_int32_t ar5212GetTxDP(struct ath_hal *ah, u_int q);
extern	HAL_BOOL ar5212SetTxDP(struct ath_hal *ah, u_int q, u_int32_t txdp);
extern	HAL_BOOL ar5212StartTxDma(struct ath_hal *ah, u_int q);
extern	u_int32_t ar5212NumTxPending(struct ath_hal *ah, u_int q);
extern	HAL_BOOL ar5212StopTxDma(struct ath_hal *ah, u_int q, u_int timeout);
extern  HAL_BOOL ar5212SetupTxDesc(struct ath_hal *ah, struct ath_desc *ds,
                 u_int pktLen, u_int hdrLen, HAL_PKT_TYPE type, u_int txPower,
                 u_int txRate0, u_int txTries0,
                 u_int key_ix, u_int antMode, u_int flags,
                 u_int rts_cts_rate, u_int rts_cts_duration,
                 u_int compicvLen, u_int compivLen, u_int comp);
extern  HAL_BOOL ar5212SetupXTxDesc(struct ath_hal *, struct ath_desc *,
                 u_int txRate1, u_int txRetries1,
                 u_int txRate2, u_int txRetries2,
                 u_int txRate3, u_int txRetries3);
extern  HAL_BOOL ar5212FillTxDesc (struct ath_hal *ah, void *ds, dma_addr_t *bufAddr,
                 u_int32_t *seg_len, u_int desc_id, u_int qcu, HAL_KEY_TYPE keyType, HAL_BOOL first_seg,
                 HAL_BOOL last_seg, const void *ds0);
extern  void ar5212SetDescLink(struct ath_hal *, void *ds, u_int32_t link);
extern  void ar5212GetDescLinkPtr(struct ath_hal *, void *ds, u_int32_t **link);
extern  void ar5212ClearTxDescStatus(struct ath_hal *ah, void *ds);
#ifdef ATH_SWRETRY
extern  void ar5212ClearDestMask(struct ath_hal *ah, void *ds);
#endif
extern	HAL_STATUS ar5212ProcTxDesc(struct ath_hal *ah, void *);
extern  u_int32_t ar5212CalcTxAirtime(struct ath_hal *ah, void *, struct ath_tx_status *,
        HAL_BOOL comp_wastedt, u_int8_t nbad, u_int8_t nframes);
extern  void ar5212GetTxIntrQueue(struct ath_hal *ah, u_int32_t *);
extern  void ar5212IntrReqTxDesc(struct ath_hal *ah, void *);
#ifdef AH_PRIVATE_DIAG
extern  void ar5212_ContTxMode(struct ath_hal *, void *, int mode);
#endif

extern	const HAL_RATE_TABLE *ar5212GetRateTable(struct ath_hal *, u_int mode);

extern	void ar5212EnableMIBCounters(struct ath_hal *);
extern	void ar5212DisableMIBCounters(struct ath_hal *);
extern	void ar5212AniAttach(struct ath_hal *);
extern	void ar5212AniDetach(struct ath_hal *);
extern	struct ar5212AniState *ar5212AniGetCurrentState(struct ath_hal *);
extern	struct ar5212Stats *ar5212AniGetCurrentStats(struct ath_hal *);
extern	HAL_BOOL ar5212AniControl(struct ath_hal *, HAL_ANI_CMD cmd, int param);
struct ath_rx_status;
extern	void ar5212AniPhyErrReport(struct ath_hal *ah,
		const struct ath_rx_status *rs);
extern	void ar5212ProcessMibIntr(struct ath_hal *, const HAL_NODE_STATS *);
extern	void ar5212AniArPoll(struct ath_hal *, const HAL_NODE_STATS *,
			     HAL_CHANNEL *, HAL_ANISTATS *);
extern	void ar5212AniReset(struct ath_hal *, int isRestore);
extern  void ar5212EnableTPC(struct ath_hal *);
#if ATH_ANT_DIV_COMB
extern  HAL_BOOL ar5212_ant_ctrl_set_lna_div_use_bt_ant(struct ath_hal * ah, HAL_BOOL enable, HAL_CHANNEL * chan);
#endif /* ATH_ANT_DIV_COMB */

/* DFS declarations*/
extern  HAL_BOOL ar5212RadarWait(struct ath_hal *ah, HAL_CHANNEL *chan);

#ifdef ATH_SUPPORT_DFS
extern  void ar5212EnableDfs(struct ath_hal *ah, HAL_PHYERR_PARAM *pe);
extern  void ar5212GetDfsThresh(struct ath_hal *ah, HAL_PHYERR_PARAM *pe);
extern void ar5212_adjust_difs(struct ath_hal *ah, u_int32_t val);
extern u_int32_t ar5212_dfs_config_fft(struct ath_hal *ah, bool is_enable);
extern void ar5212_dfs_cac_war(struct ath_hal *ah, u_int32_t start);
#endif

extern HAL_CHANNEL *ar5212GetExtensionChannel(struct ath_hal *ah);
extern HAL_BOOL ar5212IsFastClockEnabled(struct ath_hal *ah);
extern u_int16_t ar5212GetAHDevid(struct ath_hal *ah);


extern  void ar5212TxEnable(struct ath_hal *ah,HAL_BOOL enable);

#if defined(AH_PRIVATE_DIAG) && defined(AH_SUPPORT_DFS)
extern	HAL_BOOL ar5212SetRadarThresholds(struct ath_hal *ah, const u_int32_t threshType,
					  const u_int32_t value);
extern	HAL_BOOL ar5212GetRadarThresholds(struct ath_hal *ah, struct ar5212RadarState *thresh);
#endif

extern HAL_BOOL ar5212AbortTxDma(struct ath_hal *ah);
#if ATH_TxBF_DYNAMIC_LOF_ON_N_CHAIN_MASK
extern HAL_BOOL ar5212_txbf_loforceon_update(struct ath_hal *ah,HAL_BOOL loforcestate);
#endif

/* 11n specific declarations. Unused in ar5212 */
extern void ar5212Set11nTxDesc(struct ath_hal *ah, void *ds, u_int pktLen, HAL_PKT_TYPE type, u_int txPower, u_int key_ix, HAL_KEY_TYPE keyType, u_int flags);
extern void ar5212Set11nRateScenario(struct ath_hal *ah, void *ds, void *lastds, u_int dur_update_en, u_int rts_cts_rate, u_int rts_cts_duration, HAL_11N_RATE_SERIES series[], u_int nseries, u_int flags, u_int32_t smartAntenna);
extern void ar5212Set11nAggrFirst(struct ath_hal *ah, void *ds, u_int aggr_len);
extern void ar5212Set11nAggrMiddle(struct ath_hal *ah, void *ds, u_int num_delims);
extern void ar5212Set11nAggrLast(struct ath_hal *ah, void *ds);
extern void ar5212Clear11nAggr(struct ath_hal *ah, void *ds);
extern void ar5212Set11nRifsBurstMiddle(struct ath_hal *ah, void *ds);
extern void ar5212Set11nRifsBurstLast(struct ath_hal *ah, void *ds);
extern void ar5212Clr11nRifsBurst(struct ath_hal *ah, void *ds);
extern void ar5212Set11nAggrRifsBurst(struct ath_hal *ah, void *ds);
extern HAL_BOOL ar5212Set11nRxRifs(struct ath_hal *ah, HAL_BOOL enable);
extern HAL_BOOL ar5212DetectBbHang(struct ath_hal *ah);
extern HAL_BOOL ar5212DetectMacHang(struct ath_hal *ah);
extern void ar5212Set11nBurstDuration(struct ath_hal *ah, void *ds, u_int burst_duration);
extern void ar5212Set11nVirtMoreFrag(struct ath_hal *ah, void *ds, u_int vmf);
extern int8_t ar5212Get11nExtBusy(struct ath_hal *ah);
extern void ar5212Set11nMac2040(struct ath_hal *ah, HAL_HT_MACMODE);
extern HAL_HT_RXCLEAR ar5212Get11nRxClear(struct ath_hal *ah);
extern void ar5212Set11nRxClear(struct ath_hal *ah, HAL_HT_RXCLEAR rxclear);
extern u_int32_t ar5212GetMibCycleCountsPct(struct ath_hal *, u_int32_t*, u_int32_t*, u_int32_t*);
extern void ar5212DmaRegDump(struct ath_hal *);
extern u_int32_t ar5212PpmGetRssiDump(struct ath_hal *);
extern u_int32_t ar5212PpmArmTrigger(struct ath_hal *);
extern int ar5212PpmGetTrigger(struct ath_hal *);
extern u_int32_t ar5212PpmForce(struct ath_hal *);
extern void ar5212PpmUnForce(struct ath_hal *);
extern u_int32_t ar5212PpmGetForceState(struct ath_hal *);
extern void ar5212TimerStart(struct ath_hal *ah, u_int32_t timer_next, u_int32_t timer_period);
extern void ar5212TimerStop(struct ath_hal *ah);
extern int ar5212GetSpurInfo(struct ath_hal * ah, int *enable, int len, u_int16_t *freq);
extern int ar5212SetSpurInfo(struct ath_hal * ah, int enable, int len, u_int16_t *freq);
extern void ar5212SetIntrMitigationTimer(struct ath_hal* ah,
       HAL_INT_MITIGATION reg, u_int32_t value);
extern u_int32_t ar5212GetIntrMitigationTimer(struct ath_hal* ah,
       HAL_INT_MITIGATION reg);
extern void ar5212GreenApPsOnOff(struct ath_hal *ah, u_int16_t rxMask);
extern u_int16_t ar5212IsSingleAntPowerSavePossible(struct ath_hal *ah);
extern HAL_BOOL ar5212InterferenceIsPresent(struct ath_hal *ah);

extern void ar5212GetMibCycleCounts(struct ath_hal *, HAL_COUNTERS*);
extern void ar5212GetVowStats(struct ath_hal *, HAL_VOWSTATS*, u_int8_t);
#ifdef ATH_CCX
extern void ar5212ClearMibCounters(struct ath_hal *ah);
extern u_int8_t ar5212GetCcaThreshold(struct ath_hal *ah);
#endif
#ifdef ATH_BT_COEX
extern void ar5212SetBTCoexInfo(struct ath_hal *ah, HAL_BT_COEX_INFO *btinfo);
extern void ar5212BTCoexConfig(struct ath_hal *ah, HAL_BT_COEX_CONFIG *btconf);
extern void ar5212BTCoexSetQcuThresh(struct ath_hal *ah, int qnum);
extern void ar5212BTCoexSetWeights(struct ath_hal *ah, u_int32_t stompType);
extern void ar5212BTCoexSetupBmissThresh(struct ath_hal *ah, u_int32_t thresh);
extern void ar5212BTCoexSetParameter(struct ath_hal *sh, u_int32_t type, u_int32_t value);
extern void ar5212BTCoexDisable(struct ath_hal *ah);
extern int ar5212BTCoexEnable(struct ath_hal *ah);
extern u_int32_t ar5212GetBTActiveGpio(struct ath_hal *ah, u_int32_t reg);
extern u_int32_t ar5212GetWlanActiveGpio(struct ath_hal *ah, u_int32_t reg,u_int32_t bOn);
#endif
extern int ar5212AllocGenericTimer(struct ath_hal *ah, HAL_GEN_TIMER_DOMAIN tsf);
extern void ar5212FreeGenericTimer(struct ath_hal *ah, int index);
extern void ar5212StartGenericTimer(struct ath_hal *ah, int index, u_int32_t timer_next, u_int32_t timer_period);
extern void ar5212StopGenericTimer(struct ath_hal *ah, int index);
extern void ar5212GetGenTimerInterrupts(struct ath_hal *ah, u_int32_t *trigger, u_int32_t *thresh);

extern void ar5212ChkRSSIUpdateTxPwr(struct ath_hal *ah, int rssi);
extern HAL_BOOL ar5212_is_skip_paprd_by_greentx(struct ath_hal *ah);
extern void ar5212_paprd_dec_tx_pwr(struct ath_hal *ah);

extern void ar5212SetImmunity(struct ath_hal *ah, HAL_BOOL enable);
extern void ar5212GetHwHangs(struct ath_hal *ah, hal_hw_hangs_t *hangs);
extern void ar5212PromiscMode(struct ath_hal *ah, HAL_BOOL);
extern void ar5212ReadPktlogReg(struct ath_hal *ah, u_int32_t *, u_int32_t *, u_int32_t *, u_int32_t *);
extern void ar5212WritePktlogReg(struct ath_hal *ah, HAL_BOOL, u_int32_t, u_int32_t, u_int32_t, u_int32_t );
extern HAL_STATUS ar5212SetProxySTA(struct ath_hal *ah, HAL_BOOL enable);

extern HAL_BOOL ar5212ForceVCS( struct ath_hal *ah);
extern HAL_BOOL ar5212SetDfs3StreamFix(struct ath_hal *ah, u_int32_t val);
extern HAL_BOOL ar5212Get3StreamSignature( struct ath_hal *ah);

#if ATH_ANT_DIV_COMB
extern void ar5212AntDivCombGetConfig(struct ath_hal *ah, HAL_ANT_COMB_CONFIG* divCombConf);
extern void ar5212AntDivCombSetConfig(struct ath_hal *ah, HAL_ANT_COMB_CONFIG* divCombConf);
#endif /* ATH_ANT_DIV_COMB */
extern void ar5212DisablePhyRestart(struct ath_hal *ah, int disable_phy_restart);
extern void ar5212_enable_keysearch_always(struct ath_hal *ah, int enable);
#endif	/* _ATH_AR5212_H_ */
