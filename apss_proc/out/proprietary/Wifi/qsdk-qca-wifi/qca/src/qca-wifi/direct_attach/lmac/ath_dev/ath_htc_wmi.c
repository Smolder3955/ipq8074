/*
 * Copyright (c) 2011-2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011-2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 * Copyright (c) 2010, Atheros Communications Inc.
 * All Rights Reserved.
 */

#ifdef ATH_SUPPORT_HTC

#include "ath_internal.h"
#include "ieee80211_var.h" /* XXX todo: fix layer violation - ni_flags need to be accessed via umac api. temp fix */

#include "hif.h"
#include "htc_host_api.h"
#include "wmi.h"
#include "wmi_host_api.h"
#include "host_target_interface.h"

#define TSF_TO_TU(_h,_l) \
    ((((u_int32_t)(_h)) << 22) | (((u_int32_t)(_l)) >> 10))
#ifdef  ATH_HTC_TX_SCHED
static uint16_t ath_htc_can_xmit(struct ath_softc *sc, uint16_t ac);
static void ath_txep_enqueue(struct ath_txep *txep, struct ath_node *an, 
        int32_t tidno,     int switch_tid );
static void ath_htc_txep_queue(struct ath_softc *sc,wbuf_t  wbuf ,
        struct ath_node *an,u_int8_t  tidno,u_int8_t epid);
static int ath_tx_tid_host_sched(struct ath_softc *sc, ath_atx_tid_t * tid,
        struct ath_txep *txep);
static void ath_txep_drain(struct ath_softc *sc, struct ath_txep *txep);
static struct ath_buf * ath_host_buf_alloc(struct ath_softc *sc, uint8_t ac);
static void ath_host_buf_free(struct ath_softc *sc, struct ath_buf *bf);
static int ath_host_txep_schedule(struct ath_softc *sc, struct ath_txep *txep);
static struct ath_buf * ath_buffer_setup(struct ath_softc *sc, 
        ath_bufhead *head , const char *name, int32_t nbuf);
static void ath_htc_tid_cleanup(struct ath_softc *sc, struct ath_atx_tid *tid);
#endif

#ifdef MAGPIE_HIF_GMAC
int ath_htc_send_pkt(ath_dev_t dev, wbuf_t wbuf, HTC_ENDPOINT_ID epid, struct ath_node *an);
int  htc_get_creditinfo(HTC_HANDLE hHTC, HTC_ENDPOINT_ID epid);
#endif


/* Struct for WMI Command */
struct ath_wmi_rate {
    struct target_rateset rates;
    struct target_rateset htrates;
};

struct ath_wmi_rc_state_change_cmd {
    u_int8_t    vap_index;
    u_int8_t    vap_state;
    u_int16_t   dummy;    /* src3 fixed 9D08 */    
    u_int32_t   capflag;

    struct ath_wmi_rate rs;
};

struct ath_wmi_rc_rate_update_cmd {
    u_int8_t    node_index;
    u_int8_t    isnew;
    u_int16_t   dummy;    /* src3 fixed 9D08 */
    u_int32_t   capflag;

    struct ath_wmi_rate rs;
};

struct ath_wmi_aggr_info {
    u_int8_t nodeindex;
    u_int8_t tidno;
    u_int8_t aggr_enable;
    u_int8_t padding;
};

struct ath_wmi_generic_timer_info {
    int index;
    u_int32_t timer_next;
    u_int32_t timer_period;
    int mode;
};

struct ath_wmi_tgt_pause_info {
    u_int32_t nodeindex;
    u_int32_t pause_state;
};

enum ath_wmi_vap_state {
	ATH_WMI_VAP_INIT	= 0,	/* default state */
	ATH_WMI_VAP_SCAN	= 1,	/* scanning */
	ATH_WMI_VAP_JOIN	= 2,	/* join */
	ATH_WMI_VAP_AUTH	= 3,	/* try to authenticate */
	ATH_WMI_VAP_ASSOC	= 4,	/* try to assoc */
	ATH_WMI_VAP_RUN		= 5,	/* associated */
};

#if defined(MAGPIE_HIF_GMAC) || defined(MAGPIE_HIF_USB)
u_int32_t 
ath_wmi_node_getrate(ath_dev_t dev, void *target, int size)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    u_int32_t ratekbps;
    
    wmi_cmd(sc->sc_host_wmi_handle,
            WMI_NODE_GETRATE_CMDID,
            target,
            size,
            (u_int8_t *) &ratekbps,
            sizeof(ratekbps),
            ATH_HTC_WMI_TIMEOUT_DEFAULT);    
    
    return ratekbps;
}    
#endif
#ifdef MAGPIE_HIF_GMAC
void
ath_wmi_seek_credit(struct ath_softc *sc)
{
    uint8_t cmd_status;

    wmi_cmd(sc->sc_host_wmi_handle,
            WMI_HOST_SEEK_CREDIT,
            NULL,
            0,
            &cmd_status,
            sizeof(cmd_status),
            ATH_HTC_WMI_TIMEOUT_DEFAULT);    
}
#endif


void
ath_wmi_add_vap(ath_dev_t dev, void *target, int size)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    u_int32_t cmd_status;

    wmi_cmd(sc->sc_host_wmi_handle,
            WMI_VAP_CREATE_CMDID, 
            target,
            size,
            (u_int8_t *) &cmd_status,
            sizeof(cmd_status),
            ATH_HTC_WMI_TIMEOUT_DEFAULT);
}

void
ath_wmi_delete_vap(ath_dev_t dev, void *target, int size)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    u_int32_t cmd_status;

    wmi_cmd(sc->sc_host_wmi_handle,
            WMI_VAP_REMOVE_CMDID, 
            target,
            size,
            (u_int8_t *) &cmd_status,
            sizeof(cmd_status),
            ATH_HTC_WMI_TIMEOUT_DEFAULT);
}

void
ath_wmi_add_node(ath_dev_t dev, void *target, int size)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    u_int32_t cmd_status;

    wmi_cmd(sc->sc_host_wmi_handle, 
            WMI_NODE_CREATE_CMDID, 
            target,
            size,
            (u_int8_t *) &cmd_status,
            sizeof(cmd_status),
            ATH_HTC_WMI_TIMEOUT_DEFAULT);
}

void
ath_wmi_delete_node(ath_dev_t dev, void *target, int size)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    u_int32_t cmd_status;

    wmi_cmd(sc->sc_host_wmi_handle, 
            WMI_NODE_REMOVE_CMDID, 
            target,
            size,
            (u_int8_t *) &cmd_status,
            sizeof(cmd_status),
            ATH_HTC_WMI_TIMEOUT_DEFAULT);
}

void
ath_wmi_update_node(ath_dev_t dev, void *target, int size)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    u_int32_t cmd_status;

    wmi_cmd(sc->sc_host_wmi_handle, 
            WMI_NODE_UPDATE_CMDID, 
            target,
            size,
            (u_int8_t *)&cmd_status,
            sizeof(cmd_status),
            ATH_HTC_WMI_TIMEOUT_DEFAULT);
}

#if ENCAP_OFFLOAD
void
ath_wmi_update_vap(ath_dev_t dev, void *target, int size)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    u_int32_t cmd_status;
    
    wmi_cmd(sc->sc_host_wmi_handle,
            WMI_VAP_UPDATE_CMDID,
            target,
            size,
            (u_int8_t *) &cmd_status,
            sizeof(cmd_status),
            ATH_HTC_WMI_TIMEOUT_DEFAULT);
}
#endif

#if ATH_K2_PS_BEACON_FILTER
void ath_wmi_beacon_filter_enable(struct ath_softc *sc)
{
    int cmd_status;

    wmi_cmd(sc->sc_host_wmi_handle,
           WMI_ENABLE_BEACON_FILTER_CMD, 
           NULL, 
           0,
           (u_int8_t *) &cmd_status,
           sizeof(cmd_status),
           ATH_HTC_WMI_TIMEOUT_DEFAULT);
}
#endif

void 
ath_wmi_ic_update_target(ath_dev_t dev, void *target, int size)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    u_int32_t cmd_status;
    wmi_cmd(sc->sc_host_wmi_handle, 
            WMI_TARGET_IC_UPDATE_CMDID,
            (u_int8_t *) target,
            size,
            (u_int8_t *) &cmd_status,
            sizeof(cmd_status),
            ATH_HTC_WMI_TIMEOUT_DEFAULT);
    
    /* TODO: command status check */
}

void
ath_wmi_rc_state_change(ath_dev_t dev, unsigned int capflag,
       struct ieee80211_rateset *negotiated_rates, 
       struct ieee80211_rateset *negotiated_htrates)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    u_int32_t cmd_status;
    struct ath_wmi_rc_state_change_cmd wmi_data;

    wmi_data.vap_index = 0;                 // Current we fixed to vap_index = 0;
    wmi_data.vap_state = ATH_WMI_VAP_RUN;   // FIX the value
    wmi_data.capflag = cpu_to_be32(capflag);
    OS_MEMCPY(&wmi_data.rs.rates, negotiated_rates, sizeof(struct target_rateset));
    OS_MEMCPY(&wmi_data.rs.htrates, negotiated_htrates, sizeof(struct target_rateset));

    wmi_cmd(sc->sc_host_wmi_handle, 
            WMI_RC_STATE_CHANGE_CMDID, 
            (u_int8_t *) &wmi_data,
            sizeof(struct ath_wmi_rc_state_change_cmd),
            (u_int8_t *) &cmd_status,
            sizeof(cmd_status),
            ATH_HTC_WMI_TIMEOUT_DEFAULT);
}

void
ath_wmi_rc_node_update(ath_dev_t dev, struct ath_node *an, int isnew, unsigned int capflag,
        struct ieee80211_rateset *negotiated_rates, 
        struct ieee80211_rateset *negotiated_htrates)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    u_int32_t cmd_status;
    struct ath_wmi_rc_rate_update_cmd wmi_data;
    u_int8_t node_index = an->an_sc->sc_ieee_ops->ath_htc_gettargetnodeid(an->an_sc->sc_ieee, an->an_node);

    if (node_index >= ATH_HTC_MAX_NODE_NUM) {
        printk("%s: node_index exceeds the HTC capability, node_index = %d !\n", __func__, node_index);
        return;
    }

    /* printk("%s: node_index = %d, isnew = %d, capflag = 0x%x, rate num = %d(legacy) - %d(11n)\n", 
           __func__, node_index, isnew, capflag, 
           negotiated_rates->rs_nrates, negotiated_htrates->rs_nrates); */
#if 0
    for (i = 0; i < negotiated_rates->rs_nrates; i++) {
        printk("%s: Legacy(%d/%d) = 0x%x\n", __func__, i, negotiated_rates->rs_nrates, negotiated_rates->rs_rates[i]);
    }
    for (i = 0; i < negotiated_htrates->rs_nrates; i++) {
        printk("%s: 11n(%d/%d) = 0x%x\n", __func__, i, negotiated_htrates->rs_nrates, negotiated_htrates->rs_rates[i]);
    }
#endif
    
    OS_MEMZERO(&wmi_data, sizeof(struct ath_wmi_rc_rate_update_cmd));
    wmi_data.node_index = node_index;
    wmi_data.isnew = isnew;
    wmi_data.capflag = cpu_to_be32(capflag);
    OS_MEMCPY(&wmi_data.rs.rates, negotiated_rates, sizeof(struct target_rateset));
    OS_MEMCPY(&wmi_data.rs.htrates, negotiated_htrates, sizeof(struct target_rateset));

    wmi_cmd(sc->sc_host_wmi_handle, 
            WMI_RC_RATE_UPDATE_CMDID, 
            (u_int8_t *) &wmi_data,
            sizeof(struct ath_wmi_rc_rate_update_cmd), 
            (u_int8_t *) &cmd_status,
            sizeof(cmd_status),
            ATH_HTC_WMI_TIMEOUT_DEFAULT);
}

void
ath_wmi_aggr_enable(ath_dev_t dev, struct ath_node *an, u_int8_t tidno, u_int8_t aggr_enable)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_wmi_aggr_info aggr_info;
    u_int32_t cmd_status;
    u_int8_t node_index = an->an_sc->sc_ieee_ops->ath_htc_gettargetnodeid(an->an_sc->sc_ieee, an->an_node);
    if (tidno >= ATH_HTC_WME_NUM_TID) {
        //printk("%s: tidno exceeds the HTC capability, tidno = %d !\n", __func__, tidno);
        return;
    }

    if (node_index >= ATH_HTC_MAX_NODE_NUM) {
        printk("%s: node_index exceeds the HTC capability, node_index = %d !\n", __func__, node_index);
        return;
    }

    //printk("%s: node_index = %d, tidno = %d, aggr_enable = %d\n", __func__, node_index, tidno, aggr_enable);
    
    OS_MEMZERO(&aggr_info, sizeof(struct ath_wmi_aggr_info));
    aggr_info.nodeindex = node_index;
    aggr_info.tidno = tidno;
	aggr_info.aggr_enable = aggr_enable;

    wmi_cmd(sc->sc_host_wmi_handle, 
            WMI_TX_AGGR_ENABLE_CMDID,
            (u_int8_t *) &aggr_info,
            sizeof(struct ath_wmi_aggr_info), 
            (u_int8_t *) &cmd_status,
            sizeof(cmd_status),
            ATH_HTC_WMI_TIMEOUT_DEFAULT);
}

void
ath_wmi_set_desc_tpc(ath_dev_t dev, u_int8_t enable)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    u_int32_t cmd_status;
    u_int8_t setting = enable;
    
    wmi_cmd(sc->sc_host_wmi_handle, 
            WMI_TGT_SET_DESC_TPC,
            &setting,
            sizeof(u_int8_t), 
            (u_int8_t *) &cmd_status,
            sizeof(cmd_status),
            ATH_HTC_WMI_TIMEOUT_DEFAULT);
}
#ifndef MAGPIE_HIF_GMAC
void 
ath_htc_rx(ath_dev_t dev, wbuf_t wbuf)
{
    struct ath_softc            *sc = ATH_DEV_TO_SC(dev);
    struct ath_buf              *bf;
    struct ath_buf              *bf_first;
    struct ath_desc             *ds;
    struct ath_htc_rx_status    *htcRxStats;
    struct ath_rx_status        rxstats;
    u_int32_t                   length = 0;

    ATH_RXBUF_LOCK(sc);
    
    bf_first = TAILQ_FIRST(&sc->sc_rxbuf);
    /* Search for Rx descriptor list. */
    for (bf = bf_first; bf; bf = TAILQ_NEXT(bf, bf_list)) {
        /* Check the first bf without ATH_BUFSTATUS_DONE */
        if (bf->bf_status == 0)
            break;
    }

    if (bf == NULL)
    {
        printk("%s: bf is NULL\n", __func__);
        wbuf_free(wbuf);
        ATH_RXBUF_UNLOCK(sc);
        return;
    }

#ifndef __linux__
    length = wbuf_get_len(wbuf);
#else    
    length = wbuf_get_pktlen(wbuf);
#endif    

    if (length <= sizeof(struct rx_frame_header)) {
        //printk("%s: drop rx garbage\n", __func__);
        wbuf_free(wbuf);
        ATH_RXBUF_UNLOCK(sc);
        return;
    }

    htcRxStats = wbuf_raw_data(wbuf);
    rxstats.rs_tstamp = be64_to_cpu(htcRxStats->rs_tstamp);
    rxstats.rs_datalen = be16_to_cpu(htcRxStats->rs_datalen);
    rxstats.rs_status = htcRxStats->rs_status;
    rxstats.rs_phyerr = htcRxStats->rs_phyerr;
    rxstats.rs_rssi = htcRxStats->rs_rssi;
    rxstats.rs_rssi_ctl0 = htcRxStats->rs_rssi_ctl0;
    rxstats.rs_rssi_ctl1 = htcRxStats->rs_rssi_ctl1;
    rxstats.rs_rssi_ctl2 = htcRxStats->rs_rssi_ctl2;
    rxstats.rs_rssi_ext0 = htcRxStats->rs_rssi_ext0;
    rxstats.rs_rssi_ext1 = htcRxStats->rs_rssi_ext1;
    rxstats.rs_rssi_ext2 = htcRxStats->rs_rssi_ext2;
    rxstats.rs_keyix = htcRxStats->rs_keyix;
    rxstats.rs_rate = htcRxStats->rs_rate;
    rxstats.rs_antenna = htcRxStats->rs_antenna;
    rxstats.rs_more = htcRxStats->rs_more;
    rxstats.rs_isaggr = htcRxStats->rs_isaggr;
    rxstats.rs_moreaggr = htcRxStats->rs_moreaggr;
    rxstats.rs_num_delims = htcRxStats->rs_num_delims;
    rxstats.rs_flags = htcRxStats->rs_flags;
    rxstats.evm0 = be32_to_cpu(htcRxStats->evm0);
    rxstats.evm1 = be32_to_cpu(htcRxStats->evm1);
    rxstats.evm2 = be32_to_cpu(htcRxStats->evm2);

    /* check the size of FCS */
    if (rxstats.rs_datalen - (length - sizeof(struct rx_frame_header)) != 0) {
        // qdf_print("%s: rs.datalen %d len %d", __func__,rxstats.rs_datalen,(length - sizeof(struct rx_frame_header)));
#ifdef MAGPIE_HIF_GMAC
        if (length == 1474)
            rxstats.rs_datalen = length - sizeof(struct rx_frame_header);
        else 
#endif            
        {
            wbuf_free(wbuf); 
            ATH_RXBUF_UNLOCK(sc); 
            return;
        }
    }
    
    /* Copy the rx_status */
    ds = bf->bf_desc;
    OS_MEMCPY(&(ds->ds_rxstat), &rxstats, sizeof(struct ath_rx_status));

    wbuf_pull(wbuf, sizeof(struct rx_frame_header));

    /* Because FCS will be trimed in the target firmware, we need to compensate
       the length.
     */
#ifdef __linux__
    wbuf_set_pktlen(wbuf, ds->ds_rxstat.rs_datalen);
#else
    /* XXX: we may use wbuf_set_pktlen() on Windows */
    wbuf_init(wbuf, ds->ds_rxstat.rs_datalen);
#endif

#ifdef ATH_SUPPORT_UAPSD
    /* Check for valid UAPSD trigger */
    sc->sc_ieee_ops->check_uapsdtrigger(sc->sc_pdev, wbuf_raw_data(wbuf), rxstats.rs_keyix, true);
#endif

    bf->bf_mpdu = wbuf;
    
    bf->bf_status = ATH_BUFSTATUS_DONE;
    
    ATH_RXBUF_UNLOCK(sc);

    /* Schedule Rx tasklet */
    ATHUSB_SCHEDULE_TQUEUE(&sc->sc_osdev->rx_tq);
}

int
ath_htc_rx_tasklet(ath_dev_t dev, int flush)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_buf *bf;
#if defined(ATH_SUPPORT_DFS)
    struct ath_hal *ah = sc->sc_ah;
