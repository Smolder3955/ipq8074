/*
 *  Copyright (c) 2014-2017 Qualcomm Innovation Center, Inc.
 *  All Rights Reserved.
 *  Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 *  2014-2016 Qualcomm Atheros, Inc.  All rights reserved.
 *
 *  Qualcomm is a trademark of Qualcomm Technologies Incorporated, registered in the United
 *  States and other countries.  All Qualcomm Technologies Incorporated trademarks are used with
 *  permission.  Atheros is a trademark of Qualcomm Atheros, Inc., registered in
 *  the United States and other countries.  Other products and brand names may be
 *  trademarks or registered trademarks of their respective owners.
 */


/*
 *  Radio Resource measurements message handlers.
 */
#include <ieee80211_var.h>
#include "ieee80211_rrm_priv.h"
#include <ieee80211_channel.h>
#include<ieee80211_ioctl.h>
#include <ieee80211_cfg80211.h>

#if UMAC_SUPPORT_RRM

#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_objmgr_vdev_obj.h>
#include <ieee80211_objmgr_priv.h>
#include "qal_devcfg.h"

/*
 * RRM action frames recv handlers
 */

static void ieee80211_rrm_scan_evhandler(struct wlan_objmgr_vdev *vdev, struct scan_event *event, void *arg);

/**
 * @brief interaction with frame input module
 *
 * @param vap
 * @param ni
 * @param action
 * @param frm
 * @param frm_len
 *
 * @return on success return 0.
 *         on failure returns -ve value.
 */
int ieee80211_rrm_recv_action(wlan_if_t vap, wlan_node_t ni, u_int8_t action,
        u_int8_t *frm, u_int32_t frm_len)
{

#if UMAC_SUPPORT_RRM_DEBUG
    u_int8_t *act_string = NULL;
    static u_int8_t *action_names[] = {
        "Radio Req",
        "Radio Resp",
        "Link  Req",
        "Link Resp",
        "Neighbor Req",
        "Neighor Resp",
        "none"
    };
    if (action < 6)
        act_string = action_names[action];
    else
        act_string = action_names[6];

    RRM_DEBUG(vap,RRM_DEBUG_VERBOSE,"Received Frame: Action %s Len %d\n",act_string,frm_len);
#endif

    switch (action) {
        case IEEE80211_ACTION_RM_REQ:
            ieee80211_recv_radio_measurement_req(vap, ni,
                    (struct ieee80211_action_rm_req *)frm, frm_len);
            break;
        case IEEE80211_ACTION_RM_RESP:
            ieee80211_recv_radio_measurement_rsp(vap, ni,
                    frm, frm_len);
            break;
        case IEEE80211_ACTION_LM_REQ:
            break;
        case IEEE80211_ACTION_LM_RESP:
            ieee80211_recv_lm_rsp(vap, ni,frm, frm_len);
            break;
        case IEEE80211_ACTION_NR_REQ:
           ieee80211_recv_neighbor_req(vap, ni,
                    (struct ieee80211_action_nr_req *)frm, frm_len);
            break;
        case IEEE80211_ACTION_NR_RESP:
            break;
        default:
            break;
    }
    return EOK;
}
extern int num_ap_lcr;
static int ieee80211_len_nr_meas_subie(struct ieee80211_nrresp_info* nr_info, u_int8_t meas_count)
{
    u_int8_t    colocated_bss_len;
    u_int8_t    len = 0;
    /* LCI and/or LCR measurement request may come any order*/
    switch(nr_info->meas_type[meas_count]) {
        case IEEE80211_MEASRSP_LCI_REPORT:
            /* Adding all SubIEs. Note each SubIE has additional 2 bytes
               (subIE Id + Length field) */
            len = ( IEEE80211_SUBELEM_LCI_USAGE_RULE_LEN + 2 +
                    IEEE80211_SUBELEM_LCI_Z_LEN + 2 +
                    IEEE80211_LOC_CIVIC_INFO_LEN + 2 );
            /* Check if colocated_bss contains more than 1 VAP,
               colocated_bss[1] has num_vaps. Add Colocated SubIE in Neighbor
               response only when 2 or more VAPs are present */
            if(nr_info->colocated_bss[1] > 1) {
                /* Convert num_vaps to represent octects:
                   6*Num_of_vap + 1 (Max BSSID Indicator field) */
                colocated_bss_len = (nr_info->colocated_bss[1]*IEEE80211_ADDR_LEN)+1;
                /* Update length for LCI report */
                len += colocated_bss_len + 2;
            }
            break;

        case IEEE80211_MEASRSP_LOC_CIV_REPORT:
            if(num_ap_lcr == 0) {
             /* When Civic length is 0, Measurement subelement length is 6 */
                len = IEEE80211_SUBELEM_LOC_CIVIC_DEFAULT_LEN;
            } else {
                /* civic_info[64] defined in rtt.h goes over the air includes
                   Country, CAtype, Len, Civic Addr */
                /* Add octets for civic location type, civic subelement Id and length */
                len = (IEEE80211_SUBELEM_LCR_TYPE_FIELD_SIZE +
                        IEEE80211_SUBELEM_LCR_ID_FIELD_SIZE +
                        IEEE80211_SUBELEM_LCR_LENGTH_FIELD_SIZE +
                        IEEE80211_LOC_CIVIC_REPORT_LEN );

            }
            break;
        default:
            break;
    }
    /* Add sizeof Measurement report fields Token,Report Mode,Measurement Type */
    len += ( IEEE80211_MEASREPORT_TOKEN_SIZE +
             IEEE80211_MEASREPORT_MODE_SIZE +
             IEEE80211_MEASREPORT_TYPE_SIZE);
    /* Add size for subElement ID field + Length field */
    len = len + 2;

    return len;
}
/**
 * @brief
 * Go through the scan list and fill the neighbor
 * AP's information
 *
 * @param arg
 * @param se
 *
 * @return
 * @return Always success return 0.

 */

QDF_STATUS ieee80211_fill_nrinfo(void *arg, wlan_scan_entry_t se)
{
    struct ieee80211_rrm_cbinfo *cb_info = (struct ieee80211_rrm_cbinfo *)arg;
    struct ieee80211_nr_ie *nr = (struct ieee80211_nr_ie *)cb_info->frm;
    struct ieee80211_nrresp_info ninfo;
    struct ieee80211_ath_channel *chan;
    struct ieee80211_ie_ext_cap *extcaps;
#if SUPPORT_11AX_D3
    struct ieee80211_ie_heop *heop;
    struct he_op_param heop_param;
#endif
    u_int8_t scan_ssid_len;
    u_int8_t *scan_ssid = NULL;
    int32_t pktlen = 0,meas_count = 0,count;
    scan_ssid = util_scan_entry_ssid(se)->ssid;
    scan_ssid_len = util_scan_entry_ssid(se)->length;

    if((!scan_ssid_len) || (scan_ssid_len > (IEEE80211_NWID_LEN -1))){
        return EOK;
    }

    /* NOTE: Nbr request with SSID=0 is valid entry (wild card), add all nodes (irrespective of ssid) */
    /* When SSID not present in request, add nodes in current ESS. cb_info->ssid will have current ESS value */
    /* When SSID was present in request, add nodes matching this ssid (cb_info->ssid) */

    /* cb_info->ssid will always have a value, either 0 or specific ssid */
    if(cb_info->ssid[0] != 0) {
        /* Only include neighbors that match SSID value */
        if ((cb_info->ssid_len != scan_ssid_len) ||
            (OS_MEMCMP(cb_info->ssid, scan_ssid, scan_ssid_len) != 0)) {
                return EOK;
        }
    }
    OS_MEMZERO(&ninfo, sizeof(struct ieee80211_nrresp_info));
    IEEE80211_ADDR_COPY(ninfo.bssid, util_scan_entry_bssid(se));
    chan = wlan_util_scan_entry_channel(se);
    ninfo.channum = chan->ic_ieee;
    ninfo.capinfo = util_scan_entry_capinfo(se).value;
    ninfo.phytype = ieee80211_rrm_get_phy_type((u_int16_t) util_scan_entry_phymode(se));
    ninfo.chwidth = ieee80211_rrm_get_chwidth_for_wbc(chan->ic_flags);

    /* As per amendment "VHT160 operation signaling through non-zero CCFS1"
     * meanings of ic_vhtop_ch_freq_seg1 and ic_vhtop_ch_freq_seg1 are changed
     * for 160 MHz channel. But till now IEEE haven't changed the definition of
     * any of the other information elements.
     * So convert the values as per standard.
     */
    if (IEEE80211_IS_CHAN_11AC_VHT160(chan)) {
        ninfo.cf_s1 = chan->ic_vhtop_ch_freq_seg2;
        ninfo.cf_s2 = 0;
    } else {
        ninfo.cf_s1 = chan->ic_vhtop_ch_freq_seg1;
        ninfo.cf_s2 = chan->ic_vhtop_ch_freq_seg2;
    }

    ninfo.capinfo = util_scan_entry_capinfo(se).value;
#if SUPPORT_11AX_D3
    ninfo.is_he = (util_scan_entry_hecap(se) != NULL) ? 1: 0; /* If HE capability is present */
    if(ninfo.is_he) {
        heop = (struct ieee80211_ie_heop *) util_scan_entry_heop(se);
        if(heop) {
            qdf_mem_copy(&heop_param, heop->heop_param, sizeof(heop->heop_param));
            ninfo.is_he_er_su = !heop_param.er_su_disable;  /* If AP is HE ER SU capable */
        }
    }
#endif
    ninfo.is_vht = (util_scan_entry_vhtcap(se) != NULL) ? 1 : 0;  /* If VHT capability IE is present */
    ninfo.is_ht = (util_scan_entry_htcap(se) != NULL) ? 1 : 0;  /* If HT capability IE is present */
    extcaps = (struct ieee80211_ie_ext_cap *) util_scan_entry_extcaps(se);

    if(extcaps && (extcaps->ext_capflags3 & IEEE80211_EXTCAPIE_FTM_RES)) {
        /* if bit70 in Ext Capabilities is set, FTM is enabled */
        ninfo.is_ftm = 1;
    }

    ninfo.preference = cb_info->preference;

    /* Copy all measurement report related params */
    /* This includes meas_count, meas_type, meas_req_mode, loc_sub */
    OS_MEMCPY(&ninfo.meas_count, &cb_info->meas_count, IEEE80211_NRREQ_MEASREQ_FIELDS_LEN);

    pktlen = sizeof(*nr) + sizeof(struct ieee80211_nr_preference_subie) +
        sizeof(struct ieee80211_nr_wbc_subie);
    if (cb_info->meas_count) {
        count = cb_info->meas_count;
        while(count) {
            pktlen +=  ieee80211_len_nr_meas_subie(&ninfo,meas_count);
            meas_count++;
            count--;
        }
    }
    if ( (cb_info->max_pktlen - pktlen) < 0 ) {
          qdf_print("%s nr report reached the max pkt length ",__func__);
          return -EBUSY;
    }

    cb_info->frm = ieee80211_add_nr_ie(cb_info->frm, &ninfo);
    cb_info->max_pktlen -= (nr->len + 2); /* 2 is for nr_ie id and nr_ie len */

    return EOK;
}

