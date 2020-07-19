/*
 * Copyright (c) 2012, 2015, 2017-2018 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2012, 2015 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#ifndef _PKTLOG_FMT_H_
#define _PKTLOG_FMT_H_

#define CUR_PKTLOG_VER          10010  /* Packet log version */
#define PKTLOG_MAGIC_NUM        7735225

#ifndef MAX_TX_RATE_TBL
#define MAX_TX_RATE_TBL 72
#endif

#ifdef __linux__
#define PKTLOG_PROC_DIR "ath_pktlog"
#define PKTLOG_PROC_SYSTEM "system"
#define WLANDEV_BASENAME "wifi"
#endif

#ifdef WIN32
#pragma pack(push, pktlog_fmt, 1)
#define __ATTRIB_PACK
#elif defined(__EFI__)
#define __ATTRIB_PACK
#else
#ifndef __ATTRIB_PACK
#define __ATTRIB_PACK __attribute__ ((packed))
#endif
#endif
#include <a_types.h>
#include "ieee80211_radiotap.h"

/*
 * The maximum size of the radiotap data can be 36 bytes.
 * Some extra bytes for future addition.
 */
#define MAX_RADIOTAP_DATA_SIZE 40
/*
 * Each packet log entry consists of the following fixed length header
 * followed by variable length log information determined by log_type
 */

/* New fields in struct ath_pktlog_hdr* should be added at end */
struct ath_pktlog_hdr_ar900b {
    u_int16_t flags;
    u_int16_t missed_cnt;
    u_int16_t log_type;
    u_int16_t size;
    u_int32_t timestamp;
    u_int32_t type_specific_data;
}__ATTRIB_PACK;

struct ath_pktlog_hdr_ar9888 {
    u_int16_t flags;
    u_int16_t missed_cnt;
    u_int16_t log_type;
    u_int16_t size;
    u_int32_t timestamp;
    u_int32_t type_specific_data;
}__ATTRIB_PACK;


/* use bigger structure as a generic ath_pktlog_hdr so we can
 * access any fields using -> ot . operator seemlessly.
 */
typedef struct ath_pktlog_hdr_ar900b ath_pktlog_hdr_t;

#define BEELINER_EXTRA_PKTLOG_HEADER_SIZE  (sizeof(u_int32_t))

