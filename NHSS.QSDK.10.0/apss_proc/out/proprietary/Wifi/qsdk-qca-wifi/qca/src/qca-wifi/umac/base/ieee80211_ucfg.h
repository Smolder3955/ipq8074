/*
 * Copyright (c) 2016-2019 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#ifndef IEEE80211_UCFG_H_
#define IEEE80211_UCFG_H_
int ieee80211_ucfg_set_essid(wlan_if_t vap, ieee80211_ssid *data, bool is_vap_restart_required);
int ieee80211_ucfg_set_beacon_interval(wlan_if_t vap, struct ieee80211com *ic,
        int value, bool is_vap_restart_required);
int ieee80211_ucfg_get_essid(wlan_if_t vap, ieee80211_ssid *data, int *nssid);
int ieee80211_ucfg_get_freq(wlan_if_t vap);
int ieee80211_ucfg_set_freq(wlan_if_t vap, int ieeechannel);
int ieee80211_ucfg_set_freq_internal(wlan_if_t vap, int ieeechannel);
int ieee80211_ucfg_set_chanswitch(wlan_if_t vaphandle, u_int8_t chan, u_int8_t tbtt, u_int16_t ch_width);
wlan_chan_t ieee80211_ucfg_get_current_channel(wlan_if_t vaphandle, bool hwChan);
wlan_chan_t ieee80211_ucfg_get_bss_channel(wlan_if_t vaphandle);
int ieee80211_ucfg_delete_vap(wlan_if_t vap);
int ieee80211_ucfg_set_rts(wlan_if_t vap, u_int32_t val);
int ieee80211_ucfg_set_frag(wlan_if_t vap, u_int32_t val);
int ieee80211_ucfg_set_txpow(wlan_if_t vaphandle, int txpow);
int ieee80211_ucfg_get_txpow(wlan_if_t vaphandle, int *txpow, int *fixed);
int ieee80211_ucfg_get_txpow_fraction(wlan_if_t vaphandle, int *txpow, int *fixed);
int ieee80211_ucfg_set_ap(wlan_if_t vap, u_int8_t (*des_bssid)[IEEE80211_ADDR_LEN]);
int ieee80211_ucfg_get_ap(wlan_if_t vap, u_int8_t *addr);
int ieee80211_ucfg_setparam(wlan_if_t vap, int param, int value, char *extra);
int ieee80211_ucfg_getparam(wlan_if_t vap, int param, int *value);
u_int32_t ieee80211_ucfg_get_maxphyrate(wlan_if_t vaphandle);
int ieee80211_ucfg_set_phymode(wlan_if_t vap, char *modestr, int len);
int ieee80211_ucfg_set_wirelessmode(wlan_if_t vap, int mode);
int ieee80211_ucfg_set_encode(wlan_if_t vap, u_int16_t length, u_int16_t flags, void *keybuf);
int ieee80211_ucfg_set_rate(wlan_if_t vap, int value);
int ieee80211_ucfg_get_phymode(wlan_if_t vap, char *modestr, u_int16_t *length, int type);
int ieee80211_ucfg_splitmac_add_client(wlan_if_t vap, u_int8_t *stamac, u_int16_t associd,
        u_int8_t qos, struct ieee80211_rateset lrates,
        struct ieee80211_rateset htrates, u_int16_t vhtrates);
int ieee80211_ucfg_splitmac_del_client(wlan_if_t vap, u_int8_t *stamac);
int ieee80211_ucfg_splitmac_authorize_client(wlan_if_t vap, u_int8_t *stamac, u_int32_t authorize);
int ieee80211_ucfg_splitmac_set_key(wlan_if_t vap, u_int8_t *macaddr, u_int8_t cipher,
        u_int16_t keyix, u_int32_t keylen, u_int8_t *keydata);
int ieee80211_ucfg_splitmac_del_key(wlan_if_t vap, u_int8_t *macaddr, u_int16_t keyix);
int ieee80211_ucfg_getstainfo(wlan_if_t vap, struct ieee80211req_sta_info *si, uint32_t *len);
int ieee80211_ucfg_getstaspace(wlan_if_t vap);

QDF_STATUS wlan_pdev_wait_to_bringdown_all_vdevs(struct ieee80211com *ic);

int ieee80211_convert_mode(const char *mode);
#if ATH_SUPPORT_IQUE
int ieee80211_ucfg_rcparams_setrtparams(wlan_if_t vap, uint8_t rt_index, uint8_t per, uint8_t probe_intvl);
int ieee80211_ucfg_rcparams_setratemask(wlan_if_t vap, uint8_t preamble,
        uint32_t mask_lower32, uint32_t mask_higher32, uint32_t mask_lower32_2);
#endif
int ieee80211_ucfg_nawds(wlan_if_t vap, struct ieee80211_wlanconfig *config);
int ieee80211_ucfg_hmmc(wlan_if_t vap, struct ieee80211_wlanconfig *config);
int ieee80211_ucfg_ald(wlan_if_t vap, struct ieee80211_wlanconfig *config);
int ieee80211_ucfg_hmwds(wlan_if_t vap, struct ieee80211_wlanconfig *config, int buffer_len);
int ieee80211_ucfg_wnm(wlan_if_t vap, struct ieee80211_wlanconfig *config);
int ieee80211_ucfg_vendorie(wlan_if_t vap, struct ieee80211_wlanconfig_vendorie *vie);
int ieee80211_ucfg_addie(wlan_if_t vap, struct ieee80211_wlanconfig_ie *ie_buffer);
int ieee80211_ucfg_nac(wlan_if_t vap, struct ieee80211_wlanconfig *config);
int ieee80211_ucfg_nac_rssi(wlan_if_t vap, struct ieee80211_wlanconfig *config);
int ieee80211_ucgf_scanlist(wlan_if_t vap);
size_t scan_space(wlan_scan_entry_t se, u_int16_t *ielen);
QDF_STATUS get_scan_space(void *arg, wlan_scan_entry_t se);
QDF_STATUS get_scan_space_rep_move(void *arg, wlan_scan_entry_t se);
QDF_STATUS get_scan_result(void *arg, wlan_scan_entry_t se);
int ieee80211_ucfg_get_best_otherband_uplink_bssid(wlan_if_t vap, char *bssid);
int ieee80211_ucfg_get_otherband_uplink_bssid(wlan_if_t vap, char *bssid);
int ieee80211_ucfg_set_otherband_bssid(wlan_if_t vap, int *val);
int ieee80211_ucfg_scanlist(wlan_if_t vap);
void ieee80211_ucfg_setmaxrate_per_client(void *arg, wlan_node_t node);
void get_sta_space(void *arg, wlan_node_t node);
void get_sta_info(void *arg, wlan_node_t node);
int ieee80211_ucfg_setwmmparams(void *osif, int wmmparam, int ac, int bss, int value);
int ieee80211_ucfg_getwmmparams(void *osif, int wmmparam, int ac, int bss);
int ieee80211_ucfg_set_peer_nexthop(void *osif, uint8_t *mac, uint32_t if_num);
int ieee80211_ucfg_set_vlan_type( void *osif, uint8_t default_vlan, uint8_t port_vlan);
int ieee80211_ucfg_set_muedcaparams(void *osif, uint8_t muedcaparam,
        uint8_t ac, uint8_t value);
#if QCA_SUPPORT_GPR
int ieee80211_ucfg_send_gprparams(wlan_if_t vap, uint8_t value);
#endif
int ieee80211_ucfg_get_muedcaparams(void *osif, uint8_t muedcaparam, uint8_t ac);
int ieee80211_ucfg_setmlme(struct ieee80211com *ic, void *osif, struct ieee80211req_mlme *mlme);
int ieee80211_ucfg_cfr_params(struct ieee80211com *ic, wlan_if_t vap, struct ieee80211_wlanconfig *config);
extern unsigned int g_unicast_deauth_on_stop;
extern unsigned int g_csa_max_rx_wait_time;
#endif //IEEE80211_UCFG_H_
