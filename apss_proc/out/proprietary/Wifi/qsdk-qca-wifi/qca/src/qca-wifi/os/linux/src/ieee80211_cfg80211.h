/*
 * Copyright (c) 2016-2018 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#if UMAC_SUPPORT_CFG80211
#ifndef _IEEE80211_CFG80211_H___
#define _IEEE80211_CFG80211_H___
#include <wlan_cfg80211_api.h>
#include <ieee80211_var.h>
#include <ieee80211_channel.h>
#include <osif_private.h>
#include <wlan_cfg80211_scan.h>
#include <cdp_txrx_stats_struct.h>
#include <cfg80211_external.h>
#include <ieee80211_objmgr_priv.h>

#define RATETAB(rate, rate_id, flag) \
{ \
    .bitrate = rate, \
    .hw_value = rate_id, \
    .flags = flag, \
}

#define IS_NULL_ADDR(_a)            \
    ((_a)[0] == 0x00 &&             \
     (_a)[1] == 0x00 &&             \
     (_a)[2] == 0x00 &&             \
     (_a)[3] == 0x00 &&             \
     (_a)[4] == 0x00 &&             \
     (_a)[5] == 0x00)

#define MAX_BUFFER_LEN 1180

#define REG_RULE_2412_2462    REG_RULE(2412-10, 2462+10, 40, 0, 20, 0)
#define REG_RULE_2467_2472    REG_RULE(2467-10, 2472+10, 40, 0, 20, 0)
#define REG_RULE_2484         REG_RULE(2484-10, 2484+10, 40, 0, 20,0)
#define REG_RULE_5180_5320    REG_RULE(5180-10, 5320+10, 160, 0, 20,0)
#define REG_RULE_5500_5640    REG_RULE(5500-10, 5640+10, 160, 0, 20,0)
#define REG_RULE_5660_5720    REG_RULE(5660-10, 5720+10, 80, 0, 20,0)
#define REG_RULE_5745_5925    REG_RULE(5745-10, 5925+10, 80, 0, 20, 0)

#define CFG80211_MAX_CHANNELS_2G 14 /* all possible channels for 2.4G */
#define CFG80211_MAX_CHANNELS_5G 47 /* all possible channels for 5G
                                       (including 11P)*/
#define CFG80211_RTS_THRESHOLD_DISABLE  0xffffffff /*cfg80211 value for disabling RTS threshold */ 
#define CFG80211_FRAG_THRESHOLD_DISABLE 0xffffffff /*cfg80211 value for disabling FRAG threshold */ 
#define CFG80211_VAP_CREATE 1
#define EXTSCAN_EVENT_BUF_SIZE (4096)
#define RROP_REPLY_BUF_SIZE (4096)

#define MAX_SCAN_SSID                    (10)
#define SIR_MAC_MAX_SSID_LENGTH          (32)
#define MAX_CHANNEL (NUM_24GHZ_CHANNELS + NUM_5GHZ_CHANNELS)
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 7, 0))
#define NUM_NL80211_BANDS ((enum nl80211_band)IEEE80211_NUM_BANDS)
#endif
#define RATE_MASK         (0x7f)

enum {
    IEEE80211_RCPARAMS_RTPARAM	= 1,
    IEEE80211_RCPARAMS_RTMASK	= 2,
};

static inline int SPECTRAL_CMD_HANDLER(
        enum wlan_cfg80211_spectral_vendorcmd_handler_idx idx,
        struct wiphy *wiphy,
        struct wlan_objmgr_pdev *pdev,
        const void *data,
        int len)
{
    struct ieee80211com *ic;

    ic = wlan_pdev_get_mlme_ext_obj(pdev);
    if(ic == NULL)
        return -EPERM;

    if (ic->ic_cfg80211_spectral_handler.wlan_cfg80211_spectral_handlers[idx])
        return ic->ic_cfg80211_spectral_handler.wlan_cfg80211_spectral_handlers[idx](wiphy, pdev, data, len);
    else {
	QDF_TRACE_INFO(QDF_MODULE_ID_SPECTRAL, "Spectral is disabled");
        return -EPERM;
    }
}

/**
 * struct hdd_vendor_acs_chan_params - vendor acs channel parameters
 * @channel_count: channel count
 * @channel_list: pointer to channel list
 * @vendor_pcl_list: pointer to pcl list
 * @vendor_weight_list: pointer to pcl weight list
 */
