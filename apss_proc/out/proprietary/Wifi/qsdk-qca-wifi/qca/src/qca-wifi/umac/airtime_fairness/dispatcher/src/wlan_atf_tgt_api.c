/*
 *
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 */

#include <wlan_atf_tgt_api.h>
#include "../../core/atf_cmn_api_i.h"

uint8_t tgt_atf_get_atf_commit(struct wlan_objmgr_pdev *pdev)
{
	struct pdev_atf *pa;

	if (NULL == pdev) {
		atf_err("PDEV is NULL!\n");
		return 0;
	}
	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_ATF);
	if (NULL == pa) {
		atf_err("pdev_atf component object NULL!\n");
		return 0;
	}

	return pa->atf_commit;
}

uint32_t tgt_atf_get_fmcap(struct wlan_objmgr_psoc *psoc)
{
	struct atf_context *ac = NULL;

	if (NULL == psoc) {
		atf_err("PSOC is NULL!\n");
		return 0;
	}
	ac = wlan_objmgr_psoc_get_comp_private_obj(psoc, WLAN_UMAC_COMP_ATF);
	if (NULL == ac) {
		atf_err("atf context is NULL!\n");
		return 0;
	}

	return ac->atf_fmcap;
}

uint32_t tgt_atf_get_obss_scale(struct wlan_objmgr_pdev *pdev)
{
	struct pdev_atf *pa;

	if (NULL == pdev) {
		atf_err("PDEV is NULL!\n");
		return 0;
	}
	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_ATF);
	if (NULL == pa) {
		atf_err("pdev_atf component object NULL!\n");
		return 0;
	}

	return pa->atf_obss_scale;
}

uint32_t tgt_atf_get_mode(struct wlan_objmgr_psoc *psoc)
{
	struct atf_context *ac = NULL;

	if (NULL == psoc) {
		atf_err("PSOC is NULL!\n");
		return 0;
	}
	ac = wlan_objmgr_psoc_get_comp_private_obj(psoc, WLAN_UMAC_COMP_ATF);
	if (NULL == ac) {
		atf_err("atf context is NULL!\n");
		return 0;
	}

	return ac->atf_mode;
}

uint32_t tgt_atf_get_msdu_desc(struct wlan_objmgr_psoc *psoc)
{
	struct atf_context *ac = NULL;

	if (NULL == psoc) {
		atf_err("PSOC is NULL!\n");
		return 0;
	}
	ac = wlan_objmgr_psoc_get_comp_private_obj(psoc, WLAN_UMAC_COMP_ATF);
	if (NULL == ac) {
		atf_err("atf context is NULL!\n");
		return 0;
	}

	return ac->atf_msdu_desc;
}

uint32_t tgt_atf_get_max_vdevs(struct wlan_objmgr_psoc *psoc)
{
	struct atf_context *ac = NULL;

	if (NULL == psoc) {
		atf_err("PSOC is NULL!\n");
		return 0;
	}
	ac = wlan_objmgr_psoc_get_comp_private_obj(psoc, WLAN_UMAC_COMP_ATF);
	if (NULL == ac) {
		atf_err("atf context is NULL!\n");
		return 0;
	}

	return ac->atf_max_vdevs;
}

uint32_t tgt_atf_get_peers(struct wlan_objmgr_psoc *psoc)
{
	struct atf_context *ac = NULL;

	if (NULL == psoc) {
		atf_err("PSOC is NULL!\n");
		return 0;
	}
	ac = wlan_objmgr_psoc_get_comp_private_obj(psoc, WLAN_UMAC_COMP_ATF);
	if (NULL == ac) {
		atf_err("atf context is NULL!\n");
		return 0;
	}

	return ac->atf_peers;
}

uint32_t tgt_atf_get_tput_based(struct wlan_objmgr_pdev *pdev)
{
	struct pdev_atf *pa;

	if (NULL == pdev) {
		atf_err("PDEV is NULL!\n");
		return 0;
	}
	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_ATF);
	if (NULL == pa) {
		atf_err("pdev_atf component object NULL!\n");
		return 0;
	}

	return pa->atf_tput_based;
}

