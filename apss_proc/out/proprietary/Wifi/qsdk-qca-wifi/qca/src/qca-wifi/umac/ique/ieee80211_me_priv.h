/*
* Copyright (c) 2013, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 * Copyright (c) 2010, Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/*
 * 2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#if ATH_SUPPORT_IQUE

#ifndef IEEE80211_ME_PRIV_H
#define IEEE80211_ME_PRIV_H

#include <osdep.h>
#include <ieee80211_var.h>

/*
 * If Linux can't tell us if irqs are disabled, then they are not.
 */ 
#ifndef irqs_disabled
#define irqs_disabled()     (0)
#endif

/*
 * Table locking definitions.
 */
#define	IEEE80211_SNOOP_LOCK_INIT(_st, _name)	\
	OS_RWLOCK_INIT(&((_st)->msl_lock))
#define	IEEE80211_SNOOP_LOCK_DESTROY(_st)       \
    OS_RWLOCK_DESTROY(_st)
#define	IEEE80211_SNOOP_LOCK(_st, _state)				\
	OS_RWLOCK_WRITE_LOCK(&((_st)->msl_lock), _state)
#define	IEEE80211_SNOOP_UNLOCK(_st, _state)				\
	OS_RWLOCK_WRITE_UNLOCK(&((_st)->msl_lock), _state)
#define	IEEE80211_SNOOP_LOCK_BH(_st, _state)			\
	if (irqs_disabled()) {						\
		OS_RWLOCK_WRITE_LOCK(&((_st)->msl_lock), _state);	\
	} else {									\
		OS_RWLOCK_WRITE_LOCK_BH(&((_st)->msl_lock), _state); \
	}
#define	IEEE80211_SNOOP_UNLOCK_BH(_st, _state)			\
	if (irqs_disabled()) {						\
		OS_RWLOCK_WRITE_UNLOCK(&((_st)->msl_lock), _state);	\
	} else {									\
		OS_RWLOCK_WRITE_UNLOCK_BH(&((_st)->msl_lock), _state);	\
	}

#define	IEEE80211_ME_LOCK_INIT(_vap)    spin_lock_init(&(_vap)->iv_me->ieee80211_melock)
#define	IEEE80211_ME_LOCK_DESTROY(_vap)
#define	IEEE80211_ME_LOCK(_vap)         spin_lock(&(_vap)->iv_me->ieee80211_melock)
#define	IEEE80211_ME_UNLOCK(_vap)       spin_unlock(&(_vap)->iv_me->ieee80211_melock)



#define IGMP_SNOOP_CMD_OTHER 0
#define IGMP_SNOOP_CMD_JOIN  1
#define IGMP_SNOOP_CMD_LEAVE 2

#define IQUE_ME_MEMTAG 'metag'
#define IQUE_ME_OPS_MEMTAG 'meopstag'



/* debug levels */
#define IEEE80211_ME_DBG_NONE       0
#define IEEE80211_ME_DBG_INFO       1
#define IEEE80211_ME_DBG_DEBUG      2
#define IEEE80211_ME_DBG_DUMP       4
#define IEEE80211_ME_DBG_ALL        8

#define BITFIELD_LITTLE_ENDIAN      0
#define BITFIELD_BIG_ENDIAN         1

#endif
#endif /* ATH_SUPPORT_IQUE */
