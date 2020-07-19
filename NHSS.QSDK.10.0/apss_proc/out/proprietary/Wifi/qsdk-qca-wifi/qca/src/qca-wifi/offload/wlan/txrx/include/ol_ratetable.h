/*
 * Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#ifndef OL_RATETABLE_H_
#define OL_RATETABLE_H_

#include "ol_ratectrl_11ac_types.h"
#include "wlan_defs.h"
#include "_ieee80211.h"

/** From whal_desc.h */

#define WHAL_RC_FLAG_CHAIN_MASK 0x0f /* identifies tx chain config (1,5,7,f) */

#define WHAL_RC_FLAG_ONE_CHAIN     1
#define WHAL_RC_FLAG_TWO_CHAIN     5
#define WHAL_RC_FLAG_THREE_CHAIN   7
#define WHAL_RC_FLAG_FOUR_CHAIN    0xf
#define WHAL_RC_FLAG_SGI        0x08 /* use HT SGI if set */
#define WHAL_RC_FLAG_STBC       0x10 /* use HT STBC if set */
#define WHAL_RC_FLAG_40MHZ      0x20 /* 40 mhz mode */
#define WHAL_RC_FLAG_80MHZ      0x40 /* 80 mhz mode */
#define WHAL_RC_FLAG_160MHZ     0x80 /* 160 mhz mode */

#define WHAL_RC_FLAG_TXBF       0x0100
#define WHAL_RC_FLAG_RTSENA     0x0200
#define WHAL_RC_FLAG_CTSENA     0x0400
#define WHAL_RC_FLAG_LDPC       0x0800
#define WHAL_RC_FLAG_SERIES1    0x1000


/** From whal_api.h */

#define NUM_SPATIAL_STREAM 4

// TODO
// The following would span more than one octet when 160MHz BW defined for VHT
// Also it's important to maintain the ordering of this enum else it would break other
// rate adapation functions.
//

typedef enum {
    WHAL_MOD_IEEE80211_T_DS,            /* direct sequence spread spectrum */
    WHAL_MOD_IEEE80211_T_OFDM,          /* frequency division multiplexing */
    WHAL_MOD_IEEE80211_T_HT_20,
    WHAL_MOD_IEEE80211_T_HT_40,
    WHAL_MOD_IEEE80211_T_VHT_20,
    WHAL_MOD_IEEE80211_T_VHT_40,
    WHAL_MOD_IEEE80211_T_VHT_80,
//#if NUM_DYN_BW > 3
    WHAL_MOD_IEEE80211_T_VHT_160,
//#endif
    WHAL_MOD_IEEE80211_T_MAX_PHY
#define WHAL_MOD_IEEE80211_T_CCK WHAL_MOD_IEEE80211_T_DS    /* more common nomenclatur */
}WHAL_WLAN_MODULATION_TYPE;


/*      Do Not change the following without changing rate table.
          it is assumed that these rates would be allocated contigously
          starting from 20/40/80/160 and sequentlially for NSS=1,2,3,4.
          No support for NSS more than that.
These are derives based on ar600P_phy.c:
const WHAL_RATE_TABLE ar600P_11abgnRateTable
*/

#define CCK_START_INDEX         0
#define CCK_END_INDEX           3

#define OFDM_START_INDEX        4
#define OFDM_END_INDEX          11

#define HT20_NSS1_START_INDEX   12
#define HT20_NSS1_END_INDEX     19
#define HT20_NSS2_START_INDEX   20
#define HT20_NSS2_END_INDEX     27
#define HT20_NSS3_START_INDEX   28
#define HT20_NSS3_END_INDEX     35
#define HT20_NSS4_START_INDEX   36
#define HT20_NSS4_END_INDEX     43

#define HT40_NSS1_START_INDEX   44
#define HT40_NSS1_END_INDEX     51
#define HT40_NSS2_START_INDEX   52
#define HT40_NSS2_END_INDEX     59
#define HT40_NSS3_START_INDEX   60
#define HT40_NSS3_END_INDEX     67
#define HT40_NSS4_START_INDEX   68
#define HT40_NSS4_END_INDEX     75

#define VHT20_NSS1_START_INDEX  76
#define VHT20_NSS1_END_INDEX    85
#define VHT20_NSS2_START_INDEX  86
#define VHT20_NSS2_END_INDEX    95
#define VHT20_NSS3_START_INDEX  96
#define VHT20_NSS3_END_INDEX    105
#define VHT20_NSS4_START_INDEX  106
#define VHT20_NSS4_END_INDEX    115

#define VHT40_NSS1_START_INDEX  116
#define VHT40_NSS1_END_INDEX    125
#define VHT40_NSS2_START_INDEX  126
#define VHT40_NSS2_END_INDEX    135
#define VHT40_NSS3_START_INDEX  136
#define VHT40_NSS3_END_INDEX    145
#define VHT40_NSS4_START_INDEX  146
#define VHT40_NSS4_END_INDEX    155

#define VHT80_NSS1_START_INDEX  156
#define VHT80_NSS1_END_INDEX    165
#define VHT80_NSS2_START_INDEX  166
#define VHT80_NSS2_END_INDEX    175
#define VHT80_NSS3_START_INDEX  176
#define VHT80_NSS3_END_INDEX    185
#define VHT80_NSS4_START_INDEX  186
#define VHT80_NSS4_END_INDEX    195

#define VHT160_NSS1_START_INDEX  196
#define VHT160_NSS1_END_INDEX    205
#define VHT160_NSS2_START_INDEX  206
#define VHT160_NSS2_END_INDEX    215
#define VHT160_NSS3_START_INDEX  216
#define VHT160_NSS3_END_INDEX    225
#define VHT160_NSS4_START_INDEX  226
#define VHT160_NSS4_END_INDEX    235

