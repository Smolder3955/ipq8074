/*
 * Copyright (c) 2016,2017 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#if UMAC_SUPPORT_CFG80211
#include "nl80211_copy.h"
#include "cfg80211_nlwrapper_api.h"
#include <qca_vendor.h>
#endif
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

typedef enum config_mode_type {
    CONFIG_IOCTL = 0,
    CONFIG_CFG80211 = 1,
    CONFIG_INVALID = 2,
} config_mode_type;

struct socket_context {
    u_int8_t cfg80211;
    union {
#if UMAC_SUPPORT_CFG80211
        wifi_cfg80211_context cfg80211_ctxt;
#endif        
        int sock_fd;
    } ctx;
};

/**
 * init_context : initialize the context
 * @cfg80211 :cfg flag
 * @sock_ctx: socket context
 * return 0 on success and -ve value on failure
 */
int init_context (u_int8_t cfg80211, struct socket_context *sock_ctx)
{
    int err = 0;
#if UMAC_SUPPORT_CFG80211
    if (cfg80211) {
        /* Reset the private sockets to 0 if the application does not pass
         * custom private event and command sockets. In this case, the default
         * port is used for netlink communication.
         */
        sock_ctx->ctx.cfg80211_ctxt.pvt_cmd_sock_id = 0;
        sock_ctx->ctx.cfg80211_ctxt.pvt_event_sock_id = 0;
        sock_ctx->cfg80211 = 1;

        err = wifi_init_nl80211(&(sock_ctx->ctx.cfg80211_ctxt));
        if (err) {
            errx(1, "unable to create NL socket");
            return -EIO;
        }
    } else
#endif
    {
        sock_ctx->cfg80211 = 0 /*false*/;
        sock_ctx->ctx.sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock_ctx->ctx.sock_fd < 0) {
            errx(1, "socket creation failed");
            return -EIO;
        }
    }
    return 0;
}


/**
 * destroy_context : initialize the context
 * @sock_ctx: socket context
 */
void destroy_context (struct socket_context *sock_ctx)
{
#if UMAC_SUPPORT_CFG80211
    if (sock_ctx->cfg80211) {
        wifi_destroy_nl80211(&(sock_ctx->ctx.cfg80211_ctxt));
    } else
#endif
    {
        close(sock_ctx->ctx.sock_fd);
    }
    return;
}


#if UMAC_SUPPORT_CFG80211
int start_event_thread (struct socket_context *sock_ctx)
{
    if (!sock_ctx->cfg80211) {
        return 0;
    }
    return wifi_nl80211_start_event_thread(&(sock_ctx->ctx.cfg80211_ctxt));
}
#endif


#define FILE_NAME_LENGTH 64
#define MAX_WIPHY 3
/*
* get_config_mode_type: Function that detects current config type
* and returns corresponding enum value.
*/
enum config_mode_type get_config_mode_type()
{
    FILE *fp;
    char filename[FILE_NAME_LENGTH];
    int radio;
    config_mode_type ret = CONFIG_IOCTL;

    for (radio = 0; radio < MAX_WIPHY; radio++) {
        snprintf(filename, sizeof(filename),"/sys/class/net/wifi%d/phy80211/",radio);
        fp = fopen(filename, "r");
        if (fp != NULL){
            ret = CONFIG_CFG80211;
            fclose(fp);
            break;
        }
    }

    return ret;
}
