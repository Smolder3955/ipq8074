/*
 * Copyright (c) 2011, 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */
/*
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * Notifications and licenses are retained for attribution purposes only.
 */
/*-
 * Copyright (c) 2002-2005 Sam Leffler, Errno Consulting, Atheros
 * Communications, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the following conditions are met:
 * 1. The materials contained herein are unmodified and are used
 *    unmodified.
 * 2. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following NO
 *    ''WARRANTY'' disclaimer below (''Disclaimer''), without
 *    modification.
 * 3. Redistributions in binary form must reproduce at minimum a
 *    disclaimer similar to the Disclaimer below and any redistribution
 *    must be conditioned upon including a substantially similar
 *    Disclaimer requirement for further binary redistribution.
 * 4. Neither the names of the above-listed copyright holders nor the
 *    names of any contributors may be used to endorse or promote
 *    product derived from this software without specific prior written
 *    permission.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ''AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT,
 * MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE
 * FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGES.
 *
 */

#ifndef _DEV_ATH_DESC_H
#define _DEV_ATH_DESC_H

#ifdef WIN32
#pragma pack(push, ah_desc, 1)
#endif

/*
 * Transmit descriptor status.  This structure is filled
 * in only after the tx descriptor process method finds a
 * ``done'' descriptor; at which point it returns something
 * other than HAL_EINPROGRESS.
 *
 * Note that ts_antenna may not be valid for all h/w.  It
 * should be used only if non-zero.
 *
 * NOTE: Transmit descriptor status now is superset of descriptor
 *       status information cor all h/w types.
 */
struct ath_tx_status {
    u_int32_t    ts_tstamp;     /* h/w assigned timestamp */
    u_int16_t    ts_seqnum;     /* h/w assigned sequence number */
    u_int8_t     ts_status;     /* frame status, 0 => xmit ok */
    u_int8_t     ts_ratecode;   /* h/w transmit rate code */
    u_int8_t     ts_rateindex;  /* h/w transmit rate index */
    int8_t       ts_rssi;       /* tx ack RSSI */
    u_int8_t     ts_shortretry; /* # short retries */
    u_int8_t     ts_longretry;  /* # long retries */
    u_int8_t     ts_virtcol;    /* virtual collision count */
    u_int8_t     ts_antenna;    /* antenna information */

        /* Additional status information */
    u_int8_t     ts_flags;       /* misc flags */
    int8_t       ts_rssi_ctl0;   /* tx ack RSSI [ctl, chain 0] */
    int8_t       ts_rssi_ctl1;   /* tx ack RSSI [ctl, chain 1] */
    int8_t       ts_rssi_ctl2;   /* tx ack RSSI [ctl, chain 1] */
    int8_t       ts_rssi_ext0;   /* tx ack RSSI [ext, chain 1] */
    int8_t       ts_rssi_ext1;   /* tx ack RSSI [ext, chain 1] */
    int8_t       ts_rssi_ext2;   /* tx ack RSSI [ext, chain 1] */
    u_int8_t     queue_id;       /* queue number */
    u_int16_t    desc_id;        /* descriptor identifier */

    u_int32_t    ba_low;         /* blockack bitmap low */
    u_int32_t    ba_high;        /* blockack bitmap high */
    u_int32_t    evm0;           /* evm bytes */
    u_int32_t    evm1;
    u_int32_t    evm2;

    u_int8_t     ts_txbfstatus;  /* Tx bf status */ 
#ifdef ATH_SUPPORT_TxBF
#define	AR_BW_Mismatch      0x1
#define	AR_Stream_Miss      0x2
#define	AR_CV_Missed        0x4
#define AR_Dest_Miss        0x8
#define AR_Expired          0x10
#define AR_TxBF_Valid_HW_Status    (AR_BW_Mismatch|AR_Stream_Miss|AR_CV_Missed|AR_Dest_Miss|AR_Expired)
#define TxBF_STATUS_Sounding_Complete   0x20
#define TxBF_STATUS_Sounding_Request    0x40
#endif
    u_int8_t     tid;           /*TID of the transmit unit in case of aggregate */
    u_int8_t     ts_txpower;
    u_int8_t     pad[1];         /* pad for 4 byte alignment */
};