uint32_t tgt_atf_get_logging(struct wlan_objmgr_pdev *pdev)
{
	struct pdev_atf *pa;

	if (NULL == pdev) {
		atf_err("PDEV is NULL!\n");
		return 0;
	}
	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_ATF);
	if (NULL == pa) {
		atf_err("pdev_atf component object NULL!\n");
		return 0;
	}

	return pa->atf_logging;
}

void* tgt_atf_update_buf_held(struct wlan_objmgr_peer *peer, int8_t ac)
{
	struct peer_atf *pa;
	struct atf_subgroup_list *subgroup = NULL;

	if (NULL == peer) {
		atf_err("Invalid inputs!\n");
		return NULL;
	}
	pa = wlan_objmgr_peer_get_comp_private_obj(peer, WLAN_UMAC_COMP_ATF);
	if (NULL == pa) {
		atf_err("peer_atf component object NULL!\n");
		return NULL;
	}
	if (ac >= WME_NUM_AC) {
		atf_err("Invalid AC id \n");
		return NULL;
	}
	subgroup = pa->ac_list_ptr[ac];
	if (NULL == subgroup) {
		atf_err("subgroup is NULL!\n");
		return NULL;
	}
	qdf_spin_lock_bh(&subgroup->atf_sg_lock);
	subgroup->sg_num_buf_held++;
	if (subgroup->sg_num_buf_held > subgroup->sg_max_num_buf_held) {
		subgroup->sg_max_num_buf_held = subgroup->sg_num_buf_held;
	}
	qdf_spin_unlock_bh(&subgroup->atf_sg_lock);

	return subgroup;
}

uint32_t tgt_atf_get_ssidgroup(struct wlan_objmgr_pdev *pdev)
{
	struct pdev_atf *pa;

	if (NULL == pdev) {
		atf_err("PDEV is NULL!\n");
		return 0;
	}
	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_ATF);
	if (NULL == pa) {
		atf_err("pdev_atf component object NULL!\n");
		return 0;
	}

	return pa->atf_ssidgroup;
}

uint32_t tgt_atf_get_vdev_ac_blk_cnt(struct wlan_objmgr_vdev *vdev)
{
	struct vdev_atf *va;

	if (NULL == vdev) {
		atf_err("VDEV is NULL!\n");
		return 0;
	}
	va = wlan_objmgr_vdev_get_comp_private_obj(vdev, WLAN_UMAC_COMP_ATF);
	if (NULL == va) {
		atf_err("vdev_atf component object NULL!\n");
		return 0;
	}

	return va->ac_blk_cnt;
}

uint8_t tgt_atf_get_vdev_blk_txtraffic(struct wlan_objmgr_vdev *vdev)
{
	struct vdev_atf *va;

	if (NULL == vdev) {
		atf_err("VDEV is NULL!\n");
		return 0;
	}
	va = wlan_objmgr_vdev_get_comp_private_obj(vdev, WLAN_UMAC_COMP_ATF);
	if (NULL == va) {
		atf_err("vdev_atf component object NULL!\n");
		return 0;
	}

	return va->block_tx_traffic;
}

uint8_t tgt_atf_get_peer_blk_txbitmap(struct wlan_objmgr_peer *peer)
{
	struct peer_atf *pa;

	if (NULL == peer) {
		atf_err("PEER is NULL!\n");
		return 0;
	}
	pa = wlan_objmgr_peer_get_comp_private_obj(peer, WLAN_UMAC_COMP_ATF);
	if (NULL == pa) {
		atf_err("peer_atf component object NULL!\n");
		return 0;
	}

	return pa->block_tx_bitmap;
}

uint32_t tgt_atf_get_sched(struct wlan_objmgr_pdev *pdev)
{
	struct pdev_atf *pa;

	if (NULL == pdev) {
		atf_err("PDEV is NULL!\n");
		return 0;
	}
	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_ATF);
	if (NULL == pa) {
		atf_err("pdev_atf component object NULL!\n");
		return 0;
	}

	return pa->atf_sched;
}

