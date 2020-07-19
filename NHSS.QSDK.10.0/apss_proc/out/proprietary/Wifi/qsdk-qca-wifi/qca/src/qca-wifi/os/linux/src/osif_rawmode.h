/*
* Copyright (c) 2014, Qualcomm Atheros, Inc.
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

#ifndef _OSIF_RAWMODE_H_
#define _OSIF_RAWMODE_H_

#include "osif_private.h"

#if QCA_OL_SUPPORT_RAWMODE_TXRX
/**
 * @brief Process raw frame transmission, invoking simulation if necessary
 *
 * @param dev - the Linux net_device object for this transmission
 * @param pskb - double pointer to skb
 *
 * @return - 0 on success, -1 on error, and if simulation is enabled: 1 if more
 * nbufs need to be consumed.
 */
extern int
ol_tx_ll_umac_raw_process(struct net_device *dev, struct sk_buff **pskb);

#define OL_TX_LL_UMAC_RAW_PROCESS(_osdev, _pskb) \
        ol_tx_ll_umac_raw_process((_osdev), (_pskb))
#else
#define OL_TX_LL_UMAC_RAW_PROCESS(_osdev, _pskb)
#endif /* QCA_OL_SUPPORT_RAWMODE_TXRX */

#endif /* _OSIF_RAWMODE_H_ */
