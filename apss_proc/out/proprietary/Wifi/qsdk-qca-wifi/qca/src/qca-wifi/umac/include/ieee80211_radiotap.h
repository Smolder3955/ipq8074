/*
* Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * Notifications and licenses are retained for attribution purposes only.
 */
/* $FreeBSD: src/sys/net80211/ieee80211_radiotap.h,v 1.5 2005/01/22 20:12:05 sam Exp $ */
/* $NetBSD: ieee80211_radiotap.h,v 1.10 2005/01/04 00:34:58 dyoung Exp $ */

/*-
 * Copyright (c) 2003, 2004 David Young.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of David Young may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY DAVID YOUNG ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL DAVID
 * YOUNG BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 */
#ifndef _NET_IF_IEEE80211RADIOTAP_H_
#define _NET_IF_IEEE80211RADIOTAP_H_

/* A generic radio capture format is desirable. There is one for
 * Linux, but it is neither rigidly defined (there were not even
 * units given for some fields) nor easily extensible.
 *
 * I suggest the following extensible radio capture format. It is
 * based on a bitmap indicating which fields are present.
 *
 * I am trying to describe precisely what the application programmer
 * should expect in the following, and for that reason I tell the
 * units and origin of each measurement (where it applies), or else I
 * use sufficiently weaselly language ("is a monotonically nondecreasing
 * function of...") that I cannot set false expectations for lawyerly
 * readers.
 */

#include "ah_radiotap.h" /* ah_rx_radiotap_header */

/* XXX tcpdump/libpcap do not tolerate variable-length headers,
 * yet, so we pad every radiotap header to 64 bytes. Ugh.
 */
#define IEEE80211_RADIOTAP_HDRLEN   64

/* The radio capture header precedes the 802.11 header. */
struct ieee80211_radiotap_header {
    u_int8_t    it_version; /* Version 0. Only increases
                     * for drastic changes,
                     * introduction of compatible
                     * new fields does not count.
                     */
    u_int8_t    it_pad;
    u_int16_t   it_len; /* length of the whole
                     * header in bytes, including
                     * it_version, it_pad,
                     * it_len, and data fields.
                     */
    u_int32_t   it_present; /* A bitmap telling which
                     * fields are present. Set bit 31
                     * (0x80000000) to extend the
                     * bitmap by another 32 bits.
                     * Additional extensions are made
                     * by setting bit 31.
                     */
#if ATH_SUPPORT_WIRESHARK_EXTENSION
    u_int32_t   it_present_ext;  /* vendor extension */
#endif
} __attribute__((__packed__));