#define LEGACY      1
#define HT_MODE     2
#define VHT_MODE    3

/* CCK/DSSS */
#define CCK_RATE_TABLE_INDEX 0

/* OFDM */
#define OFDM_RATE_TABLE_INDEX 4

/* HT */

#define HT_20_RATE_TABLE_INDEX  12


/* The following defines valid tx rate masks for each bw for HT and VHT
*/

#define NUM_VALID_RC_MASK 6 /* 3 for VHT 20/40/80, + 2 for HT20/40 + 1 other */

#if (NUM_SPATIAL_STREAM > 3)

#define HT_40_RATE_TABLE_INDEX  44
#define VHT_20_RATE_TABLE_INDEX 76
#define VHT_40_RATE_TABLE_INDEX 116
#define VHT_80_RATE_TABLE_INDEX 156
#define VHT_160_RATE_TABLE_INDEX 196

#define RATE_TABLE_SIZE  236

#elif (NUM_SPATIAL_STREAM == 3)

#define HT_40_RATE_TABLE_INDEX  36
#define VHT_20_RATE_TABLE_INDEX 60
#define VHT_40_RATE_TABLE_INDEX 90
#define VHT_80_RATE_TABLE_INDEX 120
#define VHT_160_RATE_TABLE_INDEX 150

#define RATE_TABLE_SIZE 180

#elif (NUM_SPATIAL_STREAM == 2)

#define HT_40_RATE_TABLE_INDEX      28
#define VHT_20_RATE_TABLE_INDEX     44
#define VHT_40_RATE_TABLE_INDEX     64
#define VHT_80_RATE_TABLE_INDEX     84
#define VHT_160_RATE_TABLE_INDEX    104

#define RATE_TABLE_SIZE  124

#else /* Mandatory support */

#define HT_40_RATE_TABLE_INDEX      20
#define VHT_20_RATE_TABLE_INDEX     28
#define VHT_40_RATE_TABLE_INDEX     38
#define VHT_80_RATE_TABLE_INDEX     48
#define VHT_160_RATE_TABLE_INDEX    58

#define RATE_TABLE_SIZE  68

#endif

#define DEFAULT_LOWEST_RATE_IN_2GHZ    0x43 /* 1 Mbps */
#define DEFAULT_LOWEST_RATE_IN_5GHZ    0x03 /* 6 Mbps */

typedef enum {
    /* Neither of the rate-series should use RTS-CTS */
    WAL_RC_RTSCTS_FOR_NO_RATESERIES = 0,
    /* Only the second rate-series will use RTS-CTS */
    WAL_RC_RTSCTS_FOR_SECOND_RATESERIES = 1,
    /* Only the second rate-series will use RTS-CTS, but if there's a
     * sw retry, both the rate series will use RTS-CTS */
    WAL_RC_RTSCTS_ACROSS_SW_RETRIES = 2,
    /* Enable  RTS-CTS for all rate series */
    WAL_RC_RTSCTS_FOR_ALL_RATESERIES = 3,
   /* RTS/CTS used for ERP protection for every PPDU. */
    WAL_RC_RTSCTS_ERP = 4,
    /* Add new profiles before this.. */
    WAL_RC_RTSCTS_PROFILE_MAX = 15
} WAL_RC_RTSCTS_PROFILE;

typedef enum {
    /* RTS-CTS disabled */
    WAL_RC_RTS_CTS_DISABLED = 0,
    /* RTS-CTS enabled */
    WAL_RC_USE_RTS_CTS = 1,
    /* CTS-to-self enabled */
    WAL_RC_USE_CTS2SELF = 2,
} WAL_RC_RTS_CTS;

#define WAL_RC_RTS_CTS_MASK          0x0f
#define WAL_RC_RTS_CTS_SHIFT         0
#define WAL_RC_RTS_CTS_PROFILE_MASK  0xf0
#define WAL_RC_RTS_CTS_PROFILE_SHIFT 4

#define RTSCTS_DISABLED \
  (((WAL_RC_RTS_CTS_DISABLED << WAL_RC_RTS_CTS_SHIFT) & WAL_RC_RTS_CTS_MASK) | \
   ((WAL_RC_RTSCTS_FOR_NO_RATESERIES << WAL_RC_RTS_CTS_PROFILE_SHIFT) & WAL_RC_RTS_CTS_PROFILE_MASK))

#define RTSCTS_ENABLED_4_SECOND_RATESERIES \
  (((WAL_RC_USE_RTS_CTS << WAL_RC_RTS_CTS_SHIFT) & WAL_RC_RTS_CTS_MASK) | \
   ((WAL_RC_RTSCTS_FOR_SECOND_RATESERIES << WAL_RC_RTS_CTS_PROFILE_SHIFT) & WAL_RC_RTS_CTS_PROFILE_MASK))

#define CTS2SELF_ENABLED_4_SECOND_RATESERIES \
  (((WAL_RC_USE_CTS2SELF << WAL_RC_RTS_CTS_SHIFT) & WAL_RC_RTS_CTS_MASK) | \
   ((WAL_RC_RTSCTS_FOR_SECOND_RATESERIES << WAL_RC_RTS_CTS_PROFILE_SHIFT) & WAL_RC_RTS_CTS_PROFILE_MASK))

