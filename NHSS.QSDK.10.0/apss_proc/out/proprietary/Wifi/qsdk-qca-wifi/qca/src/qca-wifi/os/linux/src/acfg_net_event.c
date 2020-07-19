/*
 * Copyright (c) 2017, 2019 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * Copyright (c) 2008-2010, Atheros Communications Inc.
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

#include <linux/module.h>
#include <linux/version.h>
#include <linux/netdevice.h>
#include <linux/wireless.h>
#include <net/iw_handler.h>
#include <linux/if_arp.h>
#include <linux/rtnetlink.h>
#include <linux/kernel.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
#include <net/net_namespace.h>
#endif
#include <net/netlink.h>
#include <net/sock.h>
#include <osdep.h>
#include <ieee80211_var.h>
#include <ieee80211_channel.h>
#include <wlan_dfs_ioctl.h>
#include <i_qdf_types.h>
#include <acfg_api_types.h>
#include <acfg_event_types.h>
#include <ieee80211_ioctl_acfg.h>
#include <ieee80211_acfg.h>
#include <acfg_drv_event.h>
#include <ieee80211_var.h>
#include <osif_private.h>
#include <ieee80211_mlme_dfs_dispatcher.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>

struct sock *acfg_ev_sock = NULL;

/**
 * IW Events
 */
typedef uint32_t  (* __acfg_event_t)(struct net_device *,
                                      acfg_ev_data_t     *data);

#define ACFG_MAX_PAYLOAD 1024
//#define NETLINK_ACFG_EVENT 20

#define PROTO_IWEVENT(name)    \
    static uint32_t  acfg_iwevent_##name(struct net_device  *,\
                        acfg_ev_data_t *)

PROTO_IWEVENT(scan_done);
PROTO_IWEVENT(assoc_ap);
PROTO_IWEVENT(assoc_sta);
PROTO_IWEVENT(wsupp_generic);
PROTO_IWEVENT(wapi);

#define EVENT_IDX(x)    [x]

/**
 * @brief Table of functions to dispatch iw events
 */
__acfg_event_t    iw_events[] = {
    EVENT_IDX(ACFG_EV_SCAN_DONE)    = acfg_iwevent_scan_done,
    EVENT_IDX(ACFG_EV_ASSOC_AP)     = acfg_iwevent_assoc_ap,
    EVENT_IDX(ACFG_EV_ASSOC_STA)    = acfg_iwevent_assoc_sta,
    EVENT_IDX(ACFG_EV_DISASSOC_AP)    = NULL,
    EVENT_IDX(ACFG_EV_DISASSOC_STA)    = NULL,
    EVENT_IDX(ACFG_EV_WAPI)    = acfg_iwevent_wapi,
    EVENT_IDX(ACFG_EV_DEAUTH_AP)    = NULL,
    EVENT_IDX(ACFG_EV_DEAUTH_STA)    = NULL,
    EVENT_IDX(ACFG_EV_NODE_LEAVE)    = NULL,
};

/**
 * @brief Send scan done through IW
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
acfg_iwevent_scan_done(struct net_device *dev, acfg_ev_data_t    *data)
{
    union iwreq_data wreq = {{0}};

    /* dispatch wireless event indicating scan completed */
    WIRELESS_SEND_EVENT(dev, SIOCGIWSCAN, &wreq, NULL);

    return QDF_STATUS_SUCCESS;
}

/**
 * @brief Send WAPI through IW
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
acfg_iwevent_wapi(struct net_device *dev, acfg_ev_data_t    *data)
{
    union iwreq_data wreq = {{0}};
    char *buf;
    int  bufsize;
#define WPS_FRAM_TAG_SIZE 30

    acfg_wsupp_custom_message_t *custom = (acfg_wsupp_custom_message_t *)data;

    bufsize = sizeof(acfg_wsupp_custom_message_t) + WPS_FRAM_TAG_SIZE;
    buf = qdf_mem_malloc(bufsize);
    if (buf == NULL) return -ENOMEM;
    memset(buf,0,bufsize);
    wreq.data.length = bufsize;
    memcpy(buf, custom->raw_message, sizeof(acfg_wsupp_custom_message_t));

    /*
     * IWEVASSOCREQIE is HACK for IWEVCUSTOM to overcome 256 bytes limitation
     *
     * dispatch wireless event indicating probe request frame
     */
    WIRELESS_SEND_EVENT(dev, IWEVCUSTOM, &wreq, buf);

    qdf_mem_free(buf);

    return QDF_STATUS_SUCCESS;
}

