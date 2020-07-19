/*
 * Copyright (c) 2011,2017-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * Copyright (c) 2005, Atheros Communications Inc.
 * All Rights Reserved.

 */

#ifndef ATH_DEV_H
#define ATH_DEV_H

/*
 * @file ath_dev.h
 * Public Interface of ATH layer
 */

#include <osdep.h>
#include <ah.h>
#include <wbuf.h>
#include <if_athioctl.h>

#include "wlan_lmac_if_def.h"
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_global_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#ifdef UNINET
#include <net80211/ieee80211.h>
#include <net80211/_ieee80211.h>
#include <uninet_lmac_common.h>
#else
#include <ieee80211.h>
#include <_ieee80211.h>
#include <umac_lmac_common.h>
#endif
#if ATH_SUPPORT_WIFIPOS
/* find a cleaner way to include this file */
#include <ieee80211_wifipos.h>
#endif

#include "ath_opts.h"
#ifdef ATH_BT_COEX
#include "ath_bt_registry.h"
#endif

#if QCA_AIRTIME_FAIRNESS
#include <wlan_atf_utils_defs.h>
#endif /* QCA_AIRTIME_FAIRNESS */

struct ath_spectral_ops;

/**
 * @defgroup ath_dev ath_dev - Atheros Device Module
 * @{
 * ath_dev implements the Atheros' specific low-level functions of the wireless driver.
 */

/**
 * @brief Clients of ATH layer call ath_attach to obtain a reference to an ath_dev structure.
 * Hardware-related operation that follow must call back into ATH through interface,
 * supplying the reference as the first parameter.
 */
typedef void *ath_dev_t;

#include "ath_hwtimer.h"

/**
 * @brief Opaque handle of an associated node (including BSS itself).
 */
typedef void *ath_node_t;

/**
 * @defgroup ieee80211_if - 802.11 Protocal Interface required by Atheros Device Module
 * @{
 * @ingroup ath_dev
 */

/**
 * @brief Opaque handle of 802.11 protocal layer.
 * ATH module must call back into protocal layer through callbacks passed in attach time,
 * supplying the reference as the first parameter.
 */
typedef void *ieee80211_handle_t;

/**
 * @brief Opaque handle of network instance in 802.11 protocal layer.
 */
typedef void *ieee80211_if_t;

/**
 * @brief Opaque handle of per-destination information in 802.11 protocal layer.
 */
typedef void *ieee80211_node_t;

#if ATH_SUPPORT_HYFI_ENHANCEMENTS
struct ath_linkdiag;
typedef struct ath_linkdiag *ath_ald_t;
struct ath_ni_linkdiag;
typedef struct ath_ni_linkdiag *ath_ni_ald_t;
#endif

struct ath_reg_parm;

/*
 * Number of (OEM-defined) functions using GPIO pins currently defined
 *
 * Function 0: Link/Power LED
 * Function 1: Network/Activity LED
 * Function 2: Connection LED
 */
#define NUM_GPIO_FUNCS             3

/**
 * @brief Wireless Mode Definition
 */
typedef enum {
    WIRELESS_MODE_11a = 0,
    WIRELESS_MODE_11b,
    WIRELESS_MODE_11g,
    WIRELESS_MODE_108a,
    WIRELESS_MODE_108g,
    WIRELESS_MODE_11NA_HT20,
    WIRELESS_MODE_11NG_HT20,
    WIRELESS_MODE_11NA_HT40PLUS,
    WIRELESS_MODE_11NA_HT40MINUS,
    WIRELESS_MODE_11NG_HT40PLUS,
    WIRELESS_MODE_11NG_HT40MINUS,
    WIRELESS_MODE_XR
} WIRELESS_MODE;

#define WIRELESS_MODE_MAX      (WIRELESS_MODE_XR + 1)

/*
 * @breif Wireless Mode Mask
 */
typedef enum {
    MODE_SELECT_11A       = 0x00001,
    MODE_SELECT_TURBO     = 0x00002,
    MODE_SELECT_11B       = 0x00004,
    MODE_SELECT_11G       = 0x00008,
    MODE_SELECT_108G      = 0x00020,
    MODE_SELECT_108A      = 0x00040,
    MODE_SELECT_XR        = 0x00100,
    MODE_SELECT_49_27     = 0x00200,
    MODE_SELECT_49_54     = 0x00400,
    MODE_SELECT_11NG_HT20 = 0x00800,
    MODE_SELECT_11NA_HT20 = 0x01000,
    MODE_SELECT_11NG_HT40PLUS  = 0x02000,       /* 11N-G HT40+ channels */
    MODE_SELECT_11NG_HT40MINUS = 0x04000,       /* 11N-G HT40- channels */
    MODE_SELECT_11NA_HT40PLUS  = 0x08000,       /* 11N-A HT40+ channels */
    MODE_SELECT_11NA_HT40MINUS = 0x10000,       /* 11N-A HT40- channels */

    MODE_SELECT_2GHZ      = (MODE_SELECT_11B | MODE_SELECT_11G | MODE_SELECT_108G | MODE_SELECT_11NG_HT20),

    MODE_SELECT_5GHZ      = (MODE_SELECT_11A | MODE_SELECT_TURBO | MODE_SELECT_108A | MODE_SELECT_11NA_HT20),

    MODE_SELECT_ALL       = (MODE_SELECT_5GHZ | MODE_SELECT_2GHZ | MODE_SELECT_49_27 | MODE_SELECT_49_54 | MODE_SELECT_11NG_HT40PLUS | MODE_SELECT_11NG_HT40MINUS | MODE_SELECT_11NA_HT40PLUS | MODE_SELECT_11NA_HT40MINUS),

} WIRELESS_MODES_SELECT;

/*
 * Spatial Multiplexing Modes.
 */
typedef enum {
    ATH_SM_ENABLE,
    ATH_SM_PWRSAV_STATIC,
    ATH_SM_PWRSAV_DYNAMIC,
} ATH_SM_PWRSAV;

/*
 * Rate
 */
typedef enum {
    NORMAL_RATE = 0,
    HALF_RATE,
    QUARTER_RATE
} RATE_TYPE;

/**
 * @brief Protection Moide
 */
typedef enum {
    PROT_M_NONE = 0, /* no protection */
    PROT_M_RTSCTS,  /* uses RTS/CTS before every OFDM frame */
    PROT_M_CTSONLY  /* uses self CTS before every OFDM frame */
} PROT_MODE;

/**
 * @brief beacon configuration
 */
typedef struct {
    u_int16_t       beacon_interval; /* beacon interval in TUs */
    u_int16_t       listen_interval; /* listen interval in number of beacon intervals */
    u_int16_t       dtim_period;     /* dtim interval in number of beacon intervals */
    u_int16_t       bmiss_timeout;   /* time in TUs for the HW beacon miss timeout logic to detect beacon miss and generated BMISS interrupt (for station) */
    u_int8_t        dtim_count;      /* current dtim count from the latest beacon */
    u_int8_t        tim_offset;      /* currently unused */
    union {
        u_int64_t   last_tsf;
        u_int8_t    last_tstamp[8];
    } u;    /* last received beacon/probe response timestamp of this BSS. */
} ieee80211_beacon_config_t;

/**
 * @brief offsets in a beacon frame for
 * quick acess of beacon content by low-level driver
 */
typedef struct {
    u_int8_t        *bo_tim;    /* start of atim/dtim */
#if UMAC_SUPPORT_WNM
    u_int8_t        *bo_fms_desc; /* start of FMS descriptor */
#define FMS_CURR_COUNT_MASK     0xF8 /* mask of current count in the fms counter */
#endif /* UMAC_SUPPORT_WNM */
} ieee80211_beacon_offset_t;

/**
 * @brief per-frame tx control block
 */
/*
 * SHARE_CTRL_BLK_TX_CTRL_T: A block of share information between tx control
 * block and ath_buf_state. We create this share block to facilitate the copy
 * operations in ath_tx_start_dma.
 */
#define SHARE_CTRL_INFO_BEGIN       \
    union {                         \
        struct {
#define SHARE_CTRL_INFO_END         \
        };                          \
        char _head;                 \
    }
#define SHARE_CTRL_INFO_HEAD(arg)   \
    (&((arg)->_head))
/*
 * Descriptions:
 * frmlen: frame length
 * keytype: key type
 * tidno: tid number
 * isdata: if it's a data frame
 * ismcast: if it's a multicast frame
 * useminrate: if the frame should transmitted using specified mininum rate
 * isbar: if it is a block ack request
 * ispspoll: if it is a PS-Poll frame
 * calcairtime: requests airtime be calculated when set for tx frame
 * shpreamble: use short preamble
 */
#define SHARE_CTRL_BLK_TX_CTRL_DATA         \
        HAL_KEY_TYPE    keytype;            \
        u_int16_t       frmlen;             \
        u_int16_t       tidno;              \
        u_int16_t       isdata:1;           \
        u_int16_t       ismcast:1;          \
        u_int16_t       use_minrate:1;      \
        u_int16_t       isbar:1;            \
        u_int16_t       ispspoll:1;         \
        u_int16_t       calcairtime:1;      \
        u_int16_t       shortPreamble:1;    \

#define SHARE_CTRL_BLK_TX_CTRL_T            \
    SHARE_CTRL_INFO_BEGIN                   \
    SHARE_CTRL_BLK_TX_CTRL_DATA             \
    SHARE_CTRL_INFO_END

struct _random_name_for_size_calc_ {
    SHARE_CTRL_BLK_TX_CTRL_DATA
};

#define SHARE_CTRL_BLK_TX_CTRL_T_SIZE       \
    sizeof(struct _random_name_for_size_calc_)

typedef struct {
    SHARE_CTRL_BLK_TX_CTRL_T;
    ath_node_t      an;         /* destination to sent to */
    int             if_id;      /* only valid for cab traffic */
    int             qnum;       /* h/w queue number */

    u_int           istxfrag:1; /* if it's a tx fragment */
    u_int           ismgmt:1;   /* if it's a management frame */
    u_int           isqosdata:1; /* if it's a qos data frame */
    u_int           ps:1;       /* if one or more stations are in PS mode */
    u_int           ht:1;       /* if it can be transmitted using HT */
    u_int           iseap:1; /* Is this an EAP packet? */
    u_int           isnulldata:1; /* Is this a QOSNULL packet? */
#ifdef ATH_SUPPORT_UAPSD
    u_int           isuapsd:1;  /* if this frame needs uapsd handling */
#endif
#ifdef ATH_SUPPORT_TxBF
    u_int           isdelayrpt:1;   /* delay report frame*/
#endif
#if UMAC_SUPPORT_WNM
    u_int           isfmss:1;   /* If it's a FMS Stream */
#endif
    u_int           nocabq:1;    /* if no cab feature is enable */
    HAL_PKT_TYPE    atype;      /* Atheros packet type */
    u_int32_t       flags;      /* HAL flags */
    u_int32_t       keyix;      /* key index */
    u_int16_t       txpower;    /* transmit power */
    u_int16_t       seqno;      /* sequence number */

#ifdef USE_LEGACY_HAL
    u_int16_t       hdrlen;     /* header length of this frame */
    int             compression; /* compression scheme */
    u_int8_t        ivlen;      /* iv length for compression */
    u_int8_t        icvlen;     /* icv length for compression */
    u_int8_t        antenna;    /* antenna control */
#endif

    int             min_rate;   /* minimum rate */
    int             mcast_rate; /* multicast rate */
    u_int16_t       nextfraglen; /* next fragment length */

    /* below is set only by ath_dev */
    ath_dev_t       dev;        /* device handle */
#if ATH_TX_COMPACT
    u_int8_t        priv[32];   /* private rate control info */
#else
    u_int8_t        priv[64];   /* private rate control info */
#endif

    OS_DMA_MEM_CONTEXT(dmacontext) /* OS specific DMA context */
#if ATH_SUPPORT_WIFIPOS
    void *wifiposdata;
#endif
#if UMAC_SUPPORT_WNM
    u_int8_t fmsq_id;
#endif
    bool is_eapol;
} ieee80211_tx_control_t;


#define NUM_OFDM_TONES_ACK_FRAME 52
#define RESOLUTION_IN_BITS       10
#define ATH_DESC_CDUMP_SIZE(rxchain) (NUM_OFDM_TONES_ACK_FRAME * 2 * (rxchain) * RESOLUTION_IN_BITS) / 8
/*
 * @brief per frame tx status block
 */
typedef struct {
    int             retries;    /* number of retries to successufully transmit this frame */
    int             flags;      /* status of transmit , 0  means success*/
#define ATH_TX_ERROR        0x01
#define ATH_TX_XRETRY       0x02
#define ATH_TX_FLUSH        0x04
#ifdef  ATH_SUPPORT_TxBF
    int             txbf_status;    /* status of TxBF */
#define	ATH_TxBF_BW_mismatched            0x1
#define	ATH_TXBF_stream_missed            0x2
#define	ATH_TXBF_CV_missed                0x4
#define	ATH_TXBF_Destination_missed       0x8
    u_int32_t       tstamp;         /* tx time stamp*/
#endif
    u_int32_t       rateKbps;

} ieee80211_tx_status_t;

/*
 * @brief txbf capability block
 */
#ifdef ATH_SUPPORT_TxBF
typedef struct {
    u_int8_t channel_estimation_cap;
    u_int8_t csi_max_rows_bfer;
    u_int8_t comp_bfer_antennas;
    u_int8_t noncomp_bfer_antennas;
    u_int8_t csi_bfer_antennas;
    u_int8_t minimal_grouping;
    u_int8_t explicit_comp_bf;
    u_int8_t explicit_noncomp_bf;
    u_int8_t explicit_csi_feedback;
    u_int8_t explicit_comp_steering;
    u_int8_t explicit_noncomp_steering;
    u_int8_t explicit_csi_txbf_capable;
    u_int8_t calibration;
    u_int8_t implicit_txbf_capable;
    u_int8_t tx_ndp_capable;
    u_int8_t rx_ndp_capable;
    u_int8_t tx_staggered_sounding;
    u_int8_t rx_staggered_sounding;
    u_int8_t implicit_rx_capable;
} ieee80211_txbf_caps_t;
#endif

#define ATH_HOST_MAX_ANTENNA 3

#if UMAC_SUPPORT_WNM
#define ATH_MAX_FMS_QUEUES 8
#endif

/*
 * @brief headline block removal
 */
/*
 * HBR_TRIGGER_PER_HIGH is the per threshold to
 * trigger the headline block removal state machine
 * into PROBING state; HBR_TRIGGER_PER_LOW is the
 * per threshold to trigger the state machine to
 * ACTIVE mode from PROBING. Since log(5/25)/log(7/8)=12,
 * which means 12 continuous QoSNull probing frames
 * been sent successfully.
 */
#define HBR_TRIGGER_PER_HIGH    25
#define HBR_TRIGGER_PER_LOW        5


/*
 * @brief per frame rx status block
 */
typedef struct {
    int             numchains;  /*number of rx chains*/
    u_int64_t       tsf;        /* mac tsf */
    int8_t          rssi;       /* RSSI (noise floor ajusted) */
    int8_t          rssictl[ATH_HOST_MAX_ANTENNA];       /* RSSI (noise floor ajusted) */
    int8_t          rssiextn[ATH_HOST_MAX_ANTENNA];       /* RSSI (noise floor ajusted) */
    int8_t          abs_rssi;   /* absolute RSSI */
    u_int8_t        rateieee;   /* data rate received (IEEE rate code) */
    u_int8_t        ratecode;   /* phy rate code */
    u_int8_t        nomoreaggr; /* single frame or last of aggregation */
    int             rateKbps;   /* data rate received (Kbps) */
    int             flags;      /* status of associated wbuf */
#define ATH_RX_FCS_ERROR        0x01
#define ATH_RX_MIC_ERROR        0x02
#define ATH_RX_DECRYPT_ERROR    0x04
#define ATH_RX_RSSI_VALID       0x08
#define ATH_RX_CHAIN_RSSI_VALID 0x10 /* if any of ctl,extn chain rssis are valid */
#define ATH_RX_RSSI_EXTN_VALID  0x20 /* if extn chain rssis are valid */
#define ATH_RX_40MHZ            0x40 /* set if 40Mhz, clear if 20Mhz */
#define ATH_RX_SHORT_GI         0x80 /* set if short GI, clear if full GI */
#define ATH_RX_SM_ENABLE        0x100
#define ATH_RX_KEYMISS          0x200
#define ATH_RX_KEYINVALID       0x400

#ifdef ATH_SUPPORT_TxBF
    u_int8_t    bf_flags;           /*Handle OS depend function*/
#define ATH_BFRX_PKT_MORE_ACK           0x01
#define ATH_BFRX_PKT_MORE_QoSNULL       0x02
#define ATH_BFRX_PKT_MORE_DELAY         0x04
#define ATH_BFRX_DATA_UPLOAD_ACK        0x10
#define ATH_BFRX_DATA_UPLOAD_QosNULL    0x20
#define ATH_BFRX_DATA_UPLOAD_DELAY      0x40

    u_int32_t   rpttstamp;      /* cv report time statmp*/
#endif
#if ATH_VOW_EXT_STATS
    u_int32_t vow_extstats_offset;  /* Lower 16 bits holds the udp checksum offset in the data pkt */
                                    /* Higher 16 bits contains offset in the data pkt at which vow ext stats are embedded */
#endif
    u_int8_t  isaggr;               /* Is this frame part of an aggregate */
    u_int8_t  isapsd;               /* Is this frame an APSD trigger frame */
    int16_t   noisefloor;           /* Noise floor */
    u_int16_t channel;              /* Channel */
    uint8_t   lsig[IEEE80211_LSIG_LEN];
    uint8_t   htsig[IEEE80211_HTSIG_LEN];
    uint8_t   servicebytes[IEEE80211_SB_LEN];
} ieee80211_rx_status_t;

typedef struct {
    int             rssi;       /* RSSI (noise floor ajusted) */
    int             rssictl[ATH_HOST_MAX_ANTENNA];       /* RSSI (noise floor ajusted) */
    int             rssiextn[ATH_HOST_MAX_ANTENNA];       /* RSSI (noise floor ajusted) */
    int             rateieee;   /* data rate xmitted (IEEE rate code) */
    int             rateKbps;   /* data rate xmitted (Kbps) */
    int             ratecode;   /* phy rate code */
    int             flags;      /* validity flags */
#define ATH_TX_CHAIN_RSSI_VALID 0x01 /* if any of ctl,extn chain rssis are valid */
#define ATH_TX_RSSI_EXTN_VALID  0x02 /* if extn chain rssis are valid */
    u_int32_t       airtime;    /* time on air per final tx rate */
} ieee80211_tx_stat_t;

/* VAP configuration (from protocol layer) */
struct ath_vap_config {
    u_int32_t        av_fixed_rateset;
    u_int32_t        av_fixed_retryset;
#if ATH_SUPPORT_AP_WDS_COMBO
    u_int8_t	     av_no_beacon; 		/* vap will not xmit beacon/probe resp. */
#endif
    u_int8_t         av_rc_txrate_fast_drop_en;/* vap enable tx rate fast drop*/
#ifdef ATH_SUPPORT_TxBF
    u_int8_t         av_auto_cv_update;
    u_int8_t         av_cvupdate_per;
#endif
    u_int8_t         av_short_gi;
    u_int8_t         av_ampdu_sub_frames;
    u_int8_t         av_amsdu;
};


struct ath_mib_mac_stats {
    u_int32_t   ackrcv_bad;
    u_int32_t   rts_bad;
    u_int32_t   rts_good;
    u_int32_t   fcs_bad;
    u_int32_t   beacons;
};

struct ath_vap_dev_stats {
    u_int32_t av_tx_xretries;
    u_int32_t av_tx_retries;
    u_int32_t av_tx_mretries;
    u_int32_t av_ack_failures;
    u_int32_t av_retry_count;
    u_int32_t av_aggr_pkt_count;
};

#ifdef ATH_CCX
struct ath_chan_data {
    u_int8_t    clockRate;
    u_int32_t   noiseFloor;
    u_int8_t    ccaThreshold;
};
#endif

/*
* @brief callback function tabled to be registered with lmac.
* lmac will use the functional table to send events/frames to umac and
* also query information from umac.
*/
struct ieee80211_ops {
/**
 * get network interface related settings on the radio.
 * @param     ieee_handle   : handle to umac radio.
 * @return    a bit map of net if settings . the bitmap is
 *           defined below(NETIF_RUNNING, PROMISCUOUT ...etc).
 *           in most cases the os/umac needs to return ATH_NETIF_RUNNING | ATH_NETIF_ALLMULTI.
 *           if promiscuous mode is enabled on the interface then the ATH_NETIF_PROMISCUOUS
 *           needs to  be included in the returned bit map to turn on the promiscuous mode.
 */
    int         (*get_netif_settings)(ieee80211_handle_t);
#define ATH_NETIF_RUNNING       0x01
#define ATH_NETIF_PROMISCUOUS   0x02
#define ATH_NETIF_ALLMULTI      0x04
#define ATH_NETIF_NO_BEACON     0x08
/**
 *  merge all multi cast addresses to generate a filter value (deprecated)
 * @param     ieee_handle_t   : handle to umac radio.
 * @param     mfilt           : multicast filter array to be written by umac.
 * @return    umac should compute the final multicast filter to be used by lmac and write the
 *            value to the mfilr parameter. if the get_netif_settings function retuurns ALLMULTI then
 *            this function will not be called.(currently deprecated)
 */
    void        (*netif_mcast_merge)(ieee80211_handle_t, u_int32_t mfilt[2]);

/**
 *  channel list notification from lmac. to indicate initial channel list, updates to channel list.
 * @param     ieee_handle       : handle to umac radio .
 * @param     cmd               : channel list update type.(see uma_lmac_common.h for details )
 */
    void        (*setup_channel_list)(ieee80211_handle_t, enum ieee80211_clist_cmd cmd,
                                      const u_int8_t *regclassids, u_int nregclass,
                                      int countrycode);

/**
 * Get ieee80211 channel list from Regdb.
 * @param     ieee_handle       : handle to umac radio .
 */
    int         (*get_channel_list_from_regdb)(ieee80211_handle_t);
    void        (*modify_chan_144)(ieee80211_handle_t, int);

