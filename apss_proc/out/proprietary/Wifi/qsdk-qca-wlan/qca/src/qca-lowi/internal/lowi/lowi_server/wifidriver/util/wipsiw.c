/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

   WiFi Scanner with NL80211 Interface

   GENERAL DESCRIPTION
   This component performs passive scan with NL80211 Interface.

   Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
   All Rights Reserved.
   Qualcomm Atheros Confidential and Proprietary.

   Copyright (c) 2016 Qualcomm Technologies, Inc.
   All Rights Reserved.
   Confidential and Proprietary - Qualcomm Technologies, Inc.

   Copyright (c) 2007, 2008        Johannes Berg
   Copyright (c) 2007              Andy Lutomirski
   Copyright (c) 2007              Mike Kershaw
   Copyright (c) 2008-2009         Luis R. Rodriguez

   Permission to use, copy, modify, and/or distribute this software for any
   purpose with or without fee is hereby granted, provided that the above
   copyright notice and this permission notice appear in all copies.

   THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
   WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
   MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
   ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
   WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
   ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
   OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

* Driver interaction with Linux nl80211/cfg80211
 * Copyright (c) 2002-2012, Jouni Malinen <j@w1.fi>
 * Copyright (c) 2003-2004, Instant802 Networks, Inc.
 * Copyright (c) 2005-2006, Devicescape Software, Inc.
 * Copyright (c) 2007, Johannes Berg <johannes@sipsolutions.net>
 * Copyright (c) 2009-2010, Atheros Communications
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
=============================================================================*/


#define _GNU_SOURCE
#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <linux/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <err.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <linux/netlink.h>
#include <linux/wireless.h>
#ifdef __ANDROID__
  #include <cutils/properties.h>
  #include <linux/wireless.h>                         // note: NDK's Android'ized wireless.h
  #include <net/if.h>
#endif

#include <errno.h>

#include <sys/time.h>
#include <stdbool.h>
#include <netlink/netlink.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include <endian.h>
#include <asm/types.h>


#ifdef DEBUG_X86
#include <bsd/string.h>
#endif

#include "innavService.h"                           //  structure definitions and such
#include "wlan_location_defs.h"

#include <stdbool.h>
#include <netlink/netlink.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include <endian.h>

#include <linux/nl80211.h>

#include "wifiscanner.h"
#include <lowi_server/lowi_log.h>
#include "lowi_time.h"
#include "wipsiw.h"

// instead of printf using LOWI_LOG_VERB for now.
// This needs to be further evaluated and logged under different log levels.
#define PRIN_LOG(format, args...) LOWI_LOG_VERB(format "\n", ## args)
#define LOG_TAG "LOWI-Scan"

#define ETH_ALEN 6
#define TRUE 1
#define FALSE 0

#define WLAN_FC_TYPE_MGMT 0
#define WLAN_FC_STYPE_ACTION 13
#define LOWI_FRAME_MATCH_LEN 2

#define IEEE80211_FRAME_CTRL(type, stype) ((type << 2) | (stype << 4))

// In number of milli-seconds. If AP age exceeds this limit,
// it will not be adjusted for NL 3 seconds buffering
#define AP_AGE_ADJUST_UPPER_LIMIT 10000

#define MAX_2G_CHANNEL 14

// Time to wait when NL init fails before returning back
#define WIPS_WAIT_NL_FAIL  5

#define WIPSIW_ENTER LOWI_LOG_VERB( "WIPSIW_ENTER: %s\n",__FUNCTION__);
#define WIPSIW_EXIT  LOWI_LOG_VERB( "WIPSIW_EXIT : %s\n",__FUNCTION__);

#define PROPERTY_VALUE_MAX  92

/** The following is a string array for the different PHY Modes
 *  supported by FW. This string list should match the
 *  enumeration in innavService.h
 */
static const char* PHY_MODE[] =
{
 "ROME_PHY_MODE_11A",
 "ROME_PHY_MODE_11G",
 "ROME_PHY_MODE_11B",
 "ROME_PHY_MODE_11GONLY",
 "ROME_PHY_MODE_11NA_HT20",
 "ROME_PHY_MODE_11NG_HT20",
 "ROME_PHY_MODE_11NA_HT40",
 "ROME_PHY_MODE_11NG_HT40",
 "ROME_PHY_MODE_11AC_VHT20",
 "ROME_PHY_MODE_11AC_VHT40",
 "ROME_PHY_MODE_11AC_VHT80",
 "ROME_PHY_MODE_11AC_VHT20_2G",
 "ROME_PHY_MODE_11AC_VHT40_2G",
 "ROME_PHY_MODE_11AC_VHT80_2G",
 "ROME_PHY_MODE_UNKNOWN/MAX"
};

char current_ssid[32*4+1]; // ssid is no more than 32 bytes,
                     // and each byte can become 4 byte

WlanFrameStore wlanFrameStore;

struct nl80211_state {
        struct nl_sock *nl_sock;
        struct genl_family * nl80211_family_ptr;
        struct nl_cache * nl_cache;
        unsigned int nl80211_id;
        int scanmcid;
        struct nl_cb *s_cb;
        bool nlInitialized;
};

struct s_wait_event {
        int n_cmds;
        const __u32 *cmds;
        __u32 cmd;
        void *pargs;
};
static struct nl80211_state nlstate = {.nlInitialized = false}; //Used throughout the code..

static int      pipe_fd[2];                  // Pipe used to terminate select in Discovery thread
extern void iwss_bss_insert_record(void * results_buf_ptr,
                    int16  bss_rssi_0p5dBm,         /* In unit of 0.5 dBm */
                    int32  bss_age_msec,        /* In units of 1 milli-second, -1 means info not available */
                    quipc_wlan_ap_meas_s_type * p_ap_scan_res);
extern void iwss_bss_close_record(void * results_buf_ptr);
extern void iwss_bss_reset_records(void * results_buf_ptr);

extern uint64 wipsiw_scan_req_time;

#define AP_AGE_UNSPECIFIED_IN_MSECS -1

struct nl_msg
{
   int         nm_protocol;
   int         nm_flags;
   struct sockaddr_nl   nm_src;
   struct sockaddr_nl   nm_dst;
   struct ucred      nm_creds;
   struct nlmsghdr * nm_nlh;
   size_t         nm_size;
   int         nm_refcnt;
};
static int num_acks_for_dump = 0;
static int num_finishes_for_dump = 0;
static int genl_integrity_check_fail = FALSE;

// This data structure is used to filter out those APs
// that are not seen by RIVA driver layer, but is buffered by
// NL layer. NL layer will report them up to 3 seconds
// without properly populating the age info
typedef struct
{
  unsigned char bssid[6];
  uint64        bss_tsf_usec;
  uint32        bss_age_msec;
  uint64        bss_meas_recv_msec;
} wips_nl_ap_info;

// Allocate AP three times as much as the maximum used in upper layer.
// As the upper layer will filter out APs based on age
// which will reduce the number of APs outputted from
// this module
//
// It uses two list of APs:
// The current list saves the APs from the previous discovery scan
// The new list saves the APs from the on-going discovery scan.
// And right before a fresh passive scan is issued, the data
// in the new list is saved into current list, and the new list
// will be used to save the APs coming from the on-going discovery scan.
typedef struct
{
  wips_nl_ap_info current_list[3*NUM_MAX_BSSIDS];
  wips_nl_ap_info new_list[3*NUM_MAX_BSSIDS];
  uint32          num_ap_current;
  uint32          num_ap_new;
} wips_nl_ap_list;

// Memory is zero initialized
static wips_nl_ap_list wips_nl_ap_store;

static int register_for_ftmrr(struct nl80211_state * pstate);

/**
 * This function coverts the provided Channel ID to the
 * equivalent Frequency in MHz.
 *
 * @param channel: The Channel ID
 *
 * @return uint32: The Equivalent Frequency in MHz
 */
uint32 getChannelFreq (uint32 channel)
{
  LOWI_LOG_DBG("%s - Chan ID = %d", __func__, channel);
  uint32 freqBase = 2412;
  if (channel == 14)
  {
    LOWI_LOG_DBG("%s - Freq = %u", __func__, 2484);
    return 2484;
  }
  if (channel >= 1 && channel <= 13)
  {
    freqBase = 2412;
    channel = channel - 1;
  }
  else if (channel >= 34 && channel <= 64)
  {
    freqBase = 5170;
    channel = channel - 34;
  }
  else if (channel >= 100 && channel <= 165)
  {
    freqBase = 5500;
    channel = channel - 100;
  }
  else
  {
    LOWI_LOG_ERROR("%s - Invalid channel = %d", __func__, channel);
    return 0;
  }
  LOWI_LOG_DBG("%s - Freq = %u", __func__, (freqBase + (channel * 5)));
  return (freqBase + (channel * 5));
}
/*=============================================================================================
 * Function description:
 *   Gets the channel number for the frequency.
 *
 * Parameters:
 *  freq.
 *
 * Return value:
 *  channel number
 =============================================================================================*/

uint32 GetChanIndex(uint32 freq)
{
  LOWI_LOG_DBG("%s - Frequency = %d", __func__, freq);
  uint32 chanId = 0;
  uint32 chanBase = 0;
  uint32 freqBase = 2412;
  if (freq == 2484)
  {
    LOWI_LOG_DBG("%s - Chan ID = %u", __func__, 14);
    return 14;
  }
  if (freq >= 2412 && freq <= 2472)
  {
    freqBase = 2412;
    chanBase = 1;
  }
  else if (freq >= 5170 && freq <= 5320)
  {
    freqBase = 5170;
    chanBase = 34;
  }
  else if (freq >= 5500 && freq <= 5825)
  {
    freqBase = 5500;
    chanBase = 100;
  }
  else
  {
    LOWI_LOG_DBG("%s - Invalid frequency = %u", __func__, freq);
    return 0;
  }
  chanId = (freq - freqBase);
  if (chanId % 5)
  {
    LOWI_LOG_DBG("%s - Invalid frequency = %u", __func__, freq);
    return 0;
  }
  else
  {
    chanId = (chanId / 5);
    chanId = chanId + chanBase;
    LOWI_LOG_DBG("%s - Chan ID = %u", __func__, chanId);
    return (chanId);
  }
}

// Interface name. Initialize to empty string
static char wips_ifname[PROPERTY_VALUE_MAX] = "";
#define DEFAULT_WIFI_INTERFACE "wlan0"

/*===========================================================================
 * Function description:
 *   Returns the interface name by reading system property
 *   if not already read from the system property
 *
 * Parameters:
 *   None
 *
 * Return value:
 *    Interface name
 ============================================================================*/
char* get_interface_name (void)
{
  if (0 == wips_ifname[0])
  {
#ifdef __ANDROID__
    if (property_get("wifi.interface", wips_ifname, DEFAULT_WIFI_INTERFACE) != 0)
    {
      LOWI_LOG_DBG("Using interface '%s'\n", wips_ifname);
      return wips_ifname;
    }
#else
    strlcpy (wips_ifname, DEFAULT_WIFI_INTERFACE, sizeof (wips_ifname));
#endif
  }
  return wips_ifname;
}

/*=============================================================================================
 * Function description:
 *   Called by external entity to terminate the blocking of this thread on select call
 *   by means of writing to a pipe. Thread is blocked on socket and a pipe in select call
 *
 * Parameters:
 *   None
 *
 * Return value:
 *    num of bytes written, -1 for error, 0 for no bytes written
 =============================================================================================*/
int Wips_nl_shutdown_communication(void)
{
  int retVal = -1;
  char string [] = "Close";
  LOWI_LOG_DBG ("Wips_nl_shutdown_communication");
  if (0 != pipe_fd [1])
  {
    retVal = write(pipe_fd[1], string, (strlen(string)+1));

    // Close the write end of the pipe
    close (pipe_fd[1]);
    pipe_fd [1] = 0;
  }

  return retVal;
}

/*=============================================================================================
 * Function description:
 *   Called by external entity to create the pipe
 *
 * Parameters:
 *   None
 *
 * Return value:
 *    0 Success, other values otherwise
 =============================================================================================*/
int Wips_nl_init_pipe(void)
{
  LOWI_LOG_DBG( "Creating the pipe\n");
  return pipe(pipe_fd);
}

/*=============================================================================================
 * Function description:
 *   Called by external entity to close the pipe
 *
 * Parameters:
 *   None
 *
 * Return value:
 *    0 Success, other values otherwise
 =============================================================================================*/
int Wips_nl_close_pipe(void)
{
  LOWI_LOG_DBG( "Closing the pipe\n");
  if (pipe_fd[0] > 0)
  {
    close (pipe_fd[0]);
    pipe_fd [0] = 0;
  }

  if (pipe_fd[1] > 0)
  {
    close (pipe_fd[1]);
    pipe_fd [1] = 0;
  }
  return 0;
}

/*=============================================================================================
 * Function description:
 *   The cleanup function when passive scan has been performed with NL80211 Interface.
 *
 * Parameters:
 *   state: pointer to the data structure for NL80211 state
 *
 * Return value:
 *    error code: NL_OK
 =============================================================================================*/
static void nl80211_cleanup(struct nl80211_state *state)
{
  nl_socket_free(state->nl_sock);
  nl_cb_put (state->s_cb);
  if (state->nl_cache)
  {
    nl_cache_free(state->nl_cache);
  }
  if (state->nl80211_id)
  {
    genl_family_put(state->nl80211_family_ptr);
  }
  state->nlInitialized = false;
}

/*=============================================================================================
 * Function description:
 *   Initilaize NL interface for passive scan request.
 *
 * Parameters:
 *   pstate: pointer to the data structure for NL80211 state
 *
 * Return value:
 *    error code: NL error code
 =============================================================================================*/