int ieee80211_mbo_fill_nrinfo(void *arg, wlan_scan_entry_t se)
{
    struct ieee80211_mbo_info *mbo_info =(struct ieee80211_mbo_info *)arg;
    struct ieee80211_nrresp_info ninfo;
    struct ieee80211_ath_channel *chan, *cur_chan;
    u_int8_t scan_ssid_len;
    u_int8_t *scan_ssid = NULL;
    int i,to_add = true;
    /*
     * se of struct type ieee80211_scan_entry contains
     * many parameters such as channel load,
     * association cost etc that can be used to make a
     * more intelligent decision about whether or not to
     * include the AP's BSSID in neighbor list for that particular STA
     */
     /* Get channel number on which AP is currently operating */
     cur_chan = wlan_util_scan_entry_channel(se);

     scan_ssid = util_scan_entry_ssid(se)->ssid;
     scan_ssid_len = util_scan_entry_ssid(se)->length;

     for ( i = 0; i < mbo_info->num_of_channels ;i++ ) {
         /**
          *  If channel on which neighboring AP is operating
          *  is equal to a forbidden channel then mark AP as
          *  not to be added to neighbor rpt
          */
         if ( cur_chan->ic_ieee == mbo_info->channels[i] ) {
             to_add = false;
             break;
         }
      }
     /**
      * If current channel of neighboring AP is not
      * supported by STA then leave it out of the
      * neighbor rpt list
      */
     if( mbo_info->supported_channels[cur_chan->ic_ieee] == false )
         to_add = false;

     if ( to_add == false )
         return EOK;

     if (mbo_info->ssid && (mbo_info->ssid_len == scan_ssid_len) &&
         (OS_MEMCMP(mbo_info->ssid, scan_ssid, scan_ssid_len) == 0)) {
         OS_MEMZERO(&ninfo, sizeof(struct ieee80211_nrresp_info));
         IEEE80211_ADDR_COPY(ninfo.bssid, util_scan_entry_bssid(se));
         chan = wlan_util_scan_entry_channel(se);
         ninfo.channum = chan->ic_ieee;
         ninfo.capinfo = util_scan_entry_capinfo(se).value;
         mbo_info->frm = ieee80211_add_nr_ie(mbo_info->frm, &ninfo);
      }
     return EOK;

}

/** Get PHY TYPE as per Anex C 802.11mc draft spec **/
u_int8_t ieee80211_rrm_get_phy_type(u_int16_t phy_mode)
{
    u_int8_t phy_type;
    switch(phy_mode)
    {
        case 2:
                phy_type = IEEE80211_PHY_TYPE_DSSS;
                break;
        case 4:
                phy_type = IEEE80211_PHY_TYPE_FHSS;
                break;
        case 1:
        case 3:
        case 5:
        case 6:
                phy_type = IEEE80211_PHY_TYPE_OFDM;
                break;
        case 7:
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
                phy_type = IEEE80211_PHY_TYPE_HT;
                break;
        case 15:
        case 16:
        case 17:
        case 18:
        case 19:
        case 20:
        case 21:
                phy_type = IEEE80211_PHY_TYPE_VHT;
                break;
        case 22:
        case 23:
        case 24:
        case 25:
        case 26:
        case 27:
        case 28:
        case 29:
        case 30:
        case 31:
        case 32:
                phy_type = IEEE80211_PHY_TYPE_HE;
                break;
        default:
                phy_type = IEEE80211_PHY_TYPE_UNKNOWN;
                break;
    }
    return phy_type;

}
/** Getting channel width for Wide Bandwidth Channel subelement    **/
/** return: 0 (20MHZ), 1 (40MHZ), 2 (80MHz), 3 (160MHz), 4 (80+80) **/
u_int8_t ieee80211_rrm_get_chwidth_for_wbc(u_int64_t ic_flags)
{
    u_int8_t chwidth = IEEE80211_CWM_WIDTHINVALID;

    if (ic_flags & (IEEE80211_CHAN_VHT80_80 | IEEE80211_CHAN_HE80_80)) {
        chwidth = IEEE80211_CWM_WIDTH80_80;
    }
    else if (ic_flags & (IEEE80211_CHAN_VHT160 | IEEE80211_CHAN_HE160)) {
        chwidth = IEEE80211_CWM_WIDTH160;
    }
    else if (ic_flags & (IEEE80211_CHAN_VHT80 | IEEE80211_CHAN_HE80)) {
        chwidth = IEEE80211_CWM_WIDTH80;
    }
    else if (ic_flags & (IEEE80211_CHAN_HT40PLUS | IEEE80211_CHAN_HT40MINUS
        | IEEE80211_CHAN_VHT40PLUS | IEEE80211_CHAN_VHT40MINUS
        | IEEE80211_CHAN_HE40PLUS | IEEE80211_CHAN_HE40MINUS)) {
        chwidth = IEEE80211_CWM_WIDTH40;
    }
    else if (ic_flags & (IEEE80211_CHAN_TURBO | IEEE80211_CHAN_CCK |IEEE80211_CHAN_OFDM | IEEE80211_CHAN_DYN
        | IEEE80211_CHAN_GFSK | IEEE80211_CHAN_STURBO | IEEE80211_CHAN_HALF | IEEE80211_CHAN_QUARTER
        | IEEE80211_CHAN_HT20 | IEEE80211_CHAN_VHT20 | IEEE80211_CHAN_HE20)) {
        chwidth = IEEE80211_CWM_WIDTH20;
    }

    return chwidth;
}

static void ieee80211_fill_bcn_measrsp(wlan_scan_entry_t se, struct ieee80211_rrm_cbinfo *cb_info,
        u_int8_t *scan_bssid, u_int8_t *start)
{
    struct ieee80211_beacon_report *bcnrep = NULL;
    struct ieee80211_measrsp_ie *bcn_measrsp = NULL;
    unsigned long tsf;
    u_int8_t rssi;

    bcn_measrsp = (struct ieee80211_measrsp_ie *)start;
    bcn_measrsp->id = IEEE80211_ELEMID_MEASREP;
    bcn_measrsp->token = cb_info->dialogtoken;
    bcn_measrsp->rspmode = 0x00; /* Need to validate */
    bcn_measrsp->rsptype = IEEE80211_MEASRSP_BEACON_REPORT;
    bcnrep = (struct ieee80211_beacon_report *)&bcn_measrsp->rsp[0];
    bcnrep->reg_class = 0x00; /* need to check */
    bcnrep->ch_num = util_scan_entry_channel_num(se);
    qdf_mem_copy(bcnrep->ms_time, util_scan_entry_tsf(se), 8);
    bcnrep->mduration = cb_info->duration;

    /* The standard specifies that RCPI is to be encoded in half-dBm with an
       offset of 110. However, commercial client devices tested to date appear
       to just specify it in dBm using 2's complement. To ensure consistency
       (even though it's not per the standard), convert the RSSI in dB to an
       RSSI in dBm by subtracting the typical 20 MHz noise floor. */
    rssi = util_scan_entry_rssi(se);
    bcnrep->rcpi = rssi + RRM_DEFAULT_NOISE_FLOOR;

    bcnrep->rsni = bcnrep->rcpi;
    IEEE80211_ADDR_COPY(bcnrep->bssid, scan_bssid);
    bcnrep->ant_id = 0x00;
    tsf = qdf_system_ticks();
    qdf_mem_copy(bcnrep->parent_tsf, &tsf, 4);
    cb_info->frm = &bcnrep->ies[0];
    bcn_measrsp->len = cb_info->frm - start - 2;
}

