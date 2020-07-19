/*
 * Copyright (c) 2013-2015, 2017-2018 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2013-2015 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#ifndef REMOVE_PKT_LOG
#include "ol_txrx_types.h"
#include "ol_htt_tx_api.h"
#include "ol_tx_desc.h"
#include "qdf_mem.h"
#include "a_debug.h"
#include "hif.h"
#include "legacy/htt.h"
#include "htt_internal.h"
#include "pktlog_ac_i.h"
#include "target_type.h"
#include <dbglog_host.h>
#include "pktlog_ac_fmt.h"
#include "ieee80211.h"
#include <pktlog_fmt.h>
#include <linux_remote_pktlog.h>
#include <qdf_mem.h>
#include <init_deinit_lmac.h>
//#include "ratectrl_11ac.h"

#if QCA_PARTNER_DIRECTLINK_RX
#define QCA_PARTNER_DIRECTLINK_PKTLOG_INTERNAL 1
#include "ath_carr_pltfrm.h"
#undef QCA_PARTNER_DIRECTLINK_PKTLOG_INTERNAL
#endif

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
#include <osif_nss_wifiol_if.h>
#endif

#define TX_DESC_ID_LOW_MASK 0xffff
#define TX_DESC_ID_LOW_SHIFT 0
#define TX_DESC_ID_HIGH_MASK 0xffff0000
#define TX_DESC_ID_HIGH_SHIFT 16

#ifdef CONFIG_AR900B_SUPPORT
static QDF_STATUS pktlog_hif_callback(void *pdev, wbuf_t wbuf, u_int8_t pipeID);
#endif
#ifdef CONFIG_AR900B_SUPPORT
struct hif_msg_callbacks hif_pipe_callbacks;
#endif

static struct txctl_frm_hdr frm_hdr;

#ifdef CONFIG_AR900B_SUPPORT
#define CE_PKTLOG_PIPE 8
QDF_STATUS hif_pktlog_subscribe(
                struct ol_ath_softc_net80211 *scn)
{
    struct hif_opaque_softc *sc = lmac_get_ol_hif_hdl(scn->soc->psoc_obj);

    if (!sc) {
        qdf_err("Invalid sc in %s", __func__);
        return QDF_STATUS_E_INVAL;
    }

    hif_pipe_callbacks.rxCompletionHandler = pktlog_hif_callback;
    hif_pipe_callbacks.Context = scn->soc;
    hif_update_pipe_callback(sc, CE_PKTLOG_PIPE, &hif_pipe_callbacks);

    return QDF_STATUS_SUCCESS;
}
#endif


static void process_ieee_hdr(void *data)
{
    uint8_t dir;
    struct ieee80211_frame *wh = (struct ieee80211_frame *)(data);

    frm_hdr.framectrl = *(u_int16_t *)(wh->i_fc);
    frm_hdr.seqctrl   = *(u_int16_t *)(wh->i_seq);
    dir = (wh->i_fc[1] & IEEE80211_FC1_DIR_MASK);

    if (dir == IEEE80211_FC1_DIR_TODS) {
        frm_hdr.bssid_tail = (wh->i_addr1[IEEE80211_ADDR_LEN-2] << 8) |
                             (wh->i_addr1[IEEE80211_ADDR_LEN-1]);
        frm_hdr.sa_tail    = (wh->i_addr2[IEEE80211_ADDR_LEN-2] << 8) |
                             (wh->i_addr2[IEEE80211_ADDR_LEN-1]);
        frm_hdr.da_tail    = (wh->i_addr3[IEEE80211_ADDR_LEN-2] << 8) |
                             (wh->i_addr3[IEEE80211_ADDR_LEN-1]);
    } else if (dir == IEEE80211_FC1_DIR_FROMDS) {
        frm_hdr.bssid_tail = (wh->i_addr2[IEEE80211_ADDR_LEN-2] << 8) |
                             (wh->i_addr2[IEEE80211_ADDR_LEN-1]);
        frm_hdr.sa_tail    = (wh->i_addr3[IEEE80211_ADDR_LEN-2] << 8) |
                             (wh->i_addr3[IEEE80211_ADDR_LEN-1]);
        frm_hdr.da_tail    = (wh->i_addr1[IEEE80211_ADDR_LEN-2] << 8) |
                             (wh->i_addr1[IEEE80211_ADDR_LEN-1]);
    } else {
        frm_hdr.bssid_tail = (wh->i_addr3[IEEE80211_ADDR_LEN-2] << 8) |
                             (wh->i_addr3[IEEE80211_ADDR_LEN-1]);
        frm_hdr.sa_tail    = (wh->i_addr2[IEEE80211_ADDR_LEN-2] << 8) |
                             (wh->i_addr2[IEEE80211_ADDR_LEN-1]);
        frm_hdr.da_tail    = (wh->i_addr1[IEEE80211_ADDR_LEN-2] << 8) |
                             (wh->i_addr1[IEEE80211_ADDR_LEN-1]);
    }
}

A_STATUS
process_tx_msdu_info_ar9888 (ol_txrx_pdev_handle txrx_pdev,
                             void *data, ath_pktlog_hdr_t *pl_hdr)
{
    ol_pktlog_dev_t *pl_dev = NULL;
    struct ol_ath_soc_softc *soc;
    struct ath_pktlog_info *pl_info = NULL;
    uint32_t i = 0;
    uint32_t *htt_tx_desc = NULL;
    size_t log_size = 0;
    struct ol_tx_desc_t *tx_desc = NULL;
    uint8_t msdu_id_offset = 0;
    uint16_t tx_desc_id = 0;
    u_int8_t *addr = NULL;
    u_int8_t vap_addr[IEEE80211_ADDR_LEN];
    u_int8_t ret_val = 0;
    u_int8_t vdev_id = 0;
    qdf_nbuf_t netbuf = NULL;
    u_int32_t len = 0;
    uint32_t *msdu_id_info = NULL;
    uint32_t *msdu_id = NULL;
    static struct ath_pktlog_msdu_info_ar9888 pl_msdu_info;

    /*
     *  Must include to process different types
     *  TX_CTL, TX_STATUS, TX_MSDU_ID, TX_FRM_HDR
     */
    pl_dev = ((struct ol_txrx_pdev_t *)txrx_pdev)->pl_dev;
    if (!pl_dev) {
        return -1;
    }
    soc = ((struct ol_ath_softc_net80211 *)pl_dev->scn)->soc;
    if (!soc) {
        return -1;
    }
    pl_info = pl_dev->pl_info;
    msdu_id_offset = pl_dev->msdu_id_offset;

    msdu_id_info = (uint32_t *) ((void *)data + pl_dev->pktlog_hdr_size);
    msdu_id = (uint32_t *)((char *)msdu_id_info + msdu_id_offset);


    pl_msdu_info.num_msdu = *msdu_id_info;
    pl_msdu_info.priv_size = sizeof(uint32_t) * pl_msdu_info.num_msdu +
                             sizeof(uint32_t);
    log_size = sizeof(pl_msdu_info.priv);

    for (i = 0; i < pl_msdu_info.num_msdu; i++) {
        /*
         *  Handle big endianess
         *  Increment msdu_id once after retrieving
         *  lower 16 bits and uppper 16 bits
         */
        if (!(i % 2)) {
            tx_desc_id = ((*msdu_id & TX_DESC_ID_LOW_MASK)
                    >> TX_DESC_ID_LOW_SHIFT);
        } else {
            tx_desc_id = ((*msdu_id & TX_DESC_ID_HIGH_MASK)
                    >> TX_DESC_ID_HIGH_SHIFT);
            msdu_id += 1;
        }

        if (soc->ol_if_ops->ol_tx_desc_find)
            tx_desc = soc->ol_if_ops->ol_tx_desc_find(txrx_pdev, tx_desc_id);

        qdf_assert(tx_desc);
        netbuf = tx_desc->netbuf;
        qdf_assert(netbuf);
        htt_tx_desc = (uint32_t *) tx_desc->htt_tx_desc;
        qdf_assert(htt_tx_desc);
        if (!(pl_hdr->flags & (1 << PKTLOG_FLG_FRM_TYPE_CLONE_S))){
            qdf_nbuf_peek_header(netbuf, &addr, &len);
            qdf_assert(addr);
            if (len < (2 * IEEE80211_ADDR_LEN)) {
                qdf_err("TX frame does not have a valid address %d ",len);
                return -1;
            }
            /* Adding header information for the TX data frames */
            frm_hdr.da_tail    = (addr[IEEE80211_ADDR_LEN-2] << 8) |
                (addr[IEEE80211_ADDR_LEN-1]);
            frm_hdr.sa_tail    = (addr[2 * IEEE80211_ADDR_LEN-2] << 8) |
                (addr[2 * IEEE80211_ADDR_LEN-1]);

            vdev_id = (u_int8_t)(*(htt_tx_desc + HTT_TX_VDEV_ID_WORD) >>
                    HTT_TX_VDEV_ID_SHIFT) &
                HTT_TX_VDEV_ID_MASK;
            ret_val = ol_ath_vap_get_myaddr((struct ol_ath_softc_net80211 *)pl_dev->scn, vdev_id, vap_addr);

            if (ret_val) {
                frm_hdr.bssid_tail = (vap_addr[IEEE80211_ADDR_LEN-2] << 8) |
                    (vap_addr[IEEE80211_ADDR_LEN-1]);
            } else {
                frm_hdr.bssid_tail = 0x0000;
            }
            pl_msdu_info.priv.msdu_len[i] = *(htt_tx_desc +
                    HTT_TX_MSDU_LEN_DWORD)
                & HTT_TX_MSDU_LEN_MASK;
        } else {
            /* This is for cloned packet init with some constants*/
            frm_hdr.da_tail = 0x0000;
            frm_hdr.sa_tail = 0x0000;
            frm_hdr.bssid_tail = 0x0000;
            pl_msdu_info.priv.msdu_len[i] = 200;
        }
    }

    /*
     * Add more information per MSDU
     * e.g., protocol information
     */
    pl_msdu_info.ath_msdu_info = pktlog_getbuf(pl_dev, pl_info,
            log_size, pl_hdr);
    if (pl_msdu_info.ath_msdu_info == NULL) {
        return A_ERROR;
    }
    qdf_mem_copy((void *)&pl_msdu_info.priv.msdu_id_info,
            ((void *)data + pl_dev->pktlog_hdr_size),
            sizeof(pl_msdu_info.priv.msdu_id_info));
    qdf_mem_copy(pl_msdu_info.ath_msdu_info, &pl_msdu_info.priv,
            sizeof(pl_msdu_info.priv));

    return A_OK;
}