#define RTSCTS_ENABLED_4_SWRETRIES \
  (((WAL_RC_USE_RTS_CTS << WAL_RC_RTS_CTS_SHIFT) & WAL_RC_RTS_CTS_MASK) | \
   ((WAL_RC_RTSCTS_ACROSS_SW_RETRIES << WAL_RC_RTS_CTS_PROFILE_SHIFT) & WAL_RC_RTS_CTS_PROFILE_MASK))

#define CTS2SELF_ENABLED_4_SWRETRIES \
  (((WAL_RC_USE_CTS2SELF << WAL_RC_RTS_CTS_SHIFT) & WAL_RC_RTS_CTS_MASK) | \
   ((WAL_RC_RTSCTS_ACROSS_SW_RETRIES << WAL_RC_RTS_CTS_PROFILE_SHIFT) & WAL_RC_RTS_CTS_PROFILE_MASK))

typedef enum {
    WHAL_RC_MASK_IDX_NON_HT  = 0, /* CCK/DSSS/OFDM */
    WHAL_RC_MASK_IDX_HT_20   = 1, /* HT 20 rates for all NSS */
    WHAL_RC_MASK_IDX_HT_40   = 2, /* HT 40 rates for all NSS */
    WHAL_RC_MASK_IDX_VHT_20  = 3, /* VHT 20 rates for all NSS */
    WHAL_RC_MASK_IDX_VHT_40  = 4, /* VHT 40 rates for all NSS */
    WHAL_RC_MASK_IDX_VHT_80  = 5, /* VHT 80 rates for all NSS */
    WHAL_RC_MASK_IDX_VHT_160 = 6, /* VHT 160 rates for all NSS */
} WHAL_RC_MASK_IDX;
#define WHAL_RC_MASK_MIN_IDX 0
#define WHAL_RC_MASK_MAX_IDX WHAL_RC_MASK_IDX_VHT_160

/* Given we have lots of rates and associated mask the following abstract
    the rate mask for given phy
*/
typedef struct {
    A_RATEMASK rc_mask;
    WHAL_RC_MASK_IDX idx;
} RC_MASK_INFO_t;

typedef enum {
    /* values 0..0xffff are reserved for rate specific flags */
    WHAL_RC_TXDONE_A1_GROUP = 0x10000, /* group addressed frame */
    WHAL_RC_TXDONE_AGGR = 0x20000, /* aggregrate frame A-MSDU/A-MPDU */
    WHAL_RC_TXDONE_X_RETRIES = 0x40000, /* excessive retries for this frame */
    WHAL_RC_TXDONE_SERIES1 = 0x80000, /* indication of series */
} WHAL_RX_TX_DONE_FLAGS ;

/**
* @brief all rate tx done arguments are contained in this structure.
*
*/

#define MAX_SERIES_SUPPORTED    (2)

typedef struct {
    A_UINT32 num_elems; /* Number of elements in the rc_tx_done_args[] array.
                         * If 0, we do not update the rc args */
    A_UINT8 probe_aborted; /* 1: Probe frame; 0: Not a probing frame
                     * We need this so as to take care of the cases when probing
                     * frames get aborted and do not have any args to update. */
    RC_TX_DONE_PARAMS rc_tx_done_args[MAX_SERIES_SUPPORTED]; /* This should be as long as
                                                  the number of series we support. */
} WHAL_TX_DONE_RC_UPDATE_PARAMS;


typedef void (*RC_TX_DONE_CALLBACK) (void *,
        void *,
        RC_TX_DONE_PARAMS *);

/* The WHAL_RATE_TABLE was changed for 11N for the following reasons;
 *    - the rateCodeToIndex[] array found in the legacy table won't work for 11N operation
 *        as the ratecode is not sufficient to translate to an index. SGI and Spectrum width
 *        must also be considered. This feature has been replaced by a function ptr (RcToIndex).
 *    - It is useful to add max4msecLen element which represents the number of bytes that can
 *        be transmitted in 4msec at the given rate.  A necessary calculation for Aggregation
 *        support.
 *    - It was desired to remove the retry schedule from the ROM table and instead move it
 *        into the rate control logic where it could be better configured by the host and
 *        modified via firmware updates.
 *    - The nextMinIndex[] array found in the legacy table does not consider enough
 *        dynamic conditional information to resolve to the next lower rate.
 */
typedef struct _WHAL_RATE_TABLE {
    A_UINT32     rateCount; /* number of valid entries in info[] below */
    /* func to return rate index given code and code, width and sgi */
    A_UINT8 (*RcToIndex)(void*, A_UINT8, A_UINT8);
    /* func to return rate modulation given code spatial width */
    WHAL_WLAN_MODULATION_TYPE (*GetModulation)(A_UINT8, A_UINT8);

    struct {
        A_UINT32  validModeMask;    /* bit mask where 1 indicates the rate is valid for that mode
                                     * bit position is (1<<mode) */
        WHAL_WLAN_MODULATION_TYPE phy; /* CCK/OFDM/MCS */
       A_UINT32   propMask;         /* bit mask of rate property. NSS/STBC/TXBF/LDPC */
        A_UINT32  rateKbps;         /* Rate in Kbits per second */
        A_UINT32  rateKbpsSGI;      /* Rate in kbits per second if HT SGI is enabled */
        A_UINT32  userRateKbps;     /* User rate in KBits per second */
        A_UINT8   rateCode;         /* rate that goes into hw descriptors */
        A_UINT8   shortPreamble;    /* Mask for enabling short preamble in rate code for CCK */
        A_UINT8   dot11Rate;        /* Value that goes into supported rates info element of MLME */
        A_UINT8   controlRate;      /* Index of next lower basic rate, used for duration computation */
        A_RSSI    rssiAckValidMin;  /* Rate control related information */
        A_RSSI    rssiAckDeltaMin;  /* Rate control related information */
        A_UINT32  max4msecLen;        /* max length to tx with 4msec restriction */
        A_UINT16  bitsPerSymbol;    /* number of bits per rate symbol. used to calc tx time */
        A_UINT8      maxPER;            /* max practicle pkt error rate above which next lower rate should be used */
        A_UINT8   nextLowerIndex;   /* the next lower rate based only on rateKbps */
     } info[RATE_TABLE_SIZE];
    A_UINT32    probeInterval;        /* interval for ratectrl to probe for
                     other rates */
    A_UINT32    rssiReduceInterval;   /* interval for ratectrl to reduce RSSI */
    A_UINT8     initialRateMax[MODE_MAX]; /* the initial maximum rate index based on current Mode */
} WHAL_RATE_TABLE;

