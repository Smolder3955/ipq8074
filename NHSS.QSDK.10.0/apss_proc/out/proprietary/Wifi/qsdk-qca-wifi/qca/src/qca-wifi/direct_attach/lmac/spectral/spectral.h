/*
 * Copyright (c) 2011, 2017 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary . Qualcomm Innovation Center, Inc.
 *
 * Copyright (c) 2010, Atheros Communications Inc.
 * All Rights Reserved.
 *
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 */


#ifndef _SPECTRAL_H_
#define _SPECTRAL_H_

#include <osdep.h>
#include "sys/queue.h"

#ifdef WIN32
#include "spectral_types.h"
#endif
#include "spectral_classifier.h"
#include "spectralscan_classifier.h"
#include "spec_msg_proto.h"

//#include <net80211/ieee80211_var.h>

#include <ath_dev.h>
#include "ath_internal.h"
#include "if_athioctl.h"
//#include "if_athvar.h"
#include "ah.h"
#include "ah_desc.h"
#include "spectral_ioctl.h"
#include "spectral_data.h"
#include <wlan_spectral_public_structs.h>
#include <wlan_objmgr_cmn.h>


#undef line
#define line()              qdf_print("----------------------------------------------------")
#define HERE()              printk("HERE %s : %d\n", __func__, __LINE__)
#define NOT_YET()           printk("NOTYET %s : %d\n", __func__, __LINE__)
#undef SPECTRAL_TODO
#define SPECTRAL_TODO(str)  qdf_print(KERN_INFO "SPECTRAL : %s (%s : %d)", (str), __func__, __LINE__)

#ifdef WIN32
#pragma pack(push, spectral, 1)
#define __ATTRIB_PACK
#else
#ifndef __ATTRIB_PACK
#define __ATTRIB_PACK __attribute__ ((packed))
#endif
#endif

/* forward declaration */
struct ath_spectral;

#define MAX_NUM_CHANNELS             255
#define WLAN_SPECTRAL_ENABLE_DBG_FUNCS      0
//#define OL_SPECTRAL_DEBUG_CONFIG_INTERACTIONS 1

/* Used for the SWAR to obtain approximate combined rssi
   in secondary 80Mhz segment */
#define OFFSET_CH_WIDTH_20	65
#define OFFSET_CH_WIDTH_40	62
#define OFFSET_CH_WIDTH_80	56
#define OFFSET_CH_WIDTH_160	50

/* Note: Enabling the below debug flag can result in
   performance issues */
//#define SPECTRAL_DEBUG_SAMP_MSG 1


/* Support logspectral (An internal Unit Test application) */
/* TO avoid flooding the host with lot of data, when enabling
   SPECTRAL_SUPPORT_LOGSPECTRAL, we need to disable the send
   ing of SAMP message. This applies only to 11ac chips */
// #define SPECTRAL_SUPPORT_LOGSPECTRAL 1

typedef struct spectral_perchain_rssi_info {
    int8_t rssi_pri20;
    int8_t rssi_sec20;
    int8_t rssi_sec40;
    int8_t rssi_sec80;
}SPECTRAL_PERCHAIN_RSSI_INFO;

typedef struct spectral_rfqual_info {

    /* RF measurement information */

    /* RSSI Information */
    int8_t rssi_comb;

    /*
     * XXX : For now, we know we are getting information
     * for only 4 chains at max. For future extensions
     * use a define
     */
    SPECTRAL_PERCHAIN_RSSI_INFO pc_rssi_info[4];

    /* Noise floor information */
    int16_t noise_floor[4];
} SPECTRAL_RFQUAL_INFO;

typedef struct spectral_chan_info {
    /* center frequency 1 in MHz */
    u_int16_t center_freq1;

    /* center frequency 2 in MHz -valid only for 11ACVHT 80PLUS80 mode */
    u_int16_t center_freq2;

    /* channel width in MHz */
    u_int8_t chan_width;
}SPECTRAL_CHAN_INFO;


struct spectral_prep_err {
	u_int8_t rssi;
	u_int8_t dur;
	int is_pri:1;
	int is_ext:1;
	int is_dc:1;
	int is_early:1;
	u_int64_t fulltsf;
	u_int32_t rs_tstamp;
};