A_STATUS
process_tx_msdu_info_ar900b (ol_txrx_pdev_handle txrx_pdev,
                             void *data, ath_pktlog_hdr_t *pl_hdr)
{
    ol_pktlog_dev_t *pl_dev = NULL;
    struct ol_ath_soc_softc *soc = NULL;
    struct ath_pktlog_info *pl_info = NULL;
    uint32_t i = 0;
    uint32_t *htt_tx_desc = NULL;
    size_t log_size = 0;
    struct ol_tx_desc_t *tx_desc = NULL;
    uint8_t msdu_id_offset = 0;
    uint16_t tx_desc_id = 0;
    u_int8_t *addr = NULL;
    u_int8_t vap_addr[IEEE80211_ADDR_LEN];
    u_int8_t ret_val = 0;
    u_int8_t vdev_id = 0;
    qdf_nbuf_t netbuf = NULL;
    u_int32_t len = 0;
    uint32_t *msdu_id_info = NULL;
    uint32_t *msdu_id = NULL;
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    uint8_t *pl_metadata_addr = NULL;
#endif
    static struct ath_pktlog_msdu_info_ar900b pl_msdu_info;
    /*
     *  Must include to process different types
     *  TX_CTL, TX_STATUS, TX_MSDU_ID, TX_FRM_HDR
     */
    pl_dev = ((struct ol_txrx_pdev_t *)txrx_pdev)->pl_dev;
    soc = ((struct ol_ath_softc_net80211 *)pl_dev->scn)->soc;

    pl_info = pl_dev->pl_info;
    msdu_id_offset = pl_dev->msdu_id_offset;

    msdu_id_info = (uint32_t *) ((void *)data + pl_dev->pktlog_hdr_size);
    msdu_id = (uint32_t *)((char *)msdu_id_info + msdu_id_offset);


    pl_msdu_info.num_msdu = *msdu_id_info;
    pl_msdu_info.priv_size = sizeof(uint32_t) * pl_msdu_info.num_msdu +
                             sizeof(uint32_t);
    log_size = sizeof(pl_msdu_info.priv);

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    if (((struct ol_txrx_pdev_t *)txrx_pdev)->nss_wifiol_ctx) {
        pl_metadata_addr = (char *)data + 116;
        if(pl_metadata_addr == NULL)
           return A_ERROR;
    }
#endif

    for (i = 0; i < pl_msdu_info.num_msdu; i++) {
        /*
         *  Handle big endianess
         *  Increment msdu_id once after retrieving
         *  lower 16 bits and uppper 16 bits
         */
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
        if (((struct ol_txrx_pdev_t *)txrx_pdev)->nss_wifiol_ctx) {
            struct nss_wifi_pl_metadata *pl_metadata = (struct nss_wifi_pl_metadata *)(pl_metadata_addr);
            if (!(pl_hdr->flags & (1 << PKTLOG_FLG_FRM_TYPE_CLONE_S))) {
                len = pl_metadata->len;
                if (len < (2 * IEEE80211_ADDR_LEN)) {
                    qdf_err("TX frame does not have a valid address %d ",len);
                    return -1;
                }

                /* Adding header information for the TX data frames */
                frm_hdr.da_tail = pl_metadata->da_tail;
                frm_hdr.sa_tail = pl_metadata->sa_tail;

                vdev_id = pl_metadata->vdev_id;
                ret_val = ol_ath_vap_get_myaddr((struct ol_ath_softc_net80211 *)pl_dev->scn, vdev_id, vap_addr);
                if (ret_val) {
                    frm_hdr.bssid_tail = (vap_addr[IEEE80211_ADDR_LEN-2] << 8) |
                        (vap_addr[IEEE80211_ADDR_LEN-1]);
                } else {
                    frm_hdr.bssid_tail = 0x0000;
                }
                ret_val = 0;

                pl_msdu_info.priv.msdu_len[i] = pl_metadata->msdu_len;
                pl_metadata_addr += (sizeof(struct nss_wifi_pl_metadata));
            } else {
                /* This is for cloned packet init with some constants*/
                frm_hdr.da_tail = 0x0000;
                frm_hdr.sa_tail = 0x0000;
                frm_hdr.bssid_tail = 0x0000;
                pl_msdu_info.priv.msdu_len[i] = 200;
            }
        } else
#endif
        {
#ifdef BIG_ENDIAN_HOST
            if ((i % 2)) {
                tx_desc_id = ((*msdu_id & TX_DESC_ID_LOW_MASK)
                        >> TX_DESC_ID_LOW_SHIFT);
                msdu_id += 1;
            } else {
                tx_desc_id = ((*msdu_id & TX_DESC_ID_HIGH_MASK)
                        >> TX_DESC_ID_HIGH_SHIFT);
            }

#else
            if (!(i % 2)) {
                tx_desc_id = ((*msdu_id & TX_DESC_ID_LOW_MASK)
                        >> TX_DESC_ID_LOW_SHIFT);
            } else {
                tx_desc_id = ((*msdu_id & TX_DESC_ID_HIGH_MASK)
                        >> TX_DESC_ID_HIGH_SHIFT);
                msdu_id += 1;
            }
#endif
            if (soc->ol_if_ops->ol_tx_desc_find)
	    	tx_desc = soc->ol_if_ops->ol_tx_desc_find(txrx_pdev, tx_desc_id);

            qdf_assert(tx_desc);
            netbuf = tx_desc->netbuf;
            qdf_assert(netbuf);
            htt_tx_desc = (uint32_t *) tx_desc->htt_tx_desc;
            qdf_assert(htt_tx_desc);
            if(!(pl_hdr->flags & (1 << PKTLOG_FLG_FRM_TYPE_CLONE_S))){
                qdf_nbuf_peek_header(netbuf, &addr, &len);
                qdf_assert(addr);
                if (len < (2 * IEEE80211_ADDR_LEN)) {
                    qdf_err("TX frame does not have a valid address %d ",len);
                    return -1;
                }


                /* Adding header information for the TX data frames */
                frm_hdr.da_tail    = (addr[IEEE80211_ADDR_LEN-2] << 8) |
                    (addr[IEEE80211_ADDR_LEN-1]);
                frm_hdr.sa_tail    = (addr[2 * IEEE80211_ADDR_LEN-2] << 8) |
                    (addr[2 * IEEE80211_ADDR_LEN-1]);

                vdev_id = (u_int8_t)(*(htt_tx_desc + HTT_TX_VDEV_ID_WORD) >>
                        HTT_TX_VDEV_ID_SHIFT) &
                    HTT_TX_VDEV_ID_MASK;
                ret_val = ol_ath_vap_get_myaddr((struct ol_ath_softc_net80211 *)pl_dev->scn, vdev_id, vap_addr);

                if (ret_val) {
                    frm_hdr.bssid_tail = (vap_addr[IEEE80211_ADDR_LEN-2] << 8) |
                        (vap_addr[IEEE80211_ADDR_LEN-1]);
                } else {
                    frm_hdr.bssid_tail = 0x0000;
                }
                ret_val = 0;
                pl_msdu_info.priv.msdu_len[i] = *(htt_tx_desc +
                        HTT_TX_MSDU_LEN_DWORD)
                    & HTT_TX_MSDU_LEN_MASK;

            } else {
                /* This is for cloned packet init with some constants*/
                frm_hdr.da_tail = 0x0000;
                frm_hdr.sa_tail = 0x0000;
                frm_hdr.bssid_tail = 0x0000;
                pl_msdu_info.priv.msdu_len[i] = 200;
            }
        }
    }

    /*
     * Add more information per MSDU
     * e.g., protocol information
     */
    pl_msdu_info.ath_msdu_info = pktlog_getbuf(pl_dev, pl_info,
            log_size, pl_hdr);
    if (pl_msdu_info.ath_msdu_info == NULL) {
        return A_ERROR;
    }
    qdf_mem_copy((void *)&pl_msdu_info.priv.msdu_id_info,
            ((void *)data + pl_dev->pktlog_hdr_size),
            sizeof(pl_msdu_info.priv.msdu_id_info));
    qdf_mem_copy(pl_msdu_info.ath_msdu_info, &pl_msdu_info.priv,
            sizeof(pl_msdu_info.priv));

    return A_OK;
}


