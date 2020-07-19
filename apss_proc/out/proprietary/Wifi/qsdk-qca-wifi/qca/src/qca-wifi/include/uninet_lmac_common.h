/*
* Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 * Copyright (c) 2010, Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */

/*
 * This module define the constant and enum that are used between the upper
 * net80211 layer and lower device ath layer.
 * NOTE: Please do not add any functional prototype in here and also do not
 * include any other header files.
 * Only constants, enum, and structure definitions.
 */

#ifndef UNINET_LMAC_COMMON_H
#define UNINET_LMAC_COMMON_H

/* The following define the various VAP Information Types to register for callbacks */
typedef u_int32_t   ath_vap_infotype;
#define ATH_VAP_INFOTYPE_SLOT_OFFSET    (1<<0)

enum ieee80211_clist_cmd {
	CLIST_UPDATE,
	CLIST_DFS_UPDATE,
	CLIST_NEW_COUNTRY,
	CLIST_NOL_UPDATE
};

struct ieee80211_ba_seqctrl {
#if _BYTE_ORDER == _BIG_ENDIAN
        u_int16_t   startseqnum     : 12,    /* B4-15  starting sequence number */
                    fragnum         : 4;     /* B0-3  fragment number */
#else
        u_int16_t   fragnum         : 4,     /* B0-3  fragment number */
                    startseqnum     : 12;    /* B4-15  starting sequence number */
#endif
} __packed;

struct ieee80211_ba_parameterset {
#if _BYTE_ORDER == _BIG_ENDIAN
        u_int16_t   buffersize      : 10,   /* B6-15  buffer size */
                    tid             : 4,    /* B2-5   TID */
                    bapolicy        : 1,    /* B1   block ack policy */
                    amsdusupported  : 1;    /* B0   amsdu supported */
#else
        u_int16_t   amsdusupported  : 1,    /* B0   amsdu supported */
                    bapolicy        : 1,    /* B1   block ack policy */
                    tid             : 4,    /* B2-5   TID */
                    buffersize      : 10;   /* B6-15  buffer size */
#endif
} __packed;

struct ieee80211_delba_parameterset {
#if _BYTE_ORDER == _BIG_ENDIAN
        u_int16_t   tid             : 4,     /* B12-15  tid */
                    initiator       : 1,     /* B11     initiator */
                    reserved0       : 11;    /* B0-10   reserved */
#else
        u_int16_t   reserved0       : 11,    /* B0-10   reserved */
                    initiator       : 1,     /* B11     initiator */
                    tid             : 4;     /* B12-15  tid */
#endif
} __packed;

#define IEEE80211_IS_BROADCAST(_a)              \
    ((_a)[0] == 0xff &&                         \
     (_a)[1] == 0xff &&                         \
     (_a)[2] == 0xff &&                         \
     (_a)[3] == 0xff &&                         \
     (_a)[4] == 0xff &&                         \
     (_a)[5] == 0xff)

#define IEEE80211_BMISS_LIMIT               15

#define IEEE80211_IS_BEACON(_frame)    ((((_frame)->i_fc[0] & IEEE80211_FC0_TYPE_MASK) == IEEE80211_FC0_TYPE_MGT) && \
                                        (((_frame)->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) == IEEE80211_FC0_SUBTYPE_BEACON))
#define IEEE80211_IS_DATA(_frame)      (((_frame)->i_fc[0] & IEEE80211_FC0_TYPE_MASK) == IEEE80211_FC0_TYPE_DATA)
#define IEEE80211_IS_PROBEREQ(_frame)  ((((_frame)->i_fc[0] & IEEE80211_FC0_TYPE_MASK) == IEEE80211_FC0_TYPE_MGT) && \
                                        (((_frame)->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) == IEEE80211_FC0_SUBTYPE_PROBE_REQ))

/*
 * 11n A-MPDU & A-MSDU limits
 */
#define IEEE80211_AMPDU_LIMIT_MIN           (1 * 1024)
#define IEEE80211_AMPDU_LIMIT_MAX           (64 * 1024 - 1)
#define IEEE80211_AMPDU_LIMIT_DEFAULT       IEEE80211_AMPDU_LIMIT_MAX
#define IEEE80211_AMPDU_SUBFRAME_MIN        2 
#define IEEE80211_AMPDU_SUBFRAME_MAX        64 
#define IEEE80211_AMPDU_SUBFRAME_DEFAULT    32 
#define IEEE80211_AMSDU_LIMIT_MAX           4096
#define IEEE80211_RIFS_AGGR_DIV             10

#define IEEE80211_HTCAP_MAXRXAMPDU_FACTOR   13

#define  IEEE80211_BA_POLICY_DELAYED      0
#define  IEEE80211_BA_POLICY_IMMEDIATE    1
#define  IEEE80211_BA_AMSDU_SUPPORTED     1

/*
 * BAR frame format
 */
#define IEEE80211_BAR_CTL_TID_M     0xF000      /* tid mask             */
#define IEEE80211_BAR_CTL_TID_S         12      /* tid shift            */
#define IEEE80211_BAR_CTL_NOACK     0x0001      /* no-ack policy        */
#define IEEE80211_BAR_CTL_COMBA     0x0004      /* compressed block-ack */