static int nl80211_init(struct nl80211_state *state)
{
  int err;

  if (state->nlInitialized)
  {
    LOWI_LOG_VERB ("NL Soclet Already initialized\n");
    return 0;
  }

  memset(state, 0, sizeof(struct nl80211_state));

  state->nlInitialized = false;
  state->s_cb = nl_cb_alloc(NL_CB_DEFAULT);
  if (!state->s_cb)
  {
    LOWI_LOG_ERROR ("Failed to allocate memory\n");
    err = -ENOMEM;
    goto out_handle_destroy;
  }

  state->nl_sock = nl_socket_alloc_cb(state->s_cb);
  if (!state->nl_sock)
  {
    LOWI_LOG_ERROR ( "Failed to allocate netlink socket.\n");
    err = -ENOMEM;
    goto out_handle_destroy;
  }

  if (genl_connect(state->nl_sock))
  {
    LOWI_LOG_ERROR ( "Failed to connect to generic netlink.\n");
    err = -ENOLINK;
    goto out_handle_destroy;
  }

  if (genl_ctrl_alloc_cache(state->nl_sock, &(state->nl_cache)))
  {
    LOWI_LOG_ERROR ("Error in Allocating Cache\n");
    err = -ENOMEM;
    goto out_handle_destroy;
  }
  state->nl80211_family_ptr = genl_ctrl_search_by_name(state->nl_cache,"nl80211");

  if(!state->nl80211_family_ptr)
  {
      LOWI_LOG_ERROR ( "Failed to get n180211 family \n");
      err = -ENOMEM;
      goto out_handle_destroy;
  }

  state->nl80211_id = genl_family_get_id(state->nl80211_family_ptr);

  if (state->nl80211_id == 0)
  {
    LOWI_LOG_ERROR ( "nl80211 not found.\n");
    err = -ENOENT;
    goto out_handle_destroy;
  }

  state->nlInitialized = true;
  LOWI_LOG_DBG ("NL80211 Init Done.\n");
  register_for_ftmrr(state);
  LOWI_LOG_DBG ("NL80211 Init Registered for FTMRR.\n");
  return 0;

out_handle_destroy:
  nl80211_cleanup(state);
  return err;
}

/*=============================================================================================
 * Function description:
 *   External interface function to initilaizes NL interface for passive scan request.
 *
 * Parameters:
 *   none
 *
 * Return value:
 *    error code: NL error code
 =============================================================================================*/
int lowi_gen_nl_drv_open()
{
  return nl80211_init(&nlstate);
}

/*=============================================================================================
 * Function description:
 *   Callback function for NL_CB_CUSTOM with operations on NL80211 Interface.
 *
 * Parameters:
 *   msg: pointer to the msg that contains the requested info
 *   arg: data passed when set up the callback
 *
 * Return value:
 *    error code: NL error code
 =============================================================================================*/
static int dump_error_handler(struct sockaddr_nl *nla, struct nlmsgerr *err,
                         void *arg)
{
  //Something wrong with socket??
  int *ret = arg;
  *ret = err->error;
  LOWI_LOG_INFO("Dump Error Handler called with error %d . SKIP!!",*ret);
  return NL_SKIP;
}

/*=============================================================================================
 * Function description:
 *   Callback function for NL_CB_FINISH with operations on the NL80211 Interface.
 *
 * Parameters:
 *   msg: pointer to the msg that contains the requested info
 *   arg: data passed when set up the callback
 *
 * Return value:
 *    error code: NL error code
 =============================================================================================*/
static int dump_finish_handler(struct nl_msg *msg, void *arg)
{
  int *ret = arg;
  struct nlmsghdr *nlh;

  *ret = 1;
  nlh = (struct nlmsghdr *)(msg->nm_nlh);
  LOWI_LOG_DBG("Dump Finish HandlerLength %d Type %d Flags %d Seq %d Sender %d",
  (int)(nlh->nlmsg_len), (int)(nlh->nlmsg_type), (int)(nlh->nlmsg_flags),
  (int)(nlh->nlmsg_seq), (int)(nlh->nlmsg_pid));

  num_finishes_for_dump++;

  if (nlh->nlmsg_type == 3)
  {
    LOWI_LOG_DBG("Marking msg to be Finished");
    *ret = 0;
    return NL_STOP;
  }

  return NL_SKIP;
}


/*=============================================================================================
 * Function description:
 *   Callback function for NL_CB_CUSTOM with operations on the NL80211 Interface.
 *
 * Parameters:
 *   msg: pointer to the msg that contains the requested info
 *   arg: data passed when set up the callback
 *
 * Return value:
 *    error code: NL error code
 =============================================================================================*/
static int dump_ack_handler(struct nl_msg *msg, void *arg)
{
  int *ret = arg;
  struct nl_msg * a;
  struct nlmsghdr *nlh;
  char strtoprint[256];
  char * p_data;
  int i;
  a = msg;
  nlh = (struct nlmsghdr *)(msg->nm_nlh);
  *ret = 0;
  LOWI_LOG_INFO("Dump Ack Handler Length %d Type %d Flags %d Seq %d Sender %d",
  nlh->nlmsg_len, nlh->nlmsg_type, nlh->nlmsg_flags,
  nlh->nlmsg_seq, nlh->nlmsg_pid);

  num_acks_for_dump++;

  //Dump the bytes
  p_data = (char *)nlmsg_data(nlh);
  for (i=0;i<(nlh->nlmsg_len - NLMSG_HDRLEN);i++)
    LOWI_LOG_DBG("AckMsg[%d] = 0x%x",i,p_data[i]);

  return NL_STOP;
}
/*=============================================================================================
 * Function description:
 *   Callback function for NL_CB_CUSTOM with operations on NL80211 Interface.
 *
 * Parameters:
 *   msg: pointer to the msg that contains the requested info
 *   arg: data passed when set up the callback
 *
 * Return value:
 *    error code: NL error code
 =============================================================================================*/
static int error_handler(struct sockaddr_nl *nla, struct nlmsgerr *err,
                         void *arg)
{
  //Something wrong with socket??
  struct sockaddr_nl * tmp;
  int *ret = arg;
  tmp = nla;
  *ret = err->error;
  return NL_STOP;
}

/*=============================================================================================
 * Function description:
 *   Callback function for NL_CB_FINISH with operations on the NL80211 Interface.
 *
 * Parameters:
 *   msg: pointer to the msg that contains the requested info
 *   arg: data passed when set up the callback
 *
 * Return value:
 *    error code: NL error code
 =============================================================================================*/
static int finish_handler(struct nl_msg *msg, void *arg)
{
  int *ret = arg;
  struct nl_msg * a;
  a = msg;
  *ret = 0;
  return NL_SKIP;
}


/*=============================================================================================
 * Function description:
 *   Callback function for NL_CB_CUSTOM with operations on the NL80211 Interface.
 *
 * Parameters:
 *   msg: pointer to the msg that contains the requested info
 *   arg: data passed when set up the callback
 *
 * Return value:
 *    error code: NL error code
 =============================================================================================*/
static int ack_handler(struct nl_msg *msg, void *arg)
{
  LOWI_LOG_VERB("ack handler called \n");
  int *ret = arg;
  struct nl_msg * a;
  a = msg;
  *ret = 0;
  return NL_STOP;
}

struct handler_args {
  const char *group;
  int id;
};

/*=============================================================================================
 * Function description:
 *   Callback function with family and group info of the NL80211 Interface.
 *
 * Parameters:
 *   msg: pointer to the msg that contains the requested info
 *   arg: data passed when set up the callback
 *
 * Return value:
 *    error code: NL error code
 =============================================================================================*/
static int family_handler(struct nl_msg *msg, void *arg)
{
  struct handler_args *grp = arg;
  struct nlattr *tb[CTRL_ATTR_MAX + 1];
  struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
  struct nlattr *mcgrp;
  int rem_mcgrp;

  nla_parse(tb, CTRL_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
            genlmsg_attrlen(gnlh, 0), NULL);

  if (!tb[CTRL_ATTR_MCAST_GROUPS])
    return NL_SKIP;

  nla_for_each_nested(mcgrp, tb[CTRL_ATTR_MCAST_GROUPS], rem_mcgrp) {
    struct nlattr *tb_mcgrp[CTRL_ATTR_MCAST_GRP_MAX + 1];

    nla_parse(tb_mcgrp, CTRL_ATTR_MCAST_GRP_MAX,
              nla_data(mcgrp), nla_len(mcgrp), NULL);

    if (!tb_mcgrp[CTRL_ATTR_MCAST_GRP_NAME] ||
        !tb_mcgrp[CTRL_ATTR_MCAST_GRP_ID])
        continue;

    if (strncmp(nla_data(tb_mcgrp[CTRL_ATTR_MCAST_GRP_NAME]),
                grp->group, nla_len(tb_mcgrp[CTRL_ATTR_MCAST_GRP_NAME])))
      continue;

    grp->id = nla_get_u32(tb_mcgrp[CTRL_ATTR_MCAST_GRP_ID]);
    break;
  }

  return NL_SKIP;
}

/*=============================================================================================
 * Function description:
 *   This function gets the multicase ID with NL80211 Interface.
 *
 * Parameters:
 *   sock: pointer to the NL socket
 *   family: pointer to the family information
 *   group: pointer to the group information
 *
 * Return value:
 *    error code: NL_OK
 =============================================================================================*/
int nl_get_multicast_id(struct nl_sock *sock, const char *family, const char *group)
{
  struct nl_msg *msg;
  struct nl_cb *cb;
  int ret, ctrlid;
  struct handler_args grp = {
                .group = group,
                .id = -ENOENT,
        };

  msg = nlmsg_alloc();
  if (!msg)
    return -ENOMEM;

  cb = nl_cb_alloc(NL_CB_DEFAULT);
  if (!cb) {
    ret = -ENOMEM;
    goto out_fail_cb;
  }

  ctrlid = genl_ctrl_resolve(sock, "nlctrl");

  genlmsg_put(msg, 0, 0, ctrlid, 0,
              0, CTRL_CMD_GETFAMILY, 0);

  ret = -ENOBUFS;
  NLA_PUT_STRING(msg, CTRL_ATTR_FAMILY_NAME, family);

  ret = nl_send_auto_complete(sock, msg);
  if (ret < 0)
    goto out;

  ret = 1;

  nl_cb_err(cb, NL_CB_CUSTOM, error_handler, &ret);
  nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, ack_handler, &ret);
  nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, family_handler, &grp);

  // Not an infinite loop
  while (ret > 0)
    nl_recvmsgs(sock, cb);

  if (ret == 0)
    ret = grp.id;

nla_put_failure:
out:
  nl_cb_put(cb);
out_fail_cb:
  nlmsg_free(msg);
  return ret;
}

/*=============================================================================================
 * Function description:
 *   Callback function with wiphy info of the NL80211 Interface.
 *
 * Parameters:
 *   msg: pointer to the msg that contains the requested info
 *   arg: data passed when set up the callback
 *
 * Return value:
 *    error code: NL error code
 =============================================================================================*/
static int wiphy_info_handler(struct nl_msg *msg, void *arg)
{
  struct nlattr *tb[NL80211_ATTR_MAX + 1];

  struct nlattr *tb_band[NL80211_BAND_ATTR_MAX + 1];
  struct nlattr *tb_freq[NL80211_FREQUENCY_ATTR_MAX + 1];
  struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
  struct nlattr *nl_band;
  struct nlattr *nl_freq;
  int rem_band;
  int rem_freq;
  uint32 freq;

  static struct nla_policy freq_policy[NL80211_FREQUENCY_ATTR_MAX + 1] = {
    [NL80211_FREQUENCY_ATTR_FREQ] = { .type = NLA_U32 },
    [NL80211_FREQUENCY_ATTR_DISABLED] = { .type = NLA_FLAG },
    [NL80211_FREQUENCY_ATTR_PASSIVE_SCAN] = { .type = NLA_FLAG },
    [NL80211_FREQUENCY_ATTR_NO_IBSS] = { .type = NLA_FLAG },
    [NL80211_FREQUENCY_ATTR_RADAR] = { .type = NLA_FLAG },
    [NL80211_FREQUENCY_ATTR_MAX_TX_POWER] = { .type = NLA_U32 },
  };

  s_ch_info* ch_info = arg;
  LOWI_LOG_DBG ( "wiphy_info_handler");
  nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
      genlmsg_attrlen(gnlh, 0), NULL);

  if (!tb[NL80211_ATTR_WIPHY_BANDS])
  {
    LOWI_LOG_DBG ( "wiphy_info_handler, NL80211_ATTR_WIPHY_BANDS not found");
    return NL_SKIP;
  }

  nla_for_each_nested (nl_band, tb[NL80211_ATTR_WIPHY_BANDS], rem_band)
  {
    nla_parse(tb_band, NL80211_BAND_ATTR_MAX, nla_data(nl_band),
        nla_len(nl_band), NULL);
    nla_for_each_nested(nl_freq, tb_band[NL80211_BAND_ATTR_FREQS], rem_freq)
    {
      nla_parse(tb_freq, NL80211_FREQUENCY_ATTR_MAX, nla_data(nl_freq),
          nla_len(nl_freq), freq_policy);
      if (!tb_freq[NL80211_FREQUENCY_ATTR_FREQ])
      {
        continue;
      }
      freq = nla_get_u32(tb_freq[NL80211_FREQUENCY_ATTR_FREQ]);
      LOWI_LOG_INFO ("wiphy_info_handler, Freq found = %d", freq);
      if (freq <= 3000)
      {
        // 2G frequency
        ch_info->arr_2g_ch[ch_info->num_2g_ch++] = freq;
      }
      else if (freq > 4000 && freq < 6000)
      {
        // 5G frequency
        ch_info->arr_5g_ch[ch_info->num_5g_ch++] = freq;
      }
    }
  }

  return NL_SKIP;
}

/*=============================================================================================
 * Function description:
 *   This function gets the wiphy info with NL80211 Interface.
 *
 * Parameters:
 *   sock: pointer to the NL socket
 *
 * Return value:
 *    error code: NL_OK
 =============================================================================================*/
