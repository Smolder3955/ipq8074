/*
 * Copyright (c) 2011, 2015-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011, 2015-2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

/*=== includes ===*/
/* header files for OS primitives */
#include <osdep.h>        /* u_int32_t, etc. */
#include <qdf_mem.h>   /* qdf_mem_malloc, etc. */
#include <qdf_types.h> /* qdf_device_t, qdf_print */
/* header files for utilities */
#include <queue.h>        /* TAILQ */

/* header files for configuration API */
#include <ol_cfg.h>       /* ol_cfg_max_peer_id */
/* header files for target type */
#include <target_type.h>
/* header files for our internal definitions */
#include <ol_txrx_dbg.h>       /* TXRX_DEBUG_LEVEL */
#include <ol_txrx_internal.h>  /* ol_txrx_pdev_t, etc. */
#include <ol_txrx_api_internal.h>  /* ol_txrx_peer_find_inact_timeout_handler, etc. */
#include <ol_txrx.h>           /* ol_txrx_peer_unref_delete */
#include <ol_txrx_peer_find.h> /* ol_txrx_peer_find_attach, etc. */
#include <init_deinit_lmac.h>

#if QCA_PARTNER_DIRECTLINK_RX
#define QCA_PARTNER_DIRECTLINK_OL_TXRX_PEER_FIND 1
#include "ath_carr_pltfrm.h"
#undef QCA_PARTNER_DIRECTLINK_OL_TXRX_PEER_FIND
#endif

/*=== misc. / utility function definitions ==================================*/

static int
ol_txrx_log2_ceil(unsigned value)
{
    unsigned tmp = value;
    int log2 = -1;

    while (tmp) {
        log2++;
        tmp >>= 1;
    }
    if (1 << log2 != value) {
        log2++;
    }
    return log2;
}

static int
ol_txrx_peer_find_add_id_to_obj(
    struct ol_txrx_peer_t *peer,
    u_int16_t peer_id)
{
    int i;

    for (i = 0; i < MAX_NUM_PEER_ID_PER_PEER; i++) {
        if (peer->peer_ids[i] == HTT_INVALID_PEER) {
            peer->peer_ids[i] = peer_id;
            return 0; /* success */
        }
    }
    return 1; /* failure */
}

/*=== function definitions for peer MAC addr --> peer object hash table =====*/

/*
 * TXRX_PEER_HASH_LOAD_FACTOR:
 * Multiply by 2 and divide by 2^0 (shift by 0), then round up to a
 * power of two.
 * This provides at least twice as many bins in the peer hash table
 * as there will be entries.
 * Having substantially more bins than spaces minimizes the probability of
 * having to compare MAC addresses.
 * Because the MAC address comparison is fairly efficient, it is okay if the
 * hash table is sparsely loaded, but it's generally better to use extra mem
 * to keep the table sparse, to keep the lookups as fast as possible.
 * An optimization would be to apply a more conservative loading factor for
 * high latency, where the lookup happens during the tx classification of
 * every tx frame, than for low-latency, where the lookup only happens
 * during association, when the PEER_MAP message is received.
 */
#define TXRX_PEER_HASH_LOAD_MULT  2
#define TXRX_PEER_HASH_LOAD_SHIFT 0

static int
ol_txrx_peer_find_hash_attach(struct ol_txrx_pdev_t *pdev)
{
    int i, hash_elems, log2;
    struct ol_txrx_psoc_t *dp_soc = (struct ol_txrx_psoc_t *) pdev->soc;

    /* allocate the peer MAC address -> peer object hash table */
    hash_elems = ol_cfg_max_peer_id(dp_soc->psoc_obj) + 1;
    hash_elems *= TXRX_PEER_HASH_LOAD_MULT;
    hash_elems >>= TXRX_PEER_HASH_LOAD_SHIFT;
    log2 = ol_txrx_log2_ceil(hash_elems);
    hash_elems = 1 << log2;

    pdev->peer_hash.mask = hash_elems - 1;
    pdev->peer_hash.idx_bits = log2;
    /* allocate an array of TAILQ peer object lists */
    pdev->peer_hash.bins = qdf_mem_malloc(
        hash_elems * sizeof(TAILQ_HEAD(anonymous_tail_q, ol_txrx_peer_t)));
    if (!pdev->peer_hash.bins) {
        return 1; /* failure */
    }

#if PEER_FLOW_CONTROL
    /* allocate an array of TAILQ peer object lists */
    pdev->ast_entry_hash.bins = qdf_mem_malloc(
            hash_elems * sizeof(TAILQ_HEAD(anonymous_ast_tail_q, ol_txrx_ast_entry_t)));

    if (!pdev->ast_entry_hash.bins) {
        return 1; /* failure */
    }
#endif

    for (i = 0; i < hash_elems; i++) {
        TAILQ_INIT(&pdev->peer_hash.bins[i]);
#if PEER_FLOW_CONTROL
        TAILQ_INIT(&pdev->ast_entry_hash.bins[i]);
#endif
	}

    return 0; /* success */
}

