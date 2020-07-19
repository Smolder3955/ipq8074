/*
 * Copyright (c) 2006 Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */

/*
 * Implementation for hardware timer, including generic TSF timer
 * and future low frequecy timer.
 */

#include "ath_internal.h"
#include "ath_hwtimer.h"

#ifdef ATH_GEN_TIMER

static void debruijn32_setup(void);

int
ath_gen_timer_attach(struct ath_softc *sc)
{
    struct ath_gen_timer_table *timer_table = &sc->sc_gen_timers;

    OS_MEMZERO(timer_table, sizeof(struct ath_gen_timer_table));

    ATH_GENTIMER_LOCK_INIT(timer_table);
    ATH_GENTIMER_HAL_LOCK_INIT(timer_table);

    debruijn32_setup();
    
    return 0;
}

void
ath_gen_timer_detach(struct ath_softc *sc)
{
    struct ath_gen_timer_table *timer_table = &sc->sc_gen_timers;
    int i;
    UNREFERENCED_PARAMETER(timer_table);
    for (i = 0; i < ATH_MAX_GEN_TIMER; i++) {
        ASSERT(timer_table->timers[i] == NULL);
    }
    
    ATH_GENTIMER_LOCK_DESTROY(timer_table);
    ATH_GENTIMER_HAL_LOCK_DESTROY(timer_table);
}

static struct ath_gen_timer *
ath_gen_timer_alloc2(struct ath_softc *sc,
                    HAL_GEN_TIMER_DOMAIN tsf,
                    ath_hwtimer_function trigger_action,
                    ath_hwtimer_function overflow_action,
                    ath_hwtimer_function outofrange_action,
                    void *arg)
{
    struct ath_gen_timer_table *timer_table = &sc->sc_gen_timers;
    struct ath_gen_timer *timer;
    int index;
    
    /* allocate generic timer */
    timer = (struct ath_gen_timer *) OS_MALLOC(sc->sc_osdev,
                                               sizeof(struct ath_gen_timer),
                                               GFP_KERNEL);
    if (timer == NULL) {
        printk("%s: can't alloc timer object\n", __func__);
        return NULL;
    }
    OS_MEMZERO(timer, sizeof(struct ath_gen_timer));
    
    /* allocate a hardware generic timer slot */
    ATH_GENTIMER_HAL_LOCK(timer_table);
    index = ath_hal_gentimer_alloc(sc->sc_ah, tsf);
    ATH_GENTIMER_HAL_UNLOCK(timer_table);
    if (index < 0) {
        printk("%s: no hardware generic timer slot available\n", __func__);
        OS_FREE(timer);
        return NULL;
    }

    timer->index = index;
    timer->trigger = trigger_action;
    timer->overflow = overflow_action;
    timer->outofrange = outofrange_action;
    timer->arg = arg;
    timer->domain = tsf;
    timer->cached_state.active = false;
    timer->cached_state.timer_next = -1;
    timer->cached_state.period = -1;

    ATH_GENTIMER_LOCK(timer_table);
    timer_table->timers[index] = timer;
    ATH_GENTIMER_UNLOCK(timer_table);
    
    return timer;
}

struct ath_gen_timer *
ath_gen_timer_alloc(struct ath_softc *sc,
                    HAL_GEN_TIMER_DOMAIN tsf,
                    ath_hwtimer_function trigger_action,
                    ath_hwtimer_function overflow_action,
                    ath_hwtimer_function outofrange_action,
                    void *arg)
{
    struct ath_gen_timer *timer;

    if (tsf == HAL_GEN_TIMER_TSF_ANY)
    {
        /*
         * For Kiwi and Osprey, there are 8 gen timers of TSF2 available. Try TSF2 first,
         * if returns NULL. Try TSF for Kite and Merlin.
         */
        timer = ath_gen_timer_alloc2(sc, 
                                    HAL_GEN_TIMER_TSF2,
                                    trigger_action,
                                    overflow_action,
                                    outofrange_action,
                                    arg);

        if (timer == NULL) {
            timer = ath_gen_timer_alloc2(sc, 
                                        HAL_GEN_TIMER_TSF,
                                        trigger_action,
                                        overflow_action,
                                        outofrange_action,
                                        arg);
        }
    }
    else {
        timer = ath_gen_timer_alloc2(sc, 
                                    tsf,
                                    trigger_action,
                                    overflow_action,
                                    outofrange_action,
                                    arg);
    }
       
    return timer;
}