/**
 * @brief Send Assoc with AP event through IW
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
acfg_iwevent_assoc_ap (struct net_device *dev, acfg_ev_data_t    *data)
{
    union iwreq_data wreq = {{0}};

    memcpy(wreq.addr.sa_data, data->assoc_ap.bssid, ACFG_MACADDR_LEN);
    wreq.addr.sa_family = ARPHRD_ETHER;

    WIRELESS_SEND_EVENT(dev, IWEVREGISTERED, &wreq, NULL);

    return QDF_STATUS_SUCCESS;
}

/**
 * @brief Send Assoc with AP event through IW
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
acfg_iwevent_assoc_sta (struct net_device *dev, acfg_ev_data_t    *data)
{
    union iwreq_data wreq = {{0}};

    memcpy(wreq.addr.sa_data, data->assoc_sta.bssid, ACFG_MACADDR_LEN);
    wreq.addr.sa_family = ARPHRD_ETHER;

    WIRELESS_SEND_EVENT(dev, SIOCGIWAP, &wreq, NULL);

    return QDF_STATUS_SUCCESS;
}

static void acfg_event_work_process(void *arg)
{
    osdev_t osdev = (osdev_t)arg;
    qdf_nbuf_t skb = NULL;
    struct net *net = NULL;
    int process_pid = 0;
    u_int8_t skb_nl_flag = 0;
    struct nlmsghdr *nlh = NULL;
#if ACFG_NETLINK_TX
    acfg_netlink_pvt_t *acfg_nl = osdev->osdev_acfg_handle;

    if (acfg_nl == NULL || acfg_nl->acfg_sock == NULL) {
       printk("%s: acfg_nl (or) acfg_sock is NULL !!\n", __func__);
       return;
    }
#endif

    net = (struct net *)qdf_mem_malloc_atomic(sizeof(struct net));
    if (NULL == net){
        printk("%s: mem alloc failed for net\n",__FUNCTION__);
        return;
    }

    nlh = (struct nlmsghdr *)qdf_mem_malloc_atomic(sizeof(struct nlmsghdr));
    if (NULL == nlh){
        printk("%s: mem alloc failed for nlh\n",__FUNCTION__);
        qdf_mem_free(net);
        return;
    }

    nlh->nlmsg_flags = 0;

    qdf_spin_lock_bh(&(osdev->acfg_event_queue_lock));
    skb = qdf_nbuf_queue_remove(&(osdev->acfg_event_list));
    qdf_spin_unlock_bh(&(osdev->acfg_event_queue_lock));

    while(skb != NULL){
        skb_nl_flag = wbuf_get_netlink_acfg_flags(skb);
/*
acfg events have two types:
ACFG_NETLINK_MSG_RESPONSE: for offchan TX communication between user app and WLAN driver, bidirectional
ACFG_NETLINK_SENT_EVENT: from WLAN driver to user app, one-directional

For ACFG_NETLINK_MSG_RESPONSE: we use sock(acfg_nl->acfg_sock) allocated in acfg_attach
For ACFG_NETLINK_SENT_EVENT:   we use sock in Linux's official init_net
*/
        if(skb_nl_flag & ACFG_NETLINK_MSG_RESPONSE){
#if ACFG_NETLINK_TX
            net->rtnl = acfg_nl->acfg_sock;
#endif
            nlh->nlmsg_flags |= NLM_F_ECHO;
            process_pid = wbuf_get_netlink_acfg_pid(skb);
            rtnl_notify(skb, net, process_pid, 0, nlh, GFP_KERNEL);
        }else{  //ACFG_NETLINK_SENT_EVENT
            rtnl_notify(skb, &init_net, 0, RTNLGRP_NOTIFY, NULL, GFP_KERNEL);
        }

        qdf_spin_lock_bh(&(osdev->acfg_event_queue_lock));
        skb = qdf_nbuf_queue_remove(&(osdev->acfg_event_list));
        qdf_spin_unlock_bh(&(osdev->acfg_event_queue_lock));
    }

    qdf_mem_free(net);
    qdf_mem_free(nlh);
}

/**
 * @brief Indicate the ACFG Eevnt to the upper layer
 *
 * @param hdl
 * @param event
 *
 * @return
 */
