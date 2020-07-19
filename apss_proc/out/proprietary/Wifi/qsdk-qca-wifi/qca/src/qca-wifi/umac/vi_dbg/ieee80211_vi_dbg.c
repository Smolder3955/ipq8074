/*
* Copyright (c) 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 *  Copyright (c) 2010 Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/********************************************************************/
/* \file ieee80211_vi_dbg.c
 * \brief Video debug tool implementation
 *
 * This is a framework for the  implementation of a debug  tool that can be
 * used to  debug  video features with any third party tool.  The framework
 * lets the user define a set of markers  used to identify the  packets  of
 * interest. Upto 4 streams are supported and upto 4 markers can be defined
 * for each of the streams.  Each of the markers can be defined in terms of
 * an offset from the start of the wbuf, the marker field size (max 4 bytes)
 * & the marker pattern match value.
 * The user can also define the packet sequence number field for the packets
 * of interest which is spefied in terms of an offset from the start of the
 * wbuf & the field size (max 4 bytes).  This is used to track the received
 * packet sequence numbers for sequence jumps that indicate packet loss. Pkt
 * loss information is logged into pktlog.
 */

#include <ieee80211_vi_dbg_priv.h>
#include "ieee80211_vi_dbg.h"
#include <ieee80211_ioctl.h>
#include <net.h>
#include <enet.h>
#if UMAC_SUPPORT_VI_DBG
u_int32_t wifi_rx_drop[VI_MAX_NUM_STREAMS] = {0, 0, 0, 0};
u_int32_t packet_count[VI_MAX_NUM_STREAMS]  = {0, 0, 0, 0};

/* Function that acts as an interface to packet log. Used to log packet losss information */
void
ieee80211_vi_dbg_pktlog(struct ieee80211com *ic, const char *fmt, ...)
{
     char                   tmp_buf[OS_TEMP_BUF_SIZE];
     va_list                ap;
     va_start(ap, fmt);
     vsnprintf (tmp_buf, OS_TEMP_BUF_SIZE, fmt, ap);
     va_end(ap);
     ic->ic_log_text(ic, tmp_buf);
}

/* Function to display marker information */
void
ieee80211_vi_dbg_print_marker(struct ieee80211vap *vap)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211_vi_dbg *vi_dbg = ic->ic_vi_dbg;
    uint8_t i,j;

    if (vi_dbg == NULL) {
        return;
    }
    for (i = 0; i < ic->ic_vi_dbg_params->vi_num_streams; i++) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "Stream%d\n", i+1);
	    for (j = 0; j < ic->ic_vi_dbg_params->vi_num_markers; j++) {
	       IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "    marker%d: Offset %#04x Size %#04x Match %#08x\n",  j+1,vi_dbg->markers[i][j].offset, vi_dbg->markers[i][j].num_bytes, vi_dbg->markers[i][j].match);
	    }
    }
}

u_int8_t
ieee80211_vi_dbg_param(wlan_if_t vaphandle, struct ieee80211req_athdbg *req)
{
	struct ieee80211vap *vap = vaphandle;
	struct ieee80211com *ic = vap->iv_ic;
	ic->ic_vi_dbg_params->vi_num_streams = req->data.vow_dbg_param.num_stream;
	ic->ic_vi_dbg_params->vi_num_markers = req->data.vow_dbg_param.num_marker;
	ic->ic_vi_dbg_params->vi_rxseq_offset_size = req->data.vow_dbg_param.rxq_offset;
	ic->ic_vi_dbg->rxseq_num_bytes = req->data.vow_dbg_param.rxq_offset & 0x0000FFFF;
	ic->ic_vi_dbg->rxseq_offset = req->data.vow_dbg_param.rxq_offset >> 16;
	ic->ic_vi_dbg_params->vi_rx_seq_rshift = req->data.vow_dbg_param.rxq_shift;
        ic->ic_vi_dbg_params->vi_rx_seq_max = req->data.vow_dbg_param.rxq_max;
	ic->ic_vi_dbg_params->vi_time_offset_size = req->data.vow_dbg_param.time_offset;
	ic->ic_vi_dbg->time_num_bytes = req->data.vow_dbg_param.time_offset & 0x0000FFFF;
	ic->ic_vi_dbg->time_offset = req->data.vow_dbg_param.time_offset >> 16;
	return 0;
}
/* This function implements the set parameter functionality for all the video debug iwpriv
 * parameters
 */
