/*
* Copyright (c) 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 *  Copyright (c) 2009 Atheros Communications Inc.  All rights reserved.
 */

/*
 *  WNM message handlers for AP.
 */
#include <ieee80211_var.h>
#include <ieee80211_rrm.h>
#include <ieee80211_wnm.h>
#include <ieee80211_ioctl.h>
#include "ieee80211_wnm_priv.h"
#include <qdf_mem.h>
#if UMAC_SUPPORT_WNM

static void
ieee80211_fill_tclass_subelements(void *head,
                                  ieee80211_tclas_processing *tclasprocess,
                                  u_int8_t **frm)
{

    struct tclas_element *tclas_element;
    u_int16_t port;
    int flen = 0;
    TAILQ_HEAD(, tclas_element) *tclas_head;

    tclas_head = head;

    TAILQ_FOREACH(tclas_element, tclas_head, tclas_next) {
        *(*frm)++ = tclas_element->elemid;
        *(*frm)++ = tclas_element->len;
        *(*frm)++ = tclas_element->up;
        *(*frm)++ = tclas_element->type;
        *(*frm)++ = tclas_element->mask;

        if (tclas_element->type == IEEE80211_WNM_TCLAS_CLASSIFIER_TYPE3) {
            *(u_int16_t *)(*frm) = htole16(tclas_element->tclas.type3.filter_offset);
            *frm += 2;
            flen = (tclas_element->len - 5) / 2;
            OS_MEMCPY(*frm, tclas_element->tclas.type3.filter_value, flen);
            *frm += flen;
            OS_MEMCPY(*frm, tclas_element->tclas.type3.filter_mask, flen);
            *frm += flen;
        } else if (tclas_element->type == IEEE80211_WNM_TCLAS_CLASSIFIER_TYPE1 ||
                   tclas_element->type == IEEE80211_WNM_TCLAS_CLASSIFIER_TYPE4)
        {
            if (tclas_element->tclas.type14.type14_v4.version ==
                    IEEE80211_WNM_TCLAS_CLAS14_VERSION_4) {
                *(*frm)++ = tclas_element->tclas.type14.type14_v4.version;
                OS_MEMCPY(*frm, &tclas_element->tclas.type14.
                        type14_v4.src_ip, IEEE80211_IPV4_LEN);
                *frm += IEEE80211_IPV4_LEN;
                OS_MEMCPY(*frm, &tclas_element->tclas.type14.
                        type14_v4.dst_ip, IEEE80211_IPV4_LEN);
                *frm += IEEE80211_IPV4_LEN;
                port = tclas_element->tclas.type14.type14_v4.src_port;
                port = htobe16(port);
                OS_MEMCPY(*frm, &port, 2);
                *frm += 2;
                port = tclas_element->tclas.type14.type14_v4.src_port;
                port = htobe16(port);
                OS_MEMCPY(*frm, &port, 2);
                *frm += 2;
                *(*frm)++ = tclas_element->tclas.type14.type14_v4.dscp;
                *(*frm)++ = tclas_element->tclas.type14.type14_v4.protocol;
                *(*frm)++ = tclas_element->tclas.type14.type14_v4.reserved;
            } else if(tclas_element->tclas.type14.type14_v6.version ==
                    IEEE80211_WNM_TCLAS_CLAS14_VERSION_6) {
                *(*frm)++ = tclas_element->tclas.type14.type14_v6.version;
                OS_MEMCPY(*frm, &tclas_element->tclas.type14.
                        type14_v6.src_ip, IEEE80211_IPV6_LEN);
                *frm += IEEE80211_IPV6_LEN;
                OS_MEMCPY(*frm, &tclas_element->tclas.type14.
                        type14_v6.dst_ip, IEEE80211_IPV6_LEN);
                *frm += IEEE80211_IPV6_LEN;
                port = tclas_element->tclas.type14.type14_v6.src_port;
                port = htobe16(port);
                OS_MEMCPY(*frm, &port, 2);
                *frm += 2;
                port = tclas_element->tclas.type14.type14_v6.src_port;
                port = htobe16(port);
                OS_MEMCPY(*frm, &port, 2);
                *frm += 2;
                if (tclas_element->type == IEEE80211_WNM_TCLAS_CLASSIFIER_TYPE4) {
                    *(*frm)++ = tclas_element->tclas.type14.type14_v6.type4_dscp;
                    *(*frm)++ = tclas_element->tclas.type14.type14_v6.type4_next_header;
                }
                OS_MEMCPY(*frm, &tclas_element->tclas.type14.
                        type14_v6.flow_label, 3);
                *frm += 3;
            }
        }
    }

    /* IEEE80211_ELEMID_TCLAS_PROCESS will be set by wlan_wnm_set_tfs */
    if (tclasprocess->elem_id == IEEE80211_ELEMID_TCLAS_PROCESS) {
        *(*frm)++ = tclasprocess->elem_id;
        *(*frm)++ = tclasprocess->length;
        *(*frm)++ = tclasprocess->tclas_process;
    }
}

