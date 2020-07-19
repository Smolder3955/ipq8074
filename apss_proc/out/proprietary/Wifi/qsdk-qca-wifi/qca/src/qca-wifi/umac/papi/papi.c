/*
 * @@-COPYRIGHT-START-@@
 *
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * @@-COPYRIGHT-END-@@
 */

/*
 * Parameter api module
 */

#include <ieee80211_var.h>
#include <qdf_trace.h>

/*
 * @brief Inform user-space application that a sta is
 *        now associated/disassociated
 *
 * @param [in] vap     the VAP on which sta associated/disassociated
 * @param [in] ni      the sta who associated/disassociate
 * @param [in] event   event of type PAPI_STA_EVENT
 */
void ieee80211_papi_send_assoc_event(struct ieee80211vap *vap,
                                     const struct ieee80211_node *ni,
                                     u_int8_t event)
{
    struct papi_associated_sta_report_ind assoc;

    if(!vap || !vap->iv_ic->ic_papi_enable || wlan_vap_get_opmode(vap) != IEEE80211_M_HOSTAP ||
        ieee80211_vap_deleted_is_set(vap) || !ni) {
        /*QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_NONE,
             "%s: Failed to send assoc event\n",__func__);*/
        return;
    }

    OS_MEMCPY(assoc.mac, ni->ni_macaddr, IEEE80211_ADDR_LEN);
    assoc.sta_event = event;
    assoc.time_stamp = OS_GET_TIMESTAMP();

    IEEE80211_DELIVER_PAPI_EVENT(vap, PAPI_ASSOCIATED_STA_REPORT,
                                 sizeof(assoc),
                                 (const char *)&assoc);
}

/*
 * @brief Inform user-space application that a sta sent
 *        neighbor report request
 *
 * @param [in] vap     the VAP on which request received
 * @param [in] ni      the sta who sent the request
 * @param [in] event   event of type PAPI_STA_EVENT
 */
void ieee80211_papi_send_nrreq_event(struct ieee80211vap *vap,
                                     const struct ieee80211_node *ni,
                                     u_int8_t event)
{
    ieee80211_papi_send_assoc_event(vap, ni, event);
}

/*
 * @brief Inform user-space application about received
 *        probe request from non associated stations
 *
 * @param [in] vap     the VAP on which probe request received
 * @param [in] ni      the sta who sent the probe request
 * @param [in] wbuf    probe request buffer
 * @param [in] rs      rx status
 */
void ieee80211_papi_send_probe_req_event(struct ieee80211vap *vap,
                                         const struct ieee80211_node *ni,
                                         wbuf_t wbuf,
                                         struct ieee80211_rx_status *rs)
{
    struct papi_non_associated_sta_report_ind probe;
    struct ieee80211com *ic = ni->ni_ic;
    u_int8_t *ssid, ssidlen;
    u_int8_t *frm, *efrm;
    struct ieee80211_frame *wh;
    struct ieee80211_node *sender_ni;

    if(!vap || !vap->iv_ic->ic_papi_enable || wlan_vap_get_opmode(vap) != IEEE80211_M_HOSTAP ||
        ieee80211_vap_deleted_is_set(vap) || !wbuf || !rs) {
        /*QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_NONE,
             "%s: Failed to send non-assoc event\n",__func__);*/
        return;
    }

    /* check if ni is vap's self node, if true then this must
       be either broadcast bssid probe or probe from non asssoc sta */
    if (ni == vap->iv_bss) {
        /* if its bcast bssid probe then this can be sent from
           one of the associated sta. If so then we won't consider
           this as we are looking out for non associated sta */
        sender_ni = ieee80211_find_rxnode(ic, (struct ieee80211_frame_min *)
                                          wbuf_header(wbuf));
        if (sender_ni) {
            /* probe from associated sta ignore */
            ieee80211_free_node(sender_ni);
            return;
        }
        /* probe req from non associated STA */
        wh = (struct ieee80211_frame *) wbuf_header(wbuf);
        frm = (u_int8_t *)&wh[1];
        efrm = wbuf_header(wbuf) + wbuf_get_pktlen(wbuf);

        ssid = NULL;
        ssidlen = 0;
        while (((frm+1) < efrm) && (frm + frm[1] + 1 < efrm)) {
            switch (*frm) {
            case IEEE80211_ELEMID_SSID:
                ssid = frm;
                break;
             /* parse additional elements if required */
            }
            frm += frm[1] + 2;
        }
        if (frm > efrm) {
            /* Invalid frame return */
            return;
        }

        qdf_mem_zero(&probe, sizeof(struct papi_non_associated_sta_report_ind));
        OS_MEMCPY(probe.mac, wh->i_addr2, QDF_MAC_ADDR_SIZE);
        probe.time_stamp = OS_GET_TIMESTAMP();
        if (ssid) {
            ssidlen = (ssid[1] > PAPI_MAX_SSID_LEN)? PAPI_MAX_SSID_LEN : ssid[1];
            OS_MEMCPY(probe.ssid, ssid+2, ssidlen);
            probe.ssid[ssidlen] = '\0';
            probe.ssid_len = ssidlen;
        }
        probe.band = IEEE80211_IS_CHAN_5GHZ(rs->rs_full_chan)?
                         PAPI_BAND_5_GHZ : PAPI_BAND_24_GHZ;
        probe.chan_num = rs->rs_full_chan->ic_ieee;
        probe.rssi = rs->rs_abs_rssi;

        IEEE80211_DELIVER_PAPI_EVENT(vap, PAPI_NON_ASSOCIATED_STA_REPORT,
                             sizeof(probe),
                             (const char *)&probe);
    } else {
        /* not vap's self node, then it must be probe from associated
           sta, ignore */
    }

}