/**
 * ieee80211_fill_bcnreq_info() - API to fill beacon report info
 * @arg: pointer ieee80211_rrm_cbinfo
 * @se:  wlan_scan_entry_t
 *
 * Go through the scan list and fill the beacon request information
 *
 * Return: on QDF_STATUS_SUCCESS return 0.
 *         on failure returns  value.
 */
QDF_STATUS ieee80211_fill_bcnreq_info(void *arg, wlan_scan_entry_t se)
{
    struct ieee80211_rrm_cbinfo *cb_info = (struct ieee80211_rrm_cbinfo *)arg;
    u_int8_t *scan_bssid = NULL, *start = NULL;
    u_int8_t bcast[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    int32_t pktlen = (cb_info->max_pktlen - (sizeof(struct ieee80211_measrsp_ie)
                        + sizeof(struct ieee80211_beacon_report)));
    struct wlan_ssid *wssid = NULL;

    /* Ensure that the packet length does not cross the max allowed skbuf size */
    if (pktlen < 0) {
        return QDF_STATUS_E_BUSY;
    } else {
        cb_info->max_pktlen -= (sizeof(struct ieee80211_measrsp_ie) + sizeof(struct ieee80211_beacon_report));
    }

    scan_bssid = util_scan_entry_bssid(se); 
    wssid = util_scan_entry_ssid(se);
    start = (u_int8_t *)(cb_info->frm);

    if (cb_info->ssid_len > 0) {
        if (wssid && wssid->ssid && (cb_info->ssid_len == wssid->length) &&
            !qdf_mem_cmp(wssid->ssid, cb_info->ssid, cb_info->ssid_len)) {
            ieee80211_fill_bcn_measrsp(se, cb_info, scan_bssid, start);
        }
    } else if (IEEE80211_ADDR_EQ(scan_bssid, cb_info->bssid)) {
        ieee80211_fill_bcn_measrsp(se, cb_info, scan_bssid, start);
        return QDF_STATUS_E_BUSY; /* to stop it */
    } else if (IEEE80211_ADDR_EQ(cb_info->bssid,bcast )) {
        if ((util_scan_entry_channel_num(se) == cb_info->chnum) || (!cb_info->chnum)) {
            ieee80211_fill_bcn_measrsp(se, cb_info, scan_bssid, start);
        }
    }

    return QDF_STATUS_SUCCESS;
}

/**
 * send beacon measurement request.
 * @param vaphandle         : handle to the VAP
 * @param macaddr           : mac addr to send the request
 * @param binfo             : beacon measurement request information
 *
 * @return on success return 0.
 *         on failure returns -ve value.
 */
extern void ieee80211_flush_beacontable(wlan_if_t vap);

int wlan_send_beacon_measreq(wlan_if_t vaphandle, u_int8_t *macaddr,
                             ieee80211_rrm_beaconreq_info_t *binfo)
{
    wbuf_t wbuf = NULL;
    u_int8_t *frm = NULL;
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211_node *ni=NULL;
    ieee80211_rrm_t rrm = vap->rrm;
    int error = 0;
    struct wlan_objmgr_pdev *pdev = NULL;
    QDF_STATUS status;

    RRM_FUNCTION_ENTER;

    if(IEEE80211_ADDR_EQ(vap->iv_myaddr,macaddr)) {
        struct ieee80211_rrmreq_info *params=NULL;

        if (wlan_scan_in_progress(vap))
            return -EBUSY;

        ni = ieee80211_vap_find_node(vap, macaddr);

        if(NULL == ni)
            return -EINVAL;

        IEEE80211_ADDR_COPY(rrm->rrm_macaddr, macaddr);

        params = (struct ieee80211_rrmreq_info *)
            OS_MALLOC(vap->rrm->rrm_osdev, sizeof(struct ieee80211_rrmreq_info), 0);

        if(NULL == params)
        {
            ieee80211_free_node(ni);
            return -ENOMEM;
        }
        params->chnum = binfo->channum;
        params->rm_dialogtoken = IEEE80211_ACTION_RM_TOKEN;
        IEEE80211_ADDR_COPY(params->bssid,binfo->bssid);
        ieee80211_rrm_set_report_pending(vap,IEEE80211_MEASREQ_BR_TYPE,(void *)params);

        status = wlan_objmgr_vdev_try_get_ref(vap->vdev_obj, WLAN_OSIF_SCAN_ID);
        if (QDF_IS_STATUS_ERROR(status)) {
            ieee80211_free_node(ni);
            scan_info("unable to get reference");
            return -EBUSY;
        }
        pdev = wlan_vdev_get_pdev(vap->vdev_obj);
        ucfg_scan_flush_results(pdev, NULL);
        wlan_objmgr_vdev_release_ref(vap->vdev_obj, WLAN_OSIF_SCAN_ID);

        if(ieee80211_rrm_scan_start(vap, true)) {
            ieee80211_rrm_free_report(vap->rrm);
            ieee80211_free_node(ni);
            return -EBUSY;
        }
        ieee80211_free_node(ni);
        RRM_FUNCTION_EXIT;

        return EOK;
    } else {
        if(!(ieee80211_vap_rrm_is_set(vap) && ieee80211_node_is_rrm(vap,macaddr)))
            return -EINVAL;

        ni = ieee80211_vap_find_node(vap, macaddr);

        if (ni == NULL)
            return -EINVAL;

        wbuf = ieee80211_getmgtframe(ni, IEEE80211_FC0_SUBTYPE_ACTION, &frm, 0);

        IEEE80211_ADDR_COPY(rrm->rrm_macaddr, macaddr);
        if (wbuf == NULL) {
            error = -ENOMEM;
            goto exit;
        }

        frm = ieee80211_add_rrm_action_ie(frm,IEEE80211_ACTION_RM_REQ, binfo->num_rpt,ni);
        frm = ieee80211_add_measreq_beacon_ie((u_int8_t *)(frm), ni, binfo, wbuf_header(wbuf));
        wbuf_set_pktlen(wbuf, (frm - (u_int8_t*)wbuf_header(wbuf)));

        /* We are requesting for new report, so flush the existing table */
        ieee80211_flush_beacontable(vap);

        /* Check if PMF or CCX is enabled and set security bit */
        if (((vap->iv_ccx_evtable && vap->iv_ccx_evtable->wlan_ccx_is_mfp &&
              vap->iv_ccx_evtable->wlan_ccx_is_mfp(vap->iv_ifp)) ||
             ((ieee80211_vap_mfp_test_is_set(vap) ||
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
             (wlan_crypto_is_pmf_enabled(vap->vdev_obj, ni->peer_obj)))))) {
#else
               ieee80211_is_pmf_enabled(vap, ni)) && ni->ni_ucastkey.wk_valid))) {
#endif
            struct ieee80211_frame *wh = (struct ieee80211_frame*)wbuf_header(wbuf);
            wh->i_fc[1] |= IEEE80211_FC1_WEP;
        }

        error = ieee80211_send_mgmt(vap, ni, wbuf, false);
        RRM_FUNCTION_EXIT;

exit:
        /* claim node immediately */
        ieee80211_free_node(ni);
        return error;
    }
    return -ENOSPC;
}
/**
 * send traffic stream measurement request.
 * @param vaphandle         : handle to the VAP
 * @param macaddr           : mac addr to send the request
 * @param binfo             : traffic stream measurement request information
 *
 * @return on success return 0.
 *         on failure returns -ve value.
 */

