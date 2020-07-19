/*
 * Copyright (c) 2017-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */

#include <wlan_objmgr_cmn.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_objmgr_peer_obj.h>
#include <wlan_osif_priv.h>
#include <if_athvar.h>
#include <wlan_mlme_dp_dispatcher.h>
#include <da_dp_public.h>

#define WLAN_FC1_PWR_MGT_SHIFT        4
#define MAX_PRIVACY_FILTERS           4	/* max privacy filters */
#if ATH_SUPPORT_DSCP_OVERRIDE
#define DA_HOST_DSCP_MAP_MAX         (64)
#endif
typedef enum _privacy_filter {
	PRIVACY_FILTER_ALWAYS,
	PRIVACY_FILTER_KEY_UNAVAILABLE,
} privacy_filter;

typedef enum _privacy_filter_packet_type {
	PRIVACY_FILTER_PACKET_UNICAST,
	PRIVACY_FILTER_PACKET_MULTICAST,
	PRIVACY_FILTER_PACKET_BOTH
} privacy_filter_packet_type;

/**
 * Privacy exemption filter
 * @ether_type: type of ethernet to apply this filter, in host byte order
 */
typedef struct _privacy_excemption_filter {
	u_int16_t ether_type;
	privacy_filter filter_type;
	privacy_filter_packet_type packet_type;
} privacy_exemption;

/**
 * struct dadp_pdev - PDEV datapath object for Direct attach
 * @wme_hipri_traffic: VI/VO frames in beacon interval
 * @pdev_tx_coalescing: NB Tx Coalescing is a per-device setting to prevent non-PCIe (especially cardbus),
 * 						11n devices running into underrun conditions on certain PCI chipsets.
 * @pdev_softap_enable: For Lenovo SoftAP
 * @sc_amsdu_txq: amsdu tx requests
 * @sc_amsdu_flush_timer: amsdu flush timer
 * @amsdu_txq_lock: amsdu spinlock
 */
struct dadp_pdev {
	struct wlan_objmgr_pdev *pdev;
	struct ath_softc_net80211 *scn;
	osdev_t osdev;
	u_int8_t pdev_tid_override_queue_mapping;
	u_int8_t pdev_override_dscp;
	u_int8_t pdev_override_igmp_dscp;
	u_int8_t pdev_override_hmmc_dscp;
	u_int8_t pdev_dscp_igmp_tid;
	u_int8_t pdev_dscp_hmmc_tid;
	u_int32_t pdev_dscp_tid_map[NUM_DSCP_MAP][IP_DSCP_MAP_LEN];
	u_int wme_hipri_traffic;
	struct wlan_pdev_phy_stats pdev_phy_stats[IEEE80211_MODE_MAX];
#ifdef ATH_COALESCING
	ieee80211_coalescing_state pdev_tx_coalescing;
#endif
	bool pdev_softap_enable;
	u_int32_t pdev_addba_mode;
	u_int8_t pdev_broadcast[IEEE80211_ADDR_LEN];
	u_int32_t pdev_athextcap;

	TAILQ_HEAD(ath_amsdu_txq, ath_amsdu_tx) sc_amsdu_txq;
	struct ath_timer sc_amsdu_flush_timer;
	spinlock_t amsdu_txq_lock;
	u_int32_t pdev_amsdu_max_size;
	struct wlan_objmgr_vdev *pdev_mon_vdev;
	u_int32_t pdev_minframesize;
	u_int32_t pdev_dropstaquery:1,
						pdev_blkreportflood:1,
						pdev_min_rssi_enable:1;
	int pdev_min_rssi;
	void *extap_hdl;
};

/**
 * struct wlan_stats - VDEV wlan stats
 * @is_crypto_enmicfail: en-MIC failed
 * @is_rx_nowds: 4-addr packets received with no wds enabled
 * @is_rx_wrongdir: rx wrong direction
 * @is_rx_mcastecho: rx discard 'cuz mcast echo
 * @is_rx_unauth: rx on unauthorized port
 * @is_rx_notassoc: is_rx_notassoc
 * @is_rx_ctl: rx discard ctrl frames
 * @is_rx_decap: rx decapsulation failed
 */
struct wlan_stats {
	u_int32_t is_tx_nobuf;
	u_int32_t is_tx_nonode;
	u_int32_t is_crypto_enmicfail;
	u_int32_t is_rx_tooshort;
	u_int32_t is_rx_wrongbss;
	u_int32_t is_rx_nowds;
	u_int32_t is_rx_wrongdir;
	u_int32_t is_rx_mcastecho;
	u_int32_t is_rx_unauth;
	u_int32_t is_rx_notassoc;
	u_int32_t is_rx_ctl;
	u_int32_t is_rx_decap;
};

