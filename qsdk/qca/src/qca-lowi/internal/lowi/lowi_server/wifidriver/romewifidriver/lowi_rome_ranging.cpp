/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

   NL80211 Interface for ranging scan

   GENERAL DESCRIPTION
   This component performs ranging scan with NL80211 Interface.

Copyright (c) 2013-2016 Qualcomm Technologies, Inc.
All Rights Reserved.
Confidential and Proprietary - Qualcomm Technologies, Inc.

Copyright (c) 2013 Qualcomm Atheros, Inc.
All Rights Reserved.
Qualcomm Atheros Confidential and Proprietary.

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

=============================================================================*/

#define LOG_NDEBUG 0
#include <osal/os_api.h>
#include <osal/commontypes.h>
#include <common/lowi_utils.h>
#include <base_util/log.h>
#include <sys/param.h>
#include "rttm.h"
#include "wlan_location_defs.h"
#include "lowi_rome_ranging.h"
#include "wifiscanner.h"
#include "wipsiw.h"
#include "wlan_capabilities.h"
#include "lowi_p2p_ranging.h"
#include "lowi_nl80211.h"

#define MAX_NLMSG_LEN 5120   /* 5K Buffer for Netlink Message */
// This needs to be further evaluated and logged under different log levels.
#ifdef LOG_TAG
  #undef LOG_TAG
#endif
#define LOG_TAG "LOWI-ROME-RTT"

#ifdef LOWI_ON_ACCESS_POINT
#define RTT2_OFFSET (0)
#else
// AR6K - RTT offset to bring it down to RIVA level
/* 115770 = (5100 * 22.7) * 10  This offset number was copied over from AR6K (units 0.1ns)*/
#define RTT2_OFFSET (1157700)
#endif

/** This is MAX RTT3 value acceptable in ps units
 *  1200000 = 180m  (12000 * 0.03 / 2)*100 */
#define LOWI_RTT3_MAX 1200000

/** Minimum RTT value provided to the user (pico sec) */
#define RTT3_MIN_DEFAULT 1

/* RTT timeout per BSSID target in milliseconds - set to 50 ms */
#define RTT_TIMEOUT_PER_TARGET 50

#define OFFSET_30MHZ 30

#define LOWI_PHY_MODE_MASK 0x3F
#define LOWI_PHY_MODE_11A 0
#define LOWI_PHY_MODE_11G 1

//TODO: LOWI-AP: Remove following once RTT capability bit set by
//host
#define LOWI_RTT_ALWAYS_SUPPORTED

using namespace qc_loc_fw;

static int  pipe_ranging_fd[2];          /* Pipe used to terminate select in Ranging thread */
static char rxBuff[MAX_NLMSG_LEN];       /* Buffer used to store in coming messages */

ChannelInfo channelInfoArray[MAX_CHANNEL_ID];

/* msg strings used for printing/debugging PREAMBLE */
static const char* PREAMBLE_DBG[ROME_PREAMBLE_VHT+1] = {
  "PREAMBLE_LEGACY",
  "PREAMBLE_CCK",
  "PREAMBLE_HT",
  "PREAMBLE_VHT"
};

/* msg strings used for printing/debugging TX & RX BW */
static const char* TX_RX_BW[ROME_BW_160MHZ+1] = {
  "BW_20MHZ",
  "BW_40MHZ",
  "BW_80MHZ",
  "BW_160MHZ"
};

/* msg strings used for printing/debugging RX BW */
static const char* RX_BW[4] = {
  "LEGACY_20",
  "VHT20",
  "VHT40",
  "VHT80"
};

/* msg strings used for printing/debugging PKT type */
static const char* RTT_PKT_TYPE[RTT_MEAS_FRAME_TMR+1] = {
  "RTT_MEAS_FRAME_NULL",
  "RTT_MEAS_FRAME_QOSNULL",
  "RTT_MEAS_FRAME_TMR"
};

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

static int  nl_sock_fd = 0;
VDevInfo vDevInfo;
tANI_U8 vDevId;
bool romeWipsReady = FALSE;
tANI_U8 rxChainsUsed = 0;

/* WLAN frame parameters */
static uint8 gDialogTok = 1;
static uint8 gMeasTok = 1;

vector <FailedTarget> failedTargets;

/*=============================================================================================
 * Function description:
 *   Takes care of setting up the Netlink Socket and binds its the required address
 *
 * Parameters:
 *   NONE
 *
 * Return value:
 *    Valid Socket File Descriptor or a negative Error code.
 *
 =============================================================================================*/
static int create_nl_sock()
{
  int nl_sock_fd;
  int on = 1;
  int return_status;
  struct sockaddr_nl src_addr;
#ifdef LOWI_ON_ACCESS_POINT
  log_verbose(LOG_TAG, "%s: About to create net link socket\n", __FUNCTION__);
  nl_sock_fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_LOWI);
#else
  nl_sock_fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_USERSOCK);
#endif
  if( nl_sock_fd < 0 )
  {
    log_error(LOG_TAG, "%s: Failed to create Socket err:%d\n", __FUNCTION__, errno);
    return nl_sock_fd;
  }
  else
  {
    log_verbose(LOG_TAG, "%s: Succeeded in creating Socket\n", __FUNCTION__);
  }
  memset(&src_addr, 0, sizeof(src_addr));
  src_addr.nl_family = PF_NETLINK;
  src_addr.nl_pid = getpid();  /* self pid */
  /* interested in group 1<<0 */
  src_addr.nl_groups = 0;

  return_status = setsockopt(nl_sock_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
  if( return_status < 0 )
  {
    log_error(LOG_TAG, "%s: nl socket option failed\n", __FUNCTION__);
    close(nl_sock_fd);
    return return_status;
  }
  else
  {
    log_verbose(LOG_TAG, "%s: Options set successfully\n", __FUNCTION__);
  }
  return_status = bind(nl_sock_fd, (struct sockaddr*)&src_addr, sizeof(src_addr));
  if( return_status < 0 )
  {
    log_verbose(LOG_TAG, "%s: BIND errno=%d - %s\n", __FUNCTION__, errno, strerror(errno));
    close(nl_sock_fd);
    return return_status;
  }
  else
  {
    log_verbose(LOG_TAG, "%s: Binding done successfully\n", __FUNCTION__);
  }

  return nl_sock_fd;
}

/*=============================================================================================
 * Function description:
 *   Sends a netlink message via socket to the kernel
 *
 * Parameters:
 *   fd:  socket file descriptor
 *   data:  pointer to data to be sent
 *   len:   length of data to be sent
 *
 * Return value:
 *    error code: 0 = Success, -1 = Failure
 *
 =============================================================================================*/
static int send_nl_msg(int fd,char *data,unsigned int len)
{
  struct sockaddr_nl d_nladdr;
  struct msghdr msg ;
  struct iovec iov;
  struct nlmsghdr *nlh=NULL;

  nlh = (struct nlmsghdr *)calloc(NLMSG_SPACE(len),1);
  if(!nlh) /* Memory allocation failed */
  {
    return -1;
  }

  /* destination address */
  memset(&d_nladdr, 0 ,sizeof(d_nladdr));
  d_nladdr.nl_family= AF_NETLINK ;
  d_nladdr.nl_pad=0;
  d_nladdr.nl_pid = 0; /* destined to kernel */

  /* Fill the netlink message header */
  memset(nlh , 0 , len);

  nlh->nlmsg_len =NLMSG_LENGTH(len);
  nlh->nlmsg_type = WLAN_NL_MSG_OEM;
  nlh->nlmsg_flags = NLM_F_REQUEST;
  nlh->nlmsg_pid = getpid();

  /*iov structure */
  iov.iov_base = (void *)nlh;
  iov.iov_len = nlh->nlmsg_len;
  /* msg */
  memset(&msg,0,sizeof(msg));
  msg.msg_name = (void *) &d_nladdr ;
  msg.msg_namelen=sizeof(d_nladdr);
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

  memcpy(NLMSG_DATA(nlh), data,len );

  if(sendmsg(fd, &msg, 0) < 0)
  {
    log_error(LOG_TAG, "%s: Failed to send message over NL, errno :%d - %s\n", __FUNCTION__, errno, strerror(errno));
    free(nlh);
    return -1;
  }
  else
  {
    log_verbose(LOG_TAG, "%s: Successfully sent message over NL \n", __FUNCTION__);
    free(nlh);
    return 0;
  }
}

/*=============================================================================================
 * Function description:
 *   Recvs a netlink message to via socket from  the kernel
 *
 * Parameters:
 *   fd:  socket file descriptor
 *   data:  pointer to data to be sent
 *   len:   length of data to be sent
 *
 * Return value:
 *    error code: 0 = Success, -1 = Failure
 *
 =============================================================================================*/
static int recv_nl_msg(int fd, char *data, unsigned int len)
{
  struct sockaddr_nl d_nladdr;
  struct msghdr msg;
  struct iovec iov;
  struct nlmsghdr *nlh = NULL;
  char *recvdata = NULL;
  int retVal = 0;

  nlh = (struct nlmsghdr *)calloc(NLMSG_SPACE(MAX_NLMSG_LEN), 1);
  if (!nlh) /* Memory Allocation Failed */
  {
    return -1;
  }
  /* Source address */
  memset(&d_nladdr, 0, sizeof(d_nladdr));
  d_nladdr.nl_family = AF_NETLINK;
  d_nladdr.nl_pad = 0;
  d_nladdr.nl_pid = 0; /* Source from kernel */

  memset(nlh, 0, MAX_NLMSG_LEN);

  nlh->nlmsg_len = NLMSG_SPACE(MAX_NLMSG_LEN);
  nlh->nlmsg_type = WLAN_NL_MSG_OEM;
  nlh->nlmsg_flags = 0;
  nlh->nlmsg_pid = getpid();

  /*iov structure */
  iov.iov_base = (void *)nlh;
  iov.iov_len = MAX_NLMSG_LEN;
  /* msg */
  memset(&msg, 0, sizeof(msg));
  msg.msg_name = (void *)&d_nladdr;
  msg.msg_namelen = sizeof(d_nladdr);
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  log_verbose(LOG_TAG, "Waiting for NL Msg from Driver\n");
  retVal = recvmsg(fd, &msg, 0);

  if (retVal < 0)
  {
    log_error(LOG_TAG, "%s: Failed to recv message over NL \n", __FUNCTION__);
  }
  else if (retVal == 0)
  {
    log_error(LOG_TAG, "%s: No pending message or peer is gone \n", __FUNCTION__);
  }
  else
  {
    log_verbose(LOG_TAG, "%s: Successfully recvd message over NL: retVal - %d & Msg Len - %d\n", __FUNCTION__, retVal, nlh->nlmsg_len);
    recvdata = (char *)NLMSG_DATA(nlh);

    /* Actual lenght of data in NL Message */
    retVal = nlh->nlmsg_len - NLMSG_HDRLEN;
    if (retVal > ((int)len))
    {
      log_error(LOG_TAG, "%s: Recieved more Data than Expected!, expected: %u, got %u", __FUNCTION__, len, retVal);
      memcpy(data, recvdata, len);
    }
    else
    {
      memcpy(data, recvdata, retVal);
    }
  }
  free(nlh);
  return retVal;
}

/*=============================================================================================
 * Function description:
 *   Initializes Rome RTT Module
 *
 * Parameters:
 *    cfrCaptureModeState: indicates whether LOWI is running in CFR capture mode
 * Return value:
 *    error code: 0 = Success, -1 = Failure
 *
 =============================================================================================*/
int RomeWipsOpen()
{
  romeWipsReady = FALSE;
  nl_sock_fd = 0;
  memset(rxBuff, 0, MAX_NLMSG_LEN);
  nl_sock_fd = create_nl_sock();
  log_verbose(LOG_TAG, "%s: Created Netlink Socket...: %d\n", __FUNCTION__, nl_sock_fd);
  if (nl_sock_fd < 0)
  {
    return -1;
  }
  memset(pipe_ranging_fd, 0, sizeof(int));
  romeWipsReady = TRUE;
  failedTargets.flush();
  return 0;
}

/*=============================================================================================
 * Function description:
 *   Closes the Rome RTT Module
 *
 * Parameters:
 *   NONE
 * Return value:
 *    error code: Succes = 0, -1 = Failure
 *
 =============================================================================================*/
int RomeWipsClose()
{
  if (nl_sock_fd > 0)
  {
    close(nl_sock_fd);
    nl_sock_fd = 0;
  }
  return 0;
}

/** Message Senders - These functions send construct and send messages to Rome CLD/FW */

