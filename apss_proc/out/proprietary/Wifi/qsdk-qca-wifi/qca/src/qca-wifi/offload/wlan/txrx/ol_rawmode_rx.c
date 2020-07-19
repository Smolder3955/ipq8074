/* Copyright (c) 2014, 2017-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */
/*
 * 2014 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include <qdf_nbuf.h>               /* qdf_nbuf_t, etc. */
#include <ol_txrx_types.h>          /* ol_txrx_pdev_t */
#include <cdp_txrx_cmn_struct.h>            /* ol_txrx_vdev_handle */
#include <ol_rawmode_txrx_api.h>    /* API definitions */
#include <ol_htt_rx_api.h>          /* htt_rx_msdu_desc_retrieve, etc */
#include <ol_if_athvar.h>           /* ol_ath_softc_net80211, etc */
#include <ol_txrx_internal.h>       /* OL_TXRX_LIST_APPEND, etc */
#include <ol_txrx_api_internal.h>

#if QCA_SUPPORT_RAWMODE_PKT_SIMULATION
#include <osif_rawmode_sim_api.h>
#endif /* QCA_SUPPORT_RAWMODE_PKT_SIMULATION */
#include <init_deinit_lmac.h>

#if MESH_MODE_SUPPORT
extern void ol_rx_mesh_mode_per_pkt_rx_info(qdf_nbuf_t nbuf, struct ol_txrx_peer_t *peer, struct ol_txrx_vdev_t *vdev);
#endif

#if QCA_OL_SUPPORT_RAWMODE_TXRX

/* Raw Mode specific Rx functionality */

