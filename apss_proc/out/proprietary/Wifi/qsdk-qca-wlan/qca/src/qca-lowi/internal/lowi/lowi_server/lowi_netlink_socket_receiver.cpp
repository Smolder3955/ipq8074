/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

        LOWI NetLink Socket Receiver

GENERAL DESCRIPTION
  This file contains the implementation of LOWI NetLink Socket Receiver

Copyright (c) 2015-2016 Qualcomm Technologies, Inc.
  All Rights Reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.

=============================================================================*/
#include <string.h>
#include <base_util/log.h>
#include <lowi_server/lowi_netlink_socket_receiver.h>
#include <lowi_server/lowi_wifidriver_interface.h>
#include "lowi_wifidriver_utils.h"
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>

using namespace qc_loc_fw;

#define MAX_PKT_SIZE 8192 //max size of full netlink message
#define MAX_SOCKET_CREATION_RETRY_COUNT 3
#define SOCKET_CREATION_RETRY_WAIT_TIME_IN_SEC 1
const char * const LOWINetlinkSocketReceiver::TAG = "LOWINetlinkSocketReceiver";
int LOWINetlinkSocketReceiver::mNetlinkSock = 0;

LOWINetlinkSocketReceiver::LOWINetlinkSocketReceiver
(
    LOWIScanResultReceiverListener* listener
)
:LOWIScanResultReceiver(listener, NULL),
  mWifiInterfaceName(NULL),mState(SOCKET_FAILURE)
{
  mThreadAboutToBlock = false;
  mThreadTerminateRequested = false;
  mListener  = listener;
  mPipeFd[0] = 0;
  mPipeFd[1] = 0;
}

LOWINetlinkSocketReceiver::~LOWINetlinkSocketReceiver ()
{
  log_verbose (TAG, "~LOWINetlinkSocketReceiver");
  if (mReceiverThread != NULL)
  {
    terminate ();
  }
  if (mPipeFd[0] > 0)
  {
    close(mPipeFd[0]);
  }
  if (mPipeFd[1] > 0)
  {
    close(mPipeFd[1]);
  }
}

bool LOWINetlinkSocketReceiver::terminate ()
{
  bool retVal = false;
  char strClose [] = "Close";
  log_verbose (TAG, "%s", __FUNCTION__);
  bool thread_running = false;

  {
    AutoLock autolock(mMutex);
    thread_running = mThreadAboutToBlock;
    mThreadTerminateRequested = true;
    if (false == thread_running)
    {
      retVal = true;
    }
  }

  if (true == thread_running)
  {
    if ( write(mPipeFd[1], strClose, strlen(strClose)+1) <= 0)
    {
      log_debug (TAG, "%s: shutdown failed, error = %s(%d)", __FUNCTION__,
                 strerror(errno), errno);
    }
    else
    {
      log_verbose (TAG, "%s: success", __FUNCTION__);
      retVal = true;
    }
  }

  log_verbose (TAG, "%s: About to join, thread %s", __FUNCTION__,
               thread_running ? "RUNNING" : "STOPPED");
  mReceiverThread->join ();
  log_info (TAG, "%s: After join complete", __FUNCTION__);

  {
    AutoLock autolock(mMutex);
    mThreadTerminateRequested = false;
  }
  return retVal;
}

void LOWINetlinkSocketReceiver::run ()
{

  log_verbose (TAG, "%s", __FUNCTION__);
  for (int i = 0; i <= MAX_SOCKET_CREATION_RETRY_COUNT; i++)
  {
    int result = createNLSocket();
    if ( result == 0 )
    {
      log_verbose(TAG, "%s: createNLSocket returned %d", __FUNCTION__, result);
      mState = SOCKET_SUCCESS;
      break;
    }
    else
    {
      mState = SOCKET_FAILURE;
      log_debug(TAG, "%s: createNLSocket returned error %d. Retrying",
                __FUNCTION__, result);
      if(MAX_SOCKET_CREATION_RETRY_COUNT == i)
      {
        log_warning(TAG, "%s: unable to create socket after %d tries",
                    __FUNCTION__, i);
        //no point in running thread if socket is not there
        //so returning
        return;
      }
      if ( i < MAX_SOCKET_CREATION_RETRY_COUNT)
      {
        sleep(SOCKET_CREATION_RETRY_WAIT_TIME_IN_SEC);
      }
    }
  }
  if (pipe(mPipeFd) < 0)
  {
    log_debug(TAG, "%s: Unable to create pipe, error = %s(%d)", __FUNCTION__,
              strerror(errno), errno);
  }
  mWifiInterfaceName = LOWIWifiDriverUtils::get_interface_name();
  do
  {
    {
      AutoLock autolock(mMutex);
      if (true == mThreadTerminateRequested)
      {
        log_info (TAG, "%s: Thread terminate was requested", __FUNCTION__);
        break;
      }
      mThreadAboutToBlock = true;
    }

    //block on select call
    int retVal = block();
    if ( (retVal == EAGAIN) || (retVal == EINTR) )
    {
      continue;
    }
    else if (retVal != ERR_SELECT_TERMINATED)
    {
      //receive and parse the netlink message
      retVal = readParseNetlinkMessage ();
      if ( (retVal == EAGAIN) || (retVal == EINTR) )
      {
        continue;
      }
    }

    {
      AutoLock autolock(mMutex);
      mThreadAboutToBlock = false;
    }

  } while (1);
}

