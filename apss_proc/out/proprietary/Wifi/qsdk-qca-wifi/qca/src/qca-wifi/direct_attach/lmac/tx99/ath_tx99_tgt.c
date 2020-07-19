/*
 *  Copyright (c) 2010 Atheros Communications Inc.
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
 */

/*
 * Copyright (c) 2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include "ath_internal.h"
#include "if_athvar.h"
#include "ath_tx99.h"
#include "ratectrl.h"
#include "ratectrl11n.h"
#include "ar9300/ar9300reg.h"
#include "ar9300/ar9300phy.h"

#if ATH_TX99_DIAG

#include <qdf_types.h>
/* #include <qdf_pci.h> */
/* #include <qdf_dma.h> */
#include <qdf_timer.h>
#include <qdf_lock.h>
/* #include <qdf_io.h> */
#include <qdf_mem.h>
#include <qdf_util.h>
/* #include <qdf_stdtypes.h> */
#include <qdf_defer.h>
#include <qdf_atomic.h>
#include <qdf_nbuf.h>
/* #include <osif_net.h> */
/* #include <osif_net_wcmd.h> */
/* #include <qdf_irq.h> */

#include "if_athvar.h"
#include "ath_internal.h"
#include "ath_tx99.h"
#include "ratectrl.h"
#include "ratectrl11n.h"
#include "ar9300/ar9300reg.h"
#include "ar9300/ar9300phy.h"

static void tx99_bus_dma_sync_single(void *hwdev,
            dma_addr_t dma_handle,
            size_t size, int direction, dma_context_t context)
{
    osdev_t devhandle = (osdev_t)hwdev;
    HAL_BUS_CONTEXT *bc = &(devhandle->bc);
    unsigned long   addr;

    addr = (unsigned long) __va(dma_handle);

    if (!(devhandle->bdev) || bc->bc_bustype == HAL_BUS_TYPE_AHB) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
        dma_cache_sync((void *)addr, size,
            (direction == BUS_DMA_TODEVICE)? DMA_TO_DEVICE : DMA_FROM_DEVICE);
#else
#if defined ANDROID || defined(__LINUX_ARM_ARCH__)
        dma_map_single(devhandle->bdev, (void *)addr, size,
            (direction == BUS_DMA_TODEVICE)? DMA_TO_DEVICE : DMA_FROM_DEVICE);
#else
        dma_cache_sync(devhandle->bdev, (void *)addr, size,
            (direction == BUS_DMA_TODEVICE)? DMA_TO_DEVICE : DMA_FROM_DEVICE);
#endif
#endif
    } else {

        ath_dma_sync_single(devhandle->bdev, dma_handle, size,
            (direction == BUS_DMA_TODEVICE)? PCI_DMA_TODEVICE : PCI_DMA_FROMDEVICE);
    }
}

static dma_addr_t tx99_bus_map_single(void *hwdev, void *ptr,
			 size_t size, int direction)
{
    osdev_t devhandle = (osdev_t)hwdev;
    HAL_BUS_CONTEXT     *bc = &(devhandle->bc);

    if (!(devhandle->bdev) || bc->bc_bustype == HAL_BUS_TYPE_AHB) {
        dma_map_single(devhandle->bdev, ptr, size,
            (direction == BUS_DMA_TODEVICE)? DMA_TO_DEVICE : DMA_FROM_DEVICE);
    } else {
        pci_map_single(devhandle->bdev, ptr, size,
            (direction == BUS_DMA_TODEVICE)? PCI_DMA_TODEVICE : PCI_DMA_FROMDEVICE);
    }

    return __pa(ptr);
}

