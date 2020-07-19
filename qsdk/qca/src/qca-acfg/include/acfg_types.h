/*
 * Copyright (c) 2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * Copyright (c) 2008-2010, Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef __ACFG_TYPES_H
#define __ACFG_TYPES_H

#if defined(__LITTLE_ENDIAN)
#define _BYTE_ORDER _LITTLE_ENDIAN
#elif defined(__BIG_ENDIAN)
#define _BYTE_ORDER _BIG_ENDIAN
#else
#error "Please fix asm/byteorder.h"
#endif

#ifndef __packed
#define __packed    __attribute__((__packed__))
#endif

#ifndef BUILD_X86
#include <ieee80211_external_config.h>
#endif
#define EXTERNAL_USE_ONLY
#include <stdint.h>
#include <ieee80211.h>
#include <acfg_api_types.h>

#define acfg_min(x, y)   (x < y ? x : y)
#define acfg_max(x, y)   (x > y ? x : y)


enum {
    RESTART_NONE = 0,
    RESTART_SECURITY = 1,
};

enum acfg_band_type {
    ACFG_BAND_2GHZ = 1,
    ACFG_BAND_5GHZ,
};

enum acfg_chainmask_type {
    ACFG_TX_CHAINMASK = 1,
    ACFG_RX_CHAINMASK,
};

#define ACFG_CTRL_IFACE_LEN     30

typedef struct {
    uint8_t enabled;
    uint8_t wds_addr[ACFG_MACSTR_LEN];
    uint32_t wds_flags;
} acfg_wds_params_t;

typedef enum {
    ACFG_WLAN_PROFILE_NODE_ACL_OPEN   = 0,   /* set policy: no ACL's */
    ACFG_WLAN_PROFILE_NODE_ACL_ALLOW  = 1,
    ACFG_WLAN_PROFILE_NODE_ACL_DENY   = 2,
    ACFG_WLAN_PROFILE_NODE_ACL_FLUSH  = 3,   /* flush ACL database, policy will be active */
    ACFG_WLAN_PROFILE_NODE_ACL_DETACH = 4,   /* detach ACL policy */
    ACFG_WLAN_PROFILE_NODE_ACL_RADIUS = 5,   /* set policy: RADIUS managed ACLs */

    /* keep this last */
    ACFG_WLAN_PROFILE_NODE_ACL_INVALID
} ACL_POLICY;

typedef uint32_t acfg_wlan_profile_node_acl_t ;

typedef enum {
    WPS_FLAG_DISABLED           = 0,
    WPS_FLAG_UNCONFIGURED       = 1,
    WPS_FLAG_CONFIGURED         = 2,
} WPS_FLAGS;

typedef enum {
    ACFG_FLAG_INVALID = 0,
    ACFG_FLAG_VAP_IND = 1,
    ACFG_FLAG_EXTAP   = 2,
    ACFG_FLAG_VAP_ENHIND = 4,
} WDS_FLAGS;

typedef enum {
    ACFG_EAP_TYPE_PEAP = 1,
    ACFG_EAP_TYPE_LEAP,
    ACFG_EAP_TYPE_TLS,
} EAP_TYPE;

#define ACFG_WLAN_PROFILE_VLAN_INVALID 0xFFFFFFFF

#define EAP_IDENTITY_LEN 128
#define EAP_PASSWD_LEN 128
#define EAP_FILE_NAME_LEN 512
#define EAP_PVT_KEY_PASSWD_LEN 128
#define IP_ADDR_LEN 16
#define RADIUS_SHARED_SECRET_LEN 128
#define ACCT_SHARED_SECRET_LEN 128
#define ACFG_SEC_GROUP_MGMT_CIPHER_LEN 32


