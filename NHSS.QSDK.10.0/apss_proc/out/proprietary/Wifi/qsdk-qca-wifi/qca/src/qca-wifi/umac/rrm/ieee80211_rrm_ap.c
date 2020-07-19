/*
 * Copyright (c) 2013,2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */


/*
 *  Radio Resource measurements message handlers for AP.
 */
#include <ieee80211_var.h>
#include "ieee80211_rrm_priv.h"
#include <ieee80211_api.h>
#include <wlan_son_pub.h>
#if UMAC_SUPPORT_RRM

static void ieee80211_add_self_nrinfo(struct ieee80211vap *vap, struct ieee80211_rrm_cbinfo * cb_info);

/*
 * Set NR share flag to indicate for which radios
 * the NR response requested.
 *
 * @param vap
 * @param nr flag value
 *
 * @return
 * @return on success return 0.
 *         on failure returns -ve value.
 */
int ieee80211_set_nr_share_radio_flag(wlan_if_t vap, int value)
{
#if DBDC_REPEATER_SUPPORT
   struct ieee80211com *tmp_ic;
    int i;

    if(value == 0 ) {
        qdf_print("Set %d : Disable the feature.  ",value);
        qdf_print("Enter [1 to %d] : Enable the feature ",
                (1 << MAX_RADIO_CNT) - 1);
    } else {
        if ((value < 0) || (value >= (1 << MAX_RADIO_CNT))) {
            qdf_print("Enter [1 to %d] : Enable the feature. \n"
                    "Enter 0 : Disable the feature ",
                    (1 << MAX_RADIO_CNT) - 1);
            return -EINVAL;
        }
    }

    /* sync the flag to all radios */
    for (i = 0; i < MAX_RADIO_CNT; i++) {
        GLOBAL_IC_LOCK_BH(vap->iv_ic->ic_global_list);
        tmp_ic = vap->iv_ic->ic_global_list->global_ic[i];
        GLOBAL_IC_UNLOCK_BH(vap->iv_ic->ic_global_list);
        if(tmp_ic) {
            tmp_ic->ic_nr_share_radio_flag = value;
            if (tmp_ic->ic_nr_share_radio_flag & (1 << i))
                tmp_ic->ic_nr_share_enable = 1;
            else
                tmp_ic->ic_nr_share_enable = 0;
        }
    }
#endif
    return 0;
}

/*
 * Format and send neighbor report response
 */

/**
 * @brief Routine to generate neighbor report
 *
 * @param vap
 * @param ni
 * @param nrreq_info
 *
 * @return
 * @return on success return 0.
 *         on failure returns -ve value.
 */

int ieee80211_send_neighbor_resp(wlan_if_t vap, wlan_node_t ni,
                                        ieee80211_rrm_nrreq_info_t* nrreq_info)
{
    wbuf_t wbuf = NULL;
    u_int8_t *frm = NULL;
    struct ieee80211_rrm_cbinfo cb_info;
    struct ieee80211_action_nr_resp *nr_resp;
    RRM_FUNCTION_ENTER;

    OS_MEMSET(&cb_info, 0, sizeof(struct ieee80211_rrm_cbinfo));
    wbuf = ieee80211_getmgtframe(ni, IEEE80211_FC0_SUBTYPE_ACTION, &frm, 0);

    if (wbuf == NULL) {
        return -ENOMEM;
    }

    nr_resp = (struct ieee80211_action_nr_resp*)(frm);
    nr_resp->header.ia_category = IEEE80211_ACTION_CAT_RM;
    nr_resp->header.ia_action = IEEE80211_ACTION_NR_RESP;
    nr_resp->dialogtoken = nrreq_info->dialogtoken;
    cb_info.frm = (u_int8_t *)(&nr_resp->resp_ies[0]);
    OS_MEMCPY(cb_info.ssid, nrreq_info->ssid, nrreq_info->ssid_len);
    cb_info.ssid_len = nrreq_info->ssid_len;
    cb_info.essid = nrreq_info->essid;
    cb_info.essid_len = nrreq_info->essid_len;

    /* Copy all measurement report related params */
    /* This includes meas_count, meas_type, meas_req_mode, loc_sub */
    OS_MEMCPY(&cb_info.meas_count, &nrreq_info->meas_count, IEEE80211_NRREQ_MEASREQ_FIELDS_LEN);

    /* no preference field included with neighbor response */
    cb_info.preference = 0;
    /* Assign max_pktlength before fill cb_info */
    cb_info.max_pktlen = (MAX_TX_RX_PACKET_SIZE - (sizeof(struct ieee80211_frame) +
                (sizeof(struct ieee80211_action_nr_resp))));
    /* Iterate the scan table to build Neighbor report */
    /* check flag for which radios the NR information share is enabled */
    if (vap->iv_ic->ic_nr_share_radio_flag != 0) {

        if (cb_info.ssid[0] == 0) {
            /* Neighbor request with SSID=0 is valid entry (wild card), add all nodes
               (irrespective of ssid) by default design. Change the design to use
               current ess value as filter here as per FR requirement */
            OS_MEMCPY(cb_info.ssid, vap->iv_bss->ni_essid, vap->iv_bss->ni_esslen);
            cb_info.ssid_len = vap->iv_bss->ni_esslen;
        }
        /* Check current radio NR share enabled, then iterate through all radios */
        if (vap->iv_ic->ic_nr_share_enable) {
#if DBDC_REPEATER_SUPPORT
            struct ieee80211com *tmp_ic;
            int i;
            for (i = 0; i < MAX_RADIO_CNT; i++) {
                GLOBAL_IC_LOCK_BH(vap->iv_ic->ic_global_list);
                tmp_ic = vap->iv_ic->ic_global_list->global_ic[i];
                GLOBAL_IC_UNLOCK_BH(vap->iv_ic->ic_global_list);
                if (tmp_ic) {
                    /* if it's not current ic and no share flag, just return */
                    if (tmp_ic != vap->iv_ic  && tmp_ic->ic_nr_share_enable != 1) {
                        continue;
                    }
                    IEEE80211_DPRINTF(vap, IEEE80211_MSG_RRM,
                            "Go through the scan table for IC %d: channel=%d\n",
                            i, tmp_ic->ic_curchan->ic_ieee);

                    ucfg_scan_db_iterate(tmp_ic->ic_pdev_obj, ieee80211_fill_nrinfo, &cb_info);
                }
            }
#endif
         } else {
            ucfg_scan_db_iterate(wlan_vap_get_pdev(vap),
                    ieee80211_fill_nrinfo, &cb_info);
        }
    } else { /* ic_nr_share_radio_flag == 0, keep the default behavior */
        ucfg_scan_db_iterate(wlan_vap_get_pdev(vap),
                ieee80211_fill_nrinfo, &cb_info);
    }

    /* If this AP is FTM & LCI/LCR capable & Neighbor request has LCI/LCR set, add itself in Nbr element */
    if((vap->rtt_enable) && (vap->lcr_enable || vap->lci_enable) && (nrreq_info->meas_count)) {
        ieee80211_add_self_nrinfo(vap, &cb_info);
    }
    wbuf_set_pktlen(wbuf, (cb_info.frm - (u_int8_t*)wbuf_header(wbuf)));

    /* If Managment Frame protection is enabled (PMF or Cisco CCX), set Privacy bit */
    if ((vap->iv_ccx_evtable && vap->iv_ccx_evtable->wlan_ccx_is_mfp &&
        vap->iv_ccx_evtable->wlan_ccx_is_mfp(vap->iv_ifp)) ||
        ((ieee80211_vap_mfp_test_is_set(vap) ||
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
            (wlan_crypto_is_pmf_enabled(vap->vdev_obj, ni->peer_obj))))) {
#else
              ieee80211_is_pmf_enabled(vap, ni)) && ni->ni_ucastkey.wk_valid)){
#endif
        /* MFP is enabled, so we need to set Privacy bit */
        struct ieee80211_frame *wh;
        wh = (struct ieee80211_frame*)wbuf_header(wbuf);
        wh->i_fc[1] |= IEEE80211_FC1_WEP;
    }

    RRM_FUNCTION_EXIT;
    return ieee80211_send_mgmt(vap, ni, wbuf, false);
}

