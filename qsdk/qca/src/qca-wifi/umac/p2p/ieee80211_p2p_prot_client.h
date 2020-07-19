/*
* Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 * Copyright (c) 2011, Atheros Communications
 *
 * Header file for P2P Protocol Module for Client.
 *
 */
/*
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#ifndef _IEEE80211_P2P_PROT_CLIENT_H
#define _IEEE80211_P2P_PROT_CLIENT_H

static bool inline
wlan_p2p_prot_client_is_attach(wlan_p2p_client_t p2p_client_handle)
{
    return(p2p_client_handle->prot_info != NULL);
}

#endif  //_IEEE80211_P2P_PROT_CLIENT_H

