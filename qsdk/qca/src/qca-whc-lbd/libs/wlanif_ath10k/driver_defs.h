// vim: set et sw=4 sts=4 cindent:
/*
 * @File: driver_defs.h
 *
 * @Abstract: Qualcomm Technologies Inc proprietary driver dependent components
 *
 * @Notes: This header is included to remove Qualcomm Technologies Inc driver dependency
 *         to support ath10k driver
 *
 * @@-COPYRIGHT-START-@@
 *
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * @@-COPYRIGHT-END-@@
 */

#ifndef wlanif_driver__h
#define wlanif_driver__h

#define IEEE80211_NWID_LEN 32
#define IEEE80211_BSTEERING_RRM_NUM_BCNRPT_MAX 8
#define BSTEERING_INVALID_RSSI 0
#define NETLINK_BAND_STEERING_EVENT 21
#define IEEE80211_ADDR_LEN 6
#define BSTEERING_MAX_PEERS_PER_EVENT 3
#define IEEE80211_RRM_MEASRPT_MODE_SUCCESS         0x00
#define MAC_ADDR_LEN 6
#define LTEU_MAX_BINS 10
#define MAX_SCAN_CHANS       32
#define IEEE80211_RATE_MAXSIZE 44
#define MAX_CHAINS 4
#define WME_NUM_AC 4

#define IEEE80211_RRM_NUM_CHANREQ_MAX 5
#define IEEE80211_RRM_NUM_CHANREP_MAX 2

#define IEEE80211_IOCTL_SETPARAM    (SIOCIWFIRSTPRIV+0)
#define IEEE80211_IOCTL_GETPARAM    (SIOCIWFIRSTPRIV+1)
#define IEEE80211_IOCTL_SETKEY      (SIOCIWFIRSTPRIV+2)
#define IEEE80211_IOCTL_SETWMMPARAMS    (SIOCIWFIRSTPRIV+3)
#define IEEE80211_IOCTL_DELKEY      (SIOCIWFIRSTPRIV+4)
#define IEEE80211_IOCTL_GETWMMPARAMS    (SIOCIWFIRSTPRIV+5)
#define IEEE80211_IOCTL_SETMLME     (SIOCIWFIRSTPRIV+6)
#define IEEE80211_IOCTL_GETCHANINFO (SIOCIWFIRSTPRIV+7)
#define IEEE80211_IOCTL_SETOPTIE    (SIOCIWFIRSTPRIV+8)
#define IEEE80211_IOCTL_GETOPTIE    (SIOCIWFIRSTPRIV+9)
#define IEEE80211_IOCTL_ADDMAC      (SIOCIWFIRSTPRIV+10)        /* Add ACL MAC Address */
#define IEEE80211_IOCTL_DELMAC      (SIOCIWFIRSTPRIV+12)        /* Del ACL MAC Address */
#define IEEE80211_IOCTL_GETCHANLIST (SIOCIWFIRSTPRIV+13)
#define IEEE80211_IOCTL_SETCHANLIST (SIOCIWFIRSTPRIV+14)
#define IEEE80211_IOCTL_KICKMAC     (SIOCIWFIRSTPRIV+15)
#define IEEE80211_IOCTL_CHANSWITCH  (SIOCIWFIRSTPRIV+16)
#define IEEE80211_IOCTL_GETMODE     (SIOCIWFIRSTPRIV+17)
#define IEEE80211_IOCTL_SETMODE     (SIOCIWFIRSTPRIV+18)
#define IEEE80211_IOCTL_GET_APPIEBUF    (SIOCIWFIRSTPRIV+19)
#define IEEE80211_IOCTL_SET_APPIEBUF    (SIOCIWFIRSTPRIV+20)
#define IEEE80211_IOCTL_SET_ACPARAMS    (SIOCIWFIRSTPRIV+21)
#define IEEE80211_IOCTL_FILTERFRAME (SIOCIWFIRSTPRIV+22)
#define IEEE80211_IOCTL_SET_RTPARAMS    (SIOCIWFIRSTPRIV+23)
#define IEEE80211_IOCTL_DBGREQ          (SIOCIWFIRSTPRIV+24)
#define IEEE80211_IOCTL_SEND_MGMT   (SIOCIWFIRSTPRIV+26)
#define IEEE80211_IOCTL_SET_MEDENYENTRY (SIOCIWFIRSTPRIV+27)
#define IEEE80211_IOCTL_CHN_WIDTHSWITCH (SIOCIWFIRSTPRIV+28)
#define IEEE80211_IOCTL_GET_MACADDR (SIOCIWFIRSTPRIV+29)        /* Get ACL List */
#define IEEE80211_IOCTL_SET_HBRPARAMS   (SIOCIWFIRSTPRIV+30)
#define IEEE80211_IOCTL_SET_RXTIMEOUT   (SIOCIWFIRSTPRIV+31)

#define IEEE80211_IOCTL_STA_INFO    (SIOCDEVPRIVATE+6)
#define IEEE80211_PARAM_MACCMD  17
#define IEEE80211_EXTCAPIE_BSSTRANSITION    0x08
#define IEEE80211_CAPINFO_RADIOMEAS 0x1000
#define IEEE80211_PARAM_GET_ACS 309
#define IEEE80211_PARAM_GET_CAC 310
#define IEEE80211_IOCTL_STA_STATS   (SIOCDEVPRIVATE+5)
#define IEEE80211_IOCTL_ATF_SHOWATFTBL  0xFF05
#define IEEE80211_IOCTL_CONFIG_GENERIC  (SIOCDEVPRIVATE+12)

#ifndef     IFNAMSIZ
#define     IFNAMSIZ    16
#endif

/*enum to handle MAC operations*/
enum wlanif_ioops_t
{
    IO_OPERATION_ADDMAC=0,
    IO_OPERATION_DELMAC,
    IO_OPERATION_KICKMAC,
    IO_OPERATION_LOCAL_DISASSOC,
    IO_OPERATION_ADDMAC_VALIDITY_TIMER,
};


/* BSTM MBO/QCN Transition Request Reason Code */
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


/* kev event_code value for Atheros IEEE80211 events */
enum {
    IEEE80211_EV_SCAN_DONE,
    IEEE80211_EV_CHAN_START,
    IEEE80211_EV_CHAN_END,
    IEEE80211_EV_RX_MGMT,
    IEEE80211_EV_P2P_SEND_ACTION_CB,
    IEEE80211_EV_IF_RUNNING,
    IEEE80211_EV_IF_NOT_RUNNING,
    IEEE80211_EV_AUTH_COMPLETE_AP,
    IEEE80211_EV_ASSOC_COMPLETE_AP,
    IEEE80211_EV_DEAUTH_COMPLETE_AP,
    IEEE80211_EV_AUTH_IND_AP,
    IEEE80211_EV_AUTH_COMPLETE_STA,
    IEEE80211_EV_ASSOC_COMPLETE_STA,
    IEEE80211_EV_DEAUTH_COMPLETE_STA,
    IEEE80211_EV_DISASSOC_COMPLETE_STA,
    IEEE80211_EV_AUTH_IND_STA,
    IEEE80211_EV_DEAUTH_IND_STA,
    IEEE80211_EV_ASSOC_IND_STA,
    IEEE80211_EV_DISASSOC_IND_STA,
    IEEE80211_EV_DEAUTH_IND_AP,
    IEEE80211_EV_DISASSOC_IND_AP,
    IEEE80211_EV_ASSOC_IND_AP,
    IEEE80211_EV_REASSOC_IND_AP,
    IEEE80211_EV_MIC_ERR_IND_AP,
    IEEE80211_EV_KEYSET_DONE_IND_AP,
    IEEE80211_EV_BLKLST_STA_AUTH_IND_AP,
    IEEE80211_EV_WAPI,
    IEEE80211_EV_TX_MGMT,
    IEEE80211_EV_CHAN_CHANGE,
    IEEE80211_EV_RECV_PROBEREQ,
    IEEE80211_EV_STA_AUTHORIZED,
    IEEE80211_EV_STA_LEAVE,
    IEEE80211_EV_ASSOC_FAILURE,
    IEEE80211_EV_DISASSOC_COMPLETE_AP,
    IEEE80211_EV_PRIMARY_RADIO_CHANGED,
    IEEE80211_EV_MU_RPT,
    IEEE80211_EV_SCAN,
    IEEE80211_EV_ATF_CONFIG,
    IEEE80211_EV_MESH_PEER_TIMEOUT,
    IEEE80211_EV_UNPROTECTED_DEAUTH_IND_STA,
};

typedef struct ieee80211_rrm_tsmreq_info_s {
    u_int16_t   num_rpt;
    u_int16_t   rand_ivl;
    u_int16_t   meas_dur;
    u_int8_t    reqmode;
    u_int8_t    reqtype;
    u_int8_t    tid;
    u_int8_t    macaddr[6];
    u_int8_t    bin0_range;
    u_int8_t    trig_cond;
    u_int8_t    avg_err_thresh;
    u_int8_t    cons_err_thresh;
    u_int8_t    delay_thresh;
    u_int8_t    meas_count;
    u_int8_t    trig_timeout;
}ieee80211_rrm_tsmreq_info_t;


/*
 * athdbg request
 */
