/*
 * Copyright (c) 2011,2017 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * Notifications and licenses are retained for attribution purposes only.
 */
/*
 * Copyright (c) 2002-2005 Sam Leffler, Errno Consulting
 * Copyright (c) 2002-2005 Atheros Communications, Inc.
 * Copyright (c) 2010, Atheros Communications Inc.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the following conditions are met:
 * 1. The materials contained herein are unmodified and are used
 *    unmodified.
 * 2. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following NO
 *    ''WARRANTY'' disclaimer below (''Disclaimer''), without
 *    modification.
 * 3. Redistributions in binary form must reproduce at minimum a
 *    disclaimer similar to the Disclaimer below and any redistribution
 *    must be conditioned upon including a substantially similar
 *    Disclaimer requirement for further binary redistribution.
 * 4. Neither the names of the above-listed copyright holders nor the
 *    names of any contributors may be used to endorse or promote
 *    product derived from this software without specific prior written
 *    permission.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ''AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT,
 * MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE
 * FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGES.
 */

#ifndef _ATH_AH_INTERAL_H_
#define _ATH_AH_INTERAL_H_

#include "asf_print.h"  /* asf print */
#ifdef __KERNEL__
#include "qdf_mem.h" /* qdf_mem_zero */
#endif
#include "osif_fs.h" /* qdf_fs_read */
#include "ah.h"
#include <stdarg.h>

/*
 * Atheros Device Hardware Access Layer (HAL).
 *
 * Internal definitions.
 */
#define AH_NULL 0
#define AH_MIN(a,b) ((a)<(b)?(a):(b))
#define AH_MAX(a,b) ((a)>(b)?(a):(b))

#ifndef NBBY
#define NBBY    8           /* number of bits/byte */
#endif

#ifndef roundup
#define roundup(x, y)   ((((x)+((y)-1))/(y))*(y))  /* to any y */
#endif

#ifndef howmany
#define howmany(x, y)   (((x)+((y)-1))/(y))
#endif

#ifndef offsetof
#define offsetof(type, field)   ((size_t)(&((type *)0)->field))
#endif

/*
 * Remove const in a way that keeps the compiler happy.
 * This works for gcc but may require other magic for
 * other compilers (not sure where this should reside).
 * Note that uintptr_t is C99.
 */
#ifndef __DECONST
#define __DECONST(type, var)    ((type)(uintptr_t)(const void *)(var))
#endif

/* 1024 microseconds per time-unit */
#define TU_TO_USEC(_tu)  ((_tu) << 10)
/* 128 microseconds per eighth of a time-unit */
#define USEC_TO_ONE_EIGHTH_TU(_usec)  ((_usec) >> 7)
#define ONE_EIGHTH_TU_TO_USEC(tu8) ((tu8) << 7)
/* convert eighths of TUs to TUs (round down) */
#define ONE_EIGHTH_TU_TO_TU(tu8) ((tu8) >> 3)
#define TU_TO_ONE_EIGHTH_TU(tu) ((tu) << 3)

typedef enum {
    START_ADHOC_NO_11A,     /* don't use 802.11a channel */
    START_ADHOC_PER_11D,    /* 802.11a + 802.11d ad-hoc */
    START_ADHOC_IN_11A,     /* 802.11a ad-hoc */
    START_ADHOC_IN_11B,     /* 802.11b ad-hoc */
} START_ADHOC_OPTION;

#ifdef WIN32
#pragma pack(push, ah_internal, 1)
#endif

typedef struct {
    u_int8_t    id;         /* element ID */
    u_int8_t    len;        /* total length in bytes */
    u_int8_t    cc[3];      /* ISO CC+(I)ndoor/(O)utdoor */
    struct {
        u_int8_t schan;     /* starting channel */
        u_int8_t nchan;     /* number channels */
        u_int8_t maxtxpwr;
    } band[4];              /* up to 4 sub bands */
}  __packed COUNTRY_INFO_LIST;

#ifdef WIN32
#pragma pack(pop, ah_internal)
#endif

typedef struct {
    u_int32_t   start;      /* first register */
    u_int32_t   end;        /* ending register or zero */
} HAL_REGRANGE;

/*
 * Transmit power scale factor.
 *
 * NB: This is not public because we want to discourage the use of
 *     scaling; folks should use the tx power limit interface.
 */
typedef enum {
    HAL_TP_SCALE_MAX    = 0,        /* no scaling (default) */
    HAL_TP_SCALE_50     = 1,        /* 50% of max (-3 dBm) */
    HAL_TP_SCALE_25     = 2,        /* 25% of max (-6 dBm) */
    HAL_TP_SCALE_12     = 3,        /* 12% of max (-9 dBm) */
    HAL_TP_SCALE_MIN    = 4,        /* min, but still on */
} HAL_TP_SCALE;


/* Regulatory domains */
typedef enum {
        HAL_REGDMN_FCC     = 0x10,
        HAL_REGDMN_MKK     = 0x40,
        HAL_REGDMN_ETSI    = 0x30,
}HAL_REG_DMNS;
#define HAL_REG_DMN_MASK    0xF0
#define is_reg_dmn_fcc(reg_dmn) (((reg_dmn & HAL_REG_DMN_MASK) == HAL_REGDMN_FCC)?1:0)
#define is_reg_dmn_etsi(reg_dmn)(((reg_dmn & HAL_REG_DMN_MASK) == HAL_REGDMN_ETSI)?1:0)
#define is_reg_dmn_mkk(reg_dmn) (((reg_dmn & HAL_REG_DMN_MASK) == HAL_REGDMN_MKK)?1:0)

 /*
  * Enums of vendors used to modify reg domain flags if necessary
  */
 typedef enum {
    HAL_VENDOR_MAVERICK    = 1,
 } HAL_VENDORS;

#define HAL_NF_CAL_HIST_LEN_FULL  5
#define HAL_NF_CAL_HIST_LEN_SMALL 1
#define NUM_NF_READINGS           6   /* 3 chains * (ctl + ext) */
#define NF_LOAD_DELAY             1000

typedef struct {
    u_int8_t    curr_index;
    int8_t      invalidNFcount; /* TO DO: REMOVE THIS! */
    int16_t     priv_nf[NUM_NF_READINGS];
} HAL_NFCAL_BASE;

typedef struct {
    HAL_NFCAL_BASE base;
    int16_t     nf_cal_buffer[HAL_NF_CAL_HIST_LEN_SMALL][NUM_NF_READINGS];
} HAL_NFCAL_HIST_SMALL;

typedef struct {
    HAL_NFCAL_BASE base;
    int16_t     nf_cal_buffer[HAL_NF_CAL_HIST_LEN_FULL][NUM_NF_READINGS];
} HAL_NFCAL_HIST_FULL;

#ifdef ATH_NF_PER_CHAN
typedef HAL_NFCAL_HIST_FULL HAL_CHAN_NFCAL_HIST;
#else
typedef HAL_NFCAL_HIST_SMALL HAL_CHAN_NFCAL_HIST;
#endif

#if ATH_SUPPORT_CAL_REUSE
#ifndef MAX_IQ_MEASUREMENT
#define MAX_IQ_MEASUREMENT 8
#endif
#define MAX_BB_CL_TABLE_ENTRY   16
#endif

#if ATH_SUPPORT_RADIO_RETENTION
#define AH_RTT_MAX_NUM_TABLE_ENTRY      6
#define AH_RTT_MAX_NUM_HIST             3
typedef struct {
    u_int32_t table[AH_RTT_MAX_NUM_HIST][AH_MAX_CHAINS][AH_RTT_MAX_NUM_TABLE_ENTRY];
    u_int8_t saved;
    u_int8_t last;
} HAL_RADIO_RETENTION;
#endif /* ATH_SUPPORT_RADIO_RETENTION */

/*
 * Internal form of a HAL_CHANNEL.  Note that the structure
 * must be defined such that you can cast references to a
 * HAL_CHANNEL so don't shuffle the first two members.
 */