static void
ol_txrx_peer_find_hash_detach(struct ol_txrx_pdev_t *pdev)
{
#if PEER_FLOW_CONTROL
    struct ol_txrx_ast_entry_t *ast_entry = NULL;
    struct ol_txrx_ast_entry_t *tmp_ast_entry = NULL;
    int index;

    for (index = 0; index <= pdev->peer_hash.mask; index++) {
        if (!TAILQ_EMPTY(&pdev->ast_entry_hash.bins[index])) {
            TAILQ_FOREACH_SAFE(ast_entry, &pdev->ast_entry_hash.bins[index],
                               hash_list_elem, tmp_ast_entry) {
                TAILQ_REMOVE(&pdev->ast_entry_hash.bins[index], ast_entry, hash_list_elem);
                qdf_mem_free(ast_entry);
            }
        }
    }
    qdf_mem_free(pdev->ast_entry_hash.bins);
#endif
    qdf_mem_free(pdev->peer_hash.bins);
}

static inline unsigned
ol_txrx_peer_find_hash_index(
    struct ol_txrx_pdev_t *pdev,
    union ol_txrx_align_mac_addr_t *mac_addr)
{
    unsigned index;

    index =
        mac_addr->align2.bytes_ab ^
        mac_addr->align2.bytes_cd ^
        mac_addr->align2.bytes_ef;
    index ^= index >> pdev->peer_hash.idx_bits;
    index &= pdev->peer_hash.mask;
    return index;
}


void
ol_txrx_peer_find_hash_add(
    struct ol_txrx_pdev_t *pdev,
    struct ol_txrx_peer_t *peer)
{
    unsigned index;

    index = ol_txrx_peer_find_hash_index(pdev, &peer->mac_addr);
    qdf_spin_lock_bh(&pdev->peer_ref_mutex);
    /*
     * It is important to add the new peer at the tail of the peer list
     * with the bin index.  Together with having the hash_find function
     * search from head to tail, this ensures that if two entries with
     * the same MAC address are stored, the one added first will be
     * found first.
     */
    TAILQ_INSERT_TAIL(&pdev->peer_hash.bins[index], peer, hash_list_elem);
    qdf_spin_unlock_bh(&pdev->peer_ref_mutex);
}
#if ATH_SUPPORT_WRAP
struct ol_txrx_peer_t *
ol_txrx_peer_find_hash_find(
    struct ol_txrx_pdev_t *pdev,
    u_int8_t *peer_mac_addr,
    int mac_addr_is_aligned, u_int8_t vdev_id)
#else
struct ol_txrx_peer_t *
ol_txrx_peer_find_hash_find(
    struct ol_txrx_pdev_t *pdev,
    u_int8_t *peer_mac_addr,
    int mac_addr_is_aligned)
#endif
{
    union ol_txrx_align_mac_addr_t local_mac_addr_aligned, *mac_addr;
    unsigned index;
    struct ol_txrx_peer_t *peer;

    if (mac_addr_is_aligned) {
        mac_addr = (union ol_txrx_align_mac_addr_t *) peer_mac_addr;
    } else {
        qdf_mem_copy(
            &local_mac_addr_aligned.raw[0],
            peer_mac_addr, QDF_MAC_ADDR_SIZE);
        mac_addr = &local_mac_addr_aligned;
    }
    index = ol_txrx_peer_find_hash_index(pdev, mac_addr);
    qdf_spin_lock_bh(&pdev->peer_ref_mutex);
    TAILQ_FOREACH(peer, &pdev->peer_hash.bins[index], hash_list_elem) {
#if ATH_SUPPORT_WRAP
        /* ProxySTA may have multiple BSS peer with same MAC address, modified
         * find will take care of finding the correct BSS peer.
         */
        if (ol_txrx_peer_find_mac_addr_cmp(mac_addr, &peer->mac_addr) == 0 && (peer->vdev->vdev_id==vdev_id))
#else
        if (ol_txrx_peer_find_mac_addr_cmp(mac_addr, &peer->mac_addr) == 0)
#endif
        {
            /* found it - increment the ref count before releasing the lock */
            qdf_atomic_inc(&peer->ref_cnt);
            qdf_spin_unlock_bh(&pdev->peer_ref_mutex);
            return peer;
        }
    }
    qdf_spin_unlock_bh(&pdev->peer_ref_mutex);
    return NULL; /* failure */
}