#define MAX_SPECTRAL_PAYLOAD 1500
extern int BTH_MIN_NUMBER_OF_FRAMES;
extern int spectral_debug_level;

#define SPECTRAL_DPRINTK(_sc, _m, _fmt, ...) do {                  \
        printk(_fmt, __VA_ARGS__);                                 \
  }                                                                \
}while (0)

#ifdef BIG_ENDIAN_HOST
#define SPECTRAL_MSG_COPY_CHAR_ARRAY(destp, srcp, len)  do { \
	int j; \
	u_int32_t *src, *dest; \
	src = (u_int32_t *)(srcp); \
	dest = (u_int32_t *)(destp); \
	for (j = 0; j < roundup((len), sizeof(u_int32_t)) / 4; j++) { \
	*(dest + j) = qdf_le32_to_cpu(*(src + j)); \
	} \
	} while (0)
#else
#define SPECTRAL_MSG_COPY_CHAR_ARRAY(destp, srcp, len) \
	OS_MEMCPY((destp), (srcp), (len));
#endif

#define SPECTRAL_MIN(a,b) ((a)<(b)?(a):(b))
#define SPECTRAL_MAX(a,b) ((a)>(b)?(a):(b))
#define SPECTRAL_DIFF(a,b) (SPECTRAL_MAX(a,b) - SPECTRAL_MIN(a,b))
#define SPECTRAL_ABS_DIFF(a,b) (SPECTRAL_MAX(a,b) - SPECTRAL_MIN(a,b))

#define MAX_SPECTRAL_PAYLOAD 1500

#define SPECTRAL_TSMASK              0xFFFFFFFF      /* Mask for time stamp from descriptor */
#define SPECTRAL_TSSHIFT             32              /* Shift for time stamp from descriptor */
#define SPECTRAL_TSF_WRAP       0xFFFFFFFFFFFFFFFFULL   /* 64 bit TSF wrap value */
#define SPECTRAL_64BIT_TSFMASK  0x0000000000007FFFULL   /* TS mask for 64 bit value */


#define SPECTRAL_HT20_NUM_BINS                      56
#define SPECTRAL_HT20_FFT_LEN                       56
#define SPECTRAL_HT20_DC_INDEX                      (SPECTRAL_HT20_FFT_LEN / 2)

//Mon Oct 19 13:01:01 PDT 2009 - read BMAPWT and MAXINDEX for HT20 differently
#define SPECTRAL_HT20_BMAPWT_INDEX                  56
#define SPECTRAL_HT20_MAXINDEX_INDEX                58

#define SPECTRAL_HT40_NUM_BINS                      64
#define SPECTRAL_HT40_TOTAL_NUM_BINS                128
#define SPECTRAL_HT40_FFT_LEN                       128
#define SPECTRAL_HT40_DC_INDEX                      (SPECTRAL_HT40_FFT_LEN / 2)
#define SPECTRAL_HT20_DATA_LEN                      60
#define SPECTRAL_HT20_TOTAL_DATA_LEN                (SPECTRAL_HT20_DATA_LEN + 3)
#define SPECTRAL_HT40_DATA_LEN                      135
#define SPECTRAL_HT40_TOTAL_DATA_LEN                (SPECTRAL_HT40_DATA_LEN + 3)

#define SPECTRAL_HT40_LOWER_BMAPWT_INDEX            128
#define SPECTRAL_HT40_HIGHER_BMAPWT_INDEX           131
#define SPECTRAL_HT40_LOWER_MAXINDEX_INDEX          130
#define SPECTRAL_HT40_HIGHER_MAXINDEX_INDEX         133

#define SPECTRAL_HT80_NUM_BINS                      128
#define SPECTRAL_HT80_TOTAL_NUM_BINS                256
#define SPECTRAL_HT80_FFT_LEN                       256

#define ATH_SPECTRALQ_LOCK(_spectral)       qdf_spin_lock(&(_spectral)->spectral_skbqlock)
#define ATH_SPECTRALQ_UNLOCK(_spectral)     qdf_spin_unlock(&(_spectral)->spectral_skbqlock)
#define ATH_SPECTRALQ_LOCK_INIT(_spectral)  qdf_spinlock_create(&(_spectral)->spectral_skbqlock)