void
ieee80211_vi_dbg_set_param(wlan_if_t vaphandle, ieee80211_param param, u_int32_t val)
{
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211com *ic = vap->iv_ic;
    switch (param) {
	    case IEEE80211_VI_DBG_CFG:
		    ic->ic_vi_dbg_params->vi_dbg_cfg = val;
		    break;
	    case IEEE80211_VI_RESTART:
		    ic->ic_vi_dbg_params->vi_dbg_restart = val;
		    ic->ic_vi_dbg_params->vi_rx_seq_drop = 0;
		    break;
	    case IEEE80211_VI_RXDROP_STATUS:
		    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s :Not Impelented\n",__func__);
		    break;
	    default:
		    break;
    }
}


/* This function implements the get parameter functionality for all the video debug iwpriv
 * parameters
 */

u_int32_t
ieee80211_vi_dbg_get_param(wlan_if_t vaphandle, ieee80211_param param)
{
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211com *ic = vap->iv_ic;
    u_int32_t val = 0;

    switch (param) {
    case IEEE80211_VI_DBG_CFG:
	    val = ic->ic_vi_dbg_params->vi_dbg_cfg;
	    if (val & EN_MRK_PRNT) {
	        ieee80211_vi_dbg_print_marker(vap);
	    }
	    break;
    case IEEE80211_VI_DBG_NUM_STREAMS:
        val = (u_int32_t) ic->ic_vi_dbg_params->vi_num_streams;
        break;
	case IEEE80211_VI_STREAM_NUM:
        val = (u_int32_t) ic->ic_vi_dbg_params->vi_stream_num;
        break;
    case IEEE80211_VI_DBG_NUM_MARKERS:
        val = (u_int32_t) ic->ic_vi_dbg_params->vi_num_markers;
        break;
    case IEEE80211_VI_MARKER_NUM:
        val = ic->ic_vi_dbg_params->vi_marker_num;
        break;
    case IEEE80211_VI_MARKER_OFFSET_SIZE:
        val = ic->ic_vi_dbg_params->vi_marker_offset_size;
        break;
    case IEEE80211_VI_MARKER_MATCH:
        val = ic->ic_vi_dbg_params->vi_marker_match;
        break;
    case IEEE80211_VI_RXSEQ_OFFSET_SIZE:
        val = ic->ic_vi_dbg_params->vi_rxseq_offset_size;
        break;
	case IEEE80211_VI_RX_SEQ_RSHIFT:
        val = ic->ic_vi_dbg_params->vi_rx_seq_rshift;
	    break;
	case IEEE80211_VI_RX_SEQ_MAX:
        val = ic->ic_vi_dbg_params->vi_rx_seq_max;
        break;
	case IEEE80211_VI_RX_SEQ_DROP:
        val = ic->ic_vi_dbg_params->vi_rx_seq_drop;
        break;
    case IEEE80211_VI_TIME_OFFSET_SIZE:
        val = ic->ic_vi_dbg_params->vi_time_offset_size;
        break;
    case IEEE80211_VI_RESTART:
        val = ic->ic_vi_dbg_params->vi_dbg_restart;
        break;
    case IEEE80211_VI_RXDROP_STATUS:
        break;
    default:
        break;
    }
    return val;
}


/* The function is used set up the markers for pkt filtering via iwpriv. It sets the marker_info
 * fields in the ieee80211_vi_dbg stucture
 */
u_int8_t
ieee80211_vi_dbg_set_marker(struct ieee80211vap *vap, struct ieee80211req_athdbg *req)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211_vi_dbg *vi_dbg = ic->ic_vi_dbg;
    u_int32_t marker_num;
    u_int32_t stream_num;

    if (vi_dbg == NULL) {
        return 0;
    }

    marker_num = req->data.vow_dbg_stream_param.marker_num;
    stream_num = req->data.vow_dbg_stream_param.stream_num;

    if (stream_num < 1 || stream_num > VI_MAX_NUM_STREAMS || marker_num < 1 || marker_num > VI_MAX_NUM_MARKERS) {
         IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s : Invalid number of streams or markers set. Skipping marker assignment\n",__func__);
         return 0;
    }
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "Setting Stream%d Marker%d Offset: %d Num Bytes: %d Matching pattern: %d\n",
                                               stream_num, marker_num, req->data.vow_dbg_stream_param.marker_offset >> 16,
					       req->data.vow_dbg_stream_param.marker_offset & 0x0000FFFF, req->data.vow_dbg_stream_param.marker_match);
    vi_dbg->markers[stream_num-1][marker_num-1].offset    = req->data.vow_dbg_stream_param.marker_offset >> 16;
    vi_dbg->markers[stream_num-1][marker_num-1].num_bytes = req->data.vow_dbg_stream_param.marker_offset & 0x0000FFFF;
    vi_dbg->markers[stream_num-1][marker_num-1].match     = req->data.vow_dbg_stream_param.marker_match;
    return 0;
}

