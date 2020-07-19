/*
 * Copyright (c) 2011, 2017-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * Copyright (c) 2010, Atheros Communications Inc.
 * All Rights Reserved.
 *
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 */

#include <ieee80211_var.h>
#include <qdf_nbuf.h>
#include <wlan_mlme_dispatcher.h>
#include <wlan_fd_utils_api.h>
#include <ieee80211_objmgr_priv.h>
#include "../../core/fd_priv_i.h"

#define WLAN_FD_MIN_HEAD_ROOM 64

static QDF_STATUS
wlan_fd_psoc_obj_create_handler(struct wlan_objmgr_psoc *psoc, void *arg)
{
	struct fd_context *fd_ctx;

	if (psoc == NULL) {
		fd_err("Invalid PSOC!\n");
		return QDF_STATUS_E_INVAL;
	}
	fd_ctx = qdf_mem_malloc(sizeof(*fd_ctx));
	if (fd_ctx == NULL) {
		fd_err("Memory allocation faild!!\n");
		return QDF_STATUS_E_NOMEM;
	}
	fd_ctx->psoc_obj = psoc;
	if (WLAN_DEV_DA == wlan_objmgr_psoc_get_dev_type(psoc))
		fd_ctx_init_da(fd_ctx);
	else if (WLAN_DEV_OL == wlan_objmgr_psoc_get_dev_type(psoc))
		fd_ctx_init_ol(fd_ctx);

	wlan_objmgr_psoc_component_obj_attach(psoc, WLAN_UMAC_COMP_FD,
			(void *)fd_ctx, QDF_STATUS_SUCCESS);

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
wlan_fd_psoc_obj_destroy_handler(struct wlan_objmgr_psoc *psoc, void *arg)
{
	struct fd_context *fd_ctx;

	if (psoc == NULL) {
		fd_err("Invalid PSOC!\n");
		return QDF_STATUS_E_INVAL;
	}

	fd_ctx = wlan_objmgr_psoc_get_comp_private_obj(psoc, WLAN_UMAC_COMP_FD);
	if (fd_ctx) {
		wlan_objmgr_psoc_component_obj_detach(psoc, WLAN_UMAC_COMP_FD,
				(void *)fd_ctx);
		fd_ctx_deinit(fd_ctx);
		qdf_mem_free(fd_ctx);
	}

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
wlan_fd_vdev_obj_create_handler(struct wlan_objmgr_vdev *vdev, void *arg)
{
	struct fd_vdev *fv;
	struct wlan_objmgr_psoc *psoc;

	if (vdev == NULL) {
		fd_err("VDEV is NULL!!\n");
		return QDF_STATUS_E_INVAL;
	}
	psoc = wlan_vdev_get_psoc(vdev);
	if (psoc == NULL) {
		fd_err("Invalid PSOC!\n");
		return QDF_STATUS_E_INVAL;
	}

	if (QDF_SAP_MODE == wlan_vdev_mlme_get_opmode(vdev)) {
		fv = qdf_mem_malloc(sizeof(*fv));
		if (fv == NULL) {
			fd_err("Memory allocation faild!!\n");
			return QDF_STATUS_E_NOMEM;
		}
		qdf_mem_zero(fv, sizeof(*fv));
		fv->vdev_obj = vdev;
		qdf_spinlock_create(&fv->fd_lock);
		qdf_list_create(&fv->fd_deferred_list,
				WLAN_FD_DEFERRED_MAX_SIZE);
		wlan_objmgr_vdev_component_obj_attach(vdev, WLAN_UMAC_COMP_FD,
				(void *)fv, QDF_STATUS_SUCCESS);
	} else {
		return QDF_STATUS_COMP_DISABLED;
	}

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
wlan_fd_vdev_obj_destroy_handler(struct wlan_objmgr_vdev *vdev, void *arg)
{
	struct fd_vdev *fv;
	struct wlan_objmgr_psoc *psoc;

	if (vdev == NULL) {
		fd_err("VDEV is NULL!!\n");
		return QDF_STATUS_E_INVAL;
	}
	psoc = wlan_vdev_get_psoc(vdev);
	if (psoc == NULL) {
		fd_err("Invalid PSOC!\n");
		return QDF_STATUS_E_INVAL;
	}

	fv = wlan_objmgr_vdev_get_comp_private_obj(vdev, WLAN_UMAC_COMP_FD);
	if (fv) {
		wlan_objmgr_vdev_component_obj_detach(vdev, WLAN_UMAC_COMP_FD,
					(void *)fv);
		qdf_spin_lock_bh(&fv->fd_lock);
		fd_free_list(psoc, &fv->fd_deferred_list);
		qdf_spin_unlock_bh(&fv->fd_lock);
		qdf_list_destroy(&fv->fd_deferred_list);
		qdf_spinlock_destroy(&fv->fd_lock);
		qdf_mem_free(fv);
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_fd_init(void)
{
	if (wlan_objmgr_register_psoc_create_handler(WLAN_UMAC_COMP_FD,
		wlan_fd_psoc_obj_create_handler, NULL) != QDF_STATUS_SUCCESS) {
		goto fail_psoc_create;
	}
	if (wlan_objmgr_register_psoc_destroy_handler(WLAN_UMAC_COMP_FD,
		wlan_fd_psoc_obj_destroy_handler, NULL) != QDF_STATUS_SUCCESS) {
		goto fail_psoc_destroy;
	}
	if (wlan_objmgr_register_vdev_create_handler(WLAN_UMAC_COMP_FD,
		wlan_fd_vdev_obj_create_handler, NULL) != QDF_STATUS_SUCCESS) {
		goto fail_vdev_create;
	}
	if (wlan_objmgr_register_vdev_destroy_handler(WLAN_UMAC_COMP_FD,
		wlan_fd_vdev_obj_destroy_handler, NULL) != QDF_STATUS_SUCCESS) {
		goto fail_vdev_destroy;
	}

	return QDF_STATUS_SUCCESS;

fail_vdev_destroy:
	wlan_objmgr_unregister_vdev_create_handler(WLAN_UMAC_COMP_FD,
					wlan_fd_vdev_obj_create_handler, NULL);
fail_vdev_create:
	wlan_objmgr_unregister_psoc_destroy_handler(WLAN_UMAC_COMP_FD,
					wlan_fd_psoc_obj_destroy_handler, NULL);
fail_psoc_destroy:
	wlan_objmgr_unregister_psoc_create_handler(WLAN_UMAC_COMP_FD,
					wlan_fd_psoc_obj_create_handler, NULL);
fail_psoc_create:
	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS wlan_fd_deinit(void)
{
	if (wlan_objmgr_unregister_psoc_create_handler(WLAN_UMAC_COMP_FD,
		wlan_fd_psoc_obj_create_handler, NULL) != QDF_STATUS_SUCCESS) {
		return QDF_STATUS_E_FAILURE;
	}
	if (wlan_objmgr_unregister_psoc_destroy_handler(WLAN_UMAC_COMP_FD,
		wlan_fd_psoc_obj_destroy_handler, NULL) != QDF_STATUS_SUCCESS) {
		return QDF_STATUS_E_FAILURE;
	}
	if (wlan_objmgr_unregister_vdev_create_handler(WLAN_UMAC_COMP_FD,
		wlan_fd_vdev_obj_create_handler, NULL) != QDF_STATUS_SUCCESS) {
		return QDF_STATUS_E_FAILURE;
	}
	if (wlan_objmgr_unregister_vdev_destroy_handler(WLAN_UMAC_COMP_FD,
		wlan_fd_vdev_obj_destroy_handler, NULL) != QDF_STATUS_SUCCESS) {
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_fd_enable(struct wlan_objmgr_psoc *psoc)
{
	struct fd_context *fd_ctx;

	if (psoc == NULL) {
		fd_err("Invalid PSOC!\n");
		return QDF_STATUS_E_INVAL;
	}
	fd_ctx = wlan_objmgr_psoc_get_comp_private_obj(psoc, WLAN_UMAC_COMP_FD);
	if (fd_ctx == NULL) {
		fd_err("Invalid FILS Discovery Context\n");
		return QDF_STATUS_E_INVAL;
	}

	if (fd_ctx->fd_enable)
		fd_ctx->fd_enable(psoc);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_fd_disable(struct wlan_objmgr_psoc *psoc)
{
	struct fd_context *fd_ctx;

	if (psoc == NULL) {
		fd_err("Invalid PSOC!\n");
		return QDF_STATUS_E_INVAL;
	}
	fd_ctx = wlan_objmgr_psoc_get_comp_private_obj(psoc, WLAN_UMAC_COMP_FD);
	if (fd_ctx == NULL) {
		fd_debug("Invalid FILS Discovery Context\n");
		return QDF_STATUS_E_INVAL;
	}

	if (fd_ctx->fd_disable)
		fd_ctx->fd_disable(psoc);

	return QDF_STATUS_SUCCESS;
}

uint8_t* wlan_fd_frame_init(struct wlan_objmgr_peer *peer, uint8_t *frm)
{
	uint16_t fd_cntl_subfield = 0;
	struct fd_action_header *fd_header;
	struct wlan_objmgr_vdev *vdev;
	struct ieee80211vap *vap;
	uint8_t *length;
	uint8_t ssid_len = 0, ssid[WLAN_SSID_MAX_LEN+1] = {0};
	uint32_t ielen = 0, shortssid = 0;
	uint8_t prim_chan;
	uint8_t op_class;
	uint8_t ch_seg1;

	vdev = wlan_peer_get_vdev(peer);
	if (vdev == NULL) {
		fd_err("VDEV is NULL!!\n");
		return frm;
	}

	vap = wlan_vdev_get_mlme_ext_obj(vdev);
	if (vap == NULL) {
		fd_err("vap is NULL!!\n");
		return frm;
	}

	fd_header = (struct fd_action_header *)frm;
	fd_header->action_header.ia_category = IEEE80211_ACTION_CAT_PUBLIC;
	fd_header->action_header.ia_action  = WLAN_ACTION_FILS_DISCOVERY;

	/**
	 * FILS DIscovery Frame Control Subfield - 2 byte
	 * Enable Short SSID
	 */
	fd_cntl_subfield = 3 & 0x1F;
	fd_cntl_subfield |= WLAN_FD_FRAMECNTL_SHORTSSID;

	/* For 80+80 set Primary channel Indicator and Channel center freq segment 1 */
	if (vap->iv_cur_mode == IEEE80211_MODE_11AC_VHT80_80) {
		fd_cntl_subfield |= WLAN_FD_FRAMECNTL_CH_CENTERFREQ;
		fd_cntl_subfield |= WLAN_FD_FRAMECNTL_PRIMARY_CH;
	}
#if ATH_SUPPORT_MBO
	if (ieee80211_vap_oce_check(vap) && ieee80211_non_oce_ap_present (vap)) {
		fd_cntl_subfield |= WLAN_FD_FRAMECNTL_NON_OCE_PRESENT;
	}
	if (ieee80211_vap_oce_check(vap) && ieee80211_11b_ap_present (vap)) {
		fd_cntl_subfield |= WLAN_FD_FRAMECNTL_11B_PRESENT;
	}
#endif
	fd_header->fd_frame_cntl = qdf_cpu_to_le16(fd_cntl_subfield);
	fd_debug("fd_cntl : %02x\n", fd_cntl_subfield);

	/* Timestamp - 8 byte */
	qdf_mem_zero(fd_header->timestamp, sizeof(fd_header->timestamp));

	/* Beacon Interval - 2 byte */
	fd_header->bcn_interval = qdf_cpu_to_le16(
				wlan_peer_get_beacon_interval(peer));
	fd_debug("bcn_intvl : %02x\n", fd_header->bcn_interval);
	frm = &fd_header->elem[0];

	wlan_vdev_mlme_get_ssid(vdev, ssid, &ssid_len);

	/* SSID/Short SSID - 1 - 32 byte */
	if (WLAN_FD_IS_SHORTSSID_PRESENT(fd_cntl_subfield)) {
		shortssid = ieee80211_construct_shortssid(ssid, ssid_len);
		*(uint32_t *)frm = qdf_cpu_to_le32(shortssid);
		frm += 4;
	} else {
		qdf_mem_copy(frm, ssid, ssid_len);
		frm += ssid_len;
	}
	/* Length - 1 byte */
	if (WLAN_FD_IS_LEN_PRESENT(fd_cntl_subfield)) {
		length = frm;
		frm++;
	}

	/**
	 * Operating Class
	 * Primary Channel
	 * AP configuration Sequence Number
	 * Access Network Options
	 * FD RSN Information
	 * Channel Center Freq Segment 1
	 * Mobility Domain
	 */

	/* Operating Class (0 or 1 byte) and Primary Channel (0 or 1 byte) */
	if (WLAN_FD_IS_FRAMECNTL_PRIMARY_CH(fd_cntl_subfield)) {
		op_class = wlan_get_opclass(vdev);
		prim_chan = wlan_get_prim_chan(vdev);
		*frm = op_class;
		frm++;
		*frm = prim_chan;
		frm++;
	}
	/* Channel Center Freq Segment 1 */
	if (WLAN_FD_IS_FRAMECNTL_CH_CENTERFREQ(fd_cntl_subfield)) {
		/* spec has seg0 and seg1 naming while we use seg1 and seg2 */
		ch_seg1 = wlan_get_seg1(vdev);
		*frm = ch_seg1;
		frm++;
	}
	/* Update the length field */
	if (WLAN_FD_IS_LEN_PRESENT(fd_cntl_subfield)) {
		/*Indicates length from FD cap to Mobility Domain */
		*length = (uint8_t)(frm - length) - 1;
	}
    /* Reduced Neighbour Report element */
    if (vap->rnr_enable && vap->rnr_enable_fd) {
        frm = ieee80211_add_rnr_ie(frm, vap, vap->iv_bss->ni_essid, vap->iv_bss->ni_esslen);
    }
	/**
	 * FILS Indication element
	 */
	if (wlan_vdev_get_elemid(vdev, IEEE80211_FRAME_TYPE_BEACON, frm,
		&ielen, WLAN_ELEMID_FILS_INDICATION) == QDF_STATUS_SUCCESS) {
		frm += ielen;
	}

	return frm;
}

qdf_nbuf_t wlan_fd_alloc(struct wlan_objmgr_vdev *vdev)
{
	uint8_t bcast[QDF_MAC_ADDR_SIZE] = {0xff,0xff,0xff,0xff,0xff,0xff};
	struct ieee80211_frame *wh;
	struct wlan_objmgr_peer *bss_peer;
	qdf_nbuf_t wbuf;
	uint8_t *frm;

	bss_peer = wlan_vdev_get_bsspeer(vdev);
	if (bss_peer == NULL) {
		fd_err("Invalid BSS Peer!!\n");
		return NULL;
	}
	wbuf = qdf_nbuf_alloc(NULL,
		qdf_roundup(MAX_TX_RX_PACKET_SIZE + WLAN_FD_MIN_HEAD_ROOM, 4),
			WLAN_FD_MIN_HEAD_ROOM, 4, true);
	if (wbuf == NULL) {
		fd_err("Failed to allocate qdf_nbuf!!\n");
		return NULL;
	}

	wh = (struct ieee80211_frame *)qdf_nbuf_data(wbuf);
	wh->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_MGT |
				IEEE80211_FC0_SUBTYPE_ACTION;
	wh->i_fc[1] = IEEE80211_FC1_DIR_NODS;
	*(uint16_t *)wh->i_dur = 0;
	WLAN_ADDR_COPY(wh->i_addr1, bcast);
	WLAN_ADDR_COPY(wh->i_addr2, wlan_vdev_mlme_get_macaddr(vdev));
	WLAN_ADDR_COPY(wh->i_addr3, wlan_peer_get_macaddr(bss_peer));
	*(uint16_t *)wh->i_seq = 0;

	frm = (uint8_t *)&wh[1];
	frm = wlan_fd_frame_init(bss_peer, frm);

	qdf_nbuf_set_pktlen(wbuf, (frm - (uint8_t *)qdf_nbuf_data(wbuf)));

	return wbuf;
}

void wlan_fd_vdev_defer_fd_buf_free(struct wlan_objmgr_vdev *vdev)
{
	struct fd_buf_entry* buf_entry;
	struct fd_vdev *fv;

	if (vdev == NULL) {
		fd_err("VDEV is NULL!!\n");
		return;
	}
	fv = wlan_objmgr_vdev_get_comp_private_obj(vdev, WLAN_UMAC_COMP_FD);
	if (fv == NULL) {
		fd_debug("Invalid FILS DISC object!\n");
		return;
	}
	if (!fv->fd_wbuf) {
		return;
	}
	buf_entry = qdf_mem_malloc(sizeof(*buf_entry));
	if (buf_entry == NULL) {
		fd_err("Memory allocation failed!\n");
		return;
	}

	qdf_spin_lock_bh(&fv->fd_lock);
	buf_entry->is_dma_mapped = fv->is_fd_dma_mapped;
	fv->is_fd_dma_mapped = false;
	buf_entry->fd_buf = fv->fd_wbuf;
	qdf_list_insert_back(&fv->fd_deferred_list,
				&buf_entry->fd_deferred_list_elem);
	fv->fd_wbuf = NULL;
	qdf_spin_unlock_bh(&fv->fd_lock);
}

void
wlan_fd_set_valid_fd_period(struct wlan_objmgr_vdev *vdev, uint32_t fd_period)
{
	uint16_t bcn_intval = 0;
	struct wlan_objmgr_peer *bss_peer;
	struct fd_vdev *fv;

	if (vdev == NULL) {
		fd_err("VDEV is NULL!!\n");
		return;
	}
	fv = wlan_objmgr_vdev_get_comp_private_obj(vdev, WLAN_UMAC_COMP_FD);
	if (fv == NULL) {
		fd_debug("Invalid FILS DISC object!\n");
		return;
	}
	bss_peer = wlan_vdev_get_bsspeer(vdev);
	if (bss_peer == NULL) {
		fd_err("Invalid bss peer\n");
		return;
	}
	bcn_intval = wlan_peer_get_beacon_interval(bss_peer);
	if ((fd_period < WLAN_FD_INTERVAL_MIN) ||
		(fd_period >= bcn_intval) || ((bcn_intval % fd_period) != 0)) {
		fd_info("Invalid FD Interval : %d. Disabling FD\n",
					fd_period);
		fd_period = 0;
	}

	fv->fd_period = fd_period;
}

bool wlan_fd_capable(struct wlan_objmgr_psoc *psoc)
{
	struct fd_context *fd_ctx;

	if (psoc == NULL) {
		fd_err("Invalid PSOC!\n");
		return false;
	}
	fd_ctx = wlan_objmgr_psoc_get_comp_private_obj(psoc, WLAN_UMAC_COMP_FD);
	if (fd_ctx == NULL) {
		fd_err("Invalid FD Context!\n");
		return false;
	}

	return fd_ctx->is_fd_capable;
}

QDF_STATUS wlan_fd_update(struct wlan_objmgr_vdev *vdev)
{
	uint8_t *frm = NULL;
	struct fd_vdev *fv;
	struct wlan_objmgr_peer *bss_peer;

	if (vdev == NULL) {
		fd_err("VDEV is NULL!!\n");
		return QDF_STATUS_E_INVAL;
	}
	fv = wlan_objmgr_vdev_get_comp_private_obj(vdev, WLAN_UMAC_COMP_FD);
	if (fv == NULL) {
		fd_debug("Invalid FILS DISC object!\n");
		return QDF_STATUS_E_INVAL;
	}
	bss_peer = wlan_vdev_get_bsspeer(fv->vdev_obj);
	if (bss_peer == NULL) {
		fd_err("Invalid BSS Peer!!\n");
		return QDF_STATUS_E_INVAL;
	}
	if (!fv->fd_wbuf) {
		fd_err("Invalid FD buffer!!\n");
		return QDF_STATUS_E_INVAL;
	}

	if (fv->fd_update) {
		frm = (uint8_t *)qdf_nbuf_data(fv->fd_wbuf) +
					sizeof(struct ieee80211_frame);
		frm = wlan_fd_frame_init(bss_peer, frm);
		qdf_nbuf_set_pktlen(fv->fd_wbuf,
			(frm - (uint8_t *)qdf_nbuf_data(fv->fd_wbuf)));
		fv->fd_update = 0;
	}

	return QDF_STATUS_SUCCESS;
}

uint8_t wlan_fils_is_enable(struct wlan_objmgr_vdev *vdev)
{
	struct fd_vdev *fv;

	if (vdev == NULL) {
		fd_err("VDEV is NULL!!\n");
		return 0;
	}
	fv = wlan_objmgr_vdev_get_comp_private_obj(vdev, WLAN_UMAC_COMP_FD);
	if (fv == NULL) {
		fd_debug("Invalid FILS DISC object!\n");
		return 0;
	}

	return fv->fils_enable;
}

void wlan_fd_update_trigger(struct wlan_objmgr_vdev *vdev)
{
	struct fd_vdev *fv;

	if (vdev == NULL) {
		fd_err("VDEV is NULL!!\n");
		return;
	}

	fv = wlan_objmgr_vdev_get_comp_private_obj(vdev, WLAN_UMAC_COMP_FD);
	if (fv == NULL) {
		fd_debug("Invalid FILS DISC object!\n");
		return;
	}

	fv->fd_update = 1;
}
