/*
 *  Copyright (c) 2006, Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */

/*
 * Public Interface for hardware timer, including generic TSF timer
 * and future low frequecy timer.
 */

#ifndef _DEV_ATH_HWTIMER_H
#define _DEV_ATH_HWTIMER_H

/*
 * Some hardware can support multiple TSF Timers. For parameter will specific which
 * TSF Clock to use. For now, just use 0.
 */
typedef int ath_tsftimer_clk;

typedef void (*ath_hwtimer_function)(void *arg);

#ifdef ATH_USB
/* For USB split driver, offload the generic timer operation to target side. */
typedef usblock_t  ath_hwtimer_lock_t;

#define ATH_GENTIMER_LOCK_INIT(_timer)            OS_USB_LOCK_INIT(&((_timer)->gen_timer_lock))
#define ATH_GENTIMER_LOCK_DESTROY(_timer)         OS_USB_LOCK_DESTROY(&((_timer)->gen_timer_lock))
#define ATH_GENTIMER_LOCK(_timer)                 OS_USB_LOCK(&((_timer)->gen_timer_lock))
#define ATH_GENTIMER_UNLOCK(_timer)               OS_USB_UNLOCK(&((_timer)->gen_timer_lock))

#define ATH_GENTIMER_HAL_LOCK_INIT(_timer)        OS_USB_LOCK_INIT(&((_timer)->hal_gen_timer_lock))
#define ATH_GENTIMER_HAL_LOCK_DESTROY(_timer)     OS_USB_LOCK_DESTROY(&((_timer)->hal_gen_timer_lock))
#define ATH_GENTIMER_HAL_LOCK(_timer)             OS_USB_LOCK(&((_timer)->hal_gen_timer_lock))   
#define ATH_GENTIMER_HAL_UNLOCK(_timer)           OS_USB_UNLOCK(&((_timer)->hal_gen_timer_lock))
#else
typedef spinlock_t ath_hwtimer_lock_t;

#define ATH_GENTIMER_LOCK_INIT(_timer)            spin_lock_init(&((_timer)->gen_timer_lock))
#define ATH_GENTIMER_LOCK_DESTROY(_timer)         spin_lock_destroy(&((_timer)->gen_timer_lock))
#define ATH_GENTIMER_LOCK(_timer)                 spin_lock(&((_timer)->gen_timer_lock))
#define ATH_GENTIMER_UNLOCK(_timer)               spin_unlock(&((_timer)->gen_timer_lock))

#define ATH_GENTIMER_HAL_LOCK_INIT(_timer)        spin_lock_init(&((_timer)->hal_gen_timer_lock))
#define ATH_GENTIMER_HAL_LOCK_DESTROY(_timer)     spin_lock_destroy(&((_timer)->hal_gen_timer_lock))
#define ATH_GENTIMER_HAL_LOCK(_timer)             spin_lock(&((_timer)->hal_gen_timer_lock))   
#define ATH_GENTIMER_HAL_UNLOCK(_timer)           spin_unlock(&((_timer)->hal_gen_timer_lock))
#endif

/*
 * Generic TSF Timer defitions
 */
#define ATH_MAX_GEN_TIMER   16  /* XXX: an arbitrary number */
#define ATH_MASK_SIZE       howmany(ATH_MAX_GEN_TIMER, NBBY)

struct ath_gen_timer {
    ath_hwtimer_function    trigger;
    ath_hwtimer_function    overflow;
    ath_hwtimer_function    outofrange;
    void                    *arg;
    int                     index;      /* hardware generic timer index */
    HAL_GEN_TIMER_DOMAIN    domain;     /* TSF domain for the timer */
    struct {
        bool                active;
        u_int32_t           timer_next;
        u_int32_t           period;
    } cached_state;
};

struct ath_gen_timer_table {
    struct ath_gen_timer    *timers[ATH_MAX_GEN_TIMER];
    union {
        u_int8_t            bits[ATH_MASK_SIZE];
        u_int16_t           val;
    } timer_mask;
    ath_hwtimer_lock_t      gen_timer_lock;

    /* Sync hardware access. Should be moved into HAL */
    ath_hwtimer_lock_t      hal_gen_timer_lock;
};

struct ath_softc;
int ath_gen_timer_attach(struct ath_softc *sc);
void ath_gen_timer_detach(struct ath_softc *sc);
struct ath_gen_timer *ath_gen_timer_alloc(struct ath_softc *sc,
                                          HAL_GEN_TIMER_DOMAIN tsf,
                                          ath_hwtimer_function trigger_action,
                                          ath_hwtimer_function overflow_action,
                                          ath_hwtimer_function outofrange_action,
                                          void *arg);
void ath_gen_timer_free(struct ath_softc *sc, struct ath_gen_timer *timer);
void ath_gen_timer_start(struct ath_softc *sc, struct ath_gen_timer *timer,
                       u_int32_t timer_next, u_int32_t period);
void ath_gen_timer_stop(struct ath_softc *sc, struct ath_gen_timer *timer);
#ifdef ATH_USB
void ath_gen_timer_isr(struct ath_softc *sc, u_int32_t trigger_mask, u_int32_t thresh_mask, u_int32_t curr_tsf);
#else
void ath_gen_timer_isr(struct ath_softc *sc);
#endif
u_int32_t ath_gen_timer_gettsf32(struct ath_softc *sc, struct ath_gen_timer *timer);

void ath_gen_timer_tsfoor_isr(struct ath_softc *sc);
void ath_gen_timer_post_reset(struct ath_softc *sc);

struct ath_gen_timer *ath_tsf_timer_alloc(ath_dev_t dev, int tsf_id,
                                            ath_hwtimer_function trigger_action,
                                            ath_hwtimer_function overflow_action,
                                            ath_hwtimer_function outofrange_action,
                                            void *arg);

void ath_tsf_timer_free(ath_dev_t dev, struct ath_gen_timer *timer);

void ath_tsf_timer_start(ath_dev_t dev, struct ath_gen_timer *timer,
                            u_int32_t timer_next, u_int32_t period);


void ath_tsf_timer_stop(ath_dev_t dev, struct ath_gen_timer *timer);

#endif /* _DEV_ATH_HWTIMER_H */
