/*
 * Copyright (c) 2017-2019 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */
/*
 * Copyright (c) 2011, Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * UMAC beacon specific offload interface functions - for power and performance offload model
 */
#include "ol_if_athvar.h"
#include "qdf_mem.h"
#include "ol_cfg.h"
#if UNIFIED_SMARTANTENNA
#include <target_if_sa_api.h>
#endif
#include <wlan_objmgr_peer_obj.h>
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_mgmt_txrx_utils_api.h>
#include <wlan_osif_priv.h>
#include <init_deinit_lmac.h>
#ifdef QCA_SUPPORT_CP_STATS
#include <wlan_cp_stats_ic_utils_api.h>
#endif
#include <wlan_vdev_mgr_ucfg_api.h>
#include <wlan_vdev_mgr_utils_api.h>
#include <wlan_utility.h>
#if ATH_PERF_PWR_OFFLOAD
/*
 *  WMI API for sending beacons
 */
#define BCN_SEND_BY_REF
void
ol_ath_beacon_send(struct ol_ath_softc_net80211 *scn,
                   int vid,
                   wbuf_t wbuf)
{
    struct beacon_params param;
    struct wlan_objmgr_psoc *psoc = NULL;

    QDF_STATUS status = wlan_objmgr_pdev_try_get_ref(scn->sc_pdev, WLAN_SA_API_ID);
    struct common_wmi_handle *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    if (QDF_IS_STATUS_ERROR(status)) {
        QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_ERROR,
                  "%s, %d unable to get reference\n", __func__, __LINE__);
        return;
    }

    psoc = wlan_pdev_get_psoc(scn->sc_pdev);


    qdf_mem_set(&param, sizeof(param), 0);
    param.wbuf = wbuf;
    param.vdev_id = vid;
    if (ol_cfg_is_high_latency(NULL)) {
        param.is_high_latency = TRUE;

#ifdef DEBUG_BEACON
        printk("%s frm length %d \n",__func__,bcn_len);
#endif
        wmi_unified_beacon_send_cmd(pdev_wmi_handle, &param);
    } else {
        A_UINT16  frame_ctrl;
        struct ieee80211_frame *wh;
        struct ieee80211_node *ni;
        struct ieee80211vap *vap;
        struct ol_ath_vap_net80211 *avn;
        struct ieee80211_beacon_offsets *bo;
        struct ieee80211_ath_tim_ie *tim_ie;
        A_UINT32 bcn_txant = 0;
        ni = wlan_wbuf_get_peer_node(wbuf);

        if (!ni) {
            wlan_objmgr_pdev_release_ref(scn->sc_pdev, WLAN_SA_API_ID);
            return;
        }
        vap = ni->ni_vap;
        avn = OL_ATH_VAP_NET80211(vap);
        bo = &avn->av_beacon_offsets;
        tim_ie = (struct ieee80211_ath_tim_ie *)
            bo->bo_tim;
        /* Get the frame ctrl field */
        wh = (struct ieee80211_frame *)wbuf_header(wbuf);
        frame_ctrl = __le16_to_cpu(*((A_UINT16 *)wh->i_fc));

        /* get the DTIM count */

        if (tim_ie->tim_count == 0) {
            param.is_dtim_count_zero = TRUE;
            if (tim_ie->tim_bitctl & 0x01) {
                /* deliver CAB traffic in next DTIM beacon */
                param.is_bitctl_reqd = TRUE;
            }
        }
        /* Map the beacon buffer to DMA region */
        qdf_nbuf_map_single(scn->soc->qdf_dev, wbuf, QDF_DMA_TO_DEVICE);

#if UNIFIED_SMARTANTENNA
        bcn_txant = target_if_sa_api_get_beacon_txantenna(psoc, scn->sc_pdev);
#endif

        param.frame_ctrl = frame_ctrl;
        param.bcn_txant = bcn_txant;
        if(!avn->av_restart_in_progress) {
            wmi_unified_beacon_send_cmd(pdev_wmi_handle, &param);
        }
    }
    wlan_objmgr_pdev_release_ref(scn->sc_pdev, WLAN_SA_API_ID);
    return;
}

/*
 * API to be invoked from the southbound infra for beacon-Tx.
 * It invokes the legacy beacon Tx API.
 */
QDF_STATUS
ol_ath_mgmt_beacon_send(struct wlan_objmgr_vdev *vdev,
                        wbuf_t wbuf)
{
    struct wlan_objmgr_pdev *pdev;
    struct ol_ath_softc_net80211 *scn;
    struct pdev_osif_priv *osif_priv;

    if (vdev) {
        pdev = wlan_vdev_get_pdev(vdev);
        if (pdev) {
            osif_priv = wlan_pdev_get_ospriv(pdev);
            scn = (struct ol_ath_softc_net80211 *)
                    osif_priv->legacy_osif_priv;
            ol_ath_beacon_send(scn, wlan_vdev_get_id(vdev), wbuf);
            return QDF_STATUS_SUCCESS;
        }
    }
    return QDF_STATUS_E_FAILURE;
}

/*
 * Function to check if beacon offload is enabled
 */
bool ol_ath_is_beacon_offload_enabled(ol_ath_soc_softc_t *soc)
{
    struct common_wmi_handle *wmi_handle;

    wmi_handle = lmac_get_wmi_hdl(soc->psoc_obj);
    if (wlan_psoc_nif_feat_cap_get(soc->psoc_obj, WLAN_SOC_F_BCN_OFFLOAD) &&
        wmi_service_enabled(wmi_handle, wmi_service_beacon_offload)) {
        return true;
    }
    return false;
}

/*
 * Function to update beacon probe template
 */
static void
ol_ath_vdev_beacon_template_update(struct ieee80211vap *vap)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    struct ol_ath_vap_net80211 *avn = OL_ATH_VAP_NET80211(vap);
    int error = 0;
    struct ieee80211vap *transmit_vap = NULL;

    if (wlan_pdev_nif_feat_cap_get(ic->ic_pdev_obj, WLAN_PDEV_F_MBSS_IE_ENABLE)) {
        transmit_vap = ic->ic_mbss.transmit_vap;
        if (!transmit_vap) {
            qdf_warn("[MBSSID] Transmitting VAP can't be NULL!");
            return;
        }
        /* In MBSS case, avn points to transmitting VAP */
        avn = OL_ATH_VAP_NET80211(transmit_vap);
    }

    qdf_spin_lock_bh(&avn->avn_lock);

    if (avn->av_wbuf == NULL) {
       qdf_spin_unlock_bh(&avn->avn_lock);
       qdf_debug("beacon buffer av_wbuf is NULL - Ignore Template update");
       return;
    }

    if (vap->channel_switch_state) {
        qdf_spin_unlock_bh(&avn->avn_lock);
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_DOTH,
            "%s Channel switch is ON - Ignore Template update.\n", __func__);
        return;
    }

#if UMAC_SUPPORT_WNM
    if(vap->iv_bss) {
        error = ieee80211_beacon_update(vap->iv_bss, &avn->av_beacon_offsets,
                avn->av_wbuf, 0,0);
    }
#else
    if(vap->iv_bss) {
        error = ieee80211_beacon_update(vap->iv_bss, &avn->av_beacon_offsets,
                avn->av_wbuf, 0);
    }
#endif

    if (error != -1) {
        if (wlan_pdev_nif_feat_cap_get(ic->ic_pdev_obj, WLAN_PDEV_F_MBSS_IE_ENABLE)) {
	    if (!transmit_vap) {
                qdf_spin_unlock_bh(&avn->avn_lock);
                return;
	    }
            ol_ath_bcn_tmpl_send(scn, avn->av_if_id, transmit_vap);
        } else {
            ol_ath_bcn_tmpl_send(scn, avn->av_if_id, vap);
	}
    } else {
        QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_DEBUG,
                  "%s: Failed to update beacon\n", __func__);
    }
    qdf_spin_unlock_bh(&avn->avn_lock);
    return;
}

