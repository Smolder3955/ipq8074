/*
 * Copyright (c) 2016, 2017 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary . Qualcomm Innovation Center, Inc.
 *
 * 2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#ifndef ATH_UCFG_H_
#define ATH_UCFG_H_
enum {
    SPECIAL_PARAM_COUNTRY_ID,
    SPECIAL_PARAM_ASF_AMEM_PRINT,
    SPECIAL_PARAM_DISP_TPC,
    SPECIAL_PARAM_ENABLE_CH_144,
    SPECIAL_PARAM_REGDOMAIN = 12,
    SPECIAL_PARAM_ENABLE_MAC_REQ = 14,
    SPECIAL_PARAM_ENABLE_SHPREAMBLE,
    SPECIAL_PARAM_ENABLE_SHSLOT,
    SPECIAL_PARAM_RADIO_MGMT_RETRY_LIMIT,
};

int ath_ucfg_setparam(void *vscn, int param, int value);
int ath_ucfg_getparam(void *vscn, int param, int *val);
int ath_ucfg_set_countrycode(void *vscn, char *cntry);
int ath_ucfg_get_country(void *vscn, char *str);
void ath_ucfg_set_dscp_tid_map(void *vscn, u_int8_t tos, u_int8_t tid);
int ath_ucfg_get_dscp_tid_map(void *vscn, u_int8_t tos);
int ath_ucfg_set_mac_address(void *vscn, char *addr);
int ath_ucfg_set_smartantenna_param(void *vscn, char *val);
int ath_ucfg_get_smartantenna_param(void *vscn, char *val);
int ath_ucfg_create_vap(struct ath_softc_net80211 *scn, struct ieee80211_clone_params *cp, char *dev_name);
int ath_ucfg_get_vap_info(struct ath_softc_net80211 *scn, struct ieee80211_profile *profile);
int ath_ucfg_diag(struct ath_softc_net80211 *scn, struct ath_diag *ad);
int ath_ucfg_phyerr(void *vscn, void *vad);
int ath_extended_commands(struct net_device *dev, void *vextended_cmd);
int ath_iw_gethwaddr(struct net_device *dev, void *vinfo, void *w, char *extra);
#endif //ATH_UCFG_H_