/* Name                                 Data type       Units
 * ----                                 ---------       -----
 *
 * IEEE80211_RADIOTAP_TSFT              u_int64_t       microseconds
 *
 *      Value in microseconds of the MAC's 64-bit 802.11 Time
 *      Synchronization Function timer when the first bit of the
 *      MPDU arrived at the MAC. For received frames, only.
 *
 * IEEE80211_RADIOTAP_CHANNEL           2 x u_int16_t   MHz, bitmap
 *
 *      Tx/Rx frequency in MHz, followed by flags (see below).
 *
 * IEEE80211_RADIOTAP_FHSS              u_int16_t       see below
 *
 *      For frequency-hopping radios, the hop set (first byte)
 *      and pattern (second byte).
 *
 * IEEE80211_RADIOTAP_RATE              u_int8_t        500kb/s
 *
 *      Tx/Rx data rate
 *
 * IEEE80211_RADIOTAP_DBM_ANTSIGNAL     int8_t          decibels from
 *                                                      one milliwatt (dBm)
 *
 *      RF signal power at the antenna, decibel difference from
 *      one milliwatt.
 *
 * IEEE80211_RADIOTAP_DBM_ANTNOISE      int8_t          decibels from
 *                                                      one milliwatt (dBm)
 *
 *      RF noise power at the antenna, decibel difference from one
 *      milliwatt.
 *
 * IEEE80211_RADIOTAP_DB_ANTSIGNAL      u_int8_t        decibel (dB)
 *
 *      RF signal power at the antenna, decibel difference from an
 *      arbitrary, fixed reference.
 *
 * IEEE80211_RADIOTAP_DB_ANTNOISE       u_int8_t        decibel (dB)
 *
 *      RF noise power at the antenna, decibel difference from an
 *      arbitrary, fixed reference point.
 *
 * IEEE80211_RADIOTAP_LOCK_QUALITY      u_int16_t       unitless
 *
 *      Quality of Barker code lock. Unitless. Monotonically
 *      nondecreasing with "better" lock strength. Called "Signal
 *      Quality" in datasheets.  (Is there a standard way to measure
 *      this?)
 *
 * IEEE80211_RADIOTAP_TX_ATTENUATION    u_int16_t       unitless
 *
 *      Transmit power expressed as unitless distance from max
 *      power set at factory calibration.  0 is max power.
 *      Monotonically nondecreasing with lower power levels.
 *
 * IEEE80211_RADIOTAP_DB_TX_ATTENUATION u_int16_t       decibels (dB)
 *
 *      Transmit power expressed as decibel distance from max power
 *      set at factory calibration.  0 is max power.  Monotonically
 *      nondecreasing with lower power levels.
 *
 * IEEE80211_RADIOTAP_DBM_TX_POWER      int8_t          decibels from
 *                                                      one milliwatt (dBm)
 *
 *      Transmit power expressed as dBm (decibels from a 1 milliwatt
 *      reference). This is the absolute power level measured at
 *      the antenna port.
 *
 * IEEE80211_RADIOTAP_FLAGS             u_int8_t        bitmap
 *
 *      Properties of transmitted and received frames. See flags
 *      defined below.
 *
 * IEEE80211_RADIOTAP_ANTENNA           u_int8_t        antenna index
 *
 *      Unitless indication of the Rx/Tx antenna for this packet.
 *      The first antenna is antenna 0.
 *
 *
 * IEEE80211_RADIOTAP_RX_FLAGS          u_int16_t       bitmap
 *
 *  Properties of received frames. See flags defined below.
 *
 * IEEE80211_RADIOTAP_11N_CHAIN_INFO    per_chain_info  See definition
 *
 *  Per chain information (RSSI, EVM)
 *
 * IEEE80211_RADIOTAP_11N_NUM_DELIMS    u_int8_t        count
 *
 * Number of delimiters before aggregate sub-frame
 *
 * IEEE80211_RADIOTAP_11N_PHYERR_CODE   u_int8_t        data
 *
 * PHY err code in case of phy error
 */
enum ieee80211_radiotap_type {
    IEEE80211_RADIOTAP_TSFT              = 0,
    IEEE80211_RADIOTAP_FLAGS             = 1,
    IEEE80211_RADIOTAP_RATE              = 2,
    IEEE80211_RADIOTAP_CHANNEL           = 3,
    IEEE80211_RADIOTAP_FHSS              = 4,
    IEEE80211_RADIOTAP_DBM_ANTSIGNAL     = 5,
    IEEE80211_RADIOTAP_DBM_ANTNOISE      = 6,
    IEEE80211_RADIOTAP_LOCK_QUALITY      = 7,
    IEEE80211_RADIOTAP_TX_ATTENUATION    = 8,
    IEEE80211_RADIOTAP_DB_TX_ATTENUATION = 9,
    IEEE80211_RADIOTAP_DBM_TX_POWER      = 10,
    IEEE80211_RADIOTAP_ANTENNA           = 11,
    IEEE80211_RADIOTAP_DB_ANTSIGNAL      = 12,
    IEEE80211_RADIOTAP_DB_ANTNOISE       = 13,
    IEEE80211_RADIOTAP_RX_FLAGS          = 14,
    IEEE80211_RADIOTAP_XCHANNEL          = 18,
    IEEE80211_RADIOTAP_MCS               = 19,
    IEEE80211_RADIOTAP_AMPDU_STATUS      = 20,
    IEEE80211_RADIOTAP_VHT               = 21,

    IEEE80211_RADIOTAP_RADIOTAP_NAMESPACE = 29,
    /*
     * Put Atheros-specific flags in a separate word, to minimize
     * the possibility of our flags colliding with future additions
     * to standard wireshark flags.
     * Use bit 30 to indicate whether our flags are present.
     */
    IEEE80211_RADIOTAP_VENDOR_SPECIFIC   = 30,
    IEEE80211_RADIOTAP_EXT               = 31
};
enum ieee80211_ath_radiotap_type {
    IEEE80211_RADIOTAP_11N_CHAIN_INFO  = 0,
    IEEE80211_RADIOTAP_11N_NUM_DELIMS  = 1,
    IEEE80211_RADIOTAP_11N_PHYERR_CODE = 2
};