/*
 * XXX : To Define for 802.11ac
 *
 *       80MHz
 *       -----
 *       SPECTRAL_HT80_DC_INDEX
 *       SPECTRAL_HT80_DATA_LEN
 *       SPECTRAL_HT80_TOTAL_DATA_LEN
 *       SPECTRAL_HT80_LOWER_BMAPWT_INDEX
 *       SPECTRAL_HT80_HIGHER_BMAPWT_INDEX
 *
 *       160MHz
 *       ------
 *       SPECTRAL_HT160_NUM_BINS
 *       SPECTRAL_HT160_TOTAL_NUM_BINS
 *       SPECTRAL_HT160_FFT_LEN
 *       SPECTRAL_HT160_DC_INDEX
 *       SPECTRAL_HT160_DATA_LEN
 *       SPECTRAL_HT160_TOTAL_DATA_LEN
 *       SPECTRAL_HT160_LOWER_BMAPWT_INDEX
 *       SPECTRAL_HT160_HIGHER_BMAPWT_INDEX
 *
 *
 */

#define SPECTRAL_MAX_EVENTS         1024        /* Max number of spectral events which can be q'd */

#define CLASSIFY_TIMEOUT_S             2
#define CLASSIFY_TIMEOUT_MS            (CLASSIFY_TIMEOUT_S * 1000)
#define DEBUG_TIMEOUT_S                1
#define DEBUG_TIMEOUT_MS               (DEBUG_TIMEOUT_S * 1000)

#define SPECTRAL_EACS_RSSI_THRESH      30 /* Use a RSSI threshold of 10dB(?) above the noise floor*/


typedef struct ht20_bin_mag_data {
    u_int8_t bin_magnitude[SPECTRAL_HT20_NUM_BINS];
} __ATTRIB_PACK HT20_BIN_MAG_DATA;

typedef struct ht40_bin_mag_data {
    u_int8_t bin_magnitude[SPECTRAL_HT40_NUM_BINS];
} __ATTRIB_PACK HT40_BIN_MAG_DATA;

typedef struct ht80_bin_mag_data {
    u_int8_t bin_magnitude[SPECTRAL_HT80_NUM_BINS];
}__ATTRIB_PACK HT80_BIN_MAG_DATA;

#ifndef OLD_MAGDATA_DEF
typedef struct max_mag_index_data {
        u_int8_t all_bins1;
        u_int8_t max_mag_bits29;
        u_int8_t all_bins2;
}__ATTRIB_PACK MAX_MAG_INDEX_DATA;
#else
typedef struct max_mag_index_data {
        u_int8_t
            max_mag_bits01:2,
            bmap_wt:6;
        u_int8_t max_mag_bits29;
        u_int8_t
            max_index_bits05:6,
            max_mag_bits1110:2;
}__ATTRIB_PACK MAX_MAG_INDEX_DATA;
#endif
typedef struct ht20_fft_packet {
    HT20_BIN_MAG_DATA lower_bins;
    MAX_MAG_INDEX_DATA  lower_bins_max;
    u_int8_t       max_exp;
} __ATTRIB_PACK HT20_FFT_PACKET;

typedef struct ht40_fft_packet {
    HT40_BIN_MAG_DATA lower_bins;
    HT40_BIN_MAG_DATA upper_bins;
    MAX_MAG_INDEX_DATA  lower_bins_max;
    MAX_MAG_INDEX_DATA  upper_bins_max;
    u_int8_t       max_exp;
} __ATTRIB_PACK HT40_FFT_PACKET;

#ifdef WIN32
#pragma pack(pop, spectral)
#endif

struct spectral_pulseparams {
        u_int64_t       p_time;                 /* time for start of pulse in usecs*/
        u_int8_t        p_dur;                  /* Duration of pulse in usecs*/
        u_int8_t        p_rssi;                 /* Duration of pulse in usecs*/
};

struct spectral_event {
    u_int32_t   se_ts;      /* Original 15 bit recv timestamp */
    u_int64_t   se_full_ts; /* 64-bit full timestamp from interrupt time */
    u_int8_t    se_rssi;    /* rssi of spectral event */
    u_int8_t    se_bwinfo;  /* rssi of spectral event */
    u_int8_t    se_dur;     /* duration of spectral pulse */
    u_int8_t    se_chanindex;   /* Channel of event */
    STAILQ_ENTRY(spectral_event)    se_list;    /* List of spectral events */
};