void
ol_txrx_peer_find_hash_remove(
    struct ol_txrx_pdev_t *pdev,
    struct ol_txrx_peer_t *peer)
{
    unsigned index;
    struct ol_txrx_peer_t *tmppeer = NULL;
    int found = 0;

    index = ol_txrx_peer_find_hash_index(pdev, &peer->mac_addr);
    /*
     * DO NOT take the peer_ref_mutex lock here - it needs to be taken
     * by the caller.
     * The caller needs to hold the lock from the time the peer object's
     * reference count is decremented and tested up through the time the
     * reference to the peer object is removed from the hash table, by
     * this function.
     * Holding the lock only while removing the peer object reference
     * from the hash table keeps the hash table consistent, but does not
     * protect against a new HL tx context starting to use the peer object
     * if it looks up the peer object from its MAC address just after the
     * peer ref count is decremented to zero, but just before the peer
     * object reference is removed from the hash table.
     */
     TAILQ_FOREACH(tmppeer, &pdev->peer_hash.bins[index], hash_list_elem) {
          if (tmppeer == peer) {
               found = 1;
          break;
          }
     }
     /* Mac address might have corrupted and not giving proper hash index to delete
      * Traverse entire hash table to find and delete the peer*/
     if (!found) {
          qdf_print("%s:Peer (%pK) not found at hash index %d traversing complete "
               "hash table peer_mac %02x:%02x:%02x:%02x:%02x:%02x ", __func__,
               peer, index, peer->mac_addr.raw[0], peer->mac_addr.raw[1],
               peer->mac_addr.raw[2], peer->mac_addr.raw[3],
               peer->mac_addr.raw[4], peer->mac_addr.raw[5]);
          for (index = 0; index <= pdev->peer_hash.mask; index++) {
               if (!TAILQ_EMPTY(&pdev->peer_hash.bins[index])) {
                    TAILQ_FOREACH(tmppeer, &pdev->peer_hash.bins[index], hash_list_elem) {
                         if (tmppeer == peer) {
                              found = 1;
                         break;
                         }
                    }
               }
               if (found) {
                    break;
               }
          }
     }
     KASSERT ((found), ("peer %pK not found in pdev (%pK)->peer_hash.bins:%pK index:%u\n", peer, pdev,
                         &pdev->peer_hash.bins, index));
    //qdf_spin_lock_bh(&pdev->peer_ref_mutex);
    TAILQ_REMOVE(&pdev->peer_hash.bins[index], peer, hash_list_elem);
    //qdf_spin_unlock_bh(&pdev->peer_ref_mutex);
}

