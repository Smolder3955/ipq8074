/*
 * Copyright (c) 2013-2019 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.

 * 2013-2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * Copyright (c) 2010, Atheros Communications Inc.
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

#include "ol_if_athvar.h"
#include <hif.h>
#include <bmi.h>
#include <osdep.h>
#include <wbuf.h>
#include <linux/firmware.h>
#include <linux/pci.h>
#include <linux/proc_fs.h>
#include "sw_version.h"
#include "ieee80211_var.h"
#include "ieee80211_ioctl.h"
#include "ol_if_athvar.h"
#if UMAC_SUPPORT_CFG80211
#include "wlan_cfg80211_api.h"
#endif
#include "if_athioctl.h"
#include "osif_private.h"
#include "osapi_linux.h"
#include "if_media.h"
#include "bmi.h"
#include "target_type.h"
#include "reg_struct.h"
#include "regtable.h"
#include "ol_ath.h"
#include "epping_test.h"
#include "ol_helper.h"
#include "a_debug.h"
#include "pktlog_ac_api.h"
#include "ol_regdomain.h"
#include "ieee80211_ioctl_acfg.h"
#include "ald_netlink.h"
#include "ath_pci.h"
#include "bin_sig.h"
#include <ol_if_thermal.h>
#include "ah_devid.h"
#include "cdp_txrx_ctrl.h"
#include "ol_swap.h"
#include <linux/dma-mapping.h>
#include "ath_pci.h"
/* [FIXME_MR] Added for pdev attach workaround and will be removed when the workaround isn't needed any more */
//#include <ol_txrx_types.h>		/* ol_txrx_pdev_t */
#include "wlan_defs.h" /* Temporary added for VOW, needs to be removed */

#if defined(CONFIG_HL_SUPPORT)
#include "wlan_tgt_def_config_hl.h"    /* TODO: check if we need a seperated config file */
#else
#include "wlan_tgt_def_config.h"
#endif

#if ATH_SUPPORT_LOWI
#include "ath_lowi_if.h"
#endif

#include "ath_netlink.h"

#ifdef A_SIMOS_DEVHOST
#include "sim_io.h"
#endif
#ifdef QVIT
#include <qvit/qvit_defs.h>
#endif
#if PERF_FIND_WDS_NODE
#include "wds_addr_api.h"
#endif
#if MESH_MODE_SUPPORT
#include <if_meta_hdr.h>
#endif
#if WIFI_MEM_MANAGER_SUPPORT
#include "mem_manager.h"
#endif
#ifdef ATH_PARAMETER_API
#include "ath_papi.h"
#endif

#include "ol_ath_ucfg.h"
#include <targaddrs.h>

#if LINUX_VERSION_CODE > KERNEL_VERSION(3,4,103)
#include <osif_fs.h>
#endif
#include "pktlog_ac.h"
#include "cdp_txrx_cmn_reg.h"
#include <ieee80211_objmgr_priv.h>
#include <wlan_lmac_if_def.h>
#include <wlan_lmac_if_api.h>
#include <dispatcher_init_deinit.h>
#include <ext_ioctl_drv_if.h>
#include <target_if.h>
#include <init_deinit_lmac.h>
#include <init_deinit_ops.h>
#include <service_ready_util.h>
#include <wlan_global_lmac_if_api.h>
#include <wlan_osif_priv.h>
#if QCA_AIRTIME_FAIRNESS
#include <target_if_atf.h>
#endif

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
#include <osif_nss_wifiol_if.h>
#endif

#if WLAN_SPECTRAL_ENABLE
#include <wlan_spectral_utils_api.h>
#include <wlan_spectral_public_structs.h>
#include <os_if_spectral_netlink.h>
#endif
#include <wlan_mlme_dispatcher.h>
#include "cfg_ucfg_api.h"
#include "fw_dbglog_api.h"
#include <wlan_mlme_vdev_mgmt_ops.h>

/* TODO: Remove this once bmi changes are abstracted */
#define BMI_DATASZ_MAX                      256
#define BMI_NO_COMMAND                      0
#define BMI_DONE                            1
#define BMI_READ_MEMORY                     2
#define BMI_WRITE_MEMORY                    3
#define BMI_EXECUTE                         4
#define BMI_SET_APP_START                   5
#define BMI_READ_SOC_REGISTER               6
#define BMI_READ_SOC_WORD                   6
#define BMI_WRITE_SOC_REGISTER              7
#define BMI_WRITE_SOC_WORD                  7
#define BMI_GET_TARGET_ID                  8
#define BMI_GET_TARGET_INFO                8
#define BMI_ROMPATCH_INSTALL               9
#define BMI_ROMPATCH_UNINSTALL             10
#define BMI_ROMPATCH_ACTIVATE              11
#define BMI_ROMPATCH_DEACTIVATE            12
#define BMI_LZ_STREAM_START                13
#define BMI_LZ_DATA                        14
#define BMI_NVRAM_PROCESS                  15
#define BMI_NVRAM_SEG_NAME_SZ 16
#define BMI_SIGN_STREAM_START		    17

#define UNSIGNED_MAX_24BIT                 0xFFFFFF

#ifdef AH_CAL_IN_FLASH_PCI
extern int pci_dev_cnt;
#endif

