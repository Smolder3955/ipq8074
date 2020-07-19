/*
 * Copyright (c) 2010, Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 * 
 */

#include "ath_ald.h"
#include "ath_internal.h"
#include "ath_ald_external.h"

#include "if_athvar.h"
#include "ratectrl.h"
#include "ratectrl11n.h"

#define ALD_MAX_DEV_LOAD 300
#define ALD_MSDU_SIZE 1300
#define MAX_AGGR_LIMIT 32
#define DEFAULT_MSDU_SIZE 1000

#if ATH_SUPPORT_HYFI_ENHANCEMENTS

int ath_ald_collect_ni_data(ath_dev_t dev, ath_node_t an, ath_ald_t ald_data, ath_ni_ald_t ald_ni_data)
{
    u_int32_t ccf;
    u_int32_t aggrAverage = 0;
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct atheros_softc  *asc = (struct atheros_softc*)sc->sc_rc;
    struct ath_node *ant = ATH_NODE(an);
    struct atheros_node *pSib;
    TX_RATE_CTRL *pRc;
    TX_RATE_CTRL *pRc_vivo;
    const RATE_TABLE_11N  *pRateTable = (const RATE_TABLE_11N *)asc->hwRateTable[sc->sc_curmode];
    u_int32_t max_msdu_size = MAX(ald_data->msdu_size, ALD_MSDU_SIZE);
    int ac;

    pSib = ATH_NODE_ATHEROS(ant);
    pRc = (TX_RATE_CTRL *)(pSib);
    pRc_vivo  = (TX_RATE_CTRL *)(&pSib->txRateCtrlViVo);

    /* collecting rate and aggregation size stats */
    if( (pRc_vivo->TxRateCount > pRc->TxRateCount) && (pRc_vivo->TxRateCount != 0) ){
    
        ald_ni_data->ald_avgtxrate = pRc_vivo->TxRateInMbps / pRc_vivo->TxRateCount;
        ald_ni_data->ald_avgmax4msaggr = pRc_vivo->Max4msFrameLen / (pRc_vivo->TxRateCount * max_msdu_size); 
        ald_ni_data->ald_lastper = pRc_vivo->state[pRc->lastRateIndex].per;

    } else {
        if(pRc->TxRateCount != 0) {
            ald_ni_data->ald_avgtxrate = pRc->TxRateInMbps / pRc->TxRateCount;
            ald_ni_data->ald_avgmax4msaggr = pRc->Max4msFrameLen / (pRc->TxRateCount * max_msdu_size);
        } else if ( pRc->lastRateIndex ) {
            
            ald_ni_data->ald_avgtxrate = pRateTable->info[pRc->lastRateIndex].rateKbps / 1000;
            ald_ni_data->ald_avgmax4msaggr = pRateTable->info[pRc->lastRateIndex].max4msframelen / (max_msdu_size);
        }
        ald_ni_data->ald_lastper = pRc->state[pRc->lastRateIndex].per;
    }
    
    pRc->TxRateInMbps = 0;
    pRc->Max4msFrameLen = 0;
    pRc->TxRateCount = 0;
    pRc_vivo->TxRateInMbps = 0;
    pRc_vivo->Max4msFrameLen = 0;
    pRc_vivo->TxRateCount = 0;


    if (ald_ni_data->ald_avgmax4msaggr > 32)
        ald_ni_data->ald_avgmax4msaggr = 32;

    if(ald_ni_data->ald_avgtxrate) {
        ccf = ald_ni_data->ald_avgtxrate;
        aggrAverage = ald_ni_data->ald_avgmax4msaggr/2;
    }
    else {
        ccf = pRateTable->info[INIT_RATE_MAX_40-4].rateKbps/1000;
        aggrAverage = MAX_AGGR_LIMIT/2;
    }
    aggrAverage = MAX(1, aggrAverage);
    
    if (ccf != 0) {
        ald_ni_data->ald_capacity = ccf;
        ald_ni_data->ald_aggr = aggrAverage;
        ald_ni_data->ald_phyerr = ald_data->phyerr_rate;
        if (ald_data->msdu_size < DEFAULT_MSDU_SIZE) {
            ald_ni_data->ald_msdusize = ALD_MSDU_SIZE;
        } else {
            ald_ni_data->ald_msdusize = ald_data->msdu_size;
        }
    }

	ald_ni_data->ald_retries = ant->an_retries;
    for (ac = 0; ac < WME_NUM_AC; ac++) {
        /* #buffer overflows per node per AC */
        ald_ni_data->ald_ac_nobufs[ac] = ((&ant->an_tx_ac[ac])->ald_ac_stats).ac_nobufs;
        /* Number of pkts dropped after excessive retries per node per AC */
        ald_ni_data->ald_ac_excretries[ac] = ((&ant->an_tx_ac[ac])->ald_ac_stats).ac_excretries;
        /* #successfully transmitted packets per node per AC */
        ald_ni_data->ald_ac_txpktcnt[ac] = ((&ant->an_tx_ac[ac])->ald_ac_stats).ac_pktcnt;
        /* clear the stats after reading */
        OS_MEMZERO( &((ant->an_tx_ac[ac]).ald_ac_stats) , sizeof( struct ath_ald_ac_stats) );
    }
 
    ald_ni_data->ald_max4msframelen = 0;
	ant->an_retries = 0;
    ald_ni_data->ald_avgtxrate = 0;
    return 0;
}

