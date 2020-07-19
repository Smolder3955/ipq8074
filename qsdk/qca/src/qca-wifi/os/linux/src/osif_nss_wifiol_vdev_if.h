/*
 * Copyright (c) 2015-2016,2017-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2015-2016 Qualcomm Atheros, Inc.
 *
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#ifndef __OSIF_NSS_WIFIOL_VDEV_IF_H
#define __OSIF_NSS_WIFIOL_VDEV_IF_H

#include <nss_api_if.h>
#include <osif_private.h>
#define OSIF_NSS_MAX_RADIOS 3
#include <ol_if_txrx_handles.h>
#include <ol_if_athvar.h>

#define OSIF_NSS_VDEV_PKT_QUEUE_LEN 256
#define OSIF_NSS_VDEV_STALE_PKT_INTERVAL 1000
#define WDS_EXT_ENABLE 1

#define OSIF_TXRX_LIST_APPEND(head, tail, elem) \
    do {                                            \
        if (!(head)) {                              \
            (head) = (elem);                        \
        } else {                                    \
            qdf_nbuf_set_next((tail), (elem));      \
        }                                           \
        (tail) = (elem);                            \
    } while (0)

struct osif_nss_vdev_cfg_pvt {
    struct completion complete;     /* completion structure */
    int response;               /* Response from FW */
};

struct osif_nss_vdev_vow_dbg_stats_pvt {
    struct completion complete;     /* completion structure */
    int response;               /* Response from FW */
    uint32_t rx_vow_dbg_counter;
    uint32_t tx_vow_dbg_counter[8];
};

enum osif_nss_error_types {
    OSIF_NSS_VDEV_XMIT_SUCCESS = 0,
    OSIF_NSS_VDEV_XMIT_FAIL_MSG_TOO_SHORT,
    OSIF_NSS_VDEV_XMIT_FAIL_PROXY_ARP,
    OSIF_NSS_VDEV_XMIT_FAIL_NO_HEADROOM,
    OSIF_NSS_VDEV_XMIT_FAIL_NSS_QUEUE_FULL,
    OSIF_NSS_VDEV_XMIT_IF_NULL,
    OSIF_NSS_VDEV_XMIT_IF_INVALID,
    OSIF_NSS_VDEV_XMIT_CTX_NULL,
    OSIF_NSS_FAILURE_MAX
};


enum osif_nss_vdev_cmd {
    OSIF_NSS_VDEV_DROP_UNENC = 0,
    OSIF_NSS_VDEV_ENCAP_TYPE,
    OSIF_NSS_VDEV_DECAP_TYPE,
    OSIF_NSS_VDEV_ENABLE_ME,
    OSIF_NSS_WIFI_VDEV_NAWDS_MODE,
    OSIF_NSS_VDEV_EXTAP_CONFIG,
    OSIF_NSS_WIFI_VDEV_CFG_BSTEER,
    OSIF_NSS_WIFI_VDEV_VOW_DBG_MODE,
    OSIF_NSS_WIFI_VDEV_VOW_DBG_RST_STATS,
    OSIF_NSS_WIFI_VDEV_CFG_DSCP_OVERRIDE,
    OSIF_NSS_WIFI_VDEV_CFG_WNM_CAP,
    OSIF_NSS_WIFI_VDEV_CFG_WNM_TFS,
    OSIF_NSS_WIFI_VDEV_WDS_EXT_ENABLE,
    OSIF_NSS_VDEV_WDS_CFG,
    OSIF_NSS_VDEV_AP_BRIDGE_CFG,
    OSIF_NSS_VDEV_SECURITY_TYPE_CFG,
    OSIF_NSS_VDEV_AST_OVERRIDE_CFG,
    OSIF_NSS_WIFI_VDEV_CFG_SON_CAP,
    OSIF_NSS_WIFI_VDEV_MAX
};

#define OSIF_NSS_VDEV_CFG_TIMEOUT_MS 1000
#define OSIF_NSS_VDEV_RECOV_TIMEOUT_MS 1000
#define NSS_PROXY_VAP_IF_NUMBER 999