#define HAL_TXERR_XRETRY                0x01    /* excessive retries */
#define HAL_TXERR_FILT                  0x02    /* blocked by tx filtering */
#define HAL_TXERR_FIFO                  0x04    /* fifo underrun */
#define HAL_TXERR_XTXOP                 0x08    /* txop exceeded */
#define HAL_TXERR_TIMER_EXPIRED         0x10    /* tx timer expired */
#define HAL_TXERR_BADTID                0x20    /* hardware corrupted TID */

/*
 * ts_flags values
 */
#define HAL_TX_BA               0x01     /* BA seen */
#define HAL_TX_PWRMGMT          0x02     /* pwr mgmt bit set */
#define HAL_TX_DESC_CFG_ERR     0x04     /* Error in 20/40 desc config */
#define HAL_TX_DATA_UNDERRUN    0x08     /* Tx buffer underrun */
#define HAL_TX_DELIM_UNDERRUN   0x10     /* Tx delimiter underrun */
#if ATH_SUPPORT_WIFIPOS
#define HAL_TX_FAST_TS          0x20     /* Fast timestamp for positioning */
#endif
#define HAL_TX_SW_ABORTED       0x40     /* aborted by software */
#define HAL_TX_SW_FILTERED      0x80     /* filtered by software */

#define HAL_IS_TX_UNDERRUN(_ptxstat) \
        ((_ptxstat)->ts_flags & (HAL_TX_DATA_UNDERRUN | HAL_TX_DELIM_UNDERRUN))

/*
 * Receive descriptor status.  This structure is filled
 * in only after the rx descriptor process method finds a
 * ``done'' descriptor; at which point it returns something
 * other than HAL_EINPROGRESS.
 *
 * If rx_status is zero, then the frame was received ok;
 * otherwise the error information is indicated and rs_phyerr
 * contains a phy error code if HAL_RXERR_PHY is set.  In general
 * the frame contents is undefined when an error occurred thought
 * for some errors (e.g. a decryption error), it may be meaningful.
 *
 * Note that the receive timestamp is expanded using the TSF to
 * 15 bits (regardless of what the h/w provides directly).
 *
 * rx_rssi is in units of dbm above the noise floor.  This value
 * is measured during the preamble and PLCP; i.e. with the initial
 * 4us of detection.  The noise floor is typically a consistent
 * -96dBm absolute power in a 20MHz channel.
 */
struct ath_rx_status {
    u_int32_t   rs_tstamp;    /* h/w assigned timestamp */
    u_int16_t   rs_datalen;    /* rx frame length */
    u_int8_t    rs_status;    /* rx status, 0 => recv ok */
    u_int8_t    rs_phyerr;    /* phy error code */
    u_int8_t    rs_rssi;    /* rx frame RSSI */

    u_int8_t    rs_keyix;    /* key cache index */
    u_int8_t    rs_rate;    /* h/w receive rate index */
#if UNIFIED_SMARTANTENNA
    u_int32_t    rs_antenna;    /* antenna information */
#else        
    u_int8_t    rs_antenna;    /* antenna information */
#endif    
    u_int8_t    rs_more;    /* more descriptors follow */

        /* Additional receive status */
    u_int8_t    rs_rssi_ctl0;    /* rx frame RSSI [ctl, chain 0] */
    u_int8_t    rs_rssi_ctl1;    /* rx frame RSSI [ctl, chain 1] */
    u_int8_t    rs_rssi_ctl2;    /* rx frame RSSI [ctl, chain 2] */
    u_int8_t    rs_rssi_ext0;    /* rx frame RSSI [ext, chain 0] */
    u_int8_t    rs_rssi_ext1;    /* rx frame RSSI [ext, chain 1] */
    u_int8_t    rs_rssi_ext2;    /* rx frame RSSI [ext, chain 2] */

    u_int8_t    rs_isaggr;      /* is part of the aggregate */
    u_int8_t    rs_moreaggr;    /* more frames in aggr to follow */
    u_int8_t    rs_isapsd;      /* is apsd trigger frame */
    u_int8_t    rs_num_delims;  /* number of delims in aggr */
    u_int8_t    rs_flags;       /* misc flags */
    u_int32_t   evm0;
    u_int32_t   evm1;           /* evm bytes */
    u_int32_t   evm2;
    u_int32_t   evm3;
    u_int32_t   evm4;
#if ATH_SUPPORT_WIFIPOS
    u_int8_t    *hdump;
#else
    /* WAR for Kiwi,  padding is added to fix UL throughput issue*/
    u_int32_t    pad;
#endif
#ifdef ATH_SUPPORT_TxBF
    u_int8_t    rx_hw_upload_data       :1,
                rx_not_sounding         :1,
                rx_Ness                 :2,
                rx_hw_upload_data_valid :1,
                rx_hw_upload_data_type  :2;
#endif
#if ATH_SUPPORT_WIFIPOS
    u_int8_t    rx_position_bit:1;
#endif
    u_int16_t   rs_channel;     /* received channel */
};