/*
 * Neighbor report request handler
 */

/**
 * @brief Routine to parse neighnor report request
 *
 * @param vap
 * @param ni
 * @param nr_req
 * @param frm_len
 *
 * @return
 * @return on success return 0.
 *         on failure returns -ve value.
 */

int ieee80211_recv_neighbor_req(wlan_if_t vap, wlan_node_t ni,
                                struct ieee80211_action_nr_req *nr_req, int frm_len)
{
    ieee80211_rrm_nrreq_info_t nrreq_info;
    u_int8_t elem_id, len;
    u_int8_t *frm;
    int remaining_ie_length;
    u_int8_t ssid_ie_present = 0;
    RRM_FUNCTION_ENTER;

    frm = &nr_req->req_ies[0];

    // clear memory for neigbor req info data structure
    OS_MEMSET(&nrreq_info, 0, sizeof(ieee80211_rrm_nrreq_info_t));
    nrreq_info.dialogtoken = nr_req->dialogtoken;
    frm_len -= (sizeof(struct ieee80211_action) + sizeof(nr_req->dialogtoken));
    remaining_ie_length = frm_len;

    if(remaining_ie_length < 0) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_RRM, "%s: received neighbour request frame with invalid length\n", __func__);
        return -EINVAL;
    }
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ACTION, "Received neighbor report request\n");
#if ATH_PARAMETER_API
    ieee80211_papi_send_nrreq_event(vap, ni, PAPI_STA_NEIGHBOR_REPORT_REQUEST);
#endif

    while (remaining_ie_length  >= sizeof(struct ieee80211_ie_header) ) {

        elem_id = *frm++;
        len = *frm++;
        /* Do a sanity check on the length received from the packet.
           Some STAs are known to send bad lenght values*/
        if(len + sizeof(struct ieee80211_ie_header) > remaining_ie_length) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_RRM, "%s: received neighbour request length is out of bounds\n", __func__);
            break;
        }

        switch (elem_id) {
            case IEEE80211_ELEMID_SSID:
                OS_MEMCPY(nrreq_info.ssid,frm, len);
                nrreq_info.ssid_len = len;
                ssid_ie_present = 1;
                break;
            case IEEE80211_ELEMID_MEASREQ:
                /* Need to send LCI/LCR as part of Neighbor report */
                nrreq_info.meas_token[nrreq_info.meas_count] = frm[0];
                nrreq_info.meas_req_mode[nrreq_info.meas_count] = frm[1];
                nrreq_info.meas_type[nrreq_info.meas_count] = frm[2];
                nrreq_info.loc_sub[nrreq_info.meas_count] = frm[3];
                nrreq_info.meas_count++;
                break;

            default :
                break;
        }
        frm += len;
        remaining_ie_length -= (len + sizeof(struct ieee80211_ie_header));
    }
    if (!ssid_ie_present) {
        /* SSID IE was not present, make SSID equals current ESS */
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ACTION, "No SSID IE present, using current ESS value %s\n", vap->iv_bss->ni_essid);
        OS_MEMCPY(&nrreq_info.ssid,vap->iv_bss->ni_essid, vap->iv_bss->ni_esslen);
        nrreq_info.ssid_len = vap->iv_bss->ni_esslen;
    }

    RRM_FUNCTION_EXIT;
    /* send Neigbor info */
    return ieee80211_send_neighbor_resp(vap, ni, &nrreq_info);

}

/**
 * @brief to parse rrm report ,
 * its main entry point for parsing reports
 * @param vap
 * @param frm
 * @param frm_len
 *
 * @return
 * @return on success return 0.
 *         on failure returns -ve value.
 */

static int ieee80211_rrm_handle_report(wlan_if_t vap, u_int8_t *frm, u_int32_t frm_len)
{

    struct ieee80211_measrsp_ie *rsp= (struct ieee80211_measrsp_ie *) frm;

    RRM_FUNCTION_ENTER;
    if((rsp->id !=IEEE80211_ELEMID_MEASREP)
#if !QCA_SUPPORT_SON
        /* When band steering is compiled in, we will continue processing
           and generate an error event. */
        ||(rsp->rspmode & (IEEE80211_RRM_MEASRPT_MODE_BIT_INCAPABLE |
                   IEEE80211_RRM_MEASRPT_MODE_BIT_LATE |
                   IEEE80211_RRM_MEASRPT_MODE_BIT_REFUSED))
#endif
        ||(rsp->len > frm_len))
        return -EINVAL;
    switch(rsp->rsptype)
    {
        case IEEE80211_MEASRSP_BASIC_REPORT:
            break;
        case IEEE80211_MEASRSP_CCA_REPORT:
            ieee80211_rrm_cca_report(vap,frm,frm_len);
            break;
        case IEEE80211_MEASRSP_RPI_HISTOGRAM_REPORT:
            ieee80211_rrm_rpihist_report(vap,frm,frm_len);
            break;
        case IEEE80211_MEASRSP_CHANNEL_LOAD_REPORT:
            ieee80211_rrm_chload_report(vap,frm,frm_len);
            break;
        case IEEE80211_MEASRSP_NOISE_HISTOGRAM_REPORT:
            ieee80211_rrm_nhist_report(vap,frm,frm_len);
            break;
        case IEEE80211_MEASRSP_BEACON_REPORT:
#if QCA_SUPPORT_SON
        if (rsp->rspmode & (IEEE80211_RRM_MEASRPT_MODE_BIT_INCAPABLE |
                IEEE80211_RRM_MEASRPT_MODE_BIT_LATE |
                IEEE80211_RRM_MEASRPT_MODE_BIT_REFUSED)) {
            son_send_rrm_bcnrpt_error_event(
                vap->vdev_obj, IEEE80211_ACTION_RM_TOKEN,
                vap->rrm->rrm_macaddr, rsp->rspmode);
            break;
        }
#endif /* QCA_SUPPORT_SON */
            ieee80211_rrm_beacon_measurement_report(vap,frm,frm_len);
            break;
        case IEEE80211_MEASRSP_FRAME_REPORT:
            ieee80211_rrm_frm_report(vap,frm,frm_len);
            break;
        case IEEE80211_MEASRSP_STA_STATS_REPORT:
            ieee80211_rrm_stastats_report(vap,frm,frm_len);
            break;
        case IEEE80211_MEASRSP_LCI_REPORT:
            ieee80211_rrm_lci_report(vap,frm,frm_len);
            break;
        case IEEE80211_MEASRSP_TSM_REPORT:
            ieee80211_rrm_tsm_report(vap,frm,frm_len);
            break;
        default:
            RRM_DEBUG(vap,RRM_DEBUG_VERBOSE,"%s Unknown rsptype  %d\n",__func__,rsp->rsptype);
            break;
    }
    RRM_FUNCTION_EXIT;
    return EOK;
}

/**
 * @brief Routine to parse linkmeasuremnt response
 *
 * @param vap
 * @param ni
 * @param frm
 * @param frm_len
 *
 * @return
 * @return on success return 0.
 *         on failure returns -ve value.
 */

