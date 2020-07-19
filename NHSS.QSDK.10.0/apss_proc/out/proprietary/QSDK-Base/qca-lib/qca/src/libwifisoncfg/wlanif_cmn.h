/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

struct wlanif_config {
    void *ctx;
    uint32_t IsCfg80211;
    struct wlanif_config_ops *ops;
    int pvt_cmd_sock_id;
    int pvt_event_sock_id;
};

/* Netlink socket ports for different applications to bind to
 * These ports are reserved for these specific applications and
 * should take care not to reuse it.
 */
#define LBD_NL80211_CMD_SOCK      899
#define LBD_NL80211_EVENT_SOCK    900
#define WSPLCD_NL80211_CMD_SOCK   901
#define WSPLCD_NL80211_EVENT_SOCK 902
#define HYD_NL80211_CMD_SOCK      903
#define HYD_NL80211_EVENT_SOCK    904
#define IFACEMGR_NL80211_CMD_SOCK      950
#define IFACEMGR_NL80211_EVENT_SOCK    951
#define LIBSTORAGE_NL80211_CMD_SOCK    952
#define LIBSTORAGE_NL80211_EVENT_SOCK  953

struct wlanif_config_ops {
    int (* init) (struct wlanif_config *wext_conf);
    void (* deinit) (struct wlanif_config *wext_conf);
    int (* getName) (void *,const char *, char *);
    int (* isAP) (void *, const char *, uint32_t *);
    int (* getBSSID) (void *, const char *, struct ether_addr *BSSID );
    int (* getESSID) (void *, const char * , void *, uint32_t * );
    int (* getFreq) (void *, const char * , int32_t * freq);
    int (* getChannelWidth) (void *, const char *, int *);
    int (* getChannelExtOffset) (void *, const char *, int *);
    int (* getChannelBandwidth) (void *, const char *, int *);
    int (* getAcsState) (void *, const char *, int *);
    int (* getCacState) (void *, const char *, int *);
    int (* getParentIfindex) (void *, const char *, int *);
    int (* getSmartMonitor) (void *, const char *, int *);
    int (* getGenericInfoAtf) (void *, const char *, int, void *, int);
    int (* getGenericInfoAld) (void *, const char *, void *, int);
    int (* getGenericHmwds) (void *, const char *, void *, int);
    int (* getGenericNac) (void *, const char *, void *, int);
    int (* getCfreq2) (void *, const char * , int32_t *);
    int (* getChannelInfo) (void *, const char *, void *, int);
    int (* getChannelInfo160) (void *, const char *, void *, int);
    int (* getStationInfo) (void *, const char *, void *, int *);
    int (* getDbgreq) (void *, const char *, void *, uint32_t);
    int (* getExtended) (void *, const char *, void *, uint32_t);
    int (* addDelKickMAC) (void *, const char *, int , void *, uint32_t);
    int (* setFilter) (void *, const char *, void *, uint32_t);
    int (* getWirelessMode)(void *, const char *, void *, uint32_t);
    int (* sendMgmt) (void *, const char *, void *, uint32_t);
    int (* setParamMaccmd)(void *, const char *, void *, uint32_t);
    int (* setParam)(void *, const char *,int, void *, uint32_t);
    int (* getStaStats)(void *, const char *, void *, uint32_t);
    int (* getRange) (void *, const char * , int *);
    int (* getStaCount) (void *, const char *, int *);
    int (* setIntfMode) (void *, const char *, const char *, uint8_t len);
    int (* getBandInfo) (void *ctx, const char * ifname, uint8_t * band_info);
    int (* getUplinkRate) (void *ctx, const char * ifname, uint16_t * ul_rate);
    int (* setUplinkRate) (void * ctx, const char *ifname, uint16_t ul_rate);
    int (* setSonBhType) (void * ctx, const char *ifname, uint8_t bh_type);
    int (* setParamVapInd)(void * ctx, const char *ifname, void *param, uint32_t len);
    int (* setFreq)(void *ctx, const char *ifname, int freq);
};

/*enum to handle MAC operations*/
enum wlanif_ioops_t
{
    IO_OPERATION_ADDMAC=0,
    IO_OPERATION_DELMAC,
    IO_OPERATION_KICKMAC,
    IO_OPERATION_LOCAL_DISASSOC,
    IO_OPERATION_ADDMAC_VALIDITY_TIMER,
};

/* enum to handle wext/cfg80211 mode*/
enum wlanif_cfg_mode {
    WLANIF_CFG80211=0,
    WLANIF_WEXT
};

/**
 * @brief Get whether the Radio is tuned for low, high, full band or 2g.
 * the equivalent enum declared in ol_if_athvar.h and one-to-one mapping.
*/
typedef enum wlanifBandInfo_e {
    bandInfo_Unknown, /* unable to retrieve band info due to some error */
    bandInfo_High, /* supports channel starts from 100 to 165 */
    bandInfo_Full, /* supports channel starts from 36 to 165 */
    bandInfo_Low, /* supports channel starts from 36 to 64 */
    bandInfo_Non5G, /* supports 2g channel 1 to 14 */
} wlanifBandInfo_e;

/**
 * SON WiFi backhaul type is defined 1 and
 * Eth backhaul type is defined 2, PLC is defined as 3 respectively.
 */
typedef enum backhaulType_e {
    backhaulType_Unknown = 0,
    backhaulType_Wifi = 1,
    backhaulType_Ether = 2,
    bacckhaulType_Plc = 3,
} backhaulType_e;

extern struct wlanif_config * wlanif_config_init(enum wlanif_cfg_mode mode,
                                                 int pvt_cmd_sock_id,
                                                 int pvt_event_sock_id);
extern void wlanif_config_deinit(struct wlanif_config *);
