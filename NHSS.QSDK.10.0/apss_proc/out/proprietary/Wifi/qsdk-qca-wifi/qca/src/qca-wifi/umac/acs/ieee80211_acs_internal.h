/*
 * Copyright (c) 2013,2015,2017,2019 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2013, 2015 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#ifndef _IEEE80211_ACS_INTERNAL_H
#define _IEEE80211_ACS_INTERNAL_H



#define NF_WEIGHT_FACTOR (2)
#define CHANLOAD_WEIGHT_FACTOR (4)
#define CHANLOAD_INCREASE_AVERAGE_RSSI (40)
#define ACS_NOISE_FLOOR_THRESH_MIN -85 /* noise floor threshold to detect presence of video bridge */
#define ACS_NOISE_FLOOR_THRESH_MAX -65 /* noise floor threshold to detect presence of video bridge */
#define ACS_DEFAULT_SR_LOAD 4

/* Parameters to derive secondary channels */
#define UPPER_FREQ_SLOT 1
#define LOWER_FREQ_SLOT -1
#define SEC_40_LOWER -6
#define SEC_40_UPPER -2
#define SEC20_OFF_2 2
#define SEC20_OFF_6 6
#define SEC_80_1 2
#define SEC_80_2 6
#define SEC_80_3 10
#define SEC_80_4 14
#define PRI_80_CENTER 8
/* Use a RSSI threshold of 10dB(?) above the noise floor*/
#define SPECTRAL_EACS_RSSI_THRESH  30 

#define ACS_11NG_NOISE_FLOOR_REJ (-80)
#define ACS_11NA_NOISE_FLOOR_REJ (-80)
#define IEEE80211_MAX_ACS_EVENT_HANDLERS 16
#define LIMITED_OBSS_CHECK 1
#define DEBUG_EACS 1
#define MIN_DWELL_TIME        200  /* scan param to be used during acs scan 200 ms */
#define MAX_DWELL_TIME        300  /* scan param to be used during acs scan 300 ms */
#define EACS_DBG_LVL0  0x80000000
#define EACS_DBG_LVL1  0x40000000
#define EACS_DBG_DEFAULT 0x1
#define EACS_DBG_RSSI    0x2
#define EACS_DBG_ADJCH   0x4
#define EACS_DBG_NF      0x8
#define EACS_DBG_CHLOAD    0x10
#define EACS_DBG_REGPOWER  0x20
#define EACS_DBG_OBSS      0x40
#define EACS_DBG_SCAN      0x80
#define EACS_DBG_BLOCK     0x100

#define ACS_TX_POWER_OPTION_TPUT 1
#define ACS_TX_POWER_OPTION_RANGE 2


#define NF_INVALID -254

#define SEC_TO_MSEC(_t ) (_t * 1000) /* Macro to convert SEC to MSEC */
/* To restrict number of hoppings in 2.4 Gh used by channel hopping algorithm */
#define ACS_CH_HOPPING_MAX_HOP_COUNT  3 

/*cmd value to enhance read ablity between ic and ath layer */
#define IEEE80211_ENABLE_NOISE_DETECTION  1 /*from ic to enable/disable noise detection */ 
#define IEEE80211_NOISE_THRESHOLD         2 /* ic->ath noise threshold val set /get*/ 
#define IEEE80211_GET_COUNTER_VALUE       3 /* counter threshold value from ic->ath */  
#define CHANNEL_HOPPING_LONG_DURATION_TIMER 15*60 /* 15 min */
#define CHANNEL_HOPPING_NOHOP_TIMER 1*60 /* 1 min */
#define CHANNEL_HOPPING_CNTWIN_TIMER 5 /* 5 sec  */
#define CHANNEL_HOPPING_VIDEO_BRIDGE_THRESHOLD -90

#define MAX_32BIT_UNSIGNED_VALUE 0xFFFFFFFFU

#if DEBUG_EACS

extern unsigned int  eacs_dbg_mask ;

#define eacs_trace(log_level, data)  do {          \
        if(log_level & eacs_dbg_mask) {            \
                    printk("%s :",__func__);        \
                    printk data ;                  \
                    printk("\n");                  \
                }                                  \
} while (0)

#else
#define eacs_trace(log_level, data)
#endif

/* Added to avoid Static overrun Coverity issues */
#define IEEE80211_ACS_CHAN_MAX IEEE80211_CHAN_MAX+1

struct acs_user_chan_list {
    u_int32_t uchan[IEEE80211_ACS_CHAN_MAX];    /* max user channels */
    u_int32_t uchan_cnt;
};