int dfs_disable = 0;
module_param(dfs_disable, uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
EXPORT_SYMBOL(dfs_disable);

/* PLL parameters Start */
int32_t frac = -1;
module_param(frac, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
EXPORT_SYMBOL(frac);

int32_t intval = -1;
module_param(intval, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
EXPORT_SYMBOL(intval);

/* PLL parameters End */
int32_t ar900b_20_targ_clk = -1;
module_param(ar900b_20_targ_clk, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
EXPORT_SYMBOL(ar900b_20_targ_clk);

int32_t qca9888_20_targ_clk = -1;
module_param(qca9888_20_targ_clk, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
EXPORT_SYMBOL(qca9888_20_targ_clk);

int32_t ipq4019_20_targ_clk = -1;
module_param(ipq4019_20_targ_clk, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
EXPORT_SYMBOL(ipq4019_20_targ_clk);

/* Module parameter to input a list of IEEE channels to scan.  This is primarily
 * intended as a debug feature for environments such as emulation platforms. The
 * module parameter mechanism is used so that regular QSDK configuration
 * recipes used in testing are left undisturbed as far as possible (for fixed
 * BSS channel scenarios).
 * It is the end user's responsibility to ensure appropriateness of channel
 * numbers passed while using this debug mechanism, since regulations keep
 * evolving.
 *
 * Example usage: insmod umac.ko <other params...> ol_scan_chanlist=1,6,11,36
 */
unsigned short ol_scan_chanlist[IEEE80211_CUSTOM_SCAN_ORDER_MAXSIZE];
int ol_scan_chanlist_size = 0;
module_param_array(ol_scan_chanlist,
        ushort, &ol_scan_chanlist_size, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(ol_scan_chanlist,
        "offload: Specifies list of IEEE channels to scan in client mode");
EXPORT_SYMBOL(ol_scan_chanlist);
EXPORT_SYMBOL(ol_scan_chanlist_size);

unsigned int ol_dual_band_5g_radios = 0;
module_param(ol_dual_band_5g_radios, uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(ol_dual_band_5g_radios, "Bitmap of dual band radios configured as 5G");
EXPORT_SYMBOL(ol_dual_band_5g_radios);

unsigned int ol_dual_band_2g_radios = 0;
module_param(ol_dual_band_2g_radios, uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(ol_dual_band_2g_radios, "Bitmap of dual band radios configured as 2G");
EXPORT_SYMBOL(ol_dual_band_2g_radios);

/* function prototypes related to FW_CODE_SIGN
*/

#if FW_CODE_SIGN
static int request_secure_firmware(struct firmware **fw_entry, const char *file,
                                    struct device *dev, int dev_id);
static void release_secure_firmware(struct firmware *fw_entry);
static inline void htonlm(void *sptr, int len);
static inline void ntohlm(void *sptr, int len);
static inline int fw_check_img_magic(unsigned int img_magic);
static inline int fw_check_chip_id(unsigned int chip_id);
static inline int fw_sign_check_file_magic(unsigned int file_type);
static inline int fw_sign_get_hash_algo (struct firmware_head *h);
static struct firmware_head *fw_unpack(unsigned char *fw, int chip_id,
                                   int *err);
static const unsigned char  * fw_sign_get_cert_buffer(struct firmware_head *h,
                                   int *cert_type, int *len);
static inline int fw_sign_get_pk_algo (struct firmware_head *h);
static inline int fw_check_sig_algorithm(int s);
static void fw_hex_dump(unsigned char *p, int len);
#endif /* FW_CODE_SIGN */
void wmi_proc_remove(wmi_unified_t wmi_handle, struct proc_dir_entry *par_entry, int id);

#ifndef QCA_PARTNER_PLATFORM
void ol_ath_check_and_enable_btcoex_support(void *bdev, ol_ath_soc_softc_t *soc);
#endif
extern int ol_ath_target_start(struct ol_ath_soc_softc *soc);
int osif_delete_vap_recover(struct net_device *dev);
#ifdef ATH_AHB
void ahb_defer_reconnect(void *ahb_reconnect_work);
#endif
void pci_defer_reconnect(void *context);

/*
 * Maximum acceptable MTU
 * MAXFRAMEBODY - WEP - QOS - RSN/WPA:
 * 2312 - 8 - 2 - 12 = 2290
 */
#define ATH_MAX_MTU     2290
#define ATH_MIN_MTU     32
#define MAX_UTF_LENGTH 2048


void ol_ath_pdev_proc_create(struct ieee80211com *ic,struct net_device *dev);
void ol_ath_pdev_proc_detach(struct ieee80211com *ic);

/*
** Prototype for iw attach
*/

#ifdef ATH_SUPPORT_LINUX_STA
#ifdef CONFIG_SYSCTL
void ath_dynamic_sysctl_register(struct ol_ath_softc_net80211 *sc);
void ath_dynamic_sysctl_unregister(struct ol_ath_softc_net80211 *sc);
#endif
#endif
#if OS_SUPPORT_ASYNC_Q
static void os_async_mesg_handler( void  *ctx, u_int16_t  mesg_type, u_int16_t  mesg_len, void  *mesg );
#endif

#if UMAC_SUPPORT_WEXT
void ol_ath_iw_attach(struct net_device *dev);
void ol_ath_iw_detach(struct net_device *dev);
#endif
#if !NO_SIMPLE_CONFIG
extern int32_t unregister_simple_config_callback(char *name);
extern int32_t register_simple_config_callback (char *name, void *callback, void *arg1, void *arg2);
static irqreturn_t jumpstart_intr(int cpl, void *dev_id, struct pt_regs *regs, void *push_dur);
#endif

#if defined(ATH_TX99_DIAG) && (!defined(ATH_PERF_PWR_OFFLOAD))
extern u_int8_t tx99_ioctl(struct net_device *dev, struct ol_ath_softc_net80211 *sc, int cmd, void *addr);
#endif

#ifdef HIF_SDIO
#define NOHIFSCATTERSUPPORT_DEFAULT    1
unsigned int nohifscattersupport = NOHIFSCATTERSUPPORT_DEFAULT;
#endif

#ifdef QCA_PARTNER_PLATFORM
extern void WAR_PLTFRM_PCI_WRITE32(char *addr, u32 offset, u32 value, unsigned int war1);
extern void ath_pltfrm_init( struct net_device *dev );
#endif

extern ol_ath_soc_softc_t *ol_global_soc[GLOBAL_SOC_SIZE];
extern int ol_num_global_soc;

unsigned int testmode = 0;
module_param(testmode, int, 0644);
MODULE_PARM_DESC(testmode, "Configures driver in factory test mode");
EXPORT_SYMBOL(testmode);

unsigned int polledmode = 0;
module_param(polledmode, int, 0644);
MODULE_PARM_DESC(polledmode, "Configures driver in polled mode");
EXPORT_SYMBOL(polledmode);

static int
proc_ic_config_show(struct seq_file *m, void *v)
{
    struct ieee80211com *ic = (struct ieee80211com *) m->private;
#define IC_UINT_FIELD(x)   seq_printf(m, #x ": %u\n", ic->x)
#define IC_UINT64_FIELD(x) seq_printf(m, #x ": %llu\n", ic->x)

    seq_printf(m, "ic_my_hwaddr: %s\n", ether_sprintf(ic->ic_my_hwaddr));
    seq_printf(m, "ic_myaddr: %s\n", ether_sprintf(ic->ic_myaddr));
#if ATH_SUPPORT_WIFIPOS
    IC_UINT_FIELD(ic_initialized);
    IC_UINT_FIELD(ic_scan_entry_max_count);
    IC_UINT_FIELD(ic_scan_entry_timeout);
    IC_UINT64_FIELD(ic_lock_flags);
    IC_UINT64_FIELD(ic_main_sta_lock_flags);
    IC_UINT64_FIELD(ic_chan_change_lock_flags);
    IC_UINT_FIELD(ic_chanchange_serialization_flags);
    IC_UINT64_FIELD(ic_beacon_alloc_lock_flags);
    IC_UINT_FIELD(ic_flags);
    IC_UINT_FIELD(ic_flags_ext);
    IC_UINT_FIELD(ic_flags_ext2);
    IC_UINT_FIELD(ic_chanswitch_flags);
    IC_UINT_FIELD(ic_wep_tkip_htrate);
    IC_UINT_FIELD(ic_non_ht_ap);
    IC_UINT_FIELD(ic_block_dfschan);
    IC_UINT_FIELD(ic_doth);
    IC_UINT_FIELD(ic_off_channel_support);
    IC_UINT_FIELD(ic_ht20Adhoc);
    IC_UINT_FIELD(ic_ht40Adhoc);
    IC_UINT_FIELD(ic_htAdhocAggr);
    IC_UINT_FIELD(ic_disallowAutoCCchange);
    IC_UINT_FIELD(ic_p2pDevEnable);
    IC_UINT_FIELD(ic_ignoreDynamicHalt);
    IC_UINT_FIELD(ic_override_proberesp_ie);
    IC_UINT_FIELD(ic_wnm);
    IC_UINT_FIELD(ic_2g_csa);
    IC_UINT_FIELD(ic_dropstaquery);
    IC_UINT_FIELD(ic_wnm);
    IC_UINT_FIELD(ic_2g_csa);
    IC_UINT_FIELD(ic_dropstaquery);
    IC_UINT_FIELD(ic_wnm);
    IC_UINT_FIELD(ic_2g_csa);
    IC_UINT_FIELD(ic_dropstaquery);
    IC_UINT_FIELD(ic_blkreportflood);
    IC_UINT_FIELD(ic_offchanscan);
    IC_UINT_FIELD(ic_consider_obss_long_slot);
    IC_UINT_FIELD(ic_sta_vap_amsdu_disable);
    IC_UINT_FIELD(ic_enh_ind_rpt);
    IC_UINT_FIELD(ic_strict_pscan_enable);
    IC_UINT_FIELD(ic_min_rssi_enable);
    IC_UINT_FIELD(ic_no_vlan);
    IC_UINT_FIELD(ic_atf_logging);
    IC_UINT_FIELD(ic_caps);
    IC_UINT_FIELD(ic_caps_ext);
    IC_UINT_FIELD(ic_cipher_caps);
    IC_UINT_FIELD(ic_ath_cap);
    IC_UINT_FIELD(ic_roaming);
#endif
#if ATH_SUPPORT_WRAP
    IC_UINT_FIELD(ic_nwrapvaps);
    IC_UINT_FIELD(ic_proxystarxwar);
    IC_UINT_FIELD(ic_wrap_vap_sgi_cap);
    IC_UINT_FIELD(ic_ast_reserve_status);
#endif
#if ATH_SUPPORT_WIFIPOS
    IC_UINT_FIELD(ic_isoffchan);
    IC_UINT_FIELD(ic_ath_extcap);
    IC_UINT_FIELD(ic_nopened);
    IC_UINT64_FIELD(ic_modecaps);
    IC_UINT_FIELD(ic_curmode);
    IC_UINT_FIELD(ic_intval);
    IC_UINT_FIELD(ic_lintval);
    IC_UINT_FIELD(ic_lintval_assoc);
    IC_UINT_FIELD(ic_holdover);
    IC_UINT_FIELD(ic_bmisstimeout);
    IC_UINT_FIELD(ic_txpowlimit);
    IC_UINT_FIELD(ic_uapsdmaxtriggers);
    IC_UINT_FIELD(ic_coverageclass);
    IC_UINT_FIELD(ic_htflags);
    IC_UINT_FIELD(ic_htcap);
    IC_UINT_FIELD(ic_htextcap);
    IC_UINT_FIELD(ic_ldpccap);
    IC_UINT_FIELD(ic_maxampdu);
    IC_UINT_FIELD(ic_mpdudensity);
    IC_UINT_FIELD(ic_mpdudensityoverride);
    IC_UINT_FIELD(ic_enable2GHzHt40Cap);
    IC_UINT_FIELD(ic_weptkipaggr_rxdelim);
    IC_UINT_FIELD(ic_channelswitchingtimeusec);
    IC_UINT_FIELD(ic_no_weather_radar_chan);
    IC_UINT_FIELD(ic_implicitbf);
    IC_UINT_FIELD(ic_ampdu_limit);
    IC_UINT_FIELD(ic_ampdu_density);
    IC_UINT_FIELD(ic_ampdu_subframes);
    IC_UINT_FIELD(ic_amsdu_limit);
    IC_UINT_FIELD(ic_amsdu_max_size);
    IC_UINT_FIELD(ic_tx_chainmask);
    IC_UINT_FIELD(ic_rx_chainmask);
    IC_UINT_FIELD(ic_num_rx_chain);
    IC_UINT_FIELD(ic_num_tx_chain);
    IC_UINT_FIELD(ic_num_wapi_tx_maxchains);
    IC_UINT_FIELD(ic_num_wapi_rx_maxchains);
    IC_UINT_FIELD(ic_spatialstreams);
    IC_UINT_FIELD(ic_multiDomainEnabled);
    IC_UINT_FIELD(ic_isdfsregdomain);
    IC_UINT_FIELD(ic_nchans);
    IC_UINT_FIELD(ic_chanidx);
    IC_UINT_FIELD(ic_nregclass);
    IC_UINT_FIELD(ic_wnm_bss_count);
    IC_UINT_FIELD(ic_wnm_bss_active);
    IC_UINT_FIELD(ic_protmode);
    IC_UINT_FIELD(ic_sta_assoc);
    IC_UINT_FIELD(ic_nonerpsta);
    IC_UINT_FIELD(ic_longslotsta);
    IC_UINT_FIELD(ic_ht_sta_assoc);
#endif
#if ATH_TxBF_DYNAMIC_LOF_ON_N_CHAIN_MASK
    IC_UINT_FIELD(ic_ht_txbf_sta_assoc);
#endif
#if ATH_SUPPORT_WIFIPOS
    IC_UINT_FIELD(ic_ht_gf_sta_assoc);
    IC_UINT_FIELD(ic_ht40_sta_assoc);
    IC_UINT_FIELD(ic_ht20_only);
    IC_UINT_FIELD(ic_curchanmaxpwr);
    IC_UINT_FIELD(ic_chanchange_tbtt);
    IC_UINT_FIELD(ic_rcsa_count);
    IC_UINT_FIELD(ic_chanchange_chan);
    IC_UINT_FIELD(ic_chanchange_chwidth);
    IC_UINT_FIELD(ic_chanchange_secoffset);
    IC_UINT64_FIELD(ic_chanchange_chanflag);
    IC_UINT_FIELD(ic_os_rxfilter);
    IC_UINT_FIELD(ic_num_ap_vaps);
    IC_UINT_FIELD(ic_num_lp_iot_vaps);
    IC_UINT_FIELD(ic_need_vap_reinit);
    IC_UINT_FIELD(ic_intolerant40);
    IC_UINT_FIELD(ic_enable2040Coexist);
    IC_UINT_FIELD(ic_tspec_active);
    IC_UINT_FIELD(cw_inter_found);
    IC_UINT_FIELD(ic_eacs_done);
    IC_UINT_FIELD(ic_acs_ctrlflags);
    IC_UINT_FIELD(ic_softap_enable);
#ifdef ATH_BT_COEX
    IC_UINT_FIELD(ic_bt_coex_opmode);
#endif
    IC_UINT_FIELD(ic_minframesize);
    IC_UINT_FIELD(ic_ldpcsta_assoc);
    IC_UINT_FIELD(ic_chanbwflag);
#if UMAC_SUPPORT_ADMCTL
    IC_UINT_FIELD(ic_mediumtime_reserved);
#endif
    IC_UINT_FIELD(ic_custom_scan_order_size);
    IC_UINT_FIELD(ic_custom_chanlist_assoc_size);
    IC_UINT_FIELD(ic_custom_chanlist_nonassoc_size);
    IC_UINT_FIELD(ic_use_custom_chan_list);
#if WLAN_SPECTRAL_ENABLE
    IC_UINT_FIELD(chan_clr_cnt);
    IC_UINT_FIELD(cycle_cnt);
    IC_UINT_FIELD(chan_num);
#endif
    IC_UINT_FIELD(ic_vhtcap);
    IC_UINT_FIELD(ic_vhtop_basic_mcs);
    IC_UINT_FIELD(ic_vht_ampdu);
    IC_UINT_FIELD(ic_vht_amsdu);
    IC_UINT_FIELD(ic_no_bfee_limit);
    IC_UINT_FIELD(obss_rssi_threshold);
    IC_UINT_FIELD(obss_rx_rssi_threshold);
    IC_UINT_FIELD(ic_tso_support);
    IC_UINT_FIELD(ic_lro_support);
    IC_UINT_FIELD(ic_sg_support);
    IC_UINT_FIELD(ic_gro_support);
    IC_UINT_FIELD(ic_offload_tx_csum_support);
    IC_UINT_FIELD(ic_offload_rx_csum_support);
    IC_UINT_FIELD(ic_rawmode_support);
    IC_UINT_FIELD(ic_dynamic_grouping_support);
    IC_UINT_FIELD(ic_dpd_support);
    IC_UINT_FIELD(ic_aggr_burst_support);
    IC_UINT_FIELD(ic_qboost_support);
    IC_UINT_FIELD(ic_sifs_frame_support);
    IC_UINT_FIELD(ic_block_interbss_support);
    IC_UINT_FIELD(ic_disable_reset_support);
    IC_UINT_FIELD(ic_msdu_ttl_support);
    IC_UINT_FIELD(ic_ppdu_duration_support);
    IC_UINT_FIELD(ic_promisc_support);
    IC_UINT_FIELD(ic_burst_mode_support);
    IC_UINT_FIELD(ic_peer_flow_control_support);
    IC_UINT_FIELD(ic_mesh_vap_support);
    IC_UINT_FIELD(ic_wds_support);
    IC_UINT_FIELD(ic_disable_bcn_bwnss_map);
    IC_UINT_FIELD(ic_disable_bwnss_adv);
    IC_UINT_FIELD(mon_filter_osif_mac);
    IC_UINT_FIELD(mon_filter_mcast_data);
    IC_UINT_FIELD(mon_filter_ucast_data);
    IC_UINT_FIELD(mon_filter_non_data);
    IC_UINT_FIELD(ic_cal_ver_check);
#if DBDC_REPEATER_SUPPORT
    IC_UINT_FIELD(ic_radio_priority);
    IC_UINT_FIELD(ic_primary_radio);
    IC_UINT_FIELD(ic_preferredUplink);
    IC_UINT_FIELD(fast_lane);
#endif
    IC_UINT_FIELD(ic_num_clients);
    IC_UINT_FIELD(ic_emiwar_80p80);
#endif
#if ATH_GEN_RANDOMNESS
    IC_UINT_FIELD(random_gen_mode);
#endif
#if ATH_SUPPORT_WIFIPOS
    IC_UINT_FIELD(ic_diag_enable);
    IC_UINT_FIELD(ic_chan_stats_th);
    IC_UINT_FIELD(ic_min_rssi);
#endif
#if MESH_MODE_SUPPORT
    IC_UINT_FIELD(meshpeer_timeout_cnt);
#endif
#if ATH_SUPPORT_WIFIPOS
    IC_UINT_FIELD(tid_override_queue_mapping);
    IC_UINT_FIELD(bin_number);
    IC_UINT_FIELD(traf_bins);
    IC_UINT_FIELD(traf_rate);
    IC_UINT_FIELD(traf_interval);
    IC_UINT_FIELD(traf_stats_enable);
    IC_UINT_FIELD(no_chans_available);
    IC_UINT_FIELD(ic_mon_decoder_type);
    IC_UINT_FIELD(ic_strict_doth);
    IC_UINT_FIELD(ic_non_doth_sta_cnt);
    IC_UINT_FIELD(ic_chan_switch_cnt);
    IC_UINT_FIELD(ic_sec_offsetie);
    IC_UINT_FIELD(ic_wb_subelem);
    IC_UINT_FIELD(ic_auth_tx_xretry);
#if UMAC_SUPPORT_CFG80211
    IC_UINT_FIELD(ic_cfg80211_config);
#endif
    if (wlan_pdev_nif_feat_cap_get(ic->ic_pdev_obj,
                                   WLAN_PDEV_F_MBSS_IE_ENABLE)) {
        seq_printf(m, "BSSID index bitmap: 0x%lx\n",
                          ic->ic_mbss.bssid_index_bmap[0]);
        IC_UINT_FIELD(ic_mbss.num_non_transmit_vaps);
        IC_UINT_FIELD(ic_mbss.max_bssid);
    }
#endif
   return 0;
}

static int
proc_dump_mbss_ie_show(struct seq_file *m, void *v)
{
    struct ieee80211com *ic = (struct ieee80211com *) m->private;
    struct ieee80211vap *vap = ic->ic_mbss.transmit_vap;
    struct ol_ath_vap_net80211 *avn =  OL_ATH_VAP_NET80211(vap);
    struct ieee80211_beacon_offsets *bo = &avn->av_beacon_offsets;
    int i, j;
    uint8_t *prof_start, *cap_start, *ie_len_start, *ssid_start, *bssid;
    int len;

    if (!wlan_pdev_nif_feat_cap_get(ic->ic_pdev_obj,
                                    WLAN_PDEV_F_MBSS_IE_ENABLE)) {
        seq_printf(m, "MBSS IE feature not enabled!\n");
        return -1;
    }

    if (ic->ic_mbss.num_non_transmit_vaps == 0) {
        seq_printf(m, "No Non-transmitting VAPs in MBSS IE!\n");
        return 0;
    }

    ie_len_start = bo->bo_mbssid_ie + 1;

    seq_printf(m, "MBSSID IE at: 0x%pK\n", bo->bo_mbssid_ie);

    seq_printf(m, "###################\n");
    seq_printf(m, "Main Element:\n"
                  "\t ID:%d, Length:%d, MaxBSSID indicator:%d\n",
                  *(ie_len_start-1), *(ie_len_start), *(ie_len_start+1));

    prof_start = bo->bo_mbssid_ie + 3;

    for (i = 0;i < ic->ic_mbss.num_non_transmit_vaps; i++) {

      len = *(prof_start + 1);
      seq_printf(m, "\n######### Profile %d at 0x%pK: ###########\n", i+1, prof_start);

      seq_printf(m, "Profile Subelement:\n"
		 "\t Subelem ID:%d, Length:%d\n",
		 *(prof_start), *(prof_start+1));

      cap_start = prof_start + 2;
      seq_printf(m, "Capability:\n"
		  "\t Elem ID:%d, Length:%d Capability:0x%x 0x%x\n",
		 *(cap_start), *(cap_start+1), *(cap_start+2), *(cap_start+3));

      seq_printf(m, "SSID:\n"
		  "\t Elem ID:%d Length:%d SSID: ",
		 *(cap_start+4), *(cap_start+5));

      ssid_start = cap_start + 4;
      for(j=0;j< *(cap_start + 5);j++)
	seq_printf(m,"%c", *(ssid_start + 2 + j));
      seq_printf(m, "\n");

      bssid = ssid_start + 2 + *(ssid_start + 1);
      seq_printf(m,"BSSID index:\n"
		  "\tElem ID:%d Length:%d Index:%d DTIM period:%d DTIM count:%d\n\n",
		*(bssid), *(bssid+1), *(bssid+2), *(bssid+3), *(bssid+4));

      prof_start = prof_start + 2 + len;
    }

    return 0;
}

struct pdev_proc_info {
    const char *name;
    struct file_operations *ops;
    int extra;
};

#define PROC_FOO(func_base, extra) { #func_base, &proc_ieee80211_##func_base##_ops, extra }

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
#define GENERATE_PROC_STRUCTS(func_base)                                \
        static int proc_##func_base##_open(struct inode *inode, \
                                           struct file *file)           \
        {                                                               \
                struct proc_dir_entry *dp = PDE(inode);                 \
                return single_open(file, proc_##func_base##_show, dp->data); \
        }                                                               \
                                                                        \
        static struct file_operations proc_ieee80211_##func_base##_ops = { \
                .open           = proc_##func_base##_open,      \
                .read           = seq_read,                             \
                .llseek         = seq_lseek,                            \
                .release        = single_release,                       \
        };
#else
#define GENERATE_PROC_STRUCTS(func_base)                                \
        static int proc_##func_base##_open(struct inode *inode, \
                                           struct file *file)           \
        {                                                               \
        return single_open(file, proc_##func_base##_show, PDE_DATA(inode)); \
    } \
                                                                        \
                                                                        \
        static struct file_operations proc_ieee80211_##func_base##_ops = { \
                .open           = proc_##func_base##_open,      \
                .read           = seq_read,                             \
                .llseek         = seq_lseek,                            \
                .release        = single_release,                       \
        };
#endif

GENERATE_PROC_STRUCTS(ic_config);
GENERATE_PROC_STRUCTS(dump_mbss_ie);

struct pdev_proc_info pdev_proc_infos[] = {
  PROC_FOO(ic_config,0),
  PROC_FOO(dump_mbss_ie, 0),
};

#define NUM_PROC_INFOS (sizeof(pdev_proc_infos) / sizeof(pdev_proc_infos[0]))

void ol_ath_pdev_proc_create(struct ieee80211com *ic,struct net_device *dev)
{
    char *devname = NULL;
    /*
     * Reserve space for the device name outside the net_device structure
     * so that if the name changes we know what it used to be.
     */
    devname = qdf_mem_malloc((strlen(dev->name) + 1) * sizeof(char));
    if (devname == NULL) {
        qdf_info(KERN_ERR "%s: no memory for VAP name!", __func__);
        return;
    }
    strlcpy(devname, dev->name, strlen(dev->name) + 1);
    ic->ic_proc = proc_mkdir(devname, NULL);
    if (ic->ic_proc) {
        int i = 0;
        for (i = 0; i < NUM_PROC_INFOS; ++i) {
            struct proc_dir_entry *entry;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
            entry = proc_create_data(pdev_proc_infos[i].name, 0644, ic->ic_proc,
                                    (const struct file_operations *) pdev_proc_infos[i].ops, (void*)ic);
#else
            entry = create_proc_entry(pdev_proc_infos[i].name,
                                       0644, ic->ic_proc);
#endif
            if (entry == NULL) {
                qdf_info(KERN_ERR "%s: Proc Entry creation failed for wifi%d!", __func__,i);
                qdf_mem_free(devname);
                return;
            }
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
            entry->data = ic;
            entry->proc_fops = pdev_proc_infos[i].ops;
#endif
        }
    }
    qdf_mem_free(devname);
}

void
ol_ath_pdev_proc_detach(struct ieee80211com *ic)
{
    if (ic->ic_proc) {
        int i;
        for (i = 0; i < NUM_PROC_INFOS; ++i)
            remove_proc_entry(pdev_proc_infos[i].name, ic->ic_proc);
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
        remove_proc_entry(ic->ic_proc->name, ic->ic_proc->parent);
#else
        proc_remove(ic->ic_proc);
#endif
        ic->ic_proc = NULL;
    }
}


#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
extern int osif_nss_ol_assign_ifnum(int radio_id, ol_ath_soc_softc_t *soc, bool is_2g);
#endif

#if OL_ATH_SMART_LOGGING

ssize_t smartlog_dump(struct device *dev,
                               struct device_attribute *attr, char *buf)
{
    struct net_device *net = to_net_dev(dev);
    struct ol_ath_softc_net80211 *scn = ath_netdev_priv(net);
    struct ieee80211com *ic = &scn->sc_ic;
    struct target_psoc_info *tgt_psoc_info;
    void *dbglog_handle = NULL;

    tgt_psoc_info = (struct target_psoc_info *)wlan_psoc_get_tgt_if_handle(
						scn->soc->psoc_obj);
    dbglog_handle = target_psoc_get_dbglog_hdl(tgt_psoc_info);

    if (ic->smart_logging == 0) {
        qdf_info("Smart logging not enabled / Target is not up\n");
        return 0;
    }
    if (dbglog_handle)
        fwdbg_smartlog_dump(dbglog_handle, dev, attr, buf);

    return 0;
}


#endif /* OL_ATH_SMART_LOGGING */

/* The code below is used to register a hw_caps file in sysfs */
static ssize_t wifi_hwcaps_show(struct device *dev,
                               struct device_attribute *attr, char *buf)
{
    struct net_device *net = to_net_dev(dev);
    struct ol_ath_softc_net80211 *scn = ath_netdev_priv(net);
    struct ieee80211com *ic = &scn->sc_ic;

    u_int64_t hw_caps = ic->ic_modecaps;

    strlcpy(buf, "802.11", strlen("802.11") + 1);
    if(hw_caps &
        1 << IEEE80211_MODE_11A)
        strlcat(buf, "a", strlen("a") + strlen(buf) + 1);
    if(hw_caps &
        1 << IEEE80211_MODE_11B)
        strlcat(buf, "b", strlen("b") + strlen(buf) + 1);
    if(hw_caps &
        1 << IEEE80211_MODE_11G)
        strlcat(buf, "g", strlen("g") +  strlen(buf) + 1);
    if(hw_caps &
        (1 << IEEE80211_MODE_11NA_HT20 |
         1 << IEEE80211_MODE_11NG_HT20 |
         1 << IEEE80211_MODE_11NA_HT40PLUS |
         1 << IEEE80211_MODE_11NA_HT40MINUS |
         1 << IEEE80211_MODE_11NG_HT40PLUS |
         1 << IEEE80211_MODE_11NG_HT40MINUS |
         1 << IEEE80211_MODE_11NG_HT40 |
         1 << IEEE80211_MODE_11NA_HT40))
         strlcat(buf, "n", strlen("n") + strlen(buf) + 1);
    if(hw_caps &
        (1 << IEEE80211_MODE_11AC_VHT20 |
         1 << IEEE80211_MODE_11AC_VHT40PLUS |
         1 << IEEE80211_MODE_11AC_VHT40MINUS |
         1 << IEEE80211_MODE_11AC_VHT40 |
         1 << IEEE80211_MODE_11AC_VHT80 |
         1 << IEEE80211_MODE_11AC_VHT160 |
         1 << IEEE80211_MODE_11AC_VHT80_80))
         strlcat(buf, "/ac", strlen("/ac" ) + strlen(buf) + 1);
    if(hw_caps &
        (1 << IEEE80211_MODE_11AXA_HE20 |
         1 << IEEE80211_MODE_11AXG_HE20 |
         1 << IEEE80211_MODE_11AXA_HE40PLUS |
         1 << IEEE80211_MODE_11AXA_HE40MINUS |
         1 << IEEE80211_MODE_11AXG_HE40PLUS |
         1 << IEEE80211_MODE_11AXG_HE40MINUS |
         1 << IEEE80211_MODE_11AXA_HE40 |
         1 << IEEE80211_MODE_11AXG_HE40 |
         1 << IEEE80211_MODE_11AXA_HE80 |
         1 << IEEE80211_MODE_11AXA_HE160 |
         1ULL << IEEE80211_MODE_11AXA_HE80_80))
         strlcat(buf, "/ax", strlen("/ax" ) + strlen(buf) + 1);
    return strlen(buf);
}
static DEVICE_ATTR(hwcaps, S_IRUGO, wifi_hwcaps_show, NULL);

/* Handler for sysfs entry hwmodes - returns all the hwmodes supported by the radio */
static ssize_t wifi_hwmodes_show(struct device *dev,
                               struct device_attribute *attr, char *buf)
{
    struct ol_ath_softc_net80211 *scn = NULL;
    struct ieee80211com *ic = NULL;
    u_int64_t hw_caps = 0;
    int len = 0;
    struct net_device *net = to_net_dev(dev);

    scn = ath_netdev_priv(net);
    if(!scn){
       return 0;
    }

    ic = &scn->sc_ic;
    if(!ic){
        return 0;
    }

    hw_caps = ic->ic_modecaps;

    if(hw_caps &
        1 << IEEE80211_MODE_11A)
        strlcat(buf, "11A ", strlen("11A ") + strlen(buf) + 1);
    if(hw_caps &
        1 << IEEE80211_MODE_11B)
        strlcat(buf, "11B ", strlen("11B ") + strlen(buf) + 1);
    if(hw_caps &
        1 << IEEE80211_MODE_11G)
        strlcat(buf, "11G ", strlen("11G ") + strlen(buf) + 1);
    if(hw_caps &
        1 << IEEE80211_MODE_FH)
        strlcat(buf, "FH ", strlen("FH ") + strlen(buf) + 1);
    if(hw_caps &
        1 << IEEE80211_MODE_TURBO_A)
        strlcat(buf, "TURBO_A ", strlen("TURBO_A ") + strlen(buf) + 1);
    if(hw_caps &
        1 << IEEE80211_MODE_TURBO_G)
        strlcat(buf, "TURBO_G ", strlen("TURBO_G ") + strlen(buf) + 1);
    if(hw_caps &
        1 << IEEE80211_MODE_11NA_HT20)
        strlcat(buf, "11NA_HT20 ", strlen("11NA_HT20 ") + strlen(buf) + 1);
    if(hw_caps &
        1 << IEEE80211_MODE_11NG_HT20)
        strlcat(buf, "11NG_HT20 ", strlen("11NG_HT20 ") + strlen(buf) + 1);
    if(hw_caps &
        1 << IEEE80211_MODE_11NA_HT40PLUS)
        strlcat(buf, "11NA_HT40PLUS ", strlen("11NA_HT40PLUS ") + strlen(buf) + 1);
    if(hw_caps &
        1 << IEEE80211_MODE_11NA_HT40MINUS)
        strlcat(buf, "11NA_HT40MINUS ", strlen("11NA_HT40MINUS ") + strlen(buf) + 1);
    if(hw_caps &
        1 << IEEE80211_MODE_11NG_HT40PLUS)
        strlcat(buf, "11NG_HT40PLUS ", strlen("11NG_HT40PLUS ") + strlen(buf) + 1);
    if(hw_caps &
        1 << IEEE80211_MODE_11NG_HT40MINUS)
        strlcat(buf, "11NG_HT40MINUS ", strlen("11NG_HT40MINUS ") + strlen(buf) + 1);
    if(hw_caps &
        1 << IEEE80211_MODE_11NG_HT40)
        strlcat(buf, "11NG_HT40 ", strlen("11NG_HT40 ") + strlen(buf) + 1);
    if(hw_caps &
        1 << IEEE80211_MODE_11NA_HT40)
        strlcat(buf, "11NA_HT40 ", strlen("11NA_HT40 ") + strlen(buf) + 1);
    if(hw_caps &
        1 << IEEE80211_MODE_11AC_VHT20)
        strlcat(buf, "11AC_VHT20 ", strlen("11AC_VHT20 ") + strlen(buf) + 1);
    if(hw_caps &
        1 << IEEE80211_MODE_11AC_VHT40PLUS)
        strlcat(buf, "11AC_VHT40PLUS ", strlen("11AC_VHT40PLUS ") + strlen(buf) + 1);
    if(hw_caps &
        1 << IEEE80211_MODE_11AC_VHT40MINUS)
        strlcat(buf, "11AC_VHT40MINUS ", strlen("11AC_VHT40MINUS ") + strlen(buf) + 1);
    if(hw_caps &
        1 << IEEE80211_MODE_11AC_VHT40)
        strlcat(buf, "11AC_VHT40 ", strlen("11AC_VHT40 ") + strlen(buf) + 1);
    if(hw_caps &
        1 << IEEE80211_MODE_11AC_VHT80)
        strlcat(buf, "11AC_VHT80 ", strlen("11AC_VHT80 ") + strlen(buf) + 1);
    if(hw_caps &
        1 << IEEE80211_MODE_11AC_VHT160)
        strlcat(buf, "11AC_VHT160 ", strlen("11AC_VHT160 ") + strlen(buf) + 1);
    if(hw_caps &
        1 << IEEE80211_MODE_11AC_VHT80_80)
        strlcat(buf, "11AC_VHT80_80 ", strlen("11AC_VHT80_80 ") + strlen(buf) + 1);
    if(hw_caps &
        1 << IEEE80211_MODE_11AXA_HE20)
        strlcat(buf, "11AXA_HE20 ", strlen("11AXA_HE20 ") + strlen(buf) + 1);
    if(hw_caps &
        1 << IEEE80211_MODE_11AXG_HE20)
        strlcat(buf, "11AXG_HE20 ", strlen("11AXG_HE20 ") + strlen(buf) + 1);
    if(hw_caps &
        1 << IEEE80211_MODE_11AXA_HE40PLUS)
        strlcat(buf, "11AXA_HE40PLUS ", strlen("11AXA_HE40PLUS ") + strlen(buf) + 1);
    if(hw_caps &
        1 << IEEE80211_MODE_11AXA_HE40MINUS)
        strlcat(buf, "11AXA_HE40MINUS ", strlen("11AXA_HE40MINUS ") + strlen(buf) + 1);
    if(hw_caps &
        1 << IEEE80211_MODE_11AXG_HE40PLUS)
        strlcat(buf, "11AXG_HE40PLUS ", strlen("11AXG_HE40PLUS ") + strlen(buf) + 1);
    if(hw_caps &
        1 << IEEE80211_MODE_11AXG_HE40MINUS)
        strlcat(buf, "11AXG_HE40MINUS ", strlen("11AXG_HE40MINUS ") + strlen(buf) + 1);
    if(hw_caps &
        1 << IEEE80211_MODE_11AXA_HE40)
        strlcat(buf, "11AXA_HE40 ", strlen("11AXA_HE40 ") + strlen(buf) + 1);
    if(hw_caps &
        1 << IEEE80211_MODE_11AXG_HE40)
        strlcat(buf, "11AXG_HE40 ", strlen("11AXG_HE40 ") + strlen(buf) + 1);
    if(hw_caps &
        1 << IEEE80211_MODE_11AXA_HE80)
        strlcat(buf, "11AXA_HE80 ", strlen("11AXA_HE80 ") + strlen(buf) + 1);
    if(hw_caps &
        1 << IEEE80211_MODE_11AXA_HE160)
        strlcat(buf, "11AXA_HE160 ", strlen("11AXA_HE160 ") + strlen(buf) + 1);
    if(hw_caps &
        1ULL << IEEE80211_MODE_11AXA_HE80_80)
        strlcat(buf, "11AXA_HE80_80 ", strlen("11AXA_HE80_80 ") + strlen(buf) + 1);

    len = strlen(buf);
    if(len > 0){
        buf[len - 1] = '\0';
    }

    return strlen(buf);
}
static DEVICE_ATTR(hwmodes, S_IRUGO, wifi_hwmodes_show, NULL);

/*Handler for sysfs entry 2g_maxchwidth - returns the maximum channel width supported by the device in 2.4GHz*/
static ssize_t wifi_2g_maxchwidth_show(struct device *dev,
                               struct device_attribute *attr, char *buf)
{
    struct ol_ath_softc_net80211 *scn = NULL;
    struct ieee80211com *ic = NULL;
    u_int64_t hw_caps = 0;
    struct net_device *net = to_net_dev(dev);

    scn = ath_netdev_priv(net);
    if(!scn){
       return 0;
    }

    ic = &scn->sc_ic;
    if(!ic){
        return 0;
    }

    hw_caps = ic->ic_modecaps;


    if (hw_caps &
        (1 << IEEE80211_MODE_11AXG_HE40MINUS |
         1 << IEEE80211_MODE_11AXG_HE40PLUS |
         1 << IEEE80211_MODE_11AXG_HE40 |
         1 << IEEE80211_MODE_11NG_HT40MINUS |
         1 << IEEE80211_MODE_11NG_HT40PLUS |
         1 << IEEE80211_MODE_11NG_HT40)) {
         strlcpy(buf, "40", strlen("40") + strlen(buf) + 1);
    }
    else if (hw_caps &
             (1 << IEEE80211_MODE_11AXG_HE20 |
              1 << IEEE80211_MODE_11NG_HT20)) {
        strlcpy(buf, "20", strlen("20") + strlen(buf) + 1);
    }

    /* NOTE: Only >=n chipsets are considered for now since productization will
     * involve only such chipsets. In the unlikely case where lower chipsets/crimped
     * phymodes are to be handled, it is a separate TODO and relevant enums need
     * to be considered.*/

    return strlen(buf);
}

static DEVICE_ATTR(2g_maxchwidth, S_IRUGO, wifi_2g_maxchwidth_show, NULL);

/*Handler for sysfs entry 5g_maxchwidth - returns the maximum channel width supported by the device in 5GHz*/
static ssize_t wifi_5g_maxchwidth_show(struct device *dev,
                               struct device_attribute *attr, char *buf)
{
    struct ol_ath_softc_net80211 *scn = NULL;
    struct ieee80211com *ic = NULL;
    u_int64_t hw_caps = 0;
    struct net_device *net = to_net_dev(dev);

    scn = ath_netdev_priv(net);
    if(!scn){
       return 0;
    }

    ic = &scn->sc_ic;
    if(!ic){
        return 0;
    }

    hw_caps = ic->ic_modecaps;

    if (hw_caps &
        (1 << IEEE80211_MODE_11AC_VHT160 |
         1ULL << IEEE80211_MODE_11AXA_HE160)) {
        strlcpy(buf, "160", sizeof("160"));
    }
    else if (hw_caps &
        (1 << IEEE80211_MODE_11AC_VHT80 |
         1 << IEEE80211_MODE_11AXA_HE80)) {
        strlcpy(buf, "80", sizeof("80"));
    }
    else if (hw_caps &
        (1 << IEEE80211_MODE_11AXA_HE40 |
         1 << IEEE80211_MODE_11AXA_HE40MINUS |
         1 << IEEE80211_MODE_11AXA_HE40PLUS |
         1 << IEEE80211_MODE_11AC_VHT40 |
         1 << IEEE80211_MODE_11AC_VHT40MINUS |
         1 << IEEE80211_MODE_11AC_VHT40PLUS |
         1 << IEEE80211_MODE_11NA_HT40 |
         1 << IEEE80211_MODE_11NA_HT40MINUS |
         1 << IEEE80211_MODE_11NA_HT40PLUS)) {
        strlcpy(buf, "40", sizeof("40"));
    }
    else if (hw_caps &
        (1 << IEEE80211_MODE_11AXA_HE20 |
         1 << IEEE80211_MODE_11AC_VHT20 |
         1 << IEEE80211_MODE_11NA_HT20)) {
            strlcpy(buf, "20", sizeof("20"));
    }

    /* NOTE: Only >=n chipsets are considered for now since productization will
     * involve only such chipsets. In the unlikely case where lower chipsets/crimped
     * phymodes are to be handled, it is a separate TODO and relevant enums need
     * to be considered.*/

    return strlen(buf);
}

static DEVICE_ATTR(5g_maxchwidth, S_IRUGO, wifi_5g_maxchwidth_show, NULL);

/*Handler for sysfs entry is_offload - returns if the radio is offload or not */
static ssize_t wifi_is_offload_show(struct device *dev,
                               struct device_attribute *attr, char *buf)
{
    strlcpy(buf, "1", strlen("1") + 1);
    return strlen(buf);
}

static DEVICE_ATTR(is_offload, S_IRUGO, wifi_is_offload_show, NULL);

/*Handler for sysfs entry cfg80211 xmlfile
  returns xml file that is used for vendor commands */
static ssize_t wifi_cfg80211_xmlfile_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    strlcpy(buf, "qcacommands_ol_radio.xml", sizeof("qcacommands_ol_radio.xml"));
    return strlen(buf);
}

static DEVICE_ATTR(cfg80211_xmlfile, S_IRUGO, wifi_cfg80211_xmlfile_show, NULL);


#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
/* Handler for sysfs entry nssoffload -
 * returns whether this radio is capable of being offloaded to NSS.
 */
static ssize_t wifi_nssoffload_show(struct device *dev,
                               struct device_attribute *attr, char *buf)
{
    struct net_device *net = to_net_dev(dev);
    struct ol_ath_softc_net80211 *scn = ath_netdev_priv(net);

    uint32_t target_type = lmac_get_tgt_type(scn->soc->psoc_obj);

    if (nss_cmn_get_nss_enabled() != true) {
        strlcpy(buf, "NA", strlen("NA") + 1);
        return strlen(buf);
    }

    switch (target_type) {
        case TARGET_TYPE_QCA9984:
	case TARGET_TYPE_AR900B:
	case TARGET_TYPE_QCA8074:
	case TARGET_TYPE_QCA8074V2:
	case TARGET_TYPE_QCA6018:
            strlcpy(buf, "capable", strlen("capable") + 1);
        break;
	default:
            strlcpy(buf, "NA", strlen("NA") +1);
	break;
    }
    return strlen(buf);
}
static DEVICE_ATTR(nssoffload, S_IRUGO, wifi_nssoffload_show, NULL);
#endif

/***** ciphercaps for OL radio *****/

static ssize_t wifi_ol_ciphercaps_show(struct device *dev,struct device_attribute *attr, char *buf)
{
    struct net_device *net = to_net_dev(dev);
    struct ol_ath_softc_net80211 *scn = ath_netdev_priv(net);
    struct ieee80211com *ic = &scn->sc_ic;
    ol_ath_soc_softc_t *soc = scn->soc;
    u_int16_t ciphercaps = ic->ic_cipher_caps;

    if ( !buf )
        return 0 ;

    buf[0]='\0';

    if ( ciphercaps & 1<<IEEE80211_CIPHER_WEP ) {
        strlcat(buf, "wep40,", strlen("wep40,") + strlen(buf) + 1);
        strlcat(buf, "wep104,", strlen("wep104,") + strlen(buf) + 1);
        if (lmac_is_target_ar900b(soc->psoc_obj))
            strlcat(buf, "wep128,", strlen("wep128,") + strlen(buf) + 1);
    }
    if ( ciphercaps & 1<<IEEE80211_CIPHER_TKIP )
        strlcat(buf, "tkip,", strlen("tkip,") + strlen(buf) + 1);
    if ( ciphercaps & 1<<IEEE80211_CIPHER_AES_OCB )
        strlcat(buf, "aes-ocb,", strlen("aes-ocb,") + strlen(buf) + 1);
    if ( ciphercaps & 1<<IEEE80211_CIPHER_AES_CCM )
        strlcat(buf, "aes-ccmp-128,", strlen("aes-ccmp-128,") + strlen(buf) + 1);
    if ( ciphercaps & 1<<IEEE80211_CIPHER_AES_CCM_256 )
        strlcat(buf, "aes-ccmp-256,", strlen("aes-ccmp-256,") + strlen(buf) + 1);
    if ( ciphercaps & 1<<IEEE80211_CIPHER_AES_GCM )
        strlcat(buf, "aes-gcmp-128,", strlen("aes-gcmp-128,") + strlen(buf) + 1);
    if ( ciphercaps & 1<<IEEE80211_CIPHER_AES_GCM_256 )
        strlcat(buf, "aes-gcmp-256,", strlen("aes-gcmp-256,") + strlen(buf) + 1);
    if ( ciphercaps & 1<<IEEE80211_CIPHER_CKIP )
        strlcat(buf, "ckip,", strlen("ckip,") + strlen(buf) + 1);
    if ( ciphercaps & 1<<IEEE80211_CIPHER_WAPI )
        strlcat(buf, "wapi,", strlen("wapi,") + strlen(buf) + 1);
    if ( ciphercaps & 1<<IEEE80211_CIPHER_AES_CMAC )
        strlcat(buf, "aes-cmac-128,", strlen("aes-cmac-128,") + strlen(buf) + 1);
    if ( ciphercaps & 1<<IEEE80211_CIPHER_AES_CMAC_256  )
        strlcat(buf, "aes-cmac-256,", strlen("aes-cmac-256,") + strlen(buf) + 1);
    if ( ciphercaps & 1<<IEEE80211_CIPHER_AES_GMAC )
        strlcat(buf, "aes-gmac-128,", strlen("aes-gmac-128,") + strlen(buf) + 1);
    if ( ciphercaps & 1<<IEEE80211_CIPHER_AES_GMAC_256 )
        strlcat(buf, "aes-gmac-256,", strlen("aes-gmac-256,") + strlen(buf) + 1);
    if ( ciphercaps & 1<<IEEE80211_CIPHER_NONE )
        strlcat(buf, "none", strlen("none") + strlen(buf) + 1);
    return strlen(buf);
}

static DEVICE_ATTR(ciphercaps, S_IRUGO, wifi_ol_ciphercaps_show, NULL);

#if defined(HIF_CONFIG_SLUB_DEBUG_ON) || defined(HIF_CE_DEBUG_DATA_BUF)

/*
 * hif_store_desc_trace_buf_index() -
 * API to get the CE id and CE debug storage buffer index
 *
 * @dev: network device
 * @attr: sysfs attribute
 * @buf: data got from the user
 *
 * Return total length
 */
ssize_t hif_store_desc_trace_buf_index(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t size)
{
    struct net_device *net = to_net_dev(dev);
    struct ol_ath_softc_net80211 *scn =  ath_netdev_priv(net);
    struct hif_softc *hif_scn;
    struct hif_opaque_softc *hif_hdl;

    ol_ath_soc_softc_t *soc = scn->soc;
    hif_hdl = lmac_get_ol_hif_hdl(soc->psoc_obj);

    hif_scn = HIF_GET_SOFTC(hif_hdl);

    if(!buf)
        return -EINVAL;

    size = hif_input_desc_trace_buf_index(hif_scn, buf, size);

    return size;
}


/*
 * hif_dump_desc_trace_buf() -
 * API to dump the CE debug storage buffer of all CE rings
 *
 * @dev: network device
 * @attr: sysfs attribute
 * @buf: buffer to copy the data.
 *
 * Prints all the CE debug data to the console.
 *
 * Return total length copied
 */
ssize_t hif_dump_desc_trace_buf(struct device *dev,
	struct device_attribute *attr, char *buf)
{
    struct net_device *net = to_net_dev(dev);
    struct ol_ath_softc_net80211 *scn =  ath_netdev_priv(net);
    struct hif_softc *hif_scn;
    struct hif_opaque_softc *hif_hdl;

    ol_ath_soc_softc_t *soc = scn->soc;
    hif_hdl = lmac_get_ol_hif_hdl(soc->psoc_obj);

    hif_scn = HIF_GET_SOFTC(hif_hdl);

    return hif_dump_desc_event(hif_scn, buf);

}
static DEVICE_ATTR(celogs_dump, S_IRUGO | S_IWUSR, hif_dump_desc_trace_buf, hif_store_desc_trace_buf_index);
#endif /* HIF_CONFIG_SLUB_DEBUG_ON || HIF_CE_DEBUG_DATA_BUF */

#ifdef HIF_CE_DEBUG_DATA_BUF
/*
 * hif_ce_enable_desc_hist() -
 * API to enable recording the CE desc history
 *
 * @dev: network device
 * @attr: sysfs attribute
 * @buf: buffer to copy the data.
 *
 * Starts recording the ce desc history
 *
 * Return total length copied
 */
ssize_t hif_ce_enable_desc_hist(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t size)
{
    struct net_device *net = to_net_dev(dev);
    struct ol_ath_softc_net80211 *scn =  ath_netdev_priv(net);
    struct hif_softc *hif_scn;
    struct hif_opaque_softc *hif_hdl;

    ol_ath_soc_softc_t *soc = scn->soc;
    hif_hdl = lmac_get_ol_hif_hdl(soc->psoc_obj);

    hif_scn = HIF_GET_SOFTC(hif_hdl);

    if(!buf)
        return -EINVAL;

    size = hif_ce_en_desc_hist(hif_scn, buf, size);

    return size;
}

/*
 * hif_show_ce_enable_desc_hist() -
 * API to display value of data_enable
 *
 * @dev: network device
 * @attr: sysfs attribute
 * @buf: buffer to copy the data.
 *
 * Return total length copied
 */
ssize_t hif_show_ce_enable_desc_hist(struct device *dev,
	struct device_attribute *attr, char *buf)
{
    ssize_t len = 0;
    struct net_device *net = to_net_dev(dev);
    struct ol_ath_softc_net80211 *scn =  ath_netdev_priv(net);
    struct hif_softc *hif_scn;
    struct hif_opaque_softc *hif_hdl;

    ol_ath_soc_softc_t *soc = scn->soc;
    hif_hdl = lmac_get_ol_hif_hdl(soc->psoc_obj);
    hif_scn = HIF_GET_SOFTC(hif_hdl);

    len = hif_disp_ce_enable_desc_data_hist(hif_scn, buf);

    return len;
}

static DEVICE_ATTR(celogs_data_en, S_IRUGO | S_IWUSR, hif_show_ce_enable_desc_hist, hif_ce_enable_desc_hist);
#endif /* HIF_CE_DEBUG_DATA_BUF */

#ifdef OL_ATH_SMART_LOGGING
static DEVICE_ATTR(smartlogs_dump, S_IRUGO, smartlog_dump, NULL);
#endif /* OL_ATH_SMART_LOGGING */

static struct attribute *wifi_device_attrs[] = {
    &dev_attr_hwcaps.attr,
    &dev_attr_hwmodes.attr,       /*sysfs entry for displaying all the hwmodes supported by the radio*/
    &dev_attr_5g_maxchwidth.attr, /*sysfs entry for displaying the max channel width supported in 5Ghz*/
    &dev_attr_2g_maxchwidth.attr, /*sysfs entry for displaying the max channel width supported in 2.4Ghz*/
    &dev_attr_cfg80211_xmlfile.attr, /*sysfs entry for displaying the max channel width supported in 2.4Ghz*/
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    &dev_attr_nssoffload.attr,   /*sysfs entry for displaying whether nss offload is supported or not*/
#endif
    &dev_attr_ciphercaps.attr,  /*sysfs entry for displaying the supported ciphercap's in OL Chips*/
    &dev_attr_is_offload.attr,  /*sysfs entry for displaying whether the radio is offload or not*/
#if defined(HIF_CONFIG_SLUB_DEBUG_ON) || defined(HIF_CE_DEBUG_DATA_BUF)
    &dev_attr_celogs_dump.attr,  /*sysfs entry for dumping all the Copy engine debug logs*/
    &dev_attr_celogs_data_en.attr,  /*sysfs entry for Enabline Data recording (64 bytes) in CE debug logs*/
#endif
#ifdef OL_ATH_SMART_LOGGING
    &dev_attr_smartlogs_dump.attr,  /*sysfs entry for dumping all the smart logs*/
#endif /* OL_ATH_SMART_LOGGING */
    NULL
};


static struct attribute_group wifi_attr_group = {
       .attrs  = wifi_device_attrs,
};


static DEVICE_ATTR(mode, S_IRUGO | S_IWUSR, wifi_thermal_mode_show, wifi_thermal_mode_store);
static DEVICE_ATTR(temp, S_IRUGO, wifi_thermal_temp_show, NULL);
static DEVICE_ATTR(thlvl, S_IRUGO | S_IWUSR, wifi_thermal_thlvl_show, wifi_thermal_thlvl_store);
static DEVICE_ATTR(dc, S_IRUGO | S_IWUSR, wifi_thermal_dutycycle_show, wifi_thermal_dutycycle_store);
static DEVICE_ATTR(off, S_IRUGO | S_IWUSR, wifi_thermal_offpercent_show, wifi_thermal_offpercent_store);

static struct attribute *wifi_thermal_attrs[] = {
    &dev_attr_mode.attr,
    &dev_attr_temp.attr,
    &dev_attr_thlvl.attr,
    &dev_attr_dc.attr,
    &dev_attr_off.attr,
    NULL
};

static struct attribute_group wifi_thermal_group = {
       .attrs  = wifi_thermal_attrs,
       .name = "thermal",
};

/*
 * Register Thermal Mitigation Functionality
 */

int32_t ol_ath_thermal_mitigation_attach (struct ol_ath_softc_net80211 *scn,
                                      struct net_device *dev)
{
    int retval = TH_SUCCESS;
#if THERMAL_DEBUG
    //scn->thermal_param.tt_support = TH_TRUE;
#endif
    scn->thermal_param.th_cfg.log_lvl = TH_DEBUG_LVL0;

    if (get_tt_supported(scn)) {
        if (qdf_vfs_set_file_attributes((struct qdf_dev_obj *)&dev->dev.kobj,
                                        (struct qdf_vfs_attr *)
                                        &wifi_thermal_group)) {
            TH_DEBUG_PRINT(TH_DEBUG_LVL0, scn, "%s: unable to register wifi_thermal_group for %s\n", __func__, dev->name);
            return TH_FAIL;
        }
        retval = __ol_ath_thermal_mitigation_attach(scn);
        if (retval) {
            wlan_psoc_nif_fw_ext_cap_clear(scn->soc->psoc_obj,
                                  WLAN_SOC_CEXT_TT_SUPPORT);
            qdf_vfs_clear_file_attributes((struct qdf_dev_obj *)&dev->dev.kobj,
                                          (struct qdf_vfs_attr *)
                                          &wifi_thermal_group);
            TH_DEBUG_PRINT(TH_DEBUG_LVL0, scn, "%s: unable to initialize TT\n", __func__);
        }
    } else {
        TH_DEBUG_PRINT(TH_DEBUG_LVL0, scn, "%s: TT not supported in FW\n", __func__);
    }

    return retval;
}
EXPORT_SYMBOL(ol_ath_thermal_mitigation_attach);

int32_t ol_ath_thermal_mitigation_detach(struct ol_ath_softc_net80211 *scn,
                                      struct net_device *dev)
{
    int retval = 0;

    if (get_tt_supported(scn)) {
        retval = __ol_ath_thermal_mitigation_detach(scn);
        qdf_vfs_clear_file_attributes((struct qdf_dev_obj *)&dev->dev.kobj,
                                      (struct qdf_vfs_attr *)
                                      &wifi_thermal_group);
    }

    return retval;
}
EXPORT_SYMBOL(ol_ath_thermal_mitigation_detach);

/* SOC device attributes */
static ssize_t wifi_soc_hw_modes_show(struct device *dev,
                               struct device_attribute *attr, char *buf)
{
    struct net_device *net = to_net_dev(dev);
    ol_ath_soc_softc_t *soc = ath_netdev_priv(net);
    struct target_psoc_info *tgt_psoc_info;
    struct target_supported_modes *supp_modes;
    char *hw_modes[] = {"SINGLE:", "DBS:", "SBS_PASSIVE:", "SBS:", "DBS_SBS:",
                        "DBS_OR_SBS:"};
    uint32_t hw_modes_len = sizeof(hw_modes)/sizeof(char*);

    tgt_psoc_info = (struct target_psoc_info *)
                     wlan_psoc_get_tgt_if_handle(soc->psoc_obj);
    if (tgt_psoc_info == NULL) {
        qdf_info("%s: target_psoc_info is null ", __func__);
        return 0;
    }

    if ( !buf )
        return 0 ;

    buf[0]='\0';


    supp_modes = target_psoc_get_supported_hw_modes(tgt_psoc_info);
    if (supp_modes) {
        uint8_t i;
        for (i = 0; i < supp_modes->num_modes; i++) {
            if (supp_modes->hw_mode_ids[i] < hw_modes_len)
                strlcat(buf, hw_modes[supp_modes->hw_mode_ids[i]],
                        (strlen(hw_modes[supp_modes->hw_mode_ids[i]]) +
                         strlen(buf) + 1));
        }
    }

    return strlen(buf);
}
static DEVICE_ATTR(hw_modes, S_IRUGO, wifi_soc_hw_modes_show, NULL);

static struct attribute *wifi_soc_attrs[] = {
    &dev_attr_hw_modes.attr,
    NULL
};

static struct attribute_group wifi_soc_group = {
       .attrs  = wifi_soc_attrs,
};

#if 0
/*
 * Merge multicast addresses from all vap's to form the
 * hardware filter.  Ideally we should only inspect our
 * own list and the 802.11 layer would merge for us but
 * that's a bit difficult so for now we put the onus on
 * the driver.
 */
void
ath_mcast_merge(ieee80211_handle_t ieee, u_int32_t mfilt[2])
{
    struct ieee80211com *ic = NET80211_HANDLE(ieee);
    struct ieee80211vap *vap;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 34)
    struct netdev_hw_addr *ha;
#else
    struct dev_mc_list *mc;
#endif
    u_int32_t val;
    u_int8_t pos;

    mfilt[0] = mfilt[1] = 0;
    /* XXX locking */
    TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
        struct net_device *dev = ((osif_dev *)vap->iv_ifp)->netdev;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 34)
        netdev_for_each_mc_addr(ha, dev) {
            /* calculate XOR of eight 6-bit values */
            val = LE_READ_4(ha->addr + 0);
            pos = (val >> 18) ^ (val >> 12) ^ (val >> 6) ^ val;
            val = LE_READ_4(ha->addr + 3);
            pos ^= (val >> 18) ^ (val >> 12) ^ (val >> 6) ^ val;
            pos &= 0x3f;
            mfilt[pos / 32] |= (1 << (pos % 32));
        }
#else
        for (mc = dev->mc_list; mc; mc = mc->next) {
            /* calculate XOR of eight 6bit values */
            val = LE_READ_4(mc->dmi_addr + 0);
            pos = (val >> 18) ^ (val >> 12) ^ (val >> 6) ^ val;
            val = LE_READ_4(mc->dmi_addr + 3);
            pos ^= (val >> 18) ^ (val >> 12) ^ (val >> 6) ^ val;
            pos &= 0x3f;
            mfilt[pos / 32] |= (1 << (pos % 32));
        }
#endif
    }
}
#endif

#define VDEV_UP_STATE  1
#define VDEV_DOWN_STATE 0
#define MAX_ID_PER_WORD 32

static void ath_vdev_netdev_state_change(struct wlan_objmgr_pdev *pdev, void *obj, void *args)
{
    osif_dev  *osifp;
    struct net_device *netdev;
    u_int8_t myaddr[IEEE80211_ADDR_LEN];
    u_int8_t id = 0;
    u_int8_t vdev_id = 0;
    u_int8_t ix = 0;
    struct ieee80211com *ic;
    struct vdev_osif_priv *vdev_osifp = NULL;
    uint8_t flags = *((uint8_t *)args);
    struct wlan_objmgr_vdev *vdev = (struct wlan_objmgr_vdev *)obj;

    /* This API is invoked for all vdevs of the radio */
    if(vdev != NULL) {
        ic = wlan_vdev_get_ic(vdev);
        if (!ic) {
            return;
        }
        vdev_osifp = wlan_vdev_get_ospriv(vdev);
        if (!vdev_osifp) {
            return;
        }
        osifp = (osif_dev *)vdev_osifp->legacy_osif_priv;
        if (osifp == NULL) {
            return;
        }
        netdev = osifp->netdev;

        WLAN_ADDR_COPY(myaddr, wlan_vdev_mlme_get_macaddr(vdev));
	vdev_id = wlan_vdev_get_id(vdev);
        id = vdev_id;
	if(vdev_id >= MAX_ID_PER_WORD) {
            id -= MAX_ID_PER_WORD;
            ix = 1;
	}
         /* open the vdev's netdev */
        if(flags == VDEV_UP_STATE) {
	   /* Brings up the vdev */
           if( ic->id_mask_vap_downed[ix] & ( 1 << id ) ) {
               dev_change_flags(netdev, netdev->flags | ( IFF_UP ));
               ic->id_mask_vap_downed[ix] &= (~( 1 << id ));
           }
        }
        else if(flags == VDEV_DOWN_STATE) {  /* stop the vdev's netdev */
	   /* Brings down the vdev */
           if (IS_IFUP(netdev)) {
               dev_change_flags(netdev, netdev->flags & ( ~IFF_UP ));
               ic->id_mask_vap_downed[ix] |= ( 1 << id);
           }
        }
   }
}

int
ath_netdev_open(struct net_device *dev)
{
    struct ol_ath_softc_net80211 *scn = ath_netdev_priv(dev);
    ol_ath_soc_softc_t *soc = scn->soc;
    int ol_ath_ret;
    u_int8_t flags;
    uint32_t target_type;
    struct pdev_op_args oper;

#ifdef ATH_BUS_PM
    if (scn->sc_osdev->isDeviceAsleep)
        return -EPERM;
#endif /* ATH_BUS_PM */

    target_type = lmac_get_tgt_type(scn->soc->psoc_obj);
    /* recover VAPs if recovery in progress */
    if ((soc->recovery_enable == RECOVERY_ENABLE_WAIT) && (soc->recovery_in_progress == 1)) {
        int recovery_enable;
        recovery_enable = soc->recovery_enable;
#ifdef ATH_AHB
        if ((target_type == TARGET_TYPE_IPQ4019) ||
			(target_type == TARGET_TYPE_QCA8074) ||
			(target_type == TARGET_TYPE_QCA8074V2) ||
			(target_type == TARGET_TYPE_QCA6018)) {
            ath_ahb_recover(soc);
        } else
#endif
        {
            ath_pci_recover(soc);
        }
        /*Send event to user space */
        oper.type = PDEV_ITER_POWERUP;
        wlan_objmgr_iterate_obj_list(soc->psoc_obj, WLAN_PDEV_OP,
                   wlan_pdev_operation, &oper, 0, WLAN_MLME_NB_ID);

        soc->recovery_in_progress = 0;
        /* The recovery process resets recovery_enable flag. restore it here */
        soc->recovery_enable = recovery_enable;
        soc->target_status = OL_TRGET_STATUS_CONNECTED;
    }

    ol_ath_ret = ol_ath_resume(scn);
    if(ol_ath_ret == 0){
        dev->flags |= IFF_UP | IFF_RUNNING;      /* we are ready to go */
        /* flag to indicate bring up */
        flags = VDEV_UP_STATE;
        /*  If physical radio interface wifiX is shutdown,all virtual interfaces(athX) should gets shutdown and
            all these downed virtual interfaces should gets up when physical radio interface(wifiX) is up.Refer EV 116786.
         */
        wlan_objmgr_pdev_iterate_obj_list(scn->sc_pdev, WLAN_VDEV_OP,
                                    ath_vdev_netdev_state_change, &flags, 1,
                                    WLAN_MLME_NB_ID);
    }
    return ol_ath_ret;
}

int
ath_netdev_stop(struct net_device *dev)
{
    struct ol_ath_softc_net80211 *scn = ath_netdev_priv(dev);
    u_int8_t flags = VDEV_DOWN_STATE; /* flag to indicate bring up */


    /*  If physical radio interface wifiX is shutdown,all virtual interfaces(athX) should gets shutdown and
        all these downed virtual interfaces should gets up when physical radio interface(wifiX) is up.Refer EV 116786.
     */
    wlan_objmgr_pdev_iterate_obj_list(scn->sc_pdev, WLAN_VDEV_OP,
                                     ath_vdev_netdev_state_change, &flags, 1,
                                     WLAN_MLME_NB_ID);
    dev->flags &= ~IFF_RUNNING;
    return ol_ath_suspend(scn);
}

#ifdef EPPING_TEST
//#define EPPING_DEBUG 1
#ifdef EPPING_DEBUG
#define EPPING_PRINTF(...) qdf_info(__VA_ARGS__)
#else
#define EPPING_PRINTF(...)
#endif
static inline int
__ath_epping_data_tx(struct sk_buff *skb, struct net_device *dev)
{
    struct ol_ath_softc_net80211 *scn = ath_netdev_priv(dev);
    EPPING_HEADER *eppingHdr = A_NETBUF_DATA(skb);
    HTC_ENDPOINT_ID eid = ENDPOINT_UNUSED;
    struct cookie * cookie = NULL;
    A_UINT8 ac = 0;

    /* allocate resource for this packet */
    qdf_spin_lock_bh(&scn->data_lock);
    cookie = ol_alloc_cookie(scn->soc);
    qdf_spin_unlock_bh(&scn->data_lock);

    /* no resource */
    if (cookie == NULL)
        return -1;

    /*
     * a quirk of linux, the payload of the frame is 32-bit aligned and thus
     * the addition of the HTC header will mis-align the start of the HTC
     * frame, so we add some padding which will be stripped off in the target
     */
    if (EPPING_ALIGNMENT_PAD > 0) {
        A_NETBUF_PUSH(skb, EPPING_ALIGNMENT_PAD);
    }

    /* prepare ep/HTC information */
    ac = eppingHdr->StreamNo_h;
    eid = scn->EppingEndpoint[ac];
    SET_HTC_PACKET_INFO_TX(&cookie->HtcPkt,
         cookie, A_NETBUF_DATA(skb), A_NETBUF_LEN(skb), eid, 0);
    SET_HTC_PACKET_NET_BUF_CONTEXT(&cookie->HtcPkt, skb);

    /* send the packet */
    htc_send_pkt(scn->htc_handle, &cookie->HtcPkt);

    return 0;
}

static void
epping_timer_expire(unsigned long data)
{
    struct net_device *dev = (struct net_device *) data;
    struct ol_ath_softc_net80211 *scn = ath_netdev_priv(dev);
    struct sk_buff *nodrop_skb;

    EPPING_PRINTF("%s: queue len: %d\n", __func__,
            skb_queue_len(&scn->epping_nodrop_queue));

    if (!skb_queue_len(&scn->epping_nodrop_queue)) {
        /* nodrop queue is empty so no need to arm timer */
        scn->epping_timer_running = 0;
        return;
    }

    /* try to flush nodrop queue */
    while ((nodrop_skb = skb_dequeue(&scn->epping_nodrop_queue))) {
        if (__ath_epping_data_tx(nodrop_skb, dev)) {
            EPPING_PRINTF("nodrop: %pK xmit fail in timer\n", nodrop_skb);
            /* fail to xmit so put the nodrop packet to the nodrop queue */
            skb_queue_head(&scn->epping_nodrop_queue, nodrop_skb);
            break;
        } else {
            EPPING_PRINTF("nodrop: %pK xmit ok in timer\n", nodrop_skb);
        }
    }

    /* if nodrop queue is not empty, continue to arm timer */
    if (nodrop_skb) {
        scn->epping_timer_running = 1;
        qdf_timer_mod(&scn->epping_timer,
                      jiffies_to_msecs(HZ / 10));
    } else {
        scn->epping_timer_running = 0;
    }
}

static int
ath_epping_data_tx(struct sk_buff *skb, struct net_device *dev)
{
    struct ol_ath_softc_net80211 *scn = ath_netdev_priv(dev);
    struct sk_buff *nodrop_skb;
    EPPING_HEADER *eppingHdr;
    A_UINT8 ac = 0;

    if (!eppingtest) {
        goto pkt_invalid;
    }

    eppingHdr = A_NETBUF_DATA(skb);

    if (!IS_EPPING_PACKET(eppingHdr)) {
         AR_DEBUG_PRINTF(ATH_DEBUG, ("not endpoint ping packets in %s\n",
                 __FUNCTION__));
        goto pkt_invalid;
    }

    /* the stream ID is mapped to an access class */
    ac = eppingHdr->StreamNo_h;
    if (ac != 0 && ac != 1) {
        qdf_info("ac %d is not mapped to mboxping service id = %d\n",
             ac, eppingtest);
        goto pkt_invalid;
    }

    /*
     * some EPPING packets cannot be dropped no matter what access class
     * it was sent on. A special care has been taken:
     * 1. when there is no TX resource, queue the control packets to
     *    a special queue
     * 2. when there is TX resource, send the queued control packets first
     *    and then other packets
     * 3. a timer launches to check if there is queued control packets and
     *    flush them
     */

    /* check the nodrop queue first */
    while ((nodrop_skb = skb_dequeue(&scn->epping_nodrop_queue))) {
        if (__ath_epping_data_tx(nodrop_skb, dev)) {
            EPPING_PRINTF("nodrop: %pK xmit fail\n", nodrop_skb);
            /* fail to xmit so put the nodrop packet to the nodrop queue */
            skb_queue_head(&scn->epping_nodrop_queue, nodrop_skb);
            /* no cookie so free the current skb */
            goto tx_fail;
        } else {
            EPPING_PRINTF("nodrop: %pK xmit ok\n", nodrop_skb);
        }
    }

    /* send the original packet */
    if (__ath_epping_data_tx(skb, dev))
        goto tx_fail;

    return 0;

tx_fail:
    if (!IS_EPING_PACKET_NO_DROP(eppingHdr)) {
pkt_invalid:
        /* no packet to send, cleanup */
        A_NETBUF_FREE(skb);
        return -ENOMEM;
    } else {
        EPPING_PRINTF("nodrop: %pK queued\n", skb);
        skb_queue_tail(&scn->epping_nodrop_queue, skb);
        if (!scn->epping_timer_running) {
            scn->epping_timer_running = 1;
            qdf_timer_mod(&scn->epping_timer,
                          jiffies_to_msecs(HZ / 10));
        }
    }

    return 0;
}
#endif

static int
ath_netdev_hardstart(struct sk_buff *skb, struct net_device *dev)
{
#ifdef EPPING_TEST
    return ath_epping_data_tx(skb, dev);
#else
    return 0;
#endif
}

static void
ath_netdev_tx_timeout(struct net_device *dev)
{
#if 0
    struct ol_ath_softc_net80211 *scn = ath_netdev_priv(dev);

    DPRINTF(scn, ATH_DEBUG_WATCHDOG, "%s: %sRUNNING\n",
            __func__, (dev->flags & IFF_RUNNING) ? "" : "!");

	if (dev->flags & IFF_RUNNING) {
        scn->sc_ops->reset_start(scn->sc_dev, 0, 0, 0);
        scn->sc_ops->reset(scn->sc_dev);
        scn->sc_ops->reset_end(scn->sc_dev, 0);
	}
#endif
}

static int
ath_netdev_set_macaddr(struct net_device *dev, void *addr)
{
    struct ol_ath_softc_net80211 *scn = ath_netdev_priv(dev);
    struct ieee80211com *ic = &scn->sc_ic;
    struct sockaddr *mac = addr;

    if (netif_running(dev)) {
#if 0
        DPRINTF(scn, ATH_DEBUG_ANY,
            "%s: cannot set address; device running\n", __func__);
#endif
        return -EBUSY;
    }
#if 0
    DPRINTF(scn, ATH_DEBUG_ANY, "%s: %.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n",
        __func__,
        mac->sa_data[0], mac->sa_data[1], mac->sa_data[2],
        mac->sa_data[3], mac->sa_data[4], mac->sa_data[5]);
#endif

    /* XXX not right for multiple vap's */
    IEEE80211_ADDR_COPY(ic->ic_myaddr, mac->sa_data);
    IEEE80211_ADDR_COPY(ic->ic_my_hwaddr, mac->sa_data);
    /* TODO whether it needs to be done for PDEV or PSCO */
    wlan_pdev_set_hw_macaddr(scn->sc_pdev, mac->sa_data);
    IEEE80211_ADDR_COPY(dev->dev_addr, mac->sa_data);
    scn->sc_ic.ic_set_macaddr(&scn->sc_ic, dev->dev_addr);
    return 0;
}

static void
ath_netdev_set_mcast_list(struct net_device *dev)
{
#if 0
    struct ol_ath_softc_net80211 *scn = ath_netdev_priv(dev);
    scn->sc_ops->mc_upload(scn->sc_dev);
#endif
}

static int
ath_change_mtu(struct net_device *dev, int mtu)
{
    if (!(ATH_MIN_MTU < mtu && mtu <= ATH_MAX_MTU)) {
#if 0
        DPRINTF((struct ol_ath_softc_net80211 *) ath_netdev_priv(dev),
            ATH_DEBUG_ANY, "%s: invalid %d, min %u, max %u\n",
            __func__, mtu, ATH_MIN_MTU, ATH_MAX_MTU);
#endif
        return -EINVAL;
    }
#if 0
    DPRINTF((struct ol_ath_softc_net80211 *) ath_netdev_priv(dev), ATH_DEBUG_ANY,
        "%s: %d\n", __func__, mtu);
#endif

    dev->mtu = mtu;
    return 0;
}

int ath_hal_getdiagstate(struct ieee80211com* ic, u_int id, void* indata, u_int32_t insize, void* outdata, u_int32_t* outsize)
{
    qdf_info("SPECTRAL : NOT IMPLEMENTED YET %s : %d\n", __func__, __LINE__);
    return 0;
}

static int
ath_ioctl_diag(struct ol_ath_softc_net80211 *scn, struct ath_diag *ad)
{

    struct ieee80211com* ic = &scn->sc_ic;
    void *indata    = NULL;
    void *outdata   = NULL;

    int error = 0;

    u_int id= ad->ad_id & ATH_DIAG_ID;
    u_int32_t insize    = ad->ad_in_size;
    u_int32_t outsize   = ad->ad_out_size;

    if (ad->ad_id & ATH_DIAG_IN) {
        /*
         * Copy in data.
         */
        indata = OS_MALLOC(scn->sc_osdev, insize, GFP_KERNEL);
        if (indata == NULL) {
            error = -ENOMEM;
            goto bad;
        }
        if (__xcopy_from_user(indata, ad->ad_in_data, insize)) {
            error = -EFAULT;
            goto bad;
        }
    }
    if (ad->ad_id & ATH_DIAG_DYN) {
        /*
         * Allocate a buffer for the results (otherwise the HAL
         * returns a pointer to a buffer where we can read the
         * results).  Note that we depend on the HAL leaving this
         * pointer for us to use below in reclaiming the buffer;
         * may want to be more defensive.
         */
        outdata = OS_MALLOC(scn->sc_osdev, outsize, GFP_KERNEL);
        if (outdata == NULL) {
            error = -ENOMEM;
            goto bad;
        }

        id = id & ~ATH_DIAG_DYN;
    }

    if (ath_hal_getdiagstate(ic, id, indata, insize, &outdata, &outsize)) {
        if (outsize < ad->ad_out_size)
            ad->ad_out_size = outsize;
        if (outdata && _copy_to_user(ad->ad_out_data, outdata, ad->ad_out_size))
            error = -EFAULT;
    } else {
        qdf_info("SIOCATHDIAG : Error\n");
        error = -EINVAL;
    }
bad:
    if ((ad->ad_id & ATH_DIAG_IN) && indata != NULL)
        qdf_mem_free(indata);
    if ((ad->ad_id & ATH_DIAG_DYN) && outdata != NULL)
        qdf_mem_free(outdata);
    return error;
}

#ifdef ATH_USB
#include "usb_eth.h"
#else
extern int ol_ath_ioctl_ethtool(struct ol_ath_softc_net80211 *scn, int cmd, void *addr);
#endif

///TODO: Should this be defined here..
//#if ATH_PERF_PWR_OFFLOAD
int
utf_unified_ioctl (struct ol_ath_softc_net80211 *scn, struct ifreq *ifr)
{
    unsigned int cmd = 0;
    char *userdata;

    get_user(cmd, (int *)ifr->ifr_data);
    userdata = (char *)(((unsigned int *)ifr->ifr_data)+1);

    /*last parameter is length which is used only for FTM daemon*/
    return ol_ath_ucfg_utf_unified_cmd(scn, cmd, userdata, 0);
}

#if UMAC_SUPPORT_ACFG
int ol_acfg_handle_ioctl(struct net_device *dev, void *data);
#endif

int ol_ath_extended_commands(struct net_device *dev,
        void *vextended_cmd)
{
    struct extended_ioctl_wrapper *extended_cmd = ( struct extended_ioctl_wrapper *)vextended_cmd;
    struct ol_ath_softc_net80211 *scn = ath_netdev_priv(dev);
    struct ieee80211com *ic = &scn->sc_ic;
    int error = 0;
    int reply = 0;

    if (scn->soc->sc_in_delete) {
        return -ENODEV;
    }

    if (ol_ath_target_start(scn->soc)) {
        qdf_info("failed to start the target");
        return -1;
    }
    switch (extended_cmd->cmd) {
        case EXTENDED_SUBIOCTL_THERMAL_SET_PARAM:
            error = ol_ath_ioctl_set_thermal_handler(scn,
                    (caddr_t)&(extended_cmd->ext_data.th_config));
            break;
        case EXTENDED_SUBIOCTL_THERMAL_GET_PARAM:
            error = ol_ath_ioctl_get_thermal_handler(scn,
                    (caddr_t)&(extended_cmd->ext_data.get_param));
            reply = 1;
            break;
#if ATH_PROXY_NOACK_WAR
        case EXTENDED_SUBIOCTL_OL_GET_PROXY_NOACK_WAR:
            error = ol_ioctl_get_proxy_noack_war(scn,
                    (caddr_t)&(extended_cmd->ext_data.qwrap_config));
            reply = 1;
            break;
        case EXTENDED_SUBIOCTL_OL_RESERVE_PROXY_MACADDR:
            error = ol_ioctl_reserve_proxy_macaddr(scn,
                    (caddr_t*)&(extended_cmd->ext_data.qwrap_config));
            reply = 1;
            break;
#endif
        case EXTENDED_SUBIOCTL_CHANNEL_SWITCH:
            error = ieee80211_extended_ioctl_chan_switch(dev, ic,
                    (caddr_t)&(extended_cmd->ext_data.channel_switch_req));
            break;
        case EXTENDED_SUBIOCTL_CHANNEL_SCAN:
            error = ieee80211_extended_ioctl_chan_scan(dev, ic,
                    (caddr_t)&(extended_cmd->ext_data.channel_scan_req));
            break;
        case EXTENDED_SUBIOCTL_REPEATER_MOVE:
            error = ieee80211_extended_ioctl_rep_move(dev, ic,
                    (caddr_t)&(extended_cmd->ext_data.rep_move_req));
            break;
#if ATH_SUPPORT_WRAP && DBDC_REPEATER_SUPPORT
        case EXTENDED_SUBIOCTL_GET_PRIMARY_RADIO:
            error = ol_ioctl_get_primary_radio(scn,
                    (caddr_t)&(extended_cmd->ext_data.qwrap_config));
            reply = 1;
            break;
        case EXTENDED_SUBIOCTL_GET_MPSTA_MAC_ADDR:
            error = ol_ioctl_get_mpsta_mac_addr(scn,
                    (caddr_t)&(extended_cmd->ext_data.qwrap_config));
            reply = 1;
            break;
        case EXTENDED_SUBIOCTL_DISASSOC_CLIENTS:
            ol_ioctl_disassoc_clients(scn);
            break;
        case EXTENDED_SUBIOCTL_GET_FORCE_CLIENT_MCAST:
            error = ol_ioctl_get_force_client_mcast(scn,
                    (caddr_t)&(extended_cmd->ext_data.qwrap_config));
            reply = 1;
            break;
        case EXTENDED_SUBIOCTL_GET_MAX_PRIORITY_RADIO:
            error = ol_ioctl_get_max_priority_radio(scn,
                    (caddr_t)&(extended_cmd->ext_data.qwrap_config));
            reply = 1;
            break;
#endif
        case EXTENDED_SUBIOCTL_IFACE_MGR_STATUS:
#if ATH_SUPPORT_WRAP && DBDC_REPEATER_SUPPORT
            ol_ioctl_iface_mgr_status(scn, (caddr_t)&(extended_cmd->ext_data.iface_config));
#endif
            break;
        case EXTENDED_SUBIOCTL_GET_STAVAP_CONNECTION:
            error = ol_ioctl_get_stavap_connection(scn, (caddr_t)&(extended_cmd->ext_data.iface_config));
            reply = 1;
            break;
        case EXTENDED_SUBIOCTL_GET_DISCONNECTION_TIMEOUT:
#if DBDC_REPEATER_SUPPORT
            error = ol_ioctl_get_disconnection_timeout(scn, (caddr_t)&(extended_cmd->ext_data.iface_config));
            reply = 1;
#endif
            break;
        case EXTENDED_SUBIOCTL_GET_PREF_UPLINK:
#if DBDC_REPEATER_SUPPORT
            error = ol_ioctl_get_preferred_uplink(scn, (caddr_t)&(extended_cmd->ext_data.iface_config));
            reply = 1;
#endif
            break;
#if ATH_SUPPORT_ICM
        case EXTENDED_SUBIOCTL_GET_CHAN_VENDORSURVEY_INFO:
            {
                struct extacs_chan_info *chan_info;
                int ret;

                if (extended_cmd->data_len != sizeof(struct extacs_chan_info) *
                        NUM_MAX_CHANNELS) {
                    error = -EFAULT;
                    break;
                }

                chan_info = (struct extacs_chan_info *)qdf_mem_malloc(
                    sizeof(struct extacs_chan_info) * NUM_MAX_CHANNELS);
                if (!chan_info) {
                    error = -ENOMEM;
                    break;
                }

                error = ol_ioctl_get_chan_vendorsurvey_info(scn,
                            (caddr_t)chan_info);

                ret = _copy_to_user(extended_cmd->data,
                                    chan_info,
                                    sizeof(struct extacs_chan_info) *
                                        NUM_MAX_CHANNELS);
                if (ret) {
                    qdf_info("Error in copying data to user space");
                    error = -EFAULT;
                }

                qdf_mem_free(chan_info);
            }
            break;
#endif
        case EXTENDED_SUBIOCTL_GET_CAC_STATE:
            extended_cmd->ext_data.iface_config.cac_state = wlan_is_ap_cac_timer_running(ic);
            reply = 1;
            break;
        default:
            qdf_info("%s: unsupported extended command %d", __func__, extended_cmd->cmd);
            break;
    }
    if(error < 0)
        return error;
    else
        return reply;
}

#ifdef QCA_SUPPORT_CP_STATS
extern int wlan_update_pdev_cp_stats(struct ieee80211com *ic,
                              struct ol_ath_radiostats *scn_stats_user);
#endif
extern int wlan_get_pdev_dp_stats(struct ieee80211com *ic,
                              struct ol_ath_radiostats *scn_stats_user);

static int
ath_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
    struct ol_ath_softc_net80211 *scn = ath_netdev_priv(dev);
    //struct ath_phy_stats *ps;
    int error = 0;
    int ret = 0;
    char *userdata = NULL;
    struct extended_ioctl_wrapper *extended_cmd;
    ol_txrx_pdev_handle pdev;
    ol_txrx_soc_handle soc_txrx_handle = wlan_psoc_get_dp_handle(scn->soc->psoc_obj);
    struct cdp_dev_stats cdp_stats;
     struct ol_ath_radiostats radio_stats;
    struct ieee80211com *ic = &scn->sc_ic;

    if (ic->recovery_in_progress) {
       return -EINVAL;
    }

    if (cmd == ATH_IOCTL_EXTENDED) {
        /*
         * This allows for many more wireless ioctls than would otherwise
         * be available.  Applications embed the actual ioctl command in
         * the first word of the parameter block, and use the command
         * ATH_IOCTL_EXTENDED_CMD on the ioctl call.
         */
        get_user(cmd, (int *)ifr->ifr_data);
        userdata = (char *)(((unsigned int *)ifr->ifr_data)+1);
    }

    switch (cmd) {

#ifdef QVIT
    case SIOCIOCTLQVIT:
            error = qvit_unified_ioctl(dev, scn,ifr);
            break;
#endif
    case SIOCGATHEACS:
#if 0
#if WLAN_SPECTRAL_ENABLE
        error = osif_ioctl_eacs(dev, ifr, scn->sc_osdev);
#endif
#endif
        break;
    case SIOCGATHSTATS:
        {
            struct ath_stats_container asc;

            error = __xcopy_from_user(&asc, ifr->ifr_data, sizeof(asc) );
            if(error)
            {
                error = -EFAULT;
                break;
            }

            error = ol_ath_ucfg_get_ath_stats(scn, &asc);

            if (error || _copy_to_user(ifr->ifr_data, &asc, sizeof(asc)))
            {
                error = -EFAULT;
            }
        }
        break;
    case SIOCGATHSTATSCLR:
#if 0
        as = scn->sc_ops->get_ath_stats(scn->sc_dev);
        error = 0;
#endif
        break;
    case SIOCGATHPHYSTATS:

         if(((dev->flags & IFF_UP) == 0)){
         return -ENXIO;
         }
        pdev = wlan_pdev_get_dp_handle(scn->sc_pdev);
        if (!pdev) {
            return -EFAULT;
        }

        cdp_ath_getstats(soc_txrx_handle, (void *) pdev, &cdp_stats,
                        UPDATE_PDEV_STATS);

#ifdef QCA_SUPPORT_CP_STATS
        qdf_mem_zero(&radio_stats, sizeof(struct ol_ath_radiostats));
        if (wlan_update_pdev_cp_stats(ic, &radio_stats) != 0) {
            qdf_info("Invalid VAP Command !");
            return -EFAULT;
        }
#endif
        if (wlan_get_pdev_dp_stats(ic, &radio_stats) != 0) {
            qdf_print("Invalid VAP Command !");
            return -EFAULT;
        }
        if (_copy_to_user(ifr->ifr_data, &radio_stats,
                    sizeof(struct ol_ath_radiostats))) {
            error = -EFAULT;
        }
        break;
    case SIOCGATHDIAG:
#if 1
        if (!capable(CAP_NET_ADMIN))
            error = -EPERM;
        else
            error = ath_ioctl_diag(scn, (struct ath_diag *) ifr);
#endif
        break;
#if defined(ATH_SUPPORT_DFS) || defined(WLAN_SPECTRAL_ENABLE)
    case SIOCGATHPHYERR:
        {
            struct ath_diag diag;

            if (!capable(CAP_NET_ADMIN)) {
                error = -EPERM;
            } else {
                if (ol_ath_target_start(scn->soc)) {
                    qdf_info("failed to start the target");
                    return -1;
                }

                error = __xcopy_from_user(&diag, ifr->ifr_data, sizeof(diag));

                if (error) {
                    error = -EFAULT;
                    break;
                }
                error = ol_ath_ucfg_phyerr(scn, &diag);
            }
        }
        break;
#endif
    case SIOCETHTOOL:
#if 0
        if (__xcopy_from_user(&cmd, ifr->ifr_data, sizeof(cmd)))
            error = -EFAULT;
        else
            error = ol_ath_ioctl_ethtool(scn, cmd, ifr->ifr_data);
#endif
        break;
    case SIOC80211IFCREATE:
        {
            struct ieee80211_clone_params cp;

            if (__xcopy_from_user(&cp, ifr->ifr_data, sizeof(cp))) {
                return -EFAULT;
            }
            error = ol_ath_ucfg_create_vap(scn, &cp, ifr->ifr_name);
        }
        break;
#if defined(ATH_TX99_DIAG) && (!defined(ATH_PERF_PWR_OFFLOAD))
    case SIOCIOCTLTX99:
        if (ol_ath_target_start(scn->soc)) {
            qdf_info("failed to start the target");
            return -1;
        }

        qdf_info("Call Tx99 ioctl %d \n",cmd);
        error = tx99_ioctl(dev, ATH_DEV_TO_SC(scn->sc_dev), cmd, ifr->ifr_data);
        break;
#else
    case SIOCIOCTLTX99:
        if (ol_ath_target_start(scn->soc)) {
            qdf_info("failed to start the target");
            return -1;
        }

        error = utf_unified_ioctl(scn,ifr);
        break;
#endif
#ifdef ATH_SUPPORT_LINUX_VENDOR
    case SIOCDEVVENDOR:
        qdf_info("%s: SIOCDEVVENDOR TODO\n", __func__);
        //error = osif_ioctl_vendor(dev, ifr, 0);
        break;
#endif
#ifdef ATH_BUS_PM
    case SIOCSATHSUSPEND:
      {
        qdf_info("%s: SIOCSATHSUSPEND TODO\n", __func__);
#if 0
        struct ieee80211com *ic = &scn->sc_ic;
        struct ieee80211vap *tmpvap;
        int val = 0;
        if (__xcopy_from_user(&val, ifr->ifr_data, sizeof(int)))
          return -EFAULT;

        if(val) {
          /* suspend only if all vaps are down */
          TAILQ_FOREACH(tmpvap, &ic->ic_vaps, iv_next) {
            struct net_device *tmpdev = ((osif_dev *)tmpvap->iv_ifp)->netdev;
            if (tmpdev->flags & IFF_RUNNING)
              return -EBUSY;
          }
          error = bus_device_suspend(scn->sc_osdev);
        }
        else
          error = bus_device_resume(scn->sc_osdev);

        if (!error)
            scn->sc_osdev->isDeviceAsleep = val;
#endif
      }
      break;
#endif /* ATH_BUS_PM */
    case SIOCG80211PROFILE:
      {
          struct ieee80211_profile *profile;

          profile = (struct ieee80211_profile *)qdf_mem_malloc(
                  sizeof (struct ieee80211_profile));
          if (profile == NULL) {
              error = -ENOMEM;
              break;
          }
          OS_MEMSET(profile, 0, sizeof (struct ieee80211_profile));

          error = ol_ath_ucfg_get_vap_info(scn, profile);

          error = _copy_to_user(ifr->ifr_data, profile,
                  sizeof(struct ieee80211_profile));

          qdf_mem_free(profile);
          profile = NULL;
      }
      break;
#if UMAC_SUPPORT_ACFG
    case ACFG_PVT_IOCTL:
        error = ol_acfg_handle_ioctl(dev, ifr->ifr_data);
        break;
#endif
    case SIOCGATHEXTENDED:
        {
            extended_cmd = ( struct extended_ioctl_wrapper *)qdf_mem_malloc(sizeof(struct extended_ioctl_wrapper));
            if (!extended_cmd) {
                return -ENOMEM;
            }
            if (__xcopy_from_user(extended_cmd, ifr->ifr_data, sizeof(struct extended_ioctl_wrapper))) {
                qdf_mem_free(extended_cmd);
                return -EFAULT;
            }

            ret = ol_ath_extended_commands(dev ,(void*) extended_cmd);
             if (ret < 0) {
                 qdf_mem_free(extended_cmd);
                 return ret;
             }

            ret = _copy_to_user(ifr->ifr_data, extended_cmd,
                    sizeof(struct extended_ioctl_wrapper));
            if (ret) {
                qdf_info("Error in copying data to user space\n");
                error = -EFAULT;
            }
            qdf_mem_free(extended_cmd);
        }
        break;

    default:
        error = -EINVAL;
        break;
    }
    return error;
}

/*
 * Return netdevice statistics.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
static struct rtnl_link_stats64 *
ath_getstats(struct net_device *dev, struct rtnl_link_stats64* stats64)
#else
static struct net_device_stats *
ath_getstats(struct net_device *dev)
#endif
{
    struct ol_ath_softc_net80211 *scn = ath_netdev_priv(dev);
    ol_txrx_pdev_handle pdev_txrx_handle = wlan_pdev_get_dp_handle(scn->sc_pdev);
    ol_txrx_soc_handle soc_txrx_handle = wlan_psoc_get_dp_handle(scn->soc->psoc_obj);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 36)
    struct net_device_stats *stats = NULL;
#else
    struct rtnl_link_stats64 *stats = NULL;
#endif
    struct cdp_dev_stats cdp_stats;
    stats = &scn->sc_osdev->devstats;
    if ((dev->flags & IFF_RUNNING) != IFF_RUNNING)
            goto stats_done;

    if (atomic_read(&scn->soc->reset_in_progress))
            goto stats_done;

    if (!soc_txrx_handle || !pdev_txrx_handle)
            goto stats_done;
#if 0
    struct ath_stats *as;
    struct ath_phy_stats *ps;
    struct ath_11n_stats *ans;
    WIRELESS_MODE wmode;

    stats = &scn->sc_osdev->devstats;

    as = scn->sc_ops->get_ath_stats(scn->sc_dev);
    ans = scn->sc_ops->get_11n_stats(scn->sc_dev);
    /* update according to private statistics */
    stats->tx_errors = as->ast_tx_xretries
             + as->ast_tx_fifoerr
             + as->ast_tx_filtered
             ;
    stats->tx_dropped = as->ast_tx_nobuf
            + as->ast_tx_encap
            + as->ast_tx_nonode
            + as->ast_tx_nobufmgt;
    /* Add tx beacons, tx mgmt, tx, 11n tx */
    stats->tx_packets = as->ast_be_xmit
            + as->ast_tx_mgmt
            + as->ast_tx_packets
            + ans->tx_pkts;
    /* Add rx, 11n rx (rx mgmt is included) */
    stats->rx_packets = as->ast_rx_packets
            + ans->rx_pkts;

    for (wmode = 0; wmode < WIRELESS_MODE_MAX; wmode++) {
        ps = scn->sc_ops->get_phy_stats(scn->sc_dev, wmode);
        stats->rx_errors = ps->ast_rx_fifoerr;
        stats->rx_dropped = ps->ast_rx_tooshort;
        stats->rx_crc_errors = ps->ast_rx_crcerr;
    }

#endif
    if (!scn->sc_ic.ic_is_mode_offload) {
        goto stats_done;
    }

    cdp_ath_getstats(soc_txrx_handle, (void *) pdev_txrx_handle, &cdp_stats,
                    UPDATE_PDEV_STATS);
    stats->tx_packets = cdp_stats.tx_packets;
    stats->tx_bytes = cdp_stats.tx_bytes;

    stats->tx_errors = cdp_stats.tx_errors;
    stats->tx_dropped = cdp_stats.tx_dropped;

    stats->rx_packets = cdp_stats.rx_packets;
    stats->rx_bytes = cdp_stats.rx_bytes;
stats_done:
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 36)
    return stats;
#else
    memcpy(stats64, stats, sizeof(struct rtnl_link_stats64));
    return stats64;
#endif
}

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,30)
static const struct net_device_ops athdev_net_ops = {
    .ndo_open    = ath_netdev_open,
    .ndo_stop    = ath_netdev_stop,
    .ndo_start_xmit = ath_netdev_hardstart,
    .ndo_set_mac_address = ath_netdev_set_macaddr,
    .ndo_tx_timeout = ath_netdev_tx_timeout,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
    .ndo_get_stats64 = ath_getstats,
#else
    .ndo_get_stats = ath_getstats,
#endif
    .ndo_change_mtu = ath_change_mtu,
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,2,0)
    .ndo_set_multicast_list = ath_netdev_set_mcast_list,