struct hdd_vendor_acs_chan_params {
    uint32_t channel_count;
    uint8_t *channel_list;
    uint8_t *vendor_pcl_list;
    uint8_t *vendor_weight_list;
};

/**
 * struct vendor_chan_info - vendor channel info
 * @pri_ch: primary channel
 * @ht_sec_ch: secondary channel
 * @vht_seg0_center_ch: segment0 for vht
 * @vht_seg1_center_ch: vht segment 1
 * @chan_width: channel width
 */
struct vendor_chan_info {
    uint8_t pri_ch;
    uint8_t ht_sec_ch;
    uint8_t vht_seg0_center_ch;
    uint8_t vht_seg1_center_ch;
    uint8_t chan_width;
};
struct wlan_cfg8011_genric_params {
    unsigned int command; /* attribute 17*/
    unsigned int value;   /* attriute 18 */
    void *data;          /* attribute 19 */
    unsigned int data_len;
    unsigned int length; /* attribue 20 */
    unsigned int flags;  /* attribute 21 */
};

/* TODO: qca_wlan_genric_data, qca_wlan_set_params,
 * qca_wlan_get_params
 * These should be align with qca_wlan_vendor_attr_config
 * in qca-vendor.h
 * QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_COMMAND
 * QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_DATA
 *
 * It requires qca_nl80211_lib changes also in reading 
 * responses
 */
enum qca_wlan_genric_data {
    QCA_WLAN_VENDOR_ATTR_GENERIC_PARAM_INVALID = 0,
    QCA_WLAN_VENDOR_ATTR_PARAM_DATA,
    QCA_WLAN_VENDOR_ATTR_PARAM_LENGTH,
    QCA_WLAN_VENDOR_ATTR_PARAM_FLAGS,

    /* keep last */
    QCA_WLAN_VENDOR_ATTR_GENERIC_PARAM_LAST,
    QCA_WLAN_VENDOR_ATTR_GENERIC_PARAM_MAX =
        QCA_WLAN_VENDOR_ATTR_GENERIC_PARAM_LAST - 1
};

enum qca_wlan_set_params {
    QCA_WLAN_VENDOR_ATTR_SETPARAM_INVALID = 0,
    QCA_WLAN_VENDOR_ATTR_SETPARAM_COMMAND,
    QCA_WLAN_VENDOR_ATTR_SETPARAM_VALUE,

    /* keep last */
    QCA_WLAN_VENDOR_ATTR_SETPARAM_LAST,
    QCA_WLAN_VENDOR_ATTR_SETPARAM_MAX =
        QCA_WLAN_VENDOR_ATTR_SETPARAM_LAST - 1
};

enum qca_wlan_get_params {
    QCA_WLAN_VENDOR_ATTR_GETPARAM_INVALID = 0,
    QCA_WLAN_VENDOR_ATTR_GETPARAM_VALUE,

    /* keep last */
    QCA_WLAN_VENDOR_ATTR_GETPARAM_LAST,
    QCA_WLAN_VENDOR_ATTR_GETPARAM_MAX =
        QCA_WLAN_VENDOR_ATTR_GETPARAM_LAST - 1
};

enum qca_wlan_mac_commands {
    MAC_COMMND_ADD_WDS_ADDR = 1,
    MAC_COMMND_SEND_WOWPKT = 2,
    MAC_COMMND_GET_ACLMAC = 3,
};

struct cfg80211_context {
    struct ieee80211com *ic;
};

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 7, 0))
static inline int
wlan_nla_put_u64(struct sk_buff *skb, int attrtype, u64 value)
{
    return nla_put_u64(skb, attrtype, value);
}
#else
static inline int
wlan_nla_put_u64(struct sk_buff *skb, int attrtype, u64 value)
{
    return nla_put_u64_64bit(skb, attrtype, value, NL80211_ATTR_PAD);
}
#endif