int32_t osif_nss_ol_vap_delete(osif_dev *osifp, uint8_t is_recovery);
int32_t osif_nss_ol_vap_up(osif_dev *osifp);
int32_t osif_nss_ol_vap_down(osif_dev *osifp);
enum osif_nss_error_types osif_nss_ol_vap_xmit(osif_dev *osifp, struct sk_buff *skb);
int32_t osif_nss_ol_vap_hardstart(struct sk_buff *skb, struct net_device *dev);
struct net_device *osif_nss_vdev_process_mpsta_rx(struct net_device *netdev, struct sk_buff *skb, uint32_t peer_id);
void osif_nss_vdev_process_mpsta_tx(struct net_device *netdev, struct sk_buff *skb);
void osif_nss_vdev_process_mpsta_rx_to_tx(struct net_device *netdev, struct sk_buff *skb, __attribute__((unused)) struct napi_struct *napi);
void osif_nss_vdev_psta_delete_entry(osif_dev *osif);
bool osif_nss_vdev_psta_add_entry(osif_dev *osif);
bool osif_nss_vdev_qwrap_isolation_enable(osif_dev *osif);
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
void osif_nss_vdev_me_update_hifitlb(osif_dev *osifp, struct ieee80211_me_hifi_table *table);
#endif
void osif_nss_vdev_me_create_grp_list(osif_dev *osifp, uint8_t *grp_addr, uint8_t *grp_ipaddr, uint32_t ether_type, uint32_t length);
void osif_nss_vdev_me_delete_grp_list(osif_dev *osifp, uint8_t *grp_addr, uint8_t *grp_ipaddr, uint32_t ether_type);
void osif_nss_vdev_me_add_member_list(osif_dev *osifp, struct MC_LIST_UPDATE* list_entry, uint32_t ether_type, uint32_t length);
void osif_nss_vdev_me_remove_member_list(osif_dev *osifp, uint8_t *grp_ipaddr, uint8_t *grp_addr, uint8_t *grp_member_addr, uint32_t ether_type);
void osif_nss_vdev_me_update_member_list(osif_dev *osifp, struct MC_LIST_UPDATE* list_entry, uint32_t ether_type);
void osif_nss_vdev_me_add_deny_member(osif_dev *osifp, uint32_t grp_ipaddr);
void osif_nss_vdev_me_delete_deny_list(osif_dev *osifp);
void osif_nss_vdev_me_dump_snooplist(osif_dev *osifp);
void osif_nss_vdev_me_dump_denylist(osif_dev *osifp);
void *osif_nss_vdev_get_nss_wifiol_ctx(osif_dev *osifp);
int osif_nss_vdev_get_nss_id(osif_dev *osifp);
int osif_nss_vdev_tx_raw(void *osif_vdev, qdf_nbuf_t *pnbuf);
int32_t osif_nss_ol_vap_create(void * vdev, struct ieee80211vap *vap, osif_dev *osif, uint32_t nss_ifnum);
void osif_nss_vdev_update_vlan(struct ieee80211vap *vap, osif_dev *osif);
void osif_nss_vdev_set_vlan_type(osif_dev *osifp, uint8_t default_vlan, uint8_t port_vlan);
int32_t osif_nss_vdev_alloc(struct ol_ath_softc_net80211 *scn, struct ieee80211vap *vap, osif_dev *osifp);
void osif_nss_vdev_dealloc(osif_dev *osif, int32_t if_num);
void osif_nss_vdev_set_cfg(void *osifp, enum osif_nss_vdev_cmd osif_cmd);
void osif_nss_vdev_me_reset_snooplist(osif_dev *osifp);
int osif_nss_ol_extap_rx(struct net_device *dev, struct sk_buff *skb);
int osif_nss_ol_extap_tx(struct net_device *dev, struct sk_buff *skb);
void osif_nss_vdev_get_vow_dbg_stats(osif_dev *osifp);
void osif_nss_vdev_vow_dbg_cfg(osif_dev *osifp, int32_t val);
void osif_nss_vdev_set_dscp_tid_map(osif_dev *osifp, uint32_t* dscp_map);
void osif_nss_vdev_set_dscp_tid_map_id(osif_dev *osifp, uint8_t map_id);
void osif_nss_vdev_process_extap_tx(struct net_device *netdev, struct sk_buff *skb);
void osif_nss_vdev_process_extap_rx_to_tx(struct net_device *netdev, struct sk_buff *skb);
void osif_nss_vdev_stale_pkts_timer(void *arg);
void osif_nss_vdev_process_wnm_tfs_tx(struct net_device *netdev, struct sk_buff *skb);
void *osif_nss_vdev_set_wifiol_ctx(struct ieee80211com *ic);
void osif_nss_vdev_deliver_rawbuf(struct net_device *netdev, qdf_nbuf_t msdu_list,  struct napi_struct *napi);
void osif_nss_vdev_peer_tx_buf(void *osif_vdev, struct sk_buff *skb, uint16_t peer_id);
bool osif_nss_vdev_skb_needs_linearize(struct net_device *dev, struct sk_buff *skb);
int32_t osif_nss_vdev_enqueue_packet_for_tx(osif_dev *osifp, struct sk_buff *skb);
void osif_nss_vdev_frag_to_chain(void *osif_vdev, qdf_nbuf_t nbuf, qdf_nbuf_t *list_head, qdf_nbuf_t *list_tail);
void osif_nss_vdev_cfg_callback(void *app_data, struct nss_cmn_msg *msg);
void osif_nss_vdev_send_cmd(struct nss_ctx_instance *nss_wifiol_ctx, int32_t if_num, enum nss_wifi_vdev_cmd cmd, uint32_t value);
void osif_nss_vdev_me_toggle_snooplist(osif_dev *osifp);
void osif_nss_vdev_special_data_receive(struct net_device *dev, struct sk_buff *skb, __attribute__((unused)) struct napi_struct *napi);