/**
 * struct wlan_vdev_mac_stats - VDEV MAC stats
 * @ims_tx_discard: en-MIC failed
 * @ims_tx_discard: tx dropped by NIC
 * @ims_tx_packets:	frames successfully transmitted
 * @ims_tx_bytes: bytes successfully transmitted
 * @ims_tx_data_packets: data frames successfully transmitted
 * @ims_tx_datapyld_bytes: data payload bytes successfully transmitted
 * @ims_tx_data_bytes: data bytes successfully transmitted, inclusive of FCS
 * @ims_rx_decryptcrc: rx decrypt failed on crc
 * @ims_rx_datapyld_bytes: data payload successfully
 * @ims_rx_data_packets: data frames successfully received
 * @ims_rx_discard:	rx dropped by NIC
 * @ims_rx_decryptok: rx decrypt okay
 * @ims_rx_unencrypted: rx w/o wep and privacy on
 * @ims_rx_packets:	frames successfully received
 * @ims_rx_bytes: bytes successfully received
 * @ims_rx_data_bytes: data bytes successfully received, inclusive of FCS
 */
struct wlan_vdev_mac_stats {
	u_int64_t ims_tx_discard;
	u_int64_t ims_tx_packets;
	u_int64_t ims_tx_bytes;
	u_int64_t ims_tx_data_packets;
	u_int64_t ims_tx_datapyld_bytes;
	u_int64_t ims_tx_data_bytes;
	u_int64_t ims_rx_decryptcrc;
	u_int64_t ims_rx_datapyld_bytes;
	u_int64_t ims_rx_data_packets;
	u_int64_t ims_rx_discard;
	u_int64_t ims_rx_decryptok;
	u_int64_t ims_rx_unencrypted;
	u_int64_t ims_rx_packets;
	u_int64_t ims_rx_bytes;
	u_int64_t ims_rx_data_bytes;
};

/**
 * struct wlan_vdev_pause_info - VDEV pause info
 * @vdev_pause_lock: lock to protect access to vdev object data
 * @vdev_tx_data_frame_len: total data in bytes transmitted on the vdev
 * @vdev_rx_data_frame_len: total data in bytes recevdeved on the vdev
 * @vdev_tx_data_frame_count: total number of data frames transmitted on the vdev
 * @vdev_rx_data_frame_count: total number of data frames recevdeved on the vdev
 */
struct wlan_vdev_pause_info {
	spinlock_t vdev_pause_lock;
	u_int32_t vdev_tx_data_frame_len;
	u_int32_t vdev_rx_data_frame_len;
	u_int32_t vdev_tx_data_frame_count;
	u_int32_t vdev_rx_data_frame_count;
};

 /**
  * struct vdev_key - VDEV key information used by datapath
  * @key_type: Key type
  * @key_valid: 0 is key is invalid, 1 if key is valid
  * @key_header: Number of bytes in crypto header
  * @key_trailer: Number of bytes in crypto trailer
  * @key_miclen: Length of the MIC
  * @keyix: HW keycache slot number where key material is stored
  * @clearkeyix: Keycache slot number for MFP clearkey
 */
struct vdev_key {
	uint8_t key_type;
	uint8_t key_valid;
	uint8_t key_header;
	uint8_t key_trailer;
	uint8_t key_miclen;
	uint16_t keyix;
	uint16_t clearkeyix;
};
/**
 * struct dadp_vdev - VDEV datapath object for Direct attach
 * @vdev_unicast_stats: mac statistics for unicast frames
 * @vdev_multicast_stats: mac statistics for multicast frames
 * @vdev_txrxbytes: txrx bytes used by STA PM
 * @vdev_evtable: osif event handlers
 * @vdev_last_ap_frame: STA/IBSS - timestamp of last frame recvd from AP
 * @vdev_def_txkey: default/group tx key index
 * @vdev_sko_th: station kick out threshold
 * @iv_txrx_event_info: txrx event handler private data
 * @av_is_psta: is ProxySTA VAP
 * @av_is_mpsta: is Main ProxySTA VAP
 * @av_is_wrap: is WRAP VAP
 * @av_use_mat: use MAT for this VAP
 * @av_mat_addr: MAT addr
 * @iv_lock: lock to protect access to vap object data
 * @vkey: VDEV key information used by datapath
 * @def_tx_keyix: default tx keyix
 * @psta_keyix: keyix of proxy sta
 */
