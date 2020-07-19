/*
 * @File:sonlib_ath10k_CommandParser.h
 *
 * @Abstract: ATH10K Vendor specific module communicates between LBD and hostapd
 *
 * @Notes:
 *
 * @@-COPYRIGHT-START-@@
 *
 * Copyright (c) 2017, 2018 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * @@-COPYRIGHT-END-@@
 *
 */

#include <bufrd.h>
#include <sys/types.h>
#include "sonlib_cmn.h"
//#include "ieee80211_band_steering_api.h"

#define IEEE80211_ADDR_LEN 6
#define IEEE80211_EXTCAPIE_BSSTRANSITION    0x08
#define IEEE80211_BSTM_REJECT_REASON_INVALID 0x07
#define IEEE80211_NWID_LEN                  32

#define START "<"
#define AP_STA_CONNECTED "AP-STA-CONNECTED"
#define AP_STA_DISCONNECTED "AP-STA-DISCONNECTED"
#define PROBE_REQ "RX-PROBE-REQUEST"
#define STA_WIDTH_MODIFIED "STA-WIDTH-MODIFIED"
#define STA_SMPS_MODE_MODIFIED "STA-SMPS-MODE-MODIFIED"
#define AP_CSA_FINISHED "AP_CSA_FINISHED"
#define CHAN_UTIL "AP-BSS-LOAD"
#define TX_AUTH_FAIL "Ignoring auth from"
#define STA_RX_NSS_MODIFIED "STA-RX-NSS-MODIFIED"
#define RSSI_CROSSING "nl80211: CQM RSSI"
#define RATE_CROSSING "nl80211: CQM TXRATE"
#define RSSI_CROSSING_LOW "CQM RSSI LOW event for"
#define RSSI_CROSSING_HIGH "CQM RSSI HIGH event for"
#define RATE_CROSSING_LOW "CQM TXRATE LOW event for"
#define RATE_CROSSING_HIGH "CQM TXRATE HIGH event for"

#define BSS_TM_RESP "BSS-TM-RESP"
#define BCN_RESP_RX "BEACON-RESP-RX"

#define os_strlen(s) strlen(s)
#define os_strncmp(s1, s2, n) strncmp((s1), (s2), (n))

#define INACT_CHECK_PERIOD 10
#define TX_POWER_CHANGE_PERIOD 300

#define utilization_sample_period 10
#define utilization_average_num_samples 6

#define LOW_RSSI_XING_THRESHOLD 10
#define LOW_TX_RATE_XING_THRESHOLD_24G 0
#define LOW_TX_RATE_XING_THRESHOLD_5G 6000
#define HIGH_TX_RATE_XING_THRESHOLD_24G 50000
#define HIGH_TX_RATE_XING_THRESHOLD_5G 4294967295lu

#define IEEE80211_RRM_MEASRPT_MODE_SUCCESS  0x00

#define ETH_ALEN        6               /* Octets in one ethernet addr  */

#define lbAreEqualMACAddrs(arg1, arg2) (!memcmp(arg1, arg2, ETH_ALEN))

/*
 * lbCopyMACAddr - Copy MAC address variable
 */
#define lbCopyMACAddr(src, dst) memcpy( dst, src, ETH_ALEN )

#define DEBUG

#ifdef DEBUG
#define TRACE_ENTRY() dbgf(soncmnDbgS.dbgModule, DBGDUMP,"%s: Enter ",__func__)
#define TRACE_EXIT() dbgf(soncmnDbgS.dbgModule, DBGDUMP, "%s: Exit ",__func__)
#define TRACE_EXIT_ERR() dbgf(soncmnDbgS.dbgModule, DBGERR, "%s: Exit with err ",__func__)
#else
#define TRACE_ENTRY()
#define TRACE_EXIT()
#define TRACE_EXIT_ERR()
#endif




static const struct timespec MAX_WAIT_TIME = {0, 1000000000};



/* ATH10k private structure */
struct priv_ath10k {
    char *ifname;
    int32_t sock;
    //Inactivity Client List
    struct inactStaStats *head;
  //Average of channel utility
    int ChanUtilAvg;
  //Instant rssi sample
    int bs_inst_rssi_num_samples;
  // The average of RSSI samples
    int bs_avg_inst_rssi;
  // The number of valid RSSI samples gathered
    int bs_inst_rssi_count;
    int txpower;
  // Timer for channelutilization
    struct evloopTimeout chanUtil;

