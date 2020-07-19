/*
 * Copyright (c) 2011, 2017 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * Copyright (c) 2010, Atheros Communications Inc.
 * 
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
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
#define __ATTRIB_PACK_PKTLOG_WAR
#define __ATTRIB_PACK __attribute__ ((packed))
#endif
#endif

/* Each packet log entry consists of the following fixed length header
   followed by variable length log information determined by log_type */
struct ath_pktlog_hdr {
    u_int32_t flags;    /* See flags defined below */
    u_int16_t log_type; /* Type of log information foll this header */
    int16_t size;       /* Size of variable length log information in bytes */
    u_int32_t timestamp;
#ifdef CONFIG_AR900B_SUPPORT
    u_int32_t type_specific_data;
#endif
} __ATTRIB_PACK;

/*******************
Pktlog flag field details
misc_size  [3:0]
mac_rev    [11:4]
proto_info [15:12]
mac_vers   [30:16]
intr_context[31]
*********************/
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
#define ATH_PKTLOG_TX       0x000000001
#define ATH_PKTLOG_RX       0x000000002
#define ATH_PKTLOG_RCFIND   0x000000004
#define ATH_PKTLOG_RCUPDATE 0x000000008
#define ATH_PKTLOG_ANI      0x000000010
#define ATH_PKTLOG_TEXT     0x000000020
#define ATH_PKTLOG_PHYERR   0x000000040
#define ATH_PKTLOG_PROMISC  0x000000080
#define ATH_PKTLOG_CBF      0x000000100
#define ATH_PKTLOG_H_INFO   0x000000200
#define ATH_PKTLOG_STEERING 0x000000400
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
#define PKTLOG_TYPE_TXCTL    0
#define PKTLOG_TYPE_TXSTATUS 1
#define PKTLOG_TYPE_RX       2
#define PKTLOG_TYPE_RCFIND   3
#define PKTLOG_TYPE_RCUPDATE 4
#define PKTLOG_TYPE_ANI      5
#define PKTLOG_TYPE_TEXT     6
#define PKTLOG_TYPE_RX_CBF   10
#define PKTLOG_TYPE_GRPID    12

#define PKTLOG_MAX_TXCTL_WORDS 12
#define PKTLOG_MAX_TXSTATUS_WORDS 10
#define PKTLOG_MAX_PROTO_WORDS  16

struct ath_pktlog_txctl {
    u_int16_t framectrl;       /* frame control field from header */
    u_int16_t seqctrl;         /* frame control field from header */
    u_int16_t bssid_tail;      /* last two octets of bssid */
    u_int16_t sa_tail;         /* last two octets of SA */
    u_int16_t da_tail;         /* last two octets of DA */
    u_int16_t resvd;
    u_int32_t txdesc_ctl[PKTLOG_MAX_TXCTL_WORDS];     /* Tx descriptor words */
    u_int32_t *proto_hdr;   /* protocol header (variable length!) */
    u_int32_t *misc;         /* Can be used for HT specific or other misc info */
} __ATTRIB_PACK;


struct ath_pktlog_txstatus {
    u_int32_t txdesc_status[PKTLOG_MAX_TXSTATUS_WORDS]; /* Tx descriptor status words */
    int32_t misc[0];         /* Can be used for HT specific or other misc info */
} __ATTRIB_PACK;


#define PKTLOG_MAX_RXSTATUS_WORDS 11

struct ath_pktlog_rx {
    u_int16_t framectrl;       /* frame control field from header */
    u_int16_t seqctrl;         /* sequence control field */
    u_int16_t bssid_tail;      /* last two octets of bssid */
    u_int16_t sa_tail;         /* last two octets of SA */
    u_int16_t da_tail;         /* last two octets of DA */
    u_int16_t resvd;
    u_int32_t rxdesc_status[PKTLOG_MAX_RXSTATUS_WORDS];  /* Rx descriptor words */
    u_int32_t *proto_hdr;   /* protocol header (variable length!) */
    u_int32_t *misc;         /* Can be used for HT specific or other misc info */
} __ATTRIB_PACK;

struct ath_pktlog_cbf_submix {
    u_int8_t   sub_type;    /*1-->cbf(RX_EXPLICIT_CV_TRANSFER), 2-->H-info(RX_IMPLICIT_CV_TRANSFER), 3-->Steering*/
    u_int16_t  reserved;
    u_int8_t   frag_flag;   /*0-->No fragment, 1-->fragmented*/
}__ATTRIB_PACK;

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
//#ifdef __ATTRIB_PACK
#ifdef __ATTRIB_PACK_PKTLOG_WAR
#undef __ATTRIB_PACK
#endif   /* __ATTRIB_PACK */

/* The following header is included in the beginning of the file, followed by 
   log entries when the log buffer is read through procfs */

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

#define PKTLOG_MOV_RD_IDX(_rd_offset, _log_buf, _log_size)  \
    do { \
        if((_rd_offset + sizeof(struct ath_pktlog_hdr) + \
            ((struct ath_pktlog_hdr *)((_log_buf)->log_data + \
            (_rd_offset)))->size) <= _log_size) { \
            _rd_offset = ((_rd_offset) + sizeof(struct ath_pktlog_hdr) + \
                            ((struct ath_pktlog_hdr *)((_log_buf)->log_data + \
                            (_rd_offset)))->size); \
        } else { \
            _rd_offset = ((struct ath_pktlog_hdr *)((_log_buf)->log_data +  \
                                           (_rd_offset)))->size;  \
        } \
        (_rd_offset) = (((_log_size) - (_rd_offset)) >= \
                         sizeof(struct ath_pktlog_hdr)) ? _rd_offset:0;\
    } while(0)

#define PKTLOG_MOV_RD_IDX_HDRSIZE(_rd_offset, _log_buf, _log_size, _pktlog_hdr_size)  \
    do { \
        if((_rd_offset + _pktlog_hdr_size + \
            ((struct ath_pktlog_hdr *)((_log_buf)->log_data + \
            (_rd_offset)))->size) <= _log_size) { \
            _rd_offset = ((_rd_offset) + _pktlog_hdr_size + \
                            ((struct ath_pktlog_hdr *)((_log_buf)->log_data + \
                            (_rd_offset)))->size); \
        } else { \
            _rd_offset = ((struct ath_pktlog_hdr *)((_log_buf)->log_data +  \
                                           (_rd_offset)))->size;  \
        } \
        (_rd_offset) = (((_log_size) - (_rd_offset)) >= \
                         _pktlog_hdr_size) ? _rd_offset:0;\
    } while(0)

#endif  /* _PKTLOG_FMT_H_ */