/* This function is used to filter the received wbuf based on the defined markers */
int32_t
ieee80211_vi_dbg_check_markers(struct ieee80211vap *vap, wbuf_t mywbuf)
{
    struct ieee80211com *ic = vap->iv_ic;
    u_int8_t *payload;
    u_int8_t match;
    u_int8_t i, j, s;
    u_int32_t mfail;
    int offset;
    offset = ETHERNET_ADDR_LEN * 2;

    payload = (u_int8_t *)wbuf_raw_data(mywbuf);

    for (s = 0; s < ic->ic_vi_dbg_params->vi_num_streams; s++) {
	    mfail = 0;  /* Reset marker failures for new stream. mfail is used to reduce checks if maker checks
	                 * fail for any stream. If a filure is detected immediately move on to the markers for
	  	             * the next stream */
	    for (i = 0; i < ic->ic_vi_dbg_params->vi_num_markers; i++) {
	        for (j = 0; j < ic->ic_vi_dbg->markers[s][i].num_bytes; j++) {
	            match = ((ic->ic_vi_dbg->markers[s][i].match) >> (8*(ic->ic_vi_dbg->markers[s][i].num_bytes-1-j))) & 0x000000FF;
		        if (payload[ic->ic_vi_dbg->markers[s][i].offset + j] != match) {
		            mfail = 1; /* Marker failure for this stream. Move to next stream */
		            break;
		        }
	        }
	        if (mfail == 1) {
		        break;        /* Marker failure for this stream. Skip other markers for this
		                         stream & move to next stream */
	        }
	    }

	    if (!mfail) {
		   return s;
	    }
    }
    return -1;
}

/* This function is used to collect stats from the received wbuf after the marker
 * filtering has been completed
 */
void
ieee80211_vi_dbg_stats(struct ieee80211vap *vap, wbuf_t mywbuf, u_int32_t stream_num)
{
    struct ieee80211com *ic = vap->iv_ic;
    u_int8_t *payload;
    u_int32_t i, rx_seq_num = 0;
    int32_t   diff;
    static u_int32_t prev_rx_seq_num[VI_MAX_NUM_STREAMS] = {0, 0, 0, 0};
    static u_int32_t current_seq_num[VI_MAX_NUM_STREAMS]   = {0, 0, 0, 0};
    static u_int8_t start[VI_MAX_NUM_STREAMS]           = {0, 0, 0, 0};
    u_int16_t offset, num_bytes;

    if (!(ic->ic_vi_dbg_params->vi_dbg_cfg & EN_MRK_FILT))
	    return;

	/* If restart is set, re-initialize internal variables & reset the restart parameter */
    if (ic->ic_vi_dbg_params->vi_dbg_restart) 	{
	    for (i = 0; i < VI_MAX_NUM_STREAMS; i++) {
		    start[i]           = 0;
		    prev_rx_seq_num[i] = 0;
		    current_seq_num[i]   = 0;
		    wifi_rx_drop[i] = 0;
		    packet_count[i] = 0;
	    }
	    ieee80211_vi_dbg_set_param(vap, IEEE80211_VI_RESTART, 0);
    }

    payload   = (u_int8_t *)wbuf_raw_data(mywbuf);
    offset    = ic->ic_vi_dbg->rxseq_offset;
    num_bytes = ic->ic_vi_dbg->rxseq_num_bytes;

    for (i = 0; i < num_bytes; i++) {
	    rx_seq_num += (payload[offset + i] << (8 * (3 - i)));
    }
    rx_seq_num = rx_seq_num >> ic->ic_vi_dbg_params->vi_rx_seq_rshift;
#define BIT_SET_CHECK 0x80000000
#define RX_SEQ_NUM_SET 0x7fffffff

   if (start[stream_num] == 0)
   {    // check for 31st bit Set; 31st bit on means iperf sending 1st packet
	   IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "stream :%d  start seq no : 0x%08x \n",stream_num+1,rx_seq_num);
        if (rx_seq_num & BIT_SET_CHECK)
            rx_seq_num &= RX_SEQ_NUM_SET;

   }
