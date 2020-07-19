/*
 * Copyright (c) 2017,2019 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary . Qualcomm Innovation Center, Inc.
 *
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


#ifndef	__OL_IF_STATS_H
#define	__OL_IF_STATS_H

#define PKTLOG_STATS_MAX_TXCTL_WORDS 57 /* +2 words for bitmap */
#define ATH_PKTLOG_STATS_HDR_FLAGS_MASK 0xffff
#define ATH_PKTLOG_STATS_HDR_FLAGS_SHIFT 0
#define ATH_PKTLOG_STATS_HDR_FLAGS_OFFSET 0
#define ATH_PKTLOG_STATS_HDR_MISSED_CNT_MASK 0xffff0000
#define ATH_PKTLOG_STATS_HDR_MISSED_CNT_SHIFT 16
#define ATH_PKTLOG_STATS_HDR_MISSED_CNT_OFFSET 0
#define ATH_PKTLOG_STATS_HDR_LOG_TYPE_MASK 0xffff
#define ATH_PKTLOG_STATS_HDR_LOG_TYPE_SHIFT 0
#define ATH_PKTLOG_STATS_HDR_LOG_TYPE_OFFSET 1
#define ATH_PKTLOG_STATS_HDR_SIZE_MASK 0xffff0000
#define ATH_PKTLOG_STATS_HDR_SIZE_SHIFT 16
#define ATH_PKTLOG_STATS_HDR_SIZE_OFFSET 1
#define ATH_PKTLOG_STATS_HDR_TIMESTAMP_OFFSET 2
#define PKTLOG_STATS_TYPE_TX_CTRL      1
#define PKTLOG_STATS_TYPE_TX_STAT      2
#define PKTLOG_STATS_TYPE_TX_MSDU_ID   3
#define PKTLOG_STATS_TYPE_TX_FRM_HDR   4
#define PKTLOG_STATS_TYPE_RX_STAT      5
#define PKTLOG_STATS_TYPE_RC_FIND      6
#define PKTLOG_STATS_TYPE_RC_UPDATE    7
#define PKTLOG_STATS_TYPE_TX_VIRT_ADDR 8
#define PKTLOG_STATS_TYPE_DBG_PRINT    9
#define PKTLOG_STATS_TYPE_MAX          10

/* Definitions for values of command flag in
   WMI_CHAN_INFO_EVENT */
#define WMI_CHAN_INFO_FLAG_START_RESP  0
#define WMI_CHAN_INFO_FLAG_END_RESP    1
#define WMI_CHAN_INFO_FLAG_BEFORE_END_RESP   2
#define WMI_CHAN_INFO_FLAG_END_DIFF   3

#define TX_DESC_ID_LOW_MASK 0xffff
#define TX_DESC_ID_LOW_SHIFT 0
#define TX_DESC_ID_HIGH_MASK 0xffff0000
#define TX_DESC_ID_HIGH_SHIFT 16