int nl_get_wiphy_info(struct nl_sock *sock, s_ch_info* ch_info)
{
  struct nl_msg *msg;
  struct nl_cb *cb;
  int ret = 0, idx;

  if (NULL == ch_info || NULL == sock)
  {
    LOWI_LOG_ERROR ( "nl_get_wiphy_info, invalid argument, NULL pointer");
    return -1;
  }

  msg = nlmsg_alloc();
  if (!msg)
    return -ENOMEM;

  cb = nl_cb_alloc(NL_CB_DEFAULT);
  if (!cb) {
    ret = -ENOMEM;
    goto out_fail_cb;
  }

  genlmsg_put(msg, 0, 0, nlstate.nl80211_id, 0,
      NLM_F_DUMP, NL80211_CMD_GET_WIPHY, 0);
  idx = if_nametoindex("wlan0");
  NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, idx);


  ret = -ENOBUFS;
//  NLA_PUT_STRING(msg, CTRL_ATTR_FAMILY_NAME, family);

  ret = nl_send_auto_complete(sock, msg);
  if (ret < 0)
    goto out;

  ret = 1;

  nl_cb_err(cb, NL_CB_CUSTOM, error_handler, &ret);
  nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, ack_handler, &ret);
  nl_cb_set(cb, NL_CB_FINISH, NL_CB_CUSTOM, finish_handler, &ret);
  nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, wiphy_info_handler, ch_info);

  // Not an infinite loop
  while (ret > 0)
  {
    nl_recvmsgs(sock, cb);
  }
  LOWI_LOG_INFO ( "nl_get_wiphy_info, done err = %d", ret);
nla_put_failure:
out:
  nl_cb_put(cb);
out_fail_cb:
  nlmsg_free(msg);
  return ret;
}

/*=============================================================================================
 * Function description:
 *   external interface function to clean up when NL80211 Interface is being closed.
 *
 * Parameters:
 *   none
 *
 * Return value:
 *    none
 =============================================================================================*/
void lowi_gen_nl_drv_close()
{
  nl80211_cleanup(&nlstate);
}

/*=============================================================================================
 * Function description:
 *   The callback function for NL_CB_SEQ_CHECK option using NL80211 Interface.
 *
 * Parameters:
 *   nl_msg: pointer to the result message from NL80211 message
 *   arg: the passed argument via nc_cb_set
 *
 * Return value:
 *    error code: NL_OK
 =============================================================================================*/
static int no_seq_check(struct nl_msg *msg, void *arg)
{
  return NL_OK;
}

/*=============================================================================================
 * Function description:
 *   Register with Wi-fi Host driver for events when Fine Timing Measurement Request
 *   frames are received from an AP.
 *
 * Parameters:
 *   pstate: pointer to the data structure for NL80211 state
 *
 * Return value:
 *    error code: 0, no error
 *                non-0, error
 =============================================================================================*/
static int register_for_ftmrr(struct nl80211_state * pstate)
{
  LOWI_LOG_VERB("######################## Registering for FTMRR \n");
  struct nl_msg * msg;
  struct nl_cb * cb;
  int idx,err = 0, i;
  struct timeval tv;

  idx = if_nametoindex(get_interface_name());
  const uint8 lowi_ftmrr_frame_match[LOWI_FRAME_MATCH_LEN] = "\x05\x00";

  uint16 type = (WLAN_FC_TYPE_MGMT << 2) | (WLAN_FC_STYPE_ACTION << 4);
  LOWI_LOG_DBG ("%s - FTMRR frame type: %u\n",
                __FUNCTION__,
                type);

  for (i = 0; i < LOWI_FRAME_MATCH_LEN; ++i)
  {
    LOWI_LOG_DBG ("%s - FTMRR frame matching matchByte[%d]: %u\n",
                  __FUNCTION__,
                  i,
                  lowi_ftmrr_frame_match[i]);
  }

  LOWI_LOG_DBG ( "Alloc NL Messages\n");
  msg = nlmsg_alloc();
  if (!msg)
  {
    LOWI_LOG_ERROR ( "failed to allocate netlink message\n");
    return 2;
  }

  LOWI_LOG_DBG ( "Alloc Callbacks\n");
  cb = nl_cb_alloc(NL_CB_DEFAULT);
  if (!cb)
  {
    LOWI_LOG_ERROR ( "failed to allocate netlink callbacks\n");
    err = 2;
    goto out_free_msg;
  }

  genlmsg_put(msg, 0, 0, pstate->nl80211_id, 0,
              0, NL80211_CMD_REGISTER_ACTION, 0);
  NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, idx);
  NLA_PUT_U16(msg, NL80211_ATTR_FRAME_TYPE, type);
  NLA_PUT(msg, NL80211_ATTR_FRAME_MATCH, LOWI_FRAME_MATCH_LEN, lowi_ftmrr_frame_match);


  if(0 == gettimeofday(&tv,NULL))
  {
    LOWI_LOG_INFO ( "Sending NL message to register for FTMRR@ %d(s).%d(us)\n",
                    (int)tv.tv_sec,(int)tv.tv_usec);
  }
  err = nl_send_auto_complete(pstate->nl_sock, msg);
  if (err < 0)
    goto out;

  err = 1;

  nl_cb_err(cb, NL_CB_CUSTOM, error_handler, &err);
  nl_cb_set(cb, NL_CB_FINISH, NL_CB_CUSTOM, finish_handler, &err);
  nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, ack_handler, &err);

  // NOTE: Not an infinite loop
  while (err > 0)
    nl_recvmsgs(pstate->nl_sock, cb);

  LOWI_LOG_INFO ("%s: the value err is: %d\n", __FUNCTION__, err);

nla_put_failure:
out:
  nl_cb_put(cb);
out_free_msg:
  nlmsg_free(msg);
  return err;
}

/*=============================================================================================
 * Function description:
 *   Function to set up the sockeet for listeing to
 *   passive scan results with NL80211 Interface.
 *
 * Parameters:
 *   None
 *
 * Return value:
 *    error code: 0, no error
 *                non-0, error
 =============================================================================================*/
static int prepare_listen_events(void)
{
  struct nl80211_state * state = &nlstate;
  int mcid;
  int ret = -1;
  WIPSIW_ENTER
  /* Scan multicast group */
  mcid = nl_get_multicast_id(state->nl_sock, "nl80211", "scan");
  if (mcid >= 0)
  {
    ret = nl_socket_add_membership(state->nl_sock, mcid);
  }
  if (ret == 0) {
    state->scanmcid = mcid;
  }
  WIPSIW_EXIT
  return ret;
}

/*=============================================================================================
 * Function description:
 *   Callback function to nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, wait_event, &wait_ev)
 *
 * Parameters:
 *   nl_msg: pointer to the result message from NL80211 message
 *   arg: the passed argument via nc_cb_set
 *
 * Return value:
 *    error code: 0, no error
 *                non-0, error
 =============================================================================================*/
static int parse_action_frame(struct nl_msg *msg, void *arg)
{
  struct genlmsghdr *genlHdr = nlmsg_data(nlmsg_hdr(msg));
  struct nlattr *attrs[NL80211_ATTR_MAX + 1];
  tANI_U32 freq = 0;

  LOWI_LOG_DBG("%s - wlanFrameStore.numFrames: %u", __FUNCTION__, wlanFrameStore.numFrames);
  nla_parse(attrs, NL80211_ATTR_MAX, genlmsg_attrdata(genlHdr, 0),
            genlmsg_attrlen(genlHdr, 0), NULL);

  if (!attrs[NL80211_ATTR_WIPHY_FREQ])
  {
    LOWI_LOG_DBG("%s: No Source MAC address", __FUNCTION__);
  }
  else
  {
    freq = nla_get_u32(attrs[NL80211_ATTR_WIPHY_FREQ]);
  }
  if (!attrs[NL80211_ATTR_FRAME])
  {
    LOWI_LOG_DBG("%s: No Frame body", __FUNCTION__);
    return -1;
  }

  WlanFrame *wlanFrame = &wlanFrameStore.wlanFrames[wlanFrameStore.numFrames];
  wlanFrame->frameLen = nla_len(attrs[NL80211_ATTR_FRAME]);
  memcpy(wlanFrame->frameBody, nla_data(attrs[NL80211_ATTR_FRAME]), wlanFrame->frameLen);
  wlanFrame->freq = freq;
  wlanFrameStore.numFrames++;

  LOWI_LOG_DBG("%s: Frame received - frame length %u, Total Frames received: %u",
               __FUNCTION__,
               wlanFrame->frameLen,
               wlanFrameStore.numFrames);
  return 0;
}

/*=============================================================================================
 * Function description:
 *   Callback function to nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, wait_event, &wait_ev)
 *
 * Parameters:
 *   nl_msg: pointer to the result message from NL80211 message
 *   arg: the passed argument via nc_cb_set
 *
 * Return value:
 *    error code: 0, no error
 *                non-0, error
 =============================================================================================*/
static int wait_event(struct nl_msg *msg, void *arg)
{
  struct s_wait_event *wait = arg;
  struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
  int i;
  if (( gnlh->cmd == NL80211_CMD_GET_SCAN ) || (gnlh->cmd == NL80211_CMD_TRIGGER_SCAN))
    LOWI_LOG_INFO("In event monitor : captured a scan request issued by some entity, ignoring this event");

  LOWI_LOG_DBG ( "Wait done with Cmd %u", gnlh->cmd);

  for (i = 0; i < wait->n_cmds; i++)
  {
    if (gnlh->cmd == wait->cmds[i])
    {
      wait->cmd = gnlh->cmd;
     LOWI_LOG_DBG ( "We are interested in this Cmd %u", wait->cmd);
    }
  }

  if (gnlh->cmd == NL80211_CMD_FRAME)
  {
    parse_action_frame(msg, arg);
  }

  return NL_SKIP;
}
/*===========================================================================
 * Function description:
 *   Waits on a socket till some data to read becomes available, or a
 *   timeout of 10 seconds happens.
 *
 * Parameters:
 *   nl_sock which has been returned by nl_socket_alloc_cb
 *
 * Return value:
 *   TRUE, if some data is available on socket. FALSE, if timed out or error
 ===========================================================================*/
static int wips_wait_on_nl_socket(struct nl_sock * sock, int timeout_val)
{
  struct timeval tv;
  fd_set read_fd_set;
  int max_fd = -1;
  int retval;

  tv.tv_sec = timeout_val;
  tv.tv_usec = 0;
  FD_ZERO(&read_fd_set);

  // add the read end of the pipe. this will allow a "external entity" to kick the
  // pipe; hence, unblocking the thread is waiting on the socket and a request comes in.
  FD_SET(pipe_fd[0], &read_fd_set);
  if (sock != NULL)
  {
    // get the socket descriptor so the socket can be monitored
    max_fd = nl_socket_get_fd(sock);

    // add the socket descriptor to monitor incoming info on the socket
    FD_SET(max_fd, &read_fd_set);
  }

  if (pipe_fd[0] > max_fd)
  {
    LOWI_LOG_DBG( "wips_wait_on_nl_socket: updated the file descriptor to use pipe...");
    max_fd = pipe_fd[0];
  }

  // monitor until the result comes or the timeout occurs
  if (timeout_val >= 0)
  {
    LOWI_LOG_VERB("issuing a timed select \n");
    retval = select(max_fd+1, &read_fd_set, NULL,NULL,&tv);
  }
  else
  { // monitor forever...
    LOWI_LOG_VERB("issuing a blocking select \n");
    retval = select(max_fd+1, &read_fd_set, NULL,NULL,NULL);
  }

  if (retval == 0) //This means the select timed out
  {
    LOWI_LOG_DBG("No results from the Scan!! Timeout \n");
    retval = ERR_SELECT_TIMEOUT;
    return retval;
  }

  if (retval < 0) //This means the select failed with some error
  {
    LOWI_LOG_ERROR("No results from the Scan!! Error %d\n",errno);
  }

  if ( FD_ISSET( pipe_fd[0], &read_fd_set ) )
  {
    char readbuffer [50];
    int nbytes = read(pipe_fd[0], readbuffer, sizeof(readbuffer));

    // Close the read end of the pipe.
    close (pipe_fd[0]);
    pipe_fd [0] = 0;

    LOWI_LOG_DBG("Received string: %s, requesting socket shutdown \n", readbuffer);
    retval = ERR_SELECT_TERMINATED;
  }

  return retval;
}

/*=============================================================================================
 * Function description:
 *   Function to prepare to listen for passive scan results with NL80211 Interface.
 *
 * Parameters:
 *   pstate: pointer to the data structure for NL80211 state
 *
 * Return value:
 *    error code: 0, no error
 *                non-0, error
 =============================================================================================*/
