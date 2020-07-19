/*
 * Copyright (c) 2012 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#ifndef _WDI_EVENT_H_
#define _WDI_EVENT_H_

#include "ol_txrx_types.h"
#include "wdi_event_api.h"
#include "athdefs.h"
#include "qdf_mem.h"   /* qdf_mem_malloc,free */

#if WDI_EVENT_ENABLE

void wdi_event_handler(enum WDI_EVENT event, struct ol_txrx_pdev_t *txrx_pdev,
                        void *data, u_int16_t peer_id,
                        enum htt_rx_status status);
A_STATUS wdi_event_attach(struct ol_txrx_pdev_t *txrx_pdev);
A_STATUS wdi_event_detach(struct ol_txrx_pdev_t *txrx_pdev);

#else

#define wdi_event_handler(event, pdev, data, peer_id, status)
#define wdi_event_attach(txrx_pdev) A_OK
#define wdi_event_detach(txrx_pdev) A_OK

#endif /* WDI_EVENT_ENABLE */

#endif /* _WDI_EVENT_H_ */
