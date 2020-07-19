/*
 * Copyright (c) 2012, 2017-208 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2012 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include "wdi_event.h"

#if WDI_EVENT_ENABLE

static inline wdi_event_subscribe *
wdi_event_next_sub(wdi_event_subscribe *wdi_sub)
{
    if (!wdi_sub) {
        qdf_print("Invalid subscriber in %s", __FUNCTION__);
        return NULL;
    }
    return wdi_sub->priv.next;
}

static inline void
wdi_event_del_subs(wdi_event_subscribe *wdi_sub, int event_index)
{
    /* Subscribers should take care of deletion */
}

static inline void
wdi_event_iter_sub(
    struct ol_txrx_pdev_t *pdev,
    uint32_t event_index,
    wdi_event_subscribe *wdi_sub,
    void *data,
	uint16_t peer_id,
	enum htt_rx_status status)
{
    enum WDI_EVENT event = event_index + WDI_EVENT_BASE;

    if (wdi_sub) {
        do {
            wdi_sub->callback(wdi_sub->context, event, data, peer_id, status);
        } while ((wdi_sub = wdi_event_next_sub(wdi_sub)));
    }
}

void
wdi_event_handler(
    enum WDI_EVENT event,
    struct ol_txrx_pdev_t *txrx_pdev,
    void *data,
	uint16_t peer_id,
	enum htt_rx_status status)
{
    uint32_t event_index;
    wdi_event_subscribe *wdi_sub;
    /*
     * Input validation
     */
    if (!event) {
        qdf_print("Invalid WDI event in %s", __FUNCTION__);
        return;
    }
    if (!txrx_pdev) {
        qdf_print("Invalid pdev in WDI event handler");
        return;
    }
    /*
     *  There can be NULL data, so no validation for the data
     *  Subscribers must do the sanity based on the requirements
     */
    event_index = event - WDI_EVENT_BASE;
    wdi_sub = txrx_pdev->wdi_event_list[event_index];

    /* Find the subscriber */
    wdi_event_iter_sub(txrx_pdev, event_index, wdi_sub, data, peer_id, status);
}
qdf_export_symbol(wdi_event_handler);

A_STATUS
wdi_event_sub(
    struct cdp_pdev *txrx_pdev_handle,
    void *event_cb_sub_handle,
    uint32_t event)
{
    uint32_t event_index;
    wdi_event_subscribe *wdi_sub;
    struct ol_txrx_pdev_t *txrx_pdev = (struct ol_txrx_pdev_t *)txrx_pdev_handle;
    wdi_event_subscribe *event_cb_sub =
        (wdi_event_subscribe *) event_cb_sub_handle;
    /* Input validation */
    if (!txrx_pdev) {
        qdf_print("Invalid txrx_pdev in %s", __FUNCTION__);
        return A_ERROR;
    }
    if (!event_cb_sub) {
        qdf_print("Invalid callback in %s", __FUNCTION__);
        return A_ERROR;
    }
    if ((!event) || (event >= WDI_EVENT_LAST) || (event < WDI_EVENT_BASE)) {
        qdf_print("Invalid event in %s", __FUNCTION__);
        return A_ERROR;
    } /* Input validation */

    event_index = event - WDI_EVENT_BASE;
    wdi_sub = txrx_pdev->wdi_event_list[event_index];
    /*
     *  Check if it is the first subscriber of the event
     */
    if (!wdi_sub) {
        wdi_sub = event_cb_sub;
        wdi_sub->priv.next = NULL;
        wdi_sub->priv.prev = NULL;
        txrx_pdev->wdi_event_list[event_index] = wdi_sub;
        return A_OK;
    }
    event_cb_sub->priv.next = wdi_sub;
    event_cb_sub->priv.prev = NULL;
    wdi_sub->priv.prev = event_cb_sub;
    txrx_pdev->wdi_event_list[event_index] = event_cb_sub;
    return A_OK;

}

A_STATUS
wdi_event_unsub(
    struct cdp_pdev *txrx_pdev_handle,
    void *event_cb_sub_handle,
    uint32_t event)
{
    uint32_t event_index = event - WDI_EVENT_BASE;
    struct ol_txrx_pdev_t *txrx_pdev = (struct ol_txrx_pdev_t *)txrx_pdev_handle;
    wdi_event_subscribe *event_cb_sub =
        (wdi_event_subscribe *) event_cb_sub_handle;

    /* Input validation */
    if (!event_cb_sub) {
        qdf_print("Invalid callback in %s", __FUNCTION__);
        return A_ERROR;
    }
    if (!event_cb_sub->priv.prev) {
        txrx_pdev->wdi_event_list[event_index] = event_cb_sub->priv.next;
    } else {
        event_cb_sub->priv.prev->priv.next = event_cb_sub->priv.next;
    }
    if (event_cb_sub->priv.next) {
        event_cb_sub->priv.next->priv.prev = event_cb_sub->priv.prev;
    }
    //qdf_mem_free(event_cb_sub);

    return A_OK;
}

A_STATUS
wdi_event_attach(struct ol_txrx_pdev_t *txrx_pdev)
{
    /* Input validation */
    if (!txrx_pdev) {
        qdf_print(
            "Invalid device in %s\nWDI event attach failed", __FUNCTION__);
        return A_ERROR;
    }
    /* Separate subscriber list for each event */
    txrx_pdev->wdi_event_list = (wdi_event_subscribe **)
        qdf_mem_malloc(
            sizeof(wdi_event_subscribe *) * WDI_NUM_EVENTS);
    if (!txrx_pdev->wdi_event_list) {
        qdf_print("Insufficient memory for the WDI event lists");
        return A_NO_MEMORY;
    }
    return A_OK;
}

A_STATUS
wdi_event_detach(struct ol_txrx_pdev_t *txrx_pdev)
{
    int i;
    wdi_event_subscribe *wdi_sub;
    if (!txrx_pdev) {
        qdf_print("Invalid device in %s\nWDI attach failed", __FUNCTION__);
        return A_ERROR;
    }
    if (!txrx_pdev->wdi_event_list) {
        return A_ERROR;
    }
    for (i = 0; i < WDI_NUM_EVENTS; i++) {
        wdi_sub = txrx_pdev->wdi_event_list[i];
        /* Delete all the subscribers */
        wdi_event_del_subs(wdi_sub, i);
    }
    if (txrx_pdev->wdi_event_list) {
        qdf_mem_free(txrx_pdev->wdi_event_list);
    }
    return A_OK;
}

#else
A_STATUS
wdi_event_sub(
    struct cdp_pdev *txrx_pdev_handle,
    void *event_cb_sub_handle,
    uint32_t event)
{
    return A_OK;
}

A_STATUS
wdi_event_unsub(
    struct cdp_pdev *txrx_pdev_handle,
    void *event_cb_sub_handle,
    uint32_t event)
{
    return A_OK;
}
#endif /* WDI_EVENT_ENABLE */
