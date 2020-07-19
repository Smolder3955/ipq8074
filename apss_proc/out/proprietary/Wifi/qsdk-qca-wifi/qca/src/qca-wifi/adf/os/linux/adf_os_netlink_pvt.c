/*
* Copyright (c) 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
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

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/device.h>
#include <net/sock.h>

#include <adf_net.h>
#include <adf_os_netlink.h>

#include <adf_os_util.h>
#include <adf_os_types_pvt.h>

//#define NL_DEBUG

#ifdef NL_DEBUG
#define nldebug printk
#else
#define nldebug(...)
#endif

/**
 * @brief   linux netlink callback function which hides the buffer 
 *          manipulation and provides user context for the registered
 *          user callback
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,24)
static void 
__adf_netlink_input(struct sk_buff *__skb)
#else
static void
__adf_netlink_input(struct sock *__sk, int len)    
#endif
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,24)
    struct sock *sock = __skb->sk;
    struct sk_buff *skb = __skb;
#else
    struct sock *sock = __sk;
    struct sk_buff *skb = skb_dequeue(&__sk->sk_receive_queue);
#endif
    struct nlmsghdr *nlh = nlmsg_hdr(skb);
    __adf_netlink_softc_t *nlsc;

    if (sock == NULL || sock->sk_user_data == NULL) {
        nldebug("%s: failed to get ctx\n", __func__);
        return;
    }
    
    if (skb->len < NLMSG_SPACE(0)) {
        nldebug("%s: invalid packet len\n", __func__);
        return;
    }

    skb_pull(skb, NLMSG_SPACE(0));

    nlsc = (__adf_netlink_softc_t *) sock->sk_user_data;
    nlsc->input(nlsc->ctx, skb, nlh->nlmsg_pid);
    
#if LINUX_VERSION_CODE < KERNEL_VERSION (2,6,24)
    kfree_skb(skb);
#endif    
}


/**
 * @brief   linux implementation for netlink interface creation
 */
adf_netlink_handle_t
__adf_netlink_create(adf_netlink_cb_t input, void *ctx, a_uint32_t unit, a_uint32_t groups)
{
    __adf_netlink_softc_t *nlsc;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0)
    extern struct net init_net;

    struct netlink_kernel_cfg cfg = {
        .groups = groups,
        .input  = __adf_netlink_input,
    };
#endif
    nlsc = (__adf_netlink_softc_t *) kzalloc(sizeof(__adf_netlink_softc_t), 
                GFP_ATOMIC);
    if (!nlsc)
        return NULL;

    /* FIXME: compatible with earlier kernel version */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0)
    nlsc->sock = (struct sock *)netlink_kernel_create(&init_net, unit, &cfg);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0)
    nlsc->sock = (struct sock *)netlink_kernel_create(&init_net, unit,
                                   THIS_MODULE, &cfg);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,31)
    nlsc->sock = netlink_kernel_create(&init_net, unit, groups, 
               __adf_netlink_input, NULL, THIS_MODULE);
#else
    nlsc->sock = (struct sock *)netlink_kernel_create(unit,1,&spectral_nl_data_ready, THIS_MODULE);
#endif
    if (nlsc->sock == NULL) {
        kfree(nlsc);
        return NULL;
    }

    nlsc->input = input;
    nlsc->ctx = ctx;

    nlsc->sock->sk_user_data = nlsc;

    return (adf_netlink_handle_t) nlsc;
}


/**
 * @brief   linux implementation for netlink interface removal
 */
void
__adf_netlink_delete(adf_netlink_handle_t nlhdl)
{
    __adf_netlink_softc_t *nlsc = (__adf_netlink_softc_t *) nlhdl;

    netlink_kernel_release(nlsc->sock);
    
    kfree(nlsc);
}


/**
 * @brief   allocate necessary space for network buffer which contains
 *          netlink message and user data
 */
adf_nbuf_t
__adf_netlink_alloc(a_uint32_t len)
{
    struct sk_buff *skb = NULL;
    skb = alloc_skb(NLMSG_SPACE(len), GFP_ATOMIC);
    if(skb == NULL) {
        return NULL;
    }

    skb_reserve(skb, NLMSG_SPACE(0));

    return (adf_nbuf_t) skb;
}


/**
 * @brief   internal function which prepare netlink message in linux
 */
static void
__adf_netlink_prepare(adf_nbuf_t netbuf, 
                      adf_netlink_addr_t addr,
                      adf_netlink_addr_t groups)
{
    int datalen;
    struct sk_buff *skb = netbuf;
    struct nlmsghdr *nlh = NULL;

    datalen = skb->len;
    nlh = (struct nlmsghdr *) skb_push(skb, NLMSG_SPACE(0));
    nlh->nlmsg_pid = addr;
    nlh->nlmsg_flags = 0;
    nlh->nlmsg_len = NLMSG_LENGTH(datalen);

    //NETLINK_CB(skb).pid = 0;
    //NETLINK_CB(skb).dst_pid = addr;
    NETLINK_CB(skb).dst_group = groups;
}

/**
 * @brief   linux implementation for sending netlink unicast message
 */
a_status_t 
__adf_netlink_unicast(adf_netlink_handle_t nlhdl, 
                    adf_nbuf_t netbuf, 
                    adf_netlink_addr_t addr)
{
    int ret;
    __adf_netlink_softc_t *nlsc = (__adf_netlink_softc_t *) nlhdl;

    __adf_netlink_prepare(netbuf, addr, 0);

    ret = netlink_unicast(nlsc->sock, netbuf, addr, 1);

    if (ret)
        return A_STATUS_FAILED;
    return ret;
}

/**
 * @brief   linux implementation for sending netlink broadcast message
 */
a_status_t
__adf_netlink_broadcast(adf_netlink_handle_t nlhdl, 
                      adf_nbuf_t netbuf, 
                      adf_netlink_addr_t groups)
{
    int ret;
    __adf_netlink_softc_t *nlsc = (__adf_netlink_softc_t *) nlhdl;

    __adf_netlink_prepare(netbuf, 0, groups);

    ret = netlink_broadcast(nlsc->sock, netbuf, 0, groups, GFP_ATOMIC);

    if (ret)
        return A_STATUS_FAILED;
    return ret;
}

/* exported symbols */
EXPORT_SYMBOL(__adf_netlink_create);
EXPORT_SYMBOL(__adf_netlink_delete);
EXPORT_SYMBOL(__adf_netlink_unicast);
EXPORT_SYMBOL(__adf_netlink_broadcast);
EXPORT_SYMBOL(__adf_netlink_alloc);

