/*
 * Copyright (c) 2011-2019 Qualcomm Innovation Center, Inc.
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
 */

#ifndef _ATH_STA_IEEE80211_VAR_H
#define _ATH_STA_IEEE80211_VAR_H

/*
 * Emulation layer of net80211 module. In current NDIS driver implementation,
 * it acts as a shim layer between ATH layer and sample driver's STATION layer.
 */
#include <wlan_opts.h>
#include <ieee80211_options.h>
#include <_ieee80211.h>
#include <ieee80211.h>
#include <ieee80211_api.h>
#include <ieee80211_objmgr.h>
#include <wlan_cfg80211_api.h>
#include <ieee80211_crypto.h>
#include <ieee80211_crypto_wep_mbssid.h>
#include <ieee80211_node.h>
#include <ieee80211_proto.h>
#include <ieee80211_power.h>
#include <ieee80211_config.h>
#include <ieee80211_mlme.h>
#include <ieee80211_data.h>
#include <ieee80211_vap.h>
#include <ieee80211_vap_pause.h>
#include <ieee80211_resmgr.h>
#include <ieee80211_acs.h>
#include <ieee80211_extacs.h>
#include <ieee80211_cbs.h>
#include <ieee80211_ique.h>
#include <ieee80211_acl.h>
#include <ieee80211_ald.h>
#include <ieee80211_notify_tx_bcn.h>
#include <ieee80211_vap_ath_info.h>
#include <ieee80211_tsftimer.h>
#include <ieee80211_mlme_app_ie.h>
#include <ieee80211_bsscolor.h>
#include  <ext_ioctl_drv_if.h>
#if ATH_SUPPORT_WIFIPOS
#include <ieee80211_wifipos.h>
#endif

#include <if_athioctl.h>
#include <ieee80211_wps.h>
#include <ieee80211_admctl.h>
#include <ieee80211_rrm_proto.h>
#include <ieee80211_wnm.h>
#include <ieee80211_quiet.h>
#include <asf_print.h>
#include <qdf_lock.h>
#include <qdf_defer.h>
#include <qdf_mem.h>


#include "osif_wrap_private.h"

#include "ieee80211_ioctl.h"

#ifdef ATH_SUPPORT_TxBF
#include <ieee80211_txbf.h>
#endif

#include <qdf_atomic.h>

#if ATH_SW_WOW
#include <ieee80211_wow.h>
#endif
#include <ieee80211_mbo.h>

#include <ieee80211_qcn.h>

#if HOST_SW_LRO_ENABLE
#include <ath_lro.h>
#endif

#if QCA_SUPPORT_RAWMODE_PKT_SIMULATION
#include <osif_rawmode_sim_api.h>
#endif

#ifdef WLAN_CONV_CRYPTO_SUPPORTED
#include "wlan_crypto_global_def.h"
#include "wlan_crypto_global_api.h"
#endif
#include <wlan_spectral_public_structs.h>

#if ATH_PARAMETER_API
#include <ieee80211_papi.h>
#endif

#include <include/wlan_vdev_mlme.h>
#include <wlan_vdev_mgr_ucfg_api.h>

/*
 * NOTE: Although we keep the names of data structures (ieee80211com
 * and ieee80211vap), each OS will have different implementation of these
 * data structures. Linux/BSD driver will have the full implementation,
 * while in NDIS6.0 driver, this is just a glue between STATION layer
 * and ATH layer. ATH layer should use API's to access ic, iv, and ni,, * instead of referencing the pointer directly.
 */

#include <sys/queue.h>

#define ACL_HASHSIZE                 32    /* max ACL size */
#define IEEE80211_TXPOWER_MAX        100   /* .5 dbM (XXX units?) */
#define IEEE80211_TXPOWER_MIN        0     /* kill radio */

#define IEEE80211_DTIM_MAX           255   /* max DTIM period */
#define IEEE80211_DTIM_MIN           1     /* min DTIM period */
#define IEEE80211_DTIM_DEFAULT       1     /* default DTIM period */
#define IEEE80211_DTIM_DEFAULT_LP_IOT  41  /* 41x default DTIM period */

#define IEEE80211_LINTVAL_MAX        10   /* max listen interval */
#define IEEE80211_LINTVAL_MIN        1    /* min listen interval */
#define IEEE80211_LINTVAL_DEFAULT    1    /* default listen interval */

#define IEEE80211_BINTVAL_MAX        10000 /* max beacon interval (TU's) */
#define IEEE80211_BINTVAL_MIN        10    /* min beacon interval (TU's) */

#if ATH_SUPPORT_AP_WDS_COMBO
#define IEEE80211_BINTVAL_DEFAULT    400   /* default beacon interval (TU's) for 16 vap support */
#else
#define IEEE80211_BINTVAL_DEFAULT    100   /* default beacon interval (TU's) */
#endif

#define IEEE80211_BINTVAL_IWMAX       3500
#define IEEE80211_BINTVAL_IWMIN       40     /* min beacon interval */
#define IEEE80211_BINTVAL_LP_IOT_IWMIN 25    /* min beacon interval for LP IOT */

#define IEEE80211_BINTVAL_LP_IOT_DEFAULT  25
#define IEEE80211_OL_BINTVAL_MINVAL_RANGE1  40
#define IEEE80211_OL_MIN_VAP_MBSSID         1     /*minimum number of VAPs in MBSSID. To set bintval based on number of VAPS*/
#define IEEE80211_BINTVAL_MINVAL_RANGE2     100   /* used to compute the allowed bintval values based on number of vaps*/
#define IEEE80211_BINTVAL_MINVAL_RANGE3     200   /*if vap is <16 used this value as min allowed bintval value*/
#define IEEE80211_BINTVAL_VAPCOUNT1         2     /*if VAP is <=2 then min bintval is 40*/
#define IEEE80211_BINTVAL_VAPCOUNT2         8     /*if VAP is <=4 then min bintval is 100*/
#define IEEE80211_BINTVAL_VAPCOUNT3         16    /*if VAP is <=8 then min bintval is 200*/
#define MBSSID_BINTVAL_CHECK(_bintval, _numvaps, _bmode) \
        (((_numvaps <= IEEE80211_BINTVAL_VAPCOUNT1) && (_bintval >= IEEE80211_OL_BINTVAL_MINVAL_RANGE1)) ||\
        ((_numvaps <= IEEE80211_BINTVAL_VAPCOUNT2) && (_bintval >= IEEE80211_BINTVAL_MINVAL_RANGE2)) || \
        ((_numvaps <= IEEE80211_BINTVAL_VAPCOUNT3) && (_bmode == 0) && (_bintval >= IEEE80211_BINTVAL_MINVAL_RANGE3)) || \
        ((_numvaps <= IEEE80211_BINTVAL_VAPCOUNT3) && (_bmode == 1) && (_bintval >= IEEE80211_BINTVAL_MINVAL_RANGE2))? 1 : 0)
#define LIMIT_BEACON_PERIOD(_intval)                \
    do {                                            \
        if ((_intval) > IEEE80211_BINTVAL_MAX)      \
            (_intval) = IEEE80211_BINTVAL_MAX;      \
        else if ((_intval) < IEEE80211_BINTVAL_MIN) \
            (_intval) = IEEE80211_BINTVAL_MIN;      \
    } while (FALSE)

#define LIMIT_DTIM_PERIOD(_intval)                \
    do {                                            \
        if ((_intval) > IEEE80211_DTIM_MAX)      \
            (_intval) = IEEE80211_DTIM_MAX;      \
        else if ((_intval) < IEEE80211_DTIM_MIN) \
            (_intval) = IEEE80211_DTIM_MIN;      \
    } while (FALSE)

#define LIMIT_LISTEN_INTERVAL(_intval)                \
    do {                                            \
        if ((_intval) > IEEE80211_LINTVAL_MAX)      \
            (_intval) = IEEE80211_LINTVAL_MAX;      \
        else if ((_intval) < IEEE80211_LINTVAL_MIN) \
            (_intval) = IEEE80211_LINTVAL_MIN;      \
    } while (FALSE)

#define UP_CONVERT_TO_FACTOR_OF(_x,_y)             \
    do {                                           \
        if ((_x) < (_y))  break;                   \
        if ((_x)%(_y) == 0) {                      \
            break;                                 \
        } else {                                   \
            (_x) = (((_x/_y)+1) * (_y));           \
        }                                          \
    } while (FALSE)

#define DEFAULT_OBSS_RSSI_THRESHOLD 0    /* default RSSI threshold */
#define DEFAULT_OBSS_RX_RSSI_THRESHOLD 0 /* default RX RSSI threshold */
#define OBSS_RSSI_MIN 0    /* min RSSI */
#define OBSS_RSSI_MAX 127 /* max RSSI */

/* Definitions for valid VHT MCS map */
#define VHT_CAP_IE_NSS_MCS_0_7      0
#define VHT_CAP_IE_NSS_MCS_0_8      1
#define VHT_CAP_IE_NSS_MCS_0_9      2
#define VHT_CAP_IE_NSS_MCS_RES      3

#define VHT_CAP_IE_NSS_MCS_MASK     3
#define VHT_MAX_NSS 4
/* 8 chain TODO: Define VHT_MAX_NSS as 8 under #if QCA_SUPPORT_5SS_TO_8SS,
 * once VHT rate mask size is increased.
 */

#define VHT_MCSMAP_NSS1_MCS0_7  0xfffc  /* NSS=1 MCS 0-7 */
#define VHT_MCSMAP_NSS2_MCS0_7  0xfff0  /* NSS=2 MCS 0-7 */
#define VHT_MCSMAP_NSS3_MCS0_7  0xffc0  /* NSS=3 MCS 0-7 */
#define VHT_MCSMAP_NSS4_MCS0_7  0xff00  /* NSS=4 MCS 0-7 0xff.b0000,0000 */
#if QCA_SUPPORT_5SS_TO_8SS
#define VHT_MCSMAP_NSS5_MCS0_7  0xfc00  /* NSS=5 MCS 0-7 */
#define VHT_MCSMAP_NSS6_MCS0_7  0xf000  /* NSS=6 MCS 0-7 */
#define VHT_MCSMAP_NSS7_MCS0_7  0xc000  /* NSS=7 MCS 0-7 */
#define VHT_MCSMAP_NSS8_MCS0_7  0x0000  /* NSS=8 MCS 0-7 */
#endif /* QCA_SUPPORT_5SS_TO_8SS */

#define VHT_MCSMAP_NSS1_MCS0_8  0xfffd  /* NSS=1 MCS 0-8 */
#define VHT_MCSMAP_NSS2_MCS0_8  0xfff5  /* NSS=2 MCS 0-8 */
#define VHT_MCSMAP_NSS3_MCS0_8  0xffd5  /* NSS=3 MCS 0-8 */
#define VHT_MCSMAP_NSS4_MCS0_8  0xff55  /* NSS=3 MCS 0-8 0xff.b0101,0101*/
#if QCA_SUPPORT_5SS_TO_8SS
#define VHT_MCSMAP_NSS5_MCS0_8  0xfd55  /* NSS=5 MCS 0-8 */
#define VHT_MCSMAP_NSS6_MCS0_8  0xf555  /* NSS=6 MCS 0-8 */
#define VHT_MCSMAP_NSS7_MCS0_8  0xd555  /* NSS=7 MCS 0-8 */
#define VHT_MCSMAP_NSS8_MCS0_8  0x5555  /* NSS=8 MCS 0-8 */
#endif /* QCA_SUPPORT_5SS_TO_8SS */

#define VHT_MCSMAP_NSS1_MCS0_9  0xfffe  /* NSS=1 MCS 0-9 */
#define VHT_MCSMAP_NSS2_MCS0_9  0xfffa  /* NSS=2 MCS 0-9 */
#define VHT_MCSMAP_NSS3_MCS0_9  0xffea  /* NSS=3 MCS 0-9 */
#define VHT_MCSMAP_NSS4_MCS0_9  0xffaa  /* NSS=3 MCS 0-9 0xff.b1010,1010*/
#if QCA_SUPPORT_5SS_TO_8SS
#define VHT_MCSMAP_NSS5_MCS0_9  0xfeaa  /* NSS=5 MCS 0-9 */
#define VHT_MCSMAP_NSS6_MCS0_9  0xfaaa  /* NSS=6 MCS 0-9 */
#define VHT_MCSMAP_NSS7_MCS0_9  0xeaaa  /* NSS=7 MCS 0-9 */
#define VHT_MCSMAP_NSS8_MCS0_9  0xaaaa  /* NSS=8 MCS 0-9 */
#endif /* QCA_SUPPORT_5SS_TO_8SS */

/* Definitions for valid VHT MCS mask */
#define VHT_MCSMAP_NSS1_MASK   0xfffc   /* Single stream mask */
#define VHT_MCSMAP_NSS2_MASK   0xfff0   /* Dual stream mask */
#define VHT_MCSMAP_NSS3_MASK   0xffc0   /* Tri stream mask */
#define VHT_MCSMAP_NSS4_MASK   0xff00   /* four stream mask */
#if QCA_SUPPORT_5SS_TO_8SS
#define VHT_MCSMAP_NSS5_MASK   0xfc00   /* five stream mask */
#define VHT_MCSMAP_NSS6_MASK   0xf000   /* six stream mask */
#define VHT_MCSMAP_NSS7_MASK   0xc000   /* seven stream mask */
#define VHT_MCSMAP_NSS8_MASK   0x0000   /* eight stream mask */
#endif /* QCA_SUPPORT_5SS_TO_8SS */

/* Default VHT80 rate mask support all MCS rates except NSS=3 MCS 6 */
#define MAX_VHT80_RATE_MASK   0x3bffffff /* NSS=1 MCS 0-9, NSS=2 MCS 0-9, NSS=3 MCS 0-5 7-9 */

#define MAX_VHT_SGI_MASK   0x3ff /* MCS 0-9 */

/* Indicate support for VHT MCS10/11 */
#define ENABLE_VHT_MCS_10_11_SUPP   1

#define IEEE80211_BGSCAN_INTVAL_MIN            15  /* min bg scan intvl (secs) */
#define IEEE80211_BGSCAN_INTVAL_DEFAULT    (5*60)  /* default bg scan intvl */

#define IEEE80211_BGSCAN_IDLE_MIN             100  /* min idle time (ms) */
#define IEEE80211_BGSCAN_IDLE_DEFAULT         250  /* default idle time (ms) */

#define IEEE80211_CUSTOM_SCAN_ORDER_MAXSIZE   101  /* Max size of custom scan
                                                      order array */

#define IEEE80211_COVERAGE_CLASS_MAX           31  /* max coverage class */
#define IEEE80211_REGCLASSIDS_MAX              10  /* max regclass id list */

#define IEEE80211_PS_SLEEP                    0x1  /* STA is in power saving mode */
#define IEEE80211_PS_MAX_QUEUE                 50  /* maximum saved packets */

#define IEEE80211_MINIMUM_BMISS_TIME          500 /* minimum time without a beacon required for a bmiss */
#define IEEE80211_DEFAULT_BMISS_COUNT_MAX           3 /* maximum consecutive bmiss allowed */
#define IEEE80211_DEFAULT_BMISS_COUNT_RESET         2 /* number of  bmiss allowed before reset */

#define IEEE80211_BMISS_LIMIT                  15

#define IEEE80211_MAX_MCAST_LIST_SIZE          32 /* multicast list size */

#define IEEE80211_APPIE_MAX_FRAMES             10 /* max number frame types that can have app ie buffer setup */
#define IEEE80211_APPIE_MAX                  1024 /* max appie buffer size */
#define IEEE80211_OPTIE_MAX                   256 /* max optie buffer size */
#define IEEE80211_MAX_PRIVACY_FILTERS           4 /* max privacy filters */
#define IEEE80211_MAX_PMKID                     3 /* max number of PMKIDs */
#define IEEE80211_MAX_MISC_EVENT_HANDLERS       5
#define IEEE80211_MAX_RESMGR_EVENT_HANDLERS    32 /* max vdev resource mgr event handlers */
#define IEEE80211_MAX_DEVICE_EVENT_HANDLERS     6
#define IEEE80211_MAX_VAP_EVENT_HANDLERS        8
#define IEEE80211_MAX_VAP_MLME_EVENT_HANDLERS   4
#define IEEE80211_STA_NOASSOC_TIME              30000 /* 30 secs */


#define IEEE80211_INACT_WAIT                   15  /* inactivity interval (secs) */
#define IEEE80211_OBSS_NB_RU_TOLERANCE_TIME_MIN 300  /* Min Narrow Bandwidth RU tolerance time in seconds */
#define IEEE80211_OBSS_NB_RU_TOLERANCE_TIME_MAX 3600 /* Max Narrow Bandwidth RU tolerance time in seconds */
#define IEEE80211_OBSS_NB_RU_TOLERANCE_TIME_DEFVAL 1800 /* Default Narrow Bandwidth RU tolerance time in seconds */
#define IEEE80211_RUN_INACT_TIMEOUT_THRESHOLD       (u_int16_t)~0
#define IEEE80211_MLME_EXTERNAL_AUTH_TIMEOUT   (3000) /* 3000 msec*/
#define IEEE80211_MGMT_OFFCHAN_LIST_MAX        (20)
#define IEEE80211_SESSION_WAIT                  1  /* session timeout check interval (secs) */

#if ATH_PROXY_NOACK_WAR
#define TARGET_AST_RESERVE_TIMEOUT           1  /* ast reserve timeout  (secs) */
#endif

#define IEEE80211_MAGIC_ENGDBG           0x444247  /* Magic number for debugging purposes */
#define IEEE80211_DEFULT_KEEP_ALIVE_TIMEOUT    600  /* default keep alive timeout */
#define IEEE80211_FRAG_TIMEOUT                  1  /* fragment timeout in sec */

#define IEEE80211_CHAN_STATS_THRESOLD           40  /* percentage thresold */

#define IEEE80211_MS_TO_TU(x)                (((x) * 1000) / 1024)
#define IEEE80211_TU_TO_MS(x)                (((x) * 1024) / 1000)

#define IEEE80211_USEC_TO_TU(x)                ((x) >> 10)  /* (x)/1024 */
#define IEEE80211_TU_TO_USEC(x)                ((x) << 10)  /* (x)X1024 */
/*
 * Macros used for RSSI calculation.
 */
#define ATH_RSSI_EP_MULTIPLIER               (1<<7)  /* pow2 to optimize out * and / */

#define ATH_RSSI_LPF_LEN                     10
#define ATH_RSSI_DUMMY_MARKER                0x127
#define ATH_EP_MUL(x, mul)                   ((x) * (mul))
#define ATH_EP_RND(x, mul)                   ((((x)%(mul)) >= ((mul)/2)) ? ((x) + ((mul) - 1)) / (mul) : (x)/(mul))
#define ATH_RSSI_GET(x)                      ATH_EP_RND(x, ATH_RSSI_EP_MULTIPLIER)

#define RSSI_LPF_THRESHOLD                   -20

#define ATH_RSSI_OUT(x)                      (((x) != ATH_RSSI_DUMMY_MARKER) ? (ATH_EP_RND((x), ATH_RSSI_EP_MULTIPLIER)) : ATH_RSSI_DUMMY_MARKER)
#define ATH_RSSI_IN(x)                       (ATH_EP_MUL((x), ATH_RSSI_EP_MULTIPLIER))
#define ATH_LPF_RSSI(x, y, len) \
    ((x != ATH_RSSI_DUMMY_MARKER) ? ((((x) << 3) + (y) - (x)) >> 3) : (y))
#define ATH_RSSI_LPF(x, y) do {                     \
    if ((y) >= RSSI_LPF_THRESHOLD)                         \
        x = ATH_LPF_RSSI((x), ATH_RSSI_IN((y)), ATH_RSSI_LPF_LEN);  \
} while (0)
#define ATH_ABS_RSSI_LPF(x, y) do {                     \
    if ((y) >= (RSSI_LPF_THRESHOLD + ATH_DEFAULT_NOISE_FLOOR))      \
        x = ATH_LPF_RSSI((x), ATH_RSSI_IN((y)), ATH_RSSI_LPF_LEN);  \
} while (0)

#define IEEE80211_PWRCONSTRAINT_VAL(vap) \
    (((vap)->iv_bsschan->ic_maxregpower > (vap->iv_ic)->ic_curchanmaxpwr) ? \
        (vap)->iv_bsschan->ic_maxregpower - (vap->iv_ic)->ic_curchanmaxpwr : 3)

#define IEEE80211_RATE_MAX 256
#define IEEE80211_PCP_MAX 8

#define GLOBAL_SCN_SIZE 10
#define GLOBAL_SOC_SIZE GLOBAL_SCN_SIZE

#define WHC_DEFAULT_SFACTOR 70
#define WHC_DEFAULT_SKIPHYST 1
#define IS_NULL_ADDR(_a)     \
 ((_a)[0] == 0x00 &&         \
 (_a)[1] == 0x00 &&          \
 (_a)[2] == 0x00 &&          \
 (_a)[3] == 0x00 &&          \
 (_a)[4] == 0x00 &&          \
 (_a)[5] == 0x00)

#define MAX_BSS_SETS	1  /* Maximum MBSSID sets per radio */

static const u_int8_t ieee80211broadcastaddr[IEEE80211_ADDR_LEN] =
{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

/* maximum possible data size that can be passed from application*/
#define MAX_FIPS_BUFFER_SIZE (sizeof(struct ath_fips_cmd) + 1500)
struct ieee80211_chanhist{
    uint8_t    chanid;
    uint32_t   chanjiffies;
};

typedef struct ieee80211_country_entry{
    u_int16_t   countryCode;  /* HAL private */
    u_int16_t   regDmnEnum;   /* HAL private */
    u_int16_t   regDmn5G;
    u_int16_t   regDmn2G;
    u_int8_t    isMultidomain;
    u_int8_t    iso[3];       /* ISO CC + (I)ndoor/(O)utdoor or both ( ) */
} IEEE80211_COUNTRY_ENTRY;

/*
 * 802.11 control state is split into a common portion that maps
 * 1-1 to a physical device and one or more "Virtual AP's" (VAP)
 * that are bound to an ieee80211com instance and share a single
 * underlying device.  Each VAP has a corresponding OS device
 * entity through which traffic flows and that applications use
 * for issuing ioctls, etc.
 */

struct ieee80211vap;

//typedef  void *ieee80211_vap_state_priv_t;
typedef int ieee80211_vap_state_priv_t;


struct ath_fips_cmd
{
    u_int32_t fips_cmd;/* 1 - Encrypt, 2 - Decrypt*/
    u_int32_t mode; /* mode of AES - AES_CTR or AES_MIC */
    u_int32_t key_len;
#define MAX_KEY_LEN_FIPS 32
    u_int8_t  key[MAX_KEY_LEN_FIPS];
#define MAX_IV_LEN_FIPS  16
    u_int8_t iv[MAX_IV_LEN_FIPS];
    u_int32_t data_len;
    u_int32_t data[1]; /* To be allocated dynamically*/
};
#if ATH_SUPPORT_WIRESHARK
/* avoid inclusion of ieee80211_radiotap.h everywhere */
struct ath_rx_radiotap_header;
#endif
struct ieee80211_ol_wifiposdesc;

typedef spinlock_t ieee80211_ic_state_lock_t;

typedef struct ieee80211_ven_ie {
    u_int8_t                      ven_ie[IEEE80211_MAX_IE_LEN];
    u_int8_t                      ven_ie_len;
    u_int8_t                      ven_oui[3];
    bool                          ven_oui_set;
} IEEE80211_VEN_IE;



typedef struct ieee80211_vht_mcs {
    u_int16_t     mcs_map;          /* Max MCS for each SS */
    u_int16_t     data_rate;        /* Max data rate */
    u_int8_t      higher_mcs_supp;  /* VHT MCS10/11 support for each SS */
} ieee80211_vht_mcs_t;

typedef struct ieee80211_vht_mcs_set {
    ieee80211_vht_mcs_t     rx_mcs_set; /* B0-B15 Max Rx MCS for each SS
                                            B16-B28 Max Rx data rate
                                            B29-B31 reserved */
    ieee80211_vht_mcs_t     tx_mcs_set; /* B32-B47 Max Tx MCS for each SS
                                            B48-B60 Max Tx data rate
                                            B61-B63 reserved */
}ieee80211_vht_mcs_set_t;


/*
 * Data common to one or more virtual AP's.  State shared by
 * the underlying device and the net80211 layer is exposed here;
 * e.g. device-specific callbacks.
 */

#ifdef ATH_HTC_MII_RXIN_TASKLET
typedef struct ieee80211_recv_mgt_args {
    struct ieee80211_node *ni;
    int32_t subtype;
    int32_t rssi;
    uint32_t rstamp;
}ieee80211_recv_mgt_args_t;

typedef struct _nawds_dentry{
    TAILQ_ENTRY(_nawds_dentry)  nawds_dlist;
    struct ieee80211vap              *vap ;
    u_int8_t                  mac[IEEE80211_ADDR_LEN];

}nawds_dentry_t;

void _ath_htc_netdeferfn_init(struct ieee80211com *ic);
void _ath_htc_netdeferfn_cleanup(struct ieee80211com *ic);


#define ATH_HTC_NETDEFERFN_INIT(_ic) _ath_htc_netdeferfn_init(_ic)
#define ATH_HTC_NETDEFERFN_CLEANUP(_ic) _ath_htc_netdeferfn_cleanup(_ic)


#else
#define ATH_HTC_NETDEFERFN_INIT(_ic)
#define ATH_HTC_NETDEFERFN_CLEANUP(_ic)
#endif

#define ACS_PEIODIC_SCAN_CHK            0x1
#define ACS_PEREODIC_OBSS_CHK           0x2
#define ACS_PEIODIC_BG_SCAN             0x4

#define  ATH_SCANENTRY_MAX 1024
#define  ATH_SCANENTRY_TIMEOUT 60

struct mlmeop {
    struct ieee80211req_mlme *mlme;
    wlan_if_t vap;
};

#define VHT160_CENTER_EDGE_DIFF 80     /* diff between center freq and channel edges */
#define VHT80_CENTER_EDGE_DIFF  40     /* diff between center freq and channel edges */
#define VHT40_PRIMARY_EDGE_DIFF 30     /* diff between center freq and channel edges */
#define PRIMARY_EDGE_DIFF       10     /* diff between center freq and channel edges */

/*
 * Data structure which contains restricted channels
 * parameters.
 */
struct restrict_channel_params {
    u_int8_t  restrict_channel_support;  /* flag for restricted channel support */
    u_int32_t low_5ghz_chan;             /* lower bound frequency */
    u_int32_t high_5ghz_chan;            /* higher bound frequency */
};

#define MAX_RADIO_CNT 3

#define VDEV_BITMAP_SLOTS 2

#define VDEV_BITMAP_SLOT0 0
#define VDEV_BITMAP_SLOT1 1
struct ieee80211_clone_params_list {
    LIST_ENTRY(ieee80211_clone_params_list)    link_entry;
    struct ieee80211_clone_params cp;
};

typedef struct _CLONE_PARAMS {
    struct ieee80211_clone_params_list *lh_first;
} CLONE_PARAMS;

#if UMAC_SUPPORT_CFG80211

struct spectral_handler {
    int (*wlan_cfg80211_spectral_handlers[SPECTRAL_SCAN_VENDOR_CMD_HANDLER_MAX])(struct wiphy *wiphy,
						   struct wlan_objmgr_pdev *pdev,
						   const void *data,
						   int data_len);
};

struct radio_handler {
    int (*setcountry)(void *scn, char *cntry);
    int (*getcountry)(void *scn, char *str);
    int (*sethwaddr)(void *scn, char *addr);
    int (*gethwaddr)(struct net_device *dev, void *info, void *w, char *extra);
    int (*setparam)(void *scn, int param, int value);
    int (*getparam)(void *scn, int param, int *val);

#if UNIFIED_SMARTANTENNA
    int (*set_saparam)(void *scn, char *val);
    int (*get_saparam)(void *scn, char *val);
#endif

#if ATH_SUPPORT_DSCP_OVERRIDE
    void (*setdscp_tid_map)(void *scn, u_int8_t tos, u_int8_t tid);
    int (*getdscp_tid_map)(void *scn, u_int8_t tos);
#endif
    int (*gpio_config)(void *scn,int gpionum,int input,int pulltype, int intrmode);
    int (*gpio_output)(void *scn, int gpionum, int set);

#if ATH_SUPPORT_HYFI_ENHANCEMENTS
    int (*ald_getStatistics)(struct net_device *dev, void *info, void *w, char *extra);
#endif

#if PEER_FLOW_CONTROL
    void (*txrx_peer_stats)(void *scn, char *addr);
#endif

#if QCA_AIRTIME_FAIRNESS
    int (*set_atf_sched_dur)(void *scn, uint32_t ac, uint32_t duration);
#endif

    int (*get_aggr_burst)(struct net_device *dev,void *info,void *w,char *extra);
    int (*set_aggr_burst)(void *scn, uint32_t ac, uint32_t duration);
    int (*extended_commands)(struct net_device *dev,  void *command);
    int (*ucfg_phyerr)(void *scn, void *diag);
    int (*get_ath_stats) (void *scn, void *asc);
    void (*get_dp_fw_peer_stats)(void *scn, char *addr, uint8_t caps);
    void (*get_dp_htt_stats)(void *scn, void *data, uint32_t data_len);
    int (*set_he_bss_color)(void *scn, uint8_t value, uint8_t ovrride);
    int (*get_he_bss_color)(void *scn, int *value);
    void (*set_ba_timeout) (void *scn, uint8_t ac, uint32_t duration);
    void (*get_ba_timeout) (void *scn, uint8_t ac, uint32_t *duration);
    int (*set_pcp_tid_map) (void *scn, uint32_t pcp, uint32_t tid);
    int (*get_pcp_tid_map) (void *scn, uint32_t pcp, uint32_t *value);
    int (*set_tidmap_prty) (void *scn, uint32_t value);
    int (*get_tidmap_prty) (void *scn,  uint32_t *value);
#ifdef WLAN_SUPPORT_RX_PROTOCOL_TYPE_TAG
    int  (*ic_set_rx_pkt_protocol_tagging)(void *scn,
                    struct ieee80211_rx_pkt_protocol_tag *rx_pkt_protocol_tag_info);
#ifdef WLAN_SUPPORT_RX_TAG_STATISTICS
    int  (*ic_dump_rx_pkt_protocol_tag_stats)(void *scn, uint32_t protocol_type);
#endif //WLAN_SUPPORT_RX_TAG_STATISTICS
#endif //WLAN_SUPPORT_RX_PROTOCOL_TYPE_TAG
    int (*set_btcoex_duty_cycle)(void *scn, uint32_t period, uint32_t duration);

    int (*set_he_sr_config) (void *vscn, uint8_t param, uint8_t value, void *data);
    int (*get_he_sr_config) (void *vscn, uint8_t param, uint32_t *value);
};
#endif

typedef enum ieee80211_rep_move_state {
    REPEATER_MOVE_STOP = 0,
    REPEATER_MOVE_START = 1,
    REPEATER_MOVE_IN_PROGRESS = 2,
} ieee80211_rep_move_state_t;

typedef struct ieee80211_rep_move {
    ieee80211_rep_move_state_t state;
    ieee80211_ssid             ssid;
    u_int32_t                  chan;
    bool                       same_chan;
    u_int8_t                   bssid[IEEE80211_ADDR_LEN];
} ieee80211_rep_move_t;

#define NBR_SCAN_PERIOD            5      /* Neighbor scan period in sec */
#define RNR_MAX_ENTRIES            16     /* Reduced Neighbor Report entries */
#define RNR_MAX_SIZE               256    /* >= RNR_MAX_ENTRIES * (sizeof(rnr_ap_info_t) + sizeof(rnr_tbtt_info_t)) */
#define ACR_MAX_ENTRIES            16     /* AP Channel Report entries */
#define ACR_MAX_CHANNELS           16     /* Max number of channels for each op class */

typedef struct ap_chan_rpt {
    u_int8_t op_class;
    u_int8_t chan_cnt;
    u_int8_t chan[ACR_MAX_CHANNELS];
} ap_chan_rpt_t;

typedef struct chan_util_stats {
    u_int8_t free_medium;   /* Total Free medium available percentage*/
    u_int8_t ap_tx;    /* Total AP wlan Tx channel utilization percentage */
    u_int8_t ap_rx;    /* Total AP wlan Rx channel utilization percentage */
    u_int8_t obss_rx;     /* Other bss Rx channel utilization */

} chan_util_stats_t;

struct scan_chan_entry_stats {
    uint32_t freq;
    struct ieee80211_mib_cycle_cnts stats;
};

struct wlan_channel_stats {
    qdf_spinlock_t lock;
    struct channel_stats home_chan_stats;
    struct scan_chan_entry_stats entry_stats;
    struct channel_stats scan_chan_stats[MAX_DUAL_BAND_SCAN_CHANS];
};


struct last_scan_params {
   uint32_t scan_ev_started:1,
            scan_ev_completed:1;
   uint32_t scan_events;
   uint32_t scan_id;
   uint32_t scan_req_id;

   uint32_t min_rest_time;
   uint32_t max_rest_time;
   uint32_t idle_time;
   uint32_t max_scan_time;
   uint32_t dwell_time_active;
   uint32_t dwell_time_passive;

   uint32_t scan_f_passive:1,
            scan_f_bcast_probe:1,
            scan_f_continue_on_err:1,
            scan_f_forced:1,
            scan_f_2ghz:1,
            scan_f_5ghz:1,
            scan_f_wide_band:1;
   uint32_t scan_flags;
   struct chan_list chan_list;
};

/* MBSS IE support members */
struct mbss_info {
    unsigned long        bssid_index_bmap[MAX_BSS_SETS];
    struct ieee80211vap  *transmit_vap;
    uint8_t              num_non_transmit_vaps;
    uint8_t              max_bssid;
};

/*
 * beacon offload control operations.
 * Used by func ic_beacon_offload_control.
 */
#define IEEE80211_BCN_OFFLD_TX_DISABLE   0
#define IEEE80211_BCN_OFFLD_TX_ENABLE    1
#define IEEE80211_BCN_OFFLD_SWBA_DISABLE 2
#define IEEE80211_BCN_OFFLD_SWBA_ENABLE  3

typedef struct ieee80211com {
    osdev_t                       ic_osdev; /* OS opaque handle */
    qdf_device_t                  ic_qdf_dev; /* ADF opaque handle */
    qdf_netdev_t                  ic_netdev;  /* OS net device */
    bool                          ic_initialized;
    u_int16_t                     ic_scan_entry_max_count;     /* Global limit of number of  scan table entries */
    u_int16_t                     ic_scan_entry_timeout;       /* Timeout value for scan entries */
    spinlock_t                    ic_lock; /* lock to protect access to ic data */
    spinlock_t                    ic_state_check_lock; /* lock to protect access to ic data */
    unsigned long                 ic_lock_flags; /* flags for ic_lock */
    spinlock_t                    ic_main_sta_lock; /* lock to protect main sta */
    unsigned long                 ic_main_sta_lock_flags; /* flags for ic_main_sta_lock */
    spinlock_t                    ic_chan_change_lock;    /* lock to serialize more than one sequences
                                                            of chan-changes for VAPs */
    unsigned long                 ic_chan_change_lock_flags; /* flags for ic_chan_change_lock */

    spinlock_t                    ic_radar_found_lock; /* lock to serialize vap create and radar detection */

    spinlock_t                    ic_addba_lock; /* lock to protect  addba request and addba response*/
    u_int32_t                     ic_chanchange_serialization_flags; /* Channel change for all the VAPs together */
    ieee80211_ic_state_lock_t     ic_state_lock; /* lock to protect access to ic/sc state */
    spinlock_t                    ic_beacon_alloc_lock;
    unsigned long                 ic_beacon_alloc_lock_flags;

    TAILQ_HEAD(, ieee80211vap)    ic_vaps; /* list of vap instances */
    CLONE_PARAMS                  ic_cp;
    enum ieee80211_phytype        ic_phytype; /* XXX wrong for multi-mode */
    enum ieee80211_opmode         ic_opmode;  /* operation mode */

    u_int8_t                      ic_myaddr[IEEE80211_ADDR_LEN]; /* current mac address */
    u_int8_t                      ic_my_hwaddr[IEEE80211_ADDR_LEN]; /* mac address from EEPROM */

    u_int8_t                      ic_broadcast[IEEE80211_ADDR_LEN];

    u_int32_t                     ic_flags;       /* state flags */
    u_int32_t                     ic_flags_ext;   /* extension of state flags */
    u_int32_t                     ic_flags_ext2;  /* extension of state flags 2 */
    u_int32_t                     ic_chanswitch_flags;  /* Channel switch flags */
    u_int32_t                     ic_wep_tkip_htrate      :1, /* Enable htrate for wep and tkip */
                                  ic_non_ht_ap            :1, /* non HT AP found flag */
                                  ic_block_dfschan        :1, /* block the use of DFS channels flag */
                                  ic_doth                 :1, /* 11.h flag */
                                  ic_off_channel_support  :1, /* Off-channel support enabled */ //subrat
                                  ic_ht20Adhoc            :1,
                                  ic_ht40Adhoc            :1,
                                  ic_htAdhocAggr          :1,
                                  ic_disallowAutoCCchange :1, /* disallow CC change when assoc completes */
                                  ic_p2pDevEnable         :1, /* Is P2P Enabled? */
                                  ic_ignoreDynamicHalt    :1, /* disallowed  */
                                  ic_override_proberesp_ie:1, /* overwrite probe response IE with beacon IE */
                                  ic_wnm                  :1, /* WNM support flag */
                                  ic_2g_csa               :1,
                                  ic_dropstaquery         :1,
                                  ic_blkreportflood       :1,
                                  ic_offchanscan          :1,  /* Offchan scan */
                                  ic_consider_obss_long_slot :1, /*Consider OBSS non-erp to change to long slot*/
                                  ic_sta_vap_amsdu_disable:1, /* Disable Tx AMSDU for station vap */
                                  ic_enh_ind_rpt          :1, /* enhanced independent repeater  */
                                  ic_strict_pscan_enable  :1, /* do not send probe request in passive channel */
                                  ic_min_rssi_enable      :1, /* enable/disable min rssi cli block */
                                  ic_no_vlan              :1,
                                  ic_atf_logging          :1,
                                  ic_papi_enable          :1,
                                  ic_ht40intol_scan_running :1,
                                  ic_user_coext_disable   :1;
#define CONSIDER_OBSS_LONG_SLOT_DEFAULT 0



    u_int32_t                     ic_caps;        /* capabilities */
    u_int32_t                     ic_caps_ext;    /* extension of capabilities */
    u_int16_t                     ic_cipher_caps; /* cipher capabilities */
    u_int8_t                      ic_ath_cap;     /* Atheros adv. capablities */
    u_int8_t                      ic_roaming;     /* Assoc state machine roaming mode */
    void                          *ic_sta_vap;      /* the STA handle for vap up down */
    void                          *ic_mon_vap;      /* the Monitor handle for vap up down */
#if ATH_SUPPORT_WRAP
    u_int8_t                      ic_nstavaps;
    u_int8_t                      ic_nwrapvaps;     /* Number of active WRAP APs */
    u_int8_t                      ic_npstavaps;
    u_int8_t                      ic_proxystarxwar;   /* Proxy Sta mode Rx WAR */
    u_int8_t                      ic_nscanpsta;
    void                          *ic_mpsta_vap;    /* main proxy sta */
    struct wrap_com               *ic_wrap_com;     /* ic wrap dev specific */
    void                          *ic_wrap_vap;     /* wrap vap  */
    u_int8_t                      ic_wrap_vap_sgi_cap;   /* wrap vap configed by qcawifi with SGI to be leveraged for psta*/
    bool                          wrap_disable_da_war;
#if ATH_PROXY_NOACK_WAR
    struct {
    int                           blocking;
    qdf_semaphore_t                sem_ptr;
    }                             proxy_ast_reserve_wait;
    os_timer_t                    ic_ast_reserve_timer;
    qdf_atomic_t               ic_ast_reserve_event;
    int                           ic_ast_reserve_status;
#endif
#endif
#if ATH_SUPPORT_WIFIPOS
    u_int8_t                      ic_isoffchan;   /* WiFiPos going offchan */
    void                          *rtt_sock;        /* Socket for rtt-app */
#endif
    u_int32_t                     ic_ath_extcap;     /* Atheros extended capablities */
    u_int8_t                      ic_nopened; /* vap's been opened */
    u_int8_t                      ic_nr_share_radio_flag; /* indicate for which radios the NR response is */
    u_int8_t                      ic_nr_share_enable;     /* share enable per radio */
    struct ieee80211_rateset      ic_sup_rates[IEEE80211_MODE_MAX];
    struct ieee80211_rateset      ic_sup_half_rates;
    struct ieee80211_rateset      ic_sup_quarter_rates;
    struct ieee80211_rateset      ic_sup_ht_rates[IEEE80211_MODE_MAX];
    u_int64_t                     ic_modecaps;    /* set of mode capabilities */
    u_int64_t                     ic_current_modecaps; /* Do not populate radar found channel mode */
    u_int16_t                     ic_curmode;     /* current mode */
    u_int16_t                     ic_intval;      /* default beacon interval for AP, ADHOC */
    u_int16_t                     ic_lintval;     /* listen interval for STATION */
    u_int16_t                     ic_lintval_assoc;  /* listen interval to use in association for STATION */
    u_int16_t                     ic_holdover;    /* PM hold over duration */
    u_int16_t                     ic_bmisstimeout;/* beacon miss threshold (ms) */
    u_int16_t                     ic_txpowlimit;  /* global tx power limit */
    u_int16_t                     ic_uapsdmaxtriggers; /* max triggers that could arrive */
    u_int8_t                      ic_coverageclass; /* coverage class */
	u_int8_t					  ic_rssi_ctl[3]; /*RSSI of RX chains*/

    /* 11n (HT) state/capabilities */
    u_int16_t                     ic_htflags;            /* HT state flags */
    u_int16_t                     ic_htcap;              /* HT capabilities */
    u_int16_t                     ic_htextcap;           /* HT extended capabilities */
    u_int16_t                     ic_ldpccap;            /* HT ldpc capabilities */
    u_int8_t                      ic_maxampdu;           /* maximum rx A-MPDU factor */
    u_int8_t                      ic_mpdudensity;        /* MPDU density */
    u_int8_t                      ic_mpdudensityoverride;/* MPDU density override flag and value */
    u_int8_t                      ic_enable2GHzHt40Cap;  /* HT40 supported on 2GHz channels */
    u_int8_t                      ic_weptkipaggr_rxdelim; /* Atheros proprietary wep/tkip aggr mode - rx delim count */
    u_int16_t                     ic_channelswitchingtimeusec; /* Channel Switching Time in usec */
    u_int8_t                      ic_no_weather_radar_chan;  /* skip weather radar channels*/
    u_int8_t                      ic_implicitbf;
#ifdef ATH_SUPPORT_TxBF
    union ieee80211_hc_txbf       ic_txbf;                /* Tx Beamforming capabilities */
    void    (*ic_v_cv_send)(struct ieee80211_node *ni, u_int8_t	*data_buf,
                u_int16_t buf_len);
    int     (*ic_txbf_alloc_key)(struct ieee80211com *ic, struct ieee80211_node *ni);
    void    (*ic_txbf_set_key)(struct ieee80211com *ic, struct ieee80211_node *ni);
    int     (*ic_txbf_del_key)(struct ieee80211com *ic, struct ieee80211_node *ni);
    void    (*ic_init_sw_cv_timeout)(struct ieee80211com *ic, struct ieee80211_node *ni);
	int     (*ic_set_txbf_caps)(struct ieee80211com *ic);
#ifdef TXBF_DEBUG
	void    (*ic_txbf_check_cvcache)(struct ieee80211com *ic, struct ieee80211_node *ni);
#endif
	void    (*ic_txbf_stats_rpt_inc)(struct ieee80211com *ic, struct ieee80211_node *ni);
	void    (*ic_txbf_set_rpt_received)(struct ieee80211com *ic, struct ieee80211_node *ni);
#endif
#if ATH_SUPPORT_WRAP
    int     (*ic_wrap_set_key)(struct ieee80211vap *vap);
    int     (*ic_wrap_del_key)(struct ieee80211vap *vap);
#endif
#if IEEE80211_DEBUG_REFCNT
#define IC_IEEE80211_FIND_NODE(_ic,_nt,_macaddr)  (_ic)->ic_ieee80211_find_node_debug((_nt),(_macaddr), __FUNCTION__, __LINE__, __FILE__)
    struct  ieee80211_node*  (*ic_ieee80211_find_node_debug)(struct ieee80211_node_table *nt, const u_int8_t *macaddr, const char * func, int line, const char *file);
#else
#define IC_IEEE80211_FIND_NODE(_ic,_nt,_macaddr)  (_ic)->ic_ieee80211_find_node((_nt),(_macaddr))
    struct  ieee80211_node*  (*ic_ieee80211_find_node)(struct ieee80211_node_table *nt, const u_int8_t *macaddr);
#endif //IEEE80211_DEBUG_REFCNT
    /* Returns type of driver : Direct attach / Offload */
    bool    (*ic_is_mode_offload)(struct ieee80211com *ic);
    void    (*ic_if_mgmt_drain) (struct ieee80211_node *ni, int force);
    void    (*ic_ieee80211_unref_node)(struct ieee80211_node *ni);
    bool    (*ic_is_macreq_enabled)(struct ieee80211com *ic);
    u_int32_t (*ic_get_mac_prealloc_idmask)(struct ieee80211com *ic);
    void    (*ic_set_mac_prealloc_id)(struct ieee80211com *ic, u_int8_t id, u_int8_t set);
    u_int8_t (*ic_get_qwrap_num_vdevs)(struct ieee80211com *ic);
    void    (*ic_update_target_caps)(struct ieee80211com *ic, enum ieee80211_phymode mode);
    uint8_t (*ic_get_bsta_fixed_idmask)(struct ieee80211com *ic);
#ifdef OBSS_PD
    int	    (*ic_spatial_reuse)(struct ieee80211com *ic, struct ieee80211vap *vap);
    bool    (*ic_is_spatial_reuse_enabled)(struct ieee80211com *ic);
#endif
    /*
     * 11n A-MPDU and A-MSDU support
     */
    int                           ic_ampdu_limit;     /* A-MPDU length limit */
    int                           ic_ampdu_density;   /* A-MPDU density */
    int                           ic_ampdu_subframes; /* A-MPDU subframe limit */
    int                           ic_amsdu_limit;     /* A-MSDU length limit */
    u_int32_t                     ic_amsdu_max_size;  /* A-MSDU buffer max size */

    /*
     * Global chain select mask for tx and rx. This mask is used only for 11n clients.
     * For legacy clients this mask is over written by a tx chain mask of 1. Rx chain
     * mask remains the global value for legacy clients.
     */
    u_int8_t                      ic_tx_chainmask;
    u_int8_t                      ic_rx_chainmask;

    /* actual number of rx/tx chains supported by HW */
    u_int8_t                      ic_num_rx_chain;
    u_int8_t                      ic_num_tx_chain;

#if ATH_SUPPORT_WAPI
    /* Max number of rx/tx chains supported by WAPI HW engine */
    u_int8_t                      ic_num_wapi_tx_maxchains;
    u_int8_t                      ic_num_wapi_rx_maxchains;
#endif
    /*
     * Number of spatial streams supported.
     */
    u_int8_t                      ic_spatialstreams;

    u_int8_t                      ic_multiDomainEnabled;
    IEEE80211_COUNTRY_ENTRY       ic_country;   /* Current country/regdomain */
    u_int8_t                      ic_isdfsregdomain; /* operating in DFS domain ? */
    qdf_atomic_t ic_fips_event; /* Set when wmi_fips_event occurs*/
    struct ath_fips_output *ic_output_fips;
    /*
     * Channel state:
     *
     * ic_channels is the set of available channels for the device;
     *    it is setup by the driver
     * ic_nchans is the number of valid entries in ic_channels
     * ic_chan_avail is a bit vector of these channels used to check
     *    whether a channel is available w/o searching the channel table.
     * ic_chan_active is a (potentially) constrained subset of
     *    ic_chan_avail that reflects any mode setting or user-specified
     *    limit on the set of channels to use/scan
     * ic_curchan is the current channel the device is set to; it may
     *    be different from ic_bsschan when we are off-channel scanning
     *    or otherwise doing background work
     * ic_bsschan is the channel selected for operation; it may
     *    be undefined (IEEE80211_CHAN_ANYC)
     * ic_prevchan is a cached ``previous channel'' used to optimize
     *    lookups when switching back+forth between two channels
     *    (e.g. for dynamic turbo)
     */
    int                           ic_nchans;  /* # entries in ic_channels */
    struct ieee80211_ath_channel      ic_channels[IEEE80211_CHAN_MAX+1];
    u_int8_t                      ic_chan_avail[IEEE80211_CHAN_BYTES];
    u_int8_t                      ic_chan_active[IEEE80211_CHAN_BYTES];
    struct ieee80211_ath_channel      *ic_curchan;   /* current channel */
    struct ieee80211_ath_channel      *ic_prevchan;  /* previous channel */
    uint8_t                       ic_chanidx; /*active index for chanhist*/
    struct ieee80211_chanhist     ic_chanhist[IEEE80211_CHAN_MAXHIST];
                                              /*history of channel change*/

#if WLAN_SUPPORT_PRIMARY_ALLOWED_CHAN
     ieee80211_primary_allowed_chanlist_t *ic_primary_chanlist;
     bool                                  ic_primary_allowed_enable;
#endif

    struct mi_node *ic_miroot; /* EXTAP MAC - IP table Root */

    /* regulatory class ids */
    u_int                         ic_nregclass;  /* # entries in ic_regclassids */
    u_int8_t                      ic_regclassids[IEEE80211_REGCLASSIDS_MAX];

    struct ieee80211_node_table   ic_sta; /* stations/neighbors */

#if IEEE80211_DEBUG_NODELEAK
    TAILQ_HEAD(, ieee80211_node)  ic_nodes;/* information of all nodes */
    ieee80211_node_lock_t         ic_nodelock;    /* on the list */
#endif
    os_timer_t                    ic_inact_timer;
    os_timer_t                    ic_obss_nb_ru_tolerance_timer; /* Obss Narrow Bandwidth tolerance timer */
    uint32_t                      ic_obss_nb_ru_tolerance_time; /* Current Obss Narrow Bandwidth tolerance time */
#if UMAC_SUPPORT_WNM
    os_timer_t                    ic_bssload_timer;
    u_int32_t                     ic_wnm_bss_count;
    u_int32_t                     ic_wnm_bss_active;

#endif

    /* Default table for WME params */
    struct wme_phyParamType phyParamForAC[WME_NUM_AC][IEEE80211_MODE_MAX];
    struct wme_phyParamType bssPhyParamForAC[WME_NUM_AC][IEEE80211_MODE_MAX];
    /* XXX multi-bss: split out common/vap parts? */
    struct ieee80211_wme_state    ic_wme;  /* WME/WMM state */
    struct ieee80211_wme_tspec    ic_sigtspec; /* Signalling tspec */
    struct ieee80211_wme_tspec    ic_datatspec; /* Data tspec */

    /* Default MUEDCA parameters */
    struct muedcaParams           ic_muedca_defaultParams[MUEDCA_NUM_AC];

    enum ieee80211_protmode       ic_protmode;    /* 802.11g protection mode */
    u_int16_t                     ic_sta_assoc;    /* stations associated */
    u_int16_t                     ic_nonerpsta;   /* # non-ERP stations */
    u_int16_t                     ic_longslotsta; /* # long slot time stations */
    u_int16_t                     ic_ht_sta_assoc;/* ht capable stations */
#if ATH_TxBF_DYNAMIC_LOF_ON_N_CHAIN_MASK
    u_int16_t                     ic_ht_txbf_sta_assoc;/* ht txbf capable stations */
#endif
    u_int16_t                     ic_ht_gf_sta_assoc;/* ht greenfield capable stations */
    u_int16_t                     ic_ht40_sta_assoc; /* ht40 capable stations */
    systime_t                     ic_last_non_ht_sta; /* the last time a non ht sta is seen on channel */
    systime_t                     ic_last_nonerp_present; /* the last time a non ERP beacon is seen on channel */
    int8_t                        ic_ht20_only;

    /*
     *  current channel max power, used to compute Power Constraint IE.
     *
     *  NB: local power constraint depends on the channel, but assuming it must
     *     be detected dynamically, we cannot maintain a table (i.e., will not
     *     know value until change to channel and detect).
     */
    u_int8_t                      ic_curchanmaxpwr;
    u_int8_t                      ic_chanchange_tbtt;
    u_int8_t                      ic_rcsa_count;
    u_int8_t                      ic_chanchange_chan;
    u_int16_t                     ic_chanchange_chwidth;
    u_int8_t                      ic_chanchange_secoffset;
    struct ieee80211_ath_channel      *ic_chanchange_channel;
    u_int64_t                     ic_chanchange_chanflag; //e.g. IEEE80211_CHAN_11AC_VHT40PLUS
    u_int32_t                     ic_mcst_of_rootap; /* Maximum channel switch time received from the root AP */
    bool                          ic_has_rootap_done_cac; /* Check if the RootAP has done CAC in the target channel */
    struct ieee80211_ath_channel     *ic_tx_next_ch;
#if !ATH_SUPPORT_STATS_APONLY
    struct ieee80211_phy_stats    ic_phy_stats[IEEE80211_MODE_MAX]; /* PHY counters */
#endif

    /*
     *  RX filter desired by upper layers.  Note this contains some bits which
     *  must be filtered by sw since the hw supports only a subset of possible
     *  filter actions.
     */
    u_int32_t                     ic_os_rxfilter;
    u_int32_t                     ic_os_monrxfilter;
    ieee80211_resmgr_t            ic_resmgr; /* opage handle to resource manager */

    ieee80211_tsf_timer_mgr_t     ic_tsf_timer;     /* opaque handle to TSF Timer manager */

    ieee80211_notify_tx_bcn_mgr_t ic_notify_tx_bcn_mgr; /* opaque handle to "Notify Tx Beacon" manager */

    u_int8_t interface_id;       /* wifi interface number. eg. 1 for wifi1 */
    /* sta_not_connected_cfg: keeps track of channel configuration.
     * 0 - Proper channel configured,
     * 1 - channel zero configured to help scanning.
     */
    bool sta_not_connected_cfg;
    bool vap_down_in_progress;   /* 1 - vap down in progress, 0 - no pending vap down. */

#ifdef ATH_COALESCING
    /*
     * NB Tx Coalescing is a per-device setting to prevent non-PCIe (especially cardbus)
     * 11n devices running into underrun conditions on certain PCI chipsets.
     */
    ieee80211_coalescing_state    ic_tx_coalescing;
#endif
    IEEE80211_REG_PARAMETERS      ic_reg_parm;
    wlan_dev_event_handler_table  *ic_evtable[IEEE80211_MAX_DEVICE_EVENT_HANDLERS];
    void                          *ic_event_arg[IEEE80211_MAX_DEVICE_EVENT_HANDLERS];

    struct asf_print_ctrl         ic_print;
    u_int8_t                      ic_num_ap_vaps;       /* Number of AP VAPs */
    u_int8_t                      ic_num_lp_iot_vaps;   /* Number of AP VAPs in lp_iot mode*/
    u_int8_t                      ic_need_vap_reinit;   /* Flag to re-intialize all VAPs */
    u_int8_t                      ic_intolerant40;      /* intolerant 40 */
    u_int8_t                      ic_enable2040Coexist; /* Enable 20/40 coexistence */
    u_int8_t                      ic_tspec_active;      /* TSPEC is negotiated */
    u_int8_t                      cw_inter_found;   /* Flag to handle CW interference */
    u_int8_t                      ic_eacs_done;     /* Flag to indicate eacs status */
    ieee80211_acs_t               ic_acs;   /* opaque handle to acs */
    struct external_acs_obj       ic_extacs_obj;    /* object for external channel selection */
    ieee80211_cbs_t               ic_cbs;   /* opaque handle to cbs */
    u_int32_t                     ic_acs_ctrlflags;   /* Flags to control ACS*/
    struct ieee80211_key          ic_bcast_proxykey;   /* default/broadcast for proxy STA */
    bool                          ic_softap_enable; /* For Lenovo SoftAP */

#ifdef ATH_BT_COEX
    enum ieee80211_opmode         ic_bt_coex_opmode; /* opmode for BT coex */
#endif

    u_int32_t                     ic_minframesize; /* Min framesize that can be recelived */

    /*
     * Table of registered cipher modules.
     */
    const struct ieee80211_cipher *ciphers[IEEE80211_CIPHER_MAX];
    int                           ic_ldpcsta_assoc;

    u_int32_t                     ic_chanbwflag;

#if ATH_SUPPORT_DSCP_OVERRIDE
    #define NUM_DSCP_MAP 16
    #define IP_DSCP_SHIFT 2
    #define IP_DSCP_MASK 0x3f
    #define IP_DSCP_MAP_LEN 64
    u_int8_t ic_override_dscp;
    u_int32_t ic_dscp_tid_map[NUM_DSCP_MAP][IP_DSCP_MAP_LEN];
    u_int8_t ic_override_igmp_dscp;
    u_int8_t ic_dscp_igmp_tid;
    u_int8_t ic_override_hmmc_dscp;
    u_int8_t ic_dscp_hmmc_tid;
#endif
#if UMAC_SUPPORT_ADMCTL
    u_int16_t                     ic_mediumtime_reserved;
    void 						  (*ic_node_update_dyn_uapsd)(struct ieee80211_node *ni, uint8_t ac, int8_t deli, int8_t trig);
    void 						  *ic_admctl_rt[IEEE80211_MODE_MAX];
#endif
    u_int16_t                     ic_custom_scan_order[IEEE80211_CUSTOM_SCAN_ORDER_MAXSIZE];
    u_int32_t                     ic_custom_scan_order_size;
    u_int32_t                     ic_custom_chan_list_associated[IEEE80211_CUSTOM_SCAN_ORDER_MAXSIZE];
    u_int32_t                     ic_custom_chanlist_assoc_size;
    u_int32_t                     ic_custom_chan_list_nonassociated[IEEE80211_CUSTOM_SCAN_ORDER_MAXSIZE];
    u_int32_t                     ic_custom_chanlist_nonassoc_size;
    u_int32_t                     ic_use_custom_chan_list;

    /* virtual ap create/delete */
    struct ieee80211vap     *(*ic_vap_create_pre_init)(struct vdev_mlme_obj *, int flags);
    QDF_STATUS              (*ic_vap_create_post_init)(struct vdev_mlme_obj *, int flags);
    QDF_STATUS              (*ic_vap_create_init)(struct vdev_mlme_obj *);
    QDF_STATUS              (*ic_nss_vap_create)(struct vdev_mlme_obj *);
    QDF_STATUS              (*ic_nss_vap_destroy)(struct wlan_objmgr_vdev *);
    void                    (*ic_vap_delete)(struct wlan_objmgr_vdev *);
    void                    (*ic_vap_post_delete)(struct ieee80211vap *);
    void                    (*ic_vap_free)(struct ieee80211vap *);
    int                     (*ic_vap_alloc_macaddr) (struct ieee80211com *ic, u_int8_t *bssid);
    int                     (*ic_vap_free_macaddr) (struct ieee80211com *ic, u_int8_t *bssid);

    /* send management frame to driver, like hardstart */
    int                     (*ic_mgtstart)(struct ieee80211com *, wbuf_t);

    /* reset device state after 802.11 parameter/state change */
    int                     (*ic_init)(struct ieee80211com *);
    int                     (*ic_reset_start)(struct ieee80211com *, bool no_flush);
    int                     (*ic_reset)(struct ieee80211com *);
    int                     (*ic_reset_end)(struct ieee80211com *, bool no_flush);
    int                     (*ic_start)(struct ieee80211com *);
    int                     (*ic_stop)(struct ieee80211com *, bool flush);

    /* macaddr */
    int                     (*ic_set_macaddr)(struct ieee80211com *, u_int8_t *macaddr);

    /* regdomain */
    void                    (*ic_get_currentCountry)(struct ieee80211com *,
                                                     IEEE80211_COUNTRY_ENTRY *ctry);
    int                     (*ic_set_country)(struct ieee80211com *, char *isoName, u_int16_t cc, enum ieee80211_clist_cmd cmd);
    int                     (*ic_set_regdomain)(struct ieee80211com *, int regdomain, bool no_chanchange);
    int                     (*ic_get_regdomain)(struct ieee80211com *);
    int                     (*ic_set_quiet)(struct ieee80211_node *, u_int8_t *quiet_elm);
    void                    (*ic_set_bcn_offload_quiet_mode) (wlan_if_t vap, struct ieee80211com *ic);

    /* update device state for 802.11 slot time change */
    void                    (*ic_updateslot)(struct ieee80211com *);

    /* new station association callback/notification */
    void                    (*ic_newassoc)(struct ieee80211_node *, int);

    /* notify received beacon */
    void                    (*ic_beacon_update)(struct ieee80211_node *, int rssi);

    void  (*ic_beacon_free)(struct ieee80211com *ic, int if_id);
    /* node state management */
    struct ieee80211_node   *(*ic_node_alloc)(struct ieee80211vap *, const u_int8_t *macaddr, bool tmpnode, void *peer);
    void                    (*ic_node_free)(struct ieee80211_node *);
    void                    (*ic_preserve_node_for_fw_delete_resp)(struct ieee80211_node *);
    void                    (*ic_node_cleanup)(struct ieee80211_node *);
    u_int8_t                (*ic_node_getrssi)(const struct ieee80211_node *, int8_t ,u_int8_t);
    u_int32_t               (*ic_node_getrate)(const struct ieee80211_node *, u_int8_t type);
    void                    (*ic_node_authorize)(struct ieee80211_node *,u_int32_t authorize);
    void                    (*ic_get_min_and_max_power)(struct ieee80211com *ic,
                                                        int8_t *max_tx_power,
                                                        int8_t *min_tx_power);
    bool                    (*ic_is_regulatory_offloaded)(struct ieee80211com *ic);
    uint32_t                (*ic_get_modeSelect)(struct ieee80211com *ic);
    uint32_t                (*ic_get_chip_mode)(struct ieee80211com *ic);
    void                    (*ic_fill_hal_chans_from_reg_db)(struct ieee80211com *ic);
    u_int32_t                (*ic_node_get_last_txpower)(const struct ieee80211_node *);
    /* scanning support */
    void                    (*ic_scan_start)(struct ieee80211com *);
    void                    (*ic_scan_end)(struct ieee80211com *);
    void                    (*ic_led_scan_start)(struct ieee80211com *);
    void                    (*ic_led_scan_end)(struct ieee80211com *);
    int                     (*ic_set_channel)(struct ieee80211com *);
    void                    (*ic_enable_radar)(struct ieee80211com *, int no_cac);
    void                    (*ic_process_rcsa)(struct ieee80211com *);

#if ATH_SUPPORT_DFS && ATH_SUPPORT_STA_DFS
    void                    (*ic_enable_sta_radar)(struct ieee80211com *, int no_cac);
#endif
#if ATH_SUPPORT_LOWI
    int                     (*ic_lowi_frame_send)(struct ieee80211com *ic, int msg_len, void *req, int msgsubType);
#endif
#if ATH_SUPPORT_WIFIPOS
    int                     (*ic_lean_set_channel)(struct ieee80211com *);
    void                    (*ic_pause_node)(struct ieee80211com *, struct ieee80211_node *, bool pause );
    void                    (*ic_resched_txq)(struct ieee80211com *ic);
    bool                    (*ic_disable_hwq)(struct ieee80211com *ic, u_int16_t mask);
    int                     (*ic_get_channel_busy_info)(struct ieee80211com *ic, u_int32_t *rxclear_pct, u_int32_t *rxframe_pct, u_int32_t *txframe_pct);
    int                     (*ic_vap_reap_txqs)(struct ieee80211com *ic, struct ieee80211vap *vap);
    int32_t                 (*ic_update_wifipos_stats)(struct ieee80211com *ic, struct ieee80211_ol_wifiposdesc *wifiposdesc);
    u_int64_t               (*ic_get_TSFTSTAMP)(struct ieee80211com *ic);
    int32_t                 (*ic_update_ka_done)(u_int8_t *sta_mac_addr, u_int8_t status);
    int32_t                 (*ic_rtt_init_netlink)(struct ieee80211com *ic);
    int                     (*ic_xmitrtt3)(struct ieee80211vap *vap,
                                            u_int8_t *mac_addr, int extra);
#endif
    /* change power state */
    void                    (*ic_pwrsave_set_state)(struct ieee80211com *, IEEE80211_PWRSAVE_STATE newstate);

    /* update protmode */
    void                    (*ic_update_protmode)(struct ieee80211com *);

    /* get hardware txq depth */
    u_int32_t               (*ic_txq_depth)(struct ieee80211com *);
    /* get hardware txq depth per ac*/
    u_int32_t               (*ic_txq_depth_ac)(struct ieee80211com *, int ac);

    /* mhz to ieee conversion */
    u_int                   (*ic_mhz2ieee)(struct ieee80211com *, u_int freq, u_int64_t flags);

    /* channel width change */
    void                    (*ic_chwidth_change)(struct ieee80211_node *ni);

    /* Nss change */
    void                    (*ic_nss_change)(struct ieee80211_node *ni);

    /* beeliner Firmware Test command */
    void                    (*ic_ar900b_fw_test)(struct ieee80211com *ic, u_int32_t arg, u_int32_t value);
    /* Firmware Unit Test command Lithium onwards*/
    int32_t                 (*ic_fw_unit_test)(struct ieee80211vap *, struct ieee80211_fw_unit_test_cmd *);

    /* coex configuration */
    int                     (*ic_coex_cfg)(struct ieee80211vap *vap, u_int32_t type, u_int32_t *arg);

    /* Dynamic antenna switch */
    void                    (*ic_set_ant_switch)(struct ieee80211com *ic, u_int32_t ctrl_cmd_1, u_int32_t ctrl_cmd_2);
    /* Set user control table */
    void                    (*ic_set_ctrl_table)(struct ieee80211com *ic, u_int8_t *ctl_array, u_int16_t ctl_len);
    /* aggregation support */
    u_int8_t                ic_addba_mode; /* ADDBA mode auto or manual */
    void                    (*ic_set_ampduparams)(struct ieee80211_node *ni);
    void                    (*ic_set_weptkip_rxdelim)(struct ieee80211_node *ni, u_int8_t rxdelim);
    void                    (*ic_addba_requestsetup)(struct ieee80211_node *ni,
                                                     u_int8_t tidno,
                                                     struct ieee80211_ba_parameterset *baparamset,
                                                     u_int16_t *batimeout,
                                                     struct ieee80211_ba_seqctrl *basequencectrl,
                                                     u_int16_t buffersize
                                                     );
    int                     (*ic_addba_responsesetup)(struct ieee80211_node *ni,
                                                      u_int8_t tidno,
                                                      u_int8_t *dialogtoken, u_int16_t *statuscode,
                                                      struct ieee80211_ba_parameterset *baparamset,
                                                      u_int16_t *batimeout
                                                      );
    int                     (*ic_addba_resp_tx_completion)(struct ieee80211_node *ni,
                                                       u_int8_t tidno, int status
                                                       );
    int                     (*ic_addba_requestprocess)(struct ieee80211_node *ni,
                                                      u_int8_t dialogtoken,
                                                      struct ieee80211_ba_parameterset *baparamset,
                                                      u_int16_t batimeout,
                                                      struct ieee80211_ba_seqctrl basequencectrl);
    void                    (*ic_addba_responseprocess)(struct ieee80211_node *ni,
                                                        u_int16_t statuscode,
                                                        struct ieee80211_ba_parameterset *baparamset,
                                                        u_int16_t batimeout
                                                        );
    void                    (*ic_addba_clear)(struct ieee80211_node *ni);
    void                    (*ic_delba_process)(struct ieee80211_node *ni,
                                                struct ieee80211_delba_parameterset *delbaparamset,
                                                u_int16_t reasoncode
                                                );
    int                     (*ic_addba_send)(struct ieee80211_node *ni,
                                             u_int8_t tid,
                                             u_int16_t buffersize);
    void                    (*ic_delba_send)(struct ieee80211_node *ni,
                                             u_int8_t tid,
                                             u_int8_t initiator,
                                             u_int16_t reasoncode);
    void                    (*ic_addba_status)(struct ieee80211_node *ni,
                                               u_int8_t tid,
                                               u_int16_t *status);
    void                    (*ic_addba_setresponse)(struct ieee80211_node *ni,
                                                    u_int8_t tid,
                                                    u_int16_t statuscode);
    int                     (*ic_delba_tx_completion)(struct ieee80211_node *ni,
                                                u_int8_t tidno, int status);
    void                    (*ic_send_singleamsdu)(struct ieee80211_node *ni,
                                                    u_int8_t tid);
    void                    (*ic_addba_clearresponse)(struct ieee80211_node *ni);
    void                    (*ic_sm_pwrsave_update)(struct ieee80211_node *ni, int, int, int);
    /* update station power save state when operating in AP mode */
    void                    (*ic_node_psupdate)(struct ieee80211_node *, int, int);

    /* To get the number of frames queued up in lmac */
    int                     (*ic_node_queue_depth)(struct ieee80211_node *);

    int16_t                 (*ic_get_noisefloor)(struct ieee80211com *, struct ieee80211_ath_channel *, int);
    void                    (*ic_get_chainnoisefloor)(struct ieee80211com *, struct ieee80211_ath_channel *, int16_t *);
#if ATH_SUPPORT_VOW_DCS
    void                    (*ic_disable_dcsim)(struct ieee80211com *);
    void                    (*ic_enable_dcsim)(struct ieee80211com *);
#endif
    void                    (*ic_disable_dcscw)(struct ieee80211com *);
    void                    (*ic_dcs_restore)(struct ieee80211com *);
    void                    (*ic_set_config)(struct ieee80211vap *);
    bool                    (*ic_set_config_enable)(struct ieee80211vap *);

    void                    (*ic_set_safemode)(struct ieee80211vap *, int);
        /* rx monitor filter */
    int                     (*ic_set_rx_monitor_filter)(struct ieee80211com *, uint32_t);
	/* rx filter related */
    void                    (*ic_set_privacy_filters)(struct ieee80211vap *);

    int                     (*ic_rmgetcounters)(struct ieee80211com *ic, struct ieee80211_mib_cycle_cnts *pCnts);

    void                    (*ic_set_rx_sel_plcp_header)(struct ieee80211com *ic,
                                                         int8_t sel, int8_t query);

    int                     (*ic_subscribe_csa_interop_phy)(struct ieee80211com *ic,
                                                            bool subscribe);

    u_int8_t                (*ic_rcRateValueToPer)(struct ieee80211com *ic, struct ieee80211_node *ni, int txRateKbps);

    /* to get TSF values */
    u_int32_t               (*ic_get_TSF32)(struct ieee80211com *ic);
#ifdef ATH_USB
    /* to get generic timer expired tsf time, this is used to reduce the wmi command delay */
    u_int32_t               (*ic_get_target_TSF32)(struct ieee80211com *ic);
#endif
    u_int64_t               (*ic_get_TSF64)(struct ieee80211com *ic);

    /* To Set Transmit Power Limit */
    void                    (*ic_set_txPowerLimit)(struct ieee80211com *ic, u_int32_t limit, u_int16_t tpcInDb, u_int32_t is2Ghz);

	void                    (*ic_set_txPowerAdjust)(struct ieee80211com *ic, int32_t adjust, u_int32_t is2Ghz);

    /* To get Transmit power in 11a common mode */
    u_int8_t                (*ic_get_common_power)(struct ieee80211com *ic, struct ieee80211_ath_channel *chan);

    /* To get Maximum Transmit Phy rate */
    u_int32_t               (*ic_get_maxphyrate)(struct ieee80211com *ic, struct ieee80211_node *ni);

    /* Set Rx Filter */
    void                    (*ic_set_rxfilter)(struct ieee80211com *ic,u_int32_t filter);

    /* Get Mfg Serial Num */
    int                     (*ic_get_mfgsernum)(struct ieee80211com *ic, u_int8_t *pSn, int limit);

    /* Get Current RSSI */
    u_int32_t               (*ic_get_curRSSI)(struct ieee80211com *ic);

#ifdef ATH_SWRETRY
    /* Enable/Disable software retry */
    void                    (*ic_set_swretrystate)(struct ieee80211_node *ni, int flag);
    /* Schedule one single frame in LMAC upon PS-Poll, return 0 if scheduling a LMAC frame successfully */
    int                     (*ic_handle_pspoll)(struct ieee80211com *ic, struct ieee80211_node *ni);
    /* Check whether there is pending frame in LMAC tid Q's, return 0 if there exists */
    int                     (*ic_exist_pendingfrm_tidq)(struct ieee80211com *ic, struct ieee80211_node *ni);
    int                     (*ic_reset_pause_tid)(struct ieee80211com *ic, struct ieee80211_node *ni);
#endif
    u_int32_t               (*ic_get_wpsPushButton)(struct ieee80211com *ic);
    void            (*ic_green_ap_set_enable)( struct ieee80211com *ic, int val );
    int             (*ic_green_ap_get_enable)( struct ieee80211com *ic);
    void            (*ic_green_ap_set_print_level)(struct ieee80211com *ic, int val);
    int             (*ic_green_ap_get_print_level)(struct ieee80211com *ic);
    int16_t         (*ic_get_cur_chan_nf)(struct ieee80211com *ic);
    int16_t         (*ic_get_cur_hw_nf)(struct ieee80211com *ic);
    bool            (*ic_is_target_lithium)(struct wlan_objmgr_psoc *psoc);
    void            (*ic_get_cur_chan_stats)(struct ieee80211com *ic, struct ieee80211_chan_stats *chan_stats);
    int32_t         (*ic_ath_send_rssi)(struct ieee80211com *ic, u_int8_t *macaddr, struct ieee80211vap *vap);
    int32_t         (*ic_ath_request_stats)(struct ieee80211com *ic,  void *cmd);
    int32_t         (*ic_ath_enable_ap_stats)(struct ieee80211com *ic, u_int8_t val);
    int32_t         (*ic_ath_bss_chan_info_stats)(struct ieee80211com *ic, int param);
    /* update PHY counters */
    void                    (*ic_update_phystats)(struct ieee80211com *ic, enum ieee80211_phymode mode);
    void                    (*ic_clear_phystats)(struct ieee80211com *ic);
    void                    (*ic_log_text)(struct ieee80211com *ic,char *text);
    void                    (*ic_log_text_bh)(struct ieee80211com *ic,char *text);
    int                     (*ic_set_chain_mask)(struct ieee80211com *ic, ieee80211_device_param type,
                                                 u_int32_t mask);
    bool                    (*ic_need_beacon_sync)(struct ieee80211com *ic); /* check if ath is waiting for beacon sync */
#if IEEE80211_DEBUG_NODELEAK
    void                    (*ic_print_nodeq_info)(struct ieee80211_node *ni);
#endif
    void                    (*ic_bss_to40)(struct ieee80211com *);
    void                    (*ic_bss_to20)(struct ieee80211com *);

    enum ieee80211_cwm_extprotmode    (*ic_cwm_get_extprotmode)(struct ieee80211com *ic);
    enum ieee80211_cwm_extprotspacing (*ic_cwm_get_extprotspacing)(struct ieee80211com *ic);
    enum ieee80211_cwm_mode           (*ic_cwm_get_mode)(struct ieee80211com *ic);
    enum ieee80211_cwm_width          (*ic_cwm_get_width)(struct ieee80211com *ic);
    u_int32_t                         (*ic_cwm_get_enable)(struct ieee80211com *ic);
    u_int32_t                         (*ic_cwm_get_extbusythreshold)(struct ieee80211com *ic);
    int8_t                            (*ic_cwm_get_extoffset)(struct ieee80211com *ic);

    void                              (*ic_cwm_set_extprotmode)(struct ieee80211com *ic, enum ieee80211_cwm_extprotmode mode);
    void                              (*ic_cwm_set_extprotspacing)(struct ieee80211com *ic, enum ieee80211_cwm_extprotspacing sp);
    void                              (*ic_cwm_set_enable)(struct ieee80211com *ic, u_int32_t en);
    void                              (*ic_cwm_set_extoffset)(struct ieee80211com *ic, int8_t val);
    void                              (*ic_cwm_set_extbusythreshold)(struct ieee80211com *ic, u_int32_t threshold);
    void                              (*ic_cwm_set_mode)(struct ieee80211com *ic, enum ieee80211_cwm_mode mode);
    void                              (*ic_cwm_set_width)(struct ieee80211com *ic, enum ieee80211_cwm_width width);

    struct ieee80211_tsf_timer *
                            (*ic_tsf_timer_alloc)(struct ieee80211com *ic,
                                                    tsftimer_clk_id clk_id,
                                                    ieee80211_tsf_timer_function trigger_action,
                                                    ieee80211_tsf_timer_function overflow_action,
                                                    ieee80211_tsf_timer_function outofrange_action,
                                                    void *arg);

    void                    (*ic_tsf_timer_free)(struct ieee80211com *ic, struct ieee80211_tsf_timer *timer);

    void                    (*ic_tsf_timer_start)(struct ieee80211com *ic, struct ieee80211_tsf_timer *timer,
                                                    u_int32_t timer_next, u_int32_t period);

    void                    (*ic_tsf_timer_stop)(struct ieee80211com *ic, struct ieee80211_tsf_timer *timer);

#define IEEE80211_IS_TAPMON_ENABLED(__ic) 0
#if ATH_SUPPORT_WIRESHARK
    void            (*ic_fill_radiotap_hdr)(struct ieee80211com *ic, struct ath_rx_radiotap_header *rh, wbuf_t wbuf);
#endif /* ATH_SUPPORT_WIRESHARK */
#ifdef ATH_BT_COEX
    int                     (*ic_get_bt_coex_info)(struct ieee80211com *ic, u_int32_t infoType);
#endif
    /* Get MFP support */
    u_int32_t               (*ic_get_mfpsupport)(struct ieee80211com *ic);

    /* Set hw MFP Qos bits */
    void                    (*ic_set_hwmfpQos)(struct ieee80211com *ic, u_int32_t dot11w);
#ifdef ATH_SUPPORT_IQUE
    /* Set IQUE parameters: AC, Rate Control, and HBR parameters */
    void        (*ic_set_acparams)(struct ieee80211com *ic, u_int8_t ac, u_int8_t use_rts,
                                   u_int8_t aggrsize_scaling, u_int32_t min_kbps);
    void        (*ic_set_rtparams)(struct ieee80211com *ic, u_int8_t rt_index,
                                   u_int8_t perThresh, u_int8_t probeInterval);
    void        (*ic_get_iqueconfig)(struct ieee80211com *ic);
    void        (*ic_set_hbrparams)(struct ieee80211vap *, u_int8_t ac, u_int8_t enable, u_int8_t per);
#endif /*ATH_SUPPORT_IQUE*/

    u_int32_t   (*ic_get_goodput)(struct ieee80211_node *ni);

#if WLAN_SPECTRAL_ENABLE

    /* SPECTRAL Related data */
    int     (*ic_spectral_control)(struct ieee80211com *ic, u_int id,
                              void *indata, u_int32_t insize,
                              void *outdata, u_int32_t *outsize);

    /*  EACS with spectral analysis*/
    void                 (*ic_start_spectral_scan)(struct ieee80211com *ic, u_int8_t priority);
    void                 (*ic_stop_spectral_scan)(struct ieee80211com *ic);

    /* Note: We have an option of using a structure to pass the arguments.
       However, the structure would have to be available across layers.
       The added complexity is currently not worth it since the below function
       is not intended to be called in the performance path. */
    void                 (*ic_record_chan_info)(struct ieee80211com *ic,
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
#endif
#if ATH_SLOW_ANT_DIV
    void                 (*ic_antenna_diversity_suspend)(struct ieee80211com *ic);
    void                 (*ic_antenna_diversity_resume)(struct ieee80211com *ic);
#endif

    u_int8_t                (*ic_get_ctl_by_country)(struct ieee80211com *ic, u_int8_t *country, bool is2G);

    int                     (*ic_reg_notify_tx_bcn)(struct ieee80211com *ic,
                                                    ieee80211_tx_bcn_notify_func callback,
                                                    void *arg);
    int                     (*ic_dereg_notify_tx_bcn)(struct ieee80211com *ic);

    int                     (*ic_vap_pause_control)(struct ieee80211com *ic, struct ieee80211vap *vap, bool pause); /* if vap is null, pause all vaps */
    void                    (*ic_enable_rifs_ldpcwar) (struct ieee80211_node *ni, bool value);
    void                    (*ic_process_uapsd_trigger) (struct ieee80211com *ic, struct ieee80211_node *ni, bool enforce_sp_limit, bool *sent_eosp);
    int                     (*ic_is_hwbeaconproc_active) (struct ieee80211com *ic);
    void                    (*ic_hw_beacon_rssi_threshold_enable)(struct ieee80211com *ic,
                                                u_int32_t rssi_threshold);
    void                    (*ic_hw_beacon_rssi_threshold_disable)(struct ieee80211com *ic);

#if UMAC_SUPPORT_VI_DBG
    struct ieee80211_vi_dbg   *ic_vi_dbg;
    struct ieee80211_vi_dbg_params  *ic_vi_dbg_params;
    void   (*ic_set_vi_dbg_restart)(struct ieee80211com *ic);
    void   (*ic_set_vi_dbg_log) (struct ieee80211com *ic, bool enable);
#endif
    void   (*ic_set_noise_detection_param) (struct ieee80211com *ic, int cmd, int val);
    void   (*ic_get_noise_detection_param) (struct ieee80211com *ic, int cmd, int *val);
    struct ieee80211_quiet_param *ic_quiet;

    void        (*ic_rx_intr_mitigation)(struct ieee80211com *ic, u_int32_t enable);
    void        (*ic_set_beacon_interval)(struct ieee80211com *ic);
    u_int32_t   (*ic_get_txbuf_free) (struct ieee80211com *ic);
    void        (*ic_register_beacon_tx_status_event_handler)
                (struct ieee80211com *ic, bool unregister);
    void        (*ic_config_bsscolor_offload)
                (struct ieee80211vap *vap, bool disable);

#ifdef ATH_HTC_MII_RXIN_TASKLET
    qdf_nbuf_queue_t      ic_mgmt_nbufqueue;
    os_mgmt_lock_t        ic_mgmt_lock;
    atomic_t              ic_mgmt_deferflags;

    TAILQ_HEAD(, ieee80211_node)  ic_nodeleave_queue;
    os_nodeleave_lock_t           ic_nodeleave_lock;
    atomic_t                      ic_nodeleave_deferflags;
    TAILQ_HEAD(, _nawds_dentry)       ic_nawdslearnlist;
    os_nawdsdefer_lock_t          ic_nawdsdefer_lock;
    atomic_t                      ic_nawds_deferflags;
#endif
#ifdef DBG
    u_int32_t               (*ic_hw_reg_read)(struct ieee80211com *ic, u_int32_t);
#endif
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
#define ATH_HMMC_CNT_MAX 8
    u_int32_t   ic_hmmc_cnt;
    struct {
        u_int32_t   ip;
        u_int32_t   mask;
    } ic_hmmcs[ATH_HMMC_CNT_MAX];
	int 		(*ic_node_ext_stats_enable)(struct ieee80211_node *ni, u_int32_t enable);
   	void        (*ic_reset_ald_stats)(struct ieee80211_node *ni);
   	void        (*ic_collect_stats)(struct ieee80211_node *ni);

#endif
    qdf_work_t  assoc_sm_set_country_code;
    struct assoc_sm_set_country *set_country;
#ifdef ATH_SUPPORT_DFS
    qdf_work_t  dfs_cac_timer_start_work;
    u_int32_t   ic_dfs_waitfor_csa_sched:1; /* Waiting for CSA from the uplink */
    os_timer_t  ic_dfs_waitfor_csa_timer;   /* Wait for CSA from parent Repeater/Root timer */
    os_timer_t  ic_dfs_tx_rcsa_and_nol_ie_timer; /* dfs RCSA(reverse CSA) and NOL IE timer */
#if ATH_SUPPORT_DFS && ATH_SUPPORT_STA_DFS
    void          *ic_dfs_flags;
#endif

    /*
     * Update the channel list with the current set of DFS
     * NOL entries.
     *
     * + 'cmd' indicates what to do; for now it should just
     *   be DFS_NOL_CLIST_CMD_UPDATE which will update all
     *   channels, given the _entire_ NOL. (Rather than
     *   the earlier behaviour with clist_update, which
     *   was to either add or remove a set of channel
     *   entries.)
     */
    void        (*ic_dfs_start_tx_rcsa_and_waitfor_rx_csa)(struct ieee80211com *ic);
    void        (*ic_dprintf_ic)(struct ieee80211com *ic, unsigned verbose,
                                 unsigned category, const char *fmt, ...);
#endif

    void        (*ic_get_ext_chan_info)(struct ieee80211com *ic,
                                        struct ieee80211_ath_channel *chan,
                                        struct ieee80211_ath_channel_list *chan_info);
    struct ieee80211_ath_channel *(*ic_find_channel)(struct ieee80211com *ic,
                                                     int freq,
                                                     uint8_t des_cfreq2,
                                                     uint64_t flags);

    u_int       (*ic_chan2freq)(struct ieee80211com *ic,
                                const struct ieee80211_ath_channel *c);
    int         (*ic_find_any_valid_channel)(struct ieee80211com *ic,
                                             uint32_t chan_mode);

    u_int64_t                           (*ic_get_tx_hw_retries)(struct ieee80211com *ic);
    u_int64_t                           (*ic_get_tx_hw_success)(struct ieee80211com *ic);
    void                                (*ic_rate_node_update)(struct ieee80211com *ic,
                                                               struct ieee80211_node *ni,
                                                               int isnew);

    /* isr dynamic aponly mode control */
    bool                                ic_aponly;

#if LMAC_SUPPORT_POWERSAVE_QUEUE
    u_int8_t                            (*ic_get_lmac_pwrsaveq_len)(struct ieee80211com *ic,
                                                                    struct ieee80211_node *ni, u_int8_t frame_type);
    int                                 (*ic_node_pwrsaveq_send)(struct ieee80211com *ic,
                                                              struct ieee80211_node *ni, u_int8_t frame_type);
    void                                (*ic_node_pwrsaveq_flush)(struct ieee80211com *ic, struct ieee80211_node *ni);
    int                                 (*ic_node_pwrsaveq_drain)(struct ieee80211com *ic, struct ieee80211_node *ni);
    int                                 (*ic_node_pwrsaveq_age)(struct ieee80211com *ic, struct ieee80211_node *ni);
    void                                (*ic_node_pwrsaveq_get_info)(struct ieee80211com *ic,
                                                                  struct ieee80211_node *ni, ieee80211_node_saveq_info *info);
    void                                (*ic_node_pwrsaveq_set_param)(struct ieee80211com *ic,
                                                                   struct ieee80211_node *ni, enum ieee80211_node_saveq_param param,
                                                                   u_int32_t val);
#endif
#if ATH_SUPPORT_FLOWMAC_MODULE
    int                                 (*ic_get_flowmac_enabled_State)(struct ieee80211com *ic);
#endif
    int                                 (*ic_get_rx_signal_dbm)(struct ieee80211com *ic, int8_t *signal_dbm);
    /* To store the id_mask of vaps which has downed by software,when actual physical radio interface is downed.*/
    u_int32_t                           id_mask_vap_downed[VDEV_BITMAP_SLOTS];
#if WLAN_SPECTRAL_ENABLE
    u_int32_t                           chan_clr_cnt;
    u_int32_t                           cycle_cnt;
    u_int32_t                           chan_num;
#endif
    ieee80211_resmgr_t                  (*ic_resmgr_create)(struct ieee80211com *ic, ieee80211_resmgr_mode mode);
    void                                (*ic_vdev_beacon_template_update)(struct ieee80211vap *vap);
    int                                 (*ic_beacon_offload_control)(struct ieee80211vap *vap, u_int32_t bcn_ctrl_op);
    QDF_STATUS                          (*ic_bcn_tmpl_send)(struct wlan_objmgr_vdev *);
    void                                (*ic_update_restart_param)(struct ieee80211vap *vap,
                                                                   bool reset,
                                                                   bool restart_success);
    int                                 (*ic_update_phy_mode)(struct mlme_channel_param *ch_param, struct ieee80211com *ic);
    int                                 (*ic_vap_set_param)(struct ieee80211vap *vap,
                                         ieee80211_param param, u_int32_t val);
    int                                 (*ic_vap_sifs_trigger)(struct ieee80211vap *vap,
                                         u_int32_t val);
    int                                 (*ic_vap_get_param)(struct ieee80211vap *vap,
                                         ieee80211_param param);
    int                                 (*ic_ol_net80211_set_mu_whtlist)(wlan_if_t vap,
                                         u_int8_t *macaddr, u_int16_t tidmask);
    int                                 (*ic_node_add_wds_entry)(void *vdev_handle,
                                         const u_int8_t *dest_mac, u_int8_t *peer_mac, u_int32_t flags);
    void                                (*ic_node_del_wds_entry)(void *vdev_handle,
                                         u_int8_t *dest_mac);
#ifdef AST_HKV1_WORKAROUND
    int                                 (*ic_node_lookup_wds_and_del)(void *vdev_handle,
                                         uint8_t *mac,
					 struct recv_auth_params_defer *auth_cookie);
#endif
    int                                 (*ic_vap_set_ratemask)(struct ieee80211vap *vap,
                                        u_int8_t preamble, u_int32_t mask_lower32,
                                        u_int32_t mask_higher32, u_int32_t mask_lower32_2);
    int16_t                             (*ic_vap_dyn_bw_rts)(struct ieee80211vap *vap, int param);
    int                                 (*ic_node_update_wds_entry)(void *vdev_handle,
                                         u_int8_t *wds_macaddr, u_int8_t *peer_macaddr, u_int32_t flags);
    void                                (*ic_node_set_bridge_mac_addr)(struct ieee80211com *ic,
                                         u_int8_t *bridge_mac);
    int                                 (*ic_node_use_4addr)(struct ieee80211_node *ni);

    int                                 (*ic_vap_set_qdepth_thresh)(struct ieee80211com *ic,
                                         struct ieee80211vap *vap, uint8_t *mac_addr, uint32_t tid,
                                         uint32_t update_mask, uint32_t thresh_val);
#if ATH_WOW
    /* Wake on Wireless used on clients to wake up the system with a magic packet */
    int                                 (*ic_wow_get_support)(struct ieee80211com *ic);
    int                                 (*ic_wow_enable)(struct ieee80211com *ic, int clearbssid);
    int                                 (*ic_wow_wakeup)(struct ieee80211com *ic);
    int                                 (*ic_wow_add_wakeup_event)(struct ieee80211com *ic, u_int32_t type);
    void                                (*ic_wow_set_events)(struct ieee80211com *ic, u_int32_t);
    int                                 (*ic_wow_get_events)(struct ieee80211com *ic);
    int                                 (*ic_wow_add_wakeup_pattern)
                                            (struct ieee80211com *ic, u_int8_t *, u_int8_t *, u_int32_t);
    int                                 (*ic_wow_remove_wakeup_pattern)(struct ieee80211com *ic, u_int8_t *, u_int8_t *);
    int                                 (*ic_wow_get_wakeup_pattern)
                                            (struct ieee80211com *ic, u_int8_t *,u_int32_t *, u_int32_t *);
    int                                 (*ic_wow_get_wakeup_reason)(struct ieee80211com *ic);
    int                                 (*ic_wow_matchpattern_exact)(struct ieee80211com *ic);
    void                                (*ic_wow_set_duration)(struct ieee80211com *ic, u_int16_t);
    void                                (*ic_wow_set_timeout)(struct ieee80211com *ic, u_int32_t);
	u_int32_t                           (*ic_wow_get_timeout)(struct ieee80211com *ic);
#endif /* ATH_WOW */
    void                                (*ic_tx_flush)(struct ieee80211com *ic);
    int                                 (*ic_set_ru26_tolerant)(struct ieee80211com *ic, bool value);

    u_int32_t                            ic_vhtcap;              /* VHT capabilities */
    ieee80211_vht_mcs_set_t              ic_vhtcap_max_mcs;      /* VHT Supported MCS set */
    u_int16_t                            ic_vhtop_basic_mcs;     /* VHT Basic MCS set */
    u_int8_t                             ic_vht_ampdu;           /* VHT AMPDU value */
    u_int8_t                             ic_vht_amsdu;           /* VHT AMSDU value */
    u_int8_t                             ic_no_bfee_limit;       /* no MU BFEE limit */
    void                                (*ic_power_attach)(struct ieee80211com *ic);
    void                                (*ic_power_detach)(struct ieee80211com *ic);
    void                                (*ic_power_vattach)(struct ieee80211vap *vap, int fullsleepEnable,
                                            u_int32_t sleepTimerPwrSaveMax, u_int32_t sleepTimerPwrSave, u_int32_t sleepTimePerf,
                                            u_int32_t inactTimerPwrsaveMax, u_int32_t inactTimerPwrsave, u_int32_t inactTimerPerf,
                                            u_int32_t smpsDynamic, u_int32_t pspollEnabled);
    void                                (*ic_power_vdetach)(struct ieee80211vap *vap);
    int                                 (*ic_power_sta_set_pspoll)(struct ieee80211vap *vap, u_int32_t pspoll);
    int                                 (*ic_power_sta_set_pspoll_moredata_handling)(struct ieee80211vap *vap, ieee80211_pspoll_moredata_handling mode);
    u_int32_t                           (*ic_power_sta_get_pspoll)(struct ieee80211vap *vap);
    ieee80211_pspoll_moredata_handling  (*ic_power_sta_get_pspoll_moredata_handling)(struct ieee80211vap *vap);
    int                                 (*ic_power_set_mode)(struct ieee80211vap* vap, ieee80211_pwrsave_mode mode);
    ieee80211_pwrsave_mode              (*ic_power_get_mode)(struct ieee80211vap* vap);
    u_int32_t                           (*ic_power_get_apps_state)(struct ieee80211vap* vap);
    int                                 (*ic_power_set_inactive_time)(struct ieee80211vap* vap, ieee80211_pwrsave_mode mode, u_int32_t inactive_time);
    u_int32_t                           (*ic_power_get_inactive_time)(struct ieee80211vap* vap, ieee80211_pwrsave_mode mode);
    int                                 (*ic_power_force_sleep)(struct ieee80211vap* vap, bool enable);
    int                                 (*ic_power_set_ips_pause_notif_timeout)(struct ieee80211vap* vap, u_int16_t pause_notif_timeout);
    u_int16_t                           (*ic_power_get_ips_pause_notif_timeout)(struct ieee80211vap* vap);
    int                                 (*ic_power_sta_send_keepalive)(struct ieee80211vap *vap);
    int                                 (*ic_power_sta_pause)(struct ieee80211vap *vap, u_int32_t timeout);
    int                                 (*ic_power_sta_unpause)(struct ieee80211vap *vap);
    void                                (*ic_power_sta_event_dtim)(struct ieee80211vap *vap);
    void                                (*ic_power_sta_event_tim)(struct ieee80211vap *vap);
    int                                 (*ic_power_sta_unregister_event_handler)(struct ieee80211vap *vap, ieee80211_sta_power_event_handler evhandler, void *arg);
    int                                 (*ic_power_sta_register_event_handler)(struct ieee80211vap *vap, ieee80211_sta_power_event_handler evhandler, void *arg);
    void                                (*ic_power_sta_tx_start)(struct ieee80211vap *vap);
    void                                (*ic_power_sta_tx_end)(struct ieee80211vap *vap);
     /* ACS APIs for offload architecture */
    void                                (*ic_hal_get_chan_info)(struct ieee80211com *ic, u_int8_t flags);

    void                                (*ic_mcast_group_update)(struct ieee80211com *ic, int action, int wildcard, u_int8_t *mcast_ip_addr, int mcast_ip_addr_bytes, u_int8_t *ucast_mac_addr,  u_int8_t filter_mode,  u_int8_t nsrcs,  u_int8_t *srcs, u_int8_t *mask, u_int8_t vap_id);
    u_int32_t                           (*ic_num_mcast_tbl_elements)(struct ieee80211com *ic);
    u_int32_t                           (*ic_num_mcast_grps)(struct ieee80211com *ic);
    u_int16_t (*ic_me_convert)(struct ieee80211vap *vap, wbuf_t wbuf,
                               u_int8_t newmac[][IEEE80211_ADDR_LEN], uint8_t newmaccnt);
#if ATH_SUPPORT_KEYPLUMB_WAR
		int 								(*ic_checkandplumb_key)(struct ieee80211vap *vap, struct ieee80211_node *ni);
#endif
#if ATH_SUPPORT_TIDSTUCK_WAR
		void					(*ic_clear_rxtid)(struct ieee80211com *ic, struct ieee80211_node *ni);
#endif
	void                                (*ic_hifitbl_update_target)(struct ieee80211vap *vap);
    void                                (*ic_check_buffull_condition)(struct ieee80211com *ic);
#if ATH_TxBF_DYNAMIC_LOF_ON_N_CHAIN_MASK
    void                                (*ic_txbf_loforceon_update)(struct ieee80211com *ic, bool loforcestate);
#endif
    void                                (*ic_set_ctl_table)(struct ieee80211com *ic, u_int8_t *ctl_array, u_int16_t ctl_len);
    void                                (*ic_set_mimogain_table)(struct ieee80211com *ic, u_int8_t *array_gain,
                                                                 u_int16_t tbl_len, bool multichain_gain_bypass);
    void                                (*ic_ratepwr_table_ops)(struct ieee80211com *ic, u_int8_t ops,
                                                               u_int8_t *ratepwr_tbl, u_int16_t ratepwr_len);
    void                                (*ic_set_ratepwr_chainmsk)(struct ieee80211com *ic, u_int32_t *ratepwr_chain_tbl,
                                                                u_int16_t num_rate, u_int8_t pream_type, u_int8_t ops);
    void                                (*ic_set_node_tpc)(struct ieee80211com *ic, struct ieee80211_node *ni, u_int8_t tpc);
    void                                (*ic_set_sta_fixed_rate)(struct ieee80211_node *ni);
    void                                (*ic_scan_enable_txq)(struct ieee80211com *ic);

    bool (*ic_support_phy_mode)(struct ieee80211com *ic, enum ieee80211_phymode);
    int  (*ic_get_bw_nss_mapping)(struct ieee80211vap *vap,
            struct ieee80211_bwnss_map *nssmap, u_int8_t chainmask);
    bool (*ic_rate_check)(struct ieee80211com *ic, int val);
#if QCA_PARTNER_PLATFORM
    struct partner_com_param            partner_com_params;
#endif
    int (*ic_fips_test)(struct ieee80211com *ic, struct ath_fips_cmd *fips_buf);
#ifdef WLAN_SUPPORT_TWT
    int (*ic_twt_req)(wlan_if_t vap, struct ieee80211req_athdbg *req);
#endif

    /* Set/get management frame retry limit */
    int                                 (*ic_set_mgmt_retry_limit)(struct ieee80211com *ic, u_int8_t);
    u_int8_t                            (*ic_get_mgmt_retry_limit)(struct ieee80211com *ic);
    u_int32_t                           obss_rssi_threshold; /* OBSS RSSI threshold */
    u_int32_t                           obss_rx_rssi_threshold; /* OBSS RX RSSI threshold */
    void                                (*ic_node_pspoll)(struct ieee80211_node *ni, bool value);
    int                                 (*ic_tr69_request_process)(struct ieee80211vap *vap, int cmdid, void * arg1, void *arg2);
    bool                                (*ic_is_target_ar900b)(struct ieee80211com *ic);
    int32_t                             (*ic_get_pdev_idx)(struct ieee80211com *ic);
    uint32_t                             (*ic_get_tgt_type)(struct ieee80211com *ic);
#if UMAC_SUPPORT_ACFG
    void                                *ic_acfg_handle;
#endif
    u_int8_t                            ic_tso_support;
    u_int8_t                            ic_lro_support;
    u_int8_t                            ic_sg_support;
    u_int8_t                            ic_gro_support;
    u_int8_t                            ic_offload_tx_csum_support;
    u_int8_t                            ic_offload_rx_csum_support;
    u_int8_t                            ic_rawmode_support;
    u_int8_t                            ic_dynamic_grouping_support;
    u_int8_t                            ic_dpd_support;
    u_int8_t                            ic_aggr_burst_support;
    u_int8_t                            ic_qboost_support;
    u_int8_t                            ic_sifs_frame_support;
    u_int8_t                            ic_block_interbss_support;
    u_int8_t                            ic_disable_reset_support;
    u_int8_t                            ic_msdu_ttl_support;
    u_int8_t                            ic_ppdu_duration_support;
    u_int8_t                            ic_promisc_support;
    u_int8_t                            ic_burst_mode_support;
    u_int8_t                            ic_peer_flow_control_support;
    u_int8_t                            ic_mesh_vap_support;
    u_int8_t                            ic_wds_support;

    /* Number of hops away from the root AP on this radio. This is used for
     * populating the WHC AP Info IE to help devices that could potentially
     * act as a range extender decide whether they are close enough to the
     * CAP to do so or not.
     */
    uint8_t                             dfs_spoof_test_regdmn;
    u_int8_t                            ic_disable_bcn_bwnss_map;       /* variable to store bandwidth-NSS mapping status*/
    u_int8_t                            ic_disable_bwnss_adv;           /* variable to enable/disable bwnss map advertising in management frames*/
    /* Monitor VAP filters */
    u_int8_t                            mon_filter_osif_mac:1,
                                        mon_filter_mcast_data:1,
                                        mon_filter_ucast_data:1,
                                        mon_filter_non_data:1;
    u_int8_t                            ic_cal_ver_check:1;
    struct global_ic_list               *ic_global_list;     /* back ptr to global_ic_list */
#if DBDC_REPEATER_SUPPORT
    u_int8_t                            ic_radio_priority;
    bool                                ic_primary_radio;
    bool                                ic_preferredUplink;
    /* Store ic pointers from other radios in this list. Current ic is not
     * stored in this list.
     */
    struct ieee80211com                 *other_ic[MAX_RADIO_CNT-1];    /* this list has all other radio structure pointers*/
    bool                                fast_lane;
    struct ieee80211com                 *fast_lane_ic;    /* radio structure pointer for other fastlane radio*/
    u_int8_t                            ic_extender_connection;  /* 0->init value
                                                                    1->connecting RE has no RootAP access
                                                                    2->connecting RE has RootAP access */
#endif
    struct ieee80211_nl_handle          *ic_nl_handle;
    spinlock_t                          ic_cip_lock; /* lock to protect crypto module access */
    struct restrict_channel_params      ic_rch_params; /*restrict channels parammeters */
    u_int32_t                           ic_num_clients;
    u_int32_t                           ic_def_num_clients; /* Setting default num clients based on target type*/
    u_int32_t                           ic_nac_client;
    u_int32_t                           ic_nac_bssid;
    u_int8_t                            ic_emiwar_80p80; /* EMI war to restrict cfreq1: 5775, cfreq2: 5210 combination in VHT80+80 */
#if ATH_GEN_RANDOMNESS
    u_int32_t                           random_gen_mode;
#endif
#if UMAC_SUPPORT_ACFG
    u_int8_t                            ic_diag_enable;
    spinlock_t                          ic_diag_lock;
    void                                *ic_diag_handle;
#endif
    /* Percentage obss channel utilization  thresold above
     * which chan stats events will be sent to user space.
     */
    u_int8_t                            ic_chan_stats_th;
    int                                 ic_min_rssi;
#if MESH_MODE_SUPPORT
    u_int32_t                           meshpeer_timeout_cnt;
/* every IEEE80211_MESH_PEER_TIMEOUT_CNT*IEEE80211_SESSION_WAIT sec, call mesh peer timeout check func */
#define     IEEE80211_MESH_PEER_TIMEOUT_CNT  3
#endif
    u_int8_t                            tid_override_queue_mapping;
    u_int8_t                            bin_number; /* index to the bins */
    u_int8_t                            traf_bins;  /* number of bins w.r.t rate and interval */
    u_int32_t                           traf_rate;  /* The rate at which the received signal statistics are to be measured */
    u_int32_t                           traf_interval; /* The interval upto which the received signal statistics are to be measured */
    u_int8_t                            traf_stats_enable; /* The configuration parameter to enable/disable the statistics measurement */
    os_timer_t                          ic_noise_stats;  /* Timer to measure the noise statistics of each node */
    u_int8_t                            no_chans_available;
    u_int8_t                            ic_otherband_uplink_bssid[IEEE80211_ADDR_LEN]; /* backhaul otherband bssid in RE mode */
    u_int8_t                            ic_otherband_bssid[IEEE80211_ADDR_LEN]; /* Otherband bssid serving the same SSID */
    u_int8_t                            ic_mon_decoder_type;    /* monitor header type, prism=1, radiotap=0 */
    u_int8_t                            ic_strict_doth;/* Strict doth enabled */
    /* Number of non doth supporting stations associated
     *  on this radio interface.
     */
    u_int16_t                           ic_non_doth_sta_cnt;
    u_int8_t                            ic_chan_switch_cnt; /* Number of CSA/ECSA beacons to be transmitted before channel switch */
    u_int8_t                            ic_sec_offsetie; /* Enable Secondary offset IE in CSA beacons */
    u_int8_t                            ic_wb_subelem; /* Enable wide band channel switch subelement in CSA beacons */
#define MON_DECODER_RADIOTAP  0
#define MON_DECODER_PRISM     1
    int (*ic_whal_ratecode_to_kbps)(uint8_t ratecode, uint8_t bandwidth, uint8_t gintval);
#if ALL_POSSIBLE_RATES_SUPPORTED
    int (*ic_get_supported_rates)(int htflag, int shortgi, int **rates);
    int (*ic_whal_kbps_to_mcs)(int kbps_rate, int shortgi, int htflag);
#else
    int (*ic_get_supported_rates)(int htflag, int shortgi, int nss, int ch_width, int **rates);
    int (*ic_whal_kbps_to_mcs)(int kbps_rate, int shortgi, int htflag, int nss, int ch_width);
#endif

    int (*ic_ucfg_testmode_cmd)(void *data, int cmd, char *userdata, unsigned int length); /* Handles UTF commands from FTM daemon */
#if UMAC_SUPPORT_CFG80211
    void (*ic_ucfg_testmode_resp)(struct wiphy *wiphy, void *buf, size_t buf_len); /* Handles sending responses of UTF commands to cfg80211 */
#endif

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    struct nss_vdev_ops *nss_vops;
    struct nss_wifi_internal_ops *nss_iops;
    struct nss_wifi_radio_ops *nss_radio_ops;
#endif
    u_int32_t                           ic_auth_tx_xretry;
    struct wlan_objmgr_pdev *ic_pdev_obj;
    /* cfg80211 device attach for radio */
#if UMAC_SUPPORT_CFG80211
    u_int32_t                         ic_cfg80211_config; /* cfg80211 configuration */
    int (*ic_cfg80211_radio_attach) (struct device *dev, struct net_device *net_dev, struct ieee80211com *ic);
    /* cfg80211 device detach for radio */
    int (*ic_cfg80211_radio_detach) (struct ieee80211com *ic);
    int (*ic_cfg80211_update_channel_list) (struct ieee80211com *ic);

    struct wiphy                       *ic_wiphy; /* cfg80211 device */
    struct wireless_dev                ic_wdev;  /* wiress dev */
    struct ieee80211_channel           *channel_list_g;
    struct ieee80211_channel           *channel_list_a;
    struct radio_handler               ic_cfg80211_radio_handler;  /* cfg80211 radio handlers */
    struct spectral_handler            ic_cfg80211_spectral_handler;  /* cfg80211 spectral handlers */
    struct ieee80211_supported_band wlan_band_2g; /* 2g band information that required for wiphy */
    struct ieee80211_supported_band wlan_band_5g; /* 5g band information that required for wiphy */
#endif
#if QCN_IE
    u_int8_t                            ic_bpr_latency_comp; /* Stores time in ms to compensate for the
                                                               latency in sending a probe response */
    u_int8_t                            ic_bcn_latency_comp; /* Stores time in ms to compensate for the
                                                               latency in sending beacon */
    qdf_atomic_t                        ic_bpr_enable;       /* Store bpr enable state  */
#endif
#if QCA_SUPPORT_GPR
    struct acfg_offchan_tx_hdr          *acfg_frame;
    qdf_ktime_t                         ic_gpr_period;            /* Stores the gpr period in kernel time */
    qdf_hrtimer_data_t                  ic_gpr_timer;             /* hrtimer to send gpr  */
    u_int16_t                           ic_gpr_timer_start_count; /* Stores the number of times hrtimer restarted */
    u_int16_t                           ic_gpr_timer_resize_count; /* Stores the number of times hrtimer got resized */
    u_int16_t                           ic_gpr_send_count;        /* Stores the number of times gpr is sent */
    u_int8_t                            ic_gpr_enable_count;     /* number of vaps running gpr */
    u_int8_t                            ic_gpr_enable;           /* gpr status */
    u_int32_t                           ic_period;
#endif
    /* IC 11ax HE handle */
    struct ieee80211_he_handle          ic_he;
    /* HE target type */
    uint8_t                             ic_he_target;
    /* SELF AP OBSS_PD_THRESHOLD value used to configure OBSS Packet Detect
     * threshold for Spatial Reuse feature.
     */
    uint32_t                            ic_ap_obss_pd_thresh;
    /* Default BA Buf Size for HE target */
    uint16_t                            ic_he_default_ba_buf_size;
    /* HE bsscolor handle representing all VAPs */
    struct ieee80211_bsscolor_handle    ic_bsscolor_hdl;
    /* User configured He Bss Color value */
    uint8_t                             ic_he_bsscolor;
    /* HE bsscolor change tbtt. */
    uint16_t                            ic_he_bsscolor_change_tbtt;
    uint32_t                            ic_he_ul_trig_int:16, /* Trigger interval setting */
                                        ic_he_ul_ppdu_dur:13; /* UL PPDU Duration */
    uint8_t                             ic_he_bsscolor_override:1, /* Flag to enable/disable overriding bss color by user */
                                        ic_he_sr_enable:1, /* Spatial Reuse Setting */
                                        ic_he_ru_allocation:1; /* Flag to control RU allocation. */
    uint8_t                             ic_rx_mon_lite; /* Rx-mon lite */
    bool                                ic_tx_pkt_capture; /* Tx-Packetl Capture */
    /* SR Ctrl SRG OBSS PD Max/Min Offset */
    uint8_t                             ic_he_srctrl_srg_obsspd_max_offset;
    uint8_t                             ic_he_srctrl_srg_obsspd_min_offset;
    /* SR Ctrl SRP Disallowed config */
    uint8_t                             ic_he_srctrl_srp_disallowed:1,
    /* SR Ctrl Non-SRG OBSS PD SR Disallowed config */
                                        ic_he_srctrl_non_srg_obsspd_disallowed:1,
    /* SR Ctrl Non-SRG Offset Present */
                                        ic_he_srctrl_non_srg_offset_present:1,
    /* SR Ctrl SRG Information Present */
                                        ic_he_srctrl_srg_info_present:1,
    /* SR Ctrl HESIGA_Spatial_reuse_value15_allowed config */
                                        ic_he_srctrl_sr15_allowed:1;
    /* SRG supported */
    bool                                ic_he_srg_obss_pd_supported;
    /* Non-SRG OOBSS PD max offset */
    uint8_t                             ic_he_non_srg_obsspd_max_offset;
    struct ieee80211_profile            *profile; /* Radio and VAP profile of the driver */
    u_int8_t                            recovery_in_progress;
    struct ieee80211_ath_channel        *recovery_dfschan;  /* DFS channel during recovery  */
    u_int8_t                            mgmt_rx_rssi;
    qdf_event_t                         ic_wait_for_init_cc_response;
    bool                                ic_set_country_failed;
#ifdef CONFIG_DP_TRACE
    int (*ic_dptrace_set_param)(int cmd, int val1, int val2);
#endif
    struct proc_dir_entry                         *ic_proc;
    bool                                 ic_fw_ext_nss_capable; /* FW capability for EXT NSS Signaling */
    bool                                ic_ext_nss_capable; /* EXT NSS Capable */
    uint8_t                             ic_widebw_scan; /* wide band scan config */
    uint8_t                             ic_capture_latency; /* Store capture enable state  */
#define QOS_ACTION_FRAME_FRWD_TO_STACK 0x1
#define QOS_ACTION_FRAME_HNDL_UP_STACK 0x2
#define QOS_ACTION_FRAME_MASK (QOS_ACTION_FRAME_HNDL_UP_STACK | QOS_ACTION_FRAME_FRWD_TO_STACK)
    uint32_t                            ic_qos_acfrm_config; /* qos action frames config */
    void (*ic_print_peer_refs)(struct ieee80211vap *vap, struct ieee80211com *ic, bool assert);
    uint8_t                             ic_dfs_info_notify_channel_available; /* Enable DFS info notify to app */
    uint8_t                             ic_dfs_info_notify_radar_found;       /* Enable DFS info notify to app */
    bool (*wide_band_scan_enabled)(struct ieee80211com *ic);
    uint8_t                                ic_debug_sniffer;
    void (*ic_process_nondata_tx_frames)(struct ieee80211com *ic, qdf_nbuf_t payload);
    uint8_t                             ic_num_csa_bcn_tmp_sent; /* Number of beacon template sent to the FW with CSA IE. */
    uint32_t                            ic_csa_vdev_ids[WLAN_UMAC_PDEV_MAX_VDEVS]; /* Save vdev_id of the vap when it receives CSA event from FW. */
    uint32_t                            ic_csa_num_vdevs; /* Total number of vdevs which receives CSA event from FW. */
    int                                 (*ic_node_dump_wds_table) (struct ieee80211com *ic);
#if ATH_ACS_DEBUG_SUPPORT
    bool                                ic_acs_debug_support;  /* Manages the functioning of the ACS debug framework */
#endif
    void *dops;
    int (*ic_node_enable_sounding_int)(struct ieee80211_node *ni, uint32_t enable);
    int (*ic_node_set_su_sounding_int)(struct ieee80211_node *ni, uint32_t sounding_interval);
    int (*ic_node_set_mu_sounding_int)(struct ieee80211_node *ni, uint32_t sounding_interval);
    int (*ic_node_sched_mu_enable)(struct ieee80211_node *ni, uint32_t enable);
    int (*ic_node_sched_ofdma_enable)(struct ieee80211_node *ni, uint32_t enable);
    int (*ic_get_target_phymome)(struct ieee80211com *ic, uint32_t host_phymode,
                                 bool is_2gvht_en);
    ieee80211_rep_move_t                ic_repeater_move;
    /* Enable/disable CCK Tx, applies only to 2G */
    bool                                cck_tx_enable;
    bool                                schedule_bringup_vaps;
    struct last_scan_params scan_params;
    uint32_t                       multiq_support_enabled;
    u_int32_t                           device_id;   /* Device id from probe */
    u_int32_t                           vendor_id;   /* Vendor id from probe */
#if defined(WLAN_DFS_PARTIAL_OFFLOAD) && defined(HOST_DFS_SPOOF_TEST)
    uint8_t                             ic_rebuilt_chanlist:1,
                                        ic_tempchan:1;
    u_int32_t                           ic_curchan_flags;
    uint8_t                             ic_tmp_ch_width;
    uint8_t                             ic_tmp_center_freq_seg0;
    uint8_t                             ic_tmp_center_freq_seg1;
    uint8_t                             ic_tmp_sec_ch_offset;
    QDF_STATUS (*ic_is_host_dfs_check_enabled) (struct wlan_objmgr_pdev *pdev,
            bool *enabled);
#endif
    int                            ic_fw_rec_dbg[QDF_MODULE_ID_MAX];
    u_int8_t                       ic_tx_capture;  /* is tx capture enabled */
#if QCN_ESP_IE
    u_int8_t                            ic_esp_air_time_fraction;
    u_int8_t                            ic_esp_ppdu_duration;
    u_int8_t                            ic_esp_ba_window;
    u_int16_t                           ic_esp_periodicity;
    u_int32_t                           ic_fw_esp_air_time;
    bool                                ic_esp_flag;
#endif /* QCN_ESP_IE */
#if DBDC_REPEATER_SUPPORT
    uint8_t                  ic_preferred_bssid[IEEE80211_ADDR_LEN];
#endif

    /* MBSS IE */
    struct mbss_info         ic_mbss;

    bool ru26_tolerant; /* RU26 tolerance status */
    u_int8_t                 ic_hw_nac_monitor_support;
    /* ppdu and burst minimum value requirement for Lithium based targets */
    u_int8_t                 ic_ppdu_min;
    u_int8_t                 ic_burst_min;
    int (*ic_node_chan_width_switch)    (void *data, struct ieee80211vap *vap);
#if ATH_SUPPORT_DFS && QCA_DFS_NOL_VAP_RESTART
    os_timer_t  ic_dfs_nochan_vap_restart_timer;   /* vap restart timer */
    bool ic_pause_stavap_scan;
#endif
    u_int8_t                            ic_unified_stats;
#if defined(WLAN_DFS_FULL_OFFLOAD) && defined(QCA_DFS_NOL_OFFLOAD)
    bool ic_is_dfs_scan_violation; /* Status of dfs scan violation */
#endif
    void (*ic_find_next_hop_mac)(struct ieee80211vap *vap);
#ifdef OL_ATH_SMART_LOGGING
    bool                                smart_logging;
    void                                *smart_log_file;
    u_int32_t                           smart_log_skb_sz;
    bool                                smart_logging_p1pingfail_started;
    bool                                smart_logging_connection_fail_started;
#ifndef REMOVE_PKT_LOG
    qdf_mutex_t                         smart_log_fw_pktlog_mutex;
    bool                                smart_log_fw_pktlog_enabled;
    bool                                smart_log_fw_pktlog_blocked;
    bool                                smart_log_fw_pktlog_started;
#endif /* REMOVE_PKT_LOG */
#endif /* OL_ATH_SMART_LOGGING */
    struct wlan_channel_stats           ic_channel_stats;
    int                                 ic_uniform_rssi;
    qdf_timer_t                         ic_csa_max_rx_wait_timer;
    uint32_t                            ic_pcp_tid_map[IEEE80211_PCP_MAX]; /* 8021p pcp to tid map */
    uint8_t                             ic_tidmap_prty; /* tidmap priority */
    int                                 (*ic_set_pcp_tid_map)(struct ieee80211vap *vap,
                                                              uint32_t pcp, uint32_t tid);
    int                                 (*ic_set_tidmap_tbl_id)(struct ieee80211vap *vap,
                                                                uint32_t tblid);
    int                                 (*ic_set_tidmap_prty)(struct ieee80211vap *vap,
                                                              uint32_t val);
#if UMAC_SUPPORT_ACFG
    int                                 (*ic_acfg_cmd)(struct net_device *dev, void *data);
#endif
    struct ieee80211_ath_channel        *ic_radar_defer_chan;
    ieee80211_user_chanlist_t           ic_pri20_cfg_blockchanlist;
#ifdef WLAN_SUPPORT_RX_PROTOCOL_TYPE_TAG
    uint32_t                            rx_pkt_protocol_tag_mask;
#endif //WLAN_SUPPORT_RX_PKT_TAGGING
    u_int32_t                           ic_offchan_dwell_time;
} IEEE80211COM, *PIEEE80211COM;

struct assoc_sm_set_country{
    uint16_t cc;
    enum ieee80211_clist_cmd cmd;
    struct ieee80211_node *ni;
    struct ieee80211vap *vap;
};

typedef  int  (* ieee80211_vap_input_mgmt_filter ) (struct ieee80211_node *ni, wbuf_t wbuf,
                                                     int subtype, struct ieee80211_rx_status *rs);
typedef  int  (* ieee80211_vap_output_mgmt_filter ) (wbuf_t wbuf);

/* Structure in the node to store the received signal value,
 * minimum value, maximum value and median value for each
 * node at the end of every traffic rate mentioned
 */
struct noise_stats {
    u_int8_t    noise_value;
    u_int8_t    min_value;
    u_int8_t    max_value;
    u_int8_t    median_value;
};

#define ATH_MAX_SCAN_ENTRIES 256

#if ATH_WOW
struct ieee80211_wowpattern {
    u_int8_t wp_delayed:1,
             wp_set:1,
             wp_valid:1;
    u_int8_t wp_type;
#define IEEE80211_T_WOW_DEAUTH             1  /* Deauth Pattern */
#define IEEE80211_T_WOW_DISASSOC           2  /* Disassoc Pattern */
#define IEEE80211_T_WOW_MINPATTERN         IEEE80211_T_WOW_DEAUTH
#define IEEE80211_T_WOW_MAXPATTERN         8
};
#endif


typedef struct _APP_IE_LIST_HEAD {
    struct app_ie_entry     *stqh_first;    /* first element */
    struct app_ie_entry     **stqh_last;    /* addr of last next element */        \
    u_int16_t               total_ie_len;
    bool                    changed;      /* indicates that the IE contents have changed */
} APP_IE_LIST_HEAD;

#define NAWDS_LOCK_INIT(_p_lock)                   OS_RWLOCK_INIT(_p_lock)
#define NAWDS_WRITE_LOCK(_p_lock , _flags)         OS_RWLOCK_WRITE_LOCK(_p_lock , _flags)
#define NAWDS_WRITE_UNLOCK(_p_lock , _flags)       OS_RWLOCK_WRITE_UNLOCK(_p_lock , _flags)
#define nawds_rwlock_state_t(_lock_state)          rwlock_state_t (_lock_stat)
typedef rwlock_t nawdslock_t;



#if UMAC_SUPPORT_NAWDS

#ifndef UMAC_MAX_NAWDS_REPEATER
#error NAWDS feature is enabled but UMAC_MAX_NAWDS_REPEATER is not defined
#endif

struct ieee80211_nawds_repeater {
    /* Bits 0 - 7 NSS */
#define NAWDS_REPEATER_CAP_DS              0x01
#define NAWDS_REPEATER_CAP_TS              0x02
#define NAWDS_REPEATER_CAP_4S              0x04
#define NAWDS_REPEATER_CAP_5S              0x08
#define NAWDS_REPEATER_CAP_6S              0x10
#define NAWDS_REPEATER_CAP_7S              0x20
#define NAWDS_REPEATER_CAP_8S              0x40
#define NAWDS_REPEATER_CAP_NSS_RSVD        0x80

    /* Bits 8 - 15 CHWIDHT/HTMODE */
#define NAWDS_REPEATER_CAP_HT20            0x0100
#define NAWDS_REPEATER_CAP_HT2040          0x0200
    /* VHT Capability */
#define NAWDS_REPEATER_CAP_11ACVHT20       0x0400
#define NAWDS_REPEATER_CAP_11ACVHT40       0x0800
#define NAWDS_REPEATER_CAP_11ACVHT80       0x1000
#define NAWDS_REPEATER_CAP_11ACVHT80_80    0x2000
#define NAWDS_REPEATER_CAP_11ACVHT160      0x4000
#define NAWDS_REPEATER_CAP_CHWD_RSVD       0x8000

    /* Bits 16 - 23 TX BF */
#ifdef ATH_SUPPORT_TxBF
#define NAWDS_REPEATER_CAP_TXBF            0x010000
#define NAWDS_REPEATER_CAP_TXBF_RSVD       0xFE0000
#else
#define NAWDS_REPEATER_CAP_TXBF_RSVD       0xFF0000
#endif

    /* Bits 24 - 31 HE Capability */
#define NAWDS_REPEATER_CAP_11AXAHE20       0x01000000
#define NAWDS_REPEATER_CAP_11AXGHE20       0x02000000
#define NAWDS_REPEATER_CAP_11AXAHE40       0x04000000
#define NAWDS_REPEATER_CAP_11AXGHE40       0x08000000
#define NAWDS_REPEATER_CAP_11AXAHE80       0x10000000
#define NAWDS_REPEATER_CAP_11AXAHE160      0x20000000
#define NAWDS_REPEATER_CAP_11AXAHE80_80    0x40000000

#define NAWDS_REPEATER_CAP_RSVD            0x80000000

#define NAWDS_INVALID_CAP_MODE             (NAWDS_REPEATER_CAP_NSS_RSVD   | \
                                            NAWDS_REPEATER_CAP_CHWD_RSVD  | \
                                            NAWDS_REPEATER_CAP_TXBF_RSVD  | \
                                            NAWDS_REPEATER_CAP_RSVD)

#define NAWDS_MULTI_STREAMS                (NAWDS_REPEATER_CAP_DS | \
                                            NAWDS_REPEATER_CAP_TS | \
                                            NAWDS_REPEATER_CAP_4S | \
                                            NAWDS_REPEATER_CAP_5S | \
                                            NAWDS_REPEATER_CAP_6S | \
                                            NAWDS_REPEATER_CAP_7S | \
                                            NAWDS_REPEATER_CAP_8S)

#define NAWDS_5TO8_STREAMS                 (NAWDS_REPEATER_CAP_5S | \
                                            NAWDS_REPEATER_CAP_6S | \
                                            NAWDS_REPEATER_CAP_7S | \
                                            NAWDS_REPEATER_CAP_8S)

    u_int32_t caps;
    u_int8_t mac[IEEE80211_ADDR_LEN];
};

enum ieee80211_nawds_mode {
    IEEE80211_NAWDS_DISABLED = 0,
    IEEE80211_NAWDS_STATIC_REPEATER,
    IEEE80211_NAWDS_STATIC_BRIDGE,
    IEEE80211_NAWDS_LEARNING_REPEATER,
    IEEE80211_NAWDS_LEARNING_BRIDGE,
};

struct ieee80211_nawds {
    nawdslock_t lock;
    enum ieee80211_nawds_mode mode;
    u_int32_t defcaps;
    u_int8_t override;
    struct ieee80211_nawds_repeater repeater[UMAC_MAX_NAWDS_REPEATER];
    u_int8_t psk[32];
};
#endif

#if ATH_SUPPORT_NAC

struct ieee80211_nac_info {
    u_int8_t macaddr[IEEE80211_ADDR_LEN];
    u_int8_t rssi;
};

struct ieee80211_nac {
    struct ieee80211_nac_info bssid[NAC_MAX_BSSID];
    struct ieee80211_nac_info client[NAC_MAX_CLIENT];
};
#endif

#if ATH_SUPPORT_NAC_RSSI
struct ieee80211_nac_rssi {
  u_int8_t bssid_mac[6];
  u_int8_t client_mac[6];
  u_int8_t  chan_num;
  u_int8_t client_rssi_valid;
  u_int8_t client_rssi;
  u_int8_t vdev_id;
};
#endif

struct ieee80211_tim_set {
	int set;
	u_int16_t aid;
};

enum ieee80211_offchan_request_type {
    IEEE80211_OFFCHAN_TX = 0,
    IEEE80211_OFFCHAN_RX = 1,
};

struct ieee80211_offchan_req {
    u_int32_t freq;
    u_int32_t dwell_time;
    u_int64_t cookie;
    enum ieee80211_offchan_request_type request_type;
};

/* List to store the mgmt offchan request */
struct ieee80211_offchan_list {
    qdf_list_node_t  next_request;
    qdf_nbuf_t offchan_tx_frm;
    struct ieee80211_offchan_req req;
};

/* Used for MBSS IE */
struct ieee80211_mbss {
    /* identifies the MBSS set this vap belongs to */
    uint8_t mbss_set_id;
    /* flag to indicate to add/delete vap */
    uint8_t mbssid_add_del_profile;
    /* flag to indicate probe response can be sent out */
    uint8_t mbssid_send_bcast_probe_resp;
    /* flag to indicate transmitting or non-transmitting VAP to FW */
    uint32_t mbssid_flags;
    /* flag to indicate if VAP profile exists in IE */
    qdf_atomic_t iv_added_to_mbss_ie;
};

#define MON_MAC_HASHSIZE 32

#define MON_MAC_HASH(addr)  \
    (((const u_int8_t *)(addr))[IEEE80211_ADDR_LEN - 1] % MON_MAC_HASHSIZE)

struct ieee80211_mac_filter_list {
    LIST_ENTRY(ieee80211_mac_filter_list)   mac_entry;  /* link entry in mac_entry list*/
    u_int8_t mac_addr[IEEE80211_ADDR_LEN];
};

typedef enum {
    /*
     * Timer action to kick out the sole dedicated MU-MIMO 1X1 station
     * so that it can join back as SU-MIMO 2x2 client
     */
    MU_CAP_TIMER_CMD_KICKOUT_DEDICATED=0,


    /*
     * Timer action to kick out the 1 or multiple dedicated SU-MIMO 2x2 clients
     * when the AP can have multiple MU-MIMO clients instead.
     */
    MU_CAP_TIMER_CMD_KICKOUT_SU_CLIENTS=1
} MU_CAP_TIMER_CMD;

typedef enum {
    MU_CAP_CLIENT_NORMAL=0, /* Non-dedicated MU-MIMO capable client */
    MU_CAP_DEDICATED_SU_CLIENT=1, /* Dedicated SU-MIMO 2X2 client */
    MU_CAP_DEDICATED_MU_CLIENT=2, /* Dedicated MU=MIMO 1X1 client */
    MU_CAP_CLIENT_TYPE_MAX=3
} MU_CAP_CLIENT_TYPE;


typedef struct DEDICATED_CLIENT_MAC {
    u_int8_t macaddr[IEEE80211_ADDR_LEN];
    LIST_ENTRY(DEDICATED_CLIENT_MAC)list;
}DEDICATED_CLIENT_MAC;

typedef struct MU_CAP_WAR {
    /* List of all MU-Capable clients (including dedicated clients) */
    u_int16_t                  mu_cap_client_num[MU_CAP_CLIENT_TYPE_MAX];

#define MAX_PEER_NUM           IEEE80211_512_AID   /*max client numbers per ssid*/
    /*Mu-cap client address associated*/
    u_int8_t                   mu_cap_client_addr[MAX_PEER_NUM][IEEE80211_ADDR_LEN];

    /* Type of MU-capable client */
    MU_CAP_CLIENT_TYPE         mu_cap_client_flag[MAX_PEER_NUM];

    /* Lock for touching the MU-Cap array */
    qdf_spinlock_t             iv_mu_cap_lock;

    /* Timer for kicking out Dedicated clients */
    os_timer_t                 iv_mu_cap_timer;


#define MU_TIMER_PERIOD        15    /* 15*1000 seconds */
    u_int16_t                  mu_cap_timer_period;

    /* Command for the timer callback */
    MU_CAP_TIMER_CMD           mu_timer_cmd;

    /* List of clients to which we have sent MU-Disabled Probe response */
    ATH_LIST_HEAD(, DEDICATED_CLIENT_MAC) dedicated_client_list[IEEE80211_NODE_HASHSIZE];

    /*
     * Number of clients to which we have
     * sent MU-Disabled probe response.
     * This variable is only for display purpose,
     * i.e., displaying on mucapwar get command
     */
    u_int16_t                  dedicated_client_number;

#define MU_TIMER_STOP 0
#define MU_TIMER_PENDING 1
    u_int8_t                   iv_mu_timer_state:1,/* 1=pending, 0=stopped*/

                               /*
                                * MU-CAP WAR Enable/Disable
                                */
                               mu_cap_war:1,/*1: Enable, 0: Disable */

                               /*
                                * This overrides the
                                * MU-CAP WAR Probe response behaviour
                                * If this variable gets set,
                                * the AP WILL NEVER modify its VHTCAP
                                * in probe-response
                                */
                               mu_cap_war_override:1,

                               /*
                                * 1: Disable BFORMER in VHTCAP of
                                *    Probe-Response for dedicated client
                                *
                                * 0: Will not modify the existing
                                *    VHTCAP in probe-response
                                */
                               modify_probe_resp_for_dedicated:1;
} MU_CAP_WAR;

#if QCN_IE
#define DEFAULT_BCAST_PROBE_RESP_DELAY 8                 /* ms */
#define DEFAULT_BCAST_PROBE_RESP_LATENCY_COMPENSATION 5  /* ms */
#define DEFAULT_BEACON_LATENCY_COMPENSATION 5            /* ms */
#endif

#define MAX_NUM_TXPOW_MGT_ENTRY 16
#define MAX_NUM_TXPOW_FRAME_TYPE 3


#define IEEE80211_VAP_IN_FIXED_RATE_MODE(vap) \
   ((vap->iv_fixed_rate.mode == IEEE80211_FIXED_RATE_LEGACY) || \
    (vap->iv_fixed_rate.mode == IEEE80211_FIXED_RATE_MCS) || \
    (vap->iv_fixed_rate.mode == IEEE80211_FIXED_RATE_VHT) || \
    (vap->iv_fixed_rate.mode == IEEE80211_FIXED_RATE_HE))

typedef struct ieee80211vap {
    struct proc_dir_entry			  *iv_proc;
    TAILQ_ENTRY(ieee80211vap)         iv_next;     /* list of vap instances */
    struct ieee80211_wme_state        iv_wmestate; /* WME params corresponding to vap */
    struct ieee80211_muedca_state     iv_muedcastate; /* MUEDCA params
                                                       * corresponding to vap */
#if WAR_DELETE_VAP
    void                              *iv_athvap; /* opaque ath vap pointer */
#endif

    struct ieee80211com               *iv_ic;     /* back ptr to common state */
//    struct net_device_stats           iv_devstats; /* interface statistics */
    u_int                             iv_unit;    /* virtual AP unit */

    void                              *iv_priv;   /* for extending vap functionality */
    os_if_t                           iv_ifp;     /* opaque handle to OS-specific interface */
    char                              *iv_netdev_name;     /* net dev name associated with the VAP */
    wlan_event_handler_table          *iv_evtable;/* vap event handlers */

    os_handle_t                       iv_mlme_arg[IEEE80211_MAX_VAP_MLME_EVENT_HANDLERS];  /* opaque handle used for mlme event handler */
    wlan_mlme_event_handler_table     *iv_mlme_evtable[IEEE80211_MAX_VAP_MLME_EVENT_HANDLERS];  /* mlme event handlers */

    os_handle_t                       iv_misc_arg[IEEE80211_MAX_MISC_EVENT_HANDLERS];
    wlan_misc_event_handler_table     *iv_misc_evtable[IEEE80211_MAX_MISC_EVENT_HANDLERS];

    void*                             iv_event_handler_arg[IEEE80211_MAX_VAP_EVENT_HANDLERS];
    ieee80211_vap_event_handler       iv_event_handler[IEEE80211_MAX_VAP_EVENT_HANDLERS];

    os_if_t                           iv_ccx_arg;  /* opaque handle used for ccx handler */
    wlan_ccx_handler_table            *iv_ccx_evtable;  /* ccx handlers */

    u_int32_t                         iv_debug;   /* debug msg flags */

    struct asf_print_ctrl             iv_print;

    u_int8_t                          iv_myaddr[IEEE80211_ADDR_LEN]; /* current mac address */
    u_int8_t                          iv_my_hwaddr[IEEE80211_ADDR_LEN]; /* mac address from EEPROM */

    enum ieee80211_opmode             iv_opmode;  /* operation mode */
    int                               iv_scan_priority_base;  /* Base value used to determine priority of scans requested by this VAP */
    u_int32_t                         iv_create_flags;   /* vap create flags */

    u_int32_t                         iv_flags;   /* state flags */
    u_int32_t                         iv_flags_ext;   /* extension of state flags */
    u_int32_t                         iv_flags_ext2;   /* extension 2 of state flags */
/* not multiple thread safe */
    u_int32_t                         iv_deleted:1,  /* if the vap deleted by user */
                                      iv_scanning:1, /* if the vap is scanning */
                                      iv_smps:1,     /* if the vap is in static mimo ps state */
                                      iv_standby:1,  /* if the vap is temporarily stopped */
                                      iv_cansleep:1, /* if the vap can sleep*/
                                      iv_sw_bmiss:1, /* use sw bmiss timer */
                                      iv_copy_beacon:1, /* enable beacon copy */
                                      iv_wapi:1,
                                      iv_sta_fwd:1, /* set to enable sta-fws fweature */
                                      iv_dynamic_mimo_ps, /* dynamic mimo ps enabled */
                                      iv_doth:1,     /* 802.11h enabled */
                                      iv_country_ie:1,  /* Country IE enabled */
                                      iv_wme:1, /* wme enabled */
                                      iv_off_channel_support, /* Off-channel support enabled */ //subrat
                                      iv_dfswait:1,   /* if the vap is in dfswait state */
                                      iv_erpupdate:1, /* if the vap has erp update set */
                                      iv_needs_scheduler:1, /* this vap needs scheduler for off channel operation */

                                      iv_no_multichannel:1, /*does not want to trigger multi channel operation
                                                            instead follow master vaps channel (for AP/GO Vaps) */
                                      iv_vap_ind:1,   /* if the vap has wds independance set */
                                      iv_forced_sleep:1,        /*STA in forced sleep set PS bit for all outgoing frames */
                                      iv_qbssload:1, /* QBSS load IE enabled */
                                      iv_qbssload_update:1, /* update qbssload IE in beacon */
                                      iv_rrm:1, /* RRM Capabilities */
                                      iv_wnm:1, /* WNM Capabilities */
                                      iv_proxyarp:1, /* WNM Proxy ARP Capabilities */
                                      iv_dgaf_disable:1, /* Hotspot 2.0 DGAF Disable bit */
                                      iv_ap_reject_dfs_chan:1,  /* SoftAP to reject resuming in DFS channels */
                                      iv_smartnet_enable:1,  /* STA SmartNet enabled */
                                      iv_trigger_mlme_resp:1,  /* Trigger mlme response */
                                      iv_mfp_test:1;   /* test flag for MFP */
    u_int32_t                         iv_sgi:2,        /* Short Guard Interval 3600:3 1600:2 400:1 800:0 */
                                      iv_he_sgi:2,      /*Short Guard Interval 800:0 for he mode only. Kept as 2 bit for future provision of enhanced HW cap*/
                                      iv_data_sgi:2,   /* Short Guard Interval 3600:3 1600:2 400:1 800:0 for VHT & HE fixed rates */
                                      iv_he_data_sgi:2,   /* Short Guard Interval 3600:3 1600:2 800:0 for HE fixed rates*/
                                      iv_ratemask_default:1, /* flag to indicate using default ratemask */
                                      iv_key_flag:1,    /*For wakeup AP VAP when wds-sta connect to the AP only use when export (UMAC_REPEATER_DELAYED_BRINGUP || DBDC_REPEATER_SUPPORT)=1*/
                                      iv_list_scanning:1, /* if performe the iwlist scanning */
                                      iv_vap_is_down:1, /*Set when VAP down*/
#if ATH_SUPPORT_ZERO_CAC_DFS
                                      iv_pre_cac_timeout_channel_change:1,
#endif
                                      iv_needs_up_on_acs:1, /* if vap may require acs when another vap is brought down */
                                      iv_quick_reconnect:1, /* if vap (STA VAP) it attempting a quick reconnect to AP */
                                      /* Set when there is a change in MU-EDCA
                                       * params advertised by AP. It will be
                                       * reset once STA sends the updated
                                       * parameters to the target. */
                                      iv_update_muedca_params:1,
                                      iv_reset_ap_vaps:1, /* Used to check if reset is needed in wds_repeater mode */
                                      iv_stop_bss:1,    /* stop bss in progress */
                                      iv_peer_create_failed:1,    /* AP peer create failed */
                                      iv_ext_bssload:1,   /* Extended BSS Load IE in beacon */
                                      iv_ext_bssload_update:1,   /* update extended BSS Load IE in beacon */
                                      iv_he_ul_sgi:2, /* UL Short GI setting */
                                      iv_he_ul_ltf:2, /* UL LTF setting */
                                      iv_he_ul_ldpc:1, /* UL LDPC mode */
                                      iv_he_ul_stbc:1, /* UL STBC mode */
                                      iv_he_basic_mcs_for_bss:2,   /* basic mcs requirement for the bss */
                                      iv_registered:1, /* Virtual interface registered with Kernel */
                                      iv_bcca_ie_status:2; /* BCCA IE update */
    bool                              iv_is_spatial_reuse_updated; /* Spatial Reuse update status */
    bool                              iv_is_heop_param_updated; /* HE Op Param update status */
    u_int8_t                          iv_he_basic_nss_for_bss;   /* basic nss requirement for the bss */
    u_int16_t                         iv_he_ul_nss:4, /* UL NSS setting */
                                      iv_he_ul_fixed_rate:4; /* UL Fixed rate MCS */
    u_int8_t                          iv_he_ul_ppdu_bw; /* UL channel width */
    u_int16_t                         iv_he_amsdu_in_ampdu_suprt:1, /* configur HE MAC Cap amsdu in ampdu */
                                      iv_he_subfee_sts_lteq80:3, /* configure HE PHY Cap subfee_sts_lteq80 */
                                      iv_he_subfee_sts_gt80:3,   /* configure HE PHY Cap subfee_sts_gt80 */
                                      iv_he_4xltf_800ns_gi:1,    /* configure HE PHY Cap 4xltf+800ns gi support */
                                      iv_he_1xltf_800ns_gi:1,    /* configure HE PHY Cap 1xltf+800ns gi support */
                                      iv_he_max_nc:3,            /* configure HE PHY Cap max_nc */
                                      iv_twt_rsp:1;              /* configure HE MAC Cap twt responder */
    u_int32_t                         iv_256qam:1;     /* 256 QAM support in 2.4GHz mode Enable:1 Disable:0 */
    u_int8_t                          iv_11ng_vht_interop:1;     /* 2.4NG 256 QAM Interop mode Enable:1 Disable:0 */
    u_int8_t                          iv_mbo:1;    /* for mbo functionality */
    u_int8_t                          iv_oce:1;    /* for oce functionality */
#if DYNAMIC_BEACON_SUPPORT
    u_int8_t                          iv_dbeacon:1;         /* Flag for Dynamic Beacon */
    u_int8_t                          iv_dbeacon_runtime:1; /* Runtime flag to suspend/resume dbeacon for DFS or non DFS channel */
#endif

#if QCA_SUPPORT_GPR
    u_int8_t                          iv_gpr_enable : 1;  /* If set, gratuitous probe response is enabled */
#endif


    u_int8_t                          iv_ext_ifu_acs;  /* Enable external auto channel selection
                                                          entity at VAP init time */
    u_int8_t                          iv_ext_acs_inprogress;     /* Whether external auto channel selection is
                                                                    progress */
    u_int8_t                          iv_send_additional_ies;    /* Enable sending of Extra IEs to host */

    u_int8_t                          vlan_set_flags;    /* Dev Flags to control vlan tagged packets
                                                              sent out by NW stack */
    u_int8_t                          iv_rawmode_pkt_sim;
    u_int8_t                          iv_wifi_down;

#if QCA_SUPPORT_RAWMODE_PKT_SIMULATION
    struct rawmode_pkt_sim_rxstats rsim_rxstats;
    struct rawmode_pkt_sim_txstats rsim_txstats;
#endif /* QCA_SUPPORT_RAWMODE_PKT_SIMULATION */


    /* multiple thread safe */
    u_int32_t                         iv_caps;    /* capabilities */
    u_int16_t                         iv_ath_cap;                       /* Atheros adv. capablities */
    u_int8_t                          iv_chanchange_count;              /* 11h counter for channel change */
    u_int16_t                         iv_he_bsscolor_change_count;      /* 11ax counter for bss color change */
    bool                              iv_he_bsscolor_change_ongoing;    /* is BCCA ongoing for this vap */
    bool                              iv_he_bsscolor_detcn_configd_vap; /* Have we configured collision
                                                                           detection in fw for this vap */
    bool                              iv_he_bsscolor_remove_ie;         /* if true trigger beacon_reinit
                                                                           to remove BCCA ie */
    int                               iv_mcast_rate; /* Multicast rate (Kbps) */
    int                               iv_mcast_fixedrate; /* fixed rate for multicast */
    u_int8_t                          iv_mcast_rate_config_defered; /* Non Zero - Multi cast rate confuguration is defered */
    u_int32_t                         iv_node_count; /* node count */
    atomic_t                          iv_rx_gate; /* pending rx threads */
    struct ieee80211_mac_stats        iv_unicast_stats;   /* mac statistics for unicast frames */
    struct ieee80211_mac_stats        iv_multicast_stats; /* mac statistics for multicast frames */
    struct tkip_countermeasure        iv_unicast_counterm;  /* unicast tkip countermeasure */
    struct tkip_countermeasure        iv_multicast_counterm;  /* unicast tkip countermeasure */

    spinlock_t                        iv_lock; /* lock to protect access to vap object data */
    unsigned long                     *iv_aid_bitmap; /* association id map */
    u_int16_t                         iv_max_aid;
    u_int16_t                         iv_sta_assoc;   /* stations associated */
    u_int16_t                         iv_ax_sta_assoc; /* 11ax capable stations associated */
    u_int16_t                         iv_mu_bformee_sta_assoc;   /* mu mimo stations associated */
    u_int16_t                         iv_ps_sta;  /* stations in power save */
    u_int16_t                         iv_ps_pending;  /* ps sta's w/ pending frames */
    u_int8_t                          iv_dtim_count;  /* DTIM count from last bcn */
    u_int8_t                          iv_atim_window; /* ATIM window */
    u_int8_t                          *iv_tim_bitmap; /* power-save stations w/ data*/
    u_int16_t                         iv_tim_len;     /* ic_tim_bitmap size (bytes) */
                                      /* set/unset aid pwrsav state */
    u_int8_t                          iv_rescan;
    void                              (*iv_set_tim)(struct ieee80211_node *, int,bool isr_context);
    int                               (*iv_alloc_tim_bitmap)(struct ieee80211vap *vap);
    struct ieee80211_node             *iv_bss;    /* information for this node */
    struct ieee80211_rateset          iv_op_rates[IEEE80211_MODE_MAX]; /* operational rate set by os */
    struct ieee80211_fixed_rate       iv_fixed_rate;  /* 802.11 rate or -1 */
    u_int32_t                         iv_fixed_rateset;  /* 802.11 rate set or -1(invalid) */
    u_int32_t                         iv_fixed_retryset; /* retries */
    u_int32_t                         iv_legacy_ratemasklower32;    /*lower 32 bit of ratemask for legacy mode*/
    u_int32_t                         iv_ht_ratemasklower32;    /*lower 32 bit of ratemask for HT mode*/
    /* The ratemask variables are used to configure the maximum NSS MCS support
     * on the AP. The mapping of the variables to the NSS MCS rate setting is
     * as follows:
     *
     * ratemasklower32      ==> b0 - b31
     * ratemaskhigher32     ==> b32 - b63
     * ratemasklower32_2    ==> b64 - b95
     *
     * Bitmap for ratemask(for HE targets):
     *
     *     NSS8/MCS0-11    NSS7/MCS0-11         NSS2/MCS0-11    NSS1/MCS0-11
     *  |               |               |     |               |              |
     *  |   b95 - b84   |   b83 - b72   |.....|   b23 - b12   |   b11 - b0   |
     *
     *
     * Bitmap for ratemask(for legacy targets):
     *
     *    NSS4/MCS0-9     NSS3/MCS0-9      NSS2/MCS0-9    NSS1/MCS0-9
     *  |               |               |               |              |
     *  |   b39 - b30   |   b29 - b20   |   b19 - b10   |    b9 - b0   |
     *
     *
     * Each bit in the map corresponds to an MCS value, and 12 bits will
     * correspond to one NSS setting for HE targets.
     * For example: b0-b11 will correspond to NSS1 and MCS0-11
     *
     * For legacy targets 10 bits will correspond to one NSS setting.
     * For example: b0-b9 will correspond to NSS1 and MCS0-9
     *
     *  */
    u_int32_t                         iv_vht_ratemasklower32;    /*lower 32 bit of ratemask for VHT mode*/
    u_int32_t                         iv_vht_ratemaskhigher32;  /*higher 32 bit of ratemask for VHT mode for beeliner*/
    u_int32_t                         iv_vht_ratemasklower32_2;
    u_int32_t                         iv_he_ratemasklower32;
    u_int32_t                         iv_he_ratemaskhigher32;
    u_int32_t                         iv_he_ratemasklower32_2;
    u_int16_t                         iv_def_txkey;   /* default/group tx key index */
#if ATH_SUPPORT_AP_WDS_COMBO
    u_int8_t                          iv_no_beacon;   /* VAP does not transmit beacon/probe resp. */
#endif
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
    struct ieee80211_key              iv_nw_keys[IEEE80211_WEP_NKID];

    struct ieee80211_key              iv_igtk_key;

    struct ieee80211_rsnparms         iv_rsn;         /* rsn information */
#endif
    unsigned long                     iv_assoc_comeback_time;    /* assoc comeback information */
    int                               iv_pmf_enable;
    int                               iv_rsn_override;

    ieee80211_privacy_exemption       iv_privacy_filters[IEEE80211_MAX_PRIVACY_FILTERS];    /* privacy filters */
    u_int32_t                         iv_num_privacy_filters;
    ieee80211_pmkid_entry             iv_pmkid_list[IEEE80211_MAX_PMKID];
    u_int16_t                         iv_pmkid_count;
    u_int8_t                          iv_wps_mode;    /* WPS mode */
    u_int8_t                          iv_wep_mbssid;    /* force multicast wep keys in first 4 entries 0=yes, 1=no */

    enum ieee80211_phymode            iv_des_mode; /* desired wireless mode for this interface. */
    enum ieee80211_phymode            iv_des_hw_mode; /* desired hardware mode for this interface. */
    u_int16_t                         iv_des_modecaps;   /* set of desired phy modes for this VAP */
    enum ieee80211_phymode            iv_cur_mode; /* current wireless mode for this interface. */
    struct ieee80211_ath_channel      *iv_des_chan[IEEE80211_MODE_MAX]; /* desired channel for each PHY */
    struct ieee80211_ath_channel      *iv_bsschan;   /* bss channel */
    u_int8_t                          iv_des_ibss_chan;   /* desired ad-hoc channel */
    u_int8_t                          iv_des_cfreq2;     /* Desired cfreq2 for VHT80_80 mode */
    u_int8_t                          iv_rateCtrlEnable;  /* enable rate control */
    u_int8_t                          iv_rc_txrate_fast_drop_en;    /* enable tx rate fast drop*/
    int                               iv_des_nssid;       /* # desired ssids */
    ieee80211_ssid                    iv_des_ssid[IEEE80211_SCAN_MAX_SSID];/* desired ssid list */
    /* configure bssid of hidden ssid AP using iwpriv cmd */
    u_int8_t                          iv_conf_des_bssid[IEEE80211_ADDR_LEN];

    int                               iv_bmiss_count;
    int                               iv_bmiss_count_for_reset;
    int                               iv_bmiss_count_max;
    systime_t                         iv_last_beacon_time;         /* absolute system time, not TSF */
    systime_t                         iv_last_directed_frame;      /* absolute system time; not TSF */
    systime_t                         iv_last_ap_frame;            /* absolute system time; not TSF */
    systime_t                         iv_last_traffic_indication;  /* absolute system time; not TSF */
    systime_t                         iv_lastdata;                 /* absolute system time; not TSF */
    u_int64_t                         iv_txrxbytes;                /* No. of tx/rx bytes  */
    ieee80211_power_t                 iv_power;                    /* handle private to power module */
    ieee80211_sta_power_t             iv_pwrsave_sta;
    ieee80211_pwrsave_smps_t          iv_pwrsave_smps;
    u_int16_t                         iv_keep_alive_timeout;       /* keep alive time out */
    u_int16_t                         iv_inact_count;               /* inactivity count */
    u_int8_t                          iv_smps_rssithresh;
    u_int8_t                          iv_smps_datathresh;
    unsigned long                     iv_pending_sends;

    u_int8_t                          iv_lastbcn_phymode_mismatch;        /* Phy mode mismatch between scan entry, BSS */


    /* NEW APP IE implementation. Note, we need to obselete the old one */
    APP_IE_LIST_HEAD                  iv_app_ie_list[IEEE80211_APPIE_MAX_FRAMES];

    /* TODO: we need to obselete the use of these 2 fields */
    /* app ie buffer */
    struct ieee80211_app_ie_t         iv_app_ie[IEEE80211_APPIE_MAX_FRAMES];
    u_int16_t                         iv_app_ie_maxlen[IEEE80211_APPIE_MAX_FRAMES];

    IEEE80211_VEN_IE                 *iv_venie;

    struct wlan_mlme_app_ie          *vie_handle;       /*  vendor application IE handle */

    /* opt ie buffer - currently used for probereq and assocreq */
    struct ieee80211_app_ie_t         iv_opt_ie;
    u_int16_t                         iv_opt_ie_maxlen;

    /* Copy of the beacon frame */
    u_int8_t                          *iv_beacon_copy_buf;
    int                               iv_beacon_copy_len;

    /* country ie data */
    u_int64_t                         iv_country_ie_chanflags;
    struct ieee80211_ie_country       iv_country_ie_data; /* country info element */

    u_int8_t                      iv_mlmeconnect;     /* Assoc state machine roaming mode */

    /* RATE DROPDOWN CONTROL */
    u_int32_t                         iv_ratedrop;

    /* U-APSD Settings */
    u_int8_t                          iv_uapsd;
    u_int8_t                          iv_wmm_enable;
    u_int8_t                          iv_wmm_power_save;

    ieee80211_mlme_priv_t             iv_mlme_priv;    /* opaque handle to mlme private information */
    ieee80211_aplist_config_t         iv_aplist_config;

    ieee80211_candidate_aplist_t      iv_candidate_aplist;    /* opaque handle to aplist private information */

    ieee80211_resmgr_vap_priv_t       iv_resmgr_priv;         /* opaque handle with resource manager private info */

    ieee80211_vap_pause_info          iv_pause_info;          /* pause private info */

    u_int8_t                          iv_assoc_denial_notify; /* param to config assoc denial notification incase of ACL */
    ieee80211_acl_t                   iv_acl;   /* opaque handle to acl */
    ieee80211_vap_ath_info_t          iv_vap_ath_info_handle; /* opaque handle for VAP_ATH_INFO */
    enum ieee80211_protmode           iv_protmode;            /* per vap 802.11g protection mode */

    u_int8_t                          iv_ht40_intolerant;
    u_int8_t                          iv_chwidth;
    u_int8_t                          iv_sta_negotiated_ch_width; /*Channel width negitiated by station during association*/
    u_int8_t                          iv_chextoffset;
    u_int8_t                          iv_disable_HTProtection;
    u_int32_t                         iv_chscaninit;
    int                               iv_proxySTA;
    u_int8_t                          dyn_bw_rts;              /* dynamic bandwidth RTS */

#if ATH_SUPPORT_WRAP
    u_int8_t                          iv_psta:1,        /* VAP type is PSTA */
                                      iv_mpsta:1,       /* VAP type is MPSTA */
                                      iv_wrap:1,        /* VAP type is WRAP */
                                      iv_mat:1,         /* VAP type is MAT */
                                      iv_wired_pvap:1;  /* proxy vap for wired sta */
    u_int8_t                          iv_mat_addr[IEEE80211_ADDR_LEN];      /* mat address for PSTA */
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    u_int8_t                          iv_nss_qwrap_en;
#endif
#endif

    /* Optional feature: mcast enhancement */
#if ATH_SUPPORT_IQUE
    struct ieee80211_ique_me          *iv_me;
    struct ieee80211_hbr_list         *iv_hbr_list;
#endif
    struct ieee80211_ique_ops         iv_ique_ops;
    struct ieee80211_beacon_info      iv_beacon_info[100]; /*BSSID and RSSI info*/
    u_int8_t                          iv_beacon_info_count;
    u_int8_t                          iv_esslen;
    u_int8_t                          iv_essid[IEEE80211_NWID_LEN+1];
    struct ieee80211_ibss_peer_list   iv_ibss_peer[8]; /*BSSID and RSSI info*/
    u_int8_t                          iv_ibss_peer_count;

    /* channel_change_done is a flag value used to indicate that a channel
     * change happend for this VAP. This information (for now) is used to update
     * the beacon information and then reset. This is needed in case of DFS channel change
     */
    u_int8_t channel_change_done;
    u_int8_t appie_buf_updated;
    u_int8_t iv_doth_updated;
    u_int8_t mixed_encryption_mode;
    bool                             beacon_reinit_done;
    u_int32_t 			     iv_ena_vendor_ie:1;
    u_int32_t                        iv_update_vendor_ie:1;
    ieee80211_vap_input_mgmt_filter   iv_input_mgmt_filter;   /* filter  input mgmt frames */
    ieee80211_vap_output_mgmt_filter  iv_output_mgmt_filter;  /* filter outpur mgmt frames */
    struct ieee80211_roam_t           iv_roam; /* roam context */
    QDF_STATUS                        (*iv_up_pre_init)(struct wlan_objmgr_vdev *, bool);
    QDF_STATUS                        (*iv_up_complete)(struct wlan_objmgr_vdev *);
    int                               (*iv_join)(struct ieee80211vap *);
    QDF_STATUS                        (*iv_down)(struct wlan_objmgr_vdev *);
    QDF_STATUS                        (*iv_stop_pre_init)(struct wlan_objmgr_vdev *);
    QDF_STATUS                        (*iv_start_pre_init)(struct wlan_objmgr_vdev *,
                                                           struct ieee80211_ath_channel *, int);
    QDF_STATUS                        (*iv_start_post_init)(struct wlan_objmgr_vdev *,
                                                            struct ieee80211_ath_channel *chan,
                                                            bool restart);
    QDF_STATUS                        (*iv_vap_start_rsp_handler)(struct vdev_start_response *, struct vdev_mlme_obj *);
    int                               (*iv_dfs_cac)(struct ieee80211vap *);
    void                              (*iv_update_ps_mode)(struct ieee80211vap *);
    void                              (*iv_cleanup)(struct ieee80211vap *);
    int                               (*iv_listen)(struct ieee80211vap *);

#ifndef WLAN_CONV_CRYPTO_SUPPORTED
    int                               (*iv_key_alloc)(struct ieee80211vap *,
                                                      struct ieee80211_key *);
    int                               (*iv_key_delete)(struct ieee80211vap *,
                                                       const struct ieee80211_key *,
                                                       struct ieee80211_node *);
    int                               (*iv_key_map)(struct ieee80211vap *,
                                                     const struct ieee80211_key *,
                                                     const u_int8_t bssid[IEEE80211_ADDR_LEN],
                                                     struct ieee80211_node *);
    int                               (*iv_key_set)(struct ieee80211vap *,
                                                    struct ieee80211_key *,
                                                    const u_int8_t mac[IEEE80211_ADDR_LEN]);
    void                              (*iv_key_update_begin)(struct ieee80211vap *);
    void                              (*iv_key_update_end)(struct ieee80211vap *);
#endif
    int                               (*iv_root_authorize)(struct ieee80211vap *, u_int32_t);
    void                              (*iv_update_node_txpow)(struct ieee80211vap *, u_int16_t , u_int8_t *);
    int                               (*iv_set_proxysta)(struct ieee80211vap *vap, int enable);

    int                               (*iv_reg_vap_ath_info_notify)(struct ieee80211vap *,
                                                                    ath_vap_infotype,
                                                                    ieee80211_vap_ath_info_notify_func,
                                                                    void *);
    int                               (*iv_vap_ath_info_update_notify)(struct ieee80211vap *,
                                                                       ath_vap_infotype);
    int                               (*iv_dereg_vap_ath_info_notify)(struct ieee80211vap *);
    int                               (*iv_vap_ath_info_get)(struct ieee80211vap *,
                                                             ath_vap_infotype,
                                                             u_int32_t *, u_int32_t *);
    int                               (*iv_enable_radar_table)( struct ieee80211com *,
                                                         struct ieee80211vap *, u_int8_t, u_int8_t);
    int                               (*iv_vap_send)(struct ieee80211vap *, wbuf_t);
    int                               (*iv_peer_rel_ref)(struct ieee80211vap *,
                                                         struct ieee80211_node *, uint8_t *peer_mac);
#if ATH_WOW
    u_int8_t                          iv_wowflags;         /* Flags for wow */
#define IEEE80211_F_WOW_DEAUTH             1               /* Deauth Pattern */
#define IEEE80211_F_WOW_DISASSOC           2               /* Disassoc Pattern */
    struct ieee80211_wowpattern       iv_patterns[8];      /* Patterns status */
#if ATH_WOW_OFFLOAD
    int                               (*iv_vap_wow_offload_rekey_misc_info_set)(struct ieee80211com *,
                                                            struct wow_offload_misc_info *);
    int                               (*iv_vap_wow_offload_info_get)(struct ieee80211com *,
                                                            void *buf, u_int32_t param);
    int                               (*iv_vap_wow_offload_txseqnum_update)(struct ieee80211com *,
                                                            struct ieee80211_node *ni, u_int32_t tidno, u_int16_t seqnum);
#endif /* ATH_WOW_OFFLOAD */
#endif
    bool                              (*iv_modify_bcn_rate)(struct ieee80211vap *);
    u_int8_t                          iv_ccmpsw_seldec;  /* Enable/Disable encrypt/decrypt of frames selectively ( frames with KEYMISS) */
    u_int16_t                         iv_mgt_rate;       /* rate to be used for management rates */
    u_int8_t                          iv_mgt_rate_config_defered; /* Non Zero - mgmt rate configuration is defered */
    u_int16_t                         iv_prb_rate;       /* rate to be used for probe-response */
#if UMAC_SUPPORT_NAWDS
    struct ieee80211_nawds            iv_nawds;
#endif
#if WDS_VENDOR_EXTENSION
    u_int8_t                          iv_wds_rx_policy;
#define WDS_POLICY_RX_UCAST_4ADDR       0x01
#define WDS_POLICY_RX_MCAST_4ADDR       0x02
#define WDS_POLICY_RX_DEFAULT           0x03
#define WDS_POLICY_RX_MASK              0x03
#endif

    ieee80211_vap_tsf_offset          iv_tsf_offset;    /* TSF-related data utilized for concurrent multi-channel operations */
#ifdef ATH_SUPPORT_QUICK_KICKOUT
    u_int8_t						  iv_sko_th;        /* station kick out threshold */
#endif /*ATH_SUPPORT_QUICK_KICKOUT*/
    struct ieee80211_chanutil_info     chanutil_info; /* Channel Utilization information */
#if UMAC_SUPPORT_CHANUTIL_MEASUREMENT
    u_int8_t                           iv_chanutil_enab;
#endif /* UMAC_SUPPORT_CHANUTIL_MEASUREMENT */

    /* add flag to enable/disable auto-association */
    u_int8_t    auto_assoc;
                              /* in activity timeouts */
    u_int8_t    iv_inact_init;
    u_int8_t    iv_inact_auth;
    u_int8_t    iv_inact_probe;
    u_int32_t   iv_session;      /* STA session time */
#if ATH_SW_WOW
    u_int8_t                          iv_sw_wow;           /* Flags for sw wow */
#endif
#ifdef ATH_SUPPORT_TxBF
    u_int8_t    iv_txbfmode;
    u_int8_t    iv_autocvupdate;
    u_int8_t    iv_cvupdateper;
#endif
    struct ieee80211_node             *iv_ni;
    struct ieee80211_ath_channel          *iv_cswitch_chan;
    u_int8_t                          iv_cswitch_rxd;
    os_timer_t                        iv_cswitch_timer;
    os_timer_t                        iv_disconnect_sta_timer;
#if ATH_SUPPORT_WAPI
    u32    iv_wapi_urekey_pkts;/*wapi unicast rekey packets, 0 for disable*/
    u32    iv_wapi_mrekey_pkts;/*wapi muiticast rekey packets, 0 for disable*/
#endif

#if ATH_SUPPORT_IBSS_DFS
    struct ieee80211_ibssdfs_ie iv_ibssdfs_ie_data; /* IBSS DFS element */
    struct ieee80211_ath_channelswitch_ie iv_channelswitch_ie_data; /* csa data */
    u_int8_t                    iv_measrep_action_count_per_tbtt;
    u_int8_t                    iv_csa_action_count_per_tbtt;
    u_int8_t                    iv_ibssdfs_recovery_count;
    u_int8_t                    iv_ibssdfs_state;
    u_int8_t                    iv_ibss_dfs_csa_threshold;
    u_int8_t                    iv_ibss_dfs_csa_measrep_limit;
    u_int8_t                    iv_ibss_dfs_enter_recovery_threshold_in_tbtt;
    /*update dfs owner field without channel change ,
      is being used when we switch off dfs owner */
    u_int8_t                    iv_ibss_dfs_no_channel_switch;
#endif /* ATH_SUPPORT_IBSS_DFS */

#define IEEE80211_SCAN_BAND_ALL            (0)
#define IEEE80211_SCAN_BAND_2G_ONLY        (1)
#define IEEE80211_SCAN_BAND_5G_ONLY        (2)
#define IEEE80211_SCAN_BAND_CHAN_ONLY      (3)
    u_int8_t                    iv_scan_band;       /* only scan channels of requested band */
    u_int32_t                   iv_scan_chan;       /* only scan specific channel */
    struct ieee80211_quiet_param *iv_quiet;
#if ATH_SUPPORT_IBSS_NETLINK_NOTIFICATION
#define IBSS_RSSI_CLASS_MAX 7
    u_int8_t			iv_ibss_rssi_monitor;
    u_int8_t			iv_ibss_rssi_class[IBSS_RSSI_CLASS_MAX];
    u_int8_t			iv_ibss_rssi_hysteresis;
#endif
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
    struct ath_linkdiag *iv_ald;
#endif
#if UMAC_SUPPORT_RRM
    ieee80211_rrm_t             rrm; /* handle for rrm  */
#endif
#if UMAC_SUPPORT_WNM
    ieee80211_wnm_t             wnm; /* handle for wnm  */
#endif
#if ATH_SUPPORT_HS20
    u_int8_t                    iv_hessid[IEEE80211_ADDR_LEN];
    u_int8_t                    iv_access_network_type;
    u_int32_t                   iv_hotspot_xcaps;
    u_int32_t                   iv_hotspot_xcaps2;
    u_int32_t                   iv_hc_bssload;
    struct ieee80211_qos_map    iv_qos_map;
    u_int8_t                    iv_osen;
#endif
    u_int8_t iv_wep_keycache; /* static wep keys are allocated in first four slots in keycahe */
#if ATH_SUPPORT_WPA_SUPPLICANT_CHECK_TIME
    int                 iv_rejoint_attemp_time;
#endif
#if ATH_SUPPORT_WIFIPOS
    ieee80211_wifipos_data_t    *iv_wifipos;
    int                         iv_wakeup;
    u_int64_t                   iv_tsf_sync;
    u_int64_t                   iv_local_tsf_tstamp;
#endif
    u_int8_t iv_send_deauth; /* for sending deauth instead of disassoc while doing apdown */
    struct ieee80211_tim_set iv_tim_infor;
#if UMAC_SUPPORT_WNM
    u_int16_t           iv_wnmsleep_intval;
    u_int8_t            iv_wnmsleep_force;
#endif
#if ATH_SUPPORT_WRAP
    u_int8_t            iv_no_event_handler;
    int (*iv_wrap_mat_tx)(struct ieee80211vap *out, wbuf_t);
    int (*iv_wrap_mat_rx)(struct ieee80211vap *in, wbuf_t);
#endif
    bool iv_is_started;
    bool iv_is_up;
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
    u_int8_t            iv_nopbn;  /* no push button notification */
#endif
#if ATH_SSID_STEERING
    u_int8_t     iv_vap_ssid_config; /* To store Vap's configuration Public or Private */
#endif
#if UMAC_PER_PACKET_DEBUG
#define PROC_FNAME_SIZE 20
    int16_t iv_userrate;
    int8_t iv_userretries;
    int8_t iv_usertxpower;
    int8_t iv_usertxchainmask;
    struct proc_dir_entry   *iv_proc_entry;
    struct proc_dir_entry   *iv_proc_root;
    u_int8_t                 iv_proc_fname[PROC_FNAME_SIZE];
#endif
    rwlock_t            iv_tim_update_lock;

    /* vap tx dynamic aponly mode control */
    bool                    iv_aponly;
    /* force to tx with one chain for legacy client */
    bool                    iv_force_onetxchain;
    u_int8_t                   iv_vht_fixed_mcs;        /* VHT Fixed MCS Index */
    u_int8_t                   iv_he_fixed_mcs;         /* HE  Fixed MCS Index */
    u_int8_t                   iv_tx_stbc;              /* TX STBC Enable:1 Disable:0 */
    u_int8_t                   iv_rx_stbc;              /* RX STBC Enable:(1,2,3) Disable:0 */
    u_int8_t                   iv_opmode_notify;        /* Op Mode notification On:1 Off:0 */
    u_int8_t                   iv_rtscts_enabled;       /* RTS-CTS 1: enabled, 0: disabled */
    u_int8_t                   iv_rc_num_retries;       /* Number of HW retries across rate-series */
    int                        iv_bcast_fixedrate;      /* Bcast data rate */
    u_int8_t                   iv_bcast_rate_config_defered; /* Non Zero - Broadcast cast rate confuguration is defered */
    u_int16_t                  iv_vht_tx_mcsmap;        /* VHT TX MCS MAP */
    u_int16_t                  iv_vht_rx_mcsmap;        /* VHT RX MCS MAP */
    u_int16_t                  iv_disabled_legacy_rate_set; /* disable the legacy rates set for nodes connected to this vap. Each bit represents a legacy rate*/
    u_int8_t                   iv_disable_htmcs;
    u_int8_t                   iv_disabled_ht_mcsset[16]; /* disable these HT MCS rates set for nodes connected to this vap. Each bit represents a MCS rate*/
    u_int16_t                  iv_configured_vht_mcsmap;  /* All only those VHT mcs rates for nodes connected to this vap on this field */
    bool                       iv_set_vht_mcsmap;
    ieee80211_vht_mcs_set_t    iv_vhtcap_max_mcs;      /* VHT Supported MCS set */
    spinlock_t                 init_lock;
    u_int16_t                  min_dwell_time_passive;  /* Min dwell time for scan */
    u_int16_t                  max_dwell_time_passive;	/* Max dwell time for scan */
#if QCA_LTEU_SUPPORT
    u_int32_t                  scan_rest_time;          /* overriding san param */
    u_int32_t                  scan_idle_time;          /* overriding san param */
    u_int16_t                  mu_start_delay;          /* Delay between setting channel and starting MU */
    u_int16_t                  wifi_tx_power;           /* WiFi STA's Tx power, assumed for MU computation */
#endif
#if HOST_SW_LRO_ENABLE
    ath_lro_entry_t            ath_le_arr[LRO_MAX_ENTRY];   /* lro descriptor array */
    u_int32_t                  lro_to_flush;           /* flag to check if flush is required
							* when lro is disabled in the middle
							* aggregation */

    u_int32_t                  aggregated;       /* LRO stats: how many packets aggregated */
    u_int32_t                  flushed;          /* LRO stats: how many aggregated packets
						  * flushed    */
#endif /* HOST_SW_LRO_ENABLE */

    char                       iv_oper_rates[IEEE80211_RATE_MAX]; /*operational rates set by the user*/
    char                       iv_basic_rates[IEEE80211_RATE_MAX]; /*basic rates set by the user*/
    u_int16_t                  iv_vht_sgimask;          /* VHT SGI MASK */
    u_int32_t                  iv_vht80_ratemask;       /* VHT80 Auto Rate MASK */
/*
 To support per vap dscp to tid mapping.
 If vap specific dscp to tid mapping is not enabled,
 radio specific dscp to tid mapping will be effective by default.
*/
#if ATH_SUPPORT_DSCP_OVERRIDE
    u_int8_t iv_dscp_map_id;
    u_int8_t                   iv_vap_dscp_priority;        /* VAP Based DSCP - per vap priority */
#endif
 u_int8_t                   iv_pause_scan;
#if ATH_PERF_PWR_OFFLOAD
    u_int8_t                   iv_tx_encap_type;        /* per VAP tx encap type - should be used
                                                           when setting up HTT desc for packets for this VAP */
    u_int8_t                   iv_rx_decap_type;        /* per VAP Rx decap type */
#if QCA_SUPPORT_RAWMODE_PKT_SIMULATION
    u_int8_t                   iv_rawmodesim_txaggr;    /* per VAP Enable/disable raw mode simulation Tx A-MSDU aggregation */
    u_int8_t                   iv_rawmodesim_debug;     /* per VAP Enable/disable raw mode simulation debug */
#endif /* QCA_SUPPORT_RAWMODE_PKT_SIMULATION */
#endif /* ATH_PERF_PWR_OFFLOAD */
    u_int8_t                   mhdr_len;
#if MESH_MODE_SUPPORT
    u_int8_t                   iv_mesh_vap_mode;        /* mesh vap flag */
#endif
    u_int8_t                   iv_mesh_mgmt_txsend_config; /* flag to enable/disable probe and disassoc tx in mesh mode*/
    u_int8_t                   iv_special_vap_mode;     /* special vap flag */
    u_int8_t                   iv_special_vap_is_monitor; /* special vap itself is monitor vap */
    u_int8_t                   iv_smart_monitor_vap;     /* smart monitar vap */
    uint32_t                   iv_vhtbfeestscap:3;     /* VHT Beamformee STS Capability        */
#ifdef QCA_PARTNER_PLATFORM
    struct partner_vap_param   partner_vap_params;
#endif
    u_int8_t                   iv_novap_reset;    /* per VAP Enable/disable reset in NSS setting */
    u_int32_t                  iv_sifs_trigger_rate;    /* per VAP sifs trigger rate */
    u_int32_t                  iv_sifs_trigger_time;    /* per VAP sifs trigger interval */
    u_int8_t                   mcast_encrypt_addr[IEEE80211_ADDR_LEN];      /* address for Encrypt_mcast frame */
    ATH_LIST_HEAD(, ieee80211_node) iv_dump_node_list;
    ATH_LIST_HEAD(,ieee80211_mac_filter_list) mac_filter_hash[32];
    u_int8_t                   mac_entries;
    u_int8_t                   rtt_enable;
    u_int8_t                   lci_enable;
    u_int8_t                   lcr_enable;
    u_int32_t                  iv_smart_mesh_cfg; /* smart MESH configuration */
#if MESH_MODE_SUPPORT
    u_int32_t                  mhdr; /* for mesh testing */
    u_int32_t                  mdbg; /* for mesh testing */
    u_int8_t bssid_mesh[IEEE80211_ADDR_LEN];
    u_int32_t                  iv_mesh_cap; /*for mesh vap capabilities */
#endif
#if ATH_SUPPORT_NAC
    struct ieee80211_nac       iv_nac;

    int                       (*iv_neighbour_rx)(struct ieee80211vap *, u_int32_t bssid_idx,
                                          enum ieee80211_nac_param action, enum ieee80211_nac_mactype type,
                                          u_int8_t macaddr[IEEE80211_ADDR_LEN]);
    int				(*iv_neighbour_get_max_addrlimit)(
		    			struct ieee80211vap *,
					enum ieee80211_nac_mactype type);
#endif

#if ATH_DATA_RX_INFO_EN
    uint32_t                   rxinfo_perpkt; /* flag to decide whether to update rxinfo per packet*/
#endif

   /* Configure association WAR for 160 MHz width (i.e. 160/80+80 MHz
    * modes). Some STAs may have an issue associating with us if we advertise
    * 160/80+80 MHz related capabilities in probe response/association response.
    * Hence this WAR suppresses 160/80+80 MHz related information in probe
    * responses and association responses for such STAs.
    * Starting from LSB
    * First bit set        = Default WAR behavior (VHT_OP modified)
    * First+second bit set = (VHT_OP+ VHT_CAP modified)
    * No bit set (default) = WAR disabled
    */
    u_int32_t                  iv_cfg_assoc_war_160w;
    u_int8_t                   iv_rev_sig_160w;      /* Use revised signalling
                                                        for 160/80+80 MHz */
    u_int8_t                   channel_switch_state; /* flag to decide if vap undergoes channel switch*/
    u_int8_t                   iv_disable_ht_tx_amsdu; /* 0-Enable/1-Disable 11n Tx AMSDU */
    u_int8_t                   iv_cts2self_prot_dtim_bcn; /* 0-Disable/1-Enable CTS2SELF protection for DTIM beacons */
    u_int8_t                   iv_enable_vsp; /* 1-Enable/0-Disable VSP */
    u_int8_t                   iv_cfg_raw_dwep_ind;  /* flag to indicate if dummy/null key plumbing is reqd for dynamic WEP */
    struct ieee80211_clone_params cp;
#if UMAC_SUPPORT_ACFG
    u_int32_t                  iv_diag_warn_threshold;
    u_int32_t                  iv_diag_err_threshold;
#endif
    ieee80211_mbo_t            mbo; /* pointer to mbo functionality */
    ieee80211_oce_t            oce; /* pointer to oce functionality */
    u_int32_t                  nbr_scan_period;      /* Neighbor AP scan period in sec */
    u_int32_t                  nbr_scan_ts;          /* Neighbor AP scan timestamp */
    bool                       rnr_enable;           /* enable inclusion of RNR IE in Beacon and Prb-Rsp */
    bool                       rnr_enable_fd;        /* enable inclusion of RNR IE in FILS Discovery */
    bool                       rnr_enable_tbtt;      /* enable calculation of TBTT in RNR IE */
    u_int8_t                   rnr_ap_info[RNR_MAX_SIZE];
    u_int32_t                  rnr_ap_info_len;
    bool                       rnr_ap_info_complete;
    bool                       ap_chan_rpt_enable;   /* enable inclusion of AP Channel Report IE in Beacon and Prb-Rsp */
    ap_chan_rpt_t              ap_chan_rpt[ACR_MAX_ENTRIES];
    u_int32_t                  ap_chan_rpt_cnt;
    u_int64_t                  iv_estimate_tbtt;
    u_int8_t                   iv_no_cac;            /* is cac needed */
#if QCA_LTEU_SUPPORT
    u_int32_t                  scan_probe_spacing_interval; /* Num of intervals in dwell time, at end of interval a prb req is sent */
#endif
    u_int32_t                  watermark_threshold;         /* config parameter to store the threshold value */
    u_int32_t                  watermark_threshold_reached; /* number of times the threshold has been breached */
    u_int8_t                   watermark_threshold_flag;    /* flag to indicate the threshold reached status */
    u_int32_t                  assoc_high_watermark;        /* Max Number of clients has been associated post crossing the threshold */
    u_int32_t                  assoc_high_watermark_time;   /* Time stamp when the max number of clients has been associated */
    MU_CAP_WAR                 iv_mu_cap_war;
    u_int32_t                  iv_cfg_nstscap_war;
    int                        iv_bcn_rate;
    u_int8_t                   iv_csmode;        /* Channel switch mode to be announced */
    u_int8_t                   iv_enable_ecsaie:1, /* enabel/disable ECSA IE */
                               iv_enable_max_ch_sw_time_ie:1; /* enable/disable Maximum Channel Switch IE */
    u_int8_t                   iv_ecsa_opclass;  /* opClass to advertise in ECSA IE */
#if DYNAMIC_BEACON_SUPPORT
#define DEFAULT_DBEACON_RSSI_THRESHOLD  60            /* dBm,  default rssi threshold */
#define DEFAULT_DBEACON_TIMEOUT 30                    /* in secs, timeout of timer*/
    int8_t                     iv_dbeacon_rssi_thr;   /* STA probe req RSSI threshold for Dynamic beacon */
    u_int16_t                  iv_dbeacon_timeout;    /* Duration the vap beacon after STA connected */
    os_timer_t                 iv_dbeacon_suspend_beacon;
    qdf_spinlock_t             iv_dbeacon_lock;
#endif
    u_int8_t                   iv_txpow_mgt_frm[MAX_NUM_TXPOW_MGT_ENTRY];
    u_int8_t                   iv_txpow_frm[MAX_NUM_TXPOW_FRAME_TYPE][MAX_NUM_TXPOW_MGT_ENTRY];
    void                       (*iv_txpow_mgmt)(struct ieee80211vap *,int frame_subtype,u_int8_t transmit_power);
    void                       (*iv_txpow)(struct ieee80211vap *,int frame_type,int frame_subtype,u_int8_t transmit_power);
    u_int32_t                  ald_pid;/* pid per vap for hyfi*/
    qdf_atomic_t               iv_is_start_sent; /* Flag used to serialize vdev_start and vdev_stop commands */
    qdf_atomic_t               iv_restart_pending; /* Flag used to serialize vdev_start and vdev_restart commands */
    qdf_atomic_t               iv_is_start_resp_received; /* Flag used to serialize vdev_start and vdev_restart commands */
    struct wlan_objmgr_vdev    *vdev_obj; /* UMAC vdev object pointer */
    struct vdev_mlme_obj       *vdev_mlme; /* MLME vdev object pointer */
#if UMAC_SUPPORT_CFG80211
    u_int8_t                    iv_cfg80211_create;
    u_int8_t			iv_locally_generated_disconnect;
    u_int8_t                    iv_cfg80211_channel;
    struct cfg80211_chan_def    chandef_notify; /* channel notify data structure */
    qdf_work_t                  cfg80211_channel_notify_workq; /* WORKQ for cfg80211 channel notify event */
#endif
    u_int32_t                  iv_he_ctrl:1,              /* HE Signals control support Enable:1 Disable:0 */
                               iv_he_twtreq:1,            /* HE TWT Request Enable:1 Disable:0 */
                               iv_he_twtres:1,            /* HE TWT Responder Enable:1 Disable:0 */
                               iv_he_frag:2,              /* HE Frag Size - 0,1,2,3 */
                               iv_he_max_frag_msdu:3,     /* HE Max Frag MSDU : 2^n */
                               iv_he_min_frag_size:2,     /* HE Min Frag BA size : 0-128,1-256, 2-512, 3-no restriction */
                               iv_he_multi_tid_aggr:3,    /* HE  Multi TID : Signals multi TID not more than this value +1 */
                               iv_he_link_adapt:2,        /* HE Signals Link Adaptation Enable:1 Disable:0 */
                               iv_he_all_ack:1,           /* HE ALL Ack - Single Bit Signalling  Enable:1 Disable:0 */
                               iv_he_ul_mu_sched:1,       /* HE Signals RU allocation Enable:1 Disable:0 */
                               iv_he_actrl_bsr:1,         /* HE ACtrl - Buffer status report enable:1 Disable:0 */
                               iv_he_bcast_twt:1,         /* HE Bcast TWT Enable:1 Disable:0 */
                               iv_he_32bit_ba:1,          /* HE 32bit BA support Enable:1 Disable:0 */
                               iv_he_mu_cascade:1,        /* HE Signals MU Cascade Enable:1 Disable:0 */
                               iv_he_omi:1,               /* HE Signals OMI  Enable:1 Disable:0 */
                               iv_he_ofdma_ra:1,          /* HE Signals OFDMA Random Access  Enable:1 Disable:0 */
                               iv_he_amsdu_frag:1,        /* HE Signals AMSDU Frag support Enable:1 Disable:0 */
                               iv_he_flex_twt:1,          /* HE Signals Flex TWT Enable:1 Disable:0 */
                               iv_he_bsr_supp:1;          /* HE BSR Support Enable:1 Disable:0 */


    u_int32_t                  iv_he_relaxed_edca:1,      /* HE Signalls EDCA support Enable:1 Disable:0 */
                               iv_he_spatial_reuse:1,     /* HE Signals Spatial Reuse Enable:1 Disable:0 */
                               iv_he_multi_bss:1,         /* HE Multi Bss Enable:1 Disable:0 */
                               iv_he_su_bfee:1,           /* HE SU BFEE */
                               iv_he_su_bfer:1,           /* HE SU BFER */
                               iv_he_mu_bfee:1,           /* HE MU BFEE */
                               iv_he_mu_bfer:1,           /* HE MU BFER */
                               iv_he_ul_muofdma:1,        /* HE UL MU OFDMA */
                               iv_he_dl_muofdma:1,        /* HE DL MU OFDMA */
                               iv_he_ul_mumimo:1,         /* HE DL MU MIMO */
                               iv_he_sounding_mode:4,     /* HE/VHT, SU/MU and Trig/Non-Trig sounding */
                               iv_he_muedca:1;            /* HE MU EDCA */

    u_int32_t                  iv_he_tx_mcsnssmap;        /* HE TX MCS NSS MAP */
    u_int32_t                  iv_he_rx_mcsnssmap;        /* HE RX MCS NSS MAP */

    u_int32_t                  iv_he_default_pe_durn:3,   /* HE Default Pe Durn */
                               iv_he_twt_required:1,      /* HE TWT required  */
                               iv_he_rts_threshold:10,    /* HE RTS threshold duration */
                               iv_he_part_bss_color:1,    /* HE partial BSS color */
                               iv_he_vht_op_present:1,    /* HE VHT OP Info present */
                               iv_he_multiple_bssid_ap:1, /* HE Multiple BSSID AP*/
                               iv_he_tx_bssid_ind:1,      /* HE Transmit BSSID indicator */
                               iv_he_bss_color_dis:1;     /* HE BSS color disabled */

    u_int16_t                  iv_he_extended_range:1,    /* HE Extended Range*/
                               iv_he_dcm:1,               /* HE DCM Coding*/
                               iv_he_ltf:3;               /* HE LTF */
    u_int16_t                  iv_he_ar_gi_ltf;           /* HE Autorate GI LTF */

    bool                       iv_bcn_offload_enable;
    bool                       iv_ext_nss_support;        /* Flag to enable/disable extended NSS support. Referred on TX.
                                                             Support can be enabled only if capability is enabled */
    u_int8_t                   iv_csl_support;            /* Config for CSL*/
    u_int8_t                   iv_lite_monitor;           /* To disable promiscous mode in monitor vap */
    qdf_atomic_t               vap_bcn_event;             /* Set when wmi_bcn_event occurs*/
    bool                       iv_vht_mcs10_11_supp;      /* Enables/Disables overall support for VHT MCS10/11 */
    bool                       iv_vht_mcs10_11_nq2q_peer_supp; /* Enables/Disables support of VHT MCS10/11
                                                           * for non Q2Q peer */
    void (*get_vdev_bcn_stats)(struct ieee80211vap *vap);
    void (*reset_bcn_stats)(struct ieee80211vap *vap);
    bool                       iv_remove_csa_ie; /* Need to remove CSA IE or not*/
    bool                       iv_no_restart; /* No need to restart the vaps */
    bool                       iv_refuse_all_addbas;      /* User configurability to refuse all incoming add block acks */
    u_int8_t                   iv_ba_buffer_size:1;       /* Block Ack Buffer Size */
#if ATH_ACL_SOFTBLOCKING
#define SOFTBLOCK_WAIT_TIME_DEFAULT 10000
#define SOFTBLOCK_WAIT_TIME_MIN 0
#define SOFTBLOCK_WAIT_TIME_MAX 100000
#define SOFTBLOCK_ALLOW_TIME_DEFAULT 15000
#define SOFTBLOCK_ALLOW_TIME_MIN 0
#define SOFTBLOCK_ALLOW_TIME_MAX 100000
    u_int32_t                  iv_softblock_wait_time;  /* wait time in sofblokcing functionality */
    u_int32_t                  iv_softblock_allow_time; /* allow time in softblocking functionality */
#endif
#if QCN_IE
    u_int8_t                   iv_bpr_enable : 1;     /* If set, broadcast probe response is enabled */
    qdf_ktime_t                iv_next_beacon_tstamp; /* Stores the timestamp of the next beacon to be scheduled */
    u_int8_t                   iv_bpr_delay;          /* Holds broadcast probe response time in ms*/
    qdf_hrtimer_data_t      bpr_timer;             /* Holds a hrtimer to send broadcast probe response from a vap */
    u_int16_t                  iv_bpr_timer_start_count; /* Stores the number of times hrtimer restarted */
    u_int16_t                  iv_bpr_timer_resize_count; /* Stores the number of times hrtimer got resized */
    u_int16_t                  iv_bpr_callback_count; /* Stores the number of times broadcast probe response is sent */
    u_int16_t                  iv_bpr_timer_cancel_count; /* Stores the number of times the beacon is sent instead of
                                                             broadcast probe response */
    u_int16_t                  iv_bpr_unicast_resp_count; /* Stores the number of times unicast probe response is sent */
#endif
    u_int16_t                   iv_htflags;            /* HT state flags */
#ifdef QCA_OL_DMS_WAR
    uint8_t                 dms_amsdu_war;              /* Indicate if DMS A-MSDU WAR is required for Multicast packets */
    uint8_t                 iv_me_amsdu;                /* Enable ME with AMSDU encapsulation */
#endif
    uint64_t                wmi_tx_mgmt; /* mgmt frames sent over this vap */
    uint64_t                wmi_tx_mgmt_completions; /* mgmt completions received over this vap */
#if ATH_SUPPORT_NAC_RSSI
    struct ieee80211_nac_rssi       iv_nac_rssi;
    int         (*iv_scan_nac_rssi)(struct ieee80211vap *vap, enum ieee80211_nac_rssi_param cmd,
                                   u_int8_t *, u_int8_t *, u_int8_t );
#endif
    uint64_t                wmi_tx_mgmt_sta; /* mgmt frames sent over this vap using sta peers */
    uint64_t                wmi_tx_mgmt_completions_sta; /* mgmt completions received over this vap using sta peers*/
    u_int8_t                    iv_read_rxprehdr;         /* Rx preheader read enable for packets received on vap dev */
    uint8_t*                 iv_sta_assoc_resp_ie;
    uint16_t                 iv_sta_assoc_resp_len;
    qdf_semaphore_t          iv_sem_lock; /* Lock to serialize mlme_stop_bss and mlme_start_bss */
#if UMAC_SUPPORT_WPA3_STA
    bool                     iv_sta_external_auth_enabled; /* external auth is enabled by supplicant*/
    qdf_timer_t              iv_sta_external_auth_timer;   /* Start timer to track external auth by supplicant */
    u_int8_t                 iv_sae_max_auth_attempts;     /* indicate maximum no.of auth attempts that STA can */
#endif

   /* MBSS IE feature */
   struct ieee80211_mbss    iv_mbss;

#if defined(WLAN_DFS_FULL_OFFLOAD) && defined(QCA_DFS_NOL_OFFLOAD)
   /* to indicate vap start request failed */
   bool                     vap_start_failure;
   /* to indicate whether action was taken for vap start failure*/
   bool                     vap_start_failure_action_taken;
#endif
   /* Enable/disable the advertisement of STA-mode's maximum capabilities instead of it's operating mode in the capability related IEs.
      This flag precedence will be superceded by the ic_enable2GHzHt40Cap in 2.4GHz STA modes */
   u_int8_t                 iv_sta_max_ch_cap;
    uint8_t                iv_rawmode_open_war; /* rawmode open war configuration */
   uint8_t                  beacon_status;
   uint8_t                  restart_txn;
   qdf_atomic_t             kickout_in_progress;
   qdf_atomic_t             stop_action_progress;
   qdf_timer_t              peer_cleanup_timer;
   qdf_timer_t              peer_del_wait_timer;

    qdf_list_t            iv_mgmt_offchan_req_list;
    qdf_spinlock_t        iv_mgmt_offchan_tx_lock;
    qdf_atomic_t          iv_mgmt_offchan_cmpl_pending;
    u_int8_t              iv_dpp_vap_mode;     /* dpp vap flag */
    struct ieee80211_offchan_req iv_mgmt_offchan_current_req;

   u_int8_t                 iv_csa_interop_phy;
   u_int8_t                 iv_csa_interop_bss;
   u_int8_t                 iv_csa_interop_bss_active;
   u_int8_t                 iv_csa_interop_auth;
    uint32_t                iv_pcp_tid_map[IEEE80211_PCP_MAX]; /* pcp-tid map */
    uint8_t                 iv_tidmap_tbl_id; /* tidmap tbl_id in use */
    uint8_t                 iv_tidmap_prty; /* priority order for tid-mapping */
   /* Number of peer delete requests sent on SOC */
   uint64_t                peer_delete_req;
   /* Number of peer delete responsed received on SOC */
    uint64_t                peer_delete_resp;
    bool                     force_cleanup_peers;
   qdf_spinlock_t peer_free_notif_lock;
} IEEE80211VAP, *PIEEE80211VAP;

#if QCN_IE
#define EFF_CHAN_TIME(_chantime, _buffer)  (((_chantime) && (_chantime > _buffer)) ? ((_chantime) - (_buffer)) : (0))
#endif

#define NSTSWAR_IS_VHTCAP_CHANGE(x)         ((x))
/* Bitmap positions which define 160 MHz association WAR actions */
#define ASSOCWAR160_CONFIG_VHT_OP_CHANGE     0x00000001
#define ASSOCWAR160_CONFIG_VHT_OP_CHANGE_S   0
#define ASSOCWAR160_CONFIG_VHT_CAP_CHANGE    0x00000002
#define ASSOCWAR160_CONFIG_VHT_CAP_CHANGE_S  1

/* XXX When more bit definitions are added above, please add to the below master list as well */
#define ASSOCWAR160_CONFIG_VHT_ALL_CHANGES   (ASSOCWAR160_CONFIG_VHT_OP_CHANGE | ASSOCWAR160_CONFIG_VHT_CAP_CHANGE)

#define ASSOCWAR160_IS_VHT_OP_CHANGE(x)     ((x) & (1 << ASSOCWAR160_CONFIG_VHT_OP_CHANGE_S))
#define ASSOCWAR160_IS_DEFAULT_CONFIG(x)    ASSOCWAR160_IS_VHT_OP_CHANGE((x))
#define ASSOCWAR160_IS_VHT_CAP_CHANGE(x)    (ASSOCWAR160_IS_DEFAULT_CONFIG((x)) && \
                                                ((x) & (1 << ASSOCWAR160_CONFIG_VHT_CAP_CHANGE_S)))
#define ASSOCWAR160_IS_VALID_CHANGE(x)      (ASSOCWAR160_IS_DEFAULT_CONFIG((x)) &&  \
                                                (((x) | ASSOCWAR160_CONFIG_VHT_ALL_CHANGES) == \
                                                 ASSOCWAR160_CONFIG_VHT_ALL_CHANGES))

/* Default status of revised signalling for 160/80+80 MHz*/
#define DEFAULT_REV_SIG_160_STATUS          1

/* Smart MESH configuration */
#define SMART_MESH_DWDS 0x01                /* Dynamic WDS */
#define SMART_MESH_ACL_ENHANCEMENT 0x02      /* ACL enhancements */
#define SMART_MESH_80211_EVENTS   0x04       /* 802.11 Event enhancments */
#define SMART_MESH_80211_STATS     0x08      /* 802.11 Station statistics */
#define SMART_MESH_NONCONNETED_STATS 0x10    /* 802.11 frames of non connected clients */

#if MESH_MODE_SUPPORT
/* Mesh VAP capabiltiies */
#define MESH_CAP_BEACON_ENABLED  0x01    /* Enable/Disable Beacon on Mesh VAP */
#define MESH_DEBUG_ENABLED 0x01          /* Enable mesh debug */
#define MESH_DEBUG_DUMP_STATS 0x02       /* Dump rx stats for mesh mode */
#endif

struct global_ic_list {
    spinlock_t    radio_list_lock; /* lock to protect access to radio list */
    struct ieee80211com *global_ic[MAX_RADIO_CNT];
    u_int8_t      num_global_ic;
#if DBDC_REPEATER_SUPPORT
    u_int8_t      num_stavaps_up;
    bool          dbdc_process_enable;
    bool          is_dbdc_rootAP;
    bool          force_client_mcast_traffic; /* Set this flag when DBDC Repeater wants to send
                                               * MCAST traffic of connected client to RootAP on corresponding STA VAP
                                               */
    u_int8_t       delay_stavap_connection;
    u_int16_t      disconnect_timeout; /* Interface manager app uses this timeout to bring down AP VAPs after STAVAP disconnection*/
    u_int16_t      reconfiguration_timeout;  /* Interface manager app uses this timeout to bring down AP VAPs after STAVAP disconnection*/
    bool          iface_mgr_up;
    bool          always_primary;
    bool          drop_secondary_mcast;
    bool          same_ssid_support;
    struct ieee80211vap     *max_priority_stavap_up;    /* sta vap pointer of connected radio having higher priority*/
    u_int8_t      num_l2uf_retries;
    u_int8_t      num_fast_lane_ic;
    u_int8_t      num_rptr_clients;
    u_int8_t      extender_info;  /* 0x00->no stavap connection and RootAP access on Repeater AP
                                     0x0F->invalid state
                                     0xF0->stavap connection is there, but no rootap access on Repeater AP
                                     0xFF->stavap connection and rootap access is there on Repeater AP
                                   */
    u_int8_t      preferred_list_stavap[MAX_RADIO_CNT][IEEE80211_ADDR_LEN];
                                  /*Mac list of RE to which this AP has to connect*/
    u_int8_t      denied_list_apvap[MAX_RADIO_CNT][IEEE80211_ADDR_LEN];
                                  /*Mac list of RE STAvaps to which AP should deauth*/
    u_int8_t      ap_preferrence;
                                 /* 0 - No preferrence
                                    1 - connect to RootAP
                                    2 - connect to RE, whose bssid matches with preferred_list_stavap
                                  */
    systime_t     rootap_access_downtime;
#endif
};

#ifndef __ubicom32__
#define IEEE80211_ADDR_EQ(a1,a2)        (OS_MEMCMP(a1, a2, IEEE80211_ADDR_LEN) == 0)
#define IEEE80211_ADDR_COPY(dst,src)    OS_MEMCPY(dst, src, IEEE80211_ADDR_LEN)
#else
#define IEEE80211_ADDR_EQ(a1,a2)        (OS_MACCMP(a1, a2) == 0)
#define IEEE80211_ADDR_COPY(dst,src)    OS_MACCPY(dst, src)
#endif
#define IEEE80211_ADDR_IS_VALID(a)      (a[0]!=0 || a[1]!=0 ||a[2]!= 0 || a[3]!=0 || a[4]!=0 || a[5]!=0)
#define IEEE80211_SSID_IE_EQ(a1,a2)     (((((char*) a1)[1]) == (((char*) a2)[1])) && (OS_MEMCMP(a1, a2, 2+(((char*) a1)[1])) == 0))

/* ic_flags */
#define IEEE80211_F_FF                   0x00000001  /* CONF: ATH FF enabled */
#define IEEE80211_F_TURBOP               0x00000002  /* CONF: ATH Turbo enabled*/
#define IEEE80211_F_PROMISC              0x00000004  /* STATUS: promiscuous mode */
#define IEEE80211_F_ALLMULTI             0x00000008  /* STATUS: all multicast mode */
/* NB: this is intentionally setup to be IEEE80211_CAPINFO_PRIVACY */
#define IEEE80211_F_PRIVACY              0x00000010  /* CONF: privacy enabled */
#define IEEE80211_F_PUREG                0x00000020  /* CONF: 11g w/o 11b sta's */
#define IEEE80211_F_SCAN                 0x00000080  /* STATUS: scanning */
#define IEEE80211_F_SIBSS                0x00000200  /* STATUS: start IBSS */
/* NB: this is intentionally setup to be IEEE80211_CAPINFO_SHORT_SLOTTIME */
#define IEEE80211_F_SHSLOT               0x00000400  /* STATUS: use short slot time*/
#define IEEE80211_F_PMGTON               0x00000800  /* CONF: Power mgmt enable */
#define IEEE80211_F_DESBSSID             0x00001000  /* CONF: des_bssid is set */
#define IEEE80211_F_DFS_CHANSWITCH_PENDING 0x00002000  /* STATUS: channel switch event pending after DFS RADAR */
#define IEEE80211_F_BGSCAN               0x00004000  /* CONF: bg scan enabled */
#define IEEE80211_F_SWRETRY              0x00008000  /* CONF: sw tx retry enabled */
#define IEEE80211_F_TXPOW_FIXED          0x00010000  /* TX Power: fixed rate */
#define IEEE80211_F_IBSSON               0x00020000  /* CONF: IBSS creation enable */
#define IEEE80211_F_SHPREAMBLE           0x00040000  /* STATUS: use short preamble */
#define IEEE80211_F_DATAPAD              0x00080000  /* CONF: do alignment pad */
#define IEEE80211_F_USEPROT              0x00100000  /* STATUS: protection enabled */
#define IEEE80211_F_USEBARKER            0x00200000  /* STATUS: use barker preamble*/
#define IEEE80211_F_TIMUPDATE            0x00400000  /* STATUS: update beacon tim */
#define IEEE80211_F_WPA1                 0x00800000  /* CONF: WPA enabled */
#define IEEE80211_F_WPA2                 0x01000000  /* CONF: WPA2 enabled */
#define IEEE80211_F_WPA                  0x01800000  /* CONF: WPA/WPA2 enabled */
#define IEEE80211_F_DROPUNENC            0x02000000  /* CONF: drop unencrypted */
#define IEEE80211_F_COUNTERM             0x04000000  /* CONF: TKIP countermeasures */
#define IEEE80211_F_HIDESSID             0x08000000  /* CONF: hide SSID in beacon */
#define IEEE80211_F_NOBRIDGE             0x10000000  /* CONF: disable internal bridge */
#define IEEE80211_F_WMEUPDATE            0x20000000  /* STATUS: update beacon wme */
#define IEEE80211_F_COEXT_DISABLE        0x40000000  /* CONF: DISABLE 2040 coexistance */
#define IEEE80211_F_CHANSWITCH           0x80000000  /* force chanswitch */

/* ic_flags_ext and/or iv_flags_ext */
#define IEEE80211_FEXT_WDS                 0x00000001  /* CONF: 4 addr allowed */
#define IEEE80211_FEXT_COUNTRYIE           0x00000002  /* CONF: enable country IE */
#define IEEE80211_FEXT_SCAN_PENDING        0x00000004  /* STATE: scan pending */
#define IEEE80211_FEXT_BGSCAN              0x00000008  /* STATE: enable full bgscan completion */
#define IEEE80211_FEXT_UAPSD               0x00000010  /* CONF: enable U-APSD */
#define IEEE80211_FEXT_SLEEP               0x00000020  /* STATUS: sleeping */
#define IEEE80211_FEXT_EOSPDROP            0x00000040  /* drop uapsd EOSP frames for test */
#define IEEE80211_FEXT_MARKDFS             0x00000080  /* Enable marking of dfs interfernce */
#define IEEE80211_FEXT_REGCLASS            0x00000100  /* CONF: send regclassids in country ie */
#define IEEE80211_FEXT_BLKDFSCHAN          0x00000200  /* CONF: block the use of DFS channels */
#define IEEE80211_FEXT_CCMPSW_ENCDEC       0x00000400 /* enable or disable s/w ccmp encrypt decrypt support */
#define IEEE80211_FEXT_HIBERNATION         0x00000800  /* STATE: hibernating */
#define IEEE80211_FEXT_SAFEMODE            0x00001000  /* CONF: MSFT safe mode         */
#define IEEE80211_FEXT_DESCOUNTRY          0x00002000  /* CONF: desired country has been set */
#define IEEE80211_FEXT_PWRCNSTRIE          0x00004000  /* CONF: enable power capability or contraint IE */
#define IEEE80211_FEXT_DOT11D              0x00008000  /* STATUS: 11D in used */
#define IEEE80211_FEXT_RADAR               0x00010000  /* STATUS: 11D channel-switch detected */
#define IEEE80211_FEXT_AMPDU               0x00020000  /* CONF: A-MPDU supported */
#define IEEE80211_FEXT_AMSDU               0x00040000  /* CONF: A-MSDU supported */
#define IEEE80211_FEXT_HTPROT              0x00080000  /* CONF: HT traffic protected */
#define IEEE80211_FEXT_RESET               0x00100000  /* CONF: Reset once */
#define IEEE80211_FEXT_APPIE_UPDATE        0x00200000  /* STATE: beacon APP IE updated */
#define IEEE80211_FEXT_IGNORE_11D_BEACON   0x00400000  /* CONF: ignore 11d beacon */
#define IEEE80211_FEXT_WDS_AUTODETECT      0x00800000  /* CONF: WDS auto Detect/DELBA */
#define IEEE80211_FEXT_PUREB               0x01000000  /* 11b only without 11g stations */
#define IEEE80211_FEXT_HTRATES             0x02000000  /* disable HT rates */
#define IEEE80211_FEXT_HTVIE               0x04000000  /* HT CAP IE present */
//#define IEEE80211_FEXT_DUPIE             0x08000000  /* dupie (ANA,pre ANA ) */
#define IEEE80211_FEXT_AP                  0x08000000  /* Extender AP */
#define IEEE80211_FEXT_DELIVER_80211       0x10000000  /* CONF: deliver rx frames with 802.11 header */
#define IEEE80211_FEXT_SEND_80211          0x20000000  /* CONF: os sends down tx frames with 802.11 header */
#define IEEE80211_FEXT_WDS_STATIC          0x40000000  /* CONF: statically configured WDS */
#define IEEE80211_FEXT_PURE11N             0x80000000  /* CONF: pure 11n mode */

/* ic_flags_ext2 */
#define IEEE80211_FEXT2_CSA_WAIT 0x00000001 /* radio in middle of CSA */
#define IEEE80211_FEXT2_RESET_PRECACLIST   0x00000002  /* reset precac list */

/* iv_flags_ext2 */
#define IEEE80211_FEXT2_PURE11AC           0x00000001  /* CONF: pure 11ac mode */
#define IEEE80211_FEXT2_BR_UPDATE           0x00000002  /* Basic Rates Update */
#define IEEE80211_FEXT2_STRICT_BW           0x00000004  /* CONF: restrict bw ont top of per 11ac/n */
#if ATH_NON_BEACON_AP
#define IEEE80211_FEXT2_NON_BEACON          0x00000008  /*non-beaconing AP VAP*/
#endif
#define IEEE80211_FEXT2_SON                0x00000010  /* Wi-Fi SON mode (with APS) */
#define IEEE80211_FEXT2_MBO                0x00000020  /* MBO */
#define IEEE80211_FEXT2_AP_INFO_UPDATE     0x00000040  /* Wi-Fi WHC AP info update */
#define IEEE80211_FEXT2_BACKHAUL           0x00000080
#define IEEE80211_FEXT2_SON_SPL_RPT         0x00000100 /* SON SPCL Repeate mode */
#define IEEE80211_FEXT2_FILS               0x00000200
#define IEEE80211_FEXT2_NOCABQ             0x00000200  /* Don't buffer multicast when STA in ps mode */
#define IEEE80211_FEXT2_MBSS_VAP           0x00000400  /* MBSS VAP */
#define IEEE80211_FEXT2_NON_TRANSMIT_VAP   0x00000800  /* MBSS Non-transmitting VAP */
#define IEEE80211_FEXT2_MBSS_MASK          0x00000C00

/* ic_caps */
#define IEEE80211_C_WEP                  0x00000001  /* CAPABILITY: WEP available */
#define IEEE80211_C_TKIP                 0x00000002  /* CAPABILITY: TKIP available */
#define IEEE80211_C_AES                  0x00000004  /* CAPABILITY: AES OCB avail */
#define IEEE80211_C_AES_CCM              0x00000008  /* CAPABILITY: AES CCM avail */
#define IEEE80211_C_HT                   0x00000010  /* CAPABILITY: 11n HT available */
#define IEEE80211_C_CKIP                 0x00000020  /* CAPABILITY: CKIP available */
#define IEEE80211_C_FF                   0x00000040  /* CAPABILITY: ATH FF avail */
#define IEEE80211_C_TURBOP               0x00000080  /* CAPABILITY: ATH Turbo avail*/
#define IEEE80211_C_IBSS                 0x00000100  /* CAPABILITY: IBSS available */
#define IEEE80211_C_PMGT                 0x00000200  /* CAPABILITY: Power mgmt */
#define IEEE80211_C_HOSTAP               0x00000400  /* CAPABILITY: HOSTAP avail */
#define IEEE80211_C_AHDEMO               0x00000800  /* CAPABILITY: Old Adhoc Demo */
#define IEEE80211_C_SWRETRY              0x00001000  /* CAPABILITY: sw tx retry */
#define IEEE80211_C_TXPMGT               0x00002000  /* CAPABILITY: tx power mgmt */
#define IEEE80211_C_SHSLOT               0x00004000  /* CAPABILITY: short slottime */
#define IEEE80211_C_SHPREAMBLE           0x00008000  /* CAPABILITY: short preamble */
#define IEEE80211_C_MONITOR              0x00010000  /* CAPABILITY: monitor mode */
#define IEEE80211_C_TKIPMIC              0x00020000  /* CAPABILITY: TKIP MIC avail */
#define IEEE80211_C_WAPI                 0x00040000  /* CAPABILITY: ATH WAPI avail */
#define IEEE80211_C_WDS_AUTODETECT       0x00080000  /* CONF: WDS auto Detect/DELBA */
#define IEEE80211_C_WPA1                 0x00800000  /* CAPABILITY: WPA1 avail */
#define IEEE80211_C_WPA2                 0x01000000  /* CAPABILITY: WPA2 avail */
#define IEEE80211_C_WPA                  0x01800000  /* CAPABILITY: WPA1+WPA2 avail*/
#define IEEE80211_C_BURST                0x02000000  /* CAPABILITY: frame bursting */
#define IEEE80211_C_WME                  0x04000000  /* CAPABILITY: WME avail */
#define IEEE80211_C_WDS                  0x08000000  /* CAPABILITY: 4-addr support */
#define IEEE80211_C_WME_TKIPMIC          0x10000000  /* CAPABILITY: TKIP MIC for QoS frame */
#define IEEE80211_C_BGSCAN               0x20000000  /* CAPABILITY: bg scanning */
#define IEEE80211_C_UAPSD                0x40000000  /* CAPABILITY: UAPSD */
#define IEEE80211_C_DOTH                 0x80000000  /* CAPABILITY: enabled 11.h */

/* XXX protection/barker? */

#define IEEE80211_C_CRYPTO         0x0000002f  /* CAPABILITY: crypto alg's */

/* ic_caps_ext */
#define IEEE80211_CEXT_FASTCC      0x00000001  /* CAPABILITY: fast channel change */
#define IEEE80211_CEXT_P2P              0x00000002  /* CAPABILITY: P2P */
#define IEEE80211_CEXT_MULTICHAN        0x00000004  /* CAPABILITY: Multi-Channel Operations */
#define IEEE80211_CEXT_PERF_PWR_OFLD    0x00000008  /* CAPABILITY: the device supports perf and power offload */
#define IEEE80211_CEXT_11AC             0x00000010  /* CAPABILITY: the device supports 11ac */
#define IEEE80211_ACS_CHANNEL_HOPPING   0x00000020  /* CAPABILITY: the device support acs channel hopping */

#if ATH_SUPPORT_DFS && ATH_SUPPORT_STA_DFS
#define IEEE80211_CEXT_STADFS           0x00000040  /* CAPABILITY: the device support STA DFS */
#endif

/* ATF Schedular Flags */
#define IEEE80211_ATF_SCHED_STRICT          0x00000001      /* 1 - Strict, 0 - Fair */
#define IEEE80211_ATF_SCHED_OBSS            0x00000002
#define IEEE80211_ATF_GROUP_SCHED_POLICY    0x00000004

/* REPEATER channel_switch(CSH) options flags */

/* random channel selection
 * 1 - only from nonDFS channel list
 * 0 - all(dfs and nonDFS) channel list
 */
#define IEEE80211_CSH_OPT_NONDFS_RANDOM    0x00000001

/* If the CSA channel is DFS
 * 1 - ignore the CSA channel and choose a Random  channel locally
 * 0 - use the CSA channel
 */
#define IEEE80211_CSH_OPT_IGNORE_CSA_DFS   0x00000002

/* When STA brings up the AP VAP
 * 1 - CAC is done
 * 0 - CAC is not done
 */
#define IEEE80211_CSH_OPT_CAC_APUP_BYSTA   0x00000004

/* When STA brings up the AP VAP
 * 1 - CSA is sent
 * 0 - CSA is not sent
 */
#define IEEE80211_CSH_OPT_CSA_APUP_BYSTA   0x00000008

/* When Radar gets detected by REPEATER
 *  RCSA = Reverse CSA: A frame or a set of frames
 *  sent by REPEATER to its parent REPEATER when
 *  either 1) REPEATER detects RADAR
 *  or     2) REPEATER receives a RCSA from its child REPEATER
 *
 * 1 - RCSA is sent
 * 0 - RCSA is not sent
 */
#define IEEE80211_CSH_OPT_RCSA_TO_UPLINK   0x00000010

/* When RCSA is receives by Root/REPEATER
 * 1 - RCSA is processed
 * 0 - RCSA is not processed
 */
#define IEEE80211_CSH_OPT_PROCESS_RCSA     0x00000020

#define IEEE80211_CSH_OPT_APRIORI_NEXT_CHANNEL 0x00000040

/* When bringing up the Repeater AP vaps, skip the CAC, if the RootAP
 * HT20 DFS sub-channels are subset of the RepeaterAP HT20 sub-channels.
 */
#define IEEE80211_CSH_OPT_AVOID_DUAL_CAC    0x00000080

/* Channel change happens through user space (ucfg_set_freq/doth_chanswitch)
 */
#define IEEE80211_CHANCHANGE_STARTED        0x00000001

/* Channel change happens through beacon_update().
 */
#define IEEE80211_CHANCHANGE_BY_BEACONUPDATE     0x00000002

/* Channel change by dfs_task()
 */
#define IEEE80211_CHANCHANGE_MARKRADAR       0x00000004


#define IEEE80211_IS_CSH_NONDFS_RANDOM_ENABLED(_ic)   ((_ic)->ic_chanswitch_flags &   IEEE80211_CSH_OPT_NONDFS_RANDOM)
#define IEEE80211_CSH_NONDFS_RANDOM_ENABLE(_ic)       ((_ic)->ic_chanswitch_flags |=  IEEE80211_CSH_OPT_NONDFS_RANDOM)
#define IEEE80211_CSH_NONDFS_RANDOM_DISABLE(_ic)      ((_ic)->ic_chanswitch_flags &= ~IEEE80211_CSH_OPT_NONDFS_RANDOM)

#define IEEE80211_IS_CSH_IGNORE_CSA_DFS_ENABLED(_ic)  ((_ic)->ic_chanswitch_flags &   IEEE80211_CSH_OPT_IGNORE_CSA_DFS)
#define IEEE80211_CSH_IGNORE_CSA_DFS_ENABLE(_ic)      ((_ic)->ic_chanswitch_flags |=  IEEE80211_CSH_OPT_IGNORE_CSA_DFS)
#define IEEE80211_CSH_IGNORE_CSA_DFS_DISABLE(_ic)     ((_ic)->ic_chanswitch_flags &= ~IEEE80211_CSH_OPT_IGNORE_CSA_DFS)

#define IEEE80211_IS_CSH_CAC_APUP_BYSTA_ENABLED(_ic)  ((_ic)->ic_chanswitch_flags &   IEEE80211_CSH_OPT_CAC_APUP_BYSTA)
#define IEEE80211_CSH_CAC_APUP_BYSTA_ENABLE(_ic)      ((_ic)->ic_chanswitch_flags |=  IEEE80211_CSH_OPT_CAC_APUP_BYSTA)
#define IEEE80211_CSH_CAC_APUP_BYSTA_DISABLE(_ic)     ((_ic)->ic_chanswitch_flags &= ~IEEE80211_CSH_OPT_CAC_APUP_BYSTA)

#define IEEE80211_IS_CSH_CSA_APUP_BYSTA_ENABLED(_ic)  ((_ic)->ic_chanswitch_flags &   IEEE80211_CSH_OPT_CSA_APUP_BYSTA)
#define IEEE80211_CSH_CSA_APUP_BYSTA_ENABLE(_ic)      ((_ic)->ic_chanswitch_flags |=  IEEE80211_CSH_OPT_CSA_APUP_BYSTA)
#define IEEE80211_CSH_CSA_APUP_BYSTA_DISABLE(_ic)     ((_ic)->ic_chanswitch_flags &= ~IEEE80211_CSH_OPT_CSA_APUP_BYSTA)


#define IEEE80211_IS_CSH_RCSA_TO_UPLINK_ENABLED(_ic)  ((_ic)->ic_chanswitch_flags &   IEEE80211_CSH_OPT_RCSA_TO_UPLINK)
#define IEEE80211_CSH_RCSA_TO_UPLINK_ENABLE(_ic)      ((_ic)->ic_chanswitch_flags |=  IEEE80211_CSH_OPT_RCSA_TO_UPLINK)
#define IEEE80211_CSH_RCSA_TO_UPLINK_DISABLE(_ic)     ((_ic)->ic_chanswitch_flags &= ~IEEE80211_CSH_OPT_RCSA_TO_UPLINK)

#define IEEE80211_IS_CSH_PROCESS_RCSA_ENABLED(_ic)    ((_ic)->ic_chanswitch_flags &   IEEE80211_CSH_OPT_PROCESS_RCSA)
#define IEEE80211_CSH_PROCESS_RCSA_ENABLE(_ic)        ((_ic)->ic_chanswitch_flags |=  IEEE80211_CSH_OPT_PROCESS_RCSA)
#define IEEE80211_CSH_PROCESS_RCSA_DISABLE(_ic)       ((_ic)->ic_chanswitch_flags &= ~IEEE80211_CSH_OPT_PROCESS_RCSA)

#define IEEE80211_IS_CSH_OPT_APRIORI_NEXT_CHANNEL_ENABLED(_ic)  ((_ic)->ic_chanswitch_flags &   IEEE80211_CSH_OPT_APRIORI_NEXT_CHANNEL)
#define IEEE80211_CSH_OPT_APRIORI_NEXT_CHANNEL_ENABLE(_ic)      ((_ic)->ic_chanswitch_flags |=  IEEE80211_CSH_OPT_APRIORI_NEXT_CHANNEL)
#define IEEE80211_CSH_OPT_APRIORI_NEXT_CHANNEL_DISABLE(_ic)     ((_ic)->ic_chanswitch_flags &= ~IEEE80211_CSH_OPT_APRIORI_NEXT_CHANNEL)

#define IEEE80211_IS_CSH_OPT_AVOID_DUAL_CAC_ENABLED(_ic)        ((_ic)->ic_chanswitch_flags &   IEEE80211_CSH_OPT_AVOID_DUAL_CAC)
#define IEEE80211_CSH_OPT_AVOID_DUAL_CAC_ENABLE(_ic)            ((_ic)->ic_chanswitch_flags |=  IEEE80211_CSH_OPT_AVOID_DUAL_CAC)
#define IEEE80211_CSH_OPT_AVOID_DUAL_CAC_DISABLE(_ic)           ((_ic)->ic_chanswitch_flags &= ~IEEE80211_CSH_OPT_AVOID_DUAL_CAC)

#define IEEE80211_CHANCHANGE_STARTED_IS_SET(_ic)                ((_ic)->ic_chanchange_serialization_flags &   IEEE80211_CHANCHANGE_STARTED)
#define IEEE80211_CHANCHANGE_STARTED_SET(_ic)                   ((_ic)->ic_chanchange_serialization_flags |=  IEEE80211_CHANCHANGE_STARTED)
#define IEEE80211_CHANCHANGE_STARTED_CLEAR(_ic)                 ((_ic)->ic_chanchange_serialization_flags &= ~IEEE80211_CHANCHANGE_STARTED)

#define IEEE80211_CHANCHANGE_BY_BEACONUPDATE_IS_SET(_ic)        ((_ic)->ic_chanchange_serialization_flags &   IEEE80211_CHANCHANGE_BY_BEACONUPDATE)
#define IEEE80211_CHANCHANGE_BY_BEACONUPDATE_SET(_ic)           ((_ic)->ic_chanchange_serialization_flags |=  IEEE80211_CHANCHANGE_BY_BEACONUPDATE)
#define IEEE80211_CHANCHANGE_BY_BEACONUPDATE_CLEAR(_ic)         ((_ic)->ic_chanchange_serialization_flags &= ~IEEE80211_CHANCHANGE_BY_BEACONUPDATE)

#define IEEE80211_CHANCHANGE_MARKRADAR_IS_SET(_ic)              ((_ic)->ic_chanchange_serialization_flags &   IEEE80211_CHANCHANGE_MARKRADAR)
#define IEEE80211_CHANCHANGE_MARKRADAR_SET(_ic)                 ((_ic)->ic_chanchange_serialization_flags |=  IEEE80211_CHANCHANGE_MARKRADAR)
#define IEEE80211_CHANCHANGE_MARKRADAR_CLEAR(_ic)               ((_ic)->ic_chanchange_serialization_flags &= ~IEEE80211_CHANCHANGE_MARKRADAR)

/* Accessor APIs */
#define IEEE80211_FEXT_MARKDFS_ENABLE(_ic)          ((_ic)->ic_flags_ext |= IEEE80211_FEXT_MARKDFS)

#define IEEE80211_IS_UAPSD_ENABLED(_ic)             ((_ic)->ic_flags_ext & IEEE80211_FEXT_UAPSD)
#define IEEE80211_UAPSD_ENABLE(_ic)                 (ieee80211com_set_flags_ext((_ic), IEEE80211_FEXT_UAPSD))
#define IEEE80211_UAPSD_DISABLE(_ic)                (ieee80211com_clear_flags_ext((_ic), IEEE80211_FEXT_UAPSD))

#define IEEE80211_IS_SLEEPING(_ic)                  ((_ic)->ic_flags_ext & IEEE80211_FEXT_SLEEP)
#define IEEE80211_GOTOSLEEP(_ic)                    (ieee80211com_set_flags_ext((_ic), IEEE80211_FEXT_SLEEP))
#define IEEE80211_WAKEUP(_ic)                       (ieee80211com_clear_flags_ext((_ic), IEEE80211_FEXT_SLEEP))

#define IEEE80211_IS_HIBERNATING(_ic)               ((_ic)->ic_flags_ext & IEEE80211_FEXT_HIBERNATION)
#define IEEE80211_GOTOHIBERNATION(_ic)              (ieee80211com_set_flags_ext((_ic), IEEE80211_FEXT_HIBERNATION))
#define IEEE80211_WAKEUPFROMHIBERNATION(_ic)        (ieee80211com_clear_flags_ext((_ic), IEEE80211_FEXT_HIBERNATION))

#define IEEE80211_IS_PROTECTION_ENABLED(_ic)        ((_ic)->ic_flags & IEEE80211_F_USEPROT)
#define IEEE80211_ENABLE_PROTECTION(_ic)            (ieee80211com_set_flags((_ic), IEEE80211_F_USEPROT))
#define IEEE80211_DISABLE_PROTECTION(_ic)           (ieee80211com_clear_flags((_ic), IEEE80211_F_USEPROT))

#define IEEE80211_IS_SHPREAMBLE_ENABLED(_ic)        ((_ic)->ic_flags & IEEE80211_F_SHPREAMBLE)
#define IEEE80211_ENABLE_SHPREAMBLE(_ic)            (ieee80211com_set_flags((_ic), IEEE80211_F_SHPREAMBLE))
#define IEEE80211_DISABLE_SHPREAMBLE(_ic)           (ieee80211com_clear_flags((_ic), IEEE80211_F_SHPREAMBLE))

#define IEEE80211_IS_CAP_SHPREAMBLE_ENABLED(_ic)    ((_ic)->ic_caps & IEEE80211_C_SHPREAMBLE)
#define IEEE80211_ENABLE_CAP_SHPREAMBLE(_ic)        (ieee80211com_set_cap((_ic), IEEE80211_C_SHPREAMBLE))
#define IEEE80211_DISABLE_CAP_SHPREAMBLE(_ic)       (ieee80211com_clear_cap((_ic), IEEE80211_C_SHPREAMBLE))


#define IEEE80211_IS_BARKER_ENABLED(_ic)            ((_ic)->ic_flags & IEEE80211_F_USEBARKER)
#define IEEE80211_ENABLE_BARKER(_ic)                (ieee80211com_set_flags((_ic), IEEE80211_F_USEBARKER))
#define IEEE80211_DISABLE_BARKER(_ic)               (ieee80211com_clear_flags((_ic), IEEE80211_F_USEBARKER))

#define IEEE80211_IS_SHSLOT_ENABLED(_ic)            ((_ic)->ic_flags & IEEE80211_F_SHSLOT)
#define IEEE80211_ENABLE_SHSLOT(_ic)                (ieee80211com_set_flags((_ic), IEEE80211_F_SHSLOT))
#define IEEE80211_DISABLE_SHSLOT(_ic)               (ieee80211com_clear_flags((_ic), IEEE80211_F_SHSLOT))

#define IEEE80211_IS_DATAPAD_ENABLED(_ic)           ((_ic)->ic_flags & IEEE80211_F_DATAPAD)
#define IEEE80211_ENABLE_DATAPAD(_ic)               (ieee80211com_set_flags((_ic), IEEE80211_F_DATAPAD))
#define IEEE80211_DISABLE_DATAPAD(_ic)              (ieee80211com_clear_flags((_ic), IEEE80211_F_DATAPAD))

#define IEEE80211_COEXT_DISABLE(_ic)                (ieee80211com_set_flags((_ic), IEEE80211_F_COEXT_DISABLE))

#define IEEE80211_IS_COUNTRYIE_ENABLED(_ic)         ((_ic)->ic_flags_ext & IEEE80211_FEXT_COUNTRYIE)
#define IEEE80211_ENABLE_COUNTRYIE(_ic)             (ieee80211com_set_flags_ext((_ic), IEEE80211_FEXT_COUNTRYIE))
#define IEEE80211_DISABLE_COUNTRYIE(_ic)            (ieee80211com_clear_flags_ext((_ic), IEEE80211_FEXT_COUNTRYIE))

#define IEEE80211_IS_11D_ENABLED(_ic)               ((_ic)->ic_flags_ext & IEEE80211_FEXT_DOT11D)
#define IEEE80211_ENABLE_11D(_ic)                   (ieee80211com_set_flags_ext((_ic), IEEE80211_FEXT_DOT11D))
#define IEEE80211_DISABLE_11D(_ic)                  (ieee80211com_clear_flags_ext((_ic), IEEE80211_FEXT_DOT11D))

#define IEEE80211_HAS_DESIRED_COUNTRY(_ic)          ((_ic)->ic_flags_ext & IEEE80211_FEXT_DESCOUNTRY)
#define IEEE80211_SET_DESIRED_COUNTRY(_ic)          (ieee80211com_set_flags_ext((_ic), IEEE80211_FEXT_DESCOUNTRY))
#define IEEE80211_CLEAR_DESIRED_COUNTRY(_ic)        (ieee80211com_clear_flags_ext((_ic), IEEE80211_FEXT_DESCOUNTRY))

#define IEEE80211_IS_11D_BEACON_IGNORED(_ic)        ((_ic)->ic_flags_ext & IEEE80211_FEXT_IGNORE_11D_BEACON)
#define IEEE80211_ENABLE_IGNORE_11D_BEACON(_ic)     (ieee80211com_set_flags_ext((_ic), IEEE80211_FEXT_IGNORE_11D_BEACON))
#define IEEE80211_DISABLE_IGNORE_11D_BEACON(_ic)    (ieee80211com_clear_flags_ext((_ic), IEEE80211_FEXT_IGNORE_11D_BEACON))

#define IEEE80211_IS_RADAR_ENABLED(_ic)             ((_ic)->ic_flags_ext & IEEE80211_FEXT_RADAR)
#define IEEE80211_ENABLE_RADAR(_ic)                 (ieee80211com_set_flags_ext((_ic), IEEE80211_FEXT_RADAR))
#define IEEE80211_DISABLE_RADAR(_ic)                (ieee80211com_clear_flags_ext((_ic), IEEE80211_FEXT_RADAR))

#define IEEE80211_IS_CHAN_SWITCH_STARTED(_ic)       IEEE80211_IS_RADAR_ENABLED(_ic)
#define IEEE80211_CHAN_SWITCH_START(_ic)            IEEE80211_ENABLE_RADAR(_ic)
#define IEEE80211_CHAN_SWITCH_END(_ic)              IEEE80211_DISABLE_RADAR(_ic)

#define IEEE80211_IS_HTVIE_ENABLED(_ic)             ((_ic)->ic_flags_ext & IEEE80211_FEXT_HTVIE)
#define IEEE80211_ENABLE_HTVIE(_ic)                 (ieee80211com_set_flags_ext((_ic), IEEE80211_FEXT_HTVIE))
#define IEEE80211_DISABLE_HTVIE(_ic)                (ieee80211com_clear_flags_ext((_ic), IEEE80211_FEXT_HTVIE))

#define IEEE80211_IS_AMPDU_ENABLED(_ic)             ((_ic)->ic_flags_ext & IEEE80211_FEXT_AMPDU)
#define IEEE80211_ENABLE_AMPDU(_ic)                 (ieee80211com_set_flags_ext((_ic), IEEE80211_FEXT_AMPDU))
#define IEEE80211_DISABLE_AMPDU(_ic)                (ieee80211com_clear_flags_ext((_ic), IEEE80211_FEXT_AMPDU))

#define IEEE80211_IS_HTPROT_ENABLED(_ic)             ((_ic)->ic_flags_ext & IEEE80211_FEXT_HTPROT)
#define IEEE80211_ENABLE_HTPROT(_ic)                 (ieee80211com_set_flags_ext((_ic), IEEE80211_FEXT_HTPROT))
#define IEEE80211_DISABLE_HTPROT(_ic)                (ieee80211com_clear_flags_ext((_ic), IEEE80211_FEXT_HTPROT))

#define IEEE80211_GET_BCAST_ADDR(_c)                ((_c)->ic_broadcast)

#define IEEE80211_STATE_LOCK_INIT(_ic)             spin_lock_init(&(_ic)->ic_state_lock)
#define IEEE80211_STATE_LOCK_DESTROY(_ic)
#define IEEE80211_STATE_LOCK(_ic)                  spin_lock_dpc(&(_ic)->ic_state_lock)
#define IEEE80211_STATE_UNLOCK(_ic)                spin_unlock_dpc(&(_ic)->ic_state_lock)


#define IEEE80211_COMM_LOCK(_ic)                   spin_lock(&(_ic)->ic_lock)
#define IEEE80211_COMM_UNLOCK(_ic)                 spin_unlock(&(_ic)->ic_lock)
#define IEEE80211_COMM_LOCK_BH(_ic)                spin_lock_bh(&(_ic)->ic_lock)
#define IEEE80211_COMM_UNLOCK_BH(_ic)              spin_unlock_bh(&(_ic)->ic_lock)
#define IEEE80211_COMM_LOCK_IRQ(_ic)               spin_lock_irqsave(&(_ic)->ic_lock,(_ic)->ic_lock_flags)
#define IEEE80211_COMM_UNLOCK_IRQ(_ic)             spin_unlock_irqrestore(&(_ic)->ic_lock,(_ic)->ic_lock_flags)

#define IEEE80211_STATE_CHECK_LOCK(_ic)            spin_lock(&(_ic)->ic_state_check_lock)
#define IEEE80211_STATE_CHECK_UNLOCK(_ic)          spin_unlock(&(_ic)->ic_state_check_lock)

#if ATH_BEACON_DEFERRED_PROC
#define STA_VAP_DOWNUP_LOCK(_ic)                   spin_lock_bh(&(_ic)->ic_main_sta_lock)
#define STA_VAP_DOWNUP_UNLOCK(_ic)                 spin_unlock_bh(&(_ic)->ic_main_sta_lock)
#else
#define STA_VAP_DOWNUP_LOCK(_ic)                   spin_lock_irqsave(&(_ic)->ic_main_sta_lock,(_ic)->ic_main_sta_lock_flags)
#define STA_VAP_DOWNUP_UNLOCK(_ic)                 spin_unlock_irqrestore(&(_ic)->ic_main_sta_lock,(_ic)->ic_main_sta_lock_flags)
#endif


#if ATH_BEACON_DEFERRED_PROC
#define IEEE80211_CHAN_CHANGE_LOCK(_ic)             spin_lock_bh(&(_ic)->ic_chan_change_lock)
#define IEEE80211_CHAN_CHANGE_UNLOCK(_ic)           spin_unlock_bh(&(_ic)->ic_chan_change_lock)
#else
#define IEEE80211_CHAN_CHANGE_LOCK(_ic)             spin_lock_irqsave(&(_ic)->ic_chan_change_lock,(_ic)->ic_chan_change_lock_flags)
#define IEEE80211_CHAN_CHANGE_UNLOCK(_ic)           spin_unlock_irqrestore(&(_ic)->ic_chan_change_lock,(_ic)->ic_chan_change_lock_flags)
#endif

#define IEEE80211_RADAR_FOUND_LOCK(_ic)             spin_lock_dpc(&(_ic)->ic_radar_found_lock)
#define IEEE80211_RADAR_FOUND_UNLOCK(_ic)           spin_unlock_dpc(&(_ic)->ic_radar_found_lock)

#if ATH_BEACON_DEFERRED_PROC
#define ATH_BEACON_ALLOC_LOCK(_ic)                 spin_lock_bh(&(_ic)->ic_beacon_alloc_lock)
#define ATH_BEACON_ALLOC_UNLOCK(_ic)               spin_unlock_bh(&(_ic)->ic_beacon_alloc_lock)
#else
#define ATH_BEACON_ALLOC_LOCK(_ic)                 spin_lock_irqsave(&(_ic)->ic_beacon_alloc_lock,(_ic)->ic_beacon_alloc_lock_flags)
#define ATH_BEACON_ALLOC_UNLOCK(_ic)               spin_unlock_irqrestore(&(_ic)->ic_beacon_alloc_lock,(_ic)->ic_beacon_alloc_lock_flags)
#endif

#if DBDC_REPEATER_SUPPORT
#define GLOBAL_IC_LOCK_BH(_rl)                        spin_lock_bh(&(_rl)->radio_list_lock)
#define GLOBAL_IC_UNLOCK_BH(_rl)                      spin_unlock_bh(&(_rl)->radio_list_lock)
#else
#define GLOBAL_IC_LOCK_BH(_rl)
#define GLOBAL_IC_UNLOCK_BH(_rl)
#endif

#define	IEEE80211_STAT_LOCK(_vaplock)              spin_lock((_vaplock))
#define	IEEE80211_STAT_UNLOCK(_vaplock)            spin_unlock((_vaplock))

typedef spinlock_t ieee80211_p2p_gosche_lock_t;
#define IEEE80211_P2P_GOSCHE_LOCK_INIT(_gos)       spin_lock_init(&((_gos)->lock));
#define IEEE80211_P2P_GOSCHE_LOCK_DESTROY(_gos)    spin_lock_destroy(&((_gos)->lock))
#define IEEE80211_P2P_GOSCHE_LOCK(_gos)            spin_lock(&((_gos)->lock))
#define IEEE80211_P2P_GOSCHE_UNLOCK(_gos)          spin_unlock(&((_gos)->lock))

typedef spinlock_t ieee80211_tsf_timer_lock_t;
#define IEEE80211_TSF_TIMER_LOCK_INIT(_tsf)        spin_lock_init(&((_tsf)->lock));
#define IEEE80211_TSF_TIMER_LOCK_DESTROY(_tsf)     spin_lock_destroy(&((_tsf)->lock))
#define IEEE80211_TSF_TIMER_LOCK(_tsf)             spin_lock(&((_tsf)->lock))
#define IEEE80211_TSF_TIMER_UNLOCK(_tsf)           spin_unlock(&((_tsf)->lock))

typedef spinlock_t ieee80211_resmgr_oc_sched_lock_t;
#define IEEE80211_RESMGR_OCSCHE_LOCK_INIT(_ocslock)     spin_lock_init(&((_ocslock)->scheduler_lock));
#define IEEE80211_RESMGR_OCSCHE_LOCK_DESTROY(_ocslock)  spin_lock_destroy(&((_ocslock)->scheduler_lock))
#define IEEE80211_RESMGR_OCSCHE_LOCK(_ocslock)          spin_lock(&((_ocslock)->scheduler_lock))
#define IEEE80211_RESMGR_OCSCHE_UNLOCK(_ocslock)        spin_unlock(&((_ocslock)->scheduler_lock))

#define IEEE80211_VAP_LOCK(_vap)                   spin_lock_dpc(&_vap->iv_lock);
#define IEEE80211_VAP_UNLOCK(_vap)                 spin_unlock_dpc(&_vap->iv_lock);

#ifndef  ATH_BEACON_DEFERRED_PROC
#define OS_BEACON_DECLARE_AND_RESET_VAR(_flags)                unsigned long _flags = 0
#define OS_BEACON_READ_LOCK(_p_lock , _p_state, _flags)        OS_RWLOCK_READ_LOCK_IRQSAVE(_p_lock , _p_state, _flags)
#define OS_BEACON_READ_UNLOCK(_p_lock , _p_state, _flags)      OS_RWLOCK_READ_UNLOCK_IRQRESTORE(_p_lock , _p_state, _flags)
#define OS_BEACON_WRITE_LOCK(_p_lock , _p_state, _flags)       OS_RWLOCK_WRITE_LOCK_IRQSAVE(_p_lock , _p_state, _flags)
#define OS_BEACON_WRITE_UNLOCK(_p_lock , _p_state, _flags)     OS_RWLOCK_WRITE_UNLOCK_IRQRESTORE(_p_lock , _p_state, _flags)
#else
#define OS_BEACON_DECLARE_AND_RESET_VAR(_flags)                /* Do nothing */
#define OS_BEACON_READ_LOCK(_p_lock , _p_state, _flags)        OS_RWLOCK_READ_LOCK(_p_lock , _p_state)
#define OS_BEACON_READ_UNLOCK(_p_lock , _p_state, _flags)      OS_RWLOCK_READ_UNLOCK(_p_lock , _p_state)
#define OS_BEACON_WRITE_LOCK(_p_lock , _p_state, _flags)       OS_RWLOCK_WRITE_LOCK(_p_lock , _p_state)
#define OS_BEACON_WRITE_UNLOCK(_p_lock , _p_state, _flags)     OS_RWLOCK_WRITE_UNLOCK(_p_lock , _p_state)
#endif

#define    KEYIX_INVALID    ((u_int8_t) -1)

/*
 * Some times hardware passes the frames without decryption. S/W can
 * choose to decrypt them or to drop them. When enabled, all the frames
 * with KEYMISS set, would be decrypted in s/w and if not they will be
 * ignored
 */
#define IEEE80211_VAP_CCMPSW_ENCDEC_ENABLE(_v) \
            ((_v)->iv_ccmpsw_seldec = 1)

#define IEEE80211_VAP_CCMPSW_ENCDEC_DISABLE(_v)\
            ((_v)->iv_ccmpsw_seldec = 0)


#define IC_FLAG_FUNCS(xyz) \
     static INLINE int ieee80211_ic_##xyz##_is_set (struct ieee80211com *_ic) { \
        return (_ic->ic_##xyz == 1); \
     } \
     static INLINE int ieee80211_ic_##xyz##_is_clear (struct ieee80211com *_ic) { \
        return (_ic->ic_##xyz == 0); \
     } \
     static INLINE void ieee80211_ic_##xyz##_set (struct ieee80211com *_ic) { \
        _ic->ic_##xyz = 1; \
     } \
     static INLINE void  ieee80211_ic_##xyz##_clear (struct ieee80211com *_ic) { \
        _ic->ic_##xyz = 0; \
     }

IC_FLAG_FUNCS(wep_tkip_htrate)
IC_FLAG_FUNCS(non_ht_ap)
IC_FLAG_FUNCS(block_dfschan)
IC_FLAG_FUNCS(doth)
IC_FLAG_FUNCS(off_channel_support)
IC_FLAG_FUNCS(ht20Adhoc)
IC_FLAG_FUNCS(ht40Adhoc)
IC_FLAG_FUNCS(htAdhocAggr)
IC_FLAG_FUNCS(disallowAutoCCchange)
IC_FLAG_FUNCS(ignoreDynamicHalt)
IC_FLAG_FUNCS(p2pDevEnable)
IC_FLAG_FUNCS(override_proberesp_ie)
IC_FLAG_FUNCS(2g_csa)
IC_FLAG_FUNCS(wnm)
IC_FLAG_FUNCS(offchanscan)
IC_FLAG_FUNCS(enh_ind_rpt)
#define IEEE80211_VAP_IS_DROP_UNENC(_v)             ((_v)->iv_flags & IEEE80211_F_DROPUNENC)
#define IEEE80211_VAP_DROP_UNENC_ENABLE(_v)         (ieee80211vap_set_flag((_v), IEEE80211_F_DROPUNENC))
#define IEEE80211_VAP_DROP_UNENC_DISABLE(_v)        (ieee80211vap_clear_flag((_v), IEEE80211_F_DROPUNENC))

#define IEEE80211_VAP_COUNTERM_ENABLE(_v)           (ieee80211vap_set_flag((_v), IEEE80211_F_COUNTERM))
#define IEEE80211_VAP_COUNTERM_DISABLE(_v)          (ieee80211vap_clear_flag((_v), IEEE80211_F_COUNTERM))
#define IEEE80211_VAP_IS_COUNTERM_ENABLED(_v)       ((_v)->iv_flags & IEEE80211_F_COUNTERM)

#define IEEE80211_VAP_WPA_ENABLE(_v)                (ieee80211vap_set_flag((_v), IEEE80211_F_WPA))
#define IEEE80211_VAP_WPA_DISABLE(_v)               (ieee80211vap_clear_flag((_v), IEEE80211_F_WPA))
#define IEEE80211_VAP_IS_WPA_ENABLED(_v)            ((_v)->iv_flags & IEEE80211_F_WPA)

#define IEEE80211_VAP_PUREG_ENABLE(_v)              (ieee80211vap_set_flag((_v), IEEE80211_F_PUREG))
#define IEEE80211_VAP_PUREG_DISABLE(_v)             (ieee80211vap_clear_flag((_v), IEEE80211_F_PUREG))
#define IEEE80211_VAP_IS_PUREG_ENABLED(_v)          ((_v)->iv_flags & IEEE80211_F_PUREG)

#define IEEE80211_VAP_PRIVACY_ENABLE(_v)            (ieee80211vap_set_flag((_v), IEEE80211_F_PRIVACY))
#define IEEE80211_VAP_PRIVACY_DISABLE(_v)           (ieee80211vap_clear_flag((_v), IEEE80211_F_PRIVACY))
#define IEEE80211_VAP_IS_PRIVACY_ENABLED(_v)        ((_v)->iv_flags & IEEE80211_F_PRIVACY)

#define IEEE80211_VAP_HIDESSID_ENABLE(_v)           (ieee80211vap_set_flag((_v), IEEE80211_F_HIDESSID))
#define IEEE80211_VAP_HIDESSID_DISABLE(_v)          (ieee80211vap_clear_flag((_v), IEEE80211_F_HIDESSID))
#define IEEE80211_VAP_IS_HIDESSID_ENABLED(_v)       ((_v)->iv_flags & IEEE80211_F_HIDESSID)

#define IEEE80211_VAP_NOBRIDGE_ENABLE(_v)           (ieee80211vap_set_flag((_v), IEEE80211_F_NOBRIDGE))
#define IEEE80211_VAP_NOBRIDGE_DISABLE(_v)          (ieee80211vap_clear_flag((_v), IEEE80211_F_NOBRIDGE))
#define IEEE80211_VAP_IS_NOBRIDGE_ENABLED(_v)       ((_v)->iv_flags & IEEE80211_F_NOBRIDGE)

#define IEEE80211_VAP_IS_TIMUPDATE_ENABLED(_v)      ((_v)->iv_flags_ext & IEEE80211_F_TIMUPDATE)
#define IEEE80211_VAP_TIMUPDATE_ENABLE(_v)          (ieee80211vap_set_flag_ext((_v), IEEE80211_F_TIMUPDATE))
#define IEEE80211_VAP_TIMUPDATE_DISABLE(_v)         (ieee80211vap_clear_flag_ext((_v), IEEE80211_F_TIMUPDATE))

#define IEEE80211_VAP_IS_UAPSD_ENABLED(_v)          ((_v)->iv_flags_ext & IEEE80211_FEXT_UAPSD)
#define IEEE80211_VAP_UAPSD_ENABLE(_v)              (ieee80211vap_set_flag_ext((_v), IEEE80211_FEXT_UAPSD))
#define IEEE80211_VAP_UAPSD_DISABLE(_v)             (ieee80211vap_clear_flag_ext((_v), IEEE80211_FEXT_UAPSD))

#define IEEE80211_VAP_IS_SLEEPING(_v)               ((_v)->iv_flags_ext & IEEE80211_FEXT_SLEEP)
#define IEEE80211_VAP_GOTOSLEEP(_v)                 (ieee80211vap_set_flag_ext((_v), IEEE80211_FEXT_SLEEP))
#define IEEE80211_VAP_WAKEUP(_v)                    (ieee80211vap_clear_flag_ext((_v), IEEE80211_FEXT_SLEEP))

#define IEEE80211_VAP_IS_EOSPDROP_ENABLED(_v)       ((_v)->iv_flags_ext & IEEE80211_FEXT_EOSPDROP)
#define IEEE80211_VAP_EOSPDROP_ENABLE(_v)           (ieee80211vap_set_flag_ext((_v), IEEE80211_FEXT_EOSPDROP))
#define IEEE80211_VAP_EOSPDROP_DISABLE(_v)          (ieee80211vap_clear_flag_ext((_v), IEEE80211_FEXT_EOSPDROP))

#define IEEE80211_VAP_IS_HTRATES_ENABLED(_v)        ((_v)->iv_flags_ext & IEEE80211_FEXT_HTRATES)
#define IEEE80211_VAP_HTRATES_ENABLE(_v)            (ieee80211vap_set_flag_ext((_v), IEEE80211_FEXT_HTRATES))
#define IEEE80211_VAP_HTRATES_DISABLE(_v)           (ieee80211vap_clear_flag_ext((_v), IEEE80211_FEXT_HTRATES))

#define IEEE80211_VAP_SAFEMODE_ENABLE(_v)           (ieee80211vap_set_flag_ext222((_v), IEEE80211_FEXT_SAFEMODE))
#define IEEE80211_VAP_SAFEMODE_DISABLE(_v)          (ieee80211vap_clear_flag_ext222((_v), IEEE80211_FEXT_SAFEMODE))
#define IEEE80211_VAP_IS_SAFEMODE_ENABLED(_v)       ((_v)->iv_flags_ext & IEEE80211_FEXT_SAFEMODE)

#define IEEE80211_VAP_DELIVER_80211_ENABLE(_v)      (ieee80211vap_set_flag_ext((_v), IEEE80211_FEXT_DELIVER_80211))
#define IEEE80211_VAP_DELIVER_80211_DISABLE(_v)     (ieee80211vap_clear_flag_ext((_v), IEEE80211_FEXT_DELIVER_80211))
#define IEEE80211_VAP_IS_DELIVER_80211_ENABLED(_v)  ((_v)->iv_flags_ext & IEEE80211_FEXT_DELIVER_80211)

#define IEEE80211_VAP_SEND_80211_ENABLE(_v)         (ieee80211vap_set_flag_ext((_v), IEEE80211_FEXT_SEND_80211))
#define IEEE80211_VAP_SEND_80211_DISABLE(_v)        (ieee80211vap_clear_flag_ext((_v), IEEE80211_FEXT_SEND_80211))
#define IEEE80211_VAP_IS_SEND_80211_ENABLED(_v)     ((_v)->iv_flags_ext & IEEE80211_FEXT_SEND_80211)

#define IEEE80211_VAP_WDS_ENABLE(_v)                (ieee80211vap_set_flag_ext((_v), IEEE80211_FEXT_WDS))
#define IEEE80211_VAP_WDS_DISABLE(_v)               (ieee80211vap_clear_flag_ext((_v), IEEE80211_FEXT_WDS))
#define IEEE80211_VAP_IS_WDS_ENABLED(_v)            ((_v)->iv_flags_ext & IEEE80211_FEXT_WDS)

#define IEEE80211_VAP_STATIC_WDS_ENABLE(_v)         (ieee80211vap_set_flag_ext((_v), IEEE80211_FEXT_WDS_STATIC))
#define IEEE80211_VAP_STATIC_WDS_DISABLE(_v)        (ieee80211vap_clear_flag_ext((_v), IEEE80211_FEXT_WDS_STATIC))
#define IEEE80211_VAP_IS_STATIC_WDS_ENABLED(_v)     ((_v)->iv_flags_ext & IEEE80211_FEXT_WDS_STATIC)

#define IEEE80211_VAP_WDS_AUTODETECT_ENABLE(_v)     (ieee80211vap_set_flag_ext((_v), IEEE80211_FEXT_WDS_AUTODETECT))
#define IEEE80211_VAP_WDS_AUTODETECT_DISABLE(_v)    (ieee80211vap_clear_flag_ext((_v), IEEE80211_FEXT_WDS_AUTODETECT))
#define IEEE80211_VAP_IS_WDS_AUTODETECT_ENABLED(_v) ((_v)->iv_flags_ext & IEEE80211_FEXT_WDS_AUTODETECT)

#define IEEE80211_VAP_PURE11N_ENABLE(_v)            (ieee80211vap_set_flag_ext((_v), IEEE80211_FEXT_PURE11N))
#define IEEE80211_VAP_PURE11N_DISABLE(_v)           (ieee80211vap_clear_flag_ext((_v), IEEE80211_FEXT_PURE11N))
#define IEEE80211_VAP_IS_PURE11N_ENABLED(_v)        ((_v)->iv_flags_ext & IEEE80211_FEXT_PURE11N)

#define IEEE80211_VAP_PURE11AC_ENABLE(_v)           (ieee80211vap_set_flag_ext2((_v), IEEE80211_FEXT2_PURE11AC))
#define IEEE80211_VAP_PURE11AC_DISABLE(_v)          (ieee80211vap_clear_flag_ext2((_v), IEEE80211_FEXT2_PURE11AC))
#define IEEE80211_VAP_IS_PURE11AC_ENABLED(_v)       ((_v)->iv_flags_ext2 & IEEE80211_FEXT2_PURE11AC)

#define IEEE80211_VAP_STRICT_BW_ENABLE(_v)           (ieee80211vap_set_flag_ext2((_v), IEEE80211_FEXT2_STRICT_BW))
#define IEEE80211_VAP_STRICT_BW_DISABLE(_v)          (ieee80211vap_clear_flag_ext2((_v), IEEE80211_FEXT2_STRICT_BW))
#define IEEE80211_VAP_IS_STRICT_BW_ENABLED(_v)       ((_v)->iv_flags_ext2 & IEEE80211_FEXT2_STRICT_BW)

#if ATH_NON_BEACON_AP
#define IEEE80211_VAP_NON_BEACON_ENABLE(_v)           (ieee80211vap_set_flag_ext22((_v), IEEE80211_FEXT2_NON_BEACON))
#define IEEE80211_VAP_NON_BEACON_DISABLE(_v)          (ieee80211vap_clear_flag_ext22((_v), IEEE80211_FEXT2_NON_BEACON))
#define IEEE80211_VAP_IS_NON_BEACON_ENABLED(_v)       ((_v)->iv_flags_ext2 & IEEE80211_FEXT2_NON_BEACON)
#endif
#define IEEE80211_VAP_BACKHAUL_ENABLE(_v)           (ieee80211vap_set_flag_ext2((_v), IEEE80211_FEXT2_BACKHAUL))
#define IEEE80211_VAP_BACKHAUL_DISABLE(_v)          (ieee80211vap_clear_flag_ext2((_v), IEEE80211_FEXT2_BACKHAUL))
#define IEEE80211_VAP_IS_BACKHAUL_ENABLED(_v)       ((_v)->iv_flags_ext2 & IEEE80211_FEXT2_BACKHAUL)

#define IEEE80211_VAP_NOCABQ_ENABLE(_v)           (ieee80211vap_set_flag_ext2((_v), IEEE80211_FEXT2_NOCABQ))
#define IEEE80211_VAP_NOCABQ_DISABLE(_v)          (ieee80211vap_clear_flag_ext2((_v), IEEE80211_FEXT2_NOCABQ))
#define IEEE80211_VAP_IS_NOCABQ_ENABLED(_v)       ((_v)->iv_flags_ext2 & IEEE80211_FEXT2_NOCABQ)

#define IEEE80211_VAP_IS_MBSS_ENABLED(_v) \
    ((_v)->iv_flags_ext2 & IEEE80211_FEXT2_MBSS_VAP)

#define IEEE80211_VAP_MBSS_NON_TRANS_ENABLE(_v) do { \
    ieee80211vap_set_flag_ext2((_v), \
            (IEEE80211_FEXT2_NON_TRANSMIT_VAP | IEEE80211_FEXT2_MBSS_VAP)); \
  } while(0)

#define IEEE80211_VAP_MBSS_NON_TRANS_DISABLE(_v) do { \
    ieee80211vap_clear_flag_ext2((_v), IEEE80211_FEXT2_NON_TRANSMIT_VAP); \
    ieee80211vap_clear_flag_ext2((_v), IEEE80211_FEXT2_MBSS_VAP); \
 } while(0)

#define IEEE80211_VAP_IS_MBSS_NON_TRANSMIT_ENABLED(_v)  \
  (((_v)->iv_flags_ext2 & IEEE80211_FEXT2_MBSS_MASK) ==		\
    (IEEE80211_FEXT2_NON_TRANSMIT_VAP | IEEE80211_FEXT2_MBSS_VAP))

INLINE static int
isorbi_ie(wlan_if_t vap, u_int8_t *frm)
{
    /*add code to identify IE*/
    if (IEEE80211_VAP_IS_BACKHAUL_ENABLED(vap))
        return 1;
    else
        return 0;
}

#define IEEE80211_VAP_PUREB_ENABLE(_v)              (ieee80211vap_set_flag_ext((_v), IEEE80211_FEXT_PUREB))
#define IEEE80211_VAP_PUREB_DISABLE(_v)             (ieee80211vap_clear_flag_ext((_v), IEEE80211_FEXT_PUREB))
#define IEEE80211_VAP_IS_PUREB_ENABLED(_v)          ((_v)->iv_flags_ext & IEEE80211_FEXT_PUREB)

#define IEEE80211_VAP_APPIE_UPDATE_ENABLE(_v)       (ieee80211vap_set_flag_ext((_v), IEEE80211_FEXT_APPIE_UPDATE))
#define IEEE80211_VAP_APPIE_UPDATE_DISABLE(_v)      (ieee80211vap_clear_flag_ext((_v), IEEE80211_FEXT_APPIE_UPDATE))
#define IEEE80211_VAP_IS_APPIE_UPDATE_ENABLED(_v)   ((_v)->iv_flags_ext & IEEE80211_FEXT_APPIE_UPDATE)

#define IEEE80211_VAP_AUTOASSOC_ENABLE(_v)       ((_v)->auto_assoc = 1)
#define IEEE80211_VAP_AUTOASSOC_DISABLE(_v)      ((_v)->auto_assoc = 0)
#define IEEE80211_VAP_IS_AUTOASSOC_ENABLED(_v)   ((_v)->auto_assoc == 1) ? 1 : 0

#define VAP_FLAG_FUNCS(xyz) \
     static INLINE int ieee80211_vap_##xyz##_is_set (struct ieee80211vap *_vap) { \
        return (_vap->iv_##xyz == 1); \
     } \
     static INLINE int ieee80211_vap_##xyz##_is_clear (struct ieee80211vap *_vap) { \
        return (_vap->iv_##xyz == 0); \
     } \
     static INLINE void ieee80211_vap_##xyz##_set (struct ieee80211vap *_vap) { \
        _vap->iv_##xyz =1; \
     } \
     static INLINE void  ieee80211_vap_##xyz##_clear (struct ieee80211vap *_vap) { \
        _vap->iv_##xyz = 0; \
     }

VAP_FLAG_FUNCS(deleted)
VAP_FLAG_FUNCS(smps)
VAP_FLAG_FUNCS(sw_bmiss)
VAP_FLAG_FUNCS(copy_beacon)
VAP_FLAG_FUNCS(wapi)
VAP_FLAG_FUNCS(cansleep)
VAP_FLAG_FUNCS(sta_fwd)
VAP_FLAG_FUNCS(scanning)
VAP_FLAG_FUNCS(standby)
VAP_FLAG_FUNCS(dynamic_mimo_ps)
VAP_FLAG_FUNCS(wme)
VAP_FLAG_FUNCS(doth)
VAP_FLAG_FUNCS(country_ie)
VAP_FLAG_FUNCS(off_channel_support)
VAP_FLAG_FUNCS(dfswait)
VAP_FLAG_FUNCS(erpupdate)
VAP_FLAG_FUNCS(needs_scheduler)
VAP_FLAG_FUNCS(forced_sleep)
VAP_FLAG_FUNCS(no_multichannel)
VAP_FLAG_FUNCS(qbssload)
VAP_FLAG_FUNCS(qbssload_update)
VAP_FLAG_FUNCS(rrm)
VAP_FLAG_FUNCS(wnm)
VAP_FLAG_FUNCS(ap_reject_dfs_chan)
VAP_FLAG_FUNCS(smartnet_enable)
VAP_FLAG_FUNCS(trigger_mlme_resp)
VAP_FLAG_FUNCS(mfp_test)
VAP_FLAG_FUNCS(proxyarp)
VAP_FLAG_FUNCS(dgaf_disable)
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
VAP_FLAG_FUNCS(nopbn)
#endif
VAP_FLAG_FUNCS(ext_ifu_acs)
VAP_FLAG_FUNCS(ext_acs_inprogress)
VAP_FLAG_FUNCS(send_additional_ies)

VAP_FLAG_FUNCS(256qam)
#if ATH_SUPPORT_WRAP
VAP_FLAG_FUNCS(psta)
VAP_FLAG_FUNCS(wrap)
VAP_FLAG_FUNCS(mpsta)
#endif
VAP_FLAG_FUNCS(11ng_vht_interop)
VAP_FLAG_FUNCS(mbo)
VAP_FLAG_FUNCS(oce)
VAP_FLAG_FUNCS(reset_ap_vaps)
VAP_FLAG_FUNCS(stop_bss)
VAP_FLAG_FUNCS(peer_create_failed)
VAP_FLAG_FUNCS(ext_bssload)
VAP_FLAG_FUNCS(ext_bssload_update)
VAP_FLAG_FUNCS(registered)

/* There should be only one ic_evtable */
#define IEEE80211COM_DELIVER_VAP_EVENT(_ic,_osif,_evt)  do {             \
        int i;                                                                                 \
        IEEE80211_COMM_LOCK(ic);                                                               \
        for(i=0;i<IEEE80211_MAX_DEVICE_EVENT_HANDLERS; ++i) {                                  \
            if ( _ic->ic_evtable[i]  && _ic->ic_evtable[i]->wlan_dev_vap_event) {              \
                (* _ic->ic_evtable[i]->wlan_dev_vap_event)(_ic->ic_event_arg[i],_ic,_osif,_evt);\
            }                                                                                  \
        }                                                                                      \
        IEEE80211_COMM_UNLOCK(ic);                                                             \
    } while(0)

#define OSIF_RADIO_DELIVER_EVENT_RADAR_DETECTED(_ic)  do {             \
        int _i;                                                                                 \
        IEEE80211_COMM_LOCK(_ic);                                                               \
        for(_i=0;_i<IEEE80211_MAX_DEVICE_EVENT_HANDLERS; ++_i) {                                  \
            if ( (_ic)->ic_evtable[_i]  && (_ic)->ic_evtable[_i]->wlan_radar_event) {              \
                (* (_ic)->ic_evtable[_i]->wlan_radar_event)((_ic)->ic_event_arg[_i], (_ic));\
            }                                                                                  \
        }                                                                                      \
        IEEE80211_COMM_UNLOCK(_ic);                                                             \
    } while(0)

#define OSIF_RADIO_DELIVER_EVENT_WATCHDOG(_ic,_reason)  do {             \
        int _i;                                                                                 \
        IEEE80211_COMM_LOCK(_ic);                                                               \
        for(_i=0;_i<IEEE80211_MAX_DEVICE_EVENT_HANDLERS; ++_i) {                                  \
            if ( (_ic)->ic_evtable[_i]  && (_ic)->ic_evtable[_i]->wlan_wdt_event) {              \
                (* (_ic)->ic_evtable[_i]->wlan_wdt_event)((_ic)->ic_event_arg[_i],(_ic),_reason);\
            }                                                                                  \
        }                                                                                      \
        IEEE80211_COMM_UNLOCK(_ic);                                                             \
    } while(0)

#define OSIF_RADIO_DELIVER_EVENT_CAC_START(_ic)  do {     \
    int _i;                                                 \
    IEEE80211_COMM_LOCK(_ic);                               \
    for(_i=0;_i<IEEE80211_MAX_DEVICE_EVENT_HANDLERS; ++_i) {\
      if ( (_ic)->ic_evtable[_i]  && (_ic)->ic_evtable[_i]->wlan_cac_start_event) { \
        (*(_ic)->ic_evtable[_i]->wlan_cac_start_event)((_ic)->ic_event_arg[_i], (_ic));\
      }                                                     \
    }                                                       \
    IEEE80211_COMM_UNLOCK(_ic);                             \
  } while(0)

#define OSIF_RADIO_DELIVER_EVENT_UP_AFTER_CAC(_ic)  do {     \
    int _i;                                                 \
    IEEE80211_COMM_LOCK(_ic);                               \
    for(_i=0;_i<IEEE80211_MAX_DEVICE_EVENT_HANDLERS; ++_i) {\
      if ( (_ic)->ic_evtable[_i]  && (_ic)->ic_evtable[_i]->wlan_up_after_cac_event) { \
        (*(_ic)->ic_evtable[_i]->wlan_up_after_cac_event)((_ic)->ic_event_arg[_i], (_ic));\
      }                                                     \
    }                                                       \
    IEEE80211_COMM_UNLOCK(_ic);                             \
  } while(0)

#define OSIF_RADIO_DELIVER_EVENT_CHAN_UTIL(_ic, _self_util, _obss_util)  do {     \
    int _i;                                                 \
    IEEE80211_COMM_LOCK(_ic);                               \
    for(_i=0;_i<IEEE80211_MAX_DEVICE_EVENT_HANDLERS; ++_i) {\
      if ( (_ic)->ic_evtable[_i]  && (_ic)->ic_evtable[_i]->wlan_chan_util_event) { \
        (*(_ic)->ic_evtable[_i]->wlan_chan_util_event)((_ic), _self_util, _obss_util);\
      }                                                     \
    }                                                       \
    IEEE80211_COMM_UNLOCK(_ic);                             \
  } while(0)

/*
 * Used by proto SM to check if calls to LMAC are syncronous or not.
 * The check assumes offload cmds to target firmware are not-syncronous.
 */
#define IEEE80211COM_IS_SYNCRONOUS(_ic) (!(_ic)->ic_is_mode_offload(_ic))

/* Atheros ABOLT definitions */
#define IEEE80211_ABOLT_TURBO_G        0x01    /* Legacy Turbo G */
#define IEEE80211_ABOLT_TURBO_PRIME    0x02    /* Turbo Prime */
#define IEEE80211_ABOLT_COMPRESSION    0x04    /* Compression */
#define IEEE80211_ABOLT_FAST_FRAME     0x08    /* Fast Frames */
#define IEEE80211_ABOLT_BURST          0x10    /* Bursting */
#define IEEE80211_ABOLT_WME_ELE        0x20    /* WME based cwmin/max/burst tuning */
#define IEEE80211_ABOLT_XR             0x40    /* XR */
#define IEEE80211_ABOLT_AR             0x80    /* AR switches out based on adjacent non-turbo traffic */

/* Atheros Advanced Capabilities ABOLT definition */
#define IEEE80211_ABOLT_ADVCAP                  \
    (IEEE80211_ABOLT_TURBO_PRIME |              \
    IEEE80211_ABOLT_COMPRESSION |               \
    IEEE80211_ABOLT_FAST_FRAME |                \
    IEEE80211_ABOLT_XR |                        \
    IEEE80211_ABOLT_AR |                        \
    IEEE80211_ABOLT_BURST |                     \
    IEEE80211_ABOLT_WME_ELE)

/* check if a capability was negotiated for use */
#define IEEE80211_ATH_CAP(vap, ni, bit) \
    ((ni)->ni_ath_flags & (vap)->iv_ath_cap & (bit))

/*
 * flags to be passed to ieee80211_vap_create function .
 */
#define IEEE80211_CLONE_BSSID   0x0001      /* allocate unique mac/bssid */
#define IEEE80211_NO_STABEACONS 0x0002      /* Do not setup the station beacon timers */
#define IEEE80211_CLONE_WDS             0x0004  /* enable WDS processing */
#define IEEE80211_CLONE_WDSLEGACY       0x0008  /* legacy WDS operation */
#define IEEE80211_PRIMARY_VAP           0x0010  /* primary vap */
#define IEEE80211_P2PDEV_VAP            0x0020  /* p2pdev vap */
#define IEEE80211_P2PGO_VAP             0x0040  /* p2p-go vap */
#define IEEE80211_P2PCLI_VAP            0x0080  /* p2p-client vap */
#define IEEE80211_P2P_DEVICE            (IEEE80211_P2PDEV_VAP | IEEE80211_P2PGO_VAP | IEEE80211_P2PCLI_VAP)

#define NET80211_MEMORY_TAG     '11tN'

/* ic_htflags iv_htflags */
#define IEEE80211_HTF_SHORTGI40     0x0001
#define IEEE80211_HTF_SHORTGI20     0x0002

/* MFP support values */
typedef enum _ieee80211_mfp_type{
    IEEE80211_MFP_QOSDATA,
    IEEE80211_MFP_PASSTHRU,
    IEEE80211_MFP_HW_CRYPTO
} ieee80211_mfp_type;


void ieee80211_start_running(struct ieee80211com *ic);
void ieee80211_stop_running(struct ieee80211com *ic);
int ieee80211com_register_event_handlers(struct ieee80211com *ic,
                                     void *event_arg,
                                     wlan_dev_event_handler_table *evtable);
int ieee80211com_unregister_event_handlers(struct ieee80211com *ic,
                                     void *event_arg,
                                     wlan_dev_event_handler_table *evtable);

u_int16_t ieee80211_vaps_active(struct ieee80211com *ic);
u_int16_t ieee80211_vaps_ready(struct ieee80211com *ic, enum ieee80211_opmode opmode);
u_int8_t ieee80211_get_num_vaps_up(struct ieee80211com *ic);
u_int8_t ieee80211_get_num_ap_vaps_up(struct ieee80211com *ic);

int ieee80211_vap_setup(struct ieee80211com *ic, struct ieee80211vap *vap,
                        struct vdev_mlme_obj *vdev_mlme,
                        int opmode, int scan_priority_base, u_int32_t flags,
                        const u_int8_t bssid[IEEE80211_ADDR_LEN]);

void ieee80211_proc_vattach(struct ieee80211vap *vap);
int ieee80211_ifattach(struct ieee80211com *ic, IEEE80211_REG_PARAMETERS *);
void ieee80211_ifdetach(struct ieee80211com *ic);

int ieee80211_vap_attach(struct ieee80211vap *vap);
void ieee80211_vap_detach(struct ieee80211vap *vap);
void ieee80211_vap_free(struct ieee80211vap *vap);

void ieee80211_vap_deliver_stop(struct ieee80211vap *vap);
void ieee80211_vap_deliver_stop_error(struct ieee80211vap *vap);

int ieee80211_vap_update_superag_cap(struct ieee80211vap *vap, int en_superag);
int ieee80211_vap_match_ssid(struct ieee80211vap *vap, const u_int8_t *ssid, u_int8_t ssidlen);

int ieee80211_vap_register_events(struct ieee80211vap *vap, wlan_event_handler_table *evtab);
int ieee80211_vap_register_mlme_events(struct ieee80211vap *vap, os_handle_t oshandle, wlan_mlme_event_handler_table *evtab);
int ieee80211_vap_unregister_mlme_events(struct ieee80211vap *vap,os_handle_t oshandle, wlan_mlme_event_handler_table *evtab);
int ieee80211_vap_register_misc_events(struct ieee80211vap *vap, os_handle_t oshandle, wlan_misc_event_handler_table *evtab);
int ieee80211_vap_unregister_misc_events(struct ieee80211vap *vap,os_handle_t oshandle, wlan_misc_event_handler_table *evtab);
int ieee80211_vap_register_ccx_events(struct ieee80211vap *vap, os_if_t osif, wlan_ccx_handler_table *evtab);
void ieee80211_vap_mlme_inact_erp_timeout(struct ieee80211com *ic);
ieee80211_aplist_config_t ieee80211_vap_get_aplist_config(struct ieee80211vap *vap);
ieee80211_candidate_aplist_t ieee80211_vap_get_aplist(struct ieee80211vap *vap);
systime_t ieee80211_get_last_data_timestamp(wlan_if_t vaphandle);
systime_t ieee80211_get_directed_frame_timestamp(wlan_if_t vaphandle);
systime_t ieee80211_get_last_ap_frame_timestamp(wlan_if_t vaphandle);
systime_t ieee80211_get_traffic_indication_timestamp(wlan_if_t vaphandle);
bool ieee80211_is_connected(wlan_if_t vaphandle);

int ieee80211_vendorie_vdetach(wlan_if_t vaphandle);

int ieee80211_aplist_config_vattach(ieee80211_aplist_config_t *pconfig,
        osdev_t osdev);

int ieee80211_aplist_vattach(ieee80211_candidate_aplist_t *aplist,
        wlan_if_t vaphandle, osdev_t osdev);

int ieee80211_aplist_vdetach(ieee80211_candidate_aplist_t *aplist);

int ieee80211_aplist_config_vdetach(ieee80211_aplist_config_t *pconfig);

void ieee80211_aplist_config_init(ieee80211_aplist_config_t aplist_config);

bool ieee80211_aplist_get_accept_any_bssid(ieee80211_aplist_config_t pconfig);

int ieee80211_aplist_get_desired_bssid_count(ieee80211_aplist_config_t pconfig);

int ieee80211_aplist_get_desired_bssid(ieee80211_aplist_config_t pconfig,
                  int index, uint8_t **bssid);

uint32_t ieee80211_channel_ieee(struct ieee80211_ath_channel *chan);

int ieee80211_dfs_cac_start(struct ieee80211vap *vap);

int ieee80211_dfs_cac_cancel(struct ieee80211com *ic);

void ieee80211_dfs_cac_timer_start_async(void *data);

void ieee80211_dfs_cac_stop(struct ieee80211vap *vap,
                            int force);

int ieee80211_dfs_vaps_in_dfs_wait(struct ieee80211com *ic,
	                           struct ieee80211vap *curr_vap);

void ieee80211_mark_dfs(struct ieee80211com *ic,
                        uint8_t ieee,
                        uint16_t freq,
                        uint8_t ic_vhtop_ch_freq_seg2,
                        uint64_t ic_flags);

QDF_STATUS ieee80211_start_csa(struct ieee80211com *ic,
                         uint8_t ieeeChan,
                         uint16_t freq,
                         uint8_t cfreq2,
                         uint64_t flags);

void ieee80211_dfs_start_tx_rcsa_and_waitfor_rx_csa(struct ieee80211com *ic);

void ieee80211_dfs_rx_rcsa(struct ieee80211com *ic);

uint8_t *ieee80211_add_nol_ie(uint8_t *frm, struct ieee80211vap *vap,
			      struct ieee80211com *ic);

void ieee80211_dfs_cancel_waitfor_csa_timer(struct ieee80211com *ic);

bool ieee80211_dfs_start_rcsa(struct ieee80211com *ic);

OS_TIMER_FUNC(ieee80211_dfs_tx_rcsa_task);

OS_TIMER_FUNC(ieee80211_dfs_waitfor_csa_task);

#if ATH_SUPPORT_DFS && QCA_DFS_NOL_VAP_RESTART
OS_TIMER_FUNC(ieee80211_dfs_nochan_vap_restart);
#endif

void ieee80211_dfs_channel_mark_radar(struct ieee80211com *ic,
                                      struct ieee80211_ath_channel *chan);
#if ATH_SUPPORT_IBSS_DFS
u_int ieee80211_create_dfs_channel_list(struct ieee80211com *ic,
                                        struct channel_map_field *ch_map_list);
#endif

void ieee80211_dfs_clist_update(struct ieee80211com *ic,
                                void *nollist,
                                int nentries);

/*
 * IC-based functions that gather information from all VAPs.
 */

/*
 * ieee80211com_get_traffic_indication_timestamp
 *     returns most recent data traffic timestamp in all of the IC's VAPs.
 */
systime_t ieee80211com_get_traffic_indication_timestamp(struct ieee80211com *ic);

/*
 * ieee80211_get_vap_opmode_count
 *     returns number of VAPs of each type currently active in an IC.
 */
void
ieee80211_get_vap_opmode_count(struct ieee80211com *ic,
                               struct ieee80211_vap_opmode_count *vap_opmode_count);

/*
 * ieee80211_sta_assoc_in_progress
 *     returns 1 if STA vap is not in associated state else 0
 */
u_int8_t ieee80211_sta_assoc_in_progress(struct ieee80211com *ic);

int ieee80211_regdmn_reset(struct ieee80211com *ic);

int ieee80211_mbssid_setup(struct ieee80211vap *vap);
bool ieee80211_mbssid_check_max_profiles(struct ieee80211com *ic,uint8_t count);

/*
 * ieee80211_flush_vap_mgmt_baclkog_queue : Flush mgmt frames for
 * underlying vap from host backlog queue.
 * @vap: vap whose frames shall be flushed
 * @force: whether forcefully flush mgmt_txrx queue or not.
 *
 * Return: void
 */
void
ieee80211_flush_vap_mgmt_queue(struct ieee80211vap *vap,
        bool force);

int
ieee80211_has_weptkipaggr(struct ieee80211_node *ni);

bool

wlan_bsteering_set_inact_params(struct ieee80211com *ic,
                                u_int16_t inact_check_interval,
                                u_int16_t inact_normal,
                                u_int16_t inact_overload);

void wlan_bsteering_set_overload_param(struct ieee80211com *ic,bool overload);
void wlan_bsteering_send_null(struct ieee80211com *ic,
                              u_int8_t *macaddr,
                              struct ieee80211vap *vap);
bool wlan_bsteering_direct_attach_enable(struct ieee80211com *ic,bool enable);

static INLINE uint32_t ieee80211com_get_target_supported_hefrag
                                    (struct ieee80211com *ic) {
    uint32_t ic_hecap_mac;

    (void)qdf_get_u32(ic->ic_he.hecap_macinfo, &ic_hecap_mac);
    return HECAP_MAC_HEFRAG_GET_FROM_IC(qdf_cpu_to_le32(ic_hecap_mac));
}

static INLINE uint32_t ieee80211com_get_target_supported_hesubfee
                                    (struct ieee80211com *ic) {
    uint32_t *ic_hecap_phy;

    ic_hecap_phy = &ic->ic_he.hecap_phyinfo[IC_HECAP_PHYDWORD_IDX0];
    return HECAP_PHY_SUBFME_GET_FROM_IC(ic_hecap_phy);
}

static INLINE osdev_t
ieee80211com_get_oshandle(struct ieee80211com *ic)
{
     return ic->ic_osdev;
}

static INLINE void
ieee80211com_set_flags(struct ieee80211com *ic, u_int32_t cap)
{
    ic->ic_flags |= cap;
    wlan_ic_psoc_set_flag(ic, cap);
    wlan_ic_pdev_set_flag(ic, cap);
}

static INLINE void
ieee80211com_set_flags_ext(struct ieee80211com *ic, u_int32_t cap)
{
    ic->ic_flags_ext |= cap;
    wlan_ic_psoc_set_extflag(ic, cap);
    wlan_ic_pdev_set_extflag(ic, cap);
}

static INLINE void
ieee80211com_clear_flags(struct ieee80211com *ic, u_int32_t cap)
{
    ic->ic_flags &= ~cap;
    wlan_ic_psoc_clear_flag(ic, cap);
    wlan_ic_pdev_clear_flag(ic, cap);
}

static INLINE void
ieee80211com_clear_flags_ext(struct ieee80211com *ic, u_int32_t cap)
{
    ic->ic_flags_ext &= ~cap;
    wlan_ic_psoc_clear_extflag(ic, cap);
    wlan_ic_pdev_clear_extflag(ic, cap);
}

static INLINE uint32_t ieee80211com_get_target_supported_he_ht_ctrl
                                    (struct ieee80211com *ic) {
    uint32_t ic_hecap_mac;

    (void)qdf_get_u32(ic->ic_he.hecap_macinfo, &ic_hecap_mac);
    return HECAP_MAC_HECTRL_GET_FROM_IC(qdf_cpu_to_le32(ic_hecap_mac));
}
/*
 * Capabilities
 */
static INLINE void
ieee80211com_set_cap(struct ieee80211com *ic, u_int32_t cap)
{
    ic->ic_caps |= cap;
    wlan_ic_psoc_set_cap(ic, cap);
    wlan_ic_pdev_set_cap(ic, cap);
}

static INLINE void
ieee80211com_clear_cap(struct ieee80211com *ic, u_int32_t cap)
{
    ic->ic_caps &= ~cap;
    wlan_ic_psoc_clear_cap(ic, cap);
    wlan_ic_pdev_clear_cap(ic, cap);
}

static INLINE int
ieee80211com_has_cap(struct ieee80211com *ic, u_int32_t cap)
{
    return ((ic->ic_caps & cap) != 0);
}

int
ieee80211com_init_netlink(struct ieee80211com *ic);
/*
 * Extended Capabilities
 */
static INLINE void
ieee80211com_set_cap_ext(struct ieee80211com *ic, u_int32_t cap)
{
    ic->ic_caps_ext |= cap;
    wlan_ic_psoc_set_extcap(ic, cap);
    wlan_ic_pdev_set_extcap(ic, cap);
}

static INLINE void
ieee80211com_clear_cap_ext(struct ieee80211com *ic, u_int32_t cap)
{
    ic->ic_caps_ext &= ~cap;
    wlan_ic_psoc_clear_extcap(ic, cap);
    wlan_ic_pdev_clear_extcap(ic, cap);
}

static INLINE int
ieee80211com_has_cap_ext(struct ieee80211com *ic, u_int32_t cap_ext)
{
    return ((ic->ic_caps_ext & cap_ext) != 0);
}

/*
 * Cipher Capabilities
 */
static INLINE void
ieee80211com_set_ciphercap(struct ieee80211com *ic, u_int32_t ciphercap)
{
    ic->ic_cipher_caps |= ciphercap;
}

static INLINE void
ieee80211com_clear_ciphercap(struct ieee80211com *ic, u_int32_t ciphercap)
{
    ic->ic_cipher_caps &= ~ciphercap;
}

static INLINE int
ieee80211com_has_ciphercap(struct ieee80211com *ic, u_int32_t ciphercap)
{
    return ((ic->ic_cipher_caps & ciphercap) != 0);
}

/*
 * Atheros Capabilities
 */
static INLINE void
ieee80211com_set_athcap(struct ieee80211com *ic, u_int32_t athcap)
{
    ic->ic_ath_cap |= athcap;
}

static INLINE void
ieee80211com_clear_athcap(struct ieee80211com *ic, u_int32_t athcap)
{
    ic->ic_ath_cap &= ~athcap;
}

static INLINE int
ieee80211com_has_athcap(struct ieee80211com *ic, u_int32_t athcap)
{
    return ((ic->ic_ath_cap & athcap) != 0);
}

/*
 * Atheros State machine Roaming capabilities
 */


static INLINE void
ieee80211com_set_roaming(struct ieee80211com *ic, u_int8_t roaming)
{
    ic->ic_roaming = roaming;
}

static INLINE u_int8_t
ieee80211com_get_roaming(struct ieee80211com *ic)
{
    return ic->ic_roaming;
}

#if DA_SUPPORT
void wlan_pdev_set_athextcap(struct wlan_objmgr_pdev *pdev, u_int32_t val);
void wlan_pdev_clear_athextcap(struct wlan_objmgr_pdev *pdev, u_int32_t val);
#else
#define wlan_pdev_set_athextcap(pdev, val) /**/
#define wlan_pdev_clear_athextcap(pdev, val) /**/
#endif
/*
 * Atheros Extended Capabilities
 */
static INLINE void
ieee80211com_set_athextcap(struct ieee80211com *ic, u_int32_t athextcap)
{
    ic->ic_ath_extcap |= athextcap;
    wlan_pdev_set_athextcap(ic->ic_pdev_obj, athextcap);
}

static INLINE void
ieee80211com_clear_athextcap(struct ieee80211com *ic, u_int32_t athextcap)
{
    ic->ic_ath_extcap &= ~athextcap;
    wlan_pdev_clear_athextcap(ic->ic_pdev_obj, athextcap);
}

static INLINE int
ieee80211com_has_athextcap(struct ieee80211com *ic, u_int32_t athextcap)
{
    return ((ic->ic_ath_extcap & athextcap) != 0);
}

/* to check if node need, extra delimeter fix */
static INLINE int
ieee80211com_has_extradelimwar(struct ieee80211com *ic)
{
    return (ic->ic_ath_extcap & IEEE80211_ATHEC_EXTRADELIMWAR) ;
}

static INLINE int
ieee80211com_has_pn_check_war(struct ieee80211com *ic)
{
    return (ic->ic_ath_extcap & IEEE80211_ATHEC_PN_CHECK_WAR) ;
}


/*
 * 11n
 */
static INLINE void
ieee80211com_set_htcap(struct ieee80211com *ic, u_int16_t htcap)
{
    ic->ic_htcap |= htcap;
}

static INLINE void
ieee80211com_clear_htcap(struct ieee80211com *ic, u_int16_t htcap)
{
    ic->ic_htcap &= ~htcap;
}

static INLINE int
ieee80211com_has_htcap(struct ieee80211com *ic, u_int16_t htcap)
{
    return ((ic->ic_htcap & htcap) != 0);
}

static INLINE int
ieee80211com_get_ldpccap(struct ieee80211com *ic)
{
    return ic->ic_ldpccap;
}

static INLINE void
ieee80211com_set_ldpccap(struct ieee80211com *ic, u_int16_t ldpccap)
{
    if (ldpccap == 0) {
        ic->ic_ldpccap = 0;
        return;
    }

    if (ldpccap & IEEE80211_HTCAP_C_LDPC_TX)
        ic->ic_ldpccap |= IEEE80211_HTCAP_C_LDPC_TX;

    if (ldpccap & IEEE80211_HTCAP_C_LDPC_RX)
        ic->ic_ldpccap |= IEEE80211_HTCAP_C_LDPC_RX;
}

static INLINE void
ieee80211com_clear_ldpccap(struct ieee80211com *ic, u_int16_t ldpccap)
{
    if (ldpccap & IEEE80211_HTCAP_C_LDPC_TX)
        ic->ic_ldpccap &= ~IEEE80211_HTCAP_C_LDPC_TX;

    if (ldpccap & IEEE80211_HTCAP_C_LDPC_RX)
        ic->ic_ldpccap &= ~IEEE80211_HTCAP_C_LDPC_RX;
}

static INLINE void
ieee80211com_set_htextcap(struct ieee80211com *ic, u_int16_t htextcap)
{
    ic->ic_htextcap |= htextcap;
}

static INLINE void
ieee80211com_clear_htextcap(struct ieee80211com *ic, u_int16_t htextcap)
{
    ic->ic_htextcap &= ~htextcap;
}

static INLINE int
ieee80211com_has_htextcap(struct ieee80211com *ic, u_int16_t htextcap)
{
    return ((ic->ic_htextcap & htextcap) != 0);
}

static INLINE void
ieee80211com_set_htflags(struct ieee80211com *ic, u_int16_t htflags)
{
    ic->ic_htflags |= htflags;
}

static INLINE void
ieee80211com_clear_htflags(struct ieee80211com *ic, u_int16_t htflags)
{
    ic->ic_htflags &= ~htflags;
}

static INLINE int
ieee80211com_has_htflags(struct ieee80211com *ic, u_int16_t htflags)
{
    return ((ic->ic_htflags & htflags) != 0);
}

static INLINE void
ieee80211com_set_maxampdu(struct ieee80211com *ic, u_int8_t maxampdu)
{
    ic->ic_maxampdu = maxampdu;
}

static INLINE u_int8_t
ieee80211com_get_mpdudensity(struct ieee80211com *ic)
{
    return ic->ic_mpdudensity;
}

static INLINE void
ieee80211com_set_mpdudensity(struct ieee80211com *ic, u_int8_t mpdudensity)
{
    ic->ic_mpdudensity = mpdudensity;
}

static INLINE u_int8_t
ieee80211com_get_weptkipaggr_rxdelim(struct ieee80211com *ic)
{
    return (ic->ic_weptkipaggr_rxdelim);
}

static INLINE void
ieee80211com_set_weptkipaggr_rxdelim(struct ieee80211com *ic, u_int8_t weptkipaggr_rxdelim)
{
    ic->ic_weptkipaggr_rxdelim = weptkipaggr_rxdelim;
}

static INLINE u_int16_t
ieee80211com_get_channel_switching_time_usec(struct ieee80211com *ic)
{
    return (ic->ic_channelswitchingtimeusec);
}

static INLINE void
ieee80211com_set_channel_switching_time_usec(struct ieee80211com *ic, u_int16_t channel_switching_time_usec)
{
    ic->ic_channelswitchingtimeusec = channel_switching_time_usec;
}

/*
 * PHY type
 */
static INLINE enum ieee80211_phytype
ieee80211com_get_phytype(struct ieee80211com *ic)
{
    return ic->ic_phytype;
}

static INLINE void
ieee80211com_set_phytype(struct ieee80211com *ic, enum ieee80211_phytype phytype)
{
    ic->ic_phytype = phytype;
}

/*
 * 11ac
 */
static INLINE void
ieee80211com_set_vhtcap(struct ieee80211com *ic, u_int32_t vhtcap)
{
    ic->ic_vhtcap |= vhtcap;
}

static INLINE void
ieee80211com_clear_vhtcap(struct ieee80211com *ic, u_int32_t vhtcap)
{
    ic->ic_vhtcap &= ~vhtcap;
}

static INLINE int
ieee80211com_has_vhtcap(struct ieee80211com *ic, u_int32_t vhtcap)
{
    return ((ic->ic_vhtcap & vhtcap) != 0);
}

static INLINE void
ieee80211com_set_vht_mcs_map(struct ieee80211com *ic, u_int32_t mcs_map)
{
    ic->ic_vhtcap_max_mcs.rx_mcs_set.mcs_map =
    ic->ic_vhtcap_max_mcs.tx_mcs_set.mcs_map =
                                    (mcs_map & IEEE80211_VHT_LOWER_MCS_MAP);
    if(mcs_map & IEEE80211_VHT_MCS10_11_SUPP) {
        ic->ic_vhtcap_max_mcs.rx_mcs_set.higher_mcs_supp =
        ic->ic_vhtcap_max_mcs.tx_mcs_set.higher_mcs_supp =
                      ((mcs_map & IEEE80211_VHT_HIGHER_MCS_MAP) >>
                                        IEEE80211_VHT_HIGHER_MCS_S);
    }
}

static INLINE void
ieee80211com_set_vht_high_data_rate(struct ieee80211com *ic, u_int16_t datarate)
{
        ic->ic_vhtcap_max_mcs.rx_mcs_set.data_rate =
        ic->ic_vhtcap_max_mcs.tx_mcs_set.data_rate = datarate;
}

static INLINE void
ieee80211com_set_vhtop_basic_mcs_map(struct ieee80211com *ic, u_int16_t basic_mcs_map)
{
    ic->ic_vhtop_basic_mcs = basic_mcs_map;
}

static INLINE void
ieee80211vap_set_flag(struct ieee80211vap *vap, u_int32_t cap)
{
    vap->iv_flags |= cap;
    wlan_vap_vdev_set_flag(vap, cap);
}

static INLINE void
ieee80211vap_clear_flag(struct ieee80211vap *vap, u_int32_t cap)
{
    vap->iv_flags &= ~cap;
    wlan_vap_vdev_clear_flag(vap, cap);
}

static INLINE void
ieee80211vap_set_flag_ext(struct ieee80211vap *vap, u_int32_t cap)
{
    vap->iv_flags_ext |= cap;
    wlan_vap_vdev_set_extflag(vap, cap);
}

static INLINE void
ieee80211vap_clear_flag_ext(struct ieee80211vap *vap, u_int32_t cap)
{
    vap->iv_flags_ext &= ~cap;
    wlan_vap_vdev_clear_extflag(vap, cap);
}

static INLINE void
ieee80211vap_set_flag_ext2(struct ieee80211vap *vap, u_int32_t cap)
{
    vap->iv_flags_ext2 |= cap;
    wlan_vap_vdev_set_ext2flag(vap, cap);
}

static INLINE void
ieee80211vap_clear_flag_ext2(struct ieee80211vap *vap, u_int32_t cap)
{
    vap->iv_flags_ext2 &= ~cap;
    wlan_vap_vdev_clear_ext2flag(vap, cap);
}

static INLINE void
ieee80211vap_set_flag_ext22(struct ieee80211vap *vap, u_int32_t cap)
{
    vap->iv_flags_ext2 |= cap;
    wlan_vap_vdev_set_ext22flag(vap, cap);
}

static INLINE void
ieee80211vap_clear_flag_ext22(struct ieee80211vap *vap, u_int32_t cap)
{
    vap->iv_flags_ext2 &= ~cap;
    wlan_vap_vdev_clear_ext2flag(vap, cap);
}

static INLINE void
ieee80211vap_set_flag_ext222(struct ieee80211vap *vap, u_int32_t cap)
{
    vap->iv_flags_ext |= cap;
    wlan_vap_vdev_set_ext222flag(vap, cap);
}

static INLINE void
ieee80211vap_clear_flag_ext222(struct ieee80211vap *vap, u_int32_t cap)
{
    vap->iv_flags_ext &= ~cap;
    wlan_vap_vdev_clear_ext222flag(vap, cap);
}

static INLINE void
ieee80211vap_set_cap(struct ieee80211vap *vap, u_int32_t cap)
{
    vap->iv_caps |= cap;
    wlan_vap_vdev_set_cap(vap, cap);
}

static INLINE void
ieee80211vap_clear_cap(struct ieee80211vap *vap, u_int32_t cap)
{
    vap->iv_caps &= ~cap;
    wlan_vap_vdev_clear_cap(vap, cap);
}

static INLINE void
ieee80211vap_get_insctd_mcsnssmap(struct ieee80211vap *vap,
                   uint16_t rxmcsnssmap[HEHANDLE_CAP_TXRX_MCS_NSS_SIZE],
                   uint16_t txmcsnssmap[HEHANDLE_CAP_TXRX_MCS_NSS_SIZE]) {
    struct ieee80211com *ic  = vap->iv_ic;
    struct ieee80211_he_handle *ic_he = &ic->ic_he;

    if (vap->iv_he_tx_mcsnssmap != HE_INVALID_MCSNSSMAP) {
        /* User sends HE MCS-NSS info for less than equal to 80MHz encoded
         * in the lower 16 bits */
        txmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80]    =
                        HECAP_TXRX_MCS_NSS_GET_LT80_INFO(vap->iv_he_tx_mcsnssmap);
        /* User sends HE MCS-NSS info for greater than 80MHz encoded in the
         * upper 16 bits */
        txmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_160]   =
                        HECAP_TXRX_MCS_NSS_GET_GT80_INFO(vap->iv_he_tx_mcsnssmap);
        /* User sends HE MCS-NSS info for greater than 80MHz encoded in the
         * upper 16 bits */
        txmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80_80] =
                        HECAP_TXRX_MCS_NSS_GET_GT80_INFO(vap->iv_he_tx_mcsnssmap);

        /* intersect user configured values with target capabilities */
        txmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80]    =
            INTERSECT_11AX_MCSNSS_MAP(txmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80],
                    ic_he->hecap_txmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80]);

        txmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_160]    =
            INTERSECT_11AX_MCSNSS_MAP(txmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_160],
                    ic_he->hecap_txmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_160]);

        txmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80_80]    =
            INTERSECT_11AX_MCSNSS_MAP(txmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80_80],
                    ic_he->hecap_txmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80_80]);

    } else {
        txmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80] =
                        ic_he->hecap_txmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80];
        txmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_160] =
                        ic_he->hecap_txmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_160];
        txmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80_80] =
                        ic_he->hecap_txmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80_80];
    }

    if (vap->iv_he_rx_mcsnssmap != HE_INVALID_MCSNSSMAP) {
        /* User sends HE MCS-NSS info for less than equal to 80MHz encoded
         * in the lower 16 bits */
        rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80]    =
                        HECAP_TXRX_MCS_NSS_GET_LT80_INFO(vap->iv_he_rx_mcsnssmap);
        /* User sends HE MCS-NSS info for greater than 80MHz encoded in the
         * upper 16 bits */
        rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_160]   =
                        HECAP_TXRX_MCS_NSS_GET_GT80_INFO(vap->iv_he_rx_mcsnssmap);
        /* User sends HE MCS-NSS info for greater than 80MHz encoded in the
         * upper 16 bits */
        rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80_80] =
                        HECAP_TXRX_MCS_NSS_GET_GT80_INFO(vap->iv_he_rx_mcsnssmap);

        /* intersect user configured values with target capabilities */
        rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80]    =
            INTERSECT_11AX_MCSNSS_MAP(rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80],
                    ic_he->hecap_rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80]);

        rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_160]    =
            INTERSECT_11AX_MCSNSS_MAP(rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_160],
                    ic_he->hecap_rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_160]);

        rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80_80]    =
            INTERSECT_11AX_MCSNSS_MAP(rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80_80],
                    ic_he->hecap_rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80_80]);
    } else {
        rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80] =
                        ic_he->hecap_rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80];
        rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_160] =
                        ic_he->hecap_rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_160];
        rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80_80] =
                        ic_he->hecap_rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80_80];
    }
}

/**
* @brief        Checks if 160 MHz capability bit is set in vhtcap
*
* @param vhtcap: vhtcap info
*
* @return true if capability bit is set, false othersie.
*/
static INLINE bool ieee80211_get_160mhz_vhtcap(u_int32_t vhtcap)
{
    return ((vhtcap & IEEE80211_VHTCAP_SUP_CHAN_WIDTH_160) != 0);
}

/**
* @brief        Checks if 160 and 80+80 MHz capability bit is set in vhtcap
*
* @param vhtcap: vhtcap info
*
* @return true if capability bit is set, false othersie.
*/
static INLINE bool
ieee80211_get_80p80_160mhz_vhtcap(u_int32_t vhtcap)
{
    return ((vhtcap & IEEE80211_VHTCAP_SUP_CHAN_WIDTH_80_160) != 0);
}

/**
* @brief        Checks if 160 MHz short GI is enabled in vhtcap
*
* @param vhtcap: vhtcap info
*
* @return true if capability bit is set, false othersie.
*/
static INLINE bool
ieee80211_get_160mhz_shortgi_vhtcap(u_int32_t vhtcap)
{
    return ((vhtcap & IEEE80211_VHTCAP_SHORTGI_160) != 0);
}


/*
 * XXX these need to be here for IEEE80211_F_DATAPAD
 */

/*
 * Return the space occupied by the 802.11 header and any
 * padding required by the driver.  This works for a
 * management or data frame.
 */
static INLINE int
ieee80211_hdrspace(struct ieee80211com *ic, const void *data)
{
    int size = ieee80211_hdrsize(data);

    if (ic->ic_flags & IEEE80211_F_DATAPAD)
        size = roundup(size, sizeof(u_int32_t));

    return size;
}

/*
 * Like ieee80211_hdrspace, but handles any type of frame.
 */
static INLINE int
ieee80211_anyhdrspace(struct wlan_objmgr_pdev *pdev, const void *data)
{
    int size = ieee80211_anyhdrsize(data);
    const struct ieee80211_frame *wh = (const struct ieee80211_frame *)data;

    if ((wh->i_fc[0]&IEEE80211_FC0_TYPE_MASK) == IEEE80211_FC0_TYPE_CTL) {
        switch (wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) {
            case IEEE80211_FC0_SUBTYPE_CTS:
            case IEEE80211_FC0_SUBTYPE_ACK:
                return size;
        }
    }

    if (wlan_pdev_nif_feat_cap_get(pdev, WLAN_PDEV_F_DATAPAD))
        size = roundup(size, sizeof(u_int32_t));

    return size;
}

static INLINE struct wmeParams *
ieee80211com_wmm_chanparams(struct ieee80211com *ic, int ac)
{
    ASSERT(ac < WME_NUM_AC);
    return &ic->ic_wme.wme_chanParams.cap_wmeParams[ac];
}

/*
 * chainmask
 */
static INLINE void
ieee80211com_set_tx_chainmask(struct ieee80211com *ic, u_int8_t chainmask)
{
    ic->ic_tx_chainmask = chainmask;
    wlan_objmgr_update_txchainmask_to_allvdevs(ic);
}

static INLINE void
ieee80211com_set_rx_chainmask(struct ieee80211com *ic, u_int8_t chainmask)
{
    ic->ic_rx_chainmask = chainmask;
    wlan_objmgr_update_rxchainmask_to_allvdevs(ic);
}

static INLINE u_int8_t
ieee80211com_get_tx_chainmask(struct ieee80211com *ic)
{
    return ic->ic_tx_chainmask;
}

static INLINE u_int8_t
ieee80211com_get_rx_chainmask(struct ieee80211com *ic)
{
    return ic->ic_rx_chainmask;
}

static INLINE void
ieee80211com_set_spatialstreams(struct ieee80211com *ic, u_int8_t stream)
{
    ic->ic_spatialstreams = stream;
}

static INLINE u_int8_t
ieee80211com_get_spatialstreams(struct ieee80211com *ic)
{
    return ic->ic_spatialstreams;
}

static INLINE void
ieee80211com_set_num_tx_chain(struct ieee80211com *ic, u_int8_t num_chain)
{
    ic->ic_num_tx_chain = num_chain;
}

static INLINE void
ieee80211com_set_num_rx_chain(struct ieee80211com *ic, u_int8_t num_chain)
{
    ic->ic_num_rx_chain = num_chain;
}

#if ATH_SUPPORT_WAPI
static INLINE void
ieee80211com_set_wapi_max_tx_chains(struct ieee80211com *ic, u_int8_t num_chain)
{
    ic->ic_num_wapi_tx_maxchains = num_chain;
}

static INLINE void
ieee80211com_set_wapi_max_rx_chains(struct ieee80211com *ic, u_int8_t num_chain)
{
    ic->ic_num_wapi_rx_maxchains = num_chain;
}
#endif

static INLINE u_int16_t
ieee80211com_get_txpowerlimit(struct ieee80211com *ic)
{
    return ic->ic_txpowlimit;
}

static INLINE void
ieee80211com_set_txpowerlimit(struct ieee80211com *ic, u_int16_t txpowlimit)
{
    ic->ic_txpowlimit = txpowlimit;
}

/*
 * Channel
 */
static INLINE void
ieee80211com_set_curchanmaxpwr(struct ieee80211com *ic, u_int8_t maxpower)
{
    ic->ic_curchanmaxpwr = maxpower;
}

static INLINE u_int8_t
ieee80211com_get_curchanmaxpwr(struct ieee80211com *ic)
{
    return ic->ic_curchanmaxpwr;
}

static INLINE  struct ieee80211_ath_channel*
ieee80211com_get_curchan(struct ieee80211com *ic)
{
    return ic->ic_curchan;   /* current channel */
}

static INLINE void
ieee80211_set_tspecActive(struct ieee80211com *ic, u_int8_t val)
{
    ic->ic_tspec_active = val;
}

static INLINE int
ieee80211_is_tspecActive(struct ieee80211com *ic)
{
    return ic->ic_tspec_active;
}

static INLINE u_int32_t
ieee80211_get_tsf32(struct ieee80211com *ic)
{
    return ic->ic_get_TSF32(ic);
}

#if ATH_SUPPORT_WIFIPOS
static INLINE u_int64_t
ieee80211_get_tsftstamp(struct ieee80211com *ic)
{
    if(ic->ic_get_TSFTSTAMP) {
        return ic->ic_get_TSFTSTAMP(ic);
    }
    else {
        return 0;
    }
}
#endif

static INLINE int
ieee80211_get_bw_nss_mapping(struct ieee80211vap *vap, struct ieee80211_bwnss_map *nssmap, u_int8_t chainmask)
{
    struct ieee80211com *ic = vap->iv_ic;

    if(ic->ic_get_bw_nss_mapping)
        return ic->ic_get_bw_nss_mapping(vap, nssmap, chainmask);

    return -EINVAL;
}

/*
 * Pre-conditions for ForcePPM to be enabled.
 */
#define ieee80211com_can_enable_force_ppm(_ic)  0

/*
 * internal macro to iterate through vaps.
 */
#if ATH_SUPPORT_AP_WDS_COMBO
#define IEEE80211_MAX_VAPS 16
#elif ATH_SUPPORT_WRAP
#define IEEE80211_MAX_VAPS 32
#elif ATH_PERF_PWR_OFFLOAD
#define IEEE80211_MAX_VAPS 17
#else
#define IEEE80211_MAX_VAPS 16
#endif
/*
 * Need nt_nodelock since iv_bss could have changed.
 * TBD: make ic_lock a read/write lock to reduce overhead in input_all
 */
#define ieee80211_iterate_vap_list_internal(ic,iter_func,arg,vaps_count)             \
do {                                                                                 \
    struct ieee80211vap *_vap;                                                       \
    struct ieee80211_node * ni;                                                      \
    struct ieee80211_node *bss_node[IEEE80211_MAX_VAPS];                             \
    u_int16_t    idx;                                                                \
    vaps_count=0;                                                                    \
    IEEE80211_COMM_LOCK(ic);                                                         \
    TAILQ_FOREACH(_vap, &ic->ic_vaps, iv_next) {                                     \
        if (ieee80211_vap_deleted_is_set(_vap))                                      \
            continue;                                                                \
        if (vaps_count >= IEEE80211_MAX_VAPS) {                                      \
            IEEE80211_DPRINTF(_vap, IEEE80211_MSG_DEBUG,                             \
                    "Max Vap count %s reached\n", __func__);                         \
            break;                                                                   \
        }                                                                            \
        if ((ni = ieee80211_try_ref_bss_node(_vap)) != NULL)                         \
            bss_node[vaps_count++] = ni;                                             \
    }                                                                                \
    IEEE80211_COMM_UNLOCK(ic);                                                       \
    for (idx=0; idx<vaps_count; ++idx) {                                             \
         if (bss_node[idx]) {                                                        \
             iter_func(arg, bss_node[idx]->ni_vap, (idx == (vaps_count -1)));        \
             ieee80211_free_node(bss_node[idx]);                                     \
        }                                                                            \
    }                                                                                \
} while(0)


/*
 * Key update synchronization methods.  XXX should not be visible.
 */
static INLINE void
ieee80211_key_update_begin(struct ieee80211vap *vap)
{
}
static INLINE void
ieee80211_key_update_end(struct ieee80211vap *vap)
{
}

/*
 * Return the bssid of a frame.
 */
static INLINE const u_int8_t *
ieee80211vap_getbssid(struct ieee80211vap *vap, const struct ieee80211_frame *wh)
{
    if (vap->iv_opmode == IEEE80211_M_STA)
        return wh->i_addr2;
    if ((wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) != IEEE80211_FC1_DIR_NODS)
        return wh->i_addr1;
    if ((wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) == IEEE80211_FC0_SUBTYPE_PS_POLL)
        return wh->i_addr1;
    return wh->i_addr3;
}

/*
 * Operation Mode
 */
static INLINE enum ieee80211_opmode
ieee80211vap_get_opmode(struct ieee80211vap *vap)
{
    return vap->iv_opmode;
}

/*
 * Misc
 */

static INLINE struct ieee80211_node *
ieee80211vap_get_bssnode(struct ieee80211vap *vap)
{
    return vap->iv_bss;
}

static INLINE void
ieee80211vap_get_macaddr(struct ieee80211vap *vap, u_int8_t *macaddr)
{
    /* the same as IC for extensible STA mode */
    IEEE80211_ADDR_COPY(macaddr, vap->iv_myaddr);
}

static INLINE void
ieee80211vap_set_macaddr(struct ieee80211vap *vap, const u_int8_t *macaddr)
{
    /* Normally shouldn't be called for a station */
    IEEE80211_ADDR_COPY(vap->iv_myaddr, macaddr);
}

static INLINE void *
ieee80211vap_get_private_data(struct ieee80211vap *vap)
{
    return vap->iv_priv;
}

static INLINE void
ieee80211vap_set_private_data(struct ieee80211vap *vap , void *priv_data)
{
    vap->iv_priv = priv_data;
}

static INLINE IEEE80211_VEN_IE *
ieee80211vap_get_venie(struct ieee80211vap *vap)
{
    return vap->iv_venie;
}

static INLINE IEEE80211_VEN_IE *
ieee80211vap_init_venie(struct ieee80211vap *vap)
{
    vap->iv_venie = (IEEE80211_VEN_IE *) OS_MALLOC(vap->iv_ic->ic_osdev,
     					sizeof(IEEE80211_VEN_IE), GFP_KERNEL);
    return vap->iv_venie;
}

static INLINE void ieee80211vap_delete_venie(struct ieee80211vap *vap)
{
    if(vap->iv_venie) {
        OS_FREE(vap->iv_venie);
        vap->iv_venie = NULL;
    }
}

/**
 * set(register) input filter management function callback.
 * @param vap                        : pointer to vap
 * @param mgmt_filter_function       : input management filter function calback.
 * @return the value of old filter function.
 *  the input management function is called for every received management frame.
 *  if the call back function returns true frame will be dropped.
 *  if the call back function returns false then hte frame will be passed down to mlme.
 * *** NOTE: the call back function is called even if the vap is not active.
 */
static INLINE  ieee80211_vap_input_mgmt_filter
ieee80211vap_set_input_mgmt_filter(struct ieee80211vap *vap , ieee80211_vap_input_mgmt_filter mgmt_filter_func)
{
     ieee80211_vap_input_mgmt_filter old_filter=vap->iv_input_mgmt_filter;
     vap->iv_input_mgmt_filter = mgmt_filter_func;
     return old_filter;
}

/**
 * set(register) output filter management function callback.
 * @param vap                        : pointer to vap
 * @param mgmt_filter_function       : output management filter function calback.
 * @return the value of old filter function.
 *  the output management function is called for every transimitted management frame.
 *   just before handing over the frame to lmac.
 *  if the call back function returns true frame will be dropped.
 *  if the call back function returns false then hte frame will be passed down to lmac.
 */
static INLINE  ieee80211_vap_output_mgmt_filter
ieee80211vap_set_output_mgmt_filter_func(struct ieee80211vap *vap , ieee80211_vap_output_mgmt_filter mgmt_output_filter_func)
{
     ieee80211_vap_output_mgmt_filter old_func=vap->iv_output_mgmt_filter;
     vap->iv_output_mgmt_filter = mgmt_output_filter_func;
     return old_func;
}

static INLINE int
ieee80211vap_has_pssta(struct ieee80211vap *vap)
{
    return (vap->iv_ps_sta != 0);
}

static INLINE bool is_weptkip_htallowed(struct ieee80211vap *vap, struct ieee80211_node *ni)
{
    if (!(vap || ni))
	return 0;
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
    if (vap)
        return wlan_crypto_is_htallowed(vap->vdev_obj, NULL);
    else
        return wlan_crypto_is_htallowed(NULL, ni->peer_obj);
#else
    {
        struct ieee80211_rsnparms *rsnparam;

        if (vap)
            rsnparam = &vap->iv_rsn;
        else
            rsnparam = &ni->ni_rsn;
        return (RSN_CIPHER_IS_WEP(rsnparam)
               || (RSN_CIPHER_IS_TKIP(rsnparam)
               && !RSN_CIPHER_IS_CCMP128(rsnparam)
               && !RSN_CIPHER_IS_CCMP256(rsnparam)
               && !RSN_CIPHER_IS_GCMP128(rsnparam)
               && !RSN_CIPHER_IS_GCMP256(rsnparam)));
    }
#endif
}

/*
 * With WEP and TKIP encryption algorithms:
 * Disable 11n if IEEE80211_FEXT_WEP_TKIP_HTRATE is not set.
 * Check for Mixed mode, if CIPHER is set to TKIP
 */
static INLINE int
ieee80211vap_htallowed(struct ieee80211vap *vap)
{
    struct ieee80211com *ic = vap->iv_ic;

    /* Disable HT if WMM/wme is disabled */
    if (!ieee80211_vap_wme_is_set(vap)) {
        return 0;
    }

    switch (vap->iv_cur_mode) {
    case IEEE80211_MODE_11A:
    case IEEE80211_MODE_11B:
    case IEEE80211_MODE_11G:
    case IEEE80211_MODE_FH:
    case IEEE80211_MODE_TURBO_A:
    case IEEE80211_MODE_TURBO_G:
        return 0;
    default:
        break;
    }

    if (!ieee80211_ic_wep_tkip_htrate_is_set(ic) &&
        IEEE80211_VAP_IS_PRIVACY_ENABLED(vap) &&
        is_weptkip_htallowed(vap, NULL))
        return 0;
    else if (vap->iv_opmode == IEEE80211_M_IBSS)
        return (ieee80211_ic_ht20Adhoc_is_set(ic) || ieee80211_ic_ht40Adhoc_is_set(ic));
    else
        return 1;
}

static INLINE int
ieee80211vap_vhtallowed(struct ieee80211vap *vap)
{
    struct ieee80211com *ic = vap->iv_ic;

    /* Don't allow VHT if HT is not allowed */
    if (!ieee80211vap_htallowed(vap)){
        return 0;
    }

    /* Don't allow VHT if mode is HT only  */
    switch (vap->iv_cur_mode) {
        case IEEE80211_MODE_11NA_HT20:
        case IEEE80211_MODE_11NA_HT40PLUS:
        case IEEE80211_MODE_11NA_HT40MINUS:
            return 0;
        break;
        case IEEE80211_MODE_11NG_HT20:
        case IEEE80211_MODE_11NG_HT40PLUS:
        case IEEE80211_MODE_11NG_HT40MINUS:
       case IEEE80211_MODE_11AXG_HE20:
       case IEEE80211_MODE_11AXG_HE40PLUS:
       case IEEE80211_MODE_11AXG_HE40MINUS:
       case IEEE80211_MODE_11AXG_HE40:
             /*VHT is allowed in 2G if 256 QAM is supported */
           if(!ieee80211_vap_256qam_is_set(vap))
            return 0;

        default:
            break;
    }

    if (ic->ic_vhtcap) {
        return 1;
    }

    return 0;
}

static INLINE u_int8_t
ieee80211vap_11ng_vht_interopallowed (struct ieee80211vap *vap)
{
    return ieee80211_vap_11ng_vht_interop_is_set(vap);
}

static INLINE bool
ieee80211vap_ishemode(struct ieee80211vap *vap)
{
    if (vap->iv_des_mode >= IEEE80211_MODE_11AXA_HE20)
        return true;

    return false;
}

static INLINE int
ieee80211vap_heallowed(struct ieee80211vap *vap)
{
    struct ieee80211com *ic = vap->iv_ic;

    /* Don't allow HE if VHT & HT is not allowed */
    if (!ieee80211vap_htallowed(vap)){
        return 0;
    }

    switch (vap->iv_cur_mode) {
        case IEEE80211_MODE_11AXA_HE20:
        case IEEE80211_MODE_11AXA_HE40PLUS:
        case IEEE80211_MODE_11AXA_HE40MINUS:
        case IEEE80211_MODE_11AXA_HE40:
        case IEEE80211_MODE_11AXA_HE80:
        case IEEE80211_MODE_11AXA_HE160:
        case IEEE80211_MODE_11AXA_HE80_80:
            if (!ieee80211vap_vhtallowed(vap)){
                return 0;
            }
            break;
        default:
            break;
    }

    switch (vap->iv_cur_mode) {
        case IEEE80211_MODE_11AC_VHT20:
        case IEEE80211_MODE_11AC_VHT40PLUS:
        case IEEE80211_MODE_11AC_VHT40MINUS:
        case IEEE80211_MODE_11AC_VHT40:
        case IEEE80211_MODE_11AC_VHT80:
        case IEEE80211_MODE_11AC_VHT160:
        case IEEE80211_MODE_11AC_VHT80_80:
            return 0;
            break;

        default:
            break;
    }

    if (IEEE80211_IS_HECAP_MACINFO(ic->ic_he.hecap_macinfo)) {
        return 1;
    }

    return 0;
}

static INLINE bool
ieee80211vap_muedca_is_enabled(struct ieee80211vap *vap)
{
    if(vap->iv_he_muedca) {
        return 1;
    }

    return 0;
}

/*
 * Atheros Capabilities
 */
static INLINE void
ieee80211vap_set_athcap(struct ieee80211vap *vap, u_int32_t athcap)
{
    vap->iv_ath_cap |= athcap;
}

static INLINE void
ieee80211vap_clear_athcap(struct ieee80211vap *vap, u_int32_t athcap)
{
    vap->iv_ath_cap &= ~athcap;
}

static INLINE int
ieee80211vap_has_athcap(struct ieee80211vap *vap, u_int32_t athcap)
{
    return ((vap->iv_ath_cap & athcap) != 0);
}

static INLINE void
ieee80211vap_set_htflags(struct ieee80211vap *vap, u_int16_t htflags)
{
    vap->iv_htflags |= htflags;
}

static INLINE void
ieee80211vap_clear_htflags(struct ieee80211vap *vap, u_int16_t htflags)
{
    vap->iv_htflags &= ~htflags;
}

static INLINE int
ieee80211vap_has_htflags(struct ieee80211vap *vap, u_int16_t htflags)
{
    return ((vap->iv_htflags & htflags) != 0);
}

#define printf  qdf_info

#define CHK_IEEE80211_MSG_BASE(_ctxt, _prefix, _m)          \
    ((_ctxt)->_prefix.category_mask[(_m) >> 3] &            \
     (1 << ((_m) & 0x7)))
#define ieee80211_msg_ic(_ic, _m)               \
    CHK_IEEE80211_MSG_BASE(_ic, ic_print, _m)
#define ieee80211_msg(_vap, _m)                 \
    CHK_IEEE80211_MSG_BASE(_vap, iv_print, _m)
#define ieee80211_msg_dumppkts(_vap) \
        ieee80211_msg(_vap, IEEE80211_MSG_DUMPPKTS)
/*
 * if os does not define the
 * debug temp buf size, define
 * a default size.
 */
#ifndef OS_TEMP_BUF_SIZE
#define OS_TEMP_BUF_SIZE 256
#endif

#define SCANREQ_GIVE_ALL_SCAN_ENTRIES 0
#define SCANREQ_GIVE_ONLY_DESSIRED_SSID 1
#define SCANREQ_GIVE_EXCEPT_DESSIRED_SSID 2

struct scanreq
{
    struct ieee80211req_scan_result *sr;
    size_t space;
    struct ieee80211vap *vap;
    u_int8_t scanreq_type;
};

struct stainforeq
{
    wlan_if_t vap;
    struct ieee80211req_sta_info *si;
    size_t  space;
};


void ieee80211com_note(struct ieee80211com *ic, int msgtype, const char *fmt, ...);
void ieee80211_note(struct ieee80211vap *vap, int msgtype, const char *fmt, ...);
void ieee80211_note_frame(struct ieee80211vap *vap, int msgtype,
                                    struct ieee80211_frame *wh,const  char *fmt, ...);
void ieee80211_note_mac(struct ieee80211vap *vap, int msgtype, u_int8_t *mac, const  char *fmt, ...);
void ieee80211_discard_ie(struct ieee80211vap *vap, int msgtype, const char *type, const char *fmt, ...);
void ieee80211_discard_frame(struct ieee80211vap *vap, int msgtype,
                             const struct ieee80211_frame *wh,
                             const char *type, const char *fmt, ...);
void ieee80211_discard_mac(struct ieee80211vap *vap, int msgtype, u_int8_t *mac,
                           const char *type, const char *fmt, ...);

int ieee80211_set_igtk_key(struct ieee80211vap *vap, u_int16_t keyix, ieee80211_keyval *kval);

int ieee80211_cmac_calc_mic(struct ieee80211_key *key, u_int8_t *aad,
                                  u_int8_t *pkt, u_int32_t pktlen , u_int8_t *mic);

int ieee80211_gmac_calc_mic(struct ieee80211_key *key, u_int8_t *aad,
                                  u_int8_t *pkt, u_int32_t pktlen , u_int8_t *mic,u_int8_t *nounce);

uint16_t ieee80211_getCurrentCountry(struct ieee80211com *ic);

uint16_t ieee80211_getCurrentCountryISO(struct ieee80211com *ic, char *str);

uint16_t ieee80211_get_regdomain(struct ieee80211com *ic);

void ieee80211_set_country_code_assoc_sm(void *data);

int ieee80211_set_ctry_code_continue(struct ieee80211com *ic);

/**
 * ieee80211_regdmn_get_chan_params() - Get channel related parameters.
 * @ic: Pointer to ieee80211com structure.
 * @ch_params: Pointer to channel params structure.
 */
void ieee80211_regdmn_get_chan_params(struct ieee80211com *ic,
        struct ch_params *ch_params);

/**
 * ieee80211_regdmn_get_des_chan_params() - Get channel related parameters.
 * @ic: Pointer to ieee80211vap structure.
 * @ch_params: Pointer to channel params structure.
 */
void ieee80211_regdmn_get_des_chan_params(struct ieee80211vap *vap,
        struct ch_params *ch_params);

/**
 * ieee80211_get_target_channel_mode() - Get phymode from the target channel
 * @ic: Pointer to ieee80211com structure.
 * @ch_params: Pointer to channel params structure.
 *
 * Return: Return phymode.
 */
enum ieee80211_phymode ieee80211_get_target_channel_mode(
        struct ieee80211com *ic,
        struct ch_params *ch_params);

/**
 * ieee80211_dfs_reset - Reset DFS structure members.
 * @ic: Pointer to ic structure
 */
void ieee80211_dfs_reset(struct ieee80211com *ic);

extern void
ieee80211_set_vht_rates(struct ieee80211com *ic, struct ieee80211vap  *vap);

/* UMAC global category bit -> name translation */
static const struct asf_print_bit_spec ieee80211_msg_categories[] = {
    {IEEE80211_MSG_ACS, "ACS"},
    {IEEE80211_MSG_SCAN_SM, "scan state machine"},
    {IEEE80211_MSG_SCANENTRY, "scan entry"},
    {IEEE80211_MSG_WDS, "WDS"},
    {IEEE80211_MSG_ACTION, "action"},
    {IEEE80211_MSG_ROAM, "STA roaming"},
    {IEEE80211_MSG_INACT, "inactivity"},
    {IEEE80211_MSG_DOTH, "11h"},
    {IEEE80211_MSG_IQUE, "IQUE"},
    {IEEE80211_MSG_WME, "WME"},
    {IEEE80211_MSG_ACL, "ACL"},
    {IEEE80211_MSG_WPA, "WPA/RSN"},
    {IEEE80211_MSG_RADKEYS, "dump 802.1x keys"},
    {IEEE80211_MSG_RADDUMP, "dump radius packet"},
    {IEEE80211_MSG_RADIUS, "802.1x radius client"},
    {IEEE80211_MSG_DOT1XSM, "802.1x state machine"},
    {IEEE80211_MSG_DOT1X, "802.1x authenticator"},
    {IEEE80211_MSG_POWER, "power save"},
    {IEEE80211_MSG_STATE, "state"},
    {IEEE80211_MSG_OUTPUT, "output"},
    {IEEE80211_MSG_SCAN, "scan"},
    {IEEE80211_MSG_AUTH, "authentication"},
    {IEEE80211_MSG_ASSOC, "association"},
    {IEEE80211_MSG_NODE, "node"},
    {IEEE80211_MSG_ELEMID, "element ID"},
    {IEEE80211_MSG_XRATE, "rate"},
    {IEEE80211_MSG_INPUT, "input"},
    {IEEE80211_MSG_CRYPTO, "crypto"},
    {IEEE80211_MSG_DUMPPKTS, "dump packet"},
    {IEEE80211_MSG_DEBUG, "debug"},
    {IEEE80211_MSG_MLME, "mlme"},
};

void IEEE80211_DPRINTF_IC(struct ieee80211com *ic, unsigned verbose, unsigned category, const char *
fmt, ...);

void IEEE80211_DPRINTF_IC_CATEGORY(struct ieee80211com *ic, unsigned category, const char *fmt, ...);

#define ieee80211_dprintf_ic_init(_ic)                                  \
    do {                                                                \
        (_ic)->ic_print.name = "IEEE80211_IC";                          \
        (_ic)->ic_print.verb_threshold = IEEE80211_VERBOSE_LOUD;        \
        (_ic)->ic_print.num_bit_specs =                                 \
            IEEE80211_MSG_LIST_LENGTH(ieee80211_msg_categories);        \
        (_ic)->ic_print.bit_specs = ieee80211_msg_categories;           \
        (_ic)->ic_print.custom_ctxt = NULL;                             \
        (_ic)->ic_print.custom_print = NULL;                            \
        asf_print_mask_set(&(_ic)->ic_print, IEEE80211_DEBUG_DEFAULT, 1); \
        if (IEEE80211_DEBUG_DEFAULT < ASF_PRINT_MASK_BITS) {            \
            asf_print_mask_set(&(_ic)->ic_print, IEEE80211_MSG_ANY, 1); \
        }                                                               \
        asf_print_ctrl_register(&(_ic)->ic_print);                      \
    } while (0)
#define ieee80211_dprintf_ic_deregister(_ic)    \
    asf_print_ctrl_unregister(&(_ic)->ic_print)

void IEEE80211_DPRINTF(struct ieee80211vap *vap, unsigned category, const char *
fmt, ...);

void IEEE80211_DPRINTF_VB(struct ieee80211vap *vap, unsigned verbose, unsigned category, const char *
fmt, ...);

#define ieee80211_dprintf_init(_vap)                                    \
    do {                                                                \
        (_vap)->iv_print.name = "IEEE80211";                            \
        (_vap)->iv_print.verb_threshold = IEEE80211_VERBOSE_LOUD;      \
        (_vap)->iv_print.num_bit_specs =                                \
            IEEE80211_MSG_LIST_LENGTH(ieee80211_msg_categories);        \
        (_vap)->iv_print.bit_specs = ieee80211_msg_categories;          \
        (_vap)->iv_print.custom_ctxt = NULL;                            \
        (_vap)->iv_print.custom_print = NULL;                           \
        asf_print_mask_set(&(_vap)->iv_print, IEEE80211_DEBUG_DEFAULT, 1); \
        if (IEEE80211_DEBUG_DEFAULT < ASF_PRINT_MASK_BITS) {            \
            asf_print_mask_set(&(_vap)->iv_print, IEEE80211_MSG_ANY, 1); \
        }                                                               \
        asf_print_ctrl_register(&(_vap)->iv_print);                     \
    } while (0)
#define ieee80211_dprintf_deregister(_vap)          \
    asf_print_ctrl_unregister(&(_vap)->iv_print)

#define IEEE80211_MSG_LIST_LENGTH(_list) ARRAY_LENGTH(_list)


#if ATH_DEBUG
#if DBG_LVL_MAC_FILTERING
#define IEEE80211_NOTE(_vap, _m, _ni, _fmt, ...) do {                   \
        if (ieee80211_msg(_vap, _m)) {                                   \
            if (!(_vap)->iv_print.dbgLVLmac_on) {                        \
                ieee80211_note_mac(_vap, _m, (_ni)->ni_macaddr, _fmt, ##__VA_ARGS__); \
            } else if ((_ni)->ni_dbgLVLmac_on) {                        \
                ieee80211_note_mac(_vap, _m, (_ni)->ni_macaddr, _fmt, ##__VA_ARGS__); \
            }                                                          \
        }                                                              \
    } while (0)
#define IEEE80211_NOTE_MAC(_vap, _m, _mac, _fmt, ...) do {              \
        if (ieee80211_msg(_vap, _m) && (!(_vap)->iv_print.dbgLVLmac_on)) \
            ieee80211_note_mac(_vap, _m,  _mac, _fmt, ##__VA_ARGS__);        \
    } while (0)
#else
#define IEEE80211_NOTE(_vap, _m, _ni, _fmt, ...) do {                   \
        if (ieee80211_msg(_vap, _m))                                   \
                ieee80211_note_mac(_vap, _m, (_ni)->ni_macaddr, _fmt, ##__VA_ARGS__); \
    } while (0)
#define IEEE80211_NOTE_MAC(_vap, _m, _mac, _fmt, ...) do {              \
        if (ieee80211_msg(_vap, _m)) 					\
            ieee80211_note_mac(_vap, _m,  _mac, _fmt, ##__VA_ARGS__);        \
    } while (0)
#endif /*DBG_LVL_MAC_FILTERING*/
#define IEEE80211_NOTE_FRAME(_vap, _m, _wh, _fmt, ...) do {             \
        if (ieee80211_msg(_vap, _m))                                    \
            ieee80211_note_frame(_vap, _m, _wh, _fmt, ##__VA_ARGS__);       \
    } while (0)
#else
#define IEEE80211_NOTE(_vap, _m, _ni, _fmt, ...)
#define IEEE80211_NOTE_MAC(_vap, _m, _mac, _fmt, ...)
#define IEEE80211_NOTE_FRAME(_vap, _m, _wh, _fmt, ...)
#endif /* ATH_DEBUG */

#define IEEE80211_DISCARD(_vap, _m, _wh, _type, _fmt, ...) do {         \
    if (ieee80211_msg((_vap), (_m)))                                    \
        ieee80211_discard_frame(_vap, _m, _wh, _type, _fmt, __VA_ARGS__);   \
    } while (0)
#define IEEE80211_DISCARD_MAC(_vap, _m, _mac, _type, _fmt, ...) do {    \
    if (ieee80211_msg((_vap), (_m)))                                    \
        ieee80211_discard_mac(_vap, _m, _mac, _type, _fmt, __VA_ARGS__);    \
    } while (0)
#define IEEE80211_DISCARD_IE(_vap, _m, _type, _fmt, ...) do {           \
    if (ieee80211_msg((_vap), (_m)))                                    \
        ieee80211_discard_ie(_vap, _m,  _type, _fmt, __VA_ARGS__);           \
    } while (0)

#ifdef ATH_CWMIN_WORKAROUND

#else
#define IEEE80211_VI_NEED_CWMIN_WORKAROUND_INIT(_v)
#define VAP_NEED_CWMIN_WORKAROUND(_v) (0)
#endif /* #ifdef ATH_CWMIN_WORKAROUND */

#if ATH_SUPPORT_DFS && ATH_SUPPORT_STA_DFS
#define IEEE80211_IC_DFS_DFSMASTER    0x01 /* DFS Master */
#define IEEE80211_IC_DFS_RADARCAPABLE 0x02 /* Radar Capable */
#define IEEE80211_IC_DFS_CACREQUIRED  0x04 /* CAC (Channel Availability Check) Required */

#define IEEE80211_IC_DFS_DFSMASTER_SET(_ic)         ((_ic)->ic_dfs_flags |=  IEEE80211_IC_DFS_DFSMASTER)
#define IEEE80211_IC_DFS_DFSMASTER_CLR(_ic)         ((_ic)->ic_dfs_flags &= ~IEEE80211_IC_DFS_DFSMASTER)
#define IEEE80211_IC_DFS_DFSMASTER_IS_SET(_ic)      ((_ic)->ic_dfs_flags &   IEEE80211_IC_DFS_DFSMASTER)

#define IEEE80211_IC_DFS_RADARCAPABLE_SET(_ic)      ((_ic)->ic_dfs_flags |=  IEEE80211_IC_DFS_RADARCAPABLE)
#define IEEE80211_IC_DFS_RADARCAPABLE_CLR(_ic)      ((_ic)->ic_dfs_flags &= ~IEEE80211_IC_DFS_RADARCAPABLE)
#define IEEE80211_IC_DFS_RADARCAPABLE_IS_SET(_ic)   ((_ic)->ic_dfs_flags &   IEEE80211_IC_DFS_RADARCAPABLE)

#define IEEE80211_IC_DFS_CACREQUIRED_SET(_ic)       ((_ic)->ic_dfs_flags |=  IEEE80211_IC_DFS_CACREQUIRED)
#define IEEE80211_IC_DFS_CACREQUIRED_CLR(_ic)       ((_ic)->ic_dfs_flags &= ~IEEE80211_IC_DFS_CACREQUIRED)
#define IEEE80211_IC_DFS_CACREQUIRED_IS_SET(_ic)    ((_ic)->ic_dfs_flags &   IEEE80211_IC_DFS_CACREQUIRED)
#endif

#define IEEE80211_MAX_32BIT_UNSIGNED_VALUE          (0xFFFFFFFFU)

#if DBDC_REPEATER_SUPPORT
#define ROOTAP_ACCESS_MASK 0x0F
#define STAVAP_CONNECTION_MASK 0xF0
#endif

/**
 * ieee80211_dfs_rebuild_chan_list_with_non_dfs_channels() - Rebuild channel
 * list with non-DFS channels.
 * @ic: Pointer to ieee80211com structure.
 *
 * Return: 1 or 0 on success else failure.
 */
#if defined(WLAN_DFS_PARTIAL_OFFLOAD) && defined(HOST_DFS_SPOOF_TEST)
int ieee80211_dfs_rebuild_chan_list_with_non_dfs_channels(struct ieee80211com *ic);
#endif /* HOST_DFS_SPOOF_TEST */

void ieee80211_csa_interop_phy_update(struct ieee80211_node *ni, int rx_bw);

typedef enum event_type {
    TYPE_SENT_EVENT = 0,
    TYPE_MSG_RESP   = 1,
} event_type;

struct nl_event_info{
    int type;
    enum event_type ev_type;
    uint32_t ev_len;
    uint32_t pid;
    uint32_t seq;
    uint32_t flags;
    void *event;
};

int nl_ev_fill_info(struct sk_buff *skb, struct net_device *dev,
                    struct nl_event_info *info);
#endif /* end of _ATH_STA_IEEE80211_VAR_H */