int do_listen_events(struct nl80211_state *state, int timeout_val)
{
  struct nl_cb *cb = nl_cb_alloc(NL_CB_DEFAULT);
  struct s_wait_event wait_ev;
  int err_code = 0, select_retval;
  #define LOWI_NL_CMD_NUM 4
  static const __u32 cmds[LOWI_NL_CMD_NUM] = {
      NL80211_CMD_UNSPEC,
      NL80211_CMD_NEW_SCAN_RESULTS,
      NL80211_CMD_SCAN_ABORTED,
      NL80211_CMD_FRAME
  };
  WIPSIW_ENTER
  if (!cb)
  {
    LOWI_LOG_ERROR ( "failed to allocate netlink callbacks\n");
    return -ENOMEM;
  }

  /* no sequence checking for multicast messages */
  nl_cb_set(cb, NL_CB_SEQ_CHECK, NL_CB_CUSTOM, no_seq_check, NULL);

  wait_ev.cmds = cmds;
  wait_ev.n_cmds = LOWI_NL_CMD_NUM; //2 == number of events we are waiting for in the cmds array.
  wait_ev.pargs = NULL;
  nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, wait_event, &wait_ev);

  wait_ev.cmd = 0;

  while (!wait_ev.cmd)
  {
    select_retval = wips_wait_on_nl_socket(state->nl_sock, timeout_val );

    // for case a)valid result msg on socket b)scan request issued by someone
    if (select_retval > 0)
    {
      err_code = nl_recvmsgs(state->nl_sock, cb);
      LOWI_LOG_INFO("got messages on Netlink. Cmd %d Err %d\n",wait_ev.cmd,err_code);
    }

    // If the scan got aborted, we do not have to listen anymore.
    if(wait_ev.cmd == NL80211_CMD_SCAN_ABORTED)
    {
      LOWI_LOG_INFO("got scan abort message so returning");
      wait_ev.cmd = -1;
      break;
    }

    if (wait_ev.cmd == NL80211_CMD_FRAME)
    {
      if (timeout_val != -1)
      {
        /* Continue waiting because we are NOT in passiver listening meaning...
           we are waiting for some other events to complete */
        LOWI_LOG_DBG("%s - Continuing to wait on Netlink socket", __FUNCTION__);
        wait_ev.cmd = 0;
        //continue waiting;
      }
      else
      {
        LOWI_LOG_DBG("%s - NL80211_CMD_FRAME received.. stop waiting on Netlink socket - cmd: %u",
                     __FUNCTION__,
                     wait_ev.cmd);
      }
    }

    if (select_retval <= 0)
    {
      LOWI_LOG_DBG("One of the following events occured a) socket timeout b) other error c) shutdown message from pipe ");
      wait_ev.cmd = select_retval;
      break;
    }

    //timeout_val >= 0 &&  err_code < 0 , means that a trigger request was issued, and that when NL socket was read, nothing was returned
    else if (  timeout_val >= 0 &&  err_code < 0 )
    {
      // EAGAIN is acceptable error code and we can go ahead and retrieve
      // the scan dump. EAGAIN error is trying to tell us to get the command
      // again which we already have. No need to get the command again and
      // so we log it at a lower level.
      if (err_code == -EAGAIN)
      {
        LOWI_LOG_VERB("Failed to get valid messages on Netlink. Err %d",err_code);
      }
      else
      {
        LOWI_LOG_WARN("Failed to get valid messages on Netlink. Err %d",err_code);
      }
      wait_ev.cmd = select_retval;
      break;
    }

  }

  LOWI_LOG_DBG("%s - Returning to caller", __FUNCTION__);
  WIPSIW_EXIT
  nl_cb_put(cb);
  return wait_ev.cmd;
}

/*=============================================================================================
 * Function description:
 *   Untility function to print MAC ID in the format of xx:xx:xx:xx:xx:xx
 *
 * Parameters:
 *   mac_addr: output buffer to store the formatted mac address
 *   arg: input mac address
 *
 * Return value:
 *    None
 =============================================================================================*/
void mac_addr_n2a(char *mac_addr, unsigned int maxStrLen, unsigned char *arg)
{
  int i, l;

  l = 0;
  for (i = 0; i < ETH_ALEN ; i++)
  {
    if (i == 0)
    {
      l+=snprintf(mac_addr+l, maxStrLen-l, "%02x", arg[i]);
    }
    else
    {
      l+=snprintf(mac_addr+l, maxStrLen-l, ":%02x", arg[i]);
    }
  }
}

/*=============================================================================================
 * Function description:
 *   Utility function to convert SSID into string. The user must pass a character array of
 *   size WIFI_SSID_SIZE+1. The function converts the non-ASCII character into "_", and leave
 *   the ASCII character alone. The function returns the same exact pointer to the given
 *   buffer for ease of printing.
 *
 * Parameters:
 *   ssid_ptr : Pointer to the memory containing the SSID info
 *   ssid_len : Length of the buffer
 *   ssid_str : Array to store the converted SSID that is safe for printing
 *
 * Return value:
 *   Pointer to the given buffer to store print-safe SSID.
 =============================================================================================*/
const char* wipsiw_convert_ssid_to_string(const uint8* ssid_ptr, uint8 ssid_len, char ssid_str[WIFI_SSID_SIZE+1])
{
  int i;

  /* Just want to copy up to WIFI_SSID_SIZE; cap it if it's more */
  if (ssid_len > WIFI_SSID_SIZE)
  {
    ssid_len = WIFI_SSID_SIZE;
  }
  memcpy(ssid_str, ssid_ptr, ssid_len);

  /* Null terminate the string */
  ssid_str[ssid_len] = '\0';

  /* Go through each character and if it's non-ASCII, convert it to "_" */
  for (i = 0; i < ssid_len; i++)
  {
    /* If it's not printable character */
    if (!isprint(ssid_str[i]))
    {
      ssid_str[i] = '_';
    }
  }

  return ssid_str;
}

/*=============================================================================================
 * Function description:
 *   Untility function to print out IE (information elements) from the passive scan report
 *   using NL80211 Interface. Only SSID will be printed out.
 *
 * Parameters:
 *   ie: pointer to the IE
 *   ielen: length of the IE
 *
 * Return value:
 *    None
 =============================================================================================*/
/** The following are the list of Information elements that LOWI
  * is looking for in the Discovery Scan results
  */
#define COUNTRY_CODE_IE 0x7
#define BSSID_IE 0x00
#define VENDOR_SPECIFIC_IE 0xdd
#define CELL_POWER_INFO_IE 0x96
#define SUPPORTED_RATES_IE 1
#define EXT_SUPPORTED_RATES_IE 50
#define SUPPORTED_RATE_MASK 0x7F
#define BASIC_RATE_MASK    0x80

#define HT_CAP_IE          45
#define HT_CAP_40MHZ_SUPPORTED_BIT 1
#define HT_CAP_PROHIBIT_40MHZ_BIT 14

#define HT_OPERATION_IE    61
#define HT_OP_SEC_CH_NOT_PRESENT 0      /* No Seconday Channel present */
#define HT_OP_SEC_CH_ABOVE_PRIMARY_CH 1 /* Seconday Channel is above the Primary Channel */
#define HT_OP_SEC_CH_BELOW_PRIMARY_CH 3 /* Seconday Channel is below the Primary Channel */
#define VHT_CAP_IE         191
#define VHT_OPERATION_IE   192
#define EXTENDED_CAP_IE    127
#define EXTENDED_CAP_11MC_SUPPORTED_BIT 70
#define EXTENDED_CAP_LOC_CIVIC_SUPPORTED_BIT 14
#define EXTENDED_CAP_LCI_SUPPORTED_BIT 15

#define NUM_11G_RATES 8
static const uint8 elevenGRates[NUM_11G_RATES] = { 6, 9, 12, 18, 24, 36, 48, 54 };

static const unsigned char u_CiscoOUI[3] = { 0x00, 0x40, 0x96 };
#define MSAP_ADVT_IND_IE 0x18 /* MSAP advertisement indicator */
#define RSN_IE 0x30
static const unsigned char u_QcomOUI[3] = { 0x00, 0xA0, 0xC6 };
#define QCOM_BSS_AGE_IE_IND 0x100 /* QCOM BSS Age IE indicator */

static const unsigned char u_CiscoOUI_Ext[4] = { 0x00, 0x40, 0x96, 0x00 };

/**
 * Description:
 *  This function us used to generate the byte number and bit
 *  number within that byte for a given IE sub field.
 *
 * @param[in] elemBitNum: the Bit number of the IE subfield in
 *                    the IE
 * @param[out] elemByteNum: The byte within the IE where the sub
 *                     field resides.
 * @param[out] elemBitNumInByte: The bit within the above byte
 *                        where the sub field resides.
 */
void getByteAndBitForField(uint32 elemBitNum, uint8* elemByteNum, uint8 *elemBitNumInByte)
{
  *elemByteNum = elemBitNum / 8;
  *elemBitNumInByte = elemBitNum % 8;
}

/*=============================================================================
 * is_msap_ie
 *
 * Description:
 *   This function checks the given IE string for an MSAP indication. If the
 *   MSAP indication is found, then MSAP related values are copied into the
 *   given structure pointed by p_ap_scan_res.
 *
 * Parameters:
 *   ie - Pointer to the IE buffer
 *   p_ap_scan_res - Pointer to the structure to store any MSAP info found.
 *
 * Return value:
 *   TRUE - if MSAP flag is found. Memory pointed by p_ap_scan_res is updated.
 *   FALSE - if MSAP flag not found. Memory pointed by p_ap_scan_res is
 *     UNTOUCHED!
 ============================================================================*/
unsigned int is_msap_ie(unsigned char * ie,
                        quipc_wlan_ap_meas_s_type * p_ap_scan_res)
{
  int len;

  //If an IE already has been found, detecting this AP as MSAP enabled, Skip.

  len = ie[1];

  /* Check for CISCO OUI */
  if ((len >= 5) &&
      (memcmp(&ie[2], u_CiscoOUI, sizeof(u_CiscoOUI)) == 0) &&
               (ie[5] == MSAP_ADVT_IND_IE)
     )
  {
    p_ap_scan_res->is_msap = TRUE;
    p_ap_scan_res->msap_prot_ver = ie[6];
    p_ap_scan_res->svr_idx = ie[11];
    memcpy(&(p_ap_scan_res->venue_hash), &ie[7], 4);
    return TRUE;
  }
  return FALSE;
}

/*=============================================================================
 * is_qcom_ie
 *
 * Description:
 *   This function checks to see if the given IE matches the QCOM IE
 *   string, if it does it will return TRUE and save the decoded age & Delta TSF
 *   in the output parameter. Otherwise, FALSE will be returned.
 *
 * Parameters:
 *   ie - Pointer the given IE.
 *   p_bss_age - Pointer to variable to store decoded QCOM age info.
 *   p_bss_deltaTsf - Pointer to variable to store decoded delta in TSF
 *
 * Return value:
 *   TRUE - If the given IE is indeed QCOM age IE.
 *   FALSE - If the given IE is not a QCOM age IE.
 ============================================================================*/
unsigned int is_qcom_ie(unsigned char* ie, int* p_bss_age, uint32* p_bss_deltaTsf)
{
  int len, retval = FALSE;
  len = ie[1];
  unsigned int age_ie_ind;
  int bss_age;

  /* Check for QCOM OUI */
  if (memcmp(&ie[2], u_QcomOUI, sizeof(u_QcomOUI)) == 0)
  {
    /* Then check age indicator */
    memcpy(&age_ie_ind, &ie[5], 4);
    memcpy(&bss_age, &ie[9], 4);
    LOWI_LOG_DBG("Found QCOM OUI %x %x\n",  age_ie_ind, bss_age);
    if (age_ie_ind == QCOM_BSS_AGE_IE_IND)
    {
      *p_bss_age = bss_age;
      retval = TRUE;
      if ((len > 11) && (p_bss_deltaTsf != NULL))
      {
        memcpy(p_bss_deltaTsf, &ie[13], 4);
      }
    }
  }
  return retval;
}

/*=============================================================================
 * decode_cell_power_ie
 *
 * Description:
 *   This function decodes information within the cell power IE and store
 *   desired info within the given buffer. If nothing is found, then the
 *   given structure is UNTOUCHED.
 *
 * Parameters:
 *   ie - Pointer to the cell power IE buffer.
 *   p_ap_scan_res - Pointer to the structure to store any decoded info found.
 *
 * Return value: NONE
 ============================================================================*/
void decode_cell_power_ie
(
  unsigned char* ie,
  quipc_wlan_ap_meas_s_type* p_ap_scan_res
)
{
  int len;

  len = ie[1];

  /* Check for client transmit power IE */
  if ((len >= 6) &&
      (memcmp(&ie[2], u_CiscoOUI_Ext, sizeof(u_CiscoOUI_Ext)) == 0))
  {
    p_ap_scan_res->cell_power_limit_dBm = (int8)ie[6];
  }
}

/*=============================================================================
 * Function description:
 *   Utility function to determine if the data rate is a 11g datarate.
 *
 * Parameters:
 *   rate: data rate
 *
 * Return value:
 *    TRUE or FALSE
 ============================================================================*/
boolean RateIs11g(uint8 rate)
{
  unsigned int i = 0;
  for(i = 0; i < NUM_11G_RATES; i++)
  {
    if (rate == elevenGRates[i])
    {
      return true;
    }
  }

  return false;
}

/*=============================================================================
 * Function description:
 *   Utility function to determine if 11g phy mode should be used.
 *
 * Parameters:
 *   ie: pointer to the IE
 *   phyMode: The PHY Mode result
 *
 * Return value:
 *    None
 ============================================================================*/
void checkFor11gPhyMode(unsigned char *ie, uint8 *phyMode)
{
  if(ie != NULL)
  {
    uint8 numSupRates = ie[1];
    uint8 rateIdx = 2;
    while (numSupRates)
    {
      uint8 rate = ie[rateIdx] & SUPPORTED_RATE_MASK;
      if (RateIs11g(rate))
      {
        *phyMode = ROME_PHY_MODE_11G;
        if (ie[rateIdx] & BASIC_RATE_MASK)
        {
          *phyMode = ROME_PHY_MODE_11GONLY;
          break;
        }
      }
      rateIdx++;
      numSupRates--;
    }
  }
}

/*=============================================================================
 * Function description:
 *   Utility function to determine if HT20 and Ht40 are supported.
 *
 * Parameters:
 *   ie: pointer to the IE
 *   ht20Supported: The pointer to the flag to indicate HT20 support
 *   ht40Supported: The pointer to the flag to indicate HT40 support
 *
 * Return value:
 *    None
 ============================================================================*/
void checkIfHT20AndHT40Supported(unsigned char* ie, uint8 channel, boolean *ht20Supported, boolean *ht40Supported)
{
  if (ie !=NULL)
  {
    *ht20Supported = TRUE;
    LOWI_LOG_DBG("HT 20 supported");
    if (ie[1] > 2)
    {
      uint16 htCapInfo = ((ie[3] << 8) | ie[2]);
      if (htCapInfo & (1 << HT_CAP_40MHZ_SUPPORTED_BIT))
      {
        if (channel > MAX_2G_CHANNEL) /* For 5G always true */
        {
          *ht40Supported = TRUE;
          LOWI_LOG_DBG("HT 40 supported");
        }
        else
        { /* for 2.4G check intollerent bit (should NOT be set) */
          if(!(htCapInfo & (1 << HT_CAP_PROHIBIT_40MHZ_BIT)))
          {
            *ht40Supported = TRUE;
            LOWI_LOG_DBG("HT 40 supported");
          }
        }
      }
    }
  }
}