struct spectral_skb_event {
    struct sk_buff *sp_skb;
    struct nlmsghdr *sp_nlh;
    STAILQ_ENTRY(spectral_skb_event) spectral_skb_list;
};

struct spectral_stats {
        u_int32_t       num_spectral_detects;      /* total num. of spectral detects */
        u_int32_t       total_phy_errors;
        u_int32_t       owl_phy_errors;
        u_int32_t       pri_phy_errors;
        u_int32_t       ext_phy_errors;
        u_int32_t       dc_phy_errors;
        u_int32_t       early_ext_phy_errors;
        u_int32_t       bwinfo_errors;
        u_int32_t       datalen_discards;
        u_int32_t       rssi_discards;
        u_int64_t       last_reset_tstamp;
};

int spectral_get_noise_power(struct ath_spectral* spectral,
                             int rptcount,                  /* number of noise pwr reports required */
                             NOISE_PWR_CAL* cal_override,   /* cal debug override - may be NULL */
                             CHAIN_NOISE_PWR_INFO* ctl_c0,  /* reports for chain 0 control */
                             CHAIN_NOISE_PWR_INFO* ctl_c1,  /* reports for chain 1 control */
                             CHAIN_NOISE_PWR_INFO* ctl_c2,  /* reports for chain 2 control */
                             CHAIN_NOISE_PWR_INFO* ext_c0,  /* reports for chain 0 ext */
                             CHAIN_NOISE_PWR_INFO* ext_c1,  /* reports for chain 1 ext */
                             CHAIN_NOISE_PWR_INFO* ext_c2); /* reports for chain 2 ext */

typedef qdf_spinlock_t spectralq_lock_t;

#define  SPECTRAL_LOCK_INIT(_ath_spectral)     qdf_spinlock_create(&(_ath_spectral)->ath_spectral_lock)
#define  SPECTRAL_LOCK_DESTROY(_ath_spectral)  qdf_spinlock_destroy(&(_ath_spectral)->ath_spectral_lock)
#define  SPECTRAL_LOCK(_ath_spectral)          qdf_spin_lock(&(_ath_spectral)->ath_spectral_lock)
#define  SPECTRAL_UNLOCK(_ath_spectral)        qdf_spin_unlock(&(_ath_spectral)->ath_spectral_lock)

/* EACS stats from spectral samples */
typedef struct spectral_acs_stats_s
{
    int8_t nfc_ctl_rssi;
    int8_t nfc_ext_rssi;
    int8_t ctrl_nf;
    int8_t ext_nf;
} spectral_acs_stats_t;
/*
    Function Table
    Spectral module will access external APIs through
    this table. It will not call any of the external
    APIs directly
*/

typedef struct spectral_ops {
   u_int64_t (*get_tsf64)(void* arg);
   u_int32_t (*get_capability)(void* arg, enum spectral_capability_type type);

   u_int32_t (*set_rxfilter)(void* arg, int rxfilter);
   u_int32_t (*get_rxfilter)(void* arg);

   u_int32_t (*is_spectral_active)(void* arg);
   u_int32_t (*is_spectral_enabled)(void* arg);

   u_int32_t (*start_spectral_scan)(void *arg);
   u_int32_t (*stop_spectral_scan)(void *arg);

   u_int32_t (*get_extension_channel)(void* arg);

   int8_t (*get_ctl_noisefloor)(void* arg);
   int8_t (*get_ext_noisefloor)(void* arg);

   u_int32_t (*configure_spectral)(void* arg, struct spectral_config *params);
   u_int32_t (*get_spectral_config)(void* arg, struct spectral_config *params);

   u_int32_t (*get_ent_spectral_mask)(void* arg);

   u_int32_t (*get_mac_address)(void* arg, char* addr);
   u_int32_t (*get_current_channel)(void* arg);
   u_int32_t (*reset_hw)(void* arg);
   u_int32_t (*get_chain_noise_floor)(void* arg, int16_t* nfBuf);
   u_int32_t (*set_icm_active)(void* arg, int is_active);
   int16_t   (*get_nominal_nf)(void* arg, enum band_info band);
   int (*spectral_process_phyerr)(struct ath_spectral* spectral, u_int8_t* data, u_int32_t datalen,
                   SPECTRAL_RFQUAL_INFO* p_rfqual,
                   SPECTRAL_CHAN_INFO* p_chaninfo,
                   u_int64_t tsf64,
                   spectral_acs_stats_t *acs_stats);
}SPECTRAL_OPS, *P_SPECTRAL_OPS;

