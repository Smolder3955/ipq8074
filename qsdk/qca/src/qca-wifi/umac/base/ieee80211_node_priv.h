/*
 * Copyright (c) 2011-2016,2017-2018 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * Copyright (c) 2010, Atheros Communications Inc.
 * All Rights Reserved.
 * 
 * 2011-2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 */

#ifndef _IEEE80211_NODE_PRIV_H

#define _IEEE80211_NODE_PRIV_H

#include <osdep.h>
#include <ieee80211_var.h>
#include <ieee80211_channel.h>
#include <ieee80211_rateset.h>
#include <ieee80211_regdmn.h>
#include <ieee80211_target.h>
#include <ieee80211_objmgr_priv.h>

/*
 * Association id's are managed with a bit vector.
 */
#define	IEEE80211_AID_SET(_vap, _b) ({		       \
    int nbits = sizeof(unsigned long) * BITS_PER_BYTE; \
    qdf_atomic_set_bit((IEEE80211_AID(_b) % nbits), \
		       &(_vap)->iv_aid_bitmap[IEEE80211_AID(_b) / nbits]); \
    })

#define	IEEE80211_AID_CLR(_vap, _b) ({		       \
    int nbits = sizeof(unsigned long) * BITS_PER_BYTE; \
    qdf_atomic_clear_bit((IEEE80211_AID(_b) % nbits), \
			 &(_vap)->iv_aid_bitmap[IEEE80211_AID(_b) / nbits]); \
    })

#define	IEEE80211_AID_ISSET(_vap, _b) ({	       \
    int nbits = sizeof(unsigned long) * BITS_PER_BYTE; \
    qdf_atomic_test_bit((IEEE80211_AID(_b) % nbits), \
		      &(_vap)->iv_aid_bitmap[IEEE80211_AID(_b) / nbits]); \
    })

#define IEEE80211_RESV_AID_BITS 0xc000

int ieee80211_setup_node( struct ieee80211_node *ni, ieee80211_scan_entry_t scan_entry );
int ieee80211_setup_node_rsn( struct ieee80211_node *ni, ieee80211_scan_entry_t scan_entry );

#endif
