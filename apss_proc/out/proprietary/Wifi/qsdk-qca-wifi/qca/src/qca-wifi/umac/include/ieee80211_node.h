/*
 *
 * Copyright (c) 2011-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011-2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * Copyright (c) 2008 Atheros Communications Inc.
 * All Rights Reserved.
 *
 *
 */

#ifndef _ATH_STA_IEEE80211_NODE_H
#define _ATH_STA_IEEE80211_NODE_H

#include <osdep.h>
#include <sys/queue.h>
#include <ieee80211_admctl.h>
#include <osdep.h>
#include <qdf_time.h>
#include <ieee80211_power.h>
#include <umac_lmac_common.h>
#include <ieee80211_ald.h>
#include <ieee80211_ioctl.h>
#include <include/wlan_vdev_mlme.h>

#ifdef QCA_SUPPORT_CP_STATS
#include <wlan_cp_stats_ic_utils_api.h>
#endif

/* Forward declarations */
struct ieee80211com;
struct ieee80211vap;
struct MU_CAP_WAR;
struct ieee80211_node_table;
struct ieee80211_rsnparms;
#ifdef OBSS_PD
struct ieee80211_spatial_reuse_handle;
#endif

#ifndef ATH_USB
typedef rwlock_t   ieee80211_node_lock_t;
typedef spinlock_t ieee80211_node_state_lock_t;

#define IEEE80211_NODE_STATE_LOCK_INIT(_node)     spin_lock_init(&(_node)->ni_state_lock);
#define IEEE80211_NODE_STATE_LOCK_DESTROY(_node)  spin_lock_destroy(&(_node)->ni_state_lock)
#define IEEE80211_NODE_STATE_LOCK(_node)          spin_lock(&(_node)->ni_state_lock)
#define IEEE80211_NODE_STATE_LOCK_BH(_node)       spin_lock_dpc(&(_node)->ni_state_lock)
#define IEEE80211_NODE_STATE_UNLOCK(_node)        spin_unlock(&(_node)->ni_state_lock)
#define IEEE80211_NODE_STATE_UNLOCK_BH(_node)     spin_unlock_dpc(&(_node)->ni_state_lock)
#define IEEE80211_NODE_STATE_PAUSE_LOCK(_node)         IEEE80211_NODE_STATE_LOCK(_node)
#define IEEE80211_NODE_STATE_PAUSE_UNLOCK(_node)       IEEE80211_NODE_STATE_UNLOCK(_node)

#else
typedef usb_readwrite_lock_t ieee80211_node_lock_t;
typedef usblock_t            ieee80211_node_state_lock_t;

#define IEEE80211_NODE_STATE_LOCK_INIT(_node)    IEEE80211_NODE_STATE_PAUSE_LOCK_INIT(_node); \
                                                 OS_USB_LOCK_INIT(&(_node)->ni_state_lock)
#define IEEE80211_NODE_STATE_LOCK_DESTROY(_node)   IEEE80211_NODE_STATE_PAUSE_LOCK_DESTROY(_node) ;\
                                         OS_USB_LOCK_DESTROY(&(_node)->ni_state_lock)

#define IEEE80211_NODE_STATE_LOCK(_node)          OS_USB_LOCK(&(_node)->ni_state_lock)
#define IEEE80211_NODE_STATE_LOCK_BH(_node)       IEEE80211_NODE_STATE_LOCK(_node)
#define IEEE80211_NODE_STATE_UNLOCK(_node)        OS_USB_UNLOCK(&(_node)->ni_state_lock)
#define IEEE80211_NODE_STATE_UNLOCK_BH(_node)     IEEE80211_NODE_STATE_UNLOCK(_node)
#define IEEE80211_NODE_STATE_PAUSE_LOCK_INIT(_node)     spin_lock_init(&(_node)->ni_pause_lock);
#define IEEE80211_NODE_STATE_PAUSE_LOCK_DESTROY(_node)  spin_lock_destroy(&(_node)->ni_pause_lock)
#define IEEE80211_NODE_STATE_PAUSE_LOCK(_node)          spin_lock_dpc(&(_node)->ni_pause_lock)
#define IEEE80211_NODE_STATE_PAUSE_UNLOCK(_node)        spin_unlock_dpc(&(_node)->ni_pause_lock)


#endif

#define WDS_AGING_COUNT 2
#define WDS_AGING_TIME 60000   /* in ms */
#define WDS_AGING_TIMER_VAL (WDS_AGING_TIME/2)

struct  ieee80211_wnm_node {
    struct ieee80211_tfsreq         *tfsreq;
    struct ieee80211_tfsrsp         *tfsrsp;
    struct ieee80211_fmsreq         *fmsreq;
    struct ieee80211_fmsrsp         *fmsrsp;
    TAILQ_HEAD(, ieee80211_fmsreq_active_element) fmsreq_act_head;
    u_int8_t                        *timbcast_ie; /* captured TIM ie */
    u_int32_t                       timbcast_status;
    u_int8_t                        timbcast_interval;
    u_int8_t                        timbcast_dialogtoken;
    systime_t                       last_rcvpkt_tstamp;   /* to capture receive packet time */
};

struct ni_persta_key {   // for adhoc mcast rx
    struct ieee80211_key nips_hwkey; // allocated clear key used to hand frame to sw
    struct ieee80211_key nips_swkey[IEEE80211_WEP_NKID]; // key used by sw to decrypt
};

#if IEEE80211_DEBUG_REFCNT
#define NUM_TRACE_BUF           (1 << 4)
struct node_trace_buf {
    const char  *func[NUM_TRACE_BUF];
    int         line[NUM_TRACE_BUF];
    int         count[NUM_TRACE_BUF];
    atomic_t    index;
};

struct node_trace {
    struct node_trace_buf inc;
    struct node_trace_buf dec;
    atomic_t         refcnt;
};

struct node_trace_all {
    struct node_trace aponly;
    struct node_trace wireless;
    struct node_trace bs;
    struct node_trace node;
    struct node_trace base;
    struct node_trace crypto;
    struct node_trace if_lmac;
    struct node_trace mlme;
    struct node_trace txrx;
    struct node_trace wds;
    struct node_trace misc;
};
#endif

struct recv_auth_params_defer {
    u_int16_t vdev_id;
    u_int16_t algo;
    u_int16_t seq;
    u_int16_t status_code;
    u_int8_t *challenge;
    u_int8_t challenge_length;
    wbuf_t wbuf;
    struct ieee80211_rx_status rs;
};

typedef struct _ni_ext_caps {
    u_int32_t   ni_ext_capabilities;
    u_int32_t   ni_ext_capabilities2;
    u_int8_t    ni_ext_capabilities3;
    u_int8_t    ni_ext_capabilities4;
} ni_ext_caps;

/*
 * Node information. A node could represents a BSS in infrastructure network,
 * or an ad-hoc station in IBSS mode, or an associated station in HOSTAP mode.
 */