int32_t osif_nss_be_vap_updchdhdr(osif_dev *osifp);
int32_t osif_nss_wifili_vap_updchdhdr(osif_dev *osifp);
uint8_t osif_nss_wifili_vdev_get_mpsta_vdevid(ol_ath_soc_softc_t *soc, uint16_t peer_id, uint8_t vdev_id, uint8_t *mpsta_vdev_id);
void osif_nss_vdev_set_inspection_mode(osif_dev *osifp, uint32_t  value);
void osif_nss_vdev_extap_table_entry_add(osif_dev *osifp, uint16_t ip_version, uint8_t *ip, uint8_t *dest_mac);
void osif_nss_vdev_extap_table_entry_del(osif_dev *osifp, uint16_t ip_version, uint8_t *ip);
void osif_nss_vdev_set_read_rxprehdr(osif_dev *osifp, uint32_t  value);

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
struct nss_vdev_ops {
#if DBDC_REPEATER_SUPPORT
    void (* ic_osif_nss_store_other_pdev_stavap)(struct
                                ieee80211com* ic);
#endif
    void (* ic_osif_nss_vdev_me_reset_snooplist)(osif_dev *osifp);
    void (* ic_osif_nss_vdev_me_update_member_list)(osif_dev *osifp,
                                struct MC_LIST_UPDATE* list_entry, uint32_t ether_type);
    enum osif_nss_error_types (* ic_osif_nss_vap_xmit)(osif_dev *osifp, struct sk_buff *skb);
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
    void (* ic_osif_nss_vdev_me_update_hifitlb)(osif_dev *osifp,
                                struct ieee80211_me_hifi_table *table);
#endif
    void (* ic_osif_nss_vdev_me_dump_denylist)(osif_dev *osifp);
    void (* ic_osif_nss_vdev_me_add_deny_member)(osif_dev *osifp,
                                uint32_t grp_ipaddr);
    void (* ic_osif_nss_vdev_set_cfg)(void *osifp,
                                enum osif_nss_vdev_cmd osif_cmd);
    void (* ic_osif_nss_vdev_process_mpsta_tx)(struct net_device *netdev,
                                struct sk_buff *skb);
    int (* ic_osif_nss_wifi_monitor_set_filter)(struct ieee80211com *ic,
                                uint32_t filter_type);
    int (* ic_osif_nss_vdev_get_nss_id)(osif_dev *osifp);
    void (* ic_osif_nss_vdev_process_extap_tx)(struct net_device *netdev,
                                struct sk_buff *skb);
    void (* ic_osif_nss_vdev_me_dump_snooplist)(osif_dev *osifp);
    int32_t (* ic_osif_nss_vap_delete)(osif_dev *osifp, uint8_t is_recovery);
    void (* ic_osif_nss_vdev_me_add_member_list)(osif_dev *osifp,
                                struct MC_LIST_UPDATE* list_entry,
                                uint32_t ether_type, uint32_t length);
    void (* ic_osif_nss_vdev_vow_dbg_cfg)(osif_dev *osifp, int32_t val);
    void (* ic_osif_nss_enable_dbdc_process)(struct ieee80211com* ic,
                                uint32_t enable);
    void * (* ic_osif_nss_vdev_set_nss_wifiol_ctx)(struct ieee80211com *ic);
    void (* ic_osif_nss_vdev_me_delete_grp_list)(osif_dev *osifp, uint8_t *grp_addr,
                                uint8_t *grp_ipaddr, uint32_t ether_type);
    void (* ic_osif_nss_vdev_me_create_grp_list)(osif_dev *osifp, uint8_t *grp_addr,
                                uint8_t *grp_ipaddr, uint32_t ether_type, uint32_t length);
    void (* ic_osif_nss_vdev_me_delete_deny_list)(osif_dev *osifp);
    void (* ic_osif_nss_vdev_me_remove_member_list)(osif_dev *osifp, uint8_t *grp_ipaddr,
                                uint8_t *grp_addr, uint8_t *grp_member_addr, uint32_t ether_type);
    int32_t (* ic_osif_nss_vdev_alloc)(struct ol_ath_softc_net80211 *scn,
                                struct ieee80211vap *vap, osif_dev *osifp);
    int32_t (* ic_osif_nss_vap_create)(void * vdev, struct ieee80211vap *vap,
                                osif_dev *osif, uint32_t nss_ifnum);
    int32_t (* ic_osif_nss_vap_updchdhdr)(osif_dev *osifp);
    void (* ic_osif_nss_vdev_set_dscp_tid_map)(osif_dev *osifp, uint32_t* dscp_map);
    void (* ic_osif_nss_vdev_set_dscp_tid_map_id)(osif_dev *osifp, uint8_t map_id);
    int32_t (* ic_osif_nss_vap_up)(osif_dev *osifp);
    int32_t (* ic_osif_nss_vap_down)(osif_dev *osifp);
    void (* ic_osif_nss_vdev_set_inspection_mode)(osif_dev *osifp, uint32_t value);
    void (* ic_osif_nss_vdev_extap_table_entry_add)(osif_dev *osifp, uint16_t ip_version, uint8_t *ip, uint8_t *dest_mac);
    void (* ic_osif_nss_vdev_extap_table_entry_del)(osif_dev *osifp, uint16_t ip_version, uint8_t *ip);
    bool (* ic_osif_nss_vdev_psta_add_entry)(osif_dev *osif);
    void (* ic_osif_nss_vdev_psta_delete_entry)(osif_dev *osif);
    bool (* ic_osif_nss_vdev_qwrap_isolation_enable)(osif_dev *osif);
    void (* ic_osif_nss_vdev_set_read_rxprehdr)(osif_dev *osifp, uint32_t value);
    int (*ic_osif_nss_vdev_set_peer_next_hop)(osif_dev *osifp, uint8_t *addr, uint16_t if_num);
    void (*ic_osif_nss_vap_update_vlan)(struct ieee80211vap *vap, osif_dev *osif);
    void (*ic_osif_nss_vdev_set_vlan_type)(osif_dev *osifp, uint8_t default_vlan, uint8_t port_vlan);
};

