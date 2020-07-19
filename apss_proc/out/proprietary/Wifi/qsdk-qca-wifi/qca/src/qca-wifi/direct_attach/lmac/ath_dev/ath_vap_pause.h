/*
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
/*
 * Definitions for the ATH  VAP Pause module 
 */
#ifndef ATH_VAP_PAUSE_H
#define ATH_VAP_PAUSE_H

#if ATH_VAP_PAUSE_SUPPORT

void ath_tx_vap_pause_control(struct ath_softc *sc,  struct ath_vap *avp, bool pause );

#define	   ATH_VAP_PAUSE_LOCK_INIT(_sc)    spin_lock_init(&(_sc)->sc_vap_pause_lock)
#define    ATH_VAP_PAUSE_LOCK(_sc)         spin_lock(&(_sc)->sc_vap_pause_lock)
#define    ATH_VAP_PAUSE_UNLOCK(_sc)       spin_unlock(&(_sc)->sc_vap_pause_lock)
#define	   ATH_VAP_PAUSE_LOCK_DESTROY(_sc) spin_lock_destroy(&(_sc)->sc_vap_pause_lock)

#define    VAP_PAUSE_SYNCHRONIZATION_TIMEOUT  1000  /* msec */

/*
 * called by functions (TX Path) trying to Queue frames to HW queue.
 * increment tx use and synchronize with
 * vap pause. 
 * 1) wait if vap pause is in progress  .
 * 2) when no vap pause is in progress increment use count and return.
 * Note the vap pause request( look at ath_vap_pause.c) will similarly
 * wait for the tx use count to become zero before setting vap_pause_in_progress
 * flag and start pausing the HW Queues.
 */
#define ath_vap_pause_txq_use_inc(_sc) do  { \
    int                 iter_count=0;                          \
    systime_t           start_timestamp  = OS_GET_TIMESTAMP(); \
    systime_t           cur_timestamp ;                        \
    (_sc)->sc_vap_pause_timeout = 0;                            \
    while(atomic_read(&(_sc)->sc_vap_pause_in_progress)) {      \
        OS_DELAY(10);                            \
        ++ iter_count;                           \
        if ((iter_count % 1000) == 0) {          \
          cur_timestamp  = OS_GET_TIMESTAMP();   \
          if (CONVERT_SYSTEM_TIME_TO_MS(cur_timestamp - start_timestamp) >= VAP_PAUSE_SYNCHRONIZATION_TIMEOUT) { \
             (_sc)->sc_vap_pause_timeout = 1; \
             break;                           \
          }                                      \
        }                                        \
    }                                            \
    atomic_inc(&(_sc)->sc_txq_use_cnt);          \
} while(0)
 
/*
 * decrement tx use and synchronize with
 * vap pause.
 */
#define ath_vap_pause_txq_use_dec(_sc) do {           \
    atomic_dec(&(_sc)->sc_txq_use_cnt);              \
} while(0)


/* return if ath vap pause is in progress */
#define ath_vap_pause_in_progress(_sc) atomic_read(&(_sc)->sc_vap_pause_in_progress)

#else

#define ath_vap_pause_txq_use_inc(_sc)         /**/ 
#define ath_vap_pause_txq_use_dec(_sc)         /**/ 
#define ath_tx_vap_pause_control(sc,avp,pause) /**/
#define	   ATH_VAP_PAUSE_LOCK_INIT(_sc)    /**/  
#define    ATH_VAP_PAUSE_LOCK(_sc)         /**/
#define    ATH_VAP_PAUSE_UNLOCK(_sc)       /**/
#define	   ATH_VAP_PAUSE_LOCK_DESTROY(_sc) /**/
#define    ath_vap_pause_in_progress(_sc)  AH_FALSE 

#endif /*  ATH_VAP_PAUSE_SUPPORT */

#endif /* ATH_VAP_PAUSE_H */
