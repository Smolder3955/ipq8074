/*
 *
 * Copyright (c) 2014-2019 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2014-2016 Qualcomm Atheros, Inc.  All rights reserved.
 *
 * Qualcomm is a trademark of Qualcomm Technologies Incorporated, registered in the United
 * States and other countries.  All Qualcomm Technologies Incorporated trademarks are used with
 * permission.  Atheros is a trademark of Qualcomm Atheros, Inc., registered in
 * the United States and other countries.  Other products and brand names may be
 * trademarks or registered trademarks of their respective owners.
 */

#if UMAC_SUPPORT_WNM
/*
 *  Wireless Network Management handlers
 */
#include <ieee80211_var.h>
#include <ieee80211_ioctl.h>
#include "ieee80211_wnm_priv.h"
#include "linux/if.h"
#include "linux/socket.h"
#include "linux/netlink.h"
#include <net/netlink.h>
#include "linux/rtnetlink.h"
#include <net/sock.h>
#include <osif_private.h>
#include "ieee80211_ie_utils.h"
#include "if_athvar.h"
#include <wlan_mlme_dp_dispatcher.h>
#include <wlan_vdev_mlme.h>
#include <wlan_vdev_mgr_utils_api.h>
#define NETLINK_WNM_EVENT     20

/**
 * wnm action frame recv handler 
 * @param vap        : handle to vap pointer
 * @param ni         : node pointer
 * @param action     : action frame type
 * @param frm        : action frame pointer
 * @param frm_len    : frame length
 *
 * @return on success return 0.
 *         on failure returns -ve value.
 */

int ieee80211_wnm_vattach(struct ieee80211com *ic,ieee80211_vap_t vap)
{
    int i;
    ieee80211_fms_counter_t *fms_counter;
    ieee80211_wnm_t  *wnm = &vap->wnm;
    ieee80211_fms_t  *fms;
    uint32_t mgmt_rate;

    if (*wnm)
        return EINPROGRESS; /* already attached ? */

    *wnm = (ieee80211_wnm_t) OS_MALLOC(ic->ic_osdev, sizeof(struct ieee80211_wnm), 0);

    if (!*wnm)
    {
        return -ENOMEM;
    }

    OS_MEMZERO(*wnm, sizeof(struct ieee80211_wnm));
    (*wnm)->wnm_osdev  = ic->ic_osdev;
    if(vap->iv_opmode == IEEE80211_M_HOSTAP) {
        (*wnm)->wnm_bss_max_idle_period = IEEE80211_BSS_IDLE_PERIOD_DEFAULT;
    }

    wlan_util_vdev_mlme_get_param(vap->vdev_mlme,
            WLAN_MLME_CFG_TX_MGMT_RATE, &mgmt_rate);
    (*wnm)->wnm_timbcast_highrate = 48; // 24Mbps
    (*wnm)->wnm_timbcast_lowrate = mgmt_rate;
    (*wnm)->wnm_nl_sock = NULL;

    wnm_netlink_init(*wnm);
    (*wnm)->count = 0;

    fms = &vap->wnm->wnm_fms_data;
    *fms = (ieee80211_fms_t) OS_MALLOC(ic->ic_osdev, sizeof(struct ieee80211_fms), 0);
    if (!*fms)
    {
        return -ENOMEM;
    }

    OS_MEMZERO(*fms, sizeof(struct ieee80211_fms));
    for (i = 0; i < IEEE80211_FMS_MAX_COUNTERS; i++) {
        fms_counter = &(*fms)->counters[i];
        TAILQ_INIT(&fms_counter->fms_stream_head);
    }
    return EOK;
}

int ieee80211_wnm_vdetach(ieee80211_vap_t vap)
{
    int i;
    ieee80211_fms_counter_t *fms_counter;
    ieee80211_fms_stream_t *fms_stream, *tmp_stream;
    ieee80211_wnm_t *wnm = &vap->wnm;
    ieee80211_fms_t *fms;
    struct tclas_element *tclas, *tmp_tclas;

    if (*wnm == NULL) 
        return EINPROGRESS; /* already detached ? */


    wnm_netlink_delete(*wnm);

    fms = &vap->wnm->wnm_fms_data;
    if (*fms != NULL) {
        for (i = 0; i < IEEE80211_FMS_MAX_COUNTERS; i++) {
            fms_counter = &(*fms)->counters[i];
            TAILQ_FOREACH_SAFE(fms_stream, &fms_counter->fms_stream_head, stream_next, tmp_stream) {
                TAILQ_FOREACH_SAFE(tclas, &fms_stream->tclas_head, tclas_next, tmp_tclas){
                    TAILQ_REMOVE(&fms_stream->tclas_head, tclas, tclas_next);
                    OS_FREE(tclas);
                }
                TAILQ_REMOVE(&fms_counter->fms_stream_head, fms_stream, stream_next);
                OS_FREE(fms_stream);
            }
        }
    }
    OS_FREE(*fms);
    *fms = NULL;
    OS_FREE(*wnm);
    *wnm = NULL;

    return EOK;
}

void ieee80211_wnm_nattach(struct ieee80211_node *ni)
{
    struct ieee80211com *ic= ni->ni_ic;

    ni->ni_wnm->tfsreq =  (struct ieee80211_tfsreq *)
        OS_MALLOC(ic->ic_osdev, sizeof(struct ieee80211_tfsreq), 0);
 
    if(ni->ni_wnm->tfsreq == NULL) {
        goto bad;
    }

    ni->ni_wnm->tfsrsp = (struct ieee80211_tfsrsp *)
        OS_MALLOC(ic->ic_osdev, sizeof(struct ieee80211_tfsrsp), 0);


    if(ni->ni_wnm->tfsrsp == NULL) {
        OS_FREE(ni->ni_wnm->tfsreq);
        goto bad;
    }

    ni->ni_wnm->fmsreq = (struct ieee80211_fmsreq *)
        OS_MALLOC(ic->ic_osdev, sizeof(struct ieee80211_fmsreq), 0);

    if(ni->ni_wnm->fmsreq == NULL) {
        OS_FREE(ni->ni_wnm->tfsreq);
        OS_FREE(ni->ni_wnm->tfsrsp);
        goto bad;
    }

    ni->ni_wnm->fmsrsp = (struct ieee80211_fmsrsp *)
        OS_MALLOC(ic->ic_osdev, sizeof(struct ieee80211_fmsrsp), 0);

    if(ni->ni_wnm->fmsrsp == NULL) {
        OS_FREE(ni->ni_wnm->tfsreq);
        OS_FREE(ni->ni_wnm->tfsrsp);
        OS_FREE(ni->ni_wnm->fmsreq);
        goto bad;
    }
    TAILQ_INIT(&ni->ni_wnm->tfsreq->tfs_req_head);
    TAILQ_INIT(&ni->ni_wnm->tfsrsp->tfs_rsp_head);
    TAILQ_INIT(&ni->ni_wnm->fmsreq->fmsreq_head);
    TAILQ_INIT(&ni->ni_wnm->fmsrsp->fmsrsp_head);
    TAILQ_INIT(&ni->ni_wnm->fmsreq_act_head);

    return;

bad:
    IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_WNM, "%s: OS_MALLOC failed", __func__);
    OS_FREE(ni->ni_wnm);
    ni->ni_wnm = NULL;
}

void ieee80211_wnm_ndetach(struct ieee80211_node *ni)
{
    struct ieee80211_fmsreq_active_element *fmsreq_act_ptr, *tmp_req;
    struct ieee80211_fms_act_stream_ptr *fmsstream_ptr, *tmp_strm;
    struct tclas_element *tclas, *tmp_tclas;
    ieee80211_fms_stream_t *fms_stream;
    ieee80211_vap_t vap;
    ieee80211_fms_t fms = NULL;
    ieee80211_fms_counter_t *fms_counter;

    if (ni->ni_vap == NULL)
        return;

    vap = ni->ni_vap;

    if (vap->wnm != NULL) {
        fms = vap->wnm->wnm_fms_data;
    }
    if (ni->ni_wnm->timbcast_ie != NULL) {
        OS_FREE(ni->ni_wnm->timbcast_ie);
        ni->ni_wnm->timbcast_ie = NULL;
    }
    if(ni->ni_wnm->tfsreq != NULL) {
        OS_FREE(ni->ni_wnm->tfsreq);
        ni->ni_wnm->tfsreq = NULL;
    }
    if(ni->ni_wnm->tfsrsp != NULL) {
        OS_FREE(ni->ni_wnm->tfsrsp);
        ni->ni_wnm->tfsrsp = NULL;
    }

    if(ni->ni_wnm->fmsreq != NULL) {
        OS_FREE(ni->ni_wnm->fmsreq);
        ni->ni_wnm->fmsreq = NULL;
    }
    if(ni->ni_wnm->fmsrsp != NULL) {
        OS_FREE(ni->ni_wnm->fmsrsp);
        ni->ni_wnm->fmsrsp = NULL;
    }

    if( fms == NULL) {
       return;
    }
    TAILQ_FOREACH_SAFE(fmsreq_act_ptr, &ni->ni_wnm->fmsreq_act_head, fmsreq_act_next, tmp_req) {
        /* Free fms token */
        /*fmsreq_act_ptr->fms_token;*/
        TAILQ_FOREACH_SAFE(fmsstream_ptr, &fmsreq_act_ptr->fmsact_strmptr_head, strm_ptr_next, tmp_strm) {
            fms_stream = fmsstream_ptr->stream;
            fms_stream->num_sta--;
            if (fms_stream->num_sta == 0) {
                /* Free fmsid */
                /* fms_stream->fmsid;*/
                fms_counter = &fms->counters[fms_stream->counter_id];
                TAILQ_FOREACH_SAFE(tclas, &fms_stream->tclas_head, tclas_next, tmp_tclas){
                    TAILQ_REMOVE(&fms_stream->tclas_head, tclas, tclas_next);
                    OS_FREE(tclas);
                }
                TAILQ_REMOVE(&fms_counter->fms_stream_head, fms_stream, stream_next);
                OS_FREE(fms_stream);
            }
            TAILQ_REMOVE(&fmsreq_act_ptr->fmsact_strmptr_head,fmsstream_ptr, strm_ptr_next);
            OS_FREE(fmsstream_ptr);
        }
        TAILQ_REMOVE(&ni->ni_wnm->fmsreq_act_head, fmsreq_act_ptr, fmsreq_act_next);
        OS_FREE(fmsreq_act_ptr);
    }
}

