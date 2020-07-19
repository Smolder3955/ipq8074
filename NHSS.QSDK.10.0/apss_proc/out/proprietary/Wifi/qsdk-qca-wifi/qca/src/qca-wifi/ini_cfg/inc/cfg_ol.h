/*
 *
 * Copyright (c) 2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 */

#ifndef _CFG_OL_H_
#define _CFG_OL_H_

#include "cfg_define.h"
#include "wmi_unified_param.h"
#include "ol_if_athvar.h"
#include "ieee80211.h"

#define MAX_CFG 0xffffffff

#define CFG_OL_ENABLE_11AX_STUB \
	CFG_INI_BOOL("enable_11ax_stub", false, \
	"Enable 802.11ax stubbing support for testing. Valid only for QCA9984")

#define CFG_OL_TX_TCP_CKSUM \
	CFG_INI_BOOL("enable_tx_tcp_cksum", false, \
	"Enable Tx TCP Checksum")

#define CFG_OL_VOW_CONFIG \
	CFG_INI_BOOL("vow_config", false, \
	"VoW Configuration")

#define CFG_OL_CARRIER_VOW_CONFIG \
	CFG_INI_BOOL("carrier_vow_config", false, \
	"Enable Vow stats and Configuration")

#define CFG_OL_FW_VOW_STATS_ENABLE \
	CFG_INI_BOOL("fw_vow_stats_enable", false, \
	"Firmware VoW stats control")

#define CFG_OL_QWRAP_ENABLE \
	CFG_INI_BOOL("qwrap_enable", false, \
	"Enable qwrap target config")

#define CFG_OL_CCE_DISABLE \
	CFG_INI_BOOL("cce_disable", false, \
	"Disable Hardware CCE Component")

#define CFG_OL_LOW_MEM_SYSTEM \
	CFG_INI_BOOL("low_mem_system", false, \
	"Low Memory System")

#define CFG_OL_BEACON_OFFLOAD_DISABLE \
	CFG_INI_BOOL("beacon_offload_disable", false, \
	"Beacon offload disable")

#define CFG_OL_ENABLE_UART_PRINT \
	CFG_INI_BOOL("enableuartprint", false, \
	"Enable uart/serial prints from target")

#define CFG_OL_ENABLE_MESH_SUPPORT \
	CFG_INI_BOOL("mesh_support", false, \
	"Configure Mesh support")

#define CFG_OL_EAPOL_MINRATE_SET \
	CFG_INI_BOOL("eapol_minrate_set", false, \
	"Enable/Disable EAPOL Minrate")

#define CFG_OL_EAPOL_MINRATE_AC_SET \
	CFG_INI_UINT("eapol_minrate_ac_set", \
	0, 4, 0, \
	CFG_VALUE_OR_DEFAULT, "Set AC for the EAPOL minrate set")

#define CFG_OL_CFG80211_CONFIG \
	CFG_INI_BOOL("cfg80211_config", false, \
	"cfg80211 config(enable/disable)")

#define CFG_OL_CFG_IPHDR\
	CFG_INI_BOOL("iphdr_pad", true, \
	"Disable IP header padding to manage IP header unalignment")

#define CFG_OL_LTEU_SUPPORT \
	CFG_INI_UINT("lteu_support", \
	0, 0x4, 0, \
	CFG_VALUE_OR_DEFAULT, "LTEU support")

#define CFG_OL_BMI \
	CFG_INI_UINT("bmi", \
	0, 1, 0, \
	CFG_VALUE_OR_DEFAULT, "BMI Handling: 0 - Driver, 1 - User agent")

#define CFG_OL_MAX_DESC \
	CFG_INI_UINT("max_descs", \
	0, 2198, 0, \
	CFG_VALUE_OR_DEFAULT, "Override default max descriptors")

#define CFG_OL_MAX_PEERS \
	CFG_INI_UINT("max_peers", \
	0, 1024, 0, \
	CFG_VALUE_OR_DEFAULT, "Override default max peers")

#define CFG_OL_MAX_VDEVS \
	CFG_INI_UINT("max_vdevs", \
	0, 51, 0, \
	CFG_VALUE_OR_DEFAULT, "Override default max vdevs")

#define CFG_OL_ACBK_MIN_FREE \
	CFG_INI_UINT("OL_ACBKMinfree", \
	0, MAX_CFG, 0, \
	CFG_VALUE_OR_DEFAULT, "Min Free buffers reserved for AC-BK")

