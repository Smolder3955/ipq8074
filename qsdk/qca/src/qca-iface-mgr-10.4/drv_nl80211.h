/*
* Copyright (c) 2018 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

/*
 * Driver interaction with Linux nl80211/cfg80211
 * Copyright (c) 2002-2015, Jouni Malinen <j@w1.fi>
 * Copyright (c) 2003-2004, Instant802 Networks, Inc.
 * Copyright (c) 2005-2006, Devicescape Software, Inc.
 * Copyright (c) 2007, Johannes Berg <johannes@sipsolutions.net>
 * Copyright (c) 2009-2010, Atheros Communications
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#if IFACE_SUPPORT_CFG80211

#ifndef _DRV_NL80211__H
#define _DRV_NL80211__H

#define nlk_handle nl_sock
#define nlk80211_handle_alloc nl_socket_alloc_cb
#define nlk80211_handle_destroy nl_socket_free

struct nlk80211_global {
    struct nl_cb *nlk_cb;
    struct nlk_handle *nlk;
    int nlk80211_id;
    struct nlk_handle *nlk_event;
    void *ctx;
    void (*do_process_driver_event)(void *arg, void *src_addr, void *frame, uint32_t frame_len, int ifidx);
};

struct family_cnt {
        const char *group;
        int id;
};

#if __WORDSIZE == 64
#define ELOOP_SOCKET_INVALID    (intptr_t) 0x8888888888888889ULL
#else
#define ELOOP_SOCKET_INVALID    (intptr_t) 0x88888889ULL
#endif

int nlk80211_create_socket(struct nlk80211_global *g_nl);
void nlk80211_close_socket(struct nlk80211_global *g_nl);
#endif //_DRV_NL80211__H
#endif //IFACE_SUPPORT_CFG80211
