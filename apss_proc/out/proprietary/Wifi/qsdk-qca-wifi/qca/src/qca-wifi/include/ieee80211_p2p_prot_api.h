/*
* Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 * Copyright (c) 2010, Atheros Communications
 *
 * This file contains the definitions for P2P Protocol Device's API.
 *
 */
/*
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */


#ifndef _IEEE80211_P2P_PROT_API_H_
#define _IEEE80211_P2P_PROT_API_H_

#include <ieee80211P2P_api.h>

typedef struct ieee80211_p2p_prot *wlan_p2p_prot_t;

/* The action frame types */
typedef enum {
    IEEE80211_P2P_PROT_FTYPE_GO_NEG_REQ = 1,/* GO Negotiation Request frame */
    IEEE80211_P2P_PROT_FTYPE_GO_NEG_RESP,   /* GO Negotiation Response frame */
    IEEE80211_P2P_PROT_FTYPE_GO_NEG_CONF,   /* GO Negotiation Confirm frame */
    IEEE80211_P2P_PROT_FTYPE_PROV_DISC_REQ, /* Provision Discovery Request frame */
    IEEE80211_P2P_PROT_FTYPE_PROV_DISC_RESP,/* Provision Discovery Response frame */
    IEEE80211_P2P_PROT_FTYPE_INVIT_REQ,     /* Invitation Request frame */
    IEEE80211_P2P_PROT_FTYPE_INVIT_RESP,    /* Invitation Response frame */
} wlan_p2p_prot_frame_type;

/* Event type for the callback from P2P Protocol devce */
typedef enum {
    IEEE80211_P2P_PROT_EVENT_TX_ACT_FR_COMPL,   /* An action frame transmit is completed */
    IEEE80211_P2P_PROT_EVENT_RX_ACT_FR_IND,     /* An action frame is received */
    IEEE80211_P2P_PROT_EVENT_DISC_SCAN_COMPL,   /* Device Discovery Scan has completed */
} wlan_p2p_prot_event_type;

typedef enum {
    /* device info */
    WLAN_P2P_PROT_PRIMARY_DEVICE_TYPE,            /* primary device type */
	WLAN_P2P_PROT_SECONDARY_DEVICE_TYPES,         /* secondary device types */
    WLAN_P2P_PROT_DEVICE_CONFIG_METHODS,          /* WSC config methods */
    /* device capabilities */
    WLAN_P2P_PROT_DEVICE_CAPABILITY,             /* device capability bitmap */
    WLAN_P2P_PROT_SUPPORT_SERVICE_DISCOVERY,     /* flag: to indicated if the device support service discovery */
    WLAN_P2P_PROT_SUPPORT_CLIENT_DISCOVERABILITY,/* flag: to indicated if the device support client discoverability */
    WLAN_P2P_PROT_SUPPORT_CONCURRENT_OPERATION,  /* flag: to indicated if the device supports concurrent operation */
    WLAN_P2P_PROT_INFRASTRUCTURE_MANAGED,        /* flag: to indicated the device is infrastructure managed */
    WLAN_P2P_PROT_DEVICE_LIMIT,                  /* flag: to indicated if the P2P device limit has reached */
    WLAN_P2P_PROT_SUPPORT_INVITATION_PROCEDURE,  /* flag: to indicate if the device support invitation procedure */
    /* GO capabilities */
    WLAN_P2P_PROT_GROUP_CAPABILITY,              /* group capability bitmap */
    WLAN_P2P_PROT_GROUP_PERSISTENT,              /* flag: to indicate if the group is persistent */
    WLAN_P2P_PROT_GROUP_LIMIT,                   /* flag: to indicate if the group owner allows more P2P clients */
    WLAN_P2P_PROT_GROUP_INTRA_BSS,               /* flag: to indicate if the group owner supports intra bss distribution  */
    WLAN_P2P_PROT_GROUP_CROSS_CONNECTION,        /* flag: to indicate if the group owner is   */
    WLAN_P2P_PROT_GROUP_PERSISTENT_RECONNECT,    /* flag: to indicate if the group owner is   */
    WLAN_P2P_PROT_GROUP_FORMATION,               /* flag: to indicate if device is acting as GO during provisioning */
    /* Misc capabilities */
    WLAN_P2P_PROT_WPS_VERSION_ENABLED,           /* WPS Versions enabled */
    WLAN_P2P_PROT_MAX_GROUP_LIMIT,               /* Max. number of P2P clients to be allowed by the group owner */
    WLAN_P2P_PROT_DEVICE_NAME,                   /* WPS Device Name */
    WLAN_P2P_PROT_LISTEN_ST_DISC,                /* Listen State Discoverability mode */
} wlan_p2p_prot_param;