#define CFG_OL_ACBE_MIN_FREE \
	CFG_INI_UINT("OL_ACBEMinfree", \
	0, MAX_CFG, 0, \
	CFG_VALUE_OR_DEFAULT, "Min Free buffers reserved for AC-BE")

#define CFG_OL_ACVI_MIN_FREE \
	CFG_INI_UINT("OL_ACVIMinfree", \
	0, MAX_CFG, 0, \
	CFG_VALUE_OR_DEFAULT, "Min Free buffers reserved for AC-VI")

#define CFG_OL_ACVO_MIN_FREE \
	CFG_INI_UINT("OL_ACVOMinfree", \
	0, MAX_CFG, 0, \
	CFG_VALUE_OR_DEFAULT, "Min Free buffers reserved for AC-VO")

#define CFG_OL_OTP_MOD_PARAM \
	CFG_INI_UINT("otp_mod_param", \
	0, 0xffffffff, 0xffffffff, \
	CFG_VALUE_OR_DEFAULT, "OTP")

#define CFG_OL_EMU_TYPE \
	CFG_INI_UINT("emu_type", \
	0, 2, 0, \
	CFG_VALUE_OR_DEFAULT, "Emulation Type : 0-->ASIC, 1-->M2M, 2-->BB")

#define CFG_OL_MAX_ACTIVE_PEERS \
	CFG_INI_UINT("max_active_peers", \
	0, 50, 0, \
	CFG_VALUE_OR_DEFAULT, "Override max active peers in peer qcache")

#define CFG_OL_HW_MODE_ID \
	CFG_INI_UINT("hw_mode_id", \
	WMI_HOST_HW_MODE_SINGLE, WMI_HOST_HW_MODE_MAX, WMI_HOST_HW_MODE_MAX, \
	CFG_VALUE_OR_DEFAULT, "Preferred HW mode id")

#define CFG_OL_TGT_SCHED_PARAM \
	CFG_INI_UINT("tgt_sched_params", \
	0, MAX_CFG, 0, \
	CFG_VALUE_OR_DEFAULT, "Target Scheduler Parameters")

#define CFG_OL_FW_CODE_SIGN \
	CFG_INI_UINT("fw_code_sign", \
	0, 3, 0, \
	CFG_VALUE_OR_DEFAULT, "FW Code Sign")

#define CFG_OL_ALLOCRAM_TRACK_MAX \
	CFG_INI_UINT("allocram_track_max", \
	0, MAX_CFG, 0, \
	CFG_VALUE_OR_DEFAULT, "Enable target allocram tracking")

#define CFG_OL_MAX_VAPS \
	CFG_INI_UINT("max_vaps", \
	1, 51, 16, \
	CFG_VALUE_OR_DEFAULT, \
	"Max vap nodes for which mempool is statically allocated")

#define CFG_OL_MAX_CLIENTS \
	CFG_INI_UINT("max_clients", \
	1, 1024, 124, \
	CFG_VALUE_OR_DEFAULT, \
	"Max client nodes for which mempoolis statically allocated")

#define CFG_OL_FW_DUMP_OPTIONS \
	CFG_INI_UINT("fw_dump_options", \
	FW_DUMP_TO_FILE, FW_DUMP_ADD_SIGNATURE, \
	FW_DUMP_TO_CRASH_SCOPE, \
	CFG_VALUE_OR_DEFAULT, "Firmware dump options")

#define CFG_OL_ALLOW_MON_VAPS_IN_SR \
	CFG_INI_UINT("allow_mon_vaps_in_sr", \
	0, 1, \
	IEEE80211_ALLOW_MON_VAPS_IN_SR, \
	CFG_VALUE_OR_DEFAULT,"Allow monitor vaps to be present " \
	"while using Spatial Reuse")

#define CFG_OL_SRP_SR_CONTROL \
	CFG_INI_UINT("srp_sr_control", \
	0, 255, \
	IEEE80211_SRP_SR_CONTROL_DEF, \
	CFG_VALUE_OR_DEFAULT, "SRP SR Control")

#define CFG_OL_SRP_NON_SRG_SELF_OBSS_PD_THRESHOLD \
	CFG_INI_UINT("srp_non_srg_self_obss_pd_threshold", \
	0,1000,\
	IEEE80211_SRP_NON_SRG_SELF_OBSS_PD_THRESHOLD_DEF,\
	CFG_VALUE_OR_DEFAULT,"SRP Non-SRG Self OBSS-PD Threshold")

