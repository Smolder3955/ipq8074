/*
 * Copyright (c) 2010, Atheros Communications Inc.
 * All Rights Reserved.
 *
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 */

#include "wbuf.h"

#if ATH_DRIVER_SIM

#include "ar9300.h"
#include "ar9300reg.h"
#include "ar9300desc.h"
#include "ah_sim.h"
#include "ar9300_sim.h"

extern  void AHSIM_TriggerInterrupt(HAL_SOFTC *sc, u_int32_t flags);
extern  void* AHSIM_GetRxWbuf(HAL_SOFTC *sc, u_int32_t dest_paddr,
        HAL_RX_QUEUE queue_type);
extern  void* AHSIM_GetTxAthBuf(HAL_SOFTC *sc, u_int32_t q,
        u_int32_t paddr);
extern  void* AHSIM_AthBufToTxcDesc(void* ath_buf_in);
extern  void* AHSIM_GetTxBufferVirtualAddress(void* ath_buf_ptr, void* paddr);
extern  void* AHSIM_GetNextTxBuffer(void* ath_buf_ptr, void* paddr);
extern  void* AHSIM_GetTxStatusDescriptor(HAL_SOFTC *sc, u_int32_t paddr);
extern  wbuf_t AHSIM_GetTxBuffer(void* ath_buf_ptr);

#define FAKE_RSSI_STRENGTH_GOOD 0x60 /* 0x80 is the 'invalid' value */
#define FAKE_RSSI_STRENGTH_BAD 0x10 /* 0x80 is the 'invalid' value */

static int64_t
GET_TSF()
{
    LARGE_INTEGER freq;
    LARGE_INTEGER counter = KeQueryPerformanceCounter(&freq);
    int64_t var64;

    /* want to split integer divisions to reduce rounding errors */
    var64 = freq.QuadPart / 10000;
    var64 = counter.QuadPart / var64;
    var64 = var64 * 100;

    return var64;
}

static INLINE
OS_TIMER_FUNC(ah9300_beacon_tx_callback)
{
    struct ath_hal *ah;
    struct ath_hal_9300_sim* hal_9300_sim;
    int needmark = 0;

    OS_GET_TIMER_ARG(ah, struct ath_hal *);
    hal_9300_sim = &AH9300(ah)->hal_9300_sim;

    if (hal_9300_sim->swba_period) {
        AR9300_BEACON_TIMER_LOCK(hal_9300_sim);
        OS_SET_TIMER(
            &hal_9300_sim->beacon_timer, hal_9300_sim->swba_period / 1000);
        AR9300_BEACON_TIMER_UNLOCK(hal_9300_sim);
    } else {
        hal_9300_sim->beacon_timer_active = false;
    }

    AHSIM_TriggerInterrupt(AH_PRIVATE(ah)->ah_sc, HAL_INT_SWBA);
}

bool AHSIM_AR9300_attach(struct ath_hal *ah)
{
    struct ath_hal_sim *hal_sim = &AH_PRIVATE(ah)->ah_hal_sim;
    struct ath_hal_9300_sim* hal_9300_sim = &AH9300(ah)->hal_9300_sim;

    AHSIM_REG_LOCK_INIT(hal_sim);

    hal_sim->ahsim_access_sim_reg = AHSIM_AR9300_access_sim_reg;
    hal_sim->ahsim_get_active_tx_queue = AHSIM_AR9300_get_active_tx_queue;
    hal_sim->ahsim_distribute_transmit_packet = AHSIM_AR9300_transmit_packet;
    hal_sim->ahsim_done_transmit = AHSIM_AR9300_done_transmit;
    hal_sim->ahsim_record_tx_c = AHSIM_AR9300_record_tx_c;
    hal_sim->ahsim_rx_dummy_pkt = AHSIM_AR9300_rx_dummy_pkt;
    hal_sim->ahsim_get_tx_buffer = AHSIM_AR9300_get_tx_buffer;

    AR9300_BEACON_TIMER_LOCK_INIT(hal_9300_sim);
    OS_INIT_TIMER(AH_PRIVATE(ah)->ah_osdev,
        &hal_9300_sim->beacon_timer, ah9300_beacon_tx_callback, ah, QDF_TIMER_TYPE_WAKE_APPS);
    hal_9300_sim->beacon_timer_active = false;

    hal_9300_sim->timer_mode = 0;

    return true;
}

void AHSIM_AR9300_detach(struct ath_hal *ah)
{
    struct ath_hal_sim *hal_sim = &AH_PRIVATE(ah)->ah_hal_sim;
    struct ath_hal_9300_sim* hal_9300_sim = &AH9300(ah)->hal_9300_sim;

    AR9300_BEACON_TIMER_LOCK(hal_9300_sim);
    if (hal_9300_sim->beacon_timer_active == true) {
        OS_CANCEL_TIMER(&hal_9300_sim->beacon_timer);
        hal_9300_sim->beacon_timer_active = false;
    }
    AR9300_BEACON_TIMER_UNLOCK(hal_9300_sim);

    hal_sim->ahsim_access_sim_reg = NULL;
    hal_sim->ahsim_get_active_tx_queue = NULL;
    hal_sim->ahsim_distribute_transmit_packet = NULL;
    hal_sim->ahsim_done_transmit = NULL;
    hal_sim->ahsim_record_tx_c = NULL;
    hal_sim->ahsim_rx_dummy_pkt = NULL;
    hal_sim->ahsim_get_tx_buffer = NULL;

    OS_FREE_TIMER(&hal_9300_sim->beacon_timer);
    AR9300_BEACON_TIMER_LOCK_DESTROY(hal_9300_sim);
}

/* Register access */
static void ar9300_sim_reset(struct ath_hal_9300_sim* hal_9300_sim)
{
    int i;

    if (((hal_9300_sim->rtc_reset & AR_RTC_RESET_EN) == 0) ||
        ((hal_9300_sim->cr & AR_CR_RXD) != 0))
    {
        hal_9300_sim->rxdp[HAL_RX_QUEUE_HP][0] = 0;
        hal_9300_sim->rxdp_insert_here[HAL_RX_QUEUE_HP] = 0;
        hal_9300_sim->rxdp_remove_here[HAL_RX_QUEUE_HP] = 0;
        hal_9300_sim->rxdp_num_entries[HAL_RX_QUEUE_HP] = 0;

        hal_9300_sim->rxdp[HAL_RX_QUEUE_LP][0] = 0;
        hal_9300_sim->rxdp_insert_here[HAL_RX_QUEUE_LP] = 0;
        hal_9300_sim->rxdp_remove_here[HAL_RX_QUEUE_LP] = 0;
        hal_9300_sim->rxdp_num_entries[HAL_RX_QUEUE_LP] = 0;
    }

    if ((hal_9300_sim->rtc_reset & AR_RTC_RESET_EN) == 0) {
        for (i = 0; i < HAL_NUM_TX_QUEUES; ++i) {
            hal_9300_sim->txdp_insert_here[i] = 0;
            hal_9300_sim->txdp_remove_here[i] = 0;
            hal_9300_sim->txdp_num_entries[i] = 0;

            hal_9300_sim->txdp_status[i] = 0;
        }
        hal_9300_sim->txdp_enable = (0x1 << HAL_NUM_TX_QUEUES) - 1;
        hal_9300_sim->txdp_disable = 0;

        hal_9300_sim->dma_txcfg = 0;

        hal_9300_sim->rx_filter = 0;
        hal_9300_sim->mcast_filter_l32 = 0;
        hal_9300_sim->mcast_filter_u32 = 0;
        hal_9300_sim->sta_mask_l32 = 0xFFFFFFFF;
        hal_9300_sim->sta_mask_u16 = 0x0000FFFF;

#if 0
        /*
         * XXX Do we need to reset these entries
         * (resetting seems to be done manually/i.e. explicitly)
         */
        for (i = 0; i < (AR_KEY_CACHE_SIZE); ++i) {
            hal_9300_sim->key_cache[i].key0 = 0;
            hal_9300_sim->key_cache[i].key1 = 0;
            hal_9300_sim->key_cache[i].key2 = 0;
            hal_9300_sim->key_cache[i].key3 = 0;
            hal_9300_sim->key_cache[i].key4 = 0;
            hal_9300_sim->key_cache[i].key_type = 0;
            hal_9300_sim->key_cache[i].mac0 = 0;
            hal_9300_sim->key_cache[i].mac1_valid = 0;
        }
#endif
    }
}