#if UMAC_SUPPORT_QUIET
/*
 *  WMI API for sending cmd to set/unset Quiet Mode
 */
static void
ol_ath_set_quiet_mode(struct ieee80211com *ic,uint8_t enable)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    struct ieee80211_quiet_param *quiet_ic = ic->ic_quiet;
    struct set_quiet_mode_params param;
    struct common_wmi_handle *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    qdf_mem_set(&param, sizeof(param), 0);
    param.enabled = enable;
    param.intval = ic->ic_intval;
    param.period = quiet_ic->period*ic->ic_intval;
    param.duration = quiet_ic->duration;
    param.offset = quiet_ic->offset;
    wmi_unified_set_quiet_mode_cmd_send(pdev_wmi_handle, &param);
}
static void
ol_ath_set_quiet_offload_config_params(wlan_if_t vap, struct ieee80211com *ic)
{
    struct ol_ath_vap_net80211 *avn = OL_ATH_VAP_NET80211(vap);
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    struct ieee80211_quiet_param *quiet_ic = ic->ic_quiet;

    struct set_bcn_offload_quiet_mode_params param;
    struct common_wmi_handle *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    if (!pdev_wmi_handle) {
        qdf_err("%s: pdev_wmi_handle is NULL");
        return;
    }
    qdf_mem_set(&param, sizeof(param), 0);
    param.vdev_id = avn->av_if_id;
    param.period = quiet_ic->period;
    param.duration = quiet_ic->duration;
    param.next_start = quiet_ic->offset;
    param.flag = vap->iv_quiet->is_enabled;
    wmi_unified_set_bcn_offload_quiet_mode_cmd_send(pdev_wmi_handle, &param);
}
#endif

int
ol_ath_set_beacon_filter(wlan_if_t vap, u_int32_t *ie)
{
    /* Issue WMI command to set beacon filter */
    struct ol_ath_vap_net80211 *avn = OL_ATH_VAP_NET80211(vap);
    struct ol_ath_softc_net80211 *scn = avn->av_sc;
    struct set_beacon_filter_params param;
    struct common_wmi_handle *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    qdf_mem_set(&param, sizeof(param), 0);
    param.vdev_id = avn->av_if_id;
    param.ie = ie;

    return wmi_unified_set_beacon_filter_cmd_send(pdev_wmi_handle, &param);
}

int
ol_ath_remove_beacon_filter(wlan_if_t vap)
{
    struct ol_ath_vap_net80211 *avn = OL_ATH_VAP_NET80211(vap);
    struct ol_ath_softc_net80211 *scn = avn->av_sc;
    struct remove_beacon_filter_params param;
    struct common_wmi_handle *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    qdf_mem_set(&param, sizeof(param), 0);
    param.vdev_id = avn->av_if_id;
    return wmi_unified_remove_beacon_filter_cmd_send(pdev_wmi_handle, &param);
}

void
ol_ath_set_probe_filter(void *data)
{
    /* TODO: Issue WMI command to set probe response filter */
    return;
}

static void
ol_ath_beacon_update(struct ieee80211_node *ni, int rssi)
{
    /* Stub for peregrine */
    return;
}

static int
ol_ath_net80211_is_hwbeaconproc_active(struct ieee80211com *ic)
{
    /* Stub for peregrine */
    return 0;
}

static void
ol_ath_net80211_hw_beacon_rssi_threshold_enable(struct ieee80211com *ic,
                                            u_int32_t rssi_threshold)
{
    /* TODO: Issue WMI command to set beacon RSSI filter */
    return;

}

static void
ol_ath_net80211_hw_beacon_rssi_threshold_disable(struct ieee80211com *ic)
{
    /* TODO: Issue WMI command to disable beacon RSSI filter */
    return;
}

struct ol_ath_iter_update_beacon_arg {
    struct ieee80211com *ic;
    int if_id;
};

/* Move the beacon buffer to deferred_bcn_list */
static void
ol_ath_vap_defer_beacon_buf_free(struct ol_ath_vap_net80211 *avn)
{
    struct bcn_buf_entry* buf_entry;

    /* Set up buffer entry for Tx MBSS VAP only, so we return here
     * for a non-transmitting VAP
     */
    if (IEEE80211_VAP_IS_MBSS_NON_TRANSMIT_ENABLED(&(avn->av_vap))) {
        return;
    }

    buf_entry = (struct bcn_buf_entry *)qdf_mem_malloc(
	         sizeof(struct bcn_buf_entry));
    if (buf_entry) {
        qdf_spin_lock_bh(&avn->avn_lock);
	if(avn->av_wbuf == NULL){
		qdf_spin_unlock_bh(&avn->avn_lock);
		qdf_mem_free(buf_entry);
		return;
	}
#ifdef BCN_SEND_BY_REF
        buf_entry->is_dma_mapped = avn->is_dma_mapped;
        /* clear dma_mapped flag */
        avn->is_dma_mapped = 0;
#endif
        buf_entry->bcn_buf = avn->av_wbuf;
        TAILQ_INSERT_TAIL(&avn->deferred_bcn_list, buf_entry, deferred_bcn_list_elem);
        avn->av_wbuf =  NULL;
        qdf_spin_unlock_bh(&avn->avn_lock);
    }
    else {
        printk("ERROR: qdf_mem_malloc failed %s: %d \n", __func__, __LINE__);
        ASSERT(0);
    }
}

/*
 * Function to allocate beacon in host mode
 */
static void
ol_ath_vap_iter_beacon_alloc(void *arg, wlan_if_t vap)
{
    struct ol_ath_iter_update_beacon_arg* params = (struct ol_ath_iter_update_beacon_arg *)arg;
    struct ol_ath_vap_net80211 *avn = OL_ATH_VAP_NET80211(vap);
    struct ieee80211_node *ni;

    if (avn->av_if_id == params->if_id) {
            ni = vap->iv_bss;
	    /* Beacon buffer is already allocated,
	     * Move the beacon buffer to deferred_bcn_list to
	     * free the buffer on vap stop
	     * and allocate a new beacon buufer
	     */
	     ol_ath_vap_defer_beacon_buf_free(avn);
	     avn->av_wbuf = ieee80211_beacon_alloc(ni, &avn->av_beacon_offsets);
	     if (avn->av_wbuf == NULL) {
                   printk("ERROR ieee80211_beacon_alloc failed in %s:%d\n", __func__, __LINE__);
             A_ASSERT(0);
            }
    }
    return;
}

/*
 * Function to free beacon in host mode
 */
static void
ol_ath_vap_iter_beacon_free(void *arg, wlan_if_t vap)
{
    struct ol_ath_iter_update_beacon_arg* params = (struct ol_ath_iter_update_beacon_arg *)arg;
    struct ol_ath_vap_net80211 *avn = OL_ATH_VAP_NET80211(vap);
#ifdef BCN_SEND_BY_REF
    struct ieee80211com *ic = vap->iv_ic;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
#endif

    if (avn->av_if_id == params->if_id) {
            struct bcn_buf_entry* buf_entry,*buf_temp;
            struct ieee80211_tx_status ts;
            ts.ts_flags = 0;
            ts.ts_retries = 0;
            qdf_spin_lock_bh(&avn->avn_lock);
            TAILQ_FOREACH_SAFE(buf_entry, &avn->deferred_bcn_list, deferred_bcn_list_elem,buf_temp) {
                TAILQ_REMOVE(&avn->deferred_bcn_list, buf_entry, deferred_bcn_list_elem);
#ifdef BCN_SEND_BY_REF
                if (buf_entry->is_dma_mapped == 1) {
                    qdf_nbuf_unmap_single(scn->soc->qdf_dev,
                              buf_entry->bcn_buf,
                              QDF_DMA_TO_DEVICE);
                    buf_entry->is_dma_mapped = 0;
                }
#endif
                ieee80211_complete_wbuf(buf_entry->bcn_buf, &ts);
                qdf_mem_free(buf_entry);
            }
            qdf_spin_unlock_bh(&avn->avn_lock);
    }
    return;
}