int ieee80211_recv_lm_rsp(wlan_if_t vap, wlan_node_t ni,
        u_int8_t *frm, u_int32_t frm_len)
{
    struct ieee80211_action_lm_rsp *rsp = (struct ieee80211_action_lm_rsp *)frm;
    ieee80211_rrm_node_stats_t *stats =(ieee80211_rrm_node_stats_t *)ni->ni_rrm_stats;

    RRM_FUNCTION_ENTER;

    if( (rsp->dialogtoken > ni->lm_dialog_token )
            || (frm_len <  (sizeof(struct ieee80211_action_lm_rsp)-1)))
    {
        return -EINVAL;
    }
    RRM_DEBUG(vap,RRM_DEBUG_VERBOSE,"%s: tx_power %d lmargin %d rxant %d rcpi %d rsni %d\n",
              __func__, rsp->tpc.tx_pow,rsp->tpc.lmargin,rsp->rxant,rsp->rcpi,rsp->rsni);

    if(ni->ni_flags & IEEE80211_NODE_RRM){
        ieee80211_rrm_lm_data_t *ni_lm = &(stats->lm_data);
        ni_lm->tx_pow = rsp->tpc.tx_pow;
        ni_lm->lmargin = rsp->tpc.lmargin;
        ni_lm->rxant = rsp->rxant;
        ni_lm->rxant = rsp->txant;
        ni_lm->rcpi = rsp->rcpi;
        ni_lm->rsni = rsp->rsni;
    }
    RRM_FUNCTION_EXIT;
    return EOK;
}

/**
 * @brief To parse all measuremnt response
 *
 * @param vap
 * @param ni
 * @param rm_rsp
 * @param frm_len
 *
 * @return
 * @return on success return 0.
 *         on failure returns -ve value.
 */

int ieee80211_recv_radio_measurement_rsp(wlan_if_t vap, wlan_node_t ni,
        u_int8_t *frm, u_int32_t frm_len)
{
    RRM_FUNCTION_ENTER;

    if(!(ieee80211_vap_rrm_is_set(vap) && ieee80211_node_is_rrm(vap,ni->ni_macaddr)))
        return -EINVAL;

    frm +=2; /* catagory + action */
#if !QCA_SUPPORT_SON
    /* Some devices do not put the same token as the one used in the request,
       so we are skipping this check when band steering is compiled in. */
    if(*frm > ni->rrm_dialog_token)
        return -EINVAL;
#endif
    IEEE80211_ADDR_COPY(vap->rrm->rrm_macaddr, ni->ni_macaddr);
    vap->rrm->rrm_vap = vap;

    frm++; /* dialogue token */
    frm_len -= (sizeof(struct ieee80211_action_rm_rsp) -1 );

    ieee80211_rrm_handle_report(vap,frm,frm_len);

    RRM_FUNCTION_EXIT;
    return EOK;
}

/**
 * @brief  parse channel load report
 *
 * @param vap
 * @param ni
 * @param chload
 *
 * @return
 * @return on success return 0.
 *         on failure returns -ve value.
 */

int ieee80211_rrm_chload_report(wlan_if_t vap,u_int8_t *frm,u_int32_t frm_len)
{
    struct ieee80211_measrsp_ie *mie= (struct ieee80211_measrsp_ie *) frm;
    struct ieee80211_chloadrsp *chload = (struct ieee80211_chloadrsp *) (&mie->rsp[0]);
    u_int8_t elen = mie->len,*elmid;
    struct ieee80211_node *ni = NULL;

    RRM_FUNCTION_ENTER;

    ni = ieee80211_vap_find_node(vap, vap->rrm->rrm_macaddr);
    if( ni == NULL )
        return -EINVAL;

    if(( mie->token > ni->chload_measure_token) || (elen < RRM_MIN_RSP_LEN)) /* minimum length should be three */
    {
        ieee80211_free_node(ni);
        return -EINVAL;
    }

    /* element id  len  measurement token  measurement mode  measurement type
        1 +         1      +1                    +1         +     1 */

    elen -= ((sizeof(struct ieee80211_measrsp_ie) -
		(sizeof(struct ieee80211_ie_header) + 1) /* elementid + length + rsp */)
            + (sizeof(struct ieee80211_chloadrsp) - 1));

    /* Rclass  Channel  Mstart  mduration  channel load
       1 +      +1      +8        2          +1    */

    frm += ((sizeof(struct ieee80211_measrsp_ie) -1)
            + (sizeof(struct ieee80211_chloadrsp) - 1 ));

    RRM_DEBUG(vap, RRM_DEBUG_INFO,
              "%s : duration %d chnum %d regclass %d\n", __func__,
               chload->mduration, chload->chnum, chload->regclass);

    chload->chload= (chload->chload *100)/255; /* as per specification */

    RRM_DEBUG(vap, RRM_DEBUG_INFO,"%s : chnum %d chload %d\n",
               __func__, chload->chnum, chload->chload);

    vap->rrm->u_chload[chload->chnum] = chload->chload;

    while(elen  > sizeof(struct ieee80211_ie_header) )
    {
        u_int8_t info_len ;

        elmid = frm++;
        info_len = *frm++;

        if (info_len == 0) {
            frm++;    /* next IE */
            continue;
        }

        if (elen < info_len) {
            /* Incomplete/bad info element */
            ieee80211_free_node(ni);
            return -EINVAL;
        }

        switch(*elmid)
        {
            case IEEE80211_ELEMID_VENDOR:
                /*handle vendor ie*/
                break;
            default:
                break;
        }

        elen -= info_len;
        frm += info_len;
    }

    set_chload_window(vap->rrm,chload->chload);
    ieee80211_free_node(ni);
    RRM_FUNCTION_EXIT;
    return EOK;
}

/**
 * @brief Frame report parsing
 *
 * @param vap
 * @param frm
 * @param frm_len
 *
 * @return
 * @return on success return 0.
 *         on failure returns -ve value.
 */

int ieee80211_rrm_frm_report(wlan_if_t vap,u_int8_t *frm, u_int32_t frm_len)
{
    struct ieee80211_measrsp_ie *mie = (struct ieee80211_measrsp_ie *) frm;
    u_int8_t elen = mie->len,*elmid,info_len =0;
    ieee80211_rrm_t rrm = vap->rrm;
    wlan_node_t ni = NULL;
    ieee80211_rrm_frmcnt_data_t *frm_data = NULL;
    struct ieee80211_frmcnt *frm_rpt = NULL;
    int i=0;
    ieee80211_rrm_node_stats_t *stats = NULL;

    RRM_FUNCTION_ENTER;

    ni = ieee80211_vap_find_node(vap, rrm->rrm_macaddr);
    if( ni == NULL )
       return -EINVAL;

    stats = (ieee80211_rrm_node_stats_t *)ni->ni_rrm_stats;

    if( mie->token > ni->frame_measure_token || mie->len < RRM_MIN_RSP_LEN ) /* minimum length should be three */
    {
        ieee80211_free_node(ni);
        return -EINVAL;
    }

    /* element id  len  measurement token  measurement mode  measurement type
        1 +         1      +1                    +1         +     1 */
    elen -= ((sizeof(struct ieee80211_measrsp_ie) -
	      (sizeof(struct ieee80211_ie_header) + 1) /* elementid + length + rsp */)
            + (sizeof(struct ieee80211_frm_rsp) - 1));
    frm += ((sizeof(struct ieee80211_measrsp_ie) -1)
            + (sizeof(struct ieee80211_frm_rsp) - 1 ));

    while(elen  > sizeof(struct ieee80211_ie_header) ) {
        elmid = frm++;
        info_len = *frm++;

        if (info_len == 0) {
            frm++;    /* next IE */
            continue;
        }

        if (elen < info_len) {
            /* Incomplete/bad info element */
            ieee80211_free_node(ni);
            return -EINVAL;
        }
        switch(*elmid) {
            case IEEE80211_SUBELEMID_FR_REPORT_RESERVED:
                break;
            case IEEE80211_SUBELEMID_FR_REPORT_COUNT:
                {
                    u_int8_t cnt = info_len/sizeof(struct ieee80211_frmcnt);
                    for (i = 0; i < cnt; i++) {
                        frm_rpt = (struct ieee80211_frmcnt *)frm;
                        frm_data = &(stats->frmcnt_data[i]);
                        OS_MEMCPY(&frm_data->ta[0], frm_rpt->ta, IEEE80211_ADDR_LEN);
                        OS_MEMCPY(&frm_data->bssid[0], frm_rpt->bssid, IEEE80211_ADDR_LEN);
                        frm_data->phytype = frm_rpt->phytype;
                        frm_data->arcpi = frm_rpt->arcpi;
                        frm_data->lrsni = frm_rpt->lrsni;
                        frm_data->lrcpi = frm_rpt->lrcpi;
                        frm_data->antid = frm_rpt->antid;
                        frm_data->frmcnt = frm_rpt->frmcnt;
                        frm += sizeof(struct ieee80211_frmcnt);
                    }
                }
                break;
            case IEEE80211_ELEMID_VENDOR:
                /* TBD */
                break;
            default:
                break;
        }
        elen -=info_len;
        frm +=info_len;
    }

    ieee80211_free_node(ni);
    RRM_FUNCTION_EXIT;
    return EOK;
}