int ath_ald_collect_data(ath_dev_t dev, ath_ald_t ald_data)
{
    u_int32_t msdu_size=0, load=0;
    int i = 0;
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    spin_lock(&ald_data->ald_lock);

    sc->sc_ieee_ops->ald_update_phy_error_rate(
        ald_data, sc->sc_phy_stats[sc->sc_curmode].ast_rx_phyerr);

    ald_data->ald_txbuf_used = 0;
    for(i=0; i < HAL_NUM_TX_QUEUES; i++) {
        if (ATH_TXQ_SETUP(sc, i)) {
            ald_data->ald_txbuf_used += sc->sc_txq[i].axq_num_buf_used;
        }
    } 
    if ( sc->sc_txbuf_free <= sc->sc_ald.sc_ald_free_buf_lvl )
    {
        sc->sc_ald.sc_ald_buffull_wrn = 0;
    }
    else {
        sc->sc_ald.sc_ald_buffull_wrn = 1;
    }

    if(ald_data->ald_unicast_tx_packets != 0 ){
        msdu_size = ald_data->ald_unicast_tx_bytes/ald_data->ald_unicast_tx_packets;
    }else{
        msdu_size = ALD_MSDU_SIZE;
    }

    ald_data->msdu_size = msdu_size;

    load = ALD_MAX_DEV_LOAD;

    ald_data->ald_unicast_tx_bytes = 0;
    ald_data->ald_unicast_tx_packets = 0;
   
    ald_data->ald_dev_load = load;
    spin_unlock(&ald_data->ald_lock);

    return 0;
}

void ath_ald_update_frame_stats(struct ath_softc *sc, struct ath_buf *bf, struct ath_tx_status *ts)
{
    /* if buffers are above warning level, send bufferfull warning */
    if ( sc->sc_ald.sc_ald_buffull_wrn && ( sc->sc_txbuf_free <= sc->sc_ald.sc_ald_free_buf_lvl) ){
        sc->sc_ieee_ops->buffull_handler(sc->sc_ieee);
        sc->sc_ald.sc_ald_buffull_wrn = 0;
    }

}
/*
void ath_ald_update_rate_stats(struct ath_softc *sc, struct ath_node *an, u_int8_t rix)
{
    struct atheros_softc  *asc = (struct atheros_softc*)sc->sc_rc;
    const RATE_TABLE_11N  *pRateTable = (const RATE_TABLE_11N *)asc->hwRateTable[sc->sc_curmode];
    struct atheros_node   *asn = ATH_NODE_ATHEROS(an);
    TX_RATE_CTRL          *pRc = (TX_RATE_CTRL*)&(asn->txRateCtrl);

    pRc->TxRateInMbps += pRateTable->info[rix].rateKbps / 1000;
    pRc->Max4msFrameLen += pRateTable->info[rix].max4msframelen;
    pRc->TxRateCount += 1;
}
*/
#endif /* ATH_SUPPORT_HYFI_ENHANCEMENTS */