static void
pktlog_txcap_add_radiotap_header(ol_pktlog_dev_t *pl_dev,
                                 struct ath_pktlog_siginfo *siginfo,
                                 struct ath_pktlog_tx_capture *txbuf)
{
    struct ol_ath_softc_net80211 *scn =
                                    (struct ol_ath_softc_net80211 *)pl_dev->scn;
    struct ieee80211com *ic = &scn->sc_ic;
    struct ieee80211_radiotap_header *rthdr = NULL;
    u_int32_t *it_present_p = NULL;
    u_int8_t *datap = NULL;
    u_int8_t ht=0, vht=0, radiotap_mcs_details = 0, vht_coding_details = 0,
             lsig_rate = 0;
    u_int32_t bw = 0, sgi = 0, ldpc = 0, stbc = 0, groupid = 0, su_ppdu = 0,
              txbf = 0, l_sig = 0,sig_a_1 = 0, sig_a_2 = 0, sig_b_0 = 0,
              mcs = 0, nss = 0, base = 0, signal = 0, partial_aid = 0,
              nsts_u0 = 0, nsts_u2 = 0, nsts_u3 = 0, ldpc_u0 = 0,
              ldpc_u1 = 0, ldpc_u2 = 0, ldpc_u3 = 0, sig_b_1 = 0, sig_b_2 = 0,
              nsts_su = 0, rt_total_len = 0, it_present_mask = 0;
    u_int16_t radiotap_vht_details = 0;

    rthdr = &txbuf->rthdr;
    l_sig = siginfo->l_sig;
    sig_a_1 = siginfo->ht_vht_sig1;
    sig_a_2 = siginfo->ht_vht_sig2;
    sig_b_0 = siginfo->vht_sig_b_0;
    sig_b_1 = siginfo->vht_sig_b_1;
    sig_b_2 = siginfo->vht_sig_b_2;

    lsig_rate = l_sig & PHY_RATE_MCS_MASK;

    if (siginfo->preamble_type == 0xFF) {
        /* Preamble and other info needs to be extracted for AR9000 */
        if (lsig_rate & 0x1101) {
            /* This is HT/VHT frame */
            if (sig_b_1) {
                /* VHT frame */
                vht = 1;
                bw = PHY_BANDWIDTH_ENCODE_SHIFT <<
                                             (sig_a_1 & PHY_BANDWIDTH_MASK_VHT);
                sgi = sig_a_2 & PHY_SGI_MASK;
                ldpc = (sig_a_2 >> PHY_LDPC_SHIFT_VHT) & PHY_LDPC_MASK;
                stbc = (sig_a_1 >> PHY_STBC_SHIFT_VHT) & PHY_STBC_MASK_VHT;
                groupid = (sig_a_1 >> PHY_GROUPID_SHIFT) & PHY_GROUPID_MASK;
                su_ppdu = 0;
                txbuf->length_user0 = sig_b_0 & 0xffff;
                txbuf->length_user1 = sig_b_1 & 0xffff;
                txbuf->length_user2 = sig_b_2 & 0xffff;

                if ((groupid == 0) || (groupid == 63)) {
                    su_ppdu = 1;
                    nsts_su = (sig_a_1 >> PHY_NSTS_SU_SHIFT) & PHY_NSTS_MASK;
                    if (stbc)
                        nss = nsts_su >> 2;
                    else
                       nss = nsts_su;
                    ++nss;
                    mcs = (sig_a_2 >> PHY_RATE_MCS_SHIFT) & PHY_RATE_MCS_MASK_VHT;
                    txbf = (sig_a_2 >> PHY_TXBF_SHIFT) & PHY_TXBF_MASK;
                    partial_aid = (sig_a_1 >> PHY_PARTIAL_AID_SHIFT) & PHY_PARTIAL_AID_MASK;
                } else {
                    txbf = (sig_a_2 >> PHY_TXBF_SHIFT) & PHY_TXBF_MASK;
                    ldpc_u0 = (sig_a_2 >> PHY_LDPC_SHIFT_VHT_U0) & PHY_LDPC_MASK;
                    ldpc_u1 = (sig_a_2 >> PHY_LDPC_SHIFT_VHT_U1) & PHY_LDPC_MASK;
                    ldpc_u2 = (sig_a_2 >> PHY_LDPC_SHIFT_VHT_U2) & PHY_LDPC_MASK;
                    ldpc_u3 = (sig_a_2 >> PHY_LDPC_SHIFT_VHT_U3) & PHY_LDPC_MASK;
                    nsts_u0 = (sig_a_1 >> PHY_NSTS_MU_SHIFT_U0) & PHY_NSTS_MASK;
                    nss = (sig_a_1 >> PHY_NSTS_MU_SHIFT_U1) & PHY_NSTS_MASK;
                    nsts_u2 = (sig_a_1 >> PHY_NSTS_MU_SHIFT_U2) & PHY_NSTS_MASK;
                    nsts_u3 = (sig_a_1 >> PHY_NSTS_MU_SHIFT_U3) & PHY_NSTS_MASK;

                    switch (bw) {
                        case 20:
                            mcs = (sig_b_0 >> PHY_RATE_MCS_SHIFT_VHT_MU_BW20) &
                                                          PHY_RATE_MCS_MASK_VHT;
                            break;

                        case 40:
                            mcs = (sig_b_0 >> PHY_RATE_MCS_SHIFT_VHT_MU_BW40) &
                                                          PHY_RATE_MCS_MASK_VHT;
                            break;

                        case 80:
                            mcs = (sig_b_0 >> PHY_RATE_MCS_SHIFT_VHT_MU_BW80) &
                                                          PHY_RATE_MCS_MASK_VHT;
                            break;

                        case 160:
                            //TODO:need to add for correct mcs for 160Mhz MU-MIMO
                            break;
                    }
                }
            } else {
                /* HT frame */
                ht = 1;
                bw = PHY_BANDWIDTH_ENCODE_SHIFT <<
                      ((sig_a_1 >> PHY_BANDWIDTH_SHIFT_HT) &
                                        PHY_BANDWIDTH_MASK_HT);
                mcs = sig_a_1  & PHY_RATE_MCS_MASK_HT;
                sgi = (sig_a_2 >> PHY_SGI_SHIFT) & PHY_SGI_MASK;
                ldpc = (sig_a_2 >> PHY_LDPC_SHIFT) & PHY_LDPC_MASK;
                stbc = ((sig_a_2 >> PHY_STBC_SHIFT) & PHY_STBC_MASK) ? 1 : 0;
                nss = (mcs >> PHY_NSS_SHIFT) + 1;
                txbuf->aggregation = sig_a_2 & 0x8;
                txbuf->stbc = sig_a_2 & 0x30;
                txbuf->fec = sig_a_2 & 0x40;
                txbuf->gi = sig_a_2 & 0x80;
                txbuf->extspatial = sig_a_2 & 0xa0;
                txbuf->length = sig_a_1 & 0xffff;
            }
        } else {
            /* not an HT/VHT frame */
            mcs = (l_sig >> PHY_RATE_MCS_SHIFT) & PHY_RATE_MCS_MASK;
            base = (mcs & PHY_RATE_BASE_MASK) ? 9 : 6;
            mcs &= ~PHY_RATE_BASE_MASK;
            mcs = base << (11 - mcs);
            mcs = (mcs > 54) ? 54 : mcs;
            signal = l_sig & (1 << PHY_SIGNALING_SHIFT);
        }
    } else {
        /* AR9888 */
        mcs = siginfo->data_rate;
        nss = siginfo->nss;
        ldpc = siginfo->ldpc;
        bw = txbuf->bw;
        switch (siginfo->preamble_type) {
        case 0:
        case 1:
            /* Legacy Frame */
            base = (mcs & PHY_RATE_BASE_MASK) ? 9 : 6;
            mcs &= ~PHY_RATE_BASE_MASK;
            mcs = base << (11 - mcs);
            mcs = (mcs > 54) ? 54 : mcs;
            break;
        case 2:
            /* HT Frame */
            ht = 1;
            break;
        case 3:
            /* VHT Frame */
            vht = 1;
            break;
        }
    }

    /* prepare space in skb for radiotap header */
    rt_total_len = get_radiotap_total_len(ht, vht);

    /* radiotap version */
    rthdr->it_version = 0;
    rthdr->it_len = rt_total_len;
    it_present_p = &txbuf->rthdr.it_present;
    datap = &txbuf->rthdr_data[0];

