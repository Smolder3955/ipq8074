/*
 * Copyright (c) 2011-2018 Qualcomm Innovation Center, Inc.
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

#include <wbuf.h>
#include <ieee80211_var.h>
#include "wlan_mgmt_txrx_utils_api.h"
#include <wlan_son_pub.h>
#include <wlan_mlme_dp_dispatcher.h>
#include <ieee80211_wds.h>
#include <ol_if_athvar.h>
#include <wlan_reg_services_api.h>
#include <wlan_utility.h>

#ifdef QCA_SUPPORT_CP_STATS
#include <wlan_cp_stats_ic_utils_api.h>
#define WLAN_VAP_STATS(_vap, stat) (vdev_cp_stats_##stat##_inc(_vap->vdev_obj, 1))
#define WLAN_UCAST_STATS(_vap, stat) (vdev_ucast_cp_stats_##stat##_inc(_vap->vdev_obj, 1))
#define WLAN_MCAST_STATS(_vap, stat) (vdev_mcast_cp_stats_##stat##_inc(_vap->vdev_obj, 1))
#else
#define WLAN_VAP_STATS(_vap, stat) (qdf_info("cp stats is disabled"))
#define WLAN_UCAST_STATS(_vap, stat) (qdf_info("cp stats is disabled"))
#define WLAN_MCAST_STATS(_vap, stat) (qdf_info("cp stats is disabled"))
#endif

/*
 * Process a received frame if the opmode is AP
 * and returns back to ieee80211_input()
 */
static INLINE int
ieee80211_input_ap(struct ieee80211_node *ni, wbuf_t wbuf, struct ieee80211_frame *wh,
                                int type, int subtype, int dir)
{
    struct ieee80211com *ic = ni->ni_ic;
    struct ieee80211vap *vap = ni->ni_vap;
    u_int8_t *bssid;

    if (dir != IEEE80211_FC1_DIR_NODS)
        bssid = wh->i_addr1;
    else if (type == IEEE80211_FC0_TYPE_CTL)
        bssid = wh->i_addr1;
    else {
        if (wbuf_get_pktlen(wbuf) < sizeof(struct ieee80211_frame)) {
            IEEE80211_IS_MULTICAST(wh->i_addr1) ? WLAN_MCAST_STATS(vap, rx_discard) : WLAN_UCAST_STATS(vap, rx_discard);
            return 0;
        }
        bssid = wh->i_addr3;
    }
    if (type != IEEE80211_FC0_TYPE_DATA)
        return 1;
    /*
     * Data frame, validate the bssid.
     */
    if (!IEEE80211_ADDR_EQ(bssid, ieee80211_node_get_bssid(vap->iv_bss)) &&
        !IEEE80211_ADDR_EQ(bssid, IEEE80211_GET_BCAST_ADDR(ic)) &&
        subtype != IEEE80211_FC0_SUBTYPE_BEACON) {
        /* NAWDS repeaters send ADDBA with the BSSID
         * set to their BSSID and not ours
         * We should not drop those frames
         */
        if(!((ni->ni_flags & IEEE80211_NODE_NAWDS) &&
             (type == IEEE80211_FC0_TYPE_MGT) &&
             (subtype == IEEE80211_FC0_SUBTYPE_ACTION))) {
            /* not interested in */
            IEEE80211_IS_MULTICAST(wh->i_addr1) ? WLAN_MCAST_STATS(vap, rx_discard) : WLAN_UCAST_STATS(vap, rx_discard);

            return 0;
        }
    }
        return 1;
}

/*
 * Process received packet if opmode is STA, and return back to
 * ieee80211_input()
 */
int ieee80211_input_sta(struct ieee80211_node *ni, wbuf_t wbuf,struct ieee80211_rx_status *rs)
{
    struct ieee80211_frame *wh;
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211com *ic = ni->ni_ic;
    int type = -1, subtype, dir;
    u_int8_t *bssid;

	UNREFERENCED_PARAMETER(ic);

    wh = (struct ieee80211_frame *) wbuf_header(wbuf);
    type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
    subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;
    dir = wh->i_fc[1] & IEEE80211_FC1_DIR_MASK;

    bssid = wh->i_addr2;
    if (ni != vap->iv_bss && dir != IEEE80211_FC1_DIR_DSTODS)
        bssid = wh->i_addr3;

    if (!IEEE80211_ADDR_EQ(bssid, ieee80211_node_get_bssid(ni))) {
        IEEE80211_IS_MULTICAST(wh->i_addr1) ? WLAN_MCAST_STATS(vap, rx_discard) : WLAN_UCAST_STATS(vap, rx_discard);
        return 0;
    }

    /*
     * WAR for excessive beacon miss on SoC.
     * Reset bmiss counter when we receive a non-probe request
     * frame from our home AP, and save the time stamp.
     */
    if ((wlan_vdev_is_up(vap->vdev_obj) == QDF_STATUS_SUCCESS) &&
        (!((type == IEEE80211_FC0_TYPE_MGT) &&
        (subtype == IEEE80211_FC0_SUBTYPE_PROBE_REQ)))&&
        (vap->iv_lastbcn_phymode_mismatch == 0)) {

        if (vap->iv_bmiss_count > 0) {
#ifdef ATH_SWRETRY
            /* Turning on the sw retry mechanism. This should not
             * produce issues even if we are in the middle of
             * cleaning sw retried frames
             */
            if (ic->ic_set_swretrystate)
                ic->ic_set_swretrystate(vap->iv_bss, TRUE);
#endif
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
                              "clear beacon miss. frm type=%02x, subtype=%02x\n",
                              type, subtype);
            ieee80211_mlme_reset_bmiss(vap);
        }

        /*
         * Beacon timestamp will be set when beacon is processed.
         * Set directed frame timestamp if frame is not multicast or
         * broadcast.
         */
        if (! IEEE80211_IS_MULTICAST(wh->i_addr1)
#if ATH_SUPPORT_WRAP
            && IEEE80211_ADDR_EQ(wh->i_addr1, vap->iv_myaddr)
#endif
        ) {
            vap->iv_last_directed_frame = OS_GET_TIMESTAMP();
        }
    }
    return 1;
}

