/*
* Copyright (c) 2017 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef __CFG80211_IOCTL_H
#define __CFG80211_IOCTL_H

#include "cfg80211_cmn.h"

int send_command_ioctl (const char *ifname, void *buf, unsigned int buflen,
        int cmd,int ioctl_cmd, struct sock_context *sock);
int create_vap_ioctl (const char *ifname, void *buf, unsigned int buflen,
        int cmd,int ioctl_cmd, struct sock_context *sock, int ifnum1, int ifnum2);
int delete_vap_ioctl (const char *ifname, void *buf, unsigned int buflen,
        int cmd, int ioctl_cmd, struct sock_context *sock, int ifnum1, int ifnum2);
int send_command_get_ioctl(const char *ifname, int op, int *data,
        int ioctl_cmd, struct sock_context *sock);
char *get_driver_type_ioctl();
char *get_tool_type_ioctl();
int init_context_ioctl (struct sock_context *sock);
void destroy_context_ioctl (struct sock_context *sock);

#endif /* _CFG80211_IOCTL_H */
