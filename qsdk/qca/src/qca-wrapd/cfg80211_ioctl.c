/*
* Copyright (c) 2017 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/types.h>
#include <sys/un.h>
#include <linux/types.h>
#include "includes.h"
#include "common.h"
#include "linux_ioctl.h"
#include "wpa_ctrl.h"
#include "ieee802_11_defs.h"
#include "linux_wext.h"
#include "eloop.h"
#include "netlink.h"
#include "priv_netlink.h"
#include "wrapd_api.h"
#include "cfg80211_ioctl.h"

const struct init_func_pointers ioctl_initialize = {
    .send_command = send_command_ioctl,
    .create_vap = create_vap_ioctl,
    .delete_vap = delete_vap_ioctl,
    .send_command_get = send_command_get_ioctl,
    .init_context = init_context_ioctl,
    .destroy_context = destroy_context_ioctl,
    .get_driver_type = get_driver_type_ioctl,
    .get_tool_type = get_tool_type_ioctl,
};

/*Function to copy data pointer to ifr structure and
send ioctl to driver */
int send_command_ioctl(const char *ifname, void *buf, unsigned int buflen, int cmd,
        int ioctl_cmd, struct sock_context *sock)
{
    struct ifreq ifr;
    int sock_fd, err;
    sock_fd = sock->sock_fd;
    os_memset(&ifr, 0, sizeof(ifr));
    strlcpy(ifr.ifr_name, ifname, IFNAMSIZ);
    ifr.ifr_data = buf;
    err = ioctl(sock_fd, ioctl_cmd, &ifr);
    if (err < 0) {
        fprintf(stderr,"unable to send ioctl err= %d", err);
        return err;
    }
    return 0;
}
/*Ioctl to create a new vap */
int create_vap_ioctl(const char *ifname, void *buf, unsigned int buflen, int cmd,
        int ioctl_cmd, struct sock_context *sock, int ifnum1, int ifnum2)
{
    return send_command_ioctl(ifname, buf, buflen, cmd, ioctl_cmd, sock);
}

/*ioctl to delete a vap */
int delete_vap_ioctl(const char *ifname, void *buf, unsigned int buflen, int cmd,
        int ioctl_cmd, struct sock_context *sock, int ifnum1, int ifnum2)
{
    struct ifreq ifr;
    int res;
    int sock_fd;
    sock_fd = sock->sock_fd;

    os_memset(&ifr, 0, sizeof(ifr));
    res = os_snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "ath%d%d",
            ifnum1, ifnum2);
    if (res < 0 || res >= sizeof(ifr.ifr_name)) {
        fprintf(stderr,"os_snprintf err");
        return -1;
    }
    if (ioctl(sock_fd, ioctl_cmd, &ifr) < 0){
        fprintf(stderr,"ioctl(ioctl_cmd)");
        return -1;
    }
    return 0;
}
/*Ioctl to get param from friver*/
int send_command_get_ioctl(const char *ifname, int op, int *data,
        int ioctl_cmd, struct sock_context *sock)
{
    struct iwreq iwr;
    int sock_fd, err;
    sock_fd = sock->sock_fd;

    os_memset(&iwr, 0, sizeof(iwr));
    os_strlcpy(iwr.ifr_name, ifname, IFNAMSIZ);

    iwr.u.mode = op;
    err = ioctl(sock_fd, ioctl_cmd, &iwr);

    if (err < 0) {
        fprintf(stderr,"unable to send ioctl err = %d",err);
        return -1;
    }

    *data = iwr.u.mode;

    return 0 ;
}
/*return athr driver type */
char *get_driver_type_ioctl()
{
    return "athr";
}

/*return iwpriv tool type*/
char *get_tool_type_ioctl()
{
    return "iwpriv";
}

/*Ioctl socket initialisation*/
int init_context_ioctl (struct sock_context *sock)
{
    sock->sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock->sock_fd < 0) {
        fprintf(stderr,"socket creation failed");
        return -EIO;
    }
    return 0;
}
/*Ioctl socket destroy */
void destroy_context_ioctl (struct sock_context *sock)
{
    close(sock->sock_fd);
    return;
}
