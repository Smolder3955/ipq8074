/*
 * Copyright (c) 2011,2015-2018 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011, 2015-2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
/**
 * @file htt_t2h.c
 * @brief Provide functions to process target->host HTT messages.
 * @details
 *  This file contains functions related to target->host HTT messages.
 *  There are two categories of functions:
 *  1.  A function that receives a HTT message from HTC, and dispatches it
 *      based on the HTT message type.
 *  2.  functions that provide the info elements from specific HTT messages.
 */

#include <htc_api.h>         /* HTC_PACKET */
#include <htt.h>             /* HTT_T2H_MSG_TYPE, etc. */
#include <qdf_nbuf.h>        /* qdf_nbuf_t */

#include <ol_htt_rx_api.h>
#include <ol_txrx_htt_api.h> /* htt_tx_status */
#include <ol_ratectrl_11ac_if.h>
#include <ol_txrx_types.h>

#ifdef WLAN_FEATURE_FASTPATH
#include <ol_if_athvar.h>
#include <hif.h>
#endif /* WLAN_FEATURE_FASTPATH */

#include <htt_internal.h>
#include <pktlog_ac_fmt.h>
#include <wdi_event.h>
#include <ol_htt_tx_api.h>
#include <ol_txrx_api.h>
#include <init_deinit_lmac.h>
#if QCA_PARTNER_DIRECTLINK_TX
#define QCA_PARTNER_DIRECTLINK_HTT_T2H 1
#include "ath_carr_pltfrm.h"
#undef QCA_PARTNER_DIRECTLINK_HTT_T2H
#endif /* QCA_PARTNER_DIRECTLINK_TX */
#include <wlan_lmac_if_api.h>
#include <qdf_types.h>

/*--- target->host HTT message dispatch function ----------------------------*/

static u_int8_t *
htt_t2h_mac_addr_deswizzle(u_int8_t *tgt_mac_addr, u_int8_t *buffer)
{
#if BIG_ENDIAN_HOST
    /*
     * The host endianness is opposite of the target endianness.
     * To make u_int32_t elements come out correctly, the target->host
     * upload has swizzled the bytes in each u_int32_t element of the
     * message.
     * For byte-array message fields like the MAC address, this
     * upload swizzling puts the bytes in the wrong order, and needs
     * to be undone.
     */
    buffer[0] = tgt_mac_addr[3];
    buffer[1] = tgt_mac_addr[2];
    buffer[2] = tgt_mac_addr[1];
    buffer[3] = tgt_mac_addr[0];
    buffer[4] = tgt_mac_addr[7];
    buffer[5] = tgt_mac_addr[6];
    return buffer;
#else
    /*
     * The host endianness matches the target endianness -
     * we can use the mac addr directly from the message buffer.
     */
    return tgt_mac_addr;
#endif
}