    int         (*program_reg_cc)(ieee80211_handle_t, char *, uint16_t);
/**
 * deprecated
 */
    int         (*set_countrycode)(ieee80211_handle_t, char *isoName, u_int16_t cc, enum ieee80211_clist_cmd cmd);
/**
 * Get country code
 */
    int         (*get_countrycode)(ieee80211_handle_t);
/**
 * Set Regdomain code
 */
    int         (*set_regdomaincode)(ieee80211_handle_t, u_int16_t rd);

/**
 * Get Regdomain code
 */
    int         (*get_regdomaincode)(ieee80211_handle_t);
/**
 * update change in transmit power. lmac uses this call back function to notify tx power change.
 * @param     ieee_handle       : handle to umac radio .
 * @param     txpowlimit        : requested transmit power limit in db.
 * @param     txpowlevel        : actual transmit power set in the HW in db.
 */
    void        (*update_txpow)(ieee80211_handle_t, u_int16_t txpowlimit, u_int16_t txpowlevel);
/**
 * get beacon configuration. lmac uses this  function to get beacon configuration .
 * @param     ieee_handle       : handle to umac radio .
 * @param     if_id             : id of the vap whose  beacon configuration is being requested .
 * @param     conf              : pointer to beacon config struct that umac needs to fill in.
 */
    void        (*get_beacon_config)(ieee80211_handle_t, int if_id, ieee80211_beacon_config_t *conf);
 /**
  * allocate a beacon buffer .
  * @param     ieee_handle       : handle to umac radio .
  * @param     if_id             : id of the vap the beacon to be allocated for .
  * @param     bo                : pointer to beacon offset structure to be filled by umac.
  *                                umac needs to fill in the bo_tim field to point to the tim fields of the beacon.
  * @param     txctrl            : pointer to txctrl structure to be filled by umac.
  *                                 in txcontrol structure only the txpower,min_rate and short preamble need to
  *                                 filled in. txpower in db to indicate the indicate the transmit power of the beacon
  *                                 min_rate if set will be the rate used for beacon (normally it should be set to 0),
  *                                 short preamble should be set to 1 if short preamble is enabled.
  * @return wbuf containing initial beacon.
  */
    wbuf_t      (*get_beacon)(ieee80211_handle_t, int if_id, ieee80211_beacon_offset_t *bo, ieee80211_tx_control_t *txctl);
  /**
   * update beacon buffer , called for every beacon transmission  before queueing the beacon frame to HW .
   * @param     ieee_handle       : handle to umac radio .
   * @param     if_id             : id of the vap the beacon to be allocated for .
   * @param     bo                : pointer to beacon offset structure to be filled by umac.
   *                                umac needs to fill in the bo_tim field to point to the tim fields of the beacon.
   * @param     wbuf              : network buffer containing the beacon (allocated from get_beacon API).
   * @param     mcast             : number of multicast frames queued in the multicast queue to be sent out immediately after
                                     beacon. umac needs to set DTIM bit in becon if this param is non zero.
   * @param     nfmsq_mask        : Mask of eight bits corresponding to eight FMS queues indicating available one or more
                                    multicast frames in respective fms queue to be sent out immediately after
   * @return 0,1, or -ve value. reutuen 0 to indicate no change in beacon length. 1 indicate change in beacon lengh.
   *         -ve to suppress beacon transmission.
   */
#if UMAC_SUPPORT_WNM
    int         (*update_beacon)(ieee80211_handle_t, int if_id, ieee80211_beacon_offset_t *bo, wbuf_t wbuf, int mcast, u_int32_t nfmsq_mask, u_int32_t *smart_ant_bcnant);
#else
    int         (*update_beacon)(ieee80211_handle_t, int if_id, ieee80211_beacon_offset_t *bo, wbuf_t wbuf, int mcast, u_int32_t *smart_ant_bcnant);
#endif

  /**
   * beacon miss notification from lmac(HW).
   * the detection is  based on the beacon miss configuration returned via get_beacon_config API above.
   * @param     ieee_handle       : handle to umac radio .
   *      this is only useful when one station VAP and the chip is operating in STA mode.
   */
    void        (*notify_beacon_miss)(ieee80211_handle_t);

    /**
     * beacon RSSI threshold notification from lmac(HW).
     * @param     ieee_handle       : handle to umac radio .
     *      this is only useful when one station VAP and the chip is operating in STA mode.
     */
      void        (*notify_beacon_rssi)(ieee80211_handle_t);

   /**
    * hardware based TIM notification. STA specific.
    * @param     ieee_handle       : handle to umac radio .
    *      this is only useful when one station VAP and the chip is operating in STA mode.
    */
    void        (*proc_tim)(ieee80211_handle_t);

    /**
     * hardware based DTIM notification. STA specific.
     * @param     ieee_handle       : handle to umac radio .
     *      this is only useful when one station VAP and the chip is operating in STA mode.
     */
     void        (*proc_dtim)(ieee80211_handle_t);

    /**
     * send bar request to UMAC.
     * lmac requests the UMAC to send a BAR (Block Ack Request) for a given tid and sequence number.
     * @param     node       : node identifying the destination to send BAR to.
     * @param     tidno      : tid no to use in the bar.
     * @param     node       : seqno to use in the bar frame.
     * @return 0 for success and nonzero for failure.
     */
    int         (*send_bar)(ieee80211_node_t, u_int8_t tidno, u_int16_t seqno);

    /**
      * tx queue dpeth notification to UMAC.
      * @param     ieee_handle       : handle to umac radio .
      * @param     queue_depth       : combined depth of all the queues.
      * lmac sends the notification at the end of ta tasklet. registration of this call back is optional
      */
    void        (*notify_txq_status)(ieee80211_handle_t, u_int16_t queue_depth);
#if ATH_SUPPORT_WIFIPOS
    void        (*update_wifipos_stats)(ieee80211_wifiposdesc_t *);
    int        (*isthere_wakeup_request)(ieee80211_handle_t);
    void        (*update_ka_done)(u_int8_t *, u_int8_t);
#endif

    /**
     * callback to indicate transmission (succes/failure) of a single TX frame .
     * @param     wbuf              : wbuf containing the tx frame.
     * @param     tx_status         : pointer to status structure containing transmission status.
     *                                look at the ieee80211_tx_status_t strucuture for more details.
     * @param     all_frag          : complete all fragment wbuf which are linked together
     */
    void        (*tx_complete)(wbuf_t wbuf, ieee80211_tx_status_t *, bool all_frag);
#if ATH_TX_COMPACT
#ifdef ATH_SUPPORT_QUICK_KICKOUT
    void         (*tx_node_kick_event)( struct ieee80211_node * ni, u_int8_t* consretry,wbuf_t wbuf);
#endif
    void         (*tx_complete_compact)( struct ieee80211_node *ni, wbuf_t wbuf);
    void        (*tx_free_node)( struct ieee80211_node * ni,int txok);

#ifdef ATH_SUPPORT_TxBF
    void        (*tx_handle_txbf_complete)( struct ieee80211_node * ni,  u_int8_t txbf_status,  u_int32_t tstamp, u_int32_t txok);
#endif
    /*
    * callback to check for any pn corruption and update the tsc's accordingly.
    *
    */
    void        (*check_and_update_pn) ( wbuf_t wbuf);
    void        (*tx_update_stats)(ieee80211_handle_t, wbuf_t wbuf, ieee80211_tx_status_t *);
#endif
    /**
     * callback to update transmission stats for a node  .
     * @param     node              : handle to a node.
     * @param     tx_stats          : pointer to stats structure containing transmission stats.
     *                                look at the ieee80211_tx_stat_t strucuture for more details.
     */
    void        (*tx_status)(ieee80211_node_t, ieee80211_tx_stat_t *);
    /**
     * optional. currently not called from lmac directly. may not be needed for non atheros umac.
     * lmac calls ath_rx_indicate which is implmented by each os dep layer
     * and all the osdep layers using atheros umac need to call this call back.
     * atheros umac implements and registers this call back.
     * @param     ieee_handle       : handle to umac radio .
     * @param     wbuf              : wbuf containing the rx frame.
     * @param     status            : pointer to rx status structure.look at the structure definition
     *                                for more info.
     */
    int         (*rx_indicate)(struct wlan_objmgr_pdev *pdev, wbuf_t wbuf, ieee80211_rx_status_t *status, u_int16_t keyix);
    /**
     * call back to rceive an rx frame from the AMPDU reorder buffer.
     * @param     node              : handle to a node.
     * @param     wbuf              : wbuf containing the rx frame.
     * @param     status            : pointer to rx status structure.look at the structure definition
     *                                for more info.
     * umac should start processing the frame.
     */
    int         (*rx_subframe)(ieee80211_node_t, wbuf_t wbuf, ieee80211_rx_status_t *status);
    /**
     * call back to query umac for current mac mode (HT40 vs HT20).
     * this is used if umac implments dynamic HT40/20 transition based on interference
     * on extension channel. if no dynamic transition logic is impemented then umac should just
     * return the static operating mode of the channel which can be HT40 or HT20 .
     * @param     ieee_handle       : handle to umac radio .
     * @return macmode.
     */
    HAL_HT_MACMODE (*cwm_macmode)(ieee80211_handle_t);
#ifdef ATH_SUPPORT_DFS
    /* DFS related ops */
    /**
     * call back to enable radar after reset.
     * @param     ieee       : handle to umac radio .
     * @param     cac        : cac on/off.
     */
     void        (*ath_net80211_enable_radar_dfs)(ieee80211_handle_t ieee);
#endif /* ATH_SUPPORT_DFS */
    void        (*ath_net80211_set_vap_state)(ieee80211_handle_t ieee,u_int8_t if_id, u_int8_t state);
    /* unused and deprecated */
    void        (*ath_net80211_change_channel)(ieee80211_handle_t ieee, struct ieee80211_ath_channel *chan);
    /**
     * call back requesting umac to change the channel to static HT20.
     * @param     ieee       : handle to umac radio.
     */
    void        (*ath_net80211_switch_mode_static20)(ieee80211_handle_t ieee);
    /**
     * call back requesting umac to change the channel to dynamic 2040.
     * @param     ieee       : handle to umac radio .
     */
    void        (*ath_net80211_switch_mode_dynamic2040)(ieee80211_handle_t ieee);
    /* Rate control related ops */
    /**
     * call back to umac to indicat the rate table for each wireless  mode.
     * @param     ieee       : handle to umac radio .
     * @param     mode       : wireless mode for which the rate table being indicated.
     * @param     type       :  rate type.
     * @param     rt         :  pointer to rate table.
     */
    void        (*setup_rate)(ieee80211_handle_t, WIRELESS_MODE mode, RATE_TYPE type, const HAL_RATE_TABLE *rt);
    /**
     * optional call back to umac to rate being used for every frame being transmitted.
     * note ths is called for every transmit . the umac may choose not to set up this function
     * for performance reasons.
     *  @param  node_t : hadle to umac node.
     *  @param  txrate : rate index (index into the current rate table).
     */
    void        (*update_txrate)(ieee80211_node_t, int txrate);
    /**
     * called by lmac rate control module
     * after the rate table has been setup for the given vap. called when ath_ops->up call invoked
     * by umac part of vap bringup. typically  umac will call the ath_ops->ath_rate_newassoc function on
     * the bss node to initialize the rate control state of the bss node.
     *  @param  node_t  : hadle to umac node.
     *  @param  if_data : handle to the vap.
     */
    void        (*rate_newstate)(ieee80211_handle_t ieee, ieee80211_if_t if_data);
    /**
     * callback requesting umac to update the rate control.
     * umac will collect the node state and call ath_ops->ath_rate_newass to update the rate control.
     * this is usually called in response, when umac calls into lmac to change the SM power state of a node,
     *  @param     ieee       : handle to umac radio .
     *  @param  node_t        : hadle to umac node.
     *  @param  isnew         : unused always 1.
     */
    void        (*rate_node_update)(ieee80211_handle_t ieee, ieee80211_node_t, int isnew);

    /**
     * callback requesting umac to drain any buffers being queued for AMSDU.
     *  @param     ieee       : handle to umac radio .
     */
    void        (*drain_amsdu)(ieee80211_handle_t);
    /**
     * callback to fetch the atheros specific extra delemiter war capability
     * of the node.
     * @param  ni        : hadle to umac node.
     * @return the atheros specific extra delemiter war capability from atheros IE extensions.
     */
    int         (*node_get_extradelimwar)(ieee80211_node_t ni);
#if WLAN_SPECTRAL_ENABLE
    /**
     * callback to umac with a spectral scan specific message.
     *  @param     ieee       : handle to umac radio .
     *  @param     data       : pointer to message buffer .
     *  @param     len        : length of the message buffer .
     */
    void        (*spectral_indicate)(ieee80211_handle_t ieee, void* data, u_int32_t len);

    /**
     * callback to umac with a EACS spectral scan update.
     *  @param     ieee                 : handle to umac radio .
     *  @param     nfc_ctl_rssi         : control channel rssi.
     *  @param     nfc_ext_rssi         : extension channel rssi.
     *  @param     ctrl_nf              : ctrl channel noise floor.
     *  @param     ext_nf               : extension channel nf.
     */
    void        (*spectral_eacs_update)(ieee80211_handle_t ieee, int8_t nfc_ctl_rssi, int8_t nfc_ext_rssi,
                                                                       int8_t ctrl_nf, int8_t ext_nf);

    /**
     * callback to initialize spectral parameters on channel loading.
     *  @param     ieee                 : handle to umac radio .
     *  @param     curchan              : ctrl channel freq (in mhz).
     *  @param     extchan              : extension channela freq (in mhz).
     */
    void        (*spectral_init_chan_loading)(ieee80211_handle_t ieee, int curchan, int extchan);

    /**
     * callback to retrieve spectral control duty cycles.
     *  @param     ieee       : handle to umac radio .
     */
    int        (*spectral_get_freq_loading)(ieee80211_handle_t ieee);

#endif
    /**
     * callback to umac for handling CW interference
     *  @param     ieee       : handle to umac radio .
     */
    void        (*cw_interference_handler)(ieee80211_handle_t ieee);
#ifdef ATH_SUPPORT_UAPSD
    /**
     * callback from ISR for the UMAC to check if this is an UAPSD trigger.
     * the implementation needs to find the node (station structure) either from
     * mac address (or) keyindex and check if the frame is a trigger and if so
     * then call back the process_uapsd_trigger function from ath_ops to send response frames.
     * @param     ieee_handle       : handle to umac radio .
     * @param     qwh               : pointer to 802.11 header of the frame.
     * @param     keyix             : key index of the key entry containing the mac address that  matches with TA of the frame.
     * this callback is used for nos osprey chipsets (owl,merlin ...etc).
     */
    bool        (*check_uapsdtrigger)(struct wlan_objmgr_pdev *pdev, struct ieee80211_qosframe *qwh, u_int16_t keyix, bool isr_context);

    /**
     * callback to indicate that the frame with EOSP bit is transmitted..
     * this allows the UMAC to clear state in the station indicationg that the service period
     * has ended and it can accept more triggers.
     * @param     node              : handle to a node.
     * @param     wbuf              : wbuf containing the frame with EOSP bit set.
     * @param     txok              : 1 if it the tranmission was succesful,0 otherwise.
     */
    void        (*uapsd_eospindicate)(struct wlan_objmgr_peer *peer, wbuf_t wbuf, int txok, int force_eosp);

    /**
     * callback to allocate a wbuf for lmac to send qos null frames internally when required.
     *  lmac calls this call back function to pre allocate a bunch of wbufs.
     * @param     ieee_handle       : handle to umac radio .
     * @return    wbuf with enough memory to hold a qosnull frame.
     */
    wbuf_t      (*uapsd_allocqosnullframe)(struct wlan_objmgr_pdev *pdev);

    /**
     * callback to create a qosnull frame with given wbuf and attached node.
     * @param     node              : handle to a node.
     * @param     wbuf              : wbuf for creating qosnull.
     * @param     ac                : access class to use for creating qosnull.
     * @return    wbuf containing the qosnull.on most systems it is same as the one passed in.
     */
    wbuf_t      (*uapsd_getqosnullframe)(struct wlan_objmgr_peer *peer, wbuf_t wbuf, int ac);

    /**
     * callback to return a qosnull frame with given wbuf and attached node.
     * @param     node              : handle to a node.
     * @param     wbuf              : wbuf containing qosnull that is previous gotten.
     */
    void        (*uapsd_retqosnullframe)(struct wlan_objmgr_peer *peer, wbuf_t wbuf);

    /**
     * callback from ISR for the UMAC to check if this is an UAPSD trigger.
     * the implementation needs to find the node (station structure) either from
     * mac address (or) keyindex and check if the frame is a trigger and if so
     * then call back the process_uapsd_trigger function from ath_ops to send response frames.
     * @param     ieee_handle       : handle to umac radio .
     * @param     qwh               : pointer to 802.11 header of the frame.
     * @param     keyix             : key index of the key entry containing the mac address that  matches with TA of the frame.
     * @param     is_trig           : 1 if this is a trigger frame (QOS data frame).
     * this callback is used for osprey chipset .
     */
    void        (*uapsd_deliverdata)(ieee80211_handle_t ieee, struct ieee80211_qosframe *qwh, u_int16_t keyix, u_int8_t is_trig, bool isr_context);

    /**
     * callback to pauses/unpauses uapsd operation used by p2p protocol when the GO enters absent period.
     * @param     node              : handle to a node.
     * @param     pause             : if pause is true then perform pause operation. If pause is false then
     * perform unpause operation.
     */
    void        (*uapsd_pause_control)(struct wlan_objmgr_peer *peer, bool pause);
#endif
    u_int16_t   (*get_htmaxrate)(ieee80211_node_t);
#if ATH_SUPPORT_IQUE
    /**
     * call back to set the trigger to headline block removal (HBR) module.
     * Based on the signal from the rate control module, find out the
     * state machine associated with the address and then setup the
     * tri-state trigger based upon the signal and current state
     * @param	node	: the assocaited node of which the state machine to be triggered.
     * @param	state	: the input signal to the HBR state machine. The value is
     * one of the three signals: IEEE80211_HBR_EVENT_BACK, IEEE80211_HBR_EVENT_FORWARD,
     * IEEE80211_HBR_EVENT_STALE. If current state of the HBR state machine is ACTIVE,
     * and the input signal is IEEE80211_HBR_EVENT_BACK, then the state is transacted
     * to BLOCKING, and the egressing packets in the queue for this node are dropped.
     * If the current state of HBR state machine is BLOCKING, then the state will be
     * transited to PROBING unless the input signal is IEEE80211_HBR_EVENT_FORWARD;
     * If the current state is PROBING, prbing frames (QoS Null frames with specific tags)
     * are sent out, and the state remains in PROBING until the input signal is
     * IEEE80211_HBR_EVENT_FORWARD
     */
    void        (*hbr_settrigger)(ieee80211_node_t node, int state);
    u_int8_t    (*get_hbr_block_state)(ieee80211_node_t node);
#endif
#if ATH_SUPPORT_VOWEXT
    u_int16_t   (*get_aid)(ieee80211_node_t);
#endif
#ifdef ATH_SUPPORT_LINUX_STA
    void	(*ath_net80211_suspend)(ieee80211_handle_t ieee);
    void	(*ath_net80211_resume)(ieee80211_handle_t ieee);
#endif
#ifdef ATH_BT_COEX
    /**
     * call back to enable 3-wire plus ps enable/disable.
     * this is used for supporting BT ACL profile. Before BT duty cycle starts, STA sends a NULL data frame
     * with PS bit set to notify AP it is in asleep. Meanwhile the local hardware is awake. Then BT can transmit
     * without any WLAN interference. STA sends a NULL data frame without setting PS bit to resume the WLAN transmission
     * with AP.
     * @param     ieee_handle       : handle to umac radio .
     * @param     mode              : poer save mode .
     */
    void        (*bt_coex_ps_enable)(ieee80211_handle_t, u_int32_t mode);
    /**
     * call back to enable 3-wire plus PS poll.
     * this is used for supporting BT SCO/eSCO profile. STA sends a NULL data frame with PS bit set to notify AP
     * it is in asleep at beginning. When BT SCO/eSCO finish its scheduled transmission, STA sends a PS-Poll frame to
     * ask for a frame until BT transmits again. STA sends a NULL data frame without setting PS bit to end this scheme.
     * @param     ieee_handle       : handle to umac radio .
     * @param     lastpoll          : timestamp of the last poll .
     */
    void        (*bt_coex_ps_poll)(ieee80211_handle_t, u_int32_t lastpoll);
#endif
#if ATH_SUPPORT_CFEND
    /**
     * call back to allocate a cfend frame.
     * this allocates a single cfend frame. The allocated frame is cached on
     * sc->sc_cfendbuf list. There will be only one such buffer allocated at
     * any given time, and the buffer is allocated once at init time.
     * @param     ieee_handle       : handle to umac radio .
     * @return wbuf with formatted ( incomplete, filled at tx time ) frame
     */
    wbuf_t      (*cfend_alloc)( ieee80211_handle_t ieee);
#endif
#ifndef REMOVE_PKTLOG_PROTO
    u_int8_t    *(*parse_frm)(ieee80211_handle_t ieee, wbuf_t, ieee80211_node_t,
                              void *frm, u_int16_t keyix);
#endif

