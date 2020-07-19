/*
 * Copyright (c) 2011, 2019 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * Copyright (c) 2000-2003, Atheros Communications Inc.
 * All Rights Reserved.
 *
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 */

#ifndef _PKTLOG_H_
#define _PKTLOG_H_

//#include "ieee80211_var.h"
#ifndef PKTLOG_TOOLS
#include "ath_internal.h"
#include "ratectrl.h"
#endif /* PKTLOG_TOOLS */
#include "pktlog_fmt.h"
#ifndef PKTLOG_TOOLS
#include "pktlog_rc.h"
#include <osdep.h>
#else
typedef void *osdev_t;
#endif /* PKTLOG_TOOLS */

#define PKTLOG_TAG "ATH_PKTLOG"

#ifdef USE_CHATTY_DEBUG
#define PKTLOG_CHATTY_DEBUG(_fmt, ...) printk(_fmt, __VA_ARGS__)
#else
#define PKTLOG_CHATTY_DEBUG(_fmt, ...)
#endif

#ifndef REMOVE_PKT_LOG
//struct ath_generic_softc_t;
typedef struct ath_generic_softc_t *ath_generic_softc_handle;

#ifdef OL_ATH_SMART_LOGGING
struct pl_smartlog_handler_funcs {
    /*
     * See documentation for the functions that get registered to the below
     * function pointers.
     */
    QDF_STATUS (*smart_log_fw_pktlog_stop_and_block)(
            ath_generic_softc_handle sc,
            int32_t host_pktlog_types, bool block_only_if_started);
    void (*smart_log_fw_pktlog_unblock)(ath_generic_softc_handle sc);
};
#endif /* OL_ATH_SMART_LOGGING */

struct pktlog_handle_t {
    struct pl_arch_dep_funcs *pl_funcs;
    struct ath_pktlog_info *pl_info;
    ath_generic_softc_handle scn;
    char *name;
    bool tgt_pktlog_enabled;
#ifdef OL_ATH_SMART_LOGGING
    struct pl_smartlog_handler_funcs pl_sl_funcs;
#endif /* OL_ATH_SMART_LOGGING */
};

struct pl_arch_dep_funcs {
    void (*pktlog_init)(void *scn);
    int (*pktlog_enable)(ath_generic_softc_handle sc,
            int32_t log_state);
    int (*pktlog_setsize)(ath_generic_softc_handle sc, int32_t log_state);
    int (*pktlog_disable)(ath_generic_softc_handle sc);
    void (*pktlog_deinit)(void *scn);
    int (*pktlog_filter_mac)(ath_generic_softc_handle sc,
            char *macaddr);
};
extern struct pl_arch_dep_funcs pl_funcs;
extern unsigned int enable_pktlog_support;
#else
//struct ath_generic_softc_t;
typedef struct ath_generic_softc_t *ath_generic_softc_handle;
struct pktlog_handle_t {
	int i;
};

struct pl_arch_dep_funcs {
	int i;
};
extern struct pl_arch_dep_funcs pl_funcs;
#endif


#ifndef REMOVE_PKT_LOG
//#if 1

/*
 * Version info:
 * 10001 - Logs (rssiLast, rssiLastPrev)
 * 10002 - Logs (rssiLast, rssiLastPrev, rssiLastPrev2)
 */

/*
 * NOTE: the Perl script that processes the packet log data
 *       structures has hardcoded structure offsets.
 *
 * When adding new fields it is important to:
 * - assign a newer verison number (CURRENT_VER_PKT_LOG)
 * - Add fields to the end of the structure.
 * - Observe proper memory alignment of fields. This
 *     eases the process of adding the structure offsets
 *     to the Perl script.
 * - note the new size of the pkt log data structure
 * - Update the Perl script to reflect the new structure and version/size changes
 */

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
    ath_generic_softc_handle scn; /* pointer to scn object */
    u_int32_t remote_port;           /* port at which remote packetlogger sends data */