/* DEVELOPERS: use these macros to access elements from the rate table instead
 * of accessing the rate table directly.  As new chips come to
 * exist the rate table has always been modified in unpredictable ways.  To
 * ease the porting/integration efforts it helps to avoid accessing the
 * structure directly. */
#define RT_INVALID_INDEX (0xff)
#define RT_GET_RT(_rt)                      ((const WHAL_RATE_TABLE*)(_rt))
#define RT_GET_INFO(_rt, _index)            RT_GET_RT(_rt)->info[(_index)]
#define RT_GET_RATE_COUNT(_rt)              (RT_GET_RT(_rt)->rateCount)
#define RT_GET_HW_RATECODE(_rt, _index)     (RT_GET_INFO(_rt,(_index)).rateCode)
#define RT_GET_PHY(_rt, _index)             (RT_GET_INFO(_rt,(_index)).phy)
#define RT_GET_MBPS(_rt, _index)            ((RT_GET_INFO(_rt,(_index)).rateKbps+999)/1000)
#define RT_GET_RAW_KBPS(_rt, _index)        (RT_GET_INFO(_rt,(_index)).rateKbps)
#define RT_GET_SGI_KBPS(_rt, _index)        (RT_GET_INFO(_rt,(_index)).rateKbpsSGI)
#define RT_GET_USER_KBPS(_rt, _index)       (RT_GET_INFO(_rt,(_index)).userRateKbps)

#define RT_GET_SHORTPREAM_CODE(_rt, _index) (RT_GET_INFO(_rt,(_index)).shortPreamble)
#define RT_GET_DOT11RATE(_rt, _index)       (RT_GET_INFO(_rt,(_index)).dot11Rate)
#define RT_GET_CTRLRATE(_rt, _index)        (RT_GET_INFO(_rt,(_index)).controlRate)
#define RT_GET_RSSI_MIN(_rt, _index)        (RT_GET_INFO(_rt,(_index)).rssiAckValidMin)
#define RT_GET_RSSI_DELTA(_rt, _index)      (RT_GET_INFO(_rt,(_index)).rssiAckDeltaMin)
#define RT_GET_4MSEC_LEN(_rt, _index)       (RT_GET_INFO(_rt,(_index)).max4msecLen)
#define RT_GET_BITSPERSYMBOL(_rt, _index)   (RT_GET_INFO(_rt,(_index)).bitsPerSymbol)
#define RT_GET_MAX_PER(_rt, _index)         (RT_GET_INFO(_rt,(_index)).maxPER)
#define RT_GET_RSSI_REDUCE(_rt, _index)        ((_rt)->rssiReduceInterval)
#define RT_GET_INIT_MAXRATE(_rt, _mode)     (RT_GET_RT(_rt)->initialRateMax[(_mode)])
#define RT_GET_PROBE_PERIOD(_rt, _index)    (RT_GET_RT(_rt)->probeInterval)

/* FIXME: TODO Change this to use a new field for SGI 1/4 us eqiv
 * The overhead of using SGI is just ~11% and also this is minimum mpdu
 * spacing (can be more).
 */

#define RT_GET_QUART_US_LEN(_rt, _index)    (RT_GET_INFO(_rt,(_index)).max4msecLen)
#define RT_GET_NUM_DELIMS(_rt, _index, _spacing)    \
    (_spacing) ? (((_spacing) >= 8) ? 0x3fc : /*0x3fc is for spacing > 16us */\
    ((((RT_GET_SGI_KBPS((_rt), (_index)) << ((_spacing)-1)) >> 15) + 3) >> 2))\
    : (0)


#define RT_IS_VALID_RATE(_rt, _index, _mode) ((RT_GET_INFO(_rt,(_index)).validModeMask & (1<<(_mode)))? TRUE:FALSE)

#define RT_IS_40MHZ(_rt, _index) (RT_GET_INFO(_rt,(_index)).phy == WHAL_MOD_IEEE80211_T_HT_40)

#define RT_IS_HT(_rt, _index) (RT_GET_INFO(_rt,(_index)).phy == WHAL_MOD_IEEE80211_T_HT_40 || \
                               RT_GET_INFO(_rt,(_index)).phy == WHAL_MOD_IEEE80211_T_HT_20 )
#define RT_IS_RATECODE_HT(_code) (((_code) & 0x80)? 1 : 0)
#define RT_DERIVE_PHY(_rt, _code, _width) (RT_GET_RT(_rt)->GetModulation((_code), (_width)))
#define RT_CODE_TO_INDEX(_rt, _code, _width) (RT_GET_RT(_rt)->RcToIndex((void*)(_rt), (_code), (_width)))
#define RT_WHALCODE_TO_INDEX(_rt, _wcode) RT_CODE_TO_INDEX(_rt, \
    (_wcode).rateCode,(((_wcode).flags & WHAL_RC_FLAG_40MHZ)? 1: \
    (((_wcode).flags & WHAL_RC_FLAG_80MHZ)? 2:0)))