#define HS_MAX_ROAMING_CONSORTIUM_LEN    16
#define HS_MAX_VENUE_NAME_LEN            256
#define HS_MAX_AUTH_TYPE_LEN             1024
#define HS_MAX_IPADDR_TYPE_AVL_LEN       3
#define HS_MAX_NAI_REALM_LIST_NUM        10
#define HS_MAX_NAI_REALM_DATA_LEN        512
#define HS_MAX_3GPP_CELLULAR_NETWORK_LEN 255
#define HS_MAX_DOMAIN_NAME_LIST_LEN      (255 * 4)
#define HS_MAX_WAN_METRICS_LEN           128
#define HS_MAX_CONNECTION_CAP_LEN        255
#define HS_MAX_OPERATING_CLASS_LEN       255
#define HS_MAX_QOS_MAP_SET_LEN           256
#define HS_MAX_HS20_ICON_LEN             1024
#define HS_MAX_OSU_SSID_LEN              65
#define HS_MAX_OSU_SERVER_URI_LEN        256
#define HS_MAX_OSU_FRIENDLY_NAME_LEN     256
#define HS_MAX_OSU_NAI_LEN               256
#define HS_MAX_OSU_METHOD_LIST_LEN       128
#define HS_MAX_OSU_ICON_LEN              32
#define HS_MAX_OSU_SERVICE_DESC_LEN      256
#define HS_MAX_SUBSCR_REMEDIATION_URL_LEN  256

typedef struct {
    uint8_t eap_type;
    uint8_t identity[EAP_IDENTITY_LEN];
    uint8_t password[EAP_PASSWD_LEN];
    uint8_t ca_cert[EAP_FILE_NAME_LEN];
    uint8_t client_cert[EAP_FILE_NAME_LEN];
    uint8_t private_key[EAP_FILE_NAME_LEN];
    uint8_t private_key_passwd[EAP_PVT_KEY_PASSWD_LEN];
} acfg_wlan_profile_sec_eap_params_t;

typedef struct  {
    char radius_ip[IP_ADDR_LEN];
    uint32_t radius_port;
    char shared_secret[RADIUS_SHARED_SECRET_LEN];
} acfg_wlan_profile_sec_radius_params_t;

typedef struct  {
    char acct_ip[IP_ADDR_LEN];
    uint32_t acct_port;
    char shared_secret[ACCT_SHARED_SECRET_LEN];
} acfg_wlan_profile_sec_acct_server_params_t;

typedef struct {
    uint8_t hs_enabled;
    uint8_t iw_enabled;
    uint8_t network_type;
    uint8_t internet;
    uint8_t asra;
    uint8_t esr;
    uint8_t uesa;
    uint8_t venue_group;
    uint8_t venue_type;
    char hessid[ACFG_MACSTR_LEN];
    uint8_t roaming_consortium[HS_MAX_ROAMING_CONSORTIUM_LEN];
    uint8_t roaming_consortium2[HS_MAX_ROAMING_CONSORTIUM_LEN];
    char venue_name[HS_MAX_VENUE_NAME_LEN];
    uint8_t manage_p2p;
    uint8_t disable_dgaf;
    uint8_t network_auth_type[HS_MAX_AUTH_TYPE_LEN];
    uint8_t ipaddr_type_availability[HS_MAX_IPADDR_TYPE_AVL_LEN];
    uint8_t nai_realm_list[HS_MAX_NAI_REALM_LIST_NUM][HS_MAX_NAI_REALM_DATA_LEN];
    uint8_t anqp_3gpp_cellular_network[HS_MAX_3GPP_CELLULAR_NETWORK_LEN];
    uint8_t domain_name_list[HS_MAX_DOMAIN_NAME_LIST_LEN];
    uint32_t gas_frag_limit;
    uint32_t gas_comeback_delay;
    uint8_t qos_map_set[HS_MAX_QOS_MAP_SET_LEN];
    uint32_t proxy_arp;
    uint32_t osen;
    uint32_t anqp_domain_id;
    uint32_t hs20_deauth_req_timeout;
    uint8_t hs20_operator_friendly_name[HS_MAX_VENUE_NAME_LEN];
    uint8_t hs20_wan_metrics[HS_MAX_WAN_METRICS_LEN];
    uint8_t hs20_conn_capab[HS_MAX_CONNECTION_CAP_LEN];
    uint8_t hs20_operating_class[HS_MAX_OPERATING_CLASS_LEN];
    uint8_t hs20_icon[HS_MAX_HS20_ICON_LEN];
    uint8_t osu_ssid[HS_MAX_OSU_SSID_LEN];
    uint8_t osu_server_uri[HS_MAX_OSU_SERVER_URI_LEN];
    uint8_t osu_friendly_name[HS_MAX_OSU_FRIENDLY_NAME_LEN];
    uint8_t osu_nai[HS_MAX_OSU_NAI_LEN];
    uint8_t osu_method_list[HS_MAX_OSU_METHOD_LIST_LEN];
    uint8_t osu_icon[HS_MAX_OSU_ICON_LEN];
    uint8_t osu_service_desc[HS_MAX_OSU_SERVICE_DESC_LEN];
    uint8_t subscr_remediation_url[HS_MAX_SUBSCR_REMEDIATION_URL_LEN];
    uint32_t subscr_remediation_method;
}acfg_wlan_profile_sec_hs_iw_param_t;