#if REMOTE_PKTLOG_SUPPORT
    /* Remote Pktlog specific */
    u_int32_t rlog_write_index;     /* write index for remote packet log */
    u_int32_t rlog_read_index;      /* read index for remote packet log */
    u_int32_t rlog_max_size;        /* max size for remote packetlog*/
    int is_wrap;                    /* is write index wrapped ? */
#endif
    u_int32_t tx_capture_enabled;    /* is tx_capture enabled */
};

/*
 * This provides a generic API to support legacy Pktlogs and
 * Converged SW architecture Pktlogs for Peregrine
 */

/*struct pl_arch_dep_funcs {
    void (*pktlog_init)(struct ath_pktlog_info *pl_info);
    int (*pktlog_enable)(ath_generic_softc_handle sc,
            int32_t log_state);
    int (*pktlog_setsize)(ath_generic_softc_handle sc, int32_t log_state);
    int (*pktlog_disable)(ath_generic_softc_handle sc);
};
extern struct pl_arch_dep_funcs pl_funcs;

struct pktlog_handle_t {
    struct pl_arch_dep_funcs *pl_funcs;
    struct ath_pktlog_info *pl_info;
    char *name;
    bool tgt_pktlog_enabled;
};
extern struct  pktlog_handle_t *pl_dev;*/

#define set_pktlog_handle_funcs(g_exported_pktlog_funcs) \
struct pktlog_handle_t pktlog_handle_dev = { \
    .g_pktlog_funcs = &g_exported_pktlog_funcs, \
    .pl_info = NULL, \
};

struct ath_pktlog_arg {
    struct ath_pktlog_info *pl_info;
    u_int16_t log_type;
    size_t log_size;
    u_int32_t flags;
    char *buf;
};

/* Locking interface for pktlog */
#define PKTLOG_LOCK_INIT(_pl_info)    spin_lock_init(&(_pl_info)->log_lock)
#define	PKTLOG_LOCK_DESTROY(_pl_info)
#define PKTLOG_LOCK(_pl_info)         spin_lock(&(_pl_info)->log_lock)
#define PKTLOG_UNLOCK(_pl_info)       spin_unlock(&(_pl_info)->log_lock)

/* Parameter types for packet logging driver hooks */
struct log_tx {
    void *firstds;
    void *lastds;
    struct ath_buf *bf;
	int32_t nbad;               /* # of bad frames (by looking at block ACK) in an aggregate */
    int32_t misc[8];            /* Can be used for HT specific or other misc info */
    /* TBD: Add other required information */
};

struct log_rx {
    struct ath_desc *ds;
    struct ath_rx_status *status;
    struct ath_buf *bf;
    int32_t misc[8];            /* Can be used for HT specific or other misc info */
    /* TBD: Add other required information */
};

#define PKTLOG_MODE_SYSTEM   1
#define PKTLOG_MODE_ADAPTER  2

typedef enum {
    PKTLOG_PROTO_RX_DESC,
    PKTLOG_PROTO_TX_DESC,
} pktlog_proto_desc_t;

struct log_rcfind;
struct log_rcupdate;

struct ath_pktlog_funcs {
    int (*pktlog_attach) (ath_generic_softc_handle scn);
    void (*pktlog_detach) (ath_generic_softc_handle scn);
    void (*pktlog_txctl) (struct ath_softc *, struct log_tx *, u_int32_t);
    void (*pktlog_txstatus) (struct ath_softc *, struct log_tx *, u_int32_t);
    void (*pktlog_rx) (struct ath_softc *, struct log_rx *, u_int32_t);
    int (*pktlog_text) (struct ath_softc *sc, const char *text, u_int32_t flags);
    int (*pktlog_start)(ath_generic_softc_handle scn, int log_state);
    int (*pktlog_readhdr)(struct ath_softc *sc, void *buf, u_int32_t,
                          u_int32_t *, u_int32_t *);
    int (*pktlog_readbuf)(struct ath_softc *sc, void *buf, u_int32_t,
                          u_int32_t *, u_int32_t *);
    void (*pktlog_rcfind)(struct ath_softc *, struct log_rcfind *, u_int32_t);
    void (*pktlog_rcupdate)(struct ath_softc *, struct log_rcupdate *, u_int32_t);
    void (*pktlog_get_scn)(osdev_t dev,
                            ath_generic_softc_handle *scn);
    void (*pktlog_get_dev_name)(osdev_t dev, ath_generic_softc_handle scn);
};