#define TX_FRAME_OFFSET 13
#define TX_TYPE_OFFSET 14
#define TX_PEER_ID_MASK
#define TX_PEER_ID_OFFSET 1
#define TX_FRAME_TYPE_MASK 0x3c00000
#define TX_FRAME_TYPE_SHIFT 22
#define TX_FRAME_TYPE_NOACK_MASK 0x00010000
#define TX_FRAME_TYPE_NOACK_SHIFT 16
#define TX_TYPE_MASK 0xc0000
#define TX_TYPE_SHIFT 18
#define TX_AMPDU_SHIFT 15
#define TX_AMPDU_MASK 0x8000
#define PPDU_END_OFFSET 16
#define TX_OK_OFFSET (PPDU_END_OFFSET + 0)
#define TX_OK_MASK (0x80000000)
#define TX_RSSI_OFFSET (PPDU_END_OFFSET + 11)
#define TX_RSSI_MASK 0xff
#define RX_RSSI_COMB_MASK 0x000000ff
#define RX_RSSI_COMB_OFFSET 4
#define RX_RSSI_CHAIN_PRI20_MASK 0x000000ff
#define RX_RSSI_CHAIN_PRI20_SHIFT 0
#define RX_RSSI_CHAIN_SEC20_MASK 0x0000ff00
#define RX_RSSI_CHAIN_SEC20_SHIFT 8
#define RX_RSSI_CHAIN_SEC40_MASK 0x00ff0000
#define RX_RSSI_CHAIN_SEC40_SHIFT 16
#define RX_RSSI_CHAIN_SEC80_MASK 0xff000000
#define RX_RSSI_CHAIN_SEC80_SHIFT 24
#define RX_RSSI_CHAIN0_OFFSET 0
#define RX_RSSI_CHAIN1_OFFSET 1
#define RX_RSSI_CHAIN2_OFFSET 2
#define RX_RSSI_CHAIN3_OFFSET 3
#define RX_OVERFLOW_MASK 0x00010000
#define RX_NULL_DATA_MASK 0x00000080
#define SEQ_NUM_OFFSET 2
#define SEQ_NUM_MASK 0xfff

#define BA_BMAP_LSB_OFFSET 17
#define BA_BMAP_MSB_OFFSET 18
#define RATE_IDX_OFFSET 0
#define RATE_IDX_MASK 0x000000ff
#define SGI_SERIES_OFFSET 20
#define SGI_SERIES_MASK 0xff000000
#define SGI_SERIES_SHIFT 24
#define SERIES_BW_START_OFFSET 21
#define SERIES_BW_SIZE 4
#define SERIES_BW_MASK 0x30000000
#define SERIES_BW_SHIFT 28

#define RX_D_PREAMBLE_MASK 0xff000000
#define RX_D_PREAMBLE_SHIFT 24
#define RX_D_PREAMBLE_OFFSET 5
#define RX_D_MASK_LSIG_SEL  0x00000010
#define RX_D_MASK_LSIG_RATE 0x0000000f
#define RX_D_HT_RATE_OFFSET 6
#define RX_D_HT_MCS_MASK 0x7f
#define RX_D_VHT_RATE_OFFSET 7
#define RX_D_VHT_MCS_MASK 0xf
#define RX_D_VHT_NSS_MASK 0x7
#define RX_D_VHT_BW_MASK 3
#define RX_D_VHT_SGI_MASK 1

#define OL_ATH_RATE_EP_MULTIPLIER     (1<<7)  /* pow2 to optimize out * and / */
#define OL_ATH_EP_MUL(x, mul)         ((x) * (mul))
#define OL_ATH_RATE_LPF_LEN           10          // Low pass filter length for averaging rates
#define DUMMY_MARKER      0
#define OL_ATH_RATE_IN(x)             (OL_ATH_EP_MUL((x), OL_ATH_RATE_EP_MULTIPLIER))
#define OL_ATH_LPF_RATE(x, y, len) \
    (((x) != DUMMY_MARKER) ? ((((x) << 3) + (y) - (x)) >> 3) : (y))
#define OL_ATH_RATE_LPF(x, y) \
    ((x) = OL_ATH_LPF_RATE((x), OL_ATH_RATE_IN((y)), OL_ATH_RATE_LPF_LEN))
#define OL_ATH_RATE_OUT(x)  (((x) != DUMMY_MARKER) ? (OL_ATH_EP_RND((x), OL_ATH_RATE_EP_MULTIPLIER)) : DUMMY_MARKER)
#define OL_ATH_EP_RND(x, mul)       ((((x)%(mul)) >= ((mul)/2)) ? ((x) + ((mul) - 1)) / (mul) : (x)/(mul))

#define RSSI_INV     0x80 /*invalid RSSI */

#define RSSI_DROP_INV(_curr_val, _new_val) (_new_val == RSSI_INV ? _curr_val: _new_val)

