/*
 * Copyright (c) 2009, Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */

/*
 * Public Interface for slow antenna diversity control module
 */

#ifndef _DEV_ATH_ANTDIV_H
#define _DEV_ATH_ANTDIV_H

#define ATH_ANT_DIV_MAX_CFG   2
#define ATH_ANT_DIV_MAX_CHAIN 3

typedef enum {
    ATH_ANT_DIV_IDLE,
    ATH_ANT_DIV_SCAN,     /* evaluating antenna */
} ATH_ANT_DIV_STATE;

struct ath_antdiv {
    struct ath_softc        *antdiv_sc;
    u_int8_t                antdiv_start;
    u_int8_t                antdiv_suspend;
    ATH_ANT_DIV_STATE       antdiv_state;
    u_int8_t                antdiv_num_antcfg;
    u_int8_t                antdiv_curcfg;
    u_int8_t                antdiv_bestcfg;
    u_int8_t                antdiv_def_antcfg;
    int8_t                  antdiv_rssitrig;
    u_int32_t               antdiv_min_dwell_time;
    u_int32_t               antdiv_settle_time;
    int8_t                  antdiv_lastbrssictl[ATH_ANT_DIV_MAX_CHAIN][ATH_ANT_DIV_MAX_CFG];
/* not yet
    int8_t                  antdiv_lastbrssi[ATH_ANT_DIV_MAX_CFG];
    int8_t                  antdiv_lastbrssiext[ATH_ANT_DIV_MAX_CHAIN][ATH_ANT_DIV_MAX_CFG];
 */
    u_int64_t               antdiv_lastbtsf[ATH_ANT_DIV_MAX_CFG];
    u_int64_t               antdiv_laststatetsf;   
    u_int8_t                antdiv_bssid[IEEE80211_ADDR_LEN];
    spinlock_t              antdiv_lock;
};

/*
 * Interface to if_ath
 */
#if ATH_SLOW_ANT_DIV

void ath_slow_ant_div_init(struct ath_antdiv *antdiv, struct ath_softc *sc, int32_t rssitrig,
                           u_int32_t min_dwell_time, u_int32_t settle_time);
void ath_slow_ant_div_start(struct ath_antdiv *antdiv, u_int8_t num_antcfg, const u_int8_t *bssid);
void ath_slow_ant_div_stop(struct ath_antdiv *antdiv);
void ath_slow_ant_div(struct ath_antdiv *antdiv, struct ieee80211_frame *wh, struct ath_rx_status *rx_stats);

void ath_slow_ant_div_suspend(ath_dev_t dev);
void ath_slow_ant_div_resume(ath_dev_t dev);

#else

#define ath_slow_ant_div_init(antdiv, sc, rssitrig, min_dwell_time, settle_time)
#define ath_slow_ant_div_start(antdiv, num_antcfg, bssid)
#define ath_slow_ant_div_stop(antdiv)
#define ath_slow_ant_div(antdiv, wh, rx_stats)

#define ath_slow_ant_div_suspend(dev)
#define ath_slow_ant_div_resume(dev)

#endif    /* ATH_SLOW_ANT_DIV */

#if ATH_ANT_DIV_COMB
struct ath_antcomb {
    struct ath_softc        *antcomb_sc;
    u_int16_t               count;
    u_int16_t               total_pkt_count;
    int8_t                  scan;
    int8_t                  scan_not_start;
    int                     main_total_rssi;
    int                     alt_total_rssi;
    int                     low_rssi_th;
    u_int8_t                fast_div_bias;    
    int                     alt_recv_cnt;
    int                     main_recv_cnt;
    int8_t                  rssi_lna1;
    int8_t                  rssi_lna2;
    int8_t                  rssi_add;
    int8_t                  rssi_sub;
    int8_t                  rssi_first;
    int8_t                  rssi_second;
    int8_t                  rssi_third;    
    int8_t                  alt_good;
    u_int32_t               scan_start_time;
    int8_t                  quick_scan_cnt;
    int8_t                  main_conf;
    int8_t                  first_quick_scan_conf;
    int8_t                  second_quick_scan_conf;
    int8_t                  first_bias;
    int8_t                  second_bias;     
    int8_t                  first_ratio;
    int8_t                  second_ratio; 
};

