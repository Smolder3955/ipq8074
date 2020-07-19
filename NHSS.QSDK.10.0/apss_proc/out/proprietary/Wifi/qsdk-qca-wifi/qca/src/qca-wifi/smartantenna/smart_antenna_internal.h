/*
* Copyright (c) 2013 Qualcomm Atheros, Inc..
* All Rights Reserved.
* Qualcomm Atheros Confidential and Proprietary.
*/
#ifndef __SMART_ANT_INTERNAL__
#define __SMART_ANT_INTERNAL__

#include <linux/jiffies.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include "ieee80211_smart_ant_api.h"
#include <qdf_mem.h>
#include <qdf_types.h>
#include <qdf_trace.h>

/* OS dependent defines */
#define OS_ASSERT(exp) do {    \
    if (unlikely(!(exp))) {    \
        BUG();                \
    }                        \
} while (0)

static inline void OS_MEMCPY(void *dst, const void *src, size_t size)
{
    memcpy(dst, src, size);
}

/* zero a memory buffer */
static inline void OS_MEMZERO(void *buf, size_t size)
{
    memset(buf, 0, size);
}



/* Time related macros */
#define    A_MS_TICKGET()    ((uint32_t) CONVERT_SYSTEM_TIME_TO_MS(jiffies))
#define CONVERT_SYSTEM_TIME_TO_MS(_t)        jiffies_to_msecs(_t)




/* State Variable */
#define SMARTANTENNA_PRETRAIN           0 /* state where pretraining will happen */
#define SMARTANTENNA_TRAIN_INPROGRESS   1 /* state where training is in progress currently */
#define SMARTANTENNA_TRAIN_HOLD         2 /* state where stats are processed in training */
#define SMARTANTENNA_DEFAULT            3 /* default state where nothing will happen */


#define SA_MAX_RADIOS 3
#define SA_MAX_RSSI_SAMPLES 10 /* MAX RSSI sample collected from training stats */
#define SA_MAX_RECV_CHAINS 4   /* MAX receive chains */
#define SA_ANTENNA_COMBINATIONS 16
#define SA_PARALLEL_MODE 1

#define SA_DEFAULT_RETRAIN_INTERVAL 2 /* default retrain interval,2 - minutes*/
#define SA_NUM_MILLISECONDS_IN_MINUTE 60000 /* number of milli seconds in a minute */
#define SA_DEFAULT_TP_THRESHOLD 10 /* default throughput threshold in percentage */
#define MIN_GOODPUT_THRESHOLD 6 /* Minimum good put drop to tigger retraining
                                 * For most of rates which are below 50 Mbps, the difference between consecutive rates is 6.5 Mbps.
                                 */
#define SA_PERF_TRAIN_INTERVEL 2000  /* 1 sec */
//#define RETRAIN_MISS_THRESHOLD 20  /* Max number of intervals for periodic training */
#define SA_MAX_NOTRAFFIC_TIME 10 /* max number of perf slots for which no traffic can be there */
#define SA_GPUT_IGNORE_INTERVAL 1  /* default gudput ignoring interval */
#define SA_GPUT_AVERAGE_INTERVAL 2  /* default gudput averaging interval */

#define MAX_PRETRAIN_PACKETS 600 /* MAX Number of packets to be generated */
#define SA_NUM_MAX_PPDU 50     /* default value of max number of ppdu's for training */
#define SA_PERDIFF_THRESHOLD 3 /* PER diff threshold to apply secondary metric */
#define SA_PER_UPPER_BOUND 80  /* Upper bound to move to next lower rate */
#define SA_PER_LOWER_BOUND 20  /* Lower bound to move to next higher rate */
#define SA_DEFALUT_HYSTERISYS 3 /* 3 intervals */
#define SA_MAX_PRD_RETRAIN_MISS 10 /* Upper bound for periodic retrain miss in case of station */
#define SA_N_BW_PPDU_THRESHOLD 64 /* Default threshold for checking BW change */
#define SA_DEFAULT_PKT_LEN 1536 /* default packet length */
#define SA_DEFAULT_MIN_TP  344      /* default minimum throughput to consider as traffic (86 -- 2 mbps)*/
#define SA_CTS_PROT 0x10000   /* indication to use self cts */