enum {
    IEEE80211_DBGREQ_SENDADDBA     =    0,
    IEEE80211_DBGREQ_SENDDELBA     =    1,
    IEEE80211_DBGREQ_SETADDBARESP  =    2,
    IEEE80211_DBGREQ_GETADDBASTATS =    3,
    IEEE80211_DBGREQ_SENDBCNRPT    =    4, /* beacon report request */
    IEEE80211_DBGREQ_SENDTSMRPT    =    5, /* traffic stream measurement report */
    IEEE80211_DBGREQ_SENDNEIGRPT   =    6, /* neigbor report */
    IEEE80211_DBGREQ_SENDLMREQ     =    7, /* link measurement request */
    IEEE80211_DBGREQ_SENDBSTMREQ   =    8, /* bss transition management request */
    IEEE80211_DBGREQ_SENDCHLOADREQ =    9, /* bss channel load  request */
    IEEE80211_DBGREQ_SENDSTASTATSREQ =  10, /* sta stats request */
    IEEE80211_DBGREQ_SENDNHIST     =    11, /* Noise histogram request */
    IEEE80211_DBGREQ_SENDDELTS     =    12, /* delete TSPEC */
    IEEE80211_DBGREQ_SENDADDTSREQ  =    13, /* add TSPEC */
    IEEE80211_DBGREQ_SENDLCIREQ    =    14, /* Location config info request */
    IEEE80211_DBGREQ_GETRRMSTATS   =    15, /* RRM stats */
    IEEE80211_DBGREQ_SENDFRMREQ    =    16, /* RRM Frame request */
    IEEE80211_DBGREQ_GETBCNRPT     =    17, /* GET BCN RPT */
    IEEE80211_DBGREQ_SENDSINGLEAMSDU=   18, /* Sends single VHT MPDU AMSDUs */
    IEEE80211_DBGREQ_GETRRSSI      =    19, /* GET the Inst RSSI */
    IEEE80211_DBGREQ_GETACSREPORT  =    20, /* GET the ACS report */
    IEEE80211_DBGREQ_SETACSUSERCHANLIST  =    21, /* SET ch list for acs reporting  */
    IEEE80211_DBGREQ_GETACSUSERCHANLIST  =    22, /* GET ch list used in acs reporting */
    IEEE80211_DBGREQ_BLOCK_ACS_CHANNEL   =    23, /* Block acs for these channels */
    IEEE80211_DBGREQ_TR069               =    24, /* to be used for tr069 */
    IEEE80211_DBGREQ_CHMASKPERSTA        =    25, /* to be used for chainmask per sta */
    IEEE80211_DBGREQ_FIPS          = 26, /* to be used for setting fips*/
    IEEE80211_DBGREQ_FW_TEST       = 27, /* to be used for firmware testing*/
    IEEE80211_DBGREQ_SETQOSMAPCONF       =    28, /* set QoS map configuration */
    IEEE80211_DBGREQ_BSTEERING_SET_PARAMS =   29, /* Set the static band steering parameters */
    IEEE80211_DBGREQ_BSTEERING_GET_PARAMS =   30, /* Get the static band steering parameters */
    IEEE80211_DBGREQ_BSTEERING_SET_DBG_PARAMS =   31, /* Set the band steering debugging parameters */
    IEEE80211_DBGREQ_BSTEERING_GET_DBG_PARAMS =   32, /* Get the band steering debugging parameters */
    IEEE80211_DBGREQ_BSTEERING_ENABLE         =   33, /* Enable/Disable band steering */
    IEEE80211_DBGREQ_BSTEERING_SET_OVERLOAD   =   34, /* SET overload status */
    IEEE80211_DBGREQ_BSTEERING_GET_OVERLOAD   =   35, /* GET overload status */
    IEEE80211_DBGREQ_BSTEERING_GET_RSSI       =   36, /* Request RSSI measurement */
    IEEE80211_DBGREQ_INITRTT3       = 37, /* to test RTT3 feature*/
    IEEE80211_DBGREQ_SET_ANTENNA_SWITCH       = 38, /* Dynamic Antenna Selection */
    IEEE80211_DBGREQ_SETSUSERCTRLTBL          = 39, /* set User defined control table*/
    IEEE80211_DBGREQ_OFFCHAN_TX               = 40, /* Offchan tx*/
    IEEE80211_DBGREQ_BSTEERING_SET_PROBE_RESP_WH  = 41,/* Control whether probe responses are withheld for a MAC */
    IEEE80211_DBGREQ_BSTEERING_GET_PROBE_RESP_WH  = 42,/* Query whether probe responses are withheld for a MAC */
    IEEE80211_DBGREQ_GET_RRM_STA_LIST             = 43, /* to get list of connected rrm capable station */
    /* bss transition management request, targetted to a particular AP (or set of APs) */
    IEEE80211_DBGREQ_SENDBSTMREQ_TARGET           = 44,
    /* Get data rate related info for a VAP or a client */
    IEEE80211_DBGREQ_BSTEERING_GET_DATARATE_INFO  = 45,
    /* Enable/Disable band steering events on a VAP */
    IEEE80211_DBGREQ_BSTEERING_ENABLE_EVENTS      = 46,
#if QCA_LTEU_SUPPORT
    IEEE80211_DBGREQ_MU_SCAN                      = 47, /* do a MU scan */
    IEEE80211_DBGREQ_LTEU_CFG                     = 48, /* LTEu specific configuration */
    IEEE80211_DBGREQ_AP_SCAN                      = 49, /* do a AP scan */
#endif
    IEEE80211_DBGREQ_ATF_DEBUG_SIZE               = 50, /* Set the ATF history size */
    IEEE80211_DBGREQ_ATF_DUMP_DEBUG               = 51, /* Dump the ATF history */
#if QCA_LTEU_SUPPORT
    IEEE80211_DBGREQ_SCAN_REPEAT_PROBE_TIME       = 52, /* scan probe time, part of scan params */
    IEEE80211_DBGREQ_SCAN_REST_TIME               = 53, /* scan rest time, part of scan params */
    IEEE80211_DBGREQ_SCAN_IDLE_TIME               = 54, /* scan idle time, part of scan params */
    IEEE80211_DBGREQ_SCAN_PROBE_DELAY             = 55, /* scan probe delay, part of scan params */
    IEEE80211_DBGREQ_MU_DELAY                     = 56, /* delay between channel change and MU start (for non-gpio) */
    IEEE80211_DBGREQ_WIFI_TX_POWER                = 57, /* assumed tx power of wifi sta */
#endif
    /* Cleanup all STA state (equivalent to disassociation, without sending the frame OTA) */
    IEEE80211_DBGREQ_BSTEERING_LOCAL_DISASSOCIATION = 58,
    IEEE80211_DBGREQ_BSTEERING_SET_STEERING       = 59, /* Set steering in progress flag for a STA */
    IEEE80211_DBGREQ_CHAN_LIST                    =60,
    IEEE80211_DBGREQ_MBO_BSSIDPREF                = 61,
#if UMAC_SUPPORT_VI_DBG
    IEEE80211_DBGREQ_VOW_DEBUG_PARAM              = 62,
    IEEE80211_DBGREQ_VOW_DEBUG_PARAM_PERSTREAM    = 63,
#endif
#if QCA_LTEU_SUPPORT
    IEEE80211_DBGREQ_SCAN_PROBE_SPACE_INTERVAL     = 64,
#endif
    IEEE80211_DBGREQ_ASSOC_WATERMARK_TIME         = 65,  /* Get the date when the max number of devices has been associated crossing the threshold */
    IEEE80211_DBGREQ_DISPLAY_TRAFFIC_STATISTICS   = 66, /* Display the traffic statistics of each connected STA */
    IEEE80211_DBGREQ_ATF_DUMP_NODESTATE           = 67,
    IEEE80211_DBGREQ_BSTEERING_SET_DA_STAT_INTVL  = 68,
    IEEE80211_DBGREQ_BSTEERING_SET_AUTH_ALLOW     = 69,
    IEEE80211_DBGREQ_BSTEERING_SET_PROBE_RESP_ALLOW_24G = 70, /* Control whether probe responses are allowed for a MAC in 2.4g band */
    IEEE80211_DBGREQ_SETINNETWORK_2G              = 71, /*set 2.4G innetwork inforamtion to acs module*/
    IEEE80211_DBGREQ_GETINNETWORK_2G              = 72, /*get 2.4G innetwork inforamtion from acs module*/
    IEEE80211_DBGREQ_SET_SOFTBLOCKING             = 73, /* set softblocking flag of a STA */
    IEEE80211_DBGREQ_GET_SOFTBLOCKING             = 74, /* get softblocking flag of a STA */
};


/**
 * @brief BSS Transition Management response status codes
 * Taken from 802.11v standard
 */
enum IEEE80211_WNM_BSTM_RESP_STATUS {
    IEEE80211_WNM_BSTM_RESP_SUCCESS,
    IEEE80211_WNM_BSTM_RESP_REJECT_UNSPECIFIED,
    IEEE80211_WNM_BSTM_RESP_REJECT_INSUFFICIENT_BEACONS,
    IEEE80211_WNM_BSTM_RESP_REJECT_INSUFFICIENT_CAPACITY,
    IEEE80211_WNM_BSTM_RESP_REJECT_BSS_TERMINATION_UNDESIRED,
    IEEE80211_WNM_BSTM_RESP_REJECT_BSS_TERMINATION_DELAY,
    IEEE80211_WNM_BSTM_RESP_REJECT_STA_CANDIDATE_LIST_PROVIDED,
    IEEE80211_WNM_BSTM_RESP_REJECT_NO_SUITABLE_CANDIDATES,
    IEEE80211_WNM_BSTM_RESP_REJECT_LEAVING_ESS,
    IEEE80211_WNM_BSTM_RESP_INVALID
};