#endif
    struct ath_desc *ds;
    int type;
    u_int phyerr;
    struct ieee80211_frame *wh;
    wbuf_t wbuf = NULL;
    ieee80211_rx_status_t rx_status;
    struct ath_phy_stats *phy_stats = &sc->sc_phy_stats[sc->sc_curmode];
    int rx_processed = 0;

    do {
        u_int8_t frame_fc0;

        /* If handling rx interrupt and flush is in progress => exit */
        if (sc->sc_rxflush && (flush == RX_PROCESS)) {
            break;
        }
        if (sc->sc_htcrxpause)
	         break;

        ATH_RXBUF_LOCK(sc);
        bf = TAILQ_FIRST(&sc->sc_rxbuf);
        if (bf == NULL) {
            sc->sc_rxlink = NULL;
            ATH_RXBUF_UNLOCK(sc);
            break;
        }

        /*
         * Descriptors are now processed in the first-level
         * interrupt handler to support U-APSD trigger search.
         * This must also be done even when U-APSD is not active to support
         * other error handling that requires immediate attention.
         * We check bf_status to find out if the bf's descriptors have 
         * been processed by the HAL.
         */
        if (!(bf->bf_status & ATH_BUFSTATUS_DONE)) {
            ATH_RXBUF_UNLOCK(sc);
            break;
        }

        ds = bf->bf_desc;
        ++rx_processed;

        wbuf = bf->bf_mpdu;
        if (wbuf == NULL) {		/* XXX ??? can this happen */
            printk("no mpdu (%s)\n", __func__);
            ATH_RXBUF_UNLOCK(sc);
            continue;
        }

        TAILQ_REMOVE(&sc->sc_rxbuf, bf, bf_list);

        /*
         * Release the lock here in case ieee80211_input() return
         * the frame immediately by calling ath_rx_mpdu_requeue().
         */
        ATH_RXBUF_UNLOCK(sc);

        if (flush == RX_DROP) {
            /*
             * If we're asked to flush receive queue, directly
             * chain it back at the queue without processing it.
             */
            printk("%s: drop rx frame when flush rx queue\n", __func__);
            
            wbuf_free(wbuf);
            goto rx_next;
        }
        
        wh = (struct ieee80211_frame *)wbuf_raw_data(wbuf);
        frame_fc0 = wh->i_fc[0];
        OS_MEMZERO(&rx_status, sizeof(ieee80211_rx_status_t));

        if (ds->ds_rxstat.rs_status != 0) {
            phy_stats->ast_rx_err++;
            if (ds->ds_rxstat.rs_status & HAL_RXERR_CRC) {
                rx_status.flags |= ATH_RX_FCS_ERROR;
				phy_stats->ast_rx_crcerr++;
            }

            if (ds->ds_rxstat.rs_status & HAL_RXERR_FIFO)
                phy_stats->ast_rx_fifoerr++;
            if (ds->ds_rxstat.rs_status & HAL_RXERR_PHY) {
                phy_stats->ast_rx_phyerr++;
                phyerr = ds->ds_rxstat.rs_phyerr & 0x1f;
                phy_stats->ast_rx_phy[phyerr]++;
#ifdef ATH_SUPPORT_DFS
                if (sc->sc_dfs) {
                    u_int64_t tsf = ath_hal_gettsf64(ah);
                    /* Process phyerrs */
                    ath_process_phyerr(sc, bf, &ds->ds_rxstat, tsf);
                }
#endif
                if (sc->sc_opmode != HAL_M_MONITOR ||
                    ATH_WIRESHARK_FILTER_PHY_ERR(sc) )
                {
                    wbuf_free(wbuf);
                    goto rx_next;
				}
            }

            if (ds->ds_rxstat.rs_status & HAL_RXERR_DECRYPT) {
                /*
                 * Decrypt error. We only mark packet status here
                 * and always push up the frame up to let NET80211 layer
                 * handle the actual error case, be it no decryption key
                 * or real decryption error.
                 * This let us keep statistics there.
                 */
                phy_stats->ast_rx_decrypterr++;
                rx_status.flags |= ATH_RX_DECRYPT_ERROR;

            } else if (ds->ds_rxstat.rs_status & HAL_RXERR_MIC) {
                /*
                 * Demic error. We only mark frame status here
                 * and always push up the frame up to let NET80211 layer
                 * handle the actual error case.
                 * This let us keep statistics there and also apply the
                 * WAR for bug 6903: (Venice?) Hardware may
                 * post a false-positive MIC error.  Need to expose this
                 * error to tkip_demic() to really declare a failure.
                 */
                if ((frame_fc0 & IEEE80211_FC0_TYPE_MASK) == IEEE80211_FC0_TYPE_CTL) {
                    /*
                     * As doc. in hardware bug 30127, sometimes, we get invalid
                     * MIC failures on valid control frames. Remove these mic errors.
                     */
                    ds->ds_rxstat.rs_status &= ~HAL_RXERR_MIC;
                }
                else {
                    rx_status.flags |= ATH_RX_MIC_ERROR;
                }
            }

            /*
             * Reject error frames with the exception of
             * decryption and MIC failures. For monitor mode, we also
             * ignore the CRC error.
             */
            if (sc->sc_opmode == HAL_M_MONITOR) {
                u_int32_t mask;
                mask = HAL_RXERR_DECRYPT | HAL_RXERR_MIC |
                       HAL_RXERR_KEYMISS | HAL_RXERR_CRC;
                if ( ! ATH_WIRESHARK_FILTER_PHY_ERR(sc) ) {
                    mask |= HAL_RXERR_PHY;
                }
                if (ds->ds_rxstat.rs_status & ~mask) {
                    wbuf_free(wbuf);
                    goto rx_next;
                }
            } else {
                if (ds->ds_rxstat.rs_status &
                    ~(HAL_RXERR_DECRYPT | HAL_RXERR_MIC | HAL_RXERR_KEYMISS)) {
                    wbuf_free(wbuf);
                    goto rx_next;
                } else {
                    if (ds->ds_rxstat.rs_status & HAL_RXERR_KEYMISS) {
                        rx_status.flags |= ATH_RX_KEYMISS;
                    }
                }
            }
        }

        if (sc->sc_hashtsupport) {
            if (ds->ds_rxstat.rs_moreaggr == 0) {
                int rssi_chain_valid_count = 0;
                int numchains = sc->sc_rx_numchains;

                if (ds->ds_rxstat.rs_rssi != ATH_RSSI_BAD) {
                    rx_status.rssi = ds->ds_rxstat.rs_rssi;
                    rx_status.flags |= ATH_RX_RSSI_VALID;
                }
                if (ds->ds_rxstat.rs_rssi_ctl0 != ATH_RSSI_BAD) {
                    rx_status.rssictl[0]  = ds->ds_rxstat.rs_rssi_ctl0;
                    rssi_chain_valid_count++;
                }
                
                if (ds->ds_rxstat.rs_rssi_ctl1 != ATH_RSSI_BAD) {
                    rx_status.rssictl[1]  = ds->ds_rxstat.rs_rssi_ctl1;
                    rssi_chain_valid_count++;
                }        
                
                if (ds->ds_rxstat.rs_rssi_ctl2 != ATH_RSSI_BAD) {
                    rx_status.rssictl[2]  = ds->ds_rxstat.rs_rssi_ctl2;
                    rssi_chain_valid_count++;
                }
                                
                if (ds->ds_rxstat.rs_flags & HAL_RX_2040) {
                    int rssi_extn_valid_count = 0;
                    if (ds->ds_rxstat.rs_rssi_ext0 != ATH_RSSI_BAD) {
                        rx_status.rssiextn[0]  = ds->ds_rxstat.rs_rssi_ext0;
                        rssi_extn_valid_count++;
                    }
                    if (ds->ds_rxstat.rs_rssi_ext1 != ATH_RSSI_BAD) {
                        rx_status.rssiextn[1]  = ds->ds_rxstat.rs_rssi_ext1;
                        rssi_extn_valid_count++;
                    }
                    if (ds->ds_rxstat.rs_rssi_ext2 != ATH_RSSI_BAD) {
                        rx_status.rssiextn[2]  = ds->ds_rxstat.rs_rssi_ext2;
                        rssi_extn_valid_count++;
                    }
                    if (rssi_extn_valid_count == numchains) {
                        rx_status.flags |= ATH_RX_RSSI_EXTN_VALID;
                    }                        
                }
                if (rssi_chain_valid_count == numchains) {
                    rx_status.flags |= ATH_RX_CHAIN_RSSI_VALID;
                }                    
            }
        } 
        else {
            /*
             * Need to insert the "combined" rssi into the status structure
             * for upper layer processing
             */
            
            rx_status.rssi = ds->ds_rxstat.rs_rssi;
            rx_status.flags |= ATH_RX_RSSI_VALID;
        }
        rx_status.tsf = ds->ds_rxstat.rs_tstamp;

        rx_status.rateieee = sc->sc_hwmap[ds->ds_rxstat.rs_rate].ieeerate;
        rx_status.rateKbps = sc->sc_hwmap[ds->ds_rxstat.rs_rate].rateKbps;
        rx_status.ratecode = ds->ds_rxstat.rs_rate;
        /* HT rate */
        if (rx_status.ratecode & 0x80) {
            /* TODO - add table to avoid division */
            if (ds->ds_rxstat.rs_flags & HAL_RX_2040) {
                rx_status.flags |= ATH_RX_40MHZ;
                rx_status.rateKbps = (rx_status.rateKbps * 27) / 13;
            }
            if (ds->ds_rxstat.rs_flags & HAL_RX_GI) {
                rx_status.flags |= ATH_RX_SHORT_GI;
                rx_status.rateKbps = (rx_status.rateKbps * 10) / 9;
            }
        }

        /* sc->sc_noise_floor is only available when the station attaches to an AP,
         * so we use a default value if we are not yet attached.
         */
        /* XXX we should use either sc->sc_noise_floor or
         * ath_hal_getChanNoise(ah, &sc->sc_curchan) 
         * to calculate the noise floor.
         * However, the value returned by ath_hal_getChanNoise seems to be incorrect
         * (-31dBm on the last test), so we will use a hard-coded value until we 
         * figure out what is going on.
         */
        rx_status.abs_rssi = ds->ds_rxstat.rs_rssi + ATH_DEFAULT_NOISE_FLOOR;
        
        /* increment count of received bytes */
        sc->sc_stats.ast_rx_bytes += wbuf_get_pktlen(wbuf);

        /*
         * Pass frames up to the stack.
         * Note: After calling ath_rx_indicate(), we should not assumed that the
         * contents of wbuf, wh, and ds are valid.
         */
        type = ath_rx_indicate(sc, wbuf, &rx_status, ds->ds_rxstat.rs_keyix);

#ifdef ATH_ADDITIONAL_STATS
        if (type == IEEE80211_FC0_TYPE_DATA) {
            sc->sc_stats.ast_rx_num_data++;
        }
        else if (type == IEEE80211_FC0_TYPE_MGT) {
#ifdef QCA_SUPPORT_CP_STATS
            pdev_cp_stats_rx_num_mgmt_inc(sc->sc_pdev, 1);
#else
            sc->sc_stats.ast_rx_num_mgmt++;
#endif
        }
        else if (type == IEEE80211_FC0_TYPE_CTL) {
#ifdef QCA_SUPPORT_CP_STATS
            pdev_cp_stats_rx_num_ctl_inc(sc->sc_pdev, 1);
#else
            sc->sc_stats.ast_rx_num_ctl++;
#endif
        }
        else {
#ifdef QCA_SUPPORT_CP_STATS
            pdev_lmac_cp_stats_ast_rx_num_unknown_inc(sc->sc_pdev, 1);
#else
            sc->sc_stats.ast_rx_num_unknown++;
#endif
        }
#endif

rx_next:
      ATH_RXBUF_LOCK(sc);

      /* Clear bf_status */
      ATH_RXBUF_RESET(bf);

	  /* Clear wbuf pointer */
	  bf->bf_mpdu = NULL;

      /* Put bf to the end of the list */
      TAILQ_INSERT_TAIL(&sc->sc_rxbuf, bf, bf_list);
      ATH_RXBUF_UNLOCK(sc);

    } while (TRUE);

#ifdef ATH_ADDITIONAL_STATS
        if (rx_processed < ATH_RXBUF ) {
                sc->sc_stats.ast_pkts_per_intr[rx_processed]++;
        }
        else {
                sc->sc_stats.ast_pkts_per_intr[ATH_RXBUF]++;
        }
#endif

    return 0;
}
#else


static int ath_htc_process_rxhdr(
        wbuf_t wbuf ,
        struct ath_rx_status * rxstats 
        )
{
    struct rx_frame_header * rxhdr;
    u_int32_t                   length = 0;
#ifndef ATHHTC_AP_REMOVE_STATS    
    struct ath_htc_rx_status    *htcRxStats;
    htcRxStats = wbuf_raw_data(wbuf);
#endif    
   
#ifdef ATH_HTC_SG_SUPPORT
    rxhdr = wbuf_raw_data(wbuf) ;
#endif    

#ifndef __linux__
    length = wbuf_get_len(wbuf);
#else    
    length = wbuf_get_pktlen(wbuf);
#endif    

    if (length < sizeof(struct rx_frame_header)) {
        //printk("%s: drop rx garbage\n", __func__);
        wbuf_free(wbuf);
        return -1;
    }
    
#ifdef ATHHTC_AP_REMOVE_STATS    
    rxstats->rs_datalen = be16_to_cpu(rxstats->rs_datalen);
#else
    rxstats->rs_tstamp = be64_to_cpu(htcRxStats->rs_tstamp);
    rxstats->rs_datalen = be16_to_cpu(htcRxStats->rs_datalen);
    rxstats->rs_status = htcRxStats->rs_status;
    rxstats->rs_phyerr = htcRxStats->rs_phyerr;
    rxstats->rs_rssi = htcRxStats->rs_rssi;
    rxstats->rs_rssi_ctl0 = htcRxStats->rs_rssi_ctl0;
    rxstats->rs_rssi_ctl1 = htcRxStats->rs_rssi_ctl1;
    rxstats->rs_rssi_ctl2 = htcRxStats->rs_rssi_ctl2;
    rxstats->rs_rssi_ext0 = htcRxStats->rs_rssi_ext0;
    rxstats->rs_rssi_ext1 = htcRxStats->rs_rssi_ext1;
    rxstats->rs_rssi_ext2 = htcRxStats->rs_rssi_ext2;
    rxstats->rs_keyix = htcRxStats->rs_keyix;
    rxstats->rs_rate = htcRxStats->rs_rate;
    rxstats->rs_antenna = htcRxStats->rs_antenna;
    rxstats->rs_more = htcRxStats->rs_more;
    rxstats->rs_isaggr = htcRxStats->rs_isaggr;
    rxstats->rs_moreaggr = htcRxStats->rs_moreaggr;
    rxstats->rs_num_delims = htcRxStats->rs_num_delims;
    rxstats->rs_flags = htcRxStats->rs_flags;
    rxstats->evm0 = be32_to_cpu(htcRxStats->evm0);
    rxstats->evm1 = be32_to_cpu(htcRxStats->evm1);
    rxstats->evm2 = be32_to_cpu(htcRxStats->evm2);
#endif 

    wbuf_pull(wbuf, sizeof(struct rx_frame_header));

#ifdef __linux__
#ifdef ATH_HTC_SG_SUPPORT
    if(rxhdr->is_seg){
        int length0 = be32_to_cpu(rxhdr->sg_length)- sizeof(struct rx_frame_header);
      
        wbuf_set_pktlen(wbuf,length0);


    }
    else {
#endif        

        wbuf_set_pktlen(wbuf,rxstats->rs_datalen);
     }
#else
    /* XXX: we may use wbuf_set_pktlen() on Windows */
    wbuf_init(wbuf, rxstats->rs_datalen);
#endif
    /* Rx Process Stats */
   return 0;


}
#ifdef ATHHTC_AP_REMOVE_STATS
static int 
ath_process_rx_stats(struct ath_softc *sc ,wbuf_t wbuf,
        ieee80211_rx_status_t  *rx_stats,struct ath_rx_status *pkt_rx_stats,u_int64_t rx_tsf)
{
    struct ath_phy_stats *phy_stats = &sc->sc_phy_stats[sc->sc_curmode];
    u_int8_t frame_fc0;
    struct ieee80211_frame *wh;
    u_int phyerr;

    wh = (struct ieee80211_frame *)wbuf_raw_data(wbuf);
    frame_fc0 = wh->i_fc[0];

    if (pkt_rx_stats->rs_status != 0) {
        if (pkt_rx_stats->rs_status & HAL_RXERR_PHY) {
            phy_stats->ast_rx_phyerr++;
            phyerr = pkt_rx_stats->rs_phyerr & 0x1f;
            phy_stats->ast_rx_phy[phyerr]++;
#ifdef ATH_SUPPORT_DFS
            {
                u_int64_t tsf = rx_tsf;
                struct ath_buf bf;
                bf.bf_vdata = wbuf_raw_data(wbuf);
                bf.bf_mpdu = wbuf;
                /* Process phyerrs */
                ath_process_phyerr(sc, &bf, pkt_rx_stats, tsf);
            }
#endif
        }
        if(pkt_rx_stats->rs_status == 0x40){
             return 0;
        }


        if (sc->sc_opmode != HAL_M_MONITOR ||
                ATH_WIRESHARK_FILTER_PHY_ERR(sc) )
        {
            wbuf_free(wbuf);
            return -1;
        }

    }
    if (pkt_rx_stats->rs_rssi != ATH_RSSI_BAD) {
        rx_stats->rssi = pkt_rx_stats->rs_rssi;
    }else
        rx_stats->rssi = -1 ;

    return 0;
}

#else
static int 
ath_process_rx_stats(struct ath_softc *sc ,wbuf_t wbuf,
        ieee80211_rx_status_t  *rx_stats,struct ath_rx_status *pkt_rx_stats,u_int64_t rx_tsf)
{

    struct ath_phy_stats *phy_stats = &sc->sc_phy_stats[sc->sc_curmode];
    u_int8_t frame_fc0;
    struct ieee80211_frame *wh;
    u_int phyerr;

    wh = (struct ieee80211_frame *)wbuf_raw_data(wbuf);
    frame_fc0 = wh->i_fc[0];

    if (pkt_rx_stats->rs_status != 0) {
        phy_stats->ast_rx_err++;
        if (pkt_rx_stats->rs_status & HAL_RXERR_CRC) {
            rx_stats->flags |= ATH_RX_FCS_ERROR;
            phy_stats->ast_rx_crcerr++;
        }

        if (pkt_rx_stats->rs_status & HAL_RXERR_FIFO)
            phy_stats->ast_rx_fifoerr++;
        if (pkt_rx_stats->rs_status & HAL_RXERR_PHY) {
            phy_stats->ast_rx_phyerr++;
            phyerr = pkt_rx_stats->rs_phyerr & 0x1f;
            phy_stats->ast_rx_phy[phyerr]++;
#ifdef ATH_SUPPORT_DFS
            {
                u_int64_t tsf = rx_tsf;
                struct ath_buf bf;
                bf.bf_vdata = wbuf_raw_data(wbuf);
                bf.bf_mpdu = wbuf;
                /* Process phyerrs */
                ath_process_phyerr(sc, &bf, pkt_rx_stats, tsf);
            }
#endif
            if (sc->sc_opmode != HAL_M_MONITOR ||
                    ATH_WIRESHARK_FILTER_PHY_ERR(sc) )
            {
                wbuf_free(wbuf);
                return -1;
            }
        }

        if (pkt_rx_stats->rs_status & HAL_RXERR_DECRYPT) {
            /*
             * Decrypt error. We only mark packet status here
             * and always push up the frame up to let NET80211 layer
             * handle the actual error case, be it no decryption key
             * or real decryption error.
             * This let us keep statistics there.
             */
            phy_stats->ast_rx_decrypterr++;
            rx_stats->flags |= ATH_RX_DECRYPT_ERROR;

        } else if (pkt_rx_stats->rs_status & HAL_RXERR_MIC) {
            /*
             * Demic error. We only mark frame status here
             * and always push up the frame up to let NET80211 layer
             * handle the actual error case.
             * This let us keep statistics there and also apply the
             * WAR for bug 6903: (Venice?) Hardware may
             * post a false-positive MIC error.  Need to expose this
             * error to tkip_demic() to really declare a failure.
             */
            if ((frame_fc0 & IEEE80211_FC0_TYPE_MASK) == IEEE80211_FC0_TYPE_CTL) {
                /*
                 * As doc. in hardware bug 30127, sometimes, we get invalid
                 * MIC failures on valid control frames. Remove these mic errors.
                 */
                pkt_rx_stats->rs_status &= ~HAL_RXERR_MIC;
            }
            else {
                rx_stats->flags |= ATH_RX_MIC_ERROR;
            }
        }

        /*
         * Reject error frames with the exception of
         * decryption and MIC failures. For monitor mode, we also
         * ignore the CRC error.
         */
        if (sc->sc_opmode == HAL_M_MONITOR) {
            u_int32_t mask;
            mask = HAL_RXERR_DECRYPT | HAL_RXERR_MIC |
                HAL_RXERR_KEYMISS | HAL_RXERR_CRC;
            if ( ! ATH_WIRESHARK_FILTER_PHY_ERR(sc) ) {
                mask |= HAL_RXERR_PHY;
            }
            if (pkt_rx_stats->rs_status & ~mask) {
                wbuf_free(wbuf);
                return -1;
            }
        } else {
            if (pkt_rx_stats->rs_status &
                    ~(HAL_RXERR_DECRYPT | HAL_RXERR_MIC | HAL_RXERR_KEYMISS)) {
                wbuf_free(wbuf);
                return -1;
            } else {
                if (pkt_rx_stats->rs_status & HAL_RXERR_KEYMISS) {
                    rx_stats->flags |= ATH_RX_KEYMISS;
                }                
            }
        }
    }

    if (sc->sc_hashtsupport) {
        if (pkt_rx_stats->rs_moreaggr == 0) {
            int rssi_chain_valid_count = 0;
            int numchains = sc->sc_rx_numchains;

            if (pkt_rx_stats->rs_rssi != ATH_RSSI_BAD) {
                rx_stats->rssi = pkt_rx_stats->rs_rssi;
                rx_stats->flags |= ATH_RX_RSSI_VALID;
            }
            if (pkt_rx_stats->rs_rssi_ctl0 != ATH_RSSI_BAD) {
                rx_stats->rssictl[0]  = pkt_rx_stats->rs_rssi_ctl0;
                rssi_chain_valid_count++;
            }

            if (pkt_rx_stats->rs_rssi_ctl1 != ATH_RSSI_BAD) {
                rx_stats->rssictl[1]  = pkt_rx_stats->rs_rssi_ctl1;
                rssi_chain_valid_count++;
            }        

            if (pkt_rx_stats->rs_rssi_ctl2 != ATH_RSSI_BAD) {
                rx_stats->rssictl[2]  = pkt_rx_stats->rs_rssi_ctl2;
                rssi_chain_valid_count++;
            }

            if (pkt_rx_stats->rs_flags & HAL_RX_2040) {
                int rssi_extn_valid_count = 0;
                if (pkt_rx_stats->rs_rssi_ext0 != ATH_RSSI_BAD) {
                    rx_stats->rssiextn[0]  = pkt_rx_stats->rs_rssi_ext0;
                    rssi_extn_valid_count++;
                }
                if (pkt_rx_stats->rs_rssi_ext1 != ATH_RSSI_BAD) {
                    rx_stats->rssiextn[1]  = pkt_rx_stats->rs_rssi_ext1;
                    rssi_extn_valid_count++;
                }
                if (pkt_rx_stats->rs_rssi_ext2 != ATH_RSSI_BAD) {
                    rx_stats->rssiextn[2]  = pkt_rx_stats->rs_rssi_ext2;
                    rssi_extn_valid_count++;
                }
                if (rssi_extn_valid_count == numchains) {
                    rx_stats->flags |= ATH_RX_RSSI_EXTN_VALID;
                }                        
            }
            if (rssi_chain_valid_count == numchains) {
                rx_stats->flags |= ATH_RX_CHAIN_RSSI_VALID;
            }                    
        }
    } 
    else {
        /*
         * Need to insert the "combined" rssi into the status structure
         * for upper layer processing
         */

        rx_stats->rssi = pkt_rx_stats->rs_rssi;
        rx_stats->flags |= ATH_RX_RSSI_VALID;
    }
    rx_stats->tsf = pkt_rx_stats->rs_tstamp;

    rx_stats->rateieee = sc->sc_hwmap[pkt_rx_stats->rs_rate].ieeerate;
    rx_stats->rateKbps = sc->sc_hwmap[pkt_rx_stats->rs_rate].rateKbps;
    rx_stats->ratecode = pkt_rx_stats->rs_rate;
    /* HT rate */
    if (rx_stats->ratecode & 0x80) {
        /* TODO - add table to avoid division */
        if (pkt_rx_stats->rs_flags & HAL_RX_2040) {
            rx_stats->flags |= ATH_RX_40MHZ;
            rx_stats->rateKbps = (rx_stats->rateKbps * 27) / 13;
        }
        if (pkt_rx_stats->rs_flags & HAL_RX_GI) {
            rx_stats->flags |= ATH_RX_SHORT_GI;
            rx_stats->rateKbps = (rx_stats->rateKbps * 10) / 9;
        }
    }

    /* sc->sc_noise_floor is only available when the station attaches to an AP,
     * so we use a default value if we are not yet attached.
     */
    /* XXX we should use either sc->sc_noise_floor or
     * ath_hal_getChanNoise(ah, &sc->sc_curchan) 
     * to calculate the noise floor.
     * However, the value returned by ath_hal_getChanNoise seems to be incorrect
     * (-31dBm on the last test), so we will use a hard-coded value until we 
     * figure out what is going on.
     */
    rx_stats->abs_rssi = pkt_rx_stats->rs_rssi + ATH_DEFAULT_NOISE_FLOOR;

    /* increment count of received bytes */
    sc->sc_stats.ast_rx_bytes += wbuf_get_pktlen(wbuf);

    return 0;

}
#endif

#ifdef ATH_HTC_SG_SUPPORT
static __inline void
ath_clear_sg_state(struct ath_softc *sc)
{
    if(sc->sc_sgrx.Firstseg != NULL)
        wbuf_free(sc->sc_sgrx.Firstseg);
    sc->sc_sgrx.Onprocess =0;
    sc->sc_sgrx.pktremaining=0;
    sc->sc_sgrx.nextseq=0;
    sc->sc_sgrx.Firstseg =NULL;


}

