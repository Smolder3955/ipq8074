/*
 *  Copyright (c) 2014,2017 Qualcomm Innovation Center, Inc.
 *  All Rights Reserved.
 *  Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 *  2014 Qualcomm Atheros, Inc.  All rights reserved.
 *
 *  Qualcomm is a trademark of Qualcomm Technologies Incorporated, registered in the United
 *  States and other countries.  All Qualcomm Technologies Incorporated trademarks are used with
 *  permission.  Atheros is a trademark of Qualcomm Atheros, Inc., registered in
 *  the United States and other countries.  Other products and brand names may be
 *  trademarks or registered trademarks of their respective owners. 
 */


/*
 *  Radio Resource measurements message handlers for Station .
 */

#include <ieee80211_var.h>
#include "ieee80211_rrm_priv.h"

#if UMAC_SUPPORT_RRM

/**
 * @brief  Routine to send reject frame
 *
 * @param rrm
 * @param rm_req
 *
 * @return on success return 0.
 *         on failure returns -ve value.
 */
int rrm_send_reject(ieee80211_rrm_t rrm,
       struct ieee80211_action_rm_req *rm_req )
{
    struct ieee80211_rrmreq_info *params;
    struct ieee80211_measreq_ie *req;
#if UMAC_SUPPORT_RRM_DEBUG
    wlan_if_t vap = rrm->rrm_vap;
#endif
    RRM_FUNCTION_ENTER;

    req = (struct ieee80211_measreq_ie *)(&(rm_req->req_ies[0]));
    params = (struct ieee80211_rrmreq_info *)
               OS_MALLOC(rrm->rrm_osdev, sizeof(struct ieee80211_rrmreq_info), 0);

    if(NULL == params)
        return -EBUSY;

    params->rm_dialogtoken = rm_req->dialogtoken;
    params->rep_dialogtoken = req->token ;
    params->reject_type = req->reqtype;
    params->reject_mode = IEEE80211_RRM_MEASRPT_MODE_BIT_REFUSED;

    ieee80211_rrm_set_report_pending(rrm->rrm_vap,IEEE80211_MEASREQ_OTHER,(void *)params);

    if(rrm->pending_report)
        ieee80211_send_report(rrm);

    RRM_FUNCTION_EXIT;

    return EOK;
}

/**
 * @brief routine to recv chload request
 *
 * @param vap
 * @param ni
 * @param req
 * @param rm_token
 *
 * @return on success return 0.
 *         on failure returns -ve value.
 */

int ieee80211_rrm_recv_chload_req(wlan_if_t vap, wlan_node_t ni,
        struct ieee80211_measreq_ie *req,u_int8_t rm_token)
{
    struct ieee80211_chloadreq *chload;
    struct ieee80211_rrmreq_info *params;

    RRM_FUNCTION_ENTER;

    chload = (struct ieee80211_chloadreq *)(&req->req[0]);
    params = (struct ieee80211_rrmreq_info *)
        OS_MALLOC(vap->rrm->rrm_osdev, sizeof(struct ieee80211_rrmreq_info), 0);

    if(NULL == params)
        return -EBUSY;

    params->rm_dialogtoken = rm_token;
    params->rep_dialogtoken = req->token;
    params->duration = chload->mduration;
    params->chnum = chload->chnum;
    params->regclass = chload->regclass;
    RRM_DEBUG(vap, RRM_DEBUG_INFO,
              "%s : duration %d chnum %d regclass %d\n", __func__,
               params->duration, params->chnum, params->regclass);

    ieee80211_rrm_set_report_pending(vap,IEEE80211_MEASREQ_CHANNEL_LOAD_REQ,(void *)params);
    if (ieee80211_rrm_scan_start(vap, true) != 0)
    {
        ieee80211_rrm_free_report(vap->rrm);
        return -EBUSY;
    }

    RRM_FUNCTION_EXIT;
    return EOK;
}

/**
 * @brief Routine to accept station statistics request
 *
 * @param vap
 * @param ni
 * @param req
 * @param rm_token
 *
 * @return
 * @return on success return 0.
 *         on failure returns -ve value.
 */
int ieee80211_rrm_recv_stastats_req(wlan_if_t vap, wlan_node_t ni,
                struct ieee80211_measreq_ie *req,u_int8_t rm_token)
{
    struct ieee80211_rrmreq_info *params;
    struct ieee80211_stastatsreq *statsreq;

    RRM_FUNCTION_ENTER;
    statsreq = (struct ieee80211_stastatsreq *)(&(req->req[0]));

    params = (struct ieee80211_rrmreq_info *)
            OS_MALLOC(vap->rrm->rrm_osdev, sizeof(struct ieee80211_rrmreq_info), 0);

    if(NULL == params)
        return -EBUSY;
    params->rm_dialogtoken = rm_token;
    params->rep_dialogtoken= req->token;
    params->duration=statsreq->mduration;
    params->gid=statsreq->gid;

    RRM_DEBUG(vap, RRM_DEBUG_INFO,
              "%s : duration %d chnum %d gid %d\n", __func__,
               params->duration, params->chnum, params->gid);

