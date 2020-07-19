/*
 * Copyright (c) 2017-2019 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * Copyright (c) 2011, Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/*
 * LMAC offload interface functions for UMAC - for power and performance offload model
 */
#include "ol_if_athvar.h"
#include "ol_if_athpriv.h"
#include "ol_txrx_api_internal.h"
#include <osdep.h>
#include <hif.h>
#include "bmi.h"
#include "target_type.h"
#include "sw_version.h"
#include "targaddrs.h"
#include "ol_helper.h"
#include "cdp_txrx_cmn.h"
#include "qdf_lock.h"  /* qdf_spinlock_* */
#include "qdf_types.h" /* qdf_vprint */
#include "qdf_util.h" /* qdf_vprint */
#include "qdf_mem.h"   /* qdf_mem_malloc,free */
#include "qdf_trace.h"
#include "wmi_unified_api.h"
#include "cdp_txrx_cmn_reg.h"
#include "wlan_lmac_if_def.h"
#include "wlan_lmac_if_api.h"
#include "wlan_mgmt_txrx_utils_api.h"
#include "cdp_txrx_ctrl.h"
#include "qwrap_structure.h"
#include <wlan_global_lmac_if_api.h>
#include "target_if.h"
#include "service_ready_util.h"
#include "wlan_scan.h"
#include "ol_cfg.h"
#include "wlan_cfg.h"
#include "pld_common.h"
#if ATH_ACS_DEBUG_SUPPORT
#include "acs_debug.h"
#endif

#ifdef QCA_PARTNER_PLATFORM
#endif

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
#include <osif_nss_wifiol_if.h>
#endif
/* FIX THIS: the HL vs. LL selection will need to be done
 * at runtime rather than compile time
 */

#if defined(CONFIG_HL_SUPPORT)
#include "wlan_tgt_def_config_hl.h"    /* TODO: check if we need a seperated config file */
#else
#include "wlan_tgt_def_config.h"
#endif
#include "fw_dbglog_api.h"

#include "ol_if_wow.h"
#include "a_debug.h"
#include "epping_test.h"

#include "pktlog_ac.h"
#include "ol_regdomain.h"

#include "ol_if_me.h"
#if WLAN_SPECTRAL_ENABLE
#include "target_if_spectral.h"
#include "ol_if_spectral.h"
#endif
#include "ol_ath.h"
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>

#include "ol_if_stats.h"
#include "wds_addr_api.h"

#include "qdf_atomic.h"
#include "ol_swap.h"

#include "ol_if_eeprom.h"
#if ATH_PERF_PWR_OFFLOAD

#include "ath_pci.h"
#include <linux/fs.h>
#include <linux/gpio.h>
#ifndef __LINUX_POWERPC_ARCH__
#include <asm/segment.h>
#endif
#include <asm/uaccess.h>
#include <linux/buffer_head.h>
#include "reg_struct.h"
#include "regtable.h"

#if UMAC_SUPPORT_ACFG
#include "ieee80211_acfg.h"
#include <acfg_event_types.h>   /* for ACFG_WDT_TARGET_ASSERT */
#endif

#if WIFI_MEM_MANAGER_SUPPORT
#include "mem_manager.h"
#endif

#include <osif_private.h>

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
#include <osif_nss_wifiol_vdev_if.h>
#endif
#include "wlan_mgmt_txrx_utils_api.h"
#include <dispatcher_init_deinit.h>
#if QCA_AIRTIME_FAIRNESS
#include <target_if_atf.h>
#endif
#if UNIFIED_SMARTANTENNA
#include <target_if_sa_api.h>
#endif /* UNIFIED_SMARTANTENNA */
#if WLAN_SPECTRAL_ENABLE
#include <wlan_spectral_ucfg_api.h>
#include <wlan_spectral_utils_api.h>
#endif
#include <init_event_handler.h>
#include <init_cmd_api.h>
#include <init_deinit_lmac.h>
#include <wlan_osif_priv.h>
#include <wlan_utility.h>

#include <reg_services_public_struct.h>
#include <wlan_reg_services_api.h>
#include <ieee80211_regdmn.h>
#include <ol_regdomain_common.h>
#include <dp_txrx.h>
#include <wlan_reg_ucfg_api.h>

#if UMAC_SUPPORT_CBS
#include "ieee80211_cbs.h"
#endif

#if CONFIG_LEDS_IPQ
#include <drivers/leds/leds-ipq.h>
#endif
#ifdef WLAN_SUPPORT_FILS
#include <target_if_fd.h>
#endif

#include <target_if_dp.h>
#ifdef DIRECT_BUF_RX_ENABLE
#include <target_if_direct_buf_rx_api.h>
#endif
#include "cfg_ucfg_api.h"

#ifdef QCA_SUPPORT_CP_STATS
#include <wlan_cp_stats_ic_utils_api.h>
#endif
#include "fw_dbglog_api.h"
#include <wlan_utility.h>
#if WLAN_CFR_ENABLE
#include <wlan_cfr_utils_api.h>
#endif
#include <dp_rate_stats.h>

#include <wlan_vdev_mgr_ucfg_api.h>

static int ath_device_event(struct notifier_block *unused, unsigned long event, void *ptr);
static struct notifier_block ath_device_notifier = {
	        .notifier_call = ath_device_event
};

#ifdef QCA_PARTNER_PLATFORM
extern struct ol_txrx_ast_entry_t *
ol_txrx_ast_find_hash_find(
        ol_txrx_pdev_handle pdev,
        u_int8_t *peer_mac_addr,
        int mac_addr_is_aligned);
extern struct ol_txrx_peer_t *
ol_txrx_peer_find_by_id(
    ol_txrx_pdev_handle pdev,
    u_int16_t peer_id);
#endif
extern void tpc_config_event_handler(ol_scn_t sc, u_int8_t *data, u_int32_t datalen);
extern void cnss_dump_qmi_history(void);

#if UMAC_SUPPORT_CFG80211
#include <ieee80211_cfg80211.h>
#include "ol_ath_ucfg.h"
#endif

#include "pld_common.h"

#if QCA_SUPPORT_SON
#include <wlan_son_pub.h>
#endif

#include <wlan_vdev_mgr_utils_api.h>
#define    PDEV_STATS_DEFAULT_TIMER  1000
#if QCN_IE
#define    IEEE80211_MACHDR_OFFSET  4
#endif
/* Beeliner Family specific routines */
extern int ol_target_init(ol_ath_soc_softc_t *soc, bool first);
extern struct g_wifi_info g_winfo;
int osif_recover_profile_alloc(struct ieee80211com *ic);
void ath_vdev_dump(struct wlan_objmgr_psoc *psoc, void *obj, void *args);
void osif_recover_vap_create(struct ieee80211com *ic);
int ath_netdev_open(struct net_device *dev);
int ath_netdev_stop(struct net_device *dev);
void ic_reset_params(struct ieee80211com *ic);
static u_int8_t
ol_ath_get_emiwar_80p80_defval(ol_ath_soc_softc_t *soc);
void ieee80211_buffull_handler(struct ieee80211com *ic);
void wmi_proc_create(wmi_unified_t wmi_handle, struct proc_dir_entry *par_entry, int id);
void wmi_proc_remove(wmi_unified_t wmi_handle, struct proc_dir_entry *par_entry, int id);
int osif_ol_ll_vap_hardstart(struct sk_buff *skb, struct net_device *dev);
int osif_ol_vap_hardstart_wifi3(struct sk_buff *skb, struct net_device *dev);
bool ol_ath_net80211_is_mode_offload(struct ieee80211com *ic);
QDF_STATUS ol_ath_get_ah_devid(struct wlan_objmgr_pdev *pdev, uint16_t *devid);
void *ol_hif_open(struct device *dev, void *bdev, void *bid,
        enum qdf_bus_type bus_type, bool reinit, qdf_device_t qdf_dev);
#if UMAC_SUPPORT_WEXT
void ol_ath_iw_detach(struct net_device *dev);
void ol_ath_iw_attach(struct net_device *dev);
#endif
#define DEFAULT_MGMT_RETRY_LIMIT (4)

// Disabling scan offload
#if defined(EPPING_TEST) && !defined(HIF_USB)
unsigned int eppingtest = 1;
unsigned int bypasswmi = 1;
#else
unsigned int eppingtest = 0;
unsigned int bypasswmi = 0;
#endif

#define FILE_PATH_LEN 128

#define LOW_MEM_SYSTEM_RAM              (131072)                     // 131072Kb

#define DEBUG_SNIFFER_TEST_TX_METADATA "SNIFF_TXMETA"
#define DEBUG_SNIFFER_TEST_RX_METADATA "SNIFF_RXMETA"
#define DEBUG_SNIFFER_TEST_TX_DATA     "SNIFF_TXDATA"
#define DEBUG_SNIFFER_TEST_RX_DATA     "SNIFF_RXDATA"
#define DEBUG_SNIFFER_TEST_TX_MGMT     "SNIFF_TXMGMT"
#define DEBUG_SNIFFER_TEST_RX_MGMT     "SNIFF_RXMGMT"
#define DEBUG_SNIFFER_SIGNATURE_LEN 12

int ol_ath_send_delba(void *pdev_handle, void *ctrl_peer,
                   uint8_t *peer_macaddr, uint8_t tid, void *vdev_handle,
                   uint8_t reason_code);
int ol_ath_node_add_wds_entry(void *vdev_handle, struct cdp_peer *peer_handle, const u_int8_t *dest_mac,
                          u_int8_t *peer_mac, u_int32_t flags);
int ol_ath_node_update_wds_entry(void *vdev_handle, u_int8_t *wds_macaddr,
                          u_int8_t *peer_macaddr, u_int32_t flags);
void ol_ath_node_del_wds_entry(void *vdev_handle, u_int8_t *dest_mac,
                               uint8_t type);
uint8_t ol_ath_rx_invalid_peer(void *scn_handle, void *msg);
int ol_ath_peer_unref_delete(void *scn_handle, uint8_t *peer_mac,
                             uint8_t *vdev_mac, enum wlan_op_mode opmode,
                             void *old_peer, void *new_peer);
int ol_peer_map_event(void *scn_handle, uint16_t peer_id, uint16_t hw_peer_id, uint8_t vdev_id, uint8_t *peer_mac_addr, enum cdp_txrx_ast_entry_type  peer_type,
		uint32_t tx_ast_hash);
int ol_peer_unmap_event(void *scn_handle, uint16_t peer_id, uint8_t vdev_id);
int ol_ath_get_dp_config_param(void *soc, enum cdp_cfg_param_type param_num);
void ol_ath_rx_mic_error(void *scn_handle, uint16_t vdev_id, void *wh);

void ol_reset_params(struct ol_ath_softc_net80211 *scn);
QDF_STATUS ol_ath_lro_hash_config(void *soc_handle,
                struct cdp_lro_hash_config *lro_hash_cfg);

uint8_t ol_ath_freq_to_channel(void *scn_handle, uint16_t freq);
int ol_ath_peer_sta_kickout(void *pdev, uint8_t *peer_mac);
int ol_ath_node_delete_multiple_wds_entries(void *vdev_handle, u_int8_t *wds_macaddr,
        u_int8_t *peer_macaddr, u_int32_t flags);

static struct ol_if_ops  dp_ol_if_ops = {
    .peer_set_default_routing = target_if_peer_set_default_routing,
    .peer_rx_reorder_queue_setup = target_if_peer_rx_reorder_queue_setup,
    .peer_rx_reorder_queue_remove = target_if_peer_rx_reorder_queue_remove,
    .peer_add_wds_entry = ol_ath_node_add_wds_entry,
    .peer_update_wds_entry = ol_ath_node_update_wds_entry,
    .peer_del_wds_entry = ol_ath_node_del_wds_entry,
    .peer_unref_delete = ol_ath_peer_unref_delete,
    .update_dp_stats = NULL,
    .rx_invalid_peer = ol_ath_rx_invalid_peer,
    .peer_map_event = ol_peer_map_event,
    .peer_unmap_event= ol_peer_unmap_event,
    .get_dp_cfg_param = ol_ath_get_dp_config_param,
    .lro_hash_config = target_if_lro_hash_config,
    .rx_mic_error= ol_ath_rx_mic_error,
    .freq_to_channel= ol_ath_freq_to_channel,
#if QCA_SUPPORT_SON
    .record_act_change = son_record_act_change,
#endif
#if ATH_SUPPORT_NAC_RSSI
    .config_fw_for_nac_rssi = ol_ath_config_fw_for_nac_rssi,
    .config_bssid_in_fw_for_nac_rssi = ol_ath_config_bssid_in_fw_for_nac_rssi,
#endif
    .peer_sta_kickout = ol_ath_peer_sta_kickout,
    .send_delba = ol_ath_send_delba,
    .peer_delete_multiple_wds_entries = ol_ath_node_delete_multiple_wds_entries,
    /* TODO: Add any other control path calls required to OL_IF/WMA layer */
};

int ol_ath_pdev_tpc_config_event_handler (ol_soc_t sc, u_int8_t *data, u_int32_t datalen);
int ol_ath_gpio_input_event_handler (ol_soc_t sc, u_int8_t *data, u_int32_t datalen);
int ol_ath_nf_dbr_dbm_info_event_handler (ol_soc_t sc, u_int8_t *data, u_int32_t datalen);
int ol_ath_packet_power_info_event_handler (ol_soc_t sc, u_int8_t *data, u_int32_t datalen);
int ol_ath_generic_buffer_event_handler (ol_soc_t sc, u_int8_t *data, u_int32_t datalen);
int ol_ath_peer_mumimo_tx_count_event_handler (ol_soc_t sc, u_int8_t *data, u_int32_t datalen);
int ol_ath_peer_gid_userpos_list_event_handler (ol_soc_t sc, u_int8_t *data, u_int32_t datalen);
int ol_ath_pdev_caldata_version_check_event_handler (ol_soc_t sc, u_int8_t *data, u_int32_t datalen);
int ol_ath_pdev_ctl_failsafe_check_event_handler (ol_soc_t sc, u_int8_t *data, u_int32_t datalen);
#ifdef CONFIG_DP_TRACE
static int ol_ath_dptrace_setparam(int cmd, int val1, int val2);
#endif

#if  QCN_ESP_IE
int ol_ath_esp_estimate_event_handler(ol_soc_t sc, u_int8_t *data, u_int32_t datalen);
#endif

#if ATH_SUPPORT_LOWI
extern int ol_ath_lowi_data_req_to_fw(struct ieee80211com *ic, int msg_len, void *req, int msgsubType);
#endif
extern u_int8_t prealloc_disabled;

#if OL_ATH_SUPPORT_LED
static OS_TIMER_FUNC(ol_ath_led_blink_timed_out);
static OS_TIMER_FUNC(ol_ath_led_poll_timed_out);

/* 50Mbps per entry */
bool ipq4019_led_initialized = 0;
uint32_t ipq4019_led_type = 0;      /* ipq40xx gpio or led source type */

static const
OL_LED_BLINK_RATES ol_led_blink_rate_table[] = {
    {  500, 130 },
    {  400, 100 },
    {  280,  70 },
    {  240,  60 },
    {  200,  50 },
    {  160,  40 },
    {  130,  30 },
    {  100,  30 },
    {  90,  20 },
    {  80,  20 },
    {  70,  20 },
    {  60,  10 },
    {  50,  10 },
    {  40,  10 },
};

#endif

extern int whal_mcs_to_kbps(int, int, int, int);
#if ALL_POSSIBLE_RATES_SUPPORTED
    extern int whal_kbps_to_mcs(int, int, int);
    extern int whal_get_supported_rates(int, int, int **);
#else
    extern int whal_kbps_to_mcs(int, int, int, int, int);
    extern int whal_get_supported_rates(int, int, int, int, int **);
#endif
extern int whal_ratecode_to_kbps(uint8_t, uint8_t, uint8_t);
extern void ol_if_mgmt_drain(struct ieee80211_node *ni, int force);
extern int32_t ol_ath_thermal_mitigation_detach(struct ol_ath_softc_net80211 *scn,
                                              struct net_device *dev);
extern int32_t ol_ath_thermal_mitigation_attach(struct ol_ath_softc_net80211 *scn,
                                              struct net_device *dev);

#if ATH_SUPPORT_HYFI_ENHANCEMENTS
extern int ath_net80211_add_hmmc(struct ieee80211vap *vap, u_int32_t ip, u_int32_t mask);
extern int ath_net80211_del_hmmc(struct ieee80211vap *vap, u_int32_t ip, u_int32_t mask);
extern void ath_net80211_buffull_handler(struct ieee80211com *ic);
#endif

extern struct global_ic_list ic_list;

extern unsigned int testmode;

static int
ol_ath_dcs_interference_handler (ol_soc_t sc,
			  u_int8_t *data, u_int32_t datalen);

int ol_ath_target_stop(struct ieee80211com *ic, bool flush_wq);
extern int ath_get_radio_index(struct net_device *netdev);
extern int asf_adf_attach(void);


#ifdef WLAN_CONV_CRYPTO_SUPPORTED
static QDF_STATUS ol_ath_register_crypto_ops_handler(
                        struct wlan_lmac_if_crypto_tx_ops *crypto_tx_ops);
#endif

/*
 * ol_ath_pri20_cfg_blockchanlist_parse:
 * Block channels listed in CFG from auto channel selection (ACS, ICM, ...)
 */
static void ol_ath_pri20_cfg_blockchanlist_parse(struct ieee80211com *ic,
                                                 const char *cfg_str);

static
int diag_fw_event_handler(ol_soc_t sc, uint8_t *data, uint32_t datalen)
{
    ol_ath_soc_softc_t *soc = (ol_ath_soc_softc_t *) sc;
    struct wlan_objmgr_psoc *psoc;
    void *dbglog_handle;
    int ret;
    struct target_psoc_info *tgt_psoc_info;

    psoc = soc->psoc_obj;
    tgt_psoc_info = (struct target_psoc_info *)wlan_psoc_get_tgt_if_handle(
                                                    psoc);
    if (tgt_psoc_info == NULL) {
        qdf_info("%s: target_psoc_info is null ", __func__);
        return -EINVAL;
    }

    if (!(dbglog_handle = target_psoc_get_dbglog_hdl(tgt_psoc_info))) {
        qdf_info("%s: dbglog_handle is null ", __func__);
        return -EINVAL;
    }

    ret = fwdbg_fw_handler(dbglog_handle, soc, data, datalen);

    return ret;
}

static inline void ol_ath_dbglog_detach(ol_ath_soc_softc_t *soc)
{
        struct target_psoc_info *tgt_psoc_info;
        void *dbglog_handle = NULL;
        struct wmi_unified *wmi_handle = NULL;

        tgt_psoc_info = (struct target_psoc_info *)wlan_psoc_get_tgt_if_handle(
						soc->psoc_obj);
        dbglog_handle = target_psoc_get_dbglog_hdl(tgt_psoc_info);

        wmi_handle = (struct wmi_unified *)target_psoc_get_wmi_hdl(tgt_psoc_info);
        if (!wmi_handle) {
            qdf_err("null wmi_handle");
            return;
        }
        wmi_unified_unregister_event_handler(wmi_handle, wmi_diag_event_id);
        target_psoc_set_dbglog_hdl(tgt_psoc_info, NULL);

        if (dbglog_handle) {
#ifdef OL_ATH_SMART_LOGGING
                fwdbg_smartlog_deinit(dbglog_handle, soc);
#endif /* OL_ATH_SMART_LOGGING */
                fwdbg_free(dbglog_handle, soc);
                if (soc->ol_if_ops->dbglog_detach)
                        soc->ol_if_ops->dbglog_detach(dbglog_handle);
        } else {
               qdf_info("%s: dbglog_handle is null ", __func__);
        }
}

int
ol_ath_get_dp_config_param(void *psoc,
		enum cdp_cfg_param_type param_num)
{
	switch(param_num)
	{
		case  CDP_CFG_MAX_PEER_ID:
			return ol_cfg_max_peer_id(psoc);
		case CDP_CFG_CCE_DISABLE:
			return ol_cfg_is_cce_disable(psoc);
		default:
			return -EINVAL;
	}
}

void ol_ath_get_pdev_stats(void *arg)
{
#ifndef QCA_WIFI_QCA8074_VP
    struct ol_ath_softc_net80211 *scn = (struct ol_ath_softc_net80211 *)arg;
    struct wlan_objmgr_pdev *pdev = scn->sc_pdev;
    struct stats_request_params param;
    uint8_t addr[IEEE80211_ADDR_LEN];
    uint8_t *myaddr;
    wmi_unified_t pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_unified_handle(scn->sc_pdev);
    if (!pdev_wmi_handle) {
        qdf_err("WMI handle is NULL\n");
        return;
    }
    memset(&param, 0, sizeof(param));
    wlan_pdev_obj_lock(pdev);
    myaddr = wlan_pdev_get_hw_macaddr(pdev);
    IEEE80211_ADDR_COPY(addr, myaddr);
    wlan_pdev_obj_unlock(pdev);

    param.pdev_id = lmac_get_pdev_idx(pdev);
    param.stats_id = WMI_HOST_REQUEST_PDEV_STAT;
    wmi_unified_stats_request_send(pdev_wmi_handle, addr, &param);
    if (scn->is_scn_stats_timer_init) {
        qdf_timer_mod(&(scn->scn_stats_timer),scn->pdev_stats_timer);
    }
#else
    return;
#endif
}
EXPORT_SYMBOL(ol_ath_get_pdev_stats);

void ol_ath_clear_auth_cnt(void *arg)
{
    struct ol_ath_softc_net80211 *scn = (struct ol_ath_softc_net80211 *)arg;
    qdf_atomic_init(&(scn->auth_cnt));
    qdf_timer_mod(&(scn->auth_timer), DEFAULT_AUTH_CLEAR_TIMER);
}

/*
 * ol_ath_pri20_cfg_blockchanlist_parse:
 * Parse the CFG string param containing channel numbers to block from
 * channel selection (ACS, ICM, ...)
 *
 * Arguments:
 * ic     : ic handle (containing channel lists)
 * cfg_str: CFG string parameter
 *
 * Return:
 * None
 */
static void ol_ath_pri20_cfg_blockchanlist_parse(struct ieee80211com *ic,
                                                 const char *cfg_str)
{
    uint8_t ix = 0;
    int offset = 0;
    const char *str_ptr = cfg_str;

    if (cfg_str == NULL || cfg_str[0] == '\0') {
        qdf_info("cfg block channel list is empty");
        return;
    }

    while (*str_ptr != '\0') {
        switch(*str_ptr) {
            case ',':
                if (ix == (ACS_MAX_CHANNEL_COUNT-1)) {
                    qdf_info("Max channel count reached, discarding cfg param");
                    qdf_mem_zero(&ic->ic_pri20_cfg_blockchanlist,
                                 sizeof(ieee80211_user_chanlist_t));
                    return;
                }
                ix++;
                str_ptr++;
                break;
            case ' ' :
            case '\"':
            case '\'':
                /* Skipping spaces, single and double-inverted commas */
                str_ptr++;
                continue;
            default :
                if (*str_ptr >= '0' && *str_ptr <= '9') {
                    if(!sscanf(str_ptr, "%hhu %n",
                               &ic->ic_pri20_cfg_blockchanlist.chan[ix],
                               &offset)) {
                        qdf_info("Could not parse channel number from cfg param, discarding cfg param");
                        qdf_mem_zero(&ic->ic_pri20_cfg_blockchanlist,
                                     sizeof(ieee80211_user_chanlist_t));
                        return;
                    }
                    str_ptr += offset;
                    ic->ic_pri20_cfg_blockchanlist.n_chan++;
                    qdf_info("(%2hhu) chan %3hhu blocked from cfg",
                             ic->ic_pri20_cfg_blockchanlist.n_chan,
                             ic->ic_pri20_cfg_blockchanlist.chan[ix]);
                    break;
                } else {
                    qdf_info("Invalid character in string, discarding cfg param");
                    qdf_mem_zero(&ic->ic_pri20_cfg_blockchanlist,
                                 sizeof(ieee80211_user_chanlist_t));
                    return;
                }
        }
    }

    return;
}

bool
ol_ath_get_host_hybrid_mode(ol_ath_soc_softc_t *soc)
{
    return wlan_psoc_nif_fw_ext_cap_get(soc->psoc_obj,
                                   WLAN_SOC_CEXT_HYBRID_MODE);
}

/* host_interest_item_address required by only wifi2.0, but its been invoked
 * from shared code. Since this function using definitions from targaddr.h
 * firmware header which is same across 2.0 & 3.0 hence it is maintained here.
 */
__inline__
u_int32_t host_interest_item_address(u_int32_t target_type, u_int32_t item_offset)
{
    switch (target_type)
    {
        default:
            ASSERT(0);
        case TARGET_TYPE_AR6002:
            return (AR6002_HOST_INTEREST_ADDRESS + item_offset);
        case TARGET_TYPE_AR6003:
            return (AR6003_HOST_INTEREST_ADDRESS + item_offset);
        case TARGET_TYPE_AR6004:
            return (AR6004_HOST_INTEREST_ADDRESS + item_offset);
        case TARGET_TYPE_AR6006:
            return (AR6006_HOST_INTEREST_ADDRESS + item_offset);
        case TARGET_TYPE_AR9888:
            return (AR9888_HOST_INTEREST_ADDRESS + item_offset);
        case TARGET_TYPE_AR6320:
            return (AR6320_HOST_INTEREST_ADDRESS + item_offset);
        case TARGET_TYPE_AR900B:
            return (AR900B_HOST_INTEREST_ADDRESS + item_offset);
        case TARGET_TYPE_QCA9984:
            return (QCA9984_HOST_INTEREST_ADDRESS + item_offset);
        case TARGET_TYPE_QCA9888:
            return (QCA9888_HOST_INTEREST_ADDRESS + item_offset);
        case TARGET_TYPE_IPQ4019:
            return (IPQ4019_HOST_INTEREST_ADDRESS + item_offset);
    }
}
qdf_export_symbol(host_interest_item_address);

#if UMAC_SUPPORT_ACFG
extern void acfg_offchan_cancel(struct ieee80211com *ic);
#endif

void wlan_pdev_operation(struct wlan_objmgr_psoc *psoc,
                              void *obj, void *args)
{
    struct ol_ath_softc_net80211 *scn;
    struct wlan_objmgr_pdev *pdev;
    struct cdp_pdev *dp_pdev;
    ol_txrx_soc_handle soc_txrx_handle;
    struct pdev_op_args *arg = (struct pdev_op_args *)args;
    struct common_wmi_handle *pdev_wmi_handle;
    struct ieee80211com *ic;
    uint8_t i;
    int waitcnt;

    pdev = (struct wlan_objmgr_pdev *)obj;

    scn = (struct ol_ath_softc_net80211*)lmac_get_pdev_feature_ptr(pdev);
    if (scn == NULL)
       return;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(pdev);
    if(pdev_wmi_handle == NULL)
       return;

    ic =  &scn->sc_ic;

    switch (arg->type) {
       case PDEV_ITER_POWERUP:
       {
#if UMAC_SUPPORT_WEXT
           ol_ath_iw_attach(scn->netdev);
#endif
#if UMAC_SUPPORT_ACFG
           OSIF_RADIO_DELIVER_EVENT_WATCHDOG(ic, ACFG_WDT_REINIT_DONE);
#endif
           ic->recovery_in_progress = 0;
           break;
       }
       case PDEV_ITER_FATAL_SHUTDOWN:
       {
           ic->recovery_in_progress = 1;
           /* stop WMI so that no more WMI commands are sent
            * to target
            */
           if (pdev_wmi_handle)
               wmi_stop((wmi_unified_t)pdev_wmi_handle);
           break;
       }
       case PDEV_ITER_TARGET_ASSERT:
       {
           ic->recovery_in_progress = 1;
#if UMAC_SUPPORT_WEXT
           ol_ath_iw_detach(scn->netdev);
#endif
#if (UMAC_SUPPORT_ACFG && ACFG_NETLINK_TX)
           acfg_offchan_cancel(ic);
           OSIF_RADIO_DELIVER_EVENT_WATCHDOG(ic, ACFG_WDT_TARGET_ASSERT);
#endif
#ifdef QCA_SUPPORT_CP_STATS
           pdev_cp_stats_tgt_asserts_inc(ic->ic_pdev_obj, 1);
#endif
           break;
       }
       case PDEV_ITER_PCIE_ASSERT:
       {
#if UMAC_SUPPORT_ACFG
           OSIF_RADIO_DELIVER_EVENT_WATCHDOG(ic, ACFG_WDT_PCIE_ASSERT);
#endif
           break;
       }

       case PDEV_ITER_PDEV_ENTRY_ADD:
       {
           /* Max radios reached with earlier pdev attach */
           if (arg->ret_val == PDEV_ITER_STATUS_FAIL)
               break;

           i = g_winfo.num_radios;
           if (g_winfo.wifi_radios[i].sc == NULL) {
               g_winfo.wifi_radios[i].sc = (void *)scn;
               g_winfo.wifi_radios[i].wifi_radio_type = OFFLOAD;
               scn->wifi_num = i;
               arg->ret_val = PDEV_ITER_STATUS_OK;
               g_winfo.num_radios++;
               qdf_info("num_radios=%d, wifi_radios[%d].sc = %pK"
                       "wifi_radio_type = %d", g_winfo.num_radios,
                       i, g_winfo.wifi_radios[i].sc,
                       g_winfo.wifi_radios[i].wifi_radio_type);
               if(g_winfo.num_radios > NUM_MAX_RADIOS) {
                  qdf_info("%s: Need to increase the NUM_MAX_RADIOS", __func__);
                  arg->ret_val = PDEV_ITER_STATUS_FAIL;
               }

           }
           break;
       }
       case PDEV_ITER_PDEV_ENTRY_DEL:
       {
           for(i=0;i < NUM_MAX_RADIOS;i++) {
                if (g_winfo.wifi_radios[i].sc == scn) {
                    g_winfo.wifi_radios[i].sc = NULL;
                    g_winfo.num_radios--;
                }
           }
           break;
       }
       case PDEV_ITER_RECOVERY_AHB_REMOVE:
       {
           /* stop WMI so that no more WMI commands are sent
            * to target
            */
           wmi_stop((wmi_unified_t)pdev_wmi_handle);
           /* Please do not add break, both cases should be executed */
       }
       case PDEV_ITER_RECOVERY_REMOVE:
       {
           if (osif_recover_profile_alloc(ic) != 0) {
               QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY,
                       QDF_TRACE_LEVEL_INFO, "[pdev id %d]ERROR: FW Recovery"
                       "profile alloc failed\n",
                       wlan_objmgr_pdev_get_pdev_id(pdev));
               return;
           }
           break;
       }
       case PDEV_ITER_RECOVERY_WAIT:
       {
           waitcnt = 0;
           while(wlan_pdev_get_vdev_count(pdev) &&
                          waitcnt <= OSIF_MAX_DELETE_VAP_TIMEOUT) {
               schedule_timeout_interruptible(HZ);
               waitcnt++;
           }
           if(waitcnt > OSIF_MAX_DELETE_VAP_TIMEOUT) {
                wlan_objmgr_iterate_obj_list_all(psoc, WLAN_VDEV_OP,
                         ath_vdev_dump, NULL, 1,
                         WLAN_MLME_NB_ID);
                QDF_BUG(0);
           }

           break;
       }
       case PDEV_ITER_RECOVERY_STOP:
       {
           dev_close(scn->netdev);
           ic_reset_params(ic);
           break;
       }
       case PDEV_ITER_RECOVERY_PROBE:
       {
           osif_recover_vap_create(ic);
           break;
       }
#if OL_ATH_SUPPORT_LED
       case PDEV_ITER_LED_GPIO_STATUS:
       {
           struct ol_ath_softc_net80211 *scn_temp;

           scn_temp = (struct ol_ath_softc_net80211 *)arg->pointer;
           if ((scn != scn_temp) && (scn->scn_led_gpio == scn_temp->scn_led_gpio))
                arg->ret_val = PDEV_ITER_STATUS_OK;
           break;
       }
#endif
       case PDEV_ITER_PDEV_NETDEV_STOP:
       {
          ath_netdev_stop(scn->netdev);
          break;
       }
       case PDEV_ITER_PDEV_NETDEV_OPEN:
       {
          ath_netdev_open(scn->netdev);
          break;
       }
#if UMAC_SUPPORT_ACFG
       case PDEV_ITER_TARGET_FWDUMP:
       {
           OSIF_RADIO_DELIVER_EVENT_WATCHDOG(ic, ACFG_WDT_FWDUMP_READY);
           break;
       }
#endif
       case PDEV_ITER_SEND_SUSPEND:
       {
           struct suspend_params param;

           qdf_mem_set(&param, sizeof(param), 0);
           param.disable_target_intr = *((int *)arg->pointer);
           qdf_info("%s: disable_target_intr val is %d", __func__,
                    param.disable_target_intr);

           wmi_unified_suspend_send(pdev_wmi_handle, &param,
                                    lmac_get_pdev_idx(pdev));
           break;
       }
       case PDEV_ITER_SEND_RESUME:
       {
           arg->ret_val = wmi_unified_resume_send(pdev_wmi_handle,
                                         lmac_get_pdev_idx(pdev));
           break;
       }
       case PDEV_ITER_PDEV_DEINIT_BEFORE_SUSPEND:
       {
#if OL_ATH_SUPPORT_LED
           qdf_timer_sync_cancel(&scn->scn_led_blink_timer);
           qdf_timer_sync_cancel(&scn->scn_led_poll_timer);
           if(lmac_get_tgt_type(psoc) == TARGET_TYPE_IPQ4019) {
               ipq4019_wifi_led(scn, OL_LED_OFF);
           }
#endif
           if (ol_target_lithium(psoc)) {
               qdf_timer_sync_cancel(&scn->scn_stats_timer);
           }
           qdf_timer_sync_cancel(&scn->auth_timer);
#ifdef QCA_SUPPORT_CP_STATS
           if (pdev_cp_stats_ap_stats_tx_cal_enable_get(pdev)) {
               scn->sc_ic.ic_ath_enable_ap_stats(ic, 0);
               pdev_cp_stats_ap_stats_tx_cal_enable_update(pdev, 1);
           }
#endif

#if ATH_ACS_DEBUG_SUPPORT
           acs_debug_cleanup(ic->ic_acs);
#endif

#if ATH_DATA_TX_INFO_EN
           ol_ath_stats_detach(ic);
#endif

           break;
       }
       case PDEV_ITER_PDEV_DETACH_OP:
       {
           soc_txrx_handle = wlan_psoc_get_dp_handle(psoc);
           dp_pdev = wlan_pdev_get_dp_handle(scn->sc_pdev);

           wlan_pdev_set_dp_handle(scn->sc_pdev, NULL);
           qdf_info("soc_txrx_handle %pK dp_pdev %pK", soc_txrx_handle, dp_pdev);
           cdp_pdev_detach(soc_txrx_handle, dp_pdev, 0);

           break;
       }
       case PDEV_ITER_PDEV_DEINIT_OP:
       {
              soc_txrx_handle = wlan_psoc_get_dp_handle(psoc);
              dp_pdev = wlan_pdev_get_dp_handle(scn->sc_pdev);

#if ATH_ACS_DEBUG_SUPPORT
           acs_debug_cleanup(scn->sc_ic.ic_acs);
#endif
           wlan_pdev_set_dp_handle(scn->sc_pdev, NULL);
           cdp_pdev_deinit(soc_txrx_handle, dp_pdev, 0);

           ol_ath_thermal_mitigation_detach(scn, scn->netdev);

           qdf_flush_work(&scn->sc_ic.dfs_cac_timer_start_work);

           qdf_flush_work(&scn->sc_ic.assoc_sm_set_country_code);

           if (ol_target_lithium(scn->soc->psoc_obj)) {
               if (scn->is_scn_stats_timer_init)
                   qdf_timer_sync_cancel(&scn->scn_stats_timer);
           }


#if WLAN_SPECTRAL_ENABLE
           spectral_unregister_dbr(scn->sc_pdev);
#endif /* WLAN_SPECTRAL_ENABLE */

#if WLAN_CFR_ENABLE
    /* pdev deinit to be done on Target Stop */
           cfr_deinitialize_pdev(scn->sc_pdev);
#endif

           ol_ath_mgmt_detach(ic);

           if (ic->recovery_in_progress)
                   ic->recovery_dfschan = ic->ic_curchan;
           else
                   ic->recovery_dfschan = NULL;

           break;
       }
       case PDEV_ITER_PDEV_RESET_PARAMS:
       {
           ol_reset_params(scn);
           break;
       }
       default:
       break;
     }
}
qdf_export_symbol(wlan_pdev_operation);

struct file* file_open(const char* path, int flags, int rights)
{
	struct file* filp = NULL;
	mm_segment_t oldfs;
	int err = 0;
	oldfs = get_fs();
	set_fs(get_ds());
	filp = filp_open(path, flags, rights);
	set_fs(oldfs);
	if(IS_ERR(filp)) {
		err = PTR_ERR(filp);
		return NULL;
	}
	return filp;
}
qdf_export_symbol(file_open);

void file_close(struct file* file) {
	filp_close(file, NULL);
}
qdf_export_symbol(file_close);

/* wow_nack is a param used by MCL's callback */
void
ol_target_send_suspend_complete(void *ctx, bool wow_nack)
{
    ol_ath_soc_softc_t *soc = (ol_ath_soc_softc_t *)ctx;

    soc->is_target_paused = TRUE;
    __ol_target_paused_event(soc);
}

#if ATH_SUPPORT_HYFI_ENHANCEMENTS
void ol_notify_if_low_on_buffers(struct ol_ath_softc_net80211 *scn, uint32_t free_buff)
{
    struct ieee80211com *ic = &scn->sc_ic;
    if (!scn) {
        qdf_info ("scn is NULL");
        return;
    }
    scn->soc->buff_thresh.free_descs = free_buff;
    if(scn->soc->buff_thresh.ald_buffull_wrn &&
            (scn->soc->buff_thresh.free_descs <= scn->soc->buff_thresh.ald_free_buf_lvl)) {
        ieee80211_buffull_handler(ic);
        scn->soc->buff_thresh.ald_buffull_wrn = 0;
    }
}
qdf_export_symbol(ol_notify_if_low_on_buffers);
#endif
/*
 * Routin to lock vap transmit
 *
 */
void ol_vap_tx_lock( void *vosdev)
{
#if !QCA_OL_TX_PDEV_LOCK && QCA_NSS_PLATFORM || (defined QCA_PARTNER_PLATFORM && QCA_PARTNER_SUPPORT_FAST_TX)
    osif_dev  *osdev = (osif_dev  *)vosdev;
    VAP_TX_SPIN_LOCK(&osdev->tx_lock);
#endif
    return;
}

/*
 * Routin to unlock vap transmit
 *
 */
void ol_vap_tx_unlock( void *vosdev)
{
#if !QCA_OL_TX_PDEV_LOCK && QCA_NSS_PLATFORM || (defined QCA_PARTNER_PLATFORM && QCA_PARTNER_SUPPORT_FAST_TX)
    osif_dev  *osdev = (osif_dev  *)vosdev;
    VAP_TX_SPIN_UNLOCK(&osdev->tx_lock);
#endif
    return;
}

#if ATH_DEBUG
extern unsigned long ath_rtscts_enable;
#define MODE_CTS_TO_SELF 0x32
#define MODE_RTS_CTS     0x31
void set_rtscts_enable(osif_dev * osdev)
{
   struct net_device *comdev = osdev->os_comdev;
   struct ol_ath_softc_net80211 *scn = (struct ol_ath_softc_net80211*) ath_netdev_priv(comdev);
   wlan_if_t vap = osdev->os_if;

   struct ol_ath_vap_net80211 *avn;
   unsigned int val = ath_rtscts_enable;

   if (vap == NULL) return;

   avn = OL_ATH_VAP_NET80211(vap);

   if (val != scn->rtsctsenable)
   {
     scn->rtsctsenable = val;
     /* Enable CTS-to-self */
     if(val == 1)
         ol_ath_wmi_send_vdev_param( scn,avn->av_if_id,
                     wmi_vdev_param_enable_rtscts, MODE_CTS_TO_SELF);
     /* Enable RTS-CTS */
     else if(val == 2)
         ol_ath_wmi_send_vdev_param( scn,avn->av_if_id,
                     wmi_vdev_param_enable_rtscts, MODE_RTS_CTS);
  }
}
qdf_export_symbol(set_rtscts_enable);
#endif

int
ol_ath_set_host_app_area(ol_ath_soc_softc_t *soc)
{
//    qdf_info("ol_ath_set_host_app_area TODO\n");
#if 0
    u_int32_t address, data;
    struct host_app_area_s host_app_area;

    /* Fetch the address of the host_app_area_s instance in the host interest area */
    address = TARG_VTOP(soc->target_type, HOST_INTEREST_ITEM_ADDRESS(soc->target_type, hi_app_host_interest));
    if (ar6000_ReadRegDiag(soc->hif_hdl, &address, &data) != A_OK) {
        return A_ERROR;
    }
    address = TARG_VTOP(soc->target_type, data);
    host_app_area.wmi_protocol_ver = WMI_PROTOCOL_VERSION;
    if (ar6000_WriteDataDiag(soc->hif_hdl, address,
                             (A_UCHAR *)&host_app_area,
                             sizeof(struct host_app_area_s)) != A_OK)
    {
        return A_ERROR;
    }
#endif
    return A_OK;
}
A_STATUS HIF_USB_connect_service(ol_ath_soc_softc_t *soc)
{
    QDF_STATUS status = QDF_STATUS_SUCCESS;
    struct htc_service_connect_req connect;
    struct htc_service_connect_resp response;
    struct common_htc_handle *htc_handle;

    htc_handle = lmac_get_htc_hdl(soc->psoc_obj);
    if (!htc_handle)
        return -1;

    A_MEMZERO(&connect,sizeof(connect));

    connect.EpCallbacks.EpSendFull        = NULL;
    connect.EpCallbacks.EpRecv            = NULL;
    connect.LocalConnectionFlags |= HTC_LOCAL_CONN_FLAGS_ENABLE_SEND_BUNDLE_PADDING;
    connect.MaxSendMsgSize =  1664;
    connect.service_id = WMI_DATA_BE_SVC;
    if ((status = htc_connect_service(htc_handle, &connect, &response))
            != QDF_STATUS_SUCCESS) {
        qdf_info("Failed to connect to Endpoint Ping BE service status:%d \n", status);
        return -1;;
    } else {
        qdf_info("eppingtest BE endpoint:%d\n", response.Endpoint);
    }
    connect.service_id= WMI_DATA_BK_SVC;
    if ((status = htc_connect_service(htc_handle, &connect, &response))
            != EOK) {
        qdf_info("Failed to connect to Endpoint Ping BK service status:%d \n", status);
        return -1;;
    } else {
        qdf_info("eppingtest BK endpoint:%d\n", response.Endpoint);
    }
    connect.service_id = WMI_DATA_VI_SVC;
    if ((status = htc_connect_service(htc_handle, &connect, &response))
            != QDF_STATUS_SUCCESS) {
        qdf_info("Failed to connect to Endpoint Ping VI service status:%d \n", status);
        return -1;;
    } else {
        qdf_info("eppingtest VI endpoint:%d\n", response.Endpoint);
    }
    connect.service_id = WMI_DATA_VO_SVC;
    if ((status = htc_connect_service(htc_handle, &connect, &response))
            != QDF_STATUS_SUCCESS) {
        qdf_info("Failed to connect to Endpoint Ping VO service status:%d \n", status);
        return -1;;
    } else {
        qdf_info("eppingtest VO endpoint:%d\n", response.Endpoint);
    }
    return EOK;
}

int
ol_ath_connect_htc(ol_ath_soc_softc_t *soc)
{
    int status = QDF_STATUS_E_INVAL;
    struct htc_service_connect_req connect;
    struct target_psoc_info *tgt_psoc_info;
    struct common_wmi_handle *wmi_handle;
    struct common_htc_handle *htc_handle;
    struct common_hif_handle *hif_handle;
    tgt_psoc_info = (struct target_psoc_info *)wlan_psoc_get_tgt_if_handle(
						soc->psoc_obj);
    if(tgt_psoc_info == NULL) {
        qdf_info("%s: target_psoc_info is null ", __func__);
        goto conn_fail;
    }

    wmi_handle = target_psoc_get_wmi_hdl(tgt_psoc_info);
    htc_handle = target_psoc_get_htc_hdl(tgt_psoc_info);
    hif_handle = target_psoc_get_hif_hdl(tgt_psoc_info);

    OS_MEMZERO(&connect,sizeof(connect));

    /* meta data is unused for now */
    connect.pMetaData = NULL;
    connect.MetaDataLength = 0;
    /* these fields are the same for all service endpoints */
    connect.EpCallbacks.pContext = soc;
    connect.EpCallbacks.EpTxCompleteMultiple = NULL /* Control path completion ar6000_tx_complete */;
    connect.EpCallbacks.EpRecv = NULL /* Control path rx */;
    connect.EpCallbacks.EpRecvRefill = NULL /* ar6000_rx_refill */;
    connect.EpCallbacks.EpSendFull = NULL /* ar6000_tx_queue_full */;
#if 0
    /* set the max queue depth so that our ar6000_tx_queue_full handler gets called.
     * Linux has the peculiarity of not providing flow control between the
     * NIC and the network stack. There is no API to indicate that a TX packet
     * was sent which could provide some back pressure to the network stack.
     * Under linux you would have to wait till the network stack consumed all sk_buffs
     * before any back-flow kicked in. Which isn't very friendly.
     * So we have to manage this ourselves */
    connect.MaxSendQueueDepth = MAX_DEFAULT_SEND_QUEUE_DEPTH;
    connect.EpCallbacks.RecvRefillWaterMark = AR6000_MAX_RX_BUFFERS / 4; /* set to 25 % */
    if (0 == connect.EpCallbacks.RecvRefillWaterMark) {
        connect.EpCallbacks.RecvRefillWaterMark++;
    }
#endif
#if 0
    /* connect to control service */
    connect.ServiceID = WMI_CONTROL_SVC;
    if ((status = ol_ath_connectservice(soc, &connect, "WMI CONTROL")) != EOK)
        goto conn_fail;
#endif
    if (!bypasswmi) {
        if ((status = wmi_unified_connect_htc_service((wmi_unified_t)wmi_handle,
                            htc_handle)) != QDF_STATUS_SUCCESS)
             goto conn_fail;
    }
#if defined(EPPING_TEST) && !defined(HIF_USB)
    if (eppingtest){
        extern A_STATUS epping_connect_service(ol_ath_soc_softc_t *soc);
        if ((status = epping_connect_service(soc)) != EOK)
             goto conn_fail;
    }
#endif
    if (target_psoc_get_target_ver(tgt_psoc_info) == AR6004_VERSION_REV1_3) {
      if ((status = HIF_USB_connect_service(soc)) != EOK)
               goto conn_fail;
    }
    /*
     * give our connected endpoints some buffers
     */
#if 0
    ar6000_rx_refill(soc, soc->htt_control_ep);
    ar6000_rx_refill(soc, soc->htt_data_ep);
#endif

    /*
     * Since cookies are used for HTC transports, they should be
     * initialized prior to enabling HTC.
     */
    ol_cookie_init((void *)soc);

    /*
     * Start HTC
     */
    if ((status = htc_start(htc_handle)) != QDF_STATUS_SUCCESS) {
        goto conn_fail;
    }

    if (!bypasswmi) {
        /*
         * Wait for WMI event to be ready
         */
        if (target_psoc_get_target_ver(tgt_psoc_info) == AR6004_VERSION_REV1_3) {
            target_psoc_set_wmi_ready(tgt_psoc_info, TRUE);
            target_psoc_set_wlan_init_status(tgt_psoc_info,
                             TARGET_INIT_STATUS_SUCCESS);
        } else {
            if ((status = __ol_ath_check_wmi_ready(soc)) != EOK) {
                goto conn_fail1;
            }
            qdf_info("WMI is ready");

            if(target_psoc_get_wlan_init_status(tgt_psoc_info) !=
                                        TARGET_INIT_STATUS_SUCCESS)
            {
              qdf_info("%s Target wmi init failed with status %d", __func__,
                             target_psoc_get_wlan_init_status(tgt_psoc_info));
              status = ENODEV;
              goto conn_fail1;
            }
        }
        /* Communicate the wmi protocol verision to the target */
        if ((ol_ath_set_host_app_area(soc)) != EOK) {
            qdf_info("Unable to set the host app area\n");
        }
    }

    // TODO is this needed
//            ar6000_target_config_wlan_params(arPriv);
    return EOK;

conn_fail1:
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,5,0)
    if(soc->alloc_task_wqueue != NULL) {
        ATH_FLUSH_WQUEUE(soc->alloc_task_wqueue);
        ATH_DESTROY_WQUEUE(soc->alloc_task_wqueue);
        soc->alloc_task_wqueue = NULL;
    }
#endif

    hif_disable_isr((struct hif_opaque_softc *)hif_handle);
    htc_stop(htc_handle);
conn_fail:
    return status;
}

int
ol_ath_disconnect_htc(ol_ath_soc_softc_t *soc)
{
    struct common_htc_handle *htc_handle;

    htc_handle = lmac_get_htc_hdl(soc->psoc_obj);
    if (htc_handle != NULL) {
        htc_stop(htc_handle);
    }
    return 0;
}

int ol_ath_pdev_set_param(struct ol_ath_softc_net80211 *scn,
                    uint32_t param_id, uint32_t param_value)
{
    struct pdev_params pparam;
    int32_t pdev_idx;
    struct common_wmi_handle *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    pdev_idx = lmac_get_pdev_idx(scn->sc_pdev);
    if(pdev_idx < 0)
	return -1;

    qdf_mem_set(&pparam, sizeof(pparam), 0);
    pparam.param_id = param_id;
    pparam.param_value = param_value;

    return wmi_unified_pdev_param_send(pdev_wmi_handle, &pparam, pdev_idx);
}

/*
 * ol_ath_pdev_dfs_phyerr_offload_en() - Send dfs phyerr offload enable command
 * to Firmware.
 * @scn: Pointer to scn structure.
 *
 * In full-offload mode, after wifi is brought down and brought up,
 * the phyerror offload enable command (phyerr_offload_en_cmd) should
 * be resent since the firmware gets reloaded.
 * The command is to process the dfs pulses in the firmware and to
 * send the radar-found event to the host.
 */
static void ol_ath_pdev_dfs_phyerr_offload_en(struct ol_ath_softc_net80211 *scn)
{
    struct common_wmi_handle *wmi_handle;
    struct common_wmi_handle *pdev_wmi_handle;

    wmi_handle = lmac_get_wmi_hdl(scn->soc->psoc_obj);
    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    if (wmi_service_enabled(wmi_handle, wmi_service_dfs_phyerr_offload))
        wmi_unified_dfs_phyerr_offload_en_cmd(pdev_wmi_handle,
                                                WMI_HOST_PDEV_ID_SOC);
}

static void
ol_ath_update_wireless_modes(
                      struct wlan_psoc_host_hal_reg_capabilities_ext *reg_cap,
                              struct wlan_psoc_host_mac_phy_caps *mac_phy_cap)
{
    uint32_t wireless_2G_modes = WIRELESS_MODES_2G;
    uint32_t wireless_5G_modes = WIRELESS_MODES_5G;

    qdf_info("wireless_modes = %x before update",
                    reg_cap->wireless_modes);
    if (!(mac_phy_cap->supported_bands & WMI_HOST_WLAN_5G_CAPABILITY)) {
        /* mac_phy cap indicates no support for 5G. Make sure 5G specific
         * modes are not present in wireless mode
         */
        reg_cap->wireless_modes &= ~(wireless_5G_modes);
    }
    if (!(mac_phy_cap->supported_bands & WMI_HOST_WLAN_2G_CAPABILITY)) {
        /* mac_phy cap indicates no support for 2G. Make sure 2G specific
         * modes are not present in wireless mode
         */
        reg_cap->wireless_modes &= ~(wireless_2G_modes);
    }

    qdf_info("Wireless_modes = %x after update",
                    reg_cap->wireless_modes);
}

static void
ol_ath_update_target_cap_from_mac_phy_cap(struct wlan_psoc_target_capability_info *cap,
                                struct wlan_psoc_host_mac_phy_caps *mac_phy_cap)
{
    /* If Radio supports both 2G and 5G, By default it will be brought up in 5G.
     * Update to @2G caps during band switch.
     */
    if (mac_phy_cap->supported_bands & WMI_HOST_WLAN_5G_CAPABILITY) {
        cap->ht_cap_info = mac_phy_cap->ht_cap_info_5G;
        cap->vht_cap_info = mac_phy_cap->vht_cap_info_5G;
        cap->vht_supp_mcs = mac_phy_cap->vht_supp_mcs_5G;
    } else if (mac_phy_cap->supported_bands & WMI_HOST_WLAN_2G_CAPABILITY) {
        cap->ht_cap_info = mac_phy_cap->ht_cap_info_2G;
        cap->vht_cap_info = mac_phy_cap->vht_cap_info_2G;
        cap->vht_supp_mcs = mac_phy_cap->vht_supp_mcs_2G;
    } else {
        qdf_info("No 2G OR 5G support set in ext service config");
    }
}

uint32_t num_chain_from_chain_mask(uint32_t mask)
{
    int num_rf_chain = 0;

    while (mask) {
        if (mask & 0x1)
            num_rf_chain++;

        mask >>= 1;
    }

    return num_rf_chain;
}

static inline void ol_ath_populate_chainmask(struct ieee80211com *ic,
        uint32_t tx_chain_mask, uint32_t rx_chain_mask)
{
    ieee80211com_set_num_rx_chain(ic, num_chain_from_chain_mask(rx_chain_mask));
    ieee80211com_set_num_tx_chain(ic, num_chain_from_chain_mask(tx_chain_mask));
    ieee80211com_set_tx_chainmask(ic, tx_chain_mask);
    ieee80211com_set_rx_chainmask(ic, rx_chain_mask);
    /*
     * Note that in the offload architecture chain_masks
     * and spatial_streams are synonymous
     */
    ieee80211com_set_spatialstreams(ic,
                       num_chain_from_chain_mask(rx_chain_mask));
}

static inline void ol_ath_update_chainmask(struct ieee80211com *ic,
        struct wlan_psoc_target_capability_info *cap,
        struct wlan_psoc_host_mac_phy_caps *mac_phy_cap)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    struct target_psoc_info *tgt_psoc_info;

    tgt_psoc_info = (struct target_psoc_info *)wlan_psoc_get_tgt_if_handle(
						scn->soc->psoc_obj);
    if(tgt_psoc_info == NULL) {
        qdf_info("%s: target_psoc_info is null ", __func__);
        return;
    }


    if (cap->ht_cap_info & WMI_HOST_HT_CAP_ENABLED) {
        if (target_psoc_get_num_radios(tgt_psoc_info) == 1) {
            uint32_t chain_mask = ((1 << cap->num_rf_chains) - 1);

            ol_ath_populate_chainmask(ic, chain_mask, chain_mask);
        } else {
            /* 5G is used by default if support for both 5G and 2G is present */
            if (mac_phy_cap->supported_bands & WMI_HOST_WLAN_5G_CAPABILITY) {
                ol_ath_populate_chainmask(ic, mac_phy_cap->tx_chain_mask_5G,
                        mac_phy_cap->rx_chain_mask_5G);
                cap->num_rf_chains = ic->ic_num_rx_chain;
            } else if (mac_phy_cap->supported_bands & WMI_HOST_WLAN_2G_CAPABILITY) {
                ol_ath_populate_chainmask(ic, mac_phy_cap->tx_chain_mask_2G,
                        mac_phy_cap->rx_chain_mask_2G);
                cap->num_rf_chains = ic->ic_num_rx_chain;
            } else {
                qdf_info("No 2G OR 5G support set in ext service config");
            }
        }
    }

#ifdef ATH_SUPPORT_WAPI

    if(target_psoc_get_target_type(tgt_psoc_info) == TARGET_TYPE_AR9888){
    /*WAPI HW engine support upto 300 Mbps (MCS15h),
      limiting the chains to 2*/
      ic->ic_num_wapi_rx_maxchains = 2;
      ic->ic_num_wapi_tx_maxchains = 2;
    } else {
      ic->ic_num_wapi_rx_maxchains = ic->ic_num_rx_chain;
      ic->ic_num_wapi_tx_maxchains = ic->ic_num_tx_chain;
    }
#endif

}

void ol_ath_update_ht_caps(struct ieee80211com *ic, uint32_t ht_cap_info)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    ol_ath_soc_softc_t *soc = scn->soc;
    uint16_t caps  = 0;

    if (ht_cap_info & WMI_HOST_HT_CAP_ENABLED) {
        ieee80211com_set_cap(ic, IEEE80211_C_HT);
        ieee80211com_set_htcap(ic, IEEE80211_HTCAP_C_SHORTGI40
                        | IEEE80211_HTCAP_C_CHWIDTH40
                        | IEEE80211_HTCAP_C_DSSSCCK40);
        if (ht_cap_info & WMI_HOST_HT_CAP_HT20_SGI)  {
            ieee80211com_set_htcap(ic, IEEE80211_HTCAP_C_SHORTGI20);
        }
        if (ht_cap_info & WMI_HOST_HT_CAP_DYNAMIC_SMPS) {
            ieee80211com_set_htcap(ic, IEEE80211_HTCAP_C_SMPOWERSAVE_DYNAMIC);
        } else {
            ieee80211com_set_htcap(ic, IEEE80211_HTCAP_C_SMPOWERSAVE_DISABLED);
        }

        if (ht_cap_info & WMI_HOST_HT_CAP_IBF_BFER) {
            ic->ic_implicitbf = 1 ;
        }

        ieee80211com_set_htextcap(ic, IEEE80211_HTCAP_EXTC_TRANS_TIME_5000
                        | IEEE80211_HTCAP_EXTC_MCS_FEEDBACK_NONE);
        ieee80211com_set_maxampdu(ic, IEEE80211_HTCAP_MAXRXAMPDU_65536);

        /* Force this to 8usec for now, instead of checking min_pkt_size_enable */
        ieee80211com_set_mpdudensity(ic, cfg_get(soc->psoc_obj, CFG_TGT_MPDU_DENSITY));
        ic->ic_mpdudensityoverride = 0;

        IEEE80211_ENABLE_AMPDU(ic);

    }

    /* Tx STBC is a 2-bit mask. Convert to ieee definition. */
    caps = (ht_cap_info & WMI_HOST_HT_CAP_TX_STBC) >> WMI_HOST_HT_CAP_TX_STBC_MASK_SHIFT;
    ieee80211com_set_htcap(ic, IEEE80211_HTCAP_C_TXSTBC & (caps << IEEE80211_HTCAP_C_TXSTBC_S));


    /* Rx STBC is a 2-bit mask. Convert to ieee definition. */
    caps = (ht_cap_info & WMI_HOST_HT_CAP_RX_STBC) >> WMI_HOST_HT_CAP_RX_STBC_MASK_SHIFT;
    ieee80211com_set_htcap(ic, IEEE80211_HTCAP_C_RXSTBC & (caps << IEEE80211_HTCAP_C_RXSTBC_S));

    if (ht_cap_info & WMI_HOST_HT_CAP_LDPC) {
        ieee80211com_set_htcap(ic, IEEE80211_HTCAP_C_ADVCODING);
        if ((lmac_get_tgt_type(soc->psoc_obj) == TARGET_TYPE_AR900B) &&
             (lmac_get_tgt_revision(soc->psoc_obj) == AR900B_REV_1)) {
            /* disable LDPC capability for Beeliner 1.0 */
            ieee80211com_set_ldpccap(ic,IEEE80211_HTCAP_C_LDPC_NONE);
        } else {
            /* enable LDPC capability */
            ieee80211com_set_ldpccap(ic, IEEE80211_HTCAP_C_LDPC_TXRX);
        }
    } else {
        /* check if new method of LDPC flags are provided
         * RX capability should be announced by AP or STA
         * TX capability is based on what peer can receive and
         * what AP can transmit
         */
        if (ht_cap_info & (WMI_HOST_HT_CAP_RX_LDPC | WMI_HOST_HT_CAP_TX_LDPC)) {
            if (ht_cap_info & WMI_HOST_HT_CAP_RX_LDPC) {
                ieee80211com_set_htcap(ic, IEEE80211_HTCAP_C_ADVCODING);
                ieee80211com_set_ldpccap(ic, IEEE80211_HTCAP_C_LDPC_RX);
            }
            if (ht_cap_info & WMI_HOST_HT_CAP_TX_LDPC)
                ieee80211com_set_ldpccap(ic, IEEE80211_HTCAP_C_LDPC_TX);
        } else {
            ieee80211com_set_ldpccap(ic,IEEE80211_HTCAP_C_LDPC_NONE);
        }
    }

}

void ol_ath_update_vht_caps(struct ieee80211com *ic, uint32_t vht_cap_info,
                                                        uint32_t vht_supp_mcs)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    ol_ath_soc_softc_t *soc = scn->soc;
    u_int32_t ampdu_exp = 0;
    struct common_wmi_handle *wmi_handle;
    struct common_wmi_handle *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    wmi_handle = lmac_get_wmi_hdl(soc->psoc_obj);

    if (wmi_service_enabled(wmi_handle, wmi_service_11ac)) {

        /* Copy the VHT capabilities information */
        ieee80211com_set_vhtcap(ic, vht_cap_info);

        /* Adjust HT AMSDU len based on VHT MPDU len */
        if ((vht_cap_info & IEEE80211_VHTCAP_MAX_MPDU_LEN_7935) ||
            (vht_cap_info & IEEE80211_VHTCAP_MAX_MPDU_LEN_11454)) {
            ieee80211com_set_htcap(ic,IEEE80211_HTCAP_C_MAXAMSDUSIZE);
        } else {
            ieee80211com_clear_htcap(ic,IEEE80211_HTCAP_C_MAXAMSDUSIZE);
        }

        /* Adjust HT AMPDU len Exp  based on VHT MPDU len */
        ampdu_exp = vht_cap_info >> IEEE80211_VHTCAP_MAX_AMPDU_LEN_EXP_S;
        switch (ampdu_exp) {
            case 0:
            case 1:
            case 2:
            case 3:
                ieee80211com_set_maxampdu(ic, ampdu_exp);
            break;

            default:
                ieee80211com_set_maxampdu(ic, IEEE80211_HTCAP_MAXRXAMPDU_65536);
            break;
        }


        /* Set the VHT  rate information */
        {
            /*  11ac spec states it is mandatory to support MCS 0-7 and NSS=1 */
            u_int16_t basic_mcs = 0xfffc;
            ol_ath_vht_rate_setup(ic, vht_supp_mcs, 0, basic_mcs );

        }
        ic->ic_vht_ampdu = 64;
        /* The default max amsdu value in FW for Beeliner family is 4
         * and for Peregrine is 3. So init the values accordingly.
         */
        ic->ic_vht_amsdu = cfg_get(soc->psoc_obj, CFG_TGT_VHT_AMSDU);
        if (wmi_service_enabled(pdev_wmi_handle, wmi_service_extended_nss_support)) {
            ic->ic_fw_ext_nss_capable = 1;
            ic->ic_ext_nss_capable = 1;
        }
    }

}

static void
ol_ath_update_caps(struct ieee80211com *ic, struct wlan_psoc_target_capability_info *ev)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    ol_ath_soc_softc_t *soc = scn->soc;
    u_int16_t ciphercap = 0;
    uint32_t target_type;
    uint32_t target_version;
    struct common_wmi_handle *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    target_type = lmac_get_tgt_type(soc->psoc_obj);
    target_version = lmac_get_tgt_version(soc->psoc_obj);

    /* setup ieee80211 flags */
    ieee80211com_clear_cap(ic, -1);
    ieee80211com_clear_athcap(ic, -1);
    ieee80211com_clear_athextcap(ic, -1);
    ieee80211com_clear_ciphercap(ic, -1);
    ieee80211com_set_phytype(ic, IEEE80211_T_OFDM);
    ieee80211com_set_cap(ic,IEEE80211_C_SHPREAMBLE);

    ieee80211com_set_cap(ic,
                     IEEE80211_C_IBSS           /* ibss, nee adhoc, mode */
                     | IEEE80211_C_HOSTAP       /* hostap mode */
                     | IEEE80211_C_MONITOR      /* monitor mode */
                     | IEEE80211_C_SHSLOT       /* short slot time supported */
                     | IEEE80211_C_PMGT         /* capable of power management*/
                     | IEEE80211_C_WPA          /* capable of WPA1+WPA2 */
                     | IEEE80211_C_BGSCAN       /* capable of bg scanning */
                  );

    /* Setting cipher cap */
    ciphercap = ( (1 << IEEE80211_CIPHER_WEP) |
               (1 << IEEE80211_CIPHER_TKIP) |
               (1 << IEEE80211_CIPHER_AES_OCB) |
               (1 << IEEE80211_CIPHER_AES_CCM) |
               (1 << IEEE80211_CIPHER_WAPI) |
               (1 << IEEE80211_CIPHER_CKIP) |
               (1 << IEEE80211_CIPHER_AES_CMAC) |
               (1 << IEEE80211_CIPHER_AES_CCM_256) |
               (1 << IEEE80211_CIPHER_AES_GCM) |
               (1 << IEEE80211_CIPHER_AES_GCM_256) |
               (1 << IEEE80211_CIPHER_AES_GMAC) |
               (1 << IEEE80211_CIPHER_AES_GMAC_256) |
               (1 << IEEE80211_CIPHER_NONE)
             );
    /*
     * AR9888 Family of Chips do not support GCMP and CCMP-256
     */
    if ( (target_type == TARGET_TYPE_AR9888) ||
         (target_type == TARGET_TYPE_AR9888V2) )
    {
        ciphercap &= (~((1 << IEEE80211_CIPHER_AES_CCM_256) |
                        (1 << IEEE80211_CIPHER_AES_GCM) |
                        (1 << IEEE80211_CIPHER_AES_GCM_256) |
                        (1 << IEEE80211_CIPHER_AES_GMAC) |
                        (1 << IEEE80211_CIPHER_AES_GMAC_256)) & 0xffff);
    }

    ieee80211com_set_ciphercap(ic, ciphercap);
    /* WMM enable */
    ieee80211com_set_cap(ic, IEEE80211_C_WME);

    scn->burst_enable = wmi_service_enabled(pdev_wmi_handle,
                                    wmi_service_burst);

    if (wmi_service_enabled(pdev_wmi_handle, wmi_service_ap_uapsd)) {
        ieee80211com_set_cap(ic, IEEE80211_C_UAPSD);
        IEEE80211_UAPSD_ENABLE(ic);
    }

    /* Default 11h to start enabled  */
    ieee80211_ic_doth_set(ic);
#if UMAC_SUPPORT_WNM
    /* Default WNM enabled   */
    ieee80211_ic_wnm_set(ic);
    /* WNM needs PS state of STA, so enable it in the FW */
    (void)ol_ath_pdev_set_param(scn,
                    wmi_pdev_peer_sta_ps_statechg_enable, 1);
    scn->ps_report = 1;
#endif

    /* 11n Capabilities */
    ieee80211com_set_num_tx_chain(ic,1);
    ieee80211com_set_num_rx_chain(ic,1);
    ieee80211com_clear_htcap(ic, -1);
    ieee80211com_clear_htextcap(ic, -1);
    ol_ath_update_ht_caps(ic, ev->ht_cap_info);

    /* 11n configuration */
    ieee80211com_clear_htflags(ic, -1);

    /*
     * Indicate we need the 802.11 header padded to a
     * 32-bit boundary for 4-address and QoS frames.
     */
    IEEE80211_ENABLE_DATAPAD(ic);

    /* Check whether the hardware is VHT capable */
    ieee80211com_clear_vhtcap(ic, -1);
    ol_ath_update_vht_caps(ic, ev->vht_cap_info, ev->vht_supp_mcs);

    if (cfg_get(soc->psoc_obj, CFG_OL_ENABLE_MESH_SUPPORT) &&
                wmi_service_enabled(pdev_wmi_handle, wmi_service_mesh)) {
        qdf_info("Mesh Supported \n");
        ic->ic_mesh_vap_support = 1;
    }

    /* this should be updated from service bit map. This change is added temporarily untill firmware support is added*/
    ic->ic_promisc_support = 1;
    ic->ic_tso_support = cdp_get_dp_capabilities(wlan_psoc_get_dp_handle(soc->psoc_obj), CDP_CFG_DP_TSO);
    ic->ic_lro_support = cdp_get_dp_capabilities(wlan_psoc_get_dp_handle(soc->psoc_obj), CDP_CFG_DP_LRO);
    ic->ic_sg_support = cdp_get_dp_capabilities(wlan_psoc_get_dp_handle(soc->psoc_obj), CDP_CFG_DP_SG);
    ic->ic_gro_support = cdp_get_dp_capabilities(wlan_psoc_get_dp_handle(soc->psoc_obj), CDP_CFG_DP_GRO);
    ic->ic_offload_tx_csum_support = cdp_get_dp_capabilities(wlan_psoc_get_dp_handle(soc->psoc_obj), CDP_CFG_DP_OL_TX_CSUM);
    ic->ic_offload_rx_csum_support = cdp_get_dp_capabilities(wlan_psoc_get_dp_handle(soc->psoc_obj), CDP_CFG_DP_OL_RX_CSUM);
    ic->ic_rawmode_support = cdp_get_dp_capabilities(wlan_psoc_get_dp_handle(soc->psoc_obj), CDP_CFG_DP_RAWMODE);
    ic->ic_peer_flow_control_support = cdp_get_dp_capabilities(wlan_psoc_get_dp_handle(soc->psoc_obj), CDP_CFG_DP_PEER_FLOW_CTRL);
    ic->ic_dynamic_grouping_support = cfg_get(soc->psoc_obj, CFG_TGT_DYN_GROUPING_SUPPORT);
    ic->ic_dpd_support = cfg_get(soc->psoc_obj, CFG_TGT_DPD_SUPPORT);
    ic->ic_aggr_burst_support = cfg_get(soc->psoc_obj, CFG_TGT_AGGR_BURST_SUPPORT);
    ic->ic_qboost_support = cfg_get(soc->psoc_obj, CFG_TGT_QBOOST_SUPPORT);
    ic->ic_sifs_frame_support = cfg_get(soc->psoc_obj, CFG_TGT_SIFS_FRAME_SUPPORT);
    ic->ic_block_interbss_support = cfg_get(soc->psoc_obj, CFG_TGT_BLK_INTERBSS_SUPPORT);
    ic->ic_disable_reset_support = cfg_get(soc->psoc_obj, CFG_TGT_DIS_RESET_SUPPORT);
    ic->ic_msdu_ttl_support = cfg_get(soc->psoc_obj, CFG_TGT_MSDU_TTL_SUPPORT);
    ic->ic_ppdu_duration_support = cfg_get(soc->psoc_obj, CFG_TGT_PPDU_DUR_SUPPORT);
    ic->ic_burst_mode_support = cfg_get(soc->psoc_obj, CFG_TGT_BURST_MODE_SUPPORT);
    ic->ic_wds_support = cfg_get(soc->psoc_obj, CFG_TGT_WDS_SUPPORT);
    ic->ic_def_num_clients = cfg_get(soc->psoc_obj, CFG_TGT_NUM_CLIENT);
    ic->ic_nac_client = cfg_get(soc->psoc_obj, CFG_TGT_NAC_MAX_CLIENT);
    ic->ic_nac_bssid = cfg_get(soc->psoc_obj, CFG_TGT_NAC_MAX_BSSID);

    /* Set smart monitor HW capability for HKv2 */
    if (target_type == TARGET_TYPE_QCA8074V2 ||
            target_type == TARGET_TYPE_QCA6018)
        ic->ic_hw_nac_monitor_support = 1;

    /* Set no bfee limit for all lithium targets and above.
     * Legacy targets will enable bfee based on total rx
     * streams and will not enable bfee by default.
     */
    ic->ic_no_bfee_limit = cfg_get(soc->psoc_obj, CFG_TGT_NO_BFEE_LIMIT_SUPPORT);
    ic->ic_he_target     = cfg_get(soc->psoc_obj, CFG_TGT_HE_TGT_SUPPORT);

    if (target_type >= TARGET_TYPE_QCA8074) {
        ic->ic_unified_stats = 1;
        /* Set flag for ppdu_duration and burst_dur
         * HW requirement for Lithium based targets to support ppdu_duration
         * and burst_dur values greater than or equal to zero.
         */
        ic->ic_ppdu_min = 0;
        ic->ic_burst_min = 0;
    } else {
        ic->ic_ppdu_min = 100;
        ic->ic_burst_min = 1;
    }

    /* BA Buf Size will be initialized to 256 for HKv1/2, qca6018
     * and initialized to 64 for all other targets.
     */
    if (target_type ==  TARGET_TYPE_QCA8074 ||
            target_type ==  TARGET_TYPE_QCA8074V2 ||
            target_type ==  TARGET_TYPE_QCA6018) {
        ic->ic_he_default_ba_buf_size = IEEE80211_MAX_BA_BUFFER_SIZE;
    } else {
        ic->ic_he_default_ba_buf_size = IEEE80211_MIN_BA_BUFFER_SIZE;
    }

    /* The FW default for the UL PPDU Duration is 2000 usecs. */
    ic->ic_he_ul_ppdu_dur = 2000;

#if 0 // ANISH DISABLED THIS ON BEELINER
    if (wmi_service_enabled(scn->wmi_handle, wmi_service_ratectrl)) {
        ol_txrx_enable_host_ratectrl(
                (OL_ATH_SOFTC_NET80211(ic))->pdev_txrx_handle, 1);
    }
#endif

    /* ToDo, check ev->sys_cap_info for  WMI_SYS_CAP_ENABLE and WMI_SYS_CAP_TXPOWER when it is available from FW */
    ieee80211com_set_cap(ic, IEEE80211_C_TXPMGT);

    ieee80211_wme_initglobalparams(ic);
    /* Initialize MU EDCA parameters with defaults for the ic */
    ieee80211_muedca_initglobalparams(ic);
}

static void dbg_print_wmi_service_11ac(ol_ath_soc_softc_t *soc,
                                                struct wlan_psoc_target_capability_info  *ev)
{
    struct common_wmi_handle *wmi_handle;

    wmi_handle = lmac_get_wmi_hdl(soc->psoc_obj);

    if (wmi_service_enabled(wmi_handle, wmi_service_11ac)) {
	qdf_nofl_debug("num_rf_chain:0x%08x  ", ev->num_rf_chains);
	qdf_nofl_debug("ht_cap_info:0x%08x  ", ev->ht_cap_info);
	qdf_nofl_debug("vht_cap_info:0x%08x  ", ev->vht_cap_info);
	qdf_nofl_debug("vht_supp_mcs:0x%08x", ev->vht_supp_mcs);
    }
    else {
	qdf_nofl_debug("\n No WMI 11AC service event received");
    }
}

static void
ol_ath_update_ic_hecap_mcsnssmap_from_target_hecap_mcsnssmap(
                                    struct ieee80211_he_handle *ic_he,
                                    uint32_t target_hecap_mcsnssmap)
{
    /* Target sends HE MCS-NSS info for less than equal to 80MHz encoded
     * in the lower 16 bits */
    ic_he->hecap_rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80]    =
                    HECAP_TXRX_MCS_NSS_GET_LT80_INFO(target_hecap_mcsnssmap);
    /* Target sends HE MCS-NSS info for greater than 80MHz encoded in the
     * upper 16 bits */
    ic_he->hecap_rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_160]   =
                    HECAP_TXRX_MCS_NSS_GET_GT80_INFO(target_hecap_mcsnssmap);
    /* Target sends HE MCS-NSS info for greater than 80MHz encoded in the
     * upper 16 bits */
    ic_he->hecap_rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80_80] =
                    HECAP_TXRX_MCS_NSS_GET_GT80_INFO(target_hecap_mcsnssmap);
    /* Target is sending only rxmcsnss values. For the time being we consider
     * txmcsnss values to be same as those or rxmcsnss
     */
    ic_he->hecap_txmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80]    =
                    HECAP_TXRX_MCS_NSS_GET_LT80_INFO(target_hecap_mcsnssmap);
    ic_he->hecap_txmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_160]   =
                    HECAP_TXRX_MCS_NSS_GET_GT80_INFO(target_hecap_mcsnssmap);
    ic_he->hecap_txmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80_80] =
                    HECAP_TXRX_MCS_NSS_GET_GT80_INFO(target_hecap_mcsnssmap);
}

static inline void ol_ath_populate_he_caps(struct ieee80211_he_handle *ic_he,
        uint32_t *he_cap_info, uint32_t *he_supp_mcs,
        uint32_t *he_cap_phy_info, uint8_t he_cap_phy_info_size,
        struct wlan_psoc_host_ppe_threshold *he_ppet,
        uint32_t *he_cap_info_internal)
{
    qdf_mem_copy(&ic_he->hecap_macinfo, he_cap_info,
            sizeof(ic_he->hecap_macinfo));
    qdf_mem_copy(ic_he->hecap_phyinfo, he_cap_phy_info,
            he_cap_phy_info_size);
    qdf_mem_copy(&ic_he->hecap_info_internal, he_cap_info_internal,
            sizeof(ic_he->hecap_info_internal));
    ol_ath_update_ic_hecap_mcsnssmap_from_target_hecap_mcsnssmap(
            ic_he, *he_supp_mcs);
    qdf_mem_copy(&ic_he->hecap_ppet, he_ppet,
            sizeof(*he_ppet));
}

static void
ol_ath_update_ext_caps(struct wlan_psoc_host_mac_phy_caps *mac_phy_cap, struct ieee80211com *ic)
{
    struct ieee80211_he_handle *ic_he;

    if(!mac_phy_cap || !ic) {
        qdf_info("%s mac_phy_cap or ic is Null ",__func__);
        return;
    }

    ic_he = &ic->ic_he;

    /* Populate 11ax caps to IC */
    if(mac_phy_cap->supports_11ax) {

        if (mac_phy_cap->supported_bands == WMI_HOST_WLAN_5G_CAPABILITY) {

            qdf_info("11ax 5G supported case");
            ol_ath_populate_he_caps(ic_he, mac_phy_cap->he_cap_info_5G,
                    &mac_phy_cap->he_supp_mcs_5G,
                    mac_phy_cap->he_cap_phy_info_5G, sizeof(mac_phy_cap->he_cap_phy_info_5G),
                    &mac_phy_cap->he_ppet5G, &mac_phy_cap->he_cap_info_internal);

        } else if (mac_phy_cap->supported_bands == WMI_HOST_WLAN_2G_CAPABILITY) {

            qdf_info("11ax 2G supported case");
            ol_ath_populate_he_caps(ic_he, mac_phy_cap->he_cap_info_2G,
                    &mac_phy_cap->he_supp_mcs_2G,
                    mac_phy_cap->he_cap_phy_info_2G, sizeof(mac_phy_cap->he_cap_phy_info_2G),
                    &mac_phy_cap->he_ppet2G, &mac_phy_cap->he_cap_info_internal);

        } else {
            qdf_info("Both bands supported case %x, default to populate \
               5G HE caps", mac_phy_cap->supported_bands);

            ol_ath_populate_he_caps(ic_he, mac_phy_cap->he_cap_info_5G,
                    &mac_phy_cap->he_supp_mcs_5G,
                    mac_phy_cap->he_cap_phy_info_5G, sizeof(mac_phy_cap->he_cap_phy_info_5G),
                    &mac_phy_cap->he_ppet5G, &mac_phy_cap->he_cap_info_internal);
        }
    } else {

         qdf_info("11AX not supported ");
    }
}

static uint32_t wlan_pdev_get_num_peer_qca8074(struct target_psoc_info *tgt_psoc,
                                               target_resource_config *tgt_cfg,
                                               uint32_t phy_id)
{
    uint8_t num_radios = 0;
    num_radios = target_psoc_get_num_radios(tgt_psoc);

    switch (num_radios) {
        case 2:
            if (tgt_cfg->num_peers != CFG_TGT_NUM_PEERS_QCA8074) {
               /* Max peers reduced due to FW constraints.
                * Return per pdev peer by diving it equally for all pdev */
               return (tgt_cfg->num_peers/num_radios);
            }
            break;
        case 3:
            if (tgt_cfg->num_peers != CFG_TGT_NUM_PEERS_QCA8074_3M) {
               /* Max peers reduced due to FW constraints.
                * Return per pdev peer by diving it equally for all pdev */
               return (tgt_cfg->num_peers/num_radios);
            }
            break;
        case 1:
        default:
            return tgt_cfg->num_peers;
    }

    switch(phy_id) {
        case WMI_HOST_PDEV_ID_0:
                return CFG_TGT_NUM_PEERS_QCA8074_PDEV0;
        case WMI_HOST_PDEV_ID_1:
                return CFG_TGT_NUM_PEERS_QCA8074_PDEV1;
        case WMI_HOST_PDEV_ID_2:
                return CFG_TGT_NUM_PEERS_QCA8074_PDEV2;
        default:
            return 0;
    }

    return 0;
}

static uint32_t wlan_pdev_get_num_peer_qca6290(struct target_psoc_info *tgt_psoc,
                                               target_resource_config *tgt_cfg,
                                               uint32_t phy_id)
{
    uint8_t num_radios = 0;
    num_radios = target_psoc_get_num_radios(tgt_psoc);

    if ((num_radios) && (tgt_cfg->num_peers != CFG_TGT_NUM_PEERS_QCA6290)) {
        /* Max peers reduced due to FW constraints.
         * Return per pdev peer by diving it equally for all pdev */
        return (tgt_cfg->num_peers/num_radios);
    }

    switch(phy_id) {
        case WMI_HOST_PDEV_ID_0:
                return CFG_TGT_NUM_PEERS_QCA6290_PDEV0;
        case WMI_HOST_PDEV_ID_1:
                return CFG_TGT_NUM_PEERS_QCA6290_PDEV1;
        default:
            return 0;
    }

    return 0;
}

static uint32_t wlan_pdev_get_num_peer(ol_ath_soc_softc_t *soc, uint32_t phy_id)
{
    target_resource_config *tgt_cfg;
    struct target_psoc_info *tgt_psoc_info;
    uint32_t target_type;

    target_type = lmac_get_tgt_type(soc->psoc_obj);

    tgt_psoc_info = (struct target_psoc_info *)wlan_psoc_get_tgt_if_handle(
                                               soc->psoc_obj);
    if(tgt_psoc_info == NULL) {
        qdf_err("%s: target_psoc_info is null", __func__);
        return 0;
    }

    tgt_cfg = target_psoc_get_wlan_res_cfg(tgt_psoc_info);
    if(!tgt_cfg) {
        qdf_err("%s: psoc target res cfg is null", __func__);
        return 0;
    }

    if (target_type == TARGET_TYPE_QCA6290)
        return wlan_pdev_get_num_peer_qca6290(tgt_psoc_info, tgt_cfg, phy_id);
    else if ((target_type == TARGET_TYPE_QCA8074) ||
             (target_type == TARGET_TYPE_QCA8074V2) ||
             (target_type == TARGET_TYPE_QCA6018))
        return wlan_pdev_get_num_peer_qca8074(tgt_psoc_info, tgt_cfg, phy_id);

    /* No change in num_peers for other targets */
    return tgt_cfg->num_peers;
}

uint32_t
ol_ath_get_host_pltfrm_mode(ol_ath_soc_softc_t *soc)
{
	uint32_t target_type;

	target_type = lmac_get_tgt_type(soc->psoc_obj);

	switch(target_type)
	{
	case TARGET_TYPE_AR6002:
	case TARGET_TYPE_AR6003:
	case TARGET_TYPE_AR6004:
	case TARGET_TYPE_AR6006:
	case TARGET_TYPE_AR9888:
	case TARGET_TYPE_AR6320:
		return HOST_PLATFORM_LOW_PERF;
#if PEER_FLOW_CONTROL_FORCED_MODE0
	case TARGET_TYPE_AR900B:
	case TARGET_TYPE_QCA9984:
	case TARGET_TYPE_QCA9888:
		return HOST_PLATFORM_LOW_PERF_NO_FETCH;
#elif MIPS_LOW_PERF_SUPPORT
	case TARGET_TYPE_AR900B:
	case TARGET_TYPE_QCA9984:
	case TARGET_TYPE_QCA9888:
		return HOST_PLATFORM_LOW_PERF;
#else
	case TARGET_TYPE_AR900B:
	case TARGET_TYPE_QCA9984:
	case TARGET_TYPE_QCA9888:
		return HOST_PLATFORM_HIGH_PERF;
#endif
	case TARGET_TYPE_IPQ4019:
		return HOST_PLATFORM_HIGH_PERF;
	default:
		qdf_info("!!! Invalid Target Type %d !!!", target_type);
		return -EINVAL;
	}
	return EOK;
}

int
ol_ath_service_ready_event(ol_scn_t sc, uint8_t *evt_buf, uint32_t len)
{
    ol_ath_soc_softc_t *soc = (ol_ath_soc_softc_t *) sc;
    struct target_psoc_info *tgt_hdl;
    struct wlan_objmgr_psoc *psoc = soc->psoc_obj;
    struct ol_ath_target_cap target_cap;

    QDF_STATUS status = wlan_objmgr_psoc_try_get_ref(psoc, WLAN_MLME_SB_ID);
    if (QDF_IS_STATUS_ERROR(status)) {
        qdf_info("%s, %d unable to get psoc reference", __func__, __LINE__);
        return -1;
    }
    tgt_hdl = (struct target_psoc_info *)wlan_psoc_get_tgt_if_handle(psoc);
    if(!tgt_hdl) {
	target_if_err("psoc target_psoc_info is null in service ready ev");
        return -EINVAL;
    }

    target_cap.wlan_resource_config = tgt_hdl->info.wlan_res_cfg;

    /* call back into  os shim with the services bitmap and resource config to let
     * the os shim layer modify it according to its needs and requirements */
    if (soc->cfg_cb) {
        soc->cfg_cb(soc, &target_cap);
        tgt_hdl->info.wlan_res_cfg = target_cap.wlan_resource_config;
    }

    /* Dump service ready event for debugging */
    dbg_print_wmi_service_11ac(soc, &tgt_hdl->info.target_caps);

    wlan_objmgr_psoc_release_ref(psoc, WLAN_MLME_SB_ID);

    return 0;
}


/**
 * ol_ath_unified_event_handler() - Global master wmi service handler
 * for both ext and ext service ready events
 * @event_id: event_id
 * @handle: wma handle
 * @event_data: event data
 * @length: event length
 *
 * Return: 0 for success, negative error code for failure
 */
int
ol_ath_unified_event_handler(uint32_t event_id,
                             void *scn_handle,
                             uint8_t *event_data,
                             uint32_t length)
{

    if (wmi_service_ready_event_id == event_id) {
        return ol_ath_service_ready_event(scn_handle, event_data, length);
    }

    return 0;
}

/**
 * register_legacy_wmi_service_ready_callback(void) - register wmi legacy callback
 *
 * Legacy wmi Callback registered globally to target_if ctx
 * for all offload based architectures
 *
 * Return: Success
 */
QDF_STATUS register_legacy_wmi_service_ready_callback(void)
{
    target_if_register_legacy_service_ready_cb(ol_ath_unified_event_handler);

    return QDF_STATUS_SUCCESS;
}
qdf_export_symbol(register_legacy_wmi_service_ready_callback);

/*
 *  WMI API for setting fw hang.
 *  type parameter can be one of the 6 values defined in
 *  wmi_unified.h enumerated as RECOVERY_SIM_TYPE
 */
int
ol_ath_set_fw_hang(struct ol_ath_softc_net80211 *scn, u_int32_t delay_time_ms)
{
    struct crash_inject param;
    struct common_wmi_handle *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    qdf_mem_set(&param, sizeof(param), 0);
    param.delay_time_ms = delay_time_ms;
    param.type = 1; //RECOVERY_SIM_ASSERT

    return wmi_crash_inject(pdev_wmi_handle, &param);
}

/*
 *  WMI API for FIPS
 */
int
ol_ath_pdev_fips(struct ol_ath_softc_net80211 *scn,
                      u_int8_t *key,
                      u_int32_t key_len,
                      u_int8_t *data,
                      u_int32_t data_len,
                      u_int32_t mode,
                      u_int32_t op,
                      u_int32_t pdev_id)
{
    int retval = 0;
    struct fips_params param;
    struct common_wmi_handle *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    qdf_mem_set(&param, sizeof(param), 0);
    param.key_len = key_len;
    param.data_len = data_len;
    param.op = op;
    param.key = key;
    param.data = data;
    param.mode = mode;
    param.pdev_id = pdev_id;

    retval = wmi_unified_pdev_fips_cmd_send(pdev_wmi_handle, &param);
    return retval;
}

static int
ol_ath_fips_event_handler(ol_soc_t sc, u_int8_t *evt_buf, u_int32_t datalen)
{
    ol_ath_soc_softc_t *soc = (ol_ath_soc_softc_t *) sc;
    struct ieee80211com *ic;
    struct wmi_host_fips_event_param fips_ev= {0};
    u_int32_t output_len;
    struct common_wmi_handle *wmi_handle;
    struct wlan_objmgr_pdev *pdev;

    wmi_handle = lmac_get_wmi_hdl(soc->psoc_obj);

    if (wmi_extract_fips_event_data(wmi_handle, evt_buf, &fips_ev) !=
                                                           QDF_STATUS_SUCCESS) {
            qdf_info("Unable to extract FIPS event");
            return -1;
    }

    pdev = wlan_objmgr_get_pdev_by_id(soc->psoc_obj, PDEV_UNIT(fips_ev.pdev_id),
                                   WLAN_MLME_SB_ID);
    if (pdev == NULL) {
         qdf_info("%s: pdev object (id: %d) is NULL ",
                             __func__, PDEV_UNIT(fips_ev.pdev_id));
         return -1;
    }

    ic = wlan_pdev_get_mlme_ext_obj(pdev);
    if (ic == NULL) {
        qdf_info("ic (id: %d) is NULL ", PDEV_UNIT(fips_ev.pdev_id));
        wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
        return -1;
    }
    /* Set this flag to notify fips_event had occured */
    qdf_atomic_inc(&(ic->ic_fips_event));

    output_len = sizeof(struct ath_fips_output) + fips_ev.data_len;

    /* To pass the output data to application */
    ic->ic_output_fips = (struct ath_fips_output *) OS_MALLOC(ic->ic_osdev, output_len, GFP_KERNEL);
    if (ic->ic_output_fips == NULL) {
	qdf_info("Invalid ic_output_fips");
        wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
        return -1;
    }

    qdf_info("%s ic->ic_output_fips %pK\n", __func__, ic->ic_output_fips);
    ic->ic_output_fips->error_status = fips_ev.error_status;
    ic->ic_output_fips->data_len = fips_ev.data_len;
    print_hex_dump(KERN_DEBUG, "\t Handler Data: ", DUMP_PREFIX_NONE, 16, 1,
                                          fips_ev.data, fips_ev.data_len, true);
    OS_MEMCPY(ic->ic_output_fips->data, fips_ev.data, fips_ev.data_len);
    qdf_info("%s error_status %x data_len %x\n",
            __func__, ic->ic_output_fips->error_status, fips_ev.data_len);
    print_hex_dump(KERN_DEBUG, "Cipher text: ", DUMP_PREFIX_NONE, 16, 1,
               fips_ev.data, fips_ev.data_len, true);
    wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
    return 0;
}

/* This API is not called. We should probably deprecate and remove this*/
int
ol_ath_pdev_set_channel(struct ol_ath_softc_net80211 *scn,
                           struct ieee80211_ath_channel *chan, u_int32_t freq,
                           bool is_2gvht_en)
{
    struct channel_param param;
    u_int32_t chan_mode;
    struct common_wmi_handle *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    qdf_mem_set(&param, sizeof(param), 0);
    param.mhz = freq;

    chan_mode = ieee80211_chan2mode(chan);
    param.phy_mode = ol_get_phymode_info(scn, chan_mode, is_2gvht_en);
    param.cfreq1 = 0;
    param.cfreq2 = 0;
    if ((chan_mode == IEEE80211_MODE_11AC_VHT80) ||
            (chan_mode == IEEE80211_MODE_11AC_VHT160) ||
            (chan_mode == IEEE80211_MODE_11AC_VHT80_80) ||
            (chan_mode == IEEE80211_MODE_11AXA_HE80) ||
            (chan_mode == IEEE80211_MODE_11AXA_HE160) ||
            (chan_mode == IEEE80211_MODE_11AXA_HE80_80)) {
            if (chan->ic_ieee < 20)
                param.cfreq1 = ieee80211_ieee2mhz(&scn->sc_ic,
                                                          chan->ic_vhtop_ch_freq_seg1, IEEE80211_CHAN_2GHZ);
            else
                param.cfreq1 = ieee80211_ieee2mhz(&scn->sc_ic,
                                                          chan->ic_vhtop_ch_freq_seg1, IEEE80211_CHAN_5GHZ);
        if ((chan_mode == IEEE80211_MODE_11AC_VHT80_80) ||
                (chan_mode == IEEE80211_MODE_11AC_VHT160) ||
                (chan_mode == IEEE80211_MODE_11AXA_HE80_80) ||
                (chan_mode == IEEE80211_MODE_11AXA_HE160))
            param.cfreq2 = ieee80211_ieee2mhz(&scn->sc_ic,
                    chan->ic_vhtop_ch_freq_seg2, IEEE80211_CHAN_5GHZ);

    } else if ((chan_mode == IEEE80211_MODE_11NA_HT40PLUS) ||
                    (chan_mode == IEEE80211_MODE_11NG_HT40PLUS) ||
                    (chan_mode == IEEE80211_MODE_11AC_VHT40PLUS) ||
                    (chan_mode == IEEE80211_MODE_11AXA_HE40PLUS) ||
                    (chan_mode == IEEE80211_MODE_11AXG_HE40PLUS)) {
            param.cfreq1 = freq + 10;
    } else if ((chan_mode == IEEE80211_MODE_11NA_HT40MINUS) ||
                    (chan_mode == IEEE80211_MODE_11NG_HT40MINUS) ||
                    (chan_mode == IEEE80211_MODE_11AC_VHT40MINUS) ||
                    (chan_mode == IEEE80211_MODE_11AXA_HE40MINUS) ||
                    (chan_mode == IEEE80211_MODE_11AXG_HE40MINUS)) {
            param.cfreq1 = freq - 10;
    } else {
            param.cfreq1 = freq;
    }

    if (IEEE80211_IS_CHAN_DFS(chan))
        param.dfs_set = TRUE;

    if ((chan_mode == IEEE80211_MODE_11AC_VHT80_80) ||
            (chan_mode == IEEE80211_MODE_11AXA_HE80_80)) {
        if (IEEE80211_IS_CHAN_DFS_CFREQ2(chan)) {
            param.dfs_set_cfreq2 = TRUE;
        }
    }

    if (IEEE80211_IS_CHAN_HALF(chan))
        param.half_rate = TRUE;
    if (IEEE80211_IS_CHAN_QUARTER(chan))
        param.quarter_rate = TRUE;

    param.minpower = chan->ic_minpower;
    param.maxpower = chan->ic_maxpower;
    param.maxregpower = chan->ic_maxregpower;
    param.antennamax = chan->ic_antennamax;
    param.reg_class_id = chan->ic_regClassId;

    return wmi_unified_set_chan_cmd_send(pdev_wmi_handle, &param);
}

int
ol_ath_pdev_set_ht_ie(struct ol_ath_softc_net80211 *scn, u_int32_t ie_len, u_int8_t *ie_data)
{
    struct ht_ie_params param;
    struct common_wmi_handle *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    qdf_mem_set(&param, sizeof(param), 0);
    param.ie_len = ie_len;
    param.ie_data = ie_data;
    return wmi_unified_set_ht_ie_cmd_send(pdev_wmi_handle, &param);
}

int
ol_ath_pdev_set_vht_ie(struct ol_ath_softc_net80211 *scn, u_int32_t ie_len, u_int8_t *ie_data)
{
    struct vht_ie_params param;
    struct common_wmi_handle *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    qdf_mem_set(&param, sizeof(param), 0);
    param.ie_len = ie_len;
    param.ie_data = ie_data;
    return wmi_unified_set_vht_ie_cmd_send(pdev_wmi_handle, &param);
}

#define MAX_IE_SIZE 512

#define MAX_HT_IE_LEN 32
#define MAX_VHT_IE_LEN 32
void ol_ath_set_ht_vht_ies(struct ieee80211_node *ni)
{
    struct ieee80211com *ic = NULL;
    struct ieee80211vap *vap = NULL;
    struct ol_ath_softc_net80211 *scn = NULL;

    if (ni == NULL) {
        QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO, "%s: Node is NULL\n", __func__);
        return;
    }

    ic = ni->ni_ic;
    vap = ni->ni_vap;
    scn = OL_ATH_SOFTC_NET80211(ic);

    if (ic == NULL || vap == NULL || scn == NULL) {
        QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO, "%s: IC, VAP or SCN is NULL\n", __func__);
        return;
    }

    if (!scn->set_ht_vht_ies) {
        u_int8_t *buf=qdf_mem_malloc(MAX_IE_SIZE);
        u_int8_t *buf_end;
        if (buf) {
            buf_end = ieee80211_add_htcap(buf, vap->iv_bss,IEEE80211_FC0_SUBTYPE_PROBE_REQ);
            if ((buf_end - buf ) <= MAX_HT_IE_LEN ) {
                ol_ath_pdev_set_ht_ie(scn,buf_end-buf, buf);
            } else {
                qdf_info("%s: HT IE length %d is more than expected",__func__,
                                    (int) (buf_end-buf));
            }
            buf_end = ieee80211_add_vhtcap(buf, vap->iv_bss,ic,
                    IEEE80211_FC0_SUBTYPE_PROBE_REQ, NULL, NULL);
            if ((buf_end - buf ) <= MAX_VHT_IE_LEN ) {
                ol_ath_pdev_set_vht_ie(scn,buf_end-buf,buf);
            } else {
                qdf_info("%s: VHT IE length %d is more than expected",__func__,
                                    (int) (buf_end-buf));
            }
            scn->set_ht_vht_ies = 1;
            qdf_mem_free(buf);
        }
    }
}

struct ieee80211_ath_channel *
ol_ath_find_full_channel(struct ieee80211com *ic, u_int32_t freq)
{
   return wlan_find_full_channel(ic, freq);
}

/* Offload Interface functions for UMAC */
static int
ol_ath_init(struct ieee80211com *ic)
{
    /* TBD */
    return 0;
}

static int
ol_ath_reset_start(struct ieee80211com *ic, bool no_flush)
{
    /* TBD */
    return 0;
}

static int
ol_ath_reset_end(struct ieee80211com *ic, bool no_flush)
{
    /* TBD */
    return 0;
}

static void
ol_set_beacon_interval(struct ieee80211com *ic)
{
    /* TBD */
    return;
}

static int
ol_ath_reset(struct ieee80211com *ic)
{
    /* TBD */
    return 0;
}

/* struct mgmt_tx_power_param - Structure param used to iterate vaps to set
* beacon tx power.
*/
struct mgmt_tx_power_param {
    uint32_t tx_power;
    struct ol_ath_softc_net80211 *scn;
    uint32_t status;
};

int
wmi_txpower_vap_beacon(struct ol_ath_softc_net80211 *scn,u_int8_t tx_power, struct ieee80211vap *vap)
{
    int retval = EOK;
    struct ol_ath_vap_net80211 *avn;

    if (vap != NULL) {
        avn = OL_ATH_VAP_NET80211(vap);
        retval = ol_ath_wmi_send_vdev_param(scn, avn->av_if_id,
                wmi_vdev_param_mgmt_tx_power, tx_power);
        if (retval != EOK){
            qdf_info("set TX Power for beacon wmi_failed: wmi_status %d vap-id %u", retval, avn->av_if_id);
            retval = -1;
        }
    }

    return retval;
}

void fips_data_dump(void *arg)
{
    struct ath_fips_cmd *afb = (struct ath_fips_cmd *)arg;
    int i;
    u_int8_t *ptr = (u_int8_t *) afb->data;
    qdf_info("\n ********Dumping FIPS structure********\n");
    qdf_info("\n FIPS command: %d", afb->fips_cmd);
    qdf_info("\n Key Length: %d", afb->key_len);
    qdf_info("\n Data Length: %d", afb->data_len);
    qdf_info("\n************* KEY ************\n");
    for (i=0; i < afb->key_len; i++)
    {
        qdf_info("%02x ",afb->key[i]);
    }
    qdf_info("\n************* DATA ***********\n");

    for (i=0; i < (afb->data_len); i++)
    {
        qdf_info("%02x ", *(ptr + i));
    }
    qdf_info("\n************* IV  ***********\n");

    for (i=0; i < 16; i++)
    {
        qdf_info("%02x ", afb->iv[i]);
    }
}

static int ol_ath_fips_test(struct ieee80211com *ic, struct ath_fips_cmd *fips_buf)
{
    int retval = 0;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    int32_t pdev_idx;

    fips_data_dump(fips_buf);

    pdev_idx = lmac_get_pdev_idx(scn->sc_pdev);
    if(pdev_idx < 0)
	return -EINVAL;


    if (fips_buf->key_len <= MAX_KEY_LEN_FIPS)  {
            retval = ol_ath_pdev_fips(scn, fips_buf->key,
                            fips_buf->key_len, (u_int8_t *)fips_buf->data,
                            fips_buf->data_len, fips_buf->mode, fips_buf->fips_cmd, pdev_idx);
        } else retval = -EINVAL;

    if (-EINVAL == retval) {
        qdf_info("\n%s:%d Data Length invalid: must be multiple"
            "of 16 bytes and < 1500 bytes \n", __func__, __LINE__);
        retval = -EFAULT;
    }
    return retval;
}

static void
ol_ath_kbps_to_mcs(ol_ath_soc_softc_t *soc, struct ieee80211com *ic)
{
	if(soc->ol_if_ops->kbps_to_mcs)
		ic->ic_whal_kbps_to_mcs = soc->ol_if_ops->kbps_to_mcs;
}

static void
ol_ath_ratecode_to_kbps(ol_ath_soc_softc_t *soc, struct ieee80211com *ic)
{
	if(soc->ol_if_ops->ratecode_to_kbps)
		ic->ic_whal_ratecode_to_kbps = soc->ol_if_ops->ratecode_to_kbps;
}

static void
ol_ath_get_supported_rates(ol_ath_soc_softc_t *soc, struct ieee80211com *ic)
{
	if(soc->ol_if_ops->get_supported_rates)
		ic->ic_get_supported_rates =
			soc->ol_if_ops->get_supported_rates;
}

static int
ol_ath_wmm_update(struct ieee80211com *ic, struct ieee80211vap *vap,
        bool muedca_enabled)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    struct ol_ath_vap_net80211 *avn = OL_ATH_VAP_NET80211(vap);
    struct wmm_update_params param;
    struct wmeParams *wmep = NULL;
    struct muedcaParams *muedcap = NULL;
    struct wmi_host_wme_vparams wmm_vparams[WME_NUM_AC] = {0};
    int ac = 0;
    struct common_wmi_handle *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    if (!wmi_service_enabled(pdev_wmi_handle, wmi_service_deprecated_replace)) {
        qdf_mem_set(&param, sizeof(param), 0);
        param.wmep_array = (struct wmi_host_wmeParams *)ic->ic_wme.wme_chanParams.cap_wmeParams;
        wmi_unified_wmm_update_cmd_send(pdev_wmi_handle, &param);
    } else {
        /* For lithium chips onwards */
        /* Fill values from ic_wme.wme_chanParams.cap_wmeParams to wmm_vparams */
        if(muedca_enabled) {
            for (ac = 0; ac < MUEDCA_NUM_AC; ac++) {
                muedcap = &(vap->iv_muedcastate.muedca_paramList[ac]);

                wmm_vparams[ac].acm = muedcap->muedca_acm;
                wmm_vparams[ac].aifs = muedcap->muedca_aifsn;
                wmm_vparams[ac].cwmin = ATH_EXPONENT_TO_VALUE(muedcap->muedca_ecwmin);
                wmm_vparams[ac].cwmax = ATH_EXPONENT_TO_VALUE(muedcap->muedca_ecwmax);
                wmm_vparams[ac].mu_edca_timer = muedcap->muedca_timer;
                wmm_vparams[ac].noackpolicy = 0;
            }

        }
        else {
            for (ac = 0; ac < WME_NUM_AC; ac++) {
                wmep = &(ic->ic_wme.wme_chanParams.cap_wmeParams[ac]);

                wmm_vparams[ac].acm = wmep->wmep_acm;
                wmm_vparams[ac].aifs = wmep->wmep_aifsn;
                wmm_vparams[ac].cwmin = ATH_EXPONENT_TO_VALUE(wmep->wmep_logcwmin);
                wmm_vparams[ac].cwmax = ATH_EXPONENT_TO_VALUE(wmep->wmep_logcwmax);
                wmm_vparams[ac].txoplimit = wmep->wmep_txopLimit;
                wmm_vparams[ac].noackpolicy = wmep->wmep_noackPolicy;
            }
        }
        wmi_unified_process_update_edca_param(pdev_wmi_handle, avn->av_if_id,
                                                muedca_enabled, wmm_vparams);
    }

    return 0;
}


static u_int32_t
ol_ath_txq_depth(struct ieee80211com *ic)
{
    /* TBD */
    return 0;
}

static u_int32_t
ol_ath_txq_depth_ac(struct ieee80211com *ic,int ac)
{
    /* TBD */
    return 0;
}

static void
ol_net80211_chwidth_change(struct ieee80211_node *ni)
{
    struct ieee80211com *ic = ni->ni_ic;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    struct ol_ath_vap_net80211 *avn = OL_ATH_VAP_NET80211(ni->ni_vap);

    if(ol_ath_node_set_param(scn,ni->ni_macaddr,WMI_HOST_PEER_CHWIDTH,
            ni->ni_chwidth,avn->av_if_id)) {
        qdf_info("%s:Unable to change peer bandwidth\n", __func__);
    }
}

static void
ol_ath_ar900b_fw_test(struct ieee80211com *ic, u_int32_t arg, u_int32_t value )
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    struct set_fwtest_params param;
    struct common_wmi_handle *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    qdf_mem_set(&param, sizeof(param), 0);
    param.arg = arg;
    param.value = value;

#if !(QCA_TGT_FW_IRAM_DUMP_ENABLE)
#define QCA_WIFITOOL_IRAM_DUMP_COMMAND 164
    if (arg == QCA_WIFITOOL_IRAM_DUMP_COMMAND) {
        qdf_err("Command not supported\n");
        return;
    }
#endif

    wmi_unified_vdev_set_fwtest_param_cmd_send(pdev_wmi_handle, &param);

    return;
}

static int32_t
ol_ath_fw_unit_test(struct ieee80211vap *vap, struct ieee80211_fw_unit_test_cmd *fw_unit_test_cmd)
{
    struct ol_ath_vap_net80211 *avn = OL_ATH_VAP_NET80211(vap);
    struct ol_ath_softc_net80211 *scn;
    struct wmi_unit_test_cmd param;
    uint32_t i;
    struct common_wmi_handle *pdev_wmi_handle;

    scn = avn->av_sc;
    qdf_mem_set(&param, sizeof(param), 0);
    param.vdev_id = avn->av_if_id;
    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);

    param.module_id = fw_unit_test_cmd->module_id;
    param.num_args = fw_unit_test_cmd->num_args;
    param.diag_token = fw_unit_test_cmd->diag_token;
    for(i = 0; i < fw_unit_test_cmd->num_args; i++ )
	    param.args[i] = fw_unit_test_cmd->args[i];

    wmi_unified_unit_test_cmd(pdev_wmi_handle, &param);

    return 0;
}

int
ol_ath_coex_cfg(struct ieee80211vap *vap, u_int32_t type, u_int32_t *arg)
{
    struct ol_ath_vap_net80211 *avn = OL_ATH_VAP_NET80211(vap);
    struct ol_ath_softc_net80211 *scn = avn->av_sc;
    struct coex_config_params param;
    struct common_wmi_handle *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    qdf_mem_set(&param, sizeof(param), 0);
    param.vdev_id = avn->av_if_id;
    param.config_type = type;
    param.config_arg1 = arg[0];
    param.config_arg2 = arg[1];
    param.config_arg3 = arg[2];
    param.config_arg4 = arg[3];
    param.config_arg5 = arg[4];
    param.config_arg6 = arg[5];

    return wmi_unified_send_coex_config_cmd(pdev_wmi_handle, &param);
}

static void
ol_net80211_nss_change(struct ieee80211_node *ni)
{
    struct ieee80211com *ic = ni->ni_ic;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    struct ol_ath_vap_net80211 *avn = OL_ATH_VAP_NET80211(ni->ni_vap);
    struct ieee80211vap *vap = ni->ni_vap;

    /* TODO: Enable code to send differnt NSS values per BW even for cases when EXT NSS is not enabled */
    if ((ni->ni_chwidth == IEEE80211_CWM_WIDTH160) && ni->ni_ext_nss_support) {
        if (vap->iv_cur_mode == IEEE80211_MODE_11AC_VHT160) {
            if(ol_ath_node_set_param(scn,ni->ni_macaddr, WMI_HOST_PEER_NSS_VHT160,
                 ni->ni_bw160_nss, avn->av_if_id)) {
                qdf_info("%s:Unable to change peer Nss", __func__);
            }
        } else if (vap->iv_cur_mode == IEEE80211_MODE_11AC_VHT80_80) {
            if(ol_ath_node_set_param(scn,ni->ni_macaddr, WMI_HOST_PEER_NSS_VHT80_80,
                 ni->ni_bw80p80_nss, avn->av_if_id)) {
                qdf_info("%s:Unable to change peer Nss", __func__);
           }
        }
    }

    if( ol_ath_node_set_param(scn,ni->ni_macaddr, WMI_HOST_PEER_NSS,
            ni->ni_streams, avn->av_if_id)) {
        qdf_info("%s:Unable to change peer Nss", __func__);
    }

/*
 * Beeliner change
 */
#if 0
    ol_ath_node_update(ni);
#endif
}

    static void
ol_net80211_set_sta_fixed_rate(struct ieee80211_node *ni)
{
    struct ieee80211com *ic = ni->ni_ic;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    struct ol_ath_vap_net80211 *avn = OL_ATH_VAP_NET80211(ni->ni_vap);

    if(ol_ath_node_set_param(scn,ni->ni_macaddr,WMI_HOST_PEER_PARAM_FIXED_RATE,
                ni->ni_fixed_rate,avn->av_if_id)) {
        qdf_info("%s: Unable to set peer to fixed rate", __func__);
    } else {
        qdf_info("%s: Setting fixed rate value: 0x%x",
                                __func__, ni->ni_fixed_rate);
    }
}
struct ol_vap_mode_count {
    u_int32_t non_monitor_mode_vap_cnt;
    u_int32_t monitor_mode_vap_cnt;
};

static void get_sta_mode_vap(void *arg, struct ieee80211vap *vap)
{
    ieee80211_vap_t *ppvap=(ieee80211_vap_t *)arg;

    if (IEEE80211_M_STA == vap->iv_opmode) {
        *ppvap = vap;
    }

    return;
}

static void
ol_ath_net80211_enable_radar(struct ieee80211com *ic, int no_cac)
{
}

static int
ol_ath_set_channel(struct ieee80211com *ic)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    struct ieee80211_ath_channel *chan;
    u_int32_t freq;
    ieee80211_vap_t vap = NULL;
    ol_txrx_soc_handle soc_txrx_handle;
    ol_txrx_pdev_handle pdev_txrx_handle;

    soc_txrx_handle = wlan_psoc_get_dp_handle(scn->soc->psoc_obj);
    pdev_txrx_handle = wlan_pdev_get_dp_handle(scn->sc_pdev);

    chan = ieee80211_get_current_channel(ic);

    freq = ieee80211_chan2freq(ic, chan);
    if (!freq) {
        qdf_info("ERROR : INVALID Freq \n");
        return -1;
    }

    /* update max channel power to max regpower of current channel */
    ieee80211com_set_curchanmaxpwr(ic, chan->ic_maxregpower);

    /* Update the channel for monitor mode path */
    cdp_set_curchan(soc_txrx_handle, (struct cdp_pdev *)pdev_txrx_handle, freq);

    wlan_iterate_vap_list(ic, get_sta_mode_vap ,(void *)&vap );
    if (vap) {
        /* There is a active STA mode vap.
         * for STA mode channel change will happen through resmgr channnel change
         */
         return -1;
    }
    else {
        /* Call a new channel change WMI cmd for all VAPs in AP/monitor mode.  */
        /****************************************
        FIXME: better way is to restart all VAPs in osif layer resmgr_vap_start() path,
        with parameter restart=1, e.g. wlan_mlme_start_bss(vap,1)
        ****************************************/
        if((ic->ic_prevchan) && ((chan != ic->ic_prevchan))){
            /*If different channel, need to restart all VAPs.
              If channel numbers are same, but channel ic_flags are different,
              we still consider this as 'different' channel */

            wlan_pdev_mlme_vdev_sm_chan_change(ic->ic_pdev_obj, chan);
        }
    }

    /* Restore dcs state to previous state so that interference detection will work*/
    if(ic->ic_dcs_restore) {
        ic->ic_dcs_restore(ic);
    }

    if (ic->ic_cbs && ic->ic_cbs->cbs_enable) {
        wlan_bk_scan(ic);
    }

    return 0;
}

static void
ol_ath_log_text(struct ieee80211com *ic, char *text)
{
    /* This function needs to be called from interrupt context. Temporaily disabling this as this
     * is getting called from different contexts */
    return;
#if 0
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);

#ifndef REMOVE_PKT_LOG
    scn->pl_dev->pl_funcs->pktlog_text(scn,text);
#endif
#endif
}

static void
ol_ath_log_text_bh(struct ieee80211com *ic, char *text)
{
    /* This function needs to be called only from bottom half context */
#ifndef REMOVE_PKT_LOG
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);

    if(scn->pl_dev)
        scn->pl_dev->pl_funcs->pktlog_text(scn,text);
#endif
    return;
}

static void
ol_ath_pwrsave_set_state(struct ieee80211com *ic, IEEE80211_PWRSAVE_STATE newstate)
{
    /* The host does not manage the HW power state with offload FW. This function
     * exists solely for completeness.
     */
}

u_int
ol_ath_mhz2ieee(struct ieee80211com *ic, u_int freq, u_int64_t flags)
{
    struct wlan_objmgr_pdev *pdev;
    struct wlan_objmgr_psoc *psoc;
    struct wlan_lmac_if_reg_rx_ops *reg_rx_ops;
    uint32_t chan;

    pdev =  ic->ic_pdev_obj;
    if(pdev == NULL) {
        qdf_info("%s: pdev is NULL", __func__);
        return -1;
    }

    psoc = wlan_pdev_get_psoc(pdev);
    if (psoc == NULL) {
        qdf_info("%s: psoc is NULL", __func__);
        return -1;
    }

    reg_rx_ops = wlan_lmac_if_get_reg_rx_ops(psoc);
    if (wlan_objmgr_pdev_try_get_ref(pdev, WLAN_REGULATORY_SB_ID) !=
            QDF_STATUS_SUCCESS) {
        return -1;
    }

    chan = reg_rx_ops->reg_freq_to_chan(pdev, freq);
    wlan_objmgr_pdev_release_ref(pdev, WLAN_REGULATORY_SB_ID);

    /* Not found in regdomain guess */
    if(!chan) {
        if (freq == 2484)
            return 14;
        if (freq < 2484)
            return (freq - 2407) / 5;
        if (freq <= 5000) {
            if (freq > 4900)
                return (freq - 4000) / 5;
            else
                return 15 + ((freq - 2512) / 20);
        }
        return (freq - 5000) / 5;
    }

    return chan;
}
qdf_export_symbol(ol_ath_mhz2ieee);

uint8_t ol_ath_freq_to_channel(void *pdev, uint16_t freq)
{
    struct ol_ath_softc_net80211 *scn;
    struct wlan_objmgr_pdev *pdev_obj = (struct wlan_objmgr_pdev *)pdev;
    struct ieee80211com *ic;

    scn = (struct ol_ath_softc_net80211 *)lmac_get_pdev_feature_ptr(pdev_obj);
    if (!scn) {
        qdf_info("scn is NULL");
        return 0;
    }

    ic = &scn->sc_ic;
    return ol_ath_mhz2ieee(ic, freq, 0);
}

static int16_t ol_ath_get_noisefloor (struct ieee80211com *ic, struct ieee80211_ath_channel *chan,
                                        int wait_time)
{
    /* TBD */
    return 0;
}
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
static  void ol_ath_net80211_check_buffull_condition(struct ieee80211com *ic)
{
	struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
	if(scn->soc->buff_thresh.free_descs <= scn->soc->buff_thresh.ald_free_buf_lvl)
	{
			scn->soc->buff_thresh.ald_buffull_wrn = 0;
	}
	else
    {
			scn->soc->buff_thresh.ald_buffull_wrn = 1;
    }
}
#endif
static int16_t ol_ath_net80211_get_cur_chan_noisefloor(struct ieee80211com *ic)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    return scn->chan_nf;
}

static int16_t ol_ath_net80211_get_cur_hw_nf(struct ieee80211com *ic)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    return scn->cur_hw_nf;
}

static void
ol_ath_get_chainnoisefloor(struct ieee80211com *ic, struct ieee80211_ath_channel *chan, int16_t *nfBuf)
{
    /* TBD */
    return;
}

static void
ol_ath_set_ant_switch_tbl(struct ieee80211com *ic, u_int32_t antCtrlCommon1, u_int32_t antCtrlCommon2)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    struct ant_switch_tbl_params param;
    struct common_wmi_handle *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    qdf_mem_set(&param, sizeof(param), 0);
    param.ant_ctrl_common1 = antCtrlCommon1;
    param.ant_ctrl_common2 = antCtrlCommon2;

    if(wmi_unified_set_ant_switch_tbl_cmd_send(pdev_wmi_handle, &param)) {
        return;
    }

    return;

}

/*
 * Override the rate power table in EEPROM
 */
static void
ol_ath_set_ratepwr_table(struct ieee80211com *ic, u_int8_t *ratepwr_tbl, u_int16_t ratepwr_len)
{

    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    struct ratepwr_table_params param;
    struct common_wmi_handle *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    if (!ratepwr_tbl)
        return;

    qdf_mem_set(&param, sizeof(param), 0);
    param.ratepwr_tbl = ratepwr_tbl;
    param.ratepwr_len = ratepwr_len;

    if(wmi_unified_set_ratepwr_table_cmd_send(pdev_wmi_handle, &param)) {
        return;
    }

    return;
}

/*
 * Get the rate power table in EEPROM
 */
static void
ol_ath_get_ratepwr_table(struct ieee80211com *ic)
{

    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    struct common_wmi_handle *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    if(wmi_unified_get_ratepwr_table_cmd_send(pdev_wmi_handle)) {
        return;
    }

    return;
}

/*
 * EEPROM rate power table operations
 */
static void
ol_ath_ratepwr_table_ops(struct ieee80211com *ic, u_int8_t ops,
                        u_int8_t *ratepwr_tbl, u_int16_t ratepwr_len)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);

    if (lmac_get_tgt_type(scn->soc->psoc_obj) != TARGET_TYPE_AR9888) {
        qdf_info("rate power table override is only supported for AR98XX");
        return;
    }

    if (ops == WMI_HOST_RATEPWR_TABLE_OPS_SET) {
        QC98XX_EEPROM_RATETBL eep_tbl;

        memset((u_int8_t *)&eep_tbl, 0, sizeof(QC98XX_EEPROM_RATETBL));
        /* convert user format to eeprom format */
        ol_if_ratepwr_usr_to_eeprom((u_int8_t *)&eep_tbl, sizeof(QC98XX_EEPROM_RATETBL),
                                    ratepwr_tbl, ratepwr_len);

        ol_ath_set_ratepwr_table(ic, (u_int8_t*)&eep_tbl, sizeof(QC98XX_EEPROM_RATETBL));
    } else if (ops == WMI_HOST_RATEPWR_TABLE_OPS_GET) {
        ol_ath_get_ratepwr_table(ic);
    } else
        qdf_info("Unknown ratepwr table ops %d\n", ops);

    return;
}

/*
 * The format of the CTL table defined as below
 */
#if 0
typedef struct {
    A_UINT8  ctl_edges[WHAL_NUM_BAND_EDGES_2G];
} __packed CAL_CTL_DATA_2G;

typedef struct {
    A_UINT8  ctl_edges[WHAL_NUM_BAND_EDGES_5G];
} __packed CAL_CTL_DATA_5G;

typedef struct {
    A_UINT8                       ctlFreqbin2G[WHAL_NUM_CTLS_2G][WHAL_NUM_BAND_EDGES_2G];
    CAL_CTL_DATA_2G               ctlData2G[WHAL_NUM_CTLS_2G];
    A_UINT8                       ctlFreqbin5G[WHAL_NUM_CTLS_5G][WHAL_NUM_BAND_EDGES_5G];
    CAL_CTL_DATA_5G               ctlData5G[WHAL_NUM_CTLS_5G];
} __packed CAL_INFO;
#endif

static void
ol_ath_set_ctl_table(struct ieee80211com *ic, u_int8_t *ctl_array, u_int16_t ctl_len)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    struct ctl_table_params param;
    struct common_wmi_handle *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    if (!ctl_array)
        return;

    qdf_mem_set(&param, sizeof(param), 0);

    qdf_mem_copy(&param.ctl_band, ctl_array, sizeof(uint32_t));
    param.ctl_array = ctl_array + sizeof(uint32_t);
    param.ctl_cmd_len = ctl_len;
    param.target_type = lmac_get_tgt_type(scn->soc->psoc_obj);
    if (IEEE80211_IS_CHAN_2GHZ(ic->ic_curchan)) {
        param.is_2g = TRUE;
    } else {
        param.is_2g = FALSE;
    }

    if(wmi_unified_set_ctl_table_cmd_send(pdev_wmi_handle, &param)) {
        return;
    }

    return;
}

static void
ol_ath_set_mimogain_table(struct ieee80211com *ic, u_int8_t *array_gain,
                            u_int16_t tbl_len, bool multichain_gain_bypass)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    struct mimogain_table_params param;
    struct common_wmi_handle *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    qdf_mem_set(&param, sizeof(param), 0);
    if (!array_gain)
        return;

    param.array_gain = array_gain;
    param.tbl_len = tbl_len;
    param.multichain_gain_bypass = multichain_gain_bypass;

    if(wmi_unified_set_mimogain_table_cmd_send(pdev_wmi_handle, &param)) {
        return;
    }

    return;
}

/* per node tpc control */
static void
ol_ath_set_node_tpc(struct ieee80211com *ic, struct ieee80211_node *ni, u_int8_t tpc)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    struct ol_ath_vap_net80211 *avn;

    avn = (ni!=NULL)? OL_ATH_VAP_NET80211(ni->ni_vap):NULL;

    if (!avn)
        return;

    if(ol_ath_node_set_param(scn, ni->ni_macaddr,
            WMI_HOST_PEER_USE_FIXED_PWR, tpc, avn->av_if_id)) {
        qdf_info("%s:Unable to send fixed pwr\n", __func__);
    }
}

static void
ol_ath_set_rxfilter(struct ieee80211com *ic, u_int32_t filter)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);

    if (ol_ath_pdev_set_param(scn,
                wmi_pdev_param_rx_filter, filter) != EOK)
        qdf_info("Error setting rxfilter 0x%08x\n", filter);
}

void
ol_ath_setTxPowerLimit(struct ieee80211com *ic, u_int32_t limit, u_int16_t tpcInDb, u_int32_t is2GHz)
{
    int retval = 0;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    u_int16_t cur_tx_power = ieee80211com_get_txpowerlimit(ic);

    if (cur_tx_power != limit) {
        /* Update max tx power only if the current max tx power is different */
        if (limit > scn->max_tx_power) {
                qdf_info("Tx power value is greater than supported max tx power %d, Limiting to default Max\n",
                    scn->max_tx_power);
		limit = scn->max_tx_power;
        }
        if (is2GHz) {
            retval = ol_ath_pdev_set_param(scn,
                wmi_pdev_param_txpower_limit2g, limit);
        } else {
            retval = ol_ath_pdev_set_param(scn,
                wmi_pdev_param_txpower_limit5g, limit);
        }
        if (retval == EOK) {
            /* Update the ic_txpowlimit */
            if (is2GHz) {
                scn->txpowlimit2G = limit;
            } else {
                scn->txpowlimit5G = limit;
            }
            if ((is2GHz && IEEE80211_IS_CHAN_2GHZ(ic->ic_curchan)) ||
                (!is2GHz && !IEEE80211_IS_CHAN_2GHZ(ic->ic_curchan)))
            {
                ieee80211com_set_txpowerlimit(ic, (u_int16_t) (limit));
            }
        }
    }
}

static u_int8_t
ol_ath_get_common_power(struct ieee80211com *ic, struct ieee80211_ath_channel *chan)
{
    /* TBD */
    return 0;
}

static u_int32_t
ol_ath_getTSF32(struct ieee80211com *ic)
{
    /* TBD */
    return 0;
}

static int
ol_ath_getrmcounters(struct ieee80211com *ic, struct ieee80211_mib_cycle_cnts *pCnts)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);

    /* "ic->ic_mib_cycle_cnts" gets updated during a periodic stats event */
    pCnts->tx_frame_count = scn->mib_cycle_cnts.tx_frame_count;
    pCnts->rx_frame_count = scn->mib_cycle_cnts.rx_frame_count;
    pCnts->rx_clear_count = scn->mib_cycle_cnts.rx_clear_count;
    pCnts->rx_clear_ext_count = scn->mib_cycle_cnts.rx_clear_ext_count;
    pCnts->rx_clear_ext40_count = scn->mib_cycle_cnts.rx_clear_ext40_count;
    pCnts->rx_clear_ext80_count = scn->mib_cycle_cnts.rx_clear_ext80_count;
    pCnts->cycle_count = scn->mib_cycle_cnts.cycle_count;

    /* "is_rx_active" and "is_tx_active" not being used, but for safety, set it to 0 */
    pCnts->is_rx_active = 0;
    pCnts->is_tx_active = 0;

    return 0;
}

static u_int32_t
ol_ath_wpsPushButton(struct ieee80211com *ic)
{
    /* TBD */
    return 0;
}

static void
ol_ath_clear_phystats(struct ieee80211com *ic)
{
    /* TBD */
    return;
}

static int
ol_ath_set_macaddr(struct ieee80211com *ic, u_int8_t *macaddr)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    struct macaddr_params param;
    struct common_wmi_handle *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    qdf_mem_set(&param, sizeof(param), 0);
    param.macaddr = macaddr;

    if(wmi_unified_set_macaddr_cmd_send(pdev_wmi_handle, &param)) {
        return -1;
    }

    return 0;
}

static int
ol_ath_set_chain_mask(struct ieee80211com *ic, ieee80211_device_param type, u_int32_t mask)
{
    /* TBD */
    return 0;
}

static u_int32_t
ol_ath_getmfpsupport(struct ieee80211com *ic)
{
    return IEEE80211_MFP_HW_CRYPTO;
}

static void
ol_ath_setmfpQos(struct ieee80211com *ic, u_int32_t dot11w)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    ol_ath_pdev_set_param(scn,
                               wmi_pdev_param_pmf_qos, dot11w);
    return;
}

static u_int64_t
ol_ath_get_tx_hw_retries(struct ieee80211com *ic)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    ol_txrx_pdev_handle pdev = NULL;
    ol_txrx_soc_handle soc_txrx_handle;

    soc_txrx_handle = wlan_psoc_get_dp_handle(scn->soc->psoc_obj);
    pdev = wlan_pdev_get_dp_handle(ic->ic_pdev_obj);
    return cdp_ath_get_total_per(soc_txrx_handle,(struct cdp_pdev *)pdev);
}

static u_int64_t
ol_ath_get_tx_hw_success(struct ieee80211com *ic)
{
    /* TBD */
    return 0;
}

/* Update peer rate table */
static void
ol_ath_rate_node_update(struct ieee80211com *ic, struct ieee80211_node *ni,
                                   int isnew)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);

    /* reuse the ASSOC_CMDID to perform the dynamic rate setting */
    ol_ath_send_peer_assoc(scn, ic, ni, isnew);
}

#ifdef BIG_ENDIAN_HOST
void swap_bytes(void *pv, size_t n)
{
       int noWords;
       int i;
       A_UINT32 *wordPtr;

       noWords =   n/sizeof(u_int32_t);
       wordPtr = (u_int32_t *)pv;
       for (i=0;i<noWords;i++)
       {
               *(wordPtr + i) = __cpu_to_le32(*(wordPtr + i));
       }
}
#define SWAPME(x, len) swap_bytes(&x, len);
#endif

static int
ol_ath_debug_print_event_handler (ol_soc_t sc, u_int8_t *data, u_int32_t datalen)
{
#ifdef BIG_ENDIAN_HOST
       char dbgbuf[500] = {0};
       memcpy(dbgbuf, data, datalen);
       SWAPME(dbgbuf, datalen);
       qdf_info("FIRMWARE:%s \n",dbgbuf);
       return 0;
#else
       qdf_info("FIRMWARE:%s \n",data);
       return 0;
#endif
}

static bool
ol_ath_set_config_enable(struct ieee80211vap* vap)
{
       struct wlan_objmgr_psoc *psoc;

       psoc = wlan_vdev_get_psoc(vap->vdev_obj);
       return ol_target_lithium(psoc);
}

static void
ol_ath_set_config(struct ieee80211vap* vap)
{
      /* Currently Not used for Offload */
}

static void
ol_ath_set_safemode(struct ieee80211vap* vap, int val)
{
    struct wlan_objmgr_psoc *psoc;
    struct wlan_objmgr_vdev *vdev_obj = vap->vdev_obj;
    ol_txrx_vdev_handle vdev = (ol_txrx_vdev_handle)wlan_vdev_get_dp_handle(vdev_obj);
    psoc = wlan_vdev_get_psoc(vdev_obj);

    if (vdev) {
        cdp_set_safemode(wlan_psoc_get_dp_handle(psoc),
                (struct cdp_vdev *)vdev, val);
    }
    return;
}

static void
ol_ath_set_privacy_filters(struct ieee80211vap* vap)
{
    struct wlan_objmgr_psoc *psoc;
    struct wlan_objmgr_vdev *vdev_obj = vap->vdev_obj;
    ol_txrx_vdev_handle vdev = (ol_txrx_vdev_handle)wlan_vdev_get_dp_handle(vdev_obj);
    psoc = wlan_vdev_get_psoc(vdev_obj);

    if (vdev) {
        cdp_set_privacy_filters(wlan_psoc_get_dp_handle(psoc),
                (struct cdp_vdev *)vdev, vap->iv_privacy_filters, vap->iv_num_privacy_filters);
    }
    return;
}

static int
ol_ath_set_rx_monitor_filter(struct ieee80211com *ic, uint32_t val)
{
    ol_txrx_pdev_handle pdev = NULL;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    struct cdp_monitor_filter filter_val;
    ol_txrx_soc_handle soc_txrx_handle;

    soc_txrx_handle = wlan_psoc_get_dp_handle(scn->soc->psoc_obj);
    pdev = wlan_pdev_get_dp_handle(ic->ic_pdev_obj);

    filter_val.mode = RX_MON_FILTER_PASS | RX_MON_FILTER_OTHER;
    filter_val.fp_mgmt = SET_MON_FILTER_MGMT(val);
    filter_val.fp_ctrl = SET_MON_FILTER_CTRL(val);
    filter_val.fp_data = SET_MON_FILTER_DATA(val);
    filter_val.mo_mgmt = SET_MON_FILTER_MGMT(val);
    filter_val.mo_ctrl = SET_MON_FILTER_CTRL(val);
    filter_val.mo_data = SET_MON_FILTER_DATA(val);

    cdp_set_monitor_filter(soc_txrx_handle,
                (struct cdp_pdev *)pdev, &filter_val);

    return 0;
}

static void
ol_ath_scan_start(struct ieee80211com *ic)
{
#ifdef DEPRECATED_QCA
    /*
     * this command was added to support host scan egine which is deprecated.
     * now  the scan engine is in FW and host directly isssues a scan request
     * to perform scan and provide results back to host
     */
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    struct common_wmi_handle *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    wmi_unified_pdev_scan_start_cmd_send(pdev_wmi_handle);
#endif
}

static void
ol_ath_scan_end(struct ieee80211com *ic)
{
#ifdef DEPRECATED_QCA
    /*
     * this command was added to support host scan egine which is deprecated.
     * now  the scan engine is in FW and host directly isssues a scan request
     * to perform scan and provide results back to host
     */
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    struct common_wmi_handle *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    wmi_unified_pdev_scan_end_cmd_send(pdev_wmi_handle);
#endif
}

#if ATH_SUPPORT_IQUE
void
ol_ath_set_acparams(struct ieee80211com *ic, u_int8_t ac, u_int8_t use_rts,
                          u_int8_t aggrsize_scaling, u_int32_t min_kbps)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    struct acparams_params param;
    struct common_wmi_handle *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    qdf_mem_set(&param, sizeof(param), 0);
    param.ac = ac;
    param.aggrsize_scaling = aggrsize_scaling;

    wmi_unified_set_acparams_cmd_send(pdev_wmi_handle, &param);
    return;
}

void
ol_ath_set_rtparams(struct ieee80211com *ic, u_int8_t ac, u_int8_t perThresh,
                          u_int8_t probeInterval)
{
    /* TBD */
    return;
}

void
ol_ath_get_iqueconfig(struct ieee80211com *ic)
{
    /* TBD */
    return;
}

void
ol_ath_set_hbrparams(struct ieee80211vap *iv, u_int8_t ac, u_int8_t enable, u_int8_t per)
{
    /* TBD */
    return;
}
#endif /*ATH_SUPPORT_IQUE*/

/*
 * Disable the dcs im when the intereference is detected too many times. for
 * thresholds check umac
 */
static void
ol_ath_disable_dcsim(struct ieee80211com *ic)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);

    /* clear the run state, only when cwim is not set */
    if (!(OL_IS_DCS_ENABLED(scn->scn_dcs.dcs_enable) & ATH_CAP_DCS_CWIM)) {
        OL_ATH_DCS_CLR_RUNSTATE(scn->scn_dcs.dcs_enable);
    }

    OL_ATH_DCS_DISABLE(scn->scn_dcs.dcs_enable, ATH_CAP_DCS_WLANIM);

    /* send target to disable and then disable in host */
    ol_ath_pdev_set_param(scn, wmi_pdev_param_dcs, scn->scn_dcs.dcs_enable);
}

static void
ol_ath_enable_dcsim(struct ieee80211com *ic)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);

    /* Enable wlanim for DCS */
    OL_ATH_DCS_ENABLE(scn->scn_dcs.dcs_enable, ATH_CAP_DCS_WLANIM);

    /* send target to enable and then enable in host */
    ol_ath_pdev_set_param(scn, wmi_pdev_param_dcs, scn->scn_dcs.dcs_enable);
}

/*
 * Disable the dcs cw when the intereference is detected too many times. for
 * thresholds check umac
 */
static void
ol_ath_disable_dcscw(struct ieee80211com *ic)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    u_int8_t dcs_enable = scn->scn_dcs.dcs_enable;

    qdf_info("DCS: %s dcs state %x \n",__func__,scn->scn_dcs.dcs_enable);

    if (!(OL_IS_DCS_ENABLED(scn->scn_dcs.dcs_enable) & ATH_CAP_DCS_CWIM)) {
       return;
    }

    OL_ATH_DCS_DISABLE(scn->scn_dcs.dcs_enable, ATH_CAP_DCS_CWIM);
    /* send target to disable and then disable in host */
    if (ol_ath_pdev_set_param(scn, wmi_pdev_param_dcs,
                                         scn->scn_dcs.dcs_enable) != EOK) {
        OL_ATH_DCS_ENABLE(scn->scn_dcs.dcs_enable, dcs_enable);
        qdf_info("Error in disabling CWIM\n");
    }
}

/* Turn on the dcs,use the same state as what the current
*  enabled state of dcs and reset the cw interference flag.
*  Also set the run state accordingly.
*/

static void
ol_ath_dcs_restore(struct ieee80211com *ic)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    spin_lock(&ic->ic_lock);
    if (ic->cw_inter_found)
        ic->cw_inter_found = 0;

    QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO, "DCS: %s dcs state %x \n",__func__,scn->scn_dcs.dcs_enable);
    /* once the channel change is complete, turn on the dcs,
     * use the same state as what the current enabled state of the dcs. Also
     * set the run state accordingly.
     */
    (void)ol_ath_pdev_set_param(scn, wmi_pdev_param_dcs, scn->scn_dcs.dcs_enable&0x0f);
    (OL_IS_DCS_ENABLED(scn->scn_dcs.dcs_enable)) ? (OL_ATH_DCS_SET_RUNSTATE(scn->scn_dcs.dcs_enable)) :
                            (OL_ATH_DCS_CLR_RUNSTATE(scn->scn_dcs.dcs_enable));
    spin_unlock(&ic->ic_lock);
}

#ifdef ATH_SUPPORT_TxBF

static int
ol_ath_net80211_txbf_alloc_key(struct ieee80211com *ic, struct ieee80211_node *ni)
{
    return 0;
}

static void
ol_ath_net80211_txbf_set_key(struct ieee80211com *ic, struct ieee80211_node *ni)
{
    return;
}

static int
ol_ath_net80211_txbf_del_key(struct ieee80211com *ic, struct ieee80211_node *ni)
{
    return 0;
}

static void
ol_ath_net80211_init_sw_cv_timeout(struct ieee80211com *ic, struct ieee80211_node *ni)
{
    return;
}

static int
ol_ath_set_txbfcapability(struct ieee80211com *ic)
{
    return 0;
}

#ifdef TXBF_DEBUG
static void
ol_ath_net80211_txbf_check_cvcache(struct ieee80211com *ic, struct ieee80211_node *ni)
{
    return 0;
}
#endif

static void
ol_ath_net80211_v_cv_send(struct ieee80211_node *ni,
                       u_int8_t *data_buf,
                       u_int16_t buf_len)
{
    return;
}
static void
ol_ath_net80211_txbf_stats_rpt_inc(struct ieee80211com *ic,
                                struct ieee80211_node *ni)
{
    return;
}
static void
ol_ath_net80211_txbf_set_rpt_received(struct ieee80211com *ic,
                                struct ieee80211_node *ni)
{
    return;
}
#endif

QDF_STATUS
ol_is_mode_offload(struct wlan_objmgr_pdev *pdev, bool *is_offload)
{
    *is_offload = TRUE;
    return QDF_STATUS_SUCCESS;
}

bool
ol_ath_net80211_is_mode_offload(struct ieee80211com *ic)
{
    /*
     * If this function executes, it is offload mode
     */
    return TRUE;
}

QDF_STATUS ol_ath_get_ah_devid(struct wlan_objmgr_pdev *pdev,
        uint16_t *devid)
{
    /* devid is needed only for DA */
    return QDF_STATUS_SUCCESS;
}

QDF_STATUS ol_ath_get_phymode_info(struct wlan_objmgr_pdev *pdev,
        uint32_t chan_mode, uint32_t *mode_info, bool is_2gvht_en)
{
    struct ol_ath_softc_net80211 *scn;
    struct pdev_osif_priv *osif_priv;

    osif_priv = wlan_pdev_get_ospriv(pdev);

    if (osif_priv == NULL) {
        qdf_info("%s : osif_priv is NULL", __func__);
        return QDF_STATUS_E_FAILURE;
    }

    scn = (struct ol_ath_softc_net80211 *)osif_priv->legacy_osif_priv;

    *mode_info = ol_get_phymode_info_from_wlan_phymode(scn, chan_mode, is_2gvht_en);
    return QDF_STATUS_SUCCESS;
}

static bool
ol_ath_net80211_is_macreq_enabled(struct ieee80211com *ic)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);

    return ((scn->macreq_enabled == 1) ? TRUE : FALSE);
}

static u_int32_t
ol_ath_net80211_get_mac_prealloc_idmask(struct ieee80211com *ic)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);

    return scn->sc_prealloc_idmask;
}

static void
ol_ath_net80211_set_mac_prealloc_id(struct ieee80211com *ic, u_int8_t id,
                                                            u_int8_t set)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    if(set) {
        scn->sc_prealloc_idmask |= (1 << id);
    }
    else {
        scn->sc_prealloc_idmask &= ~(1 << id);
    }
}

static uint8_t
ol_ath_net80211_get_bsta_fixed_idmask(struct ieee80211com *ic)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);

    return scn->sc_bsta_fixed_idmask;
}

static int
ol_ath_net80211_tr69_get_vdev_extd_stats(struct ieee80211vap *vap,
                                         uint32_t cmd,
                                         uint32_t *arg1)
{
    ol_txrx_vdev_handle iv_txrx_handle;
    struct wlan_objmgr_psoc *psoc;
    ol_txrx_soc_handle soc_handle;
    wmi_host_vdev_extd_stats extd_stats;

    iv_txrx_handle = wlan_vdev_get_dp_handle(vap->vdev_obj);
    psoc = wlan_pdev_get_psoc(vap->iv_ic->ic_pdev_obj);

    if (!iv_txrx_handle || !psoc)
        return -ENOENT;

    soc_handle = wlan_psoc_get_dp_handle(psoc);
    cdp_get_vdev_extd_stats(soc_handle,
                            (struct cdp_vdev *)iv_txrx_handle,
                            &extd_stats);
    switch(cmd) {
        case IEEE80211_TR069_GET_FAIL_RETRANS_CNT:
             *arg1 = extd_stats.mpdu_fail_retry;
        break;
        case IEEE80211_TR069_GET_RETRY_CNT:
             *arg1 =  extd_stats.mpdu_suc_retry;
        break;
        case IEEE80211_TR069_GET_MUL_RETRY_CNT:
             *arg1 = extd_stats.mpdu_suc_multitry;
        break;
        case IEEE80211_TR069_GET_ACK_FAIL_CNT:
             *arg1 = extd_stats.ppdu_noack;
        break;
        case IEEE80211_TR069_GET_AGGR_PKT_CNT:
             *arg1 = extd_stats.ppdu_aggr_cnt;
        break;
        case IEEE80211_TR069_GET_CHAN_UTIL:
             *arg1 = 0;
        break;
        case IEEE80211_TR069_GET_RETRANS_CNT:
             *arg1 = extd_stats.mpdu_sw_requed;
        break;
    }

    return 0;
}

static int
ol_ath_net80211_tr69_get_sta_bytes_sent(struct ieee80211vap *vap, u_int32_t *bytessent, u_int8_t *dstmac)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211_node *ni = NULL;
    struct wlan_objmgr_psoc *psoc;
    struct cdp_peer_stats *peer_stats;
    struct cdp_peer *peer_dp_handle;

    ni = ieee80211_find_node(&ic->ic_sta, dstmac);
    psoc = wlan_pdev_get_psoc(ic->ic_pdev_obj);
    if (!ni || !ni->peer_obj || !psoc) {
        if(ni)
           ieee80211_free_node(ni);
        return -ENOENT;
    }

    peer_dp_handle = wlan_peer_get_dp_handle(ni->peer_obj);
    if (!peer_dp_handle) {
        if(ni)
           ieee80211_free_node(ni);
        return -ENOENT;
    }
    peer_stats = cdp_host_get_peer_stats(wlan_psoc_get_dp_handle(psoc),
                                         peer_dp_handle);
    if (!peer_stats) {
        ieee80211_free_node(ni);
        return -ENOENT;
    }
    *bytessent = peer_stats->tx.ucast.bytes;

    ieee80211_free_node(ni);
    return 0;
}

static int
ol_ath_net80211_tr69_get_sta_bytes_rcvd(struct ieee80211vap *vap, u_int32_t *bytesrcvd, u_int8_t *dstmac)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211_node *ni = NULL;
    struct wlan_objmgr_psoc *psoc;
    struct cdp_peer_stats *peer_stats;
    struct cdp_peer *peer_dp_handle;

    ni = ieee80211_find_node(&ic->ic_sta, dstmac);
    psoc = wlan_pdev_get_psoc(ic->ic_pdev_obj);
    if (!ni || !ni->peer_obj || !psoc) {
        if(ni)
           ieee80211_free_node(ni);
        return -ENOENT;
    }

    peer_dp_handle = wlan_peer_get_dp_handle(ni->peer_obj);
    if (!peer_dp_handle) {
        if(ni)
           ieee80211_free_node(ni);
        return -ENOENT;
    }

    peer_stats = cdp_host_get_peer_stats(wlan_psoc_get_dp_handle(psoc),
                                         peer_dp_handle);
    if (!peer_stats) {
        ieee80211_free_node(ni);
        return -ENOENT;
    }
    *bytesrcvd = peer_stats->rx.unicast.bytes;

    ieee80211_free_node(ni);
    return 0;
}

static int
ol_ath_net80211_tr69_process_request(struct ieee80211vap *vap, int cmdid, void * arg1, void *arg2)
{
    //	struct ieee80211com *ic = vap->iv_ic;

	switch(cmdid){
#if 0
        case IEEE80211_TR069_GET_PLCP_ERR_CNT:
			ol_ath_net80211_tr69_get_plcp_err_cnt(dev, arg1);
            break;
        case IEEE80211_TR069_GET_FCS_ERR_CNT:
			ol_ath_net80211_tr69_get_fcs_err_cnt(dev, arg1);
            break;
        case IEEE80211_TR069_GET_PKTS_OTHER_RCVD:
			ol_ath_net80211_tr69_get_pkts_other_rcvd(dev, req);
            break;
#endif
        case IEEE80211_TR069_GET_FAIL_RETRANS_CNT:
        case IEEE80211_TR069_GET_RETRY_CNT:
        case IEEE80211_TR069_GET_MUL_RETRY_CNT:
        case IEEE80211_TR069_GET_ACK_FAIL_CNT:
        case IEEE80211_TR069_GET_AGGR_PKT_CNT:
        case IEEE80211_TR069_GET_CHAN_UTIL:
        case IEEE80211_TR069_GET_RETRANS_CNT:
             ol_ath_net80211_tr69_get_vdev_extd_stats(vap, cmdid, arg1);
        break;
        case IEEE80211_TR069_GET_STA_BYTES_SENT:
             ol_ath_net80211_tr69_get_sta_bytes_sent(vap, arg1, arg2);
            break;
        case IEEE80211_TR069_GET_STA_BYTES_RCVD:
             ol_ath_net80211_tr69_get_sta_bytes_rcvd(vap, arg1, arg2);
            break;
#if 0
        case IEEE80211_TR069_GET_DATA_SENT_ACK:
			ol_ath_net80211_tr69_get_data_sent_ack(dev, arg1);
            break;
        case IEEE80211_TR069_GET_DATA_SENT_NOACK:
			ol_ath_net80211_tr69_get_data_sent_noack(dev, req);
            break;
#endif
        default:
			break;
    }

    return 0;
}

/*default rate limit period - 60 sec*/
#define PEER_REF_PRINT_RATE_LIMIT_PERIOD (60*HZ)
/*default burst for rate limit */
#define PEER_REF_PRINT_RATE_LIMIT_BURST_DEFAULT   1
DEFINE_RATELIMIT_STATE(peer_refs_ratelimit,
        PEER_REF_PRINT_RATE_LIMIT_PERIOD,
        PEER_REF_PRINT_RATE_LIMIT_BURST_DEFAULT);

static int print_peer_refs_ratelimit(void)
{
    return __ratelimit(&peer_refs_ratelimit);
}

void ol_ath_print_peer_refs(struct ieee80211vap *vap,
        struct ieee80211com *ic, bool assert)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    int loop_cnt = OSIF_PEER_DELETE_LOOP_COUNT * 2;
    bool do_fw_assert = false;
    bool too_many_prints = true;

    if (print_peer_refs_ratelimit())
        too_many_prints = false;

    if (!too_many_prints) {
        qdf_warn("dumping psoc object references");
        wlan_objmgr_print_ref_cnts(ic);
    }

    qdf_warn("%s: vap: 0x%pK, id: %d, opmode: %d, peer_cnt: %d",
            __func__, vap, vap->iv_unit, vap->iv_opmode,
            wlan_vdev_get_peer_count(vap->vdev_obj));

#ifdef QCA_SUPPORT_CP_STATS
    qdf_warn("%s:[PDEV] mgmt: tx: %llu, comp: %llu, err: %u "
              "mgmt_pending_completions: %d",
            __func__, pdev_cp_stats_wmi_tx_mgmt_get(ic->ic_pdev_obj),
            pdev_cp_stats_wmi_tx_mgmt_completions_get(ic->ic_pdev_obj),
            pdev_cp_stats_wmi_tx_mgmt_completion_err_get(ic->ic_pdev_obj),
            qdf_atomic_read(&scn->mgmt_ctx.mgmt_pending_completions));
#endif

    qdf_warn("[VDEV] mgmt: tx: %llu, comp: %llu",
            vap->wmi_tx_mgmt, vap->wmi_tx_mgmt_completions);

    qdf_warn("[VDEV] sta mgmt: tx: %llu, comp: %llu",
            vap->wmi_tx_mgmt_sta, vap->wmi_tx_mgmt_completions_sta);

    qdf_warn("[VDEV] peer_delete: req: %llu, resp: %llu",
            vap->peer_delete_req, vap->peer_delete_resp);

    if (assert && !too_many_prints) {
        if ((vap->peer_delete_req -
                    vap->peer_delete_resp) > 0) {
            do_fw_assert = true;
            qdf_warn("Missing peer delete responses on SOC");
        }
        if (((int)(vap->wmi_tx_mgmt_sta -
                    vap->wmi_tx_mgmt_completions_sta)) > 0) {
            do_fw_assert = true;
            qdf_warn("Missing Mgmt completions for STA peers on VDEV");
        }

        if (do_fw_assert && wlan_vdev_get_peer_count(vap->vdev_obj) > 1) {
#if UMAC_SUPPORT_ACFG
            OSIF_RADIO_DELIVER_EVENT_WATCHDOG(ic, ACFG_WDT_VAP_STOP_FAILED);
#endif
            /* system shall recover from SSR path */
            qdf_warn("Asserting Target...");
            ol_ath_set_fw_hang(scn, 0);
            while (loop_cnt > 0) {
                schedule_timeout_interruptible(OSIF_PEER_DELETE_TIMEOUT);
                loop_cnt--;
            }
        }
    }
}

bool
ol_ath_wide_band_scan(struct ieee80211com *ic)
{
    bool ret_val;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    /* return true if host and FW both support wide band scan.
     * false otherwise.
     */
    ret_val =(ic->ic_widebw_scan &&
              wlan_psoc_nif_fw_ext_cap_get(scn->soc->psoc_obj,
                                  WLAN_SOC_CEXT_WIDEBAND_SCAN));

    return ret_val;
}

int
ol_ath_net80211_get_vap_stats(struct ieee80211vap *vap)
{
    ol_txrx_vdev_handle iv_txrx_handle;
    struct wlan_objmgr_psoc *psoc;
    ol_txrx_soc_handle soc_handle;
    wmi_host_vdev_extd_stats vdev_extd_stats;

    iv_txrx_handle = wlan_vdev_get_dp_handle(vap->vdev_obj);
    psoc = wlan_pdev_get_psoc(vap->iv_ic->ic_pdev_obj);

    if(!iv_txrx_handle || !psoc)
        return -ENOENT;

    soc_handle = wlan_psoc_get_dp_handle(psoc);
    cdp_get_vdev_extd_stats(soc_handle,
                            (struct cdp_vdev *)iv_txrx_handle,
                            &vdev_extd_stats);
    qdf_info("-----VAP Stats------\n");
    qdf_info("\n");
    qdf_info("ppdu_aggr         = %d\n", vdev_extd_stats.ppdu_aggr_cnt);
    qdf_info("ppdu_nonaggr      = %d\n", vdev_extd_stats.ppdu_nonaggr_cnt);
    qdf_info("noack             = %d\n", vdev_extd_stats.ppdu_noack);
    qdf_info("mpdu_queued       = %d\n", vdev_extd_stats.mpdu_queued);
    qdf_info("mpdu_requeued     = %d\n", vdev_extd_stats.mpdu_sw_requed);
    qdf_info("mpdu_suc_retry    = %d\n", vdev_extd_stats.mpdu_suc_retry);
    qdf_info("mpdu_suc_multitry = %d\n", vdev_extd_stats.mpdu_suc_multitry);
    qdf_info("mpdu_fail_retry   = %d\n", vdev_extd_stats.mpdu_fail_retry);
    qdf_info("-----VAP Stats------\n");
    qdf_info("\n");

    return 0;
}

/*
 * Register the DCS functionality
 * As such this is very small function and is not going to contain too many
 * functions, right now continuing in the same file. Once it grows bigger,
 * move to different file.
 *
 *  # register event handler to receive non-wireless lan interference event
 *  # register event handler to receive the extended stats that are meant for
 *    receiving the timed extra stats
 *        - right now this is not implemented and would implement
 *          as we go with second implementation
 *  # initialize the initial enabled state
 *  # initialize the host data strucutres that are meant for handling
 *    the wireless lan interference.
 *          - right now these variables would not be used
 *  # Keep the initialized state as disabled, and enable
 *    when first channel gets activated.
 *  # Keep the status as disabled until completely qualified
 */
void
ol_ath_dcs_attach(struct ieee80211com *ic)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    /* Register WMI event handlers */
	scn->scn_dcs.dcs_enable                 = 0;
    OL_ATH_DCS_CLR_RUNSTATE(scn->scn_dcs.dcs_enable);
	scn->scn_dcs.phy_err_penalty            = DCS_PHYERR_PENALTY;
	scn->scn_dcs.phy_err_threshold          = DCS_PHYERR_THRESHOLD ;
	scn->scn_dcs.radar_err_threshold        = DCS_RADARERR_THRESHOLD;
	scn->scn_dcs.coch_intr_thresh           = DCS_COCH_INTR_THRESHOLD ;
	scn->scn_dcs.tx_err_thresh              = DCS_TXERR_THRESHOLD;
	scn->scn_dcs.user_max_cu                = DCS_USER_MAX_CU; /* tx_cu + rx_cu */
	scn->scn_dcs.intr_detection_threshold   = DCS_INTR_DETECTION_THR;
	scn->scn_dcs.intr_detection_window      = DCS_SAMPLE_SIZE;
	scn->scn_dcs.scn_dcs_im_stats.im_intr_cnt = 0;
	scn->scn_dcs.scn_dcs_im_stats.im_samp_cnt = 0;
    scn->scn_dcs.dcs_debug                  = DCS_DEBUG_DISABLE;

    return;
}

void ol_ath_soc_dcs_attach(ol_ath_soc_softc_t *soc)
{
    struct common_wmi_handle *wmi_handle;

    wmi_handle = lmac_get_wmi_hdl(soc->psoc_obj);

    wmi_unified_register_event_handler((wmi_unified_t)wmi_handle,
                    wmi_dcs_interference_event_id,
                    ol_ath_dcs_interference_handler, WMI_RX_UMAC_CTX);
}
#if ATH_OL_FAST_CHANNEL_RESET_WAR
#define DISABLE_FAST_CHANNEL_RESET 1
     /*WAR for EV#117307, MSDU_DONE is not set for data packet,
      to fix this issue, fast channel change is disabled for x86 platform*/
void ol_ath_fast_chan_change(struct ol_ath_softc_net80211 *scn)
{
    qdf_info("Disabling fast channel reset \n");
    if(ol_ath_pdev_set_param(scn,
                       wmi_pdev_param_fast_channel_reset,
                       DISABLE_FAST_CHANNEL_RESET)) {
        qdf_info(" Failed to disable fast channel reset \n");
    }
}
#endif

#define CTS2SELF_DTIM_ENABLE 0x1
#define CTS2SELF_DTIM_DISABLE 0x0
void
ol_ath_set_vap_cts2self_prot_dtim_bcn(struct ieee80211vap *vap)
{
    struct ieee80211com *ic = NULL;
    struct ol_ath_softc_net80211 *scn = NULL;
    struct ol_ath_vap_net80211 *avn = NULL;

    if (vap == NULL) return;

    ic = vap->iv_ic;
    avn = OL_ATH_VAP_NET80211(vap);
    scn = OL_ATH_SOFTC_NET80211(ic);

    /* Enable CTS-to-self */
    if (vap->iv_cts2self_prot_dtim_bcn) {
         ol_ath_wmi_send_vdev_param( scn,avn->av_if_id,
                     wmi_vdev_param_dtim_enable_cts, CTS2SELF_DTIM_ENABLE);
    } else {
         ol_ath_wmi_send_vdev_param( scn,avn->av_if_id,
                     wmi_vdev_param_dtim_enable_cts, CTS2SELF_DTIM_DISABLE);
     }
}
#undef CTS2SELF_DTIM_ENABLE
#undef CTS2SELF_DTIM_DISABLE

/* The below mapping is according the doc, which is as follows,

DSCP        TID     AC
000000      0       WME_AC_BE
001000      1       WME_AC_BK
010000      1       WME_AC_BK
011000      0       WME_AC_BE
100000      5       WME_AC_VI
101000      5       WME_AC_VI
110000      6       WME_AC_VO
111000      6       WME_AC_VO

*/

int
ol_ath_set_vap_dscp_tid_map(struct ieee80211vap *vap)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    struct ol_ath_vap_net80211 *avn = OL_ATH_VAP_NET80211(vap);
    struct vap_dscp_tid_map_params param;
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    osif_dev* osifp = (osif_dev *)(vap->iv_ifp);
#endif
    ol_txrx_soc_handle soc_txrx_handle;
    struct common_wmi_handle *pdev_wmi_handle;

    soc_txrx_handle = wlan_psoc_get_dp_handle(scn->soc->psoc_obj);
    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);

    qdf_mem_set(&param, sizeof(param), 0);
#if ATH_SUPPORT_DSCP_OVERRIDE
    if(vap->iv_dscp_map_id) {
        /* Send updated copy of the TID-Map */
        param.dscp_to_tid_map = vap->iv_ic->ic_dscp_tid_map[vap->iv_dscp_map_id];
    }
    else {
        param.dscp_to_tid_map = dscp_tid_map;
    }
#endif
    param.vdev_id = avn->av_if_id;

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    if (vap->iv_ic->nss_vops)
        vap->iv_ic->nss_vops->ic_osif_nss_vdev_set_dscp_tid_map(osifp, param.dscp_to_tid_map);
#endif
    qdf_info("Setting dscp for vap id: %d\n", param.vdev_id);

    if(ol_target_lithium(scn->soc->psoc_obj)) {
#if ATH_SUPPORT_DSCP_OVERRIDE
        cdp_set_vdev_dscp_tid_map(soc_txrx_handle,
              (struct cdp_vdev *)wlan_vdev_get_dp_handle(vap->vdev_obj), vap->iv_dscp_map_id);
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    if (vap->iv_ic->nss_vops)
        vap->iv_ic->nss_vops->ic_osif_nss_vdev_set_dscp_tid_map_id(osifp, vap->iv_dscp_map_id);
#endif
#endif
        return 0;
    } else
        return wmi_unified_set_vap_dscp_tid_map_cmd_send(pdev_wmi_handle, &param);
}

#if ATH_SUPPORT_DSCP_OVERRIDE
void
ol_ath_set_pdev_dscp_tid_map(struct ieee80211vap *vap, uint32_t tos)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    uint8_t map_id = vap->iv_dscp_map_id;
    uint8_t tid = vap->iv_ic->ic_dscp_tid_map[map_id][(tos >> IP_DSCP_SHIFT) & IP_DSCP_MASK];
    ol_txrx_soc_handle soc_txrx_handle;
    ol_txrx_pdev_handle pdev_txrx_handle;

    soc_txrx_handle = wlan_psoc_get_dp_handle(scn->soc->psoc_obj);
    pdev_txrx_handle = wlan_pdev_get_dp_handle(scn->sc_pdev);

    cdp_set_pdev_dscp_tid_map(soc_txrx_handle, (struct cdp_pdev *)pdev_txrx_handle, map_id, tos, tid);
}
void
ol_ath_set_hmmc_tid(struct ieee80211com *ic , u_int32_t tid)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    ol_txrx_soc_handle soc_txrx_handle;
    ol_txrx_pdev_handle pdev_txrx_handle;

    soc_txrx_handle = wlan_psoc_get_dp_handle(scn->soc->psoc_obj);
    if (!soc_txrx_handle) {
        QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_ERROR,
                "%s : %d : soc_txrx_handle is NULL", __func__, __LINE__);
        return;
    }

    pdev_txrx_handle = wlan_pdev_get_dp_handle(scn->sc_pdev);
    if (!pdev_txrx_handle) {
        QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_ERROR,
                "%s : %d : pdev_txrx_handle is NULL", __func__, __LINE__);
        return;
    }

    /* If Override is disabled: send default TID  else passed the intended tid value */
    if(!ic->ic_override_hmmc_dscp) {
        tid = 0xFF;                 //TBD::Replace it with proper macro
    }

    cdp_set_hmmc_tid_val(soc_txrx_handle, (struct cdp_pdev *)pdev_txrx_handle, tid);

    ic->ic_dscp_hmmc_tid = tid;

    /* Send a message to NSS */
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    if (ic->nss_radio_ops->ic_nss_ol_set_hmmc_dscp_tid) {
        ic->nss_radio_ops->ic_nss_ol_set_hmmc_dscp_tid(scn, tid);
    }
#endif
    return;
}

u_int32_t
ol_ath_get_hmmc_tid(struct ieee80211com *ic)
{
    return ic->ic_dscp_hmmc_tid;
}

void
ol_ath_set_hmmc_dscp_override(struct ieee80211com *ic , u_int32_t val)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    ol_txrx_soc_handle soc_txrx_handle;
    ol_txrx_pdev_handle pdev_txrx_handle;

    /* If nothing to do just return */
    if(ic->ic_override_hmmc_dscp == val) {
        return;
    }
    ic->ic_override_hmmc_dscp = !!val;

    soc_txrx_handle = wlan_psoc_get_dp_handle(scn->soc->psoc_obj);
    if (!soc_txrx_handle) {
        QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_ERROR,
                "%s : %d : soc_txrx_handle is NULL", __func__, __LINE__);
        return;
    }

    pdev_txrx_handle = wlan_pdev_get_dp_handle(scn->sc_pdev);
    if (!pdev_txrx_handle) {
        QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_ERROR,
                "%s : %d : pdev_txrx_handle is NULL", __func__, __LINE__);
        return;
    }

    cdp_hmmc_tid_override_en(soc_txrx_handle, (struct cdp_pdev *)pdev_txrx_handle, !!val);

    /* Send a message to NSS */
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    if (ic->nss_radio_ops->ic_nss_ol_set_hmmc_dscp_override) {
        ic->nss_radio_ops->ic_nss_ol_set_hmmc_dscp_override(scn, val);
    }
#endif
    return;
}

u_int32_t
ol_ath_get_hmmc_dscp_override(struct ieee80211com *ic)
{
    return ic->ic_override_hmmc_dscp;
}

#endif

void ol_wlan_txpow_mgmt(struct ieee80211vap *vap,u_int8_t subtype)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    struct wlan_objmgr_psoc *psoc;
    ol_txrx_vdev_handle vdev = (ol_txrx_vdev_handle)wlan_vdev_get_dp_handle(vap->vdev_obj);
    int retval = EOK;

    if (subtype == IEEE80211_FC0_SUBTYPE_BEACON) {
        retval = wmi_txpower_vap_beacon(scn,vap->iv_txpow_mgt_frm[(subtype >> IEEE80211_FC0_SUBTYPE_SHIFT)], vap);
    }
    if (retval == EOK){
        psoc = wlan_vdev_get_psoc(vap->vdev_obj);
        cdp_set_mgmt_tx_power(wlan_psoc_get_dp_handle(psoc), (struct cdp_vdev *)vdev,
             subtype,
             vap->iv_txpow_mgt_frm[(subtype >> IEEE80211_FC0_SUBTYPE_SHIFT)]);
    }
return;
}

#if ATH_SUPPORT_HYFI_ENHANCEMENTS
static void ol_ieee80211_me_hifitbl_update_target(wlan_if_t vap)
{
    int i, j;
    int group_count = 0, node_count = 0;
    int action = IGMP_ACTION_ADD_MEMBER;
    int wildcard = IGMP_WILDCARD_SINGLE;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(vap->iv_ic);
    struct ieee80211_me_hifi_table *ht = &vap->iv_me->me_hifi_table;
    struct ieee80211_me_hifi_node *node;
    char *grpaddr;
    int grpaddrlen;
    struct wlan_objmgr_psoc *psoc = NULL;
    target_resource_config *tgt_cfg;

    psoc = scn->soc->psoc_obj;
    tgt_cfg = lmac_get_tgt_res_cfg(psoc);
    if (!tgt_cfg) {
        qdf_info("%s: psoc target res cfg is null", __func__);
        return;
    }

    group_count = vap->iv_me->me_hifi_table.entry_cnt;

    if( group_count != 0)
    {
        vap->iv_ic->ic_mcast_group_update(vap->iv_ic, IGMP_ACTION_DELETE_MEMBER, IGMP_WILDCARD_ALL, NULL, 0, NULL, 0, 0, NULL, NULL, vap->iv_unit);
        if(group_count > tgt_cfg->num_mcast_groups)
            group_count = tgt_cfg->num_mcast_groups;

        for(i = 0; i < group_count ; i++) {
            node_count = ht->entry[i].node_cnt;

            if(node_count > tgt_cfg->num_mcast_table_elems)
                node_count = tgt_cfg->num_mcast_table_elems;

            grpaddr = (int8_t *)(&ht->entry[i].group.u);
            grpaddrlen = (ht->entry[i].group.pro == ETHERTYPE_IP) ? IGMP_IP_ADDR_LENGTH : 16;
            for(j=0; j< node_count; j++){
                node = &ht->entry[i].nodes[j];
                vap->iv_ic->ic_mcast_group_update(vap->iv_ic, action, wildcard,
                        grpaddr, grpaddrlen, node->mac, node->filter_mode, node->nsrcs, node->srcs, NULL, vap->iv_unit);
            }
        }
    } else {
        vap->iv_ic->ic_mcast_group_update(vap->iv_ic, IGMP_ACTION_DELETE_MEMBER, IGMP_WILDCARD_ALL, NULL, 0, NULL, 0, 0, NULL, NULL, vap->iv_unit);
    }
}
#endif

#if ATH_PROXY_NOACK_WAR

static OS_TIMER_FUNC(ol_proxy_ast_reserve_timeout)
{
    struct ieee80211com *ic;
    OS_GET_TIMER_ARG(ic, struct ieee80211com *);

    if (ic->proxy_ast_reserve_wait.blocking) {
         if(qdf_atomic_dec_and_test(&ic->ic_ast_reserve_event)) {
            ic->ic_ast_reserve_status=-2;
            qdf_semaphore_release(&(ic->proxy_ast_reserve_wait.sem_ptr));
         }
    }
}

static int
ol_ath_pdev_proxy_ast_reserve(struct ol_ath_softc_net80211 *scn, u_int8_t *macaddr)
{
    struct proxy_ast_reserve_params param;
    struct common_wmi_handle *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    qdf_mem_set(&param, sizeof(param), 0);
    param.macaddr = macaddr;

    return wmi_unified_proxy_ast_reserve_cmd_send(pdev_wmi_handle, &param);
}

int
ol_ath_pdev_proxy_ast_reserve_event_handler (ol_scn_t sc, u_int8_t *data, u_int32_t datalen)
{
    ol_ath_soc_softc_t *soc = (ol_ath_soc_softc_t *) sc;
    struct wmi_host_proxy_ast_reserve_param ev;
    struct ieee80211com *ic;
    struct common_wmi_handle *wmi_handle;
    struct wlan_objmgr_pdev *pdev;

    wmi_handle = lmac_get_wmi_hdl(soc->psoc_obj);

    wmi_handle = lmac_get_wmi_hdl(soc->psoc_obj);

    if (wmi_extract_pdev_reserve_ast_ev_param(wmi_handle, data, &ev)) {
        return -1;
    }
    pdev = wlan_objmgr_get_pdev_by_id(soc->psoc_obj, PDEV_UNIT(ev.pdev_id),
                                   WLAN_MLME_SB_ID);
    if(pdev == NULL) {
         qdf_info("%s: pdev object (id: %d) is NULL", __func__,
                   PDEV_UNIT(ev.pdev_id));
         return -1;
    }

    ic = wlan_pdev_get_mlme_ext_obj(pdev);
    if (ic == NULL) {
        qdf_info("ic (id: %d) is NULL ", PDEV_UNIT(ev.pdev_id));
        wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
        return -1;
    }

    OS_CANCEL_TIMER(&(ic->ic_ast_reserve_timer));

    if (ic->proxy_ast_reserve_wait.blocking) {
        if(qdf_atomic_dec_and_test(&ic->ic_ast_reserve_event)) {
           ic->ic_ast_reserve_status = ev.result;
           qdf_info("Status received from FW: func: %s ,event->result=%d, ic->ic_ast_reserve_status=%d \n",__func__,ev.result, ic->ic_ast_reserve_status);
           qdf_semaphore_release(&(ic->proxy_ast_reserve_wait.sem_ptr));
        }
    }
    wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
    return 0;
}

int32_t ol_ioctl_get_proxy_noack_war(struct ol_ath_softc_net80211 *scn, caddr_t param)
{
    int war_enable;
    struct qwrap_config_t *qwrap_config = (struct qwrap_config_t *)param;

    if(scn->sc_proxy_noack_war) {
        war_enable = 1;
    } else {
        war_enable = 0;
    }
    qwrap_config->proxy_noack_war = war_enable;
    return 0;
}

int32_t ol_ioctl_reserve_proxy_macaddr(struct ol_ath_softc_net80211 *scn, caddr_t *param)
{
    struct ieee80211com *ic = &scn->sc_ic;
    int error = 0;
    struct qwrap_config_t *qwrap_config = (struct qwrap_config_t *)param;
    struct {
        u8 addr[6];
        int status;
    } psta_addr_reserve;

    if(!scn->sc_proxy_noack_war) {
        return -1;
    }

    ic->ic_ast_reserve_status= -1;

    if(OS_CANCEL_TIMER(&(ic->ic_ast_reserve_timer))) {
        error = _copy_to_user((caddr_t)param , &psta_addr_reserve, sizeof(psta_addr_reserve));
        return error;
    }

    qdf_info("%s mac_addr =%s  ",__func__,ether_sprintf(qwrap_config->addr));

    qdf_atomic_init(&(ic->ic_ast_reserve_event));

    qdf_atomic_inc(&(ic->ic_ast_reserve_event));

    OS_SET_TIMER(&ic->ic_ast_reserve_timer, TARGET_AST_RESERVE_TIMEOUT *2000);

    ol_ath_pdev_proxy_ast_reserve(scn, qwrap_config->addr);

    if (ic->proxy_ast_reserve_wait.blocking) {
       qdf_semaphore_acquire(&(ic->proxy_ast_reserve_wait.sem_ptr));
    }

    qwrap_config->status = ic->ic_ast_reserve_status;

    error = _copy_to_user((caddr_t)param , qwrap_config, sizeof(psta_addr_reserve));

    qdf_info("%s  status=%d error = %d",__func__,qwrap_config->status, error);

    return error;
}
#endif

#if ATH_SUPPORT_WRAP && DBDC_REPEATER_SUPPORT
void ol_ioctl_disassoc_clients(struct ol_ath_softc_net80211 *scn)
{
	struct ieee80211com *ic = &scn->sc_ic;
	struct ieee80211vap           *tmp_vap;
	struct ieee80211com           *tmp_ic;
	int i=0;

	for (i=0; i < MAX_RADIO_CNT; i++) {
		tmp_ic = ic->ic_global_list->global_ic[i];
		if (tmp_ic) {
			TAILQ_FOREACH(tmp_vap, &tmp_ic->ic_vaps, iv_next) {
				if ((tmp_vap->iv_opmode == IEEE80211_M_HOSTAP) &&
				    (wlan_vdev_is_up(tmp_vap->vdev_obj) == QDF_STATUS_SUCCESS)) {
					wlan_iterate_station_list(tmp_vap, sta_disassoc, NULL);
				}
			}
		}
	}
}
#endif


#if  ATH_SUPPORT_WRAP && DBDC_REPEATER_SUPPORT
int32_t ol_ioctl_get_primary_radio(struct ol_ath_softc_net80211 *scn, caddr_t param)
{
    int primary_radio;
    struct ieee80211com *ic = &scn->sc_ic;
    struct qwrap_config_t *qwrap_config = (struct qwrap_config_t *)param;

    if(ic->ic_primary_radio) {
        primary_radio = 1;
    } else {
        primary_radio = 0;
    }
    qwrap_config->primary_radio = primary_radio;
    return 0;
}

int32_t ol_ioctl_get_mpsta_mac_addr(struct ol_ath_softc_net80211 *scn, caddr_t param)
{
    struct ieee80211com *ic = &scn->sc_ic;
    struct ieee80211vap *mpsta_vap;
    struct qwrap_config_t *qwrap_config = (struct qwrap_config_t *)param;

    if (ic->ic_mpsta_vap == NULL)
    {
        return -EFAULT;
    }
    mpsta_vap = ic->ic_mpsta_vap;
    qdf_mem_copy(qwrap_config->addr, mpsta_vap->iv_myaddr, IEEE80211_ADDR_LEN);
    return 0;
}

int32_t ol_ioctl_get_force_client_mcast(struct ol_ath_softc_net80211 *scn, caddr_t param)
{
    int force_client_mcast;
    struct ieee80211com *ic = &scn->sc_ic;
    struct qwrap_config_t *qwrap_config = (struct qwrap_config_t *)param;

    if(ic->ic_global_list->force_client_mcast_traffic) {
        force_client_mcast = 1;
    } else {
        force_client_mcast = 0;
    }

    qwrap_config->force_client_mcast = force_client_mcast;
    return 0;
}

int32_t ol_ioctl_get_max_priority_radio(struct ol_ath_softc_net80211 *scn, caddr_t param)
{
    char wifi_iface[IFNAMSIZ];
    struct ieee80211com *ic = &scn->sc_ic;
    struct global_ic_list   *glist = ic->ic_global_list;
    struct ieee80211vap           *max_priority_stavap_up;
    struct ieee80211com *max_priority_ic;
    struct net_device *n_dev;
    struct qwrap_config_t *qwrap_config = (struct qwrap_config_t *)param;

    memset(&wifi_iface[0], 0, sizeof(wifi_iface));
    GLOBAL_IC_LOCK_BH(glist);
    max_priority_stavap_up = glist->max_priority_stavap_up;
    GLOBAL_IC_UNLOCK_BH(glist);
    if (max_priority_stavap_up &&
        (wlan_vdev_is_up(max_priority_stavap_up->vdev_obj) == QDF_STATUS_SUCCESS)) {
	max_priority_ic = max_priority_stavap_up->iv_ic;
	n_dev = max_priority_ic->ic_netdev;
	OS_MEMCPY(wifi_iface, n_dev->name, sizeof(wifi_iface));
	qdf_info("max priority radio:%s",wifi_iface);
    }
    memcpy(&qwrap_config->max_priority_radio, wifi_iface, IFNAMSIZ);
    return 0;
}

void ol_ioctl_iface_mgr_status(struct ol_ath_softc_net80211 *scn, caddr_t param)
{
    struct ieee80211com *ic = &scn->sc_ic;
    struct iface_config_t *iface_config = (struct iface_config_t *)param;
    u_int8_t iface_mgr_up = iface_config->iface_mgr_up;

    GLOBAL_IC_LOCK_BH(ic->ic_global_list);
    if ((iface_mgr_up) == 1) {
        ic->ic_global_list->iface_mgr_up = 1;
        qdf_info("setting iface_mgr_up to 1");
    } else {
        ic->ic_global_list->iface_mgr_up = 0;
        qdf_info("setting iface_mgr_up to 0");
    }
    GLOBAL_IC_UNLOCK_BH(ic->ic_global_list);
}
#endif

u_int8_t ol_ioctl_get_stavap_connection(struct ol_ath_softc_net80211 *scn, caddr_t param)
{
    u_int8_t stavap_up;
    struct ieee80211com *ic = &scn->sc_ic;
    struct ieee80211vap *stavap = ic->ic_sta_vap;
    struct iface_config_t *iface_config = (struct iface_config_t *)param;

    if (!stavap) {
        qdf_info("stavap not up for: %pK", scn);
        return -EINVAL;
    }

    if (wlan_vdev_is_up(stavap->vdev_obj) == QDF_STATUS_SUCCESS) {
        stavap_up = 1;
    } else {
        stavap_up = 0;
    }
    iface_config->stavap_up = stavap_up;
    return 0;
}

#if DBDC_REPEATER_SUPPORT
u_int16_t ol_ioctl_get_disconnection_timeout(struct ol_ath_softc_net80211 *scn, caddr_t param)
{
    u_int16_t timeout;
    struct ieee80211com *ic = &scn->sc_ic;
    struct iface_config_t *iface_config = (struct iface_config_t *)param;
    u_int16_t disconnect_timeout = ic->ic_global_list->disconnect_timeout;
    u_int16_t reconfiguration_timeout = ic->ic_global_list->reconfiguration_timeout;

    if (disconnect_timeout >= reconfiguration_timeout) {
        timeout = disconnect_timeout;
        qdf_info("disconnect_timeout:%d",disconnect_timeout);
    } else {
        timeout = reconfiguration_timeout;
        qdf_info("reconfiguration_timeout:%d",reconfiguration_timeout);
    }

    iface_config->timeout = timeout;
    return 0;
}

int32_t ol_ioctl_get_preferred_uplink(struct ol_ath_softc_net80211 *scn, caddr_t param)
{
    int preferredUplink;
    struct ieee80211com *ic = &scn->sc_ic;
    struct iface_config_t *iface_config = (struct iface_config_t *)param;

    if(ic->ic_preferredUplink) {
        preferredUplink = 1;
    } else {
        preferredUplink = 0;
    }

    iface_config->is_preferredUplink = preferredUplink;
    return 0;
}
#endif

int32_t ol_ioctl_get_chan_vendorsurvey_info(struct ol_ath_softc_net80211 *scn,
        caddr_t param)
{
    int i;
    struct ieee80211com *ic = &scn->sc_ic;
    struct scan_chan_info *src_chan_info;
    struct extacs_chan_info *dest_chan_info = (struct extacs_chan_info *)param;
    struct external_acs_obj *extacs_handle = &ic->ic_extacs_obj;

    qdf_spin_lock(&extacs_handle->chan_info_lock);
    for (i = 0; i < NUM_MAX_CHANNELS; i++) {
        src_chan_info = &ic->ic_extacs_obj.chan_info[i];
        dest_chan_info->cycle_count = 0;
        dest_chan_info->channel_load = src_chan_info->chan_load;
        dest_chan_info->period = 0;
        dest_chan_info->noisefloor = src_chan_info->noise_floor;
        dest_chan_info->comp_usablity = 0;
        dest_chan_info->comp_usablity_sec80 = 0;
        dest_chan_info->max_regulatory_power = 0;
        dest_chan_info->max_regulatory_power_sec80 = 0;
        dest_chan_info->tx_power_tput = src_chan_info->tx_power_tput;
        dest_chan_info->tx_power_range = src_chan_info->tx_power_range;
        dest_chan_info++;
    }
    qdf_spin_unlock(&extacs_handle->chan_info_lock);

    return 0;
}

#if MESH_MODE_SUPPORT
extern void ieee80211_check_timeout_mesh_peer(void *arg, wlan_if_t vaphandle);
#endif
/*
 * Periodically check and cleanup nodes allocated for
 * non-associated clients.
 * Free the node if AP not received any ASSOC frames from client
 * after AUTH within the configured mlme timeout STA_NOASSOC_TIME
 */
static void ol_sta_noassoc_timeout(struct ieee80211_node_table *nt)
{
    struct ieee80211_node *ni, *ni_temp;
    systime_t now;
    u_int16_t associd;

    rwlock_state_t lock_state;
    OS_BEACON_DECLARE_AND_RESET_VAR(flags);
    OS_BEACON_WRITE_LOCK(&nt->nt_nodelock, &lock_state, flags);

    TAILQ_FOREACH_SAFE(ni, &nt->nt_node, ni_list, ni_temp) {

        /*
         * Special case ourself; we may be idle for extended periods
         * of time and regardless reclaiming our state is wrong.
         */
        if (ni == ni->ni_vap->iv_bss) {
            /* don't permit it to go negative */
            if (ni->ni_inact > 0)
                ni->ni_inact--;
            continue;
        }

        if ((ni->ni_associd != 0)
            || (ni->ni_authalg == IEEE80211_AUTH_ALG_FT)
            || (ni->ni_authalg == IEEE80211_AUTH_ALG_SAE)) {
            continue;
        }

        if (ni->ni_inact > 0)
            ni->ni_inact--;

#if UMAC_SUPPORT_NAWDS
        /* Never deauth the timeout NAWDS station.
         * But keep checking if it's still inactive.
         */
        if (ni->ni_flags & IEEE80211_NODE_NAWDS && ni->ni_inact <= 0) {
            ni->ni_inact = 1;
        }
#endif
        /*
         * Make sure to timeout STAs who have sent 802.11
         * authentication but not have associated.
         */
        if (ni->ni_inact <= 0) {
            IEEE80211_NOTE(ni->ni_vap, IEEE80211_MSG_INACT, ni,
                    "station timed out due to inactivity (refcnt %u) ni_macaddr:%s \n",
                    ieee80211_node_refcnt(ni),ether_sprintf(ni->ni_macaddr));
            /*
             * Send a deauthenticate frame and drop the station.
             * We grab a reference before unlocking the table so
             * the node cannot be reclaimed before we complete our
             * work.
             *
             * Separately we must drop the node lock before sending
             * in case the driver takes a lock, as this may result
             * in a LOR between the node lock and the driver lock.
             */
            ieee80211_ref_node(ni);
            OS_BEACON_WRITE_UNLOCK(&nt->nt_nodelock, &lock_state, flags);

            if (ni->ni_vap->iv_opmode == IEEE80211_M_IBSS) {
                ieee80211_sta_leave(ni);
            } else if (ni->ni_vap->iv_opmode != IEEE80211_M_STA) {
                associd = ni->ni_associd;
                now = OS_GET_TIMESTAMP();
                if( ni->ni_last_auth_rx_time != 0
                        && (CONVERT_SYSTEM_TIME_TO_MS(now - ni->ni_last_auth_rx_time) >
                            IEEE80211_STA_NOASSOC_TIME)) {
                    struct ieee80211vap * tmp_vap = ni->ni_vap;
                    IEEE80211_DPRINTF(tmp_vap, IEEE80211_MSG_AUTH,
                           "%s: sending DEAUTH to %s, timeout stations reason %d\n",
                           __func__, ether_sprintf(ni->ni_macaddr), IEEE80211_REASON_AUTH_EXPIRE);
                    wlan_mlme_deauth_request(tmp_vap,ni->ni_macaddr,IEEE80211_REASON_AUTH_EXPIRE);
                    IEEE80211_DELIVER_EVENT_MLME_DEAUTH_INDICATION(tmp_vap, ni->ni_macaddr,
                            associd, IEEE80211_REASON_AUTH_EXPIRE);
                }
            }
            OS_BEACON_WRITE_LOCK(&nt->nt_nodelock, &lock_state, flags);
            ieee80211_free_node(ni);
        }
    }
    OS_BEACON_WRITE_UNLOCK(&nt->nt_nodelock, &lock_state, flags);
}

/*
 * Per-ieee80211com timer callback.
 * used for checking any timeouts in the
 * COM device.
 */
static OS_TIMER_FUNC(ol_ath_timeout)
{
    struct ieee80211com *ic;
    OS_GET_TIMER_ARG(ic, struct ieee80211com *);
    ieee80211_session_timeout(&ic->ic_sta);
    ol_sta_noassoc_timeout(&ic->ic_sta);
    ieee80211_vap_mlme_inact_erp_timeout(ic);

#if MESH_MODE_SUPPORT
    /* check & delete timed out mesh peers */
    wlan_iterate_vap_list(ic, ieee80211_check_timeout_mesh_peer, NULL);
#endif

    OS_SET_TIMER(&ic->ic_inact_timer, IEEE80211_SESSION_WAIT*1000);
}

static int
ol_ath_set_mgmt_retry_limit(struct ieee80211com *ic , u_int8_t limit)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    int ret = 0;

    qdf_info("%s:%d Set mgmt retry limit to %d\n",__FUNCTION__,__LINE__,limit);
    ret = ol_ath_pdev_set_param(scn, wmi_pdev_param_mgmt_retry_limit, limit);
    if(ret){
        AR_DEBUG_PRINTF(ATH_DEBUG_ERR,("%s:%d Set mgmt retry limit failed!\n",__FUNCTION__,__LINE__));
        return ret;
    }
    scn->scn_mgmt_retry_limit = limit;
    return 0;
}

static u_int8_t
ol_ath_get_mgmt_retry_limit(struct ieee80211com *ic)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);

    return (scn->scn_mgmt_retry_limit);
}

static bool
ol_ath_support_phy_mode(struct ieee80211com *ic, enum ieee80211_phymode mode)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    u_int8_t cur_rx_mask = ieee80211com_get_rx_chainmask(ic);
    u_int8_t cur_tx_mask = ieee80211com_get_tx_chainmask(ic);
    bool rx_mask_is_valid = false;
    bool tx_mask_is_valid = false;
    uint32_t target_type;

    target_type = lmac_get_tgt_type(scn->soc->psoc_obj);

    /* For Lithium onwards */
    if (ol_target_lithium(scn->soc->psoc_obj)) {

        /* Validate TX & RX Chain mask for a given PHY mode */
        rx_mask_is_valid = ol_ath_validate_chainmask(scn, cur_rx_mask,
                VALIDATE_RX_CHAINMASK, mode);
        tx_mask_is_valid = ol_ath_validate_chainmask(scn, cur_tx_mask,
                VALIDATE_TX_CHAINMASK, mode);

        if (!rx_mask_is_valid || !tx_mask_is_valid)
            return false;
    } else {
        if ((mode == IEEE80211_MODE_11AC_VHT160) ||
                (mode == IEEE80211_MODE_11AC_VHT80_80) ||
                (mode == IEEE80211_MODE_11AXA_HE160) ||
                (mode == IEEE80211_MODE_11AXA_HE80_80)) {
            if (target_type == TARGET_TYPE_QCA9984) {
                /* Note: In case
                 * IEEE80211_MODE_11AXA_HE160/IEEE80211_MODE_11AXA_HE80_80 are used
                 * with TARGET_TYPE_QCA9984, then there is a separate code block
                 * which checks whether these modes are available for
                 * TARGET_TYPE_QCA9984 (via stubbing).
                 */

                /* 160 and 80+80 modes are allowed only with the following chainmasks
                 * in Cascade */
                switch (cur_rx_mask) {
                    case 0x5:
                    case 0x6:
                        /*
                         * We permit 0x7 for special case of 3x3 80MHz and 1x1 160MHz in
                         * QCA9984
                         */
                    case 0x7:
                    case 0x9:
                    case 0xa:
                    case 0xc:
                    case 0xf:
                        rx_mask_is_valid = true;
                        break;
                    default:
                        return false;
                }

                switch (cur_tx_mask) {
                    case 0x5:
                    case 0x6:
                        /*
                         * We permit 0x7 for special case of 3x3 80MHz and 1x1 160MHz in
                         * QCA9984
                         */
                    case 0x7:
                    case 0x9:
                    case 0xa:
                    case 0xc:
                    case 0xf:
                        if(rx_mask_is_valid)
                            return true;
                        else
                            return false;
                    default:
                        return false;
                }
            } else if (target_type == TARGET_TYPE_QCA9888 ) {
                switch (cur_rx_mask) {
                    /* 160 and 80+80 modes are allowed only with the following
                     * chainmasks in QCA9888 */
                    case 0x3:
                        return true;
                    default:
                        return false;
                }
            }
        }
    }
    /* Currently this function is used to validate 160/80+80 MHz vs. chainmask
     * compatibility for 160/80+80 MHz capable chipsets. Return true for other
     * cases.
     */
    return true;
}

static int
ol_ath_get_bw_nss_mapping(struct ieee80211vap *vap, struct ieee80211_bwnss_map *nssmap, u_int8_t chainmask)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    int error = 0;
    uint32_t target_type;
    enum ieee80211_phymode cur_mode;
    uint32_t iv_nss;

    target_type =  lmac_get_tgt_type(scn->soc->psoc_obj);

    /* Enabling STA max caps (if enabled through the flag) */
    if (vap->iv_sta_max_ch_cap) {
        cur_mode = vap->iv_des_mode;
    } else {
        cur_mode = vap->iv_cur_mode;
    }

    if ((cur_mode == IEEE80211_MODE_11AC_VHT160) ||
        (cur_mode == IEEE80211_MODE_11AC_VHT80_80) ||
        (cur_mode == IEEE80211_MODE_11AXA_HE160) ||
        (cur_mode == IEEE80211_MODE_11AXA_HE80_80)) {
        if (target_type == TARGET_TYPE_QCA9984) {
            /* 160 and 80+80 modes are allowed only with the following chainmasks
             * in QCA9984. */
            switch (chainmask) {
                case 0x5:
                case 0x6:
                /*
                 * We permit 0x7 for special case of 3x3 80MHz and 1x1 160MHz in
                 * QCA9984
                 */
                case 0x7:
                case 0x9:
                case 0xa:
                case 0xc:
                    nssmap->bw_nss_160 = IEEE80211_BW_NSS_MAP_1x1;
                    break;
                case 0xf:
                    nssmap->bw_nss_160 = IEEE80211_BW_NSS_MAP_2x2;
                    break;
                default:
                    error = -EINVAL;
            }
        } else if (target_type == TARGET_TYPE_QCA9888) {
            /* 160 and 80+80 modes are allowed only with the following chainmasks
             * in QCA9888.
             */
                switch (chainmask) {
                    case 0x3:
                        nssmap->bw_nss_160 = IEEE80211_BW_NSS_MAP_1x1;
                        break;
                    default:
                        error = -EINVAL;
                }
        } else if ((target_type == TARGET_TYPE_QCA8074) ||
			(target_type == TARGET_TYPE_QCA8074V2) ||
			(target_type == TARGET_TYPE_QCA6018)) {
            ieee80211_compute_nss(ic, chainmask, nssmap);
        } else if (target_type == TARGET_TYPE_QCA6290) {
            ieee80211_compute_nss(ic, chainmask, nssmap);
        } else {
            /* Currently this function is used to find valid 160MHz NSS map for
             * 160/80+80 MHz capable chipsets based on the rx_chainmask
             * configured. Return error for other cases */
            error = -EINVAL;
        }
    } else {
        /* Currently this function is used to find valid 160MHz NSS map for
         * 160/80+80 MHz capable chipsets based on the rx_chainmask configured.
         * Return error for other cases */
        error = -EINVAL;
    }

    wlan_util_vdev_mlme_get_param(vap->vdev_mlme,
            WLAN_MLME_CFG_NSS, &iv_nss);
    if ((error != -EINVAL) && (iv_nss != 0)) {

#define IS_FACTOR_1_2_OF_MAX_NSS(x, y) (((8 * x) - (5 * y)) <= 0)

        nssmap->bw_nss_160 = QDF_MIN(iv_nss, nssmap->bw_nss_160);
        if (nssmap->bw_nss_160 == iv_nss) {
            /* Same for all bandwidth */
            nssmap->flag = IEEE80211_NSSMAP_SAME_NSS_FOR_ALL_BW;
        } else if(IS_FACTOR_1_2_OF_MAX_NSS(nssmap->bw_nss_160, iv_nss)) {
            /* 1/2 case */
            nssmap->bw_nss_160 = iv_nss >> 1;
            nssmap->flag = IEEE80211_NSSMAP_1_2_FOR_160_AND_80_80;
        } else {
            /* 3/4 case */
            nssmap->flag = IEEE80211_NSSMAP_3_4_FOR_160_AND_80_80;
        }
    }

    return error;
}

static int
ol_ath_net80211_addba_responsesetup(struct ieee80211_node *ni, u_int8_t tidno,
	u_int8_t *dialogtoken, u_int16_t *statuscode,
	struct ieee80211_ba_parameterset *baparamset, u_int16_t *batimeout)
{
    struct ieee80211com *ic = ni->ni_ic;
    struct ieee80211vap *vap = ni->ni_vap;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    ol_txrx_soc_handle soc_txrx_handle;
    ol_txrx_peer_handle peer_txrx_handle;
    u_int16_t buffersize;
    uint32_t is_amsdu = 0;

    soc_txrx_handle = wlan_psoc_get_dp_handle(scn->soc->psoc_obj);
    peer_txrx_handle = wlan_peer_get_dp_handle(ni->peer_obj);
    if(!peer_txrx_handle)
        return QDF_STATUS_E_CANCELED;

    if (vap)
        wlan_util_vdev_mlme_get_param(vap->vdev_mlme,
                WLAN_MLME_CFG_AMSDU, &is_amsdu);

    if (is_amsdu)
        baparamset->amsdusupported = IEEE80211_BA_AMSDU_SUPPORTED;
    else
        baparamset->amsdusupported = 0;

    baparamset->bapolicy = IEEE80211_BA_POLICY_IMMEDIATE;
    baparamset->tid = tidno;
    cdp_addba_responsesetup(soc_txrx_handle, peer_txrx_handle,
              tidno, dialogtoken, statuscode, &buffersize, batimeout);
    baparamset->buffersize = buffersize;
    return 0;
}

static int
ol_ath_net80211_addba_resp_tx_completion(struct ieee80211_node *ni,
        u_int8_t tidno, int status)
{
    struct ieee80211com *ic = ni->ni_ic;
    ol_txrx_soc_handle soc_txrx_handle;
    ol_txrx_peer_handle peer_txrx_handle;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);

    soc_txrx_handle = wlan_psoc_get_dp_handle(scn->soc->psoc_obj);
    peer_txrx_handle = wlan_peer_get_dp_handle(ni->peer_obj);
    return cdp_addba_resp_tx_completion(soc_txrx_handle, peer_txrx_handle,
            tidno, status);
}

static int
ol_ath_net80211_addba_requestprocess( struct ieee80211_node *ni,
                u_int8_t dialogtoken,
                struct ieee80211_ba_parameterset *baparamset,
                u_int16_t batimeout,
                struct ieee80211_ba_seqctrl basequencectrl)
{
    struct ieee80211com *ic = ni->ni_ic;
    ol_txrx_soc_handle soc_txrx_handle;
    ol_txrx_peer_handle peer_txrx_handle;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);

    soc_txrx_handle = wlan_psoc_get_dp_handle(scn->soc->psoc_obj);
    peer_txrx_handle = wlan_peer_get_dp_handle(ni->peer_obj);
    if(!peer_txrx_handle)
        return QDF_STATUS_E_CANCELED;
    return cdp_addba_requestprocess(soc_txrx_handle, peer_txrx_handle,
                dialogtoken, baparamset->tid, batimeout,
                baparamset->buffersize, basequencectrl.startseqnum);
}

static void
ol_ath_net80211_delba_process(struct ieee80211_node *ni,
	struct ieee80211_delba_parameterset *delbaparamset, u_int16_t reasoncode)
{
    struct ieee80211com *ic = ni->ni_ic;
    ol_txrx_soc_handle soc_txrx_handle;
    ol_txrx_peer_handle peer_txrx_handle;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);

    soc_txrx_handle = wlan_psoc_get_dp_handle(scn->soc->psoc_obj);
    peer_txrx_handle = wlan_peer_get_dp_handle(ni->peer_obj);
    if(!peer_txrx_handle)
        return;
    cdp_delba_process(soc_txrx_handle, peer_txrx_handle,
                        delbaparamset->tid, reasoncode);
}

static int
ol_ath_net80211_delba_tx_completion(struct ieee80211_node *ni,
    u_int8_t tidno, int status)
{

    struct ieee80211com *ic = ni->ni_ic;
    ol_txrx_soc_handle soc_txrx_handle;
    ol_txrx_peer_handle peer_txrx_handle;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);

    soc_txrx_handle = wlan_psoc_get_dp_handle(scn->soc->psoc_obj);
    peer_txrx_handle = wlan_peer_get_dp_handle(ni->peer_obj);
    return cdp_delba_tx_completion(soc_txrx_handle, peer_txrx_handle,
                                  tidno, status);
}

static int ol_ath_get_target_phymome(struct ieee80211com *ic, uint32_t phymode,
                                     bool is_2gvht_en)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    return ol_get_phymode_info(scn, phymode, is_2gvht_en);
}

#ifdef WLAN_SUPPORT_TWT
static int ol_ath_twt_req(wlan_if_t vap, struct ieee80211req_athdbg *req)
{
    struct ol_ath_vap_net80211 *avn = OL_ATH_VAP_NET80211(vap);
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(vap->iv_ic);

    switch (req->cmd) {
    case IEEE80211_DBGREQ_TWT_ADD_DIALOG:
        {
            struct wmi_twt_add_dialog_param param = {0};

            if (req->data.twt_add.twt_cmd > WMI_HOST_TWT_COMMAND_REJECT_TWT) {
                qdf_info("TWT cmd %d is invalid", req->data.twt_add.twt_cmd);
                return -EINVAL;
            }

            param.vdev_id = avn->av_if_id;
            qdf_mem_copy(param.peer_macaddr, req->dstmac, 6);
            param.dialog_id = req->data.twt_add.dialog_id;
            param.wake_intvl_us = req->data.twt_add.wake_intvl_us;
            param.wake_intvl_mantis = req->data.twt_add.wake_intvl_mantis;
            param.wake_dura_us = req->data.twt_add.wake_dura_us;
            param.sp_offset_us = req->data.twt_add.sp_offset_us;
            param.twt_cmd = req->data.twt_add.twt_cmd;
            if (req->data.twt_add.flags & IEEE80211_TWT_FLAG_BCAST)
                param.flag_bcast = 1;
            if (req->data.twt_add.flags & IEEE80211_TWT_FLAG_TRIGGER)
                param.flag_trigger = 1;
            if (req->data.twt_add.flags & IEEE80211_TWT_FLAG_FLOW_TYPE)
                param.flag_flow_type = 1;
            if (req->data.twt_add.flags & IEEE80211_TWT_FLAG_PROTECTION)
                param.flag_protection = 1;

            wmi_unified_twt_add_dialog_cmd(lmac_get_pdev_wmi_handle(scn->sc_pdev), &param);
        }
        break;
    case IEEE80211_DBGREQ_TWT_DEL_DIALOG:
        {
            struct wmi_twt_del_dialog_param param = {0};

            param.vdev_id = avn->av_if_id;
            qdf_mem_copy(param.peer_macaddr, req->dstmac, 6);
            param.dialog_id = req->data.twt_del_pause.dialog_id;

            wmi_unified_twt_del_dialog_cmd(lmac_get_pdev_wmi_handle(scn->sc_pdev), &param);

        }
        break;
    case IEEE80211_DBGREQ_TWT_PAUSE_DIALOG:
        {
            struct wmi_twt_pause_dialog_cmd_param param;

            param.vdev_id = avn->av_if_id;
            qdf_mem_copy(param.peer_macaddr, req->dstmac, 6);
            param.dialog_id = req->data.twt_del_pause.dialog_id;

            wmi_unified_twt_pause_dialog_cmd(lmac_get_pdev_wmi_handle(scn->sc_pdev), &param);
        }
        break;
    case IEEE80211_DBGREQ_TWT_RESUME_DIALOG:
        {
            struct wmi_twt_resume_dialog_cmd_param param = {0};

            param.vdev_id = avn->av_if_id;
            qdf_mem_copy(param.peer_macaddr, req->dstmac, 6);
            param.dialog_id = req->data.twt_resume.dialog_id;
            param.sp_offset_us = req->data.twt_resume.sp_offset_us;
            param.next_twt_size = req->data.twt_resume.next_twt_size;

            wmi_unified_twt_resume_dialog_cmd(lmac_get_pdev_wmi_handle(scn->sc_pdev), &param);
        }
        break;
    default:
        qdf_info("Unknown option %d", req->cmd);
        return -EINVAL;
    };

    return EOK;
}
#endif

int
ol_ath_set_default_pcp_tid_map(struct ieee80211com *ic,
                               uint32_t pcp, uint32_t tid)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    ol_txrx_soc_handle soc_txrx_handle;
    ol_txrx_pdev_handle pdev_txrx_handle;

    soc_txrx_handle = wlan_psoc_get_dp_handle(scn->soc->psoc_obj);
    pdev_txrx_handle = wlan_pdev_get_dp_handle(scn->sc_pdev);

    return cdp_set_pdev_pcp_tid_map(soc_txrx_handle,
                                    (struct cdp_pdev *)pdev_txrx_handle, pcp, tid);
}

static int
ol_ath_set_vap_pcp_tid_map(struct ieee80211vap *vap, uint32_t pcp, uint32_t tid)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    ol_txrx_soc_handle soc_txrx_handle;
    QDF_STATUS ret;
    ol_txrx_vdev_handle vdev_handle =
                    (ol_txrx_vdev_handle)wlan_vdev_get_dp_handle(vap->vdev_obj);

    soc_txrx_handle = wlan_psoc_get_dp_handle(scn->soc->psoc_obj);

    ret = cdp_set_vdev_pcp_tid_map(soc_txrx_handle,
                                   (struct cdp_vdev *)vdev_handle, pcp, tid);
    return qdf_status_to_os_return(ret);
}

int
ol_ath_set_default_tidmap_prty(struct ieee80211com *ic, uint32_t val)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    ol_txrx_soc_handle soc_txrx_handle;
    ol_txrx_pdev_handle pdev_txrx_handle;
    QDF_STATUS ret;

    soc_txrx_handle = wlan_psoc_get_dp_handle(scn->soc->psoc_obj);
    pdev_txrx_handle = wlan_pdev_get_dp_handle(scn->sc_pdev);
    ret = cdp_set_pdev_tidmap_prty(soc_txrx_handle,
                                   (struct cdp_pdev *)pdev_txrx_handle, val);
    return qdf_status_to_os_return(ret);
}

static int
ol_ath_set_vap_tidmap_tbl_id(struct ieee80211vap *vap, uint32_t mapid)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    ol_txrx_soc_handle soc_txrx_handle;
    QDF_STATUS ret;
    ol_txrx_vdev_handle vdev_handle =
                    (ol_txrx_vdev_handle)wlan_vdev_get_dp_handle(vap->vdev_obj);

    soc_txrx_handle = wlan_psoc_get_dp_handle(scn->soc->psoc_obj);

    ret = cdp_set_vdev_tidmap_tbl_id(soc_txrx_handle,
                                     (struct cdp_vdev *)vdev_handle,
                                     (uint8_t)mapid);
    return qdf_status_to_os_return(ret);

}

static int
ol_ath_set_vap_tidmap_prty(struct ieee80211vap *vap, uint32_t val)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    ol_txrx_soc_handle soc_txrx_handle;
    QDF_STATUS ret;
    ol_txrx_vdev_handle vdev_handle =
                    (ol_txrx_vdev_handle)wlan_vdev_get_dp_handle(vap->vdev_obj);

    soc_txrx_handle = wlan_psoc_get_dp_handle(scn->soc->psoc_obj);

    ret = cdp_set_vdev_tidmap_prty(soc_txrx_handle,
                                   (struct cdp_vdev *)vdev_handle, val);
    return qdf_status_to_os_return(ret);
}
int
ol_ath_dev_attach(struct ol_ath_softc_net80211 *scn,
                   IEEE80211_REG_PARAMETERS *ieee80211_conf_parm)
{
    struct ieee80211com *ic = &scn->sc_ic;
    ol_ath_soc_softc_t *soc = scn->soc;
#if WLAN_SPECTRAL_ENABLE
    struct wmi_spectral_cmd_ops cmd_ops;
#endif
    int error = 0;
    spin_lock_init(&ic->ic_lock);
    spin_lock_init(&ic->ic_main_sta_lock);
    spin_lock_init(&ic->ic_addba_lock);
    IEEE80211_STATE_LOCK_INIT(ic);
    spin_lock_init(&ic->ic_beacon_alloc_lock);

    spin_lock_init(&ic->ic_state_check_lock);
    spin_lock_init(&ic->ic_radar_found_lock);

    /* attach channel width management */
    error = ol_ath_cwm_attach(scn);
    if (error) {
        qdf_info("%s : ol_ath_cwm_attach failed \n", __func__);
        return error;
    }

    /* XXX not right but it's not used anywhere important */
    ieee80211com_set_phytype(ic, IEEE80211_T_OFDM);

    /*
     * Set the Atheros Advanced Capabilities from station config before
     * starting 802.11 state machine.
     */
     /* TBD */

    /* this matches the FW default value */
    scn->arp_override = WME_AC_VO;
    scn->igmpmld_override = 0;
    scn->igmpmld_tid = 0;

    /* set default mgmt retry limit */
    scn->scn_mgmt_retry_limit = DEFAULT_MGMT_RETRY_LIMIT;

    /* Set the mac address */

    /* Setup Min frame size */
    ic->ic_minframesize = sizeof(struct ieee80211_frame_min);

    ic->id_mask_vap_downed[VDEV_BITMAP_SLOT0] = 0;
    ic->id_mask_vap_downed[VDEV_BITMAP_SLOT1] = 0;

    /*
     * Setup some device specific ieee80211com methods
     */
    ic->ic_init = ol_ath_init;
    ic->ic_reset_start = ol_ath_reset_start;
    ic->ic_reset = ol_ath_reset;
    ic->ic_reset_end = ol_ath_reset_end;
    ic->ic_wme.wme_update = ol_ath_wmm_update;
    ic->ic_txq_depth = ol_ath_txq_depth;
    ic->ic_txq_depth_ac = ol_ath_txq_depth_ac;
    ic->ic_chwidth_change = ol_net80211_chwidth_change;
    ic->ic_nss_change = ol_net80211_nss_change;
    ic->ic_ar900b_fw_test = ol_ath_ar900b_fw_test;
    ic->ic_fw_unit_test = ol_ath_fw_unit_test;
    ic->ic_coex_cfg = ol_ath_coex_cfg;
    ic->ic_set_ant_switch = ol_ath_set_ant_switch_tbl;
    ic->ic_set_ctrl_table = ol_ath_set_ctl_table;
    ic->ic_set_beacon_interval = ol_set_beacon_interval;
    ic->ic_set_sta_fixed_rate = ol_net80211_set_sta_fixed_rate;
    ic->ic_support_phy_mode = ol_ath_support_phy_mode;
    ic->ic_get_bw_nss_mapping = ol_ath_get_bw_nss_mapping;

    /* dummy scan start/end commands */
    ic->ic_scan_start = ol_ath_scan_start;
    ic->ic_scan_end = ol_ath_scan_end;
#if ATH_SUPPORT_VOW_DCS
	/* host side umac compiles with this flag, so we have no
	   option than writing this with flag, otherwise the other
	   drivers in oher OSes would fail*/
    ic->ic_disable_dcsim = ol_ath_disable_dcsim;
    ic->ic_enable_dcsim = ol_ath_enable_dcsim;
#endif
    ic->ic_disable_dcscw = ol_ath_disable_dcscw;
    ic->ic_dcs_restore = ol_ath_dcs_restore;
    ic->ic_fips_test = ol_ath_fips_test;
    /* Handles UTF commands from FTM daemon */
#if UMAC_SUPPORT_CFG80211
    ic->ic_ucfg_testmode_cmd = ol_ath_ucfg_utf_unified_cmd;
#endif
#ifdef WLAN_SUPPORT_TWT
    ic->ic_twt_req = ol_ath_twt_req;
#endif
    ol_ath_kbps_to_mcs(soc, ic);
    ol_ath_ratecode_to_kbps(soc, ic);
    ol_ath_get_supported_rates(soc, ic);
    /* Attach resmgr module */
    ol_ath_resmgr_attach(ic);

    ol_ath_power_attach(ic);

    /*
     * Attach ieee80211com object to net80211 protocal stack.
     */
    error = ieee80211_ifattach(ic, ieee80211_conf_parm);
    if (error) {
        qdf_info("ieee80211_ifattach failed error : %d\n", error);
    	return error;
    }

    qdf_spinlock_create(&ic->ic_channel_stats.lock);
     /*
     * Complete device specific ieee80211com methods init
     */
    ic->ic_set_channel = ol_ath_set_channel;
    ic->ic_enable_radar = ol_ath_net80211_enable_radar;

#if ATH_SUPPORT_DFS && ATH_SUPPORT_STA_DFS
    ic->ic_enable_sta_radar = NULL;
#endif

    ic->ic_pwrsave_set_state = ol_ath_pwrsave_set_state;
    ic->ic_mhz2ieee = ol_ath_mhz2ieee;
    ic->ic_get_noisefloor = ol_ath_get_noisefloor;
    ic->ic_get_chainnoisefloor = ol_ath_get_chainnoisefloor;
    ic->ic_set_txPowerLimit = ol_ath_setTxPowerLimit;
    ic->ic_get_common_power = ol_ath_get_common_power;
    ic->ic_get_TSF32        = ol_ath_getTSF32;
    ic->ic_rmgetcounters = ol_ath_getrmcounters;
    ic->ic_get_wpsPushButton = ol_ath_wpsPushButton;
    ic->ic_clear_phystats = ol_ath_clear_phystats;
    ic->ic_set_macaddr = ol_ath_set_macaddr;
    ic->ic_log_text = ol_ath_log_text;
    ic->ic_log_text_bh = ol_ath_log_text_bh;
    ic->ic_set_chain_mask = ol_ath_set_chain_mask;
    ic->ic_get_mfpsupport = ol_ath_getmfpsupport;
    ic->ic_set_hwmfpQos   = ol_ath_setmfpQos;
    ic->ic_get_tx_hw_retries  = ol_ath_get_tx_hw_retries;
    ic->ic_get_tx_hw_success  = ol_ath_get_tx_hw_success;
    ic->ic_rate_node_update = ol_ath_rate_node_update;
#if ATH_SUPPORT_IQUE
    ic->ic_set_acparams = ol_ath_set_acparams;
    ic->ic_set_rtparams = ol_ath_set_rtparams;
    ic->ic_get_iqueconfig = ol_ath_get_iqueconfig;
    ic->ic_set_hbrparams = ol_ath_set_hbrparams;
#endif
    ic->ic_set_config = ol_ath_set_config;
    ic->ic_set_config_enable = ol_ath_set_config_enable;
    ic->ic_set_safemode = ol_ath_set_safemode;
    ic->ic_set_privacy_filters = ol_ath_set_privacy_filters;
    /* rx monitor filter */
    ic->ic_set_rx_monitor_filter = ol_ath_set_rx_monitor_filter;

    ic->ic_addba_responsesetup = ol_ath_net80211_addba_responsesetup;
    ic->ic_addba_requestprocess = ol_ath_net80211_addba_requestprocess;
    ic->ic_addba_resp_tx_completion = ol_ath_net80211_addba_resp_tx_completion;
    ic->ic_delba_process = ol_ath_net80211_delba_process;
    ic->ic_delba_tx_completion = ol_ath_net80211_delba_tx_completion;
#ifdef ATH_SUPPORT_TxBF // For TxBF RC

#if IEEE80211_DEBUG_REFCNT
    ic->ic_ieee80211_find_node_debug = ieee80211_find_node_debug;
#else
    ic->ic_ieee80211_find_node = ieee80211_find_node;
#endif //IEEE80211_DEBUG_REFCNT
    ic->ic_v_cv_send = ol_ath_net80211_v_cv_send;
    ic->ic_txbf_alloc_key = ol_ath_net80211_txbf_alloc_key;
    ic->ic_txbf_set_key = ol_ath_net80211_txbf_set_key;
    ic->ic_txbf_del_key = ol_ath_net80211_txbf_del_key;
    ic->ic_init_sw_cv_timeout = ol_ath_net80211_init_sw_cv_timeout;
    ic->ic_set_txbf_caps = ol_ath_set_txbfcapability;
#ifdef TXBF_DEBUG
	ic->ic_txbf_check_cvcache = ol_ath_net80211_txbf_check_cvcache;
#endif
    ic->ic_txbf_stats_rpt_inc = ol_ath_net80211_txbf_stats_rpt_inc;
    ic->ic_txbf_set_rpt_received = ol_ath_net80211_txbf_set_rpt_received;
#endif
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
	ic->ic_hifitbl_update_target = ol_ieee80211_me_hifitbl_update_target;
#endif
    ic->ic_get_cur_chan_nf = ol_ath_net80211_get_cur_chan_noisefloor;
    ic->ic_get_cur_hw_nf = ol_ath_net80211_get_cur_hw_nf;
    ic->ic_is_target_lithium = ol_target_lithium;
    ic->ic_set_rxfilter = ol_ath_set_rxfilter;
    ic->ic_set_ctl_table = ol_ath_set_ctl_table;
    ic->ic_set_mimogain_table = ol_ath_set_mimogain_table;
    ic->ic_ratepwr_table_ops = ol_ath_ratepwr_table_ops;
    ic->ic_set_node_tpc = ol_ath_set_node_tpc;
    ic->ic_set_mgmt_retry_limit = ol_ath_set_mgmt_retry_limit;
    ic->ic_get_mgmt_retry_limit = ol_ath_get_mgmt_retry_limit;
#if ATH_SUPPORT_LOWI
    ic->ic_lowi_frame_send = ol_ath_lowi_data_req_to_fw;
#endif
    ic->ic_tr69_request_process = ol_ath_net80211_tr69_process_request;
    ic->wide_band_scan_enabled = ol_ath_wide_band_scan;
    ic->ic_get_target_phymome = ol_ath_get_target_phymome;
    if (ol_target_lithium(soc->psoc_obj))
        ic->ic_print_peer_refs = ol_ath_print_peer_refs;
    else
        ic->ic_print_peer_refs = NULL;
#if ATH_SUPPORT_DFS && QCA_DFS_NOL_VAP_RESTART
    ic->ic_pause_stavap_scan = 0;
#endif

    /*
     * In Offload case 'inactivity' is handled in the firmware.
     * So override the default function with a different one to handle other timeouts.
     */
    OS_INIT_TIMER(ic->ic_osdev, &(ic->ic_inact_timer), ol_ath_timeout,
            (void *) (ic), QDF_TIMER_TYPE_WAKE_APPS);

    ic->ic_nl_handle = NULL;
#if QCA_LTEU_SUPPORT
    ol_ath_nl_attach(ic);
#endif

    /*
     * pktlog scn initialization
     */
#ifndef REMOVE_PKT_LOG
    if(enable_pktlog_support)
        ol_pktlog_attach(scn);
#endif

#ifdef QCA_PARTNER_PLATFORM
    ic->partner_com_params.ipc_ol_txrx_ast_find_hash_find = ol_txrx_ast_find_hash_find;
    ic->partner_com_params.ipc_ol_txrx_peer_find_by_id = ol_txrx_peer_find_by_id;
    ic->partner_com_params.ipc_ol_ath_getvap = ol_ath_getvap;
#endif
#ifdef CONFIG_DP_TRACE
    ic->ic_dptrace_set_param = ol_ath_dptrace_setparam;
#endif
#if ATH_SUPPORT_IQUE
    ol_if_me_setup(ic);
#endif

#if WLAN_SPECTRAL_ENABLE
    cmd_ops.wmi_spectral_configure_cmd_send  = wmi_unified_vdev_spectral_configure_cmd_send;
    cmd_ops.wmi_spectral_enable_cmd_send     = wmi_unified_vdev_spectral_enable_cmd_send;
    wlan_register_wmi_spectral_cmd_ops(ic->ic_pdev_obj, &cmd_ops);
#endif

    ol_if_eeprom_attach(ic);

    ol_ath_wow_attach(ic);
#if  ATH_DATA_TX_INFO_EN
    ol_ath_stats_attach(ic);
#endif

#if ATH_OL_FAST_CHANNEL_RESET_WAR
    ol_ath_fast_chan_change(scn);
#endif

    ic->ic_no_vlan = 0;
    ic->ic_atf_logging = 0;
    ic->ic_set_pcp_tid_map = ol_ath_set_vap_pcp_tid_map;
    ic->ic_set_tidmap_tbl_id = ol_ath_set_vap_tidmap_tbl_id;
    ic->ic_set_tidmap_prty = ol_ath_set_vap_tidmap_prty;

    return EOK;
}

int
ol_asf_adf_attach(ol_ath_soc_softc_t *soc)
{
    asf_adf_attach();
    return EOK;
}

int
ol_asf_adf_detach(ol_ath_soc_softc_t *soc)
{
    return EOK;
}

static OS_TIMER_FUNC(ieee80211_mu_cap_mode_switch)
{
    struct     ieee80211com *ic;
    struct     ieee80211_node_table *nt;
    struct     ieee80211vap *vap = NULL;
    DEDICATED_CLIENT_MAC *dedicated, *temp;
    u_int8_t   i = 0;
    u_int16_t  associd;
    u_int16_t  total_mu_clients;

    MU_CAP_CLIENT_TYPE mu_type;
    ATH_LIST_HEAD(,DEDICATED_CLIENT_MAC)deauth_list;
    MU_CAP_WAR *war;
    LIST_INIT(&deauth_list);

    OS_GET_TIMER_ARG(vap, struct ieee80211vap *);

    /*mu_cap timer enrty*/
    ic = vap->iv_ic;
    nt = &ic->ic_sta;
    war = &vap->iv_mu_cap_war;
    qdf_spin_lock_bh(&war->iv_mu_cap_lock);

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ASSOC,"IN Dedicated client TIMER\n");
    if (!war->mu_cap_war) {
        /*
         * Kicking out the dedicated SU Clients is
         * the only valid timer action
         * when the WAR is disabled.
         * You would never want to kick out
         * Dedicated-MU enabled client when WAR is
         * disabled!!
         */
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ASSOC,
                "In timer while MUCAP-WAR is disabled\n");
        war->mu_timer_cmd = MU_CAP_TIMER_CMD_KICKOUT_SU_CLIENTS;
    }


    total_mu_clients = war->mu_cap_client_num[MU_CAP_CLIENT_NORMAL] +
        war->mu_cap_client_num[MU_CAP_DEDICATED_MU_CLIENT] +
        war->mu_cap_client_num[MU_CAP_DEDICATED_SU_CLIENT];

    if ((war->mu_timer_cmd == MU_CAP_TIMER_CMD_KICKOUT_DEDICATED) &&
            ((total_mu_clients != 1) ||
             (war->mu_cap_client_num[MU_CAP_DEDICATED_MU_CLIENT] != 1))) {
        /*
         * Even thought the timer was started with the
         * purpose of kicking out MU-1X1 clients,
         * now, the conditions are not correct for kicking
         * out this MU-1X1 client,
         * possibly because other MU-Capable client(s) have joined
         */
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ASSOC,
                "Kick-out to SU-Capable 2x2 not possible now\n");
        goto end_of_timer;
    }

    switch(war->mu_timer_cmd)
    {
        case MU_CAP_TIMER_CMD_KICKOUT_DEDICATED:
            {
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_ASSOC,
                        "KICKOUT-DEDICATED-MU-CLIENT\n");
                mu_type = MU_CAP_DEDICATED_MU_CLIENT;
                break;
            }
        case MU_CAP_TIMER_CMD_KICKOUT_SU_CLIENTS:
            {
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_ASSOC,
                        "KICKOUT-ALL-DEDICATED-SU-CLIENTS\n");
                mu_type = MU_CAP_DEDICATED_SU_CLIENT;
                break;
            }
        default:
            {
                goto end_of_timer;
            }
    }


    /*
     * Copy the clients which meet the kick-out conditions
     * to a local table, then deauth them
     * outside of the iv_mu_cap_lock
     * This way the function ieee80211_mu_cap_client_leave can be
     * put inside iv_mu_cap_lock again
     */
    for(i=0;i<total_mu_clients;i++)
    {
        if (war->mu_cap_client_flag[i] == mu_type)
        {
            dedicated =
            OS_MALLOC(ic->ic_osdev, sizeof(struct DEDICATED_CLIENT_MAC), 0);
            if (dedicated == NULL) {
                ieee80211_note(vap, IEEE80211_MSG_ANY, "ERROR!! Memory "
                    "allocation failed in %s\n", __func__);
                continue;
            }
            OS_MEMCPY(dedicated->macaddr, war->mu_cap_client_addr[i],
                      IEEE80211_ADDR_LEN);
            LIST_INSERT_HEAD(&deauth_list, dedicated, list);
        }

    }

end_of_timer:
    war->iv_mu_timer_state = MU_TIMER_STOP;
    qdf_spin_unlock_bh(&war->iv_mu_cap_lock);

    /*
     * We now release the lock and call the
     * deauth function. The lock will be acquired later on
     * when the deauth function results in ieee80211_mu_cap_client_leave
     */
    /*De-assoc each dedicated clients*/
    LIST_FOREACH_SAFE(dedicated, &deauth_list, list, temp) {
        struct ieee80211_node *ni = ieee80211_find_node(nt, dedicated->macaddr);
        LIST_REMOVE(dedicated, list);
        OS_FREE(dedicated);
        if (ni == NULL)
        {
            ieee80211_note(vap, IEEE80211_MSG_ANY,
                    "NI IS NULL AFTER MU-CAP-WAR TIMER LOCK! %s\n",
                    ether_sprintf(dedicated->macaddr));
            continue;
        }
        if (ni->ni_associd != 0 )
        {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_ASSOC,
                    "MUCAPWAR TIMER: Kicking out %s\n",
                            ether_sprintf(ni->ni_macaddr));
            associd = ni->ni_associd;
            wlan_mlme_deauth_request(ni->ni_vap,ni->ni_macaddr,
                    IEEE80211_REASON_UNSPECIFIED);
            IEEE80211_DELIVER_EVENT_MLME_DEAUTH_INDICATION(ni->ni_vap,
                    ni->ni_macaddr, associd, IEEE80211_REASON_UNSPECIFIED);
        }
        ieee80211_free_node(ni);
    }
}

void ieee80211_mucap_vattach(struct ieee80211vap *vap) {
   struct ieee80211com *ic = vap->iv_ic;
   int cnt;
   MU_CAP_WAR *war = &vap->iv_mu_cap_war;
   osdev_t os_handle = ic->ic_osdev;
   war->iv_mu_timer_state = MU_TIMER_STOP;

   OS_INIT_TIMER(os_handle, &war->iv_mu_cap_timer,
           ieee80211_mu_cap_mode_switch, vap, QDF_TIMER_TYPE_WAKE_APPS);

   qdf_spinlock_create(&war->iv_mu_cap_lock);
   war->mu_cap_timer_period = MU_TIMER_PERIOD;

   for(cnt=0;cnt<ARRAY_SIZE(war->dedicated_client_list);cnt++) {
       LIST_INIT(&war->dedicated_client_list[cnt]);
   }
   war->dedicated_client_number = 0;

   /* Disable MU-CAP-WAR by default */
   war->modify_probe_resp_for_dedicated =
   war->mu_cap_war = 0;
}

void ieee80211_mucap_vdetach(struct ieee80211vap *vap) {

   struct DEDICATED_CLIENT_MAC *dedicated_mac=NULL, *temp=NULL;
   int cnt;
   MU_CAP_WAR *war = &vap->iv_mu_cap_war;
   qdf_spin_lock_bh(&war->iv_mu_cap_lock);
   if (war->iv_mu_timer_state == MU_TIMER_PENDING)
      war->iv_mu_timer_state = MU_TIMER_STOP;
   OS_FREE_TIMER(&war->iv_mu_cap_timer);
   qdf_spin_unlock_bh(&war->iv_mu_cap_lock);
   for(cnt=0;cnt<ARRAY_SIZE(war->dedicated_client_list);cnt++) {
       LIST_FOREACH_SAFE(dedicated_mac, &war->dedicated_client_list[cnt],
                         list, temp) {

           war->dedicated_client_number--;
           LIST_REMOVE(dedicated_mac,list);
           OS_FREE(dedicated_mac);
       }
   }
   qdf_spinlock_destroy(&war->iv_mu_cap_lock);
}

static void ol_swap_seg_free(ol_ath_soc_softc_t *soc, struct swap_seg_info *seg_info, u_int64_t *cpuaddr, int type)
{
    if(cpuaddr) {
#if WIFI_MEM_MANAGER_SUPPORT
        wifi_cmem_free(soc->soc_idx, 0, (CM_CODESWAP + type), seg_info->seg_cpuaddr[0]);
#else
        dma_free_coherent(soc->sc_osdev->device, seg_info->seg_size, seg_info->seg_cpuaddr[0] , (dma_addr_t)seg_info->seg_busaddr[0]);
#endif
        qdf_dev_release_mem((struct qdf_dev *)soc->sc_osdev->device,
                            (struct qdf_devm *)seg_info);
    }
}
void
ol_codeswap_detach (ol_ath_soc_softc_t *soc) {
    ol_swap_seg_free(soc, soc->target_otp_codeswap_seginfo, soc->target_otp_codeswap_cpuaddr, ATH_TARGET_OTP_CODE_SWAP);
    ol_swap_seg_free(soc, soc->target_otp_dataswap_seginfo, soc->target_otp_dataswap_cpuaddr, ATH_TARGET_OTP_DATA_SWAP);
    ol_swap_seg_free(soc, soc->target_bin_codeswap_seginfo, soc->target_bin_codeswap_cpuaddr, ATH_TARGET_BIN_CODE_SWAP);
    ol_swap_seg_free(soc, soc->target_bin_dataswap_seginfo, soc->target_bin_dataswap_cpuaddr, ATH_TARGET_BIN_DATA_SWAP);
    ol_swap_seg_free(soc, soc->target_bin_utf_codeswap_seginfo, soc->target_bin_utf_codeswap_cpuaddr,ATH_TARGET_BIN_UTF_CODE_SWAP);
    ol_swap_seg_free(soc, soc->target_bin_utf_dataswap_seginfo, soc->target_bin_utf_dataswap_cpuaddr,ATH_TARGET_BIN_UTF_DATA_SWAP);
}

int
ol_mempools_attach(ol_ath_soc_softc_t *soc)
{
    if(qdf_mempool_init(soc->qdf_dev, &soc->mempool_ol_ath_vap,
         soc->max_vaps, sizeof(struct ol_ath_vap_net80211), 0)) {
         soc->mempool_ol_ath_vap = NULL;
         qdf_info("%s: ol_ath_vap memory pool init failed", __func__);
         goto fail1;
     }

    if(qdf_mempool_init(soc->qdf_dev, &soc->mempool_ol_ath_node,
        (CFG_MAX_TMPNODES + soc->max_vaps + soc->max_clients), sizeof(struct ol_ath_node_net80211), 0)) {
        soc->mempool_ol_ath_node = NULL;
        qdf_info("%s: ol_ath_node memory pool init failed", __func__);
        goto fail2;
    }
    return 0;

fail2:
    qdf_mempool_destroy(soc->qdf_dev, soc->mempool_ol_ath_vap);
fail1:
    return -ENOMEM;
}

void
ol_mempools_detach(ol_ath_soc_softc_t *soc)
{
    qdf_mempool_destroy(soc->qdf_dev, soc->mempool_ol_ath_peer);
    qdf_mempool_destroy(soc->qdf_dev, soc->mempool_ol_ath_node);
    qdf_mempool_destroy(soc->qdf_dev, soc->mempool_ol_ath_vap);
}

/* Get default value of 80+80 EMI WAR enabled status based on chipset ID */
static u_int8_t
ol_ath_get_emiwar_80p80_defval(ol_ath_soc_softc_t *soc)
{
    u_int8_t defval=0;
    u_int32_t target_type = lmac_get_tgt_type(soc->psoc_obj);

    if (!IS_EMIWAR_80P80_APPLICABLE(target_type)) {
        return 0;
    }

    switch(target_type)
    {
        case TARGET_TYPE_QCA9984:
            defval = EMIWAR_80P80_DEFVAL_QCA9984;
            break;
        case TARGET_TYPE_QCA9888:
            defval = EMIWAR_80P80_DEFVAL_QCA9888;
            break;
        default:
            defval = 0;
            break;
    }

    return defval;
}

int ol_ath_handle_wmi_message(ol_scn_t sc, void *ev, uint8_t rx_ctx)
{
    void __wmi_control_rx(struct wmi_unified *wmi_handle, wmi_buf_t evt_buf);
    ol_ath_soc_softc_t *soc = (ol_ath_soc_softc_t *) sc;
    struct common_wmi_handle *wmi_handle;

    wmi_handle = lmac_get_wmi_hdl(soc->psoc_obj);
    __wmi_control_rx((wmi_unified_t)wmi_handle, ev);

    return 0;
}

static struct wmi_rx_ops rx_ops = {
    ol_ath_handle_wmi_message
};

QDF_STATUS
ol_ath_get_wmi_target_type(ol_ath_soc_softc_t *soc, enum wmi_target_type *target)
{
    uint32_t target_type;

    target_type = lmac_get_tgt_type(soc->psoc_obj);

	switch(target_type)
	{
		case TARGET_TYPE_AR6002:
		case TARGET_TYPE_AR6003:
		case TARGET_TYPE_AR6004:
		case TARGET_TYPE_AR6006:
		case TARGET_TYPE_AR9888:
		case TARGET_TYPE_AR6320:
		case TARGET_TYPE_AR900B:
		case TARGET_TYPE_QCA9984:
		case TARGET_TYPE_QCA9888:
		case TARGET_TYPE_IPQ4019:
			*target = WMI_NON_TLV_TARGET;
			break;
#ifdef QCA_WIFI_QCA8074
		case TARGET_TYPE_QCA8074:
		case TARGET_TYPE_QCA8074V2:
			*target = WMI_TLV_TARGET;
			break;
#endif
#ifdef QCA_WIFI_QCA6290
		case TARGET_TYPE_QCA6290:
			*target = WMI_TLV_TARGET;
			break;
#endif
#ifdef QCA_WIFI_QCA6018
		case TARGET_TYPE_QCA6018:
			*target = WMI_TLV_TARGET;
			break;
#endif
		default:
			qdf_info("!!! Invalid Target Type %d !!!", target_type);
			return QDF_STATUS_E_INVAL;
	}
	return QDF_STATUS_SUCCESS;
}

static inline void ol_ath_configure_wmi_services(ol_ath_soc_softc_t *soc)
{
    uint8_t wmi_ep_count = 1;
    uint32_t target_type;
    struct target_psoc_info *tgt_psoc_info;

    tgt_psoc_info = (struct target_psoc_info *)wlan_psoc_get_tgt_if_handle(
						soc->psoc_obj);
    if (tgt_psoc_info == NULL) {
        qdf_info("%s: target_psoc_info is null ", __func__);
        return;
    }
    target_type = target_psoc_get_target_type(tgt_psoc_info);

    /* qca8074 supports upto 3 macs and hence one ep per wmi.
     * But is preferred mode is DBS, then there is no need to connect to
     * all three WMI eps
     */
    if ((target_type == TARGET_TYPE_QCA8074) ||
		    (target_type == TARGET_TYPE_QCA8074V2) ||
		    (target_type == TARGET_TYPE_QCA6018) ||
		    (target_type == TARGET_TYPE_QCA6290)) {
        switch (target_psoc_get_preferred_hw_mode(tgt_psoc_info)) {
            case WMI_HOST_HW_MODE_SINGLE:
                wmi_ep_count = 1;
                break;
            case WMI_HOST_HW_MODE_DBS:
                wmi_ep_count = 2;
                break;
            case WMI_HOST_HW_MODE_DBS_OR_SBS:
                wmi_ep_count = 2;
                break;
            case WMI_HOST_HW_MODE_DBS_SBS:
                wmi_ep_count = 3;
                break;
            default:
                wmi_ep_count = 3;
                break;
        }
    }

    htc_set_wmi_endpoint_count(target_psoc_get_htc_hdl(tgt_psoc_info),
                            wmi_ep_count);
}

int htc_wmi_init(ol_ath_soc_softc_t *soc)
{
    struct htc_init_info htcInfo;
    struct target_psoc_info *tgt_psoc_info;
    void *htc_handle;
    void *hif_hdl;
    /*
     * 5. Create HTC
     */
    OS_MEMZERO(&htcInfo,sizeof(htcInfo));
    htcInfo.pContext = soc;
    htcInfo.target_psoc = soc;
    if(soc->ol_if_ops->target_failure)
	    htcInfo.TargetFailure = soc->ol_if_ops->target_failure;
    htcInfo.TargetSendSuspendComplete = ol_target_send_suspend_complete;
    htc_global_credit_flow_disable();

    tgt_psoc_info = (struct target_psoc_info *)wlan_psoc_get_tgt_if_handle(
			soc->psoc_obj);
    if(tgt_psoc_info == NULL) {
    	qdf_info("%s: target_psoc_info is null ", __func__);
    	return -EIO;
    }

    hif_hdl = lmac_get_ol_hif_hdl(soc->psoc_obj);
    if ((htc_handle = htc_create(hif_hdl, &htcInfo, soc->qdf_dev,0)) == NULL)
    {
        return -EIO;
    }
    qdf_info("%s() HT Create . %pK", __func__, htc_handle);

    target_psoc_set_htc_hdl(tgt_psoc_info, htc_handle);

    ol_ath_configure_wmi_services(soc);

    if (!hif_needs_bmi(hif_hdl)) {
        qdf_info("%s() Skipping BMI Done. ", __func__);
    } else {
#ifdef WLAN_FEATURE_BMI
        /*
         * 6. Complete BMI phase
         */
        if (soc->bmi_handle) {
            if (soc->bmi_handle->bmi_done) {
                if (soc->bmi_handle->bmi_done(hif_hdl, soc->bmi_handle,soc->sc_osdev)
                                              != A_OK)
                {
                    return -EIO;
                }
            }
        }
#endif
    }

    if (!bypasswmi) {
        /*
         * 7. Initialize WMI
         */

        /* initialize default target config */
        if (lmac_is_target_ar900b(soc->psoc_obj)) {
            /* Beeliner supports HW IP header alignment,
             * so native WIFI support is disabled and host receives 802.3 packets
             * with IP header aligned to 4-byte boundary.
             */
            wlan_psoc_nif_feat_cap_clear(soc->psoc_obj,
                                    WLAN_SOC_F_HOST_80211_ENABLE);
            soc->nss_nwifi_offload = 0;
        }

        else if (target_psoc_get_target_type(tgt_psoc_info) == TARGET_TYPE_AR9888) {
            /* Peregrine does not support HW IP header alignment,
             * so native WIFI support should be enabled as IP header is always aligned
             * to 4-byte boundary in this mode.
             */
#if PERE_IP_HDR_ALIGNMENT_WAR
            wlan_psoc_nif_feat_cap_set(soc->psoc_obj,
                                    WLAN_SOC_F_HOST_80211_ENABLE);
#else
            wlan_psoc_nif_feat_cap_clear(soc->psoc_obj,
                                    WLAN_SOC_F_HOST_80211_ENABLE);
#endif

#if QCA_NSS_NWIFI_MODE
            soc->nss_nwifi_offload = 1;
#else
            soc->nss_nwifi_offload = 0;
#endif
        }
    }

    qdf_info("[%s:%d] host_enable %d nss_nwifi_offload %d", __func__, __LINE__,
              wlan_psoc_nif_feat_cap_get(soc->psoc_obj, WLAN_SOC_F_HOST_80211_ENABLE),
              soc->nss_nwifi_offload);
    target_if_set_default_config(soc->psoc_obj, tgt_psoc_info);

    return 0;
}

static void ol_ath_soc_rate_stats_attach(ol_ath_soc_softc_t *soc)
{
#ifdef QCA_SUPPORT_RDK_STATS
    struct wlan_soc_rate_stats_ctx *rate_stats_ctx = NULL;
    ol_txrx_soc_handle soc_txrx_handle;

    if (cfg_get(soc->psoc_obj, CFG_OL_PEER_RATE_STATS)) {
        soc_txrx_handle = wlan_psoc_get_dp_handle(soc->psoc_obj);

        rate_stats_ctx = qdf_mem_malloc(sizeof(*rate_stats_ctx));
        if (!rate_stats_ctx) {
            qdf_err("failed to allocate rate stats context");
            cdp_soc_configure_rate_stats(soc_txrx_handle, 0);
             return;
        }

        soc->wlanstats_enabled = 1;
        if (ol_target_lithium(soc->psoc_obj))
            rate_stats_ctx->is_lithium = 1;
        cdp_soc_configure_rate_stats(soc_txrx_handle, 1);
        rate_stats_ctx->soc = soc_txrx_handle;
        cdp_soc_set_rate_stats_ctx(soc_txrx_handle, rate_stats_ctx);
        qdf_info("enable rdk stats %d soc: %p ", soc->wlanstats_enabled, soc);
    }
#endif
}

static void ol_ath_soc_rate_stats_detach(ol_ath_soc_softc_t *soc)
{
#ifdef QCA_SUPPORT_RDK_STATS
    void *rate_stats_hdl;
    ol_txrx_soc_handle soc_txrx_handle;

    if (cfg_get(soc->psoc_obj, CFG_OL_PEER_RATE_STATS)) {
        soc->wlanstats_enabled = 0;
        soc_txrx_handle = wlan_psoc_get_dp_handle(soc->psoc_obj);
        cdp_soc_configure_rate_stats(soc_txrx_handle, 0);

        rate_stats_hdl = cdp_soc_get_rate_stats_ctx(soc_txrx_handle);
        if (rate_stats_hdl)
            qdf_mem_free(rate_stats_hdl);
        cdp_soc_set_rate_stats_ctx(soc_txrx_handle, NULL);
        qdf_warn("disable rdk stats");
    }
#endif
}

int ol_target_init_complete(ol_ath_soc_softc_t *soc)
{
    int status = 0;
    struct target_psoc_info *tgt_psoc_info;
    ol_txrx_soc_handle soc_txrx_handle = NULL;
    struct common_htc_handle *htc_handle;
    void *hif_hdl;
    dp_soc_txrx_handle_t *dp_ext_hdl = NULL;
    void *dbglog_handle = NULL;
    struct wmi_unified *wmi_handle = NULL;
    uint8_t num_radios;

    if (htc_wmi_init(soc))
	    goto attach_failed;

    htc_handle = lmac_get_htc_hdl(soc->psoc_obj);
    if (!htc_handle)
        goto attach_failed;

    hif_hdl = lmac_get_ol_hif_hdl(soc->psoc_obj);
    if (!hif_hdl)
        goto attach_failed;

    soc_txrx_handle = wlan_psoc_get_dp_handle(soc->psoc_obj);
#ifdef WLAN_FEATURE_FASTPATH
    hif_enable_fastpath(hif_hdl);
#endif

    if (htc_wait_target(htc_handle) != QDF_STATUS_SUCCESS) {
        status = -EIO;
        goto attach_failed;
    }
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    if (soc->nss_soc.ops) {
        soc->nss_soc.ops->nss_soc_ce_post_recv_buffer(soc);
    }
#endif

    if(soc->ol_if_ops->cdp_soc_init) {
        soc_txrx_handle = soc->ol_if_ops->cdp_soc_init(wlan_psoc_get_dp_handle(soc->psoc_obj),
			soc->device_id, hif_hdl,
                (void *)soc->psoc_obj, htc_handle,
                (void *)soc->qdf_dev, &dp_ol_if_ops);

        if (soc_txrx_handle == NULL) {

            qdf_info("%s: soc attach failed",__func__);
            status = -EIO;
            goto attach_failed;
        }
        wlan_psoc_set_dp_handle(soc->psoc_obj, soc_txrx_handle);
    }

    dp_ext_hdl = qdf_mem_malloc(sizeof(dp_soc_txrx_handle_t));
    if (dp_ext_hdl == NULL) {
        qdf_info("%s: soc attach - dp_ext_hdl alloc failed",__func__);
        status = -EIO;
        cdp_soc_set_dp_txrx_handle(soc_txrx_handle, NULL);
        goto attach_failed;
    }

    cdp_soc_set_dp_txrx_handle(soc_txrx_handle, dp_ext_hdl);
    dp_ext_hdl->lag_hdl.soc = soc_txrx_handle;
    ol_ath_soc_rate_stats_attach(soc);

    qdf_info("CDP soc attach success");

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    if (soc->nss_soc.ops) {
        soc->nss_soc.ops->nss_soc_wifi_set_cfg(soc, soc->nss_soc.nss_scfg);
    }
    qdf_info("Soc attach success NSS config %d ",
                cfg_get(soc->psoc_obj, CFG_NSS_WIFILI_OL));
#endif

    tgt_psoc_info = (struct target_psoc_info *)wlan_psoc_get_tgt_if_handle(
						soc->psoc_obj);
    if(tgt_psoc_info == NULL) {
        qdf_info("%s: target_psoc_info is null ", __func__);
        goto attach_failed;
    }

    if ((status = ol_ath_connect_htc(soc)) != A_OK)
    {
        qdf_info("%s: connect_htc failed",__func__);
        goto attach_failed;
    }

    if (!bypasswmi) {
        if(soc->ol_if_ops->dbglog_attach) {
            dbglog_handle = soc->ol_if_ops->dbglog_attach();
        }

        if(dbglog_handle) {
            target_psoc_set_dbglog_hdl(tgt_psoc_info, dbglog_handle);
            fwdbg_init(dbglog_handle, soc);
            soc->dbg_log_init = 1;
            wmi_handle = (struct wmi_unified *)target_psoc_get_wmi_hdl(tgt_psoc_info);
            if (wmi_handle) {
                wmi_unified_register_event_handler(wmi_handle, wmi_diag_event_id,
                        diag_fw_event_handler, WMI_RX_WORK_CTX);
            } else {
                qdf_err("null wmi_handle");
            }

        } else {
            qdf_info("%s: dbglog attach Failed", __func__);
        }

    }

    num_radios = target_psoc_get_num_radios(tgt_psoc_info);
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    if (soc->nss_soc.ops) {
        if ((status = soc->nss_soc.ops->nss_soc_wifi_attach(soc))) {
            qdf_info("%s: nss_soc_wifi_attach failed",__func__);
            goto attach_failed;
        }
        if (soc->nss_soc.nss_nxthop && (status = soc->nss_soc.ops->nss_soc_wifi_set_default_nexthop(soc))) {
            qdf_info("%s: nss_nxthop configuration failed",__func__);
            goto attach_failed;
        }
    }
#endif
    if (lmac_get_tgt_version(soc->psoc_obj) != AR6004_VERSION_REV1_3) {
        if ((status = cdp_soc_attach_target(soc_txrx_handle)) != A_OK) {
            qdf_info("%s: txrx soc attach failed",__func__);
            goto attach_failed;
        }
    }

    if (lmac_get_tgt_type(soc->psoc_obj) == TARGET_TYPE_QCA8074V2) {
        dp_ext_hdl->lag_hdl.ast_override_support = true;
    }

    return 0;
attach_failed:
    if (dp_ext_hdl) {
        qdf_mem_free(dp_ext_hdl);
        cdp_soc_set_dp_txrx_handle(soc_txrx_handle, NULL);
    }
    return status;
}

/* This function is to be used by components to populate
 * the OL function pointers (tx_ops) required by the component
 * for UMAC-LMAC interaction, with the appropriate handler */
QDF_STATUS olif_register_umac_tx_ops(struct wlan_lmac_if_tx_ops *tx_ops)
{
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
    ol_ath_register_crypto_ops_handler(&tx_ops->crypto_tx_ops);
#endif
#ifdef WLAN_SUPPORT_FILS
    tx_ops->mgmt_txrx_tx_ops.fd_action_frame_send = target_if_fd_send;
#endif
    tx_ops->mgmt_txrx_tx_ops.mgmt_tx_send = ol_if_mgmt_send;
    tx_ops->mgmt_txrx_tx_ops.beacon_send = ol_ath_mgmt_beacon_send;
    tx_ops->scan.set_chan_list = ol_scan_set_chan_list;
    /* DFS function pointers */
    tx_ops->dfs_tx_ops.dfs_enable = ol_if_dfs_enable;
    tx_ops->dfs_tx_ops.dfs_get_caps = ol_dfs_get_caps;
    tx_ops->dfs_tx_ops.dfs_gettsf64 = ol_if_get_tsf64;
    tx_ops->dfs_tx_ops.dfs_get_thresholds = ol_if_dfs_get_thresholds;
    tx_ops->dfs_tx_ops.dfs_disable = ol_if_dfs_disable;
    tx_ops->dfs_tx_ops.dfs_get_ext_busy = ol_if_dfs_get_ext_busy;
    tx_ops->dfs_tx_ops.dfs_get_target_type = lmac_get_pdev_target_type;
    tx_ops->dfs_tx_ops.dfs_get_ah_devid = ol_ath_get_ah_devid;
    tx_ops->dfs_tx_ops.dfs_get_phymode_info = ol_ath_get_phymode_info;
#if defined(WLAN_DFS_PARTIAL_OFFLOAD) && defined(HOST_DFS_SPOOF_TEST)
    tx_ops->dfs_tx_ops.dfs_host_dfs_check_support =
        ol_if_is_host_dfs_check_support_enabled;
#endif /* HOST_DFS_SPOOF_TEST */

    /* Regulatory tx ops */
    tx_ops->reg_ops.fill_umac_legacy_chanlist = ol_ath_fill_umac_legacy_chanlist;
    tx_ops->reg_ops.set_country_failed = ol_ath_set_country_failed;
    tx_ops->mops.get_wifi_iface_id = ol_ath_get_interface_id;

    return QDF_STATUS_SUCCESS;
}
qdf_export_symbol(olif_register_umac_tx_ops);

#ifdef WLAN_CONV_CRYPTO_SUPPORTED
static QDF_STATUS ol_defaultkey(struct wlan_objmgr_vdev *vdev,
                                      uint8_t keyidx, uint8_t *macaddr){

    struct wlan_objmgr_pdev *pdev;
    struct pdev_osif_priv *osif_priv;
    struct ol_ath_softc_net80211 *scn;

    if (vdev == NULL) {
        qdf_info("%s[%d] vdev is NULL",__func__, __LINE__);
        return QDF_STATUS_E_FAILURE;
     }

    pdev = wlan_vdev_get_pdev(vdev);

    if (pdev == NULL) {
        qdf_info("%s[%d] Pdev is NULL",__func__, __LINE__);
        return QDF_STATUS_E_FAILURE;
    }

    osif_priv = wlan_pdev_get_ospriv(pdev);

    scn = (struct ol_ath_softc_net80211 *)(osif_priv->legacy_osif_priv);

    if (scn == NULL) {
        qdf_info("%s[%d] scn is NULL",__func__, __LINE__);
        return QDF_STATUS_E_FAILURE;
    }

	ol_ath_wmi_send_vdev_param(scn,wlan_vdev_get_id(vdev),
                                          wmi_vdev_param_def_keyid, keyidx);
    return QDF_STATUS_SUCCESS;
}
static QDF_STATUS ol_deletekey(struct wlan_objmgr_vdev *vdev,
                               struct wlan_crypto_key *key, uint8_t *macaddr,
                               uint32_t keytype){

    struct wlan_objmgr_pdev *pdev;
    struct pdev_osif_priv *osif_priv;
    struct ol_ath_softc_net80211 *scn;
    struct ieee80211vap *vap;
    struct wlan_objmgr_psoc *psoc;

    if (vdev == NULL) {
        qdf_info("%s[%d] vdev is NULL",__func__, __LINE__);
        return QDF_STATUS_E_FAILURE;
    }
    pdev = wlan_vdev_get_pdev(vdev);

    if (pdev == NULL) {
        qdf_info("%s[%d] Pdev is NULL",__func__, __LINE__);
        return QDF_STATUS_E_FAILURE;
    }
    osif_priv = wlan_pdev_get_ospriv(pdev);

    scn = (struct ol_ath_softc_net80211 *)(osif_priv->legacy_osif_priv);

    if (scn == NULL) {
        qdf_info("%s[%d] scn is NULL",__func__, __LINE__);
        return QDF_STATUS_E_FAILURE;
    }

   vap = wlan_vdev_get_mlme_ext_obj(vdev);
   if (vap == NULL) {
        qdf_info("vap is NULL");
        return QDF_STATUS_E_FAILURE;
   }

   psoc = wlan_pdev_get_psoc(pdev);

   if (psoc && ol_target_lithium(psoc)) {
       ol_ath_vdev_install_key_send(vap, scn, wlan_vdev_get_id(vdev), key,
                                 macaddr, 0,0, keytype);
   } else {
       ol_ath_vdev_install_key_send(vap, scn, wlan_vdev_get_id(vdev), key,
                                 macaddr, 0,1, keytype);
   }
    return QDF_STATUS_SUCCESS;
}
static QDF_STATUS ol_setkey(struct wlan_objmgr_vdev *vdev,
                            struct wlan_crypto_key *key,
                            uint8_t *macaddr,  uint32_t keytype){

    struct wlan_objmgr_pdev *pdev;
    struct pdev_osif_priv *osif_priv;
    struct ol_ath_softc_net80211 *scn;
    struct ieee80211vap *vap;
#if ATH_SUPPORT_WRAP
    struct ol_ath_vap_net80211 *avn;
#endif

    if (vdev == NULL) {
        qdf_info("%s[%d] vdev is NULL",__func__, __LINE__);
        return QDF_STATUS_E_FAILURE;
    }
    pdev = wlan_vdev_get_pdev(vdev);

    if (pdev == NULL) {
        qdf_info("%s[%d] Pdev is NULL",__func__, __LINE__);
        return QDF_STATUS_E_FAILURE;
    }
    osif_priv = wlan_pdev_get_ospriv(pdev);

    scn = (struct ol_ath_softc_net80211 *)(osif_priv->legacy_osif_priv);

    if (scn == NULL) {
        qdf_info("%s[%d] scn is NULL",__func__, __LINE__);
        return QDF_STATUS_E_FAILURE;
    }

    vap = wlan_vdev_get_mlme_ext_obj(vdev);
    if (vap == NULL) {
        qdf_info("vap is NULL");
        return QDF_STATUS_E_FAILURE;
    }
#if ATH_SUPPORT_WRAP
    avn = OL_ATH_VAP_NET80211(vap);
    if (avn->av_is_psta && !(avn->av_is_mpsta) && (key->flags & WLAN_CRYPTO_KEY_GROUP)) {
        qdf_info("%s:Ignore set group key for psta",__func__);
        return QDF_STATUS_SUCCESS;
    }
#endif

    ol_ath_vdev_install_key_send(vap, scn, wlan_vdev_get_id(vdev), key,
               macaddr, (key->flags & WLAN_CRYPTO_KEY_DEFAULT), 0, keytype);
    return QDF_STATUS_SUCCESS;
}
static QDF_STATUS ol_allockey(struct wlan_objmgr_vdev *vdev,
                              struct wlan_crypto_key *key,
                              uint8_t *macaddr,  uint32_t keytype){
    return QDF_STATUS_SUCCESS;
}

/* OL callback to register crypto_ops handlers*/
static QDF_STATUS ol_ath_register_crypto_ops_handler(
                           struct wlan_lmac_if_crypto_tx_ops *crypto_tx_ops){

    crypto_tx_ops->setkey = ol_setkey;
    crypto_tx_ops->delkey = ol_deletekey;
    crypto_tx_ops->defaultkey = ol_defaultkey;
    crypto_tx_ops->allockey = ol_allockey;

    return QDF_STATUS_SUCCESS;
}
#endif /* WLAN_CONV_CRYPTO_SUPPORTED */

static
int unit_test_handler(ol_soc_t sc, uint8_t *data, uint32_t datalen)
{
    ol_ath_soc_softc_t *soc = (ol_ath_soc_softc_t *) sc;
    wmi_unit_test_event *unit_test;

    /*
     * Maximum buffer size from WMI is 2K
     */
    const uint32_t MAX_COPY_SIZE = 2048;
    const uint32_t MAX_BUFFER_SIZE = MAX_COPY_SIZE + sizeof(*unit_test);

    struct wlan_objmgr_psoc *psoc;
    struct wlan_objmgr_vdev *vdev;
    wlan_if_t vap;
    struct ieee80211req_athdbg_event *dbg_event;
    osif_dev *osifp;
    struct common_wmi_handle *wmi_handle;

    psoc = soc->psoc_obj;

    wmi_handle = lmac_get_wmi_hdl(psoc);
    unit_test = qdf_mem_malloc(sizeof(*unit_test)+MAX_BUFFER_SIZE);
    if (unit_test == NULL) {
        QDF_TRACE(QDF_MODULE_ID_DEBUG, QDF_TRACE_LEVEL_ERROR,
                "%s: Memory allocation failed for wmi_unit_test_event\n",
                __func__);
        return 0;
    }
    wmi_extract_unit_test(wmi_handle, data, unit_test, MAX_COPY_SIZE);
    vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, unit_test->vdev_id,
            WLAN_DEBUG_ID);
    if (vdev == NULL) {
        QDF_TRACE(QDF_MODULE_ID_DEBUG, QDF_TRACE_LEVEL_ERROR,
                FL("vdev object NULL"));
        goto cleanup1;
    }
    vap = wlan_vdev_get_mlme_ext_obj(vdev);
    if (vap == NULL) {
        QDF_TRACE(QDF_MODULE_ID_DEBUG, QDF_TRACE_LEVEL_ERROR,
                FL("vap object NULL"));
        goto cleanup2;
    }

    osifp = (osif_dev *)vap->iv_ifp;

    dbg_event =
        qdf_mem_malloc(sizeof(*dbg_event) +
                unit_test->buffer_len);
    if (dbg_event == NULL) {
        QDF_TRACE(QDF_MODULE_ID_DEBUG, QDF_TRACE_LEVEL_ERROR,
                "%s: Memory allocation failed for ieee80211req_athdbg_event\n",
                __func__);
        goto cleanup2;
    }
    dbg_event->cmd = IEEE80211_DBGREQ_FW_UNIT_TEST;
    dbg_event->fw_unit_test.hdr.module_id = unit_test->module_id;
    dbg_event->fw_unit_test.hdr.flag = unit_test->flag;
    dbg_event->fw_unit_test.hdr.diag_token = unit_test->diag_token;
    dbg_event->fw_unit_test.hdr.payload_len = unit_test->payload_len;
    dbg_event->fw_unit_test.hdr.buffer_len = unit_test->buffer_len;
    qdf_mem_copy(dbg_event->fw_unit_test.buffer, unit_test->buffer,
            unit_test->buffer_len);
#if UMAC_SUPPORT_CFG80211
    wlan_cfg80211_generic_event(vap->iv_ic,
            QCA_NL80211_VENDOR_SUBCMD_DBGREQ, dbg_event,
            sizeof(*dbg_event) + (unit_test->buffer_len-1),
            osifp->netdev, GFP_KERNEL);
#endif
    qdf_mem_free(dbg_event);

cleanup2:
    wlan_objmgr_vdev_release_ref(vdev, WLAN_DEBUG_ID);
cleanup1:
    qdf_mem_free(unit_test);
    return 0;
}

/*
 * Handle changes in state of network device.
 *
 */
static int ath_device_event(struct notifier_block *unused, unsigned long event, void *ptr)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0))
    struct net_device *dev = netdev_notifier_info_to_dev(ptr);
#else
    struct net_device *dev = ptr;
#endif
    struct ol_ath_softc_net80211 *scn = NULL;
    ol_txrx_soc_handle soc_txrx_handle = NULL;

    switch (event) {
        case NETDEV_FEAT_CHANGE:

            /* This api loops through all the vaps.
               Hence to avoid unnecessary handling of unrelated events
               this is called inside the event case
            */
            if (!(scn = osif_validate_and_get_scn_from_dev(dev))) {
                return NOTIFY_DONE;
            }

            soc_txrx_handle = wlan_psoc_get_dp_handle(scn->soc->psoc_obj);
            if (!soc_txrx_handle)
                return 0;

            if (dev->features & NETIF_F_TSO)
                cdp_tso_soc_attach(soc_txrx_handle);
            else if (!(dev->features & NETIF_F_TSO))
                cdp_tso_soc_detach(soc_txrx_handle);
            break;
        default:
            break;

    }
    return NOTIFY_DONE;
}

int
ol_ath_soc_attach(ol_ath_soc_softc_t *soc,
                  IEEE80211_REG_PARAMETERS *ieee80211_conf_parm,
                  ol_ath_update_fw_config_cb cfg_cb)
{
    int status = 0;
    struct mgmt_txrx_mgmt_frame_cb_info rx_cb_info;
    struct target_psoc_info *tgt_psoc_info = NULL;
    void *wmi_handle = NULL;
    ol_txrx_soc_handle soc_txrx_handle = NULL;
    void *dp_ext_hdl = NULL;
    struct wmi_unified_attach_params wmi_params = {0};
    struct common_htc_handle *htc_handle;

    wmi_params.osdev = soc->sc_osdev;
    wmi_params.rx_ops = &rx_ops;
    wmi_params.psoc = soc->psoc_obj;
    wmi_params.use_cookie = 0;
    wmi_params.max_commands = MAX_WMI_CMDS;
    wmi_params.soc_id = soc->soc_idx;
    wmi_params.is_async_ep = 1;
    wmi_params.enable_vdev_pdev_param_conversion = 1;

    soc->cfg_cb = cfg_cb;

    tgt_psoc_info = (struct target_psoc_info *)wlan_psoc_get_tgt_if_handle(
						soc->psoc_obj);
    if (tgt_psoc_info == NULL) {
        qdf_info("%s: target_psoc_info is null ", __func__);
        goto attach_failed;
    }

    /* detect low memory system */
    if (!cfg_get(soc->psoc_obj, CFG_OL_LOW_MEM_SYSTEM)) {
        if ((qdf_get_totalramsize() < LOW_MEM_SYSTEM_RAM) &&
                (target_psoc_get_target_type(tgt_psoc_info) == TARGET_TYPE_IPQ4019)) {
            //  low_mem_system is true
            soc->low_mem_system = true;
        }
    }

    if (soc->ol_if_ops->target_init) {
        status = soc->ol_if_ops->target_init(soc, true);
	if (status != EOK) {
            goto attach_failed;
        }
    }

    /* Get the WMI target type - TLV or Non-TLV based on target */
    if (ol_ath_get_wmi_target_type(soc, &wmi_params.target_type)) {
                status = -1;
                goto attach_failed;
    }

    /* Event handlers and extract APIs and init cmd use soc->wmi_handle
     * and other commands uses pdev_scn->wmi_handle
     * basically, events are recieved on single CE and hence it is all mapped to
     * soc's wmi_handle...this is same as first pdev's wmi_handle.
     */
    if ((wmi_handle = wmi_unified_attach(soc, &wmi_params)) == NULL)
    {
        qdf_info("%s() Failed to initialize WMI.", __func__);
        status = -1;
        goto attach_failed;
    }

    target_psoc_set_wmi_hdl(tgt_psoc_info, wmi_handle);

    soc->wmi_diag_version = 0;
    qdf_info("WMI attached. wmi_handle %pK ", wmi_handle);
    /* Enable pdev_id conversion in WMI */
    wmi_pdev_id_conversion_enable((wmi_unified_t)wmi_handle);

#if WMI_RECORDING
    wmi_proc_create(wmi_handle, soc->wmi_proc_entry, soc->soc_idx);
#endif

    if (dispatcher_psoc_open(soc->psoc_obj)) {
        qdf_info("%s() : Dispatcher open failed ", __func__);
        status = -1;
        goto attach_failed;
    }

    init_deinit_register_tgt_psoc_ev_handlers(soc->psoc_obj);

    wmi_unified_register_event_handler((wmi_unified_t)wmi_handle, wmi_debug_print_event_id,
                                            ol_ath_debug_print_event_handler, WMI_RX_UMAC_CTX);
    /* Initialize FIPS WMI Event */
    wmi_unified_register_event_handler((wmi_unified_t)wmi_handle, wmi_pdev_fips_event_id,
                                           ol_ath_fips_event_handler, WMI_RX_UMAC_CTX);

    wmi_unified_register_event_handler((wmi_unified_t)wmi_handle, wmi_tt_stats_event_id,
                          ol_ath_thermal_stats_event_handler, WMI_RX_UMAC_CTX);

    if (ol_target_lithium(soc->psoc_obj))
        osif_register_dev_ops_xmit(osif_ol_vap_hardstart_wifi3, OSIF_NETDEV_TYPE_OL_WIFI3);
    else
        osif_register_dev_ops_xmit(osif_ol_ll_vap_hardstart, OSIF_NETDEV_TYPE_OL);

    if (soc->ol_if_ops->cdp_soc_attach) {
	    soc_txrx_handle = soc->ol_if_ops->cdp_soc_attach(soc->device_id,
			    lmac_get_ol_hif_hdl(soc->psoc_obj),
			    (void *)soc->psoc_obj, lmac_get_htc_hdl(soc->psoc_obj),
			    (void *)soc->qdf_dev, &dp_ol_if_ops);
	    if (soc_txrx_handle == NULL) {

		    qdf_err("%s: soc attach failed\n",__func__);
		    status = -EIO;
		    goto attach_failed;
	    }
	    wlan_psoc_set_dp_handle(soc->psoc_obj, soc_txrx_handle);
    }

    if (ol_target_init_complete(soc)) {
        status = -1;
        goto target_init_failed;
    }

    if (lmac_get_tgt_type(soc->psoc_obj) == TARGET_TYPE_QCA8074 ||
            lmac_get_tgt_type(soc->psoc_obj) == TARGET_TYPE_QCA8074V2 ||
            lmac_get_tgt_type(soc->psoc_obj) == TARGET_TYPE_QCA6018) {
        if (register_netdevice_notifier(&ath_device_notifier))
            goto target_init_failed;
    }

    /* Register scan entry update callback */
    ucfg_scan_register_bcn_cb(soc->psoc_obj,
        wlan_scan_cache_update_callback, SCAN_CB_TYPE_UPDATE_BCN);

    /* UMAC Disatcher enable/start all components under PSOC */
    dispatcher_psoc_enable(soc->psoc_obj);

#ifdef DIRECT_BUF_RX_ENABLE
    if (direct_buf_rx_target_attach(soc->psoc_obj,
                          hif_get_hal_handle(lmac_get_hif_hdl(soc->psoc_obj)),
                          soc->qdf_dev) != QDF_STATUS_SUCCESS) {
        qdf_info("%s: Direct Buffer Rx Target Attach Failed", __func__);
    }
#endif

    /*
     * 8. Connect Services to HTC
     */
    qdf_info("connect HTC");
    qdf_info("bypasswmi : %d",bypasswmi);
    if (!bypasswmi) {

    soc->max_clients = cfg_get(soc->psoc_obj, CFG_OL_MAX_CLIENTS);
    soc->max_vaps = cfg_get(soc->psoc_obj, CFG_OL_MAX_VAPS);

#if ATH_SUPPORT_LOWI
	ol_ath_lowi_wmi_event_attach(soc);
#endif
#ifdef QVIT
        ol_ath_qvit_attach(soc);
#endif

        if(soc->ol_if_ops->wds_addr_init) {
            soc->ol_if_ops->wds_addr_init((wmi_unified_t)wmi_handle);
        }

        wmi_unified_register_event_handler(wmi_handle, wmi_unit_test_event_id,
                unit_test_handler, WMI_RX_UMAC_CTX);

        wmi_unified_register_event_handler(wmi_handle, wmi_wlan_profile_data_event_id,
                                                ol_ath_wlan_profile_data_event_handler, WMI_RX_UMAC_CTX);

        wmi_unified_register_event_handler(wmi_handle, wmi_pdev_tpc_config_event_id,
                                                ol_ath_pdev_tpc_config_event_handler, WMI_RX_UMAC_CTX);

        wmi_unified_register_event_handler(wmi_handle, wmi_gpio_input_event_id,
                                                ol_ath_gpio_input_event_handler, WMI_RX_UMAC_CTX);

        wmi_unified_register_event_handler(wmi_handle, wmi_pdev_nfcal_power_all_channels_event_id,
                                                ol_ath_nf_dbr_dbm_info_event_handler, WMI_RX_UMAC_CTX);

        wmi_unified_register_event_handler(wmi_handle, wmi_pdev_tpc_event_id,
                                                ol_ath_packet_power_info_event_handler, WMI_RX_UMAC_CTX);

        wmi_unified_register_event_handler(wmi_handle, wmi_generic_buffer_event_id,
                                                ol_ath_generic_buffer_event_handler, WMI_RX_UMAC_CTX);

        wmi_unified_register_event_handler(wmi_handle, wmi_peer_tx_mu_txmit_count_event_id,
                                                ol_ath_peer_mumimo_tx_count_event_handler, WMI_RX_UMAC_CTX);

        wmi_unified_register_event_handler(wmi_handle, wmi_peer_gid_userpos_list_event_id,
                                                ol_ath_peer_gid_userpos_list_event_handler, WMI_RX_UMAC_CTX);

        wmi_unified_register_event_handler(wmi_handle, wmi_pdev_check_cal_version_event_id,
                                                ol_ath_pdev_caldata_version_check_event_handler, WMI_RX_UMAC_CTX);
#if QCN_ESP_IE
        wmi_unified_register_event_handler(wmi_handle, wmi_esp_estimate_event_id,
                                                ol_ath_esp_estimate_event_handler, WMI_RX_UMAC_CTX);
#endif
        wmi_unified_register_event_handler(wmi_handle, wmi_pdev_ctl_failsafe_check_event_id,
                                                ol_ath_pdev_ctl_failsafe_check_event_handler, WMI_RX_UMAC_CTX);

        ol_ath_soc_dcs_attach(soc);

        ol_ath_node_soc_attach(soc);
        ol_ath_phyerr_attach(soc);
        ol_ath_stats_soc_attach(soc);
        ol_ath_soc_chan_info_attach(soc);
        ol_ath_vap_soc_attach(soc);
        ol_ath_wow_soc_attach(soc);
        ol_ath_beacon_soc_attach(soc);
        ol_ath_resmgr_soc_attach(soc);
        ol_ath_mgmt_soc_attach(soc);
#if ATH_SUPPORT_WIFIPOS
        ol_ath_rtt_meas_report_attach(soc);
#endif
    }

    qdf_info("%s() UMAC attach . ", __func__);

    /*
     * Register Rx callback with mgmt_txrx layer for receiving
     * mgmt frames.
     */
    rx_cb_info.frm_type = MGMT_FRAME_TYPE_ALL;
    rx_cb_info.mgmt_rx_cb = ieee80211_mgmt_input;
    wlan_mgmt_txrx_register_rx_cb(soc->psoc_obj, WLAN_UMAC_COMP_MLME,
                                       &rx_cb_info, 1);

#if QCA_11AX_STUB_SUPPORT
    if (1 == OL_ATH_IS_11AX_STUB_ENABLED(soc)) {
        qdf_info("%s: 802.11ax stubbing enabled",
                soc->sc_osdev->netdev->name);
    }
#endif /* QCA_11AX_STUB_SUPPORT */

    return EOK;

target_init_failed:
    dispatcher_psoc_close(soc->psoc_obj);

attach_failed:
    qdf_info(" %s error status %d\n",__func__, status);

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    if (soc->nss_soc.ops) {
        qdf_info("nss-wifi: detach -zero delay ");
        soc->nss_soc.ops->nss_soc_wifi_detach(soc, 0);
    }
#endif
    init_deinit_free_num_units(soc->psoc_obj, tgt_psoc_info);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,5,0)
    if(soc->alloc_task_wqueue != NULL) {
        ATH_FLUSH_WQUEUE(soc->alloc_task_wqueue);
        ATH_DESTROY_WQUEUE(soc->alloc_task_wqueue);
        soc->alloc_task_wqueue = NULL;
    }
#endif
    htc_handle=target_psoc_get_htc_hdl(tgt_psoc_info);
    if (htc_handle) {
        htc_destroy(htc_handle);
        target_psoc_set_htc_hdl(tgt_psoc_info, NULL);
    }

#ifdef WLAN_FEATURE_BMI
    if (soc->bmi_handle != NULL) {
        if (soc->bmi_handle->bmi_cleanup)
	    soc->bmi_handle->bmi_cleanup(soc->bmi_handle,soc->sc_osdev);
        if (soc->bmi_handle->bmi_free)
	    soc->bmi_handle->bmi_free(soc->bmi_handle);
    }
#endif
    if (!bypasswmi) {
        if ((wmi_unified_t)wmi_handle) {
            wmi_unified_detach((wmi_unified_t)wmi_handle);
#if WMI_RECORDING
            wmi_proc_remove((wmi_unified_t)wmi_handle, soc->wmi_proc_entry, soc->soc_idx);
#endif
            wmi_handle = NULL;
            target_psoc_set_wmi_hdl(tgt_psoc_info, NULL);

        }
        soc_txrx_handle = wlan_psoc_get_dp_handle(soc->psoc_obj);
        if (soc_txrx_handle) {
            cdp_txrx_intr_detach(soc_txrx_handle);
            dp_ext_hdl = cdp_soc_get_dp_txrx_handle(soc_txrx_handle);
            cdp_soc_set_dp_txrx_handle(soc_txrx_handle, NULL);

            if (dp_ext_hdl)
                qdf_mem_free(dp_ext_hdl);

            wlan_psoc_set_dp_handle(soc->psoc_obj, NULL);
            cdp_soc_detach(soc_txrx_handle);
#ifdef WLAN_FEATURE_FASTPATH
            soc->htt_handle = NULL;
#endif
        }

	if (soc->dbg_log_init) {
	    ol_ath_dbglog_detach(soc);
	    soc->dbg_log_init = 0;
	}
    }
#ifdef AH_CAL_IN_FLASH_PCI
#ifndef ATH_CAL_NAND_FLASH
    if (soc->cal_mem) {
        A_IOUNMAP(soc->cal_mem);
        soc->cal_mem = 0;
    }
#endif
    soc->cal_in_flash = 0;
#endif
    soc->cal_in_file = 0;
    soc->soc_lp_iot_vaps_mask = 0;

    return status;
}

static int ol_ath_pdev_regdmn_init(struct ol_ath_softc_net80211 *scn,
                                IEEE80211_REG_PARAMETERS *ieee80211_conf_param)
{
    ol_ath_soc_softc_t *soc = scn->soc;
    uint8_t pdev_idx;
    struct ieee80211com *ic;
    int status = 0;
    struct wlan_objmgr_psoc *psoc = soc->psoc_obj;
    struct wlan_objmgr_pdev *pdev = scn->sc_pdev;
    struct target_pdev_info *tgt_pdev;
    struct target_psoc_info *tgt_hdl;
    struct wlan_psoc_host_mac_phy_caps *mac_phy_cap;
    struct wlan_psoc_host_hal_reg_capabilities_ext *reg_cap;
    struct wlan_psoc_target_capability_info *target_cap;
    struct common_wmi_handle *wmi_handle;

    tgt_hdl = (struct target_psoc_info *)wlan_psoc_get_tgt_if_handle(
                               psoc);
    if (!tgt_hdl) {
        target_if_err("%s: target_psoc_info is null", __func__);
        return -EINVAL;
    }

    tgt_pdev = (struct target_pdev_info *)wlan_pdev_get_tgt_if_handle(
    					pdev);
    if (!tgt_hdl) {
    	target_if_err("%s: target_pdev_info is null", __func__);
    	return -EINVAL;
    }

    pdev_idx = target_pdev_get_pdev_idx(tgt_pdev);
    if(pdev_idx < 0) {
        qdf_info("%s: pdev idx is invalid ", __func__);
	return -EINVAL;
    }
    target_cap = target_psoc_get_target_caps(tgt_hdl);
    mac_phy_cap = target_psoc_get_mac_phy_cap(tgt_hdl);
    reg_cap = ucfg_reg_get_hal_reg_cap(psoc);
    if (!reg_cap)
        return -EINVAL;

    wmi_handle = lmac_get_wmi_hdl(psoc);
    if (!wmi_handle)
        return -EINVAL;

    ic = &scn->sc_ic;

    /* If extended service ready is supported by firmware, Use mac_phy
     * capabilities provided by extended service ready event instead of
     * service ready event. Extended service ready will override caps
     * recieved in service ready
     */
    if (wmi_service_enabled(wmi_handle, wmi_service_ext_msg)) {
        qdf_info(" phy id = %d Modes supported", pdev_idx);
        qdf_info(" 11b = %d 11g = %d 11a = %d 11n = %d 11ac = %d 11ax = %d",
                mac_phy_cap[pdev_idx].supports_11b,
                mac_phy_cap[pdev_idx].supports_11g,
                mac_phy_cap[pdev_idx].supports_11a,
                mac_phy_cap[pdev_idx].supports_11n,
                mac_phy_cap[pdev_idx].supports_11ac,
                mac_phy_cap[pdev_idx].supports_11ax);

        qdf_info(" Reg cap - phy_id = %d supp_bnd = %x, modes = %x, lo_2g = %d, hi_2g = %d lo_g5 = %d, hi_5g = %d", pdev_idx,
                mac_phy_cap[pdev_idx].supported_bands,
                reg_cap[pdev_idx].wireless_modes,
                reg_cap[pdev_idx].low_2ghz_chan,
                reg_cap[pdev_idx].high_2ghz_chan,
                reg_cap[pdev_idx].low_5ghz_chan,
                reg_cap[pdev_idx].high_5ghz_chan);

        ol_ath_update_wireless_modes(&reg_cap[pdev_idx], &mac_phy_cap[pdev_idx]);

        ol_ath_update_target_cap_from_mac_phy_cap(target_cap,
                                      &mac_phy_cap[pdev_idx]);

    }
    ol_ath_update_ext_caps(&mac_phy_cap[pdev_idx], ic);

    ol_ath_update_caps(ic, target_cap);
    ol_ath_update_chainmask(ic, target_cap, &mac_phy_cap[pdev_idx]);

    scn->max_tx_power = target_cap->hw_max_tx_power;
    scn->min_tx_power = target_cap->hw_min_tx_power;

    scn->txpowlimit2G = scn->max_tx_power;
    scn->txpowlimit5G = scn->max_tx_power;
    scn->txpower_scale = WMI_HOST_TP_SCALE_MAX;

    ieee80211com_set_txpowerlimit(ic, scn->max_tx_power);

    ol_regdmn_attach(scn);

    ol_regdmn_set_regdomain(scn->ol_regdmn_handle, reg_cap[pdev_idx].eeprom_reg_domain);
    ol_regdmn_set_regdomain_ext(scn->ol_regdmn_handle, reg_cap[pdev_idx].eeprom_reg_domain_ext);
#if QCA_11AX_STUB_SUPPORT
    if (1 == OL_ATH_IS_11AX_STUB_ENABLED(soc)) {
        ol_regdmn_stub_add_11ax_modes(scn->ol_regdmn_handle);
    }
#endif /* QCA_11AX_STUB_SUPPORT */
    if (target_psoc_get_target_ver(tgt_hdl) == AR6004_VERSION_REV1_3) {
        /*
           It's Hard code for HAL capability and We don't use this branch for McKinley.
           Because McKinley don't support WMI UNIFIED SERVICE READY,
           */
        reg_cap[pdev_idx].eeprom_reg_domain = 0;
        reg_cap[pdev_idx].eeprom_reg_domain_ext = 0x1f;
        reg_cap[pdev_idx].high_2ghz_chan = 0xaac;
        reg_cap[pdev_idx].high_5ghz_chan = 0x17d4;
        reg_cap[pdev_idx].low_2ghz_chan = 0x908;
        reg_cap[pdev_idx].low_5ghz_chan = 0x1338;
        reg_cap[pdev_idx].regcap1 = 7;
        reg_cap[pdev_idx].regcap2 = 0xbc0;
        reg_cap[pdev_idx].wireless_modes = 0x1f9001;
        ol_regdmn_attach(scn);
        ol_regdmn_set_regdomain(scn->ol_regdmn_handle, reg_cap[pdev_idx].eeprom_reg_domain);
        ol_regdmn_set_regdomain_ext(scn->ol_regdmn_handle, reg_cap[pdev_idx].eeprom_reg_domain_ext);
    }
    if (wmi_service_enabled(wmi_handle, wmi_service_restrt_chnl_support)) {
        ic->ic_rch_params.restrict_channel_support = 1;
        ic->ic_rch_params.low_5ghz_chan  = reg_cap[pdev_idx].low_5ghz_chan;
        ic->ic_rch_params.high_5ghz_chan = reg_cap[pdev_idx].high_5ghz_chan;

        qdf_info("%s: low_5ghz: %d  high_5gh: %d \n", __func__, reg_cap[pdev_idx].low_5ghz_chan,
               reg_cap[pdev_idx].high_5ghz_chan);
    } else {
        ic->ic_rch_params.restrict_channel_support = 0;
    }

    ic->ic_emiwar_80p80 = ol_ath_get_emiwar_80p80_defval(soc);

    ic->ic_reg_parm = *ieee80211_conf_param;
    if (!(wmi_service_enabled(wmi_handle, wmi_service_regulatory_db)))
        ol_regdmn_start(scn->ol_regdmn_handle, ieee80211_conf_param);

    return status;
}

static void ol_ath_update_phymode_caps(struct ieee80211com *ic, enum ieee80211_phymode mode)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    ol_ath_soc_softc_t *soc = scn->soc;
    struct wlan_objmgr_psoc *psoc = soc->psoc_obj;
    struct wlan_objmgr_pdev *pdev = scn->sc_pdev;
    struct target_pdev_info *tgt_pdev;
    struct target_psoc_info *tgt_hdl;
    struct wlan_psoc_host_mac_phy_caps *mac_phy_cap_arr;
    int32_t pdev_idx;
    struct common_wmi_handle *wmi_handle;

    tgt_hdl = (struct target_psoc_info *)wlan_psoc_get_tgt_if_handle(
                               psoc);
    if (!tgt_hdl) {
        target_if_err("target_psoc_info is null");
        return;
    }

    tgt_pdev = (struct target_pdev_info *)wlan_pdev_get_tgt_if_handle(
		pdev);
    if (!tgt_pdev) {
        target_if_err("target_pdev_info is null");
        return;
    }

    pdev_idx = target_pdev_get_pdev_idx(tgt_pdev);
    if (pdev_idx < 0) {
        qdf_info("pdev idx is invalid ");
	return;
    }
    QDF_TRACE(QDF_MODULE_ID_DEBUG, QDF_TRACE_LEVEL_DEBUG,
            "pdev id is %d \n",pdev_idx);
    mac_phy_cap_arr = target_psoc_get_mac_phy_cap(tgt_hdl);
    if(!mac_phy_cap_arr) {
       qdf_info("mac phy cap is NULL ");
       return;
    }
    wmi_handle = target_psoc_get_wmi_hdl(tgt_hdl);

    if (wmi_service_enabled(wmi_handle,
                wmi_service_ext_msg)) {
        struct wlan_psoc_host_mac_phy_caps *mac_phy_cap = &mac_phy_cap_arr[pdev_idx];
        struct ieee80211_he_handle *ic_he;

        ic_he = &ic->ic_he;
        if (is_phymode_2G(mode)) {
            ieee80211com_clear_htcap(ic, -1);
            ieee80211com_clear_vhtcap(ic, -1);
            ol_ath_update_ht_caps(ic, mac_phy_cap->ht_cap_info_2G);
            ol_ath_update_vht_caps(ic, mac_phy_cap->vht_cap_info_2G,
                    mac_phy_cap->vht_supp_mcs_2G);
            ol_ath_populate_he_caps(ic_he, mac_phy_cap->he_cap_info_2G,
                    &mac_phy_cap->he_supp_mcs_2G,
                    mac_phy_cap->he_cap_phy_info_2G, sizeof(mac_phy_cap->he_cap_phy_info_2G),
                    &mac_phy_cap->he_ppet2G, &mac_phy_cap->he_cap_info_internal);
        } else if (is_phymode_5G(mode)) {
            ieee80211com_clear_htcap(ic, -1);
            ieee80211com_clear_vhtcap(ic, -1);
            ol_ath_update_ht_caps(ic, mac_phy_cap->ht_cap_info_5G);
            ol_ath_update_vht_caps(ic, mac_phy_cap->vht_cap_info_5G,
                    mac_phy_cap->vht_supp_mcs_5G);
            ol_ath_populate_he_caps(ic_he, mac_phy_cap->he_cap_info_5G,
                    &mac_phy_cap->he_supp_mcs_5G,
                    mac_phy_cap->he_cap_phy_info_5G, sizeof(mac_phy_cap->he_cap_phy_info_5G),
                    &mac_phy_cap->he_ppet5G, &mac_phy_cap->he_cap_info_internal);
        } else {
            qdf_debug("Error Mode %d not 2G or 5G", mode);
        }
    }
}

#ifdef WLAN_SUPPORT_TWT
int ol_ath_twt_enable_command(struct ol_ath_softc_net80211 *scn)
{
    struct wmi_twt_enable_param twt_param;
    ol_ath_soc_softc_t *soc = scn->soc;

    if (!soc->twt_enable) {
        qdf_info("TWT is disable in INI. Do not send enable cmd to FW");
        /* Clear ext caps in psoc to indicate no support for TWT */
        wlan_psoc_nif_fw_ext_cap_clear(soc->psoc_obj, WLAN_SOC_CEXT_TWT_REQUESTER);
        wlan_psoc_nif_fw_ext_cap_clear(soc->psoc_obj, WLAN_SOC_CEXT_TWT_RESPONDER);
        return 0;
    }

    twt_param.pdev_id = lmac_get_pdev_idx(scn->sc_pdev);
    twt_param.sta_cong_timer_ms = scn->soc->twt.sta_cong_timer_ms;
    twt_param.mbss_support = scn->soc->twt.mbss_support;
    twt_param.default_slot_size = scn->soc->twt.default_slot_size;
    twt_param.congestion_thresh_setup = scn->soc->twt.congestion_thresh_setup;
    twt_param.congestion_thresh_teardown = scn->soc->twt.congestion_thresh_teardown;
    twt_param.congestion_thresh_critical = scn->soc->twt.congestion_thresh_critical;
    twt_param.interference_thresh_teardown = scn->soc->twt.interference_thresh_teardown;
    twt_param.interference_thresh_setup = scn->soc->twt.interference_thresh_setup;
    twt_param.min_no_sta_setup = scn->soc->twt.min_no_sta_setup;
    twt_param.min_no_sta_teardown = scn->soc->twt.min_no_sta_teardown;
    twt_param.no_of_bcast_mcast_slots = scn->soc->twt.no_of_bcast_mcast_slots;
    twt_param.min_no_twt_slots = scn->soc->twt.min_no_twt_slots;
    twt_param.max_no_sta_twt = scn->soc->twt.max_no_sta_twt;
    twt_param.mode_check_interval = scn->soc->twt.mode_check_interval;
    twt_param.add_sta_slot_interval = scn->soc->twt.add_sta_slot_interval;
    twt_param.remove_sta_slot_interval = scn->soc->twt.remove_sta_slot_interval;

    return wmi_unified_twt_enable_cmd(lmac_get_pdev_wmi_handle(scn->sc_pdev), &twt_param);
}
#else
int ol_ath_twt_enable_command(struct ol_ath_softc_net80211 *scn)
{
    return 0;
}
#endif

#ifdef OBSS_PD
int ol_ath_set_obss_thresh(struct ieee80211com *ic, int enable, struct ol_ath_softc_net80211 *scn)
{
    int retval = -EINVAL;
    struct ieee80211_vap_opmode_count vap_opmode_count;
    struct wlan_objmgr_psoc *psoc;
    psoc = scn->soc->psoc_obj;

    OS_MEMZERO(&vap_opmode_count, sizeof(struct ieee80211_vap_opmode_count));
    ieee80211_get_vap_opmode_count(ic, &vap_opmode_count);

    /* Since spatial reuse works on a pdev level,
     * once both STA vap and AP vap present, or any monitor vap,
     * then we must disable spatial reuse params. This is because AP
     * and STA share the same HW register, so they will overwrite the
     * same fields in that register. Since Monitors do not transmit,
     * spatial reuse should be disabled for all monitor vaps.
     */
    if ((vap_opmode_count.ap_count && vap_opmode_count.sta_count) ||
            (vap_opmode_count.monitor_count &&
            !cfg_get(psoc, CFG_OL_ALLOW_MON_VAPS_IN_SR))){
        retval = ol_ath_pdev_set_param(scn,
                    wmi_pdev_param_set_cmd_obss_pd_threshold,
                    !HE_SR_OBSS_PD_THRESH_ENABLE);
        if(retval) {
            qdf_err("%s: Could not set obss pd thresh enable to 0", __func__);
            return retval;
        }
        qdf_info("%s: Invalid configuration. Cannot have Monitor Vaps in current"
        " mode or AP and STAs in combination", __func__);
    }
    else {
        /* Keep the threshold value and clear the enable bit */
        ic->ic_ap_obss_pd_thresh =
                (ic->ic_ap_obss_pd_thresh & HE_OBSS_PD_THRESH_MASK);

        /* Set the enable bit to reflect the `enable` value */
        ic->ic_ap_obss_pd_thresh |= (HE_OBSS_PD_THRESH_ENABLE_OFFSET(enable));

        retval = ol_ath_pdev_set_param(scn,
                        wmi_pdev_param_set_cmd_obss_pd_threshold,
                        ic->ic_ap_obss_pd_thresh);
        if(retval) {
            qdf_err("%s: Could not set obss pd thresh enable val", __func__);
        }
    }

    return retval;
}
#endif //OBSS_PD


int ol_ath_pdev_attach(struct ol_ath_softc_net80211 *scn,
		IEEE80211_REG_PARAMETERS *ieee80211_conf_parm,
		uint8_t phy_id)
{
    ol_ath_soc_softc_t *soc = scn->soc;
    struct ieee80211com     *ic = &scn->sc_ic;
    int status = 0;
    osdev_t osdev = scn->sc_osdev;
    uint32_t host_num_peers_psoc = 0;
    uint32_t host_num_peers_pdev = 0;
    uint32_t max_peers = 0;
#if DBDC_REPEATER_SUPPORT
    int i,j,k;
    struct ieee80211com     *tmp_ic;
#endif
#if ATH_SUPPORT_DSCP_OVERRIDE
    int map_id;
#endif
    struct wlan_objmgr_psoc *psoc = NULL;
    struct target_psoc_info *tgt_hdl;
    target_resource_config *tgt_cfg;
    uint32_t target_type;
    uint32_t max_target_connected_peers;
    uint32_t target_version;
    uint8_t *myaddr;
    struct common_wmi_handle *wmi_handle;
#if defined(WLAN_DFS_PARTIAL_OFFLOAD) && defined(HOST_DFS_SPOOF_TEST)
    struct wlan_lmac_if_dfs_rx_ops *dfs_rx_ops;
#endif /* HOST_DFS_SPOOF_TEST */
    uint8_t num_radios;
    ol_txrx_soc_handle soc_txrx_handle;
    ol_txrx_pdev_handle pdev_txrx_handle;

    psoc = soc->psoc_obj;
    tgt_hdl = (struct target_psoc_info *)wlan_psoc_get_tgt_if_handle(psoc);
    if (!tgt_hdl) {
        qdf_info("%s: psoc target_psoc_info is null", __func__);
        return -EINVAL;
    }
    soc_txrx_handle = wlan_psoc_get_dp_handle(psoc);
    pdev_txrx_handle = wlan_pdev_get_dp_handle(ic->ic_pdev_obj);

#if defined(QCA_OL_TX_MULTIQ_SUPPORT) || defined(QCA_OL_RX_MULTIQ_SUPPORT)
    /* Enable multiq support for lithium */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
    ic->multiq_support_enabled = cfg_get(psoc, CFG_TGT_MULTIQ_SUPPORT);
#endif
#endif

    tgt_cfg = target_psoc_get_wlan_res_cfg(tgt_hdl);
    target_type = target_psoc_get_target_type(tgt_hdl);
    target_version = target_psoc_get_target_ver(tgt_hdl);
    wmi_handle = target_psoc_get_wmi_hdl(tgt_hdl);
    num_radios = target_psoc_get_num_radios(tgt_hdl);
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
    soc->buff_thresh.ald_free_buf_lvl = scn->soc->buff_thresh.pool_size - (( scn->soc->buff_thresh.pool_size * 75 ) / 100);
    soc->buff_thresh.ald_buffull_wrn = 0;
#endif

    ic->interface_id = scn->radio_id;
    qdf_info("interface_id %d", ic->interface_id);

    ic->ic_auth_tx_xretry = 0;
#if ATH_SUPPORT_DSCP_OVERRIDE
    /* Initializing tid-map parameters to default vaules. */
    for (map_id = 0; map_id < NUM_DSCP_MAP; map_id++) {
        OS_MEMCPY(ic->ic_dscp_tid_map[map_id],dscp_tid_map, sizeof(uint32_t) * WMI_HOST_DSCP_MAP_MAX);
    }
    ic->ic_override_dscp = 0x00;
    ic->ic_dscp_hmmc_tid = 0x00;
    ic->ic_override_hmmc_dscp = 0x00;
#endif
    ic->ic_consider_obss_long_slot = CONSIDER_OBSS_LONG_SLOT_DEFAULT;

    /* init IEEE80211_DPRINTF_IC control object */
    ieee80211_dprintf_ic_init(ic);

    if (!bypasswmi) {
        // Use attach_failed1 for failures beyond this
        /*
         * 9. WLAN/UMAC initialization
         */
        ic->ic_is_mode_offload = ol_ath_net80211_is_mode_offload;
        ic->ic_if_mgmt_drain = ol_if_mgmt_drain;
        ic->ic_is_macreq_enabled = ol_ath_net80211_is_macreq_enabled;
        ic->ic_get_mac_prealloc_idmask = ol_ath_net80211_get_mac_prealloc_idmask;
        ic->ic_set_mac_prealloc_id = ol_ath_net80211_set_mac_prealloc_id;
        ic->ic_get_bsta_fixed_idmask = ol_ath_net80211_get_bsta_fixed_idmask;
        ic->ic_update_target_caps = ol_ath_update_phymode_caps;
        ic->ic_osdev = osdev;
        ic->ic_qdf_dev = soc->qdf_dev;
        ic->ic_netdev = scn->netdev;
#if defined(WLAN_DFS_PARTIAL_OFFLOAD) && defined(HOST_DFS_SPOOF_TEST)
        ic->ic_is_host_dfs_check_enabled = ol_if_is_host_dfs_check_support_enabled;
#endif /* HOST_DFS_SPOOF_TEST */

        /* register bcn_tx_status_event_handler */
        ic->ic_register_beacon_tx_status_event_handler =
                    ol_ath_mgmt_register_offload_beacon_tx_status_event;
        ic->ic_config_bsscolor_offload                 =
                    ol_ath_config_bsscolor_offload;

        /* Update the peer count variable
         * The peer count configured is expected to be the max number
         * of client peers connected excluding the number of self & bss peers
         */
        max_target_connected_peers = tgt_cfg->num_peers - tgt_cfg->num_vdevs;
        max_peers = cfg_get(soc->psoc_obj, CFG_OL_MAX_PEERS);
        if(max_peers) {
            if (max_peers < num_radios) {
                qdf_info("Very low max peers configured. Setting max_peers to %d",
                       num_radios);
                host_num_peers_psoc = num_radios;
            }
            else {
                 host_num_peers_psoc = QDF_MIN(max_peers, max_target_connected_peers);
            }
        }
        else {
            host_num_peers_psoc = max_target_connected_peers;
        }
        qdf_atomic_set(&scn->peer_count, 0);
        qdf_atomic_inc(&scn->peer_count);

        /* This is required to make sure object manager psoc
         * peer count has room for connected peers as
         * well as bss peers*2 for vdevs.
         * We need 2 times the number of bss_peers
         * since during bss reset, a new peer is created
         * and then old peer is removed.
        */
#define MAX_BSS_PER_PER_VDEV 2
        if(max_peers) {
            wlan_psoc_set_max_peer_count(psoc,
            host_num_peers_psoc + (tgt_cfg->num_vdevs * MAX_BSS_PER_PER_VDEV));
        }
#if ATH_SUPPORT_WRAP
        /* This adjustment is made so that the number of peers
         * in host and FW become equal. Host is not sending a peer
         * create event when creating a sta which leads to mismatch
         * in peer count between host and FW */
        if (wlan_psoc_nif_feat_cap_get(soc->psoc_obj, WLAN_SOC_F_QWRAP_ENABLE)) {
            host_num_peers_psoc = host_num_peers_psoc - 1;
        }
#endif
        host_num_peers_pdev = (host_num_peers_psoc/num_radios);

        qdf_atomic_add(host_num_peers_pdev, &scn->peer_count);

        ol_ath_pdev_regdmn_init(scn, ieee80211_conf_parm);
        wlan_pdev_obj_lock(scn->sc_pdev);
        myaddr = wlan_pdev_get_hw_macaddr(scn->sc_pdev);
        IEEE80211_ADDR_COPY(ic->ic_myaddr, myaddr);
        IEEE80211_ADDR_COPY(ic->ic_my_hwaddr, myaddr);
        wlan_pdev_obj_unlock(scn->sc_pdev);

        ol_ath_setup_rates(ic);
        ieee80211com_set_cap_ext(ic, IEEE80211_CEXT_PERF_PWR_OFLD);

        ic->ic_dprintf_ic = IEEE80211_DPRINTF_IC;
        ic->ru26_tolerant = true;
        ic->ic_get_pdev_idx = ol_ath_get_pdev_idx;

        if (wlan_psoc_nif_fw_ext_cap_get(psoc, WLAN_SOC_CEXT_MBSS_IE) &&
            cfg_get(psoc, CFG_OL_MBSS_IE_ENABLE)) {
           qdf_info("Support for MBSS IE is enabled");
           wlan_pdev_nif_feat_cap_set(ic->ic_pdev_obj,
                                      WLAN_PDEV_F_MBSS_IE_ENABLE);
        }

        status = ol_ath_dev_attach(scn, ieee80211_conf_parm);
        if (status) {
            qdf_info("ol_ath_dev_attach failed status %d\n", status);
            return status;
        }
#if !ATH_SUPPORT_LOWI && ATH_SUPPORT_WIFIPOS
        if (wifiposenable)
        {
            ic->ic_rtt_init_netlink = ieee80211com_init_netlink;
            ol_ath_rtt_netlink_attach(ic);
        }
        else
        {
            qdf_info("%s:%d: netlink socket for wifipos not created\n", __func__,__LINE__);
        }
#endif
        ic->ic_find_channel = ieee80211_find_channel;

#if UMAC_SUPPORT_CFG80211
        ic->ic_cfg80211_radio_attach = ieee80211_cfg80211_radio_attach;
        ic->ic_cfg80211_radio_detach = ieee80211_cfg80211_radio_detach;
        ic->ic_cfg80211_update_channel_list = wlan_cfg80211_update_channel_list;

        ic->ic_cfg80211_radio_handler.setcountry = ol_ath_ucfg_set_country ;
        ic->ic_cfg80211_radio_handler.getcountry = ol_ath_ucfg_get_country ;
        ic->ic_cfg80211_radio_handler.sethwaddr = ol_ath_ucfg_set_mac_address ;
        ic->ic_cfg80211_radio_handler.gethwaddr = NULL ;
        ic->ic_cfg80211_radio_handler.setparam = ol_ath_ucfg_setparam ;
        ic->ic_cfg80211_radio_handler.getparam = ol_ath_ucfg_getparam ;
        ic->ic_cfg80211_radio_handler.extended_commands = ol_ath_extended_commands;

#if UNIFIED_SMARTANTENNA
        ic->ic_cfg80211_radio_handler.set_saparam = ol_ath_ucfg_set_smart_antenna_param;
	ic->ic_cfg80211_radio_handler.get_saparam = ol_ath_ucfg_get_smart_antenna_param;
#endif

#if ATH_SUPPORT_DSCP_OVERRIDE
        ic->ic_cfg80211_radio_handler.setdscp_tid_map = NULL ;
        ic->ic_cfg80211_radio_handler.getdscp_tid_map = NULL ;
#endif
        ic->ic_cfg80211_radio_handler.gpio_config = ol_ath_ucfg_gpio_config;
	ic->ic_cfg80211_radio_handler.gpio_output = ol_ath_ucfg_gpio_output;
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
        ic->ic_cfg80211_radio_handler.ald_getStatistics = NULL ;
#endif

#if PEER_FLOW_CONTROL
        ic->ic_cfg80211_radio_handler.txrx_peer_stats = ol_ath_ucfg_txrx_peer_stats;
#endif

#if QCA_AIRTIME_FAIRNESS
        ic->ic_cfg80211_radio_handler.set_atf_sched_dur = ol_ath_ucfg_set_atf_sched_dur ;
#endif

        ic->ic_cfg80211_radio_handler.get_aggr_burst = ol_ath_iw_get_aggr_burst;
        ic->ic_cfg80211_radio_handler.set_aggr_burst = ol_ath_ucfg_set_aggr_burst;
        ic->ic_cfg80211_radio_handler.ucfg_phyerr = ol_ath_ucfg_phyerr;
        ic->ic_cfg80211_radio_handler.get_ath_stats = ol_ath_ucfg_get_ath_stats;
        ic->ic_cfg80211_radio_handler.get_dp_fw_peer_stats = ol_ath_get_dp_fw_peer_stats;
        ic->ic_cfg80211_radio_handler.get_dp_htt_stats = ol_ath_get_dp_htt_stats;
        ic->ic_cfg80211_radio_handler.set_ba_timeout = ol_ath_set_ba_timeout;
        ic->ic_cfg80211_radio_handler.get_ba_timeout = ol_ath_get_ba_timeout;
        ic->ic_cfg80211_radio_handler.set_pcp_tid_map = ol_ath_ucfg_set_pcp_tid_map;
        ic->ic_cfg80211_radio_handler.get_pcp_tid_map = ol_ath_ucfg_get_pcp_tid_map;
        ic->ic_cfg80211_radio_handler.set_tidmap_prty = ol_ath_ucfg_set_tidmap_prty;
        ic->ic_cfg80211_radio_handler.get_tidmap_prty = ol_ath_ucfg_get_tidmap_prty;
        ic->ic_cfg80211_radio_handler.set_btcoex_duty_cycle = ol_ath_ucfg_btcoex_duty_cycle;
#endif
        ic->ic_num_mcast_tbl_elements = ol_ath_num_mcast_tbl_elements;
        ic->ic_num_mcast_grps = ol_ath_num_mcast_grps;
        ic->ic_is_target_ar900b = ol_ath_is_target_ar900b;
        ic->ic_get_tgt_type = ol_ath_get_tgt_type;
#if UMAC_SUPPORT_CFG80211
        ic->ic_cfg80211_radio_handler.set_he_bss_color = ol_ath_ucfg_set_he_bss_color;
        ic->ic_cfg80211_radio_handler.get_he_bss_color = ol_ath_ucfg_get_he_bss_color;
#ifdef WLAN_SUPPORT_RX_PROTOCOL_TYPE_TAG
        ic->ic_cfg80211_radio_handler.ic_set_rx_pkt_protocol_tagging = ol_ath_ucfg_set_rx_pkt_protocol_tagging;
#ifdef WLAN_SUPPORT_RX_TAG_STATISTICS
        ic->ic_cfg80211_radio_handler.ic_dump_rx_pkt_protocol_tag_stats = ol_ath_ucfg_dump_rx_pkt_protocol_tag_stats;
#endif //WLAN_SUPPORT_RX_TAG_STATISTICS
#endif //WLAN_SUPPORT_RX_PROTOCOL_TYPE_TAG
#endif
        ic->ic_cfg80211_radio_handler.set_he_sr_config = ol_ath_ucfg_set_he_sr_config;
        ic->ic_cfg80211_radio_handler.get_he_sr_config = ol_ath_ucfg_get_he_sr_config;

#if WLAN_SUPPORT_PRIMARY_ALLOWED_CHAN
        ic->ic_primary_chanlist = NULL;
        ic->ic_primary_allowed_enable = false;
#endif

#if ATH_SUPPORT_HYFI_ENHANCEMENTS
        ic->ic_hmmc_cnt = 3;
        ic->ic_hmmcs[0].ip = be32toh(0xeffffffa); /* 239.255.255.250 */
        ic->ic_hmmcs[0].mask = 0xffffffff;
        ic->ic_hmmcs[1].ip = be32toh(0xe00000fb); /* 224.0.0.251 */
        ic->ic_hmmcs[1].mask = 0xffffffff;
        ic->ic_hmmcs[2].ip = be32toh(0xe00000fc); /* 224.0.0.252 */
        ic->ic_hmmcs[2].mask = 0xffffffff;
        ic->ic_check_buffull_condition = ol_ath_net80211_check_buffull_condition;
#endif

#if ATH_SUPPORT_WRAP
        qdf_spinlock_create(&scn->sc_mpsta_vap_lock);
#endif

        ol_ath_vap_attach(ic);

        if ((status = ol_ath_node_attach(scn, ic))) {
            qdf_info("%s: ol_ath_node_attach failed\n",__func__);
            return status;
        };

        ol_ath_beacon_attach(ic);
#if ATH_SUPPORT_WIFIPOS
        ic->ic_xmitrtt3 = ol_ieee80211_wifipos_xmitrtt3;
#endif

        ol_ath_mgmt_attach(ic);

        ol_ath_chan_info_attach(ic);
        /* attach the dcs functionality */
        ol_ath_dcs_attach(ic);
        /* As of now setting ic with all ciphers assuming
         * hardware will support, eventually to query
         * the hardware to figure out h/w crypto support.
         */
        ieee80211com_set_cap(ic, IEEE80211_C_WEP);
        ieee80211com_set_cap(ic, IEEE80211_C_AES);
        ieee80211com_set_cap(ic, IEEE80211_C_AES_CCM);
        ieee80211com_set_cap(ic, IEEE80211_C_CKIP);
        ieee80211com_set_cap(ic, IEEE80211_C_TKIP);
        ieee80211com_set_cap(ic, IEEE80211_C_TKIPMIC);
        if (wmi_service_enabled(wmi_handle, wmi_service_11ac)) {
            ieee80211com_set_cap_ext(ic, IEEE80211_CEXT_11AC);
        }

        if (ieee80211_conf_parm->wmeEnabled) {
            ieee80211com_set_cap(ic, IEEE80211_C_WME);
        }

#if ATH_SUPPORT_WAPI
        ieee80211com_set_cap(ic, IEEE80211_C_WAPI);
#endif
	wlan_scan_update_channel_list(ic);

#if OL_ATH_SUPPORT_LED
        if (target_version == AR9888_REV2_VERSION || target_version == AR9887_REV1_VERSION) {
            scn->scn_led_gpio = PEREGRINE_LED_GPIO ;
        } else if (target_version ==  AR900B_DEV_VERSION) {
            scn->scn_led_gpio = BEELINER_LED_GPIO ;
        } else if (target_version ==  QCA9984_DEV_VERSION) {
            scn->scn_led_gpio = CASCADE_LED_GPIO ;
        } else if (target_version ==  QCA9888_DEV_VERSION) {
            scn->scn_led_gpio = BESRA_LED_GPIO ;
        }

        if(target_type == TARGET_TYPE_IPQ4019) {
            scn->scn_led_gpio = 0; //will get initialized later
            ipq4019_wifi_led_init(scn);
        }

#if QCA_LTEU_SUPPORT
        if (wlan_psoc_nif_feat_cap_get(psoc, WLAN_SOC_F_LTEU_SUPPORT)) {
            ol_gpio_config(scn, scn->scn_led_gpio, 1, WMI_HOST_GPIO_PULL_DOWN, WMI_HOST_GPIO_INTTYPE_RISING_EDGE);
        } else {
#endif
	  if ((target_type == TARGET_TYPE_QCA8074) ||
			  (target_type == TARGET_TYPE_QCA8074V2) ||
			  (target_type == TARGET_TYPE_QCA6018) ||
			  (target_type == TARGET_TYPE_QCA6290)) {
	    /* Skip as current gpio configuration is not available. */
          } else if(target_type == TARGET_TYPE_IPQ4019) {
                //ipq4019_wifi_led(scn, LED_ON);
                /* Do not enable LED for IPQ4019 during attach, as wifi LED will keep
                   glowing even if vaps are not created for that radio */
            } else {
                ol_gpio_config(scn, scn->scn_led_gpio, 0, 0, 0);
                ol_ath_gpio_output(scn, scn->scn_led_gpio, 1);
            }

            OS_INIT_TIMER(scn->sc_osdev, &(scn->scn_led_blink_timer), ol_ath_led_blink_timed_out,
                    (void *)scn, QDF_TIMER_TYPE_WAKE_APPS);
            OS_INIT_TIMER(scn->sc_osdev, &(scn->scn_led_poll_timer), ol_ath_led_poll_timed_out,
                    (void *)scn, QDF_TIMER_TYPE_WAKE_APPS);
            scn->scn_blinking = OL_BLINK_DONE;
            scn->scn_led_byte_cnt = 0;
            scn->scn_led_total_byte_cnt = 0;
            scn->scn_led_last_time = CONVERT_SYSTEM_TIME_TO_MS(OS_GET_TIMESTAMP());
            soc->led_blink_rate_table         = ol_led_blink_rate_table;
            scn->scn_led_max_blink_rate_idx = ARRAY_LENGTH(ol_led_blink_rate_table) - 1;
#if QCA_LTEU_SUPPORT
        }
#endif
#endif /* OL_ATH_SUPPORT_LED */
    }

    if(prealloc_disabled){
        ic->ic_scan_entry_max_count = ATH_SCANENTRY_MAX;
    } else {
        ic->ic_scan_entry_max_count = ATH_MAX_SCAN_ENTRIES;
    }
    ic->ic_scan_entry_timeout = ATH_SCANENTRY_TIMEOUT;

    /* unless user prefers not to allow the frames between different vaps, let frames route through*/
    scn->scn_block_interbss = 0;

#if ATH_DATA_TX_INFO_EN
    /*Alloc buffer for data TX info*/
    scn->tx_status_buf = qdf_mem_malloc(sizeof(struct ieee80211_tx_status));
#endif

#if ATH_PROXY_NOACK_WAR
    if(target_type == TARGET_TYPE_AR900B) {
        /* WAR is required only for Beeliner*/
        scn->sc_proxy_noack_war = 1;
        OS_INIT_TIMER(ic->ic_osdev, &(ic->ic_ast_reserve_timer), ol_proxy_ast_reserve_timeout,
                (void *) (ic), QDF_TIMER_TYPE_WAKE_APPS);
    } else {
        /* Peregrine/Swift/Cascade/Dakota/Besra - Don't need this QWRAP WAR */
        scn->sc_proxy_noack_war = 0;
    }
#endif

    /* Add ic to global list */
    GLOBAL_IC_LOCK_BH(&ic_list);
    ic_list.global_ic[ic_list.num_global_ic++] = ic;
    qdf_info("Set global_ic[%d], ptr:%pK", ic_list.num_global_ic,&ic_list);
    ic->ic_global_list = &ic_list;
#if DBDC_REPEATER_SUPPORT
    ic_list.dbdc_process_enable = 1;
    ic_list.force_client_mcast_traffic = 0;

    if (ol_target_lithium(psoc)) {
        dp_lag_soc_enable(ic->ic_pdev_obj, 1);

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
        if (ic->nss_radio_ops) {
            ic->nss_radio_ops->ic_nss_ol_enable_dbdc_process(ic, 1);
        }
#endif
    }
    ic_list.delay_stavap_connection = cfg_get(psoc, CFG_TGT_DBDC_TIME_DELAY);

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    if (ic->nss_radio_ops)
        ic->nss_radio_ops->ic_nss_ol_set_force_client_mcast_traffic(ic, ic_list.force_client_mcast_traffic);
#endif
    ic_list.num_stavaps_up = 0;
    ic_list.is_dbdc_rootAP = 0;
    ic_list.iface_mgr_up = 0;
    ic_list.disconnect_timeout = 10;
    ic_list.reconfiguration_timeout = 60;
    ic_list.always_primary = 0;
    ic_list.num_fast_lane_ic = 0;
    ic_list.max_priority_stavap_up = NULL;
    ic_list.drop_secondary_mcast = 0;
    ic_list.num_rptr_clients = 0;
    ic_list.same_ssid_support = 0;
    ic_list.ap_preferrence = 0;
    ic_list.extender_info = 0;
    ic_list.rootap_access_downtime = 0;
    ic_list.num_l2uf_retries = 0;
    for (i=0; i < MAX_RADIO_CNT; i++) {
        memset(&ic_list.preferred_list_stavap[i][0], 0, IEEE80211_ADDR_LEN);
        memset(&ic_list.denied_list_apvap[i][0], 0, IEEE80211_ADDR_LEN);
    }
#endif
    GLOBAL_IC_UNLOCK_BH(&ic_list);

#if DBDC_REPEATER_SUPPORT
    spin_lock(&ic->ic_lock);
    if (ic_list.num_global_ic) {
        /* In case of DBDC repeater configuration, pass Multicast/Broadcast and
         ethernet client traffic through this radio */
        ic->ic_radio_priority = ic_list.num_global_ic;
        dp_lag_pdev_set_priority(ic->ic_pdev_obj, ic->ic_radio_priority);

        if (ic_list.num_global_ic == 1) {
            ic->ic_primary_radio = 1;
        } else{
            ic->ic_primary_radio = 0;
        }
        dp_lag_pdev_set_primary_radio(ic->ic_pdev_obj, ic->ic_primary_radio);
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
        if (ic->nss_radio_ops) {
            ic->nss_radio_ops->ic_nss_ol_set_primary_radio(scn, ic->ic_primary_radio);
        }
#endif
    }
    ic->fast_lane = 0;
    ic->fast_lane_ic = NULL;
    ic->ic_extender_connection = 0;
    OS_MEMZERO(ic->ic_preferred_bssid, IEEE80211_ADDR_LEN);
    spin_unlock(&ic->ic_lock);

    k = 0;
    /*update other_ic list on each radio*/
    for (i=0; i < MAX_RADIO_CNT; i++) {
    GLOBAL_IC_LOCK_BH(&ic_list);
    tmp_ic = ic_list.global_ic[i];
    GLOBAL_IC_UNLOCK_BH(&ic_list);
    if (tmp_ic && (tmp_ic != ic) && (k < MAX_RADIO_CNT-1)) {
        spin_lock(&ic->ic_lock);
        ic->other_ic[k++] = tmp_ic;
        spin_unlock(&ic->ic_lock);
        for (j=0; j < MAX_RADIO_CNT-1 ; j++) {
        if (tmp_ic->other_ic[j] == NULL) {
            spin_lock(&tmp_ic->ic_lock);
            tmp_ic->other_ic[j] = ic;
            spin_unlock(&tmp_ic->ic_lock);
            break;
        }
        }
    }
    }

#endif
    /*Set default monitor VAP filter to enable all input*/
    ic->mon_filter_osif_mac = 0;
    ic->ic_non_doth_sta_cnt = 0;

    ic->ic_stop = ol_ath_target_stop;

    if (wmi_service_enabled(wmi_handle,
                wmi_service_multiple_vdev_restart)) {
        wlan_pdev_nif_feat_cap_set(ic->ic_pdev_obj,
                                   WLAN_PDEV_F_MULTIVDEV_RESTART);
    } else {
        wlan_pdev_nif_feat_cap_clear(ic->ic_pdev_obj,
                                     WLAN_PDEV_F_MULTIVDEV_RESTART);
    }

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    if (scn->nss_radio.nss_rctx)
        ic->nss_vops->ic_osif_nss_wifi_monitor_set_filter(ic, MON_FILTER_ALL_DISABLE);
#endif
    /* Set max vaps supported for each PDEV */
    wlan_pdev_set_max_vdev_count(scn->sc_pdev,
                                  (tgt_cfg->num_vdevs/num_radios));
    /* Set max peers supported for each PDEV */
    wlan_pdev_set_max_peer_count(scn->sc_pdev,
            wlan_psoc_get_max_peer_count(psoc)/num_radios);
    qdf_info("Pdev = %d Number of peers = %d", phy_id,
            wlan_pdev_get_num_peer(soc, phy_id));

    ol_ath_twt_enable_command(scn);
#ifdef QVIT
    ol_ath_pdev_qvit_attach(scn);
#endif

    if (dispatcher_pdev_open(scn->sc_pdev)) {
        qdf_info("%s() : Dispatcher pdev open failed ", __func__);
        return -EINVAL;
    }

    ol_ath_pdev_dfs_phyerr_offload_en(scn);
#if defined(WLAN_DFS_PARTIAL_OFFLOAD) && defined(HOST_DFS_SPOOF_TEST)
    ic->ic_rebuilt_chanlist = 0;
    dfs_rx_ops = wlan_lmac_if_get_dfs_rx_ops(psoc);
    if (dfs_rx_ops) {
        if(dfs_rx_ops->dfs_reset_spoof_test)
            dfs_rx_ops->dfs_reset_spoof_test(scn->sc_pdev);
    }
#endif /* HOST_DFS_SPOOF_TEST */
#if defined(WLAN_DFS_FULL_OFFLOAD) && defined(QCA_DFS_NOL_OFFLOAD)
    /* Resetting the scan violation status on wifi. */
    ic->ic_is_dfs_scan_violation = false;
#endif
    if (ol_target_lithium(psoc)) {
        scn->is_scn_stats_timer_init = 1;
	scn->pdev_stats_timer = PDEV_STATS_DEFAULT_TIMER;
        qdf_timer_init(soc->qdf_dev, &(scn->scn_stats_timer), ol_ath_get_pdev_stats, (void *)scn, QDF_TIMER_TYPE_SW);
    }

#ifdef WLAN_SUPPORT_RX_PROTOCOL_TYPE_TAG
    ic->rx_pkt_protocol_tag_mask = 0;
#endif //WLAN_SUPPORT_RX_PROTOCOL_TYPE_TAG

    /* Initialize Spatial Reuse Enable bit from INI */
    ic->ic_he_sr_enable = cfg_get(psoc, CFG_OL_SR_ENABLE);

    /* Set SR ctrl HESIGA_Spatial_reuse_value15_allowed
     * in SR IE based off INI
     */
    ic->ic_he_srctrl_sr15_allowed =
            cfg_get(psoc, CFG_OL_SRP_SR_CONTROL) & IEEE80211_SRP_HESIGA_SR_VALUE15_ALLOWED_MASK;

    /* Set SR ctrl SRP Disallowed field by reading its value from SRG
     * Control Field value in INI
     */
    ic->ic_he_srctrl_srp_disallowed =
            cfg_get(psoc, CFG_OL_SRP_SR_CONTROL) & IEEE80211_SRP_DISALLOWED_MASK;

    /* Set SR ctrl Non-SRG OBSS PD SR Disallowed field of
     * SR IE by reading value from SRG Control Field in INI
     */
    ic->ic_he_srctrl_non_srg_obsspd_disallowed =
            cfg_get(psoc, CFG_OL_SRP_SR_CONTROL) & IEEE80211_SRP_NON_SRG_OBSS_PD_SR_DISALLOWED_MASK;

    /* Set all SRG related fields in SR IE
     * based on value read from SRG Control Field value in INI
     */
    ic->ic_he_srg_obss_pd_supported =
            cfg_get(psoc, CFG_OL_SRP_SR_CONTROL) & IEEE80211_SRP_SRG_INFO_PRESENT_MASK;

    /* Initialize Non-SRG OBSS PD MAX OFFSET from INI */
    ic->ic_he_non_srg_obsspd_max_offset = cfg_get(psoc, CFG_OL_SRP_NON_SRG_OBSS_PD_MAX_OFFSET);

    /* Initialize SRG OBSS PD MIN OFFSET from INI */
    ic->ic_he_srctrl_srg_obsspd_min_offset = cfg_get(psoc, CFG_OL_SRP_SRG_OBSS_PD_MIN_OFFSET);

    /* Initialize SRG OBSS PD MAX OFFSET from INI */
    ic->ic_he_srctrl_srg_obsspd_max_offset = cfg_get(psoc, CFG_OL_SRP_SRG_OBSS_PD_MAX_OFFSET);

    qdf_atomic_init(&(scn->auth_cnt));
    qdf_timer_init(soc->qdf_dev, &(scn->auth_timer), ol_ath_clear_auth_cnt, (void *)scn, QDF_TIMER_TYPE_SW);


    scn->max_auth = MAX_AUTH_REQUEST;
    scn->tx_ack_timeout = DEFAULT_TX_ACK_TIMEOUT;
    scn->is_ani_enable = DEFAULT_ANI_ENABLE_STATUS;

    /* Check for channels to be blocked for automatic channel selection (ACS, ICM, ...)*/
    ol_ath_pri20_cfg_blockchanlist_parse(ic, cfg_get(soc->psoc_obj, CFG_OL_PRI20_CFG_BLOCKCHANLIST));
    ic->ic_offchan_dwell_time = cfg_get(soc->psoc_obj, CFG_OL_OFFCHAN_SCAN_DWELL_TIME);
#ifdef OBSS_PD
    /* Set Spatial Reuse OBSS PD Threshold */
    ic->ic_ap_obss_pd_thresh = cfg_get(psoc, CFG_OL_SRP_NON_SRG_SELF_OBSS_PD_THRESHOLD);
    ol_ath_set_obss_thresh(ic,
            (cfg_get(psoc,CFG_OL_SRP_NON_SRG_OBSS_PD_MAX_OFFSET_ENABLE) &&
            ic->ic_is_spatial_reuse_enabled &&
            ic->ic_is_spatial_reuse_enabled(ic)), scn);
#endif //OBSS_PD
    return EOK;
}
void ol_ath_target_deinit(ol_ath_soc_softc_t *soc, int force)
{
    ol_txrx_soc_handle soc_txrx_handle;
    void *dp_ext_hdl;
    struct target_psoc_info *tgt_psoc_info;

    tgt_psoc_info = (struct target_psoc_info *)wlan_psoc_get_tgt_if_handle(
						soc->psoc_obj);
    if (tgt_psoc_info == NULL) {
        qdf_info("%s: target_psoc_info is null ", __func__);
	return;
    }

    soc_txrx_handle = wlan_psoc_get_dp_handle(soc->psoc_obj);

    ol_ath_disconnect_htc(soc);

    if (!bypasswmi) {
        if (soc_txrx_handle) {
            cdp_txrx_intr_detach(soc_txrx_handle);
            dp_ext_hdl = cdp_soc_get_dp_txrx_handle(soc_txrx_handle);
            cdp_soc_set_dp_txrx_handle(soc_txrx_handle, NULL);

            if (dp_ext_hdl)
                qdf_mem_free(dp_ext_hdl);

            ol_ath_soc_rate_stats_detach(soc);
            cdp_soc_deinit(soc_txrx_handle);
#ifdef WLAN_FEATURE_FASTPATH
            soc->htt_handle = NULL;
#endif
        }

        if (soc->dbg_log_init) {
            ol_ath_dbglog_detach(soc);
            soc->dbg_log_init = 0;
        }

    }

}

void ol_ath_target_deinit_complete(ol_ath_soc_softc_t *soc)
{
   struct common_htc_handle *htc_handle;
   void *hif_hdl;

   htc_handle = lmac_get_htc_hdl(soc->psoc_obj);
   hif_hdl = lmac_get_ol_hif_hdl(soc->psoc_obj);

   if (htc_handle) {
        htc_destroy(htc_handle);
        lmac_set_htc_hdl(soc->psoc_obj, NULL);
    }

    /* Cleanup BMI */
#ifdef WLAN_FEATURE_BMI
    if (soc->bmi_handle != NULL) {
        if (soc->bmi_handle->bmi_cleanup)
            soc->bmi_handle->bmi_cleanup(soc->bmi_handle,soc->sc_osdev);
        if (soc->bmi_handle->bmi_free)
            soc->bmi_handle->bmi_free(soc->bmi_handle);
    }
#endif
    if (hif_hdl != NULL) {
        ol_hif_close(hif_hdl);
        hif_hdl = NULL;
    }
}

int
ol_ath_soc_detach(ol_ath_soc_softc_t *soc, int force)
{
    int status = 0;
    struct mgmt_txrx_mgmt_frame_cb_info rx_cb_info;
    struct target_psoc_info *tgt_psoc_info;
    struct common_wmi_handle *wmi_handle;
    struct common_hif_handle *hif_hdl;
    struct pdev_op_args oper;
    ol_txrx_soc_handle soc_txrx_handle = NULL;

    if (ol_target_lithium(soc->psoc_obj)) {
        unregister_netdevice_notifier(&ath_device_notifier);
    }
    tgt_psoc_info = (struct target_psoc_info *)wlan_psoc_get_tgt_if_handle(
						soc->psoc_obj);
    if(tgt_psoc_info == NULL) {
        qdf_info("%s: target_psoc_info is null ", __func__);
        return -EINVAL;
    }

    wmi_handle = target_psoc_get_wmi_hdl(tgt_psoc_info);
    hif_hdl = target_psoc_get_hif_hdl(tgt_psoc_info);
    if ((wmi_handle == NULL) || (hif_hdl == NULL))
        return -EINVAL;

#if OL_ATH_SUPPORT_LED
    if (
#if QCA_LTEU_SUPPORT
        !(wlan_psoc_nif_feat_cap_get(soc->psoc_obj, WLAN_SOC_F_LTEU_SUPPORT)) &&
#endif
        !bypasswmi) {
            soc->led_blink_rate_table = NULL;
    }
#endif /* OL_ATH_SUPPORT_LED */

    if(soc->ol_if_ops->wds_addr_detach)
    	soc->ol_if_ops->wds_addr_detach((wmi_unified_t)wmi_handle);

    ol_mempools_detach(soc);

    ol_ath_phyerr_detach(soc);
    ol_ath_soc_chan_info_detach(soc);
#if  ATH_DATA_TX_INFO_EN
    ol_ath_stats_soc_detach(soc);
#endif
    ol_ath_wow_soc_detach(soc);

    /* UMAC Disatcher disable/stop all components under PSOC */
    dispatcher_psoc_disable(soc->psoc_obj);

    if (!(soc->down_complete))
        ol_ath_target_deinit(soc, force);
    else if (ol_target_lithium(soc->psoc_obj)) {
        oper.type = PDEV_ITER_PDEV_DETACH_OP;
        wlan_objmgr_iterate_obj_list(soc->psoc_obj, WLAN_PDEV_OP,
                wlan_pdev_operation, &oper, 0, WLAN_MLME_NB_ID);
    }

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    if (soc->nss_soc.ops) {
        soc->nss_soc.ops->nss_soc_desc_pool_free(soc);
    }
#endif

    soc_txrx_handle = wlan_psoc_get_dp_handle(soc->psoc_obj);


    wlan_psoc_set_dp_handle(soc->psoc_obj, NULL);
    cdp_soc_detach(soc_txrx_handle);

#ifdef QVIT
    ol_ath_qvit_detach(soc);
#endif

    if (!bypasswmi) {
        if (soc->dbg_log_init) {
            ol_ath_dbglog_detach(soc);
            soc->dbg_log_init = 0;
        }
    }

    if (!(soc->down_complete)) {
        hif_disable_isr((struct hif_opaque_softc *)hif_hdl);
        ol_ath_target_deinit_complete(soc);
    }
    dispatcher_psoc_close(soc->psoc_obj);

    if (!bypasswmi) {
        if (wmi_handle) {
            wmi_unified_detach((wmi_unified_t)wmi_handle);
#if WMI_RECORDING
            wmi_proc_remove((wmi_unified_t)wmi_handle, soc->wmi_proc_entry, soc->soc_idx);
#endif
            wmi_handle =NULL;
        }
    }

    ol_codeswap_detach(soc);

#ifdef AH_CAL_IN_FLASH_PCI
#ifndef ATH_CAL_NAND_FLASH
    if (soc->cal_mem) {
        A_IOUNMAP(soc->cal_mem);
        soc->cal_mem = 0;
    }
#endif
    soc->cal_in_flash = 0;
#endif

    soc->cal_in_file = 0;

    /* Deregister MgMt TxRx handler */
    rx_cb_info.frm_type = MGMT_FRAME_TYPE_ALL;
    rx_cb_info.mgmt_rx_cb = ieee80211_mgmt_input;
    wlan_mgmt_txrx_deregister_rx_cb(soc->psoc_obj, WLAN_UMAC_COMP_MLME,
                                       &rx_cb_info, 1);

    if (!(soc->down_complete)) {
         init_deinit_free_num_units(soc->psoc_obj, tgt_psoc_info);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,5,0)
    if(soc->alloc_task_wqueue != NULL) {
        ATH_FLUSH_WQUEUE(soc->alloc_task_wqueue);
        ATH_DESTROY_WQUEUE(soc->alloc_task_wqueue);
        soc->alloc_task_wqueue = NULL;
    }
#endif
    }

    /* No Target accesses of any kind after this point */
    return status;
}

int ol_ath_pdev_detach(struct ol_ath_softc_net80211 *scn, int force)
{
    struct ieee80211com     *ic = &scn->sc_ic;
    ol_ath_soc_softc_t *soc = scn->soc;
    int status = 0;
    struct ieee80211com     *tmp_ic;
#if DBDC_REPEATER_SUPPORT
    int j;
#endif
    int i = 0;

#if ATH_DATA_TX_INFO_EN
    /*Free buffer for data TX info*/
    qdf_mem_free(scn->tx_status_buf);
    scn->tx_status_buf = NULL;
#endif

    if (ol_target_lithium(scn->soc->psoc_obj)) {
            if (scn->is_scn_stats_timer_init)
                qdf_timer_sync_cancel(&scn->scn_stats_timer);
    }

#if  ATH_DATA_TX_INFO_EN
    if (!(scn->soc->down_complete)) {
        ol_ath_stats_detach(ic);
    }
#endif
    if (ol_target_lithium(soc->psoc_obj))
            qdf_timer_sync_cancel(&scn->scn_stats_timer);

    qdf_timer_sync_cancel(&(scn->auth_timer));

#if ATH_SUPPORT_WRAP
    qdf_spinlock_destroy(&scn->sc_mpsta_vap_lock);
#endif

    ol_ath_wow_detach(ic);

    /* deregister IEEE80211_DPRINTF_IC control object */
    ieee80211_dprintf_ic_deregister(ic);


    if (!bypasswmi) {
        ieee80211_stop_running(ic);

        /*
         * NB: the order of these is important:
         * o call the 802.11 layer before detaching the hal to
         *   insure callbacks into the driver to delete global
         *   key cache entries can be handled
         * o reclaim the tx queue data structures after calling
         *   the 802.11 layer as we'll get called back to reclaim
         *   node state and potentially want to use them
         * o to cleanup the tx queues the hal is called, so detach
         *   it last
         * Other than that, it's straightforward...
         */
        ieee80211_ifdetach(ic);
    }

    ol_regdmn_detach(scn->ol_regdmn_handle);
    ol_ath_mgmt_detach(ic);

#if WLAN_SPECTRAL_ENABLE
    /* Unregister spectral event handlers with DDMA */
    spectral_unregister_dbr(ic->ic_pdev_obj);
#endif /* WLAN_SPECTRAL_ENABLE */

#if WLAN_CFR_ENABLE
    /* pdev deinit to be done on Target Stop */
    cfr_deinitialize_pdev(ic->ic_pdev_obj);
#endif

    qdf_event_destroy(&ic->ic_wait_for_init_cc_response);

#if QCA_LTEU_SUPPORT
    ol_ath_nl_detach(ic);
#endif

#ifndef REMOVE_PKT_LOG
    if (scn->pl_dev){
        ol_pktlog_detach(scn);
    ol_pl_freehandle(scn->pl_dev);
        scn->pl_dev = NULL;
    }
#endif

#if !ATH_SUPPORT_LOWI && ATH_SUPPORT_WIFIPOS
    ol_if_rtt_detach(ic);
#endif

#if ATH_PROXY_NOACK_WAR
    if (scn->sc_proxy_noack_war) {
        OS_FREE_TIMER(&(ic->ic_ast_reserve_timer));
    }
#endif

    /* Remove ic from global list */
    for (i=0; i < MAX_RADIO_CNT; i++) {
        tmp_ic = ic_list.global_ic[i];
        if (tmp_ic && (ic == tmp_ic)) {
            GLOBAL_IC_LOCK_BH(&ic_list);
            ic_list.global_ic[i] = NULL;
            ic_list.num_global_ic--;
            qdf_info("%s: remove global_ic[%d]..gloabl_ic ptr:%pK", __func__, ic_list.num_global_ic,&ic_list);
            GLOBAL_IC_UNLOCK_BH(&ic_list);
        }
    }
#if DBDC_REPEATER_SUPPORT
    for (i=0; i < MAX_RADIO_CNT; i++) {
        GLOBAL_IC_LOCK_BH(&ic_list);
        tmp_ic = ic_list.global_ic[i];
        GLOBAL_IC_UNLOCK_BH(&ic_list);
        if (tmp_ic && (tmp_ic != ic)) {
            for (j=0; j < MAX_RADIO_CNT-1 ; j++) {
                if (tmp_ic->other_ic[j] == ic) {
                    spin_lock(&tmp_ic->ic_lock);
                    tmp_ic->other_ic[j] = NULL;
                    spin_unlock(&tmp_ic->ic_lock);
                    break;
                }
            }
        }
    }
#endif

#if UMAC_SUPPORT_CFG80211
    if (ic->ic_cfg80211_config) {
      if(ic->ic_cfg80211_radio_detach) {
        ic->ic_cfg80211_radio_detach(ic);
      }
    }
#endif

#ifdef QVIT
    ol_ath_pdev_qvit_detach(scn);
#endif

#if OL_ATH_SUPPORT_LED
    if (
#if QCA_LTEU_SUPPORT
        !(wlan_psoc_nif_feat_cap_get(scn->soc->psoc_obj, WLAN_SOC_F_LTEU_SUPPORT)) &&
#endif
        !bypasswmi) {
            qdf_timer_sync_cancel(&scn->scn_led_blink_timer);
            qdf_timer_sync_cancel(&scn->scn_led_poll_timer);
            if(lmac_get_tgt_type(scn->soc->psoc_obj) == TARGET_TYPE_IPQ4019) {
                    ipq4019_wifi_led_deinit(scn);
	    }
    }
#endif /* OL_ATH_SUPPORT_LED */

#if OS_SUPPORT_ASYNC_Q
    OS_MESGQ_DRAIN(&scn->sc_osdev->async_q,NULL);
    OS_MESGQ_DESTROY(&scn->sc_osdev->async_q);
#endif
#ifndef BUILD_X86
    qcom_pcie_deregister_event(&soc->pcie_event);
#endif
    return status;
}

void ol_reset_params(struct ol_ath_softc_net80211 *scn)
{
    struct ieee80211com *ic = &scn->sc_ic;

    /* should always be equal to define DEFAULT_LOWEST_RATE_IN_5GHZ 0x03  6 Mbps  in firmware */
    scn->ol_rts_cts_rate                    = 0x03;
#if WDI_EVENT_ENABLE
    scn->scn_user_peer_invalid_cnt          = 0;/* By default we will send one deauth in 10 msec in response to rx_peer_invalid */
#endif
    scn->dyngroup                           = 0;
    scn->dpdenable                          = 1;
    scn->scn_amsdu_mask                     = 0xffff;
    scn->scn_amsdu_mask                     = 0xffff;
    scn->txpower_scale                      = 0;
    scn->powerscale                         = 0;
    scn->scn_dcs.dcs_enable                 = 0;
    scn->scn_dcs.coch_intr_thresh           = DCS_COCH_INTR_THRESHOLD ;
    scn->scn_dcs.tx_err_thresh              = DCS_TXERR_THRESHOLD;
    scn->scn_dcs.phy_err_threshold          = DCS_PHYERR_THRESHOLD ;
    scn->scn_dcs.user_max_cu                = DCS_USER_MAX_CU; /* tx_cu + rx_cu */
    scn->scn_dcs.dcs_debug                  = DCS_DEBUG_DISABLE;
    scn->burst_dur                          = 0;
    scn->burst_enable                       = 1;
    scn->is_ani_enable                      = DEFAULT_ANI_ENABLE_STATUS;
    scn->dtcs                               = 0; /* Dynamic Tx Chainmask Selection enabled/disabled */
    scn->vow_extstats                       = 0;
    scn->sc_arp_dbg_conf                    = 0;
#ifdef QCA_SUPPORT_CP_STATS
    pdev_cp_stats_ap_stats_tx_cal_enable_update(ic->ic_pdev_obj, 0);
#endif
    scn->macreq_enabled                     = 0; /* user mac request feature enable/disable */
    scn->sc_bsta_fixed_idmask               = 0xff; /* Mask Value to set fixed mac addr for Backhaul STA. Default 255 */
    scn->arp_override                       = WME_AC_VO;
    scn->igmpmld_override                   = 0;
    scn->igmpmld_tid                        = 0;
#if ATH_RX_LOOPLIMIT_TIMER
    scn->rx_looplimit_timeout               = 1;           /* 1 ms by default */
    scn->rx_looplimit_valid                 = true;          /* set to valid after initilization */
    scn->rx_looplimit                       = false;
#endif
    scn->sc_prealloc_idmask                 = 0;
    scn->sc_is_blockdfs_set                 = false;
    scn->scn_qboost_enable                  = 0;
    scn->scn_sifs_frmtype                   = 0;
    scn->scn_sifs_uapsd                     = 0;
    scn->scn_block_interbss                 = 0;
    scn->txbf_sound_period                  = 100;
    scn->scn_promisc                        = 0;
    scn->enable_statsv2                     = 0;
#if ATH_SUPPORT_WRAP
    scn->mcast_bcast_echo                   = 0;
#endif
#if ATH_DATA_TX_INFO_EN
    scn->enable_perpkt_txstats              = 0;
#endif
    scn->tx_ack_timeout                     = DEFAULT_TX_ACK_TIMEOUT;
#ifdef QCA_SUPPORT_CP_STATS
    pdev_cp_stats_wmi_tx_mgmt_update(ic->ic_pdev_obj, 0);
    pdev_cp_stats_wmi_tx_mgmt_completions_update(ic->ic_pdev_obj, 0);
    pdev_cp_stats_wmi_tx_mgmt_completion_err_update(ic->ic_pdev_obj, 0);
#endif

    /*
     * CCK is enabled by default in the firmware for all the chipsets.
     */
    ic->cck_tx_enable = true;

    return;
}

int ol_ath_soc_stop(struct ol_ath_soc_softc *soc, bool flush_wq)
{
    struct net_device *dev;
    struct pci_dev *pcidev;
    int target_paused = TRUE;
    uint32_t target_type;
    struct wlan_objmgr_psoc *psoc = NULL;
    struct target_psoc_info *tgt_hdl;
    struct wlan_psoc_host_service_ext_param *ext_param = NULL;
    struct common_hif_handle *hif_hdl;
    ol_txrx_soc_handle soc_txrx_handle;
    struct pdev_op_args oper;

    psoc = soc->psoc_obj;
    target_type = lmac_get_tgt_type(psoc);
    tgt_hdl = (struct target_psoc_info *)wlan_psoc_get_tgt_if_handle(psoc);
    if(!tgt_hdl) {
    	qdf_info("%s: psoc target_psoc_info is null", __func__);
    	return -EINVAL;
    }

    hif_hdl = target_psoc_get_hif_hdl(tgt_hdl);
    ext_param = &(tgt_hdl->info.service_ext_param);

    dev = soc->sc_osdev->netdev;
    pcidev = (struct pci_dev *)soc->sc_osdev->bdev;

    if (atomic_read(&soc->reset_in_progress)) {
        qdf_info("Reset in progress, return");
        return -1;
    }

    if (soc->down_complete) {
        /*
         * Target has already been stopped
         */
        return 0;
    }

     /* To avoid PCI register access after pci unmap,
      * flush any pending pci_reconnect_work */
    if (flush_wq) {
        qdf_flush_work(&soc->pci_reconnect_work);
    }

    soc->soc_attached = 0;
    atomic_inc(&soc->reset_in_progress);
#if OL_ATH_SUPPORT_LED
    soc->led_blink_rate_table = NULL;
#endif

    oper.type = PDEV_ITER_PDEV_DEINIT_BEFORE_SUSPEND;
    wlan_objmgr_iterate_obj_list(soc->psoc_obj, WLAN_PDEV_OP,
              wlan_pdev_operation, &oper, 0, WLAN_MLME_NB_ID);
    /* Suspend Target if not recovering */
    if (soc->recovery_in_progress == 0) {
        qdf_info("Suspending Target  soc=%pK", soc);
        if (!ol_ath_suspend_target(soc, 1)) {
            u_int32_t  timeleft;
            qdf_info("%s: waiting for target paused event from target ",__func__);
            /* wait for the event from Target*/
            timeleft = wait_event_interruptible_timeout(soc->sc_osdev->event_queue,
                (soc->is_target_paused == TRUE),
                200);
            if(!timeleft || signal_pending(current)) {
            qdf_info("ERROR: Failed to receive target paused event soc=%pK ", soc);
            target_paused = FALSE;
            }
            /*
             * reset is_target_paused and host can check that in next time,
             * or it will always be TRUE and host just skip the waiting
             * condition, it causes target assert due to host already suspend
             */
            soc->is_target_paused = FALSE;
        }
    }

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    if (soc->nss_soc.ops) {
            qdf_info("nss-wifi: detach soc->soc_idx: %d - 100 delay ", soc->soc_idx);
            soc->nss_soc.ops->nss_soc_wifi_detach(soc, 100);
    }
#endif

    soc_txrx_handle = wlan_psoc_get_dp_handle(psoc);
    qdf_info("disable dp interrupts:: soc: %pK", soc_txrx_handle);
    hif_disable_isr((struct hif_opaque_softc *)hif_hdl);
    cdp_txrx_intr_detach(soc_txrx_handle);

    oper.type = PDEV_ITER_PDEV_DEINIT_OP;
    wlan_objmgr_iterate_obj_list(soc->psoc_obj, WLAN_PDEV_OP,
            wlan_pdev_operation, &oper, 0, WLAN_MLME_NB_ID);

    ol_ath_target_deinit(soc, 1);

    ol_ath_target_deinit_complete(soc);
    if (ext_param) {
        /* free chain mask table */
        init_deinit_chainmask_table_free(ext_param);
    }

    init_deinit_dbr_ring_cap_free(tgt_hdl);
    init_deinit_spectral_scaling_params_free(tgt_hdl);

    if((target_type !=  TARGET_TYPE_IPQ4019) &&
       (target_type !=  TARGET_TYPE_QCA6290) &&
       (target_type !=  TARGET_TYPE_QCA8074) &&
       (target_type != TARGET_TYPE_QCA8074V2) &&
       (target_type != TARGET_TYPE_QCA6018)) {
        pci_set_drvdata(pcidev, dev);
    }

    init_deinit_free_num_units(soc->psoc_obj, tgt_hdl);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,5,0)
    if(soc->alloc_task_wqueue != NULL) {
        ATH_FLUSH_WQUEUE(soc->alloc_task_wqueue);
        ATH_DESTROY_WQUEUE(soc->alloc_task_wqueue);
        soc->alloc_task_wqueue = NULL;
    }
#endif

    target_psoc_set_wmi_ready(tgt_hdl, FALSE);
    target_psoc_set_wmi_service_ready(tgt_hdl, FALSE);
    wlan_psoc_nif_op_flag_clear(psoc, WLAN_SOC_OP_VHT_INVALID_CAP);
    soc->dbg.print_rate_limit = DEFAULT_PRINT_RATE_LIMIT_VALUE;

    oper.type = PDEV_ITER_PDEV_RESET_PARAMS;
    wlan_objmgr_iterate_obj_list(soc->psoc_obj, WLAN_PDEV_OP,
                 wlan_pdev_operation, &oper, 0, WLAN_MLME_NB_ID);

    ol_ath_diag_user_agent_fini(soc);

    soc->down_complete = true;
    atomic_dec(&soc->reset_in_progress);
    return 0;
}


/* Suspend and unload the firmware during the last vap removal */
int ol_ath_target_stop(struct ieee80211com *ic, bool flush_wq)
{
    struct ol_ath_softc_net80211 *scn;
    struct ol_ath_soc_softc *soc;
    struct platform_device *pdev;
#ifdef CONFIG_CNSS2_SUPPORT
    struct pci_dev *pcidev = NULL;
#endif
    int tgt_type = 0;

    /* get the soc struct from scn */
    scn =  OL_ATH_SOFTC_NET80211(ic);
    soc = scn->soc;
    pdev = (struct platform_device *)soc->sc_osdev->bdev;
    tgt_type = lmac_get_tgt_type(soc->psoc_obj);

#ifdef QCA_WIFI_QCA8074_VP
    if(tgt_type == TARGET_TYPE_QCA6018) {
	    qdf_info("Skip ol_ath_target_stop for TARGET_TYPE_QCA6018\n");
	    return -1;
    }
#endif

    ol_ath_soc_stop(soc, flush_wq);

#ifdef CONFIG_CNSS2_SUPPORT

    if ((tgt_type == TARGET_TYPE_QCA8074) ||
        (tgt_type == TARGET_TYPE_QCA8074V2) ||
        (tgt_type == TARGET_TYPE_QCA6018) ||
        (tgt_type == TARGET_TYPE_QCA6290)) {
        /* pld_wlan_disable is called inside hif_wlan_disable */
        if ((tgt_type == TARGET_TYPE_QCA8074) ||
            (tgt_type == TARGET_TYPE_QCA8074V2) ||
            (tgt_type == TARGET_TYPE_QCA6018)) {
                pld_subsystem_put(&pdev->dev);
        }
        if (tgt_type == TARGET_TYPE_QCA6290) {
                pcidev = (struct pci_dev *)pdev;
                pld_subsystem_put(&pcidev->dev);
        }
    }
#endif
    return 0;
}
qdf_export_symbol(ol_ath_target_stop);

int ol_ath_soc_start(struct ol_ath_soc_softc *soc)
{
    struct net_device *netdev;
    struct platform_device *pdev;
    uint32_t pdev_idx;
    struct pci_dev *pcidev;
    struct _NIC_DEV *drv;
    int i, retval = 0;
#ifdef CONFIG_CNSS2_SUPPORT
    int count = 0;
    struct device *dev = NULL;
#endif
    struct target_psoc_info *tgt_hdl;
    struct wlan_objmgr_psoc *psoc = soc->psoc_obj;
    struct wlan_psoc_host_mac_phy_caps *mac_phy_cap;
    uint32_t target_type;
    uint32_t target_version;
    struct wlan_psoc_target_capability_info *target_cap;
    ol_txrx_soc_handle soc_txrx_handle;
    ol_txrx_pdev_handle pdev_txrx_handle;
    void *hif_hdl;
#if defined(WLAN_DFS_PARTIAL_OFFLOAD) && defined(HOST_DFS_SPOOF_TEST)
    struct wlan_lmac_if_dfs_rx_ops *dfs_rx_ops;
#endif /* HOST_DFS_SPOOF_TEST */
    uint8_t num_radios;
    tgt_hdl = (struct target_psoc_info *)wlan_psoc_get_tgt_if_handle(psoc);
    if(!tgt_hdl) {
    	qdf_info("%s: psoc target_psoc_info is null", __func__);
    	return -EIO;
    }
    target_cap = target_psoc_get_target_caps(tgt_hdl);
    mac_phy_cap = target_psoc_get_mac_phy_cap(tgt_hdl);
    target_type = target_psoc_get_target_type(tgt_hdl);
    target_version = target_psoc_get_target_ver(tgt_hdl);

    pdev = (struct platform_device *)soc->sc_osdev->bdev;
    pcidev = (struct pci_dev *)soc->sc_osdev->bdev;
    netdev = soc->sc_osdev->netdev;
    atomic_inc(&soc->reset_in_progress);
    soc_txrx_handle = wlan_psoc_get_dp_handle(psoc);
    num_radios = target_psoc_get_num_radios(tgt_hdl);

#ifdef CONFIG_CNSS2_SUPPORT
    if ((target_type == TARGET_TYPE_QCA8074) ||
		    (target_type == TARGET_TYPE_QCA8074V2) ||
		    (target_type == TARGET_TYPE_QCA6018)) {
        dev = &pdev->dev;
    } else {
        dev = &pcidev->dev;
    }
    if ((target_type == TARGET_TYPE_QCA8074) ||
        (target_type == TARGET_TYPE_QCA8074V2) ||
        (target_type == TARGET_TYPE_QCA6018) ||
        (target_type == TARGET_TYPE_QCA6290)) {
            while(pld_is_fw_ready(dev) == 0) {
                    mdelay(PLD_FW_READY_DELAY);
                    if(count++ > PLD_FW_READY_TIMEOUT) {
                        qdf_info("Error: QMI FW ready timeout %d seconds",
                                 PLD_FW_READY_TIMEOUT/10);
                        cnss_dump_qmi_history();
                        qdf_assert_always(0);
                    }
            }
    }
#endif

    drv = soc->qdf_dev->drv;

    OS_MEMSET(soc->qdf_dev, 0, sizeof(*(soc->qdf_dev)));

    soc->qdf_dev->drv = drv;

    if((target_type == TARGET_TYPE_IPQ4019) ||
       (target_type == TARGET_TYPE_QCA8074) ||
       (target_type == TARGET_TYPE_QCA8074V2) ||
       (target_type == TARGET_TYPE_QCA6018)) {
        soc->qdf_dev->dev = &pdev->dev; /* device */
        soc->qdf_dev->drv_hdl = NULL; /* bus handle */
        hif_hdl = ol_hif_open(&pdev->dev, pdev, (void *)soc->platform_devid, QDF_BUS_TYPE_AHB, 0,soc->qdf_dev);
    } else {
        soc->qdf_dev->dev = &pcidev->dev; /* device */
        soc->qdf_dev->drv_hdl = pcidev; /* bus handle */
        if (target_type != TARGET_TYPE_QCA6290)
                pci_set_drvdata(pcidev, NULL);
        hif_hdl = ol_hif_open(&pcidev->dev, pcidev, (void *)soc->platform_devid, QDF_BUS_TYPE_PCI, 0,soc->qdf_dev);
        if (target_type != TARGET_TYPE_QCA6290)
                pci_set_drvdata(pcidev, netdev);
    }

    wlan_psoc_set_qdf_dev(psoc, soc->qdf_dev);
    if (hif_hdl == NULL) {
        goto fail3;
    }
    target_psoc_set_hif_hdl(tgt_hdl, hif_hdl);
    soc->targetdef = hif_get_targetdef(hif_hdl);

    target_psoc_set_total_mac_phy_cnt(tgt_hdl, 0);
    target_psoc_set_num_radios(tgt_hdl, 0);
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    if (soc->nss_soc.ops) {
            if (soc->nss_soc.ops->nss_soc_wifi_init(soc)) {
                    goto fail;
            }
    }
#endif

    if (soc->ol_if_ops->target_init) {
        if (soc->ol_if_ops->target_init(soc, true))
            goto fail;
    }

    if (ol_target_init_complete(soc))
        goto fail;

    soc_txrx_handle = wlan_psoc_get_dp_handle(psoc);
    if (!soc_txrx_handle) {
        qdf_info("%s: psoc dp handle is null", __func__);
        goto fail;
    }

#ifdef DIRECT_BUF_RX_ENABLE
    if (direct_buf_rx_target_attach(soc->psoc_obj,
                          hif_get_hal_handle(lmac_get_hif_hdl(soc->psoc_obj)),
                          soc->qdf_dev) != QDF_STATUS_SUCCESS) {
        qdf_info("%s: Direct Buffer Rx Target Attach Failed\n", __func__);
    }
#endif


    for (i = 0; i < num_radios; i++) {
        struct net_device *pdev_netdev;
        struct ol_ath_softc_net80211 *scn;
        struct ieee80211com *ic;
        struct wlan_objmgr_pdev *pdev_obj;

        pdev_obj = wlan_objmgr_get_pdev_by_id(psoc, i, WLAN_MLME_NB_ID);
        if (pdev_obj == NULL) {
             qdf_info("%s: pdev object (id: %d) is NULL ", __func__, i);
             return -1;
        }

        scn = lmac_get_pdev_feature_ptr(pdev_obj);
        ic = &scn->sc_ic;
	pdev_idx = lmac_get_pdev_idx(scn->sc_pdev);

        pdev_txrx_handle = wlan_pdev_get_dp_handle(pdev_obj);
        pdev_netdev = scn->netdev;
        if (!bypasswmi) {
            if ((target_type != TARGET_TYPE_QCA8074) &&
			    (target_type != TARGET_TYPE_QCA8074V2) &&
			    (target_type != TARGET_TYPE_QCA6018) &&
			    (target_type != TARGET_TYPE_QCA6290)) {
                if (target_version != AR6004_VERSION_REV1_3) {
                    if (cdp_pdev_attach_target(soc_txrx_handle,
                        (void *)pdev_txrx_handle) != A_OK) {
                        qdf_info("%s: txrx pdev attach target failed",__func__);
                        wlan_objmgr_pdev_release_ref(pdev_obj, WLAN_MLME_NB_ID);
                        return -1;
                    }
                }
            } else {
                qdf_info("Skip txrx_pdev_attach_target");
            }
        }

        if ((target_type != TARGET_TYPE_QCA8074) &&
            (target_type != TARGET_TYPE_QCA8074V2) &&
            (target_type != TARGET_TYPE_QCA6018)) {
            cdp_monitor_set_filter_ucast_data(soc_txrx_handle,
                    (void *)pdev_txrx_handle,0);
            cdp_monitor_set_filter_mcast_data(soc_txrx_handle,
                    (void *)pdev_txrx_handle,0);
            cdp_monitor_set_filter_non_data(soc_txrx_handle,
                    (void *)pdev_txrx_handle,0);
        }

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
        if ((target_type != TARGET_TYPE_QCA8074) &&
            (target_type != TARGET_TYPE_QCA8074V2) &&
            (target_type != TARGET_TYPE_QCA6018)) {
            qdf_info("nss-wifi: update radio nss context with soc context for legay radio");
            scn->nss_radio.nss_rctx = soc->nss_soc.nss_sctx;
            scn->nss_radio.nss_rifnum = soc->nss_soc.nss_sifnum;
            scn->nss_radio.nss_idx = soc->nss_soc.nss_sidx;
        }
#endif

        /*
         * Enable Bursting by default for pre-ES2 release only. Tobe removed Later
         */
        {
            int ac, duration, value, retval;
            ac = 0, retval = 0;
            duration = AGGR_BURST_DURATION;
            value = AGGR_PPDU_DURATION;
            qdf_info("BURSTING enabled by default");
            for(ac=0;ac<=3;ac++) {
                retval = ol_ath_pdev_set_param(scn,
                        wmi_pdev_param_aggr_burst,
                        (ac & AGGR_BURST_AC_MASK) << AGGR_BURST_AC_OFFSET |
                        (AGGR_BURST_DURATION_MASK & duration));
            }
            if( EOK == retval) {
                scn->aggr_burst_dur[0] = duration;
            }

            ol_ath_pdev_set_param(scn,
                    wmi_pdev_param_set_ppdu_duration_cmdid, value);
        }

        ol_ath_twt_enable_command(scn);
        /* Enable beacon bursting by default */
        retval = ol_ath_pdev_set_param(scn, wmi_pdev_param_beacon_tx_mode,
                                       scn->bcn_mode);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
        SET_MODULE_OWNER(pdev_netdev);
#endif

        /* Don't leave arp type as ARPHRD_ETHER as this is no eth device */
        pdev_netdev->type = ARPHRD_IEEE80211;

#if PERF_FIND_WDS_NODE
        wds_table_init(&scn->scn_wds_table);
#endif

#ifdef QCA_LOWMEM_PLATFORM
        ic->ic_num_clients = IEEE80211_33_AID;
#else
        if (target_type == TARGET_TYPE_AR9888)
        {
            ic->ic_num_clients = IEEE80211_128_AID;
        }
        else
        {
            ic->ic_num_clients = IEEE80211_512_AID;
        }
#endif

#if WLAN_SPECTRAL_ENABLE
        spectral_register_dbr(ic->ic_pdev_obj);
#endif /* WLAN_SPECTRAL_ENABLE */

#if WLAN_CFR_ENABLE
    /* pdev init to be done on Target Start */
        cfr_initialize_pdev(ic->ic_pdev_obj);
#endif
        ol_ath_mgmt_attach(ic);
#if  ATH_DATA_TX_INFO_EN
        ol_ath_stats_attach(ic);
#endif

#ifdef QCA_SUPPORT_CP_STATS
        if (pdev_cp_stats_ap_stats_tx_cal_enable_get(ic->ic_pdev_obj)) {
            scn->sc_ic.ic_ath_enable_ap_stats(&scn->sc_ic, 1);
            pdev_cp_stats_ap_stats_tx_cal_enable_update(ic->ic_pdev_obj, 0);
        }
#endif

        ieee80211com_set_cap(ic, IEEE80211_C_WEP);
        ieee80211com_set_cap(ic, IEEE80211_C_AES);
        ieee80211com_set_cap(ic, IEEE80211_C_AES_CCM);
        ieee80211com_set_cap(ic, IEEE80211_C_CKIP);
        ieee80211com_set_cap(ic, IEEE80211_C_TKIP);
        ieee80211com_set_cap(ic, IEEE80211_C_TKIPMIC);

#if ATH_SUPPORT_WAPI
        ieee80211com_set_cap(ic, IEEE80211_C_WAPI);
#endif

        wlan_scan_update_channel_list(ic);

#ifdef ATH_SUPPORT_WAPI
        if((target_type == TARGET_TYPE_QCA9984)
             || (target_type == TARGET_TYPE_QCA9888)
             || (target_type == TARGET_TYPE_IPQ4019)){
             A_UINT8 bit_loc;
             ATH_VAP_ID_BIT_LOC(bit_loc);
             qdf_info("%s[%d] WAPI MBSSID %d ",__func__,__LINE__,bit_loc);
             ol_ath_pdev_set_param(scn, wmi_pdev_param_wapi_mbssid_offset, bit_loc);
        }
#endif
        if (ol_ath_thermal_mitigation_attach(scn, scn->netdev)) {
          wlan_objmgr_pdev_release_ref(pdev_obj, WLAN_MLME_NB_ID);
          goto fail;
        }

#if OL_ATH_SUPPORT_LED
        if (!bypasswmi) {
#if QCA_LTEU_SUPPORT
          if (wlan_psoc_nif_feat_cap_get(soc->psoc_obj, WLAN_SOC_F_LTEU_SUPPORT)) {
              ol_gpio_config(scn, scn->scn_led_gpio, 1, WMI_HOST_GPIO_PULL_DOWN, WMI_HOST_GPIO_INTTYPE_RISING_EDGE);
          } else {
#endif /* QCA_LTEU_SUPPORT */

              /* HAWKEYE-WAR for SOC emulation: gpio configuration is not available for SOC Emulation
               * hence call to gpio config (WMI command for gpio config is blocked
               */
              if ((target_type == TARGET_TYPE_QCA8074) ||
			      (target_type == TARGET_TYPE_QCA8074V2) ||
			      (target_type == TARGET_TYPE_QCA6018) ||
			      (target_type == TARGET_TYPE_QCA6290)) {
                  /* Skip as current gpio configuration is not available. */
              } else if(target_type == TARGET_TYPE_IPQ4019) {
                  /* Do not enable LED for IPQ4019 during attach, as wifi LED will keep
                     glowing even if vaps are not created for that radio */
              } else {
                  ol_gpio_config(scn, scn->scn_led_gpio, 0, 0, 0);
                  ol_ath_gpio_output(scn, scn->scn_led_gpio, 1);
              }
              OS_INIT_TIMER(soc->sc_osdev, &(scn->scn_led_blink_timer), ol_ath_led_blink_timed_out, (void *)scn, QDF_TIMER_TYPE_WAKE_APPS);
              OS_INIT_TIMER(soc->sc_osdev, &(scn->scn_led_poll_timer), ol_ath_led_poll_timed_out, (void *)scn, QDF_TIMER_TYPE_WAKE_APPS);
              scn->scn_blinking = OL_BLINK_DONE;
              scn->scn_led_byte_cnt = 0;
              scn->scn_led_total_byte_cnt = 0;
              scn->scn_led_last_time = CONVERT_SYSTEM_TIME_TO_MS(OS_GET_TIMESTAMP());
              soc->led_blink_rate_table         = ol_led_blink_rate_table;
              scn->scn_led_max_blink_rate_idx = ARRAY_LENGTH(ol_led_blink_rate_table) - 1;
#if QCA_LTEU_SUPPORT
          }
#endif /* QCA_LTEU_SUPPORT */
        }
#endif /* OL_ATH_SUPPORT_LED */
        wlan_objmgr_pdev_release_ref(pdev_obj, WLAN_MLME_NB_ID);

        ol_ath_update_chainmask(ic, target_cap, &mac_phy_cap[pdev_idx]);
        ol_ath_pdev_dfs_phyerr_offload_en(scn);
#if defined(WLAN_DFS_PARTIAL_OFFLOAD) && defined(HOST_DFS_SPOOF_TEST)
        ic->ic_rebuilt_chanlist = 0;
        dfs_rx_ops = wlan_lmac_if_get_dfs_rx_ops(psoc);
        if (dfs_rx_ops) {
            if(dfs_rx_ops->dfs_reset_spoof_test)
                dfs_rx_ops->dfs_reset_spoof_test(scn->sc_pdev);
        }
#endif /* HOST_DFS_SPOOF_TEST */

#if defined(WLAN_DFS_FULL_OFFLOAD) && defined(QCA_DFS_NOL_OFFLOAD)
        /* Resetting the scan violation status on FW restart. */
        ic->ic_is_dfs_scan_violation = false;
#endif

#ifdef OL_ATH_SMART_LOGGING
        {
            struct common_wmi_handle *wmi_handle;

            wmi_handle = target_psoc_get_wmi_hdl(tgt_hdl);
            if (wmi_service_enabled(wmi_handle,
                                    wmi_service_smart_logging_support)) {
                void *dbglog_handle = target_psoc_get_dbglog_hdl(tgt_hdl);

                if (dbglog_handle && !ic->smart_logging)
                    fwdbg_smartlog_init(dbglog_handle, ic);
            }
	}
#endif /* OL_ATH_SMART_LOGGING */

        if (lmac_is_target_ar900b(soc->psoc_obj)) {
           /* HIF subscribe */
#ifndef REMOVE_PKT_LOG
           if (soc->ol_if_ops->hif_pktlog_subscribe) {
		if (soc->ol_if_ops->hif_pktlog_subscribe(scn) != QDF_STATUS_SUCCESS) {
		    QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY,
				QDF_TRACE_LEVEL_INFO,
				"Unable to subscribe to the HIF %s\n",__FUNCTION__);
		}
           }
#endif
        }
    } /* for loop */

    ol_ath_diag_user_agent_init(soc);
    soc->down_complete = false;
    atomic_dec(&soc->reset_in_progress);
    soc->soc_attached = 1;
    return 0;

fail:
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,5,0)
    if(soc->alloc_task_wqueue != NULL) {
        ATH_FLUSH_WQUEUE(soc->alloc_task_wqueue);
        ATH_DESTROY_WQUEUE(soc->alloc_task_wqueue);
        soc->alloc_task_wqueue = NULL;
    }
#endif

   hif_disable_isr(hif_hdl);

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
   if (soc->nss_soc.ops) {
       qdf_info("nss-wifi: detach - zero delay");
       soc->nss_soc.ops->nss_soc_wifi_detach(soc, 0);
   }
#endif

    /* Unlikely case.
     * Flush pending pci_reconnect_work
     */
    qdf_flush_work(&soc->pci_reconnect_work);
    ol_ath_target_deinit(soc, 1);
    ol_ath_target_deinit_complete(soc);
    if((target_type !=  TARGET_TYPE_IPQ4019) &&
       (target_type != TARGET_TYPE_QCA8074) &&
       (target_type != TARGET_TYPE_QCA8074V2) &&
       (target_type != TARGET_TYPE_QCA6018)) {
        pci_set_drvdata(pcidev, soc->sc_osdev->netdev);
    }

fail3:
    init_deinit_free_num_units(soc->psoc_obj, tgt_hdl);
    target_psoc_set_wmi_ready(tgt_hdl, FALSE);
    target_psoc_set_wmi_service_ready(tgt_hdl, FALSE);
    wlan_psoc_nif_op_flag_clear(psoc, WLAN_SOC_OP_VHT_INVALID_CAP);
    soc->down_complete = true;
    atomic_dec(&soc->reset_in_progress);
    return -1;
}

/* Load the firmware and enable interrupts during the first vap creation
 * or before sending any WMI command to the target.
 */
extern const struct of_device_id ath_wifi_of_match[];
#define IPQ4019_DEVICE_ID (0x12ef)
int ol_ath_target_start(struct ol_ath_soc_softc *soc)
{
    struct platform_device *pdev;
#ifdef CONFIG_CNSS2_SUPPORT
    void *subhandle = NULL;
    struct pci_dev *pcidev = NULL;
    int target_type = lmac_get_tgt_type(soc->psoc_obj);
#endif

    pdev = (struct platform_device *)soc->sc_osdev->bdev;

    if (atomic_read(&soc->reset_in_progress)) {
        qdf_info("Reset in progress, return");
        return -1;
    }

    if (!(soc->down_complete)) {
        /*
         * Target has already been started
         */
        return 0;
    }
#ifdef CONFIG_CNSS2_SUPPORT
    if ((target_type == TARGET_TYPE_QCA8074) ||
        (target_type == TARGET_TYPE_QCA8074V2) ||
        (target_type == TARGET_TYPE_QCA6018) ||
        (target_type == TARGET_TYPE_QCA6290)) {
        if ((target_type == TARGET_TYPE_QCA8074) ||
                (target_type == TARGET_TYPE_QCA8074V2) ||
                (target_type == TARGET_TYPE_QCA6018)) {
            subhandle = pld_subsystem_get(&pdev->dev, soc->device_id);
        }

        if(target_type == TARGET_TYPE_QCA6290) {
            //this pcidev would have got removed during subsystem put
            pld_power_on(NULL,PLD_BUS_TYPE_PCIE,QCA6290_DEVICE_ID);
            pld_rescan_bus(PLD_BUS_TYPE_PCIE);
            pcidev = (struct pci_dev *) pdev;
            subhandle = pld_subsystem_get(&pcidev->dev, soc->device_id);
            //get the new pci_device after enumeration and update the structure
            soc->sc_osdev->bdev = pld_get_pdev_device_id(soc->device_id,
                                                         PLD_BUS_TYPE_PCIE);
            pcidev = (struct pci_dev *) soc->sc_osdev->bdev;
            soc->sc_osdev->device = &((struct pci_dev *)soc->sc_osdev->bdev)->dev;
        }
        if(IS_ERR_OR_NULL(subhandle)) {
            qdf_info("subsystem get error ");
            subhandle = NULL;
            return -EINVAL;
        }
        else
            qdf_info("subsystem_get success");

    }
#endif //CONFIG_CNSS2_SUPPORT

    return ol_ath_soc_start(soc);
}
EXPORT_SYMBOL(ol_ath_target_start);

int
ol_ath_resume(struct ol_ath_softc_net80211 *scn)
{
    struct ieee80211com     *ic;

    ic = &scn->sc_ic;
    /*
     * ignore if already resumed.
     */
    if (OS_ATOMIC_CMPXCHG(&(scn->sc_dev_enabled), 0, 1) == 1) {
    return 0;
    }
    ieee80211_stop_running(ic);

    ieee80211_start_running(ic);

    return 0;
}

int
ol_ath_suspend(struct ol_ath_softc_net80211 *scn)
{
    /*
     * ignore if already suspended;
     */
    if (OS_ATOMIC_CMPXCHG(&(scn->sc_dev_enabled), 1, 0) == 0) {
        return 0;
    }

    ieee80211_stop_running(&scn->sc_ic);

    return 0;
}

void
ol_ath_target_status_update(ol_ath_soc_softc_t *soc, ol_target_status status)
{
    /* target lost, host needs to recover/reattach */
    soc->target_status = status;
}

void
ol_ath_suspend_resume_attach(struct ol_ath_softc_net80211 *scn)
{
}


int
ol_ath_suspend_target(ol_ath_soc_softc_t *soc, int disable_target_intr)
{
    struct pdev_op_args oper;

    oper.pointer = (void *)&disable_target_intr;
    oper.type = PDEV_ITER_SEND_SUSPEND;
    wlan_objmgr_iterate_obj_list(soc->psoc_obj, WLAN_PDEV_OP,
             wlan_pdev_operation, &oper, 0, WLAN_MLME_NB_ID);

    return 0;
}

int
ol_ath_resume_target(ol_ath_soc_softc_t *soc)
{
    struct pdev_op_args oper;

    oper.type = PDEV_ITER_SEND_RESUME;
    wlan_objmgr_iterate_obj_list(soc->psoc_obj, WLAN_PDEV_OP,
             wlan_pdev_operation, &oper, 0, WLAN_MLME_NB_ID);

    return oper.ret_val;
}
#ifndef A_MIN
#define A_MIN(a,b)    ((a)<(b)?(a):(b))
#endif

extern void target_if_spectral_send_intf_found_msg(struct wlan_objmgr_pdev *pdev,
				      u_int16_t cw_int, u_int32_t dcs_enabled);
/*
 * ol_ath_cw_interference_handler
 *
 * Functionality of this should be the same as
 * ath_net80211_cw_interference_handler() in lmac layer of the direct attach
 * drivers. Keep this same across both.
 *
 * When the cw interference is sent from the target, kick start the scan
 * with auto channel. This is disruptive channel change. Non-discruptive
 * channel change is the responsibility of scan module.
 *
 */
void
ol_ath_wlan_n_cw_interference_handler(struct ol_ath_softc_net80211 *scn,
                                      uint32_t interference_type)
{
    struct ieee80211vap *vap;
    struct ieee80211com *ic = &scn->sc_ic;

#if UMAC_SUPPORT_CBS
    int cbs_csa = ieee80211_cbs_get_param(ic->ic_cbs, IEEE80211_CBS_ENABLE);
#endif
    /* Check if CW Interference is already been found and being handled */
    if (ic->cw_inter_found)
        return;

	qdf_info("DCS: inteference_handler - start");

    spin_lock(&ic->ic_lock);

    /*
	 * mark this channel as cw_interference is found
     * Set the CW interference flag so that ACS does not bail out this flag
     * would be reset in ieee80211_beacon.c:ieee80211_beacon_update()
     */
    ic->cw_inter_found = 1;

    /* Before triggering the channel change, turn off the dcs until the
     * channel change completes, to avoid repeated reports.
     */
    (void)ol_ath_pdev_set_param(scn, wmi_pdev_param_dcs, 0);
    qdf_info("DCS channel change triggered, disabling until channel change completes\n");
    OL_ATH_DCS_CLR_RUNSTATE(scn->scn_dcs.dcs_enable);
    spin_unlock(&ic->ic_lock);

#if UMAC_SUPPORT_CBS
    if (!cbs_csa) {
#endif
    TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
        if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
            qdf_info("De-authenticating all the nodes before channel change \n");
            wlan_deauth_all_stas(vap);
        }
    }
#if UMAC_SUPPORT_CBS
    }
#endif

#if WLAN_SPECTRAL_ENABLE
    if (ic->ic_extacs_obj.icm_active) {
        spin_lock(&ic->ic_lock);

        qdf_info("ICM is active, sending message to change channel with "
               "DCS flag %d\n",
               OL_IS_DCS_ENABLED(scn->scn_dcs.dcs_enable));

        /* Currently, ol_ath_wlan_n_cw_interference_handler()
           is common to both CW AND WLAN interferences */
        if (interference_type == ATH_CAP_DCS_CWIM) {
            target_if_spectral_send_intf_found_msg(ic->ic_pdev_obj,
                                   1,
                                   OL_IS_DCS_ENABLED(scn->scn_dcs.dcs_enable));
        } else if (interference_type == ATH_CAP_DCS_WLANIM) {
            target_if_spectral_send_intf_found_msg(ic->ic_pdev_obj,
                                   0,
                                   OL_IS_DCS_ENABLED(scn->scn_dcs.dcs_enable));
        }
        spin_unlock(&ic->ic_lock);
    } else {
#endif /* WLAN_SPECTRAL_ENABLE */
        /* Loop through and figure the first VAP on this radio */
        /* FIXME
         * There could be some issue in mbssid mode. It does look like if
         * wlan_set_channel fails on first vap, it tries on the second vap
         * again. Given that all vaps on same radio, we may need not do this.
         * Need a test case for this. Leaving the code as it is.
         */
#if UMAC_SUPPORT_CBS
        if (!cbs_csa || (ieee80211_cbs_api_change_home_channel(ic->ic_cbs) != EOK)) {
#endif

        TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
            if ((vap->iv_opmode == IEEE80211_M_HOSTAP) &&
                (wlan_vdev_mlme_is_active(vap->vdev_obj) == QDF_STATUS_SUCCESS)) {
                vap->channel_switch_state = 1;
            }
        }

        TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
            if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
                if ((wlan_vdev_mlme_is_active(vap->vdev_obj) == QDF_STATUS_SUCCESS) &&
                      !wlan_set_channel(vap, IEEE80211_CHAN_ANY, 0)) {
                    /* ACS is done on per radio, so calling it once is
                    * good enough
                    */
                    goto done;
                }
            }
        }
#if UMAC_SUPPORT_CBS
        } else
            goto done;
#endif
        spin_lock(&ic->ic_lock);
        /*
         * reset cw_interference found flag since ACS is not triggered, so
         * it can change the channel on next CW intf detection
         */
        ic->cw_inter_found = 0;
        spin_unlock(&ic->ic_lock);

#if WLAN_SPECTRAL_ENABLE
    }
#endif /* WLAN_SPECTRAL_ENABLE */

#if WLAN_SPECTRAL_ENABLE
    if (!(ic->ic_extacs_obj.icm_active)) {
        qdf_info("DCS: %s ACS Trigger failed ", __func__);
    }
#else
    qdf_info("DCS: %s ACS Trigger failed ", __func__);
#endif /* WLAN_SPECTRAL_ENABLE */

    /* Should not come here (if ICM is not active), something is not right, hope
     * something better happens next time the flag is set
     */

done:
	qdf_info("DCS: interference_handling completed");
}

inline static void
wlan_dcs_im_copy_stats(wmi_host_dcs_im_tgt_stats_t *prev_stats, wmi_host_dcs_im_tgt_stats_t *curr_stats)
{
	/* right now no other actions are required beyond memcopy, if
	 * rquired the rest of the code would follow
	 */
	OS_MEMCPY(prev_stats, curr_stats, sizeof(wmi_host_dcs_im_tgt_stats_t));
}

inline static void
wlan_dcs_im_print_stats(wmi_host_dcs_im_tgt_stats_t *prev_stats, wmi_host_dcs_im_tgt_stats_t *curr_stats)
{
	/* debug, dump all received stats first */
	qdf_info("tgt_curr/tsf,%u", curr_stats->reg_tsf32);
	qdf_info(",tgt_curr/last_ack_rssi,%u", curr_stats->last_ack_rssi);
    qdf_info(",tgt_curr/tx_waste_time,%u", curr_stats->tx_waste_time);
    qdf_info(",tgt_curr/dcs_rx_time,%u", curr_stats->rx_time);
	qdf_info(",tgt_curr/listen_time,%u", curr_stats->mib_stats.listen_time);
	qdf_info(",tgt_curr/tx_frame_cnt,%u", curr_stats->mib_stats.reg_tx_frame_cnt);
	qdf_info(",tgt_curr/rx_frame_cnt,%u", curr_stats->mib_stats.reg_rx_frame_cnt);
	qdf_info(",tgt_curr/rxclr_cnt,%u", curr_stats->mib_stats.reg_rxclr_cnt);
	qdf_info(",tgt_curr/reg_cycle_cnt,%u", curr_stats->mib_stats.reg_cycle_cnt);
	qdf_info(",tgt_curr/rxclr_ext_cnt,%u", curr_stats->mib_stats.reg_rxclr_ext_cnt);
	qdf_info(",tgt_curr/ofdm_phyerr_cnt,%u", curr_stats->mib_stats.reg_ofdm_phyerr_cnt);
	qdf_info(",tgt_curr/cck_phyerr_cnt,%u", curr_stats->mib_stats.reg_cck_phyerr_cnt);

	qdf_info("tgt_prev/tsf,%u", prev_stats->reg_tsf32);
	qdf_info(",tgt_prev/last_ack_rssi,%u", prev_stats->last_ack_rssi);
    qdf_info(",tgt_prev/tx_waste_time,%u", prev_stats->tx_waste_time);
    qdf_info(",tgt_prev/rx_time,%u", prev_stats->rx_time);
	qdf_info(",tgt_prev/listen_time,%u", prev_stats->mib_stats.listen_time);
	qdf_info(",tgt_prev/tx_frame_cnt,%u", prev_stats->mib_stats.reg_tx_frame_cnt);
	qdf_info(",tgt_prev/rx_frame_cnt,%u", prev_stats->mib_stats.reg_rx_frame_cnt);
	qdf_info(",tgt_prev/rxclr_cnt,%u", prev_stats->mib_stats.reg_rxclr_cnt);
	qdf_info(",tgt_prev/reg_cycle_cnt,%u", prev_stats->mib_stats.reg_cycle_cnt);
	qdf_info(",tgt_prev/rxclr_ext_cnt,%u", prev_stats->mib_stats.reg_rxclr_ext_cnt);
	qdf_info(",tgt_prev/ofdm_phyerr_cnt,%u", prev_stats->mib_stats.reg_ofdm_phyerr_cnt);
	qdf_info(",tgt_prev/cck_phyerr_cnt,%u", prev_stats->mib_stats.reg_cck_phyerr_cnt);
}

/*
 * reg_xxx - All the variables are contents of the corresponding
 *  		register contents
 * xxx_delta - computed difference between previous cycle and c
 * 		    current cycle
 * reg_xxx_cu	- Computed channel utillization in %,
 *  		computed through register statistics
 *
 * FIXME ideally OL and non-OL layers can re-use the same code.
 * But this is done differently between OL and non-OL paths.
 * We may need to rework this completely for both to work with single
 * piece of code. The non-OL path also need lot of rework. All
 * stats need to be taken to UMAC layer. That seems to be too
 * much of work to do at this point of time. Right now host code
 * is limited to one below function, and this function alone
 * need to go UMAC. Given that function is small, tend to keep
 * only here here. If this gets any bigger we shall try doing it
 * in umac, and merge entire code ( ol and non-ol to umac ).
 */
static void
ol_ath_wlan_interference_handler(struct ol_ath_softc_net80211 * scn,
                                 wmi_host_dcs_im_tgt_stats_t *curr_stats,
                                 uint32_t interference_type)
{
	wmi_host_dcs_im_tgt_stats_t *prev_stats;

	u_int32_t reg_tsf_delta = 0;                /* prev-tsf32  - curr-tsf32 */
	u_int32_t rxclr_delta = 0;                  /* prev-RXCLR - curr-RXCLR */
	u_int32_t rxclr_ext_delta = 0;              /* prev-RXEXTCLR - curreent RXEXTCLR, most of the time this is zero, chip issue ?? */
	u_int32_t cycle_count_delta = 0;            /* prev CCYCLE - curr CCYCLE */
	u_int32_t tx_frame_delta = 0;               /* prev TFCT - curr TFCNT */
	u_int32_t rx_frame_delta = 0;               /* prev RFCNT - curr RFCNT */
	u_int32_t reg_total_cu = 0; 				/* total channel utilization in %*/
	u_int32_t reg_tx_cu = 0;					/* transmit channel utilization in %*/
    u_int32_t reg_rx_cu = 0;					/* receive channel utilization in %*/
	u_int32_t reg_unused_cu = 0;                /* unused channel utillization */
	u_int32_t rx_time_cu=0;						/* computed rx time*/
	u_int32_t reg_ofdm_phyerr_delta = 0;		/* delta ofdm errors */
	u_int32_t reg_cck_phyerr_delta = 0;			/* delta cck errors*/
	u_int32_t reg_ofdm_phyerr_cu = 0;			/* amount utilization by ofdm errors*/
	u_int32_t ofdm_phy_err_rate = 0;			/* rate at which ofdm errors are seen*/
	u_int32_t cck_phy_err_rate=0;				/* rate at which cck errors are seen*/
	u_int32_t max_phy_err_rate = 0;
    u_int32_t max_phy_err_count = 0;
	u_int32_t total_wasted_cu = 0;
	u_int32_t wasted_tx_cu = 0;
	u_int32_t tx_err = 0;
	int too_many_phy_errors = 0;

	if (!scn || !curr_stats) {
		qdf_info("\nDCS: scn is NULL\n");
		return;
	}
    prev_stats =  &scn->scn_dcs.scn_dcs_im_stats.prev_dcs_im_stats;

    if (unlikely(scn->scn_dcs.dcs_debug >= DCS_DEBUG_VERBOSE)) {
        wlan_dcs_im_print_stats(prev_stats, curr_stats);
    }

    /* counters would have wrapped. Ideally we should be able to figure this
     * out, but we never know how many times counters wrapped. just ignore
     */
	if ((curr_stats->mib_stats.listen_time <= 0) ||
        (curr_stats->reg_tsf32 <= prev_stats->reg_tsf32)) {

		if (unlikely(scn->scn_dcs.dcs_debug >= DCS_DEBUG_VERBOSE)) {
            qdf_info("\nDCS: ignoring due to negative TSF value\n");
        }
        /* copy the current stats to previous stats for next run */
		wlan_dcs_im_copy_stats(prev_stats, curr_stats);
		return;
	}

	reg_tsf_delta = curr_stats->reg_tsf32 - prev_stats->reg_tsf32;

	/* do nothing if current stats are not seeming good, probably
	 * a reset happened on chip, force cleared
	 */
	if (prev_stats->mib_stats.reg_rxclr_cnt > curr_stats->mib_stats.reg_rxclr_cnt) {
		if (unlikely(scn->scn_dcs.dcs_debug >= DCS_DEBUG_VERBOSE)) {
            qdf_info("\nDCS: ignoring due to negative rxclr count\n");
        }

        /* copy the current stats to previous stats for next run */
		wlan_dcs_im_copy_stats(prev_stats, curr_stats);
		return;
	}

	rxclr_delta = curr_stats->mib_stats.reg_rxclr_cnt - prev_stats->mib_stats.reg_rxclr_cnt;
	rxclr_ext_delta = curr_stats->mib_stats.reg_rxclr_ext_cnt -
								prev_stats->mib_stats.reg_rxclr_ext_cnt;
	tx_frame_delta = curr_stats->mib_stats.reg_tx_frame_cnt -
								prev_stats->mib_stats.reg_tx_frame_cnt;

	rx_frame_delta = curr_stats->mib_stats.reg_rx_frame_cnt -
								prev_stats->mib_stats.reg_rx_frame_cnt;

	cycle_count_delta = curr_stats->mib_stats.reg_cycle_cnt -
								prev_stats->mib_stats.reg_cycle_cnt;

    if (unlikely(scn->scn_dcs.dcs_debug >= DCS_DEBUG_VERBOSE)) {
        qdf_info(",rxclr_delta,%u,rxclr_ext_delta,%u,tx_frame_delta,%u,rx_frame_delta,%u,cycle_count_delta,%u",
                rxclr_delta , rxclr_ext_delta , tx_frame_delta,
                rx_frame_delta , cycle_count_delta );
    }

    if(0 == (cycle_count_delta >>8)) {
        wlan_dcs_im_copy_stats(prev_stats, curr_stats);
        if (unlikely(scn->scn_dcs.dcs_debug >= DCS_DEBUG_VERBOSE))
            qdf_info(" cycle count NULL---Investigate--\n");
        return;
    }

	/* total channel utiliztaion is the amount of time RXCLR is
	 * counted. RXCLR is counted, when 'RX is NOT clear', please
	 * refer to mac documentation. It means either TX or RX is ON
     *
     * Why shift by 8 ? after multiplication it could overflow. At one
     * second rate, neither cycle_count_celta, nor the tsf_delta would be
     * zero after shift by 8 bits
	 */
	reg_total_cu = ((rxclr_delta >> 8) * 100) / (cycle_count_delta >>8);
	reg_tx_cu = ((tx_frame_delta >> 8 ) * 100) / (cycle_count_delta >> 8);
	reg_rx_cu = ((rx_frame_delta >> 8 ) * 100) / (cycle_count_delta >> 8);
	rx_time_cu = ((curr_stats->rx_time >> 8) * 100 ) / (reg_tsf_delta >> 8);

    /**
     * Amount of the time AP received cannot go higher than the receive
     * cycle count delta. If at all it is, there should have been a
     * compution error, ceil it to receive_cycle_count_diff
     */
	if (rx_time_cu > reg_rx_cu) {
		rx_time_cu = reg_rx_cu;
	}

    if (unlikely(scn->scn_dcs.dcs_debug >= DCS_DEBUG_VERBOSE)) {
        qdf_info(",reg_total_cu,%u,reg_tx_cu,%u,reg_rx_cu,%u,rx_time_cu,%u",
                    reg_total_cu, reg_tx_cu, reg_rx_cu, rx_time_cu);
    }

	/* Unusable channel utilization is amount of time that we
	 * spent in backoff or waiting for other transmit/receive to
	 * complete. If there is interference it is more likely that
	 * we overshoot the limit. In case of multiple stations, we
	 * still see increased channel utilization.  This assumption may
	 * not be true for the VOW scenario where either multicast or
	 * unicast-UDP is used ( mixed traffic would still cause high
	 * channel utilization).
     */
	wasted_tx_cu = ((curr_stats->tx_waste_time >> 8) * 100 ) / (reg_tsf_delta >> 8);

    /**
     * transmit channel utilization cannot go higher than the amount of time
     * wasted, if so cap the wastage to transmit channel utillzation. This
     * could happen to compution error.
     */
	if (reg_tx_cu < wasted_tx_cu) {
		wasted_tx_cu = reg_tx_cu;
	}

	tx_err = (reg_tx_cu  && wasted_tx_cu) ? (wasted_tx_cu * 100 )/reg_tx_cu : 0;

    /**
     * The below actually gives amount of time we are not using, or the
     * interferer is active.
     * rx_time_cu is what computed receive time *NOT* rx_cycle_count
     * rx_cycle_count is our receive+interferer's transmit
     * un-used is really total_cycle_counts -
     *      (our_rx_time(rx_time_cu)+ our_receive_time)
     */
	reg_unused_cu = (reg_total_cu >= (reg_tx_cu + rx_time_cu)) ?
							(reg_total_cu - (reg_tx_cu + rx_time_cu)) : 0;

    /* if any retransmissions are there, count them as wastage
     */
	total_wasted_cu = reg_unused_cu + wasted_tx_cu;

	/* check ofdm and cck errors */
    if (unlikely(curr_stats->mib_stats.reg_ofdm_phyerr_cnt  <
            prev_stats->mib_stats.reg_ofdm_phyerr_cnt)) {
        reg_ofdm_phyerr_delta = curr_stats->mib_stats.reg_ofdm_phyerr_cnt ;
    } else {
        reg_ofdm_phyerr_delta = curr_stats->mib_stats.reg_ofdm_phyerr_cnt -
                                    prev_stats->mib_stats.reg_ofdm_phyerr_cnt;
    }

	if (unlikely(curr_stats->mib_stats.reg_cck_phyerr_cnt  <
            prev_stats->mib_stats.reg_cck_phyerr_cnt)) {
        reg_cck_phyerr_delta = curr_stats->mib_stats.reg_cck_phyerr_cnt;
    } else {
        reg_cck_phyerr_delta = curr_stats->mib_stats.reg_cck_phyerr_cnt -
                                    prev_stats->mib_stats.reg_cck_phyerr_cnt;
    }

	/* add the influence of ofdm phy errors to the wasted channel
	 * utillization, this computed through time wasted in errors,
	 */
	reg_ofdm_phyerr_cu = reg_ofdm_phyerr_delta * scn->scn_dcs.phy_err_penalty ;
	total_wasted_cu += (reg_ofdm_phyerr_cu > 0) ?  (((reg_ofdm_phyerr_cu >> 8) * 100) / (reg_tsf_delta >> 8)) : 0;

	ofdm_phy_err_rate = (curr_stats->mib_stats.reg_ofdm_phyerr_cnt * 1000) /
                                curr_stats->mib_stats.listen_time;
	cck_phy_err_rate = (curr_stats->mib_stats.reg_cck_phyerr_cnt * 1000) /
                                curr_stats->mib_stats.listen_time;

    if (unlikely(scn->scn_dcs.dcs_debug >= DCS_DEBUG_VERBOSE)) {
        qdf_info(",reg_unused_cu,%u,reg_ofdm_phyerr_delta,%u,reg_cck_phyerr_delta,%u,reg_ofdm_phyerr_cu,%u",
                    reg_unused_cu , reg_ofdm_phyerr_delta , reg_cck_phyerr_delta , reg_ofdm_phyerr_cu);
        qdf_info(",total_wasted_cu,%u,ofdm_phy_err_rate,%u,cck_phy_err_rate,%u",
                    total_wasted_cu , ofdm_phy_err_rate , cck_phy_err_rate );
        qdf_info(",new_unused_cu,%u,reg_ofdm_phy_error_cu,%u\n",
                reg_unused_cu, (curr_stats->mib_stats.reg_ofdm_phyerr_cnt*100)/
                                        curr_stats->mib_stats.listen_time);
    }

	/* check if the error rates are higher than the thresholds*/
	max_phy_err_rate = MAX(ofdm_phy_err_rate, cck_phy_err_rate);

	max_phy_err_count = MAX(curr_stats->mib_stats.reg_ofdm_phyerr_cnt,
                                curr_stats->mib_stats.reg_cck_phyerr_cnt);

    if (unlikely(scn->scn_dcs.dcs_debug >= DCS_DEBUG_VERBOSE)) {
        qdf_info(",max_phy_err_rate,%u, max_phy_err_count,%u",max_phy_err_rate , max_phy_err_count);
    }

	if (((max_phy_err_rate >= scn->scn_dcs.phy_err_threshold) &&
				(max_phy_err_count > scn->scn_dcs.phy_err_threshold)) ||
         (curr_stats->phyerr_cnt > scn->scn_dcs.radar_err_threshold)) {
		too_many_phy_errors = 1;
	}

    if (unlikely(scn->scn_dcs.dcs_debug >= DCS_DEBUG_CRITICAL)) {
		qdf_info("\nDCS: total_cu %u ,tx_cu %u ,rx_cu %u ,rx_time_cu %u, unused cu %u ",reg_total_cu, reg_tx_cu, reg_rx_cu, rx_time_cu, reg_unused_cu);
                qdf_info("\nDCS: phyerr %u ,total_wasted_cu %u ,phyerror_cu %u ,wasted_cu %u, reg_tx_cu %u ,reg_rx_cu %u",too_many_phy_errors, total_wasted_cu, reg_ofdm_phyerr_cu, wasted_tx_cu, reg_tx_cu, reg_rx_cu);
                qdf_info("\nDCS: tx_err %u",tx_err);
	}

	if (reg_unused_cu >= scn->scn_dcs.coch_intr_thresh) {
		scn->scn_dcs.scn_dcs_im_stats.im_intr_cnt+=2; /* quickly reach to decision*/
	} else if (too_many_phy_errors &&
			   (((total_wasted_cu > (scn->scn_dcs.coch_intr_thresh+10)) &&
					((reg_tx_cu + reg_rx_cu) > scn->scn_dcs.user_max_cu)) ||
                            ((reg_tx_cu > DCS_TX_MAX_CU) && (tx_err >= scn->scn_dcs.tx_err_thresh)) )){
		scn->scn_dcs.scn_dcs_im_stats.im_intr_cnt++;
	}

	if (scn->scn_dcs.scn_dcs_im_stats.im_intr_cnt >= scn->scn_dcs.intr_detection_threshold) {

        if (unlikely(scn->scn_dcs.dcs_debug >= DCS_DEBUG_CRITICAL)) {
            qdf_info("\n%s interference threshould exceeded\n", __func__);
            qdf_info(",unused_cu,%u,too_any_phy_errors,%u,total_wasted_cu,%u,reg_tx_cu,%u,reg_rx_cu,%u\n",
                    reg_unused_cu, too_many_phy_errors, total_wasted_cu,reg_tx_cu, reg_rx_cu);
        }

		scn->scn_dcs.scn_dcs_im_stats.im_intr_cnt = 0;
		scn->scn_dcs.scn_dcs_im_stats.im_samp_cnt = 0;
        /* once the interference is detected, change the channel, as on
         * today this is common routine for wirelesslan and non-wirelesslan
         * interference. Name as such kept the same because of the DA code,
         * which is using the same function.
         */
		ol_ath_wlan_n_cw_interference_handler(scn, interference_type);
	} else if (!scn->scn_dcs.scn_dcs_im_stats.im_intr_cnt ||
				scn->scn_dcs.scn_dcs_im_stats.im_samp_cnt >= scn->scn_dcs.intr_detection_window) {
		scn->scn_dcs.scn_dcs_im_stats.im_intr_cnt = 0;
		scn->scn_dcs.scn_dcs_im_stats.im_samp_cnt = 0;
	}

	/* count the current run too*/
	scn->scn_dcs.scn_dcs_im_stats.im_samp_cnt++;

    /* copy the stats for next cycle */
	wlan_dcs_im_copy_stats(prev_stats, curr_stats);

    if (unlikely(scn->scn_dcs.dcs_debug >= DCS_DEBUG_VERBOSE)) {
        qdf_info(",intr_count,%u,sample_count,%d\n",scn->scn_dcs.scn_dcs_im_stats.im_intr_cnt,scn->scn_dcs.scn_dcs_im_stats.im_samp_cnt);
    }
}

/*
 * ol_ath_dcs_interference_handler
 *
 * There are two different interference types can be reported by the
 * target firmware. Today either that is wireless interference or
 * could be a non-wireless lan interference. All of these are reported
 * WMI message.
 *
 * Message is of type wmi_dcs_interence_type_t
 *
 */
static int
ol_ath_dcs_interference_handler (ol_soc_t sc, u_int8_t *data, u_int32_t datalen)
{
    ol_ath_soc_softc_t *soc = (ol_ath_soc_softc_t *) sc;
    struct ieee80211com *ic;
    struct ol_ath_softc_net80211 *scn;
    periodic_chan_stats_t new_stats;
    periodic_chan_stats_t *prev_stats = NULL;
    wmi_host_dcs_im_tgt_stats_t wlan_stat = {0};
    struct wmi_host_dcs_interference_param dcs_param = {0};
    struct common_wmi_handle *wmi_handle;
    struct wlan_objmgr_pdev *pdev;

    wmi_handle = lmac_get_wmi_hdl(soc->psoc_obj);
    /* Extract interference type */
    if (wmi_extract_dcs_interference_type(wmi_handle, data, &dcs_param) !=
                                                           QDF_STATUS_SUCCESS) {
         qdf_info("Unable to extract dcs interference type");
         return -1;
    }

    /* Get pdev from pdev_id */
    pdev = wlan_objmgr_get_pdev_by_id(soc->psoc_obj, PDEV_UNIT(dcs_param.pdev_id),
                                   WLAN_MLME_SB_ID);
    if (pdev == NULL) {
         qdf_info("%s: pdev object (id: %d) is NULL", __func__,
                   PDEV_UNIT(dcs_param.pdev_id));
         return -1;
    }

    scn = lmac_get_pdev_feature_ptr(pdev);
    if (scn == NULL) {
         qdf_info("%s: scn (id: %d) is NULL", __func__,
                   PDEV_UNIT(dcs_param.pdev_id));
         wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
         return -1;
    }

    ic = &scn->sc_ic;

    /* This event is extended to provide periodic channel stats to user
     * space irrespective of DCS eneble or disable.
     * update periodic stats before handling DCS.
     */
    if (dcs_param.interference_type == ATH_CAP_DCS_WLANIM) {
        ol_txrx_soc_handle soc_txrx_handle;
        struct cdp_pdev *cdp_pdev;
        cdp_pdev = wlan_pdev_get_dp_handle(scn->sc_pdev);
        soc_txrx_handle = wlan_psoc_get_dp_handle(scn->soc->psoc_obj);


        /* periodic channel stats */
        if (wmi_extract_dcs_im_tgt_stats(wmi_handle, data, &wlan_stat) !=
                                         QDF_STATUS_SUCCESS) {
            qdf_info("Unable to extract WLAN IM stats");
            return -1;
        }
        new_stats.tx_frame_count = wlan_stat.mib_stats.reg_tx_frame_cnt;
        new_stats.rx_frame_count = wlan_stat.mib_stats.reg_rx_frame_cnt;
        new_stats.rx_clear_count = wlan_stat.mib_stats.reg_rxclr_cnt;
        new_stats.cycle_count = wlan_stat.mib_stats.reg_cycle_cnt;
        new_stats.my_bss_rx_cycle_count = wlan_stat.my_bss_rx_cycle_count;
        new_stats.rx_clear_ext_count = wlan_stat.mib_stats.reg_rxclr_ext_cnt;
        new_stats.rx_clear_ext40_count = wlan_stat.reg_rxclr_ext40_cnt;
        new_stats.rx_clear_ext80_count = wlan_stat.reg_rxclr_ext80_cnt;

        /* update noise floor information */
        scn->chan_nf = wlan_stat.chan_nf;
        cdp_pdev_set_chan_noise_floor(soc_txrx_handle, cdp_pdev, scn->chan_nf);

        prev_stats = &scn->scn_dcs.chan_stats;

        /* process channel stats first*/
        if (!wlan_pdev_scan_in_progress(ic->ic_pdev_obj)) {
            /* During scan our hardware and software counters keep incrementing
             * although they are tracking the stats of foreign channel.
             * Don't send periodic home channel stats while scan is in progress.
             */
            ol_chan_stats_event(ic, prev_stats, &new_stats);
            /* Update the counter vauses with latest one */
            scn->scn_dcs.chan_stats = new_stats;
        } else {
            ol_ath_invalidate_channel_stats(ic);
        }
    }

    /* do not handle any thing if host is in disabled state
     * This shall not happen, provide extra safty for against any delays
     * causing any kind of races.
     */
    if (!(OL_IS_DCS_RUNNING(scn->scn_dcs.dcs_enable))) {
        wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
        return 0;
    }
    switch (dcs_param.interference_type) {
	case ATH_CAP_DCS_CWIM: /* cw interferecne*/
		if (OL_IS_DCS_ENABLED(scn->scn_dcs.dcs_enable) & ATH_CAP_DCS_CWIM) {
			ol_ath_wlan_n_cw_interference_handler(scn,
                                                  dcs_param.interference_type);
		}
		break;
	case ATH_CAP_DCS_WLANIM: /* wlan interference stats*/
		if (OL_IS_DCS_ENABLED(scn->scn_dcs.dcs_enable) & ATH_CAP_DCS_WLANIM) {
			ol_ath_wlan_interference_handler(scn,
                                             &wlan_stat,
                                             dcs_param.interference_type);
		}
		break;
	default:
		if (unlikely(scn->scn_dcs.dcs_debug >= DCS_DEBUG_CRITICAL)) {
            qdf_info("DCS: unidentified interference type reported");
        }
	break;
    }
    wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
    return 0;
}

int
ol_ath_periodic_chan_stats_config (struct ol_ath_softc_net80211 * scn,
                                        bool enable,
                                        u_int32_t stats_period)
{
	struct periodic_chan_stats_params param;
	struct common_wmi_handle *pdev_wmi_handle;

	if (!scn || !(scn->periodic_chan_stats)) {
		return -EINVAL;
	}

	pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
	if (!pdev_wmi_handle)
		return -EINVAL;

	qdf_mem_set(&param, sizeof(param), 0);
  	param.stats_period = stats_period;
  	param.enable = enable;
	param.pdev_id = lmac_get_pdev_idx(scn->sc_pdev);
  	return wmi_unified_send_periodic_chan_stats_config_cmd(pdev_wmi_handle,&param);
}

int
ol_ath_pdev_ctl_failsafe_check_event_handler (ol_soc_t sc, u_int8_t *data, u_int32_t datalen)
{
    ol_ath_soc_softc_t *soc = (ol_ath_soc_softc_t *) sc;
    struct wmi_host_pdev_ctl_failsafe_event ctl_failsafe = {0};
    struct common_wmi_handle *wmi_handle;

    wmi_handle = lmac_get_wmi_hdl(soc->psoc_obj);
    if (!wmi_handle) {
        qdf_err("null wmi_handle");
        return -EINVAL;
    }

    if (wmi_extract_ctl_failsafe_check_ev_param(wmi_handle, data, &ctl_failsafe)) {
        return -1;
    }

    if (ctl_failsafe.ctl_failsafe_status != 0)
        QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_ERROR,
        "%s: CTL FAILSAFE Algorithm invoked when computing CTL table",__func__);

    return 0;
}

int
ol_ath_pdev_caldata_version_check_event_handler (ol_soc_t sc, u_int8_t *data, u_int32_t datalen)
{
    ol_ath_soc_softc_t *soc = (ol_ath_soc_softc_t *) sc;
    wmi_host_pdev_check_cal_version_event cal, *calInfo = &cal;
    struct common_wmi_handle *wmi_handle;

    wmi_handle = lmac_get_wmi_hdl(soc->psoc_obj);
    if (wmi_extract_pdev_caldata_version_check_ev_param(wmi_handle, data, calInfo)) {
        return -1;
    }

    qdf_info("****************** CAl DATA Version Details ********************");
    if (calInfo->cal_ok != WMI_HOST_NO_FEATURE){
        qdf_info("Current Meta Cal Version               : 0x%x ",
                                              calInfo->software_cal_version);

        qdf_info("Board (MCN : %s) Cal Version           : 0x%x ",
                       calInfo->board_mcn_detail, calInfo->board_cal_version);

        if (calInfo->cal_ok == WMI_HOST_CALIBRATION_OK){
            qdf_info("Calibration Status: OK ");
        } else { /* calInfo->cal_ok  CALIBRATION_NOT_OK */
            qdf_info("Calibration Status: NOT OK ");
        }
        qdf_info("Note: Please cross check board's MCN ");
    } else {
        qdf_info("Board was not calibrated with this feature ");
    }

    return 0;
}

int
ol_ath_pdev_tpc_config_event_handler(ol_soc_t sc, u_int8_t *data, u_int32_t datalen)
{
    ol_ath_soc_softc_t *soc = (ol_ath_soc_softc_t *) sc;
    if (soc->ol_if_ops->tpc_config_handler)  {
        soc->ol_if_ops->tpc_config_handler(sc, data, datalen);
    }

    return 0;
}

int
ol_ath_packet_power_info_get(struct ol_ath_softc_net80211 *scn,
    struct packet_power_info_params *arg)
{
    struct packet_power_info_params param;
    struct common_wmi_handle *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    qdf_mem_set(&param, sizeof(param), 0);
    param.chainmask = arg->chainmask;
    param.chan_width = arg->chan_width;
    param.rate_flags = arg->rate_flags;
    param.su_mu_ofdma = arg->su_mu_ofdma;
    param.nss = arg->nss;
    param.preamble = arg->preamble;
    param.hw_rate = arg->hw_rate;

    return wmi_unified_packet_power_info_get_cmd_send(pdev_wmi_handle, &param);
}

int
ol_gpio_config(struct ol_ath_softc_net80211 *scn, u_int32_t gpio_num, u_int32_t input,
                        u_int32_t pull_type, u_int32_t intr_mode)
{
    struct gpio_config_params param;
    struct common_wmi_handle *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    qdf_mem_set(&param, sizeof(param), 0);
    param.pull_type = pull_type;
    param.gpio_num = gpio_num;
    param.input = input;
    param.intr_mode = intr_mode;

    /* Donot send gpio command for 6290 target */
    if (lmac_get_tgt_type(scn->soc->psoc_obj) == TARGET_TYPE_QCA6290)
	    return 0;

    return wmi_unified_gpio_config_cmd_send(pdev_wmi_handle, &param);
}

int
ol_ath_gpio_output(struct ol_ath_softc_net80211 *scn, u_int32_t gpio_num, u_int32_t set)
{
    struct gpio_output_params param;
    struct common_wmi_handle *pdev_wmi_handle;

    if ((lmac_get_tgt_type(scn->soc->psoc_obj) == TARGET_TYPE_QCA8074) ||
            (lmac_get_tgt_type(scn->soc->psoc_obj) == TARGET_TYPE_QCA8074V2) ||
            (lmac_get_tgt_type(scn->soc->psoc_obj) == TARGET_TYPE_QCA6018)) {
        return -1;
    }
    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    qdf_mem_set(&param, sizeof(param), 0);
    param.gpio_num = gpio_num;
    param.set = set;

    /* Donot send gpio command for 6290 target */
    if (lmac_get_tgt_type(scn->soc->psoc_obj) == TARGET_TYPE_QCA6290)
	    return 0;

    return wmi_unified_gpio_output_cmd_send(pdev_wmi_handle, &param);

}

int
ol_ath_btcoex_duty_cycle(ol_ath_soc_softc_t *soc, u_int32_t period, u_int32_t duration)
{
    struct btcoex_cfg_params param;
    struct common_wmi_handle *wmi_handle;

    wmi_handle = lmac_get_wmi_hdl(soc->psoc_obj);
    qdf_mem_set(&param, sizeof(param), 0);
    param.period = period;
    param.wlan_duration = duration ;
    param.btcoex_param_flags = WMI_HOST_BTCOEX_PARAM_FLAGS_DUTY_CYCLE_BIT;
    return wmi_unified_send_btcoex_duty_cycle_cmd(wmi_handle, &param);
}

int
ol_ath_btcoex_wlan_priority(ol_ath_soc_softc_t *soc, u_int32_t wlan_priority)
{
    struct btcoex_cfg_params param;
    struct common_wmi_handle *wmi_handle;

    wmi_handle = lmac_get_wmi_hdl(soc->psoc_obj);
    qdf_mem_set(&param, sizeof(param), 0);
    param.btcoex_wlan_priority_bitmap = wlan_priority;
    param.btcoex_param_flags = WMI_HOST_BTCOEX_PARAM_FLAGS_WLAN_PRIORITY_BITMAP_BIT;
    return wmi_unified_send_btcoex_wlan_priority_cmd(wmi_handle, &param);
}
int
ol_ath_gpio_input_event_handler (ol_soc_t sc, u_int8_t *data, u_int32_t datalen)
{
    ol_ath_soc_softc_t *soc = (ol_ath_soc_softc_t *) sc;
    uint32_t gpio_num;
#if UMAC_SUPPORT_ACFG
    acfg_event_data_t *acfg_event = NULL;
#endif
    struct common_wmi_handle *wmi_handle;

    wmi_handle = lmac_get_wmi_hdl(soc->psoc_obj);
    if (wmi_extract_gpio_input_ev_param(wmi_handle, data, &gpio_num)) {
        return -1;
    }
    qdf_info("\n%s: GPIO Input Event on Num %d\n", __func__, gpio_num);

#if UMAC_SUPPORT_ACFG
    acfg_event = (acfg_event_data_t *)qdf_mem_malloc_atomic(sizeof(acfg_event_data_t));
    if (acfg_event == NULL)
        return 0;

    acfg_event->gpio_num = gpio_num;
    /*MULTI-RADIO TODO - is it SoC or we need pdev here?*/
    acfg_send_event(soc->sc_osdev->netdev, soc->sc_osdev, WL_EVENT_TYPE_GPIO_INPUT, acfg_event);
    qdf_mem_free(acfg_event);
#endif
    return 0;
}

#define MAX_NUM_CHAINS 8
int
ol_ath_nf_dbr_dbm_info_event_handler(ol_soc_t sc, u_int8_t *data, u_int32_t datalen)
{
    ol_ath_soc_softc_t *soc = (ol_ath_soc_softc_t *) sc;
    u_int32_t i, j;
    wmi_host_pdev_nfcal_power_all_channels_event event, *ev = &event;
#if UMAC_SUPPORT_ACFG
    struct ol_ath_softc_net80211 *scn;
    struct wlan_objmgr_pdev *pdev;
    acfg_event_data_t *acfg_event = NULL;
#endif
    struct common_wmi_handle *wmi_handle;

    wmi_handle = lmac_get_wmi_hdl(soc->psoc_obj);
    if (wmi_extract_nfcal_power_ev_param(wmi_handle, data, ev)) {
        return -1;
    }
    qdf_info("nfdBr\tnfdBm\n");
    for(j=0; j<WMI_HOST_RXG_CAL_CHAN_MAX; j++)
    {
        qdf_info("Freq=%d \n", ev->freqnum[j]);
        for(i = 0; i < WMI_HOST_MAX_NUM_CHAINS; i++) {
            qdf_info("%d\t%d\n",
                    (int8_t)ev->nfdbr[j*WMI_HOST_RXG_CAL_CHAN_MAX+i],
                    (int8_t)ev->nfdbm[j*WMI_HOST_RXG_CAL_CHAN_MAX+i]);
	}
    }
#if UMAC_SUPPORT_ACFG
    pdev = wlan_objmgr_get_pdev_by_id(soc->psoc_obj, PDEV_UNIT(ev->pdev_id),
                                   WLAN_MLME_SB_ID);
    if (pdev == NULL) {
         qdf_info("%s: pdev object (id: %d) is NULL ", __func__, PDEV_UNIT(ev->pdev_id));
         return -1;
    }

    scn = lmac_get_pdev_feature_ptr(pdev);
    if (scn == NULL) {
         qdf_info("%s: scn(id: %d) is NULL ", __func__, PDEV_UNIT(ev->pdev_id));
         wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
         return -1;
    }


    acfg_event = (acfg_event_data_t *)qdf_mem_malloc_atomic(sizeof(acfg_event_data_t));
    if (acfg_event == NULL) {
        wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
        return 0;
    }

    memcpy(acfg_event->nf_dbr_dbm.nfdbr, ev->nfdbr, sizeof((acfg_event->nf_dbr_dbm.nfdbr)));
    memcpy(acfg_event->nf_dbr_dbm.nfdbm, ev->nfdbm, sizeof(acfg_event->nf_dbr_dbm.nfdbm));
    memcpy(acfg_event->nf_dbr_dbm.freqNum, ev->freqnum, sizeof(acfg_event->nf_dbr_dbm.freqNum));
    acfg_send_event(scn->netdev, scn->sc_osdev, WL_EVENT_TYPE_NF_DBR_DBM_INFO, acfg_event);
    qdf_mem_free(acfg_event);
    wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
#endif

    return 0;
}

#undef MAX_NUM_CHAINS

int
ol_ath_packet_power_info_event_handler(ol_soc_t sc, u_int8_t *data, u_int32_t datalen)
{
    ol_ath_soc_softc_t *soc = (ol_ath_soc_softc_t *) sc;
    wmi_host_pdev_tpc_event event, *ev = &event;
#if UMAC_SUPPORT_ACFG
    struct ol_ath_softc_net80211 *scn;
    struct wlan_objmgr_pdev *pdev;
    acfg_event_data_t *acfg_event = NULL;
#endif
    struct common_wmi_handle *wmi_handle;

    wmi_handle = lmac_get_wmi_hdl(soc->psoc_obj);
    if (wmi_extract_pdev_tpc_ev_param(wmi_handle, data, ev)) {
        return -1;
    }
    qdf_info("Max packet power = %d, Min pkt power: %d\n",
            ev->tpc[WMI_HOST_TX_POWER_MAX], ev->tpc[WMI_HOST_TX_POWER_MIN]);
#if UMAC_SUPPORT_ACFG
    pdev = wlan_objmgr_get_pdev_by_id(soc->psoc_obj, PDEV_UNIT(ev->pdev_id),
                                   WLAN_MLME_SB_ID);
    if (pdev == NULL) {
         qdf_info("%s: pdev object (id: %d) is NULL ", __func__, PDEV_UNIT(ev->pdev_id));
         return -1;
    }

    scn = lmac_get_pdev_feature_ptr(pdev);
    if (scn == NULL) {
         qdf_info("%s: scn(id: %d) is NULL ", __func__, PDEV_UNIT(ev->pdev_id));
         wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
         return -1;
    }

    acfg_event = (acfg_event_data_t *)qdf_mem_malloc_atomic(sizeof(acfg_event_data_t));
    if (acfg_event == NULL) {
        wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
        return 0;
    }

    memcpy(&(acfg_event->packet_power), ev->tpc, sizeof(ev->tpc));
    acfg_send_event(scn->netdev, scn->sc_osdev, WL_EVENT_TYPE_PACKET_POWER_INFO, acfg_event);
    qdf_mem_free(acfg_event);
    wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
#endif
    return 0;
}

int
ol_ath_generic_buffer_event_handler (ol_soc_t sc, u_int8_t *data, u_int32_t datalen)
{
    ol_ath_soc_softc_t *soc = (ol_ath_soc_softc_t *) sc;
    wmi_host_pdev_generic_buffer_event event, *ev = &event;
#ifdef BIG_ENDIAN_HOST
    u_int8_t i;
#endif
    struct common_wmi_handle *wmi_handle;

    wmi_handle = lmac_get_wmi_hdl(soc->psoc_obj);
    if (wmi_extract_pdev_generic_buffer_ev_param(wmi_handle, data, ev)) {
        return -1;
    }
    qdf_info("Generic buffer received: type=%d len=%d frag_id=%d, more_frag=%d\n",
            ev->buf_type, ev->buf_len, ev->frag_id, ev->more_frag);

#ifdef BIG_ENDIAN_HOST
    {
        u_int32_t *destp, *srcp;
        u_int32_t len;

        srcp = &ev->buf_info[0];
        destp = &ev->buf_info[0];
        len = ev->buf_len;
        for(i=0; i < (roundup(len, sizeof(u_int32_t))/4); i++) {
            *destp = le32_to_cpu(*srcp);
            destp++; srcp++;
        }
    }
#endif

    if (ev->buf_type == WMI_HOST_BUFFER_TYPE_RATEPWR_TABLE) {
        ol_if_ratepwr_recv_buf((u_int8_t *)(&ev->buf_info[0]), ev->buf_len,
                                ev->frag_id, ev->more_frag);
    } else if (ev->buf_type == WMI_HOST_BUFFER_TYPE_CTL_TABLE) {
        /* TODO */
    }
    return 0;
}

int ol_ath_peer_mumimo_tx_count_event_handler(ol_soc_t sc, u_int8_t *data, u_int32_t datalen)
{
    ol_ath_soc_softc_t *soc = (ol_ath_soc_softc_t *) sc;
    wmi_host_peer_txmu_cnt_event event, *ev = &event;
    struct common_wmi_handle *wmi_handle;

    wmi_handle = lmac_get_wmi_hdl(soc->psoc_obj);
    if (wmi_extract_mumimo_tx_count_ev_param(wmi_handle, data, ev)) {
        return -1;
    }
    qdf_info("MUMIMO tx for this peer %u",ev->tx_mu_transmitted);
    return 0;
}

int ol_ath_peer_gid_userpos_list_event_handler(ol_soc_t sc, u_int8_t *data, u_int32_t datalen)
{
    ol_ath_soc_softc_t *soc = (ol_ath_soc_softc_t *) sc;
    wmi_host_peer_gid_userpos_list_event event, *ev = &event;
    struct common_wmi_handle *wmi_handle;

    wmi_handle = lmac_get_wmi_hdl(soc->psoc_obj);
    if (wmi_extract_peer_gid_userpos_list_ev_param(wmi_handle, data, ev)) {
        return -1;
    }
   qdf_info("User poistion list for GID 1->15:[%u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u]",
       ev->usr_list[0], ev->usr_list[1], ev->usr_list[2], ev->usr_list[3], ev->usr_list[4], ev->usr_list[5],
       ev->usr_list[6], ev->usr_list[7], ev->usr_list[8], ev->usr_list[9], ev->usr_list[10],ev->usr_list[11],
       ev->usr_list[12], ev->usr_list[13], ev->usr_list[14]);
   return 0;
}

u_int8_t ol_scn_vow_extstats(ol_pdev_handle pdev)
{
    struct ol_ath_softc_net80211 *scn = (struct ol_ath_softc_net80211 *) pdev;
    return scn->vow_extstats;
}
qdf_export_symbol(ol_scn_vow_extstats);

u_int32_t ol_scn_vow_get_rxstats(ol_pdev_handle pdev, u_int32_t *phy_err_count, u_int32_t *rx_clear_count, u_int32_t *rx_cycle_count)
{
    struct ol_ath_softc_net80211 *scn = (struct ol_ath_softc_net80211 *)pdev;
    *phy_err_count = scn->chan_stats.phy_err_cnt;
    *rx_clear_count = scn->mib_cycle_cnts.rx_clear_count;
    *rx_cycle_count = scn->mib_cycle_cnts.cycle_count;
    return 0;
}
qdf_export_symbol(ol_scn_vow_get_rxstats);

#if OL_ATH_SUPPORT_LED
static OS_TIMER_FUNC(ol_ath_led_poll_timed_out)
{
    struct ol_ath_softc_net80211 *scn ;
    OS_GET_TIMER_ARG(scn, struct ol_ath_softc_net80211 *);

    if (!scn || !scn->soc || !scn->soc->led_blink_rate_table) {
        return;
    }

    if (scn->scn_blinking != OL_BLINK_DONE)        /* don't interrupt active blink */
        return;

    if ((lmac_get_tgt_type(scn->soc->psoc_obj) == TARGET_TYPE_QCA8074) ||
            (lmac_get_tgt_type(scn->soc->psoc_obj) == TARGET_TYPE_QCA8074V2) ||
            (lmac_get_tgt_type(scn->soc->psoc_obj) == TARGET_TYPE_QCA6018)) {
        if( scn->scn_led_total_byte_cnt > scn->scn_led_byte_cnt)
            scn->scn_led_total_byte_cnt = 0;

        scn->scn_led_byte_cnt -= scn->scn_led_total_byte_cnt;
        scn->scn_led_total_byte_cnt += scn->scn_led_byte_cnt;

        ol_ath_led_event(scn, OL_ATH_LED_RX);
    } else {
        ol_ath_led_event(scn, OL_ATH_LED_POLL);
    }
}

static OS_TIMER_FUNC(ol_ath_led_blink_timed_out)
{
    struct ol_ath_softc_net80211 *scn ;
    uint32_t target_type;

    OS_GET_TIMER_ARG(scn, struct ol_ath_softc_net80211 *);

    if (!scn->soc || !scn->soc->led_blink_rate_table ||
            (lmac_get_tgt_type(scn->soc->psoc_obj) == TARGET_TYPE_QCA8074) ||
            (lmac_get_tgt_type(scn->soc->psoc_obj) == TARGET_TYPE_QCA8074V2) ||
            (lmac_get_tgt_type(scn->soc->psoc_obj) == TARGET_TYPE_QCA6018)) {
        return;
    }

    target_type =  lmac_get_tgt_type(scn->soc->psoc_obj);
    switch (scn->scn_blinking) {
    case OL_BLINK_ON_START:
            scn->scn_blinking = OL_BLINK_DONE;
#if OL_ATH_SUPPORT_LED_POLL
            OS_SET_TIMER(&scn->scn_led_poll_timer, LED_POLL_TIMER);
#endif
        break;
    case OL_BLINK_OFF_START:
	    if ((target_type == TARGET_TYPE_QCA8074) ||
                (target_type == TARGET_TYPE_QCA8074V2) ||
                (target_type == TARGET_TYPE_QCA6018)) {
            /* Skip as current gpio configuration is not available. */
	    } else if(target_type == TARGET_TYPE_IPQ4019) {
                ipq4019_wifi_led(scn, OL_LED_OFF);
            } else {
                ol_ath_gpio_output(scn, scn->scn_led_gpio, 0);
            }
            scn->scn_blinking = OL_BLINK_ON_START;
            OS_SET_TIMER(&scn->scn_led_blink_timer, scn->scn_led_time_on);
        break;
    case OL_BLINK_STOP:
        if ((target_type == TARGET_TYPE_QCA8074) ||
                (target_type == TARGET_TYPE_QCA8074V2) ||
                (target_type == TARGET_TYPE_QCA6018)) {
            /* Skip as current gpio configuration is not available. */
	    } else if(target_type == TARGET_TYPE_IPQ4019) {
                ipq4019_wifi_led(scn, OL_LED_ON);
            } else {
                ol_ath_gpio_output(scn, scn->scn_led_gpio, 1);
            }
            scn->scn_blinking = OL_BLINK_DONE;
        break;
    case OL_BLINK_DONE:
    default:
        break;
    }
}

/*
 * Blink the LED according to the specified on/off times.
 */
static void
ol_ath_led_blink(struct ol_ath_softc_net80211 *scn, u_int32_t on, u_int32_t off)
{
#define HAWKEYE_2G_LED 3
#define HAWKEYE_5G_LED 4
    uint32_t target_type;
    struct wlan_psoc_host_hal_reg_capabilities_ext *reg_cap;
    uint8_t pdev_idx;

    target_type =  lmac_get_tgt_type(scn->soc->psoc_obj);
    pdev_idx = lmac_get_pdev_idx(scn->sc_pdev);
    reg_cap = ucfg_reg_get_hal_reg_cap(scn->soc->psoc_obj);
    if (!reg_cap)
        return;

    if ((target_type == TARGET_TYPE_QCA8074) ||
            (target_type == TARGET_TYPE_QCA8074V2) ||
            (target_type == TARGET_TYPE_QCA6018)) {
#ifdef QCA_SUPPORT_CP_STATS
        struct ieee80211com *ic = &scn->sc_ic;
        if(!pdev_cp_stats_ap_stats_tx_cal_enable_get(ic->ic_pdev_obj))
           return;
#endif

#if ATH_SUPPORT_LED_CONTROLLER
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
        if(reg_cap[pdev_idx].wireless_modes & WIRELESS_MODES_2G) {
            ipq_led_set_blink(HAWKEYE_2G_LED, on, off);
        } else  {
            ipq_led_set_blink(HAWKEYE_5G_LED, on, off);
        }
#endif
#endif
    } else if(target_type == TARGET_TYPE_IPQ4019) {
        ipq4019_wifi_led(scn, OL_LED_ON);
    } else {
        ol_ath_gpio_output(scn, scn->scn_led_gpio, 1);
    }
    scn->scn_led_time_on = on;
    if ((target_type == TARGET_TYPE_QCA8074) ||
            (target_type == TARGET_TYPE_QCA8074V2) ||
            (target_type == TARGET_TYPE_QCA6018)) {
#if OL_ATH_SUPPORT_LED_POLL
        scn->scn_blinking = OL_BLINK_DONE;
        OS_SET_TIMER(&scn->scn_led_poll_timer, LED_POLL_TIMER);
#endif
    } else {
        scn->scn_blinking = OL_BLINK_OFF_START;
        OS_SET_TIMER(&scn->scn_led_blink_timer, off);
    }
}

void
ol_ath_led_event(struct ol_ath_softc_net80211 *scn, OL_LED_EVENT event)
{
    u_int32_t led_last_time = CONVERT_SYSTEM_TIME_TO_MS(OS_GET_TIMESTAMP());
    u_int32_t map_idx;

    if (!scn || !(scn->soc->led_blink_rate_table)) {
        return;
    }

#if QCA_LTEU_SUPPORT
    if (wlan_psoc_nif_feat_cap_get(scn->soc->psoc_obj, WLAN_SOC_F_LTEU_SUPPORT))
        return;
#endif

    if (scn->scn_blinking != OL_BLINK_DONE)        /* don't interrupt active blink */
        return;
    switch (event) {
    case OL_ATH_LED_TX:
    case OL_ATH_LED_RX:
            /* 1/6554 = 1000 (ms -> sec) * 8 (Byte -> Bits) / 1024 *1024 ( -> Mega) * 50 (50 Mbps per entry) */
            map_idx = scn->scn_led_byte_cnt / ((led_last_time + 1 - scn->scn_led_last_time) * 6554);
            scn->scn_led_last_time = led_last_time;
            scn->scn_led_byte_cnt = 0;
            if (map_idx < 0) {
                map_idx = 0;
            } else if (map_idx > scn->scn_led_max_blink_rate_idx) {
                map_idx = scn->scn_led_max_blink_rate_idx;
            }
            ol_ath_led_blink(scn, scn->soc->led_blink_rate_table[map_idx].timeOn,
			     scn->soc->led_blink_rate_table[map_idx].timeOff);
        break;
    case OL_ATH_LED_POLL:
            ol_ath_led_blink(scn, 100, 500);
        break;
    default:
        break;
    }
}
qdf_export_symbol(ol_ath_led_event);
#endif /* OL_ATH_SUPPORT_LED */

uint32_t
ol_get_phymode_info(struct ol_ath_softc_net80211 *scn, uint32_t chan_mode,
                    bool is_2gvht_en) {
    /* We define a non-const modeflags array in order to enable stubbing based
     * simulation. Such simulation would involve an override of the host mode to
     * FW mode mapping.
     */
    static uint32_t modeflags[] = {
        0,                            /* IEEE80211_MODE_AUTO            */
        WMI_HOST_MODE_11A,            /* IEEE80211_MODE_11A             */
        WMI_HOST_MODE_11B,            /* IEEE80211_MODE_11B             */
        WMI_HOST_MODE_11G,            /* IEEE80211_MODE_11G             */
        0,                            /* IEEE80211_MODE_FH              */
        0,                            /* IEEE80211_MODE_TURBO_A         */
        0,                            /* IEEE80211_MODE_TURBO_G         */
        WMI_HOST_MODE_11NA_HT20,      /* IEEE80211_MODE_11NA_HT20       */
        WMI_HOST_MODE_11NG_HT20,      /* IEEE80211_MODE_11NG_HT20       */
        WMI_HOST_MODE_11NA_HT40,      /* IEEE80211_MODE_11NA_HT40PLUS   */
        WMI_HOST_MODE_11NA_HT40,      /* IEEE80211_MODE_11NA_HT40MINUS  */
        WMI_HOST_MODE_11NG_HT40,      /* IEEE80211_MODE_11NG_HT40PLUS   */
        WMI_HOST_MODE_11NG_HT40,      /* IEEE80211_MODE_11NG_HT40MINUS  */
        WMI_HOST_MODE_11NG_HT40,      /* IEEE80211_MODE_11NG_HT40       */
        WMI_HOST_MODE_11NA_HT40,      /* IEEE80211_MODE_11NA_HT40       */
        WMI_HOST_MODE_11AC_VHT20,     /* IEEE80211_MODE_11AC_VHT20      */
        WMI_HOST_MODE_11AC_VHT40,     /* IEEE80211_MODE_11AC_VHT40PLUS  */
        WMI_HOST_MODE_11AC_VHT40,     /* IEEE80211_MODE_11AC_VHT40MINUS */
        WMI_HOST_MODE_11AC_VHT40,     /* IEEE80211_MODE_11AC_VHT40      */
        WMI_HOST_MODE_11AC_VHT80,     /* IEEE80211_MODE_11AC_VHT80      */
        WMI_HOST_MODE_11AC_VHT160,    /* IEEE80211_MODE_11AC_VHT160     */
        WMI_HOST_MODE_11AC_VHT80_80,  /* IEEE80211_MODE_11AC_VHT80_80   */
        WMI_HOST_MODE_11AX_HE20,      /* IEEE80211_MODE_11AXA_HE20      */
        WMI_HOST_MODE_11AX_HE20_2G,   /* IEEE80211_MODE_11AXG_HE20      */
        WMI_HOST_MODE_11AX_HE40,      /* IEEE80211_MODE_11AXA_HE40PLUS  */
        WMI_HOST_MODE_11AX_HE40,      /* IEEE80211_MODE_11AXA_HE40MINUS */
        WMI_HOST_MODE_11AX_HE40_2G,   /* IEEE80211_MODE_11AXG_HE40PLUS  */
        WMI_HOST_MODE_11AX_HE40_2G,   /* IEEE80211_MODE_11AXG_HE40MINUS */
        WMI_HOST_MODE_11AX_HE40,      /* IEEE80211_MODE_11AXA_HE40      */
        WMI_HOST_MODE_11AX_HE40_2G,   /* IEEE80211_MODE_11AXG_HE40      */
        WMI_HOST_MODE_11AX_HE80,      /* IEEE80211_MODE_11AXA_HE80      */
        WMI_HOST_MODE_11AX_HE160,     /* IEEE80211_MODE_11AXA_HE160     */
        WMI_HOST_MODE_11AX_HE80_80,   /* IEEE80211_MODE_11AXA_HE80_80   */
    };
    uint32_t temp_phymode;
    enum wmi_target_type wmi_tgt_type;

    qdf_assert_always(NULL != scn);

#if QCA_11AX_STUB_SUPPORT
    if (1 == OL_ATH_IS_11AX_STUB_ENABLED(scn->soc)) {
        /* We re-map 802.11ax modes to equivalent 802.11n/ac modes. */
        modeflags[IEEE80211_MODE_11AXA_HE20] = WMI_HOST_MODE_11AC_VHT20;
        modeflags[IEEE80211_MODE_11AXG_HE20] = WMI_HOST_MODE_11NG_HT20;
        modeflags[IEEE80211_MODE_11AXA_HE40PLUS] = WMI_HOST_MODE_11AC_VHT40;
        modeflags[IEEE80211_MODE_11AXA_HE40MINUS] = WMI_HOST_MODE_11AC_VHT40;
        modeflags[IEEE80211_MODE_11AXG_HE40PLUS] = WMI_HOST_MODE_11NG_HT40;
        modeflags[IEEE80211_MODE_11AXG_HE40MINUS] = WMI_HOST_MODE_11NG_HT40;
        modeflags[IEEE80211_MODE_11AXA_HE40] = WMI_HOST_MODE_11AC_VHT40;
        modeflags[IEEE80211_MODE_11AXG_HE40] = WMI_HOST_MODE_11NG_HT40;
        modeflags[IEEE80211_MODE_11AXA_HE80] = WMI_HOST_MODE_11AC_VHT80;
        modeflags[IEEE80211_MODE_11AXA_HE160] = WMI_HOST_MODE_11AC_VHT160;
        modeflags[IEEE80211_MODE_11AXA_HE80_80] = WMI_HOST_MODE_11AC_VHT80_80;
    }
#endif /* QCA_11AX_STUB_SUPPORT */

    /* Assert if the chan mode is invalid */
    qdf_assert_always(chan_mode < IEEE80211_MODE_MAX);

    temp_phymode = modeflags[chan_mode];
    if (ol_ath_get_wmi_target_type(scn->soc, &wmi_tgt_type) != QDF_STATUS_SUCCESS) {
        qdf_info("Not able to get wmi target type");
        return temp_phymode;
    }

    if ((wmi_tgt_type == WMI_TLV_TARGET) && (is_2gvht_en == true)) {
        switch(chan_mode) {
            case IEEE80211_MODE_11NG_HT20:
                temp_phymode = WMI_HOST_MODE_11AC_VHT20_2G;
                break;

            case IEEE80211_MODE_11NG_HT40PLUS:
            case IEEE80211_MODE_11NG_HT40MINUS:
            case IEEE80211_MODE_11NG_HT40:
                temp_phymode = WMI_HOST_MODE_11AC_VHT40_2G;
                break;
        }
    }
    return temp_phymode;
}

uint32_t
ol_get_phymode_info_from_wlan_phymode(struct ol_ath_softc_net80211 *scn,
        uint32_t chan_mode,
        bool is_2gvht_en) {

    static uint32_t conv_phy_mode_to_wmi_phy_mode[] = {
        WMI_HOST_MODE_UNKNOWN,        /* WLAN_PHYMODE_AUTO            */
        WMI_HOST_MODE_11A,            /* WLAN_PHYMODE_11A             */
        WMI_HOST_MODE_11B,            /* WLAN_PHYMODE_11B             */
        WMI_HOST_MODE_11G,            /* WLAN_PHYMODE_11G             */
        WMI_HOST_MODE_11NA_HT20,      /* WLAN_PHYMODE_11NA_HT20       */
        WMI_HOST_MODE_11NG_HT20,      /* WLAN_PHYMODE_11NG_HT20       */
        WMI_HOST_MODE_11NA_HT40,      /* WLAN_PHYMODE_11NA_HT40PLUS   */
        WMI_HOST_MODE_11NA_HT40,      /* WLAN_PHYMODE_11NA_HT40MINUS  */
        WMI_HOST_MODE_11NG_HT40,      /* WLAN_PHYMODE_11NG_HT40PLUS   */
        WMI_HOST_MODE_11NG_HT40,      /* WLAN_PHYMODE_11NG_HT40MINUS  */
        WMI_HOST_MODE_11NG_HT40,      /* WLAN_PHYMODE_11NG_HT40       */
        WMI_HOST_MODE_11NA_HT40,      /* WLAN_PHYMODE_11NA_HT40       */
        WMI_HOST_MODE_11AC_VHT20,     /* WLAN_PHYMODE_11AC_VHT20      */
        WMI_HOST_MODE_11AC_VHT40,     /* WLAN_PHYMODE_11AC_VHT40PLUS  */
        WMI_HOST_MODE_11AC_VHT40,     /* WLAN_PHYMODE_11AC_VHT40MINUS */
        WMI_HOST_MODE_11AC_VHT40,     /* WLAN_PHYMODE_11AC_VHT40      */
        WMI_HOST_MODE_11AC_VHT80,     /* WLAN_PHYMODE_11AC_VHT80      */
        WMI_HOST_MODE_11AC_VHT160,    /* WLAN_PHYMODE_11AC_VHT160     */
        WMI_HOST_MODE_11AC_VHT80_80,  /* WLAN_PHYMODE_11AC_VHT80_80   */
        WMI_HOST_MODE_11AX_HE20,      /* WLAN_PHYMODE_11AXA_HE20      */
        WMI_HOST_MODE_11AX_HE20_2G,   /* WLAN_PHYMODE_11AXG_HE20      */
        WMI_HOST_MODE_11AX_HE40,      /* WLAN_PHYMODE_11AXA_HE40PLUS  */
        WMI_HOST_MODE_11AX_HE40,      /* WLAN_PHYMODE_11AXA_HE40MINUS */
        WMI_HOST_MODE_11AX_HE40_2G,   /* WLAN_PHYMODE_11AXG_HE40PLUS  */
        WMI_HOST_MODE_11AX_HE40_2G,   /* WLAN_PHYMODE_11AXG_HE40MINUS */
        WMI_HOST_MODE_11AX_HE40,      /* WLAN_PHYMODE_11AXA_HE40      */
        WMI_HOST_MODE_11AX_HE40_2G,   /* WLAN_PHYMODE_11AXG_HE40      */
        WMI_HOST_MODE_11AX_HE80,      /* WLAN_PHYMODE_11AXA_HE80      */
        WMI_HOST_MODE_11AX_HE160,     /* WLAN_PHYMODE_11AXA_HE160     */
        WMI_HOST_MODE_11AX_HE80_80,   /* WLAN_PHYMODE_11AXA_HE80_80   */
    };
    uint32_t temp_phymode;
    enum wmi_target_type wmi_tgt_type;

    qdf_assert_always(NULL != scn);

#if QCA_11AX_STUB_SUPPORT
    if (1 == OL_ATH_IS_11AX_STUB_ENABLED(scn->soc)) {
        /* We re-map 802.11ax modes to equivalent 802.11n/ac modes. */
        conv_phy_mode_to_wmi_phy_mode[WLAN_PHYMODE_11AXA_HE20] = WMI_HOST_MODE_11AC_VHT20;
        conv_phy_mode_to_wmi_phy_mode[WLAN_PHYMODE_11AXG_HE20] = WMI_HOST_MODE_11NG_HT20;
        conv_phy_mode_to_wmi_phy_mode[WLAN_PHYMODE_11AXA_HE40PLUS] = WMI_HOST_MODE_11AC_VHT40;
        conv_phy_mode_to_wmi_phy_mode[WLAN_PHYMODE_11AXA_HE40MINUS] = WMI_HOST_MODE_11AC_VHT40;
        conv_phy_mode_to_wmi_phy_mode[WLAN_PHYMODE_11AXG_HE40PLUS] = WMI_HOST_MODE_11NG_HT40;
        conv_phy_mode_to_wmi_phy_mode[WLAN_PHYMODE_11AXG_HE40MINUS] = WMI_HOST_MODE_11NG_HT40;
        conv_phy_mode_to_wmi_phy_mode[WLAN_PHYMODE_11AXA_HE40] = WMI_HOST_MODE_11AC_VHT40;
        conv_phy_mode_to_wmi_phy_mode[WLAN_PHYMODE_11AXG_HE40] = WMI_HOST_MODE_11NG_HT40;
        conv_phy_mode_to_wmi_phy_mode[WLAN_PHYMODE_11AXA_HE80] = WMI_HOST_MODE_11AC_VHT80;
        conv_phy_mode_to_wmi_phy_mode[WLAN_PHYMODE_11AXA_HE160] = WMI_HOST_MODE_11AC_VHT160;
        conv_phy_mode_to_wmi_phy_mode[WLAN_PHYMODE_11AXA_HE80_80] = WMI_HOST_MODE_11AC_VHT80_80;
    }
#endif /* QCA_11AX_STUB_SUPPORT */

    if(chan_mode < WLAN_PHYMODE_11A || chan_mode > WLAN_PHYMODE_11AXA_HE80_80) {
        temp_phymode = WMI_HOST_MODE_UNKNOWN;
        return temp_phymode;
    }

    if (ol_ath_get_wmi_target_type(scn->soc, &wmi_tgt_type) != QDF_STATUS_SUCCESS) {
        qdf_print("Not able to get wmi target type");
        temp_phymode = WMI_HOST_MODE_UNKNOWN;
        return temp_phymode;
    }
    temp_phymode = conv_phy_mode_to_wmi_phy_mode[chan_mode];

    if ((wmi_tgt_type == WMI_TLV_TARGET) && (is_2gvht_en == true)) {
        switch(chan_mode) {
            case WLAN_PHYMODE_11NG_HT20:
                temp_phymode = WMI_HOST_MODE_11AC_VHT20_2G;
                break;

            case WLAN_PHYMODE_11NG_HT40PLUS:
            case WLAN_PHYMODE_11NG_HT40MINUS:
            case WLAN_PHYMODE_11NG_HT40:
                temp_phymode = WMI_HOST_MODE_11AC_VHT40_2G;
                break;
        }
    }
    return temp_phymode;
}

#define _HT_SGI_PRESENT 0x80
/**
 * convert_tx_to_rx_stats(): Function to update mpdu info
 * from ppdu_desc
 * @ppdu_id: ppdu_id
 * @mpdu_info: cdp_tx_indication_mpdu_info
 * @user: cdp_tx_completion_ppdu_user
 *
 * return: void
 */
static inline
QDF_STATUS convert_tx_to_rx_stats(struct cdp_tx_indication_mpdu_info *m_info,
                                  struct mon_rx_status *rx_status)
{
    rx_status->tsft = m_info->tx_duration;
    rx_status->chan_freq = m_info->channel;
    rx_status->ppdu_id = m_info->ppdu_id;
    rx_status->rssi_comb = m_info->ack_rssi;

    rx_status->tid = m_info->tid;
    rx_status->frame_control_info_valid = m_info->frame_ctrl;
    rx_status->nss = m_info->nss;
    rx_status->sgi = m_info->gi;
    rx_status->mcs = m_info->mcs;
    rx_status->preamble_type = m_info->preamble;
    rx_status->rate = m_info->tx_rate;
    rx_status->bw = m_info->bw;
    rx_status->beamformed = m_info->txbf;
    rx_status->ldpc = m_info->ldpc;

    switch(rx_status->preamble_type) {
    case DOT11_AC:
        rx_status->vht_flags = 1;
    break;
    case DOT11_AX:
        rx_status->he_flags = 1;
    break;
    default:
        rx_status->ht_flags = 1;
        rx_status->rtap_flags |= _HT_SGI_PRESENT;
    break;
    };

    return QDF_STATUS_SUCCESS;
}

/* size of struct rx_pkt_tlv */
#define RX_PADDING_SIZE 384
/**
 * ol_ath_process_tx_frames() - Callback registered for WDI_EVENT_TX_DATA
 * @pdev_hdl: pdev pointer
 * @event:   WDi event
 * @data:    skb received
 * @peer_id: peer_id
 * @status:  status from event
 *
 * The MSDU payload is appended with a tx capture identifier containing ppdu_id,
 * tx and rx address and is sent to this function.
 *
 * @Return: None
 */
static void ol_ath_process_tx_frames(void *pdev_hdl, enum WDI_EVENT event, void *data,
                                u_int16_t peer_id, uint32_t status)
{
    qdf_nbuf_t skb = NULL;
    struct wlan_objmgr_pdev *pdev_obj = (struct wlan_objmgr_pdev *) pdev_hdl;
    struct ieee80211com *ic = wlan_pdev_get_mlme_ext_obj(pdev_obj);
    struct ieee80211vap *vap;
    osif_dev  *osifp;
    struct cdp_tx_indication_info *ptr_tx_info;
    struct mon_rx_status rx_status = {0};

    ptr_tx_info = (struct cdp_tx_indication_info *)data;

    if (!ptr_tx_info->mpdu_nbuf)
        return;

    if (!ic) {
        qdf_nbuf_free(ptr_tx_info->mpdu_nbuf);
        qdf_debug("ic is NULL");
        return;
    }

    vap = (struct ieee80211vap *)ic->ic_mon_vap;
    if (!vap) {
        qdf_nbuf_free(ptr_tx_info->mpdu_nbuf);
	ptr_tx_info->mpdu_nbuf = NULL;
        QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_DEBUG,"%s: No monitor vap created, to dump skb\n",__func__);
        return;
    }

    /**
     * We are currently forwarding the buffer to user space or freeing the
     * buffer based on the feature.
     * Have callback here to extract the data if required.
     */
    if (!vap->iv_lite_monitor && (ic->ic_debug_sniffer == SNIFFER_M_COPY_MODE)){
        qdf_nbuf_free(ptr_tx_info->mpdu_nbuf);
	ptr_tx_info->mpdu_nbuf = NULL;
        return;
    }

    osifp = (osif_dev *)vap->iv_ifp;

    QDF_TRACE(QDF_MODULE_ID_TX_CAPTURE, QDF_TRACE_LEVEL_INFO,
        "%s: %d ppdu_id[%d] frm_type[%d] [%p]sending to stack!!!!\n",
        __func__, __LINE__,
        ptr_tx_info->mpdu_info.ppdu_id, ptr_tx_info->mpdu_info.frame_type,
        ptr_tx_info->mpdu_nbuf);

    skb = ptr_tx_info->mpdu_nbuf;

    if (ptr_tx_info->frame_payload) {
        /*
         * for mgmtctrl payload tlv we won't have ppdu_info
         * so deliver the packet
         */
        osif_deliver_tx_capture_data(osifp, skb);
        ptr_tx_info->mpdu_nbuf = NULL;
        return;
    }

    if (!ptr_tx_info->mpdu_nbuf) {
        QDF_TRACE(QDF_MODULE_ID_TX_CAPTURE, QDF_TRACE_LEVEL_ERROR,
            "mpdu_nbuf is NULL!!!!\n");
        return;
    }

    ptr_tx_info->mpdu_nbuf = NULL;
    /* update radiotap header */
    convert_tx_to_rx_stats(&ptr_tx_info->mpdu_info, &rx_status);
    qdf_nbuf_update_radiotap(&rx_status, skb, RX_PADDING_SIZE);

    osif_deliver_tx_capture_data(osifp, skb);

    return;
}


/**
 * ol_ath_process_tx_data_frames() - Callback registered for WDI_EVENT_TX_DATA
 * @pdev_hdl: pdev pointer
 * @event:   WDi event
 * @data:    skb received
 * @peer_id: peer_id
 * @status:  status from event
 *
 * The MSDU payload is appended with a tx capture identifier containing ppdu_id,
 * tx and rx address and is sent to this function.
 *
 * @Return: None
 */
static void ol_ath_process_tx_data_frames(void *pdev_hdl, enum WDI_EVENT event, void *data,
                                u_int16_t peer_id, uint32_t status)
{
    qdf_nbuf_t skb = (qdf_nbuf_t)data;
    struct wlan_objmgr_pdev *pdev_obj = (struct wlan_objmgr_pdev *) pdev_hdl;
    struct ieee80211com *ic = wlan_pdev_get_mlme_ext_obj(pdev_obj);
    struct ieee80211vap *vap;
    osif_dev  *osifp;
    char *buf = NULL;

    if (!ic) {
      qdf_nbuf_free(skb);
      qdf_debug("ic is NULL");
      return;
    }

    vap = (struct ieee80211vap *)ic->ic_mon_vap;
    if (!vap) {
      qdf_nbuf_free(skb);
      QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_DEBUG,"%s: No monitor vap created, to dump skb\n",__func__);
      return;
    }

    /**
     * We are currently forwarding the buffer to user space or freeing the
     * buffer based on the feature.
     * Have callback here to extract the data if required.
     */
    if (!vap->iv_lite_monitor && (ic->ic_debug_sniffer == SNIFFER_M_COPY_MODE)){
            qdf_nbuf_free(skb);
            return;
    }

    osifp = (osif_dev *)vap->iv_ifp;

    if ((buf = (char *)qdf_nbuf_put_tail(skb, DEBUG_SNIFFER_SIGNATURE_LEN))) {
        qdf_mem_copy(buf, DEBUG_SNIFFER_TEST_TX_DATA, DEBUG_SNIFFER_SIGNATURE_LEN);
    }

    /* Ensure to free the skb here if not being sent to upper layer */
    osif_deliver_tx_capture_data(osifp, skb);
}

/**
 * ol_ath_process_rx_data_frames() - Callback registered for WDI_EVENT_RX_DATA
 * @pdev_hdl: pdev pointer
 * @event:   WDi event
 * @data:    skb received
 * @peer_id: peer_id
 * @status:  status from event
 *
 * The MSDU payload is appended with a rx capture identifier containing 16bits ppdu_id,
 * and reserved field and is sent to this function.
 *
 * @Return: None
 */
static void ol_ath_process_rx_data_frames(void *pdev_hdl, enum WDI_EVENT event, void *data,
                                u_int16_t peer_id, uint32_t status)
{
    qdf_nbuf_t skb = (qdf_nbuf_t)data;
    struct wlan_objmgr_pdev *pdev_obj = (struct wlan_objmgr_pdev *) pdev_hdl;
    struct ieee80211com *ic = wlan_pdev_get_mlme_ext_obj(pdev_obj);
    struct ieee80211vap *vap;
    osif_dev  *osifp;
    char *buf = NULL;

    if (!ic) {
      qdf_nbuf_free(skb);
      qdf_debug("ic is NULL");
      return;
    }

    vap = (struct ieee80211vap *)ic->ic_mon_vap;

    if (!vap) {
      qdf_nbuf_free(skb);
      QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_DEBUG,"%s: No monitor vap created, to dump skb\n",__func__);
      return;
    }

    /**
     * We are currently forwarding the buffer to user space or freeing the
     * buffer based on the feature.
     * Have callback here to extract the data if required.
     */
    if (!vap->iv_lite_monitor && (ic->ic_debug_sniffer == SNIFFER_M_COPY_MODE)){
            qdf_nbuf_free(skb);
            return;
    }

    osifp = (osif_dev *)vap->iv_ifp;

    if ((buf = (char *)qdf_nbuf_put_tail(skb, DEBUG_SNIFFER_SIGNATURE_LEN))) {
        qdf_mem_copy(buf, DEBUG_SNIFFER_TEST_RX_DATA, DEBUG_SNIFFER_SIGNATURE_LEN);
    }

    /* Ensure to free the skb here if not being sent to upper layer */
    osif_deliver_tx_capture_data(osifp, skb);
}

#ifdef WLAN_RX_PKT_CAPTURE_ENH
/**
 * This is the length for radiotap, combined length
 * (Mandatory part struct ieee80211_radiotap_header + RADIOTAP_HEADER_LEN)
 * cannot be more than available headroom_sz.
 * increase this when we add more radiotap elements.
 * Number after '+' indicates maximum possible increase due to alignment
 */

#define RADIOTAP_VHT_FLAGS_LEN (12 + 1)
#define RADIOTAP_HE_FLAGS_LEN (12 + 1)
#define RADIOTAP_HE_MU_FLAGS_LEN (8 + 1)
#define RADIOTAP_HE_MU_OTHER_FLAGS_LEN (18 + 1)
#define RADIOTAP_FIXED_HEADER_LEN 17
#define RADIOTAP_HT_FLAGS_LEN 3
#define RADIOTAP_AMPDU_STATUS_LEN (8 + 3)
#define RADIOTAP_VENDOR_NS_LEN \
    (sizeof(struct qdf_radiotap_vendor_ns_ath) + 1)
#define RADIOTAP_HEADER_LEN (sizeof(struct ieee80211_radiotap_header) + \
                RADIOTAP_FIXED_HEADER_LEN + \
                RADIOTAP_HT_FLAGS_LEN + \
                RADIOTAP_VHT_FLAGS_LEN + \
                RADIOTAP_AMPDU_STATUS_LEN + \
                RADIOTAP_HE_FLAGS_LEN + \
                RADIOTAP_HE_MU_FLAGS_LEN + \
                RADIOTAP_HE_MU_OTHER_FLAGS_LEN + \
                RADIOTAP_VENDOR_NS_LEN)

#define IEEE80211_RADIOTAP_HE 23
#define IEEE80211_RADIOTAP_HE_MU	24
#define IEEE80211_RADIOTAP_HE_MU_OTHER	25

QDF_STATUS convert_mpdu_info_to_stats(struct cdp_rx_indication_mpdu_info *mpdu_info, struct mon_rx_status *rx_status)
{
    A_MEMZERO(rx_status,sizeof(struct mon_rx_status));
    rx_status->tsft = mpdu_info->timestamp;
    rx_status->rs_fcs_err = mpdu_info->fcs_err;
    rx_status->chan_num = mpdu_info->channel;
    rx_status->ppdu_id = mpdu_info->ppdu_id;
    rx_status->rssi_comb = mpdu_info->rssi_comb;
    rx_status->chan_noise_floor = mpdu_info->nf;

    rx_status->rate = mpdu_info->rate;
    rx_status->nss = mpdu_info->nss;
    rx_status->sgi = mpdu_info->gi;
    rx_status->mcs = mpdu_info->mcs;
    rx_status->preamble_type = mpdu_info->preamble;
    rx_status->bw = mpdu_info->bw;
    rx_status->ldpc = mpdu_info->ldpc;

	/*testing pupose*/
    if (mpdu_info->preamble == DOT11_N ||
        mpdu_info->preamble == DOT11_AC ||
        mpdu_info->preamble == DOT11_AX) {
        rx_status->ht_flags = 1;
        rx_status->ht_mcs = rx_status->mcs;
        rx_status->rtap_flags |= 0x80;
    }
    rx_status->l_sig_a_info = mpdu_info->preamble |
        rx_status->ppdu_id << 16;
    rx_status->l_sig_b_info = mpdu_info->ppdu_type;
    rx_status->device_id = rx_status->nss |
	mpdu_info->ofdma_info_valid << 8 |
	mpdu_info->ofdma_ru_start_index << 16 |
	mpdu_info->ofdma_ru_width << 24;
    return QDF_STATUS_SUCCESS;
}

static inline void ol_if_process_rx_mpdu(struct ol_ath_softc_net80211 *scn, enum WDI_EVENT event,
        void *data, u_int16_t peer_id)
{
    struct ieee80211com *ic  = &scn->sc_ic;
    struct ieee80211vap *vap = NULL;
    osif_dev  *osifp         = NULL;
    struct cdp_rx_indication_mpdu *rx_ind_mpdu = (struct cdp_rx_indication_mpdu *)data;
    qdf_nbuf_t mpdu_ind;
    struct cdp_rx_indication_mpdu_info *mpdu_info = &rx_ind_mpdu->mpdu_info;
    struct mon_rx_status rx_status;

    mpdu_ind = rx_ind_mpdu->nbuf;

    if (!mpdu_ind)
        return;

    if (!ic) {
      qdf_nbuf_free(mpdu_ind);
      qdf_info("%s: ic is NULL", __func__);
      return;
    }

    vap = (struct ieee80211vap *)ic->ic_mon_vap;

    if (!vap) {
      qdf_nbuf_free(mpdu_ind);
      return;
    }

    convert_mpdu_info_to_stats(mpdu_info, &rx_status);
    qdf_nbuf_update_radiotap(&rx_status, mpdu_ind, RADIOTAP_HEADER_LEN);

    osifp = (osif_dev *)vap->iv_ifp;

    skb_reset_mac_header(mpdu_ind);
    mpdu_ind->dev       = osifp->netdev;
    mpdu_ind->pkt_type  = PACKET_USER;
    mpdu_ind->ip_summed = CHECKSUM_UNNECESSARY;
    mpdu_ind->protocol  = qdf_cpu_to_le16(ETH_P_802_2);
    netif_rx(mpdu_ind);
}

void process_rx_mpdu(void *pdev, enum WDI_EVENT event, void *data, u_int16_t peer_id, enum htt_cmn_rx_status status)
{
    struct ol_ath_softc_net80211 *scn = (struct ol_ath_softc_net80211 *)pdev;

    ol_if_process_rx_mpdu(scn, event, data, peer_id);
}
#endif

/**
 * ol_ath_process_tx_metadata() - Callback registered for WDI_EVENT_TX_PPDU_DESC
 * @pdev_hdl: pdev pointer
 * @event:   WDi event
 * @data:    skb received
 * @peer_id: peer_id
 * @status:  status from event
 *
 * @Return: None
 */
void ol_ath_process_tx_metadata(struct ieee80211com *ic, void *data)
{
    qdf_nbuf_t skb = (qdf_nbuf_t)data;
    struct ieee80211vap *vap = (struct ieee80211vap *)ic->ic_mon_vap;
    osif_dev  *osifp = NULL;
    char *buf = NULL;

    if (!vap) {
      qdf_nbuf_free(skb);
      QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_DEBUG,"%s: No monitor vap created, to dump skb\n",__func__);
      return;
    }

    /**
     * We are currently forwarding the buffer to user space or freeing the
     * buffer based on the feature.
     * Have callback here to extract the data if required.
     */
    if (!vap->iv_lite_monitor && (ic->ic_debug_sniffer == SNIFFER_M_COPY_MODE)){
            qdf_nbuf_free(skb);
            return;
    }

    osifp = (osif_dev *)vap->iv_ifp;

    if ((buf = (char *)qdf_nbuf_put_tail(skb, DEBUG_SNIFFER_SIGNATURE_LEN))) {
        qdf_mem_copy(buf, DEBUG_SNIFFER_TEST_TX_METADATA, DEBUG_SNIFFER_SIGNATURE_LEN);
    }

    /* Ensure to free the skb here if not being sent to upper layer */
    osif_deliver_tx_capture_data(osifp, skb);
}

/**
 * ol_ath_process_rx_metadata() - Callback registered for WDI_EVENT_RX_PPDU_DESC
 * @pdev_hdl: pdev pointer
 * @event:   WDi event
 * @data:    skb received
 * @peer_id: peer_id
 * @status:  status from event
 *
 * @Return: None
 */
void ol_ath_process_rx_metadata(struct ieee80211com *ic, void *data)
{
    qdf_nbuf_t skb = (qdf_nbuf_t)data;
    struct ieee80211vap *vap = (struct ieee80211vap *)ic->ic_mon_vap;
    osif_dev  *osifp = NULL;
    char *buf = NULL;

    if (!vap) {
      qdf_nbuf_free(skb);
      QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_DEBUG,"%s: No monitor vap created, to dump skb\n",__func__);
      return;
    }

    /**
     * We are currently forwarding the buffer to user space or freeing the
     * buffer based on the feature.
     * Have callback here to extract the data if required.
     */
    if (!vap->iv_lite_monitor && (ic->ic_debug_sniffer == SNIFFER_M_COPY_MODE)){
            qdf_nbuf_free(skb);
            return;
    }

    osifp = (osif_dev *)vap->iv_ifp;

    if ((buf = (char *)qdf_nbuf_put_tail(skb, DEBUG_SNIFFER_SIGNATURE_LEN))) {
        qdf_mem_copy(buf, DEBUG_SNIFFER_TEST_RX_METADATA, DEBUG_SNIFFER_SIGNATURE_LEN);
    }

    /* Ensure to free the skb here if not being sent to upper layer */
    osif_deliver_tx_capture_data(osifp, skb);
}

/**
 * ol_ath_process_rx_nondata_frames() - Callback registered for recvd. mgmt. pkt processing
 * @pdev_hdl: pdev pointer
 * @event:   WDI event
 * @data:    skb received
 * @peer_id: peer_id
 * @status:  status from event
 *
 * @Return: None
 */
static void ol_ath_process_rx_nondata_frames(void *pdev_hdl, enum WDI_EVENT event, void *data,
                                u_int16_t peer_id, uint32_t status)
{
    qdf_nbuf_t skb = (qdf_nbuf_t)data;
    struct wlan_objmgr_pdev *pdev_obj = (struct wlan_objmgr_pdev *) pdev_hdl;
    struct ieee80211com *ic = wlan_pdev_get_mlme_ext_obj(pdev_obj);
    struct ieee80211vap *vap;
    osif_dev  *osifp = NULL;
    char *buf = NULL;

    if (!ic) {
      qdf_nbuf_free(skb);
      qdf_debug("ic is NULL");
      return;
    }

    vap = (struct ieee80211vap *)ic->ic_mon_vap;

    if (!vap) {
      qdf_nbuf_free(skb);
      QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_DEBUG,"%s: No monitor vap created, to dump skb\n",__func__);
      return;
    }

    /**
     * We are currently forwarding the buffer to user space or freeing the
     * buffer based on the feature.
     * Have callback here to extract the data if required.
     */
    if (qdf_nbuf_is_nonlinear(skb) || (!vap->iv_lite_monitor && (ic->ic_debug_sniffer == SNIFFER_M_COPY_MODE))){
            qdf_nbuf_free(skb);
            return;
    }

    osifp = (osif_dev *)vap->iv_ifp;

    if ((buf = (char *)qdf_nbuf_put_tail(skb, DEBUG_SNIFFER_SIGNATURE_LEN))) {
        qdf_mem_copy(buf, DEBUG_SNIFFER_TEST_RX_MGMT, DEBUG_SNIFFER_SIGNATURE_LEN);
    }

    /* Ensure to free the skb here if not being sent to upper layer */
    osif_deliver_tx_capture_data(osifp, skb);
}

#if QCN_IE
/**
 * ol_ath_process_offload_beacon() - Callback registered for WDI_EVENT_TX_BEACON
 * @pdev_hdl: pdev pointer
 * @event:   WDI event
 * @data:    skb received
 * @peer_id: peer_id
 * @status:  status from event
 *
 * @Return: None
 */
static void ol_ath_process_offload_beacon(void *pdev_hdl, enum WDI_EVENT event, void *data,
                               u_int16_t peer_id, uint32_t status)
{
    qdf_nbuf_t skb = (qdf_nbuf_t)data;
    struct wlan_objmgr_pdev *pdev_obj = (struct wlan_objmgr_pdev *) pdev_hdl;
    struct ieee80211com *ic = wlan_pdev_get_mlme_ext_obj(pdev_obj);
    struct ieee80211_node *ni;
    struct ieee80211vap *vap = NULL;
    qdf_hrtimer_data_t *bpr_hrtimer = NULL;
    struct ieee80211_frame *hdr;
    uint8_t *frm = NULL;

    if (!ic) {
      qdf_nbuf_free(skb);
      qdf_debug(" ic is NULL");
      return;
    }
    frm = (uint8_t *)qdf_nbuf_data(skb);
    /* Add IEEE80211_MACHDR_OFFSET to frm to point to ieee80211 beacon flags*/
    frm = frm + IEEE80211_MACHDR_OFFSET;
    hdr = (struct ieee80211_frame *)frm;
    if ((hdr->i_fc[0] & (IEEE80211_FC0_TYPE_MASK | IEEE80211_FC0_SUBTYPE_MASK)) == (IEEE80211_FC0_TYPE_MGT | IEEE80211_FC0_SUBTYPE_BEACON)) {
        ni = ieee80211_find_node(&ic->ic_sta, hdr->i_addr2);
        if (ni) {
            vap = ni->ni_vap;
            if (vap == NULL) {
                ieee80211_free_node(ni);
                if (ic->ic_debug_sniffer == SNIFFER_DISABLE &&
                    !ic->ic_tx_pkt_capture) {
                    qdf_nbuf_free(skb);
                }
                return;
            }
            if (ni == vap->iv_bss) {
                bpr_hrtimer = &vap->bpr_timer;
                if (qdf_hrtimer_active(bpr_hrtimer)) {

                    /* Cancel the timer as beacon is sent instead of a broadcast response */
                    qdf_hrtimer_cancel(bpr_hrtimer);
                    vap->iv_bpr_timer_cancel_count++;

                    IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME,
                    "Cancel timer: %s| %d | Delay: %d | Current time: %lld | Next beacon tstamp: %lld | "
                    "beacon interval: %d ms | Timer cb: %d | Enqueued: %d\n", \
                    __func__, __LINE__, vap->iv_bpr_delay, qdf_ktime_to_ns(qdf_ktime_get()), qdf_ktime_to_ns(vap->iv_next_beacon_tstamp), \
                    ic->ic_intval, qdf_hrtimer_callback_running(bpr_hrtimer), qdf_hrtimer_is_queued(bpr_hrtimer));
                }

                /* Calculate the next beacon timestamp */
                vap->iv_next_beacon_tstamp = qdf_ktime_add_ns(qdf_ktime_get(), ic->ic_intval * QDF_NSEC_PER_MSEC);
            }
            /**
             * We are currently freeing the buffer based on vap->iv_lite_monitor flag.
             * In monitor mode, the buffer will be handeled by ol_ath_process_tx_nondata_frames.
             */
            if (ic->ic_debug_sniffer == SNIFFER_DISABLE &&
                !ic->ic_tx_pkt_capture) {
                qdf_nbuf_free(skb);
            }
            ieee80211_free_node(ni);
        }
    }

}
#endif

/**
 * ol_ath_process_tx_nondata_frames() - Callback registered for WDI_EVENT_TX_MGMT_CTRL
 * @pdev_hdl: pdev pointer
 * @event:   WDI event
 * @data:    skb received
 * @peer_id: peer_id
 * @status:  status from event
 *
 * @Return: None
 */
static void ol_ath_process_tx_nondata_frames(void *pdev_hdl, enum WDI_EVENT event, void *data,
                                u_int16_t peer_id, uint32_t status)
{
    qdf_nbuf_t skb = (qdf_nbuf_t)data;
    struct wlan_objmgr_pdev *pdev_obj = (struct wlan_objmgr_pdev *) pdev_hdl;
    struct ieee80211com *ic = wlan_pdev_get_mlme_ext_obj(pdev_obj);
    struct ieee80211vap *vap;
    osif_dev  *osifp = NULL;
    char *buf = NULL;

    if (!ic) {
      qdf_nbuf_free(skb);
      qdf_debug("ic is NULL");
      return;
    }

    vap = (struct ieee80211vap *)ic->ic_mon_vap;
    if (!vap) {
      qdf_nbuf_free(skb);
      QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_DEBUG,"%s: No monitor vap created, to dump skb\n",__func__);
      return;
    }

    /**
     * We are currently forwarding the buffer to user space or freeing the
     * buffer based on the feature.
     * Have callback here to extract the data if required.
     */
    if (!vap->iv_lite_monitor && (ic->ic_debug_sniffer == SNIFFER_M_COPY_MODE)){
            qdf_nbuf_free(skb);
            return;
    }
    osifp = (osif_dev *)vap->iv_ifp;

    if ((buf = (char *)qdf_nbuf_put_tail(skb, DEBUG_SNIFFER_SIGNATURE_LEN))) {
        qdf_mem_copy(buf, DEBUG_SNIFFER_TEST_TX_MGMT, DEBUG_SNIFFER_SIGNATURE_LEN);
    }

    /* Ensure to free the skb here if not being sent to upper layer */
    osif_deliver_tx_capture_data(osifp, skb);
}

static void ol_ath_process_tx_nondata_frames_wrapper(struct ieee80211com *ic, qdf_nbuf_t payload)
{
    struct wlan_objmgr_pdev *pdev_obj = (struct wlan_objmgr_pdev *) ic->ic_pdev_obj;
    ol_ath_process_tx_nondata_frames(pdev_obj, 0, (void *)payload, 0, 0);
}

void ol_ath_subscribe_ppdu_desc_info(struct ol_ath_softc_net80211 *scn, uint8_t context)
{
    ol_txrx_pdev_handle pdev_txrx_handle = wlan_pdev_get_dp_handle(scn->sc_pdev);
    ol_txrx_soc_handle soc_txrx_handle = wlan_psoc_get_dp_handle(scn->soc->psoc_obj);
    struct ieee80211com *ic = &scn->sc_ic;

    if ((context == PPDU_DESC_DEBUG_SNIFFER) ||
            ((context == PPDU_DESC_ENHANCED_STATS))) {
        if (qdf_atomic_read(&scn->tx_metadata_ref) == 0) {
            scn->stats_tx_subscriber.callback = ol_ath_process_ppdu_stats;
            scn->stats_tx_subscriber.context = scn->sc_pdev;
            if (cdp_wdi_event_sub(soc_txrx_handle, (struct cdp_pdev *)pdev_txrx_handle,
                    &scn->stats_tx_subscriber, WDI_EVENT_TX_PPDU_DESC)) {
                return;
            }
        }
        qdf_atomic_inc(&scn->tx_metadata_ref);
    }

    if ((context == PPDU_DESC_ENHANCED_STATS) || (ic->ic_debug_sniffer == SNIFFER_M_COPY_MODE)) {
        if (qdf_atomic_read(&scn->rx_metadata_ref) == 0) {
            scn->stats_rx_subscriber.callback = ol_ath_process_ppdu_stats;
            scn->stats_rx_subscriber.context = scn->sc_pdev;
            if (cdp_wdi_event_sub(soc_txrx_handle, (struct cdp_pdev *)pdev_txrx_handle,
                    &scn->stats_rx_subscriber, WDI_EVENT_RX_PPDU_DESC)) {
                return;
            }
        }
        qdf_atomic_inc(&scn->rx_metadata_ref);
    }
}

void ol_ath_unsubscribe_ppdu_desc_info(struct ol_ath_softc_net80211 *scn, uint8_t context)
{
    struct ieee80211com *ic = &scn->sc_ic;
    ol_txrx_pdev_handle pdev_txrx_handle = wlan_pdev_get_dp_handle(scn->sc_pdev);
    ol_txrx_soc_handle soc_txrx_handle = wlan_psoc_get_dp_handle(scn->soc->psoc_obj);

    if ((context == PPDU_DESC_DEBUG_SNIFFER) ||
         ((context == PPDU_DESC_ENHANCED_STATS) )) {
        if (qdf_atomic_dec_and_test(&scn->tx_metadata_ref)) {
            if (cdp_wdi_event_unsub(soc_txrx_handle, (struct cdp_pdev *)pdev_txrx_handle,
                                &scn->stats_tx_subscriber, WDI_EVENT_TX_PPDU_DESC))
                return;
        }
    }

    if ((context == PPDU_DESC_ENHANCED_STATS) || (ic->ic_debug_sniffer == SNIFFER_M_COPY_MODE)) {
        if (qdf_atomic_dec_and_test(&scn->rx_metadata_ref)) {
                if (cdp_wdi_event_unsub(soc_txrx_handle,
                        (struct cdp_pdev *)pdev_txrx_handle,
                        &scn->stats_rx_subscriber,
                        WDI_EVENT_RX_PPDU_DESC))
                    return;
        }
    }
}

int ol_ath_set_capture_latency(struct ol_ath_softc_net80211 *scn, int val)
{
    struct ieee80211com *ic = &scn->sc_ic;
    ol_txrx_pdev_handle pdev_txrx_handle;
    ol_txrx_soc_handle soc_txrx_handle;

    soc_txrx_handle = wlan_psoc_get_dp_handle(scn->soc->psoc_obj);
    pdev_txrx_handle = wlan_pdev_get_dp_handle(scn->sc_pdev);
    if (val) {
        if (!ic->ic_capture_latency)
            ic->ic_capture_latency = val;
        else {
            qdf_info("Capture_latency already enabled. Disable it first");
            return A_ERROR;
        }
        cdp_txrx_set_pdev_param(soc_txrx_handle,
				(struct cdp_pdev *)pdev_txrx_handle,
				CDP_CONFIG_CAPTURE_LATENCY, val);
    } else {
        if (ic->ic_capture_latency)
            ic->ic_capture_latency = val;
        else {
            qdf_info("Capture_latency already disabled. Enable it first");
            return A_ERROR;
        }

        cdp_txrx_set_pdev_param(soc_txrx_handle,
				(struct cdp_pdev *)pdev_txrx_handle,
				CDP_CONFIG_CAPTURE_LATENCY, val);
    }
    return 0;
}

#if QCN_IE
int ol_ath_set_bpr_wifi3(struct ol_ath_softc_net80211 *scn, int val)
{
    struct ieee80211com *ic = &scn->sc_ic;
    ol_txrx_pdev_handle pdev_txrx_handle;
    ol_txrx_soc_handle soc_txrx_handle;

    if (ol_ath_is_beacon_offload_enabled(scn->soc)) {
        soc_txrx_handle = wlan_psoc_get_dp_handle(scn->soc->psoc_obj);
        pdev_txrx_handle = wlan_pdev_get_dp_handle(scn->sc_pdev);
        if (val) {
            if (qdf_atomic_inc_return(&ic->ic_bpr_enable) == 1) {
                scn->stats_bpr_subscriber.callback = ol_ath_process_offload_beacon;
                scn->stats_bpr_subscriber.context = scn->sc_pdev;
                if (cdp_wdi_event_sub(soc_txrx_handle,
                                (struct cdp_pdev *)pdev_txrx_handle,
                                &scn->stats_bpr_subscriber,
                                WDI_EVENT_TX_BEACON))
                    return A_ERROR;
                cdp_txrx_set_pdev_param(soc_txrx_handle,
                                (struct cdp_pdev *)pdev_txrx_handle,
                                CDP_CONFIG_BPR_ENABLE, val);
            }
        }
        else {
            if (qdf_atomic_dec_return(&ic->ic_bpr_enable) == 0) {
                if (cdp_wdi_event_unsub(soc_txrx_handle,
                                (struct cdp_pdev *)pdev_txrx_handle,
                                &scn->stats_bpr_subscriber,
                                WDI_EVENT_TX_BEACON))
                    return A_ERROR;
                cdp_txrx_set_pdev_param(soc_txrx_handle,
                                (struct cdp_pdev *)pdev_txrx_handle,
                                CDP_CONFIG_BPR_ENABLE, val);
            }
        }
    }
    return 0;
}
#endif

int ol_ath_set_debug_sniffer(struct ol_ath_softc_net80211 *scn,  int val)
{
    ol_txrx_soc_handle soc_txrx_handle;
    ol_txrx_pdev_handle pdev_txrx_handle;
    struct ieee80211com *ic = &scn->sc_ic;

    soc_txrx_handle = wlan_psoc_get_dp_handle(scn->soc->psoc_obj);
    pdev_txrx_handle = wlan_pdev_get_dp_handle(scn->sc_pdev);

    if ((val == SNIFFER_TX_CAPTURE_MODE) || (val == SNIFFER_M_COPY_MODE)) {
	if (!ic->ic_debug_sniffer) {
            ic->ic_debug_sniffer = val;
	} else {
            qdf_info("Sniffer/AM copy mode already enabled. Disable it first");
            return A_ERROR;
	}

        ic->ic_process_nondata_tx_frames = ol_ath_process_tx_nondata_frames_wrapper;

        if (val == SNIFFER_M_COPY_MODE) {
            scn->stats_rx_data_subscriber.callback = ol_ath_process_rx_data_frames;
            scn->stats_rx_data_subscriber.context = scn->sc_pdev;

             if (cdp_wdi_event_sub(soc_txrx_handle,
                        (struct cdp_pdev *)pdev_txrx_handle,
                        &scn->stats_rx_data_subscriber,
                        WDI_EVENT_RX_DATA))
                return A_ERROR;

            scn->stats_rx_nondata_subscriber.callback = ol_ath_process_rx_nondata_frames;
            scn->stats_rx_nondata_subscriber.context = scn->sc_pdev;
             if (cdp_wdi_event_sub(soc_txrx_handle,
                        (struct cdp_pdev *)pdev_txrx_handle,
                        &scn->stats_rx_nondata_subscriber,
                        WDI_EVENT_RX_MGMT_CTRL))
                return A_ERROR;
	}

        scn->stats_tx_data_subscriber.callback = ol_ath_process_tx_data_frames;
        scn->stats_tx_data_subscriber.context = scn->sc_pdev;
        scn->stats_nondata_subscriber.callback = ol_ath_process_tx_nondata_frames;
        scn->stats_nondata_subscriber.context = scn->sc_pdev;

        if (cdp_wdi_event_sub(soc_txrx_handle,
                        (struct cdp_pdev *)pdev_txrx_handle,
                        &scn->stats_tx_data_subscriber,
                        WDI_EVENT_TX_DATA))
            return A_ERROR;



        if (cdp_wdi_event_sub(soc_txrx_handle,
                        (struct cdp_pdev *)pdev_txrx_handle,
                        &scn->stats_nondata_subscriber,
                        WDI_EVENT_TX_MGMT_CTRL))
            return A_ERROR;

        ol_ath_subscribe_ppdu_desc_info(scn, PPDU_DESC_DEBUG_SNIFFER);

        cdp_txrx_set_pdev_param(soc_txrx_handle,
			(struct cdp_pdev *)pdev_txrx_handle,
			CDP_CONFIG_DEBUG_SNIFFER, val);

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
            if (scn->sc_ic.nss_radio_ops)
                scn->sc_ic.nss_radio_ops->ic_nss_ol_wifi_tx_capture_set(scn, val);
#endif
    } else if (val == SNIFFER_TX_MONITOR_MODE) {
        scn->stats_tx_data_subscriber.callback = ol_ath_process_tx_frames;
        scn->stats_tx_data_subscriber.context = scn->sc_pdev;

        if (cdp_wdi_event_sub(soc_txrx_handle,
                        (struct cdp_pdev *)pdev_txrx_handle,
                        &scn->stats_tx_data_subscriber,
                        WDI_EVENT_TX_DATA))
            return A_ERROR;

    } else {
        if (cdp_wdi_event_unsub(soc_txrx_handle,
                        (struct cdp_pdev *)pdev_txrx_handle,
                        &scn->stats_tx_data_subscriber,
                        WDI_EVENT_TX_DATA))
            return A_ERROR;

        if (cdp_wdi_event_unsub(soc_txrx_handle,
                        (struct cdp_pdev *)pdev_txrx_handle,
                        &scn->stats_rx_data_subscriber,
                        WDI_EVENT_RX_DATA))
            return A_ERROR;

        if (cdp_wdi_event_unsub(soc_txrx_handle,
                        (struct cdp_pdev *)pdev_txrx_handle,
                        &scn->stats_nondata_subscriber,
                        WDI_EVENT_TX_MGMT_CTRL))
            return A_ERROR;

        if (cdp_wdi_event_unsub(soc_txrx_handle,
                        (struct cdp_pdev *)pdev_txrx_handle,
                        &scn->stats_rx_nondata_subscriber,
                        WDI_EVENT_RX_MGMT_CTRL))
            return A_ERROR;

        ol_ath_unsubscribe_ppdu_desc_info(scn, PPDU_DESC_DEBUG_SNIFFER);

        ic->ic_debug_sniffer = 0;
        ic->ic_process_nondata_tx_frames = NULL;

        cdp_txrx_set_pdev_param(soc_txrx_handle,
				(struct cdp_pdev *)pdev_txrx_handle,
				CDP_CONFIG_DEBUG_SNIFFER, SNIFFER_DISABLE);

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
            if (scn->sc_ic.nss_radio_ops)
                scn->sc_ic.nss_radio_ops->ic_nss_ol_wifi_tx_capture_set(scn, SNIFFER_DISABLE);
#endif
	}
    return 0;
}

int ol_ath_set_tx_capture (struct ol_ath_softc_net80211 *scn, int val) {
#ifndef REMOVE_PKT_LOG
    u_int32_t tx_capture;
	ol_txrx_pdev_handle pdev_txrx_handle = wlan_pdev_get_dp_handle(scn->sc_pdev);
	ol_txrx_soc_handle soc_txrx_handle = wlan_psoc_get_dp_handle(scn->soc->psoc_obj);

    tx_capture = scn->pl_dev->pl_info->tx_capture_enabled;

#ifdef OL_ATH_SMART_LOGGING
    /* We use scn->pl_dev->tgt_pktlog_enabled inline with existing design for
     * this call. However, if access semantics for
     * scn->pl_dev->tgt_pktlog_enabled are changed for this call, then change
     * below accordingly.
     */
    if ((val == 1) && (!tx_capture)) {
        /* We intend to enable Tx capture. But if
         * scn->pl_dev->tgt_pktlog_enabled is true due to smart log FW initiated
         * packetlog, we have to get it out of the way.
         */
        if (scn->pl_dev->tgt_pktlog_enabled) {
            /* If smart log FW initiated packetlog is responsible for
             * scn->pl_dev->tgt_pktlog_enabled being true, then it will stop the
             * packetlog and block itself resulting in
             * scn->pl_dev->tgt_pktlog_enabled becoming false. These call
             * semantics are necessary for preserving locking requirements
             * within the smart log FW initiated packetlog module.
             *
             * We set the block_only_if_started argument to true so that if
             * scn->pl_dev->tgt_pktlog_enabled is true but smart log FW
             * initiated packetlog is not responsible, then the previous host
             * module caller's decision to not block FW initiated packetlog
             * remains in effect.
             */
            if (scn->pl_dev->pl_sl_funcs.smart_log_fw_pktlog_stop_and_block &&
                   (scn->pl_dev->pl_sl_funcs.smart_log_fw_pktlog_stop_and_block(
                        scn, ATH_PKTLOG_TX_CAPTURE_ENABLE, true) !=
                     QDF_STATUS_SUCCESS)) {
                return -1;
            }
        } else {
             /* Since we are the first to block smart log FW initiated
              * packetlog, we block it under all circumstances.
              */
             if (scn->pl_dev->pl_sl_funcs.smart_log_fw_pktlog_stop_and_block &&
                   (scn->pl_dev->pl_sl_funcs.smart_log_fw_pktlog_stop_and_block(
                        scn, ATH_PKTLOG_TX_CAPTURE_ENABLE, false) !=
                     QDF_STATUS_SUCCESS)) {
                return -1;
            }
        }
    }
#endif /* OL_ATH_SMART_LOGGING */

    if ((val == 1) && ((!scn->pl_dev->tgt_pktlog_enabled) && (!tx_capture))) {
           if (scn->pl_dev->pl_funcs->pktlog_enable(scn, ATH_PKTLOG_TX_CAPTURE_ENABLE) == 0) {
               qdf_info("\nEnabled Tx Capture");
            scn->pl_dev->pl_info->tx_capture_enabled = 1;
	    	cdp_set_pdev_tx_capture(soc_txrx_handle,
			    		(void *) pdev_txrx_handle, val);
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
            if (scn->sc_ic.nss_radio_ops)
                scn->sc_ic.nss_radio_ops->ic_nss_ol_wifi_tx_capture_set(scn, val);
#endif
            return 0;
        }
    } else if ((val == 0) && (tx_capture)) {
        if (scn->pl_dev->pl_funcs->pktlog_enable(scn, 0) == 0) {
            qdf_info("\nDisabled Tx Capture");
            scn->pl_dev->pl_info->tx_capture_enabled = 0;
	    cdp_set_pdev_tx_capture(soc_txrx_handle,
			    (void *) pdev_txrx_handle, val);
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
            if (scn->sc_ic.nss_radio_ops)
                scn->sc_ic.nss_radio_ops->ic_nss_ol_wifi_tx_capture_set(scn, val);
#endif
#ifdef OL_ATH_SMART_LOGGING
            if (scn->pl_dev->pl_sl_funcs.smart_log_fw_pktlog_unblock)
                scn->pl_dev->pl_sl_funcs.smart_log_fw_pktlog_unblock(scn);
#endif /* OL_ATH_SMART_LOGGING */
            return 0;
        }
    }
#endif
    return -1;
}

/*
 * Return true if target is a lithium target. else return false
 */
bool ol_target_lithium(struct wlan_objmgr_psoc *psoc)
{
    switch (lmac_get_tgt_type(psoc)) {
        case TARGET_TYPE_QCA8074:
        case TARGET_TYPE_QCA8074V2:
        case TARGET_TYPE_QCA6018:
        case TARGET_TYPE_QCA6290:
            return true;
    }

    return false;
}
qdf_export_symbol(ol_target_lithium);

QDF_STATUS
ol_scan_set_chan_list(struct wlan_objmgr_pdev *pdev, void *arg)
{
    struct ieee80211com *ic = NULL;
    struct ol_ath_softc_net80211 *scn = NULL;
    struct scan_chan_list_params *param = arg;
    struct common_wmi_handle *pdev_wmi_handle;

    ic = wlan_pdev_get_mlme_ext_obj(pdev);
    if (!ic) {
      qdf_debug(" ic is NULL");
      return QDF_STATUS_E_FAILURE;
    }

    scn = OL_ATH_SOFTC_NET80211(ic);
    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    wmi_unified_scan_chan_list_cmd_send(pdev_wmi_handle, param);

    return QDF_STATUS_SUCCESS;
}

#ifdef CONFIG_DP_TRACE
static int
ol_ath_dptrace_setparam(int cmd, int val1, int val2)
{
    switch (cmd) {
        case IEEE80211_DBG_DPTRACE_PROTO_BITMAP:
            qdf_dp_set_proto_bitmap(val1);
            return 0;
        case IEEE80211_DBG_DPTRACE_VERBOSITY:
            qdf_dp_trace_set_verbosity(val1);
            return 0;
        case IEEE80211_DBG_DPTRACE_NO_OF_RECORD:
            qdf_dp_set_no_of_record(val1);
            return 0;
        case IEEE80211_DBG_DPTRACE_DUMPALL:
            qdf_dp_trace_dump_all(val1, val2);
            return 0;
        case IEEE80211_DBG_DPTRACE_CLEAR:
            qdf_dp_trace_clear_buffer();
            return 0;
        case IEEE80211_DBG_DPTRACE_LIVEMODE_ON:
            qdf_dp_trace_enable_live_mode();
            return 0;
        case IEEE80211_DBG_DPTRACE_LIVEMODE_OFF:
            qdf_dp_trace_disable_live_mode();
            return 0;
        default:
            return -1;
    }
}
#endif


inline u_int32_t ol_if_peer_get_rate(struct wlan_objmgr_peer *peer , u_int8_t type)
{
    struct wlan_objmgr_psoc *psoc;
    struct cdp_peer_stats *peer_stats;
    struct cdp_peer *peer_dp_handle;

    if (!peer)
        return -ENOENT;
    psoc = wlan_peer_get_psoc(peer);
    if (!psoc)
        return -ENOENT;

    peer_dp_handle = wlan_peer_get_dp_handle(peer);
    if (!peer_dp_handle)
        return -ENOENT;

    peer_stats = cdp_host_get_peer_stats(wlan_psoc_get_dp_handle(psoc),
                                         peer_dp_handle);
    if (!peer_stats)
        return -ENOENT;

    switch (type) {
        case IEEE80211_RATE_TX:
             return peer_stats->tx.tx_rate;
        case IEEE80211_RATE_RX:
             return peer_stats->rx.rx_rate;
        case IEEE80211_RATECODE_TX:
             return peer_stats->tx.tx_ratecode;
        case IEEE80211_RATEFLAGS_TX:
             return peer_stats->tx.tx_flags;
        default:
             return 0;
    }
}

QDF_STATUS ol_ath_fill_umac_legacy_chanlist(struct wlan_objmgr_pdev *pdev,
        struct regulatory_channel *curr_chan_list)
{
    struct ol_ath_softc_net80211 *scn;
    struct pdev_osif_priv *osif_priv;
    struct ieee80211com *ic;
    struct common_wmi_handle *wmi_handle;

    osif_priv = wlan_pdev_get_ospriv(pdev);

    if (osif_priv == NULL) {
        qdf_info("%s : osif_priv is NULL", __func__);
        return QDF_STATUS_E_FAILURE;
    }

    scn = (struct ol_ath_softc_net80211 *)osif_priv->legacy_osif_priv;
    ic = &scn->sc_ic;
    wmi_handle = lmac_get_wmi_hdl(scn->soc->psoc_obj);
    if (wmi_service_enabled(wmi_handle, wmi_service_regulatory_db)) {
        ieee80211_reg_get_current_chan_list(ic, curr_chan_list);

        qdf_event_set(&ic->ic_wait_for_init_cc_response);
    }

    return QDF_STATUS_SUCCESS;
}

QDF_STATUS ol_ath_set_country_failed(struct wlan_objmgr_pdev *pdev)
{
    struct ol_ath_softc_net80211 *scn;
    struct pdev_osif_priv *osif_priv;
    struct ieee80211com *ic;

    osif_priv = wlan_pdev_get_ospriv(pdev);

    if (osif_priv == NULL) {
        qdf_info("%s : osif_priv is NULL", __func__);
        return QDF_STATUS_E_FAILURE;
    }

    scn = (struct ol_ath_softc_net80211 *)osif_priv->legacy_osif_priv;
    ic = &scn->sc_ic;
    ic->ic_set_country_failed = true;
    qdf_event_set(&ic->ic_wait_for_init_cc_response);

    return QDF_STATUS_SUCCESS;
}

uint32_t ol_ath_get_interface_id(struct wlan_objmgr_pdev *pdev)
{
    struct ol_ath_softc_net80211 *scn;
    struct pdev_osif_priv *osif_priv;

    osif_priv = wlan_pdev_get_ospriv(pdev);

    if (osif_priv == NULL) {
        qdf_info("%s : osif_priv is NULL", __func__);
        return (uint32_t)-1;
    }

    scn = (struct ol_ath_softc_net80211 *)osif_priv->legacy_osif_priv;

    return scn->radio_id;
}

uint8_t ol_ath_is_bcn_mode_burst(struct wlan_objmgr_pdev *pdev)
{
    struct ol_ath_softc_net80211 *scn;
    struct pdev_osif_priv *osif_priv;

    if (!pdev) {
        qdf_info("pdev is null!");
        return 0;
    }

    osif_priv = wlan_pdev_get_ospriv(pdev);
    if (!osif_priv) {
        qdf_info("osif_priv is NULL");
        return 0;
    }
    scn = (struct ol_ath_softc_net80211 *)osif_priv->legacy_osif_priv;
    if (!scn) {
        qdf_info("scn is NULL");
        return 0;
    }

    return ((scn->bcn_mode == BEACON_TX_MODE_BURST)? 1 : 0);
}

void ol_ath_init_and_enable_radar_table(struct ol_ath_softc_net80211 *scn)
{
    struct ieee80211com  *ic;
    struct wlan_objmgr_pdev *pdev;
    struct wlan_objmgr_psoc *psoc;
    struct wlan_lmac_if_dfs_rx_ops *dfs_rx_ops;
    struct common_wmi_handle *wmi_handle;

    ic = &scn->sc_ic;

    pdev = ic->ic_pdev_obj;
    wmi_handle = lmac_get_wmi_hdl(scn->soc->psoc_obj);
    if(pdev == NULL) {
        qdf_info("%s : pdev is null", __func__);
        return;
    }

    psoc = wlan_pdev_get_psoc(pdev);
    if (psoc == NULL) {
        qdf_info("%s : psoc is null", __func__);
        return;
    }

    dfs_rx_ops = wlan_lmac_if_get_dfs_rx_ops(psoc);
    if (dfs_rx_ops) {
        if (wlan_objmgr_pdev_try_get_ref(pdev, WLAN_DFS_ID) !=
                QDF_STATUS_SUCCESS) {
            return;
        }

        if (!wmi_service_enabled(wmi_handle, wmi_service_dfs_phyerr_offload) &&
                dfs_rx_ops->dfs_get_radars)
            dfs_rx_ops->dfs_get_radars(pdev);

        if (dfs_rx_ops->dfs_radar_enable)
            dfs_rx_ops->dfs_radar_enable(pdev, 0, ic->ic_opmode);
        wlan_objmgr_pdev_release_ref(pdev, WLAN_DFS_ID);
    }
}

uint32_t ol_ath_num_mcast_tbl_elements(struct ieee80211com *ic)
{
    struct ol_ath_softc_net80211 *scn;
    struct wlan_objmgr_psoc *psoc = NULL;
    target_resource_config *tgt_cfg;

    scn = OL_ATH_SOFTC_NET80211(ic);
    psoc = scn->soc->psoc_obj;
    tgt_cfg = lmac_get_tgt_res_cfg(psoc);
    if (!tgt_cfg) {
        qdf_info("%s: psoc target res cfg is null", __func__);
        return 0;
    }

    return tgt_cfg->num_mcast_table_elems;
}

uint32_t ol_ath_num_mcast_grps(struct ieee80211com *ic)
{
    struct ol_ath_softc_net80211 *scn;
    struct wlan_objmgr_psoc *psoc = NULL;
    target_resource_config *tgt_cfg;

    scn = OL_ATH_SOFTC_NET80211(ic);
    psoc = scn->soc->psoc_obj;
    tgt_cfg = lmac_get_tgt_res_cfg(psoc);
    if (!tgt_cfg) {
        qdf_info("%s: psoc target res cfg is null", __func__);
        return 0;
    }

    return tgt_cfg->num_mcast_groups;
}

bool ol_ath_is_target_ar900b(struct ieee80211com *ic)
{
    struct ol_ath_softc_net80211 *scn;
    struct wlan_objmgr_psoc *psoc = NULL;

    scn = OL_ATH_SOFTC_NET80211(ic);
    psoc = scn->soc->psoc_obj;

    return lmac_is_target_ar900b(psoc);
}

int32_t ol_ath_get_pdev_idx(struct ieee80211com *ic)
{
    struct wlan_objmgr_pdev *pdev = NULL;

    pdev = ic->ic_pdev_obj;

    return lmac_get_pdev_idx(pdev);
}

uint32_t ol_ath_get_tgt_type(struct ieee80211com *ic)
{
    struct ol_ath_softc_net80211 *scn;
    struct wlan_objmgr_psoc *psoc = NULL;

    scn = OL_ATH_SOFTC_NET80211(ic);
    psoc = scn->soc->psoc_obj;

    return lmac_get_tgt_type(psoc);
}

#if QCN_ESP_IE
int32_t ol_ath_esp_estimate_event_handler(ol_soc_t sc, uint8_t *data, uint32_t datalen)
{
    ol_ath_soc_softc_t *soc = (ol_ath_soc_softc_t *) sc;
    struct esp_estimation_event event, *ev = &event;
    struct ieee80211com *ic;
    struct common_wmi_handle *wmi_handle;
    struct wlan_objmgr_pdev *pdev;

    wmi_handle = lmac_get_wmi_hdl(soc->psoc_obj);
    if(!wmi_handle || wmi_extract_esp_estimate_ev_param(wmi_handle, data, ev)) {
        return -1;
    }

    pdev = wlan_objmgr_get_pdev_by_id(soc->psoc_obj, PDEV_UNIT(ev->pdev_id),
                                      WLAN_MLME_SB_ID);
    if (!pdev) {
        qdf_info("pdev object (id: %d) is NULL", PDEV_UNIT(ev->pdev_id));
        return -1;
    }
    ic = wlan_pdev_get_mlme_ext_obj(pdev);
    if (ic) {
        ic->ic_fw_esp_air_time = ev->ac_airtime_percentage;
        ic->ic_esp_flag = 1;
    }
    wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);

    return 0;
}
#endif /* QCN_ESP_IE */

int ol_ath_send_delba(void *pdev_handle, void *ctrl_peer,
                    uint8_t *peer_macaddr, uint8_t tid, void *vdev_handle,
                    uint8_t reason_code)
{
    struct ieee80211_node *peer_ni;
    struct ol_ath_softc_net80211 *scn;
    struct ieee80211com *ic;
    struct wlan_objmgr_pdev *pdev_obj = (struct wlan_objmgr_pdev *)pdev_handle;
    struct ieee80211_node *ni;
    struct ieee80211_action_mgt_args actionargs;

    scn = (struct ol_ath_softc_net80211 *)lmac_get_pdev_feature_ptr(pdev_obj);
    ni = wlan_peer_get_mlme_ext_obj(ctrl_peer);

    if (scn == NULL) {
        return -ENOENT;
    }

    ic = &scn->sc_ic;

    peer_ni = ieee80211_find_node(&ic->ic_sta, peer_macaddr);

    if (peer_ni == NULL) {
        qdf_info(" DP peer is not NULL, but UMAC peer is null ");
        return -ENOENT;
    }

    if (peer_ni != ni) {
        qdf_info(" DP peer & UMAC peer is not matching ");
        ieee80211_free_node(peer_ni);
        return -ENOENT;
    }

    IEEE80211_NOTE(ni->ni_vap, IEEE80211_MSG_ACTION, ni,
                "[%s]: Sending delba- node:0x%pK(%s) Tid: %d\n",
                __func__, ni, ether_sprintf(ni->ni_macaddr), tid);

    actionargs.category     = IEEE80211_ACTION_CAT_BA;
    actionargs.action       = IEEE80211_ACTION_BA_DELBA;
    actionargs.arg1         = tid;
    actionargs.arg2         = 0;
    actionargs.arg3         = reason_code;
    ieee80211_send_action(ni, &actionargs, NULL);

    ieee80211_free_node(peer_ni);

   return 0;
}

static int ol_ath_bsscolor_collision_det_config_event_handler(ol_soc_t sc,
        uint8_t *data, uint32_t datalen) {
    ol_ath_soc_softc_t *soc           = (ol_ath_soc_softc_t *) sc;
    struct ol_ath_softc_net80211 *scn = NULL;
    struct ieee80211vap *vap          = NULL;
    struct ieee80211com *ic           = NULL;
    struct ol_ath_vap_net80211 *avn   = NULL;
    struct wlan_objmgr_vdev *vdev;
    struct common_wmi_handle *wmi_handle;
    struct wmi_obss_color_collision_info info;
    uint32_t vdev_id;
    int error                         = 0;

    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
                                            "%s>>", __func__);

    wmi_handle = lmac_get_wmi_hdl(soc->psoc_obj);

    if ((wmi_handle == NULL) ||
                wmi_unified_extract_obss_color_collision_info(wmi_handle,
                data, &info) != QDF_STATUS_SUCCESS) {
        QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_ERROR,
            "%s<< Extracting bss color collision info failed", __func__);
        return -1;
    }

    vdev_id = info.vdev_id;
    vdev = wlan_objmgr_get_vdev_by_id_from_psoc(
            soc->psoc_obj, vdev_id, WLAN_MLME_SB_ID);

    if (vdev == NULL) {
        QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_ERROR,
            "%s<< Unable to find vdev for %d vdev_id", __func__, vdev_id);
        return -EINVAL;
    }

    vap = wlan_vdev_get_mlme_ext_obj(vdev);
    if (vap == NULL) {
        qdf_err(" vap is NULL");
        wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);
        return QDF_STATUS_E_FAILURE;
    }
    avn  = OL_ATH_VAP_NET80211(vap);
    scn  = avn->av_sc;
    ic   = &scn->sc_ic;

    switch(info.evt_type) {
        case OBSS_COLOR_COLLISION_DETECTION:
            /* Disable BSS Color collision fw offload */
            ol_ath_config_bsscolor_offload(vap, true);

            /* if user has overridden bsscolor to force it
             * then disable force bit on collision detection
             * as the collision has happened on the forced
             * color
             */
            ic->ic_he_bsscolor_override = false;

            /* call BSS Color detection call back to bsscolor
             * module
             */
            ic->ic_bsscolor_hdl.ieee80211_bss_color_collision_detection_hdler_cb
                        (ic, info.obss_color_bitmap_bit0to31,
                         info.obss_color_bitmap_bit32to63);
            break;
        default:
            QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_ERROR,
                "Unhandled BSS Color Event : 0x%x received", info.evt_type);
    }


    wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);

    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
                            "%s<< error: %d", __func__, error);
    return error;
}

void ol_ath_config_bsscolor_offload(wlan_if_t vap, bool disable) {
    struct ieee80211com *ic           = vap->iv_ic;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    struct ol_ath_vap_net80211 *avn   = OL_ATH_VAP_NET80211(vap);
    struct common_wmi_handle *pdev_wmi_handle;
    struct wmi_obss_color_collision_cfg_param collision_cfg_param;

    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
                        "%s>> %s", __func__, disable ? "true" : "false");

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);

    qdf_mem_zero(&collision_cfg_param,
            sizeof(struct wmi_obss_color_collision_cfg_param));

    /* populate bss_color_collision_cfg_cmd */
    collision_cfg_param.vdev_id                  = avn->av_if_id;
    collision_cfg_param.scan_period_ms           =
                    IEEE80211_BSS_COLOR_COLLISION_SCAN_PERIOD_MS;

    if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
        if (disable) {
            collision_cfg_param.evt_type             =
                            OBSS_COLOR_COLLISION_DETECTION_DISABLE;
        } else {
            collision_cfg_param.evt_type             =
                            OBSS_COLOR_COLLISION_DETECTION;
        }
        collision_cfg_param.current_bss_color        =
                        ic->ic_bsscolor_hdl.selected_bsscolor;
        collision_cfg_param.detection_period_ms      =
                        IEEE80211_BSS_COLOR_COLLISION_DETECTION_AP_PERIOD_MS;
    } else {
        collision_cfg_param.evt_type                 =
                        OBSS_COLOR_COLLISION_DETECTION;
        collision_cfg_param.detection_period_ms      =
                        IEEE80211_BSS_COLOR_COLLISION_DETECTION_STA_PERIOD_MS;

        /* send bss_color_change_enable_cmd with enable set to true */
        if ((pdev_wmi_handle == NULL) ||
                wmi_unified_send_bss_color_change_enable_cmd
                (pdev_wmi_handle, avn->av_if_id, true)) {
            QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_ERROR,
                "wmi_bss_color_change_enable_cmd returned failure");
        }
    }

    if ((pdev_wmi_handle == NULL) ||
            wmi_unified_send_obss_color_collision_cfg_cmd
                (pdev_wmi_handle, &collision_cfg_param)) {
        QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_ERROR,
                "wmi_obss_color_collision_cfg_param returned failure");
    }

    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
                                            "%s<<", __func__);
}

void
ol_ath_mgmt_register_bsscolor_collision_det_config_event(struct ieee80211com *ic) {
    struct wlan_objmgr_psoc *psoc;
    wmi_unified_t wmi_handle;

    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
                                            "%s>>", __func__);

    psoc = wlan_pdev_get_psoc(ic->ic_pdev_obj);

    wmi_handle = lmac_get_wmi_unified_hdl(psoc);

    if (wmi_handle) {
        wmi_unified_register_event_handler(wmi_handle,
                wmi_obss_color_collision_report_event_id,
                ol_ath_bsscolor_collision_det_config_event_handler,
                WMI_RX_UMAC_CTX);
    } else {
        QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_ERROR,
                "wmi_handle is null. coudl not register hdler");
    }

    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
                                            "%s<<", __func__);
}

uint8_t ol_ath_fill_umac_radio_band_info(struct wlan_objmgr_pdev *pdev)
{
    enum channel_state state;
    struct regulatory_channel *curr_chan_list;
    uint8_t i, num_valid_chan = 0;
    uint32_t low_5g = 0, high_5g = 0;
    uint8_t radio_band = NO_BAND_INFORMATION_AVAILABLE;
    uint32_t fiveg_channel[NUM_CHANNELS];
    int status = 0;

    curr_chan_list = qdf_mem_malloc
        (NUM_CHANNELS*sizeof(struct regulatory_channel));

    if (curr_chan_list == NULL) {
        QDF_TRACE(QDF_MODULE_ID_SON, QDF_TRACE_LEVEL_ERROR,"%s: fail to alloc",
                  __func__);
        return NO_BAND_INFORMATION_AVAILABLE;
    }

    status = ucfg_reg_get_current_chan_list (pdev, curr_chan_list);

    if (QDF_IS_STATUS_ERROR(status)) {
        QDF_TRACE(QDF_MODULE_ID_SON, QDF_TRACE_LEVEL_ERROR,
                  "Couldn't get current chan list");
        qdf_mem_free(curr_chan_list);
        return NO_BAND_INFORMATION_AVAILABLE;
    }

    for (i=0; i<NUM_CHANNELS; i++) {
        state = curr_chan_list[i].state;
        if (state!= CHANNEL_STATE_DISABLE && state!= CHANNEL_STATE_INVALID
            && curr_chan_list[i].center_freq > MAX_FREQ_IN_TWOG) {
            fiveg_channel[num_valid_chan++]  = curr_chan_list[i].center_freq;
        }
    }

    if (!num_valid_chan) {
        qdf_mem_free(curr_chan_list);
        return NON_5G_RADIO;
    }

    low_5g = fiveg_channel[0];
    high_5g = fiveg_channel[num_valid_chan-1];
    if (low_5g >= LOW_BAND_MIN_FIVEG_FREQ && high_5g <= LOW_BAND_MAX_FIVEG_FREQ)
        radio_band = LOW_BAND_RADIO;
    else if (low_5g > LOW_BAND_MAX_FIVEG_FREQ)
        radio_band = HIGH_BAND_RADIO;
    else if (low_5g >= LOW_BAND_MIN_FIVEG_FREQ && high_5g > LOW_BAND_MAX_FIVEG_FREQ)
        radio_band = FULL_BAND_RADIO;

    qdf_mem_free(curr_chan_list);
    return radio_band;
}
#endif