typedef struct ieee80211_node {
    TAILQ_ENTRY(ieee80211_node) ni_list; /* node table list */
    LIST_ENTRY(ieee80211_node)  ni_hash; /* hash list */
    TAILQ_ENTRY(ieee80211_node)   nodeleave_list;
	LIST_ENTRY(ieee80211_node)  ni_dump_list; /* node dump list */
#if IEEE80211_DEBUG_NODELEAK
    TAILQ_ENTRY(ieee80211_node) ni_alloc_list; /* all allocated nodes */
#endif
    u_int32_t previous_ps_time; /* time when power save state changed last */
    u_int32_t awake_time;       /* total time node is in active state since assoc time */
    u_int32_t ps_time;         /* total time node is in power save state since assoc time */

#if IEEE80211_DEBUG_REFCNT
    struct node_trace_all *trace;
#endif

    struct ieee80211_node_table *ni_table;
    struct ieee80211vap     *ni_vap;
    struct ieee80211com     *ni_ic;
    struct ieee80211_node   *ni_bss_node;
    ieee80211_node_state_lock_t    ni_state_lock;   /* Node for the WAR for bug 58187 */
#ifdef ATH_USB
    spinlock_t            ni_pause_lock ;           /*htc specific as ni_state_lock is semaphore in case of HTC */
#endif
    atomic_t                ni_refcnt;
    u_int8_t                ni_authmode;            /* authentication mode */
    u_int8_t                ni_authalg;             /* authentication algorithm */
    u_int8_t                ni_authstatus;          /* authentication response status */
    u_int32_t               ni_flags;               /* special-purpose state */
#define IEEE80211_NODE_AUTH     0x00000001          /* authorized for data */
#define IEEE80211_NODE_QOS      0x00000002          /* QoS enabled */
#define IEEE80211_NODE_ERP      0x00000004          /* ERP enabled */
#define IEEE80211_NODE_HT       0x00000008          /* HT enabled */
/* NB: this must have the same value as IEEE80211_FC1_PWR_MGT */
#define IEEE80211_NODE_PWR_MGT  0x00000010          /* power save mode enabled */
#define IEEE80211_NODE_TSC_SET  0x00000020           /* keytsc for node has already been updated */
#define IEEE80211_NODE_UAPSD    0x00000040          /* U-APSD power save enabled */
#define IEEE80211_NODE_UAPSD_TRIG 0x00000080        /* U-APSD triggerable state */
#define IEEE80211_NODE_UAPSD_SP 0x00000100          /* U-APSD SP in progress */
#define IEEE80211_NODE_ATH      0x00000200          /* Atheros Owl or follow-on device */
#define IEEE80211_NODE_OWL_WDSWAR 0x00000400        /* Owl WDS workaround needed*/
#define IEEE80211_NODE_WDS      0x00000800          /* WDS link */
#define	IEEE80211_NODE_NOAMPDU  0x00001000          /* No AMPDU support */
#define IEEE80211_NODE_WEPTKIPAGGR 0x00002000       /* Atheros proprietary wep/tkip aggregation support */
#define IEEE80211_NODE_WEPTKIP  0x00004000
#define IEEE80211_NODE_TEMP     0x00008000          /* temp node (not in the node table) */
#define IEEE80211_NODE_11NG_VHT_INTEROP_AMSDU_DISABLE   0x00010000  /* 2.4ng VHT interop AMSDU disabled */
#define IEEE80211_NODE_40MHZ_INTOLERANT    0x00020000  /* 40 MHz Intolerant  */
#define IEEE80211_NODE_PAUSED   0x00040000          /* node is  paused*/
#define IEEE80211_NODE_EXTRADELIMWAR 0x00080000
#define IEEE80211_NODE_NAWDS 0x00100000          /* node is an NAWDS repeater */
#define IEEE80211_NODE_REQ_20MHZ     0x00400000      /* 20 MHz requesting node */
#define IEEE80211_NODE_ATH_PAUSED 0x00800000         /* all the tid queues in ath layer are paused*/
#define IEEE80211_NODE_UAPSD_CREDIT_UPDATE 0x01000000  /*Require credit update*/
#define IEEE80211_NODE_KICK_OUT_DEAUTH     0x02000000  /*Require send deauth when h/w queue no data*/
#define IEEE80211_NODE_RRM                 0x04000000  /* RRM enabled node */
#if ATH_SUPPORT_WIFIPOS
#define IEEE80211_NODE_WAKEUP              0x08000000  /* Wakeup node */
#endif
#define IEEE80211_NODE_VHT                 0x10000000  /* VHT enabled node */
/* deauth/Disassoc wait for node cleanup till frame goes on
   air and tx feedback received */
#define IEEE80211_NODE_DELAYED_CLEANUP   0x20000000
#define IEEE80211_NODE_EXT_STATS           0x40000000  /* Extended stats enabled node */
#define IEEE80211_NODE_LEAVE_ONGOING       0x80000000  /* Prevent _ieee80211_node_leave() from reentry */
    u_int32_t ni_ext_flags;
#define IEEE80211_NODE_BSTEERING_CAPABLE     0x000000001 /* band steering is enabled for this node */
#define IEEE80211_LOCAL_MESH_PEER            0x000000002 /* node is a local mesh peer */
#define IEEE80211_NODE_NON_DOTH_STA          0x000000004 /* Non doth supporting STA */
#define IEEE80211_NODE_HE                    0x000000008 /* HE enabled node */
#define IEEE80211_NODE_DELAYED_CLEANUP_FAIL  0x000000010 /* delayed cleanup of node failed. do cleanup in caller context */
#define IEEE80211_NODE_ASSOC_REQ             0x000000020 /* Assoc req received */
#define IEEE80211_NODE_ASSOC_RESP            0x000000040 /* Assoc WMI event to FW */
#define IEEE80211_NODE_TWT_REQUESTER         0x000000080 /* TWT Requester */
#define IEEE80211_NODE_TWT_RESPONDER         0x000000100 /* TWT Responder */
#define IEEE80211_NODE_BCN_MEASURE_SUPPORT   0x000000200 /* STA supports either active or passive beacon measurement */

    u_int8_t                ni_ath_flags;       /* Atheros feature flags */
/* NB: These must have the same values as IEEE80211_ATHC_* */
#define IEEE80211_NODE_TURBOP   0x01          /* Turbo prime enable */
#define IEEE80211_NODE_AR       0x10          /* AR capable */
#define IEEE80211_NODE_BOOST    0x80


#define IEEE80211_NODE_WHC_APINFO_WDS    0x01
#define IEEE80211_NODE_WHC_APINFO_SON    0x02

#if QCN_IE
    u_int8_t                ni_qcn_version_flag;
    u_int8_t                ni_qcn_subver_flag;
    u_int8_t                ni_qcn_tran_rej_code;
    u_int8_t                *ni_qcn_ie; /* captured QCN ie */
#endif

#if WDS_VENDOR_EXTENSION
    u_int8_t                ni_wds_tx_policy;  /* permissible ucast/mcast framing (3-addr or 4-addr) to wds peer */
#define WDS_POLICY_TX_UCAST_4ADDR        0x01
#define WDS_POLICY_TX_MCAST_4ADDR        0x02
#define WDS_POLICY_TX_DEFAULT            0x03
#define WDS_POLICY_TX_MASK               0x03
#endif

    u_int16_t               ni_ath_defkeyindex; /* Atheros def key index */
#define IEEE80211_INVAL_DEFKEY  0x7FFF

    u_int16_t               ni_associd; /* association id */
    u_int32_t               ni_scangen;
    systick_t               ni_assocuptime;  /* association up time */
    systick_t               ni_assocstarttime;   /* association start time */
    systick_t               ni_assoctime;        /* association process time */
    u_int16_t               ni_assocstatus;  /* association status code */
    u_int16_t               ni_p2p_assocstatus;    /* P2P assoc status code */
    u_int16_t               ni_txpower; /* current transmit power */

    u_int16_t               ni_vlan;    /* vlan tag */
    u_int32_t               *ni_challenge;  /* shared-key challenge */
    u_int8_t                *ni_wpa_ie;     /* captured WPA/RSN ie */
    u_int8_t                *ni_wps_ie;     /* captured WSC ie */
    u_int8_t                 ni_cc[3];      /* captured country code */

    u_int8_t                *ni_ath_ie; /* captured Atheros ie */
    u_int8_t                *ni_wme_ie; /* captured WME ie */
    u_int8_t                *ni_mbo_ie; /* captured MBO ie */
    u_int8_t                *ni_supp_op_class_ie; /* captured supported operating class ie */
    u_int8_t                *ni_supp_chan_ie; /* captured 802.11h supported channel ie */

    u_int8_t                ni_wme_miss_threshold; /* wme_miss threshold */

    u_int16_t               ni_last_rxauth_seq;  /* last rx auth seq number*/
    systime_t               ni_last_auth_rx_time; /* last rx auth time stamp*/
    u_int16_t               ni_txseqs[IEEE80211_TID_SIZE];      /* tx seq per-tid */
    u_int16_t               ni_rxseqs[IEEE80211_TID_SIZE+1];/* rx seq previous per-tid,
                                                             * the additional one is for check seq on
                                                             * management frames. */
    u_int16_t               ni_last_rxseqs[IEEE80211_TID_SIZE+1];/* rx seq 2nd last(previous) per-tid,
                                                                  * the additional one is for check seq on
                                                                  * management frames. */
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
    struct ieee80211_rsnparms ni_rsn;       /* RSN/WPA parameters */
#endif
    struct ieee80211_key    ni_ucastkey;    /* unicast key */
    struct ni_persta_key    *ni_persta;        /* For adhoc mcast rx */
    int                     ni_rxkeyoff;    /* Receive key offset */
#if ATH_SUPPORT_WEP_MBSSID
    struct ieee80211_wep_mbssid ni_wep_mbssid;
#endif /*ATH_SUPPORT_WEP_MBSSID*/

    /*support for WAPI: keys for WAPI*/
#if ATH_SUPPORT_WAPI
	int ni_wkused;
	u32 ni_wapi_rekey_pkthresh;  /*wapi packets threshold for rekey, unicast or multicast depending on node*/
#endif

    /* 11n Capabilities */
    u_int16_t               ni_htcap;              /* HT capabilities */
    u_int32_t               ni_maxampdu;           /* maximum rx A-MPDU length */
    u_int32_t               ni_mpdudensity;        /* MPDU density in nano-sec */

#ifdef ATH_SUPPORT_TxBF
    union ieee80211_hc_txbf ni_txbf;               /* txbf capabilities */
#endif


    u_int8_t                ni_obssnonhtpresent;   /* OBSS non-HT STA present */
    u_int8_t                ni_txburstlimit;       /* TX burst limit */
    u_int8_t                ni_nongfpresent;       /* Non-green field devices present */
    u_int8_t                ni_streams;            /* number of streams supported */
    u_int8_t                ni_rxstreams;          /* number of rx streams supported */
    u_int8_t                ni_txstreams;          /* number of tx streams supported */
    /* 11n information */
    enum ieee80211_cwm_width ni_chwidth;        /* recommended tx channel width */
    u_int8_t                ni_newchwidth;      /* channel width changed */
    int8_t                  ni_extoffset;       /* recommended ext channel offset */
    u_int8_t                ni_updaterates;     /* update rate table on SM power save */
#define	IEEE80211_NODE_SM_EN                    1
#define	IEEE80211_NODE_SM_PWRSAV_STAT	        2
#define	IEEE80211_NODE_SM_PWRSAV_DYN	        4
#define	IEEE80211_NODE_RATECHG                  8

    /* activity indicators */
    systime_t               ni_beacon_rstamp; /* host timestamp of received beacon and probes */
    u_int32_t               ni_probe_ticks;   /* inactivity mark count */

    u_int8_t		        ni_weptkipaggr_rxdelim; /* number of delimeters required to receive wep/tkip w/aggr */

    /* in activity indicators for AP mode */
    u_int16_t               ni_inact;       /* inactivity mark count */
    u_int16_t               ni_inact_reload;/* inactivity reload value */
    u_int32_t               ni_session;     /* STA's session time */
    u_int8_t                ni_min_txpower; /* minimum TX power the STA supports */
    u_int8_t                ni_max_txpower; /* maximum TX power the STA supports */
#define ATH_TX_MAX_CONSECUTIVE_XRETRIES     50 /* sta gets kicked out after this */
    /* kick out STA when excessive retries occur */
    u_int16_t               ni_consecutive_xretries;

    /* hardware, not just beacon and probes */
    u_int8_t                ni_rssi;    /* recv ssi */
    u_int8_t                ni_rssi_min; /* min rssi */
    u_int8_t                ni_rssi_max; /* max rssi */
    int8_t                  ni_abs_rssi; /* absolute RSSI */
    u_int8_t                ni_macaddr[IEEE80211_ADDR_LEN]; /* MAC address */
    u_int8_t                ni_bssid[IEEE80211_ADDR_LEN]; /* BSSID */

    /* beacon, probe response */
    union {
        u_int8_t            data[8];
        u_int64_t           tsf;
    } ni_tstamp;                        /* from last rcv'd beacon */

    u_int16_t               ni_intval;  /* beacon interval */
    u_int16_t               ni_capinfo; /* negociated capabilities */

    u_int16_t               ni_ext_caps;/* exteneded node capabilities */
#define IEEE80211_NODE_C_QOS    0x0002  /* Wmm capable */
#define IEEE80211_NODE_C_UAPSD  0x0004  /* U-APSD capable */

    u_int8_t                ni_esslen;
    u_int8_t                ni_essid[IEEE80211_NWID_LEN+1];

    struct ieee80211_rateset ni_rates;   /* negotiated rate set */
    struct ieee80211_rateset ni_htrates; /* negotiated ht rate set */
    struct ieee80211_ath_channel *ni_chan;

    u_int8_t                ni_erp;     /* ERP from beacon/probe resp */

    u_int32_t               ni_wait0_ticks;       /* ticks that we stay in dot11_assoc_state_zero state */
    u_int8_t                ni_dtim_period;       /* DTIM period */
    u_int8_t                ni_dtim_count;        /* DTIM count for last bcn */
    u_int16_t               ni_lintval;           /* listen interval */
    u_int8_t                ni_minbasicrate;      /* Min basic rate */

    systime_t               ss_last_data_time;    /* last time data RX/TX time */
#if ATH_SUPPORT_WIFIPOS
    u_int32_t               last_rx_desc_tsf;
#endif
    u_int16_t               ni_pause_count;

    /* To store MBO attributes and channels supported by STA */
    struct ieee80211_mbo_attributes ni_mbo;
    struct ieee80211_supp_op_class ni_supp_op_cl;

    /* power save queues */
    IEEE80211_NODE_POWERSAVE_QUEUE(ni_dataq)
    IEEE80211_NODE_POWERSAVE_QUEUE(ni_mgmtq)

    /* AP side UAPSD */
    u_int8_t                ni_uapsd;	/* U-APSD per-node flags matching WMM STA Qos Info field */
    u_int8_t                ni_uapsd_maxsp; /* maxsp from flags above */
    u_int8_t                ni_uapsd_ac_trigena[WME_NUM_AC];    /* U-APSD per-node flags matching WMM STA Qos Info field */
    u_int8_t                ni_uapsd_ac_delivena[WME_NUM_AC];    /* U-APSD per-node flags matching WMM STA Qos Info field */
    int8_t                  ni_uapsd_dyn_trigena[WME_NUM_AC];    /* U-APSD per-node flags matching WMM STA Qos Info field */
    int8_t                  ni_uapsd_dyn_delivena[WME_NUM_AC];    /* U-APSD per-node flags matching WMM STA Qos Info field */
    ieee80211_admctl_priv_t ni_admctl_priv;         /* opaque handle with admctl private info */

    u_int8_t                ni_assoc_state; /* IBSS only */
#define IEEE80211_NODE_ADHOC_STATE_ZERO             0
#define IEEE80211_NODE_ADHOC_STATE_UNAUTH_UNASSOC   1
#define IEEE80211_NODE_ADHOC_STATE_AUTH_ASSOC       2

    u_int8_t                ni_chanswitch_tbtt;
#ifdef ATH_SUPPORT_TxBF
    /* beam forming flag */
    u_int8_t                ni_bf_update_cv        : 1, /* 1: request CV update */
                            ni_explicit_noncompbf  : 1, /* 1: set explicit non-compressed bf */
                            ni_explicit_compbf     : 1, /* 1: set explicit compressed bf*/
                            ni_implicit_bf         : 1, /* 1: set implicit bf */
                            Calibration_Done       : 1,
                            ni_rpt_received        : 1, /* 1: V/CV report recieved */
                            ni_txbf_timer_initialized   : 1,    /* 1: txbf related timer initialized */
                            ni_hw_cv_requested     : 1,     /* 1: cv requested by incorrect HW status*/
                            ni_allow_cv_update     : 1;     /* 1: sw time out , allow cv request */

    u_int8_t                ni_mmss;
    u_int32_t               ni_sw_cv_timeout;
    os_timer_t              ni_cv_timer;
    os_timer_t              ni_report_timer;

    u_int32_t               ni_cvtstamp;
    u_int8_t                ni_cvretry;
#endif

    struct ieee80211vap     *ni_wdsvap;     /* associated WDS vap */
#if ATH_SUPPORT_IQUE
    u_int8_t	ni_hbr_block;
    u_int32_t	ni_ique_flag;
#endif
#ifdef ATH_SUPPORT_P2P
    u_int8_t    ni_p2p_awared;
#endif

#if UMAC_SUPPORT_RRM
    u_int8_t   ni_rrmreq_type;
    u_int8_t   ni_rrmlci_loc; /* RRM LCI request location subject */
    void       *ni_rrm_stats; /* RRM statistics */
    u_int8_t   rrm_dialog_token;
    u_int8_t   lm_dialog_token;
    u_int8_t   nr_dialog_token;
    u_int8_t   br_measure_token;
    u_int8_t   chload_measure_token;
    u_int8_t   stastats_measure_token;
    u_int8_t   nhist_measure_token;
    u_int8_t   cca_measure_token;
    u_int8_t   rpihist_measure_token;
    u_int8_t   tsm_measure_token;
    u_int8_t   frame_measure_token;
    u_int8_t   lci_measure_token;
    u_int8_t   ni_is_rrm_ftmrr; /* If node is FTMRR enabled */
#endif
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
    struct ath_ni_linkdiag ni_ald;
#endif
#ifdef ATH_SUPPORT_IBSS_NETLINK_NOTIFICATION
    u_int8_t	ni_rssi_class;
#endif

    /* first word of extended capabilities read from the association request frame */
    /* Note: Next 2 bytes currently not used, so not copied into the node structure yet */
    ni_ext_caps ext_caps;

#if UMAC_SUPPORT_WNM
    struct ieee80211_wnm_node       *ni_wnm;
    u_int8_t wnm_bss_idle_option; /* proctedted and non protected option */
#endif

#if UMAC_SUPPORT_PROXY_ARP
#define IEEE80211_NODE_IPV6_MAX 7
    LIST_ENTRY(ieee80211_node) ni_ipv4_hash; /* ipv4 hash list */
    uint32_t ni_ipv4_addr;
    uint32_t ni_ipv4_lease_timeout;
    uint8_t ni_ipv6_addr[IEEE80211_NODE_IPV6_MAX][16];
    int ni_ipv6_nidx; /* index of the next IPv6 address for this node */
#endif
    u_int8_t   ni_160bw_requested; /* Whether 160/80+80 MHz was originally requested by STA */
    u_int8_t   ps_state;
    u_int8_t ni_maxrate; /* Max Rate Per STA */
    u_int8_t ni_maxrate_legacy;
    u_int8_t ni_maxrate_ht;
    u_int8_t ni_maxrate_vht; /* b0-b3: mcs idx; b4-b7: # streams */
    u_int32_t   ni_vhtcap;        /* VHT capability */
    u_int16_t   ni_tx_vhtrates;   /* Negotiated Tx VHT rates */
    u_int8_t    ni_higher_vhtmcs_supp; /* Support of VHT MCS 10/11 */
    u_int16_t   ni_tx_max_rate;   /* Max VHT Tx Data rate */
    u_int16_t   ni_rx_vhtrates;   /* Negotiated Rx VHT rates */
    u_int16_t   ni_rx_max_rate;   /* Max VHT Rx Data rate */
    u_int16_t   ni_phymode;       /* Phy mode */
    bool        ni_pspoll;
#ifdef ATH_SUPPORT_QUICK_KICKOUT
    bool        ni_kickout;
#endif
#if ATH_POWERSAVE_WAR
    systime_t   ni_pspoll_time;      /* absolute system time; not TSF */
#endif
    IEEE80211_REASON_CODE ni_reason_code; /* disassoc reason code for sending to hostapd */
    u_int32_t    ni_fixed_rate;    /* STA's fixed data rate */
#if ATH_SUPPORT_HS20
    u_int8_t ni_qosmap_enabled;
#endif
    u_int32_t   ni_bw160_nss;        /* NSS used for 160MHz BW */
    u_int32_t   ni_bw80p80_nss;      /* NSS used for 80+80Mhz BW */
    bool        ni_prop_ie_used;     /* prop IE used for NSS Signaling */
#if ATH_SUPPORT_NAC
    u_int8_t    is_nac;
#endif
#if DBG_LVL_MAC_FILTERING
    u_int8_t    ni_dbgLVLmac_on;   /* flag to enable/disable debug level mac filtering for this node */
#endif
    u_int32_t ni_peer_chain_rssi;
    systime_t   ni_last_assoc_rx_time; /* last rx assoc time stamp*/
    u_int8_t    ni_vhtintop_subtype; /*vhtinterop flag that has the VHTSUBTYPE*/
    struct noise_stats *ni_noise_stats; /* pointer to the structure to store the noise,min,max and median value */
    u_int8_t    dedicated_client;
    u_int8_t    ni_mu_vht_cap;
    u_int8_t    ni_mu_dedicated;
    bool        ni_node_esc;
    struct wlan_objmgr_peer *peer_obj; /* UMAC peer object */
    struct recv_auth_params_defer auth_params;
    u_int8_t auth_inprogress;
    struct ieee80211_he_handle    ni_he; /* Node 11ax HE High Efficiency handle */
#ifdef OBSS_PD
    struct ieee80211_spatial_reuse_handle ni_srp;
#endif
    atomic_t  ni_fw_peer_delete_rsp_pending; /* atomic variable to track delete responde pending from FW */
    atomic_t  ni_node_preserved; /* atomic variable to track delete responde pending from FW */
    u_int8_t  ni_operating_bands : 2;/* Operating bands if sta is 2.4G or 5G or both
                                      * 0th bit - 2.4G band (IEEE80211_2G_BAND)
                                     * 1st bit - 5G band (IEEE80211_5G_BAND) */
    bool        ni_ext_nss_capable;    /* Node's capability to understand EXT NSS Signaling */
    u_int8_t    ni_ext_nss_support;    /* Node's support for EXT NSS Signaling */
    bool        is_ft_reassoc;
    u_int8_t    ni_wds_rx_policy;
    atomic_t    ni_auth_tx_completion_pending; /* atomic variable to check AUTH frame TX completion
                                                  pending or not */
    atomic_t  ni_logi_deleted; /* atomic variable to track logical deletion */
#if MESH_MODE_SUPPORT
    #define IEEE80211_SE_FLAG_IS_MESH       0x1
    u_int8_t    ni_meshpeer_timeout_cnt;
    u_int8_t    ni_mesh_flag;            /* general flag for se */
    u_int32_t   ni_mesh_bcn_ie_chksum;   /* checksum of interested beacon IEs */
#endif
#if DBDC_REPEATER_SUPPORT
    u_int8_t    is_extender_client;
#endif
     u_int8_t    ni_first_channel;
     u_int8_t    ni_nr_channels;
     u_int8_t    ni_omn_chwidth;        /* Channel width selected by the op mode notify IE */
     u_int32_t   ni_last_bcn_rssi;
     u_int32_t   ni_last_bcn_age;
     u_int32_t   ni_last_bcn_cnt;
     u_int64_t   ni_last_bcn_jiffies;
     u_int32_t   ni_last_ack_rssi;
     u_int32_t   ni_last_ack_age;
     u_int32_t   ni_last_ack_cnt;
     u_int64_t   ni_last_ack_jiffies;
} IEEE80211_NODE, *PIEEE80211_NODE;