#define ANT_DIV_COMB_SHORT_SCAN_INTR   50
#define ANT_DIV_COMB_SHORT_SCAN_PKTCOUNT   (0x100)
#define ANT_DIV_COMB_MAX_PKTCOUNT   (0x200)
#define ANT_DIV_COMB_INIT_COUNT 95
#define ANT_DIV_COMB_MAX_COUNT 100
#define ANT_DIV_COMB_ALT_ANT_RATIO 30
#define ANT_DIV_COMB_ALT_ANT_RATIO_LOW_RSSI  50
#define ANT_DIV_COMB_ALT_ANT_RATIO2 20
#define ANT_DIV_COMB_ALT_ANT_RATIO2_LOW_RSSI 50
#define ANT_DIV_COMB_REG_MASK1 (AR_PHY_KITE_ANT_DIV_ALT_LNACONF | \
                                AR_PHY_KITE_ANT_FAST_DIV_BIAS)
#define ANT_DIV_COMB_REG_MASK2 (AR_PHY_KITE_ANT_DIV_MAIN_LNACONF | \
                                AR_PHY_KITE_ANT_DIV_ALT_LNACONF | \
                                AR_PHY_KITE_ANT_FAST_DIV_BIAS)
#define ANT_DIV_COMB_LNA1_LNA2_SWITCH_DELTA  (-1)
#define ANT_DIV_COMB_LNA1_DELTA_HI           (-4)
#define ANT_DIV_COMB_LNA1_DELTA_MID          (-2)
#define ANT_DIV_COMB_LNA1_DELTA_LOW          (2)

void ath_ant_div_comb_init(struct ath_antcomb *antcomb, struct ath_softc *sc);
void ath_ant_div_comb_scan(struct ath_antcomb *antcomb, struct ath_rx_status *rx_stats);
void ath_set_lna_div_use_bt_ant(ath_dev_t dev, bool enable);

/* smart antenna diversity */
#define SMART_ANT_FORCED_AUTO       0xa     /* smart antenna forced auto */
#define SMART_ANT_FORCED_FIXED      0xf     /* smart antenna forced fixed */
#define SMART_ANT_SKIP_MAX          5       /* The max number of times we can skip one pattern.
                                               If this pattern is very bad, we wish to skip this 
                                               pattern in next a few SA scans */
#define SMART_ANT_CONF_MASK         0x3     /* mask for hw antenna configuration in rx_stats->
                                               rs_rssi_ctl2 */
#define SMART_ANT_CUR_CONF_S        4       /* shift for current hw antenna configuration */
#define SMART_ANT_MAIN_CONF_S       2       /* shift for hw antenna configuration of main antenna */
#define SMART_ANT_ALT_CONF_S        0       /* shift for hw antenna configuration of alt antenna */
#define SMART_ANT_SWITCH_MASK       0xf     /* mask of rx switch_com in rx_stats->rs_rssi_ext2 */
#define SMART_ANT_TABLE_MAX         12      /* max row number in pattern table */
#define SMART_ANT_HISTORY_NUM       8       /* the number of SA scan history being printed in debug
                                               mode */
#define SMART_ANT_LOWER_FACTOR      85      /* lower bound of rx phy rate. If the average rx phy 
                                               rate of idle period is lower than lower bound, will
                                               trigger SA scan despite of the granted number of 
                                               idle period */
#define SMART_ANT_UPPER_FACTOR      115     /* upper bound of rx phy rate. If the average rx phy 
                                               rate of idle period is higher than upper bound, will
                                               trigger SA scan despite of the granted number of 
                                               idle period */
#define SMART_ANT_PKT_THRESH        3       /* the threshold of lowest packets. Once it rx/tx 
                                               enough packets more than this threshold, it starts 
                                               to check the rx/tx phy rate, and quick switch back 
                                               to default pattern if phy rate is low. */
#define SMART_ANT_MAX_IDLE          8       /* max granted idle period. When idle period is ended, 
                                               it will check the average rx phyrate, if the average
                                               rx phy rate does not exceed bounaries, it will not 
                                               perform SA scan. another idle period will start, 
                                               until the granted idle period reached */