/* P2P Capability - Device Capability bitmap */
#define WLAN_P2P_PROT_DEV_CAP_SERVICE_DISCOVERY        0x01 
#define WLAN_P2P_PROT_DEV_CAP_CLIENT_DISCOVERABILITY   0x02 
#define WLAN_P2P_PROT_DEV_CAP_CONCURRENT_OPER          0x04
#define WLAN_P2P_PROT_DEV_CAP_INFRA_MANAGED            0x08 
#define WLAN_P2P_PROT_DEV_CAP_DEVICE_LIMIT             0x10
#define WLAN_P2P_PROT_DEV_CAP_INVITATION_PROCEDURE     0x20

/* P2P Capability - Group Capability bitmap */
#define WLAN_P2P_PROT_GROUP_CAP_GROUP_OWNER            0x01
#define WLAN_P2P_PROT_GROUP_CAP_PERSISTENT_GROUP       0x02
#define WLAN_P2P_PROT_GROUP_CAP_GROUP_LIMIT            0x04
#define WLAN_P2P_PROT_GROUP_CAP_INTRA_BSS_DIST         0x08
#define WLAN_P2P_PROT_GROUP_CAP_CROSS_CONN             0x10
#define WLAN_P2P_PROT_GROUP_CAP_PERSISTENT_RECONN      0x20
#define WLAN_P2P_PROT_GROUP_CAP_GROUP_FORMATION        0x40

/* From WPS definitions: Attribute Types */
/* Event struct when Send Complete for GO Negotiation Request frame */
typedef struct _wlan_p2p_prot_event_tx_go_neg_req_compl {
    u_int8_t        peer_addr[IEEE80211_ADDR_LEN];          /* peer mac address to send frame */
    u_int8_t        dialog_token;                           /* Dialog token used for this request frame */
    int             status;                                 /* status of this transmission */
    u_int32_t       out_ie_offset;                          /* Offset of buffer that contains additional IE to append to frame */
    u_int32_t       out_ie_buf_len;                         /* additional IE buf size to append to frame */
} wlan_p2p_prot_event_tx_go_neg_req_compl;

/* Event struct when Send Complete for GO Negotiation Response frame */
typedef wlan_p2p_prot_event_tx_go_neg_req_compl wlan_p2p_prot_event_tx_go_neg_resp_compl;

/* Event struct when Send Complete for GO Negotiation Confirm frame */
typedef wlan_p2p_prot_event_tx_go_neg_req_compl wlan_p2p_prot_event_tx_go_neg_conf_compl;

/* Event struct when Send Complete for Provision Discovery Request frame */
typedef struct _wlan_p2p_prot_event_tx_prov_disc_req_compl {
    u_int8_t        peer_addr[IEEE80211_ADDR_LEN];          /* peer mac address to send frame */
    u_int8_t        rx_addr[IEEE80211_ADDR_LEN];            /* Receiver mac address used in the PROV_DISC_REQUEST frame */
    u_int8_t        dialog_token;                           /* Dialog token used for this request frame */
    int             status;                                 /* status of this transmission */
    u_int32_t       out_ie_offset;                          /* Offset of buffer that contains additional IE to append to frame */
    u_int32_t       out_ie_buf_len;                         /* additional IE buf size to append to frame */
} wlan_p2p_prot_event_tx_prov_disc_req_compl;

/* Event struct when Send Complete for Invitation Request frame */
typedef wlan_p2p_prot_event_tx_prov_disc_req_compl wlan_p2p_prot_event_tx_invit_req_compl;

/* Event struct when Send Complete for Provision Discovery Response frame */
typedef struct _wlan_p2p_prot_event_tx_prov_disc_resp_compl {
    u_int8_t        rx_addr[IEEE80211_ADDR_LEN];            /* Receiver mac address used in the PROV_DISC_REQUEST frame */
    u_int8_t        dialog_token;                           /* Dialog token used for this request frame */
    int             status;                                 /* status of this transmission */
    u_int32_t       out_ie_offset;                          /* Offset of buffer that contains additional IE to append to frame */
    u_int32_t       out_ie_buf_len;                         /* additional IE buf size to append to frame */
} wlan_p2p_prot_event_tx_prov_disc_resp_compl;

typedef wlan_p2p_prot_event_tx_prov_disc_resp_compl wlan_p2p_prot_event_tx_invit_resp_compl;