#else
    .ndo_set_rx_mode = ath_netdev_set_mcast_list,
#endif
    .ndo_do_ioctl = ath_ioctl,
};
#endif

static struct ieee80211_reg_parameters ol_wlan_reg_params = {
    .sleepTimePwrSave = 100,         /* wake up every beacon */
    .sleepTimePwrSaveMax = 1000,     /* wake up every 10 th beacon */
    .sleepTimePerf=100,              /* station wakes after this many mS in max performance mode */
    .inactivityTimePwrSaveMax=400,   /* in max PS mode, how long (in mS) w/o Tx/Rx before going back to sleep */
    .inactivityTimePwrSave=200,      /* in normal PS mode, how long (in mS) w/o Tx/Rx before going back to sleep */
    .inactivityTimePerf=100,         /* in max perf mode, how long (in mS) w/o Tx/Rx before going back to sleep */
    .psPollEnabled=0,                /* Use PS-POLL to retrieve data frames after TIM is received */
    .wmeEnabled    = 1,
    .enable2GHzHt40Cap = 1,
    .cwmEnable = 1,
    .cwmExtBusyThreshold = IEEE80211_CWM_EXTCH_BUSY_THRESHOLD,
    .ignore11dBeacon = 1,
    .p2pGoUapsdEnable = 1,
    .extapUapsdEnable = 1,
#ifdef ATH_SUPPORT_TxBF
    .autocvupdate = 0,
#define DEFAULT_PER_FOR_CVUPDATE 30
    .cvupdateper = DEFAULT_PER_FOR_CVUPDATE,
#endif
    .regdmn = 0,
    .wModeSelect = WMI_HOST_REGDMN_MODE_ALL,
    .netBand = WMI_HOST_REGDMN_MODE_ALL,
    .extendedChanMode = 0,
};

