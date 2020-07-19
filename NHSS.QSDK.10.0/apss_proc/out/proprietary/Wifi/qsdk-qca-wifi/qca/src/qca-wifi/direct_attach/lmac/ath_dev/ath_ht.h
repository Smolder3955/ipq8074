/*
 * Copyright (c) 2010, Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */


/*
 * Definitions for the ATH HT (11n)layer. private header file
 */

#if ATH_SUPPORT_HYFI_ENHANCEMENTS
#include "ath_ald.h"
#endif

#ifndef ATH_HT_H
#define ATH_HT_H

#ifndef UMAC
/* UNINET_TODO: unified net defines it as 16. IEEE80211_TID_SIZE should be used for this case. */
#undef WME_NUM_TID
#endif
#define WME_NUM_TID         17 /* 16 WME TIDs and 1 Mgmt TID */
#define WME_MGMT_TID        16
#define WME_BA_BMP_SIZE     64
#define WME_MAX_BA          WME_BA_BMP_SIZE
#define ATH_TID_MAX_BUFS    (2 * WME_MAX_BA)

#define ATH_SINGLES_MIN_QDEPTH 2

/* Default values for receive timeout */
#define ATH_RX_VO_TIMEOUT      40  /* 40 milliseconds */
#define ATH_RX_VI_TIMEOUT      100 /* 100 milliseconds */
#define ATH_RX_BE_TIMEOUT      100 /* 100 milliseconds */
#define ATH_RX_BK_TIMEOUT      100 /* 100 milliseconds */

/* aggr interfaces */
int ath_aggr_check(ath_dev_t, ath_node_t node, u_int8_t tidno);
void ath_set_ampduparams(ath_dev_t, ath_node_t, u_int16_t maxampdu, u_int32_t mpdudensity);
void ath_set_weptkip_rxdelim(ath_dev_t dev, ath_node_t node, u_int8_t rxdelim);
void ath_addba_requestsetup(ath_dev_t, ath_node_t,
                            u_int8_t tidno, struct ieee80211_ba_parameterset *baparamset,
                            u_int16_t *batimeout, struct ieee80211_ba_seqctrl *basequencectrl,
                            u_int16_t buffersize);
void ath_addba_responsesetup(ath_dev_t, ath_node_t,
                             u_int8_t tidno, u_int8_t *dialogtoken,
                             u_int16_t *statuscode,
                             struct ieee80211_ba_parameterset *baparamset,
                             u_int16_t *batimeout);
int ath_addba_requestprocess(ath_dev_t, ath_node_t,
                             u_int8_t dialogtoken, struct ieee80211_ba_parameterset *baparamset,
                             u_int16_t batimeout, struct ieee80211_ba_seqctrl basequencectrl);
void ath_addba_responseprocess(ath_dev_t, ath_node_t,
                               u_int16_t statuscode, struct ieee80211_ba_parameterset *baparamset,
                               u_int16_t batimeout);
void ath_addba_clear(ath_dev_t, ath_node_t);
void ath_addba_cancel_timers(ath_dev_t, ath_node_t);
void ath_delba_process(ath_dev_t, ath_node_t,
                       struct ieee80211_delba_parameterset *delbaparamset, u_int16_t reasoncode);
u_int16_t ath_addba_status(ath_dev_t dev, ath_node_t node, u_int8_t tidno);
void ath_tx_aggr_teardown(struct ath_softc *sc, struct ath_node *an, u_int8_t tidno);
void ath_rx_aggr_teardown(struct ath_softc *sc, struct ath_node *an, u_int8_t tidno);
void ath_set_addbaresponse(ath_dev_t dev, ath_node_t node,
                           u_int8_t tidno, u_int16_t statuscode);
void ath_clear_addbaresponsestatus(ath_dev_t dev, ath_node_t node);
void ath_set_addbaresponse(ath_dev_t dev, ath_node_t node,
                           u_int8_t tidno, u_int16_t statuscode);
void ath_clear_addbaresponsestatus(ath_dev_t dev, ath_node_t node);

/* AMSDU frame tx interfaces */
int ath_get_amsdusupported(ath_dev_t dev, ath_node_t node, int tidno);
void ath_aggr_teardown(ath_dev_t dev, ath_node_t node, u_int8_t tidno, u_int8_t initiator);
void ath_rx_node_init(struct ath_softc *sc, struct ath_node *an);
void ath_rx_node_cleanup(struct ath_softc *sc, struct ath_node *an);
int ath_ampdu_input(struct ath_softc *sc, struct ath_node *an, wbuf_t wbuf, ieee80211_rx_status_t *rx_status);
int ath_bar_rx(struct ath_softc *sc, struct ath_node *an, wbuf_t wbuf);

