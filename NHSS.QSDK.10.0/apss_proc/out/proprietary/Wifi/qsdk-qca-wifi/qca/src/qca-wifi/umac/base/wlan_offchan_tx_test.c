/*
 * Copyright (c) 2011-2017, 2019 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011-2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * Copyright (c) 2008 Atheros Communications Inc.
 * All Rights Reserved.
 *
 */

#include <ieee80211_var.h>
#include <ieee80211_channel.h>
#include "if_athvar.h"
#include <qdf_lock.h>
#include <ieee80211_objmgr_priv.h>
#include <wlan_offchan_txrx_api.h>
#include <ieee80211_mlme_dfs_dispatcher.h>

#define OFFCHAN_EXT_TID_NONPAUSE    19
#define OFFCHAN_EXT_TID_INVALID     31
#define OFFCHAN_TX_TEST_POWER       63
#define OFFCHAN_TX_TEST_CHAIN       -1
#define OFFCHAN_TX_TEST_MCS          3
#define OFFCHAN_TX_TEST_DEQUEUE_RATE 2
#define OFFCHAN_TX_TEST_DATA_LEN  1000

qdf_nbuf_t wlan_offchan_data_frame(struct wlan_objmgr_peer *peer, int ac,
                                enum wbuf_type type,
                                uint16_t len, bool random_addr,
                                struct net_device *netdev)
{
    wlan_node_t ni;
    struct ieee80211vap *vap;
    struct ieee80211com *ic;
    qdf_nbuf_t wbuf;
    struct ieee80211_qosframe *qwh;
    /*Setting both source and destination MAC addresses to random addresses*/
    const u_int8_t src[6] = {0x00, 0x02, 0x03, 0x06, 0x02, 0x01};
    const u_int8_t dst[6] = {0x00, 0x02, 0x03, 0x04, 0x05, 0x06};
    struct sk_buff *skb;

    if(peer == NULL) {
        qdf_print("%s:PEER is NULL ",__func__);
        return NULL;
    }

    ni = (wlan_node_t)wlan_peer_get_mlme_ext_obj(peer);
    vap = ni->ni_vap;
    ic = ni->ni_ic;

    wbuf = wbuf_alloc(ic->ic_osdev, type, len);
    if (wbuf == NULL)
    {
        return NULL;
    }

    /*
     * For non active VAP iv_bss->ni_bssid is not populated, so
     * copy VAP mac address to iv_bss->ni_bssid.
     */
    if (!ieee80211_is_vap_state_running(vap)) {
        IEEE80211_ADDR_COPY(ni->ni_bssid, vap->iv_myaddr);
    }

    ieee80211_prepare_qosnulldata(ni, wbuf, ac);

    qwh = (struct ieee80211_qosframe *)wbuf_header(wbuf);
    if (random_addr) {
        ieee80211_send_setup(vap, ni, (struct ieee80211_frame *)qwh,
                IEEE80211_FC0_TYPE_DATA | IEEE80211_FC0_SUBTYPE_QOS,
                src, /* SA */
                dst,            /* DA */
                ni->ni_bssid);
    }

    wbuf_set_pktlen(wbuf, len);
    if (IEEE80211_IS_CHAN_5GHZ(ic->ic_curchan)) {
        wbuf_set_tx_rate(wbuf, 0, RATECODE_PREAM_OFDM, OFFCHAN_TX_TEST_MCS,
                ic->ic_he_target);
    } else {
        wbuf_set_tx_rate(wbuf, 0, RATECODE_PREAM_CCK, OFFCHAN_TX_TEST_MCS,
                ic->ic_he_target);
    }
    if (type == WBUF_TX_DATA) {
        /* force with NONPAUSE_TID */
        wbuf_set_tid(wbuf, OFFCHAN_EXT_TID_NONPAUSE);
        /* Set tx ctrl params */
        wbuf_set_tx_ctrl(wbuf, 0, OFFCHAN_TX_TEST_POWER, OFFCHAN_TX_TEST_CHAIN);
        skb = (struct sk_buff *)wbuf;
        skb->dev = netdev;
    }

    wlan_wbuf_set_peer_node(wbuf, ni);

    return wbuf;
}

void wlan_offchan_tx_complete(struct wlan_objmgr_vdev *vdev,
                                struct offchan_tx_status *status,
                                struct offchan_stats *stats)
{
    struct ieee80211vap *vap;
    uint8_t i;

    for (i = 0; i < status->count; i++) {
        offchan_debug("Offchan status %d - %d\n", i, status->status[i]);
    }
    vap = wlan_vdev_get_mlme_ext_obj(vdev);
    if (vap)
        ieee80211_ic_offchanscan_clear(vap->iv_ic);
}

