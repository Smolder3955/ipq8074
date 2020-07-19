/*
 * Copyright (c) 2011,2017-2018 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * Copyright (c) 2009, Atheros Communications Inc.
 * All Rights Reserved.
 *
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 */

#ifndef _IEEE80211_P2P_API_H_
#define _IEEE80211_P2P_API_H_

/*
 * @file ieee80211P2P_api.h
 * Public Interface for Atheros P2P module Layer
 */

#include <osdep.h>
#include <wbuf.h>
#include <ieee80211_api.h>

/**
* @brief Opaque handle of p2p object.
*/
struct ieee80211_p2p;
typedef struct ieee80211p2p *wlan_p2p_t;

/**
* @brief Opaque handle of p2p GO object.
*/
struct ieee80211_p2p_go;
typedef struct ieee80211_p2p_go *wlan_p2p_go_t;

/**
* @brief Opaque handle of p2p client object.
*/
struct ieee80211_p2p_client;
typedef struct ieee80211_p2p_client *wlan_p2p_client_t;


/**
* @brief Opaque handle of p2p service discovery query id.
*/
struct ieee80211_p2p_sd_query_id;
typedef struct ieee80211_p2p_sd_query_id *wlan_p2p_sd_query_id;


#define IEEE80211_P2P_DEVICE_TYPE_LEN 8
#define IEEE80211_P2P_SEC_DEVICE_TYPES 5
#define IEEE80211_MAX_P2P_DEV_NAME_LEN 32

typedef enum {
    WLAN_P2PDEV_CHAN_END_REASON_TIMEOUT,
    WLAN_P2PDEV_CHAN_END_REASON_CANCEL,
    WLAN_P2PDEV_CHAN_END_REASON_ABORT,
} wlan_p2p_chan_end_reason;

typedef enum {
    WLAN_P2PDEV_SCAN_END_REASON_TIMEOUT,
    WLAN_P2PDEV_SCAN_END_REASON_CANCEL,
    WLAN_P2PDEV_SCAN_END_REASON_ABORT,
} wlan_p2p_scan_end_reason;

typedef enum {
    WLAN_P2P_LISTEN_CHANNEL,                 /* listen  channel, default 0(auto select) */
    WLAN_P2P_MIN_DISCOVERABLE_INTERVAL,      /* minumum discoverable interval in 100TU units (default is 1) */
    WLAN_P2P_MAX_DISCOVERABLE_INTERVAL,      /* maxmimum discoverable interval in 100TU units (default is 3) */
    /* device info */
    WLAN_P2P_DEVICE_CATEGORY,                /* primary device category of self*/
    WLAN_P2P_DEVICE_SUB_CATEGORY,            /* primary device sub category of self*/
    WLAN_P2P_GO_INTENT,                      /* group owner intent */
    WLAN_P2P_GO_CHANNEL,                     /* intended channel for GO */
    /* device capabilities */
    WLAN_P2P_SUPPORT_SERVICE_DISCOVERY,     /* flag: to indicated if the device support service discovery */
    WLAN_P2P_SUPPORT_CONCURRENT_OPERATION,  /* flag: to indicated if the device supports concurrent operation */
    WLAN_P2P_INFRASTRUCTURE_MANAGED,        /* flag: to indicated the device is infrastructure managed */
    WLAN_P2P_DEVICE_LIMIT,                  /* flag: to indicated if the P2P device limit has reached */
    /* GO capabilities */
    WLAN_P2P_GROUP_PERSISTENT,              /* flag: to indicate if the group is persistent */
    WLAN_P2P_GROUP_LIMIT,                   /* flag: to indicate if the group owner allows more P2P clients  */
    WLAN_P2P_GROUP_INTRA_BSS,              /* flag: to indicate if the group owner supports intra bss distribution  */
    WLAN_P2P_GROUP_CROSS_CONNECTION,        /* flag: to indicate if the group owner is   */
    WLAN_P2P_GROUP_PERSISTENT_RRCONNECT,    /* flag: to indicate if the group owner is   */
    WLAN_P2P_PROTECTION_MODE,               /* protection mode to be used while sending action frames   */
} wlan_p2p_param;


typedef struct _wlan_p2p_buf {
    u_int32_t  buf_len;
    u_int8_t   *buf;
} wlan_p2p_buf;

typedef struct _wlan_p2p_params {
    u_int8_t pri_dev_type[IEEE80211_P2P_DEVICE_TYPE_LEN];
    u_int8_t sec_dev_type[IEEE80211_P2P_SEC_DEVICE_TYPES][IEEE80211_P2P_DEVICE_TYPE_LEN];
    char     dev_name[IEEE80211_MAX_P2P_DEV_NAME_LEN];
    u_int8_t dev_id[IEEE80211_ADDR_LEN];
} wlan_p2p_params;

typedef struct _wlan_p2p_go_noa_req  {
    u_int8_t  num_iterations;   /* Num. of iterations. 1 for one shot, 255 for continuous, 2-254 for periodic */
    u_int16_t offset_next_tbtt; /* offset in msec from next tbtt */
    u_int16_t duration;         /* duration in msec */
} wlan_p2p_go_noa_req;

typedef enum {
    WLAN_P2PDEV_CHAN_START,                 /* radio is on the requested channel */
    WLAN_P2PDEV_CHAN_END,                   /* Channel switch operation is complete radion is off of the requested channel */
    WLAN_P2PDEV_DEV_FOUND,                  /* P2P device is found during discovery process */
    WLAN_P2PDEV_GO_NEGOTIATION_REQ_RECVD,   /* GO negotiation request received */
    WLAN_P2PDEV_GO_NEGOTIATION_COMPLETE,    /* GO negotiation is complete */
    WLAN_P2PDEV_SD_REQUEST_RECEIVED,        /* Service Discovery request received */
    WLAN_P2PDEV_SD_RESPONSE_RECEIVED,       /* Service Discovery response received */
    WLAN_P2PDEV_RX_FRAME,                   /* management Frame received */
    WLAN_P2PDEV_PROV_DISC_REQ,              /* Provision Discovery request received */
    WLAN_P2PDEV_PROV_DISC_RESP,             /* Provision Discovery response received */
    WLAN_P2PDEV_SCAN_END,                   /* External scan  operation is complete (all req. channels) - for now use chan_end event */
} wlan_p2p_event_type;