    struct bSteerEventsPriv_t_ath10k *state;
  //  bSteerEventsHandle_t lbdEvHandle;
};

/* Structure to handle events from driver */
struct  bSteerEventsPriv_t_ath10k {
    struct bufrd readBuf;
//    staStatsObserverCB staStatsCallback;
    void *staStatsCookie;
};

struct timerHandle_ath10k{
    struct evloopTimeout inactStaStatsTimeout;

    //Timer to periodically check changes in txpower in the radio
    struct evloopTimeout txPowerTimeout;
};

/**
  * @brief Internal structure for station info
  *
  * Inactivity and station stats updated periodically
  * are maintained in the below structure
  */
struct inactStaStats {
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
    struct inactStaStats *next;
};

typedef enum chwidth_e {
    chwidth_20,
    chwidth_40,
    chwidth_80,
    chwidth_160,
    chwidth_80p80,

    chwidth_invalid
} wlan_chwidth_e;

/**
 * @brief Enumerations for IEEE802.11 PHY mode
 */
typedef enum phymode_e {
    phymode_basic=1,
    phymode_ht=13,
    phymode_vht=19,
    phymode_he=22,

    phymode_invalid
} wlan_phymode;



#if 0
typedef enum wlan_band_e {
	wlan_band_24g,   ///< 2.4 GHz
	wlan_band_5g,    ///< 5 GHz
	wlan_band_invalid,  ///< band is not known or is invalid
} wlan_band_e;
#endif

typedef enum
{
    ATH10K_FALSE = 0,
    ATH10K_TRUE = !ATH10K_FALSE

}ATH10K_BOOL;

typedef enum
{
        ATH10K_OK = 0,
        ATH10K_NOK = -1

}ATH10K_STATUS;


void lbGetTimestamp(struct timespec *ts);

ATH10K_BOOL lbIsTimeBefore(const struct timespec *time1,
                        const struct timespec *time2);

static inline ATH10K_BOOL lbIsTimeAfter(const struct timespec *time1,
                                     const struct timespec *time2) {
        return lbIsTimeBefore(time2, time1);
}

void lbTimeDiff(const struct timespec *time1,
                const struct timespec *time2,
                struct timespec *diff);


int mcsbitmasktomcs(int *mcsarray);
int txrxvhtmaxmcs(int rx_vht_mcs_map,int tx_vht_mcs_map);
int maxnssconversion(int *mcs_set, int mcs_map,int ht_supported,int vht_supported);

int str_starts_ath10k(const char *str, const char *start);


int sonInit_ath10k(struct soncmnDrvHandle *drvhandle_qti, char* ifname);
void sonDeinit_ath10k(struct soncmnDrvHandle *drvhandle_qti);
int getName_ath10k(const void *ctx, const char * ifname, char *name, size_t len);
int getBSSID_ath10k(const void *ctx, const char * ifname, struct ether_addr *bssid);
int getSSID_ath10k(const void *ctx, const const char * ifname, void *ssidname, u_int32_t *length);
int isAP_ath10k(const void *ctx, const char * ifname, u_int32_t *result);
int getFreq_ath10k(const void *context, const char * ifname, int32_t * freq);
int getIfFlags_ath10k(const void *ctx, const const char *ifname , struct ifreq *ifr );
int getAcsState_ath10k(const void * ctx, const char *ifname, int *acs);
int getCacState_ath10k(const void * ctx, const char *ifname, int * cacstate);
int getCacState_ath10k(const void * ctx, const char *ifname, int * cacstate);
int setBSteerAuthAllow_ath10k (const void *context, const char *ifname,
                               const u_int8_t *destAddr, u_int8_t authAllow);
int setBSteerProbeRespWH_ath10k (const void *context, const char *ifname,
                             const u_int8_t *destAddr, u_int8_t probeRespWHEnable);
int setBSteerProbeRespAllow2G_ath10k (const void *context, const char *ifname,
                              const u_int8_t *destAddr, u_int8_t allowProbeResp2G);
int setBSteerOverload_ath10k (const void *context, const char *ifname, u_int8_t setOverload);
int performLocalDisassoc_ath10k (const void *context, const char *ifname, const u_int8_t *staAddr);
int performKickMacDisassoc_ath10k (const void *context,
                    const char* ifname, const u_int8_t* staAddr);