/*
 * Process received packet if opmode is IBSS, and return back to
 * ieee80211_input()
 */
int ieee80211_input_ibss(struct ieee80211_node *ni, wbuf_t wbuf,struct ieee80211_rx_status *rs)
{
    struct ieee80211_frame *wh;
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211com *ic = ni->ni_ic;
    int type = -1, subtype, dir;
    u_int8_t *bssid;

    wh = (struct ieee80211_frame *) wbuf_header(wbuf);
    type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
    subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;
    dir = wh->i_fc[1] & IEEE80211_FC1_DIR_MASK;

    if (dir != IEEE80211_FC1_DIR_NODS)
        bssid = wh->i_addr1;
    else if (type == IEEE80211_FC0_TYPE_CTL)
        bssid = wh->i_addr1;
    else {
        if (wbuf_get_pktlen(wbuf) < sizeof(struct ieee80211_frame)) {
            IEEE80211_IS_MULTICAST(wh->i_addr1) ? WLAN_MCAST_STATS(vap, rx_discard) : WLAN_UCAST_STATS(vap, rx_discard);
            return 0;
        }
        bssid = wh->i_addr3;
    }

    if (type != IEEE80211_FC0_TYPE_DATA)
        return 1;

    /*
     * Data frame, validate the bssid.
     */
    if (!IEEE80211_ADDR_EQ(bssid, ieee80211_node_get_bssid(vap->iv_bss)) &&
        !IEEE80211_ADDR_EQ(bssid, IEEE80211_GET_BCAST_ADDR(ic)) &&
        subtype != IEEE80211_FC0_SUBTYPE_BEACON) {
        /* not interested in */
        IEEE80211_IS_MULTICAST(wh->i_addr1) ? WLAN_MCAST_STATS(vap, rx_discard) : WLAN_UCAST_STATS(vap, rx_discard);
        return 0;
    }
    return 1;
}

/*
 * Process received packet if opmode is WDS, and return back to
 * ieee80211_input()
 */
int ieee80211_input_wds(struct ieee80211_node *ni, wbuf_t wbuf,struct ieee80211_rx_status *rs)
{
    struct ieee80211_frame *wh;
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211com *ic = ni->ni_ic;
    int type = -1, subtype;
    u_int8_t *bssid;

    wh = (struct ieee80211_frame *) wbuf_header(wbuf);
    type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
    subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;

    if (wbuf_get_pktlen(wbuf) < sizeof(struct ieee80211_frame_addr4)) {
        if ((type != IEEE80211_FC0_TYPE_MGT) &&
            (subtype != IEEE80211_FC0_SUBTYPE_DEAUTH)) {
            WLAN_VAP_STATS(vap, rx_tooshort);
            return 0;
        }
    }
    bssid = wh->i_addr1;
    if (!IEEE80211_ADDR_EQ(bssid, vap->iv_bss->ni_bssid) &&
        !IEEE80211_ADDR_EQ(bssid, IEEE80211_GET_BCAST_ADDR(ic))) {
        /* not interested in */
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "*** WDS *** "
                          "WDS ADDR1 %s, BSSID %s\n",
                          ether_sprintf(bssid),
                          ether_sprintf(ieee80211_node_get_bssid(vap->iv_bss)));
            WLAN_VAP_STATS(vap, rx_wrongbss);
        return 0;
    }
    return 1;
}





#ifndef ATH_HTC_MII_RXIN_TASKLET

#define IEEE80211_HANDLE_MGMT(_ic, _vap, _ni, _wbuf, _subtype, _type, _rs) \
        if (_vap->iv_input_mgmt_filter == NULL || \
                (_vap->iv_input_mgmt_filter && _vap->iv_input_mgmt_filter(_ni,_wbuf,_subtype,_rs) == 0)) {      \
            /*                                              \
             * if no filter function is installed (0)       \
             * if not filtered by the filter function       \
             * then process  management frame.              \
             */                                             \
            if(ieee80211_recv_mgmt(_ni, _wbuf, _subtype, _rs)){    \
                goto bad;                                   \
            }                                               \
        }                                                   \
        /*                                                  \
         *deliver the frame to the os. the handler cosumes the wbuf.    \
         **/                                                            \
        if (_vap->iv_evtable) {                                          \
            _vap->iv_evtable->wlan_receive(_vap->iv_ifp, _wbuf, _type, _subtype, _rs);    \
        }

#else

#define IEEE80211_HANDLE_MGMT( _ic, _vap, _ni, _wbuf, _subtype, _type,  _rs) \
        if (_vap->iv_input_mgmt_filter == NULL ||                                                \
            (_vap->iv_input_mgmt_filter && _vap->iv_input_mgmt_filter(_ni,_wbuf,_subtype,_rs) == 0))  \
      {                                                                                         \
            {                                                                                   \
                struct ieee80211_recv_mgt_args *temp_data ;                                     \
                temp_data = (ieee80211_recv_mgt_args_t *)qdf_nbuf_get_priv(_wbuf);               \
                qdf_mem_zero(temp_data,sizeof(struct ieee80211_recv_mgt_args));              \
                temp_data->ni = _ni;                                                             \
                temp_data->subtype = _subtype;                                                   \
                qdf_nbuf_queue_add(&_ic->ic_mgmt_nbufqueue, _wbuf);                               \
                if(atomic_read(&_ic->ic_mgmt_deferflags) != DEFER_PENDING){                      \
                    {                                                                           \
                        atomic_set(&_ic->ic_mgmt_deferflags, DEFER_PENDING);                     \
                        OS_PUT_DEFER_ITEM(_ic->ic_osdev,                                         \
                                ieee80211_recv_mgmt_defer,                                      \
                                WORK_ITEM_SINGLE_ARG_DEFERED,                                   \
                                _ic, NULL, NULL);                                                \
                    }                                                                           \
                }                                                                               \
            }                                                                                   \
     }