/*=============================================================================================
 * Function description:
 * Send Registration request to Rome CLD
 *
 * Parameters:
 *    None
 *
 * Return value:
 *    SUCCESS/FAILURE
 =============================================================================================*/
int RomeSendRegReq()
{
  if (!romeWipsReady)
  {
    /** Rome Wips Not Ready */
    log_verbose(LOG_TAG, "%s: ROME Driver Not Ready", __FUNCTION__);
    return -1;
  }

  const char appRegSignature[] = APP_REG_SIGNATURE;
  /* Alocate Space for ANI Message (Header + Body) */
  char *aniMessage = (char *)calloc((sizeof(tAniMsgHdr) + APP_REG_SIGNATURE_LENGTH), 1);
  if (!aniMessage) /* Memory Allocation Failed */
  {
    log_error(LOG_TAG, "%s: Failed to Allocate %u bytes of Memory...\n", __FUNCTION__, (sizeof(tAniMsgHdr) + APP_REG_SIGNATURE_LENGTH));
    return -1;
  }

  /* Fill the ANI Header */
  tAniMsgHdr *aniMsgHeader = (tAniMsgHdr *)aniMessage;
  aniMsgHeader->type = ANI_MSG_APP_REG_REQ;
  aniMsgHeader->length = APP_REG_SIGNATURE_LENGTH;

  /* Fill in the ANI Message Body */
  char *aniMsgBody = (char *)(aniMessage + sizeof(tAniMsgHdr));
  memcpy(aniMsgBody, appRegSignature, APP_REG_SIGNATURE_LENGTH);

  log_verbose(LOG_TAG, "%s: Sending Message over NL...\n", __FUNCTION__);
  /* ANI Message over Netlink Socket */
  if (send_nl_msg(nl_sock_fd, aniMessage, (sizeof(tAniMsgHdr) + APP_REG_SIGNATURE_LENGTH)) < 0)
  {
    log_error(LOG_TAG, "%s: NL Send Failed", __FUNCTION__);
    free(aniMessage);
    return -1;
  }

  free(aniMessage);

  return 0;
}

/*=============================================================================================
 * Function description:
 * Send Channel Info request to Rome CLD
 *
 * Parameters:
 *    iwOemDataCap: The WLAN capabilites information
 *
 * Return value:
 *    SUCCESS/FAILURE
 =============================================================================================*/
int RomeSendChannelInfoReq(IwOemDataCap iwOemDataCap)
{
  unsigned int i;
  unsigned int aniMsgLen;
  char *aniChIds;
  int *channel_list = NULL;

  log_info(LOG_TAG, "RomeSendChannelInfoReq()");
  if (iwOemDataCap.num_channels > WNI_CFG_VALID_CHANNEL_LIST_LEN)
  {
    aniMsgLen = WNI_CFG_VALID_CHANNEL_LIST_LEN;
  }
  else if (0 == iwOemDataCap.num_channels)
  {
    unsigned char channel_list_len;
    channel_list = LOWIUtils::getChannelsOrFreqs(LOWIDiscoveryScanRequest::BAND_ALL,
                                                 channel_list_len, false);
    aniMsgLen = (unsigned int)channel_list_len;
  }
  else
  {
    aniMsgLen = iwOemDataCap.num_channels;
  }

  /* Allocate Space for ANI Message (Header + Body) */
  char *aniMessage = (char *)calloc((sizeof(tAniMsgHdr) + aniMsgLen), 1);
  if (!aniMessage) /* Memory Allocation Failed */
  {
    return -1;
  }

  /* Fill the ANI Header */
  tAniMsgHdr *aniMsgHeader = (tAniMsgHdr *)aniMessage;
  aniMsgHeader->type = ANI_MSG_CHANNEL_INFO_REQ;
  aniMsgHeader->length = aniMsgLen;

  /* Fill in the ANI Message Body */
  aniChIds = (char *)(aniMessage + sizeof(tAniMsgHdr));
  if (NULL == channel_list)
  {
    for (i = 0; i < aniMsgLen; i++)
    {
      aniChIds[i] = ((unsigned char)iwOemDataCap.channel_list[i]);
    }
  }
  else
  {
    for (i = 0; i < aniMsgLen; i++)
    {
      aniChIds[i] = (char)(channel_list[i]);
    }
  }

  /* Send ANI Message over Netlink Socket */
  if (send_nl_msg(nl_sock_fd, aniMessage, (sizeof(tAniMsgHdr) + aniMsgLen)) < 0)
  {
    log_error(LOG_TAG, "%s: NL Send Failed", __FUNCTION__);
    free(aniMessage);
    return -1;
  }

  log_info(LOG_TAG, "%s: Sent Channel Info Request to CLD", __FUNCTION__);
  free(aniMessage);
  if (NULL != channel_list)
  {
    delete[] channel_list;
  }
  return 0;
}

/*=============================================================================================
 * Function description:
 * Send Ranging Capabilities request to Rome FW
 *
 * Parameters:
 *    None
 *
 * Return value:
 *    SUCCESS/FAILURE
 =============================================================================================*/
int RomeSendRangingCapReq()
{
  /* Calculate Message Length */
  unsigned int aniMsgLen = sizeof(OemMsgSubType) +
                           sizeof(RomeRttReqHeaderIE);

  log_info(LOG_TAG, "RomeSendRangingCapReq()");
  /** Request RTT Capability information from Rome FW */
  /* Allocate Space for ANI Message (Header + Body) */
  char *aniMessage = (char *)calloc((sizeof(tAniMsgHdr) + aniMsgLen), 1);
  if (aniMessage == NULL)
  {
    /* Failed to Allocate Memory */
    log_error(LOG_TAG, "%s: Failed to allocate memory for ANI message", __FUNCTION__);
    return -1;
  }
  /* Fill the ANI Header */
  tAniMsgHdr *aniMsgHeader = (tAniMsgHdr *)aniMessage;
  aniMsgHeader->type = ANI_MSG_OEM_DATA_REQ;
  aniMsgHeader->length = aniMsgLen;

  /* Fill in the ANI Message Body */
  char *aniMsgBody = (char *)(aniMessage + sizeof(tAniMsgHdr));
  memset(aniMsgBody, 0, aniMsgLen);
  OemMsgSubType *oemMsgSubType        = (OemMsgSubType *)aniMsgBody;
  RomeRttReqHeaderIE *romeRttReqHeaderIE   = (RomeRttReqHeaderIE *)(aniMsgBody + sizeof(OemMsgSubType));

  /* Load the OEM Message Subtype */
  *oemMsgSubType = TARGET_OEM_CAPABILITY_REQ;
  /* Load in the RTT Header IE */
  WMI_RTT_REQ_ID_SET((romeRttReqHeaderIE->requestID), 0);
  log_verbose(LOG_TAG, "%s: subtype: 0x%x , requestID: 0x%x\n", __FUNCTION__, *oemMsgSubType, romeRttReqHeaderIE->requestID);

  /* Send ANI Message over Netlink Socket */
  if (send_nl_msg(nl_sock_fd, aniMessage, (sizeof(tAniMsgHdr) + aniMsgLen)) < 0)
  {
    log_error(LOG_TAG, "%s: NL Send Failed", __FUNCTION__);
    free(aniMessage);
    return -1;
  }

  free(aniMessage);
  return 0;
}

void setDefaultFtmParams(tANI_U32 *ftmParams)
{
#define BURST_DURATION_32_MS 9
  FTM_SET_ASAP(*ftmParams);
  FTM_CLEAR_LCI_REQ(*ftmParams);
  FTM_CLEAR_LOC_CIVIC_REQ(*ftmParams);
  FTM_SET_BURSTS_EXP(*ftmParams, 0);
  FTM_SET_BURST_DUR(*ftmParams, BURST_DURATION_32_MS);
  FTM_SET_BURST_PERIOD(*ftmParams, 0);
}

void printFTMParams(tANI_U8 *bssid, tANI_U32 ftmParams, tANI_U32 tsfDelta)
{
  log_debug(LOG_TAG, "%s: BSSID: " QUIPC_MACADDR_FMT " ftmParams: 0x%x ASAP: 0x%x, LCI Req: 0x%x, Civic: 0x%x, TSF Valid: 0x%x, Bursts Exp: %u, Burst Duration: %u, Burst Period: %u, tsfDelta: %u",
            __FUNCTION__,
            QUIPC_MACADDR(bssid),
            ftmParams,
            FTM_GET_ASAP(ftmParams),
            FTM_GET_LCI_REQ(ftmParams),
            FTM_GET_LOC_CIVIC_REQ(ftmParams),
            FTM_GET_TSF_VALID_BIT(ftmParams),
            FTM_GET_BURSTS_EXP(ftmParams),
            FTM_GET_BURST_DUR(ftmParams),
            FTM_GET_BURST_PERIOD(ftmParams),
            tsfDelta);
}

/*=============================================================================================
 * Function description:
 *   Sends RTT request to Rome Converged Linux driver
 *
 * Parameters:
 *   chNum       : Channel Id of Target devices
 *   numBSSIDs   : unsigned int Number of BSSIDs in this request
 *   BSSIDs      : DestInfo Array of BSSIDs and RTT Type
 *   spoofBSSIDs : DestInfo Array of Spoof BSSIDs and RTT Type
 *   reportType  : unsigned int Type of Report from FW (Type: 0/1/2)
 *
 * Return value:
 *    error code : 0 for success and -1 for failure.
 *
 =============================================================================================*/