void
ol_txrx_ast_find_hash_add(
        struct ol_txrx_pdev_t *pdev,
        u_int8_t *peer_mac_addr,
        u_int8_t *dest_mac_addr,
        u_int16_t peer_id,
        int mac_addr_is_aligned)
{

    struct ol_txrx_ast_entry_t *ast_entry, *basepeer_ast_entry;
    unsigned index;
    union ol_txrx_align_mac_addr_t local_mac_addr_aligned, *mac_addr;

#ifdef OL_TXRX_AST_DEBUG
    qdf_print("%pK %s: peer_id %d ", pdev, __func__, peer_id);

    qdf_print("%pK %s: peer_mac %02x:%02x:%02x:%02x:%02x:%02x", pdev, __func__, peer_mac_addr[0],peer_mac_addr[1],peer_mac_addr[2],peer_mac_addr[3],peer_mac_addr[4],peer_mac_addr[5]);

    qdf_print("%pK %s: dest_mac %02x:%02x:%02x:%02x:%02x:%02x", pdev, __func__, dest_mac_addr[0],dest_mac_addr[1],dest_mac_addr[2],dest_mac_addr[3],dest_mac_addr[4],dest_mac_addr[5]);
#endif
    if ((peer_id != HTT_INVALID_PEER) && (pdev->peer_id_to_obj_map[peer_id] == NULL)) {
        return;
    }

    ast_entry = qdf_mem_malloc(sizeof(*ast_entry));

    if (!ast_entry) {
        qdf_print("%s: ast_entry alloc failed for peer_id %d,peer_mac %02x:%02x:%02x:%02x:%02x:%02x ",__func__,peer_id,peer_mac_addr[0],peer_mac_addr[1],peer_mac_addr[2],peer_mac_addr[3],peer_mac_addr[4],peer_mac_addr[5]);
        return;
    }

    if (mac_addr_is_aligned) {
        mac_addr = (union ol_txrx_align_mac_addr_t *) dest_mac_addr;
    } else {
        qdf_mem_copy(
                &local_mac_addr_aligned.raw[0],
                dest_mac_addr, QDF_MAC_ADDR_SIZE);
        mac_addr = &local_mac_addr_aligned;
    }

    index = ol_txrx_peer_find_hash_index(pdev, mac_addr);

    /* For WDS peers, peer_id will be passed as HTT_INVALID_PEER */
    if (peer_id == HTT_INVALID_PEER) {
        basepeer_ast_entry = ol_txrx_ast_find_hash_find(pdev, peer_mac_addr, mac_addr_is_aligned);
        if (likely(basepeer_ast_entry != NULL)) {
            peer_id = basepeer_ast_entry->peer_id;
        }
    }

    qdf_mem_copy(
            &ast_entry->dest_mac_addr.raw[0],
            dest_mac_addr, QDF_MAC_ADDR_SIZE);

    ast_entry->peer_id = peer_id;

    /*
     * Use a PDEV Tx Lock here, because AST entries are used in Tx path, for classification
     */
    OL_TX_PEER_UPDATE_LOCK(pdev, peer_id);
    /*
     * It is important to add the new peer at the tail of the peer list
     * with the bin index.  Together with having the hash_find function
     * search from head to tail, this ensures that if two entries with
     * the same MAC address are stored, the one added first will be
     * found first.
     */
    TAILQ_INSERT_TAIL(&pdev->ast_entry_hash.bins[index], ast_entry, hash_list_elem);

    OL_TX_PEER_UPDATE_UNLOCK(pdev, peer_id);
}

/* ol_txrx_peer_get_ast_info_by_pdevid:
 * Retrieve ast_info with the help of the pdev_id and ast_mac_addr
 *
 * @soc_hdl: SoC Handle
 * @ast_mac_addr: MAC Address of AST Peer
 * @pdev_id: PDEV ID
 * @ast_entry_info: AST Entry Info Structure
 *
 * Returns false for failure
 *         true for success
*/
bool ol_txrx_peer_get_ast_info_by_pdevid(struct cdp_soc_t *soc_hdl,
                                          uint8_t *ast_mac_addr,
                                          uint8_t pdev_id,
                                          struct cdp_ast_entry_info *ast_entry_info)
{
    struct ol_txrx_psoc_t *soc            = (struct ol_txrx_psoc_t *)soc_hdl;
    struct ol_txrx_pdev_t *pdev           = NULL;
    struct ol_txrx_ast_entry_t *ast_entry = NULL;
    struct ol_txrx_peer_t *ast_peer       = NULL;

    if (pdev_id < MAX_PDEV_COUNT)
        pdev = (struct ol_txrx_pdev_t *)soc->pdev[pdev_id];
    else {
        qdf_err("Invalid pdev_id");
        return false;
    }

    ast_entry = ol_txrx_ast_find_hash_find(pdev, ast_mac_addr, 0);
    if (!ast_entry) {
        qdf_err("Could not find ast entry");
        return false;
    }

    ast_peer = ol_txrx_peer_find_by_id((ol_txrx_pdev_handle)pdev, ast_entry->peer_id);
    if (!ast_peer) {
        qdf_err("Could not find ast peer");
        return false;
    }

    IEEE80211_ADDR_COPY(ast_entry_info->peer_mac_addr, ast_peer->mac_addr.raw);
    /* WDS type by default for legacy chipsets */
    ast_entry_info->type    = CDP_TXRX_AST_TYPE_WDS;
    ast_entry_info->vdev_id = ast_peer->vdev->vdev_id;
    ast_entry_info->pdev_id = ast_peer->vdev->pdev->pdev_id;
    ast_entry_info->peer_id = ast_entry->peer_id;

    return true;
}