struct ieee80211_wds_addr {
	LIST_ENTRY(ieee80211_wds_addr)    wds_hash;
	u_int8_t    wds_macaddr[IEEE80211_ADDR_LEN];
	struct ieee80211_node    *wds_ni;
    /* ni_macaddr can be accessed from ni pointer. In case of quick
     * disconnect and connect, wds entry for this would move from active to
     * staged state.
     * When in staged, this should not be refering to stale
     * node pointer. So cache the mac address alone, so that, we can use
     * this information to figure out the actual node pointer. Also new hash
     */
    u_int8_t    wds_ni_macaddr[IEEE80211_ADDR_LEN];

#define IEEE80211_NODE_F_WDS_BEHIND   0x00001
#define IEEE80211_NODE_F_WDS_REMOTE   0x00002
#define IEEE80211_NODE_F_WDS_STAGE    0x00004
#define IEEE80211_NODE_F_WDS_START    0x00010
#define IEEE80211_NODE_F_WDS_HM       0x00020
    u_int32_t   flags;
	u_int16_t   wds_agingcount;
    u_int16_t   wds_staging_age;
    systime_t   wds_last_pkt_time;
};

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

#if UMAC_SUPPORT_P2P
#define WME_UAPSD_NODE_MAXQDEPTH   100
#else
#define WME_UAPSD_NODE_MAXQDEPTH   8
#endif

