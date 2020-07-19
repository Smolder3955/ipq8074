/*
 * @File: lbdSocket.h
 *
 * @Abstract: Load balancing daemon communication with hostapd via socket
 *
 * @Notes:
 *
 * @@-COPYRIGHT-START-@@
 *
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * @@-COPYRIGHT-END-@@
 *
 */
#ifndef wlanifsocket__h
#define wlanifsocket__h

#include <bufrd.h>
#include "wlanifBSteerControlCmn.h"

/**
  * @brief structure to handle hostapd events
  *
  */
struct wlanifHostapdEvents_t {
        struct bufrd readBuf;
};
typedef struct wlanifHostapdEvents_t *wlanifHostapdEventsHandle_t;

/**
  * @brief create socket with vap structure
  *
  * @param [in] state lbd state handle
  * @param [in] vap vap structure handle
  * @param [in] radio_ifname radio interface name
  * @param [in] ifname interface name
  * @param [in] dbgModule debug module
  *
  * @return hostapd socket
  *
  */
int wlanif_createSocket(wlanifBSteerControlHandle_t State,struct wlanifBSteerControlVapInfo *vap,const char* radio_ifname, const char* ifname, struct dbgModule *dbgModule);

/**
  * @brief create socket when vap structure is not defined yet
  *
  * @param [in] radio_ifname radio interface name
  * @param [in] ifname interface name
  *
  * @return hostapd socket
  *
  */
int wlanif_create_sock(const char* radio_ifname, const char* ifname);

/**
  * @brief send command to hostapd through socket
  *
  * @param [in] sock hostapd socket
  * @param [in] command command to be sent
  * @param [in] command_len command length
  * @param [out] command_reply reply received for the command sent
  * @param [in] reply_len length of the reply received
  * @param [out] rcv number of character received
  *
  * @return 1 on successful; otherwise -1
  *
  */
int wlanif_sendCommand( int sock, char *command,int command_len,char *command_reply,int reply_len,int *rcv,struct wlanifBSteerControlVapInfo *vap);

/**
  * @brief parse message received from hostapd
  *
  * @param [in] msg message received from hostapd
  * @param [in] vap vap structure handle
  *
  * @return parsed end string
  *
  */
const char *wlanif_parseMsg(const char *msg,struct wlanifBSteerControlVapInfo *vap);

/**
  * @brief unregister all hostapd events
  *
  * @param [in] vap vap structure handle
  */
void wlanifHostapdEventsUnregister(struct wlanifBSteerControlVapInfo *vap);

/**
  * @brief disable all hostapd events
  *
  * @param [in] state lbd state handle
  * @param [in] band operating band
  */
void wlanifBSteerHostapdEventsDisable(wlanifBSteerControlHandle_t state,wlanif_band_e band);

/**
  * @brief get txpower using netlink socket
  *
  * @param [in] ifname interface name
  */
int wlanif_netlink(const char *ifname);
#endif      //wlanifsocket__h