typedef struct {
    uint8_t radio_name[ACFG_MAX_IFNAME]; /* key */
    uint8_t chan; /* read-write, mandatory */
    uint32_t freq; /* in Mhz read-write, mandatory */
    uint16_t country_code; /*read-write, optional; 0 if unspecified */
    uint8_t radio_mac[ACFG_MACSTR_LEN];
    /* read-write, optional; 0 if unspecified */
    uint8_t macreq_enabled; /* read-write, optoinal, 0 if unspecified */
    uint8_t ampdu;
    uint32_t ampdu_limit_bytes;
    uint8_t ampdu_subframes;
    uint8_t  aggr_burst; /* 0 if unspecified */
    uint32_t aggr_burst_dur; /* 0 if unspecified */
    uint8_t  ext_icm; /* 0 if unspecified */
    uint8_t sta_dfs_enable; /* 0 if unspecified */
    uint8_t atf_strict_sched; /* -1 if unspecified */
    uint8_t ccathena;
    int16_t cca_det_level; /* -70dBm if unspecified */
    uint16_t cca_det_margin; /* 3 if unspecified */
    uint8_t  beacon_burst_mode; /* 0 or 1; 0 if unspecified */
    uint8_t preCAC; /* Enable zero wait DFS or pre CAC */
    uint32_t interCACChan; /* Intermediate non-DFS Channel while preCAC is being done */
    uint32_t preCACtimeout; /* update preCAC timeout */
} acfg_wlan_profile_radio_params_t;

typedef struct {
    acfg_wlan_profile_sec_meth_e sec_method;  /* read-write, mandatory */
    acfg_wlan_profile_cipher_meth_e cipher_method; /* read-write, mandatory*/
    acfg_wlan_profile_cipher_meth_e g_cipher_method; /* read-write, mandatory*/
    char psk[ACFG_MAX_PSK_LEN];
    /* read-write, mandatory if sec_method = WPA/WPA2/WPS, 0 otherwise */
    char wep_key0[ACFG_MAX_WEP_KEY_LEN];
    /* read-write, mandatory if sec_method = WEP, 0 otherwise */
    char wep_key1[ACFG_MAX_WEP_KEY_LEN];
    /* read-write, optional, 0 if unspecified */
    char wep_key2[ACFG_MAX_WEP_KEY_LEN];
    /* read-write, optional, 0 if unspecified */
    char wep_key3[ACFG_MAX_WEP_KEY_LEN];
    /* read-write, optional, 0 if unspecified */
    uint8_t wep_key_defidx;
    uint32_t wps_pin;  /* write-only, 0 if unspecified */
    uint8_t  wps_flag;
    char  wps_manufacturer[ACFG_WSUPP_PARAM_LEN];
    char  wps_model_name[ACFG_WSUPP_PARAM_LEN];
    char  wps_model_number[ACFG_WSUPP_PARAM_LEN];
    char  wps_serial_number[ACFG_WSUPP_PARAM_LEN];
    char  wps_device_type[ACFG_WSUPP_PARAM_LEN];
    char  wps_config_methods[ACFG_WPS_CONFIG_METHODS_LEN];
    char  wps_upnp_iface[ACFG_MAX_IFNAME];
    uint8_t  wps_friendly_name[ACFG_WSUPP_PARAM_LEN];
    uint8_t  wps_man_url[ACFG_WSUPP_PARAM_LEN];
    uint8_t  wps_model_desc[ACFG_WSUPP_PARAM_LEN];
    uint8_t  wps_upc[ACFG_WSUPP_PARAM_LEN];
    uint32_t wps_pbc_in_m1;
    char wps_device_name[ACFG_WSUPP_PARAM_LEN];
    acfg_wlan_profile_sec_eap_params_t eap_param;
    uint32_t radius_retry_primary_interval;
    acfg_wlan_profile_sec_radius_params_t pri_radius_param;
    acfg_wlan_profile_sec_radius_params_t sec1_radius_param;
    acfg_wlan_profile_sec_radius_params_t sec2_radius_param;
    acfg_wlan_profile_sec_acct_server_params_t pri_acct_server_param;
    acfg_wlan_profile_sec_acct_server_params_t sec1_acct_server_param;
    acfg_wlan_profile_sec_acct_server_params_t sec2_acct_server_param;
    acfg_wlan_profile_sec_hs_iw_param_t hs_iw_param;
    /* Might be "a", "b", "g", or "ag" + \0 */
    char  wps_rf_bands[ACFG_WPS_RF_BANDS_LEN];
    uint8_t sha256;
    uint8_t ieee80211w;
    uint8_t group_mgmt_cipher;
    uint32_t assoc_sa_query_max_timeout;
    uint32_t assoc_sa_query_retry_timeout;
} acfg_wlan_profile_security_params_t;