int wnm_netlink_send(struct ieee80211vap *vap, wnm_netlink_event_t *event) 
{
    struct sk_buff *skb;
    int err;
    struct net_device *dev = ((osif_dev *)vap->iv_ifp)->netdev;
    struct nl_event_info ev_info = {0};

#if LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,24)
    if (!net_eq(dev_net(dev), &init_net))
        return QDF_STATUS_E_FAILURE  ;
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,24)
    skb = nlmsg_new(NLMSG_DEFAULT_SIZE, GFP_ATOMIC);
#else
    skb = nlmsg_new(NLMSG_SPACE(IEEE80211_WNM_NETLINKBUF));
#endif
    if (!skb)
        return -ENOMEM;

    ev_info.type      = RTM_NEWLINK;
    ev_info.ev_type   = TYPE_SENT_EVENT;
    ev_info.ev_len    = sizeof(wnm_netlink_event_t) - IEEE80211_WNM_NETLINKBUF
                        + event->datalen;
    ev_info.pid       = 0;
    ev_info.seq       = 0;
    ev_info.flags     = 0;
    ev_info.event     = event;

    err = nl_ev_fill_info(skb, dev, &ev_info);
    if (err < 0) {
        kfree_skb(skb);
        return -1;
    }

    NETLINK_CB(skb).dst_group = RTNLGRP_LINK;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,7,0)
    NETLINK_CB(skb).pid = 0;  /* from kernel */
#else
    NETLINK_CB(skb).portid = 0;  /* from kernel */
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)    
    NETLINK_CB(skb).dst_pid = 0;  /* multicast */
#endif


    /* Send event to acfg */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)   
    err = nlmsg_multicast(vap->wnm->wnm_nl_sock, skb, 0, RTNLGRP_NOTIFY);
#else
    rtnl_notify(skb, &init_net, 0, RTNLGRP_NOTIFY, NULL, GFP_ATOMIC);
#endif
    return 1;
}



int wnm_netlink_init(ieee80211_wnm_t wnm)
{
    if (wnm->wnm_nl_sock == NULL) {
#if LINUX_VERSION_CODE > KERNEL_VERSION (2,6,30)
        return QDF_STATUS_SUCCESS;
#elif LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,24)
        wnm->wnm_nl_sock = (struct sock *)netlink_kernel_create(&init_net,
                                NETLINK_WNM_EVENT, 1, NULL,
                                NULL, THIS_MODULE);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,22)
        wnm->wnm_nl_sock = (struct sock *)netlink_kernel_create(NETLINK_WNM_EVENT, 
                                1, NULL, (struct mutex *) NULL, THIS_MODULE);
#else
        wnm->wnm_nl_sock = (struct sock *)netlink_kernel_create(NETLINK_WNM_EVENT, 
                                1, NULL, THIS_MODULE);
#endif

        if (wnm->wnm_nl_sock == NULL) {
            printk("%s: %d NETLINK_KERNEL_CREATE failed!\n", __func__, __LINE__);
            return -ENODEV;
        }
    }
    return 0;
}

int wnm_netlink_delete(ieee80211_wnm_t wnm)
{
    if (wnm->wnm_nl_sock) {
	netlink_kernel_release(wnm->wnm_nl_sock);
        wnm->wnm_nl_sock = NULL;
    }

    return 0;
}


void
ieee80211_wnm_add_extcap(struct ieee80211_node *ni, u_int32_t *ext_capflags)
{
    struct ieee80211vap      *vap = ni->ni_vap;

    if (ieee80211_vap_wnm_is_set(vap)) {
        *ext_capflags |= IEEE80211_EXTCAPIE_BSSTRANSITION;
        *ext_capflags |= IEEE80211_EXTCAPIE_TIMBROADCAST;
        *ext_capflags |= IEEE80211_EXTCAPIE_TFS;
        *ext_capflags |= IEEE80211_EXTCAPIE_WNMSLEEPMODE;
        if (ieee80211_wnm_fms_is_set(vap->wnm)) {
            *ext_capflags |= IEEE80211_EXTCAPIE_FMS;
        }
    }

    return;
}

/* returns non-zero if action not-taken */
int ieee80211_wnm_recv_action(wlan_if_t vap, wlan_node_t ni, u_int8_t action,
                              u_int8_t *frm, u_int8_t frm_len)
{
    bool action_taken = TRUE;

    if(ieee80211_vap_wnm_is_set(vap) == 0) {
        return -EINVAL;
    }

    switch (action) {
    case IEEE80211_ACTION_BSTM_QUERY:
        ieee80211_recv_bstm_query(vap, ni,
                 (struct ieee80211_action_bstm_query *)frm, frm_len);
        break;
    case IEEE80211_ACTION_BSTM_RESP:
        ieee80211_recv_bstm_resp(vap, ni,
                 (struct ieee80211_action_bstm_resp *)frm, frm_len);
        break;
    case IEEE80211_ACTION_FMS_REQ:
        ieee80211_recv_fms_req(vap, ni, frm, frm_len);
        break;
    
    case IEEE80211_ACTION_FMS_RESP:
        ieee80211_recv_fms_rsp(vap, ni, frm, frm_len);
        break;

    case IEEE80211_ACTION_TFS_REQ:
        ieee80211_recv_tfs_req(vap, ni, frm, frm_len);
        break;
        break;
    case IEEE80211_ACTION_TFS_RESP:
        ieee80211_recv_tfs_resp(vap, ni,
                        frm, frm_len);
        break;
    case IEEE80211_ACTION_TFS_NOTIFY:
        ieee80211_recv_tfs_recv_notify(vap, ni,
                        frm, frm_len);
        break;
    case IEEE80211_ACTION_TIM_REQ:
        ieee80211_recv_tim_req(vap, ni, frm, frm_len);
        break;
    case IEEE80211_ACTION_TIM_RESP:
        ieee80211_recv_tim_resp(vap, ni, frm, frm_len);
        break;
    case IEEE80211_ACTION_WNM_REQ:
        ieee80211_recv_wnm_req(vap, ni, frm, frm_len);
        break;
    case IEEE80211_ACTION_WNMSLEEP_REQ:
    case IEEE80211_ACTION_WNMSLEEP_RESP:
        action_taken = FALSE;
        break;
    default:
        break;
    }

    return (action_taken ? EOK : -EINVAL);
}

int wlan_set_bssidpref(wlan_if_t vap, struct ieee80211_user_bssid_pref *userbssidpref)
{
    ieee80211_wnm_t  *wnm = &vap->wnm;
    int i;

    if (IS_NULL_ADDR(userbssidpref->bssid)) {
        (*wnm)->count = 0;
        return EOK;
    }

    /* Allow user to configure preference value for AP's own BSS */
    if (IEEE80211_ADDR_EQ(userbssidpref->bssid, vap->iv_bss->ni_bssid)) {
        (*wnm)->wnm_bss_pref = userbssidpref->pref_val;
        return EOK;
    }

    for ( i = 0; i < (*wnm)->count; i++) {
        if (!OS_MEMCMP(userbssidpref->bssid, (*wnm)->store_userbssidpref_in_ap[i].bssid,
                       sizeof(userbssidpref->bssid))) {
            (*wnm)->store_userbssidpref_in_ap[i].pref_val = userbssidpref->pref_val;
            (*wnm)->store_userbssidpref_in_ap[i].chan = userbssidpref->chan;
            (*wnm)->store_userbssidpref_in_ap[i].regclass = userbssidpref->regclass;
            return EOK;
        }
    }

    if ( (*wnm)->count == 0x20 ) { /* max numbef of entries */
        return  -E2BIG;
    }
    /*
     * Copying user given values to vap's internal storage
     */
    IEEE80211_ADDR_COPY( (*wnm)->store_userbssidpref_in_ap[(*wnm)->count].bssid, userbssidpref->bssid);
    (*wnm)->store_userbssidpref_in_ap[(*wnm)->count].pref_val = userbssidpref->pref_val;
    (*wnm)->store_userbssidpref_in_ap[(*wnm)->count].chan = userbssidpref->chan;
    (*wnm)->store_userbssidpref_in_ap[(*wnm)->count].regclass = userbssidpref->regclass;
    (*wnm)->count++;

    return EOK;

}

int wlan_send_bstmreq(wlan_if_t vaphandle, u_int8_t *macaddr,
                      struct ieee80211_bstm_reqinfo *bstmreq)
{
    struct ieee80211vap      *vap = vaphandle;
    struct ieee80211_node    *ni;
    int error = 0;

    ni = ieee80211_vap_find_node(vap, macaddr);
    if (ni == NULL) {
        error = -EINVAL;
        goto exit;
    }
    error = ieee80211_send_bstm_req(vap, ni, bstmreq);

    /* claim node immediately */
    ieee80211_free_node(ni);

exit:
    return error;
}

int wlan_send_bstmreq_target(wlan_if_t vaphandle, u_int8_t *macaddr,
                             struct ieee80211_bstm_reqinfo_target *bstmreq)
{
    struct ieee80211vap      *vap = vaphandle;
    struct ieee80211_node    *ni;
    int error = 0;

    ni = ieee80211_vap_find_node(vap, macaddr);
    if (ni == NULL) {
        error = -EINVAL;
        goto exit;
    }

    error = ieee80211_send_bstm_req_target(vap, ni, bstmreq);

    /* claim node immediately */
    ieee80211_free_node(ni);

exit:
    return error;
}

int wlan_wnm_vap_is_set(wlan_if_t vaphandle) 
{
    struct ieee80211vap      *vap = vaphandle;

    return ieee80211_vap_wnm_is_set(vap);
}

int wlan_wnm_set_bssmax(wlan_if_t vaphandle, void *data)
{
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211_wlanconfig_wnm_bssmax *bssmax;
    int status = -1;

    if (!ieee80211_wnm_bss_is_set(vap->wnm)) {
       return -EFAULT;
    }
    bssmax = (struct ieee80211_wlanconfig_wnm_bssmax *) data;
    if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
        if ((bssmax->idleperiod < 1) || (bssmax->idleperiod > 65534)) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM,
                              "%s: Idle period (1 to 65535)\n", __func__);
        } else if (bssmax->idleoption >= (WNM_BSS_IDLE_PROT_FORCE_MASK<<1)) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM,
                              "%s: Idle option (0 - %d)\n", __func__,
			      ((WNM_BSS_IDLE_PROT_FORCE_MASK<<1)-1));
        } else {
            vap->wnm->wnm_bss_max_idle_period = bssmax->idleperiod;
            vap->wnm->wnm_bss_idle_option = bssmax->idleoption;
            status = 0;
        }
    }

    return status;
}

