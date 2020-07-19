/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*
GENERAL DESCRIPTION
   This file contains functions used for p2p rtt ranging

Copyright (c) 2014, 2016 Qualcomm Technologies, Inc.
 All Rights Reserved.
 Confidential and Proprietary - Qualcomm Technologies, Inc.

=============================================================================*/
#include <base_util/log.h>
#include "rttm.h"
#include "lowi_p2p_ranging.h"
#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "LOWI-P2P-RTT"

using namespace qc_loc_fw;

/* stores p2p status info */
#define NUM_P2P_PEER_INFO_STORAGE 32
static tANI_U16 p2pPeersConnectedCnt = 0;
static ani_peer_status_info* peerInfoTable[NUM_P2P_PEER_INFO_STORAGE];
static bool tableAlreadyInitialized = false;

/* p2p peer connection status */
typedef enum p2p_peer_status_s {
  P2P_PEER_STATUS_CONNECTED = 1,
  P2P_PEER_STATUS_DISCONNECTED = 2,
  P2P_PEER_STATUS_UNDEF = 3
} p2p_peer_status_t;

/* msg strings used for printing/debugging */
const char* PEER_CONN_STATUS[P2P_PEER_STATUS_UNDEF+1] = {
  " ",
  "P2P_PEER_STATUS_CONNECTED",
  "P2P_PEER_STATUS_DISCONNECTED",
  "P2P_PEER_STATUS_UNKNOWN"
};

/* RTT capability types */
typedef enum p2p_peer_cap_s {
   P2P_PEER_CAP_RTT2 = 0,
   P2P_PEER_CAP_RTT3 = 1,
   P2P_PEER_CAP_UNDEF = 2
} p2p_peer_cap_t;

/* msg strings used for printing/debugging */
const char* PEER_RTT_CAP[P2P_PEER_CAP_UNDEF+1] = {
  "P2P_PEER_CAP_RTT2",
  "P2P_PEER_CAP_RTT3",
  "P2P_PEER_CAP_UNDEF"
};

static void p2pShowPeers(void);
static void p2pGetChanInfoFromTable(wmi_channel* channelInfoPtr, tANI_U16 k);
static void p2pAddPeer(ani_peer_status_info* newPeer);
static inline void p2pInitPeerTable();

/*=============================================================================================
 * p2pStoreStatusInfo
 *
 * Description:
 *   Stores p2p peer status information when p2p status event occurs
 *
 * Parameters:
 *   rxBuff - ptr to msg containing peer info
 *
 * Return value:
 *   -1: error, 0: ok
 =============================================================================================*/
tANI_U8 p2pStoreStatusInfo(char* rxBuff)
{
  tANI_U16 k;
  ani_peer_status_info* pInfo;

  p2pInitPeerTable();

  /* extract peer info */
  pInfo = (ani_peer_status_info*)(rxBuff + sizeof(tAniMsgHdr));

  /* check MAC address to see if peer is already stored */
  if( p2pIsStored(pInfo->peer_mac_addr, &k) )
  {
    /* it is stored, now check status and remove if status is disconnect */
    if( P2P_PEER_STATUS_DISCONNECTED == pInfo->peer_conn_status )
    {
      /* remove peer from table */
      log_debug(LOG_TAG, "%s: removing p2p peer - %02x:%02x:%02x:%02x:%02x:%02x\n",
                   __func__,
                   peerInfoTable[k]->peer_mac_addr[0],
                   peerInfoTable[k]->peer_mac_addr[1],
                   peerInfoTable[k]->peer_mac_addr[2],
                   peerInfoTable[k]->peer_mac_addr[3],
                   peerInfoTable[k]->peer_mac_addr[4],
                   peerInfoTable[k]->peer_mac_addr[5]);

      free(peerInfoTable[k]);
      peerInfoTable[k] = NULL;
      p2pPeersConnectedCnt--;
    }
  }
  else
  {
    /* new peer, let's make sure there is room to store it */
    if( p2pPeersConnectedCnt >= NUM_P2P_PEER_INFO_STORAGE )
    {
      log_warning(LOG_TAG, "%s: no room to store p2p peer - %02x:%02x:%02x:%02x:%02x:%02x\n",
                   __func__,
                   pInfo->peer_mac_addr[0],
                   pInfo->peer_mac_addr[1],
                   pInfo->peer_mac_addr[2],
                   pInfo->peer_mac_addr[3],
                   pInfo->peer_mac_addr[4],
                   pInfo->peer_mac_addr[5]);
      return -1;
    }

    /* create mem for the new peer */
    ani_peer_status_info* newPeer = (ani_peer_status_info*)malloc(sizeof(ani_peer_status_info));
    if(NULL == newPeer)
    {
      log_error(LOG_TAG, "%s: memory allocation failure - (%u) bytes",
                __func__, sizeof(ani_peer_status_info));
      return -1;
    }
    memcpy(newPeer, pInfo, sizeof(ani_peer_status_info));
    /* store peer in table */
    p2pAddPeer(newPeer);
  }

  /* show current list of peers */
  p2pShowPeers();

  return 0;
}
/* p2pStoreStatusInfo */

