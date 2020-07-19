/*
* Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 *  Copyright (c) 2008 Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */

/*
 * vap tsf offset component.
 */
#ifndef _IEEE80211_VAP_TSF_OFFSET_H_
#define _IEEE80211_VAP_TSF_OFFSET_H_

#include <ieee80211_var.h>

#if UMAC_SUPPORT_VAP_TSF_OFFSET

/**
 * @Get TSF offset based on the primary STA TSF.
 * ARGS :
 *  ieee80211_vap_t : VAP handle
 * RETURNS:
 *  current TSF offset.
 */
static INLINE void ieee80211_vap_get_tsf_offset(ieee80211_vap_t vap, ieee80211_vap_tsf_offset *tsf_info)
{
    IEEE80211_VAP_LOCK(vap);
    tsf_info->offset = vap->iv_tsf_offset.offset;
    tsf_info->offset_negative = vap->iv_tsf_offset.offset_negative;
    IEEE80211_VAP_UNLOCK(vap);
}


/**
 * @Compute the current TSF offset based on the beacon or probe request.
 * ARGS :
 *  ieee80211_vap_t : VAP handle
 *  wbuf_t : buffer that contains a beacon or probe request packet
 *  struct ieee80211_rx_status : receive status information associated with packet
 * RETURNS:
 *  None
 */
static INLINE void ieee80211_vap_compute_tsf_offset(ieee80211_vap_t vap, wbuf_t wbuf, struct ieee80211_rx_status *rs)
{
    u_int32_t rate_mbps;
    u_int32_t packet_len;
    u_int64_t tsf_desc;
    u_int64_t pri_tsf;
    u_int64_t sec_tsf;
    u_int64_t offset;
    struct ieee80211_frame                       *wh;
    struct ieee80211_beacon_frame        *beacon_frame;
    bool offset_negative = false;
    struct ieee80211com *ic = vap->iv_ic;
    int debug_tsf = 0;

#define  OFFSET_THRESHOLD_USEC  80     /* 40 usec drift (assume 20ppm) over 2 sec times 2*/
#define  BITS_PER_BYTE  8
#define  TIMESTAMP_OFFSET  24

    if (! vap || ! wbuf || ! rs) {
        return;
    }

     /*
        *   Retrieve datarate and receiver's timestamp for beacon. The
        *   receiver's timestamp is captured in the receive descriptor itself.
        */
    rate_mbps = rs->rs_datarate / 1000; /* rs_datarate is Kbps; divide by 1000 for Mbps*/
    tsf_desc = rs->rs_tstamp.tsf;

    /* Extract the transmitter's timestamp from the beacon */
    wh = (struct ieee80211_frame *)wbuf_header(wbuf);
    beacon_frame = (struct ieee80211_beacon_frame *)&wh[1];
    sec_tsf = *((u_int64_t *) &beacon_frame->timestamp[0]);

    packet_len = wbuf_get_pktlen(wbuf);

    /*
        *   Adjust the primary TSF to align in time with that of the TSF in the
        *   beacon itself. The primary tsf is sampled by the receiver at the
        *   end of the beacon packet. Therefore, we need to subtract the
        *   time needed to receive all the bytes of the beacon with the
        *   exception of the IEEE 802.11 header fields up to the location
        *   of the timestamp field in the beacon. The timestamp starts
        *   after byte 24 of the beacon packet.
        */
    pri_tsf = tsf_desc -
        ((u_int64_t) (((packet_len - TIMESTAMP_OFFSET) * BITS_PER_BYTE) / rate_mbps));

    if (pri_tsf > sec_tsf) {
        offset = pri_tsf - sec_tsf;
    }
    else {
        offset = sec_tsf - pri_tsf;
    }

    if (debug_tsf > 0) {
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_LOUD,IEEE80211_MSG_DEBUG,
                             "%s : offset = %llu pri_tsf = %llu sec_tsf = %llu\n",
                             __func__, offset, pri_tsf, sec_tsf);
    }

    if (offset < OFFSET_THRESHOLD_USEC) {
        offset = 0;
        offset_negative = false;
    }
    else {
        /*
         *   Is the offset negative or positive in relation to the primary TSF.
         *   Given the TSF is a full 64-bit value, use subtraction and compare
         *   to determine the polarity of the offset.
         */
        if (sec_tsf < pri_tsf) {
            if (debug_tsf > 0) {
                IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_LOUD,IEEE80211_MSG_DEBUG,
                                     "%s : Offset is NEGATIVE\n", __func__);
            }
            offset_negative = true;
        }
    }

    IEEE80211_VAP_LOCK(vap);

    vap->iv_tsf_offset.offset = offset;
    vap->iv_tsf_offset.offset_negative = offset_negative;

    IEEE80211_VAP_UNLOCK(vap);

#if DEBUG_TSF_OFFSET 
       IEEE80211_DPRINTF(vap, IEEE80211_MSG_POWER, "%s: negative %d offset %llu beacontsf(tsf2) %llu desc tsf (tsf1) %llu \n  ",
                              __func__, offset_negative, offset,sec_tsf, pri_tsf);
#endif
    return;
}

#else  /* UMAC_SUPPORT_VAP_TSF_OFFSET */


static INLINE void ieee80211_vap_get_tsf_offset(ieee80211_vap_t vap, ieee80211_vap_tsf_offset *tsf_info) 
{
    tsf_info->offset = 0;
    tsf_info->offset_negative =0 ;
}

#define ieee80211_vap_compute_tsf_offset(vap, wbuf, rs)  /* */

#endif  /* UMAC_SUPPORT_VAP_TSF_OFFSET */
#endif