u_int32_t AHSIM_AR9300_access_sim_reg(struct ath_hal *ah, u_int32_t reg,
    u_int32_t val, bool read_access)
{
    struct ath_hal_9300_sim* hal_9300_sim = &AH9300(ah)->hal_9300_sim;
    struct ath_hal_sim* hal_sim = &AH_PRIVATE(ah)->ah_hal_sim;
    u_int32_t result = 0;
    u_int64_t var64;
    u_int32_t i;

    AHSIM_REG_LOCK(hal_sim);

    if ((reg >= AR_KEYTABLE(0)) && (reg < AR_KEYTABLE(AR_KEY_CACHE_SIZE))) {
        u_int32_t key_index = (reg - AR_KEYTABLE_0) >> 5;
        struct ath_hal_9300_sim_key_cache* key_entry =
            &hal_9300_sim->key_cache[key_index];
        u_int32_t* raw_dwords = (u_int32_t*)key_entry;
        u_int32_t dword_index = (reg - AR_KEYTABLE_0 - (key_index << 5)) >> 2;

        if (read_access == true) {
            result = raw_dwords[dword_index];
        } else {
            raw_dwords[dword_index] = val;
        }
    } else if (reg == AR_HOSTIF_REG(ah, AR_SREV)) {
        /* WRITE access to a READ_ONLY register */
        AHSIM_ASSERT(read_access == true);
        /* Not sure this matters since override in the actual setting routine */
        result = AR_SREV_VERSION_OSPREY;
    } else {
        switch (reg) {
        case AR_DATABUF:
            if (read_access == true) {
                result = hal_9300_sim->rx_buffer_size;
            } else {
                hal_9300_sim->rx_buffer_size = val;
            }
            break;
        case AR_CR:
            if (read_access == true) {
                result = hal_9300_sim->cr;
            } else {
                hal_9300_sim->cr = val;
                ar9300_sim_reset(hal_9300_sim);
            }
            break;
        case AR_RTC_RESET:
            if (read_access == true) {
                result = hal_9300_sim->rtc_reset;
            } else {
                hal_9300_sim->rtc_reset = val;
                ar9300_sim_reset(hal_9300_sim);
            }
            break;
        case AR_RTC_STATUS:
            /* WRITE access to a READ_ONLY register */
            AHSIM_ASSERT(read_access == true);

            if (hal_9300_sim->rtc_reset == 0) {
                result = AR_RTC_STATUS_SHUTDOWN;
            } else {
                result = AR_RTC_STATUS_ON;
            }
            break;
        case AR_HP_RXDP:
            {
                int idx;
                if (read_access == true) {
                    /* READING AR_HP_RXDP but buffer is empty */
                    AHSIM_ASSERT(
                        hal_9300_sim->rxdp_num_entries[HAL_RX_QUEUE_HP] != 0);
                    idx = hal_9300_sim->rxdp_remove_here[HAL_RX_QUEUE_HP];
                    result = hal_9300_sim->rxdp[HAL_RX_QUEUE_HP][idx];
                } else {
                    /* AR_HP_RXDP is full - writing another entry */
                    AHSIM_ASSERT(
                        hal_9300_sim->rxdp_num_entries[HAL_RX_QUEUE_HP] !=
                        HAL_HP_RXFIFO_DEPTH);
                    idx = hal_9300_sim->rxdp_insert_here[HAL_RX_QUEUE_HP];
                    hal_9300_sim->rxdp[HAL_RX_QUEUE_HP][idx] = val;
                    hal_9300_sim->rxdp_wbuf[HAL_RX_QUEUE_HP][idx] =
                        AHSIM_GetRxWbuf(
                            AH_PRIVATE(ah)->ah_sc, val, HAL_RX_QUEUE_HP);
                    ++hal_9300_sim->rxdp_num_entries[HAL_RX_QUEUE_HP];
                    ++hal_9300_sim->rxdp_insert_here[HAL_RX_QUEUE_HP];
                    if (hal_9300_sim->rxdp_insert_here[HAL_RX_QUEUE_HP] >=
                        HAL_HP_RXFIFO_DEPTH)
                    {
                        hal_9300_sim->rxdp_insert_here[HAL_RX_QUEUE_HP] = 0;
                    }
                }
            }
            break;
        case AR_LP_RXDP:
            {
                int idx;
                if (read_access == true) {
                    /* READING AR_LP_RXDP but buffer is empty */
                    AHSIM_ASSERT(
                        hal_9300_sim->rxdp_num_entries[HAL_RX_QUEUE_LP] != 0);
                    idx = hal_9300_sim->rxdp_remove_here[HAL_RX_QUEUE_LP];
                    result = hal_9300_sim->rxdp[HAL_RX_QUEUE_LP][idx];
                } else {
                    /* AR_LP_RXDP is full - writing another entry! */
                    AHSIM_ASSERT(
                        hal_9300_sim->rxdp_num_entries[HAL_RX_QUEUE_LP] !=
                        HAL_LP_RXFIFO_DEPTH);
                    idx = hal_9300_sim->rxdp_insert_here[HAL_RX_QUEUE_LP];
                    hal_9300_sim->rxdp[HAL_RX_QUEUE_LP][idx] = val;
                    hal_9300_sim->rxdp_wbuf[HAL_RX_QUEUE_LP][idx] =
                        AHSIM_GetRxWbuf(
                            AH_PRIVATE(ah)->ah_sc, val, HAL_RX_QUEUE_LP);
                    ++hal_9300_sim->rxdp_num_entries[HAL_RX_QUEUE_LP];
                    ++hal_9300_sim->rxdp_insert_here[HAL_RX_QUEUE_LP];
                    if (hal_9300_sim->rxdp_insert_here[HAL_RX_QUEUE_LP] >=
                        HAL_LP_RXFIFO_DEPTH)
                    {
                        hal_9300_sim->rxdp_insert_here[HAL_RX_QUEUE_LP] = 0;
                    }
                }
            }
            break;
        case AR_Q_TXD:
            if (read_access == true) {
                result = hal_9300_sim->txdp_disable;
            } else {
                hal_9300_sim->txdp_disable = (val & AR_Q_TXD_M);
                for (i = 0; i < HAL_NUM_TX_QUEUES; ++i) {
                    u_int32_t mask = 0x1 << i;

                    if ((hal_9300_sim->txdp_disable & mask) != 0) {
                        hal_9300_sim->txdp_enable &= ~mask;

                        hal_9300_sim->txdp_insert_here[i] = 0;
                        hal_9300_sim->txdp_remove_here[i] = 0;
                        hal_9300_sim->txdp_num_entries[i] = 0;

                        hal_9300_sim->txdp_status[i] = 0;
                    }
                }
            }
            break;
        case AR_Q_TXE:
            if (read_access == true) {
                result = hal_9300_sim->txdp_enable;
            } else {
                hal_9300_sim->txdp_enable |= (val & AR_Q_TXE_M);
            }
            break;
        case AR_QSTS(0):
        case AR_QSTS(1):
        case AR_QSTS(2):
        case AR_QSTS(3):
        case AR_QSTS(4):
        case AR_QSTS(5):
        case AR_QSTS(6):
        case AR_QSTS(7):
        case AR_QSTS(8):
        case AR_QSTS(9):
            {
                u_int32_t q = (reg - AR_Q0_STS) >> 2;

                /* WRITE access to a READ_ONLY register */
                AHSIM_ASSERT(read_access == true);

                result = hal_9300_sim->txdp_status[q];
                break;
            }
        case AR_Q_STATUS_RING_START:
            if (read_access == true) {
                result = hal_9300_sim->txs_ring_start;
            } else {
                hal_9300_sim->txs_ring_start = val;
                hal_9300_sim->txs_current = hal_9300_sim->txs_ring_start;
            }
            break;
        case AR_Q_STATUS_RING_END:
            if (read_access == true) {
                result = hal_9300_sim->txs_ring_end;
            } else {
                hal_9300_sim->txs_ring_end = val;
                hal_9300_sim->txs_current = hal_9300_sim->txs_ring_start;
            }
            break;
        case AR_Q_STATUS_RING_CURRENT:
            /* WRITE access to a READ_ONLY register */
            AHSIM_ASSERT(read_access == true);

            result = hal_9300_sim->txs_current;
            break;
        case AR_QTXDP(0):
        case AR_QTXDP(1):
        case AR_QTXDP(2):
        case AR_QTXDP(3):
        case AR_QTXDP(4):
        case AR_QTXDP(5):
        case AR_QTXDP(6):
        case AR_QTXDP(7):
        case AR_QTXDP(8):
        case AR_QTXDP(9):
            {
                int idx;
                struct ar9300_txc* txc = AR9300TXC(val);

                u_int32_t q = (reg - AR_Q0_TXDP) >> 2;

                if (read_access == true) {
                    /* READING AR_QTXDP[%d] but buffer is empty */
                    AHSIM_ASSERT(hal_9300_sim->txdp_num_entries[q] != 0);
                    idx = hal_9300_sim->txdp_remove_here[q];
                    result = hal_9300_sim->txdp[q][idx];
                } else {
                    /* AR_QTXDP is full - writing another entry */
                    AHSIM_ASSERT(
                        hal_9300_sim->txdp_num_entries[q] != HAL_TXFIFO_DEPTH);
                    idx = hal_9300_sim->txdp_insert_here[q];
                    hal_9300_sim->txdp[q][idx] = val;
                    hal_9300_sim->txdp_ath_buf[q][idx] =
                        AHSIM_GetTxAthBuf(AH_PRIVATE(ah)->ah_sc, q, val);
                    ++hal_9300_sim->txdp_num_entries[q];
                    ++hal_9300_sim->txdp_insert_here[q];
                    if (hal_9300_sim->txdp_insert_here[q] >= HAL_TXFIFO_DEPTH) {
                        hal_9300_sim->txdp_insert_here[q] = 0;
                    }

                    hal_9300_sim->txdp_status[q] = 1;

                    /*
                     * unlock early since create the timer below -
                     * don't want to risk it
                     */
                    AHSIM_REG_UNLOCK(hal_sim);

                    AHSIM_TIMER_LOCK(hal_sim);
                    if (hal_sim->tx_timer_active == false) {
                        hal_sim->tx_timer_active = true;
                        OS_SET_TIMER(&hal_sim->tx_timer, 0);
                    }
                    AHSIM_TIMER_UNLOCK(hal_sim);

                    return 0; /* don't want to call unlock again */
                }
            }
            break;
        case AR_TXCFG:
            if (read_access == true) {
                result = hal_9300_sim->dma_txcfg;
            } else {
                hal_9300_sim->dma_txcfg = val;
            }
            break;
        case AR_NEXT_SWBA:
            if (read_access == true) {
                result = hal_9300_sim->next_swba;
            } else {
                hal_9300_sim->next_swba = val;
            }
            break;
        case AR_SWBA_PERIOD:
            if (read_access == true) {
                result = hal_9300_sim->swba_period;
            } else {
                hal_9300_sim->swba_period = val;
            }
            break;
        case AR_TIMER_MODE:
            if (read_access == true) {
                result = hal_9300_sim->timer_mode;
            } else {
                hal_9300_sim->timer_mode = val;

                AR9300_BEACON_TIMER_LOCK(hal_9300_sim);
                if (hal_9300_sim->timer_mode & AR_SWBA_TIMER_EN) {
                    if (!hal_9300_sim->beacon_timer_active) {
                        hal_9300_sim->beacon_timer_active = true;
                        OS_SET_TIMER(
                            &hal_9300_sim->beacon_timer,
                            (hal_9300_sim->next_swba -
                                (GET_TSF() & 0x00000000FFFFFFFF)) / 1000);
                    }
                } else {
                    if (hal_9300_sim->beacon_timer_active) {
                        OS_CANCEL_TIMER(&hal_9300_sim->beacon_timer);
                        hal_9300_sim->beacon_timer_active = false;
                    }
                }
                AR9300_BEACON_TIMER_UNLOCK(hal_9300_sim);
            }
            break;
        case AR_TSF_L32:
            if (read_access == true) {
                var64 = GET_TSF() & 0x00000000FFFFFFFF;
                result = var64;
            }
            break;
        case AR_TSF_U32:
            if (read_access == true) {
                var64 = GET_TSF() >> 32;
                result = var64;
            }
            break;
        case AR_STA_ID0:
            if (read_access == true) {
                result = hal_9300_sim->sta_addr_l32;
            } else {
                hal_9300_sim->sta_addr_l32 = val;
                hal_9300_sim->sta_addr[3] = (val & 0xFF000000) >> 24;
                hal_9300_sim->sta_addr[2] = (val & 0x00FF0000) >> 16;
                hal_9300_sim->sta_addr[1] = (val & 0x0000FF00) >> 8;
                hal_9300_sim->sta_addr[0] = (val & 0x000000FF) >> 0;
            }
            break;
        case AR_STA_ID1:
            if (read_access == true) {
                result = hal_9300_sim->sta_addr_u16;
            } else {
                hal_9300_sim->sta_addr_u16 = val;
                hal_9300_sim->sta_addr[5] = (val & 0x0000FF00) >> 8;
                hal_9300_sim->sta_addr[4] = (val & 0x000000FF) >> 0;
            }
            break;
        case AR_BSS_ID0:
            if (read_access == true) {
                result = hal_9300_sim->bssid_l32;
            } else {
                hal_9300_sim->bssid_l32 = val;
                hal_9300_sim->bssid[3] = (val & 0xFF000000) >> 24;
                hal_9300_sim->bssid[2] = (val & 0x00FF0000) >> 16;
                hal_9300_sim->bssid[1] = (val & 0x0000FF00) >> 8;
                hal_9300_sim->bssid[0] = (val & 0x000000FF) >> 0;
            }
            break;
        case AR_BSS_ID1:
            if (read_access == true) {
                result = hal_9300_sim->bssid_u16;
            } else {
                hal_9300_sim->bssid_u16 = val;
                hal_9300_sim->bssid[5] = (val & 0x0000FF00) >> 8;
                hal_9300_sim->bssid[4] = (val & 0x000000FF) >> 0;
            }
            break;
        case AR_RX_FILTER:
            if (read_access == true) {
                result = hal_9300_sim->rx_filter;
            } else {
                hal_9300_sim->rx_filter = val;
            }
            break;
        case AR_MCAST_FIL0:
            if (read_access == true) {
                result = hal_9300_sim->mcast_filter_l32;
            } else {
                hal_9300_sim->mcast_filter_l32 = val;
            }
            break;
        case AR_MCAST_FIL1:
            if (read_access == true) {
                result = hal_9300_sim->mcast_filter_u32;
            } else {
                hal_9300_sim->mcast_filter_u32 = val;
            }
            break;
        case AR_BSSMSKL:
            if (read_access == true) {
                result = hal_9300_sim->sta_mask_l32;
            } else {
                hal_9300_sim->sta_mask_l32 = val;
                hal_9300_sim->sta_mask[3] = (val & 0xFF000000) >> 24;
                hal_9300_sim->sta_mask[2] = (val & 0x00FF0000) >> 16;
                hal_9300_sim->sta_mask[1] = (val & 0x0000FF00) >> 8;
                hal_9300_sim->sta_mask[0] = (val & 0x000000FF) >> 0;
            }
            break;
        case AR_BSSMSKU:
            if (read_access == true) {
                result = hal_9300_sim->sta_mask_u16;
            } else {
                hal_9300_sim->sta_mask_u16 = val;
                hal_9300_sim->sta_mask[5] = (val & 0x0000FF00) >> 8;
                hal_9300_sim->sta_mask[4] = (val & 0x000000FF) >> 0;
            }
            break;
        default:
            result = 0;
            break;
        }
    }

    AHSIM_REG_UNLOCK(hal_sim);

    return result;
};

