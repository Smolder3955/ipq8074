/*
 * Copyright (c) 2017-2019 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */

#include <nss_api_if.h>
#include <nss_cmn.h>

#include <qdf_types.h>
#include <qdf_lock.h>
#include <hal_api.h>
#include <hif.h>
#include <htt.h>
#include <queue.h>
#include "dp_htt.h"
#include "dp_types.h"
#include "dp_internal.h"
#include "dp_tx.h"
#include "dp_rx.h"
#include "dp_rx_mon.h"
#include "../../wlan_cfg/wlan_cfg.h"
#include "osif_private.h"
#include "init_deinit_lmac.h"
#include "osif_nss_wifiol_if.h"
#include "osif_nss_wifiol_vdev_if.h"
#include <linux/etherdevice.h>
#include <init_deinit_lmac.h>

#define WIFILI_NSS_TX_DESC_SIZE 20*4
#define WIFILI_NSS_TX_EXT_DESC_SIZE 40*4
#define WIFILI_NSS_TX_DESC_PAGE_LIMIT 240 /* Number of desc per page(12bit) should be<4096, page limit per 1024 byte is 80*3=240 */
#define WIFILI_NSS_MAX_MEM_PAGE_SIZE (WIFILI_NSS_TX_DESC_PAGE_LIMIT * 1024)
#define WIFILI_NSS_MAX_EXT_MEM_PAGE_SIZE  (WIFILI_NSS_TX_DESC_PAGE_LIMIT * 1024)
#define WIFILI_NSS_SOC_PER_PACKET_HEADROOM (NSS_WIFILI_SOC_PER_PACKET_METADATA_OFFSET + NSS_WIFILI_SOC_PER_PACKET_METADATA_SIZE)
#define WIFILI_NSS_CCE_DISABLED 0x1
#define WIFILI_ADDTL_MEM_SEG_SET 0x000000002
#define WIFILI_NSS_PEER_BYTE_SIZE 1152

/*
 * DBDC Tx Descriptors
 */
#ifdef QCA_LOWMEM_CONFIG
#define WIFILI_NSS_DBDC_NUM_TX_DESC (1024 * 16)
#define WIFILI_NSS_DBDC_NUM_TX_EXT_DESC (1024 * 16)
#define WIFILI_NSS_DBDC_NUM_TX_DESC_2 (1024 * 8)
#define WIFILI_NSS_DBDC_NUM_TX_EXT_DESC_2 (1024 * 8)
#elif defined QCA_512M_CONFIG
#define WIFILI_NSS_DBDC_NUM_TX_DESC (1024 * 32)
#define WIFILI_NSS_DBDC_NUM_TX_EXT_DESC (1024 * 32)
#define WIFILI_NSS_DBDC_NUM_TX_DESC_2 (1024 * 16)
#define WIFILI_NSS_DBDC_NUM_TX_EXT_DESC_2 (1024 * 16)
#else
#define WIFILI_NSS_DBDC_NUM_TX_DESC (1024 * 32)
#define WIFILI_NSS_DBDC_NUM_TX_EXT_DESC (1024 * 32)
#define WIFILI_NSS_DBDC_NUM_TX_DESC_2 (1024 * 32)
#define WIFILI_NSS_DBDC_NUM_TX_EXT_DESC_2 (1024 * 32)
#endif

/*
 * DBTC Tx Descriptors
 */
#ifdef QCA_LOWMEM_CONFIG
#define WIFILI_NSS_DBTC_NUM_TX_DESC (1024 * 8)
#define WIFILI_NSS_DBTC_NUM_TX_EXT_DESC (1024 * 8)
#define WIFILI_NSS_DBTC_NUM_TX_DESC_2 (1024 * 8)
#define WIFILI_NSS_DBTC_NUM_TX_EXT_DESC_2 (1024 * 8)
#define WIFILI_NSS_DBTC_NUM_TX_DESC_3 (1024 * 8)
#define WIFILI_NSS_DBTC_NUM_TX_EXT_DESC_3 (1024 * 8)
#elif defined QCA_512M_CONFIG
#define WIFILI_NSS_DBTC_NUM_TX_DESC (1024 * 16)
#define WIFILI_NSS_DBTC_NUM_TX_EXT_DESC (1024 * 16)
#define WIFILI_NSS_DBTC_NUM_TX_DESC_2 (1024 * 16)
#define WIFILI_NSS_DBTC_NUM_TX_EXT_DESC_2 (1024 * 16)
#define WIFILI_NSS_DBTC_NUM_TX_DESC_3 (1024 * 16)
#define WIFILI_NSS_DBTC_NUM_TX_EXT_DESC_3 (1024 * 16)
#else
#define WIFILI_NSS_DBTC_NUM_TX_DESC (1024 * 24)
#define WIFILI_NSS_DBTC_NUM_TX_EXT_DESC (1024 * 24)
#define WIFILI_NSS_DBTC_NUM_TX_DESC_2 (1024 * 24)
#define WIFILI_NSS_DBTC_NUM_TX_EXT_DESC_2 (1024 * 24)
#define WIFILI_NSS_DBTC_NUM_TX_DESC_3 (1024 * 24)
#define WIFILI_NSS_DBTC_NUM_TX_EXT_DESC_3 (1024 * 24)
#endif

/*
 * Number of rx software descriptors
 */
#define WIFILI_NSS_NUM_RX_DESC 4096

/*
 * AP use cases need to allocate more RX Descriptors than the number of
 * entries avaialable in the SW2RXDMA buffer replenish ring. This is to account
 * for frames sitting in REO queues, HW-HW DMA rings etc. Hence using a
 * multiplication factor of 3, to allocate three times as many RX descriptors
 * as RX buffers.
 */
#define WIFILI_NSS_RX_DESC_POOL_WEIGHT 3

/*
 * ast entry type information
 */
typedef enum nss_peer_map_ast_entry_type {
    NSS_PEER_MAP_AST_TYPE_NONE,        /* invalid ast type */
    NSS_PEER_MAP_AST_TYPE_STATIC,      /* static ast entry for connected peer to AP
                           (This includes AP's self mac too as peer map comes for same) */
    NSS_PEER_MAP_AST_TYPE_STA_SELF,    /* static ast entry for sta self peer */
    NSS_PEER_MAP_AST_TYPE_WDS,     /* WDS peer ast entry type */
    NSS_PEER_MAP_AST_TYPE_MEC,     /* Multicast echo ast entry type */
    NSS_PEER_MAP_AST_TYPE_STA_BSS,     /* station's associated BSS entry */
    NSS_PEER_MAP_AST_TYPE_HM_SEC,      /* WDS peer ast entry type HM secondary */
    NSS_PEER_MAP_AST_TYPE_MAX
} nss_peer_map_ast_entry_type_t;


extern int osif_nss_li_vdev_set_peer_nexthop(osif_dev *osif, uint8_t *addr, uint16_t if_num);
bool osif_nss_vdev_skb_needs_linearize(struct net_device *dev, struct sk_buff *skb);
struct nss_wifili_msg wlmsg;
static void osif_nss_wifili_event_receive(ol_ath_soc_softc_t *soc, struct nss_wifili_msg *ntm);
static void osif_nss_wifili_return_peer_mem(struct nss_wifi_peer_mem *nwpmem, uint16_t peer_id);
static uint32_t osif_nss_wifili_get_peer_mem(ol_ath_soc_softc_t *soc, struct nss_wifi_peer_mem *nwpmem, uint16_t peer_id);
static void osif_nss_wifili_pmem_free(ol_ath_soc_softc_t *soc, struct nss_wifi_peer_mem *nwpmem);
static uint osif_nss_wifili_pmem_init(struct nss_wifi_peer_mem *nwpmem);
#if DBDC_REPEATER_SUPPORT
extern int dbdc_rx_process (os_if_t *osif ,struct net_device **dev, wlan_if_t vap, struct sk_buff *skb, int *nwifi);
extern int dbdc_tx_process (wlan_if_t vap, osif_dev **osdev , struct sk_buff *skb);
#endif


#ifdef WDS_VENDOR_EXTENSION

/*
 * osif_nss_wifili_wds_extn_peer_cfg_send()
 *  Collects the wds vendor specific configuration and sends to NSS FW
 */
static int osif_nss_wifili_wds_extn_peer_cfg_send(struct ol_ath_softc_net80211 *scn, void *peer_handle)
{
    struct dp_peer *peer = (struct dp_peer *)peer_handle;
    struct nss_wifili_wds_extn_peer_cfg_msg *wds_msg;
    nss_tx_status_t status;
    nss_wifili_msg_callback_t msg_cb;

    memset(&wlmsg, 0, sizeof(struct nss_wifili_msg));
    wds_msg = &wlmsg.msg.wpeercfg;
    wds_msg->peer_id = peer->peer_ids[0];

    /*
     * For bss peer, enable wds enabled flag
     */
    wds_msg->wds_flags = ((peer->bss_peer || peer->wds_enabled)
     | (peer->wds_ecm.wds_tx_mcast_4addr << 1)
     | (peer->wds_ecm.wds_tx_ucast_4addr << 2)
     | (peer->wds_ecm.wds_rx_filter << 3)
     | (peer->wds_ecm.wds_rx_mcast_4addr << 4)
     | (peer->wds_ecm.wds_rx_ucast_4addr << 5));

    msg_cb = (nss_wifili_msg_callback_t)osif_nss_wifili_event_receive;

    /*
     * Send wds vendor message
     */
    nss_cmn_msg_init(&wlmsg.cm, NSS_WIFILI_INTERFACE, NSS_WIFILI_WDS_VENDOR_MSG,
            sizeof(struct nss_wifili_wds_extn_peer_cfg_msg), msg_cb, NULL);

    status = nss_wifili_tx_msg(scn->nss_radio.nss_rctx, &wlmsg);
    if (status != NSS_TX_SUCCESS) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                    "[nss-wifili]: %s, wifili wds vendor extension cfg send failed status %d",
                                                                __FUNCTION__, status);
        return -1;
    }

    return 0;
}

#endif

static inline void osif_nss_wifili_init_completion_status(ol_ath_soc_softc_t *soc)
{

    init_completion(&soc->nss_soc.osif_nss_ol_wifi_cfg_complete.complete);
    soc->nss_soc.osif_nss_ol_wifi_cfg_complete.response = NSS_TX_SUCCESS;
}

static inline void osif_nss_wifili_set_completion_status(ol_ath_soc_softc_t *soc, nss_tx_status_t status)
{
    soc->nss_soc.osif_nss_ol_wifi_cfg_complete.response = status;
    complete(&soc->nss_soc.osif_nss_ol_wifi_cfg_complete.complete);
}

static void osif_nss_wifili_set_primary_radio(struct ol_ath_softc_net80211 *scn , uint32_t enable)
{
    int ifnum  = -1;
    struct nss_wifili_radio_cmd_msg *radiocmdmsg;
    struct nss_wifili_radio_cfg_msg *radiocfgmsg;
    nss_tx_status_t status;

    if (!scn->nss_radio.nss_rctx) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                "[nss-wifili]: %s, nss core ctx is invalid ", __FUNCTION__);
        return;
    }

    ifnum = scn->nss_radio.nss_rifnum;
    if (ifnum == -1) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                "[nss-wifili]: %s, no if found return ", __FUNCTION__);
        return;
    }

    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO, "[nss-wifili]: %d", NSS_WIFILI_SET_PRIMARY_RADIO);

    memset(&wlmsg, 0, sizeof(struct nss_wifili_msg));
    radiocfgmsg = &wlmsg.msg.radiocfgmsg;
    radiocfgmsg->radio_if_num = ifnum;
    radiocmdmsg = &wlmsg.msg.radiocfgmsg.radiomsg.radiocmdmsg;
    radiocmdmsg->cmd = NSS_WIFILI_SET_PRIMARY_RADIO;
    radiocmdmsg->value = enable;

    /*
     * Send WIFI configure
     */
    nss_cmn_msg_init(&wlmsg.cm, NSS_WIFILI_INTERFACE, NSS_WIFILI_RADIO_CMD_MSG,
            sizeof(struct nss_wifili_radio_cmd_msg), NULL, NULL);

    status = nss_wifili_tx_msg(scn->nss_radio.nss_rctx, &wlmsg);
    if (status != NSS_TX_SUCCESS) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                "[nss-wifili]: %s, cmd %d send failed ", __FUNCTION__, NSS_WIFILI_SET_PRIMARY_RADIO);
        return;
    }

    return;
}

static void osif_nss_wifili_set_always_primary(struct ieee80211com* ic, bool enable)
{
    int ifnum  = -1;
    struct nss_wifili_radio_cmd_msg *radiocmdmsg;
    struct nss_wifili_radio_cfg_msg *radiocfgmsg;
    nss_tx_status_t status;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);

    if (!scn->nss_radio.nss_rctx) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                "[nss-wifili]: %s, nss core ctx is invalid ", __FUNCTION__);
        return;
    }

    ifnum = scn->nss_radio.nss_rifnum;
    if (ifnum == -1) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                "[nss-wifili]: %s, no if found return ", __FUNCTION__);
        return;
    }

    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO, "[nss-wifili]: %d", NSS_WIFILI_SET_ALWAYS_PRIMARY);

    memset(&wlmsg, 0, sizeof(struct nss_wifili_msg));
    radiocfgmsg = &wlmsg.msg.radiocfgmsg;
    radiocfgmsg->radio_if_num = ifnum;
    radiocmdmsg = &wlmsg.msg.radiocfgmsg.radiomsg.radiocmdmsg;
    radiocmdmsg->cmd = NSS_WIFILI_SET_ALWAYS_PRIMARY;
    radiocmdmsg->value = enable;

    /*
     * Send WIFI configure
     */
    nss_cmn_msg_init(&wlmsg.cm, NSS_WIFILI_INTERFACE, NSS_WIFILI_RADIO_CMD_MSG,
            sizeof(struct nss_wifili_radio_cmd_msg), NULL, NULL);

    status = nss_wifili_tx_msg(scn->nss_radio.nss_rctx, &wlmsg);
    if (status != NSS_TX_SUCCESS) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                "[nss-wifili]: %s, cmd %d send failed ", __FUNCTION__, NSS_WIFILI_SET_ALWAYS_PRIMARY);
        return;
    }

    return;
}

static void osif_nss_wifili_set_force_client_mcast_traffic(struct ieee80211com* ic, bool enable)
{
    int ifnum  = -1;
    struct nss_wifili_radio_cmd_msg *radiocmdmsg;
    struct nss_wifili_radio_cfg_msg *radiocfgmsg;
    nss_tx_status_t status;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);

    if (!scn->nss_radio.nss_rctx) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                "[nss-wifili]: %s, nss core ctx is invalid ", __FUNCTION__);
        return;
    }

    ifnum = scn->nss_radio.nss_rifnum;
    if (ifnum == -1) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                "[nss-wifili]: %s, no if found return ", __FUNCTION__);
        return;
    }

    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO, "[nss-wifili]: %d", NSS_WIFILI_SET_FORCE_CLIENT_MCAST_TRAFFIC);

    memset(&wlmsg, 0, sizeof(struct nss_wifili_msg));
    radiocfgmsg = &wlmsg.msg.radiocfgmsg;
    radiocfgmsg->radio_if_num = ifnum;
    radiocmdmsg = &wlmsg.msg.radiocfgmsg.radiomsg.radiocmdmsg;
    radiocmdmsg->cmd = NSS_WIFILI_SET_FORCE_CLIENT_MCAST_TRAFFIC;
    radiocmdmsg->value = enable;

    /*
     * Send WIFI configure
     */
    nss_cmn_msg_init(&wlmsg.cm, NSS_WIFILI_INTERFACE, NSS_WIFILI_RADIO_CMD_MSG,
            sizeof(struct nss_wifili_radio_cmd_msg), NULL, NULL);

    status = nss_wifili_tx_msg(scn->nss_radio.nss_rctx, &wlmsg);
    if (status != NSS_TX_SUCCESS) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                "[nss-wifili]: %s, cmd %d send failed ", __FUNCTION__, NSS_WIFILI_SET_FORCE_CLIENT_MCAST_TRAFFIC);
        return;
    }

    return;
}

static void osif_nss_wifili_set_drop_secondary_mcast(struct ieee80211com* ic, bool enable)
{
    int ifnum  = -1;
    struct nss_wifili_radio_cmd_msg *radiocmdmsg;
    struct nss_wifili_radio_cfg_msg *radiocfgmsg;
    nss_tx_status_t status;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);

    if (!scn->nss_radio.nss_rctx) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                "[nss-wifili]: %s, nss core ctx is invalid ", __FUNCTION__);
        return;
    }

    ifnum = scn->nss_radio.nss_rifnum;
    if (ifnum == -1) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                "[nss-wifili]: %s, no if found return ", __FUNCTION__);
        return;
    }

    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO, "[nss-wifili]: %d", NSS_WIFILI_SET_DROP_SECONDARY_MCAST);

    memset(&wlmsg, 0, sizeof(struct nss_wifili_msg));
    radiocfgmsg = &wlmsg.msg.radiocfgmsg;
    radiocfgmsg->radio_if_num = ifnum;
    radiocmdmsg = &wlmsg.msg.radiocfgmsg.radiomsg.radiocmdmsg;
    radiocmdmsg->cmd = NSS_WIFILI_SET_DROP_SECONDARY_MCAST;
    radiocmdmsg->value = enable;

    /*
     * Send WIFI configure
     */
    nss_cmn_msg_init(&wlmsg.cm, NSS_WIFILI_INTERFACE, NSS_WIFILI_RADIO_CMD_MSG,
            sizeof(struct nss_wifili_radio_cmd_msg), NULL, NULL);

    status = nss_wifili_tx_msg(scn->nss_radio.nss_rctx, &wlmsg);
    if (status != NSS_TX_SUCCESS) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                "[nss-wifili]: %s, cmd %d send failed ", __FUNCTION__, NSS_WIFILI_SET_DROP_SECONDARY_MCAST);
        return;
    }

    return;
}

static void osif_nss_wifili_set_perpkt_txstats(struct ol_ath_softc_net80211 *scn)
{
    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_WARN,
                     "[nss-wifili]:T: %s", __FUNCTION__);
}

static void osif_nss_wifili_set_igmpmld_override_tos(struct ol_ath_softc_net80211 *scn)
{
    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_WARN,
                     "[nss-wifili]:T: %s", __FUNCTION__);
}

