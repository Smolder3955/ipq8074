/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*/

/*
 *This File is holds internal data structure for SON.
*/
#ifndef _SON_INTERNAL_H_
#define _SON_INTERNAL_H_

#include <wlan_son_types.h>

struct in_network_table {
	/* node table list */
	TAILQ_ENTRY(in_network_table) table_list;
	/* MAC address */
	u_int8_t  macaddr[IEEE80211_ADDR_LEN];
	/* channel */
	u_int8_t  channel;
};

struct ieee80211_uplinkinfo {
	struct wlan_objmgr_vdev *vdev;
	u_int16_t rate_estimate;
	u_int8_t root_distance;
	u_int8_t essid[IEEE80211_NWID_LEN];
	u_int8_t bssid[IEEE80211_ADDR_LEN];
	u_int8_t otherband_bssid[IEEE80211_ADDR_LEN];
	u_int8_t island_detected;
	int snr;
};

struct son_psoc_priv {
	struct wlan_objmgr_psoc *psoc;
};

struct son_pdev_priv {
	struct wlan_objmgr_pdev  *pdev;
	qdf_atomic_t son_enabled;
	qdf_spinlock_t  son_lock;
	/* Configuration parameters set from userspace. */
	ieee80211_bsteering_param_t son_config_params;
	/* Debugging control parameters set from userspace.*/
	ieee80211_bsteering_dbg_param_t son_dbg_config_params;
	/* Whether utilization has been requested but not yet completed or not. */
	bool  son_chan_util_requested;
	/* Channel on which the device is operating.
	   This caches the last active channel for two purposes:
	1) To determine whether a channel utilization report is relevant.
	2) To detect when the channel has changed between utilization
	measurements (and thus the data should be invalidated).*/
	u_int8_t  son_active_ieee_chan_num;
	/* Running total of the utilization data collected so far.*/
	u_int32_t   son_chan_util_samples_sum;
	/* The number of valid samples in the utilization_samples array.*/
	u_int16_t   son_chan_util_num_samples;
	/* Inactivity timer */
	qdf_timer_t  son_inact_timer;
	/* Normal inactivity timer value */
	u_int16_t  son_inact[BSTEERING_MAX_CLIENT_CLASS_GROUP];
	/* Overload inactivity timer value */
	u_int16_t  son_inact_overload[BSTEERING_MAX_CLIENT_CLASS_GROUP];
	/* Flag indicating the VAP overload or not */
	bool  son_vap_overload;
	/*   Members for inst RSSI measument
	 ===============================================================
	 Flag indicating if an inst RSSI measurement is in progress */
	bool son_inst_rssi_inprogress;
	/* The MAC address of the STA, whose RSSI measurement is in progress. */
	u_int8_t son_inst_rssi_macaddr[6];
	/* The number of valid RSSI samples measured through NDP */
	/* before generating an instant RSSI measurement event */
	u_int8_t son_inst_rssi_num_samples;
	/* The average of RSSI samples */
	u_int8_t  son_avg_inst_rssi;
	/* The number of valid RSSI samples gathered */
	u_int8_t son_inst_rssi_count;
	/* The number of invalid RSSI samples gathered*/
	u_int8_t son_inst_rssi_err_count;
	/* Inst RSSI measurement timer */
	qdf_timer_t son_inst_rssi_timer;
	/* Inst RSSI measurement log enable */
	bool son_inst_rssi_log;
	/* ===============================================================
	   Members for channel utilization monitoring
	   =============================================================== */
	/* Periodic timer to check channel utilization.*/
	qdf_timer_t  son_chan_util_timer;
	/* Timer to replicate behavior of WMI event that comes periodically
	   to send STA stats update to lbd for the purpose of
	   interference detection*/
	qdf_timer_t son_interference_stats_timer;
	/* The interval at which STA stats are sent to LBD via netlink msg
	   in the direct attach path to replicate the periodic nature
	   of WMI events in the offload path */
	u_int32_t sta_stats_update_interval_da;
	/* Ensure it is not a double disable.*/
	/*DA we dont need to report back pending acs report  */
	u_int32_t son_delayed_ch_load_rpt;
	/* Son rssi xing threshold used only for DA*/
	u_int32_t  son_rssi_xing_report_delta;
	struct wlan_objmgr_vdev *son_iv;
	/* backhaul bssid (connected AP) in RE mode */
	u_int8_t uplink_bssid[MAC_ADDR_LEN];
	u_int16_t uplink_rate;	 /* backhaul rate(Mbps) in RE mode */
	/* backhaul rate(Mbps) of serving AP */
	u_int16_t serving_ap_backhaul_rate;
	/* Uplink (Bakchaul) snr */
	u_int8_t uplink_snr;
	/* temp flag in case invalid event is sent through rssi measurement mechanism*/
	bool rssi_invalid_event;
	 /* backhaul otherband bssid in RE mode */
	u_int8_t son_otherband_uplink_bssid[IEEE80211_ADDR_LEN];
	/* Otherband bssid serving the same SSID */
	u_int8_t son_otherband_bssid[IEEE80211_ADDR_LEN];
	/* in network table for 2G */
	TAILQ_HEAD(, in_network_table) in_network_table_2g;
	/* Number of Vaps for which SON is enabled */
	u_int8_t son_num_vap;
	/* received uplink rate for mixed bh*/
	u_int16_t recv_ul_rate_mixedbh;
	/* current uplink rate for mixed bh*/
	u_int16_t curr_ul_rate_mixedbh;
	/* son backhaul type for mixed bh */
	u_int8_t son_backhaul_type;
	/* Configuration parameters set from userspace for MAP */
	ieee80211_bsteering_map_param_t map_config_params;
};