/* "transmission" functions */

u_int32_t AHSIM_AR9300_get_active_tx_queue(struct ath_hal *ah)
{
    struct ath_hal_9300_sim* hal_9300_sim = &AH9300(ah)->hal_9300_sim;
    struct ath_hal_sim* hal_sim = &AH_PRIVATE(ah)->ah_hal_sim;
    u_int32_t i;
    u_int32_t result = 255;

    AHSIM_REG_LOCK(hal_sim);

    for (i = 0; i < HAL_NUM_TX_QUEUES; ++i) {
        if (hal_9300_sim->txdp_num_entries[HAL_NUM_TX_QUEUES - i - 1] == 0) {
            continue;
        }

        result = HAL_NUM_TX_QUEUES - i - 1;
        break;
    }

    AHSIM_REG_UNLOCK(hal_sim);

    return result;
}

void AHSIM_AR9300_write_rxs_descriptor(u_int8_t* rx_buffer,
    struct ath_hal_9300_sim_key_cache* tx_key_entry, u_int32_t src_sim_index,
    struct ath_hal *dest_ah, bool is_first, bool is_last,
    struct ar9300_txc* txc, u_int32_t num_bytes)
{
    struct ath_hal_9300_sim* dest_hal_9300_sim =
        &AH9300(dest_ah)->hal_9300_sim;
    struct ar9300_rxs* rxs = (struct ar9300_rxs*)rx_buffer;
    u_int32_t fake_rssi = FAKE_RSSI_STRENGTH_GOOD;
    u_int32_t i;