void   ol_ath_linux_update_fw_config_cb(ol_ath_soc_softc_t *soc,
                          struct ol_ath_target_cap *tgt_cap)
{

    struct wlan_psoc_target_capability_info *target_cap;

    target_cap = lmac_get_target_cap(soc->psoc_obj);
    /*
     * tgt_cap contains default target resource configuration
     * which can be modified here, if required
     */
    /* 0: 128B - default, 1: 256B, 2: 64B */
    tgt_cap->wlan_resource_config.dma_burst_size = 1;

#if ATH_OL_11AC_MAC_AGGR_DELIM
	tgt_cap->wlan_resource_config.mac_aggr_delim = ATH_OL_11AC_MAC_AGGR_DELIM;
#endif

    /* Override the no. of max fragments as per platform configuration */
    if (target_cap) {
        tgt_cap->wlan_resource_config.max_frag_entries =
            MIN(QCA_OL_11AC_TX_MAX_FRAGS, target_cap->max_frag_entry);
        target_cap->max_frag_entry =
                       tgt_cap->wlan_resource_config.max_frag_entries;
    }
}

int ol_ath_verify_vow_config(ol_ath_soc_softc_t *soc)
{
    int vow_max_sta = ((soc->vow_config) & 0xffff0000) >> 16;
    int vow_max_desc_persta = ((soc->vow_config) & 0x0000ffff);
    struct target_psoc_info *tgt_psoc_info;

    tgt_psoc_info = (struct target_psoc_info *)wlan_psoc_get_tgt_if_handle(
                                                     soc->psoc_obj);
    if (tgt_psoc_info == NULL) {
        qdf_info("%s: target_psoc_info is null ", __func__);
        return -1;
    }

    if(lmac_get_tgt_type(soc->psoc_obj) == TARGET_TYPE_AR9888) {
        if((vow_max_sta * vow_max_desc_persta) > TOTAL_VOW_ALLOCABLE) {
            int vow_unrsvd_sta_num = 0, vow_rsvd_num = 0;

            vow_rsvd_num = TOTAL_VOW_ALLOCABLE/vow_max_desc_persta;

            vow_unrsvd_sta_num = vow_max_sta - vow_rsvd_num;

            if( (vow_unrsvd_sta_num * vow_max_desc_persta) > VOW_DESC_GRAB_MAX ) {
                /*cannot support the request*/
                vow_unrsvd_sta_num = VOW_DESC_GRAB_MAX / vow_max_desc_persta;
                qdf_info("%s: ERROR: Invalid vow_config\n",__func__);
                qdf_info("%s: Can support only %d clients for %d desc\n", __func__,
                        vow_rsvd_num + vow_unrsvd_sta_num,
                        vow_max_desc_persta);

                return -1;
            }
        }
    } else if (lmac_is_target_ar900b(soc->psoc_obj)) {
        /* Check not required as of now */
    }
    /* VoW takes precedence over max_descs and max_active_peers config. It will choose these
     * param accordingly */
    if( vow_max_sta ) {
        target_psoc_set_max_descs(tgt_psoc_info, 0);
        soc->max_active_peers = 0;
    }
    return 0;
}

int ol_ath_verify_max_descs(ol_ath_soc_softc_t *soc)
{
    struct target_psoc_info *tgt_psoc_info;
    uint32_t max_descs;

    tgt_psoc_info = (struct target_psoc_info *)wlan_psoc_get_tgt_if_handle(
                                                        soc->psoc_obj);
    if (tgt_psoc_info == NULL) {
        qdf_info("%s: target_psoc_info is null ", __func__);
        return 0;
    }

    if (!lmac_is_target_ar900b(soc->psoc_obj) ||
            soc->vow_config) {
        target_psoc_set_max_descs(tgt_psoc_info, 0);
        qdf_info("Overiding max_descs module param \n");
        return 0;
    }

    /*
     * the max_descs is a 16 bit value whose range falls between CFG_TGT_NUM_MSDU_DESC_AR900B *
     * CFG_TGT_NUM_MAX_MSDU_DESC
     * based on the max_descs value we derive max_active_peers value.
     * the dafult max_descs is CFG_TGT_NUM_MSDU_DESC_AR900B &
     * default max_active_peers is CFG_TGT_QCACHE_ACTIVE_PEERS
     * Each active peer consumes 780 bytes of DRAM and each descriptor 16 bytes
     * hence for every increase of 48 (780/16) descriptors we need to reduce
     * the active peer count by 1.
     * to ensure any future optimizations to firmware which help in reducing
     * the memory consumed by active peer, we allow a cushion of 2 descriptors
     * thus setting the value to 46 descriptors for one active peer.
     * To ensure minimum of CFG_TGT_QCACHE_MIN_ACTIVE_PEERS peers are always active we do not exceed max_descs
     * beyond CFG_TGT_QCACHE_MIN_ACTIVE_PEERS.
     */
    max_descs = target_psoc_get_max_descs(tgt_psoc_info);
    if (max_descs < CFG_TGT_NUM_MSDU_DESC_AR900B) {
        qdf_info("max_descs cannot be less then %d", CFG_TGT_NUM_MSDU_DESC_AR900B);
        max_descs = CFG_TGT_NUM_MAX_MSDU_DESC_AR900B;
        soc->max_active_peers = CFG_TGT_QCACHE_ACTIVE_PEERS;
    }
    else if (max_descs > CFG_TGT_NUM_MAX_MSDU_DESC_AR900B) {
        qdf_info("max_descs cannot exceed %d", CFG_TGT_NUM_MAX_MSDU_DESC_AR900B);
        max_descs = CFG_TGT_NUM_MAX_MSDU_DESC_AR900B;
        soc->max_active_peers = CFG_TGT_QCACHE_MIN_ACTIVE_PEERS;
    }
    else {
        soc->max_active_peers = CFG_TGT_QCACHE_ACTIVE_PEERS -
           ((max_descs - CFG_TGT_NUM_MSDU_DESC_AR900B)/CFG_TGT_NUM_MSDU_DESC_PER_ACTIVE_PEER + 1);
        max_descs = 0;
    }

    qdf_info("max_descs: %d, max_active_peers: %d\n", max_descs, soc->max_active_peers);
    target_psoc_set_max_descs(tgt_psoc_info, max_descs);

    return 0;
}


static int ol_get_powerof2(int d)
{
     int pow=1;
     int i=1;
     while(i<=d)
     {
          pow=pow*2;
          i++;
     }
     return pow;
}

int
ol_swap_seg_alloc (ol_ath_soc_softc_t *soc, struct swap_seg_info **ret_seg_info, u_int64_t **scn_cpuaddr, const char* filename, int type)
{
#if WIFI_MEM_MANAGER_SUPPORT
    int intr_ctxt;
#endif

    void *cpu_addr;
    dma_addr_t dma_handle;
    struct swap_seg_info *seg_info = NULL;
#if FW_CODE_SIGN
    struct firmware *fw_entry;
#else
    const struct firmware *fw_entry;
#endif /* FW_CODE_SIGN */
    int  swap_size=0, is_powerof_2=0, start=1, poweroff=0;
    int rfwst;

    if(!filename) {
        qdf_info("%s: File name is Null \n",__func__);
        goto end_swap_alloc;
    }

#if FW_CODE_SIGN
    rfwst = request_secure_firmware(&fw_entry, filename, soc->sc_osdev->device,
                soc->device_id);
#else
    rfwst = request_firmware(&fw_entry, filename, soc->sc_osdev->device);
#endif /* FW_CODE_SIGN */
    if (rfwst != 0)  {
        qdf_info("Failed to get fw: %s\n", filename);
        goto end_swap_alloc;
    }
    swap_size = fw_entry->size;
#if FW_CODE_SIGN
    release_secure_firmware(fw_entry);
#else
    release_firmware(fw_entry);
#endif /* FW_CODE_SIGN */
    if (swap_size == 0)  {
        qdf_info("%s : swap_size is 0\n ",__func__);
        goto end_swap_alloc;
    }

    /* check swap_size is power of 2 */
    is_powerof_2 = ((swap_size != 0) && !(swap_size & (swap_size - 1)));


    /* set the swap_size to nearest power of 2 celing */
    if (is_powerof_2 == 0) {
        while (swap_size <= EVICT_BIN_MAX_SIZE) {
            poweroff = ol_get_powerof2(start);
            start++;
            if (poweroff > swap_size) {
                swap_size = poweroff;
                break;
            }
        }
    }

    if (swap_size > EVICT_BIN_MAX_SIZE) {
        qdf_info("%s: Exceeded Max allocation %d,Swap Alloc failed: exited \n", __func__,swap_size);
        goto end_swap_alloc;
    }

#if WIFI_MEM_MANAGER_SUPPORT
    intr_ctxt = (in_interrupt() || irqs_disabled()) ? 1 : 0;
    cpu_addr = (void *) wifi_cmem_allocation(soc->soc_idx, 0, (CM_CODESWAP + type), swap_size, (void *)soc->qdf_dev->drv_hdl/*(void *)soc->sc_osdev->device*/, &dma_handle, intr_ctxt);
#else
    cpu_addr = dma_alloc_coherent(soc->sc_osdev->device, swap_size,
                                        &dma_handle, GFP_KERNEL);
#endif
    if (!cpu_addr || !dma_handle) {
        qdf_info(" Memory Alloc failed for swap feature\n");
          goto end_swap_alloc;
    }

    seg_info = devm_kzalloc(soc->sc_osdev->device, sizeof(*seg_info),
                                               GFP_KERNEL);
    if (!seg_info) {
            qdf_info("Fail to allocate memory for seg_info\n");
            goto end_dma_alloc;
    }

    memset(seg_info, 0, sizeof(*seg_info));
    seg_info->seg_busaddr[0]   = dma_handle;
    seg_info->seg_cpuaddr[0]   = cpu_addr;
    seg_info->seg_size         = swap_size;
    seg_info->seg_total_bytes  = swap_size;
    /* currently design assumes 1 valid code/data segment */
    seg_info->num_segs         = 1;
    seg_info->seg_size_log2    = ilog2(swap_size);
    *(scn_cpuaddr)   = cpu_addr;

    *(ret_seg_info) = seg_info;

    qdf_info(KERN_INFO"%s: Successfully allocated memory for SWAP size=%d \n", __func__,swap_size);

    return 0;

end_dma_alloc:
    dma_free_coherent(soc->sc_osdev->device, swap_size, cpu_addr, dma_handle);

end_swap_alloc:
    return -1;

}
qdf_export_symbol(ol_swap_seg_alloc);