/*=============================================================================
 * Function description:
 *   Utility function to determine the channel info for HT40 targets.
 *
 * Parameters:
 *   ie: pointer to the IE
 *   u_Chan: Primary channel
 *   u_Band_center_freq1: Secondary Channel
 *   ht40Supported: Flag indicating support for HT40
 *
 * Return value:
 *    None
 ============================================================================*/
void setChannelInfoForHT40(unsigned char* ie, uint8 *u_Chan, uint16 *u_Band_center_freq1, boolean *ht40Supported)
{
  uint8 primaryChan = ie[2];
  uint8 htOpInfo1 = (ie[3] & 0x3);
  *u_Chan = primaryChan;
  *u_Band_center_freq1 = (uint16)getChannelFreq((uint32)*u_Chan);
  switch (htOpInfo1)
  {
    case HT_OP_SEC_CH_NOT_PRESENT: /* No Seconday Channel present */
    {
      *ht40Supported = false;
      LOWI_LOG_DBG(" No Secondary channel present");
      break;
    }
    case HT_OP_SEC_CH_ABOVE_PRIMARY_CH: /* Seconday Channel is above the Primary Channel */
    {
      *u_Band_center_freq1 = (uint16)getChannelFreq((uint32)*u_Chan) + 10;
      break;
    }
    case HT_OP_SEC_CH_BELOW_PRIMARY_CH: /* Seconday Channel is below the Primary Channel */
    {
      *u_Band_center_freq1 = (uint16)getChannelFreq((uint32)*u_Chan) - 10;
      break;
    }
    default:
    {
      LOWI_LOG_WARN("%s: - This should have never hapenned: %u", __FUNCTION__, htOpInfo1);
      break;
    }
  }
  LOWI_LOG_DBG("HT40 - Secondary Channel: %u", *u_Band_center_freq1);
}

/*=============================================================================
 * Function description:
 *   Utility function to determine 802.11mc Ranging support.
 *
 * Parameters:
 *   ie: pointer to the IE
 *   u_Ranging_features_supported: Flag indicating support for 802.11mc Ranging
 *
 * Return value:
 *    None
 ============================================================================*/
void checkFor11mcRangingSupport(unsigned char* ie, uint32 *u_Ranging_features_supported)
{
  uint8 mcSupportbyte, mcSupportbit;
  getByteAndBitForField(EXTENDED_CAP_11MC_SUPPORTED_BIT, &mcSupportbyte, &mcSupportbit);
  if (ie[1] > mcSupportbyte)
  {
    uint8 *extendedCaps = (uint8 *) &ie[2];
    if (extendedCaps[mcSupportbyte] & (1 << mcSupportbit))
    {
      LOWI_LOG_DBG("%s - Target Supports 11mc Ranging", __FUNCTION__);
      *u_Ranging_features_supported = TRUE;
    }
  }
}

/*=============================================================================
 * Function description:
 *   Utility function to determine 802.11mc Ranging support.
 *
 * Parameters:
 *   ie: pointer to the IE
 *   u_location_features_supported: Flags indicating support for LCI and LCR
 *
 * Return value:
 *    None
 ============================================================================*/
void checkForLciLcrSupport(unsigned char* ie, uint32 *u_location_features_supported)
{
  uint8 lcrSupportbyte, lcrSupportbit;
  getByteAndBitForField(EXTENDED_CAP_LOC_CIVIC_SUPPORTED_BIT, &lcrSupportbyte, &lcrSupportbit);
  if (ie[1] > lcrSupportbyte)
  {
    uint8 *extendedCaps = (uint8 *) &ie[2];
    if (extendedCaps[lcrSupportbyte] & (1 << lcrSupportbit))
    {
      LOWI_LOG_DBG("%s - Target Supports LCR information", __FUNCTION__);
      *u_location_features_supported |= WIPS_LOC_CIVIC_SUPPORTED_MASK;
    }
    else
    {
      LOWI_LOG_DBG("%s - Target Does Not Support LCR information", __FUNCTION__);
    }
  }
  uint8 lciSupportbyte, lciSupportbit;
  getByteAndBitForField(EXTENDED_CAP_LCI_SUPPORTED_BIT, &lciSupportbyte, &lciSupportbit);
  if (ie[1] > lciSupportbyte)
  {
    uint8 *extendedCaps = (uint8 *) &ie[2];
    if (extendedCaps[lciSupportbyte] & (1 << lciSupportbit))
    {
      LOWI_LOG_DBG("%s - Target Supports LCI information", __FUNCTION__);
      *u_location_features_supported |= WIPS_LCI_SUPPORTED_MASK;
    }
    else
    {
      LOWI_LOG_DBG("%s - Target Does Not Support LCI information", __FUNCTION__);
    }
  }
}

/*=============================================================================
 * Function description:
 *   Utility function to determine the operating PHY mode for Target.
 *
 * Parameters:
 *   ht20Supported: Flag indicating Support for HT20
 *   ht40Supported: Flag indicating Support for HT40
 *   vht20Supported: Flag indicating Support for VHT20
 *   vht40Supported: Flag indicating Support for VHT40
 *   vht80Supported: Flag indicating Support for VHT80
 *   channel: Primary Operating channel
 *   phyMode: The resultant PHY mode
 *
 * Return value:
 *    None
 ============================================================================*/
void setPhyMode(boolean ht20Supported, boolean ht40Supported, boolean vht20Supported, boolean vht40Supported, boolean vht80Supported, uint8 channel, uint8 *phyMode)
{
  LOWI_LOG_DBG("%s: ht20Supported: %u, ht40Supported: %u, vht20Supported: %u, vht40Supported: %u, vht80Supported: %u, p_ap_scan_res->u_Chan: %u",
               __FUNCTION__,
               ht20Supported,
               ht40Supported,
               vht20Supported,
               vht40Supported,
               vht80Supported,
               channel);

  if (ht20Supported)
  {
    if (ht40Supported)
    {
      *phyMode = IS_2G_CHANNEL(channel) ? ROME_PHY_MODE_11NG_HT40 : ROME_PHY_MODE_11NA_HT40;
    }
    else
    {
      *phyMode = IS_2G_CHANNEL(channel) ? ROME_PHY_MODE_11NG_HT20 : ROME_PHY_MODE_11NA_HT20;
    }
  }
  if (vht20Supported)
  {
    if (vht40Supported)
    {
      if (vht80Supported)
      {
        *phyMode = IS_2G_CHANNEL(channel) ? ROME_PHY_MODE_11AC_VHT80_2G : ROME_PHY_MODE_11AC_VHT80;
      }
      else
      {
        *phyMode = IS_2G_CHANNEL(channel) ? ROME_PHY_MODE_11AC_VHT40_2G : ROME_PHY_MODE_11AC_VHT40;
      }
    }
    else
    {
      *phyMode = IS_2G_CHANNEL(channel) ? ROME_PHY_MODE_11AC_VHT20_2G : ROME_PHY_MODE_11AC_VHT20;
    }
  }
}
/*=============================================================================
 * print_and_scan_ies
 *
 * Function description:
 *   This function decodes the some information elements (IEs) that WLAN
 *   positioning module is interested in and store them in the given structure.
 *
 * Parameters:
 *   ie: pointer to the IE
 *   ielen: length of the IE
 *
 * Return value:
 *    None
 ============================================================================*/
void print_and_scan_ies(unsigned char *ie, int ielen,
                        quipc_wlan_ap_meas_s_type * p_ap_scan_res,
                        int * p_bss_age)
{
  boolean ht20Supported = false;
  boolean ht40Supported = false;
  boolean vht20Supported = false;
  boolean vht40Supported = false;
  boolean vht80Supported = false;
  uint8 phyMode;

  phyMode = IS_2G_CHANNEL(p_ap_scan_res->u_Chan) ? ROME_PHY_MODE_11B : ROME_PHY_MODE_11A;

  while (ielen >= 2 && ielen >= ie[1])
  {

    switch (ie[0])
    {
      case BSSID_IE:
      {
        // already printed out
        break;
      }
      case VENDOR_SPECIFIC_IE:
      {
        uint32 bss_deltaTsf;
        LOWI_LOG_DBG("Found Vendor IE - OUIE: 0x%x, 0x%x, 0x%x",ie[2],ie[3],ie[4]);
        if (is_qcom_ie(ie, p_bss_age, &bss_deltaTsf))
        {
          /* QC Age value is in 10ms unit, multiply by 10 to get ms */
          *p_bss_age *= 10;
          p_ap_scan_res->u_tsfDelta = bss_deltaTsf;
          LOWI_LOG_DBG("Found QCOM IE %d, tsfDelta: %u\n", *p_bss_age, p_ap_scan_res->u_tsfDelta);
        }
        else if (is_msap_ie(ie, p_ap_scan_res))
        {
          LOWI_LOG_DBG("Found MSAP ENABLED AP");
        }
        else if ((ie[1] >= 4) &&
                 (memcmp(&ie[2], "\x00\x50\xf2\x01", 4) == 0))
        {
          p_ap_scan_res->is_secure = TRUE;
        }
        break;
      }

      case RSN_IE:
      {
        p_ap_scan_res->is_secure = TRUE;
        break;
      }
      case CELL_POWER_INFO_IE:
      {
        LOWI_LOG_DBG("Found CELL_POWER_INFO_IE %02x %02x %02x %02x %02x %02x %02x",
                     ie[1], ie[2], ie[3], ie[4], ie[5], ie[6], ie[7]);
        decode_cell_power_ie(ie, p_ap_scan_res);
        break;
      }
      case COUNTRY_CODE_IE:
      {
        // Confirm the length first
        if (ie[1] < 4)
        {
          // As per 802.11d, minimum length should be 8 but we are only interested in
          // country code
          LOWI_LOG_DBG ("Error - COUNTRY CODE IE LEN Invalid = %d", ie[1]);
        }
        else
        {
          LOWI_LOG_DBG ("Found Country Code = %c%c%c", ie[2], ie[3], ie[4]);
          p_ap_scan_res->country_code[0] = ie[2];
          p_ap_scan_res->country_code[1] = ie[3];
          p_ap_scan_res->indoor_outdoor = ie[4];
        }
        break;
      }
      case SUPPORTED_RATES_IE:
      case EXT_SUPPORTED_RATES_IE:
      {
        checkFor11gPhyMode(ie, &phyMode);
        break;
      }
      case HT_CAP_IE:
      {
        LOWI_LOG_DBG("Found HT CAP IE");
        checkIfHT20AndHT40Supported(ie, p_ap_scan_res->u_Chan, &ht20Supported, &ht40Supported);
        break;
      }
      case HT_OPERATION_IE:
      {
        LOWI_LOG_DBG("Found HT Operation IE");
        if (ht40Supported && ie[1] > 2)
        {
          setChannelInfoForHT40(ie, &p_ap_scan_res->u_Chan, &p_ap_scan_res->u_Band_center_freq1, &ht40Supported);
        }
        break;
      }
      case VHT_CAP_IE:
      {
        LOWI_LOG_DBG("VHT Cap Found");
        if (p_ap_scan_res->u_Chan > MAX_2G_CHANNEL)
        {
          vht20Supported = TRUE;
          if (ht40Supported)
          {
            vht40Supported = TRUE;
          }
        }
        else
        {
          LOWI_LOG_DBG("VHT Cap Not Used");
        }
        break;
      }
      case VHT_OPERATION_IE:
      {
        LOWI_LOG_DBG("VHT Operation IE Found");
        if (p_ap_scan_res->u_Chan > MAX_2G_CHANNEL)
        {
          if (ie[1] > 2)
          {
            if (ie[2] > 0) /* 80 OR 160 OR (80 + 80) MHz BW supported */
            {
              vht80Supported = TRUE;
              p_ap_scan_res->u_Band_center_freq1 = (uint16)getChannelFreq((uint32) ie[3]);
              LOWI_LOG_DBG("VHT80 - Band_center_freq1: %u", p_ap_scan_res->u_Band_center_freq1);
            }
          }
        }
        else
        {
          LOWI_LOG_DBG("VHT Operation IE Not Used");
        }
        break;
      }
      case EXTENDED_CAP_IE:
      {
        p_ap_scan_res->u_Ranging_features_supported = FALSE;
        checkFor11mcRangingSupport(ie, &p_ap_scan_res->u_Ranging_features_supported);
        LOWI_LOG_DBG("%s: 11MC Ranging supported: %s ",
                     __FUNCTION__, (p_ap_scan_res->u_Ranging_features_supported) ? "true" : "false");

        /* Check for LCI & LCR suppport */
        p_ap_scan_res->u_location_features_supported = 0;
        checkForLciLcrSupport(ie, &p_ap_scan_res->u_location_features_supported);
        break;
      }
      default:
      {
        LOWI_LOG_DBG("Undefined IE: 0x%x", ie[0]);
        /* Do nothing*/
        break;
      }
    }

    ielen -= ie[1] + 2; //Subtract the remaining IE Length
                        // (by 2+Length of this IE)
    ie += ie[1] + 2;    //move the pointer to the next IE.
  }

  setPhyMode(ht20Supported, ht40Supported, vht20Supported, vht40Supported, vht80Supported, p_ap_scan_res->u_Chan, &phyMode);
  LOWI_LOG_DBG("PHY Mode: %u - %s", phyMode, PHY_MODE[phyMode]);
  p_ap_scan_res->u_info = phyMode;
}

/*=============================================================================================
 * Function description:
 *   This function prepares the nl ap store before the next scan starts.
 *
 * Return value:
 *  None
 =============================================================================================*/