void
ath_gen_timer_free(struct ath_softc *sc, struct ath_gen_timer *timer)
{
    struct ath_gen_timer_table *timer_table = &sc->sc_gen_timers;

    if (!sc->sc_invalid) {
        /* free the hardware generic timer slot */
        ATH_GENTIMER_HAL_LOCK(timer_table);
        /* Make sure that the time is already stopped */
        ASSERT(isclr(timer_table->timer_mask.bits, timer->index));
        ath_hal_gentimer_free(sc->sc_ah, timer->index);
        ATH_GENTIMER_HAL_UNLOCK(timer_table);
    }

    ATH_GENTIMER_LOCK(timer_table);
    timer_table->timers[timer->index] = NULL;
    ATH_GENTIMER_UNLOCK(timer_table);
    
    OS_FREE(timer);    
}

/* 
 * NB: this function could be called in any IRQL context
 * When period parameter is 0, this timer is for one-shot and
 * when period is non-zero, this timer is periodic.
 */
void
ath_gen_timer_start(struct ath_softc *sc, struct ath_gen_timer *timer,
                  u_int32_t timer_next, u_int32_t period)
{
    struct ath_gen_timer_table *timer_table = &sc->sc_gen_timers;
#ifndef ATH_USB
    struct ath_hal *ah = sc->sc_ah;
#endif

    /* Note: the period is 28 bits in the hardware */
    #define LARGEST_TIME_PERIOD     (((u_int32_t)0x0FFFFFFF)/2 - 1)

    ASSERT(period <= LARGEST_TIME_PERIOD);

    /*
     *
     * The setbit macro does not work in a big endian processor, since the
     * reading is done as short
     * setbit(timer_table->timer_mask.bits, timer->index);
     *
     */

    timer_table->timer_mask.val |= (1 << timer->index);
    
    if (period == 0) {
        // A one shot timer. Set period to the largest value.
        period = LARGEST_TIME_PERIOD;
    }
    #undef LARGEST_TIME_PERIOD

    timer->cached_state.timer_next = timer_next;
    timer->cached_state.period = period;
    timer->cached_state.active = true;
    
    ATH_PS_WAKEUP(sc);
#ifdef ATH_USB    
    ATH_GENTIMER_HAL_LOCK(timer_table);
    setbit(timer_table->timer_mask.bits, timer->index);
    ath_wmi_set_generic_timer(sc, timer->index, timer_next, period, 1);    
    sc->sc_imask |= HAL_INT_GENTIMER;
    ATH_GENTIMER_HAL_UNLOCK(timer_table);
#else
    ATH_GENTIMER_HAL_LOCK(timer_table);
    ath_hal_gentimer_start(ah, timer->index, timer_next, period);
    setbit(timer_table->timer_mask.bits, timer->index);
    
    ATH_GENTIMER_HAL_UNLOCK(timer_table);
#endif   
    ATH_PS_SLEEP(sc);
}

/* NB: this function could be called in any IRQL context */
void
ath_gen_timer_stop(struct ath_softc *sc, struct ath_gen_timer *timer)
{
    struct ath_gen_timer_table *timer_table = &sc->sc_gen_timers;
#ifndef ATH_USB
    struct ath_hal *ah = sc->sc_ah;
#endif

    if (timer->cached_state.active != true) {
       /*
        * if the timer is already stopped, ignore the call.
        * if we assert here then all the callers need to keep track of
        * current state of the time which is an unnecessary overhead.
        */
       return;
    }

    ATH_PS_WAKEUP(sc);
#ifdef ATH_USB
    ATH_GENTIMER_HAL_LOCK(timer_table);
    clrbit(timer_table->timer_mask.bits, timer->index);
    ath_wmi_set_generic_timer(sc, timer->index, 0, 0, 0);
    sc->sc_imask &= ~HAL_INT_GENTIMER;
    ATH_GENTIMER_HAL_UNLOCK(timer_table);
#else
    ATH_GENTIMER_HAL_LOCK(timer_table);
    ath_hal_gentimer_stop(ah, timer->index);
    clrbit(timer_table->timer_mask.bits, timer->index);

    ATH_GENTIMER_HAL_UNLOCK(timer_table);
#endif   
    ATH_PS_SLEEP(sc);

    timer->cached_state.active = false;
    timer->cached_state.timer_next = -1;
    timer->cached_state.period = -1;
}