#undef BIT_SET_CHECK
#undef RX_SEQ_NUM_SET
   packet_count[stream_num]++;
   current_seq_num[stream_num] = rx_seq_num;
   diff = current_seq_num[stream_num] - prev_rx_seq_num[stream_num];
    if ((diff < 0) && start[stream_num] == 1)
    {  // wrap case & Loss in Wrap
       diff = ic->ic_vi_dbg_params->vi_rx_seq_max-prev_rx_seq_num[stream_num]+current_seq_num[stream_num];
       // not handling out of order case
    }


  if ((diff > 1) && start[stream_num] == 1)
  {
        wifi_rx_drop[stream_num] += diff;
        ieee80211_vi_dbg_pktlog(ic, "wifi: Stream %d, Diff %d Rx pkt loss from %d  to %d\n", stream_num + 1,
		        	                diff,prev_rx_seq_num[stream_num],current_seq_num[stream_num]);
	    ieee80211_vi_dbg_set_param(vap, IEEE80211_VI_RX_SEQ_DROP, 1);
  }

    if (diff == 0 && start[stream_num] == 1) {
        ieee80211_vi_dbg_pktlog(ic, "Rx duplicate pkt %d \n", rx_seq_num);
    }
    prev_rx_seq_num[stream_num] = current_seq_num[stream_num];
    start[stream_num] = 1;
}

/* This function is used to check the received wbufs before they are handed over to the
 * netif_rx  function.  The  wbuf is checked for the signature  indicated by the  debug
 * markers that have been already set.
 */
void
ieee80211_vi_dbg_input(struct ieee80211vap *vap, wbuf_t wbuf)
{
    int stream_num;
    struct ieee80211com *ic = vap->iv_ic;

    if (!(ic->ic_vi_dbg_params->vi_dbg_cfg & EN_MRK_FILT)) {
	    return;
	}

    stream_num = ieee80211_vi_dbg_check_markers(vap, wbuf);

    if (stream_num != -1 && (stream_num < VI_MAX_NUM_STREAMS)) {
	    ieee80211_vi_dbg_stats(vap, wbuf, stream_num);
    }
}

void
ieee80211_vi_dbg_print_stats(struct ieee80211vap *vap)
{
	int i;
	for (i = 0; i < VI_MAX_NUM_STREAMS; i++) {
		printk("\n STREAM %d : PACKET COUNT %d\n", i,packet_count[i]);
		printk("\n No of Packets Dropped %d\n", wifi_rx_drop[i]);
	}
}

/* This function is used to attach the video debug structures & initialization */
int
ieee80211_vi_dbg_attach(struct ieee80211com *ic)
{
    if (!ic) {
        return -EFAULT;
    }

    ic->ic_vi_dbg_params = ( struct ieee80211_vi_dbg_params *)
                           OS_MALLOC(ic->ic_osdev, sizeof(struct ieee80211_vi_dbg_params), GFP_KERNEL);

    if (!ic->ic_vi_dbg_params) {
        printk("%s : Memory not allocated for ic->ic_vi_dbg_params\n",__func__); /* Not expected */
        return -ENOMEM;
    }

    OS_MEMSET(ic->ic_vi_dbg_params, 0, sizeof(struct ieee80211_vi_dbg_params));

    ic->ic_vi_dbg_params->vi_num_streams = 1;
    ic->ic_vi_dbg_params->vi_stream_num  = 1;

    ic->ic_vi_dbg = (struct ieee80211_vi_dbg *)
                    OS_MALLOC(ic->ic_osdev, sizeof(struct ieee80211_vi_dbg), GFP_KERNEL);

    if (!ic->ic_vi_dbg) {
        printk(" %s : Memory not allocated for ic->ic_vi_dbg\n",__func__); /* Not expected */
        OS_FREE(ic->ic_vi_dbg_params);
        ic->ic_vi_dbg_params = NULL;
        return -ENOMEM;
    }
    return 0;
}

/* This function is used to detach the video debug structures & initialization */
int
ieee80211_vi_dbg_detach(struct ieee80211com *ic)
{
    int err = 0;

    if (!ic) {
        return -EFAULT;
    }

    if (!ic->ic_vi_dbg) {
        printk("%s :Memory not allocated for ic->ic_vi_dbg\n",__func__); /* Not Expected */
        err = -ENOMEM;
        goto bad;
    } else {
        OS_FREE(ic->ic_vi_dbg);
        ic->ic_vi_dbg = NULL;
    }
    if (!ic->ic_vi_dbg_params) {
        printk("%s :Memory not allocated for ic->ic_vi_dbg_params\n",__func__); /* Not expected */
        err = -ENOMEM;
        goto bad;
    } else {
        OS_FREE(ic->ic_vi_dbg_params);
        ic->ic_vi_dbg_params = NULL;
    }
bad:
   return err;
}
#endif