/* Target to host Msg/event  handler  for low priority messages*/
void
htt_t2h_lp_msg_handler(void *context, qdf_nbuf_t htt_t2h_msg )
{
    struct htt_pdev_t *pdev = (struct htt_pdev_t *) context;
    u_int32_t *msg_word;
    enum htt_t2h_msg_type msg_type;

    msg_word = (u_int32_t *) qdf_nbuf_data(htt_t2h_msg);
    msg_type = HTT_T2H_MSG_TYPE_GET(*msg_word);
    switch (msg_type) {
    case HTT_T2H_MSG_TYPE_VERSION_CONF:
        {
            pdev->tgt_ver.major = HTT_VER_CONF_MAJOR_GET(*msg_word);
            pdev->tgt_ver.minor = HTT_VER_CONF_MINOR_GET(*msg_word);
            qdf_print("target uses HTT version %d.%d; host uses %d.%d",
                pdev->tgt_ver.major, pdev->tgt_ver.minor,
                HTT_CURRENT_VERSION_MAJOR, HTT_CURRENT_VERSION_MINOR);
            if (pdev->tgt_ver.major != HTT_CURRENT_VERSION_MAJOR) {
                qdf_print("*** Incompatible host/target HTT versions!");
            }
            /* abort if the target is incompatible with the host */
            qdf_assert(pdev->tgt_ver.major == HTT_CURRENT_VERSION_MAJOR);
            if (pdev->tgt_ver.minor != HTT_CURRENT_VERSION_MINOR) {
                qdf_print(
                    "*** Warning: host/target HTT versions are different, "
                    "though compatible!");
            }
            break;
        }
    case HTT_T2H_MSG_TYPE_RX_FLUSH:
        {
            u_int16_t peer_id;
            u_int8_t tid;
            int seq_num_start, seq_num_end;
            enum htt_rx_flush_action action;

            peer_id = HTT_RX_FLUSH_PEER_ID_GET(*msg_word);
            tid = HTT_RX_FLUSH_TID_GET(*msg_word);
            seq_num_start = HTT_RX_FLUSH_SEQ_NUM_START_GET(*(msg_word+1));
            seq_num_end = HTT_RX_FLUSH_SEQ_NUM_END_GET(*(msg_word+1));
            action =
                HTT_RX_FLUSH_MPDU_STATUS_GET(*(msg_word+1)) == 1 ?
                htt_rx_flush_release : htt_rx_flush_discard;
            ol_rx_flush_handler(
                pdev->txrx_pdev,
                peer_id, tid,
                seq_num_start,
                seq_num_end,
                action);
            break;
        }
    case  HTT_T2H_MSG_TYPE_RX_FRAG_IND:
        {
            unsigned num_msdu_bytes;
            u_int16_t peer_id;
            u_int8_t tid;

            peer_id = HTT_RX_FRAG_IND_PEER_ID_GET(*msg_word);
            tid = HTT_RX_FRAG_IND_EXT_TID_GET(*msg_word);

            num_msdu_bytes = HTT_RX_IND_FW_RX_DESC_BYTES_GET(*(msg_word + 2));
            /*
             * 1 word for the message header,
             * 1 word to specify the number of MSDU bytes,
             * 1 word for every 4 MSDU bytes (round up),
             * 1 word for the MPDU range header
             */
            pdev->rx_mpdu_range_offset_words = 3 + ((num_msdu_bytes + 3) >> 2);
            pdev->rx_ind_msdu_byte_idx = 0;
            ol_rx_frag_indication_handler(
                pdev->txrx_pdev,
                htt_t2h_msg,
                peer_id,
                tid);
            break;
        }
    case HTT_T2H_MSG_TYPE_RX_ADDBA:
        {
            u_int16_t peer_id;
            u_int8_t tid;
            u_int8_t win_sz;

            peer_id = HTT_RX_ADDBA_PEER_ID_GET(*msg_word);
            tid = HTT_RX_ADDBA_TID_GET(*msg_word);
            win_sz = HTT_RX_ADDBA_WIN_SIZE_GET(*msg_word);
            ol_rx_addba_handler(pdev->txrx_pdev, peer_id, tid, win_sz);
            break;
        }
    case HTT_T2H_MSG_TYPE_RX_DELBA:
        {
            u_int16_t peer_id;
            u_int8_t tid;

            peer_id = HTT_RX_DELBA_PEER_ID_GET(*msg_word);
            tid = HTT_RX_DELBA_TID_GET(*msg_word);
            ol_rx_delba_handler(pdev->txrx_pdev, peer_id, tid);
            break;
        }
    case HTT_T2H_MSG_TYPE_PEER_MAP:
        {
            u_int8_t mac_addr_deswizzle_buf[QDF_MAC_ADDR_SIZE];
            u_int8_t *peer_mac_addr;
            u_int16_t peer_id;
            u_int8_t vdev_id;

            peer_id = HTT_RX_PEER_MAP_PEER_ID_GET(*msg_word);
            vdev_id = HTT_RX_PEER_MAP_VDEV_ID_GET(*msg_word);
            peer_mac_addr = htt_t2h_mac_addr_deswizzle(
                (u_int8_t *) (msg_word+1), &mac_addr_deswizzle_buf[0]);

            if (peer_id > ol_cfg_max_peer_id((ol_soc_handle)pdev->ctrl_psoc)) {
                qdf_print("%s: HTT_T2H_MSG_TYPE_PEER_UNMAP,"
                          "invalid peer_id, %u",
                           __FUNCTION__,
                           peer_id);
                break;
            }
            ol_rx_peer_map_handler(
                (struct ol_txrx_pdev_t *)pdev->txrx_pdev, peer_id, vdev_id, peer_mac_addr);
            break;
        }
    case HTT_T2H_MSG_TYPE_PEER_UNMAP:
        {
            u_int16_t peer_id;
            peer_id = HTT_RX_PEER_UNMAP_PEER_ID_GET(*msg_word);
            if (peer_id > ol_cfg_max_peer_id((ol_soc_handle)pdev->ctrl_psoc)) {
                qdf_print("%s: HTT_T2H_MSG_TYPE_PEER_UNMAP,"
                          "invalid peer_id, %u",
                           __FUNCTION__,
                           peer_id);
                break;
            }

            ol_rx_peer_unmap_handler((struct ol_txrx_pdev_t *)pdev->txrx_pdev, peer_id);
            break;
        }
    case HTT_T2H_MSG_TYPE_SEC_IND:
        {
            u_int16_t peer_id;
            enum htt_sec_type sec_type;
            int is_unicast;

            peer_id = HTT_SEC_IND_PEER_ID_GET(*msg_word);
            sec_type = HTT_SEC_IND_SEC_TYPE_GET(*msg_word);
            is_unicast = HTT_SEC_IND_UNICAST_GET(*msg_word);
            msg_word++; /* point to the first part of the Michael key */
            ol_rx_sec_ind_handler(
                pdev->txrx_pdev, peer_id, sec_type, is_unicast, msg_word, msg_word+2);
            break;
        }
#if TXRX_STATS_LEVEL != TXRX_STATS_LEVEL_OFF
    case HTT_T2H_MSG_TYPE_STATS_CONF:
        {
            u_int64_t cookie;
            u_int8_t *stats_info_list;
            u_int32_t vdev_id;

            vdev_id = HTT_T2H_STATS_CONF_HDR_VAP_ID_GET(*msg_word);
            cookie = *(msg_word + 1);
            cookie |= ((u_int64_t) (*(msg_word + 2))) << 32;

            stats_info_list = (u_int8_t *) (msg_word + 3);
            ol_txrx_fw_stats_handler((struct ol_txrx_pdev_t *)pdev->txrx_pdev, cookie, stats_info_list, vdev_id);
            break;
        }
#endif
#ifndef REMOVE_PKT_LOG
    case HTT_T2H_MSG_TYPE_PKTLOG:
        {
            u_int32_t *pl_hdr;
            pl_hdr = (msg_word + 1);
            wdi_event_handler(WDI_EVENT_OFFLOAD_ALL, (struct ol_txrx_pdev_t *)pdev->txrx_pdev, pl_hdr,
                    HTT_INVALID_PEER, WDI_NO_VAL);
            if (((struct ol_txrx_pdev_t *)pdev)->ap_stats_tx_cal_enable)
                ol_tx_pkt_log_event_handler(pdev, pl_hdr);
            break;
        }
#endif

    default:
        break;
    };
    /* Free the indication buffer */
    qdf_nbuf_free(htt_t2h_msg);



}

/* Generic Target to host Msg/event  handler  for low priority messages
  Low priority message are handler in a different handler called from
  this function . So that the most likely succes path like Rx and
  Tx comp   has little code   foot print
 */