int
ol_swap_wlan_memory_expansion(ol_ath_soc_softc_t *soc, struct swap_seg_info *seg_info,const char*  filename, u_int32_t *target_addr)
{
    struct device *dev;
#if FW_CODE_SIGN
    struct firmware *fw_entry;
#else
    const struct firmware *fw_entry;
#endif /* FW_CODE_SIGN */
    u_int32_t fw_entry_size, size_left, dma_size_left;
    char *fw_temp;
    char *fw_data;
    char *dma_virt_addr;
    u_int32_t total_length = 0, length=0;
    /* 3 Magic zero dwords will be there in swap bin files */
    unsigned char fw_code_swap_magic[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                           0x00, 0x00 };
    int status = -1;
    int rfwst = 0;
    dev = soc->sc_osdev->device;

    if (!seg_info) {
        qdf_info("seg_info is NULL\n");
        goto end;
    }
#if FW_CODE_SIGN
    rfwst = request_secure_firmware(&fw_entry, filename, dev, soc->device_id);
#else
    rfwst = request_firmware(&fw_entry, filename, dev);
#endif /* FW_CODE_SIGN */
    if (rfwst != 0) {
        qdf_info("Failed to get fw: %s\n", filename);
        goto end;
    }
    if (!fw_entry || !fw_entry->data) {
        qdf_info("%s: INVALID FW entries\n", __func__);
        goto release_fw;
    }

    dma_virt_addr = (char *)(unsigned long)seg_info->seg_cpuaddr[0];
    fw_data = (u8 *) fw_entry->data;
    fw_temp = fw_data;
    fw_entry_size = fw_entry->size;
    if (fw_entry_size > EVICT_BIN_MAX_SIZE) {
        qdf_info("%s: Exceeded Max allocation, Swap exit \n", __func__);
        goto release_fw;
    }
    size_left = fw_entry_size;
    dma_size_left = seg_info->seg_size;
   /* parse codeswap bin file for address,length,value
    * and copy bin file to host allocated memory. Current
    * desing will have 2 semgment, 1'st valid segment ,2nd
    * will be all zero followed by target address where
    * host want to write the seginfo swap structure
    */
    while ((size_left && fw_temp) && (dma_size_left > 0)) {
        fw_temp = fw_temp + 4;
        size_left = size_left - 4;
#if defined(BIG_ENDIAN_HOST)
        length = qdf_le32_to_cpu(*(int *)fw_temp);
#else
        length = *(int *)fw_temp;
#endif
    qdf_info("%s: length:%d size_left:%d dma_size_left:%d fw_temp:%pK fw_entry_size:%zu",
                      __func__, length, size_left, dma_size_left, fw_temp,fw_entry->size);
        if ((length > size_left || length <= 0) ||
            (dma_size_left <= 0 || length > dma_size_left)) {
            qdf_info(KERN_INFO"Swap: wrong length read:%d\n",length);
            break;
        }
        fw_temp = fw_temp + 4;
        size_left = size_left - 4;
#if AH_NEED_TX_DATA_SWAP
        /* Do byte swap before storing in to DMA address */
        {
            int i=0;
            u_int32_t* dest_addr= (u_int32_t*)dma_virt_addr;
            u_int32_t* src_addr= (u_int32_t*)fw_temp;
            for (i=0; i < (length+3)/4; i++) {
                dest_addr[i] = qdf_le32_to_cpu(src_addr[i]);
            }
        }
#else
        qdf_info("%s: dma_virt_addr :%pK fw_temp: %pK length: %d",
                      __func__, dma_virt_addr, fw_temp, length);
        memcpy(dma_virt_addr, fw_temp, length);
#endif
        dma_size_left = dma_size_left - length;
        size_left = size_left - length;
        fw_temp = fw_temp + length;
        dma_virt_addr = dma_virt_addr + length;
        total_length += length;
        qdf_info(KERN_INFO"Swap: bytes_left to copy: fw:%d; dma_page:%d\n",
                                      size_left, dma_size_left);
    }
    /* we are end of the last segment where 3 dwords
     * are zero and after that a dword which has target
     * address where host write the swap info struc to fw
     */
    if(( 0 == (size_left - 12))&& length == 0) {
        fw_temp = fw_temp - 4;
        if (memcmp(fw_temp,fw_code_swap_magic,12) == 0) {
            fw_temp = fw_temp + 12;
#if defined(BIG_ENDIAN_HOST)
            {
                u_int32_t swap_target_addr = qdf_le32_to_cpu(*(int *)fw_temp);
                memcpy(target_addr,&swap_target_addr,4);
            }
#else
            memcpy(target_addr,fw_temp,4);
#endif
            qdf_info(KERN_INFO"%s: Swap total_bytes copied: %d Target address %x \n", __func__, total_length,*target_addr);
            status = 0;
        }

    }
    seg_info->seg_total_bytes = total_length;

release_fw:
#if FW_CODE_SIGN
    release_secure_firmware(fw_entry);
#else
    release_firmware(fw_entry);
#endif  /* FW_CODE_SIGN */
end:
    return status;
}
qdf_export_symbol(ol_swap_wlan_memory_expansion);

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
void ol_ath_enable_fraglist(struct net_device *dev)
{
    struct ol_ath_softc_net80211 *scn;
    void *nss_wifiol_ctx = NULL;

    scn = ath_netdev_priv(dev);
    nss_wifiol_ctx= scn->nss_radio.nss_rctx;

    if (!nss_wifiol_ctx) {
        return;
    }

#if LINUX_VERSION_CODE > KERNEL_VERSION(3,2,0) || PARTNER_ETHTOOL_SUPPORTED
    dev->hw_features |= NETIF_F_SG | NETIF_F_FRAGLIST;
    /* Folowing is required to have fraglist support for VLAN over VAP interfaces */
    dev->vlan_features |= NETIF_F_SG | NETIF_F_FRAGLIST;
    dev->features |= NETIF_F_SG | NETIF_F_FRAGLIST;
#else
    dev->features |= NETIF_F_SG | NETIF_F_FRAGLIST;
#endif
#if LINUX_VERSION_CODE > KERNEL_VERSION(3,3,0) || PARTNER_ETHTOOL_SUPPORTED
    /* ethtool interface changed for kernel versions > 3.3.0,
     * Corresponding changes as follows */
    dev->wanted_features |= NETIF_F_SG | NETIF_F_FRAGLIST;
#endif
    qdf_info("Enabled Fraglist bit for the radio %s features %x ", dev->name,(unsigned int)dev->features);
}
#endif


/* network device name of SOC (dummy) and PDEV (radio) */
#define WIFI_DEV_NAME_SOC	"soc%d"

/* net device ops & ethtool ops for SOC device */
static const struct net_device_ops soc_netdev_ops;
static const struct ethtool_ops soc_ethtool_ops;

static void soc_netdev_setup (struct net_device *ndev)
{
}

/**
 * create_target_if_ctx() - create target interface context
 *
 * Return: QDF_STATUS
 */
QDF_STATUS create_target_if_ctx(void)
{
    return target_if_init(get_psoc_handle_from_scn);
}
qdf_export_symbol(create_target_if_ctx);

/**
 * get_psoc_handle_from_scn() - get psoc handle from scn handle
 * @scn_handle: scn handle
 *
 * Callback registered to get wmi_handle form scn_handle
 * Return: psoc handle
 */
struct wlan_objmgr_psoc * get_psoc_handle_from_scn(void * scn_handle)
{
    ol_ath_soc_softc_t *soc = (ol_ath_soc_softc_t *)scn_handle;
    return soc->psoc_obj;
}

#ifdef WLAN_SUPPORT_TWT
static void init_twt_default_config(ol_ath_soc_softc_t *soc)
{
    struct wlan_objmgr_psoc *psoc = soc->psoc_obj;

    soc->twt_enable = cfg_get(psoc, CFG_OL_TWT_ENABLE);
    soc->twt.sta_cong_timer_ms = cfg_get(psoc, CFG_OL_TWT_STA_CONG_TIMER_MS);
    soc->twt.mbss_support = cfg_get(psoc, CFG_OL_TWT_MBSS_SUPPORT);
    soc->twt.default_slot_size = cfg_get(psoc, CFG_OL_TWT_DEFAULT_SLOT_SIZE);
    soc->twt.congestion_thresh_setup = cfg_get(psoc, CFG_OL_TWT_CONGESTION_THRESH_SETUP);
    soc->twt.congestion_thresh_teardown = cfg_get(psoc, CFG_OL_TWT_CONGESTION_THRESH_TEARDOWN);
    soc->twt.congestion_thresh_critical = cfg_get(psoc, CFG_OL_TWT_CONGESTION_THRESH_CRITICAL);
    soc->twt.interference_thresh_teardown = cfg_get(psoc, CFG_OL_TWT_INTERFERENCE_THRESH_TEARDOWN);
    soc->twt.interference_thresh_setup = cfg_get(psoc, CFG_OL_TWT_INTERFERENCE_THRESH_SETUP);
    soc->twt.min_no_sta_setup = cfg_get(psoc, CFG_OL_TWT_MIN_NUM_STA_SETUP);
    soc->twt.min_no_sta_teardown = cfg_get(psoc, CFG_OL_TWT_MIN_NUM_STA_TEARDOWN);
    soc->twt.no_of_bcast_mcast_slots = cfg_get(psoc, CFG_OL_TWT_NUM_BCMC_SLOTS);
    soc->twt.min_no_twt_slots = cfg_get(psoc, CFG_OL_TWT_MIN_NUM_SLOTS);
    soc->twt.max_no_sta_twt = cfg_get(psoc, CFG_OL_TWT_MAX_NUM_STA_TWT);
    soc->twt.mode_check_interval = cfg_get(psoc, CFG_OL_TWT_MODE_CHECK_INTERVAL);
    soc->twt.add_sta_slot_interval = cfg_get(psoc, CFG_OL_TWT_ADD_STA_SLOT_INTERVAL);
    soc->twt.remove_sta_slot_interval = cfg_get(psoc, CFG_OL_TWT_REMOVE_STA_SLOT_INTERVAL);
}
#else
static void init_twt_default_config(ol_ath_soc_softc_t *soc)
{
    return;
}
#endif

int
__ol_ath_attach(void *hif_hdl, struct ol_attach_t *ol_cfg, osdev_t osdev, qdf_device_t qdf_dev)
{
    ol_ath_soc_softc_t *soc;
    struct ieee80211com *ic;
    int error = 0;
    struct net_device *dev;
    u_int32_t irq = 0, j =0, retval = 0;
    int32_t i = 0;
    struct wlan_objmgr_psoc *psoc;
    QDF_STATUS status;
#if  WLAN_SPECTRAL_ENABLE
    struct spectral_legacy_cbacks spectral_legacy_cbacks;
#endif
    struct target_psoc_info *tgt_psoc_info;
    uint32_t target_type;
    uint32_t target_version;
    uint32_t preferred_hw_mode;
    ol_txrx_soc_handle soc_txrx_handle;
    ol_txrx_pdev_handle pdev_txrx_handle;
    struct common_wmi_handle *wmi_handle;
    uint8_t num_radios;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,19,0)
    dev = alloc_netdev(sizeof(struct ol_ath_soc_softc),
                       ol_cfg->bus_type == BUS_TYPE_SIM ? "wifi-sim%d" : WIFI_DEV_NAME_SOC,
                       0,
                       soc_netdev_setup);
#else
    dev = alloc_netdev(sizeof(struct ol_ath_soc_softc),
                       ol_cfg->bus_type == BUS_TYPE_SIM ? "wifi-sim%d" : WIFI_DEV_NAME_SOC,
                       soc_netdev_setup);
#endif

    if (dev == NULL) {
        qdf_info("ath: Cannot allocate softc");
        error = -ENOMEM;
        goto bad0;
    }

    soc = ath_netdev_priv(dev);
    OS_MEMZERO(soc, sizeof(*soc));

    target_type = ol_cfg->target_type;

    ol_if_offload_ops_attach(soc, target_type);
    if(!soc->ol_if_ops) {
	qdf_info("%s:ol_if_ops is NULL\n",__func__);
	error = -ENOMEM;
	goto bad0;
    }

#ifdef ATH_AHB
    if(ol_cfg->bus_type == BUS_TYPE_AHB)
        qdf_create_work(0, &soc->pci_reconnect_work, ahb_defer_reconnect, soc);
    else
#endif
        qdf_create_work(0, &soc->pci_reconnect_work, pci_defer_reconnect, soc);

    soc->recovery_enable = RECOVERY_DISABLE;
    soc->pci_reconnect = pci_reconnect_cb;


    qdf_info("Allocated soc %pK", soc);
#ifdef EPPING_TEST
    qdf_spinlock_create(&soc->data_lock);
    skb_queue_head_init(&soc->epping_nodrop_queue);
    qdf_timer_init(NULL, &soc->epping_timer, epping_timer_expire,
                  (unsigned long) dev, QDF_TIMER_TYPE_SW);
    soc->epping_timer_running = 0;
#endif
    soc->sc_osdev = osdev;
    soc->soc_attached = 0;
    soc->sc_osdev->netdev = dev;
#ifdef AH_CAL_IN_FLASH_PCI
    soc->cal_idx = pci_dev_cnt;
#endif
    if (hif_get_irq_num(hif_hdl, &irq, 1) < 0)
        qdf_info("ERROR: not able to retrive radio's irq number ");
    dev->irq = irq;
    /* Used to get fw stats */
    sema_init(&soc->stats_sem, 0);

    /* Creating an ASF instance for the ASF print framework */
    error = ol_asf_adf_attach(soc);
    if (error)
        goto bad1;

    soc->device_id = ol_cfg->devid;
    soc->platform_devid = ol_cfg->platform_devid;
    soc->targetdef = hif_get_targetdef(hif_hdl);

    psoc = wlan_objmgr_psoc_obj_create(ol_cfg->devid, WLAN_DEV_OL);
    if (psoc == NULL) {
         qdf_info("ath: psoc alloc failed");
         error = -ENOMEM;
         goto bad4;
    }
    soc->psoc_obj = psoc;

    if(target_if_alloc_psoc_tgt_info(psoc) != QDF_STATUS_SUCCESS) {
         qdf_info("ath: psoc tgt info alloc failed");
         error = -ENOMEM;
         goto bad5;
    }

    /* initialize the dump options */
    soc->sc_dump_opts = cfg_get(psoc, CFG_OL_FW_DUMP_OPTIONS);

    tgt_psoc_info = (struct target_psoc_info *)wlan_psoc_get_tgt_if_handle(
						soc->psoc_obj);
    if(tgt_psoc_info == NULL) {
        qdf_info("%s: target_psoc_info is null ", __func__);
        goto bad5;
    }
    target_type = ol_cfg->target_type;
    target_psoc_set_target_type(tgt_psoc_info, ol_cfg->target_type);
    target_psoc_set_target_rev(tgt_psoc_info, ol_cfg->target_revision);
    target_psoc_set_hif_hdl(tgt_psoc_info, hif_hdl);
    target_psoc_set_feature_ptr(tgt_psoc_info, soc);

    if(init_deinit_register_featurs_ops(soc->psoc_obj) !=
               QDF_STATUS_SUCCESS) {
        qdf_info("%s: register feature ops failed ", __func__);
        goto bad6;
    }
    wlan_psoc_set_qdf_dev(psoc, qdf_dev);
    soc->qdf_dev = qdf_dev;

#if (defined ATH_AHB) && !(defined QCA_PARTNER_PLATFORM)
    ol_ath_check_and_enable_btcoex_support(osdev->bdev, soc);
#endif
    qdf_dev->drv = osdev;
    osdev->qdf_dev = soc->qdf_dev;
    osdev->netdev = dev;

    /*
     * create and initialize ath layer
     */
    if (ol_cfg->bus_type == BUS_TYPE_SIM ) {
        soc->is_sim=true;
    }

    /* Init timeout */
    if (target_type == TARGET_TYPE_QCA6290) {
           /* Increase wmi_timeout for emulation */
           soc->sc_osdev->wmi_timeout = DEFAULT_WMI_TIMEOUT_EMU;
    } else {
           soc->sc_osdev->wmi_timeout = DEFAULT_WMI_TIMEOUT;
    }

    switch (target_type) {
        case TARGET_TYPE_QCA8074:
            cfg_psoc_parse(psoc, "QCA8074.ini");
            cfg_parse_to_psoc_store(psoc, "internal/QCA8074_i.ini");
            break;
        case TARGET_TYPE_QCA8074V2:
            cfg_psoc_parse(psoc, "QCA8074.ini");
            cfg_parse_to_psoc_store(psoc, "internal/QCA8074V2_i.ini");
            break;
        case TARGET_TYPE_QCA6018:
            cfg_psoc_parse(psoc, "QCA6018.ini");
            cfg_parse_to_psoc_store(psoc, "internal/QCA6018_i.ini");
            break;
        case TARGET_TYPE_QCA9984:
        case TARGET_TYPE_IPQ4019:
        case TARGET_TYPE_AR900B:
        case TARGET_TYPE_AR9888:
            /* Parse QCA9984 INI files as above 4 chipsets
             * fall under same AR900B family */
            cfg_psoc_parse(psoc, "QCA9984.ini");
            cfg_parse_to_psoc_store(psoc, "internal/QCA9984_i.ini");
            if (target_type == TARGET_TYPE_AR9888)
                cfg_parse_to_psoc_store(psoc, "internal/QCA9888_i.ini");
            break;
        case TARGET_TYPE_QCA6290:
            cfg_psoc_parse(psoc, "QCA6290.ini");
            cfg_parse_to_psoc_store(psoc, "internal/QCA6290_i.ini");
            break;
        default:
            break;
    }
    if (testmode == 1)
        wlan_psoc_nif_feat_cap_set(psoc, WLAN_SOC_F_TESTMODE_ENABLE);

    /* Init timeout (uninterrupted wait) */
    soc->sc_osdev->wmi_timeout_unintr = DEFAULT_WMI_TIMEOUT_UNINTR;
    target_psoc_set_wmi_timeout(tgt_psoc_info, soc->sc_osdev->wmi_timeout);

    soc->enableuartprint = cfg_get(psoc, CFG_OL_ENABLE_UART_PRINT);
    soc->vow_config = cfg_get(psoc, CFG_OL_VOW_CONFIG);
    target_psoc_set_max_descs(tgt_psoc_info, cfg_get(psoc, CFG_OL_MAX_DESC));
    if(cfg_get(psoc, CFG_OL_MAX_PEERS)) {
         wlan_psoc_set_max_peer_count(psoc, cfg_get(psoc, CFG_OL_MAX_PEERS));
    }

    if(cfg_get(psoc, CFG_OL_MAX_VDEVS)) {
         wlan_psoc_set_max_vdev_count(psoc, cfg_get(psoc, CFG_OL_MAX_VDEVS));
    }

    soc->peer_del_wait_time = cfg_get(psoc, CFG_OL_PEER_DEL_WAIT_TIME);
    soc->max_active_peers = cfg_get(psoc, CFG_OL_MAX_ACTIVE_PEERS);
    soc->max_desc = cfg_get(psoc, CFG_OL_MAX_DESC);
    if(cfg_get(psoc, CFG_OL_BEACON_OFFLOAD_DISABLE)) {
       wlan_psoc_nif_feat_cap_clear(psoc, WLAN_SOC_F_BCN_OFFLOAD);
    }
    else {
       wlan_psoc_nif_feat_cap_set(psoc, WLAN_SOC_F_BCN_OFFLOAD);
    }

    init_twt_default_config(soc);

    soc->cce_disable = cfg_get(psoc, CFG_OL_CCE_DISABLE);
    if (cfg_get(psoc, CFG_OL_HW_MODE_ID) < WMI_HOST_HW_MODE_MAX) {
        /* hw_mode_id overriden by module param. Use this mode */
        preferred_hw_mode = cfg_get(psoc, CFG_OL_HW_MODE_ID);
    } else {
        /* Detect and select from FW modes based on precedence */
        preferred_hw_mode = WMI_HOST_HW_MODE_DETECT;
    }
#if QCA_WIFI_QCA8074_VP
	qdf_info("%s[%d] preferred_hw_mode %d \n", __func__, __LINE__, preferred_hw_mode);
#endif
    target_psoc_set_preferred_hw_mode(tgt_psoc_info, preferred_hw_mode);
    target_psoc_set_total_mac_phy_cnt(tgt_psoc_info, 0);
    target_psoc_set_num_radios(tgt_psoc_info, 0);
    soc->dbg.print_rate_limit = DEFAULT_PRINT_RATE_LIMIT_VALUE;

    soc->tgt_sched_params = cfg_get(psoc, CFG_OL_TGT_SCHED_PARAM);

#if OS_SUPPORT_ASYNC_Q
    OS_MESGQ_INIT(osdev, &osdev->async_q,
		  sizeof(os_async_q_mesg),
		  OS_ASYNC_Q_MAX_MESGS,os_async_mesg_handler, osdev,
		  MESGQ_PRIORITY_NORMAL,MESGQ_ASYNCHRONOUS_EVENT_DELIVERY);
#endif

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    osif_nss_wifi_soc_setup(soc);
#endif

#if ATH_SUPPORT_WRAP
    if(cfg_get(psoc, CFG_OL_QWRAP_ENABLE)) {
        wlan_psoc_nif_feat_cap_set(psoc, WLAN_SOC_F_QWRAP_ENABLE);
    }
#endif

    /* if max active peers is explicitly configured, then max_descs will be ignored.
     * max_active_peers takes precedence over max_desc */
    if (soc->max_active_peers) {
        qdf_info("Using max_active_peers configured.\n%s\n",
               (soc->max_desc)?"Ignoring max_descs. max_active_peers takes precedence":"");
        soc->max_desc = 0;
    } else if (soc->max_desc) {
        ol_ath_verify_max_descs(soc);
    }
    target_psoc_set_max_descs(tgt_psoc_info, soc->max_desc);

    if(ol_ath_verify_vow_config(soc)) {
        /*cannot accomadate vow config requested*/
        error = -EINVAL;
        goto bad6;
    }

#if ATH_SSID_STEERING
    if(EOK != ath_ssid_steering_netlink_init()) {
        qdf_info("SSID steering socket init failed __investigate__");
        goto bad6;
    }
#endif

    init_waitqueue_head(&soc->sc_osdev->event_queue);

    /*
     * Resolving name to avoid a crash in request_irq() on new kernels
     */
    dev_alloc_name(dev, dev->name);
    qdf_info("dev name %s", dev->name);
    soc->soc_idx = (u_int32_t)(dev->name[strlen(dev->name) - 1] - '0');

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
    SET_MODULE_OWNER(dev);
#endif
    SET_NETDEV_DEV(dev, osdev->device);

#if WMI_RECORDING
    soc->wmi_proc_entry = proc_mkdir(dev->name, NULL);
    if(soc->wmi_proc_entry == NULL) {
        qdf_info("error while creating proc entry for %s", dev->name);
    }
#endif

#if QCA_LTEU_SUPPORT
#define LTEU_SUPPORT0    0x1
#define LTEU_SUPPORT1    0x2
#define LTEU_SUPPORT2    0x4
    if ((!strncmp("wifi0", dev->name, sizeof("wifi0") - 1)) &&
                     (cfg_get(psoc, CFG_OL_LTEU_SUPPORT) & LTEU_SUPPORT0) )
        wlan_psoc_nif_feat_cap_set(psoc, WLAN_SOC_F_LTEU_SUPPORT);
    else if ((!strncmp("wifi1", dev->name, sizeof("wifi1") - 1)) &&
                     (cfg_get(psoc, CFG_OL_LTEU_SUPPORT) & LTEU_SUPPORT1))
        wlan_psoc_nif_feat_cap_set(psoc, WLAN_SOC_F_LTEU_SUPPORT);
    else if ((!strncmp("wifi2", dev->name, sizeof("wifi2") - 1)) &&
                      (cfg_get(psoc, CFG_OL_LTEU_SUPPORT) & LTEU_SUPPORT2))
        wlan_psoc_nif_feat_cap_set(psoc, WLAN_SOC_F_LTEU_SUPPORT);
#endif

#ifdef QCA_PARTNER_PLATFORM
    ath_pltfrm_init( dev );
#endif


    if (wlan_global_lmac_if_open(psoc)) {
        qdf_info("LMAC_IF open failed ");
        goto bad6;
    }

    error = ol_ath_soc_attach(soc, &ol_wlan_reg_params, ol_ath_linux_update_fw_config_cb);
    if (error)
        goto bad7;

    wmi_handle = target_psoc_get_wmi_hdl(tgt_psoc_info);
    num_radios = target_psoc_get_num_radios(tgt_psoc_info);
    target_version = target_psoc_get_target_ver(tgt_psoc_info);
#if CONFIG_DP_TRACE
    qdf_dp_trace_init(DP_TRACE_CONFIG_DEFAULT_LIVE_MODE,
                      DP_TRACE_CONFIG_DEFAULT_THRESH,
                      DP_TRACE_CONFIG_DEFAULT_THRESH_TIME_LIMIT,
                      DP_TRACE_CONFIG_DEFAULT_VERBOSTY,
                      DP_TRACE_CONFIG_DEFAULT_BITMAP);
#endif

#if ATH_SUPPORT_LOWI
    if ( EOK != ath_lowi_if_netlink_init()) {
        qdf_info("LOWI driver intferace socket init failed __investigate__ \n");
    }
#endif
#if ATH_PARAMETER_API
    if ( EOK != ath_papi_netlink_init()) {
        QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO, "Parameter API socket init failed __investigate__ \n");
    }
#endif

    ath_adhoc_netlink_init();

#if ATH_RXBUF_RECYCLE
    ath_rxbuf_recycle_init(osdev);
#endif

    ald_init_netlink();

    if ((error = ol_mempools_attach(soc)) != 0) {
        qdf_info("%s: ol_mempools attach failed",__func__);
        goto bad6;
    }

    soc_txrx_handle = wlan_psoc_get_dp_handle(soc->psoc_obj);
    if ((error = cdp_mempools_attach(soc_txrx_handle, (void *)soc)) != 0) {
        qdf_info("%s: ol_txrx_mempools attach failed",__func__);
        qdf_mempool_destroy(soc->qdf_dev, soc->mempool_ol_ath_node);
        qdf_mempool_destroy(soc->qdf_dev, soc->mempool_ol_ath_vap);
	status = -1;
        goto bad8;
    }

#if  WLAN_SPECTRAL_ENABLE
    spectral_legacy_cbacks.vdev_get_chan_freq = wlan_vdev_get_chan_freq;
    spectral_legacy_cbacks.vdev_get_ch_width = wlan_vdev_get_ch_width;
    spectral_legacy_cbacks.vdev_get_sec20chan_freq_mhz =
        wlan_vdev_get_sec20chan_freq_mhz;
    spectral_register_legacy_cb(psoc, &spectral_legacy_cbacks);
#endif

    for (i = 0; i < num_radios; i++) {
        struct net_device *pdev_netdev;
        struct ol_ath_softc_net80211 *scn;
        struct wlan_objmgr_pdev *pdev;

        pdev = wlan_objmgr_get_pdev_by_id(psoc, i, WLAN_MLME_NB_ID);
        if (pdev == NULL) {
             qdf_info("%s: pdev object (id: %d) is NULL", __func__, i);
             break;
        }

        scn = lmac_get_pdev_feature_ptr(pdev);
        pdev_netdev = scn->netdev;
        pdev_txrx_handle = wlan_pdev_get_dp_handle(pdev);
        /*
         * Resolving name to avoid a crash in request_irq() on new kernels
         */
        dev_alloc_name(pdev_netdev, pdev_netdev->name);
        qdf_info("pdev_netdev name %s", pdev_netdev->name);

	if (target_type == TARGET_TYPE_IPQ4019 || num_radios == 1) {
		if (hif_get_irq_num(hif_hdl, &irq, 1) < 0)
		        qdf_info("ERROR: not able to retrive radio's irq number ");
		pdev_netdev->irq = irq;
	}

        if (wlan_pdev_get_tgt_if_handle(pdev) == NULL) {
            qdf_info("%s: target_pdev_info is NULL (pdev id%d)",
                    __func__, i);
            wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_NB_ID);
            break;
        }

        /* get radio id from linux alloc-ed id */
        scn->radio_id = (u_int32_t)(pdev_netdev->name[strlen(pdev_netdev->name) - 1] - '0');
        scn->max_clients = soc->max_clients/num_radios;
        scn->max_vaps = soc->max_vaps/num_radios;
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
        if (!bypasswmi) {
            if ((target_type != TARGET_TYPE_QCA8074) &&
                    (target_type != TARGET_TYPE_QCA8074V2) &&
                    (target_type != TARGET_TYPE_QCA6018) &&
                    (target_type != TARGET_TYPE_QCA6290)) {
                if (target_version != AR6004_VERSION_REV1_3) {
                    if (cdp_pdev_attach_target(soc_txrx_handle,
                        (void *)pdev_txrx_handle) != A_OK) {
                        qdf_info("%s: txrx pdev attach target failed\n",__func__);
                        wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_NB_ID);
                        return -1;
                    }
                }
            } else {
                qdf_info("Skip txrx_pdev_attach_target");
            }
        }

        if ((target_type != TARGET_TYPE_QCA8074) &&
                (target_type != TARGET_TYPE_QCA8074V2) &&
                (target_type != TARGET_TYPE_QCA6018) &&
                (target_type != TARGET_TYPE_QCA6290)) {
            cdp_monitor_set_filter_ucast_data(soc_txrx_handle,
                    (void *)pdev_txrx_handle,0);
            cdp_monitor_set_filter_mcast_data(soc_txrx_handle,
                    (void *)pdev_txrx_handle,0);
            cdp_monitor_set_filter_non_data(soc_txrx_handle,
                    (void *)pdev_txrx_handle,0);
        }
        ic = wlan_pdev_get_mlme_ext_obj(pdev);
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

        /* Enable beacon bursting by default */
        retval = ol_ath_pdev_set_param(scn, wmi_pdev_param_beacon_tx_mode,
                                       BEACON_TX_MODE_BURST);
        if (retval == EOK) {
            scn->bcn_mode = BEACON_TX_MODE_BURST;
        }

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
        } else {
            ic->ic_num_clients = IEEE80211_512_AID;
        }
