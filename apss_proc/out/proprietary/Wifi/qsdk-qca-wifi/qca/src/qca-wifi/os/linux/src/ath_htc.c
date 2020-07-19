/*
 * Copyright (c) 2013, 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */
/*
 * Copyright (c) 2010, Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/*
 * 2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include <osdep.h>

#include "if_athvar.h"
#include "htc_host_api.h"
#include "wmi.h"
#include "wmi_host_api.h"
#include "ath_internal.h"
#include "miscdrv.h"
#include "htc_ieee_common.h"
#include <if_ath_htc.h>

extern void host_htc_eptx_comp(void *context, wbuf_t skb, HTC_ENDPOINT_ID epid);
extern void host_htc_eprx_comp(void *context, wbuf_t skb, HTC_ENDPOINT_ID epid);
extern void ath_bmiss_event(void *Context, void *data, u_int32_t datalen);
extern void ath_swba_event(void *Context, void *data, u_int32_t datalen);
extern void ath_action_tx_event(void *Context, int8_t tx_status);
extern void ieee80211_vap_txrx_deliver_event(ieee80211_vap_t vap,ieee80211_vap_txrx_event *evt);
extern void ath_gen_timer_event(void *Context, void *data);

void MpWmiStop(osdev_t osdev);

void ath_null_send_tx_event(void *Context, void *data)
{
    struct ath_softc_net80211   *scn = (struct ath_softc_net80211 *)Context;
    struct ieee80211com         *ic = &scn->sc_ic;
    struct ieee80211_node_table *nt = &ic->ic_sta;
    struct ieee80211_node 		*ni;
	struct ieee80211vap 		*vap;
	ieee80211_vap_txrx_event    evt;
    u_int8_t addr[IEEE80211_ADDR_LEN];
    
    if(!data)
        return;
    IEEE80211_ADDR_COPY(addr, data);		
    OS_FREE(data);        
    ni = ieee80211_find_node(nt,addr);
    
    if (ni == NULL)
        return;
        		
    vap = ni->ni_vap;		
    		
    /*
     * update power management module about tx completion
     */
    evt.type = IEEE80211_VAP_OUTPUT_EVENT_COMPLETE_PS_NULL;
    evt.u.status = 0;
    ieee80211_vap_txrx_deliver_event(vap,&evt);
    
    /* free node reference count */
    ieee80211_free_node(ni);    
}

