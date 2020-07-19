/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */


#ifndef __WLAN_SCAN_H__
#define __WLAN_SCAN_H__


#include <qdf_status.h>
#include <qdf_trace.h>
#include <wlan_scan_ucfg_api.h>
#include <wlan_scan_utils_api.h>
#include <ieee80211_rsn.h>

typedef wlan_scan_entry_t  ieee80211_scan_entry_t;

#define IEEE80211_SCANENTRY_PRINTF(_ic, _cat, _fmt, ...)    \
    if (ieee80211_msg_ic(_ic, _cat)) {                  \
        ieee80211com_note((_ic), _cat, _fmt, __VA_ARGS__);        \
    }

#define IEEE80211_SCAN_MAX_SSID           10
#define IEEE80211_SCAN_MAX_BSSID          10

#define WLAN_OSIF_SCAN_ID    WLAN_OSIF_ID


#define scan_logf(level, format, args...) \
    do { \
        qdf_print(format, ## args); \
    } while (0)

#define scan_err(format, args...) \
    scan_logf(QDF_TRACE_LEVEL_FATAL, format, ## args)

#define scan_info(format, args...) \
    scan_logf(QDF_TRACE_LEVEL_INFO, format, ## args)

typedef struct ieee80211_aplist_config    *ieee80211_aplist_config_t;
typedef struct ieee80211_candidate_aplist *ieee80211_candidate_aplist_t;

QDF_STATUS wlan_scan_update_channel_list(struct ieee80211com *ic);


struct macaddr_iterate_arg {
    scan_iterator_func func;
    void *arg;
    struct qdf_mac_addr macaddr;
};

QDF_STATUS
util_wlan_scan_db_iterate_macaddr(wlan_if_t vap, uint8_t *macaddr,
        scan_iterator_func func, void *arg);

QDF_STATUS
convert_ieee_ssid_to_wlan_ssid(uint32_t num_ssid, ieee80211_ssid *ieeessid,
        struct wlan_ssid *wlanssid);

struct wlan_ssid *
convert_ieee_ssid_to_wlan_ssid_inplace(uint32_t num_ssid,
        ieee80211_ssid *ieeessid);


QDF_STATUS
wlan_update_scan_params(wlan_if_t vaphandle, struct scan_start_request *scan_params,
                enum ieee80211_opmode opmode, bool active_scan_flag,
                bool high_priority_flag, bool connected_flag,
                bool external_scan_flag, uint32_t num_ssid,
                ieee80211_ssid *ssid_list, int peer_count);

QDF_STATUS
wlan_scan_update_chan_list(wlan_if_t vap, struct scan_start_request *scan_params,
        uint32_t num_channels, uint32_t *chan_list);

QDF_STATUS
wlan_ucfg_scan_start(wlan_if_t vap, struct scan_start_request *scan_params,
        wlan_scan_requester requestor_id, enum scan_priority priority,
        wlan_scan_id *scan_id, uint32_t optie_len, uint8_t *optie_data);

QDF_STATUS
wlan_ucfg_scan_cancel(wlan_if_t vap, wlan_scan_requester requester_id,
        wlan_scan_id scan_id, enum scan_cancel_req_type type, bool sync);

static inline enum ieee80211_opmode
wlan_util_scan_entry_bss_type(struct scan_cache_entry *scan_entry)
{
    enum ieee80211_opmode mode;
    enum wlan_bss_type bss_type =  util_scan_entry_bss_type(scan_entry);

    if (bss_type == WLAN_TYPE_BSS) {
        mode = IEEE80211_M_STA;
    } else {
        mode = IEEE80211_M_IBSS;
    }
    return mode;
}

static inline uint32_t
wlan_util_scan_entry_mlme_status(struct scan_cache_entry *scan_entry)
{
    return util_scan_entry_mlme_info(scan_entry)->status;
}

static inline qdf_time_t
wlan_util_scan_entry_mlme_bad_ap_time(struct scan_cache_entry *scan_entry)
{
    return util_scan_entry_mlme_info(scan_entry)->bad_ap_time;
}

static inline uint32_t
wlan_util_scan_entry_mlme_rank(struct scan_cache_entry *scan_entry)
{
    return util_scan_entry_mlme_info(scan_entry)->rank;
}

static inline uint32_t
wlan_util_scan_entry_mlme_utility(struct scan_cache_entry *scan_entry)
{
    return util_scan_entry_mlme_info(scan_entry)->utility;
}

static inline uint32_t
wlan_util_scan_entry_mlme_assoc_state(struct scan_cache_entry *scan_entry)
{
    return util_scan_entry_mlme_info(scan_entry)->assoc_state;
}

static inline uint32_t
wlan_util_scan_entry_mlme_chanload(struct scan_cache_entry *scan_entry)
{
    return util_scan_entry_mlme_info(scan_entry)->chanload;
}

static inline void
wlan_util_scan_entry_mlme_set_status(struct scan_cache_entry *scan_entry, uint32_t val)
{
    util_scan_entry_mlme_info(scan_entry)->status = val;
}

static inline void
wlan_util_scan_entry_mlme_set_bad_ap_time(struct scan_cache_entry *scan_entry, qdf_time_t val)
{
    util_scan_entry_mlme_info(scan_entry)->bad_ap_time = val;
}

static inline void
wlan_util_scan_entry_mlme_set_rank(struct scan_cache_entry *scan_entry, uint32_t val)
{
    util_scan_entry_mlme_info(scan_entry)->rank = val;
}

static inline void
wlan_util_scan_entry_mlme_set_utility(struct scan_cache_entry *scan_entry, uint32_t val)
{
    util_scan_entry_mlme_info(scan_entry)->utility = val;
}

static inline void
wlan_util_scan_entry_mlme_set_assoc_state(struct scan_cache_entry *scan_entry, uint32_t val)
{
    util_scan_entry_mlme_info(scan_entry)->assoc_state = val;
}

static inline void
wlan_util_scan_entry_mlme_set_chanload(struct scan_cache_entry *scan_entry, uint32_t val)
{
    util_scan_entry_mlme_info(scan_entry)->chanload = val;
}

QDF_STATUS
wlan_util_scan_entry_update_mlme_info(wlan_if_t vap,
        struct scan_cache_entry *scan_entry);

static inline wlan_chan_t
wlan_util_scan_entry_channel(struct scan_cache_entry *scan_entry)
{
    return (wlan_chan_t) util_scan_entry_channel(scan_entry)->priv;
}

int
wlan_scan_entry_rsncaps(wlan_if_t vaphandle,
        struct scan_cache_entry *scan_entry, u_int16_t *rsncaps);

int
wlan_scan_entry_rsnparams(wlan_if_t vaphandle, struct scan_cache_entry *scan_entry,
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
        struct wlan_crypto_params *crypto_params);
#else
        struct ieee80211_rsnparms *rsnparams);
#endif

static inline bool
wlan_scan_in_home_channel(struct wlan_objmgr_pdev *pdev)
{
    /* Always true for OL */
    return true;
    /* What should be the case for DA ? */
}

static inline bool
wlan_scan_can_transmit(struct wlan_objmgr_pdev *pdev)
{
    /* Always true for OL */
    return true;
    /* What should be the case for DA ? */
}

void wlan_scan_set_maxentry(struct ieee80211com *ic, uint16_t val);

void wlan_scan_set_timeout(struct ieee80211com *ic, uint16_t val);

uint16_t wlan_scan_get_timeout(struct ieee80211com *ic);

uint16_t wlan_scan_get_maxentry(struct ieee80211com *ic);

bool wlan_scan_in_progress(wlan_if_t vap);

bool wlan_vdev_scan_in_progress(struct wlan_objmgr_vdev *vdev);

bool wlan_pdev_scan_in_progress(struct wlan_objmgr_pdev *pdev);

struct ieee80211_ath_channel *
wlan_find_full_channel(struct ieee80211com *ic, uint32_t freq);

void wlan_scan_cache_update_callback(struct wlan_objmgr_pdev *pdev,
        struct scan_cache_entry* scan_entry);

enum ieee80211_phymode ieee80211_get_phy_type (struct ieee80211com *ic,
        uint8_t *rates, uint8_t *xrates,
        struct ieee80211_ie_htcap_cmn *htcap,
        struct ieee80211_ie_htinfo_cmn *htinfo,
        struct ieee80211_ie_vhtcap *vhtcap,
        struct ieee80211_ie_vhtop *vhtop,
        struct ieee80211_ie_hecap *hecap,
        struct ieee80211_ie_heop *heop,
        struct ieee80211_ath_channel *bcn_recv_chan);

bool wlan_wide_band_scan_enabled(struct ieee80211com *ic);
QDF_STATUS wlan_scan_update_wide_band_scan_config(struct ieee80211com *ic);
#if WLAN_DFS_CHAN_HIDDEN_SSID
QDF_STATUS wlan_scan_config_ssid_bssid_hidden_ssid_beacon(struct ieee80211com *ic,
        ieee80211_ssid *conf_ssid, uint8_t *bssid);
#endif
#endif
