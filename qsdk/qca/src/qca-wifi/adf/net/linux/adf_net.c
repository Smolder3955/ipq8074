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
 * Copyright (c) 2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/slab.h>

#if LINUX_VERSION_CODE  <= KERNEL_VERSION(2,6,19)
#include <linux/vmalloc.h>
#endif

#if LINUX_VERSION_CODE  <= KERNEL_VERSION(3,3,8)
#include <asm/system.h>
#else
#if defined (__LINUX_MIPS32_ARCH__) || defined (__LINUX_MIPS64_ARCH__)
#include <asm/dec/system.h>
#else
#if !defined ( __i386__) && !defined (__LINUX_POWERPC_ARCH__)
#include <asm/system.h>
#endif
#endif
#endif

#include <linux/netdevice.h>
#include <linux/stddef.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/interrupt.h>
#include <linux/ip.h>
#ifdef LIMIT_MTU_SIZE
#include <net/icmp.h>
#include <linux/icmp.h>
#include <net/route.h>
#endif 
#include <asm/checksum.h>

#if (LINUX_VERSION_CODE != KERNEL_VERSION(3,3,8)) && (LINUX_VERSION_CODE < KERNEL_VERSION(3,1,0) || (LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0) && LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0)))
#include <linux/if_vlan.h>
#else
#include <../net/8021q/vlan.h>
#endif

#include <asm/irq.h>
#include <asm/io.h>
#include <net/sch_generic.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <net/iw_handler.h>
#include <adf_net.h>
#include <linux/if_arp.h>
#include <adf_os_util.h>
#include <adf_os_types_pvt.h>


#ifdef LIMIT_MTU_SIZE
#define LIMITED_MTU 1368
static struct net_device __fake_net_device = {
    .hard_header_len    = ETH_HLEN
};

static struct rtable __fake_rtable = {
    .u = {
        .dst = {
            .__refcnt       = ATOMIC_INIT(1),
            .dev            = &__fake_net_device,
            .path           = &__fake_rtable.u.dst,
            .metrics        = {[RTAX_MTU - 1] = 1500},
        }
    },
    .rt_flags   = 0,
};
#endif 


/***************************Internal Functions********************************/


/**
 * @brief open the device
 * 
 * @param netdev
 * 
 * @return int
 */
static int
__adf_net_open(struct net_device *netdev)
{
    a_status_t status;
    
    if((status = adf_drv_open(netdev)))
        return __a_status_to_os(status);

    netdev->flags |= IFF_RUNNING;
    return 0;
}
/**
 * @brief populate device desgtructor
 * 
 * @param netdev
 * 
 * @return void
 */
static void
__adf_net_free_netdev(struct net_device *netdev)
{
    free_netdev(netdev);
    return ;
}

/**
 * @brief
 * @param netdev
 * 
 * @return int
 */
static int
__adf_net_stop(struct net_device *netdev)
{
    adf_drv_close(netdev);

    netdev->flags &= ~IFF_RUNNING;
    //netif_carrier_off(netdev);
    netif_stop_queue(netdev);
    return 0;
}

static int
__adf_net_start_tx(struct sk_buff  *skb, struct net_device  *netdev)
{
    __adf_softc_t  *sc = netdev_to_softc(netdev);
    int err = 0;
    int tag = 0;

    if(unlikely((skb->len <= ETH_HLEN))) {
        printk("ADF_NET:Bad skb len %d\n", skb->len);
        dev_kfree_skb_any(skb);
        return NETDEV_TX_OK;
    }
    tag = __adf_net_get_vlantag(skb);
    if(unlikely(sc->vid && tag  == A_STATUS_OK ))
    {
        if(unlikely(!sc->vlgrp || ((sc->vid & VLAN_VID_MASK) != __adf_net_get_vlanvid(tag ))))
            err = A_STATUS_FAILED;
    }


    if(!err)
	    err = adf_drv_tx(netdev, skb);

    switch (err) {
    case A_STATUS_OK:
        err = NETDEV_TX_OK;
        netdev->trans_start = jiffies;
        break;
    case A_STATUS_EBUSY:
        err = NETDEV_TX_BUSY;
        break;
    default:
        err = NETDEV_TX_OK;
        dev_kfree_skb_any(skb);
        break;
    }

    return err;
}

/**
 * @brief add the VLAN ID
 * 
 * @param dev
 * @param vid
 */