/* BSTM MBO/QCN Transition Rejection Reason Code */
enum IEEE80211_BSTM_REJECT_REASON_CODE {
    IEEE80211_BSTM_REJECT_REASON_UNSPECIFIED,
    IEEE80211_BSTM_REJECT_REASON_FRAME_LOSS_RATE,        /* Excessive frame loss rate is expected by STA */
    IEEE80211_BSTM_REJECT_REASON_DELAY_FOR_TRAFFIC,      /* Excessive traffic delay will be incurred this time */
    IEEE80211_BSTM_REJECT_REASON_INSUFFICIENT_CAPACITY,  /* Insufficient QoS capacity for current traffic stream expected by STA */
    IEEE80211_BSTM_REJECT_REASON_LOW_RSSI,               /* STA receiving low RSSI in frames from suggested channel(s) */
    IEEE80211_BSTM_REJECT_REASON_HIGH_INTERFERENCE,      /* High Interference is expected at the candidate channel(s) */
    IEEE80211_BSTM_REJECT_REASON_SERVICE_UNAVAILABLE,    /* Required services by STA are not available on requested channel */

    IEEE80211_BSTM_REJECT_REASON_INVALID                 /* value 7 - 255 is reserved and considered invalid */
};

struct ieee80211_beaconreq_chaninfo {
    u_int8_t regclass;
    u_int8_t numchans;
    u_int8_t channum[IEEE80211_RRM_NUM_CHANREQ_MAX];
};

typedef struct ieee80211_rrm_beaconreq_info_s {
    u_int16_t   num_rpt;
    u_int8_t    regclass;
    u_int8_t    channum;
    u_int16_t   random_ivl;
    u_int16_t   duration;
    u_int8_t    reqmode;
    u_int8_t    reqtype;
    u_int8_t    bssid[6];
    u_int8_t    mode;
    u_int8_t    req_ssid;
    u_int8_t    rep_cond;
    u_int8_t    rep_thresh;
    u_int8_t    rep_detail;
    u_int8_t    req_ie;
    u_int8_t    num_chanrep;
    struct ieee80211_beaconreq_chaninfo
        apchanrep[IEEE80211_RRM_NUM_CHANREP_MAX];
}ieee80211_rrm_beaconreq_info_t;

/*
 * MAC ACL operations.
 */
enum {
    IEEE80211_MACCMD_POLICY_OPEN    = 0,  /* set policy: no ACL's */
    IEEE80211_MACCMD_POLICY_ALLOW   = 1,  /* set policy: allow traffic */
    IEEE80211_MACCMD_POLICY_DENY    = 2,  /* set policy: deny traffic */
    IEEE80211_MACCMD_FLUSH          = 3,  /* flush ACL database */
    IEEE80211_MACCMD_DETACH         = 4,  /* detach ACL policy */
    IEEE80211_MACCMD_POLICY_RADIUS  = 5,  /* set policy: RADIUS managed ACLs */
};

typedef struct ieee80211_rrm_nrreq_info_s {
    u_int8_t dialogtoken;
    u_int8_t ssid[32];
    u_int8_t ssid_len;
    u_int8_t* essid;
    u_int8_t essid_len;
    u_int8_t meas_count; /* Request for LCI/LCR may come in any order */
    u_int8_t meas_token[2];
    u_int8_t meas_req_mode[2];
    u_int8_t meas_type[2];
    u_int8_t loc_sub[2];
}ieee80211_rrm_nrreq_info_t;

struct ieee80211_bstm_reqinfo {
    u_int8_t dialogtoken;
    u_int8_t candidate_list;
    u_int8_t disassoc;
    u_int16_t disassoc_timer;
    u_int8_t validity_itvl;
    u_int8_t bssterm_inc;
    u_int64_t bssterm_tsf;
    u_int16_t bssterm_duration;
    u_int8_t abridged;
};

/* candidate BSSID for BSS Transition Management Request */
struct ieee80211_bstm_candidate {
    /* candidate BSSID */
    u_int8_t bssid[MAC_ADDR_LEN];
    /* channel number for the candidate BSSID */
    u_int8_t channel_number;
    /* preference from 1-255 (higher number = higher preference) */
    u_int8_t preference;
    /* operating class */
    u_int8_t op_class;
    /* PHY type */
    u_int8_t phy_type;
};


/* maximum number of candidates in the list.  Value 3 was selected based on a
   2 AP network, so may be increased if needed */
#define ieee80211_bstm_req_max_candidates 3
/* BSS Transition Management Request information that can be specified via ioctl */
struct ieee80211_bstm_reqinfo_target {
    u_int8_t dialogtoken;
    u_int8_t num_candidates;
#if QCN_IE
    /*If we use trans reason code in QCN IE */
    u_int8_t qcn_trans_reason;
#endif
    u_int8_t trans_reason;
    u_int8_t disassoc;
    u_int16_t disassoc_timer;
    struct ieee80211_bstm_candidate candidates[ieee80211_bstm_req_max_candidates];
};

/*
 * When user, via "wifitool athX setbssidpref mac_addr pref_val"
 * command enters a bssid with preference value needed to
 * be assigned to that bssid, it is stored in a structure
 * as given below
 */
struct ieee80211_user_bssid_pref {
    u_int8_t bssid[MAC_ADDR_LEN];
    u_int8_t pref_val;
    u_int8_t regclass;
    u_int8_t chan;
};

typedef struct _ieee80211_tspec_info {
    u_int8_t    traffic_type;
    u_int8_t    direction;
    u_int8_t    dot1Dtag;
    u_int8_t    tid;
    u_int8_t    acc_policy_edca;
    u_int8_t    acc_policy_hcca;
    u_int8_t    aggregation;
    u_int8_t    psb;
    u_int8_t    ack_policy;
    u_int16_t   norminal_msdu_size;
    u_int16_t   max_msdu_size;
    u_int32_t   min_srv_interval;
    u_int32_t   max_srv_interval;
    u_int32_t   inactivity_interval;
    u_int32_t   suspension_interval;
    u_int32_t   srv_start_time;
    u_int32_t   min_data_rate;
    u_int32_t   mean_data_rate;
    u_int32_t   peak_data_rate;
    u_int32_t   max_burst_size;
    u_int32_t   delay_bound;
    u_int32_t   min_phy_rate;
    u_int16_t   surplus_bw;
    u_int16_t   medium_time;
} ieee80211_tspec_info;

typedef struct ieee80211_rrm_chloadreq_info_s{
    u_int8_t dstmac[6];
    u_int16_t num_rpts;
    u_int8_t regclass;
    u_int8_t chnum;
    u_int16_t r_invl;
    u_int16_t m_dur;
    u_int8_t cond;
    u_int8_t c_val;
}ieee80211_rrm_chloadreq_info_t;

typedef struct ieee80211_rrm_stastats_info_s{
    u_int8_t dstmac[6];
    u_int16_t num_rpts;
    u_int16_t m_dur;
    u_int16_t r_invl;
    u_int8_t  gid;
}ieee80211_rrm_stastats_info_t;

typedef struct ieee80211_rrm_nhist_info_s{
    u_int16_t num_rpts;
    u_int8_t dstmac[6];
    u_int8_t regclass;
    u_int8_t chnum;
    u_int16_t r_invl;
    u_int16_t m_dur;
    u_int8_t cond;
    u_int8_t c_val;
}ieee80211_rrm_nhist_info_t;

typedef struct ieee80211_rrm_frame_req_info_s{
    u_int8_t dstmac[6];
    u_int8_t peermac[6];
    u_int16_t num_rpts;
    u_int8_t regclass;
    u_int8_t chnum;
    u_int16_t r_invl;
    u_int16_t m_dur;
    u_int8_t ftype;
}ieee80211_rrm_frame_req_info_t;

typedef struct ieee80211_rrm_lcireq_info_s
{
    u_int8_t dstmac[6];
    u_int16_t num_rpts;
    u_int8_t location;
    u_int8_t lat_res;
    u_int8_t long_res;
    u_int8_t alt_res;
    u_int8_t azi_res;
    u_int8_t azi_type;
}ieee80211_rrm_lcireq_info_t;

typedef struct ieee80211req_rrmstats_s {
    u_int32_t index;
    u_int32_t data_size;
    void *data_addr;
}ieee80211req_rrmstats_t;

typedef struct ieee80211req_acs_r{
    u_int32_t index;
    u_int32_t data_size;
    void *data_addr;
}ieee80211req_acs_t;

/*
 * command id's for use in tr069 request
 */