void tgt_atf_get_peer_stats(struct wlan_objmgr_peer *peer,
			    struct atf_stats *stats)
{
	struct peer_atf *pa;

	if ((NULL == peer) || (NULL == stats)) {
		atf_err("Invalid inputs!\n");
		return;
	}
	pa = wlan_objmgr_peer_get_comp_private_obj(peer, WLAN_UMAC_COMP_ATF);
	if (NULL == pa) {
		atf_err("peer_atf component object NULL!\n");
		return;
	}

	qdf_mem_copy(stats, &pa->atf_peer_stats, sizeof(pa->atf_peer_stats));
}

QDF_STATUS tgt_atf_adjust_subgroup_txtokens(struct wlan_objmgr_peer *peer,
					    uint8_t ac,
					    uint32_t actual_duration,
					    uint32_t est_duration)
{
	struct peer_atf *pa = NULL;
	struct atf_subgroup_list *subgroup = NULL;
	uint8_t diff_duration = 0;

	if (NULL == peer) {
		atf_err("Invalid inputs!\n");
		return QDF_STATUS_E_INVAL;
	}
	pa = wlan_objmgr_peer_get_comp_private_obj(peer, WLAN_UMAC_COMP_ATF);
	if (NULL == pa) {
		atf_err("peer_atf component object NULL!\n");
		return QDF_STATUS_E_INVAL;
	}
	if (ac >= WME_NUM_AC) {
		atf_err("Invalid AC id \n");
		return QDF_STATUS_E_INVAL;
	}
	if (!est_duration) {
		return QDF_STATUS_E_INVAL;
	}
	subgroup = pa->ac_list_ptr[ac];
	if (subgroup) {
		qdf_spin_lock_bh(&subgroup->atf_sg_lock);
		if (actual_duration > est_duration) {
			diff_duration = actual_duration - est_duration;
			if (subgroup->tx_tokens >= diff_duration) {
				subgroup->tx_tokens -= diff_duration;
			} else {
				subgroup->tokens_excess_consumed +=
							diff_duration;
			}
		} else {
			diff_duration = est_duration - actual_duration;
			subgroup->tokens_less_consumed += diff_duration;
		}
		qdf_spin_unlock_bh(&subgroup->atf_sg_lock);
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS tgt_atf_account_subgroup_txtokens(struct wlan_objmgr_peer *peer,
					     uint8_t ac, uint32_t duration)
{
	struct peer_atf *pa;
	struct atf_subgroup_list *subgroup = NULL;
	uint8_t ret = QDF_STATUS_SUCCESS;

	if (NULL == peer) {
		atf_err("Invalid inputs!\n");
		return QDF_STATUS_E_INVAL;
	}
	pa = wlan_objmgr_peer_get_comp_private_obj(peer, WLAN_UMAC_COMP_ATF);
	if (NULL == pa) {
		atf_err("peer_atf component object NULL!\n");
		return QDF_STATUS_E_INVAL;
	}
	if (ac >= WME_NUM_AC) {
		atf_err("Invalid AC id \n");
		return QDF_STATUS_E_INVAL;
	}
	subgroup = pa->ac_list_ptr[ac];
	if (subgroup) {
		qdf_spin_lock_bh(&subgroup->atf_sg_lock);
		if (subgroup->tx_tokens >= duration) {
			subgroup->tx_tokens -= duration;
			ret = QDF_STATUS_SUCCESS;
		} else {
			ret = QDF_STATUS_E_INVAL;
		}
		qdf_spin_unlock_bh(&subgroup->atf_sg_lock);
	}

	return ret;
}

QDF_STATUS tgt_atf_subgroup_free_buf(uint16_t buf_acc_size, void *bf_atf_sg)
{
	struct atf_subgroup_list *subgroup = NULL;

	subgroup = (struct atf_subgroup_list *)bf_atf_sg;
	if (subgroup) {
		qdf_spin_lock_bh(&subgroup->atf_sg_lock);
		if (subgroup->sg_num_buf_held == 0) {
			atf_err("an with buffer marked for atf accounting is holding no buffers\n");
		} else {
			subgroup->sg_num_buf_held--;
			if (subgroup->sg_num_buf_held <
			    subgroup->sg_min_num_buf_held) {
				subgroup->sg_min_num_buf_held =
						subgroup->sg_num_buf_held;
			}
		}
		subgroup->sg_num_bufs_sent++;
		subgroup->sg_num_bytes_sent += buf_acc_size;
		qdf_spin_unlock_bh(&subgroup->atf_sg_lock);
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS tgt_atf_update_subgroup_tidstate(struct wlan_objmgr_peer *peer,
					    uint8_t atf_nodepaused)
{
	struct atf_subgroup_list *subgroup = NULL;
	struct peer_atf *pa = NULL;
	int8_t i;

	if (NULL == peer) {
		atf_err("Invalid Peer\n");
		return QDF_STATUS_E_INVAL;
	}
	pa = wlan_objmgr_peer_get_comp_private_obj(peer, WLAN_UMAC_COMP_ATF);
	if (NULL == pa) {
		return QDF_STATUS_E_INVAL;
	}

	for (i = 0; i < WME_NUM_AC; i++) {
		subgroup = pa->ac_list_ptr[i];
		if (subgroup) {
			qdf_spin_lock_bh(&subgroup->atf_sg_lock);
			subgroup->sg_atftidstate = atf_nodepaused;
			qdf_spin_unlock_bh(&subgroup->atf_sg_lock);
		}
	}

	return QDF_STATUS_SUCCESS;
}

uint8_t tgt_atf_get_subgroup_airtime(struct wlan_objmgr_peer *peer,
            uint8_t ac)
{
	struct peer_atf *pa;
	struct atf_subgroup_list *subgroup = NULL;
	uint8_t airtime = 0;
	uint8_t used_tokens = 0;

	if (NULL == peer) {
		atf_err("Invalid inputs!\n");
		return 1;
	}
	pa = wlan_objmgr_peer_get_comp_private_obj(peer, WLAN_UMAC_COMP_ATF);
	if (NULL == pa) {
		atf_err("peer_atf component object NULL!\n");
		return 1;
	}

	subgroup = pa->ac_list_ptr[ac];
	if (subgroup) {
		qdf_spin_lock_bh(&subgroup->atf_sg_lock);
		/* 1-1 Mapping between peer and subgroup exists only if the subgroup
		 * is of peer type(ATF Peer configuration exists).
		 * Per node usage cannot be fetched for other subgroup types.
		 */
		if (subgroup->sg_atf_stats.tokens) {
			used_tokens = subgroup->sg_atf_stats.tokens - subgroup->sg_atf_stats.unused;
			airtime = (used_tokens * 100) / subgroup->sg_atf_stats.tokens;
		}
		qdf_spin_unlock_bh(&subgroup->atf_sg_lock);
	}

	return airtime;
}

uint32_t tgt_atf_get_tx_tokens(struct wlan_objmgr_peer *peer)
{
	struct peer_atf *pa;

	if (NULL == peer) {
		atf_err("PEER is NULL!\n");
		return 0;
	}
	pa = wlan_objmgr_peer_get_comp_private_obj(peer, WLAN_UMAC_COMP_ATF);
	if (NULL == pa) {
		atf_err("peer_atf component object NULL!\n");
		return 0;
	}

	return pa->tx_tokens;
}

uint32_t tgt_atf_buf_distribute(struct wlan_objmgr_pdev *pdev,
				struct wlan_objmgr_peer *peer, int8_t ac)
{
	struct peer_atf *peeratf;
	struct pdev_atf *pa;
	struct atf_subgroup_list *subgroup = NULL;
	u_int32_t tx_tokens = 0, max_allowed_buffers = 0, num_buffers_held = 0;

	if (NULL == peer) {
		atf_err("Peer is NULL\n");
		return ATF_BUF_ALLOC_SKIP;
	}
	peeratf = wlan_objmgr_peer_get_comp_private_obj(peer,
							WLAN_UMAC_COMP_ATF);
	if (NULL == peeratf) {
		atf_err("peer_atf component object NULL!\n");
		return ATF_BUF_ALLOC_SKIP;
	}
	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_ATF);
	if (NULL == pa) {
		atf_err("pdev_atf component object NULL \n");
		return ATF_BUF_ALLOC_SKIP;
	}
	if (ac >= WME_NUM_AC) {
		atf_err("Invalid AC id \n");
		return ATF_BUF_ALLOC_SKIP;
	}
	subgroup = peeratf->ac_list_ptr[ac];
	if (NULL == subgroup) {
		atf_err("subgroup is NULL!\n");
		return ATF_BUF_ALLOC_SKIP;
	}
	tx_tokens = subgroup->sg_shadow_tx_tokens;
	if (pa->atf_txbuf_share && pa->atf_commit &&
	    pa->shadow_alloted_tx_tokens) {
		qdf_spin_lock_bh(&subgroup->atf_sg_lock);
		if (subgroup->sg_allowed_buf_updated == 0) {
			max_allowed_buffers = (((tx_tokens * 100) /
			    pa->shadow_alloted_tx_tokens) * ATH_TXBUF) / 100;
		if (max_allowed_buffers > pa->atf_txbuf_max) {
			max_allowed_buffers = pa->atf_txbuf_max;
		} else if (max_allowed_buffers < pa->atf_txbuf_min) {
			max_allowed_buffers = pa->atf_txbuf_min;
		}
		subgroup->sg_allowed_bufs = max_allowed_buffers;
		subgroup->sg_allowed_buf_updated = 1;
		} else {
			max_allowed_buffers = subgroup->sg_allowed_bufs;
		}
		/* Check if we can allocate a new buffer */
		num_buffers_held = subgroup->sg_num_buf_held;
		if (pa->atf_txbuf_share == 2) {
			/* Force a drop, to be used for testing only */
			subgroup->sg_pkt_drop_nobuf++;
			qdf_spin_unlock_bh(&subgroup->atf_sg_lock);
			return ATF_BUF_NOALLOC;
		} else {
			if (num_buffers_held < max_allowed_buffers) {
				qdf_spin_unlock_bh(&subgroup->atf_sg_lock);
				return ATF_BUF_ALLOC;
			} else {
				subgroup->sg_pkt_drop_nobuf++;
				qdf_spin_unlock_bh(&subgroup->atf_sg_lock);
				return ATF_BUF_NOALLOC;
			}
		}
	}

	return ATF_BUF_ALLOC_SKIP;
}

uint32_t tgt_atf_get_shadow_alloted_tx_tokens(struct wlan_objmgr_pdev *pdev)
{
	struct pdev_atf *pa;

	if (NULL == pdev) {
		atf_err("PDEV is NULL!\n");
		return 0;
	}
	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_ATF);
	if (NULL == pa) {
		atf_err("pdev_atf component object NULL!\n");
		return 0;
	}

	return pa->shadow_alloted_tx_tokens;
}

uint32_t tgt_atf_get_txtokens_common(struct wlan_objmgr_pdev *pdev)
{
	struct pdev_atf *pa;

	if (NULL == pdev) {
		atf_err("PDEV is NULL!\n");
		return 0;
	}
	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_ATF);
	if (NULL == pa) {
		atf_err("pdev_atf component object NULL!\n");
		return 0;
	}

	return pa->txtokens_common;
}

uint16_t tgt_atf_get_token_allocated(struct wlan_objmgr_peer *peer)
{
	struct peer_atf *pa;

	if (NULL == peer) {
		atf_err("PEER is NULL!\n");
		return -1;
	}
	pa = wlan_objmgr_peer_get_comp_private_obj(peer, WLAN_UMAC_COMP_ATF);
	if (NULL == pa) {
		atf_err("peer_atf component object NULL!\n");
		return 0;
	}

	return pa->atf_token_allocated;
}

uint16_t tgt_atf_get_token_utilized(struct wlan_objmgr_peer *peer)
{
	struct peer_atf *pa;

	if (NULL == peer) {
		atf_err("PEER is NULL!\n");
		return -1;
	}
	pa = wlan_objmgr_peer_get_comp_private_obj(peer, WLAN_UMAC_COMP_ATF);
	if (NULL == pa) {
		atf_err("peer_atf component object NULL!\n");
		return 0;
	}

	return pa->atf_token_utilized;
}

void tgt_atf_set_sched(struct wlan_objmgr_pdev *pdev, uint32_t value)
{
	struct pdev_atf *pa;

	if (NULL == pdev) {
		atf_err("PDEV is NULL!\n");
		return;
	}
	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_ATF);
	if (NULL == pa) {
		atf_err("pdev_atf component object NULL!\n");
		return;
	}

	pa->atf_sched = value;
}