static int ath_htc_process_sg(struct ath_softc *sc ,
                                    wbuf_t wbuf ,
                                    struct rx_frame_header * rxhdr,
                                    struct ath_rx_status *pkt_rx_stats,
                                    int *sg_recv, 
                                    u_int64_t rx_tsf)
{

    *sg_recv = 0 ;
    /* Detection of error due to Packet Loss  across HIF layer */
#if 0    
    if(rxhdr->numseg > 1){
        printk(" isamsdu %d rxhdr->numseg %d  rxhdr->sgseqno %d  seq %d leb %d \n",rxhdr->is_seg,rxhdr->numseg,rxhdr->sgseqno,rxhdr->res,wbuf_get_pktlen(wbuf));
        
    }
#endif

    if((rxhdr->is_seg == 0) && (sc->sc_sgrx.Onprocess ==1))
    {
        printk("error \n");
        ath_clear_sg_state(sc);
        return 0;

    }

    if((rxhdr->is_seg!=0) && (rxhdr->numseg >1)){

        if(sc->sc_sgrx.Onprocess ==0){
            if(rxhdr->sgseqno !=0){
                /*qdf_print("1st packet got lost %d \n ",rxhdr->sgseqno);*/
                wbuf_free(wbuf);
                return -1 ;
            }
            ath_clear_sg_state(sc);

        }
        else{
            if(sc->sc_sgrx.nextseq != rxhdr->sgseqno){
                qdf_print(" DROP amsduamsdu required seq %d   got seq %d ",sc->sc_sgrx.nextseq,rxhdr->sgseqno);
                qdf_print("msdu Pkt 1 len  %d total %d  glblseq %d ",rxhdr->sg_length,rxhdr->numseg,rxhdr->res);
                wbuf_free(wbuf);
                ath_clear_sg_state(sc);
                return -1 ;
            }
        }
        /* Process Valid amsdu SubFrames  */
        if(sc->sc_sgrx.Onprocess == 0){

            if(ath_process_rx_stats(sc,wbuf,&sc->sc_sgrx.rx_stats, pkt_rx_stats, rx_tsf) != 0 )
                return -1;

            sc->sc_sgrx.Onprocess =1;       
            sc->sc_sgrx.nextseq =1;
            sc->sc_sgrx.pktremaining=rxhdr->numseg-1;
            sc->sc_sgrx.Firstseg = wbuf;
            return -1;

        }else{

            sc->sc_sgrx.pktremaining--;
            sc->sc_sgrx.nextseq++;
            qdf_nbuf_cat(sc->sc_sgrx.Firstseg, wbuf);                            


            if(sc->sc_sgrx.pktremaining == 0){
                sc->sc_sgrx.Onprocess =0;
                sc->sc_sgrx.pktremaining=0;
                sc->sc_sgrx.nextseq=0;
                *sg_recv =1;
                return 0;
            }else
                return -1 ;
        }
    }
    return 0;
}
#endif

#ifndef ATH_HTC_MII_RXIN_TASKLET
void 
ath_htc_rx(ath_dev_t dev, wbuf_t wbuf)
{
    
    struct ath_softc            *sc = ATH_DEV_TO_SC(dev);

    ATH_HTC_RXBUF_LOCK(sc);
    if(qdf_nbuf_queue_len(&sc->sc_rxwbufhead) >ATH_RXBUF*2){
//        qdf_print("Freeing Rx packet due to Max Rx buf reached  ");

        ATH_HTC_RXBUF_UNLOCK(sc);
        wbuf_free(wbuf);
        return;
    }

    qdf_nbuf_queue_add(&sc->sc_rxwbufhead,wbuf);
    ATH_HTC_RXBUF_UNLOCK(sc);

    ATHUSB_SCHEDULE_TQUEUE(&sc->sc_osdev->rx_tq);

}

#endif




int
ath_htc_rx_tasklet(ath_dev_t dev, int flush)
{
#ifdef ATH_HTC_MII_RXIN_TASKLET
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    if(flush == 0)
        qdf_print("Error : This function is should be used only for flushing  ");
    else{
         /* cleanup Queued buffers in Net layer because of Defer Works 1. Mgmt 2. Nawds*/
         sc->sc_ieee_ops->ath_net80211_rxcleanup(sc->sc_ieee);
    }

    return 0;
}

void 
ath_htc_rx(ath_dev_t dev, wbuf_t wbuf)
{
#else
    wbuf_t wbuf = NULL;
    int rx_processed = 0;
#endif
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    int type;
    ieee80211_rx_status_t ieee_rx_status;
    ieee80211_rx_status_t *rx_status = &ieee_rx_status;
    struct rx_frame_header * rxfrmhdr;
    struct ath_rx_status *pkt_rx_stats;
    u_int64_t rx_tsf = 0;

#ifdef ATH_HTC_SG_SUPPORT        
    int sg_recv=0;
#endif
#ifndef ATH_HTC_MII_RXIN_TASKLET
 if (sc->sc_htcrxpause)
	    return 0;
    
    ATH_HTC_RXBUF_LOCK(sc);
    wbuf = qdf_nbuf_queue_remove(&sc->sc_rxwbufhead);
    ATH_HTC_RXBUF_UNLOCK(sc);

    while(wbuf){
        /* If handling rx interrupt and flush is in progress => exit */
        if (sc->sc_rxflush && (flush == RX_PROCESS)) {
            break;
        }

        ++rx_processed;

        if (flush == RX_DROP) {
            /*
             * If we're asked to flush receive queue, directly
             * chain it back at the queue without processing it.
             */
            printk("%s: drop rx frame when flush rx queue\n", __func__);
            
            wbuf_free(wbuf);
            goto next_skb;
        }
#else
    {
#endif
        pkt_rx_stats = wbuf_raw_data(wbuf);
        
#ifdef ATH_HTC_SG_SUPPORT
        rxfrmhdr     = wbuf_raw_data(wbuf);
        rx_tsf       = rxfrmhdr->tsf;
#endif        


        
        /* Rx header Processing */
        if(ath_htc_process_rxhdr(wbuf,pkt_rx_stats) != 0)
            goto next_skb;


#ifdef ATH_HTC_SG_SUPPORT
        if(ath_htc_process_sg(sc,wbuf,rxfrmhdr,pkt_rx_stats,&sg_recv,rx_tsf) != 0)
            goto next_skb;


       if(sg_recv == 1 ){
            rx_status = &sc->sc_sgrx.rx_stats;

            wbuf = sc->sc_sgrx.Firstseg ;
            sc->sc_sgrx.Firstseg = NULL;
            goto skip_stats;

        }
#endif        
        if(ath_process_rx_stats(sc,wbuf,rx_status, pkt_rx_stats,rx_tsf) != 0 )
            goto next_skb;

#ifdef ATH_HTC_SG_SUPPORT        
skip_stats:
#endif        
        
#ifdef ATH_SUPPORT_UAPSD
        /* Check for valid UAPSD trigger */
        sc->sc_ieee_ops->check_uapsdtrigger(sc->sc_pdev, wbuf_raw_data(wbuf), pkt_rx_stats->rs_keyix, false);
#endif

        /*
         * Pass frames up to the stack.
         * Note: After calling ath_rx_indicate(), we should not assumed that the
         * contents of wbuf, wh, and ds are valid.
         */
        type = ath_rx_indicate(sc, wbuf, rx_status, pkt_rx_stats->rs_keyix);

#ifdef ATH_ADDITIONAL_STATS
        if (type == IEEE80211_FC0_TYPE_DATA) {
            sc->sc_stats.ast_rx_num_data++;
        }
        else if (type == IEEE80211_FC0_TYPE_MGT) {
#ifdef QCA_SUPPORT_CP_STATS
            pdev_cp_stats_rx_num_mgmt_inc(sc->sc_pdev, 1);
#else
            sc->sc_stats.ast_rx_num_mgmt++;
#endif
        }
        else if (type == IEEE80211_FC0_TYPE_CTL) {
#ifdef QCA_SUPPORT_CP_STATS
            pdev_cp_stats_rx_num_ctl_inc(sc->sc_pdev, 1);
#else
            sc->sc_stats.ast_rx_num_ctl++;
#endif
        }
        else {
#ifdef QCA_SUPPORT_CP_STATS
            pdev_lmac_cp_stats_ast_rx_num_unknown_inc(sc->sc_pdev, 1);
#else
            sc->sc_stats.ast_rx_num_unknown++;
#endif
        }
#endif
#ifndef ATH_HTC_MII_RXIN_TASKLET
next_skb :
        if (sc->sc_htcrxpause)
	       break;

        ATH_HTC_RXBUF_LOCK(sc);
        wbuf = qdf_nbuf_queue_remove(&sc->sc_rxwbufhead);
        ATH_HTC_RXBUF_UNLOCK(sc);

#endif
    }

#ifdef ATH_ADDITIONAL_STATS
        if (rx_processed < ATH_RXBUF ) {
                sc->sc_stats.ast_pkts_per_intr[rx_processed]++;
        }
        else {
                sc->sc_stats.ast_pkts_per_intr[ATH_RXBUF]++;
        }
#endif
#ifdef ATH_HTC_MII_RXIN_TASKLET
next_skb :

return ; 
#else
return 0;
#endif
}

#endif
void
ath_htc_tasklet(void* data)
{
    ath_dev_t dev = (ath_dev_t)data;

    ath_htc_rx_tasklet(dev, 0);
}

int
ath_htc_open(ath_dev_t dev, HAL_CHANNEL *initial_chan)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    HAL_STATUS status;
    int error = 0;
    HAL_HT_MACMODE ht_macmode = sc->sc_ieee_ops->cwm_macmode(sc->sc_ieee);
    int cmd_status;

    DPRINTF(sc, ATH_DEBUG_RESET, "%s: mode %d\n", __func__, sc->sc_opmode);

    /*
     * To make sure nobody is using hal while reset HW and SW state.
     */
    ATH_PS_INIT_LOCK(sc);
    ath_pwrsave_init(sc);
    ATH_PS_INIT_UNLOCK(sc);
    
    /*
     * Stop anything previously setup.  This is safe
     * whether this is the first time through or not.
     */
    ath_stop_locked(sc);

#if ATH_CAP_TPC
    ath_hal_setcapability(ah, HAL_CAP_TPC, 0, 1, NULL);
#endif

    /*
     * Flush the skb's allocated for receive in case the rx
     * buffer size changes.  This could be optimized but for
     * now we do it each time under the assumption it does
     * not happen often.
     */
    wmi_cmd(sc->sc_host_wmi_handle, 
            WMI_FLUSH_RECV_CMDID, 
            NULL, 
            0,
            (u_int8_t *) &cmd_status,
            sizeof(cmd_status),
            ATH_HTC_WMI_TIMEOUT_DEFAULT);

    /* Initialize chanmask selection */
    sc->sc_tx_chainmask = sc->sc_config.txchainmask;
    sc->sc_rx_chainmask = sc->sc_config.rxchainmask;
    sc->sc_rx_numchains = sc->sc_mask2chains[sc->sc_rx_chainmask];
    sc->sc_tx_numchains = sc->sc_mask2chains[sc->sc_tx_chainmask];

    /* Start ForcePPM module */
    ath_force_ppm_start(&sc->sc_ppm_info);

    /* Reset SERDES registers */
    ath_hal_configpcipowersave(ah, 0, 0);

    /*
     * The basic interface to setting the hardware in a good
     * state is ``reset''.  On return the hardware is known to
     * be powered up and with interrupts disabled.  This must
     * be followed by initialization of the appropriate bits
     * and then setup of the interrupt mask.
     */
    sc->sc_curchan = *initial_chan;

#if ATH_C3_WAR
    STOP_C3_WAR_TIMER(sc);
#endif

    ATH_RESET_LOCK(sc);
    ath_hal_htc_resetInit(ah);
#ifdef QCA_SUPPORT_CP_STATS
    pdev_lmac_cp_stats_ast_halresets_inc(sc->sc_pdev, 1);
#else
    sc->sc_stats.ast_halresets++;
#endif
    if (!ath_hal_reset(ah, sc->sc_opmode, &sc->sc_curchan, ht_macmode,
                       sc->sc_tx_chainmask, sc->sc_rx_chainmask,
                       sc->sc_ht_extprotspacing, AH_FALSE, &status,
                       sc->sc_scanning))
    {
        printk("%s: unable to reset hardware; hal status %u "
               "(freq %u flags 0x%x)\n", __func__, status,
               sc->sc_curchan.channel, sc->sc_curchan.channel_flags);
        error = -EIO;
        ATH_RESET_UNLOCK(sc);
        goto done;
    }
    ATH_RESET_UNLOCK(sc);

    /*
     * This is needed only to setup initial state
     * but it's best done after a reset.
     */
    ath_update_txpow(sc, sc->tx_power);

    /*
     * Setup the hardware after reset: the key cache
     * is filled as needed and the receive engine is
     * set going.  Frame transmit is handled entirely
     * in the frame output path; there's nothing to do
     * here except setup the interrupt mask.
     */
#if 0
    ath_initkeytable(sc);		/* XXX still needed? */
#endif

#if 0 // K2 Comment for initialization check
    wmi_cmd(sc->host_wmi_handle, 
            WMI_START_RECV_CMDID, 
            NULL, 
            0,
            (u_int8_t *) &cmd_status,
            sizeof(cmd_status),
            ATH_HTC_WMI_TIMEOUT_DEFAULT);

    if (ATH_STARTRECV(sc) != 0) {
        printk("%s: unable to start recv logic\n", __func__);
        error = -EIO;
        goto done;
    }
#endif

    /*
     *  Setup our intr mask.
     */
    sc->sc_imask = HAL_INT_RX | HAL_INT_TX
        | HAL_INT_RXEOL | HAL_INT_RXORN
        | HAL_INT_FATAL | HAL_INT_GLOBAL;

    if (ath_hal_gttsupported(ah))
        sc->sc_imask |= HAL_INT_GTT;

    if (sc->sc_hashtsupport)
        sc->sc_imask |= HAL_INT_CST;

    /*
     * Enable MIB interrupts when there are hardware phy counters.
     * Note we only do this (at the moment) for station mode.
     */
    if ((sc->sc_opmode == HAL_M_STA) ||
        (sc->sc_opmode == HAL_M_IBSS)) {
        if (sc->sc_needmib) {
            sc->sc_imask |= HAL_INT_MIB;
        }
        /*
         * Enable TSFOOR in STA and IBSS modes.
         */
        sc->sc_imask |= HAL_INT_TSFOOR;
    }

    /*
     * If hw beacon processing is supported and enabled, hardware processes
     * the TIM IE and fires an interrupt when the TIM bit is set.
     */
    if (ath_hal_hashwbeaconprocsupport(ah) &&
        sc->sc_opmode == HAL_M_STA &&
        !sc->sc_config.swBeaconProcess) {
        sc->sc_imask |= HAL_INT_TIM;
    }

#ifdef ATH_RFKILL
    if (sc->sc_hasrfkill) {
        if (ath_rfkill_hasint(sc))
            sc->sc_imask |= HAL_INT_GPIO;

        /*
         * WAR for Bug 33276: In certain systems, the RfKill signal is slow to
         * stabilize when waking up from power suspend. The RfKill is an
         * active-low GPIO signal and the problem is the slow rise from 0V to VCC.
         * For this workaround, we will delayed implementing the new RfKill
         * state if there is a change in RfKill during the sleep. This WAR is only
         * when the previous RfKill state is OFF and the new awaken state is ON.
         */
        if (sc->sc_reg_parm.rfKillDelayDetect) {
            ath_rfkill_delay_detect(sc);
        }

        sc->sc_hw_phystate = !ath_get_rfkill(sc); /* read H/W switch state */
        ath_rfkill_start_polling(sc);
    }
#endif
    
    /*
     *  Don't enable interrupts here as we've not yet built our
     *  vap and node data structures, which will be needed as soon
     *  as we start receiving.
     */
  
    ath_chan_change(sc, initial_chan, 0);

    if (!sc->sc_reg_parm.pcieDisableAspmOnRfWake) {
        ath_pcie_pwrsave_enable(sc, 1);
    } else {
        ath_pcie_pwrsave_enable(sc, 0);
    }

    /* XXX: we must make sure h/w is ready and clear invalid flag
     * before turning on interrupt. */
    sc->sc_invalid = 0;
    
    /* Start LED module; pass radio state as parameter */
    ath_led_start_control(&sc->sc_led_control, 
                          sc->sc_hw_phystate && sc->sc_sw_phystate);

    wmi_cmd(sc->sc_host_wmi_handle, 
            WMI_ATH_INIT_CMDID, 
            NULL, 
            0,
            (u_int8_t *) &cmd_status,
            sizeof(cmd_status),
            ATH_HTC_WMI_TIMEOUT_DEFAULT);

    ath_wmi_start_recv(sc);
    OS_DELAY(10000);
    if (ATH_STARTRECV(sc) != 0) {
        printk("%s: unable to restart recv logic\n",
               __func__);
        return -EIO;
    }    
    OS_DELAY(10000);

done:
    return error;
}

void 
ath_wmi_intr_enable(struct ath_softc *sc, u_int32_t mask)
{
    u_int32_t cmd_status;
    u_int32_t imask = cpu_to_be32(mask);

    wmi_cmd(sc->sc_host_wmi_handle, 
            WMI_ENABLE_INTR_CMDID, 
            (u_int8_t *) &imask,
            sizeof(imask),
            (u_int8_t *) &cmd_status,
            sizeof(cmd_status),
            ATH_HTC_WMI_TIMEOUT_DEFAULT);
}

void 
ath_wmi_intr_disable(struct ath_softc *sc, u_int32_t mask)
{
    u_int32_t cmd_status;
    u_int32_t imask = cpu_to_be32(mask);

    wmi_cmd(sc->sc_host_wmi_handle,
            WMI_DISABLE_INTR_CMDID, 
            (u_int8_t *) &imask, 
            sizeof(imask),
            (u_int8_t *) &cmd_status,
            sizeof(cmd_status),
            ATH_HTC_WMI_TIMEOUT_DEFAULT);
}

void 
ath_wmi_abort_txq(struct ath_softc *sc, HAL_BOOL fastcc)
{
    u_int32_t cmd_status;
    u_int16_t fastcc_ie = fastcc; /* store in 16 bits */

    wmi_cmd(sc->sc_host_wmi_handle, 
            WMI_ABORT_TXQ_CMDID, 
            (u_int8_t *) &fastcc_ie, 
            sizeof(u_int16_t),
            (u_int8_t *) &cmd_status,
            sizeof(cmd_status),
            ATH_HTC_WMI_TIMEOUT_DEFAULT);
}

void 
ath_wmi_rx_link(struct ath_softc *sc)
{
    u_int32_t cmd_status;

    wmi_cmd(sc->sc_host_wmi_handle,
            WMI_RX_LINK_CMDID, 
            NULL, 
            0,
            (u_int8_t *) &cmd_status,
            sizeof(cmd_status),
            ATH_HTC_WMI_TIMEOUT_DEFAULT);
}

void 
ath_wmi_drain_txq(struct ath_softc *sc)
{
    u_int32_t cmd_status;

    wmi_cmd(sc->sc_host_wmi_handle, 
            WMI_DRAIN_TXQ_ALL_CMDID, 
            NULL,
            0,
            (u_int8_t *) &cmd_status,
            sizeof(cmd_status),
            ATH_HTC_WMI_TIMEOUT_DEFAULT);
}

void 
ath_wmi_stop_recv(struct ath_softc *sc)
{
    u_int32_t cmd_status;

    wmi_cmd(sc->sc_host_wmi_handle, 
           WMI_STOP_RECV_CMDID, 
           NULL,
           0,
           (u_int8_t *) &cmd_status,
           sizeof(cmd_status),
           ATH_HTC_WMI_TIMEOUT_DEFAULT);
}

void 
ath_wmi_start_recv(struct ath_softc *sc)
{
    u_int32_t cmd_status;

    wmi_cmd(sc->sc_host_wmi_handle,
           WMI_START_RECV_CMDID, 
           NULL, 
           0,
           (u_int8_t *) &cmd_status,
           sizeof(cmd_status),
           ATH_HTC_WMI_TIMEOUT_DEFAULT);
}

void 
ath_wmi_set_mode(struct ath_softc *sc, u_int16_t phymode)
{
    u_int32_t cmd_status;

    wmi_cmd(sc->sc_host_wmi_handle, 
            WMI_SET_MODE_CMDID, 
            (u_int8_t *) &phymode, 
            sizeof(u_int16_t),
            (u_int8_t *) &cmd_status,
            sizeof(cmd_status),
            ATH_HTC_WMI_TIMEOUT_DEFAULT);
}

int ath_htc_initialize_target_q(struct ath_softc *sc)
{
    int error = 0, i;

    for (i = 0; i < NUM_TX_EP; i++)
        sc->sc_ep2qnum[i] = -1;


    return error;
}

static int
ath_htc_dataqueue_to_epid(struct ath_softc *sc, int qnum, int *epid)
{
#define	N(a)	(sizeof(a)/sizeof(a[0]))
	int i;


	for (i = 0; i < N(sc->sc_haltype2q); i++) {
		if (qnum == sc->sc_haltype2q[i]) {
			switch (i)
			{
			case HAL_WME_AC_BK:
				(*epid) = sc->sc_data_BK_ep;
				break;
			case HAL_WME_AC_BE:
				(*epid) = sc->sc_data_BE_ep;
				break;
			case HAL_WME_AC_VI:
				(*epid) = sc->sc_data_VI_ep;
				break;
			case HAL_WME_AC_VO:
				(*epid) = sc->sc_data_VO_ep;
				break;
			default:
				return 0;
			}
			
			return 1;
		}
	}

	return 0;
#undef N
}

void 
ath_htc_txq_update(ath_dev_t dev, int qnum, HAL_TXQ_INFO *qi)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
	int epid;
	
	DPRINTF(sc, ATH_DEBUG_HTC_WMI, "%s: qnum = %d, aifs = %d, cwmin = %d, cwmax = %d, burstTime = %d, readyTime = %d\n",
		   __func__, qnum, qi->tqi_aifs, qi->tqi_cwmin, qi->tqi_cwmax, qi->tqi_burst_time, qi->tqi_ready_time);
	
	if (1 == ath_htc_dataqueue_to_epid(sc, qnum, &epid)) {
		HTCTxEpUpdate(sc->sc_host_htc_handle, 
                      epid,
                      qi->tqi_aifs,
                      qi->tqi_cwmin,
                      qi->tqi_cwmax);
	}
}