int wlan_wnm_get_bssmax(wlan_if_t vaphandle, void *data)
{
    struct ieee80211vap      *vap = vaphandle;
    struct ieee80211_wlanconfig_wnm_bssmax *bssmax;

    if (!ieee80211_wnm_bss_is_set(vap->wnm))
        return -EFAULT;

    bssmax = (struct ieee80211_wlanconfig_wnm_bssmax *) data;
    bssmax->idleperiod = vap->wnm->wnm_bss_max_idle_period;
    bssmax->idleoption = vap->wnm->wnm_bss_idle_option;
    return 0;
}


void ieee80211_wnm_parse_bssmax_ie(struct ieee80211_node *ni, u_int8_t *frm)
{
    struct ieee80211vap *vap = ni->ni_vap;
    vap->wnm->wnm_bss_max_idle_period = le16toh(*(u_int16_t *)(frm + 2));
    vap->wnm->wnm_bss_idle_option = frm[4];
}


int wlan_wnm_free_tfsrsp(void *head)
{
    TAILQ_HEAD(, ieee80211_tfs_response) *tfs_rsp_head;
    ieee80211_tfs_response *trsp, *temp_trsp;
    ieee80211_tfs_subelement_rsp *tsub_rsp, *temp_tsubrsp;

    tfs_rsp_head = head;
    TAILQ_FOREACH_SAFE(trsp, tfs_rsp_head, tfs_rsp_next, temp_trsp) {
        TAILQ_FOREACH_SAFE(tsub_rsp, &trsp->tfs_rsp_sub, tsub_next,
                                                    temp_tsubrsp) {
            TAILQ_REMOVE(&trsp->tfs_rsp_sub, tsub_rsp, tsub_next);
            OS_FREE(tsub_rsp);
        }
        TAILQ_REMOVE(tfs_rsp_head, trsp, tfs_rsp_next);
        OS_FREE(trsp);
    }
    return 0;
}

int wlan_wnm_tfs_filter(wlan_if_t vaphandle, wbuf_t wbuf)
{
    struct ieee80211vap      *vap = vaphandle;

    return (ieee80211_wnm_tfs_is_set(vap->wnm) && 
            !ieee80211_tfs_filter(vap, wbuf));
}

int  ieee80211_wnm_free_fmsrsp(void *head) 
{
    TAILQ_HEAD(, ieee80211_fms_response_s) *rsp_head; 
    struct ieee80211_fms_response_s *cur_rsp, *temp_rsp; 
    struct ieee80211_fms_status_subelement_s *cur_subele_status, *temp_subele_status; 
    struct tclas_element *tclas; 

    rsp_head = head; 
    TAILQ_FOREACH_SAFE(cur_rsp, rsp_head, fmsrsp_next, temp_rsp) { 
        TAILQ_FOREACH_SAFE(cur_subele_status, &cur_rsp->status_subele_head, status_subele_next, temp_subele_status) { 
                if(cur_subele_status->subelementid == IEEE80211_TCLASS_STATUS_SUBELE) { 
                tclas = (struct tclas_element *)cur_subele_status->subele_status;
                if (tclas->type == IEEE80211_WNM_TCLAS_CLASSIFIER_TYPE3) { 
                    OS_FREE(tclas->tclas.type3.filter_value);
                    OS_FREE(tclas->tclas.type3.filter_mask);
                }
                OS_FREE(tclas); 
            }
            else if (cur_subele_status->subelementid == IEEE80211_FMS_STATUS_SUBELE)
            {
                if (cur_subele_status->subele_status != NULL)
                    OS_FREE(cur_subele_status->subele_status);
            }
            TAILQ_REMOVE(&cur_rsp->status_subele_head, cur_subele_status, status_subele_next);
            OS_FREE(cur_subele_status);
        }
        TAILQ_REMOVE(rsp_head, cur_rsp, fmsrsp_next);
        OS_FREE(cur_rsp);
    }
    return 0;
}
void  wlan_wnm_free_tclass(void *head)
{
    TAILQ_HEAD(, tclas_element) *tclas_head; 
    struct tclas_element *cur_tclas, *temp_tclas;

    tclas_head = head; 

    TAILQ_FOREACH_SAFE(cur_tclas, tclas_head, tclas_next,
            temp_tclas) { 
        if (cur_tclas->type == IEEE80211_WNM_TCLAS_CLASSIFIER_TYPE3) { 
            OS_FREE(cur_tclas->tclas.type3.filter_value);
            OS_FREE(cur_tclas->tclas.type3.filter_mask);
        }
        TAILQ_REMOVE(tclas_head, cur_tclas, tclas_next);
        OS_FREE(cur_tclas); 
    }
}
int  ieee80211_wnm_free_fmsreq(void *head) 
{
    TAILQ_HEAD(, ieee80211_fms_request_s) *req_head; 
    ieee80211_fms_request_t *cur_req, *temp_req; 
#if 0
    struct ieee80211_node_fmssubele_s  *cur_subele, *temp_subele; 
#endif
    ieee80211_fmsreq_subele_t          *cur_subele, *temp_subele;
    struct tclas_element               *cur_tclas, *temp_tclas; 
    req_head = head; 
    TAILQ_FOREACH_SAFE(cur_req, req_head, fmsreq_next, temp_req) { 
        TAILQ_FOREACH_SAFE(cur_subele, &cur_req->fmssubele_head, fmssubele_next, temp_subele) {
            if (cur_subele->accepted != 1) { 
            TAILQ_FOREACH_SAFE(cur_tclas, &cur_subele->tclas_head, tclas_next,
                                                        temp_tclas) { 
                if (cur_tclas->type == IEEE80211_WNM_TCLAS_CLASSIFIER_TYPE3) { 
                    OS_FREE(cur_tclas->tclas.type3.filter_value);
                    OS_FREE(cur_tclas->tclas.type3.filter_mask);
                }
                TAILQ_REMOVE(&cur_subele->tclas_head, cur_tclas, tclas_next);
                OS_FREE(cur_tclas); 
            }
            }
            TAILQ_REMOVE(&cur_req->fmssubele_head, cur_subele, fmssubele_next);
            OS_FREE(cur_subele);
        }
        TAILQ_REMOVE(req_head, cur_req, fmsreq_next);
        OS_FREE(cur_req);
    }
    return 0;
}

int wlan_wnm_free_tfs(void *head)
{
    TAILQ_HEAD(, ieee80211_tfs_request) *tfs_req_head;
    ieee80211_tfs_request *treq, *temp_treq;
    ieee80211_tfs_subelement_req *tsub, *temp_tsub;
    ieee80211_tclas_element *tclas, *temp_tclas;

    tfs_req_head = head;

    TAILQ_FOREACH_SAFE(treq, tfs_req_head, tfs_req_next, temp_treq) {
        TAILQ_FOREACH_SAFE(tsub, &treq->tfs_req_sub, tsub_next, temp_tsub) {
            TAILQ_FOREACH_SAFE(tclas, &tsub->tclas_head, tclas_next, temp_tclas) {
                if (tclas->type == IEEE80211_WNM_TCLAS_CLASSIFIER_TYPE3) {
                    OS_FREE(tclas->tclas.type3.filter_value);
                    OS_FREE(tclas->tclas.type3.filter_mask);
                }
                TAILQ_REMOVE(&tsub->tclas_head, tclas, tclas_next);
                OS_FREE(tclas);
            }
            TAILQ_REMOVE(&treq->tfs_req_sub, tsub, tsub_next);
            OS_FREE(tsub);
        }
        TAILQ_REMOVE(tfs_req_head, treq, tfs_req_next);
        OS_FREE(treq);
    }
    return 0;
}

int wlan_wnm_delete_tfs_req(struct ieee80211_node *ni, u_int8_t tfsid)
{
    ieee80211_tfs_request *treq, *temp_treq;
    ieee80211_tfs_subelement_req *tsub, *temp_tsub;
    ieee80211_tclas_element *tclas, *temp_tclas;

    TAILQ_FOREACH_SAFE(treq, &ni->ni_wnm->tfsreq->tfs_req_head, tfs_req_next, temp_treq) {
        if (tfsid == treq->tfs_id) {
            TAILQ_FOREACH_SAFE(tsub, &treq->tfs_req_sub, tsub_next, temp_tsub) {
                TAILQ_FOREACH_SAFE(tclas, &tsub->tclas_head, tclas_next,
                                                        temp_tclas) {
                    if (tclas->type == IEEE80211_WNM_TCLAS_CLASSIFIER_TYPE3) {
                        OS_FREE(tclas->tclas.type3.filter_value);
                        OS_FREE(tclas->tclas.type3.filter_mask);
                    }
                    TAILQ_REMOVE(&tsub->tclas_head, tclas, tclas_next);
                    OS_FREE(tclas);
                }
                TAILQ_REMOVE(&treq->tfs_req_sub, tsub, tsub_next);
                OS_FREE(tsub);
            }
            TAILQ_REMOVE(&ni->ni_wnm->tfsreq->tfs_req_head, treq, tfs_req_next);
            OS_FREE(treq);
        }
    }

    return 0;
}