void tgt_atf_set_fmcap(struct wlan_objmgr_psoc *psoc, uint32_t value)
{
	struct atf_context *ac = NULL;

	if (NULL == psoc) {
		atf_err("PSOC is NULL!\n");
		return;
	}
	ac = wlan_objmgr_psoc_get_comp_private_obj(psoc, WLAN_UMAC_COMP_ATF);
	if (NULL == ac) {
		atf_err("atf context is NULL!\n");
		return;
	}

	ac->atf_fmcap = !!value;
}

void tgt_atf_set_obss_scale(struct wlan_objmgr_pdev *pdev, uint32_t value)
{
	struct pdev_atf *pa;

	if (NULL == pdev) {
		atf_err("PDEV is NULL!\n");
		return;
	}
	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_ATF);
	if (NULL == pa) {
		atf_err("pdev_atf component object NULL!\n");
		return;
	}

	pa->atf_obss_scale = value;
}

void tgt_atf_set_msdu_desc(struct wlan_objmgr_psoc *psoc, uint32_t value)
{
	struct atf_context *ac = NULL;

	if (NULL == psoc) {
		atf_err("PSOC is NULL!\n");
		return;
	}
	ac = wlan_objmgr_psoc_get_comp_private_obj(psoc, WLAN_UMAC_COMP_ATF);
	if (NULL == ac) {
		atf_err("atf context is NULL!\n");
		return;
	}

	ac->atf_msdu_desc = value;
}