#define ath_log_txctl(_sc, _log_data, _flags)                           \
        do {                                                            \
            if (g_pktlog_funcs) {                                       \
                g_pktlog_funcs->pktlog_txctl(_sc, _log_data, _flags);   \
            }                                                           \
        } while(0)

#define ath_log_txstatus(_sc, _log_data, _flags)                        \
        do {                                                            \
            if (g_pktlog_funcs) {                                       \
                g_pktlog_funcs->pktlog_txstatus(_sc, _log_data, _flags);\
            }                                                           \
        } while(0)

#define ath_log_rx(_sc, _log_data, _flags)                          \
        do {                                                        \
            if (g_pktlog_funcs) {                                   \
                g_pktlog_funcs->pktlog_rx(_sc, _log_data, _flags);  \
            }                                                       \
        } while(0)

#define ath_log_text(_sc, _log_text, _flags)                        \
        do {                                                        \
            if (g_pktlog_funcs) {                                   \
                g_pktlog_funcs->pktlog_text(_sc, _log_text, _flags);\
            }                                                       \
        } while(0)

#define ath_pktlog_attach(_sc)                                      \
        do {                                                        \
            if (g_pktlog_funcs &&                                   \
                    g_pktlog_funcs->pktlog_attach) {               \
                g_pktlog_funcs->pktlog_attach(_sc);                 \
            }                                                       \
        } while(0)

#define ath_pktlog_detach(scn)                                      \
        do {                                                        \
            if (g_pktlog_funcs) {                                   \
                g_pktlog_funcs->pktlog_detach(scn);                 \
            }                                                       \
        } while(0)

#define ath_pktlog_start(_sc, _log, _err)                           \
        do {                                                        \
            if (g_pktlog_funcs) {                                   \
                _err = g_pktlog_funcs->pktlog_start(_sc, _log);     \
            } else {                                                \
                _err = 0;                                           \
            }                                                       \
        } while (0)

#define ath_pktlog_read_hdr(_sc, _buf, _len, _rlen, _alen, _err)    \
        do {                                                        \
            if (g_pktlog_funcs) {                                   \
                _err = g_pktlog_funcs->pktlog_readhdr(_sc, _buf,    \
                                       _len, _rlen, _alen);         \
            } else {                                                \
                _err = 0;                                           \
            }                                                       \
        } while (0)

#define ath_pktlog_read_buf(_sc, _buf, _len, _rlen, _alen, _err)    \
        do {                                                        \
            if (g_pktlog_funcs) {                                   \
                _err = g_pktlog_funcs->pktlog_readbuf(_sc, _buf,    \
                                       _len, _rlen, _alen);         \
            } else {                                                \
                _err = 0;                                           \
            }                                                       \
        } while (0)

#define ath_pktlog_get_scn(_dev, _scn)                               \
        do {                                                        \
            if (g_pktlog_funcs) {                                   \
                g_pktlog_funcs->pktlog_get_scn(_dev, _scn);         \
            }                                                       \
        } while (0)

#define ath_pktlog_get_dev_name(_dev, _scn)                      \
        do {                                                        \
            if (g_pktlog_funcs) {                                   \
                g_pktlog_funcs->pktlog_get_dev_name(_dev, _scn); \
            }                                                       \
        } while (0)

#endif /* _PKTLOG_INFO  */
#endif /* #ifndef REMOVE_PKT_LOG */

#endif /* _PKTLOG_H_ */

