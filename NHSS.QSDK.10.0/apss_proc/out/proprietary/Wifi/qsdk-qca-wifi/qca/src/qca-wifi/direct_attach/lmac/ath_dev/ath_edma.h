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
 * Definitions for the ATH layer internal API's.
 */
#ifndef ATH_EDMA_H
#define ATH_EDMA_H

#if ATH_SUPPORT_EDMA

/* number of elements in tx status ring */
#define ATH_TXS_RING_SIZE   512


/*
 * TXQ manipulation macros for Enhanced DMA hardware
 */

/* move buffers from MCASTQ to CABQ */
#define ATH_EDMA_TXQ_MOVE_MCASTQ(_tqs,_tqd) do {                    \
    ASSERT((_tqd)->axq_depth < HAL_TXFIFO_DEPTH);                   \
    (_tqd)->axq_depth++;                                            \
    (_tqd)->axq_totalqueued += (_tqs)->axq_totalqueued;             \
    TAILQ_INIT(&(_tqd)->axq_fifo[(_tqd)->axq_headindex]);          \
    TAILQ_CONCAT(&(_tqd)->axq_fifo[(_tqd)->axq_headindex], &(_tqs)->axq_q, bf_list); \
    (_tqd)->axq_headindex = ((_tqd)->axq_headindex+1) & (HAL_TXFIFO_DEPTH-1);      \
    (_tqs)->axq_depth=0;                                            \
    (_tqs)->axq_totalqueued = 0;                                    \
    (_tqs)->axq_linkbuf = 0;                                        \
    (_tqs)->axq_link = NULL;                                        \
} while (0)

/* concat a list of buffers to txq */
#define ATH_EDMA_TXQ_CONCAT(_tq, _stq) do {                                 \
    ASSERT((_tq)->axq_depth < HAL_TXFIFO_DEPTH);                            \
    if (!TAILQ_EMPTY(&(_tq)->axq_fifo[(_tq)->axq_headindex])) printk ("txq_concat err\n"); \
    TAILQ_INIT(&(_tq)->axq_fifo[(_tq)->axq_headindex]); \
    TAILQ_CONCAT(&(_tq)->axq_fifo[(_tq)->axq_headindex], (_stq), bf_list); \
    (_tq)->axq_headindex = ((_tq)->axq_headindex+1) & (HAL_TXFIFO_DEPTH-1); \
    (_tq)->axq_depth ++;                                                    \
    (_tq)->axq_totalqueued ++;                                              \
} while (0)

/* move a list from a txq to a buffer list (including _elm) */
#define ATH_EDMA_TXQ_MOVE_HEAD_UNTIL(_tq, _stq, _elm, _field) do {       \
    TAILQ_REMOVE_HEAD_UNTIL(&(_tq)->axq_fifo[(_tq)->axq_tailindex], _stq, _elm, _field); \
    if (!TAILQ_EMPTY(&(_tq)->axq_fifo[(_tq)->axq_tailindex])) printk ("txq_move err\n"); \
    (_tq)->axq_tailindex = ((_tq)->axq_tailindex+1) & (HAL_TXFIFO_DEPTH-1); \
    (_tq)->axq_depth --;                                            \
} while (0)

/* move a list from a txq to a buffer list (including _elm) */
#define ATH_EDMA_MCASTQ_MOVE_HEAD_UNTIL(_tq, _stq, _elm, _field) do {       \
    TAILQ_REMOVE_HEAD_UNTIL(&(_tq)->axq_fifo[(_tq)->axq_tailindex], _stq, _elm, _field); \
    (_tq)->axq_totalqueued --;                                            \
    if (TAILQ_EMPTY(&(_tq)->axq_fifo[(_tq)->axq_tailindex])) {  \
    (_tq)->axq_tailindex = ((_tq)->axq_tailindex+1) & (HAL_TXFIFO_DEPTH-1); \
    (_tq)->axq_depth --; }                                            \
} while (0)



/* Receive FIFO management */
struct ath_rx_edma {
    wbuf_t                  *rxfifo;
    u_int8_t                rxfifoheadindex;
    u_int8_t                rxfifotailindex;
    u_int8_t                rxfifodepth;        /* count of RXBPs pushed into fifo */
    u_int32_t               rxfifohwsize;       /* Rx FIFO size from HAL */
    ath_bufhead             rxqueue;
    spinlock_t              rxqlock;
};

#define ATH_RX_EDMA_CONTROL_OBJ(xxx)   struct ath_rx_edma xxx[HAL_NUM_RX_QUEUES]

/* Enahnced DMA locking support */
#define	ATH_RXQ_LOCK_INIT(_rxedma)    spin_lock_init(&(_rxedma)->rxqlock)
#define	ATH_RXQ_LOCK_DESTROY(_rxedma)
#define ATH_RXQ_LOCK(_rxedma)         spin_lock(&(_rxedma)->rxqlock)
#define ATH_RXQ_UNLOCK(_rxedma)       spin_unlock(&(_rxedma)->rxqlock)