#define GET_SPECTRAL_OPS(spectral) ((struct spectral_ops*)(&((spectral)->spectral_ops)))
#define GET_SPECTRAL_ATHSOFTC(spectral) ((struct ath_softc*)(((struct ath_spectral*)spectral)->ath_softc_handle))
#define GET_SPECTRAL_FROM_IC(ic)  ((struct ath_spectral*)((ic)->ic_spectral))
#undef spectral_ops_not_registered
#define spectral_ops_not_registered(str)   qdf_print(KERN_INFO "SPECTRAL : %s not registered", (str))

typedef struct ath_spectral {
    struct wlan_objmgr_pdev *pdev_obj;
    struct ieee80211com*    ic;
    void*                   ath_softc_handle;
    struct spectral_ops     spectral_ops;
    struct spectral_caps capability;

    qdf_spinlock_t        ath_spectral_lock;

    int16_t                 spectral_curchan_radindex;  /* cur. channel spectral index */
    int16_t                 spectral_extchan_radindex;  /* extension channel spectral index */

    u_int32_t               spectraldomain;             /* cur. SPECTRAL domain */
    u_int32_t               spectral_proc_phyerr;       /* Flags for Phy Errs to process */

    struct spectral_config      spectral_defaultparams;     /* Default phy params per spectral state */

    struct spectral_stats        ath_spectral_stats;    /* SPECTRAL related stats */
    struct spectral_event        *events;               /* Events structure */

    unsigned int
                    sc_spectral_ext_chan_ok:1,          /* Can spectral be detected on the extension channel? */
                    sc_spectral_combined_rssi_ok:1,     /* Can use combined spectral RSSI? */
                    sc_spectral_20_40_mode:1,           /* Is AP in 20-40 mode? */
                    sc_spectral_noise_pwr_cal:1,        /* noise power cal reqd ? */
                    sc_spectral_non_edma:1;             /* Is the spectral capable
                                                           device Non-EDMA? */



    int upper_is_control;
    int upper_is_extension;
    int lower_is_control;
    int lower_is_extension;

    u_int8_t           sc_spectraltest_ieeechan;   /* IEEE channel number to return to after a spectral mute test */

    struct sock             *spectral_sock;
    struct sk_buff          *spectral_skb;
    struct nlmsghdr         *spectral_nlh;
    u_int32_t               spectral_pid;

    STAILQ_HEAD(,spectral_skb_event) spectral_skbq;
    qdf_spinlock_t           spectral_skbqlock;

    int spectral_numbins;
    int spectral_fft_len;
    int spectral_data_len;

    /* For 11ac chipsets prior to AR900B version 2.0, a max of 512 bins are
     * delivered.  However, there can be additional bins reported for AR900B
     * version 2.0 and QCA9984 as described next:
     *
     * AR900B version 2.0: An additional tone is processed on the right hand
     * side in order to facilitate detection of radar pulses out to the extreme
     * band-edge of the channel frequency. Since the HW design processes four
     * tones at a time, this requires one additional Dword to be added to the
     * search FFT report.
     *
     * QCA9984: When spectral_scan_rpt_mode=2, i.e 2-dword summary +
     * 1x-oversampled bins (in-band) per FFT, then 8 more bins (4 more on left
     * side and 4 more on right side)are added.
     */
    int lb_edge_extrabins;   /* Number of extra bins on left band edge.*/
    int rb_edge_extrabins;   /* Number of extra bins on right band edge.*/

    int spectral_max_index_offset;
    int spectral_upper_max_index_offset;
    int spectral_lower_max_index_offset;
    int spectral_dc_index;
    int send_single_packet;
    int spectral_sent_msg;
    int classify_scan;

    os_timer_t              classify_timer;

    SPECTRAL_BURST          current_burst;
    SPECTRAL_BURST          prev_burst;

    struct spectral_config      params;
    struct spectral_classifier_params classifier_params;

    struct ss               bd_upper;
    struct ss               bd_lower;
    int                     last_capture_time;
    struct interf_src_rsp   lower_interf_list;
    struct interf_src_rsp   upper_interf_list;

    int num_spectral_data;
    int total_spectral_data;
    int max_rssi;
    int detects_control_channel;
    int detects_extension_channel;
    int detects_below_dc;
    int detects_above_dc;
    int sc_scanning;
    int sc_spectral_scan;
    int sc_spectral_full_scan;

    u_int64_t scan_start_tstamp;

    u_int32_t last_tstamp;
    u_int32_t first_tstamp;
    u_int32_t spectral_samp_count;
    u_int32_t sc_spectral_samp_count;

    /* noise pwr related data */
    int                     noise_pwr_reports_reqd;
    int                     noise_pwr_reports_recv;
    qdf_spinlock_t              noise_pwr_reports_lock;
    CHAIN_NOISE_PWR_INFO    *noise_pwr_chain_ctl[ATH_HOST_MAX_ANTENNA];
    CHAIN_NOISE_PWR_INFO    *noise_pwr_chain_ext[ATH_HOST_MAX_ANTENNA];

    struct spectral_chan_stats   chaninfo[MAX_NUM_CHANNELS];

    /* Latest TSF Value */
    u_int64_t   tsf64;

    u_int32_t   ch_width;   /* Indicates Channel Width 20/40/80/160 MHz */

    /* Diagnostic statistics */
    struct spectral_diag_stats diag_stats;

    bool is_160_format;                /* Indicates whether information provided
                                          by HW is in altered format for
                                          802.11ac 160/80+80 MHz support (QCA9984
                                          onwards) */

    bool is_lb_edge_extrabins_format;  /* Indicates whether information provided
                                          by HW has 4 extra bins, at left
                                          band edge, for report mode 2 */

    bool is_rb_edge_extrabins_format;  /* Indicates whether information provided
                                          by HW has 4 extra bins, at right
                                          band edge, for report mode 2 */

    bool is_sec80_rssi_war_required;   /* Indicates whether the software workaround
                                          is required to obtain approximate combined
                                          RSSI for secondary 80Mhz segment */
    u_int8_t hdr_sig_exp;
    u_int8_t tag_sscan_summary_exp;
    u_int8_t tag_sscan_fft_exp;
    u_int8_t tlvhdr_size;
}ATH_SPECTRAL_T;