void
htt_t2h_msg_handler(void *context, HTC_PACKET *pkt)
{
    struct htt_pdev_t *pdev = (struct htt_pdev_t *) context;
    qdf_nbuf_t htt_t2h_msg = (qdf_nbuf_t) pkt->pPktContext;
    u_int32_t *msg_word;
    enum htt_t2h_msg_type msg_type;

    /* check for successful message reception */
    if (pkt->Status != QDF_STATUS_SUCCESS) {
        if (pkt->Status != QDF_STATUS_E_CANCELED) {
            pdev->stats.htc_err_cnt++;
        }
        qdf_nbuf_free(htt_t2h_msg);
        return;
    }

    /* confirm alignment */
    HTT_ASSERT3((((unsigned long) qdf_nbuf_data(htt_t2h_msg)) & 0x3) == 0);

    msg_word = (u_int32_t *) qdf_nbuf_data(htt_t2h_msg);
    msg_type = HTT_T2H_MSG_TYPE_GET(*msg_word);
    switch (msg_type) {
    case HTT_T2H_MSG_TYPE_RX_IND:
        {
            unsigned num_mpdu_ranges;
            unsigned num_msdu_bytes;
            u_int16_t peer_id;
            u_int8_t tid;

            peer_id = HTT_RX_IND_PEER_ID_GET(*msg_word);
            tid = HTT_RX_IND_EXT_TID_GET(*msg_word);

            num_msdu_bytes = HTT_RX_IND_FW_RX_DESC_BYTES_GET(
                *(msg_word + 2 + HTT_RX_PPDU_DESC_SIZE32));
            /*
             * 1 word for the message header,
             * HTT_RX_PPDU_DESC_SIZE32 words for the FW rx PPDU desc
             * 1 word to specify the number of MSDU bytes,
             * 1 word for every 4 MSDU bytes (round up),
             * 1 word for the MPDU range header
             */
            pdev->rx_mpdu_range_offset_words =
                (HTT_RX_IND_HDR_BYTES + num_msdu_bytes + 3) >> 2;
            num_mpdu_ranges = HTT_RX_IND_NUM_MPDU_RANGES_GET(*(msg_word + 1));
            pdev->rx_ind_msdu_byte_idx = 0;

            if (pdev->cfg.is_high_latency) {
                /*
                 * TODO: remove copy after stopping reuse skb on HIF layer
                 * because SDIO HIF may reuse skb before upper layer release it
                 */
                ol_rx_indication_handler(
                        pdev->txrx_pdev, htt_t2h_msg, peer_id, tid, num_mpdu_ranges);

                return;
            } else {
                ol_rx_indication_handler(
                        pdev->txrx_pdev, htt_t2h_msg, peer_id, tid, num_mpdu_ranges);
            }
            break;
        }
    case HTT_T2H_MSG_TYPE_TX_COMPL_IND:
        {
            int num_msdus;
            enum htt_tx_status status;
            int ack_rssi_filled = 0, ppdu_peer_id_filled = 0, ast_index_filled = 0;
            uint32_t comple_cnt=0;
            uint16_t tid = HTT_INVALID_TID;

            /* status - no enum translation needed */
            status = HTT_TX_COMPL_IND_STATUS_GET(*msg_word);
            num_msdus = HTT_TX_COMPL_IND_NUM_GET(*msg_word);
#if MESH_MODE_SUPPORT
            ack_rssi_filled = HTT_TX_COMPL_IND_ACK_RSSI_PRESENT_GET(*msg_word);
#endif
            ppdu_peer_id_filled = HTT_TX_COMPL_IND_PPDU_PEER_ID_PRESENT_GET(*msg_word);

            ast_index_filled = HTT_TX_COMPL_IND_PEER_ID_PRESENT_GET(*msg_word);
            if (qdf_likely(!HTT_TX_COMPL_IND_TID_INV_GET(*msg_word)))
                tid  = HTT_TX_COMPL_IND_TID_GET(*msg_word);

            if (num_msdus & 0x1) {
                struct htt_tx_compl_ind_base *compl = (void *)msg_word;

                /*
                 * Host CPU endianness can be different from FW CPU. This
                 * can result in even and odd MSDU IDs being switched. If
                 * this happens, copy the switched final odd MSDU ID from
                 * location payload[size], to location payload[size-1],
                 * where the message handler function expects to find it
                 */
                if (compl->payload[num_msdus] != HTT_TX_COMPL_INV_MSDU_ID) {
                    compl->payload[num_msdus - 1] = compl->payload[num_msdus];
#if MESH_MODE_SUPPORT
                    if (ack_rssi_filled) {
                    /* swap rssi fields according to msdu ids */
                        compl->payload[(num_msdus << 1)] = compl->payload[(num_msdus << 1) + 1];
                    }
#endif
                }
            }
            /* comple_cnt is not used for the case of slow path, passing a dummy variable for parameter consistency */
            ol_tx_completion_handler(
                pdev->txrx_pdev, num_msdus, status,
                ack_rssi_filled,
                ppdu_peer_id_filled,
                ast_index_filled,
                msg_word + 1, &comple_cnt, tid);
#if ATH_11AC_TXCOMPACT

            htt_tx_sched(pdev);
#endif
            break;
        }
    case HTT_T2H_MSG_TYPE_TX_INSPECT_IND:
        {
            int num_msdus;

            num_msdus = HTT_TX_COMPL_IND_NUM_GET(*msg_word);
            if (num_msdus & 0x1) {
                struct htt_tx_compl_ind_base *compl = (void *)msg_word;

                /*
                 * Host CPU endianness can be different from FW CPU. This
                 * can result in even and odd MSDU IDs being switched. If
                 * this happens, copy the switched final odd MSDU ID from
                 * location payload[size], to location payload[size-1],
                 * where the message handler function expects to find it
                 */
                if (compl->payload[num_msdus] != HTT_TX_COMPL_INV_MSDU_ID) {
                    compl->payload[num_msdus - 1] =
                        compl->payload[num_msdus];
                }
            }
            ol_tx_inspect_handler(pdev->txrx_pdev, num_msdus, msg_word + 1);
#if ATH_11AC_TXCOMPACT
             htt_tx_sched(pdev);
#endif

            break;
        }
    default:
        htt_t2h_lp_msg_handler(context, htt_t2h_msg);
        return ;

    };

    /* Free the indication buffer */
    qdf_nbuf_free(htt_t2h_msg);
}

#ifdef WLAN_FEATURE_FASTPATH

#define OL_TX_LL_FAST_UPDATE_HTT_COMPLS(_num_htt_cmpls, _num_msdus) \
    _num_htt_cmpls +=  _num_msdus ;