//#define MAX_RATES_PERSET (4+8+24)    /* max rates 4 11b rates,8 11g/a rates,24 11n rates for 3 stream */
#define SA_MAX_HT_RATES 24
#define SA_MAX_VHT_RATES 40
#define NUM_TRAIN_PKTS 32            /* number of training packets required to be self generated in an interval */
#define SA_NUM_LEGACY_TRAIN_PKTS 200 /* Number of packets required for legacy rates */
#define SA_NUM_DEFAULT_TRAIN_PKTS 640 /* Default number of packets required for ht,vht rates */
#define SA_MAX_INTENSETRAIN_PKTS 1000 /* maximum packets that can be used in case of intense training */
#define SA_DEFAULT_ANTENNA 5  /* Default antenna for RX and TX. VHVH polarization */
#define SA_STATUS_SUCCESS 0
#define SA_STATUS_FAILURE -1

#define SA_MAX_BW 3 /* 20,40 and 80 */
#define SA_ANTENNAS_PER_CHAIN 2

#define SA_MAX_PERCENTAGE 100

#define SA_FEEDBACK_RATE_POSITION 8
#define SA_GET_RATE_FEEDBACK(rate_word,bw) ((rate_word) >> (bw * SA_FEEDBACK_RATE_POSITION))
#define SA_GET_BW_FEEDBACK(rate_index) (rate_index & 3)
#define SA_GET_SERIES_FEEDBACK(rate_index) ((rate_index >> 2) & 0x1)
#define SA_GET_BW_COMB_FEEDBACK(bw_nfb) ((bw_nfb >> 4)& 0xF)
#define SA_GET_NFB_COMB_FEEDBACK(bw_nfb) (bw_nfb & 0xF)

#define SA_11B_MODE 0x40
#define SA_11G_MODE 0x00
#define SA_MODE_BITS 0xF0
#define IS_SA_11B_RATE(rate) ((rate & SA_MODE_BITS) == SA_11B_MODE)
#define IS_SA_11G_RATE(rate) ((rate & SA_MODE_BITS) == SA_11G_MODE)

#define SA_STAT_INDEX_11B(rate) (rate & 0x0f)
#define SA_STAT_INDEX_11G(rate) ((rate & 0x0f) + 4)
#define SA_GET_NSS_FROM_RATE(rate) ((rate >> 4) & 0x03)
#define SA_GET_MCS_FROM_RATE(rate) (rate & 0x0f)
#define SA_STAT_INDEX_11N_11AC(nss,mcs) ((nss * 10) + mcs)
#define SA_MAX_MCS_HT 8
#define SA_MAX_MCS_VHT 10
#define SA_RATE_INDEX_11N(rate) ((SA_GET_NSS_FROM_RATE(rate) * SA_MAX_MCS_HT) + SA_GET_MCS_FROM_RATE(rate))
#define SA_RATE_INDEX_11AC(rate) ((SA_GET_NSS_FROM_RATE(rate) * SA_MAX_MCS_VHT) + SA_GET_MCS_FROM_RATE(rate))

#define SA_RATEMASK_20 0x000000FF /* rate code will be in last bit0 - bit7 */
#define SA_RATEMASK_40 0x0000FF00 /* rate code will be in last bit8 - bit15 */
#define SA_RATEMASK_80 0x00FF0000 /* rate code will be in last bit16 - bit23 */
#define SA_BYTE_MASK 0xFF

#define SA_RATEPOSITION_20 0     /* rate code will be in last bit0 - bit7 */
#define SA_RATEPOSITION_40 8     /* rate code will be in last bit8 - bit15 */
#define SA_RATEPOSITION_80 16    /* rate code will be in last bit16 - bit23 */


#define SA_LB_POSITION 0     /* will be in last bit0 - bit7 */
#define SA_UB_POSITION 8     /* will be in last bit8 - bit15 */
#define SA_PER_THRESHOLD_POSITION 16    /* will be in last bit16 - bit23 */
#define SA_CONFIG_POSITION 24

