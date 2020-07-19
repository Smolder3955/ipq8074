/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <sys/types.h>
#include <net/if.h>
#include <net/ethernet.h>

#include "wlanif_cmn.h"

struct wlanif_config_ops * wlanif_wext_get_ops();

#ifdef LIBCFG80211_SUPPORT
struct wlanif_config_ops * wlanif_cfg80211_get_ops();
#endif

/* Config init function to allocate and configure wext/cfg80211*/
struct wlanif_config * wlanif_config_init(enum wlanif_cfg_mode mode,
                                          int pvt_cmd_sock_id,
                                          int pvt_event_sock_id)
{
    struct wlanif_config * wlanif = NULL;

    wlanif = (struct wlanif_config *) malloc(sizeof(struct wlanif_config));
    if(!wlanif)
    {
        fprintf(stderr, "Error: %s malloc failed\n",__func__);
        return NULL;
    }
#ifndef LIBCFG80211_SUPPORT
    fprintf(stderr, "Error: %s Library not compiled with CFG80211 \n",__func__);
#endif

    memset(wlanif, 0, sizeof(struct wlanif_config));

    wlanif->ctx =NULL;

    switch (mode)
    {
#ifdef LIBCFG80211_SUPPORT
        case WLANIF_CFG80211:
            wlanif->ops = wlanif_cfg80211_get_ops();
            break;
#endif
        case WLANIF_WEXT:
            wlanif->ops = wlanif_wext_get_ops();
            break;
        default:
            goto err;
            break;
    }

    wlanif->pvt_cmd_sock_id = pvt_cmd_sock_id;
    wlanif->pvt_event_sock_id = pvt_event_sock_id;

    if ( wlanif->ops->init((wlanif)))
    {
        fprintf(stderr, "WLAN init ops failed\n");
        goto err;
    }
    return wlanif;
err:
    free(wlanif);
    return NULL;
}

/*Config deinit function*/
void wlanif_config_deinit(struct wlanif_config * wlanif)
{
    if(!wlanif) {
        return;
    }

    wlanif->ops->deinit(wlanif);
    wlanif->ctx =NULL;
    wlanif->ops = NULL;
    free(wlanif);
}