#ifdef ATH_SUPPORT_UAPSD
void
ath_htc_tx_queue_uapsd(struct ath_softc *sc, wbuf_t wbuf, ieee80211_tx_control_t *txctl)
{
    struct ath_buf *bf;
    struct ath_node *an = txctl->an;

    ATH_TXBUF_LOCK(sc);
    bf = TAILQ_FIRST(&sc->sc_txbuf);

    if (bf != NULL) {
        TAILQ_REMOVE(&sc->sc_txbuf, bf, bf_list);
    } else {
        ATH_TXBUF_UNLOCK(sc);
        DPRINTF(sc, ATH_DEBUG_UAPSD, "%s: can't get tx bf\n", __func__);
        wbuf_free(wbuf);
        return;
    }
    ATH_TXBUF_UNLOCK(sc);

    bf->bf_mpdu = wbuf;

    /*
     * Lock out interrupts since this queue shall be accessed
     * in interrupt context.
     */
    ATH_HTC_UAPSD_LOCK(sc);

    TAILQ_INSERT_TAIL(&an->an_uapsd_q, bf, bf_list);
    an->an_uapsd_qdepth++;

    DPRINTF(sc, ATH_DEBUG_UAPSD, "%s: depth %d \n",__func__,an->an_uapsd_qdepth);

    sc->sc_stats.ast_uapsddataqueued++;

    ATH_HTC_UAPSD_UNLOCK(sc);
}

static void
ath_htc_uapsd_sendqosnull(struct ath_softc *sc, struct ath_node *an, u_int8_t ac)
{
    wbuf_t wbuf;
    ath_data_hdr_t *tfh;

    u_int8_t nodeindex = an->an_sc->sc_ieee_ops->ath_htc_gettargetnodeid(an->an_sc->sc_ieee,
                                                                       an->an_node);
    u_int8_t vapindex = an->an_sc->sc_ieee_ops->ath_htc_gettargetvapid(an->an_sc->sc_ieee,
                                                                       an->an_node);

    wbuf = wbuf_alloc(sc->sc_osdev, WBUF_USB_TX, (ATH_HTC_HDRSPACE+sizeof(ath_data_hdr_t)+ATH_HTC_QOSNULL_LEN));

    if (wbuf == NULL) {
        sc->sc_stats.ast_uapsdqnulbf_unavail++;
        DPRINTF(sc, ATH_DEBUG_UAPSD, "%s: qosnul buffers unavailable \n",__func__);
        printk("%s: qosnul buffers unavailable \n",__func__);
        return;
    }

    //sc->sc_uapsdqnuldepth++;

    /*
     * Format a QoS NULL frame for this node and ac
     */
    wbuf = sc->sc_ieee_ops->uapsd_getqosnullframe(an->an_node, wbuf, ac);
    if (!wbuf)
        return;

    wbuf_set_pktlen(wbuf, ATH_HTC_QOSNULL_LEN);

    tfh = (ath_data_hdr_t *)wbuf_push(wbuf, sizeof(ath_data_hdr_t));

    OS_MEMZERO(tfh, sizeof(ath_data_hdr_t));

    tfh->ni_index = nodeindex;
    tfh->vap_index = vapindex;
    tfh->datatype = 0;
    tfh->tidno = wbuf_get_tid(wbuf);
#ifdef MAGPIE_HIF_GMAC    
    tfh->flags |= ATH_DH_FLAGS_QOSNULL;
#else
    tfh->flags = 0;
#endif    
    sc->sc_ieee_ops->uapsd_retqosnullframe(an->an_node, wbuf);

#ifdef ATH_SUPPORT_P2P
    /* Use Min. rate to send to increase receiving rate for WFD 6.1.12. */
    tfh->flags |= TFH_FLAGS_USE_MIN_RATE;
    tfh->flags = cpu_to_be32(tfh->flags);
#endif
    
    /* Check for headroom size */
    if (wbuf_hdrspace(wbuf) < ATH_HTC_HDRSPACE)
    {
        wbuf = wbuf_realloc_hdrspace(wbuf, ATH_HTC_HDRSPACE);

        if (wbuf == NULL) {
            DPRINTF(sc, ATH_DEBUG_HTC_WMI,"%s: drop tx frame - insufficient header room space I\n", __func__);
            return;
        }
    }
    

    HTCSendPkt(sc->sc_host_htc_handle, NULL, wbuf, sc->sc_uapsd_ep);

    sc->sc_stats.ast_uapsdqnul_pkts++;
}

int
ath_htc_process_uapsd_trigger(ath_dev_t dev, ath_node_t node, u_int8_t maxsp, u_int8_t ac, u_int8_t flush, bool *sent_eosp, u_int8_t maxqdepth)
{
    struct ath_node *an = ATH_NODE(node);
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_buf *bf, *last_bf = NULL;
    int count = 0;
    int i;
    int max_count = maxsp;
#ifdef MAGPIE_HIF_GMAC
    struct ieee80211_node *ni = (struct ieee80211_node *)an->an_node;
    ath_data_hdr_t *tfh;
#else
    struct ieee80211_qosframe *whqos;
#endif    
    if (sent_eosp) {
        ; /* nothing doing here just to avoid compiler warning */
    }
    sc->sc_stats.ast_uapsdtriggers++;
#ifdef MAGPIE_HIF_GMAC
    if (flush) {
        ieee80211node_clear_flag(ni, IEEE80211_NODE_UAPSD_CREDIT_UPDATE);
    }         
#endif
    if (!(an->an_flags & ATH_NODE_UAPSD)) {
        sc->sc_stats.ast_uapsdnodeinvalid++;
        return 0;
    }

    ATH_HTC_UAPSD_LOCK(sc);
    if (sc->sc_uapsd_pause_in_progress) {
        ATH_HTC_UAPSD_UNLOCK(sc);
        return 0;
    }
    sc->sc_uapsd_trigger_in_progress = true;

    /*
     * UAPSD queue is empty. Send QoS NULL if this is
     * not a flush operation.
     */
    if (TAILQ_EMPTY(&an->an_uapsd_q)) {
        if (!flush){
            ath_htc_uapsd_sendqosnull(sc, an, ac);
            sc->sc_ieee_ops->uapsd_pause_control(an->an_node, true);
        }
        sc->sc_uapsd_trigger_in_progress = false;
        ATH_HTC_UAPSD_UNLOCK(sc);
        return 0;
    }

    /*
     * Send all frames
     */

    if (max_count == WME_UAPSD_NODE_MAXQDEPTH)
        max_count = an->an_uapsd_qdepth;
#ifndef MAGPIE_HIF_GMAC    
    else if (max_count > an->an_uapsd_qdepth)  /* If STA's maxSP(max_count) is not zero. */
        max_count = an->an_uapsd_qdepth;
#endif

    TAILQ_FOREACH(bf, &an->an_uapsd_q, bf_list) {
        if (count < max_count){
            count++;
            last_bf = bf;
        }
        else
            break;
    }

    an->an_uapsd_qdepth -= count;
    ASSERT(an->an_uapsd_qdepth >= 0);

#ifdef MAGPIE_HIF_GMAC
    tfh = (ath_data_hdr_t *)(wbuf_header(last_bf->bf_mpdu));
    if (!flush) {
        tfh->flags |= ATH_DH_FLAGS_EOSP;
        if (an->an_uapsd_qdepth == 0)
            tfh->flags &= ~ATH_DH_FLAGS_MOREDATA;
    }   
    
    sc->sc_stats.ast_uapsddata_pkts += count;
#else    
    /*
     * Mark EOSP flag at the last frame in this SP
     */
    whqos = (struct ieee80211_qosframe *)(wbuf_header(last_bf->bf_mpdu) + sizeof(ath_data_hdr_t));
    if (!flush) {
        whqos->i_qos[0] |= IEEE80211_QOS_EOSP;
        sc->sc_stats.ast_uapsdeospdata++;
    }

    sc->sc_stats.ast_uapsddata_pkts += count;

    /*
     * Clear More data bit for EOSP frame if we have
     * no pending frames
     */
    if (an->an_uapsd_qdepth == 0) {
        whqos->i_fc[1] &= ~IEEE80211_FC1_MORE_DATA;
    }

    /* Any changes to the QOS Data Header must be reflected to the physical buffer */
    wbuf_uapsd_update(last_bf->bf_mpdu);
#endif

    for (i = 0; i < count; i++) {
        wbuf_t wbuf;

        bf = TAILQ_FIRST(&an->an_uapsd_q);

        wbuf = bf->bf_mpdu;
#ifdef MAGPIE_HIF_GMAC
        if(ath_htc_send_pkt(dev, wbuf, sc->sc_uapsd_ep, an) != 0) {
            an->an_uapsd_qdepth += (count - i);
            printk(" %s unable to send UAPSD pkts,SW queue depth %d\n",__FUNCTION__,an->an_uapsd_qdepth);
            ieee80211node_set_flag(ni, IEEE80211_NODE_UAPSD_CREDIT_UPDATE);
            goto uapsd_q;
        }
#else        
        HTCSendPkt(sc->sc_host_htc_handle, NULL, wbuf, sc->sc_uapsd_ep);
#endif        
        TAILQ_REMOVE(&an->an_uapsd_q, bf, bf_list);

        /* reclaim ath_buf */
        bf->bf_mpdu = NULL;
        ATH_TXBUF_LOCK(sc); 
        TAILQ_INSERT_TAIL(&sc->sc_txbuf, bf, bf_list);
        ATH_TXBUF_UNLOCK(sc);
    }

    if (!flush)
        sc->sc_ieee_ops->uapsd_pause_control(an->an_node, true);
        
    sc->sc_uapsd_trigger_in_progress = false;
#ifdef MAGPIE_HIF_GMAC
uapsd_q:    
#endif    
    ATH_HTC_UAPSD_UNLOCK(sc);
    DPRINTF(sc, ATH_DEBUG_UAPSD, "%s: HW queued %d SW queu depth %d \n",__func__,count,an->an_uapsd_qdepth);
    return (an->an_uapsd_qdepth);
}

void
ath_htc_tx_uapsd_node_cleanup(struct ath_softc *sc, struct ath_node *an)
{
    struct ath_buf *bf;
    ATH_HTC_UAPSD_LOCK(sc);
    /*
     * Drain the uapsd software queue.
     */
    for (;;) {
        wbuf_t wbuf;

        bf = TAILQ_FIRST(&an->an_uapsd_q);

        if (bf == NULL)
            break;

        wbuf = bf->bf_mpdu;
        HTCTxComp(sc->sc_host_htc_handle, wbuf, sc->sc_uapsd_ep);
        TAILQ_REMOVE(&an->an_uapsd_q, bf, bf_list);

        /* reclaim ath_buf */
        bf->bf_mpdu = NULL;
        ATH_TXBUF_LOCK(sc); 
        TAILQ_INSERT_TAIL(&sc->sc_txbuf, bf, bf_list);
        ATH_TXBUF_UNLOCK(sc);
    }
    an->an_uapsd_qdepth = 0; 
    ATH_HTC_UAPSD_UNLOCK(sc);
}
#endif

#ifndef MAGPIE_HIF_GMAC
int
ath_htc_tx_start(ath_dev_t dev, wbuf_t wbuf, ieee80211_tx_control_t *txctl)
{
#define ATH_DATA_TYPE_AGGR              0x01
#define ATH_DATA_TYPE_NON_AGGR          0x02
#define ATH_HTC_WMM_THRESHOLD           100
#define ATH_ETHERTYPE_IP                0x0800
#define ATH_80211_NONE_SNAP__LENGTH     30 /* 24 + 0 + 6 */
#define ATH_80211_WEP_SNAP_LENGTH       34 /* 24 + 4 + 6 */
#define ATH_80211_TKIP_SNAP_LENGTH      38 /* 24 + 8 + 6 */
#define ATH_80211_AES_SNAP_LENGTH       38 /* 24 + 8 + 6 */
#define ATH_80211_QOSPAD_LENGTH         4  /* 2 + 2 */

    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    int htcstatus = 0;

#ifdef ENCAP_OFFLOAD
    int usecrypto = 0;
#endif

    if (txctl->isdata)
    {
        ath_data_hdr_t *tfh;
        struct ath_node *an = ATH_NODE(txctl->an);
        ath_atx_tid_t *tid = ATH_AN_2_TID(an, txctl->tidno);
        HTC_ENDPOINT_ID     epid;
        u_int16_t           dropIndex = 0;
        u_int8_t            ipVersion = 0;
        u_int16_t           ipv4FlagAndFragOffset = 0;
        u_int32_t           flags;
        struct ath_vap      *avp = sc->sc_vaps[txctl->if_id];

#ifdef ATH_SUPPORT_DFS
        /*
         * If we detect radar on the current channel, stop sending data
         * packets. There is a DFS requirment that the AP should stop
         * sending data packet within 200 ms of radar detection
         */
        if (sc->sc_curchan.priv_flags & CHANNEL_INTERFERENCE)
            return -EIO;
#endif

        if (txctl->istxfrag == 0) {
            u_int8_t *rawdata = wbuf_header(wbuf);
            u_int8_t ethertypeOffset = 0;

            switch (txctl->keytype) {
            case HAL_KEY_TYPE_WEP:
                ethertypeOffset = (txctl->isqosdata)? (ATH_80211_WEP_SNAP_LENGTH + ATH_80211_QOSPAD_LENGTH): ATH_80211_WEP_SNAP_LENGTH;
                break;
            case HAL_KEY_TYPE_TKIP:
                ethertypeOffset = (txctl->isqosdata)? (ATH_80211_TKIP_SNAP_LENGTH + ATH_80211_QOSPAD_LENGTH): ATH_80211_TKIP_SNAP_LENGTH;
                break;
            case HAL_KEY_TYPE_AES:
                ethertypeOffset = (txctl->isqosdata)? (ATH_80211_AES_SNAP_LENGTH + ATH_80211_QOSPAD_LENGTH): ATH_80211_AES_SNAP_LENGTH;
                break;
            case HAL_KEY_TYPE_CLEAR:
            default:
                ethertypeOffset = (txctl->isqosdata)? (ATH_80211_NONE_SNAP__LENGTH + ATH_80211_QOSPAD_LENGTH): ATH_80211_NONE_SNAP__LENGTH;
                break;
            }

            if (txctl->frmlen >= (ethertypeOffset + 22)) { /* 22: ethertype + ipheader */
                if (be16_to_cpu(*((u_int16_t*)(rawdata + ethertypeOffset))) == ATH_ETHERTYPE_IP) {
                    ipVersion = (rawdata[ethertypeOffset + 2] >> 4);
                    if (0x4 == ipVersion) { /* check if the ipv4 frame */
                        ipv4FlagAndFragOffset = be16_to_cpu(*((u_int16_t*)(rawdata + ethertypeOffset + 8)));
                    }
                }
            }
            
            //printk("%s: ipVersion = 0x%x, ipv4FlagAndFragOffset = 0x%x, ethertypeOffset = %d\n", __func__, ipVersion, ipv4FlagAndFragOffset, ethertypeOffset);
        }

        switch (txctl->qnum) {
        case 0:
            epid = sc->sc_data_BE_ep;
            dropIndex = WME_AC_BE;
            break;
        case 2:
            epid = sc->sc_data_VI_ep;
            dropIndex = WME_AC_VI;
            break;
        case 3:
            epid = sc->sc_data_VO_ep;
            dropIndex = WME_AC_VO;
            break;
        case 1:
        default:
            epid = sc->sc_data_BK_ep;
            dropIndex = WME_AC_BK;
            break;
        }

        tfh = (ath_data_hdr_t *)wbuf_push(wbuf, sizeof(ath_data_hdr_t));

        OS_MEMZERO(tfh, sizeof(ath_data_hdr_t));

        tfh->ni_index = txctl->nodeindex;
        tfh->vap_index = txctl->vapindex;

        if ((tfh->vap_index >= ATH_HTC_MAX_VAP_NUM) || (tfh->ni_index >= ATH_HTC_MAX_NODE_NUM)) {
            HTCTxComp(sc->sc_host_htc_handle, wbuf, epid);
            wbuf_free(wbuf);
            KASSERT(0, ("%s: VAP/Node index exceeds the HTC capability, vap/node index = %d/%d !\n", __func__, tfh->vap_index, tfh->ni_index));
            return 0;
        }

        if (tid->addba_exchangecomplete)
        {
            tfh->datatype = ATH_DATA_TYPE_AGGR;
        }
        else
        {
            tfh->datatype = ATH_DATA_TYPE_NON_AGGR;
        }

        tfh->tidno = wbuf_get_tid(wbuf);
        if (tfh->tidno >= ATH_HTC_WME_NUM_TID) {
            HTCTxComp(sc->sc_host_htc_handle, wbuf, epid);
            wbuf_free(wbuf);
            KASSERT(0, ("%s: tidno exceeds the HTC capability, tidno = %d !\n", __func__, tfh->tidno));
            return 0;
        }

        switch (sc->sc_protmode) {
        case PROT_M_RTSCTS:
            flags = IEEE80211_PROT_RTSCTS;
            break;
        case PROT_M_CTSONLY:
            flags = IEEE80211_PROT_CTSONLY;
            break;
        case PROT_M_NONE:
        default:
            flags = IEEE80211_PROT_NONE;
            break;
        }
        
        if (an->an_smmode == ATH_SM_PWRSAV_DYNAMIC) {
            flags = IEEE80211_PROT_RTSCTS;
        }

        if (txctl->flags & HAL_TXDESC_RTSENA)
            flags = IEEE80211_PROT_RTSCTS;

        if (txctl->txpower == ATH_TX_POWER_SRM)
            flags |= ATH_TX_SRM;

        /* To force target send using minimum rate */
        if (txctl->use_minrate) {
            flags |= TFH_FLAGS_USE_MIN_RATE;
        }

#ifdef ENCAP_OFFLOAD        
#ifdef ATH_SUPPORT_UAPSD
        if (!txctl->isuapsd) {
#endif
        if(txctl->shortPreamble)
            flags |= ATH_SHORT_PREAMBLE;        

        if(wbuf_is_moredata(wbuf))
            flags |= ATH_DH_FLAGS_MOREDATA;

        /*
         * Assert our Exemption policy.  We assert it blindly at first, then
         * take the presence/absence of a key into acct.
         *
         * Lookup the ExemptionActionType in the send context info of this frame
         * to determine if we need to encrypt the frame.
         */
        switch (wbuf_get_exemption_type(wbuf)) {
            case WBUF_EXEMPT_NO_EXEMPTION:
                /*
                * We want to encrypt this frame.
                 */
                usecrypto = 1;
                break;

            case WBUF_EXEMPT_ALWAYS:
                /*
                * We don't want to encrypt this frame.
                 */
                usecrypto = 0;
                break;

            case WBUF_EXEMPT_ON_KEY_MAPPING_KEY_UNAVAILABLE:
                /*
                * We encrypt this frame if and only if a key mapping key is set.
                 */
                usecrypto = (txctl->key_mapping_key) ? 1 : 0;
                
                break;

            default:
                ASSERT(0);
                usecrypto = 1;
                break;
        }

        /*
         * If the frame is to be encrypted, but no key is not set, either reject the frame 
         * or clear the WEP bit.
         */
        if (usecrypto && (txctl->keytype == HAL_KEY_TYPE_CLEAR)) {
            /*
             * If this is a unicast frame or if the BSSPrivacy is on, reject the frame.
             * Otherwise, clear the WEP bit so we will not encrypt the frame. In other words,
             * we'll send multicast frame in clear if multicast key hasn't been setup.
             */
            if (!txctl->ismcast) {
                HTCTxComp(sc->sc_host_htc_handle, wbuf, epid);
                wbuf_free(wbuf);
//#ifndef GMAC_JUMBO_FRAME_SUPPORT          
//                if(wbuf1 != NULL)
//                    wbuf_free(wbuf1);
//#endif                
                printk("Rejecting unicast encrypted frame as key is not set\n");
                return 0;
            }
            printk("clearing privacy bit for mcast packet\n");
            usecrypto = 0; /* XXX: is this right??? */
        }

        if (usecrypto)
             flags |= ATH_DH_FLAGS_PRIVACY;
             
        tfh->keyid = txctl->keyid;
             
#ifdef ATH_SUPPORT_UAPSD
        }
#endif
#endif

        tfh->flags = cpu_to_be32(flags);

        // for encryption
        if( (txctl->keytype == HAL_KEY_TYPE_TKIP) && (txctl->istxfrag))
        {
            tfh->keytype = HAL_KEY_TYPE_WEP;	 
        } else {      
            tfh->keytype = txctl->keytype;	
        }
        tfh->keyix = txctl->keyix;

        /* Check for headroom size */
        if (wbuf_hdrspace(wbuf) < ATH_HTC_HDRSPACE)
        {
            wbuf = wbuf_realloc_hdrspace(wbuf, ATH_HTC_HDRSPACE);

            if (wbuf == NULL) {
                DPRINTF(sc, ATH_DEBUG_HTC_WMI,"%s: drop tx frame - insufficient header room space I\n", __func__);
                return 0;
            }
        }

        HTCTxComp(sc->sc_host_htc_handle, wbuf, epid);

#ifdef ATH_SUPPORT_UAPSD
        if (txctl->isuapsd) {
           ath_htc_tx_queue_uapsd(sc, wbuf, txctl);
//            atomic_dec(&an->an_active_tx_cnt);
            return 0;
        }
#endif

        /* CAB QUEUE : re-assign service */
        if (txctl->ps && txctl->ismcast) {
            epid = sc->sc_cab_ep;
            htcstatus = HTCSendPkt(sc->sc_host_htc_handle, NULL, wbuf, epid);
            if(htcstatus != A_OK)
                wbuf_free(wbuf);

            /*
             * avp->av_mcastq.axq_depth work with ath_start_tx_dma(), 
             * but this job switch to ath_htc_tx_start() if HTC support.
             * HTC define it to be a new simple counter. 
             */            
            avp->av_mcastq.axq_depth++;
            return 0;
        }

        if ((ipv4FlagAndFragOffset & 0x3fff) == 0x0) {
            /* case 1: non-ipv4 frame or non-fragment ipv4 frame */
            sc->sc_qosDropIpFrag[dropIndex] = 0;
        } else if ((ipv4FlagAndFragOffset & 0x3fff) == 0x2000) {
            /* case 2: ipv4 fragment frame with zero offset */
            if (HTC_busy(sc->sc_host_htc_handle, epid, ATH_HTC_WMM_THRESHOLD) == TRUE) {
                sc->sc_qosDropIpFrag[dropIndex] = 1;
                wbuf_free(wbuf);
                wbuf = NULL;

                sc->sc_qosDropIpFragNum[dropIndex]++;
                DPRINTF(sc, ATH_DEBUG_HTC_WMI,"%s: drop tx fame = %dI\n", __func__, sc->sc_qosDropIpFragNum[dropIndex]);
            } else {
                sc->sc_qosDropIpFrag[dropIndex] = 0;
            } 
        } else if ((sc->sc_qosDropIpFrag[dropIndex] == 1) 
                && ((ipv4FlagAndFragOffset & 0x1fff) != 0x0)) {
            /* case 3: frame belong to the droped fragment group */
            wbuf_free(wbuf);
            wbuf = NULL;

            sc->sc_qosDropIpFragNum[dropIndex]++;
            DPRINTF(sc, ATH_DEBUG_HTC_WMI,"%s: drop tx fame = %dI\n", __func__, sc->sc_qosDropIpFragNum[dropIndex]);
        }
#ifdef  ATH_HTC_TX_SCHED
        if ((sc->sc_qosDropIpFrag[dropIndex] == 0)
                || (wbuf != NULL)) {

            if(!tid->paused ){
                if( ath_htc_can_xmit(sc, wbuf_get_priority(wbuf)) == 0 )
                    goto queue;
                htcstatus = HTCSendPkt(sc->sc_host_htc_handle, NULL, wbuf, epid);
            }

            if (qdf_unlikely((htcstatus != A_OK) || (htcstatus == A_CREDIT_UNAVAILABLE)) || tid->paused){

queue:
                ath_htc_txep_queue(sc,wbuf,an,tfh->tidno,epid);
            }
            /*bharath to do --Queue*/
        }
#else
        if ((sc->sc_qosDropIpFrag[dropIndex] == 0)
                || (wbuf != NULL)) {
            HTCSendPkt(sc->sc_host_htc_handle, NULL, wbuf, epid);
        }
 
#endif
    }
    else
    {
        ath_mgt_hdr_t txmgt;
        u_int8_t *anbdata;

        HTCTxComp(sc->sc_host_htc_handle, wbuf, sc->sc_mgmt_ep);
        OS_MEMZERO(&txmgt, sizeof(ath_mgt_hdr_t));

        txmgt.ni_index = txctl->nodeindex;
        txmgt.vap_index = txctl->vapindex;
        if ((txmgt.vap_index >= ATH_HTC_MAX_VAP_NUM) || (txmgt.ni_index >= ATH_HTC_MAX_NODE_NUM)) {
            wbuf_free(wbuf);
            KASSERT(0, ("%s: VAP/Node index exceeds the HTC capability, vap/node index = %d/%d !\n", __func__, txmgt.vap_index, txmgt.ni_index));
            return 0;
        }

        txmgt.tidno = 0;
        txmgt.flags = 0;

        // for encryption
        txmgt.keytype = txctl->keytype;	
        txmgt.keyix = txctl->keyix;

        /* Check for headroom size */
        if (wbuf_hdrspace(wbuf) < ATH_HTC_HDRSPACE + sizeof(ath_mgt_hdr_t))
        {
            wbuf = wbuf_realloc_hdrspace(wbuf, ATH_HTC_HDRSPACE + sizeof(ath_mgt_hdr_t));

            if (wbuf == NULL) {
                DPRINTF(sc, ATH_DEBUG_HTC_WMI,"%s: drop tx frame - insufficient header room space I\n", __func__);
                return 0;
            }
        }

        anbdata = wbuf_push(wbuf, sizeof(ath_mgt_hdr_t));
        OS_MEMCPY((u_int8_t *)anbdata, (u_int8_t *)&txmgt, sizeof(ath_mgt_hdr_t));

        htcstatus = HTCSendPkt(sc->sc_host_htc_handle, NULL, wbuf, sc->sc_mgmt_ep);
#ifdef  ATH_HTC_TX_SCHED
        if (qdf_unlikely((htcstatus != A_OK) )){
            printk("Freeing Management pkt due to credit unavailibility \n");
            wbuf_free(wbuf);
            wbuf = NULL;
        }
#endif

    }

    return 0;

#undef ATH_DATA_TYPE_AGGR
#undef ATH_DATA_TYPE_NON_AGGR
#undef ATH_HTC_WMM_THRESHOLD
#undef ATH_ETHERTYPE_IP
#undef ATH_80211_NONE_SNAP__LENGTH
#undef ATH_80211_WEP_SNAP_LENGTH
#undef ATH_80211_TKIP_SNAP_LENGTH
#undef ATH_80211_AES_SNAP_LENGTH
#undef ATH_80211_QOSPAD_LENGTH
}