/* cleanup pending tx fragments */
void ath_tx_frag_cleanup(struct ath_softc *sc, struct ath_node *an);
/*
 * if HT support is not defined , turn it on by default
 */


struct ath_atx_ac;

#define ATH_DEV_RESET_HT_MODES(w) /* */

#define TX_BUF_BITMAP_WORD_SIZE  32 /* needs to be a power of 2 */
#define TX_BUF_BITMAP_LOG2_WORD_SIZE 5 /* log2(32) == 5 */
#define TX_BUF_BITMAP_WORD_MASK (TX_BUF_BITMAP_WORD_SIZE-1)
/* TX_BUF_BITMAP_SET, TX_BUF_BITMAP_CLR -
 * Find the word by dividing the index by the number of bits per word.
 * Find the bit within the word by masking out all but the LSBs of the index.
 * Set or clear the relevant bit within the relevant word.
 */
#define TX_BUF_BITMAP_SET(bitmap, i)              \
    (bitmap[i >> TX_BUF_BITMAP_LOG2_WORD_SIZE] |= \
    (1 << (i & TX_BUF_BITMAP_WORD_MASK)))
#define TX_BUF_BITMAP_CLR(bitmap, i)              \
    (bitmap[i >> TX_BUF_BITMAP_LOG2_WORD_SIZE] &= \
    ~(1 << (i & TX_BUF_BITMAP_WORD_MASK)))
#define TX_BUF_BITMAP_IS_SET(bitmap, i) \
    ((bitmap[i >> TX_BUF_BITMAP_LOG2_WORD_SIZE] & \
    (1 << (i & TX_BUF_BITMAP_WORD_MASK))) != 0)
/*
 * We expect ATH_TID_MAX_BUFS to be a multiple of BITMAP_WORD_SIZE,
 * but just in case it isn't, round up.
 */
#define TX_BUF_BITMAP_WORDS \
    ((ATH_TID_MAX_BUFS+TX_BUF_BITMAP_WORD_SIZE-1) / TX_BUF_BITMAP_WORD_SIZE)

/*
 * per TID aggregate tx state for a destination
 */
typedef struct ath_atx_tid {
    int               tidno;      /* TID number */
    u_int16_t         seq_start;  /* starting seq of BA window */
    u_int16_t         seq_next;   /* next seq to be used */
    u_int16_t         baw_size;   /* BA window size */
#ifdef ATH_HTC_TX_SCHED
    u_int8_t          tid_buf_cnt;
    u_int8_t          pad0;
#endif
    int               baw_head;   /* first un-acked tx buffer */
    int               baw_tail;   /* next unused tx buffer slot */
    u_int16_t         sched:1,    /* TID is scheduled */
                      filtered:1, /* TID has filtered pkts */
                      min_depth:2;/* num pkts that can be queued to h/w */
    atomic_t          paused;     /* TID is paused */
    int               bar_paused; /* TID is paused because of a BAR is sent */
    int               cleanup_inprogress; /* this TID's aggr being torn down */
    TAILQ_HEAD(ath_tid_bq,ath_buf) buf_q;      /* pending buffers */
    TAILQ_ENTRY(ath_atx_tid)       tid_qelem;  /* round-robin tid entry */
    TAILQ_HEAD(,ath_buf)           fltr_q;     /* filtered buffers */
    TAILQ_ENTRY(ath_atx_tid)       fltr_qelem; /* handle hwq filtering */
    struct ath_node   *an;        /* parent node structure */
    struct ath_atx_ac *ac;        /* parent access category */
    u_int32_t         tx_buf_bitmap[TX_BUF_BITMAP_WORDS]; /* active tx frames */

    /*
     * ADDBA state
     */
    u_int32_t               addba_exchangecomplete:1,
                            addba_amsdusupported:1;
    int32_t                 addba_exchangeinprogress;
    struct ath_timer        addba_requesttimer;
    int                     addba_exchangeattempts;
    u_int16_t               addba_exchangestatuscode;
#if ATH_FRAG_TX_COMPLETE_DEFER
    wbuf_t                  frag_chainhead;      /* the header of the fragment chain (same seqno) */
    wbuf_t                  frag_chaintail;      /* the tail of the fragment chain (same seqno) */
    ieee80211_tx_status_t   frag_tx_status;      /* tx status reported to upper layer for all frags */
#endif
    u_int32_t               addba_retry;
} ath_atx_tid_t;

/*
 * per access-category aggregate tx state for a destination
 */