    /* init present mask */
    it_present_mask = BIT(IEEE80211_RADIOTAP_FLAGS) | BIT(IEEE80211_RADIOTAP_CHANNEL) | BIT(IEEE80211_RADIOTAP_DBM_ANTSIGNAL);
    /*
     *  values were filled.
     */
    /* TSF bit0 */
    while((datap - (u_int8_t *)rthdr) & 7) {
        //8-byte alignment
        *datap++ = 0;
    }
    put_unaligned_le64(siginfo->tsf, datap);
    it_present_mask |= cpu_to_le32(BIT(IEEE80211_RADIOTAP_TSFT));
    datap += 8;

    /* no fcs, 4-byte FCS stripped */
    if (sgi) {
        *datap |= IEEE80211_RADIOTAP_F_SHORTGI;
    }
    datap++;
    /* rate */
    if (ht || vht) {
        /* non-legacy rates will be parsed later */
        *datap = 0;
    } else {
        it_present_mask |= cpu_to_le32(BIT(IEEE80211_RADIOTAP_RATE));
        /* radiotap rate is in 500 kbps, mcs here is in Mbps */
        *datap = DIV_ROUND_UP(mcs*10, 5);
    }
    datap++;

    /* channel */
    put_unaligned_le16(ic->ic_curchan->ic_freq, datap);
    datap += 2;

    /* antsignal */
    *datap = siginfo->rssi;
    datap++;

    if (ht) {
        it_present_mask |= cpu_to_le32(BIT(IEEE80211_RADIOTAP_MCS));
        radiotap_mcs_details |= IEEE80211_RADIOTAP_MCS_HAVE_STBC | \
                                 IEEE80211_RADIOTAP_MCS_HAVE_MCS | IEEE80211_RADIOTAP_MCS_HAVE_GI | \
                                  IEEE80211_RADIOTAP_MCS_HAVE_BW;
        *datap++ = radiotap_mcs_details;
        if (sgi)
            *datap |= IEEE80211_RADIOTAP_MCS_SGI;
        if (bw == 20)
            *datap |= IEEE80211_RADIOTAP_MCS_BW_20;
        else if (bw == 40)
            *datap |= IEEE80211_RADIOTAP_MCS_BW_40;
        *datap |= stbc << IEEE80211_RADIOTAP_MCS_STBC_SHIFT;
        datap++;
        *datap++ = mcs;
    }

    if (vht) {
        /* u16 known, u8 flags, u8 bandwidth, u8 mcs_nss[4], u8 coding, u8 group_id, u16 partial_aid */
        it_present_mask |= cpu_to_le32(BIT(IEEE80211_RADIOTAP_VHT));
        /* known */
        radiotap_vht_details |= IEEE80211_RADIOTAP_VHT_KNOWN_STBC | \
                                 IEEE80211_RADIOTAP_VHT_KNOWN_GI | IEEE80211_RADIOTAP_VHT_KNOWN_BEAMFORMED | IEEE80211_RADIOTAP_VHT_KNOWN_BANDWIDTH | \
                                  IEEE80211_RADIOTAP_VHT_KNOWN_GROUP_ID | IEEE80211_RADIOTAP_VHT_KNOWN_PARTIAL_AID;

        /* TODO: for 80+80, need to clear IEEE80211_RADIOTAP_VHT_KNOWN_BANDWIDTH */
        put_unaligned_le16(radiotap_vht_details, datap);
        datap += 2;
        /* flags */
        if(stbc)
            *datap |= IEEE80211_RADIOTAP_VHT_FLAG_STBC;
        if(sgi)
            *datap |= IEEE80211_RADIOTAP_VHT_FLAG_SGI;
        if(txbf)
            *datap |= IEEE80211_RADIOTAP_VHT_FLAG_BEAMFORMED;
        datap++;

        /* bandwidth */
        if(bw == 80)
            *datap = 4;
        else if(bw == 8080)
            /* TOTO: for 80+80 need to fix this */
            *datap = 11;
        else if(bw == 160)
            *datap = 11;
        else if(bw == 40)
            *datap = 1;
        else /* 20 */
            *datap = 0;
        datap++;

        /* mcs_nss */
        if (su_ppdu) {
            /* For SU PPDUs, only the first user will have a nonzero NSS field. */
            *datap = (mcs << 4)|nss;
            datap += 4;
        } else {
            /* MU-PPDU */
            *datap++ = (mcs << 4)|nss;
            *datap++ = (mcs << 4)|nss;
            *datap++ = (mcs << 4)|nss;
            *datap++ = (mcs << 4)|nss;
        }

        /* coding */
        if (su_ppdu) {
            *datap++ = ldpc;
        } else {
            if (ldpc_u0)
                vht_coding_details |= 1;
            if (ldpc_u1)
                vht_coding_details |= 1<<1;
            if (ldpc_u2)
                vht_coding_details |= 1<<2;
            if (ldpc_u3)
                vht_coding_details |= 1<<3;
            *datap++ = vht_coding_details;
        }

        /* group_id  SU_PPDU(0 or 63) else it's MU_PPDU */
        *datap = groupid;
        datap++;

        /* partial_aid */
        put_unaligned_le16(partial_aid, datap);
        datap += 2;
    }

    put_unaligned_le32(it_present_mask, it_present_p);
    return;
}

static A_STATUS
process_AR9888_tx_info(struct ol_txrx_pdev_t *txrx_pdev, void *data,
                       ath_pktlog_hdr_t *pl_hdr)
{
    ol_pktlog_dev_t *pl_dev;
    struct ath_pktlog_tx_capture *tx_capture_buf = NULL;
    struct ath_pktlog_siginfo siginfo;
    /*
     * series       : Whether h/w used series '0' or '1
     * series_start : offset where series_bw starts
     * bw           : Whether h/w used 20,40 or 80 Mhz
     *                0: 20 MHz
     *                1: 40 MHz
     *                2: 80 MHz
     * tpc          : transmit power
     */
    uint8_t total_tries, bitmap, try_status_start, series;
    uint8_t *tx_data, *sbw_start;

    if (!txrx_pdev->htt_pdev->htt_process_tx_metadata) {
        return A_ERROR;
    }

    tx_capture_buf = qdf_mem_malloc(sizeof(*tx_capture_buf));
    if (!tx_capture_buf) {
        qdf_err("%s() Not Enough memory",__func__);
        return A_NO_MEMORY;
    }
    tx_capture_buf->ppdu_id = pl_hdr->type_specific_data &
                               ATH_PKTLOG_TX_STAT_PPDU_ID_MASK;
    pl_dev = txrx_pdev->pl_dev;
    tx_data = (uint8_t *)data + (pl_dev->pktlog_hdr_size);
    siginfo.tsf = *((uint32_t *)tx_data); /*1st 4 bytes in the payload is tsf*/
    tx_capture_buf->mpdu_bitmap_low = *((uint32_t *)(tx_data +
                                          ATH_PKTLOG_TX_STAT_MPDU_BMAP_OFFSET));
    tx_capture_buf->mpdu_bitmap_high = *((uint32_t *)(tx_data +
                                    (ATH_PKTLOG_TX_STAT_MPDU_BMAP_OFFSET + 4)));
    bitmap = *(tx_data + ATH_PKTLOG_TX_STAT_TRIES_OFFSET);
    total_tries = (bitmap & 0x1F);
    tx_capture_buf->frame_ctrl = *((uint16_t *)(tx_data +
                                          ATH_PKTLOG_TX_STAT_FRAME_CTL_OFFSET));
    tx_capture_buf->qos_ctrl = *((uint16_t *)(tx_data +
                                    (ATH_PKTLOG_TX_STAT_FRAME_CTL_OFFSET + 4)));

    try_status_start = *(tx_data + ((4 * total_tries) + 3));
    series = try_status_start & 0x1 ;
    tx_capture_buf->bw = (try_status_start & 0x30) >> 4;

    sbw_start = tx_data + (4 * ATH_TX_PPDU_NUM_DWORDS);
    if (series) {
        /*1 series0 bw structure each of 4 integers for 20,40,80,160*/
        sbw_start += ATH_MAX_SERIES * ATH_SERIES_BW_SIZE;
    }
    /*
     * each series_bw data is of 4 words
     * so, need to use the 0th/1st/2nd
     * series_bw data
     */
    sbw_start +=  (16 * tx_capture_buf->bw);
    siginfo.rssi = ((*((uint16_t *)(sbw_start + 2))) & 0x3F0) >> 4;
    tx_capture_buf->gi = ((*(sbw_start + 3)) & 0x10) >> 4; /* 28th bit */
    siginfo.ldpc = ((*(sbw_start + 3)) & 0x40) >> 6; /* 30th bit */
    tx_capture_buf->stbc = (*(sbw_start + 6)) & 0x01; /* 20th bit */
    siginfo.preamble_type = ((*(sbw_start + 7)) & 0xC0) >> 6; /*30,31 bits */
    siginfo.data_rate = (*(sbw_start + 7)) & 0x0F; /*24th to 27th bits */
    siginfo.nss = ((*(sbw_start + 7)) & 0x30) >> 4; /*28th, 29th bits */
    pktlog_txcap_add_radiotap_header(pl_dev, &siginfo, tx_capture_buf);
    txrx_pdev->htt_pdev->htt_process_tx_metadata(tx_capture_buf);
    return A_OK;
}