#endif

        ic->ic_get_min_and_max_power = ol_ath_get_min_and_max_power;
        ic->ic_fill_hal_chans_from_reg_db = ol_ath_fill_hal_chans_from_reg_db;
        ic->ic_get_modeSelect = ol_ath_get_modeSelect;
        ic->ic_get_chip_mode = ol_ath_get_chip_mode;
        ic->ic_is_regulatory_offloaded = ol_ath_is_regulatory_offloaded;
        ic->vendor_id = ol_cfg->vendor_id;
        ic->device_id = ol_cfg->devid;
#if UMAC_SUPPORT_CFG80211
	ic->ic_cfg80211_config = cfg_get(psoc, CFG_OL_CFG80211_CONFIG);
#endif
#if QCA_SUPPORT_GPR
        ic->ic_gpr_enable_count = 0;
        ic->ic_gpr_enable = 0;
#endif
        ic->ic_mcst_of_rootap = 0;
        ic->ic_has_rootap_done_cac = false;
	if (ol_scan_chanlist_size > 0) {
            /* Populate custom scan order.
             * This is done at the IC level rather than VAP level since it is meant
             * for use with inputs provided via a module parameter. See comments for
             * ol_scan_chanlist.
             */

            qdf_info("Populating custom scan order\n");

            qdf_mem_set(ic->ic_custom_scan_order,
                        sizeof(ic->ic_custom_scan_order), 0);

            for (j = 0;
                 j < MIN(ol_scan_chanlist_size, IEEE80211_N(ic->ic_custom_scan_order));
                 j++)
            {
                ic->ic_custom_scan_order[i] =
                    ieee80211_ieee2mhz(ic, ol_scan_chanlist[i], 0);
                ic->ic_custom_scan_order_size++;
            }
        } else {
            ic->ic_custom_scan_order_size = 0;
        }

	qdf_spinlock_create(&scn->scn_lock);

        scn->dpdenable = 1;
        scn->scn_amsdu_mask = 0xffff;
        scn->scn_ampdu_mask = 0xffff;

        error = ol_ath_pdev_attach(scn, &ol_wlan_reg_params,
                                                  lmac_get_pdev_idx(scn->sc_pdev));
        if (error) {
            qdf_info("%s: ol_ath_pdev_attach() failed. error %d\n", __func__, error);
            qdf_spinlock_destroy(&scn->scn_lock);
#if PERF_FIND_WDS_NODE
            wds_table_uninit(&scn->scn_wds_table);
#endif
            wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_NB_ID);
            break;
        }

#if defined(OL_ATH_SMART_LOGGING) && !defined(REMOVE_PKT_LOG)
    qdf_mutex_create(&ic->smart_log_fw_pktlog_mutex);
#endif /* defined(OL_ATH_SMART_LOGGING) && !defined(REMOVE_PKT_LOG) */

#ifdef OL_ATH_SMART_LOGGING
    {
        void *dbglog_handle = NULL;

        dbglog_handle = target_psoc_get_dbglog_hdl(tgt_psoc_info);

        if (wmi_service_enabled(wmi_handle,
                                wmi_service_smart_logging_support)) {
            if (dbglog_handle && fwdbg_smartlog_init(dbglog_handle, ic))
                target_if_err("Error enabling smart log\n");
        /*No need to send WMI command to FW as by default enabled */
        }
    }
#endif

        osif_attach(pdev_netdev);
#if 0
        /* For STA Mode default CWM mode is Auto */
        if ( ic->ic_opmode == IEEE80211_M_STA)
            ic->ic_cwm_set_mode(ic, IEEE80211_CWM_MODE2040);
#endif
        ol_ath_pdev_proc_create(ic,scn->netdev);
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
        /*
         * initialize tx/rx engine
         */
#if 0
        qdf_info("%s: init tx/rx TODO\n", __func__);
        error = scn->sc_ops->tx_init(scn->sc_dev, ATH_TXBUF);
        if (error != 0)
            goto badTBD;

        error = scn->sc_ops->rx_init(scn->sc_dev, ATH_RXBUF);
        if (error != 0)
            goto badTBD;
#endif

    /*
     * setup net device
     */
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,30)
        pdev_netdev->netdev_ops = &athdev_net_ops;
#else
        pdev_netdev->open = ath_netdev_open;
        pdev_netdev->stop = ath_netdev_stop;
        pdev_netdev->hard_start_xmit = ath_netdev_hardstart;
        pdev_netdev->set_mac_address = ath_netdev_set_macaddr;
        pdev_netdev->tx_timeout = ath_netdev_tx_timeout;
        pdev_netdev->set_multicast_list = ath_netdev_set_mcast_list;
        pdev_netdev->do_ioctl = ath_ioctl;
        pdev_netdev->get_stats = ath_getstats;
        pdev_netdev->change_mtu = ath_change_mtu;
#endif
        pdev_netdev->watchdog_timeo = 5 * HZ;           /* XXX */
        pdev_netdev->tx_queue_len = ATH_TXBUF-1;        /* 1 for mgmt frame */

        if (lmac_is_target_ar900b(soc->psoc_obj)|| ol_target_lithium(soc->psoc_obj)) {
            pdev_netdev->needed_headroom = sizeof (struct ieee80211_qosframe) +
                sizeof(struct llc) + IEEE80211_ADDR_LEN +
#if MESH_MODE_SUPPORT
                sizeof(struct meta_hdr_s) +
#endif
                IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN;

            qdf_info("needed_headroom reservation %d",
                   pdev_netdev->needed_headroom);
        } else {
            pdev_netdev->hard_header_len += sizeof (struct ieee80211_qosframe) +
                sizeof(struct llc) + IEEE80211_ADDR_LEN +
#if MESH_MODE_SUPPORT
                sizeof(struct meta_hdr_s) +
#endif
                IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN;

            qdf_info("hard_header_len reservation %d",
                   pdev_netdev->hard_header_len);
        }

        if (cfg_get(psoc, CFG_OL_TX_TCP_CKSUM)) {
            pdev_netdev->features |= NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM;
        }

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
        ol_ath_enable_fraglist(pdev_netdev);
#endif

#ifdef QVIT
// Enable ethtool support
#ifdef QVIT_DEBUG
        qdf_info("QVIT: %s: calling ethtool_ops\n", __func__);
#endif
        qvit_set_ethtool(pdev_netdev);
#endif

#if UMAC_SUPPORT_WEXT
        /*
         * Attach the iwpriv handlers for perf_pwr_offload device
         */
        ol_ath_iw_attach(pdev_netdev);
#endif

        /* Kernel 2.6.25 needs valid dev_addr before  register_netdev */
        IEEE80211_ADDR_COPY(pdev_netdev->dev_addr,ic->ic_myaddr);
        SET_NETDEV_DEV(pdev_netdev, osdev->device);

        /*
         * CCK is enabled by default in the firmware for all the chipsets.
         */
        ic->cck_tx_enable = true;

        /*
         * Register cfg80211 before netdev registration.
         */
#if UMAC_SUPPORT_CFG80211
        if (ic->ic_cfg80211_config) {
            ic->ic_cfg80211_radio_attach(osdev->device, pdev_netdev, ic);
        }
#endif

#if WLAN_SPECTRAL_ENABLE
	os_if_spectral_netlink_init(scn->sc_pdev);
#endif /* WLAN_SPECTRAL_ENABLE */
#if UMAC_SUPPORT_ACFG
        ic->ic_acfg_cmd = ol_acfg_handle_ioctl;
#endif

        /*
         * finally register netdev and ready to go
         */
        if ((error = register_netdev(pdev_netdev)) != 0) {
            qdf_err("%s: unable to register device", pdev_netdev->name);
#if WLAN_SPECTRAL_ENABLE
            os_if_spectral_netlink_deinit(scn->sc_pdev);
#endif /* WLAN_SPECTRAL_ENABLE */
#if UMAC_SUPPORT_CFG80211
            if (ic->ic_cfg80211_config)
                ic->ic_cfg80211_radio_detach(ic);
#endif
            /*
             * Detach the iwpriv handlers for perf_pwr_offload device
             */
#if UMAC_SUPPORT_WEXT
            ol_ath_iw_detach(pdev_netdev);
#endif
            ol_ath_pdev_proc_detach(ic);
            osif_detach(pdev_netdev);
            ol_ath_pdev_detach(scn, 1);
            qdf_spinlock_destroy(&scn->scn_lock);
#if PERF_FIND_WDS_NODE
            wds_table_uninit(&scn->scn_wds_table);
#endif
            wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_NB_ID);
            break;
        }

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
        SET_MODULE_OWNER(pdev_netdev);
#endif

        if (qdf_vfs_set_file_attributes((struct qdf_dev_obj *)
                                        &pdev_netdev->dev.kobj,
                                        (struct qdf_vfs_attr *)
                                        &wifi_attr_group)) {
            qdf_warn("Failed to create sys group");
        }

#if ATH_RX_LOOPLIMIT_TIMER
        scn->rx_looplimit_timeout = 1;           /* 1 ms by default */
        scn->rx_looplimit_valid = true;          /* set to valid after initilization */
        scn->rx_looplimit = false;
#endif
        error = ol_ath_thermal_mitigation_attach(scn, pdev_netdev);
        wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_NB_ID);
        if (error) {
            TH_DEBUG_PRINT(TH_DEBUG_LVL0, scn, "%s: unable to attach TT\n", dev->name);
            break;
        }
    }   /* end of for loop */

    if (i < num_radios) {
        /* failed to attach all pdev devices. */
        while (--i >= 0) {
            struct ol_ath_softc_net80211 *scn;
            struct net_device *pdev_netdev;
            struct wlan_objmgr_pdev *pdev;

            pdev = wlan_objmgr_get_pdev_by_id(psoc, i, WLAN_MLME_NB_ID);
            if (pdev == NULL) {
                 qdf_info("%s: pdev object (id: %d) is NULL", __func__, i);
                 continue;
            }

            scn = lmac_get_pdev_feature_ptr(pdev);

            if (scn == NULL) {
                wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_NB_ID);
                continue;
            }

	    qdf_spinlock_destroy(&scn->scn_lock);

            pdev_netdev = scn->netdev;

            ol_ath_thermal_mitigation_detach(scn, pdev_netdev);

            if (pdev_netdev->reg_state == NETREG_REGISTERED)
                unregister_netdev(pdev_netdev);

#if WLAN_SPECTRAL_ENABLE
	    os_if_spectral_netlink_deinit(pdev);
#endif /* WLAN_SPECTRAL_ENABLE */

            qdf_vfs_clear_file_attributes((struct qdf_dev_obj *)
                                          &pdev_netdev->dev.kobj,
                                          (struct qdf_vfs_attr *)
                                          &wifi_attr_group);

            osif_detach(pdev_netdev);

            ol_ath_pdev_detach(scn, 1); /* Force Detach */

#if PERF_FIND_WDS_NODE
            wds_table_uninit(&scn->scn_wds_table);
#endif
            if (pdev->pdev_nif.pdev_ospriv) {
                qdf_mem_free(pdev->pdev_nif.pdev_ospriv);
                pdev->pdev_nif.pdev_ospriv = NULL;
            }

            dispatcher_pdev_close(pdev);
            qdf_mem_free(scn->sc_osdev);
            free_netdev(pdev_netdev);
	    if (wlan_pdev_get_tgt_if_handle(pdev) != NULL) {
                target_if_free_pdev_tgt_info(pdev);
            }
            wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_NB_ID);
             /* Delete PDEV object */
            status = wlan_objmgr_pdev_obj_delete(pdev);
            if (status == QDF_STATUS_COMP_ASYNC) {
                qdf_info("PDEV (partially deleted) ");
            } else if (status == QDF_STATUS_E_FAILURE) {
                qdf_info("PDEV (deletion) failed ");
            }
        }

        goto bad6;
    }

    /* Don't leave arp type as ARPHRD_ETHER as this is no eth device */
    dev->type = ARPHRD_IEEE80211;

    dev->netdev_ops = &soc_netdev_ops;
    dev->ethtool_ops = &soc_ethtool_ops;


    /*
     * finally register netdev and ready to go
     */
    if ((error = register_netdev(dev)) != 0) {
        qdf_info(KERN_ERR "%s: unable to register device\n", dev->name);
        goto bad6;
    }

#if !NO_SIMPLE_CONFIG
    /* Request Simple Config intr handler */
    register_simple_config_callback (dev->name, (void *) jumpstart_intr, (void *) dev,
                                     (void *)&osdev->sc_push_button_dur);
#endif
#ifdef ATH_SUPPORT_LINUX_STA
#ifdef CONFIG_SYSCTL
    ath_dynamic_sysctl_register(ATH_DEV_TO_SC(soc->sc_dev));
#endif
#endif

    /*
     *Should not use ol_num_global_soc as index of the array.
     *Problem with that is: if recovery is enabled, when one of the radio(say wifi0) got target assert,
     *it'll do detach&attach, in attach function __ol_ath_attach(),
     *the ol_num_global_soc is already 1(assume AP has 2 radios),
     *then ol_global_soc[1]=soc will overwrite the soc for wifi1 which was saved in ol_global_soc[1]
     */
    ol_global_soc[soc->soc_idx] = soc;
    ol_num_global_soc++;
    qdf_minidump_log((void *)soc, sizeof(*soc), "ol_ath_soc_softc");

    if (qdf_vfs_set_file_attributes((struct qdf_dev_obj *)
                                    &dev->dev.kobj,
                                    (struct qdf_vfs_attr *)
                                    &wifi_soc_group)) {
        qdf_warn("Failed to create sys group");
    }
    soc->soc_attached = 1;

    return 0;


bad8:
    /* TODO - ol_ath_soc_detach */
#if OS_SUPPORT_ASYNC_Q
    OS_MESGQ_DRAIN(&osdev->async_q,NULL);
    OS_MESGQ_DESTROY(&osdev->async_q);
#endif

    ald_destroy_netlink();

#if ATH_RXBUF_RECYCLE
    ath_rxbuf_recycle_destroy(osdev);
#endif

    ath_adhoc_netlink_delete();

#if ATH_SUPPORT_LOWI
    ath_lowi_if_netlink_delete();
#endif
#if ATH_PARAMETER_API
    ath_papi_netlink_delete();
#endif

bad7:
    wlan_global_lmac_if_close(psoc);
bad6:
    target_if_free_psoc_tgt_info(psoc);
bad5:
    wlan_objmgr_psoc_obj_delete(psoc);

bad4:
#if WMI_RECORDING
    if(soc->wmi_proc_entry) {
        wmi_proc_remove((wmi_unified_t)wmi_handle, soc->wmi_proc_entry, soc->soc_idx);
        remove_proc_entry(dev->name, NULL);
    }
#endif

#if ATH_SSID_STEERING
    ath_ssid_steering_netlink_delete();
#endif

    qdf_info("\n%s() soc %pK Detaching Codeswap\n", __func__, soc);
    ol_codeswap_detach(soc);

bad1:
    /* TODO - free dev -- reverse alloc_netdev */
    free_netdev(dev);

bad0:
    return error;
}

static void ath_vdev_delete(struct wlan_objmgr_psoc *psoc, void *obj, void *args)
{
    struct wlan_objmgr_pdev *pdev = (struct wlan_objmgr_pdev *)obj;
    struct ieee80211com *ic;
    struct ieee80211vap *vap, *vapnext;
    osif_dev  *osifp;
    struct net_device *netdev;

    if (pdev != NULL) {
        ic = wlan_pdev_get_mlme_ext_obj(pdev);
        if (!ic)
            return;

        vap = TAILQ_FIRST(&ic->ic_vaps);
        while (vap != NULL) {
            /* osif_ioctl_delete_vap() destroy vap->iv_next information,
            so need to store next VAP address in vapnext */
            vapnext = TAILQ_NEXT(vap, iv_next);
            osifp = (osif_dev *)vap->iv_ifp;
            netdev = osifp->netdev;
            qdf_info("Remove interface on %s",netdev->name);
            dev_close(netdev);
            osif_ioctl_delete_vap(netdev);
            vap = vapnext;
        }
     }
}

static
void ath_vdev_recover_peer_release_handler(struct wlan_objmgr_psoc *psoc,
                                           void *obj, void *args)
{
    struct wlan_objmgr_peer *peer = (struct wlan_objmgr_peer *)obj;
    struct ieee80211vap *vap = NULL;
    struct wlan_objmgr_vdev *vdev = wlan_peer_get_vdev(peer);

    vap = wlan_vdev_get_mlme_ext_obj(vdev);
    if (!vap) {
        qdf_info("peer_release_handler: Null vap, vdev: 0x%pK", vdev);
        return;
    }
    ol_ath_find_logical_del_peer_and_release_ref(vap, peer->macaddr);
}

void ath_vdev_tx_stop(struct wlan_objmgr_psoc *psoc, void *obj, void *args)
{
    osif_dev  *osifp;
    struct net_device *netdev;
    struct vdev_osif_priv *vdev_osifp = NULL;
    struct wlan_objmgr_vdev *vdev = (struct wlan_objmgr_vdev *)obj;

    if(vdev != NULL) {
        vdev_osifp = wlan_vdev_get_ospriv(vdev);
        if (!vdev_osifp) {
            return;
        }
        osifp = (osif_dev *)vdev_osifp->legacy_osif_priv;
        if (osifp == NULL) {
            return;
        }
        netdev = osifp->netdev;

        /* Stop tx queue of the vdev */
        netif_stop_queue(netdev);
   }
}

void
ol_ath_tx_stop(struct ol_ath_soc_softc *soc)
{
    wlan_objmgr_iterate_obj_list(soc->psoc_obj, WLAN_VDEV_OP, ath_vdev_tx_stop,
                                            NULL, 1, WLAN_MLME_NB_ID);
}

void ath_vdev_timer_stop(struct wlan_objmgr_psoc *psoc, void *obj, void *args)
{
    struct wlan_objmgr_vdev *vdev = (struct wlan_objmgr_vdev *)obj;
    struct ieee80211vap *vap;

    if(vdev != NULL) {
       vap = wlan_vdev_get_mlme_ext_obj(vdev);
       if (!vap)
           return;

       mlme_ext_vap_recover(vap);
   }
}

void
ol_ath_target_timer_stop(struct ol_ath_soc_softc *soc)
{
    wlan_objmgr_iterate_obj_list(soc->psoc_obj, WLAN_VDEV_OP,
                                 ath_vdev_timer_stop,
                                 NULL, 1, WLAN_MLME_NB_ID);
}

void ath_vdev_recover_delete(struct wlan_objmgr_psoc *psoc, void *obj, void *args)
{
    struct wlan_objmgr_vdev *vdev = (struct wlan_objmgr_vdev *)obj;
    osif_dev  *osifp;
    struct vdev_osif_priv *vdev_osifp = NULL;
    struct net_device *netdev;
    u_int8_t id = 0;
    u_int8_t vdev_id = 0;
    struct ieee80211com *ic;
    struct ol_ath_vap_net80211 *avn;
    wlan_if_t vap;
    u_int8_t is_ifup = 0;
    enum wlan_vdev_state state;
    enum wlan_vdev_state substate;

    if(vdev != NULL) {
         state = wlan_vdev_mlme_get_state(vdev);
         if (state == WLAN_VDEV_S_START) {
             /*
              * Host has sent re/start req to firmware and is waiting for
              * re/start response, during which firmware recovery was
              * triggered.
              * Hence send proxy request failure event to vdev sm, so
              * as to move to init state
              */
             substate = wlan_vdev_mlme_get_substate(vdev);
             if (substate == WLAN_VDEV_SS_START_START_PROGRESS)
                 wlan_vdev_mlme_sm_deliver_evt(vdev,
                                        WLAN_VDEV_SM_EV_START_REQ_FAIL,
                                        0, NULL);
             else if(substate == WLAN_VDEV_SS_START_RESTART_PROGRESS)
                 wlan_vdev_mlme_sm_deliver_evt(vdev,
                                        WLAN_VDEV_SM_EV_RESTART_REQ_FAIL,
                                        0, NULL);
         }

         ic = wlan_vdev_get_ic(vdev);
         if (!ic) {
            return;
         }
         vdev_id = wlan_vdev_get_id(vdev);
         id = vdev_id;

         vdev_osifp = wlan_vdev_get_ospriv(vdev);
         if (!vdev_osifp) {
            return;
         }
         osifp = (osif_dev *)vdev_osifp->legacy_osif_priv;
         if(osifp == NULL) {
             return;
         }
         netdev = osifp->netdev;
         /* save the state before delete */
         is_ifup = IS_IFUP(netdev);

         vap = wlan_vdev_get_mlme_ext_obj(vdev);
         if (vap == NULL) {
              return;
         }

         /* Flush all the pending dp peers */
         cdp_vdev_flush_peers((struct cdp_soc_t *) wlan_psoc_get_dp_handle(wlan_pdev_get_psoc(ic->ic_pdev_obj)),
                              (struct cdp_vdev *) wlan_vdev_get_dp_handle(vap->vdev_obj), true);

         /* release references held for pending peer delete response first */
         wlan_objmgr_iterate_obj_list_all(psoc, WLAN_PEER_OP,
               ath_vdev_recover_peer_release_handler, NULL, 1,
               WLAN_MLME_NB_ID);

         avn = OL_ATH_VAP_NET80211(vap);

         mlme_ext_vap_recover(vap);

	 wlan_vdev_mlme_sm_deliver_evt(vap->vdev_obj, WLAN_VDEV_SM_EV_STOP_RESP,
                                       0, NULL);

         osif_delete_vap_recover(netdev);

         /* Update the vap mask as well */
         if (is_ifup) {
             if(vdev_id >= MAX_ID_PER_WORD) {
                 id -= MAX_ID_PER_WORD;
                 ic->id_mask_vap_downed[1] |= ( 1 << id);
             } else
                 ic->id_mask_vap_downed[0] |= ( 1 << id);
         }
    }
}

void ath_vdev_dump(struct wlan_objmgr_psoc *psoc, void *obj, void *args)
{
     struct wlan_objmgr_vdev *vdev = (struct wlan_objmgr_vdev *)obj;
     wlan_if_t vap = NULL;

     if(vdev != NULL) {
        vap = wlan_vdev_get_mlme_ext_obj(vdev);
        if(vap != NULL && (vap->iv_ic->ic_print_peer_refs)) {
            vap->iv_ic->ic_print_peer_refs(vap, vap->iv_ic, false);
        }
     }
}

int
__ol_vap_delete_on_rmmod(struct net_device *dev)
{
    ol_ath_soc_softc_t *soc = ath_netdev_priv(dev);

    rtnl_lock();
    /*
     * Don't add any more VAPs after this.
     * Else probably the detach should be done with rtnl_lock() held.
     */
    soc->sc_in_delete = 1;
     /* Iterate through all the vaps to delete each vap, it is lock free operation,
        the last argument indicates the lock free */
    wlan_objmgr_iterate_obj_list(soc->psoc_obj, WLAN_PDEV_OP, ath_vdev_delete,
                                            NULL, 1, WLAN_MLME_NB_ID);
    rtnl_unlock();

    return 0;
}

int
__ol_ath_detach(struct net_device *dev)
{
    ol_ath_soc_softc_t *soc = ath_netdev_priv(dev);
    int status, i = 0;
    int soc_num, soc_idx;
    struct wlan_objmgr_psoc *psoc;
    struct wlan_lmac_if_dfs_rx_ops *dfs_rx_ops;
    QDF_STATUS obj_status;
    struct target_psoc_info *tgt_psoc_info;
    uint8_t num_radios, num_pdevs;
    struct wlan_objmgr_pdev *pdev;

    psoc = soc->psoc_obj;
    tgt_psoc_info = (struct target_psoc_info *)wlan_psoc_get_tgt_if_handle(
                                                psoc);
    if (tgt_psoc_info == NULL) {
        qdf_info("%s: target_psoc_info is null", __func__);
        return -EINVAL;
    }

    qdf_vfs_clear_file_attributes((struct qdf_dev_obj *)
                                    &dev->dev.kobj,
                                    (struct qdf_vfs_attr *)
                                    &wifi_soc_group);
    num_radios =  target_psoc_get_num_radios(tgt_psoc_info);
    num_pdevs = wlan_psoc_get_pdev_count(psoc);
    /*
     * Ok if you are here, no communication with target as
     * already SUSPEND COmmand is gone to target.
     * So cleanup only on Host
     */
    for (i = 0; i < num_pdevs; i++) {
        struct ol_ath_softc_net80211 *scn;
        struct net_device *pdev_netdev;

        pdev = wlan_objmgr_get_pdev_by_id(psoc, i, WLAN_MLME_NB_ID);
        if (pdev == NULL) {
            qdf_info("%s: pdev object (id: %d) is NULL", __func__, i);
            continue;
        }

        scn = lmac_get_pdev_feature_ptr(pdev);
        if (scn == NULL) {
            wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_NB_ID);
            continue;
        }

        qdf_minidump_log((void *)scn, sizeof(*scn), "ol_ath_softc_net80211");

        pdev_netdev = scn->netdev;

        if (!soc->down_complete)
            ol_ath_thermal_mitigation_detach(scn, pdev_netdev);

#if WLAN_SPECTRAL_ENABLE
        os_if_spectral_netlink_deinit(pdev);
#endif /* WLAN_SPECTRAL_ENABLE */

        osif_detach(pdev_netdev);

        qdf_spinlock_destroy(&scn->scn_lock);

        qdf_vfs_clear_file_attributes((struct qdf_dev_obj *)
                                      &pdev_netdev->dev.kobj,
                                      (struct qdf_vfs_attr *)&wifi_attr_group);
#ifdef OL_ATH_SMART_LOGGING
    {
        void *dbglog_handle;

        dbglog_handle = target_psoc_get_dbglog_hdl(tgt_psoc_info);
        if (dbglog_handle)
            fwdbg_smartlog_deinit(dbglog_handle, soc);
    }
#endif /* OL_ATH_SMART_LOGGING */

#if defined(OL_ATH_SMART_LOGGING) && !defined(REMOVE_PKT_LOG)
    {
        struct ieee80211com *ic = &scn->sc_ic;
        qdf_mutex_destroy(&ic->smart_log_fw_pktlog_mutex);
    }
#endif /* REMOVE_PKT_LOG */

        status = ol_ath_pdev_detach(scn, 1); /* Force Detach */
        if (pdev_netdev->reg_state == NETREG_REGISTERED)
            unregister_netdev(pdev_netdev);
        ol_ath_pdev_proc_detach(&scn->sc_ic);
#if PERF_FIND_WDS_NODE
        wds_table_uninit(&scn->scn_wds_table);
#endif
        if (pdev->pdev_nif.pdev_ospriv) {
            qdf_mem_free(pdev->pdev_nif.pdev_ospriv);
        }

        dfs_rx_ops = wlan_lmac_if_get_dfs_rx_ops(psoc);
        if (dfs_rx_ops && dfs_rx_ops->dfs_stop) {
            dfs_rx_ops->dfs_stop(pdev);
        }

        dispatcher_pdev_close(pdev);
        if (wlan_pdev_get_tgt_if_handle(pdev) != NULL) {
            target_if_free_pdev_tgt_info(pdev);
        }
        wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_NB_ID);
        /* Delete PDEV object */
        obj_status = wlan_objmgr_pdev_obj_delete(pdev);
        if (obj_status == QDF_STATUS_COMP_ASYNC) {
            qdf_info("PDEV (partially deleted) ");
        }
        else if( obj_status == QDF_STATUS_E_FAILURE) {
            qdf_info("PDEV (deletion) failed ");
        }

        qdf_mem_free(scn->sc_osdev);
        free_netdev(pdev_netdev);
    }

