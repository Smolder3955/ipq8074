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

/*
 * @File:
 *
 * @Abstract: acfg Data Structure for interfacing between Target and Host
 *
 */
#ifndef __ACFG_CMD_H
#define __ACFG_CMD_H

#include <acfg_api_types.h>


#define IOCTL_VIRDEV_MASK  (0x04000000)

#define IEEE80211_IOCTL_TESTMODE  (IOCTL_VIRDEV_MASK | 1)
                                             /* Test Mode for Factory */
#define IEEE80211_IOCTL_RSSI      (IOCTL_VIRDEV_MASK | 6) /* Get RSSI */
#define IEEE80211_IOCTL_CUSTDATA  (IOCTL_VIRDEV_MASK | 7) /* Get custdata */

typedef struct {
    uint32_t vendor;
    uint32_t cmd;
    uint8_t   name[16];
}acfg_vendor_t;

/* for wsupp bridge message exchange */
typedef struct acfg_wsupp_message {
    uint16_t len;
    uint8_t data[0];
} acfg_wsupp_message_t;

#endif /*  __ACFG_CMD_H */