#endif /* QCA_NSS_WIFI_OFFLOAD_SUPPORT */

uint32_t osif_nss_ol_be_vdev_set_cfg(osif_dev *osifp,
                                enum osif_nss_vdev_cmd osif_cmd);
uint32_t osif_nss_wifili_vdev_set_cfg(osif_dev *osifp,
                                enum osif_nss_vdev_cmd osif_cmd);
void osif_nss_ol_be_get_peerid( struct MC_LIST_UPDATE* list_entry,
                                uint32_t *peer_id);
void osif_nss_wifili_get_peerid( struct MC_LIST_UPDATE* list_entry,
                                uint32_t *peer_id);
uint32_t osif_nss_ol_be_peerid_find_hash_find(struct ieee80211vap *vap,
                                uint8_t *peer_mac_addr, int mac_addr_is_aligned);
void * osif_nss_ol_be_peer_find_hash_find(struct ieee80211vap *vap,
                                uint8_t *peer_mac_addr, int mac_addr_is_aligned);
uint32_t osif_nss_wifili_peerid_find_hash_find(struct ieee80211vap *vap,
                                uint8_t *peer_mac_addr, int mac_addr_is_aligned);
void * osif_nss_wifili_peer_find_hash_find(struct ieee80211vap *vap,
                                uint8_t *peer_mac_addr, int mac_addr_is_aligned);
