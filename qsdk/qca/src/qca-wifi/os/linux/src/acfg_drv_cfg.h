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

#ifndef __ACFG_CFG_H__
#define __ACFG_CFG_H__

#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/rtnetlink.h>
#include <linux/wireless.h>
#include <linux/etherdevice.h>
#include <net/iw_handler.h>


#include <asm/io.h>
#include <asm/byteorder.h>
/*
** Need to define byte order based on the CPU configuration.
*/
#define _LITTLE_ENDIAN  1234
#define _BIG_ENDIAN 4321
#ifdef CONFIG_CPU_BIG_ENDIAN
    #define _BYTE_ORDER    _BIG_ENDIAN
#else
    #define _BYTE_ORDER    _LITTLE_ENDIAN
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,18)
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 33)
#include <linux/autoconf.h>
#else
#include <generated/autoconf.h>
#endif
#else
#include <linux/config.h>
#endif
#include <ieee80211_external.h>


/*
  Some of the macro hardcoded for now for basic bringup 
  to be removed and the macro refernce to be taken from newma header file 
  by including them in acfg. Or else the required 
  values has to be passed from acfg/host.
  */
#define SIOC80211IFCREATE       (SIOCDEVPRIVATE+7)
#define SIOC80211IFDESTROY      (SIOCDEVPRIVATE+8)
#define IEEE80211_IOCTL_SETMODE     (SIOCIWFIRSTPRIV+18)
#define IEEE80211_IOCTL_SETPARAM    (SIOCIWFIRSTPRIV+0)
#define IEEE80211_IOCTL_GETPARAM    (SIOCIWFIRSTPRIV+1)
#define IEEE80211_IOCTL_GETWMMPARAMS    (SIOCIWFIRSTPRIV+5)

#define SIOCIOCTLTX99               (SIOCDEVPRIVATE+13)
#define SIOCIOCTLNAWDS               (SIOCDEVPRIVATE+12)

#define ATH_HAL_IOCTL_SETPARAM       (SIOCIWFIRSTPRIV+0)
#define ATH_HAL_IOCTL_GETPARAM       (SIOCIWFIRSTPRIV+1)


#define SIOCDEVVENDOR                   (SIOCDEVPRIVATE+15)


typedef int (* acfg_cmd_handler_t)(void *ctx, uint16_t cmdid,
                                  uint8_t *buffer, int32_t Length);

typedef struct _acfg_dispatch_entry {
    acfg_cmd_handler_t      cmd_handler;    /* dispatch function */
    uint32_t               cmdid;          /* WMI command to dispatch from */
    uint16_t             flags;
} acfg_dispatch_entry_t;

int
__acfg_ioctl(struct net_device *netdev, void *data);

int32_t
netdev_ioctl(struct net_device *dev, struct ifreq   *ifr, int cmd);

#endif