static int osif_nss_wifili_stats_cfg(struct ol_ath_softc_net80211 *scn, uint32_t cfg)
{
    struct nss_wifili_stats_cfg_msg *cfgmsg;
    nss_tx_status_t status;
    nss_wifili_msg_callback_t msg_cb;

    if (!scn->nss_radio.nss_rctx) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                     "[nss-wifili]: %s, nss core ctx is NULL", __FUNCTION__);
        return -1;
    }

    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                 "[nss-wifili]: scn %pK nss_ctx %pK nss wifili cfg = %d", scn, scn->nss_radio.nss_rctx, cfg);

    /*
     * memset the nss wifili msg
     */
    memset(&wlmsg, 0, sizeof(struct nss_wifili_msg));
    cfgmsg = &wlmsg.msg.scm;
    cfgmsg->cfg = cfg;

    msg_cb = (nss_wifili_msg_callback_t)osif_nss_wifili_event_receive;

    nss_cmn_msg_init(&wlmsg.cm, NSS_WIFILI_INTERFACE, NSS_WIFILI_STATS_CFG_MSG,
                         sizeof(struct nss_wifili_stats_cfg_msg), msg_cb, NULL);

    status = nss_wifili_tx_msg(scn->nss_radio.nss_rctx, &wlmsg);
    if (status != NSS_TX_SUCCESS) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                   "[nss-wifili]: %s, wiflii extended stats cfg failed status %d ",
                                                                __FUNCTION__, status);
        return -1;
    }

    return 0;
}

static int osif_nss_wifili_enable_v3_stats(struct ol_ath_softc_net80211 *scn, uint32_t flag)
{
    struct nss_wifili_enable_v3_stats_msg *stats_msg;
    nss_tx_status_t status;
    nss_wifili_msg_callback_t msg_cb;

    if (!scn->nss_radio.nss_rctx) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                     "[nss-wifili]: %s, nss core ctx is NULL", __FUNCTION__);
        return -1;
    }

    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                 "[nss-wifili]: scn %pK nss_ctx %pK nss wifili v3 enable = %d", scn, scn->nss_radio.nss_rctx, flag);

    /*
     * memset the nss wifili msg
     */
    memset(&wlmsg, 0, sizeof(struct nss_wifili_msg));
    stats_msg = &wlmsg.msg.enablev3statsmsg;
    stats_msg->radio_id = scn->radio_id;

    /*
     * enable v3 stats collection from NSS
     */
    if (flag) {
        stats_msg->flag = NSS_WIFILI_PDEV_FLAG_V3_STATS_ENABLED;
    } else {
        stats_msg->flag = 0;
    }

    msg_cb = (nss_wifili_msg_callback_t)osif_nss_wifili_event_receive;
    nss_cmn_msg_init(&wlmsg.cm, NSS_WIFILI_INTERFACE, NSS_WIFILI_ENABLE_V3_STATS_MSG,
                         sizeof(struct nss_wifili_enable_v3_stats_msg), msg_cb, NULL);

    status = nss_wifili_tx_msg(scn->nss_radio.nss_rctx, &wlmsg);
    if (status != NSS_TX_SUCCESS) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                   "[nss-wifili]: %s, wiflii v3 stats enable failed status %d ",
                                                                __FUNCTION__, status);
        return -1;
    }

    return 0;
}

static void osif_nss_wifili_enable_ol_statsv2(struct ol_ath_softc_net80211 *scn, uint32_t enable)
{
    struct nss_wifili_stats_cfg_msg *cfgmsg;
    nss_tx_status_t status;
    nss_wifili_msg_callback_t msg_cb;

    if (!scn->nss_radio.nss_rctx) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                "[nss-wifili]: %s, nss core ctx is NULL", __FUNCTION__);
        return;
    }

    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
            "[nss-wifili]: scn %pK nss_ctx %pK nss wifili cfg = %d", scn, scn->nss_radio.nss_rctx, enable);

    /*
     * memset the nss wifili msg
     */
    memset(&wlmsg, 0, sizeof(struct nss_wifili_msg));
    cfgmsg = &wlmsg.msg.scm;
    cfgmsg->cfg = enable;

    msg_cb = (nss_wifili_msg_callback_t)osif_nss_wifili_event_receive;

    nss_cmn_msg_init(&wlmsg.cm, NSS_WIFILI_INTERFACE, NSS_WIFILI_STATS_V2_CFG_MSG,
            sizeof(struct nss_wifili_stats_cfg_msg), msg_cb, NULL);

    status = nss_wifili_tx_msg(scn->nss_radio.nss_rctx, &wlmsg);
    if (status != NSS_TX_SUCCESS) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                "[nss-wifili]: %s, wiflii extended stats cfg failed status %d ",
                __FUNCTION__, status);
        return;
    }
}

static int osif_nss_wifili_pktlog_cfg(struct ol_ath_softc_net80211 *scn, int enable)
{
    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_WARN,
                   "[nss-wifili]:d %s", __FUNCTION__);
    return 0;
}

static int osif_nss_wifili_tx_queue_cfg(struct ol_ath_softc_net80211 *scn, uint32_t range, uint32_t buf_size)
{
    int ifnum  = -1;
    struct nss_wifili_radio_buf_cfg_msg *radiobufcfg;
    struct nss_wifili_radio_cfg_msg *radiocfgmsg;
    nss_tx_status_t status;

    if (!scn->nss_radio.nss_rctx) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                "[nss-wifili]: %s, nss core ctx is invalid ", __FUNCTION__);
        return 0;
    }

    ifnum = scn->nss_radio.nss_rifnum;
    if (ifnum == -1) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                "[nss-wifili]: %s, no if found return ", __FUNCTION__);
        return -1;
    }

    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO, "[nss-wifili]: %d %d %d", NSS_WIFILI_RADIO_BUF_CFG, range, buf_size);

    memset(&wlmsg, 0, sizeof(struct nss_wifili_msg));
    radiocfgmsg = &wlmsg.msg.radiocfgmsg;
    radiocfgmsg->radio_if_num = ifnum;
    radiobufcfg = &wlmsg.msg.radiocfgmsg.radiomsg.radiobufcfgmsg;
    radiobufcfg->range = range;
    radiobufcfg->buf_cnt = buf_size;

    /*
     * Send WIFI configure
     */
    nss_cmn_msg_init(&wlmsg.cm, NSS_WIFILI_INTERFACE, NSS_WIFILI_RADIO_BUF_CFG,
            sizeof(struct nss_wifili_radio_buf_cfg_msg), NULL, NULL);

    status = nss_wifili_tx_msg(scn->nss_radio.nss_rctx, &wlmsg);
    if (status != NSS_TX_SUCCESS) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                "[nss-wifili]: %s, cmd %d send failed ", __FUNCTION__, NSS_WIFILI_RADIO_BUF_CFG);
        return -1;
    }

    return 0;
}

static int osif_nss_wifili_tx_queue_min_threshold_cfg(struct ol_ath_softc_net80211 *scn, uint32_t min_threshold)
{
    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_WARN,
                    "[nss-wifili]:d %s", __FUNCTION__);
    return 0;
}

static int osif_nss_wifili_tx_capture_set(struct ol_ath_softc_net80211 *scn, uint8_t tx_capture_enable)
{
    int ifnum  = -1;
    struct nss_wifili_radio_cmd_msg *radiocmdmsg;
    struct nss_wifili_radio_cfg_msg *radiocfgmsg;
    nss_tx_status_t status;

    if (!scn->nss_radio.nss_rctx) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                "[nss-wifili]: %s, nss core ctx is invalid ", __FUNCTION__);
        return 0;
    }

    ifnum = scn->nss_radio.nss_rifnum;
    if (ifnum == -1) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                "[nss-wifili]: %s, no if found return ", __FUNCTION__);
        return -1;
    }

    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO, "[nss-wifili]: %d %d", NSS_WIFILI_RADIO_TX_CAPTURE_CMD, tx_capture_enable);

    memset(&wlmsg, 0, sizeof(struct nss_wifili_msg));
    radiocfgmsg = &wlmsg.msg.radiocfgmsg;
    radiocfgmsg->radio_if_num = ifnum;
    radiocmdmsg = &wlmsg.msg.radiocfgmsg.radiomsg.radiocmdmsg;
    radiocmdmsg->cmd = NSS_WIFILI_RADIO_TX_CAPTURE_CMD;
    radiocmdmsg->value = tx_capture_enable;

    /*
     * Send WIFI configure
     */
    nss_cmn_msg_init(&wlmsg.cm, NSS_WIFILI_INTERFACE, NSS_WIFILI_RADIO_CMD_MSG,
            sizeof(struct nss_wifili_radio_cmd_msg), NULL, NULL);

    status = nss_wifili_tx_msg(scn->nss_radio.nss_rctx, &wlmsg);
    if (status != NSS_TX_SUCCESS) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                "[nss-wifili]: %s, cmd %d send failed ", __FUNCTION__, NSS_WIFILI_RADIO_TX_CAPTURE_CMD);
        return -1;
    }

    return 0;
}

static int osif_nss_wifili_pdev_add_wds_peer(struct ol_ath_softc_net80211 *scn, uint8_t *peer_mac, uint8_t *dest_mac, uint8_t ast_type, struct cdp_peer *peer_handle)
{
    struct nss_wifili_wds_peer_msg *wlwdspeermsg;
    nss_tx_status_t status;
    nss_wifili_msg_callback_t msg_cb;
    struct dp_peer *peer = (struct dp_peer *)peer_handle;
    struct dp_vdev *vdev = peer->vdev;

    if (!scn->nss_radio.nss_rctx) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                 "[nss-wifili]: %s, nss core ctx is invalid ", __FUNCTION__);
        return 0;
    }

    /*
     * Send wds message to NSS
     */
    memset(&wlmsg, 0, sizeof(struct nss_wifili_msg));

    wlwdspeermsg = &wlmsg.msg.wdspeermsg;
    /*
     * For WDS type HM_SEC peer mac is taken from peer
     */
    if (peer_mac == NULL && ast_type == CDP_TXRX_AST_TYPE_WDS_HM_SEC) {
        memcpy(wlwdspeermsg->peer_mac, peer->mac_addr.raw, 6);
        ast_type = NSS_PEER_MAP_AST_TYPE_HM_SEC;
    } else if (peer_mac != NULL) {
        memcpy(wlwdspeermsg->peer_mac, peer_mac, 6);
        ast_type = NSS_PEER_MAP_AST_TYPE_NONE;
    }

    memcpy(wlwdspeermsg->dest_mac, dest_mac, 6);
    wlwdspeermsg->ast_type = ast_type;
    wlwdspeermsg->pdev_id = vdev->pdev->pdev_id;
    wlwdspeermsg->peer_id = peer->peer_ids[0];

    msg_cb = (nss_wifili_msg_callback_t)osif_nss_wifili_event_receive;
    nss_cmn_msg_init(&wlmsg.cm, NSS_WIFILI_INTERFACE, NSS_WIFILI_WDS_PEER_ADD_MSG,
            sizeof(struct nss_wifili_wds_peer_msg), msg_cb, NULL);

    status = nss_wifili_tx_msg(scn->nss_radio.nss_rctx, &wlmsg);
    if (status != NSS_TX_SUCCESS) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                   "[nss-wifili]: wifi peer message send fail%d ", status);
        return 1;
    }

    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                "[nss-wifili]: sent wds peer add message for mac %pM to NSS", dest_mac);

    return 0;
}

static int osif_nss_wifili_pdev_update_wds_peer(struct ol_ath_softc_net80211 *scn, uint8_t *peer_mac,
                                                uint8_t *dest_mac, uint8_t pdev_id)
{
    struct nss_wifili_wds_peer_msg *wlwdspeermsg;
    nss_tx_status_t status;
    nss_wifili_msg_callback_t msg_cb;

    if (!scn->nss_radio.nss_rctx) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                 "[nss-wifili]: %s, nss core ctx is invalid ", __FUNCTION__);
        return 0;
    }

    /*
     * Send wds update message to NSS
     */
    memset(&wlmsg, 0, sizeof(struct nss_wifili_msg));

    wlwdspeermsg = &wlmsg.msg.wdspeermsg;

    memcpy(wlwdspeermsg->peer_mac, peer_mac, 6);
    memcpy(wlwdspeermsg->dest_mac, dest_mac, 6);
    wlwdspeermsg->ast_type = CDP_TXRX_AST_TYPE_WDS;
    wlwdspeermsg->pdev_id = pdev_id;

    msg_cb = (nss_wifili_msg_callback_t)osif_nss_wifili_event_receive;
    nss_cmn_msg_init(&wlmsg.cm, NSS_WIFILI_INTERFACE, NSS_WIFILI_WDS_PEER_UPDATE_MSG,
            sizeof(struct nss_wifili_wds_peer_msg), msg_cb, NULL);

    status = nss_wifili_tx_msg(scn->nss_radio.nss_rctx, &wlmsg);
    if (status != NSS_TX_SUCCESS) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                   "[nss-wifili]: wifi wds peer update message send fail%d ", status);
        return 1;
    }

    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                "[nss-wifili]: sent wds peer update message for mac %pM to NSS", dest_mac);

    return 0;
}

void osif_nss_wifili_pdev_del_wds_peer_send(struct ol_ath_softc_net80211 *scn, uint8_t *peer_mac, uint8_t *dest_mac, uint8_t ast_type, uint8_t pdev_id)
{
    struct nss_wifili_wds_peer_msg *wlwdspeermsg;
    nss_tx_status_t status;
    nss_wifili_msg_callback_t msg_cb;

    if (!scn->nss_radio.nss_rctx) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
              "[nss-wifili]: %s, nss core ctx is invalid ", __FUNCTION__);
        return;
    }

    /*
     * Send wds message to NSS
     */
    memset(&wlmsg, 0, sizeof(struct nss_wifili_msg));

    wlwdspeermsg = &wlmsg.msg.wdspeermsg;
    memcpy(wlwdspeermsg->peer_mac, peer_mac, 6);
    memcpy(wlwdspeermsg->dest_mac, dest_mac, 6);
    wlwdspeermsg->ast_type = ast_type;
    wlwdspeermsg->pdev_id = pdev_id;

    msg_cb = (nss_wifili_msg_callback_t)osif_nss_wifili_event_receive;
    nss_cmn_msg_init(&wlmsg.cm, NSS_WIFILI_INTERFACE, NSS_WIFILI_WDS_PEER_DEL_MSG,
            sizeof(struct nss_wifili_wds_peer_msg), msg_cb, NULL);

    status = nss_wifili_tx_msg(scn->nss_radio.nss_rctx, &wlmsg);
    if (status != NSS_TX_SUCCESS) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                 "[nss-wifili]: wifi peer message send fail%d ", status);
        return;
    }

    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
              "[nss-wifili]: sent wds peer delete message for mac %pM to NSS", dest_mac);
    return;
}

void osif_nss_wifili_pdev_del_wds_peer_work_func(void *arg)
{
    struct ol_ath_nss_del_wds_entry *del_wds = (struct ol_ath_nss_del_wds_entry *)arg;
    osif_nss_wifili_pdev_del_wds_peer_send(del_wds->scn, del_wds->peer_mac, del_wds->dest_mac, del_wds->ast_type, del_wds->pdev_id);
    qdf_mem_free(del_wds);
}

static int osif_nss_wifili_pdev_del_wds_peer(struct ol_ath_softc_net80211 *scn, uint8_t *peer_mac, uint8_t *dest_mac, uint8_t ast_type, uint8_t pdev_id)
{
    /*
     * Send NSS HM_SEC AST type.
     */
    if (ast_type == CDP_TXRX_AST_TYPE_WDS_HM_SEC) {
        ast_type = NSS_PEER_MAP_AST_TYPE_HM_SEC;
    } else {
        ast_type = NSS_PEER_MAP_AST_TYPE_NONE;
    }

    if (in_irq() || irqs_disabled()) {
        struct ol_ath_nss_del_wds_entry  *del_wds_entry = qdf_mem_malloc(sizeof(struct ol_ath_nss_del_wds_entry));
        if (del_wds_entry) {
            del_wds_entry->scn = scn;
            memcpy(del_wds_entry->dest_mac, dest_mac, 6);
            memcpy(del_wds_entry->peer_mac, peer_mac, 6);
            del_wds_entry->ast_type = ast_type;
            del_wds_entry->pdev_id = pdev_id;
            qdf_create_work(0, &del_wds_entry->wds_del_work, osif_nss_wifili_pdev_del_wds_peer_work_func, del_wds_entry);
            qdf_sched_work(0, &del_wds_entry->wds_del_work);
        }
    } else {
        osif_nss_wifili_pdev_del_wds_peer_send(scn, peer_mac, dest_mac, ast_type, pdev_id);
    }
    return 0;
}

static int osif_nss_wifili_map_wds_peer(ol_ath_soc_softc_t *soc, uint16_t peer_id, uint16_t hw_ast_idx, uint8_t vdev_id, uint8_t *mac)
{
    struct nss_wifili_wds_peer_map_msg *wlwdspeermsg;
    nss_tx_status_t status;
    nss_wifili_msg_callback_t msg_cb;

    if (!soc->nss_soc.nss_sctx) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                "[nss-wifili]: nss core ctx is invalid ");
        return 0;
    }

    /*
     * Send wds message to NSS
     */
    memset(&wlmsg, 0, sizeof(struct nss_wifili_msg));

    wlwdspeermsg = &wlmsg.msg.wdspeermapmsg;
    wlwdspeermsg->peer_id = peer_id;
    wlwdspeermsg->ast_idx = hw_ast_idx;
    wlwdspeermsg->vdev_id = vdev_id;
    memcpy(wlwdspeermsg->dest_mac, mac, 6);

    /*
     * TODO: Modify the nss interface correctly
     */
    msg_cb = (nss_wifili_msg_callback_t)osif_nss_wifili_event_receive;
    nss_cmn_msg_init(&wlmsg.cm, NSS_WIFILI_INTERFACE, NSS_WIFILI_WDS_PEER_MAP_MSG,
            sizeof(struct nss_wifili_wds_peer_map_msg), msg_cb, NULL);

    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                "[nss-wifili]: wds peer notify %d hw %d vdev %d mac %pM ", peer_id, hw_ast_idx, vdev_id, mac);

    status = nss_wifili_tx_msg(soc->nss_soc.nss_sctx, &wlmsg);
    if (status != NSS_TX_SUCCESS) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
               "[nss-wifili]: wifi peer message send fail%d ", status);
        return 1;
    }

    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
               "[nss-wifili]: sent wds peer map message to NSS");

    return 0;
}