    void        (*get_bssid)(ieee80211_handle_t ieee,
                    struct ieee80211_frame_min *shdr, u_int8_t *bssid);
#ifdef ATH_TX99_DIAG
    struct ieee80211_ath_channel *  (*ath_net80211_find_channel)(struct ath_softc *sc, int ieee, u_int8_t des_cfreq2, enum ieee80211_phymode mode);
#endif
    void        (*set_stbc_config)(ieee80211_handle_t, u_int8_t stbc, u_int8_t istx);
    void        (*set_ldpc_config)(ieee80211_handle_t, u_int8_t ldpc);
#if UNIFIED_SMARTANTENNA
    int         (*smart_ant_update_txfeedback) (struct ieee80211_node *ni, void *tx_feedback);
    int         (*smart_ant_update_rxfeedback) (ieee80211_handle_t ieee, wbuf_t wbuf, void *rx_feedback);
    int         (*smart_ant_setparam) (ieee80211_handle_t, char *params);
    int         (*smart_ant_getparam) (ieee80211_handle_t, char *params);
#endif
    u_int32_t     (*get_total_per)(ieee80211_handle_t);
#ifdef ATH_SWRETRY
    u_int16_t   (*get_pwrsaveq_len)(ieee80211_node_t node);
    void	(*set_tim)(ieee80211_node_t node, u_int8_t setflag);
#endif
#if UMAC_SUPPORT_WNM
    wbuf_t      (*get_timbcast)(ieee80211_handle_t, int if_id, int, ieee80211_tx_control_t *);
    int         (*update_timbcast)(ieee80211_handle_t, int if_id, wbuf_t);
    int         (*timbcast_highrate)(ieee80211_handle_t, int if_id);
    int         (*timbcast_lowrate)(ieee80211_handle_t, int if_id);
    int         (*timbcast_cansend)(ieee80211_handle_t, int if_id);
    int         (*timbcast_enabled)(ieee80211_handle_t, int if_id);
    int         (*wnm_fms_enabled)(ieee80211_handle_t, int if_id);
#endif
    /**
     * called by lmac to get the umac node flags (ni->ni_flags)
     *  @param  if_t    : handle to the vap.
     *  @param  node_t  : hadle to umac node.
     */
    u_int32_t   (*get_node_flags)(ieee80211_handle_t, int if_id, ieee80211_node_t);
#if ATH_SUPPORT_FLOWMAC_MODULE
    void          (*notify_flowmac_state)(ieee80211_handle_t, int);
#endif
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
    void         (*buffull_handler)(ieee80211_handle_t ieee);
    void         (*drop_query_from_sta)(ieee80211_handle_t ieee, u_int32_t enable);
    u_int32_t    (*get_drop_query_from_sta)(ieee80211_handle_t ieee);
    void         (*block_flooding_report)(ieee80211_handle_t ieee, u_int32_t enable);
    u_int32_t    (*get_block_flooding_report)(ieee80211_handle_t ieee);
    void         (*ald_update_phy_error_rate)(struct ath_linkdiag *ald, u_int32_t new_phyerr);
#endif
    void         (*set_tid_override_queue_mapping)(ieee80211_handle_t ieee, u_int8_t val);
#if ATH_SUPPORT_DSCP_OVERRIDE
    void         (*set_dscp_override)(ieee80211_handle_t ieee, u_int32_t val);
    u_int32_t    (*get_dscp_override)(ieee80211_handle_t ieee);
    void         (*set_dscp_tid_map)(ieee80211_handle_t ieee, u_int8_t tos, u_int8_t tid);
    u_int8_t     (*get_dscp_tid_map)(ieee80211_handle_t ieee, u_int8_t tos);
    void         (*reset_dscp_tid_map)(ieee80211_handle_t ieee, u_int8_t tid);
    void         (*set_igmp_dscp_override)(ieee80211_handle_t ieee, u_int32_t enable);
    u_int32_t    (*get_igmp_dscp_override)(ieee80211_handle_t ieee);
    void         (*set_igmp_dscp_tid_map)(ieee80211_handle_t ieee, u_int8_t tid);
    u_int32_t    (*get_igmp_dscp_tid_map)(ieee80211_handle_t ieee);
    void         (*set_hmmc_dscp_override)(ieee80211_handle_t ieee, u_int32_t enable);
    u_int32_t    (*get_hmmc_dscp_override)(ieee80211_handle_t ieee);
    void         (*set_hmmc_dscp_tid_map)(ieee80211_handle_t ieee, u_int8_t tid);
    u_int32_t    (*get_hmmc_dscp_tid_map)(ieee80211_handle_t ieee);
#endif
    int              (*dfs_proc_phyerr)(ieee80211_handle_t, void *buf, u_int16_t datalen, u_int8_t rssi,
                                u_int8_t ext_rssi, u_int32_t rs_tstamp, u_int64_t fulltsf);
#if ATH_SUPPORT_TIDSTUCK_WAR
    void        (*rxtid_delba)(ieee80211_node_t node, u_int8_t tid);
#endif
    int         (*wds_is_enabled)(ieee80211_handle_t);
    u_int8_t*   (*get_mac_addr)(ieee80211_node_t node);
  /*
   * called by lmac to get the umac mode for a specific vap.
   * @param handle_t  : handle to umac radio
   * @param node_t    : handle to umac node
   */
  WIRELESS_MODE  (*get_vap_bss_mode) (ieee80211_handle_t ieee, ieee80211_node_t node);
    int         (*set_acs_param)(ieee80211_handle_t, int param, int flag);
    int         (*get_acs_param)(ieee80211_handle_t,int param);
#if QCA_SUPPORT_SON
    void        (*bsteering_rssi_update)(ieee80211_handle_t ,u_int8_t *macaddres, u_int8_t status ,int8_t rssi, u_int8_t subtype);
    void        (*bsteering_rate_update)(ieee80211_handle_t ,u_int8_t *macaddres, u_int8_t status ,u_int32_t rateKbps);
#endif
#if QCA_AIRTIME_FAIRNESS
    QDF_STATUS   (*atf_adjust_tokens)(ieee80211_node_t node, int8_t ac, uint32_t actual_duration, uint32_t est_duration);
    QDF_STATUS   (*atf_account_tokens)(ieee80211_node_t node, int8_t ac, uint32_t pkt_duration);
    void         (*atf_scheduling)(ieee80211_handle_t ieee, u_int32_t enable);
    u_int32_t    (*get_atf_scheduling)(ieee80211_handle_t ieee);
    uint8_t      (*atf_buf_distribute)(struct wlan_objmgr_pdev *pdev, ieee80211_node_t node, u_int8_t tid);
    QDF_STATUS   (*atf_subgroup_free_buf)(ieee80211_node_t node, uint16_t buf_acc_size, void *bf_atf_sg);
    QDF_STATUS   (*atf_update_subgroup_tidstate)(ieee80211_node_t node, uint8_t atf_nodepaused);
    uint8_t      (*atf_get_subgroup_airtime)(ieee80211_node_t node, uint8_t ac);
    void         (*atf_obss_scale)(ieee80211_handle_t ieee, u_int32_t scale);
#endif
    void          (*proc_chan_util)(ieee80211_handle_t ieee, u_int32_t obss_rx_busy_per, u_int32_t channel_busy_per);
    /**
     * Functions to allow the user to set min rssi value for frames,
     * below which the STA sending the frames will be de-authed.
     * This feature can be enabled/disabled as well using the below functions
     */
    int          (*set_enable_min_rssi)(ieee80211_handle_t ieee, u_int8_t val);
    u_int8_t     (*get_enable_min_rssi)(ieee80211_handle_t ieee);
    int          (*set_min_rssi)(ieee80211_handle_t ieee, int rssi);
    int          (*get_min_rssi)(ieee80211_handle_t ieee);
    int          (*set_max_sta)(ieee80211_handle_t ieee, int value);
    int          (*get_max_sta)(ieee80211_handle_t ieee);
    int          (*tx_mgmt_complete)(struct ieee80211_node *ni, wbuf_t wbuf, int txok);
};

/**
 * @}
 */

/*
 * Device revision information.
 */
typedef enum {
    ATH_MAC_VERSION,        /* MAC version id */
    ATH_MAC_REV,            /* MAC revision */
    ATH_PHY_REV,            /* PHY revision */
    ATH_ANALOG5GHZ_REV,     /* 5GHz radio revision */
    ATH_ANALOG2GHZ_REV,     /* 2GHz radio revision */
    ATH_WIRELESSMODE,       /* Wireless mode select */
} ATH_DEVICE_INFO;


#if ATH_SUPPORT_VOWEXT
/**
 * @brief List of VOW capabilities that can be set or unset
 *        dynamically
 * @see VOW feature document for details on each feature
 *
 */
#define	ATH_CAP_VOWEXT_FAIR_QUEUING     0x1
#define	ATH_CAP_VOWEXT_AGGR_SIZING      0x4
#define	ATH_CAP_VOWEXT_BUFF_REORDER     0x8
#define ATH_CAP_VOWEXT_RATE_CTRL_N_AGGR   0x10 /* for RCA */
#define ATH_CAP_VOWEXT_MASK (ATH_CAP_VOWEXT_FAIR_QUEUING |\
                            ATH_CAP_VOWEXT_AGGR_SIZING|\
                            ATH_CAP_VOWEXT_BUFF_REORDER|\
			          ATH_CAP_VOWEXT_RATE_CTRL_N_AGGR)
#endif

/**
 * @brief List of DCS capabilities that can be set or unset
 *        dynamically
 * @see UMAC auto channel selection document for details on each feature
 *
 */
#define ATH_CAP_DCS_CWIM     0x1
#define ATH_CAP_DCS_WLANIM   0x2
#define ATH_CAP_DCS_MASK  (ATH_CAP_DCS_CWIM | ATH_CAP_DCS_WLANIM)


/**
 * @brief Capabilities of Atheros Devices
 */
typedef enum {
    ATH_CAP_FF = 0,     /* obsolete */
    ATH_CAP_BURST,      /* obsolete */
    ATH_CAP_TURBO,      /* obsolete */
    ATH_CAP_XR,         /* obsolete */
    ATH_CAP_TXPOWER,
    ATH_CAP_DIVERSITY,
    ATH_CAP_BSSIDMASK,      /* support for multiple bssid */
    ATH_CAP_TKIP_SPLITMIC,  /* HW needs split (older chipsets before merlin,osprey) which requires 4 entries in key cache */
    ATH_CAP_MCAST_KEYSEARCH, /* HW supports multi cast key search (merlin,osprey  and beyond)  */
    ATH_CAP_TKIP_WMEMIC,    /* HW supports TKIP MIC when WME is enabled (merlin,osprey and beyond support this) */
    ATH_CAP_WMM,            /* HW supports WME (merlin,osprey and beyond support this) */
    ATH_CAP_HT,             /* HW supports 11n (merlin,osprey and beyond support this) */
    ATH_CAP_HT20_SGI,       /* HW supports Short GI in 11n ht20(merlin,osprey and beyond support this) */
    ATH_CAP_RX_STBC,        /* HW supports RX stbc in 11n (merlin,osprey and beyond support this) */
    ATH_CAP_TX_STBC,        /* HW supports TX stbc in 11n (merlin,osprey and beyond support this) */
    ATH_CAP_LDPC,
    ATH_CAP_4ADDR_AGGR,     /* HW supports 4 Address aggregation (osprey and beyond support this) */
    ATH_CAP_HW_BEACON_PROC_SUPPORT,/* HW supports beacon processing (osprey and beyond support this) */
    ATH_CAP_MBSSID_AGGR_SUPPORT,/* HW supports enhanced power  management (merlin,osprey and beyond support this) */
#ifdef ATH_SWRETRY
    ATH_CAP_SWRETRY_SUPPORT,/* lmac supports SW retry */
#endif
#ifdef ATH_SUPPORT_UAPSD
    ATH_CAP_UAPSD,          /* lmac supports UAPSD */
#endif
    ATH_CAP_NUMTXCHAINS,
    ATH_CAP_NUMRXCHAINS,
    ATH_CAP_RXCHAINMASK,
    ATH_CAP_TXCHAINMASK,
    ATH_CAP_DYNAMIC_SMPS,
    ATH_CAP_WPS_BUTTON,
    ATH_CAP_EDMA_SUPPORT,
    ATH_CAP_WEP_TKIP_AGGR,
    ATH_CAP_DS,
    ATH_CAP_TS,
    ATH_CAP_DEV_TYPE,
#ifdef  ATH_SUPPORT_TxBF
    ATH_CAP_TXBF,
#endif
    ATH_CAP_EXTRADELIMWAR, /* for osprey 1.0 and older 11n chips */
    ATH_CAP_LDPCWAR, /* for osprey <= 2.2 and Peacock */
    ATH_CAP_ZERO_MPDU_DENSITY,
#if ATH_SUPPORT_DYN_TX_CHAINMASK
    ATH_CAP_DYN_TXCHAINMASK,
#endif /* ATH_CAP_DYN_TXCHAINMASK */
#if ATH_SUPPORT_WAPI
    ATH_CAP_WAPI_MAXTXCHAINS,
    ATH_CAP_WAPI_MAXRXCHAINS,
#endif
#if ATH_SUPPORT_WRAP
    ATH_CAP_PROXYSTARXWAR,
#endif
    ATH_CAP_PN_CHECK_WAR,
} ATH_CAPABILITY_TYPE;

typedef enum {
    ATH_RX_NON_CONSUMED = 0,
    ATH_RX_CONSUMED
} ATH_RX_TYPE;

// Parameters for get/set _txqproperty()
#define     TXQ_PROP_PRIORITY           0x00   /* not used */
#define     TXQ_PROP_AIFS               0x01   /* aifs */
#define     TXQ_PROP_CWMIN              0x02   /* cwmin */
#define     TXQ_PROP_CWMAX              0x03   /* cwmax */
#define     TXQ_PROP_SHORT_RETRY_LIMIT  0x04   /* short retry limit */
#define     TXQ_PROP_LONG_RETRY_LIMIT   0x05   /* long retry limit - not used */
#define     TXQ_PROP_CBR_PERIOD         0x06   /*  */
#define     TXQ_PROP_CBR_OVFLO_LIMIT    0x07   /*  */
#define     TXQ_PROP_BURST_TIME         0x08   /*  */
#define     TXQ_PROP_READY_TIME         0x09   /* specified in msecs */
#define     TXQ_PROP_COMP_BUF           0x0a   /* compression buffer phys addr */

struct ATH_HW_REVISION
{
    u_int32_t   macversion;     /* MAC version id */
    u_int16_t   macrev;         /* MAC revision */
    u_int16_t   phyrev;         /* PHY revision */
    u_int16_t   analog5GhzRev;  /* 5GHz radio revision */
    u_int16_t   analog2GhzRev;  /* 2GHz radio revision */
};


#define IEEE80211_ADDR_LEN    6

/* Reset flag */
#define RESET_RETRY_TXQ     0x00000001

typedef enum {
    ATH_MFP_QOSDATA = 0,
    ATH_MFP_PASSTHRU,
    ATH_MFP_HW_CRYPTO
} ATH_MFP_OPT_t;

typedef void (*ath_notify_tx_beacon_function)(void *arg, int beacon_id, u_int32_t tx_status);
typedef void (*ath_vap_info_notify_func)(void *arg, ath_vap_infotype infotype,
                                         u_int32_t param1, u_int32_t param2);


#if QCA_AIRTIME_FAIRNESS
struct atf_stats;
struct atf_peerstate;
#endif

/*
 * @brief callback function tabled to be registered with umac.
 * umac will use the functional table to send events/frames to lmac and
 * also query information from lmac.
 */
struct ath_ops {
    /* To query hardware version and revision */
    /**
     * callback to query hardware version and revision
     * @param     dev : handle to LMAC device
     * @param     type : the device info to request. Possible types are defined in ATH_DEVICE_INFO
     * @return     the value of the requested device info
     */
    u_int32_t   (*get_device_info)(ath_dev_t dev, u_int32_t type);

    /* To query hardware capabilities */
    /**
     * callback to query hardware capabilities
     * @param dev : handle to LMAC device
     * @param type : the device capability info to request. Possible types are defined in ATH_CAPABILITY_TYPE
     * @return 0 means not supported; for return values larger than 0, it means supported and the value might
     * represent the detail of supported capability. E.g. for ATH_CAP_NUMTXCHAINS, the return value represents
     *  the number of TX chainmasks supported
     */
    int         (*have_capability)(ath_dev_t, ATH_CAPABILITY_TYPE type);

    /**
     * callback to query hardware cipher support
     * @param dev : handle to LMAC device
     * @param cipher : the type of cipher to request
     * @return 1 means supported; 0 means not supported
     */
    int         (*has_cipher)(ath_dev_t, HAL_CIPHER cipher);

    /* To initialize/de-initialize */
    /**
     * callback to initialize device on a specific channel.
     * @param dev : handle to LMAC device
     * @param initial_chan : the initial channel the device stays on
     * @return 0 on success; -EIO means the initializaiton failed
     */
    int         (*open)(ath_dev_t, HAL_CHANNEL *initial_chan);

    /**
     * callback to stop LMAC device
     * @param dev : the LMAC device to stop
     * @return 0 on success; -EIO means it failed to stop the device
     */
    int         (*stop)(ath_dev_t);

    /* To attach/detach virtual interface */
#define ATH_IF_ID_ANY   0xff
#if WAR_DELETE_VAP
    int         (*add_interface)(ath_dev_t, int if_id, ieee80211_if_t if_data, HAL_OPMODE opmode, HAL_OPMODE iv_opmode, int nostabeacon, void **athvap);
    int         (*remove_interface)(ath_dev_t, int if_id, void *athvap);
#else
    /**
     * callback to add virtual interface (virtual AP)
     * @param dev         : the handle to LMAC device
     * @param if_id       : the ID for the new interface (id is generated by the caller ).
     * @param if_data     : interface(vap) handle to instance from 802.11 protocal layer. opaque to lmac.
     * @param opmode      : the mode this device runs. Possible values are define in HAL_OPMODE
     * @param iv_opmode   : the mode this VAP runs. Possible values are define in HAL_OPMODE
     * @param nostabeacon : 1 means disable updating HW with beacon info from station vap,
     *                      0 means enable updaing HW with beacon info from station vap.
     * @return 0 on success; non-zero value means fail to add a virtual interface
     */
    int         (*add_interface)(ath_dev_t, int if_id, ieee80211_if_t if_data, HAL_OPMODE opmode, HAL_OPMODE iv_opmode, int nostabeacon);

    /**
     * callback to delete virtual interface
     * @param dev   : the handle to LMAC device
     * @param if_id : the interface ID to be deleted.
     * @return 0 on success; non-zero value means fail to delete the virtual interface
     */
    int         (*remove_interface)(ath_dev_t, int if_id);
#endif

    /**
     * callback to configure virtual interface
     * @param dev       : the handle to LMAC device
     * @param if_id     : the interface ID to be configured
     * @param if_config : the VAP configuration for the interface
     * @return 0 on success; non-zero value means fail to configure the interface
     */
    int         (*config_interface)(ath_dev_t, int if_id, struct ath_vap_config *if_config);

    /* Notification of event/state of virtual interface */
    /**
     * callback to deactivate a virtual interface
     * @param dev   : handle to LMAC device
     * @param if_id : the interface ID to be deactivated
     * @param flags : defined below. For this callback, only flag ATH_IF_HW_OFF matters. If this flag is set, it will disable
     *                all the interrupts and stop the HW. this flag should be set if this is the last vap that is
     *                being deactivated and no other vap is in active state (join, listen (or) up).
     * @return 0 on success; -EINVAL indicates the interface with the ID does not exist.
     */
    int         (*down)(ath_dev_t, int if_id, u_int flags);

    /**
     * This routine brings the VAP out of the down state into a "listen" state
     *  where it can recieve all the directed frames without bssid filtering. this
     *  is called when a vap needs to  scan while it is down state.
     * @param dev : handle to LMAC device
     * @param if_id : the interface ID
     * @return 0 on success; -EINVAL indicates the interface with the ID does not exist.
     */
    int         (*listen)(ath_dev_t, int if_id);

    /**
     * callback to put the underlying H/W to join state for a specific VAP
     * @param dev : handle to LMAC device
     * @param if_id : the interface ID
     * @param bssid : the BSSID string
     * @param flags : defined below. For this callback, only two flags matter currently: ATH_IF_HW_ON and ATH_IF_HT.
     * ATH_IF_HW_ON : if set turns on  the HW , enables interrupts ,enable tx and rx engines.this should be used
     *                if no other vaps are active(either in join , listen (or) up state).
     * ATH_IF_HT    : if set uses multiple chains for proper HT (11n) operation and uses chain mask
     *                if not set uses chain mask
                      set using set_rx_chainmasklegacy, set_tx_chainmasklegacy functions.
     * @return 0 on success;  -EINVAL indicates the interface with the ID does not exist.
     */
    int         (*join)(ath_dev_t, int if_id, const u_int8_t bssid[IEEE80211_ADDR_LEN], u_int flags);

    /**
     * callback to prepare things needed for the VAP to run. This VAP is in RUN state and ready to
     * function if this callback succeeds.
     * @param dev   : handle to LMAC device
     * @param if_id : the interface ID
     * @param bssid : the BSSID string
     * @param aid   : current association ID(valid for STA vap and 0 for AP vap).
     * @param flags : defined below.
     * ATH_IF_HW_ON : if set turns on  the HW , enables interrupts ,enable tx and rx engines.this should be used
     *                if no other vaps are active(either in join, listen  (or) up state).
     * ATH_IF_HT    : if set uses multiple chains for proper HT (11n) operation.
     * @return 0 on success;  -EINVAL indicates the interface with the ID does not exist.
     */
    int         (*up)(ath_dev_t, int if_id, const u_int8_t bssid[IEEE80211_ADDR_LEN], u_int8_t aid, u_int flags);

    /**
     *  This routine will disable TX and wait for radar signal. If no radar found during
     *   CAC period, UMAC DFS Timer thread will timeout and send Event to transtion to UP state.
     *
     * @param dev   : handle to LMAC device
     * @param if_id : the interface ID
     * @return 0 on success;  -EINVAL indicates the interface with the ID does not exist.
     */
    int         (*dfs_wait)(ath_dev_t dev, int if_id);

    /**
     *  This routine will deactivate the vap similar to down . The difference is that this routine
     *   will not stop the HW and will keep the HW on (keep the tx,rx and interrupts on)
     *   and let any pending transimits to complete. For graceful bring down of the vap, this routine should
     *   be called and wait for all the pending transmits to complete (for this umac needs to keep track of number
     *   tx frames  pending for a given vap) and the call down.
     *
     * @param dev   : handle to LMAC device
     * @param if_id : the interface ID
     * @return 0 on success;  -EINVAL indicates the interface with the ID does not exist.
     */
    int         (*stopping)(ath_dev_t dev, int if_id);
    /*
     *@breif sequence of calls to be used for different vap operations.
     * associating a STA VAP from down state     :  join , up.
     * scan on any VAP in down state             :  listen(at the begining of scan), down (at the end of scan).
     * bringing up an AP VAP from down state     :  join,up.
     * bringing up an IBSS VAP from down state   :  join,up.
     * bringing down any VAP from up state       :  stopping (optional), down.
     */
#define ATH_IF_HW_OFF           0x0001  /* hardware state needs to turn off */
#define ATH_IF_HW_ON            0x0002  /* hardware state needs to turn on */
#define ATH_IF_HT               0x0004  /* STA only: the associated AP is HT capable */
#define ATH_IF_PRIVACY          0x0008  /* AP/IBSS only: current BSS has privacy on */
#define ATH_IF_BEACON_ENABLE    0x0010  /* AP/IBSS only: enable beacon */
#define ATH_IF_BEACON_SYNC      0x0020  /* IBSS only: need to sync beacon */
#define ATH_IF_DTURBO           0x0040  /* STA: the associated AP is dynamic turbo capable
                                         * AP: current BSS is in turbo mode */

    /* To attach/detach per-destination info (i.e., node) */
    /**
     * callback to allocate and initialize an ath_node_t(lmac node) object.
     * @param dev      : handle to LMAC device
     * @param if_id    : the VAP this node is associated with
     * @param ni       : the ieee80211_node_t this node is associated with
     *                   (internally ath_node will keep a back reference).
     * @param tmpnode  : A bool to indicate whether this node is for temporary use.
     *                   for temp nodes lmac will not allocate memory to keep track of  aggregation state.
     * @return the pointer to the newly allocated node
     */
    ath_node_t  (*alloc_node)(ath_dev_t, int if_id, ieee80211_node_t, bool tmpnode, void *peer);

    /**
     * callback to free a ath_node_t structure
     * @param dev  : handle to LMAC device
     * @param node : the target node to be freed.
     */
    void        (*free_node)(ath_dev_t, ath_node_t);

    /**
     * callback to clean up the states of a node. Both TX and RX buffers for the node will be cleaned
     *  this is called for a node that the vap lost connection to (a STA node leaving bss on a AP,IBSS vap,
     *  or bss node of STA vap when it is disassociated with its AP). this should be called before free_node.
     * @param dev : handle to LMAC device.
     * @param an  : the target node to clean up.
     * @return zero if cleanup done, non-zero if skipped.
     */
    int        (*cleanup_node)(ath_dev_t, ath_node_t);