int wlan_send_tsm_measreq(wlan_if_t vaphandle, u_int8_t *macaddr,
                          ieee80211_rrm_tsmreq_info_t *tsminfo)
{
    wbuf_t wbuf = NULL;
    u_int8_t *frm = NULL;
    struct ieee80211vap   *vap = vaphandle;
    struct ieee80211_node *ni=NULL;
    int error = 0;

    RRM_FUNCTION_ENTER;

    if(!(ieee80211_vap_rrm_is_set(vap) && ieee80211_node_is_rrm(vap,macaddr)))
        return -EINVAL;

    ni = ieee80211_vap_find_node(vap, macaddr);

    if (NULL == ni) {
        return -EINVAL;
    }

    wbuf = ieee80211_getmgtframe(ni, IEEE80211_FC0_SUBTYPE_ACTION, &frm, 0);

    if (wbuf == NULL) {
        error = -ENOMEM;
        goto exit;
    }

    frm = ieee80211_add_rrm_action_ie(frm,IEEE80211_ACTION_RM_REQ, tsminfo->num_rpt,ni);
    frm = ieee80211_add_measreq_tsm_ie(frm, tsminfo,ni);

    wbuf_set_pktlen(wbuf, (frm - (u_int8_t*)wbuf_header(wbuf)));
    error =  ieee80211_send_mgmt(vap, ni, wbuf, false);

exit:
    /* claim node immediately */
    ieee80211_free_node(ni);
    RRM_FUNCTION_EXIT;
    return error;
}

/**
 * send neighbor report.
 * @param vaphandle         : handle to the VAP
 * @param macaddr           : mac addr to send the request
 * @param nrinfo            : neighbor request info
 *
 * @return on success return 0.
 *         on failure returns -ve value.
 */

int wlan_send_neig_report(wlan_if_t vaphandle, u_int8_t *macaddr,
                          ieee80211_rrm_nrreq_info_t *nrinfo)
{
    struct ieee80211vap      *vap = vaphandle;
    struct ieee80211_node    *ni;
    int error = 0;

    RRM_FUNCTION_ENTER;
    if(!(ieee80211_vap_rrm_is_set(vap) && ieee80211_node_is_rrm(vap,macaddr)))
        return -EINVAL;

    ni = ieee80211_vap_find_node(vap, macaddr);

    if (ni == NULL) {
        return -EINVAL;
    }

    error = ieee80211_send_neighbor_resp(vap, ni, nrinfo);

    /* claim node immediately */
    ieee80211_free_node(ni);
    RRM_FUNCTION_EXIT;
    return error;
}

/**
 * send link measurement request.
 * @param vaphandle         : handle to the VAP
 * @param macaddr           : mac addr to send the request
 *
 * @return on success return 0.
 *         on failure returns -ve value.
 */

int wlan_send_link_measreq(wlan_if_t vaphandle, u_int8_t *macaddr)
{
    wbuf_t wbuf = NULL;
    u_int8_t *frm = NULL;
    struct ieee80211vap  *vap = vaphandle;
    struct ieee80211_node *ni=NULL;
    struct ieee80211_action_lm_req *lm_req=NULL;
    int error = 0;
    struct ieee80211com  *ic = vap->iv_ic;

    RRM_FUNCTION_ENTER;

    if(!(ieee80211_vap_rrm_is_set(vap) && ieee80211_node_is_rrm(vap,macaddr)))
        return -EINVAL;

    ni = ieee80211_vap_find_node(vap, macaddr);

    if (ni == NULL)
        return -EINVAL;

    wbuf = ieee80211_getmgtframe(ni, IEEE80211_FC0_SUBTYPE_ACTION, &frm, 0);

    if (wbuf == NULL) {
        error = -ENOMEM;
        goto exit;
    }

    lm_req = (struct ieee80211_action_lm_req*)(frm);
    lm_req->header.ia_category = IEEE80211_ACTION_CAT_RM;
    lm_req->header.ia_action = IEEE80211_ACTION_LM_REQ;
    lm_req->dialogtoken = ni->lm_dialog_token;
    ni->lm_dialog_token ++;
    lm_req->txpwr_used = (u_int8_t)(ieee80211_node_get_txpower(ni)>>1); /* stored in 0.5 dbm */
    lm_req->max_txpwr =  ic->ic_curchanmaxpwr;
    frm = &lm_req->opt_ies[0];

    wbuf_set_pktlen(wbuf, (frm - (u_int8_t*)wbuf_header(wbuf)));
    error = ieee80211_send_mgmt(vap, ni, wbuf, false);

exit:
    RRM_FUNCTION_EXIT;

    /* claim node immediately */
    ieee80211_free_node(ni);

    return error;
}

/**
 * @brief
 *
 * @param vap
 * @param macaddr
 * @param stastats
 *
 * @return
 * @return on success return 0.
 *         on failure returns -ve value.
 */
int wlan_send_stastats_req(wlan_if_t vap, u_int8_t *macaddr, ieee80211_rrm_stastats_info_t *stastats)
{
    struct ieee80211_node *ni=NULL;
    wbuf_t wbuf = NULL;
    u_int8_t *frm = NULL;
    struct ieee80211com    *ic = vap->iv_ic;
    int error = 0;

    RRM_FUNCTION_ENTER;
    if(!(ieee80211_vap_rrm_is_set(vap) && ieee80211_node_is_rrm(vap,macaddr)))
        return -EINVAL;

    ni = ieee80211_find_node(&ic->ic_sta, macaddr);


    if (ni == NULL)
        return -EINVAL;

    wbuf = ieee80211_getmgtframe(ni, IEEE80211_FC0_SUBTYPE_ACTION, &frm,0);

    if (wbuf == NULL) {
        error = -ENOMEM;
        goto exit;
    }

	frm = ieee80211_add_rrm_action_ie(frm,IEEE80211_ACTION_RM_REQ, stastats->num_rpts,ni);

    frm = ieee80211_add_measreq_stastats_ie(frm, ni, stastats);

    wbuf_set_pktlen(wbuf, (frm - (u_int8_t*)wbuf_header(wbuf)));
    error = ieee80211_send_mgmt(vap, ni, wbuf, false);

    RRM_FUNCTION_EXIT;
exit:

    /* claim node immediately */
    ieee80211_free_node(ni);
    return error;
}
/**
 * @brief
 *
 * @param vaphandle
 * @param macaddr
 * @param chinfo
 *
 * @return
 * @return on success return 0.
 *         on failure returns -ve value.
 */

int wlan_send_lci_req(wlan_if_t vaphandle, u_int8_t *macaddr,
        ieee80211_rrm_lcireq_info_t *lcireq_info)
{
    struct ieee80211vap    *vap = vaphandle;
    struct ieee80211com    *ic = vap->iv_ic;
    struct ieee80211_node  *ni=NULL;
    wbuf_t wbuf = NULL;
    u_int8_t *frm = NULL;
    int error=0;

    RRM_FUNCTION_ENTER;

    if(!(ieee80211_vap_rrm_is_set(vap) && ieee80211_node_is_rrm(vap,macaddr)))
        return -EINVAL;

    ni = ieee80211_find_node(&ic->ic_sta, macaddr);
    if (ni == NULL) {
        return -EINVAL;
    }

    /* Save the location subject value to determine
     * the received report contains LCI of requesting/reporting
     * station
     */
    ni->ni_rrmlci_loc = lcireq_info->location;
    wbuf = ieee80211_getmgtframe(ni, IEEE80211_FC0_SUBTYPE_ACTION, &frm,0);
    if (wbuf == NULL) {
        error = -ENOMEM;
        goto exit;
    }

    frm = ieee80211_add_rrm_action_ie(frm,IEEE80211_ACTION_RM_REQ, lcireq_info->num_rpts,ni);
    frm = ieee80211_add_measreq_lci_ie(frm, ni, lcireq_info);
    wbuf_set_pktlen(wbuf, (frm - (u_int8_t*)wbuf_header(wbuf)));
    error = ieee80211_send_mgmt(vap, ni, wbuf, false);
    RRM_FUNCTION_EXIT;

exit:
    ieee80211_free_node(ni);
    return error;
}
/**
 * @brief
 *
 * @param vaphandle
 * @param macaddr
 * @param chinfo
 *
 * @return
 * @return on success return 0.
 *         on failure returns -ve value.
 */

