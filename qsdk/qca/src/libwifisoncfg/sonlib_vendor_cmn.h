/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

/* SON Libraries - Commond Vendor specific module header file */

#ifndef __SONLIB_VENDOR_CMN_H__
#define __SONLIB_VENDOR_CMN_H__

/****************************************************************************/
/****** #enums and Marcos need to be defined in Vendor.h for 3rd party ******/
/****** These enums and preprocessor marcos are used in SON code ************/
/****************************************************************************/

// 3rd party vendor should include this file in their vendor specific C file
// SON modules use these enums. 3rd party needs to translate their information
// and use these enums and structure for communication of information.


/* Channel width enum. Currently, SON expects bandwidth */
/* reported by driver to be one of this enum value.     */
/* Channel width is reported via "max_chwidth" field in */
/* struct soncmn_ieee80211_bsteering_datarate_info_t &  */
/* struct ieee80211_bsteering_datarate_info_t           */
enum ieee80211_cwm_width {
    IEEE80211_CWM_WIDTH20,
    IEEE80211_CWM_WIDTH40,
    IEEE80211_CWM_WIDTH80,
    IEEE80211_CWM_WIDTH160,
    IEEE80211_CWM_WIDTHINVALID = 0xff    /* user invalid value */
};

/* PHY mode enum. Currently, SON expects PHY mode from  */
/* driver to be one of this enum value. PHY mode is     */
/* reported via "phymode" field in following struct:    */
/* struct soncmn_ieee80211_bsteering_datarate_info_t &  */
/* struct ieee80211_bsteering_datarate_info_t           */
enum ieee80211_phymode {
    IEEE80211_MODE_AUTO             = 0,    /* autoselect */
    IEEE80211_MODE_11A              = 1,    /* 5GHz, OFDM */
    IEEE80211_MODE_11B              = 2,    /* 2GHz, CCK */
    IEEE80211_MODE_11G              = 3,    /* 2GHz, OFDM */
    IEEE80211_MODE_FH               = 4,    /* 2GHz, GFSK */
    IEEE80211_MODE_TURBO_A          = 5,    /* 5GHz, OFDM, 2x clock dynamic turbo */
    IEEE80211_MODE_TURBO_G          = 6,    /* 2GHz, OFDM, 2x clock dynamic turbo */
    IEEE80211_MODE_11NA_HT20        = 7,    /* 5Ghz, HT20 */
    IEEE80211_MODE_11NG_HT20        = 8,    /* 2Ghz, HT20 */
    IEEE80211_MODE_11NA_HT40PLUS    = 9,    /* 5Ghz, HT40 (ext ch +1) */
    IEEE80211_MODE_11NA_HT40MINUS   = 10,   /* 5Ghz, HT40 (ext ch -1) */
    IEEE80211_MODE_11NG_HT40PLUS    = 11,   /* 2Ghz, HT40 (ext ch +1) */
    IEEE80211_MODE_11NG_HT40MINUS   = 12,   /* 2Ghz, HT40 (ext ch -1) */
    IEEE80211_MODE_11NG_HT40        = 13,   /* 2Ghz, Auto HT40 */
    IEEE80211_MODE_11NA_HT40        = 14,   /* 2Ghz, Auto HT40 */
    IEEE80211_MODE_11AC_VHT20       = 15,   /* 5Ghz, VHT20 */
    IEEE80211_MODE_11AC_VHT40PLUS   = 16,   /* 5Ghz, VHT40 (Ext ch +1) */
    IEEE80211_MODE_11AC_VHT40MINUS  = 17,   /* 5Ghz  VHT40 (Ext ch -1) */
    IEEE80211_MODE_11AC_VHT40       = 18,   /* 5Ghz, VHT40 */
    IEEE80211_MODE_11AC_VHT80       = 19,   /* 5Ghz, VHT80 */
    IEEE80211_MODE_11AC_VHT160      = 20,   /* 5Ghz, VHT160 */
    IEEE80211_MODE_11AC_VHT80_80    = 21,   /* 5Ghz, VHT80_80 */
};

#define IEEE80211_MODE_MAX      (IEEE80211_MODE_11AC_VHT80_80 + 1)
#define IEEE80211_MODE_NONE      (IEEE80211_MODE_MAX + 1)

/* BSTM MBO/QCN Transition Request Reason Code.  */
/* These are as per standard, and currently      */
/* used by SON as reason code while sending BSTM */
/* to WLAN driver.                               */
enum IEEE80211_BSTM_REQ_REASON_CODE {
    IEEE80211_BSTM_REQ_REASON_UNSPECIFIED,
    IEEE80211_BSTM_REQ_REASON_FRAME_LOSS_RATE,
    IEEE80211_BSTM_REQ_REASON_DELAY_FOR_TRAFFIC,
    IEEE80211_BSTM_REQ_REASON_INSUFFICIENT_BANDWIDTH,
    IEEE80211_BSTM_REQ_REASON_LOAD_BALANCING,
    IEEE80211_BSTM_REQ_REASON_LOW_RSSI,
    IEEE80211_BSTM_REQ_REASON_EXCESSIVE_RETRANSMISSION,
    IEEE80211_BSTM_REQ_REASON_HIGH_INTERFERENCE,
    IEEE80211_BSTM_REQ_REASON_GRAY_ZONE,
    IEEE80211_BSTM_REQ_REASON_PREMIUM_AP,

    IEEE80211_BSTM_REQ_REASON_INVALID
};

/* Regulatory class value. These are used by SON */
/* to define regulatory class in Beacon request  */
/* from SON to driver during steering process.   */
/* Enum value used for "regclass" field in struct*/
/* soncmn_ieee80211_beaconreq_chaninfo           */
typedef enum {
    IEEE80211_RRM_REGCLASS_81 = 81,
    IEEE80211_RRM_REGCLASS_82 = 82,
    IEEE80211_RRM_REGCLASS_112 = 112,
    IEEE80211_RRM_REGCLASS_115 = 115,
    IEEE80211_RRM_REGCLASS_118 = 118,
    IEEE80211_RRM_REGCLASS_121 = 121,
    IEEE80211_RRM_REGCLASS_124 = 124,
    IEEE80211_RRM_REGCLASS_125 = 125,

    IEEE80211_RRM_REGCLASS_RESERVED
} IEEE80211_RRM_REGCLASS;

/* Beacon Request measurement mode enum. These  */
/* as per standard. SON uses these values while */
/* sending Beacon request to driver during      */
/* steering process.                            */
typedef enum {
    IEEE80211_RRM_BCNRPT_MEASMODE_PASSIVE = 0,
    IEEE80211_RRM_BCNRPT_MEASMODE_ACTIVE = 1,
    IEEE80211_RRM_BCNRPT_MEASMODE_BCNTABLE = 2,

    IEEE80211_RRM_BCNRPT_MEASMODE_RESERVED
} IEEE80211_RRM_BCNRPT_MEASMODE;


/* event code for Channel change  */
#define  IEEE80211_EV_CHAN_CHANGE 28



#endif /* #define __SONLIB_VENDOR_CMN_H__ */