#define    HAL_RXERR_CRC        0x01    /* CRC error on frame */
#define    HAL_RXERR_PHY        0x02    /* PHY error, rs_phyerr is valid */
#define    HAL_RXERR_FIFO       0x04    /* fifo overrun */
#define    HAL_RXERR_DECRYPT    0x08    /* non-Michael decrypt error */
#define    HAL_RXERR_MIC        0x10    /* Michael MIC decrypt error */
#define    HAL_RXERR_INCOMP     0x20    /* Rx Desc processing is incomplete */
#define    HAL_RXERR_KEYMISS    0x40    /* Key not found in keycache */

/* rs_flags values */
#define HAL_RX_MORE             0x01    /* more descriptors follow */
#define HAL_RX_MORE_AGGR        0x02    /* more frames in aggr */
#define HAL_RX_GI               0x04    /* full gi */
#define HAL_RX_2040             0x08    /* 40 Mhz */
#define HAL_RX_DELIM_CRC_PRE    0x10    /* crc error in delimiter pre */
#define HAL_RX_DELIM_CRC_POST   0x20    /* crc error in delim after */
#define HAL_RX_DECRYPT_BUSY     0x40    /* decrypt was too slow */
#define HAL_RX_HI_RX_CHAIN      0x80    /* SM power save: Rx chain control in high */

enum {
    HAL_PHYERR_UNDERRUN             = 0,    /* Transmit underrun */
    HAL_PHYERR_TIMING               = 1,    /* Timing error */
    HAL_PHYERR_PARITY               = 2,    /* Illegal parity */
    HAL_PHYERR_RATE                 = 3,    /* Illegal rate */
    HAL_PHYERR_LENGTH               = 4,    /* Illegal length */
    HAL_PHYERR_RADAR                = 5,    /* Radar detect */
    HAL_PHYERR_SERVICE              = 6,    /* Illegal service */
    HAL_PHYERR_TOR                  = 7,    /* Transmit override receive */
    /* NB: these are specific to the 5212 */
    HAL_PHYERR_OFDM_TIMING          = 17,    /* */
    HAL_PHYERR_OFDM_SIGNAL_PARITY   = 18,    /* */
    HAL_PHYERR_OFDM_RATE_ILLEGAL    = 19,    /* */
    HAL_PHYERR_OFDM_LENGTH_ILLEGAL  = 20,    /* */
    HAL_PHYERR_OFDM_POWER_DROP      = 21,    /* */
    HAL_PHYERR_OFDM_SERVICE         = 22,    /* */
    HAL_PHYERR_OFDM_RESTART         = 23,    /* */
    HAL_PHYERR_FALSE_RADAR_EXT      = 24,    /* Radar detect */

    HAL_PHYERR_CCK_TIMING           = 25,    /* */
    HAL_PHYERR_CCK_HEADER_CRC       = 26,    /* */
    HAL_PHYERR_CCK_RATE_ILLEGAL     = 27,    /* */
    HAL_PHYERR_CCK_SERVICE          = 30,    /* */
    HAL_PHYERR_CCK_RESTART          = 31,    /* */
    HAL_PHYERR_CCK_LENGTH_ILLEGAL   = 32,   /* */
    HAL_PHYERR_CCK_POWER_DROP       = 33,   /* */

    HAL_PHYERR_HT_CRC_ERROR         = 34,   /* */
    HAL_PHYERR_HT_LENGTH_ILLEGAL    = 35,   /* */
    HAL_PHYERR_HT_RATE_ILLEGAL      = 36,   /* */
    HAL_PHYERR_SPECTRAL             = 38,   /* Spectral scan packet -- Only Kiwi and later */
};

/* value found in rs_keyix to mark invalid entries */
#define    HAL_RXKEYIX_INVALID    ((u_int8_t) -1)
/* value used to specify no encryption key for xmit */
#define    HAL_TXKEYIX_INVALID    ((u_int) -1)

/* value used to specify invalid signal status */
#define    HAL_RSSI_BAD           0x80
#define    HAL_EVM_BAD            0x80