int wlan_get_rrmstats(wlan_if_t vaphandle, u_int8_t *macaddr,
                      ieee80211_rrmstats_t *rrmstats)
{
    struct ieee80211vap    *vap = vaphandle;
    struct ieee80211com    *ic = vap->iv_ic;
    struct ieee80211_rrm   *rrm = vap->rrm;
    struct ieee80211_node  *ni=NULL;
    u_int8_t zero_mac[IEEE80211_ADDR_LEN] = {0x00,0x00,0x00,0x00,0x00,0x00};
    u_int32_t chnum;

    RRM_FUNCTION_ENTER;

    if(!(ieee80211_vap_rrm_is_set(vap)))
        return -EINVAL;

    if (!IEEE80211_ADDR_EQ(zero_mac, macaddr) && ieee80211_node_is_rrm(vap,macaddr)) {
        ni = ieee80211_find_node(&ic->ic_sta, macaddr);
        if (ni == NULL) {
            return -EINVAL;
        }
        OS_MEMCPY(&rrmstats->ni_rrm_stats, ni->ni_rrm_stats, sizeof(rrmstats->ni_rrm_stats));
        ieee80211_free_node(ni);
    }

    for (chnum = 1; chnum < IEEE80211_RRM_CHAN_MAX; chnum++) {
        if (rrm->u_chload[chnum] != 0) {
            rrmstats->chann_load[chnum] = rrm->u_chload[chnum]; 
        }
        if ((OS_MEMCMP(&rrm->user_nhist_resp[chnum], &rrmstats->noise_data[chnum],
                        sizeof(ieee80211_rrm_noise_data_t))) != 0) {
            OS_MEMCPY(&rrmstats->noise_data[chnum], &rrm->user_nhist_resp[chnum],
                    sizeof(ieee80211_rrm_noise_data_t));
        }
        if ((OS_MEMCMP(&rrm->user_cca_resp[chnum], &rrmstats->cca_data[chnum],
                    sizeof(ieee80211_rrm_cca_data_t))) != 0) {
            OS_MEMCPY(&rrmstats->cca_data[chnum], &rrm->user_cca_resp[chnum],
                    sizeof(ieee80211_rrm_cca_data_t));
        }
        if ((OS_MEMCMP(&rrm->user_rpihist_resp[chnum], &rrmstats->rpi_data[chnum],
                        sizeof(ieee80211_rrm_rpi_data_t))) != 0) {
            OS_MEMCPY(&rrmstats->rpi_data[chnum], &rrm->user_rpihist_resp[chnum],
                    sizeof(ieee80211_rrm_rpi_data_t));
        }
    }
    RRM_FUNCTION_EXIT;
    return EOK;    
}

void wlan_rrm_sta_info(void *arg, wlan_node_t node)
{
    ieee80211_sta_info_t *sta_info = (ieee80211_sta_info_t *)arg;
 
    if (sta_info->count < sta_info->max_sta_cnt) {
        if(node->ni_flags & IEEE80211_NODE_RRM ) {
            IEEE80211_ADDR_COPY((sta_info->dest_addr + (sta_info->count * IEEE80211_ADDR_LEN)),node->ni_macaddr);
            sta_info->count++;
        }
    }
}

/**
* @brief finds the number of rrm stas
*
* @param vap    pointer to corresponding vap
*
* @return rrm capable sta counts
*/
int wlan_get_rrm_sta_count(wlan_if_t vap)
{
    struct ieee80211_node *ni  = NULL;
    struct ieee80211_node_table *nt = &vap->iv_ic->ic_sta;
    int count = 0;

    TAILQ_FOREACH(ni, &nt->nt_node, ni_list) {
        if(ni->ni_flags & IEEE80211_NODE_RRM )
            count++;
    }

    return count;
}

/**
  * @brief to send the MAC address of the RRM capable STA to the upper layers
  *
  * @param vaphandle
  * @param max_sta_count: number of macs buff can hold at a time 
  * @param buff   : pointer to memory where Mac address will be copied
  *
  * @return rrm sta count
  */
int wlan_get_rrm_sta_list(wlan_if_t vap, uint32_t max_sta_count, uint8_t *buff)
{
    ieee80211_sta_info_t dest_stats;

    dest_stats.dest_addr = (u_int8_t *)buff;
    dest_stats.max_sta_cnt = max_sta_count;
    dest_stats.count = 0;
    wlan_iterate_station_list(vap, wlan_rrm_sta_info, &dest_stats);

    return dest_stats.count;
}

#if UMAC_SUPPORT_CFG80211
/**
* @brief send cfg80211 reply containg rrm beacon report
*
* @param vap      :  Vap
* @param len      :  length of payload
* @param cnt      :  number of ieee80211_bcnrpt_t in buff
* @param buff     : buffer containing array of ieee80211_bcnrpt_t
*
* @return 0 for success or -ve for error cases
*/
int send_cfg80211_bcnrpt(wlan_if_t vap, uint32_t len,
                         uint32_t cnt, uint8_t *buff)
{
    struct sk_buff *reply_skb = NULL;
    QDF_STATUS status;

    reply_skb = cfg80211_vendor_cmd_alloc_reply_skb(vap->iv_ic->ic_wiphy,
            (len + (2 * sizeof(u_int32_t)) + NLMSG_HDRLEN));

    if (!reply_skb) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,"%s: Unable to allocate reply skb\n", __func__);
        return -ENOMEM;
    }

    /* Put payload len in bytes */
    nla_put_u32(reply_skb, QCA_WLAN_VENDOR_ATTR_PARAM_LENGTH, len);
    /* Put number of reports in flags */
    nla_put_u32(reply_skb, QCA_WLAN_VENDOR_ATTR_PARAM_FLAGS, cnt);
    /* Put beacon report */
    if (nla_put(reply_skb, QCA_WLAN_VENDOR_ATTR_PARAM_DATA, len, buff)) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,"%s: nla_put() failed for len: %d\n", __func__, len);
        kfree_skb(reply_skb);
        return -EIO;
    }
    status = qal_devcfg_send_response((qdf_nbuf_t)reply_skb);
    return qdf_status_to_os_return(status);
}

/**
* @brief sends rrm beacon report via cfg80211/netlink
*
* @param vap
*
* @return   0: for success else negative error code
*/
int wlan_get_cfg80211_bcnrpt(wlan_if_t vap)
{
#define NUM_BEACON_REPORTS_IN_REPLY     128
    int cnt = 0;
    uint8_t *temp_buf = NULL;
    ieee80211_bcnrpt_t *rpt = NULL;
    u_int32_t len = NUM_BEACON_REPORTS_IN_REPLY * sizeof(ieee80211_bcnrpt_t);
    struct ieee80211_beacon_report_table *btable = vap->rrm->beacon_table;
    struct ieee80211_beacon_report *report = NULL;
    struct ieee80211_beacon_entry   *current_beacon_entry;

    if(btable == NULL) {
        return EOK ;
    }

    temp_buf = OS_MALLOC(vap->iv_ic->ic_osdev, len, GFP_KERNEL);
    if (!temp_buf) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,"%s: Unable to allocate temp buffer\n", __func__);
        return -ENOMEM;
    }

    rpt = (ieee80211_bcnrpt_t *)temp_buf;
    RRM_BTABLE_LOCK(btable);
    {
        TAILQ_FOREACH(current_beacon_entry ,&(btable->entry),blist) {
            report = &current_beacon_entry->report;
            if (cnt >= NUM_BEACON_REPORTS_IN_REPLY) {
                /* No more space in buffer. Send it now */
                /* Even if send_cfg80211_bcnrpt returns failure, temp_buf should not be freed */
                send_cfg80211_bcnrpt(vap, (cnt * sizeof(ieee80211_bcnrpt_t)), cnt, temp_buf);
                cnt = 0;
                rpt = (ieee80211_bcnrpt_t *)temp_buf;
            }
            IEEE80211_ADDR_COPY(rpt->bssid,report->bssid);
            rpt->rsni = report->rsni;
            rpt->rcpi = report->rcpi;
            rpt->chnum = report->ch_num;
            cnt++;
            rpt++;
        }
    }
    RRM_BTABLE_UNLOCK(btable);

    if (cnt) {
        send_cfg80211_bcnrpt(vap, (cnt * sizeof(ieee80211_bcnrpt_t)), cnt, temp_buf);
    }
    OS_FREE(temp_buf);
    return EOK;
}
#endif
/**
 * @brief  to send beacon report to upper layers
 *
 * @param vaphandle
 * @param macaddr
 * @param bcnrpt
 *
 * @return zero if no more entries are present
 */
int wlan_get_bcnrpt(wlan_if_t vap, u_int8_t *macaddr, u_int32_t index,
        ieee80211_bcnrpt_t  *bcnrpt)
{
    int cnt = 1;
    struct ieee80211_beacon_report_table *btable = vap->rrm->beacon_table;

    if(btable == NULL) {
        bcnrpt->more = 0;
        return EOK ;
    }
    RRM_BTABLE_LOCK(btable);
    {
        struct ieee80211_beacon_entry   *current_beacon_entry;
        u_int8_t flag=0;
        TAILQ_FOREACH(current_beacon_entry ,&(btable->entry),blist) {
            struct ieee80211_beacon_report *report = &current_beacon_entry->report;
            if(cnt == index) {
                IEEE80211_ADDR_COPY(bcnrpt->bssid,report->bssid);
                bcnrpt->rsni = report->rsni;
                bcnrpt->rcpi = report->rcpi;
                bcnrpt->chnum = report->ch_num;
                flag = 1;
                break;
            }
            cnt++;
        }

        if(flag)
            bcnrpt->more = cnt;
        else
            bcnrpt->more = 0;
    }
    RRM_BTABLE_UNLOCK(btable);
    return EOK;
}

/**
 * @brief to send CCA from ap
 *
 * @param vap
 * @param macaddr
 * @param cca
 *
 * @return
 * @return on success return 0.
 *         on failure returns -ve value.
 */