u_int32_t ath_gen_timer_gettsf32(struct ath_softc *sc, struct ath_gen_timer *timer)
{
    struct ath_hal *ah = sc->sc_ah;

    if (timer->domain == HAL_GEN_TIMER_TSF2) {
        return ath_hal_gettsf2_32(ah);
    }
    else {
        return ath_hal_gettsf32(ah);
    }
}

/*
 * Using de Bruijin sequence to to look up 1's index in a 32 bit number
 */
#define debruijn32 0x077CB531U /* debruijn32 = 0000 0111 0111 1100 1011 0101 0011 0001 */

static u_int32_t index32[32];

/* routine to initialize index32 */
static void
debruijn32_setup(void)
{
    int i;
    /* copy this into a local variable to enforce that the
     * variable is 32-bits in size. 0x077CB531UL (as it was
     * before this change) is 32-bit on LLP64 systems (win64),
     * but 64-bit on LP64 systems (64-bit darwin, 64-bit linux)
     */
    u_int32_t debruijn32_local = (u_int32_t)debruijn32;
    for(i=0; i<32; i++)
        index32[(debruijn32_local << i) >> 27] = i;
}

/* compute and clear index of rightmost 1 */
static INLINE int
rightmost_index(u_int32_t *mask)
{
    u_int32_t b;

    b = *mask;
    b &= (0-b);
    *mask &= ~b;
    b *= debruijn32;
    b >>= 27;
    
    return index32[b];
}

/*
 * Generic Timer Interrupts handling
 * NB: this function must be called in tasklet due to spin lock
 */
#ifdef ATH_USB
void
ath_gen_timer_isr(struct ath_softc *sc, u_int32_t trigger_mask, u_int32_t thresh_mask, u_int32_t curr_tsf)
{
    struct ath_gen_timer_table *timer_table = &sc->sc_gen_timers;
    struct ath_gen_timer *timer;
    int index;

    /* To get the timer trigger mask/threshold and current tsf value from WMI event.
     * This could reduce the delay of multiple wmi commands/events
     */

    trigger_mask &= timer_table->timer_mask.val;
    thresh_mask &= timer_table->timer_mask.val;

    /*
     * XXX: when timer overflows, it usually means TSF drifts too far.
     * Instead of invoking normal timer actions, invoke overflow action.
     */
    trigger_mask &= ~thresh_mask; /* only loop through the timers that do not overflows */

    ATH_GENTIMER_LOCK(timer_table);
    while (thresh_mask) {
        index = rightmost_index(&thresh_mask);
        timer = timer_table->timers[index];
        if (timer) {
            clrbit(timer_table->timer_mask.bits, index);
            timer->cached_state.active = false;
            timer->cached_state.timer_next = -1;
            timer->cached_state.period = -1;
            timer->overflow(timer->arg);
        }
    }

    while (trigger_mask) {
        index = rightmost_index(&trigger_mask);
        timer = timer_table->timers[index];
        if (timer) {
            clrbit(timer_table->timer_mask.bits, index);
            timer->cached_state.active = false;
            timer->cached_state.timer_next = -1;
            timer->cached_state.period = -1;
            timer->trigger(timer->arg);
        }
    }
    ATH_GENTIMER_UNLOCK(timer_table);
}
#else
void
ath_gen_timer_isr(struct ath_softc *sc)
{
    struct ath_gen_timer_table *timer_table = &sc->sc_gen_timers;
    struct ath_gen_timer *timer;
    u_int32_t  trigger_mask, thresh_mask;
    int index;

   
    /* To get the timer trigger mask/threshold and current tsf value from WMI event.
     * This could reduce the delay of multiple wmi commands/events
     */
    /* get hardware generic timer interrupt status */
    ATH_PS_WAKEUP(sc);
    ath_hal_gentimer_getintr(sc->sc_ah, &trigger_mask, &thresh_mask);
    ATH_PS_SLEEP(sc);

    trigger_mask &= timer_table->timer_mask.val;
    thresh_mask &= timer_table->timer_mask.val;
    /*
     * XXX: when timer overflows, it usually means TSF drifts too far.
     * Instead of invoking normal timer actions, invoke overflow action.
     */
    trigger_mask &= ~thresh_mask; /* only loop through the timers that do not overflows */

    ATH_GENTIMER_LOCK(timer_table);
    while (thresh_mask) {
        index = rightmost_index(&thresh_mask);
        timer = timer_table->timers[index];
        if (timer) {
            timer->overflow(timer->arg);
        }
    }

    while (trigger_mask) {
        index = rightmost_index(&trigger_mask);
        timer = timer_table->timers[index];
        if (timer) {
            timer->trigger(timer->arg);
        }
    }
    ATH_GENTIMER_UNLOCK(timer_table);
}
#endif