static void osif_nss_wifili_set_hmmc_dscp_tid(struct ol_ath_softc_net80211 *scn, uint8_t value)
{
    struct nss_wifili_hmmc_dscp_tid_set_msg *shmmcdcptidmsg;
    nss_tx_status_t status;
    nss_wifili_msg_callback_t msg_cb;

    if (!scn->nss_radio.nss_rctx) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                "[nss-wifili]: nss core ctx is invalid ");
        return;
    }

    /*
     * Send wds message to NSS
     */
    memset(&wlmsg, 0, sizeof(struct nss_wifili_msg));

    shmmcdcptidmsg = &wlmsg.msg.shmmcdcptidmsg;
    shmmcdcptidmsg->value = value;
    shmmcdcptidmsg->radio_id = scn->radio_id;

    msg_cb = (nss_wifili_msg_callback_t)osif_nss_wifili_event_receive;
    nss_cmn_msg_init(&wlmsg.cm, NSS_WIFILI_INTERFACE, NSS_WIFILI_SET_HMMC_DSCP_TID_MSG,
            sizeof(struct nss_wifili_hmmc_dscp_tid_set_msg), msg_cb, NULL);

    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                "[nss-wifili]: hmmc dscp tid set to val %d", value);

    status = nss_wifili_tx_msg(scn->nss_radio.nss_rctx, &wlmsg);
    if (status != NSS_TX_SUCCESS) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
               "[nss-wifili]: hmmc dscp tid set message send fail%d ", status);
        return;
    }

    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
               "[nss-wifili]: sent hmmc dscp tid set message to NSS");

    return;
}

static void osif_nss_wifili_set_hmmc_dscp_override(struct ol_ath_softc_net80211 *scn, uint8_t value)
{
    struct nss_wifili_hmmc_dscp_override_set_msg *shmmcdscpmsg;
    nss_tx_status_t status;
    nss_wifili_msg_callback_t msg_cb;

    if (!scn->nss_radio.nss_rctx) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                "[nss-wifili]: nss core ctx is invalid ");
        return;
    }

    /*
     * Send wds message to NSS
     */
    memset(&wlmsg, 0, sizeof(struct nss_wifili_msg));

    shmmcdscpmsg = &wlmsg.msg.shmmcdscpmsg;
    shmmcdscpmsg->value = value;
    shmmcdscpmsg->radio_id = scn->radio_id;

    msg_cb = (nss_wifili_msg_callback_t)osif_nss_wifili_event_receive;
    nss_cmn_msg_init(&wlmsg.cm, NSS_WIFILI_INTERFACE, NSS_WIFILI_SET_HMMC_DSCP_OVERRIDE_MSG,
            sizeof(struct nss_wifili_hmmc_dscp_override_set_msg), msg_cb, NULL);

    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                "[nss-wifili]: hmmc dscp override enable val %d", value);

    status = nss_wifili_tx_msg(scn->nss_radio.nss_rctx, &wlmsg);
    if (status != NSS_TX_SUCCESS) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
               "[nss-wifili]: hmmc dscp override set message send fail%d ", status);
        return;
    }

    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
               "[nss-wifili]: sent hmmc dscp override set message to NSS");

    return;
}


static void osif_nss_wifili_enable_dbdc_process(struct ieee80211com* ic, uint32_t enable)
{
    struct nss_wifili_dbdc_repeater_set_msg *dbdcrptrmsg;
    nss_tx_status_t status;
    nss_wifili_msg_callback_t msg_cb;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);

    if (!scn->nss_radio.nss_rctx) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                "[nss-wifili]: nss core ctx is invalid ");
        return;
    }

    /*
     * Send wds message to NSS
     */
    memset(&wlmsg, 0, sizeof(struct nss_wifili_msg));

    dbdcrptrmsg = &wlmsg.msg.dbdcrptrmsg;
    dbdcrptrmsg->is_dbdc_en = enable;

    msg_cb = (nss_wifili_msg_callback_t)osif_nss_wifili_event_receive;
    nss_cmn_msg_init(&wlmsg.cm, NSS_WIFILI_INTERFACE, NSS_WIFILI_DBDC_REPEATER_SET_MSG,
            sizeof(struct nss_wifili_dbdc_repeater_set_msg), msg_cb, NULL);

    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                "[nss-wifili]: dbdc repeater enable val %d", enable);

    status = nss_wifili_tx_msg(scn->nss_radio.nss_rctx, &wlmsg);
    if (status != NSS_TX_SUCCESS) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
               "[nss-wifili]: wifi peer message send fail%d ", status);
        return;
    }

    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
               "[nss-wifili]: sent dbdc repeater set message to NSS");

    return;
}

static void osif_nss_wifili_set_cmd(struct ol_ath_softc_net80211 *scn, enum osif_nss_wifi_cmd osif_cmd)
{
    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_WARN,
            "[nss-wifili]:T %s", __FUNCTION__);
}

static void osif_nss_wifili_read_pkt_prehdr(struct ol_ath_softc_net80211 *scn, struct sk_buff *nbuf)
{
    struct dp_soc *dpsoc = NULL;
    struct dp_pdev *pdev = (struct dp_pdev *)wlan_pdev_get_dp_handle(scn->sc_pdev);
    uint32_t rx_desc_len = hal_rx_get_desc_len();

    if (!pdev) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                "[nss-wifili]: %s pdev is invalid ", __FUNCTION__);
        return;
    }

    dpsoc = pdev->soc;
    if (!dpsoc) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                "[nss-wifili]: %s, soc is invalid ", __FUNCTION__);
        return;
    }

    dma_unmap_single(scn->soc->nss_soc.dmadev, virt_to_phys(nbuf->head), rx_desc_len, DMA_FROM_DEVICE);

    /*
     * check if first MSDU in MPDU flag is set
     */
    if (hal_rx_msdu_end_first_msdu_get((uint8_t *)nbuf->head) ) {
        qdf_nbuf_set_rx_chfrag_start(nbuf, 1);
    }

    if (hal_rx_msdu_end_last_msdu_get((uint8_t *)nbuf->head)) {
        qdf_nbuf_set_rx_chfrag_end(nbuf, 1);
    }
}

/*
 * osif_nss_ol_set_peer_security_info()
 *	Set the security per peer
 */
static int osif_nss_wifili_set_peer_security_info(struct ol_ath_softc_net80211 *scn, void *peer_handler,
                                                     uint8_t pkt_type, uint8_t cipher_type, uint8_t mic_key[])
{
    struct dp_peer *peer= (struct dp_peer *)peer_handler;
    uint16_t peer_id = 0;
    struct nss_ctx_instance *nss_contex = NULL;
    struct nss_wifili_peer_security_type_msg *sec_msg = NULL;
    nss_tx_status_t status;
    nss_wifili_msg_callback_t msg_cb;

    /*
     * check whether scn or peer handler is null
     */
    if (!scn || !peer) {
       QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                    "[nss-wifili]: %s, peer handler %pK or radio %pK is NULL", __func__, peer_handler, scn);
       return QDF_STATUS_E_FAILURE;
    }

    nss_contex = scn->nss_radio.nss_rctx;
    if (!nss_contex) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                    "[nss-wifili]: peer security error - Null nss context");
        return QDF_STATUS_E_FAILURE;
    }

    /*
     * get the peer id from the peer handler
     */
    peer_id = peer->peer_ids[0];

    memset(&wlmsg, 0, sizeof(struct nss_wifili_msg));
    sec_msg = &wlmsg.msg.securitymsg;
    sec_msg->peer_id = peer_id;
    sec_msg->pkt_type = pkt_type;
    sec_msg->security_type = cipher_type;
    memcpy(&sec_msg->mic_key[0], mic_key, NSS_WIFILI_MIC_KEY_LEN);

    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
             "[nss-wifili]: Send the security type for peer_id %d cipher mode %d pkt_type %d",
                                                peer_id, cipher_type, pkt_type);

    msg_cb = (nss_wifili_msg_callback_t)osif_nss_wifili_event_receive;
    nss_cmn_msg_init(&wlmsg.cm, NSS_WIFILI_INTERFACE, NSS_WIFILI_PEER_SECURITY_TYPE_MSG,
            sizeof(struct nss_wifili_peer_security_type_msg), msg_cb, NULL);

    status = nss_wifili_tx_msg(nss_contex, &wlmsg);
    if (status != NSS_TX_SUCCESS) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                 "[nss-wifili]: wifi security send message failed error %d ", status);
        return QDF_STATUS_E_FAILURE;
    }

    return QDF_STATUS_SUCCESS;
}

static struct nss_wifi_radio_ops nss_wifili_radio_ops = {
#ifdef WDS_VENDOR_EXTENSION
    osif_nss_wifili_wds_extn_peer_cfg_send,
#endif
    osif_nss_wifili_set_primary_radio,
    osif_nss_wifili_set_always_primary,
    osif_nss_wifili_set_force_client_mcast_traffic,
    osif_nss_wifili_set_perpkt_txstats,
    osif_nss_wifili_set_igmpmld_override_tos,
    osif_nss_wifili_stats_cfg,
    osif_nss_wifili_enable_ol_statsv2,
    osif_nss_wifili_pktlog_cfg,
    osif_nss_wifili_tx_queue_cfg,
    osif_nss_wifili_tx_queue_min_threshold_cfg,
    osif_nss_wifili_tx_capture_set,
    osif_nss_wifili_pdev_add_wds_peer,
    osif_nss_wifili_pdev_del_wds_peer,
    osif_nss_wifili_enable_dbdc_process,
    osif_nss_wifili_set_cmd,
    osif_nss_wifili_read_pkt_prehdr,
    osif_nss_wifili_set_drop_secondary_mcast,
    osif_nss_wifili_set_peer_security_info,
    osif_nss_wifili_set_hmmc_dscp_override,
    osif_nss_wifili_set_hmmc_dscp_tid,
    osif_nss_wifili_enable_v3_stats,
    osif_nss_wifili_pdev_update_wds_peer,
};

static void osif_nss_wifili_store_other_pdev_stavap(struct ieee80211com* ic)
{
    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_WARN,
             "[nss-wifili]:%s ",__FUNCTION__);
}

static int osif_nss_wifili_wifi_monitor_set_filter(struct ieee80211com* ic, uint32_t filter_type)
{
    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_WARN,
             "[nss-wifili]:%s ",__FUNCTION__);
    return 0;
}


static struct nss_vdev_ops nss_wifili_vops = {
#if DBDC_REPEATER_SUPPORT
    osif_nss_wifili_store_other_pdev_stavap,            /* pdev command */
#endif
    osif_nss_vdev_me_reset_snooplist,
    osif_nss_vdev_me_update_member_list,
    osif_nss_ol_vap_xmit,
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
    osif_nss_vdev_me_update_hifitlb,
#endif
    osif_nss_vdev_me_dump_denylist,
    osif_nss_vdev_me_add_deny_member,
    osif_nss_vdev_set_cfg,
    osif_nss_vdev_process_mpsta_tx,
    osif_nss_wifili_wifi_monitor_set_filter,        /* set monitor */
    osif_nss_vdev_get_nss_id,
    osif_nss_vdev_process_extap_tx,
    osif_nss_vdev_me_dump_snooplist,
    osif_nss_ol_vap_delete,
    osif_nss_vdev_me_add_member_list,
    osif_nss_vdev_vow_dbg_cfg,
    osif_nss_wifili_enable_dbdc_process,
    osif_nss_vdev_set_wifiol_ctx,
    osif_nss_vdev_me_delete_grp_list,
    osif_nss_vdev_me_create_grp_list,
    osif_nss_vdev_me_delete_deny_list,
    osif_nss_vdev_me_remove_member_list,
    osif_nss_vdev_alloc,
    osif_nss_ol_vap_create,
    osif_nss_wifili_vap_updchdhdr,
    osif_nss_vdev_set_dscp_tid_map,
    osif_nss_vdev_set_dscp_tid_map_id,
    osif_nss_ol_vap_up,
    osif_nss_ol_vap_down,
    osif_nss_vdev_set_inspection_mode,
    osif_nss_vdev_extap_table_entry_add,
    osif_nss_vdev_extap_table_entry_del,
    osif_nss_vdev_psta_add_entry,
    osif_nss_vdev_psta_delete_entry,
    osif_nss_vdev_qwrap_isolation_enable,
    osif_nss_vdev_set_read_rxprehdr,
    osif_nss_li_vdev_set_peer_nexthop,
    osif_nss_vdev_update_vlan,
    osif_nss_vdev_set_vlan_type,
};

static struct nss_wifi_internal_ops nss_wifili_iops = {
    osif_nss_wifili_vdev_set_cfg,
    osif_nss_wifili_get_peerid,
    osif_nss_wifili_peerid_find_hash_find,
    osif_nss_wifili_get_vdevid_fromosif,
    osif_nss_wifili_get_vdevid_fromvdev,
    osif_nss_wifili_find_pstosif_by_id,
    osif_nss_wifili_vdevcfg_set_offload_params,
    osif_nss_wifili_vdev_handle_monitor_mode,
    osif_nss_wifili_vdev_tx_inspect_handler,
#if ATH_DATA_RX_INFO_EN || MESH_MODE_SUPPORT
    osif_nss_wifili_vdev_data_receive_meshmode_rxinfo,
#else
    NULL,
#endif
    osif_nss_wifili_find_peer_by_id,
    osif_nss_ol_vdev_handle_rawbuf,
    osif_nss_wifili_vdev_call_monitor_mode,
    osif_nss_wifili_vdev_get_stats,
    osif_nss_wifili_vdev_spl_receive_exttx_compl,
    osif_nss_wifili_vdev_spl_receive_ext_mesh,
    osif_nss_ol_li_vdev_spl_receive_ext_wdsdata,
#if ATH_SUPPORT_WRAP
    osif_nss_ol_li_vdev_qwrap_mec_check,
#else
    NULL,
#endif
    osif_nss_ol_li_vdev_spl_receive_ppdu_metadata,
#if ATH_SUPPORT_WRAP
    osif_nss_ol_li_vdev_get_nss_qwrap_en,
#else
    NULL,
#endif
#ifdef WDS_VENDOR_EXTENSION
    osif_nss_wifili_vdev_get_wds_peer,
#endif
    osif_nss_wifili_vdev_txinfo_handler,
    osif_nss_wifili_vdev_update_statsv2,
    osif_nss_ol_li_vdev_data_receive_mec_check,
    osif_nss_wifili_peer_find_hash_find,
};

static void osif_nss_update_ring_info(struct dp_soc *dpsoc, struct dp_srng *srng , struct nss_wifili_hal_srng_info *wlhsm, void *pdev_base_paddr, void *pdev_base_addr)
{
    uint32_t dev_base_paddr = (uint32_t)(nss_ptr_t)pdev_base_paddr;
    uint32_t dev_base_addr = (uint32_t )(nss_ptr_t)pdev_base_addr;


    struct hal_srng_params ring_params;
    int j =0 ;
    uint32_t hwreg_off = 0;

    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                   "[nss-wifili]: %s, enter ", __FUNCTION__);

    hal_get_srng_params(dpsoc->hal_soc, srng->hal_srng, &ring_params);
    wlhsm->ring_id =  ring_params.ring_id;
    wlhsm->ring_dir =  ring_params.ring_dir;
    wlhsm->ring_base_paddr =  ring_params.ring_base_paddr;
    wlhsm->entry_size =  ring_params.entry_size;
    wlhsm->num_entries =  ring_params.num_entries;
    wlhsm->flags = ring_params.flags;

    for (j = 0 ; j < MAX_SRNG_REG_GROUPS; j++) {
        hwreg_off = (uint32_t)(nss_ptr_t)ring_params.hwreg_base[j] - dev_base_addr;
        wlhsm->hwreg_base[j] = dev_base_paddr + hwreg_off;
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                 "[nss-wifili]: offset %d dev_base_paddr %x reg base %x  dev_base_addr %x reg base %p",
                  hwreg_off, dev_base_paddr, wlhsm->hwreg_base[j], dev_base_addr, ring_params.hwreg_base[j]);
    }

    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                 "[nss-wifili]: ID %x Dir %x  base %x ",wlhsm->ring_id, wlhsm->ring_dir, wlhsm->ring_base_paddr );
    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                "[nss-wifili]: entry_size %x  num entries %x  ", wlhsm->entry_size, wlhsm->num_entries );
    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                "[nss-wifili]: flag %x  ", wlhsm->flags);
}

static void osif_nss_wifili_desc_pool_free(ol_ath_soc_softc_t *soc)
{
    uint8_t i;
    struct dp_soc *dpsoc = (struct dp_soc *)wlan_psoc_get_dp_handle(soc->psoc_obj);

    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_WARN,
                    "[nss-wifili]: Cleaning NSS WiFili Memory %s", __FUNCTION__);
    /*
     * Free the host pool memory
     */
    for (i = 0;i < OSIF_NSS_WIFILI_MAX_NUMBER_OF_PAGE; i++) {
        if (!soc->nss_soc.desc_pmemaddr[i]) {
            continue;
        }
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                 "[nss-wifili]: Free vmem = %lx Free pmeme = %x Memsize = %u",
                               soc->nss_soc.desc_vmemaddr[i], soc->nss_soc.desc_pmemaddr[i], soc->nss_soc.desc_memsize[i]);

        qdf_mem_free_consistent(dpsoc->osdev, dpsoc->osdev->dev, soc->nss_soc.desc_memsize[i],
                     (void *)soc->nss_soc.desc_vmemaddr[i], soc->nss_soc.desc_pmemaddr[i], 0);
        soc->nss_soc.desc_vmemaddr[i] = 0;
        soc->nss_soc.desc_pmemaddr[i] = 0;
        soc->nss_soc.desc_memsize[i] = 0;
    }
    return;
}

static int osif_nss_wifili_set_default_nexthop(ol_ath_soc_softc_t *soc)
{
    nss_tx_status_t status;

    if (!soc->nss_soc.nss_sctx) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
               "[nss-wifili]: %s, nss core ctx is invalid ", __FUNCTION__);
        return -1;
    }

    status = nss_wifi_vdev_base_set_next_hop(soc->nss_soc.nss_sctx, NSS_ETH_RX_INTERFACE);
    if (status != NSS_TX_SUCCESS) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_WARN,
            "[nss-wifili]: Unable to send the vdev set next hop message to NSS %d", status);
        return -1;
    }

    return 0;
}

static void osif_nss_wifili_peer_delete(ol_ath_soc_softc_t *soc, uint16_t peer_id, uint8_t vdev_id)
{

    /* struct nss_wifili_msg wlmsg; */
    struct nss_wifili_peer_msg *wlpeermsg;
    nss_tx_status_t status;
    nss_wifili_msg_callback_t msg_cb;
#if ATH_SUPPORT_WRAP
    uint8_t mpsta_vdev_id;
#endif

    if (!soc->nss_soc.nss_sctx) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
               "[nss-wifili]: %s, nss core ctx is invalid ", __FUNCTION__);
        return;
    }

    /*
     * Send pdev init message to NSS
     */
    memset(&wlmsg, 0, sizeof(struct nss_wifili_msg));

    wlpeermsg = &wlmsg.msg.peermsg;
    wlpeermsg->peer_id = peer_id;
    wlpeermsg->psta_vdev_id = vdev_id;
