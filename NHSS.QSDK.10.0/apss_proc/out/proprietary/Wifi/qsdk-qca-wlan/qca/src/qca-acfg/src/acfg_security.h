
#include <linux/un.h>

#define WPA_DRIVER_ATHEROS "atheros"
#define WPA_DRIVER_ATHR "athr"
#define WPA_INTERFACE_ADD_CMD_PREFIX "INTERFACE_ADD"
#define WPA_INTERFACE_REMOVE_CMD_PREFIX "INTERFACE_REMOVE"
#define WPA_INTERFACE_MODIFY_CMD_PREFIX "SET_NETWORK"
#define WPA_ADD_NETWORK_CMD_PREFIX "ADD_NETWORK"
#define WPA_ENABLE_NETWORK_CMD_PREFIX "ENABLE_NETWORK"
#define WPA_SET_NETWORK_CMD_PREFIX   "SET_NETWORK"
#define WPA_GET_NETWORK_CMD_PREFIX   "GET_NETWORK"
#define WPA_DISABLE_NETWORK_CMD_PREFIX "DISABLE_NETWORK"
#define WPA_REMOVE_NETWORK_CMD_PREFIX "REMOVE_NETWORK"
#define WPA_SET_CMD_PREFIX             "SET"
#define WPA_WPS_PIN_CMD_PREFIX "WPS_PIN"
#define WPA_WPS_AP_PIN_CMD_PREFIX "WPS_AP_PIN"
#define WPA_WPS_PBC_CMD_PREFIX "WPS_PBC"
#define WPA_WPS_CHECK_PIN_CMD_PREFIX "WPS_CHECK_PIN"
#define WPA_WPS_CANCEL_CMD_PREFIX "WPS_CANCEL"
#define WPA_CTRL_IFACE_ATTACH_CMD "ATTACH"
#define WPA_CTRL_IFACE_DETACH_CMD "DETACH"

#define WPA_HAPD_GET_CONFIG_CMD_PREFIX "GET_CONFIG"

#define WPA_SUPPLICANT_BIN "wpa_supplicant"

#define WPA_SUPP_MODIFY_SSID            0x00000001
#define WPA_SUPP_MODIFY_SEC_METHOD      0x00000002
#define WPA_SUPP_MODIFY_CIPHER             0x00000004
#define WPA_SUPP_MODIFY_BSSID             0x00000008

#define CTRL_IFACE_STRING "ctrl_interface"

#define WPA_SUPP_CONFFILE_PREFIX "/tmp/wpa_supplicant"
#define ACFG_HAPD_GLOBAL_CTRL_IFACE "global"
#define ACFG_GLOBAL_CTRL_IFACE "global"

#define WPA_SUPP_KEYMGMT_WPA_PSK "WPA-PSK"
#define WPA_SUPP_PROTO_WPA "WPA"
#define WPA_SUPP_PROTO_WPA2 "WPA2"
#define WPA_SUPP_PAIRWISE_CIPHER_TKIP "TKIP"
#define WPA_SUPP_PAIRWISE_CIPHER_CCMP "CCMP"

#define WPA_PSK_ASCII_LEN_MAX 63
#define WPA_PSK_ASCII_LEN_MIN 8
#define WPA_PSK_HEX_LEN 64

#define WPS_TIMEOUT 300
#define WPS_PBC_TIMEOUT 120

#define ACFG_MAX_WPS_FILE_SIZE  512

#define ACFG_MAX_HAPD_CONFIG_PARAM 64
#define ACFG_WPS_CONFIG_PREFIX "acfg_wps"
#define ACFG_WPS_DEV_CONFIG_PREFIX "acfg_wps_dev"

#define ACFG_IS_SEC_ENABLED(sec_method) ((sec_method == \
            ACFG_WLAN_PROFILE_SEC_METH_WPA) || \
        (sec_method == ACFG_WLAN_PROFILE_SEC_METH_WPA2) || \
        (sec_method == ACFG_WLAN_PROFILE_SEC_METH_WPAWPA2) || \
        (sec_method == ACFG_WLAN_PROFILE_SEC_METH_WPA_EAP) || \
        (sec_method == ACFG_WLAN_PROFILE_SEC_METH_WPA2_EAP) || \
        (sec_method == ACFG_WLAN_PROFILE_SEC_METH_WPS))

#define ACFG_VAP_CMP_SSID(vap1, vap2) strcmp(vap1->ssid, vap2->ssid)
#define ACFG_SEC_CMP_WPS_STATE(sec1, sec2) (sec1.wps_flag != sec2.wps_flag)

