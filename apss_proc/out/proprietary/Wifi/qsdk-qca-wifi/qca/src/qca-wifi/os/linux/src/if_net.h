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
 * Copyright (c) 2013,2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#ifndef __QDF_HST_NET_PVT_H
#define __QDF_HST_NET_PVT_H

#include <net/sch_generic.h>
#include <net/inet_ecn.h>
#include <linux/netdevice.h>
#include <linux/pci.h>
#include <linux/stddef.h>
#include <linux/err.h>
#include <asm/checksum.h>
#include <net/ip6_checksum.h>
#include <linux/crc32.h>


#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/in.h>
#define QDF_NET_MAX_NAME            64
#define QDF_DEF_TX_TIMEOUT          5 /*Seconds I suppose*/
#define __QDF_NET_NULL              NULL

#define QDF_NET_ETHERNET_FCS_SIZE   4
#define QDF_MAX_WPS_CALLBACKS       10


/**
 *  *  * @brief Generic status to be used by qdf_drv.
 *   *   */
/**
 *  *  * @brief An ecore needs to provide a table of all pci device/vendor id's it
 *   *   * supports
 *    *    *
 *     *     * This table should be terminated by a NULL entry , i.e. {0}
 *      *      */
typedef struct {
    uint32_t vendor;
    uint32_t device;
    uint32_t subvendor;
    uint32_t subdevice;
}qdf_pci_dev_id_t;


/**
 *  *  * @brief Types of buses.
 *   *   */
typedef enum {
    QDF_BUSES_TYPE_PCI = 1,
    QDF_BUS_TYPE_GENERIC,
}qdf_bus_type_t;


/**
 *  *  * @brief Representation of bus registration data.
 *   *   */
typedef union {
    qdf_pci_dev_id_t  *pci;
    void              *raw;
}qdf_bus_reg_data_t;

/**
 *  *  * @brief Representation of data required for attach.
 *   *   */
typedef union {
    qdf_pci_dev_id_t pci;
    void *raw;
}qdf_attach_data_t;

/**
 *  *  * @brief driver info structure needed while we do the register
 *   *   *        for the driver to the shim.
 *    *    */
typedef struct _qdf_drv_info{
    /**
     *          *      * @brief driver specific functions
     *                   *           */
    qdf_drv_handle_t (*drv_attach)  (qdf_resource_t *res, int count,
            qdf_attach_data_t *data, qdf_device_t osdev);
    void       (*drv_detach)  (qdf_drv_handle_t hdl);
    void       (*drv_suspend) (qdf_drv_handle_t hdl, qdf_pm_t pm);
    void       (*drv_resume)  (qdf_drv_handle_t hdl);
    /**
     *          *      * @brief driver specific data
     *                   *           */
    qdf_bus_type_t          bus_type;
    qdf_bus_reg_data_t      bus_data;
    unsigned char           *mod_name;
    unsigned char           *ifname;
}qdf_drv_info_t;

/**
 * This is a per driver structure, that holds info about an qdf_drv that has
 * registered with anet. This will be used to lookup the details when a probe
 * is called.
 */
typedef struct __qdf_softc {
    qdf_drv_handle_t            drv_hdl;
    struct net_device          *netdev;
    struct vlan_group          *vlgrp;  /**< Vlan group*/
    uint16_t		            vid;    /**< vlan id */
    void                       *poll_arg;   /**< for poll_schedule_cpu */
    qdf_dev_sw_t                sw; /**< cp of the switch*/
    struct net_device_stats     pkt_stats;
    uint8_t                     event_to_acfg;  /**< Send events to ACFG */
    void                       *cfg_api;
    uint32_t                 unit;
}__qdf_softc_t;



/**
 * @brief WPS PBC callback function arguments
 */
typedef struct __qdf_wps_cb_args {
    uint32_t push_dur ;
} qdf_wps_cb_args_t ;


/**
 * @brief WPS PBC callback type
 *
 * @param
 * @param cbargs
 *
 * @return
 */
typedef uint32_t
(*qdf_net_wps_cb_t)(void *ctx, qdf_wps_cb_args_t *cbargs) ;


/**
 * @brief Register WPS PBC callback.
 *
 * @param ctx
 * @param cb
 *
 * @return
 */