/*
 * TSF Timer Out-Of-Range Interrupts handling
 * NB: this function will be called in DPC context
 */
void
ath_gen_timer_tsfoor_isr(struct ath_softc *sc)
{
    struct ath_gen_timer_table *timer_table = &sc->sc_gen_timers;
    int                         i;

    ATH_GENTIMER_LOCK(timer_table);
    for (i = 0; i < ATH_MAX_GEN_TIMER; i++) {
        if (timer_table->timers[i] != NULL) {
            struct ath_gen_timer *timer;

            timer = timer_table->timers[i];
            if (timer->outofrange != NULL) {
                timer->outofrange(timer->arg);
            }
        }
    }
    ATH_GENTIMER_UNLOCK(timer_table);
}

/*
 * Called after a reset is done to the hardware. We should informed
 * the generic timer users that the TSF is disrupted.
 */
void
ath_gen_timer_post_reset(struct ath_softc *sc)
{
    int                         i;
    struct ath_gen_timer_table *timer_table = &sc->sc_gen_timers;
    struct ath_hal *ah = sc->sc_ah;

    ATH_GENTIMER_LOCK(timer_table);
    for (i = 0; i < ATH_MAX_GEN_TIMER; i++) {
     if (timer_table->timers[i] != NULL) {
        struct ath_gen_timer *timer;
            timer = timer_table->timers[i];
            if (timer->cached_state.active == true) {
                ATH_GENTIMER_HAL_LOCK(timer_table);
                ath_hal_gentimer_start(ah, timer->index, timer->cached_state.timer_next, 
                    timer->cached_state.period);
                setbit(timer_table->timer_mask.bits, timer->index);
                ATH_GENTIMER_HAL_UNLOCK(timer_table);
            }
        }
    }
    ATH_GENTIMER_UNLOCK(timer_table);
}

struct ath_gen_timer *
ath_tsf_timer_alloc(ath_dev_t dev, int tsf_id,
                    ath_hwtimer_function trigger_action,
                    ath_hwtimer_function overflow_action,
                    ath_hwtimer_function outofrange_action,
                    void *arg)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    
    return(ath_gen_timer_alloc(sc, tsf_id, trigger_action, overflow_action, outofrange_action, arg));
}

void
ath_tsf_timer_free(ath_dev_t dev, struct ath_gen_timer *timer)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    ath_gen_timer_free(sc, timer);
}

void
ath_tsf_timer_start(ath_dev_t dev, struct ath_gen_timer *timer,
                            u_int32_t timer_next, u_int32_t period)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    ath_gen_timer_start(sc, timer, timer_next, period);
}


void
ath_tsf_timer_stop(ath_dev_t dev, struct ath_gen_timer *timer)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    ath_gen_timer_stop(sc, timer);
}

#endif /* end of ATH_GEN_TIMER */
