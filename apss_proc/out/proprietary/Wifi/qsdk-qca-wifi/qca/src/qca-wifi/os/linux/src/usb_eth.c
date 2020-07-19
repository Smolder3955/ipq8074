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

#include <linux/if_arp.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/usb.h>
#include "if_athvar.h"

#include <linux/ethtool.h>
#include "version.h"
static char *version = ATH_USB_VERSION " (Atheros/multi-bss)";
static char *dev_info = "ath_usb";

int
ath_ioctl_ethtool(struct ath_softc_net80211 *scn, int cmd, void *addr)
{
    struct ethtool_drvinfo info;

    if (cmd != ETHTOOL_GDRVINFO)
        return -EOPNOTSUPP;
    memset(&info, 0, sizeof(info));
    info.cmd = cmd;
    strncpy(info.driver, dev_info, sizeof(info.driver)-1);
    strncpy(info.version, version, sizeof(info.version)-1);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,22)
    /* include the device name so later versions of kudzu DTRT */
    strncpy(info.bus_info, "usb",sizeof(info.bus_info)-1);    
#endif
    return _copy_to_user(addr, &info, sizeof(info)) ? -EFAULT : 0;
}

MODULE_AUTHOR("Atheros Communications, Inc.");
MODULE_DESCRIPTION("Atheros Device Module");
#ifdef MODULE_LICENSE
MODULE_LICENSE("Proprietary");
#endif

static int __init
init_ath_usb_eth(void)
{
    printk(KERN_INFO "%s: "
           "Copyright (c) 2001-2005 Atheros Communications, Inc, "
           "All Rights Reserved\n", dev_info);
    return 0;
}
module_init(init_ath_usb_eth);

static void __exit
exit_ath_usb_eth(void)
{
    printk(KERN_INFO "%s: driver unloaded\n", dev_info);
}
module_exit(exit_ath_usb_eth);