#define ACFG_SEC_CMP_WPS_PARAMS(sec1, sec2) (strcmp(sec1.wps_rf_bands, sec2.wps_rf_bands) != 0)

#define ACFG_SEC_CMP_CIPHER(sec1, sec2) ((sec1.cipher_method != \
            sec2.cipher_method) || \
        (sec1.g_cipher_method != \
         sec2.g_cipher_method))
#define ACFG_SEC_CMP_PSK(sec1, sec2) strcmp(sec1.psk,sec2.psk)
#define ACFG_SEC_CMP_SEC_METHOD(sec1, sec2) sec1.sec_method !=	\
                                                                sec2.sec_method	
#define ACFG_SEC_CMP_IEEE80211W(sec1, sec2) (sec1.ieee80211w != sec2.ieee80211w)
#define ACFG_SEC_CMP_GROUP_MGMT_CIPHER(sec1, sec2) (sec1.group_mgmt_cipher != sec2.group_mgmt_cipher)
#define ACFG_SEC_CMP_SA_QUERY_MAX_TIMEOUT(sec1, sec2) \
        (sec1.assoc_sa_query_max_timeout != sec2.assoc_sa_query_max_timeout)
#define ACFG_SEC_CMP_SA_QUERY_RETRY_TIMEOUT(sec1, sec2) \
        (sec1.assoc_sa_query_retry_timeout != sec2.assoc_sa_query_retry_timeout)

#define ACFG_SEC_CMP_RADIUS(sec1, sec2) \
    (strcmp(sec1.pri_radius_param.radius_ip, sec2.pri_radius_param.radius_ip) || \
     (sec1.pri_radius_param.radius_port != sec2.pri_radius_param.radius_port) || \
     strcmp(sec1.pri_radius_param.shared_secret, sec2.pri_radius_param.shared_secret) || \
     strcmp(sec1.sec1_radius_param.radius_ip, sec2.sec1_radius_param.radius_ip) || \
     (sec1.sec1_radius_param.radius_port != sec2.sec1_radius_param.radius_port) || \
     strcmp(sec1.sec1_radius_param.shared_secret, sec2.sec1_radius_param.shared_secret) || \
     strcmp(sec1.sec2_radius_param.radius_ip, sec2.sec2_radius_param.radius_ip) || \
     (sec1.sec2_radius_param.radius_port != sec2.sec2_radius_param.radius_port) || \
     strcmp(sec1.sec2_radius_param.shared_secret, sec2.sec2_radius_param.shared_secret))

#define ACFG_SEC_CMP_ACCT(sec1, sec2) \
    (strcmp(sec1.pri_acct_server_param.acct_ip, sec2.pri_acct_server_param.acct_ip) || \
     (sec1.pri_acct_server_param.acct_port != sec2.pri_acct_server_param.acct_port) || \
     strcmp(sec1.pri_acct_server_param.shared_secret, sec2.pri_acct_server_param.shared_secret) || \
     strcmp(sec1.sec1_acct_server_param.acct_ip, sec2.sec1_acct_server_param.acct_ip) || \
     (sec1.sec1_acct_server_param.acct_port != sec2.sec1_acct_server_param.acct_port) || \
     strcmp(sec1.sec1_acct_server_param.shared_secret, sec2.sec1_acct_server_param.shared_secret) || \
     strcmp(sec1.sec2_acct_server_param.acct_ip, sec2.sec2_acct_server_param.acct_ip) || \
     (sec1.sec2_acct_server_param.acct_port != sec2.sec2_acct_server_param.acct_port) || \
     strcmp(sec1.sec2_acct_server_param.shared_secret, sec2.sec2_acct_server_param.shared_secret))

#define ACFG_SEC_CMP_WDS(vap1, vap2) \
    (strcmp((char *)vap1->wds_params.wds_addr, (char *)vap2->wds_params.wds_addr))

#define ACFG_SEC_CMP(vap1, vap2) (ACFG_SEC_CMP_CIPHER(vap1->security_params, \
            vap2->security_params) ||\
        (ACFG_SEC_CMP_PSK(vap1->security_params, \
                          vap2->security_params)) ||\
        (ACFG_VAP_CMP_SSID(vap1, vap2)) ||\
        (ACFG_SEC_CMP_SEC_METHOD(\
                                 vap1->security_params,\
                                 vap2->security_params)) || \
        (ACFG_SEC_CMP_RADIUS(\
                             vap1->security_params,\
                             vap2->security_params)) || \
        (ACFG_SEC_CMP_ACCT(\
                           vap1->security_params,\
                           vap2->security_params)) || \
        (ACFG_SEC_CMP_WDS(vap1, vap2))) || \
        (ACFG_SEC_CMP_IEEE80211W( \
                             vap1->security_params, \
                             vap2->security_params)) || \
        (ACFG_SEC_CMP_SA_QUERY_MAX_TIMEOUT(\
                             vap1->security_params, \
                             vap2->security_params)) || \
        (ACFG_SEC_CMP_SA_QUERY_RETRY_TIMEOUT( \
                             vap1->security_params,\
                             vap2->security_params)) || \
        (ACFG_SEC_CMP_GROUP_MGMT_CIPHER( \
                             vap1->security_params, \
                             vap2->security_params))

