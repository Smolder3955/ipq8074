/*
 * Copyright (c) 2019 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 */

/*****************************************************************************/
/* \file qdf_net_vlan.c
** \brief Provides VLAN functionality
**
**  This is a temporary shim file used to provide a bridge to the qdf_net_
**  functions that will eventually replace this.  Used to contain the VLAN
**  support functions.
**
** Copyright (c) 2010, Atheros Communications Inc.
**
** Permission to use, copy, modify, and/or distribute this software for any
** purpose with or without fee is hereby granted, provided that the above
** copyright notice and this permission notice appear in all copies.
**
** THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
** WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
** MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
** ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
** WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
** ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
** OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
**/

/*
 * Copyright (c) 2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#if ATH_SUPPORT_VLAN

#include <osdep.h>
#include <wbuf.h>

#include "osif_private.h"
#include "ath_internal.h"
#include "if_athvar.h"
#include <linux/proc_fs.h>
#include <linux/if_vlan.h>

#include <_ieee80211.h>
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
#include <osif_nss_wifiol_vdev_if.h>
#endif

/*
** Module Level Definitions
*/


/*
** Internal Prototypes
*/

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0)
static void	ospriv_vlan_add_vid(struct net_device *dev, unsigned short vid);
static void	ospriv_vlan_kill_vid(struct net_device *dev, unsigned short vid);
#else
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
static int     ospriv_vlan_add_vid(struct net_device *dev, unsigned short vid);
static int     ospriv_vlan_kill_vid(struct net_device *dev, unsigned short vid);
#else
static int     ospriv_vlan_add_vid(struct net_device *dev, __be16 port, unsigned short vid);
static int     ospriv_vlan_kill_vid(struct net_device *dev, __be16 port, unsigned short vid);
#endif
#endif

#ifdef QCA_PARTNER_PLATFORM
extern 	void osif_pltfrm_vlan_feature_set(struct net_device *dev);
extern  int ospriv_pltfrm_vlan_skip(void);
#endif


/****************************************************************
** Public Interface
****************************************************************/

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,30)
void
qdf_net_vlan_attach(struct net_device *dev,struct net_device_ops *osif_dev_ops)
#else
void
qdf_net_vlan_attach(struct net_device *dev)
#endif
{
#ifdef QCA_PARTNER_PLATFORM
    osif_pltfrm_vlan_feature_set(dev);
#else
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
	dev->features |= NETIF_F_HW_VLAN_TX | NETIF_F_HW_VLAN_RX |
		NETIF_F_HW_VLAN_FILTER;
#else
	dev->features |= NETIF_F_HW_VLAN_CTAG_TX | NETIF_F_HW_VLAN_CTAG_RX |
			 NETIF_F_HW_VLAN_CTAG_FILTER |
			 NETIF_F_HW_VLAN_STAG_TX | NETIF_F_HW_VLAN_STAG_RX |
			 NETIF_F_HW_VLAN_STAG_FILTER;
#endif
#endif

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,30)
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,1,0)
    osif_dev_ops->ndo_vlan_rx_register = ospriv_vlan_register;
#endif
    osif_dev_ops->ndo_vlan_rx_add_vid = ospriv_vlan_add_vid;
    osif_dev_ops->ndo_vlan_rx_kill_vid = ospriv_vlan_kill_vid;
#else
    dev->vlan_rx_register = ospriv_vlan_register;
    dev->vlan_rx_add_vid = ospriv_vlan_add_vid;
    dev->vlan_rx_kill_vid = ospriv_vlan_kill_vid;
#endif
}

void
qdf_net_vlan_detach(struct net_device *dev)
{
}

unsigned short
qdf_net_get_vlan(osdev_t osif)
{
    osif_dev  *osifp = (osif_dev *) osif;
	return (osifp->vlanID);
}

int
qdf_net_is_vlan_defined(osdev_t osif)
{
    osif_dev  *osifp = (osif_dev *) osif;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,1,0)
	if( osifp->vlanID > 0)
#else
	if( osifp->vlgrp != NULL)
#endif
		return TRUE;
	else
		return FALSE;
}


/****************************************************************************
** Private Functions
****************************************************************************/

/******************************************************************************/

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0)
static void
#else
static int
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
ospriv_vlan_add_vid(struct net_device *dev, unsigned short vid)
#else
ospriv_vlan_add_vid(struct net_device *dev, __be16 proto, unsigned short vid)
#endif
{
    osif_dev  *osifp = ath_netdev_priv(dev);
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    struct ieee80211vap *vap = NULL;
    struct ieee80211com *ic = NULL;
#endif

#ifdef QCA_PARTNER_PLATFORM
    if( ospriv_pltfrm_vlan_skip() )
        return 0;
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,1,0)
    if (osifp->vlgrp != NULL)
        osifp->vlanID = vid;
    else
        osifp->vlanID = 0;
#else
    osifp->vlanID = vid;
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    vap = osifp->os_if;
    ic = vap->iv_ic;
    if (!ic->nss_vops || !ic->nss_vops->ic_osif_nss_vap_update_vlan)
        return 0;
    ic->nss_vops->ic_osif_nss_vap_update_vlan(vap, osifp);
#endif

#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0)
    return 0;
#endif
}

/******************************************************************************/
/*!
**  \brief Kill a VLAN group
**
**  Kills a defined VLAN group by setting the "magic" number to NULL
**
**  \param dev	pointer to device structure
**	\param vid	VLAN id of the group to kill
**  \return N/A
*/

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0)
static void
#else
static int
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
ospriv_vlan_kill_vid(struct net_device *dev, unsigned short vid)
#else
ospriv_vlan_kill_vid(struct net_device *dev, __be16 proto, unsigned short vid)
#endif
{
    osif_dev  *osifp = ath_netdev_priv(dev);

#ifdef QCA_PARTNER_PLATFORM
    if( ospriv_pltfrm_vlan_skip() )
        return 0;
#endif

	if (osifp->vlgrp != NULL)
	{
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,20)
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0)
	vlan_group_set_device(osifp->vlgrp, vid, NULL);
#else
	osifp->vlgrp = NULL;
	osifp->vlanID = 0;
#endif
#else
        osifp->vlgrp->vlan_devices[vid] = NULL;
#endif
	}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,1,0)
	osifp->vlanID = 0;
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0)
	return 0;
#endif
}

#endif /* ATH_SUPPORT_VLAN */


