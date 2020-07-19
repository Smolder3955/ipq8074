/*
 * Copyright (c) 2002-2006 Atheros Communications, Inc.
 * All rights reserved.
 *
 */
/*
 * Copyright (c) 2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
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

#include "ath_netlink.h"
#include "sys/queue.h"


#define MAX_PAYLOAD 1024
static struct sock *adhoc_nl_sock = NULL;

void ath_adhoc_netlink_send(ath_netlink_event_t *event, char *event_data, u_int32_t event_datalen)
{
    struct sk_buff *skb = NULL;
    struct nlmsghdr *nlh;
    char * msg_event_data;

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
    if(event_data) {
        msg_event_data = (char*)(((ath_netlink_event_t *)NLMSG_DATA(nlh)) + 1);
        memcpy(msg_event_data, event_data, event_datalen);
        event->datalen = event_datalen;
    }
    event->datalen += sizeof(ath_netlink_event_t);
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

/* --adhoc_netlink_data_ready--
 * Stub function that does nothing */
void ath_adhoc_netlink_reply(int pid,int seq,void *payload)
{
    return;
}

/* Receive messages from netlink socket. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,24)
static void ath_adhoc_netlink_receive(struct sk_buff *__skb)
#else
static void ath_adhoc_netlink_receive(struct sock *sk, int len)
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

    ath_adhoc_netlink_reply(pid, seq,data);
   }

   return ;
}

int ath_adhoc_netlink_init(void)
{
    if (adhoc_nl_sock == NULL) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION (3,10,0)
        struct netlink_kernel_cfg cfg;
        memset(&cfg, 0, sizeof(cfg));
        cfg.groups = 1;
        cfg.input = &ath_adhoc_netlink_receive;
        adhoc_nl_sock = (struct sock *)netlink_kernel_create(&init_net, NETLINK_ATH_EVENT,&cfg);

#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0)
        struct netlink_kernel_cfg cfg = {
            .groups = 1,
            .input  = ath_adhoc_netlink_receive,
        };

        adhoc_nl_sock = (struct sock *)netlink_kernel_create(&init_net, NETLINK_ATH_EVENT, &cfg);

#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0)
        struct netlink_kernel_cfg cfg = {
            .groups = 1,
            .input  = ath_adhoc_netlink_receive,
        };

        adhoc_nl_sock = (struct sock *)netlink_kernel_create(&init_net, NETLINK_ATH_EVENT,
                                   THIS_MODULE, &cfg);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,24)
        adhoc_nl_sock = (struct sock *)netlink_kernel_create(&init_net, NETLINK_ATH_EVENT,
                                   1, &ath_adhoc_netlink_receive, NULL, THIS_MODULE);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,22)
        adhoc_nl_sock = (struct sock *)netlink_kernel_create(NETLINK_ATH_EVENT,
                                   1, &ath_adhoc_netlink_receive, (struct mutex *) NULL, THIS_MODULE);
#else
        adhoc_nl_sock = (struct sock *)netlink_kernel_create(NETLINK_ATH_EVENT,
                                   1, &ath_adhoc_netlink_receive, THIS_MODULE);
#endif
        if (adhoc_nl_sock == NULL) {
            printk("%s NETLINK_KERNEL_CREATE FAILED\n", __func__);
            return -ENODEV;
        }
    }

    return 0;
}

int ath_adhoc_netlink_delete(void)
{
    if (adhoc_nl_sock) {
	netlink_kernel_release(adhoc_nl_sock);
        adhoc_nl_sock = NULL;
    }

    return 0;
}

EXPORT_SYMBOL(ath_adhoc_netlink_init);
EXPORT_SYMBOL(ath_adhoc_netlink_delete);
EXPORT_SYMBOL(ath_adhoc_netlink_send);