#endif


#ifndef ATH_HTC_MII_DISCARD_NETDUPCHECK
#define IEEE80211_CHECK_DUPPKT(_ic, _ni,  _type,_subtype,_dir, _wh, _phystats, _rs, _errorlable, _rxseqno)        \
        /* Check duplicates */                                                                          \
        if (HAS_SEQ(_type, _subtype)) {                                                                 \
            u_int8_t tid;                                                                               \
            if (IEEE80211_QOS_HAS_SEQ(wh)) {                                                            \
                if (dir == IEEE80211_FC1_DIR_DSTODS) {                                                  \
                    tid = ((struct ieee80211_qosframe_addr4 *)wh)->                                     \
                        i_qos[0] & IEEE80211_QOS_TID;                                                   \
                } else {                                                                                \
                    tid = ((struct ieee80211_qosframe *)wh)->                                           \
                        i_qos[0] & IEEE80211_QOS_TID;                                                   \
                }                                                                                       \
                if (TID_TO_WME_AC(tid) >= WME_AC_VI)                                                    \
                    ic->ic_wme.wme_hipri_traffic++;                                                     \
            } else {                                                                                    \
                if (type == IEEE80211_FC0_TYPE_MGT)                                                     \
                    tid = IEEE80211_TID_SIZE; /* use different pool for rx mgt seq number */            \
                else                                                        \
                    tid = IEEE80211_NON_QOS_SEQ;                            \
            }                                                               \
                                                                            \
            _rxseqno = le16toh(*(u_int16_t *)wh->i_seq);                       \
            if ((_wh->i_fc[1] & IEEE80211_FC1_RETRY) &&                     \
                (_rxseqno == _ni->ni_rxseqs[tid])) {                           \
                WLAN_PHY_STATS(_phystats, ips_rx_dup);                       \
                                                                            \
                if (_ni->ni_last_rxseqs[tid] == _ni->ni_rxseqs[tid]) {      \
                    WLAN_PHY_STATS(_phystats, ips_rx_mdup);                 \
                }                                                           \
                _ni->ni_last_rxseqs[tid] = _ni->ni_rxseqs[tid];             \
                goto _errorlable;                                           \
            }                                                               \
            _ni->ni_rxseqs[tid] = _rxseqno;                                    \
                                                                            \
        }
#else
   #define IEEE80211_CHECK_DUPPKT(_ic, _ni,  _type,_subtype,_dir, _wh, _phystats, _rs, _errorlable)
#endif

/*
 * Process a received frame.  The node associated with the sender
 * should be supplied.  If nothing was found in the node table then
 * the caller is assumed to supply a reference to iv_bss instead.
 * The RSSI and a timestamp are also supplied.  The RSSI data is used
 * during AP scanning to select a AP to associate with; it can have
 * any units so long as values have consistent units and higher values
 * mean ``better signal''.
 */
void ieee80211_sta2ibss_header(struct ieee80211_frame* wh){
    unsigned char DA[IEEE80211_ADDR_LEN];
    unsigned char SA[IEEE80211_ADDR_LEN];
    unsigned char BSSID[IEEE80211_ADDR_LEN];
    int i;

    wh->i_fc[1] &= ~IEEE80211_FC1_DIR_MASK;
    wh->i_fc[1] |= IEEE80211_FC1_DIR_NODS;
    for(i=0;i<IEEE80211_ADDR_LEN;i++){
        BSSID[i]=wh->i_addr1[i];
        SA[i]=wh->i_addr2[i];
        DA[i]=wh->i_addr3[i];
    }
    for(i=0;i<IEEE80211_ADDR_LEN;i++){
        wh->i_addr1[i]=DA[i];
        wh->i_addr2[i]=SA[i];
        wh->i_addr3[i]=BSSID[i];
    }
}

#define WLAN_VAP_LASTDATATSTAMP(_vap, _tstamp) \
              _vap->iv_lastdata = _tstamp;
#define WLAN_VAP_TXRXBYTE(_vap,_wlen) \
           _vap->iv_txrxbytes += _wlen;
#define WLAN_VAP_TRAFIC_INDICATION(_vap , _is_bcast, _subtype) \
         if (!_is_bcast && IEEE80211_CONTAIN_DATA(_subtype)) {  \
                     _vap->iv_last_traffic_indication = OS_GET_TIMESTAMP(); \
                   }

#if !UMAC_SUPPORT_OPMODE_APONLY
/*
 * Process received data packet if opmode is STA, and return back to
 * ieee80211_input_data()
 */