#if ATH_SUPPORT_WRAP
    /*
     * Check if the vap associated with peer is psta_peer.
     * Then send mpsta_vdev id to NSS FW.
     */
    if(osif_nss_wifili_vdev_get_mpsta_vdevid(soc, peer_id, vdev_id, &mpsta_vdev_id)) {
        vdev_id = mpsta_vdev_id;
    }
#endif
    wlpeermsg->vdev_id = vdev_id;

    /*
     * TODO: Modify the nss interface correctly
     */
    msg_cb = (nss_wifili_msg_callback_t)osif_nss_wifili_event_receive;
    nss_cmn_msg_init(&wlmsg.cm, NSS_WIFILI_INTERFACE, NSS_WIFILI_PEER_DELETE_MSG,
            sizeof(struct nss_wifili_peer_msg), msg_cb, NULL);

    status = nss_wifili_tx_msg(soc->nss_soc.nss_sctx, &wlmsg);
    if (status != NSS_TX_SUCCESS) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
            "[nss-wifili]: wifi peer message send fail%d ", status);
        return;
    }

    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
            "[nss-wifili]: Sent peer delete message to NSS");
}

static void osif_nss_wifili_nawds_enable(ol_ath_soc_softc_t *soc, void *dpeer, uint16_t is_nawds)
{
    struct nss_wifili_peer_nawds_enable_msg *nawdsmsg;
    nss_tx_status_t status;
    struct dp_peer *peer = (struct dp_peer*)dpeer;

    if (!soc->nss_soc.nss_sctx) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                "[nss-wifili]: nss core ctx is invalid");
        return;
    }

    /*
     * Send pdev init message to NSS
     */
    memset(&wlmsg, 0, sizeof(struct nss_wifili_msg));

    nawdsmsg = &wlmsg.msg.nawdsmsg;
    nawdsmsg->peer_id = peer->peer_ids[0];

    if (peer->nawds_enabled) {
        nawdsmsg->is_nawds = is_nawds;
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                "[nss-wifili]: nawds enable peer-id %d ", nawdsmsg->peer_id);
    }

    nss_cmn_msg_init(&wlmsg.cm, NSS_WIFILI_INTERFACE, NSS_WIFILI_PEER_NAWDS_ENABLE_MSG,
            sizeof(struct nss_wifili_peer_nawds_enable_msg), NULL, NULL);

    status = nss_wifili_tx_msg(soc->nss_soc.nss_sctx, &wlmsg);
    if (status != NSS_TX_SUCCESS) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                "[nss-wifili]: wifi peer message send fail%d", status);
        return;
    }

    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
              "[nss-wifili]: sent nawds enable message to NSS peer_id = %u", nawdsmsg->peer_id);
}

static void osif_nss_wifili_peer_create(ol_ath_soc_softc_t *soc, uint16_t peer_id,
        uint16_t hw_peer_id, uint8_t vdev_id, uint8_t *peer_mac_addr, uint32_t tx_ast_hash)
{

    struct nss_wifili_peer_msg *wlpeermsg;
    nss_tx_status_t status;
#if ATH_SUPPORT_WRAP
    uint8_t mpsta_vdev_id;
#endif
    nss_wifili_msg_callback_t msg_cb;
    struct dp_peer *peer  = NULL;
    struct dp_soc  *dpsoc = NULL;

    if (!soc->nss_soc.nss_sctx) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                 "[nss-wifili]: nss core ctx is invalid");
        return;
    }

    /*
     * Send pdev init message to NSS
     */
    memset(&wlmsg, 0, sizeof(struct nss_wifili_msg));

    wlpeermsg = &wlmsg.msg.peermsg;
    wlpeermsg->peer_id = peer_id;
    wlpeermsg->psta_vdev_id = vdev_id;
#if ATH_SUPPORT_WRAP
    /*
     * Check if the vap associated with peer is psta_peer.
     * Then send mpsta_vdev id to NSS FW.
     */
    if(osif_nss_wifili_vdev_get_mpsta_vdevid(soc, peer_id, vdev_id, &mpsta_vdev_id)) {
        vdev_id = mpsta_vdev_id;
    }
#endif
    wlpeermsg->vdev_id = vdev_id;
    wlpeermsg->hw_ast_idx = hw_peer_id;
    wlpeermsg->tx_ast_hash = tx_ast_hash;

    dpsoc = (struct dp_soc *)wlan_psoc_get_dp_handle(soc->psoc_obj);
    if (!dpsoc) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                "[nss-wifili]: dp_soc is NULL");
        return;
    }

    peer = dp_peer_find_by_id(dpsoc, peer_id);

    if (peer && peer->nawds_enabled) {
        wlpeermsg->is_nawds = 1;
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                "[nss-wifili]: Nawds Peer Create with mac %pM ", peer_mac_addr);
    }

    memcpy(wlpeermsg->peer_mac_addr, peer_mac_addr, 6);
    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
               "[nss-wifili]: peer create id %d vap %d ast %d mac %pM",
                    peer_id, vdev_id, hw_peer_id, wlpeermsg->peer_mac_addr);

    wlpeermsg->nss_peer_mem = osif_nss_wifili_get_peer_mem(soc, &soc->nss_soc.nwpmem, peer_id);
    if (wlpeermsg->nss_peer_mem == 0) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                "[nss-wifili]: peer memory allocation failed");
        return;
    }
    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
               "[nss-wifili]: peer memory 0x%x peer id %d",
                    (uint32_t)wlpeermsg->nss_peer_mem, peer_id);
    /*
     * TODO: Modify the nss interface correctly
     */
    msg_cb = (nss_wifili_msg_callback_t)osif_nss_wifili_event_receive;
    nss_cmn_msg_init(&wlmsg.cm, NSS_WIFILI_INTERFACE, NSS_WIFILI_PEER_CREATE_MSG,
            sizeof(struct nss_wifili_peer_msg), msg_cb, NULL);

    status = nss_wifili_tx_msg(soc->nss_soc.nss_sctx, &wlmsg);
    if (status != NSS_TX_SUCCESS) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
              "[nss-wifili]: wifi peer message send fail%d", status);
        return;
    }

    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
              "[nss-wifili]: sent peer create message to NSS peer_id = %u",peer_id);
}

static int osif_nss_wifili_soc_stop(ol_ath_soc_softc_t *soc)
{
    /* struct nss_wifili_stop_msg *wsrm = NULL; */
    nss_tx_status_t status;
    uint32_t if_num = soc->nss_soc.nss_sifnum;
    struct nss_ctx_instance *nss_contex = soc->nss_soc.nss_sctx;
    nss_wifili_msg_callback_t msg_cb;
    int retrycnt = 0;
    int ret;

    if (!nss_contex) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                    "[nss-wifili]: stop ERROR - Null nss context");
        return QDF_STATUS_E_FAILURE;
    }
    /*
     * TODO: Add if number sanity check
     */
    /* wsrm = &wifilimsg.msg.stop; */
    /* wsrm->stop = 1; */

    msg_cb = (nss_wifili_msg_callback_t)osif_nss_wifili_event_receive;
    osif_nss_wifili_init_completion_status(soc);

    nss_cmn_msg_init(&wlmsg.cm, if_num, NSS_WIFILI_STOP_MSG,
            0, msg_cb, NULL);

    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                "[nss-wifili]: sending soc stop message to NSS");

    while ((status = nss_wifili_tx_msg(nss_contex, &wlmsg)) != NSS_TX_SUCCESS) {
        retrycnt++;
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
              "[nss-wifili]: wifi soc stop fail %d retrying %d", status, retrycnt);
        if (retrycnt >= OSIF_NSS_OL_MAX_RETRY_CNT) {
            QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                  "[nss-wifili]: wifi soc stop failed with max retry of %d", OSIF_NSS_OL_MAX_RETRY_CNT);
            return QDF_STATUS_E_FAILURE;
        }
    }

    /*
     * Blocking call, wait till we get ACK for this msg.
     *
     * NOTE: Return value is ignored and if we timedout we treat the operation as if
     * it was a success
     */
    ret = wait_for_completion_timeout(&soc->nss_soc.osif_nss_ol_wifi_cfg_complete.complete,
                                      msecs_to_jiffies(OSIF_NSS_WIFI_SOC_STOP_TIMEOUT_MS));
    if (ret == 0) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                    "[nss-wifili]: Waiting for soc wifili soc stop timed out");
        return QDF_STATUS_E_FAILURE;
    }

    /*
     * ACK/NACK received from NSS FW
     *
     */
    if (NSS_TX_FAILURE == soc->nss_soc.osif_nss_ol_wifi_cfg_complete.response) {
         QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                "[nss-wifili]: received NACK for soc stop from nss");
         return QDF_STATUS_E_FAILURE;
    }

    mdelay(500);
    return QDF_STATUS_SUCCESS;
}

static QDF_STATUS osif_nss_wifili_soc_reset(ol_ath_soc_softc_t *soc)
{
    nss_tx_status_t status;
    uint32_t if_num = soc->nss_soc.nss_sifnum;
    struct nss_ctx_instance *nss_contex = soc->nss_soc.nss_sctx;
    nss_wifili_msg_callback_t msg_cb;
    QDF_STATUS error = QDF_STATUS_SUCCESS;
    int retrycnt = 0;

    if (!nss_contex) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
               "[nss-wifili]: nss context is null");
        return QDF_STATUS_E_FAILURE;
    }

    /*
     * TODO: Add if number sanity check
     */
    /* wsrm = &wlmsg.msg.wlsoc_reset; */
    /* wsrm->reset = 1; */

    msg_cb = (nss_wifili_msg_callback_t)osif_nss_wifili_event_receive;
    osif_nss_wifili_init_completion_status(soc);

    nss_cmn_msg_init(&wlmsg.cm, if_num, NSS_WIFILI_SOC_RESET_MSG,
            0, msg_cb, NULL);

    while ((status = nss_wifili_tx_msg(nss_contex, &wlmsg)) != NSS_TX_SUCCESS) {
        retrycnt++;
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                 "[nss-wifili]: wifi soc reset fail %d retrying %d", status, retrycnt);
        if (retrycnt >= OSIF_NSS_OL_MAX_RETRY_CNT) {
            QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                  "[nss-wifili]: wifi soc reset failed with max retry of %d", OSIF_NSS_OL_MAX_RETRY_CNT);
            error = QDF_STATUS_E_FAILURE;
            goto cleanup;
        }
    }

    /*
     * Blocking call, wait till we get ACK for this msg.
     *
     * NOTE: Return value is ignored and if we timedout we treat the operation as if
     * it was a success
     */
    (void)wait_for_completion_timeout(&soc->nss_soc.osif_nss_ol_wifi_cfg_complete.complete,
                                      msecs_to_jiffies(OSIF_NSS_WIFI_SOC_RESTART_TIMEOUT_MS));

    /*
     * ACK/NACK received from NSS FW
     *
     */
    if (NSS_TX_FAILURE == soc->nss_soc.osif_nss_ol_wifi_cfg_complete.response) {
         QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                      "[nss-wifili]: received NACK for soc reset");
    }

cleanup:
    /*
     * we should not care about the tx sent above as we should necessarily
     * clean up host memory.
     */
    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                  "nss-wifili: Free Host Pool Memory");
    nss_unregister_wifili_if(if_num);
    soc->nss_soc.nss_sctx = NULL;
    soc->nss_soc.nss_sifnum = -1;
    soc->nss_soc.nss_sidx = -1;
    return error;
}

static int osif_nss_wifili_init(ol_ath_soc_softc_t *soc)
{
    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_WARN,
                "[nss-wifili]:#2 init (nocode) ");
    return 0;
}

static void osif_nss_wifili_post_recv_buffer(ol_ath_soc_softc_t *soc)
{
    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_WARN,
                 "[nss-wifili]:#3 post recv (nocode) ");
}

static int osif_nss_wifili_ce_flush_raw_send(ol_ath_soc_softc_t *soc)
{
    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_WARN,
                  "[nss-wifili]:#4 ce_flush (nocode) ");
    return 0;
}

void osif_nss_wifili_set_cfg(ol_ath_soc_softc_t *soc, uint32_t wifili_cfg)
{

    cdp_soc_set_nss_cfg(wlan_psoc_get_dp_handle(soc->psoc_obj), wifili_cfg);
    soc->nss_soc.nss_scfg = wifili_cfg;
    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
              "[nss-wifili]:#4 cfg set %d", wifili_cfg);
}

static QDF_STATUS osif_nss_wifili_soc_start(ol_ath_soc_softc_t *soc)
{
    nss_tx_status_t status;
    uint32_t if_num = soc->nss_soc.nss_sifnum;
    struct nss_ctx_instance *nss_contex = soc->nss_soc.nss_sctx;
    nss_wifili_msg_callback_t msg_cb;
    int ret;

    if (!nss_contex) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                         "[nss-wifili]: %s, nss context is null", __FUNCTION__);
        return QDF_STATUS_E_FAILURE;
    }

    /*
     * TODO: Add if number sanity check
     */
    msg_cb = (nss_wifili_msg_callback_t)osif_nss_wifili_event_receive;
    osif_nss_wifili_init_completion_status(soc);

    nss_cmn_msg_init(&wlmsg.cm, if_num, NSS_WIFILI_START_MSG, 0, msg_cb, NULL);

    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                    "[nss-wifili]: send start message");

    status = nss_wifili_tx_msg(nss_contex, &wlmsg);
    if (status != NSS_TX_SUCCESS) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                   "[nss-wifili]: wifi soc start fail %d ", status);
        return QDF_STATUS_E_FAILURE;
    }

    /*
     * Blocking call, wait till we get ACK for this msg.
     */
    ret = wait_for_completion_timeout(&soc->nss_soc.osif_nss_ol_wifi_cfg_complete.complete, msecs_to_jiffies(OSIF_NSS_WIFI_SOC_START_TIMEOUT_MS));
    if (ret == 0) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                "[nss-wifili]: waiting for soc start timed out");
        return QDF_STATUS_E_FAILURE;
    }

    /*
     * ACK/NACK received from NSS FW
     */
    if (NSS_TX_FAILURE == soc->nss_soc.osif_nss_ol_wifi_cfg_complete.response) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                "[nss-wifili]: received NACK for soc wifiili start");
        return QDF_STATUS_E_FAILURE;
    }

    return QDF_STATUS_SUCCESS;
}

/*
 * osif_nss_wifili_pdev_detach
 *  de - allocate pdev
 */
void osif_nss_wifili_pdev_detach(struct ol_ath_softc_net80211 *scn, uint32_t *radio_id)
{
    struct nss_wifili_pdev_deinit_msg *deinit;
    nss_tx_status_t status;
    enum nss_dynamic_interface_type di_type;
    uint32_t if_num = -1;
    nss_wifili_msg_callback_t msg_cb;
    int ret = 0;

    if (!scn->nss_radio.nss_rctx) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                "[nss-wifili]: %s, nss core ctx is invalid ", __FUNCTION__);
        return;
    }

    /*
     * Send pdev de init message to NSS
     */
    memset(&wlmsg, 0, sizeof(struct nss_wifili_msg));

    deinit = &wlmsg.msg.pdevdeinit;
    deinit->ifnum = *radio_id;
    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
            "[nss-wifili]: de-Init Radio_id = %d", *radio_id);

    msg_cb = (nss_wifili_msg_callback_t)osif_nss_wifili_event_receive;
    osif_nss_wifili_init_completion_status(scn->soc);

    nss_cmn_msg_init(&wlmsg.cm, NSS_WIFILI_INTERFACE, NSS_WIFILI_PDEV_DEINIT_MSG,
            sizeof(struct nss_wifili_pdev_deinit_msg), msg_cb, NULL);

    status = nss_wifili_tx_msg(scn->nss_radio.nss_rctx, &wlmsg);
    if (status != NSS_TX_SUCCESS) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                "[nss-wifili]: wifi pdev de initialization fail%d ", status);
        return;
    }

    /*
     * Blocking call, wait till we get ACK for this msg.
     */
    ret = wait_for_completion_timeout(&scn->soc->nss_soc.osif_nss_ol_wifi_cfg_complete.complete, msecs_to_jiffies(OSIF_NSS_WIFI_SOC_START_TIMEOUT_MS));
    if (ret == 0) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                "[nss-wifili]: waiting for pdev detach timed out");
        return;
    }

    /*
     * ACK/NACK received from NSS FW
     */
    if (NSS_TX_FAILURE == scn->soc->nss_soc.osif_nss_ol_wifi_cfg_complete.response) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                "[nss-wifili]: received NACK for pdev detach");
        return;
    }


    di_type = NSS_DYNAMIC_INTERFACE_TYPE_WIFILI;

    if_num = scn->nss_radio.nss_rifnum;
    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
            "[nss-wifili]: dealloc Dynamic Pdev Node :%d of type:%d", if_num, di_type);

    if (nss_dynamic_interface_dealloc_node(if_num, di_type) != NSS_TX_SUCCESS) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                "[nss-wifili]: di returned error for destroy : %d", if_num);
    }

    nss_unregister_wifili_radio_if(if_num);
    scn->nss_radio.nss_rifnum = -1;
    scn->nss_radio.nss_rctx = NULL;
}

static QDF_STATUS nss_wifili_pdev_attach(struct ol_ath_softc_net80211 *scn, uint32_t  *radio_id)
{

    /* struct nss_wifili_msg wlmsg; */
    struct dp_pdev *pdev = (struct dp_pdev *)wlan_pdev_get_dp_handle(scn->sc_pdev);
    enum nss_dynamic_interface_type di_type;
    int if_num = -1;
    struct dp_soc *dpsoc = NULL;
    struct nss_wifili_pdev_init_msg *wlpdevinitmsg;
    nss_tx_status_t status;
    struct hal_mem_info hmi;
    int ret = 0;
    nss_wifili_msg_callback_t msg_cb;
    struct cdp_lro_hash_config lro_hash;

    if (!pdev) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                 "[nss-wifili]: %s pdev is invalid ", __FUNCTION__);
        return QDF_STATUS_E_FAILURE;
    }

    dpsoc = pdev->soc;
    if (!dpsoc) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                 "[nss-wifili]: %s, soc is invalid ", __FUNCTION__);
        return QDF_STATUS_E_FAILURE;
    }

    di_type = NSS_DYNAMIC_INTERFACE_TYPE_WIFILI;

    /*
     * Allocate a dynamic interface in NSS which represents the vdev
     */
    if_num = nss_dynamic_interface_alloc_node(di_type);
    if (if_num < 0) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                "[nss-wifili]: di returned error : %d", if_num);
        return QDF_STATUS_E_FAILURE;
    }

    scn->nss_radio.nss_rifnum = if_num;

    /*
     * Register pdev with nss driver
     */
    scn->nss_radio.nss_rctx = nss_register_wifili_radio_if((uint32_t)if_num, NULL, NULL, NULL, (struct net_device *)pdev, 0);
    if (!scn->nss_radio.nss_rctx) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                  "[nss-wifili]: unable to register with nss driver");
        goto fail1;
    }

    /*
     * Send pdev init message to NSS
     */
    memset(&wlmsg, 0, sizeof(struct nss_wifili_msg));

    wlpdevinitmsg = &wlmsg.msg.pdevmsg;
    wlpdevinitmsg->radio_id = *radio_id;
    wlpdevinitmsg->lmac_id = pdev->lmac_id;