struct son_peer_priv {
	struct wlan_objmgr_peer *peer;
	u_int32_t son_rssi_seq; /* last RSSI sequence number */
	/* inactivity indicators for band steering */
	u_int16_t son_inact; /* inactivity mark count */
	u_int16_t son_inact_reload; /* inactivity reload value */
	u_int32_t tx_packet_count; /* TX packet count */
	u_int32_t rx_packet_count; /* TX packet count */
	u_int8_t son_rssi;    /* received  ssi used in bsteering */
	u_int8_t son_max_txpower;/* maximum TX power the STA
                                    supports */
	u_int8_t son_last_rssi;
	/* Band steering related flags */
	wbuf_t assoc_frame; // Assoc frame for MAP
	u_int8_t ni_bs_rssi_prev_xing_map; /* store the prev xing used for MAP */
	/* if the node is under steering */
	bool son_steering_flag : 1;
	bool son_inact_flag : 1; /* inactivity flag */
	/* we dont need it for OL as we are already
	   storing it in peer structure in stats path*/
	u_int32_t son_last_tx_rate_da;
	u_int8_t ni_whc_apinfo_flags;
	u_int8_t ni_whc_rept_info;
	u_int8_t son_peer_class_group; /* Client Classification Group */
};

struct son_vdev_priv {
	struct wlan_objmgr_vdev *vdev;
	qdf_atomic_t v_son_enabled;
	qdf_atomic_t v_ackrssi_enabled;
	osif_dev *son_osifp;
	u_int32_t lbd_pid;
	qdf_atomic_t event_bcast_enabled;
	u_int32_t son_rept_multi_special:1;
#define IEEE80211_BS_MAX_STA_WH_24G 10
	ieee80211_bsteering_probe_resp_wh_entry_t iv_bs_prb_resp_wh_table[IEEE80211_BS_MAX_STA_WH_24G];
	ieee80211_bsteering_probe_resp_allow_entry_t iv_bs_prb_resp_allow_table[IEEE80211_BS_MAX_STA_WH_24G];
	u_int8_t                   iv_connected_REs;   /* number of connected REs */
	u_int32_t                  iv_whc_scaling_factor; /* scaling factor for WHC best uplink algorithm */
	u_int32_t                  iv_whc_skip_hyst; /* skip hyst for best ap */
	u_int8_t whc_root_ap_distance;
	int8_t cap_rssi;
	u_int8_t bestul_hyst;
	u_int8_t iv_map:1, /* Flag to check if map is enabled */
	         iv_mapbh:1, /* flag to check if map backhaul is set */
	         iv_mapfh:1, /* flag to check if map fronthaul is set */
	         iv_mapbsta:1, /* flag to check if map bSta is set */
	         iv_mapvapup:1; /* flag to check if vap is up */
};

int  son_core_enable_disable_vdev_events(struct wlan_objmgr_vdev *vdev,
					 u_int8_t bstering_enable);

int son_core_pdev_enable_disable_steering(struct wlan_objmgr_vdev *vdev,
					  int enable);

int son_core_pdev_enable_ackrssi(struct wlan_objmgr_vdev *vdev,
                                        u_int8_t ackrssi_enable);

void son_core_mark_peer_inact(struct wlan_objmgr_peer *peer, bool invactive);

int son_core_null_frame_tx(struct wlan_objmgr_vdev *vdev,
			   char *macid,
			   u_int8_t num_rssi_sample);

int8_t son_core_set_get_peer_class_group(struct wlan_objmgr_vdev *vdev,
				 char *macaddr,
				 u_int8_t *bsteering_peer_class_group,
				 bool set);