    /**
     * callback to update the power save state of a node. It will pause traffic for the node if entering
     * power save state and unpause when the node exits powersave.
     * @param dev     : handle to LMAC device
     * @param node    : the target node to update power save state
     * @param pwrsave : if this value is non-zero, the target node will go to power save state. If this
     * value is non zero, traffic on all tids is paused including management tid.A special tid
     * number 16 is used for queuing managemt traffic. (so total of 17 tids). if the valus is 0 then traffic
     * on all tids is unpaused.
     */
    void        (*update_node_pwrsave)(ath_dev_t, ath_node_t, int, int);

    /* notify a newly associated node */
    /**
     * Setup driver-specific state for a newly associated node.
     * should be called when ever a station (re)associates with an AP VAP (or) a STA
     * joins for an IBSS vap (or) STA VAP joins a network(need to use the bss node representing AP ).
     * @param dev     : handle to LMAC device
     * @param node    : the newly assiciated node
     * @param isnew   : 0 inidcates this is a reassociation; non-zero value indicates a new association
     * @param isuapsd : 0 indicates this node not supporting UAPSD; non-zero vice versa
     */
    void        (*new_assoc)(ath_dev_t, ath_node_t, int isnew, int isuapsd);

    /* interrupt processing */
    /**
     * callback to enable interrupt generation from atheros HW.
     * @param dev : the handle to LMAC device
     */
    void        (*enable_interrupt)(ath_dev_t);

    /**
     * callback to disable interrupt generation from atheros HW.
     * @param dev : the handle to LMAC device
     */
    void        (*disable_interrupt)(ath_dev_t);
#define ATH_ISR_NOSCHED         0x0000  /* Do not schedule bottom half/DPC                */
#define ATH_ISR_SCHED           0x0001  /* Schedule the bottom half for execution        */
#define ATH_ISR_NOTMINE         0x0002  /* This was not my interrupt - for shared IRQ's    */
    /**
     * Interrupt handler used to serve the interrup. Most of the actual processing is deferred.
     * this should be called from the OS ISR routine registerd for the driver.
     * @param dev      : the handle to LMAC device
     * @return possible return values are defined above.
     * ATH_ISR_NOSCHED : do not schedule bottom half/DPC,
     * ATH_ISR_SCHED   : schedule the bottom half for execution  and in the bottom half routine
     *                   handle_intr callback needs to be called.
     * ATH_ISR_NOTMINE : the interrupt is not for this device
     *                 (for shared IRQ's) and do not schedule bottom half.
     * note that it may be required to translate this rerurn value to os specific return value to
     * be returned to os indicating weather the interrupt handled (or) not.
     * Also note that a platform can choose to call handle_isr directly in the isr context
     * instead of scheduling a bottom half
     */
    int         (*isr)(ath_dev_t);

    /**
     * Message  interrupt handler. Most of the actual processing is deferred
     * @param dev    : the handle to LMAC device
     * @param mesgid : the interrupt message
     * @return possible return values are defined above. ATH_ISR_NOSCHED means do not schedule bottom half/DPC,
     * ATH_ISR_SCHED means schedule the bottom half for execution and ATH_ISR_NOTMINE means the
     * interrupt is not for this device (for shared IRQ's) and do not schedule bottom half.
     * note that it may be required to translate this rerurn value to os specific return value to
     * be returned to os indicating weather the interrupt handled (or) not.
     * Also note that a platform can choose to call handle_isr directly in the isr context
     * instead of scheduling a bottom half
     */
    int         (*msisr)(ath_dev_t, u_int);

    /*
     * @brief each plat form needs to choose either isr or msisr for interrupt handling.
     */

    /**
     * Deferred interrupt processing can be called in the Tasklet context (or)
     * in the ISR context. both the tx frame completion and rx frame completion
     * gets handled in this routine.
     * @param dev : the handle to LMAC device
     */
    void        (*handle_intr)(ath_dev_t);

    /* reset the HW. to reset the hardware the follwing 3 APIs need to  be called in sequence */
    /**
     * Start the Reset operation. part of the reset start the TX and RX engines are stopped and interrupts are disabled.
     * @param dev : the handle to LMAC device
     * @param no_flush   : a boolean value to indicate whether the tx buffers on HW queues need to be drained
     * @param tx_timeout : the time the driver waits for TX DMA to stop. if 0 is used then the lmac uses a default timeout.
     * @param rx_timeout : the time the driver waits for RX DMA to stop. if 0 is used then the lmac uses a default timeout.
     * @return always be 0
     */
    int         (*reset_start)(ath_dev_t, HAL_BOOL, int tx_timeout, int rx_timeout);

    /**
     * Reset the hardware (HAL)
     * @param dev : the handle to LMAC device
     * @return 0 on success; otherwise means the reset failed
     */
    int         (*reset)(ath_dev_t);

    /**
     * Finish the reset procedure and resstart the tx,rx engines and enable interrupts.
     * @param dev      : the handle to LMAC device
     * @param no_flush : if no_flush is true, the TX queue will be scheduled to transmit
     * @return the return value is always 0
     */
    int         (*reset_end)(ath_dev_t, HAL_BOOL);

    /**
     * Switch the current operation mode of the HW. If the target mode is not the same as the existed mode,
     * a full reset procedure will be conducted (reset_start -> reset -> reset_end).
     * this api is used when it is required to bring up/down a combination of STA and AP vaps.
     * for simple case of multiple AP and nostabeacon STA vaps there is no need to switch opmode (should remain AP).
     * @param dev    : the handle to LMAC device
     * @param opmode : the operation mode for the device. The possible modes are defined in HAL_OPMODE
     * @return always return 0
     */
    int         (*switch_opmode)(ath_dev_t, HAL_OPMODE opmode);

    /* set and get channel */
    /**
     * callback for the protocol layer to change the current channel the AP is serving
     * @param dev : the handle to LMAC device
     * @param hchan : pointer to channel structure with new channel information
     * @param tx_timeout : the time the driver waits for TX DMA to stop. if 0 is used then the lmac uses a default timeout.
     * @param rx_timeout : the time the driver waits for RX DMA to stop. if 0 is used then the lmac uses a default timeout.
     * @param ignore_dfs : if true then radar detectio feature will not be enabled when we are in a DFS channel
     */
    void        (*set_channel)(ath_dev_t, HAL_CHANNEL *, int tx_timeout, int rx_timeout, bool flush, bool ignore_dfs);

    /**
     * callback to return the channel information of the current channel
     * @param dev : the handle to LMAC device
     * @return pointer to the channel structure of the current channel
     */
	HAL_CHANNEL *(*get_channel)(ath_dev_t dev);

    /* scan notifications */
    /**
     * This function is called when starting a channel scan.
     * set the filter for the scan, and get the chip ready to send broadcast packets out during the scan.
     * @param dev : the handle to LMAC device
     */
    void        (*scan_start)(ath_dev_t);

    /**
     * This routine is called by the upper layer when the scan is completed.  This will set the filters back to
     * normal operating mode, set the BSSID to the correct value.
     * @param dev : the handle to LMAC device
     */
    void        (*scan_end)(ath_dev_t);
    /**
     * This routine is called by the upper layer when the scan is completed.  This will enable all the txq.
     * @param dev : the handle to LMAC device
     */
    void        (*scan_enable_txq)(ath_dev_t);

    /**
     * This routine will call the LED routine that "blinks" the light indicating the device is in Scan mode.
     * This sets no other status.
     * @param dev : the handle to LMAC device
     */
    void        (*led_scan_start)(ath_dev_t);

    /**
     * Calls the LED Scan End function to change the LED state back to normal. Does not set any other states.
     * @param dev : the handle to LMAC device
     */
    void        (*led_scan_end)(ath_dev_t);

    /**
     * force ppm notication. only used for older chipsets for doing  calibration.
     * not required for newer chipsets including merlin, osprey and beyond.
     * @param dev   : the handle to LMAC device
     * @param event : notification event.
     * @param bssid : bssid of the nosing network.
     *
     */
    void        (*force_ppm_notify)(ath_dev_t, int event, u_int8_t *bssid);

    /**
     * synchronize beacon. when called the lmac reprograms
     * beacon config regsisters. only needed when the chip is operating in STA
     * mode and when station looses clock synchronization with AP . example is at the
     * scan and station  receives beacon miss during sleep ..etc.
     * @param dev   : the handle to LMAC device
     * @param if_id : the interface ID
     */
    void        (*sync_beacon)(ath_dev_t, int if_id);

    /**
     * update the lmac(HAL) with beacon rssi.
     * called when in STA/IBSS mode. depedning on the
     * the value of the rssi the HW will adjust the transmit power.
     * if the rssi is high the implementation of the functio  reduces the xmit power.
     * This is used to reduce power consumption in STA/IBSS mode.
     * @param dev     : the handle to LMAC device
     * @param avgrssi : the average rssi of the received beacon
     *                  from the associated AP/IBSS STA.
     */
    void        (*update_beacon_info)(ath_dev_t, int avgrssi);

    /**
     * This routine will help in modifying the beacon rate as per the user's choice.
     *
     * @param dev   : handle to LMAC device
     * @param if_id : the interface ID
     * @param new_rate: The new rate passed by user in which rate beacons will get transmitted
     */
    bool         (*modify_bcn_rate)(ath_dev_t dev, int if_id, int new_rate);

    /**
     *  configure the txpower for the mgmt frames
     * called whenever the user configures some value
     * @param dev           : the handle to LMAC device
     * @param if_id         : the interface ID
     * @param frame_subtype : the frame subtype
     * @param transmit power: the tx power
     */
    void         (*txpow_mgmt)(ath_dev_t dev, int if_id, int frame_subtype, u_int8_t transmit_power);

    void         (*txpow)(ath_dev_t dev, int if_id, int frame_type, int frame_subtype, u_int8_t transmit_power);
#if ATH_SUPPORT_WIFIPOS
    /**
     * updating the location based register
     */
    void       (*update_loc_ctl_reg)(ath_dev_t, int pos_bit);
    u_int32_t  (*read_loc_timer_reg)(ath_dev_t);
    u_int8_t   (*get_eeprom_chain_mask)(ath_dev_t);
#endif

    u_int32_t   (*ledctl_state) (ath_dev_t dev,u_int32_t bOn);
    int         (*radio_info) (ath_dev_t dev);

#ifdef ATH_USB
    /* Heart beat callback */
    void        (*heartbeat_callback)(ath_dev_t);
#endif

    /* tx callbacks */
    /**
     * callback to initialize tx engine. called at device attach time immediatly after creating the lmac device handle.
     * @param dev   : the handle to LMAC device
     * @param nbufs : number ath buffers to allocate.ath buf carries information about a
     *                a frame. one ath buf is used for each frame (wbuf transmitted).
     * @return 0 means successfully initialized the tx engine.
     */
    int         (*tx_init)(ath_dev_t, int nbufs);

    /**
     * reclaim all tx queue resource
     * @param dev : the handle to LMAC device
     */
    int         (*tx_cleanup)(ath_dev_t);

    /**
     * return TX queue number (id)
     * @param dev     : the handle to LMAC device
     * @param qtype   : transmit queue types. Possible values are defined in HAL_TX_QUEUE
     * @param haltype : Transmit queue subtype.  These map directly to WME Access Categories (except for UPSD).
     * Possible values are defined in HAL_TX_QUEUE_SUBTYPE
     * @return the number/id of the TX queue with the queue type/subtype.
     */
    int         (*tx_get_qnum)(ath_dev_t, int qtype, int haltype);

    /**
     * Update the parameters for a transmit queue
     * @param dev  : the handle to LMAC device
     * @param qnum :  the tranmist queue numbered with qnum
     * @param qi   : the transmit queue information to update
     * @return 0 means succeed; otherwise failed
     */
    int         (*txq_update)(ath_dev_t, int qnum, HAL_TXQ_INFO *qi);

    /**
     * callback to queue the wbuf to transmit queue and start to transmit buffers in transmit queue
     * @param dev  : handle to LMAC device
     * @param wbuf : the wbuf to be queued
     * @param txctl: the transmit control information for this wbuf
     * @return 0 on success; otherwise failed
     */
    int         (*tx)(ath_dev_t, wbuf_t wbuf, ieee80211_tx_control_t *txctl);

    /**
     * deferred processing of transmit interrupt
     * it should be called when ever there is a transmit interrupt .
     * the lmac calls the ath_handle_tx_intr() function implemented by the platform specific layer and
     * a typical implementation will call the tx_proc callback.
     * @param dev : handle to LMAC device
     */
    void        (*tx_proc)(ath_dev_t);

    /**
     * Flush the pending traffic in tx queue. Beacon queue is not stopped
     * @param dev : handle to LMAC device
     */
    void        (*tx_flush)(ath_dev_t);

    /**
     * callback to return the queue depth in a specific transmit queue
     * @param dev  : handle to LMAC device
     * @param qnum :  the transmit queue numbered with the qnum
     * @return the queue depth in the transmit queue
     */
    u_int32_t   (*txq_depth)(ath_dev_t, int qnum);

    /**
     * callback to return the number of aggregations queued in a specific transmit queue
     * @param dev  : handle to LMAC device
     * @param qnum : the transmit queue numbered with the qnum
     * @return the number of aggregations queued in a specific transmit queue
     */
    u_int32_t   (*txq_aggr_depth)(ath_dev_t, int qnum);

#if ATH_TX_BUF_FLOW_CNTL
    /**
     * callback to set the min number of free buffers in a specific transmit queue before frames are dropped
     * @param dev  : handle to LMAC device
     * @param qnum : the transmit queue numbered with the qnum
     * @param haltype :
     * @param minfree : the min number of free buffers in a specific transmit queue
     */
    void        (*txq_set_minfree)(ath_dev_t, int qtype, int haltype, u_int minfree);

    /**
     * callback to return the number of buffers used up in a specific transmit queue
     * @param dev  : handle to LMAC device
     * @param qnum : the transmit queue numbered with the qnum
     * @return the number of buffers used up in a specific transmit queue
     */
    u_int32_t   (*txq_num_buf_used)(ath_dev_t, int qnum);
#endif

#if ATH_TX_DROP_POLICY
    /**
     * callback to drop the unnecessary fragmented packet
     * @param wbuf  : wbuf containing the tx frame
     * @param arg   : tx control block
     * @return 0 on success; otherwise failed
     */
    int         (*tx_drop_policy)(wbuf_t wbuf, void *arg);
#endif

    /* rx callbacks */
    /**
     * callback to initialize xx engine. called at device attach time immediatly after creating the lmac device handle.
     * allocates ath bufs to be used for receiving frames.
     * @param dev   : the handle to LMAC device
     * @param nbufs : the size of the RX queue
     * @return 0 on success; otherwise failed.
     */
    int         (*rx_init)(ath_dev_t, int nbufs);

    /**
     * reclaim all rx queue resource
     * @param dev : the handle to LMAC device
     */
    void        (*rx_cleanup)(ath_dev_t);

    /**
     * deferred processing of receive interrupt
     * it should be called when ever there is a rx interrupt .
     * the lmac calls the ath_handle_rx_intr() function implemented by the platform specific layer and
     * a typical implementation will call the tx_proc callback.
     * @param dev : handle to LMAC device
     */
    int         (*rx_proc)(ath_dev_t, int flushing);

    /**
     * callback to requeue this buffer to hardware
     * lmac hands over a received frame from HW queue to platform specific layer via the
     * function ath_rx_indicate() which is implemented by the platform specific layer. the implementation  of the
     * ath_rx_indicate() after processing the received wbuf , it should queue back  a new one using this call back function.
     * @param dev  : handle to LMAC device
     * @param wbuf : the buffer to be requeued
     */
    void        (*rx_requeue)(ath_dev_t, wbuf_t wbuf);

    /**
     * handle rx buffer reordering of an individual frames . If the processing is done in LMAC,
     * the status would be ATH_RX_CONSUMED; otherwise, ATH_RX_NON_CONSUMED . this call back is made
     * from the ath_rx_indicate function in platform layer. if the ATH_RX_NON_CONSUMED is returned
     * then the caller (ath_rx_indicate) should hand over the frame to upper layers.
     * @param dev       : handle to LMAC device
     * @param node      : the target node this frame is delivered to
     * @param is_ampdu  : 1 means this node is AMPDU capable.
     * @param wbuf      : the buffer pointer of the frame.
     * @param rx_status : the rx status for this frame.
     * @param status    : the variable to store the result of LMAC frame processing
     * @return the type of the frame. Possible values could be IEEE80211_FCO_TYPE_MGT,
     * IEEE80211_FCO_TYPE_CTL, IEEE80211_FCO_TYPE_DATA or -1 (not a valid frame)
     */
    int         (*rx_proc_frame)(ath_dev_t, ath_node_t, int is_ampdu,
                                 wbuf_t wbuf, ieee80211_rx_status_t *rx_status,
                                 ATH_RX_TYPE *status);

    /* aggregation callbacks */
    /**
     * Check if an ADDBA is required for a TID.
     * @param dev   : handle to LMAC device
     * @param node  : the target node to check
     * @param tidno : the TID number
     * @return 0 indicates not required; 1 indicates required.
     */
    int         (*check_aggr)(ath_dev_t, ath_node_t, u_int8_t tidno);

    /**
     * Set the AMPDU parameters of a node
     * @param dev        : handle to LMAC node
     * @param node       : target node to set the AMPDU parameters
     * @param maxampdu   :  max AMPDU length
     * @param mpdudensity: max AMPDU factor
     */
    void        (*set_ampdu_params)(ath_dev_t, ath_node_t, u_int16_t maxampdu, u_int32_t mpdudensity);

    /**
     * Set the delimiter count for WEP/TKIP. Delimiter count for WEP/TKIP is the maximum of
     * the delim count required by the receiver and the delim count required by the device for transmitting.
     * only used for a proprietary mode where AMPDU is enabled with WEP/TKIP.
     * @param dev      : handle to LMAC device
     * @param node    : the target node to set delimiter count
     * @param rxdelim : the number to set.
     */
    void        (*set_weptkip_rxdelim)(ath_dev_t, ath_node_t, u_int8_t rxdelim);

    /**
     * callback to setup ADDBA request. umac needs to call this API to get
     * the Block ack request parameters and use them to construct Addba request.
     * @param dev        : handle to LMAC device
     * @param node       : the node associated with this ADDBA request
     * @param tindo      : the TID number for this ADDBA request
     * @param baparamset : the data structure for lmac to fill in ADDBA request info
     * @param batimeout  : the pointer for lmac to fill in ADDBA timeout
     * @param basequencectrl : the data structure for lmac to fill in frame sequence control info
     * @param buffersize : BAW(block ack window size) for transmit AMPDU.
     */
    void        (*addba_request_setup)(ath_dev_t, ath_node_t, u_int8_t tidno,
                                       struct ieee80211_ba_parameterset *baparamset,
                                       u_int16_t *batimeout,
                                       struct ieee80211_ba_seqctrl *basequencectrl,
                                       u_int16_t buffersize);

    /**
     * callback to setup ADDBA response  umac needs to call this API to get
     * the Block ack response parameters and use them to construct Addba response.
     * @param dev        : handle to LMAC device
     * @param an         : the node associated with this ADDBA response
     * @param tindo      : the TID number for this ADDBA response
     * @param dialogtoken: The dialog token for the response (filled by lmac)
     * @param statuscode : The status code for this response (filled by lmac)
     * @param baparamset : The BA parameters for this response (filled by lmac)
     * @param batimeout  : The BA timeout for this response (filled by lmac)
     */
    void        (*addba_response_setup)(ath_dev_t, ath_node_t an,
                                        u_int8_t tidno, u_int8_t *dialogtoken,
                                        u_int16_t *statuscode,
                                        struct ieee80211_ba_parameterset *baparamset,
                                        u_int16_t *batimeout);

    /**
     * Process ADDBA request and save response information in per-TID data structure
     * umac needs to call this function when it receives addba request.
     * @param dev         : handle to LMAC device
     * @param an          : the node associated with the ADDBA request
     * @param dialogtoken : the dialog token saved for future ADDBA response
     * @param baparamset  : pointer to the BA parameters from  received the ADDBA request
     * @param batimeout   : the BA timeout value from ADDBA request
     * @param basequencectrl : the BA sequence control structure from received  ADDBA request
     * @return 0 on success; -EINVAL indicates that aggregation is not enabled.
     *    umac needs to send an addba response with success code if the return value is 0.
     */
    int         (*addba_request_process)(ath_dev_t, ath_node_t an,
                                         u_int8_t dialogtoken,
                                         struct ieee80211_ba_parameterset *baparamset,
                                         u_int16_t batimeout,
                                         struct ieee80211_ba_seqctrl basequencectrl);

    /**
     * Process ADDBA response
     * umac needs to call this function when it receives addba response.
     * @param dev        : handle to LMAC device
     * @param node       : the node associated with the received ADDBA response
     * @param statuscode : the status code from the received ADDBA response
     * @param baparamset : pointer to the BA parameter structure from the received  ADDBA response
     * @param batimeout  : the BA timeout value from the received ADDBA response
     */
    void        (*addba_response_process)(ath_dev_t, ath_node_t,
                                          u_int16_t statuscode,
                                          struct ieee80211_ba_parameterset *baparamset,
                                          u_int16_t batimeout);

    /**
     * Clear ADDBA for all tids in this node
     * @param dev : handle to LMAc device
     * @param node : the target node to clear ADDBA
     */
    void        (*addba_clear)(ath_dev_t, ath_node_t);

    /**
     * Cancel ADDBA timers for all tids in this node
     * @param dev : handle to LMAC device
     * @param node : the target node to cancel ADDBA timers
     */
    void        (*addba_cancel_timers)(ath_dev_t, ath_node_t);

    /**
     * Process DELBA
     * umac needs to call this function when it receives DELBA.
     * @param dev           : handle to LMAC device
     * @param an            : the node associated with this DELBA
     * @param delbaparamset : point to the DELBA parameter structure
     * @param reasoncode    : reason code from this DELBA
     */
    void        (*delba_process)(ath_dev_t, ath_node_t an,
                                 struct ieee80211_delba_parameterset *delbaparamset,
                                 u_int16_t reasoncode);

    /**
     * callback to get the ADDBA status of a TID
     * @param dev : handle to LMAC device
     * @param node : the node this TID associates with
     * @param tidno : the TID number
     * @return ADDBA status code
     */
    u_int16_t   (*addba_status)(ath_dev_t, ath_node_t, u_int8_t tidno);

    /**
     * Tear down either tx or rx aggregation. This is usually called
     * before protocol layer sends a DELBA.
     * @param dev   : handle to LMAC device
     * @param node  : the node the TID associates with
     * @param tidno : the TID number of the TID to tear down aggregation
     * @param initiator : 0 indicates a rx aggregation to tear down; non-zero for a tx aggregation
     */
    void        (*aggr_teardown)(ath_dev_t, ath_node_t, u_int8_t tidno, u_int8_t initiator);

