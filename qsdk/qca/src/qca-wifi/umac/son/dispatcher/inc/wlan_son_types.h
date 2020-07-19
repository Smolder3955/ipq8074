/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*/
#ifndef _SON_TYPES_H_
#define _SON_TYPES_H_

#include <qdf_status.h>
#include <osdep.h>
#include <qdf_lock.h>
#include <qdf_atomic.h>
#include <qdf_types.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_global_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_objmgr_peer_obj.h>
#include <ieee80211_band_steering_api.h>
#include <ieee80211_var.h>
#include <ieee80211_objmgr_priv.h>
#include <osif_private.h>
#include <ieee80211_rateset.h>
#include <ieee80211_ioctl.h>  /* for ieee80211req_athdbg */
#include <wlan_mlme_dispatcher.h>

typedef enum {
	SON_DISPATCHER_SET_CMD, /* set command from user space */
	SON_DISPATCHER_GET_CMD, /* get command from user space */
	SON_DISPATCHER_SEND_EVENT, /* send event to user space */
} SON_DISPATCHER_ACTION;

typedef enum {
	/* Set/GET the static band steering parameters */
	SON_DISPATCHER_CMD_BSTEERING_PARAMS = 1,
	/* Set the static band steering parameters */
	SON_DISPATCHER_CMD_BSTEERING_DBG_PARAMS = 2,
	/* Enable/Disable band steering */
	SON_DISPATCHER_CMD_BSTEERING_ENABLE = 3,
	/* SET/GET overload status */
	SON_DISPATCHER_CMD_BSTEERING_OVERLOAD = 4,
	/* Request RSSI measurement */
	SON_DISPATCHER_CMD_BSTEERING_TRIGGER_RSSI = 5,
	/* Control whether probe responses are withheld for a MAC */
	SON_DISPATCHER_CMD_BSTERRING_PROBE_RESP_WH  = 6,
	/* Data rate info for node */
	SON_DISPATCHER_CMD_BSTEERING_DATARATE_INFO = 7,
	/* Enable/Disable Band steering events */
	SON_DISPATCHER_CMD_BSTEERING_ENABLE_EVENTS = 8,
	/* Set Local disassociation*/
	SON_DISPATCHER_CMD_BSTEERING_LOCAL_DISASSOC = 9,
	/* set steering in progress for node */
	SON_DISPATCHER_CMD_BSTEERING_INPROG_FLAG = 10,
	/* set stats interval for da */
	SON_DISPATCHER_CMD_BSTEERING_DA_STAT_INTVL = 11,
	/* AUTH ALLOW during steering prohibit time */
	SON_DISPATCHER_CMD_BSTERING_AUTH_ALLOW = 12,
	/* Control whether probe responses are allowed for a MAC in 2.4g band */
	SON_DISPATCHER_CMD_BSTEERING_PROBE_RESP_ALLOW_24G = 13,
	/* set in network mac address for 2.4g band */
	SON_DISPATCHER_CMD_BSTEERING_SET_INNETWORK_24G = 14,
	/* get in network mac address for 2.4g band */
	SON_DISPATCHER_CMD_BSTEERING_GET_INNETWORK_24G = 15,
	/* get assoc frame for MAP */
	SON_DISPATCHER_CMD_MAP_GET_ASSOC_FRAME = 16,
	/* get AP hardware capabilities */
	SON_DISPATCHER_CMD_MAP_GET_AP_HWCAP = 17,
	/* set MAP RSSI policy */
	SON_DISPATCHER_CMD_MAP_SET_RSSI_POLICY = 18,
	/* add MAC timer policy */
	SON_DISPATCHER_CMD_MAP_SET_TIMER_POLICY = 19,
	/* get OP channels for MAP */
	SON_DISPATCHER_CMD_MAP_GET_OP_CHANNELS = 20,
	/* get ESP info for MAP */
	SON_DISPATCHER_CMD_MAP_GET_ESP_INFO = 21,
	/* Enable ack rssi*/
	SON_DISPATCHER_CMD_BSTEERING_ENABLE_ACK_RSSI = 22,
	/* Get/Set Peer Class Group*/
	SON_DISPATCHER_CMD_BSTEERING_PEER_CLASS_GROUP = 23,
} SON_DISPATCHER_CMD;

typedef struct SNRToPhyRateEntry {
	u_int8_t  snr;
	u_int16_t phyRate;
} SNRToPhyRateEntry_t;

#define MIN_NSS          1
#define MAX_NSS          8
#define MAX_RATES        22
#define MAX_SNR          0xFF
#define INVALID_LINK_CAP 0x0

typedef enum {
	SON_CAP_GET = 1,
	SON_CAP_SET = 2,
	SON_CAP_CLEAR = 3,
}son_capability_action;

#define PUBLIC

#define SON_LOGD(args ...)						\
	QDF_TRACE(QDF_MODULE_ID_SON, QDF_TRACE_LEVEL_DEBUG, ## args)

#define SON_LOGI(args ...)						\
	QDF_TRACE(QDF_MODULE_ID_SON, QDF_TRACE_LEVEL_INFO, ## args)

#define SON_LOGW(args ...)						\
	QDF_TRACE(QDF_MODULE_ID_SON, QDF_TRACE_LEVEL_WARN, ## args)

#define SON_LOGE(args ...)						\
	QDF_TRACE(QDF_MODULE_ID_SON, QDF_TRACE_LEVEL_ERROR, ## args)

#define SON_LOGF(args ...)						\
	QDF_TRACE(QDF_MODULE_ID_SON, QDF_TRACE_LEVEL_FATAL, ## args)

#define SON_LOCK(_x)  qdf_spin_lock_bh(_x)
#define SON_UNLOCK(_x)  qdf_spin_unlock_bh(_x)

#define BSTEER_INVALID_RSSI_HIGH_THRESHOLD ((u8)~0U)

/**
 * @brief Enumerations for bandwidth (MHz) supported by STA
 */
typedef enum wlan_chwidth_e {
	wlan_chwidth_20,
	wlan_chwidth_40,
	wlan_chwidth_80,
	wlan_chwidth_160,
	wlan_chwidth_invalid
} wlan_chwidth_e;

/**
 * @brief Enumerations for IEEE802.11 PHY mode
 */
typedef enum wlan_phymode_e {
	wlan_phymode_basic,
	wlan_phymode_ht,
	wlan_phymode_vht,
	wlan_phymode_he,

	wlan_phymode_invalid
} wlan_phymode_e;

/**
 * @brief Enum for Multi-AP Capabilities
 */
typedef enum {
	SON_MAP_CAPABILITY = 1,
	SON_MAP_CAPABILITY_VAP_TYPE = 2,
	SON_MAP_CAPABILITY_VAP_UP = 3,
} son_map_capability;

/* Bit mask for Multi-AP IE */
#define MAP_FRONTHAUL_BSS    0x20
#define MAP_BACKHAUL_BSS     0x40
#define MAP_BACKHAUL_STA     0x80
#define WFA_OUI_MAP_TYPE_IE  0x1B
#define WFA_OUI_IE           0x9a6f50

/* Number of seconds to wait for instaneous RSSI report */
#define INST_RSSI_TIMEOUT 5

#define SON_BEST_UL_HYST_DEF 10

#endif
