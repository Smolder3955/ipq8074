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
 * Public Interface for power control module
 */


#ifndef _DEV_ATH_POWER_H
#define _DEV_ATH_POWER_H

#include "ath_timer.h"

typedef enum {
        ATH_PWRSAVE_AWAKE=0,
        ATH_PWRSAVE_FULL_SLEEP,
        ATH_PWRSAVE_NETWORK_SLEEP,
} ATH_PWRSAVE_STATE;                /* slot time update fsm */

/*
 * PCI Express core vendor specific registers
 * ASPM (Active State Power Management)
 */
#define PCIE_CAP_ID                 0x10    /* PCIe Capability ID */
#define PCIE_CAP_LNKCTL_OFFSET      0x10    /* Link control offset in PCIE capability */ 
#define PCIE_CAP_OFFSET             0x60    /* PCIe cabability offset on Atheros Cards */
#define PCIE_CAP_LINK_CTRL          0x70    /* PCIe Link Capability */
#define PCIE_CAP_LINK_L0S           1       /* ASPM L0s */
#define PCIE_CAP_LINK_L1            2       /* ASPM L1 */
#define PCIE_CAP_LINK_EXT_SYNC      (1<<7)  /* Extended Sync bit */

#if ATH_SUPPORT_POWER

/*
 * Definitions for the Atheros power control module.
 */
#define ATH_MAX_SLEEP_TIME           10          /* # of beacon intervals to sleep in MAX PWRSAVE*/
#define ATH_NORMAL_SLEEP_TIME        1           /* # of beacon intervals to sleep in NORMAL PWRSAVE*/

#ifdef ATH_USB
#define	ATH_PS_LOCK_INIT(_sc)        OS_USB_LOCK_INIT(&(_sc)->sc_pwrsave.ps_pwrsave_lock)
#define	ATH_PS_LOCK_DESTROY(_sc)     OS_USB_LOCK_DESTROY(&(_sc)->sc_pwrsave.ps_pwrsave_lock)
#define	ATH_PS_LOCK(_sc)             OS_USB_LOCK(&(_sc)->sc_pwrsave.ps_pwrsave_lock)   
#define	ATH_PS_UNLOCK(_sc)           OS_USB_UNLOCK(&(_sc)->sc_pwrsave.ps_pwrsave_lock)
#define	ATH_PS_INIT_LOCK_INIT(_sc)   OS_USB_LOCK_INIT(&(_sc)->sc_pwrsave.ps_init_lock)
#define	ATH_PS_INITLOCK_DESTROY(_sc) OS_USB_LOCK_DESTROY(&(_sc)->sc_pwrsave.ps_init_lock)
#define	ATH_PS_INIT_LOCK(_sc)        OS_USB_LOCK(&(_sc)->sc_pwrsave.ps_init_lock)   
#define	ATH_PS_INIT_UNLOCK(_sc)      OS_USB_UNLOCK(&(_sc)->sc_pwrsave.ps_init_lock)
#ifdef ATH_SUPPORT_P2P
#define ATH_PS_ALWAYS_AWAKE(_sc)     ath_pwrsave_always_awake(_sc)
#else
#define ATH_PS_ALWAYS_AWAKE(_sc)     (0) /* do nothing */
#endif
#else
#define	ATH_PS_LOCK_INIT(_sc)        spin_lock_init(&(_sc)->sc_pwrsave.ps_pwrsave_lock)
#define	ATH_PS_LOCK_DESTROY(_sc)
#ifdef ATH_SUPPORT_LINUX_STA
#define	ATH_PS_LOCK(_sc)             spin_lock_dpc(&(_sc)->sc_pwrsave.ps_pwrsave_lock)
#define	ATH_PS_UNLOCK(_sc)           spin_unlock_dpc(&(_sc)->sc_pwrsave.ps_pwrsave_lock)
#else
#define	ATH_PS_LOCK(_sc)             os_tasklet_lock(&(_sc)->sc_pwrsave.ps_pwrsave_lock,  \
                                                     (_sc)->sc_pwrsave.ps_pwrsave_flags)
#define	ATH_PS_UNLOCK(_sc)           os_tasklet_unlock(&(_sc)->sc_pwrsave.ps_pwrsave_lock,\
                                                     (_sc)->sc_pwrsave.ps_pwrsave_flags)
#endif
#define	ATH_PS_INIT_LOCK_INIT(_sc)
#define	ATH_PS_INITLOCK_DESTROY(_sc)
#define	ATH_PS_INIT_LOCK(_sc)
#define	ATH_PS_INIT_UNLOCK(_sc)
#define ATH_PS_ALWAYS_AWAKE(_sc)     (0) /* do nothing */
#endif /* ATH_USB */