int
ieee80211_input_data_sta(struct ieee80211_node *ni, wbuf_t wbuf, int dir, int subtype)
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211com *ic = ni->ni_ic;
    struct ieee80211_node *ni_wds=NULL;
    struct ieee80211_node *temp_node=NULL;
    struct ieee80211_frame *wh;
    int is_mcast;

    wh = (struct ieee80211_frame *) wbuf_header(wbuf);
    is_mcast = (dir == IEEE80211_FC1_DIR_DSTODS ||
                dir == IEEE80211_FC1_DIR_TODS ) ?
        IEEE80211_IS_MULTICAST(IEEE80211_WH4(wh)->i_addr3) :
        IEEE80211_IS_MULTICAST(wh->i_addr1);

    /*
     * allow all frames with FromDS bit set .
     * allow Frames with DStoDS if the vap is WDS capable.
     */
    if (!(dir == IEEE80211_FC1_DIR_FROMDS ||
          (dir == IEEE80211_FC1_DIR_DSTODS && (IEEE80211_VAP_IS_WDS_ENABLED(vap))))) {
        if (dir == IEEE80211_FC1_DIR_DSTODS) {
            IEEE80211_DISCARD(vap,
                              IEEE80211_MSG_INPUT |
                              IEEE80211_MSG_WDS, wh,
                              "4-address data",
                              "%s", "WDS not enabled");
            WLAN_VAP_STATS(vap, rx_nowds);
        } else {
            IEEE80211_DISCARD(vap,
                              IEEE80211_MSG_INPUT, wh,
                              "data",
                              "invalid dir 0x%x", dir);
            WLAN_VAP_STATS(vap, rx_wrongdir);
        }
	is_mcast ? WLAN_MCAST_STATS(vap, rx_discard): WLAN_UCAST_STATS(vap, rx_discard);
        return 0;
    }

    if (dir == IEEE80211_FC1_DIR_DSTODS){
        struct ieee80211_node_table *nt;
        struct ieee80211_frame_addr4 *wh4;
        wh4 = (struct ieee80211_frame_addr4 *) wbuf_header(wbuf);
        nt = &ic->ic_sta;
        ni_wds = ieee80211_find_wds_node(nt, wh4->i_addr4);

        if (ni_wds == NULL)
        {
        /*
         * In STA mode, add wds entries for hosts behind us, but
         * not for hosts behind the rootap.
         */
            if (!IEEE80211_ADDR_EQ(wh4->i_addr2, vap->iv_bss->ni_bssid))
            {
                temp_node = ieee80211_find_node(nt,wh4->i_addr4);
                if (temp_node == NULL)
                {
                    ieee80211_add_wds_addr(vap, nt, ni, wh4->i_addr4,
                                        IEEE80211_NODE_F_WDS_REMOTE);
                }
                else if (!IEEE80211_ADDR_EQ(temp_node->ni_macaddr, vap->iv_myaddr))
                {
                    ieee80211_free_node(temp_node);
                    ieee80211_add_wds_addr(vap, nt, ni, wh4->i_addr4,
                                IEEE80211_NODE_F_WDS_REMOTE);
                }
            }
        }
        else
        {
            ieee80211_remove_wds_addr(vap, nt,wh4->i_addr4,IEEE80211_NODE_F_WDS_BEHIND|IEEE80211_NODE_F_WDS_REMOTE);
            ieee80211_free_node(ni_wds);
        }
    }
    return 1;
}

/*
 * Process data frames if the opmode is AP,
 * and return to ieee80211_input_data()
 */
static INLINE int
ieee80211_input_data_ap(struct ieee80211_node *ni, wbuf_t wbuf, struct ieee80211_frame *wh, int dir)
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211com *ic = ni->ni_ic;
    /*
     * allow all frames with ToDS bit set .
     * allow Frames with DStoDS if the vap is WDS capable.
     */
    if (!((dir == IEEE80211_FC1_DIR_TODS) ||
          (dir == IEEE80211_FC1_DIR_DSTODS && (IEEE80211_VAP_IS_WDS_ENABLED(vap))))) {
        if (dir == IEEE80211_FC1_DIR_DSTODS) {
            IEEE80211_DISCARD(vap,
                              IEEE80211_MSG_INPUT |
                              IEEE80211_MSG_WDS, wh,
                              "4-address data",
                              "%s", "WDS not enabled");
            WLAN_VAP_STATS(vap, rx_nowds);
        } else {
            IEEE80211_DISCARD(vap,
                              IEEE80211_MSG_INPUT, wh,
                              "data",
                              "invalid dir 0x%x", dir);

            WLAN_VAP_STATS(vap, rx_wrongdir);
        }
        return 0;
    }
#if UMAC_SUPPORT_NAWDS
    /* if NAWDS learning feature is enabled, add the mac to NAWDS table */
    if ((ni == vap->iv_bss) &&
        (dir == IEEE80211_FC1_DIR_DSTODS) &&
        (ieee80211_nawds_enable_learning(vap))) {
         IEEE80211_NAWDS_LEARN(vap, wh->i_addr2);
        /* current node is bss node so drop it to avoid sending dis-assoc. packet */
        return 0;
    }