/* event struct when type is IEEE80211_P2P_PROT_EVENT_TX_ACT_FR_COMPL */
typedef struct _wlan_p2p_prot_event_tx_act_fr_compl {
    wlan_p2p_prot_frame_type    frame_type; /* Transmit frame type */

    union {
        wlan_p2p_prot_event_tx_go_neg_req_compl     go_neg_req;     /* frame_type==IEEE80211_P2P_PROT_FTYPE_GO_NEG_REQ */
        wlan_p2p_prot_event_tx_go_neg_resp_compl    go_neg_resp;    /* frame_type==IEEE80211_P2P_PROT_FTYPE_GO_NEG_RESP */
        wlan_p2p_prot_event_tx_go_neg_conf_compl    go_neg_conf;    /* frame_type==IEEE80211_P2P_PROT_FTYPE_GO_NEG_CONF */
        wlan_p2p_prot_event_tx_prov_disc_req_compl  prov_disc_req;  /* frame_type==IEEE80211_P2P_PROT_FTYPE_PROV_DISC_REQ */
        wlan_p2p_prot_event_tx_prov_disc_resp_compl prov_disc_resp; /* frame_type==IEEE80211_P2P_PROT_FTYPE_PROV_DISC_RESP */
        wlan_p2p_prot_event_tx_invit_req_compl      invit_req;      /* frame_type==IEEE80211_P2P_PROT_FTYPE_INVIT_REQ */
        wlan_p2p_prot_event_tx_invit_resp_compl     invit_resp;     /* frame_type==IEEE80211_P2P_PROT_FTYPE_INVIT_RESP */
    };

} wlan_p2p_prot_event_tx_act_fr_compl;

/* Event struct when Rx indication for GO Negotiation Request frame */
typedef struct _wlan_p2p_prot_event_rx_go_neg_req_ind {
    u_int8_t        peer_addr[IEEE80211_ADDR_LEN];          /* peer mac address to send frame */
    u_int8_t        dialog_token;                           /* Dialog token used for this request frame */
    void            *request_context;                       /* request context to be used when sending a reply frame */
    u_int8_t        *add_ie_buf;                            /* additional IE to append to frame */
    u_int32_t       add_ie_buf_len;                         /* additional IE buf size to append to frame */
} wlan_p2p_prot_event_rx_go_neg_req_ind;

/* Event struct when Rx indication for GO Negotiation Response frame */
typedef wlan_p2p_prot_event_rx_go_neg_req_ind   wlan_p2p_prot_event_rx_go_neg_resp_ind;

/* Event struct when Rx indication for GO Negotiation Confirmation frame */
typedef struct _wlan_p2p_prot_event_rx_go_neg_conf_ind {
    u_int8_t        peer_addr[IEEE80211_ADDR_LEN];          /* peer mac address to send frame */
    u_int8_t        dialog_token;                           /* Dialog token used for this request frame */
    u_int8_t        status;                                 /* status */
    u_int8_t        *add_ie_buf;                            /* additional IE to append to frame */
    u_int32_t       add_ie_buf_len;                         /* additional IE buf size to append to frame */
} wlan_p2p_prot_event_rx_go_neg_conf_ind;

/* Event struct when Rx indication for Provision Discovery Request frame */
typedef struct _wlan_p2p_prot_event_rx_prov_disc_req_ind {
    u_int8_t        ta[IEEE80211_ADDR_LEN];                 /* transmitter address of rx frame */
    u_int8_t        bssid[IEEE80211_ADDR_LEN];              /* BSSID of rx frame */
    u_int8_t        dialog_token;                           /* Dialog token used for this request frame */
    void            *request_context;                       /* request context to be used when sending a reply frame */
    u_int8_t        *add_ie_buf;                            /* additional IE to append to frame */
    u_int32_t       add_ie_buf_len;                         /* additional IE buf size to append to frame */
} wlan_p2p_prot_event_rx_prov_disc_req_ind;

typedef wlan_p2p_prot_event_rx_prov_disc_req_ind    wlan_p2p_prot_event_rx_invit_req_ind;

/* Event struct when Rx indication for Provision Discovery Response frame */
typedef struct _wlan_p2p_prot_event_rx_prov_disc_resp_ind {
    u_int8_t        ta[IEEE80211_ADDR_LEN];                 /* transmitter address of rx frame */
    u_int8_t        bssid[IEEE80211_ADDR_LEN];              /* BSSID of rx frame */
    u_int8_t        dialog_token;                           /* Dialog token used for this request frame */
    u_int8_t        *add_ie_buf;                            /* additional IE to append to frame */
    u_int32_t       add_ie_buf_len;                         /* additional IE buf size to append to frame */
} wlan_p2p_prot_event_rx_prov_disc_resp_ind;

typedef wlan_p2p_prot_event_rx_prov_disc_resp_ind   wlan_p2p_prot_event_rx_invit_resp_ind;