    IEEE80211_ADDR_COPY(params->bssid,statsreq->dstmac);
    ieee80211_rrm_set_report_pending(vap,IEEE80211_MEASREQ_STA_STATS_REQ,(void *)params);

    if(vap->rrm->pending_report)
        ieee80211_send_report(vap->rrm);

    RRM_FUNCTION_EXIT;
    return EOK;
}

/**
 * @brief Routine to process location request
 *
 * @param vap
 * @param ni
 * @param req
 * @param rm_token
 *
 * @return
 * @return on success return 0.
 *         on failure returns -ve value.
 */

int ieee80211_rrm_recv_lci_req(wlan_if_t vap, wlan_node_t ni,
                            struct ieee80211_measreq_ie *req, u_int8_t rm_token)
{
    struct ieee80211_rrmreq_info *params;
    struct ieee80211_lcireq *lcireq;

    RRM_FUNCTION_ENTER;

    lcireq = (struct ieee80211_lcireq *)(&(req->req[0]));

    params = (struct ieee80211_rrmreq_info *)
              OS_MALLOC(vap->rrm->rrm_osdev, sizeof(struct ieee80211_rrmreq_info), 0);

    if(NULL == params)
        return -EBUSY;

    params->rm_dialogtoken = rm_token;
    params->rep_dialogtoken = req->token;
    params->location = lcireq->location;
    params->lat_res = lcireq->lat_res;
    params->long_res = lcireq->long_res;
    params->alt_res = lcireq->alt_res;

    RRM_DEBUG(vap, RRM_DEBUG_INFO,
              "%s : location %d lat_res %d long_res %d alt_res %d\n", __func__,
               params->location, params->lat_res, params->long_res, params->alt_res);

    ieee80211_rrm_set_report_pending(vap, IEEE80211_MEASREQ_LCI_REQ, (void *)params);

    if(vap->rrm->pending_report)
        ieee80211_send_report(vap->rrm);

    RRM_FUNCTION_EXIT;
    return EOK;
}

/**
 * @brief
 *
 * @param vap
 * @param mode
 *
 * @return
 * @return on success return 0.
 *         on failure returns -ve value.
 */
int ieee80211_rrm_scan(wlan_if_t vap, u_int8_t mode)
{
    int retval = 0;
    struct wlan_objmgr_pdev *pdev = NULL;
    QDF_STATUS status;

    RRM_FUNCTION_ENTER;

    if ((mode == 0) || (mode == 1))
    {
        status = wlan_objmgr_vdev_try_get_ref(vap->vdev_obj, WLAN_OSIF_SCAN_ID);
        if (QDF_IS_STATUS_ERROR(status)) {
            scan_info("unable to get reference");
            return -EINVAL;
        }
        pdev = wlan_vdev_get_pdev(vap->vdev_obj);
        ucfg_scan_flush_results(pdev, NULL);
        wlan_objmgr_vdev_release_ref(vap->vdev_obj, WLAN_OSIF_SCAN_ID);
        /* Active mode */
        retval = ieee80211_rrm_scan_start(vap, (mode == 1)?true:false);
        if ((retval != 0) && vap->rrm->pending_report)
        {
            ieee80211_rrm_free_report(vap->rrm);
            return -EBUSY;
        }
        else
        {
             vap->rrm->rrm_last_scan = OS_GET_TIMESTAMP();
        }
    }
    else
    {
        /* Send last Beacon Table as report */
        if(vap->rrm->pending_report)
        {
            ieee80211_send_report(vap->rrm);
            return EOK;
        }
    }

    RRM_FUNCTION_EXIT;

    return EOK;
}

/**
 * @brief
 *
 * @param vap
 * @param ni
 * @param req
 * @param rm_token
 *
 * @return
 * @return on success return 0.
 *         on failure returns -ve value.
 */
int ieee80211_rrm_recv_beacon_req(wlan_if_t vap, wlan_node_t ni,
                struct ieee80211_measreq_ie *req,u_int8_t rm_token)
{
    struct ieee80211_rrmreq_info *params=NULL;
    struct ieee80211_beaconreq *bcnreq=NULL;
    u_int8_t measreq_len, remaining_subelm_len;
    u_int8_t len, subelm_id, *frm = NULL;

    RRM_FUNCTION_ENTER;


    bcnreq = (struct ieee80211_beaconreq *)(&(req->req[0]));
    params = (struct ieee80211_rrmreq_info *)
           OS_MALLOC(vap->rrm->rrm_osdev, sizeof(struct ieee80211_rrmreq_info), 0);

    if(NULL == params)
        return -EBUSY;
    params->rm_dialogtoken = rm_token;
    params->rep_dialogtoken= req->token;
    params->duration=bcnreq->duration;
    params->chnum = bcnreq->channum;
    params->ssid_len = 0;

    measreq_len = req->len;