    /* fake RSSI */
    if (src_sim_index != 0) {
        fake_rssi = FAKE_RSSI_STRENGTH_BAD;
    }

    /* create Rx status descriptor */
    rxs->ds_info = (ATHEROS_VENDOR_ID << AR_desc_id_S) |
        0xC;
    /*
     * XXX What about RATE - which field to copy from the tx_c desc?
     * (faking 36Mbps here)
     */
    rxs->status1 = 0xD << AR_rx_rate_S |
        fake_rssi << AR_rx_rssi_ant02_S |
        fake_rssi << AR_rx_rssi_ant01_S |
        fake_rssi << AR_rx_rssi_ant00_S;
    rxs->status2 = num_bytes;
    rxs->status3 =
        AHSIM_AR9300_access_sim_reg(dest_ah, AR_TSF_L32, 0, true);
    rxs->status4 = AR_rx_not_sounding;
    rxs->status5 = fake_rssi << AR_rx_rssi_combined_S |
        fake_rssi << AR_rx_rssi_ant12_S |
        fake_rssi << AR_rx_rssi_ant11_S |
        fake_rssi << AR_rx_rssi_ant10_S;
    rxs->status6 = 0;
    rxs->status7 = 0;
    rxs->status8 = 0;
    rxs->status9 = 0;
    rxs->status10 = 0;
    rxs->status11 = 0;
    if ((txc->ds_ctl12 & AR_is_aggr) && is_first) {
        rxs->status11 |= AR_rx_first_aggr;
    }
    if (txc->ds_ctl12 & AR_is_aggr) {
        rxs->status11 |= AR_rx_aggr;
    }
    if (txc->ds_ctl12 & AR_more_aggr) {
        rxs->status11 |= AR_rx_more_aggr;
    }

    rx_buffer += sizeof(struct ar9300_rxs);

    /* encryption */
    {
        u_int8_t rx_protected_frame;

        /* check on-air Encryption flag */
        rx_protected_frame = rx_buffer[1] & 0x40;

        /* Can't be non-encrypted but provide a hardware key */
        AHSIM_ASSERT(rx_protected_frame || !tx_key_entry);

        if (rx_protected_frame && !AH_PRIVATE(dest_ah)->ah_hal_sim.is_monitor) {
            if (tx_key_entry) {
                struct ath_hal_9300_sim_key_cache* rx_key_entry = NULL;

                for (i = 0; i < AR_KEY_CACHE_SIZE; ++i) {
                    rx_key_entry = &dest_hal_9300_sim->key_cache[i];

                    if ((rx_key_entry->key0 == tx_key_entry->key0) &&
                        (rx_key_entry->key1 == tx_key_entry->key1) &&
                        (rx_key_entry->key2 == tx_key_entry->key2) &&
                        (rx_key_entry->key3 == tx_key_entry->key3) &&
                        (rx_key_entry->key4 == tx_key_entry->key4) &&
                        (rx_key_entry->key_type == tx_key_entry->key_type)) {
                        break;
                    }

                    rx_key_entry = NULL;
                }

                if (rx_key_entry) {
                    rxs->status11 |=
                        AR_rx_frame_ok |
                        AR_rx_key_idx_valid |
                        (i << AR_key_idx_S);
                } else {
                    rxs->status11 |= AR_key_miss;
                }
            } else {
                rxs->status11 |= AR_key_miss;
            }
        } else {
            rxs->status11 |= AR_rx_frame_ok;
        }
    }

    rxs->status11 |= AR_rx_done;

    /* update hal_sim structure to remove this Rx buffer */
    --dest_hal_9300_sim->rxdp_num_entries[HAL_RX_QUEUE_LP];
    ++dest_hal_9300_sim->rxdp_remove_here[HAL_RX_QUEUE_LP];
    if (dest_hal_9300_sim->rxdp_remove_here[HAL_RX_QUEUE_LP] >=
        HAL_LP_RXFIFO_DEPTH)
    {
        dest_hal_9300_sim->rxdp_remove_here[HAL_RX_QUEUE_LP] = 0;
    }

    AHSIM_monitor_pkt_data_descriptor(12 * 4, (u_int8_t*)rxs);
}

