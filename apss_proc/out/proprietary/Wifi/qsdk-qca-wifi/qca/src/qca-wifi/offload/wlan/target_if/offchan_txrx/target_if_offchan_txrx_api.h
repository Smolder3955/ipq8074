/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc..
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 */

#ifndef _TARGET_IF_OFFCHAN_TXRX_API_H_
#define _TARGET_IF_OFFCHAN_TXRX_API_H_

/**
 * target_if_offchan_txrx_ops_register - Registration function to regsiter
 *                                offchan txrx module target_if ops.
 * @tx_ops: pointer to lmac if txops
 * Returns: None
 */
void target_if_offchan_txrx_ops_register(struct wlan_lmac_if_tx_ops *tx_ops);

#endif
