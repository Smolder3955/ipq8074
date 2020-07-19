/*
 * Copyright (c) 2017-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 */
#include <_ieee80211.h>
#include "ieee80211_ioctl.h"
#ifndef _WLAN_MLME_DISPATCHER_H
#define _WLAN_MLME_DISPATCHER_H
#include <wlan_reg_services_api.h>

uint8_t wlan_vdev_get_chan_num(struct wlan_objmgr_vdev *vdev);

int16_t wlan_vdev_get_chan_freq(struct wlan_objmgr_vdev *vdev);

enum phy_ch_width wlan_vdev_get_ch_width(struct wlan_objmgr_vdev *vdev);
enum wlan_phymode wlan_vdev_get_phymode(struct wlan_objmgr_vdev *vdev);
int wlan_vdev_acl_auth(struct wlan_objmgr_vdev *vdev, bool set, struct ieee80211req_athdbg *req_t);

int wlan_vdev_acl_probe(struct wlan_objmgr_vdev *vdev, bool set, struct ieee80211req_athdbg *req_t);

int wlan_vdev_local_disassoc(struct wlan_objmgr_vdev *vdev, struct wlan_objmgr_peer *peer);


int wlan_vdev_get_node_info(struct wlan_objmgr_vdev *vdev,
			    struct ieee80211req_athdbg *req);

int wlan_peer_get_node_max_MCS(struct wlan_objmgr_peer *peer);

QDF_STATUS wlan_peer_send_null(struct wlan_objmgr_peer *peer);

bool wlan_peer_update_sta_stats(struct wlan_objmgr_peer *peer,
				struct bs_sta_stats_ind *sta_stats,
                                void *stats);

u_int8_t wlan_peer_get_rssi(struct wlan_objmgr_peer *peer);

u_int32_t wlan_peer_get_rate(struct wlan_objmgr_peer *peer);

wlan_chan_t wlan_vdev_get_channel(struct wlan_objmgr_vdev *vdev);

int wlan_vdev_acs_set_user_chanlist(struct wlan_objmgr_vdev *vdev
				    , u_int8_t *extra);

u_int8_t wlan_vdev_get_rx_streams(struct wlan_objmgr_vdev *vdev);

int wlan_vdev_get_apcap(struct wlan_objmgr_vdev *vdev, mapapcap_t *apcap);

int wlan_vdev_add_acl_validity_timer(struct wlan_objmgr_vdev *vdev,
				     const u_int8_t *mac_addr,
				     u_int16_t validity_timer);

int wlan_vdev_acs_start_scan_report(struct wlan_objmgr_vdev *vdev , int val);

u_int8_t  wlan_vdev_get_chwidth(struct wlan_objmgr_vdev *vdev);

bool wlan_vdev_is_deleted_set(struct wlan_objmgr_vdev *vdev);
bool wlan_vdev_acl_is_probe_wh_set(struct wlan_objmgr_vdev *vdev,
				   const u_int8_t *mac_addr);
#if QCA_SUPPORT_SON
bool wlan_vdev_acl_is_drop_mgmt_set(struct wlan_objmgr_vdev *vdev,
                                    const u_int8_t *mac_addr);
#endif

int wlan_node_get_capability(struct wlan_objmgr_peer *peer,
			     struct bs_node_associated_ind *assoc);

int wlan_vdev_get_sec20chan_num(struct wlan_objmgr_vdev *vdev,
        uint8_t *sec20chan_num);

int wlan_vdev_get_sec20chan_freq_mhz(struct wlan_objmgr_vdev *vdev,
        uint16_t *sec20chan_freq);

uint32_t wlan_pdev_in_gmode(struct wlan_objmgr_pdev *pdev);

uint32_t wlan_pdev_in_amode(struct wlan_objmgr_pdev *pdev);

int wlan_pdev_get_esp_info(struct wlan_objmgr_pdev *pdev,
			   map_esp_info_t *map_esp_info);