#define RSSI_CHAIN_STATS(ppdu, rx_chain)        do {        \
        (rx_chain).rx_rssi_pri20 = RSSI_DROP_INV((rx_chain).rx_rssi_pri20, ((ppdu) & RX_RSSI_CHAIN_PRI20_MASK)); \
        (rx_chain).rx_rssi_sec20 = RSSI_DROP_INV((rx_chain).rx_rssi_sec20, ((ppdu) & RX_RSSI_CHAIN_SEC20_MASK) >> RX_RSSI_CHAIN_SEC20_SHIFT) ; \
        (rx_chain).rx_rssi_sec40 = RSSI_DROP_INV((rx_chain).rx_rssi_sec40, ((ppdu) & RX_RSSI_CHAIN_SEC40_MASK) >> RX_RSSI_CHAIN_SEC40_SHIFT); \
        (rx_chain).rx_rssi_sec80 = RSSI_DROP_INV((rx_chain).rx_rssi_sec80, ((ppdu) & RX_RSSI_CHAIN_SEC80_MASK) >> RX_RSSI_CHAIN_SEC80_SHIFT); \
    } while ( 0 )


#define RSSI_CHAIN_PRI20(ppdu, rx_chain)       do { \
      rx_chain = RSSI_DROP_INV((rx_chain), ((ppdu) & HTT_T2H_EN_STATS_RSSI_PRI_20_M) >> HTT_T2H_EN_STATS_RSSI_PRI_20_S); \
    } while ( 0 )


struct ath_pktlog_stats_hdr {
    u_int16_t flags;
    u_int16_t missed_cnt;
    u_int16_t log_type;
    u_int16_t size;
    u_int32_t timestamp;
#ifdef CONFIG_AR900B_SUPPORT
    u_int32_t type_specific_data;
#endif
}__attribute__ ((packed));

struct txctl_stats_frm_hdr {
    u_int16_t framectrl;       /* frame control field from header */
    u_int16_t seqctrl;         /* frame control field from header */
    u_int16_t bssid_tail;      /* last two octets of bssid */
    u_int16_t sa_tail;         /* last two octets of SA */
    u_int16_t da_tail;         /* last two octets of DA */
    u_int16_t resvd;
};

struct ath_pktlog_stats_txctl {
    struct ath_pktlog_stats_hdr pl_hdr;
    //struct txctl_frm_hdr frm_hdr;
    void *txdesc_hdr_ctl;   /* frm_hdr + Tx descriptor words */
    struct {
        struct txctl_stats_frm_hdr frm_hdr;
        u_int32_t txdesc_ctl[PKTLOG_STATS_MAX_TXCTL_WORDS];
        //u_int32_t *proto_hdr;   /* protocol header (variable length!) */
        //u_int32_t *misc; /* Can be used for HT specific or other misc info */
        } priv;
}__attribute__ ((packed));

int
ol_ath_wlan_profile_data_event_handler (ol_soc_t sc, u_int8_t *data,
        u_int32_t datalen);

void
ol_ath_chan_info_attach(struct ieee80211com *ic);

void
ol_ath_chan_info_detach(struct ieee80211com *ic);

void
ol_ath_stats_soc_attach(ol_ath_soc_softc_t *soc);

void
ol_ath_stats_soc_detach(ol_ath_soc_softc_t *soc);

void
ol_ath_stats_attach(struct ieee80211com *ic);

void
ol_ath_stats_detach(struct ieee80211com *ic);

void
ol_ath_soc_chan_info_detach(ol_ath_soc_softc_t *soc);

void
ol_ath_soc_chan_info_attach(ol_ath_soc_softc_t *soc);

int32_t
ol_ath_request_stats(struct ieee80211com *ic, void *cmd_param);

void
acfg_chan_stats_event(struct ieee80211com *ic,
         u_int8_t self_util, u_int8_t obss_util);

QDF_STATUS ol_find_chan_stats_delta(struct ieee80211com *ic,
                                    periodic_chan_stats_t *pstats,
                                    periodic_chan_stats_t *nstats,
                                    periodic_chan_stats_t *delta);

#endif /* OL_IF_STATS_H */