/*
 * offload beacon APIs for other offload modules
 */
void
ol_ath_beacon_alloc(struct ieee80211com *ic, int if_id)
{
    struct ol_ath_iter_update_beacon_arg params;

    params.ic = ic;
    params.if_id = if_id;
    wlan_iterate_vap_list(ic,ol_ath_vap_iter_beacon_alloc,(void *) &params);
}

void
ol_ath_beacon_stop(struct ol_ath_softc_net80211 *scn,
                   struct ol_ath_vap_net80211 *avn)
{
    struct ieee80211_tx_status ts;

    if (IEEE80211_VAP_IS_MBSS_NON_TRANSMIT_ENABLED(&(avn->av_vap))) {
        /*  Beacon buffer is set up for tx VAP only */
        return;
    }

    ts.ts_flags = 0;
    ts.ts_retries = 0;
    /* Move the beacon buffer to deferred_bcn_list
     * and wait for stooped event from Target.
     * beacon buffer in deferred_bcn_list gets freed - on
     * vap stopped event from Target
     */
    ol_ath_vap_defer_beacon_buf_free(avn);

    return;
}


void
ol_ath_beacon_free(struct ieee80211com *ic, int if_id)
{
    struct ol_ath_iter_update_beacon_arg params;

    params.ic = ic;
    params.if_id = if_id;
    wlan_iterate_vap_list(ic,ol_ath_vap_iter_beacon_free,(void *) &params);
}

#if UMAC_SUPPORT_QUIET
/*
 * Function to update quiet element in the beacon and the VAP quite params
 */
static void
ol_ath_update_quiet_params(struct ieee80211com *ic, struct ieee80211vap *vap)
{

    struct ieee80211_quiet_param *quiet_iv = vap->iv_quiet;
    struct ieee80211_quiet_param *quiet_ic = ic->ic_quiet;
    struct ieee80211vap *quiet_vap = TAILQ_FIRST(&ic->ic_vaps);

    /* Update quiet params for the vap with beacon offset 0 */
    if (quiet_ic->is_enabled) {

        u_int64_t tsf_adj;
        ucfg_wlan_vdev_mgr_get_tsf_adjust(vap->vdev_obj,
                &tsf_adj);
        /* convert tsf adjust in to TU */
        tsf_adj = tsf_adj >> 10;

        /* Compute the beacon_offset from slot 0 */
        if (tsf_adj) {
           quiet_iv->beacon_offset = ic->ic_intval - tsf_adj;
        }
        else {
            quiet_iv->beacon_offset = 0;
        }

        while(quiet_vap != NULL) {
            if(!quiet_vap->iv_vap_is_down)
                    break;
            quiet_vap = TAILQ_NEXT(quiet_vap, iv_next);
        }

		if (quiet_vap && (vap->iv_unit == quiet_vap->iv_unit)) {
			quiet_ic->beacon_offset = quiet_iv->beacon_offset;

			if (quiet_ic->tbttcount == 1) {
				quiet_ic->tbttcount = quiet_ic->period;
			}
			else {
				quiet_ic->tbttcount--;
			}

			if (quiet_ic->tbttcount == 1) {
				ol_ath_set_quiet_mode(ic,1);
			}
			else if (quiet_ic->tbttcount == (quiet_ic->period-1)) {
				ol_ath_set_quiet_mode(ic,0);
			}

		}
	} else if (quiet_ic->tbttcount != quiet_ic->period) {
  		/* quiet support is disabled
         * since tbttcount is not '0', the hw quiet period was set before
         * so just disable the hw quiet period and
         * tbttcount to 0.
         */
		quiet_ic->tbttcount = quiet_ic->period;
		ol_ath_set_quiet_mode(ic,0);
	}
}
#endif /* UMAC_SUPPORT_QUIET */

/* returns the pointer to beacon buffer for the vdev
 * used by pktlog message handler to retrieve the beacon
 * header
 */

void *
ol_ath_get_bcn_header(ol_pdev_handle pdev, A_UINT32 vdev_id)
{
    struct ieee80211vap *vap;
    struct ol_ath_softc_net80211 *scn;
    struct ol_ath_vap_net80211 *avn;

    scn = (struct ol_ath_softc_net80211 *) lmac_get_pdev_feature_ptr((struct wlan_objmgr_pdev *)pdev);
    if (!scn)
	return NULL;

    vap = ol_ath_vap_get(scn, vdev_id);
    if (!vap)
        return NULL;
    avn = OL_ATH_VAP_NET80211(vap);

    if ((avn == NULL) || (avn->av_wbuf == NULL))
    {
        printk("%s: Invalid buffer pointer\n",__func__);
        ol_ath_release_vap(vap);
        return NULL;
    }
    ol_ath_release_vap(vap);
    return (wbuf_header(avn->av_wbuf));
}
qdf_export_symbol(ol_ath_get_bcn_header);

/* returns the pointer to beacon buffer for the vdev
 * used by pktlog message handler
 */

void *
ol_ath_get_bcn_buffer(ol_pdev_handle pdev, A_UINT32 vdev_id)
{
    struct ieee80211vap *vap;
    struct ol_ath_softc_net80211 *scn;
    struct ol_ath_vap_net80211 *avn;

    scn = (struct ol_ath_softc_net80211 *) lmac_get_pdev_feature_ptr((struct wlan_objmgr_pdev *)pdev);
    if (!scn)
	return NULL;

    vap = ol_ath_vap_get(scn, vdev_id);
    if (!vap)
        return NULL;
    avn = OL_ATH_VAP_NET80211(vap);

    if ((avn == NULL) || (avn->av_wbuf == NULL))
    {
        ASSERT(avn != NULL);
        ASSERT(avn->av_wbuf != NULL);
        qdf_print("%s(): beacon buffer vacant",__func__);
        ol_ath_release_vap(vap);
        return NULL;
    }
    ol_ath_release_vap(vap);

    return (avn->av_wbuf);
}
qdf_export_symbol(ol_ath_get_bcn_buffer);

void ieee80211_wnm_timbcast_send(struct ieee80211vap *vap)
{
    struct ieee80211_node *ni = vap->iv_bss;
    struct ieee80211com *ic = vap->iv_ic;
    wbuf_t wbuf_tim_hr = NULL;
    wbuf_t wbuf_tim_lr = NULL;
    struct ol_ath_vap_net80211 *avn;
    int error = 0;

    avn = OL_ATH_VAP_NET80211(vap);
    if (ieee80211_wnm_timbcast_enabled(vap) > 0) {
        if (ieee80211_wnm_timbcast_cansend(vap) > 0) {
            if (ni && ieee80211_timbcast_highrateenable(vap)) {
                ieee80211_ref_node(ni);
                wbuf_tim_hr = ieee80211_timbcast_alloc(ni);
                if (wbuf_tim_hr) {
                    error = ieee80211_timbcast_update(vap->iv_bss,
                                         &avn->av_beacon_offsets, wbuf_tim_hr);
                    if (error) {
                        wbuf_free(wbuf_tim_hr);
                        wbuf_tim_hr = NULL;
                    } else {
                        wbuf_set_tid(wbuf_tim_hr, EXT_TID_NONPAUSE);
                        wbuf_set_tx_rate(wbuf_tim_hr, 0, 0, 1, ic->ic_he_target); //24Mbps
                        ieee80211_send_mgmt(vap, ni, wbuf_tim_hr, true);
                    }
                }
                ieee80211_free_node(ni);
            }
            if (ni && ieee80211_timbcast_lowrateenable(vap)) {
                ieee80211_ref_node(ni);
                wbuf_tim_lr = ieee80211_timbcast_alloc(ni);
                if (wbuf_tim_lr) {
                    error = ieee80211_timbcast_update(vap->iv_bss,
                                          &avn->av_beacon_offsets, wbuf_tim_lr);
                    if (error) {
                        wbuf_free(wbuf_tim_lr);
                        wbuf_tim_lr = NULL;
                    } else {
                        ieee80211_send_mgmt(vap, ni, wbuf_tim_lr, true);
                    }
                }
                ieee80211_free_node(ni);
            }
        }
    }
}