/* event struct when type is CHAN_START */
struct wlan_p2p_event_chan_start {
    u_int32_t   freq;  /* freq */
    u_int32_t   chan_flags;
    u_int32_t   duration;
};

/* event struct when type is CHAN_END */
struct wlan_p2p_event_chan_end {
    u_int32_t                   freq;  /* freq */
    wlan_p2p_chan_end_reason    reason;
    u_int32_t duration;
};

/* event struc twhen type is DEV_FOUND */
struct wlan_p2p_event_dev_found {
    /**
     * Source address of the message triggering this notification
     */
    const u_int8_t   *addr;

    /**
     * P2P Device Address of the found P2P Device
     */
    const u_int8_t   *dev_addr;

    /**
     * Primary Device Type (length is IEEE80211_P2P_DEVICE_TYPE_LEN);
     */
    const u_int8_t   *pri_dev_type;

    /**
     * Device Name( NULL terminated).
     */
    const char       *dev_name;

    /**
     * config methods.
     */
    u_int16_t  config_methods;

    /**
     * Device Capabilities.
     */
    u_int8_t   dev_capab;

    /**
     * Group Capabilities.
     */
    u_int8_t   group_capab;
};

/* event struct when type is GO_NEGOTIATION_REQ_RECEIVED */
struct wlan_p2p_event_go_neg_received {
    /**
     * Source address of the message triggering this notification
     */
    const u_int8_t *src_addr;
};


/* event struct when type is GO_NEGOTIATION_COMPLETE */
struct wlan_p2p_event_p2p_go_neg_results {
	/**
	 * status - Negotiation result (Status Code)
	 *
	 * 0 (P2P_SC_SUCCESS) indicates success. Non-zero values indicate
	 * failed negotiation.
	 */
	int status;

	/**
	 * role_go - Whether local end is Group Owner
	 */
	int role_go;

	/**
	 * freq - Frequency of the group operational channel in MHz
	 */
	int freq;

	/**
	 * ssid - SSID of the group
	 */
	u_int8_t *ssid;

	/**
	 * ssid_len - Length of SSID in octets
	 */
	size_t ssid_len;

	/**
	 * passphrase - WPA2-Personal passphrase for the group (GO only)
	 */
	char *passphrase; /* lenght of 9 chars */

	/**
	 * peer_device_addr - P2P Device Address of the peer
	 */
	u_int8_t *peer_device_addr;

	/**
	 * peer_interface_addr - P2P Interface Address of the peer
	 */
	u_int8_t *peer_interface_addr;

	/**
	 * wps_pbc - Whether PBC is to be used during provisioning
	 */
	int wps_pbc;

	/**
	 * wps_pin - WPS PIN to use during provisioning (if wps_pbc == 0)
	 */
	char *wps_pin; /* 9 chars */
};

/* event struct when type is SD_REQUEST_RECEIVED */
struct wlan_p2p_event_sd_request {
    /**
     * Frequency (in MHz) of the channel
     */
    u_int32_t  freq;
    /**
     * Source address of the message triggering this notification
     */
    const u_int8_t *src_addr;

    /**
     * Dialog token
     */
    u_int8_t  dialog_token;

	/**
     * Service Update Indicator from the source of request
     */
    u_int16_t update_indic;
    /**
	 * P2P Service Request TLV(s)
     */
    const u_int8_t  *tlvs;

    /**
	 * Length of tlvs buffer in octets
     */
    size_t   tlvs_len;
};

/* event struct when type is SD_REQUEST_RECEIVED */
struct wlan_p2p_event_sd_response {
    /**
     * Source address of the message triggering this notification
     */
    const u_int8_t *src_addr;

	/**
     * Service Update Indicator from the source of request
     */
    u_int16_t update_indic;
    /**
	 * P2P Service Request TLV(s)
     */
    const u_int8_t  *tlvs;

    /**
	 * Length of tlvs buffer in octets
     */
    size_t   tlvs_len;
};

/* 11AX TODO: Change chan_flags in wlan_p2p_rx_frame to u_int64_t if required in
 * the future */

struct wlan_p2p_rx_frame {
    /**
     * management frame type
     */
    u_int16_t  frame_type;
    /**
     * management frame len
     */
    u_int32_t  frame_len;
    /**
     * management frame buffer
     */
    u_int8_t   *frame_buf;
    /**
     * ie len
     */
    u_int32_t  ie_len;
    /**
     * ie frame buffer (within frame_buf)
     */
    u_int8_t   *ie_buf;
    /**
     * Source address of the message triggering this notification
     */
    const u_int8_t *src_addr;
    /**
     * management frame rssi
     */
    u_int32_t   frame_rssi;
    /**
     * Frequency (in MHz) of the channel
     * Flags for channel
     */
    u_int32_t  freq;
    /**
     * channel flags .
     */
    u_int32_t  chan_flags;
    /* wbuf of the frame */
    wbuf_t     wbuf;
};

struct wlan_p2p_event_prov_disc_req {
    const u_int8_t *peer;
    u_int16_t config_methods;
};

struct wlan_p2p_event_prov_disc_resp {
    const u_int8_t *peer;
    u_int16_t config_methods;
};

/* event struct when type is SCAN_END */
struct wlan_p2p_event_scan_end {
    wlan_p2p_scan_end_reason    reason;
};

typedef struct _wlan_p2p_event {
        wlan_p2p_event_type      type;
        u_int32_t    req_id;
        union {
            struct wlan_p2p_event_chan_start chan_start;       /* if type is CHAN_START */
            struct wlan_p2p_event_chan_end  chan_end;          /* if type is CHAN_END */
            struct wlan_p2p_event_dev_found dev_found;         /* if type is DEV_FOUND */
            struct wlan_p2p_event_go_neg_received go_neg_req_recvd;  /* if type is GO_NEGOTIATION_REQ_RECEIVED */
            struct wlan_p2p_event_p2p_go_neg_results go_neg_results; /* if type is GO_NEGOTIATION_COMPLETE */
            struct wlan_p2p_event_sd_request         sd_request;     /* if type is SD_REQUEST_RECEIVED */
            struct wlan_p2p_event_sd_response        sd_response;    /* if type is SD_RESPONSE_RECEIVED */
            struct wlan_p2p_rx_frame                 rx_frame;       /* if type is RX_FRAME */
	        struct wlan_p2p_event_prov_disc_req prov_disc_req; /* if type is PROV_DISC_REQ */
	        struct wlan_p2p_event_prov_disc_resp prov_disc_resp; /* if type is PROV_DISC_RESP */
            struct wlan_p2p_event_scan_end  scan_end;          /* if type is SCAN_END */
        } u;
} wlan_p2p_event;


