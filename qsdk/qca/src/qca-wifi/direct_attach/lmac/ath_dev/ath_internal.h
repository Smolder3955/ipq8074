/*
 * Copyright (c) 2011,2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * Copyright (c) 2010, Atheros Communications Inc.
 * All Rights Reserved.
 */

/*
 * Definitions for the ATH layer internal API's.
 */
#ifndef ATH_INTERNAL_H
#define ATH_INTERNAL_H

#include <ah_osdep.h>
#include <osdep.h>
#include <wlan_opts.h>

#include <if_athioctl.h>
#include <if_athrate.h>
#include <ath_desc.h>
#include <asf_print.h>
#if UNIFIED_SMARTANTENNA
#include <wlan_sa_api_utils_defs.h>
#endif

#ifdef UNINET
#include <net80211/ieee80211.h>
#include <net80211/ieee80211_var.h>
#include <uninet_lmac_common.h>
#else
#include <umac_lmac_common.h>
#include <ieee80211.h>
#include <ieee80211_api.h>
#include <ieee80211_defines.h>
#endif

#include "ath_dev.h"
#include "ath_led.h"
#include "ath_power.h"
#include "ath_rfkill.h"
#include "ath_ppm.h"

#if WLAN_SPECTRAL_ENABLE
#include "../spectral/spectral.h"
#endif

#if (ATH_SLOW_ANT_DIV || ATH_ANT_DIV_COMB)
#include "ath_antdiv.h"
#endif
#ifdef ATH_BT_COEX
#include "ath_bt.h"
#endif
#ifdef ATH_GEN_TIMER
#include "ath_hwtimer.h"
#endif
#include "ath_ht.h"
#include "ath_edma.h"
#ifdef ATH_SUPPORT_TxBF
#include "ath_txbf.h"
#endif
#include "ath_htc_wmi.h"
#include "ath_usb.h"
#include "ath_notify_tx_bcn.h"
#include "ath_vap_info.h"
#include "ath_vap_pause.h"

#include "ieee80211_var.h"

#if ATH_SUPPORT_HYFI_ENHANCEMENTS
#include "ath_ald.h"
#endif

#if ATH_SUPPORT_MCI
#include "ath_mci.h"
#endif

#if LMAC_SUPPORT_POWERSAVE_QUEUE
struct ath_node_pwrsaveq;
#include "ath_legacy_ps.h"
#endif

#if ATH_LOW_PRIORITY_CALIBRATE
#include "ath_defer.h"
#endif

#define ATH_RX_HANG_CHECK_MS             1000
#define IF_ID_DIRECT_ATTACH (0)
#if WLAN_SPECTRAL_ENABLE
struct ath_spectral;
typedef struct ath_spectral_ops {
   u_int64_t (*get_tsf64)(void* arg);
   u_int32_t (*get_capability)(void* arg, enum spectral_capability_type type);

   u_int32_t (*set_rxfilter)(void* arg, int rxfilter);
   u_int32_t (*get_rxfilter)(void* arg);

   u_int32_t (*is_spectral_active)(void* arg);
   u_int32_t (*is_spectral_enabled)(void* arg);

   u_int32_t (*start_spectral_scan)(void* arg);
   u_int32_t (*stop_spectral_scan)(void* arg);

   u_int32_t (*get_extension_channel)(void* arg);

   int8_t (*get_ctl_noisefloor)(void* arg);
   int8_t (*get_ext_noisefloor)(void* arg);

   u_int32_t (*configure_spectral)(void* arg, HAL_SPECTRAL_PARAM *params);
   u_int32_t (*get_spectral_config)(void* arg, HAL_SPECTRAL_PARAM *params);

   u_int32_t (*get_ent_spectral_mask)(void* arg);

   u_int32_t (*get_mac_address)(void* arg, char* addr);
   u_int32_t (*get_current_channel)(void* arg);
   u_int32_t (*reset_hw)(void* arg);
   u_int32_t (*get_chain_noise_floor)(void* arg, int16_t* nfBuf);
}ATH_SPECTRAL_OPS;

#undef not_yet_implemented
#define not_yet_implemented()   qdf_print("SPECTRAL : %s : %d Not yet implemented", __func__, __LINE__)
#endif /* WLAN_SPECTRAL_ENABLE */



/*
 * Macro to expand scalars to 64-bit objects
 */
#define    ito64(x) (sizeof(x)==8) ? (((unsigned long long int)(x)) & (0xff)) : \
         (sizeof(x)==16) ? (((unsigned long long int)(x)) & 0xffff) : \
         ((sizeof(x)==32) ? (((unsigned long long int)(x)) & 0xffffffff): (unsigned long long int)(x))


#define ATH_TXMAXTRY               13     /* max number of transmit attempts (tries) */
#define ATH_11N_TXMAXTRY           10     /* max number of 11n transmit attempts (tries) */
#define ATH_MGT_TXMAXTRY            4     /* max number of tries for management and control frames */

typedef enum {
/* MCS Rates */
    ATH_MCS0 = 0x80,
    ATH_MCS1,
    ATH_MCS2,
    ATH_MCS3,
    ATH_MCS4,
    ATH_MCS5,
    ATH_MCS6,
    ATH_MCS7,
    ATH_MCS8,
    ATH_MCS9,
    ATH_MCS10,
    ATH_MCS11,
    ATH_MCS12,
    ATH_MCS13,
    ATH_MCS14,
    ATH_MCS15,
} ATH_MCS_RATES;

#define BITS_PER_BYTE           8
#define OFDM_PLCP_BITS          22
#define HT_RC_2_MCS(_rc)        ((_rc) & 0x7f)
#define HT_RC_2_STREAMS(_rc)    ((((_rc) & 0x78) >> 3) + 1)
#define L_STF                   8
#define L_LTF                   8
#define L_SIG                   4
#define HT_SIG                  8
#define HT_STF                  4
#define HT_LTF(_ns)             (4 * (_ns))
#define SYMBOL_TIME(_ns)        ((_ns) << 2)            // ns * 4 us
#define SYMBOL_TIME_HALFGI(_ns) (((_ns) * 18 + 4) / 5)  // ns * 3.6 us
#define NUM_SYMBOLS_PER_USEC(_usec) (_usec >> 2)
#define NUM_SYMBOLS_PER_USEC_HALFGI(_usec) (((_usec*5)-4)/18)
#define IS_HT_RATE(_rate)     ((_rate) & 0x80)

// Do we need to enable the DFS BB hang SW WAR ? Check for DFS channel.
#define ATH_DFS_BB_HANG_WAR_REQ(_sc) \
    (((_sc)->sc_hang_war & HAL_DFS_BB_HANG_WAR) && \
     ((_sc)->sc_curchan.priv_flags & CHANNEL_DFS))
// Do we need to enable the RIFS BB hang SW WAR ?
#define ATH_RIFS_BB_HANG_WAR_REQ(_sc) \
    ((_sc)->sc_hang_war & HAL_RIFS_BB_HANG_WAR)
// Do we need to enable the RIFS BB hang early reset SW WAR ?
// If disabling RIFS when TKIP/WEP enabled didn't work, preemptively reset.
#define ATH_RIFS_BB_HANG_RESET_WAR_REQ(_sc) \
    (ATH_RIFS_BB_HANG_WAR_REQ(_sc) &&   \
     !(ath_hal_bb_rifs_rx_enabled((_sc)->sc_ah)))
// Do we need to enable the BB rx_clear stuck low hang SW WAR ?
#define ATH_BB_RX_STUCK_LOW_WAR_REQ(_sc) \
    (((_sc)->sc_hang_war & HAL_RX_STUCK_LOW_BB_HANG_WAR) && \
     ((_sc)->sc_noise >= 3))
// Do we need a generic BB hang SW WAR?
#define ATH_BB_GENERIC_HANG_WAR_REQ(_sc) \
    (ATH_BB_RX_STUCK_LOW_WAR_REQ(_sc) || \
     ATH_RIFS_BB_HANG_RESET_WAR_REQ(_sc))
// Do we need to enable either the RIFS early reset or DFS BB hang SW WAR ?
#define ATH_BB_HANG_WAR_REQ(_sc) \
    (ATH_DFS_BB_HANG_WAR_REQ(_sc) || \
     ATH_BB_GENERIC_HANG_WAR_REQ(_sc))
// Do we need to enable the MAC hang SW WAR ?
#define ATH_MAC_HANG_WAR_REQ(_sc) \
    ((_sc)->sc_hang_war & HAL_MAC_HANG_WAR)
// Do we need to enable any hang SW WAR ?
#define ATH_HANG_WAR_REQ(_sc) \
    (ATH_BB_HANG_WAR_REQ(_sc) || \
     ATH_MAC_HANG_WAR_REQ(_sc))
// We've found a BB hang, increment stats, and note the hang
#ifdef QCA_SUPPORT_CP_STATS
#define ATH_BB_GENERIC_HANG(_sc)    \
do {                        \
    pdev_lmac_cp_stats_ast_bb_hang_inc((_sc)->sc_pdev, 1); \
    (_sc)->sc_hang_war |= HAL_BB_HANG_DETECTED;    \
} while (0)
#else
#define ATH_BB_GENERIC_HANG(_sc)    \
do {                        \
    (_sc)->sc_stats.ast_bb_hang++;        \
    (_sc)->sc_hang_war |= HAL_BB_HANG_DETECTED;    \
} while (0)
#endif
// We've found a MAC hang, increment stats, and note the hang
#ifdef QCA_SUPPORT_CP_STATS
#define ATH_MAC_GENERIC_HANG(_sc)    \
do {                        \
    pdev_lmac_cp_stats_ast_mac_hang_inc((_sc)->sc_pdev, 1); \
    (_sc)->sc_hang_war |= HAL_MAC_HANG_DETECTED;    \
} while (0)
#else
#define ATH_MAC_GENERIC_HANG(_sc)    \
do {                        \
    (_sc)->sc_stats.ast_mac_hang++;        \
    (_sc)->sc_hang_war |= HAL_MAC_HANG_DETECTED;    \
} while (0)
#endif
// We've detected a BB hang
#define  ATH_BB_GENERIC_HANG_DETECTED(_sc)    \
    ((_sc)->sc_hang_war & HAL_BB_HANG_DETECTED)
// We've detected a MAC hang
#define  ATH_MAC_GENERIC_HANG_DETECTED(_sc)    \
    ((_sc)->sc_hang_war & HAL_MAC_HANG_DETECTED)
// Do we need to reset the HW?
#define ATH_HANG_DETECTED(_sc) \
    (ATH_BB_GENERIC_HANG_DETECTED(_sc) || \
     ATH_MAC_GENERIC_HANG_DETECTED(_sc))
// Unset all hangs detected
#define ATH_CLEAR_HANGS(_sc)    \
    ((_sc)->sc_hang_war &= ~(HAL_BB_HANG_DETECTED | HAL_MAC_HANG_DETECTED))


#define ATH_RSSI_EP_MULTIPLIER     (1<<7)  /* pow2 to optimize out * and / */
#define ATH_RATE_EP_MULTIPLIER     (1<<7)  /* pow2 to optimize out * and / */

#define ATH_RSSI_LPF_LEN           10
#define ATH_RSSI_DUMMY_MARKER      0x127
#define ATH_EP_MUL(x, mul)         ((x) * (mul))
#define ATH_EP_RND(x, mul)         ((((x)%(mul)) >= ((mul)/2)) ? ((x) + ((mul) - 1)) / (mul) : (x)/(mul))

#define    ATH_DEFAULT_NOISE_FLOOR     -95
#define RSSI_LPF_THRESHOLD          -20

#define ATH_RSSI_BAD                HAL_RSSI_BAD

#define ATH_RSSI_OUT(x)            (((x) != ATH_RSSI_DUMMY_MARKER) ? (ATH_EP_RND((x), ATH_RSSI_EP_MULTIPLIER)) : ATH_RSSI_DUMMY_MARKER)
#define ATH_RSSI_IN(x)             (ATH_EP_MUL((x), ATH_RSSI_EP_MULTIPLIER))
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

#define ATH_RATE_LPF_LEN           10          // Low pass filter length for averaging rates
#define ATH_RATE_DUMMY_MARKER      0
#define ATH_RATE_OUT(x)            (((x) != ATH_RATE_DUMMY_MARKER) ? (ATH_EP_RND((x), ATH_RATE_EP_MULTIPLIER)) : ATH_RATE_DUMMY_MARKER)
#define ATH_RATE_IN(x)             (ATH_EP_MUL((x), ATH_RATE_EP_MULTIPLIER))
#define ATH_LPF_RATE(x, y, len) \
    (((x) != ATH_RATE_DUMMY_MARKER) ? ((((x) << 3) + (y) - (x)) >> 3) : (y))
#define ATH_RATE_LPF(x, y) \
    ((x) = ATH_LPF_RATE((x), ATH_RATE_IN((y)), ATH_RATE_LPF_LEN))
#define ATH_IS_SINGLE_CHAIN(x)      ((x & (x - 1)) == 0)
#define ATH_IS_THREE_CHAIN(x)       ((x) == 7)
#define IS_SINGLE_STREAM(rate) (rate < 8)  ? 1 : 0
#define IS_THREE_STREAM(rate)  (rate > 15) ? 1 : 0

#ifdef BUILD_AP
#define    ATH_SHORT_CALINTERVAL   100     /* 100 milliseconds between calibrations */
#define    ATH_OLPC_CALINTERVAL    5000    /* 5 second between calibrations for open loop power control */
#define    ATH_LONG_CALINTERVAL    30000   /* 30 seconds between calibrations */
#define    ATH_RESTART_CALINTERVAL 1200000 /* 20 minutes between calibrations */
#define    ATH_CRDC_CALINTERVAL    100     /* 100 milliseconds between calibrations */
#else
#define    ATH_SHORT_CALINTERVAL   1000    /* 1 second between calibrations */
#define    ATH_OLPC_CALINTERVAL    5000    /* 5 second between calibrations for open loop power control */
#define    ATH_LONG_CALINTERVAL    30000   /* Set timer to 30 seconds. Station always uses 30 seconds between calibrations */
#define    ATH_RESTART_CALINTERVAL 1200000 /* 20 minutes between calibrations */
#define    ATH_CRDC_CALINTERVAL    100     /* 100 milliseconds between calibrations */
#endif

#define    ATH_UP_BEACON_INTVAL	   30000   /* Set beacon config timer to 30 seconds */

#define    ATH_CHAN_BUSY_INTVAL	   200     /*Set channel utilisation timer to 200msec */
#define    ATH_PERIODIC_STATS_INTVAL    1000   /* Periodic stats need to be sent up the layer every 1 sec */

#define ATH_TXPOWER_MAX         63 /* .5 dBm units */
#define ATH_TXPOWER_MAX_2G      ATH_TXPOWER_MAX
#define ATH_TXPOWER_MAX_5G      ATH_TXPOWER_MAX

#define ATH_TX_DUTY_CYCLE_MIN	20 /* Minimum TX Duty cycle, in percent */
#define ATH_TX_DUTY_CYCLE_MAX  100 /* Maximum TX Duty cycle, in percent */

#define ATH_REGCLASSIDS_MAX     10

#define TAIL_DROP_COUNT 50             /* maximum number of queued frames allowed */
#define ATH_CABQ_READY_TIME      80 /* % of beacon interval */

#if ATH_SUPPORT_AP_WDS_COMBO
#define ATH_DEFAULT_BINTVAL     400 /* default beacon interval in TU for 16 VAP support */
#else
#define ATH_DEFAULT_BINTVAL     100 /* default beacon interval in TU */
#endif

#define ATH_DEFAULT_BMISS_LIMIT  10

/*
 * dynamic turbo specific macros.
 */

#define ATH_TURBO_UP_THRESH 750000 /* bytes/sec */
#define ATH_TURBO_DN_THRESH 1000000 /* bytes/sec */
#define ATH_TURBO_PERIOD_HOLD 1 /*in seconds */

#define    ATH_TXDESC    1        /* number of descriptors per buffer */

#define WEP_IV_FIELD_SIZE       4       /* wep IV field size */
#define WEP_ICV_FIELD_SIZE      4       /* wep ICV field size */
#define AES_ICV_FIELD_SIZE      8       /* AES ICV field size */
#define EXT_IV_FIELD_SIZE       4       /* ext IV field size */

#define US_PER_4MS                       4000 /*  4msec define  */

#ifndef A_MAX
#define A_MAX(a,b) ((a) > (b) ? (a) : (b))
#endif
/*
 * The key cache is used for h/w cipher state and also for
 * tracking station state such as the current tx antenna.
 * We also setup a mapping table between key cache slot indices
 * and station state to short-circuit node lookups on rx.
 * Different parts have different size key caches.  We handle
 * up to ATH_KEYMAX entries (could dynamically allocate state).
 */
#define    ATH_KEYMAX    128        /* max key cache size we handle */
#define    ATH_KEYOFFSET32    32
#define    ATH_KEYOFFSET64    64
#define    ATH_KEYBYTES    (ATH_KEYMAX/NBBY)    /* storage space in bytes */
#define    ATH_MIN_FF_RATE    12000        /* min rate fof ff aggragattion.in Kbps  */

#define ATH_CHAINMASK_SEL_TIMEOUT    6000

/* Default - Number of last RSSI values that is used for chainmask
   selection */
#define ATH_CHAINMASK_SEL_RSSI_CNT    10

/* Select 1 chain instead of configured chainmask */
#define ATH_CHAINMASK_ONE_CHAIN		1

/* Means use 3x3 chainmask instead of configured chainmask */
#define ATH_CHAINMASK_SEL_3X3        7

/* Means use 2x3 chainmask instead of configured chainmask */
#define ATH_CHAINMASK_SEL_2X3        5

/* Default - Rssi threshold below which we have to switch to 3x3 */
#define ATH_CHAINMASK_SEL_UP_RSSI_THRES     20

/* Default - Rssi threshold above which we have to switch to user
   configured values */
#define ATH_CHAINMASK_SEL_DOWN_RSSI_THRES  35

#if ATH_C3_WAR
#define STOP_C3_WAR_TIMER(_sc)                                          \
do {                                                                \
    if (_sc->sc_reg_parm.c3WarTimerPeriod) {                        \
        spin_lock(&_sc->sc_c3_war_lock);                            \
        if (sc->c3_war_timer_active) {                              \
            ath_gen_timer_stop(_sc, _sc->sc_c3_war_timer);          \
            _sc->c3_war_timer_active = 0;                           \
            printk("%s: sc_c3_war_timer is stopped!!\n", __func__); \
        }                                                           \
        spin_unlock(&_sc->sc_c3_war_lock);                          \
    }                                                               \
} while(0)
#else
#define STOP_C3_WAR_TIMER(_sc)
#endif

#ifdef ATH_HTC_SG_SUPPORT
struct ath_htc_rxsg {
    wbuf_t Firstseg;
    uint8_t Onprocess;
    uint8_t nextseq;
    uint8_t pktremaining;
    uint8_t padd;
    ieee80211_rx_status_t  rx_stats;

};
#endif


/* Struct to store the chainmask select related info */
struct ath_chainmask_sel {
    struct ath_timer    timer;
    int                 cur_tx_mask; /* user configured or 3x3 */
    int                 cur_rx_mask; /* user configured or 3x3 */
    int                 tx_avgrssi;
    int                 switch_allowed:1, /* timer will set this */
                        cm_sel_enabled:1;
};

typedef enum {
    ATH_OK,                 /* no change needed */
    ATH_UPDATE,             /* update pending */
    ATH_COMMIT              /* beacon sent, commit change */
} sc_updateslot_t;            /* slot time update fsm */

struct ath_softc;

#ifdef ATH_SWRETRY
struct ath_swretry_info {
  u_int32_t         swr_num_pendfrms;        /* num of frames which are pending in queue */
#if defined(ATH_SWRETRY) && defined(ATH_SWRETRY_MODIFY_DSTMASK)
  HAL_BOOL          swr_need_cleardest;      /* need to set the clearDestMask on next frame */
#endif
  HAL_BOOL          swr_state_filtering;      /* whether the txq is in filtering state */
  /* SW retry eligible frame includes frame without BA protection:
   * - legacy frame (non-HT)
   * - HT frame with bf_isampdu = 0
   * Later, we consider AMPDU also for clearing dest mask in case of AMPDU.
   * So this variable actually indicates num of frames (AMPDU and non-AMPDU)
   * in HWQ for each node.
   */
  u_int32_t         swr_num_eligible_frms;   /* num of swretry eligible frames in txq */
};
#endif

struct ath_chan_stats{
    u_int32_t   chan_clr_cnt;
    u_int32_t   cycle_cnt;
    u_int32_t   phy_err_cnt;
    u_int32_t   duration_11b_data;
};

#if ATH_SUPPORT_VOW_DCS
struct ath_dcs_params{
    u_int32_t  bytecntrx;             /* received data counter */
    u_int32_t  rxtime;                /* total time for which received valid packets */
    u_int32_t  wastedtxtime;          /* unsuccessful transmission time */
    u_int32_t  bytecnttx;             /* transmitted data counter */
    u_int32_t  phyerrcnt;             /* phy error counter */
    /* can add one counter for RTS failures too */
    u_int32_t  prev_rxclr;            /* last rx clear count */
    u_int32_t  prev_ts;               /* last timestamp */
    u_int8_t   intr_cnt;              /* Interefernce detection counter */
    u_int8_t   samp_cnt;              /* sample counter */
    /* to-do: RSSI should be stored per node*/
    u_int8_t   last_ack_rssi;         /* RSSI of the last ack received */
};
#endif


#if UNIFIED_SMARTANTENNA
struct smart_ant_traininfo  {
    u_int32_t antenna_array[SMART_ANT_MAX_SA_CHAINS];
    u_int32_t rate_array[SMART_ANTENNA_MAX_RATE_SERIES];
    u_int32_t num_pkts;
};

/* Add new fields to this structure based on the Algo*/
struct smart_ant_trainstats  {
    u_int32_t nFrames;
};
#endif

/* driver-specific node state */
struct ath_node {
    /* Pointer to the corresponding node object in protocol stack.
     * It should be opaque to ATH layer. */
    ieee80211_node_t    an_node;
    struct ath_softc    *an_sc; /* back pointer */
    struct ath_vap      *an_avp; /* back pointer to vap*/

    u_int8_t    an_prevdatarix;    /* rate ix of last data frame */
    u_int16_t    an_minffrate;    /* mimum rate in kbps for ff to aggragate */

    bool        an_fixedrate_enable;	/* fixed rate flag */
    u_int8_t    an_fixedrix;			/* fixed rate index */
    u_int8_t    an_fixedratecode;       /* fixed rate code */

    struct ath_buf    *an_tx_ffbuf[WME_NUM_AC]; /* ff staging area */
#ifdef ATH_SUPPORT_UAPSD
    ath_bufhead                an_uapsd_q;                /* U-APSD delivery queue */
    int                        an_uapsd_qdepth;           /* U-APSD delivery queue depth */
    struct ath_buf             *an_uapsd_last_frm;        /* last subframe in the uapsd queque */
    struct ath_buf             *an_uapsd_last_stage_frm;  /* last stage frame in the uapsd queque */
#ifdef ATH_SWRETRY
    int                        an_uapsd_num_addbuf;       /* number of frames eligible for swretry added to uapsdq upon each trigger */
#endif
#ifdef ATH_VAP_PAUSE_SUPPORT
    ath_bufhead                an_uapsd_stage_q;          /* U-APSD temporay stage queue used by vap pause */
    int                        an_uapsd_stage_qdepth;     /* U-APSD temporary stage queue depth  depth */
    struct ath_node            *an_temp_next;             /* temp node list used by ath_vap_pause module */
#endif
#endif
#ifdef ATH_CHAINMASK_SELECT
    struct ath_chainmask_sel an_chainmask_sel;
#endif

    struct ath_node_aggr    an_aggr;        /* A-MPDU aggregation state */
    struct atheros_node     *an_rc_node;    /* opaque ptr to rc-private struct */
    u_int8_t                an_smmode;  /* SM Power save mode */
#define ATH_NODE_CLEAN          0x1     /* indicates the node is clened up */
#define ATH_NODE_PWRSAVE        0x2     /* indicates the node is 80211 power save */
#define ATH_NODE_UAPSD          0x4     /* indicates the node is uapsd */
#define ATH_NODE_SWRETRYQ_FLUSH 0x8     /* indicates the node's swretryq is flushed */
#define ATH_NODE_TEMP           0x10    /* indicates the node is temporary */
#define ATH_NODE_LEAVE           0x20    /* indicates the node leave */
    u_int8_t                an_flags;
    u_int16_t               an_cap;     /* ath_node capabilities */
#ifdef ATH_SWRETRY
    int32_t                 an_swrenabled;       /* Whether retry is enabled for this node */
    ath_bufhead             an_softxmit_q;       /* Soft Tx Queue used for Sw Retries */
    int                     an_softxmit_qdepth;  /* Depth of soft tx queue */
    spinlock_t              an_softxmit_lock;    /* Soft_Xmit delivery queue lock */
    struct ath_swretry_info an_swretry_info[HAL_NUM_TX_QUEUES];  /* SW retry info state machine */
    u_int32_t               an_total_swrtx_pendfrms;    /* Num of frms which are on SW retry */
    u_int32_t               an_total_swrtx_successfrms; /* Total num of suceeded sw retry frms */
    u_int32_t               an_total_swrtx_flushfrms;   /* Total num of flushed frms */
    LIST_ENTRY(ath_node)    an_le;  /* link entry in node list */

    atomic_t                an_pspoll_pending;  /* flag to handle PS-Poll */
    atomic_t                an_pspoll_response;  /* flag to handle PS-Poll response */
    bool                    an_tim_set;  		/* flag used for reducing the number of calls to get_pwrsavq_len() */
#endif
#if ATH_SUPPORT_IQUE
    u_int                    an_minRate[WME_NUM_AC];    /* Min rate index for each AC */
#endif
#if ATH_SUPPORT_VOWEXT
    u_int8_t                min_depth;
    u_int8_t                throttle;
#endif
    u_int16_t is_sta_node;
    atomic_t  an_active_tx_cnt;     /* pending tx thread */
#ifdef  ATH_SUPPORT_TxBF
    ieee80211_txbf_caps_t   an_txbf_caps;
#endif
#ifdef ATH_LMAC_TXSEQ
    u_int16_t               ni_txseqs[WME_NUM_TID];      /* tx seq per-tid */
#endif

    int32_t      an_avgbrssi;    /* average beacon rssi */
    int32_t      an_avgbchainrssi[ATH_HOST_MAX_ANTENNA]; /* avg rssi of all rx beacon per antenna */
    int32_t      an_avgbchainrssiext[ATH_HOST_MAX_ANTENNA]; /* avg rssi of all rx beacon on ext chan */
    int32_t      an_avgdrssi;    /* average data rssi */
    int32_t      an_avgdchainrssi[ATH_HOST_MAX_ANTENNA]; /* avg rssi of all rx data per antenna */
    int32_t      an_avgdchainrssiext[ATH_HOST_MAX_ANTENNA]; /* avg rssi of all rx data on ext chan */
    int32_t      an_avgrssi;     /* average rssi of all received frames */
    int32_t      an_avgchainrssi[ATH_HOST_MAX_ANTENNA]; /* avg rssi of all rx frames per antenna */
    int32_t      an_avgchainrssiext[ATH_HOST_MAX_ANTENNA]; /* avg rssi of all rx on ext chan */
    int32_t      an_avgtxrssi;   /* average aggr rssi over all tx acks */
    int32_t      an_avgtxchainrssi[ATH_HOST_MAX_ANTENNA];/* avg rssi of all tx acks per antenna */
    int32_t      an_avgtxchainrssiext[ATH_HOST_MAX_ANTENNA];/* avg rssi of all tx acks on ext chan */
    u_int32_t    an_avgtxrate;   /* average tx rate (Kbps) */
    u_int32_t    an_avgrxrate;   /* average rx rate (Kbps) */
    u_int32_t    an_lasttxrate;  /* last packet tx rate  */
    u_int32_t    an_lastrxrate;  /* last packet rx rate  */
    u_int8_t     an_txratecode;  /* last packet tx ratecode */
    u_int8_t     an_rxratecode;  /* last packet rx ratecode */
    u_int8_t     an_txrateflags;  /* last packet rate flag*/
    u_int8_t     an_lasttxpower;
#if UNIFIED_SMARTANTENNA
    u_int32_t  smart_ant_antenna_array[SMART_ANT_MAX_SA_CHAINS]; /* rate used stats for smart antenna retraining */
    struct smart_ant_traininfo traininfo;
    struct smart_ant_trainstats trainstats;
    int8_t smart_ant_train_status;
#endif
#if ATH_SUPPORT_WIFIPOS
    void         *an_wifipos_data;
#endif
#if LMAC_SUPPORT_POWERSAVE_QUEUE
    /* power save queues */
    struct ath_node_pwrsaveq an_dataq;
    struct ath_node_pwrsaveq an_mgmtq;
#endif
    ath_bufhead   an_tx_frag_q;                     /* hold fragments and send down together */
    u_int8_t an_consecutive_xretries;
#if ATH_SUPPORT_KEYPLUMB_WAR
    HAL_KEYVAL   an_halkey;      /* HAL Key */
    u_int32_t    an_decrypt_err; /* decrypt error stats for a node */
    u_int8_t     an_macaddr[IEEE80211_ADDR_LEN];    /* macaddr of the node */
    u_int8_t     an_keyix;       /* keyix of the node */
#endif
	u_int32_t an_retries;

    /* per node tx chainmask for internally generated frame */
    u_int8_t    an_tx_chainmask;
    bool an_pspoll;
#if QCA_AIRTIME_FAIRNESS
    u_int32_t   an_atf_nodepaused;      /* Bitmap indicating TID paused by ATF */
    u_int16_t   an_pkt_drop_nobuf;      /* Packets dropped as node is already holding max allowed buffers */
    u_int32_t   an_atf_capable;         /* mark ATF capable client */
    spinlock_t  an_atftoken_lock;       /* atf token lock */
    u_int32_t   an_tx_unusable_tokens_updated:1, /* for athstats */
                an_atf_nomore_tokens:1, /* Set if this node (any AC) is using all tokens */
                an_atf_nomore_tokens_show:1; /* for wlanconfig showtbl */
    u_int32_t   an_old_max_tput;        /* If we can't estimate, use an older value */
#endif
    void *an_peer;
};

