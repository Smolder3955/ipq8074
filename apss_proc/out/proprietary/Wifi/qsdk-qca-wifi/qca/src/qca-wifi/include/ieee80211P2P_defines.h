/*
* Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 * Copyright (c) 2009, Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */

#ifndef _IEEE80211_P2P_DEFINES_H_
#define _IEEE80211_P2P_DEFINES_H_

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
} wlan_p2p_param;


#define WLAN_P2P_DEVICE_ID_LEN     6
#define WLAN_P2P_DEVICE_NAME_LEN   32 

typedef int (* wlan_p2p_scan_iter_func) (void *, wlan_scan_entry_t);
typedef enum {
    WLAN_P2P_SCANNIG,                    /* entered scan state */
    WLAN_P2P_SEARCHING,                  /* entered search state */
    WLAN_P2P_LISTENING,                  /* entered listen state */
    WLAN_GO_NEGOTIATION_REQUEST_EVENT,   /* received GO negotiation request from a P2P Device */
    WLAN_GO_NEGOTIATIION_START,          /* started GO negotiation with the desired device */
    WLAN_GO_NEGOTIATIION_COMPLETE,       /* GO negotiation complete with GO device */
} wlan_p2p_event_type; 

/*
 * th P2P SM sends the WLAN_GO_NEGOTIATION_REQUEST_EVENT if it receives a GO negotiation request from
 * a device that is not in the list of desired P2P Devices.once the event is received the 
 */

typedef struct _wlan_p2p_event {
	wlan_p2p_event_type type;
} wlan_p2p_event;


typedef struct _wlan_p2p_device_info {
    u_int32_t match_dev_id:1,
              match_name:1,
              match_category:1,
              match_sub_category:1;
    u_int8_t device_id[WLAN_P2P_DEVICE_ID_LEN];
    u_int8_t device_name[WLAN_P2P_DEVICE_NAME_LEN];
    u_int16_t  category;
    u_int16_t  sub_category;
} wlan_p2p_device_info;

typedef void (*wlan_p2p_event_handler)(os_handle_t,wlan_p2p_event *event);

/*
* flags to be passed to wlan_p2p_start_discovery API.
*/
#define WLAN_P2P_PERFORM_DEVICE_DISCOVERY  0x1  /* perform device discovery */
#define WLAN_P2P_PERFORM_SERVICE_DISCOVERY 0x2  /* perform service discovery */

#endif