typedef void (*wlan_p2p_event_handler)(void *,wlan_p2p_event *event);

typedef struct _wlan_p2p_noa_descriptor {
    u_int32_t  type_count; /* 255: continuous schedule, 0: reserved */
    u_int32_t  duration ;  /* Absent period duration in micro seconds */
    u_int32_t  interval;   /* Absent period interval in micro seconds */
    u_int32_t  start_time; /* 32 bit tsf time when in starts */
} wlan_p2p_noa_descriptor ;

#define IEEE80211_P2P_MAX_NOA_DESCRIPTORS 4
typedef struct _wlan_p2p_noa_info {
    u_int32_t       cur_tsf32;       /* current tsf */
    u_int8_t        index;           /* identifies instance of NOA su element */
    u_int8_t        oppPS;           /* oppPS state of the AP */
    u_int8_t        ctwindow;        /* ctwindow in TUs */
    u_int8_t        num_descriptors; /* number of NOA descriptors */
    wlan_p2p_noa_descriptor noa_descriptors[IEEE80211_P2P_MAX_NOA_DESCRIPTORS];
} wlan_p2p_noa_info;

/* flags for wlan_p2p_stop() API */
#define WLAN_P2P_STOP_WAIT 0x1
#define WLAN_P2P_STOP_SYNC 0x2

typedef enum {
    WLAN_P2PGO_CTWIN = 1,                   /* CT Window */
    WLAN_P2PGO_OPP_PS,                      /* Opportunistic Power Save */
    WLAN_P2PGO_USE_SCHEDULER_FOR_SCAN,      /* use scheduler for scan, when GO is active */
    WLAN_P2PGO_ALLOW_P2P_CLIENTS_ONLY,       /* only allow P2P-aware clients to associate */
    WLAN_P2PGO_MAX_CLIENTS,                  /* maximum number of associated clients*/
    WLAN_P2PGO_PRIMARY_DEVICE_TYPE,         /* primary device type */
	WLAN_P2PGO_SECONDARY_DEVICE_TYPES,      /* secondary device types */
    WLAN_P2PGO_DEVICE_CONFIG_METHODS,       /* WSC config methods */
    WLAN_P2PGO_DEVICE_CAPABILITY,           /* device capability bitmap */
    WLAN_P2PGO_GROUP_CAPABILITY,            /* group capability bitmap */
    WLAN_P2PGO_WPS_VERSION_ENABLED,         /* WPS Versions enabled */
    WLAN_P2PGO_MAX_GROUP_LIMIT,             /* Max. number of P2P clients to be allowed by the group owner */
    WLAN_P2PGO_DEVICE_NAME,                 /* WPS Device Name */
    WLAN_P2PGO_P2P_DEVICE_ADDR             /* P2P Device Address */
} wlan_p2p_go_param_type;

typedef enum {
    WLAN_P2P_CLIENT_OPPPS,                  /* test param to manually  set oppps state */
    WLAN_P2P_CLIENT_CTWINDOW,               /* test param to manually set ctwindow */
    WLAN_P2P_CLIENT_PRIMARY_DEVICE_TYPE,    /* primary device type */
	WLAN_P2P_CLIENT_SECONDARY_DEVICE_TYPES, /* secondary device types */
    WLAN_P2P_CLIENT_DEVICE_CONFIG_METHODS,  /* WSC config methods */
    WLAN_P2P_CLIENT_DEVICE_CAPABILITY,      /* device capability bitmap */
    WLAN_P2P_CLIENT_WPS_VERSION_ENABLED,    /* WPS Versions enabled */
    WLAN_P2P_CLIENT_DEVICE_NAME,            /* WPS Device Name */
    WLAN_P2P_CLIENT_P2P_DEVICE_ADDR,        /* P2P Device Address */
    WLAN_P2P_CLIENT_MAX_PARAM               /* Please make sure this is the last one */
} wlan_p2p_client_param;

typedef enum {
    WLAN_P2P_PROT_CLIENT_JOIN_PARAM = WLAN_P2P_CLIENT_MAX_PARAM,
                                            /* Join Parameters */
    WLAN_P2P_PROT_CLIENT_GROUP_ID,          /* Group ID */
} wlan_p2p_prot_client_param;

/**
 * Flush all P2P module state.
 * @param p2p_handle        : handle to the p2p object.
 */
void wlan_p2p_flush(wlan_p2p_t p2p_handle);

/**
 * start p2p device discovery SM.
 * @param p2p_handle        : handle to the p2p object .
 * @param timeout           :  timeout in seconds (0 means no timeout).
 * @param flags             : flags to control the SM behavior.
 * @return on success returns 0. -ve otherwise.
 *  perform Device discovery , service discovery procedures based on the flags set.
 */
int wlan_p2p_start_discovery(wlan_p2p_t p2p_handle,u_int32_t timeout, u_int32_t flags);

int wlan_p2p_listen(wlan_p2p_t p2p_handle, u_int32_t timeout);

/**
 * start p2p GO negotiation SM.
 * @param p2p_handle         : handle to the p2p object .
 * @param peer_addr          : mac address of the device to do GO negotiation
 * @param go_intent          : Local GO intent value (1..15)
 * @param own_interface_addr : Intended interface address to use with the group
 * @param force_freq         : The only allowed channel frequency in MHz or 0
 * @param pin                : PIN or NULL to indicate PBC
 * @return on success returns 0. -ve otherwise.
 *  perform GO negotiation procedure.
 */
int wlan_p2p_start_GO_negotiation(wlan_p2p_t p2p_handle,u_int8_t *peer_addr,
      int go_intent, u_int8_t *own_interface_addr, int force_freq, char *pin);