#else
int 
ath_htc_send_pkt(ath_dev_t dev, wbuf_t wbuf, HTC_ENDPOINT_ID epid, struct ath_node *an) 
{
#define ATH_GMAC_FRAG_THRESH            1400
#define ATH_GMAC_FRAG_BUF_SIZE          512
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    ath_data_hdr_t *tfh;
    u_int8_t *anbdata;
    ath_atx_tid_t *tid = NULL;
    int htcstatus      = 0;
#ifndef GMAC_JUMBO_FRAME_SUPPORT        
    struct ath_txep *txep;
    int htcstatus1        = 0;
    ath_data_hdr_t  *tfh1 = NULL;
    u_int32_t       frag_len;
    wbuf_t          wbuf1;
    int credit = 0;
#endif    
    
    anbdata = wbuf_header(wbuf);
    tfh = (ath_data_hdr_t *)anbdata;
    tid = ATH_AN_2_TID(an, tfh->tidno);
    if (tid == NULL) {
        printk("wrong tid, tidno %d\n",tfh->tidno);
        return -1;
    }

#ifndef GMAC_JUMBO_FRAME_SUPPORT        

    if (wbuf_get_pktlen(wbuf) > ATH_GMAC_FRAG_THRESH) {
        ATH_TXBUF_LOCK(sc);

        /*
        * there are 2 fragments. Ensure buffers
         */
        if (epid == sc->sc_uapsd_ep)
            txep = &sc->sc_txep[sc->sc_data_BK_ep];
        else
            txep = &sc->sc_txep[epid];

        if (txep->ep_depth > (txep->ep_max - 2)) {
            if (epid != sc->sc_uapsd_ep) 
                wbuf_free(wbuf);

            ATH_TXBUF_UNLOCK(sc);
            return -1;
        }
        if (epid == sc->sc_uapsd_ep) {
            credit = htc_get_creditinfo(sc->sc_host_htc_handle, epid);
            if (credit < 2) {
                ATH_TXBUF_UNLOCK(sc);
                return -1;
            }    
        }
        ATH_TXBUF_UNLOCK(sc);

        wbuf1 = wbuf_alloc(sc->sc_osdev,WBUF_TX_DATA, ATH_GMAC_FRAG_BUF_SIZE);
    }
    else
        wbuf1 = NULL;

    if (wbuf1) {
        anbdata = wbuf_header(wbuf);
        frag_len = wbuf_get_pktlen(wbuf) - ATH_GMAC_FRAG_THRESH;
        tfh = (ath_data_hdr_t *)anbdata;

        qdf_mem_copy(qdf_nbuf_put_tail(wbuf1, frag_len), anbdata + 1400,
                frag_len);

        wbuf_trim(wbuf, frag_len);

        tfh->pkt_type = ATH_DATA_HDR_FRAG_FIRST;    

        tfh1 =(ath_data_hdr_t *)wbuf_push(wbuf1, sizeof(struct __data_header));
        OS_MEMCPY((u_int8_t *)tfh1, (u_int8_t *)tfh, sizeof(struct __data_header));
        tfh1->pkt_type = ATH_DATA_HDR_FRAG_LAST;
    }

#endif   

#ifdef  ATH_HTC_TX_SCHED
    if(wbuf != NULL) {

        if(!tid->paused ){
             if(epid == sc->sc_uapsd_ep)
                goto send;
            if( ath_htc_can_xmit(sc, wbuf_get_priority(wbuf)) == 0 )
                goto queue;
send:        
            htcstatus = HTCSendPkt(sc->sc_host_htc_handle, NULL, wbuf, epid);
        }

        if (qdf_unlikely((htcstatus != A_OK) || (htcstatus == A_CREDIT_UNAVAILABLE)) || tid->paused){
            if(epid == sc->sc_uapsd_ep)
               return -1;
queue:
            ath_htc_txep_queue(sc, wbuf, an, tfh->tidno, epid);
        }
        /*bharath to do --Queue*/
    }

#ifndef GMAC_JUMBO_FRAME_SUPPORT     
    if (wbuf1 != NULL) {

        if(!tid->paused ){
            if(epid == sc->sc_uapsd_ep)
                goto send1;
            if( ath_htc_can_xmit(sc, wbuf_get_priority(wbuf1)) == 0 )
                goto queue1;
send1:            
            htcstatus1 = HTCSendPkt(sc->sc_host_htc_handle, NULL, wbuf1, epid);
        }

        if (qdf_unlikely((htcstatus1 != A_OK) || (htcstatus1 == A_CREDIT_UNAVAILABLE)) || tid->paused){
            if(epid == sc->sc_uapsd_ep)
                return -1;

queue1:
            ath_htc_txep_queue(sc, wbuf1, an, tfh1->tidno, epid);
        }
    }
#endif
    return 0;
#else
    if (wbuf != NULL) {
        HTCSendPkt(sc->sc_host_htc_handle, NULL, wbuf, epid);
        return 0;
    }

#endif

}


int
ath_htc_tx_start(ath_dev_t dev, wbuf_t wbuf, ieee80211_tx_control_t *txctl)
{
#define ATH_DATA_TYPE_AGGR              0x01
#define ATH_DATA_TYPE_NON_AGGR          0x02
#define ATH_HTC_WMM_THRESHOLD           200
#define ATH_ETHERTYPE_IP                0x0800
#define ATH_80211_NONE_SNAP__LENGTH     30 /* 24 + 0 + 6 */
#define ATH_80211_WEP_SNAP_LENGTH       34 /* 24 + 4 + 6 */
#define ATH_80211_TKIP_SNAP_LENGTH      38 /* 24 + 8 + 6 */
#define ATH_80211_AES_SNAP_LENGTH       38 /* 24 + 8 + 6 */
#define ATH_80211_QOSPAD_LENGTH         4  /* 2 + 2 */

    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    
    int htcstatus = 0;
    
    if (txctl->isdata)
    {
        ath_data_hdr_t tfh;
        u_int8_t *anbdata;
        struct ath_node *an = ATH_NODE(txctl->an);
        ath_atx_tid_t *tid = ATH_AN_2_TID(an, txctl->tidno);
        HTC_ENDPOINT_ID     epid;
        u_int32_t           flags;
        struct ath_vap      *avp = sc->sc_vaps[txctl->if_id];
        int                 usecrypto = 0;

#ifdef ATH_SUPPORT_DFS
        /*
         * If we detect radar on the current channel, stop sending data
         * packets. There is a DFS requirment that the AP should stop
         * sending data packet within 200 ms of radar detection
         */
        if (sc->sc_curchan.priv_flags & CHANNEL_INTERFERENCE)
            return -EIO;
#endif

        switch (txctl->qnum) {
            case 0:
                epid = sc->sc_data_BE_ep;
                break;
            case 2:
                epid = sc->sc_data_VI_ep;
                break;
            case 3:
                epid = sc->sc_data_VO_ep;
                break;
            case 1:
            default:
                epid = sc->sc_data_BK_ep;
                break;
        }

        HTCTxComp(sc->sc_host_htc_handle, wbuf, epid);

        /* CAB QUEUE : re-assign service */
        if (txctl->ps && txctl->ismcast) {
            epid = sc->sc_cab_ep;

            /*
            * avp->av_mcastq.axq_depth work with ath_start_tx_dma(), 
            * but this job switch to ath_htc_tx_start() if HTC support.
            * HTC define it to be a new simple counter. 
             */
            avp->av_mcastq.axq_depth++;
        }

        OS_MEMZERO(&tfh, sizeof(struct __data_header));

        if (txctl->istxfrag == 0) {
            u_int8_t ethertypeOffset = 0;

            switch (txctl->keytype) {
                case HAL_KEY_TYPE_WEP:
                    ethertypeOffset = (txctl->isqosdata)? (ATH_80211_WEP_SNAP_LENGTH + ATH_80211_QOSPAD_LENGTH): ATH_80211_WEP_SNAP_LENGTH;
                    break;
                case HAL_KEY_TYPE_TKIP:
                    ethertypeOffset = (txctl->isqosdata)? (ATH_80211_TKIP_SNAP_LENGTH + ATH_80211_QOSPAD_LENGTH): ATH_80211_TKIP_SNAP_LENGTH;
                    break;
                case HAL_KEY_TYPE_AES:
                    ethertypeOffset = (txctl->isqosdata)? (ATH_80211_AES_SNAP_LENGTH + ATH_80211_QOSPAD_LENGTH): ATH_80211_AES_SNAP_LENGTH;
                    break;
                case HAL_KEY_TYPE_CLEAR:
                default:
                    ethertypeOffset = (txctl->isqosdata)? (ATH_80211_NONE_SNAP__LENGTH + ATH_80211_QOSPAD_LENGTH): ATH_80211_NONE_SNAP__LENGTH;
                    break;
            }


            //printk("%s: ipVersion = 0x%x, ipv4FlagAndFragOffset = 0x%x, ethertypeOffset = %d\n", __func__, ipVersion, ipv4FlagAndFragOffset, ethertypeOffset);
        }
        tfh.ni_index = txctl->nodeindex;
        tfh.vap_index = txctl->vapindex;
#define OWLMAX_RETRIES 10
        tfh.maxretry     =  OWLMAX_RETRIES ; /* Accesing This varibale Each
                                                packet maxretry can be 
                                                changed dynamically */

        if ((tfh.vap_index >= ATH_HTC_MAX_VAP_NUM) || (tfh.ni_index >= ATH_HTC_MAX_NODE_NUM)) {
            wbuf_free(wbuf);
            KASSERT(0, ("%s: VAP/Node index exceeds the HTC capability, vap/node index = %d/%d !\n", __func__, tfh.vap_index, tfh.ni_index));
            return 0;
        }

        if (tid->addba_exchangecomplete)
        {
            tfh.datatype = ATH_DATA_TYPE_AGGR;
        }
        else
        {
            tfh.datatype = ATH_DATA_TYPE_NON_AGGR;
        }

        tfh.tidno = wbuf_get_tid(wbuf);
        if (tfh.tidno >= ATH_HTC_WME_NUM_TID) {
            wbuf_free(wbuf);
            KASSERT(0, ("%s: tidno exceeds the HTC capability, tidno = %d !\n", __func__, tfh.tidno));
            return 0;
        }

        switch (sc->sc_protmode) {
            case PROT_M_RTSCTS:
                flags = IEEE80211_PROT_RTSCTS;
                break;
            case PROT_M_CTSONLY:
                flags = IEEE80211_PROT_CTSONLY;
                break;
            case PROT_M_NONE:
            default:
                flags = IEEE80211_PROT_NONE;
                break;
        }

        if (an->an_smmode == ATH_SM_PWRSAV_DYNAMIC) {
            flags = IEEE80211_PROT_RTSCTS;
        }

        if (txctl->flags & HAL_TXDESC_RTSENA)
            flags = IEEE80211_PROT_RTSCTS;

        if (txctl->txpower == ATH_TX_POWER_SRM)
            flags |= ATH_TX_SRM;

        /* To force target send using minimum rate */
        if (txctl->use_minrate) {
            flags |= TFH_FLAGS_USE_MIN_RATE;
        }

#ifdef ENCAP_OFFLOAD        
        if(txctl->shortPreamble)
            flags |= ATH_SHORT_PREAMBLE;        

        if(wbuf_is_moredata(wbuf))
            flags |= ATH_DH_FLAGS_MOREDATA;

        /*
         * Assert our Exemption policy.  We assert it blindly at first, then
         * take the presence/absence of a key into acct.
         *
         * Lookup the ExemptionActionType in the send context info of this frame
         * to determine if we need to encrypt the frame.
         */
        switch (wbuf_get_exemption_type(wbuf)) {
            case WBUF_EXEMPT_NO_EXEMPTION:
                /*
                * We want to encrypt this frame.
                 */
                usecrypto = 1;
                break;

            case WBUF_EXEMPT_ALWAYS:
                /*
                * We don't want to encrypt this frame.
                 */
                usecrypto = 0;
                break;

            case WBUF_EXEMPT_ON_KEY_MAPPING_KEY_UNAVAILABLE:
                /*
                * We encrypt this frame if and only if a key mapping key is set.
                 */
                usecrypto = (txctl->key_mapping_key) ? 1 : 0;
                
                break;

            default:
                ASSERT(0);
                usecrypto = 1;
                break;
        }

        /*
         * If the frame is to be encrypted, but no key is not set, either reject the frame 
         * or clear the WEP bit.
         */
        if (usecrypto && (txctl->keytype == HAL_KEY_TYPE_CLEAR)) {
            /*
             * If this is a unicast frame or if the BSSPrivacy is on, reject the frame.
             * Otherwise, clear the WEP bit so we will not encrypt the frame. In other words,
             * we'll send multicast frame in clear if multicast key hasn't been setup.
             */
            if (!txctl->ismcast) {
                wbuf_free(wbuf);
                printk("Rejecting unicast encrypted frame as key is not set\n");
                return 0;
            }
            printk("clearing privacy bit for mcast packet\n");
            usecrypto = 0; /* XXX: is this right??? */
        }

        if (usecrypto)
             flags |= ATH_DH_FLAGS_PRIVACY;

        tfh.keyid = txctl->keyid;
#endif
        
        tfh.flags = cpu_to_be32(flags);

        // for encryption
        if( (txctl->keytype == HAL_KEY_TYPE_TKIP) && (txctl->istxfrag))
        {
            tfh.keytype = HAL_KEY_TYPE_WEP;  
        } else {      
            tfh.keytype = txctl->keytype;   
        }
        tfh.keyix = txctl->keyix;

        /* Check for headroom size */
        if (wbuf_hdrspace(wbuf) < ATH_HTC_HDRSPACE + sizeof(struct __data_header))
        {
            wbuf = wbuf_realloc_hdrspace(wbuf, ATH_HTC_HDRSPACE + sizeof(struct __data_header));

            if (wbuf == NULL) {
                DPRINTF(sc, ATH_DEBUG_HTC_WMI, "%s: drop tx frame - insufficient header room space I\n", __func__);
                return 0;
            }
        }

        anbdata = wbuf_push(wbuf, sizeof(struct __data_header));
        OS_MEMCPY((u_int8_t *)anbdata, (u_int8_t *)&tfh, sizeof(struct __data_header));


        if (txctl->isuapsd) {
            ath_htc_tx_queue_uapsd(sc, wbuf, txctl);
            return 0;
        }

        ath_htc_send_pkt(dev, wbuf, epid, an);
    }
    else
    {
        ath_mgt_hdr_t txmgt;
        u_int8_t *anbdata;

        HTCTxComp(sc->sc_host_htc_handle, wbuf, sc->sc_mgmt_ep);
        OS_MEMZERO(&txmgt, sizeof(ath_mgt_hdr_t));

        txmgt.ni_index = txctl->nodeindex;
        txmgt.vap_index = txctl->vapindex;
        if ((txmgt.vap_index >= ATH_HTC_MAX_VAP_NUM) || (txmgt.ni_index >= ATH_HTC_MAX_NODE_NUM)) {
            wbuf_free(wbuf);
            KASSERT(0, ("%s: VAP/Node index exceeds the HTC capability, vap/node index = %d/%d !\n", __func__, txmgt.vap_index, txmgt.ni_index));
            return 0;
        }

        txmgt.tidno = 0;
        txmgt.flags = 0;

        // for encryption
        txmgt.keytype = txctl->keytype; 
        txmgt.keyix = txctl->keyix;

        /* Check for headroom size */
        if (wbuf_hdrspace(wbuf) < ATH_HTC_HDRSPACE + sizeof(ath_mgt_hdr_t))
        {
            wbuf = wbuf_realloc_hdrspace(wbuf, ATH_HTC_HDRSPACE + sizeof(ath_mgt_hdr_t));

            if (wbuf == NULL) {
                DPRINTF(sc, ATH_DEBUG_HTC_WMI,"%s: drop tx frame - insufficient header room space I\n", __func__);
                return 0;
            }
        }

        anbdata = wbuf_push(wbuf, sizeof(ath_mgt_hdr_t));
        OS_MEMCPY((u_int8_t *)anbdata, (u_int8_t *)&txmgt, sizeof(ath_mgt_hdr_t));

        htcstatus = HTCSendPkt(sc->sc_host_htc_handle, NULL, wbuf, sc->sc_mgmt_ep);
        if (qdf_unlikely((htcstatus != A_OK) )){
            printk("Freeing Management pkt due to credit unavailibility \n");
            wbuf_free(wbuf);
            wbuf = NULL;


        }

    }

    return 0;

#undef ATH_DATA_TYPE_AGGR
#undef ATH_DATA_TYPE_NON_AGGR
#undef ATH_HTC_WMM_THRESHOLD
#undef ATH_ETHERTYPE_IP
#undef ATH_80211_NONE_SNAP__LENGTH
#undef ATH_80211_WEP_SNAP_LENGTH
#undef ATH_80211_TKIP_SNAP_LENGTH
#undef ATH_80211_AES_SNAP_LENGTH
#undef ATH_80211_QOSPAD_LENGTH
}
#endif
#ifndef MAGPIE_HIF_GMAC
int
ath_htc_rx_init(ath_dev_t dev, int nbufs)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_buf *bf;
    struct ath_buf *bf_first;
    int error = 0;
    sc->sc_htcrxpause = 0;
    do {
        ATH_RXFLUSH_LOCK_INIT(sc);
        sc->sc_rxflush = 0;
        ATH_RXBUF_LOCK_INIT(sc);

        /*
         * Cisco's VPN software requires that drivers be able to
         * receive encapsulated frames that are larger than the MTU.
         * Since we can't be sure how large a frame we'll get, setup
         * to handle the larges on possible.
         */
        sc->sc_rxbufsize = IEEE80211_MAX_MPDU_LEN;

        DPRINTF(sc,ATH_DEBUG_RESET, "%s: cachelsz %u rxbufsize %u\n",
                __func__, sc->sc_cachelsz, sc->sc_rxbufsize);

        /*
         * Initialize rx descriptors
         */
        error = ath_descdma_setup(sc, &sc->sc_rxdma, &sc->sc_rxbuf,
                                  "rx", nbufs, 1, 1, ATH_FRAG_PER_MSDU);
        if (error != 0) {
            printk("failed to allocate rx descriptors: %d\n", error);
            break;
        }

        bf_first = TAILQ_FIRST(&sc->sc_rxbuf);

        /* Iterate descriptor list. */
        for (bf = bf_first; bf; bf = TAILQ_NEXT(bf, bf_list)) {
            /* Clear the bf_status */
            ATH_RXBUF_RESET(bf);
        }

        sc->sc_rxlink = NULL;

    } while (0);

    if (error) {
        ath_rx_cleanup(sc);
    }

    return error;
}
#else
int
ath_htc_rx_init(ath_dev_t dev, int nbufs)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    int error = 0;

    ATH_RXFLUSH_LOCK_INIT(sc);
    sc->sc_rxflush = 0;
    sc->sc_htcrxpause = 0;
    sc->sc_rxbufsize = IEEE80211_MAX_MPDU_LEN;

    sc->sc_rxlink = NULL;
    
