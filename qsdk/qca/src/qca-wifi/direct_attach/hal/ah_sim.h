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
 
#ifndef _ATH_AH_SIM_H_
#define _ATH_AH_SIM_H_

#include "ah.h"

#if ATH_DRIVER_SIM

#define MAX_NUMBER_SIM_DEVICES 100

#define AHSIM_BREAKPOINT __asm int 3
#define AHSIM_ASSERT(_cond_fail_on_0)   \
    if (_cond_fail_on_0) {              \
    } else {                            \
        AHSIM_BREAKPOINT                \
    }
#define AHSIM_MON_PKT_BUFFER_SIZE 16384
#define AHSIM_MON_PKT_MAX_SIZE 1000 /* could use actual Rx limit but ... */
#define AHSIM_MON_PKT_NUMBER 20 /* ~AHSIM_MON_PKT_SIZE/AHSIM_MON_PKT_MAX_SIZE make sure always big enough so don't have to check limit in code */

#define AHSIM_INTER_SIM_BUFFER_SIZE 1048576

/*
struct ath_hal_sim is defined in ah.h. 
 
Need to put definition there since it uses definitions from that file and is used within that file (would have 
been better to split ah.h but ...). 
 
*/

#define AHSIM_REG_LOCK_INIT(__hal_sim)    spin_lock_init(&(__hal_sim)->reg_lock)
#define AHSIM_REG_LOCK_DESTROY(__hal_sim) spin_lock_destroy(&(__hal_sim)->reg_lock)
#define AHSIM_REG_LOCK(__hal_sim)         spin_lock(&(__hal_sim)->reg_lock)
#define AHSIM_REG_UNLOCK(__hal_sim)       spin_unlock(&(__hal_sim)->reg_lock)

#define AHSIM_TIMER_LOCK_INIT(__hal_sim)     spin_lock_init(&(__hal_sim)->tx_timer_lock) 
#define AHSIM_TIMER_LOCK_DESTROY(__hal_sim)  spin_lock_destroy(&(__hal_sim)->tx_timer_lock)
#define AHSIM_TIMER_LOCK(__hal_sim)          spin_lock(&(__hal_sim)->tx_timer_lock)
#define AHSIM_TIMER_UNLOCK(__hal_sim)        spin_unlock(&(__hal_sim)->tx_timer_lock)

#define AHSIM_OUTPUT_BUFFER_LOCK_INIT()    spin_lock_init(&AHSIM_sim_info.output_buffer_lock)
#define AHSIM_OUTPUT_BUFFER_LOCK_DESTROY() spin_lock_destroy(&AHSIM_sim_info.output_buffer_lock)
#define AHSIM_OUTPUT_BUFFER_LOCK()         spin_lock(&AHSIM_sim_info.output_buffer_lock)
#define AHSIM_OUTPUT_BUFFER_UNLOCK()       spin_unlock(&AHSIM_sim_info.output_buffer_lock)

#define AHSIM_INPUT_BUFFER_LOCK_INIT()    spin_lock_init(&AHSIM_sim_info.input_buffer_lock)
#define AHSIM_INPUT_BUFFER_LOCK_DESTROY() spin_lock_destroy(&AHSIM_sim_info.input_buffer_lock)
#define AHSIM_INPUT_BUFFER_LOCK()         spin_lock(&AHSIM_sim_info.input_buffer_lock)
#define AHSIM_INPUT_BUFFER_UNLOCK()       spin_unlock(&AHSIM_sim_info.input_buffer_lock)

bool AHSIM_register_sim_device_instance(u_int16_t devid, struct ath_hal *ah);
void AHSIM_unregister_sim_device_instance(struct ath_hal *ah);

