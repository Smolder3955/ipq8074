/*
* Copyright (c) 2014, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 *  2014 Qualcomm Atheros, Inc.  All rights reserved. 
 *
 *  Qualcomm is a trademark of Qualcomm Technologies Incorporated, registered in the United
 *  States and other countries.  All Qualcomm Technologies Incorporated trademarks are used with
 *  permission.  Atheros is a trademark of Qualcomm Atheros, Inc., registered in
 *  the United States and other countries.  Other products and brand names may be
 *  trademarks or registered trademarks of their respective owners. 
 */

/* implements P2P protpcol layer.
 * WARNING: synchronization issues are not handled yet.
 */

#ifndef _IEEE80211_WPS_H_
#define _IEEE80211_WPS_H_

#define IEEE80211_WPS_WFA_OUI     { 0x00,0x50,0xf2 }

#define WPS_UUID_LEN                        16
#define WPS_NONCE_LEN                       16
#define WPS_AUTHENTICATOR_LEN               8
#define WPS_AUTHKEY_LEN                     32
#define WPS_KEYWRAPKEY_LEN                  16
#define WPS_EMSK_LEN                        32
#define WPS_PSK_LEN                         16
#define WPS_SECRET_NONCE_LEN                16
#define WPS_HASH_LEN                        32
#define WPS_KWA_LEN                         8
#define WPS_MGMTAUTHKEY_LEN                 32
#define WPS_MGMTENCKEY_LEN                  16
#define WPS_MGMT_KEY_ID_LEN                 16
#define WPS_OOB_DEVICE_PASSWORD_ATTR_LEN    54
#define WPS_OOB_DEVICE_PASSWORD_LEN         32
#define WPS_OOB_PUBKEY_HASH_LEN             20
#define WPS_DEV_TYPE_LEN                    8

#define MAX_CRED_COUNT 10
#define MAX_REQ_DEV_TYPE_COUNT 10

/* heavily pruned from wpa_supplicant code */
struct wps_parsed_attr {
	/* fixed length fields */
	const u8 *version; /* 1 octet */
	const u8 *version2; /* 1 octet */
	const u8 *msg_type; /* 1 octet */
	const u8 *enrollee_nonce; /* WPS_NONCE_LEN (16) octets */
	const u8 *registrar_nonce; /* WPS_NONCE_LEN (16) octets */
	const u8 *uuid_r; /* WPS_UUID_LEN (16) octets */
	const u8 *uuid_e; /* WPS_UUID_LEN (16) octets */
	const u8 *auth_type_flags; /* 2 octets */
	const u8 *encr_type_flags; /* 2 octets */
	const u8 *conn_type_flags; /* 1 octet */
	const u8 *config_methods; /* 2 octets */
	const u8 *sel_reg_config_methods; /* 2 octets */
	const u8 *primary_dev_type; /* 8 octets */
	const u8 *rf_bands; /* 1 octet */
	const u8 *assoc_state; /* 2 octets */
	const u8 *config_error; /* 2 octets */
	const u8 *dev_password_id; /* 2 octets */
	const u8 *oob_dev_password; /* WPS_OOB_DEVICE_PASSWORD_ATTR_LEN (54)
				     * octets */
	const u8 *os_version; /* 4 octets */
	const u8 *wps_state; /* 1 octet */
	const u8 *authenticator; /* WPS_AUTHENTICATOR_LEN (8) octets */
	const u8 *r_hash1; /* WPS_HASH_LEN (32) octets */
	const u8 *r_hash2; /* WPS_HASH_LEN (32) octets */
	const u8 *e_hash1; /* WPS_HASH_LEN (32) octets */
	const u8 *e_hash2; /* WPS_HASH_LEN (32) octets */
	const u8 *r_snonce1; /* WPS_SECRET_NONCE_LEN (16) octets */
	const u8 *r_snonce2; /* WPS_SECRET_NONCE_LEN (16) octets */
	const u8 *e_snonce1; /* WPS_SECRET_NONCE_LEN (16) octets */
	const u8 *e_snonce2; /* WPS_SECRET_NONCE_LEN (16) octets */
	const u8 *key_wrap_auth; /* WPS_KWA_LEN (8) octets */
	const u8 *auth_type; /* 2 octets */
	const u8 *encr_type; /* 2 octets */
	const u8 *network_idx; /* 1 octet */
	const u8 *network_key_idx; /* 1 octet */
	const u8 *mac_addr; /* ETH_ALEN (6) octets */
	const u8 *key_prov_auto; /* 1 octet (Bool) */
	const u8 *dot1x_enabled; /* 1 octet (Bool) */
	const u8 *selected_registrar; /* 1 octet (Bool) */
	const u8 *request_type; /* 1 octet */
	const u8 *response_type; /* 1 octet */
	const u8 *ap_setup_locked; /* 1 octet */
	const u8 *settings_delay_time; /* 1 octet */
	const u8 *network_key_shareable; /* 1 octet (Bool) */
	const u8 *request_to_enroll; /* 1 octet (Bool) */