#define SMART_ANT_SCAN_TIME         100     /* in ms. The scan time of SA scan for each pattern. 
                                               When the scan time ended, the average rx phy rate
                                               will be recorded and start another SA scan for next
                                               pattern */
#define SMART_ANT_CHECK_BAD_TIME    20      /* in ms. Set a short time for a quick check for bad
                                               performance so it doesn't stuck at low phy rate. */
#define SMART_ANT_IDLE_TIME         3000    /* in ms. Time for idle period. Idle period is the time
                                               between SA scans */
#define SMART_ANT_LOW_TRAFFIC       100     /* threshold of low traffic. Traffic is low if packets
                                               received at idle period is lower than this value */

typedef enum {
    SMART_ANT_INIT,                         /* smart antenna is attached but not running */
    SMART_ANT_IDLE,                         /* smart antenna is running at idle period */
    SMART_ANT_SCAN,                         /* smart antenna is running at scan period */
} ATH_SA_STATE;

typedef struct ath_sa_pattern_info {
	char*               lna_str;            /* string of which LNA combination is used */
	char*               ant_str;            /* string of which antenna is used */
    u_int8_t            main_lna;           /* configuration of LNA combination */
    u_int8_t            switch_com_ra1l1;   /* rx antenna switch_com setting */
    u_int8_t            switch_com_t1;      /* tx antenna switch_com setting */
} ATH_SA_PATTERN_INFO;

typedef struct ath_sa_pattern_table {
    u_int8_t            patternCount;       /* number of patterns in this table */
    ATH_SA_PATTERN_INFO *info;              /* pattern information */
    u_int8_t            antennaAntx;        /* number of antennas connected to LNA1 */
} ATH_SA_PATTERN_TABLE;

struct ath_sa_info {
    u_int32_t           data_cnt;           /* packets received/transmitted */
    u_int32_t           avg_phyrate;        /* moving average of rx/tx phy rate */
    u_int32_t           delta_time;         /* scan period time spent */
    u_int8_t            f_skip:1,           /* flag that this pattern is skipped at current SA scan */
                        f_bad_detect:1,     /* flag that this pattern is found a bad pattern at 
                                               current SA scan */
                        visited:1;          /* flag that this pattern is visited at current SA scan */
};

typedef struct ath_sa_aplist {
    TAILQ_ENTRY(ath_sa_aplist) ap_next;
    systime_t           time_stamp;
    u_int8_t            addr[IEEE80211_ADDR_LEN]; /* ap list to record bssid and the Beacon rssi of 
                                                     given antenna. */
    u_int8_t            rssi[SMART_ANT_TABLE_MAX];
} ATH_SA_APLIST;

struct ath_sa_query_info {
    u_int32_t           sa_attached;        /* smart antenna module attached */
    u_int32_t           sa_started;         /* smart antenna is not in SMART_ANT_INIT state */
    u_int32_t           sa_current_ant;     /* current used antenna */
    u_int32_t           sa_ap_count;        /* total ap count in ap list */
    u_int8_t            bssid[IEEE80211_ADDR_LEN];
    u_int8_t            sa_ap_list[];
    
};

struct ath_sa_config_info {
    u_int32_t           saAlg;              /* force smart antenna to AUTO or FIX mode */
    u_int32_t           saFixedMainLna;     /* if saAlg is FIX mode, use to fix the main LNA */
    u_int32_t           saFixedSwitchComR;  /* if saAlg is FIX mode, use to fix rx switch com */
    u_int32_t           saFixedSwitchComT;  /* if saAlg is FIX mode, use to fix tx switch com */  
};

/* smart antenna scan */
struct ath_sascan {
    struct ath_softc   *sa_sc;

    u_int8_t            sa_mode;            /* could be SMART_ANT_FORCED_AUTO or 
                                               SMART_ANT_FORCED_FIXED if set by registry or oid.
                                               Otherwise it represents the number of antennas 
                                               connected to LNA1. 0 means smart antenna not enabled */
    ATH_SA_STATE        sa_state;           /* state of smart antenna */
    ATH_SA_PATTERN_TABLE *antenna_table;    
    u_int8_t            patternMaxCount;    /* number of patterns in antenna_table */
    u_int8_t            antennaAntxCount;   /* number of antennas in LNA1 */
    os_timer_t          sa_scan_timer;      /* timer for pattern scan */
    os_timer_t          sa_idle_timer;      /* timer for idle period */
    os_timer_t          sa_check_timer;     /* timer for checking for bad throughput */

