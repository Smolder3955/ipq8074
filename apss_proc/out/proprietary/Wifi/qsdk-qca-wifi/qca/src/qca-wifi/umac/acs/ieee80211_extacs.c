/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */

#include <osdep.h>
#include <ieee80211_var.h>
#include <ieee80211_channel.h>
#include <ol_if_athvar.h>
#include <ieee80211.h>
#include <ieee80211_extacs.h>
#include <ol_if_stats.h>

/* MACRO for finding modulo diff between 2 numbers */
#define CNT_DIFF(cur, prev) \
    ((cur >= prev) ? (cur - prev) : (cur + (MAX_COUNT - (prev) + 1)))
#define MAX_COUNT 0xffffffff

static void _record_schan_info(struct external_acs_obj *extacs_handle,
            struct scan_chan_info *chan,
            struct scan_chan_info *info, uint32_t cmd_flag)
{
    u_int32_t chan_load = ((1<<16)-1);
    u_int32_t clear_cnt = 0, cycle_cnt = 0, shift_fact = 0;

    if ((info->cmd_flag != WMI_CHAN_INFO_FLAG_START_RESP) &&
       (info->cmd_flag != WMI_CHAN_INFO_FLAG_END_RESP) &&
       (info->cmd_flag != WMI_CHAN_INFO_FLAG_END_DIFF))
        qdf_print("cmd flag is invalid: %d", info->cmd_flag);

    qdf_spin_lock(&extacs_handle->chan_info_lock);

    chan->freq = info->freq;
    chan->noise_floor = info->noise_floor;
    chan->clock_freq = info->clock_freq;
    chan->cmd_flag = info->cmd_flag;
    chan->tx_power_tput = info->tx_power_tput;
    chan->tx_power_range = info->tx_power_range;

    if (info->cmd_flag == WMI_CHAN_INFO_FLAG_START_RESP) {
        chan->cycle_count = info->cycle_count;
        chan->rx_clear_count = info->rx_clear_count;
        chan->tx_frame_count = info->tx_frame_count;
        goto end;
    }

    if (info->cmd_flag == WMI_CHAN_INFO_FLAG_END_DIFF) {
        chan->cycle_count    = info->cycle_count;
        chan->rx_clear_count = info->rx_clear_count;
        chan->tx_frame_count = info->tx_frame_count;
        clear_cnt = chan->rx_clear_count;
        cycle_cnt = chan->cycle_count;
    } else if (info->cmd_flag == WMI_CHAN_INFO_FLAG_END_RESP) {
        u_int32_t scanstart_cycle_cnt = chan->cycle_count;
        u_int32_t scanend_cycle_cnt = info->cycle_count;
        u_int32_t scanstart_clr_cnt = chan->rx_clear_count;
        u_int32_t scanend_clr_cnt = info->rx_clear_count;

        chan->cycle_count = CNT_DIFF(info->cycle_count, chan->cycle_count);
        chan->rx_clear_count =
                CNT_DIFF(info->rx_clear_count, chan->rx_clear_count);
        chan->tx_frame_count =
                CNT_DIFF(info->tx_frame_count, chan->tx_frame_count);

        /* Calculate chan load info for legacy */
        clear_cnt = scanend_clr_cnt - scanstart_clr_cnt;
        cycle_cnt = scanend_cycle_cnt - scanstart_cycle_cnt;

        /* The count could have overflowed, so it is important
        to check for this condition and correct for the same. */
        if (scanend_clr_cnt < scanstart_clr_cnt) {
            clear_cnt += 1 << 31;
        }
        if (scanend_cycle_cnt < scanstart_cycle_cnt) {
            cycle_cnt += 1 << 31;
        }

    }
    /* Take only the 16 significant bits */
    shift_fact = 16;
    if (cycle_cnt) {
        while ((cycle_cnt >= (1 << shift_fact)) && (shift_fact < 32))
        {
            shift_fact++;
        }
    }
    shift_fact -= 16;

    clear_cnt >>= shift_fact;
    cycle_cnt >>= shift_fact;

    if (cycle_cnt) {
        chan_load -= (clear_cnt *((1 << 16) - 1)) / cycle_cnt;
    } else {
        /* Cycle count cannot be zero. But if so, set the chan_load to
        zero */
        chan_load = 0;
    }
    chan->chan_load = chan_load;

end:
    qdf_spin_unlock(&extacs_handle->chan_info_lock);
}
#undef CNT_DIFF
#undef MAX_COUNT

/**
 * ieee80211_extacs_record_schan_info() - channel info callback
 * @extacs_handle: handle to external acs object
 * @info:        pointer to channel info input
 * @chan_num:    ieee channel number
 *
 * Store channel info into object for external acs
 *
 * Return: None.
 */
void ieee80211_extacs_record_schan_info(struct external_acs_obj *extacs_handle, struct scan_chan_info *info,
                                      int chan_num)
{
    struct scan_chan_info *chan;
    chan = extacs_handle->chan_info;

    if (chan_num >= NUM_MAX_CHANNELS) {
        return;
    }

    _record_schan_info(extacs_handle, &chan[chan_num], info,
        info->cmd_flag);
    qdf_print("chan_num:%d cmd:%d freq:%u nf:%d cc:%u rcc:%u clk:%u tfc:%d "
              "txpt:%d txpr:%d",
              chan_num, chan[chan_num].cmd_flag, chan[chan_num].freq,
              chan[chan_num].noise_floor,
              chan[chan_num].cycle_count, chan[chan_num].rx_clear_count,
              chan[chan_num].clock_freq,
              chan[chan_num].tx_frame_count,
              chan[chan_num].tx_power_tput,
              chan[chan_num].tx_power_range);
}