/* event struct when type is IEEE80211_P2P_PROT_EVENT_RX_ACT_FR_IND */
typedef struct _wlan_p2p_prot_event_rx_act_fr_ind {
    wlan_p2p_prot_frame_type    frame_type; /* Received frame type */

    union {
        wlan_p2p_prot_event_rx_go_neg_req_ind       go_neg_req;     /* frame_type==IEEE80211_P2P_PROT_FTYPE_GO_NEG_REQ */
        wlan_p2p_prot_event_rx_go_neg_resp_ind      go_neg_resp;    /* frame_type==IEEE80211_P2P_PROT_FTYPE_GO_NEG_RESP */
        wlan_p2p_prot_event_rx_go_neg_conf_ind      go_neg_conf;    /* frame_type==IEEE80211_P2P_PROT_FTYPE_GO_NEG_CONF */
        wlan_p2p_prot_event_rx_prov_disc_req_ind    prov_disc_req;  /* frame_type==IEEE80211_P2P_PROT_FTYPE_PROV_DISC_REQ */
        wlan_p2p_prot_event_rx_prov_disc_resp_ind   prov_disc_resp; /* frame_type==IEEE80211_P2P_PROT_FTYPE_PROV_DISC_RESP */
        wlan_p2p_prot_event_rx_invit_req_ind        invit_req;      /* frame_type==IEEE80211_P2P_PROT_FTYPE_INVIT_REQ */
        wlan_p2p_prot_event_rx_invit_resp_ind       invit_resp;      /* frame_type==IEEE80211_P2P_PROT_FTYPE_INVIT_RESP */
    };

} wlan_p2p_prot_event_rx_act_fr_ind;

/*
 * Completion event struct when type is IEEE80211_P2P_PROT_EVENT_DISC_SCAN_COMPL.
 * This structure is used to indicate the results of the Device Discovery Scan.
 */
typedef struct _wlan_p2p_prot_event_disc_scan_compl {
    int                         status;         /* Completion status for this discovery request */
} wlan_p2p_prot_event_disc_scan_compl;

/* Parameter structure for Callback Events from the P2P Protocol Device */
typedef struct _wlan_p2p_prot_event {
    wlan_p2p_prot_event_type    event_type;
    union {
        wlan_p2p_prot_event_tx_act_fr_compl tx_act_fr_compl;    /* type==IEEE80211_P2P_PROT_EVENT_TX_ACT_FR_COMPL */
        wlan_p2p_prot_event_rx_act_fr_ind   rx_act_fr_ind;      /* type==IEEE80211_P2P_PROT_EVENT_RX_ACT_FR_IND */
        wlan_p2p_prot_event_disc_scan_compl disc_scan_compl;    /* type==IEEE80211_P2P_PROT_EVENT_DISC_SCAN_COMPL */
    };
} wlan_p2p_prot_event;

/* Callback function prototype to indicate an event */
typedef void (*ieee80211_p2p_prot_event_handler) (void *ctx, wlan_p2p_prot_event *param);  

typedef enum {
    wlan_p2p_prot_disc_type_scan_only = 1,  /* discovery scan using full scan only */
    wlan_p2p_prot_disc_type_find_only,      /* discovery scan using find phase only  */
    wlan_p2p_prot_disc_type_auto,           /* discovery scan using both full scan and find phase */
    wlan_p2p_prot_disc_type_find_dev        /* discovery scan to find a device (listen chan is known) */
} wlan_p2p_prot_disc_type;

typedef enum {
    wlan_p2p_prot_disc_scan_type_active,    /* performs active scan during the full scan */
    wlan_p2p_prot_disc_scan_type_passive,   /* performs passive scan during the full scan */
    wlan_p2p_prot_disc_scan_type_auto       /* performs active/passive (depends on channel chars.) scan */
} wlan_p2p_prot_disc_scan_type;

#define WLAN_P2P_PROT_MAX_NUM_DISC_DEVICE_FILTER    2

typedef struct _wlan_p2p_prot_disc_dev_filter {
    u_int8_t        device_id[IEEE80211_ADDR_LEN];
    u_int8_t        bitmask;
    #define WLAN_P2P_PROT_DISC_DEV_FILTER_BITMASK_DEVICE    0x1
    #define WLAN_P2P_PROT_DISC_DEV_FILTER_BITMASK_GO        0x2
    #define WLAN_P2P_PROT_DISC_DEV_FILTER_BITMASK_ANY       0xF
    ieee80211_ssid  grp_ssid;
    bool            grp_ssid_is_p2p_wildcard;
    bool            discovered;
#if DBG
    #define WLAN_P2P_PROT_DBG_MEM_MARKER    0xBEEF0BAD
    u_int32_t       dbg_mem_marker; /* memory marker to check for memory corruption */
#endif
} wlan_p2p_prot_disc_dev_filter;

