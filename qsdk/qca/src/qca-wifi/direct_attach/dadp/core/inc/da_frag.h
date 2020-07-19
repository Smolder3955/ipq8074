/*
 * Copyright (c) 2011,2017-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * Copyright (c) 2005 Atheros Communications Inc.  All rights reserved.
 */

#ifndef _IEEE80211_TXRX_PRIV_H
#define _IEEE80211_TXRX_PRIV_H

#include <osdep.h>
#include <if_upperproto.h>
#include <ieee80211_var.h>
#include <ieee80211_channel.h>
#include <ieee80211_rateset.h>
#include <ieee80211_wds.h>

#if UMAC_SUPPORT_TX_FRAG
int ieee80211_fragment(struct wlan_objmgr_vdev *vdev, wbuf_t wbuf0,
		       u_int hdrsize, u_int ciphdrsize, u_int mtu);
/*
 * checkand if required fragment the frame.
 */
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
bool ieee80211_check_and_fragment(struct wlan_objmgr_vdev *vdev, wbuf_t wbuf,
				  struct ieee80211_frame *wh, int usecrypto,
				  struct wlan_crypto_key *key, u_int hdrsize);
#else
bool ieee80211_check_and_fragment(struct wlan_objmgr_vdev *vdev, wbuf_t wbuf,
				  struct ieee80211_frame *wh, int usecrypto,
				  struct ieee80211_key *key, u_int hdrsize);
#endif
#else /* UMAC_SUPPORT_TX_FRAG */

#define ieee80211_check_and_fragment(vdev,wbuf,wh,usecrypto,key,hdrsize)  true

#endif /* UMAC_SUPPORT_TX_FRAG */


wbuf_t ieee80211_defrag(struct wlan_objmgr_peer *peer, wbuf_t wbuf, int hdrlen);

#endif /* TXRX_PRIV_H */