void
htt_t2h_msg_handler_misc(struct htt_pdev_t *pdev, qdf_nbuf_t htt_t2h_msg,
                                        uint32_t *num_htt_cmpls)
{
    u_int32_t *msg_word = 0;
    enum htt_t2h_msg_type msg_type;

    msg_word = (u_int32_t *) qdf_nbuf_data(htt_t2h_msg);
    msg_type = HTT_T2H_MSG_TYPE_GET(*msg_word);
    switch (msg_type) {
    case HTT_T2H_MSG_TYPE_VERSION_CONF:
        {
            pdev->tgt_ver.major = HTT_VER_CONF_MAJOR_GET(*msg_word);
            pdev->tgt_ver.minor = HTT_VER_CONF_MINOR_GET(*msg_word);
            qdf_print(KERN_INFO"target uses HTT version %d.%d; host uses %d.%d",
                pdev->tgt_ver.major, pdev->tgt_ver.minor,
                HTT_CURRENT_VERSION_MAJOR, HTT_CURRENT_VERSION_MINOR);
            if (pdev->tgt_ver.major != HTT_CURRENT_VERSION_MAJOR) {
                qdf_print("*** Incompatible host/target HTT versions!");
            }
            /* abort if the target is incompatible with the host */
            qdf_assert(pdev->tgt_ver.major == HTT_CURRENT_VERSION_MAJOR);
            if (pdev->tgt_ver.minor != HTT_CURRENT_VERSION_MINOR) {
                qdf_print(
                    "*** Warning: host/target HTT versions are different, "
                    "though compatible!");
            }
            break;
        }
    case HTT_T2H_MSG_TYPE_RX_FLUSH:
        {
            u_int16_t peer_id;
            u_int8_t tid;
            int seq_num_start, seq_num_end;
            enum htt_rx_flush_action action;

            peer_id = HTT_RX_FLUSH_PEER_ID_GET(*msg_word);
            tid = HTT_RX_FLUSH_TID_GET(*msg_word);
            seq_num_start = HTT_RX_FLUSH_SEQ_NUM_START_GET(*(msg_word+1));
            seq_num_end = HTT_RX_FLUSH_SEQ_NUM_END_GET(*(msg_word+1));
            action =
                HTT_RX_FLUSH_MPDU_STATUS_GET(*(msg_word+1)) == 1 ?
                htt_rx_flush_release : htt_rx_flush_discard;
            ol_rx_flush_handler(
                pdev->txrx_pdev,
                peer_id, tid,
                seq_num_start,
                seq_num_end,
                action);
            break;
        }
    case  HTT_T2H_MSG_TYPE_RX_FRAG_IND:
        {
            unsigned num_msdu_bytes;
            u_int16_t peer_id;
            u_int8_t tid;

            peer_id = HTT_RX_FRAG_IND_PEER_ID_GET(*msg_word);
            tid = HTT_RX_FRAG_IND_EXT_TID_GET(*msg_word);

            num_msdu_bytes = HTT_RX_IND_FW_RX_DESC_BYTES_GET(*(msg_word + 2));
            /*
             * 1 word for the message header,
             * 1 word to specify the number of MSDU bytes,
             * 1 word for every 4 MSDU bytes (round up),
             * 1 word for the MPDU range header
             */
            pdev->rx_mpdu_range_offset_words = 3 + ((num_msdu_bytes + 3) >> 2);
            pdev->rx_ind_msdu_byte_idx = 0;
            ol_rx_frag_indication_handler(
                pdev->txrx_pdev,
                htt_t2h_msg,
                peer_id,
                tid);
            break;
        }
    case HTT_T2H_MSG_TYPE_RX_ADDBA:
        {
            u_int16_t peer_id;
            u_int8_t tid;
            u_int8_t win_sz;

            peer_id = HTT_RX_ADDBA_PEER_ID_GET(*msg_word);
            tid = HTT_RX_ADDBA_TID_GET(*msg_word);
            win_sz = HTT_RX_ADDBA_WIN_SIZE_GET(*msg_word);
            ol_rx_addba_handler(pdev->txrx_pdev, peer_id, tid, win_sz);
            break;
        }
    case HTT_T2H_MSG_TYPE_RX_DELBA:
        {
            u_int16_t peer_id;
            u_int8_t tid;

            peer_id = HTT_RX_DELBA_PEER_ID_GET(*msg_word);
            tid = HTT_RX_DELBA_TID_GET(*msg_word);
            ol_rx_delba_handler(pdev->txrx_pdev, peer_id, tid);
            break;
        }
    case HTT_T2H_MSG_TYPE_PEER_MAP:
        {
            u_int8_t mac_addr_deswizzle_buf[QDF_MAC_ADDR_SIZE];
            u_int8_t *peer_mac_addr;
            u_int16_t peer_id;
            u_int8_t vdev_id;

            peer_id = HTT_RX_PEER_MAP_PEER_ID_GET(*msg_word);
            vdev_id = HTT_RX_PEER_MAP_VDEV_ID_GET(*msg_word);
            peer_mac_addr = htt_t2h_mac_addr_deswizzle(
                (u_int8_t *) (msg_word+1), &mac_addr_deswizzle_buf[0]);

            ol_rx_peer_map_handler((struct ol_txrx_pdev_t *)pdev->txrx_pdev,
				    peer_id, vdev_id, peer_mac_addr);
            break;
        }
    case HTT_T2H_MSG_TYPE_PEER_UNMAP:
        {
            u_int16_t peer_id;
            peer_id = HTT_RX_PEER_UNMAP_PEER_ID_GET(*msg_word);

            ol_rx_peer_unmap_handler((struct ol_txrx_pdev_t *)pdev->txrx_pdev, peer_id);
            break;
        }
    case HTT_T2H_MSG_TYPE_SEC_IND:
        {
            u_int16_t peer_id;
            enum htt_sec_type sec_type;
            int is_unicast;

            peer_id = HTT_SEC_IND_PEER_ID_GET(*msg_word);
            sec_type = HTT_SEC_IND_SEC_TYPE_GET(*msg_word);
            is_unicast = HTT_SEC_IND_UNICAST_GET(*msg_word);
            msg_word++; /* point to the first part of the Michael key */
            ol_rx_sec_ind_handler(
                pdev->txrx_pdev, peer_id, sec_type, is_unicast, msg_word, msg_word+2);
            break;
        }
    case HTT_T2H_MSG_TYPE_TX_INSPECT_IND:
        {
            int num_msdus;

            num_msdus = HTT_TX_COMPL_IND_NUM_GET(*msg_word);
            if (num_msdus & 0x1) {
                struct htt_tx_compl_ind_base *compl = (void *)msg_word;

                /*
                 * Host CPU endianness can be different from FW CPU. This
                 * can result in even and odd MSDU IDs being switched. If
                 * this happens, copy the switched final odd MSDU ID from
                 * location payload[size], to location payload[size-1],
                 * where the message handler function expects to find it
                 */
                if (compl->payload[num_msdus] != HTT_TX_COMPL_INV_MSDU_ID) {
                    compl->payload[num_msdus - 1] =
                        compl->payload[num_msdus];
                }
            }
#if QCA_PARTNER_DIRECTLINK_TX
            if (CE_is_directlink(((struct ol_txrx_pdev_t*)pdev->txrx_pdev)->ce_tx_hdl)) {
                printk("ERROR: TX_INSPECT_IND over Directlink\n");
                A_ASSERT(0);
            }
#endif /* QCA_PARTNER_DIRECTLINK_TX */
            ol_tx_inspect_handler(pdev->txrx_pdev, num_msdus, msg_word + 1);

            OL_TX_LL_FAST_UPDATE_HTT_COMPLS(*num_htt_cmpls, num_msdus);

            break;
        }
    case HTT_T2H_MSG_TYPE_MGMT_TX_COMPL_IND:
        {
            struct htt_mgmt_tx_compl_ind * compl_msg =
                                 (struct htt_mgmt_tx_compl_ind *)(msg_word + 1);
            qdf_nbuf_t tx_mgmt_frm = NULL;
            qdf_nbuf_t mgmt_frm_cpy = NULL;
            A_UINT32 *ppdu_id_ptr = NULL;

            /* tx capture related processing */
            if (compl_msg->status != IEEE80211_TX_ERROR) {
                tx_mgmt_frm =
                      pdev->tx_mgmt_desc_ctxt.pool[compl_msg->desc_id].mgmt_frm;
                if (tx_mgmt_frm &&
                     ((struct ol_txrx_pdev_t *)pdev->txrx_pdev)->tx_capture &&
                     pdev->htt_process_nondata_tx_frames) {
                    mgmt_frm_cpy = qdf_nbuf_copy(tx_mgmt_frm);
                    if (mgmt_frm_cpy) {
                        ppdu_id_ptr = (A_UINT32 *)qdf_nbuf_push_head(
                                      mgmt_frm_cpy, sizeof(compl_msg->ppdu_id));
                        *(ppdu_id_ptr) = compl_msg->ppdu_id;
                        /*
                         * frame will be freed after processing in the below
                         * callback
                         */
                        pdev->htt_process_nondata_tx_frames(mgmt_frm_cpy);
                    }
                }
                tx_mgmt_frm = NULL;
            }

            htt_tx_mgmt_desc_free(pdev, compl_msg->desc_id, compl_msg->status);
            OL_TX_LL_FAST_UPDATE_HTT_COMPLS(*num_htt_cmpls, 1);
            break;
        }
#if TXRX_STATS_LEVEL != TXRX_STATS_LEVEL_OFF
    case HTT_T2H_MSG_TYPE_STATS_CONF:
        {
            u_int64_t cookie;
            u_int8_t *stats_info_list;
            u_int32_t vdev_id;

            vdev_id = HTT_T2H_STATS_CONF_HDR_VAP_ID_GET(*msg_word);
            cookie = *(msg_word + 1);
            cookie |= ((u_int64_t) (*(msg_word + 2))) << 32;

            stats_info_list = (u_int8_t *) (msg_word + 3);
            if ((ol_txrx_fw_stats_handler((struct ol_txrx_pdev_t *)pdev->txrx_pdev, cookie, stats_info_list, vdev_id)) == 0) {
                (*num_htt_cmpls)++;
	            htt_pkt_buf_list_del(pdev,cookie);
            }
            break;
        }
#endif
#ifndef REMOVE_PKT_LOG
    case HTT_T2H_MSG_TYPE_PKTLOG:
        {
            u_int32_t *pl_hdr;
            pl_hdr = (msg_word + 1);
            wdi_event_handler(WDI_EVENT_OFFLOAD_ALL, (struct ol_txrx_pdev_t *)pdev->txrx_pdev, pl_hdr,
                    HTT_INVALID_PEER, WDI_NO_VAL);
            break;
        }
#endif

    case HTT_T2H_MSG_TYPE_EN_STATS:
        {
#if UNIFIED_SMARTANTENNA
            ol_ath_smart_ant_stats_handler((struct ol_txrx_pdev_t *)pdev->txrx_pdev, msg_word, qdf_nbuf_len(htt_t2h_msg));
#endif
#if ENHANCED_STATS
            ol_tx_update_peer_stats((struct ol_txrx_pdev_t *)pdev->txrx_pdev, msg_word, qdf_nbuf_len(htt_t2h_msg));
#endif
            break;
        }
    case HTT_T2H_MSG_TYPE_CHAN_CHANGE:
        {
            pdev->rs_freq = *(msg_word + 1);
            ol_ath_chan_change_msg_handler((struct ol_txrx_pdev_t *)pdev->txrx_pdev, msg_word, qdf_nbuf_len(htt_t2h_msg));

            break;
        }
    case HTT_T2H_MSG_TYPE_AGGR_CONF:
        {
            msg_word++;
            htt_pkt_buf_list_del(pdev,(u_int64_t)*msg_word);
            (*num_htt_cmpls)++;
        }
        break;
    case HTT_T2H_MSG_TYPE_STATS_NOUPLOAD:
        {
            u_int64_t cookie;

            cookie = *(msg_word + 1);
            cookie |= ((u_int64_t) (*(msg_word + 2))) << 32;

            htt_pkt_buf_list_del(pdev,cookie);
            (*num_htt_cmpls)++;
        }
        break;
#if PEER_FLOW_CONTROL && !PEER_FLOW_CONTROL_HOST_SCHED
    case HTT_T2H_MSG_TYPE_TX_MODE_SWITCH_IND:
        {
            struct ol_txrx_pdev_t *radiopdev = (struct ol_txrx_pdev_t*)pdev->txrx_pdev;
            uint8_t mode_enable = 0, mode = 0;
            uint16_t threshold = 0;
            unsigned long modified_time_ns = radiopdev->pmap_rotting_timer_interval * 1000000;
            ktime_t modified_ktime;

            mode_enable   = HTT_T2H_TX_MODE_SWITCH_IND_ENABLE_GET(*msg_word);

            if(mode_enable) {
                mode      = HTT_T2H_TX_MODE_SWITCH_IND_MODE_GET(*(msg_word + 1));

                if(mode == HTT_TX_MODE_PUSH_NO_CLASSIFY) {
                    radiopdev->pflow_ctrl_mode    = HTT_TX_MODE_PUSH_NO_CLASSIFY;
                    qdf_print("Switching to Tx Mode-%u", mode);
                    ol_tx_switch_mode_flush_buffers((ol_txrx_pdev_handle) radiopdev);
                } else if(mode == HTT_TX_MODE_PUSH_PULL) {
                    radiopdev->pflow_ctrl_mode         = HTT_TX_MODE_PUSH_PULL;
                    /* Get threshold from target */
                    threshold = HTT_T2H_TX_MODE_SWITCH_IND_THRESHOLD_GET(*(msg_word + 1));
                    qdf_print("Switching to Tx Mode-%u Threshold %d", mode, threshold);
                    radiopdev->pflow_ctl_min_threshold = threshold;
                    modified_ktime = ktime_set(0, modified_time_ns);
                }
            }
        }
        break;
    case HTT_T2H_MSG_TYPE_TX_FETCH_IND:
        {
            uint32_t i, num_fetch_records = 0, resp_id = 0;
            uint32_t dialog = 0, num_resp_records = 0, num_resp_id_records = 0;
            struct htt_tx_fetch_desc_buf *tx_fetch_desc;
            uint16_t seq_num;
            struct htt_tx_fetch_resp_t *tx_fetch_resp_msg;
            uint32_t *fetch_ind_rec = NULL,*resp_ind_rec = NULL;
            struct ol_txrx_pdev_t *radiopdev = (struct ol_txrx_pdev_t*)pdev->txrx_pdev;
            unsigned long delta_usecs;
            ktime_t tstamp;

            if(radiopdev == NULL)
            {
                qdf_print("\n Invalid txrx_pdev HTT fetch failed");
                break;
            }

            if(radiopdev->prof_trigger)
            {
                tstamp = ktime_get_real();
            }

            radiopdev->pdev_data_stats.tx.pstats[HTTFETCH].value++;
            seq_num = HTT_T2H_TX_FETCH_IND_SEQ_NUM_GET(*msg_word);
            dialog = (*(msg_word + 1));
            num_fetch_records = HTT_T2H_TX_FETCH_IND_NUM_FETCH_RECORDS_GET((*(msg_word + 2)));
            num_resp_id_records = HTT_T2H_TX_FETCH_IND_NUM_RESP_ID_RECORDS_GET((*(msg_word + 2)));

            fetch_ind_rec = msg_word + 3;
            resp_ind_rec  = fetch_ind_rec + (num_fetch_records * 2);

            if(num_resp_id_records == 0)
                radiopdev->pdev_data_stats.tx.pstats[HTTFETCH_NOID].value++;
            /* free response ids  */
            for(i=0; i < num_resp_id_records; i++) {

                resp_id = *resp_ind_rec;
                htt_tx_fetch_desc_free(pdev, resp_id);

                resp_ind_rec++;
                /* Account for FETCH Responses in htt completions for CE index update */
                (*num_htt_cmpls)++;
            }

            tx_fetch_desc = htt_tx_fetch_desc_alloc(pdev);
            if (!tx_fetch_desc) {
                radiopdev->pdev_data_stats.tx.pstats[HTTDESC_ALLOC_FAIL].value++;
                break;
            }

            radiopdev->pdev_data_stats.tx.pstats[HTTDESC_ALLOC].value++;
#if PEER_FLOW_CONTROL_DEBUG
            qdf_print("\n HTT FETCH Rx Start");
            qdf_print("\n Fetch seq_num %u num_fetch_records %u num_resp_id_records %u",seq_num,
                    num_fetch_records, num_resp_id_records);
#endif
            tx_fetch_resp_msg = htt_h2t_fetch_resp_prepare(radiopdev, tx_fetch_desc, num_fetch_records*8);
            if(tx_fetch_resp_msg == NULL)
            {
                qdf_print("\n Response prepare failed");
                htt_tx_fetch_desc_free(pdev, tx_fetch_desc->desc_id);
                break;
            }

#if !PEER_FLOW_CONTROL_HOST_SCHED
            num_resp_records = ol_tx_ll_fetch_sched((struct ol_txrx_pdev_t *)pdev->txrx_pdev, num_fetch_records,
                                 fetch_ind_rec,  tx_fetch_desc->msg_buf);
#endif
            tx_fetch_resp_msg->word1 = seq_num;
            HTT_H2T_TX_FETCH_RESP_NUM_FETCH_RECORDS_SET(tx_fetch_resp_msg->word1, num_resp_records);
            tx_fetch_resp_msg->word2 = dialog;

            radiopdev->pdev_data_stats.tx.pstats[HTTFETCH_RESP].value++;
            htt_h2t_fetch_resp_tx((struct ol_txrx_pdev_t *)pdev->txrx_pdev, tx_fetch_desc);

            /* Get Fetch delta */
            if(radiopdev->prof_trigger) {
                delta_usecs = ktime_to_us(ktime_sub(ktime_get_real(), tstamp));

                if(delta_usecs < 500)
                    PFLOW_CTRL_PDEV_STATS_ADD(radiopdev, HTTFETCHBIN500, 1);
                else if(delta_usecs < 1000)
                    PFLOW_CTRL_PDEV_STATS_ADD(radiopdev, HTTFETCHBIN1000, 1);
                else if(delta_usecs < 2000)
                    PFLOW_CTRL_PDEV_STATS_ADD(radiopdev, HTTFETCHBIN2000, 1);
                else
                    PFLOW_CTRL_PDEV_STATS_ADD(radiopdev, HTTFETCHBINHIGH, 1);
            }

        }
        break;
    case HTT_T2H_MSG_TYPE_TX_FETCH_CONF:
        {
            uint32_t i, resp_id =0, num_resp_id_records = 0;
            struct ol_txrx_pdev_t *radiopdev = (struct ol_txrx_pdev_t*)pdev->txrx_pdev;
            num_resp_id_records = HTT_T2H_TX_FETCH_CONF_NUM_RESP_ID_RECORDS_GET(*msg_word);

            radiopdev->pdev_data_stats.tx.pstats[HTTFETCH_CONF].value++;

            /* free response ids  */
            for(i=0; i < num_resp_id_records; i++) {

                resp_id = *(msg_word + i + 1);

                htt_tx_fetch_desc_free(pdev, resp_id);

                /* Account for FETCH Responses in htt completions for CE index update */
                (*num_htt_cmpls)++;
            }
        }
        break;
#endif

#if WLAN_CFR_ENABLE
    case HTT_T2H_MSG_TYPE_CFR_DUMP_COMPL_IND:
        {
            void *pdev_ptr = NULL;
            struct ol_txrx_pdev_t *prdev = (struct ol_txrx_pdev_t *)pdev->txrx_pdev;

	    qdf_debug("CFR:%s HTT event received from FW\n");
            if(prdev == NULL)
            {
                qdf_err("Invalid txrx_pdev HTT fetch failed\n");
                break;
            }

            pdev_ptr = (void *)prdev->ctrl_pdev;
            if(pdev_ptr)
                ol_txrx_htt_cfr_rx_ind_handler(pdev_ptr, msg_word, qdf_nbuf_len(htt_t2h_msg));

        }
        break;
#endif

    default:
        break;
    };
}