#define ATH_PKTLOG_HDR_FLAGS_MASK 0xffff
#define ATH_PKTLOG_HDR_FLAGS_SHIFT 0
#define ATH_PKTLOG_HDR_FLAGS_OFFSET 0
#define ATH_PKTLOG_HDR_MISSED_CNT_MASK 0xffff0000
#define ATH_PKTLOG_HDR_MISSED_CNT_SHIFT 16
#define ATH_PKTLOG_HDR_MISSED_CNT_OFFSET 0
#define ATH_PKTLOG_HDR_LOG_TYPE_MASK 0xffff
#define ATH_PKTLOG_HDR_LOG_TYPE_SHIFT 0
#define ATH_PKTLOG_HDR_LOG_TYPE_OFFSET 1
#define ATH_PKTLOG_HDR_SIZE_MASK 0xffff0000
#define ATH_PKTLOG_HDR_SIZE_SHIFT 16
#define ATH_PKTLOG_HDR_SIZE_OFFSET 1
#define ATH_PKTLOG_HDR_TIMESTAMP_OFFSET 2
#define ATH_PKTLOG_HDR_TYPE_SPECIFIC_DATA_OFFSET 3
#define ATH_PKTLOG_HDR_MAC_ID_MASK 0x30
#define ATH_PKTLOG_HDR_MAC_ID_SHIFT 4
/* Beacon indication and vdev_id offsets in pktlog header */
#define ATH_PKTLOG_TX_IND_BEACON_OFFSET 3
#define ATH_PKTLOG_TX_IND_BEACON_MASK 0xFF000000
#define ATH_PKTLOG_TX_IND_BEACON 0xFF000000
#define ATH_PKTLOG_TX_VDEVID_OFFSET 3
#define ATH_PKTLOG_TX_VDEVID_MASK 0x00FF0000
#define ATH_PKTLOG_TX_VDEVID_SHIFT 16
#define ATH_PKTLOG_TX_STAT_PPDU_ID_MASK 0x0000FFFF
#define ATH_PKTLOG_TX_STAT_PPDU_ID_SHIFT 16
#define ATH_PKTLOG_TX_STAT_PPDU_ID_OFFSET 156
#define ATH_PKTLOG_TX_STAT_FRM_CTRL_OFFSET ( ATH_PKTLOG_TX_STAT_PPDU_ID_OFFSET - 2 )
#define ATH_PKTLOG_TX_STAT_QOS_CTRL_OFFSET ( ATH_PKTLOG_TX_STAT_PPDU_ID_OFFSET - 4 )
#define ATH_PKTLOG_TX_STAT_MPDU_LOW_OFFSET ( ATH_PKTLOG_TX_STAT_PPDU_ID_OFFSET - 28 )
#define ATH_PKTLOG_TX_STAT_MPDU_HIGH_OFFSET ( ATH_PKTLOG_TX_STAT_MPDU_LOW_OFFSET - 4 )
#define ATH_PKTLOG_TX_STAT_TSF_OFFSET ( ATH_PKTLOG_TX_STAT_PPDU_ID_OFFSET + 44 )
#define ATH_PKTLOG_TX_STAT_RSSI_OFFSET ( ATH_PKTLOG_TX_STAT_TSF_OFFSET + 24 )
#define ATH_PKTLOG_TX_STAT_LSIG_OFFSET ( ATH_PKTLOG_TX_STAT_RSSI_OFFSET + 12 )
#define ATH_PKTLOG_TX_STAT_SIG1_OFFSET ( ATH_PKTLOG_TX_STAT_LSIG_OFFSET + 4 )
#define ATH_PKTLOG_TX_STAT_SIG2_OFFSET ( ATH_PKTLOG_TX_STAT_SIG1_OFFSET + 4 )
#define ATH_PKTLOG_TX_STAT_SIGB0_OFFSET ( ATH_PKTLOG_TX_STAT_SIG2_OFFSET + 4 )
#define ATH_PKTLOG_TX_STAT_SIGB1_OFFSET ( ATH_PKTLOG_TX_STAT_SIGB0_OFFSET + 4 )
#define ATH_PKTLOG_TX_STAT_SIGB2_OFFSET ( ATH_PKTLOG_TX_STAT_SIGB1_OFFSET + 4 )

/*AR9888 related*/
#define ATH_PKTLOG_TX_STAT_TRIES_OFFSET 115
#define ATH_PKTLOG_TX_STAT_MPDU_BMAP_OFFSET (ATH_PKTLOG_TX_STAT_TRIES_OFFSET + 29)
#define ATH_PKTLOG_TX_STAT_FRAME_CTL_OFFSET (ATH_PKTLOG_TX_STAT_MPDU_BMAP_OFFSET + 46)
#define ATH_TX_PPDU_NUM_DWORDS 54
#define ATH_MAX_SERIES 4
#define ATH_SERIES_BW_SIZE 16