#define	ATH_TXSTATUS_LOCK_INIT(_sc)    spin_lock_init(&(_sc)->sc_txstatuslock)
#define	ATH_TXSTATUS_LOCK_DESTROY(_sc)
#define ATH_TXSTATUS_LOCK(_sc)         spin_lock(&(_sc)->sc_txstatuslock)
#define ATH_TXSTATUS_UNLOCK(_sc)       spin_unlock(&(_sc)->sc_txstatuslock)

void ath_edma_attach(struct ath_softc *sc, struct ath_ops **ops);

/*
 * Transmit functions to support enhanced DMA
 */
int ath_txstatus_setup(struct ath_softc *sc, struct ath_descdma *dd,
        const char *name, int ndesc);
void ath_txstatus_cleanup( struct ath_softc *sc, struct ath_descdma *dd);
int ath_tx_edma_process(ath_dev_t);
void ath_tx_edma_tasklet(ath_dev_t);
int ath_tx_edma_init(struct ath_softc *sc);
void ath_tx_edma_cleanup(struct ath_softc *sc);


/*
 * Receive functions to support enhanced DMA
 */
int ath_rx_edma_init(ath_dev_t, int nbufs);
void ath_rx_edma_requeue(ath_dev_t, wbuf_t wbuf);
int ath_rx_edma_tasklet(ath_dev_t, int flush);
int ath_edma_startrecv(struct ath_softc *sc);
HAL_BOOL ath_edma_stoprecv(struct ath_softc *sc, int timeout);
void ath_rx_edma_cleanup(ath_dev_t);
void ath_rx_edma_intr(ath_dev_t dev, HAL_INT status, int *sched);
void ath_edmaAllocRxbufsForFreeList(struct ath_softc *sc);
void ath_rx_intr(ath_dev_t dev, HAL_RX_QUEUE qtype);
#ifdef ATH_SUPPORT_UAPSD
void ath_rx_process_uapsd(struct ath_softc *sc, HAL_RX_QUEUE qtype, wbuf_t wbuf, struct ath_rx_status *rxs, bool isr_context);
#endif

/*
 * TODO : Adding dummy functions to get the perf optimization compile
 * going. Need to fix.
 */
#define ath_edma_enable_spectral(sc)    do{}while(0)
#define ath_edma_disable_spectral(sc, rxs)    do{}while(0)

#else /* ATH_SUPPORT_EDMA */

#define ATH_RX_EDMA_CONTROL_OBJ(xxx)   u_int8_t xxx

#define	ATH_RXQ_LOCK_INIT(_rxedma)
#define	ATH_RXQ_LOCK_DESTROY(_rxedma)
#define ATH_RXQ_LOCK(_rxedma)
#define ATH_RXQ_UNLOCK(_rxedma)

#define	ATH_TXSTATUS_LOCK_INIT(_sc)
#define	ATH_TXSTATUS_LOCK_DESTROY(_sc)
#define ATH_TXSTATUS_LOCK(_sc)
#define ATH_TXSTATUS_UNLOCK(_sc)

#define ATH_EDMA_TXQ_CONCAT(_tq, _stq)  KASSERT(0,("ATH_SUPPORT_EDMA not defined"))
#define ATH_EDMA_TXQ_MOVE_HEAD_UNTIL(_tq, _stq, _elm, _field) KASSERT(0,("ATH_SUPPORT_EDMA not defined"))
#define ATH_EDMA_TXQ_MOVE_MCASTQ(_tqs,_tqd) KASSERT(0,("ATH_SUPPORT_EDMA not defined"))
#define ATH_EDMA_MCASTQ_MOVE_HEAD_UNTIL(_tq, _stq, _elm, _field) KASSERT(0,("ATH_SUPPORT_EDMA not defined"))

#define ath_edma_attach(sc, ops) /* */

#define ath_tx_edma_tasklet(dev) /* */
#define ath_tx_edma_init(sc)  0  /* */
#define ath_tx_edma_cleanup(sc)  /* */

#define ath_rx_edma_init(dev, nbufs)                /* */
#define ath_rx_edma_requeue(dev, wbuf)              /* */
#define ath_rx_edma_tasklet(dev, flush)             /* */
#define ath_edma_startrecv(sc) 0                    /* */
#define ath_edma_stoprecv(sc, timeout) 0            /* */
#define ath_rx_edma_cleanup(dev)                    /* */
#define ath_rx_edma_intr(dev, status, sched)        /* */
#define ath_edmaAllocRxbufsForFreeList(sc)          /* */
#define ath_rx_intr(dev, qtype)                     /* */


#endif /* ATH_SUPPORT_EDMA */

#endif /* ATH_EDMA_H */

