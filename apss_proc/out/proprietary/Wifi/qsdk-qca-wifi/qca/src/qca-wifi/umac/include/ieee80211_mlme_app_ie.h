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

#ifndef _ATH_MLME_APP_IE_H
#define _ATH_MLME_APP_IE_H

struct app_ie_entry {

    bool                        inserted;       /* whether this entry is inserted into IE list */
    STAILQ_ENTRY(app_ie_entry)    link_entry;     /* link entry in IE list */

    struct ieee80211_app_ie_t   app_ie;
    u_int16_t                   ie_buf_maxlen;
    u_int8_t                    identifier;
};

bool ieee80211_mlme_app_ie_check_wpaie(struct ieee80211vap *vap,ieee80211_frame_type ftype);

u_int8_t *ieee80211_mlme_app_ie_append(
    struct ieee80211vap     *vap, 
    ieee80211_frame_type    ftype, 
    u_int8_t                *frm);

#if ATH_SUPPORT_DYNAMIC_VENDOR_IE
#define IEEE80211_VENDORIE_INCLUDE_IN_BEACON        0x10
#define IEEE80211_VENDORIE_INCLUDE_IN_ASSOC_REQ     0x01
#define IEEE80211_VENDORIE_INCLUDE_IN_ASSOC_RES     0x02
#define IEEE80211_VENDORIE_INCLUDE_IN_PROBE_REQ     0x04
#define IEEE80211_VENDORIE_INCLUDE_IN_PROBE_RES     0x08
#endif


#endif /* end of _ATH_MLME_APP_IE_H */