void tgt_atf_set_max_vdevs(struct wlan_objmgr_psoc *psoc, uint32_t value)
{
	struct atf_context *ac = NULL;

	if (NULL == psoc) {
		atf_err("PSOC is NULL!\n");
		return;
	}
	ac = wlan_objmgr_psoc_get_comp_private_obj(psoc, WLAN_UMAC_COMP_ATF);
	if (NULL == ac) {
		atf_err("atf context is NULL!\n");
		return;
	}

	ac->atf_max_vdevs = value;
}

void tgt_atf_set_peers(struct wlan_objmgr_psoc *psoc, uint32_t value)
{
	struct atf_context *ac = NULL;

	if (NULL == psoc) {
		atf_err("PSOC is NULL!\n");
		return;
	}
	ac = wlan_objmgr_psoc_get_comp_private_obj(psoc, WLAN_UMAC_COMP_ATF);
	if (NULL == ac) {
		atf_err("atf context is NULL!\n");
		return;
	}

	ac->atf_peers = value;
}

void tgt_atf_set_peer_stats(struct wlan_objmgr_peer *peer,
			    struct atf_stats *stats)
{
	struct peer_atf *pa;

	if ((NULL == peer) || (NULL == stats)) {
		atf_err("Invalid inputs!\n");
		return;
	}
	pa = wlan_objmgr_peer_get_comp_private_obj(peer, WLAN_UMAC_COMP_ATF);
	if (NULL == pa) {
		atf_err("peer_atf component object NULL!\n");
		return;
	}

