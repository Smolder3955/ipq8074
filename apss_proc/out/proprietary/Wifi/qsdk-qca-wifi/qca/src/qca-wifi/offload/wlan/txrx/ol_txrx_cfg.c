/*
 * Copyright (c) 2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011-2015 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

/*=== includes ===*/
/* header files for OS primitives */
#include <osdep.h>         /* u_int32_t, etc. */
#include <qdf_mem.h>    /* qdf_mem_malloc,free */
#include <qdf_types.h>
#include <cdp_txrx_ops.h>
#include <ol_txrx_cfg.h>
#include <cfg_dp.h>
#include <cfg_ucfg_api.h>

struct ol_txrx_cfg_soc_ctxt *ol_txrx_cfg_soc_attach(void *psoc)
{
    struct ol_txrx_cfg_soc_ctxt *ol_txrx_cfg_ctx =
        qdf_mem_malloc(sizeof(struct ol_txrx_cfg_soc_ctxt));

    if (ol_txrx_cfg_ctx == NULL)
        return NULL;

    if (psoc == NULL)
        return NULL;

    ol_txrx_cfg_ctx->tso_enabled = cfg_get(psoc, CFG_DP_TSO);
    ol_txrx_cfg_ctx->lro_enabled = cfg_get(psoc, CFG_DP_LRO);
    ol_txrx_cfg_ctx->sg_enabled = cfg_get(psoc, CFG_DP_SG);
    ol_txrx_cfg_ctx->gro_enabled = cfg_get(psoc, CFG_DP_GRO);
    ol_txrx_cfg_ctx->ol_tx_csum_enabled = cfg_get(psoc, CFG_DP_OL_TX_CSUM);
    ol_txrx_cfg_ctx->ol_rx_csum_enabled = cfg_get(psoc, CFG_DP_OL_RX_CSUM);
    ol_txrx_cfg_ctx->rawmode_enabled = cfg_get(psoc, CFG_DP_RAWMODE);
    ol_txrx_cfg_ctx->peer_flow_ctrl_enabled =
        cfg_get(psoc, CFG_DP_PEER_FLOW_CTRL);

    return ol_txrx_cfg_ctx;
}

/**
 * ol_txrx_cfg_soc_detach() - detach ol txrx cfg
 * @cfg: ol txrx cfg ctxt
 *
 * Return: void
 */
void ol_txrx_cfg_soc_detach(struct ol_txrx_cfg_soc_ctxt *cfg)
{
    qdf_mem_free(cfg);
}

/**
 * ol_txrx_cfg_get_dp_caps() - get dp capabilities
 * @soc_handle: datapath soc handle
 * @dp_caps: enum for dp capabilities
 *
 * Return: bool to determine if a dp caps is enabled
 */
bool
ol_txrx_cfg_get_dp_caps(struct ol_txrx_cfg_soc_ctxt *cfg,
            enum cdp_capabilities dp_caps)
{
    switch (dp_caps) {
        case CDP_CFG_DP_TSO:
            return cfg->tso_enabled;
            break;
        case CDP_CFG_DP_LRO:
            return cfg->lro_enabled;
            break;
        case CDP_CFG_DP_SG:
            return cfg->sg_enabled;
            break;
        case CDP_CFG_DP_GRO:
            return cfg->gro_enabled;
            break;
        case CDP_CFG_DP_OL_TX_CSUM:
            return cfg->ol_tx_csum_enabled;
            break;
        case CDP_CFG_DP_OL_RX_CSUM:
            return cfg->ol_rx_csum_enabled;
            break;
        case CDP_CFG_DP_RAWMODE:
            return cfg->rawmode_enabled;
            break;
        case CDP_CFG_DP_PEER_FLOW_CTRL:
            return cfg->peer_flow_ctrl_enabled;
            break;
        default:
            return false;
    }
}
