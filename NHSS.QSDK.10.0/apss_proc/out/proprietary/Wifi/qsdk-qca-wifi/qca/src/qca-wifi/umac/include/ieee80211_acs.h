/*
 * Copyright (c) 2011, 2017 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 *  Copyright (c) 2008 Atheros Communications Inc.
 * All Rights Reserved.
 *
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#ifndef _IEEE80211_ACS_H
#define _IEEE80211_ACS_H

typedef struct ieee80211_acs    *ieee80211_acs_t;

typedef struct _ieee80211_chan_neighbor_list {
    ieee80211_acs_t  acs;        /* acs reference */
    u_int8_t         chan;       /* ieee channel number */
    u_int8_t         nbss;       /*number of bss per channel */
    ieee80211_neighbor_info   *neighbor_list; /* List of Neighbor per channel */
    int32_t         neighbor_size;
    int32_t         status;
} ieee80211_chan_neighbor_list;

#define IEEE80211_ACS_CTRLFLAG 0x1
#define IEEE80211_ACS_ENABLE_BK_SCANTIMER  0x2
#define IEEE80211_ACS_SCANTIME 0x3
#define IEEE80211_ACS_RSSIVAR  0x4
#define IEEE80211_ACS_CHLOADVAR 0x5
#define IEEE80211_ACS_LIMITEDOBSS 0x6
#define IEEE80211_ACS_DEBUGTRACE 0x7
#define IEEE80211_ACS_BLOCK_MODE 0x8
#define IEEE80211_ACS_TX_POWER_OPTION 0x9
#define IEEE80211_ACS_2G_ALL_CHAN 0xa
#define IEEE80211_ACS_SRLOADVAR 0xb
#define IEEE80211_ACS_RANK 0xc
#define IEEE80211_ACS_NF_THRESH 0xd



#define ACS_CHAN_STATS 0
#define ACS_CHAN_STATS_NF 1
#define ACS_CHAN_STATS_DIFF 3
#define IEEE80211_HAL_OFFLOAD_ARCH 0
#define IEEE80211_HAL_DIRECT_ARCH 1
int ieee80211_acs_attach(ieee80211_acs_t *acs,
                          struct ieee80211com *ic,
                          osdev_t             osdev);

int ieee80211_acs_init(ieee80211_acs_t *acs,
                          struct ieee80211com *ic,
                          osdev_t osdev);
void ieee80211_acs_deinit(ieee80211_acs_t *acs);
int ieee80211_acs_detach(ieee80211_acs_t *acs);
int ieee80211_acs_startscantime(struct ieee80211com *ic);
int ieee80211_acs_state(struct ieee80211_acs *acs);

void ieee80211_acs_stats_update(ieee80211_acs_t acs,
                                u_int8_t flags,
                                u_int ieee_chan,
                                int16_t chan_nf,
                                struct ieee80211_chan_stats *chan_stats);

#if ATH_ACS_SUPPORT_SPECTRAL && WLAN_SPECTRAL_ENABLE
void ieee80211_update_eacs_counters(struct ieee80211com *ic, int8_t nfc_ctl_rssi,
                                     int8_t nfc_ext_rssi, int8_t ctrl_nf, int8_t ext_nf);
void ieee80211_init_spectral_chan_loading(struct ieee80211com *ic, int current_channel,
                                                                         int ext_channel);
int ieee80211_get_spectral_freq_loading(struct ieee80211com *ic);
#endif
int ieee80211_acs_set_param(ieee80211_acs_t acs, int param , int val);
int ieee80211_acs_get_param(ieee80211_acs_t acs, int param );
#if UMAC_SUPPORT_CFG80211
int wlan_acs_cfg80211_start_scan_report(wlan_if_t vap,
                        cfg80211_hostapd_acs_params *cfg_acs_params);
#endif

/* List of function APIs used by other modules(like CBS) to control ACS */
int ieee80211_acs_api_prepare(struct ieee80211vap *vap, ieee80211_acs_t acs, enum ieee80211_phymode mode,
                              uint32_t *chan_list, uint32_t *nchans);
int ieee80211_acs_api_rank(ieee80211_acs_t acs, uint8_t top);
int ieee80211_acs_api_complete(ieee80211_acs_t acs);
int ieee80211_acs_api_flush(struct ieee80211vap *vap);
int ieee80211_acs_api_update(struct ieee80211com *ic, enum scan_event_type type, uint32_t freq);
int ieee80211_acs_api_get_ranked_chan(ieee80211_acs_t acs, int rank);
int ieee80211_acs_api_resttime(struct ieee80211_acs *acs);
int ieee80211_acs_api_dwelltime(struct ieee80211_acs *acs);

#endif