typedef struct {
    /*
     *  This list is to add mac addresses to the first ACL list
     */
    uint8_t  acfg_acl_node_list[ACFG_MAX_ACL_NODE][ACFG_MACADDR_LEN];
    uint8_t  num_node;
    acfg_wlan_profile_node_acl_t node_acl; /* Set acl policy of first ACL list*/
    /*
     *  This list is to add mac addresses to the secondary ACL list
     */
    uint8_t  acfg_acl_node_list_sec[ACFG_MAX_ACL_NODE][ACFG_MACADDR_LEN];
    uint8_t  num_node_sec;
    acfg_wlan_profile_node_acl_t node_acl_sec; /* Set acl policy of secondary ACL list*/
} acfg_wlan_profile_node_params_t;

typedef struct {
    uint8_t vap_name[ACFG_MAX_IFNAME]; /* key */
    char radio_name[ACFG_MAX_IFNAME]; /* key */
    acfg_opmode_t opmode; /* read-write, mandatory */
    int32_t      vapid;  /* for MAC addr request */
    uint32_t phymode; /* read-write, mandatory */
    uint8_t      ampdu;
    char ssid[ACFG_MAX_SSID_LEN + 1]; /* read-write, mandatory */
    uint32_t bitrate; /* read-write, mandatory*/
    uint8_t rate[16]; /* read-write, mandatory*/
    uint32_t retries;
    uint32_t txpow; /* read-write mandatory */
    uint32_t rssi; /* read-only */
    uint32_t beacon_interval; /* read-write, optional; 0 if unspecified */
    uint32_t rts_thresh; /* read-write, optional; 0 if unspecified */
    uint32_t frag_thresh; /* read-write, optional; 0 if unspecified */
    uint8_t vap_mac[ACFG_MACSTR_LEN];
    acfg_wlan_profile_security_params_t security_params;
    acfg_wlan_profile_node_params_t node_params;
    acfg_wds_params_t wds_params;
    uint32_t vlanid;
    char bridge[ACFG_MAX_IFNAME];
    acfg_wlan_profile_vendor_param_t vendor_param[ACFG_MAX_VENDOR_PARAMS];
    uint8_t num_vendor_params;
    /* read-write, optional; 0 if unspecified */
    uint8_t num_nai_realm_data;
    uint32_t pureg;
    uint32_t puren;
    uint32_t hide_ssid;
    uint32_t doth;
    uint32_t client_isolation;
    uint8_t coext;
    uint32_t uapsd;
    uint8_t  shortgi;
    uint32_t amsdu; /* 0 if unspecified */
    uint32_t max_clients; /* 0 if unspecified */
    uint8_t atf_options;
    uint8_t dtim_period;
    acfg_atf_params_t atf;
    uint8_t primary_vap;
    u_int16_t bcn_rate;
    uint8_t implicitbf;
    uint8_t wnm;
    uint8_t rrm;
} acfg_wlan_profile_vap_params_t;