typedef enum _ieee80211_tr069_cmd_ {
    TR069_CHANHIST           = 1,
    TR069_TXPOWER            = 2,
    TR069_GETTXPOWER         = 3,
    TR069_GUARDINTV          = 4,
    TR069_GET_GUARDINTV      = 5,
    TR069_GETASSOCSTA_CNT    = 6,
    TR069_GETTIMESTAMP       = 7,
    TR069_GETDIAGNOSTICSTATE = 8,
    TR069_GETNUMBEROFENTRIES = 9,
    TR069_GET11HSUPPORTED    = 10,
    TR069_GETPOWERRANGE      = 11,
    TR069_SET_OPER_RATE      = 12,
    TR069_GET_OPER_RATE      = 13,
    TR069_GET_POSIBLRATE     = 14,
    TR069_SET_BSRATE         = 15,
    TR069_GET_BSRATE         = 16,
    TR069_GETSUPPORTEDFREQUENCY  = 17,
    TR069_GET_PLCP_ERR_CNT   = 18,
    TR069_GET_FCS_ERR_CNT    = 19,
    TR069_GET_PKTS_OTHER_RCVD = 20,
    TR069_GET_FAIL_RETRANS_CNT = 21,
    TR069_GET_RETRY_CNT      = 22,
    TR069_GET_MUL_RETRY_CNT  = 23,
    TR069_GET_ACK_FAIL_CNT   = 24,
    TR069_GET_AGGR_PKT_CNT   = 25,
    TR069_GET_STA_BYTES_SENT = 26,
    TR069_GET_STA_BYTES_RCVD = 27,
    TR069_GET_DATA_SENT_ACK  = 28,
    TR069_GET_DATA_SENT_NOACK = 29,
    TR069_GET_CHAN_UTIL      = 30,
    TR069_GET_RETRANS_CNT    = 31,
}ieee80211_tr069_cmd;


/*
 * common structure to handle tr069 commands;
 * the cmdid and data pointer has to be appropriately
 * filled in
 */
typedef struct{
    u_int32_t data_size;
    ieee80211_tr069_cmd cmdid;
    void *data_addr;
}ieee80211req_tr069_t;

typedef struct ieee80211req_fips {
    u_int32_t data_size;
    void *data_addr;
}ieee80211req_fips_t;

#define IEEE80211_MAX_QOS_UP_RANGE  8
#define IEEE80211_MAX_QOS_DSCP_EXCEPT   21

struct ieee80211_dscp_range {
    u_int8_t low;
    u_int8_t high;
};

struct ieee80211_dscp_exception {
    u_int8_t dscp;
    u_int8_t up;
};


struct ieee80211_qos_map {
    struct ieee80211_dscp_range
        up[IEEE80211_MAX_QOS_UP_RANGE];                 /* user priority map fields */
    u_int16_t valid;
    u_int16_t num_dscp_except;                          /* count of dscp exception fields */
    struct ieee80211_dscp_exception
        dscp_exception[IEEE80211_MAX_QOS_DSCP_EXCEPT];  /* dscp exception fields */
};

typedef struct ieee80211_bsteering_param_t {
    /* Amount of time a client has to be idle under normal (no overload)
       conditions before it becomes a candidate for steering.*/
    u_int32_t inactivity_timeout_normal;
    /*  Amount of time a client has to be idle under overload conditions
        before it becomes a candidate for steering.*/
    u_int32_t inactivity_timeout_overload;
    /* Frequency (in seconds) at which the client inactivity staus should
       be checked. */
    u_int32_t inactivity_check_period;
    /* Frequency (in seconds) at which the medium utilization should be
       measured. */
    u_int32_t utilization_sample_period;
    /* The number of samples over which the medium utilization should be
       averaged before being reported.*/
    u_int32_t utilization_average_num_samples;
    /* Two RSSI values for which to generate threshold crossing events for
       an idle client. Such events are generated when the thresholds are
       crossed in either direction.*/
    u_int32_t inactive_rssi_xing_high_threshold;
    u_int32_t inactive_rssi_xing_low_threshold;
    /* The RSSI value for which to generate threshold crossing events for
       both active and idle clients. This value should generally be less
       than inactive_rssi_xing_low_threshold.*/
    u_int32_t low_rssi_crossing_threshold;
    /* The lower-bound Tx rate value (Kbps) for which to generate threshold crossing events
       if the Tx rate for a client decreases below this value.*/
    u_int32_t low_tx_rate_crossing_threshold;
    /* The upper-bound Tx rate (Kbps) value for which to generate threshold crossing events
       if the Tx rate for a client increases above this value.*/
    u_int32_t high_tx_rate_crossing_threshold;
    /* The RSSI value for which to generate threshold crossing events for
       active clients. Used in conjunction with the rate crossing events
       to determine if STAs should be downgraded. */
    u_int32_t low_rate_rssi_crossing_threshold;
    /* The RSSI value for which to generate threshold crossing events for
       active clients. Used in conjunction with the rate crossing events
       to determine if STAs should be upgraded. */
    u_int32_t high_rate_rssi_crossing_threshold;
    /* The RSSI value for which to generate threshold crossing events for
       a client. Used to determine if STAs should be steered to another AP. */
    u_int32_t ap_steer_rssi_xing_low_threshold;
    /* If set, enable interference detection */
    u_int8_t interference_detection_enable;
    /* Delay sending probe responses, if 2.4G RSSI of a STA is
     * above this threshold */
    u_int32_t delay_24g_probe_rssi_threshold;
    /* Delay sending probe responses till this time window and probe request
     * count is less than or equal to 'delay_24g_probe_min_req_count' */
    u_int32_t delay_24g_probe_time_window;
    /* Deny sending probe responses for this many times in
     * 'delay_24g_probe_time_window' time window */
    u_int32_t delay_24g_probe_min_req_count;
} ieee80211_bsteering_param_t;

/**
 * Parameters that can be configured by userspace to enable logging of
 * intermediate results via events to userspace.
 */
typedef struct ieee80211_bsteering_dbg_param_t {
    /* Whether logging of the raw channel utilization data is enabled.*/
    u_int8_t  raw_chan_util_log_enable:1;
    /* Whether logging of the raw RSSI measurement is enabled.*/
    u_int8_t  raw_rssi_log_enable:1;
    /* Whether logging of the raw Tx rate measurement is enabled.*/
    u_int8_t  raw_tx_rate_log_enable:1;
} ieee80211_bsteering_dbg_param_t;

/**
 * Parameters that must be specified to trigger an RSSI measurement by
 * sending QoS Null Data Packets and examining the RSSI from the ACK.
 */
typedef struct ieee80211_bsteering_rssi_req_t {
    /* The address of the client to measure.*/
    u_int8_t sender_addr[IEEE80211_ADDR_LEN];
    /* The number of consecutive measurements to make. This must be
       at least 1.*/
    u_int16_t num_measurements;
} ieee80211_bsteering_rssi_req_t;

/**
 * Data rated related information contained in ATH_EVENT_BSTEERING_NODE_ASSOCIATED
 * and IEEE80211_DBGREQ_BSTEERING_GET_DATARATE_INFO response
 */
typedef struct ieee80211_bsteering_datarate_info_t {
    /* Maximum bandwidth the client supports, valid values are enumerated
     * in enum ieee80211_cwm_width in _ieee80211.h. But the header file cannot
     * be included here because of potential circular dependency. Caller should
     * make sure that only valid values can be written/read. */
    u_int8_t max_chwidth;
    /* Number of spatial streams the client supports */
    u_int8_t num_streams;
    /* PHY mode the client supports. Same as max_chwidth field, only valid values
     * enumerated in enum ieee80211_phymode can be used here. */
    u_int8_t phymode;
    /* Maximum MCS the client supports */
    u_int8_t max_MCS;
    /* Maximum TX power the client supports */
    u_int8_t max_txpower;
    /* Set to 1 if this client is operating in Static SM Power Save mode */
    u_int8_t is_static_smps : 1;
    /* Set to 1 if this client supports MU-MIMO */
    u_int8_t is_mu_mimo_supported : 1;
} ieee80211_bsteering_datarate_info_t;

typedef struct ieee80211_offchan_tx_test {
    u_int8_t ieee_chan;
    u_int16_t dwell_time;
} ieee80211_offchan_tx_test_t;

typedef struct ieee80211_vow_dbg_stream_param {
    u_int8_t  stream_num;         /* the stream number whose markers are being set */
    u_int8_t  marker_num;         /* the marker number whose parameters (offset, size & match) are being set */
    u_int32_t marker_offset;      /* byte offset from skb start (upper 16 bits) & size in bytes(lower 16 bits) */
    u_int32_t marker_match;       /* marker pattern match used in filtering */
} ieee80211_vow_dbg_stream_param_t;

typedef struct ieee80211_vow_dbg_param {
    u_int8_t  num_stream;        /* Number of streams */
    u_int8_t  num_marker;       /* total number of markers used to filter pkts */
    u_int32_t rxq_offset;      /* Rx Seq num offset skb start (upper 16 bits) & size in bytes(lower 16 bits) */
    u_int32_t rxq_shift;         /* right-shift value in case field is not word aligned */
    u_int32_t rxq_max;           /* Max Rx seq number */
    u_int32_t time_offset;       /* Time offset for the packet*/
} ieee80211_vow_dbg_param_t;

typedef enum {
    MU_ALGO_1 = 0x1, /* Basic binning algo */
    MU_ALGO_2 = 0x2, /* Enhanced binning algo */
    MU_ALGO_3 = 0x4, /* Enhanced binning including accounting for hidden nodes */
    MU_ALGO_4 = 0x8, /* TA based MU calculation */
} mu_algo_t;