#endif /* UMAC_SUPPORT_NAWDS */
    /* check if source STA is associated */
    if (ni == vap->iv_bss) {
        IEEE80211_DISCARD(vap, IEEE80211_MSG_INPUT,
                          wh, "data", "%s", "unknown src");

        /* NB: caller deals with reference */

        /*  the following WIFIPOS logic is WAR, the WAR is due to the data frames
            are received by the off-channel AP, which is causing the AP
            to send the deauth packets, whcih is eating the BW.
            So, to avoid this we have a WAR which checks if we are in
            off-channel and if we are then we don't send the deauth
            packets.
        */
#if ATH_SUPPORT_WIFIPOS
        if ((wlan_vdev_is_up(vap->vdev_obj) == QDF_STATUS_SUCCESS) &&
            (ic->ic_isoffchan == 0) && !IEEE80211_IS_MULTICAST(wh->i_addr2))
#else
        if ((wlan_vdev_is_up(vap->vdev_obj) == QDF_STATUS_SUCCESS) &&
            !IEEE80211_IS_MULTICAST(wh->i_addr2))
#endif
        {
            ni = ieee80211_tmp_node(vap, wh->i_addr2);
            if (ni != NULL) {
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_AUTH, "%s: sending DEAUTH to %s, unknown src reason %d\n",
                        __func__, ether_sprintf(wh->i_addr2), IEEE80211_REASON_NOT_AUTHED);
                ieee80211_send_deauth(ni, IEEE80211_REASON_NOT_AUTHED);
                IEEE80211_DELIVER_EVENT_MLME_DEAUTH_INDICATION(vap, (wh->i_addr2), 0,
                                           IEEE80211_REASON_NOT_AUTHED);


                /* claim node immediately */
                wlan_objmgr_delete_node(ni);
            }
        }
        WLAN_VAP_STATS(vap, rx_not_assoc);
        return 0;
    }

    if (ni->ni_associd == 0) {
        IEEE80211_DISCARD(vap, IEEE80211_MSG_INPUT,
                          wh, "data", "%s", "unassoc src");
        ieee80211_send_disassoc(ni, IEEE80211_REASON_NOT_ASSOCED);
        IEEE80211_DELIVER_EVENT_MLME_DISASSOC_INDICATION(vap, (wh->i_addr2),
                                          0, IEEE80211_REASON_NOT_ASSOCED);
        WLAN_VAP_STATS(vap, rx_not_assoc);
        return 0;
    }


        /* If we're a 4 address packet, make sure we have an entry in
            the node table for the packet source address (addr4).  If not,
            add one */
        if (dir == IEEE80211_FC1_DIR_DSTODS){
            wds_update_rootwds_table(vap,ni,&ic->ic_sta, wbuf);
        }

        return 1;
}
#endif
 /*
  * processes data frames.
  * ieee80211_input_data consumes the wbuf .
  */
static void
ieee80211_input_data(struct ieee80211_node *ni, wbuf_t wbuf, struct ieee80211_rx_status *rs, int subtype, int dir)
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211com *ic = ni->ni_ic;
    struct ieee80211_phy_stats *phy_stats;
    struct ieee80211_frame *wh;
    int is_mcast, is_bcast;

    rs->rs_cryptodecapcount = 0;
    rs->rs_qosdecapcount = 0;

    wh = (struct ieee80211_frame *) wbuf_header(wbuf);
    is_mcast = (dir == IEEE80211_FC1_DIR_DSTODS ||
                dir == IEEE80211_FC1_DIR_TODS ) ?
        IEEE80211_IS_MULTICAST(IEEE80211_WH4(wh)->i_addr3) :
        IEEE80211_IS_MULTICAST(wh->i_addr1);
    is_bcast = (dir == IEEE80211_FC1_DIR_FROMDS) ? IEEE80211_IS_BROADCAST(wh->i_addr1) : FALSE;


#if !ATH_SUPPORT_STATS_APONLY
    phy_stats = &ic->ic_phy_stats[vap->iv_cur_mode];
#endif
        UNREFERENCED_PARAMETER(phy_stats);

#if UMAC_PER_PACKET_DEBUG
       IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "  %d %d %d     %d %d %d %d %d %d       "
                                    "  %d %d  \n\n",
                                         rs->rs_lsig[0],
                                         rs->rs_lsig[1],
                                         rs->rs_lsig[2],
                                         rs->rs_htsig[0],
                                         rs->rs_htsig[1],
                                         rs->rs_htsig[2],
                                         rs->rs_htsig[3],
                                         rs->rs_htsig[4],
                                         rs->rs_htsig[5],
                                         rs->rs_servicebytes[0],
                                         rs->rs_servicebytes[1]);
#endif

    WLAN_VAP_LASTDATATSTAMP(vap,  OS_GET_TIMESTAMP());
    WLAN_PHY_STATS(phy_stats, ips_rx_fragments);
    WLAN_VAP_TXRXBYTE(vap,wbuf_get_pktlen(wbuf));


    /*
     * Store timestamp for actual (non-NULL) data frames.
     * This provides other modules such as SCAN and LED with correct
     * information about the actual data traffic in the system.
     * We don't take broadcast traffic into consideration.
     */
    WLAN_VAP_TRAFIC_INDICATION(vap , is_bcast, subtype);


    switch (vap->iv_opmode) {
    case IEEE80211_M_STA:
                if (ieee80211_input_data_sta(ni, wbuf, dir, subtype) == 0) {
                        goto bad;
                }
        break;

    case IEEE80211_M_HOSTAP:
        if (likely(ic->ic_curchan == vap->iv_bsschan)) {
                    if (ieee80211_input_data_ap(ni, wbuf, wh, dir) == 0) {
                            goto bad;
                    }
        } else
            goto bad;
        break;

    default:
        break;
    }

    if (subtype == IEEE80211_FC0_SUBTYPE_NODATA || subtype == IEEE80211_FC0_SUBTYPE_QOS_NULL) {
        /* no need to process the null data frames any further */
        goto bad;
    }
