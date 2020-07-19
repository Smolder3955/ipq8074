/*
 * Copyright (c) 2010, Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */

#ifndef _AH_RADIOTAP_H_
#define _AH_RADIOTAP_H_

#include "qdf_types.h" /* qdf_packed */

#ifndef ATH_OUI
#define ATH_OUI 0x7f0300 /* Atheros OUI */
#endif

#define AH_RADIOTAP_VERSION 0
#define RADIOTAP_MAX_CHAINS 3

/* radiotap flags */
#define AH_RADIOTAP_F_BADFCS          0x00000040  /* frame failed crc check */

/* radiotap rx flags */
#define AH_RADIOTAP_F_RX_BADFCS          0x00000001  /* frame failed crc check - (deprecated?) */
#define AH_RADIOTAP_11NF_RX_HALFGI       0x00000002  /* half-gi used */
#define AH_RADIOTAP_11NF_RX_40           0x00000004  /* 40MHz Recv */
#define AH_RADIOTAP_11NF_RX_40P          0x00000008  /* Parallel 40MHz Recv */
#define AH_RADIOTAP_11NF_RX_AGGR         0x00000010  /* Frame is part of aggr */
#define AH_RADIOTAP_11NF_RX_MOREAGGR     0x00000020  /* Aggr sub-frames follow */
#define AH_RADIOTAP_11NF_RX_MORE         0x00000040  /* Non-aggr sub-frames follow */
#define AH_RADIOTAP_11NF_RX_PREDELIM_CRCERR  0x00000080  /* Bad delimiter CRC */
#define AH_RADIOTAP_11NF_RX_POSTDELIM_CRCERR 0x00000100  /* Bad delimiter CRC after this frame */
#define AH_RADIOTAP_11NF_RX_PHYERR        0x00000200  /* PHY error */
#define AH_RADIOTAP_11NF_RX_DECRYPTCRCERR 0x00000400 /* Decrypt CRC error */
#define AH_RADIOTAP_11NF_RX_FIRSTAGGR     0x00000800  /* First sub-frame in Aggr */
#define AH_RADIOTAP_11NF_RX_PLCP          0x00001000  /* PLCP header */
#define AH_RADIOTAP_11NF_RX_STBC          0x00002000  /* STBC used */

struct ah_rx_radiotap_per_chain_info {
    u_int16_t   num_chains; /* Indicates no. of valid chains */
    u_int8_t    rssi_ctl[RADIOTAP_MAX_CHAINS]; /* RSSI on ctrl channel of each chain */
    u_int8_t    rssi_ext[RADIOTAP_MAX_CHAINS]; /* RSSI on ctrl channel of each chain */
    u_int32_t   evm[RADIOTAP_MAX_CHAINS];
};

struct vendor_space {
    u_int8_t    oui[3]; /* Organizationally Unique ID, i.e. vendor ID */
    u_int8_t    sub_namespace;
    u_int8_t    skip_length;
};

struct ah_rx_vendor_radiotap_header {
    /****
     * N.B. for backwards compatibility, always leave "version"
     * as the initial element.
     ****/
    u_int32_t   version;

    /****
     * N.B. If ah_rx_vendor_radiotap_header is modified by adding new
     * fields at the end of the struct, it is not necessary to change
     * AH_RADIOTAP_VERSION.
     * If any existing fields are modified, moved, or deleted, it is
     * necessary to increase AH_RADIOTAP_VERSION.
     ****/
    struct ah_rx_radiotap_per_chain_info ch_info; /* AH_RADIOTAP_11N_CH0_INFO */
    u_int8_t    n_delims;            /* AH_RADIOTAP_11N_NUM_DELIMS */
    u_int8_t    phyerr_code;         /* AH_RADIOTAP_11N_PHYERR_CODE */
};

struct wr_struct_mcs {
	u_int8_t	known;
    u_int8_t    flags;
    u_int8_t    mcs;
} __packed;

struct wr_struct_xchannel {
    u_int32_t   flags;
    u_int16_t   freq;
    u_int8_t    chan;
    u_int8_t    maxpower;
};

struct ah_rx_radiotap_header {
    /* The following fields correspond to the bits in it_present bitmask in
       the above header. See the definitions of field types indicated against
       each field */
    u_int64_t                   tsf;          /* AH_RADIOTAP_DB_TSFT */
    u_int8_t                    wr_flags;     /* AH_RADIOTAP_FLAGS */
    u_int8_t                    wr_rate;      /* AH_RADIOTAP_RATE */
    u_int16_t                   wr_chan_freq; /* AH_RADIOTAP_CHANNEL */
    u_int16_t                   wr_chan_flags; /* AH_RADIOTAP_CHANNEL (part 2) */
    int8_t                    wr_antsignal_dbm; /* AH_RADIOTAP_ANTSIGNAL_DBM */
    u_int8_t                    wr_antsignal; /* AH_RADIOTAP_DB_ANTSIGNAL */
    u_int16_t                   wr_rx_flags;  /* AH_RADIOTAP_RX_FLAGS */
    /* NOTE: This way of alignment is not compiler independent. Need to fix this. */
    u_int16_t                   wr_dummy;     /* 2 bytes dummy so that the next field wr_xchannel has 4 bytes alignment */
    struct wr_struct_xchannel   wr_xchannel;  /*  4 byte alignment is needed.s */
    struct wr_struct_mcs        wr_mcs;       /* */
    struct vendor_space vendor_ext; /* AH_RADIOTAP_VENDOR_SPECIFIC */
    struct ah_rx_vendor_radiotap_header ath_ext;
};