    /**
     * Set the user defined ADDBA response status code for this TID
     * the user/umac can set a specific response for certain tids in certain nodes.
     * for example umac will set the response status to IEEE80211_STATUS_REFUSED for all tids of
     * a non HT capable station (or) on stations if user wants to disable aggregation. default status
     * inside the lmac for all tids is IEEE80211_STATUS_SUCCESS.
     * @param dev  : handle to LMAC device
     * @param node : the node the TID associates with
     * @param tidno: the TID number for this ADDBA response
     * @param statuscode : the status code to set
     */
    void        (*set_addbaresponse)(ath_dev_t, ath_node_t, u_int8_t tidno, u_int16_t statuscode);

    /**
     * Clear the user define ADDBA response status code for all tids of the node
     * and set it back to IEEE80211_STATUS_SUCCESS.
     * @param dev : handle to LMAC device
     * @param node : the target node to clear status code
     */
    void        (*clear_addbaresponsestatus)(ath_dev_t, ath_node_t);

    /* Misc runtime configuration */
    /**
     * This is a call interface used by the protocol layer to update the current slot time setting based on
     * updated settings.  This updates the hardware also. this API is used to switch the slot time
     * between 9 usec (short slot time) and 20usec (long slot time) . To support a 11b Station the slot time
     * needs to be long.
     * @param dev      : handle to LMAC device
     * @param slottime : slot duration in microseconds
     */
    void        (*set_slottime)(ath_dev_t, int slottime);

    /**
     * Set the protection mode. the protection mode is common to all VAPS.
     *
     * @param dev  : handle to LMAC device
     * @param mode : protection mode to set. Possible values are defined in PROT_MODE
     */
    void        (*set_protmode)(ath_dev_t, PROT_MODE);

    /**
     * Set the transmit power limit in lmac. the power limit is cached and updated to HW at the end of
     * channel change (or) reset operation.
     * @param dev        : handle to LMAC device
     * @param twpowlimit : 16 bit transmit power limit in db.
     * @param is2GHz     : used to identiy whether the configuration is for 2G or 5G
     */
    int         (*set_txpowlimit)(ath_dev_t, u_int16_t txpowlimit, u_int32_t is2GHz);

    /**
     * Enable the transmit power control
     * @param dev : handle to LMAC device
     */
    void        (*enable_tpc)(ath_dev_t);

    /* get/set MAC address, multicast list, etc. */
    /**
     * Copy the current MAC address to the buffer provided
     * @param dev     : handle to LMAC device
     * @param macaddr : buffer to store MAC address
     */
    void        (*get_macaddr)(ath_dev_t, u_int8_t macaddr[IEEE80211_ADDR_LEN]);

    /**
     * Copy the MAC address read from EEPROM to the buffer provided
     * @param dev     : handle to LMAC device
     * @param macaddr : buffer to store MAC address
     */
    void        (*get_hw_macaddr)(ath_dev_t, u_int8_t macaddr[IEEE80211_ADDR_LEN]);

    /**
     * Copy the provided MAC address into LMAC device structure , and set the
     *  value into the chipset through the HAL.
     * @param dev     : handle to LMAC device
     * @param macaddr : the MAC address to store
     */
    void        (*set_macaddr)(ath_dev_t, const u_int8_t macaddr[IEEE80211_ADDR_LEN]);

    /**
     * Set multi-cast address
     * @param dev : handle to LMAC device
     * this API is deprecatd.
     */
    void        (*set_mcastlist)(ath_dev_t);

    /**
     * Set the RX filter. The receive filter is calculated according to the operating mode and state
     * o always accept unicast, broadcast, and multicast traffic
     * o maintain current state of phy error reception (the hal
     *   may enable phy error frames for noise immunity work)
     * o probe request frames are accepted only when operating in
     *   hostap, adhoc, or monitor modes
     * o enable promiscuous mode according to the interface state.
     * o accept beacons:
     *  all the modes except in STA mode if the interface settings hasthe ATH_NETIF_NO_BEACON flag is set.
     * @param dev : handle to LMAC device
     * this API internally calls back the get_netif_settings from ieee80211_ops strucutre (above)
     * to get the network interface settings.
     * to set the HW into promiscuous mode, you  need to call this API and the in the implemenation of
     * get_netif_settings should return  ATH_NETIF_PROMISCUOUS in the returned flags.
     */
    void        (*set_rxfilter)(ath_dev_t);

    /**
     * Get the RX filter.
     */
    u_int32_t   (*get_rxfilter)(ath_dev_t);

    /**
     * Initialize the device operation. This callback will set RX filter, BSSID mask, operation mode,
     * MAC, multicast filter to H/W
     * @param dev : handle to LMAC device
     */
    void        (*mc_upload)(ath_dev_t);

    /* key cache callbacks */
    /**
     * Allocate tx/rx and MIC key slots for TKIP.
     * it allocates 4 slots.one for encrypt , one for decrypt and other
     * 2 dor RX MIC and TX MIC.
     * @param dev : handle to LMAC device
     * @return keyindex identifying the 4 slots.
     * this is only  need to be called if the ATH_CAP_TKIP_SPLITMIC capability
     * is set to true for the HW( i.e. only for older HW and not owl,merlin , osprey and beyond).
     */
    u_int16_t   (*key_alloc_2pair)(ath_dev_t);

    /**
     * Allocate data  key slots for TKIP.  We allocate two slots for
     * one for decrypt/encrypt and the other for the MIC.
     * @param dev : handle to LMAC device
     * @return key index of the fisr slot (second slot index = (first slot index + 64) ).
     * this is only  need to be called if the ATH_CAP_TKIP_SPLITMIC capability
     * is not set to true for the HW(for newer HW including owl,merlin ,osprey and beyond).
     */
    u_int16_t   (*key_alloc_pair)(ath_dev_t);

    /**
     * Allocate a single key cache slot
     * @param dev : handle to LMAC device
     * @return key index of the allocated slot .
     */
    u_int16_t   (*key_alloc_single)(ath_dev_t);

    /**
     * Clear the specified key cache entry.
     * @param dev      : handle to LMAC device
     * @param keyix    : the key cache entry index
     * @param freeslot : whether to the free the key cache entry or not
     * Note this function will only free one slot at the the given index.
     * in TKIP case caller needs to free the other 1 (or)3 slot(s) allocated depending
     * upon how they are allocated (key_alloc_2pair vs key_alloc_pair).
     */
    void        (*key_delete)(ath_dev_t, u_int16_t keyix, int freeslot);

    /**
     * Sets the contents of the specified key cache entry
     * @param dev   : handle to LMAC device
     * @param keyix : the key cache entry index
     * @param hk    : the HAL key value to set. Possible values are defined in HAL_KEYVAL
     * @param mac   : the MAC address for this key cache
     * @return 0 for success.
     */
    int         (*key_set)(ath_dev_t, u_int16_t keyix, HAL_KEYVAL *hk,
                           const u_int8_t mac[IEEE80211_ADDR_LEN]);

    /**
     * Return the size of the key cache
     * @param dev      : handle to LMAC device
     * @return the size of the key cache
     */
    u_int       (*keycache_size)(ath_dev_t);

    /* PHY state */
    /**
     * To query software PHY state
     * @param dev      : handle to LMAC device
     * @return 0 indicates off; 1 indicates on.
     */
    int         (*get_sw_phystate)(ath_dev_t);

    /**
     * To query hardware PHY state
     * @param dev      : handle to LMAC device
     * @return 0 indicates off; 1 indicates on.
     */
    int         (*get_hw_phystate)(ath_dev_t);

    /**
     * To set software PHY state
     * NB: this is just to set the software state. To turn on/off
     * radio, ath_radio_enable/ath_radio_disable has to be
     * explicitly called.
     * @param dev     : handle to LMAC device
     * @param swstate : 0 indicates off; 1 indicates on
     * @sw phy state is only used for LED control .
     */
    void        (*set_sw_phystate)(ath_dev_t, int onoff);

    /**
     * To enable PHY (radio on)
     * @param dev     : handle to LMAC device
     * @return 0 on success; -EIO indicates the handle is invalid.
     * radio enable/disable control is only used turn on/off the radio.
     * typically used on clients where you need to turn the radio on/off.
     */
    int         (*radio_enable)(ath_dev_t);

    /**
     * To disable PHY (radio off)
     * @param dev     : handle to LMAC device
     * @return 0 on success; -EIO indicates the handle is invalid.
     */
    int         (*radio_disable)(ath_dev_t);

    /* LED control */
    /**
     * Suspend LED control
     * @param dev     : handle to LMAC device
     */
    void        (*led_suspend)(ath_dev_t);

    /* power management, used for saving power in client mode */
    /**
     * Awake the hardware if it is in sleep mode
     * @param dev : handle to LMAC device
     */
    void        (*awake)(ath_dev_t);

    /**
     * Set the hardware to net sleep mode
     * in this mode HW only wakes up to receive beacon and
     * will go back to the sleep at the end.
     * @param dev : handle to LMAC device
     */
    void        (*netsleep)(ath_dev_t);

    /**
     * Set the hardware to full sleep mode. In this mode
     * the HW is completely turned off.
     * @param dev : handle to LMAC device
     */
    void        (*fullsleep)(ath_dev_t);

    /**
     * Get the current hardware power save state
     * @param dev : handle to LMAC device
     * @return the current hardware power save state. Possible values are define in ATH_PWRSAVE_STATE
     */
    int         (*getpwrsavestate)(ath_dev_t);

    /**
     * Set the hardware power save state
     * a redundant/single function to put the hardware into one of the
     * awake,fullsleep,network sleep states.
     * @param dev : handle to LMAC device
     * @param newstateint : the new power save state for hardware. Possible values are defined in ATH_PWRSAVE_STATE
     */
    void        (*setpwrsavestate)(ath_dev_t, int);

    /* regdomain callbacks */
    /**
     * callback to set the ieee802.11D country in to HW.
     * @param dev : handle to LMAC device
     * @param iso_name : the ISO 3166 code for this country
     * @param cc : country code
     * @param cmd : coutry list command. Possible values are defined in ieee80211_clist_cmd
     * @return 0 on success; otherwise, failed to set the country.
     *  in the case of success, the lmac implementation will update the channel list in the
     *  driver to match the contry and reg domain and indicate back the updated channel list via
     *  the callback function setup_channel_list . In the call back it will use the same cmd value
     *  that is passed in to this function.
     */
    int         (*set_country)(ath_dev_t, char *iso_name, u_int16_t, enum ieee80211_clist_cmd);

    /**
     * Return the current country and domain information
     * @param dev : handle to LMAC device
     * @param ctry : pointer to HAL country entry
     */
    void        (*get_current_country)(ath_dev_t, HAL_COUNTRY_ENTRY *ctry);

    /**
     * Set the regulatory domain to hardware
     * @param dev       : handle to LMAC device
     * @param regdomain : the regulatory domain to set to H/W. Possible values are define in EnumRd
     * @param wrtEeprom : 0 indicates not writing regulatory domain to EEPROM; otherwise, write to EEPROM.
     * NB: ar9300 EEPROM is read-only
     * @return 0 on success; otherwise, failed to set regulatory domain.
     *  this function should be used for testing purpose only . the reg domain information comes from EPROM and it
     *  should not be changed during normal mode of operation.
     */
    int         (*set_regdomain)(ath_dev_t, int regdomain, HAL_BOOL);

    /**
     * Get the regulatory domain from hardware
     * @param dev : handle to LMAC device
     * @return the regulatory domain value.
     */
    int         (*get_regdomain)(ath_dev_t);
    int         (*get_dfsdomain)(ath_dev_t);
    /**
     * Quiet the hardware for some duration
     * @param dev       : handle to LMAC device
     * @param period    : period of time for H/W to be quiet in TUs.
     * @param duration  : duration of quiet time in TUs.
     * @param nextStart : offset from tbtt in TUs where the the quiet period starts.
     * @param flag      : Options for setting quiet timer
     *   HAL_QUIET_DISABLE             :  disable quiet timer
     *   HAL_QUIET_ENABLE              :  enable quiet timer
     *   HAL_QUIET_ADD_CURRENT_TSF     :  include current TSF to nextStart
     *   HAL_QUIET_ADD_SWBA_RESP_TIME  :  include beacon response time to nextStart
     * @return  return 0 on success.
     */
    int         (*set_quiet)(ath_dev_t, u_int32_t period, u_int32_t duration,
                             u_int32_t nextStart, HAL_QUIET_FLAG flag);

    /**
     * Find the country code by ISO country string
     * @param dev     : handle to LMAC device
     * @param isoName : the ISO 3166 country string
     * @return the country code
     */
    u_int16_t   (*find_countrycode)(ath_dev_t, char* isoName);

    /**
     * Find the conformance_test_limit by country code.
     * @param dev     : handle to LMAC device
     * @param country : country code
     * @param is2G    : For 2G CTL or 5G CTL
     * @return conformance test limit
     */
    u_int8_t    (*get_ctl_by_country)(ath_dev_t, u_int8_t *country, HAL_BOOL is2G);

    /* antenna select */
    /**
     * Select the antenna to use
     * @param dev            : handle to LMAC device
     * @param antenna_select : Select the antenna setting. Possible values are defined in HAL_ANT_SETTING
     *  HAL_ANT_FIXED_A : will set the tx,rx chain mask to use antenna 0 only. will save the configurex tx/rx chainmask.
     *  HAL_ANT_FIXED_B : will set the tx,rx chain mask to use antenna 1 only. will save the configurex tx/rx chainmask.
     *  HAL_ANT_VARIABL : restores back the tx,xx chain masks to what it was configured.
     * @return 0 on success; -EIO means failed to select antenna setting.
     */
    int         (*set_antenna_select)(ath_dev_t, u_int32_t antenna_select);

    /**
     * Get the current antenna setting
     * @param dev : handle to LMAC device
     * @return the tx antenna used by last transmission by HW.
     * for 11n chipsets (owl,merlin,osprey ..) this returns 0 all the time.
     */
    u_int32_t   (*get_current_tx_antenna)(ath_dev_t);

    /**
     * Get the default antenna setting
     * @param dev : handle to LMAC device
     * @return the default antenna setting.
     *  if the system has external support for miltple antennas selection then this returns the
     *  default antenna slection setting for the receiving frame.
     */
    u_int32_t   (*get_default_antenna)(ath_dev_t);

    /* PnP */
    /**
     * Notification of device suprise removal event.
     * This is called by platfrom layer when the HW is removed (typically external PCIE card, PC card ..etc)
     * with this notification, lmac will stop accessing HW.
     * @param dev : handle to LMAC device
     */
    void        (*notify_device_removal)(ath_dev_t);

    /**
     * Detect if our card is present by reading HW revision registers.
     * @param dev : handle to LMAC device
     * @return 1 indicates the card is present; 0 indicates not present
     */
    int         (*detect_card_present)(ath_dev_t);

    /* convert frequency to channel number */
    /**
     * Convert the frequency, provided in MHz, to an ieee 802.11 channel number.
     * @param dev   : handle to LMAC device
     * @param freq  : the frequency to convert
     * @param flags : flag to indicate the properties of this frequency.
     * @return channel number
     */
    u_int       (*mhz2ieee)(ath_dev_t, u_int freq, u_int flags);

    /* statistics */
    /**
     * callback to get the current PHY statistics
     * @param dev   : handle to LMAC device
     * @param wmode : the wireless mode. Possible values are defined in WIRELESS_MODE
     * @return the pointer to ath_phy_stats structure
     */
    struct ath_phy_stats    *(*get_phy_stats)(ath_dev_t, WIRELESS_MODE);

    /**
     * callback to get the current PHY statistics for all modes
     * @param dev   : handle to LMAC device
     * @return a pointer to array of ath_phy_stats structures, containing
     *         WIRELESS_MODE_MAX array entries.
     */
    struct ath_phy_stats    *(*get_phy_stats_allmodes)(ath_dev_t);

    /**
     * callback to get the current device statistics
     * @param dev : handle to LMAC device
     * @return the pointer to ath_stats structure
     */
    struct ath_stats        *(*get_ath_stats)(ath_dev_t);

    /**
     * callback to get the 11N specific device statistics
     * @param dev : handle to LMAC device
     * @return the pointer to ath_11n_stats
     */
    struct ath_11n_stats    *(*get_11n_stats)(ath_dev_t);

    /**
     * callback to get the 11N specific device statistics
     * @param dev : handle to LMAC device
     * @return the pointer to ath_dfs_stats
     */
    struct ath_dfs_stats    *(*get_dfs_stats)(ath_dev_t);

    /**
     * callback to clean the PHY statistics
     * @param dev : handle to LMAc device
     */
    void                    (*clear_stats)(ath_dev_t);

    /* channel width management */
    /**
     * callback to set the MAC mode to H/W. Possible MAC modes are 20 MHz or 20/40 MHz.
     * @param dev     : handle to LMAC device
     * @param macmode : possible values are defined in HAL_HT_MACMODE
     */
    void        (*set_macmode)(ath_dev_t, HAL_HT_MACMODE);

    /**
     * callback to set channel spacing between main channel and extension channel.
     * Possible values are 20 or 25 MHz spacing
     * @param dev            : handle to LMAC device
     * @param extprotspacing : possible values are defined in HAL_HT_EXTPROTSPACING
     */
    void        (*set_extprotspacing)(ath_dev_t, HAL_HT_EXTPROTSPACING);

    /**
     * Return approximation of extension channel (20/40 mode) busy over an time interval
     * @param dev : handle to LMAC device
     * @return 0% (clear) -> 100% (busy)
     * -1 for invalid estimate
     */
    int         (*get_extbusyper)(ath_dev_t);

    /**
     * Return approximation of channel busy stats like ctrl/ext channel busy %,
     * Tx/Rx frame %.
     * @param dev : handle to LMAC device
     * @return 0% (clear) -> 100% (busy)
     * -1 for invalid estimate
     */
    unsigned int         (*get_chbusyper)(ath_dev_t);

#ifdef ATH_SUPPORT_DFS
    /**
     * Attach dfs subsystem.
     */
    int       (*ath_dfs_get_caps)(ath_dev_t, struct wlan_dfs_caps *);
    /**
     * Detach dfs subsystem..
     * @param dev            : handle to LMAC device
     */
    int       (*ath_detach_dfs)(ath_dev_t);
    /**
     * Enable DFS settings.
     * @param dev            : handle to LMAC device
     */
    int       (*ath_enable_dfs)(ath_dev_t, int *, HAL_PHYERR_PARAM *, u_int32_t);

    /**
     * Disable DFS settings.
     * @param dev            : handle to LMAC device
     */
    int       (*ath_disable_dfs)(ath_dev_t);
    /**
     * Get %RX, TX Clear/Busy counters .
     * @param dev            : handle to LMAC device
     */
    int     (*ath_get_mib_cycle_counts_pct)(ath_dev_t, u_int32_t *, u_int32_t *, u_int32_t *);

    /**
     * Fetch the current DFS settings from the hardware.
     * @param dev            : handle to the LMAC device
     * @param pe             : pointer to HAL_PHYERR_PARAM to populate
     */
    int     (*ath_get_radar_thresholds)(ath_dev_t, HAL_PHYERR_PARAM *);

#endif /* ATH_SUPPORT_DFS */

    void (*get_min_and_max_powers)(ath_dev_t dev,
                                   int8_t *max_tx_power,
                                   int8_t *min_tx_power);
    uint32_t (*get_modeSelect)(ath_dev_t dev);
    uint32_t (*get_chip_mode)(ath_dev_t dev);
    void     (*get_freq_range)(ath_dev_t dev,
                               uint32_t flags,
                               uint32_t *low_freq,
                               uint32_t *high_freq);
    void     (*fill_hal_chans_from_regdb)(ath_dev_t,
                                          uint16_t,
                                          int8_t,
                                          int8_t,
                                          int8_t,
                                          uint32_t,
                                          int);
#if ATH_WOW
    /* Wake on Wireless used on clients to wake up the system with a magic packet */
    int         (*ath_get_wow_support)(ath_dev_t);
    int         (*ath_set_wow_enable)(ath_dev_t, int clearbssid);
    int         (*ath_wow_wakeup)(ath_dev_t);
    void        (*ath_set_wow_events)(ath_dev_t, u_int32_t);
    int         (*ath_get_wow_events)(ath_dev_t);
    int         (*ath_wow_add_wakeup_pattern)(ath_dev_t,u_int32_t, u_int8_t *, u_int8_t *, u_int32_t);
    int         (*ath_wow_remove_wakeup_pattern)(ath_dev_t, u_int8_t *, u_int8_t *);
    int         (*ath_wow_get_wakeup_pattern)(ath_dev_t, u_int8_t *,u_int32_t *, u_int32_t *);
    int         (*ath_get_wow_wakeup_reason)(ath_dev_t);
    int         (*ath_wow_matchpattern_exact)(ath_dev_t);
    void        (*ath_wow_set_duration)(ath_dev_t, u_int16_t);
    void        (*ath_set_wow_timeout)(ath_dev_t, u_int32_t);
	u_int32_t   (*ath_get_wow_timeout)(ath_dev_t);
#if ATH_WOW_OFFLOAD
    void        (*ath_set_wow_enabled_events)(ath_dev_t, u_int32_t, u_int32_t, u_int32_t);
    void        (*ath_get_wow_enabled_events)(ath_dev_t, u_int32_t *, u_int32_t *, u_int32_t *);
    int         (*ath_get_wowoffload_support)(ath_dev_t);
    int         (*ath_get_wowoffload_gtk_support)(ath_dev_t dev);
    int         (*ath_get_wowoffload_arp_support)(ath_dev_t dev);
    int         (*ath_get_wowoffload_max_arp_offloads)(ath_dev_t dev);
    int         (*ath_get_wowoffload_ns_support)(ath_dev_t dev);
    int         (*ath_get_wowoffload_max_ns_offloads)(ath_dev_t dev);
    int         (*ath_get_wowoffload_4way_hs_support)(ath_dev_t dev);
    int         (*ath_get_wowoffload_acer_magic_support)(ath_dev_t dev);
    int         (*ath_get_wowoffload_acer_swka_support)(ath_dev_t dev, u_int32_t id);
    bool        (*ath_wowoffload_acer_magic_enable)(ath_dev_t dev);
    u_int32_t   (*ath_wowoffload_set_rekey_suppl_info)(ath_dev_t dev, u_int32_t id, u_int8_t *kck, u_int8_t *kek,
                        u_int64_t *replay_counter);
    u_int32_t   (*ath_wowoffload_remove_offload_info)(ath_dev_t dev, u_int32_t id);
    u_int32_t   (*ath_wowoffload_set_rekey_misc_info)(ath_dev_t dev, struct wow_offload_misc_info *wow);
    u_int32_t   (*ath_wowoffload_get_rekey_info)(ath_dev_t dev, void *buf, u_int32_t param);
    u_int32_t   (*ath_wowoffload_update_txseqnum)(ath_dev_t dev, struct ath_node *an, u_int32_t tidno, u_int16_t seqnum);
    int         (*ath_acer_keepalive)(ath_dev_t dev, u_int32_t id, u_int32_t msec, u_int32_t size, u_int8_t* packet);
    int         (*ath_acer_keepalive_query)(ath_dev_t dev, u_int32_t id);
    int         (*ath_acer_wakeup_match)(ath_dev_t dev, u_int8_t* pattern);
    int         (*ath_acer_wakeup_disable)(ath_dev_t dev);
    int         (*ath_acer_wakeup_query)(ath_dev_t dev);
    u_int32_t   (*ath_wowoffload_set_arp_info)(ath_dev_t dev, u_int32_t protocolOffloadId, void *pParams);
    u_int32_t   (*ath_wowoffload_set_ns_info)(ath_dev_t dev, u_int32_t protocolOffloadId, void *pParams);
#endif /* ATH_WOW_OFFLOAD */
#endif