static
void dispatch_magpie_sys_evts(void *pContext, WMI_EVENT_ID evt_id, u_int8_t *pBuffer, u_int32_t buf_len)
{   
    struct ath_softc_net80211 *scn = (struct ath_softc_net80211 *)pContext;
    u_int8_t *ps_null_addr;

    switch(evt_id) {
        case WMI_TGT_RDY_EVENTID:
            break;
        case WMI_SWBA_EVENTID:
            ath_swba_event(scn, pBuffer, buf_len);
            break;
            break;
        case WMI_FATAL_EVENTID:
            break;
        case WMI_TXTO_EVENTID:
            break;
        case WMI_BMISS_EVENTID:
            printk("WMI_BMISS_EVENTID\n");
            ath_bmiss_event(scn, pBuffer, buf_len);       
            break;
        case WMI_WLAN_TXCOMP_EVENTID:
            /* Event for NULL data and pwrMgt bit = 1 TX Complete */
            ps_null_addr = OS_MALLOC(scn->sc_osdev, IEEE80211_ADDR_LEN, GFP_ATOMIC);
            if(ps_null_addr) {
                IEEE80211_ADDR_COPY(ps_null_addr, pBuffer);
                printk(KERN_DEBUG "WMI_WLAN_TXCOMP_EVENTID: %02x %02x %02x %02x %02x %02x\n",
                    ps_null_addr[0],ps_null_addr[1],ps_null_addr[2],
                    ps_null_addr[3],ps_null_addr[4],ps_null_addr[5]);

                ath_put_defer_item(scn->sc_osdev, (void *)ath_null_send_tx_event, WORK_ITEM_SET_PS_DELIVER_EVENT, 
                        scn, ps_null_addr, NULL);
            } else {
                printk("##dbg [%s] fail to allocate buffer, line %d\n",__func__,__LINE__);
            }
            break;
        case WMI_DELBA_EVENTID:
            break;
        case WMI_TXRATE_EVENTID:
            scn->sc_htc_txRateKbps = be32_to_cpu(((HTC_HOST_TGT_RATE_INFO *)pBuffer)->txRateKbps);
            break;
        case WMI_ACTION_TXCOMP_EVENTID:
            /* Event for Action frame TX Complete */
            //printk("WMI_ACTION_TXCOMP_EVENTID\n");
            ath_action_tx_event(scn, A_OK);
            break;
        case WMI_ACTION_TXFAIL_EVENTID:
            /* Event for Action frame TX Failure */
            printk("WMI_ACTION_TXFAIL_EVENTID\n");
            ath_action_tx_event(scn, A_ERROR);
            break;
        case WMI_GENTIMER_EVENTID:
            {
                WMI_GENTIMER_EVENT *gentimerEvt;
                
                gentimerEvt = (WMI_GENTIMER_EVENT *)OS_MALLOC(scn->sc_osdev, sizeof(WMI_GENTIMER_EVENT), GFP_ATOMIC);
                if (gentimerEvt) {
                    //printk("WMI_GENTIMER_EVENTID\n");
                    gentimerEvt->trigger_mask = be32_to_cpu(((WMI_GENTIMER_EVENT *)pBuffer)->trigger_mask);
                    gentimerEvt->thresh_mask = be32_to_cpu(((WMI_GENTIMER_EVENT *)pBuffer)->thresh_mask);
                    gentimerEvt->curr_tsf = be32_to_cpu(((WMI_GENTIMER_EVENT *)pBuffer)->curr_tsf);

                    if (ath_put_defer_item(scn->sc_osdev, (void *)ath_gen_timer_event,  WORK_ITEM_SET_TIMER_DELIVER_EVENT,
                                           scn, gentimerEvt, NULL)){
                        OS_FREE(gentimerEvt);
                    }
                } else {
                    printk("##dbg [%s] fail to allocate buffer, line %d\n",__func__,__LINE__);
                }
            }        
            break;
        case WMI_NODEKICK_EVENTID:
            {   
                u_int32_t node_index;
                node_index = *((uint32_t *)pBuffer);
                node_index = ntohl(node_index);
                ath_node_kick_event(scn, node_index);
            }
            break;     
    }
}