#define RT_GET_BW_FLAG(_flag, _rt, _index) do {\
            switch (RT_GET_INFO(_rt,(_index)).phy) { \
                case WHAL_MOD_IEEE80211_T_HT_40:    \
                case WHAL_MOD_IEEE80211_T_VHT_40:   \
                    (_flag) |= WHAL_RC_FLAG_40MHZ;  \
                    break;  \
                case WHAL_MOD_IEEE80211_T_VHT_80:   \
                    (_flag) |= WHAL_RC_FLAG_80MHZ;  \
                    break;  \
                case WHAL_MOD_IEEE80211_T_VHT_160:   \
                    (_flag) |= WHAL_RC_FLAG_160MHZ;  \
                    break; \
        default: \
             (_flag) |=0; }  \
             } while (0);

#define RT_MAP_BW_IDX_2_RC_FLAG(_flag, _index) do {\
            switch ((_index)) { \
                case 1:    \
                    (_flag) |= WHAL_RC_FLAG_40MHZ;  \
                    break;  \
                case 2:   \
                    (_flag) |= WHAL_RC_FLAG_80MHZ;  \
                    break;  \
                case 3:   \
                    (_flag) |= WHAL_RC_FLAG_160MHZ;  \
                    break; \
                default: \
                     (_flag) |=0; }  \
             } while (0);


#define RT_GET_NEXT_MIN_INDEX(_rt, _index) (RT_GET_INFO(_rt,(_index)).nextLowerIndex)
#define RT_WHALCODE_IS_SGI(_wcode) (((_wcode).flags & WHAL_RC_FLAG_SGI)? 1:0)
#define RATE_PROP_1NSS 0x00000001
#define RATE_PROP_2NSS 0x00000002
#define RATE_PROP_3NSS 0x00000004
#define RATE_PROP_4NSS 0x00000008
#define RATE_PROP_LDPC 0x00000010
#define RATE_PROP_STBC 0x00000020
#define RATE_PROP_TXBF 0x00000040
#define RT_IS_STBC(_rt, _index) (RT_GET_INFO(_rt,(_index)).propMask & RATE_PROP_STBC)
#define RT_IS_LDPC(_rt, _index) (RT_GET_INFO(_rt,(_index)).propMask & RATE_PROP_LDPC)
#define RT_IS_TXBF(_rt, _index) (RT_GET_INFO(_rt,(_index)).propMask & RATE_PROP_TXBF)
#define RT_IS_1SS(_rt, _index)  (RT_GET_INFO(_rt,(_index)).propMask & RATE_PROP_1NSS)
#define RT_IS_2SS(_rt, _index)  (RT_GET_INFO(_rt,(_index)).propMask & RATE_PROP_2NSS)

#define RT_IS_VHT_20MHZ(_rt, _index) (RT_GET_INFO(_rt,(_index)).phy == \
        WHAL_MOD_IEEE80211_T_VHT_20)

#define RT_IS_VHT_40MHZ(_rt, _index) (RT_GET_INFO(_rt,(_index)).phy == \
        WHAL_MOD_IEEE80211_T_VHT_40)
#define RT_IS_VHT_80MHZ(_rt, _index) (RT_GET_INFO(_rt,(_index)).phy == \
        WHAL_MOD_IEEE80211_T_VHT_80)
#define RT_IS_VHT_160MHZ(_rt, _index) (RT_GET_INFO(_rt,(_index)).phy == \
        WHAL_MOD_IEEE80211_T_VHT_160)


#define RT_IS_VHT(_rt, _index) (RT_GET_INFO(_rt,(_index)).phy == \
        WHAL_MOD_IEEE80211_T_VHT_20 || \
        RT_GET_INFO(_rt,(_index)).phy == WHAL_MOD_IEEE80211_T_VHT_40 || \
        RT_GET_INFO(_rt,(_index)).phy == WHAL_MOD_IEEE80211_T_VHT_80 || \
        RT_GET_INFO(_rt,(_index)).phy == WHAL_MOD_IEEE80211_T_VHT_160 )



/* TODO:
     The following should belong to AR600P_phy/descs.
     Move these after initial validation.
*/

#define RT_IS_VHT_INDEX(index) ((index) >= VHT_20_RATE_TABLE_INDEX)
#define RT_IS_HT_INDEX(index) ((index) >= HT_20_RATE_TABLE_INDEX)
#define RT_IS_OFDM_CCK_INDEX(index) ((index) < HT_20_RATE_TABLE_INDEX)

#define RT_HT_RATE_BASE_IDX    (HT_20_RATE_TABLE_INDEX)

#define RT_VHT_RATE_BASE_IDX    (VHT_20_RATE_TABLE_INDEX)

/* Macros to get min/max mandatory supported VHT Rates */
#define RT_GET_VHT_20M_MIN_RATE (VHT_20_RATE_TABLE_INDEX)
#define RT_GET_VHT_20M_MAX_RATE (VHT_20_RATE_TABLE_INDEX + \
    ((NUM_SPATIAL_STREAM << 3) + (NUM_SPATIAL_STREAM << 1)-1))

#define RT_GET_VHT_40M_MIN_RATE (VHT_40_RATE_TABLE_INDEX)
#define RT_GET_VHT_40M_MAX_RATE (VHT_40_RATE_TABLE_INDEX + \
    ((NUM_SPATIAL_STREAM << 3) + (NUM_SPATIAL_STREAM << 1)-1))