#define ATH_NODE_NET80211_UAPSD_MAXQDEPTH (2*WME_UAPSD_NODE_MAXQDEPTH)

#define WME_UAPSD_AC_CAN_TRIGGER(_ac, _ni)  \
             ((((_ni)->ni_uapsd_dyn_trigena[_ac] == -1) ? ((_ni)->ni_uapsd_ac_trigena[_ac]) \
                            :((_ni)->ni_uapsd_dyn_trigena[_ac])) &&  ((_ni)->ni_flags & IEEE80211_NODE_UAPSD_TRIG))
#define WME_UAPSD_AC_ISDELIVERYENABLED(_ac, _ni)  \
             (((((_ni)->ni_uapsd_dyn_delivena[_ac] == -1) && (_ni)->ni_uapsd_dyn_trigena[_ac] ==-1)? ((_ni)->ni_uapsd_ac_delivena[_ac]) \
                            :((_ni)->ni_uapsd_dyn_delivena[_ac])) &&  ((_ni)->ni_flags & IEEE80211_NODE_UAPSD_TRIG))
#define IEEE80211_NODE_UAPSD_USETIM(_ni) \
        (WME_UAPSD_AC_ISDELIVERYENABLED(WME_AC_BK, _ni) && \
        WME_UAPSD_AC_ISDELIVERYENABLED(WME_AC_BE, _ni) && \
        WME_UAPSD_AC_ISDELIVERYENABLED(WME_AC_VI, _ni) && \
        WME_UAPSD_AC_ISDELIVERYENABLED(WME_AC_VO, _ni))