void AHSIM_MonitorPktDataNodeIndex(u_int32_t node_index, struct ath_hal *ah, u_int8_t* alt_data);
void AHSIM_monitor_pkt_data_descriptor(u_int32_t num_bytes, u_int8_t* bytes);
void AHSIM_monitor_pkt_snippet(u_int32_t num_bytes, u_int8_t* bytes);
void AHSIM_monitor_pkt_filter_info(u_int32_t rx_filter, u_int32_t failed_filter);
void AHSIM_monitor_pkt_agg_per_info(u_int8_t agg_per, u_int8_t rv, u_int32_t agg_pkt_num);
void AHSIM_MonitorPktKey(u_int8_t* key_buffer);
void AHSIM_MonitorPktStall();

#define DBG_ATHEROS_MASK                0xFFFF0000
#define DBG_ATHEROS_DESC_ID             ((ATHEROS_VENDOR_ID << 16) ^ 0xFFFF0000)
#define DBG_ATHEROS_SNIPPET_ID          ((ATHEROS_VENDOR_ID << 16) ^ 0xF0F00000)
#define DBG_ATHEROS_FILTER_ID           ((ATHEROS_VENDOR_ID << 16) ^ 0xFF000000)
#define DBG_ATHEROS_CHANNEL_ID          ((ATHEROS_VENDOR_ID << 16) ^ 0x00FF0000)
#define DBG_ATHEROS_PER_ID              ((ATHEROS_VENDOR_ID << 16) ^ 0x0F0F0000)
#define DBG_ATHEROS_AGG_PER_ID          ((ATHEROS_VENDOR_ID << 16) ^ 0xFFF00000)
#define DBG_ATHEROS_KEY_ID              ((ATHEROS_VENDOR_ID << 16) ^ 0x0FFF0000)
#define DBG_ATHEROS_SIM_STALL_ID        ((ATHEROS_VENDOR_ID << 16) ^ 0xF00F0000)

void AHSIM_srand(u_int32_t x);
u_int32_t AHSIM_rand(u_int32_t min, u_int32_t max);

struct AHSIM_sim_info_t {
    /* sim structures */
    spinlock_t sim_info_lock;
    u_int32_t num_of_sim_devices;
    struct ath_hal* sim_devices[MAX_NUMBER_SIM_DEVICES];
    u_int32_t sim_eng_id;
    u_int32_t sim_mach_id;
    bool sim_stand_alone_mode;

    /* oid control info */
    bool output_mon_pkts;
    u_int32_t output_pkt_snippet_length;
    bool output_key;
      
    u_int8_t per_src_dest[MAX_NUMBER_SIM_DEVICES][MAX_NUMBER_SIM_DEVICES];
    u_int8_t agg_per_src_dest[MAX_NUMBER_SIM_DEVICES][MAX_NUMBER_SIM_DEVICES];

    bool sim_filter;
    bool sim_channels;

    bool break_on_packet; /* break-on-packet (bop) information */
    u_int32_t bop_type;
    u_int32_t bop_subtype;
    
    /* monitor data */
    u_int8_t mon_pkt[AHSIM_MON_PKT_BUFFER_SIZE];
    u_int32_t mon_pkt_entry_size[AHSIM_MON_PKT_NUMBER];
    u_int32_t mon_pkt_index;
    u_int32_t mon_pkt_entry_index;
    bool do_mon_pkt;

    /* inter-device simulation */
    bool inter_sim_promiscuous_mode; /* controls whether packets intended for local recipients are transmitted as well (so they can be monitored elsewhere) */
    spinlock_t output_buffer_lock;
    u_int8_t output_buffer[AHSIM_INTER_SIM_BUFFER_SIZE];
    u_int32_t output_buffer_num_bytes;
    bool output_buffer_overflow_indicator;

    spinlock_t input_buffer_lock;
    u_int8_t input_buffer[AHSIM_INTER_SIM_BUFFER_SIZE];
    u_int32_t input_buffer_num_bytes;
    
    /* misc internal simulator variables */
    u_int32_t ibss_beacon_sta_index;
};

extern struct AHSIM_sim_info_t AHSIM_sim_info;

#endif // ATH_DRIVER_SIM

#endif // _ATH_AH_SIM_H_