typedef struct {
    u_int8_t     mu_req_id;             /* MU request id */
    u_int8_t     mu_channel;            /* IEEE channel number on which to do MU scan */
    mu_algo_t    mu_type;               /* which MU algo to use */
    u_int32_t    mu_duration;           /* duration of the scan in ms */
    u_int32_t    lteu_tx_power;         /* LTEu Tx power */
    u_int32_t    mu_rssi_thr_bssid;     /* RSSI threshold to account for active APs */
    u_int32_t    mu_rssi_thr_sta;       /* RSSI threshold to account for active STAs */
    u_int32_t    mu_rssi_thr_sc;        /* RSSI threshold to account for active small cells */
    u_int32_t    home_plmnid;           /* to be compared with PLMN ID to distinguish same and different operator WCUBS */
    u_int32_t    alpha_num_bssid;       /* alpha for num active bssid calculation,kept for backward compatibility */
} ieee80211req_mu_scan_t;

typedef struct {
    u_int8_t     lteu_gpio_start;        /* start MU/AP scan after GPIO toggle */
    u_int8_t     lteu_num_bins;          /* no. of elements in the following arrays */
    u_int8_t     use_actual_nf;          /* whether to use the actual NF obtained or a hardcoded one */
    u_int32_t    lteu_weight[LTEU_MAX_BINS];  /* weights for MU algo */
    u_int32_t    lteu_thresh[LTEU_MAX_BINS];  /* thresholds for MU algo */
    u_int32_t    lteu_gamma[LTEU_MAX_BINS];   /* gamma's for MU algo */
    u_int32_t    lteu_scan_timeout;      /* timeout in ms to gpio toggle */
    u_int32_t    alpha_num_bssid;      /* alpha for num active bssid calculation */
    u_int32_t    lteu_cfg_reserved_1;    /* used to indicate to fw whether or not packets with phy error are to
                                            be included in MU calculation or not */

} ieee80211req_lteu_cfg_t;

typedef enum {
    SCAN_PASSIVE,
    SCAN_ACTIVE,
} scan_type_t;


typedef struct {
    u_int8_t     scan_req_id;          /* AP scan request id */
    u_int8_t     scan_num_chan;        /* Number of channels to scan, 0 for all channels */
    u_int8_t     scan_channel_list[MAX_SCAN_CHANS]; /* IEEE channel number of channels to scan */
    scan_type_t  scan_type;            /* Scan type - active or passive */
    u_int32_t    scan_duration;        /* Duration in ms for which a channel is scanned, 0 for default */
    u_int32_t    scan_repeat_probe_time;   /* Time before sending second probe request, (u32)(-1) for default */
    u_int32_t    scan_rest_time;       /* Time in ms on the BSS channel, (u32)(-1) for default */
    u_int32_t    scan_idle_time;       /* Time in msec on BSS channel before switching channel, (u32)(-1) for default */
    u_int32_t    scan_probe_delay;     /* Delay in msec before sending probe request, (u32)(-1) for default */
} ieee80211req_ap_scan_t;

#define MAX_CUSTOM_CHANS     101

typedef struct {
    u_int8_t     scan_numchan_associated;        /* Number of channels to scan, 0 for all channels */
    u_int8_t     scan_numchan_nonassociated;
    u_int8_t     scan_channel_list_associated[MAX_CUSTOM_CHANS]; /* IEEE channel number of channels to scan */
    u_int8_t     scan_channel_list_nonassociated[MAX_CUSTOM_CHANS];
}ieee80211req_custom_chan_t;

typedef struct {
    void *ptr;
    u_int32_t size;
} ieee80211req_atf_debug_t;

struct ieee80211req_athdbg {
    u_int8_t cmd;
    u_int8_t dstmac[IEEE80211_ADDR_LEN];
    union {
        u_long param[4];
        ieee80211_rrm_beaconreq_info_t bcnrpt;
        ieee80211_rrm_tsmreq_info_t    tsmrpt;
        ieee80211_rrm_nrreq_info_t     neigrpt;
        struct ieee80211_bstm_reqinfo   bstmreq;
        struct ieee80211_bstm_reqinfo_target   bstmreq_target;
        struct ieee80211_user_bssid_pref bssidpref;
        ieee80211_tspec_info     tsinfo;
        ieee80211_rrm_chloadreq_info_t chloadrpt;
        ieee80211_rrm_stastats_info_t  stastats;
        ieee80211_rrm_nhist_info_t     nhist;
        ieee80211_rrm_frame_req_info_t frm_req;
        ieee80211_rrm_lcireq_info_t    lci_req;
        ieee80211req_rrmstats_t        rrmstats_req;
        ieee80211req_acs_t             acs_rep;
        ieee80211req_tr069_t           tr069_req;
        ieee80211req_fips_t fips_req;
        struct ieee80211_qos_map       qos_map;
        ieee80211_bsteering_param_t    bsteering_param;
        ieee80211_bsteering_dbg_param_t bsteering_dbg_param;
        ieee80211_bsteering_rssi_req_t bsteering_rssi_req;
        u_int8_t                       bsteering_probe_resp_wh;
        u_int8_t                       bsteering_auth_allow;
        u_int8_t bsteering_enable;
        u_int8_t bsteering_overload;
        u_int8_t bsteering_rssi_num_samples;
        ieee80211_bsteering_datarate_info_t bsteering_datarate_info;
        u_int8_t bsteering_steering_in_progress;
        ieee80211_offchan_tx_test_t offchan_req;
        ieee80211_vow_dbg_stream_param_t   vow_dbg_stream_param;
        ieee80211_vow_dbg_param_t      vow_dbg_param;

        ieee80211req_mu_scan_t         mu_scan_req;
        ieee80211req_lteu_cfg_t        lteu_cfg;
        ieee80211req_ap_scan_t         ap_scan_req;
        ieee80211req_custom_chan_t     custom_chan_req;
        ieee80211req_atf_debug_t       atf_dbg_req;
        u_int32_t                      bsteering_sta_stats_update_interval_da;
        u_int8_t                       bsteering_probe_resp_allow_24g;
        u_int8_t                       acl_softblocking;
    } data;
};


/**
 * Parameters that can be configured by userspace to control the band
 * steering events.
 */


/* to user level */
typedef struct ieee80211_bcnrpt_s {
    u_int8_t bssid[6];
    u_int8_t rsni;
    u_int8_t rcpi;
    u_int8_t chnum;
    u_int8_t more;
} ieee80211_bcnrpt_t;


/* Enumeration for 802.11k beacon report request measurement mode,
 * as defined in Table 7-29e in IEEE Std 802.11k-2008 */
typedef enum {
    IEEE80211_RRM_BCNRPT_MEASMODE_PASSIVE = 0,
    IEEE80211_RRM_BCNRPT_MEASMODE_ACTIVE = 1,
    IEEE80211_RRM_BCNRPT_MEASMODE_BCNTABLE = 2,

    IEEE80211_RRM_BCNRPT_MEASMODE_RESERVED
} IEEE80211_RRM_BCNRPT_MEASMODE;

enum ieee80211_cwm_width {
    IEEE80211_CWM_WIDTH20,
    IEEE80211_CWM_WIDTH40,
    IEEE80211_CWM_WIDTH80,
    IEEE80211_CWM_WIDTH160,
    IEEE80211_CWM_WIDTH80_80,
    IEEE80211_CWM_WIDTHINVALID = 0xff    /* user invalid value */
};

/* XXX not really a mode; there are really multiple PHY's */
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
    IEEE80211_MODE_11NA_HT40        = 14,   /* 5Ghz, Auto HT40 */
    IEEE80211_MODE_11AC_VHT20       = 15,   /* 5Ghz, VHT20 */
    IEEE80211_MODE_11AC_VHT40PLUS   = 16,   /* 5Ghz, VHT40 (Ext ch +1) */
    IEEE80211_MODE_11AC_VHT40MINUS  = 17,   /* 5Ghz  VHT40 (Ext ch -1) */
    IEEE80211_MODE_11AC_VHT40       = 18,   /* 5Ghz, VHT40 */
    IEEE80211_MODE_11AC_VHT80       = 19,   /* 5Ghz, VHT80 */
    IEEE80211_MODE_11AC_VHT160      = 20,   /* 5Ghz, VHT160 */
    IEEE80211_MODE_11AC_VHT80_80    = 21,   /* 5Ghz, VHT80_80 */
};

/**
 * Enumeration for 802.11 regulatory class as defined in Annex E of
 * 802.11-Revmb/D12, November 2011
 *
 * It currently includes a subset of global operating classes as defined in Table E-4.
 */
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

/* as per 802.11mc spec anex C, used in Radio resource mgmt reprots */
enum ieee80211_phytype_mode {
    IEEE80211_PHY_TYPE_UNKNOWN = 0,
    IEEE80211_PHY_TYPE_FHSS = 1,    /* 802.11 2.4GHz 1997 */
    IEEE80211_PHY_TYPE_DSSS = 2,    /* 802.11 2.4GHz 1997 */
    IEEE80211_PHY_TYPE_IRBASEBAND = 3,
    IEEE80211_PHY_TYPE_OFDM  = 4,   /* 802.11ag */
    IEEE80211_PHY_TYPE_HRDSSS  = 5, /* 802.11b 1999 */
    IEEE80211_PHY_TYPE_ERP  = 6,    /* 802.11g 2003 */
    IEEE80211_PHY_TYPE_HT   = 7,    /* 802.11n */
    IEEE80211_PHY_TYPE_DMG  = 8,    /* 802.11ad */
    IEEE80211_PHY_TYPE_VHT  = 9,    /* 802.11ac */
    IEEE80211_PHY_TYPE_TVHT  = 10,      /* 802.11af */
};