    /* rx antenna scan information */
    u_int8_t            antenna_pattern;    /* selected pattern in current pattern scan */
    struct ath_sa_info  scan_rx_info[SMART_ANT_TABLE_MAX];      /* info for sa rx scan */
    struct ath_sa_info  idle_rx_info;                           /* info for idle period */
    u_int8_t            antenna_skip_cnt[SMART_ANT_TABLE_MAX];  /* record times one pattern  */
    u_int8_t            cur_rx_conf;        /* current antenna pattern. It represents real phy 
                                               setting which might be different from antenna_pattern. */
    u_int8_t            best_rx_conf;       /* best antenna pattern result from SA scan */
    u_int32_t           best_rx_phyrate;    /* best rx phyrate result from SA scan */
    u_int32_t           rx_phyrate_thresh1; /* first level of rx phyrate threshold */
    u_int32_t           rx_phyrate_thresh2; /* second level of rx phyrate threshold worse than level 1 */
    u_int8_t            antx_conf;          /* current rx antenna of LNA1 */
    u_int8_t            old_antx_conf;      /* old rx antenna */
    HAL_BOOL            bad_rx_detected;    /* bad rx is detected in current scan of a given pattern */

    /* tx antenna scan information */
    struct ath_sa_info  scan_tx_info[SMART_ANT_TABLE_MAX];      /* info for sa tx scan */
    u_int8_t            cur_tx_conf;        /* current tx switch com setting */
    u_int8_t            best_tx_conf;       /* best tx switch com setting result from SA scan */
    u_int32_t           best_tx_phyrate;    /* best tx phyrate result from SA scan */
    u_int32_t           tx_phyrate_thresh;  /* tx phyrate threshold */
    HAL_BOOL            bad_tx_detected;    /* bad tx is detected in current scan of a given pattern */

    u_int8_t            keep_same_cnt;      /* idle periods being used */
    u_int8_t            keep_same_granted;  /* idle periods being granted */
    systime_t           period_last_time;   /* time stamp of current scan of a given pattern */
    systime_t           scan_time;          /* time stamp of SA scan start point */
    
    u_int8_t            sa_ap_conf;         /* round robin of antennas for normal scan */
    ATH_SA_STATE        sa_o_state;         /* old SA state */
    u_int8_t            bssid[IEEE80211_ADDR_LEN];  /* associated/joining to bssid */
    TAILQ_HEAD(, ath_sa_aplist)    sa_aplist;       /* bssid and beacon rssi of a given ap */
    u_int16_t           sa_ap_count;        /* total APs being heard */
    
    /* Debug : record selected ant history  */    
    u_int8_t            h_ant_conf[SMART_ANT_HISTORY_NUM];
    u_int8_t            h_antx_conf[SMART_ANT_HISTORY_NUM];
    u_int32_t           h_scan_Start2End_time[SMART_ANT_HISTORY_NUM];
    u_int8_t            h_r_idx;
    u_int32_t           sa_r_id;
    u_int32_t           h_r_id[SMART_ANT_HISTORY_NUM];
};

u_int8_t    ath_smartant_attach(struct ath_softc *sc);
void        ath_smartant_detach(struct ath_softc *sc);
void        ath_smartant_fixed(struct ath_sascan *sascan);
void        ath_smartant_start(struct ath_sascan *sascan, const u_int8_t *bssid);
void        ath_smartant_stop(struct ath_sascan *sascan);
void        ath_smartant_rx_scan(struct ath_sascan *sascan, struct ieee80211_frame *wh, 
                                    struct ath_rx_status *rx_stats, int phyrate);
void        ath_smartant_tx_scan(struct ath_sascan *sascan, int phyrate);
u_int32_t   ath_smartant_query(ath_dev_t dev, u_int32_t *buffer);
void        ath_smartant_config(ath_dev_t dev, u_int32_t *buffer);
void        ath_smartant_get_apconf(struct ath_sascan *sascan, const u_int8_t *bssid);
#endif /* ATH_ANT_DIV_COMB */

#endif