#define HTT_T2H_MSG_BUF_REINIT(_buf, _qdf_dev)                              \
	do {                                                                \
		uint32_t paddr_lo; unsigned char *buf_end;	\
		paddr_lo = qdf_nbuf_get_frag_paddr((_buf), 0);  \
		buf_end = skb_end_pointer(_buf);	\
		qdf_nbuf_init((_buf));                                              \
		OS_SYNC_SINGLE((osdev_t)_qdf_dev->drv, paddr_lo,                          \
				buf_end - (_buf)->data,                             \
				BUS_DMA_FROMDEVICE, (dma_context_t)&paddr_lo);                    \
	} while(0)

void
htt_t2h_msg_handler_fast(void *htt_pdev, qdf_nbuf_t *nbuf_cmpl_arr,
        uint32_t num_cmpls)
{
    struct htt_pdev_t *pdev = (struct htt_pdev_t *)htt_pdev;
    struct ol_txrx_pdev_t *txrx_pdev = (struct ol_txrx_pdev_t *)pdev->txrx_pdev;
    qdf_nbuf_t htt_t2h_msg;
    u_int32_t *msg_word;
    uint32_t i;
    enum htt_t2h_msg_type msg_type;
    qdf_device_t qdf_dev = wlan_psoc_get_qdf_dev((void *)pdev->ctrl_psoc);
    uint32_t num_htt_cmpls = 0;
    uint32_t msg_len;

    for (i = 0; i < num_cmpls; i++) {

        htt_t2h_msg = nbuf_cmpl_arr[i];

        msg_len = qdf_nbuf_len(htt_t2h_msg);
        /* Move the data pointer to point to HTT header
         * past the HTC header + HTC header alignment padding
         */
        qdf_nbuf_pull_head(htt_t2h_msg, HTC_HEADER_LEN + HTC_HDR_ALIGNMENT_PADDING);

        msg_word = (u_int32_t *) qdf_nbuf_data(htt_t2h_msg);
        msg_type = HTT_T2H_MSG_TYPE_GET(*msg_word);

        /*
         * Process the data path messages first
         */
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
        if (pdev->nss_wifiol_ctx) {
            if ((msg_type == HTT_T2H_MSG_TYPE_TX_INSPECT_IND) ||
                    (msg_type == HTT_T2H_MSG_TYPE_TX_FETCH_IND) ||
                    (msg_type == HTT_T2H_MSG_TYPE_TX_FETCH_CONF)){
                printk("NSS Wifi Offload : Unhandled %x\n", msg_type);
                qdf_nbuf_free(htt_t2h_msg);
                continue;

            }
        }
#endif

        switch (msg_type) {
            case HTT_T2H_MSG_TYPE_RX_IND:
                {
                    unsigned num_mpdu_ranges;
                    unsigned num_msdu_bytes;
                    u_int16_t peer_id;
                    u_int8_t tid;

                    peer_id = HTT_RX_IND_PEER_ID_GET(*msg_word);
                    tid = HTT_RX_IND_EXT_TID_GET(*msg_word);

                    num_msdu_bytes = HTT_RX_IND_FW_RX_DESC_BYTES_GET(
                            *(msg_word + 2 + HTT_RX_PPDU_DESC_SIZE32));
                    /*
                     * 1 word for the message header,
                     * HTT_RX_PPDU_DESC_SIZE32 words for the FW rx PPDU desc
                     * 1 word to specify the number of MSDU bytes,
                     * 1 word for every 4 MSDU bytes (round up),
                     * 1 word for the MPDU range header
                     */
                    pdev->rx_mpdu_range_offset_words =
                        (HTT_RX_IND_HDR_BYTES + num_msdu_bytes + 3) >> 2;
                    num_mpdu_ranges = HTT_RX_IND_NUM_MPDU_RANGES_GET(*(msg_word + 1));
                    pdev->rx_ind_msdu_byte_idx = 0;
                    ol_rx_indication_handler(pdev->txrx_pdev,
                            htt_t2h_msg, peer_id, tid, num_mpdu_ranges);
                    break;
                }
            case HTT_T2H_MSG_TYPE_TX_COMPL_IND:
                {
                    int num_msdus;
                    enum htt_tx_status status;
                    int ack_rssi_filled = 0, ppdu_peer_id_filled = 0, ast_index_filled = 0;
                    uint16_t tid = HTT_INVALID_TID;
                    HTT_TX_PDEV_LOCK(&txrx_pdev->tx_lock);

                    /* status - no enum translation needed */
                    status = HTT_TX_COMPL_IND_STATUS_GET(*msg_word);
                    num_msdus = HTT_TX_COMPL_IND_NUM_GET(*msg_word);
#if MESH_MODE_SUPPORT
                    ack_rssi_filled = HTT_TX_COMPL_IND_ACK_RSSI_PRESENT_GET(*msg_word);
#endif
                    ppdu_peer_id_filled = HTT_TX_COMPL_IND_PPDU_PEER_ID_PRESENT_GET(*msg_word);

                    ast_index_filled = HTT_TX_COMPL_IND_PEER_ID_PRESENT_GET(*msg_word);
                    if (qdf_likely(!HTT_TX_COMPL_IND_TID_INV_GET(*msg_word)))
                        tid  = HTT_TX_COMPL_IND_TID_GET(*msg_word);

                    if (num_msdus & 0x1) {
                        struct htt_tx_compl_ind_base *compl = (void *)msg_word;

                        /*
                         * Host CPU endianness can be different from FW CPU. This
                         * can result in even and odd MSDU IDs being switched. If
                         * this happens, copy the switched final odd MSDU ID from
                         * location payload[size], to location payload[size-1],
                         * where the message handler function expects to find it
                         */
                        if (compl->payload[num_msdus] != HTT_TX_COMPL_INV_MSDU_ID) {
                            compl->payload[num_msdus - 1] = compl->payload[num_msdus];
#if MESH_MODE_SUPPORT
                            if (ack_rssi_filled) {
                            /* swap rssi fields according to msdu ids */
                                compl->payload[(num_msdus << 1)] = compl->payload[(num_msdus << 1) + 1];
                            }
#endif
                            if (ack_rssi_filled && ppdu_peer_id_filled) {
                                /*
                                 * ppdu_id follows the ack_rssi if the latter is filled
                                 */
                                compl->payload[(num_msdus << 2)] = compl->payload[(num_msdus << 2) + 1];
                            }
                        }
                    }
                    /* num_htt_cmpls is passed to the function for counting additional
                     * CE descriptors used for the case of NON_Std desc types */

#if QCA_PARTNER_DIRECTLINK_TX
                    if (CE_is_directlink(((struct ol_txrx_pdev_t*)pdev->txrx_pdev)->ce_tx_hdl)) {
                        printk("ERROR: TX_COMPL_IND received over Directlink\n");
                        A_ASSERT(0);
                    }
#endif /* QCA_PARTNER_DIRECTLINK_TX */
                    ol_tx_completion_handler(
                            pdev->txrx_pdev, num_msdus, status,
                            ack_rssi_filled,
                            ppdu_peer_id_filled,
                            ast_index_filled,
                            msg_word + 1, &num_htt_cmpls, tid);

                    /* Each Tx completion takes 2 slots for Linux */
                    /*
                     * When turning on this code for other OS'es, this
                     * hard-coding needs to be removed.
                     */
                    /* Following takes care of CE cnt for STD descriptor type */
                    OL_TX_LL_FAST_UPDATE_HTT_COMPLS(num_htt_cmpls, num_msdus);

                    HTT_TX_PDEV_UNLOCK(&txrx_pdev->tx_lock);
#if PEER_FLOW_CONTROL
                    PFLOW_CTRL_PDEV_STATS_ADD(txrx_pdev, TX_COMPL_IND, num_htt_cmpls);
#endif

                    break;
                }

            default:
                /*
                 * Handle less frequent HTT messages here
                 */
                htt_t2h_msg_handler_misc(pdev, htt_t2h_msg, &num_htt_cmpls);
                break;
        }
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
        if (pdev->nss_wifiol_ctx) {
            qdf_nbuf_free(htt_t2h_msg);
        } else
#endif
        {
            /* Re-initialize the indication buffer */
            HTT_T2H_MSG_BUF_REINIT(htt_t2h_msg, qdf_dev);
        }
    }

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    if (pdev->nss_wifiol_ctx) {
        return;
    }
#endif

    if(num_htt_cmpls) {
        HTT_TX_PDEV_LOCK(&txrx_pdev->tx_lock);
#if QCA_OL_TX_PDEV_LOCK
        hif_update_tx_ring(pdev->osc, num_htt_cmpls);
#endif
#if !PEER_FLOW_CONTROL
        ol_tx_ll_sched(pdev->txrx_pdev);
#endif
        HTT_TX_PDEV_UNLOCK(&txrx_pdev->tx_lock);
    }
}
qdf_export_symbol(htt_t2h_msg_handler_fast);
#endif /* WLAN_FEATURE_FASTPATH */

