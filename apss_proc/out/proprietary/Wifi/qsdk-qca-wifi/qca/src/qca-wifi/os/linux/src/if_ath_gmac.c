/*
 * Copyright (c) 2013, 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */
/*
 * 2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/if_ether.h>
#include <linux/etherdevice.h>
#include <linux/notifier.h>
#include <linux/delay.h>

#include <athdefs.h>
#include <a_osapi.h>
#include <hif_gmac.h>

#include "ath_htc.h"

#include "if_athvar.h"
#include "version.h"

#include <linux/ethtool.h>

#define NUM_RECV_FNS            2

/* int     __gmac_dev_event(struct notifier_block  *nb, unsigned long event,  */
/* void  *dev); */
int     __gmac_recv(struct sk_buff  *skb, struct net_device  *dev,
                    struct packet_type  *type, struct net_device   *orig_dev);

extern int  __ath_attach(u_int16_t devid, struct net_device *dev, HAL_BUS_CONTEXT *bus_context, osdev_t osdev);
extern int  __ath_detach(struct net_device *dev);

extern void        MpHtcInit(void);

extern void init_wlan(void);

#define hdl_to_gmac_sc(_hdl)   (hif_gmac_softc_t *)(_hdl)

#define chk_ucast(daddr)    !is_broadcast_ether_addr((daddr))


/*****************************************************************************/
/*****************************  Globals  *************************************/
/*****************************************************************************/

const char *version = ATH_GMAC_VERSION "(Atheros/multi-bss)";
const char *dev_info = "ath_gmac";

struct packet_type       __gmac_pkt = {
    .type = __constant_htons(ETH_P_ATH),
    .func = __gmac_recv, /* GMAC receive method */
};

typedef struct ath_gmac_softc {
    struct ath_softc_net80211   aps_sc;
    struct _NIC_DEV             aps_osdev;
#ifdef CONFIG_PM
    u32                         aps_pmstate[16];
#endif
} ath_gmac_softc_t;
/* struct notifier_block    __gmac_notifier = { */
/* .notifier_call = __gmac_dev_event, */
/* }; */

/*****************************************************************************/
/*******************************  APIs  **************************************/
/*****************************************************************************/
#if 0
void
__dbg_print_buf(struct sk_buff  *skb)
{
    uint32_t  i = 0, len = skb->len;
    uint8_t  *data = skb->data;

    for ( i = 0; i < len; i++) {
        printk("%#x ", data[i]);
        if (i && !(i%16)) {
            printk("\n");
            printk("%8d: ", i);
        }
    }

    printk("\n");

}

void
__dbg_print_skb(struct sk_buff  *skb)
{
    printk("skb->len = %d\n",skb->len);
    printk("skb->dev = %#x\n",(uint32_t)skb->dev);

}

void
__dbg_print_ath(athhdr_t  *ath)
{
    uint8_t *data = (uint8_t *)ath;

    printk("ath[0] = %#x\n",data[0]);
    printk("ath[1] = %#x\n",data[1]);
    printk("ath[2] = %#x\n",data[2]);
    printk("ath[3] = %#x\n",data[3]);

    printk("ath->type.proto = %#x\n",ath->type.proto);
}

/**
 * @brief Check if the device / port  is present in the mac
 *        table
 * 
 * @param tbl
 * @param dev
 * 
 * @return uint8_t
 */
uint8_t
__gmac_chk_dev(hif_gmac_node_t  tbl[], struct net_device  *dev)
{
    uint32_t  i = 0;

    for (i = 0; i < GMAC_TBL_SIZE; i++) {
        if(tbl[i].dev == dev)
            return A_TRUE;
    }

    return A_FALSE;
}

/**
 * @brief Device event notifier
 * 
 * @param nb
 * @param event
 * @param dev
 * 
 * @return int
 */