/**
 * wlan_p2p_sd_request - Schedule a service discovery query
 * @param p2p_handle         : handle to the p2p object .
 * @param dst                : Destination peer or %NULL to apply for all peers
 * @param tlvs               : P2P Service Query TLV(s)
 * Returns                   : Reference to the query or %NULL on failure
 *
 * Response to the query is indicated with the WLAN_P2PDEV_SD_RESPONSE_RECEIVED event.
 */
wlan_p2p_sd_query_id wlan_p2p_sd_request(wlan_p2p_t p2p_handle, const u_int8_t *dst,
		      const wlan_p2p_buf *tlvs);

/**
 * wlan_p2p_sd_response - Send response to a service discovery query
 * @param p2p_handle         : handle to the p2p object .
 * @param freq               : Frequency from WLAN_P2PDEV_SD_REQUEST_RECEIVED
 * @param dst                : Destination peer or %NULL to apply for all peers
 * @param resp_tlvs          : P2P Service Query TLV(s)
 * @return on success returns 0. -ve otherwise.
 *
 * This function is called as a response to the request indicated with
 * WLAN_P2PDEV_SD_REQUEST_RECEIVED.
 */
int wlan_p2p_sd_response(wlan_p2p_t p2p_handle, u_int32_t freq, const u_int8_t *dst,
                         u_int8_t dialog_token ,const wlan_p2p_buf *resp_tlvs);

/**
 * wlan_p2p_sd_cancel_request - Cancel a pending service discovery query
 * @param p2p_handle         : handle to the p2p object .
 * @req                      : Query reference returned from wlan_p2p_sd_request()
 */
void wlan_p2p_sd_cancel_request(wlan_p2p_t p2p_handle, wlan_p2p_sd_query_id req);

/**
 * wlan_p2p_sd_service_update - Indicate a change in local services
 * @param p2p_handle         : handle to the p2p object .
 *
 * This function needs to be called whenever there is a change in availability
 * of the local services. This will increment the Service Update Indicator
 * value which will be used in SD Request and Response frames.
 */
void wlan_p2p_sd_service_update(wlan_p2p_t p2p_handle);

int wlan_p2p_prov_disc_req(wlan_p2p_t p2p_handle, const u_int8_t *peer_addr,
			   u_int16_t config_methods);

#if UMAC_SUPPORT_P2P

/**
* stop p2p device.
* @param p2p_handle: handle to the p2p device object.
* stops all the activity on p2p device.
*/
void wlan_p2p_stop(wlan_p2p_t p2p_handle, u_int32_t flags);

void ieee80211_p2p_attach(wlan_dev_t dev_handle);
void ieee80211_p2p_detach(wlan_dev_t dev_handle);

/**
 * creates an instance of p2p object .
 * @param dev_handle     : handle to the radio .
 *
 * @return  on success return an opaque handle to newly created p2p object.
 *          on failure returns NULL.
 */
wlan_p2p_t wlan_p2p_create(wlan_dev_t dev_handle, wlan_p2p_params *p2p_params);

/**
 * delete p2p object.
 * @param p2p_handle : handle to the p2p object .
 *
 * @return on success returns 0.
 *         on failure returns a negative value.
 */
int wlan_p2p_delete(wlan_p2p_t p2p_handle);

/**
 * Called at the start of a reset
 * @param p2p_handle : handle to the p2p object .
 *
 * @return on success returns 0.
 *         on failure returns a negative value.
 */
void
wlan_p2p_reset_start(wlan_p2p_t p2p_handle);

/**
 * Called at the end of a reset
 * @param p2p_handle : handle to the p2p object .
 *
 * @return on success returns 0.
 *         on failure returns a negative value.
 */
void
wlan_p2p_reset_end(wlan_p2p_t p2p_handle);

/**
 * set simple p2p configuration parameter.
 *
 *  @param p2p_handle     : handle to the vap.
 *  @param param         : simple config paramaeter.
 *  @param val           : value of the parameter.
 *  @return 0  on success and -ve on failure.
 */
int wlan_p2p_set_param(wlan_p2p_t p2p_handle, wlan_p2p_param param, u_int32_t val);

/**
 * get simple p2p configuration parameter.
 *
 *  @param p2p_handle     : handle to the vap.
 *  @param param         : simple config paramaeter.
 *  @return value of the parameter.
 */
u_int32_t wlan_p2p_get_param(wlan_p2p_t p2p_handle, wlan_p2p_param param);


/**
 * register  event handler with p2p object.
 * @param p2p_handle        : handle to the p2p object .
 * @param event_arg         : handle opaque to the implementor.
 * @handler                 : handler function to receive p2p related events.
 *                            if handler is NULL it unregisters the previously registered
 *                            handler. returns an error if handler is registered
 *                            already.
 *
 * @return on success return 0.
 *         on failure returns -ve value.
 */

int wlan_p2p_register_event_handlers(wlan_p2p_t p2p_handle,
                                          void *event_arg,
                                          wlan_p2p_event_handler handler);

/**
 * register p2p event handler with p2p client object.
 * @param p2p_handle        : handle to the p2p client object .
 * @param event_arg         : handle opaque to the implementor.
 * @handler                 : handler function to receive p2p related events.
 *                            if handler is NULL it unregisters the previously registered
 *                            handler. returns an error if handler is registered
 *                            already.
 *
 * @return on success return 0.
 *         on failure returns -ve value.
 */
int
wlan_p2p_client_register_event_handlers(wlan_p2p_client_t p2p_client_handle,
                                 void *event_arg, wlan_p2p_event_handler handler);

/**
 * register p2p event handler with p2p go object.
 * @param p2p_handle        : handle to the p2p GO object .
 * @param event_arg         : handle opaque to the implementor.
 * @handler                 : handler function to receive p2p related events.
 *                            if handler is NULL it unregisters the previously registered
 *                            handler. returns an error if handler is registered
 *                            already.
 *
 * @return on success return 0.
 *         on failure returns -ve value.
 */
int
wlan_p2p_GO_register_event_handlers(wlan_p2p_go_t p2p_go_handle,
                                 void *event_arg, wlan_p2p_event_handler handler);