/**
 * @brief Traffic stream matrics
 *
 * @param vap
 * @param frm
 * @param frm_len
 *
 * @return
 * @return on success return 0.
 *         on failure returns -ve value.
 */

int ieee80211_rrm_tsm_report(wlan_if_t vap,u_int8_t *frm, u_int32_t frm_len)
{
    struct ieee80211_measrsp_ie *mie = (struct ieee80211_measrsp_ie *) frm;
    struct ieee80211_tsm_rsp *rsp =(struct ieee80211_tsm_rsp *)(&mie->rsp[0]);
    u_int8_t elen = mie->len,*elmid,info_len = 0;
    ieee80211_rrm_t rrm = vap->rrm;
    wlan_node_t ni = NULL;
    ieee80211_rrm_node_stats_t *stats = NULL;
    ieee80211_rrm_tsm_data_t *tsm = NULL;

    RRM_FUNCTION_ENTER;

    ni = ieee80211_vap_find_node(vap, rrm->rrm_macaddr);
    if( ni == NULL )
        return -EINVAL;

    stats =(ieee80211_rrm_node_stats_t *)ni->ni_rrm_stats;
    tsm = &stats->tsm_data;

    if((mie->token > ni->tsm_measure_token) || (mie->len < 3)) /* minimum length should be three */
    {
        ieee80211_free_node(ni);
        return -EINVAL;
    }

    /* element id  len  measurement token  measurement mode  measurement type
        1 +         1      +1                    +1         +     1 */
    elen -= ((sizeof(struct ieee80211_measrsp_ie) -
	     (sizeof(struct ieee80211_ie_header) + 1) /* elementid + length + rsp */)
            + (sizeof(struct ieee80211_tsm_rsp) - 1));

    /* tsm response */
    frm += ( (sizeof(struct ieee80211_measrsp_ie) -1)
            + (sizeof(struct ieee80211_tsm_rsp) - 1));

    tsm->tid = rsp->tid;
    tsm->tx_cnt = rsp->tx_cnt;
    tsm->discnt = rsp->discnt;
    tsm->multirtycnt = rsp->multirtycnt;
    tsm->cfpoll = rsp->cfpoll;
    tsm->qdelay = rsp->qdelay;
    tsm->txdelay = rsp->txdelay;
    tsm->brange = rsp->brange;
    OS_MEMCPY(tsm->mac,rsp->mac,IEEE80211_ADDR_LEN);
    OS_MEMCPY(tsm->bin,rsp->bin,IEEE80211_ADDR_LEN);

    while(elen  > sizeof(struct ieee80211_ie_header)) {

        elmid = frm++;
        info_len = *frm++;

        if (info_len == 0) {
            frm++;    /* next IE */
            continue;
        }

        if (elen < info_len) {
            /* Incomplete/bad info element */
            ieee80211_free_node(ni);
            return -EINVAL;
        }

        switch(*elmid)
        {
            case IEEE80211_ELEMID_VENDOR:
                /*handle vendor ie*/
                break;
            default:
                break;
        }
        elen -= info_len;
        frm += info_len;
    }
    ieee80211_free_node(ni);
    RRM_FUNCTION_EXIT;
    return EOK;
}

/**
 * @brief station stats report
 *
 * @param vap
 * @param frm
 * @param frm_len
 *
 * @return
 * @return on success return 0.
 *         on failure returns -ve value.
 */

int ieee80211_rrm_stastats_report(wlan_if_t vap, u_int8_t *frm, u_int32_t frm_len)
{
    struct ieee80211_measrsp_ie *mie = (struct ieee80211_measrsp_ie *) frm;
    struct ieee80211_stastatsrsp *rsp =(struct ieee80211_stastatsrsp *)(&mie->rsp[0]);
    ieee80211_rrm_t rrm = vap->rrm;
    wlan_node_t ni = NULL;
    ieee80211_rrm_node_stats_t *stats = NULL;
    u_int8_t elen = mie->len,*elmid,info_len =0;

    RRM_FUNCTION_ENTER;

    ni = ieee80211_vap_find_node(vap, rrm->rrm_macaddr);
    if( ni == NULL )
        return -EINVAL;

    stats = ni->ni_rrm_stats;

    if( mie->token > ni->stastats_measure_token
            || mie->len < RRM_MIN_RSP_LEN ) /* minimum length should be three */
    {
        ieee80211_free_node(ni);
        return -EINVAL;
    }

    /* element id  len  measurement token  measurement mode  measurement type
        1 +         1      +1                    +1         +     1 */

    elen -=(sizeof(struct ieee80211_measrsp_ie) -
	    (sizeof(struct ieee80211_ie_header) + 1) /* elementid + length + rsp */);
    frm +=(sizeof(struct ieee80211_measrsp_ie) -1);

    switch(rsp->gid)
    {
        case IEEE80211_STASTATS_GID0: {
                ieee80211_rrm_statsgid0_t *gid = &stats->gid0;
                OS_MEMCPY(gid,&rsp->stats.gid0,sizeof(ieee80211_rrm_statsgid0_t));
                set_frmcnt_window(rrm,rsp->stats.gid0.txfrmcnt);
                info_len = sizeof(ieee80211_rrm_statsgid0_t) - 1;
            }
            break;
        case IEEE80211_STASTATS_GID1: {
                ieee80211_rrm_statsgid1_t *gid = &stats->gid1;
                OS_MEMCPY(gid,&rsp->stats.gid1,sizeof(ieee80211_rrm_statsgid1_t));
                set_ackfail_window(rrm,rsp->stats.gid1.ackfail);
                info_len = sizeof(ieee80211_rrm_statsgid1_t) - 1;
            }
            break;
        case IEEE80211_STASTATS_GID2:
        case IEEE80211_STASTATS_GID3:
        case IEEE80211_STASTATS_GID4:
        case IEEE80211_STASTATS_GID5:
        case IEEE80211_STASTATS_GID6:
        case IEEE80211_STASTATS_GID7:
        case IEEE80211_STASTATS_GID8:
        case IEEE80211_STASTATS_GID9: {
                ieee80211_rrm_statsgidupx_t *gid = &stats->gidupx[rsp->gid - 2];
                OS_MEMCPY(gid,&rsp->stats.upstats,sizeof(ieee80211_rrm_statsgidupx_t));
                info_len = sizeof(ieee80211_rrm_statsgidupx_t) - 1;
            }
            break;
        case IEEE80211_STASTATS_GID10: {
                ieee80211_rrm_statsgid10_t *gid = &stats->gid10;
                OS_MEMCPY(gid,&rsp->stats.gid10,sizeof(ieee80211_rrm_statsgid10_t));
                set_stcnt_window(rrm,rsp->stats.gid10.st_cnt);
                set_be_window(rrm,rsp->stats.gid10.be_avg_delay);
                set_vo_window(rrm,rsp->stats.gid10.vo_avg_delay);
                info_len = sizeof(ieee80211_rrm_statsgid10_t) - 1;
            }
            break;
        default:
            break;
    }
    info_len += (sizeof(rsp->m_intvl) + sizeof(rsp->gid));
    frm += info_len;
    elen-= info_len;
    while(elen > sizeof(struct ieee80211_ie_header)) {
        elmid = frm++;
        info_len = *frm++;
        if (info_len == 0) {
            frm++;    /* next IE */
            continue;
        }
        if (elen < info_len) {
            /* Incomplete/bad info element */
            ieee80211_free_node(ni);
            return -EINVAL;
        }
        switch(*elmid) {
            case IEEE80211_ELEMID_VENDOR:
                /*TBD*/
                break;
            default:
                break;
        }
        elen -= info_len;
        frm += info_len;
    }

    ieee80211_free_node(ni);

    RRM_FUNCTION_EXIT;
    return EOK;
}
/**
 * @brief CCA report parsing
 *
 * @param vap
 * @param frm
 * @param frm_len
 *
 * @return
 * @return on success return 0.
 *         on failure returns -ve value.
 */