uint32_t
acfg_net_indicate_event(struct net_device *dev, osdev_t osdev, void *event,
                    u_int32_t event_type, int send_iwevent, u_int32_t pid)
{
    struct sk_buff *skb;
    int err;
    __acfg_event_t   fn;
    u_int32_t event_len = 0;
    struct nl_event_info ev_info = {0};

#if LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,24)
    if (!net_eq(dev_net(dev), &init_net))
        return QDF_STATUS_E_FAILURE  ;
#endif

    if(!osdev->acfg_netlink_wq_init_done){
        printk("Work queue not ready, drop acfg event!\n");
        return QDF_STATUS_E_FAILURE;
    }


    qdf_spin_lock_bh(&(osdev->acfg_event_queue_lock));
    if(qdf_nbuf_queue_len(&(osdev->acfg_event_list)) >= ACFG_EVENT_LIST_MAX_LEN){
        qdf_spin_unlock_bh(&(osdev->acfg_event_queue_lock));
        return QDF_STATUS_E_FAILURE;
    }
    qdf_spin_unlock_bh(&(osdev->acfg_event_queue_lock));

#if LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,24)
    skb = nlmsg_new(NLMSG_DEFAULT_SIZE, GFP_ATOMIC);
#else
    skb = nlmsg_new(NLMSG_SPACE(ACFG_MAX_PAYLOAD));
#endif
    if (!skb)
        return QDF_STATUS_E_NOMEM ;

    if(event_type == ACFG_NETLINK_MSG_RESPONSE){
        event_len = sizeof(struct acfg_offchan_resp);
        wbuf_set_netlink_acfg_flags(skb, ACFG_NETLINK_MSG_RESPONSE);
        wbuf_set_netlink_acfg_pid(skb, pid);
    }else if(event_type == ACFG_NETLINK_SENT_EVENT){
        event_len = sizeof(acfg_os_event_t);
    }

    if(event_len > NLMSG_DEFAULT_SIZE){
        printk("%s: Event length bigger than NLMSG_DEFAULT_SIZE(%d)\n",__FUNCTION__,(int)NLMSG_DEFAULT_SIZE);
        kfree_skb(skb);
        return QDF_STATUS_E_FAILURE ;
    }

    ev_info.type      = RTM_NEWLINK;
    if (event_type == ACFG_NETLINK_MSG_RESPONSE) {
        ev_info.ev_type = TYPE_MSG_RESP;
    } else if (event_type == ACFG_NETLINK_SENT_EVENT) {
        ev_info.ev_type = TYPE_SENT_EVENT;
    }
    ev_info.ev_len    = event_len;
    ev_info.pid       = pid;
    ev_info.seq       = 0;
    ev_info.flags     = 0;
    ev_info.event     = event;

    err = nl_ev_fill_info(skb, dev, &ev_info);
    if (err < 0) {
        kfree_skb(skb);
        return QDF_STATUS_E_FAILURE ;
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
  	err = nlmsg_multicast(acfg_ev_sock, skb, 0, RTNLGRP_NOTIFY);
#else
    qdf_spin_lock_bh(&(osdev->acfg_event_queue_lock));
    qdf_nbuf_queue_add(&(osdev->acfg_event_list), skb);
    qdf_spin_unlock_bh(&(osdev->acfg_event_queue_lock));

    qdf_sched_work(NULL, &(osdev->acfg_event_os_work));
#endif
    if(event_type == ACFG_NETLINK_SENT_EVENT){
        if (send_iwevent) {
            /* Send iw event */
            fn = iw_events[((acfg_os_event_t *)event)->id];
            if (fn != NULL) {
                fn(dev, &((acfg_os_event_t *)event)->data);
            }
        }
    }
    return QDF_STATUS_SUCCESS ;
}

uint32_t acfg_event_workqueue_init(osdev_t osdev)
{
    if(!osdev->acfg_netlink_wq_init_done){
        qdf_create_work(NULL,&(osdev->acfg_event_os_work), acfg_event_work_process, osdev);
        qdf_nbuf_queue_init(&(osdev->acfg_event_list));
        qdf_spinlock_create(&(osdev->acfg_event_queue_lock));
        osdev->acfg_netlink_wq_init_done = 1;
    }
	return QDF_STATUS_SUCCESS;
}

static void
acfg_nl_queue_free(qdf_nbuf_queue_t *qhead)
{
    qdf_nbuf_t  buf = NULL;

    while((buf=qdf_nbuf_queue_remove(qhead))!=NULL){
        kfree_skb(buf);
    }
}

void acfg_event_workqueue_delete(osdev_t osdev)
{
    if(osdev->acfg_netlink_wq_init_done){
        qdf_disable_work(&(osdev->acfg_event_os_work));
        qdf_flush_work(&(osdev->acfg_event_os_work));
        if( !qdf_nbuf_is_queue_empty(&(osdev->acfg_event_list)) ){
            qdf_spin_lock_bh(&(osdev->acfg_event_queue_lock));
            acfg_nl_queue_free(&(osdev->acfg_event_list));
            qdf_spin_unlock_bh(&(osdev->acfg_event_queue_lock));
        }
        osdev->acfg_netlink_wq_init_done = 0;
    }
}

/*acfg_event_netlink_init is used for direct attach*/
/*acfg_attach is used for offload*/
uint32_t acfg_event_netlink_init(void)
{
	if (acfg_ev_sock == NULL) {
#if LINUX_VERSION_CODE > KERNEL_VERSION (2,6,30)
	return QDF_STATUS_SUCCESS;
#elif LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,24)
		acfg_ev_sock = (struct sock *)netlink_kernel_create(&init_net,
								NETLINK_ROUTE, 1, &acfg_nl_recv,
								NULL, THIS_MODULE);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,22)
		acfg_ev_sock = (struct sock *)netlink_kernel_create(NETLINK_ROUTE,
								1, &acfg_nl_recv, (struct mutex *) NULL,
								THIS_MODULE);
#else
		acfg_ev_sock = (struct sock *)netlink_kernel_create(
									NETLINK_ACFG_EVENT,
									1, &acfg_nl_recv, THIS_MODULE);

#endif
		if (acfg_ev_sock == NULL) {
			printk("%s: netlink create failed\n", __func__);
			return QDF_STATUS_E_FAILURE;
		}
	}
	return QDF_STATUS_SUCCESS;
}