struct ah_tx_radiotap_header {
    u_int8_t    wt_flags;       /* XXX for padding */
    u_int8_t    wt_rate;
    u_int8_t    wt_txpower;
    u_int8_t    wt_antenna;
    u_int16_t   wt_tx_flags;
    u_int8_t    wt_rts_retries;
    u_int8_t    wt_data_retries;
};

#define RADIOTAP_MCS_BW_MASK            0x03
#define RADIOTAP_MCS_FLAGS_20MHZ        0x00

#define RADIOTAP_MCS_FLAGS_40MHZ        0x01
#define RADIOTAP_MCS_FLAGS_20MHZ_L      0x02
#define RADIOTAP_MCS_FLAGS_20MHZ_U      0x03

#define RADIOTAP_MCS_FLAGS_SHORT_GI     0x04
#define RADIOTAP_MCS_FLAGS_FMT_GF       0x08
#define RADIOTAP_MCS_FLAGS_FEC_LDPC     0x10
#define RADIOTAP_MCS_FLAGS_20MHZ_L      0x02
#define RADIOTAP_MCS_FLAGS_20MHZ_U      0x03
#define RADIOTAP_MCS_FLAGS_SHORT_GI     0x04

#define RADIOTAP_MCS_KNOWN_BW			0x01
#define RADIOTAP_MCS_KNOWN_MCS			0x02
#define RADIOTAP_MCS_KNOWN_GI			0x04
#define RADIOTAP_MCS_KNOWN_FMT			0x08
#define RADIOTAP_MCS_KNOWN_FEC			0x10


/* PPI Interface Data structures */

#ifdef WIN32
#pragma pack(push, ah_ppi_pfield_common, 1)
#endif
struct ah_ppi_pfield_common {
    u_int64_t               common_tsft;
    u_int16_t               common_flags;
    u_int16_t               common_rate;
    u_int16_t               common_chan_freq;
    u_int16_t               common_chan_flags;
    u_int8_t                common_fhssHopset;
    u_int8_t                common_fhssPattern;
    u_int8_t                common_dbm_ant_signal;
    u_int8_t                common_dBmAntNoise;       
} qdf_packed;
#ifdef WIN32
#pragma pack(pop, ah_ppi_pfield_common)
#endif

#ifdef WIN32
#pragma pack(push, ah_ppi_pfield_mac_extensions, 1)
#endif
struct ah_ppi_pfield_mac_extensions {
    u_int32_t               mac_flags;
    u_int32_t               mac_ampduId;
    u_int8_t                mac_delimiters;
    u_int8_t                mac_rsvd[3];
} qdf_packed;
#ifdef WIN32
#pragma pack(pop, ah_ppi_pfield_mac_extensions)
#endif

#ifdef WIN32
#pragma pack(push, ah_ppi_pfield_macphy_extensions, 1)
#endif
struct ah_ppi_pfield_macphy_extensions {
    u_int32_t               macphy_flags;
    u_int32_t               macphy_ampdu_id;
    u_int8_t                macphy_delimiters;
    u_int8_t                macphy_mcs;
    u_int8_t                macphy_num_streams;
    u_int8_t                macphy_rssi_combined;
    u_int8_t                macphy_rssi_ant0_ctl;
    u_int8_t                macphy_rssi_ant1_ctl;
    u_int8_t                macphy_rssi_ant2_ctl;
    u_int8_t                macphy_rssiAnt3Ctl;
    u_int8_t                macphy_rssi_ant0_ext;
    u_int8_t                macphy_rssi_ant1_ext;
    u_int8_t                macphy_rssi_ant2_ext;
    u_int8_t                macphy_rssiAnt3Ext;
    u_int16_t               macphy_chan_freq;
    u_int16_t               macphy_chanFlags;
    u_int8_t                macphy_ant0_signal;
    u_int8_t                macphy_ant0_noise;
    u_int8_t                macphy_ant1_signal;
    u_int8_t                macphy_ant1_noise;
    u_int8_t                macphy_ant2_signal;
    u_int8_t                macphy_ant2_noise;
    u_int8_t                macphy_ant3Signal;
    u_int8_t                macphy_ant3Noise;
    u_int32_t               macphy_evm0;
    u_int32_t               macphy_evm1;
    u_int32_t               macphy_evm2;
    u_int32_t               macphy_evm3;
} qdf_packed;
#ifdef WIN32
#pragma pack(pop, ah_ppi_pfield_macphy_extensions)
#endif

#ifdef WIN32
#pragma pack(push, ah_ppi_pfield_spectrum_map, 1)
#endif
struct ah_ppi_pfield_spectrum_map {
    u_int32_t               spectrum_timeStart;
    u_int32_t               spectrum_timeEnd;
    u_int32_t               spectrum_type;    
    u_int32_t               spectrum_freqkHzStart;
    u_int32_t               spectrum_freqkHzEnd;
    u_int16_t               spectrum_numSamples;
    u_int8_t                *spectrum_samples;
} qdf_packed;
#ifdef WIN32
#pragma pack(pop, ah_ppi_pfield_spectrum_map)
#endif

struct ah_ppi_data {
    struct ah_ppi_pfield_common             *ppi_common;
    struct ah_ppi_pfield_mac_extensions     *ppi_mac_ext;
    struct ah_ppi_pfield_macphy_extensions  *ppi_macphy_ext;
    struct ah_ppi_pfield_spectrum_map       *ppi_spectrum_map;
};

#endif /* _AH_RADIOTAP_H_ */