struct bs_probe_req_ind {
    /* The MAC address of the client that sent the probe request.*/
    u_int8_t sender_addr[IEEE80211_ADDR_LEN];
    /*  The RSSI of the received probe request.*/
    u_int8_t rssi;
};

/**
 * Metadata about an authentication message that was sent with a failure
 * code due to the client being prohibited by the ACL.
 */
struct bs_auth_reject_ind {
    /* The MAC address of the client to which the authentication message
       was sent with a failure code.*/
    u_int8_t client_addr[IEEE80211_ADDR_LEN];
    /* The RSSI of the received authentication message (the one that
       triggered the rejection).*/
    u_int8_t rssi;
};


/**
 * Metadata about a client activity status change.
 */
struct bs_activity_change_ind {
    /* The MAC address of the client that activity status changes */
    u_int8_t client_addr[IEEE80211_ADDR_LEN];
    /* Activity status*/
    u_int8_t activity;
};

/**
 * Data for a channel utilization measurement.
 */
struct bs_chan_utilization_ind {
    /* The current utilization on the band, expressed as a percentage.*/
    u_int8_t utilization;
};

/**
 * Enumeration to mark crossing direction
 */
typedef enum {
    /* Threshold not crossed */
    BSTEERING_XING_UNCHANGED = 0,
    /* Threshold crossed in the up direction */
    BSTEERING_XING_UP = 1,
    /* Threshold crossed in the down direction */
    BSTEERING_XING_DOWN = 2
} BSTEERING_XING_DIRECTION;

/**
 * Metadata about a client RSSI measurement crossed threshold.
 */
struct bs_rssi_xing_threshold_ind {
    /* The MAC address of the client */
    u_int8_t client_addr[IEEE80211_ADDR_LEN];
    /* The measured RSSI */
    u_int8_t rssi;
    /* Flag indicating if it crossed inactivity RSSI threshold */
    BSTEERING_XING_DIRECTION inact_rssi_xing;
    /* Flag indicating if it crossed low RSSI threshold */
    BSTEERING_XING_DIRECTION low_rssi_xing;
    /* Flag indicating if it crossed the rate RSSI threshold */
    BSTEERING_XING_DIRECTION rate_rssi_xing;
    /* Flag indicating if it crossed the AP steering RSSI threshold */
    BSTEERING_XING_DIRECTION ap_rssi_xing;
};

/**
 * Metadata about a client requested RSSI measurement
 */
struct bs_rssi_measurement_ind {
    /* The MAC address of the client */
    u_int8_t client_addr[IEEE80211_ADDR_LEN];
    /* The measured RSSI */
    u_int8_t rssi;

    /* Flag indicating if it crossed inactivity RSSI threshold */
    BSTEERING_XING_DIRECTION map_rssi_xing;
};

/**
 * Metadata about a Tx rate measurement
 * NOTE: Debug event only, use bs_tx_rate_xing_threshold_ind for
 * rate crossing information.
 */
struct bs_tx_rate_measurement_ind {
    /* The MAC address of the client */
    u_int8_t client_addr[IEEE80211_ADDR_LEN];
    /* The measured Tx rate */
    u_int32_t tx_rate;
};

/**
 * Radio Resource Managmenet report types
 *
 * Note that these types are only used between user space and driver, and
 * not in sync with the OTA types defined in 802.11k spec.
 */
typedef enum {
    /* Indication of a beacon report. */
    BSTEERING_RRM_TYPE_BCNRPT,

    BSTEERING_RRM_TYPE_INVALID
} BSTEERING_RRM_TYPE;

/**
 * Metadata and report contents about a Radio Resource Measurement report
 */
struct bs_rrm_report_ind {
    /* The type of the rrm event: One of BSTEERING_RRM_TYPE.*/
    u_int32_t rrm_type;
    /* The token corresponding to the measurement request.*/
    u_int8_t dialog_token;
    /* MAC address of the reporter station.*/
    u_int8_t macaddr[IEEE80211_ADDR_LEN];
    /* The result bitmap, as defined in IEEE80211_RRM_MEASRPT_MODE.*/
    u_int8_t measrpt_mode;
    /* The report data. Which member is valid is based on the
       rrm_type field.*/
    union {
        ieee80211_bcnrpt_t bcnrpt[IEEE80211_BSTEERING_RRM_NUM_BCNRPT_MAX];
    } data;
};

/*
 * Metadata and report raw contents about a Radio Resource Measurement report
 */
struct bs_rrm_frame_report_ind {
    /* The type of the rrm event: One of BSTEERING_RRM_TYPE.*/
    u_int32_t rrm_type;
    /* The token corresponding to the measurement request.*/ 
    u_int8_t dialog_token;
    /* MAC address of the reporter station.*/
    u_int8_t macaddr[IEEE80211_ADDR_LEN];
    /* The result bitmap, as defined in IEEE80211_RRM_MEASRPT_MODE.*/
    u_int8_t measrpt_mode;
    /* number of measurement reports */
    u_int8_t num_meas_rpts;
    /* the data length */
    u_int32_t data_len;	
    /* the raw bytes for the measurement report */
    u_int8_t meas_rpt_data[];
};

/**
 * Wireless Network Management (WNM) report types
 */
typedef enum {
    /* Indication of reception of a BSS Transition Management response frame */
    BSTEERING_WNM_TYPE_BSTM_RESPONSE,

    BSTEERING_WNM_TYPE_INVALID
} BSTEERING_WNM_TYPE;

/* BSS Transition Management Response information that can be returned via netlink message */
struct bs_wnm_bstm_resp {
    /* status of the response to the request frame */
    u_int8_t status;
    /* BSTM Reject Reason Code Received from STA */
    u_int8_t reject_code;
    /* number of minutes that the STA requests the BSS to delay termination */
    u_int8_t termination_delay;
    /* BSSID of the BSS that the STA transitions to */
    u_int8_t target_bssid[IEEE80211_ADDR_LEN];
} ;

/**
 * Metadata and report contents about a Wireless Network
 * Management event
 */
struct bs_wnm_event_ind {
    /* The type of the wnm event: One of BSTEERING_WNM_TYPE.*/
    u_int32_t wnm_type;
    /* The token corresponding to the message.*/
    u_int8_t dialog_token;
    /* MAC address of the sending station.*/
    u_int8_t macaddr[IEEE80211_ADDR_LEN];
    /* The event data. Which member is valid is based on the
       wnm_type field.*/
    union {
        struct bs_wnm_bstm_resp bstm_resp;
    } data;
};

/**
 * Metadata about a client Tx rate threshold crossing event.
 */
struct bs_tx_rate_xing_threshold_ind {
    /* The MAC address of the client */
    u_int8_t client_addr[IEEE80211_ADDR_LEN];
    /* The Tx rate (in Kbps) */
    u_int32_t tx_rate;
    /* Flag indicating crossing direction */
    BSTEERING_XING_DIRECTION xing;
};

/**
 * Metadata about Tx power change on a VAP
 */
struct bs_tx_power_change_ind {
    /* The new Tx power */
    u_int16_t tx_power;
};

/**
 * STA stats per peer
 */
struct bs_sta_stats_per_peer {
    /* The MAC address of the client */
    u_int8_t client_addr[IEEE80211_ADDR_LEN];
    /* Uplink RSSI */
    u_int8_t rssi;
    /* PER */
    u_int8_t per;
    /* The Tx byte count */
    u_int64_t tx_byte_count;
    /* The Rx byte count */
    u_int64_t rx_byte_count;
    /* The Tx packet count */
    u_int32_t tx_packet_count;
    /* The Rx packet count */
    u_int32_t rx_packet_count;
    /* The last Tx rate (in Kbps) */
    u_int32_t tx_rate;
};

/**
 * Metadata for STA stats
 */
struct bs_sta_stats_ind {
    /* Number of peers for which stats are provided */
    u_int8_t peer_count;
    /* Stats per peer */
    struct bs_sta_stats_per_peer peer_stats[BSTEERING_MAX_PEERS_PER_EVENT];
};

/**
 * Metadate for STA SM Power Save mode update
 */
struct bs_node_smps_update_ind {
    /* The MAC address of the client */
    u_int8_t client_addr[IEEE80211_ADDR_LEN];
    /* Whether the client is operating in Static SMPS mode */
    u_int8_t is_static;
};

/**
 * Metadata for STA OP_MODE update
 */
struct bs_node_opmode_update_ind {
    /* The MAC address of the client */
    u_int8_t client_addr[IEEE80211_ADDR_LEN];
    /* Data rate related information supported by this client */
    ieee80211_bsteering_datarate_info_t datarate_info;
};

/**
 * Metadata about a STA that has associated
 */