int ieee80211_tclass_element_parse(wlan_if_t vap, void *head,
                                   ieee80211_tclas_processing *tclasprocess,
                                   struct fmsresp_tclas_subele_status *tclas_status_msg,
                                   u_int8_t **frm, u_int8_t tclass_len)
{
    TAILQ_HEAD(, tclas_element) *tclass_head;
    uint8_t *filter_value;
    uint8_t *filter_mask;
    struct tclas_element *tclas;
    int filter_len;
    struct tfsreq_tclas_element *tclass_msg;
    u_int8_t num_tclas_elements=0;
    u_int8_t elemid, version;

    tclass_head = head;

    TAILQ_INIT(tclass_head);

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, "***** Enter Function %s tclas_len %d\n", __func__, tclass_len);
    if (tclas_status_msg != NULL) {
        OS_MEMSET(tclas_status_msg, 0, sizeof(*tclas_status_msg));
    }

    while (tclass_len > 0) {
        elemid = *(*frm)++;
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, "tclass_len %d elemend id %x\n", tclass_len, elemid);
        if (IEEE80211_ELEMID_TCLAS == elemid) {
            tclas = OS_MALLOC(vap->iv_ic->ic_osdev,
                    sizeof(struct tclas_element), 0);

            if (tclas == NULL) {
                wlan_wnm_free_tclass(tclass_head);
                printk("%s: %d OS_MALLOC failed!\n", __func__, __LINE__);
                return ENOMEM;
            }

            tclas->elemid = elemid;
            tclas->len = *(*frm)++;
            tclas->up = *(*frm)++;
            tclas->type = *(*frm)++;
            tclas->mask = *(*frm)++;
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, "len %d up %d type %d mask %d\n",
                    tclas->len, tclas->up, tclas->type,tclas->mask);
            tclass_msg = NULL;

            if (tclas_status_msg != NULL && num_tclas_elements < FMS_MAX_TCLAS_ELEMENTS){
                tclass_msg = &tclas_status_msg->tclas[num_tclas_elements];
                tclass_msg->classifier_type = tclas->type;
                tclass_msg->classifier_mask = tclas->mask;
                tclass_msg->priority = tclas->up;
            }

            if(tclas->type == IEEE80211_WNM_TCLAS_CLASSIFIER_TYPE3) 
            {
                filter_len = (tclas->len - 5) / 2;
                filter_value = OS_MALLOC(vap->iv_ic->ic_osdev, filter_len, GFP_KERNEL);
                if (filter_value == NULL) {
                    printk("%s: %d OS_MALLOC failed!\n", __func__, __LINE__);
                    OS_FREE(tclas);
                    wlan_wnm_free_tclass(tclass_head);
                    return ENOMEM;
                }

                filter_mask = OS_MALLOC(vap->iv_ic->ic_osdev, filter_len, GFP_KERNEL);
  
                if (filter_mask == NULL) {
                    printk("%s: %d OS_MALLOC failed !\n", __func__, __LINE__);
                    OS_FREE(tclas);
                    OS_FREE(filter_value);
                    wlan_wnm_free_tclass(tclass_head);
                    return ENOMEM;
                }

                tclas->tclas.type3.filter_offset = (**(u_int16_t **)frm);
                *frm += 2;
                memcpy(filter_value, *frm , filter_len);
                tclas->tclas.type3.filter_value = filter_value;
                *frm += filter_len;
                memcpy(filter_mask, *frm, filter_len);
                tclas->tclas.type3.filter_mask = filter_mask;
                *frm += filter_len;

                if (tclass_msg != NULL) {
                    struct clas3 *clas3;
                    clas3 = &tclass_msg->clas.clas3;
                    clas3->filter_offset = tclas->tclas.type3.filter_offset;
                    clas3->filter_len = filter_len;
                    if (clas3->filter_len > TFS_MAX_FILTER_LEN) {
                        clas3->filter_len = TFS_MAX_FILTER_LEN;
                    }
                    memcpy(clas3->filter_value, filter_value, clas3->filter_len);
                    memcpy(clas3->filter_mask, filter_mask, clas3->filter_len);
                }
            } 
            else if (tclas->type == IEEE80211_WNM_TCLAS_CLASSIFIER_TYPE1 ||
                     tclas->type == IEEE80211_WNM_TCLAS_CLASSIFIER_TYPE4) 
            {
                version = *(*frm)++;
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, "version %d", version);
                if (version == IEEE80211_WNM_TCLAS_CLAS14_VERSION_4) {
                    tclas->tclas.type14.type14_v4.version = version;
                    memcpy(tclas->tclas.type14.type14_v4.src_ip, *frm, IEEE80211_IPV4_LEN);
                    *frm += IEEE80211_IPV4_LEN;
                    memcpy(tclas->tclas.type14.type14_v4.dst_ip, *frm, IEEE80211_IPV4_LEN);
                    *frm += IEEE80211_IPV4_LEN;
                    tclas->tclas.type14.type14_v4.src_port = be16toh(**(u_int16_t **)frm);
                    *frm += 2;
                    tclas->tclas.type14.type14_v4.dst_port = be16toh(**(u_int16_t**)frm);
                    *frm += 2;
                    tclas->tclas.type14.type14_v4.dscp = *(*frm)++;
                    tclas->tclas.type14.type14_v4.protocol = *(*frm)++;
                    tclas->tclas.type14.type14_v4.reserved = *(*frm)++;
                    
                    if (tclass_msg != NULL) {
                        struct clas14_v4 *ipv4;
                        ipv4 = &tclass_msg->clas.clas14.clas14_v4;
                        ipv4->version = tclas->tclas.type14.type14_v4.version;
                        memcpy(ipv4->source_ip, tclas->tclas.type14.type14_v4.src_ip, IEEE80211_IPV4_LEN);
                        memcpy(ipv4->dest_ip, tclas->tclas.type14.type14_v4.dst_ip, IEEE80211_IPV4_LEN);
                        ipv4->source_port = tclas->tclas.type14.type14_v4.src_port;
                        ipv4->dest_port = tclas->tclas.type14.type14_v4.dst_port;
                        ipv4->dscp = tclas->tclas.type14.type14_v4.dscp;
                        ipv4->protocol = tclas->tclas.type14.type14_v4.protocol;
                        if (ipv4->dest_ip[0] >= 224 && ipv4->dest_ip[0] <= 234) {
                            IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, "Mcast tclass element\n");
                            tclas_status_msg->ismcast = 1;
                            memcpy(&tclas_status_msg->mcast_ipaddr, ipv4->dest_ip, IEEE80211_IPV4_LEN);
                        }
                    } 
                } else if (version == IEEE80211_WNM_TCLAS_CLAS14_VERSION_6) {
                    tclas->tclas.type14.type14_v6.version = version;
                    memcpy(tclas->tclas.type14.type14_v6.src_ip, *frm, IEEE80211_IPV6_LEN);
                    *frm += IEEE80211_IPV6_LEN;
                    memcpy(tclas->tclas.type14.type14_v6.dst_ip, *frm, IEEE80211_IPV6_LEN);
                    *frm += IEEE80211_IPV6_LEN;
                    tclas->tclas.type14.type14_v6.src_port = be16toh(**(u_int16_t**)frm);
                    *frm += 2;
                    tclas->tclas.type14.type14_v6.dst_port = be16toh(**(u_int16_t**)frm);
                    *frm += 2;
                    if (tclas->type == IEEE80211_WNM_TCLAS_CLASSIFIER_TYPE4) {
                        tclas->tclas.type14.type14_v6.type4_dscp = *(*frm)++;
                        tclas->tclas.type14.type14_v6.type4_next_header = *(*frm)++;
                    }
                    memcpy(tclas->tclas.type14.type14_v6.flow_label, *frm, 3);
                    *frm += 3;

                    if (tclass_msg != NULL) {
                        struct clas14_v6 *ipv6;
                        ipv6 = &tclass_msg->clas.clas14.clas14_v6;
                        ipv6->version = tclas->tclas.type14.type14_v6.version;
                        memcpy(ipv6->source_ip, tclas->tclas.type14.type14_v6.src_ip, IEEE80211_IPV6_LEN);
                        memcpy(ipv6->dest_ip, tclas->tclas.type14.type14_v6.dst_ip, IEEE80211_IPV6_LEN);
                        ipv6->source_port = tclas->tclas.type14.type14_v6.src_port;
                        ipv6->dest_port = tclas->tclas.type14.type14_v6.dst_port;
                        if (tclas->type == IEEE80211_WNM_TCLAS_CLASSIFIER_TYPE4) {
                            /* Only for Classifier Type 4 */
                            ipv6->clas4_dscp = tclas->tclas.type14.type14_v6.type4_dscp;
                            ipv6->clas4_next_header = tclas->tclas.type14.type14_v6.type4_next_header;
                        }
                    } 
                }
            } else {
                wlan_wnm_free_tclass(tclass_head);
                printk("%s: %d unknown tclass classifier type %d\n",
                          __func__, __LINE__, tclas->type);
                OS_FREE(tclas);
                return -EINVAL;
            }
            num_tclas_elements++;
            tclass_len = tclass_len - (tclas->len + 2);
            TAILQ_INSERT_TAIL(tclass_head, tclas, tclas_next);
        } else if (IEEE80211_ELEMID_TCLAS_PROCESS == elemid) {
            tclasprocess->elem_id = elemid;
            tclasprocess->length = *(*frm)++;
            tclasprocess->tclas_process = *(*frm)++;
            tclass_len = tclass_len - 3;
            if (tclas_status_msg != NULL) {
                memcpy(&tclas_status_msg->tclasprocess, tclasprocess,
                        sizeof(ieee80211_tclas_processing));
            }
        } else {
            wlan_wnm_free_tclass(tclass_head);
            printk("%s: %d unknown elementid %d\n", __func__, __LINE__, elemid);
            return -EINVAL;
        }
    }

    if(tclas_status_msg) {
        tclas_status_msg->num_tclas_elements = num_tclas_elements;
    }

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, "***** Exit Function %s\n", __func__);
    return 0;
}

