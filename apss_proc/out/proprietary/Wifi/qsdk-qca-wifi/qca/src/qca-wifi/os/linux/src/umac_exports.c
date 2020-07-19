/*
 * Copyright (c) 2016-2019 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include <linux/module.h>
#include <ieee80211_var.h>
#include <ieee80211_vap.h>
#include <ieee80211_defines.h>
#include <ieee80211_acs.h>
#include <ieee80211_extacs.h>
#include <ieee80211_cbs.h>
#include <ieee80211_channel.h>
#include <ieee80211_api.h>
#include <ieee80211_wds.h>
#include <ath_dev.h>
#include <ieee80211_regdmn.h>
#include <if_athvar.h>
#include <acfg_drv_event.h>
#include <ald_netlink.h>
#include <ieee80211_proto.h>
#include <ieee80211_admctl.h>
#include <ieee80211_vi_dbg.h>
#include <ieee80211_acfg.h>
#include <ol_if_athvar.h>
#include <wdi_event_api.h>
#include <wlan_mgmt_txrx_utils_api.h>
#include <wlan_scan.h>
#include <wlan_scan_ucfg_api.h>
#include <wlan_utility.h>
#include <wlan_mlme_dispatcher.h>
#include <wlan_son_pub.h>
#include <osif_private.h>

#if ATH_ACS_DEBUG_SUPPORT
#include <acs_debug.h>
#endif

/* Export ieee80211_acs.c functions */
EXPORT_SYMBOL(ieee80211_acs_set_param);
EXPORT_SYMBOL(ieee80211_acs_get_param);
EXPORT_SYMBOL(ieee80211_acs_stats_update);
EXPORT_SYMBOL(wlan_acs_block_channel_list);

/* Export ieee80211_cbs.c functions */
EXPORT_SYMBOL(ieee80211_cbs_set_param);
EXPORT_SYMBOL(ieee80211_cbs_get_param);
EXPORT_SYMBOL(ieee80211_cbs_api_change_home_channel);
EXPORT_SYMBOL(wlan_bk_scan);
EXPORT_SYMBOL(wlan_bk_scan_stop);

#if ATH_SUPPORT_ICM
EXPORT_SYMBOL(ieee80211_extacs_record_schan_info);
#endif


/* Export ieee80211_ald.c functions */
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
EXPORT_SYMBOL(ieee80211_ald_record_tx);
#endif

/* Export ieee80211_aow.c/ieee80211_aow_mck.c functions */
#if ATH_SUPPORT_AOW
EXPORT_SYMBOL(wlan_get_tsf);
EXPORT_SYMBOL(wlan_aow_set_audioparams);
EXPORT_SYMBOL(wlan_aow_tx);
EXPORT_SYMBOL(wlan_aow_dispatch_data);
EXPORT_SYMBOL(wlan_aow_register_calls_to_usb);
EXPORT_SYMBOL(aow_register_usb_calls_to_wlan);
#endif

EXPORT_SYMBOL(ieee80211_channel_notify_to_app);

/* Export band_steering_direct_attach.c functions */

/* Export ieee80211_channel.c functions */
EXPORT_SYMBOL(ieee80211_chan2mode);
EXPORT_SYMBOL(ieee80211_ieee2mhz);
EXPORT_SYMBOL(ieee80211_chan2freq);
EXPORT_SYMBOL(ieee80211_find_alternate_mode_channel);
EXPORT_SYMBOL(ieee80211_find_any_valid_channel);
EXPORT_SYMBOL(ieee80211_find_channel);
EXPORT_SYMBOL(ieee80211_doth_findchan);
EXPORT_SYMBOL(ieee80211_find_dot11_channel);
EXPORT_SYMBOL(wlan_get_desired_phymode);
EXPORT_SYMBOL(wlan_get_dev_current_channel);
EXPORT_SYMBOL(wlan_set_channel);
EXPORT_SYMBOL(ieee80211_get_chan_width);
EXPORT_SYMBOL(ieee80211_check_overlap);
EXPORT_SYMBOL(ieee80211_sec_chan_offset);
EXPORT_SYMBOL(wlan_get_current_phymode);
EXPORT_SYMBOL(ieee80211_set_channel_for_cc_change);