#ifdef QCA_LOWMEM_CONFIG
    wlpdevinitmsg->num_rx_swdesc = WIFILI_NSS_NUM_RX_DESC;
#else
    wlpdevinitmsg->num_rx_swdesc = WIFILI_NSS_NUM_RX_DESC * WIFILI_NSS_RX_DESC_POOL_WEIGHT;
#endif

    hal_get_meminfo(dpsoc->hal_soc, &hmi);
    osif_nss_update_ring_info(dpsoc, &pdev->rx_refill_buf_ring, &wlpdevinitmsg->rxdma_ring, hmi.dev_base_paddr, hmi.dev_base_addr);

    /*
     * TODO: Modify the nss interface correctly
     */
    msg_cb = (nss_wifili_msg_callback_t)osif_nss_wifili_event_receive;
    osif_nss_wifili_init_completion_status(scn->soc);

    nss_cmn_msg_init(&wlmsg.cm, NSS_WIFILI_INTERFACE, NSS_WIFILI_PDEV_INIT_MSG,
            sizeof(struct nss_wifili_pdev_init_msg), msg_cb, NULL);


    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                    "[nss-wifili]: sent pdev init message to NSS radio_id = %d di if_num = %d",
                                                                 *radio_id, if_num);

    status = nss_wifili_tx_msg(scn->nss_radio.nss_rctx, &wlmsg);
    if (status != NSS_TX_SUCCESS) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                "[nss-wifili]: wifi pdev initialization fail%d ", status);
        goto fail2;
    }

    /*
     * Blocking call, wait till we get ACK for this msg.
     */
    ret = wait_for_completion_timeout(&scn->soc->nss_soc.osif_nss_ol_wifi_cfg_complete.complete,
                                          msecs_to_jiffies(OSIF_NSS_WIFI_PDEV_INIT_TIMEOUT_MS));
    if (ret == 0) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                  "[nss-wifili]: waiting for pdev init timed out");
        goto fail2;
    }

    /*
     * ACK/NACK received from NSS FW
     */
    if (NSS_TX_FAILURE == scn->soc->nss_soc.osif_nss_ol_wifi_cfg_complete.response) {
         QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
               "[nss-wifili]: received NACK for soc wifiili pdev-init from nss");
         goto fail2;
    }

    /*
     * setting seed value for toeplitz hash
     */
    qdf_mem_zero(&lro_hash, sizeof(lro_hash));
    qdf_get_random_bytes(lro_hash.toeplitz_hash_ipv4,
            (sizeof(lro_hash.toeplitz_hash_ipv4[0]) *
            LRO_IPV4_SEED_ARR_SZ));
    qdf_get_random_bytes(lro_hash.toeplitz_hash_ipv6,
            (sizeof(lro_hash.toeplitz_hash_ipv6[0]) *
            LRO_IPV6_SEED_ARR_SZ));

    if (dpsoc->cdp_soc.ol_ops->lro_hash_config)
        (void)dpsoc->cdp_soc.ol_ops->lro_hash_config
            (pdev->ctrl_pdev, &lro_hash);

    return QDF_STATUS_SUCCESS;

    /*
     * unregister the pdev interface
     */
fail2:
    nss_unregister_wifili_radio_if(if_num);
    scn->nss_radio.nss_rctx = NULL;

    /*
     * De allocate the dynamic interface
     */
fail1:
    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
              "[nss-wifili]: dealloc Dynamic Pdev Node :%d of type:%d", if_num, di_type);

    scn->nss_radio.nss_rifnum = -1;
    if (nss_dynamic_interface_dealloc_node(if_num, di_type) != NSS_TX_SUCCESS) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                "[nss-wifili]: di returned error for destroy : %d", if_num);
	}
    return QDF_STATUS_E_FAILURE;
}

/*
 * osif_nss_wifili_peer_stats
 *	Process the enhanced stats message from NSS-FW
 *
 * NOTE: Always return success
 */
int osif_nss_wifili_peer_stats(ol_ath_soc_softc_t *soc, struct nss_wifili_peer_stats *stats)
{
    struct dp_peer *peer = NULL;
    struct nss_wifili_peer_ctrl_stats *pstats = NULL;
    uint32_t i, k;
    struct dp_soc *dpsoc = (struct dp_soc *)wlan_psoc_get_dp_handle(soc->psoc_obj);
    uint32_t ring_id;

    for(i = 0; i < stats->npeers; i++) {
        pstats = &stats->wpcs[i];

        peer = dp_peer_find_by_id(dpsoc, pstats->peer_id);
        if (peer) {

            /* In offload mode, pdev_id is mapped one to one to REO ring id */
            ring_id = peer->vdev->pdev->pdev_id;

            /* Tx Dropped stats */
            DP_STATS_INC(peer, tx.ofdma, pstats->tx.ofdma);
            DP_STATS_INC(peer, tx.dropped.fw_rem.num, pstats->tx.dropped.drop_stats[HAL_TX_TQM_RR_REM_CMD_REM - 1]);
            DP_STATS_INC(peer, tx.dropped.fw_rem_tx, pstats->tx.dropped.drop_stats[HAL_TX_TQM_RR_REM_CMD_TX - 1]);
            DP_STATS_INC(peer, tx.dropped.fw_rem_notx, pstats->tx.dropped.drop_stats[HAL_TX_TQM_RR_REM_CMD_NOTX - 1]);
            DP_STATS_INC(peer, tx.dropped.age_out, pstats->tx.dropped.drop_stats[HAL_TX_TQM_RR_REM_CMD_AGED - 1]);

            DP_STATS_INC(peer, tx.amsdu_cnt, pstats->tx.amsdu_cnt);
            DP_STATS_INC(peer, tx.non_amsdu_cnt, pstats->tx.non_amsdu_cnt);

	    DP_STATS_INCC(peer, tx.retries, 1, pstats->tx.transmit_cnt > 1);

            DP_STATS_INC_PKT(peer, tx.tx_success, pstats->tx.tx_success_cnt,
                                                  pstats->tx.tx_success_bytes);
            DP_STATS_INC_PKT(peer, tx.mcast, pstats->tx.tx_mcast_cnt,
                                                  pstats->tx.tx_mcast_bytes);
            DP_STATS_INC_PKT(peer, tx.bcast, pstats->tx.tx_bcast_cnt,
                                                  pstats->tx.tx_bcast_bytes);
            DP_STATS_INC_PKT(peer, tx.ucast, pstats->tx.tx_ucast_cnt,
                                                  pstats->tx.tx_ucast_bytes);
            DP_STATS_INC_PKT(peer, tx.nawds_mcast, pstats->tx.tx_nawds_mcast_cnt,
                    pstats->tx.tx_nawds_mcast_bytes);
            DP_STATS_INC(peer, tx.nawds_mcast_drop, pstats->tx.dropped.tx_nawds_mcast_drop_cnt);

            /* Peer rx stats*/
            DP_STATS_INC(peer, rx.amsdu_cnt, pstats->rx.amsdu_cnt);
            DP_STATS_INC(peer, rx.non_amsdu_cnt, pstats->rx.non_amsdu_cnt);
            DP_STATS_INC(peer, rx.err.decrypt_err, pstats->rx.err.decrypt_err);
            DP_STATS_INC(peer, rx.err.mic_err, pstats->rx.err.mic_err);

            DP_STATS_INC_PKT(peer, rx.to_stack, pstats->rx.rx_recvd,
                                                pstats->rx.rx_recvd_bytes);
            DP_STATS_INC_PKT(peer, rx.multicast, pstats->rx.mcast_rcv_cnt,
                                                pstats->rx.mcast_rcv_bytes);
            DP_STATS_INC_PKT(peer, rx.bcast, pstats->rx.bcast_rcv_cnt,
                                                pstats->rx.bcast_rcv_bytes);
            DP_STATS_INC(peer, rx.nawds_mcast_drop, pstats->rx.nawds_mcast_drop);

            DP_STATS_INC_PKT(peer, rx.rcvd_reo[ring_id], pstats->rx.rx_recvd,
                                                pstats->rx.rx_recvd_bytes);
            DP_STATS_INC_PKT(peer, rx.intra_bss.pkts, pstats->rx.rx_intra_bss_pkts_num,
                    pstats->rx.rx_intra_bss_pkts_bytes);
            DP_STATS_INC_PKT(peer, rx.intra_bss.fail, pstats->rx.rx_intra_bss_fail_num,
                    pstats->rx.rx_intra_bss_fail_bytes);
            for (k = 0; k < MAX_RECEPTION_TYPES; k++) {
               DP_STATS_INC(peer, rx.reception_type[k], pstats->rx.reception_type[k]);
            }

            if (peer->vdev->rx_decap_type == htt_cmn_pkt_type_raw) {
                 DP_STATS_INC_PKT(peer, rx.raw, pstats->rx.rx_recvd,
                                                    pstats->rx.rx_recvd_bytes);
            }
        }
    }
    return 0;
}

/*
 * osif_nss_wifili_update_pdev_v3_tx_rx_stats()
 *	Process V3 statistics from NSS
 */
static int osif_nss_wifili_update_pdev_v3_tx_rx_stats(ol_ath_soc_softc_t *soc, struct nss_wifili_pdev_v3_tx_rx_stats_sync_msg *statsmsg)
{
    struct dp_pdev *pdev = NULL;
    struct ol_ath_softc_net80211 *scn = NULL;
    uint8_t radio_id, tid;
    struct nss_ctx_instance *nss_ctx = NULL;
    struct wlan_objmgr_pdev *pdev_obj;
    struct nss_wifili_radio_tx_rx_stats_v3 *stats_v3;
    struct cdp_tid_tx_stats *txstats = NULL;
    struct cdp_tid_rx_stats *rxstats = NULL;
    struct nss_wifili_v3_tx_rx_per_tid_stats *tidstats;
    struct nss_wifili_v3_tx_rx_per_ac_stats *acstats;
    struct dp_soc *dpsoc = (struct dp_soc *)wlan_psoc_get_dp_handle(soc->psoc_obj);

    if (!dpsoc) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                "[nss-wifili] %p: Could not update stats, DP handle is NULL", soc);
        return 0;
    }

    radio_id = statsmsg->radio_id;
    pdev_obj = wlan_objmgr_get_pdev_by_id(soc->psoc_obj, radio_id,
            WLAN_MLME_NB_ID);
    if (pdev_obj == NULL) {
        return 0;
    }

    scn = lmac_get_pdev_feature_ptr(pdev_obj);
    if (!scn) {
        wlan_objmgr_pdev_release_ref(pdev_obj, WLAN_MLME_NB_ID);
        return 0;
    }

    nss_ctx = (struct nss_ctx_instance *)scn->nss_radio.nss_rctx;

    pdev = (struct dp_pdev *)wlan_pdev_get_dp_handle(pdev_obj);
    if (!pdev) {
        wlan_objmgr_pdev_release_ref(pdev_obj, WLAN_MLME_NB_ID);
        return 0;
    }

    /*
     * update VoW Tx/Rx per TID statistics
     */
    stats_v3 = &statsmsg->wlpv3_txrx_stats;

    for (tid = 0; tid < NSS_WIFILI_MAX_TID; tid++) {

        txstats = &pdev->stats.tid_stats.tid_tx_stats[tid];
        rxstats = &pdev->stats.tid_stats.tid_rx_stats[tid];
        tidstats = &stats_v3->tid_stats[tid];
        acstats = &stats_v3->ac_stats[TID_TO_WME_AC(tid)];

        /*
         * Update Tx Per TID stats
         */
        txstats->success_cnt += tidstats->transmit_succes_cnt;
        txstats->complete_cnt += tidstats->transmit_complete_cnt;
        txstats->comp_fail_cnt += tidstats->transmit_fwdrop_cnt;
        txstats->swdrop_cnt[TX_DESC_ERR] += tidstats->transmit_desc_fail_cnt;
        txstats->swdrop_cnt[TX_HW_ENQUEUE] += tidstats->transmit_hwdrop_cnt;
        txstats->swdrop_cnt[TX_SW_ENQUEUE] += tidstats->radio_ingress_enq_drop_cnt;

        /*
         * Update Rx Per TID stats
         */
        rxstats->delivered_to_stack += tidstats->rx_delivered_cnt;
        rxstats->intrabss_cnt += tidstats->rx_intrabss_cnt;
        rxstats->msdu_cnt += tidstats->num_msdu_recived;
        rxstats->mcast_msdu_cnt += tidstats->num_mcast_msdu_recived;
        rxstats->bcast_msdu_cnt += tidstats->num_bcast_msdu_recived;
        rxstats->fail_cnt[INTRABSS_DROP] += tidstats->rx_intrabss_fail_cnt;
        rxstats->fail_cnt[ENQUEUE_DROP] += tidstats->rx_deliver_fail_cnt;
    }

    wlan_objmgr_pdev_release_ref(pdev_obj, WLAN_MLME_NB_ID);

    return 0;
}

/*
 * osif_nss_wifili_update_pdev_v3_delay_stats()
 *	Process V3 statistics from NSS
 */
static int osif_nss_wifili_update_pdev_v3_delay_stats(ol_ath_soc_softc_t *soc, struct nss_wifili_pdev_v3_delay_stats_sync_msg *statsmsg)
{
    struct dp_pdev *pdev = NULL;
    struct ol_ath_softc_net80211 *scn = NULL;
    uint8_t radio_id, tid, index;
    struct nss_ctx_instance *nss_ctx = NULL;
    struct cdp_tid_tx_stats *txstats = NULL;
    struct cdp_tid_rx_stats *rxstats = NULL;
    struct wlan_objmgr_pdev *pdev_obj;
    struct nss_wifili_radio_delay_stats_v3 *stats_v3;
    struct nss_wifili_v3_delay_per_tid_stats *dstats;
    struct dp_soc *dpsoc = (struct dp_soc *)wlan_psoc_get_dp_handle(soc->psoc_obj);

    if (!dpsoc) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                "[nss-wifili] %p: Could not update stats, DP handle is NULL", soc);
        return 0;
    }

    radio_id = statsmsg->radio_id;
    pdev_obj = wlan_objmgr_get_pdev_by_id(soc->psoc_obj, radio_id,
            WLAN_MLME_NB_ID);
    if (pdev_obj == NULL) {
        return 0;
    }

    scn = lmac_get_pdev_feature_ptr(pdev_obj);
    if (!scn) {
        wlan_objmgr_pdev_release_ref(pdev_obj, WLAN_MLME_NB_ID);
        return 0;
    }

    nss_ctx = (struct nss_ctx_instance *)scn->nss_radio.nss_rctx;

    pdev = (struct dp_pdev *)wlan_pdev_get_dp_handle(pdev_obj);
    if (!pdev) {
        wlan_objmgr_pdev_release_ref(pdev_obj, WLAN_MLME_NB_ID);
        return 0;
    }

    /*
     * Update VoW Delay statistics
     */
    stats_v3 = &statsmsg->wlpv3_delay_stats;

    for (tid = 0; tid < NSS_WIFILI_MAX_TID; tid++) {

        txstats = &pdev->stats.tid_stats.tid_tx_stats[tid];
        rxstats = &pdev->stats.tid_stats.tid_rx_stats[tid];
        dstats = &stats_v3->v3_delay_stats[tid];

        /*
         * Update delay buckets
         */
        for (index = 0; index < NSS_WIFILI_DELAY_INDEX_MAX; index++) {
            txstats->swq_delay.delay_bucket[index] += dstats->swq_delay.delay_bucket[index];
            txstats->hwtx_delay.delay_bucket[index] += dstats->hwtx_delay.delay_bucket[index];
            txstats->intfrm_delay.delay_bucket[index] += dstats->tx_intfrm_delay.delay_bucket[index];
            rxstats->intfrm_delay.delay_bucket[index] += dstats->rx_intfrm_delay.delay_bucket[index];
        }

        /*
         * Update minimum, average and maximum delays
         */
        txstats->swq_delay.min_delay = dstats->swq_delay.min_delay;
        txstats->swq_delay.avg_delay = dstats->swq_delay.avg_delay;
        txstats->swq_delay.max_delay = dstats->swq_delay.max_delay;

        txstats->hwtx_delay.min_delay = dstats->hwtx_delay.min_delay;
        txstats->hwtx_delay.avg_delay = dstats->hwtx_delay.avg_delay;
        txstats->hwtx_delay.max_delay = dstats->hwtx_delay.max_delay;

        txstats->intfrm_delay.min_delay = dstats->tx_intfrm_delay.min_delay;
        txstats->intfrm_delay.avg_delay = dstats->tx_intfrm_delay.avg_delay;
        txstats->intfrm_delay.max_delay = dstats->tx_intfrm_delay.max_delay;

        rxstats->intfrm_delay.min_delay = dstats->rx_intfrm_delay.min_delay;
        rxstats->intfrm_delay.avg_delay = dstats->rx_intfrm_delay.avg_delay;
        rxstats->intfrm_delay.max_delay = dstats->rx_intfrm_delay.max_delay;
    }

    wlan_objmgr_pdev_release_ref(pdev_obj, WLAN_MLME_NB_ID);

    return 0;
}

/*
 * osif_nss_wifili_sojourn_peer_stats
 *	Process the sojourn statistics message from NSS-FW
 */
