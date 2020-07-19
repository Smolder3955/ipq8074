/*
 * @File: wlanifCommandParser.h
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
#ifndef wlanifcommandparser__h
#define wlanifcommandparser__h

#include "wlanif.h"

/**
  * @brief Internal structure for station info
  *
  * Inactivity and station stats updated periodically
  * are maintained in the below structure
  */
struct wlanifInactStaStats {
    int valid;
    int flag;
    u_int8_t macaddr[IEEE80211_ADDR_LEN];
    int tx_bytes;
    int rx_bytes;
    int tx_rate;
    int tx_packets;
    int rx_packets;
    int rssi;
    int activity;
    struct wlanifInactStaStats *next;
};

/**
  * @brief get channel width info based on driver inputs
  *
  * @param [in] secondary_channel  secondary channel
  * @param [in] ieee80211n  ieee80211n info
  * @param [in] ieee80211ac  ieee80211ac info
  * @param [in] vht_oper_chwidth  vht operation channel width info
  *
  * @return wlanif_chwidth_e enum value
  */
wlanif_chwidth_e getChannelWidth(int , int, int, int);

/**
  * @brief get Phy mode from driver inputs
  *
  * @param [in] ieee80211n ieee80211n info
  * @param [in] ieee80211ac ieee80211n info
  * @param [in] freq frequency
  *
  * @return wlanif_phymode_e enum value
  *
  */
wlanif_phymode_e getPhyMode(int, int, int);

/**
  * @brief get station node association Event data
  *
  * @param [in] sock hostapd socket
  * @param [in] macaddress mac address of the station
  * @param [in] freq operating frequency
  * @param [out] event event structure data
  */
void wlanif_getNodeAssocEventData(int sock,char *macaddr, int freq,ath_netlink_bsteering_event_t *event,struct wlanifBSteerControlVapInfo *vap);

/**
  * @brief update disassoc event data
  *
  * @param [in] macaddr station mac address
  * @param [out] event event structure
  */
void wlanif_getNodeDisassocEventData(char *macaddr, ath_netlink_bsteering_event_t *event);

/**
  * @brief get station Phy mode
  *
  * @param [in] freq operating frequency
  * @param [in] ht ht enable/disable
  * @param [in] vht vht enable/disable
  *
  * @return phy mode
  *
  */
int wlanif_getStaPhyMode(int freq,LBD_BOOL ht,LBD_BOOL vht);

/**
  * @brief get mac address removing ':'
  *
  * @param [in] macaddr macaddress with ':'
  * @param [out] client_addr array where updated macaddress is saved
  */
void wlanif_getMacaddr(char * macaddr,u_int8_t *client_addr);

/**
  * @brief get frequency of the radio when vap sturcture is yet to be initialized
  *
  * @param [in] ifname interface name
  * @param [in] radio_ifname radio interface name
  */
int wlanif_getFreq(const char* ifname,const char* radio_ifname);

/**
  * @brief get station chnnel width
  *
  * @param [in] ht ht enable/disable
  * @param [in] vht vht enable/disable
  * @param [in] vhtcaps vhtcaps info
  * @param [in] htcaps htcaps info
  *
  * @return width channel width
  *
  */
int wlanif_getStaChannelWidth(LBD_BOOL ht,LBD_BOOL vht,int vhtcaps, int htcaps);

/**
  * @brief get frequency from VAP
  *
  * @param [in] vap vap structure handle
  *
  * @return freq radio frequency
  *
  */
int wlanif_getVapFreq(struct wlanifBSteerControlVapInfo *vap);

/**
  * @brief get channel utilization from VAP
  *
  * @param [in] vap vap structure handle
  *
  * @return chanUtil channel utilization
  *
  */
int wlanif_getChannelUtilization(struct wlanifBSteerControlVapInfo *vap);

/**
  * @brief get ssidname from VAP
  *
  * @param [in] vap vap structure handle
  * @param [out] ssidname array to store ssid name
  * @param [out] length length of the ssid
  */
void wlanif_getssidName(struct wlanifBSteerControlVapInfo *vap,u_int8_t *ssidname,u_int32_t *length);