int
__gmac_dev_event(struct notifier_block  *nb, unsigned long event, void  *dev)
{
    hif_gmac_softc_t  *sc = __gmac_pkt.af_packet_priv;

    switch (event) {
    case NETDEV_DOWN:
        /**
         * XXX: how to handle multiple magpies
         */
        if(__gmac_chk_dev(sc->node_tbl, dev))
            app_dev_removed(sc->app_reg[sc->app_cur]);
        break;
    default:
        break;
    }
    return NOTIFY_DONE;
}
#endif

/*
 * Module glue.
 */

int
ath_ioctl_ethtool(struct ath_softc *sc, int cmd, void *addr)
{
    struct ethtool_drvinfo info = {0};

    if (cmd != ETHTOOL_GDRVINFO)
        return -EOPNOTSUPP;

    info.cmd = cmd;
    
    strncpy(info.driver, version, (sizeof(info.driver) - 1));
    strncpy(info.version, dev_info, (sizeof(info.version) - 1));

    return _copy_to_user(addr, &info, sizeof(info)) ? -EFAULT : 0;
}
/**
 * @brief Copy the Ethernet header from the skb into the
 *        destination
 */

void
__gmac_copy_ethhdr(ethhdr_t   *dst, struct sk_buff  *skb)
{
    ethhdr_t           *src = eth_hdr(skb);
    /**
     * Build the ethernet header for Xmit
     */
    IEEE80211_ADDR_COPY(dst->h_dest, src->h_source);
    IEEE80211_ADDR_COPY(dst->h_source, skb->dev->dev_addr);

    dst->h_proto  = qdf_htons(ETH_P_ATH);
}

/**
 * @brief Generate a index into the MAC table
 * 
 * @param mac
 * 
 * @return uint32_t
 */
static inline uint32_t
__gmac_hash_idx(uint8_t *mac)
{
    uint64_t x;
    
    x = mac[0];
    x = (x << 2) ^ mac[1];
    x = (x << 2) ^ mac[2];
    x = (x << 2) ^ mac[3];
    x = (x << 2) ^ mac[4];
    x = (x << 2) ^ mac[5];
    
    x ^= x >> 8;
    
    return x & (GMAC_TBL_SIZE - 1);
}

/**
 * @brief this returns the entry in the mac table
 * 
 * @param tbl
 * @param smac
 * 
 * @return hif_gmac_softc_t*
 */
static inline hif_gmac_node_t *
__gmac_qry_tbl(uint8_t  *smac)
{
    hif_gmac_softc_t  *sc = __gmac_pkt.af_packet_priv;

    return &sc->node_tbl[__gmac_hash_idx(smac)];
}

/**
 * @brief Protocol drivers Receive function
 * 
 * @param skb
 * @param dev
 * @param type
 * @param orig_dev
 * 
 * @return int
 */
int
__gmac_recv(struct sk_buff  *skb, struct net_device  *dev,
            struct packet_type  *type, struct net_device   *orig_dev)
{
    athhdr_t           *ath;     
    ethhdr_t           *eth;
    hif_gmac_node_t    *node = NULL;
    uint32_t          idx = 0;
    hif_gmac_softc_t   *sc = __gmac_pkt.af_packet_priv;

    if (skb_shared(skb)) 
        skb = qdf_nbuf_unshare(skb);

    eth = eth_hdr(skb); /* Linux specific */
    ath = ath_hdr(skb);

    /* printk("%s\n",__func__); */
    /**
     * Query MAC Table
     */
    node = __gmac_qry_tbl(eth->h_source);
    
    idx = ((node->hdr.ath.type.proto == ath->type.proto) &&
            chk_ucast(eth->h_dest));

    sc->recv_fn[idx](skb, node, ath->type.proto);

    return 0;
}



void *
__gmac_create_wlan_dev(void)
{
    struct net_device  *dev;
    ath_gmac_softc_t  *sc = NULL;

    dev = alloc_netdev(sizeof(struct ath_gmac_softc), "wifi%d", ether_setup);
    if (dev == NULL) {
        printk(KERN_ERR "ath_gmac: no memory for device state\n");
        goto done;
    }

    sc = netdev_priv(dev);
    
    memset(sc, 0, sizeof(struct ath_gmac_softc));

    sc->aps_osdev.netdev = dev;
    sc->aps_osdev.bdev = sc;
    sc->aps_osdev.wmi_dev = sc;

done:    
    return (void *)&sc->aps_osdev;
}
/**************************************/
/******** Exported functions **********/
/**************************************/