int ieee80211_send_fms_req(wlan_if_t vap, wlan_node_t ni)
{
    wbuf_t wbuf = NULL;
    u_int8_t *frm = NULL;
    ieee80211_fms_request_t *fms_req;
    ieee80211_fmsreq_subele_t *fms_subele;
    u_int16_t rate;

    wbuf = ieee80211_getmgtframe(ni, IEEE80211_FC0_SUBTYPE_ACTION, &frm, 0);
    if (wbuf == NULL) {
        return -ENOMEM;
    }

    *frm++ = IEEE80211_ACTION_CAT_WNM;
    *frm++ = IEEE80211_ACTION_FMS_REQ;

    ni->ni_wnm->fmsreq->dialog_token++;
    ni->ni_wnm->fmsreq->dialog_token %= 255;
    *frm++ = ni->ni_wnm->fmsreq->dialog_token;

    TAILQ_FOREACH(fms_req, &ni->ni_wnm->fmsreq->fmsreq_head, fmsreq_next) {

        *frm++ =  IEEE80211_ELEMID_FMS_REQUEST;
        *frm++ = fms_req->len;
        *frm++ = fms_req->fms_token;

        TAILQ_FOREACH(fms_subele, &fms_req->fmssubele_head, fmssubele_next) {
            *frm++ = fms_subele->elem_id;
            *frm++ = fms_subele->len;
            *frm++ = fms_subele->del_itvl;
            *frm++ = fms_subele->max_del_itvl;
            *frm++ = fms_subele->rate_id.mask;
            *frm++ = fms_subele->rate_id.mcs_idx;
            rate = htobe16(fms_subele->rate_id.rate);
            OS_MEMCPY(frm, &rate, 2);
            frm += 2;

            ieee80211_fill_tclass_subelements(&fms_subele->tclas_head, &fms_subele->tclasprocess, &frm);
        }
    }

    wbuf_set_pktlen(wbuf, (frm - (u_int8_t*)wbuf_header(wbuf)));

    return ieee80211_send_mgmt(vap, ni, wbuf, false);
}