int ieee80211_rrm_cca_report(wlan_if_t vap, u_int8_t *frm, u_int32_t frm_len)
{

    struct ieee80211_measrsp_ie *mie= (struct ieee80211_measrsp_ie *) frm;
    struct ieee80211_ccarsp  *cca = (struct ieee80211_ccarsp *) (&mie->rsp[0]);
    u_int8_t elen = mie->len,*elmid;
    ieee80211_rrm_cca_data_t *ucca = &vap->rrm->user_cca_resp[cca->chnum];
    struct ieee80211_node *ni = NULL;

    RRM_FUNCTION_ENTER;

    ni = ieee80211_vap_find_node(vap, vap->rrm->rrm_macaddr);
    if( ni == NULL )
        return -EINVAL;

    if( mie->token > ni->cca_measure_token || elen < RRM_MIN_RSP_LEN ) /*minimum elen should be three */
    {
        ieee80211_free_node(ni);
        return -EINVAL;
    }

    /* element id  len  measurement token  measurement mode  measurement type
        1 +         1      +1                    +1         +     1 */
    elen -= ((sizeof(struct ieee80211_measrsp_ie) -
              (sizeof(struct ieee80211_ie_header) + 1)/* elementid + length + rsp */)
            + (sizeof(struct ieee80211_ccarsp) - 1));

    /* Channel  Mstart  mduration CCA_BUSY
       1        +8      +2        +1     */
    frm += ((sizeof(struct ieee80211_measrsp_ie) -1)
            + (sizeof(struct ieee80211_ccarsp) - 1));
    ucca->cca_busy = cca->cca_busy;
    while(elen  > sizeof(struct ieee80211_ie_header) )
    {
        u_int8_t info_len ;

        elmid = frm++;
        info_len = *frm++;

        if (info_len == 0) {
            frm++;    /* next IE */
            continue;
        }

        if (elen < info_len) {
            /* Incomplete/bad info element */
            ieee80211_free_node(ni);
            return -EINVAL;
        }
        switch(*elmid)
        {
            case IEEE80211_ELEMID_VENDOR:
                /*handle vendor ie*/
                break;
            default:
                break;
        }
        elen-=info_len;
        frm +=info_len;
    }

    ieee80211_free_node(ni);
    RRM_FUNCTION_EXIT;
    return EOK;
}


/**
 * @brief RPI histogram report parsing
 *
 * @param vap
 * @param frm
 * @param frm_len
 *
 * @return
 * @return on success return 0.
 *         on failure returns -ve value.
 */

int ieee80211_rrm_rpihist_report(wlan_if_t vap, u_int8_t *frm, u_int32_t frm_len)
{

    struct ieee80211_measrsp_ie *mie= (struct ieee80211_measrsp_ie *) frm;
    struct ieee80211_rpihistrsp  *rpihist = (struct ieee80211_rpihistrsp *) (&mie->rsp[0]);
    u_int8_t elen = mie->len,*elmid;
    ieee80211_rrm_rpi_data_t *urpihist = &vap->rrm->user_rpihist_resp[rpihist->chnum];
    struct ieee80211_node *ni = NULL;

    RRM_FUNCTION_ENTER;

    ni = ieee80211_vap_find_node(vap, vap->rrm->rrm_macaddr);
    if( ni == NULL )
        return -EINVAL;

    if( mie->token > ni->rpihist_measure_token || elen < RRM_MIN_RSP_LEN ) /*minimum elen should be three */
    {
        ieee80211_free_node(ni);
        return -EINVAL;
    }

    /* element id  len  measurement token  measurement mode  measurement type
        1 +         1      +1                    +1         +     1 */
    elen -= ((sizeof(struct ieee80211_measrsp_ie) -
              (sizeof(struct ieee80211_ie_header) + 1)/* elementid + length + rsp */)
            + (sizeof(struct ieee80211_rpihistrsp) - 1));

    /* Channel  Mstart  mduration  RPI
       1 +      +8      +2         +10 */
    frm += ((sizeof(struct ieee80211_measrsp_ie) -1)
            + (sizeof(struct ieee80211_rpihistrsp) - 1));
    OS_MEMCPY(urpihist->rpi,rpihist->rpi,IEEE80211_RRM_RPI_SIZE);

    while(elen  > sizeof(struct ieee80211_ie_header) )
    {
        u_int8_t info_len ;

        elmid = frm++;
        info_len = *frm++;

        if (info_len == 0) {
            frm++;    /* next IE */
            continue;
        }

        if (elen < info_len) {
            /* Incomplete/bad info element */
            ieee80211_free_node(ni);
            return -EINVAL;
        }
        switch(*elmid)
        {
            case IEEE80211_ELEMID_VENDOR:
                /*handle vendor ie*/
                break;
            default:
                break;
        }
        elen-=info_len;
        frm +=info_len;
    }

    ieee80211_free_node(ni);
    RRM_FUNCTION_EXIT;
    return EOK;
}



/**
 * @brief Noise histogram report parsing
 *
 * @param vap
 * @param ni
 * @param rpt
 *
 * @return
 * @return on success return 0.
 *         on failure returns -ve value.
 */

int ieee80211_rrm_nhist_report(wlan_if_t vap, u_int8_t *frm, u_int32_t frm_len)
{

    struct ieee80211_measrsp_ie *mie= (struct ieee80211_measrsp_ie *) frm;
    struct ieee80211_nhistrsp  *nhist = (struct ieee80211_nhistrsp *) (&mie->rsp[0]);
    u_int8_t elen = mie->len,*elmid;
    ieee80211_rrm_noise_data_t *unhist = &vap->rrm->user_nhist_resp[nhist->chnum];
    struct ieee80211_node *ni = NULL;

    RRM_FUNCTION_ENTER;

    ni = ieee80211_vap_find_node(vap, vap->rrm->rrm_macaddr);
    if( ni == NULL )
        return -EINVAL;

    if( mie->token > ni->nhist_measure_token || elen < RRM_MIN_RSP_LEN ) /*minimum elen should be three */
    {
        ieee80211_free_node(ni);
        return -EINVAL;
    }

    /* element id  len  measurement token  measurement mode  measurement type
        1 +         1      +1                    +1         +     1 */
    elen -= ((sizeof(struct ieee80211_measrsp_ie) -
	      (sizeof(struct ieee80211_ie_header) + 1)/* elementid + length + rsp */)
            + (sizeof(struct ieee80211_nhistrsp) - 1));

    /* Rclass  Channel  Mstart  mduration  antennaid ANPI  IPI
       1 +      +1      +8        2          +1       + 1  10 */
    frm += ((sizeof(struct ieee80211_measrsp_ie) -1)
            + (sizeof(struct ieee80211_nhistrsp) - 1));
    unhist->anpi = (int8_t)nhist->anpi;
    unhist->antid = nhist->antid;
#define IPI_SIZE 11
    OS_MEMCPY(unhist->ipi,nhist->ipi,IPI_SIZE);
#undef IPI_SIZE

    RRM_DEBUG(vap, RRM_DEBUG_INFO,
              "%s : duration %d chnum %d regclass %d anpi %d antid %d  anpi %d\n ", __func__,
               nhist->mduration, nhist->chnum, nhist->regclass, nhist->anpi, nhist->antid, (int8_t) nhist->anpi);

    while(elen  > sizeof(struct ieee80211_ie_header) )
    {
        u_int8_t info_len ;

        elmid = frm++;
        info_len = *frm++;

        if (info_len == 0) {
            frm++;    /* next IE */
            continue;
        }

        if (elen < info_len) {
            /* Incomplete/bad info element */
            ieee80211_free_node(ni);
            return -EINVAL;
        }
        switch(*elmid)
        {
            case IEEE80211_ELEMID_VENDOR:
                /*handle vendor ie*/
                break;
            default:
                break;
        }
        elen-=info_len;
        frm +=info_len;
    }
    set_anpi_window(vap->rrm,unhist->anpi);

    ieee80211_free_node(ni);
    RRM_FUNCTION_EXIT;
    return EOK;
}