    /* Configuration Interface */
    /**
     * This returns the current value of the indicated parameter.  Conversion to integer done here.
     * @param dev : handle to LMAC device
     * @param ID : Parameter ID value. Possible values are defined in the ath_param_ID_t type
     * @param buff : buffer to place the integer value into
     * @return 0 indicates the value in buff is valid; -1 indicates invalid.
     */
    int         (*ath_get_config_param)(ath_dev_t dev, ath_param_ID_t ID, void *buff);

    /**
     * This routine will set ATH parameters to the provided values.  The incoming values are always
     * encoded as integers -- conversions are done here.
     * @param dev : handle to LMAC device
     * @param ID : Parameter ID value. Possible values are defined in the ath_param_ID_t type
     * @param buff : integer pointer pointing to the value to set the parameter
     * @return 0 on success; -1 for unsupported values
     */
    int         (*ath_set_config_param)(ath_dev_t dev, ath_param_ID_t ID, void *buff);

    /* Noise floor func */
    /**
     * callback to get the noise floor value of a channel
     * @param dev : handle to LMAC device
     * @param freq : the frequency of the channel to get noise floor
     * @param chan_flags : the channel flags of the channel to get noise floor
     * @param wait_time : if wait_time > 0, we will try to get the current NF value saved in register.
     * @return the noise floor value
     */
    short       (*get_noisefloor)(ath_dev_t dev, unsigned short    freq,  unsigned int chan_flags, int wait_time);

    /**
     * callback to get noise Floor values for all chains.
     * Most recently updated values from the NF history buffer are used.
     * @param dev : handle to LMAC device
     * @param freq : the frequency of the channel
     * @param chan_flags : the channel flags of the channel
     * @param nfBuf : pointer to buffer to store the return values
     */
	void        (*get_chainnoisefloor)(ath_dev_t dev, unsigned short freq, unsigned int chan_flags, int16_t *nfBuf);

    /**
     * callback to get Rx noise Floor offset values for all chains.
     * Those offset values are flashed into EEPROM/OTP/Flash during the calibration time.
     * @param dev : handle to LMAC device
     * @param freq : the frequency of the channel
     * @param chan_flags : the channel flags of the channel
     * @param nf_pwr : pointer to buffer to store the NF offset pwr values
     * @param nf_cal : pointer to buffer to store the NF offset cal values
     */
    int        (*get_rx_nf_offset)(ath_dev_t dev, unsigned short freq, unsigned int chan_flags, int8_t *nf_pwr, int8_t *nf_cal);

    /**
     * callback to get the latest Rx signal strength values in dBm for all chains.
     * @param dev : handle to LMAC device
     * @param signal_dbm : pointer to buffer to store the RX signal strength values in dBm
     */
    int        (*get_rx_signal_dbm)(ath_dev_t dev, int8_t *signal_dbm);

#if ATH_SUPPORT_VOW_DCS
    /**
     * callback to disable dcs-im feature (dynamic channel selection for interfernce mitigation).
     * @param dev : handle to LMAC device
     */
	void        (*disable_dcsim)(ath_dev_t dev);
    /**
     * callback to enable dcs-im feature (dynamic channel selection for interfernce mitigation).
     * @param dev : handle to LMAC device
     */
	void        (*enable_dcsim)(ath_dev_t dev);
#endif

#if ATH_SUPPORT_RAW_ADC_CAPTURE
    /**
     * callback to get the spectral raw adc support
     * @param dev : handle to LMAC device
     * @param freq : the frequency to check
     * @param gain_buf : the buffer to store return gain values
     * @param num_gain_buf : the size of the buffer
     * @return 0 indicates the raw adc support is valid; otherwise, no raw adc support and the values
     * in buffer are invalid
     */
    int         (*get_min_agc_gain)(ath_dev_t dev, unsigned short freq, int32_t *gain_buf, int num_gain_buf);
#endif

    /**
     * Update spatial multiplexing mode. Only works for HOSTAP and STA.
     * @param dev : handle to LMAC device
     * @param node : the node to update spatial multiplexing mode. Only valid for HOSTAP mode. The node
     * is the target client node to update
     * @param mode : spatial multiplexing mode to update. Possible values are define in ath_dev.h
     */
    void        (*ath_sm_pwrsave_update)(ath_dev_t, ath_node_t,
                                         ATH_SM_PWRSAV mode, int);

    /* Transmit rate */
    /**
     * callback to get TX rate of a node.
     * @param dev : handle to LMAC device
     * @param node : the target node to query transmit rate
     * @return the transmit rate (unit : kbps)
     */
    u_int32_t   (*node_gettxrate)(ath_node_t);
    void        (*node_setfixedrate)(ath_node_t, u_int8_t);

    /**
     * callback to get maximum PHY rate of a node
     * @param dev : handle to LMAC device
     * @param node : the target node to query maximum PHY rate
     * @return the maximum PHY rate (unit : kbps)
     */
    u_int32_t   (*node_getmaxphyrate)(ath_dev_t, ath_node_t);

    /* Rate control */
    /**
     * Update rate-control state on station associate/reassociate.
     * @param dev : handle to LMAC device
     * @param node : the node associated/reassociated
     * @param isnew : 1 indicates association; 0 indicates reassociation
     * @param capflag : capability flags of the association/reassocaition
     * @param negotiated_rates : pointer to the info structure of negotiated rates
     * @param negotiated_htrates : pointer the info structure of negotiated HT rates
     * @return always returns 0
     */
    int         (*ath_rate_newassoc)(ath_dev_t dev, ath_node_t , int isnew, unsigned int capflag,
                                     struct ieee80211_rateset *negotiated_rates,
                                     struct ieee80211_rateset *negotiated_htrates);

    /**
     * Update rate-control state on a device state change.  When
     * operating as a station this includes associate/reassociate
     * with an AP.  Otherwise this gets called, for example, when
     * the we transition to run state when operating as an AP.
     * @param dev : handle to LMAC device
     * @param if_id : the virtual interface to update state change
     * @param up : non-zero value will indicate the state change to UMAC layer
     */
    void        (*ath_rate_newstate)(ath_dev_t dev, int if_id, int up);

#ifdef DBG
    /* Debug feature: register access */
    /**
     * Callback to read a register. Only valid with DBG defined
     * @param dev : handle to LMAC device
     * @param offset : the register to read. Possible values are defined in arXXXreg.h
     * @return the value read from the register
     */
    u_int32_t   (*ath_register_read)(ath_dev_t, u_int32_t);

    /**
     * Callback to write a register. Only valid with DBG defined
     * @param dev : handle to LMAC device
     * @param offset : the register to write. Possible values are defined in arXXXreg.h
     * @param value : the value to write to the register
     */
    void        (*ath_register_write)(ath_dev_t, u_int32_t, u_int32_t);
#endif

    /* TX Power Limit */
    int         (*ath_set_txPwrAdjust)(ath_dev_t dev, int32_t adjust, u_int32_t is2GHz);
    /**
     * callback to set TX power limit. This routine makes the actual HAL calls to set the new transmit power
     *  limit.  This also calls back into the protocol layer setting the max
     *  transmit power limit.
     * @param dev : handle to LMAC device
     * @param limit : the TX power limit to set
     * @param tpcInDb : transmit power control (DB)
     * @param is2GHz : whether this call is for 2G or 5G
     * @return 0 on success; -EINVAL for the limit exceeding maximum TX power
     * limit for the specified band
     */
    int         (*ath_set_txPwrLimit)(ath_dev_t dev, u_int32_t limit, u_int16_t tpcInDb, u_int32_t is2GHz);

    /**
     * Get common power level of a specified frequency
     * @param dev : handle to LMAC device
     * @param freq : frequency to find common power level
     * @return the common power level for the frequency
     */
    u_int8_t    (*get_common_power)(u_int16_t freq);

    /* TSF 32/64 */
    /**
     * get the 32 bits TSF
     * @param dev : handle to LMAC device
     * @return value of 32 bits TSF
     */
    u_int32_t   (*ath_get_tsf32)(ath_dev_t dev);

    /**
     * get the 64 bits TSF
     * @param dev : handle to LMAC device
     * @return value of 64 bits TSF
     */
    u_int64_t   (*ath_get_tsf64)(ath_dev_t dev);
    /**
     * get the 64 bits TSF of locationing AP
     * @param dev : handle to LMAC device
     * @return value of 64 bits TSF of locationing AP
     */
#if ATH_SUPPORT_WIFIPOS
    u_int64_t   (*ath_get_tsftstamp)(ath_dev_t dev);
#endif
    /* Rx Filter */
    /**
     * calculate RX filter and set the filter to H/W
     * @param dev : handle to LMAC device
     */
    void        (*ath_set_rxfilter)(ath_dev_t dev, u_int32_t filter);
	/* Select PLCP header or EVM data */
    /**
     * callback to select to pass PLCP headr or EVM data
     * @param dev : handle to LMAC device
     * @param selEvm : flag to set to H/W. Non-zero value set the H/W register to pass
     * PLCP header or EVM data
     * @param justQuery : flag to indicate to query the old setting only. Will not set H/W register.
     * @return The old setting. 0 indicates passing PLCP header or EVM data
     */
    int         (*ath_set_sel_evm)(ath_dev_t dev, int selEvm, int justQuery);

    int         (*ath_get_mibCycleCounts)(ath_dev_t dev, HAL_COUNTERS *pCnts);
    /* MIB Control */
    /**
     * callback to update mib mac statistics. This will update hal internal stored
     * mac statistics variables by reading from hardware.
     * @param dev : handle to LMAC device
     * @return 0 always.
     */
    int         (*ath_update_mibMacstats)(ath_dev_t dev);
    /**
     * callback to query mib MAC statistics data from hal internal stored variables.
     * @param dev : handle to LMAC device
     * @param pStat : pointer to mib mac statistics structure.
     * @return 0 on success. negetive value on failure(illegal input pointer).
     */
    int         (*ath_get_mibMacstats)(ath_dev_t dev, struct ath_mib_mac_stats *pStats);
    /**
     * callback to clear MIB cycle counts.
     * @param dev : handle to LMAC device
     */
    void        (*ath_clear_mibCounters)(ath_dev_t dev);
#ifdef ATH_CCX
    /**
     * callback to get packet error rate % from a rate value
     * @param dev : handle to LMAC device
     * @param node : per for the node
     * @param rateKbps : Rate value in kbps
     * @return packet error rate % (0-100)
     */
    u_int8_t    (*rcRateValueToPer)(ath_dev_t, struct ath_node *, int);

    /* Get Serial Number */
    /**
     * callback to query Manufacturer Serial number copied from hal internal buff to input buffer
     * @param dev : handle to LMAC device
     * @param pSn : pointer to serial number buffer
     * @param limit : limit indicates the number of bytes to limit copy of bytes into pSn buffer
     * @return number of bytes truncated. If input buff is bigger than hal ser num buff, then
     * will be zero. If less, then we return number of bytes truncated.
     */
    int         (*ath_get_sernum)(ath_dev_t dev, u_int8_t *pSn, int limit);

    /* Get Channel Data */
    /**
     * callback to query Channel data like clock rate, noise floor etc
     * @param dev : handle to LMAC device
     * @param pChan : pointer to channel information
     * @param pData : pointer to channel data buffer
     * @return 0 on SUCCESS; Non-zero value indicates failure (invalid pointer)
     */
    int         (*ath_get_chandata)(ath_dev_t dev, struct ieee80211_ath_channel *pChan, struct ath_chan_data *pData);

    /* Get Current RSSI */
    /**
     * callback to query RSSI of the frame currently being received
     * @param dev : handle to LMAC device
     * @return absolute RSSI value
     */
    u_int32_t   (*ath_get_curRSSI)(ath_dev_t dev);
#endif // ATH_CCX
#if ATH_GEN_RANDOMNESS
    /* Get RSSI from ctl Chain0 */
    /**
     * callback to query RSSI from BB on ctl chain0
     * @param dev : handle to LMAC device
     * @return absolute RSSI value
     */
    u_int32_t   (*ath_get_rssi_chain0)(ath_dev_t dev);
#endif
    /* A-MSDU frame */
    /**
     * callback to query whether a TID has A-MSDU support
     * @param dev : handle to LMAC device
     * @param node : the node the TID associated with
     * @param tidno : the TID number
     * @return 0 indicates no A-MSDU support; Non-zero value indicates A-MSDU support
     */
    int         (*get_amsdusupported)(ath_dev_t, ath_node_t, int);

#ifdef ATH_SWRETRY
    /**
     * Interface function for the IEEE layer to manipulate
     * the software retry state. Used during BMISS and
     * scanning state machine in IEEE layer
     * @param dev : handle to LMAC device
     * @param node : the node to enable/disable software retry
     * @param flag : 1 on enable; 0 on disable
     */
    void        (*set_swretrystate)(ath_dev_t dev, ath_node_t node, int flag);
    int         (*ath_handle_pspoll)(ath_dev_t dev, ath_node_t node);
    /* Check whether there is pending frames in tid queue */
    int         (*get_exist_pendingfrm_tidq)(ath_dev_t dev, ath_node_t node);
    int         (*reset_paused_tid)(ath_dev_t dev, ath_node_t node);
#endif
#ifdef ATH_SUPPORT_UAPSD
    /* UAPSD */
    /**
     * This function will send UAPSD frames out to a destination node.
     * should be called when a trigger frame is received from station.
     * the implementation of the fuction will queue a set of UAPSD frames from
     * per node uapsd queue to the UAPSD HW queue.
     * if the per node uapsd queue is empty then it will create a qos null with EOSP bit set
     * and queue it to the UAPSD HW queue.
     * Context: Interrupt
     * @param dev   : handle to LMAC device
     * @param node  : the node to send UAPSD frames to
     * @param maxsp : UAPSD frame bust count (if equal to WME_UAPSD_NODE_MAXQDEPTH then burst every thing queued)
     * @param ac    : access category (uses to determine the rate and set the ac in qos null).
     * @param flush : flag to indicate whether this is a flush operation.
     * @return the UAPSD queue depth after processing
     */
    int         (*process_uapsd_trigger)(ath_dev_t dev, ath_node_t node, u_int8_t maxsp,
                                         u_int8_t ac, u_int8_t flush, bool *sent_eosp, u_int8_t maxqdepth);

    /**
     * Returns number for UAPSD frames queued in software for a given node.
     * Context: Tasklet
     * @param dev : handle to LMAC device
     * @param node : the node the UAPSD queue associated with
     * @return the depth of the UAPSD queue
     */
    u_int32_t   (*uapsd_depth)(ath_node_t node);
#endif
#ifndef REMOVE_PKT_LOG
    /**
     * callback to start packet logging
     * @param dev : handle to LMAC device
     * @param log_state : packet logging option flags. Possible flags are defined in Pktlog_fmt.h
     * @return 0 on success; -ENOMEM on failing to allocate buffer for packet logging.
     */
    int         (*pktlog_start)(void *scn, int log_state);

    /**
     * callback to read the header from the current packet logging buffer.
     * @param dev : handle to LMAC device
     * @param buf : the pointer to the buffer to hold the return header
     * @param buf_len : the size of buf
     * @param required_len : pointer to a variable that holds the buffer length required
     * Valid when the callback fails.
     * @param actual_len : pointer to a variable that holds the length of the returned header
     * @return 0 on success; -EINVAL for the case that the buffer size is smaller than
     * the required length to hold the returned header. One can use required_len to know how
     * much buffer size is required.
     */
    int         (*pktlog_read_hdr)(ath_dev_t, void *buf, u_int32_t buf_len,
                                   u_int32_t *required_len,
                                   u_int32_t *actual_len);

    /**
     * callback to get the content from the current packet logging buffer
     * @param dev : handle to LMAC device
     * @param buf : the pointer to the buffer the hold the content from packet logging buffer
     * @param buf_len : the size of buf
     * @param required_len : pointer to a variable that holds the buffer length required
     * Valid when the callback fails.
     * @param actual_len : pointer to a variable that holds the length of the content
     * @return 0 on success; -EINVAL for the case that the buffer size is smaller than the required
     * length to hold the content. One can use required_len to know how much buffer size required.
     */
    int         (*pktlog_read_buf)(ath_dev_t, void *buf, u_int32_t buf_len,
                                   u_int32_t *required_len,
                                   u_int32_t *actual_len);
#endif
#if ATH_SUPPORT_IQUE
    /* Set and Get AC and Rate Control parameters */
	/**
	 * callback to set the rate control parameters for specific AC.
	 * @param dev	: handle to LMAC device
	 * @param ac	: access category, BE(0), BK(1), VI(2), VO(3).
	 * @param use_rts: Use RTS for this ac or not.
	 * @param aggrsize_scaling: The scaling factor to divide the 4ms aggregated airtime
 	 * to several equivalent parts. This allows for aggregates to occupy a configurable
	 * airtime in steps of 4ms (with value 0), 2ms (with value 1),
	 * 1ms (with value 2), 0.5ms (with value 3).
	 * @param min_kbps: the lower threshold of what rates can be picked up by the
	 * "rate find" algorithm, to make sure that we shall not pick rates that cannot
	 * suppor the VI/VO application bit rates.
	 */
    void        (*ath_set_acparams)(ath_dev_t, u_int8_t ac, u_int8_t use_rts,
                                    u_int8_t aggrsize_scaling, u_int32_t min_kbps);

	/**
	 * callback to set the rate control parameters for specific rate table.
	 * @param dev	: handle to LMAC device
	 * @param rt_index: Index to the rate table, 0 for BE/BK traffic,
	 * and 1 for VI/VO traffic.
	 * @param perThresh: If the PER value for a rate is above this threshold, then
	 * this rate will not be included in the rate series.
	 * @param probeInterval: The time interval in ms with which we shall probe.
	 */
    void        (*ath_set_rtparams)(ath_dev_t, u_int8_t rt_index, u_int8_t perThresh,
                                    u_int8_t probeInterval);
	/**
	 * callback to print the IQUE configuration.
	 * @param	dev	: handle to LMAC device.
	 */
    void        (*ath_get_iqueconfig)(ath_dev_t);

	/**
	 * callback to set the parameters for headline block removal (HBR).
	 * @param	dev	: handle to LMAC device
	 * @param	ac	: access category
	 * @param	enable:	Enbale HBR for this ac or not.
	 * @param	per:	the Low PER threshold with which to trigger HBR.
	 */
    void        (*ath_set_hbrparams)(ath_dev_t, u_int8_t ac, u_int8_t enable, u_int8_t per);
#endif
    /**
     * callback for power consumption WAR. It will re-program the PCI powersave regs. For STA,
     * this callback will keep the power consumption minimum when loaded first time and we are
     * in OFF state by triggering a hardware reset.
     * @param dev : handle to LMAC device
     * @param channel : channel for reset
     * @param channelflags : channel flags for reset
     * @param stamode : Non-zero indicates operating on STA mode
     */
    void        (*do_pwr_workaround)(ath_dev_t dev, u_int16_t channel, u_int32_t channelflags, u_int16_t stamode);

    /**
     * callback to get the property of a TX queue
     * @param dev : handle to LMAC device
     * @param q_id : the ID of the target queue
     * @param property : the property to query. Possible values are TXQ_PROP_SHORT_RETRY_LIMIT
     * and TXQ_PROP_LONG_RETRY_LIMIT
     * @param retval : pointer to the buffer that holds the return value
     */
    void        (*get_txqproperty)(ath_dev_t dev, u_int32_t q_id, u_int32_t property, void *retval);

    /**
     * callback to set the property of a TX queue
     * @param dev : handle to LMAC device
     * @param q_id : the ID of the target queue
     * @param property : the proterty to set. Possible values are TXQ_PROP_SHORT_RETRY_LIMIT
     * and TXQ_PROP_LONG_RETRY_LIMIT
     * @param value : pointer to the buffer that holds the value to set
     */
    void        (*set_txqproperty)(ath_dev_t dev, u_int32_t q_id, u_int32_t property, void *value);
    void        (*update_txqproperty)(ath_dev_t dev, u_int32_t q_id, u_int32_t property, void *value);

    /**
     * callback to get H/W revision. It is depreciated.
     * @param dev : handle to LMAC device
     * @param hwrev : pointer to buffer to hold the return value
     */
    void        (*get_hwrevs)(ath_dev_t dev, struct ATH_HW_REVISION *hwrev);

    /**
     * This routine is called to map the baseIndex to the rate in the RATE_TABLE
     * @param dev : handle to LMAC device
     * @param curmode : the current wireless mode. Possible values are defined in WIRELESS_MODE
     * @param isratecode : Flag for the type of return value. If it is true, the return value is rate code
     * that is used to set to H/W descriptor; otherwise, the return value is kbps rate
     * @return the rate where the baseIndex maps to in rate table
     */
    u_int32_t   (*rc_rate_maprix)(ath_dev_t dev, u_int16_t curmode, int isratecode);

    /**
     * callback to set multicast rate to rate control module. Currently it will only check the multicast
     * rate is valid or not.
     * @param dev : handle to LMAC device
     * @param req_rate : the multicast rate to set. It will not set the multicast rate for
     * current implementation.
     * @return -1 on invalid multicast rate; otherwise, the valid multicast rate.
     */
    int         (*rc_rate_set_mcast_rate)(ath_dev_t dev, u_int32_t req_rate);

    /**
     * Call into the HAL to set the default antenna to use.  Not really valid for
     *  MIMO technology.
     * @param dev : handle to LMAC device
     * @param antenna : antenna Index of antenna to select
     */
    void        (*set_defaultantenna)(ath_dev_t dev, u_int antenna);

    /**
     * callback to set diversity to HAL
     * @param dev : handle to LMAC device
     * @param diversity : the diversity to set
     */
    void        (*set_diversity)(ath_dev_t dev, u_int diversity);

    /**
     * callback to set RX chain mask
     * @param dev : handle to LMAC device
     * @param mask : chain mask to set
     */
    void        (*set_rx_chainmask)(ath_dev_t dev, u_int8_t mask);

  /**
     * callback to set TX chain mask
     * @param dev : handle to LMAC device
     * @param mask : chain mask to set
     */
    void        (*set_tx_chainmask)(ath_dev_t dev, u_int8_t mask);