typedef struct acs_bchan_list_r {
    u_int32_t uchan[IEEE80211_CHAN_MAX];    /* max user channels */
    u_int32_t uchan_cnt;
} acs_bchan_list_t;

struct acs_scan_req_param_t {
    u_int8_t acs_scan_report_active;
    u_int8_t acs_scan_report_pending;
    u_int32_t mindwell;
    u_int32_t maxdwell;
    u_int8_t scan_mode;
    u_int32_t rest_time;
    u_int32_t idle_time;
    u_int32_t max_scan_time;
};
struct acs_ch_hopping_param_t {
    u_int32_t long_dur;
    u_int32_t nohop_dur;
    u_int32_t cnt_dur;
    u_int32_t cnt_thresh;
    int32_t noise_thresh;
};

struct acs_ch_hopping_t {
    struct acs_ch_hopping_param_t param;
    os_timer_t ch_long_timer;  /* Long timer */
    os_timer_t ch_nohop_timer; /* No hop timer */
    os_timer_t ch_cntwin_timer; /*counter window timer*/
    u_int32_t  ch_max_hop_cnt; /*we should not hop for more than this counter */
    bool       ch_nohop_timer_active;
    bool       ch_hop_triggered; /*To mark channel hopping is trying to change channel */
};

struct acs_srp_info_s {
    uint8_t srp_allowed:1,
            srp_obsspd_allowed:1;

    uint8_t srp_nonsrg_obss_pd_max_offset;
    uint8_t srp_srg_obss_pd_min_offset;
    uint8_t srp_srg_obss_pd_max_offset;
};

/* NOTE: This macro corresponds to macro ACS_RANK_DESC_DBG_LEN, Please change
 * it aswell if changing this.
 */
#define ACS_RANK_DESC_LEN 80

/* ACS Channel Ranking structure
 * rank: Channel Rank
 * desc: Reason in case of no rank
 */
typedef struct acs_rank {
    u_int32_t rank;
    char desc[ACS_RANK_DESC_LEN];
}acs_rank_t;

/**
 * struct acs_last_chutil - keeps track of last recorded channel utilization
 * @ieee_chan: channel number
 * @ch_util: last recorded chan util for ieee_chan
 */
struct acs_last_chutil {
    uint16_t ieee_chan;
    uint16_t ch_util;
};

/**
 * struct acs_last_event - keeps track of last acs activity/event
 * @ieee_chan: last events channel number 
 * @event: last event type
 * @last_util: last recorded channel utilization info
 */
struct acs_last_event {
    uint16_t ieee_chan;
    uint16_t event;
    struct acs_last_chutil last_util;
};

struct ieee80211_acs {
    /* Driver-wide data structures */
    wlan_dev_t                          acs_ic;
    wlan_if_t                           acs_vap;
    osdev_t                             acs_osdev;

    spinlock_t                          acs_lock;                /* acs lock */
    qdf_spinlock_t                      acs_ev_lock;             /* serialize between scan event handling and iwpriv commands */

    /* List of clients to be notified about scan events */
    u_int16_t                           acs_num_handlers;
    ieee80211_acs_event_handler         acs_event_handlers[IEEE80211_MAX_ACS_EVENT_HANDLERS];
    void                                *acs_event_handler_arg[IEEE80211_MAX_ACS_EVENT_HANDLERS];

    wlan_scan_requester            	    acs_scan_requestor;    /* requestor id assigned by scan module */
    wlan_scan_id	                    acs_scan_id;           /* scan id assigned by scan scheduler */
    u_int8_t                            acs_scan_2ghz_only:1; /* flag for scan 2.4 GHz channels only */
    u_int8_t                            acs_scan_5ghz_only:1; /* flag for scan 5 GHz channels only */
    atomic_t                            acs_in_progress; /* flag for ACS in progress */
    bool                                acs_run_incomplete;
    struct ieee80211_ath_channel            *acs_channel;

    u_int16_t                           acs_nchans;         /* # of all available chans */
    struct ieee80211_ath_channel            *acs_chans[IEEE80211_ACS_CHAN_MAX];
    u_int8_t                            acs_chan_maps[IEEE80211_ACS_CHAN_MAX];       /* channel mapping array */

    int32_t                             acs_chan_rssi[IEEE80211_ACS_CHAN_MAX];         /* Total rssi of these channels */
    int32_t                             acs_chan_rssitotal[IEEE80211_ACS_CHAN_MAX];    /* Calculated rssi of these channels */
    int32_t                             acs_chan_loadsum[IEEE80211_ACS_CHAN_MAX];      /* Sum of channle load  */
    int32_t                             acs_adjchan_load[IEEE80211_ACS_CHAN_MAX];      /* Sum of channle load  */
    int32_t                             acs_chan_regpower[IEEE80211_ACS_CHAN_MAX];      /* Sum of channle load  */
    int32_t                             acs_80211_b_duration[IEEE80211_ACS_CHAN_MAX];   /* 11b duration in channel */


