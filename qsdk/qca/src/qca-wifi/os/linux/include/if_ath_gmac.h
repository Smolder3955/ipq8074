/*
 * Copyright (c) 2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#ifndef __IF_ATH_GMAC_H
#define __IF_ATH_GMAC_H
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <qdf_types.h>

typedef struct net_device       os_gmac_dev_t;

#define DEVID_MAGPIE_MERLIN     0x002A
#define HAL_BUS_TYPE_GMAC       0x03

/**
 * @brief Transmit the packet from the given interface
 * 
 * @param skb
 * @param hdr
 * @param dev
 * 
 * @return int
 */
static inline int
os_gmac_xmit(struct sk_buff *skb, os_gmac_dev_t *dev)
{
    skb->dev = dev;

    nbuf_debug_del_record(skb);
    return (dev_queue_xmit(skb)==0) ? QDF_STATUS_SUCCESS: QDF_STATUS_E_FAILURE;
}



#endif