#define WME_UAPSD_NODE_INVALIDSEQ    0xffff


#ifdef QCA_SUPPORT_CP_STATS
#define    WLAN_PEER_CP_STAT(ni,stat) (peer_cp_stats_##stat##_inc(ni->peer_obj, 1))
#define    WLAN_PEER_CP_STAT_SET(ni,stat,v) (peer_cp_stats_##stat##_update(ni->peer_obj, v))

#define    WLAN_PEER_CP_STAT_ADD(_ni,stat,_v) \
                   {\
                            peer_cp_stats_##stat##_inc(_ni->peer_obj, _v);\
                   }

#define    WLAN_PEER_CP_STAT_ADDRBASED(_vap, _macaddr, stat) \
                    {\
                        struct ieee80211_node *_ni = NULL;\
                        if ((_ni = ieee80211_find_node(&(_vap)->iv_ic->ic_sta, (_macaddr))) != NULL) {\
                            peer_cp_stats_##stat##_inc(_ni->peer_obj, 1);\
                            ieee80211_free_node(_ni);\
                        }\
                    }
#endif

#define    IEEE80211_NODE_STAT_ADDRBASED(_vap, _macaddr, _stat) \
                    {\
                        struct ieee80211_node *_ni = NULL;\
                        if ((_ni = ieee80211_find_node(&(_vap)->iv_ic->ic_sta, (_macaddr))) != NULL) {\
                            IEEE80211_NODE_STAT(_ni, _stat);\
                            ieee80211_free_node(_ni);\
                        }\
                    }

 #define    IEEE80211_NODE_STAT_ADD_ADDRBASED(_vap, _macaddr, _stat, _v) \
                    {\
                        struct ieee80211_node *_ni = NULL;\
                        if ((_ni = ieee80211_find_node(&(_vap)->iv_ic->ic_sta, (_macaddr))) != NULL) {\
                            IEEE80211_NODE_STAT_ADD(_ni, _stat, (_v));\
                            ieee80211_free_node(_ni);\
                        }\
                    }

#define    IEEE80211_NODE_STAT_SUB_ADDRBASED(_vap, _macaddr, _stat, _v) \
                    {\
                        struct ieee80211_node *_ni = NULL;\
                        if ((_ni = ieee80211_find_node(&(_vap)->iv_ic->ic_sta, (_macaddr))) != NULL) {\
                            IEEE80211_NODE_STAT_SUB(_ni, _stat, (_v));\
                            ieee80211_free_node(_ni);\
                        }\
                    }

#define    IEEE80211_NODE_STAT_SET_ADDRBASED(_vap, _macaddr, _stat, _v) \
                    {\
                        struct ieee80211_node *_ni = NULL;\
                        if ((_ni = ieee80211_find_node(&(_vap)->iv_ic->ic_sta, (_macaddr))) != NULL) {\
                            IEEE80211_NODE_STAT_SET(_ni, _stat, (_v));\
                            ieee80211_free_node(_ni);\
                        }\
                    }


/*
 * Table of node instances.
 */

#define	IEEE80211_NODE_HASHSIZE	32

/* simple hash is enough for variation of macaddr */
#define	IEEE80211_NODE_HASH(addr)   \
    (((const u_int8_t *)(addr))[IEEE80211_ADDR_LEN - 1] % IEEE80211_NODE_HASHSIZE)

#if UMAC_SUPPORT_PROXY_ARP
#define IEEE80211_IPV4_HASHSIZE 32
#define IEEE80211_IPV4_HASH(n) \
    (((const uint8_t *)(&n))[3] % IEEE80211_IPV4_HASHSIZE)
#define IEEE80211_IPV6_HASHSIZE 32
#define IEEE80211_IPV6_HASH(n) \
    (((const uint8_t *)(n))[15] % IEEE80211_IPV6_HASHSIZE)
#endif

#if UMAC_SUPPORT_PROXY_ARP
struct ieee80211_ipv6_node {
    struct ieee80211_node *node;
    int index;
    LIST_ENTRY(ieee80211_ipv6_node) ni_hash;       /* ipv6 hash list */
    TAILQ_ENTRY(ieee80211_ipv6_node) ni_list; /* ipv6 node table list */
};
#endif

struct ieee80211_node_table {
    struct ieee80211com             *nt_ic;         /* back reference */
    ieee80211_node_lock_t           nt_nodelock;    /* on node table */
    ieee80211_node_lock_t           nt_wds_nodelock;    /* on node table */
    TAILQ_HEAD(node_list, ieee80211_node)  nt_node;        /* information of all nodes */
    ATH_LIST_HEAD(, ieee80211_node) nt_hash[IEEE80211_NODE_HASHSIZE];
    ATH_LIST_HEAD(, ieee80211_wds_addr) nt_wds_hash[IEEE80211_NODE_HASHSIZE];
    const char                      *nt_name;
    u_int32_t                       nt_scangen;
    os_timer_t                      nt_wds_aging_timer;    /* timer to age out wds entries */
#if UMAC_SUPPORT_PROXY_ARP
    ATH_LIST_HEAD(, ieee80211_node)      nt_ipv4_hash[IEEE80211_IPV4_HASHSIZE];
    rwlock_t                     nt_ipv4_hash_lock;
    TAILQ_HEAD(, ieee80211_ipv6_node)    nt_ipv6_node;     /* all ipv6 nodes */
    ATH_LIST_HEAD(, ieee80211_ipv6_node) nt_ipv6_hash[IEEE80211_IPV6_HASHSIZE];
    rwlock_t                     nt_ipv6_hash_lock;
#endif
    struct wlan_objmgr_psoc *psoc; /* UMAC psoc object */
};

void ieee80211_node_attach(struct ieee80211com *ic);
void ieee80211_node_detach(struct ieee80211com *ic);
void ieee80211_node_vattach(struct ieee80211vap *vap, struct vdev_mlme_obj *vdev_mlme);
void ieee80211_node_vdetach(struct ieee80211vap *vap);
int ieee80211_node_latevattach(struct ieee80211vap *vap);
void ieee80211_node_latevdetach(struct ieee80211vap *vap);
void ieee80211_node_reset(struct ieee80211_node *node);

void ieee80211_copy_bss(struct ieee80211_node *nbss, const struct ieee80211_node *obss);
int ieee80211_reset_bss(struct ieee80211vap *vap);
int ieee80211_sta_join_bss(struct ieee80211_node *selbs);
int ieee80211_aid_bmap_alloc(struct ieee80211vap *vap);

struct ieee80211_node *
ieee80211_ref_bss_node(struct ieee80211vap *vap);
struct ieee80211_node *
ieee80211_try_ref_bss_node(struct ieee80211vap *vap);

#if IEEE80211_DEBUG_REFCNT
#define	ieee80211_ref_bss_node(vap) \
    ieee80211_ref_bss_node_debug(vap, __func__, __LINE__, __FILE__)