#define CFG_OL_SRP_NON_SRG_OBSS_PD_MAX_OFFSET_ENABLE \
	CFG_INI_UINT("self_obss_pd_tx_enable", \
	0, 1, \
	IEEE80211_SRP_NON_SRG_SELF_OBSS_PD_ENABLE, \
	CFG_VALUE_OR_DEFAULT, "SRP Non SRG OBSS PD MAX Offset Enable")

#define CFG_OL_SRP_NON_SRG_OBSS_PD_MAX_OFFSET \
	CFG_INI_UINT("srp_non_srg_obss_pd_max_offset", \
	0, 1000, \
	IEEE80211_SRP_NON_SRG_OBSS_PD_MAX_OFFSET_DEF, \
	CFG_VALUE_OR_DEFAULT, "SRP Non SRG OBSS PD MAX Offset")

#define CFG_OL_SRP_SRG_OBSS_PD_MIN_OFFSET \
	CFG_INI_UINT("srp_srg_obss_pd_min_offset", \
	0, 1000, \
	IEEE80211_SRP_SRG_OBSS_PD_MIN_OFFSET_DEF, \
	CFG_VALUE_OR_DEFAULT, "SRP SRG OBSS PD MIN Offset")

#define CFG_OL_SRP_SRG_OBSS_PD_MAX_OFFSET \
	CFG_INI_UINT("srp_srg_obss_pd_max_offset", \
	0, 1000, \
	IEEE80211_SRP_SRG_OBSS_PD_MAX_OFFSET_DEF, \
	CFG_VALUE_OR_DEFAULT, "SRP SRG OBSS PD MAX Offset")

#define CFG_OL_TWT_ENABLE \
	CFG_INI_BOOL("twt_enable", \
	true, \
	"TWT Enable/Disable")

#define CFG_OL_TWT_STA_CONG_TIMER_MS \
	CFG_INI_UINT("twt_sta_config_timer_ms", \
	0, 0xFFFFFFFF, 5000, \
	CFG_VALUE_OR_DEFAULT, "TWT STa config timer (ms)")

#define CFG_OL_TWT_MBSS_SUPPORT \
	CFG_INI_BOOL("twt_mbss_support", false, \
	"Enable TWT MBSS support")

#define CFG_OL_TWT_DEFAULT_SLOT_SIZE \
	CFG_INI_UINT("twt_default_slot_size", \
	0, 0xFFFFFFFF, 10, \
	CFG_VALUE_OR_DEFAULT, "TWT default slot size")

#define CFG_OL_TWT_CONGESTION_THRESH_SETUP \
	CFG_INI_UINT("twt_congestion_thresh_setup", \
	0, 100, 50, \
	CFG_VALUE_OR_DEFAULT, "Minimum congestion required to setup TWT")

#define CFG_OL_TWT_CONGESTION_THRESH_TEARDOWN \
	CFG_INI_UINT("twt_congestion_thresh_teardown", \
	0, 100, 20, \
	CFG_VALUE_OR_DEFAULT, "Minimum congestion for TWT teardown")

#define CFG_OL_TWT_CONGESTION_THRESH_CRITICAL \
	CFG_INI_UINT("twt_congestion_thresh_critical", \
	0, 100, 100, \
	CFG_VALUE_OR_DEFAULT, "TWT teardown Threshold above which TWT will not be active")

#define CFG_OL_TWT_INTERFERENCE_THRESH_TEARDOWN \
	CFG_INI_UINT("twt_interference_thresh_teardown", \
	0, 100, 80, \
	CFG_VALUE_OR_DEFAULT, "Interference threshold in percentage")

#define CFG_OL_TWT_INTERFERENCE_THRESH_SETUP \
	CFG_INI_UINT("twt_interference_thresh_setup", \
	0, 100, 50, \
	CFG_VALUE_OR_DEFAULT, "TWT Setup Interference threshold in percentage")

#define CFG_OL_TWT_MIN_NUM_STA_SETUP \
	CFG_INI_UINT("twt_min_no_sta_setup", \
	0, 4096, 10, \
	CFG_VALUE_OR_DEFAULT, "Minimum num of STA required for TWT setup")