#ifdef ATH_SUPPORT_LINUX_STA
#ifdef CONFIG_SYSCTL
    ath_dynamic_sysctl_unregister(ATH_DEV_TO_SC(soc->sc_dev));
#endif
#endif

#ifndef NO_SIMPLE_CONFIG
    unregister_simple_config_callback(dev->name);
#endif

#ifdef QVIT
    qvit_cleanup();
#endif

    if (dev->reg_state == NETREG_REGISTERED)
        unregister_netdev(dev);

#if OS_SUPPORT_ASYNC_Q
    OS_MESGQ_DRAIN(&soc->sc_osdev->async_q,NULL);
    OS_MESGQ_DESTROY(&soc->sc_osdev->async_q);
#endif

#if 0
    qdf_info("%s: init tx/rx cleanup TODO\n", __func__);
    soc->sc_ops->rx_cleanup(soc->sc_dev);
    soc->sc_ops->tx_cleanup(soc->sc_dev);
#endif

    ath_adhoc_netlink_delete();

#if ATH_SUPPORT_LOWI
    ath_lowi_if_netlink_delete();
#endif

#if ATH_SSID_STEERING
    ath_ssid_steering_netlink_delete();
#endif

#if ATH_PARAMETER_API
    ath_papi_netlink_delete();
#endif

#if ATH_RXBUF_RECYCLE
    ath_rxbuf_recycle_destroy(soc->sc_osdev);
#endif /* ATH_RXBUF_RECYCLE */

    ald_destroy_netlink();

    status = ol_ath_soc_detach(soc, 1); /* Force Detach */

    soc_idx = 0;
    soc_num = 0;
    while ((soc_num < ol_num_global_soc) && (soc_idx < GLOBAL_SOC_SIZE)) {
        if (ol_global_soc[soc_idx] == soc) {
            ol_global_soc[soc_idx] = NULL;
            break;
        } else if (ol_global_soc[soc_idx] != NULL) {
            soc_num++;
        }
        soc_idx++;
    }

    ol_num_global_soc--;

#if WMI_RECORDING
    if (soc->wmi_proc_entry){
        remove_proc_entry(dev->name, NULL);
    }
#endif

#ifdef EPPING_TEST
    qdf_spinlock_destroy(&soc->data_lock);
    qdf_timer_stop(&soc->epping_timer);
    soc->epping_timer_running = 0;
    skb_queue_purge(&soc->epping_nodrop_queue);
#endif

    wlan_global_lmac_if_close(psoc);

    wlan_objmgr_psoc_check_for_peer_leaks(psoc);
    wlan_objmgr_psoc_check_for_vdev_leaks(psoc);
    wlan_objmgr_psoc_check_for_pdev_leaks(psoc);

    target_if_free_psoc_tgt_info(psoc);
    wlan_objmgr_psoc_obj_delete(psoc);
    free_netdev(dev);

    return status;
}

void
__ol_target_paused_event(ol_ath_soc_softc_t *soc)
{
    wake_up(&soc->sc_osdev->event_queue);
}

void
__ol_ath_suspend_resume_attach(struct net_device *dev)
{
    struct ol_ath_softc_net80211 *scn = ath_netdev_priv(dev);
    ol_ath_suspend_resume_attach(scn);
}

int
__ol_ath_suspend(struct net_device *dev)
{
    ol_ath_soc_softc_t *soc = ath_netdev_priv(dev);
    u_int32_t  timeleft;
    struct pdev_op_args oper;

    oper.type = PDEV_ITER_PDEV_NETDEV_STOP;
    wlan_objmgr_iterate_obj_list(soc->psoc_obj, WLAN_PDEV_OP,
               wlan_pdev_operation, &oper, 1, WLAN_MLME_NB_ID);

    /* Suspend target with disable_target_intr set to 0 */
    if (!ol_ath_suspend_target(soc, 0)) {
        qdf_info("waiting for target paused event from target\n");
        /* wait for the event from Target*/
        timeleft = wait_event_interruptible_timeout(soc->sc_osdev->event_queue,
                                                    (soc->is_target_paused == TRUE),
                                                    200);
        if(!timeleft || signal_pending(current)) {
            qdf_info("Failed to receive target paused event \n");
            return -EIO;
        }
        /*
         * reset is_target_paused and host can check that in next time,
         * or it will always be TRUE and host just skip the waiting
         * condition, it causes target assert due to host already suspend
         */
        soc->is_target_paused = FALSE;
        return (0);
    }
    return (-1);
}

int
__ol_ath_resume(struct net_device *dev)
{
    ol_ath_soc_softc_t *soc = ath_netdev_priv(dev);
    struct pdev_op_args oper;

    if (ol_ath_resume_target(soc)) {
        return -1;
    }
    oper.type = PDEV_ITER_PDEV_NETDEV_OPEN;
    wlan_objmgr_iterate_obj_list(soc->psoc_obj, WLAN_PDEV_OP,
               wlan_pdev_operation, &oper, 1, WLAN_MLME_NB_ID);

    return 0;
}


void __ol_ath_target_status_update(struct net_device *dev, ol_target_status status)
{
    ol_ath_soc_softc_t *soc = ath_netdev_priv(dev);
    ol_ath_target_status_update(soc,status);
}

#if !NO_SIMPLE_CONFIG
/*
 * Handler for front panel SW jumpstart switch
 */


void *ath_vdev_jumpstart_intr(struct wlan_objmgr_psoc *psoc, void *obj, void *args)
{
    osif_dev  *osifp;
    struct wlan_objmgr_vdev *vdev = (struct wlan_objmgr_vdev *)obj;
    struct vdev_osif_priv *vdev_osifp = NULL;
    uint32_t push_time = *((uint32_t *)args);
    u_int32_t push_duration;
    wlan_if_t vap;

    if(vdev != NULL) {
        vdev_osifp = wlan_vdev_get_ospriv(vdev);
        if (!vdev_osifp) {
            return;
        }
        osifp = (osif_dev *)vdev_osifp->legacy_osif_priv;
        if(osifp == NULL) {
            return;
        }
        if (push_time) {
            push_duration = push_time;
        } else {
            push_duration = 0;
        }
        vap = wlan_vdev_get_mlme_ext_obj(vdev);
        if (vap == NULL) {
           return;
        }
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
        if (!ieee80211_vap_nopbn_is_set(vap))
        {
#endif
           /* Since we are having single physical push button on device,
              and for push button + muti bss combination mode we will be
              have limitations, we designed that,physical push button
              notification will be sent to first AP vap(main BSS) and all
              sta vaps.
            */
           if ((wlan_vdev_get_opmode(vdev) != WLAN_OPMODE_AP) ||
                     pdev->pdev_nif.notified_ap_vdev == 0 ) {
               qdf_info("SC Pushbutton Notify on %s for %d sec(s)"
                      "and the vap %pK dev %pK:",dev->name, push_duration, vap,
                      (struct net_device *)(osifp)->netdev);
               osif_notify_push_button (osifp, push_duration);
               if (wlan_vdev_get_opmode(vdev) == WLAN_OPMODE_AP )
                   pdev->pdev_nif.notified_ap_vdev = 1;
               }
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
        }
#endif
    }
}

static irqreturn_t
jumpstart_intr (int cpl, void *dev_id, struct pt_regs *regs, void *push_time)
{
    struct net_device *dev = dev_id;
    ol_ath_soc_softc_t *soc = ath_netdev_priv(dev);
    /*
    ** Iterate through all VAPs, since any of them may have WPS enabled
    */
    scn->sc_pdev->pdev_nif.notified_ap_vdev = 0;
    wlan_objmgr_iterate_obj_list(soc->psoc_obj, WLAN_VDEV_OP,
                                 ath_vdev_jumpstart_intr, push_time, 1,
                                 WLAN_MLME_NB_ID);
    return IRQ_HANDLED;
}
#endif

#if OS_SUPPORT_ASYNC_Q
static void os_async_mesg_handler( void  *ctx, u_int16_t  mesg_type, u_int16_t  mesg_len, void  *mesg )
{
    if (mesg_type == OS_SCHEDULE_ROUTING_MESG_TYPE) {
        os_schedule_routing_mesg  *s_mesg = (os_schedule_routing_mesg *) mesg;
        s_mesg->routine(s_mesg->context, NULL);
    }
}
#endif

int
__ol_ath_check_wmi_ready(ol_ath_soc_softc_t *soc)
{
    struct target_psoc_info *tgt_hdl;
    struct wlan_objmgr_psoc *psoc = soc->psoc_obj;
    QDF_STATUS status;

    if (!psoc) {
        qdf_info("%s:psoc is null", __func__);
        return -EINVAL;
    }

    tgt_hdl = (struct target_psoc_info *)wlan_psoc_get_tgt_if_handle(
                                        psoc);
    if(!tgt_hdl) {
        qdf_info("%s:target_psoc_info is null", __func__);
	return -EINVAL;
    }

    /*It is observed in some high CPU load scenarios, WMI_INIT is sent late
     * proivinding aditional 5 sec time to send WMI_INIT and receive WMI
     * ready event only for these conditions
     */

    status = qdf_wait_for_event_completion(&tgt_hdl->info.event,
                                   (tgt_hdl->info.wmi_timeout+5)*1000);

    if (status != QDF_STATUS_SUCCESS)
    {
        qdf_info("WMI is not ready\n");
        return -EIO;
    }
#if !ENABLE_10_4_FW_HEADERS
/* Temporary added since missing in FW 1.0.0 headers. Remove after it added in
 * FW header file */
#define SOC_ABI_VERSION        3
#endif

    /* ABI version check happens in WMI for target supporting ext */
    if ((init_deinit_is_service_ext_msg(psoc, tgt_hdl) !=
			QDF_STATUS_SUCCESS) &&
	(tgt_hdl->info.version.abi_ver != SOC_ABI_VERSION)) {
        qdf_info("ABI Version mismatch: Host(0x%x), Target(0x%x)",
                SOC_ABI_VERSION, tgt_hdl->info.version.abi_ver);
    }

    /* Return failure if FW anounced 160/80+80 MHz wireless modes
     * and vhtcap entries are either invalid or out of sync.
     */
    if (wlan_psoc_nif_op_flag_get(soc->psoc_obj, WLAN_SOC_OP_VHT_INVALID_CAP)) {
	qdf_info(" FW VHT cap invalid \n");
        return -EPERM;
    }

    return EOK;
}

extern void ath_sysfs_diag_init(ol_ath_soc_softc_t *soc);
extern void ath_sysfs_diag_fini(ol_ath_soc_softc_t *soc);

void
ol_ath_diag_user_agent_init(ol_ath_soc_softc_t *soc)
{
#if defined(CONFIG_ATH_SYSFS_DIAG_SUPPORT)
    ath_sysfs_diag_init(soc);
#endif
}

void
ol_ath_diag_user_agent_fini(ol_ath_soc_softc_t *soc)
{
#if defined(CONFIG_ATH_SYSFS_DIAG_SUPPORT)
    ath_sysfs_diag_fini(soc);
#endif
}


#if defined(HIF_PCI)
/* BEGIN AR9888v1 WORKAROUND for EV#106293 { */

/*
 * This is a workaround for a HW issue in AR9888v1 in which there is
 * some chance that a write from Host to Target register at address X
 * may have a side effect of writing a value to X+4 (as well as the
 * requested write to X).  The value written to X+4 is whatever value
 * was read by the Host 3 reads prior.  The write to X+4 is just as
 * if software * had done it intentionally (so all effects that would
 * normally occur if that register were written do occur).
 *
 * Example1: Host tries to clear a few bits in a Copy Engine's HOST_IS
 * register (offset 0x30). As a side effect, the CE's MISC_IE register
 * (offset 0x34) is overwritten with a recently read value.
 *
 * Example2: A CE is used for Host to Target transfers, so the
 * Source Ring is maintained by the Host.  When the Host writes the
 * Source Ring Write Index, the Destination Ring Write Index is corrupted.
 *
 * The general workaround is to
 *  A) force the third read prior to a write(X) to be a read(X+4).
 *     That way, when X+4 is overwritten, it will be overwritten
 *     with the value that was there originally.
 *  B) Use a dedicated spin lock and block interrupts in order to
 *     guarantee that the above 3 reads + write occur atomically
 *     with respect to other writes from Host to Target.
 * In addition, special handling is needed for cases when re-writing
 * a value to the register at X+4 has side effects.  The only case
 * of this that occurs in practice is Example2, above.  If we simply
 * allow hardware to re-commit the value in DST_WR_IDX we may run
 * into problems: The Target may update DST_WR_IDX after our first
 * read but before the write. In that case, our re-commit is a
 * stale value. This has a catostophic side-effect because the CE
 * interprets this as a Destination Overflow.  The CE reacts by
 * setting the DST_OVFL bit in MISC_IS and halting the CE. It can
 * only be restarted with a very expensive operation of flushing,
 * re-queueing descriptors (and per-transfer software arguments)
 * and then re-starting the CE.  Rather than attempt this expensive
 * recovery process, we try to avoid this situation by synchronizing
 * Host writes(SR_WR_IDX) with Target writes(DST_WR_IDX).  The
 * currently implementation uses the low bit of DST_WATERMARK
 * register for this synchronization and it relies on reasonable
 * timing characteristics (rather than a stronger synchronization
 * algorithm --  Dekker's, etc.).  Because we rely on timing -- as
 * well as to minimize busy waiting on the Target side -- both
 * Host and Target disable interrupts for the duration of the
 * workaround.
 *
 * The intent is to fix this in HW so this is a temporary workaround.
 */


/*
 * Allow this workaround to be disabled when the driver is loaded
 * by adding "war1=0" to "insmod umac".  There is still a bit of
 * additional overhead.  Can be disabled on the small portion (10%?)
 * of boards that don't suffer from EV#106293.
 */
unsigned int war1 = 1;
module_param(war1, int, 0644);

/*
 * Allow to use CDC WAR which reaches less peak throughput but allow
 * SoC to go to sleep. By default it is disabled.
 */
unsigned int war1_allow_sleep = 0;
module_param(war1_allow_sleep, int, 0644);

DEFINE_SPINLOCK(pciwar_lock);

void
WAR_PCI_WRITE32(char *addr, u32 offset, u32 value)
{
#ifdef QCA_PARTNER_PLATFORM
    WAR_PLTFRM_PCI_WRITE32(addr, offset, value, war1);
#else
    if (war1) {
        unsigned long irq_flags;

        spin_lock_irqsave(&pciwar_lock, irq_flags);

        (void)ioread32((void __iomem *)(addr+offset+4)); /* 3rd read prior to write */
        (void)ioread32((void __iomem *)(addr+offset+4)); /* 2nd read prior to write */
        (void)ioread32((void __iomem *)(addr+offset+4)); /* 1st read prior to write */
        iowrite32((u32)(value), (void __iomem *)(addr+offset));

        spin_unlock_irqrestore(&pciwar_lock, irq_flags);
    } else {
        iowrite32((u32)(value), (void __iomem *)(addr+offset));
    }
#endif
}
EXPORT_SYMBOL(war1);
EXPORT_SYMBOL(war1_allow_sleep);
EXPORT_SYMBOL(WAR_PCI_WRITE32);
/* } END AR9888v1 WORKAROUND for EV#106293 */
#endif

/* Update host conig based on Target info */
void ol_ath_host_config_update(ol_ath_soc_softc_t *soc)
{
    uint32_t target_version;

    target_version = lmac_get_tgt_version(soc->psoc_obj);
    if (target_version == AR900B_DEV_VERSION || (target_version == AR9888_REV2_VERSION) || (target_version == AR9887_REV1_VERSION || target_version == QCA9984_DEV_VERSION || target_version == IPQ4019_DEV_VERSION || target_version == QCA9888_DEV_VERSION)) {
        /* AR9888v1 CDC WORKAROUND for EV#106293 */
#if defined(HIF_PCI)
        hif_ce_war_disable();
        qdf_info(KERN_INFO"\n CE WAR Disabled\n");
#endif
    }

#if QCA_LTEU_SUPPORT
    if (target_version != AR9888_REV2_VERSION) {
        wlan_psoc_nif_feat_cap_clear(soc->psoc_obj, WLAN_SOC_F_LTEU_SUPPORT);
    }
#endif
}

EXPORT_SYMBOL(__ol_ath_attach);
EXPORT_SYMBOL(__ol_ath_detach);
EXPORT_SYMBOL(__ol_ath_target_status_update);
EXPORT_SYMBOL(__ol_ath_check_wmi_ready);
EXPORT_SYMBOL(ol_ath_diag_user_agent_init);
EXPORT_SYMBOL(ol_ath_diag_user_agent_fini);
EXPORT_SYMBOL(ol_ath_host_config_update);

#ifndef REMOVE_PKT_LOG
extern struct ol_pl_os_dep_funcs *g_ol_pl_os_dep_funcs;
#endif

#if FW_CODE_SIGN


const static fw_device_id fw_auth_supp_devs[] =
    {
        {0x3Cu, "PEREGRINE",    FW_IMG_MAGIC_PEREGRINE},
        {0x50u, "SWIFT",        FW_IMG_MAGIC_SWIFT},
        {0x40u, "BEELINER",     FW_IMG_MAGIC_BEELINER},
        {0x46u, "CASCADE",      FW_IMG_MAGIC_CASCADE},
        {0x12ef,"DAKOTA",       FW_IMG_MAGIC_DAKOTA},
        {0x0u,  "UNSUPP",       FW_IMG_MAGIC_UNKNOWN}
    };

const unsigned char test_target_wlan_x509[] = {
  0x30, 0x82, 0x05, 0xa2, 0x30, 0x82, 0x03, 0x8a, 0xa0, 0x03, 0x02, 0x01,
  0x02, 0x02, 0x09, 0x00, 0xaa, 0x19, 0xdc, 0x72, 0xfd, 0x7f, 0xf0, 0x61,
  0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01,
  0x05, 0x05, 0x00, 0x30, 0x60, 0x31, 0x12, 0x30, 0x10, 0x06, 0x03, 0x55,
  0x04, 0x0a, 0x0c, 0x09, 0x4d, 0x61, 0x67, 0x72, 0x61, 0x74, 0x68, 0x65,
  0x61, 0x31, 0x1c, 0x30, 0x1a, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0c, 0x13,
  0x47, 0x6c, 0x61, 0x63, 0x69, 0x65, 0x72, 0x20, 0x73, 0x69, 0x67, 0x6e,
  0x69, 0x6e, 0x67, 0x20, 0x6b, 0x65, 0x79, 0x31, 0x2c, 0x30, 0x2a, 0x06,
  0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x09, 0x01, 0x16, 0x1d,
  0x73, 0x6c, 0x61, 0x72, 0x74, 0x69, 0x62, 0x61, 0x72, 0x74, 0x66, 0x61,
  0x73, 0x74, 0x40, 0x6d, 0x61, 0x67, 0x72, 0x61, 0x74, 0x68, 0x65, 0x61,
  0x2e, 0x68, 0x32, 0x67, 0x32, 0x30, 0x20, 0x17, 0x0d, 0x31, 0x34, 0x30,
  0x38, 0x31, 0x33, 0x31, 0x32, 0x30, 0x34, 0x33, 0x39, 0x5a, 0x18, 0x0f,
  0x32, 0x31, 0x31, 0x34, 0x30, 0x37, 0x32, 0x30, 0x31, 0x32, 0x30, 0x34,
  0x33, 0x39, 0x5a, 0x30, 0x60, 0x31, 0x12, 0x30, 0x10, 0x06, 0x03, 0x55,
  0x04, 0x0a, 0x0c, 0x09, 0x4d, 0x61, 0x67, 0x72, 0x61, 0x74, 0x68, 0x65,
  0x61, 0x31, 0x1c, 0x30, 0x1a, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0c, 0x13,
  0x47, 0x6c, 0x61, 0x63, 0x69, 0x65, 0x72, 0x20, 0x73, 0x69, 0x67, 0x6e,
  0x69, 0x6e, 0x67, 0x20, 0x6b, 0x65, 0x79, 0x31, 0x2c, 0x30, 0x2a, 0x06,
  0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x09, 0x01, 0x16, 0x1d,
  0x73, 0x6c, 0x61, 0x72, 0x74, 0x69, 0x62, 0x61, 0x72, 0x74, 0x66, 0x61,
  0x73, 0x74, 0x40, 0x6d, 0x61, 0x67, 0x72, 0x61, 0x74, 0x68, 0x65, 0x61,
  0x2e, 0x68, 0x32, 0x67, 0x32, 0x30, 0x82, 0x02, 0x22, 0x30, 0x0d, 0x06,
  0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00,
  0x03, 0x82, 0x02, 0x0f, 0x00, 0x30, 0x82, 0x02, 0x0a, 0x02, 0x82, 0x02,
  0x01, 0x00, 0xd4, 0xfe, 0xc2, 0x51, 0x6d, 0xd6, 0x4c, 0xd3, 0xb3, 0x6c,
  0xa3, 0x29, 0x58, 0x44, 0x11, 0x3f, 0x58, 0x6b, 0x65, 0xb0, 0xf4, 0xdb,
  0x60, 0xe5, 0x3a, 0x3a, 0x31, 0x58, 0xb5, 0x2b, 0x5f, 0x20, 0x64, 0x55,
  0xe8, 0x29, 0x55, 0x0f, 0xa0, 0x66, 0xa6, 0x02, 0x63, 0x40, 0x72, 0xd5,
  0x35, 0xa9, 0x5a, 0x04, 0x39, 0x2b, 0x03, 0xe1, 0x8b, 0xfa, 0xf6, 0x39,
  0xc5, 0x5d, 0xa9, 0x8d, 0x82, 0x0c, 0xd5, 0x32, 0x42, 0x57, 0x4c, 0x56,
  0x7b, 0x69, 0xf8, 0x5a, 0x28, 0x3a, 0x9c, 0x08, 0x2a, 0x1a, 0x9a, 0x4e,
  0xe7, 0xfa, 0x89, 0x4d, 0x14, 0x5d, 0x8c, 0x16, 0xb2, 0xfd, 0x94, 0x60,
  0xd4, 0xa2, 0x64, 0x92, 0x30, 0xc7, 0x72, 0x7f, 0x6a, 0x17, 0xb5, 0x76,
  0x5f, 0xaf, 0x5c, 0x2f, 0x06, 0x16, 0x9a, 0x27, 0xc7, 0xd0, 0xae, 0xe3,
  0x0e, 0xba, 0x1d, 0xaf, 0x17, 0xf1, 0x5d, 0xa6, 0x0d, 0x67, 0x0b, 0xf2,
  0x2d, 0x60, 0xc1, 0x2e, 0x0c, 0x5c, 0xdf, 0xed, 0x9e, 0x9d, 0x60, 0xd1,
  0x8e, 0x48, 0xf3, 0x4d, 0xc7, 0xa3, 0x41, 0x02, 0x53, 0x5f, 0xb4, 0xe9,
  0xcc, 0x60, 0x04, 0x47, 0xc7, 0x27, 0x1e, 0xf4, 0x65, 0xbe, 0x90, 0xb7,
  0x97, 0x3c, 0x65, 0x0b, 0xee, 0x6b, 0x8f, 0xe3, 0xfd, 0xd5, 0x78, 0x2b,
  0xb7, 0x09, 0x2e, 0xd9, 0x5e, 0x2e, 0xae, 0x80, 0x0d, 0xa5, 0x74, 0xb7,
  0xbc, 0x8d, 0x10, 0x21, 0x8a, 0x35, 0x63, 0x5a, 0x27, 0x94, 0xe9, 0x7a,
  0x5e, 0x3a, 0x91, 0x75, 0xad, 0xc2, 0xe4, 0x66, 0xbd, 0x49, 0x1f, 0x20,
  0x24, 0x3c, 0xad, 0x40, 0x57, 0x43, 0x29, 0x2f, 0x53, 0xad, 0xa9, 0xf6,
  0x26, 0xad, 0x5e, 0x37, 0xd4, 0x34, 0xab, 0x45, 0xbe, 0x41, 0x89, 0xba,
  0x6d, 0x15, 0xba, 0x26, 0xfd, 0xbf, 0x59, 0x28, 0x94, 0x2d, 0xb2, 0x55,
  0xcf, 0x46, 0x60, 0x5c, 0xe6, 0x20, 0x30, 0xee, 0x45, 0xae, 0x81, 0x86,
  0x14, 0xd9, 0x83, 0x85, 0x3e, 0x32, 0x53, 0xe3, 0xf8, 0x70, 0xb1, 0xb7,
  0xf0, 0x5d, 0xc2, 0x71, 0xae, 0x7b, 0x7e, 0x48, 0x6c, 0x0d, 0x7c, 0x83,
  0x27, 0xea, 0xc5, 0xc7, 0xca, 0x7a, 0x51, 0xd8, 0x2d, 0x55, 0x5b, 0x68,
  0xa9, 0xca, 0x6f, 0xbb, 0x45, 0x05, 0x61, 0x57, 0xf7, 0x89, 0xa0, 0xd9,
  0xcc, 0xbd, 0x81, 0x6b, 0xde, 0xf4, 0x47, 0xad, 0x00, 0xc0, 0x43, 0xe1,
  0x97, 0xc2, 0xc2, 0xbb, 0x0b, 0x88, 0x07, 0x39, 0x8e, 0x86, 0x28, 0x84,
  0xcb, 0xdc, 0x64, 0x5b, 0x08, 0xc8, 0xad, 0x55, 0xb6, 0x02, 0xa7, 0xa7,
  0xa7, 0x01, 0x7d, 0xc0, 0xca, 0xdb, 0x56, 0xf7, 0x73, 0xc9, 0xc8, 0xf2,
  0x33, 0xe9, 0xd6, 0xf1, 0x47, 0xcc, 0xd3, 0x45, 0xdb, 0x6d, 0x05, 0x31,
  0xe6, 0x81, 0x85, 0x9c, 0x46, 0x47, 0x87, 0x57, 0x1e, 0x97, 0xae, 0x72,
  0x6d, 0xb7, 0x9b, 0x6b, 0x8b, 0xa0, 0x90, 0xdc, 0x47, 0x20, 0xd4, 0x1b,
  0x20, 0xb9, 0x0c, 0x8e, 0x9d, 0x31, 0xce, 0xca, 0xe6, 0x24, 0x2d, 0xcb,
  0x6d, 0x54, 0xbe, 0xab, 0x1e, 0xaa, 0xbf, 0x95, 0xa8, 0x55, 0xca, 0x32,
  0x53, 0xe2, 0x02, 0xbd, 0x43, 0x98, 0x04, 0xef, 0x62, 0x0f, 0xe9, 0x0f,
  0x37, 0xbb, 0xdd, 0x8c, 0x08, 0x5f, 0xab, 0x04, 0x8d, 0x12, 0x48, 0x16,
  0xd4, 0x20, 0xee, 0x5a, 0xf3, 0xfb, 0x7d, 0x52, 0x1d, 0x48, 0xcf, 0x2c,
  0x25, 0xa8, 0x4d, 0xd3, 0x80, 0x92, 0x8e, 0x21, 0xe9, 0x9b, 0xfe, 0x58,
  0x15, 0x61, 0xa8, 0xd0, 0x9e, 0x51, 0xe2, 0xa9, 0x7d, 0xc2, 0x7c, 0x8c,
  0x4b, 0xf0, 0x3a, 0x6d, 0xcd, 0xa3, 0xc0, 0xba, 0xbb, 0x01, 0x5c, 0x7f,
  0x36, 0x91, 0x7f, 0x4a, 0x63, 0xe6, 0x83, 0xaf, 0x61, 0x04, 0xc0, 0x66,
  0xa7, 0xef, 0xbc, 0xa7, 0xbe, 0x68, 0x39, 0x80, 0xd6, 0xad, 0x02, 0x03,
  0x01, 0x00, 0x01, 0xa3, 0x5d, 0x30, 0x5b, 0x30, 0x0c, 0x06, 0x03, 0x55,
  0x1d, 0x13, 0x01, 0x01, 0xff, 0x04, 0x02, 0x30, 0x00, 0x30, 0x0b, 0x06,
  0x03, 0x55, 0x1d, 0x0f, 0x04, 0x04, 0x03, 0x02, 0x07, 0x80, 0x30, 0x1d,
  0x06, 0x03, 0x55, 0x1d, 0x0e, 0x04, 0x16, 0x04, 0x14, 0x56, 0x47, 0xf1,
  0xdb, 0xd8, 0xf7, 0x0c, 0xe0, 0xd1, 0x74, 0xfd, 0x2c, 0x62, 0x86, 0x9d,
  0x6e, 0xb9, 0xb4, 0x97, 0x48, 0x30, 0x1f, 0x06, 0x03, 0x55, 0x1d, 0x23,
  0x04, 0x18, 0x30, 0x16, 0x80, 0x14, 0x56, 0x47, 0xf1, 0xdb, 0xd8, 0xf7,
  0x0c, 0xe0, 0xd1, 0x74, 0xfd, 0x2c, 0x62, 0x86, 0x9d, 0x6e, 0xb9, 0xb4,
  0x97, 0x48, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d,
  0x01, 0x01, 0x05, 0x05, 0x00, 0x03, 0x82, 0x02, 0x01, 0x00, 0x11, 0x45,
  0xf2, 0x16, 0x31, 0xb0, 0xac, 0x53, 0x01, 0x2f, 0x8b, 0xac, 0x33, 0xd2,
  0xed, 0x5b, 0x4b, 0x0f, 0x62, 0x09, 0xcf, 0xbc, 0x9a, 0xf0, 0x6a, 0x58,
  0xc0, 0x4d, 0x55, 0x68, 0x3b, 0x2e, 0x66, 0x27, 0x24, 0xab, 0x14, 0xd6,
  0x7c, 0x4c, 0x1b, 0x24, 0x7e, 0x43, 0x96, 0x74, 0xb8, 0x65, 0x8f, 0x25,
  0x90, 0x32, 0xb8, 0xfd, 0xc9, 0x99, 0x5b, 0x15, 0xaf, 0x77, 0x02, 0x46,
  0x3b, 0xb6, 0x0d, 0xfa, 0xcb, 0x8b, 0xb7, 0xaa, 0x9d, 0x31, 0x16, 0xc4,
  0xfb, 0xfe, 0x44, 0xbe, 0xd5, 0x58, 0xe0, 0x11, 0x6c, 0x37, 0x54, 0x0d,
  0xd7, 0xb9, 0x95, 0x3d, 0x1f, 0xf9, 0x7b, 0x81, 0xfa, 0xf1, 0xd8, 0x19,
  0x7a, 0xff, 0x29, 0xa5, 0xac, 0x69, 0xe3, 0xae, 0x86, 0x15, 0x5a, 0x86,
  0xd4, 0xb6, 0xf7, 0x09, 0x5d, 0x8c, 0xf3, 0x39, 0x88, 0xc6, 0xa7, 0x39,
  0x37, 0xc9, 0xb2, 0x30, 0x5b, 0x11, 0xb6, 0xf1, 0x92, 0xcb, 0x88, 0x82,
  0x70, 0x0e, 0x0a, 0x5e, 0x11, 0x83, 0xec, 0xab, 0x09, 0x39, 0xf6, 0x6c,
  0x42, 0x52, 0x39, 0xa9, 0x58, 0xa6, 0x2e, 0x58, 0x32, 0x95, 0x0e, 0x13,
  0x89, 0x9d, 0x48, 0x3c, 0x1f, 0xfa, 0x6b, 0xaa, 0x59, 0x6d, 0xd6, 0xac,
  0x53, 0x56, 0xb0, 0x25, 0x72, 0x9d, 0xf0, 0x38, 0xb1, 0x96, 0x67, 0x0a,
  0xdb, 0x36, 0x34, 0x90, 0x3f, 0x6a, 0x18, 0xa1, 0x96, 0xbc, 0xfa, 0x72,
  0x9c, 0x49, 0x6a, 0xbb, 0x9d, 0xe9, 0xc3, 0xcf, 0xdb, 0x56, 0xb8, 0x46,
  0x15, 0x35, 0x9d, 0x6a, 0x2a, 0x07, 0xf2, 0xfa, 0x58, 0xc3, 0x4f, 0x52,
  0x74, 0x1c, 0xe4, 0x92, 0xaa, 0x26, 0x40, 0xac, 0xaa, 0xe8, 0x50, 0x77,
  0xdc, 0x07, 0x82, 0x59, 0x6e, 0xd8, 0xc9, 0x21, 0xaa, 0x95, 0x00, 0xf8,
  0x6a, 0xc0, 0x9e, 0x88, 0xb1, 0xa4, 0x26, 0xba, 0xef, 0x52, 0x9d, 0x17,
  0x33, 0x0b, 0xde, 0x1a, 0xa8, 0x9c, 0xe5, 0x72, 0x57, 0xd2, 0xad, 0x23,
  0xae, 0x75, 0x30, 0x65, 0xf5, 0xcb, 0xb6, 0xdf, 0x24, 0x3a, 0xb2, 0x3b,
  0xa7, 0xe2, 0x64, 0xcf, 0x65, 0x06, 0x4b, 0xfa, 0x77, 0xa3, 0xc8, 0x16,
  0x73, 0x25, 0x32, 0x8a, 0x96, 0x50, 0x35, 0x65, 0x43, 0x27, 0x06, 0x56,
  0x45, 0xbd, 0x20, 0xc9, 0xaf, 0x98, 0x20, 0x78, 0xb0, 0xca, 0x47, 0x93,
  0x1f, 0x82, 0x0b, 0x77, 0xaa, 0x85, 0xf7, 0x9b, 0xa3, 0xb8, 0xb7, 0xc3,
  0x57, 0xa1, 0x5d, 0x0c, 0x5c, 0x36, 0x32, 0xd4, 0x19, 0x4f, 0x98, 0xa6,
  0x34, 0x72, 0xe7, 0xb3, 0xdb, 0xd4, 0xed, 0xc9, 0x98, 0x44, 0x71, 0x97,
  0xc4, 0x94, 0x46, 0x9e, 0xdd, 0x64, 0x8a, 0x79, 0xee, 0x90, 0x5b, 0xbb,
  0xc3, 0xc7, 0xde, 0x20, 0xb6, 0x78, 0x66, 0xae, 0xd5, 0x98, 0x5c, 0x20,
  0x9c, 0x75, 0xfe, 0x1a, 0xd4, 0x50, 0xd0, 0x8b, 0x3b, 0xee, 0x55, 0x0c,
  0x17, 0xf8, 0x4c, 0x00, 0x47, 0x33, 0x59, 0xcf, 0x97, 0x13, 0x29, 0x7e,
  0xb9, 0xbd, 0x86, 0x01, 0x82, 0x94, 0x26, 0x05, 0x96, 0x32, 0xf0, 0xf3,
  0x03, 0xf7, 0x2c, 0x1f, 0xcd, 0x64, 0x83, 0xcf, 0xc0, 0xa9, 0x7b, 0xcb,
  0x34, 0x3c, 0x72, 0x88, 0xec, 0x81, 0x96, 0x30, 0x6c, 0x3a, 0xf0, 0xe2,
  0x09, 0x7a, 0x49, 0x2d, 0x58, 0x50, 0x9b, 0x1e, 0xc0, 0x26, 0xc4, 0x3f,
  0xd1, 0x78, 0x71, 0x9e, 0x2c, 0x50, 0x29, 0x82, 0x28, 0x86, 0x32, 0xe5,
  0x55, 0x48, 0x4d, 0xf4, 0x45, 0x72, 0x70, 0x3e, 0x0c, 0x6e, 0xd3, 0x13,
  0x82, 0xdb, 0x11, 0xa6, 0x0c, 0x29, 0x84, 0xe0, 0xe5, 0x01, 0x43, 0xc4,
  0xe8, 0x48, 0x18, 0x96, 0x2c, 0x69, 0xe9, 0x1c, 0xea, 0xcc, 0xf1, 0x32,
  0xca, 0x68, 0xf5, 0x8a, 0x34, 0x12, 0x5f, 0xdc, 0x2f, 0xe3, 0xa0, 0xb8,
  0x11, 0x8b, 0x18, 0x9e, 0x48, 0x42
};
const unsigned int test_target_wlan_x509_len = 1446;