#ifdef ATH_HTC_SG_SUPPORT
    sc->sc_sgrx.Onprocess = 0;
    sc->sc_sgrx.nextseq = 0;
    sc->sc_sgrx.pktremaining = 0;
    sc->sc_sgrx.Firstseg= NULL;
#endif
    ATH_HTC_RXBUF_LOCK_INIT(sc);
    qdf_nbuf_queue_init(&sc->sc_rxwbufhead);

    return error;
}

#endif
HAL_BOOL
ath_wmi_aborttxep(struct ath_softc *sc)
{
    u_int8_t cmd_status;

    if (sc->sc_invalid)
        return AH_TRUE;

    wmi_cmd(sc->sc_host_wmi_handle, 
            WMI_ABORT_TX_DMA_CMDID, 
            NULL, 
            0,
            &cmd_status,
            sizeof(cmd_status),
            ATH_HTC_WMI_TIMEOUT_DEFAULT);

    if(cmd_status == AH_FALSE)
        return AH_FALSE;

    return AH_TRUE;
}

HAL_BOOL
ath_wmi_abortrecv(struct ath_softc *sc)
{
    struct ath_hal *ah = sc->sc_ah;
    u_int32_t cmd_status;

    ath_hal_setrxfilter(ah, 0); /* clear recv filter */

    /* disable DMA engine */
    wmi_cmd(sc->sc_host_wmi_handle, 
            WMI_STOP_DMA_RECV_CMDID, 
            NULL, 
            0,
            (u_int8_t *) &cmd_status,
            sizeof(cmd_status),
            ATH_HTC_WMI_TIMEOUT_DEFAULT);

    if(cmd_status == AH_FALSE)
        return AH_FALSE;

    wmi_cmd(sc->sc_host_wmi_handle, 
            WMI_RX_LINK_CMDID, 
            NULL, 
            0,
            (u_int8_t *) &cmd_status,
            sizeof(cmd_status),
            ATH_HTC_WMI_TIMEOUT_DEFAULT);

    return AH_TRUE;
}

void
ath_wmi_get_target_stats(ath_dev_t dev, HTC_HOST_TGT_MIB_STATS *stats )
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    int cmd_status;

    cmd_status = wmi_cmd(sc->sc_host_wmi_handle, 
        WMI_TGT_STATS_CMDID, 
        (u_int8_t *) NULL,
        0,
        (u_int8_t *) stats,
        sizeof(*stats),
        100 /* timeout unit? */);

    if(cmd_status) {
        printk("%s, get_status fail = %x\n", __FUNCTION__, cmd_status);
    }
    else {
        /* Translate status to host endian */
        stats->tx_shortretry        = be32_to_cpu(stats->tx_shortretry);
        stats->tx_longretry         = be32_to_cpu(stats->tx_longretry);
        stats->tx_xretries          = be32_to_cpu(stats->tx_xretries);        
        stats->ht_tx_unaggr_xretry  = be32_to_cpu(stats->ht_tx_unaggr_xretry);
        stats->ht_tx_xretries       = be32_to_cpu(stats->ht_tx_xretries);
        stats->tx_pkt               = be32_to_cpu(stats->tx_pkt);
        stats->tx_retry             = be32_to_cpu(stats->tx_retry);
        stats->tx_aggr              = be32_to_cpu(stats->tx_aggr);
        stats->txaggr_retry         = be32_to_cpu(stats->txaggr_retry);
        stats->txaggr_sub_retry     = be32_to_cpu(stats->txaggr_sub_retry);
    }
}

static struct ath_buf *
ath_htc_beacon_generate(struct ath_softc *sc, int if_id, int *needmark)
{
    struct ath_buf *bf;
    struct ath_vap *avp;
    wbuf_t wbuf;
    int ncabq;

    avp = sc->sc_vaps[if_id];
    ASSERT(avp);
    
    if (avp->av_bcbuf == NULL)
    {
        DPRINTF(sc, ATH_DEBUG_ANY, "%s: avp=%pK av_bcbuf=%pK\n",
                __func__, avp, avp->av_bcbuf);
        return NULL;
    }
    bf = avp->av_bcbuf;
    wbuf = (wbuf_t)bf->bf_mpdu;

#ifdef ATH_SUPERG_DYNTURBO
    /* 
     * If we are using dynamic turbo, update the
     * capability info and arrange for a mode change
     * if needed.
     */
    if (sc->sc_dturbo)
    {
        u_int8_t dtim;
        dtim = ((avp->av_boff.bo_tim[2] == 1) || (avp->av_boff.bo_tim[3] == 1));
    }
#endif
    /*
     * Update dynamic beacon contents.  If this returns
     * non-zero then we need to remap the memory because
     * the beacon frame changed size (probably because
     * of the TIM bitmap).
     */
    ncabq = avp->av_mcastq.axq_depth;
    
    /*
     * avp->av_mcastq.axq_depth increase in ath_start_tx_dma(), 
     * but this job switch to ath_htc_tx_start() if HTC support.
     * HTC define it to be a new simple counter. 
     */
    avp->av_mcastq.axq_depth = 0;

    if (!sc->sc_ieee_ops->update_beacon)
    {
        /*
         * XXX: if protocol layer doesn't support update beacon at run-time,
         * we have to free the old beacon and allocate a new one.
         */
        if (sc->sc_ieee_ops->get_beacon)
        {
            ieee80211_tx_status_t tx_status;
#if 0            
            wbuf_unmap_single(sc->sc_osdev, wbuf, BUS_DMA_TODEVICE,
                              OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));
#endif
            tx_status.flags = 0;
            tx_status.retries = 0;
#ifdef  ATH_SUPPORT_TxBF
            tx_status.txbf_status = 0;
#endif
            bf->bf_daddr = 0;
            bf->bf_mpdu = 0;
            sc->sc_ieee_ops->tx_complete(wbuf, &tx_status, 0);

            wbuf = sc->sc_ieee_ops->get_beacon(sc->sc_ieee, if_id, &avp->av_boff, &avp->av_btxctl);
            if (wbuf == NULL)
                return NULL;

            bf->bf_mpdu = wbuf;
#if 0
            bf->bf_buf_addr[0] = wbuf_map_single(sc->sc_osdev, wbuf, BUS_DMA_TODEVICE,
                                              OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));
#endif
        }
    }
    else
    {
#if UMAC_SUPPORT_WNM
        int retVal = sc->sc_ieee_ops->update_beacon(sc->sc_ieee, if_id, &avp->av_boff, wbuf, ncabq, 0);
#else
        int retVal = sc->sc_ieee_ops->update_beacon(sc->sc_ieee, if_id, &avp->av_boff, wbuf, ncabq);
#endif
        if (retVal == 1) {
#if 0
            wbuf_unmap_single(sc->sc_osdev, wbuf, BUS_DMA_TODEVICE,
                              OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));
            bf->bf_buf_addr[0] = wbuf_map_single(sc->sc_osdev, wbuf, BUS_DMA_TODEVICE,
                                              OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));
#endif
        } else if (retVal == 0) {
#if 0
            OS_SYNC_SINGLE(sc->sc_osdev,
                       bf->bf_buf_addr, wbuf_get_pktlen(wbuf), BUS_DMA_TODEVICE,
                       OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));
#endif
        } else if (retVal < 0) {
            /* ieee layer does not want to send beacon */
            return NULL;
        }
    }

    /*
     * if the CABQ traffic from previous DTIM is pending and the current
     *  beacon is also a DTIM. 
     *  1) if there is only one vap let the cab traffic continue. 
     *  2) if there are more than one vap and we are using staggered
     *     beacons, then drain the cabq by dropping all the frames in
     *     the cabq so that the current vaps cab traffic can be scheduled.
     */
    if (ncabq && (avp->av_boff.bo_tim[4] & 1) && sc->sc_cabq->axq_depth)
    {
        if (sc->sc_nvaps > 1 && sc->sc_stagbeacons)
        {
            ath_tx_draintxq(sc, sc->sc_cabq, AH_FALSE);
            DPRINTF(sc, ATH_DEBUG_BEACON,
                    "%s: flush previous cabq traffic\n", __func__);
        }
    }

#if 0
    // Don't setup tx descriptor in host side

    /*
     * Construct tx descriptor.
     */
    ath_beacon_setup(sc, avp, bf);

    /*
     * Enable the CAB queue before the beacon queue to
     * insure cab frames are triggered by this beacon.
     */
    if (avp->av_boff.bo_tim[4] & 1)
    {   /* NB: only at DTIM */
        struct ath_txq *cabq = sc->sc_cabq;
        struct ath_buf *bfmcast;
        /*
         * Move everything from the vap's mcast queue 
         * to the hardware cab queue.
         * XXX MORE_DATA bit?
         */
        ATH_TXQ_LOCK(&avp->av_mcastq);
        ATH_TXQ_LOCK(cabq);
        if ((bfmcast = TAILQ_FIRST(&avp->av_mcastq.axq_q)) != NULL)
        {
            /* link the descriptors */
            if (cabq->axq_link == NULL)
            {
                ath_hal_puttxbuf(ah, cabq->axq_qnum, bfmcast->bf_daddr);
            }
            else
            {
#ifdef AH_NEED_DESC_SWAP
                *cabq->axq_link = cpu_to_le32(bfmcast->bf_daddr);
#else
                *cabq->axq_link = bfmcast->bf_daddr;
#endif
            }
            /* append the private vap mcast list to  the cabq */
            ATH_TXQ_MOVE_MCASTQ(&avp->av_mcastq, cabq);
#ifdef ATH_ADDITIONAL_STATS
            sc->sc_stats.ast_txq_packets[cabq->axq_qnum]++;
#endif
        }
        /* NB: gated by beacon so safe to start here */
        if (!TAILQ_EMPTY(&(cabq->axq_q)))
        {
            ath_hal_txstart(ah, cabq->axq_qnum);
        }
        ATH_TXQ_UNLOCK(cabq);
        ATH_TXQ_UNLOCK(&avp->av_mcastq);
    }
#endif

    /*
     * generate a new buf for beacon transmission
     */
    {
        wbuf_t wbuf_org;
        u_int8_t *src;
        u_int8_t *dst;

        wbuf_org = (wbuf_t)bf->bf_mpdu;

        if (wbuf_org != NULL) {
            wbuf = wbuf_alloc(sc->sc_osdev, WBUF_USB_TX, wbuf_get_pktlen(wbuf_org));

            if (wbuf == NULL) {
                printk("%s : allocate wbuf fail for beacon\n", __func__);
                return NULL;
            }
            else {
                /* copy data to new wbuf */
                dst = wbuf_header(wbuf); 
                src = wbuf_header(wbuf_org);

                memcpy(dst, src, wbuf_get_pktlen(wbuf_org));
                wbuf_set_pktlen(wbuf, wbuf_get_pktlen(wbuf_org));
                bf->bf_daddr = (dma_addr_t)wbuf;
            }
        } else {
            KASSERT(0, ("%s: wbuf_org is NULL\n", __func__));
            return NULL;
        }
    }

    return bf;
}

void
ath_wmi_beacon_stuck_internal(struct ath_softc *sc)
{
    struct ath_hal *ah = sc->sc_ah;
    u_int32_t rx_clear = 0, rx_frame = 0, tx_frame = 0;
    u_int32_t show_cycles = 0;
    int needmark = 0;

    show_cycles = ath_hal_getMibCycleCountsPct(ah, 
                  &rx_clear, &rx_frame, &tx_frame);
    
    sc->sc_bmisscount++;
#ifdef MAGPIE_HIF_GMAC   
    //printk("bmiss %d \n", sc->sc_bmisscount);  
#else
    printk("%s: bmisscount = %d, show_cycles = %d, rx_clear = %d, rx_frame = %d, tx_frame = %d\n", __func__, sc->sc_bmisscount, show_cycles, rx_clear, rx_frame, tx_frame);
#endif    
#ifdef ATH_SUPPORT_DFS    
    if((sc->sc_curchan.priv_flags & CHANNEL_DFS) && (sc->sc_bmisscount == 1))
            sc->sc_3stream_sigflag1 = ath_hal_get3StreamSignature(ah);

    if((sc->sc_curchan.priv_flags & CHANNEL_DFS) && (sc->sc_3stream_sigflag1) && (sc->sc_bmisscount == 2))
            sc->sc_3stream_sigflag2 = ath_hal_get3StreamSignature(ah);

    if((sc->sc_3stream_sigflag1) && (sc->sc_3stream_sigflag2)) {
            sc->sc_3stream_sigflag1 = 0;
            sc->sc_3stream_sigflag2 = 0;
            ath_hal_dmaRegDump(ah);
            ath_hal_dfs3streamfix(sc->sc_ah, 1);
            ath_hal_forcevcs(sc->sc_ah);
            sc->sc_3streamfix_engaged = 1;
            OS_SET_TIMER(&sc->sc_dfs->sc_dfs_war_timer, (10 * 60 * 1000));
            printk("%s engaing fix 0x9944 %x 0x9970 %x\n",__FUNCTION__,OS_REG_READ(sc->sc_ah,0x9944),
                            OS_REG_READ(sc->sc_ah,0x9970));
    }
#endif
    /* XXX: doth needs the chanchange IE countdown decremented.
     *      We should consider adding a net80211 call to indicate
     *      a beacon miss so appropriate action could be taken
     *      (in that layer).
     */
    if (sc->sc_bmisscount < (BSTUCK_THRESH_PERVAP * sc->sc_nvaps)) {
    
        if (show_cycles && !tx_frame && (rx_clear >= 99)) {
            /* Meant to reduce PHY errors and potential false detects */
            if (sc->sc_toggle_immunity)
                ath_hal_set_immunity(ah, AH_TRUE);
    
            sc->sc_noise++;
        }
    
        /* If the HW requires hang detection, notify caller
         * and detect in the tasklet context.
         */
        if (ATH_HANG_WAR_REQ(sc)) {
            sc->sc_hang_check = AH_TRUE;
            needmark = 1;
            return;
        }
    
        if (sc->sc_noreset) {
            printk("%s: missed %u consecutive beacons\n",
                   __func__, sc->sc_bmisscount);
            if (show_cycles) {
                /*
                 * Display cycle counter stats from HAL to
                 * aide in debug of stickiness.
                 */
                printk("%s: busy times: rx_clear=%d, rx_frame=%d, tx_frame=%d\n", __func__, rx_clear, rx_frame, tx_frame);
            } else {
                printk("%s: unable to obtain busy times\n", __func__);
            }
        } else {
            DPRINTF(sc, ATH_DEBUG_BEACON_PROC,
                    "%s: missed %u consecutive beacons\n",
                    __func__, sc->sc_bmisscount);
        }
    } else if (sc->sc_bmisscount >= (BSTUCK_THRESH_PERVAP * sc->sc_nvaps)) {
        if (sc->sc_noreset) {
            if (sc->sc_bmisscount == (BSTUCK_THRESH_PERVAP * sc->sc_nvaps)) {
                printk("%s: beacon is officially stuck\n",
                       __func__);
                ath_hal_dmaRegDump(ah);
            }
        } else {
            DPRINTF(sc, ATH_DEBUG_BEACON_PROC,
                    "%s: beacon is officially stuck\n",
                    __func__);
    
            if (show_cycles && rx_clear >= 99) {
                sc->sc_hang_war |= HAL_BB_HANG_DETECTED;
    
                if (rx_frame >= 99) {
                    printk("Busy environment detected\n");
                } else {
                    printk("Interference detected\n");
                }
    
                printk("rx_clear=%d, rx_frame=%d, tx_frame=%d\n",
                       rx_clear, rx_frame, tx_frame);
    
                sc->sc_noise = 0;
            }
            needmark = 1;
        }
    }

    if (needmark) {
        /* We have a beacon stuck */
        if (((AH_TRUE == sc->sc_hang_check) && ath_hw_hang_check(sc)) ||
            (!sc->sc_noreset && (sc->sc_bmisscount >= (BSTUCK_THRESH_PERVAP * sc->sc_nvaps)))) {
            printk("%s: --- Beacon Stuck ---\n", __func__);
            
            ATH_PS_WAKEUP(sc);
            ath_bstuck_tasklet(sc);
            ATH_CLEAR_HANGS(sc);
            ATH_PS_SLEEP(sc);
        }
    }
}