#if LINUX_VERSION_CODE > KERNEL_VERSION(3,2,0)
static int
#else
static void
#endif
__adf_net_vlan_add(struct net_device  *dev,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
				__be16 proto,
#endif
			unsigned short vid)
{
    __adf_softc_t   *sc = netdev_to_softc(dev);

    if(!sc->vlgrp)
#if LINUX_VERSION_CODE > KERNEL_VERSION(3,2,0)
        return EINVAL;
#else
        return;
#endif
    sc->vid = vid & VLAN_VID_MASK;	
#if LINUX_VERSION_CODE > KERNEL_VERSION(3,2,0)
    return 0;
#endif
}

#if (LINUX_VERSION_CODE != KERNEL_VERSION(3,3,8)) && (LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0) && LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0))
static inline void vlan_group_set_device(struct vlan_group *vg,
                                         u16 vlan_id,
                                         struct net_device *dev)
{
        struct net_device **array;
        if (!vg)
                return;
        array = vg->vlan_devices_arrays[vlan_id / VLAN_GROUP_ARRAY_PART_LEN];
        array[vlan_id % VLAN_GROUP_ARRAY_PART_LEN] = dev;
}
#endif

/**
 * @brief delete the VLAN ID
 * 
 * @param dev
 * @param vid
 */
#if LINUX_VERSION_CODE > KERNEL_VERSION(3,2,0)
static int
#else
static void
#endif
__adf_net_vlan_del(struct net_device  *dev,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
				__be16 proto,
#endif
				unsigned short vid)
{
    __adf_softc_t        *sc = netdev_to_softc(dev);
#if LINUX_VERSION_CODE > KERNEL_VERSION(3,10,0)
	struct vlan_dev_priv *vlan = vlan_dev_priv(dev);
#endif

    if(!sc->vlgrp)
#if LINUX_VERSION_CODE > KERNEL_VERSION(3,2,0)
        return EINVAL;
#else
        return;
#endif
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,20)
    sc->vlgrp->vlan_devices[vid] = NULL;
#elif LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
    vlan_group_set_device(sc->vlgrp, vid, NULL);
#else
    vlan_group_set_device(sc->vlgrp, vlan->vlan_proto, vid, NULL);
#endif
    sc->vid = 0;
    sc->vlgrp = NULL ;
#if LINUX_VERSION_CODE > KERNEL_VERSION(3,2,0)
    return 0;
#endif
}



/**
 * @brief retrieve the vlan tag from skb
 * 
 * @param skb
 * @param tag
 * 
 * @return a_status_t (ENOTSUPP for tag not present)
 */
#define VLAN_PRI_SHIFT  13
#define VLAN_PRI_MASK   7
uint32_t
__adf_net_get_vlantag(struct sk_buff *skb)
{

    if(!vlan_tx_tag_present(skb))
        return A_STATUS_ENOTSUPP;

    return vlan_tx_tag_get(skb);
}
EXPORT_SYMBOL(__adf_net_get_vlantag);



#define VLAN_PRI_SHIFT  13
#define VLAN_PRI_MASK   7

uint32_t
__adf_net_get_vlanvid(uint32_t tag )
{
    return tag & VLAN_VID_MASK;
}
EXPORT_SYMBOL(__adf_net_get_vlanvid);


static void
__adf_net_vlan_register(struct net_device *dev, struct vlan_group *grp)
{
    __adf_softc_t  *sc = netdev_to_softc(dev);

    sc->vlgrp  = grp;

}
static struct net_device_stats *
__adf_net_get_stats(struct net_device *dev)
{
    return &netdev_to_stats(dev);
}

#ifndef ADF_NET_IOCTL_SUPPORT 
 /*When ioctl support is enabled adf_net_ioclt.c will have the function*/
int
__adf_net_wifi_ioctl(struct net_device *netdev, struct ifreq *ifr, int cmd)
{
    printk(" ADF_NET_IOCTL_SUPPORT is not On \n");

    return -EINVAL;
}

int
__adf_net_vap_ioctl(struct net_device *netdev, struct ifreq *ifr, int cmd)
{
    printk(" ADF_NET_IOCTL_SUPPORT is not On \n");

    return -EINVAL;
}

int
__adf_net_eth_ioctl(struct net_device *netdev, struct ifreq *ifr, int cmd)
{
    printk(" ADF_NET_IOCTL_SUPPORT is not On \n");

    return -EINVAL;
}
int
__adf_net_set_wifiaddr(struct net_device *netdev, void *addr)
{


    int error = EINVAL;

    printk(" ADF_NET_IOCTL_SUPPORT is not On \n");
    return error;

}