/* XXX rs_antenna definitions */

/*
 * Definitions for the software frame/packet descriptors used by
 * the Atheros HAL.  This definition obscures hardware-specific
 * details from the driver.  Drivers are expected to fillin the
 * portions of a descriptor that are not opaque then use HAL calls
 * to complete the work.  Status for completed frames is returned
 * in a device-independent format.
 */
/*
 * Do not attempt to use the __packed attribute to 
 * this desc data structure to save more CPU cycles
 * because gcc tend to put extra code for protection
 * with this attribute. 
 * Be EXTRA care that the beginning fields from ds_link to 
 * ds_hw should be 32 bits
 */
struct ath_desc {
    /*
     * The following definitions are passed directly
     * the hardware and managed by the HAL.  Drivers
     * should not touch those elements marked opaque.
     */
    u_int32_t    ds_link;    /* phys address of next descriptor */
    u_int32_t    ds_data;    /* phys address of data buffer */
    u_int32_t    ds_ctl0;    /* opaque DMA control 0 */
    u_int32_t    ds_ctl1;    /* opaque DMA control 1 */
    u_int32_t    ds_hw[20];    /* opaque h/w region */
    /*
     * The remaining definitions are managed by software;
     * these are valid only after the rx/tx process descriptor
     * methods return a non-EINPROGRESS  code.
     */
    union {
        struct ath_tx_status tx;/* xmit status */
        struct ath_rx_status rx;/* recv status */
        void    *stats;
    } ds_us;
    void        *ds_vdata;    /* virtual addr of data buffer */
};

#define    ds_txstat    ds_us.tx
#define    ds_rxstat    ds_us.rx
#define ds_stat        ds_us.stats

/* flags passed to tx descriptor setup methods */
#define HAL_TXDESC_CLRDMASK     0x0001    /* clear destination filter mask */
#define HAL_TXDESC_NOACK        0x0002    /* don't wait for ACK */
#define HAL_TXDESC_RTSENA       0x0004    /* enable RTS */
#define HAL_TXDESC_CTSENA       0x0008    /* enable CTS */
#define HAL_TXDESC_INTREQ       0x0010    /* enable per-descriptor interrupt */
#define HAL_TXDESC_VEOL         0x0020    /* mark virtual EOL */
#define HAL_TXDESC_EXT_ONLY     0x0040  /* send on ext channel only */
#define HAL_TXDESC_EXT_AND_CTL  0x0080  /* send on ext + ctl channels */
#define HAL_TXDESC_VMF          0x0100  /* virtual more frag */
#define HAL_TXDESC_FRAG_IS_ON   0x0200  /* Fragmentation enabled */
#define HAL_TXDESC_LOWRXCHAIN   0x0400  /* switch to low rx chain */
#ifdef  ATH_SUPPORT_TxBF
#define HAL_TXDESC_TXBF         0xf800  /*for txbf*/
#define HAL_TXDESC_TXBF_SOUND   0x1800  /* for sounding settings*/
#define HAL_TXDESC_TXBF_SOUND_S 11
#define HAL_TXDESC_SOUND        0x1     /* enable sounding */
#define HAL_TXDESC_STAG_SOUND   0x2     /* enable sounding */
#define HAL_TXDESC_TRQ          0x3     /* request sounding */
#define HAL_TXDESC_CAL          0x2000  /* calibration frame */
#define HAL_TXDESC_CEC          0xC000  /* channel calibration capability*/
#define HAL_TXDESC_CEC_S        14
#endif
#define HAL_TXDESC_LDPC             0x00010000
#if ATH_SUPPORT_WIFIPOS
#define HAL_TXDESC_POS              0x00020000
#define HAL_TXDESC_POS_TXCHIN_1     0x00040000
#define HAL_TXDESC_POS_TXCHIN_2     0x00080000
#define HAL_TXDESC_POS_TXCHIN_3     0x00100000
#define HAL_TXDESC_POS_KEEP_ALIVE   0x00200000
#endif
#define HAL_TXDESC_ENABLE_DURATION  0x1000 

#define HAL_TXPOWER_MAX         100  

/* flags passed to rx descriptor setup methods */
#define HAL_RXDESC_INTREQ       0x0020    /* enable per-descriptor interrupt */

#ifdef WIN32
#pragma pack(pop, ah_desc)
#endif

#endif /* _DEV_ATH_AR521XDMA_H */