/* Export ieee80211_wireless.c functions */
EXPORT_SYMBOL(phymode_to_htflag);

EXPORT_SYMBOL(ieee80211_ifattach);
EXPORT_SYMBOL(ieee80211_ifdetach);
EXPORT_SYMBOL(ieee80211_start_running);
EXPORT_SYMBOL(ieee80211_stop_running);
EXPORT_SYMBOL(wlan_get_device_param);
EXPORT_SYMBOL(wlan_get_device_mac_addr);
EXPORT_SYMBOL(ieee80211_vaps_active);

/* Export ieee80211_msg.c functions */
EXPORT_SYMBOL(ieee80211_note);
EXPORT_SYMBOL(ieee80211_note_mac);
EXPORT_SYMBOL(ieee80211_discard_frame);
EXPORT_SYMBOL(ieee80211_discard_mac);

/* Export ieee80211_node.c functions */
#if IEEE80211_DEBUG_REFCNT
EXPORT_SYMBOL(ieee80211_free_node_debug);
EXPORT_SYMBOL(ieee80211_find_node_debug);
EXPORT_SYMBOL(ieee80211_find_txnode_debug);
EXPORT_SYMBOL(ieee80211_find_rxnode_debug);
EXPORT_SYMBOL(ieee80211_find_rxnode_nolock_debug);
EXPORT_SYMBOL(ieee80211_ref_bss_node_debug);
EXPORT_SYMBOL(ieee80211_try_ref_bss_node_debug);
#else
EXPORT_SYMBOL(ieee80211_free_node);
EXPORT_SYMBOL(ieee80211_find_node);
EXPORT_SYMBOL(ieee80211_find_txnode);
EXPORT_SYMBOL(ieee80211_find_rxnode);
EXPORT_SYMBOL(ieee80211_find_rxnode_nolock);
EXPORT_SYMBOL(ieee80211_ref_bss_node);
EXPORT_SYMBOL(ieee80211_try_ref_bss_node);
#endif
EXPORT_SYMBOL(_ieee80211_find_logically_deleted_node);
EXPORT_SYMBOL(wlan_node_peer_delete_response_handler);
#ifdef AST_HKV1_WORKAROUND
EXPORT_SYMBOL(wlan_wds_delete_response_handler);
#endif
#if ATH_SUPPORT_WRAP
#if IEEE80211_DEBUG_REFCNT
EXPORT_SYMBOL(ieee80211_find_wrap_node_debug);
#else
EXPORT_SYMBOL(ieee80211_find_wrap_node);
#endif
EXPORT_SYMBOL(wlan_is_wrap);
#endif
EXPORT_SYMBOL(ieee80211_iterate_node);
EXPORT_SYMBOL(wlan_iterate_station_list);
EXPORT_SYMBOL(ieee80211_has_weptkipaggr);

#if ATH_SUPPORT_AOW
extern void ieee80211_send2all_nodes(void *reqvap, void *data, int len, u_int32_t seqno, u_int64_t tsf);
EXPORT_SYMBOL(ieee80211_send2all_nodes);
#endif

#ifdef WLAN_DFS_PRECAC_AUTO_CHAN_SUPPORT
int ieee80211_ucfg_set_chanswitch(wlan_if_t vaphandle, u_int8_t chan, u_int8_t tbtt, u_int16_t ch_width);
#endif

/* Export ieee80211_node_ap.c functions */
EXPORT_SYMBOL(ieee80211_tmp_node);

/* Export ieee80211_vap.c functions */
EXPORT_SYMBOL(ieee80211_vap_setup);
EXPORT_SYMBOL(ieee80211_vap_detach);
EXPORT_SYMBOL(wlan_iterate_vap_list);
EXPORT_SYMBOL(wlan_vap_get_hw_macaddr);
EXPORT_SYMBOL(ieee80211_new_opmode);
EXPORT_SYMBOL(ieee80211_get_vap_opmode_count);
EXPORT_SYMBOL(wlan_vap_get_registered_handle);
EXPORT_SYMBOL(ieee80211_mbssid_setup);
EXPORT_SYMBOL(ieee80211_mbssid_del_profile);