int wlan_wnm_set_tclass(wlan_if_t vap, struct tfsreq_tclas_element *req_tclas,
                        void *head, int *subelem_length)
{
    ieee80211_tclas_element *tclas;
    struct tclas_type14_v6 *ipv6;
    TAILQ_HEAD(, tclas_element) *tclas_head;
    uint8_t *filter_value;
    uint8_t *filter_mask;

    tclas_head = head;
    
    if (req_tclas->classifier_type == IEEE80211_WNM_TCLAS_CLASSIFIER_TYPE1 ||
        req_tclas->classifier_type == IEEE80211_WNM_TCLAS_CLASSIFIER_TYPE4)
    {
        if (req_tclas->clas.clas14.clas14_v4.version ==
            IEEE80211_WNM_TCLAS_CLAS14_VERSION_4)
        {
            tclas = OS_MALLOC(vap->iv_ic->ic_osdev,
                    sizeof(ieee80211_tclas_element), GFP_KERNEL);
            if (tclas == NULL) {
                return -ENOMEM;
            }

            tclas->elemid = IEEE80211_ELEMID_TCLAS;
            tclas->len = sizeof(struct clas14_v4);;
            tclas->up = req_tclas->priority;
            tclas->type = req_tclas->classifier_type;
            tclas->mask = req_tclas->classifier_mask;
            tclas->len = tclas->len + 3;
            tclas->tclas.type14.type14_v4.version =
                            req_tclas->clas.clas14.clas14_v4.version;
            tclas->tclas.type14.type14_v4.src_port =
                       req_tclas->clas.clas14.clas14_v4.source_port;
            tclas->tclas.type14.type14_v4.dst_port =
                       req_tclas->clas.clas14.clas14_v4.dest_port;
            tclas->tclas.type14.type14_v4.dscp =
                       req_tclas->clas.clas14.clas14_v4.dscp;
            tclas->tclas.type14.type14_v4.protocol =
                       req_tclas->clas.clas14.clas14_v4.protocol;
            tclas->tclas.type14.type14_v4.reserved = 0;
            memcpy(&tclas->tclas.type14.type14_v4.src_ip,
                     &req_tclas->clas.clas14.clas14_v4.source_ip,
                     IEEE80211_IPV4_LEN);
            memcpy(&tclas->tclas.type14.type14_v4.dst_ip,
                     &req_tclas->clas.clas14.clas14_v4.dest_ip,
                    IEEE80211_IPV4_LEN);
            tclas->tclas.type14.type14_v4.version =
                    IEEE80211_WNM_TCLAS_CLAS14_VERSION_4;
            TAILQ_INSERT_TAIL(tclas_head, tclas, tclas_next);
            *subelem_length += tclas->len + (2 * sizeof(uint8_t));

        } else if (req_tclas->clas.clas14.clas14_v6.version ==
                        IEEE80211_WNM_TCLAS_CLAS14_VERSION_6)
        {
            tclas = OS_MALLOC(vap->iv_ic->ic_osdev,
                    sizeof(ieee80211_tclas_element), GFP_KERNEL);
            if (tclas == NULL) {
                return -ENOMEM;
            }
            tclas->elemid = IEEE80211_ELEMID_TCLAS;
            tclas->len = sizeof(struct tclas_type14_v6);
            tclas->up = req_tclas->priority;
            tclas->type = req_tclas->classifier_type;
            tclas->mask = req_tclas->classifier_mask;
            tclas->len = tclas->len + 3;
            ipv6 = &tclas->tclas.type14.type14_v6;
            ipv6->version = IEEE80211_WNM_TCLAS_CLAS14_VERSION_6;
            memcpy(ipv6->src_ip,&req_tclas->clas.clas14.clas14_v6.source_ip, IEEE80211_IPV6_LEN);
            memcpy(ipv6->dst_ip,&req_tclas->clas.clas14.clas14_v6.dest_ip, IEEE80211_IPV6_LEN);
            ipv6->src_port = req_tclas->clas.clas14.clas14_v6.source_port;
            ipv6->dst_port = req_tclas->clas.clas14.clas14_v6.dest_port;
            if (req_tclas->classifier_type == IEEE80211_WNM_TCLAS_CLASSIFIER_TYPE4) {
                /* Only for Classifier Type 4 */
                ipv6->type4_dscp = req_tclas->clas.clas14.clas14_v6.clas4_dscp;
                ipv6->type4_next_header = req_tclas->clas.clas14.clas14_v6.clas4_next_header;
            }
            memcpy(ipv6->flow_label,&req_tclas->clas.clas14.clas14_v6.flow_label, 3);
            TAILQ_INSERT_TAIL(tclas_head, tclas, tclas_next);
            *subelem_length += tclas->len + (2 * sizeof(uint8_t));
        } else {
            return -EINVAL;
        }
    } else if (req_tclas->classifier_type ==
               IEEE80211_WNM_TCLAS_CLASSIFIER_TYPE3)
    {
        uint32_t filter_len;

        tclas = OS_MALLOC(vap->iv_ic->ic_osdev,
                sizeof(ieee80211_tclas_element), GFP_KERNEL);
        if (tclas == NULL) {
            return -ENOMEM;
        }

        tclas->elemid = IEEE80211_ELEMID_TCLAS;
        tclas->len = req_tclas->clas.clas3.filter_len * 2 + 5 * sizeof(uint8_t);
        tclas->up = req_tclas->priority;
        tclas->type = req_tclas->classifier_type;
        tclas->mask = req_tclas->classifier_mask;

        tclas->tclas.type3.filter_offset = req_tclas->clas.clas3.filter_offset;

        filter_len = req_tclas->clas.clas3.filter_len;
        filter_len = (filter_len > TFS_MAX_FILTER_LEN) ? TFS_MAX_FILTER_LEN : filter_len;

        filter_value = OS_MALLOC(vap->iv_ic->ic_osdev, filter_len, GFP_KERNEL);
        if (filter_value == NULL) {
            OS_FREE(tclas);
            return -ENOMEM;
        }
        tclas->tclas.type3.filter_value = filter_value;
        memcpy(filter_value, req_tclas->clas.clas3.filter_value, filter_len);

        filter_mask = OS_MALLOC(vap->iv_ic->ic_osdev, filter_len, GFP_KERNEL);
        if (filter_mask == NULL) {
            OS_FREE(tclas);
            OS_FREE(filter_value);
            return -ENOMEM;
        }
        tclas->tclas.type3.filter_mask = filter_mask;
        memcpy(filter_mask, req_tclas->clas.clas3.filter_mask, filter_len);

        TAILQ_INSERT_TAIL(tclas_head, tclas, tclas_next);

        *subelem_length += tclas->len + (2 * sizeof(uint8_t));
    } else {
        return -EINVAL;
    }

    return 0;
}

int wlan_wnm_set_tfs(wlan_if_t vaphandle, void *data)
{
    struct ieee80211vap      *vap = vaphandle;
    struct ieee80211_node     *ni;
    struct ieee80211_tfs_request *tfs_req;
    struct tfsreq_subelement *req_subelement;
    struct ieee80211_wlanconfig_wnm_tfs_req *req_tfsreq;
    struct tfsreq_tclas_element *req_tclas;
    wnm_netlink_event_t *event;
    TAILQ_HEAD(, ieee80211_tfs_request) tfs_req_head;
    ieee80211_tfs_subelement_req *tsub;
    struct ieee80211_wlanconfig_wnm_tfs *tfs;
    int tfsreqelem_count = 0;
    int subelem_count = 0;
    int tclas_elem_count = 0;
    int total_tfs_length = 0;
    uint32_t subelem_length;
    uint32_t request_length = 0;
    int rv;


    if (!ieee80211_wnm_tfs_is_set(vap->wnm)) 
        return -EFAULT;

    TAILQ_INIT(&tfs_req_head);

    tfs = (struct ieee80211_wlanconfig_wnm_tfs *)data;

    while (tfsreqelem_count < tfs->num_tfsreq) {
        req_tfsreq = &tfs->tfs_req[tfsreqelem_count];

        tfs_req = OS_MALLOC(vap->iv_ic->ic_osdev,
                sizeof(ieee80211_tfs_request), GFP_KERNEL);
        if (tfs_req == NULL) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, "%s:%d OS_MALLOC "
                    "failed\n", __func__, __LINE__);
            wlan_wnm_free_tfs(&tfs_req_head);
            return -ENOMEM;
        }

        tfs_req->tfs_id = req_tfsreq->tfsid;
        tfs_req->tfs_action_code = req_tfsreq->actioncode;

        TAILQ_INIT(&tfs_req->tfs_req_sub);

        subelem_count = 0;
        request_length = 0;

        while (subelem_count < req_tfsreq->num_subelements) {
            req_subelement = &req_tfsreq->subelement[subelem_count];

            tsub = OS_MALLOC(vap->iv_ic->ic_osdev,
                    sizeof(ieee80211_tfs_subelement_req), GFP_KERNEL);
            if (tsub == NULL) {
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, "%s:%d OS_MALLOC "
                        "failed\n", __func__, __LINE__);
                wlan_wnm_free_tfs(&tfs_req_head);
                OS_FREE(tfs_req);
                return -ENOMEM;
            }

            tsub->elem_id = IEEE80211_TCLAS_SUB_ELEMID;
            TAILQ_INIT(&tsub->tclas_head);

            subelem_length = 0;
            tclas_elem_count = 0;

            while (tclas_elem_count < req_subelement->num_tclas_elements) {
                req_tclas = &req_subelement->tclas[tclas_elem_count];

                rv = wlan_wnm_set_tclass(vap, req_tclas, &tsub->tclas_head, &subelem_length);
                if (rv != 0) {
                    IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, "%s:%d OS_MALLOC "
                            "failed\n", __func__, __LINE__);
                    wlan_wnm_free_tfs(&tfs_req_head);
                    OS_FREE(tfs_req);
                    OS_FREE(tsub);
                    return -ENOMEM;
                }
                tclas_elem_count++;
            }

            tsub->tclasprocess.elem_id = IEEE80211_ELEMID_TCLAS_PROCESS;
            tsub->tclasprocess.length = 1;
            tsub->tclasprocess.tclas_process = req_subelement->tclas_processing;

            tsub->length = subelem_length + 3;
            TAILQ_INSERT_TAIL(&tfs_req->tfs_req_sub, tsub, tsub_next);
            request_length += tsub->length + (2 * sizeof(uint8_t));
            subelem_count++;
        }
        tfs_req->length = request_length + 2;
        TAILQ_INSERT_TAIL(&tfs_req_head, tfs_req, tfs_req_next);
        tfsreqelem_count++;
    }

    ni = ieee80211_find_node(&vap->iv_ic->ic_sta, vap->iv_bss->ni_bssid);
    if (ni == NULL) {
        wlan_wnm_free_tfs(&tfs_req_head);
        return -EINVAL;
    }

    wlan_wnm_free_tfs(&ni->ni_wnm->tfsreq->tfs_req_head);

    if (tfsreqelem_count) {
        ni->ni_wnm->tfsreq->dialogtoken %= 255;
        TAILQ_CONCAT(&ni->ni_wnm->tfsreq->tfs_req_head, &tfs_req_head, tfs_req_next);
    }

    event = OS_MALLOC(vap->iv_ic->ic_osdev, sizeof(*event), GFP_KERNEL);
    if (!event) {
        wlan_wnm_free_tfs(&tfs_req_head);
        ieee80211_free_node(ni);
        return -ENOMEM;
    }

    total_tfs_length = tfs->num_tfsreq * sizeof(struct ieee80211_wlanconfig_wnm_tfs);
    event->type = 0x11;
    memcpy(event->mac, ni->ni_macaddr, MAC_ADDR_LEN);  
    memcpy(event->event_data, (char *)data, total_tfs_length);  
    event->datalen = total_tfs_length;
    wnm_netlink_send(vap, event);
    OS_FREE(event);

    ieee80211_send_tfs_req(vap, ni);
    ieee80211_free_node(ni);

    return 0;
}