int LOWINetlinkSocketReceiver::block ()
{
   fd_set readSet;
   int retVal = 1;
   int max_fd = MAX(mNetlinkSock, mPipeFd[0]);

   if (max_fd <= 0)
   {
     log_warning (TAG, "%s: NL fd (%d) and pipe fd (%d) are not valid",
                  __FUNCTION__, mNetlinkSock, mPipeFd[0]);
     retVal = -1;
     return retVal;
   }

   FD_ZERO( &readSet );
   if (mNetlinkSock > 0)
   {
     FD_SET(mNetlinkSock, &readSet);
   }
   if (mPipeFd[0] > 0)
   {
     FD_SET(mPipeFd[0], &readSet);
   }

   retVal = select( max_fd+1, &readSet, NULL, NULL, NULL);
   if ( retVal < 0 )
   {
     if ( (errno == EAGAIN) || (errno == EINTR) )
     {
         return errno;
     }
     log_debug (TAG, "%s: select returned %d", __FUNCTION__, errno );
     return retVal;
   }
   else if ((mPipeFd[0] > 0) && FD_ISSET(mPipeFd[0], &readSet))
   {
     char readbuffer [50] = "";
     int nbytes = read(mPipeFd[0], readbuffer, sizeof(readbuffer));

     log_debug (TAG, "%s: read returned %d, Received string: %s",
                __FUNCTION__, nbytes, readbuffer);
     retVal = ERR_SELECT_TERMINATED;
   }
   return retVal;
}

int LOWINetlinkSocketReceiver::readParseNetlinkMessage ()
{
  int bytesRx;
  struct sockaddr_nl  sockAddr;
  struct sockaddr     *pSockAddr = (struct sockaddr *)&sockAddr;
  char *gRxBuf = (char *)malloc(MAX_PKT_SIZE);
  if(NULL == gRxBuf)
  {
    log_debug (TAG, "%s: memory allocation failed", __FUNCTION__);
    return 0;
  }
  socklen_t sockAddrLen = sizeof(struct sockaddr_nl);
  memset(&sockAddr, 0, sizeof(struct sockaddr_nl));

  bytesRx = recvfrom( mNetlinkSock, gRxBuf, MAX_PKT_SIZE, MSG_DONTWAIT,pSockAddr, &sockAddrLen );

  if (sockAddr.nl_pid != 0)
  {
    log_debug(TAG, "%s: Ignore msg received from non-kernel process (%d)",
              __FUNCTION__, sockAddr.nl_pid);
    free(gRxBuf);
    return 0;
  }

  if ( bytesRx < 0 )
  {
    if ( (errno == EAGAIN) || (errno == EINTR) )
    {
      free(gRxBuf);
      return errno;
    }
    log_debug(TAG,"%s: recvfrom returned %d", __FUNCTION__, errno);
    free(gRxBuf);
    return errno;
  }
  //process the received netlink message to read RTM Netlink
  //(nlmsghdr) packets
  processNetlinkPacket( gRxBuf, bytesRx );
  free(gRxBuf);
  return 1;
}

int LOWINetlinkSocketReceiver::createNLSocket ()
{
  int retVal = 0;
  struct sockaddr_nl sockAddr;
  struct sockaddr *pSockAddr = (struct sockaddr *)&sockAddr;

  log_verbose(TAG, "%s", __FUNCTION__);
  if(0 == mNetlinkSock)
  {
    mNetlinkSock = socket( PF_NETLINK, SOCK_RAW, NETLINK_ROUTE );
    if ( mNetlinkSock < 0 )
    {
      log_warning( TAG, "%s: socket returned: %d", __FUNCTION__, errno );
      retVal = -1;
      mNetlinkSock = 0;
      return retVal;
    }
    memset( &sockAddr, 0, sizeof(sockAddr) );
    sockAddr.nl_family = AF_NETLINK;
    sockAddr.nl_groups = RTMGRP_LINK;
    retVal = bind( mNetlinkSock, pSockAddr, sizeof(sockAddr) );
    if ( retVal < 0 )
    {
      log_warning( TAG, "%s: bind returned: %d", __FUNCTION__, errno );
      mNetlinkSock = 0;
      return (retVal);
    }
  }
  return retVal;
}