#ifndef WLAN_CONV_CRYPTO_SUPPORTED
/* Export ieee80211_crypto.c functions */
EXPORT_SYMBOL(ieee80211_crypto_newkey);
EXPORT_SYMBOL(ieee80211_crypto_delkey);
EXPORT_SYMBOL(ieee80211_crypto_setkey);
EXPORT_SYMBOL(ieee80211_crypto_encap);
EXPORT_SYMBOL(ieee80211_crypto_decap);
EXPORT_SYMBOL(ieee80211_crypto_wep_setdummykey);

/* Export ieee80211_crypto_none.c functions */
EXPORT_SYMBOL(ieee80211_cipher_none);

EXPORT_SYMBOL(ieee80211_crypto_encap_mon);
#endif

/* Export ieee80211_dfs.c functions */
EXPORT_SYMBOL(ieee80211_dfs_action);
EXPORT_SYMBOL(ieee80211_mark_dfs);

/* Export ieee80211_extap.c functions */
extern void compute_udp_checksum(qdf_net_iphdr_t *p_iph, unsigned short  *ip_payload);
EXPORT_SYMBOL(compute_udp_checksum);

/* Export ieee80211_beacon.c functions */
EXPORT_SYMBOL(ieee80211_beacon_alloc);
EXPORT_SYMBOL(ieee80211_beacon_update);
EXPORT_SYMBOL(wlan_pdev_beacon_update);
EXPORT_SYMBOL(ieee80211_mlme_beacon_suspend_state);
EXPORT_SYMBOL(ieee80211_csa_interop_phy_update);

/* Export ieee80211_ie.c functions */
#if ATH_SUPPORT_IBSS_DFS
EXPORT_SYMBOL(ieee80211_measurement_report_action);
extern void ieee80211_ibss_beacon_update_start(struct ieee80211com *ic);
EXPORT_SYMBOL(ieee80211_ibss_beacon_update_start);
#endif

/* Export ieee80211_mgmt.c functions */
EXPORT_SYMBOL(ieee80211_send_deauth);
EXPORT_SYMBOL(ieee80211_send_disassoc);
EXPORT_SYMBOL(ieee80211_send_action);
#ifdef ATH_SUPPORT_TxBF
EXPORT_SYMBOL(ieee80211_send_v_cv_action);
#endif
EXPORT_SYMBOL(ieee80211_send_bar);
EXPORT_SYMBOL(ieee80211_prepare_qosnulldata);
EXPORT_SYMBOL(ieee80211_recv_mgmt);
EXPORT_SYMBOL(ieee80211_recv_ctrl);
EXPORT_SYMBOL(ieee80211_dfs_cac_cancel);
EXPORT_SYMBOL(ieee80211_dfs_cac_start);

#ifdef WLAN_DFS_PRECAC_AUTO_CHAN_SUPPORT
EXPORT_SYMBOL(ieee80211_ucfg_set_chanswitch);
#endif

/* Export ieee80211_mlme.c functions */
EXPORT_SYMBOL(wlan_mlme_deauth_request);
EXPORT_SYMBOL(sta_disassoc);
EXPORT_SYMBOL(ieee80211_mlme_node_pwrsave);
EXPORT_SYMBOL(cleanup_sta_peer);

/* Export ieee80211_mlme_sta.c functions */
EXPORT_SYMBOL(ieee80211_beacon_miss);
EXPORT_SYMBOL(ieee80211_notify_beacon_rssi);
#if ATH_SUPPORT_DFS && ATH_SUPPORT_STA_DFS
EXPORT_SYMBOL(mlme_set_stacac_valid);
#endif
EXPORT_SYMBOL(ieee80211_indicate_sta_radar_detect);
EXPORT_SYMBOL(channel_switch_set_channel);

/* Export ieee80211_proto.c functions */
EXPORT_SYMBOL(ieee80211_wme_initglobalparams);
EXPORT_SYMBOL(ieee80211_muedca_initglobalparams);
EXPORT_SYMBOL(ieee80211_wme_amp_overloadparams_locked);
EXPORT_SYMBOL(ieee80211_wme_updateparams_locked);
EXPORT_SYMBOL(ieee80211_dump_pkt);