    int32_t                             acs_adjchan_flag[IEEE80211_ACS_CHAN_MAX];      /* Adj channel rej flag*/
    int32_t                             acs_channelrejflag[IEEE80211_ACS_CHAN_MAX];    /* Channel Rejection flag */


    int32_t                             acs_rssivar;
    int32_t                             acs_chloadvar;
    int32_t                             acs_srvar;
    int32_t                             acs_limitedbsschk;
    int32_t                             acs_bkscantimer_en;
    int32_t                             acs_bk_scantime;


    int32_t                             acs_11nabestchan;
    int32_t                             acs_11ngbestchan;
    int32_t                             acs_minrssisum_11ng;

    ieee80211_acs_scantimer_handler     acs_scantimer_handler;
    void                               *acs_scantimer_arg;
    os_timer_t                          acs_bk_scantimer;





    int32_t                             acs_chan_maxrssi[IEEE80211_ACS_CHAN_MAX];    /* max rssi of these channels */
    int32_t                             acs_chan_minrssi[IEEE80211_ACS_CHAN_MAX];    /* Min rssi of the channel [debugging] */
    int32_t                             acs_noisefloor[IEEE80211_ACS_CHAN_MAX];      /* Noise floor value read current channel */
    int16_t                             acs_channel_loading[IEEE80211_ACS_CHAN_MAX];      /* Noise floor value read current channel */
    u_int32_t                           acs_chan_load[IEEE80211_ACS_CHAN_MAX];
    u_int32_t                           acs_cycle_count[IEEE80211_ACS_CHAN_MAX];
#if ATH_SUPPORT_VOW_DCS
    u_int32_t                           acs_intr_ts[IEEE80211_ACS_CHAN_MAX];
    u_int8_t                            acs_intr_status[IEEE80211_ACS_CHAN_MAX];
    os_timer_t                          dcs_enable_timer;
    int32_t                             dcs_enable_time;
    u_int8_t                            is_dcs_enable_timer_set;
#endif
    int32_t                             acs_minrssi_11na;    /* min rssi in 5 GHz band selected channel */
    int32_t                             acs_avgrssi_11ng;    /* average rssi in 2.4 GHz band selected channel */
    bool                                acs_sec_chan[IEEE80211_ACS_CHAN_MAX];       /*secondary channel flag */
    u_int8_t                            acs_chan_nbss[IEEE80211_ACS_CHAN_MAX];      /* No. of BSS of the channel */
    u_int16_t                           acs_nchans_scan;         /* # of all available chans */
    u_int8_t                            acs_ieee_chan[IEEE80211_ACS_CHAN_MAX];       /* channel mapping array */
    struct acs_srp_info_s               acs_srp_info[IEEE80211_ACS_CHAN_MAX]; /* srp info */
    int32_t                             acs_srp_supported[IEEE80211_ACS_CHAN_MAX]; /* no. of BSS supporting spatil reuse */
    int32_t                             acs_srp_load[IEEE80211_ACS_CHAN_MAX]; /* Load sue to spatil reuse APs */
#if ATH_ACS_SUPPORT_SPECTRAL
    int8_t                              ctl_eacs_rssi_thresh;          /* eacs spectral control channel rssi threshold */
    int8_t                              ext_eacs_rssi_thresh;          /* eacs spectral extension channel rssi threshold */
    int8_t                              ctl_eacs_avg_rssi;             /* eacs spectral control channel avg rssi */
    int8_t                              ext_eacs_avg_rssi;             /* eacs spectral extension channel avg rssi */