bad:
/*  FIX ME: linux specific netdev struct iv_destats has to be replaced*/
//    vap->iv_devstats.rx_errors++;
    wbuf_free(wbuf);
}
int
ieee80211_input(struct ieee80211_node *ni, wbuf_t wbuf, struct ieee80211_rx_status *rs)
{
#define QOS_NULL   (IEEE80211_FC0_TYPE_DATA | IEEE80211_FC0_SUBTYPE_QOS_NULL)
#define HAS_SEQ(type, subtype)   (((type & 0x4) == 0) && ((type | subtype) != QOS_NULL))
    struct ieee80211com *ic = ni->ni_ic;
    struct ieee80211_frame *wh;
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211_phy_stats *phy_stats;
    int type = -1, subtype, dir;
#if UMAC_SUPPORT_WNM
    u_int8_t secured;
#endif
    u_int16_t rxseq =0;
    u_int8_t *bssid;
    bool rssi_update = true;

    KASSERT((wbuf_get_pktlen(wbuf) >= ic->ic_minframesize),
            ("frame length too short: %u", wbuf_get_pktlen(wbuf)));

    wlan_wbuf_set_peer_node(wbuf, ni);

    if (ieee80211_vap_stop_bss_is_set(vap)) {
        goto bad1;
    }
    if (wbuf_get_pktlen(wbuf) < ic->ic_minframesize) {
        goto bad1;
    }

    wh = (struct ieee80211_frame *) wbuf_header(wbuf);
    if ((wh->i_fc[0] & IEEE80211_FC0_VERSION_MASK) != IEEE80211_FC0_VERSION_0) {
        /* XXX: no stats for it. */
        goto bad1;
    }

    type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
    subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;
    dir = wh->i_fc[1] & IEEE80211_FC1_DIR_MASK;

    if (OS_ATOMIC_CMPXCHG(&vap->iv_rx_gate, 0, 1) != 0) {
        goto bad1;
    }
    /* Allow mgmt frames when ACS or external channel selection is in progress
     * in AP mode*/
    if ((wlan_vdev_chan_config_valid(vap->vdev_obj) != QDF_STATUS_SUCCESS) ||
          ((vap->iv_opmode == IEEE80211_M_HOSTAP) && (vap->iv_vap_is_down) &&
                !(wlan_autoselect_in_progress(vap)) &&
                !(ieee80211_vap_ext_acs_inprogress_is_set(vap)))) {
        if (vap->iv_input_mgmt_filter && type == IEEE80211_FC0_TYPE_MGT && IEEE80211_ADDR_EQ(wh->i_addr1, vap->iv_myaddr) ) {
              vap->iv_input_mgmt_filter(ni,wbuf,subtype,rs) ;
        }
        if ((!ic->ic_nl_handle ||
            !ieee80211_vap_dfswait_is_set(vap) ||
            type != IEEE80211_FC0_TYPE_MGT ||
            (subtype != IEEE80211_FC0_SUBTYPE_BEACON &&
            subtype != IEEE80211_FC0_SUBTYPE_PROBE_RESP)) &&
            vap->iv_opmode != IEEE80211_M_STA &&
            vap->iv_mgmt_offchan_current_req.request_type !=
            IEEE80211_OFFCHAN_RX) {
                goto bad;
        }
    }

    /* Mark node as WDS */
    if (dir == IEEE80211_FC1_DIR_DSTODS)
            ieee80211node_set_flag(ni, IEEE80211_NODE_WDS);


#if !ATH_SUPPORT_STATS_APONLY
    phy_stats = &ic->ic_phy_stats[vap->iv_cur_mode];
#endif
    UNREFERENCED_PARAMETER(phy_stats);

    /*
     * XXX Validate received frame if we're not scanning.
     * why do we receive only data frames when we are scanning and
     * current (foreign channel) channel is the bss channel ?
     * should we simplify this to if (vap->iv_bsschan == ic->ic_curchan) ?
     */
    if (wlan_scan_in_home_channel(wlan_vdev_get_pdev(vap->vdev_obj)))
     {
        switch (vap->iv_opmode) {
        case IEEE80211_M_STA:
            if (ieee80211_input_sta(ni, wbuf, rs) == 0) {
                if (subtype != IEEE80211_FC0_SUBTYPE_ACTION &&
                    vap->iv_opmode != IEEE80211_M_STA &&
                    vap->iv_mgmt_offchan_current_req.request_type !=
                    IEEE80211_OFFCHAN_RX) {
                    goto bad;
                }
            }
            break;
        case IEEE80211_M_IBSS:
        case IEEE80211_M_AHDEMO:
            if (ieee80211_input_ibss(ni, wbuf, rs) == 0) {
                goto bad;
            }
            if (dir != IEEE80211_FC1_DIR_NODS)
                bssid = wh->i_addr1;
            else if (type == IEEE80211_FC0_TYPE_CTL)
                bssid = wh->i_addr1;
            else {
                if (wbuf_get_pktlen(wbuf) < sizeof(struct ieee80211_frame)) {
                    IEEE80211_IS_MULTICAST(wh->i_addr1) ? WLAN_MCAST_STATS(vap, rx_discard) : WLAN_UCAST_STATS(vap, rx_discard);
                    goto bad;
                }
                bssid = wh->i_addr3;
            }
            if ((wlan_vdev_is_up(vap->vdev_obj) == QDF_STATUS_SUCCESS) &&
                !IEEE80211_ADDR_EQ(bssid, ieee80211_node_get_bssid(vap->iv_bss))) {
                rssi_update = false;
            }
            break;
        case IEEE80211_M_HOSTAP:
            {
                if (ieee80211_input_ap(ni, wbuf, wh, type, subtype, dir) == 0) {
                    goto bad;
                }
            }
            break;

        case IEEE80211_M_WDS:
            if (ieee80211_input_wds(ni, wbuf, rs) == 0) {
                goto bad;
            }
            break;
        case IEEE80211_M_BTAMP:
            break;

        default:
            goto bad;
        }

        if(rssi_update && rs->rs_isvalidrssi)
            ni->ni_rssi = rs->rs_rssi;

        IEEE80211_CHECK_DUPPKT(ic, ni,  type, subtype, dir, wh, phy_stats, rs, bad, rxseq);

        }

#if UMAC_SUPPORT_WNM
    if (ieee80211_vap_wnm_is_set(vap)) {
        secured = wh->i_fc[1] & IEEE80211_FC1_WEP;
        ieee80211_wnm_bssmax_updaterx(ni, secured);
    }
#endif
    /* For 11ac offload powersave is handled in Target FW */
    if (!ic->ic_is_mode_offload(ic))
    {
        /*
         * Check for power save state change.
         */
        if ((vap->iv_opmode == IEEE80211_M_HOSTAP) &&
            (ni != vap->iv_bss) &&
            !(type == IEEE80211_FC0_TYPE_MGT && subtype == IEEE80211_FC0_SUBTYPE_PROBE_REQ))
        {

            if ((type == IEEE80211_FC0_TYPE_CTL) && (subtype ==  IEEE80211_FC0_SUBTYPE_PS_POLL)) {

                if ((ni->ni_flags & IEEE80211_NODE_PWR_MGT)) {

                    if( !(wh->i_fc[1] & IEEE80211_FC1_PWR_MGT)){
                        wh->i_fc[1] |= IEEE80211_FC1_PWR_MGT;
                        IEEE80211_DPRINTF(vap, IEEE80211_MSG_POWER,
                              "[%s]PS poll with PM bit clear in sleep state,Keep continue sleep SM\n",__func__);
                    }

                } else {

                    if ( !(wh->i_fc[1] & IEEE80211_FC1_PWR_MGT)) {
                       IEEE80211_DPRINTF(vap, IEEE80211_MSG_POWER,
                                    "[%s]Rxed PS poll with PM set in awake node state,drop packet\n",__func__);
                        goto bad;
                    }
                }
            }

            if ((wh->i_fc[1] & IEEE80211_FC1_PWR_MGT) ^
                (ni->ni_flags & IEEE80211_NODE_PWR_MGT)) {
                ic->ic_node_psupdate(ni, wh->i_fc[1] & IEEE80211_FC1_PWR_MGT, 0);
                ieee80211_mlme_node_pwrsave(ni, wh->i_fc[1] & IEEE80211_FC1_PWR_MGT);
            }
        }
    }

    if (type == IEEE80211_FC0_TYPE_DATA) {
        if (wlan_vdev_is_up(vap->vdev_obj) != QDF_STATUS_SUCCESS) {
            goto bad;
        }
        /* ieee80211_input_data consumes the wbuf */
        ieee80211_node_activity(ni); /* node has activity */
#if ATH_SW_WOW
        if (wlan_get_wow(vap)) {
            ieee80211_wow_magic_parser(ni, wbuf);
        }
#endif
        ieee80211_input_data(ni, wbuf, rs, subtype, dir);
    } else if (type == IEEE80211_FC0_TYPE_MGT) {
        /* check for ACL policy for blocking mgmt  and drop frame */
#if QCA_SUPPORT_SON
        if (wlan_vdev_acl_is_drop_mgmt_set(vap->vdev_obj, wh->i_addr2)) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_ACL,
                              "[%s] MGMT frame blocked by ACL \n",
                              ether_sprintf(wh->i_addr2));
            goto bad;
        }