struct samp_msg_params {
    int8_t      rssi;
    int8_t      rssi_sec80;
    int8_t      lower_rssi;
    int8_t      upper_rssi;
    int8_t      chain_ctl_rssi[ATH_HOST_MAX_ANTENNA];
    int8_t      chain_ext_rssi[ATH_HOST_MAX_ANTENNA];

    uint16_t    bwinfo;
    uint16_t    datalen;
    uint16_t    datalen_sec80;
    uint32_t    tstamp;
    uint32_t    last_tstamp;
    uint16_t    max_mag;
    uint16_t    max_mag_sec80;
    uint16_t    max_index;
    uint16_t    max_index_sec80;
    uint8_t     max_exp;

    int         peak;
    int         pwr_count;
    int         pwr_count_sec80;

    int8_t      nb_lower;
    int8_t      nb_upper;
    uint16_t    max_lower_index;
    uint16_t    max_upper_index;

    u_int8_t    **bin_pwr_data;
    u_int8_t    **bin_pwr_data_sec80;
    u_int16_t   freq;
    u_int16_t   vhtop_ch_freq_seg1;
    u_int16_t   vhtop_ch_freq_seg2;
    u_int16_t   freq_loading;

    int16_t noise_floor;
    int16_t noise_floor_sec80;
    struct interf_src_rsp interf_list;
    struct spectral_classifier_params classifier_params;
    struct ath_softc *sc;
};


static inline
struct ath_spectral *get_spectral_handle_from_pdev(struct wlan_objmgr_pdev *pdev)
{
    struct ath_spectral *spectral = NULL;
    struct wlan_objmgr_psoc *psoc = NULL;

    psoc = wlan_pdev_get_psoc(pdev);
    spectral = (struct ath_spectral *)psoc->soc_cb.rx_ops.sptrl_rx_ops.sptrlro_get_target_handle(pdev);