typedef struct {
    u_int16_t   channel;    /* NB: must be first for casting */
    u_int32_t channel_flags;
    u_int8_t  priv_flags;
    int8_t    max_reg_tx_power;
    int8_t    max_tx_power;
    int8_t    min_tx_power; /* as above... */
    int8_t      max_rate_power;     /* max power for all rates */
    int8_t      min_rate_power;     /* min power for all rates */
    u_int8_t    regClassId; /* Regulatory class id */
    u_int8_t  paprd_done:1,           /* 1: PAPRD DONE, 0: PAPRD Cal not done */
              paprd_table_write_done:1; /* 1: DONE, 0: Cal data write not done */
    bool      bssSendHere;
    u_int8_t    gainI;
    bool      iqCalValid;
    int32_t   cal_valid;
    bool      one_time_cals_done;
#if ATH_SUPPORT_CAL_REUSE
    u_int32_t num_measures[AH_MAX_CHAINS];
    int32_t   tx_corr_coeff[MAX_IQ_MEASUREMENT][AH_MAX_CHAINS];
    bool      one_time_txiqcal_done;
    u_int32_t tx_clcal[AH_MAX_CHAINS][MAX_BB_CL_TABLE_ENTRY];
    bool      one_time_txclcal_done;
#endif
    int8_t    i_coff;
    int8_t    q_coff;
    int16_t   raw_noise_floor;
    int16_t     noiseFloorAdjust;
    int8_t      antennaMax;
    u_int32_t   regDmnFlags;    /* Flags for channel use in reg */
    u_int32_t conformance_test_limit; /* conformance test limit from reg domain */
    u_int64_t       ah_tsf_last;            /* tsf @ which time accured is computed */
    u_int64_t       ah_channel_time;        /* time on the channel  */
    u_int16_t   mainSpur;       /* cached spur value for this cahnnel */
    u_int64_t dfs_tsf;
    /*
     * Each channels has a NF history buffer.
     * If ATH_NF_PER_CHAN is defined, this history buffer is full-sized
     * (HAL_NF_CAL_HIST_MAX elements).  Else, this history buffer only
     * stores a single element.
     */
    HAL_CHAN_NFCAL_HIST  nf_cal_hist;
#if ATH_SUPPORT_RADIO_RETENTION
    HAL_RADIO_RETENTION rtt;
#endif
} HAL_CHANNEL_INTERNAL;

typedef struct {
        u_int           hal_chan_spread_support         : 1,
                        hal_sleep_after_beacon_broken   : 1,
                        hal_burst_support               : 1,
                        hal_fast_frames_support         : 1,
                        hal_chap_tuning_support         : 1,
                        hal_turbo_g_support             : 1,
                        hal_turbo_prime_support         : 1,
                        hal_xr_support                  : 1,
                        hal_mic_aes_ccm_support         : 1,
                        hal_mic_ckip_support            : 1,
                        hal_mic_tkip_support            : 1,
                        hal_cipher_aes_ccm_support      : 1,
                        hal_cipher_ckip_support         : 1,
                        hal_cipher_tkip_support         : 1,
                        hal_ps_poll_broken              : 1,
                        hal_veol_support                : 1,
                        hal_bss_id_mask_support         : 1,
                        hal_mcast_key_srch_support      : 1,
                        hal_tsf_add_support             : 1,
                        hal_chan_half_rate              : 1,
                        hal_chan_quarter_rate           : 1,
                        hal_ht_support                  : 1,
                        hal_ht20_sgi_support            : 1,
                        hal_rx_stbc_support             : 1,
                        hal_tx_stbc_support             : 1,
                        hal_gtt_support                 : 1,
                        hal_fast_cc_support             : 1,
                        hal_rf_silent_support           : 1,
                        hal_wow_support                 : 1,
                        hal_cst_support                 : 1,
                        hal_rifs_rx_support             : 1,
                        hal_rifs_tx_support             : 1,
                        halforce_ppm_support            : 1,
                        hal_ext_chan_dfs_support        : 1,
                        hal_use_combined_radar_rssi     : 1,
                        hal_hw_beacon_proc_support      : 1,
                        hal_auto_sleep_support          : 1,
                        hal_mbssid_aggr_support         : 1,
                        hal4kb_split_trans_support      : 1,
                        hal_wow_match_pattern_exact     : 1,
                        hal_bt_coex_support             : 1,
                        hal_gen_timer_support           : 1,
                        hal_wow_pattern_match_dword     : 1,
                        hal_wps_push_button             : 1,
                        hal_wep_tkip_aggr_support       : 1,
#if WLAN_SPECTRAL_ENABLE
                        hal_spectral_scan               : 1,
#endif
#if ATH_SUPPORT_RAW_ADC_CAPTURE
                        hal_raw_adc_capture             : 1,
#endif
                        hal_enhanced_dma_support        : 1,
#ifdef ATH_SUPPORT_DFS
                        hal_enhanced_dfs_support        : 1,
#endif
                        hal_isr_rac_support             : 1,
                        hal_cfend_fix_support           : 1,
                        hal_bt_coex_aspm_war            : 1,
#ifdef ATH_SUPPORT_TxBF
                        hal_tx_bf_support               : 1,
#endif
                        hal_aggr_extra_delim_war        : 1,
                        hal_rx_tx_abort_support         : 1,
                        hal_paprd_enabled               : 1,
                        hal_hw_uapsd_trig               : 1,
                        hal_ant_div_comb_support        : 1,
                        hal49Ghz                        : 1,
                        hal_ldpc_support                : 2,
                        hal_enable_apm                  : 1,
                        hal_pcie_lcr_extsync_en         : 1,
#if ATH_WOW_OFFLOAD
                        hal_wow_gtk_offload_support     : 1,
                        hal_wow_arp_offload_support     : 1,
                        hal_wow_ns_offload_support      : 1,
                        hal_wow_4way_hs_wakeup_support  : 1,
                        hal_wow_acer_magic_support      : 1,
                        hal_wow_acer_swka_support       : 1,
#endif /* ATH_WOW_OFFLOAD */
                        hal_mci_support                 : 1,
#if ATH_SUPPORT_WRAP
                        hal_proxy_sta_rx_war            : 1,
#endif
                        hal_radio_retention_support     : 1;
        u_int32_t       hal_wireless_modes;
        u_int16_t       hal_total_queues;
        u_int16_t       hal_key_cache_size;
        u_int16_t       hal_low_5ghz_chan, hal_high_5ghz_chan;
        u_int16_t       hal_low_2ghz_chan, hal_high_2ghz_chan;
        u_int16_t       hal_num_mr_retries;
        u_int16_t       hal_rts_aggr_limit;
        u_int16_t       hal_tx_trig_level_max;
        u_int16_t       hal_reg_cap;
        u_int16_t       hal_wep_tkip_aggr_num_tx_delim;
        u_int16_t       hal_wep_tkip_aggr_num_rx_delim;
        u_int8_t        hal_wep_tkip_max_ht_rate;
        u_int8_t        hal_tx_chain_mask;
        u_int8_t        hal_rx_chain_mask;
        u_int8_t        hal_num_gpio_pins;
        u_int8_t        hal_num_ant_cfg_2ghz;
        u_int8_t        hal_num_ant_cfg_5ghz;
        u_int8_t        hal_num_tx_maps;
        u_int8_t        hal_tx_desc_len;
        u_int8_t        hal_tx_status_len;
        u_int8_t        hal_rx_status_len;
        u_int8_t        hal_rx_hp_depth;
        u_int8_t        hal_rx_lp_depth;
        u_int8_t        halintr_mitigation;
        u_int32_t       hal_mfp_support;
        u_int8_t        hal_proxy_sta_support;
        u_int8_t        hal_rx_desc_timestamp_bits;
        u_int16_t       hal_ani_poll_interval; /* ANI poll interval in ms */
        u_int16_t       hal_channel_switch_time_usec;
        u_int8_t        hal_pcie_lcr_offset;
        u_int8_t        hal_lmac_smart_ant;
#if ATH_SUPPORT_WAPI
        u_int8_t        hal_wapi_max_tx_chains;
        u_int8_t        hal_wapi_max_rx_chains;
#endif
#if ATH_ANT_DIV_COMB
        u_int8_t        hal_ant_div_comb_support_org;
#endif /* ATH_ANT_DIV_COMB */
} HAL_CAPABILITIES;

/* Serialize Register Access Mode */
typedef enum {
    SER_REG_MODE_OFF    = 0,
    SER_REG_MODE_ON     = 1,
    SER_REG_MODE_AUTO   = 2,
} SER_REG_MODE;

/*****
** HAL Configuration
**
** This type contains all of the "factory default" configuration
** items that may be changed during initialization or operation.
** These were formerly global variables which are now PER INSTANCE
** values that are used to control the operation of the specific
** HAL instance
*/