uint8_t osif_nss_ol_be_get_vdevid_fromosif(osif_dev *osifp, uint8_t *vdev_id);
uint8_t osif_nss_wifili_get_vdevid_fromosif(osif_dev *osifp, uint8_t *vdev_id);
uint8_t osif_nss_ol_be_get_vdevid_fromvdev(void *vdev, uint8_t *vdev_id);
uint8_t osif_nss_wifili_get_vdevid_fromvdev(void *vdev, uint8_t *vdev_id);
uint8_t osif_nss_ol_be_find_pstosif_by_id(struct net_device *netdev, uint32_t peer_id,
                                osif_dev **psta_osifp);
uint8_t osif_nss_wifili_find_pstosif_by_id(struct net_device *netdev, uint32_t peer_id,
                                osif_dev **psta_osifp);
void osif_nss_ol_be_vdevcfg_set_offload_params(void * vdev,
                                struct nss_wifi_vdev_config_msg **p_wifivdevcfg);
void osif_nss_wifili_vdevcfg_set_offload_params(void * vdev,
                                struct nss_wifi_vdev_config_msg **p_wifivdevcfg);
bool osif_nss_ol_be_vdev_handle_monitor_mode(struct net_device *netdev,
                                struct sk_buff *skb, uint8_t is_chain);
bool osif_nss_wifili_vdev_handle_monitor_mode(struct net_device *netdev,
                                struct sk_buff *skb, uint8_t is_chain);
void osif_nss_ol_be_vdev_tx_inspect_handler(void *vdev_handle, struct sk_buff *skb);
void osif_nss_wifili_vdev_tx_inspect_handler(void *vdev_handle, struct sk_buff *skb);
void osif_nss_wifili_vdev_txinfo_handler(struct ol_ath_softc_net80211 *scn, struct sk_buff *skb,
                             struct nss_wifi_vdev_per_packet_metadata *wifi_metadata, bool is_raw);
void osif_nss_ol_be_vdev_txinfo_handler(struct ol_ath_softc_net80211 *scn, struct sk_buff *skb,
                             struct nss_wifi_vdev_per_packet_metadata *wifi_metadata, bool is_raw);
void osif_nss_ol_be_vdev_update_statsv2(struct ol_ath_softc_net80211 *scn, struct sk_buff * nbuf, void *rx_mpdu_desc, uint8_t htt_rx_status);
void osif_nss_wifili_vdev_update_statsv2(struct ol_ath_softc_net80211 *scn, struct sk_buff * nbuf, void *rx_mpdu_desc, uint8_t htt_rx_status);
void osif_nss_ol_be_vdev_data_receive_meshmode_rxinfo(struct net_device *dev, struct sk_buff *skb);
void osif_nss_wifili_vdev_data_receive_meshmode_rxinfo(struct net_device *dev, struct sk_buff *skb);
void * osif_nss_ol_be_find_peer_by_id(osif_dev *osifp, uint32_t peer_id);
void * osif_nss_wifili_find_peer_by_id(osif_dev *osifp, uint32_t peer_id);
void osif_nss_ol_vdev_handle_rawbuf(struct net_device *netdev, qdf_nbuf_t nbuf,
                                __attribute__((unused)) struct napi_struct *napi);