/**
 * @brief i Location configuration request
 *
 * @param lcirpt_data
 * @param lci_entry
 */

static void ieee80211_rrm_lci_report_parse(const u_int8_t *lcirpt_data,
                                           ieee80211_rrm_lci_data_t *lci_entry)
{
    u_int32_t param, bit;
    u_int32_t param_val;
    u_int32_t idx, bit_idx;
    u_int32_t bit_off, bit_len;
    struct rrm_lci_report_struct {
        u_int32_t bit_offset;
        u_int32_t bit_len;
    }lcirpt_fields[IEEE80211_RRM_LCI_LAST] =  {
        {0,   8}, /* IEEE80211_RRM_LCI_ID */
        {8,   8}, /* IEEE80211_RRM_LCI_LEN */
        {16,  6}, /* IEEE80211_RRM_LCI_LAT_RES */
        {22, 25}, /* IEEE80211_RRM_LCI_LAT_FRAC */
        {47,  9}, /* IEEE80211_RRM_LCI_LAT_INT */
        {56,  6}, /* IEEE80211_RRM_LCI_LONG_RES */
        {62, 25}, /* IEEE80211_RRM_LCI_LONG_FRAC */
        {87,  9}, /* IEEE80211_RRM_LCI_LONG_INT */
        {96,  4}, /* IEEE80211_RRM_LCI_ALT_TYPE */
        {100, 6}, /* IEEE80211_RRM_LCI_ALT_RES */
        {106, 8}, /* IEEE80211_RRM_LCI_ALT_FRAC */
        {114,22}, /* IEEE80211_RRM_LCI_ALT_INT */
        {136, 8}, /* IEEE80211_RRM_LCI_DATUM */
    };
    for (param = IEEE80211_RRM_LCI_ID; param < IEEE80211_RRM_LCI_LAST; param++)
    {
        param_val = 0;
        bit_off = lcirpt_fields[param].bit_offset;
        bit_len = lcirpt_fields[param].bit_len;
        for (bit = 0; bit < bit_len; bit++)
        {
            idx = ((bit_off )>>3); /*dividing by 8 */
            bit_idx = ((bit_off) % 8);
            if (lcirpt_data[idx] & (0x1 << bit_idx))
            {
                param_val |= (1 << bit);
            }
            else
            {
                param_val |= (0 << bit);
            }
            bit_off++;
        }
        switch(param)
        {
            case IEEE80211_RRM_LCI_ID:
                lci_entry->id = (u_int8_t)param_val;
                break;
            case IEEE80211_RRM_LCI_LEN:
                lci_entry->len = (u_int8_t)param_val;
                break;
            case IEEE80211_RRM_LCI_LAT_RES:
                lci_entry->lat_res = (u_int8_t)param_val;
                break;
            case IEEE80211_RRM_LCI_LAT_FRAC:
                lci_entry->lat_frac = (u_int32_t)param_val;
                break;
            case IEEE80211_RRM_LCI_LAT_INT:
                lci_entry->lat_integ = (u_int16_t)param_val;
                break;
            case IEEE80211_RRM_LCI_LONG_RES:
                lci_entry->long_res = (u_int8_t)param_val;
                break;
            case IEEE80211_RRM_LCI_LONG_FRAC:
                lci_entry->long_frac = (u_int32_t)param_val;
                break;
            case IEEE80211_RRM_LCI_LONG_INT:
                lci_entry->long_integ = (u_int16_t)param_val;
                break;
            case IEEE80211_RRM_LCI_ALT_TYPE:
                lci_entry->alt_type = (u_int8_t) param_val;
                break;
            case IEEE80211_RRM_LCI_ALT_RES:
                lci_entry->alt_res = (u_int8_t) param_val;
                break;
            case IEEE80211_RRM_LCI_ALT_FRAC:
                lci_entry->alt_frac = (u_int8_t) param_val;
                break;
            case IEEE80211_RRM_LCI_ALT_INT:
                lci_entry->alt_integ = (u_int32_t) param_val;
                break;
            case IEEE80211_RRM_LCI_DATUM:
                lci_entry->datum = (u_int8_t) param_val;
                break;
            default:
                break;
        }
    }
    return;
}

/**
 * @brief Parse location configuration report
 *
 * @param vap
 * @param ni
 * @param rpt
 *
 * @return
 * @return on success return 0.
 *         on failure returns -ve value.
 */

int ieee80211_rrm_lci_report(wlan_if_t vap, u_int8_t *frm, u_int32_t frm_len)
{
    struct ieee80211_measrsp_ie *mie= (struct ieee80211_measrsp_ie *) frm;
    struct ieee80211_lcirsp  *lcirpt = (struct ieee80211_lcirsp *) (&mie->rsp[0]);
    ieee80211_rrm_lci_data_t lci_entry;
    ieee80211_rrm_t rrm = vap->rrm;
    wlan_node_t ni = NULL;
    u_int16_t azimuth_report;
    u_int8_t *lci_data = (&lcirpt->lci_data[0]);
    u_int8_t elen = mie->len,*elmid;
    ieee80211_rrm_node_stats_t *stats = NULL;

    RRM_FUNCTION_ENTER;

    ni = ieee80211_vap_find_node(vap, rrm->rrm_macaddr);
    if( ni == NULL )
        return -EINVAL;

    stats =(ieee80211_rrm_node_stats_t *) ni->ni_rrm_stats;

    if( mie->token > ni->lci_measure_token || elen < RRM_MIN_RSP_LEN ) /*minimum elen should be three */
    {
        ieee80211_free_node(ni);
        return -EINVAL;
    }

    /* element id  len  measurement token  measurement mode  measurement type
        1 +         1      +1                    +1         +     1 */
    elen -= ((sizeof(struct ieee80211_measrsp_ie) -
		(sizeof(struct ieee80211_ie_header) + 1) /* elementid + length + rsp */)
            + (sizeof(struct ieee80211_lcirsp) - 1));
    frm += ((sizeof(struct ieee80211_measrsp_ie) -1)
            + (sizeof(struct ieee80211_lcirsp) - 1));

    OS_MEMSET(&lci_entry, 0x0, sizeof(lci_entry));
    ieee80211_rrm_lci_report_parse(lci_data, &lci_entry);

    while(elen  > sizeof(struct ieee80211_ie_header) )
    {
        u_int8_t info_len ;

        elmid = frm++;
        info_len = *frm++;

        if (info_len == 0) {
            frm++;    /* next IE */
            continue;
        }

        if (elen < info_len) {
            /* Incomplete/bad info element */
            ieee80211_free_node(ni);
            return -EINVAL;
        }
        switch(*elmid)
        {
            case IEEE80211_SUBELEMID_LCI_AZIMUTH_REPORT:
                OS_MEMCPY(&azimuth_report, frm, 2);
                lci_entry.azi_type =  (azimuth_report >> 2) & 0x1;
                lci_entry.azi_res = (azimuth_report >> 3) & 0xf;
                lci_entry.azimuth = (azimuth_report >> 7) & 0x1ff;
            case IEEE80211_SUBELEMID_LCI_VENDOR:
            case IEEE80211_SUBELEMID_LCI_RESERVED:
                /*handle vendor ie*/
                break;
            default:
                break;
        }
        elen-=info_len;
        frm +=info_len;
    }
    if (ni->ni_rrmlci_loc == 0)
        OS_MEMCPY(&stats->ni_vap_lciinfo, &lci_entry, sizeof(lci_entry));
    else if (ni->ni_rrmlci_loc == 1)
        OS_MEMCPY(&stats->ni_rrm_lciinfo, &lci_entry, sizeof(lci_entry));

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_RRM,
            " \t%2x %2x %2x %2x %2x %2x\t %d \t %d  %d  %d  %d \n",
            lci_entry.id, lci_entry.len,
            lci_entry.lat_res,lci_entry.lat_frac,
            lci_entry.lat_integ,lci_entry.long_res,
            lci_entry.long_frac,lci_entry.long_integ,
            lci_entry.alt_res, lci_entry.alt_integ, lci_entry.alt_frac);

    ieee80211_free_node(ni);
    RRM_FUNCTION_EXIT;
    return EOK;
}