typedef struct {
    int         ath_hal_dma_beacon_response_time;
    int         ath_hal_sw_beacon_response_time;
    int         ath_hal_additional_swba_backoff;
    int         ath_hal_6mb_ack;
    int         ath_hal_cwm_ignore_ext_cca;
    int16_t     ath_hal_cca_detection_level;
    u_int16_t   ath_hal_cca_detection_margin;
#ifdef ATH_FORCE_BIAS
    /* workaround Fowl bug for orientation sensitivity */
    int         ath_hal_forceBias;
    int         ath_hal_forceBiasAuto ;
#endif
    u_int8_t    ath_hal_soft_eeprom;
    u_int8_t    ath_hal_pcie_power_save_enable;
    u_int8_t    ath_hal_pcie_ser_des_write;
    u_int8_t    ath_hal_pcieL1SKPEnable;
    u_int8_t    ath_hal_pcie_clock_req;
#define         AR_PCIE_PLL_PWRSAVE_CONTROL               (1<<0)
#define         AR_PCIE_PLL_PWRSAVE_ON_D3                 (1<<1)
#define         AR_PCIE_PLL_PWRSAVE_ON_D0                 (1<<2)

    u_int8_t    ath_hal_desc_tpc;
    u_int32_t   ath_hal_pcie_waen; /****************/
    u_int32_t   ath_hal_pcieDetach; /* clear bit 14 of AR_WA when detach */
    int         ath_hal_pciePowerReset;
    u_int8_t    ath_hal_pcieRestore;
    u_int8_t    ath_hal_pll_pwr_save;
    u_int8_t	ath_hal_analogShiftRegDelay;
    u_int8_t    ath_hal_ht_enable;
#if ATH_SUPPORT_MCI
    u_int32_t   ath_hal_mci_config;
#endif
#if ATH_SUPPORT_RADIO_RETENTION
    u_int32_t   ath_hal_rtt_ctrl;
#endif
#if ATH_SUPPORT_CAL_REUSE
    u_int32_t   ath_hal_cal_reuse;
#endif
#if ATH_WOW_OFFLOAD
    u_int32_t   ath_hal_wow_offload;
#endif
    u_int8_t    ath_hal_redchn_maxpwr;
#ifdef ATH_SUPERG_DYNTURBO
    int         ath_hal_disableTurboG;
#endif
    u_int32_t   ath_hal_ofdmTrigLow;
    u_int32_t   ath_hal_ofdmTrigHigh;
    u_int32_t   ath_hal_cckTrigHigh;
    u_int32_t   ath_hal_cckTrigLow;
    u_int32_t   ath_hal_enable_ani;
    u_int32_t   ath_hal_enable_adaptiveCCAThres;
    u_int8_t    ath_hal_noiseImmunityLvl;
    u_int32_t   ath_hal_ofdmWeakSigDet;
    u_int32_t   ath_hal_cckWeakSigThr;
    u_int8_t    ath_hal_spurImmunityLvl;
    u_int8_t    ath_hal_firStepLvl;
    int8_t      ath_hal_rssiThrHigh;
    int8_t      ath_hal_rssiThrLow;
    u_int16_t   ath_hal_diversity_control;
    u_int16_t   ath_hal_antenna_switch_swap;
    int         ath_hal_rifs_enable;
    int         ath_hal_serialize_reg_mode; /* serialize all reg accesses mode */
    int         ath_hal_intr_mitigation_rx;
    int         ath_hal_intr_mitigation_tx;
    int         ath_hal_fastClockEnable;
#if ATH_SUPPORT_FAST_CC
    int         ath_hal_fast_channel_change;
#endif

#if AH_DEBUG || AH_PRINT_FILTER
    int         ath_hal_debug;
#endif
#define         SPUR_DISABLE        0
#define         SPUR_ENABLE_IOCTL   1
#define         SPUR_ENABLE_EEPROM  2
#define         AR_EEPROM_MODAL_SPURS   5
#define         AR_SPUR_5413_1      1640    /* Freq 2464 */
#define         AR_SPUR_5413_2      1200    /* Freq 2420 */
#define         AR_NO_SPUR      0x8000
#define         AR_BASE_FREQ_2GHZ   2300
#define         AR_BASE_FREQ_5GHZ   4900
#define         AR_SPUR_FEEQ_BOUND_HT40  19
#define         AR_SPUR_FEEQ_BOUND_HT20  10

    int         ath_hal_spur_mode;
    u_int16_t   ath_hal_spur_chans[AR_EEPROM_MODAL_SPURS][2];
    u_int8_t    ath_hal_defaultAntCfg;
    u_int8_t    ath_hal_enable_msi;
    u_int32_t   ath_hal_mfp_support;
    u_int32_t   ath_hal_fullResetWarEnable;
    u_int8_t    ath_hal_legacyMinTxPowerLimit;
    u_int8_t    ath_hal_skip_eeprom_read;
    u_int8_t    ath_hal_txTrigLevelMax;
    u_int8_t    ath_hal_disPeriodicPACal;
    u_int32_t   ath_hal_keep_alive_timeout;  /* in ms */
#ifdef ATH_SUPPORT_TxBF
    u_int8_t    ath_hal_cvtimeout;
    u_int16_t   ath_hal_txbf_ctl;
#define TXBF_CTL_IM_BF                        0x01
#define TXBF_CTL_IM_BF_S                      0
#define TXBF_CTL_NON_EX_BF                    0x02
#define TXBF_CTL_NON_EX_BF_S                  1
#define TXBF_CTL_COMP_EX_BF                   0x04
#define TXBF_CTL_COMP_EX_BF_S                 2
#define TXBF_CTL_IM_BF_FB                     0x08
#define TXBF_CTL_IM_BF_FB_S                   3
#define TXBF_CTL_NON_EX_BF_IMMEDIATELY_RPT    0x10
#define TXBF_CTL_NON_EX_BF_IMMEDIATELY_RPT_S  4
#define TXBF_CTL_COMP_EX_BF_IMMEDIATELY_RPT   0x20
#define TXBF_CTL_COMP_EX_BF_IMMEDIATELY_RPT_S 5

#define TXBF_CTL_NON_EX_BF_DELAY_RPT          0x40
#define TXBF_CTL_NON_EX_BF_DELAY_RPT_S        6
#define TXBF_CTL_COMP_EX_BF_DELAY_RPT         0x80
#define TXBF_CTL_COMP_EX_BF_DELAY_RPT_S       7

#define TXBF_CTL_DISABLE_STEERING             0x100

#define Delay_Rpt                   1
#define Immediately_Rpt             2
#endif
    bool        ath_hal_lowerHTtxgain;
    u_int16_t   ath_hal_xpblevel5;
    bool        ath_hal_sta_update_tx_pwr_enable;
    bool        ath_hal_sta_update_tx_pwr_enable_S1;
    bool        ath_hal_sta_update_tx_pwr_enable_S2;
    bool        ath_hal_sta_update_tx_pwr_enable_S3;
    u_int32_t   ath_hal_war70c;
    u_int32_t   ath_hal_beacon_filter_interval;
    u_int32_t   ath_hal_ant_ctrl_comm2g_switch_enable;
    u_int32_t   ath_hal_ext_lna_ctl_gpio;
    u_int8_t    ath_hal_ext_atten_margin_cfg;
#if ATH_WOW_OFFLOAD
    u_int32_t   ath_hal_pcie_000;
    u_int32_t   ath_hal_pcie_008;
    u_int32_t   ath_hal_pcie_02c;
    u_int32_t   ath_hal_pcie_040;
    u_int32_t   ath_hal_pcie_70c;
#endif
    u_int8_t    ath_hal_show_bb_panic;
    u_int32_t    ath_hal_ch144;

#ifdef HOST_OFFLOAD
    /* EV 124660 : required to check only for WDS-ROOTAP for FO mode */
    bool         ath_hal_lower_rx_mitigation;
#endif
} HAL_OPS_CONFIG;

#if ATH_SUPPORT_RADIO_RETENTION
#define AH_RTT_ENABLE_RADIO_RETENTION   0x00000001
#define AH_RTT_OVERRIDE_OLD_HIST        0x00000002

    #define HAL_RTT_CTRL(_ah, _flag) \
        ((AH_PRIVATE(_ah)->ah_config.ath_hal_rtt_ctrl) & (_flag))
#endif

#if ATH_SUPPORT_CAL_REUSE
#define ATH_CAL_REUSE_ENABLE                0x00000001
#define ATH_CAL_REUSE_REDO_IN_FULL_RESET    0x00000002
#endif