uint32_t
__qdf_net_register_wpscb(qdf_net_handle_t radiodev, void *ctx, qdf_net_wps_cb_t cb);


/**
 * @brief Unregister WPS PBC callback
 *
 * @param ctx
 * @param cb
 *
 * @return
 */
uint32_t
__qdf_net_unregister_wpscb(qdf_net_handle_t radiodev);

uint32_t         __qdf_net_get_vlantag(struct sk_buff *skb );
uint32_t       __qdf_net_indicate_packet(qdf_net_handle_t hdl,
                                           struct sk_buff *pkt, uint32_t len);
uint32_t       __qdf_net_indicate_vlanpkt(qdf_net_handle_t hdl,
                                            struct sk_buff *pkt, uint32_t len,
                                            qdf_net_vid_t *vid);
void             __qdf_net_poll_schedule(qdf_net_handle_t hdl);
uint32_t             __qdf_net_poll_schedule_cpu(qdf_net_handle_t hdl,
                                             uint32_t cpu_msk, void *arg);
uint32_t       __qdf_net_dev_tx(qdf_net_handle_t hdl, struct sk_buff *pkt);

qdf_net_handle_t __qdf_net_create_wifidev(qdf_drv_handle_t hdl,
                                          qdf_dev_sw_t *op,
                                          qdf_net_dev_info_t *info,
                                          void  *wifi_cfg);
qdf_net_handle_t __qdf_net_create_vapdev(qdf_drv_handle_t hdl,
                                         qdf_dev_sw_t *op,
                                         qdf_net_dev_info_t *info,
                                         void  *vap_cfg);
qdf_net_handle_t __qdf_net_create_ethdev(qdf_drv_handle_t hdl,
                                         qdf_dev_sw_t *op,
                                         qdf_net_dev_info_t *info);
uint32_t      __qdf_net_delete_dev(qdf_net_handle_t hdl);


uint32_t      __qdf_net_dev_open(qdf_net_handle_t hdl);
uint32_t      __qdf_net_dev_close(qdf_net_handle_t hdl);
const uint8_t * __qdf_net_ifname(qdf_net_handle_t  hdl);
uint32_t      __qdf_net_fw_mgmt_to_app(qdf_net_handle_t hdl,
                                          struct sk_buff *pkt, uint32_t len);
uint32_t      __qdf_net_send_wireless_event(qdf_net_handle_t hdl,
                                    qdf_net_wireless_event_t what, void *data,
                                    qdf_size_t data_len);


uint32_t      __qdf_net_indicate_event(qdf_net_handle_t   hdl, void    *event);
uint32_t      __qdf_send_custom_wireless_event(qdf_net_handle_t  hdl,
												 void  *event);
uint32_t      __qdf_net_get_vlanvid(uint32_t tag ) ;

uint32_t      __qdf_net_register_drv(qdf_drv_info_t *drv);
void            __qdf_net_unregister_drv(uint8_t *drv_name);

/**
 * @brief
 *
 * @param hdl
 *
 * @return
 */
int
__qdf_net_new_wlanunit(void);

/**
 * @brief
 *
 * @param hdl
 *
 * @return
 */
int
__qdf_net_alloc_wlanunit(u_int unit);


/**
 * @brief
 *
 * @param hdl
 *
 * @return
 */
void
__qdf_net_delete_wlanunit(u_int unit);

/**
 * @brief
 *
 * @param hdl
 *
 * @return
 */
int
__qdf_net_ifc_name2unit(const char *name, int *unit);


void
__qdf_net_free_wlanunit(qdf_net_handle_t hdl);


int
__qdf_net_dev_exist_by_name(const char *name);


#define hdl_to_softc(_hdl)      ((__qdf_softc_t *)(_hdl))
#define hdl_to_netdev(_hdl)     ((hdl_to_softc(_hdl))->netdev)
#define hdl_to_virt(_hdl)       (hdl_to_softc(_hdl)->virt)
#define hdl_to_sw(_hdl)         (hdl_to_softc(_hdl)->sw)
#define hdl_to_drv(_hdl)        (hdl_to_softc(_hdl)->drv_hdl)
#define hdl_to_stats(_hdl)      (hdl_to_softc(_hdl)->pkt_stats)
/**
 * XXX:netdev->flags are in the the section which are not
 * exposed to the outside world, hence subjected to change.
 * Thats what comment says in netdev
 */