/*=============================================================================================
 * p2pGetNumPeersConnected
 *
 * Description:
 *   Returns the number of currently connected p2p peers
 *
 * Parameters:
 *   none
 * Return value:
 *   number of currently connected p2p peers
 =============================================================================================*/
tANI_U16 p2pGetNumPeersConnected(void)
{
  return p2pPeersConnectedCnt;
}
/* p2pGetNumPeersConnected */

/*=============================================================================================
 * p2pBssidDetected
 *
 * Description:
 *   checks whether mac address of peer is already stored
 *
 * Parameters:
 *   numBssids - number of BSSIDs to check
 *   bssidsToScan - array of BSSIDs to check
 *   channelInfoPtr - ptr to channel information used for RTT request
 *   vDev - ptr to vdev ID to be used in the RTT request
 * Return value:
 *   FALSE: peer not stored, TRUE: peer is stored in table
 =============================================================================================*/
boolean p2pBssidDetected(tANI_U32 numBssids, DestInfo* bssidsToScan, wmi_channel* channelInfoPtr, tANI_U8* vDev)
{
  tANI_U32 i;
  tANI_U16 k;

  p2pInitPeerTable();

  for(i = 0; i < numBssids; i++)
  {
    if( p2pIsStored(bssidsToScan[i].mac, &k) )
    {
      p2pGetChanInfoFromTable(channelInfoPtr, k);
      *vDev = peerInfoTable[k]->vdev_id;
      log_debug(LOG_TAG, "%s: p2p peer detected - %02x:%02x:%02x:%02x:%02x:%02x, vdev_id - %u\n",
                   __func__,
                   peerInfoTable[k]->peer_mac_addr[0],
                   peerInfoTable[k]->peer_mac_addr[1],
                   peerInfoTable[k]->peer_mac_addr[2],
                   peerInfoTable[k]->peer_mac_addr[3],
                   peerInfoTable[k]->peer_mac_addr[4],
                   peerInfoTable[k]->peer_mac_addr[5],
                   *vDev);
      return TRUE;
    }
    else {
      log_debug(LOG_TAG, "%s: NO p2p peer detected - %02x:%02x:%02x:%02x:%02x:%02x\n",
                   __func__,
                   bssidsToScan[i].mac[0],
                   bssidsToScan[i].mac[1],
                   bssidsToScan[i].mac[2],
                   bssidsToScan[i].mac[3],
                   bssidsToScan[i].mac[4],
                   bssidsToScan[i].mac[5]);
    }
  }

  return FALSE;
}
/* p2pBssidDetected */

/*=============================================================================================
 * p2pSameBSSID
 *
 * Description:
 *   Utility function that checks whether two mac addresses are the same or not.
 *
 * Parameters:
 *   addr1 - Mac address #1
 *   addr2 - Mac address #2
 *
 * Return value:
 *   TRUE if addr1 equals addr2. FALSE, otherwise.
 =============================================================================================*/
static boolean p2pSameBSSID(const uint8* addr1, const uint8* addr2)
{
  p2pInitPeerTable();

  if ( (NULL == addr1) || (NULL == addr2) )
  {
    log_debug(LOG_TAG, "%s: Bad inputs\n", __func__);
    return FALSE;
  }

  for (int i = 0; i < WIFI_MAC_ID_SIZE; i++)
  {
    if (addr1[i] != addr2[i])
    {
      return FALSE;
    }
  }
  return TRUE;
}
/* p2pSameBSSID */

/*=============================================================================================
 * p2pShowPeers
 *
 * Description:
 *   Displays p2p peer status information of all peers currently connected
 *
 * Parameters:
 *   none
 * Return value:
 *   none
 =============================================================================================*/
