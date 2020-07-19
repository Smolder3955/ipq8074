/*
 * @@-COPYRIGHT-START-@@
 *
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * @@-COPYRIGHT-END-@@
 */

#include "linux/if.h"
#include "linux/socket.h"
#include "linux/netlink.h"
#include <net/genetlink.h>
#include <net/sock.h>
#include <linux/init.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/cache.h>
#include <linux/proc_fs.h>
#include <linux/module.h>
#include "sys/queue.h"
#include "ath_papi.h"
#include <qdf_trace.h>

#if ATH_PARAMETER_API

/* genl attribute policies */
static struct nla_policy papi_genl_policy[PAPI_ATTR_MAX + 1] = {
    [PAPI_ATTR_EVENT]    = {.type = NLA_UNSPEC,
                            .len = sizeof(struct ath_netlink_papi_event)},
    [PAPI_ATTR_TEST]     = {.type = NLA_NUL_STRING },
};

/* genl family definition */
static struct genl_family papi_genl_family = {
    .id = GENL_ID_GENERATE,
    .hdrsize = 0,
    .name = PAPI_GENL_FAMILY_NAME,
    .version = PAPI_GENL_VERSION,
    .maxattr = PAPI_ATTR_MAX,
};

/* genl multicast group */
static struct genl_multicast_group papi_genl_mcgrp[] = {
    {
        .name = PAPI_GENL_MCGRP,
    },
};

/* Used for reference count */
static atomic_t registered;

void ath_papi_netlink_send(ath_netlink_papi_event_t *event)
{
    int ret_val;
    int buf_len;
    int total_len;
    void *msg_head;
    struct sk_buff *skb;

    /* calculate payload size */

    /* add family hdr size */
    buf_len = papi_genl_family.hdrsize;

    /* nla_total_size of each attribute */
    buf_len += nla_total_size(sizeof(*event));

    /* add space for nl msg header with the help of
       helper function and by providing payload size */
    total_len = nlmsg_total_size(buf_len);

    skb = genlmsg_new(total_len, GFP_ATOMIC);
    if (!skb) {
        QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_ERROR,
            "%s: Parameter API genlmsg_new failed\n",__func__);
        return;
    }

    msg_head = genlmsg_put(skb,
                           0, /* netlink PID */
                           0, /* sequence number */
                           &papi_genl_family,
                           0, /* flags */
                           PAPI_CMD_EVENT);
    if (!msg_head) {
        QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_ERROR,
            "%s: Parameter API genlmsg_put failed\n",__func__);
        nlmsg_free(skb);
        return;
    }

    ret_val = nla_put(skb, PAPI_ATTR_EVENT, sizeof(*event), event);
    if (ret_val != 0) {
        QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_ERROR,
            "%s: Parameter API nla_put failed\n",__func__);
        nlmsg_free(skb);
        return;
    }

    genlmsg_end(skb, msg_head);

    /* genlmsg_multicast frees skb in both succes and failure case */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0)
    ret_val = genlmsg_multicast(&papi_genl_family,
                                skb,
                                0,
                                0,
                                GFP_ATOMIC);
#else
    ret_val = genlmsg_multicast(skb,
                                0,
                                papi_genl_mcgrp[0].id,
                                GFP_ATOMIC);
#endif
    if (ret_val != 0) {
        /*QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_WARN,
            "%s: Parameter API genlmsg_multicast failed %d\n",__func__, ret_val);*/
        return;
    }
}

/* Ignore EVENT msgs */
static int papi_genl_msg_dump(struct sk_buff *skb,
                              struct netlink_callback *cb)
{
    return 0;
}

/* Ignore EVENT msgs */
static int papi_genl_msg_test(struct sk_buff *skb,
                              struct genl_info *info)
{
    QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO,
         "%s: Parameter API test msg received\n",__func__);
    return 0;
}

static struct genl_ops papi_genl_ops[] = {
    {
        .cmd = PAPI_CMD_EVENT,
        .flags = 0,
        .policy = papi_genl_policy,
        .doit = NULL,
        .dumpit = papi_genl_msg_dump,
    },
    {
        .cmd = PAPI_CMD_TEST,
        .flags = 0,
        .policy = papi_genl_policy,
        .doit = papi_genl_msg_test,
        .dumpit = NULL,
    },
};

/* Register parameter api generic netlink family */
int ath_papi_netlink_init(void)
{
    int result;

    if (!atomic_read(&registered)) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0)
        result = genl_register_family_with_ops_groups(&papi_genl_family,
                                                      papi_genl_ops,
                                                      papi_genl_mcgrp);
        if (result != 0) {
            QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_ERROR,
                "%s: ####Parameter API genl family registration failed %d\n",__func__, result);
            return result;
        }
        atomic_set(&registered, 1);
#else
        result = genl_register_family(&papi_genl_family);
        if (result != 0) {
            QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_ERROR,
                "%s: ###Parameter API genl fanily registration failed %d\n",__func__, result);
            goto err1;
        }

        result = genl_register_ops(&papi_genl_family,
                                   papi_genl_ops);
        if (result != 0) {
                QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_ERROR,
                    "%s: ###Parameter API register genl ops failed %d\n",__func__, result);
                goto err2;
        }

        result = genl_register_mc_group(&papi_genl_family,
                                        papi_genl_mcgrp);
        if (result != 0) {
            QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_ERROR,
                "%s: Parameter API genl mcast group registration failed %d\n",__func__, result);
            goto err2;
        }

        atomic_set(&registered, 1);
        return 0;

    err2:
        genl_unregister_family(&papi_genl_family);
    err1:
#endif
        return result;
    } else {
        atomic_inc(&registered);
        return 0;
    }
}

/* Deregister parameter api generic netlink family */
int ath_papi_netlink_delete(void)
{
    if (!atomic_dec_return(&registered)) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 13, 0)
        genl_unregister_mc_group(&papi_genl_family, papi_genl_mcgrp);
#endif
        genl_unregister_family(&papi_genl_family);
    }

    return 0;
}

EXPORT_SYMBOL(ath_papi_netlink_send);
EXPORT_SYMBOL(ath_papi_netlink_init);
EXPORT_SYMBOL(ath_papi_netlink_delete);


#endif /* ATH_PARAMETER_API */