    RRM_DEBUG(vap, RRM_DEBUG_INFO, "%s : meareq len %d, mesreq len without subelm %d\n", __func__, measreq_len,
        sizeof(req->token) + sizeof(req->reqmode) + sizeof(req->reqtype) + sizeof(struct ieee80211_beaconreq) - 1);

    /*
     * If sub element exists in the measurement request.
     */
    if (measreq_len >  sizeof(req->token) + sizeof(req->reqmode) + sizeof(req->reqtype) +
        sizeof(struct ieee80211_beaconreq) - 1) {

        remaining_subelm_len = measreq_len -
            (sizeof(req->token) + sizeof(req->reqmode) + sizeof(req->reqtype) +
            sizeof(struct ieee80211_beaconreq) - 1);

        RRM_DEBUG(vap, RRM_DEBUG_INFO, "%s : remaining_subelm_len %d \n", __func__, remaining_subelm_len);

        frm = (u_int8_t *)(&bcnreq->subelm[0]);

        while (remaining_subelm_len  >= sizeof(struct ieee80211_subelm_header) ) {

            subelm_id = *frm++;
            len = *frm++;

            switch (subelm_id) {
                case IEEE80211_SUBELEMID_BR_SSID:
                    OS_MEMCPY(params->ssid, frm, len);
                    params->ssid_len = len;
                    RRM_DEBUG(vap, RRM_DEBUG_INFO,
                        "%s : ssid %s ssid_len %d\n", __func__, params->ssid, len);
                    break;

                case IEEE80211_SUBELEMID_BR_RINFO:
                    RRM_DEBUG(vap, RRM_DEBUG_INFO,
                        "%s : IEEE80211_SUBELEMID_BR_RINFO\n", __func__);
                    break;

                case IEEE80211_SUBELEMID_BR_RDETAIL:
                    RRM_DEBUG(vap, RRM_DEBUG_INFO,
                        "%s : IEEE80211_SUBELEMID_BR_RDETAIL\n", __func__);
                    break;

                default:
                    break;
            }

            frm += len;
            remaining_subelm_len -=  sizeof(struct ieee80211_subelm_header) + len;
            RRM_DEBUG(vap, RRM_DEBUG_INFO,
                "%s : remaining_subelm_len %d, len %d\n", __func__, remaining_subelm_len, len);
        }
    }

    RRM_DEBUG(vap, RRM_DEBUG_INFO,
        "%s : duration %d chnum %d regclass %d ssid %s\n", __func__,
        params->duration, params->chnum, params->regclass, params->ssid);

    IEEE80211_ADDR_COPY(params->bssid,bcnreq->bssid);
    ieee80211_rrm_set_report_pending(vap,IEEE80211_MEASREQ_BR_TYPE,(void *)params);