u_int32_t AHSIM_AR9300_process_tx_descriptor(void* src_ath_buf_ptr,
    struct ath_hal *src_ah, u_int8_t* alt_src, struct ath_hal *dest_ah,
    bool is_first, u_int64_t* ba_mask, u_int32_t agg_pkt_num)
{
    struct ath_hal_9300_sim* dest_hal_9300_sim = NULL;
    struct ath_hal_9300_sim* src_hal_9300_sim = NULL;
    struct ar9300_txc* txc = NULL;
    u_int32_t ds_link = 0;
    void* dest_wbuf_ptr = NULL;
    u_int32_t num_bytes = 0;
    u_int32_t alt_src_num_bytes = 0;
    u_int8_t* dest_buffer = NULL;
    u_int32_t i = 0;
    /*
     * catch invalid values in AHSIM_AR9300_validate_tx_desc
     * but do this as backup
     */
    u_int32_t enc_pad_bytes[8] = { 0, 4, 8, 12, 16, 0, 0, 0 };
    u_int32_t enc_pad = 0;
    u_int32_t agg_pkts = 0;
    u_int8_t agg_per = 0;
    u_int8_t rv = 0;
    bool drop_agg_pkt = false;
    struct ath_hal_9300_sim_key_cache* tx_key_entry = NULL;

    dest_hal_9300_sim = &AH9300(dest_ah)->hal_9300_sim;

    if (src_ah) {
        src_hal_9300_sim = &AH9300(src_ah)->hal_9300_sim;
        txc = AR9300TXC(AHSIM_AthBufToTxcDesc(src_ath_buf_ptr));
    } else {
        txc = (struct ar9300_txc*)alt_src;
    }

    if (src_ah) { /* XXX : disabling Agg PER for intersim - TODO! */
        if (txc->ds_ctl12 & AR_is_aggr) { /* Agg PER simulation */
            int src_idx, dest_idx;

            src_idx = AH_PRIVATE(src_ah)->ah_hal_sim.sim_index;
            dest_idx = AH_PRIVATE(dest_ah)->ah_hal_sim.sim_index;
            agg_per = AHSIM_sim_info.agg_per_src_dest[src_idx][dest_idx];
            if (agg_per && !AH_PRIVATE(dest_ah)->ah_hal_sim.is_monitor) {
                rv = AHSIM_rand(0, 99);
                if (agg_per > rv) {
                    drop_agg_pkt = true;
                    AHSIM_monitor_pkt_agg_per_info(agg_per, rv, agg_pkt_num);
                }
            }
        }
    }

    ds_link = txc->ds_link;
    if (txc->ds_ctl11 & AR_veol) {
        ds_link = 0;
    }

    if (!drop_agg_pkt) {
        int idx;
        /* get next available Rx buffer */
        if (dest_hal_9300_sim->rxdp_num_entries[HAL_RX_QUEUE_LP] == 0) {
            ath_hal_printf(AH_NULL,
                "AHSIM_AR9300_process_tx_descriptor: "
                "No Rx buffers available!\n");
            AHSIM_ASSERT(0);
            return agg_pkts;
        }
        idx = dest_hal_9300_sim->rxdp_remove_here[HAL_RX_QUEUE_LP];
        dest_wbuf_ptr = dest_hal_9300_sim->rxdp_wbuf[HAL_RX_QUEUE_LP][idx];
        dest_buffer = (u_int8_t*) wbuf_raw_data(
            (wbuf_t) dest_wbuf_ptr) + sizeof(struct ar9300_rxs);
        if (src_ah) {
            if (txc->ds_data1 == 0) {
                /* single link Tx: most likely mgmt */
                num_bytes = (txc->ds_ctl3 & AR_buf_len) >> AR_buf_len_S;
                if (num_bytes > dest_hal_9300_sim->rx_buffer_size) {
                    ath_hal_printf(AH_NULL,
                        "AHSIM_AR9300_process_tx_descriptor: "
                        "Rx buffer isn't large enough!\n");
                    AHSIM_ASSERT(0);
                    return agg_pkts;
                }

                OS_MEMCPY(
                    dest_buffer,
                    AHSIM_GetTxBufferVirtualAddress(
                        src_ath_buf_ptr, (void*)txc->ds_data0),
                    num_bytes);
            } else {
                /*
                 * multi-link Tx: scatter-gather structure:
                 * defer to internal copy
                 */
                wbuf_t tx_wbuf = AHSIM_GetTxBuffer(src_ath_buf_ptr);
                num_bytes = wbuf_get_pktlen(tx_wbuf);
                if (num_bytes > dest_hal_9300_sim->rx_buffer_size) {
                    ath_hal_printf(AH_NULL,
                        "AHSIM_AR9300_process_tx_descriptor: "
                        "Rx buffer isn't large enough!\n");
                    AHSIM_ASSERT(0);
                    return agg_pkts;
                }

                wbuf_copydata(tx_wbuf, 0, num_bytes, dest_buffer);
            }
        } else {
            alt_src_num_bytes =
                *(u_int32_t*)(alt_src + 23 * 4 + 9 * 4) & ~DBG_ATHEROS_MASK;
            num_bytes = alt_src_num_bytes;
            OS_MEMCPY(dest_buffer, alt_src + 23 * 4 + 9 * 4 + 4, num_bytes);
        }

        /* Append encryption info (if needed) and FCS - set to known values */
        enc_pad =
            enc_pad_bytes[(txc->ds_ctl17 & AR_encr_type) >> AR_encr_type_S];
        for (i = 0; i < enc_pad; ++i) { /* Encryption padding */
            dest_buffer[num_bytes++] = 0xEE;
        }

        for (i = 0; i < 4; ++i) { /* FCS */
            dest_buffer[num_bytes++] = 0xFF;
        }

        *ba_mask |= ((u_int64_t)0x1 << agg_pkt_num);
        agg_pkts = 1;

        /* Tx encryption key lookup */
        if (txc->ds_ctl11 & AR_dest_idx_valid) {
            if (src_ah) {
                int idx = MS(txc->ds_ctl12, AR_dest_idx);
                tx_key_entry = &src_hal_9300_sim->key_cache[idx];
            } else {
                tx_key_entry = (struct ath_hal_9300_sim_key_cache*)
                    (alt_src + 23 * 4 + 4);
            }
            if ((tx_key_entry->key_type & AR_KEY_TYPE) ==
                AR_KEYTABLE_TYPE_CLR)
            {
                tx_key_entry = NULL;
            }
        }

        /* create Rx descriptor */
        AHSIM_AR9300_write_rxs_descriptor(
            wbuf_raw_data((wbuf_t)dest_wbuf_ptr), tx_key_entry,
            src_ah ? AH_PRIVATE(src_ah)->ah_hal_sim.sim_index : 0xFFFF,
            dest_ah, is_first, ds_link == 0, txc, num_bytes);

        AHSIM_monitor_pkt_snippet(
            AH_PRIVATE(dest_ah)->ah_hal_sim.output_pkt_snippet_length,
            dest_buffer);
    }

    /* process next descriptor in the chain */
    if (ds_link != 0) {
        if (alt_src) {
            alt_src += 23 * 4 + 9 * 4 + alt_src_num_bytes + 4;

            AHSIM_ASSERT(
                (*(u_int32_t*)alt_src & DBG_ATHEROS_MASK) ==
                (ATHEROS_VENDOR_ID << 16));
            AHSIM_ASSERT(
                *(u_int32_t*)(alt_src + 23 * 4) == DBG_ATHEROS_KEY_ID);
            AHSIM_ASSERT(
                (*(u_int32_t*)(alt_src + 23 * 4 + 9 * 4) & DBG_ATHEROS_MASK) ==
                DBG_ATHEROS_SNIPPET_ID);
        }
        agg_pkts += AHSIM_AR9300_process_tx_descriptor(
            src_ah ?
                AHSIM_GetNextTxBuffer(src_ath_buf_ptr, (void*)ds_link) : NULL,
            src_ah, alt_src,
            dest_ah, false, ba_mask, agg_pkt_num + 1);
    }

    return agg_pkts;
}