static
int wlan_offchan_tx_test_send_req(struct wlan_objmgr_vdev *vdev, void *netdev,
                                    uint32_t chan_freq, uint16_t dwell_time)
{
    struct ieee80211vap *vap;
    struct offchan_tx_req req;
    qdf_nbuf_t nbuf_mgmt, nbuf_data;
    struct ieee80211_node *ni;

    vap = wlan_vdev_get_mlme_ext_obj(vdev);
    if (vap == NULL) {
        offchan_err("VAP not in valid state");
        return -EINVAL;
    }

    qdf_mem_zero(&req, sizeof(struct offchan_tx_req));
    req.chan = chan_freq;
    req.dwell_time = dwell_time;
    req.offchan_rx = false;
    req.complete_dwell_tx = true;
    req.dequeue_rate = OFFCHAN_TX_TEST_DEQUEUE_RATE;
    qdf_nbuf_queue_init(&req.offchan_tx_list);
    ni = ieee80211_try_ref_node(vap->iv_bss);
    if (ni) {
        nbuf_mgmt = wlan_offchan_data_frame(ni->peer_obj, WME_AC_VI, WBUF_TX_MGMT,
                sizeof(struct ieee80211_qosframe), false, netdev);
        if (nbuf_mgmt == NULL) {
            offchan_err("Failed to get offchan data nbuf");
            ieee80211_free_node(ni);
            return -EINVAL;
        }
        qdf_nbuf_queue_add(&req.offchan_tx_list, nbuf_mgmt);

        nbuf_data = wlan_offchan_data_frame(ni->peer_obj, WME_AC_VO, WBUF_TX_DATA,
                OFFCHAN_TX_TEST_DATA_LEN, true, netdev);
        if (nbuf_data == NULL) {
            offchan_err("Failed to get offchan data nbuf");
            wbuf_free(nbuf_mgmt);
            ieee80211_free_node(ni);
            return -EINVAL;
        }
        qdf_nbuf_queue_add(&req.offchan_tx_list, nbuf_data);
        ieee80211_free_node(ni);
    } else {
        offchan_err("%s: Failed to get node ref\n", __func__);
        return -EINVAL;
    }
    req.tx_comp = wlan_offchan_tx_complete;
    req.rx_ind = NULL;

    if (ucfg_offchan_tx_request(vap->vdev_obj, &req) != QDF_STATUS_SUCCESS) {
        /* Free nbuf list */
        wbuf_free(nbuf_mgmt);
        wbuf_free(nbuf_data);
        return -EINVAL;
    }
    ieee80211_ic_offchanscan_set(vap->iv_ic);

    return 0;
}

int ucfg_offchan_tx_test(struct wlan_objmgr_vdev *vdev, void *netdev,
        u_int32_t chan, u_int16_t dwell_time)
{
    struct ieee80211vap *vap;
    struct ieee80211com *ic;
    struct ieee80211_ath_channel *channel;
    struct wlan_objmgr_pdev *pdev;

    vap = wlan_vdev_get_mlme_ext_obj(vdev);
    if (vap == NULL) {
        offchan_err("VAP not in valid state");
        return -EINVAL;
    }
    ic = vap->iv_ic;
    if (ic->recovery_in_progress) {
        offchan_err("target recovery in progress, return");
        return -EINVAL;
    }
    pdev = ic->ic_pdev_obj;
    if(pdev == NULL) {
        return -EINVAL;
    }

    /*fail if CAC timer is running*/
    if (wlan_objmgr_pdev_try_get_ref(pdev, WLAN_DFS_ID) !=
            QDF_STATUS_SUCCESS) {
        return -EINVAL;
    }
    if (mlme_dfs_is_ap_cac_timer_running(pdev)) {
        offchan_err("cac timer still running, return");
        wlan_objmgr_pdev_release_ref(pdev, WLAN_DFS_ID);
        return -EINVAL;
    }
    wlan_objmgr_pdev_release_ref(pdev, WLAN_DFS_ID);

    /*fail if ioctl CSA's going on*/
    if((ic->ic_flags_ext2 & IEEE80211_FEXT2_CSA_WAIT) ||
        vap->channel_switch_state) {
        return -EINVAL;
    }
    /* check channel is supported */
    channel = ieee80211_find_dot11_channel(ic, chan, vap->iv_des_cfreq2, vap->iv_des_mode | ic->ic_chanbwflag);
    if (channel == NULL) {
        channel = ieee80211_find_dot11_channel(ic, chan, 0, IEEE80211_MODE_AUTO);

        if (channel == NULL) {
            qdf_print("Invalid Channel");
            return -EINVAL;
        }
    }

    if(IEEE80211_IS_CHAN_RADAR(channel))
    {
        qdf_print("Radar detected on channel %d.So, no Tx is allowed till the nol expiry",channel->ic_ieee);
        return -EINVAL;
    }

    if(!((dwell_time > 0) && (dwell_time < ieee80211_vap_get_beacon_interval(vap)))) {
        qdf_print("Dwell time is more than Beacon interval, Offchan scan duration is"
                " not allowed more than Beacon interval ");
        return -EINVAL;
    }

    return wlan_offchan_tx_test_send_req(vdev, netdev, channel->ic_freq, dwell_time);
}