struct dadp_vdev {
	struct wlan_objmgr_vdev *vdev;
#if ATH_SUPPORT_FLOWMAC_MODULE
	int vdev_dev_stopped;
	int vdev_flowmac;
#endif
	u_int32_t me_hifi_enable;
	u_int32_t mc_snoop_enable;
	u_int32_t mc_mcast_enable;
	u_int32_t mc_discard_mcast;
	u_int8_t av_ampdu_subframes;
	u_int8_t av_amsdu;
	u_int32_t vdev_smps:1;
	u_int32_t vdev_smartnet_enable;
	u_int8_t vdev_tspecActive;
#if ATH_SUPPORT_DSCP_OVERRIDE
	u_int8_t vdev_dscp_map_id;
#endif
	struct wlan_stats vdev_stats;
	struct wlan_vdev_mac_stats vdev_unicast_stats;
	struct wlan_vdev_mac_stats vdev_multicast_stats;
	u_int64_t vdev_txrxbytes;
	wlan_event_handler_table *vdev_evtable;
	os_if_t vdev_ccx_arg;
	wlan_ccx_handler_table *vdev_ccx_evtable;
	struct wlan_vdev_ique_ops ique_ops;
	systime_t vdev_last_ap_frame;
	systime_t vdev_last_traffic_indication;
	u_int16_t vdev_def_txkey;
	u_int16_t vdev_fragthreshold;
	u_int16_t vdev_rtsthreshold;
	int16_t vdev_userrate;
	int8_t vdev_userretries;
	int8_t vdev_txpower;
	int8_t vdev_txchainmask;
	u_int8_t vdev_sko_th;
	struct wlan_vdev_pause_info vdev_pause_info;
	ieee80211_txrx_event_info iv_txrx_event_info;
#if ATH_SUPPORT_WRAP
	u_int32_t av_is_psta:1,
			  av_is_mpsta:1,
			  av_is_wrap:1,
			  av_use_mat:1;
	u_int8_t av_mat_addr[IEEE80211_ADDR_LEN];
#endif
	int vdev_mcast_rate;
	u_int32_t vdev_protmode;
	privacy_exemption privacy_filters[MAX_PRIVACY_FILTERS];
	u_int32_t filters_num;
	struct ieee80211_qos_map iv_qos_map;
	spinlock_t iv_lock;
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
	struct vdev_key vkey[WLAN_CRYPTO_MAXKEYIDX];
	uint16_t def_tx_keyix;
	uint16_t psta_keyix;
#endif
};

/**
 * struct wlan_peer_stats - PEER statistics
 * @ps_tx_discard: tx dropped by NIC
 * @ps_tx_data_success: tx data frames successfully transmitted (unicast only)
 * @ps_tx_bytes_success: tx success data count - unicast only (bytes)
 * @ps_tx_ucast: tx unicast frames
 * @ps_tx_mcast: tx multi/broadcast frames
 * @ps_tx_ucast_bytes: tx bytes of unicast frames
 * @ps_tx_mcast_bytes: tx bytes of multi/broadcast frames
 * @ps_tx_uapsd: tx on uapsd queue
 * @ps_tx_data: tx data frames
 * @ps_tx_bytes: tx data count (bytes)
 * @ps_tx_eosplost: uapsd EOSP retried out
 * @ps_rx_decryptcrc: rx decrypt failed on crc
 * @ps_rx_bytes: rx data count (bytes)
 * @ps_rx_ucast: rx unicast frames
 * @ps_rx_mcast: rx multi/broadcast frames
 * @ps_rx_mcast_bytes: rx bytes of multi/broadcast frames
 * @ps_rx_ucast_bytes: rx bytes of unicast frames
 * @ps_rx_data: rx data frames
 * @ps_rx_unencrypted: rx unecrypted w/ privacy
 * @ps_uapsd_triggerenabled: uapsd triggers recvd
 * @ps_uapsd_duptriggers: uapsd duplicate triggers
 * @ps_uapsd_active: uapsd active
 * @ps_uapsd_ignoretriggers: uapsd duplicate triggers
 * @ps_uapsd_triggers: uapsd triggers
 *
 */
struct wlan_peer_stats {
	u_int32_t ps_tx_discard;
	u_int32_t ps_tx_data_success;
	u_int64_t ps_tx_bytes_success;
	u_int32_t ps_tx_ucast;
	u_int32_t ps_tx_mcast;
#if UMAC_SUPPORT_STA_STATS_ENHANCEMENT
	u_int64_t ps_tx_ucast_bytes;
	u_int64_t ps_tx_mcast_bytes;
#endif
	u_int32_t ps_tx_uapsd;
	u_int32_t ps_tx_data;
	u_int64_t ps_tx_bytes;
	u_int32_t ps_tx_eosplost;
	u_int32_t ps_rx_decryptcrc;
	u_int64_t ps_rx_bytes;
	u_int32_t ps_rx_ucast;
	u_int32_t ps_rx_mcast;
	u_int64_t ps_rx_mcast_bytes;
	u_int64_t ps_rx_ucast_bytes;
	uint32_t ps_rx_data;
	u_int32_t ps_rx_unencrypted;
	u_int32_t ps_uapsd_triggerenabled;
	u_int32_t ps_uapsd_duptriggers;
	u_int32_t ps_uapsd_active;
	u_int32_t ps_uapsd_ignoretriggers;
	u_int32_t ps_uapsd_triggers;
};