A_STATUS 
host_htc_rdy_handler(osdev_t osdev, u_int32_t TotalCredits)
{
    A_STATUS status;
    HTC_SERVICE_CONNECT_REQ connect;
    struct ath_softc_net80211 *scn = (struct ath_softc_net80211 *)ath_netdev_priv(osdev->netdev);
#ifdef HTC_HOST_CREDIT_DIST
    int   credit_state_info ; 
#endif    
    if (HTCWaitForHtcRdy(osdev->host_htc_handle) != A_OK) {
        printk("HTCWaitForHtcRdy failed, init failed!\n");
        return A_ERROR;
    }

    /* connect to WMI service */
    status = wmi_connect(osdev->host_htc_handle, osdev->host_wmi_handle, (HTC_ENDPOINT_ID*)&(osdev->wmi_command_ep));
    if (A_FAILED(status)) {
        printk("wmi connect failed!");
        return A_ERROR;
    }

    //qdf_mem_zero(&connect, sizeof(HTC_SERVICE_CONNECT_REQ));
    memset(&connect, 0, sizeof(HTC_SERVICE_CONNECT_REQ));
    connect.EpCallbacks.pContext = scn;//osdev;
    //connect.EpCallbacks.EpTxComplete = host_htc_eptx_comp;
    connect.EpCallbacks.EpRecv = host_htc_eprx_comp;

    connect.ServiceID = WMI_BEACON_SVC;
    connect.EpCallbacks.EpTxComplete = NULL;//host_htc_eptx_comp;
    //connect.UL_PipeID = 1;
    //connect.DL_PipeID = 1;
    status = HTCConnectService(osdev->host_htc_handle, &connect, (HTC_ENDPOINT_ID*)&(osdev->beacon_ep));
    if (A_FAILED(status)) {
        printk("Failed to connect services!");
        return A_ERROR;
    }

    connect.ServiceID = WMI_CAB_SVC;
    connect.EpCallbacks.EpTxComplete = host_htc_eptx_comp;
    //connect.UL_PipeID = 1;
    //connect.DL_PipeID = 1;
    status = HTCConnectService(osdev->host_htc_handle, &connect, (HTC_ENDPOINT_ID*)&(osdev->cab_ep));
    if (A_FAILED(status)) {
        printk("Failed to connect services!");
        return A_ERROR;
    }

    connect.ServiceID = WMI_UAPSD_SVC;
    connect.EpCallbacks.EpTxComplete = host_htc_eptx_comp;
    //connect.UL_PipeID = 1;
    //connect.DL_PipeID = 1;
    status = HTCConnectService(osdev->host_htc_handle, &connect, (HTC_ENDPOINT_ID*)&(osdev->uapsd_ep));
    if (A_FAILED(status)) {
        printk("Failed to connect services!");
        return A_ERROR;
    }

    connect.ServiceID = WMI_MGMT_SVC;
    connect.EpCallbacks.EpTxComplete = host_htc_eptx_comp;
    //connect.UL_PipeID = 1;
    //connect.DL_PipeID = 1;
    status = HTCConnectService(osdev->host_htc_handle, &connect, (HTC_ENDPOINT_ID*)&(osdev->mgmt_ep));
    if (A_FAILED(status)) {
        printk("Failed to connect services!");
        return A_ERROR;
    }

    connect.ServiceID = WMI_DATA_BE_SVC;
    connect.EpCallbacks.EpTxComplete = host_htc_eptx_comp;
    //connect.UL_PipeID = 1;
    //connect.DL_PipeID = 1;
    status = HTCConnectService(osdev->host_htc_handle, &connect, (HTC_ENDPOINT_ID*)&(osdev->data_BE_ep));
    if (A_FAILED(status)) {
        printk("Failed to connect services!");
        return A_ERROR;
    }

    connect.ServiceID = WMI_DATA_BK_SVC;
    connect.EpCallbacks.EpTxComplete = host_htc_eptx_comp;
    //connect.UL_PipeID = 1;
    //connect.DL_PipeID = 1;
    status = HTCConnectService(osdev->host_htc_handle, &connect, (HTC_ENDPOINT_ID*)&(osdev->data_BK_ep));

    if (A_FAILED(status)) {
        printk("Failed to connect services!");
        return A_ERROR;
    }

    connect.ServiceID = WMI_DATA_VI_SVC;
    connect.EpCallbacks.EpTxComplete = host_htc_eptx_comp;
    //connect.UL_PipeID = 1;
    //connect.DL_PipeID = 1;
    status = HTCConnectService(osdev->host_htc_handle, &connect, (HTC_ENDPOINT_ID*)&(osdev->data_VI_ep));
    if (A_FAILED(status)) {
        printk("Failed to connect services!");
        return A_ERROR;
    }

    connect.ServiceID = WMI_DATA_VO_SVC;
    connect.EpCallbacks.EpTxComplete = host_htc_eptx_comp;
    //connect.UL_PipeID = 1;
    //connect.DL_PipeID = 1;
    status = HTCConnectService(osdev->host_htc_handle, &connect, (HTC_ENDPOINT_ID*)&(osdev->data_VO_ep));

    if (A_FAILED(status)) {
        printk("Failed to connect services!");
        return A_ERROR;
    }

    /* setup credit distribution */
    HTCSetupCreditDist(osdev->host_htc_handle, &credit_state_info);
    status = HTCStart(osdev->host_htc_handle);
    if (A_FAILED(status)) {
        printk("Failed to start HTC!");
        return A_ERROR;
    } 
    return A_OK;
    //ath_host_attach(sc);  move to ath_usb_probe()
}