#if ATH_WOW_OFFLOAD
#define HAL_WOW_OFFLOAD_SW_WOW_ENABLE       0x00000001
#define HAL_WOW_OFFLOAD_GTK_DISABLE         0x00000002
#define HAL_WOW_OFFLOAD_ARP_DISABLE         0x00000004
#define HAL_WOW_OFFLOAD_NS_DISABLE          0x00000008
#define HAL_WOW_OFFLOAD_4WAY_HS_DISABLE     0x00000010
#define HAL_WOW_OFFLOAD_ACER_MAGIC_DISABLE  0x00000020
#define HAL_WOW_OFFLOAD_ACER_SWKA_DISABLE   0x00000040
#define HAL_WOW_OFFLOAD_WAIT_BT_TIMEOUT     0x00FF0000
#define HAL_WOW_OFFLOAD_WAIT_BT_TIMEOUT_S   16
#define HAL_WOW_OFFLOAD_FORCE_BT_SLEEP      0x01000000
#define HAL_WOW_OFFLOAD_FORCE_GTK_ERR_WAKE  0x02000000
#define HAL_WOW_OFFLOAD_FORCE_AP_LOSS_WAKE  0x04000000
#define HAL_WOW_OFFLOAD_FORCE_4WAY_HS_WAKE  0x08000000
#define HAL_WOW_OFFLOAD_SW_NULL_DISABLE     0x10000000
#define HAL_WOW_OFFLOAD_DEVID_SWAR_DISABLE  0x20000000
#define HAL_WOW_OFFLOAD_KAFAIL_ENABLE       0x40000000
#define HAL_WOW_OFFLOAD_SET_4004_BIT14      0x80000000

    #define HAL_WOW_CTRL(_ah, _flag) \
        ((AH_PRIVATE(_ah)->ah_config.ath_hal_wow_offload) & (_flag))

    #define HAL_WOW_CTRL_WAIT_BT_TO(_ah) \
        (   HAL_WOW_CTRL((_ah), HAL_WOW_OFFLOAD_WAIT_BT_TIMEOUT) >> \
            HAL_WOW_OFFLOAD_WAIT_BT_TIMEOUT_S   )
#endif

#if !defined(_NET_IF_IEEE80211_H_) && !defined(_NET80211__IEEE80211_H_)
typedef enum {
    DFS_UNINIT_DOMAIN   = 0,    /* Uninitialized dfs domain */
    DFS_FCC_DOMAIN      = 1,    /* FCC3 dfs domain */
    DFS_ETSI_DOMAIN     = 2,    /* ETSI dfs domain */
    DFS_MKK4_DOMAIN = 3,    /* Japan dfs domain */
} HAL_DFS_DOMAIN;
#endif

#define MAX_PACAL_SKIPCOUNT    8
 typedef struct {
    int32_t   prevOffset; /* Previous value of PA offset value */
    int8_t    maxSkipCount;   /* Max No. of time PACAl can be skipped */
    int8_t    skipCount;  /* No. of time the PACAl to be skipped */
} HAL_PACAL_INFO;

/* nf - parameters related to noise floor filtering */
struct noise_floor_limits {
    int16_t nominal; /* what is the expected NF for this chip / band */
    int16_t min;     /* maximum expected NF for this chip / band */
    int16_t max;     /* minimum expected NF for this chip / band */
};

/*
 * Definitions for ah_flags in ath_hal_private
 */
#define AH_USE_EEPROM   0x1
#define AH_IS_HB63      0x2

/*
 * The ``private area'' follows immediately after the ``public area''
 * in the data structure returned by ath_hal_attach.  Private data are
 * used by device-independent code such as the regulatory domain support.
 * This data is not resident in the public area as a client may easily
 * override them and, potentially, bypass access controls.  In general,
 * code within the HAL should never depend on data in the public area.
 * Instead any public data needed internally should be shadowed here.
 *
 * When declaring a device-specific ath_hal data structure this structure
 * is assumed to at the front; e.g.
 *
 *  struct ath_hal_5212 {
 *      struct ath_hal_private  ah_priv;
 *      ...
 *  };
 *
 * It might be better to manage the method pointers in this structure
 * using an indirect pointer to a read-only data structure but this would
 * disallow class-style method overriding (and provides only minimally
 * better protection against client alteration).
 */
struct ath_hal_private {
    struct ath_hal  h;          /* public area */

    /* NB: all methods go first to simplify initialization */
    bool    (*ah_get_channel_edges)(struct ath_hal*,
                u_int16_t channel_flags,
                u_int16_t *lowChannel, u_int16_t *highChannel);
    u_int       (*ah_get_wireless_modes)(struct ath_hal*);
    bool    (*ah_eeprom_read)(struct ath_hal *, u_int off,
                u_int16_t *data);
    bool    (*ah_eeprom_write)(struct ath_hal *, u_int off,
                u_int16_t data);
    u_int       (*ah_eeprom_dump)(struct ath_hal *ah, void **pp_e);
    bool    (*ah_get_chip_power_limits)(struct ath_hal *,
                HAL_CHANNEL *, u_int32_t);
    int16_t     (*ah_get_nf_adjust)(struct ath_hal *,
                const HAL_CHANNEL_INTERNAL*);

    u_int16_t   (*ah_eeprom_get_spur_chan)(struct ath_hal *, u_int16_t, bool);
    HAL_STATUS  (*ah_get_rate_power_limit_from_eeprom)(struct ath_hal *ah,
                    u_int16_t freq, int8_t *max_rate_power, int8_t *min_rate_power);

    /*
     * Device revision information.
     */
    HAL_ADAPTER_HANDLE  ah_osdev;           /* back pointer to OS adapter handle */
    HAL_BUS_TAG         ah_st;              /* params for register r+w */
    HAL_BUS_HANDLE      ah_sh;              /* back pointer to OS bus handle */
    HAL_SOFTC           ah_sc;              /* back pointer to driver/os state */
    u_int32_t           ah_magic;           /* HAL Magic number*/
    u_int16_t           ah_devid;           /* PCI device ID */
    u_int32_t           ah_mac_version;      /* MAC version id */
    u_int16_t           ah_mac_rev;          /* MAC revision */
    u_int16_t           ah_phy_rev;          /* PHY revision */
    u_int16_t           ah_analog_5ghz_rev;   /* 2GHz radio revision */
    u_int16_t           ah_analog2GhzRev;   /* 5GHz radio revision */
    u_int32_t           ah_flags;           /* misc flags */
    HAL_VENDORS         ah_vendor;          /* Vendor ID */

#ifdef ATH_TX99_DIAG
    u_int16_t           ah_pwr_offset;
#endif
#if ATH_SUPPORT_CFEND
    /* number of good frames in current aggr */
    u_int8_t            ah_rx_num_cur_aggr_good;
#endif
    HAL_OPMODE          ah_opmode;          /* operating mode from reset */
    HAL_OPS_CONFIG      ah_config;          /* Operating Configuration */
    HAL_CAPABILITIES    ah_caps;            /* device capabilities */
    u_int32_t           ah_diagreg;         /* user-specified AR_DIAG_SW */
    int16_t             ah_power_limit;      /* tx power cap */
    u_int16_t           ah_max_power_level;   /* calculated max tx power */
    u_int               ah_tp_scale;         /* tx power scale factor */
    u_int16_t           ah_extra_txpow;     /* low rates extra-txpower */
    u_int16_t           ah_power_scale;     /* power scale factor */
#if AH_DEBUG || AH_PRINT_FILTER
    struct asf_print_ctrl ah_asf_print;     /* HAL asf_print control object */
    char                ah_print_itf_name[8]; /* interface name to print */
#endif
    /*
     * State for regulatory domain handling.
     */
    HAL_REG_DOMAIN      ah_current_rd;       /* Current regulatory domain */
    HAL_REG_DOMAIN      ah_current_rd_ext;    /* Regulatory domain Extension reg from EEPROM*/
    HAL_CTRY_CODE       ah_countryCode;     /* current country code */
    HAL_REG_DOMAIN      ah_currentRDInUse;  /* Current 11d domain in used */
    HAL_REG_DOMAIN      ah_currentRD5G;     /* Current 5G regulatory domain */
    HAL_REG_DOMAIN      ah_currentRD2G;     /* Current 2G regulatory domain */
    char                ah_iso[4];          /* current country iso + NULL */
    HAL_DFS_DOMAIN      ah_dfsDomain;       /* current dfs domain */
    START_ADHOC_OPTION  ah_adHocMode;    /* ad-hoc mode handling */
    bool            ah_commonMode;      /* common mode setting */
    /* NB: 802.11d stuff is not currently used */
    bool            ah_cc11d;       /* 11d country code */
    COUNTRY_INFO_LIST   ah_cc11dInfo;     /* 11d country code element */
    u_int               ah_nchan;       /* valid channels in list */
    HAL_CHANNEL_INTERNAL *ah_curchan;   /* current channel */

