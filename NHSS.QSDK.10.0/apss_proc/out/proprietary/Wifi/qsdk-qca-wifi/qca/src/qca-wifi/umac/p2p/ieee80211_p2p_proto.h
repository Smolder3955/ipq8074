/*
* Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
/*
 * P2P Protocol specific definitions.
 */

#ifndef IEEE80211_P2P_PROTO
#define IEEE80211_P2P_PROTO

#define IEEE80211_P2P_IE_ID       0xdd

#define IEEE80211_P2P_WPA_VER     0x01
#define IEEE80211_P2P_WPS_VER     0x04

/* Invitation Flags */
#define P2P_INVITATION_FLAGS_TYPE_REINVOKE_PERSISTENT_GRP   BIT(0)
#define P2P_INVITATION_FLAGS_TYPE_JOIN_ACTIVE_GRP           0

#define IEEE80211_P2P_MIN_RATE    12  /* minimum ieee rate is 12 (6Mbps) */
/*
 * P2P IE structural definition.
 */
struct ieee80211_p2p_ie {
    u_int8_t  p2p_id;
    u_int8_t  p2p_len;
    u_int8_t  p2p_oui[3];
    u_int8_t  p2p_oui_type;
} __packed;

/* Offset from the start of action frame (without 802.11 header) to the start of IE. */
#define IEEE80211_P2P_ACTION_FR_IE_OFFSET   8

/* p2p status code */
enum p2p_status_code {
    P2P_SC_SUCCESS = 0,
    P2P_SC_FAIL_INFO_CURRENTLY_UNAVAILABLE = 1,
    P2P_SC_FAIL_INCOMPATIBLE_PARAMS = 2,
    P2P_SC_FAIL_LIMIT_REACHED = 3,
    P2P_SC_FAIL_INVALID_PARAMS = 4,
    P2P_SC_FAIL_UNABLE_TO_ACCOMMODATE = 5,
    P2P_SC_FAIL_PREV_PROTOCOL_ERROR = 6,
    P2P_SC_FAIL_NO_COMMON_CHANNELS = 7,
    P2P_SC_FAIL_UNKNOWN_GROUP = 8,
    P2P_SC_FAIL_BOTH_GO_INTENT_15 = 9,
    P2P_SC_FAIL_INCOMPATIBLE_PROV_METHOD = 10,
    P2P_SC_FAIL_REJECTED_BY_USER = 11,
};

/* P2P Sub element defintions (according to table 5 of Wifi's P2P spec) */

#define IEEE80211_P2P_SUB_ELEMENT_STATUS                    0
#define IEEE80211_P2P_SUB_ELEMENT_MINOR_REASON              1
#define IEEE80211_P2P_SUB_ELEMENT_CAPABILITY                2
#define IEEE80211_P2P_SUB_ELEMENT_DEVICE_ID                 3
#if MAV_P2P_SUBTYPE
#define IEEE80211_P2P_SUB_ELEMENT_GO_INTENT                 12
#else	/* MAV_P2P_SUBTYPE */
#define IEEE80211_P2P_SUB_ELEMENT_GO_INTENT                 4
#endif	/* MAV_P2P_SUBTYPE */
#define IEEE80211_P2P_SUB_ELEMENT_CONFIGURATION_TIMEOUT     5
#define IEEE80211_P2P_SUB_ELEMENT_LISTEN_CHANNEL            6
#define IEEE80211_P2P_SUB_ELEMENT_GROUP_BSSID               7
#define IEEE80211_P2P_SUB_ELEMENT_EXTENDED_LISTEN_TIMING    8
#define IEEE80211_P2P_SUB_ELEMENT_INTENDED_INTERFACE_ADDR   9
#define IEEE80211_P2P_SUB_ELEMENT_MANAGEABILITY             10
#define IEEE80211_P2P_SUB_ELEMENT_CHANNEL_LIST              11
#if MAV_P2P_SUBTYPE
#define IEEE80211_P2P_SUB_ELEMENT_NOA                       4
#else	/* MAV_P2P_SUBTYPE */
#define IEEE80211_P2P_SUB_ELEMENT_NOA                       12
#endif	/* MAV_P2P_SUBTYPE */
#define IEEE80211_P2P_SUB_ELEMENT_DEVICE_INFO               13
#define IEEE80211_P2P_SUB_ELEMENT_GROUP_INFO                14
#define IEEE80211_P2P_SUB_ELEMENT_GROUP_ID                  15
#define IEEE80211_P2P_SUB_ELEMENT_INTERFACE                 16
#define IEEE80211_P2P_SUB_ELEMENT_OP_CHANNEL                17
#define IEEE80211_P2P_SUB_ELEMENT_INVITATION_FLAGS          18
#define IEEE80211_P2P_SUB_ELEMENT_VENDOR                    221

struct ieee80211_p2p_sub_element_capability {
    u_int8_t     p2p_sub_id;
    u_int8_t     p2p_sub_len;
    u_int8_t     service_discovery      :1,
                 client_discoverability :1,
                 cocurrent_operation    :1,
                 infrastrucuture_managed:1,
                 device_limit           :1,
        reserved                        :3;
    u_int8_t     group_capability;
} __packed;

#define IEEE80211_P2P_NOA_DESCRIPTOR_LEN 13
struct ieee80211_p2p_noa_descriptor {
    u_int8_t   type_count; /* 255: continuous schedule, 0: reserved */
    u_int32_t  duration ;  /* Absent period duration in micro seconds */
    u_int32_t  interval;   /* Absent period interval in micro seconds */
    u_int32_t  start_time; /* 32 bit tsf time when in starts */
} __packed;

#define IEEE80211_MAX_NOA_DESCRIPTORS 4
/*
 * Length = (2 octets for Index and CTWin/Opp PS) and
 * (13 octets for each NOA Descriptors)
 */
#define IEEE80211_NOA_IE_SIZE(num_desc)     (2 + (13 * (num_desc)))

#define IEEE80211_P2P_NOE_IE_OPP_PS_SET                     (0x80)
#define IEEE80211_P2P_NOE_IE_CTWIN_MASK                     (0x7F)

struct ieee80211_p2p_sub_element_noa {
    u_int8_t        p2p_sub_id;
    u_int8_t        p2p_sub_len;
    u_int8_t        index;           /* identifies instance of NOA su element */
    u_int8_t        oppPS:1,         /* oppPS state of the AP */
                    ctwindow:7;      /* ctwindow in TUs */
    u_int8_t        num_descriptors; /* number of NOA descriptors */
    struct ieee80211_p2p_noa_descriptor noa_descriptors[IEEE80211_MAX_NOA_DESCRIPTORS];
};

/* P2P public action frame types as defined in Table 48, p2p spec. v1.0 */
enum p2p_action_frame_type {
    P2P_GO_NEG_REQ = 0,
    P2P_GO_NEG_RESP = 1,
    P2P_GO_NEG_CONF = 2,
    P2P_INVITATION_REQ = 3,
    P2P_INVITATION_RESP = 4,
    P2P_DEV_DISC_REQ = 5,
    P2P_DEV_DISC_RESP = 6,
    P2P_PROV_DISC_REQ = 7,
    P2P_PROV_DISC_RESP = 8
};

void ieee80211_p2p_setup_rates(wlan_if_t vaphandle);
#endif