int RomeSendRttReq(uint16 reqId,
                   ChannelInfo  chanInfo,
                   unsigned int numBSSIDs,
                   DestInfo bssidsToScan[MAX_BSSIDS_TO_SCAN],
                   DestInfo spoofBssids[MAX_BSSIDS_TO_SCAN],
                   unsigned int reportType)
{

  /* flush the list of failed Targets */
  failedTargets.flush();
  wmi_channel channelInfo = chanInfo.wmiChannelInfo;
  /* Initialize the local VdevID to STA Vdev ID */
  tANI_U8 locVDevId = vDevId;
  unsigned int i = 0;
  unsigned int aniMsgLen;
  tANI_U32 phyMode;
  aniMsgLen = sizeof(OemMsgSubType) +
              sizeof(RomeRttReqHeaderIE) +
              (numBSSIDs * sizeof(RomeRTTReqCommandIE));
  tANI_U32 flag = 0;

  /** Retrieve Channel information from Channel Info Array for the specific channel ID */
  log_verbose(LOG_TAG, "%s: Going to retrieve channel info for ch#: %u \n",
              __FUNCTION__, LOWIUtils::freqToChannel(channelInfo.mhz));

  /* Check to see if the Target is a P2P Peer.
   * IF target is a P2P Peer, load the channel info from p2p event storage table
   */
  p2pBssidDetected((tANI_U32)numBSSIDs, bssidsToScan, &channelInfo, &locVDevId);

  log_verbose(LOG_TAG, "%s: Channel info for chNum(%u): mhz#= %u band_center_freq1= %u band_center_freq2= %u, info= 0x%x, reg_info_1= 0x%x, reg_info_2= 0x%x\n",
              __FUNCTION__,
              LOWIUtils::freqToChannel(channelInfo.mhz),
              channelInfo.mhz,
              channelInfo.band_center_freq1,
              channelInfo.band_center_freq2,
              channelInfo.info,
              channelInfo.reg_info_1,
              channelInfo.reg_info_2);

  /* Allocate Space for ANI Message (Header + Body) */
  char *aniMessage = (char *)calloc((sizeof(tAniMsgHdr) + aniMsgLen), 1);
  if (aniMessage == NULL)
  {
    /* Failed to Allocate Memory */
    return -1;
  }
  /* Fill the ANI Header */
  tAniMsgHdr *aniMsgHeader = (tAniMsgHdr *)aniMessage;
  aniMsgHeader->type = ANI_MSG_OEM_DATA_REQ;
  aniMsgHeader->length = aniMsgLen;

  /* Fill in the ANI Message Body */
  char *aniMsgBody = (char *)(aniMessage + sizeof(tAniMsgHdr));
  memset(aniMsgBody, 0, aniMsgLen);
  OemMsgSubType *oemMsgSubType        = (OemMsgSubType *)aniMsgBody;
  RomeRttReqHeaderIE *romeRttReqHeaderIE   = (RomeRttReqHeaderIE *)(aniMsgBody + sizeof(OemMsgSubType));

  void *rttCommandIE                              = (aniMsgBody + sizeof(OemMsgSubType) + sizeof(RomeRttReqHeaderIE));
  RomeRTTReqCommandIE *romeRTTReqCommandIEs       = (RomeRTTReqCommandIE *)(rttCommandIE);

  /* Load the OEM Message Subtype */
  *oemMsgSubType = TARGET_OEM_MEASUREMENT_REQ;
  /* Load in the RTT Header IE */
  WMI_RTT_REQ_ID_SET((romeRttReqHeaderIE->requestID), reqId);
  WMI_RTT_NUM_STA_SET((romeRttReqHeaderIE->numSTA), numBSSIDs);
  log_verbose(LOG_TAG, "%s: subtype: 0x%x, requestID: 0x%x, numSTA: 0x%x\n",
              __FUNCTION__,
              *oemMsgSubType,
              romeRttReqHeaderIE->requestID,
              romeRttReqHeaderIE->numSTA);

  if (channelInfo.band_center_freq1 == 0)
  {
    log_verbose(LOG_TAG, "%s: Setting band_center_freq1 = primary Frequency\n",
                __FUNCTION__);
    channelInfo.band_center_freq1 = channelInfo.mhz;
  }
  memcpy(&(romeRttReqHeaderIE->channelInfo), &channelInfo, sizeof(channelInfo));

  /* Load in the RTT Command IEs for all the APs */
  for (i = 0; i < numBSSIDs; i++)
  {
    phyMode  = channelInfo.info & ~(PHY_MODE_MASK);

    /* set the RTT type, tx/rx chain, QCA peer and BW */
    WMI_RTT_FRAME_TYPE_SET(romeRTTReqCommandIEs->controlFlag, bssidsToScan[i].rttFrameType);
    WMI_RTT_TX_CHAIN_SET(romeRTTReqCommandIEs->controlFlag, TX_CHAIN_1);
    WMI_RTT_RX_CHAIN_SET(romeRTTReqCommandIEs->controlFlag, RX_CHAIN_1);
    WMI_RTT_QCA_PEER_SET(romeRTTReqCommandIEs->controlFlag, NON_QCA_PEER);
    WMI_RTT_BW_SET(romeRTTReqCommandIEs->controlFlag, bssidsToScan[i].bandwidth);
    log_debug(LOG_TAG, "%s: TX_BW: %s, RTT_PKT_TYPE: %s\n",
              __FUNCTION__,
              LOWI_TO_STRING(bssidsToScan[i].bandwidth, TX_RX_BW),
              LOWI_TO_STRING(bssidsToScan[i].rttFrameType, RTT_PKT_TYPE));

    /* Pick Preamble */
    switch (bssidsToScan[i].preamble)
    {
    case RTT_PREAMBLE_LEGACY:
      {
        WMI_RTT_PREAMBLE_SET(romeRTTReqCommandIEs->controlFlag, ROME_PREAMBLE_LEGACY);
        break;
      }
    case RTT_PREAMBLE_HT:
      {
        WMI_RTT_PREAMBLE_SET(romeRTTReqCommandIEs->controlFlag, ROME_PREAMBLE_HT);
        break;
      }
    case RTT_PREAMBLE_VHT:
      {
        WMI_RTT_PREAMBLE_SET(romeRTTReqCommandIEs->controlFlag, ROME_PREAMBLE_VHT);
        break;
      }
    default:
      {
        WMI_RTT_PREAMBLE_SET(romeRTTReqCommandIEs->controlFlag, ROME_PREAMBLE_LEGACY);
        break;
      }
    }

    flag = WMI_RTT_PREAMBLE_GET(romeRTTReqCommandIEs->controlFlag);

    /* Pick the data rate */
    if (flag == ROME_PREAMBLE_LEGACY)
    {
      /* for Legacy Frame types, we will always use a data rate of 6MBps.
         This is indicated to FW by setting the MCS field to 3 */
      WMI_RTT_MCS_SET(romeRTTReqCommandIEs->controlFlag, 3);
    }
    else
    {
      /* for HT and VHT Frame types, we will always use adata rate of 6.5MBps.
         This is indicated to FW by setting the MCS field to 0 */
      WMI_RTT_MCS_SET(romeRTTReqCommandIEs->controlFlag, 0x00);
    }

    /** Set the number of HW retries for RTT frames:
     *  For RTT2 it is the QosNull Frame retries
     *  For RTT3 it is the FTMR Frame retries. */
    WMI_RTT_RETRIES_SET(romeRTTReqCommandIEs->controlFlag, bssidsToScan[i].numFrameRetries);

    /* Load the measurementInfo */
    WMI_RTT_VDEV_ID_SET(romeRTTReqCommandIEs->measurementInfo, locVDevId);
    log_debug(LOG_TAG, "%s: PHY Mode : %s, PREAMBLE: %s, controlFlag[%u]: 0x%x, vDevID: %u\n",
              __FUNCTION__,
              LOWI_TO_STRING(phyMode, PHY_MODE),
              LOWI_TO_STRING(flag, PREAMBLE_DBG),
              i,
              romeRTTReqCommandIEs->controlFlag,
              locVDevId);

    WMI_RTT_MEAS_NUM_SET(romeRTTReqCommandIEs->measurementInfo, bssidsToScan[i].numFrames);
    WMI_RTT_TIMEOUT_SET(romeRTTReqCommandIEs->measurementInfo, RTT_TIMEOUT_PER_TARGET);
    WMI_RTT_REPORT_TYPE_SET(romeRTTReqCommandIEs->measurementInfo, reportType);
    log_verbose(LOG_TAG, "%s: measurementInfo[%u]: 0x%x, numFrames[%u]: %u\n",
                __FUNCTION__,
                i,
                romeRTTReqCommandIEs->measurementInfo,
                i,
                WMI_RTT_MEAS_NUM_GET(romeRTTReqCommandIEs->measurementInfo));

    memcpy(romeRTTReqCommandIEs->destMac, &bssidsToScan[i].mac[0], ETH_ALEN);
    memcpy(romeRTTReqCommandIEs->spoofBSSID, &spoofBssids[i].mac[0], ETH_ALEN);

    romeRTTReqCommandIEs->ftmParams = bssidsToScan[i].ftmParams;

    if (bssidsToScan[i].tsfValid)
    {
      FTM_SET_TSF_VALID(romeRTTReqCommandIEs->ftmParams);
      romeRTTReqCommandIEs->tsfDelta = bssidsToScan[i].tsfDelta;
      log_verbose(LOG_TAG, "%s: Valid TSF\n", __FUNCTION__);
    }
    else
    {
      FTM_CLEAR_TSF_VALID(romeRTTReqCommandIEs->ftmParams);
      romeRTTReqCommandIEs->tsfDelta = 0;
      log_verbose(LOG_TAG, "%s: InValid TSF\n", __FUNCTION__);
    }

    printFTMParams(romeRTTReqCommandIEs->destMac,
                   romeRTTReqCommandIEs->ftmParams,
                   romeRTTReqCommandIEs->tsfDelta);

    romeRTTReqCommandIEs++;
  }

  log_verbose(LOG_TAG, "%s: sending Ranging Req message over NL at TS: %" PRId64 " ms\n", __FUNCTION__,
              LOWIUtils::currentTimeMs());

  /* ANI Message over Netlink Socket */
  if (send_nl_msg(nl_sock_fd, aniMessage, (sizeof(tAniMsgHdr) + aniMsgLen)) < 0)
  {
    log_error(LOG_TAG, "%s: NL Send Failed", __FUNCTION__);
    free(aniMessage);
    return -1;
  }

  free(aniMessage);
  return 0;
}

int qc_loc_fw::RomeSendLCIConfiguration(tANI_U16 reqId, LOWISetLCILocationInformation *request)
{
  int retVal = 0;

  log_verbose(LOG_TAG, "%s: \n", __FUNCTION__);

  if (request == NULL)
  {
    return -1;
  }

  LOWILciInformation lciInfo = request->getLciParams();
  tANI_U32        usageRules = request->getUsageRules();


  log_verbose(LOG_TAG, "%s - LCIParam - latitude: %" PRId64 " latitude_unc: %d \n",
              __FUNCTION__,
              lciInfo.latitude,
              lciInfo.latitude_unc);
  log_verbose(LOG_TAG, "%s - LCIParam - longitude: %" PRId64 " longitude_unc: %d\n",
              __FUNCTION__,
              lciInfo.longitude,
              lciInfo.longitude_unc);
  log_verbose(LOG_TAG, "%s - LCIParam - altitude: %d altitude_unc: %d",
              __FUNCTION__,
              lciInfo.altitude,
              lciInfo.altitude_unc);
  log_verbose(LOG_TAG, "%s - LCIParam - motion_pattern: %d, floor: %d & usageRules: %u",
              __FUNCTION__,
              lciInfo.motion_pattern,
              lciInfo.floor,
              usageRules);
  log_verbose(LOG_TAG, "%s - LCIParam - height_above_floor: %d height_unc: %d",
              __FUNCTION__,
              lciInfo.height_above_floor,
              lciInfo.height_unc);

  unsigned int aniMsgLen;
  aniMsgLen = sizeof(OemMsgSubType) +
              sizeof(wmi_rtt_lci_cfg_head);

  /* Allocate Space for ANI Message (Header + Body) */
  char *aniMessage = (char *)calloc((sizeof(tAniMsgHdr) + aniMsgLen), 1);
  if (aniMessage == NULL)
  {
    /* Failed to Allocate Memory */
    return -1;
  }

  log_verbose(LOG_TAG, "%s: Fill in LCI Config ANI Message Header\n", __FUNCTION__);
  /* Fill the ANI Header */
  tAniMsgHdr *aniMsgHeader = (tAniMsgHdr *)aniMessage;
  aniMsgHeader->type = ANI_MSG_OEM_DATA_REQ;
  aniMsgHeader->length = aniMsgLen;

  log_verbose(LOG_TAG, "%s: Fill in LCI Config Message Body\n", __FUNCTION__);
  /* Fill in the ANI Message Body */
  char *aniMsgBody = (char *)(aniMessage + sizeof(tAniMsgHdr));
  memset(aniMsgBody, 0, aniMsgLen);
  OemMsgSubType *oemMsgSubType        = (OemMsgSubType *)aniMsgBody;
  wmi_rtt_lci_cfg_head *lciConfig        = (wmi_rtt_lci_cfg_head *)(aniMsgBody + sizeof(OemMsgSubType));

  /* Load the OEM Message Subtype */
  *oemMsgSubType = TARGET_OEM_CONFIGURE_LCI;
  /* Load in the LCI IE */
  WMI_RTT_REQ_ID_SET((lciConfig->req_id), reqId);
  log_verbose(LOG_TAG, "%s: subtype: 0x%x, requestID: 0x%x\n",
              __FUNCTION__,
              *oemMsgSubType,
              lciConfig->req_id);

  lciConfig->latitude  = (tANI_U64)lciInfo.latitude;
  lciConfig->longitude = (tANI_U64)lciInfo.longitude;
  WMI_RTT_LCI_LAT_UNC_SET(lciConfig->lci_cfg_param_info, lciInfo.latitude_unc);
  WMI_RTT_LCI_LON_UNC_SET(lciConfig->lci_cfg_param_info, lciInfo.longitude_unc);
  WMI_RTT_LCI_Z_MOTION_PAT_SET(lciConfig->lci_cfg_param_info, lciInfo.motion_pattern);
  lciConfig->altitude  = (tANI_U64)lciInfo.altitude;
  WMI_RTT_LCI_ALT_UNC_SET(lciConfig->altitude_info, lciInfo.altitude_unc);
  lciConfig->floor     = (tANI_U32)lciInfo.floor;
  WMI_RTT_LCI_Z_HEIGHT_ABV_FLR_SET(lciConfig->floor_param_info, (tANI_U32)lciInfo.height_above_floor);
  WMI_RTT_LCI_Z_HEIGHT_UNC_SET(lciConfig->floor_param_info, (tANI_U32)lciInfo.height_unc);
  lciConfig->usage_rules = usageRules;

  log_verbose(LOG_TAG, "%s: Sending LCI Configuration Req message over NL at TS: %" PRId64 " ms\n", __FUNCTION__,
              LOWIUtils::currentTimeMs());

  /* ANI Message over Netlink Socket */
  if (send_nl_msg(nl_sock_fd, aniMessage, (sizeof(tAniMsgHdr) + aniMsgLen)) < 0)
  {
    log_error(LOG_TAG, "%s: NL Send Failed", __FUNCTION__);
    free(aniMessage);
    return -1;
  }

  free(aniMessage);

  return retVal;
}
int qc_loc_fw::RomeSendLCRConfiguration(tANI_U16 reqId, LOWISetLCRLocationInformation *request)
{
  int retVal = 0;

  log_verbose(LOG_TAG, "%s: \n", __FUNCTION__);

  if (request == NULL)
  {
    return -1;
  }

  LOWILcrInformation lcrInfo = request->getLcrParams();

  unsigned int aniMsgLen;
  aniMsgLen = sizeof(OemMsgSubType) +
              sizeof(wmi_rtt_lcr_cfg_head);

  /* Allocate Space for ANI Message (Header + Body) */
  char *aniMessage = (char *)calloc((sizeof(tAniMsgHdr) + aniMsgLen), 1);
  if (aniMessage == NULL)
  {
    /* Failed to Allocate Memory */
    return -1;
  }

  log_verbose(LOG_TAG, "%s: Fill in LCR Config ANI Message Header\n", __FUNCTION__);
  /* Fill the ANI Header */
  tAniMsgHdr *aniMsgHeader = (tAniMsgHdr *)aniMessage;
  aniMsgHeader->type = ANI_MSG_OEM_DATA_REQ;
  aniMsgHeader->length = aniMsgLen;

  log_verbose(LOG_TAG, "%s: Fill in LCR Config Message Body\n", __FUNCTION__);
  /* Fill in the ANI Message Body */
  char *aniMsgBody = (char *)(aniMessage + sizeof(tAniMsgHdr));
  memset(aniMsgBody, 0, aniMsgLen);
  OemMsgSubType *oemMsgSubType = (OemMsgSubType *)aniMsgBody;
  wmi_rtt_lcr_cfg_head *lcrConfig = (wmi_rtt_lcr_cfg_head *)(aniMsgBody + sizeof(OemMsgSubType));

  /* Load the OEM Message Subtype */
  *oemMsgSubType = TARGET_OEM_CONFIGURE_LCR;
  /* Load in the LCI IE */
  WMI_RTT_REQ_ID_SET((lcrConfig->req_id), reqId);

  /* The following subtraction and addition of the value 2 to length is being done because
   * Country code which is 2 bytes comes separately from the actual Civic Info String
   */
  tANI_U8 len = (lcrInfo.length > (MAX_CIVIC_INFO_LEN - 2)) ? (MAX_CIVIC_INFO_LEN - 2) : lcrInfo.length;

  WMI_RTT_LOC_CIVIC_LENGTH_SET(lcrConfig->loc_civic_params, (len + 2));

  tANI_U8 *civicInfo = (tANI_U8 *)lcrConfig->civic_info;

  log_verbose(LOG_TAG, "%s - subtype: 0x%x, requestID: 0x%x, LCRParam: country[0]: %u & country[1]: %u",
              __FUNCTION__,
              *oemMsgSubType,
              lcrConfig->req_id,
              lcrInfo.country_code[0],
              lcrInfo.country_code[1]);

  civicInfo[0] = lcrInfo.country_code[0];
  civicInfo[1] = lcrInfo.country_code[1];
  memcpy(&civicInfo[2], lcrInfo.civic_info, (tANI_U8)lcrInfo.length);


  log_verbose(LOG_TAG, "%s: Sending LCR Configuration Req message over NL at TS: %" PRId64 " ms\n", __FUNCTION__,
              LOWIUtils::currentTimeMs());

  /* ANI Message over Netlink Socket */
  if (send_nl_msg(nl_sock_fd, aniMessage, (sizeof(tAniMsgHdr) + aniMsgLen)) < 0)
  {
    log_error(LOG_TAG, "%s: NL Send Failed", __FUNCTION__);
    free(aniMessage);
    return -1;
  }

  free(aniMessage);

  return retVal;
}

