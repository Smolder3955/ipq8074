/*
 * Copyright (c) 2000-2002, Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 *
 * Definitions for core driver
 * This is a common header file for all platforms and operating systems.
 */
#ifndef _ATH_RC_H_
#define    _ATH_RC_H_

#include <osdep.h>

#include "ah.h"
#include "if_athrate.h"
#ifdef UNINET
#include <net80211/_ieee80211.h>
#else
#include <_ieee80211.h>
#endif

#include <ath_dev.h>

/*
 * Set configuration parameters here; they cover multiple files.
 */
#define    ATHEROS_DEBUG    1

/*
 * Compatibility shims.  We leverage the work already done for the hal.
 */
#ifndef _A_TYPES_H_
#define _A_TYPES_H_
typedef    u_int8_t    A_UINT8;
typedef    int8_t        A_INT8;
typedef    u_int16_t    A_UINT16;
typedef    int16_t        A_INT16;
typedef    u_int32_t    A_UINT32;
typedef    int32_t        A_INT32;
typedef    u_int64_t    A_UINT64;
typedef    unsigned char  A_UCHAR;
typedef    bool         A_BOOL;
#endif

#ifndef FALSE
#define    FALSE    0
#endif
#ifndef TRUE
#define    TRUE    1
#endif

typedef    int8_t        A_RSSI;
typedef    int32_t        A_RSSI32;

typedef u_int8_t    WLAN_PHY;

#ifndef    INLINE
#define    INLINE        __inline
#endif

#ifndef A_MIN
#define    A_MIN(a,b)    ((a)<(b)?(a):(b))
#endif

#ifndef A_MAX
#define    A_MAX(a,b)    ((a)>(b)?(a):(b))
#endif

/*
 * Use the hal os glue code to get ms time
 */
#define    A_MS_TICKGET()    ((A_UINT32) CONVERT_SYSTEM_TIME_TO_MS(OS_GET_TIMESTAMP()))
#define    A_MEM_ZERO(p,s)    OS_MEMZERO(p,s)

#define    WLAN_PHY_OFDM    IEEE80211_T_OFDM
#define    WLAN_PHY_TURBO    IEEE80211_T_TURBO
#define    WLAN_PHY_CCK    IEEE80211_T_CCK
#define    WLAN_PHY_XR    (IEEE80211_T_TURBO+1)

#define IS_CHAN_TURBO(_c)    (((_c)->channel_flags & CHANNEL_TURBO) != 0)
#define    IS_CHAN_2GHZ(_c)    (((_c)->channel_flags & CHANNEL_2GHZ) != 0)


#define    WIRELESS_MODE_TURBO     WIRELESS_MODE_108a  /* NB: diff reduction */

#define    RX_FLIP_THRESHOLD    3    /* XXX */

/* 
 * There is no real reason why this should be 10. Usually a frame is tried 
 * 10 times by s/w. So choose the same as the count to choose different 
 * rate series
 */
#define    MAX_CONSEC_RTS_FAILED_FRAMES 10
enum {
    WLAN_RC_PHY_CCK,
    WLAN_RC_PHY_OFDM,
    WLAN_RC_PHY_TURBO,
    WLAN_RC_PHY_XR,
    WLAN_RC_PHY_HT_20_SS,
    WLAN_RC_PHY_HT_20_DS,
    WLAN_RC_PHY_HT_20_TS,
    WLAN_RC_PHY_HT_40_SS,
    WLAN_RC_PHY_HT_40_DS,
    WLAN_RC_PHY_HT_40_TS,
    WLAN_RC_PHY_HT_20_SS_HGI,
    WLAN_RC_PHY_HT_20_DS_HGI,
    WLAN_RC_PHY_HT_20_TS_HGI,
    WLAN_RC_PHY_HT_40_SS_HGI,
    WLAN_RC_PHY_HT_40_DS_HGI,
    WLAN_RC_PHY_HT_40_TS_HGI,
    WLAN_RC_PHY_MAX
};