struct ol_txrx_ast_entry_t *
ol_txrx_ast_find_hash_find(
        struct ol_txrx_pdev_t *pdev,
        u_int8_t *peer_mac_addr,
        int mac_addr_is_aligned)
{
    union ol_txrx_align_mac_addr_t local_mac_addr_aligned, *mac_addr;
    unsigned index;
    struct ol_txrx_ast_entry_t *ast_entry;

    if (!pdev) {
        return NULL;
    }

    if (mac_addr_is_aligned) {
        mac_addr = (union ol_txrx_align_mac_addr_t *) peer_mac_addr;
    } else {
        qdf_mem_copy(
                &local_mac_addr_aligned.raw[0],
                peer_mac_addr, QDF_MAC_ADDR_SIZE);
        mac_addr = &local_mac_addr_aligned;
    }


#ifdef OL_TXRX_AST_DEBUG
    qdf_print("%s %pK: peer_mac %02x:%02x:%02x:%02x:%02x:%02x", __func__, pdev, peer_mac_addr[0],peer_mac_addr[1],peer_mac_addr[2],peer_mac_addr[3],peer_mac_addr[4],peer_mac_addr[5]);

    qdf_print("%s %pK: aligned mac %02x:%02x:%02x:%02x:%02x:%02x", __func__, pdev, mac_addr->raw[0],mac_addr->raw[1],mac_addr->raw[2],mac_addr->raw[3],mac_addr->raw[4],mac_addr->raw[5]);
#endif

    index = ol_txrx_peer_find_hash_index(pdev, mac_addr);

    TAILQ_FOREACH(ast_entry, &pdev->ast_entry_hash.bins[index], hash_list_elem) {
        if (ol_txrx_peer_find_mac_addr_cmp(mac_addr, &ast_entry->dest_mac_addr) == 0)
        {
            /* found it - increment the ref count before releasing the lock */
            return ast_entry;
        }
    }

#ifdef OL_TXRX_AST_DEBUG
    qdf_print("%s ast_entry not found ", __func__);
#endif

    return NULL; /* failure */
}
qdf_export_symbol(ol_txrx_ast_find_hash_find);

    int8_t
ol_txrx_ast_find_hash_remove(
        struct ol_txrx_pdev_t *pdev,
        u_int8_t *peer_mac_addr)
{
    unsigned index;
    struct ol_txrx_ast_entry_t *ast_entry;
    union ol_txrx_align_mac_addr_t *mac_addr = (union ol_txrx_align_mac_addr_t *) peer_mac_addr;

#ifdef OL_TXRX_AST_DEBUG
    qdf_print("%s %pK: peer mac %02x:%02x:%02x:%02x:%02x:%02x", __func__, pdev, mac_addr->raw[0],mac_addr->raw[1],mac_addr->raw[2],mac_addr->raw[3],mac_addr->raw[4],mac_addr->raw[5]);
#endif

    index = ol_txrx_peer_find_hash_index(pdev, mac_addr);
    ast_entry = ol_txrx_ast_find_hash_find(pdev, peer_mac_addr, 1);

    if (!ast_entry) {
        qdf_print("%s ast entry delete failed ", __func__);
        return -1;
    }

    /*
     * Use a PDEV Tx Lock here, because AST entries are used in Tx path, for classification
     */
    OL_TX_PEER_UPDATE_LOCK(pdev, ast_entry->peer_id);
    TAILQ_REMOVE(&pdev->ast_entry_hash.bins[index], ast_entry, hash_list_elem);
    OL_TX_PEER_UPDATE_UNLOCK(pdev, ast_entry->peer_id);

    /* Once ast_entry is removed from hash table, free the memory */
    qdf_mem_free(ast_entry);
    return 0;
}

void
ol_txrx_peer_find_hash_erase(struct ol_txrx_pdev_t *pdev)
{
    int i;
    /*
     * Not really necessary to take peer_ref_mutex lock - by this point,
     * it's known that the pdev is no longer in use.
     */

    for (i = 0; i <= pdev->peer_hash.mask; i++) {
        if (!TAILQ_EMPTY(&pdev->peer_hash.bins[i])) {
            struct ol_txrx_peer_t *peer, *peer_next;

			/*
			 * TAILQ_FOREACH_SAFE must be used here to avoid any memory access
			 * violation after peer is freed
			 */
			TAILQ_FOREACH_SAFE(
                peer, &pdev->peer_hash.bins[i], hash_list_elem, peer_next)
			{
				/*
                 * Don't remove the peer from the hash table -
                 * that would modify the list we are currently traversing,
                 * and it's not necessary anyway.
                 */
                /*
                 * Artificially adjust the peer's ref count to 1, so it
                 * will get deleted by ol_txrx_peer_unref_delete.
                 */
                qdf_atomic_init(&peer->ref_cnt); /* set to zero */
                qdf_atomic_inc(&peer->ref_cnt);  /* incr to one */
                ol_txrx_ast_find_hash_remove(pdev, (u_int8_t *)&peer->mac_addr);
                ol_txrx_peer_unref_delete(peer);
            }
        }
    }
}