int
__adf_net_set_vapaddr(struct net_device *netdev, void *addr)
{

    int error = EINVAL;

    printk(" ADF_NET_IOCTL_SUPPORT is not On \n");
    return error;

}
#define __adf_net_iwget_wifi()      NULL
#define __adf_net_iwget_vap()       NULL

#else
extern  int __adf_net_set_wifiaddr(struct net_device *netdev, void *addr);
extern  int __adf_net_set_vapaddr(struct net_device *netdev, void *addr);
extern  int __adf_net_wifi_ioctl(struct net_device *netdev, struct ifreq *ifr, 
                                 int cmd);
extern  int __adf_net_vap_ioctl(struct net_device *netdev, struct ifreq *ifr, 
                                int cmd);
extern  int __adf_net_eth_ioctl(struct net_device *netdev, struct ifreq *ifr, 
                                int cmd);
struct iw_handler_def *     __adf_net_iwget_wifi(void);
struct iw_handler_def *     __adf_net_iwget_vap(void);
#endif

/********************************EXPORTED***********************/


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29)
struct net_device_ops  __adf_net_wifidev_ops = {
    .ndo_open               = __adf_net_open,
    .ndo_stop               = __adf_net_stop,
    .ndo_start_xmit         = __adf_net_start_tx,
    .ndo_do_ioctl           = __adf_net_wifi_ioctl,
    .ndo_get_stats          = __adf_net_get_stats,
    .ndo_set_mac_address    = __adf_net_set_wifiaddr,
};

struct net_device_ops  __adf_net_vapdev_ops = {
    .ndo_open               = __adf_net_open,
    .ndo_stop               = __adf_net_stop,
    .ndo_start_xmit         = __adf_net_start_tx,
    .ndo_do_ioctl           = __adf_net_vap_ioctl,
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,1,0)
    .ndo_vlan_rx_register   = __adf_net_vlan_register,
#endif
    .ndo_vlan_rx_add_vid    = __adf_net_vlan_add,
    .ndo_vlan_rx_kill_vid   = __adf_net_vlan_del,
    .ndo_get_stats          = __adf_net_get_stats,
    .ndo_set_mac_address    = __adf_net_set_vapaddr,
};

struct net_device_ops  __adf_net_ethdev_ops = {
    .ndo_open               = __adf_net_open,
    .ndo_stop               = __adf_net_stop,
    .ndo_start_xmit         = __adf_net_start_tx,
    .ndo_do_ioctl           = __adf_net_eth_ioctl,
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,1,0)
    .ndo_vlan_rx_register   = __adf_net_vlan_register,
#endif
    .ndo_vlan_rx_add_vid    = __adf_net_vlan_add,
    .ndo_vlan_rx_kill_vid   = __adf_net_vlan_del,
    .ndo_get_stats          = __adf_net_get_stats,
};
#endif

/**
 * @brief Create a Wifi Networking device
 * 
 * @param hdl
 * @param op
 * @param info
 * 
 * @return adf_net_handle_t
 */
adf_net_handle_t
__adf_net_create_wifidev(adf_drv_handle_t        hdl, adf_dev_sw_t   *op,
                         adf_net_dev_info_t     *info, void          *wifi_cfg)
{
    __adf_softc_t      *sc      = NULL;
    struct net_device  *netdev  = NULL;
    int                 error   = 0;

    netdev = alloc_netdev(sizeof(struct __adf_softc), info->if_name,
                          ether_setup);
    
    if (!netdev) return NULL;

    sc              = netdev_to_softc(netdev);
    sc->netdev      = netdev;
    sc->sw          = *op;
    sc->drv_hdl     = hdl;
    sc->vlgrp       = NULL; /*Not part of any VLAN*/
    sc->vid         = 0;
    sc->cfg_api     = wifi_cfg; 

    netdev->watchdog_timeo      = ADF_DEF_TX_TIMEOUT * HZ;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29)
    netdev->netdev_ops          = &__adf_net_wifidev_ops;
#else
    netdev->open                = __adf_net_open;
    netdev->stop                = __adf_net_stop;
    netdev->hard_start_xmit     = __adf_net_start_tx;
    netdev->do_ioctl            = __adf_net_wifi_ioctl;
    netdev->get_stats           = __adf_net_get_stats;
    netdev->set_mac_address     = __adf_net_set_wifiaddr;
