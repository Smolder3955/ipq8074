/*
 * Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * Copyright (c) 2009, Atheros Communications Inc.
 * All Rights Reserved.
 */

/*
 *  Implementation of receive path in atheros OS-independent layer.
 */

/*
 * Definitions for the ATH TxBF layer. private header file
 */
#ifndef ATH_TXBF_H
#define ATH_TXBF_H


#ifdef ATH_SUPPORT_TxBF

#define LOWEST_RATE_RESET_PERIOD (10*60*1000) /* ten minutes in ms*/

int  ath_get_txbfcaps(ath_dev_t dev, ieee80211_txbf_caps_t **txbf_caps);
HAL_BOOL ath_txbf_alloc_key(ath_dev_t dev, u_int8_t *mac, u_int16_t *keyix);
void ath_txbf_set_key (ath_dev_t dev,u_int16_t keyidx,u_int8_t rx_staggered_sounding,u_int8_t channel_estimation_cap
                ,u_int8_t MMSS);
void ath_txbf_del_key (ath_dev_t dev,u_int16_t keyidx,int freeslot);
void ath_txbf_set_hw_cvtimeout(ath_dev_t dev, HAL_BOOL opt);
void ath_txbf_print_cv_cache(ath_dev_t dev);
void ath_txbf_get_cvcache_nr(ath_dev_t dev, u_int16_t keyidx, u_int8_t *nr);
void ath_txbf_set_rpt_received(ath_node_t node);
bool ath_txbf_chk_rpt_frm(struct ieee80211_frame *wh);
#endif /* ATH_SUPPORT_TxBF */

#endif /* ATH_TXBF_H */