#define IEEE80211_SEQ_MAX           IEEE80211_SEQ_RANGE           

#define  IEEE80211_STATUS_REFUSED         37

#define IEEE80211_RATE_IDX_ENTRY(val, idx) (((val&(0xff<<(idx*8)))>>(idx*8)))

struct ieee80211_ctlframe_addr2 {
    u_int8_t    i_fc[2];
    u_int8_t    i_aidordur[2]; /* AID or duration */
    u_int8_t    i_addr1[6];
    u_int8_t    i_addr2[6];
} __packed;

/*
 * WMM Power Save (a.k.a U-APSD) Configuration Parameters
 */
typedef struct _ieee80211_wmmPowerSaveConfig {
    u_int8_t    vo;    /* true=U-APSD for AC_VO, false=legacy power save */
    u_int8_t    vi;    /* true=U-APSD for AC_VI, false=legacy power save */
    u_int8_t    bk;    /* true=U-APSD for AC_BK, false=legacy power save */
    u_int8_t    be;    /* true=U-APSD for AC_BE, false=legacy power save */
} IEEE80211_WMMPOWERSAVE_CFG;

typedef struct ieee80211_reg_parameters {
    u_int8_t        tx_dc_active_pct;                 /* tx active duty cycle percent */    
    u_int32_t       tx_busy_pct;                      /* tx busy cycle percent */
} IEEE80211_REG_PARAMETERS, *PIEEE80211_REG_PARAMETERS;

enum IEEE80211_REG_PARM_IOCTLS
{
    IEEE80211_REG_PARM_IOC_TX_ACTIVE_DUTY_CYCLE = 4600,  /* tx active percent */
    IEEE80211_REG_PARM_IOC_TX_BUSY_CYCLE,                /* tx busy percent */
};

typedef struct ieee80211_country_entry{
    u_int16_t   countryCode;  /* HAL private */
    u_int16_t   regDmnEnum;   /* HAL private */
    u_int16_t   regDmn5G;
    u_int16_t   regDmn2G;
    u_int8_t    isMultidomain;
    u_int8_t    iso[3];       /* ISO CC + (I)ndoor/(O)utdoor or both ( ) */
} IEEE80211_COUNTRY_ENTRY;

#define IEEE80211_RSSI_RX       0x00000001
#define IEEE80211_RSSI_TX       0x00000002
#define IEEE80211_RSSI_EXTCHAN  0x00000004
#define IEEE80211_RSSI_BEACON   0x00000008
#define IEEE80211_RSSI_RXDATA   0x00000010

#define IEEE80211_RATE_TX 0
#define IEEE80211_RATE_RX 1
#define IEEE80211_LASTRATE_TX 2
#define IEEE80211_LASTRATE_RX 3
#define IEEE80211_RATECODE_TX 4
#define IEEE80211_RATECODE_RX 5
#define IEEE80211_RATEFLAGS_TX 6

/*
 * *************************
 * Update PHY stats
 * *************************
 */
#if !ATH_SUPPORT_STATS_APONLY
#define WLAN_PHY_STATS(_phystat, _field)        _phystat->_field ++
#else
#define WLAN_PHY_STATS(_phystat, _field)
#endif

#define IEEE80211_MAX_MPDU_LEN      (3840 + IEEE80211_CRC_LEN + \
    (IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN + IEEE80211_WEP_CRCLEN))

#ifdef ATH_USB
#define    ATH_BCBUF    8        /* should match ATH_USB_BCBUF defined in ath_usb.h */#elif ATH_SUPPORT_AP_WDS_COMBO
#define    ATH_BCBUF    16       /* number of beacon buffers for 16 VAP support */
#elif  defined(QCA_LOWMEM_PLATFORM)
#define    ATH_BCBUF    4       /* number of beacon buffers for 4 VAP support */
#elif IEEE80211_PROXYSTA
#define    ATH_BCBUF    17       /* number of beacon buffers for 17 VAP support (STA + 16 proxies)*/
#else
#define    ATH_BCBUF    8        /* number of beacon buffers */
#endif
#define ATH_SET_VAP_BSSID_MASK(bssid_mask)      ((bssid_mask)[0] &= ~(((ATH_BCBUF-1) << 4) | 0x02))
#define ATH_SET_VAP_BSSID(bssid, hwbssid, id)                        \
    do {                                                             \
        if (id) {                                                    \
            u_int8_t hw_bssid = (hwbssid[0] >> 4) & (ATH_BCBUF - 1); \
            u_int8_t tmp_bssid;                                      \
                                                                     \
            (bssid)[0] &= ~((ATH_BCBUF - 1) << 4);                   \
            tmp_bssid = (((id-1) + hw_bssid) & (ATH_BCBUF - 1));     \
            (bssid)[0] |= (((tmp_bssid) << 4) | 0x02);               \
        }                                                            \
    } while(0)

#endif  //UNINET_LMAC_COMMON_H
