/*
* Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 * Copyright (c) 2011, Atheros Communications
 *
 * Header file for P2P Protocol Module for Group Owner.
 *
 */
/*
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#ifndef _IEEE80211_P2P_PROT_GO_H
#define _IEEE80211_P2P_PROT_GO_H

static bool inline
wlan_p2p_prot_go_is_attach(wlan_p2p_go_t p2p_go_handle)
{
    return(p2p_go_handle->prot_info != NULL);
}

int wlan_p2p_prot_go_set_param(wlan_p2p_go_t p2p_go_handle, 
                               wlan_p2p_go_param_type param_type, u_int32_t param);

int
wlan_p2p_prot_go_set_param_array(wlan_p2p_go_t p2p_go_handle, wlan_p2p_go_param_type param,
                                 u_int8_t *byte_arr, u_int32_t len);

#endif  //_IEEE80211_P2P_PROT_GO_H
