/*
 * Copyright (c) 2013,2015 Qualcomm Atheros, Inc..
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>

unsigned int print_rssi = 0;
module_param(print_rssi, uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(print_rssi,
"prints rssi");
EXPORT_SYMBOL(print_rssi);

extern void (*external_tx_complete)(struct sk_buff *skb);

void __external_tx_complete(struct sk_buff *skb)
{
    if(print_rssi) {
        printk("rssi %d chn %d \n",skb->data[4],skb->data[2]);
    }
    dev_kfree_skb_any(skb);

}

static int __tm_init_module(void)
{
    external_tx_complete = __external_tx_complete;
    printk("%s \n", __func__);
    return 0;
}

static void __tm_exit_module(void)
{
    external_tx_complete = ((void*)0);
    printk("%s \n", __func__);
}

module_init(__tm_init_module);
module_exit(__tm_exit_module);