/**
  * @brief get station rssi info
  *
  * @param [in] vap vap structure handle
  * @param [in] addr station mac address
  * @param [out] rssi return rssi value
  *
  * @return LBD_OK on successful get; otherwise LBD_NOK
  *
  */
int wlanif_getRssi(struct wlanifBSteerControlVapInfo *vap, const struct ether_addr *addr, int *rssi);

/**
  * @brief update probe request event data
  *
  * @param [in] macaddr station mac address
  * @param [out] event event structure
  * @param [in] rssi station rssi value
  */
void wlanif_getProbeReqInd(char *macaddr,ath_netlink_bsteering_event_t *event,int rssi);

/**
  * @brief update opmode change event  data
  *
  * @param [in] macaddr station mac address
  * @param [out] event event structure info
  * @param [in] value opmode value
  * @param [in] isbandwidth isbandwidth enable/disable
  */
void wlanif_getOpModeChangeInd(struct wlanifBSteerControlVapInfo *vap,char *macaddr,ath_netlink_bsteering_event_t *event,int value , LBD_BOOL isbandwidth);

/**
  * @brief update smps event data
  *
  * @param [in] macaddr station mac address
  * @param [out] event event structure data
  * @param [in] smpsmode smpsmode ebale/disable
  */
void wlanif_getSmpsChangeInd (char *macaddr,ath_netlink_bsteering_event_t *event,int smpsMode);

/**
  * @brief update wnm event data
  *
  * @param [in] macaddr station mac address
  * @param [out] event event structure data
  * @param [in] statusCode current status code
  * @param [in] bssTermDelay bss term delay received
  * @param [in] targetBssid target sta mac address
  */
void wlanif_getWnmEventInd(char *macaddr, ath_netlink_bsteering_event_t *event,int statusCode,int bssTermDelay,char *targetBssid);

/**
  * @brief update tx auth fail event data
  *
  * @param [in] macaddr station mac address
  * @param [out] event structure data
  */
void wlanif_getTxAuthFailInd(char *macaddr, ath_netlink_bsteering_event_t *event);

/**
  * @brief update rssi crossing event data
  *
  * @param [in] macaddr station mac address
  * @param [out] event structure data
  * @param [in] rssi sta rssi info
  * @param [in] isrssilow rssi low enable/disable
  */
void wlanif_getRSSICrossingEvent(char *macaddr, ath_netlink_bsteering_event_t *event, int rssi, LBD_BOOL isrssilow);

/**
  * @brief update rate crossing event data
  *
  * @param [in] macaddr station mac address
  * @param [out] event structure data
  * @param [in] rate sta rate info
  * @param [in] isratelow rate low enable/disable
  */
void wlanif_getRateCrossingEvent(char *macaddr, ath_netlink_bsteering_event_t *event, int rate, LBD_BOOL isratelow);

/**
  * @brief get channel utilization event data
  *
  * @param [out] event event structure info
  * @param [in] chan_util channel utilization
  */
void wlanif_getChanUtilInd(ath_netlink_bsteering_event_t *event, int chan_util);

/**
  * @brief enable channel utilization
  *
  * @param [in] vap vap structure handle
  */
void wlanif_chanutilEventEnable(struct wlanifBSteerControlVapInfo *vap);

/**
  * @brief enable probe events
  *
  * @param [in] vap vap structure handle
  * @param [in] enable events enable/disable
  */
void wlanif_probeEventEnable(struct wlanifBSteerControlVapInfo *vap,int enable);

/**
  * @brief update BSS load update period
  *
  * @param [in] vap vap structure handle
  * @param [in] update_period period to be updated
  */
void wlanif_setBssUpdatePeriod(struct wlanifBSteerControlVapInfo *vap,int update_period);

/**
  * @brief set average utilization period
  *
  * @param [in] vap vap structure handle
  * @param [in] avg_period average period to be updated
  */
void wlanif_setChanUtilAvgPeriod(struct wlanifBSteerControlVapInfo *vap,int avg_period);

/**
  * @brief get station data rate info
  *
  * @param [in] vap vap structure handle
  * @param [in] addr mac address info
  * @param [out] phyCapInfo phy info
  * @param [out] isStaticSMPS static smps enabled/disabled
  * @param [out] isMUMIMOSupported MUMIMO enabled/disabled
  * @param [out] isRRMSupported RRM supported/unsupported
  * @param [out] isBTMSupported BTM supported/unsupported
  * @param [in] loop_iter iteration loop
  */