/*
 * State structures for new rate adaptation code
 *
 * NOTE: Modifying these structures will impact
 * the Perl script that parses packet logging data.
 * See the packet logging module for more information.
 */
#define MAX_TX_RATE_TBL         72
#define MAX_TX_RATE_PHY         48
#define MAX_SCHED_TBL           48

#if ATH_SUPPORT_VOWEXT /* for RCA */
#define MAX_AGGR_LIMIT           32
#define MIN_AGGR_LIMIT		 8
#define CONSTANT_RATE_DURATION_MSEC 10
#define IS_LEGACY_RATE(__rate) (__rate < 0x80)
#define IF_ABOVE_MIN_RCARATE(__rate) ( (__rate > 0x89) || ( (__rate > 0x83) && (__rate < 0x88) ) )
#endif

#ifdef ATH_SUPPORT_TxBF
#define ATH_TXBF_SOUNDING(_sc) (_sc)->sc_txbfsounding
#else
#define ATH_TXBF_SOUNDING(_sc) 0
#endif

typedef struct TxRateCrtlState_s {
    A_RSSI rssiThres;           /* required rssi for this rate (dB) */
    A_UINT8 per;                /* recent estimate of packet error rate (%) */
#if ATH_SUPPORT_VOWEXT /* for RCA */
    A_UINT8 maxAggrSize; /* maximum allowed aggr size for each rate */
#endif
    A_UINT16 used;
    A_UINT32 aggr;
    A_UINT16 aggr_count[8];
} TxRateCtrlState;

typedef struct TxRateCtrl_s {
    TxRateCtrlState state[MAX_TX_RATE_TBL];    /* state for each rate */
    A_RSSI rssiLast;            /* last ack rssi */
    A_RSSI rssiLastLkup;        /* last ack rssi used for lookup */
    A_RSSI rssiLastPrev;        /* previous last ack rssi */
    A_RSSI rssiLastPrev2;       /* 2nd previous last ack rssi */
    A_RSSI32 rssiSumCnt;        /* count of rssiSum for averaging */
    A_RSSI32 rssiSumRate;       /* rate that we are averaging */
    A_RSSI32 rssiSum;           /* running sum of rssi for averaging */
    A_UINT32 validTxRateMask;   /* mask of valid rates */
    A_UINT8 rateTableSize;      /* rate table size */
    A_UINT8 rateMax;            /* max rate that has recently worked */
    A_UINT8 probeRate;          /* rate we are probing at */
    A_INT8  antFlipCnt;         /* number of consec times retry=2,3 */
    A_UINT8 misc[16];           /* miscellaneous state */
    A_UINT32 rssiTime;          /* msec timestamp for last ack rssi */
    A_UINT32 rssiDownTime;      /* msec timestamp for last down step */
    A_UINT32 probeTime;         /* msec timestamp for last probe */
    A_UINT8 hwMaxRetryRate;     /* rate of last max retry fail */
    A_UINT8 hwMaxRetryPktCnt;   /* num packets since we got HW max retry error */
    A_UINT8 recommendedPrimeState;   /* 0 = regular (11a/11g); 1 = Turbo (Turbo A/108g) */
    A_UINT8 switchCount;        /* num consec frames sent at rate past the mode switch threshold */
    A_UINT8 maxValidRate;       /* maximum number of valid rate */
    A_UINT8 maxValidTurboRate;  /* maximum number of valid turbo rate */
    A_UINT8 SoundRateIndex[MAX_TX_RATE_TBL];
    A_UINT8 validRateIndex[MAX_TX_RATE_TBL]; /* valid rate index */
    A_UINT8 validRateSeries[MAX_TX_RATE_TBL][MAX_SCHED_TBL]; /* corrsponding rate series for the valid rate index */
    A_UINT32 perDownTime;       /* msec timstamp for last PER down step */

    /* 11n state */
    A_UINT8  validPhyRateCount[WLAN_RC_PHY_MAX]; /* valid rate count */
    A_UINT8  validPhyRateIndex[WLAN_RC_PHY_MAX][MAX_TX_RATE_TBL]; /* index */
    A_UINT8  rcPhyMode;
    A_UINT8  rateMaxPhy;        /* Phy index for the max rate */
    A_UINT32 rateMaxLastUsed;   /* msec timstamp of when we last used rateMaxPhy */
    A_UINT32 probeInterval;     /* interval for ratectrl to probe for other rates */
#if ATH_SUPPORT_VOWEXT
    A_UINT8 nHeadFail;		/* number of failures in first half of the aggregate */
    A_UINT8 nTailFail;		/* number of failures in second half of the aggregate */
    A_UINT8 nAggrSize;		/* aggregation size of last transmitted packet */
    /* RCA variables */
    A_UINT8 aggrLimit;           /* aggregation size of the next packet as given by RCA */
    A_UINT8 lastRate;            /* tx rate of the last packet */
    A_UINT32 aggrUpTime;         /* msec timstamp for last maxAggrSize aging */
    A_UINT32 badPerTime;         /* msec timestamp for when we last hit bad per condition */ 
#endif
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
    A_UINT8 lastRateIndex;      /* last rate index that used for sending a frame */
    A_UINT32 TxRateInMbps;      /* The actual rate value in Mbps for last Tx */
    A_UINT32 Max4msFrameLen;    /* The maximum frame length allowed for last Tx */
    A_UINT32 TxRateCount;       /* The counter for the number of records */
#endif
    A_UINT32 rptGoodput;
    A_UINT32 consecRtsFailCount; /* number of consecutive frames that are failed to due to 
                                    RTS failure
                                 */
    A_UINT8  rate_fast_drop_en;  /* enable tx rate fast drop*/                             
} TX_RATE_CTRL;