/**
 * @brief Routine to flush beacon table , will be called every time beacon report come
 *
 * @param vap
 */

void ieee80211_flush_beacontable(wlan_if_t vap)
{
    struct ieee80211_beacon_report_table *btable = vap->rrm->beacon_table;
    RRM_FUNCTION_ENTER ;

    RRM_BTABLE_LOCK(btable);
    {
        struct ieee80211_beacon_entry   *current_beacon_entry,*beacon;
        struct ieee80211_beacon_report *report;
        /* limit the scope of this variable */

        TAILQ_FOREACH_SAFE(current_beacon_entry ,&(btable->entry),blist,beacon)
        {
            report = &(current_beacon_entry->report);
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_RRM,
                    "\t%2x %2x %2x %2x %2x %2x\t %d \t %d \n",
                    report->bssid[0],report->bssid[1],
                    report->bssid[2],report->bssid[3],
                    report->bssid[4],report->bssid[5],
                    report->ch_num,report->rcpi);
            TAILQ_REMOVE(&(btable->entry), current_beacon_entry, blist);
            OS_FREE(current_beacon_entry);
        }
    }
    RRM_BTABLE_UNLOCK(btable);
    RRM_FUNCTION_EXIT;
    return;
}

/**
 * @brief Routine to add beacon entry in list
 *
 * @param vap
 * @param bcnrpt
 */

void add_beacon_entry(wlan_if_t vap ,struct ieee80211_beacon_report *bcnrpt)
{
    struct ieee80211_beacon_report_table *btable = vap->rrm->beacon_table;
    uint8_t flag =0;

    RRM_BTABLE_LOCK(btable);
    {
        struct ieee80211_beacon_entry   *current_beacon_entry;
        struct ieee80211_beacon_report *report;
        /* limit the scope of this variable */
        TAILQ_FOREACH(current_beacon_entry ,&(btable->entry),blist)
        {
            report = &current_beacon_entry->report;
            if(IEEE80211_ADDR_EQ(report->bssid,bcnrpt->bssid)) {
                flag = 1;
                break;
            }
        }
        if(!flag) { /* Entry not found */
            current_beacon_entry  = (struct ieee80211_beacon_entry *)
                OS_MALLOC(vap->rrm->rrm_osdev, sizeof(struct ieee80211_beacon_entry), 0);
            if (!current_beacon_entry) {
                qdf_err("Memory allocation failed! \n");
                RRM_BTABLE_UNLOCK(btable);
                return;
            }
            OS_MEMZERO(current_beacon_entry, sizeof(struct ieee80211_beacon_entry));
            report = &current_beacon_entry->report;
            TAILQ_INSERT_TAIL(&(btable->entry), current_beacon_entry, blist);
        }
        report->reg_class = bcnrpt->reg_class;
        report->ch_num = bcnrpt->ch_num;
        report->frame_info = bcnrpt->frame_info ;
        report->rcpi= bcnrpt->rcpi;
        report->rsni = bcnrpt->rsni;
        IEEE80211_ADDR_COPY(report->bssid,bcnrpt->bssid);
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_RRM,
                " \t%2x %2x %2x %2x %2x %2x\t %d \t %d \n",
                report->bssid[0],report->bssid[1],
                report->bssid[2],report->bssid[3],
                report->bssid[4],report->bssid[5],
                report->ch_num,report->rcpi);
    }
    RRM_BTABLE_UNLOCK(btable);
}

int ieee80211_rrm_is_valid_beacon_measurement_report(wlan_if_t vap,
        u_int8_t *sfrm,u_int32_t frm_len)
{
    struct ieee80211_measrsp_ie *mie ;
    u_int8_t *efrm = sfrm + frm_len;
    struct ieee80211_node *ni = NULL;

    RRM_FUNCTION_ENTER;

    ni = ieee80211_vap_find_node(vap, vap->rrm->rrm_macaddr);
    if( ni == NULL )
        return -EINVAL;

    while (sfrm < efrm )
    {
        mie= (struct ieee80211_measrsp_ie *) sfrm;

        if ( mie->len == 0) {
            sfrm++; /*id */
            sfrm++; /*len */
            continue;
        }
        if (mie->len < sizeof(struct ieee80211_ie_header) ||
                sfrm + sizeof(struct ieee80211_ie_header) + mie->len > efrm) {
            ieee80211_free_node(ni);
            return 0;
        }
        switch(mie->id)
        {
            case IEEE80211_ELEMID_MEASREP:
                {
                    /* A measurement report with an empty beacon report is still
                     * valid as per the 802.11k standard.
                     */
                    u_int8_t min_ie_len = (sizeof(struct ieee80211_measrsp_ie) -
                            sizeof(struct ieee80211_ie_header) - 1);
                    if (mie->len >= min_ie_len &&
                            mie->token <= ni->br_measure_token) {
                        ieee80211_free_node(ni);
                        return 1;
                    }
                    sfrm += mie->len + sizeof(struct ieee80211_ie_header);
                    break;
                }
            case IEEE80211_SUBELEMID_BREPORT_FRAME_BODY:
                /*handle Frame body here */
                /* XXX: follow-thru to default case */
            case IEEE80211_SUBELEMID_BREPORT_RESERVED:
                /*Reserved   */
                /* XXX: follow-thru to default case */
            default:
                sfrm += mie->len + sizeof(struct ieee80211_ie_header);
                break;
        }
    }

    ieee80211_free_node(ni);
    RRM_FUNCTION_EXIT;

    return 0;
}

/**
 * @brief Check a measurement response ie and store the 
 *        information.  If compiled with QCA_SUPPORT_SON, also
 *        store in the beacon report structure.
 *  
 * @param [in] vap VAP this packet was received on 
 * @param [in] mie pointer to the measurement response ie
 * @param [in] sfrm pointer to the current position in the 
 *                  packet
 * @param [in] token current measurement response token
 * @param [out] report  beacon report to fill in 
 *                      (QCA_SUPPORT_SON only)
 */
static void ieee80211_rrm_handle_measurement_response_ie(
    wlan_if_t vap,
    struct ieee80211_measrsp_ie *mie,
    u_int8_t *sfrm,
#if QCA_SUPPORT_SON
    u_int8_t token,
    ieee80211_bcnrpt_t *report) {
#else
    u_int8_t token) {
#endif
    u_int8_t zero_bssid[IEEE80211_ADDR_LEN] = {0, 0, 0, 0, 0, 0};
    u_int8_t min_ie_len = (sizeof(struct ieee80211_measrsp_ie) -
                           sizeof(struct ieee80211_ie_header) - 1) +
        (sizeof(struct ieee80211_beacon_report) - 1);
    if (mie->token <= token) {
        if (mie->len >= min_ie_len) {
            /* There is a beacon report present */
            struct ieee80211_beacon_report *bcnrpt =
                (struct ieee80211_beacon_report *)(sfrm +
                                                   sizeof(struct ieee80211_measrsp_ie) - 1);
            if (!IEEE80211_ADDR_EQ(&bcnrpt->bssid, &zero_bssid) &&
                bcnrpt->ch_num) {
                /* search for this bssid in all available list */
                add_beacon_entry(vap, bcnrpt);
            }
#if QCA_SUPPORT_SON
            if (report) {
               /* Still send notification even for an invalid BSSID / channel so
                * that band steering can identify that an error occurred
                */
                IEEE80211_ADDR_COPY(report->bssid, bcnrpt->bssid);
                report->rsni = bcnrpt->rsni;
                report->rcpi = bcnrpt->rcpi;
                report->chnum = bcnrpt->ch_num;
            }
        } else {
                /*
                 * There is no beacon report present.
                 * Still fill in the report to be NULL and report up
                 */
                if (report) {
                    memset(report, 0, sizeof(ieee80211_bcnrpt_t));
                }
#endif /* QCA_SUPPORT_SON */
        }
    }
}