#define CFG_OL_TWT_MIN_NUM_STA_TEARDOWN \
	CFG_INI_UINT("twt_min_no_sta_teardown", \
	0, 4096, 2, \
	CFG_VALUE_OR_DEFAULT, "Minimum num of STA below which TWT will be torn down")

#define CFG_OL_TWT_NUM_BCMC_SLOTS \
	CFG_INI_UINT("twt_no_of_bcast_mcast_slots", \
	0, 100, 2, \
	CFG_VALUE_OR_DEFAULT, "num of bcast/mcast TWT slots")

#define CFG_OL_TWT_MIN_NUM_SLOTS \
	CFG_INI_UINT("twt_min_no_twt_slots", \
	0, 1000, 2, \
	CFG_VALUE_OR_DEFAULT, "Minimum num of TWT slots")

#define CFG_OL_TWT_MAX_NUM_STA_TWT \
	CFG_INI_UINT("twt_max_no_sta_twt", \
	0, 1000, 500, \
	CFG_VALUE_OR_DEFAULT, "Maximum num of STA TWT slots")

#define CFG_OL_TWT_MODE_CHECK_INTERVAL \
	CFG_INI_UINT("twt_mode_check_interval", \
	0, 0xFFFFFFFF, 10000, \
	CFG_VALUE_OR_DEFAULT, "Interval between two successive check for TWT mode")

#define CFG_OL_TWT_ADD_STA_SLOT_INTERVAL \
	CFG_INI_UINT("twt_add_sta_slot_interval", \
	0, 0xFFFFFFFF, 1000, \
	CFG_VALUE_OR_DEFAULT, "Interval between decision making for TWT slot creation")

#define CFG_OL_TWT_REMOVE_STA_SLOT_INTERVAL \
	CFG_INI_UINT("twt_remove_sta_slot_interval", \
	0, 0xFFFFFFFF, 5000, \
	CFG_VALUE_OR_DEFAULT, "Interval between decision making for TWT slot removal")

#define CFG_OL_AP_BSS_COLOR_COLLISION_DETECTION \
	CFG_INI_BOOL("ap_bss_color_collision_detection", true, \
	"AP BSS COLOR COLLISION DETECTION")

#define CFG_OL_STA_BSS_COLOR_COLLISION_DETECTION \
	CFG_INI_BOOL("sta_bss_color_collision_detection", true, \
	"STA BSS COLOR COLLISION DETECTION")

#define CFG_OL_SR_ENABLE \
	CFG_INI_BOOL("spatial_reuse_enable", true, \
	"Enable Spatial Reuse IE")

#define CFG_OL_MBSS_IE_ENABLE \
	CFG_INI_BOOL("mbss_ie_enable", false, \
	"Enable MBSS IE")

#define CFG_OL_PEER_RATE_STATS \
	CFG_INI_BOOL("enable_rdk_stats", false, \
	"ENABLE RDK STATS")

#define CFG_OL_PRI20_CFG_BLOCKCHANLIST \
	CFG_INI_STRING("pri20_cfg_blockchanlist", \
		       0, 512, "", \
		       "Primary 20MHz CFG block channel list")

#define CFG_OL_PEER_DEL_WAIT_TIME \
	CFG_INI_UINT("g_peer_del_wait_time", \
	1000, 10000, 5000, \
	CFG_VALUE_OR_DEFAULT, "Timeout to print peer refs")

#define CFG_OL_OFFCHAN_SCAN_DWELL_TIME \
	CFG_INI_UINT("offchan_dwell_time", \
	0, 2000, 1500, \
	CFG_VALUE_OR_DEFAULT, "Offchan Scan Dwell Time")