#if ATH_SUPPORT_IQUE
typedef struct RateCtrlFunc {
#ifdef ATH_SUPPORT_VOWEXT
    void    (*rcUpdate)(struct ath_softc *, struct ath_node *, A_RSSI,
                        A_UINT8, int, int, struct ath_rc_series rcs[],
                        int, int, int, int, int, int);

    void    (*rcFind)(struct ath_softc *, struct ath_node *, A_UINT8,
                     int, int, unsigned int, struct ath_rc_series series[],
                     int *, int);
#else
    void    (*rcUpdate)(struct ath_softc *, struct ath_node *, A_RSSI,
                        A_UINT8, int, int, struct ath_rc_series rcs[],
                        int, int, int, int);
    void    (*rcFind)(struct ath_softc *, struct ath_node *, A_UINT8, int,
                      int, unsigned int, struct ath_rc_series series[],
                      int *, int);
#endif
} RATECTRL_FN;
#endif

struct atheros_softc;
struct atheros_vap;

/* per-node state */
struct atheros_node {
    TX_RATE_CTRL txRateCtrl;    /* rate control state proper */
#if ATH_SUPPORT_IQUE
    TX_RATE_CTRL txRateCtrlViVo;    /* rate control state proper */

    RATECTRL_FN rcFunc[WME_NUM_AC]; /* rate control functions */
    struct ath_node            *an;
#endif

    A_UINT32 lastRateKbps;    /* last rate in Kb/s */

    /* map of rate ix -> negotiated rate set ix */
    A_UINT8 rixMap[MAX_TX_RATE_TBL];

    /* map of ht rate ix -> negotiated rate set ix */
    A_UINT8 htrixMap[MAX_TX_RATE_TBL];