#define hdl_to_ifflags(_hdl)    (hdl_to_netdev(_hdl)->flags)

#define netdev_to_softc(_dev)   ((__qdf_softc_t *)netdev_priv(_dev))
#define netdev_to_sw(_dev)      ((netdev_to_softc(_dev)->sw))
#define netdev_to_drv(_dev)     (netdev_to_softc(_dev)->drv_hdl)
#define netdev_to_stats(_dev)   (netdev_to_softc(_dev)->pkt_stats)
#define netdev_to_cfg(_dev)     (netdev_to_softc(_dev)->cfg_api)

#define qdf_drv_open(_dev)  \
    netdev_to_sw(_dev).drv_open(netdev_to_drv(_dev))

#define qdf_drv_close(_dev) \
    netdev_to_sw(_dev).drv_close(netdev_to_drv(_dev))

#define qdf_drv_tx(_dev, _pkt)  \
    netdev_to_sw(_dev).drv_tx(netdev_to_drv(_dev), _pkt)

#define qdf_drv_cmd(_dev, _cmd, _data)  \
    netdev_to_sw(_dev).drv_cmd(netdev_to_drv(_dev), _cmd, _data)

#define qdf_drv_ioctl(_dev, _num, _data)    \
    netdev_to_sw(_dev).drv_ioctl(netdev_to_drv(_dev), _num, _data)

#define qdf_drv_poll(_dev, _qouta, _work_done)  \
    netdev_to_sw(_dev).drv_poll(netdev_to_drv(_dev), _quota, _work_done)

#define qdf_drv_poll_cpu(_dev, _quota, _work_done, _arg)    \
    netdev_to_sw(_dev).drv_poll_cpu(netdev_to_drv(_dev), _quota, _work_done, _arg)

#define qdf_drv_poll_int_disable(_dev)  \
    netdev_to_sw(_dev).drv_open(netdev_to_drv(_dev))

#define qdf_drv_tx_timeout(_dev)    \
    netdev_to_sw(_dev).drv_tx_timeout(netdev_to_drv(_dev))

#define qdf_drv_wcmd(_dev, _wcmd, _data) \
    netdev_to_sw(_dev).drv_wcmd(netdev_to_drv(_dev), _wcmd, _data)

#define __QDF_STATUS_ERRNO(x, ret) \
    case QDF_STATUS_##x:  return -ret

static inline int
__qdf_status_to_os(uint32_t _status)
{
    switch(_status) {
        __QDF_STATUS_ERRNO(SUCCESS, 0);
        __QDF_STATUS_ERRNO(E_FAILURE, EINVAL);
        __QDF_STATUS_ERRNO(E_NOENT, ENOENT);
        __QDF_STATUS_ERRNO(E_NOMEM, ENOMEM);
        __QDF_STATUS_ERRNO(E_INVAL, EINVAL);
        __QDF_STATUS_ERRNO(E_ALREADY, EINPROGRESS);
        __QDF_STATUS_ERRNO(E_NOSUPPORT, ENOTSUPP);
        __QDF_STATUS_ERRNO(E_BUSY, EBUSY);
        __QDF_STATUS_ERRNO(E_E2BIG, E2BIG);
        __QDF_STATUS_ERRNO(E_AGAIN, EAGAIN);
        __QDF_STATUS_ERRNO(E_NOSPC, ENOSPC);
        __QDF_STATUS_ERRNO(E_ADDRNOTAVAIL, EADDRNOTAVAIL);
        __QDF_STATUS_ERRNO(E_ENXIO, ENXIO);
        __QDF_STATUS_ERRNO(E_NETDOWN, ENETDOWN);
        __QDF_STATUS_ERRNO(E_FAULT, EFAULT);
        __QDF_STATUS_ERRNO(E_IO, EIO);
        __QDF_STATUS_ERRNO(E_NETRESET, ENETRESET);
        __QDF_STATUS_ERRNO(E_EXISTS, EEXIST);
        __QDF_STATUS_ERRNO(E_SIG, EINTR);

        default:
            printk("qdf_net:error:unkown error status: 0x%x\n", _status);
            dump_stack();
            return -ENOTSUPP;
    }
}

static inline void
__qdf_net_carrier_on(qdf_net_handle_t hdl)
{
    return(netif_carrier_on(hdl_to_netdev(hdl)));
}

