/*
 * Copyright (c) 2010, Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */
 
#include "ath_internal.h"

#if ATH_DRIVER_SIM

#include "ah_sim.h"

extern void ath_handle_intr(ath_dev_t dev);

void AHSIM_TriggerInterrupt(struct ath_softc *sc, u_int32_t flags) 
{
    AHSIM_ASSERT(sc);

    sc->sc_intrstatus = flags;

    ath_handle_intr(sc);
}

void* AHSIM_GetRxWbuf(struct ath_softc *sc, u_int32_t paddr, HAL_RX_QUEUE queue_type)
{
    struct ath_rx_edma *rxedma;
    wbuf_t wbuf;
    struct ath_buf *bf;

    AHSIM_ASSERT(sc);

    rxedma = &sc->sc_rxedma[queue_type];
    AHSIM_ASSERT(rxedma);

    wbuf = rxedma->rxfifo[(rxedma->rxfifotailindex - 1) & (rxedma->rxfifohwsize - 1)];
    AHSIM_ASSERT(wbuf); /* Hardware Rx buffer address (AR_LP_RXDP queue) and the firmware's address (NULL) do not match */
    
    bf = ATH_GET_RX_CONTEXT_BUF(wbuf);
    AHSIM_ASSERT(bf);

    AHSIM_ASSERT(bf->bf_buf_addr[0] == paddr); /* Hardware Rx buffer address (AR_LP_RXDP queue) and the firmware's address do not match */
    
    return wbuf;
}


void* AHSIM_GetTxAthBuf(struct ath_softc *sc, u_int32_t q, u_int32_t paddr) 
{
    int i;

    AHSIM_ASSERT(sc);

    if (q == sc->sc_bhalq) {
        for (i = 0; i < ATH_VAPSIZE; ++i) {
            if (sc->sc_vaps[i] != NULL && sc->sc_vaps[i]->av_bcbuf != NULL && sc->sc_vaps[i]->av_bcbuf->bf_daddr == paddr)
                return sc->sc_vaps[i]->av_bcbuf;
        }

        AHSIM_ASSERT(0); /* Can't find the Beacon Tx buffer */
    } else {
        struct ath_buf *bf;
        struct ath_txq *txq = &sc->sc_txq[q];
        AHSIM_ASSERT(txq);

        bf = TAILQ_FIRST(&txq->axq_fifo[(txq->axq_headindex - 1) & (HAL_TXFIFO_DEPTH - 1)]);
        AHSIM_ASSERT(bf);

        AHSIM_ASSERT(bf->bf_daddr == paddr); /* Hardware Tx buffer address (AR_QTXDP queue) and the firmware's address do not match */

        return bf;
    }
    
    return NULL;
}

void* AHSIM_AthBufToTxcDesc(void* ath_buf_in) 
{
    struct ath_buf* bf = (struct ath_buf*)ath_buf_in;
    AHSIM_ASSERT(bf);

    return bf->bf_desc;
}

void* AHSIM_GetTxBufferVirtualAddress(void* ath_buf_ptr, void* paddr) 
{
    struct ath_buf *bf = (struct ath_buf *)ath_buf_ptr;
    AHSIM_ASSERT(bf);

    AHSIM_ASSERT(bf->bf_buf_addr[0] == (u_int32_t)paddr);
    
    return wbuf_header((wbuf_t)bf->bf_mpdu);
}

wbuf_t AHSIM_GetTxBuffer(void* ath_buf_ptr) 
{
    struct ath_buf *bf = (struct ath_buf *)ath_buf_ptr;
    AHSIM_ASSERT(bf);

    return (wbuf_t)bf->bf_mpdu;
}

void* AHSIM_GetNextTxBuffer(void* ath_buf_ptr, void* paddr) 
{
    struct ath_buf *bf = (struct ath_buf *)ath_buf_ptr;
    AHSIM_ASSERT(bf);

    bf = TAILQ_NEXT(bf, bf_list);
    AHSIM_ASSERT(bf);

    AHSIM_ASSERT(bf->bf_daddr == (u_int32_t)paddr);

    return bf;
}

void* AHSIM_GetTxStatusDescriptor(struct ath_softc *sc, u_int32_t paddr) 
{
    void* txs;

    AHSIM_ASSERT(sc);
    
    txs = paddr + (caddr_t)sc->sc_txsdma.dd_desc - sc->sc_txsdma.dd_desc_paddr;
    
    return txs;
}

#endif // ATH_DRIVER_SIM
 