int wlan_send_cca_req(wlan_if_t vap, u_int8_t *macaddr, ieee80211_rrm_cca_info_t *cca)
{
    wbuf_t wbuf = NULL;
    u_int8_t *frm = NULL;
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211_node *ni=NULL;
    int error = 0;

    RRM_FUNCTION_ENTER;

    if(!(ieee80211_vap_rrm_is_set(vap) && ieee80211_node_is_rrm(vap,macaddr)))
        return -EINVAL;

    ni = ieee80211_find_node(&ic->ic_sta, macaddr);

    if (ni == NULL) {
        return -EINVAL;
    }

    wbuf = ieee80211_getmgtframe(ni, IEEE80211_FC0_SUBTYPE_ACTION, &frm,0);
    if (wbuf == NULL) {
        error = -ENOMEM;
        goto exit;
    }

    frm = ieee80211_add_rrm_action_ie(frm,IEEE80211_ACTION_RM_REQ, cca->num_rpts,ni);
    frm = ieee80211_add_measreq_cca_ie(frm, ni, cca);
    wbuf_set_pktlen(wbuf, (frm - (u_int8_t*)wbuf_header(wbuf)));
    error = ieee80211_send_mgmt(vap, ni, wbuf, false);
    RRM_FUNCTION_EXIT;
exit:

    ieee80211_free_node(ni);
    return error;
}


/**
 * @brief to send RPI histogram from ap
 *
 * @param vap
 * @param macaddr
 * @param rpihist
 *
 * @return
 * @return on success return 0.
 *         on failure returns -ve value.
 */

int wlan_send_rpihist_req(wlan_if_t vap, u_int8_t *macaddr, ieee80211_rrm_rpihist_info_t *rpihist)
{
    wbuf_t wbuf = NULL;
    u_int8_t *frm = NULL;
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211_node *ni=NULL;
    int error = 0;

    RRM_FUNCTION_ENTER;

    if(!(ieee80211_vap_rrm_is_set(vap) && ieee80211_node_is_rrm(vap,macaddr)))
        return -EINVAL;

    ni = ieee80211_find_node(&ic->ic_sta, macaddr);

    if (ni == NULL) {
        return -EINVAL;
    }

    wbuf = ieee80211_getmgtframe(ni, IEEE80211_FC0_SUBTYPE_ACTION, &frm,0);

    if (wbuf == NULL) {
        error = -ENOMEM;
        goto exit;
    }

    frm = ieee80211_add_rrm_action_ie(frm,IEEE80211_ACTION_RM_REQ, rpihist->num_rpts,ni);
    frm = ieee80211_add_measreq_rpihist_ie(frm, ni, rpihist);
    wbuf_set_pktlen(wbuf, (frm - (u_int8_t*)wbuf_header(wbuf)));
    error = ieee80211_send_mgmt(vap, ni, wbuf, false);
    RRM_FUNCTION_EXIT;
exit:

    ieee80211_free_node(ni);
    return error;
}


/**
 * @brief send channel load request from ap
 *
 * @param vaphandle
 * @param macaddr
 * @param chinfo
 *
 * @return
 * @return on success return 0.
 *         on failure returns -ve value.
 */

int wlan_send_chload_req(wlan_if_t vaphandle, u_int8_t *macaddr,
        ieee80211_rrm_chloadreq_info_t *chinfo)
{

    struct ieee80211vap    *vap = vaphandle;
    struct ieee80211com    *ic = vap->iv_ic;
    struct ieee80211_node  *ni=NULL;
    wbuf_t wbuf = NULL;
    u_int8_t *frm = NULL;
    int error=0;

    RRM_FUNCTION_ENTER;

    if(!(ieee80211_vap_rrm_is_set(vap) && ieee80211_node_is_rrm(vap,macaddr)))
        return -EINVAL;

    ni = ieee80211_find_node(&ic->ic_sta, macaddr);

    if (ni == NULL) {
        return -EINVAL;
    }

    wbuf = ieee80211_getmgtframe(ni, IEEE80211_FC0_SUBTYPE_ACTION, &frm,0);


    if (wbuf == NULL) {
        error = -ENOMEM;
        goto exit;
    }

    frm = ieee80211_add_rrm_action_ie(frm,IEEE80211_ACTION_RM_REQ, chinfo->num_rpts,ni);

    frm = ieee80211_add_measreq_chload_ie(frm, ni, chinfo);

    wbuf_set_pktlen(wbuf, (frm - (u_int8_t*)wbuf_header(wbuf)));

    error = ieee80211_send_mgmt(vap, ni, wbuf, false);
    RRM_FUNCTION_EXIT;
exit:

    ieee80211_free_node(ni);
    return error;
}
/**
 * @brief to send Noise histogram from ap
 *
 * @param vap
 * @param macaddr
 * @param nhist
 *
 * @return
 * @return on success return 0.
 *         on failure returns -ve value.
 */

int wlan_send_nhist_req(wlan_if_t vap, u_int8_t *macaddr, ieee80211_rrm_nhist_info_t *nhist)
{
    wbuf_t wbuf = NULL;
    u_int8_t *frm = NULL;
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211_node *ni=NULL;
    int error = 0;

    RRM_FUNCTION_ENTER;

    if(!(ieee80211_vap_rrm_is_set(vap) && ieee80211_node_is_rrm(vap,macaddr)))
        return -EINVAL;

    ni = ieee80211_find_node(&ic->ic_sta, macaddr);

    if (ni == NULL) {
        return -EINVAL;
    }

    wbuf = ieee80211_getmgtframe(ni, IEEE80211_FC0_SUBTYPE_ACTION, &frm,0);

    if (wbuf == NULL) {
        error = -ENOMEM;
        goto exit;
    }

    frm = ieee80211_add_rrm_action_ie(frm,IEEE80211_ACTION_RM_REQ, nhist->num_rpts,ni);
    frm = ieee80211_add_measreq_nhist_ie(frm, ni, nhist);
    wbuf_set_pktlen(wbuf, (frm - (u_int8_t*)wbuf_header(wbuf)));
    error = ieee80211_send_mgmt(vap, ni, wbuf, false);
    RRM_FUNCTION_EXIT;
exit:

    ieee80211_free_node(ni);
    return error;
}

/**
 * @brief send frame request from ap
 *
 * @param vap
 * @param macaddr
 * @param fr_info
 *
 * @return
 * @return on success return 0.
 *         on failure returns -ve value.
 */

int wlan_send_frame_request(wlan_if_t vap, u_int8_t *macaddr,
                            ieee80211_rrm_frame_req_info_t  *fr_info)
{
    wbuf_t wbuf = NULL;
    u_int8_t *frm = NULL;
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211_node *ni;
    int error = 0;

    RRM_FUNCTION_ENTER;

    if(!(ieee80211_vap_rrm_is_set(vap) && ieee80211_node_is_rrm(vap,macaddr)))
        return -EINVAL;

    ni = ieee80211_find_node(&ic->ic_sta, macaddr);

    if (ni == NULL)
        return -EINVAL;

    wbuf = ieee80211_getmgtframe(ni, IEEE80211_FC0_SUBTYPE_ACTION, &frm,0);

    if (wbuf == NULL) {
        error = -ENOMEM;
        goto exit;
    }

    frm = ieee80211_add_rrm_action_ie(frm,IEEE80211_ACTION_RM_REQ, fr_info->num_rpts,ni);
    frm = ieee80211_add_measreq_frame_req_ie(frm, ni,fr_info);
    wbuf_set_pktlen(wbuf, (frm - (u_int8_t*)wbuf_header(wbuf)));
    error = ieee80211_send_mgmt(vap, ni, wbuf, false);
    RRM_FUNCTION_EXIT;
exit:
    ieee80211_free_node(ni);
    return error;
}

/**
 * @brief free scan resource after scan
 *
 * @param rrm
 * @return void
 */

static void ieee80211_rrm_free_scan_resource(ieee80211_rrm_t rrm)
{
    struct wlan_objmgr_psoc *psoc = NULL;
    psoc = wlan_vap_get_psoc(rrm->rrm_vap);
    /* Free requester ID and callback () */
    ucfg_scan_unregister_requester(psoc, rrm->rrm_scan_requestor);
}



/**
 * @brief scan event handler , will be called at every channel switch
 *
 * @param originator
 * @param event
 * @param arg
 * @return void
 */