#define RT_GET_VHT_80M_MIN_RATE (VHT_80_RATE_TABLE_INDEX)
#define RT_GET_VHT_80M_MAX_RATE (VHT_80_RATE_TABLE_INDEX + \
    ((NUM_SPATIAL_STREAM << 3) + (NUM_SPATIAL_STREAM << 1)-1))

/* Macros to get min/max supported HT Rates */
#define RT_GET_HT_20M_MIN_RATE (HT_20_RATE_TABLE_INDEX)
#define RT_GET_HT_20M_MAX_RATE (HT_20_RATE_TABLE_INDEX + \
    ((NUM_SPATIAL_STREAM << 3)-1))

#define RT_GET_HT_40M_MIN_RATE (HT_40_RATE_TABLE_INDEX)
#define RT_GET_HT_40M_MAX_RATE (HT_40_RATE_TABLE_INDEX + \
       ((NUM_SPATIAL_STREAM << 3)-1))

/* Macros to get default starting rate index for ratecontrol */
#define RT_GET_HT_20M_START_RIX (HT_20_RATE_TABLE_INDEX + 4)
#define RT_GET_HT_40M_START_RIX (HT_40_RATE_TABLE_INDEX + 4)
#define RT_GET_VHT_20M_START_RIX (VHT_20_RATE_TABLE_INDEX + 4)
#define RT_GET_VHT_40M_START_RIX (VHT_40_RATE_TABLE_INDEX + 4)
#define RT_GET_VHT_80M_START_RIX (VHT_80_RATE_TABLE_INDEX + 4)

/*
 * The following functions are used to get rate mask for non-HT,
 * HT 20/40, and VHT 20/40/80 Modes.
 */

static inline void RT_GET_RIX_LIMITS_4_MASK(A_UINT8 *upper,
   A_UINT8 *lower, WHAL_RC_MASK_IDX idx)
{
    static const A_UINT8 _rix_lower_limits[WHAL_RC_MASK_MAX_IDX + 1] =
        {  /*[WHAL_RC_MASK_IDX_NON_HT]  */ 0,
            /*[WHAL_RC_MASK_IDX_HT_20]  */ HT_20_RATE_TABLE_INDEX,
            /*[WHAL_RC_MASK_IDX_HT_40]  */ HT_40_RATE_TABLE_INDEX,
            /*[WHAL_RC_MASK_IDX_VHT_20] */ VHT_20_RATE_TABLE_INDEX,
            /*[WHAL_RC_MASK_IDX_VHT_40] */ VHT_40_RATE_TABLE_INDEX,
            /*[WHAL_RC_MASK_IDX_VHT_80] */ VHT_80_RATE_TABLE_INDEX,
        };
    static const A_UINT8 _rix_upper_limits[WHAL_RC_MASK_MAX_IDX + 1] =
        {  /* [WHAL_RC_MASK_IDX_NON_HT]  */ HT_20_RATE_TABLE_INDEX -1,
           /* [WHAL_RC_MASK_IDX_HT_20]   */ RT_GET_HT_20M_MAX_RATE,
           /* [WHAL_RC_MASK_IDX_HT_40]   */ RT_GET_HT_40M_MAX_RATE,
           /* [WHAL_RC_MASK_IDX_VHT_20]  */ RT_GET_VHT_20M_MAX_RATE,
           /* [WHAL_RC_MASK_IDX_VHT_40]  */ RT_GET_VHT_40M_MAX_RATE,
           /* [WHAL_RC_MASK_IDX_VHT_80]  */ RT_GET_VHT_80M_MAX_RATE,
        };

    A_ASSERT(idx <=WHAL_RC_MASK_MAX_IDX);
    *upper = _rix_upper_limits[idx];
    *lower = _rix_lower_limits[idx];

}

static inline void RT_GET_RC_MASK_4_RIX(void * rt, A_UINT32 rix,
    A_UINT32 *rc_mask_idx, A_UINT32 *rc_bit_idx)
{
    A_UINT32 mask_idx, bit_idx;

    static const A_UINT8 _rc_mask_idx[WHAL_MOD_IEEE80211_T_MAX_PHY] = {
        /*[WHAL_MOD_IEEE80211_T_DS]     */ WHAL_RC_MASK_IDX_NON_HT,
        /*[WHAL_MOD_IEEE80211_T_OFDM]   */ WHAL_RC_MASK_IDX_NON_HT,
        /*[WHAL_MOD_IEEE80211_T_HT_20]  */ WHAL_RC_MASK_IDX_HT_20,
        /*[WHAL_MOD_IEEE80211_T_HT_40]  */ WHAL_RC_MASK_IDX_HT_40,
        /*[WHAL_MOD_IEEE80211_T_VHT_20] */ WHAL_RC_MASK_IDX_VHT_20,
        /*[WHAL_MOD_IEEE80211_T_VHT_40] */ WHAL_RC_MASK_IDX_VHT_40,
        /*[WHAL_MOD_IEEE80211_T_VHT_80] */ WHAL_RC_MASK_IDX_VHT_80};

    static const A_UINT8 _rc_bit_off[WHAL_MOD_IEEE80211_T_MAX_PHY] = {
        /*[WHAL_MOD_IEEE80211_T_DS]     */ CCK_RATE_TABLE_INDEX,
        /*[WHAL_MOD_IEEE80211_T_OFDM]   */ CCK_RATE_TABLE_INDEX,
        /*[WHAL_MOD_IEEE80211_T_HT_20]  */ HT_20_RATE_TABLE_INDEX,
       /*[WHAL_MOD_IEEE80211_T_HT_40]   */ HT_40_RATE_TABLE_INDEX,
        /*[WHAL_MOD_IEEE80211_T_VHT_20] */ VHT_20_RATE_TABLE_INDEX,
        /*[WHAL_MOD_IEEE80211_T_VHT_40] */ VHT_40_RATE_TABLE_INDEX,
        /*[WHAL_MOD_IEEE80211_T_VHT_80] */ VHT_80_RATE_TABLE_INDEX};

    WHAL_WLAN_MODULATION_TYPE mod = RT_GET_PHY(rt,rix);
    mask_idx = _rc_mask_idx[mod];
    bit_idx = rix - _rc_bit_off[mod];

    A_ASSERT(mask_idx <= WHAL_RC_MASK_MAX_IDX
           && bit_idx < A_RATEMASK_NUM_BITS);

    *rc_mask_idx = mask_idx;
    *rc_bit_idx = bit_idx;

}