int qc_loc_fw::RomeSendLCIRequest(tANI_U16 reqId, LOWISendLCIRequest *request)
{
  int retVal = 0;

  log_verbose(LOG_TAG, "%s: \n", __FUNCTION__);

  if (request == NULL)
  {
    return -1;
  }

  unsigned int aniMsgLen;
  aniMsgLen = sizeof(OemMsgSubType) +
              sizeof(meas_req_lci_request);

  /* Allocate Space for ANI Message (Header + Body) */
  char *aniMessage = (char *)calloc((sizeof(tAniMsgHdr) + aniMsgLen), 1);
  if (aniMessage == NULL)
  {
    /* Failed to Allocate Memory */
    return -1;
  }

  log_verbose(LOG_TAG, "%s: Fill in Where are you request ANI Message Header\n", __FUNCTION__);
  /* Fill the ANI Header */
  tAniMsgHdr *aniMsgHeader = (tAniMsgHdr *)aniMessage;
  aniMsgHeader->type = ANI_MSG_OEM_DATA_REQ;
  aniMsgHeader->length = aniMsgLen;

  log_verbose(LOG_TAG, "%s: Fill in Where are you Message Body\n", __FUNCTION__);
  /* Fill in the ANI Message Body */
  char *aniMsgBody = (char *)(aniMessage + sizeof(tAniMsgHdr));
  memset(aniMsgBody, 0, aniMsgLen);

  /* Load the OEM Message Subtype */
  OemMsgSubType *oemMsgSubType = (OemMsgSubType *)aniMsgBody;
  *oemMsgSubType = TARGET_OEM_LCI_REQ;

  meas_req_lci_request *wru = (meas_req_lci_request *)(aniMsgBody + sizeof(OemMsgSubType));
  memset(wru, 0, sizeof(meas_req_lci_request));
  wru->req_id = request->getRequestId();
  LOWIMacAddress sta_mac(request->getBssid());
  for (int i = 0; i < BSSID_SIZE; i++)
  {
    wru->sta_mac[i] = sta_mac[i];
  }
  wru->dialogtoken = gDialogTok++;
  if (gDialogTok == 0)
  {
    /* Dialog Token shall always be a non zero number so increment again*/
    gDialogTok++;
  }
  wru->element_id     = RM_MEAS_REQ_ELEM_ID;
  wru->length         = sizeof(wru->meas_token) + sizeof(wru->meas_req_mode) +
                        sizeof(wru->meas_type) + sizeof(wru->loc_subject);
  wru->meas_token = gMeasTok++;
  if (gMeasTok == 0)
  {
    /* Mesurement Token shall always be a non zero number so increment again*/
    gMeasTok++;
  }
  wru->meas_type      = LOWI_WLAN_LCI_REQ_TYPE;
  wru->loc_subject    = LOWI_LOC_SUBJECT_REMOTE;

  log_verbose(LOG_TAG, "%s: Sending Where are you Req message over NL at TS: %" PRId64 " ms\n", __FUNCTION__,
              LOWIUtils::currentTimeMs());

  /* ANI Message over Netlink Socket */
  if (send_nl_msg(nl_sock_fd, aniMessage, (sizeof(tAniMsgHdr) + aniMsgLen)) < 0)
  {
    log_error(LOG_TAG, "%s: NL Send Failed", __FUNCTION__);
    free(aniMessage);
    return -1;
  }

  free(aniMessage);

  return retVal;
}

int qc_loc_fw::RomeSendFTMRR(tANI_U16 reqId, LOWIFTMRangingRequest *request)
{
  int retVal = 0;

  log_verbose(LOG_TAG, "%s: \n", __FUNCTION__);

  if (request == NULL)
  {
    return -1;
  }

  unsigned int aniMsgLen;
  aniMsgLen = sizeof(OemMsgSubType) +
              sizeof(neighbor_report_arr);

  /* Allocate Space for ANI Message (Header + Body) */
  char *aniMessage = (char *)calloc((sizeof(tAniMsgHdr) + aniMsgLen), 1);
  if (aniMessage == NULL)
  {
    /* Failed to Allocate Memory */
    return -1;
  }

  log_verbose(LOG_TAG, "%s: Fill in FTMRR ANI Message Header\n", __FUNCTION__);
  /* Fill the ANI Header */
  tAniMsgHdr *aniMsgHeader = (tAniMsgHdr *)aniMessage;
  aniMsgHeader->type = ANI_MSG_OEM_DATA_REQ;
  aniMsgHeader->length = aniMsgLen;

  log_verbose(LOG_TAG, "%s: Fill in FTMRR Message Body\n", __FUNCTION__);
  /* Fill in the ANI Message Body */
  char *aniMsgBody = (char *)(aniMessage + sizeof(tAniMsgHdr));
  memset(aniMsgBody, 0, aniMsgLen);

  /* Load the OEM Message Subtype */
  OemMsgSubType *oemMsgSubType = (OemMsgSubType *)aniMsgBody;
  *oemMsgSubType = TARGET_OEM_FTMR_REQ;

  neighbor_report_arr *ftmrr = (neighbor_report_arr *)(aniMsgBody + sizeof(OemMsgSubType));
  memset(ftmrr, 0, sizeof(neighbor_report_arr));
  ftmrr->req_id = request->getRequestId();
  LOWIMacAddress sta_mac(request->getBSSID());
  for (int i = 0; i < BSSID_SIZE; i++)
  {
    ftmrr->sta_mac[i] = sta_mac[i];
  }
  ftmrr->dialogtoken = gDialogTok++;
  if (gDialogTok == 0)
  {
    /* Dialog Token shall always be a non zero number so increment again*/
    gDialogTok++;
  }
  ftmrr->element_id     = RM_MEAS_REQ_ELEM_ID;
  ftmrr->len         = sizeof(ftmrr->meas_token) + sizeof(ftmrr->meas_req_mode) +
                       sizeof(ftmrr->meas_type) + sizeof(ftmrr->rand_inter) +
                       sizeof(ftmrr->min_ap_count) + sizeof(ftmrr->elem);
  ftmrr->meas_token = gMeasTok++;
  if (gMeasTok == 0)
  {
    /* Mesurement Token shall always be a non zero number so increment again*/
    gMeasTok++;
  }
  ftmrr->meas_type      = LOWI_WLAN_FTM_RANGE_REQ_TYPE;
  ftmrr->rand_inter     = request->getRandInter();

  vector<LOWIFTMRRNodeInfo> nodes = request->getNodes();
  ftmrr->min_ap_count   = nodes.getNumOfElements();
  for (int ii = 0; ii < nodes.getNumOfElements(); ii++)
  {
    ftmrr->elem[ii].sub_element_id = RM_NEIGHBOR_RPT_ELEM_ID;
    ftmrr->elem[ii].sub_element_len = sizeof(neighbor_report_element_arr) -
                                      sizeof(ftmrr->elem[ii].sub_element_id) -
                                      sizeof(ftmrr->elem[ii].sub_element_len);
    for (int bssid_idx = 0; bssid_idx < BSSID_SIZE; bssid_idx++)
    {
      ftmrr->elem[ii].bssid[bssid_idx] = nodes[ii].bssid[bssid_idx];
    }
    ftmrr->elem[ii].bssid_info = nodes[ii].bssidInfo;
    ftmrr->elem[ii].operating_class = nodes[ii].operatingClass;
    ftmrr->elem[ii].channel_num = nodes[ii].ch;
    ftmrr->elem[ii].phy_type = nodes[ii].phyType;
    ftmrr->elem[ii].wbc_element_id = RM_WIDE_BW_CHANNEL_ELEM_ID;
    ftmrr->elem[ii].wbc_len = sizeof(ftmrr->elem[ii].wbc_ch_width) +
                              sizeof(ftmrr->elem[ii].wbc_center_ch0) +
                              sizeof(ftmrr->elem[ii].wbc_center_ch0);
    ftmrr->elem[ii].wbc_ch_width = (uint8)nodes[ii].bandwidth;
    ftmrr->elem[ii].wbc_center_ch0 = nodes[ii].center_Ch1;
    ftmrr->elem[ii].wbc_center_ch1 = nodes[ii].center_Ch2;

    unsigned char *charPtr = (unsigned char *)&(ftmrr->elem[ii]);
    for (int idx = 0; idx < sizeof(neighbor_report_element_arr); idx++)
    {
      log_verbose(LOG_TAG, "%s - nbr(%d), %02x ", __FUNCTION__, ii, charPtr[idx]);
    }
  }
  log_verbose(LOG_TAG, "%s: Sending FTMRR message over NL at TS: %" PRId64 " ms\n", __FUNCTION__,
              LOWIUtils::currentTimeMs());

  /* ANI Message over Netlink Socket */
  if (send_nl_msg(nl_sock_fd, aniMessage, (sizeof(tAniMsgHdr) + aniMsgLen)) < 0)
  {
    log_error(LOG_TAG, "%s: NL Send Failed", __FUNCTION__);
    free(aniMessage);
    return -1;
  }

  free(aniMessage);

  return retVal;
}