#if !defined(_KERNEL)
#ifndef _NET80211__IEEE80211_H_
/* Channel flags. */
#define    IEEE80211_CHAN_TURBO   0x0010    /* Turbo channel */
#define    IEEE80211_CHAN_CCK     0x0020    /* CCK channel */
#define    IEEE80211_CHAN_OFDM    0x0040    /* OFDM channel */
#define    IEEE80211_CHAN_2GHZ    0x0080    /* 2 GHz spectrum channel. */
#define    IEEE80211_CHAN_5GHZ    0x0100    /* 5 GHz spectrum channel */
#define    IEEE80211_CHAN_PASSIVE 0x0200    /* Only passive scan allowed */
#define    IEEE80211_CHAN_DYN     0x0400    /* Dynamic CCK-OFDM channel */
#define    IEEE80211_CHAN_GFSK    0x0800    /* GFSK channel (FHSS PHY) */
#endif /*these macros redefined in include/_ieee80211.h*/
#endif /* !_KERNEL */

/* For IEEE80211_RADIOTAP_FLAGS */
#define IEEE80211_RADIOTAP_F_CFP    0x01    /* sent/received
                                             * during CFP
                                             */
#define IEEE80211_RADIOTAP_F_SHORTPRE   0x02    /* sent/received
                                                 * with short
                                                 * preamble
                                                 */
#define IEEE80211_RADIOTAP_F_WEP    0x04    /* sent/received
                                             * with WEP encryption
                                             */
#define IEEE80211_RADIOTAP_F_FRAG   0x08    /* sent/received
                                             * with fragmentation
                                             */
#define IEEE80211_RADIOTAP_F_FCS    0x10    /* frame includes FCS */
#define IEEE80211_RADIOTAP_F_PREAM  0x02    /* frame to indicate long/short preamble */
#define IEEE80211_RADIOTAP_F_DATAPAD    0x20    /* frame has padding between
                                                 * 802.11 header and payload
                                                 * (to 32-bit boundary)
                                                 */
#define IEEE80211_RADIOTAP_F_BADFCS AH_RADIOTAP_F_BADFCS     /* does not pass FCS check */
#define IEEE80211_RADIOTAP_F_SHORTGI    0x80    /* HT short GI */

/* For IEEE80211_RADIOTAP_RX_FLAGS
   Translate HAL rx radiotap flags into IEEE80211 radiotap flags.
 */
#define IEEE80211_RADIOTAP_F_RX_BADFCS    AH_RADIOTAP_F_RX_BADFCS /* frame failed crc check */
#define IEEE80211_RADIOTAP_11NF_RX_HALFGI AH_RADIOTAP_11NF_RX_HALFGI  /* half-gi used */
#define IEEE80211_RADIOTAP_11NF_RX_40     AH_RADIOTAP_11NF_RX_40  /* 40MHz Recv */
#define IEEE80211_RADIOTAP_11NF_RX_40P    AH_RADIOTAP_11NF_RX_40P  /* Parallel 40MHz Recv */
#define IEEE80211_RADIOTAP_11NF_RX_AGGR   AH_RADIOTAP_11NF_RX_AGGR  /* Frame is part of aggr */
#define IEEE80211_RADIOTAP_11NF_RX_MOREAGGR AH_RADIOTAP_11NF_RX_MOREAGGR  /* Aggr sub-frames follow */
#define IEEE80211_RADIOTAP_11NF_RX_MORE   AH_RADIOTAP_11NF_RX_MORE  /* Non-aggr sub-frames follow */
#define IEEE80211_RADIOTAP_11NF_RX_PREDELIM_CRCERR AH_RADIOTAP_11NF_RX_PREDELIM_CRCERR  /* Bad delimiter CRC */
#define IEEE80211_RADIOTAP_11NF_RX_POSTDELIM_CRCERR AH_RADIOTAP_11NF_RX_POSTDELIM_CRCERR  /* Bad delimiter CRC after this frame */
#define IEEE80211_RADIOTAP_11NF_RX_PHYERR  AH_RADIOTAP_11NF_RX_PHYERR  /* PHY error */
#define IEEE80211_RADIOTAP_11NF_RX_DECRYPTCRCERR AH_RADIOTAP_11NF_RX_DECRYPTCRCERR /* Decrypt CRC error */
#define IEEE80211_RADIOTAP_11NF_RX_PLCP AH_RADIOTAP_11NF_RX_PLCP  /* PLCP header or EVM */
#define IEEE80211_RADIOTAP_11NF_RX_FIRSTAGGR AH_RADIOTAP_11NF_RX_FIRSTAGGR  /* First sub-frame in Aggr */