    int32_t                             ctl_chan_loading;              /* eacs spectral control channel spectral load*/
    int32_t                             ctl_chan_frequency;            /* eacs spectral control channel frequency (in Mhz) */
    int32_t                             ctl_chan_noise_floor;          /* eacs spectral control channel noise floor*/
    int32_t                             ext_chan_loading;              /* eacs spectral extension channel spectral load*/      
    int32_t                             ext_chan_frequency;            /* eacs spectral extension channel frequency (in Mhz) */
    int32_t                             ext_chan_noise_floor;          /* eacs spectral externsion channel noise floor*/        
    int32_t                             ctl_eacs_spectral_reports;     /* no of spectral reports received for control channel*/
    int32_t                             ext_eacs_spectral_reports;     /* no of spectral reports received for extension channel*/
    int32_t                             ctl_eacs_interf_count;         /* no of times interferece detected on control channel */
    int32_t                             ext_eacs_interf_count;         /* no of times interferece detected on extension*/
    int32_t                             ctl_eacs_duty_cycle;           /* duty cycles on control channel*/
    int32_t                             ext_eacs_duty_cycle;           /* duty cycles on extension channel*/
    int32_t                             eacs_this_scan_spectral_data;  /* no. of spectral sample to calculate duty cycles */
#endif
    struct acs_scan_req_param_t         acs_scan_req_param;
    struct acs_user_chan_list           acs_uchan_list; 	      /* struct user chan */
    struct acs_ch_hopping_t             acs_ch_hopping;		      /* To hold channel hopping related parammeter */	
    acs_bchan_list_t                    acs_bchan_list;         /* channel blocked by user */
#if ATH_CHANNEL_BLOCKING
#define ACS_BLOCK_MANUAL           0x1
#define ACS_BLOCK_EXTENSION        0x2
    u_int32_t                           acs_block_mode;         /* whether to block a channel if extension channel is blocked or
                                                                 * whether to block a channel if set manually (instead of acs) */
#endif
    u_int32_t                           acs_startscantime;      /* to note the time when the scan has started */
    u_int8_t                            acs_tx_power_type;
    u_int8_t                            acs_2g_allchan;
    acs_rank_t                          acs_rank[IEEE80211_ACS_CHAN_MAX]; /* ACS Rank and channel reject code for max channels */
    bool                                acs_ranking;            /* Enable/Disable ACS channel Ranking */
#if ATH_ACS_DEBUG_SUPPORT
    void *                              acs_debug_bcn_events;    /* Pointer to beacon events for the ACS debug framework */
    void *                              acs_debug_chan_events;   /* Pointer to channel events for the ACS debug framework */
#endif
    int32_t                             acs_noisefloor_threshold;
    u_int8_t                            acs_status;              /* ACS success/failed status */
    struct acs_last_event               acs_last_evt;            /* Last acs event */
};

struct ieee80211_acs_adj_chan_stats {
    u_int32_t                           adj_chan_load;
    u_int32_t                           adj_chan_rssi;
    u_int8_t                            if_valid_stats;    
    u_int8_t                            adj_chan_idx;
    u_int32_t                           adj_chan_flag;
    u_int32_t                           adj_chan_loadsum;
    u_int32_t                           adj_chan_rssisum;
    u_int32_t                           adj_chan_srsum;
};

struct acs_obsscheck{
	ieee80211_acs_t acs ;
	struct ieee80211_ath_channel *channel;
	int onlyextcheck;
	int extchan;
	int olminlimit;
	int olmaxlimit;
};

#define UNII_II_EXT_BAND(freq)  (freq >= 5500) && (freq <= 5700)


#define ADJ_CHAN_SEC_NF_FLAG       0x1
#define ADJ_CHAN_SEC1_NF_FLAG      0x2
#define ADJ_CHAN_SEC2_NF_FLAG      0x4
#define ADJ_CHAN_SEC3_NF_FLAG      0x8
#define ADJ_CHAN_SEC4_NF_FLAG      0x10
#define ADJ_CHAN_SEC5_NF_FLAG      0x11
#define ADJ_CHAN_SEC6_NF_FLAG      0x12


#define ACS_FLAG_NON5G                  0x1
#define ACS_REJFLAG_SECCHAN             0x2
#define ACS_REJFLAG_WEATHER_RADAR       0x4
#define ACS_REJFLAG_DFS                 0x8
#define ACS_REJFLAG_HIGHNOISE           0x10
#define ACS_REJFLAG_RSSI                0x20
#define ACS_REJFLAG_CHANLOAD            0x40
#define ACS_REJFLAG_REGPOWER            0x80
#define ACS_REJFLAG_NON2G               0x100
#define ACS_REJFLAG_PRIMARY_80_80       0x200
#define ACS_REJFLAG_NO_SEC_80_80        0x400
#define ACS_REJFLAG_NO_PRIMARY_80_80    0x800
#define ACS_REJFLAG_SPATIAL_REUSE       0x1000
#define ACS_REJFLAG_BLACKLIST           0x2000


#define ACS_ALLOWED_RSSIVARAINCE   10
#define ACS_ALLOWED_CHANLOADVARAINCE 10
#define ACS_ALLOWED_SRVARAINCE 3
#define ATH_ACS_DEFAULT_SCANTIME   120
#define DCS_ENABLE_TIME            (30*60) /* Duration after which DCS should be enabled back after disabling it
                                         due to 3 triggers in 5 minutes */






#endif


