/*
 * Copyright (c) 2019 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */

#include <acfg_api_types.h>

typedef enum {
	WL_EVENT_TYPE_DISASSOC_AP = 1,
	WL_EVENT_TYPE_DISASSOC_STA,
	WL_EVENT_TYPE_DEAUTH_AP,
	WL_EVENT_TYPE_DEAUTH_STA,
	WL_EVENT_TYPE_ASSOC_AP,
	WL_EVENT_TYPE_ASSOC_STA,
	WL_EVENT_TYPE_AUTH_AP,
	WL_EVENT_TYPE_AUTH_STA,
	WL_EVENT_TYPE_NODE_LEAVE,
	WL_EVENT_TYPE_PUSH_BUTTON,
    WL_EVENT_TYPE_CHAN,
    WL_EVENT_TYPE_RADAR,
	WL_EVENT_TYPE_STA_SESSION,
	WL_EVENT_TYPE_TX_OVERFLOW,
    WL_EVENT_TYPE_GPIO_INPUT,
    WL_EVENT_TYPE_MGMT_RX_INFO,
    WL_EVENT_TYPE_WDT_EVENT,
    WL_EVENT_TYPE_NF_DBR_DBM_INFO,
    WL_EVENT_TYPE_PACKET_POWER_INFO,
    WL_EVENT_TYPE_CAC_START,
    WL_EVENT_TYPE_UP_AFTER_CAC,
    WL_EVENT_TYPE_QUICK_KICKOUT,
    WL_EVENT_TYPE_ASSOC_FAILURE,
    WL_EVENT_TYPE_CHAN_STATS,
    WL_EVENT_TYPE_EXCEED_MAX_CLIENT,
#if defined(OL_ATH_SMART_LOGGING) && !defined(REMOVE_PKT_LOG)
    WL_EVENT_TYPE_SMART_LOG_FW_PKTLOG_STOP,
#endif /* defined(OL_ATH_SMART_LOGGING) && !defined(REMOVE_PKT_LOG) */
} acfg_event_type_t;

typedef struct acfg_event_data {
	u_int16_t result;
	u_int16_t reason;
	u_int8_t addr[IEEE80211_ADDR_LEN];
	u_int8_t downlink;
    u_int8_t count;
    u_int8_t freqs[IEEE80211_CHAN_MAX];
    u_int8_t gpio_num;
    acfg_mgmt_rx_info_t mgmt_ri;
    acfg_nf_dbr_dbm_t nf_dbr_dbm;
    acfg_packet_power_t   packet_power;
    u_int8_t kick_node_mac[IEEE80211_ADDR_LEN];
    acfg_chan_stats_t chan_stats;
} acfg_event_data_t;

#if UMAC_SUPPORT_ACFG
void acfg_send_event(struct net_device *dev, osdev_t osdev, acfg_event_type_t type,
                    acfg_event_data_t *acfg_event);
uint32_t acfg_event_workqueue_init(osdev_t osdev);
void acfg_event_workqueue_delete(osdev_t osdev);
#else
#define acfg_send_event(dev,handle,type,acfg_event) {}
#endif