#define ACFG_WPS_CMP(vap1, vap2) (ACFG_SEC_CMP_WPS_STATE(vap1->security_params, \
            vap2->security_params) ||\
        (ACFG_SEC_CMP_WPS_PARAMS(vap1->security_params, \
            vap2->security_params)))

#define ACFG_IS_SEC_WEP(sec_param) (((sec_param.sec_method == \
                ACFG_WLAN_PROFILE_SEC_METH_OPEN) ||\
            (sec_param.sec_method == \
             ACFG_WLAN_PROFILE_SEC_METH_SHARED) ||\
            (sec_param.sec_method == \
             ACFG_WLAN_PROFILE_SEC_METH_AUTO)) &&\
        (sec_param.cipher_method == \
         ACFG_WLAN_PROFILE_CIPHER_METH_WEP))

#define ACFG_IS_SEC_OPEN(sec_param) ((sec_param.sec_method == \
            ACFG_WLAN_PROFILE_SEC_METH_OPEN) && \
        (sec_param.cipher_method == \
         ACFG_WLAN_PROFILE_CIPHER_METH_NONE))

#define ACFG_IS_WPS_WEP_ENABLED(sec_param)  (ACFG_IS_SEC_WEP(\
            sec_param) &&\
        sec_param.wps_flag)
#define ACFG_IS_VALID_WPS(sec_param)    (sec_param.wps_flag && \
        !ACFG_IS_SEC_WEP(\
            sec_param))


enum {
    PROFILE_CREATE = 1,
    PROFILE_MODIFY,
    PROFILE_DELETE,
};

enum wpa_supp_param_type {
    ACFG_WPA_PROTO = 1,
    ACFG_WPA_KEY_MGMT,
    ACFG_WPA_UCAST_CIPHER,
    ACFG_WPA_MCAST_CIPHER,
};


struct sec_params
{
    uint8_t wpa_proto;
    uint8_t cipher;
};

struct wsupp_ev_handle {
    uint8_t ifname[32];
    int sock;
    struct sockaddr_un local;
    struct sockaddr_un dest;
    int event_set;
    acfg_opmode_t opmode;
};

enum {
    ACFG_SEC_UNCHANGED = 1,
    ACFG_SEC_SET_SECURITY,
    ACFG_SEC_DISABLE_SECURITY,
    ACFG_SEC_MODIFY_SECURITY_PARAMS,
    ACFG_SEC_DISABLE_SECURITY_CHANGED,
    ACFG_SEC_RESET_SECURITY,
};

typedef struct {
    int32_t wps_state;
    char ssid[ACFG_MAX_SSID_LEN + 1];
    int32_t ssid_len;
    uint16_t wpa;	
    uint16_t enc_type;
    uint16_t key_len;
    char   key[ACFG_MAX_PSK_LEN];
    int8_t   key_mgmt;
    int8_t auth_alg;
    char wep_key[255];
    int8_t wep_key_idx;		
} acfg_wps_cred_t;

char ctrl_hapd[ACFG_CTRL_IFACE_LEN];
char ctrl_wpasupp[ACFG_CTRL_IFACE_LEN];

uint32_t
acfg_config_security(acfg_wlan_profile_vap_params_t *vap_params);

int     
hwaddr_aton(char  *txt, uint8_t *addr);

uint32_t 
acfg_set_auth_open(acfg_wlan_profile_vap_params_t *vap_params,
        uint32_t state);

uint32_t
acfg_set_security(acfg_wlan_profile_vap_params_t *vap_params, 
        acfg_wlan_profile_vap_params_t *cur_vap_params,
        uint8_t action,
        int8_t *sec);

int acfg_get_wps_config(uint8_t *ifname, acfg_wps_cred_t *wps_cred);

int
acfg_get_open_wep_state(acfg_wlan_profile_vap_params_t *vap_params,
        acfg_wlan_profile_vap_params_t *cur_vap_params);

uint32_t
acfg_wpa_supp_delete(acfg_wlan_profile_vap_params_t *vap_params);

