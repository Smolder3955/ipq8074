/*
* Copyright (c) 2017 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef _CFG80211_CMN_H
#define _CFG80211_CMN_H

#ifdef CFG80211_ENABLE
#include "cfg80211_nlwrapper_api.h"
#endif

enum qca__nl80211_vendor_subcmds {
    QCA_NL80211_VENDOR_SUBCMD_GET_PARAMS = 201,
    QCA_NL80211_VENDOR_SUBCMD_EXTENDEDSTATS = 222,
    QCA_NL80211_VENDOR_SUBCMD_CLONEPARAMS = 225,
};

const struct init_func_pointers *driver;

struct sock_context {
#ifdef CFG80211_ENABLE
        wifi_cfg80211_context cfg80211_ctxt;
#endif
        int sock_fd;
};

struct init_func_pointers {
    int (*send_command)(const char *ifname, void *buf, unsigned int buflen,
        int cmd, int ioctl_cmd, struct sock_context *sock);
    int (*create_vap)(const char *ifname, void *buf, unsigned int buflen,
        int cmd, int ioctl_cmd, struct sock_context *sock, int ifnum1, int ifnum2);
    int (*delete_vap)(const char *ifname, void *buf, unsigned int buflen,
        int cmd, int ioctl_cmd, struct sock_context *sock, int ifnum1, int ifnum2);
    int (*send_command_get)(const char *ifname, int op, int *data, int ioctl_cmd,
        struct sock_context *sock);
    char *(*get_driver_type)();
    char *(*get_tool_type)();
    int (*init_context)(struct sock_context *sock);
    void (*destroy_context)(struct sock_context *sock);
};

#endif /*_CFG80211_CMN_H */
