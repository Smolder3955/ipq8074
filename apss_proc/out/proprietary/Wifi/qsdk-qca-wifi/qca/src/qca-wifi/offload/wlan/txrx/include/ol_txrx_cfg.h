/*
 * Copyright (c) 2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011-2015 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
/**
 * @file ol_txrx_types.h
 * @brief Define the major data types used internally by the host datapath SW.
 */
#ifndef _OL_TXRX_CFG__H_
#define _OL_TXRX_CFG__H_

struct ol_txrx_cfg_soc_ctxt {
    bool tso_enabled;
    bool lro_enabled;
    bool sg_enabled;
    bool gro_enabled;
    bool ol_tx_csum_enabled;
    bool ol_rx_csum_enabled;
    bool rawmode_enabled;
    bool peer_flow_ctrl_enabled;
};

/**
 * ol_txrx_cfg_soc_attach() - attach ol_txrx_cfg to DP soc
 * @psoc: opaque soc handle
 *
 * Return: pointer of type struct ol_txrx_cfg_soc_ctxt
 */
struct ol_txrx_cfg_soc_ctxt *ol_txrx_cfg_soc_attach(void *psoc);

/**
 * ol_txrx_cfg_soc_detach() - detach ol txrx cfg
 * @cfg: ol txrx cfg ctxt
 *
 * Return: void
 */
void ol_txrx_cfg_soc_detach(struct ol_txrx_cfg_soc_ctxt *cfg);

/**
 * ol_txrx_cfg_get_dp_caps() - get dp capabilities
 * @soc_handle: datapath soc handle
 * @dp_caps: enum for dp capabilities
 *
 * Return: bool to determine if a dp caps is enabled
 */
bool
ol_txrx_cfg_get_dp_caps(struct ol_txrx_cfg_soc_ctxt *cfg,
            enum cdp_capabilities dp_caps);
#endif