int ieee80211_recv_fms_rsp(wlan_if_t vap, wlan_node_t ni,
                                u_int8_t *frm, int frm_len)
{
    int fms_length;
    TAILQ_HEAD(, ieee80211_fms_response_s) fmsrsp_head;
    ieee80211_fms_status_subelement_t *status_subele;
    ieee80211_fms_response_t *fms_rsp;
    struct ieee80211_wnm_fmsresp *fmsrsp_msg;
    wnm_netlink_event_t *event = NULL;
    u_int8_t *sfrm;
    u_int8_t resp_count = 0, subelem_count=0;
    u_int32_t tresplen = 0;
    int tclass_len=0;
    struct fmsresp_element  *fmsrsp_ie=NULL;
    struct fmsresp_tclas_subele_status *tclas_status_msg=NULL;
    struct fmsresp_fms_subele_status   *fmsresp_subele_status=NULL;
    int rv = 0;

    TAILQ_INIT(&fmsrsp_head);
    sfrm = frm;
    frm = frm + 2;
    frm++;

    event = (wnm_netlink_event_t *)qdf_mem_malloc(sizeof(*event));
    if (event == NULL) {
        printk("%s[%d]: Memory allocation failed\n", __func__, __LINE__);
        return -ENOMEM;
    }

    fmsrsp_msg = qdf_mem_malloc(sizeof(*fmsrsp_msg));
    if (fmsrsp_msg == NULL) {
        printk("%s[%d]: Memory allocation failed\n", __func__, __LINE__);
        qdf_mem_free(event);
        return -ENOMEM;
    }

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, "*** Entering the function %s\n", __func__);
    while((frm- sfrm) < frm_len) {

        fmsrsp_ie = NULL;

        if(*frm != IEEE80211_ELEMID_FMS_RESPONSE) {
            break;
        }
        fms_rsp = (ieee80211_fms_response_t *)
                  OS_MALLOC(vap->iv_ic->ic_osdev,
                      (sizeof(ieee80211_fms_response_t)),0);
        if (fms_rsp == NULL) {
            ieee80211_wnm_free_fmsrsp(&fmsrsp_head);
            printk("%s : %d OS_MALLOC failed!\n", __func__, __LINE__);
            rv = -ENOMEM;
            goto out;
        }
        fms_rsp->elemid = *frm++;
        fms_rsp->len = *frm++;
        fms_rsp->fms_token = *frm++;
        TAILQ_INIT(&fms_rsp->status_subele_head);
        fms_length = fms_rsp->len;

        if (resp_count < FMS_MAX_RESPONSE) {
            fmsrsp_ie = &fmsrsp_msg->fms_resp[resp_count];
            fmsrsp_ie->fms_token = fms_rsp->fms_token;
        }

        while(fms_length > 0) {

            status_subele = (ieee80211_fms_status_subelement_t *)
                      OS_MALLOC(vap->iv_ic->ic_osdev,
                            (sizeof(ieee80211_fms_status_subelement_t)),0);

            if (status_subele == NULL) {
                OS_FREE(fms_rsp);
                ieee80211_wnm_free_fmsrsp(&fmsrsp_head);
                rv = -ENOMEM;
                goto out;
            }
            status_subele->subelementid = *frm;

            if (fmsrsp_ie != NULL){
                fmsrsp_ie->subelement_type = status_subele->subelementid;
            }

            if (IEEE80211_FMS_STATUS_SUBELE == status_subele->subelementid)
            {
                ieee80211_fms_status_subelements_t *subele_status = (ieee80211_fms_status_subelements_t *)
                                                                    status_subele->subele_status;
                subele_status = (ieee80211_fms_status_subelements_t *)
                                 OS_MALLOC(vap->iv_ic->ic_osdev,
                                (sizeof(ieee80211_fms_status_subelements_t)),0);

                if (subele_status == NULL) {
                    OS_FREE(fms_rsp);
                    OS_FREE(status_subele);
                    ieee80211_wnm_free_fmsrsp(&fmsrsp_head);
                    printk("%s : %d OS_MALLOC failed!\n", __func__, __LINE__);
                    rv = -ENOMEM;
                    goto out;
                }

                subele_status->subelement_id = *frm++;
                subele_status->len = *frm++;
                subele_status->element_status = *frm++;
                subele_status->del_itvl = *frm++;
                subele_status->max_del_itvl = *frm++;
                subele_status->fmsid = *frm++;
                subele_status->fms_counter = *frm++;
                OS_MEMCPY(&subele_status->rate_id, frm, sizeof(subele_status->rate_id));
                frm += sizeof(subele_status->rate_id);
                OS_MEMCPY(&subele_status->mcast_addr, frm, sizeof(subele_status->mcast_addr));
                frm += sizeof(subele_status->mcast_addr);
                fms_length = fms_length - 18;

                if (subelem_count < FMS_MAX_SUBELEMENTS && fmsrsp_ie != NULL) {
                    fmsresp_subele_status = &fmsrsp_ie->status.fms_subele_status[subelem_count];
                    fmsresp_subele_status->status = subele_status->element_status;
                    fmsresp_subele_status->del_itvl =  subele_status->del_itvl;
                    fmsresp_subele_status->max_del_itvl =  subele_status->max_del_itvl;
                    fmsresp_subele_status->fmsid =  subele_status->fmsid;
                    fmsresp_subele_status->fms_counter =  subele_status->fms_counter;
                    OS_MEMCPY(&fmsresp_subele_status->rate_id, &subele_status->rate_id, sizeof(subele_status->rate_id));
                    OS_MEMCPY(&fmsresp_subele_status->mcast_addr, &subele_status->mcast_addr, sizeof(subele_status->mcast_addr));
                }
            }
            else if (IEEE80211_TCLASS_STATUS_SUBELE == status_subele->subelementid) {
                ieee80211_tclass_subelement_status_t *tclas_subele_status = (ieee80211_tclass_subelement_status_t *)
                                                                     status_subele->subele_status;

                tclas_subele_status = (ieee80211_tclass_subelement_status_t *)
                                 OS_MALLOC(vap->iv_ic->ic_osdev,
                                (sizeof(ieee80211_tclass_subelement_status_t)),0);

                if (tclas_subele_status == NULL) {
                    OS_FREE(fms_rsp);
                    OS_FREE(status_subele);
                    ieee80211_wnm_free_fmsrsp(&fmsrsp_head);
                    printk("%s : %d OS_MALLOC failed!\n", __func__, __LINE__);
                    rv = -ENOMEM;
                    goto out;
                }
                tclas_subele_status->subelementid = *frm++;
                tclas_subele_status->len = *frm++;
                tclas_subele_status->fmsid = *frm++;
                tclass_len = tclas_subele_status->len - 1;

                if (subelem_count < FMS_MAX_TCLAS_ELEMENTS && fmsrsp_ie != NULL) {
                    tclas_status_msg = &fmsrsp_ie->status.tclas_subele_status[subelem_count];
                    tclas_status_msg->fmsid = tclas_subele_status->fmsid;
                }
                if (tclass_len > 0) {
                    rv = ieee80211_tclass_element_parse(vap,
                            &tclas_subele_status->tclas_head,
                            &tclas_subele_status->tclasprocess,
                            tclas_status_msg, &frm, tclass_len);
                    if (rv != 0) {
                        OS_FREE(tclas_subele_status);
                        OS_FREE(status_subele);
                        OS_FREE(&fms_rsp);
                        ieee80211_wnm_free_fmsrsp(&fmsrsp_head);
                        printk("%s:%d ieee80211_tclass_element_parse failed %d\n",
                                __func__, __LINE__, rv);
                        goto out;
                    }
                }
                fms_length = fms_length - (tclas_subele_status->len+3);
            } else {
                OS_FREE(status_subele);
                OS_FREE(&fms_rsp);
                ieee80211_wnm_free_fmsrsp(&fmsrsp_head);
                printk("%s:%d unknown elementid %d\n", __func__, __LINE__,
                        status_subele->subelementid);
                rv = -EINVAL;
                goto out;
            }
            subelem_count++;
            TAILQ_INSERT_TAIL(&fms_rsp->status_subele_head, status_subele, status_subele_next);
        }
        if(fmsrsp_ie != NULL) {
             fmsrsp_ie->num_subelements = subelem_count;
        }
        resp_count++;
		TAILQ_INSERT_TAIL(&fmsrsp_head, fms_rsp, fmsrsp_next);
    }
    fmsrsp_msg->num_fmsresp = resp_count;