/**
 * return vaphandle associated with underlying p2p device.
 * @param p2p_handle         : handle to the p2p object .
 * @return vaphandle.
 */
wlan_if_t  wlan_p2p_get_vap_handle(wlan_p2p_t p2p_handle);


/**
 * request to switch to a chnnel and remain on channel for certain duration.
 * @param p2p_handle     : handle to the p2p device object.
 * @param in_listen_state : whether in listen state for this set channel.
 * @param freq          :  channel to remain on in mhz
 * @param  req_id       : id of the request  (passed back to the event handler to match request/response )
 * @param  channel_time :  time to remain on channel in msec.
 * @param  scan_priority : scan priority for this P2P scan.
 *  The driver asynchronously switches to the requested channel at an appropriate time
 *  and sends event WLAN_P2PDEV_CHAN_START. the driver keeps the radio on the channel
 *  for channel_time (using a timer) , then switches away from the channel and sends
 *  WLAN_P2PDEV_CHAN_END event with reason WLAN_P2PDEV_CHAN_END_REASON_TIMEOUT,
 * @return EOK if successful .
 *
 */
int wlan_p2p_set_channel(wlan_p2p_t  p2p_handle, bool in_listen_state, u_int32_t freq,
                         u_int32_t req_id, u_int32_t channel_time,
                         IEEE80211_SCAN_PRIORITY scan_priority);

/**
 * cancel current remain on channel .
 * the driver switches away the channel and sends
 * WLAN_P2PDEV_CHAN_END event with reason WLAN_P2PDEV_CHAN_END_REASON_CANCEL.
 */
int wlan_p2p_cancel_channel(wlan_p2p_t  p2p_handle, u_int32_t flags);


/**
 * start a p2p scan .
 *  @param p2p_handle             : handle to the p2p device object.
 *  @param params                : scan params.
 *
 * @return:
 *   internally uses the SCAN module to start the scan request. if the scan module is busy
 *  the p2p module will queue the request and issue it when the scan module is ready.
 *  on success returns 0.
 *  on failure returns a negative value.
 *  if succesful
 *  at the end of scan WLAN_P2PDEV_SCAN_END event is delivered via the event handler.
 */
int wlan_p2p_scan(wlan_p2p_t             p2p_handle,
                  ieee80211_scan_params *params);

/**
* send action management frame.
 *@param p2p_handle: handle to the p2p device object.
* @param freq     : channel to send on (only to validate/match with current channel)
* @param dwell_time : channel dwell time.
* @param handler  : call back function.
* @param arg      : arg (will be used in the mlme_action_send_complete)
* @param dst_addr : destination mac address
* @param src_addr : source address.(most of the cases vap mac address).
* @param bssid    : BSSID
* @param data     : includes total payload of the action management frame.
* @param data_len : data len.
* @returns 0 if succesful and -ve if failed.
* if returns 0 then mlme_action_send_complete will be called with the status of
* the frame transmission.
*/
int wlan_p2p_send_action_frame(wlan_p2p_t p2p_handle, u_int32_t freq,u_int32_t dwell_time,
                               wlan_action_frame_complete_handler handler,void *arg,
                               const u_int8_t *dst_addr, const u_int8_t *src_addr,const u_int8_t *bssid,
                               const u_int8_t *data, u_int32_t data_len);

/*
 * Set the primary device type
 */
int
wlan_p2p_set_primary_dev_type(wlan_p2p_t p2p_handle, u_int8_t *pri_dev_type);

/*
 * Set the secondary device type(s)
 */
int
wlan_p2p_set_secondary_dev_type(wlan_p2p_t p2p_handle, int num_dev_types, u_int8_t *sec_dev_type);

/*
** API to create/manage P2P GO/client.
*/

/**
 * creates an instance of p2p GO(Group Owner) object .
 * @param p2p_handle   : handle to the p2p object .
 *                       the information from p2p_handle is used for all the device specific
 *                       information like device id, device category ..etc.
 * @param bssid         : MAC address that was previously allocated.
 *
 * @return  on success return an opaque handle to newly created p2p GO object.
 *          on failure returns NULL.
 */
wlan_p2p_go_t wlan_p2p_GO_create(wlan_p2p_t p2p_handle, u_int8_t *bssid);

/**
 * delete p2p GO object.
 * @param p2p_go_handle : handle to the p2p object .
 *
 * @return on success returns 0.
 *         on failure returns a negative value.
 */
int wlan_p2p_GO_delete(wlan_p2p_go_t p2p_go_handle);

/*
 * Routine to set a new NOA schedule into the GO vap.
 * @param p2p_go_handle : handle to the p2p object .
 * @param num_noa_schedules : number of NOA schedule to install.
 * @param request           : array of new NOA schedules.
 *
 * @return on success returns 0.
 *         on failure returns a negative value.
 */
int wlan_p2p_GO_set_noa_schedule(
    wlan_p2p_go_t                   p2p_go_handle,
    u_int8_t                            num_noa_schedules,
    wlan_p2p_go_noa_req                 *request);

/*
 * Routine to set the Beacon Suppression when no station associated.
 * @param p2p_go_handle : handle to the p2p object .
 * @param enable_suppression : enable/disable the suppression of beacon
 *                             when no station is associated.
 *
 * @return on success returns 0.
 *         on failure returns a negative value.
 */
int wlan_p2p_GO_set_suppress_beacon(
    wlan_p2p_go_t   p2p_go_handle,
    bool            enable_suppression);

/**
 * creates an instance of p2p client object .
 * @param p2p_handle   : handle to the p2p object .
 *                       the information from p2p_handle is used for all the device specific
 *                       information like device id, device category ..etc.
 *
 * @return  on success return an opaque handle to newly created p2p client object.
 *          on failure returns NULL.
 */
wlan_p2p_client_t wlan_p2p_client_create(wlan_p2p_t p2p_handle, u_int8_t *bssid);


/**
 * delete p2p client object.
 * @param p2p_client_handle : handle to the p2p object .
 *
 * @return on success returns 0.
 *         on failure returns a negative value.
 */
int wlan_p2p_client_delete(wlan_p2p_client_t p2p_go_handle);