    /**
     * callback to set RX chain mask in legacy mode
     * @param dev : handle to LMAC device
     * @param mask : chain mask to set
     */
    void        (*set_rx_chainmasklegacy)(ath_dev_t dev, u_int8_t mask);

    /**
     * callback to set TX chain mask in legacy mode
     * @param dev : handle to LMAC device
     * @param mask : chain mask to set
     */
    void        (*set_tx_chainmasklegacy)(ath_dev_t dev, u_int8_t mask);

    /**
     * callback to set optional TX chain mask
     * @param dev : handle to LMAC device
     * @param mask : chain mask to set
     */
    void        (*set_tx_chainmaskopt)(ath_dev_t dev, u_int8_t mask);

    /**
     * callback to get the current max TX power
     * @param dev : handle to LMAC device
     * @param twpow : pointer to buffer to save the return TX power value
     */
    void        (*get_maxtxpower) (ath_dev_t dev, u_int32_t *txpow);

    /**
     * callback to read information in EEPROM
     * @param dev : handle to LMAC device
     * @param address : The offset address in EEPROM to read
     * @param len : The length to read. The size should be 2 (sizeof(u_int16_t))
     * @param value : double pointer to the buffer to store the return value
     * @param bytesread : pointer to the buffer to store the size of the return value
     */
    void        (*ath_eeprom_read)(ath_dev_t dev, u_int16_t address, u_int32_t len, u_int16_t **value, u_int32_t *bytesread);

    /**
     * callback to log a text string format to packet log
     * @param dev : handle to LMAC device
     * @param env_flags : flags used to evaluate how to get packet logging buffer
     * @param fmt : the text format to log
     */
    void        (*log_text_fmt)(ath_dev_t dev,  u_int32_t env_flags, const char *fmt, ...);

    /**
     * callback to log a text string to packet log
     * @param dev : handle to LMAC device
     * @param text : a C string to log in packet log
     */
    void        (*log_text)(ath_dev_t dev, const char *text);
#if AR_DEBUG
    /**
     * callback to print per node TID info. Only valid when AR_DEBUG is on
     * @param dev : handle to LMAC device
     * @param sc : pointer to LMAC data structure
     * @param node : the target node to query
     */
    void        (*node_queue_stats) (struct ath_softc *sc , ath_node_t node);
#endif
    int         (*node_queue_depth) (struct ath_softc *sc , ath_node_t node);
    /**
     * Update rate-control state on a device state change.  When
     * operating as a station this includes associate/reassociate
     * with an AP.  Otherwise this gets called, for example, when
     * the we transition to run state when operating as an AP.
     * @param dev : handle to LMAC device
     * @param if_idx : the index of the VAP to set
     * @param up : If the device transitions to run state. Will notify protocol layer if it is true
     * @return always return 0
     */
    int         (*rate_newstate_set) (ath_dev_t dev, int if_idx, int up);

    /**
     * callback to get the rate code with data rate
     * @param dev : handle to LMAC device
     * @param rate : the data rate to query its rate code
     * @return 0 indicates not found; otherwise, the rate code of the data rate
     */
    u_int8_t    (*rate_findrc) (ath_dev_t dev, u_int32_t rate);

    /**
     * callback to print the values of registers
     * @param dev : handle to LMAC device
     * @param printctrl : flags to control what registers to print out. Possible flags are
     * HAL_DIAG_PRINT_REG_COUNTER and HAL_DIAG_PRINT_REG_ALL
     */
    void        (*ath_printreg)(ath_dev_t dev, u_int32_t printctrl);
    /* Get MFP support level */
    /**
     * callback to get MFP support level
     * @param dev : handle to LMAC device
     * @return MFP support level. Possible values are defined in HAL_MFP_OPT_T
     */
    u_int32_t   (*ath_get_mfpsupport)(ath_dev_t dev);

    /**
     * callback to set RX timeout value
     * @param dev : handle to LMAC device
     * @param ac : the access category to set RX timeout value
     * @param rxtimeout : the timeout value to set
     */
    void        (*ath_set_rxtimeout)(ath_dev_t, u_int8_t ac, u_int8_t rxtimeout);

#ifdef ATH_USB
    /**
     * callback to suspend USB feature. Only valid for USB device
     * @param dev : handle to LMAC device
     */
    void        (*ath_usb_suspend)(ath_dev_t);                /*ath_usb_suspend*/

    /**
     * callback to cleanup USB RX buffers. Only valid for USB device
     * @param dev : handle to LMAC device
     */
    void        (*ath_usb_rx_cleanup)(ath_dev_t);             /*ath_usb_rx_cleanup*/
#endif
#if ATH_SUPPORT_WIRESHARK
    /**
     * Ask the HAL to copy rx status info from the PHY frame descriptor
     * into the packet's radiotap header. Only vaild with ATH_SUPPORT_WIRESHARK
     * turned on
     * @param dev : handle to LMAC device
     * @param rh : pointer to radiotop header buffer
     * @param ppi : PPI data
     * @param wbuf : the RX buffer to get rx status info from
     */
    void        (*ath_fill_radiotap_hdr)(ath_dev_t dev,
                                         struct ah_rx_radiotap_header *rh, struct ah_ppi_data *ppi,
                                         wbuf_t wbuf);

    /**
     * callback to update the PhyErr filter value. Only valid with ATH_SUPPORT_WIRESHARK
     * turned on
     * @param dev : handle to LMAC device
     * @param filterPhyErr :  the new value to set to PhyErr filter. If it is zero, it will not be set to
     * PhyErr filter even if justQuery is zero.
     * @param justQuery : If non-zero value, the filterPhyErr will not be set to filter
     * @return the old value of PhyErr filter
     */
    int         (*ath_monitor_filter_phyerr)(ath_dev_t dev, int filterPhyErr, int justQuery);
#endif
#ifdef ATH_BT_COEX
    /**
     * callback to inform ath_bt module to change stomp type based on the PS type and current scheme.
     * @param dev : handle to LMAC device
     * @param type :  the PS type
     * @return 1 if successed
     */
    u_int32_t   (*bt_coex_ps_complete) (ath_dev_t dev, u_int32_t type);
    /**
     * callback to inform other module the BT WLAN coexistence information.
     * @param dev : handle to LMAC device
     * @param type :  the information type
     * @param : pointer to info structure
     * @return True if successed
     */
    int         (*bt_coex_get_info) (ath_dev_t dev, u_int32_t type, void *);
    /**
     * callback to inform ath_bt module to BT and WLAN event that may effect coexistence state and scheme.
     * @param dev : handle to LMAC device
     * @param coexevent :  the current BT or WLAN event
     * @param param : the pointer to the information structure provided with the event
     * @return 1 if successed
     */
    int         (*bt_coex_event) (ath_dev_t dev, u_int32_t coexevent, void *param);
	u_int32_t   (*ath_btcoex_info) (ath_dev_t dev,u_int32_t reg);
    u_int32_t   (*ath_coex_wlan_info) (ath_dev_t dev,u_int32_t reg,u_int32_t bOn);
#endif
#if QCA_AIRTIME_FAIRNESS
    /* Airtime Fairness */
    void        (*ath_atf_node_unblock)(ath_dev_t dev, ath_node_t node);
    void        (*ath_atf_set_clear)(ath_dev_t dev, u_int8_t enable_disable);
    u_int8_t    (*ath_atf_tokens_used)(ath_node_t node);
    void        (*ath_atf_node_resume)(ath_node_t node);
    void        (*ath_atf_tokens_unassigned)(ath_dev_t dev, u_int32_t airtime_unassigned);
    void        (*ath_atf_capable_node)(ath_node_t node, u_int8_t val, u_int8_t atfstate_change);
    u_int32_t   (*ath_atf_airtime_estimate)(ath_dev_t dev, ath_node_t node, u_int32_t tput, u_int32_t *possible_tput);
    u_int32_t    (*ath_atf_debug_nodestate)(ath_node_t node, struct atf_peerstate *peerstate);
#endif

    /**
     * callback to get the current noise floor setting from H/W
     * @param dev : handle to LMAC device
     * @return current noise floor
     */
    int16_t     (*ath_dev_get_noisefloor)( ath_dev_t dev);
    void        (*ath_dev_get_chan_stats)(ath_dev_t dev, void *chan_stats);

    /**
     * callback to control spectral functionalities
     * @param dev : handle to LMAC device
     * @param id : the spectral item to control. Possible values are defined in
     * spectral_raw_adc_ioctl.h (with RAW ADC CAPTURE support) and spectral_ioctl.h
     * @param indata : pointer to buffer that holds input arguments
     * @param insize : the size of the buffer indata points to
     * @param outdata : pointer to buffer that holds return results
     * @param outsize : the size of the buffer outdata points to
     * @return 0 on success; otherwise, failed to do spectral I/O control
     * possible values for id :
	 * SPECTRAL_SET_CONFIG
	 *  Description: Set FFT period, scan count, scan period, short report.
	 *  Indata: struct spectral_ioctl_params
	 * SPECTRAL_GET_CONFIG
	 *  Description: Return FFT period, scan count, scan period, short report.
	 *  Outdata: struct spectral_ioctl_params
	 * SPECTRAL_IS_ACTIVE
	 *  Description: Whether spectral is active or not
	 *  Outdata: 1: active 0: inactive
	 * SPECTRAL_IS_ENABLED
	 *  Description: Whether spectral is enabled or not
	 *  Outdata: 1: enabled 0: disabled
	 * SPECTRAL_SET_DEBUG_LEVEL:
	 *  Description: Set spectral debug level
	 *  Indata: 0x100(level 1), 0x200(level 2), 0x400(level 3), 0x800(level 4)
	 * SPECTRAL_CLASSIFY_SCAN:
	 *  Description: Begin a classification scan
	 * SPECTRAL_STOP_SCAN:
	 *  Description: Stop current scan
	 * SPECTRAL_ACTIVATE_FULL_SCAN:
	 *  Description: Activate full scan
	 * SPECTRAL_STOP_FULL_SCAN:
	 *  Description: Stop full scan
	 * SPECTRAL_GET_DIAG_STATS:
	 *  Description: Get Diagnostic stats
	 */

#if ATH_PCIE_ERROR_MONITOR

    int         (*ath_start_pcie_error_monitor)(ath_dev_t dev,int b_auto_stop);
    int         (*ath_read_pcie_error_monitor)(ath_dev_t dev, void* pReadCounters);
    int         (*ath_stop_pcie_error_monitor)(ath_dev_t dev);

#endif //ATH_PCIE_ERROR_MONITOR

#if WLAN_SPECTRAL_ENABLE

	/**
     * Start spectral scan
     * @param dev : handle to LMAC device
     */
    void        (*ath_dev_start_spectral_scan)(ath_dev_t dev, u_int8_t priority);

	/**
     * Stop spectral scan
     * @param dev : handle to LMAC device
     */
    void        (*ath_dev_stop_spectral_scan)(ath_dev_t dev);

    /**
     * Record channel information
     * @param dev                 : Handle to LMAC device
     * @param chan_num            : Current channel number
     * @param are_chancnts_valid  : Whether channel counts are valid
     * @param scanend_clr_cnt     : Rx clear count at end of scan
     * @param scanstart_clr_cnt   : Rx clear count at start of scan
     * @param scanend_cycle_cnt   : Cycle count at end of scan
     * @param scanstart_cycle_cnt : Cycle count at start of scan
     * @param is_nf_valid         : Whether Noise Floor (NF) passed is valid.
     *                              The function will record NF by itself if
     *                              the caller doesn't pass NF. This mechanism
     *                              is required since the Spectral module can
     *                              make a direct determination of NF without
     *                              the caller's assistance in the case of
     *                              Direct Attach, while for 11ac Offload, the
     *                              caller needs to pass NF derived from means
     *                              such as Channel Event.
     *                              XXX Desirable to remove the asymmetry to
     *                              some extent by having a common set of calls.
     * @param nf                  : Noise Floor (if valid).
     * @param is_per_valid        : Whether PER passed is valid.
     * @param per                 : PER (if valid).
     */
    void        (*ath_dev_record_chan_info)(ath_dev_t dev,
                                            u_int16_t chan_num,
                                            bool are_chancnts_valid,
                                            u_int32_t scanend_clr_cnt,
                                            u_int32_t scanstart_clr_cnt,
                                            u_int32_t scanend_cycle_cnt,
                                            u_int32_t scanstart_cycle_cnt,
                                            bool is_nf_valid,
                                            int16_t nf,
                                            bool is_per_valid,
                                            u_int32_t per);
    u_int32_t (*ath_init_spectral_ops)(struct ath_spectral_ops* p_sops);
#endif
    int         (*set_proxysta)(ath_dev_t dev, int enable);

#ifdef ATH_GEN_TIMER
    /**
     *  Allocates a hardware timer based on the TSF clock to be used
     *            by the UMAC virtual timer.
     *  @param dev             : handle to the ath device object,
     *  @param tsf_id          : type of the timer clock,
     *  @param trigger_actioni : function to be called by the timer module when the
     *                           timer triggers,
     *  @param overflow_action :   function to be called by the timer module when the
     *                             TSF value is changed to a value beyond the next
     *                             trigger point,
     *  @param outofrange_action : function to be called by the timer module when the
     *                             TSF value is changes by more than a certain threshold
     *                            (5ms),
     *  @param arg              : pointer to context to be provided by the timer module when invoking
     *                            the callback functions above.
     *  @returns handle to a hardware timer
     */
    struct ath_gen_timer *
                (*ath_tsf_timer_alloc)(ath_dev_t dev, int tsf_id,
                                        ath_hwtimer_function trigger_action,
                                        ath_hwtimer_function overflow_action,
                                        ath_hwtimer_function outofrange_action,
                                        void *arg);
    /**
     *   Free a hardware timer.
     *  @param dev   : handle to the ath device object,
     *  @param timer : handle to a hardware timer returned by a previous call to ath_tsf_timer_alloc.
     */
    void        (*ath_tsf_timer_free)(ath_dev_t dev, struct ath_gen_timer *timer);

    /**
     *  Start a hardware timer.
     *  @param dev        : handle to the ath device object,
     *  @param timer      : handle to a hardware timer returned by a previous call to ath_tsf_timer_alloc.
     *  @param timer_next : absolute TSF value when the timer should trigger,
     *  @param period     : if non-zero, indicates the timer should trigger periodically after timer_next
     */
    void        (*ath_tsf_timer_start)(ath_dev_t dev, struct ath_gen_timer *timer,
                                        u_int32_t timer_next, u_int32_t period);
    /**
     * Stop a hardware timer.
     *  @param dev        : handle to the ath device object,
     *  @param timer      : handle to a hardware timer returned by a previous call to ath_tsf_timer_alloc.
     */
    void        (*ath_tsf_timer_stop)(ath_dev_t dev, struct ath_gen_timer *timer);
#endif	//ifdef ATH_GEN_TIMER

#ifdef ATH_RFKILL
    /**
     *  Enable/disable RF kill polling, which is used for hardware
     *  that does not generate an interrupt when power is turned on/off.
     *  @param dev        : handle to the ath device object,
     *  @parma enable     : flag indicating whether to enable or disable RF Kill polling.
     */
    void        (*enableRFKillPolling)(ath_dev_t dev, int enable);
#endif	//ifdef ATH_RFKILL
#if ATH_SLOW_ANT_DIV
    /**
     * Suspend slow antenna diversity and switch to default antenna
     * @param dev : handle to LMAC device
     */
    void        (*ath_antenna_diversity_suspend)(ath_dev_t dev);

    /**
     * Resume slow antenna diversity if it has been suspended
     * @param dev : handle to LMAC device
     */
    void        (*ath_antenna_diversity_resume)(ath_dev_t dev);
#endif  //if ATH_SLOW_ANT_DIV
    /**
     * callback to register the "notify TX beacon event" callback
     * @param dev      : handle to LMAC device
     * @param callback : the callback function when "TX beacon event" happened
     * @param arg      : the argument for the callback
     * @return 0 on success; -1 indicates the notify_tx_bcn is not initialized
     */
    int         (*ath_reg_notify_tx_bcn)(ath_dev_t dev,
                                         ath_notify_tx_beacon_function callback,
                                         void *arg);

    /**
     * callback to De-register the "notify TX beacon event" callback
     * @param dev    : handle to LMAC device
     * @return 0 on success; -1 if unsuccessful. 1 if success but callback in progress.
     * @Note This routine may be called multiple times to check if the callback is
     * still "in progress".
     */
    int         (*ath_dereg_notify_tx_bcn)(ath_dev_t dev);

    /**
     * callback to register a notification when the specified ATH information of a VAP changes
     * @param dev           : handle to LMAC device
     * @param vap_if_id     : the index of the VAP
     * @param infotype_mask : The ATH information. Current implementation only supports
     * ATH_VAP_INFOTYPE_SLOT_OFFSET
     * @param callback      : the callback to execute when the ATH information changes
     * @param arg           : the argument for the callback
     * @return 0 on success; -EINVAL indicates either the VAP index is invalid, avp_vap_info not
     * initialized or callback already reigstered.
     */
    int         (*ath_reg_notify_vap_info)(ath_dev_t dev,
                                             int vap_if_id,
                                             ath_vap_infotype infotype_mask,
                                             ath_vap_info_notify_func callback,
                                             void *arg);

    /**
     * callback to update the list of interested ATH information of a VAP for callback. Must already register
     * notification before calling this update callback.
     * @param dev           : handle to LMAC device
     * @param vap_if_id     : the index of the VAP
     * @param infotype_mask : The ATH information. Current implementation only supports
     * ATH_VAP_INFOTYPE_SLOT_OFFSET
     * @return 0 on success; -EINVAL indicates either the VAP index is invalid or avp_vap_info not initialized
     */
    int         (*ath_vap_info_update_notify)(ath_dev_t dev,
                                              int vap_if_id,
                                              ath_vap_infotype infotype_mask);

    /**
     * callback to De-register the VAP information changed callback
     * @param dev       : handle to LMAC device
     * @param vap_if_id : the index of the VAP
     * @return Return 0 if successful. -1 if unsuccessful. 1 if success but callback in progress.
     * Note: This routine may be called multiple times to check if the callback is
     * still "in progress".
     */
    int         (*ath_dereg_notify_vap_info)(ath_dev_t dev, int vap_if_id);

    /**
     * callback to get the current information about the VAP
     * @param dev       : handle to LMAC device
     * @param vap_if_id : the index of the VAP
     * @param type      : the ATH info to get. Current implementation only supports
     * ATH_VAP_INFOTYPE_SLOT_OFFSET
     * @param param1    : pointer to buffer to store the return value
     * @param param2    : pointer to buffer to store the 2nd return value. Current implementation
     * does not check/use this pointer
     * @retun 0 on success; -EINVAL indicates either the VAP index is invalid or the type is not supported
     */
    int         (*ath_vap_info_get)(ath_dev_t dev, int vap_if_id, ath_vap_infotype type,
                                   u_int32_t *param1, u_int32_t *param2);

    /**
     * pause/unpause dat trafic on HW queues for the given vap(s).
     * @param dev   : handle to LMAC device
     * @param if_id : the index of the VAP. If vap is null the perform the requested operation on
     * all the vaps. If vap is non null the perform the requested operation on the vap.
     * @param pause :  if pause is true then perform pause operation. If pause is false then
     * perform unpause operation.
     */
    void        (*ath_vap_pause_control)(ath_dev_t dev,int if_id, bool pause );

    /**
     * pause/unpause dat trafic on HW queues for the given node
     * @param dev   : handle to LMAC device
     * @param node  : the target node to pause/unpause
     * @param pause : If pause == true then pause the node (pause all the tids ).
     * If pause == false then unpause the node (unpause all the tids )
     */
    void        (*ath_node_pause_control)(ath_dev_t dev,ath_node_t node, bool pause);

    /**
     * callback to get goodput of a given node
     * @param dev  : handle to LMAC device
     * @param node : the node to get goodput
     * @return : goodput number (kbps)
     */
    u_int32_t   (*ath_get_goodput) ( ath_dev_t dev, ath_node_t node);
#ifdef  ATH_SUPPORT_TxBF
    /**
     * callback to get the TxBF capability information about the device
     * @param dev : handle to LMAC device
     * @param txbf_caps : TxBF capability information
     * @retun AH_TRUE if device supports TxBF; AH_FALSE indicates TxBF is not supported
     */
    int         (*get_txbf_caps)(ath_dev_t dev, ieee80211_txbf_caps_t **txbf_caps);

    /**
     * Allocate a slot in the key cache for TxBF node
     * @param dev : handle to LMAC device
     * @param mac : MAC address of TxBF node
     * @param keyix:  key index if key cache allocated;
     * @retun AH_TRUE key index allocated; AH_FALSE if fails to allocate key index.
     */
	HAL_BOOL   (*txbf_alloc_key)(ath_dev_t dev, u_int8_t *mac, u_int16_t *keyix);

    /**
     * Configure key cache for TxBF setting
     * @param dev : handle to LMAC device
     * @param keyidx : remote node index in in key cache
     * @param rx_staggered_sounding : stagger sounding of remote node
     * @param channel_estimation_cap : channel estimation capability of remote node
     * @param MMSS : MPDU density of remote node
     *   NO RESTRICTION  0
     *   1/4 us          1
     *   1/2 us          2
     *   1 us            3
     *   2 us            4
     *   4 us            5
     *   8 us            6
     *   16 us           7
     */
	void        (*txbf_set_key)(ath_dev_t dev, u_int16_t keyidx, u_int8_t rx_staggered_sounding,
                    u_int8_t channel_estimation_cap , u_int8_t MMSS);
    /**
     * Delete key cache allocated for TxBF
     * @param dev : handle to LMAC device
     * @param keyidx : remote node index in in key cache
     * @param freeslot : whether to free the key cache entry or not
     */
	void        (*txbf_del_key)(ath_dev_t dev, u_int16_t keyix, int freeslot);

    /**
     * Control HW/SW CV timer
     * @param dev : handle to LMAC device
     * @param opt : 0 to use SW timer, 1 to use HW timer, default is SW timer
     */
    void        (*txbf_set_hw_cvtimeout)(ath_dev_t dev, HAL_BOOL opt);

