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

/**
 * @ingroup adf_os_public
 * @file adf_os_netlink.h
 * This file abstracts netlink related functionality.
 */
#ifndef _ADF_CMN_OS_NETLINK_H
#define _ADF_CMN_OS_NETLINK_H


#include <adf_os_types.h>
#include "adf_net_types.h"
#include <adf_nbuf.h>
#include <adf_net_offload_cfg.h>
#include <adf_net_sw.h>
#include <adf_net_pvt.h>
#include <adf_os_netlink_pvt.h>


typedef __adf_netlink_cb_t adf_netlink_cb_t;

typedef __adf_netlink_addr_t adf_netlink_addr_t;

/** 
 * @brief               Create netlink communication interface
 * 
 * @param[in] input     The callback function when netlink message is arrived
 * @param[in] ctx       The user supplied context for input function
 * @param[in] unit      A unique number which specifies 
 *                      the OS netlink pipe for unicast
 * @param[in] groups    A unique number which specifies 
 *                      the OS netlink pipe for multicast
 * 
 * @return              opaque netlink handle
 */
static adf_os_inline adf_netlink_handle_t 
adf_netlink_create(adf_netlink_cb_t input, 
                   void *ctx, 
                   a_uint32_t unit,
                   a_uint32_t groups)
{
    return __adf_netlink_create(input, ctx, unit, groups);
}

/**
 * @brief               Destroy netlink communication interface
 *
 * @param[in] nlhdl     opaque netlink handle
 */
static adf_os_inline void 
adf_netlink_delete(adf_netlink_handle_t nlhdl)
{
    __adf_netlink_delete(nlhdl);
}

/**
 * @brief           Alloc network buffer which is used for netlink communication
 *
 * @param[in] len   the size of the user data which the network buffer contains
 *
 * @return          the network buffer handle
 */
static adf_os_inline adf_nbuf_t
adf_netlink_alloc(a_uint32_t len)
{
    return __adf_netlink_alloc(len);
}

/**
 * @brief               Send netlink message to a specified address on which 
 *                      the listener is binding
 *
 * @param[in] nlhdl     the netlink opaque handle
 * @param[in] netbuf    the network buffer in which the user data is stored
 * @param[in] addr      the address of the listener
 *
 * @return              A_STATUS_OK if operation succeeds, A_STATUS_FAILED if not
 */
static adf_os_inline a_status_t 
adf_netlink_unicast(adf_netlink_handle_t nlhdl, 
                    adf_nbuf_t netbuf, 
                    adf_netlink_addr_t addr)
{
    return __adf_netlink_unicast(nlhdl, netbuf, addr);
}

/**
 * @brief               Send netlink message to a specified address on which 
 *                      a group of listeners are binding
 *
 * @param[in] nlhdl     the netlink opaque handle
 * @param[in] netbuf    the network buffer in which the user data is stored
 * @param[in] addr      the address of the group of listeners
 *
 * @return              A_STATUS_OK if operation succeeds, A_STATUS_FAILED if not
 */
static adf_os_inline a_status_t
adf_netlink_broadcast(adf_netlink_handle_t nlhdl, 
                      adf_nbuf_t netbuf, 
                      adf_netlink_addr_t groups)
{
    return __adf_netlink_broadcast(nlhdl, netbuf, groups);
}


#endif