/***** END - Message Senders *******/

/***** Event related Functions *******/
/*=============================================================================================
 * Function description:
 *   Called by external entity to terminate the blocking of the Ranging thread on select call
 *   by means of writing to a pipe. Thread is blocked on socket and a pipe in select call
 *
 * Parameters:
 *   None
 *
 * Return value:
 *    num of bytes written, -1 for error, 0 for no bytes written
 =============================================================================================*/
int RomeUnblockRangingThread(void)
{
  int retVal = -1;
  char string [] = "Close";
  log_debug(LOG_TAG, "RomeUnblockRangingThread");
  if (0 != pipe_ranging_fd [1])
  {
    retVal = write(pipe_ranging_fd[1], string, (strlen(string)+1));
  }
  log_debug(LOG_TAG, "RomeUnblockRangingThread - completed");
  return retVal;
}

/*=============================================================================================
 * Function description:
 *   Called by external entity to create the pipe to Ranging Thread
 *
 * Parameters:
 *   None
 *
 * Return value:
 *    0 Success, other values otherwise
 =============================================================================================*/
int RomeInitRangingPipe(void)
{
  if (pipe_ranging_fd[0] == 0 &&
      pipe_ranging_fd[1] == 0)
  {
    log_debug(LOG_TAG,  "Creating pipe to Ranging Thread\n");
    return pipe(pipe_ranging_fd);
  }
  else
  {
    log_debug(LOG_TAG,  "The pipe to Ranging Thread is already valid\n");
    return -1;
  }
}

/*=============================================================================================
 * Function description:
 *   Called by external entity to close the pipe to Ranging thread
 *
 * Parameters:
 *   None
 *
 * Return value:
 *    0 Success, other values otherwise
 =============================================================================================*/
int RomeCloseRangingPipe(void)
{
  log_debug(LOG_TAG,  "Closing the Ranging pipe\n");
  if (pipe_ranging_fd[0] > 0)
  {
    close (pipe_ranging_fd[0]);
    pipe_ranging_fd [0] = 0;
  }

  if (pipe_ranging_fd[1] > 0)
  {
    close (pipe_ranging_fd[1]);
    pipe_ranging_fd [1] = 0;
  }
  return 0;
}

/*===========================================================================
 * Function description:
 *   Waits on a private Netlink socket for one of the following events
 *       1) Data becomes available on socket
 *       2) Activity on unBlock Pipe
 *       3) A timeout happens.
 *
 * Parameters:
 *   nl_sock_fd is the file descriptor of the Private Netlink Socket
 *   int timeout_val is the timout value specified by caller
 *
 * Return value:
 *   TRUE, if some data is available on socket. FALSE, if timed out or error
 ===========================================================================*/
int RomeWaitOnActivityOnSocketOrPipe(int timeout_val)
{
  struct timeval tv;
  fd_set read_fd_set;
  int max_fd;
  int retval;

  max_fd = MAX(nl_sock_fd, pipe_ranging_fd[0]);
  if (max_fd <= 0)
  {
    log_error(LOG_TAG, "%s: Both socket and pipe not initialized %d, %d",
                   __FUNCTION__, nl_sock_fd, pipe_ranging_fd[0]);
    return ERR_NOT_READY;
  }
  // At least one socket is valid
  tv.tv_sec = timeout_val;
  tv.tv_usec = 0;
  FD_ZERO(&read_fd_set);
  if (nl_sock_fd > 0)
  {
    FD_SET(nl_sock_fd, &read_fd_set);
  }
  else
  {
    log_warning(LOG_TAG, "%s: Bad netlink socket %d", __FUNCTION__,
                  nl_sock_fd);
  }
  if (pipe_ranging_fd[0] > 0)
  {
    FD_SET(pipe_ranging_fd[0], &read_fd_set);
  }
  else
  {
    log_warning(LOG_TAG, "%s: Bad pipe %d", __FUNCTION__, pipe_ranging_fd[0]);
  }

  if (timeout_val >= 0)
  {
    log_debug(LOG_TAG, "%s: issuing a timed select: TO: %i \n", __FUNCTION__, timeout_val);
    retval = select(max_fd+1, &read_fd_set, NULL,NULL,&tv);
    log_debug(LOG_TAG, "%s: Came out of timed select: TO: %i retVal: %d\n",
                 __FUNCTION__, timeout_val, retval);
  }
  else
  {
    log_debug(LOG_TAG, "%s: issuing a blocking select \n", __FUNCTION__);
    retval = select(max_fd+1, &read_fd_set, NULL,NULL,NULL);
  }

  if (retval == 0) //This means the select timed out
  {
    log_debug(LOG_TAG, "%s: No Messages Received!! Timeout \n", __FUNCTION__);
    retval = ERR_SELECT_TIMEOUT;
    return retval;
  }

  if (retval < 0) //This means the select failed with some error
  {
    log_error(LOG_TAG, "%s: No results from the Scan!! Error %s(%d)\n",
                   __FUNCTION__, strerror(errno), errno);
  }

  if ( FD_ISSET( pipe_ranging_fd[0], &read_fd_set ) )
  {
    char readbuffer [50];
    int nbytes = read(pipe_ranging_fd[0], readbuffer, sizeof(readbuffer));

    log_debug(LOG_TAG, "%s: Ranging thread Received string: %s, requesting socket shutdown \n",
                 __FUNCTION__, readbuffer);
    retval = ERR_SELECT_TERMINATED;
  }

  return retval;

}
/***** END - Event related Functions *******/

/******* Message Handlers **************/

/*=============================================================================================
 * Function description:
 * Close Rome RTT Interface Module
 *
 * Parameters:
 *    msgType: The Type of Message received
 *    data   : The message body
 *
 * Return value:
 *    SUCCESS/FAILURE
 =============================================================================================*/
int RomeNLRecvMessage(RomeNlMsgType* msgType, void* data, tANI_U32 maxDataLen)
{
  int retVal = 0;
  OemMsgSubType* oemMsgSubType = NULL;
  tAniMsgHdr* aniMsgHdr = NULL;
  bool validMsgType = TRUE;
  char* localp = (char*)data;

  log_info(LOG_TAG, "RomeNLRecvMessage()");

  if (msgType == NULL || localp == NULL)
  {
    log_error(LOG_TAG, "%s, Received invalid pointer for msgType or data", __FUNCTION__);
    return -1;
  }

  tANI_U32 maxCopyLen = (maxDataLen > MAX_NLMSG_LEN) ? MAX_NLMSG_LEN : maxDataLen;
  memset(rxBuff, 0, MAX_NLMSG_LEN);
  memset(localp, 0, maxDataLen);
  *msgType = ROME_MSG_MAX;

  if(recv_nl_msg(nl_sock_fd, rxBuff, MAX_NLMSG_LEN) > 0)
  {
    aniMsgHdr = (tAniMsgHdr*) rxBuff;

    switch(aniMsgHdr->type)
    {
      case ANI_MSG_APP_REG_RSP:
      {
        *msgType = ROME_REG_RSP_MSG;
        break;
      }
      case ANI_MSG_OEM_DATA_RSP:
      {
        oemMsgSubType = (OemMsgSubType *) (rxBuff + sizeof(tAniMsgHdr));
        log_error(LOG_TAG, "RomeNLRecvMessage:  received message of subtype: %u", *oemMsgSubType);
        switch (*oemMsgSubType)
        {
          case TARGET_OEM_CAPABILITY_RSP:
          {
            *msgType = ROME_RANGING_CAP_MSG;
            break;
          }
          case TARGET_OEM_MEASUREMENT_RSP:
          {
            log_verbose(LOG_TAG, "%s: Received Ranging Response message over NL at TS: %" PRId64 " ms\n",
                          __FUNCTION__, LOWIUtils::currentTimeMs());

            *msgType = ROME_RANGING_MEAS_MSG;
            break;
          }
          case TARGET_OEM_ERROR_REPORT_RSP:
          {
            *msgType = ROME_RANGING_ERROR_MSG;
            break;
          }
          default:
          {
            log_error(LOG_TAG, "%s: Received a OEM Data message with bad subtype: %u",
                           __FUNCTION__, *oemMsgSubType);
            *msgType = ROME_NL_ERROR_MSG;
            validMsgType = FALSE;
            retVal = -1;
            break;
          }
        }
        break;
      }
      case ANI_MSG_CHANNEL_INFO_RSP:
      {
        *msgType = ROME_CHANNEL_INFO_MSG;
        break;
      }
      case ANI_MSG_OEM_ERROR:
      {
        *msgType = ROME_CLD_ERROR_MSG;
        break;
      }
      case ANI_MSG_PEER_STATUS_IND:
      {
        *msgType = ROME_P2P_PEER_EVENT_MSG;
        break;
      }
      default:
      {
        *msgType = ROME_NL_ERROR_MSG;
        validMsgType = FALSE;
        break;
      }
    }
  }
  else
  {
    log_error(LOG_TAG, "%s: NL Recv Failed, with errno(%d): %s", __FUNCTION__, errno, strerror(errno));
    *msgType = ROME_NL_ERROR_MSG;
    retVal = -1;
  }

  if (validMsgType)
  {
    log_info(LOG_TAG, "Loading Message to Data");
    memcpy(localp, rxBuff, maxCopyLen);
  }

  log_info(LOG_TAG, "Loading Message to Data - Done");
  if (aniMsgHdr)
  {
    if (oemMsgSubType)
    {
      log_info(LOG_TAG, "%s: The Received ANI Msg Type: %u, OEM Type: %u, RomeMsgType: %u",
                    __FUNCTION__, aniMsgHdr->type, *oemMsgSubType, *msgType);
    }
    else
    {
      log_info(LOG_TAG, "%s: The Received ANI Msg Type: %u, Type: %u",
                    __FUNCTION__, aniMsgHdr->type, *msgType);
    }
  }
  return retVal;
}

/*=============================================================================================
 * Function description:
 * Extract information from registration message
 *
 * Parameters:
 *    data   : The message body
 *
 * Return value:
 *    SUCCESS/FAILURE
 =============================================================================================*/
int RomeExtractRegRsp(void* data)
{
  char* aniNumVDevs;
  char* datap = (char*) data;
  tANI_U8  numVDevs = 0;
  VDevMap* aniVdevInfo;
  unsigned int i;
  /* Extract Data from ANI Message Body */
  memset(&vDevInfo, 0, sizeof(VDevInfo));
  aniNumVDevs = (char *)(datap + sizeof(tAniMsgHdr));
  aniVdevInfo = (VDevMap*) (aniNumVDevs + 1);
  log_verbose(LOG_TAG, "%s: aniNumVDevs: %u\n", __FUNCTION__, (*aniNumVDevs));
  vDevInfo.numInterface = (*aniNumVDevs);
  for(i = 0;i < (*aniNumVDevs) ; i++)
  {
    vDevInfo.vDevMap[i].iFaceId = (aniVdevInfo + i)->iFaceId;
    vDevInfo.vDevMap[i].vDevId  = (aniVdevInfo + i)->vDevId;
  }

  numVDevs = (*aniNumVDevs);

  if(numVDevs <= 0)
  { /* Failed to register with CLD */
    return -1;
  }
  else if(numVDevs > WLAN_HDD_MAX_DEV_MODE)
  {
    log_error(LOG_TAG, " %s: Received more VDevs than expected!, expected: %u, received: %u\n",
                   __FUNCTION__, WLAN_HDD_MAX_DEV_MODE, numVDevs);
    numVDevs = WLAN_HDD_MAX_DEV_MODE;
  }
  log_verbose(LOG_TAG, "%s: Done Registering with CLD... Received %u vDevs\n", __FUNCTION__, numVDevs);
  /* Pick up just the STA VDev for now */
  for(i = 0; i < numVDevs; i++)
  {
    if(vDevInfo.vDevMap[i].iFaceId == WLAN_HDD_INFRA_STATION)
    {
      vDevId = vDevInfo.vDevMap[i].vDevId;
    }
  }
  log_verbose(LOG_TAG, "%s: using vDev: %d...: \n", __FUNCTION__, vDevId);
  return 0;
}

/*=============================================================================================
 * Function description:
 * Extract information from Channel Info message
 *
 * Parameters:
 *    data   : The message body
 *
 * Return value:
 *    SUCCESS/FAILURE
 =============================================================================================*/