void osif_nss_wifili_sojourn_peer_stats(ol_ath_soc_softc_t *soc, uint32_t npeers,
                                        struct nss_wifili_sojourn_peer_stats *sjp_stats)
{
    uint32_t peer_idx, tid_idx;
    struct cdp_tx_sojourn_stats *cdp_sj_stats;
    struct cdp_tx_sojourn_stats cdp_sj_peer;
    struct dp_peer *peer = NULL;
    struct dp_pdev *pdev = NULL;
    struct dp_soc *dpsoc = (struct dp_soc *)wlan_psoc_get_dp_handle(soc->psoc_obj);
    if (!dpsoc) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                "[nss-wifili]: DP Soc handler is null");
        return;
    }

    /*
     * Loop through the peers and copy the stats per tid.
     */
    for (peer_idx = 0; peer_idx < npeers; peer_idx++) {

        peer = dp_peer_find_by_id(dpsoc, (&sjp_stats[peer_idx])->peer_id);
        if (!peer) {
            QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_WARN,
                    "[nss-wifili]: wifili peer is invalid");
            continue;
        }

        if (!peer->wlanstats_ctx) {
            QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_WARN,
                    "[nss-wifili]: wifili peer wlanstats ctx is invalid");
            continue;
        }

        pdev = peer->vdev->pdev;
        if (!pdev) {
            QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_WARN,
                    "[nss-wifili]: wifili pdev is invalid");
            continue;
        }

        if (!pdev->sojourn_buf) {
            return;
        }

        cdp_sj_stats = (struct cdp_tx_sojourn_stats *)qdf_nbuf_data(pdev->sojourn_buf);

        /*
         * Populate stats per peer per tid.
         */
        for (tid_idx = 0; tid_idx < CDP_DATA_TID_MAX; tid_idx++) {
            qdf_ewma_tx_lag_add(&cdp_sj_peer.avg_sojourn_msdu[tid_idx],
                    (&sjp_stats[peer_idx])->stats[tid_idx].avg_sojourn_msdu);
            cdp_sj_peer.sum_sojourn_msdu[tid_idx] = (&sjp_stats[peer_idx])->stats[tid_idx].sum_sojourn_msdu;
            cdp_sj_peer.num_msdus[tid_idx] = (&sjp_stats[peer_idx])->stats[tid_idx].num_msdus;
        }

        cdp_sj_peer.cookie = (void *)peer->wlanstats_ctx;

        qdf_mem_copy(cdp_sj_stats, &cdp_sj_peer, sizeof(struct cdp_tx_sojourn_stats));

        qdf_mem_zero(&cdp_sj_peer, sizeof(struct cdp_tx_sojourn_stats));
        dp_wdi_event_handler(WDI_EVENT_TX_SOJOURN_STAT, pdev->soc,
                pdev->sojourn_buf, HTT_INVALID_PEER,
                WDI_NO_VAL, pdev->pdev_id);
    }
}

/*
 * osif_nss_wifili_update_wds_activeinfo()
 *
 */
static int osif_nss_wifili_update_wds_activeinfo(ol_ath_soc_softc_t *soc, struct nss_wifili_wds_active_info_msg *msg)
{
    struct dp_soc *dpsoc = NULL;
    uint32_t max_ast_index;
    uint16_t i, sa_idx, nentries = msg->nentries;
    struct nss_wifili_wds_active_info *info = msg->info;

    dpsoc = (struct dp_soc *)wlan_psoc_get_dp_handle(soc->psoc_obj);
    if (!dpsoc) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                 "[nss-wifili]: DP Soc handler is null");
        return 0;
    }

    max_ast_index = wlan_cfg_get_max_ast_idx(dpsoc->wlan_cfg_ctx);
    for (i = 0; i < nentries; i++) {

        sa_idx = info[i].ast_idx;

        if (sa_idx >= max_ast_index) {
            continue;
        }

        if (dp_rx_ast_set_active(dpsoc, sa_idx, true)) {
            QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                   "[nss-wifili]: Could not set astenty active for hw_idx = %u", sa_idx);
            continue;
        }
    }

    return 0;
}

/*
 * osif_nss_wifili_ol_stats
 *	Offload stats from NSS
 */
static void osif_nss_wifili_ol_stats(ol_ath_soc_softc_t *soc, struct nss_wifili_device_stats *olstats) {
    struct dp_pdev *pdev = NULL;
    struct ol_ath_softc_net80211 *scn = NULL;
    uint8_t i;
    struct nss_ctx_instance *nss_ctx = NULL;
    struct wlan_objmgr_pdev *pdev_obj;
    struct dp_soc *dpsoc = (struct dp_soc *)wlan_psoc_get_dp_handle(soc->psoc_obj);

    if (!dpsoc) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
              "[nss-wifili] %p: Could not update stats, DP handle is NULL", soc);
        return;
    }

    /* Update the per radio offload stats */
    for (i = 0; i < MAX_RADIOS; i++) {
        pdev_obj = wlan_objmgr_get_pdev_by_id(soc->psoc_obj, i,
                                       WLAN_MLME_NB_ID);
        if (pdev_obj == NULL) {
             continue;
        }

        scn = lmac_get_pdev_feature_ptr(pdev_obj);
        if (!scn) {
            wlan_objmgr_pdev_release_ref(pdev_obj, WLAN_MLME_NB_ID);
            continue;
        }

        nss_ctx = (struct nss_ctx_instance *)scn->nss_radio.nss_rctx;

        pdev = (struct dp_pdev *)wlan_pdev_get_dp_handle(pdev_obj);
        if (!pdev) {
            wlan_objmgr_pdev_release_ref(pdev_obj, WLAN_MLME_NB_ID);
            continue;
        }

        DP_STATS_INC_PKT(pdev, replenish.pkts, olstats->rxdma_stats[i].rx_buf_replenished,
                            (RX_BUFFER_SIZE * olstats->rxdma_stats[i].rx_buf_replenished));

        pdev->stats.replenish.nbuf_alloc_fail += olstats->rx_sw_pool_stats[i].rx_no_pb;
        pdev->stats.err.desc_alloc_fail += olstats->rx_sw_pool_stats[i].desc_alloc_fail;

               wlan_objmgr_pdev_release_ref(pdev_obj, WLAN_MLME_NB_ID);
    }

    /*
     * Update the REO error codes
     */
    for (i = 0; i < NSS_WIFILI_REO_CODE_MAX; i++) {
        DP_STATS_INC(dpsoc, rx.err.reo_error[i], olstats->rxwbm_stats.err_reo_codes[i]);
    }

    /*
     * Update the DMA error codes
     */
    for (i = 0; i < NSS_WIFILI_DMA_CODE_MAX; i++) {
        DP_STATS_INC(dpsoc, rx.err.rxdma_error[i], olstats->rxwbm_stats.err_dma_codes[i]);
    }
}

static void osif_nss_wifili_link_desc_return(ol_ath_soc_softc_t *soc, struct nss_wifili_soc_linkdesc_buf_info_msg  *linkdesc)
{
    struct dp_soc *dpsoc = NULL;

    dpsoc = (struct dp_soc *)wlan_psoc_get_dp_handle(soc->psoc_obj);
    if (!dpsoc) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
              "[nss-wifili] %pK: Dp handler is NULL", soc);
        return;
    }

    /*
     * linkdesc will contain the buff_addr_info
     */
    dp_rx_link_desc_return_by_addr(dpsoc, (void *)linkdesc,
                                    HAL_BM_ACTION_PUT_IN_IDLE_LIST);
    return;

}

/*
 * osif_nss_wifili_reo_tidq_setup()
 */
void osif_nss_wifili_reo_tidq_setup(ol_ath_soc_softc_t *soc, struct nss_wifili_reo_tidq_msg *msg)
{
    struct dp_soc *dpsoc = NULL;
    struct dp_peer *peer = NULL;
    uint16_t peer_id;
    uint32_t tid;

    dpsoc = (struct dp_soc *)wlan_psoc_get_dp_handle(soc->psoc_obj);
    if (!dpsoc) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                "[nss-wifili]: DP Soc handler is null");
        return;
    }

    peer_id = msg->peer_id;
    peer = dp_peer_find_by_id(dpsoc, peer_id);
    if (!peer) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_WARN,
                "[nss-wifili]: wifili peer is invalid");
        return;
    }

    tid = msg->tid;

    if (peer->rx_tid[tid].hw_qdesc_vaddr_unaligned == NULL) {
        /* IEEE80211_SEQ_MAX indicates invalid start_seq */
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                "[nss-wifili]: wifili TIDQ setup message received = %u", tid);
        dp_rx_tid_setup_wifi3(peer, tid, 1, IEEE80211_SEQ_MAX);
    }
}

/*
 * osif_nss_wifili_event_receive
 *       wifili event callback
 */
void osif_nss_wifili_event_receive(ol_ath_soc_softc_t *soc, struct nss_wifili_msg *ntm)
{

    uint32_t msg_type = ntm->cm.type;
    enum nss_cmn_response response = ntm->cm.response;
    uint32_t error =  ntm->cm.error;

    if (!soc) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                 "[nss-wifili]: soc %pK is NULL", soc);
        return;
    }

    /*
     * Handle the nss wifili message
     */
    switch (msg_type) {
        case NSS_WIFILI_PEER_STATS_MSG:
            {
                struct nss_wifili_peer_stats *stats = &ntm->msg.peer_stats.stats;
                if (response == NSS_CMN_RESPONSE_EMSG) {
                    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                              "[nss-wifili]: peer stats msg failed with error = %u", error);
                    return;
                }
                osif_nss_wifili_peer_stats(soc, stats);
            }
            break;
         case NSS_WIFILI_WDS_ACTIVE_INFO_MSG:
             {
                 struct nss_wifili_wds_active_info_msg *msg = &ntm->msg.wdsinfomsg;
                 if (response == NSS_CMN_RESPONSE_EMSG) {
                     QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                              "[nss-wifili]: wds active info msg failed with error %u", error);
                     return;
                 }
                 osif_nss_wifili_update_wds_activeinfo(soc, msg);
             }
             break;
        case NSS_WIFILI_INIT_MSG:
            {
                if (response == NSS_CMN_RESPONSE_EMSG) {
                    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                             "[nss-wifili]: init configuration message failed with error = %u", error);
                    osif_nss_wifili_set_completion_status(soc, NSS_TX_FAILURE);
                    return;
                }

                osif_nss_wifili_set_completion_status(soc, NSS_TX_SUCCESS);
                QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                       "[nss-wifili]: soc initialization is successfull");
            }
            break;
        case NSS_WIFILI_STOP_MSG:
            {
                if (response == NSS_CMN_RESPONSE_EMSG) {
                    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                             "[nss-wifili]: stop configuration message failed with error = %u", error);
                    osif_nss_wifili_set_completion_status(soc, NSS_TX_FAILURE);
                    return;
                }

                osif_nss_wifili_set_completion_status(soc, NSS_TX_SUCCESS);
                QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                              "[nss-wifili]: ACK for stop configuration message received");
            }
            break;
        case NSS_WIFILI_SOC_RESET_MSG:
            {
                if (response == NSS_CMN_RESPONSE_EMSG) {
                    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                            "[nss-wifili]: reset configuration message failed with error = %u", error);
                    osif_nss_wifili_set_completion_status(soc, NSS_TX_FAILURE);
                    return;
                }

                osif_nss_wifili_set_completion_status(soc, NSS_TX_SUCCESS);
                QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                            "[nss-wifili]: reset configuration is successful");
            }
            break;
        case NSS_WIFILI_PDEV_DEINIT_MSG:
            {
                if (response == NSS_CMN_RESPONSE_EMSG) {
                    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                           "[nss-wifili]: pdev de-init message failed with error = %u", error);
                    osif_nss_wifili_set_completion_status(soc, NSS_TX_FAILURE);
                    return;
                }

                osif_nss_wifili_set_completion_status(soc, NSS_TX_SUCCESS);
                QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                           "[nss-wifili]: pdev de-init configuration is successful");
            }
            break;
        case NSS_WIFILI_PDEV_INIT_MSG:
            {
                if (response == NSS_CMN_RESPONSE_EMSG) {
                    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                              "[nss-wifili]: pdev init configuration message failed with error = %u", error);
                    osif_nss_wifili_set_completion_status(soc, NSS_TX_FAILURE);
                    return;
                }

                osif_nss_wifili_set_completion_status(soc, NSS_TX_SUCCESS);
                QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                              "[nss-wifili]: pdev init configuration message successful");
            }
            break;
        case NSS_WIFILI_STATS_CFG_MSG:
            if (response == NSS_CMN_RESPONSE_EMSG) {
                QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                            "[nss-wifili]: NSS STATS configuration message failed with error = %u", error);
            }
            break;
        case NSS_WIFILI_START_MSG:
            {
                if (response == NSS_CMN_RESPONSE_EMSG) {
                    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                                 "[nss-wifili]: start message failed with error = %u", error);
                    osif_nss_wifili_set_completion_status(soc, NSS_TX_FAILURE);
                    return;
                }

                osif_nss_wifili_set_completion_status(soc, NSS_TX_SUCCESS);
                QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                           "[nss-wifili]: send start messgae is successfull");
            }
            break;
        case NSS_WIFILI_PEER_CREATE_MSG:
            if (response == NSS_CMN_RESPONSE_EMSG) {
                QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                        "[nss-wifili]: NSS wifili peer create configuration message failed with error = %u", error);
            }
            break;
        case NSS_WIFILI_PEER_DELETE_MSG:
            {
                uint32_t peerid;
                if (response == NSS_CMN_RESPONSE_EMSG) {
                    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                            "[nss-wifili]:NSS Wifili peer delete configuration message failed with error = %u", error);
                    return;
                }

                peerid = (&ntm->msg.peermsg)->peer_id;
                osif_nss_wifili_return_peer_mem(&soc->nss_soc.nwpmem, peerid);
            }
            break;
        case NSS_WIFILI_WDS_PEER_ADD_MSG:
            if (response == NSS_CMN_RESPONSE_EMSG) {
                QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                        "[nss-wifili]: NSS Wifili peer wds add message failed with error = %u", error);
            }
            break;
        case NSS_WIFILI_WDS_PEER_DEL_MSG:
            if (response == NSS_CMN_RESPONSE_EMSG) {
                QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                        "[nss-wifili]: NSS Wifili peer wds delete message failed with error = %u", error);
            }
            break;
        case NSS_WIFILI_WDS_PEER_MAP_MSG:
            if (response == NSS_CMN_RESPONSE_EMSG) {
                QDF_TRACE_ERROR_RL(QDF_MODULE_ID_NSS,
                        "[nss-wifili]: NSS Wifili peer wds peer map message failed with error = %u", error);
            }
            break;
        case NSS_WIFILI_STATS_MSG:
            {
                /* Update the pdev ol stats */
                struct nss_wifili_device_stats *olstats = &ntm->msg.wlsoc_stats.stats;
                if (response == NSS_CMN_RESPONSE_EMSG) {
                    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                            "[nss-wifili]: wifili stats message recieved with error = %u", error);
                    return;
                }
                osif_nss_wifili_ol_stats(soc, olstats);
            }
            break;
        case NSS_WIFILI_TID_REOQ_SETUP_MSG:
            {
                struct nss_wifili_reo_tidq_msg *msg = &ntm->msg.reotidqmsg;
                QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                        "[nss-wifili]: wifili tidq setup message received");
                osif_nss_wifili_reo_tidq_setup(soc, msg);
            }
            break;

        case NSS_WIFILI_LINK_DESC_INFO_MSG:
            {
                 struct nss_wifili_soc_linkdesc_buf_info_msg *linkdescinfomsg = &ntm->msg.linkdescinfomsg;
                 QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                         "[nss-wifili]: wifili msdu link descriptor info received");
                 osif_nss_wifili_link_desc_return(soc, linkdescinfomsg);
            }
            break;

        case NSS_WIFILI_PEER_SECURITY_TYPE_MSG:
            {
                if (response == NSS_CMN_RESPONSE_EMSG) {
                    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                                 "[nss-wifili]: peer security message failed error = %u", error);
                    return;
                }

                QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                           "[nss-wifili]:peer security message is successfull");
            }
            break;
        case NSS_WIFILI_DBDC_REPEATER_SET_MSG:
            if (response == NSS_CMN_RESPONSE_EMSG) {
                QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                        "[nss-wifili]: NSS Wifili DBDC Repeater enable message failed with error = %u", error);
            }
                QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                        "[nss-wifili]: NSS Wifili DBDC Repeater enable message passed");
            break;
        case NSS_DBDC_REPEATER_AST_FLUSH_MSG:
            {
                /*
                 * AST Flush message from NSS FW after DBDC loop
                 * is detected.
                 */
                ol_txrx_soc_handle soc_txrx_handle = NULL;
                if (response == NSS_CMN_RESPONSE_EMSG) {
                    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                            "[nss-wifili]: wifili ast flush message recieved with error = %u", error);
                    return;
                }

                soc_txrx_handle = wlan_psoc_get_dp_handle(soc->psoc_obj);
                if (!soc_txrx_handle) {
                    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                            "[nss-wifili]: wifili ast flush message - Null soc handle = %u", error);
                    return;
                }

                qdf_print("\n **** Received AST Flush Event from NSS FW ****");
                cdp_peer_flush_ast_table(soc_txrx_handle);
            }
            break;
        case NSS_WIFILI_SET_HMMC_DSCP_TID_MSG:
            {
                if (response == NSS_CMN_RESPONSE_EMSG) {
                    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                            "[nss-wifili]: NSS Wifili setting hmmc dscp tid failed");
                }

                QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                        "[nss-wifili]: NSS Wifili setting hmmc dscp tid passed");
            }
            break;
        case NSS_WIFILI_SET_HMMC_DSCP_OVERRIDE_MSG:
            {
                if (response == NSS_CMN_RESPONSE_EMSG) {
                    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                            "[nss-wifili]: NSS Wifili setting hmmc dscp override failed");
                }

                QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                        "[nss-wifili]: NSS Wifili setting hmmc dscp override passed");
            }
        case NSS_WIFILI_PDEV_STATS_V3_TXRX_SYNC_MSG:
            {
                struct nss_wifili_pdev_v3_tx_rx_stats_sync_msg *statsmsg = &ntm->msg.v3_txrx_stats_msg;
                if (response == NSS_CMN_RESPONSE_EMSG) {
                    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                              "[nss-wifili]: pdev v3 tx/rx stats sync msg failed with error = %u", error);
                    return;
                }
                osif_nss_wifili_update_pdev_v3_tx_rx_stats(soc, statsmsg);
            }
            break;
        case NSS_WIFILI_PDEV_STATS_V3_DELAY_SYNC_MSG:
            {
                struct nss_wifili_pdev_v3_delay_stats_sync_msg *statsmsg = &ntm->msg.v3_delay_stats_msg;
                if (response == NSS_CMN_RESPONSE_EMSG) {
                    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                              "[nss-wifili]: pdev v3 delay stats sync msg failed with error = %u", error);
                    return;
                }
                osif_nss_wifili_update_pdev_v3_delay_stats(soc, statsmsg);
            }
            break;
        case NSS_WIFILI_ENABLE_V3_STATS_MSG:
            if (response == NSS_CMN_RESPONSE_EMSG) {
                QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                        "[nss-wifili]: NSS V3 STATS configuration message failed with error = %u", error);
            }
            break;
        case NSS_WIFILI_STATS_V2_CFG_MSG:
            {
                if (response == NSS_CMN_RESPONSE_EMSG) {
                    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                            "[nss-wifili]: sojourn peer stats msg failed with error = %u", error);
                    return;
                }
                QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                        "[nss-wifili]: NSS Wifili enable stats v2 passed");
            }
            break;
        case NSS_WIFILI_SOJOURN_STATS_MSG:
            {
                struct nss_wifili_sojourn_stats_msg *stats_msg = &ntm->msg.sj_stats_msg;
                if (response == NSS_CMN_RESPONSE_EMSG) {
                    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                            "[nss-wifili]: sojourn peer stats msg failed with error = %u", error);
                    return;
                }
                osif_nss_wifili_sojourn_peer_stats(soc, stats_msg->npeers, &stats_msg->sj_peer_stats[0]);
            }
            break;
        default:
            break;
    }
    return;
}