/**
 * return Station vaphandle associated with underlying p2p client object.
 * @param p2p_handle         : handle to the p2p object .
 * @return vaphandle.
 */
wlan_if_t  wlan_p2p_GO_get_vap_handle(wlan_p2p_go_t p2p_GO_handle);

/**
 * return Station vaphandle associated with underlying p2p client object.
 * @param p2p_client_handle         : handle to the p2p object .
 * @return vaphandle.
 */
wlan_if_t  wlan_p2p_client_get_vap_handle(wlan_p2p_client_t p2p_client_handle);

/*
 * return noa schedule descriptor.
 * @param p2p_go_handle : handle to the p2p object.
 * @param noa_ifo           : NOA info.
 */
int wlan_p2p_GO_get_noa_info(wlan_p2p_go_t p2p_go_handle, wlan_p2p_noa_info  *noa_info);

/**
 *set application specific ie buffer for a given frame..
 *
 * @param vaphandle     : handle to the vap.
 * @param ftype         : frame type.
 * @param buf           : appie buffer.
 * @param buflen        : length of the buffer.
 * @return  0  on success and -ve on failure.
 * once the buffer is set for a perticular frame the MLME keep setting up the ie in to the frame
 * every time the frame is sent out. passing 0 value to buflen would remove the appie biffer.
 */
int wlan_p2p_GO_set_appie(wlan_p2p_go_t p2p_go_handle, ieee80211_frame_type ftype, u_int8_t *buf, u_int16_t buflen);

/**
 *parse p2p ie for a given ie buffer..
 * 
 * @param vaphandle     : handle to the vap.
 * @param ftype         : frame type.   
 * @param buf           : appie buffer.
 * @param buflen        : length of the buffer.
 * @return  0  on success and -ve on failure.
 */
int wlan_p2p_GO_parse_appie(wlan_p2p_go_t p2p_go_handle, ieee80211_frame_type ftype, u_int8_t *buf, u_int16_t buflen);

/**
 *parse p2p ie for a given ie buffer..
 * 
 * @param vaphandle     : handle to the vap.
 * @param ftype         : frame type.   
 * @param buf           : appie buffer.
 * @param buflen        : length of the buffer.
 * @return  0  on success and -ve on failure.
 */
int wlan_p2p_parse_appie(wlan_p2p_t p2p_handle, ieee80211_frame_type ftype, u_int8_t *buf, u_int16_t buflen);

/*
 * return noa schedule descriptor.
 * @param p2p_client_handle : handle to the p2p object.
 * @param noa_ifo           : NOA info.
 */
int wlan_p2p_client_get_noa_info(wlan_p2p_client_t p2p_client_handle, wlan_p2p_noa_info  *noa_info);

/**
 *parse p2p ie for a given ie buffer..
 *
 * @param vaphandle     : handle to the vap.
 * @param ftype         : frame type.
 * @param buf           : appie buffer.
 * @param buflen        : length of the buffer.
 * @return  0  on success and -ve on failure.
 */
int wlan_p2p_GO_parse_appie(wlan_p2p_go_t p2p_go_handle, ieee80211_frame_type ftype, u_int8_t *buf, u_int16_t buflen);

/**
 *parse p2p ie for a given ie buffer..
 *
 * @param vaphandle     : handle to the vap.
 * @param ftype         : frame type.
 * @param buf           : appie buffer.
 * @param buflen        : length of the buffer.
 * @return  0  on success and -ve on failure.
 */
int wlan_p2p_parse_appie(wlan_p2p_t p2p_handle, ieee80211_frame_type ftype, u_int8_t *buf, u_int16_t buflen);

/*
 * For Testing only
 * Routine to set a new NOA schedule into the p2p client.
 * @param p2p_client_handle : handle to the p2p object .
 * @param num_noa_schedules : number of NOA schedule to install.
 * @param request           : array of new NOA schedules.
 *
 * @return on success returns 0.
 *         on failure returns a negative value.
 */
int wlan_p2p_client_set_noa_schedule(
    wlan_p2p_client_t                   p2p_go_handle,
    u_int8_t                            num_noa_schedules,
    wlan_p2p_go_noa_req                 *request);

/**
 * set simple p2p client configuration parameter.
 *
 *  @param p2p_client_handle     : handle to the vap.
 *  @param param         : simple config paramaeter.
 *  @param val           : value of the parameter.
 *  @return 0  on success and -ve on failure.
 */
int wlan_p2p_client_set_param(wlan_p2p_client_t p2p_client_handle, wlan_p2p_client_param param, u_int32_t val);

/**
 * get simple p2p client configuration parameter.
 *
 *  @param p2p_client_handle     : handle to the vap.
 *  @param param         : simple config paramaeter.
 *  @return value of the param
 */
int wlan_p2p_client_get_param(wlan_p2p_client_t p2p_client_handle, wlan_p2p_client_param param);

/**
 * To set a parameter array in the P2P client module.
 *  @param p2p_client_handle  : handle to the p2p client object.
 *  @param param              : config paramaeter.
 *  @param byte_arr           : byte array .
 *  @param len                : length of byte array.
 *  @return 0  on success and -ve on failure.
 */
int wlan_p2p_client_set_param_array(wlan_p2p_client_t p2p_client_handle, wlan_p2p_client_param param,
                                    u_int8_t *byte_arr, u_int32_t len);

/**
 * To set the desired SSIDList in the P2P client module.
 *  @param p2p_client_handle  : handle to the p2p client object.
 *  @param ssidlist           : new SSID.
 *  @return 0  on success and -ve on failure.
 */
int
wlan_p2p_client_set_desired_ssid(wlan_p2p_client_t p2p_client_handle,
                                 ieee80211_ssid *ssidlist);

/**
 * To set a parameter in the P2P GO module.
 * @param p2p_go_handle         : handle to the p2p GO object.
 * @param param_type            : type of parameter to set.
 * @param param                 : new parameter.
 * @return Error Code. Equal 0 if success.
 */
int wlan_p2p_go_set_param(wlan_p2p_go_t p2p_go_handle,
      wlan_p2p_go_param_type param_type, u_int32_t param);

/**
 * To set a parameter array in the P2P GO module.
 *  @param p2p_go_handle : handle to the p2p GO object.
 *  @param param         : config paramaeter.
 *  @param byte_arr      : byte array .
 *  @param len           : length of byte array.
 *  @return 0  on success and -ve on failure.
 */