int RomeExtractChannelInfo(void* data, ChannelInfo *pChannelInfoArray)
{
  Ani_channel_info* aniChannelInfo;
  unsigned int i;
  char* aniNumChan = NULL;
  char* datap = (char*) data;
  if (datap == NULL)
  {
    return -1;
  }
  /* Extract Data from ANI Message Body */
  log_info(LOG_TAG, "RomeExtractChannelInfo()");
  aniNumChan     = (char *)(datap + sizeof(tAniMsgHdr));
  aniChannelInfo = (Ani_channel_info*) (aniNumChan+1);
  log_verbose(LOG_TAG, "%s: aniNumChan: %u \n", __FUNCTION__, (*aniNumChan));
  for( i = 0;i < (*aniNumChan) ; i++ )
  {
    log_verbose(LOG_TAG, "%s: Chan ID = %d", __FUNCTION__, aniChannelInfo[i].chan_id);
    /* Copy Channel info for valid positive channel IDs only */
    if (aniChannelInfo[i].chan_id > 0 && aniChannelInfo[i].chan_id <= MAX_CHANNEL_ID)
    {
      ChannelInfo* channelInfo = &(channelInfoArray[(aniChannelInfo[i].chan_id) - 1]);
      channelInfo->chId = (tANI_U8)aniChannelInfo[i].chan_id;
      memcpy(&(channelInfo->wmiChannelInfo), &(aniChannelInfo[i].channel_info), sizeof(wmi_channel));
      /* Set default values for phy mode */
      channelInfo->wmiChannelInfo.info &= ~(LOWI_PHY_MODE_MASK);
      if (LOWIUtils::freqToBand(channelInfo->wmiChannelInfo.mhz) == LOWIDiscoveryScanRequest::TWO_POINT_FOUR_GHZ)
      {
        channelInfo->wmiChannelInfo.info |= LOWI_PHY_MODE_11G;
      }
      else
      {
        channelInfo->wmiChannelInfo.info |= LOWI_PHY_MODE_11A;
      }

      /* Copy over to Caller's Memory */
      if (pChannelInfoArray != NULL)
      {
        pChannelInfoArray[(channelInfo->chId - 1)].chId = channelInfo->chId;
        memcpy(&(pChannelInfoArray[channelInfo->chId - 1].wmiChannelInfo),
               &(channelInfo->wmiChannelInfo), sizeof(wmi_channel));
      }
    }
  }
#if 0  /* Enable this section to verify the channel info sent by the host driver */
  log_verbose(LOG_TAG, "%s: Here is the channel info From the Host Driver", __FUNCTION__);

  for(i = 0; i < MAX_CHANNEL_ID; i++)
  {
    if(channelInfoArray[i].chId != 0)
    {
      log_verbose(LOG_TAG, "%s: Ch ID: %u\n", __FUNCTION__, channelInfoArray[i].chId);
      log_verbose(LOG_TAG, "%s: MHz#: %u\n", __FUNCTION__, channelInfoArray[i].wmiChannelInfo.mhz);
      log_verbose(LOG_TAG, "%s: band_center_freq1: %u, band_center_freq2: %u\n", __FUNCTION__,
                    channelInfoArray[i].wmiChannelInfo.band_center_freq1,
                    channelInfoArray[i].wmiChannelInfo.band_center_freq2);
      log_verbose(LOG_TAG, "%s: info: 0x%x, reg_info_1: 0x%x, reg_info_2: 0x%x\n", __FUNCTION__,
                    channelInfoArray[i].wmiChannelInfo.info,
                    channelInfoArray[i].wmiChannelInfo.reg_info_1,
                    channelInfoArray[i].wmiChannelInfo.reg_info_2);
    }
    else
    {
      log_verbose(LOG_TAG, "%s: Skipping index %u which is for Ch ID: %u\n", __FUNCTION__, i, i + 1);
    }

  }
#endif
  return 0;
}

/*=============================================================================================
 * Function description:
 * Extract information from Ranging Capability message
 *
 * Parameters:
 *    data   : The message body
 *
 * Return value:
 *    SUCCESS/FAILURE
 =============================================================================================*/
int RomeExtractRangingCap(void* data, RomeRttCapabilities*   pRomeRttCapabilities)
{
  if (data == NULL || pRomeRttCapabilities == NULL)
  {
    log_error(LOG_TAG, "%s, Received invalid pointer for message body or capabilties structure - data: %llu, pRomeCap: %p",
                   __FUNCTION__, data, pRomeRttCapabilities);
    return -1;
  }
  log_info(LOG_TAG, "RomeExtractRangingCap()");
  /** Get pointer to the start of the RTT capability data
   *  Moving pointer by the size of:
   *  The ANI message header +
   *  OEM Data message subtype +
   *  Request ID element */
  char* rttCap = ((char*)(data)) + sizeof(tAniMsgHdr) + sizeof(OemMsgSubType) + sizeof(tANI_U32);

  memcpy(pRomeRttCapabilities, rttCap, sizeof(RomeRttCapabilities));

#ifdef LOWI_RTT_ALWAYS_SUPPORTED
  pRomeRttCapabilities->rangingTypeMask |= CAP_SINGLE_SIDED_RANGING;
  pRomeRttCapabilities->rangingTypeMask |= CAP_DOUBLE_SIDED_RANGING;
  pRomeRttCapabilities->rangingTypeMask |= CAP_11MC_DOUBLE_SIDED_RANGING;
#endif

  /* Extract number of RX chains being used and store in Ranging Driver*/
  rxChainsUsed = 0;
  tANI_U8 rxChainBitMask = pRomeRttCapabilities->maxRfChains;

  /* This for loop counts the bits that are set in the chain mask and stores them*/
  for (rxChainsUsed = 0; rxChainBitMask; rxChainsUsed++)
  {
    rxChainBitMask &= rxChainBitMask - 1;
  }

  return 0;

}

/*=============================================================================================
 * Function description:
 * Extract Error Code from Ranging Error Message
 *
 * Parameters:
 *    data   : The message body
 *    errorCode: Thelocation to store the extracted error code
 *
 * Return value:
 *    SUCCESS/FAILURE
 =============================================================================================*/
int RomeExtractRangingError(void* data, tANI_U32* errorCode, tANI_U8* bssid)
{
  int retVal = 0;
  if (data == NULL ||
      errorCode == NULL ||
      bssid == NULL)
  {
    log_error(LOG_TAG, "%s, Received invalid pointer - data: %p, errorCode: %p, bssid: %p",
                   __FUNCTION__, data, errorCode, bssid);
    return -1;
  }
  RomeRTTReportHeaderIE* romeRTTReportHeaderIE = (RomeRTTReportHeaderIE*)(((char*) data) + sizeof(tAniMsgHdr) + sizeof(OemMsgSubType));
  memcpy(bssid, romeRTTReportHeaderIE->dest_mac, ETH_ALEN + 2);
  tANI_U32* pErrorCode = (tANI_U32*) (((char*) romeRTTReportHeaderIE) + sizeof(RomeRTTReportHeaderIE));
  *errorCode = *pErrorCode;
  return retVal;
}

/*=============================================================================================
 * Function description:
 * Based on the Error Code if the bssid Will be skipped, then the bssid will be added
 * to the "Skipped Targets" List. This will allow the driver to send back appropriate
 * error Codes to the Client.
 *
 * Parameters:
 *    errorCode   : The Error code recieved from FW
 *    bssid       : the bssid associated with the error code
 *
 * Return value:
 *    SUCCESS/FAILURE
 =============================================================================================*/
int RomeAddSkippedTargetToList(tANI_U32 errorCode, tANI_U8 mac[WIFI_MAC_ID_SIZE + 2])
{
  switch (errorCode)
  {
    case RTT_TRANSIMISSION_ERROR:
    case RTT_TMR_TRANS_ERROR:
    case RTT_TM_TIMER_EXPIRE:
    case WMI_RTT_REJECT_MAX:
    {
      /* First check if Target already has an associated error code */
      for (unsigned int i = 0; i <failedTargets.getNumOfElements(); ++i)
      {
        if (!memcmp(failedTargets[i].mac, mac, WIFI_MAC_ID_SIZE))
        {
          /* Already in the List */
          failedTargets[i].errorCode = (WMI_RTT_ERROR_INDICATOR) errorCode;
          log_verbose(LOG_TAG, "%s: Found Target in Failed Target List. Just updating the error code", __FUNCTION__);
          return 0;
        }
      }

      FailedTarget failedTarget;
      failedTarget.errorCode = (WMI_RTT_ERROR_INDICATOR) errorCode;
      memcpy (failedTarget.mac, mac, WIFI_MAC_ID_SIZE + 2);
      failedTargets.push_back(failedTarget);
      log_verbose(LOG_TAG, "%s: Added Target to Failed Target List", __FUNCTION__);
      break;
    }
    default:
    {
      /* Do Nothing */
      log_verbose(LOG_TAG, "%s: Do Nothing", __FUNCTION__);
      break;
    }
  }
#if 0 /* Enable this section code for debugging puurposes */
  for (unsigned int i = 0; i <failedTargets.getNumOfElements(); ++i)
  {
    log_verbose(LOG_TAG, "%s: BSSID: " QUIPC_MACADDR_FMT " ErrorCode: %u",
                   __FUNCTION__,
                   QUIPC_MACADDR(failedTargets[i].mac),
                   failedTargets[i].errorCode);

  }
#endif
  return 0;
}

/*=============================================================================================
 * Function description:
 * Extract information from P2P Peer info message
 *
 * Parameters:
 *    data   : The message body
 *
 * Return value:
 *    SUCCESS/FAILURE
 =============================================================================================*/
int RomeExtractP2PInfo(void* data)
{
  log_debug(LOG_TAG, " %s - P2P EVENT Received!", __FUNCTION__);

  if(NULL == data)
  {
    log_error(LOG_TAG, "%s: got NULL ptr...\n", __FUNCTION__);
    return -1;
  }

  // save peerInfo in table to be used later in ranging requests
  if( p2pStoreStatusInfo((char*) data) )
  {
    log_error(LOG_TAG, "%s: Could not store p2p peer ...\n", __FUNCTION__);
    return -1;
  }

  return 0;
}

void print_tx_rx_params(LOWIMeasurementInfo* measInfo)
{
  log_verbose(LOG_TAG, "%s: TX Params from FW: tx_preamble: %u, tx_nss: %u, tx_bw: %u, tx_mcs: %u, tx_bitrate: %u\n",
                __FUNCTION__,
               measInfo->tx_preamble,
               measInfo->tx_nss,
               measInfo->tx_bw,
               measInfo->tx_mcsIdx,
               measInfo->tx_bitrate);

  log_verbose(LOG_TAG, "%s: RX Params from FW: rx_preamble: %u, rx_nss: %u, rx_bw: %u, rx_mcs: %u, rx_bitrate: %u\n",
                __FUNCTION__,
                measInfo->rx_preamble,
                measInfo->rx_nss,
                measInfo->rx_bw,
                measInfo->rx_mcsIdx,
                measInfo->rx_bitrate);
}

int failedTargetCheck(tANI_U8  dest_mac[ETH_ALEN + 2], WMI_RTT_ERROR_INDICATOR &errorCode)
{
  for (unsigned int i = 0; i < failedTargets.getNumOfElements(); ++i)
  {
    if(!memcmp(dest_mac, failedTargets[i].mac, WIFI_MAC_ID_SIZE+2))
    {
      errorCode = failedTargets[i].errorCode;
      log_verbose(LOG_TAG, "%s: Found Target in Failed Target List.", __FUNCTION__);
      return 1;
    }
  }
  log_verbose(LOG_TAG, "%s: Did not Found Target in Failed Target List.", __FUNCTION__);
  return 0;
}

/*=============================================================================================
 * Function description:
 *   Processes the received RTT response from Rome FW
 *
 * Parameters:
 *   measResp: The ranging measurements from FW.
 *   scanMeasurements: The destination where parsed scan measurements
 *                     will be stored.
 *
 * Return value:
 *   error Code: 0 - Success, -1 - Failure
 *
 =============================================================================================*/