/*
 * osif_nss_wifili_frag_to_chain()
 */
void osif_nss_wifili_frag_to_chain(qdf_nbuf_t nbuf, qdf_nbuf_t *list_head, qdf_nbuf_t *list_tail)
{
    qdf_nbuf_t deliver_list_head = NULL;
    qdf_nbuf_t deliver_list_tail = NULL;
    qdf_nbuf_t tmp;

    /*
     * Check if SKB has fraglist and convert the fraglist to
     * skb->next chains
     */
    memset(nbuf->cb, 0x0, sizeof(nbuf->cb));
    OSIF_TXRX_LIST_APPEND(deliver_list_head, deliver_list_tail, nbuf);
    qdf_nbuf_set_rx_chfrag_start(nbuf, 1);
    if (skb_has_frag_list(nbuf)) {
        nbuf->len  =  qdf_nbuf_headlen(nbuf);
        nbuf->truesize -= nbuf->data_len;
        nbuf->data_len = 0;
        nbuf->next = qdf_nbuf_get_ext_list(nbuf);

        skb_walk_frags(nbuf, tmp) {
            OSIF_TXRX_LIST_APPEND(deliver_list_head, deliver_list_tail, tmp);
        }

        /*
         * Reset the skb frag list
         */
        skb_frag_list_init(nbuf);
    }

    qdf_nbuf_set_rx_chfrag_end(deliver_list_tail, 1);
    *list_head = deliver_list_head;
    *list_tail = deliver_list_tail;
}

/*
 * Invalid peer handler
 */
static uint8_t osif_nss_wifili_handle_invalid_peer(ol_ath_soc_softc_t *soc, struct sk_buff *nbuf,
							uint8_t pool_id)
{
    struct dp_soc *dpsoc = NULL;
    struct dp_pdev *dp_pdev = NULL;

    dpsoc = (struct dp_soc *)wlan_psoc_get_dp_handle(soc->psoc_obj);
    if (!dpsoc) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
              "[nss-wifili] %pK: Dp handler is NULL", soc);
        qdf_nbuf_free(nbuf);
        return 0;
    }

    dp_pdev = dpsoc->pdev_list[pool_id];
    if (!dp_pdev) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
              "[nss-wifili]: %pK DP Pdev handler is NULL", dp_pdev);
        qdf_nbuf_free(nbuf);
        return 0;
    }

    hal_rx_mon_hw_desc_get_mpdu_status(dpsoc->hal_soc, nbuf->data,
                                 &(dp_pdev->ppdu_info.rx_status));

    dp_pdev->invalid_peer_head_msdu = NULL;
    dp_pdev->invalid_peer_tail_msdu = NULL;
    osif_nss_wifili_frag_to_chain(nbuf, &dp_pdev->invalid_peer_head_msdu,
                 &dp_pdev->invalid_peer_tail_msdu);

    /* Process the invalid peer */
    dp_rx_process_invalid_peer(dpsoc, nbuf, pool_id);
    dp_pdev->invalid_peer_head_msdu = NULL;
    dp_pdev->invalid_peer_tail_msdu = NULL;
    return 0;
}

#ifdef NBUF_MEMORY_DEBUG
static inline void osif_nss_qdf_nbuf_dbg_add(struct sk_buff *nbuf)
{
    qdf_nbuf_t curr_nbuf, next_nbuf;

    curr_nbuf = nbuf;
    while (curr_nbuf) {
        next_nbuf = qdf_nbuf_next(curr_nbuf);
        nbuf_debug_add_record(curr_nbuf);
        curr_nbuf = next_nbuf;
    }
}
#else
static inline void osif_nss_qdf_nbuf_dbg_add(struct sk_buff *nbuf) {}
#endif
/*
 * Extended data callback
 */
void osif_nss_wifili_ext_callback_fn(ol_ath_soc_softc_t *soc, struct sk_buff *nbuf,  __attribute__((unused)) struct napi_struct *napi)
{
    struct nss_wifili_soc_per_packet_metadata *wepm = NULL;
    uint16_t type;
    uint32_t headroom;
    struct dp_soc *dpsoc;

    osif_nss_qdf_nbuf_dbg_add(nbuf);

    if (soc == NULL) {
         QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                     "[nss-wifili]: %s , netdev is NULL, freeing skb", __func__);
         qdf_nbuf_free(nbuf);
         return;
    }

    dpsoc = (struct dp_soc *)wlan_psoc_get_dp_handle(soc->psoc_obj);
    headroom = skb_headroom(nbuf);

    /**
     * Headroom will be zero for packets exceptioned to host from NSS FW.
     * When packet is sent to host, nss driver does dma_unmap from head,
     * so skipping unmapping here.
     */

    dma_unmap_single(soc->nss_soc.dmadev, virt_to_phys(nbuf->head), headroom, DMA_FROM_DEVICE);
    wepm = (struct nss_wifili_soc_per_packet_metadata *)(nbuf->head + NSS_WIFILI_SOC_PER_PACKET_METADATA_OFFSET);
    type = wepm->pkt_type;

    switch (type) {
        case NSS_WIFILI_SOC_EXT_DATA_PKT_INVALID_PEER:
            {
                /**
                 * Reset the data to tlv header
                 */
                qdf_nbuf_push_head(nbuf, headroom);
                osif_nss_wifili_handle_invalid_peer(soc, nbuf, wepm->pool_id);
            }
            break;

        case NSS_WIFILI_SOC_EXT_DATA_PKT_MIC_ERROR:
            {
                uint8_t *rx_tlv_hdr = qdf_nbuf_data(nbuf);
                struct dp_peer *peer = NULL;
                uint16_t peer_id;
                peer_id = hal_rx_mpdu_start_sw_peer_id_get(rx_tlv_hdr);
                peer = dp_peer_find_by_id(dpsoc, peer_id);
                dp_rx_process_mic_error((struct dp_soc *)dpsoc, nbuf, rx_tlv_hdr, peer);
                if (peer) {
                    dp_peer_unref_del_find_by_id(peer);
                }
            }
            break;

        case NSS_WIFILI_SOC_EXT_DATA_PKT_2K_JUMP_ERROR:
            {
                uint8_t *rx_tlv_hdr = NULL;
                uint16_t peer_id;
                uint8_t tid;
                void *hal_soc = dpsoc->hal_soc;

                if (!hal_soc) {
                    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                               "[nss-wifili]: hal soc not found for 2k jump");
                    qdf_nbuf_free(nbuf);
                    break;
                }

                /**
                 * Reset the data to tlv header as the 2k jump
                 * API expect the data to be at tlv header.
                 */
                qdf_nbuf_push_head(nbuf, headroom);
                rx_tlv_hdr = qdf_nbuf_data(nbuf);
                peer_id = hal_rx_mpdu_start_sw_peer_id_get(rx_tlv_hdr);
                tid = hal_rx_mpdu_start_tid_get(hal_soc, rx_tlv_hdr);
                dp_2k_jump_handle(dpsoc, nbuf, rx_tlv_hdr,
                                  peer_id, tid);
            }
            break;

        case NSS_WIFILI_SOC_EXT_DATA_PKT_WIFI_PARSE_ERROR:
            {
                struct dp_peer *peer = NULL;
                uint16_t peer_id;
                uint8_t *rx_tlv_hdr = NULL;
                /**
                 * Reset the data to tlv header as
                 * API expects data to be at tlv header.
                 */
                qdf_nbuf_push_head(nbuf, headroom);
                rx_tlv_hdr = qdf_nbuf_data(nbuf);
                peer_id = hal_rx_mpdu_start_sw_peer_id_get(rx_tlv_hdr);
                peer = dp_peer_find_by_id(dpsoc, peer_id);
                dp_rx_process_rxdma_err(dpsoc, nbuf,
                                        rx_tlv_hdr, peer,
                                        HAL_RXDMA_ERR_WIFI_PARSE, wepm->pool_id);
                if (peer) {
                    dp_peer_unref_del_find_by_id(peer);
                }
                break;
            }
        default :
           {
                QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                      "[nss-wifili]: WIFILI EXT TYPE NOT FOUND");
                qdf_nbuf_free(nbuf);
           }
           break;
    }
}

static QDF_STATUS nss_wifili_soc_init(ol_ath_soc_softc_t *soc)
{
    /* struct nss_wifili_msg wlmsg; */
    struct dp_soc *dpsoc = (struct dp_soc *)wlan_psoc_get_dp_handle(soc->psoc_obj);
    struct nss_wifili_hal_srng_soc_msg *wlhssm;
    struct hal_mem_info hmi;
    uint32_t i, if_num;
    qdf_dma_addr_t mempaddr = 0;
    void *mem;
    struct nss_ctx_instance *nss_contex;
    struct nss_wifili_init_msg *wim = NULL;
    nss_tx_status_t status;
    nss_wifili_msg_callback_t msg_cb;
    int ret = 0;
    uint8_t num_radios = 0;
    uint8_t target_type = lmac_get_tgt_type(soc->psoc_obj);
    uint32_t required_size = 0;
    uint32_t mem_seg_cnt = 0;
    uint32_t required_size_ext = 0;
    struct nss_wifili_tx_desc_init_msg *wtdim;
    struct nss_wifili_tx_desc_addtnl_mem_msg *wtdam;
    uint32_t num_page_idx = 0;
    uint32_t cur_page_idx = 0;

    num_radios = lmac_get_num_radios(soc->psoc_obj);

    memset(&wlmsg, 0, sizeof(struct nss_wifili_msg));
    wim = &wlmsg.msg.init;

    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                  "[nss-wifili]:#4 soc init ");

    /*
     * TODO disable interrupt
     */
    memset(wim, 0, sizeof(struct nss_wifili_init_msg));

    if ((target_type != TARGET_TYPE_QCA8074) &&
        (target_type != TARGET_TYPE_QCA8074V2) &&
        (target_type != TARGET_TYPE_QCA6018)) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                "[nss-wifili]: Invalid target soc type");
        return QDF_STATUS_E_FAILURE;
    }

    wim->target_type = target_type;
    wim->wrip.tlv_size = sizeof(struct rx_pkt_tlvs);
    wim->wrip.rx_buf_len = RX_BUFFER_SIZE;

    hal_get_meminfo(dpsoc->hal_soc, &hmi);
    wlhssm = &wim->hssm;
    wlhssm->dev_base_addr = (uint32_t)(nss_ptr_t) hmi.dev_base_paddr;

    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                   "[nss-wifili]: dev Base addr %p %p ",hmi.dev_base_paddr, hmi.dev_base_addr);

    wlhssm->shadow_rdptr_mem_addr = (uint32_t)(nss_ptr_t)hmi.shadow_rdptr_mem_paddr;
    wlhssm->shadow_wrptr_mem_addr = (uint32_t)(nss_ptr_t)hmi.shadow_wrptr_mem_paddr;

    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
       "[nss-wifili]: dev_mem %x shadow rd %x wr %x ",wlhssm->dev_base_addr, wlhssm->shadow_rdptr_mem_addr, wlhssm->shadow_wrptr_mem_addr);

    wim->num_tcl_data_rings = dpsoc->num_tcl_data_rings;
    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                    "[nss-wifili]: num TCL ring %d ", dpsoc->num_tcl_data_rings);

    for (i = 0; i < dpsoc->num_tcl_data_rings; i++) {
        osif_nss_update_ring_info(dpsoc, &dpsoc->tcl_data_ring[i], &wim->tcl_ring_info[i], hmi.dev_base_paddr, hmi.dev_base_addr);
        osif_nss_update_ring_info(dpsoc, &dpsoc->tx_comp_ring[i], &wim->tx_comp_ring[i], hmi.dev_base_paddr, hmi.dev_base_addr);
    }

    wtdim = &wim->wtdim;
    wtdam = &wim->wtdam;
    wtdim->num_pool = num_radios;
    if (wtdim->num_pool == 1) {
       wtdim->num_tx_desc = WIFILI_NSS_DBDC_NUM_TX_DESC;
       wtdim->num_tx_desc_ext = WIFILI_NSS_DBDC_NUM_TX_EXT_DESC;
       required_size = (wtdim->num_tx_desc + 1) * WIFILI_NSS_TX_DESC_SIZE;
       required_size_ext = (wtdim->num_tx_desc_ext + 1) * WIFILI_NSS_TX_EXT_DESC_SIZE;
    } else if (wtdim->num_pool == 2) {
       wtdim->num_tx_desc = WIFILI_NSS_DBDC_NUM_TX_DESC;
       wtdim->num_tx_desc_ext = WIFILI_NSS_DBDC_NUM_TX_EXT_DESC;
       wtdim->num_tx_desc_2 = WIFILI_NSS_DBDC_NUM_TX_DESC_2;
       wtdim->num_tx_desc_ext_2 = WIFILI_NSS_DBDC_NUM_TX_EXT_DESC_2;
       required_size = (wtdim->num_tx_desc + wtdim->num_tx_desc_2 + 2) * WIFILI_NSS_TX_DESC_SIZE;
       required_size_ext = (wtdim->num_tx_desc_ext + wtdim->num_tx_desc_ext_2 + 2) * WIFILI_NSS_TX_EXT_DESC_SIZE;
    } else if (wtdim->num_pool == 3) {
       wtdim->num_tx_desc = WIFILI_NSS_DBTC_NUM_TX_DESC;
       wtdim->num_tx_desc_ext = WIFILI_NSS_DBTC_NUM_TX_EXT_DESC;
       wtdim->num_tx_desc_2 = WIFILI_NSS_DBTC_NUM_TX_DESC_2;
       wtdim->num_tx_desc_ext_2 = WIFILI_NSS_DBTC_NUM_TX_EXT_DESC_2;
       wtdim->num_tx_desc_3 = WIFILI_NSS_DBTC_NUM_TX_DESC_3;
       wtdim->num_tx_desc_ext_3 = WIFILI_NSS_DBTC_NUM_TX_EXT_DESC_3;
       required_size = (wtdim->num_tx_desc + wtdim->num_tx_desc_2 + wtdim->num_tx_desc_3 + 3) * WIFILI_NSS_TX_DESC_SIZE;
       required_size_ext = (wtdim->num_tx_desc_ext + wtdim->num_tx_desc_ext_2 + wtdim->num_tx_desc_ext_3 + 3) * WIFILI_NSS_TX_EXT_DESC_SIZE;
    } else {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                    "[nss-wifili]: Invalid nummber of pool %d configured", wtdim->num_pool);
        return QDF_STATUS_E_FAILURE;
    }

    wtdim->num_memaddr = 0;
    num_page_idx = 0;

    {
        int i = 0;
        uint32_t allocsize = 0;


        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                   "[nss-wifili]: NSS Tx desc size %d cnt %d  ", WIFILI_NSS_TX_DESC_SIZE, wtdim->num_tx_desc);
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                   "[nss-wifili]: Required Mem %d  Max per page %d ", required_size, WIFILI_NSS_MAX_MEM_PAGE_SIZE);


        for ( i = 0; (i < required_size && (num_page_idx < OSIF_NSS_WIFILI_MAX_NUMBER_OF_PAGE)); ) {
            allocsize = required_size - i ;
            if(  allocsize > WIFILI_NSS_MAX_MEM_PAGE_SIZE ) {
                allocsize = WIFILI_NSS_MAX_MEM_PAGE_SIZE;
            }

            mempaddr = soc->nss_soc.desc_pmemaddr[num_page_idx];
            mem = (void *)soc->nss_soc.desc_vmemaddr[num_page_idx];
            if (!mem || (mempaddr == 0x0)) {
                mem = qdf_mem_alloc_consistent(dpsoc->osdev, dpsoc->osdev->dev, allocsize, &mempaddr);
                if ( mem == NULL) {
                    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                        "[nss-wifili]: memory allocation failure");
                    goto free_mem;
                }
                soc->nss_soc.desc_pmemaddr[num_page_idx] = (uint32_t)mempaddr;
                soc->nss_soc.desc_vmemaddr[num_page_idx] = (uintptr_t)mem;
                soc->nss_soc.desc_memsize[num_page_idx] = allocsize;
                mem_seg_cnt++;
            }

            i += allocsize ;
            num_page_idx++;
        }

        if (mem_seg_cnt) {
            QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_WARN,
                   "[nss-wifili]: NSS Txdesc alloc of size %d required_size %d  mem_seg_cnt %d",
                   WIFILI_NSS_TX_DESC_SIZE, required_size, mem_seg_cnt);
        }
    }

    if (num_page_idx == OSIF_NSS_WIFILI_MAX_NUMBER_OF_PAGE) {
            QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                   "[nss-wifili]: Requested NSS Txdesc total memory of size %d cannot be accomodated in  mem_seg_cnt %d",
                   required_size, mem_seg_cnt);
            goto free_mem;
    }


    wtdim->ext_desc_page_num  = num_page_idx;
    mem_seg_cnt = 0;
    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                        "[nss-wifili]: ext desc page addr %d ",wtdim->ext_desc_page_num );

    {
        int i = 0;
        uint32_t allocsize = 0;

        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                  "[nss-wifili]: ext Tx desc size %d cnt %d  ", WIFILI_NSS_TX_EXT_DESC_SIZE,
                                                              wtdim->num_tx_desc_ext);

        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                   "[nss-wifili]: required Mem %d  Max per page %d ", required_size_ext,
                    WIFILI_NSS_MAX_EXT_MEM_PAGE_SIZE);

        for ( i = 0; (i < required_size_ext && (num_page_idx < OSIF_NSS_WIFILI_MAX_NUMBER_OF_PAGE)); ) {
            allocsize = required_size_ext - i ;
            if(  allocsize > WIFILI_NSS_MAX_EXT_MEM_PAGE_SIZE ) {
                allocsize = WIFILI_NSS_MAX_EXT_MEM_PAGE_SIZE;
            }

            mempaddr = soc->nss_soc.desc_pmemaddr[num_page_idx];
            mem = (void *)soc->nss_soc.desc_vmemaddr[num_page_idx];
            if (!mem || (mempaddr == 0x0)) {
                mem = qdf_mem_alloc_consistent(dpsoc->osdev, dpsoc->osdev->dev, allocsize, &mempaddr);
                if ( mem == NULL) {
                    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                       "[nss-wifili]: memory allocation failure");
                    goto free_mem;
                }

                soc->nss_soc.desc_pmemaddr[num_page_idx] = (uint32_t)mempaddr;
                soc->nss_soc.desc_vmemaddr[num_page_idx] = (uintptr_t)mem;
                soc->nss_soc.desc_memsize[num_page_idx] = allocsize;
                mem_seg_cnt++;
            }

            i += allocsize;
            num_page_idx++;
        }

        if (mem_seg_cnt) {
            QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_WARN,
                    "[nss-wifili]: NSS Tx Ext desc alloc of size %d required_size_ext  %d mem_seg_cnt %d",
                    WIFILI_NSS_TX_DESC_SIZE, required_size_ext, mem_seg_cnt);
        }
    }

    if (num_page_idx == OSIF_NSS_WIFILI_MAX_NUMBER_OF_PAGE) {
            QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                   "[nss-wifili]: Requested NSS extended Txdesc total memory of size %d cannot be accomodated in  mem_seg_cnt %d",
                   required_size_ext, num_page_idx);
            goto free_mem;
    }

    /*
     * check if nss init message can accomodate requested number of pages
     */
    QDF_ASSERT(OSIF_NSS_WIFILI_MAX_NUMBER_OF_PAGE <=
            (NSS_WIFILI_MAX_NUMBER_OF_PAGE_MSG + WIFILI_MAX_NUMBER_OF_ADDTNL_SEG));

    /*
     * Update page details in the message
     */
    for (cur_page_idx = 0; cur_page_idx < num_page_idx; cur_page_idx++)
    {
        if (cur_page_idx >= NSS_WIFILI_MAX_NUMBER_OF_PAGE_MSG) {
            uint32_t addtnl_page_idx = cur_page_idx - NSS_WIFILI_MAX_NUMBER_OF_PAGE_MSG;
            wtdam->addtnl_memory_addr[addtnl_page_idx] = soc->nss_soc.desc_pmemaddr[cur_page_idx];
            wtdam->addtnl_memory_size[addtnl_page_idx] = soc->nss_soc.desc_memsize[cur_page_idx];
            QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                    "[nss-wifili]: allocated additional Page  addtnl_page_idx %d size %d memaddr %x",
                    addtnl_page_idx, (uint64_t)wtdam->addtnl_memory_size[addtnl_page_idx], wtdam->addtnl_memory_addr[addtnl_page_idx]);
            wtdam->num_addtnl_addr++;
        } else {
            wtdim->memory_addr[cur_page_idx] = soc->nss_soc.desc_pmemaddr[cur_page_idx];
            wtdim->memory_size[cur_page_idx] = soc->nss_soc.desc_memsize[cur_page_idx];
            QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                    "[nss-wifili]: allocated Page  page_idx %d size %d memaddr %x",
                    cur_page_idx , (uint64_t)wtdim->memory_size[cur_page_idx], wtdim->memory_addr[cur_page_idx]);
            wtdim->num_memaddr++;
        }
    }

    if (cur_page_idx > NSS_WIFILI_MAX_NUMBER_OF_PAGE_MSG) {
        wim->flags |= WIFILI_ADDTL_MEM_SEG_SET;
    }

    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                  "[nss-wifili]: num REO ring %d ", dpsoc->num_reo_dest_rings);
    wim->num_reo_dest_rings = dpsoc->num_reo_dest_rings;
    for (i = 0; i < dpsoc->num_reo_dest_rings; i++) {
        osif_nss_update_ring_info(dpsoc, &dpsoc->reo_dest_ring[i], &wim->reo_dest_ring[i], hmi.dev_base_paddr, hmi.dev_base_addr);
    }

    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                  "[nss-wifili]: reo_reinject_ring ");
    osif_nss_update_ring_info(dpsoc, &dpsoc->reo_reinject_ring, &wim->reo_reinject_ring, hmi.dev_base_paddr, hmi.dev_base_addr);

    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                  "[nss-wifili]: rx_rel_ring ");
    osif_nss_update_ring_info(dpsoc, &dpsoc->rx_rel_ring, &wim->rx_rel_ring, hmi.dev_base_paddr, hmi.dev_base_addr);

    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                  "[nss-wifili]: reo_exception_ring");
    osif_nss_update_ring_info(dpsoc, &dpsoc->reo_exception_ring, &wim->reo_exception_ring, hmi.dev_base_paddr, hmi.dev_base_addr);

    /*
     * set the cce disabled flag
     */
    if (dpsoc->cce_disable) {
        wim->flags |= WIFILI_NSS_CCE_DISABLED;
    }

    if_num = NSS_WIFILI_INTERFACE;
    /*
     * add callback
     */
    nss_contex = nss_register_wifili_if(NSS_WIFILI_INTERFACE, NULL,
            (nss_wifili_callback_t)osif_nss_wifili_ext_callback_fn,
            (nss_wifili_msg_callback_t)osif_nss_wifili_event_receive,
            (struct net_device *)soc, 0);
    if (!nss_contex) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                 "[nss-wifili]: registration failed");
        goto free_mem;
    }

    if (nss_cmn_get_state(nss_contex) != NSS_STATE_INITIALIZED) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                 "[nss-wifili]: NSS core is not initialised");
        goto unregister;
    }

    soc->nss_soc.nss_sctx = nss_contex;
    soc->nss_soc.nss_sifnum = if_num;
    soc->nss_soc.nss_sidx = 0;
    soc->nss_soc.dmadev = dpsoc->osdev->dev;

    msg_cb = (nss_wifili_msg_callback_t)osif_nss_wifili_event_receive;
    osif_nss_wifili_init_completion_status(soc);

    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                  "[nss-wifili]: send init message to NSS nss ctx %p wifili if_num %d", nss_contex, if_num);

    nss_cmn_msg_init(&wlmsg.cm, if_num, NSS_WIFILI_INIT_MSG,
            sizeof(struct nss_wifili_init_msg), msg_cb, NULL);


    status = nss_wifili_tx_msg(nss_contex, &wlmsg);
    if (status != NSS_TX_SUCCESS) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                   "[nss-wifili]: wifi initialization fail%d ", status);
        goto unregister;
    }

    /*
     * Blocking call, wait till we get ACK for this msg.
     */
    ret = wait_for_completion_timeout(&soc->nss_soc.osif_nss_ol_wifi_cfg_complete.complete, msecs_to_jiffies(OSIF_NSS_WIFI_INIT_TIMEOUT_MS));
    if (ret == 0) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                    "[nss-wifili]: Waiting for soc wifili init timed out");
        goto unregister;
    }

    /*
     * ACK/NACK received from NSS FW
     */
    if (NSS_TX_FAILURE == soc->nss_soc.osif_nss_ol_wifi_cfg_complete.response) {
         QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                    "[nss-wifili]: received NACK for soc wifiili init");
         goto unregister;
    }

    /*
     * Register the nss related functions
     */
    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                "[nss-wifili]:Register hard start call osif");
    osif_register_dev_ops_xmit(osif_nss_ol_vap_hardstart, OSIF_NETDEV_TYPE_NSS_WIFIOL);
    return QDF_STATUS_SUCCESS;