bool AHSIM_AR9300_rx_filter_accept_packet(
    struct ath_hal_9300_sim* dest_hal_9300_sim, u_int8_t* tx_buffer,
    bool* is_unicast_for_me)
{
    u_int32_t rx_filter = dest_hal_9300_sim->rx_filter;
    u_int8_t* addr1 = tx_buffer + 4;
    u_int8_t* bssid = 0;
    u_int8_t ds = *(tx_buffer + 1) & 0x03;
    u_int8_t bcast[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    u_int32_t failure_reason = 0;
    u_int8_t masked_sta_addr[6];
    u_int8_t masked_addr1[6];
    u_int32_t i;

    *is_unicast_for_me = false;

    if (rx_filter & AR_RX_PROM) {
        return true;
    }

    switch (ds) {
    case 0x00: /* To = 0, From = 0 */
        bssid = addr1 + 12;
        break;
    case 0x01: /* To = 1, From = 0 */
        bssid = addr1;
        break;
    case 0x02: /* To = 0, From = 1 */
        bssid = addr1 + 6;
        break;
    case 0x03: /* 4 address format */
        bssid = addr1;
        break;
    }

    if (((*tx_buffer & 0x0C) == 0x00) && ((*tx_buffer & 0xF0) == 0x40)) {
        /* Probe request */
        failure_reason = rx_filter & AR_RX_PROBE_REQ;
        if (!failure_reason) {
            AHSIM_monitor_pkt_filter_info(rx_filter, AR_RX_PROBE_REQ);
        }
        return failure_reason;
    }

    if (((*tx_buffer & 0x0C) == 0x00) && ((*tx_buffer & 0xF0) == 0x80)) {
        /* Beacon */
        if (rx_filter & AR_RX_BEACON) {
            return true;
        } else if (rx_filter & AR_RX_MY_BEACON) {
            if (!OS_MEMCMP(bssid, dest_hal_9300_sim->bssid, 6)) {
                return true;
            }
        }

        AHSIM_monitor_pkt_filter_info(
            rx_filter, AR_RX_BEACON | AR_RX_MY_BEACON);
        return false;
    }

    if (!OS_MEMCMP(addr1, bcast, 6)) {
        if (rx_filter & AR_RX_BCAST) {
            failure_reason = !OS_MEMCMP(bssid, dest_hal_9300_sim->bssid, 6);
            if (!failure_reason) {
                AHSIM_monitor_pkt_filter_info(rx_filter, AR_RX_BCAST);
            }
            return failure_reason;
        }

        AHSIM_monitor_pkt_filter_info(rx_filter, AR_RX_BCAST);
        return false;
    }

    if (addr1[0] & 0x01) { /* MCAST */
        if (rx_filter & AR_RX_MCAST) {
            /* TBD: check for multicast filter */

            failure_reason = !OS_MEMCMP(bssid, dest_hal_9300_sim->bssid, 6);
            if (!failure_reason) {
                AHSIM_monitor_pkt_filter_info(rx_filter, AR_RX_MCAST);
            }
            return failure_reason;
        }

        return false;
    }

    for (i = 0; i < 6; ++i) {
        masked_sta_addr[i] =
            dest_hal_9300_sim->sta_addr[i] & dest_hal_9300_sim->sta_mask[i];
        masked_addr1[i] = addr1[i] & dest_hal_9300_sim->sta_mask[i];
    }
    *is_unicast_for_me = !OS_MEMCMP(masked_addr1, masked_sta_addr, 6);
    if (rx_filter & AR_RX_UCAST) {
        if (!*is_unicast_for_me) {
            AHSIM_monitor_pkt_filter_info(rx_filter, AR_RX_UCAST);
        }
        return *is_unicast_for_me;
    }

    AHSIM_monitor_pkt_filter_info(rx_filter, 0);
    return false;
}

u_int8_t* AHSIM_AR9300_get_tx_buffer(struct ath_hal* ah, u_int32_t q,
    u_int32_t* frag_length)
{
    struct ath_hal_9300_sim* hal_9300_sim = &AH9300(ah)->hal_9300_sim;
    void* ath_buf_ptr;
    struct ar9300_txc* txc;
    u_int8_t* tx_buffer;
    int idx;

    idx = hal_9300_sim->txdp_remove_here[q];
    ath_buf_ptr = hal_9300_sim->txdp_ath_buf[q][idx];
    txc = AR9300TXC(AHSIM_AthBufToTxcDesc(ath_buf_ptr));
    tx_buffer =
        AHSIM_GetTxBufferVirtualAddress(ath_buf_ptr, (void*)txc->ds_data0);

    *frag_length = MS(txc->ds_ctl3, AR_buf_len);

    return tx_buffer;
}

u_int32_t AHSIM_AR9300_transmit_packet(struct ath_hal *src_ah,
    u_int8_t* alt_src, struct ath_hal *dest_ah, u_int32_t q,
    u_int64_t* ba_mask, bool *is_unicast_for_me)
{
    struct ath_hal_9300_sim* src_hal_9300_sim = NULL;
    struct ath_hal_9300_sim* dest_hal_9300_sim = NULL;
    struct ath_hal_sim* dest_hal_sim = NULL;
    void* src_ath_buf_ptr = NULL;
    u_int32_t dest_rxdp_num_entries = 0;
    u_int32_t agg_pkts = 0;
    struct ar9300_txc* txc = NULL;
    u_int8_t* tx_buffer = NULL;

    dest_hal_9300_sim = &AH9300(dest_ah)->hal_9300_sim;
    dest_hal_sim = &AH_PRIVATE(dest_ah)->ah_hal_sim;

    if (src_ah) {
        int idx;
        src_hal_9300_sim = &AH9300(src_ah)->hal_9300_sim;
        idx = src_hal_9300_sim->txdp_remove_here[q];
        src_ath_buf_ptr = src_hal_9300_sim->txdp_ath_buf[q][idx];
        txc = AR9300TXC(AHSIM_AthBufToTxcDesc(src_ath_buf_ptr));
        tx_buffer = AHSIM_GetTxBufferVirtualAddress(
            src_ath_buf_ptr, (void*)txc->ds_data0);
    } else {
        txc = (struct ar9300_txc*)alt_src;
        tx_buffer = alt_src + 23 * 4 + 9 * 4 + 4;
    }

    if (!AHSIM_AR9300_rx_filter_accept_packet(
            dest_hal_9300_sim, tx_buffer, is_unicast_for_me))
    {
        if (AHSIM_sim_info.sim_filter) {
            return 0;
        }
    }

    AHSIM_REG_LOCK(dest_hal_sim);
    dest_rxdp_num_entries =
        dest_hal_9300_sim->rxdp_num_entries[HAL_RX_QUEUE_LP];
    AHSIM_REG_UNLOCK(dest_hal_sim);

    if (dest_rxdp_num_entries == 0) {
        /* destination node is not ready for reception yet */
        ath_hal_printf(dest_ah,
                       "AHSIM_AR9300_transmit_packet: No destination "
                       "Rx buffers available - not ready for reception! "
                       "[%d -> %d]\n",
                       AH_PRIVATE(src_ah)->ah_hal_sim.sim_index,
                       AH_PRIVATE(dest_ah)->ah_hal_sim.sim_index);
        return 0;
    }

    agg_pkts = AHSIM_AR9300_process_tx_descriptor(
        src_ath_buf_ptr, src_ah, alt_src, dest_ah, true, ba_mask, 0);

    AHSIM_TriggerInterrupt(
        AH_PRIVATE(dest_ah)->ah_sc, HAL_INT_RXHP | HAL_INT_RXLP);

    if ((txc->ds_ctl12 & AR_no_ack) || !*is_unicast_for_me) {
        return 0;
    }

    return agg_pkts;
}

bool AHSIM_AR9300_record_tx_c(struct ath_hal* ah, u_int32_t q,
    bool intersim)
{
    struct ath_hal_9300_sim* hal_9300_sim = &AH9300(ah)->hal_9300_sim;
    int idx = hal_9300_sim->txdp_remove_here[q];
    void* ath_buf_ptr = hal_9300_sim->txdp_ath_buf[q][idx];
    struct ar9300_txc* txc;
    u_int32_t ds_link;
    u_int32_t num_bytes;
    bool intersim_success = true;
    u_int32_t original_output_buffer_num_bytes =
        AHSIM_sim_info.output_buffer_num_bytes;

    if (intersim) {
        AHSIM_OUTPUT_BUFFER_LOCK();

        if ((AHSIM_sim_info.output_buffer_num_bytes + 22) >
            AHSIM_INTER_SIM_BUFFER_SIZE)
        {
            intersim_success = false;
        } else {
            num_bytes = AHSIM_sim_info.output_buffer_num_bytes;
            *(u_int32_t*) &AHSIM_sim_info.output_buffer[num_bytes] =
                DBG_ATHEROS_DESC_ID | AH_PRIVATE(ah)->ah_hal_sim.sim_index;
            ah->ah_get_mac_address(ah,
                AHSIM_sim_info.output_buffer + num_bytes + 4);
            AHSIM_sim_info.output_buffer_num_bytes += 10;
            num_bytes = AHSIM_sim_info.output_buffer_num_bytes;
            *(u_int32_t*) &AHSIM_sim_info.output_buffer[num_bytes] =
                DBG_ATHEROS_CHANNEL_ID;
            *(u_int32_t*) &AHSIM_sim_info.output_buffer[num_bytes + 4] =
                AH_PRIVATE(ah)->ah_curchan->channel;
            /* not needed but want to keep same format */
            *(u_int32_t*) &AHSIM_sim_info.output_buffer[num_bytes + 8] = 0;
            AHSIM_sim_info.output_buffer_num_bytes += 12;
        }
    }

    if (intersim_success) {
        do {
            AHSIM_ASSERT(ath_buf_ptr);

            txc = AR9300TXC(AHSIM_AthBufToTxcDesc(ath_buf_ptr));

            if (intersim) {
                if ((AHSIM_sim_info.output_buffer_num_bytes + 23 * 4) >
                    AHSIM_INTER_SIM_BUFFER_SIZE)
                {
                    intersim_success = false;
                    break;
                }
                OS_MEMCPY(
                    AHSIM_sim_info.output_buffer +
                        AHSIM_sim_info.output_buffer_num_bytes,
                    (u_int8_t*)txc, 23 * 4);
                AHSIM_sim_info.output_buffer_num_bytes += 23 * 4;
            } else {
                AHSIM_monitor_pkt_data_descriptor(23 * 4, (u_int8_t*)txc);
            }

            if (intersim || AHSIM_sim_info.output_key) {
                struct ath_hal_9300_sim_key_cache nop_key_entry = {
                    0, /* key0 */
                    0, /* key1 */
                    0, /* key2 */
                    0, /* key3 */
                    0, /* key4 */
                    AR_KEYTABLE_TYPE_CLR, /* key_type */
                    0, /* mac0 */
                    0 /* mac1 */
                };
                struct ath_hal_9300_sim_key_cache* tx_key_entry = NULL;

                if (txc->ds_ctl11 & AR_dest_idx_valid) {
                    int dest_idx = MS(txc->ds_ctl12, AR_dest_idx);
                    tx_key_entry = &hal_9300_sim->key_cache[dest_idx];
                }

                if (intersim) {
                    if ((AHSIM_sim_info.output_buffer_num_bytes + 9 * 4) >
                        AHSIM_INTER_SIM_BUFFER_SIZE)
                    {
                        intersim_success = false;
                        break;
                    }
                    num_bytes = AHSIM_sim_info.output_buffer_num_bytes;
                    *(u_int32_t*) &AHSIM_sim_info.output_buffer[num_bytes] =
                        DBG_ATHEROS_KEY_ID;
                    OS_MEMCPY(
                        AHSIM_sim_info.output_buffer +
                            AHSIM_sim_info.output_buffer_num_bytes + 4,
                        (u_int8_t*) (tx_key_entry ?
                            tx_key_entry : &nop_key_entry), 8 * 4);
                    AHSIM_sim_info.output_buffer_num_bytes += 9 * 4;
                } else {
                    if (tx_key_entry) {
                        AHSIM_MonitorPktKey((u_int8_t*)tx_key_entry);
                    }
                }
            }

            if (intersim) { /* output packet data */
                int out_buf_num_bytes;
                if (txc->ds_data1 == 0) {
                    /* single link Tx: most likely mgmt */
                    out_buf_num_bytes = AHSIM_sim_info.output_buffer_num_bytes;
                    num_bytes = MS(txc->ds_ctl3, AR_buf_len);
                    if ((out_buf_num_bytes + num_bytes + 4) >
                        AHSIM_INTER_SIM_BUFFER_SIZE)
                    {
                        intersim_success = false;
                        break;
                    }
                    *(u_int32_t*)
                        &AHSIM_sim_info.output_buffer[out_buf_num_bytes] =
                            DBG_ATHEROS_SNIPPET_ID | num_bytes;
                    OS_MEMCPY(
                        AHSIM_sim_info.output_buffer + out_buf_num_bytes + 4,
                        AHSIM_GetTxBufferVirtualAddress(
                            ath_buf_ptr, (void*)txc->ds_data0),
                        num_bytes);
                } else {
                    /*
                     * multi-link Tx: scatter-gather structure:
                     * defer to internal copy
                     */
                    wbuf_t tx_wbuf = AHSIM_GetTxBuffer(ath_buf_ptr);
                    out_buf_num_bytes = AHSIM_sim_info.output_buffer_num_bytes;
                    num_bytes = wbuf_get_pktlen(tx_wbuf);
                    if ((out_buf_num_bytes + num_bytes + 4) >
                        AHSIM_INTER_SIM_BUFFER_SIZE)
                    {
                        intersim_success = false;
                        break;
                    }
                    *(u_int32_t*)
                        &AHSIM_sim_info.output_buffer[out_buf_num_bytes] =
                            DBG_ATHEROS_SNIPPET_ID | num_bytes;
                    wbuf_copydata(
                        tx_wbuf, 0, num_bytes,
                        AHSIM_sim_info.output_buffer + out_buf_num_bytes + 4);
                }
                AHSIM_sim_info.output_buffer_num_bytes += num_bytes + 4;
           }

            ds_link = txc->ds_link;
            if (txc->ds_ctl11 & AR_veol) {
                ds_link = 0;
            }

            if (!ds_link) {
                break;
            }

            ath_buf_ptr = AHSIM_GetNextTxBuffer(ath_buf_ptr, (void*)ds_link);
        } while (1);
    }

    if (intersim) {
        if (!intersim_success) {
            AHSIM_sim_info.output_buffer_num_bytes =
                original_output_buffer_num_bytes;
        }
        AHSIM_OUTPUT_BUFFER_UNLOCK();
        return intersim_success;
    }

    return true;
}

void AHSIM_AR9300_done_transmit(struct ath_hal *ah, u_int32_t q,
    u_int32_t num_pkts, u_int64_t ba_mask)
{
    struct ath_hal_9300_sim* hal_9300_sim = &AH9300(ah)->hal_9300_sim;
    void* ath_buf_ptr;
    struct ar9300_txc* txc;
    u_int32_t tx_desc_id;
    struct ar9300_txs* txs;
    struct ar9300_txs *txs_as_firmware_calcs;
    struct ath_hal_sim *hal_sim = &AH_PRIVATE(ah)->ah_hal_sim;
    u_int8_t* tx_buffer;
    u_int32_t seq_number = 0;
    u_int32_t tid = 0;
    bool ack_required = false;
    int idx;

    AHSIM_REG_LOCK(hal_sim);

    idx = hal_9300_sim->txdp_remove_here[q];
    ath_buf_ptr = hal_9300_sim->txdp_ath_buf[q][idx];

    --hal_9300_sim->txdp_num_entries[q];
    ++hal_9300_sim->txdp_remove_here[q];
    if (hal_9300_sim->txdp_remove_here[q] >= HAL_TXFIFO_DEPTH) {
        hal_9300_sim->txdp_remove_here[q] = 0;
    }

    if (hal_9300_sim->txdp_num_entries[q] == 0) {
        hal_9300_sim->txdp_status[q] = 0;
    }

    txc = AR9300TXC(AHSIM_AthBufToTxcDesc(ath_buf_ptr));
    tx_buffer = wbuf_header(AHSIM_GetTxBuffer(ath_buf_ptr));

    ack_required = !(txc->ds_ctl12 & AR_no_ack);

    if (txc->ds_ctl12 & AR_is_aggr) {
        seq_number = (tx_buffer[23] << 4) | ((tx_buffer[22] & 0xF0) >> 4);

        if (((tx_buffer[0] & 0x0C) == 0x08) && ((tx_buffer[0] & 0x80) != 0)) {
            tid = tx_buffer[((tx_buffer[1] & 0x3) == 0x3) ? 30 : 24] & 0x0F;
        }
    }

    tx_desc_id = ((txc->ds_ctl10) & AR_desc_id) >> AR_desc_id_S;

    txs = AR9300TXS(AHSIM_GetTxStatusDescriptor(
        AH_PRIVATE(ah)->ah_sc, AH9300(ah)->hal_9300_sim.txs_current));

    txs_as_firmware_calcs = &AH9300(ah)->ts_ring[AH9300(ah)->ts_tail];

    AHSIM_REG_UNLOCK(hal_sim);

    /*
     * The hardware (sim) and the firmware calculate different
     * Tx status descriptor addresses!
     */
    AHSIM_ASSERT(txs == txs_as_firmware_calcs);

    txs->ds_info = (ATHEROS_VENDOR_ID << AR_desc_id_S) |
        (0x1 << AR_tx_rx_desc_S) |
        (q << AR_tx_qcu_num_S) |
        (0x9);
    txs->status1 = tx_desc_id << 16;
    txs->status2 = (FAKE_RSSI_STRENGTH_GOOD << AR_tx_rssi_ant00_S) |
        (FAKE_RSSI_STRENGTH_GOOD << AR_tx_rssi_ant01_S) |
        (FAKE_RSSI_STRENGTH_GOOD << AR_tx_rssi_ant02_S);

    txs->status5 = 0x0;
    txs->status6 = 0x0;
    if ((txc->ds_ctl12 & AR_is_aggr) && ack_required && num_pkts) {
        txs->status2 |= AR_tx_ba_status;

        txs->status5 = ba_mask & 0xFFFFFFFF;
        txs->status6 = (ba_mask & 0xFFFFFFFF00000000) >> 32;
    }
    txs->status3 = 0;
    txs->status4 = AHSIM_AR9300_access_sim_reg(ah, AR_TSF_L32, 0, true);
    txs->status7 = (FAKE_RSSI_STRENGTH_GOOD << AR_tx_rssi_ant10_S) |
        (FAKE_RSSI_STRENGTH_GOOD << AR_tx_rssi_ant11_S) |
        (FAKE_RSSI_STRENGTH_GOOD << AR_tx_rssi_ant12_S) |
        (FAKE_RSSI_STRENGTH_GOOD << AR_tx_rssi_combined_S);
    txs->status8 = (seq_number << AR_seq_num_S) |
        (tid << AR_tx_tid_S) |
        AR_tx_done;

    if (ack_required) {
        if (num_pkts) {
            txs->status3 = AR_frm_xmit_ok;
        } else {
            u_int32_t status3;
            u_int32_t try_counter;

            status3 =
                SM(MS(txc->ds_ctl13, AR_xmit_data_tries0), AR_data_fail_cnt);
            try_counter = 0;
            if (MS(txc->ds_ctl13, AR_xmit_data_tries1)) {
                status3 = SM(
                    MS(txc->ds_ctl13, AR_xmit_data_tries1), AR_data_fail_cnt);
                try_counter = 1;
            }
            if (MS(txc->ds_ctl13, AR_xmit_data_tries2)) {
                status3 = SM(
                    MS(txc->ds_ctl13, AR_xmit_data_tries2), AR_data_fail_cnt);
                try_counter = 2;
            }
            if (MS(txc->ds_ctl13, AR_xmit_data_tries3)) {
                status3 = SM(
                    MS(txc->ds_ctl13, AR_xmit_data_tries3), AR_data_fail_cnt);
                try_counter = 3;
            }

            txs->status3 = status3 | AR_excessive_retries;
            txs->status8 |= SM(try_counter, AR_final_tx_idx);
        }
    } else {
        txs->status3 = AR_frm_xmit_ok;
    }

    AHSIM_REG_LOCK(hal_sim);
    AH9300(ah)->hal_9300_sim.txs_current += sizeof(struct ar9300_txs);
    if (AH9300(ah)->hal_9300_sim.txs_current >=
        AH9300(ah)->hal_9300_sim.txs_ring_end)
    {
        AH9300(ah)->hal_9300_sim.txs_current =
            AH9300(ah)->hal_9300_sim.txs_ring_start;
    }
    AHSIM_REG_UNLOCK(hal_sim);

    AHSIM_monitor_pkt_data_descriptor(9 * 4, (u_int8_t*)txs);

    AHSIM_TriggerInterrupt(AH_PRIVATE(ah)->ah_sc, HAL_INT_TX);
}

void AHSIM_AR9300_rx_dummy_pkt(struct ath_hal* ah, u_int8_t* dummy_pkt,
    u_int32_t dummy_pkt_bytes)
{
    struct ath_hal_9300_sim* hal_9300_sim = &AH9300(ah)->hal_9300_sim;
    void* wbuf_ptr;
    u_int8_t* buffer;
    struct ar9300_rxs* rxs;
    int idx;

    if (hal_9300_sim->rxdp_num_entries[HAL_RX_QUEUE_LP] == 0) {
        ath_hal_printf(AH_NULL,
            "AHSIM_AR9300_rx_dummy_pkt: No Rx buffers available!\n");
        return;
    }
    idx = hal_9300_sim->rxdp_remove_here[HAL_RX_QUEUE_LP];
    wbuf_ptr = hal_9300_sim->rxdp_wbuf[HAL_RX_QUEUE_LP][idx];
    buffer = (u_int8_t*)wbuf_raw_data((wbuf_t)wbuf_ptr);

    /* frame control (reserved control) */
    buffer[sizeof(struct ar9300_rxs)] = 0x04;
    buffer[sizeof(struct ar9300_rxs) + 1] = 0x00;
    OS_MEMCPY(
        buffer + sizeof(struct ar9300_rxs) + 2,
        dummy_pkt,
        dummy_pkt_bytes);

    rxs = (struct ar9300_rxs*)buffer;

    /* create Rx status descriptor */
    rxs->ds_info = (ATHEROS_VENDOR_ID << AR_desc_id_S) |
        0xC;
    rxs->status1 = 0x0 << AR_rx_rate_S |
        0x80 << AR_rx_rssi_ant02_S |
        0x80 << AR_rx_rssi_ant01_S |
        0x80 << AR_rx_rssi_ant00_S;
    rxs->status2 = dummy_pkt_bytes + 2;
    rxs->status3 = AHSIM_AR9300_access_sim_reg(ah, AR_TSF_L32, 0, true);
    rxs->status4 = AR_rx_not_sounding;
    rxs->status5 = 0x80 << AR_rx_rssi_combined_S |
        0x80 << AR_rx_rssi_ant12_S |
        0x80 << AR_rx_rssi_ant11_S |
        0x80 << AR_rx_rssi_ant10_S;
    rxs->status6 = 0;
    rxs->status7 = 0;
    rxs->status8 = 0;
    rxs->status9 = 0;
    rxs->status10 = 0;
    rxs->status11 = AR_rx_frame_ok | AR_rx_done;

    --hal_9300_sim->rxdp_num_entries[HAL_RX_QUEUE_LP];
    ++hal_9300_sim->rxdp_remove_here[HAL_RX_QUEUE_LP];
    if (hal_9300_sim->rxdp_remove_here[HAL_RX_QUEUE_LP] >=
        HAL_LP_RXFIFO_DEPTH)
    {
        hal_9300_sim->rxdp_remove_here[HAL_RX_QUEUE_LP] = 0;
    }

    AHSIM_TriggerInterrupt(AH_PRIVATE(ah)->ah_sc, HAL_INT_RXHP | HAL_INT_RXLP);
}

#endif /* ATH_DRIVER_SIM */