static void init_wips_nl_ap_store_for_next_scan (void)
{
  // Copy new list to current list, so that
  // the new list can be used to save next scan result
  if (wips_nl_ap_store.num_ap_new > 0)
  {
     memset (wips_nl_ap_store.current_list,
             0,
             sizeof (wips_nl_ap_store.current_list));

     memcpy (wips_nl_ap_store.current_list,
             wips_nl_ap_store.new_list,
             sizeof (wips_nl_ap_store.current_list));

     wips_nl_ap_store.num_ap_current = wips_nl_ap_store.num_ap_new;

     memset (wips_nl_ap_store.new_list, 0, sizeof (wips_nl_ap_store.new_list));
     wips_nl_ap_store.num_ap_new = 0;
  }
}

/*=============================================================================================
 * Function description:
 *   This function adjusts the age for the AP.
 *   The reason to adjust the age is that we may or may not get the Age from the driver
 *   based on what driver is used and also in case of age from driver the NL layer does
 *   caching.
 *
 *   NL caching has the following limitation:
 *   If one AP is found in current scan, NL will cache a copy. If subsequent scan does
 *   not find this AP, it will return this cached copy with the same tsf and age for up
 *   to three seconds.
 *
 *   This function will adjust the age of the AP in this case to properly reflect the
 *   cache due to NL layer buffering. For example, say NL fist receives an AP with age of 500ms,
 *   if the next scan result was received 1 second later by NL layer and wlan driver
 *   does not find this AP, NL layer will report this AP with age unchanged of 500 ms
 *   to this module. This function will then adjust the age of the AP to (500ms + 1000ms).
 *
 *   This function will also approximately compute the age of the AP in case there is
 *   no age available from the driver.
 *
 * Parameters:
 *  mac_id_ptr:
 *  tsf: from NL socket
 *  age: from the IE
 *
 * Return value:
 *  the age after adjustment
 =============================================================================================*/
static int32 adjust_age (char* mac_addr_ptr, uint64 tsf, int32 age)
{
  // This function is called to adjust age of each ap and insert it in the new list.
  // num_ap_new contains the number of ap's inserted in the new list so far and incremented
  // only after a new ap is inserted in the new list. So essentially this is an index to the
  // new list where a new ap could potentially be inserted.
  uint32 index = wips_nl_ap_store.num_ap_new;

  LOWI_LOG_DBG("Age - adjust age. Age found is = %d", age);

  do
  {
    // exceeds maximum size. So we do not want to store this AP
    if (index >= (sizeof (wips_nl_ap_store.new_list)/ sizeof (wips_nl_ap_info)))
    {
      LOWI_LOG_DBG("Age - No more space left to store the AP");
      break;
    }

    // If AP is more than AP_AGE_ADJUST_UPPER_LIMIT (10) seconds old, we will not save
    // this AP for subsequent adjustment. As the NL buffering is 3 seconds, and when
    // age is old enough, the adjustment is not going to have significant impact.
    // And this will allow more entries for APs with smaller age that adjustment
    // will be more significant.
    if (age > AP_AGE_ADJUST_UPPER_LIMIT)
    {
      LOWI_LOG_DBG("Age - AGE upper limit reached. Not storing this AP");
      break;
    }

    // Set the defaults
    memcpy (wips_nl_ap_store.new_list[index].bssid,
            mac_addr_ptr,
            sizeof (wips_nl_ap_store.new_list[index].bssid));
    wips_nl_ap_store.new_list[index].bss_tsf_usec = tsf;
    wips_nl_ap_store.new_list[index].bss_meas_recv_msec = lowi_get_time_from_boot();

    // check whether this AP is found in the previous scan. If it was, adjust
    // the age accordingly.
    uint32 i;
    for (i = 0; i < wips_nl_ap_store.num_ap_current; i++)
    {
      // Check if this AP is already found in previous scans
      if ((memcmp (mac_addr_ptr,
                   wips_nl_ap_store.current_list[i].bssid,
                   sizeof (wips_nl_ap_store.current_list[i].bssid)) == 0))
      {
        LOWI_LOG_DBG("Age - Found AP in the store");
        break;
      }
    }

    // Check if the AP was found in the last snapshot or not
    if (i < wips_nl_ap_store.num_ap_current)
    {
      LOWI_LOG_DBG("Age - Adjust age for AP in the store");
      // The AP was seen earlier
      // Check if the tsf is still the same as last time. If that's the case,
      // this is a cached result.
      // In this case, we need to adjust the AP age by time elapsed in between.
      if ( (0 != tsf) && (tsf == wips_nl_ap_store.current_list[i].bss_tsf_usec) )
      {
        LOWI_LOG_DBG("Age - Found AP, tsf has not changed");
        // Check if the time has moved backwards. We have observed
        // this on a few occurrences and ended up calculating age
        // in negative.
        if (wips_nl_ap_store.new_list[index].bss_meas_recv_msec <
            wips_nl_ap_store.current_list[i].bss_meas_recv_msec)
        {
          LOWI_LOG_DBG("Age - Time has moved backwards. Set age to 0");
          age = 0;
        }
        else
        {
          // calculate the time difference between now and when
          // this AP with the same tsf was first seen
          age =
            (int32) (wips_nl_ap_store.new_list[index].bss_meas_recv_msec -
                     wips_nl_ap_store.current_list[i].bss_meas_recv_msec);
        }

        // store age
        wips_nl_ap_store.new_list[index].bss_age_msec = age;

        // save the first recv time in bss_meas_recv_msec for
        // subsequent adjustment
        wips_nl_ap_store.new_list[index].bss_meas_recv_msec =
            wips_nl_ap_store.current_list[i].bss_meas_recv_msec;

        // increase the number of aps in new list
        wips_nl_ap_store.num_ap_new++;

        break;
      }
      else
      {
        LOWI_LOG_DBG("Age - Found AP, tsf has changed or is 0");
      }
    }
    else
    {
      LOWI_LOG_DBG("Age - AP not found in the store");
    }

    // We are here because
    // AP was not found in last scan snapshot
    // OR Found but tsf has changed
    // OR Found but tsf is not available
    LOWI_LOG_DBG("Age - Adjust age for remaining cases");
    // Check if the age information is avialble from the driver
    if (age == AP_AGE_UNSPECIFIED_IN_MSECS)
    {
      // Age not available from driver
      // Store the age as 0 and measurement time as current time
      age = 0;
      wips_nl_ap_store.new_list[index].bss_age_msec = age;

      // Meas recv time already set as current time. No need to set again
    }
    else
    {
      // Age available from driver
      // Store the age that's received from driver
      wips_nl_ap_store.new_list[index].bss_age_msec = age;

      // Store the meas recv time as "cur time - age"
      wips_nl_ap_store.new_list[index].bss_meas_recv_msec -= age;
    }

    // increase the number of aps in new list
    wips_nl_ap_store.num_ap_new++;
  } while (0);

  return age;
}

/*=============================================================================================
 * Function description:
 *   The callback function to process passive scan result using NL80211 Interface.
 *
 * Parameters:
 *   nl_msg: pointer to the result message from NL80211 message
 *   results_buf_ptr: if this is NULL, the result will only be printed on the screen.
 *                    This function assumes that caller has allocated sufficient amount
 *                    of memory to store max APs in the results buf ptr. If that doesn't
 *                    happen - this function WILL CRASH!!
 *
 * Return value:
 *    error code: 0, no error
 *                non-0, error
 =============================================================================================*/
#define WLAN_CAPABILITY_ESS             (1<<0)
#define WLAN_CAPABILITY_IBSS            (1<<1)
#define WLAN_CAPABILITY_CF_POLLABLE     (1<<2)
#define WLAN_CAPABILITY_CF_POLL_REQUEST (1<<3)
#define WLAN_CAPABILITY_PRIVACY         (1<<4)
#define WLAN_CAPABILITY_SHORT_PREAMBLE  (1<<5)
#define WLAN_CAPABILITY_PBCC            (1<<6)
#define WLAN_CAPABILITY_CHANNEL_AGILITY (1<<7)
#define WLAN_CAPABILITY_SPECTRUM_MGMT   (1<<8)
#define WLAN_CAPABILITY_QOS             (1<<9)
#define WLAN_CAPABILITY_SHORT_SLOT_TIME (1<<10)
#define WLAN_CAPABILITY_APSD            (1<<11)
#define WLAN_CAPABILITY_DSSS_OFDM       (1<<13)
#define MAC_STR_LEN 20
static int print_bss_handler(struct nl_msg *msg, void *arg)
{
  struct nlattr *tb[NL80211_ATTR_MAX + 1];
  struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
  struct nlattr *bss[NL80211_BSS_MAX + 1];
  char mac_addr[MAC_STR_LEN], dev[20];
  char * mac_addr_n;
  __u32 bss_status = 0, bss_freq = 0, bss_freq2 = 0, bss_ssid_len = 0, bss_chan = 0;
  int bss_rssi = 0;
  __u16 bss_capa = 0;
  char * bss_ssid_ptr = NULL;
  quipc_wlan_ap_meas_s_type ap_scan_res;
  int age = AP_AGE_UNSPECIFIED_IN_MSECS;
  uint64_t tsf = 0;
  int16 bss_rssi_0p5dBm;
  bool bss_associated = false;
  static struct nla_policy bss_policy[NL80211_BSS_MAX + 1] = {
                [NL80211_BSS_TSF] = { .type = NLA_U64 },
                [NL80211_BSS_FREQUENCY] = { .type = NLA_U32 },
                [NL80211_BSS_BSSID] = { .type = NLA_UNSPEC },
                [NL80211_BSS_BEACON_INTERVAL] = { .type = NLA_U16 },
                [NL80211_BSS_CAPABILITY] = { .type = NLA_U16 },
                [NL80211_BSS_INFORMATION_ELEMENTS] = { .type = NLA_UNSPEC },
                [NL80211_BSS_SIGNAL_MBM] = { .type = NLA_U32 },
                [NL80211_BSS_SIGNAL_UNSPEC] = { .type = NLA_U8 },
                [NL80211_BSS_STATUS] = { .type = NLA_U32 },
                [NL80211_BSS_SEEN_MS_AGO] = { .type = NLA_U32 },
                [NL80211_BSS_BEACON_IES] = { .type = NLA_UNSPEC },
  };
  char * results_buf_ptr = arg;
  char ssid_str[WIFI_SSID_SIZE+1];
  WIPSIW_ENTER
  //Initialize ap_scan_res to 0
  /* Initialize resulting structure */
  memset ((void *)&ap_scan_res, 0, sizeof (ap_scan_res));
  ap_scan_res.is_msap = FALSE;
  ap_scan_res.is_secure = FALSE;
  ap_scan_res.cell_power_limit_dBm = WPOS_CPL_UNAVAILABLE;

  nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
            genlmsg_attrlen(gnlh, 0), NULL);

  if (!tb[NL80211_ATTR_BSS])
  {
    LOWI_LOG_DBG("bss info missing! Msg Size %d",((nlmsg_hdr(msg))->nlmsg_len - NLMSG_HDRLEN));
    genl_integrity_check_fail = TRUE;
    return NL_STOP;
  }

  if (nla_parse_nested(bss, NL80211_BSS_MAX,
                       tb[NL80211_ATTR_BSS],
                       bss_policy))
  {
    LOWI_LOG_ERROR ( "failed to parse nested attributes!\n");
    return NL_SKIP;
  }

  if (!bss[NL80211_BSS_BSSID])
    return NL_SKIP;

  mac_addr_n = nla_data(bss[NL80211_BSS_BSSID]);
  mac_addr_n2a(mac_addr, MAC_STR_LEN, nla_data(bss[NL80211_BSS_BSSID]));
  if_indextoname(nla_get_u32(tb[NL80211_ATTR_IFINDEX]), dev);

  if (bss[NL80211_BSS_STATUS])
  {
    bss_status = nla_get_u32(bss[NL80211_BSS_STATUS]);
    switch (bss_status)
    {
      case NL80211_BSS_STATUS_AUTHENTICATED:
        LOWI_LOG_DBG(" -- authenticated");
        break;
      case NL80211_BSS_STATUS_ASSOCIATED:
        LOWI_LOG_DBG(" -- associated");
        /* Setting flag to Associated*/
        ap_scan_res.u_AssociatedToAp = true;
        bss_associated = true;
        break;
      case NL80211_BSS_STATUS_IBSS_JOINED:
        LOWI_LOG_DBG(" -- joined");
        break;
      default:
        LOWI_LOG_DBG(" -- unknown status: %d",
        nla_get_u32(bss[NL80211_BSS_STATUS]));
        break;
    }
  }
  LOWI_LOG_DBG("\n");

  if (bss[NL80211_BSS_TSF])
  {
    tsf = (uint64_t)nla_get_u64(bss[NL80211_BSS_TSF]);
  }
  else
  {
    LOWI_LOG_DBG("TSF is not reported\n");
  }

  /**
   * IMPORTANT NOTE: THE FOLLOWING CODE IS IN
   * PLACE TO OVERCOME A MISMATCH IN THE KERNEL BETWEEN PL
   * BRANCHES.
   *
   * TODO: After kernel changes are merged, replace
   * "LOWI_NL80211_BSS_BEACON_TSF" with "NL80211_BSS_BEACON_TSF".
   *
   * Reference Kernel Enumeration
   * enum nl80211_bss
   * {
   *   __NL80211_BSS_INVALID,  // 0
   *   NL80211_BSS_BSSID,      // 1
   *   NL80211_BSS_FREQUENCY,  // 2
   *   NL80211_BSS_TSF,        // 3
   *   NL80211_BSS_BEACON_INTERVAL,      // 4
   *   NL80211_BSS_CAPABILITY,           // 5
   *   NL80211_BSS_INFORMATION_ELEMENTS, // 6
   *   NL80211_BSS_SIGNAL_MBM,           // 7
   *   NL80211_BSS_SIGNAL_UNSPEC,        // 8
   *   NL80211_BSS_STATUS,     // 9
   *   NL80211_BSS_SEEN_MS_AGO,// 10
   *   NL80211_BSS_BEACON_IES, // 11
   *   NL80211_BSS_CHAN_WIDTH, // 12
   *   NL80211_BSS_BEACON_TSF, // 13 <=LOWI_NL80211_BSS_BEACON_TSF
   *   NL80211_BSS_PRESP_DATA, // 14
   *
   *   // keep last
   *   __NL80211_BSS_AFTER_LAST,
   *   NL80211_BSS_MAX = __NL80211_BSS_AFTER_LAST - 1  // 14
   * };
   *
   */
