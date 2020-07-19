/*
* Copyright (c) 2016, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 * @@-COPYRIGHT-START-@@
 *
 * 2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * @@-COPYRIGHT-END-@@
 */

/*
 MBO  module
*/


#include <ieee80211_var.h>
#include <ieee80211_ioctl.h>  /* for ieee80211req_athdbg */

#define MBO_ATTRIB_NON_CHANNEL_LEN 3
#define MBO_SUBELM_NON_CHANNEL_LEN 7

/*
 * Structure for id(elem id or attribute id) and
 * len fields.Done to avoid redundancy due to
 * repeated usage of these two fields.
 */
struct ieee80211_mbo_header {
    u_int8_t ie;
    u_int8_t len;
}qdf_packed;

struct ieee80211_mbo_ie {
    struct ieee80211_mbo_header header;
    u_int8_t oui[3];
    u_int8_t oui_type;
    u_int8_t opt_ie[0];
}qdf_packed; /*packing is required */

struct ieee80211_assoc_disallow {
    struct ieee80211_mbo_header header;
    u_int8_t reason_code;
    u_int8_t opt_ie[0];
}qdf_packed;

struct ieee80211_cell_cap {
    struct ieee80211_mbo_header header;
    u_int8_t cell_conn;
    u_int8_t opt_ie[0];
}qdf_packed;

struct ieee80211_mbo_cap {
    struct ieee80211_mbo_header header;
    u_int8_t  reserved6:6;
    u_int8_t  cap_cellular:1;
    u_int8_t  cap_reserved1:1;
    u_int8_t  opt_ie[0];
}qdf_packed;

struct ieee80211_cell_data_conn_pref {
    struct ieee80211_mbo_header header;
    u_int8_t cell_pref;
    u_int8_t opt_ie[0];
}qdf_packed;

struct ieee80211_transit_reason_code {
    struct ieee80211_mbo_header header;
    u_int8_t reason_code;
    u_int8_t opt_ie[0];
}qdf_packed;

struct ieee80211_transit_reject_reason_code {
    struct ieee80211_mbo_header header;
    u_int8_t reason_code;
    u_int8_t opt_ie[0];
}qdf_packed;

struct ieee80211_assoc_retry_delay {
    struct ieee80211_mbo_header header;
    u_int16_t re_auth_delay;
    u_int8_t opt_ie[0];
}qdf_packed;

struct ieee80211_mbo {
    osdev_t        mbo_osdev;
    wlan_dev_t     mbo_ic;
    u_int8_t       usr_assoc_disallow;
    u_int8_t       usr_mbocap_cellular_capable;
    u_int8_t       usr_cell_pref;
    u_int8_t       usr_trans_reason;
    u_int16_t      usr_assoc_retry;
};

#define OCE_WAN_METRICS_DL_MASK          0x0F
#define OCE_WAN_METRICS_DL_SHIFT         0
#define OCE_WAN_METRICS_UL_MASK          0xF0
#define OCE_WAN_METRICS_UL_SHIFT         4
#define OCE_WAN_METRIC(a)                (a)

#define OCE_CAP_REL_DEFAULT              1
/* WFA OCE spec specifies a range of -60 to -90 dBm, but a wider range is used
 * here to allow AP to work as OCE CTT (Compliance Test Tool)
 */
#define OCE_ASSOC_MIN_RSSI_DEFAULT       -90    /* dBm */
#define OCE_ASSOC_MIN_RSSI_MIN           -90    /* dBm */
#define OCE_ASSOC_MIN_RSSI_MAX           -30    /* dBm */
#define OCE_ASSOC_RETRY_DELAY_DEFAULT    1
#define OCE_WAN_METRICS_CAP_DEFAULT      13     /* 819.2 Mbps */