/**
 * struct dadp_peer - PEER datapath object for Direct Attach
 * @peer_uapsd_ac_trigena: U-APSD per-node flags matching WMM STA Qos Info field
 * @peer_uapsd_ac_delivena: U-APSD per-node flags matching WMM STA Qos Info field
 * @peer_uapsd_dyn_trigena: U-APSD per-node flags matching WMM STA Qos Info field
 * @peer_uapsd_dyn_delivena : U-APSD per-node flags matching WMM STA Qos Info field
 * @an: lmac node
 * @key_type: Key type
 * @key_valid: 0 is key is invalid, 1 if key is valid
 * @key_header: Number of bytes in crypto header
 * @key_trailer: Number of bytes in crypto trailer
 * @key_miclen: Length of the MIC
 * @keyix: HW keycache slot number where key material is stored
 * @clearkeyix: Keycache slot number for MFP clearkey
 * @def_tx_keyix: default tx keyix
 * @key: Key
 * @peer_rxfragstamp: timestamp of last rx frag
 * @peer_rxfrag_lock: indicate timer handler is currently executing
 * @peer_rxfrag: rx frag reassembly. XXX ???: do we have to have a reassembly line for each TID
 * @an_amsdu: AMSDU frame buf
 * @ni_uapsd_trigseq: trigger suppression on retry
 * @peer_txseqs: tx seq per-tid
 */
struct dadp_peer {
	struct wlan_objmgr_peer *peer;
	u_int8_t peer_uapsd_ac_trigena[WME_NUM_AC];
	u_int8_t peer_uapsd_ac_delivena[WME_NUM_AC];
	int8_t peer_uapsd_dyn_trigena[WME_NUM_AC];
	int8_t peer_uapsd_dyn_delivena[WME_NUM_AC];
	struct wlan_peer_stats peer_stats;
	void *an;
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
	uint8_t key_type;
	uint8_t key_valid;
	uint8_t key_header;
	uint8_t key_trailer;
	uint8_t key_miclen;
	uint16_t keyix;
	uint16_t clearkeyix;
	uint16_t def_tx_keyix;
#else
	struct ieee80211_key *key;
#endif
	u_int32_t nawds;
	u_int16_t peer_consecutive_xretries;
	systime_t peer_rxfragstamp;
	atomic_t peer_rxfrag_lock;
	wbuf_t peer_rxfrag[1];
	u_int8_t peer_minbasicrate;
	struct ath_amsdu *an_amsdu;
	u_int16_t ni_uapsd_trigseq[WME_NUM_AC];
	u_int16_t peer_txseqs[IEEE80211_TID_SIZE];
	u_int16_t peer_rxseqs[IEEE80211_TID_SIZE];
	u_int16_t peer_last_rxseqs[IEEE80211_TID_SIZE];
	u_int8_t peer_uapsd_maxsp;

};

struct dadp_pdev *wlan_get_dp_pdev(struct wlan_objmgr_pdev *pdev);
struct dadp_vdev *wlan_get_dp_vdev(struct wlan_objmgr_vdev *vdev);
struct dadp_peer *wlan_get_dp_peer(struct wlan_objmgr_peer *peer);
int wlan_vap_send(struct wlan_objmgr_vdev *vdev, wbuf_t wbuf);

#define WME_UAPSD_PEER_TRIGSEQINIT(_dp_peer)    (memset(&(_dp_peer)->ni_uapsd_trigseq[0], 0xff, sizeof((_dp_peer)->ni_uapsd_trigseq)))

static INLINE int wlan_hdrspace(struct wlan_objmgr_pdev *pdev, const void *data)
{
	int size = ieee80211_hdrsize(data);

	if (wlan_pdev_nif_feat_cap_get(pdev, WLAN_PDEV_F_DATAPAD))
		size = roundup(size, sizeof(u_int32_t));

	return size;
}

#define    IEEE80211_PEER_STAT(dp_peer,stat)    (dp_peer->peer_stats.ps_##stat++)
#define    IEEE80211_PEER_STAT_ADD(dp_peer,stat,v)    (dp_peer->peer_stats.ps_##stat += v)
#define    IEEE80211_PEER_STAT_SUB(dp_peer,stat,v)    (dp_peer->peer_stats.ps_##stat -= v)
#define    IEEE80211_PEER_STAT_SET(dp_peer,stat,v)    (dp_peer->peer_stats.ps_##stat = v)

#define IEEE80211_PEER_USEAMPDU(_peer)        wlan_peer_use_ampdu(_peer)

#define UAPSD_AC_ISDELIVERYENABLED(_ac, _dp_peer)  \
             (((((_dp_peer)->peer_uapsd_dyn_delivena[_ac] == -1) && (_dp_peer)->peer_uapsd_dyn_trigena[_ac] ==-1)? ((_dp_peer)->peer_uapsd_ac_delivena[_ac]) \
                            :((_dp_peer)->peer_uapsd_dyn_delivena[_ac])) &&  wlan_peer_mlme_flag_get(_dp_peer->peer, WLAN_PEER_F_UAPSD_TRIG))