void wlan_offchan_rx_indication(struct wlan_objmgr_vdev *vdev)
{
    struct ieee80211vap *vap;
    offchan_debug("Offchan RX foreign chan indication \n");
    vap = wlan_vdev_get_mlme_ext_obj(vdev);
    if (vap)
        ieee80211_ic_offchanscan_clear(vap->iv_ic);
}

static
int wlan_offchan_rx_test_send_req(struct wlan_objmgr_vdev *vdev, uint32_t chan_freq,
                                  uint16_t dwell_time, uint32_t chan,
                                  uint8_t bw_mode, uint8_t sec_chan_offset)
{
    struct ieee80211vap *vap;
    struct offchan_tx_req req = { 0 };

    vap = wlan_vdev_get_mlme_ext_obj(vdev);
    if (vap == NULL) {
        offchan_err("VAP not in valid state");
        return -EINVAL;
    }

    req.chan = chan_freq;
    req.dwell_time = dwell_time;
    req.offchan_rx = true;
    req.wide_scan.chan_no = chan;
    req.wide_scan.bw_mode = bw_mode;
    req.wide_scan.sec_chan_offset = sec_chan_offset;
    req.complete_dwell_tx = false;
    req.dequeue_rate = OFFCHAN_TX_TEST_DEQUEUE_RATE;
    qdf_nbuf_queue_init(&req.offchan_tx_list);
    req.tx_comp = wlan_offchan_tx_complete;
    req.rx_ind = wlan_offchan_rx_indication;

    if (ucfg_offchan_tx_request(vap->vdev_obj, &req) != QDF_STATUS_SUCCESS) {
        return -EINVAL;
    }
    ieee80211_ic_offchanscan_set(vap->iv_ic);

    return 0;
}

int ucfg_offchan_rx_test(struct wlan_objmgr_vdev *vdev, u_int32_t chan, u_int16_t dwell_time,
                         u_int8_t bw_mode, u_int8_t sec_chan_offset)
{
    struct ieee80211vap *vap;
    struct ieee80211com *ic;
    struct ieee80211_ath_channel *channel;
    struct wlan_objmgr_pdev *pdev;

    vap = wlan_vdev_get_mlme_ext_obj(vdev);
    if (vap == NULL) {
        offchan_err("VAP not in valid state");
        return -EINVAL;
    }
    ic = vap->iv_ic;
    if (ic->recovery_in_progress) {
        offchan_err("target recovery in progress, return");
        return -EINVAL;
    }
    pdev = ic->ic_pdev_obj;
    if(pdev == NULL) {
        return -EINVAL;
    }

    /*fail if CAC timer is running*/
    if (wlan_objmgr_pdev_try_get_ref(pdev, WLAN_DFS_ID) !=
            QDF_STATUS_SUCCESS) {
        return -EINVAL;
    }
    if (mlme_dfs_is_ap_cac_timer_running(pdev)) {
        offchan_err("cac timer still running, return");
        wlan_objmgr_pdev_release_ref(pdev, WLAN_DFS_ID);
        return -EINVAL;
    }
    wlan_objmgr_pdev_release_ref(pdev, WLAN_DFS_ID);

    /*fail if ioctl CSA's going on*/
    if((ic->ic_flags_ext2 & IEEE80211_FEXT2_CSA_WAIT) ||
        vap->channel_switch_state) {
        return -EINVAL;
    }
    /* check channel is supported */
    channel = ieee80211_find_dot11_channel(ic, chan, vap->iv_des_cfreq2, vap->iv_des_mode | ic->ic_chanbwflag);
    if (channel == NULL) {
        channel = ieee80211_find_dot11_channel(ic, chan, 0, IEEE80211_MODE_AUTO);

        if (channel == NULL) {
            qdf_err("Invalid Channel");
            return -EINVAL;
        }
    }

    if(IEEE80211_IS_CHAN_RADAR(channel))
    {
        qdf_err("Radar detected on channel %d.So, no Tx is allowed till the nol expiry",channel->ic_ieee);
        return -EINVAL;
    }

    if(!((dwell_time > 0) && (dwell_time < ieee80211_vap_get_beacon_interval(vap)))) {
        qdf_err("Dwell time is more than Beacon interval, Offchan scan duration is"
                " not allowed more than Beacon interval ");
        return -EINVAL;
    }

    return wlan_offchan_rx_test_send_req(vdev, channel->ic_freq, dwell_time, chan, bw_mode, sec_chan_offset);
}