#endif
    
    netdev->destructor          = __adf_net_free_netdev;
    netdev->hard_header_len     = info->header_len ;
    netdev->wireless_handlers   = __adf_net_iwget_wifi();
    netdev->type                = ARPHRD_IEEE80211; /* WLAN device */

    adf_os_assert(is_valid_ether_addr(info->dev_addr));

    memcpy(netdev->dev_addr, info->dev_addr, ADF_NET_MAC_ADDR_MAX_LEN);
    memcpy(netdev->perm_addr, info->dev_addr, ADF_NET_MAC_ADDR_MAX_LEN);

    /**
     * make sure nothing's on before open
     */
    netif_stop_queue(netdev);

    error = register_netdev(netdev) ;

    adf_os_assert(!error);

    return sc;
}
EXPORT_SYMBOL(__adf_net_create_wifidev);

/**
 * @brief Create a VAP networking device
 * 
 * @param hdl
 * @param op
 * @param info
 * 
 * @return adf_net_handle_t
 */
adf_net_handle_t
__adf_net_create_vapdev(adf_drv_handle_t        hdl, adf_dev_sw_t   *op,
                        adf_net_dev_info_t     *info, void          *vap_cfg)
{
    __adf_softc_t      *sc      = NULL;
    struct net_device  *netdev  = NULL;
    int                 error   = 0;

    netdev = alloc_netdev(sizeof(struct __adf_softc), info->if_name,
                          ether_setup);
    
    if (!netdev) return NULL;

    sc              = netdev_to_softc(netdev);
    sc->netdev      = netdev;
    sc->sw          = *op;
    sc->drv_hdl     = hdl;
    sc->vlgrp       = NULL; /*Not part of any VLAN*/
    sc->vid         = 0;
    sc->cfg_api     = vap_cfg; 
    sc->unit        = info->unit;

    netdev->watchdog_timeo      = ADF_DEF_TX_TIMEOUT * HZ;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
    netdev->features           |= ( NETIF_F_HW_VLAN_FILTER | 
                                    NETIF_F_HW_VLAN_RX | 
                                    NETIF_F_HW_VLAN_TX );
#else
    netdev->features           |= ( NETIF_F_HW_VLAN_CTAG_FILTER | 
                                    NETIF_F_HW_VLAN_CTAG_RX | 
                                    NETIF_F_HW_VLAN_CTAG_TX );
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29)
    netdev->netdev_ops          = &__adf_net_vapdev_ops;
#else
    netdev->open                = __adf_net_open;
    netdev->stop                = __adf_net_stop;
    netdev->hard_start_xmit     = __adf_net_start_tx;
    netdev->do_ioctl            = __adf_net_vap_ioctl;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,1,0)
    netdev->vlan_rx_register    = __adf_net_vlan_register;
#endif
    netdev->vlan_rx_add_vid     = __adf_net_vlan_add;
    netdev->vlan_rx_kill_vid    = __adf_net_vlan_del;
    netdev->get_stats           = __adf_net_get_stats;
    netdev->set_mac_address     = __adf_net_set_vapaddr;
#endif
    
    netdev->destructor          = __adf_net_free_netdev;
    netdev->hard_header_len     = info->header_len ;
    netdev->wireless_handlers   = __adf_net_iwget_vap();

    if(!is_valid_ether_addr(info->dev_addr)){
        printk("ADF_NET:invalid MAC address\n");
        error = EINVAL;
    }
    
    adf_os_assert(is_valid_ether_addr(info->dev_addr));

    memcpy(netdev->dev_addr, info->dev_addr, ADF_NET_MAC_ADDR_MAX_LEN);
    memcpy(netdev->perm_addr, info->dev_addr, ADF_NET_MAC_ADDR_MAX_LEN);

    /**
     * make sure nothing's on before open
     */
    netif_stop_queue(netdev);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
    if(rtnl_is_locked())
        error = register_netdevice(netdev) ;
    else
#endif        
        error = register_netdev(netdev) ;
    
    adf_os_assert(!error);

    return (adf_net_handle_t)sc;
}
EXPORT_SYMBOL(__adf_net_create_vapdev);
  

/**
 * @brief Create a Eth networking device
 * 
 * @param hdl
 * @param op
 * @param info
 * 
 * @return adf_net_handle_t
 */