#endif
		/* ieee80211_recv_mgmt does not consume the wbuf */
		if (subtype != IEEE80211_FC0_SUBTYPE_PROBE_REQ) {
			ieee80211_node_activity(ni); /* node has activity */
		}
#if 0 /*TBD_DADP*/
	   if (subtype == IEEE80211_FC0_SUBTYPE_BEACON) {
			if (vap->iv_txrx_event_info.iv_txrx_event_filter & IEEE80211_VAP_INPUT_EVENT_BEACON) {
				ieee80211_vap_txrx_event    evt;
				OS_MEMZERO(&evt, sizeof(evt));
				evt.type = IEEE80211_VAP_INPUT_EVENT_BEACON;
				evt.ni   = ni;
				ieee80211_vap_txrx_deliver_event(vap,&evt);
			}
		}
#endif
#ifdef QCA_SUPPORT_CP_STATS
                peer_cp_stats_rx_mgmt_inc(ni->peer_obj, 1);
#endif

		IEEE80211_HANDLE_MGMT(ic,vap,ni, wbuf, subtype,type,  rs);
    } else if (type == IEEE80211_FC0_TYPE_CTL) {

        WLAN_VAP_STATS(vap, rx_ctl);
        ieee80211_recv_ctrl(ni, wbuf, subtype, rs);
        /*
         * deliver the frame to the os. the handler cosumes the wbuf.
         */
        if (vap->iv_evtable) {
            vap->iv_evtable->wlan_receive(vap->iv_ifp, wbuf, type, subtype, rs);
        }
    } else {
        goto bad;
    }

    (void) OS_ATOMIC_CMPXCHG(&vap->iv_rx_gate, 1, 0);

    if(wlan_pdev_nif_feat_cap_get(ic->ic_pdev_obj, WLAN_PDEV_F_MBSS_IE_ENABLE)) {
        if (IEEE80211_IS_PROBEREQ(wh) && IEEE80211_IS_BROADCAST(wh->i_addr3)) {
            vap->iv_mbss.mbssid_send_bcast_probe_resp = 1;
	}
    }

    return type;

bad:
    (void) OS_ATOMIC_CMPXCHG(&vap->iv_rx_gate, 1, 0);
bad1:
    if(wbuf_next(wbuf) != NULL) {
       wbuf_t wbx = wbuf;
       while (wbx) {
           wbuf = wbuf_next(wbx);
           wbuf_free(wbx);
           wbx = wbuf;
       }
    }
    else {
        wbuf_free(wbuf);
    }

    if(wlan_pdev_nif_feat_cap_get(ic->ic_pdev_obj, WLAN_PDEV_F_MBSS_IE_ENABLE)) {
        vap->iv_mbss.mbssid_send_bcast_probe_resp = 0;
    }

    return type;

#undef HAS_SEQ
#undef QOS_NULL
}