uint8_t osif_nss_ol_be_vdev_call_monitor_mode(struct net_device *netdev, osif_dev  *osdev,
                                qdf_nbuf_t skb_list_head, uint8_t is_chain);
uint8_t osif_nss_wifili_vdev_call_monitor_mode(struct net_device *netdev, osif_dev  *osdev,
                                qdf_nbuf_t skb_list_head, uint8_t is_chain);
uint8_t osif_nss_ol_be_vdev_get_stats(osif_dev *osifp, struct nss_cmn_msg *wifivdevmsg);
uint8_t osif_nss_wifili_vdev_get_stats(osif_dev *osifp, struct nss_cmn_msg *wifivdevmsg);
void osif_nss_ol_be_vdev_spl_receive_exttx_compl(struct net_device *dev, struct sk_buff *skb,
                                struct nss_wifi_vdev_tx_compl_metadata *tx_compl_metadata);
void osif_nss_wifili_vdev_spl_receive_exttx_compl(struct net_device *dev, struct sk_buff *skb,
                                struct nss_wifi_vdev_tx_compl_metadata *tx_compl_metadata);
void osif_nss_ol_be_vdev_spl_receive_ext_mesh(struct net_device *dev, struct sk_buff *skb,
                                struct nss_wifi_vdev_mesh_per_packet_metadata *mesh_metadata);
void osif_nss_wifili_vdev_spl_receive_ext_mesh(struct net_device *dev, struct sk_buff *skb,
                                struct nss_wifi_vdev_mesh_per_packet_metadata *mesh_metadata);
bool osif_nss_ol_be_vdev_spl_receive_ext_wdsdata(struct net_device *dev, struct sk_buff *skb,
        struct nss_wifi_vdev_wds_per_packet_metadata *wds_metadata);
bool osif_nss_ol_li_vdev_spl_receive_ext_wdsdata(struct net_device *dev, struct sk_buff *skb,
        struct nss_wifi_vdev_wds_per_packet_metadata *wds_metadata);
uint8_t osif_nss_ol_be_vdev_qwrap_mec_check(osif_dev *mpsta_osifp, struct sk_buff *skb);
uint8_t osif_nss_ol_li_vdev_qwrap_mec_check(osif_dev *mpsta_osifp, struct sk_buff *skb);
uint8_t osif_nss_ol_be_vdev_get_nss_qwrap_en(struct ieee80211vap *vap);
uint8_t osif_nss_ol_li_vdev_get_nss_qwrap_en(struct ieee80211vap *vap);
bool osif_nss_ol_be_vdev_spl_receive_ppdu_metadata(struct net_device *dev, struct sk_buff *skb,
        struct nss_wifi_vdev_ppdu_metadata *ppdu_metadata);
bool osif_nss_ol_li_vdev_spl_receive_ppdu_metadata(struct net_device *dev, struct sk_buff *skb,
        struct nss_wifi_vdev_ppdu_metadata *ppdu_metadata);
void *osif_nss_wifili_vdev_get_wds_peer(void *vdev_handle);
void *osif_nss_ol_be_vdev_get_wds_peer(void *vdev_handle);
bool osif_nss_ol_be_vdev_data_receive_mec_check(osif_dev *osdev, struct sk_buff *skb);
bool osif_nss_ol_li_vdev_data_receive_mec_check(osif_dev *osdev, struct sk_buff *skb);

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT

struct nss_wifi_internal_ops {
    uint32_t (* ic_osif_nss_vdev_set_cfg)(osif_dev *osifp, enum osif_nss_vdev_cmd osif_cmd);
    void (* ic_osif_nss_ol_get_peerid)(struct MC_LIST_UPDATE* list_entry, uint32_t *peer_id);
    uint32_t (* ic_osif_nss_ol_peerid_find_hash_find)(struct ieee80211vap *vap,
                                uint8_t *peer_mac_addr, int mac_addr_is_aligned);
    uint8_t (* ic_osif_nss_ol_get_vdevid) (osif_dev *osifp, uint8_t *vdev_id);
    uint8_t (* ic_osif_nss_ol_get_vdevid_fromvdev)(void *vdev, uint8_t *vdev_id);
    uint8_t (* ic_osif_nss_ol_find_pstosif_by_id)(struct net_device *netdev,
                                uint32_t peer_id, osif_dev **psta_osifp);
    void (* ic_osif_nss_ol_vdevcfg_set_offload_params)(void * vdev,
                                struct nss_wifi_vdev_config_msg **p_wifivdevcfg);
    bool (* ic_osif_nss_ol_vdev_handle_monitor_mode)(struct net_device *netdev,
                                struct sk_buff *skb, uint8_t is_chain);
    void (* ic_osif_nss_ol_vdev_tx_inspect_handler)(void *vdev_handle, struct sk_buff *skb);
    void (* ic_osif_nss_ol_vdev_data_receive_meshmode_rxinfo)(struct net_device *dev,
                                struct sk_buff *skb);
    void * (* ic_osif_nss_ol_find_peer_by_id)(osif_dev *osifp, uint32_t peer_id);
    void (* ic_osif_nss_ol_vdev_handle_rawbuf)(struct net_device *netdev, qdf_nbuf_t nbuf,
                                __attribute__((unused)) struct napi_struct *napi);
    uint8_t (* ic_osif_nss_ol_vdev_call_monitor_mode)(struct net_device *netdev, osif_dev  *osdev,
                                qdf_nbuf_t skb_list_head, uint8_t is_chain);
    uint8_t (* ic_osif_nss_ol_vdev_get_stats)(osif_dev *osifp, struct nss_cmn_msg *wifivdevmsg);
    void (* ic_osif_nss_ol_vdev_spl_receive_exttx_compl)(struct net_device *dev, struct sk_buff *skb,
                                struct nss_wifi_vdev_tx_compl_metadata *tx_compl_metadata);
    void (* ic_osif_nss_ol_vdev_spl_receive_ext_mesh)(struct net_device *dev, struct sk_buff *skb,
                                struct nss_wifi_vdev_mesh_per_packet_metadata *mesh_metadata);
    bool (* ic_osif_nss_ol_vdev_spl_receive_extwds_data) (struct net_device *dev, struct sk_buff *skb,
            struct nss_wifi_vdev_wds_per_packet_metadata *wds_metadata);
    uint8_t (* ic_osif_nss_ol_vdev_qwrap_mec_check)(osif_dev *mpsta_osifp, struct sk_buff *skb);
    bool (* ic_osif_nss_ol_vdev_spl_receive_ppdu_metadata) (struct net_device *dev, struct sk_buff *skb,
            struct nss_wifi_vdev_ppdu_metadata *ppdu_metadata);
    uint8_t (* ic_osif_nss_ol_vdev_get_nss_qwrap_en)(struct ieee80211vap *vap);
#ifdef WDS_VENDOR_EXTENSION
    void *(* ic_osif_nss_ol_vdev_get_wds_peer)(void *vdev_handle);
#endif
    void (* ic_osif_nss_ol_vdev_txinfo_handler)(struct ol_ath_softc_net80211 *scn, struct sk_buff *skb,
                                struct nss_wifi_vdev_per_packet_metadata *wifi_metadata, bool is_raw);
    void (* ic_osif_nss_ol_vdev_update_statsv2)(struct ol_ath_softc_net80211 *scn, struct sk_buff * nbuf, void *rx_mpdu_desc, uint8_t htt_rx_status);
    bool (* ic_osif_nss_ol_vdev_data_receive_mec_check)(osif_dev *osdev, struct sk_buff *nbuf);

    void *(* ic_osif_nss_ol_peer_find_hash_find)(struct ieee80211vap *vap,
                                uint8_t *peer_mac_addr, int mac_addr_is_aligned);
};

#endif /* QCA_NSS_WIFI_OFFLOAD_SUPPORT */

#endif /* __NSS_WIFI_MGR_H */