int
tx99_rate_setup(struct ath_softc *sc, uint32_t *pRateCode, u_int32_t rateKBPS)
{
    struct ath_tx99 *tx99_tgt = sc->sc_tx99;
    struct atheros_softc  *asc = (struct atheros_softc*)sc->sc_rc;
    uint32_t i;

    if(asc == NULL)
    {
    	qdf_print("asc NULL");
    	return -EINVAL;
    }

    {
    	RATE_TABLE_11N  *pRateTable;
        int rate_mode=0;
        /*Mapping enum ieee80211_phymode to enum WIRELESS_MODE*/
        switch(tx99_tgt->txmode){
            case IEEE80211_MODE_11A:
                rate_mode = WIRELESS_MODE_11a;
                break;
            case IEEE80211_MODE_11B:
                rate_mode = WIRELESS_MODE_11b;
                break;
            case IEEE80211_MODE_11G:
                rate_mode = WIRELESS_MODE_11g;
                break;
            case IEEE80211_MODE_TURBO_A:
                rate_mode = WIRELESS_MODE_108a;
                break;
            case IEEE80211_MODE_TURBO_G:
                rate_mode = WIRELESS_MODE_108g;
                break;
            case IEEE80211_MODE_11NA_HT20:
                rate_mode = WIRELESS_MODE_11NA_HT20;
                break;
            case IEEE80211_MODE_11NG_HT20:
                rate_mode = WIRELESS_MODE_11NG_HT20;
                break;
            case IEEE80211_MODE_11NA_HT40PLUS:
                rate_mode = WIRELESS_MODE_11NA_HT40PLUS;
                break;
            case IEEE80211_MODE_11NA_HT40MINUS:
                rate_mode = WIRELESS_MODE_11NA_HT40MINUS;
                break;
            case IEEE80211_MODE_11NG_HT40PLUS:
                rate_mode = WIRELESS_MODE_11NG_HT40PLUS;
                break;
            case IEEE80211_MODE_11NG_HT40MINUS:
                rate_mode = WIRELESS_MODE_11NG_HT40MINUS;
                break;

            case IEEE80211_MODE_FH:
            case IEEE80211_MODE_11NG_HT40:
            case IEEE80211_MODE_11NA_HT40:
            case IEEE80211_MODE_11AC_VHT20:
            case IEEE80211_MODE_11AC_VHT40:
            case IEEE80211_MODE_11AC_VHT80:
            default:
    		    qdf_print("rate mode not supported!");
    		    return -EINVAL;
        }

    	pRateTable = (RATE_TABLE_11N *)asc->hwRateTable[rate_mode];
    	if (pRateTable == NULL) {
    		qdf_print("no 11n rate control table");
    		return -EINVAL;
    	}

    	if (tx99_tgt->txrate != 0) {
    	    for (i = 0; i < pRateTable->rateCount; i++) {
#if 0
/* ignore the supported rate lookup, since TX99 is test mode.
*/
                if ( pRateTable->info[i].valid==FALSE )
            	    continue;
                if (tx99_tgt->htmode == IEEE80211_CWM_MODE20) {
                    if( pRateTable->info[i].valid==TRUE_40 )
                        continue;
                } else {
                    //in IEEE80211_CWM_MODE40 mode
                    if( (tx99_tgt->txrate == 54000) && ( pRateTable->info[i].valid != TRUE_40 ))
                        continue;
                }
#endif
                if (tx99_tgt->txrate == pRateTable->info[i].rateKbps)
                    break;
            }

            if (i == pRateTable->rateCount) {
                /*
                 * Requested rate not found; use the highest rate.
                 */
                i = pRateTable->rateCount-1;
                qdf_print("txrate %u not found, using %u", tx99_tgt->txrate, pRateTable->info[i].rateKbps);
            }
    	} else {
            /*
            * Use the lowest rate for the current channel.
            */
            qdf_print("tx99rate was set to 0, use the lowest rate");
            i = 0;
    	}

    	*pRateCode = pRateTable->info[i].rateCode;
    }
    return EOK;
}