#if 0
    tresplen = (fmsrsp_msg.num_fmsresp * 2) + 1;
#else
    tresplen = sizeof(*fmsrsp_msg);
#endif
    event->type = 0x15;
    OS_MEMCPY(event->mac, ni->ni_macaddr, MAC_ADDR_LEN);
    OS_MEMCPY(event->event_data, fmsrsp_msg, tresplen);
    event->datalen = tresplen;
    wnm_netlink_send(vap, event);
    TAILQ_CONCAT(&ni->ni_wnm->fmsrsp->fmsrsp_head,
                        &fmsrsp_head, fmsrsp_next);
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, "*** Exiting the function %s\n", __func__);

 out:
    qdf_mem_free(event);
    event = NULL;
    qdf_mem_free(fmsrsp_msg);
    return 0;
}


u_int8_t *
ieee80211_add_tfsreq_ie(wlan_if_t vap, wlan_node_t ni, u_int8_t *frm)
{
    struct ieee80211_tfs_request *tfs_req;
    struct ieee80211_tfs_subelement_req *tfs_subele;

    TAILQ_FOREACH(tfs_req, &ni->ni_wnm->tfsreq->tfs_req_head, tfs_req_next) {

        tfs_req->tfs_elemid = IEEE80211_ELEMID_TFS_REQUEST;
        *frm++ = tfs_req->tfs_elemid;
        *frm++ = tfs_req->length;
        *frm++ = tfs_req->tfs_id;
        *frm++ = tfs_req->tfs_action_code;

        TAILQ_FOREACH(tfs_subele, &tfs_req->tfs_req_sub, tsub_next) {
            *frm++ = tfs_subele->elem_id;
            *frm++ = tfs_subele->length;
            ieee80211_fill_tclass_subelements(&tfs_subele->tclas_head,
                    &tfs_subele->tclasprocess, &frm);
        }
    }

    return frm;
}