/* Export ieee80211_power.c functions */
EXPORT_SYMBOL(ieee80211_power_class_attach);

/* Export ieee80211_power_queue.c functions */
EXPORT_SYMBOL(ieee80211_node_saveq_queue);
EXPORT_SYMBOL(ieee80211_node_saveq_flush);

/* Export ieee80211_sta_power.c functions */
EXPORT_SYMBOL(ieee80211_sta_power_event_tim);
EXPORT_SYMBOL(ieee80211_sta_power_event_dtim);

/* Export ieee80211_regdmn.c functions */
EXPORT_SYMBOL(ieee80211_set_regclassids);
EXPORT_SYMBOL(wlan_set_countrycode);
EXPORT_SYMBOL(wlan_set_regdomain);
EXPORT_SYMBOL(regdmn_update_ic_channels);

/* Export ieee80211_resmgr.c functions */
#if DA_SUPPORT
EXPORT_SYMBOL(ieee80211_resmgr_attach);
#endif

#if ATH_SUPPORT_DFS && ATH_SUPPORT_STA_DFS
EXPORT_SYMBOL(mlme_reset_mlme_req);
EXPORT_SYMBOL(wlan_mlme_sm_get_curstate);
EXPORT_SYMBOL(mlme_cancel_stacac_timer);
EXPORT_SYMBOL(mlme_set_stacac_running);
#endif

#if UNIFIED_SMARTANTENNA
extern int register_smart_ant_ops(struct smartantenna_ops *sa_ops);
extern int deregister_smart_ant_ops(char *dev_name);
EXPORT_SYMBOL(register_smart_ant_ops);
EXPORT_SYMBOL(deregister_smart_ant_ops);
#endif

/* Export ieee80211_txbf.c functions */
#ifdef ATH_SUPPORT_TxBF
EXPORT_SYMBOL(ieee80211_set_TxBF_keycache);
EXPORT_SYMBOL(ieee80211_request_cv_update);
#endif

/* Export ieee80211_input.c functions */
EXPORT_SYMBOL(ieee80211_input);
EXPORT_SYMBOL(ieee80211_input_all);
EXPORT_SYMBOL(ieee80211_mgmt_input);

#ifdef ATH_SUPPORT_TxBF
extern void
ieee80211_tx_bf_completion_handler(struct ieee80211_node *ni,  struct ieee80211_tx_status *ts);
EXPORT_SYMBOL(ieee80211_tx_bf_completion_handler);
#endif
EXPORT_SYMBOL(ieee80211_kick_node);
EXPORT_SYMBOL(ieee80211_complete_wbuf);
EXPORT_SYMBOL(ieee80211_mgmt_complete_wbuf);
/* Export ieee80211_wds.c functions */
EXPORT_SYMBOL(ieee80211_nawds_disable_beacon);

/* Export ieee80211_wifipos.c functions */
#if ATH_SUPPORT_WIFIPOS
extern int ieee80211_update_wifipos_stats(ieee80211_wifiposdesc_t *wifiposdesc);
extern int ieee80211_isthere_wakeup_request(struct ieee80211_node *ni);
extern int ieee80211_update_ka_done(u_int8_t *sta_mac_addr, u_int8_t ka_tx_status);
EXPORT_SYMBOL(ieee80211_isthere_wakeup_request);
EXPORT_SYMBOL(ieee80211_update_ka_done);
EXPORT_SYMBOL(ieee80211_update_wifipos_stats);
EXPORT_SYMBOL(ieee80211_cts_done);
#endif

/* Export ieee80211_wnm.c functions */
EXPORT_SYMBOL(wlan_wnm_tfs_filter);
EXPORT_SYMBOL(ieee80211_timbcast_alloc);
EXPORT_SYMBOL(ieee80211_timbcast_update);
EXPORT_SYMBOL(ieee80211_wnm_timbcast_cansend);
EXPORT_SYMBOL(ieee80211_wnm_timbcast_enabled);
EXPORT_SYMBOL(ieee80211_timbcast_get_highrate);
EXPORT_SYMBOL(ieee80211_timbcast_get_lowrate);
EXPORT_SYMBOL(ieee80211_timbcast_lowrateenable);
EXPORT_SYMBOL(ieee80211_timbcast_highrateenable);
EXPORT_SYMBOL(ieee80211_wnm_fms_enabled);