struct bs_node_associated_ind {
    /* The MAC address of the client that is associated.*/
    u_int8_t client_addr[IEEE80211_ADDR_LEN];
    /* Set to 1 if this client supports BSS Transition Management */
    u_int8_t isBTMSupported : 1;
    /* Set to 1 if this client implements Radio Resource Manangement */
    u_int8_t isRRMSupported : 1;
    /* Band capability that tell if the client is 2.4G or 5G or both */
    u_int8_t band_cap : 2;
    /* Data rate related information supported by this client */
    ieee80211_bsteering_datarate_info_t datarate_info;
};

struct bs_node_disassociated_ind {
    /* The MAC address of the client that is associated.*/
    u_int8_t client_addr[IEEE80211_ADDR_LEN];
};

struct bs_channel_change_ind {
    u_int8_t channelChanged;
};
/**
 * Metadata about a client RSSI measurement crossed threshold for MAP.
 */
struct bs_rssi_xing_threshold_ind_map {	
    /* The MAC address of the client */	
    u_int8_t client_addr[IEEE80211_ADDR_LEN];
    /* The measured RSSI */
    u_int8_t rssi;
    /* Flag indicating if it crossed inactivity RSSI threshold */
    BSTEERING_XING_DIRECTION map_rssi_xing;
};
/**
 * Common event structure for all Netlink indications to userspace.
 */
typedef struct ath_netlink_bsteering_event {
    /* The type of the event: One of ATH_BSTEERING_EVENT.*/
    u_int32_t type;
    /* The OS-specific index of the VAP on which the event occurred.*/
    u_int32_t sys_index;
    /* The data for the event. Which member is valid is based on the
       type field.*/
    union {
        struct bs_probe_req_ind bs_probe;
        struct bs_node_associated_ind bs_node_associated;
        struct bs_node_disassociated_ind bs_node_disassociated;
        struct bs_activity_change_ind bs_activity_change;
        struct bs_auth_reject_ind bs_auth;
        struct bs_chan_utilization_ind bs_chan_util;
        struct bs_rssi_xing_threshold_ind bs_rssi_xing;
        struct bs_rssi_measurement_ind bs_rssi_measurement;
        struct bs_rrm_report_ind rrm_report;
        struct bs_wnm_event_ind wnm_event;
        struct bs_tx_rate_xing_threshold_ind bs_tx_rate_xing;
        struct bs_tx_rate_measurement_ind bs_tx_rate_measurement;
        struct bs_tx_power_change_ind bs_tx_power_change;
        struct bs_sta_stats_ind bs_sta_stats;
        struct bs_node_smps_update_ind smps_update;
        struct bs_node_opmode_update_ind opmode_update;
        struct bs_channel_change_ind bs_channel_change;
        struct bs_rrm_frame_report_ind rrm_frame_report;
        struct bs_rssi_xing_threshold_ind_map bs_rssi_xing_map;
    } data;
} ath_netlink_bsteering_event_t;

/**
 * Event types that are asynchronously generated by the band steering
 * module.
 */
typedef enum {
    /* Indication of utilization of the channel.*/
    ATH_EVENT_BSTEERING_CHAN_UTIL = 1,
    /* Indication that a probe request was received from a client.*/
    ATH_EVENT_BSTEERING_PROBE_REQ = 2,
    /* Indicated that a STA associated.*/
    ATH_EVENT_BSTEERING_NODE_ASSOCIATED = 3,
    /* Indication that an authentication frame was sent with a failure
       status code.*/
    ATH_EVENT_BSTEERING_TX_AUTH_FAIL = 4,
    /* Indication that a client changes from active to inactive or
       vice versa.*/
    ATH_EVENT_BSTEERING_CLIENT_ACTIVITY_CHANGE = 5,
    /* Indication when the client RSSI crosses above or below the
       configured threshold.*/
    ATH_EVENT_BSTEERING_CLIENT_RSSI_CROSSING = 6,
    /* Indication when a requested RSSI measurement for a specific
       client is available.*/
    ATH_EVENT_BSTEERING_CLIENT_RSSI_MEASUREMENT = 7,
    /* Indication when a 802.11k radio resource management report
       is received from a client.*/
    ATH_EVENT_BSTEERING_RRM_REPORT = 8,
    /* Indication when a 802.11v wireless network management (WNM) message
       is received from a client.*/
    ATH_EVENT_BSTEERING_WNM_EVENT = 9,
    /* Indication when the client Tx rate crosses above or below the
       configured threshold. */
    ATH_EVENT_BSTEERING_CLIENT_TX_RATE_CROSSING = 10,
    /* Indication when a VAP has stopped.
       Note: This is not the same as a VAP being brought down.  This will be seen
       in RE mode when the uplink STA interface disassociates. */
    ATH_EVENT_BSTEERING_VAP_STOP = 11,
    /* Indication when Tx power changes on a VAP. */
    ATH_EVENT_BSTEERING_TX_POWER_CHANGE = 12,
    /* Indication of new STA stats from firmware. */
    ATH_EVENT_BSTEERING_STA_STATS = 13,
    /* Indication of SM Power Save mode update for a client. */
    ATH_EVENT_BSTEERING_SMPS_UPDATE = 14,
    /* Indication of OP_MODE IE received from a client */
    ATH_EVENT_BSTEERING_OPMODE_UPDATE = 15,

    /* Indication when a 802.11k radio resource management frame report
       is received from a client.*/
    ATH_EVENT_BSTEERING_RRM_FRAME_REPORT = 16,
    /* Indication when the client RSSI crosses above or below the
       configured threshold for MAP*/
    ATH_EVENT_BSTEERING_MAP_CLIENT_RSSI_CROSSING = 17,

    /*  Events generated solely for debugging purposes. These are not
        intended for direct consumption by any algorithm components but are
        here to facilitate logging the raw data.*/
    ATH_EVENT_BSTEERING_DBG_CHAN_UTIL = 32,
    /* Raw RSSI measurement event used to facilitate logging.*/
    ATH_EVENT_BSTEERING_DBG_RSSI = 33,
    /* Raw Tx rate measurement event used to facilitate logging.*/
    ATH_EVENT_BSTEERING_DBG_TX_RATE = 34,
    /* Indication that an authentication is allowed due to Auth Allow flag
       set.*/
    ATH_EVENT_BSTEERING_DBG_TX_AUTH_ALLOW = 35,
} ATH_BSTEERING_EVENT;

/*
 * Station information block; the mac address is used
 * to retrieve other data like stats, unicast key, etc.
 */
struct ieee80211req_sta_info {
    u_int16_t       isi_len;                /* length (mult of 4) */
    u_int16_t       isi_freq;               /* MHz */
    u_int32_t       awake_time;             /* time is active mode */
    u_int32_t       ps_time;                /* time in power save mode */
    u_int32_t       isi_flags;      /* channel flags */
    u_int16_t       isi_state;              /* state flags */
    u_int8_t        isi_authmode;           /* authentication algorithm */
    int8_t          isi_rssi;
    int8_t          isi_min_rssi;
    int8_t          isi_max_rssi;
    u_int16_t       isi_capinfo;            /* capabilities */
    u_int8_t        isi_athflags;           /* Atheros capabilities */
    u_int8_t        isi_erp;                /* ERP element */
    u_int8_t        isi_ps;         /* psmode */
    u_int8_t        isi_macaddr[IEEE80211_ADDR_LEN];
    u_int8_t        isi_nrates;
    /* negotiated rates */
    u_int8_t        isi_rates[IEEE80211_RATE_MAXSIZE];
    u_int8_t        isi_txrate;             /* index to isi_rates[] */
    u_int32_t       isi_txratekbps; /* tx rate in Kbps, for 11n */
    u_int16_t       isi_ie_len;             /* IE length */
    u_int16_t       isi_associd;            /* assoc response */
    u_int16_t       isi_txpower;            /* current tx power */
    u_int16_t       isi_vlan;               /* vlan tag */
    u_int16_t       isi_txseqs[17];         /* seq to be transmitted */
    u_int16_t       isi_rxseqs[17];         /* seq previous for qos frames*/
    u_int16_t       isi_inact;              /* inactivity timer */
    u_int8_t        isi_uapsd;              /* UAPSD queues */
    u_int8_t        isi_opmode;             /* sta operating mode */
    u_int8_t        isi_cipher;
    u_int32_t       isi_assoc_time;         /* sta association time */
    struct timespec isi_tr069_assoc_time;   /* sta association time in timespec format */


    u_int16_t   isi_htcap;      /* HT capabilities */
    u_int32_t   isi_rxratekbps; /* rx rate in Kbps */
    /* We use this as a common variable for legacy rates
       and lln. We do not attempt to make it symmetrical
       to isi_txratekbps and isi_txrate, which seem to be
       separate due to legacy code. */
    /* XXX frag state? */
    /* variable length IE data */
    u_int8_t isi_maxrate_per_client; /* Max rate per client */
    u_int16_t   isi_stamode;        /* Wireless mode for connected sta */
    u_int32_t isi_ext_cap;              /* Extended capabilities */
    u_int8_t isi_nss;         /* number of tx and rx chains */
    u_int8_t isi_is_256qam;    /* 256 QAM support */
    u_int8_t isi_operating_bands : 2; /* Operating bands */
#if ATH_SUPPORT_EXT_STAT
    u_int8_t  isi_chwidth;            /* communication band width */
    u_int32_t isi_vhtcap;             /* VHT capabilities */
#endif
    u_int8_t isi_rx_nss;         /* number of rx chains */
    u_int8_t isi_tx_nss;         /* number of tx chains */
#if ATH_EXTRA_RATE_INFO_STA
    u_int8_t isi_tx_rate_mcs;
    u_int8_t isi_tx_rate_flags;
#endif

};