#define IEEE80211_PEER_UAPSD_USETIM(_dp_peer) \
        (UAPSD_AC_ISDELIVERYENABLED(WME_AC_BK, _dp_peer) && \
        UAPSD_AC_ISDELIVERYENABLED(WME_AC_BE, _dp_peer) && \
        UAPSD_AC_ISDELIVERYENABLED(WME_AC_VI, _dp_peer) && \
        UAPSD_AC_ISDELIVERYENABLED(WME_AC_VO, _dp_peer))

#define IEEE80211_PEER_AC_UAPSD_ENABLED(_dp_peer, _ac) ((_dp_peer)->peer_uapsd_ac_delivena[(_ac)])

#if UMAC_SUPPORT_AP_POWERSAVE
#define VDEV_SET_TIM(_peer, _set, _isr_ctxt) wlan_vdev_set_tim(_peer, _set, _isr_ctxt)
#else
#define VDEV_SET_TIM(_peer, _set, _isr_ctxt)	/* NOT DEFINED */
#endif
u_int8_t *wlan_peer_get_bssid(struct wlan_objmgr_peer * peer);
void wlan_vdev_set_lastdata_timestamp(struct wlan_objmgr_vdev *vdev);
struct ieee80211_ath_channel *wlan_vdev_get_bsschan(struct wlan_objmgr_vdev
						    *vdev);
os_if_t wlan_vdev_get_osifp(struct wlan_objmgr_vdev *vdev);
int wlan_vdev_wnm_is_set(struct wlan_objmgr_vdev *vdev);
void wlan_peer_reload_inact(struct wlan_objmgr_peer *peer);
extern int dp_extap_rx_process(struct wlan_objmgr_vdev *, struct sk_buff *, int *nwifi);
extern int dp_extap_tx_process(struct wlan_objmgr_vdev *, struct sk_buff **, uint8_t);
#define ADP_EXT_AP_RX_PROCESS(_vdev, _skb, _nwifi) \
	dp_extap_rx_process(_vdev, _skb, _nwifi)
#define ADP_EXT_AP_TX_PROCESS(_vdev, _skb, _mhdr_len) \
	dp_extap_tx_process(_vdev, _skb, _mhdr_len)

void ieee80211_vap_txrx_deliver_event(struct wlan_objmgr_vdev *vdev,
				      ieee80211_vap_txrx_event * evt);
void wlan_add_wds_addr(struct wlan_objmgr_vdev *vdev,
		       struct wlan_objmgr_peer *peer, const u_int8_t * macaddr,
		       u_int32_t flags);
void wlan_vdev_set_last_directed_frame(struct wlan_objmgr_vdev *vdev);

void wlan_remove_wds_addr(struct wlan_objmgr_vdev *vdev,
			  const u_int8_t * macaddr, u_int32_t flags);
void wlan_send_addba(struct wlan_objmgr_peer *peer, wbuf_t wbuf);
void
wlan_send_delba(struct wlan_objmgr_peer *peer,
		u_int8_t tidno, u_int8_t initiator, u_int16_t reasoncode);
void wlan_vdev_set_tim(struct wlan_objmgr_peer *peer, int set,
		       bool isr_context);
#if UMAC_SUPPORT_OPMODE_APONLY
#define ath_net80211_rx_monitor(_pdev, _wbuf, _rxs)
#else
void ath_net80211_rx_monitor(struct wlan_objmgr_pdev *pdev, wbuf_t wbuf,
			     ieee80211_rx_status_t * rx_status);
#endif
void wlan_peer_delete(struct wlan_objmgr_peer *peer);
struct wlan_objmgr_peer *wlan_find_wds_peer(struct wlan_objmgr_vdev *vdev,
					    const u_int8_t * macaddr);
void wlan_admctl_classify(struct wlan_objmgr_vdev *vdev,
			  struct wlan_objmgr_peer *peer, int *tid, int *ac);
u_int16_t wlan_vdev_get_sta_assoc(struct wlan_objmgr_vdev *vdev);
int wlan_peer_pause(struct wlan_objmgr_peer *peer);
void wlan_peer_saveq_queue(struct wlan_objmgr_peer *peer, wbuf_t wbuf,
			   u_int8_t frame_type);
int wlan_peer_unpause(struct wlan_objmgr_peer *peer);
int wlan_vdev_wnm_tfs_filter(struct wlan_objmgr_vdev *vdev, wbuf_t wbuf);
#if UMAC_SUPPORT_PROXY_ARP
int wlan_do_proxy_arp(struct wlan_objmgr_vdev *vdev, qdf_nbuf_t netbuf);
#endif