/* Parameters to request a device discovery scan */
typedef struct _wlan_p2p_prot_disc_scan_param {
    wlan_p2p_prot_disc_type         disc_type;          /* type of this device discovery scan */
    bool                            disc_scan_forced;   /* must do and complete this discovery scan */
    wlan_p2p_prot_disc_scan_type    scan_type;          /* passive/active/auto scan during full scan */
    u_int32_t                       timeout;            /* max duration of this device discovery (in millisec) */
    wlan_p2p_prot_disc_dev_filter   *dev_filter_list;   /* ptr to array of device filters */
                                                        /* array of device filters */
    int                             dev_filter_list_num;/* no. of filters in device filter list */
    u_int8_t                        *add_ie_buf;        /* additional IE for probe req frame */
    u_int32_t                       add_ie_buf_len;     /* len of additional IE for probe req frame */
    bool                            scan_legy_netwk;    /* Search for legacy networksduring scan phase */
} wlan_p2p_prot_disc_scan_param;

typedef struct {
    u_int8_t        tie_breaker :1,
                    intent      :7;
} wlan_p2p_prot_go_intent;

/* Param struc to send a IEEE80211_P2P_PROT_FTYPE_GO_NEG_REQ frame */
typedef struct _wlan_p2p_prot_tx_go_neg_req_param {
    u_int8_t                dialog_token;
    u_int8_t                peer_addr[IEEE80211_ADDR_LEN];          /* peer mac address to send frame */
    u_int32_t               send_timeout;                           /* max time in millisec to send frame */
    wlan_p2p_prot_go_intent go_intent;                              /* Local GO intent value (1..15) */
    u_int8_t                min_config_go_timeout;                  /* configuration timeout to change GO mode of operation */
    u_int8_t                min_config_client_timeout;              /* configuration timeout to change client mode of operation */
    u_int8_t                own_interface_addr[IEEE80211_ADDR_LEN]; /* Intended interface address to use with the group */
    u_int8_t                grp_capab;                              /* bitmap for group capability */
    u_int8_t                *add_ie_buf;                            /* additional IE to append to frame */
    u_int32_t               add_ie_buf_len;                         /* additional IE buf size to append to frame */
} wlan_p2p_prot_tx_go_neg_req_param;

typedef struct {
    u_int8_t        device_addr[IEEE80211_ADDR_LEN];
    ieee80211_ssid  ssid;
} wlan_p2p_prot_grp_id;

/* Param struc to send a IEEE80211_P2P_PROT_FTYPE_PROV_DISC_REQ frame */
typedef struct _wlan_p2p_prot_tx_prov_disc_req_param {
    u_int8_t                dialog_token;
    u_int8_t                peer_addr[IEEE80211_ADDR_LEN];          /* peer mac address to send frame */
    u_int32_t               send_timeout;                           /* max time in millisec to send frame */
    u_int8_t                grp_capab;                              /* bitmap for group capability */
    wlan_p2p_prot_grp_id    grp_id;                                 /* group ID */
    bool                    use_grp_id;                             /* Use the group ID */
    u_int8_t                *add_ie_buf;                            /* additional IE to append to frame */
    u_int32_t               add_ie_buf_len;                         /* additional IE buf size to append to frame */
} wlan_p2p_prot_tx_prov_disc_req_param;

/* Param struc to send a IEEE80211_P2P_PROT_FTYPE_PROV_DISC_RESP frame */
typedef struct _wlan_p2p_prot_tx_prov_disc_resp_param {
    u_int8_t                rx_addr[IEEE80211_ADDR_LEN];            /* The TA of request frame. Also the destination mac address to send response frame */
    u_int8_t                dialog_token;                           /* Dialog token used for this request frame */
    void                    *request_context;                       /* request context to be used when sending a reply frame */
    u_int32_t               send_timeout;                           /* max time in millisec to send frame */
    u_int8_t                *add_ie_buf;                            /* additional IE to append to frame */
    u_int32_t               add_ie_buf_len;                         /* additional IE buf size to append to frame */
} wlan_p2p_prot_tx_prov_disc_resp_param;