/* prep and send beacon for a vap */
static inline int
ol_prepare_send_vap_bcn(struct ieee80211vap *vap,
                        struct ol_ath_softc_net80211 *scn,
                        u_int32_t if_id,
                        wmi_host_tim_info *tim_info)
{
    struct ol_ath_vap_net80211 *avn;
#if UMAC_SUPPORT_QUIET
    struct ieee80211com *ic = vap->iv_ic;
#endif
    struct ieee80211_node *ni = vap->iv_bss;
    int error = 0, retval = -1;

        /*
         * Update the TIM bitmap. At VAP attach memory will be allocated for TIM
         * based on the iv_max_aid set. Update this field and beacon update will
         * automatically take care of populating the bitmap in the beacon buffer.
         */
        if (vap->iv_tim_bitmap && tim_info->tim_changed) {

            /* The tim bitmap is a byte array that is passed through WMI as a
             * 32bit word array. The CE will correct for endianess which is
             * _not_ what we want here. Un-swap the words so that the byte
             * array is in the correct order.
             */
#ifdef BIG_ENDIAN_HOST
            int j;
            for (j = 0; j < WMI_HOST_TIM_BITMAP_ARRAY_SIZE; j++) {
                tim_info->tim_bitmap[j] = le32_to_cpu(tim_info->tim_bitmap[j]);
            }
#endif

            if(tim_info->tim_len <= MAX_TIM_BITMAP_LENGTH) {
                vap->iv_tim_len = (u_int16_t)tim_info->tim_len;
                OS_MEMCPY(vap->iv_tim_bitmap, tim_info->tim_bitmap, vap->iv_tim_len);
                vap->iv_ps_pending = tim_info->tim_num_ps_pending;

                IEEE80211_VAP_TIMUPDATE_ENABLE(vap);
            } else {
                printk("\n %s : Ignoring TIM Update for the VAP (%d)  because of INVALID Tim length from the firmware \n", __func__, if_id);
            }

        }

        /* Update quiet params and beacon update will take care of the rest */
        avn = OL_ATH_VAP_NET80211(vap);
        qdf_spin_lock_bh(&avn->avn_lock);

        if (avn->av_wbuf == NULL) {
            qdf_debug("beacon buffer av_wbuf is NULL - Ignoring SWBA event \n");
	    qdf_spin_unlock_bh(&avn->avn_lock);
            return retval;
        }

#if UMAC_SUPPORT_QUIET
        ol_ath_update_quiet_params(ic, vap);
#endif /* UMAC_SUPPORT_QUIET */
#if UMAC_SUPPORT_WNM
	if(vap->iv_bss) {
            error = ieee80211_beacon_update(vap->iv_bss, &avn->av_beacon_offsets,
                                        avn->av_wbuf, tim_info->tim_mcast,0);
	}
#else
	if(vap->iv_bss) {
            error = ieee80211_beacon_update(vap->iv_bss, &avn->av_beacon_offsets,
                                        avn->av_wbuf, tim_info->tim_mcast);
	}
#endif
        if (error != -1) {

#if UMAC_SUPPORT_WNM
            ieee80211_wnm_timbcast_send(vap);
#endif

            if (!vap->iv_bcn_offload_enable && ni) {
            /* Send beacon to target */
#ifdef BCN_SEND_BY_REF
                if (avn->is_dma_mapped) {

                    qdf_nbuf_unmap_single(scn->soc->qdf_dev,
                                  avn->av_wbuf,
                                  QDF_DMA_TO_DEVICE);
                    avn->is_dma_mapped = 0;
                }
#endif
                retval = wlan_mgmt_txrx_beacon_frame_tx(ni->peer_obj,
                                                    avn->av_wbuf,
                                                    WLAN_UMAC_COMP_MLME);
                if (!retval) {
#ifdef QCA_SUPPORT_CP_STATS
                    vdev_cp_stats_tx_bcn_swba_inc(vap->vdev_obj, 1);
#endif

#ifdef BCN_SEND_BY_REF
                    avn->is_dma_mapped = 1;
#endif
                } else {
                    /*
                     * The caller expects -1 in return for failure.
                     * As the failure returned from mgmt_txrx
                     * is greater than 0, retval should be assigned
                     * appropriately.
                     */
                    retval = -1;
                }
            }
        }
        qdf_spin_unlock_bh(&avn->avn_lock);
        return retval;
}

/*
 * Handler for Host SWBA events in host mode
 */
static int
ol_ath_beacon_swba_handler(ol_ath_soc_softc_t *soc,
                           uint8_t *data)
{
    struct ol_ath_softc_net80211 *scn;
    struct ol_ath_vap_net80211 *avn;
    struct ieee80211vap *vap = NULL;
    wmi_host_tim_info tim_info = {0};
#if UMAC_SUPPORT_QUIET
    wmi_host_quiet_info quiet_info = {0};
#endif
    u_int8_t vdev_id = 0, idx = 0;
    u_int32_t num_vdevs = 0;
    struct wlan_objmgr_vdev *vdev;
    bool disable_beacon = false;
    struct common_wmi_handle *wmi_handle;

    wmi_handle = lmac_get_wmi_hdl(soc->psoc_obj);
    if(wmi_extract_swba_num_vdevs(wmi_handle, data, &num_vdevs)) {
        QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_ERROR,
                  "%s: Unable to extact num vdevs from swba event\n", __func__);
        return -1;
    }


    /* Generate LP IOT vap beacons first and then send other vap beacons later
    */
    if (soc->soc_lp_iot_vaps_mask) { /* if there LP IOT vaps */
        for (idx = 0; idx < num_vdevs; idx++) {

            if (wmi_extract_swba_tim_info(wmi_handle, data, idx, &tim_info)) {
                return -1;
            }

            /* Proceed only if beacon_offload is disabled */
            if (ol_ath_is_beacon_offload_enabled(soc))
                continue;

            vdev_id = tim_info.vdev_id;
            vdev = wlan_objmgr_get_vdev_by_id_from_psoc(soc->psoc_obj,
                    vdev_id, WLAN_MLME_SB_ID);
            if (vdev == NULL) {
                /*should continue if vdev is NULL*/
                QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_ERROR,
                          "%s: Unable to find vdev for %d vdev_id\n",
                          __func__, vdev_id);
                continue;
            }
            if ((vap = wlan_vdev_get_mlme_ext_obj(vdev))) {
                /* Disable beacon if VAP is operating in NAWDS bridge mode */
                if (ieee80211_nawds_disable_beacon(vap)){
                    disable_beacon = true;
                }
            }

            if ((!vap) || (disable_beacon)) {
                /*should continue if current vap is NULL*/
                wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);
                disable_beacon = false;
                continue;
            }
            avn = OL_ATH_VAP_NET80211(vap);
            scn = avn->av_sc;
            if (vap->iv_create_flags & IEEE80211_LP_IOT_VAP)   {
#if ATH_NON_BEACON_AP
                if (IEEE80211_VAP_IS_NON_BEACON_ENABLED(vap)){
                    /*for non-beaconing VAP, don't send beacon*/
                    wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);
                    continue;
                }