static A_STATUS
process_tx_info_2_0(struct ol_pktlog_dev_t *pl_dev, void *data,
                        ath_pktlog_hdr_t *pl_hdr)
{
    struct ath_pktlog_info *pl_info;
    void *pl;
    int beacon = 0;
    qdf_nbuf_t  beacon_buf, beacon_buf_cpy = NULL;
    uint32_t *ppdu_ptr;
    struct ol_ath_softc_net80211 *scn =
        (struct ol_ath_softc_net80211 *)(pl_dev->scn);
    struct ol_txrx_pdev_t *txrx_pdev =
         wlan_pdev_get_dp_handle(scn->sc_pdev);
    /*
     *  Must include to process different types
     *  TX_CTL, TX_STATUS, TX_MSDU_ID, TX_FRM_HDR
     */
    pl_info = pl_dev->pl_info;


    if(pl_hdr->log_type == PKTLOG_TYPE_TX_FRM_HDR) {
        /* Valid only for the TX CTL */
        process_ieee_hdr(data + pl_dev->pktlog_hdr_size);
    }

    if (pl_hdr->log_type == PKTLOG_TYPE_TX_VIRT_ADDR) {

        if (txrx_pdev->is_ar900b) {
            A_UINT32 desc_id = (A_UINT32) *((A_UINT32 *)(data + pl_dev->pktlog_hdr_size));
            A_UINT32 vdev_id = desc_id;
            /* if the pkt log msg is for the bcn frame the vdev id is piggybacked in desc_id
             * and the MSB of the desc ID would be set to FF
             */
            #define BCN_DESC_ID 0xFF
            if ((desc_id >> 24) == BCN_DESC_ID) {
                vdev_id &= 0x00FFFFFF;
                        pl = ol_ath_get_bcn_header(txrx_pdev->ctrl_pdev, vdev_id);
                if (pl)
                    process_ieee_hdr(pl);
            } else {
                /*TODO*/
                /* get the hdr conetnt for mgmt frames from Tx mgmt desc pool*/
            }
        } else {
               A_UINT32 virt_addr = (A_UINT32) *((A_UINT32 *)(data + pl_dev->pktlog_hdr_size));
               /* checking the virtual address recevied from FW is valid */
               if (virt_addr > 0) {
                   wbuf_t wbuf = (wbuf_t) (uintptr_t)virt_addr;
                   process_ieee_hdr(wbuf_header(wbuf));
               }
        }
    }

    if(pl_hdr->log_type == PKTLOG_TYPE_TX_CTRL) {
        size_t log_size = 0;

        if (lmac_is_target_ar900b(scn->soc->psoc_obj)) {
            void *txdesc_hdr_ctl = NULL;

            log_size = sizeof(frm_hdr) + pl_hdr->size;
            txdesc_hdr_ctl = (void *)
                pktlog_getbuf(pl_dev, pl_info, log_size, pl_hdr);
            if (txdesc_hdr_ctl == NULL) {
                return A_NO_MEMORY;
            }
            qdf_assert(txdesc_hdr_ctl);

            qdf_assert(pl_hdr->size < PKTLOG_MAX_TXCTL_WORDS_AR900B * sizeof(u_int32_t));
            qdf_mem_copy(txdesc_hdr_ctl, &frm_hdr, sizeof(frm_hdr));
            qdf_mem_copy((char *)txdesc_hdr_ctl + sizeof(frm_hdr),
                        ((void *)data + pl_dev->pktlog_hdr_size),
                         pl_hdr->size);
        } else {
            struct ath_pktlog_txctl_ar9888 txctl_log;
            log_size = sizeof(txctl_log.priv);
            txctl_log.txdesc_hdr_ctl = (void *)
                pktlog_getbuf(pl_dev, pl_info, log_size, pl_hdr);

            if(!txctl_log.txdesc_hdr_ctl) {
                return A_ERROR;
            }
            /*
             *  frm hdr is currently Valid only for local frames
             *  Add capability to include the fmr hdr for remote frames
             */
            txctl_log.priv.frm_hdr = frm_hdr;
            qdf_assert(txctl_log.priv.txdesc_ctl);
            qdf_mem_copy((void *)&txctl_log.priv.txdesc_ctl,
                    ((void *)data + pl_dev->pktlog_hdr_size),
                    pl_hdr->size);
            qdf_mem_copy(txctl_log.txdesc_hdr_ctl,
                    &txctl_log.priv, sizeof(txctl_log.priv));
        }
    }

    if (pl_hdr->log_type == PKTLOG_TYPE_TX_STAT) {
        if (pl_info->tx_capture_enabled &&
             txrx_pdev->htt_pdev->htt_process_tx_metadata) {
            struct ath_pktlog_tx_capture *tx_capture_buf = NULL;
            struct ath_pktlog_siginfo siginfo;
            uint8_t *tx_data = NULL;
            uint32_t vdev_id;
            tx_capture_buf = qdf_mem_malloc(sizeof(*tx_capture_buf));
            if (!tx_capture_buf) {
                return A_NO_MEMORY;
            }

            /*
             * The pkt carrying beacon related meta-info carries special
             * marking for identification: the 1 MSB byte is set to 0xFF
             * next MSB byte carries the vdev_id
             * the 2 LSB bytes carry the ppdu_id
             */
            if (((*((uint32_t *)data + ATH_PKTLOG_TX_IND_BEACON_OFFSET)) &
                                                  ATH_PKTLOG_TX_IND_BEACON_MASK)
                                                    == ATH_PKTLOG_TX_IND_BEACON)
                beacon = 1;

            vdev_id = ((*((uint32_t *)data + ATH_PKTLOG_TX_VDEVID_OFFSET)) &
                       ATH_PKTLOG_TX_VDEVID_MASK) >> ATH_PKTLOG_TX_VDEVID_SHIFT;
            tx_data = ((uint8_t *)data + pl_dev->pktlog_hdr_size);

            if (beacon) {
                beacon_buf = ol_ath_get_bcn_buffer(txrx_pdev->ctrl_pdev,
                                                                       vdev_id);
                if (beacon_buf)
	                beacon_buf_cpy = qdf_nbuf_copy(beacon_buf);

                if (beacon_buf_cpy) {
                    ppdu_ptr = (uint32_t*) qdf_nbuf_push_head(beacon_buf_cpy,
                                                                   sizeof(int));
                    *(ppdu_ptr) = tx_capture_buf->ppdu_id;
                }
            }

            tx_capture_buf->ppdu_id = *((uint16_t *)(tx_data +
                                            ATH_PKTLOG_TX_STAT_PPDU_ID_OFFSET));
            tx_capture_buf->qos_ctrl = *((uint16_t *)(tx_data +
                                           ATH_PKTLOG_TX_STAT_QOS_CTRL_OFFSET));
            tx_capture_buf->frame_ctrl = *((uint16_t *)(tx_data +
                                           ATH_PKTLOG_TX_STAT_FRM_CTRL_OFFSET));
            tx_capture_buf->mpdu_bitmap_low = *((uint16_t *)(tx_data +
                                           ATH_PKTLOG_TX_STAT_MPDU_LOW_OFFSET));
            tx_capture_buf->mpdu_bitmap_high = *((uint16_t *)(tx_data +
                                          ATH_PKTLOG_TX_STAT_MPDU_HIGH_OFFSET));

            siginfo.tsf = *((uint32_t *)(tx_data +
                                          ATH_PKTLOG_TX_STAT_TSF_OFFSET));
            siginfo.rssi = *((uint8_t *)(tx_data +
                                          ATH_PKTLOG_TX_STAT_RSSI_OFFSET));
            siginfo.l_sig = *((uint32_t *)(tx_data +
                                          ATH_PKTLOG_TX_STAT_LSIG_OFFSET));
            siginfo.ht_vht_sig1 = *((uint32_t *)(tx_data +
                                          ATH_PKTLOG_TX_STAT_SIG1_OFFSET));
            siginfo.ht_vht_sig2 = *((uint32_t *)(tx_data +
                                          ATH_PKTLOG_TX_STAT_SIG2_OFFSET));
            siginfo.vht_sig_b_0 = *((uint32_t *)(tx_data +
                                          ATH_PKTLOG_TX_STAT_SIGB0_OFFSET));
            siginfo.vht_sig_b_1 = *((uint32_t *)(tx_data +
                                          ATH_PKTLOG_TX_STAT_SIGB1_OFFSET));
            siginfo.vht_sig_b_2 = *((uint32_t *)(tx_data +
                                          ATH_PKTLOG_TX_STAT_SIGB2_OFFSET));

            pktlog_txcap_add_radiotap_header(pl_dev, &siginfo, tx_capture_buf);
            txrx_pdev->htt_pdev->htt_process_tx_metadata(tx_capture_buf);
        } else {
            struct ath_pktlog_tx_status txstat_log;
            size_t log_size = pl_hdr->size;
            txstat_log.ds_status = (void *)
            pktlog_getbuf(pl_dev, pl_info, log_size, pl_hdr);
            if (txstat_log.ds_status == NULL) {
                return A_NO_MEMORY;
            }
            qdf_assert(txstat_log.ds_status);
            qdf_mem_copy(txstat_log.ds_status,
                           ((void *)data + pl_dev->pktlog_hdr_size),
                                                        pl_hdr->size);
        }
    }
    if (pl_hdr->log_type == PKTLOG_TYPE_TX_MSDU_ID) {

        (void)pl_dev->process_tx_msdu_info((ol_txrx_pdev_handle)txrx_pdev, data, pl_hdr);
    }
    return A_OK;
}