static
void driver_early_notify(void *pContext)
{
    struct ath_softc_net80211 *scn = (struct ath_softc_net80211 *)pContext;
    struct ieee80211com *ic = &(scn->sc_ic);

    /* 
        Flags to notify the plug-out happened. 
        Upper layer should has the corresponding actions.
    */
    ic->ic_delete_in_progress = 
    scn->sc_htc_delete_in_progress = 1;
}

A_STATUS ath_hif_htc_wmi_init(osdev_t osdev)
{
    HTC_INIT_INFO initInfo;
    A_STATUS status;

    /* initialize host-side HTC */
    //qdf_mem_zero(&initInfo, sizeof(HTC_INIT_INFO));
    OS_MEMZERO(&initInfo, sizeof(HTC_INIT_INFO));
    //memset(&initInfo, 0, sizeof(HTC_INIT_INFO));
    //initInfo.TargetReadyEvent = host_htc_rdy_handler;
    //initInfo.ConnectRspEvent = host_htc_conn_rsp_handler;
    //initInfo.ConfigPipeRspEvent = host_htc_cfgpipe_rsp_handler;

    initInfo.HTCCreditUpdateEvent = HTC_CREDIT_UPDATE_FN;
/* TODO: (Fusion K2) */
    //initInfo.HTCSchedNextEvent = host_htc_sched_next;

    //sc->sc_ic.host_htc_handle = HTCInit(sc, sc->sc_ic.sc_hdl, &initInfo);
    //HTCInit(sc->sc_ic.host_htc_handle, sc, sc->sc_ic.sc_hdl, &initInfo);
    HTCInit(osdev->host_htc_handle, osdev, osdev, &initInfo);

    //HTCSetDefaultPipe(sc->sc_ic.host_htc_handle, 0, 0);

    /* obtain host-side hif handle */
    osdev->host_hif_handle = HTCGetHIFHandle(osdev->host_htc_handle);

    /* initialize host-side WMI */
    //sc->sc_ic.host_wmi_handle = wmi_init(sc, sc->sc_ic.sc_hdl, &dispatch_magpie_sys_evts);
//    osdev->host_wmi_handle = wmi_init(osdev->bdev, osdev, &dispatch_magpie_sys_evts);
    osdev->host_wmi_handle = wmi_init(osdev->wmi_dev, osdev, &dispatch_magpie_sys_evts, &driver_early_notify);
    //printk("HOST : wmi handle = %pK hif handle = %pK\n", (u_int32_t *) sc->sc_ic.host_wmi_handle, (u_int32_t *)sc->sc_ic.host_hif_handle);

    status = host_htc_rdy_handler(osdev, 0);
    if (A_FAILED(status)) {
        printk("%s : host_htc_rdy_handler() call failed!", __func__);
    }

    return status;
}

void host_htc_credit_update(void *dev, HTC_ENDPOINT_ID epid, uint32_t credits)
{
#ifdef ATH_HTC_TX_SCHED    
     osdev_t osdev = (osdev_t)dev;
     
     struct ath_softc_net80211 *scn = (struct ath_softc_net80211 *)ath_netdev_priv(osdev->netdev);
     struct ath_softc *sc = (struct ath_softc *)scn->sc_dev; 
          
     if (epid == sc->sc_uapsd_ep) 
        ATH_HTC_UAPSD_CREDITUPDATE_SCHEDULE(osdev);
     else 
        scn->sc_ops->ath_htc_tx_schedule(osdev,epid);
#endif     
    return;
}

int ath_htc_notify_init(osdev_t dev, HTC_HANDLE hHTC)
{
    osdev_t osdev = (osdev_t)dev;

    /* initialize host-side hif/htc/wmi */
    osdev->host_htc_handle = hHTC;    
    return ath_hif_htc_wmi_init(osdev);
}

/* undo what was done ath_hif_htc_wmi_init */
void ath_hif_htc_wmi_free(osdev_t osdev)
{
    /* undo what was done wmi_init */
    wmi_shutdown(osdev->host_wmi_handle);
    osdev->host_wmi_handle = NULL;

    /* undo what was done in HTCInit() */
    HTCShutDown(osdev->host_htc_handle);
}