#define LOWI_NL80211_BSS_BEACON_TSF 13
  if (LOWI_NL80211_BSS_BEACON_TSF <= NL80211_BSS_MAX)
  {
    if (bss[LOWI_NL80211_BSS_BEACON_TSF])
    {
      uint64_t beaconTsf = 0;
      beaconTsf = (uint64_t)nla_get_u64(bss[LOWI_NL80211_BSS_BEACON_TSF]);
      LOWI_LOG_DBG("Beacon TSF Found: beaconTsf = %"PRIu64", tsf = %"PRIu64"\n", beaconTsf, tsf);
      tsf = (tsf > beaconTsf) ? tsf : beaconTsf;
    }
    else
    {
      LOWI_LOG_DBG("Beacon TSF not reported - scan result from Probe response\n");
    }
  }
  if (bss[NL80211_BSS_FREQUENCY])
  {
    bss_freq = nla_get_u32(bss[NL80211_BSS_FREQUENCY]);

    bss_chan=GetChanIndex(bss_freq);
    LOWI_LOG_DBG("\tchan: %d\n",bss_chan);
    LOWI_LOG_DBG("\tfreq: %d\n",bss_freq);
    /**
     * Warning 1 : u_chan can get assigned 0 as a channel number
     * here.
     * Warning 2 : uint32 being assigned to uint8
     */
    ap_scan_res.u_Chan = bss_chan;
    ap_scan_res.u_Freq = bss_freq;
    ap_scan_res.u_Band_center_freq1 = bss_freq;
  }

  if (bss[NL80211_BSS_BEACON_INTERVAL])
    LOWI_LOG_DBG("\tbeacon interval: %d\n",
           nla_get_u16(bss[NL80211_BSS_BEACON_INTERVAL]));

  if (bss[NL80211_BSS_CAPABILITY])
  {
    __u16 capa = nla_get_u16(bss[NL80211_BSS_CAPABILITY]);
    bss_capa = capa;
    LOWI_LOG_DBG("\tcapability:");
    if (capa & WLAN_CAPABILITY_ESS)
      LOWI_LOG_DBG(" ESS");
    if (capa & WLAN_CAPABILITY_IBSS)
      LOWI_LOG_DBG(" IBSS");
    if (capa & WLAN_CAPABILITY_PRIVACY)
      LOWI_LOG_DBG(" Privacy");
    if (capa & WLAN_CAPABILITY_SHORT_PREAMBLE)
      LOWI_LOG_DBG(" ShortPreamble");
    if (capa & WLAN_CAPABILITY_PBCC)
      LOWI_LOG_DBG(" PBCC");
    if (capa & WLAN_CAPABILITY_CHANNEL_AGILITY)
      LOWI_LOG_DBG(" ChannelAgility");
    if (capa & WLAN_CAPABILITY_SPECTRUM_MGMT)
      LOWI_LOG_DBG(" SpectrumMgmt");
    if (capa & WLAN_CAPABILITY_QOS)
      LOWI_LOG_DBG(" QoS");
    if (capa & WLAN_CAPABILITY_SHORT_SLOT_TIME)
      LOWI_LOG_DBG(" ShortSlotTime");
    if (capa & WLAN_CAPABILITY_APSD)
      LOWI_LOG_DBG(" APSD");
    if (capa & WLAN_CAPABILITY_DSSS_OFDM)
      LOWI_LOG_DBG(" DSSS-OFDM");
    LOWI_LOG_DBG(" (0x%.4x)\n", capa);
  }

  if (bss[NL80211_BSS_SIGNAL_MBM])
  {
    int s = nla_get_u32(bss[NL80211_BSS_SIGNAL_MBM]);
    bss_rssi = s;
    LOWI_LOG_DBG("\tsignal: %d.%.2d dBm\n", s/100, s%100);
  }
  if (bss[NL80211_BSS_SIGNAL_UNSPEC])
  {
    unsigned char s = nla_get_u8(bss[NL80211_BSS_SIGNAL_UNSPEC]);
    bss_rssi = s;
    LOWI_LOG_DBG("\tsignal: %d/100\n", s);
  }

  // The AGE info from the block below may not be available on
  // external drivers
  if (bss[NL80211_BSS_INFORMATION_ELEMENTS] )
  {
    char * ie_data_ptr;
    int  ie_len;
    ie_data_ptr = nla_data(bss[NL80211_BSS_INFORMATION_ELEMENTS]);
    ie_len = nla_len(bss[NL80211_BSS_INFORMATION_ELEMENTS]);
    if (ie_data_ptr[0] == 0)
    {
      bss_ssid_ptr = ie_data_ptr+2;
      bss_ssid_len = ie_data_ptr[1];
    }
    print_and_scan_ies(nla_data(bss[NL80211_BSS_INFORMATION_ELEMENTS]),
              nla_len(bss[NL80211_BSS_INFORMATION_ELEMENTS]),&ap_scan_res, &age);
  }

  bss_freq2 = ap_scan_res.u_Band_center_freq1;

  // There seems to be some issue with bitwidth of bss_rssi, because
  // of which it wraps around..
  //
  // So the issue is further complicated because in AR6K driver, there is a factor of 96dB,
  // so -96dBm is represented as 0 coming out of the AR6K driver.
  //
  // Here is the table that will explain the bug:
  // ==================================================
  // Actual RSSI : Driver RSSI : NL-truncated RSSI : NL-reported RSSI
  // ==================================================
  // -224dBm -128 128   3200
  //  ..      ..  ..    ..
  // -128dBm -32  224  12800
  //  ..      ..  ..    ..
  // -98 dBm  -2  254  15800
  // -97 dBm  -1  255  15900
  // -96 dBm   0    0  -9600
  // -95 dBm   1    1  -9500
  //  ..      ..   ..   ..
  //   0 dBm  96   96   0
  //  ..      ..   ..   ..
  // 31  dBm 127  127  3100
  //
  // So the bug started to show up when the actual RSSI dips below -96 dBm,
  // all of a sudden the rssi becomes a large number, i.e. ~15900. To mitigate this,
  // any large number above 12800, can be considered a negative number. This is
  // safe to assume because RSSI is assumed that it cannot be larger than 128 dBm,
  // and cannot be smaller than -128 dBm. Note that this workaround does not harm RIVA
  // or any driver that does *not* have a bug; because we assume that RSSI cannot be
  // larger than 128dBm, so it won't even kick in for any other driver without a bug!
  if (bss_rssi > 12800)
  {
    bss_rssi -= 25600;
  }

  bss_rssi_0p5dBm = (int16)(((float)bss_rssi/100.0) * 2 - 0.5);
  if (FALSE == ap_scan_res.is_secure)
  {
    ap_scan_res.is_secure =  ((0 == (bss_capa & WLAN_CAPABILITY_PRIVACY))?TRUE:FALSE);
  }
  // Limit the SSID size to be capped at a max, as that is the size of buffer allocated
  // to retain the SSID.
  ap_scan_res.u_lenSSID = (bss_ssid_len > WIFI_SSID_SIZE)?WIFI_SSID_SIZE:bss_ssid_len;

//  if (bss_freq < MAX_FREQ_BANDS24)
  {
    // There seems to be some bug in age for Associated AP
    // set it to AP_AGE_UNSPECIFIED_IN_MSECS.
    //
    // Please note that setting it to 0 or small values will cause issue for
    // cached passive scan. In this usage scenario, when any measurement
    // returned has a smaller age than the specified, the cached scan will be
    // deemed fresh and it will be returned to the caller without triggering
    // fresh passive scan.
    if (bss_associated == true)
    {
      LOWI_LOG_DBG("Age - AP associated");
      age = AP_AGE_UNSPECIFIED_IN_MSECS;
    }

    // Adjust the current age, which could be -1 (third party wifi or associated)
    // or some age (provided by wifi driver) in case of QC wifi.
    age = adjust_age (mac_addr_n, tsf, age);

    // Copy the other fields to the struct
    if (NULL != mac_addr_n)
    {
      memcpy (ap_scan_res.u_MacId, mac_addr_n, WIFI_MAC_ID_SIZE);
    }
    if (NULL != bss_ssid_ptr)
    {
      memcpy(ap_scan_res.u_SSID,bss_ssid_ptr,ap_scan_res.u_lenSSID);
    }
    // Copy the tsf
    ap_scan_res.tsf = tsf;
    if (NULL != results_buf_ptr)
    {
      iwss_bss_insert_record(results_buf_ptr,
                      bss_rssi_0p5dBm,         /* In unit of 0.5 dBm */
                      age,        /* In units of 1 milli-second, -1 means info not available */
                      &ap_scan_res);
    }
    else
    {
      LOWI_LOG_ERROR("%s]%d results_buf_ptr is NULL\n", __func__, __LINE__);
    }
  }

  LOWI_LOG_DBG("BSS %s (%s) Associated to AP: %s Ch %lu(%lu) center_Freq1: %lu TSF %llud, %.2lld:%.2llu:%.2llu.%.3llu RSSI %d.%.2d CPL %d Age %d MSAP %d SSID %s tsf_Delta: 0x%x",
                mac_addr, dev, (ap_scan_res.u_AssociatedToAp) ? "TRUE" : "FALSE",bss_chan, bss_freq, bss_freq2,
                tsf/1000/1000/60/60/24, (tsf/1000/1000/60/60) % 24,
                (tsf/1000/1000/60) % 60, (tsf/1000/1000) % 60, (tsf/1000) % 1000, bss_rssi/100, bss_rssi%100,
                ap_scan_res.cell_power_limit_dBm, age,
                ap_scan_res.is_msap, wipsiw_convert_ssid_to_string(ap_scan_res.u_SSID, ap_scan_res.u_lenSSID, ssid_str), ap_scan_res.u_tsfDelta);
  WIPSIW_EXIT
  return NL_SKIP;
}


/*=============================================================================================
 * Function description:
 *   Set up various callbacks to process the passive scan results using NL80211 Interface.
 *   If the scan is succesful, the result will be processed by print_bss_handler.
 *
 * Parameters:
 *   pstate: pointer to the data structure for NL80211 state
 *   results_buf_tpr: ptr to store the scan results
 *
 * Return value:
 *    error code: 0, no error
 *                non-0, error
 =============================================================================================*/
static int do_scan_dump(struct nl80211_state *p_nlstate,
                        char * results_buf_ptr)
{
  struct nl_msg * msg;
  struct nl_cb * cb;
  int idx,err;
  WIPSIW_ENTER
  idx = if_nametoindex(get_interface_name());

  num_acks_for_dump = 1; //Just to satisfy the while condition
  num_finishes_for_dump = 0;
  while ((num_finishes_for_dump == 0)&&(num_acks_for_dump > 0))
  {
    num_acks_for_dump = 0;
    num_finishes_for_dump = 0;
    genl_integrity_check_fail = FALSE;
    iwss_bss_reset_records(results_buf_ptr);
    msg = nlmsg_alloc();
    if (!msg)
    {
      LOWI_LOG_ERROR ( "failed to allocate netlink message\n");
      return 2;
    }
    cb = nl_cb_alloc(NL_CB_DEFAULT);
    if (!cb)
    {
      LOWI_LOG_ERROR ( "failed to allocate netlink callbacks\n");
      err = 2;
      goto out_free_msg;
    }

    genlmsg_put(msg, 0, 0, nlstate.nl80211_id, 0,
            (NLM_F_ROOT|NLM_F_MATCH), NL80211_CMD_GET_SCAN, 0);

    err = 3;
    NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, idx);
    err = 0;
    //Here, do whatever the handle_scan_dump function does...
    //Should set a pointer to the buffer where the scan results
    //will arrive and also a callback function which will be called
    //with this buffer pointer as a parameter.
    nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, print_bss_handler,results_buf_ptr);
    err = nl_send_auto_complete(nlstate.nl_sock, msg);
    if (err < 0)
    {
      LOWI_LOG_WARN ( "Failed to send the request to get the scan results to nl. Retry\n");
      num_acks_for_dump = 1; //Just to satisfy the while condition
      num_finishes_for_dump = 0;
      goto nla_put_failure;
    }

    err = 1; // Set err to 1, so that we can wait for err to become 0
    // after sending the command. err is set to 0 by finish_handler or
    // ack handler.
    nl_cb_err(cb, NL_CB_CUSTOM, dump_error_handler, &err);
    nl_cb_set(cb, NL_CB_FINISH, NL_CB_CUSTOM, dump_finish_handler, &err);
    nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, dump_ack_handler, &err);

    // DO NOTE THAT IF FOR WHATEVER REASON ERROR HANDLER IS CALLED -
    // WE SPIN FOREVER HERE !!
    // NOTE: Not an infinite loop
    while ((err > 0) && (FALSE == genl_integrity_check_fail) )
      nl_recvmsgs(nlstate.nl_sock, cb);

    if ((genl_integrity_check_fail == TRUE) && (num_finishes_for_dump == 0))
    {
      num_acks_for_dump = 1;
      genl_integrity_check_fail = FALSE;
      LOWI_LOG_ERROR ( "GENL Msg Integrity Check Failed");
    }

nla_put_failure:
  nl_cb_put(cb); //Free the CB.
