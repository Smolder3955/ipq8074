/*
 * Copyright (c) 2016-2017 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary . Qualcomm Innovation Center, Inc.
 *
 * 2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#ifndef OL_ATH_UCFG_H_
#define OL_ATH_UCFG_H_
/*
 ** "split" of config param values, since they are all combined
 ** into the same table.  This value is a "shift" value for ATH parameters
 */
#define OL_ATH_PARAM_SHIFT     0x1000
#define OL_SPECIAL_PARAM_SHIFT 0x2000
#define OL_MGMT_RETRY_LIMIT_MIN (1)
#define OL_MGMT_RETRY_LIMIT_MAX (15)
#define ATH_XIOCTL_UNIFIED_UTF_CMD  0x1000
#define ATH_XIOCTL_UNIFIED_UTF_RSP  0x1001
#define ATH_FTM_UTF_CMD 0x1002
#define MAX_UTF_LENGTH 2048

enum {
    OL_SPECIAL_PARAM_COUNTRY_ID,
    OL_SPECIAL_PARAM_ASF_AMEM_PRINT,
    OL_SPECIAL_DBGLOG_REPORT_SIZE,
    OL_SPECIAL_DBGLOG_TSTAMP_RESOLUTION,
    OL_SPECIAL_DBGLOG_REPORTING_ENABLED,
    OL_SPECIAL_DBGLOG_LOG_LEVEL,
    OL_SPECIAL_DBGLOG_VAP_ENABLE,
    OL_SPECIAL_DBGLOG_VAP_DISABLE,
    OL_SPECIAL_DBGLOG_MODULE_ENABLE,
    OL_SPECIAL_DBGLOG_MODULE_DISABLE,
    OL_SPECIAL_PARAM_DISP_TPC,
    OL_SPECIAL_PARAM_ENABLE_CH_144,
    OL_SPECIAL_PARAM_REGDOMAIN,
    OL_SPECIAL_PARAM_ENABLE_OL_STATS,
    OL_SPECIAL_PARAM_ENABLE_MAC_REQ,
    OL_SPECIAL_PARAM_ENABLE_SHPREAMBLE,
    OL_SPECIAL_PARAM_ENABLE_SHSLOT,
    OL_SPECIAL_PARAM_RADIO_MGMT_RETRY_LIMIT,
    OL_SPECIAL_PARAM_SENS_LEVEL,
    OL_SPECIAL_PARAM_TX_POWER_5G,
    OL_SPECIAL_PARAM_TX_POWER_2G,
    OL_SPECIAL_PARAM_CCA_THRESHOLD,
    OL_SPECIAL_PARAM_WLAN_PROFILE_ID_ENABLE,
    OL_SPECIAL_PARAM_WLAN_PROFILE_TRIGGER,
    OL_SPECIAL_PARAM_ENABLE_CH144_EPPR_OVRD,
    OL_SPECIAL_PARAM_ENABLE_PERPKT_TXSTATS,
    OL_SPECIAL_PARAM_ENABLE_OL_STATSv2,
    OL_SPECIAL_PARAM_ENABLE_OL_STATSv3,
    OL_SPECIAL_PARAM_BSTA_FIXED_IDMASK,
};

#define HE_SR_NON_SRG_OBSS_PD_MAX_THRESH_OFFSET_VAL 20
enum {
    HE_SR_SRP_ENABLE = 1,
    HE_SR_NON_SRG_OBSSPD_ENABLE = 2,
    HE_SR_SR15_ENABLE = 3,
};

int ol_ath_ucfg_setparam(void *vscn, int param, int value);
int ol_ath_ucfg_getparam(void *vscn, int param, int *val);
int ol_ath_ucfg_set_country(void *vscn, char *cntry);
int ol_ath_ucfg_get_country(void *vscn, char *str);
int ol_ath_ucfg_set_mac_address(void *vscn, char *addr);
int ol_ath_ucfg_gpio_config(void *scn,
        int gpionum,
        int input,
        int pulltype,
        int intrmode);