int8_t son_core_set_get_overload(struct wlan_objmgr_vdev *vdev,
				 u_int8_t *bsteering_overload,
				 bool set);

int8_t son_core_set_get_steering_params(struct wlan_objmgr_vdev *vdev,
					struct ieee80211_bsteering_param_t *bsteering_param,
					bool action);

int8_t son_core_set_dbg_params(struct wlan_objmgr_vdev *vdev,
			       struct ieee80211_bsteering_dbg_param_t *bsteering_dbg_param);

int8_t son_core_set_steer_in_prog(struct wlan_objmgr_peer *peer,
				  u_int8_t steer_in_prog);

int8_t son_core_set_stastats_intvl(struct wlan_objmgr_peer *peer,
				   u_int8_t stastats_invl);

int8_t son_core_get_dbg_params(struct wlan_objmgr_vdev *vdev,
			       struct ieee80211_bsteering_dbg_param_t *bsteering_dbg_param);


int8_t son_core_peer_set_probe_resp_allow_2g(struct wlan_objmgr_vdev *vdev,
					     char *dstmac,
					     u_int8_t action /*add or remove */);

bool son_core_is_probe_resp_wh_2g(struct wlan_objmgr_vdev *vdev,
				  char *mac_addr, u_int8_t sta_rssi);

void son_core_reset_chan_utilization(struct son_pdev_priv *pd_priv);

int son_get_max_MCS(enum ieee80211_phymode phymode,
		    u_int16_t rx_vht_mcs_map, u_int16_t tx_vht_mcs_map,
		    const struct ieee80211_rateset *htrates,
		    const struct ieee80211_rateset *basic_rates);

int8_t son_core_get_root_dist(struct wlan_objmgr_vdev *vdev);

int8_t son_core_get_scaling_factor(struct wlan_objmgr_vdev *vdev);

void son_core_set_scaling_factor(struct wlan_objmgr_vdev *vdev,
				 int8_t scaling_factor);

int8_t son_core_get_skip_hyst(struct wlan_objmgr_vdev *vdev);

void son_core_set_skip_hyst(struct wlan_objmgr_vdev *vdev,
				 int8_t skip_hyst);

void son_core_set_uplink_rate(struct wlan_objmgr_vdev *vdev,
			      u_int32_t uplink_rate);

int son_core_set_map_rssi_policy(struct wlan_objmgr_vdev *vdev,
				 u_int8_t rssi_threshold, u_int8_t rssi_hysteresis);

void son_core_set_cap_rssi(struct wlan_objmgr_vdev *vdev, int8_t rssi);

int son_core_get_map_operable_channels(struct wlan_objmgr_vdev *vdev,
				       struct map_op_chan_t *map_op_chan);

int son_core_get_map_apcap(struct wlan_objmgr_vdev *vdev, mapapcap_t *apcap);

int8_t son_core_get_cap_rssi(struct wlan_objmgr_vdev *vdev);
int son_core_get_cap_snr(struct wlan_objmgr_vdev *vdev, int *cap_snr);
wlan_phymode_e convert_phymode(struct wlan_objmgr_vdev *vdev);
int8_t son_core_set_otherband_bssid(struct wlan_objmgr_pdev *pdev, int *val);
int son_core_get_best_otherband_uplink_bssid(struct wlan_objmgr_pdev *pdev, char *bssid);
int son_core_set_best_otherband_uplink_bssid(struct wlan_objmgr_vdev *vdev, char *bssid);
int8_t son_core_set_root_dist(struct wlan_objmgr_vdev *vdev,
			      u_int8_t root_distance);
u_int16_t son_core_get_uplink_rate(struct wlan_objmgr_vdev *vdev);
u_int8_t son_core_get_uplink_snr(struct wlan_objmgr_vdev *vdev);

int son_core_get_map_esp_info(struct wlan_objmgr_vdev *vdev, map_esp_info_t *map_esp_info);

u_int8_t son_core_get_bestul_hyst(struct wlan_objmgr_vdev *vdev);
void son_core_set_bestul_hyst(struct wlan_objmgr_vdev *vdev, u_int8_t hyst);
int8_t son_core_set_innetwork_2g_mac(struct wlan_objmgr_vdev *vdev, const u_int8_t *macaddr,
				     int channel);
int8_t son_core_get_innetwork_2g_mac(struct wlan_objmgr_vdev *vdev, void *data, int32_t num_entries,
				     int8_t ch);
int son_core_get_assoc_frame(struct wlan_objmgr_vdev *vdev, char *macaddr,
			     char *info, u_int16_t *length);
bool son_core_is_peer_inact(struct wlan_objmgr_peer *peer);

#endif
