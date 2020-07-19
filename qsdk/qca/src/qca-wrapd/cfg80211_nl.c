/*
* Copyright (c) 2017 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include <linux/types.h>
#include <sys/ioctl.h>
#include <sys/un.h>
#include "includes.h"
#include "common.h"
#include "linux_ioctl.h"
#include "wpa_ctrl.h"
#include "ieee802_11_defs.h"
#include "linux_wext.h"
#include "eloop.h"
#include "wrapd_api.h"
#include "cfg80211_nl.h"
#include "nl80211_copy.h"
#include "cfg80211_nlwrapper_api.h"
#include "cfg80211_nlwrapper_pvt.h"
#include "cfg80211_ioctl.h"

#define MAX_CMD_LEN 128
static const unsigned NL80211_ATTR_MAX_INTERNAL = 256;

const struct init_func_pointers cfg80211_initialize = {
    .send_command = send_command_cfg80211,
    .create_vap = create_vap_cfg80211,
    .delete_vap = delete_vap_cfg80211,
    .send_command_get = send_command_get_cfg80211,
    .init_context = init_context_cfg80211,
    .destroy_context = destroy_context_cfg80211,
    .get_driver_type = get_driver_type_cfg80211,
    .get_tool_type = get_tool_type_cfg80211,
};

/*sending cfg80211 command nl_send command is used to send command*/
int send_command_cfg80211 (const char *ifname, void *buf, unsigned int buflen,
        int cmd, int ioctl_cmd, struct sock_context *sock)
{
    int msg=0;
    struct cfg80211_data buffer;
    buffer.data = buf;
    buffer.length = buflen;
    buffer.callback = NULL;
    msg = wifi_cfg80211_send_generic_command(&(sock->cfg80211_ctxt),
            QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_CONFIGURATION,
            cmd, ifname, (char *)&buffer, buflen);
    if (msg < 0) {
        fprintf(stderr,"Couldn't send NL command\n");
        return -1;
    }
    return 0;
}

/*cfg80211 command for creating vap*/
int create_vap_cfg80211 (const char *ifname, void *buf, unsigned int buflen, int cmd,
        int ioctl_cmd, struct sock_context *sock, int ifnum1, int ifnum2)
{
    int res, ret, err;
    char command[MAX_CMD_LEN] = {0};
    res = send_command_cfg80211 (ifname, buf, buflen, cmd, ioctl_cmd, sock);
    if(res != 0 )
    {
        fprintf(stderr,"Not able to send clone_params");
        return -1;
    }
    ret = os_snprintf(command, sizeof(command),
	"iw dev wifi%d interface add ath%d%d type managed",
	ifnum1, ifnum1, ifnum2);
    if(ret < 0 ){
        fprintf(stderr, "os_snprintf err");
        return -1;
    }
    err = system(command);
    if(err != 0)
    {
        fprintf(stderr, "Err: Cannot create vap\n");
        return -1;
    }
    return 0;
}

/*cfg80211 command for deleting a vap*/
int delete_vap_cfg80211 (const char *ifname, void *buf, unsigned int buflen, int cmd,
        int ioctl_cmd, struct sock_context *sock, int ifnum1, int ifnum2)
{
    int ret, err;
    char command[MAX_CMD_LEN] = {0};
    ret = os_snprintf(command, sizeof(command), "iw ath%d%d del", ifnum1, ifnum2);
    if(ret < 0 ){
        fprintf(stderr, "os_snprintf err");
        return -1;
    }
    err = system(command);
    if(err != 0)
    {
        fprintf(stderr, "Err: Cannot delete vap");
        return -1;
    }
    return 0;
}

/*cfg80211 command to get param from driver*/
int send_command_get_cfg80211(const char *ifname, int op, int *data,
        int ioctl_cmd, struct sock_context *sock)
{

    int msg=0;
    u_int32_t value = 0;
    struct cfg80211_data buffer;
    buffer.data = &value;
    buffer.length = sizeof(u_int32_t);
    buffer.parse_data = 0;
    buffer.callback = NULL;
    msg = wifi_cfg80211_send_getparam_command(&(sock->cfg80211_ctxt),
            QCA_NL80211_VENDOR_SUBCMD_PARAMS, op, ifname, (char *)&buffer,
            sizeof(uint32_t));
    if (msg < 0) {
        fprintf(stderr,"Couldn't send NL command\n");
        return -1;
    }
    *data = value;
    return 0;
}

/*function to return nl80211 type*/
char *get_driver_type_cfg80211()
{
    return "nl80211";
}


/*function to return cfg80211tool*/
char *get_tool_type_cfg80211()
{
    return "cfg80211tool";
}

/*initializing cfg80211 socket context */
int init_context_cfg80211 (struct sock_context *sock)
{
    int err = 0;
    os_memset(sock, 0, sizeof(struct sock_context));
    err = wifi_init_nl80211(&(sock->cfg80211_ctxt));
    if (err) {
        fprintf(stderr, "unable to create NL socket\n");
        return -EIO;
    }
    return 0;
}

/*destroying cfg80211 socket context */
void destroy_context_cfg80211 (struct sock_context *sock)
{
    wifi_destroy_nl80211(&(sock->cfg80211_ctxt));
}
