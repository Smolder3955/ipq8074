/*
 * Copyright (c) 2009, Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */

/*
 * Public Interface for timer module
 */

/*
 * Definitions for the Atheros Timer module.
 */
#ifndef _DEV_ATH_TIMER_H
#define _DEV_ATH_TIMER_H

/* 
 * timer handler function. 
 *     Must return 0 if timer should be rearmed, or !0 otherwise.
 */

typedef int (*timer_handler_function) (void *context);

struct ath_timer {
    os_timer_t             os_timer;            // timer object
    u_int8_t               active_flag;         // indicates timer is running
    u_int32_t              timer_period;        // timer period
    void*                  context;             // execution context
    int (*timer_handler) (void*);               // handler function
    u_int32_t              signature;           // contains a signature indicating object has been initialized
};

/*
 * Specifies whether ath_cancel_timer can sleep while trying to cancel the timer.
 * Sleeping is allowed only if ath_cancel_timer is running at an IRQL that
 * supports changes in context and memory faults (i.e. Passive Level)
 */
enum timer_flags {
	CANCEL_SLEEP    = 0,
	CANCEL_NO_SLEEP = 1		
};

u_int8_t ath_initialize_timer_module (osdev_t sc_osdev);
u_int8_t ath_initialize_timer_int    (osdev_t                osdev,
                                      struct ath_timer*      timer_object, 
                                      u_int32_t              timer_period, 
                                      timer_handler_function timer_handler, 
                                      void*                  context);
void     ath_set_timer_period        (struct ath_timer* timer_object, u_int32_t timer_period);
u_int8_t ath_start_timer             (struct ath_timer* timer_object);
u_int8_t ath_cancel_timer            (struct ath_timer* timer_object, enum timer_flags flags);
u_int8_t ath_timer_is_active         (struct ath_timer* timer_object);
u_int8_t ath_timer_is_initialized    (struct ath_timer* timer_object);
void ath_free_timer_int(struct ath_timer* timer_object);

#if ATH_DEBUG_TIMERS
#define ath_initialize_timer   printk("%s [%d] creating a timer\n"\
                                    ,__func__,__LINE__), \
                                    ath_initialize_timer_int

#define ath_free_timer   printk("%s [%d] deleting a timer\n"\
                                ,__func__,__LINE__), \
                                ath_free_timer_int
#else
#define ath_initialize_timer    ath_initialize_timer_int
#define ath_free_timer          ath_free_timer_int
#endif

#ifdef ATH_HTC_MII_RXIN_TASKLET

typedef  struct timer_list  ath_rxtimer ;
u_int8_t ath_initialize_timer_rxtask        (osdev_t                osdev,
                                      ath_rxtimer       *   timer_object, 
                                      u_int32_t              timer_period, 
                                      timer_handler_function timer_handler, 
                                      void*                  context);
void     ath_set_timer_period_rxtask        (ath_rxtimer* timer_object, u_int32_t timer_period);
u_int8_t ath_start_timer_rxtask             (ath_rxtimer* timer_object);
u_int8_t ath_cancel_timer_rxtask            (ath_rxtimer* timer_object, enum timer_flags flags);
u_int8_t ath_timer_is_active_rxtask         (ath_rxtimer* timer_object);
void     ath_free_timer_rxtask(ath_rxtimer* timer_object);


#define ATH_RXTIMER_INIT(_osdev, _timer_object, _timer_period, _timer_handler, _context) \
                   ath_initialize_timer_rxtask(_osdev, _timer_object, _timer_period, _timer_handler, _context)
#define ATH_CANCEL_RXTIMER( _timer_object, _flags)       ath_cancel_timer_rxtask ( _timer_object, _flags)
#define ATH_FREE_RXTIMER(_timer_object)         ath_free_timer_rxtask(_timer_object)
#define ATH_RXTIMER_IS_ACTIVE( _timer_object)    ath_timer_is_active_rxtask( _timer_object)
#define ATH_SET_RXTIMER_PERIOD(_timer_object, _timer_period)   ath_set_timer_period_rxtask(_timer_object, _timer_period)
#define ATH_START_RXTIMER(_timer_object)        ath_start_timer_rxtask(_timer_object)

#else
typedef  struct ath_timer  ath_rxtimer ;
#define ATH_RXTIMER_INIT(_osdev, _timer_object, _timer_period, _timer_handler, _context) \
                   ath_initialize_timer(_osdev, _timer_object, _timer_period, _timer_handler, _context)
#define ATH_CANCEL_RXTIMER( _timer_object, _flags)       ath_cancel_timer ( _timer_object, _flags)
#define ATH_FREE_RXTIMER(_timer_object)         ath_free_timer(_timer_object)
#define ATH_RXTIMER_IS_ACTIVE( _timer_object)    ath_timer_is_active( _timer_object)
#define ATH_SET_RXTIMER_PERIOD(_timer_object, _timer_period)   ath_set_timer_period(_timer_object, _timer_period)
#define ATH_START_RXTIMER(_timer_object)        ath_start_timer(_timer_object)
#endif



#endif /* _DEV_ATH_TIMER_H */