static INLINE int
wlan_wds_is4addr(struct wlan_objmgr_vdev *vdev, struct ether_header eh,
		 struct wlan_objmgr_peer *peer)
{
	/*
	 * Removing the below check, to force all frames to WDS STA as 4-addr
	 *     !(IEEE80211_ADDR_EQ(eh.ether_dhost, macaddr)))
	 * This is done to WAR a HW issue, where encryption fails if 4-addr and 3-addr
	 * frames are mixed in an AMPDU
	 */
	if ((wlan_vdev_mlme_feat_cap_get(vdev, WLAN_VDEV_F_WDS)) &&
	    (wlan_peer_mlme_flag_get(peer, WLAN_PEER_F_WDS))) {
		return 1;
	} else {
		return 0;
	}
}

#ifndef WLAN_CONV_CRYPTO_SUPPORTED
struct ieee80211_rsnparms *wlan_vdev_get_rsn(struct wlan_objmgr_vdev *vdev);
u_int16_t wlan_vdev_get_def_txkey(struct wlan_objmgr_vdev *vdev);
struct ieee80211_key *wlan_crypto_get_txkey(struct wlan_objmgr_vdev *vdev,
					    wbuf_t wbuf);
struct ieee80211_key *wlan_crypto_peer_encap(struct wlan_objmgr_peer *peer,
					     wbuf_t wbuf);
struct ieee80211_key *wlan_vdev_get_nw_keys(struct wlan_objmgr_vdev *vdev);
#else
struct wlan_crypto_key *wlan_crypto_get_txkey(struct wlan_objmgr_vdev *vdev,
					    wbuf_t wbuf);
struct wlan_crypto_key *wlan_crypto_peer_encap(struct wlan_objmgr_peer *peer,
					     wbuf_t wbuf);
struct wlan_crypto_key *wlan_vdev_get_nw_keys(struct wlan_objmgr_vdev *vdev);
#endif

u_int8_t wlan_peer_get_bf_update_cv(struct wlan_objmgr_peer *peer);
int wlan_peer_get_pspoll(struct wlan_objmgr_peer *peer);
static INLINE int wlan_peer_use_ampdu(struct wlan_objmgr_peer *peer)
{
	if (wlan_peer_mlme_flag_get(peer, WLAN_PEER_F_HT) &&
	    !(wlan_peer_mlme_flag_get(peer, WLAN_PEER_F_NOAMPDU))
	    && !(wlan_peer_get_pspoll(peer))) {
		return (1);	/* Supports AMPDU */
	}
	return (0);		/* Do not use AMPDU since non HT */
}

u_int8_t wlan_pdev_get_wmep_noackPolicy(struct wlan_objmgr_pdev * pdev, int ac);
u_int16_t wlan_peer_get_txseqs(struct wlan_objmgr_peer *peer, int tid);
void wlan_request_cv_update(struct wlan_objmgr_pdev *pdev,
			    struct wlan_objmgr_peer *peer, wbuf_t wbuf,
			    int use4addr);
void wlan_peer_incr_txseq(struct wlan_objmgr_peer *peer, int tid);
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
void wlan_ald_record_tx(struct wlan_objmgr_vdev *vdev, wbuf_t wbuf,
			int datalen);
#endif
int wlan_nawds_send_wbuf(struct wlan_objmgr_vdev *vdev, wbuf_t wbuf);
/* Functions below are used by aponly and generic */
void ieee80211_amsdu_encap(struct wlan_objmgr_vdev *vdev,
			   wbuf_t amsdu_wbuf,
			   wbuf_t wbuf, u_int16_t framelen, int append_wlan);
int
ieee80211_amsdu_input(struct wlan_objmgr_peer *peer,
		      wbuf_t wbuf, struct ieee80211_rx_status *rs,
		      int is_mcast, u_int8_t subtype);
int ieee80211_amsdu_check(struct wlan_objmgr_vdev *vdev, wbuf_t m);
int ieee80211_8023frm_amsdu_check(wbuf_t wbuf);
wbuf_t ieee80211_encap(struct wlan_objmgr_peer *peer, wbuf_t wbuf);
wbuf_t __ieee80211_encap(struct wlan_objmgr_peer *peer, wbuf_t wbuf);
#ifdef ENCAP_OFFLOAD
#define ieee80211_encap( _ni , _wbuf)   (_wbuf)
#define ieee80211_encap_force( _ni , _wbuf)   __ieee80211_encap( _ni, _wbuf)
#else
#define ieee80211_encap( _ni , _wbuf)   __ieee80211_encap( _ni, _wbuf)
#endif

/* WDS */
int wlan_peer_check_cap(struct wlan_objmgr_peer *peer, u_int16_t cap);
u_int32_t wlan_pdev_get_tx_staggered_sounding(struct wlan_objmgr_pdev *pdev);
u_int32_t wlan_peer_get_rx_staggered_sounding(struct wlan_objmgr_peer *peer);
u_int32_t wlan_peer_get_channel_estimation_cap(struct wlan_objmgr_peer *peer);
u_int16_t wlan_pdev_get_ldpcap(struct wlan_objmgr_pdev *pdev);
u_int16_t wlan_peer_get_txpower(struct wlan_objmgr_peer *peer);
u_int16_t wlan_vdev_has_pssta(struct wlan_objmgr_vdev *vdev);
u_int16_t wlan_vdev_get_rtsthreshold(struct wlan_objmgr_vdev *vdev);
u_int32_t wlan_peer_get_htcap(struct wlan_objmgr_peer *peer);
u_int16_t wlan_vdev_get_mgt_rate(struct wlan_objmgr_vdev *vdev);
struct wlan_objmgr_peer *wlan_find_rxpeer(struct wlan_objmgr_pdev *pdev,
					  struct ieee80211_frame_min *wh);