int setBSteering_ath10k (const void *context, const char *ifname, u_int8_t bSteerEnable);
int setBSteerEvents_ath10k (const void *context, const char *ifname, u_int8_t bSteerEventsEnable);
int setBSteerInProgress_ath10k (const void *context, const char *ifname,
                             const u_int8_t *destAddr, u_int8_t bSteerInProgress);
int setBSteerParams_ath10k (const void *context, const char *ifname, void *data, u_int32_t data_len);
int getBSteerRSSI_ath10k(const void * context,
                               const char * ifname,
                               const u_int8_t *staAddr, u_int8_t numSamples);
int getDataRateInfo_ath10k (const void *context, const char *ifname,
       const u_int8_t *destAddr, struct soncmn_ieee80211_bsteering_datarate_info_t *datarateInfo);
int getStaInfo_ath10k (const void * context, const char * ifname, struct soncmn_req_stainfo_t *getStaInfo);
int getStaStats_ath10k(const void * ctx , const char *ifname,
       const u_int8_t *staAddr, struct soncmn_staStatsSnapshot_t *stats);
int setAclPolicy_ath10k(const void * context, const char *ifname, int aclCmd);
int sendBcnRptReq_ath10k (const void * context, const char * ifname,
        const u_int8_t *staAddr, struct soncmn_ieee80211_rrm_beaconreq_info_t *bcnrpt);
int sendBTMReq_ath10k (const void * context, const char * ifname,
        const u_int8_t *staAddr, struct soncmn_ieee80211_bstm_reqinfo_target *reqinfo);
int setMacFiltering_ath10k (const void *context, const char* ifname,
                           const u_int8_t* staAddr, u_int8_t isAdd);
int getChannelWidth_ath10k (const void *ctx, const char * ifname, int * chwidth);
int getOperatingMode_ath10k (const void *ctx, const char * ifname, int *opMode);
int getChannelExtOffset_ath10k (const void *ctx, const char *ifname, int *choffset);
int getCenterFreq160_ath10k(const void *ctx,
             const char *ifname, u_int8_t channum, u_int8_t *cfreq);
int getCenterFreqSeg1_ath10k (const void *ctx, const char *ifname, int32_t *cFreqSeg1);
int getCenterFreqSeg0_ath10k(const void *ctx, const char *ifname, u_int8_t channum, u_int8_t *cFreqSeg0);
int enableBSteerEvents_ath10k(u_int32_t sysIndex);
int dumpAtfTable_ath10k(const void *ctx, const char *ifname, void *atfInfo, int32_t len);


 /**
  * @brief send command to hostapd through socket
  *
  * @param [in] sock hostapd socket
  * @param [in] command command to be sent
  * @param [in] command_len command length
  * @param [out] command_reply reply received for the command sent
  * @param [in] reply_len length of the reply received
  * @param [out] rcv number of character received
  * @param [in]  main struct priv_ath10k
  *
  * @return 1 on successful; otherwise -1
  *
  */
int sendCommand_ath10k( int sock, char *command,int command_len,char *command_reply,int reply_len,int *rcv,struct priv_ath10k *privAth10k);

/**
  * @brief parse message received from hostapd
  *
  * @param [in] msg message received from hostapd
  * @param [in] vap vap structure handle
  *
  * @return parsed end string
  *
  */
void parseMsg_ath10k(const char *msg,struct priv_ath10k *privAth10k);

/**
  * @brief callback function for msg received in hostapd socket
  *
  * @param [in] cookie struct priv_ath10k handle in which the socket is associated
  */
void bufferRead_ath10k(void *cookie);

/**
  * @brief unregister all hostapd events
  *
  * @param [in] priv_ath10k structure handle
  */
void wlanifHostapdEventsUnregister(struct priv_ath10k *privAth10k);


/**
  * @brief get txpower using netlink socket
  *
  * @param [in] ifname interface name
  */
int netlink_ath10k(const char *ifname);


/**
  * @brief get channel width info based on driver inputs
  *
  * @param [in] secondary_channel  secondary channel
  * @param [in] ieee80211n  ieee80211n info
  * @param [in] ieee80211ac  ieee80211ac info
  * @param [in] vht_oper_chwidth  vht operation channel width info
  *
  * @return wlan_chwidth_e enum value
  */
wlan_chwidth_e getChannelWidth(int , int, int, int);