	qdf_mem_copy(&pa->atf_peer_stats, stats, sizeof(pa->atf_peer_stats));
}

static void
atf_peer_block_unblock(struct wlan_objmgr_vdev *vdev, void *object, void *arg) {
	struct wlan_objmgr_peer *peer = (struct wlan_objmgr_peer *)object;
	int8_t *block = (int8_t*)arg;
	struct peer_atf *pa;
	struct vdev_atf *va;

	if (NULL == peer) {
		atf_err ("peer is NULL!\n");
		return;
	}
	pa = wlan_objmgr_peer_get_comp_private_obj(peer, WLAN_UMAC_COMP_ATF);
	if (NULL == pa) {
		atf_err("peer_atf component object NULL!\n");
		return;
	}
	va = wlan_objmgr_vdev_get_comp_private_obj(vdev, WLAN_UMAC_COMP_ATF);
	if (NULL == va) {
		atf_err("vdev_atf component object NULL!\n");
		return;
	}

	if (!*block) {
		pa->block_tx_bitmap = 0;
		va->block_tx_traffic = 0;
		va->ac_blk_cnt = 0;
		pa->ac_blk_cnt = 0;
		atf_debug("Peer:%s bitmap:0x%08x blktxtraffic:%d acblkct:%d \n",
			  ether_sprintf(peer->macaddr), pa->block_tx_bitmap,
			  va->block_tx_traffic, va->ac_blk_cnt);
	} else {
		pa->block_tx_bitmap = ATF_TX_BLOCK_AC_ALL;
		va->block_tx_traffic = !!block;
		va->ac_blk_cnt = wlan_vdev_get_peer_count(vdev) * WME_NUM_AC;
		pa->ac_blk_cnt += WME_NUM_AC;
		atf_debug("peer:%s bitmap:0x%08x blktxtraffic:%d acblkct:%d \n",
			  ether_sprintf(peer->macaddr), pa->block_tx_bitmap,
			  va->block_tx_traffic, va->ac_blk_cnt);
	}
}