#if PEER_FLOW_CONTROL
/* Do a plain dump of pktlog data */
static A_STATUS
process_opaque_pktlog(struct ol_pktlog_dev_t *pl_dev, void *data,
                      ath_pktlog_hdr_t *pl_hdr)
{
    size_t log_size;
    struct ath_pktlog_info *pl_info;
    void *buf;

    log_size = pl_hdr->size;
    pl_info = pl_dev->pl_info;

    buf = (void *)pktlog_getbuf(pl_dev, pl_info, log_size, pl_hdr);
    if (!buf) {
        return A_ERROR;
    }

    qdf_mem_copy(buf, ((char *)data + sizeof(ath_pktlog_hdr_t)), pl_hdr->size);

    return A_OK;
}
#endif

static A_STATUS
process_rx_info(struct ol_pktlog_dev_t *pl_dev, void *data,
                        ath_pktlog_hdr_t *pl_hdr)
{
    struct ath_pktlog_info *pl_info;
    struct ath_pktlog_rx_info rxstat_log;
    size_t log_size;

    pl_info = pl_dev->pl_info;
    log_size = pl_hdr->size;
    rxstat_log.rx_desc = (void *)
        pktlog_getbuf(pl_dev, pl_info, log_size, pl_hdr);
    if (rxstat_log.rx_desc == NULL) {
        return A_NO_MEMORY;
    }
    qdf_mem_copy(rxstat_log.rx_desc,
                    (void *)data + pl_dev->pktlog_hdr_size,
                    pl_hdr->size);

    return A_OK;
}

static A_STATUS
process_rate_find(struct ol_pktlog_dev_t *pl_dev, void *data,
                          ath_pktlog_hdr_t *pl_hdr)
{
    struct ath_pktlog_info *pl_info;
    size_t log_size;
    struct ath_pktlog_rc_find rcf_log;

    pl_info           = pl_dev->pl_info;
    log_size          = pl_hdr->size;
    rcf_log.rcFind    = (void *) pktlog_getbuf(pl_dev, pl_info,
            log_size, pl_hdr);
    if (rcf_log.rcFind == NULL ){
        return A_NO_MEMORY;
    }
    qdf_mem_copy(rcf_log.rcFind,
            ((char *)data + pl_dev->pktlog_hdr_size), pl_hdr->size);

    return A_OK;
}

static A_STATUS
process_rate_update(struct ol_pktlog_dev_t *pl_dev, void *data,
                            ath_pktlog_hdr_t *pl_hdr)
{
    size_t log_size;
    struct ath_pktlog_info *pl_info;
    struct ath_pktlog_rc_update rcu_log;

    log_size = pl_hdr->size;
    pl_info = pl_dev->pl_info;

    /*
     * Will be uncommented when the rate control update
     * for pktlog is implemented in the firmware.
     * Currently derived from the TX PPDU status
     */
    rcu_log.txRateCtrl = (void *)
        pktlog_getbuf(pl_dev, pl_info, log_size, pl_hdr);
    if (rcu_log.txRateCtrl == NULL) {
        return A_NO_MEMORY;
    }
    qdf_mem_copy(rcu_log.txRateCtrl,
            ((char *)data + pl_dev->pktlog_hdr_size), pl_hdr->size);

    return A_OK;
}

static A_STATUS
process_tgt_selfgen_pkts(struct ol_txrx_pdev_t *txrx_pdev,
                         void *pktlog_buf, u_int16_t plhdr_size,
                         size_t hdr_length)
{
    /*
     * strip pktlog_hdr and send this buf for further processing.
     */
    if (txrx_pdev->htt_pdev->htt_process_nondata_tx_frames) {
        qdf_nbuf_t pktbuf = NULL;
        uint8_t *pktdata = NULL;
        uint8_t *data_start = NULL;
        int pkt_size = plhdr_size + 2; /* ppdu_id of 2 bytes before payload */
        pktbuf = qdf_nbuf_alloc(NULL, pkt_size, 0, 0, false);

        if (!pktbuf) {
            return A_NO_MEMORY;
        }

        qdf_nbuf_set_pktlen(pktbuf, pkt_size);
        pktdata = (uint8_t *) qdf_nbuf_data(pktbuf);
        data_start = (uint8_t *)pktlog_buf + (hdr_length - 2);
        qdf_mem_copy(pktdata, data_start, pkt_size);
        txrx_pdev->htt_pdev->htt_process_nondata_tx_frames(pktbuf);
        return A_OK;
    }
    return A_ERROR;
}

A_STATUS
process_offload_pktlog_2_0(struct ol_pktlog_dev_t *pl_dev, void *data)
{
    struct ol_ath_softc_net80211 *scn;
    ath_pktlog_hdr_t pl_hdr;
    uint32_t *pl_tgt_hdr, target_type;
    A_STATUS rc = A_OK;
    struct ol_txrx_pdev_t *txrx_pdev;

    if (!pl_dev) {
        qdf_err("Invalid context in %s", __func__);
        return A_ERROR;
    }
    if (!data) {
        qdf_err("Invalid data in %s", __func__);
        return A_ERROR;
    }
    scn = (struct ol_ath_softc_net80211 *)(pl_dev->scn);
    pl_tgt_hdr = (uint32_t *)data;
    txrx_pdev =  (struct ol_txrx_pdev_t *)wlan_pdev_get_dp_handle(scn->sc_pdev);
    target_type = lmac_get_tgt_type(scn->soc->psoc_obj);

    /*
     * Makes the short words (16 bits) portable b/w little endian
     * and big endian
     */
    pl_hdr.flags = (*(pl_tgt_hdr + ATH_PKTLOG_HDR_FLAGS_OFFSET) &
            ATH_PKTLOG_HDR_FLAGS_MASK) >>
        ATH_PKTLOG_HDR_FLAGS_SHIFT;
    pl_hdr.missed_cnt =  (*(pl_tgt_hdr + ATH_PKTLOG_HDR_MISSED_CNT_OFFSET) &
            ATH_PKTLOG_HDR_MISSED_CNT_MASK) >>
        ATH_PKTLOG_HDR_MISSED_CNT_SHIFT;
    pl_hdr.log_type =  (*(pl_tgt_hdr + ATH_PKTLOG_HDR_LOG_TYPE_OFFSET) &
            ATH_PKTLOG_HDR_LOG_TYPE_MASK) >>
        ATH_PKTLOG_HDR_LOG_TYPE_SHIFT;
    pl_hdr.size =  (*(pl_tgt_hdr + ATH_PKTLOG_HDR_SIZE_OFFSET) &
            ATH_PKTLOG_HDR_SIZE_MASK) >>
        ATH_PKTLOG_HDR_SIZE_SHIFT;
    pl_hdr.timestamp = *(pl_tgt_hdr + ATH_PKTLOG_HDR_TIMESTAMP_OFFSET);

    if (lmac_is_target_ar900b(scn->soc->psoc_obj)) {
        pl_hdr.type_specific_data = *(pl_tgt_hdr + ATH_PKTLOG_HDR_TYPE_SPECIFIC_DATA_OFFSET);
    } else {
        pl_hdr.type_specific_data = 0;
    }

    /*
     * For AR9888, the events from id 10 onwards have different meaning
     * compared to AR9000. Hence, these need to be handled separately.
     */
    if (target_type == TARGET_TYPE_AR9888) {
        switch (pl_hdr.log_type) {
            case PKTLOG_TYPE_TX_STATS_COMBINED:
                rc = process_AR9888_tx_info(txrx_pdev, data, &pl_hdr);
                break;
            case PKTLOG_TYPE_TX_LOCAL:
                rc = process_tgt_selfgen_pkts(txrx_pdev, data, pl_hdr.size,
                                                  pl_dev->pktlog_hdr_size);
                break;
            default:
               break;
        }
    }

    /*
     * Makes the short words (16 bits) portable b/w little endian
     * and big endian
     */
    switch(pl_hdr.log_type) {
        case PKTLOG_TYPE_TX_CTRL:
        case PKTLOG_TYPE_TX_STAT:
        case PKTLOG_TYPE_TX_MSDU_ID:
        case PKTLOG_TYPE_TX_FRM_HDR:
        case PKTLOG_TYPE_TX_VIRT_ADDR:
            rc = process_tx_info_2_0(pl_dev, data, &pl_hdr);
            break;

        case PKTLOG_TYPE_RC_FIND:
            rc = process_rate_find(pl_dev, data, &pl_hdr);
            break;

        case PKTLOG_TYPE_RC_UPDATE:
            rc = process_rate_update(pl_dev, data, &pl_hdr);
            break;

        case PKTLOG_TYPE_RX_STAT:
            rc = process_rx_info(pl_dev, data, &pl_hdr);
            break;
        case PKTLOG_TYPE_TX_FW_GENERATED1:
        case PKTLOG_TYPE_TX_FW_GENERATED2:
            rc = process_tgt_selfgen_pkts(txrx_pdev, data, pl_hdr.size,
                                              pl_dev->pktlog_hdr_size);
            break;
        default:
#if PEER_FLOW_CONTROL
            rc = process_opaque_pktlog(pl_dev, data, &pl_hdr);
#else
            rc = A_ERROR;
#endif
            break;
    }
    return rc;
}