#define ATF_MIN_BUFS                         64
#define ATF_MAX_BUFS                         1024
#define ATF_TPUT_RESV_AIRTIME                10
#define ATF_ADJUST_POSSIBLE_TPUT_MASK        0xFFFFFF
#define ATF_USE_AGGR_FOR_POSSIBLE_TPUT_MASK  0xFF
#define ATF_USE_AGGR_FOR_POSSIBLE_TPUT_SHIFT 24
#define ATF_ADJUST_TPUT_MORE_THRS_MASK       0xFFFFFF
#define ATF_ADJUST_TPUT_MORE_MASK            0xFF
#define ATF_ADJUST_TPUT_MORE_SHIFT           24
#define ATF_ADJUST_TPUT_DFL                  2500
#define ATF_ADJUST_TPUT_MORE_DFL             30
#define ATF_ADJUST_TPUT_MORE_THRS_DFL        40000

#define ATH_NODE(_n)                    ((struct ath_node *)(_n))

#ifdef ATH_SWRETRY
#define    ATH_NODE_SWRETRY_TXBUF_LOCK_INIT(_an)    spin_lock_init(&(_an)->an_softxmit_lock)
#define    ATH_NODE_SWRETRY_TXBUF_LOCK_DESTROY(_an)
#define    ATH_NODE_SWRETRY_TXBUF_LOCK(_an)         spin_lock_dpc(&(_an)->an_softxmit_lock)
#define    ATH_NODE_SWRETRY_TXBUF_UNLOCK(_an)       spin_unlock_dpc(&(_an)->an_softxmit_lock)
#endif

#if QCA_AIRTIME_FAIRNESS
#define    ATH_NODE_ATF_TOKEN_LOCK_INIT(_an)    spin_lock_init(&(_an)->an_atftoken_lock)
#define    ATH_NODE_ATF_TOKEN_LOCK_DESTROY(_an)
#define    ATH_NODE_ATF_TOKEN_LOCK(_an)         spin_lock(&(_an)->an_atftoken_lock)
#define    ATH_NODE_ATF_TOKEN_UNLOCK(_an)       spin_unlock(&(_an)->an_atftoken_lock)
#endif
#define    ATH_ANTENNA_DIFF    2    /* Num farmes difference in
                       tx to flip default recv
                       antenna */

struct ath_hal;
struct ath_desc;
struct atheros_softc;
struct proc_dir_entry;

/*
 * Data transmit queue state.  One of these exists for each
 * hardware transmit queue.  Packets sent to us from above
 * are assigned to queues based on their priority.  Not all
 * devices support a complete set of hardware transmit queues.
 * For those devices the array sc_ac2q will map multiple
 * priorities to fewer hardware queues (typically all to one
 * hardware queue).
 */
struct ath_txq {
    u_int           axq_qnum;       /* hardware q number */
    u_int32_t       *axq_link;      /* link ptr in last TX desc */
    TAILQ_HEAD(, ath_buf)   axq_q;  /* transmit queue */
    spinlock_t      axq_lock;       /* lock on q and link */
    unsigned long   axq_lockflags;  /* intr state when must cli */
    u_int           axq_depth;      /* queue depth */
    u_int           axq_burst_time; /* queue txop burst time */
    int             axq_qtype;      /* hal queue type */
#if ATH_TX_BUF_FLOW_CNTL
    u_int           axq_minfree;    /* Number of free tx_bufs required in common buf pool */
    u_int           axq_num_buf_used; /* Number of used tx_buf for this q */
#endif
    u_int8_t        axq_aggr_depth; /* aggregates queued */
    u_int32_t       axq_totalqueued;/* total ever queued */
    u_int           axq_intrcnt;	/* count to determine if descriptor
                                     * should generate int on this txq.
                                     */
	/*
	 * State for patching up CTS when bursting.
	 */
	struct	ath_buf     *axq_linkbuf;   /* virtual addr of last buffer*/
	struct	ath_desc    *axq_lastdsWithCTS; /* first desc of the last descriptor
                                             * that contains CTS
                                             */
	struct	ath_desc    *axq_gatingds;	/* final desc of the gating desc
                                         * that determines whether lastdsWithCTS has
                                         * been DMA'ed or not
                                         */
    /*
     * Staging queue for frames awaiting a fast-frame pairing.
     */
    TAILQ_HEAD(axq_headtype, ath_buf) axq_stageq;
    TAILQ_HEAD(,ath_atx_ac) axq_acq;

#if defined(ATH_SWRETRY) && defined(ATH_SWRETRY_MODIFY_DSTMASK)
    u_int                   axq_destmask:1; /*Destination mask for this queue*/
#endif
#if ATH_TX_POLL
    int            axq_lastq_tick; /* ticks when last packet queued */
#endif
    TAILQ_HEAD(, ath_buf)   axq_fifo[HAL_TXFIFO_DEPTH];
    u_int8_t                axq_headindex;
    u_int8_t                axq_tailindex;
    u_int                   irq_shared:1;
#if ATH_HW_TXQ_STUCK_WAR
    u_int8_t               tx_done_stuck_count;
#endif
#if ATH_SUPPORT_TX_BACKLOG
    ath_bufhead             backlogq;        /* s/w queue for Tx buffers */
    u_int                   axq_bqdepth;
#endif
};

struct ath_vap_stats {
    u_int32_t av_tx_xretries;
    u_int32_t av_tx_retries;
    u_int32_t av_tx_mretries;
    u_int32_t av_ack_failures;
    u_int32_t av_retry_count;
    u_int32_t av_aggr_pkt_count;
};

#define MCAST_MIN_FREEBUF 48
#define MAX_TIMBUFFERS_PER_VAP 2
/* driver-specific vap state */
struct ath_vap {
    ieee80211_if_t                  av_if_data; /* interface(vap) instance from 802.11 protocal layer */
    HAL_OPMODE                      av_opmode;  /* VAP operational mode */
    struct ath_buf                  *av_bcbuf;    /* beacon buffer */
    ieee80211_beacon_offset_t       av_boff;    /* dynamic update state */
    ieee80211_tx_control_t          av_btxctl;   /* tx control information for beacon */
#if UMAC_SUPPORT_WNM
    struct ath_buf                  *av_tbuf_lowrate;    /* TIM buffer */
    struct ath_buf                  *av_tbuf_highrate;    /* TIM buffer */
    ieee80211_tx_control_t          av_timtxctl_lowrate;   /* tx control information for TIM */
    ieee80211_tx_control_t          av_timtxctl_highrate;   /* tx control information for TIM */
    u_int8_t                        av_tim_lowrix; /* rate code for beacon */
    u_int8_t                        av_tim_highrix; /* rate code for beacon */
    u_int16_t                       av_tim_lowrate_code; /* rate code for beacon */
    u_int16_t                       av_tim_highrate_code; /* rate code for beacon */
    struct ath_txq                  av_fmsq[ATH_MAX_FMS_QUEUES]; /* FMS Counter Queues */
#endif
    int                             av_bslot;    /* beacon slot index */
    u_int16_t                       av_beacon_rate_code; /* rate code for beacon */
    struct ath_txq                  av_mcastq;    /* multicast transmit queue */
    u_int8_t                        av_dfswait_run:1,
                                    av_up:1;
    u_int64_t                       av_tsfadjust; /* tsf adjustment value for the vap */
    struct atheros_vap              *av_atvp;   /* opaque ptr to rc-private struct */
    struct ath_vap_config           av_config;  /* vap configuration parameters from 802.11 protocol layer*/
#if WAR_DELETE_VAP
    struct ath_vap                  *av_del_next;  /* List of vaps that are hanging due to a node refcount being non-zero */
    struct ath_vap                  *av_del_prev;  /* List of vaps that are hanging due to a node refcount being non-zero */
    //systime_t                       av_del_timestamp; /* Time of moving to the pend queue */
#endif
    atomic_t                        av_stop_beacon;         /* stop and return beacon resource */
    atomic_t                        av_beacon_cabq_use_cnt; /* pending threads using beacon and cabq */
    ath_vap_info_t                  av_vap_info;            /* Opaque handle for ATH_VAP_INFO */
    struct ath_vap_stats            av_stats;               /* VAP specific stats */
    u_int32_t                       av_modify_bcn_rate;     /* To modify beacon rate via iwpriv */
    u_int8_t                        av_txpow_mgt_frm[MAX_NUM_TXPOW_MGT_ENTRY]; /* To store the txpower value for mgmt frames */
    HAL_CHANNEL                     av_bsschan;
    u_int8_t                        av_txpow_frm[MAX_NUM_TXPOW_FRAME_TYPE][MAX_NUM_TXPOW_MGT_ENTRY]; /* To store the txpower value for control frames */
};


#define ATH_DIFF(a,b)  ((a) > (b) ? ((a) - (b)) : ((b) - (a)))

#define    ATH_BEACON_AIFS_DEFAULT        0  /* Default aifs for ap beacon q */
#define    ATH_BEACON_CWMIN_DEFAULT    0  /* Default cwmin for ap beacon q */
#define    ATH_BEACON_CWMAX_DEFAULT    0  /* Default cwmax for ap beacon q */

#define ATH_TXQ_INTR_PERIOD        5  /* axq_intrcnt period for intr gen */
#define    ATH_TXQ_LOCK_INIT(_tq)        spin_lock_init(&(_tq)->axq_lock)
#define    ATH_TXQ_LOCK_DESTROY(_tq)

#define	ATH_TXQ_LOCK(_tq)           do{os_tasklet_lock(&(_tq)->axq_lock, (_tq)->axq_lockflags);}while(0)
#define	ATH_TXQ_UNLOCK(_tq)         do{os_tasklet_unlock(&(_tq)->axq_lock, (_tq)->axq_lockflags);}while(0)

/* move buffers from MCASTQ to CABQ */
#define ATH_TXQ_MOVE_MCASTQ(_tqs,_tqd) do {                         \
    (_tqd)->axq_depth += (_tqs)->axq_depth;                         \
    (_tqd)->axq_totalqueued += (_tqs)->axq_totalqueued;             \
    (_tqd)->axq_linkbuf = (_tqs)->axq_linkbuf;                      \
    (_tqd)->axq_link = (_tqs)->axq_link;                            \
    TAILQ_CONCAT(&(_tqd)->axq_q, &(_tqs)->axq_q, bf_list);          \
    (_tqs)->axq_depth=0;                                            \
    (_tqs)->axq_totalqueued = 0;                                    \
    (_tqs)->axq_linkbuf = 0;                                        \
    (_tqs)->axq_link = NULL;                                        \
} while (0)

/* concat a list of buffers to txq */
#define ATH_TXQ_CONCAT(_tq, _stq) do {                              \
    TAILQ_CONCAT(&(_tq)->axq_q, (_stq), bf_list);                   \
    (_tq)->axq_depth ++;                                            \
    (_tq)->axq_totalqueued ++;                                      \
    (_tq)->axq_linkbuf = TAILQ_LAST(&(_tq)->axq_q, ath_bufhead_s);  \
} while (0)


/* move a list from a txq to a buffer list (including _elm) */
#define ATH_TXQ_MOVE_HEAD_UNTIL(_tq, _stq, _elm, _field) do {       \
    TAILQ_REMOVE_HEAD_UNTIL(&(_tq)->axq_q, _stq, _elm, _field);     \
    (_tq)->axq_depth --;                                            \
} while (0)

/* move a list from a txq to a buffer list (not including _elm) */
#define ATH_TXQ_MOVE_HEAD_BEFORE(_tq, _stq, _elm, _field) do {                  \
    TAILQ_REMOVE_HEAD_BEFORE(&(_tq)->axq_q, _stq, _elm, ath_bufhead_s, _field); \
    (_tq)->axq_depth --;                                                        \
} while (0)

#define ATH_TXQ_REMOVE_STALE_HEAD(_tq, _elm, _field)                \
    TAILQ_REMOVE(&(_tq)->axq_q, (_elm), _field)

/*
 * concat buffers from one queue to other
 */
#define ATH_TXQ_MOVE_Q(_tqs,_tqd)  ATH_TXQ_MOVE_MCASTQ(_tqs,_tqd)

/*
 * Regardless of the number of beacons we stagger, (i.e. regardless of the
 * number of BSSIDs) if a given beacon does not go out even after waiting this
 * number of beacon intervals, the game's up.
 */
#define BSTUCK_THRESH_PERVAP   9
#define BSTUCK_THRESH   (BSTUCK_THRESH_PERVAP * ATH_BCBUF)

/* unalligned little endian access */
#ifndef LE_READ_2
#define LE_READ_2(p)                            \
    ((u_int16_t)                            \
     ((((u_int8_t *)(p))[0]      ) | (((u_int8_t *)(p))[1] <<  8)))
#endif
#ifndef LE_READ_4
#define LE_READ_4(p)                            \
    ((u_int32_t)                            \
     ((((u_int8_t *)(p))[0]      ) | (((u_int8_t *)(p))[1] <<  8) |    \
      (((u_int8_t *)(p))[2] << 16) | (((u_int8_t *)(p))[3] << 24)))
#endif
/* increment with wrap-around */
#define INCR(_l, _sz)   (_l) ++; (_l) &= ((_sz) - 1)
#define DECR(_l, _sz)   (_l) --; (_l) &= ((_sz) - 1)

/* add with wrap-around */
#define ADD(_l, _n, _sz)   (_l) += (_n); (_l) &= ((_sz) - 1)

/*
 * new aggregation related macros
 */
#define ATH_AGGR_DELIM_SZ       4       /* delimiter size   */
#define ATH_AGGR_MINHWPLEN      256     /* in bytes, minimum packet length supported by HW */
/* SW minimum packet length. Must account for start delimiter */
#define ATH_AGGR_MINPLEN        (ATH_AGGR_MINHWPLEN -  ATH_AGGR_DELIM_SZ)
#if ATH_SUPPORT_WAPI
#define ATH_AGGR_ENCRYPTDELIM   12      /* number of delimiters for encryption padding for WAPI */
#else
#define ATH_AGGR_ENCRYPTDELIM   10      /* number of delimiters for encryption padding */
#endif

typedef enum {
    ATH_AGGR_DONE,
    ATH_AGGR_BAW_CLOSED,
    ATH_AGGR_LIMITED,
    ATH_AGGR_SHORTPKT,
    ATH_AGGR_8K_LIMITED,
} ATH_AGGR_STATUS;

typedef enum ath_get_buf_status {
    ATH_BUF_NEW=1,
    ATH_BUF_CONT,
    ATH_BUF_NONE,
    ATH_BUF_LAST,
} ath_get_buf_status_t;

/* ath internal reset type */
typedef enum{
    ATH_RESET_DEFAULT = 0,
    ATH_RESET_NOLOSS = 1,
} ATH_RESET_TYPE;

#define BAW_WITHIN(_start, _bawsz, _seqno)      \
    ((((_seqno) - (_start)) & 4095) < (_bawsz))

#define PADBYTES(_len)   ((4 - ((_len) % 4)) % 4)
#define ATH_MAX_SW_RETRIES  10


/*
 * return block-ack bitmap index given sequence and starting sequence
 */
#define ATH_BA_INDEX(_st, _seq) (((_seq) - (_st)) & (IEEE80211_SEQ_MAX - 1))

/*
 * return whether a bit at index _n in bitmap _bm is set
 * _sz is the size of the bitmap
 */
#define ATH_BA_ISSET(_bm, _n)        (((_n) < (WME_BA_BMP_SIZE)) &&          \
                                     ((_bm)[(_n) >> 5] & (1 << ((_n) & 31))))

/*
 * desc accessor macros
 */
#define ATH_DS_BA_SEQ(_ts)          (_ts)->ts_seqnum
#define ATH_DS_BA_BITMAP(_ts)       (&(_ts)->ba_low)
#define ATH_DS_TX_BA(_ts)           ((_ts)->ts_flags & HAL_TX_BA)
#define ATH_DS_TX_STATUS(_ts)       (_ts)->ts_status
#define ATH_DS_TX_FLAGS(_ts)        (_ts)->ts_flags


#define ATH_RIFS_NONE 0
#define ATH_RIFS_SUBFRAME_FIRST 1
#define ATH_RIFS_SUBFRAME_LAST 2
#define ATH_RIFS_BAR 3

#define ATH_SET_TX_SET_NOACK_POLICY(_sc, _wh) do {              \
    if (_wh) {                                                  \
        if (((_wh)->i_fc[1] & IEEE80211_FC1_DIR_MASK) !=        \
             IEEE80211_FC1_DIR_DSTODS)                          \
        ((struct ieee80211_qosframe *)_wh)->i_qos[0]            \
                      |= (1 << IEEE80211_QOS_ACKPOLICY_S)       \
                         & IEEE80211_QOS_ACKPOLICY;             \
        else                                                    \
        ((struct ieee80211_qosframe_addr4 *)_wh)->i_qos[0]      \
                      |= (1 << IEEE80211_QOS_ACKPOLICY_S)       \
                         & IEEE80211_QOS_ACKPOLICY;             \
    }                                                           \
} while (0)

#define ATH_SET_TX_CLR_NOACK_POLICY(_sc, _wh) do {              \
    if (_wh) {                                                  \
        if (((_wh)->i_fc[1] & IEEE80211_FC1_DIR_MASK) !=        \
             IEEE80211_FC1_DIR_DSTODS)                          \
        ((struct ieee80211_qosframe *)(_wh))->i_qos[0]          \
                      &= ~IEEE80211_QOS_ACKPOLICY;              \
    else                                                        \
        ((struct ieee80211_qosframe_addr4 *)(_wh))->i_qos[0]    \
                      &= ~IEEE80211_QOS_ACKPOLICY;              \
    }                                                           \
} while (0)

#define ATH_MPDU_2_QOS_WH(_mpdu) \
        (struct ieee80211_qosframe *)wbuf_header(_mpdu)

struct aggr_rifs_param {
    int param_max_frames;
    int param_max_len;
    int param_rl;
    int param_al;
    struct ath_rc_series *param_rcs;
};

/*
 * Per-instance load-time (note: NOT run-time) configurations for Atheros Device
 */
struct ath_config {
#ifdef ATH_USB
    usblock_t  txpow_lock;
#else
    spinlock_t  txpow_lock;
#endif
    u_int8_t    txchainmask;
    u_int8_t    rxchainmask;
    u_int8_t    txchainmasklegacy;
    u_int8_t    rxchainmasklegacy;
    u_int8_t    chainmask_sel; /* enable automatic tx chainmask selection */
    int         ampdu_limit;
    int         ampdu_subframes;
    int         rifs_ampdu_div;
    u_int32_t   ath_aggr_prot;
    u_int32_t   ath_aggr_prot_duration;
    u_int32_t   ath_aggr_prot_max;
    int         ath_countrycode;    /* country code */
    int         ath_xchanmode;        /* enable extended channels */
    int16_t     txpowupdown2G;  /*adjustment for 2G */
    int16_t     txpowupdown5G;  /*adjustment for 5G */
    int16_t     txpow_need_update;  /* flag to tune txpower */
    u_int16_t   txpowlimit2G;
    u_int16_t   txpowlimit5G;
    u_int16_t   txpowlimit_override;
    u_int16_t   tpscale;
    u_int16_t   pwscale;
    u_int8_t    pcieDisableAspmOnRfWake;         /* Only use ASPM when RF Silenced */
    u_int8_t    pcieAspm;                        /* ASPM bit settings */
    u_int8_t    pcieLcrOffset;                   /* LCR register offset */
    u_int8_t    cabqReadytime;                   /* Cabq Readytime % */
    u_int8_t    swBeaconProcess;                 /* Process received beacons in SW (vs HW) */
    u_int8_t    ampdu_rx_bsize;                  /* override BA window/buffer size for RX */
    u_int8_t    ath_noise_spur_opt;     /* noise spur optimization */
};

#if ATH_WOW

/* Structure and defines for WoW */
/* TBD: Should dynamically allocate memeory after query hal for hardware capability */
#define MAX_PATTERN_SIZE                  256
#define MAX_PATTERN_MASK_SIZE             32
#define MAX_NUM_PATTERN                   8
#define MAX_NUM_USER_PATTERN              6  /* Deducting the disassociate/deauthenticate packets */

#define ATH_WAKE_UP_PATTERN_MATCH       0x00000001
#define ATH_WAKE_UP_MAGIC_PACKET        0x00000002
#define ATH_WAKE_UP_LINK_CHANGE         0x00000004
#define ATH_WAKE_UP_BEACON_MISS         0x00000008
#define ATH_WOW_NLO_DISCOVERY           0x00000010  /* SW WOW offload */
#define ATH_WOW_AP_ASSOCIATION_LOST     0x00000020  /* SW WOW offload */
#define ATH_WOW_GTK_HANDSHAKE_ERROR     0x00000040  /* SW WOW offload */
#define ATH_WAKE_UP_4WAY_HANDSHAKE      0x00000080  /* SW WOW offload */
#define ATH_WOW_GTK_OFFLOAD             0x00000100  /* SW WOW offload */
#define ATH_WOW_ARP_OFFLOAD             0x00000200  /* SW WOW offload */
#define ATH_WOW_NS_OFFLOAD              0x00000400  /* SW WOW offload */
#define ATH_WAKE_UP_ACER_MAGIC_PACKET   0x00001000  /* SW WOW offload */
#define ATH_WAKE_UP_ACER_KEEP_ALIVE     0x00002000  /* SW WOW offload */

/* PCI Power management (PM_CSR) D0-D3 state settings */
#define PCIE_PM_CSR                 0x44    // PM Control/Status Register

#define PCIE_PM_CSR_D0              0x00
#define PCIE_PM_CSR_D1              0x01
#define PCIE_PM_CSR_D2              0x02
#define PCIE_PM_CSR_D3              0x03
#define PCIE_PM_CSR_PME_ENABLE      0x0100
#define PCIE_PM_CSR_STATUS          0x8000


typedef struct wowPattern {
    u_int32_t valid;
    u_int8_t  patternBytes[MAX_PATTERN_SIZE];
    u_int8_t  maskBytes[MAX_PATTERN_SIZE];
    u_int32_t patternId;
    u_int32_t patternLen;
} WOW_PATTERN;


#if ATH_WOW_OFFLOAD

#define RSN_KCK_LEN 16
#define RSN_KEK_LEN 16

/* Data is downloaded from host to embedded CPU via MAC PCU buffers.
 * These are actually 4 byte registers and can be written to/read from using
 * OS_REG_READ/OS_REG_WRITE only. Therefore data written to this space
 * can only be in the form of a word. Also to take care of endianness,
 * data greater than 4 bytes have been split into 'lower' and 'upper' */
struct wow_offload_rekey_download_data {
    u_int32_t kck_0; /* Lower bytes */
    u_int32_t kck_1;
    u_int32_t kck_2;
    u_int32_t kck_3; /* Upper bytes */
    u_int32_t kek_0; /* Lower bytes */
    u_int32_t kek_1;
    u_int32_t kek_2;
    u_int32_t kek_3; /* Upper bytes */
    u_int32_t replay_counter_lower;
    u_int32_t replay_counter_upper;
#define WOW_NODE_QOS 0x1
    u_int32_t flags;
    u_int32_t myaddr_lower;
    u_int32_t myaddr_upper;
    u_int32_t bssid_lower;
    u_int32_t bssid_upper;
    u_int32_t tx_seqnum;
    u_int32_t ucast_keyix;
#define WOW_CIPHER_NONE 0x0
#define WOW_CIPHER_AES  0x1
#define WOW_CIPHER_TKIP 0x2
#define WOW_CIPHER_WEP  0x3
    u_int32_t cipher;
    u_int32_t keytsc_lower;
    u_int32_t keytsc_upper;
};

/* Embedded CPU uploads this information to the MAC PCU buffer
 * on exiting WoW mode. WLAN driver picks up this information on
 * waking up and updates its state */
struct wow_offload_rekey_upload_data {
    u_int64_t replay_counter;
    u_int64_t keytsc;
    u_int32_t tx_seqnum;
};

/* GTK rekey offload state structure */
struct wow_offload_rekey_info {
    bool valid;              /* Information validity */
    u_int32_t id;            /* to identify protocol during remove */
    struct wow_offload_rekey_download_data dl_data;
    struct wow_offload_rekey_upload_data ul_data;
};

/* ARP offload */
#define WOW_OFFLOAD_ARP_INFO_MAX    2

struct wow_offload_arp_info {
    u_int32_t   valid;
    u_int32_t   id;

    u_int32_t   Flags;
    union {
        u_int8_t    u8[4];
        u_int32_t   u32;
    } RemoteIPv4Address;
    union {
        u_int8_t    u8[4];
        u_int32_t   u32;
    } HostIPv4Address;
    union {
        u_int8_t    u8[6];
        u_int32_t   u32[2];
    } MacAddress;
};

/* NS offload */
#define WOW_OFFLOAD_NS_INFO_MAX    2

struct wow_offload_ns_info {
    u_int32_t   valid;
    u_int32_t   id;

    u_int32_t   Flags;
    union {
        u_int8_t    u8[16];
        u_int32_t   u32[4];
    } RemoteIPv6Address;
    union {
        u_int8_t    u8[16];
        u_int32_t   u32[4];
    } SolicitedNodeIPv6Address;
    union {
        u_int8_t    u8[6];
        u_int32_t   u32[2];
    } MacAddress;
    union {
        u_int8_t    u8[16];
        u_int32_t   u32[4];
    } TargetIPv6Addresses[2];
};

/* Acer Magic Packet format detect */
#define WOW_OFFLOAD_ACER_MAGIC_DATA_SIZE    6
struct wow_offload_acer_magic_packet {
    bool        valid;
    u_int8_t    data[WOW_OFFLOAD_ACER_MAGIC_DATA_SIZE];
};

/* Acer SW Keep alive */
#define WOW_OFFLOAD_ACER_SWKA_MAX           2
#define WOW_OFFLOAD_ACER_SWKA_DATA_SIZE     80
struct wow_offload_acer_swka {
    bool        valid;
    u_int32_t   id;
    u_int32_t   period; /* ms */
    u_int32_t   size;   /* data size in byte */
    u_int32_t   data[(WOW_OFFLOAD_ACER_SWKA_DATA_SIZE + 3) / 4];
};
#endif /* ATH_WOW_OFFLOAD */

struct wow_info {
    u_int32_t wakeUpEvents; //Values passed in OID_PNP_ENABLE_WAKE_UP
    u_int32_t numOfPatterns;
    u_int32_t wowWakeupReason;
    HAL_INT   intrMaskBeforeSleep;
    WOW_PATTERN patterns[MAX_NUM_PATTERN];
    u_int8_t  chipPatternBytes[MAX_PATTERN_SIZE];
    u_int16_t wowDuration;
    u_int32_t timeoutinsecs;
#if ATH_WOW_OFFLOAD
    u_int32_t EnabledWoLPacketPatterns;
    u_int32_t EnabledProtocolOffloads;
    u_int32_t MediaSpecificWakeUpEvents;
    struct wow_offload_rekey_info *rekeyInfo;
    struct wow_offload_acer_magic_packet acer_magic;
    struct wow_offload_acer_swka         acer_swka[WOW_OFFLOAD_ACER_SWKA_MAX];
    struct wow_offload_arp_info          arp_info[WOW_OFFLOAD_ARP_INFO_MAX];
    struct wow_offload_ns_info           ns_info[WOW_OFFLOAD_ARP_INFO_MAX];
#endif /* ATH_WOW_OFFLOAD */
};
#endif

#if defined(ATH_SUPPORT_DFS) && !defined(ATH_DFS_RADAR_DETECTION_ONLY)
struct dfs_hang_war {
    os_timer_t          hang_war_timer;
    int                 hang_war_ht40count;
    int                 hang_war_ht20count;
    int                 hang_war_timeout;
    int                 hang_war_activated;
    int                 total_stuck;
    u_int64_t           last_dfs_hang_war_tstamp;
};
#endif

#ifdef ATH_MIB_INTR_FILTER
typedef enum {
    INTR_FILTER_OFF,           /* waiting for first MIB interrupt */
    INTR_FILTER_DEGLITCHING,   /* waiting for signals of burst */
    INTR_FILTER_ON             /* discarding MIB interrupts */
} INTR_FILTER_STATE;
#ifdef ATH_SUPPORT_EEEPC
#define MIB_FILTER_COUNT_THRESHOLD           100   /* number of interrupts to characterize a long burst ("storm") */
#define MIB_FILTER_MAX_INTR_ELAPSED_TIME     100   /* maximum allowed time interval for between 2 consecutive interrupts within the same burst, in ms */
#define MIB_FILTER_MAX_BURST_ELAPSED_TIME    100   /* maximum allowed elapsed time since the beginning of the burst, in ms */
#define MIB_FILTER_RECOVERY_TIME             1000000   /* minimum time, in ms, the MIB interrupts must be kept disabled after a burst has been detected */
#else
#define MIB_FILTER_COUNT_THRESHOLD           500   /* number of interrupts to characterize a long burst ("storm") */
#define MIB_FILTER_MAX_INTR_ELAPSED_TIME      20   /* maximum allowed time interval for between 2 consecutive interrupts within the same burst, in ms */
#define MIB_FILTER_MAX_BURST_ELAPSED_TIME    100   /* maximum allowed elapsed time since the beginning of the burst, in ms */
#define MIB_FILTER_RECOVERY_TIME              50   /* minimum time, in ms, the MIB interrupts must be kept disabled after a burst has been detected */
#endif

typedef struct ath_intr_filter {
    INTR_FILTER_STATE    state;                  /* state machine's state */
    u_int32_t            activation_count;       /* number of storms detected so far */
    systime_t            burst_start_time;       /* timestamp of the first interrupt determined to be part of a burst */
    systime_t            last_intr_time;         /* timestamp of the last interrupt */
    u_int32_t            intr_count;             /* number of interrupts in the current storm */
} ATH_INTR_FILTER;
#endif

typedef spinlock_t ath_beacon_lock_t;

#if ATH_RESET_SERIAL
typedef struct ath_defer_txbuf {
    spinlock_t      lock;
    ath_bufhead     txbuf_head;
} ATH_DEFER_TXBUF;
#endif

typedef struct {
    ath_gpio_intr_func_t func;
    void *context;
} ath_gpio_callback_t;

struct ath_softc {
    struct ath_ops          sc_ath_ops;

    ieee80211_handle_t      sc_ieee;
    struct ieee80211_ops    *sc_ieee_ops;

    struct wlan_objmgr_pdev    *sc_pdev;
    struct wlan_lmac_if_rx_ops *sc_rx_ops;