#define CFG_OL \
	CFG(CFG_OL_ENABLE_11AX_STUB) \
	CFG(CFG_OL_TX_TCP_CKSUM) \
	CFG(CFG_OL_VOW_CONFIG) \
	CFG(CFG_OL_CARRIER_VOW_CONFIG) \
	CFG(CFG_OL_FW_VOW_STATS_ENABLE) \
	CFG(CFG_OL_QWRAP_ENABLE) \
	CFG(CFG_OL_CCE_DISABLE) \
	CFG(CFG_OL_LOW_MEM_SYSTEM) \
	CFG(CFG_OL_BEACON_OFFLOAD_DISABLE) \
	CFG(CFG_OL_ENABLE_UART_PRINT) \
	CFG(CFG_OL_ENABLE_MESH_SUPPORT) \
	CFG(CFG_OL_LTEU_SUPPORT) \
	CFG(CFG_OL_CFG80211_CONFIG) \
	CFG(CFG_OL_CFG_IPHDR) \
	CFG(CFG_OL_BMI) \
	CFG(CFG_OL_MAX_DESC) \
	CFG(CFG_OL_MAX_PEERS) \
	CFG(CFG_OL_MAX_VDEVS) \
	CFG(CFG_OL_ACBK_MIN_FREE) \
	CFG(CFG_OL_ACBE_MIN_FREE) \
	CFG(CFG_OL_ACVI_MIN_FREE) \
	CFG(CFG_OL_ACVO_MIN_FREE) \
	CFG(CFG_OL_OTP_MOD_PARAM) \
	CFG(CFG_OL_EMU_TYPE) \
	CFG(CFG_OL_MAX_ACTIVE_PEERS) \
	CFG(CFG_OL_HW_MODE_ID) \
	CFG(CFG_OL_TGT_SCHED_PARAM) \
	CFG(CFG_OL_FW_CODE_SIGN) \
	CFG(CFG_OL_ALLOCRAM_TRACK_MAX) \
	CFG(CFG_OL_MAX_VAPS) \
	CFG(CFG_OL_MAX_CLIENTS) \
	CFG(CFG_OL_FW_DUMP_OPTIONS) \
	CFG(CFG_OL_SRP_SR_CONTROL) \
	CFG(CFG_OL_SRP_NON_SRG_SELF_OBSS_PD_THRESHOLD) \
	CFG(CFG_OL_SRP_NON_SRG_OBSS_PD_MAX_OFFSET_ENABLE) \
	CFG(CFG_OL_ALLOW_MON_VAPS_IN_SR) \
	CFG(CFG_OL_SRP_NON_SRG_OBSS_PD_MAX_OFFSET) \
	CFG(CFG_OL_SRP_SRG_OBSS_PD_MIN_OFFSET) \
	CFG(CFG_OL_SRP_SRG_OBSS_PD_MAX_OFFSET)\
	CFG(CFG_OL_EAPOL_MINRATE_SET) \
	CFG(CFG_OL_EAPOL_MINRATE_AC_SET) \
	CFG(CFG_OL_TWT_ENABLE) \
	CFG(CFG_OL_TWT_STA_CONG_TIMER_MS) \
	CFG(CFG_OL_TWT_MBSS_SUPPORT) \
	CFG(CFG_OL_TWT_DEFAULT_SLOT_SIZE) \
	CFG(CFG_OL_TWT_CONGESTION_THRESH_SETUP) \
	CFG(CFG_OL_TWT_CONGESTION_THRESH_TEARDOWN) \
	CFG(CFG_OL_TWT_CONGESTION_THRESH_CRITICAL) \
	CFG(CFG_OL_TWT_INTERFERENCE_THRESH_TEARDOWN) \
	CFG(CFG_OL_TWT_INTERFERENCE_THRESH_SETUP) \
	CFG(CFG_OL_TWT_MIN_NUM_STA_SETUP) \
	CFG(CFG_OL_TWT_MIN_NUM_STA_TEARDOWN) \
	CFG(CFG_OL_TWT_NUM_BCMC_SLOTS) \
	CFG(CFG_OL_TWT_MIN_NUM_SLOTS) \
	CFG(CFG_OL_TWT_MAX_NUM_STA_TWT) \
	CFG(CFG_OL_TWT_MODE_CHECK_INTERVAL) \
	CFG(CFG_OL_TWT_ADD_STA_SLOT_INTERVAL) \
	CFG(CFG_OL_TWT_REMOVE_STA_SLOT_INTERVAL) \
	CFG(CFG_OL_AP_BSS_COLOR_COLLISION_DETECTION) \
	CFG(CFG_OL_STA_BSS_COLOR_COLLISION_DETECTION) \
	CFG(CFG_OL_MBSS_IE_ENABLE) \
	CFG(CFG_OL_SR_ENABLE) \
	CFG(CFG_OL_PEER_RATE_STATS) \
	CFG(CFG_OL_PRI20_CFG_BLOCKCHANLIST) \
	CFG(CFG_OL_PEER_DEL_WAIT_TIME) \
	CFG(CFG_OL_OFFCHAN_SCAN_DWELL_TIME)

#endif /*_CFG_OL_H_*/