static void ieee80211_rrm_scan_evhandler(struct wlan_objmgr_vdev *vdev, struct scan_event *event, void *arg)
{
    struct ieee80211vap *originator = wlan_vdev_get_mlme_ext_obj(vdev);
    ieee80211_rrm_t rrm = (ieee80211_rrm_t)arg;
    struct ieee80211_chan_stats chan_stats;
    struct ieee80211com *ic;
    u_int32_t temp = 0;

    if (!originator) {
        RRM_DEBUG(rrm->rrm_vap,RRM_DEBUG_DEFAULT,"%s: Unable to find vap from vdev \n ", __func__)
        return;
    }
    ic = originator->iv_ic;

    RRM_DEBUG(rrm->rrm_vap, RRM_DEBUG_VERBOSE,"%s scan_id %08X event %d reason %d \n",
              __func__, event->scan_id, event->type, event->reason);

    if (event->type == SCAN_EVENT_TYPE_RADIO_MEASUREMENT_START) {
        /* get initial chan stats for the current channel */
        int ieee_chan_num = ieee80211_mhz2ieee(originator->iv_ic,event->chan_freq, 0);
        if(ieee_chan_num > (IEEE80211_RRM_CHAN_MAX - 1))
           return;
        (void)ic->ic_get_cur_chan_stats(ic, &chan_stats);
        rrm->rrm_chan_load[ieee_chan_num] = chan_stats.chan_clr_cnt;
        rrm->rrm_cycle_count[ieee_chan_num] = chan_stats.cycle_cnt;

        RRM_DEBUG(rrm->rrm_vap,RRM_DEBUG_VERBOSE, "%s: initial_stats noise %d ch: %d ch load: %d cycle cnt %d \n ",
                  __func__, rrm->rrm_noisefloor[ieee_chan_num], ieee_chan_num,
                  chan_stats.chan_clr_cnt,chan_stats.cycle_cnt );
    }
    if(event->type == SCAN_EVENT_TYPE_RADIO_MEASUREMENT_END)  {
        int ieee_chan_num = ieee80211_mhz2ieee(originator->iv_ic,event->chan_freq, 0);
        if(ieee_chan_num > (IEEE80211_RRM_CHAN_MAX - 1))
           return;
        /* Get the noise floor value */
        if (ieee_chan_num != IEEE80211_CHAN_ANY) {
            rrm->rrm_noisefloor[ieee_chan_num] = ic->ic_get_cur_chan_nf(ic);
        }
#define MIN_CLEAR_CNT_DIFF 1000
        /* get final chan stats for the current channel*/
       (void) ic->ic_get_cur_chan_stats(ic, &chan_stats);
       rrm->rrm_chan_load[ieee_chan_num] = chan_stats.chan_clr_cnt - rrm->rrm_chan_load[ieee_chan_num] ;
       rrm->rrm_cycle_count[ieee_chan_num] = chan_stats.cycle_cnt - rrm->rrm_cycle_count[ieee_chan_num] ;

       /* make sure when new clr_cnt is more than old clr cnt, ch utilization is non-zero */
       if (rrm->rrm_chan_load[ieee_chan_num] > MIN_CLEAR_CNT_DIFF ) {
           temp = (u_int32_t)(255* rrm->rrm_chan_load[ieee_chan_num]); /* we are taking % w,r,t 255 as per specification */
           temp = (u_int32_t)( temp/(rrm->rrm_cycle_count[ieee_chan_num]));
           rrm->rrm_chan_load[ieee_chan_num] = MAX( 1,temp);
       } else
           rrm->rrm_chan_load[ieee_chan_num] = 0;

       RRM_DEBUG(rrm->rrm_vap,RRM_DEBUG_VERBOSE,"%s: final_stats noise: %d ch: %d ch load: %d cycle cnt: %d \n ",
                 __func__,rrm->rrm_noisefloor[ieee_chan_num], ieee_chan_num,
                 rrm->rrm_chan_load[ieee_chan_num], rrm->rrm_cycle_count[ieee_chan_num] );
#undef MIN_CLEAR_CNT_DIFF
    }

    if(event->type == SCAN_EVENT_TYPE_COMPLETED) {
        ieee80211_rrm_free_scan_resource(rrm);
        if(rrm->pending_report)
            ieee80211_send_report(rrm);
    }
    return;
}


/**
 * @brief to start scan from rrm
 *
 * @param vap
 * @param active_scan
 *
 * @return
 * @return on success return 0.
 *         on failure returns -ve value.
 */

int ieee80211_rrm_scan_start(wlan_if_t vap, bool active_scan)
{
    ieee80211_rrm_t rrm;
    struct scan_start_request *scan_params = NULL;
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211_rrmreq_info *rrm_req_params=NULL;
    enum ieee80211_opmode opmode = wlan_vap_get_opmode(vap);
    bool is_connected = wlan_is_connected(vap);
    struct ieee80211_ath_channel *chan;
    struct wlan_objmgr_psoc *psoc = NULL;
    uint32_t chan_list = 0;
    QDF_STATUS status = QDF_STATUS_SUCCESS;
    struct wlan_objmgr_vdev *vdev = NULL;

    vdev = vap->vdev_obj;
    status = wlan_objmgr_vdev_try_get_ref(vdev, WLAN_OSIF_SCAN_ID);
    if (QDF_IS_STATUS_ERROR(status)) {
        scan_info("unable to get reference");
        return -EBUSY;
    }

    rrm = vap->rrm;

    rrm_req_params = rrm->rrmcb;

    RRM_FUNCTION_ENTER;

    psoc = wlan_vap_get_psoc(vap);
    rrm->rrm_scan_requestor = ucfg_scan_register_requester(psoc, (u_int8_t*)"rrm",
            ieee80211_rrm_scan_evhandler, (void *)rrm);
    if (!rrm->rrm_scan_requestor) {
        wlan_objmgr_vdev_release_ref(vdev, WLAN_OSIF_SCAN_ID);
        scan_err("unable to allocate requester");
        return -EBUSY;
    }
    scan_params = (struct scan_start_request *)
        qdf_mem_malloc(sizeof(*scan_params));

    if (NULL == scan_params) {
        ieee80211_rrm_free_scan_resource(rrm);
        wlan_objmgr_vdev_release_ref(vdev, WLAN_OSIF_SCAN_ID);
        scan_err("unable to allocate scan request");
        return -ENOMEM;
    }

    OS_MEMZERO(scan_params, sizeof(*scan_params));

    if ((opmode == IEEE80211_M_STA) || (opmode == IEEE80211_M_IBSS)) {
        status = wlan_update_scan_params(vap,scan_params,opmode,active_scan,
                true,is_connected,true,0,NULL,0);
        if (status) {
            qdf_mem_free(scan_params);
            ieee80211_rrm_free_scan_resource(rrm);
            wlan_objmgr_vdev_release_ref(vdev, WLAN_OSIF_SCAN_ID);
            scan_err("scan param init failed with status: %d", status);
            return -EINVAL;
        }
        scan_params->legacy_params.scan_type = SCAN_TYPE_BACKGROUND;
        scan_params->scan_req.scan_f_2ghz = false;
        scan_params->scan_req.scan_f_5ghz = false;
        scan_params->scan_req.scan_f_bcast_probe = true;
        scan_params->scan_req.repeat_probe_time = 30;
        scan_params->legacy_params.min_dwell_active =
            scan_params->scan_req.dwell_time_active = 110;
        if(rrm_req_params->chnum) {
            scan_params->legacy_params.scan_type = SCAN_TYPE_RADIO_MEASUREMENTS;

            chan = ieee80211_find_dot11_channel(ic, rrm_req_params->chnum, 0, IEEE80211_MODE_AUTO);

            if (chan == NULL) {
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_RRM,
                        "%s: Invalid channel passed for rrm request: %u\n", __func__, rrm_req_params->chnum);
                ieee80211_rrm_free_scan_resource(rrm);
                qdf_mem_free(scan_params);
                wlan_objmgr_vdev_release_ref(vdev, WLAN_OSIF_SCAN_ID);
                return -EINVAL;
            }
            chan_list = rrm_req_params->chnum;
            status = ucfg_scan_init_chanlist_params(scan_params, 1, &chan_list, NULL);
            if (status) {
                scan_err("init chan list failed with status: %d", status);
                ieee80211_rrm_free_scan_resource(rrm);
                qdf_mem_free(scan_params);
                wlan_objmgr_vdev_release_ref(vdev, WLAN_OSIF_SCAN_ID);
                return -EINVAL;
            }
        }
    } else if (opmode == IEEE80211_M_HOSTAP) {
        status = wlan_update_scan_params(vap,scan_params,opmode,active_scan,true,true,true,0,NULL,0);
        if (status) {
            qdf_mem_free(scan_params);
            ieee80211_rrm_free_scan_resource(rrm);
            wlan_objmgr_vdev_release_ref(vdev, WLAN_OSIF_SCAN_ID);
            scan_err("scan param init failed with status: %d", status);
            return -EINVAL;
        }
        scan_params->scan_req.scan_flags = 0;
        scan_params->scan_req.scan_f_passive = true;
        scan_params->legacy_params.scan_type = SCAN_TYPE_FOREGROUND;
        scan_params->legacy_params.min_dwell_passive = 200;
        scan_params->scan_req.dwell_time_passive = 300;
    } else {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_RRM,
                "%s: rrm request received in incorrect vap mode\n", __func__);
        ieee80211_rrm_free_scan_resource(rrm);
        qdf_mem_free(scan_params);
        wlan_objmgr_vdev_release_ref(vdev, WLAN_OSIF_SCAN_ID);
        return -EINVAL;
    }
    switch (vap->iv_des_mode) {
        case IEEE80211_MODE_11A:
        case IEEE80211_MODE_TURBO_A:
        case IEEE80211_MODE_11NA_HT20:
        case IEEE80211_MODE_11NA_HT40PLUS:
        case IEEE80211_MODE_11NA_HT40MINUS:
        case IEEE80211_MODE_11NA_HT40:
            scan_params->scan_req.scan_f_5ghz = true;
            break;
        case IEEE80211_MODE_11B:
        case IEEE80211_MODE_11G:
        case IEEE80211_MODE_TURBO_G:
        case IEEE80211_MODE_11NG_HT20:
        case IEEE80211_MODE_11NG_HT40PLUS:
        case IEEE80211_MODE_11NG_HT40MINUS:
        case IEEE80211_MODE_11NG_HT40:
            scan_params->scan_req.scan_f_2ghz = true;
            break;
        default:
            scan_params->scan_req.scan_f_2ghz = true;
            scan_params->scan_req.scan_f_5ghz = true;
            break;
    }
    status = wlan_ucfg_scan_start(vap, scan_params, rrm->rrm_scan_requestor,
            SCAN_PRIORITY_LOW, &(rrm->rrm_scan_id), 0, NULL);
    if (status) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_RRM,
                "%s: Issue a scan fail.\n", __func__);
        ieee80211_rrm_free_scan_resource(rrm);
        wlan_objmgr_vdev_release_ref(vdev, WLAN_OSIF_SCAN_ID);
        scan_err("scan_start failed with status: %d", status);
        return -EINVAL;
    }

    wlan_objmgr_vdev_release_ref(vdev, WLAN_OSIF_SCAN_ID);
    RRM_FUNCTION_EXIT;
    return EOK;
}