#define	ieee80211_try_ref_bss_node(vap) \
    ieee80211_try_ref_bss_node_debug(vap, __func__, __LINE__, __FILE__)

struct ieee80211_node *
ieee80211_ref_bss_node_debug(struct ieee80211vap *vap,
                             const char *func, int line, const char *file);
struct ieee80211_node *
ieee80211_try_ref_bss_node_debug(struct ieee80211vap *vap,
                             const char *func, int line, const char *file);

#else  /* !IEEE80211_DEBUG_REFCNT */
struct ieee80211_node *
ieee80211_ref_bss_node(struct ieee80211vap *vap);
struct ieee80211_node *
ieee80211_try_ref_bss_node(struct ieee80211vap *vap);
#endif  /* IEEE80211_DEBUG_REFCNT */


struct ieee80211_node *
ieee80211_add_neighbor(struct ieee80211_node *ni,
                       ieee80211_scan_entry_t scan_entry);

struct ieee80211_node *
ieee80211_tmp_node(struct ieee80211vap *vap, const u_int8_t *macaddr);

int
ieee80211_dup_ibss(struct ieee80211vap *vap, struct ieee80211_node *org_ibss);

int
ieee80211_sta_join(struct ieee80211vap *vap, ieee80211_scan_entry_t scan_entry,
                   bool *thread_started);

int
ieee80211_join_ibss(struct ieee80211vap *vap, ieee80211_scan_entry_t scan_entry);

int
ieee80211_ibss_merge(struct ieee80211_node *ni, ieee80211_scan_entry_t scan_entry);

int
ieee80211_create_ibss(struct ieee80211vap *vap,
                      const u_int8_t *bssid,
                      const u_int8_t *essid,
                      const u_int16_t esslen);
int
ieee80211_create_infra_bss(struct ieee80211vap *vap,
                      const u_int8_t *essid,
                           const u_int16_t esslen);

struct ieee80211_node *
ieee80211_alloc_node(struct ieee80211_node_table *nt,
                     struct ieee80211vap *vap,
                     struct wlan_objmgr_peer *peer);

void
ieee80211_wnm_nattach(struct ieee80211_node *ni);

void
ieee80211_wnm_ndetach(struct ieee80211_node *ni);

void ieee80211_timeout_stations(struct ieee80211_node_table *nt);

void ieee80211_session_timeout(struct ieee80211_node_table *nt);


/*
 * ieee80211_node_refcnt	reference count for printing (only)
 */
#define	ieee80211_node_refcnt(_ni) wlan_objmgr_node_refcnt(_ni)
void _ieee80211_free_node(struct ieee80211_node *ni);
struct ieee80211_node *
_ieee80211_find_logically_deleted_node(struct ieee80211_node_table *nt,
                             const u_int8_t *macaddr, const u_int8_t *bssid);

#ifndef WLAN_CONV_CRYPTO_SUPPORTED
void ieee80211_node_clear_keys(struct ieee80211_node *ni);
#endif

#if IEEE80211_DEBUG_REFCNT

void ieee80211_free_node_debug(struct ieee80211_node *,
                               const char *func, int line, const char *file);

struct ieee80211_node *
ieee80211_find_node_debug(struct ieee80211_node_table *nt,
                                                 const u_int8_t *macaddr,
                                                 const char *func, int line, const char *file);
struct ieee80211_node *
ieee80211_find_txnode_debug(struct ieee80211vap *vap, const u_int8_t *macaddr,
                            const char *func, int line, const char *file);
struct ieee80211_node *
ieee80211_find_rxnode_debug(struct ieee80211com *ic,
                            const struct ieee80211_frame_min *wh,
                            const char *func, int line, const char *file);

#define ieee80211_find_rxnode_nolock(ni, hdr)                  \
    ieee80211_find_rxnode_nolock_debug(ni, hdr, __func__, __LINE__, __FILE__)
struct ieee80211_node *
ieee80211_find_rxnode_nolock_debug(struct ieee80211com *ic,
                                   const struct ieee80211_frame_min *wh,
                                   const char *func, int line, const char *file);

struct ieee80211_node *
ieee80211_ref_node_debug(struct ieee80211_node *ni,
                         const char *func, int line, const char *file);
struct ieee80211_node *
ieee80211_try_ref_node_debug(struct ieee80211_node *ni,
                         const char *func, int line, const char *file);
void
ieee80211_unref_node_debug(struct ieee80211_node **ni,
                         const char *func, int line, const char *file);
bool
ieee80211_node_leave_debug(struct ieee80211_node *ni,
                         const char *func, int line, const char *file);
bool
ieee80211_sta_leave_debug(struct ieee80211_node *ni,
                         const char *func, int line, const char *file);

#define	ieee80211_free_node(ni) \
    ieee80211_free_node_debug(ni, __func__, __LINE__, __FILE__)
#define ieee80211_find_node(nt, mac)    \
    ieee80211_find_node_debug(nt, mac, __func__, __LINE__, __FILE__)
#define ieee80211_find_txnode(vap, mac) \
    ieee80211_find_txnode_debug(vap, mac, __func__, __LINE__, __FILE__)
#define ieee80211_find_rxnode(ic, wh)   \
    ieee80211_find_rxnode_debug(ic, wh, __func__, __LINE__, __FILE__)

#if ATH_SUPPORT_WRAP
#define ieee80211_vap_find_node(_vap, _mac)     \
    ieee80211_find_wrap_node_debug(_vap, _mac, __func__, __LINE__, __FILE__)
#else
#define ieee80211_vap_find_node(_vap, _mac)     \
    ieee80211_find_node_debug(&(_vap)->iv_ic->ic_sta, _mac, __func__, __LINE__, __FILE__)
#endif

#define	ieee80211_ref_node(ni) \
    ieee80211_ref_node_debug(ni, __func__, __LINE__, __FILE__)
#define	ieee80211_try_ref_node(ni) \
    ieee80211_try_ref_node_debug(ni, __func__, __LINE__, __FILE__)
#define	ieee80211_unref_node(ni) \
    ieee80211_unref_node_debug(ni, __func__, __LINE__, __FILE__)

#define _ieee80211_node_leave(ni) \
    ieee80211_node_leave_debug(ni, __func__, __LINE__, __FILE__)

#define ieee80211_sta_leave(ni) \
    ieee80211_sta_leave_debug(ni, __func__, __LINE__, __FILE__)

#ifndef ATH_HTC_MII_RXIN_TASKLET
#define     IEEE80211_NODE_LEAVE(_ni) _ieee80211_node_leave(_ni)
#else
bool IEEE80211_NODE_LEAVE(struct ieee80211_node *ni);
#endif

#else  /* !IEEE80211_DEBUG_REFCNT */

void ieee80211_free_node(struct ieee80211_node *ni);
struct ieee80211_node *
ieee80211_find_node(struct ieee80211_node_table *nt,
                    const u_int8_t *macaddr);
struct ieee80211_node *
ieee80211_find_txnode(struct ieee80211vap *vap,
                      const u_int8_t *macaddr);
struct ieee80211_node *
ieee80211_find_rxnode(struct ieee80211com *ic,
                      const struct ieee80211_frame_min *wh);

#if ATH_SUPPORT_WRAP
#define ieee80211_vap_find_node(_vap, _mac)     \
    ieee80211_find_wrap_node(_vap, _mac)
#else
#define ieee80211_vap_find_node(_vap, _mac)     \
    ieee80211_find_node(&(_vap)->iv_ic->ic_sta, _mac)
#endif

struct ieee80211_node *
ieee80211_find_rxnode_nolock(struct ieee80211com *ic,
                      const struct ieee80211_frame_min *wh);

static INLINE struct ieee80211_node *
ieee80211_ref_node(struct ieee80211_node *ni)
{
    wlan_objmgr_ref_node(ni, WLAN_MLME_SB_ID);
    return ni;
}

static INLINE struct ieee80211_node *
ieee80211_try_ref_node(struct ieee80211_node *ni)
{
    if (wlan_objmgr_try_ref_node(ni, WLAN_MLME_SB_ID) == QDF_STATUS_SUCCESS)
        return ni;
    return NULL;
}

static INLINE void
ieee80211_unref_node(struct ieee80211_node **ni)
{
    wlan_objmgr_unref_node(*ni, WLAN_MLME_SB_ID);
    *ni = NULL;			/* guard against use */
}