typedef struct ath_atx_ac {
    int                     sched;      /* dest-ac is scheduled */
    int                     qnum;       /* H/W queue number associated with this AC */
    int                     hwqcnt;     /* count of pkts on hw queue */
#if ATH_SUPPORT_VOWEXT
    uint16_t                max_sch_penality;	/* Max Scheuding penality fo this ac */
    uint16_t                sch_penality_cnt;	/* Current pending scheduling penality count */
#endif
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
    struct ath_ald_ac_stats ald_ac_stats;  /* link stats per node per ac */
#endif
    TAILQ_ENTRY(ath_atx_ac) ac_qelem;   /* round-robin txq entry */
    TAILQ_HEAD(,ath_atx_tid)tid_q;      /* queue of TIDs with buffers */
    int                     filtered;   /* ac is filtered */
    TAILQ_ENTRY(ath_atx_ac) fltr_qelem; /* handle hwq filtering */
    TAILQ_HEAD(,ath_atx_tid)fltr_q;     /* queue of TIDs being filtered */
} ath_atx_ac_t;

/*
 * per dest tx state
 */
struct ath_atx {
    int                 hwqcnt;         /* count of pkts on hw queue */
    u_int16_t           maxampdu;       /* per-destination max ampdu */
    u_int16_t           mpdudensity;    /* per-destination ampdu density */
    u_int8_t            weptkipdelim;   /* per-destination delimiter count for WEP/TKIP w/aggregation */
    struct ath_atx_tid  tid[WME_NUM_TID];
    struct ath_atx_ac   ac[WME_NUM_AC];
};

#define ATH_AN_2_AC(_an, _priority) &(_an)->an_aggr.tx.ac[(_priority)]
#define ATH_AN_2_TID(_an, _tidno)   (&(_an)->an_aggr.tx.tid[(_tidno)])

struct ath_rxbuf {
    wbuf_t                  rx_wbuf;   /* buffer */
    systime_t               rx_time;    /* system time when received */
    ieee80211_rx_status_t   rx_status;  /* cached rx status */
};

/*
 * Per-TID aggregate receiver state for a node
 */
struct ath_arx_tid {
    struct ath_node     *an;        /* parent ath node */
    u_int16_t           seq_next;   /* next expected sequence */
    u_int16_t           baw_size;   /* block-ack window size */
    int                 baw_head;   /* seq_next at head */
    int                 baw_tail;   /* tail of block-ack window */
    int                 seq_reset;  /* need to reset start sequence */
    ath_rxtimer        timer;      /* timer element */
    struct ath_rxbuf    *rxbuf;     /* re-ordering buffer */
#ifdef ATH_USB
    usblock_t           tidlock;
#else
    spinlock_t          tidlock;    /* lock to protect this TID structure */
#endif

    /*
     * ADDBA response information
     */
    u_int16_t                       dialogtoken;
    u_int16_t                       statuscode;
    struct ieee80211_ba_parameterset baparamset;
    u_int16_t                       batimeout;
    u_int16_t                       userstatuscode;
    int                             addba_exchangecomplete;
#if ATH_SUPPORT_TIDSTUCK_WAR
    systime_t                       rxtid_lastdata;
#endif
};

#ifdef ATH_USB
#define	   ATH_RXTID_LOCK_INIT(_rxtid)     OS_USB_LOCK_INIT(&(_rxtid)->tidlock)
#define	   ATH_RXTID_LOCK_DESTROY(_rxtid)  OS_USB_LOCK_DESTROY(&(_rxtid)->tidlock)
#define	   ATH_RXTID_LOCK(_rxtid)          OS_USB_LOCK(&(_rxtid)->tidlock)
#define	   ATH_RXTID_UNLOCK(_rxtid)        OS_USB_UNLOCK(&(_rxtid)->tidlock)
#else
#define    ATH_RXTID_LOCK_INIT(_rxtid)     spin_lock_init(&(_rxtid)->tidlock)
#define    ATH_RXTID_LOCK_DESTROY(_rxtid)
#define    ATH_RXTID_LOCK(_rxtid)          spin_lock(&(_rxtid)->tidlock)
#define    ATH_RXTID_UNLOCK(_rxtid)        spin_unlock(&(_rxtid)->tidlock)
#endif

/*
 * Per-node receiver aggregate state
 */
struct ath_arx {
    struct ath_arx_tid  tid[WME_NUM_TID];
};

/*
 * Per-node aggregation state
 */
struct ath_node_aggr {
    struct ath_atx      tx;         /* node transmit state */
    struct ath_arx      rx;         /* node receive state */
};
#define an_tx_tid       an_aggr.tx.tid
#define an_tx_ac        an_aggr.tx.ac
#define an_rx_tid       an_aggr.rx.tid