int ieee80211_cfg80211_dump_chandef(struct cfg80211_chan_def *chan_def);
void ieee80211_cfg80211_schedule_channel_notify(void *context);
int wlan_cfg80211_set_freq(struct net_device *dev, struct ieee80211_channel *cfgchan);
int wlan_cfg80211_chan_to_phymode(struct cfg80211_chan_def chandef);
int wlan_cfg80211_set_phymode(struct net_device *dev, struct cfg80211_chan_def chandef);
int wlan_cfg80211_set_channel(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        struct wlan_cfg8011_genric_params *params);
int wlan_cfg80211_set_beacon_interval(wlan_if_t vap, struct ieee80211com *ic, int value);
int wlan_cfg80211_getopmode(enum nl80211_iftype type, u_int32_t *flags);
int wlan_cfg80211_probe_client(struct wiphy *wiphy, struct net_device *dev,const u8 *peer, u64 *cookie);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0))
struct wireless_dev *wlan_cfg80211_add_virtual_intf(struct wiphy *wiphy,
        const char *name,
        unsigned char name_assign_type,
        enum nl80211_iftype type,
        u_int32_t *flags,
        struct vif_params *params);
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 7, 0))
struct wireless_dev *wlan_cfg80211_add_virtual_intf(struct wiphy *wiphy,
        const char *name,
        enum nl80211_iftype type,
        u_int32_t *flags,
        struct vif_params *params);
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0))
struct wireless_dev *wlan_cfg80211_add_virtual_intf(struct wiphy *wiphy, char *name,
        enum nl80211_iftype type,
        u_int32_t *flags,
        struct vif_params *params);
#else
struct net_device *wlan_cfg80211_add_virtual_intf(struct wiphy *wiphy, char *name,
        enum nl80211_iftype type,
        u_int32_t *flags,
        struct vif_params *params);
#endif        
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0))
int wlan_cfg80211_del_virtual_intf(struct wiphy *wiphy, struct wireless_dev *wdev);
#else
int wlan_cfg80211_del_virtual_intf(struct wiphy *wiphy, struct net_device *dev);
#endif
int wlan_cfg80211_set_beacon(struct wiphy *wiphy,struct net_device *dev, struct cfg80211_beacon_data *params);
int wlan_cfg80211_start_ap(struct wiphy *wiphy, struct net_device *dev, struct cfg80211_ap_settings *params);
int wlan_cfg80211_change_beacon(struct wiphy *wiphy, struct net_device *dev, struct cfg80211_beacon_data *params);
int wlan_cfg80211_stop_ap(struct wiphy *wiphy, struct net_device *dev);
int wlan_cfg80211_change_bss(struct wiphy *wiphy, struct net_device *dev, struct bss_parameters *params);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 6, 0))
int wlan_scan(struct wiphy *wiphy, struct net_device *dev,struct cfg80211_scan_request *request);
#else
int wlan_scan(struct wiphy *wiphy,struct cfg80211_scan_request *request);
#endif
int wlan_connect(struct wiphy *wiphy, struct net_device *ndev, struct cfg80211_connect_params *req);
int wlan_disconnect(struct wiphy *wiphy, struct net_device *dev, u16 reason);
int wlan_join_ibss(struct wiphy *wiphy, struct net_device *dev, struct cfg80211_ibss_params *params);
int wlan_leave_ibss(struct wiphy *wiphy, struct net_device *dev);
int wlan_cfg80211_set_wiphy_paramss(struct wiphy *wiphy, u32 changed);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 8, 0)
int wlan_cfg80211_set_txpower(struct wiphy *wiphy, struct wireless_dev *wdev, enum nl80211_tx_power_setting type, int dbm);
#else
int wlan_cfg80211_set_txpower(struct wiphy *wiphy, enum nl80211_tx_power_setting type, int dbm);
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 8, 0) || defined(WITH_BACKPORTS)
int wlan_cfg80211_get_txpower(struct wiphy *wiphy, struct wireless_dev *wdev,int *dbm);
#else
int wlan_cfg80211_get_txpower(struct wiphy *wiphy, int *dbm);
#endif
int wlan_cfg80211_set_default_mgmt_key_key(struct wiphy *wiphy, struct net_device *netdev, u8 key_index);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0))
int wlan_cfg80211_get_station(struct wiphy *wiphy, struct net_device *dev, const uint8_t *mac, struct station_info *sinfo);
#else
int wlan_cfg80211_get_station(struct wiphy *wiphy, struct net_device *dev, uint8_t *mac, struct station_info *sinfo);
#endif
int wlan_cfg80211_set_power_mgmt(struct wiphy *wiphy, struct net_device *dev, bool mode, int timeout);
int wlan_cfg80211_set_pmksa(struct wiphy *wiphy, struct net_device *dev, struct cfg80211_pmksa *pmksa);
int wlan_cfg80211_del_pmksa(struct wiphy *wiphy, struct net_device *dev, struct cfg80211_pmksa *pmksa);
int wlan_cfg80211_flush_pmksa(struct wiphy *wiphy, struct net_device *dev);
int wlan_cfg80211_update_ft_ies(struct wiphy *wiphy, struct net_device *dev, struct cfg80211_update_ft_ies_params *ftie);
int wlan_cfg80211_set_rekey_dataata(struct wiphy *wiphy, struct net_device *dev, struct cfg80211_gtk_rekey_data *data);
int wlan_cfg80211_sched_scan_start(struct wiphy *wiphy, struct net_device *dev, struct cfg80211_sched_scan_request *request);
int wlan_cfg80211_sched_scan_stop(struct wiphy *wiphy,  struct net_device *dev);
void wlan_regulatory_notifier(struct wiphy *wiphy,struct regulatory_request *request);
int wlan_cfg80211_set_mac_acl(struct wiphy *wiphy, struct net_device *dev, const struct cfg80211_acl_data *params);
/* FTM */
int wlan_cfg80211_remain_on_channel(struct wiphy *wiphy, struct wireless_dev *wdev, struct ieee80211_channel *chan, unsigned int duration, u64 *cookie);
int wlan_cfg80211_cancel_remain_on_channel(struct wiphy *wiphy, struct wireless_dev *wdev, u64 cookie);
int wlan_cfg80211_testmode(struct wiphy *wiphy, struct wireless_dev *wdev, void *data, int len); /* Receives UTF commands from FTM daemon */
int wlan_cfg80211_mgmt_tx(struct wiphy *wiphy, struct wireless_dev *wdev, struct cfg80211_mgmt_tx_params *params, u64 *cookie);
int wlan_cfg80211_mgmt_tx_cancel_wait(struct wiphy *wiphy, struct wireless_dev *wdev, u64 cookie);
int wlan_get_num_channels(struct ieee80211com *ic, enum ieee80211_phymode phymode);
int wlan_cfg80211_update_channel_list(struct ieee80211com *ic);
int wlan_cfg80211_populate_channels(struct ieee80211com *ic, enum ieee80211_phymode phymode, enum ieee80211_band phy_band,
        struct ieee80211_channel *channel_list);