/*
 * Per/node (station) statistics available when operating as an AP.
 */
struct ieee80211_nodestats {
    u_int32_t    ns_rx_data;             /* rx data frames */
    u_int32_t    ns_rx_mgmt;             /* rx management frames */
    u_int32_t    ns_rx_ctrl;             /* rx control frames */
    u_int32_t    ns_rx_ucast;            /* rx unicast frames */
    u_int32_t    ns_rx_mcast;            /* rx multi/broadcast frames */
    u_int64_t    ns_rx_bytes;            /* rx data count (bytes) */
    u_int64_t    ns_last_per;            /* last packet error rate */
    u_int64_t    ns_rx_beacons;          /* rx beacon frames */
    u_int32_t    ns_rx_proberesp;        /* rx probe response frames */

    u_int32_t    ns_rx_dup;              /* rx discard 'cuz dup */
    u_int32_t    ns_rx_noprivacy;        /* rx w/ wep but privacy off */
    u_int32_t    ns_rx_wepfail;          /* rx wep processing failed */
    u_int32_t    ns_rx_demicfail;        /* rx demic failed */

    /* We log MIC and decryption failures against Transmitter STA stats.
       Though the frames may not actually be sent by STAs corresponding
       to TA, the stats are still valuable for some customers as a sort
       of rough indication.
       Also note that the mapping from TA to STA may fail sometimes. */
    u_int32_t    ns_rx_tkipmic;          /* rx TKIP MIC failure */
    u_int32_t    ns_rx_ccmpmic;          /* rx CCMP MIC failure */
    u_int32_t    ns_rx_wpimic;           /* rx WAPI MIC failure */
    u_int32_t    ns_rx_tkipicv;          /* rx ICV check failed (TKIP) */
    u_int32_t    ns_rx_decap;            /* rx decapsulation failed */
    u_int32_t    ns_rx_defrag;           /* rx defragmentation failed */
    u_int32_t    ns_rx_disassoc;         /* rx disassociation */
    u_int32_t    ns_rx_deauth;           /* rx deauthentication */
    u_int32_t    ns_rx_action;           /* rx action */
    u_int32_t    ns_rx_decryptcrc;       /* rx decrypt failed on crc */
    u_int32_t    ns_rx_unauth;           /* rx on unauthorized port */
    u_int32_t    ns_rx_unencrypted;      /* rx unecrypted w/ privacy */

    u_int32_t    ns_tx_data;             /* tx data frames */
    u_int32_t    ns_tx_data_success;     /* tx data frames successfully
                                            transmitted (unicast only) */
    u_int64_t    ns_tx_wme[4];           /* tx data per AC */
    u_int64_t    ns_rx_wme[4];           /* rx data per AC */
    u_int32_t    ns_tx_mgmt;             /* tx management frames */
    u_int32_t    ns_tx_ucast;            /* tx unicast frames */
    u_int32_t    ns_tx_mcast;            /* tx multi/broadcast frames */
    u_int64_t    ns_tx_bytes;            /* tx data count (bytes) */
    u_int64_t    ns_tx_bytes_success;    /* tx success data count - unicast only
                                            (bytes) */
    u_int32_t    ns_tx_probereq;         /* tx probe request frames */
    u_int32_t    ns_tx_uapsd;            /* tx on uapsd queue */
    u_int32_t    ns_tx_discard;          /* tx dropped by NIC */
    u_int32_t    ns_is_tx_not_ok;        /* tx not ok */
    u_int32_t    ns_tx_novlantag;        /* tx discard 'cuz no tag */
    u_int32_t    ns_tx_vlanmismatch;     /* tx discard 'cuz bad tag */

    u_int32_t    ns_tx_eosplost;         /* uapsd EOSP retried out */
    u_int32_t    ns_ps_discard;          /* ps discard 'cuz of age */

    u_int32_t    ns_uapsd_triggers;      /* uapsd triggers */
    u_int32_t    ns_uapsd_duptriggers;    /* uapsd duplicate triggers */
    u_int32_t    ns_uapsd_ignoretriggers; /* uapsd duplicate triggers */
    u_int32_t    ns_uapsd_active;         /* uapsd duplicate triggers */
    u_int32_t    ns_uapsd_triggerenabled; /* uapsd duplicate triggers */
    u_int32_t    ns_last_tx_rate;         /* last tx data rate */
    u_int32_t    ns_last_rx_rate;         /* last received data frame rate */
    u_int32_t    ns_is_tx_nobuf;
    u_int32_t    ns_last_rx_mgmt_rate;    /* last received mgmt frame rate */
    int8_t       ns_rx_mgmt_rssi;         /* mgmt frame rssi */
    u_int32_t    ns_dot11_tx_bytes;
    u_int32_t    ns_dot11_rx_bytes;
#if ATH_SUPPORT_EXT_STAT
    u_int32_t    ns_tx_bytes_rate;         /* transmitted bytes for last one second */
    u_int32_t    ns_tx_data_rate;          /* transmitted data for last one second */
    u_int32_t    ns_rx_bytes_rate;         /* received bytes for last one second */
    u_int32_t    ns_rx_data_rate;          /* received data for last one second */
    u_int32_t    ns_tx_bytes_success_last; /* last time tx bytes */
    u_int32_t    ns_tx_data_success_last;  /* last time tx packets */
    u_int32_t    ns_rx_bytes_last;         /* last time rx bytes */
    u_int32_t    ns_rx_data_last;          /* last time rx packets */
#endif

    /* MIB-related state */
    u_int32_t    ns_tx_assoc;            /* [re]associations */
    u_int32_t    ns_tx_assoc_fail;       /* [re]association failures */
    u_int32_t    ns_tx_auth;             /* [re]authentications */
    u_int32_t    ns_tx_auth_fail;        /* [re]authentication failures*/
    u_int32_t    ns_tx_deauth;           /* deauthentications */
    u_int32_t    ns_tx_deauth_code;      /* last deauth reason */
    u_int32_t    ns_tx_disassoc;         /* disassociations */
    u_int32_t    ns_tx_disassoc_code;    /* last disassociation reason */
    u_int32_t    ns_psq_drops;           /* power save queue drops */
    u_int32_t    ns_rssi_chain[MAX_CHAINS]; /* Ack RSSI per chain */
    /* IQUE-HBR related state */
#ifdef ATH_SUPPORT_IQUE
    u_int32_t   ns_tx_dropblock;    /* tx discard 'cuz headline block */
#endif
    u_int32_t   inactive_time;
    u_int32_t   ns_tx_mu_blacklisted_cnt;  /* number of time MU tx has been blacklisted */
    u_int64_t   ns_excretries[WME_NUM_AC]; /* excessive retries */
#if UMAC_SUPPORT_STA_STATS_ENHANCEMENT
    u_int64_t    ns_rx_ucast_bytes;      /* tx bytes of unicast frames */
    u_int64_t    ns_rx_mcast_bytes;      /* tx bytes of multi/broadcast frames */
    u_int64_t    ns_tx_ucast_bytes;      /* tx bytes of unicast frames */
    u_int64_t    ns_tx_mcast_bytes;      /* tx bytes of multi/broadcast frames */
#endif
    u_int64_t    ns_rx_mpdus;            /* Number of rs mpdus */
    u_int64_t    ns_rx_ppdus;            /* Number of ppdus */
    u_int64_t    ns_rx_retries;          /* Number of rx retries */
};

/*
 * Retrieve per-node statistics.
 */
struct ieee80211req_sta_stats {
    union {
        /* NB: explicitly force 64-bit alignment */
        u_int8_t    macaddr[IEEE80211_ADDR_LEN];
        u_int64_t   pad;
    } is_u;
    struct ieee80211_nodestats is_stats;
};

struct atfcntbl{
    u_int8_t     ssid[IEEE80211_NWID_LEN+1];
    u_int8_t     sta_mac[IEEE80211_ADDR_LEN];
    u_int32_t    value;
    u_int8_t     info_mark;   /*0--vap, 1-sta*/
    u_int8_t     assoc_status;     /*1--Yes, 0 --No*/
    u_int8_t     all_tokens_used;  /*1--Yes, 0 --No*/
    u_int32_t    cfg_value;
    u_int8_t     grpname[IEEE80211_NWID_LEN+1];
};

#define ATF_ACTIVED_MAX_CLIENTS   50
#define ATF_CFG_NUM_VDEV          16

struct atftable{
    u_int16_t         id_type;
    struct atfcntbl   atf_info[ATF_ACTIVED_MAX_CLIENTS+ATF_CFG_NUM_VDEV];
    u_int16_t         info_cnt;
    u_int8_t          atf_status;
    u_int32_t         busy;
    u_int32_t         atf_group;
    u_int8_t          show_per_peer_table;
};
/** 
 * Map Configurable parameteres
 */
typedef struct ieee80211_bsteering_map_param_t {
  /* STA metrics reporting RSSI threshold */
  u_int8_t rssi_threshold;
  /* STA Metrics Reporting RSSI Hysteresis Margin Override */ 
  u_int8_t rssi_hysteresis;
} ieee80211_bsteering_map_param_t;
#endif      //wlanif_driver__h