    A_UINT16 htcap;        /* ht capabilities */
    A_UINT8 antTx;        /* current transmit antenna */
#ifdef  ATH_SUPPORT_TxBF
    A_UINT8 usedNss;      /* current used Nss(number of spatial streams) for Beamforming*/  
    A_UINT8 lastTxmcs0cnt;  /* total number of Tx rate is mcs0 in last 255 frame */
    A_UINT8 Txmcs0cnt;      /* counter for Tx rate is mcs0*/
      A_UINT8 pktcount;       /* 255 frame counter*/
    A_UINT8 txbf                    :1,   /* transmit beamforming capable */
            txbf_sounding           :1,   /* sounding indicator */
            txbf_rpt_received       :1,   /* V /CV report received */
            txbf_steered            :1,   /* bf steered */
            txbf_sounding_request   :1;   /*  request new sounding*/  
#endif

    A_UINT8 singleStream    :1,   /* When TRUE, only single stream Tx possible */
            dualStream      :1,   /* When TRUE, dual stream Tx possible */
            tripleStream    :1,   /* When TRUE, triple stream Tx possible */
            stbc            :2,   /* Rx stbc capability */
            uapsd_rate_ctrl :1;   /* When TRUE, use UAPSD rate control */
    struct atheros_softc    *asc; /* back pointer to atheros softc */
    struct atheros_vap      *avp; /* back pointer to atheros vap */
};

/*
 * Rate Table structure for various modes - 'b', 'a', 'g', 'xr';
 * order of fields in info structure is important because hardcoded
 * structures are initialized within the hal for these
 */
#define RATE_TABLE_SIZE             32
typedef struct {
    int         rateCount;
    A_UINT8     rateCodeToIndex[RATE_TABLE_SIZE]; /* backward mapping */
    struct {
        A_UINT8   valid: 1,         /* Valid for use in rate control */
                  validUAPSD   : 1; /* Valid for use in rate control for UAPSD operation */
        WLAN_PHY  phy;              /* CCK/OFDM/TURBO/XR */
        A_UINT16  rateKbps;         /* Rate in Kbits per second */
        A_UINT16  userRateKbps;     /* User rate in KBits per second */
        A_UINT8   rateCode;         /* rate that goes into hw descriptors */
        A_UINT8   shortPreamble;    /* Mask for enabling short preamble in rate code for CCK */
        A_UINT8   dot11Rate;        /* Value that goes into supported rates info element of MLME */
        A_UINT8   controlRate;      /* Index of next lower basic rate, used for duration computation */
        A_RSSI    rssiAckValidMin;  /* Rate control related information */
        A_RSSI    rssiAckDeltaMin;  /* Rate control related information */
        A_UINT16  lpAckDuration;    /* long preamble ACK duration */
        A_UINT16  spAckDuration;    /* short preamble ACK duration*/
        A_UINT32  max4msFrameLen;   /* Maximum frame length(bytes) for 4ms tx duration */
        struct {
            A_UINT8  Retries[4];
            A_UINT8  Rates[4];
        } normalSched;
        struct {
            A_UINT8  Retries[4];
            A_UINT8  Rates[4];
        } shortSched;
        struct {
            A_UINT8  Retries[4];
            A_UINT8  Rates[4];
        } probeSched;
        struct {
            A_UINT8  Retries[4];
            A_UINT8  Rates[4];
        } probeShortSched;
        struct {
            A_UINT8  Retries[4];
            A_UINT8  Rates[4];
        } uapsd_normalSched;
        struct {
            A_UINT8  Retries[4];
            A_UINT8  Rates[4];
        } uapsd_shortSched;
    } info[32];
    A_UINT32    probeInterval;        /* interval for ratectrl to probe for
                     other rates */
    A_UINT32    rssiReduceInterval;   /* interval for ratectrl to reduce RSSI */
    A_UINT8     regularToTurboThresh; /* upperbound on regular (11a or 11g)
                     mode's rate before switching to turbo*/
    A_UINT8     turboToRegularThresh; /* lowerbound on turbo mode's rate before
                     switching to regular */
    A_UINT8     pktCountThresh;       /* mode switch recommendation criterion:
                     number of consecutive packets sent at
                     rate beyond the rate threshold */
    A_UINT8     initialRateMax;       /* the initial rateMax value used in
                     rcSibUpdate() */
    A_UINT8     numTurboRates;        /* number of Turbo rates in the rateTable */
    A_UINT8     xrToRegularThresh;    /* threshold to switch to Normal mode */
} RATE_TABLE;