#ifdef CONFIG_AR900B_SUPPORT
static QDF_STATUS
pktlog_hif_callback(void *context, qdf_nbuf_t netbuf, u_int8_t pipeID)
{
    uint8_t       *netdata;
    uint32_t      netlen;
    A_STATUS rc = A_OK;

    uint8_t mac_id = 0;
    struct ol_ath_soc_softc *soc = (struct ol_ath_soc_softc *)context;
    struct ol_ath_softc_net80211 *scn;
    struct ol_pktlog_dev_t *pl_dev;
    struct wlan_objmgr_pdev *pdev;

    ASSERT(pipeID == CE_PKTLOG_PIPE);

    netdata = qdf_nbuf_data(netbuf);
    netlen = qdf_nbuf_len(netbuf);

    pdev = wlan_objmgr_get_pdev_by_id(soc->psoc_obj, mac_id, WLAN_MLME_SB_ID);
    if (!pdev) {
      qdf_print("%s: pdev (obj id %d) is NULL", __func__, mac_id);
      return QDF_STATUS_E_FAILURE;
    }

    scn = lmac_get_pdev_feature_ptr(pdev);
    if (!scn) {
      qdf_print("%s: scn(obj id %d) is NULL", __func__, mac_id);
      wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
      return QDF_STATUS_E_FAILURE;
    }

    pl_dev = scn->pl_dev;

    if ( pl_dev->tgt_pktlog_enabled == true ) {
        rc = process_offload_pktlog_2_0(pl_dev, netdata);
    }
    if (rc != A_CONSUMED) {
        qdf_nbuf_free(netbuf);
    }
    wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);

    return rc;
}
#endif


A_STATUS
process_rx_cbf_remote(void *context, qdf_nbuf_t amsdu)
{
    struct ol_pktlog_dev_t *pl_dev = (struct ol_pktlog_dev_t *)context;
    struct ol_ath_softc_net80211 *scn =
        (struct ol_ath_softc_net80211 *)(pl_dev->scn);
    struct ol_txrx_pdev_t *pdev =
        (struct ol_txrx_pdev_t *)wlan_pdev_get_dp_handle(scn->sc_pdev);
    struct ath_pktlog_info *pl_info;
    void *rx_desc;
    ath_pktlog_hdr_t pl_hdr;
    size_t log_size;
    struct ath_pktlog_cbf_info rxcbf_log;
    qdf_nbuf_t msdu;
    struct ieee80211_frame *wh;
    A_UINT8 type, subtype, action_category;
    struct ieee80211_action *ia_header = NULL;
    struct bb_tlv_pkt_hdr *bb_hdr = NULL;
    struct ath_pktlog_cbf_submix_info  cbf_mix_info;
    int    msdu_chained = 0;
    A_BOOL fragNend = true;
    int    msdu_len = 0;
    uint32_t rx_desc_size = 0;

    if (!amsdu) {
        qdf_err("Invalid data in %s", __func__);
        return A_ERROR;
    }

    qdf_mem_set(&pl_hdr, sizeof(pl_hdr), 0);
    qdf_mem_set(&cbf_mix_info,sizeof(cbf_mix_info),0);
    pl_info = pl_dev->pl_info;
    msdu = amsdu;

    while (msdu) {

#if QCA_PARTNER_DIRECTLINK_RX
        /*
         * For Direct Link RX, neeed to get rx descriptor by partner API.
         */
        if (CE_is_directlink(((struct ol_txrx_pdev_t *) pdev)->ce_tx_hdl)) {
            rx_desc = htt_rx_desc_partner(msdu);
        } else
#endif /* QCA_PARTNER_DIRECTLINK_RX */
    {
        rx_desc = htt_rx_desc(msdu);
    } /* QCA_PARTNER_DIRECTLINK_RX */

        /* Check PHY data type */
        if (pdev->htt_pdev->ar_rx_ops->msdu_desc_phy_data_type(rx_desc)) {
           /* Handle Implicit CV*/
           bb_hdr    = (struct bb_tlv_pkt_hdr *)qdf_nbuf_data(msdu);
           if ((bb_hdr->sig == PKTLOG_PHY_BB_SIGNATURE) &&
               (bb_hdr->tag == PKTLOG_RX_IMPLICIT_CV_TRANSFER ||
                bb_hdr->tag == PKTLOG_BEAMFORM_DEBUG_H)) {
               ia_header = (struct ieee80211_action *)(&bb_hdr[1]);
               log_size = qdf_nbuf_len(msdu) - sizeof(struct bb_tlv_pkt_hdr);
               cbf_mix_info.sub_type = PKTLOG_RX_IMPLICIT_CV_TRANSFER;
           } else {
               goto ChkNextMsdu;
           }
        } else {
            /* Handle Explicit CV*/
            wh = (struct ieee80211_frame *)qdf_nbuf_data(msdu);
            type    = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
            subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;
            action_category = ((struct ieee80211_action *)(&wh[1]))->ia_category;
            if ( (type == IEEE80211_FC0_TYPE_MGT ) &&
                  (subtype == IEEE80211_FCO_SUBTYPE_ACTION_NO_ACK) &&
                 (action_category == IEEE80211_ACTION_CAT_VHT)) {
               ia_header   = (struct ieee80211_action *)(&wh[1]);
               log_size = qdf_nbuf_len(msdu) - sizeof(struct ieee80211_frame);
               cbf_mix_info.sub_type = PKTLOG_RX_EXPLICIT_CV_TRANSFER;
           } else {
               goto ChkNextMsdu;
           }

        }

        msdu_len = pdev->htt_pdev->ar_rx_ops->msdu_desc_msdu_length(rx_desc);
        msdu_chained = pdev->htt_pdev->ar_rx_ops->msdu_desc_msdu_chained(rx_desc);

        cbf_mix_info.frag_flag = 0;

        do{
             log_size += sizeof(struct ath_pktlog_cbf_submix_info);
             /*
              * Construct the pktlog header pl_hdr
              * Because desc is DMA'd to the host memory
              */
             pl_hdr.flags = (1 << PKTLOG_FLG_FRM_TYPE_CBF_S);
             pl_hdr.missed_cnt = 0;
             pl_hdr.log_type = PKTLOG_TYPE_RX_CBF;
             pl_hdr.size += log_size;
             pl_hdr.timestamp =  pdev->htt_pdev->ar_rx_ops->msdu_desc_tsf_timestamp(rx_desc);

             rxcbf_log.cbf = (void *)
                   pktlog_getbuf(pl_dev, pl_info, log_size, &pl_hdr);
             if (rxcbf_log.cbf == NULL) {
                 return A_ERROR;
             }
             qdf_mem_copy(rxcbf_log.cbf, (void *)&cbf_mix_info, sizeof(struct ath_pktlog_cbf_submix_info));

             rxcbf_log.cbf = (void *)rxcbf_log.cbf + sizeof(struct ath_pktlog_cbf_submix_info);

             qdf_mem_copy(rxcbf_log.cbf, (void *)ia_header, log_size - sizeof(struct ath_pktlog_cbf_submix_info));
             if ((msdu_chained != 0) && (qdf_nbuf_next(msdu) != NULL))
             {
                rx_desc_size = pdev->htt_pdev->ar_rx_ops->sizeof_rx_desc();
                msdu_len -= qdf_nbuf_len(msdu);
                msdu = qdf_nbuf_next(msdu);
                msdu_chained--;
                if(msdu_chained == 0)
                {
                    if (((unsigned)msdu_len) > ((unsigned)(HTT_RX_BUF_SIZE - rx_desc_size)))
                    {
                        msdu_len = (HTT_RX_BUF_SIZE - rx_desc_size);
                    }
                    log_size = msdu_len;
                }
                qdf_mem_set(&pl_hdr,sizeof(pl_hdr),0);
                cbf_mix_info.frag_flag = 1;
                ia_header = (struct ieee80211_action *)qdf_nbuf_data(msdu);
                fragNend = true;
             }else{
                fragNend = false;
             }
        }while(fragNend);

ChkNextMsdu:
        msdu = qdf_nbuf_next(msdu);
    }
    return A_OK;
}


