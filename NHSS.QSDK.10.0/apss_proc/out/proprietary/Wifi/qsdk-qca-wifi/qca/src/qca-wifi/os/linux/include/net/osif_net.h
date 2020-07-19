/*
 * Copyright (c) 2010, Atheros Communications Inc.
 * All Rights Reserved.
 *
 * Copyright (c) 2011,2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 */

/**
 * @defgroup qdf_net_public network abstraction API
 */

/**
 * @ingroup qdf_net_public
 * @file osif_net.h
 * These APIs abstract the OS networking stack from a driver.
 */

/**
 * @mainpage
 * @section Introduction
 * The Atheros Driver Framework provides a mechanism to run the Atheros
 * WLAN driver on a variety of Operating Systems and Platforms. It achieves
 * this by abstracting all OS-specific and platform-specific functionality
 * the driver requires. This ensures the core logic in the driver is OS-
 * and platform-independent.
 * @section Modules
 * The driver framework consists of three main components:
 * @subsection sec1 Network Stack
 * This component abstracts the OS network stack.
 * See @ref qdf_net_public for details.
 * @subsection sec2 Network Buffer
 * This component abstracts the OS network buffer.
 * See @ref qdf_nbuf_public for details.
 * @subsection sec3 OS services
 * This component abstracts any OS services. See @ref qdf_public for details.
 */

#ifndef _QDF_NET_H
#define _QDF_NET_H

#include <qdf_types.h>
//#include <cdf_nbuf.h>
#include <qdf_net_types.h>
#include "osif_net_wcmd.h"
#include <osif_net_sw.h>
#include <if_net.h>

#define __qdf_inline  inline
#define qdf_inline _qdf_inline
/*
 * check for a NULL handle
 * */
#define QDF_NET_NULL        __QDF_NET_NULL


#define qdf_net_indicate_app qdf_net_fw_mgmt_to_app

/**
 * qdf_net_new_wlanunit - get new wlan unit
 * @return - free wlan unit index
 */
static inline int
qdf_net_new_wlanunit(void)
{
	return __qdf_net_new_wlanunit();
}

/**
 * qdf_net_alloc_wlanunit - alloc/mark  wlan unit passed by user into wlan bit map array
 * @unit: wlan unit index
 * @return - int success/failure
 */
static inline int
qdf_net_alloc_wlanunit(u_int unit)
{
	return __qdf_net_alloc_wlanunit(unit);
}

/**
 * qdf_net_delete_wlanunit - free passed wlan unit
 * @unit: wlan unit index
 * @return - none
 */
static inline void
qdf_net_delete_wlanunit(u_int unit)
{
	__qdf_net_delete_wlanunit(unit);
}

/**
 * qdf_net_ifc_name2unit - extract wlan unit index from the if name
 * @name: ifname
 * @unit: wlan unit index
 * @return - success/Failure
 */
static inline int
qdf_net_ifc_name2unit(const char *name, int *unit)
{
	return __qdf_net_ifc_name2unit(name, unit);
}

/**
 * qdf_net_dev_exist_by_name - check if a wlan device exists by the provided interface name
 * @name: ifname
 * @return - int success/Failure
 */
static inline int
qdf_net_dev_exist_by_name(const char *name)
{
	return __qdf_net_dev_exist_by_name(name);
}

/**
 * qdf_net_stop_queue - inform the networking stack to stop sending transmit packets.
 * Typically called if the driver runs out of resources for the device.
 * @hdl: opaque device handle
 * return - none
 */
static inline void
qdf_net_stop_queue(qdf_net_handle_t hdl)
{
	__qdf_net_stop_queue(hdl);
}
#endif
