/*
 * Copyright (c) 2014,2017-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2014 Qualcomm Atheros Inc.
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
 * LMAC offload interface functions for UMAC - Mcast enhancement feature
 */

#include "dp_extap.h"
#include "osif_ol.h"
#include "ol_if_athpriv.h"
#include "sw_version.h"
#include "cdp_txrx_me.h"
#include "cdp_txrx_cmn_struct.h"
#include "qdf_mem.h"   /* qdf_mem_malloc,free */
#include "qdf_lock.h"  /* qdf_spinlock_* */
#include "qdf_types.h" /* qdf_vprint */
#include "ol_ath.h"

#include "ol_if_stats.h"
#include "init_deinit_lmac.h"

#if ATH_SUPPORT_IQUE
static void
ol_ath_mcast_group_update(
    struct ieee80211com *ic,
    int action,
    int wildcard,
    u_int8_t *mcast_ip_addr,
    int mcast_ip_addr_bytes,
    u_int8_t *ucast_mac_addr,
    u_int8_t filter_mode,
    u_int8_t nsrcs,
    u_int8_t *srcs,
    u_int8_t *mask,
    u_int8_t vap_id)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    struct mcast_group_update_params param;
    struct common_wmi_handle *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    if (lmac_is_target_ar900b(scn->soc->psoc_obj))
      return;

    qdf_mem_set(&param, sizeof(param), 0);
    param.action = action;
    param.wildcard = wildcard;
    param.mcast_ip_addr = mcast_ip_addr;
    param.nsrcs = nsrcs;
    param.srcs = srcs;
    param.mask = mask;
    param.vap_id = vap_id;
    param.filter_mode = filter_mode;
    param.mcast_ip_addr_bytes = mcast_ip_addr_bytes;

    if(action == IGMP_ACTION_DELETE_MEMBER && wildcard && !mcast_ip_addr)  {
        param.is_action_delete = TRUE;
    }

    if(mcast_ip_addr_bytes != IGMP_IP_ADDR_LENGTH)
        param.is_mcast_addr_len = TRUE;

    if(filter_mode != IGMP_SNOOP_CMD_ADD_INC_LIST)
        param.is_filter_mode_snoop = TRUE;

    /* now correct for endianness, if necessary */
    /*
     * For Little Endian, N/w Stack gives packets in Network byte order and issue occurs
     * if both Host and Target happens to be in Little Endian. Target when compares IP
     * addresses in packet with MCAST_GROUP_CMDID given IP addresses, it fails. Hence
     * swap only mcast_ip_addr ( 16 bytes ) for now.
     * TODO : filter
     */
/*
#ifdef BIG_ENDIAN_HOST
    ol_bytestream_endian_fix(
            (u_int32_t *)&cmd->ucast_mac_addr, (sizeof(*cmd)-4) / sizeof(u_int32_t));
#else
    ol_bytestream_endian_fix(
            (u_int32_t *)&cmd->mcast_ip_addr, (sizeof(cmd->mcast_ip_addr)) / sizeof(u_int32_t));
#endif  Little Endian */

    wmi_unified_mcast_group_update_cmd_send(
            pdev_wmi_handle, &param);
}

/*
 * ol_me_find_next_hop_mac:
 * Find the receiver's address (RA) for a multicast group member given
 * it's destination address (DA) from the MCS snoop table.
 *
 * Parameters:
 * @vap: Handle to the vap structure
 *
 * Returns:
 * Nothing
 */
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
void ol_me_find_next_hop_mac(struct ieee80211vap *vap)
{
    void *soc = wlan_psoc_get_dp_handle(wlan_pdev_get_psoc(vap->iv_ic->ic_pdev_obj));
    uint8_t pdev_id = wlan_objmgr_pdev_get_pdev_id(vap->iv_ic->ic_pdev_obj);
    struct cdp_ast_entry_info ast_entry_info;
    uint32_t group_ix, group_cnt;
    uint32_t node_ix, node_ix2, node_cnt;

    /* Resolving the RA, if there is any failure, the destination
     * MAC address will be used. This is done for each node of each
     * group address */
    group_cnt = vap->iv_me->me_hifi_table.entry_cnt;
    for (group_ix = 0; group_ix < group_cnt; group_ix++) {
        node_cnt = vap->iv_me->me_hifi_table.entry[group_ix].node_cnt;
        for (node_ix = 0; node_ix < node_cnt; node_ix++) {
            vap->iv_me->me_ra[group_ix][node_ix].dup = 0;
            if (cdp_peer_get_ast_info_by_pdev((struct cdp_soc_t *)soc,
                                               vap->iv_me->me_hifi_table.entry[group_ix].nodes[node_ix].mac,
                                               pdev_id, &ast_entry_info)) {
                IEEE80211_ADDR_COPY(vap->iv_me->me_ra[group_ix][node_ix].mac,
                                    ast_entry_info.peer_mac_addr);
            } else {
                /* If AST entry is not found, then the destination address is copied
                 * to the receiver's address table */
                IEEE80211_ADDR_COPY(vap->iv_me->me_ra[group_ix][node_ix].mac,
                                    vap->iv_me->me_hifi_table.entry[group_ix].nodes[node_ix].mac);
            }

            /* If there are members with the same next-hop, subsequent members
             * are ignored */
            for (node_ix2 = 0; node_ix2 < node_ix; node_ix2++) {
                if(IEEE80211_ADDR_EQ(vap->iv_me->me_ra[group_ix][node_ix].mac,
                                     vap->iv_me->me_ra[group_ix][node_ix2].mac)) {
                    vap->iv_me->me_ra[group_ix][node_ix].dup = 1;
                    break;
                }
            }
        }
    }
}
#endif