adf_net_handle_t
__adf_net_create_ethdev(adf_drv_handle_t        hdl, adf_dev_sw_t   *op,
                        adf_net_dev_info_t     *info)
{
    __adf_softc_t      *sc      = NULL;
    struct net_device  *netdev  = NULL;
    int                 error   = 0;

    netdev = alloc_netdev(sizeof(struct __adf_softc), info->if_name,
                          ether_setup);
    
    if (!netdev) return NULL;

    sc              = netdev_to_softc(netdev);
    sc->netdev      = netdev;
    sc->sw          = *op;
    sc->drv_hdl     = hdl;
    sc->vlgrp       = NULL; /*Not part of any VLAN*/
    sc->vid         = 0;
    sc->cfg_api     = NULL; 

    netdev->watchdog_timeo      = ADF_DEF_TX_TIMEOUT * HZ;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
    netdev->features           |= ( NETIF_F_HW_VLAN_FILTER | 
                                    NETIF_F_HW_VLAN_RX | 
                                    NETIF_F_HW_VLAN_TX );
#else
    netdev->features           |= ( NETIF_F_HW_VLAN_CTAG_FILTER | 
                                    NETIF_F_HW_VLAN_CTAG_RX | 
                                    NETIF_F_HW_VLAN_CTAG_TX );
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29)
    netdev->netdev_ops          = &__adf_net_vapdev_ops;
#else
    netdev->open                = __adf_net_open;
    netdev->stop                = __adf_net_stop;
    netdev->hard_start_xmit     = __adf_net_start_tx;
    netdev->do_ioctl            = __adf_net_eth_ioctl;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,1,0)
    netdev->vlan_rx_register    = __adf_net_vlan_register;
#endif
    netdev->vlan_rx_add_vid     = __adf_net_vlan_add;
    netdev->vlan_rx_kill_vid    = __adf_net_vlan_del;
    netdev->get_stats           = __adf_net_get_stats;
#endif
    
    netdev->destructor          = __adf_net_free_netdev;
    netdev->hard_header_len     = info->header_len ;

    adf_os_assert(!is_valid_ether_addr(info->dev_addr));

    memcpy(netdev->dev_addr, info->dev_addr, ADF_NET_MAC_ADDR_MAX_LEN);
    memcpy(netdev->perm_addr, info->dev_addr, ADF_NET_MAC_ADDR_MAX_LEN);

    /**
     * make sure nothing's on before open
     */
    netif_stop_queue(netdev);

    error = register_netdev(netdev) ;

    adf_os_assert(!error);

    return sc;
}
EXPORT_SYMBOL(__adf_net_create_ethdev);
/**
 * @brief remove the device
 * @param hdl
 */
a_status_t
__adf_net_delete_dev(adf_net_handle_t hdl)
{    
    __adf_softc_t *sc = hdl_to_softc(hdl);

    __adf_net_stop(sc->netdev);
    
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
    if (rtnl_is_locked())
        unregister_netdevice(sc->netdev);
    else
#endif        
        unregister_netdev(sc->netdev);
    
    return A_STATUS_OK;
}
EXPORT_SYMBOL(__adf_net_delete_dev);


int
__adf_net_dev_exist_by_name(const char *dev_name)
{
    struct net_device *dev;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
    dev = dev_get_by_name(dev_name);
#else
    dev = dev_get_by_name(&init_net, dev_name);
#endif
    if (dev)
    {
        dev_put(dev);
        return 1;
    }
    else
        return 0;
}
EXPORT_SYMBOL(__adf_net_dev_exist_by_name);

/**
 * @brief this adds a IP ckecksum in the IP header of the packet
 * @param skb
 */
static void
__adf_net_ip_cksum(struct sk_buff *skb)
{
    struct iphdr   *ih  = {0};
#ifdef ADF_OS_DEBUG
    struct skb_shared_info  *sh = skb_shinfo(skb);
#endif
    adf_os_assert(sh->nr_frags == 0);

    ih = (struct iphdr *)(skb->data + sizeof(struct ethhdr));
    ih->check = 0;
    ih->check = ip_fast_csum((unsigned char *)ih, ih->ihl);
}

a_status_t
__adf_net_indicate_packet(adf_net_handle_t hdl, struct sk_buff *skb,
                          uint32_t len)
{
    struct net_device *netdev   = hdl_to_netdev(hdl);
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,1,0)
    __adf_softc_t  *sc          = hdl_to_softc(hdl);
#endif
    /**
     * For pseudo devices IP checksum has to computed
     */
    if(adf_os_unlikely(skb->ip_summed == CHECKSUM_UNNECESSARY))
        __adf_net_ip_cksum(skb);

    /**
     * also pulls the ether header
     */
    skb->protocol           =   eth_type_trans(skb, netdev);
    skb->dev                =   netdev;
    netdev->last_rx         =   jiffies;