#if UMAC_SUPPORT_ACFG
/* Export acfg_net_event.c functions */
EXPORT_SYMBOL(acfg_event_netlink_init);
EXPORT_SYMBOL(acfg_event_netlink_delete);
#endif

/* Export adf_net_vlan.c functions */
#if ATH_SUPPORT_VLAN
EXPORT_SYMBOL(qdf_net_get_vlan);
EXPORT_SYMBOL(qdf_net_is_vlan_defined);
#endif

/* Export ald_netlink.c functions */
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
EXPORT_SYMBOL(ald_init_netlink);
EXPORT_SYMBOL(ald_destroy_netlink);
EXPORT_SYMBOL(ieee80211_ald_update_phy_error_rate);
#endif

/* Export osif_umac.c functions */
extern struct ath_softc_net80211 *global_scn[10];
extern ol_ath_soc_softc_t *ol_global_soc[GLOBAL_SOC_SIZE];
extern int num_global_scn;
extern int ol_num_global_soc;
extern unsigned long ath_ioctl_debug;
EXPORT_SYMBOL(global_scn);
EXPORT_SYMBOL(num_global_scn);
EXPORT_SYMBOL(ol_global_soc);
EXPORT_SYMBOL(ol_num_global_soc);
EXPORT_SYMBOL(ath_ioctl_debug);
#if ATH_DEBUG
extern unsigned long ath_rtscts_enable;
EXPORT_SYMBOL(ath_rtscts_enable);
#endif
EXPORT_SYMBOL(osif_restart_for_config);

/* Export ath_timer.c functions */
EXPORT_SYMBOL(ath_initialize_timer_module);
EXPORT_SYMBOL(ath_initialize_timer_int);
EXPORT_SYMBOL(ath_set_timer_period);
EXPORT_SYMBOL(ath_timer_is_initialized);
EXPORT_SYMBOL(ath_start_timer);
EXPORT_SYMBOL(ath_cancel_timer);
EXPORT_SYMBOL(ath_timer_is_active);
EXPORT_SYMBOL(ath_free_timer_int);

/* Export ath_wbuf.c functions */
EXPORT_SYMBOL(__wbuf_uapsd_update);

/* Export if_bus.c functions*/
EXPORT_SYMBOL(bus_read_cachesize);

/* Export if_ath_pci.c functions */
extern unsigned int ahbskip;
EXPORT_SYMBOL(ahbskip);

/* Export osif_proxyarp.c functions */
int wlan_proxy_arp(wlan_if_t vap, wbuf_t wbuf);
EXPORT_SYMBOL(wlan_proxy_arp);

int do_proxy_arp(wlan_if_t vap, qdf_nbuf_t netbuf);
EXPORT_SYMBOL(do_proxy_arp);

/* Export ext_ioctl_drv_if.c functions */
EXPORT_SYMBOL(ieee80211_extended_ioctl_chan_switch);
EXPORT_SYMBOL(ieee80211_extended_ioctl_chan_scan);
EXPORT_SYMBOL(ieee80211_extended_ioctl_rep_move);

#if UMAC_SUPPORT_ACFG
/* Export ieee80211_ioctl_acfg.c */
void acfg_convert_to_acfgprofile (struct ieee80211_profile *profile,
                                acfg_radio_vap_info_t *acfg_profile);
EXPORT_SYMBOL(acfg_convert_to_acfgprofile);
#endif
/* Export ieee80211_admctl.c */
#if UMAC_SUPPORT_ADMCTL
EXPORT_SYMBOL(ieee80211_admctl_classify);
#endif

/* ieee80211_vi_dbg.c */
#if UMAC_SUPPORT_VI_DBG
EXPORT_SYMBOL(ieee80211_vi_dbg_input);
#endif

/* ieee80211_common.c */
EXPORT_SYMBOL(IEEE80211_DPRINTF);