struct ieee80211_oce_cap {
    struct ieee80211_mbo_header header;
#if _BYTE_ORDER == _BIG_ENDIAN
    u_int8_t  ctrl_reserved:1;
    u_int8_t  ctrl_non_oce_ap_present:1;
    u_int8_t  ctrl_hlp_enabled:1;
    u_int8_t  ctrl_11b_ap_present:1;
    u_int8_t  ctrl_sta_cfon:1;
    u_int8_t  ctrl_oce_rel:3;
#else
    u_int8_t  ctrl_oce_rel:3;
    u_int8_t  ctrl_sta_cfon:1;
    u_int8_t  ctrl_11b_ap_present:1;
    u_int8_t  ctrl_hlp_enabled:1;
    u_int8_t  ctrl_non_oce_ap_present:1;
    u_int8_t  ctrl_reserved:1;
#endif
} qdf_packed;

struct ieee80211_oce_assoc_reject {
    struct ieee80211_mbo_header header;
    u_int8_t  delta_rssi;
    u_int8_t  retry_delay;
} qdf_packed;

struct ieee80211_oce_wan_metrics {
    struct ieee80211_mbo_header header;
    u_int8_t  avail_cap;
} qdf_packed;

struct ieee80211_oce_rnr_completeness {
    struct ieee80211_mbo_header header;
    u_int32_t  short_ssid[0];
} qdf_packed;

struct ieee80211_oce {
    osdev_t        oce_osdev;
    wlan_dev_t     oce_ic;
    u_int8_t       usr_assoc_reject;
    int8_t         usr_assoc_min_rssi;
    u_int8_t       usr_assoc_retry_delay;
    u_int8_t       usr_wan_metrics;
    u_int8_t       usr_hlp;
    u_int8_t       oce_ap_cnt;
    u_int8_t       non_oce_ap_cnt;
    bool           non_oce_ap_present;
    bool           only_11b_ap_present;
};

typedef struct ieee80211_oce_scan_cbinfo {
	u_int8_t  channel;
	u_int32_t age_timeout;
	bool      non_oce_ap_present;
	bool      only_11b_ap_present;
} ieee80211_oce_scan_cbinfo_t;

typedef struct ieee80211_rnr_tbtt_info {
	u_int8_t  tbtt_offset;
	u_int8_t  bssid[IEEE80211_ADDR_LEN];
	u_int32_t short_ssid;
} __packed ieee80211_rnr_tbtt_info_t;

typedef struct ieee80211_rnr_ap_info {
#if _BYTE_ORDER == _BIG_ENDIAN
    u_int16_t hdr_info_len:8;
    u_int16_t hdr_info_cnt:4;
    u_int16_t hdr_reserved:1;
    u_int16_t hdr_filtered_nbr_ap:1;
    u_int16_t hdr_field_type:2;
#else
    u_int16_t hdr_field_type:2;
    u_int16_t hdr_filtered_nbr_ap:1;
    u_int16_t hdr_reserved:1;
    u_int16_t hdr_info_cnt:4;
    u_int16_t hdr_info_len:8;
#endif
	u_int8_t  op_class;
	u_int8_t  channel;
	struct ieee80211_rnr_tbtt_info tbtt_info[0];
} __packed ieee80211_rnr_ap_info_t;

typedef struct ieee80211_rnr_cbinfo {
	struct ieee80211vap *vap;
	u_int8_t  *ap_info;
	u_int8_t  ap_info_cnt;
	u_int8_t  ap_info_cnt_max;
} ieee80211_rnr_cbinfo_t;

typedef struct ieee80211_ap_chan_info {
	u_int8_t  op_class;
	u_int8_t  channel;
} ieee80211_ap_chan_info_t;

typedef struct ieee80211_acr_cbinfo {
	struct ieee80211vap *vap;
    ieee80211_ap_chan_info_t *ap_chan_info;
	u_int32_t ap_chan_info_cnt;
	u_int32_t ap_chan_info_cnt_max;
} ieee80211_acr_cbinfo_t;

bool ieee80211_oce_is_vap_valid (struct ieee80211vap *vap);
u_int8_t ieee80211_setup_oce_attrs (int f_type, struct ieee80211vap *vap, u_int8_t *frm, struct ieee80211_node *ni);