/**
 * @brief setting report as pending
 *
 * @param vap
 * @param type
 * @param cbinfo
 *
 * @return
 * @return on success return 0.
 */

int ieee80211_rrm_set_report_pending(wlan_if_t vap,u_int8_t type,void *cbinfo)
{
    ieee80211_rrm_t rrm;
    rrm = vap->rrm;
    rrm->pending_report = TRUE;
    rrm->pending_report_type = type;
    rrm->rrmcb = cbinfo;
    return EOK;
}

/**
 * @brief
 *
 * @param rrm
 *
 * @return
 * @return on success return 0.
 *         on failure returns -ve value.
 */

int ieee80211_rrm_free_report(ieee80211_rrm_t rrm)
{
    if(rrm->rrmcb) {
        OS_FREE(rrm->rrmcb);
        rrm->pending_report = FALSE;
        rrm->rrm_in_progress = 0;
        rrm->rrmcb = NULL;
    }
    return EOK;
}

/**
 * @brief
 *
 * @param vap
 * @param mac
 *
 * @return
 * @return on success return 1.
 */

int ieee80211_node_is_rrm(wlan_if_t vap,char *mac)
{
    struct ieee80211_node *ni  = NULL;
    struct ieee80211_node_table *nt = &vap->iv_ic->ic_sta;

    TAILQ_FOREACH(ni, &nt->nt_node, ni_list) {
      if(ni->ni_flags & IEEE80211_NODE_RRM  
         &&(IEEE80211_ADDR_EQ(ni->ni_macaddr,mac)))
          return 1; 
    }
    return EOK;   
}

/**
 * @brief will be called from association to mark node as rrm  
 *
 * @param ni
 * @param flag
 */
void ieee80211_set_node_rrm(struct ieee80211_node *ni,uint8_t flag)
{
    
#if UMAC_SUPPORT_RRM_DEBUG    
    ieee80211_vap_t vap = ni->ni_vap;
#endif

    RRM_FUNCTION_ENTER;

    /* Free rrm stats from prevoius assoc */
    if(ni->ni_rrm_stats) {
        OS_FREE(ni->ni_rrm_stats);
        ni->ni_rrm_stats = NULL;
    }

    if(flag) {
        ni->ni_rrm_stats =(ieee80211_rrm_node_stats_t *)OS_MALLOC(ni->ni_ic, sizeof(ieee80211_rrm_node_stats_t), 0); 
        if( NULL == ni->ni_rrm_stats) {
            IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_RRM," can not allocate memory %s \n",__func__);
            return;
        }
        ieee80211node_set_flag(ni, IEEE80211_NODE_RRM);
        OS_GET_RANDOM_BYTES(&ni->ni_rrmreq_type,sizeof(ni->ni_rrmreq_type));
        ni->ni_rrmreq_type %= 5;
    }
    else {
        ieee80211node_clear_flag(ni, IEEE80211_NODE_RRM);
        ni->ni_rrmreq_type = 0;
    }

    /* default values for dialogue token */
    ni->rrm_dialog_token = IEEE80211_ACTION_RM_TOKEN;
    ni->lm_dialog_token = IEEE80211_ACTION_LM_TOKEN;
    ni->nr_dialog_token = IEEE80211_ACTION_NR_TOKEN;

    /* default values for measurement token */
    ni->br_measure_token = IEEE80211_MEASREQ_BR_TOKEN;
    ni->chload_measure_token = IEEE80211_MEASREQ_CHLOAD_TOKEN;
    ni->stastats_measure_token = IEEE80211_MEASREQ_STASTATS_TOKEN;
    ni->nhist_measure_token = IEEE80211_MEASREQ_NHIST_TOKEN;
    ni->tsm_measure_token = IEEE80211_MEASREQ_TSMREQ_TOKEN;
    ni->frame_measure_token = IEEE80211_MEASREQ_FRAME_TOKEN;
    ni->lci_measure_token = IEEE80211_MEASREQ_LCI_TOKEN;

    RRM_FUNCTION_EXIT;
    return;
}


/**
 * @brief to attach rrm module from vap attach
 *
 * @param ic
 * @param vap
 *
 * @return
 * @return on success return 0.
 *         on failure returns -ve value.
 */

int ieee80211_rrm_vattach(struct ieee80211com *ic,ieee80211_vap_t vap)
{
    ieee80211_rrm_t  *rrm = &vap->rrm;
    struct ieee80211_beacon_report_table *btable=NULL;
    enum ieee80211_opmode opmode = wlan_vap_get_opmode(vap);


    if (*rrm)
        return -EINPROGRESS; /* already attached ? */

    *rrm = (ieee80211_rrm_t) OS_MALLOC(ic->ic_osdev, sizeof(struct ieee80211_rrm), 0);

    if(NULL == *rrm)
        return -ENOMEM;

    RRM_FUNCTION_ENTER;

    OS_MEMZERO(*rrm, sizeof(struct ieee80211_rrm));

    btable =  (struct ieee80211_beacon_report_table *)
        OS_MALLOC(ic->ic_osdev, (sizeof(struct ieee80211_beacon_report_table)), 0);

    if(NULL == btable)
        return -ENOMEM;

    if (opmode == IEEE80211_M_IBSS) /* making it enabled by default for adhoc */
        ieee80211_vap_rrm_set(vap);

    OS_MEMZERO(btable, sizeof(struct ieee80211_beacon_report_table));

    TAILQ_INIT(&(btable->entry));

    /* XXX lock alloc failure ?*/
    RRM_BTABLE_LOCK_INIT(btable);
    (*rrm)->rrm_osdev = ic->ic_osdev;
    (*rrm)->rrm_vap = vap;
    rrm_attach_misc(*rrm);
    (*rrm)->beacon_table = btable;
#if UMAC_SUPPORT_RRM_DEBUG
    (*rrm)->rrm_dbg_lvl = RRM_DEBUG_DEFAULT;
#endif
    qdf_spinlock_create(&(*rrm)->rrm_lock);

    RRM_FUNCTION_EXIT;

    return EOK;
}

/**
 * @brief at time of shutdown
 *
 * @param vap
 *
 * @return
 */
int ieee80211_rrm_vdetach(ieee80211_vap_t vap)
{

    ieee80211_rrm_t *rrm = &vap->rrm;

    if (*rrm == NULL)
        return -EINPROGRESS; /* already detached ? */

    qdf_spinlock_destroy(&(*rrm)->rrm_lock);

    RRM_FUNCTION_ENTER;
    rrm_detach_misc(*rrm);

    RRM_BTABLE_LOCK_DESTROY((*rrm)->beacon_table);

    OS_FREE((*rrm)->beacon_table);

    OS_FREE(*rrm);

    *rrm = NULL;

    return EOK;
}
#if UMAC_SUPPORT_RRM_DEBUG
/**
 * @brief set debug level
 *
 * @param vap
 * @param val
 *
 * @return
 */
int ieee80211_rrmdbg_set(wlan_if_t vap, u_int32_t val)
{
  ieee80211_rrm_t rrm = vap->rrm;
  rrm->rrm_dbg_lvl = val;
  return EOK;
}

/**
 * @brief get debug level
 *
 * @param vap
 *
 * @return
 */
int ieee80211_rrmdbg_get(wlan_if_t vap)
{
  return vap->rrm->rrm_dbg_lvl;
}
#endif
#endif /* UMAC_SUPPORT_RRM */