void cfg80211_populate_band_2g(struct ieee80211com *ic, struct ieee80211_supported_band *band, struct ieee80211_channel *channel_list, int num_channels);
void cfg80211_populate_band_5g(struct ieee80211com *ic, struct ieee80211_supported_band *band, struct ieee80211_channel *channel_list, int num_channels);
int ieee80211_cfg80211_init(struct net_device *dev, struct wiphy *wiphy, struct ieee80211com *ic);
int ieee80211_cfg80211_radio_attach(struct device *dev, struct net_device *net_dev, struct ieee80211com *ic);
int ieee80211_cfg80211_radio_detach(struct ieee80211com *ic);
void ieee80211_cfg80211_send_connect_result(osif_dev  *osdev, u_int8_t bssid[IEEE80211_ADDR_LEN], uint8_t status);
void ieee80211_cfg80211_disconnected(struct net_device *dev, u_int8_t *locally_generated_disconnect, u_int8_t reasoncode);
int ieee80211_ucfg_handle_dbgreq (struct net_device *dev, struct ieee80211req_athdbg *req, void *buff, unsigned int data_size);
int wlan_cfg80211_start_acs_scan(wlan_if_t vap, cfg80211_hostapd_acs_params *cfg_acs_params);
void wlan_cfg80211_acs_report_channel(wlan_if_t vap,
                                  struct ieee80211_ath_channel *channel);
void wlan_cfg80211_generic_event(struct ieee80211com *ic,
                            int cmdid,
                            void *buffer,
                            uint32_t buffer_len,
                            struct net_device *vap_dev,
                            gfp_t gfp);
void wlan_cfg80211_dfs_cac_start(wlan_if_t vap, u_int32_t timeout);
int ieee80211_sendwowpkt(struct net_device *dev, u_int8_t macaddr[IEEE80211_ADDR_LEN]);