void
ol_rx_deliver_raw(
    struct ol_txrx_vdev_t *vdev,
    struct ol_txrx_peer_t *peer,
    unsigned tid,
    qdf_nbuf_t nbuf_list)
{
    struct ol_txrx_pdev_t *pdev = vdev->pdev;
    htt_pdev_handle htt_pdev = pdev->htt_pdev;
    qdf_nbuf_t deliver_list_head = NULL;
    qdf_nbuf_t deliver_list_tail = NULL;
    qdf_nbuf_t nbuf;
    u_int8_t is_last_frag = 0;
#if QCA_SUPPORT_RAWMODE_PKT_SIMULATION
    struct ieee80211vap *vap = NULL;
#endif
#if OL_ATH_SUPPORT_LED
    uint32_t   byte_cnt = 0;
    uint32_t   tot_frag_cnt = 0;
#endif

    nbuf = nbuf_list;
    /*
     * Check each nbuf to see whether it requires special handling,
     * and free each nbuf's rx descriptor
     */
    while (nbuf) {
        void *rx_desc;
        int discard, inspect, dummy_fwd;
        qdf_nbuf_t next = qdf_nbuf_next(nbuf);
        qdf_nbuf_t nextfrag = NULL;
        qdf_nbuf_t tempfrag = NULL;

#if OL_ATH_SUPPORT_LED
        tot_frag_cnt = 0;
#endif

        rx_desc = htt_rx_msdu_desc_retrieve(pdev->htt_pdev, nbuf);

        htt_rx_msdu_actions(
            pdev->htt_pdev, rx_desc, &discard, &dummy_fwd, &inspect);

        htt_rx_msdu_desc_free(htt_pdev, nbuf);

        /* Access Controller to carry out any additional filtering if required,
         * since payload could be encrypted.
         */
        if (discard) {
            if (qdf_nbuf_is_rx_chfrag_start(nbuf)) {
                nextfrag = qdf_nbuf_next(nbuf);
                qdf_nbuf_free(nbuf);
                while (nextfrag) {
                    is_last_frag = qdf_nbuf_is_rx_chfrag_end(nextfrag);
                    tempfrag = nextfrag;
                    nextfrag = qdf_nbuf_next(tempfrag);
                    qdf_nbuf_free(tempfrag);

                    if (is_last_frag) {
                        next = nextfrag;
                        /* At this point, next either points to the nbuf after
                         * the last fragment, or is NULL.
                         */
                        break;
                    }
                }
            } else {
                qdf_nbuf_free(nbuf);
            }

            /* If discarded packet is last packet of the delivery list,
             * NULL terminator should be added to delivery list. */
            if (next == NULL && deliver_list_head){
                qdf_nbuf_set_next(deliver_list_tail, NULL);
            }
        } else {
#if MESH_MODE_SUPPORT
            if (vdev->mesh_vdev) {
                ol_rx_mesh_mode_per_pkt_rx_info(nbuf, peer, vdev);
            }
#endif
            OL_TXRX_LIST_APPEND(deliver_list_head, deliver_list_tail, nbuf);
            if (qdf_nbuf_is_rx_chfrag_start(nbuf)) {
                nextfrag = qdf_nbuf_next(nbuf);
                while (nextfrag) {
#if OL_ATH_SUPPORT_LED
                    tot_frag_cnt += qdf_nbuf_len(nextfrag);
#endif
                    OL_TXRX_LIST_APPEND(deliver_list_head,
                            deliver_list_tail,
                            nextfrag);

                    if (qdf_nbuf_is_rx_chfrag_end(nextfrag)) {
                        next = qdf_nbuf_next(nextfrag);
                        /* At this point, next either points to the nbuf after
                         * the last fragment, or is NULL.
                         */
                        break;
                    } else {
                        nextfrag = qdf_nbuf_next(nextfrag);
                    }
                }
            }

#if OL_ATH_SUPPORT_LED
            /* Note: For Raw Mode, this will only be a rough count, since
             * payload (which includes A-MSDU) could be encrypted.
             */
            byte_cnt += (qdf_nbuf_len(nbuf) + tot_frag_cnt);
#endif
        }

        nbuf = next;
    }
    /* sanity check - are there any frames left to give to the OS shim? */
    if (!deliver_list_head) {
        return;
    }

#if QCA_SUPPORT_RAWMODE_PKT_SIMULATION
    vap = ol_ath_getvap(vdev->osif_vdev);
    if (!vap) {
        return;
    }

    if (vap->iv_rawmode_pkt_sim) {
	vdev->osif_rsim_rx_decap(vdev->osif_vdev, &deliver_list_head, &deliver_list_tail, (struct cdp_peer *) peer);
	if (!deliver_list_head) {
	    return;
	}
    }
#endif /* QCA_SUPPORT_RAWMODE_PKT_SIMULATION */

    vdev->osif_rx(vdev->osif_vdev, deliver_list_head);

#if OL_ATH_SUPPORT_LED
    {
        struct ol_ath_softc_net80211 *scn;
        scn = (struct ol_ath_softc_net80211 *)
                  lmac_get_pdev_feature_ptr((struct wlan_objmgr_pdev *)pdev->ctrl_pdev);
        if (scn) {
            scn->scn_led_byte_cnt += byte_cnt;
            ol_ath_led_event(scn, OL_ATH_LED_RX);
        }
    }
#endif
}

/* Note: This is an adaptation of the function ol_rx_mpdu_list_next(), for Raw
 * Mode.
 */
#if QCA_RAWMODE_OPTIMIZATION_CONFIG > 0
inline
#endif /* QCA_RAWMODE_OPTIMIZATION_CONFIG */
void
ol_rx_mpdu_list_next_raw(
    struct ol_txrx_pdev_t *pdev,
    void *mpdu_list,
    qdf_nbuf_t *mpdu_tail,
    qdf_nbuf_t *next_mpdu)
{
    qdf_nbuf_t nbuf;

    TXRX_ASSERT2(mpdu_list);
    nbuf = mpdu_list;

    if (qdf_nbuf_is_rx_chfrag_start(nbuf)) {
        nbuf = qdf_nbuf_next(nbuf);
        TXRX_ASSERT2(nbuf);
        while (!qdf_nbuf_is_rx_chfrag_end(nbuf))
        {
            nbuf = qdf_nbuf_next(nbuf);
            TXRX_ASSERT2(nbuf);
        }
    }

    *mpdu_tail = nbuf;
    *next_mpdu = qdf_nbuf_next(nbuf);
}

#endif /* QCA_OL_SUPPORT_RAWMODE_TXRX */