int wlan_wnm_set_fms(wlan_if_t vaphandle, void *data)
{
    struct ieee80211vap      *vap = vaphandle;
    struct ieee80211_node     *ni = vap->iv_bss;
    ieee80211_fms_request_t   *fms_req;
    struct fmsreq_subelement *req_subelement;
    struct ieee80211_wlanconfig_wnm_fms_req *req_fmsreq;
    struct tfsreq_tclas_element *req_tclas;
    wnm_netlink_event_t *event = NULL;
    TAILQ_HEAD(,  ieee80211_fms_request_s) fmsreq_head;
    ieee80211_fmsreq_subele_t *fmssub;
    struct ieee80211_wlanconfig_wnm_fms *fms;
    int fmsreqelem_count = 0;
    int subelem_count = 0;
    int tclas_elem_count = 0;
    int total_fms_length = 0;
    uint32_t subelem_length;
    uint32_t request_length = 0;
    int rv;


    IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, "****** %s: Entering function %d ******\n",
            __func__, __LINE__);
    TAILQ_INIT(&fmsreq_head);

    fms = (struct ieee80211_wlanconfig_wnm_fms *) data;

    while (fmsreqelem_count < fms->num_fmsreq) {
        req_fmsreq = (struct ieee80211_wlanconfig_wnm_fms_req *)
                                &fms->fms_req[fmsreqelem_count];

        fms_req = OS_MALLOC(vap->iv_ic->ic_osdev,
                sizeof(ieee80211_fms_request_t), GFP_KERNEL);
        if (fms_req == NULL) {
            ieee80211_wnm_free_fmsreq(&fmsreq_head);
            printk("%s: %d OS_MALLOC failed!\n", __func__, __LINE__);
            return -ENOMEM;
        }
        fms_req->fms_token = req_fmsreq->fms_token;

        IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, "%s: fms_token %d\n", __func__, fms_req->fms_token);

        TAILQ_INIT(&fms_req->fmssubele_head);

        subelem_count = 0;
        request_length = 0;

        while (subelem_count < req_fmsreq->num_subelements) {
            req_subelement = (struct fmsreq_subelement *)
                                &req_fmsreq->subelement[subelem_count];

            fmssub = OS_MALLOC(vap->iv_ic->ic_osdev,
                    sizeof(ieee80211_fmsreq_subele_t), GFP_KERNEL);
            if (fmssub == NULL) {
                ieee80211_wnm_free_fmsreq(&fmsreq_head);
                OS_FREE(fms_req);
                printk("%s: %d OS_MALLOC failed!\n", __func__, __LINE__);
                return -ENOMEM;
            }

            fmssub->elem_id = IEEE80211_FMS_SUBELEMENT_ID;
            fmssub->del_itvl = req_subelement->del_itvl;
            fmssub->max_del_itvl = req_subelement->max_del_itvl;
            memcpy(&fmssub->rate_id, &req_subelement->rate_id,
                    sizeof(ieee80211_fms_rate_identifier_t));
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, "%s: del_itvl %d "
                    "max_del_itvl %d rate(%x.%x.%x)\n", __func__,
                    fmssub->del_itvl, fmssub->max_del_itvl,
                    fmssub->rate_id.mask,fmssub->rate_id.mcs_idx,
                    fmssub->rate_id.rate);

            subelem_length = 6;
            tclas_elem_count = 0;

            while (tclas_elem_count < req_subelement->num_tclas_elements) {
                req_tclas = &req_subelement->tclas[tclas_elem_count];

                TAILQ_INIT(&fmssub->tclas_head);
                rv = wlan_wnm_set_tclass(vap, req_tclas, &fmssub->tclas_head, &subelem_length);
                if (rv != 0) {
                    wlan_wnm_free_tfs(&fmsreq_head);
                    OS_FREE(fms_req);
                    OS_FREE(fmssub);
                    printk("%s: wlan_wnm_set_tclass failed!\n", __func__);
                    return -ENOMEM;
                }
                tclas_elem_count++;
            }

            fmssub->tclasprocess.elem_id = IEEE80211_ELEMID_TCLAS_PROCESS;
            fmssub->tclasprocess.length = 1;
            fmssub->tclasprocess.tclas_process = req_subelement->tclas_processing;

            fmssub->len = subelem_length + 3;
            TAILQ_INSERT_TAIL(&fms_req->fmssubele_head, fmssub, fmssubele_next);
            request_length += fmssub->len + (2 * sizeof(uint8_t));
            subelem_count++;
        }
        fms_req->len = request_length + 1;
        TAILQ_INSERT_TAIL(&fmsreq_head, fms_req, fmsreq_next);
        fmsreqelem_count++;
    }

    ni = vap->iv_bss;
    ieee80211_wnm_free_fmsreq(&ni->ni_wnm->fmsreq->fmsreq_head);
    ni->ni_wnm->fmsreq->dialog_token %= 255;
    if(TAILQ_FIRST(&ni->ni_wnm->fmsreq->fmsreq_head) == NULL) {
        TAILQ_CONCAT(&ni->ni_wnm->fmsreq->fmsreq_head, &fmsreq_head, fmsreq_next);
    }

    ni = ieee80211_vap_find_node(vap, vap->iv_bss->ni_bssid);
    if (ni == NULL) {
        printk("%s : %d ieee80211_vap_find_node failed !\n", __func__, __LINE__);
        return -EINVAL;
    }
    event = OS_MALLOC(vap->iv_ic->ic_osdev, sizeof(*event), GFP_KERNEL);

    if (!event) {
        ieee80211_free_node(ni);
        return -ENOMEM;
    }

    total_fms_length = fms->num_fmsreq * sizeof(struct ieee80211_wlanconfig_wnm_fms);
    event->type = 0x14;
    memcpy(event->mac, ni->ni_macaddr, MAC_ADDR_LEN);
    memcpy(event->event_data, (char *) data, total_fms_length);
    event->datalen = total_fms_length;
    wnm_netlink_send(vap, event);
    OS_FREE(event);

    ieee80211_send_fms_req(vap, ni);
    ieee80211_free_node(ni);

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, "****** %s: Exiting function %d ******\n",
                      __func__, __LINE__);
    return 0;
}

int wlan_wnm_set_timbcast(wlan_if_t vaphandle, void *data)
{
    struct ieee80211vap      *vap = vaphandle;
    struct ieee80211_node     *ni = vap->iv_bss;
    struct ieee80211_wlanconfig_wnm_tim *tim;

    if (!ieee80211_wnm_tim_is_set(vap->wnm))
        return -EFAULT;

    tim = (struct ieee80211_wlanconfig_wnm_tim *)data;
    vap->wnm->wnm_timbcast_enable = (!!tim->enable_highrate) |
                            (!!tim->enable_lowrate << 1);

    if (vap->iv_opmode != IEEE80211_M_STA) {
        return 0;
    }

    vap->wnm->wnm_timbcast_interval = tim->interval;

    ni = ieee80211_vap_find_node(vap, vap->iv_bss->ni_bssid);
    if (ni == NULL) {
        return 0;
    }

    ieee80211_send_tim_req(vap, ni);
    ieee80211_free_node(ni);

    return 0;
}

int wlan_wnm_get_timbcast(wlan_if_t vaphandle, void *data)
{
    struct ieee80211vap      *vap = vaphandle;
    struct ieee80211_wlanconfig_wnm_tim *tim;

    if (!ieee80211_wnm_tim_is_set(vap->wnm))
        return -EFAULT;

    tim = (struct ieee80211_wlanconfig_wnm_tim *) data;
    tim->interval = vap->wnm->wnm_timbcast_interval;
    tim->enable_highrate = vap->wnm->wnm_timbcast_enable &
                                IEEE80211_WNM_TIM_HIGHRATE_ENABLE;
    tim->enable_lowrate = !!(vap->wnm->wnm_timbcast_enable &
                                IEEE80211_WNM_TIM_LOWRATE_ENABLE);

    return 0;
}

/*
 * Allocate a TIM frame and fillin the appropriate bits.
 */
wbuf_t
ieee80211_timbcast_alloc(struct ieee80211_node *ni)
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211com *ic = ni->ni_ic;
    wbuf_t wbuf;
    struct ieee80211_frame *wh;
    struct ieee80211_ath_tim_ie *tie;
    u_int8_t *frm;

    wbuf = wbuf_alloc(ic->ic_osdev, WBUF_TX_BEACON, MAX_TX_RX_PACKET_SIZE);
    if (wbuf == NULL)
        return NULL;

    wh = (struct ieee80211_frame *)wbuf_header(wbuf);
    wh->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_MGT |
        IEEE80211_FC0_SUBTYPE_ACTION;
    wh->i_fc[1] = IEEE80211_FC1_DIR_NODS;
    *(u_int16_t *)wh->i_dur = 0;
    IEEE80211_ADDR_COPY(wh->i_addr1, IEEE80211_GET_BCAST_ADDR(ic));
    IEEE80211_ADDR_COPY(wh->i_addr2, vap->iv_myaddr);
    IEEE80211_ADDR_COPY(wh->i_addr3, ni->ni_bssid);
    *(u_int16_t *)wh->i_seq = 0;

    frm = (u_int8_t *)&wh[1];
    *frm++ = IEEE80211_ACTION_CAT_UNPROTECTED_WNM;
    *frm++ = IEEE80211_ACTION_TIM_FRAME;
    frm++;
    OS_MEMZERO(frm, 8);
    frm += 8;
    tie = (struct ieee80211_ath_tim_ie *) frm;
    tie->tim_ie = IEEE80211_ELEMID_TIM;
    tie->tim_len = 4;    /* length */
    tie->tim_count = 0;    /* DTIM count */
    tie->tim_period = vap->vdev_mlme->proto.generic.dtim_period;    /* DTIM period */
    tie->tim_bitctl = 0;    /* bitmap control */
    tie->tim_bitmap[0] = 0;    /* Partial Virtual Bitmap */
    frm += sizeof(struct ieee80211_ath_tim_ie);

    wbuf_set_pktlen(wbuf, (frm - (u_int8_t *) wbuf_header(wbuf)));

    if (!(ni = ieee80211_try_ref_node(ni))) {
        wbuf_release(ic->ic_osdev, wbuf);
        return NULL;
    }
    wlan_wbuf_set_peer_node(wbuf, ni);

    IEEE80211_NOTE_MAC(vap, IEEE80211_MSG_MLME, vap->iv_myaddr,
                       "%s \n", __func__);
    return wbuf;
}

int
ieee80211_timbcast_update(struct ieee80211_node *ni,
                          struct ieee80211_beacon_offsets *bo, wbuf_t wbuf)
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211_frame *wh = (struct ieee80211_frame *)wbuf_header(wbuf);
    struct ieee80211_ath_tim_ie *tie;
    u_int8_t *frm;

    if ((ni->ni_txseqs[IEEE80211_NON_QOS_SEQ]& IEEE80211_SEQ_MASK) < MIN_SW_SEQ) {
        ni->ni_txseqs[IEEE80211_NON_QOS_SEQ] = MIN_SW_SEQ;
    }
    *(u_int16_t *)&wh->i_seq[0] =
        htole16(ni->ni_txseqs[IEEE80211_NON_QOS_SEQ] << IEEE80211_SEQ_SEQ_SHIFT);

    ni->ni_txseqs[IEEE80211_NON_QOS_SEQ]++;
    frm = (u_int8_t *)&wh[1];
    *frm++ = IEEE80211_ACTION_CAT_UNPROTECTED_WNM;
    *frm++ = IEEE80211_ACTION_TIM_FRAME;
    *frm++ = vap->wnm->wnm_check_beacon;
    OS_MEMZERO(frm, 8);
    frm += 8;
    tie = (struct ieee80211_ath_tim_ie *) frm;
    OS_MEMCPY(tie, bo->bo_tim, 5 + bo->bo_tim_len);
    frm += 5 + bo->bo_tim_len;

    wbuf_set_pktlen(wbuf, (frm - (u_int8_t *) wbuf_header(wbuf)));

    IEEE80211_NOTE_MAC(vap, IEEE80211_MSG_MLME, vap->iv_myaddr,
                       "%s \n", __func__);
    return 0;
}