bool _ieee80211_node_leave(struct ieee80211_node *ni);


void ieee80211_mu_cap_client_join_leave(struct ieee80211_node *ni,const u_int8_t type);
//int  ieee80211_mu_cap_dedicated_mu_kickout(struct MU_CAP_WAR *war);
//u_int16_t get_mu_total_clients(struct MU_CAP_WAR *war);


#ifndef ATH_HTC_MII_RXIN_TASKLET
#define     IEEE80211_NODE_LEAVE(_ni) _ieee80211_node_leave(_ni)
#else
bool IEEE80211_NODE_LEAVE(struct ieee80211_node *ni);

#endif
bool ieee80211_sta_leave(struct ieee80211_node *ni);

#endif /* IEEE80211_DEBUG_REFCNT */

u_int16_t get_mu_total_clients(struct MU_CAP_WAR *war);
int  ieee80211_mu_cap_dedicated_mu_kickout(struct MU_CAP_WAR *war);


void ieee80211_mu_cap_client_join_leave(struct ieee80211_node *ni,const u_int8_t type);

/* after adding the new staging flag for the wds, ieee80211_find_wds_node
 * would get called in the context of ieee80211_find_node OR
 * ieee80211_find_txnode. Calling function without '_' would call recursion.
 * To avoid this, _ieee80211_find_node, is made global by removing the
 * static, also moved the definition here.
 */
#if IEEE80211_DEBUG_REFCNT

#define _ieee80211_find_node(nt, mac) _ieee80211_find_node_debug(nt, mac, __func__, __LINE__, __FILE__)

struct ieee80211_node *
_ieee80211_find_node_debug(
        struct ieee80211_node_table *nt, const u_int8_t *macaddr,
        const char *func, int line, const char *file);
#else

struct ieee80211_node *
_ieee80211_find_node(struct ieee80211_node_table *nt,
                    const u_int8_t *macaddr);
#endif

#if UMAC_SUPPORT_PROXY_ARP
struct ieee80211_node *
ieee80211_find_node_by_ipv4(struct ieee80211_node_table *nt,
                            const uint32_t addr);
void
ieee80211_node_add_ipv4(struct ieee80211_node_table *nt,
                        struct ieee80211_node *ni,
                        const uint32_t ipaddr);
void
ieee80211_node_remove_ipv4(struct ieee80211_node_table *nt,
                            struct ieee80211_node *ni);
struct ieee80211_node *
ieee80211_find_node_by_ipv6(struct ieee80211_node_table *nt,
                            u8 *ip6addr);
int
ieee80211_node_add_ipv6(struct ieee80211_node_table *nt,
                        struct ieee80211_node *ni,
                        u8 *ip6addr);
void
ieee80211_node_remove_ipv6(struct ieee80211_node_table *nt, u8 *ip6addr);
#endif

#if ATH_SUPPORT_WRAP
struct ieee80211_node *
#if IEEE80211_DEBUG_REFCNT
ieee80211_find_wrap_node_debug(struct ieee80211vap *vap, const u_int8_t *macaddr,
                               const char *func, int line, const char *file);
#else
ieee80211_find_wrap_node(struct ieee80211vap *vap, const u_int8_t *macaddr);
#endif
#endif

void ieee80211_kick_node(struct ieee80211_node *ni);

#if IEEE80211_DEBUG_NODELEAK
void
ieee80211_dump_alloc_nodes(struct ieee80211com *ic);
#endif

void ieee80211_node_leave_11g(struct ieee80211_node *ni);

#if ATH_TxBF_DYNAMIC_LOF_ON_N_CHAIN_MASK
void iee80211_txbf_loforce_check(struct ieee80211_node *ni, bool nodejoin);
#endif

void ieee80211_node_set_chan(struct ieee80211_node *ni);

void ieee80211_node_update_chan_and_phymode(void *arg, struct ieee80211_node *ni);

int ieee80211_node_join(struct ieee80211_node *ni);
struct ieee80211_node *
ieee80211_dup_bss(struct ieee80211vap *vap, const u_int8_t *macaddr);


static INLINE int
ieee80211_node_is_authorized(const struct ieee80211_node *ni)
{
    return (ni->ni_flags & IEEE80211_NODE_AUTH);
}

void ieee80211_node_authorize(struct ieee80211_node *ni);

void ieee80211_node_unauthorize(struct ieee80211_node *ni);

static INLINE void
ieee80211_node_activity(struct ieee80211_node *ni)
{
    ni->ni_inact = ni->ni_inact_reload;
}

#define	IEEE80211_INACT_INIT    (30/IEEE80211_INACT_WAIT)   /* initial */
#define IEEE80211_INACT_AUTH    (180/IEEE80211_INACT_WAIT)    /* associated but not authorized */
#define IEEE80211_INACT_RUN     (300/IEEE80211_INACT_WAIT)    /* authorized */
#define IEEE80211_INACT_PROBE   (90/IEEE80211_INACT_WAIT)    /* probe */
#define IEEE80211_SESSION_TIME  ((u_int32_t)-1/IEEE80211_SESSION_WAIT) /* infinite by default */

typedef void ieee80211_iter_func(void *, struct ieee80211_node *);
void ieee80211_iterate_node(struct ieee80211com *ic, ieee80211_iter_func *func, void *arg);


/*
 * Accessor methods for node
 */

/* Get the VAP object that this node belongs to */
static INLINE struct ieee80211vap *
ieee80211_node_get_vap(struct ieee80211_node *ni)
{
    return ni->ni_vap;
}

static INLINE u_int16_t
ieee80211_node_get_txpower(struct ieee80211_node *ni)
{
    return ni->ni_txpower;
}

/* Return the beacon interval of associated BSS */
static INLINE u_int16_t
ieee80211_node_get_beacon_interval(struct ieee80211_node *ni)
{
    return ni->ni_intval;
}

static INLINE u_int64_t
ieee80211_node_get_tsf(struct ieee80211_node *ni)
{
    return ni->ni_tstamp.tsf;
}

static INLINE u_int16_t
ieee80211_node_get_associd(struct ieee80211_node *ni)
{
    return ni->ni_associd;
}

static INLINE u_int8_t *
ieee80211_node_get_bssid(struct ieee80211_node *ni)
{
    return ni->ni_bssid;
}

static INLINE u_int8_t *
ieee80211_node_get_macaddr(struct ieee80211_node *ni)
{
    return ni->ni_macaddr;
}

static INLINE u_int16_t
ieee80211_node_get_maxampdu(struct ieee80211_node *ni)
{
    return ni->ni_maxampdu;
}

#ifdef ATH_SWRETRY
static INLINE u_int16_t
ieee80211_node_get_pwrsaveq_len(struct ieee80211_node *ni)
{
    struct node_powersave_queue *dataq,*mgmtq;
    dataq = &ni->ni_dataq;
    mgmtq  = &ni->ni_mgmtq;

    return (dataq->nsq_len + mgmtq->nsq_len);
}
#endif

#define IEEE80211_NODE_CLEAR_HTCAP(_ni)     ((_ni)->ni_htcap = 0)

#define IEEE80211_NODE_USE_HT(_ni)          ((_ni)->ni_flags & IEEE80211_NODE_HT)
#define IEEE80211_NODE_USEAMPDU(_ni)        ieee80211node_use_ampdu(_ni)
#define IEEE80211_NODE_ISAMPDU(_ni)         ieee80211node_is_ampdu(_ni)

#define IEEE80211_NODE_USE_VHT(_ni)          ((_ni)->ni_flags & IEEE80211_NODE_VHT)

#define IEEE80211_NODE_USE_HE(_ni)          ((_ni)->ni_ext_flags & IEEE80211_NODE_HE)

/* Function used in TX path */
static INLINE int ieee80211node_use_ampdu(struct ieee80211_node *ni)
{
    if (IEEE80211_NODE_USE_HT(ni) &&
        !(ni->ni_flags & IEEE80211_NODE_NOAMPDU) && !ni->ni_pspoll) {
        return(1);  /* Supports AMPDU */
    }
    return(0);  /* Do not use AMPDU since non HT */
}