int qc_loc_fw::RomeParseRangingMeas(char* measResp, vector <LOWIScanMeasurement*> *scanMeasurements)
{
  tANI_U8 rttFrameType = FRAME_TYPE_NULL;
  char* tempPtr = NULL;
  OemMsgSubType *oemMsgSubType;
  uint64 rttMeasTimestamp = 0;

  if (scanMeasurements == NULL)
  {
    return -1;
  }

  oemMsgSubType = (OemMsgSubType *) (measResp + sizeof(tAniMsgHdr));

  /* Extract RTT Report */
  RomeRTTReportHeaderIE*    rttReportHdr      = (RomeRTTReportHeaderIE *)  ((char*)oemMsgSubType     + sizeof(OemMsgSubType));
  RomeRttPerPeerReportHdr*  rttPerAPReportHdr = (RomeRttPerPeerReportHdr*) ((char*)rttReportHdr      + sizeof(RomeRTTReportHeaderIE));
  uint8*                    romeRttIes        = ((unsigned char*)rttPerAPReportHdr                   + sizeof(RomeRttPerPeerReportHdr));
  RomeRttPerFrame_IE_RTTV2* rttPerFrameRtt2IE = (RomeRttPerFrame_IE_RTTV2*)((char*)rttPerAPReportHdr + sizeof(RomeRttPerPeerReportHdr));
  RomeRttPerFrame_IE_RTTV3* rttPerFrameRtt3IE = (RomeRttPerFrame_IE_RTTV3*)((char*)rttPerAPReportHdr + sizeof(RomeRttPerPeerReportHdr));

  /* RTT was successfull -  Report has valid Data */
  /* Start extracting Measurements */

  uint8 rtsctsTag  = WMI_RTT_REPORT_REQ_ID_GET(rttReportHdr->req_id);
  uint8 status     = WMI_RTT_REPORT_STATUS_GET(rttReportHdr->req_id);
  uint8 numBSSIDs  = WMI_RTT_REPORT_NUM_AP_GET(rttReportHdr->req_id);
  uint8 reportType = WMI_RTT_REPORT_REPORT_TYPE_GET(rttReportHdr->req_id);

  /* Enable following section to debug the Data coming from FW */
#if 0
  uint32* dPtr = (uint32*) (rttReportHdr);
  #define RTT_SIZE_OF_STRING 5
  for (unsigned int kk = 0; kk < ((MAX_WMI_MESSAGE_SIZE / 4) / 5) ; kk++)
  {
    unsigned int idx = kk * 5;
    log_verbose(LOG_TAG, "%s: Data from FW: 0x%x 0x%x 0x%x 0x%x 0x%x ", __FUNCTION__, dPtr[idx], dPtr[idx + 1], dPtr[idx + 2], dPtr[idx + 3], dPtr[idx + 4]);
  }
#endif

  if (reportType != REPORT_AGGREGATE_MULTIFRAME)
  {
    log_warning(LOG_TAG, "%s: Received RTT report type: %u, when not expecting it.",
                  __FUNCTION__, reportType);
    return -1;
  }
  uint32* freq = (uint32*)rttReportHdr->dest_mac;
  *freq = WMI_RTT_REPORT_CHAN_INFO_GET(*freq);
  log_verbose(LOG_TAG, "%s: Received RTT response from FW: numBSSIDs: %u, ReqID: 0x%x, channel: %u\n",
                __FUNCTION__,
                numBSSIDs,
                (rttReportHdr->req_id),
                *freq);

  for(unsigned int i = 0; i < numBSSIDs; i++)
  {
    LOWIScanMeasurement* rangingMeasurement = new (std::nothrow) LOWIScanMeasurement;
    bool invalidTimeStamp = false;
    if (rangingMeasurement == NULL)
    {
      log_warning(LOG_TAG, "%s: Failed to allocate memory for rangingMeasurement", __FUNCTION__);
      return -1;
    }
    rangingMeasurement->bssid.setMac(rttPerAPReportHdr->dest_mac);
    rangingMeasurement->frequency = *freq;
    rangingMeasurement->isSecure = false;
    rangingMeasurement->msapInfo = NULL;
    rangingMeasurement->cellPowerLimitdBm = 0;

    /* by default the measurement is a success */
    rangingMeasurement->targetStatus = LOWIScanMeasurement::LOWI_TARGET_STATUS_SUCCESS;

    if (p2pIsStored(rttPerAPReportHdr->dest_mac, NULL))
    {
      rangingMeasurement->type = PEER_DEVICE;
    }
    else
    {
      rangingMeasurement->type = ACCESS_POINT;
    }

    rttFrameType = WMI_RTT_REPORT_TYPE2_MEAS_TYPE_GET(rttPerAPReportHdr->control);
    if (rttFrameType == RTT_MEAS_FRAME_TMR)
    {
      rangingMeasurement->rttType = qc_loc_fw::RTT3_RANGING;
    }
    else
    {
      rangingMeasurement->rttType = qc_loc_fw::RTT2_RANGING;
    }

    uint32 numSuccessfulMeasurements = WMI_RTT_REPORT_TYPE2_NUM_MEAS_GET(rttPerAPReportHdr->control);

    rangingMeasurement->num_frames_attempted = WMI_RTT_REPORT_TYPE2_NUM_FRAMES_ATTEMPTED_GET(rttPerAPReportHdr->result_info1);
    rangingMeasurement->actual_burst_duration = WMI_RTT_REPORT_TYPE2_ACT_BURST_DUR_GET(rttPerAPReportHdr->result_info1);
    rangingMeasurement->negotiated_num_frames_per_burst = WMI_RTT_REPORT_TYPE2_NEGOT_NUM_FRAMES_PER_BURST_GET(rttPerAPReportHdr->result_info2);
    rangingMeasurement->retry_after_duration = WMI_RTT_REPORT_TYPE2_RETRY_AFTER_DUR_GET(rttPerAPReportHdr->result_info2);
    rangingMeasurement->negotiated_burst_exp = WMI_RTT_REPORT_TYPE2_ACT_BURST_EXP_GET(rttPerAPReportHdr->result_info2);
    uint32 numIes = WMI_RTT_REPORT_TYPE2_NUM_IES_GET(rttPerAPReportHdr->result_info2);

    log_debug(LOG_TAG, "%s: BSSID: " QUIPC_MACADDR_FMT ", controlField: 0x%x, numMeas: %u, rttFrameType: %u, num_frames_attempted: %u, actual_burst_duration: %u, negotiated_num_frames_per_burst: %u, retry_after_duration: %u, negotiated_burst_exp: %u, numIEs: %u\n",
                 __FUNCTION__,
                 QUIPC_MACADDR(rttPerAPReportHdr->dest_mac),
                 rttPerAPReportHdr->control,
                 numSuccessfulMeasurements,
                 rttFrameType,
                 rangingMeasurement->num_frames_attempted,
                 rangingMeasurement->actual_burst_duration,
                 rangingMeasurement->negotiated_num_frames_per_burst,
                 rangingMeasurement->retry_after_duration,
                 rangingMeasurement->negotiated_burst_exp,
                 numIes);

    rangingMeasurement->lciInfo = NULL;
    rangingMeasurement->lcrInfo = NULL;

    if (numIes)
    {
      uint8* rttIes;
      LOWILocationIE* locIe = NULL;

      for (unsigned int i = 0; i < numIes; i++)
      {
        rttIes = romeRttIes;
        // The first byte is the ID - access like an array
        uint8 id = rttIes[0];
        // The second byte is the Length - access like an array
        uint8 len = rttIes[1];
        // Move pointer by 2 bytes to point to the data section;
        romeRttIes += 2 * sizeof(uint8);
        log_verbose(LOG_TAG, "%s: LOWI received IE: %u, and Len: %u", __FUNCTION__, id, len);
        if (i > 1) // LOWI should receive only 2 IEs
        {
          log_warning(LOG_TAG, "%s: LOWI received more than 2 IES from FW", __FUNCTION__);
        }
        else
        {
          if (romeRttIes[2] == RTT_LCI_ELE_ID ||
              romeRttIes[2] == RTT_LOC_CIVIC_ELE_ID)
          {
            locIe = new LOWILocationIE();
            if (locIe == NULL)
            {
              log_warning(LOG_TAG, "%s - Failed to allocate memory!", __FUNCTION__);
              return -1;
            }
            locIe->id = id;
            locIe->len = len;
            locIe->locData = (uint8*)malloc(len);
            if (NULL == locIe->locData)
            {
              log_warning(LOG_TAG, "%s - Failed to allocate memory!", __FUNCTION__);
              return -1;
            }

            memcpy(locIe->locData, &romeRttIes[0], len);
          }
          if (locIe == NULL)
          {
            log_warning(LOG_TAG, "%s - locIe is NULL!", __FUNCTION__);
            return -1;
          }
          switch(romeRttIes[2])
          {
            case RTT_LCI_ELE_ID:
            {
              if(rangingMeasurement->lciInfo == NULL)
              {
                log_verbose(LOG_TAG, "%s: LOWI received an LCI IE %u with len: %u", __FUNCTION__, romeRttIes[2], locIe->len);
                rangingMeasurement->lciInfo = locIe;
              }
              else
              {
                log_warning(LOG_TAG, "%s: LOWI already received an LCI IE discarding this one", __FUNCTION__);
                delete locIe;
              }
              break;
            }
            case RTT_LOC_CIVIC_ELE_ID:
            {
              if (rangingMeasurement->lcrInfo == NULL)
              {
                log_verbose(LOG_TAG, "%s: LOWI received an LCR IE %u with len: %u", __FUNCTION__, romeRttIes[2], locIe->len);
                rangingMeasurement->lcrInfo = locIe;
              }
              else
              {
                log_warning(LOG_TAG, "%s: LOWI already received an LCR IE discarding this one", __FUNCTION__);
                delete locIe;
              }

              break;
            }
            default:
            {
              log_warning(LOG_TAG, "%s: LOWI received an IE it doesn't recognize: %u", __FUNCTION__, romeRttIes[2]);
              break;
            }
          }
          locIe = NULL;
        }

        /* Move to next IE */
        romeRttIes += len;
        if ((len + 2) % 4) // check if length is 4 byte aligned
        {
          romeRttIes += (4 - ((len + 2)% 4));
        }
      }

      /* If there are measurements following, They will be after the IEs
         So setting Meas pointers accordingly */
      rttPerFrameRtt2IE = (RomeRttPerFrame_IE_RTTV2*)(romeRttIes);
      rttPerFrameRtt3IE = (RomeRttPerFrame_IE_RTTV3*)(romeRttIes);
    }
    else
    {
      log_verbose(LOG_TAG, "%s: No IEs were received", __FUNCTION__);
    }

    if(numSuccessfulMeasurements)
    {
      for(unsigned int j = 0; j < numSuccessfulMeasurements; j++)
      {
        tANI_U64 rttTod = 0, rttToa = 0, rtt64 = 0;

        LOWIMeasurementInfo* measurementInfo = new (std::nothrow) LOWIMeasurementInfo;
        if (measurementInfo == NULL)
        {
          log_warning(LOG_TAG, "%s: Failed to allocate memmory for measurementInfo", __FUNCTION__);
          return -1;
        }

        if(rttFrameType == FRAME_TYPE_NULL ||
           rttFrameType == FRAME_TYPE_QOS_NULL)
        {
          measurementInfo->rssi = rttPerFrameRtt2IE->rssi & 0x000000FF;

          log_verbose(LOG_TAG, "%s: RTT V2 Performed - Raw RSSI: 0x%x\n",
                        __FUNCTION__,
                        measurementInfo->rssi);

          /* Convert RSSI to 0.5 dBm units */
          measurementInfo->rssi = LOWI_RSSI_05DBM_UNITS(measurementInfo->rssi);

          log_verbose(LOG_TAG, "%s: TOD.time32: 0x%x, TOD.time0: 0x%x, TOA.time32: 0x%x, TOA.time0: 0x%x \n", __FUNCTION__,
                        rttPerFrameRtt2IE->tod.time32,
                        rttPerFrameRtt2IE->tod.time0,
                        rttPerFrameRtt2IE->toa.time32,
                        rttPerFrameRtt2IE->toa.time0);

          rttTod = (tANI_U64)rttPerFrameRtt2IE->tod.time32;
          rttTod = ((rttTod << 32) | rttPerFrameRtt2IE->tod.time0);
          rttToa = (tANI_U64)rttPerFrameRtt2IE->toa.time32;
          rttToa = ((rttToa << 32) | rttPerFrameRtt2IE->toa.time0);
          rtt64 = rttToa - rttTod;
          /** Note: For now we are subtracting a offset value which will be removed
           *  After the FW team figures out the HW calibration factor
           */
          measurementInfo->rtt_ps = ((tANI_U32)((rtt64) - RTT2_OFFSET))*100;
          measurementInfo->rtt    = measurementInfo->rtt_ps/1000;

          log_debug(LOG_TAG, "%s: RTT: %u (ps), RSSI: %d \n",
                       __FUNCTION__,
                       measurementInfo->rtt_ps,
                       measurementInfo->rssi);

          /* Get TX Parameters */
          measurementInfo->tx_preamble = WMI_RTT_RSP_X_PREAMBLE_GET(rttPerFrameRtt2IE->tx_rate_info_1);
          measurementInfo->tx_nss = TX_CHAIN_1;
          measurementInfo->tx_bw = WMI_RTT_RSP_X_BW_USED_GET(rttPerFrameRtt2IE->tx_rate_info_1);
          measurementInfo->tx_mcsIdx = WMI_RTT_RSP_X_MCS_GET(rttPerFrameRtt2IE->tx_rate_info_1);
          measurementInfo->tx_bitrate = rttPerFrameRtt2IE->tx_rate_info_2;
          /* Get RX Parameters */
          measurementInfo->rx_preamble = WMI_RTT_RSP_X_PREAMBLE_GET(rttPerFrameRtt2IE->rx_rate_info_1);
          measurementInfo->rx_nss = rxChainsUsed;
          measurementInfo->rx_bw = WMI_RTT_RSP_X_BW_USED_GET(rttPerFrameRtt2IE->rx_rate_info_1);
          measurementInfo->rx_mcsIdx = WMI_RTT_RSP_X_MCS_GET(rttPerFrameRtt2IE->rx_rate_info_1);
          measurementInfo->rx_bitrate = rttPerFrameRtt2IE->rx_rate_info_2;

          /* Increment pointers - Per frame Pointers*/
          rttPerFrameRtt2IE++;
        }
        else if(rttFrameType == FRAME_TYPE_TMR)
        {
          log_verbose(LOG_TAG, "%s: T1.time32: 0x%x, T1.time0: 0x%x, T2.time32: 0x%x, T2.time0: 0x%x, T3_del: 0x%x, T4_del: 0x%x \n",
                        __FUNCTION__,
                        rttPerFrameRtt3IE->t1.time32,
                        rttPerFrameRtt3IE->t1.time0,
                        rttPerFrameRtt3IE->t2.time32,
                        rttPerFrameRtt3IE->t2.time0,
                        rttPerFrameRtt3IE->t3_del,
                        rttPerFrameRtt3IE->t4_del);

          /* collect Time stamp from first measurement */
          if (j == 0)
          {
            rttMeasTimestamp = (uint64) (rttPerFrameRtt3IE->t2.time32);
            rttMeasTimestamp = (uint64) ((rttMeasTimestamp << 32) | rttPerFrameRtt3IE->t2.time0);
            log_verbose(LOG_TAG, "%s: rttMeasTimestamp = 0x%llx", __FUNCTION__, rttMeasTimestamp);
          }
          WMI_RTT_ERROR_INDICATOR errCode;
          if(failedTargetCheck(rttPerAPReportHdr->dest_mac, errCode))
          {
            /** This AP has a failure code associated with it, which
             *  means we shall discard its measurements and use only the
             *  timestamp from the first measurement */
            log_verbose(LOG_TAG, "%s: Failed to range with this target - Not parsing Measurement",
                          __FUNCTION__);
            delete measurementInfo;
            measurementInfo = NULL;
            break;
          }
          else if (numSuccessfulMeasurements == 1 &&
                   (!rttPerFrameRtt3IE->t3_del &&
                    !rttPerFrameRtt3IE->t4_del))
          {
            /* This implies that there are no valid successful measurements */
            invalidTimeStamp = true;
            delete measurementInfo;
            measurementInfo = NULL;
            break; /* Skip this measurement */
          }
          measurementInfo->rssi = rttPerFrameRtt3IE->rssi & 0x000000FF;

          /* Convert RSSI to 0.5 dBm units */
          measurementInfo->rssi = LOWI_RSSI_05DBM_UNITS(measurementInfo->rssi);

          /** According to FW team t3_del & t4_del are defined as follows
           *  t3_del = T3 - T2
           *  t4_Del = T4 - T1
           *
           * In RTTV3 protocol:   RTT =  (T4 - T1) - (T3 - T2)
           * which translates to: RTT =    t4_del  -  t3_del
           * T1 and T2 are not needed for calculating RTT
           */

          if (rttPerFrameRtt3IE->t4_del < rttPerFrameRtt3IE->t3_del)
          {
            log_info(LOG_TAG, "%s: WARNING - Delta_T4: %u < Delta_T3: %u\n", __FUNCTION__,
                          rttPerFrameRtt3IE->t4_del, rttPerFrameRtt3IE->t3_del);
            invalidTimeStamp = true;
            /* Increment per frame Pointer to point to next measurement frame*/
            rttPerFrameRtt3IE++;
            delete measurementInfo;
            measurementInfo = NULL;
            continue; /* Skip this measurement */
          }
          else if (rttPerFrameRtt3IE->t4_del == rttPerFrameRtt3IE->t3_del)
          {
            /* When RTT is 0, send the user a minimum */
            measurementInfo->rtt_ps = (tANI_U32) RTT3_MIN_DEFAULT;
          }
          else
          {
            rtt64 = rttPerFrameRtt3IE->t4_del - rttPerFrameRtt3IE->t3_del;

            /* Check to see if RTT3 value is a very large outlier */
            if (rtt64 > LOWI_RTT3_MAX)
            {
              log_info(LOG_TAG, "%s: WARNING - RTT3 > %u: %u, Delta_T4: %u & Delta_T3: %u\n",
                            __FUNCTION__,
                            LOWI_RTT3_MAX,
                            rtt64,
                            rttPerFrameRtt3IE->t4_del,
                            rttPerFrameRtt3IE->t3_del);
              invalidTimeStamp = true;
              /* Increment per frame Pointer to point to next measurement frame*/
              rttPerFrameRtt3IE++;
              delete measurementInfo;
              measurementInfo = NULL;
              continue; /* Skip this measurement */
            }
            else
            {
              measurementInfo->rtt_ps = ((tANI_U32)(rtt64))*100;
            }
          }

          measurementInfo->rtt    = measurementInfo->rtt_ps/1000;
          log_debug(LOG_TAG, "%s: RTT: %u (ps), RSSI: %d \n", __FUNCTION__,
                       measurementInfo->rtt_ps,
                       measurementInfo->rssi);

          /* Get TX Parameters */
          measurementInfo->tx_preamble = WMI_RTT_RSP_X_PREAMBLE_GET(rttPerFrameRtt3IE->tx_rate_info_1);
          measurementInfo->tx_nss = TX_CHAIN_1;
          measurementInfo->tx_bw = WMI_RTT_RSP_X_BW_USED_GET(rttPerFrameRtt3IE->tx_rate_info_1);
          measurementInfo->tx_mcsIdx = WMI_RTT_RSP_X_MCS_GET(rttPerFrameRtt3IE->tx_rate_info_1);
          measurementInfo->tx_bitrate = rttPerFrameRtt3IE->tx_rate_info_2;
          /* Get RX Parameters */
          measurementInfo->rx_preamble = WMI_RTT_RSP_X_PREAMBLE_GET(rttPerFrameRtt3IE->rx_rate_info_1);
          measurementInfo->rx_nss = rxChainsUsed;
          measurementInfo->rx_bw = WMI_RTT_RSP_X_BW_USED_GET(rttPerFrameRtt3IE->rx_rate_info_1);
          measurementInfo->rx_mcsIdx = WMI_RTT_RSP_X_MCS_GET(rttPerFrameRtt3IE->rx_rate_info_1);
          measurementInfo->rx_bitrate = rttPerFrameRtt3IE->rx_rate_info_2;

          log_verbose(LOG_TAG, "%s: RTT V3 Performed - Preamble: %s, RX BW: %s, MCS Index: %u, BitRate: %u (100Kbps) Raw RSSI: 0x%x \n",
                        __FUNCTION__,
                        LOWI_TO_STRING(measurementInfo->rx_preamble, PREAMBLE_DBG),
                        LOWI_TO_STRING(measurementInfo->rx_bw, TX_RX_BW),
                        measurementInfo->rx_mcsIdx,
                        measurementInfo->rx_bitrate,
                        measurementInfo->rssi);

          /* Increment pointers - Per frame Pointers*/
          rttPerFrameRtt3IE++;
        }

        //print_tx_rx_params(measurementInfo);
        /* Set the timestamp for this measurement */
        measurementInfo->rssi_timestamp = measurementInfo->rtt_timestamp = LOWIUtils::currentTimeMs();
        rangingMeasurement->measurementsInfo.push_back(measurementInfo);
      } /* loop end */
    }

    /** Section of code that assigns the error codes */
    {

      WMI_RTT_ERROR_INDICATOR errorCode;
      if (failedTargetCheck(rttPerAPReportHdr->dest_mac, errorCode))
      {
        /* Target failed so it was skipped by FW */
        if (errorCode == RTT_TRANSIMISSION_ERROR ||
            errorCode == RTT_TMR_TRANS_ERROR)
        {
          rangingMeasurement->targetStatus = LOWIScanMeasurement::LOWI_TARGET_STATUS_RTT_FAIL_NO_RSP;
          log_verbose(LOG_TAG, "%s: Target Setting Status to LOWI_TARGET_STATUS_RTT_FAIL_NO_RSP\n",
                        __FUNCTION__);
        }
        else if (errorCode == WMI_RTT_REJECT_MAX)
        {
          rangingMeasurement->targetStatus = LOWIScanMeasurement::LOWI_TARGET_STATUS_RTT_FAIL_REJECTED;
          log_verbose(LOG_TAG, "%s: Target Setting Status to LOWI_TARGET_STATUS_RTT_FAIL_REJECTED\n",
                        __FUNCTION__);
        }
        else if (errorCode == RTT_TM_TIMER_EXPIRE)
        {
          rangingMeasurement->targetStatus = LOWIScanMeasurement::LOWI_TARGET_STATUS_RTT_FAIL_FTM_TIMEOUT;
          log_verbose(LOG_TAG, "%s: Target Setting Status to LOWI_TARGET_STATUS_RTT_FAIL_FTM_TIMEOUT\n",
                        __FUNCTION__);
        }
      }
      else if (rangingMeasurement->retry_after_duration)
      {
        rangingMeasurement->targetStatus = LOWIScanMeasurement::LOWI_TARGET_STATUS_RTT_FAIL_TARGET_BUSY_TRY_LATER;
        log_verbose(LOG_TAG, "%s: Target Setting Status to LOWI_TARGET_STATUS_RTT_FAIL_TARGET_BUSY_TRY_LATER\n",
                      __FUNCTION__);
      }
    }

    /* Target has no valid measurements because all the Time stamps were invalid */
    if (invalidTimeStamp &&
        rangingMeasurement->measurementsInfo.getNumOfElements() == 0)
    {
      rangingMeasurement->targetStatus = LOWIScanMeasurement::LOWI_TARGET_STATUS_RTT_FAIL_INVALID_TS;
      log_verbose(LOG_TAG, "%s: Target Setting Status to LOWI_TARGET_STATUS_RTT_FAIL_INVALID_TS\n",
                    __FUNCTION__);
    }

    /* Store RTT time stamp for when the measurement was started in FW */
    rangingMeasurement->rttMeasTimeStamp = rttMeasTimestamp;
    log_verbose(LOG_TAG, "%s: The RTT timestamp for this target: 0x%llx\n",
                  __FUNCTION__, rangingMeasurement->rttMeasTimeStamp);

    /* Increment and reset pointers - Per AP pointers*/
    tempPtr = (char*)rttPerAPReportHdr;
    if(rttFrameType == FRAME_TYPE_TMR)
    {
      tempPtr = tempPtr + (sizeof(RomeRttPerPeerReportHdr) + (sizeof(RomeRttPerFrame_IE_RTTV3) * numSuccessfulMeasurements));
    }
    else
    {
      tempPtr = tempPtr + (sizeof(RomeRttPerPeerReportHdr) + (sizeof(RomeRttPerFrame_IE_RTTV2) * numSuccessfulMeasurements));
    }
    rttPerAPReportHdr = (RomeRttPerPeerReportHdr*)tempPtr;
    rttPerFrameRtt2IE = (RomeRttPerFrame_IE_RTTV2*)(tempPtr + sizeof(RomeRttPerPeerReportHdr));
    rttPerFrameRtt3IE = (RomeRttPerFrame_IE_RTTV3*)(tempPtr + sizeof(RomeRttPerPeerReportHdr));

    log_verbose(LOG_TAG, "%s: The Target Status is set to: %u\n", __FUNCTION__, rangingMeasurement->targetStatus);
    scanMeasurements->push_back(rangingMeasurement);
    invalidTimeStamp = false;
  }

  failedTargets.flush();
  return 0;
}

/************ END - Message Handlers ****************/