int ieee80211_wnm_timbcast_cansend(wlan_if_t vap)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211_node_table *nt;
    struct ieee80211_node *ni = NULL, *next = NULL;
    rwlock_state_t lock_state;
    u_int8_t match = 0;

    if (ieee80211_vap_wnm_is_set(vap) == 0) {
        return -1;
    }

    if (vap->wnm->wnm_timbcast_interval == 0) {
        return -1;
    }
    nt = &ic->ic_sta;
    OS_RWLOCK_READ_LOCK(&nt->nt_nodelock, &lock_state);
    TAILQ_FOREACH_SAFE(ni, &nt->nt_node, ni_list, next) {
        if (vap == ni->ni_vap) {
            if (ni->ni_wnm->timbcast_status == IEEE80211_WNM_TIMREQUEST_ACCEPT) {
                u_int8_t sleep;
                sleep = (ic->ic_is_mode_offload(ic) ?
                         ni->ps_state : (ni->ni_flags & IEEE80211_NODE_PWR_MGT));
                if (sleep) {
                    match = 1;
                    break;
                }

            }
        }
    }
    OS_RWLOCK_READ_UNLOCK(&nt->nt_nodelock, &lock_state);

    vap->wnm->wnm_timbcast_counter++;
    if (vap->wnm->wnm_timbcast_interval == vap->wnm->wnm_timbcast_counter) {
        vap->wnm->wnm_timbcast_counter = 0;
        return match;
    } else {
        return 0;
    }
}

int ieee80211_wnm_timbcast_enabled(wlan_if_t vap)
{
    return ieee80211_wnm_tim_is_set(vap->wnm);
}

int ieee80211_timbcast_get_highrate(wlan_if_t vap)
{
    return vap->wnm->wnm_timbcast_highrate;
}

int ieee80211_timbcast_get_lowrate(wlan_if_t vap)
{
    return vap->wnm->wnm_timbcast_lowrate;
}

int ieee80211_timbcast_lowrateenable(wlan_if_t vap)
{
    if (vap->wnm->wnm_timbcast_enable & IEEE80211_WNM_TIM_LOWRATE_ENABLE) {
        return 1;
    } else {
        return 0;
    }
}

int ieee80211_timbcast_highrateenable(wlan_if_t vap)
{
    if (vap->wnm->wnm_timbcast_enable & IEEE80211_WNM_TIM_HIGHRATE_ENABLE) {
        return 1;
    } else {
        return 0;
    }
}

int ieee80211_wnm_tim_incr_checkbeacon(wlan_if_t vap)
{
    vap->wnm->wnm_check_beacon++;
    return vap->wnm->wnm_check_beacon;
}

/* Ask app to send WNM-Sleep Request frame with action=enter/exit */
int ieee80211_wnm_sleepreq_to_app(wlan_if_t vap, u_int8_t action, u_int16_t intval)
{
    static const char *tag = "MLME-SLEEPMODE.request";
    osif_dev  *osifp = (osif_dev *)vap->iv_ifp;
    struct net_device *dev = osifp->netdev;
    union iwreq_data wrqu;
    char buf[128];

    if (action == IEEE80211_WNMSLEEP_ACTION_ENTER)
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_POWER, "Ask app to send "
                "WNM-Sleep Request frame with action=enter\n");
    else if (action == IEEE80211_WNMSLEEP_ACTION_EXIT)
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_POWER, "Ask app to send "
                "WNM-Sleep Request frame with action=exit\n");
    else {
        printk("Not supported WNM-Sleep action %d\n", action);
        return -1;
    }

    /* Parameters: action, sleep intval */
    snprintf(buf, sizeof(buf), "%s(action=%d intval=%d)", tag, action, intval);
    memset(&wrqu, 0, sizeof(wrqu));
    wrqu.data.length = strlen(buf);
    WIRELESS_SEND_EVENT(dev, IWEVCUSTOM, &wrqu, buf);

    return 0;
}

/** ieee80211_wnm_set_appie - to process WNM command from applicantion
 *  buf - contains WNM message Peer-MAC, WNM_TYPE and
 *  APPIE if needed
 *  len - length of the buffer
 */     
int ieee80211_wnm_set_appie(wlan_if_t vap, u8 *buf, int len)
{
    struct ieee80211_node *ni;
    u16 type, appie_len;
    u8 *mac, *pos;
    int err = 0;
    struct ieee80211com *ic;

    if (len < ETH_ALEN + 4)
        return -EINVAL;

    pos = buf;
    mac = pos;
    pos += ETH_ALEN;
    type = *(u16 *) pos;
    pos += 2;
    appie_len = *(u16 *) pos;
    pos += 2;

    if (len < ETH_ALEN + 4 + appie_len)
        return -EINVAL;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "WNM_CMD: PEER-MAC:%s "
            "for len:%d, type:%d, vap:%pK\n",
            ether_sprintf(mac), appie_len, type, vap);

    ni = ieee80211_vap_find_node(vap, mac);
    if (!ni)
        return -EINVAL;

    switch (type) {
    case IEEE80211_WNM_SLEEP_ENTER_CONFIRM:
        if (vap->iv_opmode == IEEE80211_M_STA) {
            /* deliver WNMSLEEP_RESP event to ps sta */
            ieee80211_vap_txrx_event evt;
            evt.type = IEEE80211_VAP_INPUT_EVENT_WNMSLEEP_RESP;
            evt.peer = ni->peer_obj;
            evt.u.status = 1;   /* success */
            wlan_vdev_txrx_deliver_event(vap->vdev_obj,&evt);
        } else if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
            /* mark the node as ps node */
            ic = ni->ni_ic;
            if (!ic->ic_is_mode_offload(ic)) {
                if ((ni != vap->iv_bss) && !(ni->ni_flags & IEEE80211_NODE_PWR_MGT))
                    ieee80211_mlme_node_pwrsave(ni, 1); 
            } else {
                if (ni != vap->iv_bss)
                    ieee80211_mlme_node_pwrsave(ni, 1); 
            }
        }
        break;
    case IEEE80211_WNM_SLEEP_ENTER_FAIL:
        if (vap->iv_opmode == IEEE80211_M_STA) {
            /* deliver WNMSLEEP_RESP event to ps sta */
            ieee80211_vap_txrx_event evt;
            evt.type = IEEE80211_VAP_INPUT_EVENT_WNMSLEEP_RESP;
            evt.peer = ni->peer_obj;
            evt.u.status = 0;   /* fail */
            wlan_vdev_txrx_deliver_event(vap->vdev_obj,&evt);
        }
        break;
    case IEEE80211_WNM_SLEEP_EXIT_CONFIRM:
        if (vap->iv_opmode == IEEE80211_M_STA) {
            /* deliver WNMSLEEP_RESP event to ps sta */
            ieee80211_vap_txrx_event evt;
            evt.type = IEEE80211_VAP_INPUT_EVENT_WNMSLEEP_RESP;
            evt.peer = ni->peer_obj;
            evt.u.status = 1;   /* success */
            wlan_vdev_txrx_deliver_event(vap->vdev_obj,&evt);
        } else if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
            /* mark the node as non-ps node */
            ic = ni->ni_ic;
            if (!ic->ic_is_mode_offload(ic)) {
                if ((ni != vap->iv_bss) && (ni->ni_flags & IEEE80211_NODE_PWR_MGT))
                    ieee80211_mlme_node_pwrsave(ni, 0); 
            } else {
                if (ni != vap->iv_bss)
                    ieee80211_mlme_node_pwrsave(ni, 0); 
            }
        }
        break;
    case IEEE80211_WNM_SLEEP_EXIT_FAIL:
        if (vap->iv_opmode == IEEE80211_M_STA) {
            /* deliver WNMSLEEP_RESP event to ps sta */
            ieee80211_vap_txrx_event evt;
            evt.type = IEEE80211_VAP_INPUT_EVENT_WNMSLEEP_RESP;
            evt.peer = ni->peer_obj;
            evt.u.status = 0;   /* fail */
            wlan_vdev_txrx_deliver_event(vap->vdev_obj,&evt);
        }
        break;
    case IEEE80211_WNM_SLEEP_TFS_REQ_IE_SET:
        if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
            u8 *frm = pos;
            if (ni->ni_wnm)
                frm = ieee80211_add_tfs_req(vap, ni, frm, len);

            if(!frm)
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "Set tfs req ie error");
        }
        break;
    case IEEE80211_WNM_SLEEP_TFS_RESP_IE_SET:
        if (vap->iv_opmode == IEEE80211_M_STA) {
            u8 *frm = pos;
            int  resp_count = 0;
            if (ni->ni_wnm)
                resp_count = ieee80211_add_tfs_resp(vap, ni, frm, len);

            if (resp_count < 0)
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "Set tfs resp ie error");
        }
        break;
    case IEEE80211_WNM_SLEEP_TFS_IE_DEL:
        if (vap->iv_opmode == IEEE80211_M_STA) {
            if (ni->ni_wnm) {
                wlan_wnm_free_tfs(&ni->ni_wnm->tfsreq->tfs_req_head);
                wlan_wnm_free_tfsrsp(&ni->ni_wnm->tfsrsp->tfs_rsp_head);
            }
        }
        break;
    default:
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
                  "Unsupported WNM command type %u",
                  type);
        err = -EOPNOTSUPP;
        break;
    }

    if (err) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
                  "%s: type:%d error:%d\n",
                  __func__, type, err);
    }

    ieee80211_free_node(ni);

    return err;
}