/*
 * dettach hif/htc/wmi what init by ath_htc_notify_init
 */
void ath_htc_notify_free(void* dev)
{
    osdev_t osdev = (osdev_t) dev;
    ath_hif_htc_wmi_free(osdev);

    /* free host-side hif/htc/wmi */
    osdev->host_htc_handle = NULL;
}

void ath_hif_htc_wmi_start(osdev_t osdev)
{
    wmi_start(osdev->host_wmi_handle);
}

void ath_hif_htc_wmi_stop(osdev_t osdev)
{
    wmi_stop(osdev->host_wmi_handle);
}

int MpHtcAttach(osdev_t osdev)
{
    HTC_host_switch_table_t sw;

    /* host-side HTC registration */
    OS_MEMZERO(&sw, sizeof(HTC_host_switch_table_t));
    sw.AddInstance = ath_htc_notify_init;
    sw.DeleteInstance = ath_htc_notify_free;//ath_detach;
    HTC_host_drv_register(&sw);

    return HIF_USBDeviceInserted(osdev);
}

void MpHtcDetach(osdev_t osdev)
{
    printk("%s: MpWmiStop\n", __func__);
    MpWmiStop(osdev);
}

void MpWmiStop(osdev_t osdev)
{
    ath_hif_htc_wmi_stop(osdev);
}

void MpWmiStart(osdev_t osdev)
{
    ath_hif_htc_wmi_start(osdev);
}

void MpHtcAddVapTarget(osdev_t osdev)
{
    struct ieee80211vap_target vt;
    struct ath_softc_net80211 *scn = ath_netdev_priv(osdev->netdev);
    struct ieee80211com *ic = &scn->sc_ic;
    int i;

    for (i = 0; i < HTC_MAX_VAP_NUM; i++) {
        if (ic->target_vap_bitmap[i].vap_valid == 1) {
            vt.iv_vapindex = ic->target_vap_bitmap[i].iv_vapindex;
            vt.iv_opmode = ic->target_vap_bitmap[i].iv_opmode;
            vt.iv_mcast_rate = ic->target_vap_bitmap[i].iv_mcast_rate;
            vt.iv_rtsthreshold = cpu_to_be16(ic->target_vap_bitmap[i].iv_rtsthreshold);
            IEEE80211_ADDR_COPY(vt.iv_myaddr,ic->target_vap_bitmap[i].vap_macaddr);

            scn->sc_ops->ath_wmi_add_vap(scn->sc_dev, &vt, sizeof(vt));
        }
    }
}