	/* variable length fields */
	const u8 *manufacturer;
	size_t manufacturer_len;
	const u8 *model_name;
	size_t model_name_len;
	const u8 *model_number;
	size_t model_number_len;
	const u8 *serial_number;
	size_t serial_number_len;
	const u8 *dev_name;
	size_t dev_name_len;
	const u8 *public_key;
	size_t public_key_len;
	const u8 *encr_settings;
	size_t encr_settings_len;
	const u8 *ssid; /* <= 32 octets */
	size_t ssid_len;
	const u8 *network_key; /* <= 64 octets */
	size_t network_key_len;
	const u8 *eap_type; /* <= 8 octets */
	size_t eap_type_len;
	const u8 *eap_identity; /* <= 64 octets */
	size_t eap_identity_len;
	const u8 *authorized_macs; /* <= 30 octets */
	size_t authorized_macs_len;

	/* attributes that can occur multiple times */
	const u8 *cred[MAX_CRED_COUNT];
	size_t cred_len[MAX_CRED_COUNT];
	size_t num_cred;

	const u8 *req_dev_type[MAX_REQ_DEV_TYPE_COUNT];
	size_t num_req_dev_type;
    const u8 *requested_dev_type;
};

/* WPS Attribute Types */
enum wps_attribute {
	ATTR_AP_CHANNEL = 0x1001,
	ATTR_ASSOC_STATE = 0x1002,
	ATTR_AUTH_TYPE = 0x1003,
	ATTR_AUTH_TYPE_FLAGS = 0x1004,
	ATTR_AUTHENTICATOR = 0x1005,
	ATTR_CONFIG_METHODS = 0x1008,
	ATTR_CONFIG_ERROR = 0x1009,
	ATTR_CONFIRM_URL4 = 0x100a,
	ATTR_CONFIRM_URL6 = 0x100b,
	ATTR_CONN_TYPE = 0x100c,
	ATTR_CONN_TYPE_FLAGS = 0x100d,
	ATTR_CRED = 0x100e,
	ATTR_ENCR_TYPE = 0x100f,
	ATTR_ENCR_TYPE_FLAGS = 0x1010,
	ATTR_DEV_NAME = 0x1011,
	ATTR_DEV_PASSWORD_ID = 0x1012,
	ATTR_E_HASH1 = 0x1014,
	ATTR_E_HASH2 = 0x1015,
	ATTR_E_SNONCE1 = 0x1016,
	ATTR_E_SNONCE2 = 0x1017,
	ATTR_ENCR_SETTINGS = 0x1018,
	ATTR_ENROLLEE_NONCE = 0x101a,
	ATTR_FEATURE_ID = 0x101b,
	ATTR_IDENTITY = 0x101c,
	ATTR_IDENTITY_PROOF = 0x101d,
	ATTR_KEY_WRAP_AUTH = 0x101e,
	ATTR_KEY_ID = 0x101f,
	ATTR_MAC_ADDR = 0x1020,
	ATTR_MANUFACTURER = 0x1021,
	ATTR_MSG_TYPE = 0x1022,
	ATTR_MODEL_NAME = 0x1023,
	ATTR_MODEL_NUMBER = 0x1024,
	ATTR_NETWORK_INDEX = 0x1026,
	ATTR_NETWORK_KEY = 0x1027,
	ATTR_NETWORK_KEY_INDEX = 0x1028,
	ATTR_NEW_DEVICE_NAME = 0x1029,
	ATTR_NEW_PASSWORD = 0x102a,
	ATTR_OOB_DEVICE_PASSWORD = 0x102c,
	ATTR_OS_VERSION = 0x102d,
	ATTR_POWER_LEVEL = 0x102f,
	ATTR_PSK_CURRENT = 0x1030,
	ATTR_PSK_MAX = 0x1031,
	ATTR_PUBLIC_KEY = 0x1032,
	ATTR_RADIO_ENABLE = 0x1033,
	ATTR_REBOOT = 0x1034,
	ATTR_REGISTRAR_CURRENT = 0x1035,
	ATTR_REGISTRAR_ESTABLISHED = 0x1036,
	ATTR_REGISTRAR_LIST = 0x1037,
	ATTR_REGISTRAR_MAX = 0x1038,
	ATTR_REGISTRAR_NONCE = 0x1039,
	ATTR_REQUEST_TYPE = 0x103a,
	ATTR_RESPONSE_TYPE = 0x103b,
	ATTR_RF_BANDS = 0x103c,
	ATTR_R_HASH1 = 0x103d,
	ATTR_R_HASH2 = 0x103e,
	ATTR_R_SNONCE1 = 0x103f,
	ATTR_R_SNONCE2 = 0x1040,
	ATTR_SELECTED_REGISTRAR = 0x1041,
	ATTR_SERIAL_NUMBER = 0x1042,
	ATTR_WPS_STATE = 0x1044,
	ATTR_SSID = 0x1045,
	ATTR_TOTAL_NETWORKS = 0x1046,
	ATTR_UUID_E = 0x1047,
	ATTR_UUID_R = 0x1048,
	ATTR_VENDOR_EXT = 0x1049,
	ATTR_VERSION = 0x104a,
	ATTR_X509_CERT_REQ = 0x104b,
	ATTR_X509_CERT = 0x104c,
	ATTR_EAP_IDENTITY = 0x104d,
	ATTR_MSG_COUNTER = 0x104e,
	ATTR_PUBKEY_HASH = 0x104f,
	ATTR_REKEY_KEY = 0x1050,
	ATTR_KEY_LIFETIME = 0x1051,
	ATTR_PERMITTED_CFG_METHODS = 0x1052,
	ATTR_SELECTED_REGISTRAR_CONFIG_METHODS = 0x1053,
	ATTR_PRIMARY_DEV_TYPE = 0x1054,
	ATTR_SECONDARY_DEV_TYPE_LIST = 0x1055,
	ATTR_PORTABLE_DEV = 0x1056,
	ATTR_AP_SETUP_LOCKED = 0x1057,
	ATTR_APPLICATION_EXT = 0x1058,
	ATTR_EAP_TYPE = 0x1059,
	ATTR_IV = 0x1060,
	ATTR_KEY_PROVIDED_AUTO = 0x1061,
	ATTR_802_1X_ENABLED = 0x1062,
	ATTR_APPSESSIONKEY = 0x1063,
	ATTR_WEPTRANSMITKEY = 0x1064,
	ATTR_REQUESTED_DEV_TYPE = 0x106a,
	ATTR_EXTENSIBILITY_TEST = 0x10fa /* _NOT_ defined in the spec */
};

#ifndef ETH_ALEN
#define ETH_ALEN 6
#endif

#endif