#ifdef QCA_OL_DMS_WAR
/*
 * ol_me_convert_amsdu_ucast:
 * Encapsulates an 802.11 header on the 802.3 frame and sends the resulting
 * buffer to the firmware to enable the AMSDU aggregation.
 *
 * Parameters:
 * @scn        : Handle to scn pointer
 * @vap        : Handle to vap pointer
 * @wbuf       : Frame buffer containing the 802.3 frame
 * @newmac     : List of MAC address of the members of the multicast group
 * @new_mac_cnt: Total number of members in the multicast group
 *
 * Return:
 * Total Number of successful unicast conversions
 */

uint16_t ol_me_convert_amsdu_ucast(struct ol_ath_softc_net80211 *scn,
                                   struct ieee80211vap *vap, qdf_nbuf_t wbuf,
                                   uint8_t mcast_grp_mac[][IEEE80211_ADDR_LEN],
                                   uint8_t mcast_grp_mac_cnt)
{
    uint8_t total_mcast_grp_mac = mcast_grp_mac_cnt;
    struct cdp_tx_exception_metadata tx_exc_param = {0};
    struct vdev_osif_priv *vdev_osifp = NULL;
    osif_dev  *osifp_handle = NULL;
    qdf_nbuf_t wbuf_copy = NULL;

    vdev_osifp = wlan_vdev_get_ospriv(vap->vdev_obj);
    if (!vdev_osifp) {
        qdf_err("Invalid vdev osif pointer");
        QDF_ASSERT(0);
        return 0;
    }

    osifp_handle = vdev_osifp->legacy_osif_priv;
    if (!osifp_handle) {
        qdf_err("Invalid osifp_handle");
        return 0;
    }

    while(mcast_grp_mac_cnt > 0 && mcast_grp_mac_cnt--) {
        if (mcast_grp_mac_cnt > 0) {
            wbuf_copy = qdf_nbuf_copy(wbuf);
            if (wbuf_copy == NULL) {
                qdf_nbuf_free(wbuf);
                return (total_mcast_grp_mac - (mcast_grp_mac_cnt+1));
            }
        } else {
            wbuf_copy = wbuf;
        }
        wbuf_copy->next = NULL;

        if (OL_DMS_AMSDU_WAR(&wbuf_copy, scn, vap, mcast_grp_mac[mcast_grp_mac_cnt],
                             &tx_exc_param)) {
            qdf_err("Unable to convert to native wifi packet");
            qdf_nbuf_free(wbuf_copy);
            return (total_mcast_grp_mac - (mcast_grp_mac_cnt+1));
        }

        wbuf_copy = ((ol_txrx_tx_exc_fp)osifp_handle->iv_vap_send_exc)
                             (wlan_vdev_get_dp_handle(osifp_handle->ctrl_vdev),
                             wbuf_copy, &tx_exc_param);

        if (wbuf_copy != NULL) {
            qdf_err("There was an error in TX");
            qdf_nbuf_free(wbuf_copy);
        }
    }

    return (total_mcast_grp_mac - mcast_grp_mac_cnt);
}
#endif

extern uint16_t
ol_me_convert_ucast(struct ieee80211vap *vap, qdf_nbuf_t wbuf,
                    u_int8_t newmac[][IEEE80211_ADDR_LEN],
                    uint8_t new_mac_cnt)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(vap->iv_ic);
    ol_txrx_soc_handle soc_txrx_handle;

    soc_txrx_handle = wlan_psoc_get_dp_handle(scn->soc->psoc_obj);

#ifdef QCA_OL_DMS_WAR
    if (vap->iv_me_amsdu)
        return ol_me_convert_amsdu_ucast(scn, vap, wbuf, newmac, new_mac_cnt);
    else
#endif
        return cdp_tx_me_convert_ucast(soc_txrx_handle,
                                       (void *)wlan_vdev_get_dp_handle(vap->vdev_obj), wbuf,
                                       newmac, new_mac_cnt);
}

int ol_if_me_setup(struct ieee80211com *ic)
{
    ic->ic_mcast_group_update = ol_ath_mcast_group_update;
    ic->ic_me_convert = ol_me_convert_ucast;
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
    ic->ic_find_next_hop_mac = ol_me_find_next_hop_mac;
#endif
    return 1;
}

#endif /*ATH_SUPPORT_IQUE*/