void acfg_event_netlink_delete (void)
{
	if (acfg_ev_sock) {
		netlink_kernel_release(acfg_ev_sock);
		acfg_ev_sock = NULL;
	}
}

/*Per-radio events*/

void osif_radio_evt_radar_detected_ap(void *event_arg, struct ieee80211com *ic)
{
    struct net_device *dev = (struct net_device *)event_arg;
    acfg_event_data_t *acfg_event = NULL;
    int i, count = 0;
    struct dfsreq_nolinfo *nolinfo = NULL;
    struct wlan_objmgr_pdev *pdev;

    acfg_event = (acfg_event_data_t *)qdf_mem_malloc_atomic(sizeof(acfg_event_data_t));
    if (acfg_event == NULL)
        return;
    memset(acfg_event, 0, sizeof(acfg_event_data_t));

    nolinfo = qdf_mem_malloc_atomic(sizeof(struct dfsreq_nolinfo));
    if (nolinfo == NULL){
        printk("%s: mem alloc failed for nolinfo\n",__FUNCTION__);
        qdf_mem_free(acfg_event);
        return;
    }
    memset(nolinfo, 0, sizeof(struct dfsreq_nolinfo));

    pdev = ic->ic_pdev_obj;
    if(pdev == NULL) {
        qdf_print("pdev is null ");
        return;
    }

    /*Get correct non-occupancy channel info from NOL*/
    if (wlan_objmgr_pdev_try_get_ref(pdev, WLAN_DFS_ID) !=
            QDF_STATUS_SUCCESS) {
        return;
    }
    mlme_dfs_getnol(pdev, nolinfo);
    wlan_objmgr_pdev_release_ref(pdev, WLAN_DFS_ID);

    for(i=0; i < nolinfo->dfs_ch_nchans; i++)
    {
        acfg_event->freqs[count] = wlan_mhz2ieee(ic, nolinfo->dfs_nol[i].nol_freq, 0);
        count++;
    }

    acfg_event->count = count;
    acfg_send_event(dev, ic->ic_osdev, WL_EVENT_TYPE_RADAR, acfg_event);

    qdf_mem_free(nolinfo);
    qdf_mem_free(acfg_event);
}

void osif_radio_evt_watch_dog(void *event_arg, struct ieee80211com *ic, u_int32_t reason)
{
    struct net_device *dev = (struct net_device *)event_arg;
    acfg_event_data_t *acfg_event = NULL;

    acfg_event = (acfg_event_data_t *)qdf_mem_malloc_atomic(sizeof(acfg_event_data_t));
    if (acfg_event == NULL)
        return;

    memset(acfg_event, 0, sizeof(acfg_event_data_t));
    acfg_event->reason = reason;
    acfg_send_event(dev, ic->ic_osdev, WL_EVENT_TYPE_WDT_EVENT, acfg_event);
    qdf_mem_free(acfg_event);
}

