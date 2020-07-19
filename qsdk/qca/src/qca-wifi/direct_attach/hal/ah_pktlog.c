/*
* Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 * Copyright (c) 2008-2010, Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */

#include "opt_ah.h"

#include "ah.h"
#include "ah_internal.h"
#include "ah_pktlog.h"

#ifndef REMOVE_PKT_LOG

static void __ahdecl3
(*ath_hal_pktlog_ani) (HAL_SOFTC, struct log_ani *, u_int32_t) = AH_NULL;

#if !NO_HAL
void __ahdecl ath_hal_log_ani_callback_register(
    void __ahdecl3 (*pktlog_ani)(HAL_SOFTC, struct log_ani *, u_int32_t)
    )
{
    ath_hal_pktlog_ani = pktlog_ani;
}
#endif

void
ath_hal_log_ani(HAL_SOFTC sc, struct log_ani *log_data, u_int32_t iflags)
{
    if ( ath_hal_pktlog_ani ) {
        ath_hal_pktlog_ani(sc, log_data, iflags);
    }
}

#endif /* REMOVE_PKT_LOG */