u_int16_t wlan_pdev_get_curmode(struct wlan_objmgr_pdev *pdev);
u_int16_t wlan_pdev_get_curchan_freq(struct wlan_objmgr_pdev *pdev);
struct ieee80211_ath_channel *wlan_pdev_get_curchan(struct wlan_objmgr_pdev
						    *pdev);
#if ATH_SUPPORT_WRAP
#define ieee80211_vdev_find_node(_psoc, _vdev, _mac)     \
	wlan_objmgr_get_peer_by_mac_n_vdev(_psoc, wlan_objmgr_pdev_get_pdev_id(wlan_vdev_get_pdev(_vdev)), \
		       wlan_vdev_mlme_get_macaddr(_vdev), _mac, WLAN_MLME_SB_ID)
#else
#define ieee80211_vdev_find_node(_psoc, _vdev, _mac)     \
	wlan_objmgr_get_peer(_psoc, wlan_objmgr_pdev_get_pdev_id(wlan_vdev_get_pdev(_vdev)), \
		       	_mac, WLAN_MLME_SB_ID)
#endif

void wlan_vdev_vi_dbg_input(struct wlan_objmgr_vdev *vdev, wbuf_t wbuf);
#ifndef ATHHTC_AP_REMOVE_STATS
#define IEEE80211_INPUT_UPDATE_DATA_STATS(_peer, _mac_stats, _wbuf, _rs, _realhdrsize)\
    _ieee80211_input_update_data_stats(_peer, _mac_stats, _wbuf, _rs, _realhdrsize)

#define WLAN_VAP_STATS(_dp_vdev, _statsmem) _dp_vdev->vdev_stats._statsmem++
#define WLAN_MAC_STATS(_mac_stats, _statsmem) _mac_stats->_statsmem++
#define WLAN_VDEV_LASTDATATSTAMP(_vdev, _tstamp) \
					wlan_vdev_set_lastdata_timestamp(_vdev);

#define WLAN_VDEV_TXRXBYTE(_dp_vdev,_wlen) \
					dp_vdev->vdev_txrxbytes += _wlen;

#define WLAN_VDEV_TRAFIC_INDICATION(_dp_vdev , _is_bcast, _subtype) \
					if (!_is_bcast && IEEE80211_CONTAIN_DATA(_subtype)) {  \
						_dp_vdev->vdev_last_traffic_indication = OS_GET_TIMESTAMP(); \
					}
#define WLAN_MAC_STATSINCVAL(_mac_stats, _ims_rx_bytes, _wlen)  \
           _mac_stats->_ims_rx_bytes += _wlen;

#else

#define IEEE80211_INPUT_UPDATE_DATA_STATS(_peer, _mac_stats, _wbuf, _rs, _realhdrsize)
#define WLAN_VAP_STATS(_dp_vdev, _statsmem)
#define WLAN_MAC_STATS(_mac_stats, _statsmem)
#define WLAN_VDEV_LASTDATATSTAMP(_vdev, _tstamp)
#define WLAN_VDEV_TXRXBYTE(_dp_vdev,_wlen)
#define WLAN_VDEV_TRAFIC_INDICATION(_dp_vdev, _is_bcast, _subtype)
#define WLAN_MAC_STATSINCVAL(_mac_stats, _ims_rx_bytes, _wlen)

#endif
int wlan_cmn_input_mgmt(struct wlan_objmgr_peer *peer,
			wbuf_t wbuf, struct ieee80211_rx_status *rs);
int
dadp_input_all(struct wlan_objmgr_pdev *pdev,
	       wbuf_t wbuf, struct ieee80211_rx_status *rs);
/*
 * Return the bssid of a frame.
 */
static INLINE const u_int8_t *wlan_wbuf_getbssid(struct wlan_objmgr_vdev *vdev,
						 const struct ieee80211_frame
						 *wh)
{
	if (wlan_vdev_mlme_get_opmode(vdev) == QDF_STA_MODE)
		return wh->i_addr2;
	if ((wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) != IEEE80211_FC1_DIR_NODS)
		return wh->i_addr1;
	if ((wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) ==
	    IEEE80211_FC0_SUBTYPE_PS_POLL)
		return wh->i_addr1;
	return wh->i_addr3;
}

struct wlan_objmgr_peer *wlan_create_tmp_peer(struct wlan_objmgr_vdev *vdev,
					      const u_int8_t * macaddr);