#ifdef LIMIT_MTU_SIZE

    if (skb->len >=  LIMITED_MTU) {
        skb->h.raw = skb->nh.raw = skb->data;

        skb->dst = (struct dst_entry *)&__fake_rtable;
        skb->pkt_type = PACKET_HOST;
        dst_hold(skb->dst);

#if 0
        printk("addrs : sa : %x : da:%x\n", skb->nh.iph->saddr, skb->nh.iph->daddr);
        printk("head : %pK tail : %pK iph %pK %pK\n", skb->head, skb->tail,
                skb->nh.iph, skb->mac.raw);
#endif 

        icmp_send(skb, ICMP_DEST_UNREACH, ICMP_FRAG_NEEDED, htonl(LIMITED_MTU - 4 ));
        dev_kfree_skb_any(skb);
        return A_STATUS_OK;
    }
#endif


    
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,1,0)
    if(sc->vlgrp) {
        vlan_hwaccel_receive_skb(skb,sc->vlgrp, sc->vid);
    }
    else 
#endif
    if (in_irq()) {
        netif_rx(skb);
    }
    else {
        netif_receive_skb(skb);
    }

    return A_STATUS_OK;
}
EXPORT_SYMBOL(__adf_net_indicate_packet);

a_status_t
__adf_net_dev_tx(adf_net_handle_t hdl, struct sk_buff  *skb)
{
    struct net_device   *netdev = hdl_to_netdev(hdl);

    if(unlikely(!netdev)){
        printk("ADF_NET:netdev not found\n");
        adf_os_assert(0);
    }

    skb->dev = netdev;

    nbuf_debug_del_record(skb);
    dev_queue_xmit(skb);

    return A_STATUS_OK;
}
EXPORT_SYMBOL(__adf_net_dev_tx);

/**
 * @brief call the init of the hdl passed on
 * 
 * @param hdl
 * 
 * @return a_uint32_t
 */
a_status_t
__adf_net_dev_open(adf_net_handle_t hdl)
{
    dev_open(hdl_to_netdev(hdl));
    return A_STATUS_OK;
}
EXPORT_SYMBOL(__adf_net_dev_open);

a_status_t
__adf_net_dev_close(adf_net_handle_t hdl)
{
    dev_close(hdl_to_netdev(hdl));
    
    return A_STATUS_OK;
}
EXPORT_SYMBOL(__adf_net_dev_close);

const uint8_t *
__adf_net_ifname(adf_net_handle_t  hdl)
{
    struct net_device *netdev = hdl_to_netdev(hdl);

    return (netdev->name);
}
EXPORT_SYMBOL(__adf_net_ifname);

a_status_t             
__adf_net_fw_mgmt_to_app(adf_net_handle_t hdl, struct sk_buff *skb, 
                         uint32_t len)
{
    struct net_device *netdev = hdl_to_netdev(hdl);
    skb->dev = netdev;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,20)
    skb->mac.raw = skb->data;
#else
    skb_reset_mac_header(skb);
#endif
    skb->ip_summed = CHECKSUM_NONE;
    skb->pkt_type = PACKET_OTHERHOST;
    skb->protocol = __constant_htons(0x0019);  /* ETH_P_80211_RAW */
         
    netif_rx(skb);

    return A_STATUS_OK;
}
EXPORT_SYMBOL(__adf_net_fw_mgmt_to_app);

a_status_t
__adf_net_send_wireless_event(adf_net_handle_t hdl, adf_net_wireless_event_t what, 
                              void *data, size_t data_len)
{
    wireless_send_event(hdl_to_netdev(hdl),
                        what,
                        (union iwreq_data *)data,
                        NULL);
    return A_STATUS_OK;
}
EXPORT_SYMBOL(__adf_net_send_wireless_event);

a_status_t 
__adf_net_indicate_vlanpkt(adf_net_handle_t hdl, struct sk_buff *skb, 
                           uint32_t len, adf_net_vid_t *vid)
{
    __adf_softc_t   *sc = hdl_to_softc(hdl);
    struct net_device *netdev = hdl_to_netdev(hdl);

    skb->protocol           =   eth_type_trans(skb, netdev);
    skb->dev                =   netdev;
    netdev->last_rx         =   jiffies;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,1,0)
    if(sc->vlgrp) {
        vlan_hwaccel_receive_skb(skb,sc->vlgrp, vid->val);
    } else {
        (in_irq() ? netif_rx(skb) : netif_receive_skb(skb));
    }