#define PKTLOG_CHECK_SET_VAL(field, val) \
    A_ASSERT(!((val) & ~((field ## _MASK) >> (field ## _SHIFT))))

#define PKTLOG_MAC_ID_GET(hdr) \
            (((*((uint32_t *)(hdr) + ATH_PKTLOG_HDR_TYPE_SPECIFIC_DATA_OFFSET)) \
                 & ATH_PKTLOG_HDR_MAC_ID_MASK) >> \
                 ATH_PKTLOG_HDR_MAC_ID_SHIFT)

#define PKTLOG_MAC_ID_SET(hdr, value) \
    do {                                                   \
        PKTLOG_CHECK_SET_VAL(ATH_PKTLOG_HDR_MAC_ID, value); \
        (*((uint32_t *)(hdr) + ATH_PKTLOG_HDR_TYPE_SPECIFIC_DATA_OFFSET)) |= (value)  << ATH_PKTLOG_HDR_MAC_ID_SHIFT; \
    } while (0)


enum {
    PKTLOG_FLG_FRM_TYPE_LOCAL_S = 0,
    PKTLOG_FLG_FRM_TYPE_REMOTE_S,
    PKTLOG_FLG_FRM_TYPE_CLONE_S,
    PKTLOG_FLG_FRM_TYPE_CBF_S,
    PKTLOG_FLG_FRM_TYPE_UNKNOWN_S
};

#define PKTLOG_PHY_BB_SIGNATURE 0xBB
enum {
    PKTLOG_TX_CV_TRANSFER = 0,
    PKTLOG_RX_EXPLICIT_CV_TRANSFER,
    PKTLOG_RX_IMPLICIT_CV_TRANSFER,
    PKTLOG_EXPLICIT_DEBUG_H = 240,
    PKTLOG_IMPLICIT_DEBUG_H,
    PKTLOG_BEAMFORM_DEBUG_H,
};


/****************************
 * Pktlog flag field details
 * packet origin [1:0]
 * 00 - Local
 * 01 - Remote
 * 10 - Unknown/Not applicable
 * 11 - Reserved
 * reserved [15:2]
 * *************************/

#define PHFLAGS_PROTO_MASK      0x0000f000
#define PHFLAGS_PROTO_SFT       12
#define PHFLAGS_MACVERSION_MASK 0x0fff0000
#define PHFLAGS_MACVERSION_SFT  16
/*
 * XXX: This need not be part of packetlog header flags - Should be
 * moved to plinfo
 */

#define PHFLAGS_INTERRUPT_CONTEXT 0x80000000

/* flags in pktlog header */
#define PHFLAGS_MISCCNT_MASK 0x000F /* Indicates no. of misc log parameters
                                       (32-bit integers) at the end of a
                                       log entry */

#define PHFLAGS_MACREV_MASK 0xff0  /* MAC revision */
#define PHFLAGS_MACREV_SFT  4

/* Types of protocol logging flags */
//#define PHFLAGS_PROTO_MASK  0xf000
//#define PHFLAGS_PROTO_SFT   12
#define PKTLOG_PROTO_NONE   0
#define PKTLOG_PROTO_UDP    1
#define PKTLOG_PROTO_TCP    2

/* Masks for setting pktlog events filters */
#define ATH_PKTLOG_TX        0x000000001
#define ATH_PKTLOG_RX        0x000000002
#define ATH_PKTLOG_RCFIND    0x000000004
#define ATH_PKTLOG_RCUPDATE  0x000000008
#define ATH_PKTLOG_ANI       0x000000010
#define ATH_PKTLOG_DBG_PRINT 0x000000020
#define ATH_PKTLOG_PHYERR    0x000000040
#define ATH_PKTLOG_PROMISC   0x000000080
#define ATH_PKTLOG_CBF       0x000000100
#define ATH_PKTLOG_H_INFO    0x000000200
#define ATH_PKTLOG_STEERING  0x000000400
#define ATH_PKTLOG_REMOTE_LOGGING_ENABLE  0x000000800
#define ATH_PKTLOG_TX_CAPTURE_ENABLE  0x000001000
#define ATH_PKTLOG_LITE_T2H  0x000002000
#define ATH_PKTLOG_LITE_RX   0x000004000

/* Masks for setting pktlog info filters */
#define ATH_PKTLOG_PROTO            0x00000001  /* Decode and log protocol headers */
#define ATH_PKTLOG_TRIGGER_SACK     0x00000002  /* Triggered stop as seeing TCP SACK packets */
#define ATH_PKTLOG_TRIGGER_THRUPUT  0x00000004  /* Triggered stop as throughput drops below a threshold */
#define ATH_PKTLOG_TRIGGER_PER      0x00000008  /* Triggered stop as PER goes above a threshold */
#define ATH_PKTLOG_TRIGGER_PHYERR   0x00000010  /* Triggered stop as # of phyerrs goes above a threshold */

/* Types of packet log events */
#define PKTLOG_TYPE_TX_CTRL         1
#define PKTLOG_TYPE_TX_STAT         2
#define PKTLOG_TYPE_TX_MSDU_ID      3
#define PKTLOG_TYPE_TX_FRM_HDR      4
#define PKTLOG_TYPE_RX_STAT         5
#define PKTLOG_TYPE_RC_FIND         6
#define PKTLOG_TYPE_RC_UPDATE       7
#define PKTLOG_TYPE_TX_VIRT_ADDR    8
#define PKTLOG_TYPE_DBG_PRINT       9
#define PKTLOG_TYPE_RX_CBF          10
#define PKTLOG_TYPE_ANI             11
#define PKTLOG_TYPE_GRPID           12
#define PKTLOG_TYPE_TX_MU           13
#define PKTLOG_TYPE_SMART_ANTENNA   14
#define PKTLOG_TYPE_TX_PFSCHED_CMD  15
#define PKTLOG_TYPE_TX_FW_GENERATED1  19
#define PKTLOG_TYPE_TX_FW_GENERATED2  20
#define PKTLOG_TYPE_MAX             21
#define PKTLOG_TYPE_RX_STATBUF     22
#define PKTLOG_TYPE_LITE_T2H     23
#define PKTLOG_TYPE_LITE_RX     24

/*AR9888-specific events*/
#define PKTLOG_TYPE_TX_STATS_COMBINED 11
#define PKTLOG_TYPE_TX_LOCAL          12

#define PKTLOG_MAX_TXCTL_WORDS_AR9888 57 /* +2 words for bitmap */
#define PKTLOG_MAX_TXSTATUS_WORDS_AR9888 32

#define PKTLOG_MAX_TX_WORDS 512
#define PKTLOG_MAX_TXCTL_WORDS_AR900B 370
#define PKTLOG_MAX_TXSTATUS_WORDS_AR900B 150

#define PKTLOG_MAX_PROTO_WORDS  16
#define PKTLOG_MAX_RXDESC_WORDS 62

struct txctl_frm_hdr {
    u_int16_t framectrl;       /* frame control field from header */
    u_int16_t seqctrl;         /* frame control field from header */
    u_int16_t bssid_tail;      /* last two octets of bssid */
    u_int16_t sa_tail;         /* last two octets of SA */
    u_int16_t da_tail;         /* last two octets of DA */
    u_int16_t resvd;
};

/* Peregrine 11ac based */
#define MAX_PKT_INFO_MSDU_ID_AR900B     1
#define MAX_PKT_INFO_MSDU_ID_AR9888     192
/*
 * msdu_id_info_ar9xxx_t is defined for reference only
 */
typedef struct {
    A_UINT32 num_msdu;
    A_UINT8 bound_bmap[(MAX_PKT_INFO_MSDU_ID_AR900B + 7)>>3];
    /* TODO:
     *  Convert the id's to uint32_t
     *  Reduces computation in the driver code
     */
    A_UINT16 id[MAX_PKT_INFO_MSDU_ID_AR900B];
}__ATTRIB_PACK msdu_id_info_ar900b_t;

typedef struct {
    A_UINT32 num_msdu;
    A_UINT8 bound_bmap[MAX_PKT_INFO_MSDU_ID_AR9888 >> 3];
    /* TODO:
     *  Convert the id's to uint32_t
     *  Reduces computation in the driver code
     */
    A_UINT16 id[MAX_PKT_INFO_MSDU_ID_AR9888];
}__ATTRIB_PACK msdu_id_info_ar9888_t;


#define MSDU_ID_INFO_NUM_MSDU_OFFSET 0 /* char offset */
#define MSDU_ID_INFO_BOUND_BM_OFFSET_AR900B offsetof(msdu_id_info_ar900b_t, bound_bmap)
#define MSDU_ID_INFO_BOUND_BM_OFFSET_AR9888 offsetof(msdu_id_info_ar9888_t, bound_bmap)

#define MSDU_ID_INFO_ID_OFFSET_AR900B  offsetof(msdu_id_info_ar900b_t, id)
#define MSDU_ID_INFO_ID_OFFSET_AR9888  ((MAX_PKT_INFO_MSDU_ID_AR9888 >> 3) + 4)

struct ath_pktlog_txctl_ar9888 {
    struct ath_pktlog_hdr_ar9888 pl_hdr;
        //struct txctl_frm_hdr frm_hdr;
    void *txdesc_hdr_ctl;   /* frm_hdr + Tx descriptor words */
    struct {
        struct txctl_frm_hdr frm_hdr;
        u_int32_t txdesc_ctl[PKTLOG_MAX_TXCTL_WORDS_AR9888];
        //u_int32_t *proto_hdr;   /* protocol header (variable length!) */
        //u_int32_t *misc; /* Can be used for HT specific or other misc info */
    } priv;
} __ATTRIB_PACK;

struct ath_pktlog_tx_status {
    ath_pktlog_hdr_t pl_hdr;
    void *ds_status;
    int32_t misc[0];        /* Can be used for HT specific or other misc info */
}__ATTRIB_PACK;


struct ath_pktlog_msdu_info_ar900b {
    struct ath_pktlog_hdr_ar900b pl_hdr;
    void *ath_msdu_info;
    A_UINT32 num_msdu;
    struct {
        /*
         * Provision to add more information fields
         */
        struct msdu_info_t {
            A_UINT32 num_msdu;
            A_UINT8 bound_bmap[MAX_PKT_INFO_MSDU_ID_AR900B >> 3];
        } msdu_id_info;
        /*
         * array of num_msdu
         * Static implementation will consume unwanted memory
         * Need to split the pktlog_get_buf to get the buffer pointer only
         */
        uint16_t msdu_len[MAX_PKT_INFO_MSDU_ID_AR900B];
    } priv;
    size_t priv_size;

}__ATTRIB_PACK;


struct ath_pktlog_msdu_info_ar9888 {
    struct ath_pktlog_hdr_ar9888 pl_hdr;
    void *ath_msdu_info;
    A_UINT32 num_msdu;
    struct {
        /*
         * Provision to add more information fields
         */
        struct msdu_info_type {
            A_UINT32 num_msdu;
            A_UINT8 bound_bmap[MAX_PKT_INFO_MSDU_ID_AR9888 >> 3];
        } msdu_id_info;
        /*
         * array of num_msdu
         * Static implementation will consume unwanted memory
         * Need to split the pktlog_get_buf to get the buffer pointer only
         */
        uint16_t msdu_len[MAX_PKT_INFO_MSDU_ID_AR9888];
    } priv;
    size_t priv_size;

}__ATTRIB_PACK;

struct ath_pktlog_cbf_info {
    ath_pktlog_hdr_t pl_hdr;
    void *cbf;
}__ATTRIB_PACK;

struct bb_tlv_pkt_hdr {
    u_int32_t    len    : 16,   //[15:0]
                 tag    : 8,    //[23:16]
                 sig    : 8;    //[31:16]
} __ATTRIB_PACK;

struct ath_pktlog_cbf_submix_info {
    u_int8_t   sub_type;    /*1-->cbf(RX_EXPLICIT_CV_TRANSFER), 2-->H-info(RX_IMPLICIT_CV_TRANSFER), 3-->Steering*/
    u_int16_t  reserved;
    u_int8_t   frag_flag;   /*0-->No fragment, 1-->fragmented*/
}__ATTRIB_PACK;

struct ath_pktlog_rx_info {
    ath_pktlog_hdr_t pl_hdr;
    void *rx_desc;
}__ATTRIB_PACK;

struct ath_pktlog_rc_find {
    ath_pktlog_hdr_t pl_hdr;
    void *rcFind;
}__ATTRIB_PACK;

struct ath_pktlog_rc_update {
    ath_pktlog_hdr_t pl_hdr;
    void *txRateCtrl;/* rate control state proper */
}__ATTRIB_PACK;

struct ath_pktlog_dbg_print {
    ath_pktlog_hdr_t pl_hdr;
    void *dbg_print;
}__ATTRIB_PACK;

#define PKTLOG_MAX_RXSTATUS_WORDS 11

struct ath_pktlog_ani {
    u_int8_t phyStatsDisable;
    u_int8_t noiseImmunLvl;
    u_int8_t spurImmunLvl;
    u_int8_t ofdmWeakDet;
    u_int8_t cckWeakThr;
    int8_t rssi;
    u_int16_t firLvl;
    u_int16_t listenTime;
    u_int16_t resvd;
    u_int32_t cycleCount;
    u_int32_t ofdmPhyErrCount;
    u_int32_t cckPhyErrCount;
    int32_t misc[0];         /* Can be used for HT specific or other misc info */
} __ATTRIB_PACK;


struct ath_pktlog_rcfind {
    u_int8_t rate;
    u_int8_t rateCode;
    int8_t rcRssiLast;
    int8_t rcRssiLastPrev;
    int8_t rcRssiLastPrev2;
    int8_t rssiReduce;
    u_int8_t rcProbeRate;
    int8_t isProbing;
    int8_t primeInUse;
    int8_t currentPrimeState;
    u_int8_t rcRateTableSize;
    u_int8_t rcRateMax;
    u_int8_t ac;
    int32_t misc[0];         /* Can be used for HT specific or other misc info */
} __ATTRIB_PACK;


struct ath_pktlog_rcupdate {
    u_int8_t txRate;
    u_int8_t rateCode;
    int8_t rssiAck;
    u_int8_t Xretries;
    u_int8_t retries;
    int8_t rcRssiLast;
    int8_t rcRssiLastLkup;
    int8_t rcRssiLastPrev;
    int8_t rcRssiLastPrev2;
    u_int8_t rcProbeRate;
    u_int8_t rcRateMax;
    int8_t useTurboPrime;
    int8_t currentBoostState;
    u_int8_t rcHwMaxRetryRate;
    u_int8_t ac;
    u_int8_t resvd[2];
    int8_t rcRssiThres[MAX_TX_RATE_TBL];
    u_int8_t rcPer[MAX_TX_RATE_TBL];
    u_int8_t rcMaxAggrSize[MAX_TX_RATE_TBL];
    u_int8_t headFail; /* rate control and aggregation variables ( part of ATH_SUPPORT_VOWEXT ) */
    u_int8_t tailFail; /* rate control and aggregation variables ( part of ATH_SUPPORT_VOWEXT ) */
    u_int8_t aggrSize; /* rate control and aggregation variables ( part of ATH_SUPPORT_VOWEXT ) */
    u_int8_t aggrLimit;/* rate control and aggregation variables ( part of ATH_SUPPORT_VOWEXT ) */
    u_int8_t lastRate; /* rate control and aggregation variables ( part of ATH_SUPPORT_VOWEXT ) */
    int32_t misc[0];         /* Can be used for HT specific or other misc info */
    /* TBD: Add any new parameters required */
} __ATTRIB_PACK;

#ifdef WIN32
#pragma pack(pop, pktlog_fmt)
#endif
#ifdef __ATTRIB_PACK
#undef __ATTRIB_PACK
#endif   /* __ATTRIB_PACK */

/*
 * The following header is included in the beginning of the file,
 * followed by log entries when the log buffer is read through procfs
 */

struct ath_pktlog_bufhdr {
    u_int32_t magic_num;  /* Used by post processing scripts */
    u_int32_t version;    /* Set to CUR_PKTLOG_VER */
};

struct ath_pktlog_buf {
    struct ath_pktlog_bufhdr bufhdr;
    int32_t rd_offset;
    int32_t wr_offset;
    char log_data[0];
};

#define PKTLOG_MOV_RD_IDX_HDRSIZE(_rd_offset, _log_buf, _log_size, _pktlog_hdr_size)  \
    do { \
        if((_rd_offset + _pktlog_hdr_size + \
            ((ath_pktlog_hdr_t *)((_log_buf)->log_data + \
            (_rd_offset)))->size) <= _log_size) { \
            _rd_offset = ((_rd_offset) + _pktlog_hdr_size + \
                            ((ath_pktlog_hdr_t *)((_log_buf)->log_data + \
                            (_rd_offset)))->size); \
        } else { \
            _rd_offset = ((ath_pktlog_hdr_t *)((_log_buf)->log_data +  \
                                           (_rd_offset)))->size;  \
        } \
        (_rd_offset) = (((_log_size) - (_rd_offset)) >= \
                         _pktlog_hdr_size) ? _rd_offset:0;\
    } while(0)

struct ath_pktlog_tx_capture {
    uint16_t ppdu_id;
    uint16_t mpdu_bitmap;
    uint16_t frame_ctrl;
    uint16_t qos_ctrl;
    uint32_t mpdu_bitmap_low;
    uint32_t mpdu_bitmap_high;
    uint32_t aggregation :  1,
             gi          :  1,
             bw          :  2,
             stbc        :  2,
             fec         :  1,
             extspatial  :  2,
             length      : 16;
    /*
     * The following elements will have values
     * only for MU
     */
    uint32_t length_user0;
    uint32_t length_user1;
    uint32_t length_user2;
    struct ieee80211_radiotap_header rthdr;
    uint8_t rthdr_data[MAX_RADIOTAP_DATA_SIZE];
};

struct ath_pktlog_siginfo {
    uint32_t tsf;
    uint32_t l_sig;
    uint32_t ht_vht_sig1;
    uint32_t ht_vht_sig2;
    uint32_t vht_sig_b_0;
    uint32_t vht_sig_b_1;
    uint32_t vht_sig_b_2;
    uint16_t rssi;
    uint8_t  preamble_type;
    uint8_t  nss;
    uint8_t  data_rate;
    uint8_t  ldpc;
};
#endif  /* _PKTLOG_FMT_H_ */