static inline void
__qdf_net_carrier_off(qdf_net_handle_t hdl)
{
    return(netif_carrier_off(hdl_to_netdev(hdl)));
}

static inline int
__qdf_net_carrier_ok(qdf_net_handle_t hdl)
{
    return (netif_carrier_ok(hdl_to_netdev(hdl)));
}

static inline void
__qdf_net_start_queue(qdf_net_handle_t hdl)
{
    return(netif_start_queue(hdl_to_netdev(hdl)));
}

static inline void
__qdf_net_wake_queue(qdf_net_handle_t hdl)
{
    return(netif_start_queue(hdl_to_netdev(hdl)));
}

static inline void
__qdf_net_stop_queue(qdf_net_handle_t hdl)
{
    return(netif_stop_queue(hdl_to_netdev(hdl)));
}

static inline int
__qdf_net_queue_stopped(qdf_net_handle_t hdl)
{
    return(netif_queue_stopped(hdl_to_netdev(hdl)));
}
/**
 * @brief check if the interface is running
 * @param hdl
 *
 * @return uint8_t
 */
static inline uint8_t
__qdf_net_is_running(qdf_net_handle_t hdl)
{

    return (hdl_to_ifflags(hdl) & IFF_RUNNING);
}
/**
 * @brief check if the interface is UP
 * @param hdl
 *
 * @return uint8_t
 */
static inline uint8_t
__qdf_net_is_up(qdf_net_handle_t hdl)
{
    return (hdl_to_ifflags(hdl) & IFF_UP);
}
/**
 * @brief check ALLMULTI flag
 * @param hdl
 *
 * @return uint8_t
 */
static inline uint8_t
__qdf_net_is_allmulti(qdf_net_handle_t   hdl)
{
    return (uint8_t)(hdl_to_ifflags(hdl) & IFF_ALLMULTI);
}
/**
 * @brief check MULTICAST flag
 * @param hdl
 *
 * @return uint8_t
 */
static inline uint8_t
__qdf_net_is_multi(qdf_net_handle_t   hdl)
{
    return (uint8_t)(hdl_to_ifflags(hdl) & IFF_MULTICAST);
}
/**
 * @brief check the promisc flag
 * @param hdl
 *
 * @return uint8_t
 */
static inline uint8_t
__qdf_net_is_promisc(qdf_net_handle_t   hdl)
{
    return (uint8_t)(hdl_to_ifflags(hdl) & IFF_PROMISC);
}

static inline qdf_handle_t
__qdf_net_hdl_to_os(qdf_net_handle_t hdl)
{
    return hdl;
}
static inline qdf_handle_t
__qdf_net_dev_to_os(__qdf_device_t osdev)
{
        return ((qdf_handle_t)osdev);
}

static inline void
__qdf_net_get_mclist(qdf_net_handle_t  hdl, qdf_net_devaddr_t  *mclist)
{
    struct net_device    *netdev = hdl_to_netdev(hdl);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 34)
    struct netdev_hw_addr *ha;
#else
    struct dev_mc_list   *mc = netdev->mc_list;
#endif
    int cnt = 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 34)
    netdev_for_each_mc_addr(ha, netdev) {
        mclist->da_addr[cnt] = ha->addr;
    }
#else
    for(; mc; mc = mc->next, cnt++)
        mclist->da_addr[cnt] = mc->dmi_addr;
#endif

    mclist->num = cnt;
}


/*
 * Convert MHz frequency to IEEE channel number.
*/
extern uint32_t
__qdf_mhz2ieee(uint32_t freq) ;


/*
 * P2P big param flags
 */
#define IEEE80211_IOC_P2P_GO_NOA            623
#define IEEE80211_IOC_SCAN_REQ              624
#define IEEE80211_IOC_P2P_SET_CHANNEL       641
#define IEEE80211_IOC_P2P_SEND_ACTION       643
#define IEEE80211_IOC_P2P_FETCH_FRAME       645
#define IEEE80211_IOC_P2P_NOA_INFO          648
#define IEEE80211_IOC_P2P_FIND_BEST_CHANNEL 649
#define IEEE80211_IOC_P2P_FRAME_LIST_EMPTY  652

#endif /*__qdf_net_PVT_H*/