out_free_msg:
  nlmsg_free(msg); //Free the allocated Message
    if ((num_finishes_for_dump == 0) && (num_acks_for_dump > 0))
    {
      LOWI_LOG_ERROR ( "RETRY: Finish NL Msg err code %d Num Finish %d Num Ack %d",
                  err,num_finishes_for_dump,num_acks_for_dump);
     nl80211_cleanup(&nlstate);
     nl80211_init(&nlstate);
    }
  }
  iwss_bss_close_record(results_buf_ptr);
  WIPSIW_EXIT
  return err;
}

/*=============================================================================================
 * Function description:
 *   Trigger the passive scan results from NL80211 Interface.
 *
 * Parameters:
 *   pstate: pointer to the data structure for NL80211 state
 *   pFreq: pointer to the frequency's that needs to be scanned.
 *   num_of_freq: Number of frequency's that needs to be scanned.
 *
 * Return value:
 *    error code: 0, no error
 *                non-0, error
 =============================================================================================*/
static int do_trigger_scan(struct nl80211_state * pstate, int* pFreq, int num_of_freq)
{
  LOWI_LOG_VERB("######################## in wipsiw do trigger scan \n");
  struct nl_msg * msg;
  struct nl_msg *ssids = NULL, *freqs = NULL;
  struct nl_cb * cb;
  int idx,err, i;
  struct timeval tv;

  WIPSIW_ENTER
  if ( (NULL == pFreq) || (0 == num_of_freq) )
  {
    LOWI_LOG_ERROR ("No frequency specified to be scanned");
    return -1;
  }

  idx = if_nametoindex(get_interface_name());
  LOWI_LOG_DBG ( "Alloc NL Messages\n");
  msg = nlmsg_alloc();
  if (!msg)
  {
    LOWI_LOG_ERROR ( "failed to allocate netlink message\n");
    return 2;
  }

  ssids = nlmsg_alloc();
  if (!ssids) {
    LOWI_LOG_ERROR ( "failed to allocate ssid space\n");
    return -ENOMEM;
  }

  freqs = nlmsg_alloc();
  if (!freqs)
  {
    nlmsg_free(ssids);
    fprintf(stderr, "failed to allocate freq space\n");
    return -ENOMEM;
  }

  LOWI_LOG_DBG ( "Alloc Callbacks\n");
  cb = nl_cb_alloc(NL_CB_DEFAULT);
  if (!cb)
  {
    LOWI_LOG_ERROR ( "failed to allocate netlink callbacks\n");
    err = 2;
    goto out_free_msg;
  }

  for ( i=0;i < num_of_freq; i++)
  {
    LOWI_LOG_DBG ("Freq added = %d\n", pFreq[i]);
    NLA_PUT_U32(freqs, i+1, pFreq[i]);
  }


  genlmsg_put(msg, 0, 0, nlstate.nl80211_id, 0,
      0, NL80211_CMD_TRIGGER_SCAN, 0);

  NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, idx);
  //Here, add whatever other options for scan..like filling in ssid, freqs etc..

  // note: Appending SSID ATTRIB structure to the message. With zero payload. Doing this to circumvent RIVA issue.
  NLA_PUT(ssids, 1, 0, "");

  // adding the required frequencies
  nla_put_nested(msg, NL80211_ATTR_SCAN_FREQUENCIES, freqs);

  // adding dummy ssid
  nla_put_nested(msg, NL80211_ATTR_SCAN_SSIDS, ssids);

  //Here, add whatever other options for scan..like filling in ssid, freqs etc..

  if (0 == gettimeofday(&tv, NULL))
  {
    LOWI_LOG_INFO ( "Sending NL message Trigger Scan@ %d(s).%d(us)\n",(int)tv.tv_sec,(int)tv.tv_usec);
  }
  wipsiw_scan_req_time = lowi_get_time_from_boot();
  err = nl_send_auto_complete(nlstate.nl_sock, msg);
  if (err < 0)
    goto out;

  err = 1;

  nl_cb_err(cb, NL_CB_CUSTOM, error_handler, &err);
  nl_cb_set(cb, NL_CB_FINISH, NL_CB_CUSTOM, finish_handler, &err);
  nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, ack_handler, &err);

  // NOTE: Not an infinite loop
  while (err > 0)
    nl_recvmsgs(nlstate.nl_sock, cb);

  LOWI_LOG_INFO ("Trigger Scan: the value err is: %d\n", err);
  WIPSIW_EXIT
out:
  nl_cb_put(cb);
out_free_msg:
  nlmsg_free(msg);
  nlmsg_free(ssids);
  nlmsg_free(freqs);
  return err;
nla_put_failure:
  nl_cb_put(cb);
  nlmsg_free(msg);
  nlmsg_free(ssids);
  nlmsg_free(freqs);
  return 2;
}

/*=============================================================================================
 * Function description:
 *   Performs Passive Scan using NL80211 Interface.
 *
 * Parameters:
 *   results_buf_ptr: if this is NULL, the result will only be printed on the screen.
 *                    This function assumes that caller has allocated sufficient amount
 *                    of memory to store max APs in the results buf ptr. If that doesn't
 *                    happen - this function WILL CRASH!!
 *
 *   cached: whether cached result can be used or no. Currently it is assumed the function
 *           is called with cached set to FALSE.
 *   pFreq: pointer to the frequency's that needs to be scanned.
 *   num_of_freq: Number of frequency's that needs to be scanned.
 *
 * Return value:
 *    error code
 =============================================================================================*/
int WipsScanUsingNL(char * results_buf_ptr,int cached, int timeout_val, int* pFreq, int num_of_freq)
{
  int err;
  struct timeval tv;
  uint64 req_time;
  WIPSIW_ENTER
  // Initialize the nl AP store
  init_wips_nl_ap_store_for_next_scan();

  req_time = lowi_get_time_from_boot();

  err = nl80211_init(&nlstate);
  if (0 == err)
  {
    do
    {
      // Only continue if the init is successful.
      if (FALSE == cached)
      {
        if ( timeout_val >= 0)
        {
          err = do_trigger_scan(&nlstate, pFreq, num_of_freq);
          if (-EINVAL == err || -ENODEV == err)
          {
            LOWI_LOG_ERROR ("WipsScanUsingNL - Trigger scan failed with err: %d\n", err);
            break;
          }
          // For all other unknown error cases we could potentially leverage the scans
          // triggered by wpa_supplicant so we should continue.
        }

        // Now that a scan has been issued, listen to incoming events on Scan multicast socket
        LOWI_LOG_DBG ( "Preparing to wait for netlink message\n");
        prepare_listen_events();
        LOWI_LOG_DBG ( "wait for netlink message\n");
        err = do_listen_events(&nlstate, timeout_val);
      }
      else
      {
        // In case of a cached scan request also, initialize the request time
        wipsiw_scan_req_time = lowi_get_time_from_boot ();
      }

      if (0 == gettimeofday(&tv, NULL))
      {
        LOWI_LOG_DBG ( "Got Done. Send dump@ %d(s).%d(us)\n",(int)tv.tv_sec,(int)tv.tv_usec);
      }


      if (err >= 0)
      {
        do_scan_dump(&nlstate,results_buf_ptr);
      }
      else
      {
        LOWI_LOG_DBG ( "No dump done. Error %d",err);
      }
    }
    while (0);

    LOWI_LOG_INFO ("WIPSIW: passive scan cached %d, takes %d ms, err = %d",
        cached, (uint32) (lowi_get_time_from_boot() - req_time), err);
  }
  else
  {
    //Error in NL socket creation
    //If doing Passive scan, just wait for next command from Controller
    // for 5 seconds
    if (timeout_val < 0)
    {
      err = wips_wait_on_nl_socket(NULL, WIPS_WAIT_NL_FAIL);
    }
  }
  WIPSIW_EXIT
  return err;
}

/*=============================================================================================
 * Function description:
 *   Looks up Wiphy info using NL80211 Interface and parses the supported channels.
 *
 * Parameters:
 *   s_ch_info: Contains the 2G and 5G supported channels
 *
 * Return value:
 *    error code - 0 = SUCCESS
 =============================================================================================*/
int WipsGetSupportedChannels(s_ch_info* p_ch_info)
{
  int err;
  LOWI_LOG_DBG ( "WipsGetSupportedChannels\n");
  if (NULL == p_ch_info)
  {
    LOWI_LOG_ERROR ( "WipsGetSupportedChannels invalid argument\n");
    return -1;
  }

  err = nl80211_init(&nlstate);
  if (0 == err)
  {
    p_ch_info->num_2g_ch = 0;
    p_ch_info->num_5g_ch = 0;
    err = nl_get_wiphy_info (nlstate.nl_sock, p_ch_info);
  }
  LOWI_LOG_DBG ( "WipsGetSupportedChannels err = %d\n", err);
  return err;
}

int WipsSendActionFrame(uint8* frameBody,
                        uint32 frameLen,
                        uint32 freq,
                        uint8 destMac[BSSID_SIZE],
                        uint8 selfMac[BSSID_SIZE])
{
  WIPSIW_ENTER
  LOWI_LOG_VERB("######################## Sending out Action Frame \n");
  struct nl_msg * msg;
  struct nl_msg *ssids = NULL, *freqs = NULL;
  struct nl_cb * cb;
  int idx,err = 0, i, l=0;
  struct timeval tv;
  struct nl80211_state* pstate = &nlstate;
  uint8 frameBuff[5000];
  char frameChar[5000];
  unsigned long long int errCode = 0;
  tANI_U32 dur = 20;

  LOWI_LOG_DBG ( ">>> ENTER: %s  frameLen(%u) freq(%u) srcMac(" QUIPC_MACADDR_FMT ") selfMac(" QUIPC_MACADDR_FMT ")\n",
                 __FUNCTION__, frameLen, freq, destMac, selfMac);

  Wlan80211FrameHeader frameHeader;
  memset(&frameHeader, 0, sizeof(Wlan80211FrameHeader));

  frameHeader.frameControl = IEEE80211_FRAME_CTRL(WLAN_FC_TYPE_MGMT, WLAN_FC_STYPE_ACTION);
  memcpy(frameHeader.addr1, destMac, BSSID_SIZE);
  memcpy(frameHeader.addr2, selfMac, BSSID_SIZE);
  memcpy(frameHeader.addr3, destMac, BSSID_SIZE);

  LOWI_LOG_DBG ( ">>> %s  sizes: frameHeader(%u) frameHeader.frameControl(%u) frameHeader.addr1(%u) frameHeader.addr2(%u) frameHeader.addr3(%u)\n",
                 __FUNCTION__,
                 sizeof(frameHeader),
                 sizeof(frameHeader.frameControl),
                 sizeof(frameHeader.addr1),
                 sizeof(frameHeader.addr2),
                 sizeof(frameHeader.addr3));

  LOWI_LOG_DBG("%s - AP Mac " QUIPC_MACADDR_FMT " Self Mac " QUIPC_MACADDR_FMT " BSSID Mac " QUIPC_MACADDR_FMT,
               __FUNCTION__,
               QUIPC_MACADDR(frameHeader.addr1),
               QUIPC_MACADDR(frameHeader.addr2),
               QUIPC_MACADDR(frameHeader.addr3));

  memset(frameBuff, 0, sizeof(frameBuff));
  memcpy(frameBuff, &frameHeader, sizeof(frameHeader));

  memcpy((frameBuff + sizeof(frameHeader)), frameBody, frameLen);
  idx = if_nametoindex(get_interface_name());

  LOWI_LOG_DBG ( "Alloc NL Messages\n");
  msg = nlmsg_alloc();
  if (!msg)
  {
    LOWI_LOG_DBG ( "failed to allocate netlink message\n");
    return 2;
  }

  LOWI_LOG_DBG ( "Alloc Callbacks\n");
  cb = nl_cb_alloc(NL_CB_DEFAULT);
  if (!cb)
  {
    LOWI_LOG_DBG ( "failed to allocate netlink callbacks\n");
    err = 2;
    goto out_free_msg;
  }

  errCode = genlmsg_put(msg, 0, 0, pstate->nl80211_id, 0,
                        0, NL80211_CMD_FRAME, 0);
  LOWI_LOG_DBG( "%s - Error Code when adding command: %d\n",__FUNCTION__,errCode);
  NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, idx);
  /* Add Channel Here */
  NLA_PUT_U32(msg, NL80211_ATTR_WIPHY_FREQ, freq);
  /* Add Wait Duration Here */
  NLA_PUT_U32(msg, NL80211_ATTR_DURATION, dur);
  NLA_PUT_FLAG(msg, NL80211_ATTR_TX_NO_CCK_RATE);
  /* Add ACK or No ACK flag Here */
  NLA_PUT_FLAG(msg, NL80211_ATTR_DONT_WAIT_FOR_ACK);
  /* Add Frame here */
  NLA_PUT(msg, NL80211_ATTR_FRAME, (frameLen + sizeof(frameHeader)), frameBuff);

  for (i = 0; i < (frameLen + sizeof(frameHeader)); i++)
  {
    l+=snprintf(frameChar+l, 10, "0x%02x ", frameBuff[i]);
  }

  LOWI_LOG_DBG("%s - The Final Frame: %s", __FUNCTION__, frameChar);

  if (0 == gettimeofday(&tv, NULL))
  {
    LOWI_LOG_DBG( "%s - Sending NL message with Frame@ %d(s).%d(us)\n",
                    __FUNCTION__,(int)tv.tv_sec,(int)tv.tv_usec);
  }
  err = nl_send_auto_complete(pstate->nl_sock, msg);
  if (err < 0)
    goto out;

  err = 1;

  nl_cb_err(cb, NL_CB_CUSTOM, error_handler, &err);
  nl_cb_set(cb, NL_CB_FINISH, NL_CB_CUSTOM, finish_handler, &err);
  nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, ack_handler, &err);

  // NOTE: Not an infinite loop
  while (err > 0)
    nl_recvmsgs(pstate->nl_sock, cb);

  LOWI_LOG_DBG ("%s: the value err is: %d\n", __FUNCTION__, err);

nla_put_failure:
out:
  nl_cb_put(cb);
out_free_msg:
  nlmsg_free(msg);
  WIPSIW_EXIT
  return err;
}
