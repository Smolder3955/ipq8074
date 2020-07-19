/*
 * @@-COPYRIGHT-START-@@
 *
 * Copyright (c) 2014 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * @@-COPYRIGHT-END-@@
 */
#include "linux/if.h"
#include "linux/socket.h"
#include "linux/netlink.h"
#include <net/sock.h>
#include <linux/init.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/cache.h>
#include <linux/proc_fs.h>
#include <linux/module.h>
#include "sys/queue.h"
#include "ath_ssid_steering.h"

#if ATH_SSID_STEERING
#define MAX_PAYLOAD 1024
static struct sock *adhoc_nl_sock = NULL;

void ath_ssid_steering_netlink_send(ssidsteering_event_t *event)
{
    struct sk_buff *skb = NULL;
    struct nlmsghdr *nlh;
   
    skb = alloc_skb(NLMSG_SPACE(MAX_PAYLOAD), GFP_ATOMIC);
    if (NULL == skb) {
        return;
    }

    skb_put(skb, NLMSG_SPACE(MAX_PAYLOAD));
    nlh = (struct nlmsghdr *)skb->data;
    nlh->nlmsg_len = NLMSG_SPACE(MAX_PAYLOAD);
    nlh->nlmsg_pid = 0;  /* from kernel */
    nlh->nlmsg_flags = 0;
    event->datalen = 0;
    event->datalen += sizeof(ssidsteering_event_t);
    memcpy(NLMSG_DATA(nlh), event, sizeof(*event));
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,7,0)
    NETLINK_CB(skb).pid = 0;  /* from kernel */
#else
    NETLINK_CB(skb).portid = 0;  /* from kernel */
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
    NETLINK_CB(skb).dst_pid = 0;  /* multicast */
#endif
    /* to mcast group 1<<0 */
    NETLINK_CB(skb).dst_group = 1;

    /*multicast the message to all listening processes*/
    netlink_broadcast(adhoc_nl_sock, skb, 0, 1, GFP_KERNEL);
}

/* Receive messages from netlink socket. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,24)
static void ath_ssid_steering_netlink_receive(struct sk_buff *__skb)
#else
static void ath_ssid_steering_netlink_receive(struct sock *sk, int len)
#endif
{
    struct sk_buff *skb;
    struct nlmsghdr *nlh = NULL;
    u_int8_t *data = NULL;
    u_int32_t uid, pid, seq;

#if LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,24)
    while ((skb = skb_get(__skb)) !=NULL){
#else
        while ((skb = skb_dequeue(&sk->sk_receive_queue)) != NULL) {
#endif
            /* process netlink message pointed by skb->data */
            nlh = (struct nlmsghdr *)skb->data;
            pid = NETLINK_CREDS(skb)->pid;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,7,0)
            uid = NETLINK_CREDS(skb)->uid;
#else
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)
	        uid = NETLINK_CREDS(skb)->uid.val;
#else
#ifdef CONFIG_UIDGID_STRICT_TYPE_CHECKS
            uid = NETLINK_CREDS(skb)->uid.val;
#else
            uid = NETLINK_CREDS(skb)->uid;
#endif
#endif
#endif
            seq = nlh->nlmsg_seq;
            data = NLMSG_DATA(nlh);

            printk("recv skb from user space uid:%d pid:%d seq:%d,\n",uid,pid,seq);
            printk("data is :%s\n",(char *)data);

        }
        return ;
    }

int ath_ssid_steering_netlink_init(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0)
    extern struct net init_net;

    struct netlink_kernel_cfg cfg = {
        .groups = 1,
        .input  = ath_ssid_steering_netlink_receive,
    };
#endif
        if (adhoc_nl_sock == NULL) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0)
        adhoc_nl_sock = (struct sock *)netlink_kernel_create(&init_net, NETLINK_ATH_SSID_EVENT,
                    &cfg);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0)
        adhoc_nl_sock = (struct sock *)netlink_kernel_create(&init_net, NETLINK_ATH_SSID_EVENT,
                    THIS_MODULE, &cfg);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,24)
            adhoc_nl_sock = (struct sock *)netlink_kernel_create(&init_net, NETLINK_ATH_SSID_EVENT,
                    1, &ath_ssid_steering_netlink_receive, NULL, THIS_MODULE);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,22)
            adhoc_nl_sock = (struct sock *)netlink_kernel_create(NETLINK_ATH_SSID_EVENT,
                    1, &ath_ssid_steering_netlink_receive, (struct mutex *) NULL, THIS_MODULE);
#else
            adhoc_nl_sock = (struct sock *)netlink_kernel_create(NETLINK_ATH_SSID_EVENT,
                    1, &ath_ssid_steering_netlink_receive, THIS_MODULE);
#endif
            if (adhoc_nl_sock == NULL) {
                printk("%s NETLINK_KERNEL_CREATE FAILED\n", __func__);
                return -ENODEV;
            }
        }

        return 0;
}

int ath_ssid_steering_netlink_delete(void)
{
        if (adhoc_nl_sock) {
	    netlink_kernel_release(adhoc_nl_sock);
            adhoc_nl_sock = NULL;
        }

        return 0;
}
EXPORT_SYMBOL(ath_ssid_steering_netlink_send);
EXPORT_SYMBOL(ath_ssid_steering_netlink_init);
EXPORT_SYMBOL(ath_ssid_steering_netlink_delete);
#endif