void
tgt_atf_set_vdev_blk_txtraffic(struct wlan_objmgr_vdev *vdev, uint8_t block)
{
	struct vdev_atf *va;

	if (NULL == vdev) {
		atf_err("VDEV is NULL!\n");
		return;
	}
	va = wlan_objmgr_vdev_get_comp_private_obj(vdev, WLAN_UMAC_COMP_ATF);
	if (NULL == va) {
		atf_err("vdev_atf component object NULL!\n");
		return;
	}

	wlan_objmgr_iterate_peerobj_list(vdev,
					 atf_peer_block_unblock,
					 &block,
					 WLAN_UMAC_COMP_ATF);

}

uint8_t atf_peer_num_ac_blocked(uint8_t ac_bitmap)
{
	uint8_t num_ac_blocked = 0;

	while (ac_bitmap) {
		ac_bitmap &= ac_bitmap - 1;
		num_ac_blocked++;
	}

	return num_ac_blocked;
}

void
tgt_atf_peer_blk_txtraffic(struct wlan_objmgr_peer *peer, int8_t ac_id)
{
	struct peer_atf *pa;
	struct wlan_objmgr_vdev *vdev;
	struct vdev_atf *va;
	int8_t ac_blocked = 0;

	if (NULL == peer) {
		atf_err("PEER is NULL!\n");
		return;
	}
	pa = wlan_objmgr_peer_get_comp_private_obj(peer, WLAN_UMAC_COMP_ATF);
	if (NULL == pa) {
		atf_err("peer_atf component object NULL!\n");
		return;
	}
	vdev = wlan_peer_get_vdev(peer);
	if (NULL == vdev) {
		atf_err("vdev is NULL!\n");
		return;
	}
	va = wlan_objmgr_vdev_get_comp_private_obj(vdev, WLAN_UMAC_COMP_ATF);
	if (NULL == va) {
		atf_err("vdev_atf component object NULL!\n");
		return;
	}

	if (ac_id == ATF_TX_BLOCK_AC_ALL) {
		ac_blocked = atf_peer_num_ac_blocked(pa->block_tx_bitmap);
		pa->block_tx_bitmap |= ATF_TX_BLOCK_AC_ALL;
		va->ac_blk_cnt += (WME_NUM_AC - ac_blocked);
		pa->ac_blk_cnt += (WME_NUM_AC - ac_blocked);
		atf_debug("peer:%s vapac blk_cnt:%d peerblock count:0x%08x \n",
			  ether_sprintf(peer->macaddr), va->ac_blk_cnt,
			  pa->block_tx_bitmap);
	} else {
		pa->block_tx_bitmap |= (1 << (ac_id-1));
		va->ac_blk_cnt++;
		pa->ac_blk_cnt++;
		atf_debug("peer:%s vapac blk_cnt: %d peerblock count:0x%08x \n",
			   ether_sprintf(peer->macaddr), va->ac_blk_cnt,
			   pa->block_tx_bitmap);

	}
}