#endif
                /* If vap is not active, no need to queue beacons to FW, Ignore SWBA*/
                if (wlan_vdev_is_up(vdev) != QDF_STATUS_SUCCESS) {
                    qdf_print("vap %d: drop SWBA event, since vap is not active  ", vdev_id);
                    wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);
                    continue;
                }
                if (ol_prepare_send_vap_bcn(vap, scn, (u_int32_t)vdev_id, &tim_info)<0) {
                    wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);
                    return -1;
                }
           }
           wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);
        }
    }

    /* Generate a beacon for all vaps other than lp_iot vaps specified in the list */
    for (idx = 0; idx < num_vdevs; idx++) {

        if (wmi_extract_swba_tim_info(wmi_handle, data, idx, &tim_info)) {
            return -1;
        }
        /* Proceed only if beacon offload is disabled */
        if (ol_ath_is_beacon_offload_enabled(soc))
            continue;

        vdev_id = tim_info.vdev_id;
        vdev = wlan_objmgr_get_vdev_by_id_from_psoc(soc->psoc_obj,
                vdev_id, WLAN_MLME_SB_ID);
        if (vdev == NULL) {
            /*should continue if vdev is NULL*/
            QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_ERROR,
                      "%s: Unable to find vdev for %d vdev_id\n",
                      __func__, vdev_id);
            continue;
        }
        if ((vap = wlan_vdev_get_mlme_ext_obj(vdev))) {
           /* Disable beacon if VAP is operating in NAWDS bridge mode */
           if (ieee80211_nawds_disable_beacon(vap)){
               disable_beacon = true;
           }
        }

        if ((!vap) || (disable_beacon)){
            /*should continue if current vap is NULL*/
            wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);
            disable_beacon = false;
            continue;
        }
        avn = OL_ATH_VAP_NET80211(vap);
        scn = avn->av_sc;

        /* Skip LP IOT vaps. They have been sent already */
        if (!(vap->iv_create_flags & IEEE80211_LP_IOT_VAP)) {
#if ATH_NON_BEACON_AP
             if (IEEE80211_VAP_IS_NON_BEACON_ENABLED(vap)){
                 wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);
                 /*for non-beaconing VAP, don't send beacon*/
                 continue;
            }
#endif
            /* If vap is not active, no need to queue beacons to FW, Ignore SWBA*/
            if (wlan_vdev_is_up(vdev) != QDF_STATUS_SUCCESS) {
                QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_DEBUG,
                    "vap %d: drop SWBA event, since vap is not active",
                    vdev_id);
                wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);
                continue;
            }
            if (ol_prepare_send_vap_bcn(vap, scn, (u_int32_t)vdev_id, &tim_info)<0) {
                wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);
                return -1;
            }
        }
        wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);
    }

#if UMAC_SUPPORT_QUIET
    for (idx = 0; idx < num_vdevs; idx++) {
        if (wmi_extract_swba_quiet_info(wmi_handle, data, idx, &quiet_info)) {
            return -1;
        }
        if (quiet_info.tbttcount == 0) {
            continue;
        }

        vdev_id = quiet_info.vdev_id;
        vdev = wlan_objmgr_get_vdev_by_id_from_psoc(soc->psoc_obj,
                vdev_id, WLAN_MLME_SB_ID);
        if (vdev == NULL) {
            /*should continue if vdev is NULL*/
            QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_ERROR,
                    "%s: Unable to find vdev for %d vdev_id\n",
                    __func__, vdev_id);
            continue;
        }

        vap = wlan_vdev_get_mlme_ext_obj(vdev);
        if (!vap){
            /*should continue if vap is NULL*/
            QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_ERROR,
                      "%s: Unable to find vap for %d vdev_id\n",
                      __func__, vdev_id);
            wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);
            continue;
        }
        if (vap->iv_bcn_offload_enable) {
            /* In beacon offload case, quiet IE is offloaded to FW. So in
             * case if probe response needs it, we need to update it in vap.
             */
            vap->iv_quiet->tbttcount   = quiet_info.tbttcount;
            vap->iv_quiet->period      = quiet_info.period;
            vap->iv_quiet->duration    = quiet_info.duration;
            vap->iv_quiet->offset      = quiet_info.offset;
        }
        wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);
    }
#endif

    return 0;
}

int ol_ath_bcn_tmpl_send(struct ol_ath_softc_net80211 *scn,
				int vdev_id, struct ieee80211vap *vap)
{
	struct ol_ath_vap_net80211 *avn;
	uint32_t tmpl_len, tmpl_len_aligned;
	int ret;
	uint64_t adjusted_tsf_le;
	struct ieee80211_frame *wh;
	struct ieee80211_beacon_offsets *bo;
	struct ieee80211_ath_tim_ie *tim_ie;
	struct beacon_tmpl_params bcn_tmpl_param = {0};
	struct common_wmi_handle *pdev_wmi_handle;
	osif_dev *osifp = NULL;
	struct ieee80211com *ic = vap->iv_ic;
	u_int64_t tsf_adj;

	avn = OL_ATH_VAP_NET80211(vap);
	osifp = (osif_dev *)vap->iv_ifp;
	if (avn->av_wbuf == NULL) {
		QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_DEBUG,
				"%s: Beacon buffer is NULL. Skip sending beacon template for vdev %d\n",
				__func__, vdev_id);
		return -1;
	}
	pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
	QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_DEBUG,
			"%s: Send beacon template for vdev %d\n", __func__, vdev_id);
	qdf_mem_zero(&bcn_tmpl_param, sizeof(bcn_tmpl_param));
	tmpl_len = wbuf_get_pktlen(avn->av_wbuf);
	tmpl_len_aligned = roundup(tmpl_len, sizeof(A_UINT32));
	ucfg_wlan_vdev_mgr_get_tsf_adjust(vap->vdev_obj,
			&tsf_adj);
	/** Make the TSF offset negative so beacons in the same staggered
	 * batch have the same TSF. */
	adjusted_tsf_le = qdf_cpu_to_le64(0ULL - tsf_adj);
	/* Update the timstamp in the beacon buffer with adjusted TSF */
	wh = (struct ieee80211_frame *)wbuf_header(avn->av_wbuf);
	qdf_mem_copy(&wh[1], &adjusted_tsf_le, sizeof(adjusted_tsf_le));

	bcn_tmpl_param.vdev_id = vdev_id;

	bo = &avn->av_beacon_offsets;
	tim_ie = (struct ieee80211_ath_tim_ie *) bo->bo_tim;

	bcn_tmpl_param.tim_ie_offset = ((uint8_t*)tim_ie -
			(uint8_t*) wbuf_header(avn->av_wbuf));
	bcn_tmpl_param.tmpl_len = tmpl_len;
	bcn_tmpl_param.tmpl_len_aligned = tmpl_len_aligned;

	if (wlan_pdev_nif_feat_cap_get(ic->ic_pdev_obj,
                                       WLAN_PDEV_F_MBSS_IE_ENABLE)) {
	    bcn_tmpl_param.mbssid_ie_offset = (bo->bo_mbssid_ie -
					   wbuf_header(avn->av_wbuf));
	}

    if (vap->channel_switch_state && (vap->iv_ic->ic_num_csa_bcn_tmp_sent != 0)) {
        struct ieee80211_ath_channelswitch_ie *csa;

        csa = (struct ieee80211_ath_channelswitch_ie *) bo->bo_chanswitch;
        if (csa->ie != IEEE80211_ELEMID_CHANSWITCHANN) {
            QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_DEBUG,
                      "%s: CSA IE is not added at the offset\n", __func__);
        }
        bcn_tmpl_param.csa_switch_count_offset = (((uint8_t*)&csa->tbttcount) -
                                           (uint8_t*)wbuf_header(avn->av_wbuf));

        if (vap->iv_enable_ecsaie) {
            struct ieee80211_extendedchannelswitch_ie *ecsa;

            ecsa = (struct ieee80211_extendedchannelswitch_ie *)
                ((u_int8_t*) bo->bo_chanswitch + IEEE80211_CHANSWITCHANN_BYTES);
            if (ecsa->ie != IEEE80211_ELEMID_EXTCHANSWITCHANN) {
                QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_DEBUG,
                          "%s: ECSA IE is not added at the offset\n", __func__);
            }
            bcn_tmpl_param.ext_csa_switch_count_offset = (((uint8_t*)&(ecsa->tbttcount)) -
                    (uint8_t*)wbuf_header(avn->av_wbuf));
        }
    }