static int
ath_tgt_tx99(struct ath_softc *sc, int chtype)
{
    struct ath_tx99 *tx99_tgt = sc->sc_tx99;
    qdf_nbuf_t buf = tx99_tgt->skb;
    struct ath_buf *bf=NULL, *prev=NULL, *first=NULL, *last=NULL, *tbf;
    struct ath_desc *ds=NULL;
    HAL_11N_RATE_SERIES series[4] = {{ 0 }};
    uint32_t i;
    struct ath_txq *txq;
    uint32_t txrate = 0;
    uint32_t totalbuffer=1; //20
    uint32_t r;
    uint8_t *tmp;
    uint32_t dmalen =0;
    uint32_t duplicate_mode_flag = 0;
 #if UNIFIED_SMARTANTENNA
    uint32_t smartAntenna = SMARTANT_INVALID;
    u_int32_t antenna_array[4]= {0,0,0,0}; /* initilize to zero */
#endif

    if(tx99_tgt->testmode == TX99_TESTMODE_SINGLE_CARRIER)
    {
    	qdf_print("Single carrier mode");
    	ah_tx99_set_single_carrier(sc->sc_ah, tx99_tgt->chanmask, chtype);
    	if (tx99_tgt->skb) {
    	    qdf_nbuf_free(tx99_tgt->skb);
    	    tx99_tgt->skb = NULL;
    	}
    	return EOK;
    }

    /* build output packet */
    {
        struct ieee80211_frame *hdr;
        uint32_t tmplen;

        /* by default use PN9 data */
        static uint8_t PN9Data[] = {0xff, 0x87, 0xb8, 0x59, 0xb7, 0xa1, 0xcc, 0x24,
                         0x57, 0x5e, 0x4b, 0x9c, 0x0e, 0xe9, 0xea, 0x50,
                         0x2a, 0xbe, 0xb4, 0x1b, 0xb6, 0xb0, 0x5d, 0xf1,
                         0xe6, 0x9a, 0xe3, 0x45, 0xfd, 0x2c, 0x53, 0x18,
                         0x0c, 0xca, 0xc9, 0xfb, 0x49, 0x37, 0xe5, 0xa8,
                         0x51, 0x3b, 0x2f, 0x61, 0xaa, 0x72, 0x18, 0x84,
                         0x02, 0x23, 0x23, 0xab, 0x63, 0x89, 0x51, 0xb3,
                         0xe7, 0x8b, 0x72, 0x90, 0x4c, 0xe8, 0xfb, 0xc0};
        uint8_t Data[64] = {0};

        if (tx99_tgt->testmode == TX99_TESTMODE_TX_PN) {
            OS_MEMCPY(Data, PN9Data, sizeof(Data));
        }
        else if (tx99_tgt->testmode == TX99_TESTMODE_TX_ZEROS) {
            OS_MEMSET(Data, 0x0, sizeof(Data));
        }
        else if (tx99_tgt->testmode == TX99_TESTMODE_TX_ONES) {
            OS_MEMSET(Data, 0xFF, sizeof(Data));
        }

        qdf_nbuf_peek_header(buf, &tmp, &tmplen);

        hdr = (struct ieee80211_frame *)tmp;
        IEEE80211_ADDR_COPY(&hdr->i_addr1, sc->sc_myaddr);
        IEEE80211_ADDR_COPY(&hdr->i_addr2, sc->sc_myaddr);
        IEEE80211_ADDR_COPY(&hdr->i_addr3, sc->sc_myaddr);
        hdr->i_dur[0] = 0x0;
        hdr->i_dur[1] = 0x0;
        hdr->i_seq[0] = 0x5a;
        hdr->i_seq[1] = 0x5a;
        hdr->i_fc[0] = IEEE80211_FC0_TYPE_DATA;
        hdr->i_fc[1] = 0;
        dmalen += sizeof(struct ieee80211_frame);
        tmp +=dmalen;
        /* data content */
        for (r = 0; (r + sizeof(Data)) < tmplen; r += sizeof(Data)) {
            qdf_mem_copy(tmp, Data, sizeof(Data));
            tmp +=sizeof(Data);
        }
        qdf_mem_copy(tmp,Data,(sizeof(Data) % r));
        dmalen = tmplen;
    }

    /* to setup tx rate */
    tx99_rate_setup(sc, &txrate, 0);

    /* turn on duplicate mode with legacy rate in HT40 mode */
    if (txrate < 0x80 && tx99_tgt->htmode == IEEE80211_CWM_MODE40)
        duplicate_mode_flag = HAL_TXDESC_EXT_AND_CTL;

    /* to set chainmsk */
    ah_tx99_chainmsk_setup(sc->sc_ah, tx99_tgt->chanmask);
    for (i=0; i<4; i++) {
        series[i].Tries = 0xf;
        series[i].Rate = txrate;
        series[i].ch_sel = tx99_tgt->chanmask;
        series[i].RateFlags = (tx99_tgt->htmode == IEEE80211_CWM_MODE40) ?  HAL_RATESERIES_2040: 0;   //half GI???
    }

    txq = &sc->sc_txq[WME_AC_VO];
    for (i = 0; i < totalbuffer; i++)
    {
    	bf = TAILQ_FIRST(&sc->sc_txbuf);
        if(bf ==  NULL)
        {
            qdf_print("ath_tgt_tx99: allocate ath buffer fail:%d",i+1);
            return -EINVAL;
        }
    	TAILQ_REMOVE(&sc->sc_txbuf, bf, bf_list);

        bf->bf_frmlen = dmalen+IEEE80211_CRC_LEN;
        bf->bf_mpdu = buf;
        bf->bf_qnum = txq->axq_qnum;
        bf->bf_buf_addr[0] = tx99_bus_map_single(sc->sc_osdev, buf->data,
				(UNI_SKB_END_POINTER(buf)-buf->data), BUS_DMA_TODEVICE);
        bf->bf_buf_len[0] = roundup(bf->bf_frmlen, 4);
        ds = bf->bf_desc;
        ath_hal_setdesclink(sc->sc_ah, ds, 0);

        ath_hal_set11n_txdesc(sc->sc_ah, ds
                              , bf->bf_frmlen               /* frame length */
                              , HAL_PKT_TYPE_NORMAL         /* Atheros packet type */
                              , tx99_tgt->txpower           /* txpower */
                              , HAL_TXKEYIX_INVALID         /* key cache index */
                              , HAL_KEY_TYPE_CLEAR          /* key type */
                              , HAL_TXDESC_CLRDMASK
                                | HAL_TXDESC_NOACK          /* flags */
                                | duplicate_mode_flag
                              );

        r = ath_hal_filltxdesc(sc->sc_ah, ds
                               , bf->bf_buf_addr 	        /* buffer address */
                               , bf->bf_buf_len		        /* buffer length */
                               , 0    				        /* descriptor id */
                               , bf->bf_qnum  		        /* QCU number */
                               , HAL_KEY_TYPE_CLEAR         /* key type */
                               , AH_TRUE                    /* first segment */
                               , AH_TRUE                    /* last segment */
                               , ds                         /* first descriptor */
                               );
    	if (r == AH_FALSE) {
            qdf_print("%s: fail fill tx desc r(%d)", __func__, r);
            return -EINVAL;
    	}

 #if UNIFIED_SMARTANTENNA
    smartAntenna = 0;
    for (i =0 ;i <= sc->max_fallback_rates; i++) {
        antenna_array[i] = sc->smart_ant_tx_default;
    }
    ath_hal_set11n_ratescenario(sc->sc_ah, ds, ds, 0, 0, 0 ,
            series, 4, 0, smartAntenna, antenna_array);
#else
    ath_hal_set11n_ratescenario(sc->sc_ah, ds, ds, 0, 0, 0, series, 4, 0, SMARTANT_INVALID);
#endif

    	if (prev != NULL)
            ath_hal_setdesclink(sc->sc_ah, prev->bf_desc, bf->bf_daddr);

    	/* insert the buffers in to tmp_q */
    	TAILQ_INSERT_TAIL(&sc->tx99_tmpq, bf, bf_list);

    	if(i == totalbuffer-1)
    	  last= bf;

    	prev = bf;
    }

    first = TAILQ_FIRST(&sc->tx99_tmpq);

    if(last && first) {
        ath_hal_setdesclink(sc->sc_ah, last->bf_desc, first->bf_daddr);
    }

    ath_hal_intrset(sc->sc_ah, 0);    	/* disable interrupts */
    /* Force receive off */
    /* XXX re-enable RX in DIAG_SW as otherwise xmits hang below */
    ath_hal_stoppcurecv(sc->sc_ah);	/* disable PCU */
    ath_hal_setrxfilter(sc->sc_ah, 0);	/* clear recv filter */
    ath_hal_stopdmarecv(sc->sc_ah, 0); /* disable DMA engine */
    sc->sc_rxlink = NULL;       /* just in case */

    TAILQ_FOREACH(tbf, &sc->tx99_tmpq, bf_list) {
        tx99_bus_dma_sync_single(sc->sc_osdev, tbf->bf_buf_addr[0], (UNI_SKB_END_POINTER(buf)-buf->data),
                        BUS_DMA_TODEVICE, OS_GET_DMA_MEM_CONTEXT(tbf, bf_dmacontext));
        tx99_bus_dma_sync_single(sc->sc_osdev, tbf->bf_daddr,
                        sc->sc_txdesclen, BUS_DMA_TODEVICE, NULL);
    }

    ah_tx99_start(sc->sc_ah, (uint8_t *)(uintptr_t)txq->axq_qnum);

    ath_hal_puttxbuf(sc->sc_ah, txq->axq_qnum, (uint32_t)first->bf_daddr);
    /* trigger tx dma start */
    if (!ath_hal_txstart(sc->sc_ah, txq->axq_qnum)) {
        qdf_print("ath_tgt_tx99: txstart failed, disabled by dfs?");
        return -EINVAL;
    }

    qdf_print("tx99 continuous tx done");
    return EOK;
}