int wlan_pdev_get_multi_ap_opclass(struct wlan_objmgr_pdev *pdev, mapapcap_t *apcap,
				   struct map_op_chan_t *map_op_chan);

uint32_t wlan_node_get_peer_chwidth(struct wlan_objmgr_peer *peer);

void wlan_node_get_peer_phy_info(struct wlan_objmgr_peer *peer, uint8_t *rxstreams, uint8_t *streams, uint8_t *cap, uint32_t *mode);
#define RXMIC_OFFSET 8

void wlan_set_peer_rate_legacy(struct wlan_objmgr_peer *peer,
                                struct ieee80211_rateset *rates);
void wlan_set_peer_rate_ht(struct wlan_objmgr_peer *peer,
           struct ieee80211_rateset *rates);
void wlan_set_peer_rate_vht(struct wlan_objmgr_peer *peer,
           u_int16_t vhtrate_map);
void wlan_peer_dump_rates(struct wlan_objmgr_peer *peer);
void wlan_deliver_mlme_evt_disassoc(struct wlan_objmgr_vdev *vdev,
           u_int8_t *stamac, struct wlan_objmgr_peer *peer);
int wlan_add_client(struct wlan_objmgr_vdev *vdev,
            struct wlan_objmgr_peer *peer, u_int16_t associd,
            u_int8_t qos, void *lrates,
            void *htrates, u_int16_t vhtrates, u_int16_t herates);
u_int16_t wlan_get_aid(struct wlan_objmgr_peer *peer);
int wlan_is_aid_set(struct wlan_objmgr_vdev *vdev, u_int16_t associd);
void wlan_set_aid(struct wlan_objmgr_vdev *vdev, u_int16_t associd);
void wlan_clear_aid(struct wlan_objmgr_vdev *vdev, u_int16_t associd);
void wlan_set_peer_aid(struct wlan_objmgr_peer *peer, u_int16_t associd);
void wlan_set_peer_flag(struct wlan_objmgr_peer *peer, int flag);
void wlan_clear_peer_flag(struct wlan_objmgr_peer *peer, int flag);
int wlan_peer_is_flag_set(struct wlan_objmgr_peer *peer, int flag);
int wlan_get_peer_num_streams(struct wlan_objmgr_peer *peer);
void wlan_peer_auth(struct wlan_objmgr_vdev *vdev, u_int8_t *stamac,
                u_int32_t authorize);
void wlan_peer_mlme_recv_assoc_request(struct wlan_objmgr_peer *peer);
int wlan_vdev_del_key(struct wlan_objmgr_vdev *vdev, u_int16_t keyix,
                u_int8_t *macaddr);
int wlan_vdev_set_key(struct wlan_objmgr_vdev *vdev, u_int8_t *macaddr,
            u_int8_t cipher, u_int16_t keyix, u_int32_t keylen,
            u_int8_t *keydata);
u_int32_t wlan_ucfg_get_maxphyrate(struct wlan_objmgr_vdev *vdev);
int wlan_vdev_get_curmode(struct wlan_objmgr_vdev *vdev);
uint8_t wlan_get_opclass(struct wlan_objmgr_vdev *vdev);
uint8_t wlan_get_prim_chan(struct wlan_objmgr_vdev *vdev);
uint8_t wlan_get_seg1(struct wlan_objmgr_vdev *vdev);
uint16_t wlan_peer_get_beacon_interval(struct wlan_objmgr_peer *peer);

QDF_STATUS wlan_vdev_get_elemid(struct wlan_objmgr_vdev *vdev,
				ieee80211_frame_type ftype, uint8_t *iebuf,
				uint32_t *ielen, uint32_t elem_id);

extern uint8_t phymode2convphymode[IEEE80211_MODE_11AXA_HE80_80 + 1];
int wlan_peer_is_extflag_set(struct wlan_objmgr_peer *peer, int flag);

#endif