    u_int8_t            ah_coverageClass;       /* coverage class */
    bool            ah_regdomainUpdate;     /* regdomain is updated? */
    u_int64_t           ah_tsf_channel;         /* tsf @ which last channel change happened */
    bool            ah_cwCalRequire;
    /*
     * RF Silent handling; setup according to the EEPROM.
     */
    u_int16_t   ah_rfsilent;        /* GPIO pin + polarity */
    bool    ah_rfkillEnabled;   /* enable/disable RfKill */
    bool    ah_rfkillINTInit;   /* RFkill INT initialization */

    bool    ah_is_pci_express;    /* XXX: only used for ar5212 MAC (Condor/Hawk/Swan) */
    bool    ah_eepromTxPwr;     /* If EEPROM is programmed with tx_power (Condor/Swan/Hawk) */

    u_int8_t    ah_singleWriteKC;   /* write key cache one word at time */
    u_int8_t    ah_tx_trig_level;     /* Stores the current prefetch trigger
                                       level so that it can be applied across reset */
    struct noise_floor_limits *nfp; /* points to either nf_2GHz or nf_5GHz */
    struct noise_floor_limits nf_2GHz;
    struct noise_floor_limits nf_5GHz;
    int16_t nf_cw_int_delta; /* diff btwn nominal NF and CW interf threshold */
#ifndef ATH_NF_PER_CHAN
    HAL_NFCAL_HIST_FULL  nf_cal_hist;
#endif
#ifdef ATH_CCX
    u_int8_t        ser_no[13];
#endif
#if ATH_WOW
    u_int32_t   ah_wow_event_mask;   /* Mask to the AR_WOW_PATTERN_REG for the */
                                    /* enabled patterns and WOW Events. */
#endif
    u_int16_t   ah_devType;     /* Type of device */

    /* green_ap_ps_on -- Used to indicate that the Power save status is set */
    u_int16_t   green_ap_ps_on;

    u_int32_t   ah_dcs_enable;

    HAL_RSSI_TX_POWER   green_tx_status;   /* The status of Green TX function */

    HAL_PACAL_INFO ah_paCalInfo;

    u_int32_t   ah_bb_panic_timeout_ms;    /* 0 to disable Watchdog */
    u_int32_t   ah_bb_panic_last_status;   /* last occurrence status saved */
    u_int8_t    ah_reset_reason;        /* reason for ath hal reset */
    /*  Phy restart is disabled for Osprey,if any BB Panic due to RX_OFDM is seen */
    u_int8_t    ah_phyrestart_disabled;
#if AH_REGREAD_DEBUG
/* In order to save memory, the buff to record the register reads are
 * divided by 8 segments, with each the size is 8192 (0x2000),
 * and the ah_regaccessbase is from 0 to 7.
 */
    u_int32_t   ah_regaccess[8192];
    u_int8_t    ah_regaccessbase;
    u_int64_t   ah_regtsf_start;
#endif
	u_int8_t    ah_enable_keysearch_always;
	u_int8_t 	ah_intr_async_cause_read; /* is async_cause reg been read? */
	u_int8_t 	ah_intr_sync_cause_read;/* is sync_cause reg been read? */
	u_int32_t 	ah_intr_async_cause; /* last read of async_cause reg */
	u_int32_t 	ah_intr_sync_cause; /* last read of sync_cause reg */
    HAL_BUS_TYPE    ah_bustype;
    u_int32_t   ah_ob_db1[3];
    u_int32_t   ah_db2[3];
    /* switch flag for reg_write(original) and reg_write_delay(usb multiple) */
    u_int32_t   ah_reg_write_buffer_flag;
#ifdef ATH_SUPPORT_WAPI
    u_int8_t    ah_hal_keytype;        /* one of HAL_CIPHER */
#endif
#if ICHAN_WAR_SYNCH
    spinlock_t  ah_ichan_lock;
    bool        ah_ichan_set;
#endif
#ifdef ATH_SUPPORT_TxBF
    bool        ah_txbf_hw_cvtimeout;
    u_int32_t   ah_basic_set_buf;
    u_int8_t    ah_lowest_txrate;
#endif
#if ATH_DRIVER_SIM
    struct ath_hal_sim ah_hal_sim;
#endif
    u_int32_t   ah_mfp_qos;
    /* User defined antenna gain value */
    u_int32_t   ah_antenna_gain_2g;
    u_int32_t   ah_antenna_gain_5g;
};
#define AH_PRIVATE(_ah) ((struct ath_hal_private *)(_ah))

#ifdef ATH_NO_5G_SUPPORT
#define AH_MAX_CHANNELS 64
#else
#define AH_MAX_CHANNELS 256
#endif

struct ath_hal_private_tables {
    struct ath_hal_private priv;
    HAL_CHANNEL_INTERNAL ah_channels[AH_MAX_CHANNELS];  /* calculated channel list */
};
#define AH_TABLES(_ah) ((struct ath_hal_private_tables *)(_ah))

#define ath_hal_getChannelEdges(_ah, _cf, _lc, _hc) \
    AH_PRIVATE(_ah)->ah_get_channel_edges(_ah, _cf, _lc, _hc)
#define ath_hal_getWirelessModes(_ah) \
    AH_PRIVATE(_ah)->ah_get_wireless_modes(_ah)
#define ath_hal_eepromRead(_ah, _off, _data) \
    AH_PRIVATE(_ah)->ah_eeprom_read(_ah, (_off), (_data))
#define ath_hal_eepromWrite(_ah, _off, _data) \
    AH_PRIVATE(_ah)->ah_eeprom_write(_ah, (_off), (_data))
#define ath_hal_eepromDump(_ah, _data) \
    AH_PRIVATE(_ah)->ah_eeprom_dump(_ah, (_data))
#define ath_hal_gpio_cfg_output(_ah, _gpio, _signalType) \
    (_ah)->ah_gpio_cfg_output(_ah, (_gpio), (_signalType))
#define ath_hal_gpio_cfg_output_led_off(_ah, _gpio, _signalType) \
    (_ah)->ah_gpio_cfg_output_led_off(_ah, (_gpio), (_signalType))
#define ath_hal_gpio_cfg_input(_ah, _gpio) \
    (_ah)->ah_gpio_cfg_input(_ah, (_gpio))
#define ath_hal_gpioGet(_ah, _gpio) \
    (_ah)->ah_gpio_get(_ah, (_gpio))
#define ath_hal_gpioSet(_ah, _gpio, _val) \
    (_ah)->ah_gpio_set(_ah, (_gpio), (_val))
#define ath_hal_gpioSetIntr(_ah, _gpio, _ilevel) \
    (_ah)->ah_gpio_set_intr(_ah, (_gpio), (_ilevel))
#define ath_hal_getpowerlimits(_ah, _chans, _nchan) \
    AH_PRIVATE(_ah)->ah_get_chip_power_limits(_ah, (_chans), (_nchan))
#define ath_hal_getNfAdjust(_ah, _c) \
    AH_PRIVATE(_ah)->ah_get_nf_adjust(_ah, _c)

#if !defined(_NET_IF_IEEE80211_H_) && !defined(_NET80211__IEEE80211_H_)
/*
 * Stuff that would naturally come from _ieee80211.h
 */
#define IEEE80211_ADDR_LEN      6

#define IEEE80211_WEP_KEYLEN            5   /* 40bit */
#define IEEE80211_WEP_IVLEN         3   /* 24bit */
#define IEEE80211_WEP_KIDLEN            1   /* 1 octet */
#define IEEE80211_WEP_CRCLEN            4   /* CRC-32 */

#define IEEE80211_CRC_LEN           4

#define IEEE80211_MTU               1500
#define IEEE80211_MAX_LEN           (2300 + IEEE80211_CRC_LEN + \
    (IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN + IEEE80211_WEP_CRCLEN))
#define IEEE80211_AMPDU_LIMIT_MAX   (64 * 1024 - 1)

#include <ieee80211_phytype.h> /* IEEE80211_T_ phy types */

#endif /* _NET_IF_IEEE80211_H_ */

/* NB: these are defined privately until XR support is announced */
enum {
    ATHEROS_T_XR    = IEEE80211_T_TURBO+1,  /* extended range */
};