static void p2pShowPeers(void)
{
  for(int j = 0; j < NUM_P2P_PEER_INFO_STORAGE; j++)
  {
    /* prints all non-NULL entries */
    if(NULL != peerInfoTable[j])
    {
      log_debug(LOG_TAG, "\nP2P BSSID   = %02x:%02x:%02x:%02x:%02x:%02x\n",
                   peerInfoTable[j]->peer_mac_addr[0],
                   peerInfoTable[j]->peer_mac_addr[1],
                   peerInfoTable[j]->peer_mac_addr[2],
                   peerInfoTable[j]->peer_mac_addr[3],
                   peerInfoTable[j]->peer_mac_addr[4],
                   peerInfoTable[j]->peer_mac_addr[5]);

      log_debug(LOG_TAG, "p2p channel = %lu", peerInfoTable[j]->peer_chan_info.channel_info.mhz);
      log_debug(LOG_TAG, "p2p status  = %s", PEER_CONN_STATUS[peerInfoTable[j]->peer_conn_status]);
      log_debug(LOG_TAG, "p2p rtt_cap = %s", PEER_RTT_CAP[peerInfoTable[j]->peer_rtt_cap]);
      log_debug(LOG_TAG, "p2p vdev_id = %u", peerInfoTable[j]->vdev_id);
      log_debug(LOG_TAG, "******************************************\n");
    }
  }
}
/* p2pShowPeers */

/*=============================================================================================
 * p2pGetChanInfoFromTable
 *
 * Description:
 *   copies channel information for p2p peer into struct to be used in rtt request
 *
 * Parameters:
 *   channelInfoPtr - ptr to channel information for rtt request
 *   k - index where peer is stored in peer info table
 *
 * Return value:
 *   none
 =============================================================================================*/
static void p2pGetChanInfoFromTable(wmi_channel* channelInfoPtr, tANI_U16 k)
{
    memcpy(channelInfoPtr, &peerInfoTable[k]->peer_chan_info.channel_info, sizeof(wmi_channel));
}
/* p2pGetChanInfoFromTable */

/*=============================================================================================
 * p2pAddPeer
 *
 * Description:
 *   add peer to peer status info table
 *
 * Parameters:
 *   newPeer - ptr to peer status info to be added
 *
 * Return value:
 *   none
 =============================================================================================*/
static void p2pAddPeer(ani_peer_status_info* newPeer)
{
  log_debug(LOG_TAG, "%s: adding p2p peer - %02x:%02x:%02x:%02x:%02x:%02x\n",
               __func__,
               newPeer->peer_mac_addr[0],
               newPeer->peer_mac_addr[1],
               newPeer->peer_mac_addr[2],
               newPeer->peer_mac_addr[3],
               newPeer->peer_mac_addr[4],
               newPeer->peer_mac_addr[5]);

  for(int j=0; j<NUM_P2P_PEER_INFO_STORAGE; j++)
  {
    /* look for the first NULL entry in table and put it there */
    if(NULL == peerInfoTable[j])
    {
      peerInfoTable[j] = newPeer;
      p2pPeersConnectedCnt++;
      break;
    }
  }
}
/* p2pAddPeer */

/*=============================================================================================
 * p2pIsStored
 *
 * Description:
 *   checks whether mac address of peer is already stored.
 *
 * Parameters:
 *   addr1 - Mac address #1
 *   k - placeholder for index where peer is stored to be passed to caller
 * Return value:
 *   FALSE: peer not stored, TRUE: peer is stored in table array
 =============================================================================================*/
boolean p2pIsStored(tANI_U8* addr1, tANI_U16* k)
{
  p2pInitPeerTable();

  if ( (NULL == addr1) || (NULL == k) )
  {
    log_debug(LOG_TAG, "%s: Bad inputs\n", __func__);
    return FALSE;
  }

  for(int i=0; i<NUM_P2P_PEER_INFO_STORAGE; i++)
  {
    if( peerInfoTable[i] != NULL )
    {
      if( p2pSameBSSID(addr1, peerInfoTable[i]->peer_mac_addr) )
      {
        *k = i;
        return(TRUE);
      }
    }
  }
  return FALSE;
}
/* p2pIsStored */


/*=============================================================================================
 * p2pInitPeerTable
 *
 * Description:
 *   initialize peer info table
 *
 * Parameters:
 *   none
 *
 * Return value:
 *   none
 =============================================================================================*/
static void p2pInitPeerTable()
{
  if (tableAlreadyInitialized == false)
  {
    for (uint32 ii=0; ii<NUM_P2P_PEER_INFO_STORAGE; ii++)
    {
      peerInfoTable[ii] = NULL;
    }
    tableAlreadyInitialized = true;
  }
}
/* p2pAddPeer */