/* For IEEE80211_RADIOTAP_TX_FLAGS */
#define IEEE80211_RADIOTAP_F_TX_FAIL    0x0001  /* failed due to excessive
                         * retries */

/*
 * Radio capture format.
 */

#define ATH_RX_RADIOTAP_PRESENT_COMMON (               \
    (1 << IEEE80211_RADIOTAP_TSFT)            | \
    (1 << IEEE80211_RADIOTAP_FLAGS)           | \
    (1 << IEEE80211_RADIOTAP_RATE)            | \
    (1 << IEEE80211_RADIOTAP_CHANNEL)         | \
    (1 << IEEE80211_RADIOTAP_DB_ANTSIGNAL)    | \
    (1 << IEEE80211_RADIOTAP_DBM_ANTSIGNAL)   | \
    (1 << IEEE80211_RADIOTAP_RX_FLAGS)        | \
    (1 << IEEE80211_RADIOTAP_AMPDU_STATUS)    | \
    (1 << IEEE80211_RADIOTAP_XCHANNEL)        | \
    (1 << IEEE80211_RADIOTAP_MCS)             | \
    0)
 
#if ATH_SUPPORT_WIRESHARK_EXTENSION
#define ATH_RX_RADIOTAP_PRESENT (               \
    ATH_RX_RADIOTAP_PRESENT_COMMON            | \
    (1 << IEEE80211_RADIOTAP_VENDOR_SPECIFIC) | \
    (1 << IEEE80211_RADIOTAP_EXT)             | \
    0)
#else
#define ATH_RX_RADIOTAP_PRESENT                 ATH_RX_RADIOTAP_PRESENT_COMMON
#endif

#define ATH_RX_RADIOTAP_ATHEROS_EXT (           \
    (1 << IEEE80211_RADIOTAP_11N_CHAIN_INFO)  | \
    (1 << IEEE80211_RADIOTAP_11N_NUM_DELIMS)  | \
    (1 << IEEE80211_RADIOTAP_11N_PHYERR_CODE) | \
    0)

struct ath_rx_radiotap_header {
    struct ieee80211_radiotap_header wr_ihdr;
	struct ah_rx_radiotap_header wr_hal;
};

#define ATH_TX_RADIOTAP_PRESENT (               \
    (1 << IEEE80211_RADIOTAP_FLAGS)           | \
    (1 << IEEE80211_RADIOTAP_RATE)            | \
    (1 << IEEE80211_RADIOTAP_DBM_TX_POWER)    | \
    (1 << IEEE80211_RADIOTAP_ANTENNA)         | \
    0)

struct ath_tx_radiotap_header {
    struct ieee80211_radiotap_header wt_ihdr;
    struct ah_tx_radiotap_header wt_hal;
};


//
//  Data structures to support the PPI capture interface. 
//

struct ieee80211_ppi_packetheader {
    u_int8_t     pph_version;             /* Version, Currently 0 */
    u_int8_t     pph_flags;               /* Flags */
    u_int16_t    pph_len;                 /* Len of entire msg including this header and tlv payload */
    u_int32_t    pph_dlt;                 /* datalink type of captured packet */
}__attribute__((__packed__));

#define IEEE80211_RADIO_PPI_VERSION         0
#define IEEE80211_PPH_FLAGS_32BITALIGNED    1
/*
ieee80211_ppi_packetheader

pph_version
    The version of the PPI header. MUST be set to zero (0).
pph_flags
    An 8-bit mask that defines the behavior of the header. The following values are defined:
pph_len
    The length of the entire PPI header, including the packet header and fields. It MUST be between 4 and 65,532 inclusive.
pph_dlt
    This MUST contain a valid data link type as defined in pcap-bpf.h from the libpcap distribution. If an official DLT registry is ever created by the libpcap development team, then it will supersede this list.
    A capture facility can implement per-packet DLTs by setting pph_version to 0, pph_flags to 0, pph_len to 8, and pph_dlt to the DLT of the encapsulated packet.
*/

struct ieee80211_ppi_fieldheader {
    u_int16_t   pfh_type;               /* Type */
    u_int16_t   pfh_datalen;            /* Length of data */
}__attribute__((__packed__));


/* 
ieee80211_ppi_fieldheader

pfh_type
    The type of data following the field header. Must be a valid type as defined below
          0-29999           General Purpose field.
          30000-65535    Vendor Specific fields.

pfh_datalen 
    Length of data in bytes that follows, must be between 0-65520. End of data must not exceed header len.
*/


/* Field Type definition for PPI. 0, 1 and 8-29999are reserved */