#define HAL_COMP_BUF_MAX_SIZE   9216       /* 9K */
#define HAL_COMP_BUF_ALIGN_SIZE 512

#define HAL_TXQ_USE_LOCKOUT_BKOFF_DIS   0x00000001

#define INIT_AIFS       2
#define INIT_CWMIN      15
#define INIT_CWMIN_11B      31
#define INIT_CWMAX      1023
#define INIT_SH_RETRY       10 /* Per rate rts fail count before switching to next rate */
#define INIT_LG_RETRY       10
#define INIT_SSH_RETRY      63
#define INIT_SLG_RETRY      32

typedef struct {
    u_int32_t   tqi_ver;        /* HAL TXQ verson */
    HAL_TX_QUEUE    tqi_type;       /* hw queue type*/
    HAL_TX_QUEUE_SUBTYPE tqi_subtype;   /* queue subtype, if applicable */
    HAL_TX_QUEUE_FLAGS tqi_qflags;      /* queue flags */
    u_int32_t   tqi_priority;
    u_int32_t   tqi_aifs;       /* aifs */
    u_int32_t   tqi_cwmin;      /* cw_min */
    u_int32_t   tqi_cwmax;      /* cwMax */
    u_int16_t   tqi_shretry;        /* frame short retry limit */
    u_int16_t   tqi_lgretry;        /* frame long retry limit */
    u_int32_t   tqi_cbr_period;
    u_int32_t   tqi_cbr_overflow_limit;
    u_int32_t   tqi_burst_time;
    u_int32_t   tqi_ready_time;
    u_int32_t   tqi_phys_comp_buf;
    u_int32_t   tqi_int_flags;       /* flags for internal use */
} HAL_TX_QUEUE_INFO;

extern  bool ath_hal_set_tx_q_props(struct ath_hal *ah,
        HAL_TX_QUEUE_INFO *qi, const HAL_TXQ_INFO *q_info);
extern  bool ath_hal_get_tx_q_props(struct ath_hal *ah,
        HAL_TXQ_INFO *q_info, const HAL_TX_QUEUE_INFO *qi);

typedef enum {
    HAL_ANI_PRESENT = 0x1,            /* is ANI support present */
    HAL_ANI_NOISE_IMMUNITY_LEVEL = 0x2,            /* set level */
    HAL_ANI_OFDM_WEAK_SIGNAL_DETECTION = 0x4, /* enable/disable */
    HAL_ANI_CCK_WEAK_SIGNAL_THR = 0x8,        /* enable/disable */
    HAL_ANI_FIRSTEP_LEVEL = 0x10,                  /* set level */
    HAL_ANI_SPUR_IMMUNITY_LEVEL = 0x20,            /* set level */
    HAL_ANI_MODE = 0x40,              /* 0 => manual, 1 => auto */
    HAL_ANI_PHYERR_RESET = 0x80,       /* reset phy error stats */
    HAL_ANI_MRC_CCK = 0x100,                  /* enable/disable */
    HAL_ANI_ALL = 0xffff
} HAL_ANI_CMD;

#define HAL_SPUR_VAL_MASK       0x3FFF
#define HAL_SPUR_CHAN_WIDTH     87
#define HAL_BIN_WIDTH_BASE_100HZ    3125
#define HAL_BIN_WIDTH_TURBO_100HZ   6250
#define HAL_MAX_BINS_ALLOWED        28

#define CHANNEL_XR_A    (CHANNEL_A | CHANNEL_XR)
#define CHANNEL_XR_G    (CHANNEL_PUREG | CHANNEL_XR)
#define CHANNEL_XR_T    (CHANNEL_T | CHANNEL_XR)

/*
 * A    = 5GHZ|OFDM
 * T    = 5GHZ|OFDM|TURBO
 * XR_T = 2GHZ|OFDM|XR
 *
 * IS_CHAN_A(T) or IS_CHAN_A(XR_T) will return TRUE.  This is probably
 * not the default behavior we want.  We should migrate to a better mask --
 * perhaps CHANNEL_ALL.
 *
 * For now, IS_CHAN_G() masks itself with CHANNEL_108G.
 *
 */

#define IS_CHAN_A(_c)   ((((_c)->channel_flags & CHANNEL_A) == CHANNEL_A) || \
             (((_c)->channel_flags & CHANNEL_A_HT20) == CHANNEL_A_HT20) || \
             (((_c)->channel_flags & CHANNEL_A_HT40PLUS) == CHANNEL_A_HT40PLUS) || \
             (((_c)->channel_flags & CHANNEL_A_HT40MINUS) == CHANNEL_A_HT40MINUS))
#define IS_CHAN_B(_c)   (((_c)->channel_flags & CHANNEL_B) == CHANNEL_B)
#define IS_CHAN_G(_c)   ((((_c)->channel_flags & (CHANNEL_108G|CHANNEL_G)) == CHANNEL_G) ||  \
             (((_c)->channel_flags & CHANNEL_G_HT20) == CHANNEL_G_HT20) || \
             (((_c)->channel_flags & CHANNEL_G_HT40PLUS) == CHANNEL_G_HT40PLUS) || \
             (((_c)->channel_flags & CHANNEL_G_HT40MINUS) == CHANNEL_G_HT40MINUS))
#define IS_CHAN_108G(_c)(((_c)->channel_flags & CHANNEL_108G) == CHANNEL_108G)
#define IS_CHAN_T(_c)   (((_c)->channel_flags & CHANNEL_T) == CHANNEL_T)
#define IS_CHAN_X(_c)   (((_c)->channel_flags & CHANNEL_X) == CHANNEL_X)
#define IS_CHAN_PUREG(_c) \
    (((_c)->channel_flags & CHANNEL_PUREG) == CHANNEL_PUREG)

#define IS_CHAN_TURBO(_c)   (((_c)->channel_flags & CHANNEL_TURBO) != 0)
#define IS_CHAN_CCK(_c)     (((_c)->channel_flags & CHANNEL_CCK) != 0)
#define IS_CHAN_OFDM(_c)    (((_c)->channel_flags & CHANNEL_OFDM) != 0)
#define IS_CHAN_XR(_c)      (((_c)->channel_flags & CHANNEL_XR) != 0)
#define IS_CHAN_5GHZ(_c)    (((_c)->channel_flags & CHANNEL_5GHZ) != 0)
#define IS_CHAN_2GHZ(_c)    (((_c)->channel_flags & CHANNEL_2GHZ) != 0)
#define IS_CHAN_PASSIVE(_c) (((_c)->channel_flags & CHANNEL_PASSIVE) != 0)
#define IS_CHAN_HALF_RATE(_c)   (((_c)->channel_flags & CHANNEL_HALF) != 0)
#define IS_CHAN_QUARTER_RATE(_c) (((_c)->channel_flags & CHANNEL_QUARTER) != 0)
#define IS_CHAN_HT20(_c)        (((_c)->channel_flags & CHANNEL_HT20) != 0)
#define IS_CHAN_HT40(_c)        ((((_c)->channel_flags & CHANNEL_HT40PLUS) != 0) || (((_c)->channel_flags & CHANNEL_HT40MINUS) != 0))
#define IS_CHAN_HT(_c)          (IS_CHAN_HT20((_c)) || IS_CHAN_HT40((_c)))

#define IS_CHAN_NA_20(_c) \
    (((_c)->channel_flags & CHANNEL_A_HT20) == CHANNEL_A_HT20)
#define IS_CHAN_NA_40PLUS(_c) \
    (((_c)->channel_flags & CHANNEL_A_HT40PLUS) == CHANNEL_A_HT40PLUS)
#define IS_CHAN_NA_40MINUS(_c) \
    (((_c)->channel_flags & CHANNEL_A_HT40MINUS) == CHANNEL_A_HT40MINUS)

#define IS_CHAN_NG_20(_c) \
    (((_c)->channel_flags & CHANNEL_G_HT20) == CHANNEL_G_HT20)
#define IS_CHAN_NG_40PLUS(_c) \
    (((_c)->channel_flags & CHANNEL_G_HT40PLUS) == CHANNEL_G_HT40PLUS)
#define IS_CHAN_NG_40MINUS(_c) \
    (((_c)->channel_flags & CHANNEL_G_HT40MINUS) == CHANNEL_G_HT40MINUS)

#define IS_CHAN_IN_PUBLIC_SAFETY_BAND(_c) ((_c) > 4940 && (_c) < 4990)