struct ieee80211_iter_input_all_arg {
    wbuf_t wbuf;
    struct ieee80211_rx_status *rs;
    int type;
};
static INLINE void
ieee80211_iter_input_all(void *arg, struct ieee80211vap *vap, bool is_last_vap)
{
    struct ieee80211_iter_input_all_arg *params = (struct ieee80211_iter_input_all_arg *) arg;
    struct ieee80211_node *ni;
    wbuf_t wbuf1;

    if (!vap) {
        printk("Already Vap is deleted!!\n");
        return;
    }

    if (vap->iv_opmode == IEEE80211_M_MONITOR ||
        ((wlan_vdev_chan_config_valid(vap->vdev_obj) != QDF_STATUS_SUCCESS) &&
        !vap->iv_input_mgmt_filter &&
        (vap->iv_opmode != IEEE80211_M_STA) &&
        (vap->iv_mgmt_offchan_current_req.request_type != IEEE80211_OFFCHAN_RX) &&
        (!vap->iv_ic->ic_nl_handle || !ieee80211_vap_dfswait_is_set(vap)))) {
        return;
    }

    ni = vap->iv_bss;
    if(!ni)
        return;

    if (!is_last_vap) {
        wbuf1 = wbuf_clone(vap->iv_ic->ic_osdev, params->wbuf);
        if (wbuf1 == NULL) {
            /* XXX stat+msg */
            return;
        }
    } else {
        wbuf1 = params->wbuf;
        params->wbuf = NULL;
    }

    params->type = ieee80211_input(ni, wbuf1, params->rs);
}

int
ieee80211_input_all(struct ieee80211com *ic,
    wbuf_t wbuf, struct ieee80211_rx_status *rs)
{
    struct ieee80211_iter_input_all_arg params;
    u_int32_t num_vaps;
    struct ieee80211_node *ni = NULL;
    struct ieee80211_frame *wh = (struct ieee80211_frame *)wbuf_header(wbuf);
    struct ieee80211_framing_extractx extractx;
    struct ieee80211vap *transmit_vap = NULL;
    struct ieee80211vap *tmpvap = NULL;
    bool send_probe_resp = false;

    params.wbuf = wbuf;
    params.rs = rs;
    params.type = -1;

    ieee80211_iterate_vap_list_internal(ic,ieee80211_iter_input_all,(void *)&params,num_vaps);

    if (wlan_pdev_nif_feat_cap_get(ic->ic_pdev_obj, WLAN_PDEV_F_MBSS_IE_ENABLE)) {

        TAILQ_FOREACH(tmpvap, &ic->ic_vaps, iv_next) {
            if(tmpvap->iv_mbss.mbssid_send_bcast_probe_resp) {
	        send_probe_resp = true;
	    }
	}

       transmit_vap = ic->ic_mbss.transmit_vap;
       if (!transmit_vap) {
           goto out;
       }

       if (send_probe_resp && transmit_vap->iv_mbss.mbssid_send_bcast_probe_resp) {
           ni = transmit_vap->iv_bss;
	   if (!ni)
	       goto out;

	   OS_MEMZERO(&extractx, sizeof(extractx));
           ieee80211_send_proberesp(ni, wh->i_addr2, NULL, 0, &extractx);
       }
    }

out:

    if (params.wbuf != NULL)        /* no vaps, reclaim wbuf */
        wbuf_free(params.wbuf);
    return params.type;
}


#if ATH_SUPPORT_IWSPY
int ieee80211_input_iwspy_update_rssi(struct ieee80211com *ic, u_int8_t *address, int8_t rssi)
{
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);

	if (vap)
	{
		vap->iv_evtable->wlan_iwspy_update(vap->iv_ifp, address, rssi);
	}

	return 0;
}
#endif

/*
 * This Function is the entry-point from the mgmt TxRx
 * layer to umac layer.
 * It will in turn invoke the legacy input() API for
 * further processing.
 */
QDF_STATUS
ieee80211_mgmt_input(struct wlan_objmgr_psoc *psoc,
                     struct wlan_objmgr_peer *peer,
                     wbuf_t wbuf,
                     struct mgmt_rx_event_params *mgmt_rx_params,
                     enum mgmt_frame_type txrx_type)
{
    struct ieee80211_rx_status *rx_status = (struct ieee80211_rx_status *)mgmt_rx_params->rx_params;
    struct ieee80211_node *ni = (struct ieee80211_node *)rx_status->rs_ni;
    struct ieee80211_frame *wh = (struct ieee80211_frame *)wbuf_header(wbuf);
    struct ieee80211com *ic = (struct ieee80211com *)rx_status->rs_ic;

#ifndef QCA_WIFI_QCA8074_VP
    /* TODO : The 4.9GHz check below is a temporary fix. As soon
     * as FW fixes the negative rssi issue we need to remove this.
     */
    if(!WLAN_REG_IS_49GHZ_FREQ(ic->ic_curchan->ic_freq)) {
        if (rx_status->rs_rssi < ic->mgmt_rx_rssi) {
            if(wbuf_next(wbuf) != NULL) {
                wbuf_t wbx = wbuf;
                while (wbx) {
                    wbuf = wbuf_next(wbx);
                    wbuf_free(wbx);
                    wbx = wbuf;
                }
            } else {
                wbuf_free(wbuf);
            }
            if (ic->ic_is_mode_offload(ic)) {
#ifdef QCA_SUPPORT_CP_STATS
                pdev_cp_stats_rx_mgmt_rssi_drop_inc(ic->ic_pdev_obj, 1);
#endif
            }
            return QDF_STATUS_SUCCESS;
        }
    }
#endif
    if (!ni || (IEEE80211_IS_PROBEREQ(wh) && IEEE80211_IS_BROADCAST(wh->i_addr3))) {
        ieee80211_input_all(ic, wbuf, rx_status);
    } else {
        ieee80211_input(ni, wbuf, rx_status);
    }

    return QDF_STATUS_SUCCESS;
}