/* Param struc to send a IEEE80211_P2P_PROT_FTYPE_GO_NEG_RESP frame */
typedef struct _wlan_p2p_prot_tx_go_neg_resp_param {
    u_int8_t                peer_addr[IEEE80211_ADDR_LEN];          /* peer mac address to send frame */
    u_int8_t                dialog_token;                           /* Dialog token used for this request frame */
    void                    *request_context;                       /* request context to be used when sending a reply frame */
    u_int32_t               send_timeout;                           /* max time in millisec to send frame */
    int                     status;
    wlan_p2p_prot_go_intent go_intent;                              /* Local GO intent value (1..15) */
    u_int8_t                min_config_go_timeout;                  /* configuration timeout to change GO mode of operation */
    u_int8_t                min_config_client_timeout;              /* configuration timeout to change client mode of operation */
    u_int8_t                own_interface_addr[IEEE80211_ADDR_LEN]; /* Intended interface address to use with the group */
    u_int32_t               grp_capab;                              /* bitmap for group capability */
    wlan_p2p_prot_grp_id    grp_id;
    bool                    use_grp_id;                             /* Use the group ID */

    bool                    request_fr_parsed_ie;                   /* whether the previous request frame's IE is OK? */

    u_int8_t                *add_ie_buf;                            /* additional IE to append to frame */
    u_int32_t               add_ie_buf_len;                         /* additional IE buf size to append to frame */
} wlan_p2p_prot_tx_go_neg_resp_param;

/* Param struc to send a IEEE80211_P2P_PROT_FTYPE_INVIT_RESP frame */
typedef struct _wlan_p2p_prot_tx_invit_resp_param {
    u_int8_t                rx_addr[IEEE80211_ADDR_LEN];            /* The TA of request frame. Also the destination mac address to send response frame */
    u_int8_t                dialog_token;                           /* Dialog token used for this request frame */
    void                    *request_context;                       /* request context to be used when sending a reply frame */
    u_int32_t               send_timeout;                           /* max time in millisec to send frame */
    int                     status;
    u_int8_t                min_config_go_timeout;                  /* configuration timeout to change GO mode of operation */
    u_int8_t                min_config_client_timeout;              /* configuration timeout to change client mode of operation */
    u_int8_t                grp_bssid[IEEE80211_ADDR_LEN];          /* Group BSSID */
    bool                    use_grp_bssid;                          /* Use the group BSSID */

    u_int8_t                op_chan_op_class;                       /* Operating channel's Op Class */
    u_int8_t                op_chan_chan_num;                       /* Operating channel's Chan number */

    bool                    use_specified_op_chan;                  /* Use the specified Operating Channel */

    bool                    req_have_op_chan;                       /* Received INVITE_REQ frame has Operating Channel */
    u_int8_t                rx_req_op_chan[5];                      /* Received INVITE_REQ frame's Operating Channel */

    bool                    request_fr_parsed_ie;                   /* whether the previous request frame's IE is OK? */

    u_int8_t                *add_ie_buf;                            /* additional IE to append to frame */
    u_int32_t               add_ie_buf_len;                         /* additional IE buf size to append to frame */
} wlan_p2p_prot_tx_invit_resp_param;

/* Param struc to send a IEEE80211_P2P_PROT_FTYPE_GO_NEG_CONF frame */
typedef struct _wlan_p2p_prot_tx_go_neg_conf_param {
    u_int8_t                peer_addr[IEEE80211_ADDR_LEN];          /* peer mac address to send frame */
    u_int8_t                dialog_token;                           /* Dialog token used for this request frame */
    void                    *request_context;                       /* request context to be used when sending a reply frame */
    u_int32_t               send_timeout;                           /* max time in millisec to send frame */
    int                     status;
    u_int32_t               grp_capab;                              /* bitmap for group capability */
    wlan_p2p_prot_grp_id    grp_id;
    bool                    use_grp_id;                             /* Use the group ID */
    bool                    resp_have_op_chan;                      /* Received GO_NEG_RESP frame has Operating Channel */
    u_int8_t                rx_resp_op_chan[5];                     /* Received GO_NEG_RESP frame has Operating Channel */
    bool                    response_fr_parsed_ie;                  /* whether the response frame's IE is OK? */

    u_int8_t                *add_ie_buf;                            /* additional IE to append to frame */
    u_int32_t               add_ie_buf_len;                         /* additional IE buf size to append to frame */
} wlan_p2p_prot_tx_go_neg_conf_param;

/* Param struc to send a IEEE80211_P2P_PROT_FTYPE_INVIT_REQ frame */
typedef struct _wlan_p2p_prot_tx_invit_req_param {
    u_int8_t                dialog_token;
    u_int8_t                peer_addr[IEEE80211_ADDR_LEN];          /* peer mac address to send frame */
    u_int32_t               send_timeout;                           /* max time in millisec to send frame */
    u_int8_t                min_config_go_timeout;                  /* configuration timeout to change GO mode of operation */
    u_int8_t                min_config_client_timeout;              /* configuration timeout to change client mode of operation */

#define WLAN_P2P_PROT_INVIT_TYPE_MASK               1
#define WLAN_P2P_PROT_INVIT_TYPE_JOIN_ACTIVE_GRP    0
#define WLAN_P2P_PROT_INVIT_TYPE_REINVOKE           1
    u_int8_t                invitation_flags;                       /* Invitation flags */

    u_int8_t                grp_bssid[IEEE80211_ADDR_LEN];          /* Group BSSID */
    bool                    use_grp_bssid;                          /* Use the group BSSID */

    u_int8_t                op_chan_op_class;                       /* Operating channel's Op Class */
    u_int8_t                op_chan_chan_num;                       /* Operating channel's Chan number */

    bool                    use_specified_op_chan;                  /* Use the specified Operating Channel */
    wlan_p2p_prot_grp_id    grp_id;                                 /* group ID */

    u_int8_t                *add_ie_buf;                            /* additional IE to append to frame */
    u_int32_t               add_ie_buf_len;                         /* additional IE buf size to append to frame */
} wlan_p2p_prot_tx_invit_req_param;

