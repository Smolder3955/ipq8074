/*
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#ifndef OL_RATECTRL_IF_H_
#define OL_RATECTRL_IF_H_

#include <ol_txrx_ctrl_api.h>

void* ol_ratectrl_vdev_ctxt_attach(
        struct ol_txrx_pdev_t *pdev,
        struct ol_txrx_vdev_t *vdev);

void ol_ratectrl_vdev_ctxt_detach(void *vdev_ratectxt);

void* ol_ratectrl_peer_ctxt_attach(
        struct ol_txrx_pdev_t *pdev,
        struct ol_txrx_vdev_t *vdev,
        struct ol_txrx_peer_t *peer);

void ol_ratectrl_peer_ctxt_update(struct ol_txrx_peer_t *peer,
        struct peer_ratectrl_params_t *peer_ratectrl_params);

void ol_ratectrl_peer_ctxt_detach(void *peer_ratectxt);

void ol_ratectrl_enable_host_ratectrl(ol_txrx_pdev_handle pdev,
        u_int32_t enable);

void ol_rc_update_handler(ol_txrx_pdev_handle pdev, u_int16_t peer_id,
        u_int8_t vdev_id, u_int8_t *peer_mac_addr, u_int8_t num_elems,
        void *args);


#endif /* OL_RATECTRL_IF_H_ */
