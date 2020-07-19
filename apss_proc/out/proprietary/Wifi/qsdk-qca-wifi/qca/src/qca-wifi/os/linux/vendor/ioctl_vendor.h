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

#ifndef _IOCTL_VENDOR_H_
#define _IOCTL_VENDOR_H_
/* 
 * Vendor Specific IOCTL API
 */
#ifndef _NET_IF_H
#include <linux/if.h>
#endif

typedef void * ioctl_ifreq_req_t;

typedef struct _ioctl_ifreq {
    char                    ii_name[IFNAMSIZ];
    ioctl_ifreq_req_t       *ii_req;
} ioctl_ifreq_t;

int osif_ioctl_vendor(struct net_device *dev, struct ifreq *ifr, int fromVirtual);
#endif /* _IOCTL_VENDOR_H_ */