void
ath_htc_beacon_tasklet(struct ath_softc *sc, int *needmark, u_int64_t currentTsf,
    u_int8_t beaconPendingCount, u_int8_t update)
{
#define TSF_TO_TU(_h,_l) \
    ((((u_int32_t)(_h)) << 22) | (((u_int32_t)(_l)) >> 10))
    
    struct ath_buf *bf=NULL;
    int slot, if_id = 0, vap_index = 0;
    dma_addr_t bfaddr;
    u_int32_t bc = 0; /* beacon count */

    //Avoid dealing SWBA event when set to FULL SLEEP or in FULL SLEEP
    if (sc->sc_pwrsave.ps_set_state == ATH_PWRSAVE_FULL_SLEEP || sc->sc_pwrsave.ps_pwrsave_state == ATH_PWRSAVE_FULL_SLEEP) {
       return;
    }

#ifdef ATH_SUPPORT_DFS
    /* Make sure no beacons go out during the DFS wait period (CAC timer)*/
    if (sc->sc_dfs && sc->sc_dfs->sc_dfswait) {
       return;
    }
#endif

    /*
     * Check if the previous beacon has gone out.  If
     * not don't try to post another, skip this period
     * and wait for the next.  Missed beacons indicate
     * a problem and should not occur.  If we miss too
     * many consecutive beacons reset the device.
     */
    if (beaconPendingCount != 0)
    {
#if 0
        show_cycles = ath_hal_getMibCycleCountsPct(ah, 
                      &rx_clear, &rx_frame, &tx_frame);

        sc->sc_bmisscount++;

        printk("%s: bmisscount = %d, show_cycles = %d, rx_clear = %d, rx_frame = %d, tx_frame = %d\n", __func__, sc->sc_bmisscount, show_cycles, rx_clear, rx_frame, tx_frame);
        
        /* XXX: doth needs the chanchange IE countdown decremented.
         *      We should consider adding a net80211 call to indicate
         *      a beacon miss so appropriate action could be taken
         *      (in that layer).
         */
        if (sc->sc_bmisscount < BSTUCK_THRESH) {

            if (show_cycles && !tx_frame && (rx_clear >= 99)) {
                /* Meant to reduce PHY errors and potential false detects */
                if (sc->sc_toggle_immunity)
                    ath_hal_set_immunity(ah, AH_TRUE);

                sc->sc_noise++;
            }

            /* If the HW requires hang detection, notify caller
             * and detect in the tasklet context.
             */
            if (ATH_HANG_WAR_REQ(sc)) {
                sc->sc_hang_check = AH_TRUE;
                *needmark = 1;
                return;
            }

            if (sc->sc_noreset) {
                printk("%s: missed %u consecutive beacons\n",
                       __func__, sc->sc_bmisscount);
                if (show_cycles) {
                    /*
                     * Display cycle counter stats from HAL to
                     * aide in debug of stickiness.
                     */
                    printk("%s: busy times: rx_clear=%d, rx_frame=%d, tx_frame=%d\n", __func__, rx_clear, rx_frame, tx_frame);
                } else {
                    printk("%s: unable to obtain busy times\n", __func__);
                }
            } else {
                DPRINTF(sc, ATH_DEBUG_BEACON_PROC,
                        "%s: missed %u consecutive beacons\n",
                        __func__, sc->sc_bmisscount);
            }
        } else if (sc->sc_bmisscount >= BSTUCK_THRESH) {
            if (sc->sc_noreset) {
                if (sc->sc_bmisscount == BSTUCK_THRESH) {
                    printk("%s: beacon is officially stuck\n",
                           __func__);
                    ath_hal_dmaRegDump(ah);
                }
            } else {
                DPRINTF(sc, ATH_DEBUG_BEACON_PROC,
                        "%s: beacon is officially stuck\n",
                        __func__);

                if (show_cycles && rx_clear >= 99) {
                    sc->sc_hang_war |= HAL_BB_HANG_DETECTED;

                    if (rx_frame >= 99) {
                        printk("Busy environment detected\n");
                    } else {
                        printk("Interference detected\n");
                    }

                    printk("rx_clear=%d, rx_frame=%d, tx_frame=%d\n",
                           rx_clear, rx_frame, tx_frame);

                    sc->sc_noise = 0;
                }
                *needmark = 1;
            }
        }
#endif

        ath_usb_schedule_thread(sc, KEVENT_BEACON_STUCK);
        return;
    }

    if (sc->sc_toggle_immunity)
    {
        ath_usb_schedule_thread(sc, KEVENT_BEACON_IMMUNITY);
//        ath_hal_set_immunity(ah, AH_FALSE);
    }
    sc->sc_noise = 0;
    sc->sc_hang_check = AH_FALSE;

    if (sc->sc_bmisscount != 0)
    {
        if (sc->sc_noreset) {
            printk("%s: resume beacon xmit after %u misses\n",
                   __func__, sc->sc_bmisscount);
        } else {
            DPRINTF(sc, ATH_DEBUG_BEACON_PROC,
                    "%s: resume beacon xmit after %u misses\n",
                    __func__, sc->sc_bmisscount);
        }
        sc->sc_bmisscount = 0;
    }

    /*
     * Generate beacon frames.  If we are sending frames
     * staggered then calculate the slot for this frame based
     * on the tsf to safeguard against missing an swba.
     * Otherwise we are bursting all frames together and need
     * to generate a frame for each vap that is up and running.
     */
    if (sc->sc_stagbeacons)
    {       /* staggered beacons */
        u_int64_t tsf;
        u_int32_t tsftu;
        u_int16_t intval;
        int nslots;

        nslots = sc->sc_nslots;

        if (sc->sc_ieee_ops->get_beacon_config)
        {
            ieee80211_beacon_config_t conf;
            sc->sc_ieee_ops->get_beacon_config(sc->sc_ieee, ATH_IF_ID_ANY, &conf);
            intval = conf.beacon_interval;
        }
        else
            intval = ATH_DEFAULT_BINTVAL;

        if (intval == 0 || nslots == 0) {  
            /*
             * This should not happen. We're seeing zero bintval sometimes 
             * in WDS testing but is not easily reproducible 
             */
            return;
        }

//        tsf = ath_hal_gettsf64(ah);
        tsf = currentTsf;
        tsftu = TSF_TO_TU(tsf>>32, tsf);
        slot = ((tsftu % intval) * nslots) / intval;
        if_id = sc->sc_bslot[(slot + 1) % nslots];

        DPRINTF(sc, ATH_DEBUG_BEACON_PROC,
                "%s: slot %d [tsf %llu tsftu %u intval %u] if_id %d\n",
                __func__, slot, (unsigned long long)tsf, tsftu, intval, if_id);
        bfaddr = 0;
        if (if_id != ATH_IF_ID_ANY)
        {
            bf = ath_htc_beacon_generate(sc, if_id, needmark);
            if (bf != NULL) {
                bfaddr = bf->bf_daddr;
                bc = 1;
            }
        }
    }
    else
    {                        /* burst'd beacons */
        dma_addr_t *bflink;

        bfaddr = 0;
        bflink = &bfaddr;
        /* XXX rotate/randomize order? */
        for (slot = 0; slot < ATH_BCBUF; slot++)
        {
            if_id = sc->sc_bslot[slot];
            if (if_id != ATH_IF_ID_ANY)
            {
                bf = ath_htc_beacon_generate(sc, if_id, needmark);
                if (bf != NULL)
                {
#ifdef AH_NEED_DESC_SWAP
                    if(bflink != &bfaddr)
                        *bflink = cpu_to_le32(bf->bf_daddr);
                    else
                        *bflink = bf->bf_daddr;
#else
                    *bflink = bf->bf_daddr;
#endif
                    //ath_hal_getdesclinkptr(ah, bf->bf_desc, &bflink);
                    bc ++;
                    vap_index = if_id;
                    bfaddr = bf->bf_daddr;
                }
            }
        }
        *bflink = 0;    /* link of last frame */
        if_id = vap_index;
    }

    /*
     * Handle slot time change when a non-ERP station joins/leaves
     * an 11g network.  The 802.11 layer notifies us via callback,
     * we mark updateslot, then wait one beacon before effecting
     * the change.  This gives associated stations at least one
     * beacon interval to note the state change.
     *
     * NB: The slot time change state machine is clocked according
     *     to whether we are bursting or staggering beacons.  We
     *     recognize the request to update and record the current
     *     slot then don't transition until that slot is reached
     *     again.  If we miss a beacon for that slot then we'll be
     *     slow to transition but we'll be sure at least one beacon
     *     interval has passed.  When bursting slot is always left
     *     set to ATH_BCBUF so this check is a noop.
     */
    /* XXX locking */
    if (sc->sc_updateslot == ATH_UPDATE)
    {
        sc->sc_updateslot = ATH_COMMIT; /* commit next beacon */
        sc->sc_slotupdate = slot;
    }
    else if (sc->sc_updateslot == ATH_COMMIT && sc->sc_slotupdate == slot)
        ath_usb_schedule_thread(sc, KEVENT_BEACON_SETSLOTTIME);

    if ((!sc->sc_stagbeacons || slot == 0) && (!sc->sc_diversity))
    {
        int otherant;
        /*
         * Check recent per-antenna transmit statistics and flip
         * the default rx antenna if noticeably more frames went out
         * on the non-default antenna.  Only do this if rx diversity
         * is off.
         * XXX assumes 2 anntenae
         */
        otherant = sc->sc_defant & 1 ? 2 : 1;
        if (sc->sc_ant_tx[otherant] > sc->sc_ant_tx[sc->sc_defant] + ATH_ANTENNA_DIFF)
        {
            DPRINTF(sc, ATH_DEBUG_BEACON,
                    "%s: flip defant to %u, %u > %u\n",
                    __func__, otherant, sc->sc_ant_tx[otherant],
                    sc->sc_ant_tx[sc->sc_defant]);
            ath_setdefantenna(sc, otherant);
        }
        sc->sc_ant_tx[1] = sc->sc_ant_tx[2] = 0;
    }
   
    if (update)
        return;

    if (bfaddr != 0)
    {
        ath_beacon_hdr_t *bchdr;
        wbuf_t wbuf;
        int status;
        u_int8_t *anbdata;
        int need_headroom = ATH_HTC_HDRSPACE + sizeof(ath_beacon_hdr_t);

        wbuf = (wbuf_t)bfaddr;

#if 1
        /* Check for headroom size */
        if (wbuf_hdrspace(wbuf) < need_headroom)
        {
            wbuf = wbuf_realloc_hdrspace(wbuf, need_headroom);

            if (wbuf == NULL) {
                printk("%s: Insufficient header room space and allocate new buffer fail I\n", __func__);
                return;
            }
        }
#endif

        anbdata = wbuf_push(wbuf, sizeof(ath_beacon_hdr_t));
        bchdr = (ath_beacon_hdr_t *)anbdata;
		bchdr->len_changed  = 0;
        bchdr->vap_index = if_id;
        
        status = HTCSendPkt(sc->sc_host_htc_handle, NULL, wbuf, sc->sc_beacon_ep);  // api parameters to be discussed, but notion is to send beacon packet to HTC

#ifdef ATH_SUPPORT_P2P
        /* We don't really have Beacon Done event, treat this as done event and always tx successfully. */
        ath_usb_schedule_thread(sc, KEVENT_BEACON_DONE);
#endif

#ifdef  ATH_HTC_TX_SCHED
        if (qdf_unlikely((status != A_OK) )){
            printk("Freeing Beacon pkt due to credit unavailibility \n");
			wbuf_free(wbuf);
            wbuf = NULL;


        }
#endif


#ifdef QCA_SUPPORT_CP_STATS
        pdev_cp_stats_tx_beacon_inc(sc->sc_pdev, bc);
        pdev_cp_stats_tx_mgmt_inc(sc->sc_pdev, bc);
#else
        sc->sc_stats.ast_be_xmit += bc;     /* XXX per-vap? */
        sc->sc_stats.ast_tx_mgmt += bc;     /* XXX per-vap? */
#endif

#ifdef ATH_ADDITIONAL_STATS
        sc->sc_stats.ast_txq_packets[sc->sc_bhalq]++;
#endif
    }
#undef TSF_TO_TU
}

/******************************************************************************/
/*!
**  \brief Startup beacon transmission for WINHTC adhoc mode
**
** Startup beacon transmission for adhoc mode when they are sent entirely
** by the hardware using the self-linked descriptor + veol trick.
**
**  \param sc Pointer to ATH object. "This" pointer.
**  \param if_id Integer containing index of VAP interface to start the beacon on.
**
**  \return N/A
**
**  compare with ath_beacon_start_adhoc() / common/lmac/ath_dev/ath_beacon.c
*/
void
ath_htc_beacon_start_adhoc(struct ath_softc *sc, int *needmark, int if_id, u_int8_t beaconPendingCount)
{
    struct ath_hal *ah = sc->sc_ah;
    struct ath_buf *bf;
    struct ath_vap *avp;
    wbuf_t wbuf;

    ath_beacon_hdr_t *bchdr;
    int status;
    u_int8_t *anbdata;
    int need_headroom = ATH_HTC_HDRSPACE + sizeof(ath_beacon_hdr_t);

    u_int32_t rx_clear = 0, rx_frame = 0, tx_frame = 0;
    u_int32_t show_cycles = 0;

    /*
    ** Code Begins
    */

    /*
     * Check if the previous beacon has gone out.  If
     * not don't try to post another, skip this period
     * and wait for the next.  Missed beacons indicate
     * a problem and should not occur.  If we miss too
     * many consecutive beacons reset the device.
     */
   if (beaconPendingCount != 0) {
        show_cycles = ath_hal_getMibCycleCountsPct(ah, 
                      &rx_clear, &rx_frame, &tx_frame);

        sc->sc_bmisscount++;

        printk("%s: bmisscount = %d, show_cycles = %d, rx_clear = %d, rx_frame = %d, tx_frame = %d\n", __func__, sc->sc_bmisscount, show_cycles, rx_clear, rx_frame, tx_frame);
        
        /* XXX: doth needs the chanchange IE countdown decremented.
         *      We should consider adding a net80211 call to indicate
         *      a beacon miss so appropriate action could be taken
         *      (in that layer).
         */
        if (sc->sc_bmisscount < (BSTUCK_THRESH_PERVAP * sc->sc_nvaps)) {

            if (show_cycles && !tx_frame && (rx_clear >= 99)) {
                /* Meant to reduce PHY errors and potential false detects */
                if (sc->sc_toggle_immunity)
                    ath_hal_set_immunity(ah, AH_TRUE);

                sc->sc_noise++;
            }

            /* If the HW requires hang detection, notify caller
             * and detect in the tasklet context.
             */
            if (ATH_HANG_WAR_REQ(sc)) {
                sc->sc_hang_check = AH_TRUE;
                *needmark = 1;
                return;
            }

            if (sc->sc_noreset) {
                printk("%s: missed %u consecutive beacons\n",
                       __func__, sc->sc_bmisscount);
                if (show_cycles) {
                    /*
                     * Display cycle counter stats from HAL to
                     * aide in debug of stickiness.
                     */
                    printk("%s: busy times: rx_clear=%d, rx_frame=%d, tx_frame=%d\n", __func__, rx_clear, rx_frame, tx_frame);
                } else {
                    printk("%s: unable to obtain busy times\n", __func__);
                }
            } else {
                DPRINTF(sc, ATH_DEBUG_BEACON_PROC,
                        "%s: missed %u consecutive beacons\n",
                        __func__, sc->sc_bmisscount);
            }
        } else if (sc->sc_bmisscount >= (BSTUCK_THRESH_PERVAP * sc->sc_nvaps)) {
            if (sc->sc_noreset) {
                if (sc->sc_bmisscount == (BSTUCK_THRESH_PERVAP * sc->sc_nvaps)) {
                    printk("%s: beacon is officially stuck\n",
                           __func__);
                    ath_hal_dmaRegDump(ah);
                }
            } else {
                DPRINTF(sc, ATH_DEBUG_BEACON_PROC,
                        "%s: beacon is officially stuck\n",
                        __func__);

                if (show_cycles && rx_clear >= 99) {
                    sc->sc_hang_war |= HAL_BB_HANG_DETECTED;

                    if (rx_frame >= 99) {
                        printk("Busy environment detected\n");
                    } else {
                        printk("Interference detected\n");
                    }

                    printk("rx_clear=%d, rx_frame=%d, tx_frame=%d\n",
                           rx_clear, rx_frame, tx_frame);

                    sc->sc_noise = 0;
                }
                *needmark = 1;
            }
        }

		return;
   }

    if (sc->sc_toggle_immunity)
        ath_hal_set_immunity(ah, AH_FALSE);
    sc->sc_noise = 0;
    sc->sc_hang_check = AH_FALSE;

    if (sc->sc_bmisscount != 0)
    {
        if (sc->sc_noreset) {
            printk("%s: resume beacon xmit after %u misses\n",
                   __func__, sc->sc_bmisscount);
        } else {
            DPRINTF(sc, ATH_DEBUG_BEACON_PROC,
                    "%s: resume beacon xmit after %u misses\n",
                    __func__, sc->sc_bmisscount);
        }
        sc->sc_bmisscount = 0;
    }

    avp = sc->sc_vaps[if_id];
    //ASSERT(avp);
    if (avp == NULL)
    {
        DPRINTF(sc, ATH_DEBUG_ANY, "%s: avp == NULL\n", __func__);
        return;
    }
    
    if (avp->av_bcbuf == NULL)
    {
        DPRINTF(sc, ATH_DEBUG_ANY, "%s: avp=%pK av_bcbuf=%pK\n",
                __func__, avp, avp != NULL ? avp->av_bcbuf : NULL);

        return;
    }
    bf = avp->av_bcbuf;

	/*
	 * new allocate a tx beacon buffer 
	 */
	{
		wbuf_t wbuf_org;
		u_int8_t *src;
		u_int8_t *dst;

		wbuf_org = (wbuf_t)bf->bf_mpdu;
		wbuf	 = wbuf_alloc(sc->sc_osdev, WBUF_USB_TX,  wbuf_get_pktlen(wbuf_org));

		if (wbuf == NULL) {
			printk("%s : allocate wbuf fail for beacon\n", __func__);
			return;
		}
		else {
            struct ieee80211_frame *wh;

			/* copy data to new wbuf */
			dst = wbuf_header(wbuf); 
			src = wbuf_header(wbuf_org);

			memcpy(dst, src, wbuf_get_pktlen(wbuf_org));
			wbuf_set_pktlen(wbuf, wbuf_get_pktlen(wbuf_org));

            /* set timestamp to 0 */
            wh = (struct ieee80211_frame *)dst;
            *((u_int64_t *)(wh+1)) = 0;
		}
	}

    /* XXX: We don't do ATIM, so we don't need to update beacon contents */
#if 0
    wbuf_t wbuf;
    wbuf = (wbuf_t)bf->bf_mpdu;
    /*
     * Update dynamic beacon contents.  If this returns
     * non-zero then we need to remap the memory because
     * the beacon frame changed size (probably because
     * of the TIM bitmap).
     */
    /*
     * XXX: This function should be called from ath_beacon_tasklet,
     * which is in ISR context, thus use the locked version.
     * Better to have an assertion to verify that.
     */
    if (sc->sc_ieee_ops->update_beacon &&
        sc->sc_ieee_ops->update_beacon(ni, &avp->av_boff, wbuf, 0) == 0)
    {
        wbuf_unmap_single(sc->sc_osdev, wbuf,
                          BUS_DMA_TODEVICE,
                          OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));
        bf->bf_buf_addr = wbuf_map_single(sc->sc_osdev, wbuf,
                                          BUS_DMA_TO_DEVICE,
                                          OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));
    }
#endif

#if 0
    // Don't setup tx descriptor in host side
    /*
     * Construct tx descriptor.
     */
    ath_beacon_setup(sc, avp, bf);

    /* NB: caller is known to have already stopped tx dma */
    ath_hal_puttxbuf(ah, sc->sc_bhalq, bf->bf_daddr);
    ath_hal_txstart(ah, sc->sc_bhalq);
#else
    /* Check for headroom size */
    if (wbuf_hdrspace(wbuf) < need_headroom)
    {
        wbuf = wbuf_realloc_hdrspace(wbuf, need_headroom);

        if (wbuf == NULL) {
            printk("%s: Insufficient header room space and allocate new buffer fail I\n", __func__);
            return;
        }
    }

#if 0
/* Dump Data content */
	{
		u_int8_t *p;
		u_int32_t ii;
        p = wbuf->data;
		for(ii = 0; ii < wbuf->len; ii++)
		{
			printk("%02x ", p[ii]);

			if ((ii & 0xf) == 0xf)
				printk("\n");
		}

		printk("\n");
	}
#endif

    anbdata = wbuf_push(wbuf, sizeof(ath_beacon_hdr_t));
    bchdr = (ath_beacon_hdr_t *)anbdata;

	bchdr->len_changed  = 0;
    bchdr->vap_index	= if_id;

    status = HTCSendPkt(sc->sc_host_htc_handle, NULL, wbuf, sc->sc_beacon_ep);  // api parameters to be discussed, but notion is to send beacon packet to HTC
#ifdef  ATH_HTC_TX_SCHED
    if (qdf_unlikely((status != A_OK) )){
        printk("Freeing adhoc beacon pkt due to credit unavailibility \n");
        wbuf_free(wbuf);
        wbuf = NULL;


    }
#endif

#endif
}

void
ath_wmi_generc_timer(ath_dev_t dev, u_int32_t trigger_mask, u_int32_t thresh_mask, u_int32_t curr_tsf)
{
#ifdef ATH_GEN_TIMER
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    ath_gen_timer_isr(sc, trigger_mask, thresh_mask, curr_tsf);
#endif
}

void
ath_wmi_beacon(ath_dev_t dev, u_int64_t currentTsf, u_int8_t beaconPendingCount, u_int8_t update)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    int needmark = 0;

    /*
     * Software beacon alert--time to send a beacon.
     * Handle beacon transmission directly; deferring
     * this is too slow to meet timing constraints
     * under load.
     */
    if (sc->sc_opmode == HAL_M_IBSS && !sc->sc_hasveol)
		ath_htc_beacon_start_adhoc(sc, &needmark, 0, beaconPendingCount);
    else if (sc->sc_opmode == HAL_M_HOSTAP
#ifdef ATH_SUPPORT_P2P        
        || atomic_read(&sc->sc_nap_vaps_up)
#endif  
        ) {
        ath_htc_beacon_tasklet(sc, &needmark, currentTsf, beaconPendingCount,update);
    }

//    if (needmark) {
//        /* We have a beacon stuck */
//        if (((AH_TRUE == sc->sc_hang_check) && ath_hw_hang_check(sc)) ||
//            (!sc->sc_noreset && (sc->sc_bmisscount >= BSTUCK_THRESH))) {
//            printk("%s: --- Beacon Stuck ---\n", __func__);
//            
//            ATH_PS_WAKEUP(sc);
//            ath_bstuck_tasklet(sc);
//            ATH_CLEAR_HANGS(sc);
//            ATH_PS_SLEEP(sc);
//            return;
//        }
//    }
}

void
ath_wmi_bmiss(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    ath_bmiss_tasklet(sc);
}

int 
ath_htc_startrecv(struct ath_softc *sc)
{
    struct ath_hal *ah = sc->sc_ah;

    ath_hal_rxena(ah);          /* enable recv descriptors */

    ath_opmode_init(sc);        /* set filters, etc. */
    ath_hal_startpcurecv(ah, sc->sc_scanning);   /* re-enable PCU/DMA engine */

    return 0;
}

HAL_BOOL 
ath_htc_stoprecv(struct ath_softc *sc)
{
    HAL_BOOL stopped = AH_TRUE;
    struct ath_hal *ah = sc->sc_ah;

    ath_hal_setrxfilter(ah, 0);	/* clear recv filter */
    ath_wmi_stop_recv(sc);

    return stopped;
}

void
ath_usb_rx_cleanup(ath_dev_t dev)
{
	struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    /* cleanup rx descriptors */
    if (sc->sc_rxdma.dd_desc_len != 0)
        ath_descdma_cleanup(sc, &sc->sc_rxdma, &sc->sc_rxbuf);

    ATH_RXBUF_LOCK_DESTROY(sc);
    ATH_RXFLUSH_LOCK_DESTROY(sc);
}

void
ath_schedule_wmm_update(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    /* Schedule the thread for delay WMM parameters update */
    ath_usb_schedule_thread(sc, KEVENT_WMM_UPDATE);
}

void
ath_wmi_beacon_stuck(struct ath_softc *sc)
{
    ath_wmi_beacon_stuck_internal(sc);
}

void
ath_wmi_beacon_immunity(struct ath_softc *sc)
{
    struct ath_hal *ah = sc->sc_ah;

    ath_hal_set_immunity(ah, AH_FALSE);
}

void
ath_wmi_beacon_setslottime(struct ath_softc *sc)
{
    ath_setslottime(sc);        /* commit change to hardware */
}

void
ath_wmi_set_generic_timer(struct ath_softc *sc, int index, u_int32_t timer_next, u_int32_t timer_period, int mode)
{
    struct ath_wmi_generic_timer_info timer_info;
    u_int32_t cmd_status;

    timer_info.index = cpu_to_be32(index);
    timer_info.timer_next = cpu_to_be32(timer_next);
    timer_info.timer_period = cpu_to_be32(timer_period);
    timer_info.mode = cpu_to_be32(mode);

    wmi_cmd(sc->sc_host_wmi_handle, 
            WMI_GENERIC_TIMER_CMDID,
            (u_int8_t *) &timer_info,
            sizeof(struct ath_wmi_generic_timer_info), 
            (u_int8_t *) &cmd_status,
            sizeof(cmd_status),
            ATH_HTC_WMI_TIMEOUT_DEFAULT);
}

void
ath_wmi_pause_ctrl(ath_dev_t dev, ath_node_t node, u_int32_t pause_status)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_node *an = (struct ath_node *)node;
    u_int32_t cmd_status;
    u_int8_t node_index = an->an_sc->sc_ieee_ops->ath_htc_gettargetnodeid(an->an_sc->sc_ieee, an->an_node);
    struct ath_wmi_tgt_pause_info pause_data;

#ifdef ATH_SUPPORT_UAPSD
	if(pause_status)
    {
    	ATH_UAPSD_LOCK_IRQ(sc);
	    sc->sc_uapsd_pause_in_progress = AH_TRUE;
	    while (sc->sc_uapsd_trigger_in_progress) {
	        ATH_UAPSD_UNLOCK_IRQ(sc);
	        OS_SLEEP(10);
	        ATH_UAPSD_LOCK_IRQ(sc);
	    }
	    ATH_UAPSD_UNLOCK_IRQ(sc);
	} else {
		ATH_UAPSD_LOCK_IRQ(sc);
	    sc->sc_uapsd_pause_in_progress = AH_FALSE;
	    ATH_UAPSD_UNLOCK_IRQ(sc);
	}
#endif

	/*  update target side pause ctrl state */
    pause_data.nodeindex = cpu_to_be32(node_index);
    pause_data.pause_state = cpu_to_be32(pause_status);

    wmi_cmd(sc->sc_host_wmi_handle, 
            WMI_TGT_NODE_PAUSE_CMDID,
            (u_int8_t *) &pause_data,
            sizeof(struct ath_wmi_tgt_pause_info), 
            (u_int8_t *) &cmd_status,
            sizeof(cmd_status),
            ATH_HTC_WMI_TIMEOUT_DEFAULT);
}