/**
 * @brief Parse beacon measurement report
 *
 * @param vap
 * @param ni
 * @param rsp
 * @param frm_len
 *
 * @return 
 */

int ieee80211_rrm_beacon_measurement_report(wlan_if_t vap,
        u_int8_t *sfrm,u_int32_t frm_len)
{
    struct ieee80211_measrsp_ie *mie ;
#if QCA_SUPPORT_SON
    ieee80211_bcnrpt_t *reports = NULL;
    u_int8_t num_bcnrpt_requested = 0;
    u_int8_t bcnrpt_count = 0;
    u_int8_t *frame = sfrm;
    u_int8_t total_bcnrpt_count = 0;
#endif
    u_int8_t *efrm = sfrm + frm_len;
    struct ieee80211_node *ni = NULL;
    RRM_FUNCTION_ENTER;
    ni = ieee80211_vap_find_node(vap, vap->rrm->rrm_macaddr);
    if (ni == NULL)
        return -EINVAL;

    if (!ieee80211_rrm_is_valid_beacon_measurement_report(vap, sfrm, frm_len))
    {
        ieee80211_free_node(ni);
        return EOK;
    }

#if QCA_SUPPORT_SON
    num_bcnrpt_requested = son_alloc_rrm_bcnrpt(vap->vdev_obj, &reports);
#endif

    while (sfrm < efrm )
    {
        mie= (struct ieee80211_measrsp_ie *) sfrm;

        if ( mie->len == 0) {
            sfrm++; /*id */
            sfrm++; /*len */
#if QCA_SUPPORT_SON
            total_bcnrpt_count++;
#endif
            continue;
        }
        if (mie->len < sizeof(struct ieee80211_ie_header) ||
                sfrm + sizeof(struct ieee80211_ie_header) + mie->len > efrm) {
            break;
        }

        switch (mie->id)
        {
            case IEEE80211_ELEMID_MEASREP:
                {
#if QCA_SUPPORT_SON
                    ieee80211_rrm_handle_measurement_response_ie(
                            vap, mie, sfrm, ni->br_measure_token,
                           num_bcnrpt_requested ? &reports[bcnrpt_count] : NULL);
                    if (num_bcnrpt_requested) {
                            if (bcnrpt_count) {
                                    reports[(bcnrpt_count - 1)].more = 1;
                            }
                            ++bcnrpt_count;
                            if (bcnrpt_count == num_bcnrpt_requested) {
                                    /* Reach memory limit, but there are more reports to send */
                                    son_send_rrm_bcnrpt_event(
                                            vap->vdev_obj, IEEE80211_ACTION_RM_TOKEN,
                                            vap->rrm->rrm_macaddr, reports,
                                            bcnrpt_count);
                                    bcnrpt_count = 0;
                            }
                    }
#else
                        ieee80211_rrm_handle_measurement_response_ie(
                                vap, mie, sfrm, ni->br_measure_token);
#endif
                        sfrm += mie->len + sizeof(struct ieee80211_ie_header);
                        break;
                }
        case IEEE80211_SUBELEMID_BREPORT_FRAME_BODY:
                /*handle Frame body here */
                /* XXX: follow-thru to default case */
        case IEEE80211_SUBELEMID_BREPORT_RESERVED:
                /*Reserved   */
                /* XXX: follow-thru to default case */
        default:
                sfrm += mie->len + sizeof(struct ieee80211_ie_header);
                break;
        }
#if QCA_SUPPORT_SON
        total_bcnrpt_count++;
#endif
    }
#if QCA_SUPPORT_SON
    if (bcnrpt_count) {
            son_send_rrm_bcnrpt_event(
                    vap->vdev_obj, IEEE80211_ACTION_RM_TOKEN, vap->rrm->rrm_macaddr,
                    reports, bcnrpt_count);
    }
    son_dealloc_rrm_bcnrpt(&reports);
    if (total_bcnrpt_count) {
        son_send_rrm_frame_bcnrpt_event(
            vap->vdev_obj, IEEE80211_ACTION_RM_TOKEN, vap->rrm->rrm_macaddr,
            frame, frm_len, total_bcnrpt_count);
    }
#endif

    ieee80211_free_node(ni);
    RRM_FUNCTION_EXIT;
    return EOK;
}

static void ieee80211_add_self_nrinfo(struct ieee80211vap *vap, struct ieee80211_rrm_cbinfo * cb_info)
{
    /*AP adds itself in Neighbor element */
    struct ieee80211_nrresp_info ninfo;
    struct ieee80211_ath_channel *chan;

    OS_MEMZERO(&ninfo, sizeof(struct ieee80211_nrresp_info));

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ACTION, "Nbr Request is for LCI/LCR, Add self as NbrElement, BSSID: %s\n", ether_sprintf(vap->iv_bss->ni_bssid));
    IEEE80211_ADDR_COPY(ninfo.bssid, vap->iv_bss->ni_bssid);
    chan = vap->iv_bsschan;
    ninfo.channum = chan->ic_ieee;
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

    ninfo.phytype = ieee80211_rrm_get_phy_type(vap->iv_cur_mode);

    if (ieee80211_vap_rrm_is_set(vap)) {
        ninfo.capinfo |= IEEE80211_CAPINFO_RADIOMEAS;
    }
    if (ieee80211_vap_doth_is_set(vap)) {
        ninfo.capinfo |= IEEE80211_CAPINFO_SPECTRUM_MGMT;
    }
    if (ieee80211_vap_wme_is_set(vap) &&
        (IEEE80211_IS_CHAN_11AC(vap->iv_bsschan) || IEEE80211_IS_CHAN_11NG(vap->iv_bsschan)) &&
            ieee80211vap_vhtallowed(vap)) {
               ninfo.is_vht = 1;
    }
    if (ieee80211_vap_wme_is_set(vap) &&
        (IEEE80211_IS_CHAN_11AC(vap->iv_bsschan) || IEEE80211_IS_CHAN_11N(vap->iv_bsschan))) {
        ninfo.is_ht = 1;
    }
#if SUPPORT_11AX_D3
    if (ieee80211_vap_wme_is_set(vap) &&
            IEEE80211_IS_CHAN_11AX(vap->iv_bsschan) &&
            ieee80211vap_heallowed(vap)) {
        ninfo.is_he = 1;
        ninfo.is_he_er_su = (vap->iv_ic->ic_he.heop_param &
                            IEEE80211_HEOP_ER_SU_DISABLE_MASK) >>
                            IEEE80211_HEOP_ER_SU_DISABLE_S;
    }
#endif
    /* 11AX TODO (Phase II) - Revisit later */
    ninfo.is_ftm = (vap->rtt_enable) ? 1 : 0;  /* If RTT is enabled, FTM enabled */
    ninfo.preference = cb_info->preference;

    /* Copy all measurement report related params */
    /* This includes meas_count, meas_type, meas_req_mode, loc_sub */
    OS_MEMCPY(&ninfo.meas_count, &cb_info->meas_count, IEEE80211_NRREQ_MEASREQ_FIELDS_LEN);

    /* Get co-located bssids for self */
#if ATH_SUPPORT_WIFIPOS
    wlan_iterate_vap_list(vap->iv_ic, ieee80211_vap_iter_get_colocated_bss,(void *) &(ninfo.colocated_bss));
#endif
    cb_info->frm = ieee80211_add_nr_ie(cb_info->frm, &ninfo);
}

#endif /* UMAC_SUPPORT_RRM */