/**
 * @brief Do a device discovery
 * 
 * @param skb
 * @param node
 * 
 * @return int
 */
int
__gmac_dev_discv(struct sk_buff   *skb, hif_gmac_node_t   *node)
{
     athhdr_t    *ath = &node->hdr.ath;     

    /**
     * If there exist a older entry,
     * XXX: HTC notification, Reference count on the device
     */
    if(node->dev == skb->dev)
        return 1;

    __gmac_copy_ethhdr(&node->hdr.eth, skb);

    ath->type.proto = ATH_P_MAGBOOT;
    node->dev = skb->dev;
    
    node->os_hdl = __gmac_create_wlan_dev();

    return 0;
}


void
__gmac_attach(void *  arg)
{
    osdev_t   os_hdl = ((hif_gmac_node_t *)arg)->os_hdl;
    HAL_BUS_CONTEXT bus_context;
    struct ath_gmac_softc *sc = netdev_priv(((osdev_t)os_hdl)->netdev);
    struct net_device *dev = sc->aps_osdev.netdev;

    /* XXX:BUS specific stuff not needed for pseudo devices */
    bus_context.bc_info.bc_tag = NULL;
    bus_context.bc_info.cal_in_flash = 0;
    bus_context.bc_handle = (void*) NULL;
    bus_context.bc_bustype = HAL_BUS_TYPE_GMAC;

    /* Register thread callbacks */
    ath_register_htc_thread_callback(&sc->aps_osdev);
   
    /* Send HIF layer device inserted event  */
    hif_gmac_dev_attach((hif_gmac_node_t *)arg);

    if(__ath_attach(DEVID_MAGPIE_MERLIN, dev, &bus_context, &sc->aps_osdev))
        printk(KERN_ERR "%s: __ath_attach fail!\n", __func__);
}

void
__gmac_detach(hif_gmac_node_t  *node)
{
    osdev_t  os_hdl = (osdev_t)(node->os_hdl);
    struct net_device *dev = os_hdl->netdev;

    ath_htc_thread_stopping(os_hdl);
    
    __ath_detach(dev);

    hif_gmac_dev_detach(node);

    ath_terminate_htc_thread(NULL);

    free_netdev(dev);
}

void
__gmac_defer_attach(hif_gmac_node_t  *node)
{
    gmac_os_init_work(&node->work, __gmac_attach, node);
    gmac_os_sched_work(&node->work);
}

/* @brief HIF GMAC init module */
int
os_gmac_init(void)
{
    hif_gmac_softc_t  *sc;

    init_wlan();
    
    sc = __gmac_pkt.af_packet_priv  = hif_gmac_init();
    
    sc->attach          = __gmac_defer_attach;
    sc->detach          = __gmac_detach;
    sc->discv  = __gmac_dev_discv;

    dev_add_pack(&__gmac_pkt);

    fwd_module_init();

    MpHtcInit();

    if (ath_create_htc_thread(NULL) != 0)
        printk(KERN_ERR "%s: create thread fail\n", __func__);

    /* register_netdevice_notifier(&__gmac_notifier); */

    return 0;
}

void
gmac_os_defer_func(struct work_struct *work)
{
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,19)
    return;
#else
    gmac_os_work_t  *ctx = container_of(work, gmac_os_work_t, work);
    ctx->fn(ctx->arg);
#endif
}


void
os_gmac_exit(void)
{
    hif_gmac_exit(__gmac_pkt.af_packet_priv);

    printk("%s\n",__func__); 

    dev_remove_pack(&__gmac_pkt);

    /* unregister_netdevice_notifier(&__gmac_notifier); */
}


module_init(os_gmac_init);
module_exit(os_gmac_exit);