    return spectral;
}

static inline
uint32_t spectral_send_phydata(struct wlan_objmgr_pdev *pdev,
                               struct sock *sock, qdf_nbuf_t nbuf)
{
	return netlink_broadcast(sock, nbuf, 0, 1, GFP_ATOMIC);
}

int pdev_spectral_postinit(struct ath_softc* sc);
void pdev_spectral_deinit_da(struct wlan_objmgr_pdev *pdev);
void da_ath_sptrl_register_tx_ops(struct wlan_lmac_if_tx_ops *tx_ops);
extern int spectral_process_spectralevent(struct ath_softc *sc, HAL_CHANNEL *chan);
extern int spectral_scan_enable(struct ath_spectral* spectral, u_int8_t priority);
extern int spectral_scan_enable_params(struct ath_spectral *spectral, struct spectral_config* spectral_params);
extern int spectral_check_chirping(struct ath_softc *sc, struct ath_desc *ds, int is_ctl, int is_ext, int *slope, int *is_dc);
void spectral_get_thresholds(struct wlan_objmgr_pdev *pdev, struct spectral_config *param);
void spectral_record_chan_info(struct ath_spectral *spectral,
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

extern void  pdev_spectral_deinit(struct wlan_objmgr_pdev *pdev);
extern void  spectral_clear_stats(struct ath_spectral* spectral);
extern void  ath_process_spectraldata(struct ath_spectral *sc, struct ath_buf *bf, struct ath_rx_status *rxs, u_int64_t fulltsf);

/* NETLINK related declarations */
#ifdef SPECTRAL_USE_NETLINK_SOCKETS
    #if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,31))
        void spectral_nl_data_ready(struct sock *sk, int len);
    #else
        void spectral_nl_data_ready(struct sk_buff *skb);
    #endif /* VERSION CHECK */
#endif /* SPECTRAL_USE_NETLINK_SOCKETS defined */

extern int spectral_init_netlink(struct ath_spectral* spectral);
extern int spectral_destroy_netlink(struct ath_spectral* spectral);

extern void spectral_unicast_msg(struct ath_spectral* spectral);
extern void spectral_bcast_msg(struct ath_spectral* spectral);
extern void spectral_prep_skb(struct ath_spectral* spectral);
extern void spectral_skb_dequeue(unsigned long data);

#ifdef SPECTRAL_CLASSIFIER_IN_KERNEL
extern void init_classifier(struct ath_softc *sc);
extern void classifier_initialize(struct ss *spectral_lower, struct ss *spectral_upper);
extern void classifier(struct ss *bd, int timestamp, int last_capture_time, int rssi, int narrowband, int peak_index);
OS_TIMER_FUNC(spectral_classify_scan);
#endif

extern void enable_beacons(struct ath_softc *sc);
extern void disable_beacons(struct ath_softc *sc);

OS_TIMER_FUNC(spectral_debug_timeout);

extern void print_classifier_counts(struct ath_softc *sc, struct ss *bd, const char *print_str);
extern void print_detection(struct ath_softc *sc);
extern void print_phy_err_stats(struct ath_softc *sc);
extern void print_fft_ht40_bytes(HT40_FFT_PACKET *fft_40, struct ath_softc *sc);
extern void print_config_regs(struct ath_softc *sc);
extern void printNoiseFloor(struct ath_softc *sc);
extern void print_ht40_bin_mag_data(HT40_BIN_MAG_DATA *bmag, struct ath_softc *sc);
extern void print_max_mag_index_data(MAX_MAG_INDEX_DATA *imag, struct ath_softc *sc);
extern void print_fft_ht40_packet(HT40_FFT_PACKET *fft_40, struct ath_softc *sc);
extern void print_hex_fft_ht20_packet(HT20_FFT_PACKET *fft_20, struct ath_softc *sc);

extern void fake_ht40_data_packet(HT40_FFT_PACKET *ht40pkt, struct ath_softc *sc);
extern void send_fake_ht40_data(struct ath_softc *sc);
extern void process_mag_data(MAX_MAG_INDEX_DATA *imag, u_int16_t *mmag, u_int8_t *bmap_wt, u_int8_t *max_index);
extern void process_fft_ht20_packet(HT20_FFT_PACKET *fft_20, struct ath_softc *sc, u_int16_t *max_mag, u_int8_t* max_index, int *narrowband, int8_t *rssi, int *bmap);
extern void process_fft_ht40_packet(HT40_FFT_PACKET *fft_40, struct ath_softc *sc, u_int16_t *max_mag_lower, u_int8_t* max_index_lower, u_int16_t *max_mag_upper, u_int8_t* max_index_upper, u_int16_t *max_mag, u_int8_t* max_index, int *narrowband_lower, int *narrowband_upper, int8_t *rssi_lower, int8_t*rssi_upper, int *bmap_lower, int *bmap_upper);
extern u_int8_t return_max_value(u_int8_t* datap, u_int8_t numdata, u_int8_t *max_index, struct ath_softc *sc, u_int8_t *strong_bins);

/* SAMP declarations - SPECTRAL ANALYSIS MESSAGING PROTOCOL */
extern void spectral_add_interf_samp_msg(struct samp_msg_params *params, struct ath_softc *sc);
extern void spectral_create_samp_msg(struct ath_spectral* spectral, struct samp_msg_params *params);
extern void spectral_send_intf_found_msg(struct ath_spectral* spectral, u_int16_t cw_int, u_int32_t dfs_enabled);
extern void spectral_create_msg(struct ath_softc *sc, uint16_t rssi, uint16_t bwinfo, uint16_t datalen, uint32_t tstamp, uint16_t max_mag, uint16_t max_index, int peak);
extern void print_samp_msg (struct spectral_samp_msg *samp, struct ath_softc *sc);

/* Spectral commands that can be issued from the commandline using spectraltool */
extern int start_spectral_scan_da(struct wlan_objmgr_pdev *pdev);
extern void start_classify_scan(struct ath_softc *sc);
extern void stop_current_scan_da(struct wlan_objmgr_pdev *pdev);
extern int spectral_set_thresholds(struct wlan_objmgr_pdev *pdev, const u_int32_t threshtype,const u_int32_t value);

extern int8_t fix_maxindex_inv_only (struct ath_spectral *spectral, u_int8_t val);
extern int8_t adjust_rssi_with_nf_noconv_dbm (struct ath_spectral *spectral, int8_t rssi, int upper, int lower);
extern int8_t adjust_rssi_with_nf_conv_dbm (struct ath_spectral *spectral, int8_t rssi, int upper, int lower);
extern int8_t adjust_rssi_with_nf_dbm (struct ath_spectral* spectral, int8_t rssi, int upper, int lower, int convert_to_dbm);
extern int8_t fix_rssi_for_classifier (struct ath_spectral* spectral, u_int8_t rssi_val, int upper, int lower);
extern int8_t fix_rssi_inv_only (u_int8_t rssi_val);
extern u_int32_t spectral_round(int32_t val);

extern int is_spectral_phyerr(struct ath_spectral *spectral, struct ath_buf *buf, struct ath_rx_status* rxs);

extern int spectral_check_hw_capability(struct ath_spectral *spectral);
extern void spectral_register_funcs(void* arg, SPECTRAL_OPS* p_sops);
extern int spectral_dump_fft(u_int8_t* pfft, int fftlen);
extern int spectral_send_tlv_to_host(struct ath_spectral* spectral, u_int8_t* data, u_int32_t datalen);
/* BEGIN EACS related declarations */
extern void spectral_read_mac_counters(struct ath_softc *sc);
extern void init_chan_loading(struct ieee80211com *ic, int current_channel, int ext_channel);
extern void update_eacs_counters(struct ieee80211com *ic, int8_t nfc_ctl_rssi,
                                      int8_t nfc_ext_rssi, int8_t ctrl_nf, int8_t ext_nf);
extern int8_t get_nfc_ctl_rssi(struct ath_spectral* spectral, int8_t rssi, int8_t *ctl_nf);
extern int8_t get_nfc_ext_rssi(struct ath_spectral* spectral, int8_t rssi, int8_t *ext_nf );

extern int get_freq_loading(struct ieee80211com *ic);
extern void print_spectral_params(struct spectral_config *p);

#ifdef __ATTRIB_PACK
#undef __ATTRIB_PACK
#endif

#endif  /* _SPECTRAL_H_ */
