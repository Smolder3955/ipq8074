/*
* Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 *  Copyright (c) 2009 Atheros Communications Inc.  All rights reserved.
 */
/*
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */


#ifndef _IEEE80211_P2P_GO_H_
#define _IEEE80211_P2P_GO_H_

/*
 * Inform the P2P GO that there is a new P2P sub-attributes (excluding the NOA subattribute).
 * p2p_sub_ie will contains the new P2P sub-attributes.
 */
int wlan_p2p_go_update_p2p_ie(
    wlan_p2p_go_t   p2p_go_handle,
    u_int8_t        *p2p_sub_ie,
    u_int16_t       p2p_sub_ie_len);

/*
 * Routine to inform the P2P GO that there is a new NOA IE.
 * @param p2p_go_handle : handle to the p2p object .
 * @param new_noa_ie    : pointer to new NOA IE schedule to update.
 *                        If new_noa_ie is NULL, then disable the append of NOA IE.
 */
void wlan_p2p_go_update_noa_ie(
    wlan_p2p_go_t                           p2p_go_handle,
    struct ieee80211_p2p_sub_element_noa    *new_noa_ie,
    u_int32_t                               tsf_offset);

/*
 * This routine will store the new ProbeResp P2P IE.
 */
void ieee80211_p2p_go_store_proberesp_p2p_ie(
    wlan_p2p_go_t p2p_go_handle, u8 *p2p_ie_ptr, u_int16_t p2p_ie_len);

#endif  //_IEEE80211_P2P_GO_H_