/*--- target->host HTT message Info Element access methods ------------------*/

/*--- tx completion message ---*/

u_int16_t
htt_tx_compl_desc_id(void *iterator, int num)
{
    /*
     * The MSDU IDs are packed , 2 per 32-bit word.
     * Iterate on them as an array of 16-bit elements.
     * This will work fine if the host endianness matches
     * the target endianness.
     * If the host endianness is opposite of the target's,
     * this iterator will produce descriptor IDs in a different
     * order than the target inserted them into the message -
     * if the target puts in [0, 1, 2, 3, ...] the host will
     * put out [1, 0, 3, 2, ...].
     * This is fine, except for the last ID if there are an
     * odd number of IDs.  But the TX_COMPL_IND handling code
     * in the htt_t2h_msg_handler already added a duplicate
     * of the final ID, if there were an odd number of IDs,
     * so this function can safely treat the IDs as an array
     * of 16-bit elements.
     */
    return *(((u_int16_t *) iterator) + num);
}

/*--- rx indication message ---*/

int
htt_rx_ind_flush(qdf_nbuf_t rx_ind_msg)
{
    u_int32_t *msg_word;

    msg_word = (u_int32_t *) qdf_nbuf_data(rx_ind_msg);
    return HTT_RX_IND_FLUSH_VALID_GET(*msg_word);
}