void
tgt_atf_peer_unblk_txtraffic(struct wlan_objmgr_peer *peer, int8_t ac_id)
{
	struct peer_atf *pa;
	struct wlan_objmgr_vdev *vdev;
	struct vdev_atf *va;
	uint8_t ac_blocked = 0;

	if (NULL == peer) {
		atf_err("PEER is NULL!\n");
		return;
	}
	pa = wlan_objmgr_peer_get_comp_private_obj(peer, WLAN_UMAC_COMP_ATF);
	if (NULL == pa) {
		atf_err("peer_atf component object NULL!\n");
		return;
	}
        vdev = wlan_peer_get_vdev(peer);
	if (NULL == vdev) {
		atf_err("vdev is NULL!\n");
		return;
	}
	va = wlan_objmgr_vdev_get_comp_private_obj(vdev, WLAN_UMAC_COMP_ATF);
	if (NULL == va) {
		atf_err("vdev_atf component object NULL!\n");
		return;
	}

	if (pa->block_tx_bitmap) {
		if (va->ac_blk_cnt && (ac_id == ATF_TX_BLOCK_AC_ALL)) {
			ac_blocked = atf_peer_num_ac_blocked(pa->block_tx_bitmap);
			pa->block_tx_bitmap &= ~(ATF_TX_BLOCK_AC_ALL);
			va->ac_blk_cnt -= ac_blocked;
			pa->ac_blk_cnt -= ac_blocked;
			atf_debug("peer:%s vapacblkcnt:%d peerblkcnt:0x%08x\n",
				  ether_sprintf(peer->macaddr), va->ac_blk_cnt,
				  pa->block_tx_bitmap);
		} else {
			pa->block_tx_bitmap &= ~(1 << (ac_id-1));
			if (va->ac_blk_cnt) {
				va->ac_blk_cnt--;
				pa->ac_blk_cnt--;
				atf_debug("peer%s vpacblkct%d prblkcnt0x%08x\n",
					  ether_sprintf(peer->macaddr),
					  va->ac_blk_cnt, pa->block_tx_bitmap);
			}
		}
	}
}

void tgt_atf_set_token_allocated(struct wlan_objmgr_peer *peer, uint16_t value)
{
	struct peer_atf *pa;

	if (NULL == peer) {
		return;
	}
	pa = wlan_objmgr_peer_get_comp_private_obj(peer, WLAN_UMAC_COMP_ATF);
	if (NULL == pa) {
		return;
	}

	pa->atf_token_allocated = value;
}

void tgt_atf_set_token_utilized(struct wlan_objmgr_peer *peer, uint16_t value)
{
	struct peer_atf *pa;

	if (NULL == peer) {
		return;
	}
	pa = wlan_objmgr_peer_get_comp_private_obj(peer, WLAN_UMAC_COMP_ATF);
	if (NULL == pa) {
		return;
	}

	pa->atf_token_utilized = value;
}