void wlanif_getDataRateSta(struct wlanifBSteerControlVapInfo *vap,struct ether_addr *addr, wlanif_phyCapInfo_t *phyCapInfo, LBD_BOOL *isStaticSMPS,LBD_BOOL *isMUMIMOSupported,LBD_BOOL *isRRMSupported,LBD_BOOL *isBTMSupported,int *loop_iter);

/**
  * @brief refresh station rssi info
  *
  * @param [in] vap vap structure info
  * @param [in] addr station mac address
  * @param [in] numSamples number of samples
  */
void wlanif_RequestRssi(struct wlanifBSteerControlVapInfo *vap,const struct ether_addr *addr, u_int8_t numSamples);

/**
  * @brief get data rate from vap
  *
  * @param [in] vap vap structure handle
  */
void wlanif_getDataRate (struct wlanifBSteerControlVapInfo *vap);

/**
  * @brief get station status
  *
  * @param [in] sock hostapd socket
  * @param [in] staAddr station mac address
  * @param [out] stats sta stats structure handle
  */
void wlanif_getStaStats(int sock, const struct ether_addr *staAddr, struct ieee80211req_sta_stats *stats,struct wlanifBSteerControlVapInfo *vap);

void wlanif_getStachwidth(int sock,char *macaddr,ath_netlink_bsteering_event_t *event,struct wlanifBSteerControlVapInfo *vap);
void wlanif_getStanss(int sock,char *macaddr,ath_netlink_bsteering_event_t *event,struct wlanifBSteerControlVapInfo *vap);

/**
  * @brief enable rrsi threshold
  *
  * @param [in] vap vap structure handle
  */
void wlanif_RssiXingThreshold(struct wlanifBSteerControlVapInfo *vap);

/**
  * @brief enable rate threshold
  *
  * @param [in] vap vap structure handle
  */
void wlanif_RateXingThreshold(struct wlanifBSteerControlVapInfo *vap);

/**
  * @brief get acs,cac state
  *
  * @param [in] sock hostapd socket
  * @param [out] acs input to store acs value
  * @param [out] cac input to store cac value
  * @param [in] bssid bssid info
  */
int wlanif_getAcsCacState (int, int *, int *,u_int8_t *,struct wlanifBSteerControlVapInfo *vap);

/**
  * @brief get station inactivity status for all sta connected to the interface
  *
  * @param [in] vap vap structure handle
  */
void wlanif_getInactStaStats(struct wlanifBSteerControlVapInfo *vap);

/**
  * @brief send BTM request
  *
  * @param [in] vap vap structure handle
  * @param [in] addr station mac address
  * @param [in] reqinfo btm requirement info to be sent
  *
  * @return LBD_OK on successful get; otherwise LBD_NOK
  *
  */
int wlanif_SendBTMRequest(struct wlanifBSteerControlVapInfo *vap,const struct ether_addr *addr, struct ieee80211_bstm_reqinfo_target *reqinfo);

/**
  * @brief send RRM beacon request
  *
  * @param [in] vap vap structure handle
  * @param [in] addr station mac address
  * @param [in] reqmode reqmode in which the report has to be sent
  * @param [in] bcnrpt beconreport info to be sent
  *
  * @return LBD_OK on successful get; otherwise LBD_NOK
  *
  */
int wlanif_SendRRMBcnrptRequest(struct wlanifBSteerControlVapInfo *vap, const struct ether_addr *addr, u_int8_t reqmode, ieee80211_rrm_beaconreq_info_t);

/**
  * @brief breakdown the beacon report received to lbd readable form
  *
  * @param [in] sock hostapd socket
  * @param [in] macaddr station macaddr
  * @param [in] msg msg received from hostapd
  * @param [out] event event structure date
  */
void wlanif_getBSteerRRMRptEventData(int sock, char *macaddr, const char *msg, ath_netlink_bsteering_event_t *event,struct wlanifBSteerControlVapInfo *vap);

#endif          //wlanifcommandparser__h