void
htt_rx_ind_flush_seq_num_range(
    qdf_nbuf_t rx_ind_msg,
    int *seq_num_start,
    int *seq_num_end)
{
    u_int32_t *msg_word;

    msg_word = (u_int32_t *) qdf_nbuf_data(rx_ind_msg);
    msg_word++;
    *seq_num_start = HTT_RX_IND_FLUSH_SEQ_NUM_START_GET(*msg_word);
    *seq_num_end   = HTT_RX_IND_FLUSH_SEQ_NUM_END_GET(*msg_word);
}

int
htt_rx_ind_release(qdf_nbuf_t rx_ind_msg)
{
    u_int32_t *msg_word;

    msg_word = (u_int32_t *) qdf_nbuf_data(rx_ind_msg);
    return HTT_RX_IND_REL_VALID_GET(*msg_word);
}

void
htt_rx_ind_release_seq_num_range(
    qdf_nbuf_t rx_ind_msg,
    int *seq_num_start,
    int *seq_num_end)
{
    u_int32_t *msg_word;

    msg_word = (u_int32_t *) qdf_nbuf_data(rx_ind_msg);
    msg_word++;
    *seq_num_start = HTT_RX_IND_REL_SEQ_NUM_START_GET(*msg_word);
    *seq_num_end   = HTT_RX_IND_REL_SEQ_NUM_END_GET(*msg_word);
}

