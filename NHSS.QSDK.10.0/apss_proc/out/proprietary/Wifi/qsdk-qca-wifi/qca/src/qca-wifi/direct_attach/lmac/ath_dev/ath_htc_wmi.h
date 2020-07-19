/*
 * Copyright (c) 2011, 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */
/*
 * Copyright (c) 2010, Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */

/*
 * Definitions for the HTC module
 */
#ifndef ATH_HTC_WMI_H
#define ATH_HTC_WMI_H

#define ath_wmi_beacon(dev, beaconPendingCount, update)
#define ath_wmi_add_vap(dev, target, size)
#define ath_wmi_add_node(dev, target, size)
#define ath_wmi_delete_node(dev, target, size)
#define ath_wmi_ic_update_target(dev, target, size)
#define ath_htc_rx(dev, wbuf)
#define ath_htc_open(dev, initial_chan)
#define ath_htc_initialize_target_q(sc)
#define ath_htc_host_txep_init(sc)
#define ath_htc_rx_init(dev, nbufs)
#define ath_htc_tx_init(dev, nbufs)
#define host_htc_sched_next(dev, nbufs)
#define ath_htc_tx_cleanup(dev)
#define ath_wmi_intr_disable(sc, mask)
#define ath_wmi_abort_txq(sc, fastcc)
#define ath_wmi_rx_link(sc)
#define ath_wmi_drain_txq(sc)
#define ath_wmi_stop_recv(sc)
#define ath_wmi_start_recv(sc)
#define ath_wmi_intr_enable(sc, mask)
#define ath_wmi_set_mode(sc, phymode)
#define ath_wmi_rc_node_update(dev, an, isnew, capflag, rates, htrates);
#define ath_wmi_aborttxep(sc)                   (AH_TRUE)
#define ath_wmi_abortrecv(sc)                   (AH_TRUE)
#define ath_htc_tx_start(dev, wbuf, txctl)      (0)
#define ath_htc_tx_tasklet                      (0)
#define ath_htc_uapsd_creditupdate_tasklet      (0)
#define ath_htc_uapsd_credit_update             (0)
#define ath_htc_tasklet                         (0)
#define ath_htc_rx_tasklet(dev, flush)          (0)
#define ath_wmi_aggr_enable(dev, an, tidno, aggr_enable)
#define ath_htc_startrecv(sc)                   (0)
#define ath_htc_stoprecv(sc)                    (AH_TRUE)
#define ath_htc_txq_update(dev, qnum, qi)
#define ath_wmi_delete_vap(dev, target, size)
#define ath_wmi_set_desc_tpc(dev, enable)
#define ath_wmi_update_vap(dev, target, size)   (0)
#ifdef ATH_SUPPORT_UAPSD
#define ath_htc_process_uapsd_trigger(dev, node, maxsp, ac, flush, maxqdepth)
#define ath_htc_tx_uapsd_node_cleanup(sc, an)
#endif
#define ath_wmi_generc_timer(dev, trigger_mask, thresh_mask)
#define ath_wmi_set_generic_timer(sc, index, timer_next, timer_period, mode)
#define ath_wmi_pause_ctrl(dev, an, pause_status)

#define ATH_HTC_SET_CALLBACK_TABLE(_ath_ar_ops)
#define ATH_TXEP_LOCK_INIT(_ep)      
#define TXEP_TAILQ_INIT(_txep) 


#define ATH_HTC_SET_HANDLE(_sc, _osdev)
#define ATH_HTC_SET_EPID(_sc, _osdev)
#define ATH_HTC_SETMODE(_sc, _mode)
#define ATH_HTC_TXRX_STOP(_sc, _fastcc)
#define ATH_HTC_SET_BCNQNUM(_sc, _qnum)
#define ATH_HTC_DRAINTXQ(_sc)
#define ATH_HTC_ABORTTXQ(_sc)

#endif /* ATH_HTC_WMI_H */