/* Function used in RX path */
static INLINE int ieee80211node_is_ampdu(struct ieee80211_node *ni)
{
    if (IEEE80211_NODE_USE_HT(ni) &&
        !(ni->ni_flags & IEEE80211_NODE_NOAMPDU)) {
        return(1);  /* Supports AMPDU */
    }
    return(0);  /* Do not use AMPDU since non HT */
}

#define IEEE80211_NODE_AC_UAPSD_ENABLED(_ni, _ac) ((_ni)->ni_uapsd_ac_delivena[(_ac)])


/*
 * ************************************
 * IEEE80211_NODE Interfaces
 * ************************************
 */
static INLINE void
ieee80211node_set_flag(struct ieee80211_node *ni, u_int32_t flag)
{
    ni->ni_flags |= flag;
    wlan_node_peer_set_flag(ni, flag);
}

static INLINE void
ieee80211node_clear_flag(struct ieee80211_node *ni, u_int32_t flag)
{
    ni->ni_flags &= ~flag;
    wlan_node_peer_clear_flag(ni, flag);
}

static INLINE void
ieee80211node_set_extflag(struct ieee80211_node *ni, u_int32_t flag)
{
    ni->ni_ext_flags |= flag;
    wlan_node_peer_set_extflag(ni, flag);
}

static INLINE void
ieee80211node_clear_extflag(struct ieee80211_node *ni, u_int32_t flag)
{
    ni->ni_ext_flags &= ~flag;
    wlan_node_peer_clear_extflag(ni, flag);
}

static INLINE int
ieee80211node_has_flag(struct ieee80211_node *ni, u_int32_t flag)
{
    return ((ni->ni_flags & flag) != 0);
}

static INLINE int
ieee80211node_has_extflag(struct ieee80211_node *ni, u_int32_t flag)
{
        return ((ni->ni_ext_flags & flag) != 0);
}

static INLINE void
ieee80211node_test_set_delayed_node_cleanup_fail(struct ieee80211_node *ni,
        u_int32_t flag)
{
    if (ieee80211node_has_flag(ni, IEEE80211_NODE_DELAYED_CLEANUP))
        ieee80211node_set_extflag(ni, IEEE80211_NODE_DELAYED_CLEANUP_FAIL);
}

static INLINE u_int16_t
ieee80211node_get_phymodes(struct ieee80211_node *ni)
{
    return ni->ni_phymode;
}

static INLINE void
ieee80211node_set_athflag(struct ieee80211_node *ni, u_int8_t flag)
{
    ni->ni_ath_flags |= flag;
}

static INLINE void
ieee80211node_clear_athflag(struct ieee80211_node *ni, u_int8_t flag)
{
    ni->ni_ath_flags &= ~flag;
}

static INLINE int
ieee80211node_has_athflag(struct ieee80211_node *ni, u_int8_t flag)
{
    return ((ni->ni_ath_flags & flag) != 0);
}

static INLINE void
ieee80211node_clear_supp_chan_info(struct ieee80211_node *ni)
{
    ni->ni_first_channel = 0;
    ni->ni_nr_channels = 0;
}

static INLINE void
ieee80211node_set_txpower(struct ieee80211_node *ni, u_int16_t txpower)
{
    ni->ni_txpower = txpower;
}

static INLINE void
ieee80211_node_set_beacon_interval(struct ieee80211_node *ni, u_int16_t intval)
{
    ni->ni_intval = intval;
}

static INLINE u_int8_t
ieee80211node_get_rssi(struct ieee80211_node *ni)
{
    return ni->ni_rssi;
}

static INLINE struct ieee80211_rateset *
ieee80211node_get_rateset(struct ieee80211_node *ni)
{
    return &ni->ni_rates;
}

static INLINE u_int8_t *
ieee80211node_get_tstamp(struct ieee80211_node *ni)
{
    return ni->ni_tstamp.data;
}

static INLINE u_int16_t
ieee80211node_get_capinfo(struct ieee80211_node *ni)
{
    return ni->ni_capinfo;
}

static INLINE int
ieee80211node_has_cap(struct ieee80211_node *ni, u_int16_t cap)
{
    return ((ni->ni_capinfo & cap) != 0);
}

static INLINE int
ieee80211node_is_paused(struct ieee80211_node *ni)
{
    return ( (ni->ni_flags & IEEE80211_NODE_PAUSED) != 0);
}

static INLINE int
ieee80211node_pause(struct ieee80211_node *ni)
{
    u_int16_t pause_count;

    IEEE80211_NODE_STATE_PAUSE_LOCK(ni);
    pause_count = ++ni->ni_pause_count;
    ieee80211node_set_flag(ni, IEEE80211_NODE_PAUSED);
    IEEE80211_NODE_STATE_PAUSE_UNLOCK(ni);
    return pause_count;
}

static INLINE int
ieee80211node_unpause(struct ieee80211_node *ni)
{
	/*
	** Note:
	** Check NI is valid, just in case
	*/

	if( ni )
	{
        IEEE80211_NODE_STATE_PAUSE_LOCK(ni);

		/*
		** Simply set the pause count to zero.  This should
		** NOT have any detrimental effects
		****** PERMINANT FIX REQUIRED, THIS IS TEMPORARY *****
		*/

		if ( ni->ni_pause_count > 0 ) {
			ni->ni_pause_count--;
        }
		else {
			//printk("%s: Pause Count already zero\n",__func__);
        }

		/*
		** You can put a debug message here in case you want
		** notification of an extra unpause.  At this point, we
		** don't want to assert
		*/

    	if (ni->ni_pause_count == 0 && (ni->ni_flags & IEEE80211_NODE_PAUSED))
    	{
            ieee80211node_clear_flag(ni, IEEE80211_NODE_PAUSED);
    	    ieee80211_node_saveq_flush(ni);
    	}
        IEEE80211_NODE_STATE_PAUSE_UNLOCK(ni);

    	return ni->ni_pause_count;
	}
	return (0);
}
/*
 * 11n
 */
static INLINE int
ieee80211_node_has_htcap(struct ieee80211_node *ni, u_int16_t htcap)
{
    return ((ni->ni_htcap & htcap) != 0);
}

#if ATH_SUPPORT_IQUE
static INLINE u_int8_t
ieee80211_node_get_hbr_block_state(struct ieee80211_node *ni)
{
    return ni->ni_hbr_block;
}
#endif

#define    IEEE80211_NODE_AID(ni)    IEEE80211_AID(ni->ni_associd)


/* Not supported */
#define IEEE80211_NODE_WDSWAR_ISSENDDELBA(_ni)    \
    ( 0 )

int wlan_node_alloc_aid_bitmap(wlan_if_t vap, u_int16_t old_len);

#if (MESH_MODE_SUPPORT||ATH_SUPPORT_NAC)
int wlan_add_localpeer (wlan_if_t vaphandle, char *macaddr, u_int32_t caps);
int wlan_authorise_local_peer(wlan_if_t vaphandle, char *macaddr);
int wlan_del_localpeer(wlan_if_t vaphandle, char *macaddr);
#endif
int wlan_node_peer_delete_response_handler(struct ieee80211vap *vap, struct ieee80211_node *ni);
#ifdef AST_HKV1_WORKAROUND
enum wds_auth_defer_action {
    IEEE80211_AUTH_CONTINUE,
    IEEE80211_AUTH_ABORT,
};
int wlan_wds_delete_response_handler(void *soc,
				     struct recv_auth_params_defer *auth_params,
				     enum wds_auth_defer_action action);
#endif
bool ieee80211_try_mark_node_for_delayed_cleanup(struct ieee80211_node *ni);
bool is_node_self_peer(struct ieee80211vap *vap, const uint8_t *macaddr);
struct ieee80211_node* find_logically_deleted_node_on_soc(struct wlan_objmgr_psoc *psoc,
        const uint8_t *macaddr, const uint8_t *bssid);

/* Structures to use in the UMAC for compiling peer channel width information
 * during AP channel width change
 * NOTE: node_chan_width_switch_info is to be kept identical to
 * peer_chan_width_switch_info defined in wmi_unified_param.h */
struct node_chan_width_switch_info {
    uint8_t mac_addr[IEEE80211_ADDR_LEN];
    uint32_t chan_width;
};

struct node_chan_width_switch_params {
    uint32_t num_peers;
    uint32_t max_peers;
    struct node_chan_width_switch_info *chan_width_peer_list;
};

#endif /* end of _ATH_STA_IEEE80211_NODE_H */