#else
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
    if(sc->vlgrp)       __vlan_hwaccel_put_tag(skb, sc->vid);
#else
    if(sc->vlgrp)       __vlan_hwaccel_put_tag(skb, htons(ETH_P_8021Q), sc->vid);
#endif
    if (in_irq())  	netif_rx(skb);
    else netif_receive_skb(skb);
#endif

    return A_STATUS_OK;
}
EXPORT_SYMBOL(__adf_net_indicate_vlanpkt);



/* Bitmap array maintained for WLAN UNIT Index */
u_int8_t wlan_units[32];     /* enough for 256 */

/*
* Allocate a new unit number.  If the map is full return -1;
* otherwise the allocate unit number is returned.
*/
#ifndef __ADF_NBBY
#define __ADF_NBBY       (8)  /* number of bits/byte */
#endif
/* Bit map related macros. */
#define __adf_setbit(a,i) ((a)[(i)/__ADF_NBBY] |= 1<<((i)%__ADF_NBBY))
#define __adf_clrbit(a,i) ((a)[(i)/__ADF_NBBY] &= ~(1<<((i)%__ADF_NBBY)))
#define __adf_isset(a,i)  ((a)[(i)/__ADF_NBBY] & (1<<((i)%__ADF_NBBY)))
#define __adf_isclr(a,i)  (((a)[(i)/__ADF_NBBY] & (1<<((i)%__ADF_NBBY))) == 0)

int
__adf_net_new_wlanunit(void)
{
#define N(a)    (sizeof(a)/sizeof(a[0]))
    u_int unit;
    u_int8_t b;
    int i;

    /* NB: covered by rtnl_lock */
    unit = 0;
    for (i = 0; i < N(wlan_units) && wlan_units[i] == 0xff; i++)
        unit += __ADF_NBBY;
    if (i == N(wlan_units))
        return -1;
    for (b = wlan_units[i]; b & 1; b >>= 1)
        unit++;
    __adf_setbit(wlan_units, unit);

    return unit;
#undef N
}
EXPORT_SYMBOL(__adf_net_new_wlanunit);

/*
* Check if the specified unit number is available and, if
* so, mark it in use.  Return 1 on success, 0 on failure.
*/
int
__adf_net_alloc_wlanunit(u_int unit)
{
    /* NB: covered by rtnl_lock */
    if (unit < sizeof(wlan_units)*__ADF_NBBY && __adf_isclr(wlan_units, unit))
    {
        __adf_setbit(wlan_units, unit);
        return 1;
    }
    else
        return 0;
}
EXPORT_SYMBOL(__adf_net_alloc_wlanunit);

/*
* Reclaim the specified unit number.
*/
void
__adf_net_delete_wlanunit(u_int unit)
{
    /* NB: covered by rtnl_lock */
    __adf_clrbit(wlan_units, unit);
}
EXPORT_SYMBOL(__adf_net_delete_wlanunit);

/*
* Extract a unit number from an interface name.  If no unit
* number is specified then -1 is returned for the unit number.
* Return 0 on success or an error code.
*/
int
__adf_net_ifc_name2unit(const char *name, int *unit)
{
    const char *cp;

    for (cp = name; *cp != '\0' && !('0' <= *cp && *cp <= '9'); cp++)
        ;
    if (*cp != '\0')
    {
        *unit = 0;
        for (; *cp != '\0'; cp++)
        {
            if (!('0' <= *cp && *cp <= '9'))
                return -EINVAL;
            *unit = (*unit * 10) + (*cp - '0');
        }
    }
    else
        *unit = -1;
    return 0;
}
EXPORT_SYMBOL(__adf_net_ifc_name2unit);

/**
 * @brief free the netdev index of vap to wlan unit bitmap of adf
 * @param hdl
 */
void
__adf_net_free_wlanunit(adf_net_handle_t hdl)
{
    __adf_softc_t *sc = hdl_to_softc(hdl);

    if(sc->netdev) {
        __adf_net_delete_wlanunit(sc->unit);
    }
}
EXPORT_SYMBOL(__adf_net_free_wlanunit);

#if !NO_SIMPLE_CONFIG
extern int32_t register_simple_config_callback (char *name,
                                void *callback, void *arg1, void *arg2);
extern int32_t unregister_simple_config_callback (char *name);
#endif