void ath_tx_node_init(struct ath_softc *sc, struct ath_node *an);
void ath_tx_node_free(struct ath_softc *sc, struct ath_node *an);
void ath_tx_node_pause(struct ath_softc *sc, struct ath_node *an);
void ath_tx_node_pause_nolock(struct ath_softc *sc, struct ath_node *an);
void ath_tx_node_resume(struct ath_softc *sc, struct ath_node *an);
#if QCA_AIRTIME_FAIRNESS
void ath_tx_node_atf_pause(struct ath_softc *sc, struct ath_node *an, int8_t tidno);
void ath_tx_node_atf_pause_nolock(struct ath_softc *sc, struct ath_node *an, int8_t tidno);
void ath_tx_node_atf_resume(struct ath_softc *sc, struct ath_node *an, int8_t tidno);
#endif
void ath_txq_schedule(struct ath_softc *sc, struct ath_txq *txq);
int ath_tx_send_normal(struct ath_softc *sc, struct ath_txq *txq,
                       struct ath_atx_tid *tid, ath_bufhead *bf_head, ieee80211_tx_control_t *txctl);
int ath_tx_send_ampdu(struct ath_softc *sc, struct ath_txq *txq, struct ath_atx_tid *tid,
                      ath_bufhead *bf_head, ieee80211_tx_control_t *txctl);
int ath_tx_num_badfrms(struct ath_softc *sc, struct ath_buf *bf, struct ath_tx_status *ts, int txok);
void ath_tx_complete_aggr_rifs(struct ath_softc *sc, struct ath_txq *txq, struct ath_buf *bf,
                               ath_bufhead *bf_q, struct ath_tx_status *ts, int txok);
#if  ATH_TX_COMPACT  &&  UMAC_SUPPORT_APONLY
int
ath_tx_complete_aggr_compact(struct ath_softc *sc, struct ath_txq *txq, struct ath_buf *bf,
                ath_bufhead *bf_head, struct ath_tx_status *ts, int txok,ath_bufhead *bf_headfree);

void  ath_tx_mark_aggr_rifs_done(struct ath_softc *sc, struct ath_txq *txq, struct ath_buf *bf,
        ath_bufhead *bf_head, struct ath_tx_status *ts, int txok);

void  ath_edma_free_complete_buf( struct ath_softc *sc , struct ath_txq *txq, ath_bufhead * bf_headfree, int txok );
#else
#define ath_tx_mark_aggr_rifs_done(_sc, _txq, _bf, _bfhead, _ts, _txok ) \
           ath_tx_complete_aggr_rifs(_sc, _txq, _bf, _bfhead, _ts, _txok )
#endif



void ath_txq_drain_pending_buffers(struct ath_softc *sc, struct ath_txq *txq);
#if ATH_TXQ_DRAIN_DEFERRED
void ath_txq_defer_drain_pending_buffers(struct ath_softc *sc, struct ath_txq *txq, ath_bufhead *drained_bf_head);
void ath_complete_drained_bufs(struct ath_softc *sc, ath_bufhead *drained_bf_head);
#endif
void ath_tx_update_baw(struct ath_softc *sc, struct ath_atx_tid *tid, int seqno);
int  ath_insertq_inorder(struct ath_softc *sc, struct ath_atx_tid *tid, ath_bufhead *bf_pending);
void ath_tx_sched_normal(struct ath_softc *sc, struct ath_txq *txq, ath_atx_tid_t *tid);
/*
 * Check if it's okay to send out aggregates
 */
#define ath_aggr_query(tid)  (tid->addba_exchangecomplete || tid->addba_exchangeinprogress)

#ifndef ATH_HTC_TX_SCHED
void ath_tx_pause_tid(struct ath_softc *sc, struct ath_atx_tid *tid);
void ath_tx_resume_tid(struct ath_softc *sc, struct ath_atx_tid *tid);
void ath_tx_node_cleanup(struct ath_softc *sc, struct ath_node *an);

#define ATH_TX_RESUME_TID ath_tx_resume_tid
#define ATH_TX_PAUSE_TID  ath_tx_pause_tid
#define ATH_TX_PAUSE_TID_NOLOCK  ath_tx_pause_tid_nolock
#define ATH_TX_RESUME_TID_NOLOCK  ath_tx_resume_tid_nolock
#define ATH_TX_NODE_CLEANUP ath_tx_node_cleanup


#else
void ath_txep_resume_tid(struct ath_softc *sc, ath_atx_tid_t *tid);
void ath_txep_pause_tid(struct ath_softc *sc, ath_atx_tid_t *tid);
void ath_htc_tx_node_cleanup(struct ath_softc *sc, struct ath_node *an);

#define ATH_TX_RESUME_TID ath_txep_resume_tid
#define ATH_TX_PAUSE_TID  ath_txep_pause_tid
#define ATH_TX_NODE_CLEANUP ath_htc_tx_node_cleanup

#endif
#endif /* ATH_HT_H */