#if QCA_PARTNER_PLATFORM
EXPORT_SYMBOL(wlan_vap_get_devhandle);
EXPORT_SYMBOL(wlan_vap_unregister_mlme_event_handlers);
EXPORT_SYMBOL(wlan_vap_register_mlme_event_handlers);
EXPORT_SYMBOL(ieee80211_find_wds_node);
#endif
void ic_reset_params(struct ieee80211com *ic);
EXPORT_SYMBOL(ic_reset_params);

EXPORT_SYMBOL(IEEE80211_DPRINTF_IC);
EXPORT_SYMBOL(IEEE80211_DPRINTF_IC_CATEGORY);
EXPORT_SYMBOL(ieee80211_dfs_proc_cac);
EXPORT_SYMBOL(ieee80211_bringup_ap_vaps);
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
EXPORT_SYMBOL(ieee80211_ioctl_ald_getStatistics);
#endif
EXPORT_SYMBOL(ieee80211_bcn_prb_template_update);
EXPORT_SYMBOL(ieee80211_is_pmf_enabled);
EXPORT_SYMBOL(wlan_get_operational_rates);
EXPORT_SYMBOL(ieee80211_aplist_get_desired_bssid_count);
EXPORT_SYMBOL(ieee80211_vap_get_opmode);
EXPORT_SYMBOL(delete_default_vap_keys);
EXPORT_SYMBOL(ieee80211_reset_erp);
EXPORT_SYMBOL(ieee80211_wnm_bssmax_updaterx);
EXPORT_SYMBOL(ieee80211_notify_michael_failure);
EXPORT_SYMBOL(ieee80211_secondary20_channel_offset);
EXPORT_SYMBOL(wlan_vap_get_opmode);
EXPORT_SYMBOL(ieee80211_send_mgmt);
EXPORT_SYMBOL(ieee80211_vap_mlme_inact_erp_timeout);
EXPORT_SYMBOL(ieee80211_vap_is_any_running);
EXPORT_SYMBOL(osif_restart_vaps);
EXPORT_SYMBOL(ieee80211_wme_initparams);
EXPORT_SYMBOL(wlan_deauth_all_stas);
EXPORT_SYMBOL(ieee80211com_note);
EXPORT_SYMBOL(ieee80211_setpuregbasicrates);
EXPORT_SYMBOL(ieee80211_set_shortslottime);
EXPORT_SYMBOL(ieee80211_set_protmode);
EXPORT_SYMBOL(ieee80211_aplist_get_desired_bssid);
EXPORT_SYMBOL(wlan_vap_get_bssid);
EXPORT_SYMBOL(ieee80211_add_vhtcap);
EXPORT_SYMBOL(ieee80211_get_chan_centre_freq);
EXPORT_SYMBOL(ieee80211_mhz2ieee);
#if UMAC_SUPPORT_ACFG
EXPORT_SYMBOL(acfg_send_event);
EXPORT_SYMBOL(acfg_chan_stats_event);
#endif
EXPORT_SYMBOL(ieee80211_vap_deliver_stop_error);
EXPORT_SYMBOL(ieee80211_mlme_sta_swbmiss_timer_disable);
EXPORT_SYMBOL(wlan_get_param);
EXPORT_SYMBOL(wlan_get_key);
EXPORT_SYMBOL(ieee80211_mlme_sta_bmiss_ind);
EXPORT_SYMBOL(ieee80211_phymode_name);
#if ATH_SUPPORT_WIFIPOS
EXPORT_SYMBOL(ieee80211com_init_netlink);
#endif
EXPORT_SYMBOL(ieee80211_mlme_sta_swbmiss_timer_alloc_id);
EXPORT_SYMBOL(wlan_channel_ieee);