/**
  * @brief get Phy mode from driver inputs
  *
  * @param [in] ieee80211n ieee80211n info
  * @param [in] ieee80211ac ieee80211n info
  * @param [in] freq frequency
  *
  * @return wlan_phymode enum value
  *
  */
wlan_phymode getPhyMode(int, int, int);

/**
  * @brief get station node association Event data
  *
  * @param [in] sock hostapd socket
  * @param [in] macaddress mac address of the station
  * @param [in] freq operating frequency
  * @param [out] event event structure data
  */
void getNodeAssocEventData_ath10k(int sock,char *macaddr, int freq,wlan_band band, ath_netlink_bsteering_event_t *event,struct priv_ath10k *privAth10k);

/**
  * @brief update disassoc event data
  *
  * @param [in] macaddr station mac address
  * @param [out] event event structure
  */
void getNodeDisassocEventData_ath10k(char *macaddr, struct iw_event  *event);

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
int getStaPhyMode_ath10k(int freq,ATH10K_BOOL ht,ATH10K_BOOL vht);

/**
  * @brief get mac address removing ':'
  *
  * @param [in] macaddr macaddress with ':'
  * @param [out] client_addr array where updated macaddress is saved
  */
void getMacaddr_ath10k(char *macaddr,u_int8_t *client_addr);

/**
  * @brief get frequency of the radio when priv_ath10k sturcture is yet to be initialized
  *
  * @param [in] ifname interface name
  * @param [in] radio_ifname radio interface name
  */
int getFreq(const char* ifname,const char* radio_ifname);

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
int getStaChannelWidth_ath10k(ATH10K_BOOL ht,ATH10K_BOOL vht,int vhtcaps, int htcaps);

/**
  * @brief get frequency from VAP
  *
  * @param [in] priv_ath10k structure handle
  *
  * @return freq radio frequency
  *
  */
int getVapFreq_ath10k(struct priv_ath10k *privAth10k);

wlan_band resolveBandFromChannel(u_int8_t);

u_int8_t resolveChannum(u_int32_t , u_int8_t *);

/**
  * @brief get channel utilization from VAP
  *
  * @param [in] priv_ath10k structure handle
  *
  * @return chanUtil channel utilization
  *
  */
int getChannelUtilization_ath10k(struct priv_ath10k *privAth10k);

/**
  * @brief get ssidname from priv_ath10k
  *
  * @param [in] priv_ath10k structure handle
  * @param [out] ssidname array to store ssid name
  * @param [out] length length of the ssid
  */
void getssidName_ath10k(struct priv_ath10k *privAth10k,u_int8_t *ssidname,u_int32_t *length);

/**
  * @brief get station rssi info
  *
  * @param [in] priv_ath10k structure handle
  * @param [in] addr station mac address
  * @param [out] rssi return rssi value
  *
  * @return ATH10K_OK on successful get; otherwise ATH10K_NOK
  *
  */
int getRssi_ath10k(struct priv_ath10k *privAth10k, const struct ether_addr *addr, int *rssi);

/**
  * @brief update probe request event data
  *
  * @param [in] macaddr station mac address
  * @param [out] event event structure
  * @param [in] rssi station rssi value
  */
void getProbeReqInd_ath10k(char *macaddr,ath_netlink_bsteering_event_t *event,int rssi);

/**
  * @brief update opmode change event  data
  *
  * @param [in] macaddr station mac address
  * @param [out] event event structure info
  * @param [in] value opmode value
  * @param [in] isbandwidth isbandwidth enable/disable
  */
void getOpModeChangeInd_ath10k(struct priv_ath10k *privAth10k,char *macaddr,ath_netlink_bsteering_event_t *event,int value , ATH10K_BOOL isbandwidth);

/**
  * @brief update smps event data
  *
  * @param [in] macaddr station mac address
  * @param [out] event event structure data
  * @param [in] smpsmode smpsmode ebale/disable
  */
void getSmpsChangeInd_ath10k (char *macaddr,ath_netlink_bsteering_event_t *event,int smpsMode);

/**
  * @brief update wnm event data
  *
  * @param [in] macaddr station mac address
  * @param [out] event event structure data
  * @param [in] statusCode current status code
  * @param [in] bssTermDelay bss term delay received
  * @param [in] targetBssid target sta mac address
  */