int ol_ath_ucfg_gpio_output(void *scn, int gpionum, int set);
int ol_ath_ucfg_set_smart_antenna_param(void *vscn, char *val);
int ol_ath_ucfg_get_smart_antenna_param(void *vscn, char *val);
void ol_ath_ucfg_txrx_peer_stats(void *vscn, char *addr);
void ol_ath_set_ba_timeout(void *vscn, uint8_t ac, uint32_t value);
void ol_ath_get_ba_timeout(void *vscn, uint8_t ac, uint32_t *value);
int ol_ath_ucfg_create_vap(struct ol_ath_softc_net80211 *scn, struct ieee80211_clone_params *cp, char *dev_name);
/*Function to handle UTF commands from QCMBR and FTM daemon */
int ol_ath_ucfg_utf_unified_cmd(void *data, int cmd, char *userdata, unsigned int length);
int ol_ath_ucfg_get_ath_stats(void *vscn, void *vasc);
int ol_ath_ucfg_get_vap_info(struct ol_ath_softc_net80211 *scn, struct ieee80211_profile *profile);
int ol_ath_ucfg_get_nf_dbr_dbm_info(struct ol_ath_softc_net80211 *scn);
int ol_ath_ucfg_get_packet_power_info(struct ol_ath_softc_net80211 *scn, struct packet_power_info_params *param);
int ol_ath_ucfg_phyerr(void *vscn, void *vad);
int ol_ath_ucfg_ctl_set(struct ol_ath_softc_net80211 *scn, ath_ctl_table_t *ptr);
int ol_ath_ucfg_set_op_support_rates(struct ol_ath_softc_net80211 *scn, struct ieee80211_rateset *target_rs);
int ol_ath_ucfg_btcoex_duty_cycle(void *vscn, u_int32_t period, u_int32_t duration);

int ol_ath_ucfg_get_radio_supported_rates(struct ol_ath_softc_net80211 *scn,
    enum ieee80211_phymode mode,
    struct ieee80211_rateset *target_rs);

int ol_ath_ucfg_set_aggr_burst(void *scn, uint32_t ac, uint32_t duration);
int ol_ath_ucfg_set_atf_sched_dur(void *vscn, uint32_t ac, uint32_t duration);
int ol_ath_extended_commands(struct net_device *dev, void *vextended_cmd);
int ol_ath_iw_get_aggr_burst(struct net_device *dev, void *vinfo, void *w, char *extra);
void ol_ath_get_dp_fw_peer_stats(void *vscn, char *extra, uint8_t caps);
void ol_ath_get_dp_htt_stats (void *vscn, void *data, uint32_t data_len);
#if ATH_SUPPORT_ICM
int ol_get_nominal_nf(struct ieee80211com *ic);
#endif /* ATH_SUPPORT_ICM */
int ol_ath_ucfg_set_he_bss_color(void *vscn, uint8_t value, uint8_t ovrride);
int ol_ath_ucfg_get_he_bss_color(void *vscn, int *value);
int ol_ath_ucfg_set_he_sr_config(void *vscn, uint8_t param, uint8_t value, void *data);
int ol_ath_ucfg_get_he_sr_config(void *vscn, uint8_t param, uint32_t *value);
int ol_ath_ucfg_set_pcp_tid_map(void *vscn, uint32_t pcp, uint32_t tid);
int ol_ath_ucfg_get_pcp_tid_map(void *vscn, uint32_t pcp, uint32_t *value);
int ol_ath_ucfg_set_tidmap_prty(void *vscn, uint32_t val);
int ol_ath_ucfg_get_tidmap_prty(void *vscn, uint32_t *val);

#ifdef WLAN_SUPPORT_RX_PROTOCOL_TYPE_TAG
int ol_ath_ucfg_set_rx_pkt_protocol_tagging(void *vscn,
            struct ieee80211_rx_pkt_protocol_tag *rx_pkt_protocol_tag_info);
#ifdef WLAN_SUPPORT_RX_TAG_STATISTICS
int ol_ath_ucfg_dump_rx_pkt_protocol_tag_stats(void *vscn, uint32_t protocol_type);
#endif //WLAN_SUPPORT_RX_TAG_STATISTICS
#endif //WLAN_SUPPORT_RX_PROTOCOL_TYPE_TAG

#endif //OL_ATH_UCFG_H_