#if QCN_ESP_IE
    if (ic->ic_esp_periodicity) {
        struct ieee80211_est_ie *est_ie;

        est_ie = (struct ieee80211_est_ie *) bo->bo_esp_ie;
        if ((est_ie->element_id != IEEE80211_ELEMID_EXTN) ||
            (est_ie->element_id_extension != IEEE80211_ELEMID_EXT_ESP_ELEMID_EXTENSION)) {
            QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_DEBUG,
                      "%s: ESP IE is not added at the offset\n", __func__);
        }
        bcn_tmpl_param.esp_ie_offset = ((uint8_t*)est_ie -
                        (uint8_t*) wbuf_header(avn->av_wbuf));
    }
#endif /* QCN_ESP_IE */

	bcn_tmpl_param.frm = (uint8_t *)wh;

	ret = wmi_unified_beacon_tmpl_send_cmd(pdev_wmi_handle,
			&bcn_tmpl_param);
	if( ret < 0 ){
                QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_ERROR,
		          "%s: Failed to send beacon template\n", __func__);
	}
	return ret;
}

int ol_ath_offload_bcn_tx_status_event_handler(ol_scn_t sc,
        uint8_t *data, uint32_t datalen) {
    ol_ath_soc_softc_t *soc           = (ol_ath_soc_softc_t *) sc;
    struct ieee80211vap *vap          = NULL;
    struct ieee80211com *ic           = NULL;
    struct ieee80211vap *tempvap;
    struct wlan_objmgr_vdev *vdev;
    struct common_wmi_handle *wmi_handle;
    uint32_t vdev_id, tx_status;
    int temp_vdev_id;

    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
                                                "%s>>", __func__);

    wmi_handle = lmac_get_wmi_hdl(soc->psoc_obj);
    if ((wmi_handle == NULL)
            || wmi_extract_offload_bcn_tx_status_evt(wmi_handle,
                data, &vdev_id, &tx_status) != QDF_STATUS_SUCCESS) {
        QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_ERROR,
            "%s: Extracting offload bcn tx status event failed\n", __func__);

        QDF_TRACE(QDF_MODULE_ID_BSSCOLOR,
                QDF_TRACE_LEVEL_DEBUG, "%s<<", __func__);
        return -EINVAL;
    }

    vdev = wlan_objmgr_get_vdev_by_id_from_psoc(
            soc->psoc_obj, vdev_id, WLAN_MLME_SB_ID);
    if (vdev == NULL) {
        QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_ERROR,
                "Unable to find vdev for %d vdev_id", vdev_id);

        QDF_TRACE(QDF_MODULE_ID_BSSCOLOR,
                QDF_TRACE_LEVEL_DEBUG, "%s<<", __func__);
        return -EINVAL;
    }

    vap = wlan_vdev_get_mlme_ext_obj(vdev);
    if (vap == NULL) {
        qdf_err("vap is NULL %d vdev_id", vdev_id);
        wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);
        return -EINVAL;
    }
    ic  = vap->iv_ic;

    /* if bcca is complete for all vaps then
     * unregister itself
     */
    if (!(temp_vdev_id = ieee80211_is_bcca_ongoing_for_any_vap(ic))) {
        /* unregister offload beacon tx status event
         * handler as bss color change announcemtn has
         * completed for all vaps
         */
        ic->ic_register_beacon_tx_status_event_handler(ic, true);
        /* update beacon templates for all
         * vaps so that bcca ie can be removed
         * from any pending vap
         */
        TAILQ_FOREACH(tempvap, &ic->ic_vaps, iv_next) {
            if ((tempvap->iv_opmode == IEEE80211_M_HOSTAP)
                    && tempvap->iv_is_up) {
                wlan_vdev_beacon_update(tempvap);
            }
        }
    } else {
        QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
        "cannot unregister bcn_tx_status "
        "event as bcca ongoing. vdev-id: 0x%x",
        temp_vdev_id - 1);
        wlan_vdev_beacon_update(vap);
    }

    if (tx_status) {
        /* if we dont update beacon template on bcn
         * tx fail then we stop getting the tx status
         * event. Need to look into that in future
         */
        QDF_TRACE(QDF_MODULE_ID_BSSCOLOR,  QDF_TRACE_LEVEL_ERROR,
            "%s: beacon tx failed for vdev id: %d", __func__, vdev_id);
    }

    wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);

    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
                                            "%s<<", __func__);
    return 0;
}

/*
 * CSA switch count status event handler for Beacon Offload
 */