#define IEEE80211_PFHTYPE_COMMON                2
#define IEEE80211_PFHTYPE_MAC_EXTENSIONS        3
#define IEEE80211_PFHTYPE_MACPHY_EXTENSIONS     4
#define IEEE80211_PFHTYPE_SPECTRUM_MAP          5
#define IEEE80211_PFHTYPE_PROCESS_INFO          6
#define IEEE80211_PFHTYPE_CAPTURE_INFO          7

struct ieee80211_ppi_hdr_data {
    struct ieee80211_ppi_packetheader       ppi_pktheader;
    struct ieee80211_ppi_fieldheader        ppi_common_fields_hdr;
    struct ah_ppi_pfield_common             ppi_common_fields;
    struct ieee80211_ppi_fieldheader        ppi_mac_extensions_hdr;
    struct ah_ppi_pfield_mac_extensions     ppi_mac_extensions;
    struct ieee80211_ppi_fieldheader        ppi_macphy_extensions_hdr;
    struct ah_ppi_pfield_macphy_extensions  ppi_macphy_extensions;
}__attribute__((__packed__));


/* For IEEE80211_RADIOTAP_MCS */
#define IEEE80211_RADIOTAP_MCS_HAVE_BW      0x01
#define IEEE80211_RADIOTAP_MCS_HAVE_MCS     0x02
#define IEEE80211_RADIOTAP_MCS_HAVE_GI      0x04
#define IEEE80211_RADIOTAP_MCS_HAVE_FMT     0x08
#define IEEE80211_RADIOTAP_MCS_HAVE_FEC     0x10
#define IEEE80211_RADIOTAP_MCS_HAVE_STBC    0x20
#define IEEE80211_RADIOTAP_MCS_BW_MASK      0x03
#define     IEEE80211_RADIOTAP_MCS_BW_20    0
#define     IEEE80211_RADIOTAP_MCS_BW_40    1
#define     IEEE80211_RADIOTAP_MCS_BW_20L   2
#define     IEEE80211_RADIOTAP_MCS_BW_20U   3
#define IEEE80211_RADIOTAP_MCS_SGI      0x04
#define IEEE80211_RADIOTAP_MCS_FMT_GF       0x08
#define IEEE80211_RADIOTAP_MCS_FEC_LDPC     0x10
#define IEEE80211_RADIOTAP_MCS_STBC_MASK    0x60
#define     IEEE80211_RADIOTAP_MCS_STBC_1   1
#define     IEEE80211_RADIOTAP_MCS_STBC_2   2
#define     IEEE80211_RADIOTAP_MCS_STBC_3   3

#define IEEE80211_RADIOTAP_MCS_STBC_SHIFT   5

/* For IEEE80211_RADIOTAP_AMPDU_STATUS */
#define IEEE80211_RADIOTAP_AMPDU_REPORT_ZEROLEN     0x0001
#define IEEE80211_RADIOTAP_AMPDU_IS_ZEROLEN     0x0002
#define IEEE80211_RADIOTAP_AMPDU_LAST_KNOWN     0x0004
#define IEEE80211_RADIOTAP_AMPDU_IS_LAST        0x0008
#define IEEE80211_RADIOTAP_AMPDU_DELIM_CRC_ERR      0x0010
#define IEEE80211_RADIOTAP_AMPDU_DELIM_CRC_KNOWN    0x0020

/* For IEEE80211_RADIOTAP_VHT */
#define IEEE80211_RADIOTAP_VHT_KNOWN_STBC           0x0001
#define IEEE80211_RADIOTAP_VHT_KNOWN_TXOP_PS_NA         0x0002
#define IEEE80211_RADIOTAP_VHT_KNOWN_GI             0x0004
#define IEEE80211_RADIOTAP_VHT_KNOWN_SGI_NSYM_DIS       0x0008
#define IEEE80211_RADIOTAP_VHT_KNOWN_LDPC_EXTRA_OFDM_SYM    0x0010
#define IEEE80211_RADIOTAP_VHT_KNOWN_BEAMFORMED         0x0020
#define IEEE80211_RADIOTAP_VHT_KNOWN_BANDWIDTH          0x0040
#define IEEE80211_RADIOTAP_VHT_KNOWN_GROUP_ID           0x0080
#define IEEE80211_RADIOTAP_VHT_KNOWN_PARTIAL_AID        0x0100

