/*
 * Copyright (c) 2004 Atheros Communications, Inc.
 * All rights reserved.
 *
 */
/*
 * Copyright (c) 2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#ifndef _ATH_NETLINK_H_
#define _ATH_NETLINK_H_

#define NETLINK_ATH_EVENT     19

enum {
    ATH_EVENT_CLIENT_REGISTER     = 1,
    ATH_EVENT_CLIENT_DEREGISTER   = 2,
    ATH_EVENT_NODE_JOIN           = 3,
    ATH_EVENT_NODE_LEAVE          = 4,
    ATH_EVENT_NODE_RSSI_MONITOR   = 5,
    ATH_EVENT_NODE_CHLOAD         = 6,
    ATH_EVENT_NODE_NONERP_JOINED  = 7,
    ATH_EVENT_NODE_BG_JOINED      = 8,
    ATH_EVENT_NODE_COCHANNEL_AP_CNT = 9,
    ATH_EVENT_CH_HOP_CHANNEL_CHANGE = 10,
};

#define MAC_ADDR_LEN           6 
typedef struct ath_netlink_event {
	u_int32_t type;
	u_int8_t mac[MAC_ADDR_LEN];
	u_int32_t datalen;
} ath_netlink_event_t;

int ath_adhoc_netlink_init(void);
int ath_adhoc_netlink_delete(void);
void ath_adhoc_netlink_send(ath_netlink_event_t *event, char *event_data, u_int32_t event_datalen);

#endif /* _ATH_NETLINK_H_ */
