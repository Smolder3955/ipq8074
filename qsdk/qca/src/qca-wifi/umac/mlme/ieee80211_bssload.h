/*
 *  Copyright (c) 2008 Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */

#ifndef _IEEE80211_BSSLOAD_H
#define _IEEE80211_BSSLOAD_H

#if UMAC_SUPPORT_QBSSLOAD || UMAC_SUPPORT_XBSSLOAD || UMAC_SUPPORT_CHANUTIL_MEASUREMENT
void
ieee80211_beacon_chanutil_update(struct ieee80211vap *vap);
#else
#define ieee80211_beacon_chanutil_update(vap)
#endif /* UMAC_SUPPORT_QBSSLOAD || UMAC_SUPPORT_XBSSLOAD || UMAC_SUPPORT_CHANUTIL_MEASUREMENT */

#if UMAC_SUPPORT_QBSSLOAD

u_int8_t *ieee80211_add_qbssload(u_int8_t *frm, struct ieee80211_node *ni);

u_int8_t *ieee80211_qbssload_beacon_setup(struct ieee80211vap *vap,
                                          struct ieee80211_node *ni,
                                          struct ieee80211_beacon_offsets *bo,
                                          u_int8_t *frm);

void ieee80211_qbssload_beacon_update(struct ieee80211vap *vap,
                                      struct ieee80211_node *ni,
                                      struct ieee80211_beacon_offsets *bo);

#else /* UMAC_SUPPORT_QBSSLOAD */

#define ieee80211_add_qbssload(frm, ni)                      (frm)
#define ieee80211_qbssload_beacon_setup(vap, ni, bo, frm)    (frm)
#define ieee80211_qbssload_beacon_update(vap, ni, bssload)   /**/

#endif  /* UMAC_SUPPORT_QBSSLOAD */

#endif /* end of _IEEE80211_BSSLOAD_H */

#if UMAC_SUPPORT_XBSSLOAD
u_int8_t *
ieee80211_add_ext_bssload(u_int8_t *frm, struct ieee80211_node *ni);

u_int8_t *
ieee80211_ext_bssload_beacon_setup(struct ieee80211vap *vap, struct ieee80211_node *ni,
                                   struct ieee80211_beacon_offsets *bo,
                                   u_int8_t *frm);

void ieee80211_ext_bssload_beacon_update(struct ieee80211vap *vap,
                                         struct ieee80211_node *ni,
                                         struct ieee80211_beacon_offsets *bo);

#else /* UMAC_SUPPORT_XBSSLOAD */
#define ieee80211_add_ext_bssload(frm, ni)                      (frm)
#define ieee80211_ext_bssload_beacon_setup(vap, ni, bo, frm)    (frm)
#define ieee80211_ext_bssload_beacon_update(vap, ni, bssload)   /**/

#endif /* UMAC_SUPPORT_XBSSLOAD */