void
htt_rx_ind_mpdu_range_info(
    struct htt_pdev_t *pdev,
    qdf_nbuf_t rx_ind_msg,
    int mpdu_range_num,
    enum htt_rx_status *status,
    int *mpdu_count)
{
    u_int32_t *msg_word;

    msg_word = (u_int32_t *) qdf_nbuf_data(rx_ind_msg);
    msg_word += pdev->rx_mpdu_range_offset_words + mpdu_range_num;
    *status = HTT_RX_IND_MPDU_STATUS_GET(*msg_word);
    *mpdu_count = HTT_RX_IND_MPDU_COUNT_GET(*msg_word);
}


/*--- stats confirmation message ---*/

void
htt_t2h_dbg_stats_hdr_parse(
    u_int8_t *stats_info_list,
    enum htt_dbg_stats_type *type,
    enum htt_dbg_stats_status *status,
    int *length,
    u_int8_t **stats_data)
{
    u_int32_t *msg_word = (u_int32_t *) stats_info_list;
    *type = HTT_T2H_STATS_CONF_TLV_TYPE_GET(*msg_word);
    *status = HTT_T2H_STATS_CONF_TLV_STATUS_GET(*msg_word);
    *length = HTT_T2H_STATS_CONF_TLV_HDR_SIZE +       /* header length */
        HTT_T2H_STATS_CONF_TLV_LENGTH_GET(*msg_word); /* data length */
    *stats_data = stats_info_list + HTT_T2H_STATS_CONF_TLV_HDR_SIZE;
}

void
htt_rx_frag_ind_flush_seq_num_range(
    qdf_nbuf_t rx_frag_ind_msg,
    int *seq_num_start,
    int *seq_num_end)
{
    u_int32_t *msg_word;

    msg_word = (u_int32_t *) qdf_nbuf_data(rx_frag_ind_msg);
    msg_word++;
    *seq_num_start = HTT_RX_FRAG_IND_FLUSH_SEQ_NUM_START_GET(*msg_word);
    *seq_num_end   = HTT_RX_FRAG_IND_FLUSH_SEQ_NUM_END_GET(*msg_word);
}

void
htt_t2h_dbg_enh_stats_hdr_parse(
    u_int32_t *stats_base,
    enum htt_t2h_en_stats_type *type,
    enum htt_t2h_en_stats_status *status)
{
    u_int32_t *msg_word;

    msg_word = stats_base + 1;

    *type = HTT_T2H_EN_STATS_CONF_TLV_TYPE_GET(*msg_word);
    *status = HTT_T2H_EN_STATS_CONF_TLV_STATUS_GET(*msg_word);
}