/*=== function definitions for peer id --> peer object map ==================*/

static int
ol_txrx_peer_find_map_attach(struct ol_txrx_pdev_t *pdev)
{
    int max_peers, peer_map_size;

    struct ol_txrx_psoc_t *dp_soc = (struct ol_txrx_psoc_t *) pdev->soc;
    /* allocate the peer ID -> peer object map */
    max_peers = ol_cfg_max_peer_id(dp_soc->psoc_obj) + 1;
    pdev->max_peers = max_peers;
    printk(KERN_INFO"\n<=== cfg max peer id %d ====>\n",max_peers);
    peer_map_size = max_peers * sizeof(pdev->peer_id_to_obj_map[0]);
    pdev->peer_id_to_obj_map = qdf_mem_malloc(peer_map_size);
    if (!pdev->peer_id_to_obj_map) {
        return 1; /* failure */
    }

    /*
     * The peer_id_to_obj_map doesn't really need to be initialized,
     * since elements are only used after they have been individually
     * initialized.
     * However, it is convenient for debugging to have all elements
     * that are not in use set to 0.
     */
    qdf_mem_set(pdev->peer_id_to_obj_map, peer_map_size, 0);
    return 0; /* success */
}

static void
ol_txrx_peer_find_map_detach(struct ol_txrx_pdev_t *pdev)
{
    qdf_mem_free(pdev->peer_id_to_obj_map);
}

static inline void
ol_txrx_peer_find_add_id(
    struct ol_txrx_pdev_t *pdev,
    u_int8_t *peer_mac_addr,
    u_int16_t peer_id,
    u_int8_t vdev_id)
{
    struct ol_txrx_peer_t *peer;

    TXRX_ASSERT1(peer_id <= ol_cfg_max_peer_id(dp_soc->psoc_obj) + 1);
    /* check if there's already a peer object with this MAC address */
#if ATH_SUPPORT_WRAP
    peer = ol_txrx_peer_find_hash_find(pdev, peer_mac_addr, 0 /* is aligned */, vdev_id);
#else
    peer = ol_txrx_peer_find_hash_find(pdev, peer_mac_addr, 0 /* is aligned */);
#endif
    TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1,
        "%s: peer %pK ID %d vid %d mac %02x:%02x:%02x:%02x:%02x:%02x\n", __func__, peer, peer_id, vdev_id, peer_mac_addr[0],peer_mac_addr[1],peer_mac_addr[2],peer_mac_addr[3],peer_mac_addr[4],peer_mac_addr[5]);

    if (peer) {
        /*
         * Use a PDEV Tx Lock here, because AST entries are used in Tx path, for classification
         */
        OL_TX_PEER_UPDATE_LOCK(pdev, peer_id);
        /* peer's ref count was already incremented by peer_find_hash_find */
        pdev->peer_id_to_obj_map[peer_id] = peer;
        OL_TX_PEER_UPDATE_UNLOCK(pdev, peer_id);

        if (ol_txrx_peer_find_add_id_to_obj(peer, peer_id)) {
            /* TBDXXX: assert for now */
            qdf_assert(0);
        }

        if ((peer->vdev) && (peer->vdev->opmode == wlan_op_mode_sta)) {
            peer->vdev->sta_peer_id = peer->peer_ids[0];
        }
#if QCA_PARTNER_DIRECTLINK_RX
        /* provide peer add information to partner side */
        if (CE_is_directlink(pdev->ce_tx_hdl)) {
            ol_txrx_peer_find_add_id_partner(peer, peer_id, vdev_id);
        }
#endif /* QCA_PARTNER_DIRECTLINK_RX */

        return;
    }
    /*
     * Currently peer IDs are assigned for vdevs as well as peers.
     * If the peer ID is for a vdev, then we will fail to find a peer
     * with a matching MAC address.
     */
    //TXRX_ASSERT2(0);
}

/*=== allocation / deallocation function definitions ========================*/

int
ol_txrx_peer_find_attach(struct ol_txrx_pdev_t *pdev)
{
    if (ol_txrx_peer_find_map_attach(pdev)) {
        return 1;
    }
    if (ol_txrx_peer_find_hash_attach(pdev)) {
        ol_txrx_peer_find_map_detach(pdev);
        return 1;
    }
    return 0; /* success */
}

void
ol_txrx_peer_find_detach(struct ol_txrx_pdev_t *pdev)
{
    ol_txrx_peer_find_map_detach(pdev);
    ol_txrx_peer_find_hash_detach(pdev);
}