int wlan_p2p_go_set_param_array(wlan_p2p_go_t p2p_go_handle, wlan_p2p_go_param_type param,
                                u_int8_t *byte_arr, u_int32_t len);

#else   //UMAC_SUPPORT_P2P

#define wlan_p2p_stop(p2p_handle, flags)            {}
#define ieee80211_p2p_attach(dev_handle)            {}
#define ieee80211_p2p_detach(dev_handle)            {}

/**
 * creates an instance of p2p object .
 * @param dev_handle     : handle to the radio .
 *
 * @return  on success return an opaque handle to newly created p2p object.
 *          on failure returns NULL.
 */
#define wlan_p2p_create(dev_handle, p2p_params)      (NULL)

/**
 * delete p2p object.
 * @param p2p_handle : handle to the p2p object .
 *
 * @return on success returns 0.
 *         on failure returns a negative value.
 */
#define wlan_p2p_delete(p2p_handle)                         (EINVAL)

/**
 * set simple p2p configuration parameter.
 *
 *  @param p2p_handle     : handle to the vap.
 *  @param param         : simple config paramaeter.
 *  @param val           : value of the parameter.
 *  @return 0  on success and -ve on failure.
 */
#define wlan_p2p_set_param(p2p_handle, param, val)          (EINVAL)

/**
 * get simple p2p configuration parameter.
 *
 *  @param p2p_handle     : handle to the vap.
 *  @param param         : simple config paramaeter.
 *  @return value of the parameter.
 */
#define wlan_p2p_get_param(p2p_handle, param)               (0)

/**
 * register  event handler with p2p object.
 * @param p2p_handle        : handle to the p2p object .
 * @param event_arg         : handle opaque to the implementor.
 * @handler                 : handler function to receive p2p related events.
 *                            if handler is NULL it unregisters the previously registered
 *                            handler. returns an error if handler is registered
 *                            already.
 *
 * @return on success return 0.
 *         on failure returns -ve value.
 */

#define wlan_p2p_register_event_handlers(p2p_handle, event_arg, handler)                (EINVAL)

/*
 * Set the primary device type
 */
#define wlan_p2p_set_primary_dev_type(p2p_handle, pri_dev_type)                         (-EINVAL)

/*
 * Set the secondary device type(s)
 */
#define wlan_p2p_set_secondary_dev_type(p2p_handle, num_dev_types, sec_dev_type)        (-EINVAL)

/**
 * register p2p event handler with p2p client object.
 * @param p2p_handle        : handle to the p2p client object .
 * @param event_arg         : handle opaque to the implementor.
 * @handler                 : handler function to receive p2p related events.
 *                            if handler is NULL it unregisters the previously registered
 *                            handler. returns an error if handler is registered
 *                            already.
 *
 * @return on success return 0.
 *         on failure returns -ve value.
 */
#define wlan_p2p_client_register_event_handlers(p2p_client_handle, event_arg, handler)  (EINVAL)

/**
 * register p2p event handler with p2p go object.
 * @param p2p_handle        : handle to the p2p GO object .
 * @param event_arg         : handle opaque to the implementor.
 * @handler                 : handler function to receive p2p related events.
 *                            if handler is NULL it unregisters the previously registered
 *                            handler. returns an error if handler is registered
 *                            already.
 *
 * @return on success return 0.
 *         on failure returns -ve value.
 */
#define wlan_p2p_GO_register_event_handlers(p2p_go_handle, event_arg, handler)          (EINVAL)

/**
 * return vaphandle associated with underlying p2p device.
 * @param p2p_handle         : handle to the p2p object .
 * @return vaphandle.
 */
#define wlan_p2p_get_vap_handle(p2p_handle)                             (0)

/**
 * request to switch to a chnnel and remain on channel for certain duration.
 * @param p2p_handle     : handle to the p2p device object.
 * @param in_listen_state : whether in listen state for this set channel.
 * @param freq          :  channel to remain on in mhz
 * @param  req_id       : id of the request  (passed back to the event handler to match request/response )
 * @param  channel_time :  time to remain on channel in msec.
 * @param  scan_priority : scan priority for this P2P scan.
 *  The driver asynchronously switches to the requested channel at an appropriate time
 *  and sends event WLAN_P2PDEV_CHAN_START. the driver keeps the radio on the channel
 *  for channel_time (using a timer) , then switches away from the channel and sends
 *  WLAN_P2PDEV_CHAN_END event with reason WLAN_P2PDEV_CHAN_END_REASON_TIMEOUT,
 * @return EOK if successful .
 *
 */
#define wlan_p2p_set_channel(p2p_handle, in_listen_state, freq, req_id, channel_time, scan_priority)    (EINVAL)

/**
 * cancel current remain on channel .
 * the driver switches away the channel and sends
 * WLAN_P2PDEV_CHAN_END event with reason WLAN_P2PDEV_CHAN_END_REASON_CANCEL.
 */
#define wlan_p2p_cancel_channel(p2p_handle, flags)                      (EINVAL)

/**
 * start a p2p scan .
 *  @param p2p_handle             : handle to the p2p device object.
 *  @param params                : scan params.
 *
 * @return:
 *   internally uses the SCAN module to start the scan request. if the scan module is busy
 *  the p2p module will queue the request and issue it when the scan module is ready.
 *  on success returns 0.
 *  on failure returns a negative value.
 *  if succesful
 *  at the end of scan WLAN_P2PDEV_SCAN_END event is delivered via the event handler.
 */
#define wlan_p2p_scan(p2p_handle, params)                               (EINVAL)

/**
* send action management frame.
 *@param p2p_handle: handle to the p2p device object.
* @param freq     : channel to send on (only to validate/match with current channel)
* @param arg      : arg (will be used in the mlme_action_send_complete)
* @param dst_addr : destination mac address
* @param src_addr : source address.(most of the cases vap mac address).
* @param bssid    : BSSID
* @param data     : includes total payload of the action management frame.
* @param data_len : data len.
* @returns 0 if succesful and -ve if failed.
* if returns 0 then mlme_action_send_complete will be called with the status of
* the frame transmission.
*/
#define wlan_p2p_send_action_frame(p2p_handle, freq, handler, arg, dst_addr, src_addr, bssid, data, data_len)   (EINVAL)