    if(vap->rrm->rrm_last_scan == 0)
    {
        ieee80211_rrm_scan(vap, bcnreq->mode);
        return EOK;
    }
    else
    {
        u_int32_t last_scan_time=0,msec_current_time = 0;
        last_scan_time = (u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(vap->rrm->rrm_last_scan);
        msec_current_time = (u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(OS_GET_TIMESTAMP());
        msec_current_time -= last_scan_time;

        if(msec_current_time <  60*1000)
        {
            if(vap->rrm->pending_report)
                ieee80211_send_report(vap->rrm);
        }
        else
        {
            ieee80211_rrm_scan(vap, bcnreq->mode);
        }
    }

    RRM_FUNCTION_EXIT;
    return EOK;
}

/**
 * @brief process CCA request
 *
 * @param vap
 * @param ni
 * @param req
 * @param rm_token
 *
 * @return
 * @return on success return 0.
 *         on failure returns -ve value.
 */
int ieee80211_rrm_recv_cca_req(wlan_if_t vap, wlan_node_t ni,
              struct ieee80211_measreq_ie *req,u_int8_t rm_token)
{
    struct ieee80211_ccareq *cca=NULL;
    struct ieee80211_rrmreq_info *params=NULL;

    RRM_FUNCTION_ENTER;

    cca = (struct ieee80211_ccareq *)(&req->req[0]);
    params = (struct ieee80211_rrmreq_info *)
               OS_MALLOC(vap->rrm->rrm_osdev, sizeof(struct ieee80211_rrmreq_info), 0);

    if(NULL == params)
        return -EBUSY;

    params->rm_dialogtoken = rm_token;
    params->rep_dialogtoken = req->token;
    params->duration = cca->mduration;
    params->chnum = cca->chnum;


    RRM_DEBUG(vap, RRM_DEBUG_INFO,
              "%s : duration %d chnum %d \n", __func__,
               params->duration, params->chnum);


    ieee80211_rrm_set_report_pending(vap,IEEE80211_MEASREQ_CCA_REQ,(void *)params);

    if (ieee80211_rrm_scan_start(vap, true)) {
      ieee80211_rrm_free_report(vap->rrm);
      return -EBUSY;
    }

    RRM_FUNCTION_EXIT;
    return EOK;
}

/**
 * @brief process RPI histogram request
 *
 * @param vap
 * @param ni
 * @param req
 * @param rm_token
 *
 * @return
 * @return on success return 0.
 *         on failure returns -ve value.
 */
int ieee80211_rrm_recv_rpihist_req(wlan_if_t vap, wlan_node_t ni,
              struct ieee80211_measreq_ie *req,u_int8_t rm_token)
{
    struct ieee80211_rpihistreq *rpihist=NULL;
    struct ieee80211_rrmreq_info *params=NULL;

    RRM_FUNCTION_ENTER;

    rpihist = (struct ieee80211_rpihistreq *)(&req->req[0]);
    params = (struct ieee80211_rrmreq_info *)
               OS_MALLOC(vap->rrm->rrm_osdev, sizeof(struct ieee80211_rrmreq_info), 0);

    if(NULL == params)
        return -EBUSY;

    params->rm_dialogtoken = rm_token;
    params->rep_dialogtoken = req->token;
    params->duration = rpihist->mduration;
    params->chnum = rpihist->chnum;


    RRM_DEBUG(vap, RRM_DEBUG_INFO,
              "%s : duration %d chnum %d \n", __func__,
               params->duration, params->chnum);


    ieee80211_rrm_set_report_pending(vap,IEEE80211_MEASREQ_RPI_HISTOGRAM_REQ,(void *)params);

    if (ieee80211_rrm_scan_start(vap, true)) {
      ieee80211_rrm_free_report(vap->rrm);
      return -EBUSY;
    }

    RRM_FUNCTION_EXIT;
    return EOK;
}

/**
 * @brief process noise histogram request
 *
 * @param vap
 * @param ni
 * @param req
 * @param rm_token
 *
 * @return
 * @return on success return 0.
 *         on failure returns -ve value.
 */
int ieee80211_rrm_recv_nhist_req(wlan_if_t vap, wlan_node_t ni,
              struct ieee80211_measreq_ie *req,u_int8_t rm_token)
{
    struct ieee80211_nhistreq *nhist=NULL;
    struct ieee80211_rrmreq_info *params=NULL;

    RRM_FUNCTION_ENTER;

    nhist = (struct ieee80211_nhistreq *)(&req->req[0]);
    params = (struct ieee80211_rrmreq_info *)
               OS_MALLOC(vap->rrm->rrm_osdev, sizeof(struct ieee80211_rrmreq_info), 0);

    if(NULL == params)
        return -EBUSY;

    params->rm_dialogtoken = rm_token;
    params->rep_dialogtoken = req->token;
    params->duration = nhist->mduration;
    params->chnum = nhist->chnum;
    params->regclass = nhist->regclass;


    RRM_DEBUG(vap, RRM_DEBUG_INFO,
              "%s : duration %d chnum %d regclass %d\n", __func__,
               params->duration, params->chnum, params->regclass);

    ieee80211_rrm_set_report_pending(vap,IEEE80211_MEASREQ_NOISE_HISTOGRAM_REQ,(void *)params);

    if (ieee80211_rrm_scan_start(vap, true)) {
      ieee80211_rrm_free_report(vap->rrm);
      return -EBUSY;
    }

    RRM_FUNCTION_EXIT;
    return EOK;
}

/**
 * @brief send response to beacon request
 *
 * @param rrm
 * @param ie
 *
 * @return
 */
u_int8_t *ieee80211_rrm_send_bcnreq_resp(ieee80211_rrm_t rrm , u_int8_t *ie)
{
    struct ieee80211_rrmreq_info *params;
    struct ieee80211_rrm_cbinfo cb_info;
    struct ieee80211vap *vap =NULL;
    vap = rrm->rrm_vap;

    RRM_FUNCTION_ENTER;
    params = (struct ieee80211_rrmreq_info *)(rrm->rrmcb);
    cb_info.frm = ie;
    IEEE80211_ADDR_COPY(cb_info.bssid, params->bssid);
    cb_info.chnum = params->chnum;
    cb_info.duration = params->duration;
    cb_info.dialogtoken = params->rep_dialogtoken;
    cb_info.max_pktlen = (MAX_TX_RX_PACKET_SIZE - (sizeof(struct ieee80211_frame) + (sizeof(struct ieee80211_action_rm_rsp))));
    cb_info.ssid_len = params->ssid_len;
    if (params->ssid_len > 0)
        qdf_mem_copy(cb_info.ssid, params->ssid, params->ssid_len);

    ucfg_scan_db_iterate(wlan_vap_get_pdev(vap),
            ieee80211_fill_bcnreq_info, &cb_info);

    RRM_FUNCTION_EXIT;
    return cb_info.frm;
}

/**
 * @brief  response to chload request
 *
 * @param rrm
 * @param ie
 *
 * @return
 */
u_int8_t *ieee80211_rrm_send_chload_resp(ieee80211_rrm_t rrm , u_int8_t *ie)
{
    struct ieee80211_chloadrsp *chloadrsp=NULL;
    struct ieee80211_measrsp_ie *measrsp=NULL;
    struct ieee80211_rrmreq_info *params=NULL;
#if UMAC_SUPPORT_RRM_DEBUG
    wlan_if_t vap = rrm->rrm_vap;
#endif

    RRM_FUNCTION_ENTER;

    params = (struct ieee80211_rrmreq_info *)(rrm->rrmcb);
    measrsp = (struct ieee80211_measrsp_ie *)ie;
    measrsp->id = IEEE80211_ELEMID_MEASREP;
    measrsp->token=params->rep_dialogtoken;
    measrsp->rspmode = 0x00; /* Need to validate */
    measrsp->rsptype = IEEE80211_MEASRSP_CHANNEL_LOAD_REPORT;
    chloadrsp = (struct ieee80211_chloadrsp *)&measrsp->rsp[0];
    chloadrsp->regclass = params->regclass;
    chloadrsp->chnum =params->chnum;
    OS_GET_RANDOM_BYTES(&chloadrsp->tsf[0], sizeof(chloadrsp->tsf));
    chloadrsp->mduration= params->duration;
    chloadrsp->chload = rrm->rrm_chan_load[chloadrsp->chnum];
    ie = (u_int8_t *)(&chloadrsp->rsp[0]);
    measrsp->len = ie -(u_int8_t *)(&measrsp->token);

    RRM_FUNCTION_EXIT;
    return ie;
}

/**
 * @brief CCA response
 *
 * @param rrm
 * @param ie
 *
 * @return
 */
u_int8_t *ieee80211_rrm_send_cca_resp(ieee80211_rrm_t rrm,u_int8_t *ie)
{
    struct ieee80211_ccarsp *rsp=NULL;
    struct ieee80211_measrsp_ie *measrsp=NULL;
    struct ieee80211_rrmreq_info *params=NULL;
    u_int8_t tsf[8];
#if UMAC_SUPPORT_RRM_DEBUG
    wlan_if_t vap = rrm->rrm_vap;
#endif

    RRM_FUNCTION_ENTER;
    params = (struct ieee80211_rrmreq_info *)(rrm->rrmcb);
    measrsp = (struct ieee80211_measrsp_ie *)ie;
    measrsp->id = IEEE80211_ELEMID_MEASREP;
    measrsp->token=params->rep_dialogtoken;
    measrsp->rspmode = 0x00; /* Need to validate */
    measrsp->rsptype = IEEE80211_MEASRSP_CCA_REPORT;
    rsp = (struct ieee80211_ccarsp *)(&measrsp->rsp[0]);
    rsp->mduration = params->duration;
    rsp->chnum = params->chnum;
    rsp->cca_busy = 0x10; /*send dummy for now */
    OS_GET_RANDOM_BYTES(tsf,sizeof(tsf));
    rsp->tsf =   ( ((u_int64_t)tsf[0])     | ((u_int64_t)tsf[1]<<8)
                 | ((u_int64_t)tsf[2]<<16) | ((u_int64_t)tsf[3]<<24)
                 | ((u_int64_t)tsf[4]<<32) | ((u_int64_t)tsf[5]<<40)
                 | ((u_int64_t)tsf[6]<<48) | ((u_int64_t)tsf[7]<<56) );

    ie = &rsp->rsp[0];
    measrsp->len = (ie - &measrsp->token);

    RRM_FUNCTION_EXIT;
    return ie;
}


/**
 * @brief RPI histogrm response
 *
 * @param rrm
 * @param ie
 *
 * @return
 */
u_int8_t *ieee80211_rrm_send_rpihist_resp(ieee80211_rrm_t rrm,u_int8_t *ie)
{
    struct ieee80211_rpihistrsp *rsp=NULL;
    struct ieee80211_measrsp_ie *measrsp=NULL;
    struct ieee80211_rrmreq_info *params=NULL;
    u_int8_t tsf[8];
#if UMAC_SUPPORT_RRM_DEBUG
    wlan_if_t vap = rrm->rrm_vap;
#endif

    RRM_FUNCTION_ENTER;
    params = (struct ieee80211_rrmreq_info *)(rrm->rrmcb);
    measrsp = (struct ieee80211_measrsp_ie *)ie;
    measrsp->id = IEEE80211_ELEMID_MEASREP;
    measrsp->token=params->rep_dialogtoken;
    measrsp->rspmode = 0x00; /* Need to validate */
    measrsp->rsptype = IEEE80211_MEASRSP_RPI_HISTOGRAM_REPORT;
    rsp = (struct ieee80211_rpihistrsp *)(&measrsp->rsp[0]);
    rsp->mduration = params->duration;
    rsp->chnum = params->chnum;
    OS_GET_RANDOM_BYTES(rsp->rpi,sizeof(rsp->rpi));
    OS_GET_RANDOM_BYTES(tsf,sizeof(tsf));
    rsp->tsf =   ( ((u_int64_t)tsf[0])     | ((u_int64_t)tsf[1]<<8)
                 | ((u_int64_t)tsf[2]<<16) | ((u_int64_t)tsf[3]<<24)
                 | ((u_int64_t)tsf[4]<<32) | ((u_int64_t)tsf[5]<<40)
                 | ((u_int64_t)tsf[6]<<48) | ((u_int64_t)tsf[7]<<56) );

    ie = &rsp->rsp[0];
    measrsp->len = (ie - &measrsp->token);

    RRM_FUNCTION_EXIT;
    return ie;
}


/**
 * @brief noise histogrm response
 *
 * @param rrm
 * @param ie
 *
 * @return
 */
u_int8_t *ieee80211_rrm_send_nhist_resp(ieee80211_rrm_t rrm,u_int8_t *ie)
{
    struct ieee80211_nhistrsp *rsp=NULL;
    struct ieee80211_measrsp_ie *measrsp=NULL;
    struct ieee80211_rrmreq_info *params=NULL;
    unsigned long tsf;
#if UMAC_SUPPORT_RRM_DEBUG
    wlan_if_t vap = rrm->rrm_vap;
#endif

    RRM_FUNCTION_ENTER;

    params = (struct ieee80211_rrmreq_info *)(rrm->rrmcb);
    measrsp = (struct ieee80211_measrsp_ie *)ie;
    measrsp->id = IEEE80211_ELEMID_MEASREP;
    measrsp->token=params->rep_dialogtoken;
    measrsp->rspmode = 0x00; /* Need to validate */
    measrsp->rsptype = IEEE80211_MEASRSP_NOISE_HISTOGRAM_REPORT;
    rsp = (struct ieee80211_nhistrsp *)(&measrsp->rsp[0]);
    rsp->mduration = params->duration;
    rsp->regclass = params->regclass;
    rsp->chnum = params->chnum;
    rsp->anpi = (u_int8_t)rrm->rrm_noisefloor[rsp->chnum];
    OS_GET_RANDOM_BYTES(&rsp->antid,sizeof(rsp->antid));
    OS_GET_RANDOM_BYTES(rsp->ipi,sizeof(rsp->ipi));
    tsf = OS_GET_TIMESTAMP();
    OS_MEMCPY(rsp->tsf,&tsf,4);
    ie = &rsp->rsp[0];
    measrsp->len = (ie - &measrsp->token);

    RRM_FUNCTION_EXIT;
    return ie;
}

/**
 * @brief location configuration response
 *
 * @param rrm
 * @param ie
 *
 * @return ie
 */
u_int8_t *ieee80211_rrm_send_lci_resp(ieee80211_rrm_t rrm,u_int8_t *ie)
{
    struct ieee80211_lcirsp *lcirpt;
    struct ieee80211_measrsp_ie *measrsp;
    struct ieee80211_rrmreq_info *params;
#if UMAC_SUPPORT_RRM_DEBUG
    wlan_if_t vap = rrm->rrm_vap;
#endif

    RRM_FUNCTION_ENTER;

    params = (struct ieee80211_rrmreq_info *)(rrm->rrmcb);
    measrsp = (struct ieee80211_measrsp_ie *)ie;

    measrsp->id = IEEE80211_ELEMID_MEASREP;
    measrsp->token = params->rep_dialogtoken;
    measrsp->rspmode = 0x00; /* Need to validate */
    measrsp->rsptype = IEEE80211_MEASRSP_LCI_REPORT;

    lcirpt = (struct ieee80211_lcirsp *)(&measrsp->rsp[0]);

    OS_GET_RANDOM_BYTES(&lcirpt->lci_data, sizeof(lcirpt->lci_data));

    ie = &lcirpt->rsp[0];

    measrsp->len = (ie - &measrsp->token);

    RRM_FUNCTION_EXIT;
    return ie;
}

/**
 * @brief sta statistics response
 *
 * @param rrm
 * @param ie
 *
 * @return
 */
u_int8_t *ieee80211_rrm_send_stastats_resp(ieee80211_rrm_t rrm,u_int8_t *ie)
{
    struct ieee80211_stastatsrsp *rsp;
    struct ieee80211_measrsp_ie *measrsp;
    struct ieee80211_rrmreq_info *params;
    unsigned long random = 0;
#if UMAC_SUPPORT_RRM_DEBUG
    wlan_if_t vap = rrm->rrm_vap;
#endif
    RRM_FUNCTION_ENTER;

    params = (struct ieee80211_rrmreq_info *)(rrm->rrmcb);
    measrsp = (struct ieee80211_measrsp_ie *)ie;
    measrsp->id = IEEE80211_ELEMID_MEASREP;
    measrsp->token=params->rep_dialogtoken;
    measrsp->rspmode = 0x00; /* Need to validate */
    measrsp->rsptype = IEEE80211_MEASRSP_STA_STATS_REPORT;
    rsp = (struct ieee80211_stastatsrsp *)(&measrsp->rsp[0]);
    rsp->m_intvl = params->duration;
    rsp->gid = params->gid;
    switch(rsp->gid)
    {
        case IEEE80211_STASTATS_GID0:
            OS_GET_RANDOM_BYTES(&random,sizeof(random));
            rsp->stats.gid0.txfragcnt =random % 100;
            rsp->stats.gid0.mcastrxfrmcnt = random % 23;
            rsp->stats.gid0.failcnt   = random %89;
            rsp->stats.gid0.rxfragcnt = random % 87;
            rsp->stats.gid0.fcserrcnt = random %13;
            rsp->stats.gid0.txfrmcnt  =  random % 32103;
            ie = &(rsp->rsp[0]);
            break;
        case IEEE80211_STASTATS_GID1:
            OS_GET_RANDOM_BYTES(&random,sizeof(random));
            rsp->stats.gid1.rty =random % 100;
            rsp->stats.gid1.multirty = random % 23;
            rsp->stats.gid1.frmdup = random %33;
            rsp->stats.gid1.rtsuccess = random % 87;
            rsp->stats.gid1.rtsfail = random %13;
            rsp->stats.gid1.ackfail =  random % 29;
            ie = &(rsp->rsp[0]);
            break;
        case IEEE80211_STASTATS_GID10: /*Average access delay */
            OS_GET_RANDOM_BYTES(&random,sizeof(random));
            if(random)
            {
                rsp->stats.gid10.ap_avg_delay  = random % 88;
                rsp->stats.gid10.be_avg_delay  = random % 99;
                rsp->stats.gid10.bk_avg_delay  = random % 77;
            }
            OS_GET_RANDOM_BYTES(&random,sizeof(random)) ;
            if(random)
            {
                rsp->stats.gid10.vi_avg_delay  = random % 47;
                rsp->stats.gid10.vo_avg_delay  = random % 34;
                rsp->stats.gid10.st_cnt = random % 128;
                rsp->stats.gid10.ch_util =random % 60;
            }
            ie = &(rsp->rsp[0]);
            break;
        default:
            break;
    }
    measrsp->len = (ie - &measrsp->token);

    RRM_FUNCTION_EXIT;
    return ie ;
}

/**
 * @brief
 *
 * @param rrm
 */

void ieee80211_send_report(ieee80211_rrm_t rrm)
{
    struct ieee80211vap *vap =NULL;
    struct ieee80211_node *ni = NULL;
    struct ieee80211_measrsp_ie *measrsp=NULL;
    wbuf_t wbuf = NULL;
    u_int8_t *frm = NULL,*fptr = NULL;
    struct ieee80211_action_rm_rsp *rm_rsp=NULL;
    struct ieee80211_rrmreq_info *params=NULL;
    enum ieee80211_opmode opmode;

    params = (struct ieee80211_rrmreq_info *)(rrm->rrmcb);
    vap = rrm->rrm_vap;
    opmode = wlan_vap_get_opmode(vap);

    RRM_FUNCTION_ENTER;

    qdf_spin_lock_bh( &rrm->rrm_lock );
    ni = ieee80211_vap_find_node(vap, rrm->rrm_macaddr);
    if( ni == NULL )
    {
        ieee80211_rrm_free_report(rrm);
        qdf_spin_unlock_bh( &rrm->rrm_lock );
        return;
    }

    wbuf = ieee80211_getmgtframe(ni,IEEE80211_FC0_SUBTYPE_ACTION, &frm,0);
    if(wbuf == NULL)
    {
        ieee80211_rrm_free_report(rrm);
        ieee80211_free_node(ni);
        qdf_spin_unlock_bh( &rrm->rrm_lock );
        return;
    }

    rm_rsp = (struct ieee80211_action_rm_rsp *)(frm);
    rm_rsp->header.ia_category = IEEE80211_ACTION_CAT_RM;
    rm_rsp->header.ia_action  = IEEE80211_ACTION_RM_RESP;
    rm_rsp->dialogtoken = params->rm_dialogtoken;

    switch(rrm->pending_report_type)
    {
        case IEEE80211_MEASREQ_BR_TYPE:
            fptr = ieee80211_rrm_send_bcnreq_resp(rrm,&rm_rsp->rsp_ies[0]);
            break;
        case IEEE80211_MEASREQ_CHANNEL_LOAD_REQ:
            fptr = ieee80211_rrm_send_chload_resp(rrm,&rm_rsp->rsp_ies[0]);
            break;
        case IEEE80211_MEASREQ_STA_STATS_REQ:
            fptr = ieee80211_rrm_send_stastats_resp(rrm,&rm_rsp->rsp_ies[0]);
            break;
        case IEEE80211_MEASREQ_CCA_REQ:
            fptr = ieee80211_rrm_send_cca_resp(rrm,&rm_rsp->rsp_ies[0]);
            break;
        case IEEE80211_MEASREQ_RPI_HISTOGRAM_REQ:
            fptr = ieee80211_rrm_send_rpihist_resp(rrm,&rm_rsp->rsp_ies[0]);
            break;
        case IEEE80211_MEASREQ_NOISE_HISTOGRAM_REQ:
            fptr = ieee80211_rrm_send_nhist_resp(rrm,&rm_rsp->rsp_ies[0]);
            break;
        case IEEE80211_MEASREQ_LCI_REQ:
            fptr = ieee80211_rrm_send_lci_resp(rrm, &rm_rsp->rsp_ies[0]);
            break;
        case IEEE80211_MEASREQ_OTHER:
            measrsp = (struct ieee80211_measrsp_ie *)(&rm_rsp->rsp_ies[0]);
            measrsp->id = IEEE80211_ELEMID_MEASREP;
            measrsp->token=params->rep_dialogtoken;
            measrsp->rspmode |= params->reject_mode;
            measrsp->rsptype = params->reject_type;
            fptr = (u_int8_t *)(&measrsp->rsp[0]);
            measrsp->len = fptr -(u_int8_t *)(&measrsp->token);
            break;
        default:
            ieee80211_rrm_free_report(rrm);
            ieee80211_free_node(ni);
            frm += sizeof(struct ieee80211_action_rm_rsp);
            wbuf_set_pktlen(wbuf, (frm -(u_int8_t*)(wbuf_header(wbuf))));
            wbuf_complete(wbuf);
            qdf_spin_unlock_bh( &rrm->rrm_lock );
            return;
    }

    if(IEEE80211_ADDR_EQ(ni->ni_macaddr, vap->iv_myaddr)) { /* self report */
        wbuf_set_pktlen(wbuf, (fptr -(u_int8_t*)(wbuf_header(wbuf))));
        ieee80211_recv_radio_measurement_rsp(vap, ni, (u_int8_t *)rm_rsp, (fptr-frm) );
        if ( wbuf != NULL) {
            wbuf_complete(wbuf);
        }
    } else {
        wbuf_set_pktlen(wbuf, (fptr -(u_int8_t*)(wbuf_header(wbuf))));

#ifdef WLAN_CONV_CRYPTO_SUPPORTED
        if (wlan_crypto_is_pmf_enabled(vap->vdev_obj, ni->peer_obj)) {
#else
        if (ieee80211_is_pmf_enabled(vap, ni) && ni->ni_ucastkey.wk_valid) {
#endif
            /* MFP is enabled, so we need to set Privacy bit */
            struct ieee80211_frame *wh = (struct ieee80211_frame*)wbuf_header(wbuf);
            wh->i_fc[1] |= IEEE80211_FC1_WEP;
        }

        ieee80211_send_mgmt(vap, ni, wbuf, false);
    }
    rrm->rrm_in_progress = 0; /* making module available */
    ieee80211_rrm_free_report(rrm);
    ieee80211_free_node(ni);
    qdf_spin_unlock_bh( &rrm->rrm_lock );

    RRM_FUNCTION_EXIT;
    return;
}

/**
 * @brief
 *
 * @param vap
 * @param ni
 * @param rm_req
 * @param frm_len
 *
 * @return
 */
int ieee80211_recv_radio_measurement_req(wlan_if_t vap, wlan_node_t ni,
    struct ieee80211_action_rm_req *rm_req , int  frm_len)
{
    struct ieee80211_measreq_ie *req;
    ieee80211_rrm_t rrm = vap->rrm;

    RRM_FUNCTION_ENTER;

    req = (struct ieee80211_measreq_ie *)(&(rm_req->req_ies[0]));
    IEEE80211_ADDR_COPY(rrm->rrm_macaddr, ni->ni_macaddr);

    if(rrm->rrm_in_progress) {
        rrm_send_reject(rrm,rm_req);
        return EOK;
    }
    else
        rrm->rrm_in_progress = 1;

    switch(req->reqtype)
    {
        case IEEE80211_MEASREQ_BASIC_REQ:
            break;
        case IEEE80211_MEASREQ_CCA_REQ:
            ieee80211_rrm_recv_cca_req(vap,ni,req,rm_req->dialogtoken);
            break;
        case IEEE80211_MEASREQ_RPI_HISTOGRAM_REQ:
            ieee80211_rrm_recv_rpihist_req(vap,ni,req,rm_req->dialogtoken);
            break;
        case IEEE80211_MEASREQ_TSMREQ_TYPE:
        case IEEE80211_MEASREQ_FRAME_REQ:
            break;
        case IEEE80211_MEASREQ_CHANNEL_LOAD_REQ:
            ieee80211_rrm_recv_chload_req(vap,ni,req,rm_req->dialogtoken);
            break;
        case IEEE80211_MEASREQ_NOISE_HISTOGRAM_REQ:
            ieee80211_rrm_recv_nhist_req(vap,ni,req,rm_req->dialogtoken);
            break;
        case IEEE80211_MEASREQ_BR_TYPE:
            ieee80211_rrm_recv_beacon_req(vap,ni,req,rm_req->dialogtoken);
            break;
        case IEEE80211_MEASREQ_STA_STATS_REQ:
            ieee80211_rrm_recv_stastats_req(vap,ni,req,rm_req->dialogtoken);
            break;
        case IEEE80211_MEASREQ_LCI_REQ:
            ieee80211_rrm_recv_lci_req(vap,ni,req,rm_req->dialogtoken);
            break;
        default:
            break;
    }

    RRM_FUNCTION_EXIT;
    return EOK;
}
#endif