    /**
     * Dump CV cache registers, this is for debug only
     * @param dev : handle to LMAC device
     */
    void        (*txbf_print_cv_cache)(ath_dev_t dev);
    void        (*txbf_get_cvcache_nr)(ath_dev_t dev, u_int16_t keyidx, u_int8_t *nr);
    void        (*txbf_set_rpt_received)(ath_dev_t dev);
#endif
    void        (*set_rifs)(ath_dev_t dev, bool enable);
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
    /**
     * Collect the wanted statistics from lmac (load, medium utilization, etc.)
     * @param dev : handle to LMAC device
     * @param ald_data, ald_ni_data : the data structure to hold statistics
     */
    int         (*ald_collect_data)(ath_dev_t dev, ath_ald_t ald_data);
    /**
     * Collect the wanted statistics from lmac per link
     * @param dev : handle to LMAC device
     * @param ald_data, ald_ni_data : the data structure to hold statistics
     * @param ni:  the node index that the statitics for (different node has its own link stats);
     */
    int         (*ald_collect_ni_data)(ath_dev_t dev, ath_node_t an, ath_ald_t ald_data, ath_ni_ald_t ald_ni_data);
#endif
#if UMAC_SUPPORT_VI_DBG
    void         (*ath_set_vi_dbg_restart)(ath_dev_t dev);
    void         (*ath_set_vi_dbg_log)(ath_dev_t dev, bool enable);
#endif
    void         (*ath_cdd_control)(ath_dev_t dev, u_int32_t cddEnable);
    u_int16_t    (*ath_get_tid_seqno)(ath_dev_t dev, ath_node_t node,
                                      u_int8_t tidno);
	int8_t		 (*get_noderssi)(ath_node_t an, int8_t chain, u_int8_t flag);
	u_int32_t 	 (*get_noderate)(ath_node_t node, u_int8_t type);
    void        (*ath_set_hw_mfp_qos)(ath_dev_t dev, u_int32_t dot11w);
#if ATH_SUPPORT_FLOWMAC_MODULE
    void        (*netif_stop_queue)(ath_dev_t);
    void        (*netif_wake_queue)(ath_dev_t);
    void        (*netif_set_watchdog_timeout)(ath_dev_t, int);
    void        (*flowmac_pause)(ath_dev_t, int stall );
#endif
#if ATH_SUPPORT_IBSS_DFS
    void        (*ath_ibss_beacon_update_start)(ath_dev_t dev);
    void        (*ath_ibss_beacon_update_stop)(ath_dev_t dev);
#endif /* ATH_SUPPORT_IBSS_DFS */
#if ATH_ANT_DIV_COMB
    u_int32_t   (*smartAnt_query)(ath_dev_t, u_int32_t *);
    void        (*smartAnt_config)(ath_dev_t, u_int32_t *);
    void        (*ath_sa_normal_scan_handle)(ath_dev_t dev, u_int8_t);
    void        (*ath_set_lna_div_use_bt_ant)(ath_dev_t dev, bool enable);
#endif
    void        (*ath_set_bssid_mask)(ath_dev_t dev, const u_int8_t bssid_mask[IEEE80211_ADDR_LEN]);
    /**
     * query if hardware beacon processing is active
     * @param dev       : handle to LMAC device
     * @return Return 1 if active, otherwise 0.
     */
    int         (*get_hwbeaconproc_active)(ath_dev_t dev);

    /**
     * Enable hardware beacon rssi threshold notification
     * @param dev       : handle to LMAC device
     * @return Return 1 if active, otherwise 0.
     */
    void         (*hw_beacon_rssi_threshold_enable)(ath_dev_t dev, u_int32_t rssi_threshold);
    void         (*hw_beacon_rssi_threshold_disable)(ath_dev_t dev);
    void         (*conf_rx_intr_mit)(ath_dev_t dev,u_int32_t enable);
   /*
    * Total free buffer avaliable in common pool of buffers
    */
    u_int32_t    (*get_txbuf_free)(ath_dev_t);
#if ATH_TX_DUTY_CYCLE
    /*
     * Configure quiet time for throttle wireless transmission
     * in order to control power dissipation
     */
    int         (*tx_duty_cycle)(ath_dev_t, int active_pct, bool flag);
#endif
#if UNIFIED_SMARTANTENNA
    int (*smart_ant_enable) (ath_dev_t, int enable, int mode);
    void (*smart_ant_set_tx_antenna) (ath_node_t node, u_int32_t *antenna_array, u_int8_t num_rates);
    void (*smart_ant_set_tx_defaultantenna) (ath_node_t node, u_int32_t antenna);
    void (*smart_ant_set_training_info) (ath_node_t node, uint32_t *rate_array, uint32_t *antenna_array, uint32_t numpkts);
   void  (*smart_ant_prepare_rateset)(ath_dev_t, ath_node_t node ,void *prtset);
#endif

#if ATH_SUPPORT_WIFIPOS
    /* set and get channel */
    /**
     * callback for the protocol layer to change the current channel the AP is serving
     * This is the optimised channel change code.
     * @param dev : the handle to LMAC device
     * @param hchan : pointer to channel structure with new channel information
     * @param tx_timeout : the time the driver waits for TX DMA to stop. if 0 is used then the lmac uses a default timeout.
     * @param rx_timeout : the time the driver waits for RX DMA to stop. if 0 is used then the lmac uses a default timeout.
     */
    void        (*ath_lean_set_channel)(ath_dev_t, HAL_CHANNEL *, int tx_timeout, int rx_timeout, bool flush);
    /* restart the HW transmission when HW queue is empty and txq is non-empty.
     */
    void        (*ath_resched_txq)(ath_dev_t dev);
    bool        (*ath_disable_hwq)(ath_dev_t dev, u_int16_t mask);
    /* For reaping frames from txqs when we are changing channel.
     */
    void    (*ath_vap_reap_txqs)(ath_dev_t dev,int if_id);
    int         (*ath_get_channel_busy_info)(ath_dev_t dev, u_int32_t *rxclear_pct, u_int32_t *rxframe_pct, u_int32_t *txframe_pct);
#endif
    u_int64_t   (*get_tx_hw_retries)(ath_dev_t dev);
    u_int64_t   (*get_tx_hw_success)(ath_dev_t dev);
#if ATH_SUPPORT_FLOWMAC_MODULE
    int         (*get_flowmac_enabled_state)(ath_dev_t dev);
#endif
#if UMAC_SUPPORT_WNM
    void        (*set_beacon_config)(ath_dev_t dev, int reason, int if_id);
#endif
#if LMAC_SUPPORT_POWERSAVE_QUEUE
    u_int8_t    (*get_pwrsaveq_len)(ath_node_t node, u_int8_t frame_type);
    int         (*node_pwrsaveq_send)(ath_node_t node, u_int8_t frame_type);
    void        (*node_pwrsaveq_flush)(ath_node_t node);
    int         (*node_pwrsaveq_drain)(ath_node_t node);
    int         (*node_pwrsaveq_age)(ath_node_t node);
    void        (*node_pwrsaveq_get_info)(ath_node_t node, void *info);
    void        (*node_pwrsaveq_set_param)(ath_node_t node, u_int8_t param, u_int32_t val);
#endif
    u_int8_t*   (*get_tpc_tables)(ath_dev_t dev, u_int8_t *ratecount);
    u_int8_t    (*get_tx_chainmask)(struct ath_softc *sc);
#if ATH_SUPPORT_TIDSTUCK_WAR
   void         (*clear_rxtid)(ath_dev_t dev, ath_node_t node);
#endif
#if ATH_SUPPORT_KEYPLUMB_WAR
    void        (*save_halkey)(ath_node_t node, HAL_KEYVAL *hk, u_int16_t keyix, const u_int8_t *macaddr);
    int         (*checkandplumb_key)(ath_dev_t, ath_node_t node, u_int16_t keyix);
#endif
/* api to set and get noise detection related param used in channel hopping algo */
    void        (*set_noise_detection_param)(ath_dev_t dev, int cmd, int val);
    void        (*get_noise_detection_param)(ath_dev_t dev, int cmd, int *val);
#ifdef ATH_TX99_DIAG
    u_int8_t    (*tx99_ops)(ath_dev_t dev, int cmd, void *tx99_wcmd);
#endif
    u_int8_t    (*set_ctl_pwr)(ath_dev_t dev, u_int8_t *ctl_pwr_array, int rd, u_int8_t chainmask);
    bool        (*ant_switch_sel)(ath_dev_t dev, u_int8_t ops, u_int32_t *tbl1, u_int32_t *tbl2);
    /* set per node tx chainmask for LMAC internally generated frame */
    void        (*set_node_tx_chainmask)(ath_node_t node, u_int8_t chainmask);
    void        (*set_beacon_interval)(ath_dev_t arg, u_int16_t intval);
#if ATH_TxBF_DYNAMIC_LOF_ON_N_CHAIN_MASK
    void        (*txbf_loforceon_update)(ath_dev_t dev,bool loforcestate);
#endif
    void         (*node_pspoll)(ath_dev_t dev,ath_node_t node,bool value);
    int         (*get_vap_stats)(ath_dev_t dev, int if_id, struct ath_vap_dev_stats *pStats);
    u_int32_t   (*get_last_txpower)(ath_node_t node);
    bool        (*direct_rate_check)(ath_dev_t dev,int val);
    void        (*set_use_cac_prssi)(ath_dev_t);

    /* scan ops */
    int       (*ath_scan_sm_start)(ath_dev_t dev, void *params);
    int       (*ath_scan_cancel)(ath_dev_t  dev, void *req);
    void      (*ath_scan_sta_power_events)(ath_dev_t dev, int event_type, int event_status);
    void      (*ath_scan_resmgr_events)(ath_dev_t dev, int event_type, int event_status);
    void      (*ath_scan_connection_lost)(ath_dev_t dev);
    bool      (*ath_scan_get_sc_isscan)(ath_dev_t dev);
    int       (*ath_scan_set_channel_list)(ath_dev_t dev, void *params);
    bool      (*ath_scan_in_home_channel)(ath_dev_t dev);

#ifdef WLAN_SUPPORT_GREEN_AP
    QDF_STATUS (*ath_green_ap_ps_on_off)(ath_dev_t dev,
                                uint32_t value, uint8_t mac_id);
     QDF_STATUS (*ath_green_ap_reset_dev)(ath_dev_t dev);
     uint16_t (*ath_green_ap_get_current_channel)(ath_dev_t dev);
     uint64_t (*ath_green_ap_get_current_channel_flags)(ath_dev_t dev);
     QDF_STATUS (*ath_green_ap_get_capab)(ath_dev_t dev);
#endif
};

/* Load-time configuration for ATH device */
struct ath_reg_parm {
    u_int32_t            AthDebugLevel;
    u_int16_t            busConfig;                       /* PCI target retry and TRDY timeout */
    u_int8_t             networkAddress[6];               /* Number of map registers for DMA mapping */
    u_int16_t            calibrationTime;                 /* how often to do calibrations, in seconds (0=off) */
    u_int8_t             gpioPinFuncs[NUM_GPIO_FUNCS];    /* GPIO pin for each associated function */
    u_int8_t             gpioActHiFuncs[NUM_GPIO_FUNCS];  /* Set gpioPinFunc0 active high */
    u_int8_t             WiFiLEDEnable;                   /* Toggle WIFI recommended LED mode controlled by GPIO */
    u_int8_t             softLEDEnable;                   /* Enable AR5213 software LED control to work in WIFI LED mode */
    u_int8_t             swapDefaultLED;                  /* Enable default LED swap */
    u_int16_t            linkLedFunc;                     /* Led registry entry for setting the Link Led operation */
    u_int16_t            activityLedFunc;                 /* Led registry entry for setting the Activity Led operation */
    u_int16_t            connectionLedFunc;               /* Led registry entry for setting the Connection Led operation */
    u_int8_t             gpioLedCustom;                   /* Defines customer-specific blinking requirements (OWL) */
    u_int8_t             linkLEDDefaultOn;                /* State of link LED while not connected */
    u_int8_t             DisableLED01;                    /* LED_0 or LED_1 in PCICFG register is used for other purposes */
    u_int8_t             sharedGpioFunc[NUM_GPIO_FUNCS];  /* gpio pins may be shared with other devices */
    u_int16_t            diversityControl;                /* Enable/disable antenna diversity */
    u_int32_t            hwTxRetries;                     /* num of times hw retries the frame */
    u_int16_t            tpScale;                         /* Scaling factor to be applied to max transmit power */
    u_int8_t             extendedChanMode;                /* Extended Channel Mode flag */
    u_int16_t            overRideTxPower;                 /* Override of transmit power */
    u_int8_t             enableFCC3;                      /* TRUE only if card has been FCC3-certified */
    u_int32_t            DmaStopWaitTime;                 /* Overrides default dma wait time */
    u_int8_t             pciDetectEnable;                 /* Chipenabled. */
    u_int8_t             singleWriteKC;                   /* write key cache one word at time */
    u_int32_t            smbiosEnable;                    /* Bit 1 - enable device ID check, Bit 2 - enable smbios check */
    u_int8_t             userCoverageClass;               /* User defined coverage class */
    u_int16_t            pciCacheLineSize;                /* User cache line size setting */
    u_int16_t            pciCmdRegister;                  /* User command register setting */
    u_int32_t            regdmn;                          /* Regulatory domain code for private test */
    char                 countryName[4];                  /* country name */
    u_int32_t            wModeSelect;                     /* Software limiting of device capability for SKU reduction */
    u_int32_t            NetBand;                         /* User's choice of a, b, turbo, g, etc. from registry */
    u_int8_t             pcieDisableAspmOnRfWake;         /* Only use ASPM when RF Silenced */
    u_int8_t             pcieAspm;                        /* ASPM bit settings */
    u_int8_t             txAggrEnable;                    /* Enable Tx aggregation */
    u_int8_t             rxAggrEnable;                    /* Enable Rx aggregation */
    u_int8_t             txAmsduEnable;                   /* Enable Tx AMSDU */
    int                  aggrLimit;                       /* A-MPDU length limit */
    int                  aggrSubframes;                   /* A-MPDU subframe limit */
    u_int8_t             aggrProtEnable;                  /* Enable aggregation protection */
    u_int32_t            aggrProtDuration;                /* Aggregation protection duration */
    u_int32_t            aggrProtMax;                     /* Aggregation protection threshold */
    u_int8_t             txRifsEnable;                    /* Enable Tx RIFS */
    int                  rifsAggrDiv;                     /* RIFS aggregation divisor */
#if WLAN_SPECTRAL_ENABLE
    u_int8_t             spectralEnable;
#endif
    u_int8_t             txChainMask;                     /* Tx ChainMask */
    u_int8_t             rxChainMask;                     /* Rx ChainMask */
    u_int8_t             txChainMaskLegacy;               /* Tx ChainMask in legacy mode */
    u_int8_t             rxChainMaskLegacy;               /* Rx ChainMask in legacy mode */
    u_int8_t             rxChainDetect;                   /* Rx chain detection for Lenovo */
    u_int8_t             rxChainDetectRef;                /* Rx chain detection reference RSSI */
    u_int8_t             rxChainDetectThreshA;            /* Rx chain detection RSSI threshold in 5GHz */
    u_int8_t             rxChainDetectThreshG;            /* Rx chain detection RSSI threshold in 2GHz */
    u_int8_t             rxChainDetectDeltaA;             /* Rx chain detection RSSI delta in 5GHz */
    u_int8_t             rxChainDetectDeltaG;             /* Rx chain detection RSSI delta in 2GHz */
#if ATH_WOW
    u_int8_t             wowEnable;
#endif
    u_int32_t            shMemAllocRetry;                 /* Shared memory allocation no of retries */
    u_int8_t             forcePpmEnable;                  /* Force PPM */
    u_int16_t            forcePpmUpdateTimeout;           /* Force PPM window update interval in ms. */
    u_int8_t             enable2GHzHt40Cap;               /* Enable HT40 in 2GHz channels */
    u_int8_t             swBeaconProcess;                 /* Process received beacons in SW (vs HW) */
#ifdef ATH_RFKILL
    u_int8_t             disableRFKill;                   /* Disable RFKill */
#endif
    u_int8_t             rfKillDelayDetect;               /* Enable WAR for slow rise for RfKill on power resume */
#ifdef ATH_SWRETRY
    u_int8_t             numSwRetries;                    /* Num of sw retries to be done */
#endif
    u_int32_t            ant_div_low_rssi_cfg;            /* Non-zero: enabled low RSSI enhancement when rssi < (X) */
    u_int8_t             ant_div_fast_div_bias;           /* Non-zero: set fast_div_bias to (x) for all LNA combinations*/
    u_int8_t             showAntDivDbgInfo;                /* Enable slow antenna diversity debug print info*/
    u_int8_t             slowAntDivEnable;                /* Enable slow antenna diversity */
    int8_t               slowAntDivThreshold;             /* Rssi threshold for slow antenna diversity trigger */
    u_int32_t            slowAntDivMinDwellTime;          /* Minimum dwell time on the best antenna configuration */
    u_int32_t            slowAntDivSettleTime;            /* Time spent on probing antenna configurations */
    u_int8_t             stbcEnable;                      /* Enable STBC */
    u_int8_t             ldpcEnable;                      /* Enable LDPC */
#ifdef ATH_BT_COEX
    struct ath_bt_registry bt_reg;
#endif
    u_int8_t             cwmEnable;                       /* Enable Channel Width Management (CWM) */
    u_int8_t             wpsButtonGpio;                   /* GPIO associated with Push Button */
    u_int8_t             pciForceWake;                    /* PCI Force Wake */
    u_int8_t             paprdEnable;                     /* Enable PAPRD feature */
#if ATH_C3_WAR
    u_int32_t            c3WarTimerPeriod;
#endif
    u_int8_t             cddSupport;                      /* CDD - Cyclic Delay Diversity : Use Single chain for Ch 36/40/44/48 */
#ifdef ATH_SUPPORT_TxBF
    u_int32_t            TxBFSwCvTimeout;
#endif
#if ATH_TX_BUF_FLOW_CNTL
    u_int                ACBKMinfree;
    u_int                ACBEMinfree;
    u_int                ACVIMinfree;
    u_int                ACVOMinfree;
    u_int                CABMinfree;
    u_int                UAPSDMinfree;
#endif
#if ATH_SUPPORT_FLOWMAC_MODULE
    u_int8_t             osnetif_flowcntrl:1;               /* enable flowcontrol with os netif */
    u_int8_t             os_ethflowmac_enable:1;            /* enable interaction with flowmac module that forces PAUSE/RESUME on Ethernet */
    u_int32_t            os_flowmac_debug;
#endif
#if ATH_ANT_DIV_COMB
    u_int8_t             forceSmartAntenna;
    u_int8_t             saFixedMainLna;
    u_int8_t             saFixedAltLna;
    u_int8_t             saFastDivDisable;
    u_int8_t             saFixedSwitchComR;
    u_int8_t             saFixedSwitchComT;
    u_int8_t             lnaDivUseBtAntEnable;              /* Enable LNA Diversity use BT Antenna feature */
#endif
    u_int8_t             disable_ht20_sgi;                  /* disable HT20 shortGI */
    u_int8_t             rc_txrate_fast_drop_en;            /* enable tx rate fast drop*/
    u_int8_t             burstEnable;
    u_int32_t            burstDur;
    u_int8_t             burst_beacons:1;                /* burst or staggered beacons for multiple vaps */
};

/*
struct ath_vow_stats {
    u_int32_t   tx_frame_count;
    u_int32_t   rx_frame_count;
    u_int32_t   rx_clear_count;
    u_int32_t   cycle_count;
    u_int32_t   ext_cycle_count;
} HAL_VOWSTATS;
*/

/**
 * @Create and attach an ath_dev object
 * @param devid     : device id  of the atheros device identifying the chipset.
 * @param base_addr : usually  base address of the device address space for memory mapped bus.
 *                     for non memory  mapped bus ,it can be a opaque pointer and HAL os/bus specific
 *                     need to provide an implementation to access the register space of the chip from the opaque
 *                     pointer.
 * @param ieee      : handle to umac radio .
 * @param ieee_ops  : pointer to ieee_ops structure with all the required function pointers filled in
 *                    used by lmac to make specific call backs into umac.
 * @param osdev     : os/platform specific handle .will be opaque to lmac.
 * @param dev       : pointer for lmac to store device handle for lmac. this handle is passed down for all calls into lmac.
 * @param ops       : pointer for lmac to store a reference (pointer) to ath_ops structure.
 * @param ath_conf_parm : pointer to ath_reg_param structure filled with default values to  be used
 *                         for certain configuration in lmac.
 * @param hal_conf_parm : pointer to hal_reg_param structure filled with default values to  be used
 *                         for certain configuration in hal.
 *
 * @return 0 when success.
 */
int init_sc_params(struct ath_softc *sc,struct ath_hal *ah);
void init_sc_params_complete(struct ath_softc *sc,struct ath_hal *ah);
int ath_dev_attach(u_int16_t devid, void *base_addr,
                   ieee80211_handle_t ieee, struct ieee80211_ops *ieee_ops,
                   struct wlan_objmgr_pdev *sc_pdev, struct wlan_lmac_if_rx_ops *rx_ops,
                   osdev_t osdev,
                   ath_dev_t *dev, struct ath_ops **ops,
                   struct ath_reg_parm *ath_conf_parm,
                   struct hal_reg_parm *hal_conf_parm,
				   struct ath_hal *ah);

/**
 * @brief Free an ath_dev object
 */
void ath_dev_free(ath_dev_t);


typedef enum {
    ATH_GPIO_PIN_INPUT  = 0,
    ATH_GPIO_PIN_OUTPUT = 1
} ATH_GPIO_PIN_TYPE;

/**
 * @Config the GPIO pin type
 *
 * @param sc   : pinter to LMAC data structure
 * @param pin  : GPIO pin number
 * @param type : GPIO pin type in ATH_GPIO_PIN_TYPE
 *
 * @return true when configure successfully
 */
bool ath_gpio_config(struct ath_softc *sc, u_int pin, u_int type);

/**
 * @brief GPIO interrupt callback function prototype
 *
 * @param context : opaque pointer for caller context keeping
 * @param pin     : GPIO pin number
 * @param mask    : GPIO interrupt mask
 */
typedef int (*ath_gpio_intr_func_t)(void *context, unsigned int pin,
             unsigned int mask);

/**
 * @Register a callback function for GPIO Interrupts
 *
 * @param sc : pointer to LMAC data structure
 * @param mask : GPIO mask
 * @func : GPIO callback function
 * @context : opaque pointer passed back in the callback for context keeping
 */
int ath_gpio_register_callback(struct ath_softc *sc, unsigned int mask,
                               ath_gpio_intr_func_t func, void *context);

/**
 * @Get the GPIO interrupt
 *
 * @param sc : pointer to LMAC data structure
 *
 * @return the GPIO interrupt mask
 */
u_int32_t ath_gpio_get_intr(struct ath_softc *sc);


typedef enum {
    ATH_GPIO_INTR_LOW     = 0,
    ATH_GPIO_INTR_HIGH    = 1,
    ATH_GPIO_INTR_DISABLE = 2,
} ATH_GPIO_INTR_TYPE;

/**
 * @Set the GPIO interrupt
 *
 * @param sc     : pointer to LMAC data structure
 * @param pin    : GPIO pin number
 * @param ilevel : GPIO interrupt level in type ATH_GPIO_INTR_TYPE
 */
void ath_gpio_set_intr(struct ath_softc *sc, u_int pin, u_int32_t ilevel);

void ath_mgmt_tx_complete(struct ieee80211_node *ni, wbuf_t wbuf, int txok);
/**
 * @}
 */

#ifdef DEBUG
#define ATH_TRACE   printk("%s[%d]\n", __func__, __LINE__);
#else
#define ATH_TRACE
#endif /* DEBUG */

#endif /* ATH_DEV_H */