#define SA_MASK_TXCHAIN 0x0f
#define SA_MASK_RXCHAIN 0xf0

#define SA_NIBBLE_MASK 0xF
#define SA_RADIOID_OFFSET 4
#define SA_GET_RADIO_ID(radio_id) ((radio_id >> SA_RADIOID_OFFSET) & SA_NIBBLE_MASK)

#define SA_DEBUG 1
#define not_yet 0
#define SA_DEBUG_LVL1 0x1
#define SA_DEBUG_LVL2 0x2
#define SA_DEBUG_LVL3 0x4
#define SA_DEBUG_LVL4 0x8
#define SA_DEBUG_LVL5 0x10
#if 1
#define SA_DEBUG_MASK(mask,radio_id)  (g_sa.radio[radio_id].sa_dbg_level & mask)
#define SA_DEBUG_PRINT(mask,radio_id, args...) do { \
    if (SA_DEBUG_MASK(mask,radio_id)) {  \
        qdf_print(args);    \
    }                    \
} while (0)
#else
#define SA_DEBUG_MASK(mask,radio)
#define SA_DEBUG_PRINT(mask,radio_id, args...)
#endif

enum {
    SA_RATECOUNT_IDX_LEGACY=0,
    SA_RATECOUNT_IDX_20,
    SA_RATECOUNT_IDX_40,
    SA_RATECOUNT_IDX_80
};

enum {
    SA_BW_20=0,
    SA_BW_40,
    SA_BW_80,
    SA_BW_160
};

enum {
    SA_OPMODE_LEGACY=0,
    SA_OPMODE_HT,
    SA_OPMODE_VHT
};

enum {
    SA_TRAINING_MODE_EXISTING=0,
    SA_TRAINING_MODE_MIXED=1
};

enum {
    SA_NUM_PKT_THRESHOLD_20 = 20,
    SA_NUM_PKT_THRESHOLD_40 = 10,
    SA_NUM_PKT_THRESHOLD_80 = 5
};

enum {
    NEG_TRIGGER = -1,
    POS_TRIGGER = 1,
    INIT_TRIGGER = 0
};

/* config params */
enum  {
    SMART_ANT_PARAM_HELP = 0,   /* Display current available commands list */
    SMART_ANT_PARAM_TRAIN_MODE ,    /* smart antenna training mode implicit or explicit*/
    SMART_ANT_PARAM_TRAIN_PER_THRESHOLD ,    /* smart antenna lower, upper and per diff thresholds */
    SMART_ANT_PARAM_PKT_LEN      ,    /* packet length of the training packet */
    SMART_ANT_PARAM_NUM_PKTS     ,    /* number of packets need to send for training */
    SMART_ANT_PARAM_TRAIN_START  ,    /* start smart antenna training */
    SMART_ANT_PARAM_TRAFFIC_GEN_TIMER,/* Self packet generation timer value configuration */
    SMART_ANT_PARAM_TRAIN_ENABLE,         /* Smart antenna training (initial,periodic,performence) enable/disable */
    SMART_ANT_PARAM_PERFTRAIN_INTERVAL,   /* performnce monitoring interval */
    SMART_ANT_PARAM_RETRAIN_INTERVAL,    /* periodic retrain interval */
    SMART_ANT_PARAM_PERFTRAIN_THRESHOLD,    /* % change in goodput to tigger performance training */
    SMART_ANT_PARAM_MIN_GOODPUT_THRESHOLD, /* Minimum Good put threshold to tigger performance training */
    SMART_ANT_PARAM_GOODPUT_AVG_INTERVAL, /* Number of intervals Good put need to be averaged to use in performance training tigger */
    SMART_ANT_PARAM_DEFAULT_ANTENNA , /* default antenna for RX */
    SMART_ANT_PARAM_DEFAULT_TX_ANTENNA, /* default tx antenna when node connects */
    SMART_ANT_PARAM_TX_ANTENNA ,      /* current smart antenna used for TX */
    SMART_ANT_PARAM_DBG_LEVEL ,       /* Debug level for each radio */
    SMART_ANT_PARAM_PRETRAIN_PKTS,    /* number of pretrain packets */
    SMART_ANT_PARAM_OTHER_BW_PKTS_TH, /* threshold for other bw packets in training */
    SMART_ANT_PARAM_GOODPUT_IGNORE_INTERVAL,/* Number of intervals Good put need to be ignored in performance training tigger */
    SMART_ANT_PARAM_MIN_PKT_TH_BW20,  /* minimum number of pckets to check if bw 20 is active */
    SMART_ANT_PARAM_MIN_PKT_TH_BW40,  /* minimum number of pckets to check if bw 20 is active */
    SMART_ANT_PARAM_MIN_PKT_TH_BW80,  /* minimum number of pckets to check if bw 20 is active */
    SMART_ANT_PARAM_DEBUG_INFO,       /* To display debug info */
    SMART_ANT_PARAM_MAX_TRAIN_PPDU,   /* max number of train ppdu's */
    SMART_ANT_PARAM_PERF_HYSTERESIS,  /* hysteresis for performence based trigger */
    SMART_ANT_PARAM_TX_FEEDBACK_CONFIG,  /* configure target to send 1 tx feedback to host for N ppdus sent */
    SMART_ANT_PARAM_BCN_ANTENNA,  /* Beacon Antenna value */
    SMART_ANT_PARAM_TRAIN_RATE_TESTMODE, /* Train rate in test mode */
    SMART_ANT_PARAM_TRAIN_ANTENNA_TESTMODE, /* Train antenna in test mode */
    SMART_ANT_PARAM_TRAIN_PACKETS_TETSMODE, /* Number of train packets in test mode */
    SMART_ANT_PARAM_TRAIN_START_TETSMODE,   /* Start training in test mode */
};