static struct {
    void *ctx ;
    struct net_device *dev;
    a_uint32_t push_dur ;
    adf_net_wps_cb_t cb ;
}scCB[ADF_MAX_WPS_CALLBACKS];


/** 
 * @brief WPS PBC callback registered with kernel.
 * 
 * @param cpl
 * @param dev_id
 * @param regs
 * @param push_time
 * 
 * @return 
 */
static irqreturn_t
__sccfg_cb(int cpl, void *dev_id, struct pt_regs *regs, void *push_time)
{
    adf_wps_cb_args_t cbargs ;
    a_uint32_t i;
    
    for(i = 0 ; i < ADF_MAX_WPS_CALLBACKS ; i++)
    {
            if( scCB[i].dev == (struct net_device *)dev_id )
            {
                cbargs.push_dur = *(a_uint32_t *)push_time ;
                //printk("%s: Calling PBC callback for %s\n",__FUNCTION__,scCB[i].dev->name);
                scCB[i].cb(scCB[i].ctx, &cbargs) ;
            }
    }
 
    return IRQ_HANDLED;
}


/** 
 * @brief Register WPS PBC callback
 * 
 * @param ctx
 * @param cb
 * 
 * @return 
 */
a_status_t
__adf_net_register_wpscb(adf_net_handle_t radiodev, void *ctx, adf_net_wps_cb_t cb)
{
    a_uint32_t ret = 0;
    a_status_t status ;
    struct net_device *netdev = NULL;
    a_uint32_t i;

    /* Save context and callback pointer */
    for(i = 0 ; i < ADF_MAX_WPS_CALLBACKS ; i++)
    {
        if(!scCB[i].ctx)
            break;
    }

    if(i == ADF_MAX_WPS_CALLBACKS)
        return A_STATUS_FAILED;

    netdev = hdl_to_netdev(radiodev);
   
#if !NO_SIMPLE_CONFIG
    ret = register_simple_config_callback (netdev->name, 
            (void *)__sccfg_cb , (void *)netdev, (void *)&scCB[i].push_dur);
#endif

    if(ret == 0)
    {
        scCB[i].ctx = ctx ;
        scCB[i].cb = cb ;
        scCB[i].dev = netdev ;
        status = A_STATUS_OK ;
        printk("%s: Registerd wps callback for %s \n",__FUNCTION__,netdev->name);
    }
    else
    {
        status = A_STATUS_FAILED ;
        printk("%s: Failed to register wps callback for %s \n",__FUNCTION__,netdev->name);
    }

    return status ;
}
EXPORT_SYMBOL(__adf_net_register_wpscb);


/** 
 * @brief 
 * 
 * @param ctx
 * @param cb
 * 
 * @return 
 */
a_status_t
__adf_net_unregister_wpscb(adf_net_handle_t radiodev)
{
    a_uint32_t ret = 0;
    a_status_t status = A_STATUS_FAILED ;
    struct net_device *netdev = NULL;
    a_uint32_t i;

    netdev = hdl_to_netdev(radiodev);

    for(i = 0 ; i < ADF_MAX_WPS_CALLBACKS ; i++)
    {
        if(scCB[i].dev == netdev)
        {
#if !NO_SIMPLE_CONFIG
            ret = unregister_simple_config_callback(netdev->name);
#endif
            scCB[i].ctx = NULL ;
            scCB[i].cb = 0 ;
            scCB[i].dev = NULL ;
            scCB[i].push_dur = 0 ;
            printk("%s: Deregister wps callback for %s \n",__FUNCTION__,netdev->name);
            if(ret == 0)
                status = A_STATUS_OK ;
            
            break;
        }
    }

    return status ;
}
EXPORT_SYMBOL(__adf_net_unregister_wpscb);

void 
__adf_net_poll_schedule(adf_net_handle_t hdl)
{
    return;
}
EXPORT_SYMBOL(__adf_net_poll_schedule);

a_status_t 
__adf_net_poll_schedule_cpu(adf_net_handle_t hdl, uint32_t cpu_msk, void *arg)
{
    return A_STATUS_OK;
}
EXPORT_SYMBOL(__adf_net_poll_schedule_cpu);

a_status_t
__adf_net_register_drv(adf_drv_info_t *drv)
{
    return A_STATUS_OK;
}
EXPORT_SYMBOL(__adf_net_register_drv);

void
__adf_net_unregister_drv(a_uint8_t *drv_name)
{
    return;
}
EXPORT_SYMBOL(__adf_net_unregister_drv);

MODULE_LICENSE("Proprietary");