int ieee80211_wnm_get_appie(wlan_if_t vap, u_int8_t *buf,
                            u_int32_t *ielen, u_int32_t buflen)
{
    struct ieee80211_node *ni;
    struct ieee80211com *ic;
    u16 type, appie_len;
    u8 *mac, *pos, *appie_buf;
    int err = 0;

    if (buflen < ETH_ALEN + 4)
        return -EINVAL;

    pos = buf;
    mac = pos;
    pos += ETH_ALEN;
    type = *(u16 *) pos;
    pos += 2;
    appie_len = *(u16 *) pos;
    pos += 2;

    if (buflen < ETH_ALEN + 4 + appie_len)
        return -EINVAL;

    ni = ieee80211_vap_find_node(vap, mac);
    if (!ni)
        return -EINVAL;

    switch (type) {
    case IEEE80211_WNM_SLEEP_TFS_REQ_IE_ADD:
        /* STA mode only */
        if (vap->iv_opmode != IEEE80211_M_STA) {
            err = -EINVAL;
            goto exit;
        }
        ic = ni->ni_ic;
        appie_buf = OS_MALLOC(ic->ic_osdev, IEEE80211_APPIE_MAX, 0);

        if(appie_buf == NULL) {
            OS_FREE(appie_buf);
            err = -EINVAL;
            goto exit;
        }

        pos = appie_buf;
        pos = ieee80211_add_tfsreq_ie(vap, ni, pos);
        if (!pos) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
                    "Add tfsreq ie error");
            *ielen = 0;
            OS_FREE(appie_buf);
            err = -EINVAL;
            goto exit;
        }
        if (pos - appie_buf > buflen) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
                    "Out of buffer %d > %d",
                    pos-appie_buf, buflen);
            *ielen = 0;
            OS_FREE(appie_buf);
            err = -EINVAL;
            goto exit;
        }

        *ielen = pos - appie_buf;
        OS_MEMCPY(buf, appie_buf, *ielen);
        OS_FREE(appie_buf);
        break;
    case IEEE80211_WNM_SLEEP_TFS_RESP_IE_ADD:
        /* AP mode only */
        if (vap->iv_opmode != IEEE80211_M_HOSTAP) {
            err = -EINVAL;
            goto exit;
        }
        ic = ni->ni_ic;
        appie_buf = OS_MALLOC(ic->ic_osdev, IEEE80211_APPIE_MAX, 0);

        if(appie_buf == NULL) {
            OS_FREE(appie_buf);
            err = -EINVAL;
            goto exit;
        }

        pos = appie_buf;
        pos = ieee80211_add_tfsrsp_ie(vap, ni, pos);
        if (!pos) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
                    "Add tfsrsp ie error");
            *ielen = 0;
            OS_FREE(appie_buf);
            err = -EINVAL;
            goto exit;
        }
        if (pos - appie_buf > buflen) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
                    "Out of buffer %d > %d",
                    pos-appie_buf, buflen);
            *ielen = 0;
            OS_FREE(appie_buf);
            err = -EINVAL;
            goto exit;
        }

        *ielen = pos - appie_buf;
        OS_MEMCPY(buf, appie_buf, *ielen);
        OS_FREE(appie_buf);
        break;
    default:
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
                  "Unsupported WNM command type %u",
                  type);
        err = -EOPNOTSUPP;
        break;
    }

    if (err) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
                  "%s: type:%d error:%d\n",
                  __func__, type, err);
    }

exit:
    ieee80211_free_node(ni);

    return err;
}

int ieee80211_wnm_forward_action_app(wlan_if_t vap, wlan_node_t ni, wbuf_t wbuf,
                                int subtype, struct ieee80211_rx_status *rs)
{
    wlan_p2p_event p2p_event;
    wlan_chan_t channel = NULL;
    u_int32_t freq = 0;
    struct ieee80211_frame *wh = (struct ieee80211_frame *) wbuf_header(wbuf);
    u_int8_t *ie_data;
    u_int16_t ie_len;
    union iwreq_data wreq;
    osif_dev *osifp = NULL;

    if (!vap || !ni)
        return -1;

    if(ieee80211_vap_wnm_is_set(vap) == 0) {
        return -1;
    }

#if UMAC_SUPPORT_CFG80211
    /* Driver is running with cfg80211 interface */
    if (vap->iv_cfg80211_create) {
        if (vap->iv_evtable && vap->iv_evtable->wlan_receive_filter_80211) {
            vap->iv_evtable->wlan_receive_filter_80211(vap->iv_ifp, wbuf,
                                           IEEE80211_FC0_TYPE_MGT, subtype, rs);
        }
    } else {
#endif /* UMAC_SUPPORT_CFG80211 */
    osifp = (osif_dev *)vap->iv_ifp;

    channel = wlan_node_get_chan(ni);
    freq = wlan_channel_frequency(channel);

#ifndef MAC2STR
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#endif

    memset(&wreq, 0, sizeof(wreq));

    ie_data = ieee80211_mgmt_iedata(wbuf,subtype);
    ie_len = wbuf_get_pktlen(wbuf) - (ie_data - (u_int8_t *)wh);

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s ACTION DA[" MACSTR "] SA[" MACSTR "] "
                          "seq[%04x] freq[%4u]\n", __func__, MAC2STR(wh->i_addr1),
                          MAC2STR(wh->i_addr2), le16toh(*(u_int16_t *)wh->i_seq), freq);

    p2p_event.type = WLAN_P2PDEV_RX_FRAME;
    p2p_event.u.rx_frame.frame_type = subtype;
    p2p_event.u.rx_frame.frame_len = wbuf_get_pktlen(wbuf);
    p2p_event.u.rx_frame.frame_buf = wbuf_header(wbuf);
    p2p_event.u.rx_frame.ie_len = ie_len;
    p2p_event.u.rx_frame.ie_buf = ie_data;
    p2p_event.u.rx_frame.src_addr = wh->i_addr2;
    p2p_event.u.rx_frame.frame_rssi = rs->rs_rssi;
    p2p_event.u.rx_frame.freq = freq;
    p2p_event.u.rx_frame.chan_flags = wlan_channel_flags(channel);
    p2p_event.u.rx_frame.wbuf = wbuf;

    wreq.data.flags = IEEE80211_EV_RX_MGMT;
    osif_p2p_rx_frame_handler(osifp, &p2p_event, IEEE80211_EV_RX_MGMT);
    WIRELESS_SEND_EVENT(osifp->netdev, IWEVCUSTOM, &wreq, NULL);
#if UMAC_SUPPORT_CFG80211
    }
#endif /* UMAC_SUPPORT_CFG80211 */

    return 0;
}
int ieee80211_wnm_fms_enabled(wlan_if_t vap)
{
    return (ieee80211_vap_wnm_is_set(vap) && ieee80211_wnm_fms_is_set(vap->wnm));
}

int wlan_wnm_set_bssterm(wlan_if_t vaphandle, void *data)
{
    struct ieee80211vap *vap = vaphandle;
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(vap->iv_ic);
    struct ieee80211_node_table *nt = &vap->iv_ic->ic_sta;
    struct ieee80211_wlanconfig_wnm_bssterm *bssterm;
    struct ieee80211_bstm_reqinfo bstmreq;
    struct ieee80211_node *ni, *next;
    rwlock_state_t lock_state;
    u_int64_t tsf;

    if (!ieee80211_vap_wnm_is_set(vap))
        return -EFAULT;

    if (vap->iv_opmode != IEEE80211_M_HOSTAP)
        return 0;

    bssterm = (struct ieee80211_wlanconfig_wnm_bssterm *)data;
    tsf = scn->sc_ops->ath_get_tsf64(scn->sc_dev);
    if (!bssterm->delay) {
        vap->wnm->wnm_bssterm_tsf = 0;
        vap->wnm->wnm_bssterm_duration = 0;
        return 0;
    }
    vap->wnm->wnm_bssterm_tsf = tsf +
            bssterm->delay * vap->iv_bss->ni_intval * TIME_UNIT_TO_MICROSEC;
    vap->wnm->wnm_bssterm_duration = bssterm->duration;

    memset(&bstmreq, 0, sizeof(bstmreq));
    bstmreq.dialogtoken = 1;
    bstmreq.bssterm_inc = 1;

    /* Send BSS Transition Management Request frame to all vap STA nodes */
    OS_RWLOCK_READ_LOCK(&nt->nt_nodelock, &lock_state);
    TAILQ_FOREACH_SAFE(ni, &nt->nt_node, ni_list, next) {
        if (ni->ni_vap == vap && ni->ni_associd &&
            !IEEE80211_ADDR_EQ(ni->ni_macaddr, vap->iv_myaddr))
        {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_WNM, "Send BSTM Request to "
                    "%s. BSS will terminate at %llu (current TSF %llu)\n",
                    ether_sprintf(ni->ni_macaddr),
                    vap->wnm->wnm_bssterm_tsf, tsf);
            ieee80211_send_bstm_req(vap, ni, &bstmreq);
        }
    }
    OS_RWLOCK_READ_UNLOCK(&nt->nt_nodelock, &lock_state);

    return 0;
}

void wlan_btm_sta_info(void *arg, wlan_node_t node)
{
    ieee80211_sta_info_t *sta_info = (ieee80211_sta_info_t *)arg;

    if (sta_info->count < sta_info->max_sta_cnt) {
        if (node->ext_caps.ni_ext_capabilities & IEEE80211_EXTCAPIE_BSSTRANSITION) {
            IEEE80211_ADDR_COPY((sta_info->dest_addr + (sta_info->count * IEEE80211_ADDR_LEN)),node->ni_macaddr);
            sta_info->count++;
        }
    }
}

/**
* @brief finds the number of btm stas
*
* @param vap    pointer to corresponding vap
*
* @return btm capable sta counts
*/
int wlan_get_btm_sta_count(wlan_if_t vap)
{
    struct ieee80211_node *ni  = NULL;
    struct ieee80211_node_table *nt = &vap->iv_ic->ic_sta;
    int count = 0;

    TAILQ_FOREACH(ni, &nt->nt_node, ni_list) {
      if (ni->ext_caps.ni_ext_capabilities & IEEE80211_EXTCAPIE_BSSTRANSITION)
          count++;
    }

    return count;
}

/**
* @brief to send the MAC address of the BTM capable STA to the upper layers
*
* @param vaphandle
* @param max_sta_count: number of macs buff can hold at a time
* @param buff   : pointer to memory where Mac address will be copied
*
* @return btm sta count
*/
int wlan_get_btm_sta_list(wlan_if_t vap, uint32_t max_sta_count, uint8_t *buff)
{
    ieee80211_sta_info_t dest_stats;

    dest_stats.dest_addr = (u_int8_t *)buff;
    dest_stats.max_sta_cnt = max_sta_count;
    dest_stats.count = 0;
    wlan_iterate_station_list(vap, wlan_btm_sta_info, &dest_stats);

    return dest_stats.count;
}


#endif /* UMAC_SUPPORT_WNM */
