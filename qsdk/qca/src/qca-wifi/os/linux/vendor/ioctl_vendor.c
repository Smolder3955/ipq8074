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

/*
 *  Vendor Specific IOCTL
 */
#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
    #include <linux/config.h>
#endif
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/utsname.h>
#include <linux/if_arp.h>       /* XXX for ARPHRD_ETHER */
#include <net/iw_handler.h>

#include <asm/uaccess.h>

#include "if_media.h"
#include "_ieee80211.h"
#include <osif_private.h>

#ifdef ATH_DEBUG
#define IOCTL_DPRINTF(_fmt, ...) do { printk(_fmt, __VA_ARGS__); } while (0)
#else
#define IOCTL_DPRINTF(_vap, _m, _fmt, ...)
#endif /* ATH_DEBUG */

extern int __osif_ioctl_vendor(struct net_device *dev, ioctl_ifreq_req_t *iiReq, int fromVirtual);  /* implement by vendor */

int osif_ioctl_vendor(struct net_device *dev, struct ifreq *ifr, int fromVirtual)
{
    ioctl_ifreq_t     *ii = (ioctl_ifreq_t *)ifr;
    ioctl_ifreq_req_t *iiReq = ii->ii_req;
    int retv = -EOPNOTSUPP;

    retv = __osif_ioctl_vendor(dev, iiReq, fromVirtual);

    return retv;
}