int ieee80211_send_tfs_req(wlan_if_t vap, wlan_node_t ni)
{
    wbuf_t wbuf;
    u_int8_t *frm;

    wbuf = ieee80211_getmgtframe(ni, IEEE80211_FC0_SUBTYPE_ACTION, &frm, 0);
    if (wbuf == NULL) {
        return -ENOMEM;
    }

    *frm++ = IEEE80211_ACTION_CAT_WNM;
    *frm++ = IEEE80211_ACTION_TFS_REQ;

    /* TFS request dialog token is from 1 to 255 */
    if (++ni->ni_wnm->tfsreq->dialogtoken > 255)
        ni->ni_wnm->tfsreq->dialogtoken = 1;

    *frm++ = ni->ni_wnm->tfsreq->dialogtoken;
    frm = ieee80211_add_tfsreq_ie(vap, ni, frm);
    wbuf_set_pktlen(wbuf, (frm - (u_int8_t *)wbuf_header(wbuf)));

    return ieee80211_send_mgmt(vap, ni, wbuf, false);
}

int ieee80211_add_tfs_resp(wlan_if_t vap, wlan_node_t ni,
                                u_int8_t *frm, int frm_len)
{
    int tfs_length;
    TAILQ_HEAD(, ieee80211_tfs_response) tfs_rsp_head;
    struct ieee80211_tfs_subelement_rsp *tfs_subrsp;
    struct ieee80211_tfs_response *tfs_rsp;
    u_int8_t *sfrm = frm;
    struct ieee80211_wnm_tfsresp *tfsrsp;
    wnm_netlink_event_t *event = NULL;
    u_int32_t tresplen = 0;
    int resp_count = 0;
    int rv = 0;
    TAILQ_INIT(&tfs_rsp_head);

    event = qdf_mem_malloc(sizeof(*event));
    if (event == NULL) {
        printk("%s[%d]: Memory allocation failed\n", __func__, __LINE__);
        return -ENOMEM;
    }

    tfsrsp = qdf_mem_malloc(sizeof(*tfsrsp));
    if (tfsrsp == NULL) {
        printk("%s[%d]: Memory allocation failed\n", __func__, __LINE__);
        qdf_mem_free(event);
        return -ENOMEM;
    }

    while((frm- sfrm) <= frm_len) {
        if(*frm != IEEE80211_ELEMID_TFS_RESPONSE) {
            break;
        }
        tfs_rsp = (struct ieee80211_tfs_response *)
                  OS_MALLOC(vap->iv_ic->ic_osdev,
                      (sizeof(struct ieee80211_tfs_response)),0);
        if (tfs_rsp == NULL) {
            wlan_wnm_free_tfsrsp(&tfs_rsp);
            rv = -ENOMEM;
            goto out;
        }
        tfs_rsp->tfs_elemid = *frm++;
        tfs_rsp->length = *frm++;
        TAILQ_INIT(&tfs_rsp->tfs_rsp_sub);
        tfs_length = tfs_rsp->length;
        while(tfs_length > 0) {
            tfs_subrsp = (struct ieee80211_tfs_subelement_rsp *)
                      OS_MALLOC(vap->iv_ic->ic_osdev,
                            (sizeof(struct ieee80211_tfs_subelement_rsp)),0);
            if (tfs_subrsp == NULL) {
                wlan_wnm_free_tfs(&tfs_rsp);
                rv = -ENOMEM;
                goto out;
            }
            tfs_subrsp->elem_id = *frm++;
            tfs_subrsp->length = *frm++;
            tfs_subrsp->status = *frm++;
            tfs_subrsp->tfsid = *frm++;
            tfs_length = tfs_length - 4;
            tfsrsp->tfs_resq[resp_count].tfsid = tfs_subrsp->tfsid;
            tfsrsp->tfs_resq[resp_count].status = tfs_subrsp->status;
            resp_count++;
            TAILQ_INSERT_TAIL(&tfs_rsp->tfs_rsp_sub, tfs_subrsp, tsub_next);
        }
		TAILQ_INSERT_TAIL(&tfs_rsp_head, tfs_rsp, tfs_rsp_next);
    }
    tfsrsp->num_tfsresp = resp_count;
    tresplen = (tfsrsp->num_tfsresp * 2) + 1;

    event->type = 0x12;
    OS_MEMCPY(event->mac, ni->ni_macaddr, MAC_ADDR_LEN);
    OS_MEMCPY(event->event_data, tfsrsp, tresplen);
    event->datalen = tresplen;
    wnm_netlink_send(vap, event);

    TAILQ_CONCAT(&ni->ni_wnm->tfsrsp->tfs_rsp_head,
                        &tfs_rsp_head, tfs_rsp_next);
 out:
    qdf_mem_free(event);
    event = NULL;
    qdf_mem_free(tfsrsp);

    return resp_count;
}