void getWnmEventInd_ath10k(char *macaddr, ath_netlink_bsteering_event_t *event,int statusCode,int bssTermDelay,char *targetBssid);

/**
  * @brief update tx auth fail event data
  *
  * @param [in] macaddr station mac address
  * @param [out] event structure data
  */
void getTxAuthFailInd_ath10k(char *macaddr, ath_netlink_bsteering_event_t *event);

/**
  * @brief update rssi crossing event data
  *
  * @param [in] macaddr station mac address
  * @param [out] event structure data
  * @param [in] rssi sta rssi info
  * @param [in] isrssilow rssi low enable/disable
  */
void getRSSICrossingEvent_ath10k(char *macaddr, ath_netlink_bsteering_event_t *event, int rssi, ATH10K_BOOL isrssilow);

/**
  * @brief update rate crossing event data
  *
  * @param [in] macaddr station mac address
  * @param [out] event structure data
  * @param [in] rate sta rate info
  * @param [in] isratelow rate low enable/disable
  */
void getRateCrossingEvent_ath10k(char *macaddr, ath_netlink_bsteering_event_t *event, int rate, ATH10K_BOOL isratelow);

/**
  * @brief get channel utilization event data
  *
  * @param [out] event event structure info
  * @param [in] chan_util channel utilization
  */
void getChanUtilInd_ath10k(ath_netlink_bsteering_event_t *event, int chan_util);

/**
  * @brief enable channel utilization
  *
  * @param [in] priv_ath10k structure handle
  */
void chanutilEventEnable_ath10k(struct priv_ath10k *privAth10k);

/**
  * @brief enable probe events
  *
  * @param [in] priv_ath10k structure handle
  * @param [in] enable events enable/disable
  */
void probeEventEnable_ath10k(struct priv_ath10k *privAth10k,int enable);

/**
  * @brief update BSS load update period
  *
  * @param [in] priv_ath10k structure handle
  * @param [in] update_period period to be updated
  */
void setBssUpdatePeriod_ath10k(struct priv_ath10k *privAth10k,int update_period);

/**
  * @brief set average utilization period
  *
  * @param [in] priv_ath10k structure handle
  * @param [in] avg_period average period to be updated
  */
void setChanUtilAvgPeriod_ath10k(struct priv_ath10k *privAth10k,int avg_period);

/**
  * @brief get station data rate info
  *
  * @param [in] priv_ath10k structure handle
  * @param [in] addr mac address info
  * @param [out] phyCapInfo phy info
  * @param [out] isStaticSMPS static smps enabled/disabled
  * @param [out] isMUMIMOSupported MUMIMO enabled/disabled
  * @param [out] isRRMSupported RRM supported/unsupported
  * @param [out] isBTMSupported BTM supported/unsupported
  * @param [in] loop_iter iteration loop
  */

void getDataRateSta_ath10k(struct priv_ath10k *privAth10k,struct ether_addr *addr, struct soncmn_ieee80211_bsteering_datarate_info_t *phyCapInfo, ATH10K_BOOL *isRRMSupported,ATH10K_BOOL *isBTMSupported,int *loop_iter);



void getStachwidth_ath10k(int sock,char *macaddr,ath_netlink_bsteering_event_t *event,struct priv_ath10k *privAth10k);
void getStanss_ath10k(int sock,char *macaddr,ath_netlink_bsteering_event_t *event,struct priv_ath10k *privAth10k);

/**
  * @brief enable rrsi threshold
  *
  * @param [in] priv_ath10k structure handle
  */
void rssiXingThreshold_ath10k(struct priv_ath10k *privAth10k);

/**
  * @brief enable rate threshold
  *
  * @param [in]  priv_ath10k structure handle
  */
void rateXingThreshold_ath10k(struct priv_ath10k *privAth10k);

/**
  * @brief get station inactivity status for all sta connected to the interface
  *
  * @param [in] priv_ath10k structure handle
  */
void getInactStaStats_ath10k(struct priv_ath10k *privAth10k);

/**
  * @brief breakdown the beacon report received to readable form
  *
  * @param [in] sock hostapd socket
  * @param [in] macaddr station macaddr
  * @param [in] msg msg received from hostapd
  * @param [out] event event structure date
  */
void getBSteerRRMRptEventData_ath10k(int sock, char *macaddr, const char *msg, ath_netlink_bsteering_event_t *event,struct priv_ath10k *privAth10k);