struct smartantenna_params {
    u_int8_t training_mode;      /* use existing traffic alone - 0
                                    use mixed (existing/proprietary) traffic - 1*/
    u_int8_t lower_bound;        /* lower bound of rate drop in training  */
    u_int8_t upper_bound;        /*  upper bound of rate increase in training */
    u_int8_t per_diff_threshold; /* per diff that treated as equal */
    u_int16_t n_packets_to_train;    /* number of packets used for training */
    u_int16_t packetlen;         /* packet lenght of propritory generated training packet */
    u_int8_t num_tx_antennas;    /* possible tx antenna combinations */
    u_int8_t num_recv_chains;    /* rx antenna chains */
    u_int16_t num_pkt_threshold; /* throshold for minimum traffic to consider */
    u_int32_t retrain_interval;  /* periodic timer value */
    u_int32_t perftrain_interval;/* performence monitor timer value */
    u_int8_t max_throughput_change;/* threshold in throghput fluctuation for performence based trigger */
    u_int8_t hysteresis;         /* hsteresis value for throughput fluctuations */
    u_int8_t min_goodput_threshold; /* Minimum good put drop to tigger retraining */
    u_int8_t goodput_avg_interval;    /* goodput averaging interval */
    u_int8_t goodput_ignore_interval; /* goodput ignoing interval */
#define SA_CONFIG_INTENSETRAIN 0x1     /* setting this bit in config indicates training with double number of packets */
#define SA_CONFIG_EXTRATRAIN   0x2     /* setting this bit in config indicates to do extra traing in case of conflits in first metric */
#define SA_CONFIG_SLECTSPROTEXTRA 0x4  /* setting this bit in config indicates to protect extra training frames with self CTS */
#define SA_CONFIG_SLECTSPROTALL   0x8  /* setting this bit in config indicates to protect all training frames with self CTS */
    u_int8_t config;                   /* contains configuration for SA algorithm */
    u_int16_t num_pretrain_packets;    /* num of pre training packets */
    u_int16_t num_other_bw_pkts_th;    /* threshold for number of packets that can be sent in other bw */
    u_int8_t  train_enable;            /* Master bitmap indicating train enable for init,perf,prd triggers */
    u_int16_t num_min_pkt_threshod[SA_MAX_BW]; /* Num min packet to find bandwidth active */
    u_int32_t default_tx_antenna; /* default tx antenna when node connects */
    u_int8_t antenna_change_ind;  /* global indicate to change antenna in next feedback */
    u_int16_t max_train_ppdu;     /* max number of train ppdus */
};