void osif_radio_evt_dfs_cac_start(void *event_arg, struct ieee80211com *ic)
{
    struct net_device *dev = (struct net_device *)event_arg;
    acfg_event_data_t *acfg_event = NULL;

    acfg_event = (acfg_event_data_t *)qdf_mem_malloc_atomic(sizeof(acfg_event_data_t));
    if (acfg_event == NULL)
        return;

    memset(acfg_event, 0, sizeof(acfg_event_data_t));

    acfg_send_event(dev, ic->ic_osdev, WL_EVENT_TYPE_CAC_START, acfg_event);
    qdf_mem_free(acfg_event);
}

void osif_radio_evt_dfs_up_after_cac(void *event_arg, struct ieee80211com *ic)
{
    struct net_device *dev = (struct net_device *)event_arg;
    acfg_event_data_t *acfg_event = NULL;
    union iwreq_data wreq;
    struct ieee80211vap *tmpvap;

    acfg_event = (acfg_event_data_t *)qdf_mem_malloc_atomic(sizeof(acfg_event_data_t));
    if (acfg_event == NULL)
        return;

    memset(acfg_event, 0, sizeof(acfg_event_data_t));

    acfg_send_event(dev, ic->ic_osdev, WL_EVENT_TYPE_UP_AFTER_CAC, acfg_event);
    qdf_mem_free(acfg_event);

    TAILQ_FOREACH(tmpvap, &ic->ic_vaps, iv_next) {
        if ((tmpvap->iv_opmode == IEEE80211_M_HOSTAP)) {
            memset(&wreq, 0, sizeof(wreq));
            wreq.data.flags = IEEE80211_EV_CAC_EXPIRED;
            wreq.data.length = IFNAMSIZ;
            WIRELESS_SEND_EVENT(dev, IWEVCUSTOM, &wreq, tmpvap->iv_netdev_name);
        }
    }
}

/*  Channel utilisation event.
 *
 */
void osif_radio_evt_chan_util(struct ieee80211com *ic, u_int8_t self_util, u_int8_t obss_util)
{
    if( ic == NULL)
        return;

    acfg_chan_stats_event(ic, self_util, obss_util);
}

/* per-VAP assoc failure event */
void acfg_assoc_failure_indication_event(struct ieee80211vap *vap, u_int8_t *macaddr, u_int16_t reason)
{
    struct net_device *dev = ((osif_dev *)(vap->iv_ifp))->netdev;
    /*send acfg event*/
    acfg_event_data_t *acfg_event = (acfg_event_data_t *)qdf_mem_malloc(sizeof(acfg_event_data_t));
    if (acfg_event == NULL){
        return;
    }

    acfg_event->reason = reason;
    IEEE80211_ADDR_COPY(acfg_event->addr, macaddr);
    acfg_send_event(dev, ((osif_dev *)(vap->iv_ifp))->os_handle, WL_EVENT_TYPE_ASSOC_FAILURE, acfg_event);
    qdf_mem_free(acfg_event);
}

/**
* @brief    Sends a netlink broadcast event containing
*           self bss utilization, obss utilization,
*           noise floor and current channel centre frequency
*
* @param ic             pointer to ieee80211com
* @param self_util      self bss channel utilization
* @param obss_util      other bss channel utilization
* @Return               Void
*/
void acfg_chan_stats_event(struct ieee80211com *ic,
                      u_int8_t self_util,
                      u_int8_t obss_util)
{
    osdev_t osdev = ic->ic_osdev;
    struct net_device *dev = osdev->netdev;
    wlan_chan_t chan = NULL;
    acfg_event_data_t *acfg_event = NULL;

    if (obss_util < ic->ic_chan_stats_th) {
        /* OBSS channel utilization is below thresold value.
         * Skip sending this event.
         */
        return;
    }


    /* create and send an acfg event to upper layer */
    acfg_event = (acfg_event_data_t *)
        qdf_mem_malloc(sizeof(acfg_event_data_t));

    if (acfg_event == NULL) {
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
            "%s: Unable to allocate memory for chan stats event\n", __func__);
        return;
    }

    chan = wlan_get_dev_current_channel(ic);
    if (chan) {
        acfg_event->chan_stats.frequency = wlan_channel_frequency(chan);
    }
    acfg_event->chan_stats.noise_floor = ic->ic_get_cur_chan_nf(ic);
    acfg_event->chan_stats.obss_util = obss_util;
    acfg_event->chan_stats.self_bss_util = self_util;

    acfg_send_event(dev, ic->ic_osdev, WL_EVENT_TYPE_CHAN_STATS, acfg_event);
    qdf_mem_free(acfg_event);
}