/* per-vap state */
struct atheros_vap {
    const void        *rateTable;
    struct atheros_softc    *asc; /* backpointer */
    struct ath_vap          *athdev_vap; /* backpointer */
};

/* per-device state */
struct atheros_softc {
    /* phy tables that contain rate control data */
    const void *hwRateTable[WIRELESS_MODE_MAX];
    int        fixedrix;      /* -1 or index of fixed rate */
    u_int32_t  ah_magic;      /* per device magic number */
    u_int32_t  txTrigLevelMax;/* max transmit trigger level */
    u_int32_t (*prate_maprix)(struct atheros_softc *asc, WIRELESS_MODE curmode, u_int8_t baseIndex, u_int8_t flags, int isratecode);
#ifdef ATH_SUPPORT_TxBF
#define ATH_MAX_RATES 36
    u_int8_t   sc_txbf_disable_flag[ATH_MAX_RATES][AH_MAX_CHAINS]; /*  TxBF disable flag per rate */
#endif
};

#ifdef AH_SUPPORT_AR5212
/*
 *  Update the SIB's rate control information
 *
 *  This should be called when the supported rates change
 *  (e.g. SME operation, wireless mode change)
 *
 *  It will determine which rates are valid for use.
 */
void
rcSibUpdate(struct atheros_softc *asc, struct atheros_node *pSib,
            int keepState, struct ieee80211_rateset *pRateSet,
            enum ieee80211_phymode curmode);

/*
 *  This routine is called to initialize the rate control parameters
 *  in the SIB. It is called initially during system initialization
 *  or when a station is associated with the AP.
 */
void
rcSibInit(struct atheros_node *pSib);

/*
 * Determines and returns the new Tx rate index.
 */
A_UINT16
rcRateFind(struct ath_softc *sc, struct atheros_node *,
           A_UINT32 frameLen,
           const RATE_TABLE *pRateTable,
           HAL_CHANNEL *chan, int isretry);

/*
 * This routine is called by the Tx interrupt service routine to give
 * the status of previous frames.
 */
void
rcUpdate(struct atheros_node *pSib,
         int Xretries, int txRate, int retries, A_RSSI rssiAck,
         A_UINT8 curTxAnt, const RATE_TABLE *pRateTable,
         enum ieee80211_opmode opmode,
         int diversity,
         SetDefAntenna_callback setDefAntenna, void *context,
         int short_retry_fail);

void	atheros_setuptable(RATE_TABLE *rt);
void	ar5212SetupRateTables(void);
void	ar5212AttachRateTables(struct atheros_softc *sc);
void 	ar5212SetFullRateTable(struct atheros_softc *sc);
void 	ar5212SetHalfRateTable(struct atheros_softc *sc);
void 	ar5212SetQuarterRateTable(struct atheros_softc *sc);
#else
#define rcSibUpdate(asc,pSib, keepState,pRateSet, curmode) (pRateSet = pRateSet) 
#define rcSibInit(pSib) /* */
#define rcRateFind(sc,an,frameLen,pRateTable, chan, isretry) (0)
static INLINE void
rcUpdate(struct atheros_node *pSib,
         int Xretries, int txRate, int retries, A_RSSI rssiAck,
         A_UINT8 curTxAnt, const RATE_TABLE *pRateTable,
         enum ieee80211_opmode opmode,
         int diversity,
         SetDefAntenna_callback setDefAntenna, void *context, int short_retry_fail)
{

}
#define	atheros_setuptable(rt)  /* */
#define	ar5212SetupRateTables() /* */
#define	ar5212AttachRateTables(sc) /* */
#define	ar5212SetFullRateTable(sc) /* */
#define	ar5212SetHalfRateTable(sc) /* */
#define	ar5212SetQuarterRateTable(sc) /* */

