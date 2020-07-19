/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */

#ifndef _IEEE80211_EXTACS_H
#define _IEEE80211_EXTACS_H

#define NUM_MAX_CHANNELS    (255)

/**
 * struct scan_chan_info - channel info
 * @freq:           radio frequence
 * @cmd flag:       cmd flag
 * @noise_floor:    noise floor
 * @cycle_count:    cycle count
 * @rx_clear_count: rx clear count
 * @tx_frame_count: TX frame count
 * @clock_freq:     clock frequence MHZ
 * @tx_power_tput:  Tx power used for the channel, with emphasis on throughput
 * @tx_power_range: Tx power used for the channel, with emphasis on range
 */
struct scan_chan_info {
    uint32_t freq;
    uint32_t cmd_flag;
    uint32_t noise_floor;
    uint32_t cycle_count;
    uint32_t rx_clear_count;
    uint32_t tx_frame_count;
    uint32_t clock_freq;
    int chan_load;
    int32_t  tx_power_tput;
    int32_t  tx_power_range;
};

struct external_acs_obj {
    u_int8_t              icm_active;  /* Whether Intelligent Channel Manager
                                                is active. */
    qdf_spinlock_t        chan_info_lock;
    struct scan_chan_info chan_info[NUM_MAX_CHANNELS];
};

void ieee80211_extacs_record_schan_info(struct external_acs_obj *extacs_handle,
                                        struct scan_chan_info *info,
                                       int chan_num);
#endif