struct radio_config {
   struct sa_config init_config;
   struct smartantenna_params sa_params;
   u_int32_t default_antenna;
   u_int32_t bcn_antenna;
   u_int8_t node_count_per_antenna[SA_ANTENNA_COMBINATIONS];
   u_int16_t nodes_connected;

   uint8_t init_done;
   uint8_t sa_dbg_level;
   uint8_t radiotype;
   /* Number of antenna combinations: ??? get from Makefile */
};

struct sa_global {
   struct radio_config radio[SA_MAX_RADIOS];
};

struct sa_node_rate_stats {
    uint32_t npackets_legacy[MAX_CCK_OFDM_RATES];
    uint32_t npackets[SA_MAX_BW][MAX_MCS_RATES];
    uint32_t nppdu_bw[SA_MAX_BW];
    uint32_t nppdu_gp[SA_MAX_BW];
    uint32_t ins_goodput[SA_MAX_BW];
    uint32_t rate_mcs; /* last packet rates */
    uint32_t rate_maxphy;
};

struct node_train_stats {
    u_int32_t rate;         /* rate code used for training */
    u_int32_t antenna;      /* training antenna */
    u_int16_t nbad;         /* falied frames for current training rate and antenna */
    u_int16_t nframes;      /* total frames sent for current training rate and antenna */
    u_int8_t per;           /* training PER */
    u_int8_t bw;            /* training bw */
    uint8_t rssi[SA_MAX_RECV_CHAINS][SA_MAX_RSSI_SAMPLES];   /* Block-ACK/ACK RSSI for all chains */

    u_int8_t nextant_per;   /* training PER */
    u_int16_t nextant_nbad; /* falied frames for current training next rate and antenna */
    u_int16_t nextant_nframes; /* total frames sent for current training next rate and antenna */
    int8_t nextant_rssi[SA_MAX_RECV_CHAINS][SA_MAX_RSSI_SAMPLES];   /* next antennas Block-ACK/ACK RSSI */

    u_int8_t bw_change;    /* bandwidth changed */
    u_int8_t first_per;    /* flag for storing firt PER */
    u_int32_t last_rate;  /* previous train rate index  */
    int8_t    last_train_dir;    /* last trained index ; 1 - above the rate, -1 - below the rate */
    u_int32_t ant_map[SA_ANTENNA_COMBINATIONS];    /* Antenna combinations index map */
    u_int32_t skipmask;

    u_int8_t extra_stats;   /* flag to collect extra stats */
    u_int8_t stats_dir;     /* direction of PER for extra stats */
    u_int8_t double_train;  /* flag to traing with double packets */
    u_int8_t extra_sel;     /* flag indicating collection of extra stats for selected antenna */
    u_int8_t extra_cmp;     /* flag indicating collection of extra stats for comparing antenna */
    u_int16_t extra_nbad;   /* falied frames for current training rate and antenna when collecting extra stats */
    u_int16_t extra_nframes; /* falied frames for current training rate and antenna when collecting extra stats */
};

struct sa_perf_info {
    int8_t  trigger_type; /* +ve trigger or -ve trigger */
    u_int8_t hysteresis;    /* hysteresis for throughput drop */
    int8_t   gput_avg_interval; /* number of intervals to ignore goodput for retraining*/
    int8_t   idle_time;
    uint32_t avg_goodput; /* average good put for previous intervel */
};

struct sa_node_train_info {
    u_int8_t  is_training; /* current training status. Setting this indicates training is happenning */
#define SA_INIT_TRAIN_EN    0x1
#define SA_PERIOD_TRAIN_EN  0x2
#define SA_PERF_TRAIN_EN    0x4
#define SA_RX_TRAIN_EN 0x10
    u_int8_t  train_enable; /* BITMAP indicating whcin training is enabled */
    u_int8_t  smartantenna_state; /* node specific smart antenna state */
    u_int8_t  intense_train; /* setting this indicates to use more packets for training */
    u_int32_t selected_antenna; /* currently used tx antenna */
    u_int32_t previous_selected_antenna; /* previous used tx antenna */
    u_int32_t train_rate; /* rate index of current used rate */
    u_int32_t prev_ratemax ; /* previous rate */
    u_int32_t txed_packets; /* tx unicast packets trasmited in retraining intervel */