#endif /* AH_SUPPORT_AR5212 */


#ifdef AH_SUPPORT_AR5416
void    ar5416SetupRateTables(void);
void    ar5416AttachRateTables(struct atheros_softc *sc);
#endif /*AH_SUPPORT_AR5416*/

#ifdef AH_SUPPORT_AR9300
void    ar9300AttachRateTables(struct atheros_softc *sc);
void    ar9300SetupRateTables(void);
#endif /*AH_SUPPORT_AR9300*/

#ifndef ATH_NO_5G_SUPPORT
#ifdef AH_SUPPORT_AR5416
void    ar5416SetFullRateTable(struct atheros_softc *sc);
void    ar5416SetHalfRateTable(struct atheros_softc *sc);
void    ar5416SetQuarterRateTable(struct atheros_softc *sc);
#endif /*AH_SUPPORT_AR5416*/

#ifdef AH_SUPPORT_AR9300
void    ar9300SetQuarterRateTable(struct atheros_softc *sc);
void    ar9300SetHalfRateTable(struct atheros_softc *sc);
void    ar9300SetFullRateTable(struct atheros_softc *sc);
#endif /*AH_SUPPORT_AR9300*/
#endif /*ATH_NO_5G_SUPPORT*/

u_int32_t ar5212_rate_maprix(struct atheros_softc *asc, WIRELESS_MODE curmode, u_int8_t baseIndex,u_int8_t flags, int isratecode);
u_int32_t ar5416_rate_maprix(struct atheros_softc *asc, WIRELESS_MODE curmode,  u_int8_t baseIndex,u_int8_t flags, int isratecode);
u_int32_t ar9300_rate_maprix(struct atheros_softc *asc, WIRELESS_MODE curmode,  u_int8_t baseIndex,u_int8_t flags, int isratecode);

#define RATECTRL_MEMTAG 'Mtar'

#define RC_OS_MALLOC(_ppMem, _size, _tag)       OS_MALLOC_WITH_TAG(_ppMem, _size, _tag)
#define RC_OS_FREE(_pMem, _size)                OS_FREE_WITH_TAG(_pMem, _size)

#if ATH_CCX
u_int8_t
rcRateValueToPer(struct ath_softc *sc, struct ath_node *an, int txRateKbps);
#endif /* ATH_CCX */

#ifdef  ATH_SUPPORT_TxBF
#define TX_STREAM_USED_NUM(_pSib) (3*(_pSib->tripleStream) + 2*(_pSib->dualStream) + (_pSib->singleStream))
#define OPPOSITE_CEC_NUM(_capflag) (((_capflag & WLAN_RC_CEC_FLAG) >> WLAN_RC_CEC_FLAG_S) + 1) 
#endif /* ATH_SUPPORT_TxBF */

#ifdef ATH_SUPPORT_TxBF
#define HT_RATE_CODE 0x80
#define RcUpdate_TxBF_STATS(_sc, _rcs, _ts) \
    do { \
        if ((_ts)->ts_status == 0){\
            /* HT rate*/ \
            if ((_ts)->ts_ratecode & HT_RATE_CODE) { \
                if (((_rcs)[0].flags & ATH_RC_SOUNDING_FLAG) ==0){\
                    /*only for not sounding frame*/ \
                    (_sc)->sc_stats.ast_lastratecode = (_ts)->ts_ratecode; \
                    (_sc)->sc_stats.ast_mcs_count[((_ts)->ts_ratecode & MCS_RATE)]++; \
                } else{ \
                    (_sc)->sc_stats.ast_sounding_count++; \
                } \
            } \
        } \
    } while (0)
            
#else
#define RcUpdate_TxBF_STATS(_sc, _rcs, _ts)
#endif
#endif /* _ATH_RC_H_ */

