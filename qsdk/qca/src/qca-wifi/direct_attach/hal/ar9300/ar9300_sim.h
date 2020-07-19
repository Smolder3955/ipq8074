/*
* Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
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
 
#ifndef _ATH_AR9300_SIM_H_
#define _ATH_AR9300_SIM_H_

#if ATH_DRIVER_SIM

#include "ah.h"
#include "ar9300reg.h"

struct ath_hal_9300_sim_key_cache {
    u_int32_t key0;
    u_int32_t key1;
    u_int32_t key2;
    u_int32_t key3;
    u_int32_t key4;
    u_int32_t key_type;
    u_int32_t mac0;
    u_int32_t mac1_valid;
} __packed;

struct ath_hal_9300_sim {
    u_int32_t rtc_reset;
    u_int32_t cr;
    
    u_int32_t rx_buffer_size;
    
    u_int32_t rxdp[HAL_NUM_RX_QUEUES][128]; // larger than needed for the HP queue - but testing is done within the code for index violations
    void*     rxdp_wbuf[HAL_NUM_RX_QUEUES][128];
    u_int32_t rxdp_insert_here[HAL_NUM_RX_QUEUES];
    u_int32_t rxdp_remove_here[HAL_NUM_RX_QUEUES];
    u_int32_t rxdp_num_entries[HAL_NUM_RX_QUEUES];

    u_int32_t txdp_enable;
    u_int32_t txdp_disable;
    u_int32_t txdp_status[HAL_NUM_TX_QUEUES];
    u_int32_t txdp[HAL_NUM_TX_QUEUES][HAL_TXFIFO_DEPTH];
    void*     txdp_ath_buf[HAL_NUM_TX_QUEUES][HAL_TXFIFO_DEPTH];
    u_int32_t txdp_insert_here[HAL_NUM_TX_QUEUES];
    u_int32_t txdp_remove_here[HAL_NUM_TX_QUEUES];
    u_int32_t txdp_num_entries[HAL_NUM_TX_QUEUES];
    
    u_int32_t txs_ring_start;
    u_int32_t txs_ring_end;
    u_int32_t txs_current;

    u_int32_t sta_addr_l32;
    u_int32_t sta_addr_u16; /* note that contains other flags! */
    u_int8_t sta_addr[6];
    u_int32_t bssid_l32;
    u_int32_t bssid_u16; /* note that contains other flags! */
    u_int8_t bssid[6];

    u_int32_t rx_filter;
    u_int32_t mcast_filter_l32;
    u_int32_t mcast_filter_u32;
    u_int32_t sta_mask_l32;
    u_int32_t sta_mask_u16;
    u_int8_t sta_mask[6];

    struct ath_hal_9300_sim_key_cache key_cache[AR_KEY_CACHE_SIZE];

    u_int32_t next_swba;
    u_int32_t swba_period;
    u_int32_t timer_mode;

    u_int32_t dma_txcfg;

    /* beacon specific */
    spinlock_t beacon_timer_lock;    
    os_timer_t beacon_timer;
    bool beacon_timer_active;
}; 

#define AR9300_BEACON_TIMER_LOCK_INIT(__ah9300)     spin_lock_init(&(__ah9300)->beacon_timer_lock) 
#define AR9300_BEACON_TIMER_LOCK_DESTROY(__ah9300)  spin_lock_destroy(&(__ah9300)->beacon_timer_lock)
#define AR9300_BEACON_TIMER_LOCK(__ah9300)          spin_lock(&(__ah9300)->beacon_timer_lock)
#define AR9300_BEACON_TIMER_UNLOCK(__ah9300)        spin_unlock(&(__ah9300)->beacon_timer_lock)

bool AHSIM_AR9300_attach(struct ath_hal *ah);
void AHSIM_AR9300_detach(struct ath_hal *ah);

u_int32_t AHSIM_AR9300_access_sim_reg(struct ath_hal *ah, u_int32_t reg, u_int32_t val, bool read_access);

u_int32_t AHSIM_AR9300_get_active_tx_queue(struct ath_hal *ah);
u_int32_t AHSIM_AR9300_transmit_packet(struct ath_hal *src_ah, u_int8_t* alt_src, struct ath_hal *dest_ah, u_int32_t q, u_int64_t* ba_mask, bool *is_unicast_for_me);
void AHSIM_AR9300_done_transmit(struct ath_hal *ah, u_int32_t q, u_int32_t num_agg_pkts, u_int64_t ba_mask);
bool AHSIM_AR9300_record_tx_c(struct ath_hal* ah, u_int32_t q, bool intersim);
void AHSIM_AR9300_rx_dummy_pkt(struct ath_hal* ah, u_int8_t* dummy_pkt, u_int32_t dummy_pkt_bytes);
u_int8_t* AHSIM_AR9300_get_tx_buffer(struct ath_hal* ah, u_int32_t q, u_int32_t* frag_length);
void AHSIM_AR9300_WriteIntersimBuffer(struct ath_hal *ah, u_int32_t q);

#endif // ATH_DRIVER_SIM

#endif // _ATH_AR9300_SIM_H_