unregister:
    nss_unregister_wifili_if(if_num);
    soc->nss_soc.nss_sctx = NULL;
    soc->nss_soc.nss_sifnum = -1;
    soc->nss_soc.nss_sidx = -1;

free_mem:
    /*
     * Free the host pool allocated for NSS
     */
    (void)osif_nss_wifili_desc_pool_free(soc);
    return QDF_STATUS_E_FAILURE;
}

static int osif_nss_wifili_detach(ol_ath_soc_softc_t *soc, uint32_t delay)
{
    uint32_t i, radio_id = 0;
    uint8_t num_radios;

    if (!soc->nss_soc.nss_scfg) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                           "[nss-wifili]: nss_scfg = %d", soc->nss_soc.nss_scfg);
        return QDF_STATUS_SUCCESS;
    }

    num_radios = lmac_get_num_radios(soc->psoc_obj);

    osif_nss_wifili_soc_stop(soc);

    for (i = 0; i < num_radios; i++) {
        if (soc->nss_soc.nss_scfg & (1 << i) ) {
            struct wlan_objmgr_pdev *pdev;
            struct ol_ath_softc_net80211 *scn;

            pdev = wlan_objmgr_get_pdev_by_id(soc->psoc_obj, i, WLAN_MLME_NB_ID);
            if (pdev == NULL) {
                 qdf_print("%s: pdev object (id: %d) is NULL ", __func__, i);
                 continue;
            }

            scn = lmac_get_pdev_feature_ptr(pdev);
            if (scn == NULL) {
                 wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_NB_ID);
                 continue;
            }

            QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
            "[nss-wifili]: calling pdev de-attach with id %d ", radio_id);
            osif_nss_wifili_pdev_detach(scn, &radio_id);
            radio_id++;
            wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_NB_ID);
        }
    }

    osif_nss_wifili_pmem_free(soc, &soc->nss_soc.nwpmem);
    osif_nss_wifili_soc_reset(soc);
    return QDF_STATUS_SUCCESS;
}

static int osif_nss_wifili_attach(ol_ath_soc_softc_t *soc)
{
    uint32_t i, radio_id = 0;
    uint8_t num_radios;
    QDF_STATUS error = QDF_STATUS_SUCCESS;

    num_radios = lmac_get_num_radios(soc->psoc_obj);

    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
              "[nss-wifili]:#4 soc attach soc cfg%d num radio %d ", soc->nss_soc.nss_scfg, num_radios);

    error =  nss_wifili_soc_init(soc);
    if (error != QDF_STATUS_SUCCESS) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                "[nss-wifili]: Unable to do soc attach ");
        return error;
    }

    if (osif_nss_wifili_pmem_init(&soc->nss_soc.nwpmem)) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                "[nss-wifili]: Peer mem initialization failed");
    }

    for (i = 0; i < num_radios; i++) {
        if (soc->nss_soc.nss_scfg & (1 << i)) {
            struct ieee80211com* ic;
            struct wlan_objmgr_pdev *pdev;
            struct ol_ath_softc_net80211 *scn;

            pdev = wlan_objmgr_get_pdev_by_id(soc->psoc_obj, i, WLAN_MLME_NB_ID);
            if (pdev == NULL) {
                 qdf_print("%s: pdev object (id: %d) is NULL ", __func__, i);
                 continue;
            }

            scn = lmac_get_pdev_feature_ptr(pdev);
            if (scn == NULL) {
                 wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_NB_ID);
                 continue;
            }

            ic = &scn->sc_ic;

            if (soc->nss_soc.nss_nxthop  & (1 << i)) {
                scn->nss_radio.nss_nxthop = 1;
            }
            QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                        "[nss-wifili]: call pdev attach radio id %d ", radio_id);
            ic->nss_vops = &nss_wifili_vops;
            ic->nss_iops = &nss_wifili_iops;
            ic->nss_radio_ops = &nss_wifili_radio_ops;
            error = nss_wifili_pdev_attach(scn, &radio_id);
            if (error != QDF_STATUS_SUCCESS) {
                /*
                 * free the host allocated memory passed to NSS
                 * and the nss memory by sending reset cmd.
                 */
                QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                        "[nss-wifili]: Unable to do pdev attach %d ", i);
                 wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_NB_ID);
                return error;
            }
            radio_id++;
            wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_NB_ID);
        }
    }

    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                        "[nss-wifili]: send start message");
    if ((error = osif_nss_wifili_soc_start(soc)) != QDF_STATUS_SUCCESS) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                        "[nss-wifili]: wifili soc start failed");
        return error;
    }

    return error;
}

static void osif_nss_wifili_return_peer_mem(struct nss_wifi_peer_mem *nwpmem, uint16_t peer_id)
{
    uint32_t alloc_idx = 0;

    if (peer_id >= OSIF_NSS_WIFI_MAX_PEER_ID) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                "[nss-wifili]: %s, invalid peer id  %d", __FUNCTION__, peer_id);
        return;
    }

    spin_lock_bh(&nwpmem->queue_lock);
    alloc_idx = nwpmem->peer_id_to_alloc_idx_map[peer_id];

    if (!nwpmem->in_use[alloc_idx]) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                "[nss-wifili]: %s, already deleted peer %d %d", __FUNCTION__, peer_id, alloc_idx);
        spin_unlock_bh(&nwpmem->queue_lock);
        return;
    }

    nwpmem->peer_id_to_alloc_idx_map[peer_id] = -1;
    nwpmem->in_use[alloc_idx] = false;
    spin_unlock_bh(&nwpmem->queue_lock);
    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
            "[nss-wifili]: %s, Peer %d deleted with memory pool %d ", __FUNCTION__, peer_id, alloc_idx);
}

static uint32_t osif_nss_wifili_pmem_alloc(ol_ath_soc_softc_t *soc, struct nss_wifi_peer_mem *nwpmem, uint16_t peer_id)
{
    void *mem = NULL;
    uint32_t pmem = 0;
    uint32_t mem_allocated = 0;
    struct dp_soc  *dpsoc = NULL;

    dpsoc = (struct dp_soc *)wlan_psoc_get_dp_handle(soc->psoc_obj);
    if (!dpsoc) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                "[nss-wifili]: dp_soc is NULL");
        return 0;
    }

    mem = qdf_mem_malloc_atomic(WIFILI_NSS_PEER_BYTE_SIZE);
    if (mem == NULL) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,"[nss-wifili]: memory allocation failure");
        return 0;
    }
    pmem = (uint32_t)dma_map_single(dpsoc->osdev->dev, mem, WIFILI_NSS_PEER_BYTE_SIZE, DMA_TO_DEVICE);
    if (dma_mapping_error(dpsoc->osdev->dev, pmem)) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                "[nss-wifili]: failed to get physical memory %pK", mem);
        return 0;
    }

    /*
     *Store the physical address sent to NSS
     */
    spin_lock_bh(&nwpmem->queue_lock);
    nwpmem->vaddr[nwpmem->available_idx] = (uintptr_t)mem;
    nwpmem->paddr[nwpmem->available_idx] = pmem;
    nwpmem->in_use[nwpmem->available_idx] = true;
    nwpmem->peer_id_to_alloc_idx_map[peer_id] = nwpmem->available_idx;
    mem_allocated = nwpmem->paddr[nwpmem->available_idx];
    nwpmem->available_idx ++;
    spin_unlock_bh(&nwpmem->queue_lock);

    return mem_allocated;
}

static uint32_t osif_nss_wifili_get_peer_mem(ol_ath_soc_softc_t *soc, struct nss_wifi_peer_mem *nwpmem, uint16_t peer_id)
{
    uint32_t j = 0;

    if (peer_id >= OSIF_NSS_WIFI_MAX_PEER_ID) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                "[nss-wifili]: %s, invalid peer id  %d", __FUNCTION__, peer_id);
        return 0;
    }

    /*
     * Find the free location in memory array which can be sent to NSS
     */
    spin_lock_bh(&nwpmem->queue_lock);
    for (j = 0; j < nwpmem->available_idx; j++) {
        if (nwpmem->in_use[j] == false) {

            /*
             * Store the peer id for corresponding memory allocation
             */
            nwpmem->in_use[j] = true;
            nwpmem->peer_id_to_alloc_idx_map[peer_id] = j;
            spin_unlock_bh(&nwpmem->queue_lock);
            return (nwpmem->paddr[j]);
        }
    }
    spin_unlock_bh(&nwpmem->queue_lock);

    /*
     * allocate new peer memory and retrun
     */
    return osif_nss_wifili_pmem_alloc(soc, nwpmem, peer_id);
}

static void osif_nss_wifili_pmem_free(ol_ath_soc_softc_t *soc, struct nss_wifi_peer_mem *nwpmem)
{
    uint32_t j = 0;
    struct dp_soc  *dpsoc = NULL;
    dpsoc = (struct dp_soc *)wlan_psoc_get_dp_handle(soc->psoc_obj);

    if (!dpsoc) {
        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_ERROR,
                "[nss-wifili]: dp_soc is NULL");
        return;
    }

    for (j = 0; j < nwpmem->available_idx; j++) {
        /*
         * Free all the memory blocks allocated for NSS peer
         */
        if (nwpmem->vaddr[j] != 0) {
            dma_unmap_single(dpsoc->osdev->dev, nwpmem->paddr[j], WIFILI_NSS_PEER_BYTE_SIZE, DMA_FROM_DEVICE);
            qdf_mem_free((void*)nwpmem->vaddr[j]);
        }

        nwpmem->in_use[j] = false;
        nwpmem->peer_id_to_alloc_idx_map[j] = -1;
        nwpmem->vaddr[j] = 0;
        nwpmem->paddr[j] = 0;

        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                "[nss-wifili]: freeing memory with block id %d", j);

    }
    nwpmem->available_idx = 0;
}

static uint osif_nss_wifili_pmem_init(struct nss_wifi_peer_mem *nwpmem)
{
    int j = 0;
    for (j = 0; j < OSIF_NSS_WIFI_MAX_PEER_ID; j++) {
        /*
         *Initializing the peer index allocated to false
         */
        nwpmem->in_use[j] = false;
        nwpmem->peer_id_to_alloc_idx_map[j] = -1;
        nwpmem->vaddr[j] = 0;
        nwpmem->paddr[j] = 0;
    }
    nwpmem->available_idx = 0;
    spin_lock_init(&nwpmem->queue_lock);
    return 0;

}

struct nss_wifi_soc_ops nss_wifili_soc_ops = {
    osif_nss_wifili_peer_delete,
    osif_nss_wifili_peer_create,
    osif_nss_wifili_init,
    osif_nss_wifili_post_recv_buffer,
    osif_nss_wifili_ce_flush_raw_send,
    osif_nss_wifili_set_cfg,
    osif_nss_wifili_detach,
    osif_nss_wifili_attach,
    osif_nss_wifili_map_wds_peer,
    osif_nss_wifili_soc_stop,
    osif_nss_wifili_nawds_enable,
    osif_nss_wifili_set_default_nexthop,
    osif_nss_wifili_desc_pool_free,
};