int ol_ath_pdev_csa_status_event_handler(ol_scn_t sc,
        uint8_t *data, uint32_t datalen)
{
    ol_ath_soc_softc_t *soc = (ol_ath_soc_softc_t *) sc;
    struct pdev_csa_switch_count_status csa_status;
    struct ieee80211com *ic = NULL;
    struct ol_ath_softc_net80211 *scn = NULL;
    struct ieee80211vap *vap = NULL;
    int i = 0;
    struct ol_ath_vap_net80211 *avn;
    int error = 0;
    bool vap_restart = true;
    struct common_wmi_handle *wmi_handle;
    struct wlan_objmgr_pdev *pdev;

    wmi_handle = lmac_get_wmi_hdl(soc->psoc_obj);
    if (wmi_extract_pdev_csa_switch_count_status(wmi_handle,
                data, &csa_status) != QDF_STATUS_SUCCESS) {
        QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_ERROR,
                "%s: Extracting CSA switch count status event failed\n", __func__);
        return -1;
    }
    pdev = wlan_objmgr_get_pdev_by_id(soc->psoc_obj, PDEV_UNIT(csa_status.pdev_id), WLAN_MLME_SB_ID);
    if (pdev == NULL) {
         qdf_print("%s: pdev object (id: %d) is NULL ", __func__, PDEV_UNIT(csa_status.pdev_id));
         return -1;
    }

    scn = lmac_get_pdev_feature_ptr(pdev);
    if (scn == NULL) {
         qdf_print("%s: scn(id: %d) is NULL ", __func__, PDEV_UNIT(csa_status.pdev_id));
         wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
         return -1;
    }

    ic = &scn->sc_ic;

    /* Update beacon template for all VAPs with beacon offload enabled
     * everytime the CSA switch count is updated by FW.
     */
    for (i = 0; i < csa_status.num_vdevs; i++) {

        /* Get the VAP corresponding to the vdev id received */
        vap = ol_ath_vap_get(scn, csa_status.vdev_ids[i]);
        if (vap == NULL)
            continue;

        if (csa_status.current_switch_count == 0) {
            if (ic->ic_num_csa_bcn_tmp_sent == 0) {
                ol_ath_release_vap(vap);
                QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_ERROR,
                        "%s: Host should not have received this event"
                        " when ic->ic_num_csa_bcn_tmp_sent=0\n", __func__);
                wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
                return -1;
            }

            /* Set channel change count for VAP to number of CSA beacon counts
             * configured for the radio and update beacon
             */
            vap->iv_chanchange_count = ic->ic_chanchange_tbtt;
            ic->ic_num_csa_bcn_tmp_sent--;
            ic->ic_csa_vdev_ids[ic->ic_csa_num_vdevs] = csa_status.vdev_ids[i];
            if (ic->ic_csa_num_vdevs < WLAN_UMAC_PDEV_MAX_VDEVS)
                ic->ic_csa_num_vdevs++;

            /* Send multiple vdev restart command to FW if AP switches to new
             * channel. Removing the CSA-IE from the beacon is taken care in
             * ol_ath_vap_up() before sending vdev_up command to FW.
             */
            wlan_vdev_mlme_sm_deliver_evt(vap->vdev_obj, WLAN_VDEV_SM_EV_CSA_COMPLETE, 0, NULL);
        }
        ol_ath_release_vap(vap);
    }

    /* Receive CSA completion from all the vaps. */
    if (ic->ic_num_csa_bcn_tmp_sent == 0) {
        struct ieee80211vap *tmp_vap = NULL;
            for (i = 0; i < ic->ic_csa_num_vdevs; i++) {
            tmp_vap = ol_ath_vap_get(scn, ic->ic_csa_vdev_ids[i]);
            if (!tmp_vap)
                return -1;
            avn = OL_ATH_VAP_NET80211(tmp_vap);

            if (wlan_vdev_chan_config_valid(tmp_vap->vdev_obj) !=
                         QDF_STATUS_SUCCESS) {
                QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_ERROR,
                        "%s: Vap is not in run state, vdev_id = %d\n", __func__, ic->ic_csa_vdev_ids[i]);
                ol_ath_release_vap(tmp_vap);
                continue;
            }

#if UMAC_SUPPORT_WNM
            if(tmp_vap->iv_bss)
                error = ieee80211_beacon_update(tmp_vap->iv_bss,
                        &avn->av_beacon_offsets,
                        avn->av_wbuf, 0, 0);
#else
            if(tmp_vap->iv_bss)
                error = ieee80211_beacon_update(tmp_vap->iv_bss,
                        &avn->av_beacon_offsets,
                        avn->av_wbuf, 0);
#endif

            if (error != -1) {
                if (tmp_vap->iv_remove_csa_ie) {
                    /* When AP detects the RADAR or user changes the channel
                     * using doth_chanswitch command, AP sends the CSA to its
                     * BSS. When Host receives CSA event handler, it removes the
                     * CSA IE from beacon in ieee80211_beacon_update() and sends
                     * beacon template to Firmware.
                     */
                    qdf_spin_lock_bh(&avn->avn_lock);
                    ol_ath_bcn_tmpl_send(scn, avn->av_if_id, tmp_vap);
                    qdf_spin_unlock_bh(&avn->avn_lock);
                    tmp_vap->iv_remove_csa_ie = false;
                }

                if (tmp_vap->iv_no_restart) {
                    /* When usenol is set to 0 and if AP detects the RADAR, it
                     * sends CSA to its BSS, but it does not change the channel.
                     * In this case, host only needs to remove the CSA IE and
                     * send beacon template to FW. Since AP is not changing the
                     * channel, vap restart is not required.
                     */
                    tmp_vap->channel_switch_state = 0;
                    vap_restart = false;
                    tmp_vap->iv_no_restart = false;
                }
            }

            ol_ath_release_vap(tmp_vap);
        }

        /* Send multiple vdev restart command to FW if AP switches to new
         * channel. Removing the CSA-IE from the beacon is taken care in
         * ol_ath_vap_up() before sending vdev_up command to FW.
         */
        TAILQ_FOREACH(tmp_vap, &ic->ic_vaps, iv_next) {
            if (tmp_vap->iv_opmode == IEEE80211_M_MONITOR) {
                ic->ic_csa_vdev_ids[ic->ic_csa_num_vdevs] = wlan_vdev_get_id(tmp_vap->vdev_obj);
                if (ic->ic_csa_num_vdevs < WLAN_UMAC_PDEV_MAX_VDEVS) {
                    ic->ic_csa_num_vdevs++;
                    wlan_vdev_mlme_sm_deliver_evt(tmp_vap->vdev_obj,
                                                  WLAN_VDEV_SM_EV_CSA_COMPLETE,
                                                  0, NULL);
                }
            }
        }

        qdf_mem_zero(ic->ic_csa_vdev_ids, WLAN_UMAC_PDEV_MAX_VDEVS);
        ic->ic_csa_num_vdevs = 0;
    }

    wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
    return 0;
}

static INLINE QDF_STATUS
ol_ath_extract_tsf(ol_ath_soc_softc_t *soc,
                   u_int8_t *data, u_int8_t idx,
                   struct tbttoffset_params *tbtt_param,
                   bool is_ext_tbtt)
{
    QDF_STATUS retval;
    struct common_wmi_handle *wmi_handle;

    wmi_handle = lmac_get_wmi_hdl(soc->psoc_obj);
    if (is_ext_tbtt) {
        retval = wmi_extract_ext_tbttoffset_update_params(wmi_handle,
                                                          data, idx, tbtt_param);
    } else {
        retval = wmi_extract_tbttoffset_update_params(wmi_handle,
                                                      data, idx, tbtt_param);
    }

    return retval;
}

/*
 * TSF Offset event handler
 */
static int
ol_ath_tsf_offset_event_handler(ol_ath_soc_softc_t *soc,
                                u_int8_t *data, u_int32_t num_vdevs,
                                bool is_ext_tbtt)
{
    struct ieee80211com *ic = NULL;
    struct ol_ath_softc_net80211 *scn = NULL;
    struct ieee80211vap *vap = NULL;
    struct ol_ath_vap_net80211 *avn = NULL;
    struct ieee80211_frame  *wh;
    u_int32_t vdev_id=0;
    u_int64_t adjusted_tsf_le;
    u_int32_t adjusted_tsf;
    uint8_t idx;
    struct tbttoffset_params tbtt_param = {0};
    struct wlan_objmgr_vdev *vdev;
    struct wlan_vdev_mgr_cfg mlme_cfg;
    bool disable_beacon = false;
    u_int64_t tx_delay = 0;
    uint64_t tsf_adj;

    for (idx = 0; idx < num_vdevs; idx++) {

        if (ol_ath_extract_tsf(soc, data, idx, &tbtt_param, is_ext_tbtt)) {
            QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_ERROR,
                      "%s: Failed to extract tsf offset info\n", __func__);
            return -1;
        }

        vdev_id = tbtt_param.vdev_id;
        adjusted_tsf = tbtt_param.tbttoffset;

        /* Get the beaconing VAP corresponding to the id */
        vdev = wlan_objmgr_get_vdev_by_id_from_psoc(soc->psoc_obj,
                vdev_id, WLAN_MLME_SB_ID);
        if (vdev == NULL) {
            /*should continue if vdev is NULL*/
            QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_ERROR,
                      "%s: Unable to find vdev for %d vdev_id\n",
                      __func__, vdev_id);
            continue;
        }

        if ((vap = wlan_vdev_get_mlme_ext_obj(vdev))) {
            /* Disable beacon if VAP is operating in NAWDS bridge mode */
            if (ieee80211_nawds_disable_beacon(vap)){
                disable_beacon = true;
            }
        }

        if ((!vap) || (disable_beacon)) {
            /*should continue if current vap is NULL*/
            wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);
            disable_beacon = false;
            continue;
        }

        avn = OL_ATH_VAP_NET80211(vap);
        scn = avn->av_sc;
        ic = &scn->sc_ic;

        if (IEEE80211_VAP_IS_MBSS_NON_TRANSMIT_ENABLED(vap)) {
            /* Don't handle TSF event for a non-transmitting MBSS VAP */
            wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);
	    continue;
        }

        qdf_spin_lock_bh(&avn->avn_lock);
        if (avn->av_wbuf == NULL) {
            qdf_spin_unlock_bh(&avn->avn_lock);
            qdf_debug("Beacon buf NULL. Ignoring tsf offset evt");
            wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);
            return -1;
        }

        /* Save the adjusted TSF */
        mlme_cfg.tsf = adjusted_tsf;
        wlan_util_vdev_mlme_set_param(vap->vdev_mlme, WLAN_MLME_CFG_TSF_ADJUST,
                mlme_cfg);

        if (lmac_is_target_ar900b(scn->soc->psoc_obj) ||
            ol_target_lithium(scn->soc->psoc_obj)) {
            if (IEEE80211_IS_CHAN_PUREG(ic->ic_curchan) || IEEE80211_IS_CHAN_5GHZ(ic->ic_curchan)
                                || IEEE80211_IS_CHAN_OFDM(ic->ic_curchan)) {
                /* 6Mbps Beacon: */
                tx_delay = 56; /*20(lsig)+2(service)+32(6mbps, 24 bytes) = 54us + 2us(MAC/BB DELAY) */
            }
            else if(IEEE80211_IS_CHAN_2GHZ(ic->ic_curchan)) {
                /* 1Mbps Beacon: */
                tx_delay = 386; /*144 us ( LPREAMBLE) + 48 (PLCP Header) + 192 (1Mbps, 24 ytes) = 384 us + 2us(MAC/BB DELAY */
            }
            /* Save the adjusted TSF */
            mlme_cfg.tsf = adjusted_tsf - tx_delay;
            wlan_util_vdev_mlme_set_param(vap->vdev_mlme, WLAN_MLME_CFG_TSF_ADJUST,
                    mlme_cfg);
        }

        ucfg_wlan_vdev_mgr_get_tsf_adjust(vdev,
                &tsf_adj);
        /*
         * Make the TSF offset negative so beacons in the same staggered batch
         * have the same TSF.
         */
        adjusted_tsf_le = qdf_cpu_to_le64(0ULL - tsf_adj);

        /* Update the timstamp in the beacon buffer with adjusted TSF */
        wh = (struct ieee80211_frame *)wbuf_header(avn->av_wbuf);
        OS_MEMCPY(&wh[1], &adjusted_tsf_le, sizeof(adjusted_tsf_le));

        if (vap->iv_bcn_offload_enable)
            ol_ath_bcn_tmpl_send(scn, vdev_id, vap);

        qdf_spin_unlock_bh(&avn->avn_lock);

        wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);
    }

    return 0;
}