/* Structure of parameters used for transmitting an action frame */
typedef struct _wlan_p2p_prot_tx_act_fr_param {
    wlan_p2p_prot_frame_type                frtype;
    wlan_p2p_prot_t                         prot_handle;    /* P2P Protocol device handle */
    union {
        wlan_p2p_prot_tx_go_neg_req_param       go_neg_req;     /* type==IEEE80211_P2P_PROT_FTYPE_GO_NEG_REQ */
        wlan_p2p_prot_tx_go_neg_resp_param      go_neg_resp;    /* type==IEEE80211_P2P_PROT_FTYPE_GO_NEG_RESP */
        wlan_p2p_prot_tx_go_neg_conf_param      go_neg_conf;    /* type==IEEE80211_P2P_PROT_FTYPE_GO_NEG_CONF */
        wlan_p2p_prot_tx_prov_disc_req_param    prov_disc_req;  /* type==IEEE80211_P2P_PROT_FTYPE_PROV_DISC_REQ */
        wlan_p2p_prot_tx_prov_disc_resp_param   prov_disc_resp; /* type==IEEE80211_P2P_PROT_FTYPE_PROV_DISC_RESP */
        wlan_p2p_prot_tx_invit_req_param        invit_req;      /* type==IEEE80211_P2P_PROT_FTYPE_INVIT_REQ */
        wlan_p2p_prot_tx_invit_resp_param       invit_resp;     /* type==IEEE80211_P2P_PROT_FTYPE_INVIT_RESP */
    };
} wlan_p2p_prot_tx_act_fr_param;

/* Structure of parameters used by client when joining a GO */
typedef struct _wlan_p2p_prot_client_join_param {
    u_int8_t        go_op_chan_country_code[3];
    u_int8_t        go_op_chan_op_class;
    u_int8_t        go_op_chan_number;

    u_int32_t       go_config_time;             /* timeout for GO to start */

    bool            p2p_ie_in_group_formation;  /* Wait for Group Formation Bit to be this value before connecting */
    bool            wps_ie_wait_for_wps_ready;  /* If true, wait for Selected Registrar WPS attribute
                                                 * set to TRUE before connecting. If false, then ignore this field. */
} wlan_p2p_prot_client_join_param;

/*
 * To create the P2P Protocol Device.
 * Return P2P Protocol Device handle if succeed. Else return NULL if error.
 */
wlan_p2p_prot_t wlan_p2p_prot_create(wlan_dev_t dev_handle, wlan_p2p_params *p2p_params,
                                     void *event_arg, ieee80211_p2p_prot_event_handler ev_handler);

/**
 * delete p2p protocol device object.
 * @param p2p_prot_handle : handle to the p2p protocol object .
 *
 * @return on success returns 0.
 *         on failure returns a negative value.
 */
int
wlan_p2p_prot_delete(wlan_p2p_prot_t prot_handle);

/**
 * stop p2p protocol device object.
 * @param p2p_prot_handle : handle to the p2p protocol object .
 * @param flags: 
 *
 * @return on success returns 0.
 *         on failure returns a negative value.
 */
int
wlan_p2p_prot_stop(wlan_p2p_prot_t prot_handle, u_int32_t flags);

void
wlan_p2p_prot_reset_start(wlan_p2p_prot_t prot_handle, ieee80211_reset_request *reset_req);

void
wlan_p2p_prot_reset_end(wlan_p2p_prot_t prot_handle, ieee80211_reset_request *reset_req);

/**
 * return new dialog token to be used for the next action frame
 * transaction.
 * @param p2p_prot_handle         : handle to the p2p protocol object .
 * @return dialog_token.
 */
u_int8_t
wlan_p2p_prot_generate_dialog_token(wlan_p2p_prot_t prot_handle);

/**
 * return vaphandle associated with underlying p2p protocol device.
 * @param p2p_prot_handle         : handle to the p2p protocol object .
 * @return vaphandle.
 */
wlan_if_t
wlan_p2p_prot_get_vap_handle(wlan_p2p_prot_t  prot_handle);