int ieee80211_recv_tfs_resp(wlan_if_t vap, wlan_node_t ni,
                                u_int8_t *frm, int frm_len)
{
    int resp_count = 0;

    frm = frm + 2;
    frm++;
    resp_count = ieee80211_add_tfs_resp(vap, ni, frm, frm_len-3);
    if (resp_count < 0)
        return -EINVAL;

    return 0;
}


int ieee80211_recv_tfs_recv_notify(wlan_if_t vap, wlan_node_t ni,
                                u_int8_t *frm, int frm_len)
{
    u_int8_t tfsid_count;
    u_int8_t tfsid[10];
    wnm_netlink_event_t *event = NULL;

    event = qdf_mem_malloc(sizeof(*event));
    if (event == NULL) {
        printk("%s[%d]: Memory allocation failed\n", __func__, __LINE__);
        return -ENOMEM;
    }

    frm = frm + 2;
    tfsid_count = *frm++;
    OS_MEMCPY(tfsid , frm, tfsid_count);
    frm = frm + tfsid_count;
    event->type = 0x13;
    OS_MEMCPY(event->mac, ni->ni_macaddr, MAC_ADDR_LEN);
    OS_MEMCPY(event->event_data,  tfsid, tfsid_count);
    event->datalen = tfsid_count;
    wnm_netlink_send(vap, event);
    qdf_mem_free(event);
    event = NULL;

    return 0;
}