#define WHAL_HT40_CHANNEL_CENTER_SHIFT  10 /* MHz */



typedef enum {
    WHAL_WLAN_11A_CAPABILITY   = 1,
    WHAL_WLAN_11G_CAPABILITY   = 2,
    WHAL_WLAN_11AG_CAPABILITY  = 3,
} WHAL_WLAN_MODE_CAPABILITY;


typedef enum {
    WHAL_CHANNEL_HALF_RATE          = 0x1, /* Half rate */
    WHAL_CHANNEL_QUARTER_RATE       = 0x2, /* Quarter rate */
} WHAL_CHANNEL_FLAGS;

#define WHAL_GET_WLAN_MODE_CAPABILITY(opFlags)  (WHAL_WLAN_MODE_CAPABILITY)((opFlags) & 0xFF)

#define WHAL_MAX_ANACFG_LEN          324
#define WHAL_RSSI_EP_MULTIPLIER  (1<<7)  /* pow2 to optimize out * and / */
#define WHAL_THEORETICAL_NOISE_FLOOR (-95)

#define WHAL_IS_CHAN_A(_c)       (((_c)->phy_mode == MODE_11A) || \
                                  ((_c)->phy_mode == MODE_11NA_HT20) || \
                                  ((_c)->phy_mode == MODE_11NA_HT40) || \
                                  (IS_MODE_VHT((_c)->phy_mode)))

#define WHAL_IS_CHAN_B(_c)       ((_c)->phy_mode == MODE_11B)
#define WHAL_IS_CHAN_G(_c)       (((_c)->phy_mode == MODE_11G) || \
                                  ((_c)->phy_mode == MODE_11GONLY) || \
                                  ((_c)->phy_mode == MODE_11NG_HT20) || \
                                  ((_c)->phy_mode == MODE_11NG_HT40))
#define WHAL_IS_CHAN_GONLY(_c)   ((_c)->phy_mode == MODE_11GONLY)
#define WHAL_IS_CHAN_CCK(_c)     ((_c)->phy_mode == MODE_11B)
#define WHAL_IS_CHAN_OFDM(_c)    (!(WHAL_IS_CHAN_CCK(_c)))
#define WHAL_IS_CHAN_5GHZ(_c)    (WHAL_IS_CHAN_A(_c))
#define WHAL_IS_CHAN_2GHZ(_c)    (!(WHAL_IS_CHAN_5GHZ(_c)))
#define WHAL_IS_CHAN_HT40(_c)    (((_c)->phy_mode == MODE_11NA_HT40) || \
                                  ((_c)->phy_mode == MODE_11NG_HT40) ||\
                                  ((_c)->phy_mode == MODE_11AC_VHT40))

#define WHAL_IS_CHAN_HT20(_c)    (((_c)->phy_mode == MODE_11NA_HT20) || \
                                  ((_c)->phy_mode == MODE_11NG_HT20) || \
                                    ((_c)->phy_mode == MODE_11AC_VHT20))
#define WHAL_IS_CHAN_HT40PLUS(_c) ((WHAL_IS_CHAN_HT40((_c))) && \
                                   ((_c)->flags & WHAL_CHANNEL_HT40PLUS))


#define WHAL_IS_CHAN_VHT20(_c)  (((_c)->phy_mode == MODE_11AC_VHT20))
#define WHAL_IS_CHAN_VHT40(_c)  (((_c)->phy_mode == MODE_11AC_VHT40))
#define WHAL_IS_CHAN_VHT80(_c)  (((_c)->phy_mode == MODE_11AC_VHT80))
#define WHAL_IS_CHAN_VHT160(_c)  (((_c)->phy_mode == MODE_11AC_VHT160))
#define WHAL_IS_CHAN_VHT80_80(_c)  (((_c)->phy_mode == MODE_11AC_VHT80_80))

#define WHAL_IS_CHAN_HALF_RATE(_c) (((_c)->flags & WHAL_CHANNEL_HALF_RATE) != 0)
#define WHAL_IS_CHAN_QUARTER_RATE(_c) (((_c)->flags & WHAL_CHANNEL_QUARTER_RATE) != 0)


/*
 * Status codes that may be returned by the WHAL.  Note that
 * interfaces that return a status code set it only when an
 * error occurs--i.e. you cannot check it for success.
 */