    void                   *sc_scanner;
    osdev_t                 sc_osdev;   /* Opaque handle to OS-specific device */
    spinlock_t              sc_lock;    /* softc-level lock */
    qdf_device_t            qdf_dev;    /* adf handle */

    struct ath_reg_parm     sc_reg_parm;/* per-instance attach-time parameters */
    struct ath_config       sc_config;  /* per-instance load-time parameters */
    struct ath_stats        sc_stats;   /* private statistics */

#ifdef ATH_SUPPORT_DFS
    u_int32_t                sc_dfsdomain;
    int                         sc_dfs_wait;
    uint32_t                ath_dfs_ext_chan_ok:1,
                            ath_dfs_combined_rssi_ok:1,
                            ath_dfs_use_enhancement:1,
                            ath_strong_signal_diversiry:1,
                            ath_chip_is_bb_tlv:1,
                            ath_chip_is_over_sampled:1,
                            ath_chip_is_ht160:1,
                            ath_chip_is_false_detect:1;
    uint32_t                ath_fastdiv_val;
#endif

#ifdef ATH_TX99_DIAG
    struct ath_tx99         *sc_tx99;
    TAILQ_HEAD(,ath_buf)    tx99_tmpq;
#endif
#if WLAN_SPECTRAL_ENABLE
    void* sc_spectral;
    struct ath_spectral_ops    p_sops;
    void    (*ath_start_spectral_scan)(void* ps);
    void    (*ath_stop_spectral_scan)(void *ps);
    u_int16_t                  sc_icm_active;
#endif
#if ATH_SUPPORT_RAW_ADC_CAPTURE
    int 				sc_raw_adc_capture_enabled;
    HAL_OPMODE 			sc_save_current_opmode;
    HAL_CHANNEL 		sc_save_current_chan;
    u_int8_t            sc_save_rx_chainmask;
    u_int16_t           sc_adc_num_chains;     /* number of chains */
    u_int16_t           sc_adc_chain_mask;     /* chain mask (lsb=ch0) */
    u_int16_t           sc_adc_freq;         /* setting in Mhz */
    u_int16_t           sc_adc_ieee;         /* IEEE channel number */
    u_int32_t           sc_adc_chan_flags;   /* channel flags */
	u_int32_t			sc_adc_capture_flags;
#endif
    /* private PHY statisitics for each PHY mode */
    struct ath_phy_stats    sc_phy_stats[WIRELESS_MODE_MAX];

    struct asf_print_ctrl   sc_print;
#if ATH_DEBUG
    int                     sc_print_flag;      /* flag for pktlog */
    char                    sc_print_itf_name[8]; /* interface name to print */
#endif

    SetDefAntenna_callback  sc_setdefantenna;
    void                    *sc_bdev;   /* associated bus device */

    struct ath_hal          *sc_ah;     /* Atheros HAL */
    struct atheros_softc    *sc_rc;     /* tx rate control support */

    u_int32_t               sc_intrstatus; /* XXX HAL_STATUS */
    spinlock_t              sc_intrstatus_lock;

    atomic_t                sc_inuse_cnt;

    /* Properties */
    unsigned int
        sc_invalid             : 1, /* being detached */
        sc_needmib             : 1, /* enable MIB stats intr */
        sc_hasdiversity        : 1, /* rx diversity available */
        sc_diversity           : 1, /* enable rx diversity */
        sc_olddiversity        : 1, /* diversity setting before XR enable */
        sc_haswme              : 1, /* wme priority queueing support */
        sc_hasveol             : 1, /* tx VEOL support */
        sc_hastpc              : 1, /* per-packet TPC support */
        sc_dturbo              : 1, /* dynamic turbo capable */
        sc_dturbo_switch       : 1, /* turbo switch mode*/
        sc_rate_recn_state     : 1, /* dynamic turbo state recmded by ratectrl */
        sc_ignore_ar           : 1, /* ignore AR during transision*/
        sc_hasbmask            : 1, /* bssid mask support */
        sc_hastsfadd           : 1, /* tsf adjust support */
        sc_scanning            : 1, /* scanning active */
        sc_nostabeacons        : 1, /* no beacons for station */
        sc_xrgrppoll           : 1, /* xr group polls are active */
        sc_hasclrkey           : 1, /* CLR key supported */
        sc_devstopped          : 1, /* stopped 'cuz no tx bufs */
        sc_stagbeacons         : 1, /* use staggered beacons */
        sc_rtasksched          : 1, /* radar task is scheduled */
        sc_hasrfkill           : 1, /* RfKill is enabled */
#ifdef ATH_RFKILL
        sc_rfkillpollenabled   : 1, /* RFKill poll timer is enabled */
#endif	//ifdef ATH_RFKILL
        sc_hw_phystate         : 1, /* hardware (GPIO) PHY state */
        sc_sw_phystate         : 1, /* software PHY state */
        sc_txaggr              : 1, /* enable 11n tx aggregation */
        sc_rxaggr              : 1, /* enable 11n rx aggregation */
        sc_hasautosleep        : 1, /* automatic sleep after TIM */
        sc_waitbeacon          : 1, /* waiting for first beacon after waking up */
        sc_no_tx_3_chains      : 1, /* user, hardware, regulatory or country
                                     * may disallow transmit on three chains. */
        sc_cus_cts_set         : 1, /* customer specified cts rate */
        sc_allow_promisc       : 1; /* promiscuous allow option */
    unsigned int
        sc_update_chainmask    : 1, /* change chain mask */
        sc_rx_chainmask_detect : 1, /* enable rx chain mask detection */
        sc_rx_chainmask_start  : 1, /* start rx chain mask detection */
        sc_txrifs              : 1, /* enable 11n tx rifs */
        sc_hashtsupport        : 1, /* supports 11n */
        sc_ht20sgisupport      : 1, /* supports short GI in ht20 mode */
        sc_txstbcsupport       : 1,
        sc_rxstbcsupport       : 2,
        sc_log_rcfind          : 1, /* log rcfind */
        sc_txamsdu             : 1, /* enable 11n tx amsdu */
        sc_swRetryEnabled      : 1, /* support for sw retrying mechanism */
        sc_full_reset          : 1, /* force full reset */
        sc_uapsdsupported      : 1, /* supports UAPSD */
#if ATH_WOW
        sc_hasWow              : 1, /* Is WoW feature present in HW */
        sc_wow_sleep           : 1, /* In the middle of Wow Sleep? */
        sc_wow_bmiss_intr      : 1, /* Beacon Miss workaround during WOW sleep. */
        sc_wowenable           : 1, /* plt support */
#if ATH_WOW_OFFLOAD
        sc_wowSupportOffload   : 1, /* Any SW WOW offload feature enabled */
        sc_hasWowGtkOffload    : 1, /* Is GTK rekey offload in WoW present in HW */
        sc_hasWowArpOffload    : 1, /* IPV4 ARP offload */
        sc_hasWowNsOffload     : 1, /* IPV6 NS offload */
        sc_wowSupport4wayHs    : 1, /* 4-way handhshake wake up */
        sc_wowSupportAcerMagic : 1, /* Acer extended Magic Packet format */
        sc_wowSupportAcerSwKa  : 1, /* Acer SW keep alive */
#endif /* ATH_WOW_OFFLOAD */
#endif
        sc_slowAntDiv          : 1, /* enable slow antenna diversity */
        sc_removed             : 1, /* card has been physically removed */
        sc_wpsgpiointr         : 1, /* WPS Push button GPIO interrupt enabled */
#ifdef ATH_BT_COEX
        sc_hasbtcoex           : 1,
#endif
#ifdef ATH_GEN_TIMER
        sc_hasgentimer         : 1,
#endif
#if ATH_ANT_DIV_COMB
        sc_antDivComb          : 1, /* enable antenna diversity and combining feature for KITE */
        sc_antDivComb_record   : 1,
#endif
#if ATH_SUPPORT_WIRESHARK
        sc_tapmonenable        : 1, /* tap monitor mode */
#endif

        sc_wpsbuttonpushed     : 1, /* WPS Push Button status */
        sc_enhanceddmasupport  : 1, /* Enhanced DMA supported */
#ifdef  ATH_SUPPORT_TxBF
        sc_txbfsupport         : 1, /* Tx BF supported */
#endif
        sc_bbpanicsupport      : 1, /* BB Panic supported */
        sc_ldpcsupport         : 2,
        sc_fastabortenabled    : 1, /* fast rx/tx abort */
        sc_hwuapsdtrig         : 1, /* HW Uapsd trigger */
        sc_edmarxdpc           : 1, /* flag to indicate edma rxdpc */
#if ATH_SUPPORT_DYN_TX_CHAINMASK
        sc_dyn_txchainmask     : 1, /* dynamic tx chain support */
#endif /* ATH_SUPPORT_DYN_TX_CHAINMASK */
        sc_enableapm           : 1, /* apm enabled */
        sc_msi_enable          : 1, /* MSI enabled */
#if ATH_TX_DUTY_CYCLE
        sc_tx_dc_enable        : 1, /* tx duty cycle enable flag */
#endif
#if ATH_SUPPORT_FLOWMAC_MODULE
        sc_osnetif_flowcntrl   : 1, /* flow control with os network stack enabled */
        sc_ethflowmac_enable   : 1, /* enable flow control between ethernet and 802.11 interface */
#endif
        sc_enableproxysta      : 1, /* proxy sta enabled */
	sc_cddLimitingEnabled  : 1, /* Disable Cyclic Delay Diversity on the channel for legacy rates */
        sc_pn_checknupdate_war : 1; /* PN check and update WAR specific to chip */
    u_int32_t       sc_3stream_sigflag1 ;  /* signature flag for 3 stream fix */
    u_int32_t       sc_3stream_sigflag2 ;  /* signature flag for 3 stream fix */
    u_int32_t       sc_3streamfix_engaged ;  /* flag for stating 3 stream fix is engaged */


    ATH_RESET_TYPE          sc_reset_type; /* reason for calling ath_internal_reset */
#if ATH_VOW_EXT_STATS
    unsigned int  sc_vowext_stats:1; /* enable/disable extended stats */
#endif

    atomic_t                sc_in_reset;    /* reset in progress */
    atomic_t                sc_rx_hw_en;    /* Rx Hw is enabled and running */

    HAL_OPMODE              sc_opmode;  /* current operating mode */
    u_int8_t                sc_coverageclass;

    /* rate tables */
    const HAL_RATE_TABLE    *sc_rates[WIRELESS_MODE_MAX];
    const HAL_RATE_TABLE    *sc_currates;   /* current rate table */
    const HAL_RATE_TABLE    *sc_half_rates; /* half rate table */
    const HAL_RATE_TABLE    *sc_quarter_rates; /* quarter rate table */
    WIRELESS_MODE           sc_curmode;     /* current phy mode */
    u_int16_t               sc_curtxpow;    /* current tx power limit */
    u_int16_t               sc_curaid;      /* current association id */
    HAL_CHANNEL             sc_curchan;     /* current h/w channel */
    HAL_CHANNEL             sc_extchan;     /* extension h/w channel */
    u_int8_t                sc_curbssid[IEEE80211_ADDR_LEN];
    u_int8_t                sc_myaddr[IEEE80211_ADDR_LEN]; /* current mac address */
    u_int8_t                sc_my_hwaddr[IEEE80211_ADDR_LEN]; /* mac address from EEPROM */
    u_int8_t                sc_rixmap[256]; /* IEEE to h/w rate table ix */
    struct {
        u_int32_t           rateKbps;       /* transfer rate in kbs */
        u_int8_t            ieeerate;       /* IEEE rate */
        u_int8_t            flags;          /* radiotap flags */
        u_int16_t           ledon;          /* softled on time */
        u_int16_t           ledoff;         /* softled off time */
    } sc_hwmap[256];         /* h/w rate ix mappings */
    u_int32_t               sc_max_wep_tkip_ht20_tx_rateKbps;
    u_int32_t               sc_max_wep_tkip_ht40_tx_rateKbps;
    u_int32_t               sc_max_wep_tkip_ht20_rx_rateKbps;
    u_int32_t               sc_max_wep_tkip_ht40_rx_rateKbps;

#if ATH_SUPPORT_IQUE
    struct {
        u_int8_t            per_threshold;
        u_int8_t            probe_interval;
    } sc_rc_params[2];
    struct {
        u_int8_t            use_rts;
        u_int8_t            aggrsize_scaling;
        u_int32_t           min_kbps;
    } sc_ac_params[WME_NUM_AC];
    struct {
        u_int8_t            hbr_enable;
        u_int8_t            hbr_per_low;
    } sc_hbr_params[WME_NUM_AC];
    u_int                   sc_retry_duration;
    u_int8_t                sc_hbr_per_high;
    u_int8_t                sc_hbr_per_low;
#endif

    u_int8_t                sc_minrateix;   /* min h/w rate index */
    u_int8_t                sc_protrix;     /* protection rate index */
    PROT_MODE               sc_protmode;    /* protection mode */
    u_int8_t                sc_mcastantenna;/* Multicast antenna number */
    u_int8_t                sc_txantenna;   /* data tx antenna (fixed or auto) */
    u_int8_t                sc_cur_txant;   /* current tx antenna */
    u_int8_t                sc_nbcnvaps;    /* # of vaps sending beacons */
    u_int8_t                sc_nslots;      /* # of beacon slots allocated for staggered beacons case */
    ath_beacon_lock_t       sc_beacon_lock;    /* lock to be used by beacon module */
    u_int16_t               sc_nvaps;       /* # of active virtual ap's */
    atomic_t                sc_nsta_vaps_up;/* # of active STA virtual macs */
    atomic_t                sc_nap_vaps_up;/* # of active AP virtual macs */
    struct ath_vap          *sc_vaps[ATH_VAPSIZE]; /* interface id to avp map */
#if WAR_DELETE_VAP
    struct ath_vap          *sc_vap_del_pend_head;  /* List of vaps that are hanging due to a node refcount being non-zero */
    struct ath_vap          *sc_vap_del_pend_tail;  /* List of vaps that are hanging due to a node refcount being non-zero */
    u_int32_t               sc_vap_del_pend_count;  /* Number of vaps that are in limbo */
#endif

    HAL_INT                 sc_imask;       /* interrupt mask copy */
    u_int                   sc_keymax;      /* size of key cache */
    u_int8_t                sc_keymap[ATH_KEYBYTES];/* key use bit map */
    struct ieee80211_node   *sc_keyixmap[ATH_KEYMAX];/* key ix->node map */
    u_int8_t                sc_bssidmask[IEEE80211_ADDR_LEN];

    u_int8_t                sc_rxrate;      /* current rx rate for LED */
    u_int8_t                sc_txrate;      /* current tx rate for LED */

    ATH_LED_CONTROL_OBJ(sc_led_control);     /* ath led control */

    struct ath_force_ppm    sc_ppm_info;    /* PPM info */

    int                     sc_rxbufsize;   /* rx size based on mtu */
    struct ath_descdma      sc_rxdma;       /* RX descriptors */
    ath_bufhead             sc_rxbuf;       /* receive buffer */
    struct ath_buf *        sc_rxfreebuf;   /* free athbuf -head - need buffers to be allocated */
    struct ath_buf *        sc_rxfreetail;  /* free athbuf -tail-need buffers to be allocated */
    u_int32_t               sc_num_rxbuf;
#if USE_MULTIPLE_BUFFER_RCV || AP_MULTIPLE_BUFFER_RCV
    wbuf_t                  sc_rxpending;   /* pending 1st half of 2 buf rx */
    struct ath_buf *        sc_bfpending;   /* athbuf corresponding to sc_rxpending */
#if AP_MULTIPLE_BUFFER_RCV
    u_int32_t               sc_prebuflen;   /* record the current size */
    u_int32_t               sc_mergecheck; /* check the which fram to start merge use only for TX BF delay report*/
#endif
#endif /* USE_MULTIPLE_BUFFER_RCV || AP_MULTIPLE_BUFFER_RCV*/
    u_int8_t                sc_rxtimeout[WME_NUM_AC];
    u_int32_t               *sc_rxlink;     /* link ptr in last RX desc */
    spinlock_t              sc_rxflushlock; /* lock of RX flush */
    int32_t                 sc_rxflush;     /* rx flush in progress */
    spinlock_t              sc_rxbuflock;
    unsigned long           sc_rxbuflockflags;
    u_int8_t                sc_defant;      /* current default antenna */
    u_int8_t                sc_rxotherant;  /* rx's on non-default antenna*/
    u_int16_t               sc_cachelsz;    /* cache line size */
    u_int32_t               sc_rc_rx_mgmt_num; /* old rx mgmt pkt number for rate control*/

    /* Enhanced DMA support */
    u_int32_t               sc_num_txmaps;
    ATH_RX_EDMA_CONTROL_OBJ(sc_rxedma);
    spinlock_t              sc_txstatuslock;/* tx status ring lock (edma) */
    struct ath_descdma      sc_txsdma;
    u_int32_t               sc_txstatuslen; /* Tx descriptor status length */
    u_int32_t               sc_rxstatuslen; /* Rx descriptor status length */
    struct ath_buf          *sc_rxbufptr;
    struct ath_rx_status    *sc_rxsptr;

    struct ath_descdma      sc_txdma;       /* TX descriptors */
    ath_bufhead             sc_txbuf;       /* transmit buffer */
#if ATH_RESET_SERIAL
    struct ath_defer_txbuf  sc_queued_txbuf[HAL_NUM_TX_QUEUES]; /*queued txbuf */
    atomic_t                sc_hold_reset; /* reset or not */
    atomic_t                sc_dpc_stop;
    atomic_t                sc_reset_queue_flag; /*queue buf or not*/
    atomic_t                sc_tx_add_processing;
    atomic_t                sc_rx_return_processing;
    atomic_t                sc_dpc_processing;
    atomic_t                sc_intr_processing;
    OS_MUTEX                sc_reset_mutex;
    OS_MUTEX                sc_reset_hal_mutex;
#endif
#if ATH_LOW_PRIORITY_CALIBRATE
    ath_defer_call_t        sc_calibrate_deferred;
#endif
#if TRACE_TX_LEAK
    ath_bufhead             sc_tx_trace_head;
#endif //TRACE_TX_LEAK

    u_int                   sc_txbuf_free;  /* No. of free tx buffers */
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
    struct ath_ald_record   sc_ald; /* statistics holder */
#endif
    spinlock_t              sc_txbuflock;   /* txbuf lock */
    unsigned long           sc_txbuflockflags;
    u_int                   sc_txqsetup;    /* h/w queues setup */
    u_int                   sc_txintrperiod;/* tx interrupt batching */
    struct ath_txq          sc_txq[HAL_NUM_TX_QUEUES];
    int                     sc_haltype2q[HAL_WME_AC_VO+1]; /* HAL WME AC -> h/w qnum */
    HAL_TXQ_INFO            sc_beacon_qi;   /* adhoc only: beacon queue parameters */
    u_int32_t               sc_txdesclen;   /* Tx descriptor length */
#ifdef ATH_SUPPORT_UAPSD
    struct ath_descdma      sc_uapsdqnuldma;    /* U-APSD QoS NULL TX descriptors */
    ath_bufhead             sc_uapsdqnulbf;     /* U-APSD QoS NULL free buffers */
    spinlock_t              sc_uapsdirqlock;    /* U-APSD IRQ lock */
    unsigned long           sc_uapsdirqlockflags;
    int                     sc_uapsdqnuldepth;  /* U-APSD delivery queue depth */
    bool                    sc_uapsd_pause_in_progress;
    bool                    sc_uapsd_trigger_in_progress;
#endif

    struct ath_descdma      sc_bdma;        /* beacon descriptors */
    ath_bufhead             sc_bbuf;        /* beacon buffers */
    u_int                   sc_bhalq;       /* HAL q for outgoing beacons */
    u_int                   sc_bmisscount;  /* missed beacon transmits */
#if UMAC_SUPPORT_WNM
    struct ath_descdma      sc_tdma;        /* TIM descriptors */
    ath_bufhead             sc_tbuf;        /* TIM buffers */
#endif
    u_int                   sc_consecutive_gtt_count;  /* consecutive gtt intr count */
    u_int                   sc_noise;       /* noise detected on channel */
    u_int                   sc_toggle_immunity; /* toggle immunity parameters */
    u_int32_t               sc_ant_tx[8];   /* recent tx frames/antenna */
    struct ath_txq          *sc_cabq;       /* tx q for cab frames */
    struct ath_txq          *sc_uapsdq;     /* tx q for uapsd */
    struct ath_txq          sc_burstbcn_cabq;/* staging q for mcast frames when bursted beaconing */
#if ATH_SUPPORT_CFEND
    struct ath_txq          *sc_cfendq;       /* tx q for CF-END frame */
    struct ath_descdma      sc_cfenddma;    /* CF-END Tx descriptor */
    ath_bufhead             sc_cfendbuf;    /* CF-END free buffer */
    u_int8_t                rxLastAggrBssid[6];      /* Last Aggregation BSSID */
#endif
//PAPRD related vars
    int8_t                  sc_paprd_rate;           /* PAPRD TX rate */
    int8_t                  sc_paprd_enable;         /* PAPRD enabled */
    int8_t                  sc_paprd_done_chain_mask;      /* PAPRD Done on chain num */
    int8_t                  sc_paprd_chain_num;      /* chain num - current PAPRD frame to be sent*/
    int8_t                  sc_paprd_abort;          /* PAPRD abort */
    int8_t                  sc_paprd_failed_count;   /* PAPRD failed count */
    int8_t                  sc_paprd_retrain_count;  /* PAPRD re-train failed count */
    int8_t                  sc_paprd_cal_started;    /* PAPRD CAL started */
    int16_t                 sc_paprd_bufcount;       /* Total paprd buf need to send for 1 paprd CAL */
    spinlock_t              sc_paprd_lock;
    struct ath_txq          *sc_paprdq;             /* tx q for PAPRD frame */
    struct ath_descdma      sc_paprddma;            /* PAPRD Tx descriptor */
    ath_bufhead             sc_paprdbuf;            /* PAPRD free buffer */
    u_int16_t               sc_paprd_lastchan_num;  /* Last chan PAPRD Cal completed */
    int8_t                  sc_paprd_thermal_packet_not_sent;       /* PAPRD thermal packet */
    int8_t                  sc_paprd_ontraining;    /* PAPRD is on training */
//End PAPRD vars
    sc_updateslot_t         sc_updateslot;  /* slot time update fsm */
    int                     sc_slotupdate;  /* slot to next advance fsm */
    int                     sc_slottime;    /* slot time */
    int                     sc_bslot[ATH_BCBUF];/* beacon xmit slots */
    int                     sc_bnext;       /* next slot for beacon xmit */

    struct ath_timer        sc_cal_ch;      /* calibration timer */
    struct ath_timer        sc_up_beacon;       /* beacon config timer after vap up */
    u_int8_t                sc_up_beacon_purge; /* purge beacon config after vap up */
    HAL_NODE_STATS          sc_halstats;    /* station-mode rssi stats */

    u_int16_t               sc_reapcount;   /* # of tx buffers reaped after net dev stopped */
    int16_t                 sc_noise_floor; /* signal noise floor in dBm */
    u_int8_t                sc_noreset;
    u_int8_t                sc_limit_legacy_frames;

    int8_t                   sc_noise_floor_th; /* signal noise floor in dBm used in ch hoping */
    u_int16_t                sc_noise_floor_report_iter; /* #of iteration noise is higher then threshold */
    u_int16_t                sc_noise_floor_total_iter;/* total # of iteration */
    u_int8_t                 sc_enable_noise_detection; /* Enable/Disable noise detection due to channel hopping in acs */

    u_int8_t                sc_consecutive_rxeol_count; /* consecutive rxeol intr count */
#ifdef ATH_SUPERG_DYNTURBO
    os_timer_t              sc_dturbo_switch_mode;  /* AP scan timer */
    u_int32_t               sc_dturbo_tcount;       /* beacon intval count */
    u_int32_t               sc_dturbo_hold_max;     /* hold count before switching to base*/
    u_int16_t               sc_dturbo_hold_count;   /* hold count before switching to base*/
    u_int16_t               sc_dturbo_turbo_tmin;   /* min turbo count */
    u_int32_t               sc_dturbo_bytes;        /* bandwidth stats */
    u_int32_t               sc_dturbo_base_tmin;    /* min time in base */
    u_int32_t               sc_dturbo_turbo_tmax;   /* max time in turbo */
    u_int32_t               sc_dturbo_bw_base;      /* bandwidth threshold */
    u_int32_t               sc_dturbo_bw_turbo;     /* bandwidth threshold */
#endif

    HAL_HT_EXTPROTSPACING   sc_ht_extprotspacing;

    u_int8_t                *sc_mask2chains;
    u_int8_t                sc_tx_numchains;
    u_int8_t                sc_rx_numchains;

#ifdef ATH_TX_INACT_TIMER
    os_timer_t              sc_inact;      /* inactivity timer */
    u_int8_t                sc_tx_inact;
    u_int8_t                sc_max_inact;
#endif

#if ATH_HW_TXQ_STUCK_WAR
    os_timer_t              sc_qstuck;      /* q stuck monitoring timer */
    systime_t               sc_last_rxeol;  /* last Rx EOL interrupt receive time */
#endif

    u_int8_t                sc_tx_chainmask;
    u_int8_t                sc_rx_chainmask;
    u_int8_t                sc_rxchaindetect_ref;
    u_int8_t                sc_rxchaindetect_thresh5GHz;
    u_int8_t                sc_rxchaindetect_thresh2GHz;
    u_int8_t                sc_rxchaindetect_delta5GHz;
    u_int8_t                sc_rxchaindetect_delta2GHz;
    u_int8_t                sc_cfg_txchainmask;     /* initial chainmask config */
    u_int8_t                sc_cfg_rxchainmask;
    u_int8_t                sc_cfg_txchainmask_leg;
    u_int8_t                sc_cfg_rxchainmask_leg;

    /*
     * In certain case, we need to attach the LMAC/HAL with one txchainmask,
     * however use another for actual data transmission.
     */
    u_int8_t                sc_tx_chainmaskopt;