struct cert fw_test_certs[4] =  {
    {sizeof (test_target_wlan_x509), &test_target_wlan_x509[0]},
    {sizeof (test_target_wlan_x509), &test_target_wlan_x509[0]},
    {sizeof (test_target_wlan_x509), &test_target_wlan_x509[0]},
    {sizeof (test_target_wlan_x509), &test_target_wlan_x509[0]}
};
struct cert fw_prod_certs[4] =  {
    {sizeof (test_target_wlan_x509), &test_target_wlan_x509[0]},
    {sizeof (test_target_wlan_x509), &test_target_wlan_x509[0]},
    {sizeof (test_target_wlan_x509), &test_target_wlan_x509[0]},
    {sizeof (test_target_wlan_x509), &test_target_wlan_x509[0]}
};
 /* interface function to return the firmware file pointers
 * Description:
 * Load the firmware, check security attributes and then see if this requires
 * firmware authentication or not. If no firmware authentication required
 * return firmware pointer, otherwise, returns the firmware pointer iff signature
 * checks are good.  if code sign feature is disabled, do nothing, but return
 * the same fw_entry pointer, if enabled, process the header and advance the
 * fw_entry->data pointer by header size, reduce the size by the header
 * in the fw_entry, while freeing make sure that we go back by header size
 * and free.
 */
static int
request_secure_firmware(struct firmware **fw_entry, const char *file,
        struct device *dev, int dev_id)
{
    unsigned char *fw;
    unsigned int len=0;
    struct auth_input fw_check_buffer;
    int cert_type, err=0;
    struct firmware_head *h;
    int status = 0;

    if(!file) return -1;

    status = request_firmware((const struct firmware **)fw_entry, file, dev);

    if (status != 0) return status;

    /* code sign is not enabled, assume the real file starts from the start of
     * the file and return success. There is no risk in this, because
     * fw_code_sign module param can't modified from out side.
     */
    if (!cfg_get(psoc, CFG_OL_FW_CODE_SIGN)) return status;

    /* below is only when code sign is enabled
     */
    fw = (unsigned char*)(*fw_entry)->data;                 /* the start of the firmware file */

    qdf_info("%s Total Firmware file allocated %zd\n", __func__, (*fw_entry)->size);
    h = fw_unpack(fw, dev_id, &err);
    if(!h) {
        qdf_info("%s: something wrong in reading the firmware header", __func__);
        release_firmware(*fw_entry);
        return -1;
    }
    /* we assume post the integration of the code, all files continue to have
     * this header if not, there is some thing wrong. Also in case of signed
     * files, firmware signing version would be present and if that is zero,
     * that is not signed
     */
    if (h && !h->fw_img_sign_ver) {
       goto fail;
    }

    /* now the fimrware is unpacked, build and pass it to kernel
     */
    memset(&fw_check_buffer, 0, sizeof(fw_check_buffer));
    fw_check_buffer.certBuffer = (unsigned char *)fw_sign_get_cert_buffer(h, &cert_type, &len);
    if (fw_check_buffer.certBuffer) {
        int err=0;
        fw_check_buffer.signature = (unsigned char*)fw +
                                    h->fw_hdr_length + h->fw_img_size;
        /* sign is calculated with header */
        fw_check_buffer.data = fw ;
        fw_check_buffer.cert_len = len;
        fw_check_buffer.sig_len = h->fw_sig_len;
        fw_check_buffer.data_len = h->fw_img_size + h->fw_hdr_length;
        fw_check_buffer.sig_hash_algo = fw_sign_get_hash_algo(h);
        fw_check_buffer.pk_algo = fw_sign_get_pk_algo(h);
        fw_check_buffer.cert_type = cert_type;
        /* we are done with all, now call into linux and see what it returns,
         * if authentication is good, return the pointer to fw, otherwise,
         * free the buffer and return NULL
         */

        if (cfg_get(psoc, CFG_OL_FW_CODE_SIGN) >= 2) {
            qdf_info("fw_check_buffer.signature %x,\n"
                   "fw_check_buffer.data %x,\n"
                   "fw_check_buffer.cert_len %x,\n"
                   "fw_check_buffer.sig_len %x,\n"
                   "fw_check_buffer.data_len %x,\n"
                   "fw_check_buffer.sig_hash_algo %x,\n"
                   "fw_check_buffer.pk_algo %x,\n"
                   "fw_check_buffer.cert_type %x\n",
            (unsigned int)fw_check_buffer.signature ,
            (unsigned int)fw_check_buffer.data ,
            (unsigned int)fw_check_buffer.cert_len ,
            (unsigned int)fw_check_buffer.sig_len ,
            (unsigned int)fw_check_buffer.data_len,
            (unsigned int)fw_check_buffer.sig_hash_algo ,
            (unsigned int)fw_check_buffer.pk_algo ,
            (unsigned int)fw_check_buffer.cert_type );
        }
        if ((err=authenticate_fw(&fw_check_buffer)) < 0)  {
            qdf_info("%s: authentication status %d \n", __func__, err);
            goto fail;
        } else {
            qdf_info("%s: authentication status %d \n", __func__, err);
        }
    } else {
        goto fail;
    }
    /*
     * caller is not aware of that firmware is signed, make sure that caller
     * gets the real firmware pointer, this is, nothing but, fw->data+sizeof(h0;
     * data pointer is of type u8, so regular math should work
     */
    (*fw_entry)->data += sizeof(struct firmware_head);
    /* fw_img_size is actual size of file for loading, check WAR for
     * release_secure_firmware how this impacts free of the firmware.
     */
    (*fw_entry)->size = h->fw_img_size;
    if (h) qdf_mem_free(h);
    return 0;
fail:
    release_firmware(*fw_entry);
    if(h) qdf_mem_free(h);
    return -1;
}

/* release_secure_firmware
 * Description
 *      In case of code signing, the firmware has extra pointers, because
 *      request_secure_firmware uses the request_firmware internally, which
 *      does simply loads the firmware. Because of this, we either need to
 *      move the firmware pointer back by size of the header or free it
 *      simply, based firmware entry
 */
static void
release_secure_firmware(struct firmware *fw_entry)
{
    struct firmware_head *h;

    if (!fw_entry) return;
    if (!cfg_get(psoc, CFG_OL_FW_CODE_SIGN)) return release_firmware(fw_entry);
    /* modify the firmware data pointer */
    fw_entry->data -= sizeof (struct firmware_head);
    /*
     * release_firmware actually frees the virtual pages based on size allocated
     * earlier, so correct the size size back to correct number
     */
    h = (struct firmware_head *) (fw_entry->data);

    /*
     * In fw_unpack(), the header bytes are copied to a distinct block of memory
     * because  sign compuation  happens post converting to network byte order.
     *
     * Before freeing, there is no use of this memory, and we can aovid copy
     * and do the in-memory conversion in place.
     *
     * fw_entry->size is set by linux kernel and it is always equal to size of
     * the firmware file. So it is fine to assign the same number of bytes
     * to size. release_firmware() actually gets number of pages from size.
     */
    ntohlm(h, sizeof(struct firmware_head));
    fw_entry->size = h->fw_img_length;

    if (cfg_get(psoc, CFG_OL_FW_CODE_SIGN) >= 2) {
        qdf_info("\nFirmware header base        :0x%8x\n", (uintptr_t)h);
        qdf_info("                length      :0x%8x\n", h->fw_hdr_length);
        qdf_info("                img size    :0x%8x\n", h->fw_img_size);
        qdf_info("                img length  :0x%8x\n", h->fw_img_length);
        qdf_info("                img magic   :0x%8x\n", h->fw_img_magic_number);
        qdf_info("                chip id     :0x%8x\n", h->fw_chip_identification);
        qdf_info("                file magic  :0x%8x\n", h->fw_img_file_magic);
        qdf_info("                signing ver :0x%8x\n", h->fw_img_sign_ver);
        qdf_info("                ver M/S/E   :%x/%x/%x\n",
                                                    FW_VER_GET_MAJOR(h),
                                                    FW_VER_GET_IMG_TYPE(h),
                                                    FW_VER_GET_IMG_TYPE_VER(h));
        qdf_info("                flags       :%x\n",h->fw_hdr_flags);
        qdf_info("                sign length :%8x\n", h->fw_sig_len);
    }
    qdf_info("%s Total Firmware file allocated %d\n", __func__, fw_entry->size);
    release_firmware(fw_entry);
}

/*
 * dump the bytes in 2 bytes, and 16 bytes per line
 */
static void
fw_hex_dump(unsigned char *p, int len)
{
    int i=0;

    for (i=0; i<len; i++) {
        if (!(i%16)) {
            qdf_info("\n");
        }
        qdf_info("%2x ", p[i]);
    }
}
/*
 * fw_unpack extracts the firmware header from the bin file, that is loaded
 * through request_firmware(). Firmware header is in network byte order. This is
 * copied to a block of memory and converted to host byteorder to make sure that
 * the firmware header do not depend on system where it is generated.
 *
 * Post filling that it does check lot of sanity checks and then returns pointer
 * to firmware header.
 */
static struct firmware_head *
fw_unpack(unsigned char *fw, int chip_id, int *err)
{
    struct firmware_head *th, *h;
    int i = 0;

    if (!fw) return FW_SIGN_ERR_INTERNAL;

    /* at this point, the firmware header is in network byte order
     * load that into memort and convert to host byte order
     */
    th = qdf_mem_malloc(sizeof(struct firmware_head));
    if (!th) {
        return NULL;
    }
    memcpy(th, fw, sizeof(struct firmware_head));

	ntohlm(th, sizeof(struct firmware_head));
    /* do not access header before this line */
    h = th;

    if (cfg_get(psoc, CFG_OL_FW_CODE_SIGN) >= 2) {
        unsigned char *data, *signature;

        if (cfg_get(psoc, CFG_OL_FW_CODE_SIGN) >= 3) {
            fw_hex_dump((unsigned char*)h, sizeof(struct firmware_head));
        }

        signature = (unsigned char*)fw + h->fw_hdr_length + h->fw_img_size;
        data = (unsigned char *)fw + sizeof(struct firmware_head);
        qdf_info("\nFirmware header base        :0x%8x\n", (uintptr_t)fw);
        qdf_info("                length      :0x%8x\n", h->fw_hdr_length);
        qdf_info("                img size    :0x%8x\n", h->fw_img_size);
        qdf_info("                img length  :0x%8x\n", h->fw_img_length);
        qdf_info("                img magic   :0x%8x\n", h->fw_img_magic_number);
        qdf_info("                chip id     :0x%8x\n", h->fw_chip_identification);
        qdf_info("                file magic  :0x%8x\n", h->fw_img_file_magic);
        qdf_info("                signing ver :0x%8x\n", h->fw_img_sign_ver);
        qdf_info("                ver M/S/E   :%x/%x/%x\n",
                                                    FW_VER_GET_MAJOR(h),
                                                    FW_VER_GET_IMG_TYPE(h),
                                                    FW_VER_GET_IMG_TYPE_VER(h));
        qdf_info("                flags       :%x\n",h->fw_hdr_flags);
        qdf_info("                sign length :%8x\n", h->fw_sig_len);
        if (cfg_get(psoc, CFG_OL_FW_CODE_SIGN) >= 3) {
            qdf_info(" ==== HEX DUMPS OF REGIONS ====\n");
            qdf_info("SIGNATURE:\n");
            fw_hex_dump(signature, h->fw_sig_len);
            qdf_info("\nDATA\n");
            fw_hex_dump(data, h->fw_img_size);
            qdf_info("=== HEX DUMP END ===\n");
        }
    }
    if ( (i=fw_check_img_magic(h->fw_img_magic_number)) >= 0) {
        qdf_info("Chip Identification:%x Device Magic %x: %s found\n",
                fw_auth_supp_devs[i].dev_id,
                fw_auth_supp_devs[i].img_magic,
                fw_auth_supp_devs[i].dev_name);
    } else {
        *err = -FW_SIGN_ERR_UNSUPP_CHIPSET;
        qdf_mem_free(h);
        return NULL;
    }
    if ( (chip_id == h->fw_chip_identification) &&
            (fw_check_chip_id(h->fw_chip_identification) >= 0)) {
        qdf_info("Chip Identification:%x Device Magic %x: %s found\n",
                fw_auth_supp_devs[i].dev_id,
                fw_auth_supp_devs[i].img_magic,
                fw_auth_supp_devs[i].dev_name);
    } else {
        *err =  -FW_SIGN_ERR_INV_DEV_ID;
        qdf_mem_free(h);
        return NULL;
    }

    if (fw_sign_check_file_magic(h->fw_img_file_magic) >= 0) {
        qdf_info("Found good image magic %x\n", h->fw_img_file_magic);
    } else {
        *err = -FW_SIGN_ERR_INVALID_FILE_MAGIC;
        qdf_mem_free(h);
        return NULL;
    }
    /* dump various versions */
    if (h->fw_img_sign_ver && (h->fw_img_sign_ver  != THIS_FW_IMAGE_VERSION)) {
        qdf_info("Firmware is not signed with same version, as the tool\n");
        *err = -FW_SIGN_ERR_IMAGE_VER;
        qdf_mem_free(h);
        return NULL;
    }
    /* check and dump the versions that are available in the file for now
     * TODO roll back check would be added in future
     */
    qdf_info("Major: %d S/E/C: %d Version %d\n",
            FW_VER_GET_MAJOR(h), FW_VER_GET_IMG_TYPE(h), FW_VER_GET_IMG_TYPE_VER(h));

    qdf_info("Minor: %d sub version:%d build_id %d\n",
            FW_VER_IMG_GET_MINOR_VER(h),
            FW_VER_IMG_GET_MINOR_SUBVER(h),
            FW_VER_IMG_GET_MINOR_RELNBR(h));

    qdf_info("Header Flags %x\n", h->fw_hdr_flags);
    /* sanity can be added, but ignored for now, if this goes
     * wrong, file authentication goes wrong
     */
    qdf_info("image size %d \n", h->fw_img_size);
    qdf_info("image length %d \n", h->fw_img_length);
    /* check if signature algorithm is supported or not */
    if (!fw_check_sig_algorithm(h->fw_sig_algo)) {
    debugfs_remove(smart_log_file->dbgfs.dfs);
        qdf_info("image signing algorithm mismatch %d \n", h->fw_sig_algo);
        *err = -FW_SIGN_ERR_SIGN_ALGO;
        qdf_mem_free(h);
        return NULL;
    }

    /* do not check oem for now */
    /* TODO dump the sinagure now here, and passit down to kernel
     * crypto module
     */
    return h;
}
/*
 * find and check the known chip ids
 */
static inline int
fw_check_chip_id(unsigned int chip_id)
{
    int i=0;

    for (i = 0;
         ((i < NELEMENTS(fw_auth_supp_devs)) && (fw_auth_supp_devs[i].dev_id != chip_id) &&
            (fw_auth_supp_devs[i].dev_id != 0)) ;
         i++)
       ;
    if (fw_auth_supp_devs[i].dev_id == 0) return -1;
    return i;
}
/*
 * validate file image magic numbers
 */
inline int
fw_check_img_magic(unsigned int img_magic)
{
    int i=0;

    for (i = 0;
         ((i < NELEMENTS(fw_auth_supp_devs)) && (fw_auth_supp_devs[i].img_magic != img_magic) &&
            (fw_auth_supp_devs[i].dev_id != 0)) ;
         i++)
       ;
    if (fw_auth_supp_devs[i].dev_id == 0) return -1;
    return i;
}
/*
 * check image magics
 */
static inline int
fw_sign_check_file_magic(unsigned int file_type)
{
    switch (file_type) {
    case FW_IMG_FILE_MAGIC_TARGET_BOARD_DATA:
    case FW_IMG_FILE_MAGIC_TARGET_WLAN:
    case FW_IMG_FILE_MAGIC_TARGET_OTP:
    case FW_IMG_FILE_MAGIC_TARGET_CODE_SWAP:
        return 0;
    case FW_IMG_FILE_MAGIC_INVALID:
        return -FW_SIGN_ERR_INVALID_FILE_MAGIC;
    default:
        return -FW_SIGN_ERR_INVALID_FILE_MAGIC;
    }
    return 0;
}

/*
 * get supported hash method
 */
static int
fw_sign_get_hash_algo (struct firmware_head *h)
{
    if(!h) return -1;

    /* avoid warning */
    h = h;

    /* right now do not check any thing but return one known
     * algorithm, fix this by looking at versions of the
     * signing
     */
    return HASH_ALGO_SHA256;
}

/*
 * At this time, do not use any thing, but return the same known algorithm type. Ideally we should
 * add this by knowing the file type and signature version
 */
static int
fw_sign_get_pk_algo (struct firmware_head *h)
{
    if(!h) return -1;

    /* avoid warning*/
    h = h;
    /* FIXME based on signing algorithm, we should choose
     * different keys, right now return only one
     */
    return PKEY_ALGO_RSA;
}
/*
 * get certficate buffer based on file type and signing version
 */
const unsigned char  *
fw_sign_get_cert_buffer(struct firmware_head *h, int *cert_type, int *len)
{
    int idx=0;

    if (!h) return NULL;

    /* based on signing version, we should be filling these numbers, right now now checks */
    *cert_type = PKEY_ID_X509;
    switch (h->fw_img_file_magic)
    {
        case FW_IMG_FILE_MAGIC_TARGET_WLAN:
            idx = 0;
            break;
        case FW_IMG_FILE_MAGIC_TARGET_OTP:
            idx = 1;
            break;
        case FW_IMG_FILE_MAGIC_TARGET_BOARD_DATA:
            idx = 2;
            break;
        case FW_IMG_FILE_MAGIC_TARGET_CODE_SWAP:
            idx = 3;
            break;
        default:
            return NULL;
    }
    if (h->fw_ver_rel_type == FW_IMG_VER_REL_TEST) {
        *len = fw_test_certs[idx].cert_len;
        return &fw_test_certs[idx].cert[0];
    } else if (h->fw_ver_rel_type == FW_IMG_VER_REL_PROD) {
        *len = fw_prod_certs[idx].cert_len;
        return &fw_prod_certs[idx].cert[0];
    } else {
        return NULL;
    }
}
static inline int
fw_check_sig_algorithm(int s)
{
    switch (s) {
        case RSA_PSS1_SHA256:
            return 1;
    }
    return 0;
}
/* utility functions */
static inline void
htonlm(void *sptr, int len)
{
    int i = 0;
    unsigned int *dptr = (unsigned int*)sptr;
    /* length 0 is not allowed, minimum 4 bytes */
    if (len <= 0) len = 4;
    for(i=0; i<len/4; i++) {
        dptr[i] = htonl(dptr[i]);
    }
}
static inline void
ntohlm(void *sptr, int len)
{
    int i = 0;
    unsigned int *dptr = (unsigned int*)sptr;
    /* length 0 is not allowed, minimum 4 bytes */
    if (len <= 0) len = 4;
    for(i=0; i<len/4; i++) {
        dptr[i] = ntohl(dptr[i]);
    }
}
#endif  /* FW_CODE_SIGN */

/* ol_ath_request_firmware
 */
int
ol_ath_request_firmware(struct firmware_priv **fw_entry, const char *file,
		        struct device *dev, int dev_id)
{
	int rfwst;
#if FW_CODE_SIGN
    rfwst = request_secure_firmware(fw_entry, file, dev, dev_id);
#else
    rfwst = request_firmware((const struct firmware **) fw_entry, file, dev);
#endif /* FW_CODE_SIGN */
    return rfwst;
}
EXPORT_SYMBOL(ol_ath_request_firmware);

/* ol_ath_release_firmware
 */
void
ol_ath_release_firmware(struct firmware_priv *fw_entry)
{
#if FW_CODE_SIGN
    release_secure_firmware((struct firmware *) fw_entry);
#else
    release_firmware((struct firmware *) fw_entry);
#endif /* FW_CODE_SIGN */
}
EXPORT_SYMBOL(ol_ath_release_firmware);

#if QCA_11AX_STUB_SUPPORT
/**
 * @brief Determine whether 802.11ax stubbing is enabled or not
 *
 * @param scn - ol_ath_softc_net80211 structure for the radio
 * @return Integer status value.
 *      -1 : Failure
 *       0 : Disabled
 *       1 : Enabled
 */
int ol_ath_is_11ax_stub_enabled(ol_ath_soc_softc_t *soc)
{
    uint32_t target_type;

    if (NULL == soc) {
        qdf_info("%s: soc is NULL. Unable to determine 802.11ax stubbing "
                "status", __func__);
        return -1;
    }
    target_type = lmac_get_tgt_type(soc->psoc_obj);

    if (TARGET_TYPE_UNKNOWN == target_type)
    {
        qdf_info("%s: Target type unknown. Unable to determine 802.11ax "
                "stubbing status", __func__);
        return -1;
    }

    /*
     * 802.11ax stubbing is enabled only if the enable_11ax_stub module
     * parameter is set to 1, and only for QCA9984.
     */
    if (cfg_get(soc->psoc_obj, CFG_OL_ENABLE_11AX_STUB)
        && (target_type == TARGET_TYPE_QCA9984)) {
        return 1;
    } else {
        return 0;
    }
}
#endif /* QCA_11AX_STUB_SUPPORT */