    u_int8_t  last_train_bw; /* last train band width */
    u_int8_t  retrain_miss;  /* Number of consecutive retrain intervels with out doing retraining*/
    u_int8_t  long_retrain_count;  /* counter for long retrain */
    u_int8_t  traffic_none;  /* Number of consecutive retrain intervels with out any traffic*/

    struct sa_perf_info perf_info[SA_MAX_BW]; /* Performence info for each bandwidth */
    u_int32_t ts_perfslot;  /* starting time stamp of performence monitor time slot */

    u_int8_t  hybrid_train;    /* setting this to indicate hybrid mode for training */
    u_int32_t ts_trainstart;      /* train start time stamp */
    u_int32_t ts_traindone;       /* train done time stamp */
 #if SA_DEBUG
    u_int16_t prd_trigger_count;  /* periodic trigger count */
    u_int16_t perf_trigger_count; /* performence trigger count */
    u_int16_t last_training_time; /* training time for last training */
#endif
   struct node_train_stats train_state; /* current training sate info */
   u_int8_t training_start;     /* value other than one indicates fixed rate training */
   u_int8_t antenna_change_ind; /* indicate to change antenna in next feedack */
   uint32_t nppdu_bw[SA_MAX_BW];/* Number of training ppdu's sent in the bandwidth */
   uint32_t nppdu; /* counter for training ppdu's */
#define SA_TX_FEEDBACK_CONFIG_DEFAULT 0xe4
   uint8_t tx_feedback_config; /* configure target to send 1 tx feedback to host for N ppdus sent*/
};

struct sa_node_train_data {
    u_int32_t antenna;    /* anntenna for which stats are colleted*/
    u_int32_t ratecode;   /* rateCode at which training is happening */
    u_int16_t nFrames;   /* Total number of trasmited frames */
    u_int16_t nBad;      /* Total number of failed frames */
    int8_t rssi[SA_MAX_RECV_CHAINS][SA_MAX_RSSI_SAMPLES];  /* Block ACK/ACK RSSI for all chains */
    u_int16_t last_nFrames; /* packets sent in last interval*/
    u_int16_t numpkts;   /* Number of packets required for training*/
    u_int8_t cts_prot;   /* CTS protection flag */
    u_int8_t samples;    /* number of RSSI smaples */
};

#if SA_DEBUG
struct sa_debug_train_cmd {
    u_int32_t antenna;
    u_int32_t ratecode;
    u_int16_t nframes;
    u_int16_t nbad;
    u_int16_t nppdu;
    int8_t rssi[SA_MAX_RECV_CHAINS];
    u_int8_t per;
    u_int8_t bw;
#define SA_DBG_CODE_NXT_RATE_LLB 0x10
#define SA_DBG_CODE_CUR_RATE_LLB 0x20
#define SA_DBG_CODE_NXT_RATE_LUB 0x30
#define SA_DBG_CODE_RSSI_BETTER  0x40
#define SA_DBG_CODE_PER_BETTER   0x50
#define SA_DBG_CODE_ANT_SWITCH   0x01
    u_int8_t ant_sel;
    u_int8_t last_train_dir;
    u_int32_t last_rate;
};

struct sa_node_debug_data {
#define SA_MAX_DEBUG_TRAIN_CMDS 16
    struct sa_debug_train_cmd train_cmd[SA_MAX_DEBUG_TRAIN_CMDS];
    u_int32_t cmd_count;
};
#endif

struct sa_node_ccp {
  struct sa_node_info node_info;
  struct sa_node_train_info train_info;
  struct sa_node_rate_stats rate_stats;
  struct sa_node_train_data train_data;
#if SA_DEBUG
  struct sa_node_debug_data debug_data;
#endif
};

struct sa_testmode_train_params {
    u_int32_t ratecode; /* training rate */
    u_int32_t antenna; /* training antenna */
    u_int16_t numpkts; /* Number of packets required for training*/
};

#endif