void LOWINetlinkSocketReceiver::processNetlinkPacket( char *buf, int bytesRx )
{
    bool retVal = false;

    //Netlink messages consist of a byte stream with
    //one or multiple nlmsghdr headers and associated payload.
    struct nlmsghdr *pNetlinkHdr;

    pNetlinkHdr = (struct nlmsghdr *)buf;
    while ( bytesRx >= (int)sizeof(*pNetlinkHdr) )
    {
        int msgLen = pNetlinkHdr->nlmsg_len;
        int plen = msgLen - sizeof(*pNetlinkHdr);
        eWifiIntfState wifiState = INTF_UNKNOWN;

        //read the ifname, Only process NEWLINK and DELLINK for wlan0
        retVal = unpackRTMLinkMessage( pNetlinkHdr, wifiState );
        if (retVal && wifiState != INTF_UNKNOWN)
        {
          log_info ( TAG, "%s: WIFI status %d", __FUNCTION__, wifiState);
          mListener->wifiStateReceived (wifiState);
        }
        msgLen = NLMSG_ALIGN( msgLen );
        bytesRx -= msgLen;
        pNetlinkHdr = (struct nlmsghdr*)((char*)pNetlinkHdr + msgLen);
  }
}

bool LOWINetlinkSocketReceiver::unpackRTMLinkMessage( struct nlmsghdr *pNetlinkHdr,
                                                      eWifiIntfState& intfState )
{
  bool retVal = FALSE;
  do
  {
    if ((pNetlinkHdr == NULL) ||
        !((pNetlinkHdr->nlmsg_type == RTM_NEWLINK) ||
          (pNetlinkHdr->nlmsg_type == RTM_DELLINK)))
    {
      log_info(TAG, "%s: Not parsing message ptr = %p, type = %d", __FUNCTION__,
               pNetlinkHdr, (pNetlinkHdr ? pNetlinkHdr->nlmsg_type : -1));
      break;
    }
    //information about a specific network inferface(NEWLINK & DELLINK)
    //is in ifinfomsg header followed by a series of rtattr structures.
    // NLMSG_DATA Return a pointer to the payload associated with the passed nlmsghdr.
    struct ifinfomsg        *pIfInfo = (struct ifinfomsg *)NLMSG_DATA( pNetlinkHdr );

    // NLMSG_ALIGN Round the length of a netlink message up to align it properly.
    int                     ifInfoLen = NLMSG_ALIGN( sizeof( struct ifinfomsg ));
    int                     netlinkLen = pNetlinkHdr->nlmsg_len;
    char                    ifname[IFNAMSIZ + 1] = "";
    struct rtattr           *pAttr = (struct rtattr *)((char*)(pIfInfo) + ifInfoLen);
    int                     attrLen = netlinkLen - ifInfoLen; //rtattr buffer length

    //RTA_ALIGN macro is used to round off
    //length to the nearest nibble boundary
    int                     rta_len = RTA_ALIGN(sizeof(struct rtattr));

    //RTA_OK macro checks to see if the given len is greater than 0, if the
    //length of the attribute is atleast the size of the struct rta and
    //if the length of the attribute is lesser than the argument len passed to it.
    while ( RTA_OK( pAttr, attrLen ) )
    {
        //we are only intersted in interface name
        if ( pAttr->rta_type ==  IFLA_IFNAME )
        {
          int n = pAttr->rta_len - rta_len;
          pAttr = (struct rtattr *)((char*)(pIfInfo) + ifInfoLen);
          if ((size_t) n > sizeof(ifname))
          {
            n = sizeof(ifname);
          }
          memcpy(ifname, ((char *) pAttr) + rta_len,n);
        }
        //move to next RTA message
        pAttr = RTA_NEXT( pAttr, attrLen );
    }
    log_debug( TAG, "%s: %s - ifname %s, flags 0x%x (%s%s)", __FUNCTION__,
               (pNetlinkHdr->nlmsg_type == RTM_NEWLINK ? "RTM_NEWLINK" : "RTM_DELLINK"),
               ifname, pIfInfo->ifi_flags,
               (pIfInfo->ifi_flags & IFF_UP) ? "[UP]" : "",
               (pIfInfo->ifi_flags & IFF_RUNNING) ? "[RUNNING]" : "" );

    //Return FALSE if not WLAN interface
    if (strncmp(ifname, mWifiInterfaceName, sizeof(mWifiInterfaceName)-1) != 0)
    {
      break;
    }
    // If NEWLINK, check for flags as well.
    if (pNetlinkHdr->nlmsg_type == RTM_DELLINK)
    {
      intfState = INTF_DOWN;
      retVal     = TRUE;
    }
    else
    {
      // Ignore NEW_LINK message if neither UP nor RUNNING is set
      pIfInfo->ifi_flags & IFF_RUNNING ? intfState = INTF_RUNNING, retVal = TRUE :
      (pIfInfo->ifi_flags & IFF_UP     ? intfState = INTF_UP     , retVal = TRUE :
       retVal = FALSE);
    }
  }
  while (0);
  return retVal;
}

LOWINetlinkSocketReceiver::eState LOWINetlinkSocketReceiver::getState() const
{
  return mState;
}