#if MESH_MODE_SUPPORT
extern void ieee80211_check_timeout_mesh_peer(void *arg, wlan_if_t vaphandle);
EXPORT_SYMBOL(ieee80211_check_timeout_mesh_peer);
extern void os_if_tx_free_batch_ext(struct sk_buff *bufs, int tx_err);
EXPORT_SYMBOL(os_if_tx_free_batch_ext);
#endif /* MESH_MODE_SUPPORT */
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
EXPORT_SYMBOL(ieee80211_node_clear_keys);
#endif
EXPORT_SYMBOL(ieee80211_get_txstreams);
extern uint32_t promisc_is_active (struct ieee80211com *ic);
EXPORT_SYMBOL(promisc_is_active);
#if UMAC_SUPPORT_VI_DBG
EXPORT_SYMBOL(ieee80211_vi_dbg_print_stats);
#endif
EXPORT_SYMBOL(ieee80211_getstreams);
EXPORT_SYMBOL(ieee80211_vap_get_aplist_config);
EXPORT_SYMBOL(ieee80211_add_htcap);
EXPORT_SYMBOL(ieee80211_session_timeout);
EXPORT_SYMBOL(wlan_set_param);
EXPORT_SYMBOL(transcap_dot3_to_eth2);
EXPORT_SYMBOL(wlan_get_vap_opmode_count);
EXPORT_SYMBOL(ieee80211_get_num_vaps_up);
EXPORT_SYMBOL(ieee80211_get_num_ap_vaps_up);
EXPORT_SYMBOL(ieee80211_vap_deliver_stop);
extern unsigned int enable_pktlog_support;
EXPORT_SYMBOL(enable_pktlog_support);
EXPORT_SYMBOL(wlan_mgmt_txrx_register_rx_cb);
EXPORT_SYMBOL(wlan_mgmt_txrx_deregister_rx_cb);
EXPORT_SYMBOL(wlan_mgmt_txrx_beacon_frame_tx);
EXPORT_SYMBOL(wlan_mgmt_txrx_vdev_drain);
EXPORT_SYMBOL(wlan_find_full_channel);
EXPORT_SYMBOL(wlan_scan_update_channel_list);
EXPORT_SYMBOL(util_wlan_scan_db_iterate_macaddr);
EXPORT_SYMBOL(wlan_pdev_scan_in_progress);
EXPORT_SYMBOL(wlan_scan_cache_update_callback);
EXPORT_SYMBOL(ucfg_scan_register_bcn_cb);
EXPORT_SYMBOL(wlan_chan_to_freq);
EXPORT_SYMBOL(ieee80211_compute_nss);
EXPORT_SYMBOL(wlan_scan_update_wide_band_scan_config);
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
EXPORT_SYMBOL(wlan_crypto_get_param);
EXPORT_SYMBOL(wlan_crypto_delkey);
EXPORT_SYMBOL(wlan_crypto_setkey);
EXPORT_SYMBOL(wlan_crypto_getkey);
EXPORT_SYMBOL(wlan_crypto_restore_keys);
EXPORT_SYMBOL(wlan_crypto_is_pmf_enabled);
EXPORT_SYMBOL(wlan_crypto_vdev_is_pmf_enabled);
#endif
#if DBDC_REPEATER_SUPPORT
EXPORT_SYMBOL(wlan_update_radio_priorities);
#endif
#if ATH_SUPPORT_WRAP && DBDC_REPEATER_SUPPORT
EXPORT_SYMBOL(osif_set_primary_radio_event);
#endif

#ifdef WLAN_SUPPORT_FILS
EXPORT_SYMBOL(wlan_mgmt_txrx_fd_action_frame_tx);
#endif
EXPORT_SYMBOL(wlan_objmgr_peer_obj_create);
EXPORT_SYMBOL(ieee80211_try_mark_node_for_delayed_cleanup);

#if ATH_ACS_DEBUG_SUPPORT
/* Exporting Functions from ACS debug framework */
EXPORT_SYMBOL(acs_debug_add_bcn);
EXPORT_SYMBOL(acs_debug_add_chan_event);
EXPORT_SYMBOL(acs_debug_cleanup);
#endif

EXPORT_SYMBOL(is_node_self_peer);
EXPORT_SYMBOL(wlan_objmgr_free_node);
EXPORT_SYMBOL(wlan_is_ap_cac_timer_running);
EXPORT_SYMBOL(ieee80211_get_num_active_vaps);
EXPORT_SYMBOL(wlan_pdev_mlme_vdev_sm_chan_change);