int ieee80211_add_wdsaddr(wlan_if_t vap, union iwreq_data *u);
#if WLAN_DFS_CHAN_HIDDEN_SSID
int ieee80211_set_conf_bssid(wlan_if_t vap, u_int8_t macaddr[IEEE80211_ADDR_LEN]);
#endif
int wlan_cfg80211_phyerr(struct wiphy *wiphy, struct wireless_dev *wdev, const void *data, int data_len);
/* free the clone params list on radio detach */
void wlan_cfg80211_free_clone_params_list(struct ieee80211com *ic);
int wlan_cfg80211_set_params(struct wiphy *wiphy,
        struct wireless_dev *wdev,
                struct wlan_cfg8011_genric_params *params);
int wlan_cfg80211_nawds_params(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        const void *data,
        int data_len);
int wlan_cfg80211_get_params(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        struct wlan_cfg8011_genric_params *params);
int wlan_cfg80211_hmwds_params(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        const void *data,
        int data_len);
int wlan_cfg80211_list_cap(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        const void *data,
        int data_len);
int wlan_cfg80211_list_chan(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        const void *data,
        int data_len);
int wlan_cfg80211_active_chan_list(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        const void *data,
        int data_len);
int wlan_cfg80211_list_chan160(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        const void *data,
        int data_len);
int wlan_cfg80211_wnm_params(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        const void *data,
        int data_len);
int wlan_cfg80211_vendorie_params(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        const void *data,
        int data_len);
int wlan_cfg80211_set_maxrate(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        const void *data,
        int data_len);
int wlan_cfg80211_nac_rssi_params(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        const void *data,
        int data_len);
int wlan_cfg80211_nac_params(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        const void *data,
        int data_len);
#if QCA_AIRTIME_FAIRNESS
int wlan_cfg80211_atf(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        const void *data,
        int data_len);
#endif
int wlan_cfg80211_list_sta(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        const void *data,
        int data_len);
int wlan_cfg80211_list_scan(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        const void *data,
        int data_len);
int wlan_cfg80211_hmmc_params(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        const void *data,
        int data_len);
int wlan_cfg80211_sta_stats(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        const void *data,
        int data_len);
int wlan_cfg80211_80211stats(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        const void *data,
        int data_len);
int wlan_cfg80211_cloneparams(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        const void *data,
        int data_len);
int wlan_cfg80211_extendedstats(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        const void *data,
        int data_len);
int wlan_cfg80211_athstats(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        const void *data,
        int data_len);
int wlan_cfg80211_phystats(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        const void *data,
        int data_len);
int wlan_cfg80211_ald_params(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        const void *data,
        int data_len);
int wlan_cfg80211_vendor_scan(struct wiphy *wiphy,
                              struct wireless_dev *wdev,
                              const void *data,
                              int data_len);
int wlan_cfg80211_update_vendor_channel(struct wiphy *wiphy,
						struct wireless_dev *wdev,
						const void *data, int data_len);
int wlan_hdd_cfg80211_dump_survey(struct wiphy *wiphy,
                                  struct net_device *dev,
                                  int idx,
                                  struct survey_info *survey);
int ieee80211_cfg80211_construct_chandef(struct ieee80211com *ic, struct cfg80211_chan_def *chan_def,
        struct ieee80211_channel *nl_chan);
int wlan_cfg80211_httstats(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        const void *data,
        uint32_t data_len);

void wlan_cfg80211_httstats_event(struct ieee80211com *ic, void *buffer,
                                  uint32_t buffer_len);

int wlan_cfg80211_vendor_get_rropinfo(struct wiphy *wiphy,
                              struct wireless_dev *wdev,
                              const void *data,
                              int data_len);
void wlan_vendor_fwd_mgmt_frame(wlan_if_t vap, struct sk_buff *skb, u_int16_t subtype);
void ieee80211_cfg80211_post_ft_event(osif_dev *osdev);

int wlan_cfg80211_cfr_params(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        const void *data, int data_len);

int wlan_cfg80211_get_channel_survey_stats(struct ieee80211com *ic);
int wlan_cfg80211_reset_channel_survey_stats(struct ieee80211com *ic);
#endif

#endif
