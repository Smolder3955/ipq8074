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
 * Public Interface for RfKill module
 */

#ifndef _DEV_ATH_RFKILL_H
#define _DEV_ATH_RFKILL_H

#include "ath_timer.h"

struct ath_rfkill_info {
    u_int16_t           rf_gpio_select;
    u_int16_t           rf_gpio_polarity;
    HAL_BOOL            rf_hasint;
    HAL_BOOL            rf_phystatechange;
    HAL_BOOL            delay_chk_start;    /* This flag is used by the WAR for
                                             * RfKill Delay; used to skip the
                                             * WAR check for first system boot.
                                             */
    struct ath_timer    rf_timer;
};

#ifdef ATH_RFKILL
HAL_BOOL ath_get_rfkill(struct ath_softc *sc);
#else
#define ath_get_rfkill(_sc) (AH_FALSE)
#endif

int ath_rfkill_attach(struct ath_softc *sc);
void ath_rfkill_detach(struct ath_softc *sc);
void ath_rfkill_start_polling(struct ath_softc *sc);
void ath_rfkill_stop_polling(struct ath_softc *sc);
HAL_BOOL ath_rfkill_gpio_isr(struct ath_softc *sc);
void ath_rfkill_gpio_intr(struct ath_softc *sc);
void ath_rfkill_delay_detect(struct ath_softc *sc);
int ath_rfkill_poll(struct ath_softc *sc);
void ath_enable_RFKillPolling(ath_dev_t dev, int enable);

#define ath_rfkill_hasint(_sc)  ((_sc)->sc_rfkill.rf_hasint)

#endif /* _DEV_ATH_RFKILL_H */