A_STATUS
process_rx_info_remote(struct ol_pktlog_dev_t *pl_dev, qdf_nbuf_t amsdu)
{
    struct ol_ath_softc_net80211 *scn;
    struct ath_pktlog_info *pl_info;
    struct htt_host_rx_desc_base *rx_desc;
    ath_pktlog_hdr_t pl_hdr;
    void *rx_desc_ptr;
    size_t log_size;
    qdf_nbuf_t msdu;
    struct ol_txrx_pdev_t *pdev;
    uint32_t rx_desc_size = 0;

    if (!pl_dev) {
        qdf_err("Invalid context in %s", __func__);
        return A_ERROR;
    }

    if (!amsdu) {
        qdf_err("Invalid data in %s", __func__);
        return A_ERROR;
    }

    scn = (struct ol_ath_softc_net80211 *)(pl_dev->scn);
    pdev = (struct ol_txrx_pdev_t *)wlan_pdev_get_dp_handle(scn->sc_pdev);

    if (!pdev) {
        qdf_err("Invalid pdev in %s", __func__);
        return A_ERROR;
    }

    pl_info = pl_dev->pl_info;
    rx_desc_size = pdev->htt_pdev->ar_rx_ops->sizeof_rx_desc();

    /* Insert VHT ACTION group ID information*/
    if(((struct ol_txrx_pdev_t *) pdev)->gid_flag)
    {
        pl_hdr.flags = (1 << PKTLOG_FLG_FRM_TYPE_REMOTE_S);
        pl_hdr.missed_cnt = 0;
        pl_hdr.log_type = PKTLOG_TYPE_GRPID;
        pl_hdr.size = sizeof(((struct ol_txrx_pdev_t *) pdev)->gid_mgmt.member_status) +
                      sizeof(((struct ol_txrx_pdev_t *) pdev)->gid_mgmt.user_position);
        pl_hdr.timestamp = 0;
#ifdef CONFIG_AR900B_SUPPORT
        pl_hdr.type_specific_data = 0;
#endif
        log_size = pl_hdr.size;
        rx_desc_ptr = (void *)pktlog_getbuf(pl_dev, pl_info, log_size, &pl_hdr);
        if (rx_desc_ptr == NULL) {
            return A_ERROR;
        }
        qdf_mem_copy(rx_desc_ptr,(u_int8_t *)&(((struct ol_txrx_pdev_t *) pdev)->gid_mgmt.member_status),
                                            sizeof(((struct ol_txrx_pdev_t *) pdev)->gid_mgmt.member_status));

        rx_desc_ptr += sizeof(((struct ol_txrx_pdev_t *) pdev)->gid_mgmt.member_status);
        qdf_mem_copy(rx_desc_ptr,(u_int8_t *)&(((struct ol_txrx_pdev_t *) pdev)->gid_mgmt.user_position),
                     sizeof((((struct ol_txrx_pdev_t *) pdev)->gid_mgmt.user_position)));
    }
    msdu = amsdu;
    while (msdu) {
        /* old code:

        rx_desc = (struct htt_host_rx_desc_base *)(
                            qdf_nbuf_data(msdu)) - 1;

           replaced by following line, it should work for Peregrine
           as well, but not verified.
        */
#if QCA_PARTNER_DIRECTLINK_RX
        /*
         * For Direct Link RX, neeed to get rx descriptor by partner API.
         */
        if (CE_is_directlink(((struct ol_txrx_pdev_t *) pdev)->ce_tx_hdl)) {
            rx_desc = htt_rx_desc_partner(msdu);
        } else
#endif /* QCA_PARTNER_DIRECTLINK_RX */
    {
        rx_desc = htt_rx_desc(msdu);
    } /* QCA_PARTNER_DIRECTLINK_RX */

        log_size = rx_desc_size -
                   pdev->htt_pdev->ar_rx_ops->fw_rx_desc_size();

        /*
         * Construct the pktlog header pl_hdr
         * Because desc is DMA'd to the host memory
         */
        pl_hdr.flags = (1 << PKTLOG_FLG_FRM_TYPE_REMOTE_S);
        pl_hdr.missed_cnt = 0;
        pl_hdr.log_type = PKTLOG_TYPE_RX_STAT;
        pl_hdr.size = log_size;
        pl_hdr.timestamp =  pdev->htt_pdev->ar_rx_ops->msdu_desc_tsf_timestamp(rx_desc);

#ifdef CONFIG_AR900B_SUPPORT
        pl_hdr.type_specific_data = 0;
#endif

        rx_desc_ptr = (void *)
            pktlog_getbuf(pl_dev, pl_info, log_size, &pl_hdr);
        if (rx_desc_ptr == NULL) {
            return A_NO_MEMORY;
        }

        qdf_mem_copy(rx_desc_ptr, (void *)rx_desc +
                pdev->htt_pdev->ar_rx_ops->fw_rx_desc_size(),
                pl_hdr.size);

        msdu = qdf_nbuf_next(msdu);
    }
    return A_OK;
}


void
pktlog_callback_2_0(void *context, enum WDI_EVENT event, void *log_data,
    u_int16_t peer_id, enum htt_rx_status status)
{
    switch(event) {
        case WDI_EVENT_RX_DESC_REMOTE:
        {
             /*
              * process RX message for remote frames
              */
            if (process_rx_info_remote(context, (qdf_nbuf_t)log_data)) {
                AR_DEBUG_PRINTF(ATH_DEBUG_TRC, (" Unable to process RX info \n"));
                return;
            }
           break;
        }
        case WDI_EVENT_RX_CBF_REMOTE:
        {
             /*
              * process RX message for remote frames
              */
            if (process_rx_cbf_remote(context, (qdf_nbuf_t)log_data)) {
                AR_DEBUG_PRINTF(ATH_DEBUG_TRC, (" Unable to process RX cbf \n"));
                return;
            }
           break;
        }
        case WDI_EVENT_OFFLOAD_ALL:
        {
            /*
             * process all offload pktlog
             */
            if (process_offload_pktlog_2_0(context, log_data)) {
                AR_DEBUG_PRINTF(ATH_DEBUG_TRC, (" Unable to process offload pktlog \n"));
                return;
            }
            break;
        }

        default:
            break;
    }
}

void
pktlog_init_2_0(struct ol_ath_softc_net80211 *scn)
{
    struct ol_pktlog_dev_t *pl_dev = NULL;
    uint32_t target_type;

    if ((scn == NULL) || (scn->pl_dev == NULL)) {
        return;
    }

    target_type = lmac_get_tgt_type(scn->soc->psoc_obj);
    pl_dev = scn->pl_dev;

    if (pl_dev) {
        pl_dev->target_type = target_type;
        pl_dev->PKTLOG_RX_REMOTE_SUBSCRIBER.callback = pktlog_callback_2_0;
        pl_dev->PKTLOG_RX_REMOTE_SUBSCRIBER.context = pl_dev;
        pl_dev->PKTLOG_RX_CBF_SUBSCRIBER.callback = pktlog_callback_2_0;
        pl_dev->PKTLOG_RX_CBF_SUBSCRIBER.context = pl_dev;
        pl_dev->PKTLOG_OFFLOAD_SUBSCRIBER.callback = pktlog_callback_2_0;
        pl_dev->PKTLOG_OFFLOAD_SUBSCRIBER.context = pl_dev;
    }

    if (lmac_is_target_ar900b(scn->soc->psoc_obj)) {
        struct ath_pktlog_msdu_info_ar900b pl_msdu_info;
        pl_dev->pktlog_hdr_size = sizeof(struct ath_pktlog_hdr_ar900b);
        pl_dev->msdu_id_offset = MSDU_ID_INFO_ID_OFFSET_AR900B;
        pl_dev->msdu_info_priv_size =  sizeof(pl_msdu_info.priv);
        pl_dev->process_tx_msdu_info = process_tx_msdu_info_ar900b;
        qdf_info("Initializing Pktlog for AR900B, pktlog_hdr_size = %zd",
                                            pl_dev->pktlog_hdr_size);
    } else if (target_type == TARGET_TYPE_AR9888) {
        struct ath_pktlog_msdu_info_ar9888 pl_msdu_info;
        pl_dev->pktlog_hdr_size = sizeof(struct ath_pktlog_hdr_ar9888);
        pl_dev->msdu_id_offset = MSDU_ID_INFO_ID_OFFSET_AR9888;
        pl_dev->msdu_info_priv_size =  sizeof(pl_msdu_info.priv);
        pl_dev->process_tx_msdu_info = process_tx_msdu_info_ar9888;
        qdf_info("Initializing Pktlog for AR9888, pktlog_hdr_size = %zd",
                                            pl_dev->pktlog_hdr_size);
    } else {
        qdf_warn("WARNING: Pktlog enabled for offload target type %d",
                                                         target_type);
    }
#ifdef CONFIG_AR900B_SUPPORT
    if (lmac_is_target_ar900b(scn->soc->psoc_obj)) {
            /* HIF subscribe */
            if (hif_pktlog_subscribe(scn)!= QDF_STATUS_SUCCESS) {
                qdf_warn("Unable to subscribe to the HIF %s",__func__);
                return;
            }
    }
#endif
}
qdf_export_symbol(pktlog_init_2_0);

#endif /*REMOVE_PKT_LOG*/

