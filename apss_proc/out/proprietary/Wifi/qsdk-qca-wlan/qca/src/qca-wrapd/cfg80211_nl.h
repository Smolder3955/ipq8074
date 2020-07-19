/*
* Copyright (c) 2017 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef _CFG80211_NL_H
#define _CFG80211_NL_H

#include "cfg80211_cmn.h"


int send_command_cfg80211 (const char *ifname, void *buf, unsigned int buflen,
        int cmd,int ioctl_cmd, struct sock_context *sock);
int create_vap_cfg80211 (const char *ifname, void *buf, unsigned int buflen,
        int cmd,int ioctl_cmd, struct sock_context *sock, int ifnum1, int ifnum2);
int delete_vap_cfg80211 (const char *ifname, void *buf, unsigned int buflen,
        int cmd,int ioctl_cmd, struct sock_context *sock, int ifnum1, int ifnum2);
int send_command_get_cfg80211(const char *ifname, int op, int *data,
        int ioctl_cmd, struct sock_context *sock);
char *get_driver_type_cfg80211();
char *get_tool_type_cfg80211();
int init_context_cfg80211 (struct sock_context *sock);
void destroy_context_cfg80211 (struct sock_context *sock);

#define QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_CONFIGURATION 74
#define QCA_NL80211_VENDOR_SUBCMD_PARAMS 200

#endif /*_CFG80211_NL_H*/
