/*
 * Copyright (c) 2012 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
#ifndef _VOW_DEFINES__H_
#define _VOW_DEFINES__H_


#define UDP_CKSUM_OFFSET   40 /* UDP check sum offset in network buffer */
#define RTP_HDR_OFFSET  42    /* RTP header offset in network buffer */
#define EXT_HDR_OFFSET 54     /* Extension header offset in network buffer */
#define UDP_PDU_RTP_EXT  0x90   /* ((2 << 6) | (1 << 4)) RTP Version 2 + X bit */
#define IP_VER4_N_NO_EXTRA_HEADERS 0x45 
#define IPERF3_DATA_OFFSET 12  /* iperf3 data offset from EXT_HDR_OFFSET */
#define HAL_RX_40  0x08 /* 40 Mhz */
#define HAL_RX_GI  0x04    /* full gi */

#define AR600P_ASSEMBLE_HW_RATECODE(_rate, _nss, _pream)     \
    (((_pream) << 6) | ((_nss) << 4) | (_rate))

struct vow_extstats {
    u_int8_t rx_rssi_ctl0; /* control channel chain0 rssi */
    u_int8_t rx_rssi_ctl1; /* control channel chain1 rssi */
    u_int8_t rx_rssi_ctl2; /* control channel chain2 rssi */
    u_int8_t rx_rssi_ext0; /* extention channel chain0 rssi */
    u_int8_t rx_rssi_ext1; /* extention channel chain1 rssi */
    u_int8_t rx_rssi_ext2; /* extention channel chain2 rssi */
    u_int8_t rx_rssi_comb; /* combined RSSI value */
    u_int8_t rx_bw;   /* Band width 0-20, 1-40, 2-80 */
    u_int8_t rx_sgi;  /* Guard interval, 0-Long GI, 1-Short GI */
    u_int8_t rx_nss;  /* Number of spatial streams */
    u_int8_t rx_mcs;  /* Rate MCS value */
    u_int8_t rx_ratecode; /* Hardware rate code */
    u_int8_t rx_rs_flags;  /* Recieve misc flags */
    u_int8_t rx_moreaggr;  /* 0 - non aggr frame */
    u_int32_t rx_macTs;     /* Time stamp */
    u_int16_t rx_seqno;     /* rx sequence number*/
};

#endif /* _VOW_DEFINES__H_ */