void MpHtcAddNodeTarget(osdev_t osdev)
{
    struct ieee80211_node_target nt, *ntar;
    struct ath_softc_net80211 *scn = ath_netdev_priv(osdev->netdev);
    struct ieee80211com *ic = &scn->sc_ic;
    struct ieee80211_node_table *_nt = &ic->ic_sta;
    struct ieee80211_node *ni, *next;
    int i;

    TAILQ_FOREACH_SAFE(ni, &_nt->nt_node, ni_list, next) {
        printk(KERN_INFO "%s: %02x %02x %02x %02x %02x %02x\n", __func__,
            ni->ni_macaddr[0], ni->ni_macaddr[1], ni->ni_macaddr[2],
            ni->ni_macaddr[3], ni->ni_macaddr[4], ni->ni_macaddr[5]);

        for (i = 0; i < HTC_MAX_NODE_NUM; i++) {
            if (ic->target_node_bitmap[i].ni_valid == 1) {
                if (memcmp(ni->ni_ic->target_node_bitmap[i].ni_macaddr,
                           ni->ni_macaddr, sizeof(ni->ni_macaddr)) == 0) {
                    break;
                }
            }
        }

        if (i == HTC_MAX_NODE_NUM) {
            printk("%s: can't find node in the node_bitmap\n", __func__);
            continue;
        }

        /*
         * Fill the target node nt.
         */
        nt.ni_associd    = htons(ni->ni_associd);
        nt.ni_txpower    = htons(ni->ni_txpower);

        qdf_mem_copy(&nt.ni_macaddr, &(ni->ni_macaddr), (IEEE80211_ADDR_LEN * sizeof(u_int8_t)));
        qdf_mem_copy(&nt.ni_bssid, &(ni->ni_bssid), (IEEE80211_ADDR_LEN * sizeof(u_int8_t)));
    
        nt.ni_nodeindex  = ic->target_node_bitmap[i].ni_nodeindex;
        nt.ni_vapindex   = ic->target_node_bitmap[i].ni_vapindex;
        nt.ni_vapnode    = 0;//add for rate control test
        nt.ni_flags      = htonl(ni->ni_flags);
    
        nt.ni_htcap      = htons(ni->ni_htcap);
        nt.ni_capinfo    = htons(ni->ni_capinfo);
        nt.ni_is_vapnode = ic->target_node_bitmap[i].ni_is_vapnode;
        nt.ni_maxampdu   = htons(ni->ni_maxampdu);
    #ifdef ENCAP_OFFLOAD
        nt.ni_ucast_keytsc = 0;
        nt.ni_mcast_keytsc = 0;
    #endif

        ntar = &nt;
        scn->sc_ops->ath_wmi_add_node(scn->sc_dev, ntar, sizeof(nt));
    }
}

void MpHtcUpdateTargetIC(osdev_t osdev)
{
    struct ieee80211com_target {
        u_int32_t    ic_flags;
        u_int32_t    ic_flags_ext;
        u_int32_t    ic_ampdu_limit;
        u_int8_t     ic_ampdu_subframes;
        u_int8_t     ic_tx_chainmask;
        u_int8_t     ic_tx_chainmask_legacy;
        u_int8_t     ic_rtscts_ratecode;
        u_int8_t     ic_protmode;
    };

    struct ieee80211com_target ic_tgt;
    struct ath_softc_net80211 *scn = ath_netdev_priv(osdev->netdev);
    int val;

    scn->sc_ops->ath_get_config_param(scn->sc_dev, ATH_PARAM_TXCHAINMASK, &val);
    ic_tgt.ic_tx_chainmask = val;    

    /* FIXME: Currently we fix these values */
    ic_tgt.ic_flags = cpu_to_be32(0x400c2400);
    ic_tgt.ic_flags_ext = cpu_to_be32(0x106080);
    ic_tgt.ic_ampdu_limit = cpu_to_be32(0xffff);    
    ic_tgt.ic_ampdu_subframes = 20;
    ic_tgt.ic_tx_chainmask_legacy = 1;
    ic_tgt.ic_rtscts_ratecode = 0;
    ic_tgt.ic_protmode = 1;   

    scn->sc_ops->ath_wmi_ic_update_target(scn->sc_dev, &ic_tgt, sizeof(struct ieee80211com_target));
}

void MpHtcSuspend(osdev_t osdev)
{
    MpWmiStop(osdev);
}

void MpHtcResume(osdev_t osdev)
{
    A_STATUS status;

    status = host_htc_rdy_handler(osdev, 0);

    /* TODO : fw-recovery and plug-out happened. */
    ASSERT(A_SUCCESS(status));
    MpWmiStart(osdev);

    /* TODO */
    MpHtcAddVapTarget(osdev);
    MpHtcAddNodeTarget(osdev);
    MpHtcUpdateTargetIC(osdev);
}

void MpHtcInit(void)
{
    HTC_host_switch_table_t sw = {0};

    /* host-side HTC registration */
    sw.AddInstance = ath_htc_notify_init;
    sw.DeleteInstance = ath_htc_notify_free;//ath_detach;
    HTC_host_drv_register(&sw);
}

#ifdef ATH_USB
EXPORT_SYMBOL(MpHtcDetach);
EXPORT_SYMBOL(MpHtcAttach);
EXPORT_SYMBOL(MpHtcSuspend);
EXPORT_SYMBOL(MpHtcResume);
#endif