int wlan_vdev_nawds_enable_learning(struct wlan_objmgr_vdev *vdev);
void wlan_vdev_nawds_learn(struct wlan_objmgr_vdev *vdev, u_int8_t * sender);
int wlan_send_deauth(struct wlan_objmgr_peer *peer, u_int16_t reason);
void wlan_vdev_deliver_event_mlme_deauth_indication(struct wlan_objmgr_vdev
						    *vdev, u_int8_t * macaddr,
						    u_int16_t associd,
						    u_int16_t reason);
u_int16_t wlan_peer_get_associd(struct wlan_objmgr_peer *peer);
int wlan_send_disassoc(struct wlan_objmgr_peer *peer, u_int16_t reason);
void wlan_vdev_deliver_event_mlme_disassoc_indication(struct wlan_objmgr_vdev
						      *vdev, u_int8_t * macaddr,
						      u_int16_t associd,
						      u_int16_t reason);
void wlan_wds_update_rootwds_table(struct wlan_objmgr_peer *peer,
				   struct wlan_objmgr_pdev *pdev, wbuf_t wbuf);
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
struct ieee80211_key *wlan_crypto_peer_decap(struct wlan_objmgr_peer *peer,
					     wbuf_t wbuf, int hdrlen,
					     struct ieee80211_rx_status *rs);
int wlan_vdev_crypto_demic(struct wlan_objmgr_vdev *vdev,
			   struct ieee80211_key *key, wbuf_t wbuf, int hdrlen,
			   int force, struct ieee80211_rx_status *rs);
#else
int wlan_vdev_crypto_demic(struct wlan_objmgr_vdev *vdev,
			   struct wlan_crypto_key *key, wbuf_t wbuf, int hdrlen,
			   int force, struct ieee80211_rx_status *rs);
#endif
int wlan_vdev_rx_gate(struct wlan_objmgr_vdev *vdev);
void wlan_vdev_rx_ungate(struct wlan_objmgr_vdev *vdev);
void wlan_peer_set_rssi(struct wlan_objmgr_peer *peer, u_int8_t rssi);
void wlan_peer_mlme_pwrsave(struct wlan_objmgr_peer *peer, int enable);
int
wlan_peer_recv_ctrl(struct wlan_objmgr_peer *peer,
		    wbuf_t wbuf, int subtype, struct ieee80211_rx_status *rs);
static INLINE int wlan_peer_is_ampdu(struct wlan_objmgr_peer *peer)
{
	if (wlan_peer_mlme_flag_get(peer, WLAN_PEER_F_HT) &&
	    !(wlan_peer_mlme_flag_get(peer, WLAN_PEER_F_NOAMPDU))) {
		return (1);	/* Supports AMPDU */
	}
	return (0);		/* Do not use AMPDU since non HT */
}

#define IEEE80211_PEER_ISAMPDU(_peer)         wlan_peer_is_ampdu(_peer)
/* Not supported */
#if UMAC_SUPPORT_VAP_PAUSE
/*
 * update_tx_frame_stats.
 * to be called from the TX path when ever it sends a data frame.
 */
#define ieee80211_vap_pause_update_xmit_stats(dp_vdev, wbuf)                \
    IEEE80211_STAT_LOCK(&dp_vdev->vdev_pause_info.vdev_pause_lock);            \
    dp_vdev->vdev_pause_info.vdev_tx_data_frame_len += wbuf_get_pktlen(wbuf);  \
    dp_vdev->vdev_pause_info.vdev_tx_data_frame_count++;                       \
    IEEE80211_STAT_UNLOCK(&dp_vdev->vdev_pause_info.vdev_pause_lock);          \

#else
#define ieee80211_vap_pause_update_xmit_stats(dp_vdev,wbuf)	/* */
#endif

int ieee80211_me_Convert(struct wlan_objmgr_vdev *vdev, wbuf_t wbuf,
			 u_int8_t newmac[][IEEE80211_ADDR_LEN],
			 uint8_t newmaccnt);
void ieee80211_me_hifi_forward(struct wlan_objmgr_vdev *vdev, wbuf_t wbuf,
			       struct wlan_objmgr_peer *peer);
int wlan_da_input_mgmt(struct wlan_objmgr_pdev *pdev,
		       struct wlan_objmgr_peer *peer, wbuf_t wbuf,
		       struct ieee80211_rx_status *rs);

/*
 * TBD_CRYPTO:
 * Introduce below APIs in crypto module (cmn_dev)
 */
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
uint8_t wlan_crypto_peer_get_header(struct wlan_crypto_key *key);
uint8_t wlan_crypto_peer_get_trailer(struct wlan_crypto_key *key);
uint8_t wlan_crypto_peer_get_miclen(struct wlan_crypto_key *key);
uint8_t wlan_crypto_peer_get_cipher_type(struct wlan_crypto_key *key);
#endif
u_int8_t wlan_vdev_is_radar_channel(struct wlan_objmgr_vdev *vdev);
