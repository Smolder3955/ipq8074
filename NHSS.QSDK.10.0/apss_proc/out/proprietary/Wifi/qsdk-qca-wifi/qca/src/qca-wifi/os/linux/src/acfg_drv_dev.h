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

#ifndef __ATH_DEV_H__
#define __ATH_DEV_H__

#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/rtnetlink.h>
#include <linux/wireless.h>
#include <linux/etherdevice.h>
#include <net/iw_handler.h>

#include <acfg_api_types.h>
#include <sys/queue.h>

#define LINUX_PVT_WIOCTL  (SIOCWANDEV)

typedef struct dev_ioctl_map {
    char     name[IFNAMSIZ];
    struct     net_device *dev;
    int        (*do_ioctl)(struct net_device *dev,
                       struct ifreq *ifr, int cmd);
   LIST_ENTRY(dev_ioctl_map) map_list;
} __acfg_dev_ioctl_map_t;

__acfg_dev_ioctl_map_t *
acfg_alloc_map_entry (struct net_device *dev);

__acfg_dev_ioctl_map_t *
acfg_find_map_entry_by_dev (struct net_device *dev);

__acfg_dev_ioctl_map_t *
acfg_find_map_entry_by_name (char *dev);

int
acfg_free_map_entry (__acfg_dev_ioctl_map_t *);

#endif