#ifdef ATH_NF_PER_CHAN
#define AH_HOME_CHAN_NFCAL_HIST(ah) (AH_PRIVATE(ah)->ah_curchan ? &AH_PRIVATE(ah)->ah_curchan->nf_cal_hist: NULL)
#else
#define AH_HOME_CHAN_NFCAL_HIST(ah) (&AH_PRIVATE(ah)->nf_cal_hist)
#endif
/*
 * Deduce if the host cpu has big- or litt-endian byte order.
 */
static inline int
isBigEndian(void)
{
    union {
        int32_t i;
        char c[4];
    } u;
    u.i = 1;
    return (u.c[0] == 0);
}

/* unalligned little endian access */
#ifndef LE_READ_2
#define LE_READ_2(p)                            \
    ((u_int16_t)                            \
     ((((const u_int8_t *)(p))[0]    ) | (((const u_int8_t *)(p))[1]<< 8)))
#endif
#ifndef LE_READ_4
#define LE_READ_4(p)                            \
    ((u_int32_t)                            \
     ((((const u_int8_t *)(p))[0]    ) | (((const u_int8_t *)(p))[1]<< 8) |\
      (((const u_int8_t *)(p))[2]<<16) | (((const u_int8_t *)(p))[3]<<24)))
#endif
/*
 * Register manipulation macros that expect bit field defines
 * to follow the convention that an _S suffix is appended for
 * a shift count, while the field mask has no suffix.
 */
#define SM(_v, _f)  (((_v) << _f##_S) & _f)
#define MS(_v, _f)  (((_v) & _f) >> _f##_S)

#define OS_REG_RMW(_a, _r, _set, _clr)    \
        OS_REG_WRITE(_a, _r, (OS_REG_READ(_a, _r) & ~(_clr)) | (_set))