    u_int32_t               sc_rtsaggrlimit; /* Chipset specific aggr limit */
    ATH_PWRSAVE_CONTROL_OBJ(sc_pwrsave);     /* power save control */
    u_int32_t               sc_pktlog_enable;
    systime_t               sc_shortcal_timer;
    systime_t               sc_longcal_timer;
    systime_t               sc_ani_timer;
    systime_t               sc_olpc_timer;
    HAL_BOOL                sc_Caldone;
    u_int32_t               sc_ent_min_pkt_size_enable : 1,
                            sc_ent_spectral_mask : 2,
                            sc_ent_rtscts_delim_war : 1;
#ifdef ATH_USB
    usblock_t               sc_resetlock;
#else
    spinlock_t              sc_resetlock;
#endif
    unsigned long           sc_resetlock_flags;
#ifdef ATH_USB
    usblock_t               sc_reset_serialize;
#else
    qdf_spinlock_t       sc_reset_serialize;
#endif
#if ATH_WOW
    struct wow_info         *sc_wowInfo;
#endif

#ifdef ATH_RFKILL
    struct ath_rfkill_info  sc_rfkill;      /* RfKill settings */
#endif

#ifdef ATH_BT_COEX
    struct ath_bt_info      sc_btinfo;      /* BT coexistence settings */
#endif

#ifdef ATH_GEN_TIMER
    struct ath_gen_timer_table  sc_gen_timers;
#endif

#ifndef REMOVE_PKT_LOG
    struct ath_pktlog_info  *pl_info;
    u_int32_t               ah_mac_version; /* shadow copy for pktlog */
    u_int16_t               ah_mac_rev;     /* shadow copy for pktlog */
#endif
    hal_hw_hangs_t          sc_hang_war;
    HAL_BOOL                sc_hang_check;
#if ATH_SUPPORT_VOWEXT
    int                     sc_phyrestart_disable;
#endif
    int                     sc_keysearch_always_enable;

#ifdef ATH_SWRETRY
    u_int32_t                   sc_num_swretries;
    ATH_LIST_HEAD(, ath_node)   sc_nt;          /* node table list */
    spinlock_t                  sc_nt_lock;     /* node table lock */
#endif

#if ATH_SLOW_ANT_DIV
    /* Slow antenna diversity */
    struct ath_antdiv       sc_antdiv;
#endif
    u_int8_t        sc_lastdatarix;    /* last data frame rate index */
    u_int8_t        sc_lastrixflags;/* last data frame rate flags */

#ifdef ATH_MIB_INTR_FILTER
    struct ath_intr_filter  sc_intr_filter;
#endif


#if ATH_TX_DROP_POLICY
	u_int8_t            sc_qoscommonDropIpFrag[WME_NUM_AC];
#endif


#ifdef ATH_SUPPORT_LINUX_STA
#ifdef CONFIG_SYSCTL
    struct ctl_table_header *sc_sysctl_header;
    struct ctl_table        *sc_sysctls;
#endif
#endif

#if ATH_TX_DUTY_CYCLE
    uint32_t                sc_tx_dc_period;        /* tx duty cycle period */
    int8_t                  sc_tx_dc_active_pct;    /* tx duty cycle active percentage  */
#endif

#if ATH_SUPPORT_WIRESHARK
    struct {
        unsigned char
            filter_phy_err : 1;
	} sc_wireshark;
#endif /* ATH_SUPPORT_WIRESHARK */

#if ATH_SUPPORT_CFEND
    spinlock_t              sc_cfend_lock;
#endif
#if ATH_SUPPORT_VOWEXT
    u_int32_t               sc_vowext_flags;
    /* aggregate control related variables*/
    u_int32_t               agtb_tlim;
    u_int32_t               agtb_blim;
    u_int32_t               agthresh;
    /* RCA variables */
    u_int8_t 		    rca_aggr_uptime;
    u_int8_t 		    aggr_aging_factor;
    u_int8_t 		    per_aggr_thresh;
    u_int8_t 		    excretry_aggr_red_factor;
    u_int8_t 		    badper_aggr_red_factor;
    u_int32_t vsp_enable;				/* enable/disable vsp algo */
    u_int32_t vsp_threshold;			/* VSP uses this threshold value (in mbps) to differenatite the good/bad vi streams */
    u_int32_t vsp_evalinterval;			/* Time interval in ms after which vsp will be triggered if required */
    u_int32_t vsp_prevevaltime;			/* Stores the previous time stamp of vsp algo run */
    u_int32_t vsp_vicontention;         /* Counter indicating no of vi contention occurances (incoming video packet sees buffer full cond) */
    u_int32_t vsp_bependrop;            /* Counter indicating no of be packets(bufs) penalized for incoming vi packet */
    u_int32_t vsp_bkpendrop;            /* Counter indicating no of bk packets(bufs) penalized for incoming vi packet */
    u_int32_t vsp_vipendrop;            /* Counter indicating no of low priority (decided b vsp) vi packets(bufs) penalized for incoming vi packet of high priority */
#endif

#if ATH_C3_WAR
    atomic_t                sc_pending_tx_data_frames;
    bool                    sc_fifo_underrun;
    bool                    sc_txop_burst;
    bool                    c3_war_timer_active;
    spinlock_t              sc_c3_war_lock;
    struct ath_gen_timer    *sc_c3_war_timer;
#endif

#if ATH_RX_LOOPLIMIT_TIMER
    struct ath_gen_timer    *sc_rx_looplimit_timer;
    u_int32_t               sc_rx_looplimit_timeout;        /* timeout intval */
    u_int32_t               sc_rx_looplimit;
#endif

#if ATH_ANT_DIV_COMB
    /* antenna diversity & combining */
    struct ath_antcomb      sc_antcomb;
    struct ath_sascan       *sc_sascan;
    u_int8_t                sc_smart_antenna;
#endif
    HAL_CALIBRATION_TIMER   *sc_cals;
	int                     sc_num_cals;
	u_int32_t               sc_min_cal;
	u_int32_t               sc_sched_cals;
#ifdef ATH_SUPPORT_TxBF
    u_int8_t                sc_txbfsounding    :1,
                            sc_txbfcalibrating :1;
    u_int8_t                last_rx_type;
    struct ieee80211_node   *v_cv_node;
    u_int8_t                do_node_check;
    u_int8_t                last_h_invalid;
    u_int8_t                last_be_aggr;
    wbuf_t                  sc_rx_wbuf_waiting;
    struct ath_buf	        *sc_bf_waiting;
    struct ath_rx_status    *sc_rxs_waiting;
    u_int32_t               sounding_rx_kbps;
    struct ath_timer        sc_selfgen_timer;
#endif
    atomic_t                sc_has_tx_bcn_notify;   /* Enable flag for this feature */
    ath_notify_tx_bcn_t     sc_notify_tx_bcn;       /* Opaque handle */
#ifdef ATH_SUPPORT_IQUE
    u_int32_t total_delay_timeout;
#endif
#if ATH_VAP_PAUSE_SUPPORT
    spinlock_t              sc_vap_pause_lock;    /* lock to serialize vap pause operations */
    atomic_t                sc_txq_use_cnt;       /* number of txq users */
    atomic_t                sc_vap_pause_in_progress; /* flag indicating vap pause is in progress */
    bool                    sc_vap_pause_timeout;
#endif
#if ATH_SUPPORT_MCI
    struct ath_mci_coex     sc_mci;
#endif
#if UMAC_SUPPORT_VI_DBG
    u_int32_t               sc_en_vi_dbg_log;
    u_int32_t               sc_vi_dbg_start;
#endif
#ifdef ATH_USB
    u_int32_t               curr_tsf;             /* cache last tsf time, this is used to reduce the wmi command delay */
#endif
    int                     sc_devid;           /*sc needs to know devid, needed to switch to ART mode at runtime */
#if ATH_TRAFFIC_FAST_RECOVER
    struct ath_timer        sc_traffic_fast_recover_timer;
#endif
    u_int16_t tx_power;
#if ATH_SUPPORT_FLOWMAC_MODULE
    void                    *sc_flowmac_dev;
    int                     sc_flowmac_debug;
#endif
#define ATH_BURST_DURATION 5100
    bool                    sc_aggr_burst;
    u_int32_t               sc_aggr_burst_duration;
#if ATH_SUPPORT_RX_PROC_QUOTA
    u_int32_t               sc_rx_work_hp;
	u_int32_t               sc_rx_work_lp;
    u_int32_t               sc_process_rx_num; /*No of packets to process per rx schedule */
#endif

#define ATH_GPIO_MAX_NUM 32
    u_int32_t               sc_gpio_mask;
    u_int32_t               sc_gpio_polarity_map;
    ath_gpio_callback_t     sc_gpio_intr_handle[ATH_GPIO_MAX_NUM];
#if ATH_SUPPORT_WIFIPOS
    u_int64_t      sc_tsf_tstamp;
     struct ath_txq  *sc_wifiposq_oc;             /* tx q for wifipositioning probe frames */
    struct ath_txq  *sc_wifiposq_hc;          /* tx q for wifipositioning probe frames in homce channel  */
#endif
    /* Flag used to enable/disable DCS. Default to be set to
     * 1 and can be changed to 0/1 from command line
     */
    u_int32_t sc_dcs_enabled;
#if ATH_SUPPORT_VOW_DCS
    struct ath_dcs_params      sc_dcs_params;
    u_int16_t                  sc_phyerr_penalty;
    u_int8_t                   sc_coch_intr_thresh;
    u_int8_t                   sc_tx_err_thresh;
#endif
#if ATH_GEN_RANDOMNESS
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,8)
    uint32_t sc_random_value;
#else
    uint8_t  sc_random_value;
#endif

    uint8_t sc_random_pos;
#endif
#if !ATHR_PROXYSTA_MAT
    int sc_keycheck_enabled; /* Periodic key cache checking to detect MAC hang in ProxySTA mode */
    spinlock_t  sc_keycheck_lock;
    os_timer_t  sc_keycheck_timer;
#endif
    u_int32_t sc_nodebug;
#if ATH_DATABUS_ERROR_RESET_LOCK_3PP
    spinlock_t              sc_internal_reset_lock;
    unsigned long           sc_internal_reset_flag;
#endif
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
    u_int32_t sc_aldstatsenabled;
#endif
#if UNIFIED_SMARTANTENNA
    u_int8_t max_fallback_rates;
    u_int32_t smart_ant_tx_default;
    int8_t sc_smart_ant_mode;
    int8_t sc_smart_ant_enable; /* A bit map: bit 0 : 0 - SA disable, 1- SA enable
                                 * bit 4 : 0 - Tx feedback disable, 1 - Tx feedback enable,
                                 * bit 5 : 0 - Rx feedback disable, 1 - Rx feedback enable,
                                 */
#endif
    u_int16_t sc_intval;
#if ATH_TxBF_DYNAMIC_LOF_ON_N_CHAIN_MASK
    u_int32_t        sc_loforce_enable      : 1; /* loforece enable/disable */
#endif
    /*Rate in Kbps below this rate for first
      series of rate rts/Cts may get enabled */
    u_int32_t  sc_limitrate;
    /* rts/cts enabled */
    u_int16_t  sc_en_rts_cts;
    u_int16_t  sc_user_en_rts_cts;
    bool       sc_is_blockdfs_set;
#if QCA_AIRTIME_FAIRNESS
    u_int16_t
                sc_atf_enable                  : 2, /* enable/disable atf */
                sc_atf_strictsched             : 1, /* enable/disable strict scheduling */
                sc_use_aggr_for_possible_tput  : 2; /* take into account aggr for estimating */
    u_int8_t    sc_adjust_tput_more;
    u_int32_t   sc_atf_tokens_unassigned;
    u_int32_t   sc_atf_tokens_unassigned_adj;
    u_int16_t   sc_num_buf_held;
    u_int32_t   sc_adjust_possible_tput;
    u_int32_t   sc_adjust_tput;
    u_int32_t   sc_adjust_tput_more_thrs;
#endif
    os_timer_t              sc_rxstuck;     /* rx stuck monitoring timer */
    os_timer_t              sc_hwstuck;     /* hw stuck monitoring timer */
    bool                    sc_rxstuck_shutdown_flag;  /*rx stuck timer monitor flag */
    u_int32_t sc_last_rxorn;
    u_int32_t sc_last_lp_rx_dp;
    struct ath_timer        sc_chan_busy;
    u_int32_t   sc_chan_busy_per;       /* Channel busy percentage */
#if ATH_GEN_RANDOMNESS
    u_int32_t rng_last;
    struct task_struct *rng_task;
#endif
    u_int32_t sc_11b_data_dur;      /* total duration for the received 11b data packets */
    u_int32_t sc_rx_packet_dur;     /* Total duration for all rx packets */
#if ATH_BT_COEX_3WIRE_MODE
    u_int32_t sc_3wire_bt_coex_enable;
#endif
    atomic_t                sc_hwif_stop_refcnt;/*atomic variable to check if HW Stop in Progress*/
    u_int8_t                sc_isscan;
};

typedef enum {
    ATH_BEACON_CONFIG_REASON_VAP_DOWN,
    ATH_BEACON_CONFIG_REASON_VAP_UP,
    ATH_BEACON_CONFIG_REASON_RESET,
    ATH_BEACON_CONFIG_REASON_OPMODE_SWITCH
} ath_beacon_config_reason;

#if ATH_DEBUG
static inline const char *
ath_beacon_config_reason_name(ath_beacon_config_reason reason)
{
    switch (reason) {
    case ATH_BEACON_CONFIG_REASON_VAP_DOWN:
        return "VAP_DOWN";
    case ATH_BEACON_CONFIG_REASON_VAP_UP:
        return "VAP_UP";
    case ATH_BEACON_CONFIG_REASON_RESET:
        return "RESET";
    case ATH_BEACON_CONFIG_REASON_OPMODE_SWITCH:
        return "OPMODE_SWITCH";
    default:
        return "UNKNOWN";
    }
}
#else
#define ath_beacon_config_reason_name(reason) ((const char *) 0x0)
#endif /* ATH_DEBUG */

#define ATH_DEV_TO_SC(_dev)         ((struct ath_softc *)(_dev))


typedef void (*ath_callback) (struct ath_softc *);

#define    ATH_TXQ_SETUP(sc, i)        ((sc)->sc_txqsetup & (1<<i))

#define	   ATH_TXBUF_LOCK_INIT(_sc)    spin_lock_init(&(_sc)->sc_txbuflock)
#define	   ATH_TXBUF_LOCK_DESTROY(_sc)
#define    ATH_TXBUF_LOCK(_sc)         do{os_tasklet_lock(&(_sc)->sc_txbuflock, (_sc)->sc_txbuflockflags);}while(0)
#define    ATH_TXBUF_UNLOCK(_sc)       do{os_tasklet_unlock(&(_sc)->sc_txbuflock, (_sc)->sc_txbuflockflags);}while(0)

#define    ATH_RXBUF_LOCK_INIT(_sc)    spin_lock_init(&(_sc)->sc_rxbuflock)
#define    ATH_RXBUF_LOCK_DESTROY(_sc) spin_lock_destroy(&(_sc)->sc_rxbuflock)

#if ((ATH_SUPPORT_UAPSD) || (defined CONFIG_SMP))
#define    ATH_RXBUF_LOCK(_sc)         spin_lock_irqsave(&(_sc)->sc_rxbuflock, (_sc)->sc_rxbuflockflags)
#define    ATH_RXBUF_UNLOCK(_sc)       spin_unlock_irqrestore(&(_sc)->sc_rxbuflock, (_sc)->sc_rxbuflockflags)
#define    ATH_RXBUF_LOCK_IN_ISR(_sc)   ATH_RXBUF_LOCK(_sc)
#define    ATH_RXBUF_UNLOCK_IN_ISR(_sc) ATH_RXBUF_UNLOCK(_sc)
#else
#define    ATH_RXBUF_LOCK(_sc)         spin_lock(&(_sc)->sc_rxbuflock)
#define    ATH_RXBUF_UNLOCK(_sc)       spin_unlock(&(_sc)->sc_rxbuflock)
#define    ATH_RXBUF_LOCK_IN_ISR(_sc)
#define    ATH_RXBUF_UNLOCK_IN_ISR(_sc)
#endif

#if (ATH_SUPPORT_TXPOWER_ADJUST)
#ifdef ATH_USB
#define ATH_TXPOWER_INIT(_sc)         OS_USB_LOCK_INIT(&(_sc->sc_config.txpow_lock))
#define ATH_TXPOWER_DESTROY(_sc)      OS_USB_LOCK_DESTROY(&(_sc->sc_config.txpow_lock))
#define ATH_TXPOWER_LOCK(_sc)         OS_USB_LOCK(&(_sc->sc_config.txpow_lock))
#define ATH_TXPOWER_UNLOCK(_sc)       OS_USB_UNLOCK(&(_sc->sc_config.txpow_lock))
#else
#define ATH_TXPOWER_INIT(_sc)         spin_lock_init(&(_sc->sc_config.txpow_lock))
#define ATH_TXPOWER_DESTROY(_sc)      spin_lock_destroy(&(_sc->sc_config.txpow_lock))
#define ATH_TXPOWER_LOCK(_sc)         spin_lock(&(_sc->sc_config.txpow_lock))
#define ATH_TXPOWER_UNLOCK(_sc)       spin_unlock(&(_sc->sc_config.txpow_lock))
#endif /* ATH_USB */
#else
#define ATH_TXPOWER_INIT(_sc)
#define ATH_TXPOWER_DESTROY(_sc)
#define ATH_TXPOWER_LOCK(_sc)
#define ATH_TXPOWER_UNLOCK(_sc)
#endif


#ifdef ATH_SUPPORT_UAPSD
#define    ATH_UAPSD_LOCK_INIT(_sc)    spin_lock_init(&(_sc)->sc_uapsdirqlock)
#define    ATH_UAPSD_LOCK_DESTROY(_sc) spin_lock_destroy(&(_sc)->sc_uapsdirqlock)
#define    ATH_UAPSD_LOCK_IRQ(_sc)     spin_lock_irqsave(&(_sc)->sc_uapsdirqlock, (_sc)->sc_uapsdirqlockflags)
#define    ATH_UAPSD_UNLOCK_IRQ(_sc)   spin_unlock_irqrestore(&(_sc)->sc_uapsdirqlock, (_sc)->sc_uapsdirqlockflags)
#define    ATH_HTC_UAPSD_LOCK(_sc)
#define    ATH_HTC_UAPSD_UNLOCK(_sc)
#endif

#if ATH_DATABUS_ERROR_RESET_LOCK_3PP
#define   ATH_INTERNAL_RESET_LOCK_INIT(_sc)       spin_lock_init(&(_sc)->sc_internal_reset_lock)
#define   ATH_INTERNAL_RESET_LOCK_DESTROY(_sc)    spin_lock_destroy(&(_sc)->sc_internal_reset_lock)
#ifdef ATH_BEACON_DEFERRED_PROC
#define   ATH_INTERNAL_RESET_LOCK(_sc)            spin_lock_dpc(&(_sc)->sc_internal_reset_lock)
#define   ATH_INTERNAL_RESET_UNLOCK(_sc)          spin_unlock_dpc(&(_sc)->sc_internal_reset_lock)
#else
#define   ATH_INTERNAL_RESET_LOCK(_sc)            spin_lock_irqsave(&(_sc)->sc_internal_reset_lock, (_sc)->sc_internal_reset_flag)
#define   ATH_INTERNAL_RESET_UNLOCK(_sc)          spin_unlock_irqrestore(&(_sc)->sc_internal_reset_lock, (_sc)->sc_internal_reset_flag)
#endif /* ATH_BEACON_DEFERRED_PROC */
#endif /* ATH_DATABUS_ERROR_RESET_LOCK_3PP */

#if ATH_RESET_SERIAL
#define   ATH_RESET_LOCK_Q_INIT(_sc, _i)       spin_lock_init(&((_sc)->sc_queued_txbuf[_i].lock))
#define   ATH_RESET_LOCK_Q_DESTROY(_sc, _i)    spin_lock_destroy(&((_sc)->sc_queued_txbuf[_i].lock))
//#define   ATH_RESET_LOCK_Q(_sc, _i)            spin_lock(&((_sc)->sc_queued_txbuf[_i].lock))
//#define   ATH_RESET_UNLOCK_Q(_sc, _i)          spin_unlock(&((_sc)->sc_queued_txbuf[_i].lock))
#define   ATH_RESET_LOCK_Q(_sc, _i)
#define   ATH_RESET_UNLOCK_Q(_sc, _i)


#define   ATH_RESET_ACQUIRE_MUTEX(_sc)          acquire_mutex(&(_sc)->sc_reset_mutex)
#define   ATH_RESET_RELEASE_MUTEX(_sc)          release_mutex(&(_sc)->sc_reset_mutex)
#define   ATH_RESET_INIT_MUTEX(_sc)             mutex_init(&(_sc)->sc_reset_mutex)
#define   ATH_RESET_DESTROY_MUTEX(_sc)

#define   ATH_RESET_HAL_ACQUIRE_MUTEX(_sc)          acquire_mutex(&(_sc)->sc_reset_hal_mutex)
#define   ATH_RESET_HAL_RELEASE_MUTEX(_sc)          release_mutex(&(_sc)->sc_reset_hal_mutex)
#define   ATH_RESET_HAL_INIT_MUTEX(_sc)             mutex_init(&(_sc)->sc_reset_hal_mutex)
#define   ATH_RESET_HAL_DESTROY_MUTEX(_sc)

#endif

#ifdef ATH_USB
#define	   ATH_RESET_LOCK_INIT(_sc)    OS_USB_LOCK_INIT(&(_sc)->sc_resetlock)
#define	   ATH_RESET_LOCK_DESTROY(_sc) OS_USB_LOCK_DESTROY(&(_sc)->sc_resetlock)
#define	   ATH_RESET_LOCK(_sc)         OS_USB_LOCK(&(_sc)->sc_resetlock)
#define	   ATH_RESET_UNLOCK(_sc)       OS_USB_UNLOCK(&(_sc)->sc_resetlock)
#else
#define    ATH_RESET_LOCK_INIT(_sc)    spin_lock_init(&(_sc)->sc_resetlock)
#define    ATH_RESET_LOCK_DESTROY(_sc)

#ifdef ATH_SUPPORT_LINUX_STA
#define    ATH_RESET_LOCK(_sc)         spin_lock_dpc(&(_sc)->sc_resetlock)
#define    ATH_RESET_UNLOCK(_sc)       spin_unlock_dpc(&(_sc)->sc_resetlock)
#else
#define    ATH_RESET_LOCK(_sc)       spin_lock_irqsave(&(_sc)->sc_resetlock, _sc->sc_resetlock_flags)
#define    ATH_RESET_UNLOCK(_sc)     spin_unlock_irqrestore(&(_sc)->sc_resetlock, _sc->sc_resetlock_flags)
#endif
#endif /* ATH_USB */

#ifdef ATH_USB
#define    ATH_RESET_SERIALIZE_LOCK_INIT(_sc)    OS_USB_LOCK_INIT(&(_sc)->sc_reset_serialize)
#define    ATH_RESET_SERIALIZE_LOCK_DESTROY(_sc) OS_USB_LOCK_DESTROY(&(_sc)->sc_reset_serialize)
#define    ATH_RESET_SERIALIZE_LOCK(_sc)         OS_USB_LOCK(&(_sc)->sc_reset_serialize)
#define    ATH_RESET_SERIALIZE_UNLOCK(_sc)       OS_USB_UNLOCK(&(_sc)->sc_reset_serialize)
#define    ATH_RESET_SERIALIZE_TRYLOCK(_sc)      OS_USB_TRYLOCK(&(_sc)->sc_reset_serialize)
#else
#define    ATH_RESET_SERIALIZE_LOCK_INIT(_sc)    qdf_spinlock_create(&(_sc)->sc_reset_serialize)
#define    ATH_RESET_SERIALIZE_LOCK_DESTROY(_sc) qdf_spinlock_destroy(&(_sc)->sc_reset_serialize)
#define    ATH_RESET_SERIALIZE_LOCK(_sc)         qdf_spin_lock_bh(&(_sc)->sc_reset_serialize)
#define    ATH_RESET_SERIALIZE_UNLOCK(_sc)       qdf_spin_unlock_bh(&(_sc)->sc_reset_serialize)
#define    ATH_RESET_SERIALIZE_TRYLOCK(_sc)      qdf_spin_trylock_bh(&(_sc)->sc_reset_serialize)
#endif

#if ATH_SUPPORT_SHARED_IRQ
#define		ATH_LOCK_PCI_IRQ(_sc)		do {	\
		ATH_LOCK_IRQ(_sc->sc_osdev);	\
		ATH_RESET_SERIALIZE_LOCK(_sc);	\
} while (0)
#define		ATH_UNLOCK_PCI_IRQ(_sc)		do {	\
		ATH_RESET_SERIALIZE_UNLOCK(_sc);	\
		ATH_UNLOCK_IRQ(_sc->sc_osdev);				\
}while (0)
#else
#define         ATH_LOCK_PCI_IRQ(_sc)           ATH_RESET_SERIALIZE_LOCK(_sc)
#define         ATH_UNLOCK_PCI_IRQ(_sc)         ATH_RESET_SERIALIZE_UNLOCK(_sc)
#endif

#define    ATH_RXFLUSH_LOCK_INIT(_sc)       spin_lock_init(&(_sc)->sc_rxflushlock)
#define    ATH_RXFLUSH_LOCK_DESTROY(_sc)    spin_lock_destroy(&(_sc)->sc_rxflushlock)
#define    ATH_RXFLUSH_LOCK(_sc)            spin_lock(&(_sc)->sc_rxflushlock)
#define    ATH_RXFLUSH_UNLOCK(_sc)          spin_unlock(&(_sc)->sc_rxflushlock)

#define    ATH_INTR_STATUS_LOCK_INIT(_sc)     spin_lock_init(&(_sc)->sc_intrstatus_lock)
#define    ATH_INTR_STATUS_LOCK_DESTROY(_sc)  spin_lock_destroy(&(_sc)->sc_intrstatus_lock)
#define    ATH_INTR_STATUS_LOCK(_sc)          spin_lock(&(_sc)->sc_intrstatus_lock)
#define    ATH_INTR_STATUS_UNLOCK(_sc)        spin_unlock(&(_sc)->sc_intrstatus_lock)


#ifdef ATH_SWRETRY
#define    ATH_NODETABLE_LOCK_INIT(_sc)       spin_lock_init(&(_sc)->sc_nt_lock)
#define    ATH_NODETABLE_LOCK_DESTROY(_sc)    spin_lock_destroy(&(_sc)->sc_nt_lock)
#define    ATH_NODETABLE_LOCK(_sc)            spin_lock(&(_sc)->sc_nt_lock)
#define    ATH_NODETABLE_LOCK_BH(_sc)         spin_lock_dpc(&(_sc)->sc_nt_lock)
#define    ATH_NODETABLE_UNLOCK(_sc)          spin_unlock(&(_sc)->sc_nt_lock)
#define    ATH_NODETABLE_UNLOCK_BH(_sc)       spin_unlock_dpc(&(_sc)->sc_nt_lock)
#endif

#define    ATH_BEACON_LOCK_INIT(_sc)       spin_lock_init(&(_sc)->sc_beacon_lock)
#define    ATH_BEACON_LOCK_DESTROY(_sc)    spin_lock_destroy(&(_sc)->sc_beacon_lock)
#define    ATH_BEACON_LOCK(_sc)            spin_lock(&(_sc)->sc_beacon_lock)
#define    ATH_BEACON_UNLOCK(_sc)          spin_unlock(&(_sc)->sc_beacon_lock)

#if ATH_SUPPORT_WIRESHARK
    #define ATH_WIRESHARK_FILTER_PHY_ERR(sc) sc->sc_wireshark.filter_phy_err
#else
    #define ATH_WIRESHARK_FILTER_PHY_ERR(sc) 1
#endif

#if ATH_SUPPORT_VOWEXT

#define ATH_11N_AGTB_TLIM  5000
#define ATH_11N_AGTB_THRESH 8
#define ATH_11N_AGTB_BLIM  ATH_11N_AGTB_THRESH *(4+1540)

/* RCA thresholds */
#define ATH_11N_RCA_AGGR_UPTIME 25
#define ATH_11N_AGGR_AGING_FACTOR 2
#define ATH_11N_PER_AGGR_THRESH 26
#define ATH_11N_EXCRETRY_AGGR_RED_FACTOR 3
#define ATH_11N_BADPER_AGGR_RED_FACTOR 2

#define ATH_IS_VOWEXT_FAIRQUEUE_ENABLED(_sc)\
    ((_sc)->sc_vowext_flags & ATH_CAP_VOWEXT_FAIR_QUEUING)

#define ATH_IS_VOWEXT_AGGRSIZE_ENABLED(_sc)\
    ((_sc)->sc_vowext_flags & ATH_CAP_VOWEXT_AGGR_SIZING)

#define ATH_IS_VOWEXT_BUFFREORDER_ENABLED(_sc)\
    ((_sc)->sc_vowext_flags & ATH_CAP_VOWEXT_BUFF_REORDER)

#if ATH_SUPPORT_IQUE

#define ATH_IS_VOWEXT_RCA_ENABLED(_sc)\
    ((_sc)->sc_vowext_flags & ATH_CAP_VOWEXT_RATE_CTRL_N_AGGR) /* for RCA */

#else

#define ATH_IS_VOWEXT_RCA_ENABLED(_sc) 0

#endif

/* VSP default configuration values */
#define	 VSP_ENABLE_DEFAULT	        1	         /* VSP is Enabled by default - Can be disabled/enabled using "iwpriv wifi0 set_vsp_enable" */
#define	 VSP_THRESHOLD_DEFAULT      4096000	     /* 40mbps converted to goodput (kbps*100) - default threshold value used to differentiate bad/good video stremas */
#define	 VSP_EVALINTERVAL_DEFAULT   100	         /* Default vsp eval interval -100ms. Time after which vi streams are evaluted for bad/good */
#define  VSP_MAX_AGGR_LIMIT         8000         /* Aggr limit for the vi streams which are identified as candidate streams (bad, penalized) by vsp algo */
#define  VSP_MAX_SCH_PENALITY       2	         /* Scheduling penality for vi streams which are identified as candidate streams (bad, penalized) by vsp algo */

#endif
/* Rx pkt  processed per tasklet run should be keep in sync with HAL_LP_RXFIFO_DEPTH*/
#define     RX_PKTS_PROC_PER_RUN   128
/* WPS Push Button */
enum {
    ATH_WPS_BUTTON_EXISTS       = 0x00000001,     /* Push button Exists */
    ATH_WPS_BUTTON_PUSHED       = 0x00000002,     /* Push button Pressed since last query */
    ATH_WPS_BUTTON_PUSHED_CURR  = 0x00000008,     /* Push button Pressed currently */
    ATH_WPS_BUTTON_DOWN_SUP     = 0x00000010,     /* Push button down event supported */
    ATH_WPS_BUTTON_STATE_SUP    = 0x00000040      /* Push button current state reporting supported */
};

#define ATH_DEBUG_MAX_MASK 32
enum {
    ATH_DEBUG_XMIT = 0,    /* basic xmit operation */
    ATH_DEBUG_XMIT_DESC,   /* xmit descriptors */
    ATH_DEBUG_RECV,        /* basic recv operation */
    ATH_DEBUG_RECV_DESC,   /* recv descriptors */
    ATH_DEBUG_RATE,        /* rate control */
    ATH_DEBUG_RESET,       /* reset processing */
    ATH_DEBUG_MAT,         /* MAT for s/w proxysta */
    ATH_DEBUG_BEACON,      /* beacon handling */
    ATH_DEBUG_WATCHDOG,    /* watchdog timeout */
    ATH_DEBUG_SCAN,        /* scan debug prints */
    ATH_DEBUG_GREEN_AP,    /* GreenAP debug prints */
    ATH_DEBUG_HTC_WMI,     /* htc/wmi debug prints */
    ATH_DEBUG_INTR,        /* ISR */
    ATH_DEBUG_TX_PROC,     /* tx ISR proc */
    ATH_DEBUG_RX_PROC,     /* rx ISR proc */
    ATH_DEBUG_BEACON_PROC, /* beacon ISR proc */
    ATH_DEBUG_CALIBRATE,   /* periodic calibration */
    ATH_DEBUG_KEYCACHE,    /* key cache management */
    ATH_DEBUG_STATE,       /* 802.11 state transitions */
    ATH_DEBUG_NODE,        /* node management */
    ATH_DEBUG_LED,         /* led management */
    ATH_DEBUG_TX99,        /* tx99 function */
    ATH_DEBUG_DCS,         /* dynamic channel switch */
    ATH_DEBUG_UAPSD,       /* uapsd */
    ATH_DEBUG_DOTH,        /* 11.h */
    ATH_DEBUG_CWM,         /* channel width managment */
    ATH_DEBUG_PPM,         /* Force PPM management */
    ATH_DEBUG_PWR_SAVE,    /* PS Poll and PS save */
    ATH_DEBUG_SWR,         /* SwRetry mechanism */
    ATH_DEBUG_AGGR_MEM,
    ATH_DEBUG_BTCOEX,      /* BT coexistence */
    ATH_DEBUG_FATAL,       /* fatal errors */
    ATH_DEBUG_WNM_FMS,
    /*
     * First fill in the UNUSED values above, then
     * add new values here.
     */

    ATH_DEBUG_NUM_CATEGORIES,
    ATH_DEBUG_ANY,
    ATH_DEBUG_UNMASKABLE = ATH_DEBUG_MAX_MASK,
};
#if ATH_DEBUG_NUM_CATEGORIES > ATH_DEBUG_MAX_MASK
#error                                               \
    change ATH_DEBUG_MAX_MASK to ASF_PRINT_MASK_BITS-1 \
    and update ath_set_config and ath_get_config
#endif


#define CHK_SC_DEBUG(_sc, _m)                   \
    ((_sc)->sc_print.category_mask[(_m) >> 3] & \
     (1 << ((_m) & 0x7)))
#define CHK_SC_DEBUG_SCN(_scn, _m)                      \
    CHK_SC_DEBUG(ATH_DEV_TO_SC((_scn)->sc_dev), (_m))

#define PHFLAGS_INTERRUPT_CONTEXT 0x80000000

void ath_pkt_log_text (ath_dev_t dev,  u_int32_t env_flags, const char *fmt, ...);
void ath_fixed_log_text (ath_dev_t dev, const char *text);

