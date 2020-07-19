/*
 * Copyright (c) 2011, 2015, 2017 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary . Qualcomm Innovation Center, Inc.
 *
 * 2011, 2015 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
/**
 * @file ol_txrx_peer_find.h
 * @brief Define the API for the rx peer lookup datapath module.
 */
#ifndef _OL_TXRX_PEER_FIND__H_
#define _OL_TXRX_PEER_FIND__H_

#include <htt.h>              /* HTT_INVALID_PEER */
#include <ol_txrx_types.h>    /* ol_txrx_pdev_t, etc. */
#include <ol_txrx_internal.h> /* TXRX_ASSERT */

int ol_txrx_peer_find_attach(struct ol_txrx_pdev_t *pdev);

void ol_txrx_peer_find_detach(struct ol_txrx_pdev_t *pdev);

static inline
int
ol_txrx_peer_find_mac_addr_cmp(
    union ol_txrx_align_mac_addr_t *mac_addr1,
    union ol_txrx_align_mac_addr_t *mac_addr2)
{
    return
        !((mac_addr1->align4.bytes_abcd == mac_addr2->align4.bytes_abcd)
        /*
         * Intentionally use & rather than &&.
         * because the operands are binary rather than generic boolean,
         * the functionality is equivalent.
         * Using && has the advantage of short-circuited evaluation,
         * but using & has the advantage of no conditional branching,
         * which is a more significant benefit.
         */
        &
        (mac_addr1->align4.bytes_ef == mac_addr2->align4.bytes_ef));
}

static inline
struct ol_txrx_peer_t *
ol_txrx_peer_find_by_id(
    ol_txrx_pdev_handle pdev,
    u_int16_t peer_id)
{
    struct ol_txrx_peer_t *peer;
    peer = (peer_id == HTT_INVALID_PEER) ? NULL :
        ((struct ol_txrx_pdev_t *)pdev)->peer_id_to_obj_map[peer_id];
    /*
     * Currently, peer IDs are assigned to vdevs as well as peers.
     * If the peer ID is for a vdev, the peer_id_to_obj_map entry
     * will hold NULL rather than a valid peer pointer.
     */
    //TXRX_ASSERT2(peer != NULL);
    return peer;
}

void
ol_txrx_peer_find_hash_add(
    struct ol_txrx_pdev_t *pdev,
    struct ol_txrx_peer_t *peer);

#if ATH_SUPPORT_WRAP
struct ol_txrx_peer_t *
ol_txrx_peer_find_hash_find(
    struct ol_txrx_pdev_t *pdev,
    u_int8_t *peer_mac_addr,
    int mac_addr_is_aligned,u_int8_t vdev_id);
#else
struct ol_txrx_peer_t *
ol_txrx_peer_find_hash_find(
    struct ol_txrx_pdev_t *pdev,
    u_int8_t *peer_mac_addr,
    int mac_addr_is_aligned);
#endif
void
ol_txrx_peer_find_hash_remove(
    struct ol_txrx_pdev_t *pdev,
    struct ol_txrx_peer_t *peer);

void
ol_txrx_ast_find_hash_add(
        struct ol_txrx_pdev_t *pdev,
        u_int8_t *peer_mac_addr,
        u_int8_t *dest_mac_addr,
        u_int16_t peer_id,
        int mac_addr_is_aligned);

int8_t
ol_txrx_ast_find_hash_remove(
        struct ol_txrx_pdev_t *pdev,
        u_int8_t *peer_mac_addr);

struct ol_txrx_ast_entry_t *
ol_txrx_ast_find_hash_find(
        struct ol_txrx_pdev_t *pdev,
        u_int8_t *peer_mac_addr,
        int mac_addr_is_aligned);

void
ol_txrx_peer_find_hash_erase(
    struct ol_txrx_pdev_t *pdev);

void
ol_tx_rawsim_getastentry(struct cdp_vdev *vdev_handle, qdf_nbuf_t *pnbuf, struct cdp_raw_ast *raw_ast_handle);

#if TXRX_DEBUG_LEVEL > 5
void ol_txrx_peer_find_display(ol_txrx_pdev_handle pdev, int indent);
#else
#define ol_txrx_peer_find_display(pdev, indent)
#endif /* TXRX_DEBUG_LEVEL */

#endif /* _OL_TXRX_PEER_FIND__H_ */