int ath_tx99_tgt_start(struct ath_softc *sc, int chtype)
{
    int ret;

    qdf_print("tx99 parameter dump");
    qdf_print("testmode:%d",sc->sc_tx99->testmode);
    qdf_print("txrate:%d",sc->sc_tx99->txrate);
    qdf_print("txpower:%d",sc->sc_tx99->txpower);
    qdf_print("txchain:%d",sc->sc_tx99->chanmask);
    qdf_print("htmode:%d",sc->sc_tx99->htmode);
    qdf_print("type:%d",sc->sc_tx99->type);
    qdf_print("channel type:%d",chtype);

    TAILQ_INIT(&sc->tx99_tmpq);
    ret = ath_tgt_tx99(sc, chtype);

    qdf_print("Trigger tx99 start done");

    return ret;
}

void ath_tx99_tgt_stop(struct ath_softc *sc)
{
    uint8_t j;
    struct ath_buf *bf=NULL;
    struct ath_tx99 *tx99_tgt = sc->sc_tx99;

    ah_tx99_stop(sc->sc_ah);

    if(tx99_tgt->testmode != TX99_TESTMODE_SINGLE_CARRIER)
    {
        for (j = 0; (bf = TAILQ_FIRST(&sc->tx99_tmpq)) != NULL; j++) {
            TAILQ_REMOVE(&sc->tx99_tmpq, bf, bf_list);
        	TAILQ_INSERT_HEAD(&sc->sc_txbuf, bf, bf_list);
        }
    }
}

#endif /* ATH_TX99_DIAG */