#define IEEE80211_RADIOTAP_VHT_FLAG_STBC            0x01
#define IEEE80211_RADIOTAP_VHT_FLAG_TXOP_PS_NA          0x02
#define IEEE80211_RADIOTAP_VHT_FLAG_SGI             0x04
#define IEEE80211_RADIOTAP_VHT_FLAG_SGI_NSYM_M10_9      0x08
#define IEEE80211_RADIOTAP_VHT_FLAG_LDPC_EXTRA_OFDM_SYM     0x10
#define IEEE80211_RADIOTAP_VHT_FLAG_BEAMFORMED          0x20


#define MAX_RADIOTAP_LEN  (64)  /*need to use actual size*/
#define DEFAULT_CHAN_NOISE_FLOOR     (-110)
#define DEFAULT_CHAN_REF_NOISE_FLOOR (-96)

/* QCA phy info definitions */
#define PHY_PREAM_TYPE_MASK     (0xf)
#define PHY_PREAM_TYPE_OFDM     (0)
#define PHY_PREAM_TYPE_CCK      (1)
#define PHY_PREAM_TYPE_HT       (2)
#define PHY_PREAM_TYPE_VHT      (3)

#define PHY_RATE_BASE_MASK      (0x4)
#define PHY_RATE_MCS_MASK       (0xf)
#define PHY_RATE_MCS_SHIFT      (4)
#define PHY_RATE_MCS_SHIFT_VHT_MU_BW20    (16)
#define PHY_RATE_MCS_SHIFT_VHT_MU_BW40    (17)
#define PHY_RATE_MCS_SHIFT_VHT_MU_BW80    (19)
#define PHY_RATE_MCS_MASK_HT    (0x7f)
#define PHY_RATE_MCS_MASK_VHT   (0xf)

#define PHY_SIGNALING_SHIFT     (12)    /*static/dynamic signaling*/
#define PHY_SGI_MASK            (0x1)
#define PHY_SGI_SHIFT           (7)
#define PHY_LDPC_MASK           (0x1)
#define PHY_LDPC_SHIFT          (6)
#define PHY_LDPC_SHIFT_VHT      (2)
#define PHY_LDPC_SHIFT_VHT_U0      (2)
#define PHY_LDPC_SHIFT_VHT_U1      (4)
#define PHY_LDPC_SHIFT_VHT_U2      (5)
#define PHY_LDPC_SHIFT_VHT_U3      (6)
#define PHY_STBC_MASK           (0x3)
#define PHY_STBC_MASK_VHT       (0x1)
#define PHY_STBC_SHIFT          (4)
#define PHY_STBC_SHIFT_VHT      (3)
#define PHY_NSS_SHIFT           (3)
#define PHY_GROUPID_MASK        (0x3f)
#define PHY_GROUPID_SHIFT       (4)
#define PHY_TXBF_MASK           (0x1)
#define PHY_TXBF_SHIFT          (8)
#define PHY_PARTIAL_AID_MASK           (0x1ff)
#define PHY_PARTIAL_AID_SHIFT          (13)

#define PHY_BANDWIDTH_MASK      (0x3)   /*b'14..b'13 band witdth*/
#define PHY_BANDWIDTH_SHIFT     (13)
#define PHY_BANDWIDTH_MASK_HT   (0x1)
#define PHY_BANDWIDTH_MASK_VHT  (0x3)
#define PHY_BANDWIDTH_SHIFT_HT  (7)
#define PHY_BANDWIDTH_ENCODE_SHIFT     (20)

#define PHY_SIG_A1_MASK         (0xffff)
#define PHY_SIG_A2_MASK_HT      (0xfff)
#define PHY_SIG_A1_MASK_VHT     (0xffffff)
#define PHY_SIG_A2_MASK_VHT     (0xffffff)
#define PHY_SIG_B_MASK_VHT_BW20     (0x3ffffff)
#define PHY_SIG_B_MASK_VHT_BW40     (0x7ffffff)
#define PHY_SIG_B_MASK_VHT_BW80     (0x1fffffff)

#define PHY_NSTS_MASK        (0x7)
#define PHY_NSTS_SU_SHIFT       (10)
#define PHY_NSTS_MU_SHIFT_U0    (10)
#define PHY_NSTS_MU_SHIFT_U1    (13)
#define PHY_NSTS_MU_SHIFT_U2    (16)
#define PHY_NSTS_MU_SHIFT_U3    (19)


#endif /* _NET_IF_IEEE80211RADIOTAP_H_ */


extern int get_radiotap_total_len(u_int8_t ht, u_int8_t vht);
/* vim: set shiftwidth=4 tabstop=4 expandtab: */