/*
** API to create/manage P2P GO/client.
*/

/**
 * creates an instance of p2p GO(Group Owner) object .
 * @param p2p_handle   : handle to the p2p object .
 *                       the information from p2p_handle is used for all the device specific
 *                       information like device id, device category ..etc.
 * @param bssid         : MAC address that was previously allocated.
 *
 * @return  on success return an opaque handle to newly created p2p GO object.
 *          on failure returns NULL.
 */
#define wlan_p2p_GO_create(p2p_handle, bssid)           (NULL)

/**
 * delete p2p GO object.
 * @param p2p_go_handle : handle to the p2p object .
 *
 * @return on success returns 0.
 *         on failure returns a negative value.
 */
#define wlan_p2p_GO_delete(p2p_go_handle)                                           (EINVAL)

/*
 * For Testing only
 * Routine to set a new NOA schedule into the p2p client.
 * @param p2p_client_handle : handle to the p2p object .
 * @param num_noa_schedules : number of NOA schedule to install.
 * @param request           : array of new NOA schedules.
 *
 * @return on success returns 0.
 *         on failure returns a negative value.
 */
#define wlan_p2p_client_set_noa_schedule(p2p_go_handle, num_noa_schedules, request) (EINVAL)

/*
 * Routine to set a new NOA schedule into the GO vap.
 * @param p2p_go_handle : handle to the p2p object .
 * @param num_noa_schedules : number of NOA schedule to install.
 * @param request           : array of new NOA schedules.
 *
 * @return on success returns 0.
 *         on failure returns a negative value.
 */
#define wlan_p2p_GO_set_noa_schedule(p2p_go_handle, num_noa_schedules, request)     (EINVAL)

/*
 * Routine to set the Beacon Suppression when no station associated.
 * @param p2p_go_handle : handle to the p2p object .
 * @param enable_suppression : enable/disable the suppression of beacon
 *                             when no station is associated.
 *
 * @return on success returns 0.
 *         on failure returns a negative value.
 */
#define wlan_p2p_GO_set_suppress_beacon(p2p_go_handle, enable_suppression)          (EINVAL)

/**
 * creates an instance of p2p client object .
 * @param p2p_handle   : handle to the p2p object .
 *                       the information from p2p_handle is used for all the device specific
 *                       information like device id, device category ..etc.
 *
 * @return  on success return an opaque handle to newly created p2p client object.
 *          on failure returns NULL.
 */
#define wlan_p2p_client_create(p2p_handle, bssid)                   (NULL)

/**
 * delete p2p client object.
 * @param p2p_client_handle : handle to the p2p object .
 *
 * @return on success returns 0.
 *         on failure returns a negative value.
 */
#define wlan_p2p_client_delete(p2p_go_handle)                       (EINVAL)

/**
 * return Station vaphandle associated with underlying p2p client object.
 * @param p2p_handle         : handle to the p2p object .
 * @return vaphandle.
 */
#define wlan_p2p_GO_get_vap_handle(p2p_GO_handle)                   (NULL)

/**
 * return Station vaphandle associated with underlying p2p client object.
 * @param p2p_client_handle         : handle to the p2p object .
 * @return vaphandle.
 */
#define wlan_p2p_client_get_vap_handle(p2p_client_handle)           (NULL)

/*
 * return noa schedule descriptor.
 * @param p2p_go_handle : handle to the p2p object.
 * @param noa_ifo           : NOA info.
 */
#define wlan_p2p_GO_get_noa_info(p2p_go_handle, noa_info)           (EINVAL)

/*
 * return noa schedule descriptor.
 * @param p2p_client_handle : handle to the p2p object.
 * @param noa_ifo           : NOA info.
 */
#define wlan_p2p_client_get_noa_info(p2p_client_handle, noa_info)   (EINVAL)

/**
 * set simple p2p client configuration parameter.
 *
 *  @param p2p_client_handle     : handle to the vap.
 *  @param param         : simple config paramaeter.
 *  @param val           : value of the parameter.
 *  @return 0  on success and -ve on failure.
 */
#define wlan_p2p_client_set_param(p2p_client_handle, param, val)    (EINVAL)

/**
 * get simple p2p client configuration parameter.
 *
 *  @param p2p_client_handle     : handle to the vap.
 *  @param param         : simple config paramaeter.
 *  @return value of the param
 */
#define wlan_p2p_client_get_param(p2p_client_handle, param)         (EINVAL)

/**
 * To set a parameter array in the P2P client module.
 *  @param p2p_client_handle  : handle to the p2p client object.
 *  @param param              : config paramaeter.
 *  @param byte_arr           : byte array .
 *  @param len                : length of byte array.
 *  @return 0  on success and -ve on failure.
 */
#define wlan_p2p_client_set_param_array(p2p_client_handle, param, byte_arr, len)    (EINVAL)

/**
 * To set the desired SSIDList in the P2P client module.
 *  @param p2p_client_handle  : handle to the p2p client object.
 *  @param ssidlist           : new SSID.
 *  @return 0  on success and -ve on failure.
 */
#define wlan_p2p_client_set_desired_ssid(p2p_client_handle, ssidlist)    (EINVAL)

/**
 * To set a parameter in the P2P GO module.
 * @param p2p_go_handle         : handle to the p2p GO object.
 * @param param_type            : type of parameter to set.
 * @param param                 : new parameter.
 * @return Error Code. Equal 0 if success.
 */
#define wlan_p2p_go_set_param(p2p_go_handle, param_type, param)     (EINVAL)

/**
 * To set a parameter array in the P2P GO module.
 *  @param p2p_go_handle : handle to the p2p GO object.
 *  @param param         : config paramaeter.
 *  @param byte_arr      : byte array .
 *  @param len           : length of byte array.
 *  @return 0  on success and -ve on failure.
 */
#define wlan_p2p_go_set_param_array(p2p_go_handle, param, byte_arr, len)    (EINVAL)

#endif  //UMAC_SUPPORT_P2P

#endif  //_IEEE80211_P2P_API_H_