/*
 * Routine to request a device discovery scan.
 */
int
wlan_p2p_prot_disc_scan_request(wlan_p2p_prot_t prot_handle, 
                                wlan_p2p_prot_disc_scan_param *disc_scan_param);

/*
 * Routine to stop an existing device discovery scan (if any).
 */
int
wlan_p2p_prot_disc_scan_stop(wlan_p2p_prot_t prot_handle);

/*
 * Routine to send an Action Frame of the "request" type.
 * If successful, the new Dialog Token is returned.
 */
int
wlan_p2p_prot_tx_action_request_fr(wlan_p2p_prot_t prot_handle, 
                                   wlan_p2p_prot_tx_act_fr_param *tx_param
                                   );

/*
 * Routine to send an Action Frame of the "response/confirm" type.
 */
int
wlan_p2p_prot_tx_action_reply_fr(wlan_p2p_prot_t prot_handle, 
                                   wlan_p2p_prot_tx_act_fr_param *tx_param);

/**
 * set simple p2p configuration parameter.
 *
 *  @param p2p_handle    : handle to the vap.
 *  @param param         : simple config paramaeter.
 *  @param val           : value of the parameter.
 *  @return 0  on success and -ve on failure.
 */
int wlan_p2p_prot_set_param(wlan_p2p_prot_t prot_handle, wlan_p2p_prot_param param, u_int32_t val);

/**
 * set p2p configuration byte array.
 *
 *  @param p2p_handle    : handle to the vap.
 *  @param param         : simple config paramaeter.
 *  @param ptr           : pointer to byte array.
 *  @param len           : length of byte array.
 *  @return 0  on success and -ve on failure.
 */
int wlan_p2p_prot_set_param_array(wlan_p2p_prot_t prot_handle, wlan_p2p_prot_param param, u_int8_t *byte_arr, u_int32_t len);

/* The Listen State Discoverability options */
typedef enum {
    IEEE80211_P2P_PROT_LISTEN_STATE_DISC_NOT = 0,   /* Not discoverable mode */
    IEEE80211_P2P_PROT_LISTEN_STATE_DISC_AUTO,      /* Auto mode */
    IEEE80211_P2P_PROT_LISTEN_STATE_DISC_HIGH,      /* Highly discoverable mode */
} wlan_p2p_prot_listen_state_disc;

typedef enum {
    IEEE80211_P2P_PROT_CLIENT_EVENT_CONNECT_TO_COMPLETE,
} ieee80211_p2p_prot_client_event;

typedef void (*ieee80211_p2p_prot_client_event_handler) (wlan_p2p_client_t p2p_client_handle, 
                                              ieee80211_p2p_prot_client_event event, void *arg, u_int32_t status);  

int wlan_p2p_prot_client_attach(wlan_p2p_client_t p2p_client_handle, 
                                ieee80211_p2p_prot_client_event_handler evhandler,
                                void *callback_arg);

void wlan_p2p_prot_client_detach(wlan_p2p_client_t p2p_client_handle);

int wlan_p2p_prot_go_attach(wlan_p2p_go_t p2p_go_handle);

void wlan_p2p_prot_go_detach(wlan_p2p_go_t p2p_go_handle);

/*
 * Extract the P2P Device address from this beacon or probe response frame.
 * Return true if the p2p device address is found and copied into p2p_dev_addr[]
 */
bool
wlan_p2p_prot_get_dev_addr_from_ie(struct ieee80211vap  *vap,
                           const u8             *ie_data,
                           size_t               ie_len,
                           int                  subtype, 
                           osdev_t              st_osdev,
                           u_int8_t             *p2p_dev_addr);

int wlan_p2p_prot_client_set_param(wlan_p2p_client_t p2p_client_handle,
                                   wlan_p2p_client_param param, u_int32_t val);

int wlan_p2p_prot_client_set_param_array(wlan_p2p_client_t p2p_client_handle, 
                                     wlan_p2p_client_param param,
                                     u_int8_t *byte_arr, u_int32_t len);

int wlan_p2p_prot_client_connect_to(wlan_p2p_client_t p2p_client_handle);

int wlan_p2p_prot_client_reset(wlan_p2p_client_t p2p_client_handle);

int wlan_p2p_prot_get_chan_info(wlan_dev_t devhandle, wlan_if_t vap,
                                u_int8_t *CountryCode, u_int8_t *OpClass,
                                u_int8_t *OpChan);

/* Set application defined IEs for various frames */
int
wlan_p2p_prot_set_appie(
    wlan_p2p_prot_t prot_handle,
    ieee80211_frame_type ftype,
    u_int8_t *buf,
    u_int16_t buflen);

#endif  //_IEEE80211_P2P_PROT_API_H_