u_int8_t *
ieee80211_add_timreq_ie(wlan_if_t vap, wlan_node_t ni, u_int8_t *frm)
{
    *frm++ = IEEE80211_ELEMID_TIM_BCAST_REQUEST;
    *frm++ = 1;
    *frm++ = vap->wnm->wnm_timbcast_interval;

    return frm;
}

int ieee80211_send_tim_req(wlan_if_t vap, wlan_node_t ni)
{
    wbuf_t wbuf = NULL;
    u_int8_t *frm = NULL;

    wbuf = ieee80211_getmgtframe(ni, IEEE80211_FC0_SUBTYPE_ACTION, &frm, 0);
    if (wbuf == NULL) {
        return -ENOMEM;
    }

    *frm++ = IEEE80211_ACTION_CAT_WNM;
    *frm++ = IEEE80211_ACTION_TIM_REQ;
    ni->ni_wnm->timbcast_dialogtoken++;
    ni->ni_wnm->timbcast_dialogtoken %= 255;
    *frm++ = ni->ni_wnm->timbcast_dialogtoken;

    frm = ieee80211_add_timreq_ie(vap, ni, frm);

    wbuf_set_pktlen(wbuf, (frm - (u_int8_t*)wbuf_header(wbuf)));

    return ieee80211_send_mgmt(vap, ni, wbuf, false);
}

int ieee80211_parse_timresp_ie(u_int8_t *frm, u_int8_t *efrm, wlan_node_t ni)
{
    wlan_if_t vap;
    int status = -EINVAL;
    int len = 0;
    int resp_status = 0;

    vap = ni->ni_vap;

    vap->wnm->wnm_timbcast_enable = 0;
    while (((frm + 1) < efrm) && (frm + frm[1] + 1) < efrm) {
        switch (frm[0]) {
            case IEEE80211_ELEMID_TIM_BCAST_RESPONSE:
                len = frm[1];
                if (len < 2) {
                    vap->wnm->wnm_timbcast_interval = 0;
                    break;
                }
                resp_status = frm[2];
                if (resp_status == IEEE80211_WNM_TIMREQUEST_DENIED) {
                    vap->wnm->wnm_timbcast_interval = 0;
                    status = 0;
                    break;
                } else if (len != 10) {
                    ni->ni_wnm->timbcast_interval = 0;
                } else {
                    vap->wnm->wnm_timbcast_interval = frm[3];
                    vap->wnm->wnm_timbcast_offset =
                            le32toh(*((u_int32_t *)(frm + 4)));
                    vap->wnm->wnm_timbcast_highrate =
                            le16toh(*((u_int16_t *)(frm + 8)));
                    vap->wnm->wnm_timbcast_lowrate =
                            le16toh(*((u_int16_t *)(frm + 10)));
                    if (vap->wnm->wnm_timbcast_highrate) {
                        vap->wnm->wnm_timbcast_enable =
                            IEEE80211_WNM_TIM_HIGHRATE_ENABLE;
                    }
                    if (vap->wnm->wnm_timbcast_lowrate) {
                        vap->wnm->wnm_timbcast_enable |=
                            IEEE80211_WNM_TIM_LOWRATE_ENABLE;
                    }
	                    status = 0;
                }
            break;
            default:
            break;
        }
        frm += frm[1] + 2;
    }

    return status;
}
int ieee80211_recv_tim_resp(wlan_if_t vap, wlan_node_t ni,
                                u_int8_t *frm, int frm_len)
{
    int status = -EINVAL;
    u_int8_t *efrm;

    efrm = frm + frm_len;
    frm += 2;
    if (*frm != ni->ni_wnm->timbcast_dialogtoken) {
        return status;
    }
    frm++;
    status = ieee80211_parse_timresp_ie(frm, frm + frm_len, ni);
    return status;

}

#endif /* UMAC_SUPPORT_WNM */