typedef enum {
    WHAL_OK      = 0,    /* No error */
    WHAL_ENXIO   = 1,    /* No hardware present */
    WHAL_ENOMEM  = 2,    /* Memory allocation failed */
    WHAL_EIO     = 3,    /* Hardware didn't respond as expected */
    WHAL_EEMAGIC = 4,    /* EEPROM magic number invalid */
    WHAL_EEVERSION   = 5,    /* EEPROM version invalid */
    WHAL_EELOCKED    = 6,    /* EEPROM unreadable */
    WHAL_EEBADSUM    = 7,    /* EEPROM checksum invalid */
    WHAL_EEREAD  = 8,    /* EEPROM read problem */
    WHAL_EEBADMAC    = 9,    /* EEPROM mac address invalid */
    WHAL_EESIZE  = 10,   /* EEPROM size not supported */
    WHAL_EEWRITE = 11,   /* Attempt to change write-locked EEPROM */
    WHAL_EINVAL  = 12,   /* Invalid parameter to function */
    WHAL_ENOTSUPP    = 13,   /* Hardware revision not supported */
    WHAL_ESELFTEST   = 14,   /* Hardware self-test failed */
    WHAL_EINPROGRESS = 15,   /* Operation incomplete */
    WHAL_MOREPENDING = 16,   /* Rx - haven't reached the last of the desc chain */
    WHAL_EBUSY       = 17,   /* Hardware is busy */
} WHAL_STATUS;

typedef enum {
    WHAL_RESET_SW_SETTINGS = 1,  /* Reset the h/w and s/w state */
    WHAL_RESTORE_SW_SETTINGS = 2, /* Reset the h/w state and restore s/w state */
} WHAL_RESET_FLAGS;

typedef enum {
    WHAL_RESET_CAUSE_FIRST_RESET      = 0x01, /* First reset by application */
    WHAL_RESET_CAUSE_BAND_CHANGE      = 0x02, /* Trigered due to band change */
    WHAL_RESET_CAUSE_CHWIDTH_CHANGE   = 0x04, /* Trigered due to channel width change */
    WHAL_RESET_CAUSE_ERROR            = 0x08, /* Trigered due to error */
    WHAL_RESET_CAUSE_CHANNEL_CHANGE   = 0x10, /* For normal channel change */
    WHAL_RESET_CAUSE_DEEP_SLEEP       = 0x20, /* Reset after deep sleep */
} WHAL_RESET_CAUSE;

/*
 * Receive queue types.  These are used to tag
 * each transmit queue in the hardware and to identify a set
 * of transmit queues for operations such as start/stop dma.
 */
typedef enum {
    WHAL_RX_QUEUE_LP   = 0,        /* low priority recv queue */
    WHAL_RX_QUEUE_HP   = 1,        /* high priority recv queue */
} WHAL_RX_QUEUE;

#define WHAL_LP_FIFO_DEPTH_MAX 128 /* Max depth of low priority RX FIFO */
#define WHAL_HP_FIFO_DEPTH_MAX 16 /* Max depth of high priority RX FIFO */

#define WHAL_CALIBRATION_ATTEMPTS   (2)


/** From ar6000_phy.h */

typedef struct {
    const WHAL_RATE_TABLE       *pHwRateTable[MODE_MAX];
    A_UINT32 coexMode;
    A_UINT32 coexWeights;
    A_UINT32 coexMode2;
    A_BOOL   absRssiEn;
    A_UINT32 synthSettleDelay;
    A_UINT32 synthSettleDelayReset;
    A_UINT32 refClkMultiplier;
} AR6000_PHY_STRUCT;

void *ar6000ChipSpecificPhyAttach(void);

/** From ar600P_desc.h */

#define AR600P_ASSEMBLE_HW_RATECODE(_rate, _nss, _pream)     \
            (((_pream) << 6) | ((_nss) << 4) | (_rate))
#define AR600P_GET_HW_RATECODE_PREAM(_rcode)     (((_rcode) >> 6) & 0x3)
#define AR600P_GET_HW_RATECODE_NSS(_rcode)       (((_rcode) >> 4) & 0x3)
#define AR600P_GET_HW_RATECODE_RATE(_rcode)      (((_rcode) >> 0) & 0xF)

#define HW_RATECODE_CCK_SHORT_PREAM_MASK  0x4
#define IS_HW_RATECODE_CCK(_rc) ((((_rc) >> 6) & 0x3) == 1)
#define IS_CCK_SHORT_PREAM_RC(_rc) (((_rc) & 0xcc) == 0x44)
#define IS_HW_RATECODE_HT(_rc)  (((_rc) & 0xc0) == 0x80)
#define IS_HW_RATECODE_VHT(_rc) (((_rc) & 0xc0) == 0xc0)

enum AR600P_HW_RATECODE_PREAM_TYPE {
    AR600P_HW_RATECODE_PREAM_OFDM,
    AR600P_HW_RATECODE_PREAM_CCK,
    AR600P_HW_RATECODE_PREAM_HT,
    AR600P_HW_RATECODE_PREAM_VHT,
};


/** For ol_ratetable.c */
const WHAL_RATE_TABLE *whalGetRateTable(WLAN_PHY_MODE wlanMode);
int whal_mcs_to_kbps(int preamb, int mcs, int htflag, int gintval);
int whal_ratecode_to_kbps(uint8_t ratecode, uint8_t bw, uint8_t gintval);
int whal_rate_idx_to_kbps(uint8_t rate_idx, uint8_t gintval);
#if ALL_POSSIBLE_RATES_SUPPORTED
int whal_get_supported_rates(int htflag, int shortgi, int **rates);
int whal_kbps_to_mcs(int kbps_rate, int shortgi, int htflag);
#else
int whal_kbps_to_mcs(int kbps_rate, int shortgi, int htflag, int nss, int ch_width);
int whal_get_supported_rates(int htflag, int shortgi, int nss, int ch_width, int **rates);
#endif

#endif /* OL_RATETABLE_H_ */