#if ATH_DEBUG
/*
 * Initialize and register the asf_print control object for DPRINTF
*/
#define ath_dprintf_init(_ctxt)                                        \
    do {                                                               \
        _ctxt->sc_print.name = "ATH";                                  \
        _ctxt->sc_print.num_bit_specs =                                \
            ARRAY_LENGTH(ath_print_categories);                        \
        _ctxt->sc_print.bit_specs = ath_print_categories;              \
        _ctxt->sc_print.custom_ctxt = _ctxt;                           \
        _ctxt->sc_print.custom_print = ath_print;                      \
        asf_print_mask_set(&_ctxt->sc_print, ATH_DEBUG_UNMASKABLE, 1); \
        asf_print_ctrl_register(&_ctxt->sc_print);                     \
} while (0)
/*
 * Deregister the asf_print control object for DPRINTF
 */
#define ath_dprintf_deregister(_ctxt)                   \
    asf_print_ctrl_unregister(&_ctxt->sc_print);
/*
 * Set of base macros for DPRINTF
 */
#define DPRINTF_BASE(sc, _m, _fmt, ...)         \
    dprintf((sc), (_m), (_fmt), ## __VA_ARGS__)
#define DPRINTF_TIMESTAMP_BASE(sc, _m, _fmt, ...)                       \
    do {                                                                      \
        u_int32_t now_ms, now_sec_int, now_sec_fract;                         \
        now_ms = (u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(OS_GET_TIMESTAMP());   \
            now_sec_int = now_ms / 1000;                                      \
            now_sec_fract = now_ms - now_sec_int * 1000;                      \
        dprintf((sc), (_m), "%d.%03d | " _fmt, now_sec_int, now_sec_fract, \
                ## __VA_ARGS__);                                        \
    } while (0)
#define DPRINTF_INTSAFE_BASE(sc, _m, _fmt, ...)                         \
    do {                                                                \
        (sc)->sc_print_flag = PHFLAGS_INTERRUPT_CONTEXT;                \
        dprintf((sc), (_m), (_fmt), ## __VA_ARGS__);                    \
        (sc)->sc_print_flag = 0;                                        \
} while (0)

#ifdef _DEV_ATH_ATHVAR_H
/*
 * _DEV_ATH_ATHVAR_H is defined in if_athvar.h and if it is defined then the
 * DPRINTF macro asumes that it is called with scn.
 */
#define dprintf if_ath_dprintf
#define DPRINTF(scn, _m, _fmt, ...)             \
    DPRINTF_BASE(ATH_DEV_TO_SC((scn)->sc_dev), _m, _fmt, ## __VA_ARGS__)
#define DPRINTF_TIMESTAMP(scn, _m, _fmt, ...)               \
    DPRINTF_TIMESTAMP_BASE(ATH_DEV_TO_SC((scn)->sc_dev), _m, _fmt, ## __VA_ARGS__)
#define DPRINTF_INTSAFE(scn, _m, _fmt, ...)             \
    DPRINTF_INTSAFE_BASE(ATH_DEV_TO_SC((scn)->sc_dev), _m, _fmt, ## __VA_ARGS__)
#else

#define dprintf ath_dprintf
#define DPRINTF(sc, _m, _fmt, ...)              \
    DPRINTF_BASE(sc, _m, _fmt, ## __VA_ARGS__)
#define DPRINTF_TIMESTAMP(sc, _m, _fmt, ...)                                  \
    DPRINTF_TIMESTAMP_BASE(sc, _m, _fmt, ## __VA_ARGS__)
#define DPRINTF_INTSAFE(sc, _m, _fmt, ...)              \
    DPRINTF_INTSAFE_BASE(sc, _m, _fmt, ## __VA_ARGS__)
#endif /* _DEV_ATH_ATHVAR_H */

/*
 * dprintf - a macro for function prototype of asf_print_category wrapper
 * function. The wrapper functions could be ath_dprintf or if_ath_dprintf,
 * depending on inside ath_dev or if_ath layers
 *
 * Our Linux driver builds different layers to dynamically loadable modules.
 * If we use macros to call the functions that reside in other kernel modules,
 * e.g. asf_print_category, the compiler will insert the symbol of the
 * external function in each place the macro is used.
 * With the wrapper function inside ath_dev, the size of the symbol table
 * info used for dynamic linking could be reduced significantly since the
 * compiler only needs to insert the symbol of the external function in the
 * wrapper function.
 */
void dprintf(struct ath_softc *sc, unsigned category, const char *fmt, ...);
#else
#define ath_dprintf_init(_ctxt)
#define ath_dprintf_deregister(_ctxt)
#define DPRINTF(sc, _m, _fmt, ...)
#define DPRINTF_INTSAFE(sc, _m, _fmt, ...)
#define DPRINTF_TIMESTAMP(sc, _m, _fmt, ...)
#endif /* ATH_DEBUG */

#define KEYPRINTF(scn, ix, hk, mac) do {                             \
        if (CHK_SC_DEBUG_SCN((scn), ATH_DEBUG_KEYCACHE))             \
            ath_keyprint(__func__, (ix), (hk), (mac));               \
} while (0)

/*
** Setup the "default" debug mask
*/

#ifdef DBG
#define ath_dprintf_set_default_mask(_sc)                               \
    do {                                                                \
        int i;                                                          \
        u_int8_t categories[] = {                                       \
            ATH_DEBUG_FATAL,                                            \
            ATH_DEBUG_STATE,                                            \
            ATH_DEBUG_BEACON,                                           \
            ATH_DEBUG_BEACON_PROC,                                      \
            ATH_DEBUG_KEYCACHE,                                         \
            ATH_DEBUG_CALIBRATE,                                        \
            ATH_DEBUG_BTCOEX,                                           \
        };                                                              \
        for (i = 0; i < ARRAY_LENGTH(categories); i++) {                \
            asf_print_mask_set(&(_sc)->sc_print, categories[i], 1);     \
        }                                                               \
    } while (0);
#else
#define ath_dprintf_set_default_mask(_sc)
#endif
#define ath_dprintf_set_mask(_sc,_m)                               \
    do {                                                                \
        int j;                                                          \
        u_int8_t categories[] = {                                       \
            ATH_DEBUG_XMIT,                                             \
            ATH_DEBUG_XMIT_DESC,   /* xmit descriptors */               \
            ATH_DEBUG_RECV,        /* basic recv operation */           \
            ATH_DEBUG_RECV_DESC,   /* recv descriptors */               \
            ATH_DEBUG_RATE,        /* rate control */                   \
            ATH_DEBUG_RESET,       /* reset processing */               \
            ATH_DEBUG_MAT,                                          \
            ATH_DEBUG_BEACON,      /* beacon handling */                \
            ATH_DEBUG_WATCHDOG,    /* watchdog timeout */               \
            ATH_DEBUG_SCAN,        /* scan debug prints */              \
            ATH_DEBUG_GREEN_AP,    /* GreenAP debug prints */           \
            ATH_DEBUG_HTC_WMI,     /* htc/wmi debug prints */           \
            ATH_DEBUG_INTR,        /* ISR */                            \
            ATH_DEBUG_TX_PROC,     /* tx ISR proc */                    \
            ATH_DEBUG_RX_PROC,     /* rx ISR proc */                    \
            ATH_DEBUG_BEACON_PROC, /* beacon ISR proc */                \
            ATH_DEBUG_CALIBRATE,   /* periodic calibration */           \
            ATH_DEBUG_KEYCACHE,    /* key cache management */           \
            ATH_DEBUG_STATE,       /* 802.11 state transitions */       \
            ATH_DEBUG_NODE,        /* node management */                \
            ATH_DEBUG_LED,         /* led management */                 \
            ATH_DEBUG_TX99,        /* tx99 function */                  \
            ATH_DEBUG_DCS,                                          \
            ATH_DEBUG_UAPSD,       /* uapsd */                          \
            ATH_DEBUG_DOTH,        /* 11.h */                           \
            ATH_DEBUG_CWM,         /* channel width managment */        \
            ATH_DEBUG_PPM,         /* Force PPM management */           \
            ATH_DEBUG_PWR_SAVE,    /* PS Poll and PS save */            \
            ATH_DEBUG_SWR,         /* SwRetry mechanism */              \
            ATH_DEBUG_AGGR_MEM,                                         \
            ATH_DEBUG_BTCOEX,      /* BT coexistence */                 \
            ATH_DEBUG_FATAL,       /* fatal errors */                   \
            ATH_DEBUG_WNM_FMS, \
        };                                                              \
        for (j = 0; j < ARRAY_LENGTH(categories); j++) {                \
            if(_m & (1<<categories[j]))                                 \
            asf_print_mask_set(&(_sc)->sc_print, categories[j], 1);     \
        }                                                               \
    } while (0);

#define ATH_ADDR_LEN            6
#ifndef __ubicom32__
#define ATH_ADDR_EQ(a1,a2)      (OS_MEMCMP(a1,a2,ATH_ADDR_LEN) == 0)
#define ATH_ADDR_COPY(dst,src)  OS_MEMCPY(dst,src,ATH_ADDR_LEN)
#else
#define ATH_ADDR_EQ(a1,a2)      (OS_MACCMP(a1,a2) == 0)
#define ATH_ADDR_COPY(dst,src)  OS_MACCPY(dst,src)
#endif

#define ATH_INTR_DISABLE(_sc)       (ath_hal_intrset(_sc->sc_ah, 0))
#define ATH_INTR_ENABLE(_sc)        (ath_hal_intrset(_sc->sc_ah, _sc->sc_imask))
#define ATH_DRAINTXQ(_sc, _retry, _timeout)   (ath_reset_draintxq(_sc, _retry, _timeout))
#if ATH_SUPPORT_WIFIPOS
#define ATH_CLEANHWQ(_sc)   (ath_tx_abortalldma(_sc))
#endif
#define ATH_STARTRECV(_sc)          (ath_startrecv(_sc))
#define ATH_STOPRECV(_sc, _timeout) (ath_stoprecv(_sc, _timeout))
#define ATH_STOPRECV_EX(_sc, _timeout)(ath_stoprecv(_sc, _timeout))
#define ATH_RX_TASKLET(_sc, _flush) do {      \
        if (sc->sc_enhanceddmasupport)        \
            ath_rx_edma_tasklet(_sc, _flush); \
        else                                  \
            ath_rx_tasklet(_sc, _flush);      \
    } while (0)

#ifdef ATH_USB
#define ATH_GET_CACHELINE_SIZE(_sc)
#define ATH_SET_TX(_sc)                 (ath_usb_set_tx(_sc))
#else
#define ATH_GET_CACHELINE_SIZE(_sc)                             \
    {                                                           \
        int bustype = ((HAL_BUS_CONTEXT *)bus_context)->bc_bustype; \
        int csz = 0;                                           \
        bus_read_cachesize(_sc->sc_osdev, &csz, bustype);      \
        /* XXX assert csz is non-zero */                        \
        _sc->sc_cachelsz = csz << 2;    /* convert to bytes */  \
    }
#define ATH_SET_TX(_sc)                 (ath_set_tx(_sc))
#endif

/*
 * ************************
 * Internal interfaces
 * ************************
 */
#define __11nstats(sc, _x)          sc->sc_stats.ast_11n_stats._x ++
#define __11nstatsn(sc, _x, _n)     sc->sc_stats.ast_11n_stats._x += _n

/*
 * Control interfaces
 */
void ath_setdefantenna(void *sc, u_int antenna);
void ath_setslottime(struct ath_softc *sc);
void ath_update_txpow(struct ath_softc *sc, u_int16_t tpcInDb);
void ath_update_tpscale(struct ath_softc *sc);
void ath_control_pwscale(struct ath_softc *sc);
int ath_setTxPwrAdjust(ath_dev_t dev, int32_t adjust, u_int32_t is2GHz);
int ath_setTxPwrLimit(ath_dev_t dev, u_int32_t limit, u_int16_t tpcInDb, u_int32_t is2GHz);

#if ATH_SUPPORT_WIFIPOS
/* For jumpstarting the transmission */
void ath_resched_txq(ath_dev_t);
#endif

int ath_reset_start(ath_dev_t, HAL_BOOL, int tx_timeout, int rx_timeout);
int ath_reset_end(ath_dev_t, HAL_BOOL);
int ath_reset(ath_dev_t);
#if ATH_RESET_SERIAL
int ath_reset_wait_tx_rx_finished(struct ath_softc *);
int ath_reset_wait_intr_dpc_finished(struct ath_softc *);
#endif
int ath_cabq_update(struct ath_softc *);
int ath_stop_locked(struct ath_softc *sc);
void ath_chan_change(struct ath_softc *sc, HAL_CHANNEL *chan, bool ignore_dfs);
void ath_set_channel(ath_dev_t dev, HAL_CHANNEL *hchan, int tx_timeout, int rx_timeout, bool no_flush, bool ignore_dfs);
#if ATH_SUPPORT_WIFIPOS
void ath_lean_set_channel(ath_dev_t dev, HAL_CHANNEL *hchan, int tx_timeout, int rx_timeout, bool retry);
int  ath_get_channel_busy_info(ath_dev_t dev, u_int32_t *rxclear_pct, u_int32_t *rxframe_pct, u_int32_t *txframe_pct);
bool ath_disable_hwq(ath_dev_t dev, u_int16_t mask);
#endif
void ath_set_rx_chainmask(ath_dev_t dev, u_int8_t mask);
#if ATH_TX_DUTY_CYCLE
int ath_set_tx_duty_cycle(ath_dev_t dev, int active_pct, bool enabled);
#endif
bool ath_rate_check(ath_dev_t dev,int val);

/*
 * Support interfaces
 */
void ath_hw_phystate_change(struct ath_softc *sc, int newstate);
u_int64_t ath_extend_tsf(struct ath_softc *sc, u_int32_t rstamp);
void ath_pcie_pwrsave_enable_on_phystate_change(struct ath_softc *sc, int enable);
void ath_pcie_pwrsave_enable(struct ath_softc *sc, int enable);
void ath_internal_reset(struct ath_softc *sc);
#if ATH_LOW_PRIORITY_CALIBRATE
typedef int (*athCalCallback)(void *);
int ath_cal_workitem(struct ath_softc *sc, athCalCallback func, void *arg);
#endif

#ifdef ATH_BT_COEX
void ath_pcie_pwrsave_btcoex_enable(struct ath_softc *sc, int enable);
#endif


/*
 * Beacon interfaces
 */
void ath_beacon_attach(struct ath_softc *sc);
void ath_beacon_detach(struct ath_softc *sc);
void ath_beacon_sync(ath_dev_t dev, int if_id);
void ath_beacon_config(struct ath_softc *sc,ath_beacon_config_reason reason, int if_id);
int ath_beaconq_setup(struct ath_softc *sc, struct ath_hal *ah);
int ath_beacon_alloc(struct ath_softc *sc, int if_id);
void ath_bstuck_tasklet(struct ath_softc *sc);
void ath_beacon_tasklet(struct ath_softc *sc, int *needmark);
void ath_beacon_free(struct ath_softc *sc);
void ath_beacon_return(struct ath_softc *sc, struct ath_vap *avp);
void ath_bmiss_tasklet(struct ath_softc *sc);
void ath_txto_tasklet(struct ath_softc *sc);
int ath_hw_hang_check(struct ath_softc *sc);
int ath_get_hwbeaconproc_active(ath_dev_t dev);
void ath_hwbeaconproc_enable(struct ath_softc *sc);
void ath_hwbeaconproc_disable(struct ath_softc *sc);
void ath_hw_beacon_rssi_threshold_enable(ath_dev_t dev, u_int32_t rssi_threshold);
void ath_hw_beacon_rssi_threshold_disable(ath_dev_t dev);
u_int32_t ath_hw_beacon_rssi(struct ath_softc *sc);
void ath_hw_beacon_rssi_reset(struct ath_softc *sc);
void ath_brssi_tasklet(struct ath_softc *sc);
#if UMAC_SUPPORT_WNM
int ath_timbcast_alloc(struct ath_softc *sc, int if_id);
void ath_timbcast_return(struct ath_softc *sc, struct ath_vap *avp);
#endif

/*
 * Descriptor/MPDU interfaces
 */
int
ath_descdma_setup(
    struct ath_softc *sc,
    struct ath_descdma *dd, ath_bufhead *head,
    const char *name, int nbuf, int ndesc, int is_tx, int frag_per_msdu);

int
ath_txstatus_setup(
    struct ath_softc *sc,
    struct ath_descdma *dd,
    const char *name, int ndesc);

void
ath_descdma_cleanup(
    struct ath_softc *sc,
    struct ath_descdma *dd,
    ath_bufhead *head);

void
ath_txstatus_cleanup(
    struct ath_softc *sc,
    struct ath_descdma *dd);

void ath_desc_swap(struct ath_desc *ds);

wbuf_t ath_rxbuf_alloc(struct ath_softc *sc, u_int32_t len);
void ath_allocRxbufsForFreeList(struct ath_softc *sc);

/*
 * Transmit interfaces
 */

int ath_tx_init(ath_dev_t, int nbufs);
int ath_tx_cleanup(ath_dev_t);
struct ath_txq *ath_txq_setup(struct ath_softc *sc, int qtype, int subtype);
void ath_tx_cleanupq(struct ath_softc *sc, struct ath_txq *txq);
int ath_set_tx(struct ath_softc *sc);
int ath_tx_setup(struct ath_softc *sc, int haltype);
int ath_tx_get_qnum(ath_dev_t, int qtype, int haltype);
int ath_txq_update(ath_dev_t, int qnum, HAL_TXQ_INFO *qi);
void ath_txq_burst_update(ath_dev_t, int qnum, u_int32_t duration);
void ath_tx_tasklet(ath_dev_t);
void ath_tx_flush(ath_dev_t);
void ath_reset_draintxq(struct ath_softc *sc, HAL_BOOL retry_tx, int timeout);
#if ATH_SUPPORT_WIFIPOS
void ath_tx_abortalldma(struct ath_softc *sc);
#endif
void ath_draintxq(struct ath_softc *sc, HAL_BOOL retry_tx, int timeout);
void ath_tx_draintxq(struct ath_softc *sc, struct ath_txq *txq, HAL_BOOL retry_tx);
void ath_tx_mcast_draintxq(struct ath_softc *sc, struct ath_txq *txq);
int ath_tx_processq(struct ath_softc *sc, struct ath_txq *txq);

#ifdef AR_DEBUG
void ath_tx_node_queue_stats(struct ath_softc *sc , ath_node_t node);
void ath_dump_descriptors(ath_dev_t dev);
void ath_dump_rx_edma_desc(ath_dev_t dev);
void ath_dump_rx_desc(ath_dev_t dev);
#endif

int ath_tx_node_queue_depth(struct ath_softc *sc , ath_node_t node);

#ifdef ATH_CHAINMASK_SELECT
int ath_chainmask_sel_logic(struct ath_softc *sc, struct ath_node *an);
#endif
void ath_handle_tx_intr(struct ath_softc *sc);

int ath_tx_start(ath_dev_t, wbuf_t wbuf, ieee80211_tx_control_t *txctl);

u_int32_t ath_txq_depth(ath_dev_t, int);
#if ATH_TX_BUF_FLOW_CNTL
void ath_txq_set_minfree(ath_dev_t, int qtype, int haltype, u_int minfree);
u_int32_t ath_txq_num_buf_used(ath_dev_t dev, int qnum);
#endif
u_int32_t ath_txq_aggr_depth(ath_dev_t, int);
#if ATH_TX_BUF_FLOW_CNTL
void ath_txq_set_minfree(ath_dev_t, int qtype, int haltype, u_int minfree);
#endif
int ath_tx_start_dma(wbuf_t, sg_t *sg, u_int32_t n_sg, void *arg);
void ath_set_protmode(ath_dev_t, PROT_MODE mode);

int ath_tx_txqaddbuf(struct ath_softc *sc, struct ath_txq *txq, ath_bufhead *head);
int ath_tx_mcastqaddbuf(struct ath_softc *sc, struct ath_vap *avp , ath_bufhead *head);

#ifdef  ATH_SUPPORT_TxBF
void _ath_tx_complete_buf(struct ath_softc *sc, struct ath_buf *bf, ath_bufhead *bf_q,
                         int txok, u_int8_t txbf_status, u_int32_t tstamp);
#else
void _ath_tx_complete_buf(struct ath_softc *sc, struct ath_buf *bf, ath_bufhead *bf_q,
                         int txok);
#endif
#if ATH_TX_COMPACT && UMAC_SUPPORT_APONLY

#ifdef  ATH_SUPPORT_TxBF

void ath_tx_complete_buf(struct ath_softc *sc, struct ath_buf *bf, ath_bufhead *bf_q,
                         int txok, u_int8_t txbf_status, u_int32_t tstamp);
#else
void ath_tx_complete_buf(struct ath_softc *sc, struct ath_buf *bf, ath_bufhead *bf_q,
                         int txok);
#endif
#else

#ifdef  ATH_SUPPORT_TxBF

#define ath_tx_complete_buf(_sc, _bf, _bf_q, _txok, _txbf_status, _tstamp) \
    _ath_tx_complete_buf(_sc, _bf, _bf_q, _txok, _txbf_status, _tstamp)

#else

#define ath_tx_complete_buf(_sc, _bf, _bf_q, _txok) \
      _ath_tx_complete_buf(_sc, _bf, _bf_q, _txok)

#endif // ATH_SUPPORT_TxBF
#endif // ATH_TX_COMPACT

uint8_t ath_txchainmask_reduction(struct ath_softc *sc, uint8_t chainmask, uint8_t rate);
void ath_buf_set_rate(struct ath_softc *sc, struct ath_buf *bf);
int  ath_tx_update_hwretries_estimate(struct ath_softc *sc,
                                      struct ath_buf *bf,
                                      struct ath_tx_status *ts);
void ath_tx_update_stats(struct ath_softc *sc, struct ath_buf *bf, u_int qnum, struct ath_tx_status *ts);
int ath_tx_num_badfrms(struct ath_softc *sc, struct ath_buf *bf, struct ath_tx_status *ts, int txok);
void ath_txq_schedule_before_eapol(struct ath_softc *sc, struct ath_atx_tid *tid);
void ath_txq_schedule(struct ath_softc *sc, struct ath_txq *txq);
void ath_tx_complete_aggr_rifs(struct ath_softc *sc, struct ath_txq *txq, struct ath_buf *bf,
                               ath_bufhead *bf_q, struct ath_tx_status *ts, int txok);
void ath_tx_update_minimal_stats(struct ath_softc *sc, struct ath_buf *bf, struct ath_tx_status *ts);
#if  ATH_TX_COMPACT  &&  UMAC_SUPPORT_APONLY
int
ath_tx_complete_aggr_compact(struct ath_softc *sc, struct ath_txq *txq, struct ath_buf *bf,
        ath_bufhead *bf_head, struct ath_tx_status *ts, int txok,ath_bufhead *bf_headfree);
void
ath_tx_mark_aggr_rifs_done(struct ath_softc *sc, struct ath_txq *txq, struct ath_buf *bf,
                ath_bufhead *bf_head, struct ath_tx_status *ts, int txok);

void
ath_edma_free_complete_buf( struct ath_softc *sc , struct ath_txq *txq, ath_bufhead * bf_headfree,int txok );

#else
#ifndef ath_tx_mark_aggr_rifs_done
#define ath_tx_mark_aggr_rifs_done(_sc, _txq, _bf, _bfhead, _ts, _txok ) \
               ath_tx_complete_aggr_rifs(_sc, _txq, _bf, _bfhead, _ts, _txok )
#endif
#endif // ATH_TX_COMPACT







void ath_printtxbuf(struct ath_buf *bf, u_int8_t desc_status) ;

/* RTS retry limit is per DCU. Instead of assuming constant
 * value on all queues, query the queue configuration to
 * get the actual limit on that queue.
 */
int ath_tx_get_rts_retrylimit(struct ath_softc *sc, struct ath_txq *txq);

void ath_netif_stop_queue(ath_dev_t dev);
void ath_netif_wake_queue(ath_dev_t dev);
void ath_netif_set_watchdog_timeout(ath_dev_t dev, int val);
void ath_netif_clr_flag(ath_dev_t dev, unsigned int flag);
void ath_netif_update_trans(ath_dev_t dev);


#ifdef ATH_SWRETRY
/*
 * Transmit interfaces for software retry
 */
void ath_set_swretrystate(ath_dev_t dev, ath_node_t node, int flag);
int ath_check_swretry_req(struct ath_softc *sc, struct ath_buf *bf);
struct ath_buf *ath_form_swretry_frm(struct ath_softc *sc, struct ath_txq *txq,
                                     ath_bufhead *bf_q, struct ath_buf *bf);
void ath_seqno_recover(struct ath_softc *sc, struct ath_buf *bf);
void ath_tx_drain_sxmitq(struct ath_softc *sc, struct ath_node *an);
HAL_STATUS ath_tx_mpdu_resend(struct ath_softc *sc, struct ath_txq *txq,
                              ath_bufhead *bf_head, struct ath_tx_status tx_status);
HAL_STATUS ath_tx_mpdu_requeue(struct ath_softc *sc, struct ath_txq *txq,
                              ath_bufhead *bf_head, struct ath_tx_status tx_status);
void ath_tx_dec_eligible_frms(struct ath_softc *sc, struct ath_buf *bf,
                                    u_int qnum, struct ath_tx_status *ts);
void ath_tx_flush_node_sxmitq(struct ath_softc *sc, struct ath_node *an);
void dumpTxQueue(struct ath_softc *sc, ath_bufhead *bfhead);
#if ATH_SWRETRY_MODIFY_DSTMASK
void ath_tx_modify_cleardestmask(struct ath_softc *sc, struct ath_txq *txq, ath_bufhead *bf_head);
#endif
int ath_handlepspoll(ath_dev_t dev, ath_node_t node);
int ath_exist_pendingfrm_tidq(ath_dev_t dev, ath_node_t node);
int ath_reset_paused_tid(ath_dev_t dev, ath_node_t node);
#endif

/* aggr interfaces */
void ath_rx_node_free(struct ath_softc *sc, struct ath_node *an);

#if ATH_TX_DROP_POLICY
int ath_tx_drop_policy(wbuf_t wbuf, void *arg);
#endif
/*
 * Receive interfaces
 */

int ath_rx_init(ath_dev_t, int nbufs);
void ath_rx_cleanup(ath_dev_t);
int ath_startrecv(struct ath_softc *sc);
HAL_BOOL ath_stoprecv(struct ath_softc *sc, int timeout);
void ath_flushrecv(struct ath_softc *sc);
void ath_rx_requeue(ath_dev_t, wbuf_t wbuf);
u_int32_t ath_calcrxfilter(struct ath_softc *sc);
void ath_handle_rx_intr(struct ath_softc *sc);
int ath_rx_indicate(struct ath_softc *sc, wbuf_t wbuf, ieee80211_rx_status_t *status, u_int16_t keyix);
int ath_rx_input(ath_dev_t, ath_node_t node, int is_ampdu,
                 wbuf_t wbuf, ieee80211_rx_status_t *rx_status,
                 ATH_RX_TYPE *status);
void ath_printrxbuf(struct ath_buf *bf, u_int8_t desc_status) ;
#if ATH_SUPPORT_WIRESHARK
#include "ah_radiotap.h" /* ah_rx_radiotap_header */
void ath_fill_radiotap_hdr(ath_dev_t dev, struct ah_rx_radiotap_header *rh, struct ah_ppi_data *ppi,
						   wbuf_t wbuf);
#endif
void ath_rx_process(struct ath_softc *sc, struct ath_buf *bf, struct ath_rx_status *rxs,
                    u_int8_t frame_fc0, ieee80211_rx_status_t *rx_status, u_int8_t *chainreset);

#if ATH_SUPPORT_VOW_DCS
u_int32_t ath_rx_duration(struct ath_softc *sc, u_int8_t rix, int pktlen, int more, int error, int width, int half_gi, HAL_BOOL shortPreamble);
#endif

u_int8_t ath_rate_findrix(const HAL_RATE_TABLE *rt , u_int32_t rateKbps);

#if AP_MULTIPLE_BUFFER_RCV
HAL_BOOL ath_rx_buf_relink(ath_dev_t dev,struct ath_buf *bf);
wbuf_t ath_rx_buf_merge(struct ath_softc *sc, u_int32_t len,wbuf_t sbuf,wbuf_t wbuf);
HAL_BOOL ath_rx_edma_buf_relink(ath_dev_t dev,struct ath_buf *bf);
wbuf_t ath_rx_edma_buf_merge(struct ath_softc *sc,wbuf_t sbuf, wbuf_t wbuf,struct ath_rx_status *rxs);
#endif


/* Parameters for ath_rx_tasklet */
#define RX_PROCESS          0   /* Process rx frames in rx interrupt. */
#define RX_DROP             1   /* Drop rx frames in flush routine.*/
#define RX_FORCE_PROCESS    2   /* Flush and indicate rx frames */
int ath_rx_tasklet(ath_dev_t, int flush);

void ath_opmode_init(ath_dev_t);

int ath_wait_sc_inuse_cnt(struct ath_softc *sc, u_int32_t timeout);

/* PHY state interfaces */
int ath_get_sw_phystate(ath_dev_t);
int ath_get_hw_phystate(ath_dev_t);
void ath_set_sw_phystate(ath_dev_t, int swstate);
int ath_radio_disable(ath_dev_t);
int ath_radio_enable(ath_dev_t);

/* PnP interfaces */
void ath_notify_device_removal(ath_dev_t);
int ath_detect_card_present(ath_dev_t);

/* fast frame interfaces */
int ath_ff_check(ath_dev_t dev, ath_node_t node, int qnum,
                 int check_qdepth, u_int32_t txoplimit, u_int32_t frameLen);


#if ATH_WOW
/* WakeonWireless interfaces */
int ath_get_wow_support(ath_dev_t dev);
int ath_set_wow_enable(ath_dev_t dev, int clearbssid);
int ath_wow_wakeup(ath_dev_t dev);
void ath_set_wow_events(ath_dev_t dev, u_int32_t wowEvents);
int ath_get_wow_events(ath_dev_t dev);
int ath_wow_add_wakeup_pattern(ath_dev_t dev, u_int32_t patternId, u_int8_t *patternBytes, u_int8_t *maskBytes, u_int32_t patternLen);
int ath_wow_remove_wakeup_pattern(ath_dev_t dev, u_int8_t *patternBytes, u_int8_t *maskBytes);
int ath_wow_get_wakeup_pattern(ath_dev_t dev, u_int8_t *wakePatternBytes,u_int32_t *wakePatternSize, u_int32_t *patternId);
int ath_get_wow_wakeup_reason(ath_dev_t dev);
int ath_wow_matchpattern_exact(ath_dev_t dev);
void ath_wow_set_duration(ath_dev_t dev, u_int16_t duration);
void ath_set_wow_timeout(ath_dev_t dev, u_int32_t timeout);
u_int32_t ath_get_wow_timeout(ath_dev_t dev);
#if ATH_WOW_OFFLOAD
void ath_set_wow_enabled_events(ath_dev_t dev,
                                u_int32_t EnabledWoLPacketPatterns,
                                u_int32_t EnabledProtocolOffloads,
                                u_int32_t MediaSpecificWakeUpEvents);
void ath_get_wow_enabled_events(ath_dev_t dev,
                                u_int32_t *pEnabledWoLPacketPatterns,
                                u_int32_t *pEnabledProtocolOffloads,
                                u_int32_t *pMediaSpecificWakeUpEvents);
int ath_get_wowoffload_support(ath_dev_t dev);
int ath_get_wowoffload_gtk_support(ath_dev_t dev);
int ath_get_wowoffload_arp_support(ath_dev_t dev);
int ath_get_wowoffload_max_arp_offloads(ath_dev_t dev);
int ath_get_wowoffload_ns_support(ath_dev_t dev);
int ath_get_wowoffload_max_ns_offloads(ath_dev_t dev);
int ath_get_wowoffload_4way_hs_support(ath_dev_t dev);
int ath_get_wowoffload_acer_magic_support(ath_dev_t dev);
int ath_get_wowoffload_acer_swka_support(ath_dev_t dev, u_int32_t id);
bool ath_wowoffload_acer_magic_enable(ath_dev_t dev);
void ath_wowoffload_remove_gtk_rekey_info(ath_dev_t dev);
u_int32_t ath_wowoffload_remove_offload_info(ath_dev_t dev, u_int32_t id);
u_int32_t ath_wowoffload_set_rekey_suppl_info(ath_dev_t dev, u_int32_t id, u_int8_t *kck, u_int8_t *kek,
                        u_int64_t *replay_counter);
u_int32_t
ath_wowoffload_set_rekey_misc_info(ath_dev_t dev, struct wow_offload_misc_info *wow);
u_int32_t ath_wowoffload_get_rekey_info(ath_dev_t dev, void *buf, u_int32_t param);
u_int32_t
ath_wowoffload_update_txseqnum(ath_dev_t dev, struct ath_node *an, u_int32_t tidno, u_int16_t seqnum);
int ath_acer_keepalive(ath_dev_t dev, u_int32_t id, u_int32_t msec, u_int32_t size, u_int8_t* packet);
int ath_acer_keepalive_query(ath_dev_t dev, u_int32_t id);
int ath_acer_wakeup_match(ath_dev_t dev, u_int8_t* pattern);
int ath_acer_wakeup_disable(ath_dev_t dev);
int ath_acer_wakeup_query(ath_dev_t dev);
u_int32_t ath_wowoffload_set_arp_info(ath_dev_t dev, u_int32_t protocolOffloadId, void *pParams);
u_int32_t ath_wowoffload_set_ns_info(ath_dev_t dev, u_int32_t protocolOffloadId, void *pParams);
#endif /* ATH_WOW_OFFLOAD */
#endif

#ifdef  ATH_SUPPORT_TxBF
typedef enum {
    TX_BF_DO_NOTHING,
    TX_BF_DO_RX_NEXT,
    TX_BF_DO_CONTINUE
} BF_STATUS;
void ath_rx_bf_process(struct ath_softc *sc, struct ath_buf *bf, struct ath_rx_status *rxs, ieee80211_rx_status_t *rx_status);
BF_STATUS
ath_rx_bf_handler(ath_dev_t dev,wbuf_t wbuf, struct ath_rx_status *rxs, struct ath_buf *bf);
#endif /* ATH_SUPPORT_TxBF */

#ifdef ATH_SUPPORT_UAPSD
/* UAPSD Interface */
int ath_process_uapsd_trigger(ath_dev_t dev, ath_node_t node, u_int8_t maxsp,
                              u_int8_t ac, u_int8_t flush, bool *sent_eosp, u_int8_t maxqdepth);
int ath_tx_uapsd_init(struct ath_softc *sc);
void ath_tx_uapsd_cleanup(struct ath_softc *sc);
void ath_tx_uapsd_node_cleanup(struct ath_softc *sc, struct ath_node *an);
u_int32_t ath_tx_uapsd_depth(ath_node_t node);
void ath_tx_queue_uapsd(struct ath_softc *sc, struct ath_txq *txq, ath_bufhead *bf_head, ieee80211_tx_control_t *txctl);
#ifdef  ATH_SUPPORT_TxBF
void ath_tx_uapsd_complete(struct ath_softc *sc, struct ath_node *an,
        struct ath_buf *bf, ath_bufhead *bf_q, int txok, u_int8_t txbf_status, u_int32_t tstamp);
#else   //ATH_SUPPORT_TxBF
void ath_tx_uapsd_complete(struct ath_softc *sc, struct ath_node *an, struct ath_buf *bf, ath_bufhead *bf_q, int txok);
#endif  //ATH_SUPPORT_TxBF
void ath_tx_uapsdqnulbf_complete(struct ath_softc *sc, struct ath_buf *bf, bool held_uapsd_lock);
void ath_tx_uapsd_draintxq(struct ath_softc *sc);
#ifdef ATH_VAP_PAUSE_SUPPORT
void ath_tx_stage_queue_uapsd(struct ath_softc *sc, struct ath_node *an, ath_bufhead *bf_head);
void ath_tx_prepend_uapsd_stage_queue(struct ath_softc *sc, struct ath_node *an);
void ath_tx_uapsd_pause_begin(struct ath_softc *sc);
void ath_tx_uapsd_pause_end(struct ath_softc *sc);

#else   //ATH_VAP_PAUSE_SUPPORT

#define ath_tx_stage_queue_uapsd(sc, an, bf_head) /**/
#define ath_tx_prepend_uapsd_stage_queue(sc, an)  /**/
#define ath_tx_uapsd_pause_begin(sc)              /**/
#define ath_tx_uapsd_pause_end(sc)                   /**/

#endif  //ATH_VAP_PAUSE_SUPPORT

#define ATH_TX_UAPSD_NODE_CLEANUP(_sc, _an)(ath_tx_uapsd_node_cleanup(_sc, _an))

#else   //ATH_SUPPORT_UAPSD

#define ath_tx_uapsd_pause_begin(sc)              /**/
#define ath_tx_uapsd_pause_end(sc)                /**/

#endif  //ATH_SUPPORT_UAPSD

#if ATH_SUPPORT_DESCFAST
void ath_rx_proc_descfast(ath_dev_t dev);
#endif

#if ATH_SUPPORT_CFEND
int  ath_tx_cfend_init(struct ath_softc *sc);
void ath_tx_cfend_cleanup(struct ath_softc *sc);
void ath_sendcfend(struct ath_softc *sc, u_int8_t *bssid);
void ath_tx_cfend_complete(struct ath_softc *sc, struct ath_buf *bf, ath_bufhead *bf_q);
#endif
#if ATH_SUPPORT_PAPRD
int ath_tx_paprd_init(struct ath_softc *sc);
int ath_paprd_cal(struct ath_softc *sc);
int ath_apply_paprd_table(struct ath_softc *sc);
void ath_tx_paprd_cleanup(struct ath_softc *sc);
void ath_tx_paprd_complete(struct ath_softc *sc, struct ath_buf *bf,
        ath_bufhead *bf_q);
#else
#define ath_tx_paprd_init(_sc)     (0)
#define ath_paprd_cal(_sc)         (-1)
#define ath_apply_paprd_table(_sc) (-1)
#define ath_tx_paprd_cleanup(_sc)               /* */
#define ath_tx_paprd_complete(_sc, _bf, _bf_q)  /* */

#endif /* ATH_SUPPORT_PAPRD */

u_int32_t ath_pkt_duration(struct ath_softc *sc, u_int8_t rix, struct ath_buf *bf,
                                  int width, int half_gi, HAL_BOOL shortPreamble);

/*
** Configuration Prototypes
*/

int ath_get_config(ath_dev_t dev, ath_param_ID_t ID, void *buff);
int ath_set_config(ath_dev_t dev, ath_param_ID_t ID, void *buff);

#if ATH_PCIE_ERROR_MONITOR
int
ath_start_pcie_error_monitor(ath_dev_t dev,int b_auto_stop);
int
ath_read_pcie_error_monitor(ath_dev_t dev, void* pReadCounters);
int
ath_stop_pcie_error_monitor(ath_dev_t dev);

#endif //ATH_PCIE_ERROR_MONITOR

#if WLAN_SPECTRAL_ENABLE
int ath_dev_get_ctldutycycle(ath_dev_t dev);
int ath_dev_get_extdutycycle(ath_dev_t dev);
void ath_dev_start_spectral_scan(ath_dev_t dev, u_int8_t priority);
void ath_dev_stop_spectral_scan(ath_dev_t dev);
void ath_dev_record_chan_info(ath_dev_t dev,
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
u_int32_t ath_init_spectral_ops(struct ath_spectral_ops* p_sops);
#endif

u_int32_t   ath_gettsf32(ath_dev_t dev);
u_int64_t   ath_gettsf64(ath_dev_t dev);
#if ATH_SUPPORT_WIFIPOS
u_int64_t   ath_gettsftstamp(ath_dev_t dev);
#endif
void        ath_setrxfilter(ath_dev_t dev, u_int32_t filter);

int         ath_get_mib_cyclecounts(ath_dev_t dev, HAL_COUNTERS *pCnts);
int         ath_update_mib_macstats(ath_dev_t dev);
int         ath_get_mib_macstats(ath_dev_t dev, struct ath_mib_mac_stats *pStats);
#ifdef ATH_CCX
u_int8_t    ath_rcRateValueToPer(ath_dev_t, struct ath_node *, int);
void        ath_clear_mib_counters(ath_dev_t dev);
u_int64_t   ath_gettsf64(ath_dev_t dev);
int         ath_getserialnumber(ath_dev_t dev, u_int8_t *pSerNum, int limit);
int         ath_getchandata(ath_dev_t dev, struct ieee80211_ath_channel *pChan, struct ath_chan_data *pData);
u_int32_t   ath_getcurrssi(ath_dev_t dev);
#endif
#if ATH_GEN_RANDOMNESS
u_int32_t   ath_get_rssi_chain0(ath_dev_t dev);
#endif

u_int8_t ath_get_common_power(u_int16_t freq);
u_int8_t ath_rate_findrc(ath_dev_t dev, u_int32_t rate);

#if WLAN_SPECTRAL_ENABLE
void        ath_spectral_indicate(struct ath_softc *sc, void* spectral_buf,
                                  u_int32_t buf_size);
#endif

#if ATH_SUPPORT_RAW_ADC_CAPTURE
int spectral_enter_raw_capture_mode(ath_dev_t dev, void* indata);
int spectral_exit_raw_capture_mode(ath_dev_t dev);
int spectral_retrieve_raw_capture(ath_dev_t dev, void* outdata, u_int32_t *outsize);
#endif

#if ATH_DEBUG
void ath_print_tbf_usage(struct ath_softc *sc);
#endif
#if UNIFIED_SMARTANTENNA
int ath_smart_ant_select_train_packet(struct ath_atx_tid *tid,struct ath_buf *bf);

#define SMART_ANT_ENABLED(_sc) \
    ((_sc->sc_smart_ant_enable) & SMART_ANT_ENABLE_MASK)
#define SMART_ANT_TX_FEEDBACK_ENABLED(_sc) \
    ((_sc->sc_smart_ant_enable) & SMART_ANT_TX_FEEDBACK_MASK)
#define SMART_ANT_RX_FEEDBACK_ENABLED(_sc) \
    ((_sc->sc_smart_ant_enable) & SMART_ANT_RX_FEEDBACK_MASK)
#endif

void        ath_process_phyerr(struct ath_softc *sc, struct ath_buf *bf,
                               struct ath_rx_status *rxs, u_int64_t fulltsf);

/*
 * HAL definitions to comply with local coding convention.
 */
#define ath_hal_reset(_ah, _opmode, _chan, _macmode, _txchainmask, _rxchainmask, _extprotspacing, _outdoor, _pstatus, _is_scan) \
    ((*(_ah)->ah_reset)((_ah), (_opmode), (_chan), (_macmode), (_txchainmask), (_rxchainmask), (_extprotspacing), (_outdoor), (_pstatus), (_is_scan)))
#if ATH_SUPPORT_WIFIPOS
#define ath_hal_lean_channel_change(_ah, _opmode, _chan, _macmode, _txchainmask, _rxchainmask) \
    ((*(_ah)->ah_lean_channel_change)((_ah), (_opmode), (_chan), (_macmode), (_txchainmask), (_rxchainmask)))
#define ath_hal_disable_hwq(_ah, _mask) \
    ((*(_ah)->ah_disable_hwq)((_ah), (_mask)))
#endif
#define ath_hal_getratetable(_ah, _mode) \
    ((*(_ah)->ah_get_rate_table)((_ah), (_mode)))
#define ath_hal_getmac(_ah, _mac) \
    ((*(_ah)->ah_get_mac_address)((_ah), (_mac)))
#define ath_hal_setmac(_ah, _mac) \
    ((*(_ah)->ah_set_mac_address)((_ah), (_mac)))
#define ath_hal_getbssidmask(_ah, _mask) \
    ((*(_ah)->ah_get_bss_id_mask)((_ah), (_mask)))
#define ath_hal_setbssidmask(_ah, _mask) \
    ((*(_ah)->ah_set_bss_id_mask)((_ah), (_mask)))
#define ath_hal_intrset_nortc(_ah, _mask) \
    ((*(_ah)->ah_set_interrupts)((_ah), (_mask), 1))
#define ath_hal_intrset(_ah, _mask) \
    ((*(_ah)->ah_set_interrupts)((_ah), (_mask), 0))
#define ath_hal_intrget(_ah) \
    ((*(_ah)->ah_get_interrupts)((_ah)))
#define ath_hal_intrpend(_ah) \
    ((*(_ah)->ah_is_interrupt_pending)((_ah)))
#define ath_hal_getisr_nortc(_ah, _pmask, _type, _msg) \
    ((*(_ah)->ah_get_pending_interrupts)((_ah), (_pmask), (_type), (_msg), 1))
#define ath_hal_getisr(_ah, _pmask, _type, _msg) \
    ((*(_ah)->ah_get_pending_interrupts)((_ah), (_pmask), (_type), (_msg), 0))
#define ath_hal_setintrmitigation_timer(_ah, _reg, _val) \
    ((*(_ah)->ah_set_intr_mitigation_timer)((_ah), (_reg), (_val)))
#define ath_hal_getintrmitigation_timer(_ah, _reg) \
    ((*(_ah)->ah_get_intr_mitigation_timer)((_ah), (_reg)))
#if ATH_SUPPORT_WIFIPOS
#define ath_hal_read_loc_timer_reg(_ah)           \
        ((*(_ah)->ah_read_loc_timer_reg)(_ah))
#define ath_hal_get_eeprom_chain_mask(_ah)  \
        ((*(_ah)->ah_get_eeprom_chain_mask)(_ah))
#endif
#define ath_hal_updatetxtriglevel(_ah, _inc) \
    ((*(_ah)->ah_update_tx_trig_level)((_ah), (_inc)))
#define ath_hal_gettxtriglevel(_ah) \
    ((*(_ah)->ah_get_tx_trig_level)((_ah)))
#define ath_hal_setpower(_ah, _mode) \
    ((*(_ah)->ah_set_power_mode)((_ah), (_mode), AH_TRUE))
#define ath_hal_setsmpsmode(_ah, _mode) \
    ((*(_ah)->ah_set_sm_ps_mode)((_ah), (_mode)))
#if ATH_WOW
#define ath_hal_hasWow(_ah) \
    (ath_hal_getcapability(_ah, HAL_CAP_WOW, 0, NULL) == HAL_OK)
#define ath_hal_wowApplyPattern(_ah, _ppattern, _pmask, _count, _plen) \
    ((*(_ah)->ah_wow_apply_pattern)((_ah), (_ppattern), (_pmask), (_count), (_plen)))
/* #define ath_hal_wowWakeUp(_ah, _pattern) \
  ((*(_ah)->ah_wow_wake_up)((_ah), _pattern)) */
#define ath_hal_wowEnable(_ah, _patternEnable, _sleeptimeout, _clearbssid, _offloadEnable) \
    ((*(_ah)->ah_wow_enable)((_ah), (_patternEnable), (_sleeptimeout), (_clearbssid), \
                                                                       (_offloadEnable)))
#define ath_hal_wowWakeUp(_ah, _offloadEnabled) \
    ((*(_ah)->ah_wow_wake_up)((_ah), (_offloadEnabled)))
#define ath_hal_wowMatchPatternExact(_ah) \
    (ath_hal_getcapability(_ah, HAL_CAP_WOW_MATCH_EXACT, 0, NULL) == HAL_OK)
#define ath_hal_wowMatchPatternDword(_ah) \
    (ath_hal_getcapability(_ah, HAL_CAP_WOW_MATCH_DWORD, 0, NULL) == HAL_OK)
#if ATH_WOW_OFFLOAD
#define ath_hal_wowGtkOffloadSupported(_ah) \
    (ath_hal_getcapability(_ah, HAL_CAP_WOW_GTK_OFFLOAD, 0, NULL) == HAL_OK)
#define ath_hal_wowArpOffloadSupported(_ah) \
    (ath_hal_getcapability(_ah, HAL_CAP_WOW_ARP_OFFLOAD, 0, NULL) == HAL_OK)
#define ath_hal_wowNsOffloadSupported(_ah) \
    (ath_hal_getcapability(_ah, HAL_CAP_WOW_NS_OFFLOAD, 0, NULL) == HAL_OK)
#define ath_hal_wow4wayHsSupported(_ah) \
    (ath_hal_getcapability(_ah, HAL_CAP_WOW_4WAY_HS_WAKEUP, 0, NULL) == HAL_OK)
#define ath_hal_wowAcerMagicSupported(_ah) \
    (ath_hal_getcapability(_ah, HAL_CAP_WOW_ACER_MAGIC, 0, NULL) == HAL_OK)
#define ath_hal_wowAcerSwKaSupported(_ah) \
    (ath_hal_getcapability(_ah, HAL_CAP_WOW_ACER_SWKA, 0, NULL) == HAL_OK)
#define ath_hal_wowOffloadPrep(_ah) \
    (((_ah)->ah_wow_offload_prep((_ah))))
#define ath_hal_wowOffloadPost(_ah) \
    (((_ah)->ah_wow_offload_post((_ah))))
#define ath_hal_wowOffloadDownloadRekeyData(_ah, datap, bytes) \
    (((_ah)->ah_wow_offload_download_rekey_data((_ah), (datap), (bytes))))
#define ath_hal_wowOffloadRetrieveParam(_ah, _param, _type) \
    ((*(_ah)->ah_wow_offload_retrieve_data)((_ah), (_param), (_type)))
#define ath_hal_wowOffloadDownloadAcerMagic(_ah, _valid, _datap, _bytes) \
    (((_ah)->ah_wow_offload_download_acer_magic((_ah), (_valid), (_datap), (_bytes))))
#define ath_hal_wowOffloadDownloadAcerSWKA(_ah, _id, _valid, _period, _size, _datap) \
    (((_ah)->ah_wow_offload_download_acer_swka((_ah), (_id), (_valid), (_period), (_size), (_datap))))
#define ath_hal_wowOffloadDownloadArpOffloadData(_ah, _id, _data) \
    (((_ah)->ah_wow_offload_download_arp_info((_ah), (_id), (_data))))
#define ath_hal_wowOffloadDownloadNsOffloadData(_ah, _id, _data) \
    (((_ah)->ah_wow_offload_download_ns_info((_ah), (_id), (_data))))

#endif /* ATH_WOW_OFFLOAD */
#endif
#define ath_hal_keycachesize(_ah) \
    ((*(_ah)->ah_get_key_cache_size)((_ah)))
#define ath_hal_keyreset(_ah, _ix) \
    ((*(_ah)->ah_reset_key_cache_entry)((_ah), (_ix)))
#define ath_hal_keyset(_ah, _ix, _pk, _mac) \
    ((*(_ah)->ah_set_key_cache_entry)((_ah), (_ix), (_pk), (_mac), AH_FALSE))
#define ath_hal_keyisvalid(_ah, _ix) \
    (((*(_ah)->ah_is_key_cache_entry_valid)((_ah), (_ix))))
#define ath_hal_keysetmac(_ah, _ix, _mac) \
    ((*(_ah)->ah_set_key_cache_entry_mac)((_ah), (_ix), (_mac)))
#define ath_hal_printkeycache(_ah) \
    ((*(_ah)->ah_print_key_cache)((_ah)))
#if ATH_SUPPORT_KEYPLUMB_WAR
#define ath_hal_checkkey(_ah, _ix, _pk) \
    ((*(_ah)->ah_check_key_cache_entry)((_ah), (_ix), (_pk), AH_FALSE))
#endif
#define ath_hal_getrxfilter(_ah) \
    ((*(_ah)->ah_get_rx_filter)((_ah)))
#define ath_hal_setrxfilter(_ah, _filter) \
    ((*(_ah)->ah_set_rx_filter)((_ah), (_filter)))
#define ath_hal_setrxselevm(_ah, _selEvm, _justQuery) \
    ((int) (*(_ah)->ah_set_rx_sel_evm)((_ah), ((HAL_BOOL)(_selEvm)), \
    ((HAL_BOOL)(_justQuery))))
#define    ath_hal_setrxabort(_ah, _set) \
    ((*(_ah)->ah_set_rx_abort)((_ah), ((HAL_BOOL)(_set))))
#define ath_hal_setmcastfilter(_ah, _mfilt0, _mfilt1) \
    ((*(_ah)->ah_set_multicast_filter)((_ah), (_mfilt0), (_mfilt1)))
#define ath_hal_putrxbuf(_ah, _bufaddr, _qtype) \
    ((*(_ah)->ah_set_rx_dp)((_ah), (_bufaddr), (_qtype)))
#define ath_hal_gettsf32(_ah) \
    ((*(_ah)->ah_get_tsf32)((_ah)))
#define ath_hal_gettsf64(_ah) \
    ((*(_ah)->ah_get_tsf64)((_ah)))
#define ath_hal_gettsf2_32(_ah) \
    ((*(_ah)->ah_get_tsf2_32)((_ah)))
#define ath_hal_resettsf(_ah) \
    ((*(_ah)->ah_reset_tsf)((_ah)))
#define ath_hal_detectcardpresent(_ah) \
    ((*(_ah)->ah_detect_card_present)((_ah)))
#define ath_hal_rxena(_ah) \
    ((*(_ah)->ah_enable_receive)((_ah)))
#define ath_hal_numtxpending(_ah, _q) \
    ((*(_ah)->ah_num_tx_pending)((_ah), (_q)))
#define ath_hal_puttxbuf(_ah, _q, _bufaddr) \
    ((*(_ah)->ah_set_tx_dp)((_ah), (_q), (_bufaddr)))
#define ath_hal_gettxbuf(_ah, _q) \
    ((*(_ah)->ah_get_tx_dp)((_ah), (_q)))
#define ath_hal_getrxbuf(_ah, _qtype) \
    ((*(_ah)->ah_get_rx_dp)((_ah), (_qtype)))
#define ath_hal_txstart(_ah, _q) \
    ((*(_ah)->ah_start_tx_dma)((_ah), (_q)))
#define ath_hal_setchannel(_ah, _chan) \
    ((*(_ah)->ah_setChannel)((_ah), (_chan)))
#define ath_hal_calibrate(_ah, _chan, _rxchainmask, _longcal, _isIQdone, _is_scan, _cals) \
    ((*(_ah)->ah_per_calibration)((_ah), (_chan), (_rxchainmask), (_longcal), (_isIQdone), (_is_scan), _cals))
#define ath_hal_reset_calvalid(_ah, _chan, _isIQdone, _type) \
    ((*(_ah)->ah_reset_cal_valid)((_ah), (_chan), (_isIQdone), _type))
#define ath_hal_setledstate(_ah, _state) \
    ((*(_ah)->ah_set_led_state)((_ah), (_state)))
#define ath_hal_beaconinit(_ah, _nextb, _bperiod, _bfrac, _opmode) \
    ((*(_ah)->ah_beacon_init)((_ah), (_nextb), (_bperiod) ,(_bfrac),(_opmode)))
#define ath_hal_beacontimers(_ah, _bs) \
    ((*(_ah)->ah_set_station_beacon_timers)((_ah), (_bs)))
#define ath_hal_setassocid(_ah, _bss, _associd) \
    ((*(_ah)->ah_write_associd)((_ah), (_bss), (_associd)))
#define ath_hal_phydisable(_ah) \
    ((*(_ah)->ah_phy_disable)((_ah)))
#define ath_hal_disable(_ah) \
    ((*(_ah)->ah_disable)((_ah)))
#define ath_hal_setopmode(_ah) \
    ((*(_ah)->ah_set_pcu_config)((_ah)))
#define ath_hal_configpcipowersave(_ah, _restore, _powerOff) \
    ((*(_ah)->ah_config_pci_power_save)((_ah), (_restore), (_powerOff)))
#define ath_hal_stoptxdma(_ah, _qnum, _timeout) \
    ((*(_ah)->ah_stop_tx_dma)((_ah), (_qnum), (_timeout)))
#define ath_hal_stoptxdma_indvq(_ah, _qnum, _timeout) \
    ((*(_ah)->ah_stop_tx_dma_indv_que)((_ah), (_qnum), (_timeout)))
#define ath_hal_aborttxdma(_ah) \
    ((*(_ah)->ah_abort_tx_dma)((_ah)))
#define ath_hal_stoppcurecv(_ah) \
    ((*(_ah)->ah_stop_pcu_receive)((_ah)))
#define ath_hal_startpcurecv(_ah, _is_scanning) \
    ((*(_ah)->ah_start_pcu_receive)((_ah), (_is_scanning)))
#define ath_hal_stopdmarecv(_ah, _timeout) \
    ((*(_ah)->ah_stop_dma_receive)((_ah), (_timeout)))
#define ath_hal_getdiagstate(_ah, _id, _indata, _insize, _outdata, _outsize) \
    ((*(_ah)->ah_get_diag_state)((_ah), (_id), \
        (_indata), (_insize), (_outdata), (_outsize)))
#define ath_hal_gettxqueueprops(_ah, _q, _qi) \
    ((*(_ah)->ah_get_tx_queue_props)((_ah), (_q), (_qi)))
#define ath_hal_settxqueueprops(_ah, _q, _qi) \
    ((*(_ah)->ah_set_tx_queue_props)((_ah), (_q), (_qi)))
#define ath_hal_setuptxqueue(_ah, _type, _irq) \
    ((*(_ah)->ah_setup_tx_queue)((_ah), (_type), (_irq)))
#define ath_hal_resettxqueue(_ah, _q) \
    ((*(_ah)->ah_reset_tx_queue)((_ah), (_q)))
#define ath_hal_releasetxqueue(_ah, _q) \
    ((*(_ah)->ah_release_tx_queue)((_ah), (_q)))
#define ath_hal_getrfgain(_ah) \
    ((*(_ah)->ah_get_rf_gain)((_ah)))
#define ath_hal_getdefantenna(_ah) \
    ((*(_ah)->ah_get_def_antenna)((_ah)))
#define ath_hal_setdefantenna(_ah, _ant) \
    ((*(_ah)->ah_set_def_antenna)((_ah), (_ant)))
#define ath_hal_setAntennaSwitch(_ah, _ant, _chan, _txchmsk, _rxchmsk, _antcfgd) \
    ((*(_ah)->ah_set_antenna_switch)((_ah), (_ant), (_chan), (_txchmsk), (_rxchmsk), (_antcfgd)))
#define ath_hal_selectAntConfig(_ah, _cfg) \
    ((*(_ah)->ah_select_ant_config)((_ah), (_cfg)))
#define ath_hal_ant_ctrl_common_get(_ah, _is_2ghz) \
    ((*(_ah)->ah_ant_ctrl_common_get)((_ah), (_is_2ghz)))
#define ath_hal_ant_swcom_sel(_ah, _ops, _tbl1, _tbl2) \
    ((*(_ah)->ah_ant_swcom_sel)((_ah), (_ops), (_tbl1), (_tbl2)))
#define ath_hal_rxmonitor(_ah, _arg, _chan, _anistats) \
    ((*(_ah)->ah_rx_monitor)((_ah), (_arg), (_chan), (_anistats)))
#define ath_hal_mibevent(_ah, _stats) \
    ((*(_ah)->ah_proc_mib_event)((_ah), (_stats)))
#define ath_hal_setslottime(_ah, _us) \
    ((*(_ah)->ah_set_slot_time)((_ah), (_us)))
#define ath_hal_setacktimeout(_ah, _us) \
    ((*(_ah)->ah_set_ack_timeout)((_ah), (_us)))
#define ath_hal_getacktimeout(_ah) \
    ((*(_ah)->ah_get_ack_timeout)((_ah)))
#define ath_hal_enablePhyDiag(_ah) \
    ((*(_ah)->ah_enablePhyErrDiag)((_ah)))
#define ath_hal_disablePhyDiag(_ah) \
    ((*(_ah)->ah_disablePhyErrDiag)((_ah)))
#define ath_hal_getcapability(_ah, _cap, _param, _result) \
    ((*(_ah)->ah_get_capability)((_ah), (_cap), (_param), (_result)))
#define ath_hal_setcapability(_ah, _cap, _param, _v, _status) \
    ((*(_ah)->ah_set_capability)((_ah), (_cap), (_param), (_v), (_status)))
#define ath_hal_ciphersupported(_ah, _cipher) \
    (ath_hal_getcapability(_ah, HAL_CAP_CIPHER, _cipher, NULL) == HAL_OK)
#define ath_hal_fastframesupported(_ah) \
    (ath_hal_getcapability(_ah, HAL_CAP_FASTFRAME, 0, NULL) == HAL_OK)
#define ath_hal_burstsupported(_ah) \
    (ath_hal_getcapability(_ah, HAL_CAP_BURST, 0, NULL) == HAL_OK)
#define ath_hal_xrsupported(_ah) \
    (ath_hal_getcapability(_ah, HAL_CAP_XR, 0, NULL) == HAL_OK)
#define ath_hal_proxySTASupported(_ah) \
		(ath_hal_getcapability(_ah, HAL_CAP_PROXYSTA, 0, NULL) == HAL_OK)
#define ath_hal_htsupported(_ah) \
    (ath_hal_getcapability(_ah, HAL_CAP_HT, 0, NULL) == HAL_OK)
#define ath_hal_ht20sgisupported(_ah) \
    (ath_hal_getcapability(_ah, HAL_CAP_HT20_SGI, 0, NULL) == HAL_OK)
#define ath_hal_rxstbcsupport(_ah, _rxstbc) \
    (ath_hal_getcapability(_ah, HAL_CAP_RX_STBC, 0, _rxstbc) == HAL_OK)
#define ath_hal_txstbcsupport(_ah, _txstbc) \
    (ath_hal_getcapability(_ah, HAL_CAP_TX_STBC, 0, _txstbc) == HAL_OK)
#define ath_hal_ldpcsupport(_ah, _ldpc) \
    (ath_hal_getcapability(_ah, HAL_CAP_LDPC, 0, _ldpc) == HAL_OK)
#define ath_hal_weptkipaggrsupport(_ah) \
    (ath_hal_getcapability(_ah, HAL_CAP_WEP_TKIP_AGGR, 0, NULL) == HAL_OK)
#define ath_hal_gettxdelimweptkipaggr(_ah, _pv) \
    (ath_hal_getcapability(_ah, HAL_CAP_WEP_TKIP_AGGR_TX_DELIM, 0, _pv))
#define ath_hal_getrxdelimweptkipaggr(_ah, _pv) \
    (ath_hal_getcapability(_ah, HAL_CAP_WEP_TKIP_AGGR_RX_DELIM, 0, _pv))
#define ath_hal_gttsupported(_ah) \
    (ath_hal_getcapability(_ah, HAL_CAP_GTT, 0, NULL) == HAL_OK)
#define ath_hal_turboagsupported(_ah, _ath_countrycode) \
    (ath_hal_getwirelessmodes(_ah, _ath_countrycode) & (HAL_MODE_108G|HAL_MODE_TURBO))
#define ath_hal_halfrate_chansupported(_ah) \
    (ath_hal_getcapability(_ah, HAL_CAP_CHAN_HALFRATE, 0, NULL) == HAL_OK)
#define ath_hal_quarterrate_chansupported(_ah) \
    (ath_hal_getcapability(_ah, HAL_CAP_CHAN_QUARTERRATE, 0, NULL) == HAL_OK)
#define ath_hal_getregdomain(_ah, _prd) \
    ath_hal_getcapability(_ah, HAL_CAP_REG_DMN, 0, (_prd))
#define ath_hal_setregdomain(_ah, _regdmn, _statusp) \
    ((*(_ah)->ah_set_regulatory_domain)((_ah), (_regdmn), (_statusp)))
#define ath_hal_getcountrycode(_ah, _pcc) \
    (*(_pcc) = ath_hal_get_countryCode(_ah))
#define ath_hal_tkipsplit(_ah) \
    (ath_hal_getcapability(_ah, HAL_CAP_TKIP_SPLIT, 0, NULL) == HAL_OK)
#define ath_hal_wmetkipmic(_ah) \
    (ath_hal_getcapability(_ah, HAL_CAP_WME_TKIPMIC, 0, NULL) == HAL_OK)
#define ath_hal_hwphycounters(_ah) \
    (ath_hal_getcapability(_ah, HAL_CAP_PHYCOUNTERS, 0, NULL) == HAL_OK)
#define ath_hal_hasdiversity(_ah) \
    (ath_hal_getcapability(_ah, HAL_CAP_DIVERSITY, 0, NULL) == HAL_OK)
#define ath_hal_getdiversity(_ah) \
    (ath_hal_getcapability(_ah, HAL_CAP_DIVERSITY, 1, NULL) == HAL_OK)
#define ath_hal_setdiversity(_ah, _v) \
    ath_hal_setcapability(_ah, HAL_CAP_DIVERSITY, 1, _v, NULL)
#define ath_hal_getnumtxqueues(_ah, _pv) \
    (ath_hal_getcapability(_ah, HAL_CAP_NUM_TXQUEUES, 0, _pv) == HAL_OK)
#define ath_hal_hasveol(_ah) \
    (ath_hal_getcapability(_ah, HAL_CAP_VEOL, 0, NULL) == HAL_OK)
#define ath_hal_hastxpowlimit(_ah) \
    (ath_hal_getcapability(_ah, HAL_CAP_TXPOW, 0, NULL) == HAL_OK)
#define ath_hal_settxpowlimit(_ah, _pow, _extra_txpow, _tpcInDb) \
    ((*(_ah)->ah_set_tx_power_limit)((_ah), (_pow), (_extra_txpow), (_tpcInDb)))
#define ath_hal_gettxpowlimit(_ah, _ppow) \
    ath_hal_getcapability(_ah, HAL_CAP_TXPOW, 1, _ppow)
#define ath_hal_getmaxtxpow(_ah, _ppow) \
    ath_hal_getcapability(_ah, HAL_CAP_TXPOW, 2, _ppow)
#define ath_hal_gettpscale(_ah, _scale) \
    ath_hal_getcapability(_ah, HAL_CAP_TXPOW, 3, _scale)
#define ath_hal_getpwscale(_ah, _pwscale) \
    ath_hal_getcapability(_ah, HAL_CAP_TXPOW, 5, _pwscale)
#define ath_hal_settpscale(_ah, _v) \
    ath_hal_setcapability(_ah, HAL_CAP_TXPOW, 3, _v, NULL)
#define ath_hal_setpwscale(_ah, _v) \
    ath_hal_setcapability(_ah, HAL_CAP_TXPOW, 5, _v, NULL)
#define ath_hal_hastpc(_ah) \
    (ath_hal_getcapability(_ah, HAL_CAP_TPC, 0, NULL) == HAL_OK)
#define ath_hal_gettpc(_ah) \
    (ath_hal_getcapability(_ah, HAL_CAP_TPC, 1, NULL) == HAL_OK)
#define ath_hal_settpc(_ah, _v) \
    ath_hal_setcapability(_ah, HAL_CAP_TPC, 1, _v, NULL)
#define ath_hal_hasbursting(_ah) \
    (ath_hal_getcapability(_ah, HAL_CAP_BURST, 0, NULL) == HAL_OK)
#define ath_hal_hasfastframes(_ah) \
    (ath_hal_getcapability(_ah, HAL_CAP_FASTFRAME, 0, NULL) == HAL_OK)
#define ath_hal_hasbssidmask(_ah) \
    (ath_hal_getcapability(_ah, HAL_CAP_BSSIDMASK, 0, NULL) == HAL_OK)
#define ath_hal_setmcastkeysearch(_ah, _v)                                 \
    ath_hal_setcapability(_ah, HAL_CAP_MCAST_KEYSRCH, 1, _v, NULL)
#define ath_hal_hasmcastkeysearch(_ah) \
    (ath_hal_getcapability(_ah, HAL_CAP_MCAST_KEYSRCH, 0, NULL) == HAL_OK)
#define ath_hal_getmcastkeysearch(_ah) \
    (ath_hal_getcapability(_ah, HAL_CAP_MCAST_KEYSRCH, 1, NULL) == HAL_OK)
#define ath_hal_hastkipmic(_ah) \
    (ath_hal_getcapability(_ah, HAL_CAP_TKIP_MIC, 0, NULL) == HAL_OK)
#define ath_hal_gettkipmic(_ah) \
    (ath_hal_getcapability(_ah, HAL_CAP_TKIP_MIC, 1, NULL) == HAL_OK)
#define ath_hal_settkipmic(_ah, _v) \
    ath_hal_setcapability(_ah, HAL_CAP_TKIP_MIC, 1, _v, NULL)
#define ath_hal_hastsfadjust(_ah) \
    (ath_hal_getcapability(_ah, HAL_CAP_TSF_ADJUST, 0, NULL) == HAL_OK)
#define ath_hal_gettsfadjust(_ah) \
    (ath_hal_getcapability(_ah, HAL_CAP_TSF_ADJUST, 1, NULL) == HAL_OK)
#define ath_hal_settsfadjust(_ah, _v) \
    ath_hal_setcapability(_ah, HAL_CAP_TSF_ADJUST, 1, _v, NULL)
#define ath_hal_hashwbeaconprocsupport(_ah) \
    (ath_hal_getcapability(_ah, HAL_CAP_HW_BEACON_PROC_SUPPORT, 0, NULL) == HAL_OK)
#define ath_hal_haswpsbutton(_ah) \
    (ath_hal_getcapability(_ah, HAL_CAP_WPS_PUSH_BUTTON, 0, NULL) == HAL_OK)
#define ath_hal_AntDivCombSupport(_ah) \
    (ath_hal_getcapability(_ah, HAL_CAP_ANT_DIV_COMB, 0, NULL) == HAL_OK)
#define ath_hal_hasenhanceddmasupport(_ah) \
    (ath_hal_getcapability(_ah, HAL_CAP_ENHANCED_DMA_SUPPORT, 0, NULL) == HAL_OK)
#ifdef ATH_SUPPORT_DFS
#define ath_hal_hasenhanceddfssupport(_ah) \
    (ath_hal_getcapability(_ah, HAL_CAP_ENHANCED_DFS_SUPPORT, 0, NULL) == HAL_OK)
#define ath_hal_forcevcs(_ah) \
    ((*(_ah)->ah_forceVCS)((_ah)))
#define ath_hal_dfs3streamfix(_ah, _on) \
    ((*(_ah)->ah_setDfs3StreamFix)((_ah), (_on)))
#define ath_hal_get3StreamSignature(_ah) \
        ((*(_ah)->ah_get3StreamSignature)((_ah)))
#endif
#define ath_hal_hasrxtxabortsupport(_ah) \
    (ath_hal_getcapability((_ah), HAL_CAP_RXTX_ABORT, 0, NULL) == HAL_OK)
#define ath_hal_getanipollinterval(_ah, _interval) \
    (ath_hal_getcapability((_ah), HAL_CAP_ANI_POLL_INTERVAL, 0, (_interval)) == HAL_OK)
#define ath_hal_getchannelswitchingtime(_ah, _pv) \
            (ath_hal_getcapability(_ah, HAL_CAP_CHANNEL_SWITCH_TIME_USEC, 0, _pv))
#define ath_hal_hashwuapsdtrig(_ah) \
    (ath_hal_getcapability(_ah, HAL_CAP_HW_UAPSD_TRIG, 0, NULL) == HAL_OK)
#define ath_hal_getenableapm(_ah, _enableapm) \
    (ath_hal_getcapability(_ah, HAL_CAP_ENABLE_APM, 0, (_enableapm)) == HAL_OK)
#define ath_hal_getsmartantenna(_ah, _smartant) \
    ath_hal_getcapability(_ah, HAL_CAP_LMAC_SMART_ANT, 0, _smartant)

#define ath_hal_setuprxdesc(_ah, _ds, _size, _intreq) \
    ((*(_ah)->ah_setup_rx_desc)((_ah), (_ds), (_size), (_intreq)))
#define ath_hal_rxprocdesc(_ah, _ds, _dspa, _dsnext, _tsf, _rxs) \
    ((*(_ah)->ah_proc_rx_desc)((_ah), (_ds), (_dspa), (_dsnext), (_tsf), (_rxs)))
#define ath_hal_get_rxkeyidx(_ah, _ds, _keyix, _dsstat) \
    ((*(_ah)->ah_get_rx_key_idx)((_ah), (_ds), (_keyix), (_dsstat)))
#define ath_hal_rxprocdescfast(_ah, _ds, _dspa, _dsnext, _rxstat, _bfaddr) \
    ((*(_ah)->ah_proc_rx_desc_fast)((_ah), (_ds), (_dspa), (_dsnext), (_rxstat), (_bfaddr)))
#define ath_hal_filltxdesc(_ah, _ds, _addr, _l, _descid, _q, _keytype, _first, _last, _ds0) \
    ((*(_ah)->ah_fill_tx_desc)((_ah), (_ds), (_addr), (_l), (_descid), (_q), (_keytype), (_first), (_last), (_ds0)))
#define ath_hal_setdesclink(_ah, _ds, _link) \
    ((*(_ah)->ah_set_desc_link)((_ah), (_ds), (_link)))
#define ath_hal_getdesclinkptr(_ah, _ds, _link) \
    ((*(_ah)->ah_get_desc_link_ptr)((_ah), (_ds), (_link)))
#define ath_hal_cleartxdesc(_ah, _ds) \
    ((*(_ah)->ah_clear_tx_desc_status)((_ah), (_ds)))
#ifdef ATH_SWRETRY
#define ath_hal_cleardestmask(_ah, _ds) \
    ((*(_ah)->ah_clear_dest_mask)((_ah), (_ds)))
#endif
#define ath_hal_txprocdesc(_ah, _ds) \
    ((*(_ah)->ah_proc_tx_desc)((_ah), (_ds)))
#define ath_hal_getrawtxdesc(_ah, _ts) \
    ((*(_ah)->ah_get_raw_tx_desc)((_ah), (_ts)))
#define ath_hal_gettxratecode(_ah, _ds, _ts) \
    ((*(_ah)->ah_get_tx_rate_code)((_ah), (_ds), (_ts)))
#define ath_hal_gettxintrtxqs(_ah, _txqs) \
    ((*(_ah)->ah_get_tx_intr_queue)((_ah), (_txqs)))
#define ath_hal_txreqintrdesc(_ah, _ds) \
    ((*(_ah)->ah_req_tx_intr_desc)((_ah), (_ds)))
#define ath_hal_txcalcairtime(_ah, _ds, _ts, _comp_wastedt, _nbad, _nframes) \
    ((*(_ah)->ah_calc_tx_airtime)((_ah), (_ds), (_ts), (_comp_wastedt), (_nbad), (_nframes)))

#define ath_hal_gpioCfgInput(_ah, _gpio) \
    ((*(_ah)->ah_gpio_cfg_input)((_ah), (_gpio)))
#define ath_hal_gpioCfgOutput(_ah, _gpio, _signalType) \
    ((*(_ah)->ah_gpio_cfg_output)((_ah), (_gpio), (_signalType)))
#define ath_hal_gpioCfgOutput_LEDoff(_ah, _gpio, _signalType) \
    ((*(_ah)->ah_gpio_cfg_output_led_off)((_ah), (_gpio), (_signalType)))
#define ath_hal_gpioset(_ah, _gpio, _b) \
    ((*(_ah)->ah_gpio_set)((_ah), (_gpio), (_b)))
#define ath_hal_gpioget(_ah, _gpio) \
        ((*(_ah)->ah_gpio_get)((_ah), (_gpio)))
#define    ath_hal_setcoverageclass(_ah, _coverageclass, _now) \
    ((*(_ah)->ah_set_coverage_class)((_ah), (_coverageclass), (_now)))
#define    ath_hal_setQuiet(_ah, _period, _duration, _nextStart, _flag) \
    ((*(_ah)->ah_set_quiet)((_ah), (_period),(_duration), (_nextStart), (_flag)))

#define ath_hal_setpowerledstate(_ah, _enable) \
    ((*(_ah)->ah_setpowerledstate)((_ah), (_enable)))
#define ath_hal_setnetworkledstate(_ah, _enable) \
    ((*(_ah)->ah_setnetworkledstate)((_ah), (_enable)))

#define ath_hal_setuptxstatusring(_ah, _vaddr, _paddr, _len) \
    ((*(_ah)->ah_setup_tx_status_ring)((_ah), (_vaddr), (_paddr), (_len)))

#define ath_hal_enabletpc(_ah) \
    ((*(_ah)->ah_enable_tpc)((_ah)))

#if ATH_ANT_DIV_COMB
#define ath_hal_set_lna_div_use_bt_ant(_ah, _enable, _chan) \
    ((*(_ah)->ah_ant_ctrl_set_lna_div_use_bt_ant)((_ah), (_enable), (_chan)))
#endif /* ATH_ANT_DIV_COMB */

/* TSF Sync : AoW */
#define ath_hal_forcetsf_sync(_ah, _bss, _associd) \
    ((*(_ah)->ah_force_tsf_sync)((_ah), (_bss), (_associd)))

/* DFS defines */
#define ath_hal_radar_wait(_ah, _chan) \
    ((*(_ah)->ah_radar_wait)((_ah), (_chan)))
#define ath_hal_dfsfound(_ah, _chan, _time) \
        ((*(_ah)->ah_ar_dfs_found)((_ah), (_chan), (_time)))
#define ath_hal_enabledfs(_ah, _param) \
        ((*(_ah)->ah_ar_enable_dfs)((_ah), (_param)))
#define ath_hal_getdfsthresh(_ah, _param) \
        ((*(_ah)->ah_ar_get_dfs_thresh)((_ah), (_param)))
#define ath_hal_get_extension_channel(_ah) \
        ((*(_ah)->ah_get_extension_channel)((_ah)))
#define ath_hal_is_fast_clock_enabled(_ah) \
        ((*(_ah)->ah_is_fast_clock_enabled)((_ah)))
#define ath_hal_adjust_difs(_ah, _val) \
        ((*(_ah)->ah_adjust_difs)((_ah), (_val)))
#define ath_hal_dfs_config_fft(_ah, _val)       \
    ((*(_ah)->ah_dfs_config_fft)((_ah), (_val)))
#define ath_hal_dfs_cac_war(_ah, _val) \
        ((*(_ah)->ah_dfs_cac_war)((_ah), (_val)))

/* PCIe Error Monitor utility defines */
#define ath_hal_start_pcie_error_monitor(_ah,_bAutoStop) \
    ( (*(_ah)->ah_start_pcie_error_monitor) ? (*(_ah)->ah_start_pcie_error_monitor)((_ah),(_bAutoStop)) : -1)

#define ath_hal_read_pcie_error_monitor(_ah,_pReadCounters) \
    ( (*(_ah)->ah_read_pcie_error_monitor) ? (*(_ah)->ah_read_pcie_error_monitor)((_ah),(_pReadCounters)) : -1)

#define ath_hal_stop_pcie_error_monitor(_ah) \
    ( (*(_ah)->ah_stop_pcie_error_monitor) ? (*(_ah)->ah_stop_pcie_error_monitor)((_ah)) : -1)

/* Spectral scan defines*/
#define ath_hal_configure_spectral(_ah, _param) \
        ((*(_ah)->ah_ar_configure_spectral)((_ah), (_param)))
#define ath_hal_is_spectral_active(_ah) \
        ((*(_ah)->ah_ar_is_spectral_active)((_ah)))
#define ath_hal_is_spectral_enabled(_ah) \
        ((*(_ah)->ah_ar_is_spectral_enabled)((_ah)))
#define ath_hal_start_spectral_scan(_ah) \
        ((*(_ah)->ah_ar_start_spectral_scan)((_ah)))
#define ath_hal_stop_spectral_scan(_ah) \
        ((*(_ah)->ah_ar_stop_spectral_scan)((_ah)))
#define ath_hal_get_spectral_config(_ah, _param) \
        ((*(_ah)->ah_ar_get_spectral_config)((_ah), (_param)))
#define ath_hal_get_ctl_nf(_ah) \
        ((*(_ah)->ah_ar_get_ctl_nf)((_ah)))
#define ath_hal_get_ext_nf(_ah) \
        ((*(_ah)->ah_ar_get_ext_nf)((_ah)))
#define ath_hal_get_nominal_nf(_ah, band) \
        ((*(_ah)->ah_ar_get_nominal_nf)((_ah), (band)))
/* Spectral scan defines end*/

#define ath_hal_enable_test_addac_mode(_ah) \
        ((*(_ah)->ah_ar_enable_test_addac_mode)((_ah)))
#define ath_hal_disable_test_addac_mode(_ah) \
        ((*(_ah)->ah_ar_disable_test_addac_mode)((_ah)))
#define ath_hal_begin_adc_capture(_ah, _auto_agc_gain) \
        ((*(_ah)->ah_ar_begin_adc_capture)((_ah), (_auto_agc_gain)))
#define ath_hal_retrieve_capture_data(_ah, _chain_mask, _disable_dc_filter, _sample_buf, _max_samples) \
        ((*(_ah)->ah_ar_retrieve_capture_data)((_ah), (_chain_mask), (_disable_dc_filter), (_sample_buf), (_max_samples)))
#define ath_hal_calculate_adc_ref_powers(_ah, _freq, _sample_min, _sample_max, _chain_ref_pwrs, _len_chain_ref_pwrs) \
        ((*(_ah)->ah_ar_calculate_adc_ref_powers)((_ah), (_freq), (_sample_min), (_sample_max), (_chain_ref_pwrs), (_len_chain_ref_pwrs)))
#define ath_hal_get_min_agc_gain(_ah, _freq, _chain_gain, _len_chain_gain) \
        ((*(_ah)->ah_ar_get_min_agc_gain)((_ah), (_freq), (_chain_gain), (_len_chain_gain)))

extern int16_t ath_hal_getChanNoise(struct ath_hal *ah, HAL_CHANNEL *chan);

#define ath_hal_gpioGet(_ah, _gpio) \
    ((*(_ah)->ah_gpio_get)((_ah), (_gpio)))
#define ath_hal_gpioGetIntr(_ah) \
    (_ah)->ah_gpio_get_intr(_ah)
#define ath_hal_gpioSetIntr(_ah, _gpio, _ilevel) \
    ((*(_ah)->ah_gpio_set_intr)((_ah), (_gpio), (_ilevel)))
#define ath_hal_gpioGetPolarity(_ah) \
    (_ah)->ah_gpio_get_polarity(_ah)
#define ath_hal_gpioSetPolarity(_ah, _pol_map, _changed_mask) \
    (_ah)->ah_gpio_set_polarity(_ah, _pol_map, _changed_mask)
#define ath_hal_gpioGetMask(_ah) \
    (_ah)->ah_gpio_get_mask(_ah)
#define ath_hal_gpioSetMask(_ah, _mask, _pol_map) \
    (_ah)->ah_gpio_set_mask(_ah, _mask, _pol_map)
#define ath_hal_hasrfkill(_ah)  \
    (ath_hal_getcapability(_ah, HAL_CAP_RFSILENT, 0, NULL) == HAL_OK)
#define ath_hal_isrfkillenabled(_ah)  \
    (ath_hal_getcapability(_ah, HAL_CAP_RFSILENT, 1, NULL) == HAL_OK)
#define ath_hal_getrfkillinfo(_ah, _ri)  \
    ath_hal_getcapability(_ah, HAL_CAP_RFSILENT, 2, (void *)(_ri))
#define ath_hal_enable_rfkill(_ah, _v) \
    ath_hal_setcapability(_ah, HAL_CAP_RFSILENT, 1, _v, NULL)
#define ath_hal_hasrfkillInt(_ah)  \
    (ath_hal_getcapability(_ah, HAL_CAP_RFSILENT, 3, NULL) == HAL_OK)
#define ath_hal_hasPciePwrsave(_ah)  \
    (ath_hal_getcapability(_ah, HAL_CAP_PCIE_PS, 0, NULL) == HAL_OK)
#define ath_hal_isPciePwrsaveEnabled(_ah)  \
    (ath_hal_getcapability(_ah, HAL_CAP_PCIE_PS, 1, NULL) == HAL_OK)
#define ath_hal_isPcieEntSyncEnabled(_ah)  \
    (ath_hal_getcapability(_ah, HAL_CAP_PCIE_LCR_EXTSYNC_EN, 0, NULL) == HAL_OK)
#define ath_hal_pcieGetLcrOffset(_ah, _offset)  \
    ath_hal_getcapability(_ah, HAL_CAP_PCIE_LCR_OFFSET, 0, (void *)(_offset))

#define ath_hal_hasMbssidAggrSupport(_ah)  \
    (ath_hal_getcapability(_ah, HAL_CAP_MBSSID_AGGR_SUPPORT, 0, NULL) == HAL_OK)
#define ath_hal_getdescinfo(_ah, _descinfo) \
    ((*(_ah)->ah_get_desc_info)((_ah), (_descinfo)))

#define ath_hal_hasfastcc(_ah) \
    (ath_hal_getcapability(_ah, HAL_CAP_FAST_CC, 0, NULL) == HAL_OK)
#define ath_hal_gettxchainmask(_ah, _pv)  \
    (ath_hal_getcapability(_ah, HAL_CAP_TX_CHAINMASK, 0, _pv) == HAL_OK)
#define ath_hal_getrxchainmask(_ah, _pv)  \
    (ath_hal_getcapability(_ah, HAL_CAP_RX_CHAINMASK, 0, _pv) == HAL_OK)
#define ath_hal_hascst(_ah) \
    (ath_hal_getcapability(_ah, HAL_CAP_CST, 0, NULL) == HAL_OK)
#define ath_hal_hasrifsrx(_ah) \
    (ath_hal_getcapability(_ah, HAL_CAP_RIFS_RX, 0, NULL) == HAL_OK)
#define ath_hal_hasrifstx(_ah) \
    (ath_hal_getcapability(_ah, HAL_CAP_RIFS_TX, 0, NULL) == HAL_OK)
#define ath_hal_getrtsaggrlimit(_ah, _pv) \
    ath_hal_getcapability(_ah, HAL_CAP_RTS_AGGR_LIMIT, 0, _pv)
#define ath_hal_4addraggrsupported(_ah) \
    (ath_hal_getcapability(_ah, HAL_CAP_4ADDR_AGGR, 0, NULL) == HAL_OK)
#define ath_hal_bb_rifs_rx_enabled(_ah) \
    (ath_hal_getcapability(_ah, HAL_CAP_RIFS_RX_ENABLED, 0, NULL) == HAL_OK)
#define ath_hal_get_hang_types(_ah, _types) \
    (*(_ah)->ah_get_hang_types)((_ah), (_types))

#define ath_hal_hasautosleep(_ah) \
    (ath_hal_getcapability(_ah, HAL_CAP_AUTO_SLEEP, 0, NULL) == HAL_OK)

#define ath_hal_has4kbsplittrans(_ah) \
    (ath_hal_getcapability(_ah, HAL_CAP_4KB_SPLIT_TRANS, 0, NULL) == HAL_OK)

#define ath_hal_get_max_weptkip_ht20tx_rateKbps(_ah, _pv) \
    ath_hal_getcapability(_ah, HAL_CAP_MAX_WEP_TKIP_HT20_TX_RATEKBPS, 0, _pv)
#define ath_hal_get_max_weptkip_ht40tx_rateKbps(_ah, _pv) \
    ath_hal_getcapability(_ah, HAL_CAP_MAX_WEP_TKIP_HT40_TX_RATEKBPS, 0, _pv)
#define ath_hal_get_max_weptkip_ht20rx_rateKbps(_ah, _pv) \
    ath_hal_getcapability(_ah, HAL_CAP_MAX_WEP_TKIP_HT20_RX_RATEKBPS, 0, _pv)
#define ath_hal_get_max_weptkip_ht40rx_rateKbps(_ah, _pv) \
    ath_hal_getcapability(_ah, HAL_CAP_MAX_WEP_TKIP_HT40_RX_RATEKBPS, 0, _pv)
#define ath_hal_getnumtxmaps(_ah, _pv) \
    ath_hal_getcapability(_ah, HAL_CAP_NUM_TXMAPS, 0, _pv)
#define ath_hal_gettxdesclen(_ah, _pv) \
    ath_hal_getcapability(_ah, HAL_CAP_TXDESCLEN, 0, _pv)
#define ath_hal_gettxstatuslen(_ah, _pv) \
    ath_hal_getcapability(_ah, HAL_CAP_TXSTATUSLEN, 0, _pv)
#define ath_hal_getrxstatuslen(_ah, _pv) \
    ath_hal_getcapability(_ah, HAL_CAP_RXSTATUSLEN, 0, _pv)
#define ath_hal_getrxfifodepth(_ah, _qtype, _pv) \
    ath_hal_getcapability(_ah, HAL_CAP_RXFIFODEPTH, _qtype, _pv)
#define ath_hal_setrxbufsize(_ah, _v) \
    ath_hal_setcapability(_ah, HAL_CAP_RXBUFSIZE, 1, _v, NULL)
#if ATH_SUPPORT_WAPI
#define ath_hal_getwapimaxtxchains(_ah, _pv)  \
    ath_hal_getcapability(_ah, HAL_CAP_WAPI_MAX_TX_CHAINS, 0, _pv)
#define ath_hal_getwapimaxrxchains(_ah, _pv)  \
    ath_hal_getcapability(_ah, HAL_CAP_WAPI_MAX_RX_CHAINS, 0, _pv)
#endif

/* 11n */
#define ath_hal_set11n_txdesc(_ah, _ds, _pktlen, _type, _txpower,           \
    _keyix, _keytype, _flags)                                               \
    ((*(_ah)->ah_set_11n_tx_desc)(_ah, _ds, _pktlen, _type, _txpower,       \
    _keyix, _keytype, _flags));
#if ATH_SUPPORT_WIFIPOS
#define ath_hal_set_rx_chainmask(_ah, _chainmask)           \
        ((*(_ah)->ah_set_rx_chainmask)(_ah, _chainmask))
#define ath_hal_update_loc_ctl_reg(_ah, _pos_bit)           \
        ((*(_ah)->ah_update_loc_ctl_reg)(_ah, _pos_bit))
#endif

#define ath_hal_set_paprd_txdesc(_ah, _ds, chain_num)           \
        ((*(_ah)->ah_set_paprd_tx_desc)(_ah, _ds, chain_num))
#define ath_hal_paprd_setup_gain_table(_ah, chain_num)           \
        ((*(_ah)->ah_paprd_setup_gain_table)(_ah, chain_num))
#define ath_hal_paprd_create_curve(_ah, _chan, _chainNum)           \
        ((*(_ah)->ah_paprd_create_curve)(_ah, _chan, _chainNum))

#define ath_hal_paprd_init_table(_ah, _chan)           \
        ((*(_ah)->ah_paprd_init_table)(_ah, _chan))

#define ath_hal_paprd_is_done(_ah)                           \
        ((*(_ah)->ah_paprd_is_done)(_ah))
#define ath_hal_paprd_enable(_ah, _enableFlag, _chan)               \
        ((*(_ah)->ah_PAPRDEnable)(_ah, _enableFlag, _chan))

#define ath_hal_paprd_papulate_table(_ah, _chan, _chainNum)   \
        ((*(_ah)->ah_paprd_populate_table)(_ah, _chan, _chainNum))

#define ath_hal_is_tx_done(_ah)                               \
        ((*(_ah)->ah_is_tx_done)(_ah))
#define ath_hal_paprd_dec_tx_pwr(_ah)                          \
        ((*(_ah)->ah_paprd_dec_tx_pwr)(_ah))

#define ath_hal_paprd_thermal_send(_ah)                           \
        ((*(_ah)->ah_paprd_thermal_send)(_ah))

#define ath_hal_factory_reset(_ah)                             \
        ((*(_ah)->ah_factory_defaults)(_ah))
/* TxBF */
#ifdef  ATH_SUPPORT_TxBF
#define ath_hal_hastxbf(_ah) \
    (ath_hal_getcapability(_ah, HAL_CAP_TXBF, 0, NULL) == HAL_OK)
#define ath_hal_set11nTxBFSounding(_ah, _ds, _CalPos,_series,_CEC, _opt)                    \
    ((*(_ah)->ah_set_11n_tx_bf_sounding)((_ah), (_ds), (_CalPos), (_series),(_CEC),(_opt)))
#define ath_hal_set11nTxBFCal(_ah, _ds, _CalPos,_codeRate,_CEC,_opt)                    \
    ((*(_ah)->ah_set_11n_tx_bf_cal)((_ah), (_ds), (_CalPos), (_codeRate),(_CEC),(_opt)))
#define ath_hal_TxBFRCUpdate(_ah,_rx_status,_local_h,_CSIFrame,_NESSA,_NESSB,_BW) \
    ((*(_ah)->ah_tx_bf_rc_update)((_ah),(_rx_status),(_local_h),(_CSIFrame),(_NESSA),(_NESSB),(_BW)))
#define ath_hal_gettxbfcapability(_ah)             \
    ((*(_ah)->ah_get_tx_bf_capability)((_ah)))
#define ath_hal_FillCSIFrame(_ah,_rx_status,_BW,_local_h,_CSIFrameBody) \
    ((*(_ah)->ah_fill_csi_frame)((_ah),(_rx_status),(_BW),(_local_h),(_CSIFrameBody)))
#define ath_hal_keyreadmac(_ah, _ix, _mac) \
    ((*(_ah)->ah_read_key_cache_mac)((_ah), (_ix), (_mac)))
#define ath_hal_TxBFSetKey(_ah,_entry,_rx_staggered_sounding,_channel_estimation_cap,_MMSS) \
    ((*(_ah)->ah_tx_bf_set_key)((_ah),(_entry),(_rx_staggered_sounding),(_channel_estimation_cap),(_MMSS)))
#define ath_hal_setHwCvTimeout(_ah , _opt)             \
    ((*(_ah)->ah_setHwCvTimeout)((_ah) ,(_opt)))
#define ath_hal_TxBFGetCvCacheNr(_ah, _keyidx, _nr) \
    ((*(_ah)->ah_tx_bf_get_cv_cache_nr)((_ah), (_keyidx), (_nr)))
#define ath_hal_reconfigHXferTimeout(_ah , _is_reset)   \
    ((*(_ah)->ah_reconfig_h_xfer_timeout)((_ah) ,(_is_reset)))
#define ath_hal_resetlowesttxrate(_ah)   \
    ((*(_ah)->ah_reset_lowest_txrate)((_ah)))
#define ath_hal_get_perrate_txbfflag(_ah, _txbf_disable_flag) \
	((*(_ah)->ah_get_perrate_txbfflag))((_ah), (_txbf_disable_flag))
#endif

#if UNIFIED_SMARTANTENNA
#define ath_hal_set11n_ratescenario(_ah, _ds, _lastds, _durupdate, _rtsctsrate, _rtsctsduration,    \
        _series, _nseries, _flags, _smart_ant_status, _antenna_array)                                                   \
    ((*(_ah)->ah_set_11n_rate_scenario)(_ah, _ds, _lastds, _durupdate, _rtsctsrate, _rtsctsduration,\
        _series, _nseries, _flags, _smart_ant_status, _antenna_array))
#else
#define ath_hal_set11n_ratescenario(_ah, _ds, _lastds, _durupdate, _rtsctsrate, _rtsctsduration,    \
        _series, _nseries, _flags, _smartAntenna)                                                   \
    ((*(_ah)->ah_set_11n_rate_scenario)(_ah, _ds, _lastds, _durupdate, _rtsctsrate, _rtsctsduration,\
        _series, _nseries, _flags, _smartAntenna))
#endif
#define ath_hal_set11n_aggr_first(_ah, _ds, _aggrlen) \
    ((*(_ah)->ah_set_11n_aggr_first)(_ah, _ds, _aggrlen))
#define ath_hal_set11n_aggr_middle(_ah, _ds, _numdelims) \
    ((*(_ah)->ah_set_11n_aggr_middle)(_ah, _ds, _numdelims))
#define ath_hal_set11n_aggr_last(_ah, _ds) \
    ((*(_ah)->ah_set_11n_aggr_last)(_ah, _ds))
#define ath_hal_clr11n_aggr(_ah, _ds) \
    ((*(_ah)->ah_clr_11n_aggr)(_ah, _ds))
#define ath_hal_set11n_rifs_burst_middle(_ah, _ds) \
    ((*(_ah)->ah_set_11n_rifs_burst_middle)(_ah, _ds))
#define ath_hal_set11n_rifs_burst_last(_ah, _ds) \
    ((*(_ah)->ah_set_11n_rifs_burst_last)(_ah, _ds))
#define ath_hal_clr11n_rifs_burst(_ah, _ds) \
    ((*(_ah)->ah_clr_11n_rifs_burst)(_ah, _ds))
#define ath_hal_set11n_aggr_rifs_burst(_ah, _ds) \
    ((*(_ah)->ah_set_11n_aggr_rifs_burst)(_ah, _ds))
#define ath_hal_set11n_burstduration(_ah, _ds, _burstduration) \
    ((*(_ah)->ah_set_11n_burst_duration)(_ah, _ds, _burstduration))
#define ath_hal_set11n_virtualmorefrag(_ah, _ds, _vmf)  \
    ((*(_ah)->ah_set_11n_virtual_more_frag)(_ah, _ds, _vmf))
#define ath_hal_get11nextbusy(_ah) \
    ((*(_ah)->ah_get_11n_ext_busy)((_ah)))
#if ATH_GEN_RANDOMNESS
#define ath_hal_adc_data_read(_ah, _buf, _size, scrnglast) \
    ((*(_ah)->ah_adc_data_read)(_ah, _buf, _size, scrnglast))
#endif
#define ath_hal_getchbusyper(_ah) \
    ((*(_ah)->ah_get_ch_busy_pct)((_ah)))
#define ath_hal_set11nmac2040(_ah, _mode) \
    ((*(_ah)->ah_set_11n_mac2040)((_ah), (_mode)))
#define ath_hal_get11nRxClear(_ah) \
    ((*(_ah)->ah_get_11n_rx_clear)((_ah)))
#define ath_hal_set11nRxClear(_ah, _rxclear) \
    ((*(_ah)->ah_set_11n_rx_clear)((_ah), (_rxclear)))

#define ath_hal_ppmGetRssiDump(_ah) \
    ((*(_ah)->ah_ppm_get_rssi_dump)((_ah)))
#define ath_hal_ppmArmTrigger(_ah) \
    ((*(_ah)->ah_ppm_arm_trigger)((_ah)))
#define ath_hal_ppmGetTrigger(_ah) \
    ((*(_ah)->ah_ppm_get_trigger)((_ah)))
#define ath_hal_ppmForce(_ah) \
    ((*(_ah)->ah_ppm_force)((_ah)))
#define ath_hal_ppmUnForce(_ah) \
    ((*(_ah)->ah_ppm_un_force)((_ah)))
#define ath_hal_ppmGetForceState(_ah) \
    ((*(_ah)->ah_ppm_get_force_state)((_ah)))

#define ath_hal_getspurInfo(_ah, _en, _ln, _hi) \
    ((*(_ah)->ah_get_spur_info)((_ah), (_en), (_ln), (_hi)))
#define ath_hal_setspurInfo(_ah, _en, _ln, _hi) \
    ((*(_ah)->ah_setSpurInfo)((_ah), (_en), (_ln), (_hi)))

#define ath_hal_get_noisefloor(_ah)\
    ((*(_ah)->ah_ar_get_noise_floor_val)((_ah)))

#define ath_hal_setGreenApPsOnOff(_ah, _rxMask) \
    ((*(_ah)->ah_set_rx_green_ap_ps_on_off)((_ah), (_rxMask)))
#define ath_hal_singleAntPwrSavePossible(_ah) \
    ((*(_ah)->ah_is_single_ant_power_save_possible)((_ah)))


#define ath_hal_getMibCycleCountsPct(_ah, _pRxc, _pRxf, _pTxf) \
    ((*(_ah)->ah_get_mib_cycle_counts_pct)((_ah), (_pRxc), (_pRxf), (_pTxf)))
#define ath_hal_getMibCycleCounts(_ah, _pContext) \
    ((*(_ah)->ah_get_mib_cycle_counts)((_ah), (_pContext)))
#define ath_hal_getVowStats(_ah, _pContext, _flag)              \
    ((*(_ah)->ah_get_vow_stats)((_ah), (_pContext), (_flag)))
#define ath_hal_dmaRegDump(_ah) \
    ((*(_ah)->ah_dma_reg_dump)((_ah)))
#define ath_hal_updateMibMacStats(_ah) \
    ((*(_ah)->ah_update_mib_mac_stats)((_ah)))
#define ath_hal_getMibMacStats(_ah, _pStats) \
    ((*(_ah)->ah_get_mib_mac_stats)((_ah), (_pStats)))
#define ath_hal_clearMibCounters(_ah) \
        ((*(_ah)->ah_clear_mib_counters)((_ah)))
#define ath_hal_getCurRSSI(_ah) \
            ((*(_ah)->ah_get_cur_rssi)((_ah)))
#define ath_hal_get_rssi_chain0(_ah) \
            ((*(_ah)->ah_get_rssi_chain0)((_ah)))
#define ath_hal_set_smartantenna(_ah, _enable, _mode) \
        (((_ah)->ah_setSmartAntenna) ? (*(_ah)->ah_setSmartAntenna)((_ah), (_enable), (_mode)):-1)
#define ath_hal_is_bb_hung(_ah) \
    ((*(_ah)->ah_detect_bb_hang)((_ah)))
#define ath_hal_is_mac_hung(_ah) \
    ((*(_ah)->ah_detect_mac_hang)((_ah)))
#define ath_hal_set_rifs(_ah, _enable) \
    ((*(_ah)->ah_set_11n_rx_rifs)((_ah), (_enable)))
#define ath_hal_hasdynamicsmps(_ah) \
    (ath_hal_getcapability(_ah, HAL_CAP_DYNAMIC_SMPS, 0, NULL) == HAL_OK)
#define ath_hal_set_immunity(_ah, _enable) \
    ((*(_ah)->ah_immunity)((_ah), (_enable)))

#ifdef ATH_BT_COEX
#define ath_hal_hasbtcoex(_ah)                  \
    (ath_hal_getcapability((_ah), HAL_CAP_BT_COEX, 0, NULL) == HAL_OK)
#define ath_hal_bt_setinfo(_ah, _btinfo)        \
    ((*(_ah)->ah_set_bt_coex_info)((_ah), (_btinfo)))
#define ath_hal_bt_config(_ah, _btconf)         \
    ((*(_ah)->ah_bt_coex_config)((_ah), (_btconf)))
#define ath_hal_bt_setqcuthresh(_ah, _qnum) \
    ((*(_ah)->ah_bt_coex_set_qcu_thresh)((_ah), (_qnum)))
#define ath_hal_bt_setweights(_ah, _stomptype)   \
    ((*(_ah)->ah_bt_coex_set_weights)((_ah), (_stomptype)))
#define ath_hal_bt_setbmissthresh(_ah, _tr) \
    ((*(_ah)->ah_bt_coex_set_bmiss_thresh)((_ah), (_tr)))
#define ath_hal_bt_setParameter(_ah, _type, _value) \
    ((*(_ah)->ah_bt_coex_set_parameter)((_ah), (_type), (_value)))
#define ath_hal_bt_disable_coex(_ah)             \
    ((*(_ah)->ah_bt_coex_disable)((_ah)))
#define ath_hal_bt_enable_coex(_ah)              \
    ((*(_ah)->ah_bt_coex_enable)((_ah)))
#define ath_hal_bt_aspm_war(_ah)                  \
    (ath_hal_getcapability((_ah), HAL_CAP_BT_COEX_ASPM_WAR, 0, NULL) == HAL_OK)
#define ath_hal_bt_coexinfo(_ah,reg)               \
    ((*(_ah)->ah_bt_coex_info)((_ah),(reg)))
#define ath_hal_coex_wlan_info(_ah,reg,bOn)               \
    ((*(_ah)->ah_coex_wlan_info)((_ah),(reg),(bOn)))
#endif /* ATH_BT_COEX */

#define ath_hal_hasgentimer(_ah)                \
    (ath_hal_getcapability((_ah), HAL_CAP_GEN_TIMER, 0, NULL) == HAL_OK)
#define ath_hal_gentimer_alloc(_ah, _tsf)             \
    ((*(_ah)->ah_gentimer_alloc)((_ah), (_tsf)))
#define ath_hal_gentimer_free(_ah, _idx)        \
    ((*(_ah)->ah_gentimer_free)((_ah), (_idx)))
#define ath_hal_gentimer_start(_ah, _idx,         \
    _timer_next, _timer_period)                 \
    ((*(_ah)->ah_gentimer_start)((_ah), (_idx),    \
    (_timer_next), (_timer_period)))
#define ath_hal_gentimer_stop(_ah, _idx)         \
    ((*(_ah)->ah_gentimer_stop)((_ah), (_idx)))
#define ath_hal_gentimer_getintr(_ah, _trig, _thresh)   \
    ((*(_ah)->ah_gentimer_get_intr)((_ah), (_trig), (_thresh)))

#define ath_hal_getmfpsupport(_ah, _ri)  \
    ath_hal_getcapability(_ah, HAL_CAP_MFP, 0, (void *)(_ri))

#define ath_hal_promisc_mode(_ah, _ena) \
	(*(_ah)->ah_promisc_mode)((_ah), (_ena))
#define ath_hal_read_pktlog_reg(_ah, v0, v1, v2, v3) \
	((*(_ah)->ah_read_pktlog_reg)((_ah), (v0), (v1), (v2), (v3)))
#define ath_hal_write_pktlog_reg(_ah, en, v0, v1, v2, v3) \
		((*(_ah)->ah_write_pktlog_reg)((_ah), (en), (v0), (v1), (v2), (v3)))
#define ath_hal_fill_radiotap_hdr(_ah, _rh, _ppi, _ds, _buf) \
	((*(_ah)->ah_fill_radiotap_hdr)((_ah), (_rh), (_ppi), (_ds), (_buf)))
#if ATH_TRAFFIC_FAST_RECOVER
#define ath_hal_get_pll3_sqsum_dvc(_ah) \
    ((*(_ah)->ah_get_pll3_sqsum_dvc)((_ah)))
#endif /* ATH_TRAFFIC_FAST_RECOVER */
#define ath_hal_set_proxysta(_ah, _en) \
	((*(_ah)->ah_set_proxy_sta)((_ah), (_en)))

#define ath_hal_get_cal_intervals(_ah, _p, _query) \
	((*(_ah)->ah_get_cal_intervals))((_ah), (_p), (_query))

#define ath_hal_get_bbpanic_info(_ah, _panic) \
    ((*(_ah)->ah_get_bb_panic_info)((_ah), (_panic)))

#define ath_hal_handle_radar_bbpanic(_ah) \
    ((*(_ah)->ah_handle_radar_bb_panic)((_ah)))

#define ath_hal_set_halreset_reason(_ah, _reason) \
    ((*(_ah)->ah_set_hal_reset_reason)((_ah), (_reason)))

#define ath_hal_clear_halreset_reason(_ah) \
    ((*(_ah)->ah_set_hal_reset_reason)((_ah), HAL_RESET_NONE))

#define ath_hal_isOpenLoopPwrCntrl(_ah)  \
    (ath_hal_getcapability(_ah, HAL_CAP_OL_PWRCTRL, 0, NULL) == HAL_OK)

#define ath_hal_is_crdc(_ah)  \
    (ath_hal_getcapability(_ah, HAL_CAP_CRDC, 0, NULL) == HAL_OK)

#define ath_hal_olpcTempCompensation(_ah) \
    ((*(_ah)->ah_olpc_temp_compensation)((_ah)))

#define ath_hal_chain_rssi_diff_compensation(_ah) \
    ((*(_ah)->ah_chain_rssi_diff_compensation)((_ah)))

#define ath_hal_getAntDivCombConf(_ah, _conf) \
    ((*(_ah)->ah_get_ant_dvi_comb_conf)((_ah), (_conf)))

#define ath_hal_setAntDivCombConf(_ah, _conf) \
    ((*(_ah)->ah_set_ant_dvi_comb_conf)((_ah), (_conf)))

#define ath_hal_chk_rssi(_ah, _rssi)    \
    ((*(_ah)->ah_ChkRssiUpdateTxPwr)((_ah), (_rssi)))

#define ath_hal_is_skip_paprd_by_greentx(_ah)    \
    ((*(_ah)->ah_is_skip_paprd_by_greentx)((_ah)))

#define ath_hal_hwgreentx_set_pal_spare(_ah, _val)    \
    ((*(_ah)->ah_hwgreentx_set_pal_spare)((_ah), (_val)))

#define ath_hal_is_ani_noise_spur(_ah)   \
  ((*(_ah)->ah_is_ani_noise_spur)((_ah)))

#define ath_hal_reset_hw_beacon_proc_crc(_ah)      \
    ((*(_ah)->ah_reset_hw_beacon_proc_crc)((_ah)))

#define ath_hal_get_hw_beacon_rssi(_ah)    \
    ((*(_ah)->ah_get_hw_beacon_rssi)((_ah)))

#define ath_hal_set_hw_beacon_rssi_threshold(_ah, _rssi_threshold)      \
    ((*(_ah)->ah_set_hw_beacon_rssi_threshold)((_ah), (_rssi_threshold)))

#define ath_hal_reset_hw_beacon_rssi(_ah)    \
    ((*(_ah)->ah_reset_hw_beacon_rssi)((_ah)))

#define ath_hal_set_hw_beacon_proc(_ah, _on)      \
    ((*(_ah)->ah_set_hw_beacon_proc)((_ah), (_on)))

#define ath_hal_get_smart_ant_tx_info(_ah, _ds, _rate_array, _antenna_array)  \
    ((*(_ah)->ah_get_smart_ant_tx_info)((_ah), (_ds), (_rate_array), (_antenna_array)))

#define ath_hal_is_ani_noise_spur(_ah)   \
  ((*(_ah)->ah_is_ani_noise_spur)((_ah)))

#define ath_hal_interferenceIsPresent(_ah) \
    ((*(_ah)->ah_interference_is_present)((_ah)))

#define ath_hal_get_tpc_tables(_ah, _ratecount) \
    ((*(_ah)->ah_get_tpc_tables)((_ah), (_ratecount)))

#if ATH_TxBF_DYNAMIC_LOF_ON_N_CHAIN_MASK
#define ath_hal_txbf_loforceon_update(_ah, _on)      \
    ((*(_ah)->ah_txbf_loforceon_update)((_ah), (_on)))
#endif
#define ath_hal_disable_interrupts_on_exit(_ah)      \
    ((*(_ah)->ah_disable_interrupts_on_exit) ? (*(_ah)->ah_disable_interrupts_on_exit)((_ah)) : (void)0)

#ifdef ATH_TX99_DIAG
/* Tx99 functions */
#define ath_hal_tx99_tx_stopdma(_ah, _hwq_num) \
    ((*(_ah)->ah_tx99txstopdma)((_ah), (_hwq_num)))
#define ath_hal_tx99_drain_alltxq(_ah) \
    ((*(_ah)->ah_tx99drainalltxq)((_ah)))
#define ath_hal_tx99_channel_pwr_update(_ah, _c, _txpower) \
    ((*(_ah)->ah_tx99channelpwrupdate)((_ah), (_c), (_txpower)))
#define ah_tx99_start(_ah, _data) \
    ((*(_ah)->ah_tx99start)((_ah), (_data)))
#define ah_tx99_stop(_ah) \
    ((*(_ah)->ah_tx99stop)((_ah)))
#define ah_tx99_chainmsk_setup(_ah, _c) \
    ((*(_ah)->ah_tx99_chainmsk_setup)((_ah), (_c)))
#define ah_tx99_set_single_carrier(_ah, _c, _type) \
    ((*(_ah)->ah_tx99_set_single_carrier)((_ah), (_c), (_type)))
#endif

#define ath_hal_disablePhyRestart(_ah, _val)    \
    ((*(_ah)->ah_disable_phy_restart)((_ah), (_val)))
#define ath_hal_mat_enable(_ah, _val) \
    ((*(_ah)->ah_mat_enable)((_ah), (_val)))
#define ath_hal_dump_keycache(_ah, n, entry) \
    ((*(_ah)->ah_dump_keycache)((_ah), n, entry))

#if ATH_SUPPORT_DYN_TX_CHAINMASK
#define ath_hal_dyn_txchainmask_support(_ah, _dtcs) \
        (ath_hal_getcapability(_ah, HAL_CAP_DYN_TX_CHAINMASK, 0, _dtcs) == HAL_OK)
#endif /* ATH_SUPPORT_DYN_TX_CHAINMASK */

#if ATH_RESET_SERIAL
#if DBG
#define SC_DPC_PROCESSING_INC(_sc_dpc_processing) \
    printk("%s(%d) dpc count:%d\n", __func__, __LINE__,atomic_inc(_sc_dpc_processing))

#define SC_DPC_PROCESSING_DEC(_sc_dpc_processing) \
    printk("%s(%d) dpc count:%d\n", __func__, __LINE__,atomic_dec(_sc_dpc_processing))
#else
#define SC_DPC_PROCESSING_INC(_sc_dpc_processing) \
    atomic_inc(_sc_dpc_processing)

#define SC_DPC_PROCESSING_DEC(_sc_dpc_processing) \
    atomic_dec(_sc_dpc_processing))
#endif
#endif
#define ATH_MULT_30_DIV_13(x) (((x) * 591L) >> 8) /* 2.3086 vs. 2.3077 */
#define ATH_MULT_27_DIV_13(x) (((x) * 133L) >> 6) /* 2.0781 vs. 2.0769 */
#define ATH_MULT_10_DIV_9(x)  (((x) * 71L) >> 6)  /* 1.1093 vs. 1.1111 */

#if ATH_SUPPORT_MCI
#define ath_hal_mci_setup(_ah, _gpmAddr, _gpmBuf, _len, _schedAddr) \
    ((*(_ah)->ah_mci_setup)((_ah), (_gpmAddr), (_gpmBuf), (_len), (_schedAddr)))
#define ath_hal_mci_send_message(_ah, _header, _flag, _payload, _len, _wait_done, _check_bt) \
    ((*(_ah)->ah_mci_send_message)((_ah), (_header), (_flag), \
    (_payload), (_len), (_wait_done), (_check_bt)))
#define ath_hal_mci_get_int(_ah, _int, _intRxMsg) \
    ((*(_ah)->ah_mci_get_interrupt)((_ah), (_int), (_intRxMsg)))
#define ath_hal_mci_state(_ah, _type, _p_data) \
    ((*(_ah)->ah_mci_state)((_ah), (_type), (_p_data)))
#define ath_hal_hasmci(_ah)                  \
    (ath_hal_getcapability((_ah), HAL_CAP_MCI, 0, NULL) == HAL_OK)
#define ath_hal_mci_detach(_ah)              \
    ((*(_ah)->ah_mci_detach)((_ah)))
#endif
#define ath_hal_cac_tx_quiet(_ah, _ena)   \
    ((*(_ah)->ah_cac_tx_quiet)((_ah), (_ena)))

#define ath_hal_set_ctl_pwr(_ah, _ctl_array) \
    ((*(_ah)->ah_set_ctl_pwr)((_ah), (_ctl_array)))

#define ath_hal_set_txchainmaskopt(_ah, _mask) \
    ((*(_ah)->ah_set_txchainmaskopt)((_ah), (_mask)))
#define ath_hal_reset_nav(_ah) \
    ((*(_ah)->ah_reset_nav)((_ah)))

#if ATH_BT_COEX_3WIRE_MODE
#define ath_hal_enable_basic_3wire_btcoex(_ah) \
    ((*(_ah)->ah_enable_basic_3wire_btcoex)((_ah)))
#define ath_hal_disable_basic_3wire_btcoex(_ah) \
    ((*(_ah)->ah_disable_basic_3wire_btcoex)((_ah)))
#endif

#define ath_hal_get_ah_devid(_ah)   \
    ((*(_ah)->ah_hal_get_ah_devid)((_ah)))

#define ath_hal_get_chip_min_and_max_powers(_ah, _max_tx_power, _min_tx_power) \
    ((*(_ah)->ah_get_chip_min_and_max_powers)((_ah), (_max_tx_power), (_min_tx_power)))

#define ath_hal_fill_hal_chans_from_regdb(_ah, _freq, _maxregpower, \
        _maxpower, _minpower, _flags, _index)\
    ((*(_ah)->ah_fill_hal_chans_from_regdb)((_ah), (_freq), \
        (_maxregpower), (_maxpower), (_minpower), (_flags), (_index)))

#define ath_hal_set_sc(_ah, _sc)   \
    ((*(_ah)->ah_hal_set_sc)((_ah), (_sc)))

#if ATH_ANT_DIV_COMB
void ath_smartant_normal_scan_handle(ath_dev_t dev, u_int8_t scan_start);
#endif

#define SMARTANT_INVALID 0xffffffff

#if ATH_GEN_RANDOMNESS
#define ATH_RSSI_CONST_COUNT_LIMIT 5
void ath_gen_randomness(struct ath_softc *sc);
#endif
void ath_tid_cqw(struct ath_softc *sc, struct ath_atx_tid *tid);
/*
 * FIXME: This inline function should actually be in ath_ht.h, but due to
 * inter-dependencies between these 2 header files, it is here.
 *
 * queue up a dest/ac pair for tx scheduling
 * NB: must be called with txq lock held
 */
static INLINE void
    ath_tx_queue_tid(struct ath_txq *txq, struct ath_atx_tid *tid)
{
    struct ath_atx_ac *ac = tid->ac;

    /*
     * if tid is paused, hold off
     */
#ifdef ATH_SWRETRY
    /* When sw retry is enabled, we still schedule the paused tid of ps node upon
     * PS-Poll reception.
     */

    if (qdf_atomic_read(&tid->paused) && !atomic_read(&tid->an->an_pspoll_pending))
#else
        if (qdf_atomic_read(&tid->paused))
#endif
            return;

    /*
     * add tid to ac atmost once
     */
    if (tid->sched)
        return;

    tid->sched = AH_TRUE;
    TAILQ_INSERT_TAIL(&ac->tid_q, tid, tid_qelem);

    /*
     * add node ac to txq atmost once
     */
    if (ac->sched)
        return;

    ac->sched = AH_TRUE;
    TAILQ_INSERT_TAIL(&txq->axq_acq, ac, ac_qelem);
}

static INLINE void
ath_tx_set_retry(struct ath_softc *sc, struct ath_buf *bf)
{
    wbuf_t wbuf;
    struct ieee80211_frame *wh;

    __11nstats(sc, tx_retries);

    bf->bf_isretried = 1;
    bf->bf_retries ++;

    wbuf = bf->bf_mpdu;
    wh = (struct ieee80211_frame *)wbuf_header(wbuf);
    wh->i_fc[1] |= IEEE80211_FC1_RETRY;
}

#define SA_TRAIN_DATA_THRESHOLD 700  /* Min number of packets for collecting training stats*/
#define SA_TRAIN_MIN_PKTS 32 /* self packet generation check threshold */
#define SA_TRAIN_PER_THRESHOLD 75  /* Min accepted PER for continuing current training */
#define SA_MAX_TRAIN_ATTEMPTS 3  /* Max retry attmepts as training packet*/
#define MULTICAST_DROP_THRESHOLD (ATH_TXBUF/10) /* Set 10% of the ATH_TXBUF */

#define ATH_NODE_ATHEROS(_an)         (_an)->an_rc_node


static inline u_int32_t
__ath_buf_get_ba_period(struct ath_softc *sc, struct ath_buf *bf)
{
    u_int32_t    ba_period;
    u_int8_t    rix, cix;
    const HAL_RATE_TABLE *rt;

    rt = sc->sc_currates;

    /*
     * The second aggregate has only one rate, so use it to get
     * the control rate.
     */
    rix = bf->bf_rcs[0].rix;
    cix = rt->info[rix].controlRate;

    switch (rt->info[cix].rate_code) {
    default:
    case 0x0b: /* 6 Mb OFDM */
        ba_period = 68;
        break;
    case 0x0a: /* 12 Mb OFDM */
        ba_period = 44;
        break;
    case 0x09: /* 24 Mb OFDM */
        ba_period = 32;
        break;
    }

    return (ba_period);
}
#if defined(QCA_PARTNER_PLATFORM) && !(QCA_PARTNER_DIRECTLINK_RX || QCA_PARTNER_CBM_DIRECTPATH)
static u_int32_t
#else
static INLINE u_int32_t
#endif
ath_buf_get_ba_period(struct ath_softc *sc, struct ath_buf *bf)
{
    return __ath_buf_get_ba_period(sc, bf);
}

static inline u_int32_t
ath_get_ratekbps_from_ratecode(struct ath_softc *sc, u_int8_t ratecode, u_int8_t rateflags)
{
    u_int32_t ratekbps = sc->sc_hwmap[ratecode].rateKbps;
    if (IS_HT_RATE(ratecode)) {
        if (rateflags & ATH_RC_CW40_FLAG) {
            ratekbps = (ratekbps * 27) / 13; // HT20 rate to HT40 rate for same MCS
        }
        if (rateflags & ATH_RC_SGI_FLAG) {
            ratekbps = (ratekbps * 10) / 9; // LGI rate to SGI rate for same MCS
        }
    }
    return ratekbps;
}

#ifdef WLAN_SUPPORT_GREEN_AP
bool ath_green_ap_is_powersave_on(struct ath_softc *sc);
bool ath_green_ap_get_enable_print(struct ath_softc *sc);
u_int8_t ath_green_ap_get_enable(struct ath_softc *sc);
bool ath_green_ap_set_enable(struct ath_softc *sc, uint32_t value);
void ath_green_ap_suspend(struct ath_softc *sc);
#endif /* WLAN_SUPPORT_GREEN_AP */
#endif /* ATH_INTERNAL_H */