#define OS_REG_RMW_FIELD(_a, _r, _f, _v) \
    OS_REG_WRITE(_a, _r, \
        (OS_REG_READ(_a, _r) &~ _f) | (((_v) << _f##_S) & _f))



#define OS_REG_RMW_FIELD_ALT(_a, _r, _f, _v) \
    OS_REG_WRITE(_a, _r, \
	(OS_REG_READ(_a, _r) &~(_f<<_f##_S)) | (((_v) << _f##_S) & (_f<<_f##_S)))

#define OS_REG_READ_FIELD(_a, _r, _f) \
        (((OS_REG_READ(_a, _r) & _f) >> _f##_S))

#define OS_REG_READ_FIELD_ALT(_a, _r, _f) \
	((OS_REG_READ(_a, _r) >> (_f##_S))&(_f))

#define OS_REG_SET_BIT(_a, _r, _f) \
    OS_REG_WRITE(_a, _r, OS_REG_READ(_a, _r) | (_f))
#define OS_REG_CLR_BIT(_a, _r, _f) \
    OS_REG_WRITE(_a, _r, OS_REG_READ(_a, _r) &~ (_f))

#define OS_REG_IS_BIT_SET(_a, _r, _f) \
        ((OS_REG_READ(_a, _r) & (_f)) != 0)

/*
 * Regulatory domain support.
 */

/*
 * Return the max allowed antenna gain based on the current
 * regulatory domain.
 */
extern  u_int ath_hal_getantennareduction(struct ath_hal *,
        HAL_CHANNEL *, u_int twiceGain);
/*
 *
 */
u_int
ath_hal_getantennaallowed(struct ath_hal *ah, HAL_CHANNEL *chan);

/*
 * Return the test group for the specific channel based on
 * the current regulator domain.
 */
extern  u_int ath_hal_getctl(struct ath_hal *, HAL_CHANNEL *);

/*
 * Return whether or not a noise floor check is required
 * based on the current regulatory domain for the specified
 * channel.
 */
extern  bool ath_hal_getnfcheckrequired(struct ath_hal *, HAL_CHANNEL *);

/*
 * Map a public channel definition to the corresponding
 * internal data structure.  This implicitly specifies
 * whether or not the specified channel is ok to use
 * based on the current regulatory domain constraints.
 */
extern  HAL_CHANNEL_INTERNAL *ath_hal_checkchannel(struct ath_hal *,
        const HAL_CHANNEL *);

/*
 * Convert Country / RegionDomain to RegionDomain
 * rd could be region or country code.
*/
extern HAL_REG_DOMAIN
ath_hal_getDomain(struct ath_hal *ah, u_int16_t rd);

/*
 * wait for the register contents to have the specified value.
 * timeout is in usec.
 */
extern  bool ath_hal_wait(struct ath_hal *, u_int reg,
        u_int32_t mask, u_int32_t val, u_int32_t timeout);
#define AH_WAIT_TIMEOUT     100000   /* default timeout (usec) */

/* return the first n bits in val reversed */
extern  u_int32_t ath_hal_reverse_bits(u_int32_t val, u_int32_t n);

/* printf interfaces */
extern  void __ahdecl ath_hal_printf(struct ath_hal *, const char*, ...)
        __printflike(2,3);
extern  void __ahdecl ath_hal_vprintf(struct ath_hal *, const char*, __va_list)
        __printflike(2, 0);
extern  const char* __ahdecl ath_hal_ether_sprintf(const u_int8_t *mac);

/*
 * Make ath_hal_malloc a macro rather than an inline function.
 * This allows the underlying amalloc macro to keep track of the
 * __FILE__ and __LINE__ of the callers to ath_hal_malloc, which
 * is a lot more interesting than only being able to see the
 * __FILE__ and __LINE__ of the def of a inline function.
 */
#define ath_hal_malloc(ah, size) qdf_mem_malloc(size)
#define ath_hal_free(ah, p) qdf_mem_free(p)

/* read PCI information */
extern u_int32_t ath_hal_read_pci_config_space(struct ath_hal *ah, u_int32_t offset, void *pBuffer, u_int32_t length);

/**
**  Debugging Interface
**
**  Implemented a "DPRINTF" like interface that allows for selective enable/disable
**  of debug information.  This is completely compiled out if AH_DEBUG is not defined.
**  Bit mask is used to specify debug level.  Renamed from HALDEBUG to identify as a
**  seperately defined interface.
**
**  Note that the parameter that is used to enable debug levels is the instance variable
**  ath_hal_debug, defined in the config structure in the common hal object (ah).  This
**  can be set at startup, or can be dynamically changed through the HAL configuration
**  interface using  OOIDS or the iwpriv interface.  Allows a user to change debugging
**  emphasis without having to recompile the HAL.
**/

#if AH_DEBUG || AH_PRINT_FILTER

/*
** Debug Level Definitions
*/

/*
 * HAL_DBG_MAX_MASK - limit on the number of HAL_DBG print categories.
 * This value can be increased up to ASF_PRINT_MASK_BITS without affecting
 * driver operation, but the code in ath_hal_set_config_param(HAL_CONFIG_DEBUG)
 * currently assumes HAL_DBG_MAX_MASK is 32 (more precisely, sizeof(int)*8).
 */
#define HAL_DBG_MAX_MASK 32
enum {
    HAL_DBG_RESET = 0,
    HAL_DBG_PHY_IO,
    HAL_DBG_REG_IO,
    HAL_DBG_RF_PARAM,
    HAL_DBG_QUEUE,
    HAL_DBG_EEPROM_DUMP,
    HAL_DBG_EEPROM,
    HAL_DBG_NF_CAL,
    HAL_DBG_CALIBRATE,
    HAL_DBG_CHANNEL,
    HAL_DBG_INTERRUPT,
    HAL_DBG_DFS,
    HAL_DBG_DMA,
    HAL_DBG_REGULATORY,
    HAL_DBG_TX,
    HAL_DBG_TXDESC,
    HAL_DBG_RX,
    HAL_DBG_RXDESC,
    HAL_DBG_ANI,
    HAL_DBG_BEACON,
    HAL_DBG_KEYCACHE,
    HAL_DBG_POWER_MGMT,
    HAL_DBG_MALLOC,
    HAL_DBG_FORCE_BIAS,
    HAL_DBG_POWER_OVERRIDE,
    HAL_DBG_SPUR_MITIGATE,
    HAL_DBG_PRINT_REG,
    HAL_DBG_TIMER,
    HAL_DBG_BT_COEX,
    HAL_DBG_FCS_RTT,
    /* add new print categories here*/

    HAL_DBG_NUM_CATEGORIES,
    HAL_DBG_UNMASKABLE = HAL_DBG_MAX_MASK,
};
#if HAL_DBG_NUM_CATEGORIES > HAL_DBG_MAX_MASK
#error                                               \
    change HAL_DBG_MAX_MASK to ASF_PRINT_MASK_BITS-1 \
    and update HAL_CONFIG_DEBUG
#endif

/*
 * External reference to hal dprintf function
 */
extern void HDPRINTF(struct ath_hal *ah, unsigned category, const char *fmt, ...);

/*
 * Initialize HAL HDPRINTF
 */
#define ath_hal_hdprintf_init(_ah)                                      \
    do {                                                                \
        struct ath_hal_private *ahp = AH_PRIVATE(_ah);                  \
        ahp->ah_asf_print.name = "HAL";                                 \
        ahp->ah_asf_print.num_bit_specs =                               \
            ARRAY_LENGTH(ath_hal_print_categories);                     \
        ahp->ah_asf_print.bit_specs = ath_hal_print_categories;         \
        ahp->ah_asf_print.custom_ctxt = _ah;                            \
        ahp->ah_asf_print.custom_print = hal_print;                     \
        asf_print_mask_set(&ahp->ah_asf_print, HAL_DBG_UNMASKABLE, 1);  \
        ath_hal_set_config_param(_ah, HAL_CONFIG_DEBUG,                 \
                                 &ahp->ah_config.ath_hal_debug);        \
        asf_print_ctrl_register(&ahp->ah_asf_print);            \
    } while (0)
/*
 * Deregister HAL HDPRINTF
 */
#define ath_hal_hdprintf_deregister(_ah)                        \
    asf_print_ctrl_unregister(&AH_PRIVATE(_ah)->ah_asf_print)
#else
#define HDPRINTF(_ah, _level, _fmt, ...)
#define ath_hal_hdprintf_init(_ah)
#define ath_hal_hdprintf_deregister(_ah)
#endif /* AH_DEBUG */

/*
** Prototype for factory initialization function
*/

extern void __ahdecl ath_hal_factory_defaults(struct ath_hal_private *ap,
                                              struct hal_reg_parm *hal_conf_parm);

/*
 * Register logging definitions shared with ardecode.
 */
#include "ah_decode.h"

/*
 * Common assertion interface.  Note: it is a bad idea to generate
 * an assertion failure for any recoverable event.  Instead catch
 * the violation and, if possible, fix it up or recover from it; either
 * with an error return value or a diagnostic messages.  System software
 * does not panic unless the situation is hopeless.
 */
#ifdef AH_ASSERT
extern  void ath_hal_assert_failed(const char* filename,
        int lineno, const char* msg);

#define HALASSERT(_x) do {                  \
    if (!(_x)) {                        \
        ath_hal_assert_failed(__FILE__, __LINE__, #_x); \
    }                           \
} while (0)

/*
 * Provide an inline function wrapper for HALASSERT, to allow assertions
 * to be used in code locations where the macro can't be used.
 * Only use this when the macro doesn't work, because wrapping HALASSERT
 * in a function (even an inline function) defeats the purpose of the
 * __FILE__ and __LINE__ diagnostic info and condition display inside
 * HALASSERT.
 */
static inline void hal_assert(int condition)
{
    HALASSERT(condition);
}
#else
#define HALASSERT(_x)
#define hal_assert(condition) 0
#endif /* AH_ASSERT */

/*
 * Convert between microseconds and core system clocks.
 */
extern  u_int ath_hal_mac_clks(struct ath_hal *ah, u_int usecs);
extern  u_int ath_hal_mac_usec(struct ath_hal *ah, u_int clks);

/*
 * Generic get/set capability support.  Each chip overrides
 * this routine to support chip-specific capabilities.
 */
extern  HAL_STATUS ath_hal_getcapability(struct ath_hal *ah,
        HAL_CAPABILITY_TYPE type, u_int32_t capability,
        u_int32_t *result);
extern  bool ath_hal_setcapability(struct ath_hal *ah,
        HAL_CAPABILITY_TYPE type, u_int32_t capability,
        u_int32_t setting, HAL_STATUS *status);

/*
 * Macros to set/get RF KILL capabilities.
 * These macros define inside the HAL layer some of macros defined in
 * if_athvar.h, which are visible only by the ATH layer.
 */
#define ath_hal_isrfkillenabled(_ah)  \
    ((*(_ah)->ah_get_capability)(_ah, HAL_CAP_RFSILENT, 1, AH_NULL) == HAL_OK)
#define ath_hal_enable_rfkill(_ah, _v) \
    (*(_ah)->ah_set_capability)(_ah, HAL_CAP_RFSILENT, 1, _v, AH_NULL)
#define ath_hal_hasrfkill_int(_ah)  \
    ((*(_ah)->ah_get_capability)(_ah, HAL_CAP_RFSILENT, 3, AH_NULL) == HAL_OK)

/*
 * Device revision information.
 */
typedef struct {
    u_int16_t   ah_devid;       /* PCI device ID */
    u_int16_t   ah_subvendorid;     /* PCI subvendor ID */
    u_int32_t   ah_mac_version;      /* MAC version id */
    u_int16_t   ah_mac_rev;      /* MAC revision */
    u_int16_t   ah_phy_rev;      /* PHY revision */
    u_int16_t   ah_analog_5ghz_rev;   /* 2GHz radio revision */
    u_int16_t   ah_analog2GhzRev;   /* 5GHz radio revision */
} HAL_REVS;

/*
 * Argument payload for HAL_DIAG_SETKEY.
 */
typedef struct {
    HAL_KEYVAL  dk_keyval;
    u_int16_t   dk_keyix;   /* key index */
    u_int8_t    dk_mac[IEEE80211_ADDR_LEN];
    int     dk_xor;     /* XOR key data */
} HAL_DIAG_KEYVAL;

/*
 * Argument payload for HAL_DIAG_EEWRITE.
 */
typedef struct {
    u_int16_t   ee_off;     /* eeprom offset */
    u_int16_t   ee_data;    /* write data */
} HAL_DIAG_EEVAL;

typedef struct {
        u_int offset;           /* reg offset */
        u_int32_t val;          /* reg value  */
} HAL_DIAG_REGVAL;

extern  bool ath_hal_getdiagstate(struct ath_hal *ah, int request,
            const void *args, u_int32_t argsize,
            void **result, u_int32_t *resultsize);

/*
 * Setup a h/w rate table for use.
 */
extern  void ath_hal_setupratetable(struct ath_hal *ah, HAL_RATE_TABLE *rt);

/*
 * Common routine for implementing getChanNoise api.
 */
extern  int16_t ath_hal_get_chan_noise(struct ath_hal *ah, HAL_CHANNEL *chan);

#ifdef ATH_TX99_DIAG
/* Tx99 functions */
#endif

/*
 * The following are for direct integration of Atheros code.
 */
typedef enum {
    WIRELESS_MODE_11a   = 0,
    WIRELESS_MODE_TURBO = 1,
    WIRELESS_MODE_11b   = 2,
    WIRELESS_MODE_11g   = 3,
    WIRELESS_MODE_108g  = 4,
    WIRELESS_MODE_XR    = 5,
    WIRELESS_MODE_11NA  = 6,
    WIRELESS_MODE_11NG  = 7
} WIRELESS_MODE;

#ifndef WIRELESS_MODE_MAX
#define WIRELESS_MODE_MAX      (WIRELESS_MODE_11NG + 1)
#endif

extern  WIRELESS_MODE ath_hal_chan2wmode(struct ath_hal *, const HAL_CHANNEL *);
extern  WIRELESS_MODE ath_hal_chan2htwmode(struct ath_hal *, const HAL_CHANNEL *);
extern  u_int ath_hal_get_curmode(struct ath_hal *, HAL_CHANNEL_INTERNAL *);

extern  u_int8_t ath_hal_chan_2_clock_rate_mhz(struct ath_hal *);
extern int8_t ath_hal_get_twice_max_regpower(struct ath_hal_private *ahpriv, HAL_CHANNEL_INTERNAL *ichan,
                            HAL_CHANNEL *chan);

#ifndef REMOVE_PKT_LOG
#include "ah_pktlog.h"
extern void ath_hal_log_ani(
            HAL_SOFTC sc, struct log_ani *log_data, u_int32_t iflags);
#endif

#define FRAME_DATA      2   /* Data frame */
#define SUBT_DATA_CFPOLL    2   /* Data + CF-Poll */
#define SUBT_NODATA_CFPOLL  6   /* No Data + CF-Poll */
#define WLAN_CTRL_FRAME_SIZE    (2+2+6+4)   /* ACK+FCS */

#define MAX_REG_ADD_COUNT   129

#endif /* _ATH_AH_INTERAL_H_ */