void ath_htc_host_txep_init(struct ath_softc *sc)
{
    int ep;
    
    printk("%s: Epnum to Hardware Queue Mapping\n", __func__);	

    for (ep = 0; ep < NUM_TX_EP; ep++)
    {
        struct ath_txep *txep;

        txep = &sc->sc_txep[ep];
        txep->ep_num = ep;
#if (defined(MAGPIE_HIF_GMAC)  && !defined(GMAC_JUMBO_FRAME_SUPPORT ))
        txep->ep_depth = 0;
        txep->ep_max = 0;        
#endif		
        txep->hwq_num = sc->sc_ep2qnum[ep];
        txep->ep_linkbuf = NULL;
        ATH_TXEP_LOCK_INIT(txep);
        TXEP_TAILQ_INIT(txep);
        printk("%s:epnum=%d hqnum=%d\n", __func__, txep->ep_num,txep->hwq_num);
    }

    sc->sc_cabep               = &sc->sc_txep[sc->sc_osdev->cab_ep];
    sc->sc_beaconep            = &sc->sc_txep[sc->sc_osdev->beacon_ep];
    sc->sc_uapsdep             = &sc->sc_txep[sc->sc_osdev->uapsd_ep];
    sc->sc_ac2ep[WME_AC_BE]    = &sc->sc_txep[sc->sc_osdev->data_BE_ep];
    sc->sc_ac2ep[WME_AC_BK]    = &sc->sc_txep[sc->sc_osdev->data_BK_ep];
    sc->sc_ac2ep[WME_AC_VI]    = &sc->sc_txep[sc->sc_osdev->data_VI_ep];
    sc->sc_ac2ep[WME_AC_VO]    = &sc->sc_txep[sc->sc_osdev->data_VO_ep];

#if (defined(MAGPIE_HIF_GMAC)  && !defined(GMAC_JUMBO_FRAME_SUPPORT) )
    sc->sc_ac2ep[WME_AC_BE]->ep_max        = 110;
    sc->sc_ac2ep[WME_AC_BK]->ep_max        = 38;
    sc->sc_ac2ep[WME_AC_VI]->ep_max        = 154;
    sc->sc_ac2ep[WME_AC_VO]->ep_max        = 210;
#endif    
    //For management we dont have separate hq so 
    //we need to assign it to BK
    sc->sc_txep[sc->sc_osdev->mgmt_ep].hwq_num = sc->sc_txep[sc->sc_osdev->data_BK_ep].hwq_num;

   qdf_print(" osdev->beacon_ep%d ",sc->sc_osdev->beacon_ep);
   qdf_print(" osdev->cab_ep %d ", sc->sc_osdev->cab_ep);
   qdf_print(" osdev->uapsd_ep %d ",sc->sc_osdev->uapsd_ep);
   qdf_print(" osdev->mgmt_ep %d ",sc->sc_osdev->mgmt_ep);
   qdf_print(" osdev->data_BE_ep %d ",sc->sc_osdev->data_BE_ep);
   qdf_print(" osdev->data_BK_ep %d ",sc->sc_osdev->data_BK_ep);
   qdf_print(" osdev->data_VI_ep %d ",sc->sc_osdev->data_VI_ep);
   qdf_print(" osdev->data_VO_ep %d ",sc->sc_osdev->data_VO_ep);

    return ;	
}

#ifdef ATH_HTC_TX_SCHED
int 
ath_htc_tx_init(ath_dev_t dev, int nbufs) {

    struct ath_softc *sc = ATH_DEV_TO_SC(dev);


    sc->sc_ath_memtxbuf = ath_buffer_setup(sc,&sc->sc_txbuf, "tx", nbufs+1);
    if( sc->sc_ath_memtxbuf == NULL )
        return -1 ;
    /* XXX allocate beacon state together with vap */
    sc->sc_ath_membbuf  = ath_buffer_setup(sc, &sc->sc_bbuf, 
            "beacon", ATH_BCBUF);
    if (sc->sc_ath_membbuf == NULL ) {
        OS_FREE(sc->sc_ath_memtxbuf);
        return -1 ;
    }

    return 0;

}



static struct ath_buf * 
ath_buffer_setup(struct ath_softc *sc, ath_bufhead *head,
                    const char *name, int32_t nbuf) {
    /* allocate buffers */
    struct ath_buf * bf = NULL;
    struct ath_buf *firstbf = NULL;
   
    int32_t i, bsize;

    bsize = sizeof(struct ath_buf) * (nbuf);

    bf = (struct ath_buf *)OS_MALLOC(sc->sc_osdev, bsize, GFP_KERNEL);
    if (bf == NULL) {
        qdf_print("ath_buffer_setup buf allocation Fail ");
        return NULL ;
    }
    firstbf =bf;
    OS_MEMSET(bf, 0, bsize);
    TAILQ_INIT(head);

    for (i = 0; i < nbuf; i++, bf++) {
        TAILQ_INSERT_TAIL(head, bf, bf_list);
    }
    return firstbf ;


}

void ath_htc_uapsd_creditupdate_tasklet(void *data)
{
    struct ath_softc *sc = (struct ath_softc *)data;
    sc->sc_ieee_ops->ath_net80211_uapsd_creditupdate(sc->sc_ieee);    
}

void ath_htc_uapsd_credit_update(ath_dev_t dev, ath_node_t node)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_buf *bf = NULL;
    int i;
    int break_from_loop = 0;
    struct ieee80211_node *ni = NULL;
    struct ath_node *an = (struct ath_node *)node;
    ath_data_hdr_t *tfh = NULL; 

    ATH_HTC_UAPSD_LOCK(sc);
    if (an == NULL) {
        printk("%s an is NULL \n",__FUNCTION__);
        ATH_HTC_UAPSD_UNLOCK(sc);
        return;
    }    
    ni = (struct ieee80211_node *)an->an_node;

    for (i = 0; i < an->an_uapsd_qdepth; i++) {
        wbuf_t wbuf;

        bf = TAILQ_FIRST(&an->an_uapsd_q);

        wbuf = bf->bf_mpdu;
        tfh = (ath_data_hdr_t *)(wbuf_header(wbuf));
        if (tfh->flags & ATH_DH_FLAGS_EOSP) {
            break_from_loop = 1;        
        }
        if(ath_htc_send_pkt(dev, wbuf, sc->sc_uapsd_ep, an) != 0) {
            an->an_uapsd_qdepth -=  i;
            printk("%s unable to send UAPSD pkts,SW queue depth %d\n",__FUNCTION__,an->an_uapsd_qdepth);
            ieee80211node_set_flag(ni, IEEE80211_NODE_UAPSD_CREDIT_UPDATE);
            goto uapsd_queue;
        }
        
        TAILQ_REMOVE(&an->an_uapsd_q, bf, bf_list);

        /* reclaim ath_buf */
        bf->bf_mpdu = NULL;
        ATH_TXBUF_LOCK(sc);
        TAILQ_INSERT_TAIL(&sc->sc_txbuf, bf, bf_list);
        ATH_TXBUF_UNLOCK(sc);
        if (break_from_loop)
            break;
    }        
    ieee80211node_clear_flag(ni, IEEE80211_NODE_UAPSD_SP);
    ieee80211node_clear_flag(ni, IEEE80211_NODE_UAPSD_CREDIT_UPDATE);
    sc->sc_uapsd_trigger_in_progress = false;
    if (break_from_loop)
        an->an_uapsd_qdepth -=  (i + 1);
    else
        an->an_uapsd_qdepth -=  i;
uapsd_queue:
    ATH_HTC_UAPSD_UNLOCK(sc);
    return;
}

void ath_htc_tx_schedule(osdev_t osdev, int epid  )
{

    ATH_HTC_TXTQUEUE_SCHEDULE(osdev);

}
int 
ath_htc_tx_cleanup(ath_dev_t dev) {

    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    uint8_t cmd_status;


    wmi_cmd(sc->sc_host_wmi_handle,
                WMI_TGT_DETACH_CMDID,
                NULL,
                0,
                &cmd_status,
                sizeof(cmd_status),
                100);

    qdf_print(" < TGT DETACH CMD Executed > ");

    wmi_stop(sc->sc_host_wmi_handle);

    OS_FREE(sc->sc_ath_membbuf);

    /* when we need to free all desc resources, we need to insert
     * back the one hold in sc_txbuf_held to txbuf queue
     */
    OS_FREE(sc->sc_ath_memtxbuf);

    sc->sc_ath_memtxbuf = sc->sc_ath_membbuf = NULL;
    qdf_print("Buffer FRee Done...");
    return 0 ;
}

void
ath_txep_pause_tid(struct ath_softc *sc, ath_atx_tid_t *tid)
{
//    __stats(sc, tx_tidpaused);

    tid->paused = 1 ;
    
}

void
ath_txep_resume_tid(struct ath_softc *sc, ath_atx_tid_t *tid)
{
    struct ath_txep  *txep = ATH_TID_2_TXEP(sc, tid->tidno);

    tid->paused = 0 ;

    if (TAILQ_EMPTY(&tid->buf_q))
        return;

    ATH_TXEP_LOCK_BH(txep);
    ath_txep_enqueue(txep, tid->an, tid->tidno,1);
    ath_host_txep_schedule(sc, txep);
    ATH_TXEP_UNLOCK_BH(txep);
}

/* Returns 0 : Packets to be Queued
 * Returns 1 : Packets can be Xmitted
 */
static uint16_t
ath_htc_can_xmit(struct ath_softc *sc, uint16_t ac)
{
    struct ath_txep  *txepBE, *txepBK, *txepVO, *txepVI;
    int bke, bee, vie, voe;

    txepBE = sc->sc_ac2ep[WME_AC_BE];
    txepBK = sc->sc_ac2ep[WME_AC_BK];
    txepVI = sc->sc_ac2ep[WME_AC_VI];
    txepVO = sc->sc_ac2ep[WME_AC_VO];

    bke = TAILQ_EMPTY(&txepBK->ep_acq);
    bee = TAILQ_EMPTY(&txepBE->ep_acq);
    vie = TAILQ_EMPTY(&txepVI->ep_acq);
    voe = TAILQ_EMPTY(&txepVO->ep_acq);

    switch(ac)
    {
        case WME_AC_BK:
            return ( bke && bee && vie && voe );
        case WME_AC_BE:
            return ( bee && vie && voe );
        case WME_AC_VI:
            return ( vie && voe );
        case WME_AC_VO:
            return voe;
        default:
            qdf_assert(0);
            return 0;
    }
}

static void
ath_htc_txep_queue(struct ath_softc *sc,wbuf_t  wbuf ,struct ath_node *an,
        u_int8_t  tidno,u_int8_t epid)
{
    struct ath_buf          *bf;
    ath_atx_tid_t         *tid;
    struct ath_txep * txep ;

    txep = &sc->sc_txep[epid];


    bf = ath_host_buf_alloc(sc, wbuf_get_priority(wbuf));
    if(bf == NULL ){
        wbuf_free(wbuf);
        return ;
    }

    bf->bf_mpdu   = wbuf;
    bf->bf_node  = (struct ieee80211_node * )an ;
    
    txep->ep_depth++;

    tid   = ATH_AN_2_TID( an, tidno);

    
    ATH_TXEP_LOCK_BH(txep);
    
    TAILQ_INSERT_TAIL(&tid->buf_q, bf, bf_list);
    ath_txep_enqueue(txep, an, tidno,1);

    
    ATH_TXEP_UNLOCK_BH(txep);
    
    return ;
}
static void
ath_txep_enqueue(struct ath_txep *txep, struct ath_node *an, int32_t tidno,
        int switch_tid )
{
    struct ath_atx_tid  *tid;
    struct ath_atx_ac   *ac;

    tid = ATH_AN_2_TID(an, tidno);
    ac  = tid->ac;

    /*
     * if tid is paused, hold off
     */
    if (tid->paused)
        return;
    /*
     * add tid to ac atmost once
     */
    if (tid->sched)
        return;

    tid->sched = AH_TRUE;

    if(switch_tid)
        TAILQ_INSERT_TAIL(&ac->tid_q, tid, tid_qelem);
    else
        TAILQ_INSERT_HEAD(&ac->tid_q, tid,tid_qelem);

    /*
     * add node ac to txq atmost once
     */
    if (ac->sched)
        return;

    ac->sched = AH_TRUE;
    if(switch_tid)
        TAILQ_INSERT_TAIL(&txep->ep_acq, ac, ac_qelem);
    else
        TAILQ_INSERT_HEAD(&txep->ep_acq, ac,ac_qelem)       ;

}

static int
ath_tx_tid_host_sched(struct ath_softc *sc, ath_atx_tid_t * tid,
        struct ath_txep *txep)
{
    struct ath_buf  *bf;
    A_STATUS  status    = A_OK;

    do{
        bf = TAILQ_FIRST(&tid->buf_q);
        if(bf == NULL){
            tid->tid_buf_cnt = ATH_MAX_TID_FLUSH;
            return ATH_STATUS_TID_EMPTY;
        }

        status = HTCSendPkt(sc->sc_host_htc_handle, NULL, bf->bf_mpdu, 
                txep->ep_num);

#ifdef HTC_HOST_CREDIT_DIST
        if (status == A_ERROR || status == A_CREDIT_UNAVAILABLE)
#else
            if (status == A_ERROR)
#endif
                break;
            else {
                TAILQ_REMOVE(&tid->buf_q,bf,bf_list);
                ATH_TXBUF_LOCK(sc);
                txep->ep_depth --;
                ATH_TXBUF_UNLOCK(sc);         
                ath_host_buf_free(sc,bf);
            }

        tid->tid_buf_cnt--;

        if(tid->tid_buf_cnt <= 0){
            tid->tid_buf_cnt=ATH_MAX_TID_FLUSH;
            return ATH_STATUS_TID_MAXFLUSH;
        }
    }while( 1 );

    return ATH_STATUS_HTC_TXEPQ_FULL ;
}



void
ath_htc_tid_cleanup(struct ath_softc *sc, struct ath_atx_tid *tid)
{
    struct ath_buf  *bf = NULL;


    while (!TAILQ_EMPTY(&tid->buf_q)) {
        TAILQ_DEQ(&tid->buf_q, bf, bf_list);
        wbuf_free(bf->bf_mpdu);
        ath_host_buf_free(sc, bf);
    }

}
/*
 * Drain all pending buffers
 */
static void
ath_txep_drain(struct ath_softc *sc, struct ath_txep *txep)
{
    struct ath_atx_ac   *ac = NULL;
    struct ath_atx_tid  *tid = NULL;

    while (!TAILQ_EMPTY(&txep->ep_acq)) {
        TAILQ_DEQ(&txep->ep_acq, ac, ac_qelem);
        ac->sched = AH_FALSE;

        while (!TAILQ_EMPTY(&ac->tid_q)) {
            TAILQ_DEQ(&ac->tid_q, tid, tid_qelem);
            tid->sched = AH_FALSE;

            ath_htc_tid_cleanup(sc, tid);

        }
    }
}


void
ath_htc_txep_drain(struct ath_softc *sc, struct ath_txep *txep)
{
    ath_txep_drain(sc, txep);
}




static struct ath_buf *
ath_host_buf_alloc(struct ath_softc *sc, uint8_t ac)
{
    struct ath_buf *bf = NULL;


    ATH_TXBUF_LOCK(sc);

    if(sc->sc_devstopped == 1){
        ATH_TXBUF_UNLOCK(sc);
        qdf_print("Error should not come in as dev stopped ");
        return NULL;
    }


    bf = TAILQ_FIRST(&sc->sc_txbuf);
    if(bf == NULL ){
        ATH_TXBUF_UNLOCK(sc);
        /*qdf_print("Bf null detected  ******* ");*/
        return bf;
    }
    TAILQ_REMOVE(&sc->sc_txbuf, bf, bf_list);

    /*
     * stop higher layers if no more buffers
     */
    ATH_TXBUF_UNLOCK(sc);

    return bf;
}
static void
ath_host_buf_free(struct ath_softc *sc, struct ath_buf *bf)
{
    bf->bf_mpdu  = NULL;
    bf->bf_node = NULL;
    ATH_TXBUF_LOCK(sc);
 
    if (bf != NULL) {
        TAILQ_INSERT_TAIL(&sc->sc_txbuf, bf, bf_list);
    }
    ATH_TXBUF_UNLOCK(sc);

}




/*
    * Deferred processing of transmit interrupt.
     */
int32_t priority_ac[4] = {3,2,0,1};
void
ath_htc_tx_tasklet(void * data)
{

    struct ath_softc *sc = (struct ath_softc *)data;

    int32_t i;
    struct ath_txep *txep;

    /* Host_Magpie_shceduler : do we need to add code for uapsd and cab */
    for (i = 0; i < 4 ; i++) {

        txep = sc->sc_ac2ep[priority_ac[i]] ;

        ATH_TXEP_LOCK_BH(txep);
        ath_host_txep_schedule(sc, txep);
        ATH_TXEP_UNLOCK_BH(txep);
    }

}
static int
ath_host_txep_schedule(struct ath_softc *sc, struct ath_txep *txep)
{


    struct ath_atx_ac   *ac;
    struct ath_atx_tid  *tid;
    uint8_t tid_switch = 1;
    int status =1;

    /*
     * check if nothing to schedule
     */
    if (TAILQ_EMPTY(&txep->ep_acq)) {
        return 0;
    }

    /*
     * dequeue first node/ac entry on the queue
     */
    TAILQ_DEQ(&txep->ep_acq, ac, ac_qelem);

    ac->sched = AH_FALSE;

    /* Process each pending tid until we process one TID */
    do {
        /*
         * process a single tid per destination
         */
        TAILQ_DEQ(&ac->tid_q, tid, tid_qelem);
        if(tid == NULL){
            break ;
        }
        tid->sched = AH_FALSE;
        if (tid->paused) {
            /* Keep the hw busy, check next tid */
            continue;
        }

        status = ath_tx_tid_host_sched(sc, tid,txep);

        if(status == ATH_STATUS_HTC_TXEPQ_FULL  )
            tid_switch=0;
        else
            tid_switch = 1;
        if(status != ATH_STATUS_TID_EMPTY)
            ath_txep_enqueue(txep, tid->an, tid->tidno,tid_switch);


    } while(!TAILQ_EMPTY(&ac->tid_q) && (status != ATH_STATUS_HTC_TXEPQ_FULL));

    /*
     * schedule AC if more TIDs need processing
     */
    if (!TAILQ_EMPTY(&ac->tid_q)) {
        /*
         * add dest ac to txq if not already added
         */

        if (ac->sched == AH_FALSE) {
            ac->sched = AH_TRUE;

            TAILQ_INSERT_TAIL(&txep->ep_acq, ac, ac_qelem);
        }
    }

    if ( status == ATH_STATUS_HTC_TXEPQ_FULL ) {
        return 1;
    } else {
        return 0;
    }

}

/*
 * Cleanupthe pending buffers for the node. 
 */
void
ath_htc_tx_node_cleanup(struct ath_softc *sc, struct ath_node *an)
{
    int i, tidno;
    struct ath_atx_ac *ac;
    struct ath_atx_tid *tid, *next_tid;
    struct ath_txep *txep;
    
    for (i = 0; i < WME_NUM_AC ; i++) {
	    txep = sc->sc_ac2ep[priority_ac[i]] ;
	    ATH_TXEP_LOCK_BH(txep);
	    TAILQ_FOREACH(ac,&txep->ep_acq,ac_qelem) {
		    tid = TAILQ_FIRST(&ac->tid_q);
		    if (tid && tid->an != an) {
			    continue;
		    }
		    TAILQ_REMOVE(&txep->ep_acq, ac, ac_qelem);
		    ac->sched = AH_FALSE;

		    TAILQ_FOREACH_SAFE(tid, &ac->tid_q, tid_qelem, next_tid) {
			    TAILQ_REMOVE(&ac->tid_q, tid, tid_qelem);
			    tid->sched = AH_FALSE;
			    /* stop ADDBA request timer (if ADDBA in progress) */
			    if (cmpxchg(&tid->addba_exchangeinprogress, 1, 0) == 1) {
				    ath_cancel_timer(&tid->addba_requesttimer, CANCEL_NO_SLEEP);
				    /* Tid is paused - resume the tid */
				    tid->paused--;
				    __11nstats(sc, tx_tidresumed);
			    }
			    ath_htc_tid_cleanup(sc,tid);
		    }
	    }

	    ATH_TXEP_UNLOCK_BH(txep);
    }
    /* Free the unscheduled tid buffer queue for this node */
    for (tidno = 0, tid = &an->an_tx_tid[tidno]; tidno < WME_NUM_TID; 
		    tidno++, tid++) {
	    txep = sc->sc_ac2ep[ TID_TO_WME_AC(tidno)];
	    ATH_TXEP_LOCK_BH(txep);
	    /* stop ADDBA request timer (if ADDBA in progress) */
	    if (cmpxchg(&tid->addba_exchangeinprogress, 1, 0) == 1) {
		    ath_cancel_timer(&tid->addba_requesttimer, CANCEL_NO_SLEEP);
		    /* Tid is paused - resume the tid */
		    tid->paused--;
		    __11nstats(sc, tx_tidresumed);
	    }
	    ath_htc_tid_cleanup(sc,tid);
	    ATH_TXEP_UNLOCK_BH(txep);
    }

    for (tidno = 0, tid = &an->an_tx_tid[tidno]; tidno < WME_NUM_TID;
         tidno++, tid++) {
            ath_free_timer(&tid->addba_requesttimer);
    }

}


#endif

void
ath_wmi_set_btcoex(struct ath_softc *sc, u_int8_t dutyCycle, u_int16_t period,  u_int8_t stompType)
{
    u_int32_t cmd_status = -1;
    struct {
	    uint8_t Scheme;
	    uint8_t DutyCycle;
	    uint16_t Period;
	    uint8_t  StompType;
        uint32_t BtMode;
    } bt_3wire;	


    bt_3wire.Scheme = 3;
    bt_3wire.DutyCycle = dutyCycle;
    bt_3wire.Period = period;
    bt_3wire.StompType = stompType;
    bt_3wire.BtMode = 0;

    bt_3wire.Period = cpu_to_be16(bt_3wire.Period);
    bt_3wire.BtMode = cpu_to_be32(bt_3wire.BtMode);

    cmd_status = wmi_cmd(sc->sc_host_wmi_handle, 
        WMI_BT_COEX_CMDID, 
        (u_int8_t *) &bt_3wire, 
        12,
        (u_int8_t *)&cmd_status,
        sizeof(cmd_status),
        100 /* timeout unit? */);

    if(cmd_status) {
        printk("Host : WMI COMMAND READ FAILURE stat = %x\n", cmd_status);
        return ; /*TODO: return value is for read reg value result, not for read reg status result, need enhence later*/
    }

    return;
}
#ifdef ATH_HTC_MII_RXIN_TASKLET
u_int8_t ath_initialize_timer_rxtask  (osdev_t                osdev,
                                       ath_rxtimer *         timer_object, 
                                       u_int32_t              timer_period, 
                                       timer_handler_function timer_handler, 
                                       void*                  context)
{
    init_timer(timer_object);
    timer_object->function = (dummy_timer_func_t)timer_handler;
    timer_object->data = (unsigned long)(context);
    return 0;

}
void ath_set_timer_period_rxtask(ath_rxtimer* timer_object, u_int32_t timer_period)
{

     mod_timer( timer_object, jiffies +  timer_period*HZ/1000);
}

u_int8_t ath_start_timer_rxtask (ath_rxtimer* timer_object)
{
    return 0;
}
u_int8_t ath_cancel_timer_rxtask(ath_rxtimer* timer_object, enum timer_flags flags)
{
    return del_timer(timer_object);

}
u_int8_t ath_timer_is_active_rxtask(ath_rxtimer* timer_object)
{
    return timer_pending(timer_object);

}

void ath_free_timer_rxtask(ath_rxtimer* timer_object)
{
    return;
}
#endif

#endif

