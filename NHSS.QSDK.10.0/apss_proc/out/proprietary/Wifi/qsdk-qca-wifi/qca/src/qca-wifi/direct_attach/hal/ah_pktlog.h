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

#ifndef __AH_PKTLOG_H__
#define __AH_PKTLOG_H__

/* Parameter types for packet logging driver hooks */
struct log_ani {
    u_int8_t phy_stats_disable;
    u_int8_t noise_immun_lvl;
    u_int8_t spur_immun_lvl;
    u_int8_t ofdm_weak_det;
    u_int8_t cck_weak_thr;
    u_int16_t fir_lvl;
    u_int16_t listen_time;
    u_int32_t cycle_count;
    u_int32_t ofdm_phy_err_count;
    u_int32_t cck_phy_err_count;
    int8_t rssi;
    int32_t misc[8]; /* Can be used for HT specific or other misc info */
    /* TBD: Add other required log information */
};

#ifndef __ahdecl3
#ifdef __i386__
#define __ahdecl3 __attribute__((regparm(3)))
#else
#define __ahdecl3
#endif
#endif

#ifndef REMOVE_PKT_LOG
#if !NO_HAL
/*
 */
extern void __ahdecl ath_hal_log_ani_callback_register(
            void __ahdecl3 (*pktlog_ani)(void *, struct log_ani *, u_int32_t));
#else
static inline void __ahdecl ath_hal_log_ani_callback_register(
		void __ahdecl3 (*pktlog_ani)(void *, struct log_ani *, u_int32_t))
{
}
#endif
#else
#define ath_hal_log_ani_callback_register(func_ptr)
#endif /* REMOVE_PKT_LOG */

#endif /* __AH_PKTLOG_H__ */