/*=== function definitions for message handling =============================*/

#if PEER_FLOW_CONTROL
static void
ol_txrx_peer_config_max_buf(struct ol_txrx_pdev_t *pdev)
{
#if MIPS_LOW_PERF_SUPPORT
    return;
#else
    uint32_t tgt_type;

    if (pdev->ctrl_pdev == NULL)
        return;

    lmac_get_pdev_target_type((struct wlan_objmgr_pdev *)
				pdev->ctrl_pdev, &tgt_type);

    if (pdev->carrier_vow_config)
        return;
    if (tgt_type == TARGET_TYPE_IPQ4019) {
        return;
    }

    if (pdev->num_active_peers > OL_TX_PFLOW_CTRL_ACTIVE_PEERS_RANGE2_MAX) {
        pdev->pflow_ctl_max_buf_global = OL_TX_PFLOW_CTRL_MAX_BUF_GLOBAL;
    } else if (pdev->num_active_peers > OL_TX_PFLOW_CTRL_ACTIVE_PEERS_RANGE1_MAX) {
        pdev->pflow_ctl_max_buf_global = OL_TX_PFLOW_CTRL_MAX_BUF_2;
    } else if (pdev->num_active_peers > OL_TX_PFLOW_CTRL_ACTIVE_PEERS_RANGE0_MAX) {
        pdev->pflow_ctl_max_buf_global = OL_TX_PFLOW_CTRL_MAX_BUF_1;
    } else {
        pdev->pflow_ctl_max_buf_global = OL_TX_PFLOW_CTRL_MAX_BUF_0;
    }
#endif
}
#endif

void
ol_rx_peer_map_handler(
    struct ol_txrx_pdev_t *pdev,
    u_int16_t peer_id,
    u_int8_t vdev_id,
    u_int8_t *peer_mac_addr)
{

#if PEER_FLOW_CONTROL
    struct ol_txrx_peer_t *peer;
#endif

    TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1,
        "peer_map_event (pdev:%pK): peer_id %d, peer_mac %02x:%02x:%02x:%02x:%02x:%02x , vdev_id %d \n",
         pdev, peer_id, peer_mac_addr[0],peer_mac_addr[1],peer_mac_addr[2],peer_mac_addr[3],peer_mac_addr[4],peer_mac_addr[5], vdev_id);

#if PEER_FLOW_CONTROL
    peer = ol_txrx_peer_find_by_id((ol_txrx_pdev_handle) pdev, peer_id);

    /*
     * If the peer id is already assigned, ignore the duplicate peer map event
     */
    if (peer != NULL) {
        return;
    }
#endif

    ol_txrx_peer_find_add_id(pdev, peer_mac_addr, peer_id, vdev_id);
#if PEER_FLOW_CONTROL
    ol_txrx_ast_find_hash_add(pdev, peer_mac_addr, peer_mac_addr, peer_id, 0);
    pdev->num_active_peers++;
    ol_txrx_peer_config_max_buf(pdev);
#endif
}

void
ol_rx_peer_unmap_handler(
    struct ol_txrx_pdev_t *pdev,
    u_int16_t peer_id)
{
    struct ol_txrx_peer_t *peer;
    u_int8_t i;
#if PEER_FLOW_CONTROL
    uint32_t tgt_type;
#endif
    peer = ol_txrx_peer_find_by_id((ol_txrx_pdev_handle)pdev, peer_id);

    TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1,
        "peer_unmap_event (pdev:%pK) peer_id %d peer %pK \n", pdev, peer_id, peer);

    /*
     * Currently peer IDs are assigned for vdevs as well as peers.
     * If the peer ID is for a vdev, then the peer pointer stored
     * in peer_id_to_obj_map will be NULL.
     */
    if (!peer) return;

    if ((peer->vdev) && (peer->vdev->sta_peer_id == peer_id)) {
        peer->vdev->sta_peer_id = HTT_INVALID_PEER;
    }
