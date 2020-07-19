/*
 * Copyright (c) 2013, 2017 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary . Qualcomm Innovation Center, Inc.
 *
 * 2013 Qualcomm Atheros, Inc..
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
#ifndef __OL_IF_SMARTANT_H
#define __OL_IF_SMARTANT_H

#if UNIFIED_SMARTANTENNA

#ifndef __ATTRIB_PACK
#define __ATTRIB_PACK __attribute__ ((packed))
#endif

#define TX_FRAME_OFFSET 13
#define TX_PEER_ID_OFFSET 1
#define TX_FRAME_TYPE_MASK 0x3c00000
#define TX_FRAME_TYPE_SHIFT 22

#define PPDU_TRY_WORDS 16
#define MAX_TX_PPDU_SIZE 32


#define TX_OK_MASK 0x80000000
#define TX_TRY_SERIES_MASK 0x01000000
#define TX_TRY_BW_MASK 0x30000000
#define TX_TRY_BW_SHIFT 28

#define NUM_DYN_BW_MAX 4
#define SBW_INDX_MAX 8

#define TX_TOTAL_TRIES_OFFSET 27
#define TX_TOTAL_TRIES_SHIFT 24
#define TX_TOTAL_TRIES_MASK 0x1f000000

#define SMART_ANT_FEEDBACK_OFFSET 29
#define SMART_ANT_FEEDBACK_TRAIN_MASK 0x80000000

#define SMART_ANT_FEEDBACK_OFFSET_2 30

#define ACK_RSSI0_OFFSET 23
#define ACK_RSSI1_OFFSET 24
#define ACK_RSSI2_OFFSET 25
#define ACK_RSSI3_OFFSET 26

#define LONG_RETRIES_OFFSET 21
#define SHORT_RETRIES_OFFSET 22
#define MAX_RETRIES 8 /* 2 series * 4 BW */

#define TX_ANT_OFFSET_S0 18 
#define TX_ANT_OFFSET_S1 19
#define TX_ANT_MASK 0x00ffffff

#define TXCTRL_S0_RATE_BW20_OFFSET   22
#define TXCTRL_S0_RATE_BW40_OFFSET   26
#define TXCTRL_S0_RATE_BW80_OFFSET   30
#define TXCTRL_S0_RATE_BW160_OFFSET  34
#define TXCTRL_RATE_MASK 0xff000000
#define TXCTRL_RATE_SHIFT 24

#define TXCTRL_S1_RATE_BW20_OFFSET   38
#define TXCTRL_S1_RATE_BW40_OFFSET   42
#define TXCTRL_S1_RATE_BW80_OFFSET   46
#define TXCTRL_S1_RATE_BW160_OFFSET  50

#define MAX_LEGACY_RATE_DWORDS 3
#define MAX_HT_RATE_DWORDS 10
#define BYTES_IN_DWORD 4
#define MASK_BYTE 0xff

/* Duplication of Packet Log Specific headers */

#define ATH_SMART_ANT_PKTLOG_HDR_LOG_TYPE_MASK 0xffff
#define ATH_SMART_ANT_PKTLOG_HDR_LOG_TYPE_SHIFT 0
#define ATH_SMART_ANT_PKTLOG_HDR_LOG_TYPE_OFFSET 1

/* Types of packet log events */
#define SMART_ANT_PKTLOG_TYPE_TX_CTRL      1
#define SMART_ANT_PKTLOG_TYPE_TX_STAT      2

#define SMART_ANT_PKTLOG_MAX_TXCTL_WORDS 57 /* +2 words for bitmap */
#define SMART_ANT_PKTLOG_MAX_TXSTATUS_WORDS 32 

#define MASK_LOWER_NIBBLE 0x0f
#define NIBBLE_BITS 4

struct ath_smart_ant_pktlog_hdr {
    u_int16_t flags;
    u_int16_t missed_cnt;
    u_int16_t log_type;
    u_int16_t size;
    u_int32_t timestamp;
}__ATTRIB_PACK;

struct smart_ant_txctl_frm_hdr {
    u_int16_t framectrl;       /* frame control field from header */
    u_int16_t seqctrl;         /* frame control field from header */
    u_int16_t bssid_tail;      /* last two octets of bssid */
    u_int16_t sa_tail;         /* last two octets of SA */
    u_int16_t da_tail;         /* last two octets of DA */
    u_int16_t resvd;
};

struct ath_smart_ant_pktlog_txctl {
    struct ath_smart_ant_pktlog_hdr pl_hdr;
        //struct txctl_frm_hdr frm_hdr;
    void *txdesc_hdr_ctl;   /* frm_hdr + Tx descriptor words */
    struct {
        struct smart_ant_txctl_frm_hdr frm_hdr;
        u_int32_t txdesc_ctl[SMART_ANT_PKTLOG_MAX_TXCTL_WORDS];
        //u_int32_t *proto_hdr;   /* protocol header (variable length!) */
        //u_int32_t *misc; /* Can be used for HT specific or other misc info */
    } priv;
} __ATTRIB_PACK;

struct ath_smart_ant_pktlog_tx_status {
    struct ath_smart_ant_pktlog_hdr pl_hdr;
    void *ds_status;
    int32_t misc[0];        /* Can be used for HT specific or other misc info */
}__ATTRIB_PACK;

#endif

#endif //end of __OL_IF_SMARTANT_H