/*
 * macros to wake up and restore the HW powersave state
 * for temporary hal access.
 * when calling any HAL function that accesses registers,
 * the chip should be awake otherwise the PCI bus will hang.
 * use ATH_HAL_PS_WAKEUP to wakeup the chip before any hal
 * function calls that access registers.
 * use ATH_HAL_PS_SLEEP after all hal function calls to push
 * it to SLEEP state.
 */
#define ATH_PS_WAKEUP(sc) do {                                                 \
    ATH_PS_LOCK(sc);                                                           \
    if(!(sc)->sc_pwrsave.ps_hal_usecount) {                                    \
        (sc)->sc_pwrsave.ps_restore_state = (sc)->sc_pwrsave.ps_pwrsave_state; \
        ath_pwrsave_set_state((sc), ATH_PWRSAVE_AWAKE);                        \
    }                                                                          \
    (sc)->sc_pwrsave.ps_hal_usecount++;                                        \
    ATH_PS_UNLOCK(sc);                                                         \
  } while (0)

#define ATH_PS_SLEEP(sc) do {                                                  \
    ATH_PS_LOCK(sc);                                                           \
    (sc)->sc_pwrsave.ps_hal_usecount--;                                        \
    if(!(sc)->sc_pwrsave.ps_hal_usecount) {                                    \
        ath_pwrsave_set_state((sc), (sc)->sc_pwrsave.ps_restore_state);        \
    }                                                                          \
    ATH_PS_UNLOCK(sc);                                                         \
  } while (0)

#define ATH_FUNC_ENTRY_CHECK(sc, error)                                         \
    if(!((sc)->sc_sw_phystate && (sc)->sc_hw_phystate)) {                       \
       return (error);                                                          \
    }

#define ATH_FUNC_ENTRY_VOID(sc)	                                                \
    if(!((sc)->sc_sw_phystate && (sc)->sc_hw_phystate)) {                       \
       return;                                                                  \
    }


typedef struct _ath_pwrsave {
    u_int32_t               ps_hal_usecount;      /* number of hal users */
    ATH_PWRSAVE_STATE       ps_restore_state;     /* power save state to restore*/
    ATH_PWRSAVE_STATE	    ps_pwrsave_state;     /* power save state */
    ATH_PWRSAVE_STATE	    ps_set_state;         /* power save state to set*/
#ifdef ATH_USB
    usblock_t               ps_pwrsave_lock;
    usblock_t               ps_init_lock;         /* power save init lock */
#else
    spinlock_t              ps_pwrsave_lock;      /* softc-level lock */
    unsigned long           ps_pwrsave_flags;
#endif
} ath_pwrsave_t;

/*
 * Support interfaces
 */
#define ATH_PWRSAVE_CONTROL_OBJ(xxx) ath_pwrsave_t xxx 

void ath_pwrsave_set_state(ath_dev_t dev, int newstateint);
void ath_pwrsave_proc(struct ath_softc *sc);
int  ath_pwrsave_timer(struct ath_softc *sc);
void ath_pwrsave_init(struct ath_softc *sc);
void ath_pwrsave_awake(ath_dev_t);
void ath_pwrsave_netsleep(ath_dev_t);
void ath_pwrsave_fullsleep(ath_dev_t);
int  ath_pwrsave_get_state(ath_dev_t dev);
void ath_pwrsave_proc_intdone(struct ath_softc *sc, u_int32_t intrStatus);

#else /* ATH_SUPPORT_POWER */

#define ATH_PWRSAVE_CONTROL_OBJ(xxx) u_int8_t xxx /* */ 

#define ath_pwrsave_set_state(dev, newstateint)
#define ath_pwrsave_proc(sc)
#define ath_pwrsave_init(sc)
#define ath_pwrsave_awake(dev)
#define ath_pwrsave_netsleep(dev)
#define ath_pwrsave_fullsleep(dev)
#define ath_pwrsave_get_state(dev) 0
#define ath_pwrsave_proc_intdone(sc, intrStatus);

#define	ATH_PS_LOCK_INIT(_sc)
#define	ATH_PS_LOCK_DESTROY(_sc)
#define ATH_PS_WAKEUP(sc)
#define ATH_PS_SLEEP(sc)
#define ATH_FUNC_ENTRY_CHECK(sc, error)
#define ATH_FUNC_ENTRY_VOID(sc)

#endif /* ATH_SUPPORT_POWER */

#endif /* _DEV_ATH_POWER_H */