typedef struct {
    char ctrl_hapd[ACFG_CTRL_IFACE_LEN];
    char ctrl_wpasupp[ACFG_CTRL_IFACE_LEN];
    acfg_wlan_profile_radio_params_t radio_params;
    acfg_wlan_profile_vap_params_t vap_params[ACFG_MAX_VAPS];
    uint8_t num_vaps;
    void *priv; /* acfg library internal */
} acfg_wlan_profile_t;

typedef struct {
    uint8_t  wps_state;
} acfg_hapd_param_t;

typedef enum {
    PRIVATE_NETWORK,
    PRIVATE_NETWORK_WITH_GUEST_ACCESS,
    CHARGEABLE_PUBLIC_NETWORK,
    FREE_PUBLIC_NETWORK,
    PERSONAL_DEVICE_NETWORK,
    EMERGENCY_SERVICES_ONLY_NETWORK,
    RESERVED_START_NETWORK = 6,
    RESERVED_END_NETWORK = 13,
    TEST_OR_EXPERIMENTAL_NETWORK,
    WILDCARD_NETWORK,
    DEFAULT_NETWORK_TYPE = CHARGEABLE_PUBLIC_NETWORK,
} IW_NETWORK_TYPE;

typedef enum {
    INTERNET_ACCESS_UNSPECIFIED,
    INTERNET_ACCESS_AVAILABLE,
    DEFAULT_INTERNET = INTERNET_ACCESS_UNSPECIFIED,
} IW_INTERNET;

typedef enum {
    NO_ADDITIONAL_STEP_REQUIRED_RSNA_ENABLED,
    ADDITIONAL_STEP_REQUIRED,
    DEFAULT_ASRA = NO_ADDITIONAL_STEP_REQUIRED_RSNA_ENABLED,
} IW_ASRA;

typedef enum {
    EMERGENCY_SERVICES_UNSPECIFIED,
    EMERGENCY_SERVICES_REACHABLE,
    DEFAULT_ESR = EMERGENCY_SERVICES_UNSPECIFIED,
} IW_ESR;

typedef enum {
    NO_UNAUTHENTICATED_EMERGENCY_SERVICES_REACHABLE,
    UNAUTHENTICATED_EMERGENCY_SERVICES_REACHABLE,
    DEFAULT_UESA = NO_UNAUTHENTICATED_EMERGENCY_SERVICES_REACHABLE,
} IW_UESA;

typedef enum {
    VENUE_GROUP_UNSPECIFIED,
    VENUE_GROUP_ASSEMBLY,
    VENUE_GROUP_BUSINESS,
    VENUE_GROUP_EDUCATIONAL,
    VENUE_GROUP_FACTORY_INDUSTRIAL,
    VENUE_GROUP_INSTITUTIONAL,
    VENUE_GROUP_MERCANTILE,
    VENUE_GROUP_RESIDENTIAL,
    VENUE_GROUP_STORAGE,
    VENUE_GROUP_UTILITY_MISC,
    VENUE_GROUP_VEHICULAR,
    VENUE_GROUP_OUTDOOR,
    VENUE_GROUP_RESERVED_START = 12,
    VENUE_GROUP_RESERVED_END = 255,
    DEFAULT_VENUE_GROUP = VENUE_GROUP_BUSINESS,
} IW_VENUE_GROUP;

#define DEFAULT_VENUE_TYPE 8 //Research and Development facility

enum {
    ACFG_WPS_PIN_SET = 1,
    ACFG_WPS_PIN_RANDOM,
};


enum acfg_event_handler_type {
    ACFG_EVENT_WPS_NEW_AP_SETTINGS = 1,
    ACFG_EVENT_WPS_SUCCESS,
};

#endif