#if PEER_FLOW_CONTROL && defined(WLAN_FEATURE_FASTPATH)
    if ((peer->vdev) && (peer->vdev->pdev != pdev)) {
        qdf_print("%s: Peer lookup returned invalid peer %pK peer_id %d, pdev %pK", __func__, peer, peer_id, pdev);
        return;
    }

    if(ol_txrx_ast_find_hash_remove(pdev, (u_int8_t *)&peer->mac_addr)) {
       union ol_txrx_align_mac_addr_t *mac_addr = NULL;
       mac_addr = (union ol_txrx_align_mac_addr_t *)&peer->mac_addr;
       qdf_print("%s:Peer map has not handled properly for this, peer %pK,peer_id %d,peer_mac %02x:%02x:%02x:%02x:%02x:%02x ", __func__,peer,peer_id,mac_addr->raw[0],mac_addr->raw[1],mac_addr->raw[2],mac_addr->raw[3],mac_addr->raw[4],mac_addr->raw[5]);
       return;
    }

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    if(!pdev->nss_wifiol_ctx)
#endif
    {
        ol_tx_flush_peer_queue_pflow_ctrl((ol_txrx_pdev_handle) pdev, peer_id);
    }
#endif

#if PEER_FLOW_CONTROL
    lmac_get_pdev_target_type((struct wlan_objmgr_pdev *)
				pdev->ctrl_pdev, &tgt_type);
    if (tgt_type == TARGET_TYPE_AR9888) {
        pdev->peer_id_to_obj_map[peer_id] = NULL;
        for (i = 0; i < MAX_NUM_PEER_ID_PER_PEER; i++) {
            if (peer->peer_ids[i] == peer_id) {
                peer->peer_ids[i] = HTT_INVALID_PEER;
                break;
            }
        }
    } else {
        /*
         * Use a PDEV Tx Lock here, because peer used in Tx path for peer/TID enqueue and dequeue
         */
        OL_TX_PEER_UPDATE_LOCK(pdev, peer_id);
        pdev->peer_id_to_obj_map[peer_id] = NULL;
        /* to avoid clearing the obj_map[] later when the peer_id is allocated for
         * another peer.
        */
        for (i = 0; i < MAX_NUM_PEER_ID_PER_PEER; i++) {
            if (peer->peer_ids[i] == peer_id) {
                peer->peer_ids[i] = HTT_INVALID_PEER;
                break;
            }
        }
        OL_TX_PEER_UPDATE_UNLOCK(pdev, peer_id);
    }
#else
    pdev->peer_id_to_obj_map[peer_id] = NULL;
    for (i = 0; i < MAX_NUM_PEER_ID_PER_PEER; i++) {
        if (peer->peer_ids[i] == peer_id) {
            peer->peer_ids[i] = HTT_INVALID_PEER;
            break;
        }
    }
#endif

#if QCA_PARTNER_DIRECTLINK_RX
    /* provide peer unmap information to partner side */
    if (CE_is_directlink(pdev->ce_tx_hdl)) {
        ol_rx_peer_unmap_handler_partner(peer, peer_id);
    }
#endif /* QCA_PARTNER_DIRECTLINK_RX */

    /*
     * Remove a reference to the peer.
     * If there are no more references, delete the peer object.
     */
    ol_txrx_peer_unref_delete(peer);
#if PEER_FLOW_CONTROL
    pdev->num_active_peers--;
    ol_txrx_peer_config_max_buf(pdev);
#endif
}

/*=== function definitions for debug ========================================*/

#if TXRX_DEBUG_LEVEL > 5
void
ol_txrx_peer_find_display(ol_txrx_pdev_handle pdev, int indent)
{
    int i, max_peers;

    struct ol_txrx_psoc_t *dp_soc = (struct ol_txrx_psoc_t *) pdev->soc;
    qdf_print("%*speer map:", indent, " ");
    max_peers = ol_cfg_max_peer_id(dp_soc->psoc_obj) + 1;
    for (i = 0; i < max_peers; i++) {
        if (pdev->peer_id_to_obj_map[i]) {
            qdf_print("%*sid %d -> %pK",
                indent+4, " ", i, pdev->peer_id_to_obj_map[i]);
        }
    }
    qdf_print("%*speer hash table:", indent, " ");
    for (i = 0; i <= pdev->peer_hash.mask; i++) {
        if (!TAILQ_EMPTY(&pdev->peer_hash.bins[i])) {
            struct ol_txrx_peer_t *peer;
            TAILQ_FOREACH(peer, &pdev->peer_hash.bins[i], hash_list_elem) {
                qdf_print(
                    "%*shash idx %d -> %pK (%02x:%02x:%02x:%02x:%02x:%02x)",
                    indent+4, " ", i, peer,
                    peer->mac_addr.raw[0], peer->mac_addr.raw[1],
                    peer->mac_addr.raw[2], peer->mac_addr.raw[3],
                    peer->mac_addr.raw[4], peer->mac_addr.raw[5]);
            }
        }
    }
}
#endif /* if TXRX_DEBUG_LEVEL */