/* WMI Beacon related Event APIs */
static int
ol_beacon_swba_handler(ol_scn_t sc, u_int8_t *data, u_int32_t datalen)
{
    ol_ath_soc_softc_t *soc = (ol_ath_soc_softc_t *) sc;

    return ol_ath_beacon_swba_handler(soc, data);
}

static int
ol_tbttoffset_update_event_handler(ol_scn_t sc, u_int8_t *data, u_int32_t datalen)
{
    ol_ath_soc_softc_t *soc = (ol_ath_soc_softc_t *) sc;
    uint32_t num_vdevs = 0;
    struct common_wmi_handle *wmi_handle;

    wmi_handle = lmac_get_wmi_hdl(soc->psoc_obj);
    if(wmi_extract_tbttoffset_num_vdevs(wmi_handle, data, &num_vdevs)) {
        QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_ERROR,
                  "%s: Failed to extract num vdevs\n", __func__);
        return -1;
    }
    return ol_ath_tsf_offset_event_handler(soc, data, num_vdevs, false);
}

static int
ol_ext_tbttoffset_update_event_handler(ol_scn_t sc, u_int8_t *data, u_int32_t datalen)
{
    ol_ath_soc_softc_t *soc = (ol_ath_soc_softc_t *) sc;
    uint32_t num_vdevs;
    struct common_wmi_handle *wmi_handle;

    wmi_handle = lmac_get_wmi_hdl(soc->psoc_obj);
    if(wmi_extract_ext_tbttoffset_num_vdevs(wmi_handle, data, &num_vdevs)) {
        QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_ERROR,
                  "%s: Failed to extract num vdevs\n", __func__);
        return -1;
    }
    return ol_ath_tsf_offset_event_handler(soc, data, num_vdevs, true);
}

int ol_ath_beacon_offload_control(struct ieee80211vap *vap, uint32_t bcn_ctrl_op)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    ol_ath_soc_softc_t *soc = scn->soc;
    struct ol_ath_vap_net80211 *avn = OL_ATH_VAP_NET80211(vap);
    struct bcn_offload_control bcn_ctrl = {0};
	struct common_wmi_handle *wmi_handle, *pdev_wmi_handle;

	pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
	wmi_handle = lmac_get_wmi_hdl(soc->psoc_obj);
    if(!wmi_handle) {
        qdf_err("%s: wmi_handle is NULL", __func__);
        return -1;
    }

    if (ol_ath_is_beacon_offload_enabled(scn->soc)) {
        if (!wmi_service_enabled(wmi_handle, wmi_service_bcn_offload_start_stop_support)) {
            qdf_err("Beacon offload start/stop not supported");
            return -1;
        }
    }

    bcn_ctrl.vdev_id = avn->av_if_id;
    switch (bcn_ctrl_op) {
        case IEEE80211_BCN_OFFLD_TX_DISABLE:
            bcn_ctrl.bcn_ctrl_op = BCN_OFFLD_CTRL_TX_DISABLE;
            break;
        case IEEE80211_BCN_OFFLD_TX_ENABLE:
            bcn_ctrl.bcn_ctrl_op = BCN_OFFLD_CTRL_TX_ENABLE;
            break;
        case IEEE80211_BCN_OFFLD_SWBA_DISABLE:
            bcn_ctrl.bcn_ctrl_op = BCN_OFFLD_CTRL_SWBA_DISABLE;
            break;
        case IEEE80211_BCN_OFFLD_SWBA_ENABLE:
            bcn_ctrl.bcn_ctrl_op = BCN_OFFLD_CTRL_SWBA_ENABLE;
            break;
        default:
            qdf_print("Beacon offload unknown ctrl operation %d",
                      bcn_ctrl_op);
            return -1;
    }

    if (wmi_send_bcn_offload_control_cmd(pdev_wmi_handle, &bcn_ctrl) !=
                                                          QDF_STATUS_SUCCESS) {
        qdf_print("Failed to send beacon control command");
        return -1;
    }

    return 0;
}
/*
 * Beacon related attach functions for offload solutions
 */
void
ol_ath_beacon_attach(struct ieee80211com *ic)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);

    if (ol_ath_is_beacon_offload_enabled(scn->soc)) {
        ic->ic_vdev_beacon_template_update = ol_ath_vdev_beacon_template_update;
        ic->ic_beacon_offload_control = ol_ath_beacon_offload_control;
#if UMAC_SUPPORT_QUIET
        ic->ic_set_bcn_offload_quiet_mode = ol_ath_set_quiet_offload_config_params;
#endif
    } else {
        ic->ic_vdev_beacon_template_update = NULL;
        ic->ic_beacon_offload_control = ol_ath_beacon_offload_control;
    }
    ic->ic_beacon_update = ol_ath_beacon_update;
    ic->ic_beacon_free = ol_ath_beacon_free;
    ic->ic_is_hwbeaconproc_active = ol_ath_net80211_is_hwbeaconproc_active;
    ic->ic_hw_beacon_rssi_threshold_enable = ol_ath_net80211_hw_beacon_rssi_threshold_enable;
    ic->ic_hw_beacon_rssi_threshold_disable = ol_ath_net80211_hw_beacon_rssi_threshold_disable;

}

void
ol_ath_beacon_soc_attach(ol_ath_soc_softc_t *soc)
{
	struct common_wmi_handle *wmi_handle;

    wmi_handle = lmac_get_wmi_hdl(soc->psoc_obj);
    /* Register WMI event handlers */
    wmi_unified_register_event_handler((void *)wmi_handle, wmi_host_swba_event_id,
                                            ol_beacon_swba_handler, WMI_RX_UMAC_CTX);
    wmi_unified_register_event_handler((void *)wmi_handle, wmi_tbttoffset_update_event_id,
                                           ol_tbttoffset_update_event_handler, WMI_RX_UMAC_CTX);
    wmi_unified_register_event_handler((void *)wmi_handle, wmi_ext_tbttoffset_update_event_id,
                                           ol_ext_tbttoffset_update_event_handler, WMI_RX_UMAC_CTX);
}
#endif
