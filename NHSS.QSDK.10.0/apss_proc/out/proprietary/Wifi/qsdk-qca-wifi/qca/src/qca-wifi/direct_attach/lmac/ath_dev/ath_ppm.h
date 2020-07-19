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
 * Public Interface for LED control module
 */

/*
 * Definitions for the Atheros LED control module.
 */
#ifndef _DEV_ATH_PPM_H
#define _DEV_ATH_PPM_H

#include "ath_timer.h"


#define ATH_FORCE_PPM_PERIOD                     1000  /* ms, support timer period. Def 1 second */
#define ATH_FORCE_PPM_UPDATE_TIMEOUT             2000  /* ms, between sample window updates */
#define ATH_FORCE_PPM_INACTIVITY_TIMEOUT        20000  /* ms, wd timeout for rx frames */

#define ATH_FORCE_PPM_RECOVERY_TIMEOUT         0x3000  /* us, recovery timeout.  Def 0x3000=12288 (12 ms) */
 
enum ath_force_ppm_event_t {
    ATH_FORCE_PPM_DISABLE,
    ATH_FORCE_PPM_ENABLE,
    ATH_FORCE_PPM_SUSPEND,
    ATH_FORCE_PPM_RESUME,

    ATH_FORCE_PPM_NUMBER_EVENTS
};

#ifdef ATH_USB
#define	ATH_PPM_LOCK_INIT(_afp)             OS_USB_LOCK_INIT(&(_afp)->ppmLock)
#define	ATH_PPM_LOCK_DESTROY(_afp)          OS_USB_LOCK_DESTROY(&(_afp)->ppmLock)
#define	ATH_PPM_LOCK(_afp)                  OS_USB_LOCK(&(_afp)->ppmLock)     
#define	ATH_PPM_UNLOCK(_afp)                OS_USB_UNLOCK(&(_afp)->ppmLock)
#else
#define	ATH_PPM_LOCK_INIT(_afp)             spin_lock_init(&(_afp)->ppmLock)
#define	ATH_PPM_LOCK_DESTROY(_afp)
#define	ATH_PPM_LOCK(_afp)                  spin_lock(&(_afp)->ppmLock)
#define	ATH_PPM_UNLOCK(_afp)                spin_unlock(&(_afp)->ppmLock)
#endif

#if ATH_SUPPORT_FORCE_PPM
/*
 * Force PPM tracking workaround
 */
typedef struct ath_force_ppm {
    osdev_t             osdev;                        // pointer to OS device
    struct ath_softc    *sc;                          // pointer to sc
    struct ath_hal      *ah;                          // pointer to HAL object
#ifdef ATH_USB
    usblock_t           ppmLock;
#else
    spinlock_t          ppmLock;                      // Synchronization object
#endif

    u_int8_t            ppmActive;

    struct ath_timer    timer;
    int                 timer_running;

    int                 timerStart1;
    u_int32_t           timerCount1;
    u_int32_t           timerThrsh1;
    int32_t             timerStart2;
    u_int32_t           timerCount2;
    u_int32_t           timerThrsh2;
    u_int32_t           lastTsf1;
    u_int32_t           lastTsf2;
    u_int32_t           latchedRssi;
    int                 forceState;
    HAL_BOOL            isConnected;
    HAL_BOOL            isSuspended;
    int                forcePpmEnable;
    u_int8_t            bssid[IEEE80211_ADDR_LEN];  
    u_int32_t           force_ppm_update_timeout;
    u_int32_t           force_ppm_inactivity_timeout;
    u_int32_t           force_ppm_recovery_timeout;
} ath_force_ppm_t;

void ath_force_ppm_attach            (ath_force_ppm_t      *afp, 
                                      struct ath_softc     *sc, 
                                      osdev_t              sc_osdev,
                                      struct ath_hal       *ah,
                                      struct ath_reg_parm  *pRegParam);
void ath_force_ppm_start             (ath_force_ppm_t      *afp);
void ath_force_ppm_halt              (ath_force_ppm_t      *afp);
int  ath_force_ppm_logic             (ath_force_ppm_t      *afp, 
                                      struct ath_buf       *bf, 
                                      HAL_STATUS           status, 
                                      struct ath_rx_status *rx_stats);
void ath_force_ppm_notify            (ath_force_ppm_t      *afp,
                                      int                  event,
                                      u_int8_t             *bssid);

#else
typedef struct ath_force_ppm { u_int8_t dummy;} ath_force_ppm_t;
#define ath_force_ppm_attach(afp,sc,sc_osdev,ah,pRegParam)  /* */
#define ath_force_ppm_start(afp) /* */
#define ath_force_ppm_halt(afp) /* */
#define ath_force_ppm_logic(afp,bf,status,rx_stats)  /* */ 
#define ath_force_ppm_notify(afp,event,bssid)  (sc=sc) 

#endif /* ATH_SUPPORT_FORCE_PPM */

#endif /* _DEV_ATH_PPM_H */
