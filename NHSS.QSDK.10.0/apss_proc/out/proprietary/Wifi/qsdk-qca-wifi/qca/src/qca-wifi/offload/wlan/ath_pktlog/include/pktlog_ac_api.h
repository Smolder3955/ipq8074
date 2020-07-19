/*
 * Copyright (c) 2012 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

/* 
 *  The file is used to define structures that are shared between
 *  kernel space and user space pktlog application. 
 */

#ifndef _PKTLOG_AC_API_
#define _PKTLOG_AC_API_

/**
 * @typedef ol_pktlog_dev_handle
 * @brief opaque handle for pktlog device object
 */
typedef struct ol_pktlog_dev_t ol_pktlog_dev_t;

/**
 * @typedef ol_ath_softc_net80211_handle
 * @brief opaque handle for ol_ath_softc_net80211
 */
struct ol_ath_softc_net80211;
typedef struct ol_ath_softc_net80211* ol_ath_softc_net80211_handle; 

/**
 * @typedef net_device_handle
 * @brief opaque handle linux phy device object 
 */
struct net_device;
typedef struct net_device* net_device_handle;

void ol_pl_set_name(ol_ath_softc_net80211_handle scn, net_device_handle dev);

void ol_pl_sethandle(ol_pktlog_dev_t **pl_handle, ol_ath_softc_net80211_handle scn);

void ol_pl_freehandle(ol_pktlog_dev_t *pl_handle);

struct ol_ath_generic_softc_t;
typedef struct ol_ath_generic_softc_t* ol_ath_generic_softc_handle;
/* Packet log state information */
#ifndef _PKTLOG_INFO
#define _PKTLOG_INFO
struct ath_pktlog_info {
    struct ath_pktlog_buf *buf;
    u_int32_t log_state;
    u_int32_t saved_state;
    u_int32_t options;
    int32_t buf_size;           /* Size of buffer in bytes */
    qdf_spinlock_t log_lock;
    qdf_mutex_t pktlog_mutex;
    struct ath_softc *pl_sc;    /*Needed to call 'AirPort_AthrFusion__allocatePktLog' or similar functions */
    int sack_thr;               /* Threshold of TCP SACK packets for triggered stop */
    int tail_length;            /* # of tail packets to log after triggered stop */
    u_int32_t thruput_thresh;           /* throuput threshold in bytes for triggered stop */
    u_int32_t pktlen;          /* (aggregated or single) packet size in bytes */
    /* a temporary variable for counting TX throughput only */
    u_int32_t per_thresh;               /* PER threshold for triggered stop, 10 for 10%, range [1, 99] */
    u_int32_t phyerr_thresh;          /* Phyerr threshold for triggered stop */
    u_int32_t trigger_interval;       /* time period for counting trigger parameters, in milisecond */
    u_int32_t start_time_thruput;
    u_int32_t start_time_per;
    size_t pktlog_hdr_size;         /*  sizeof(struct ath_pktlog_hdr) */
    ol_ath_generic_softc_handle scn; /* pointer to scn object */
    u_int32_t remote_port;           /* port at which remote packetlogger sends data */
#if REMOTE_PKTLOG_SUPPORT
    /* Remote Pktlog specific */
    u_int32_t rlog_write_index;     /* write index for remote packet log */
    u_int32_t rlog_read_index;      /* read index for remote packet log */
    u_int32_t rlog_max_size;        /* max size for remote packetlog*/
    int is_wrap;                    /* is write index wrapped ? */
#endif
    u_int32_t tx_capture_enabled;    /* is tx_capture enabled */
    char macaddr[IEEE80211_ADDR_LEN];
    uint8_t peer_based_filter;
};
#endif /* _PKTLOG_INFO */

extern char dbglog_print_buffer[1024];

#endif  /* _PKTLOG_AC_API_ */
