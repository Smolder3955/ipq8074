/*
 * Copyright (c) 2011 Qualcomm Atheros, Inc..
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include <sys/socket.h>
#include <sys/types.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#define NETLINK_WIFIPOS (NETLINK_GENERIC + 6)

#ifndef MAX_PAYLOAD
#define MAX_PAYLOAD 8192 //1024
#endif

int wpc_create_nl_sock()
{
    int wpc_driver_sock;
    int on = 16*1024 ;
    int return_status;
    struct sockaddr_nl src_addr;

    wpc_driver_sock = socket(PF_NETLINK, SOCK_RAW, NETLINK_WIFIPOS);
    if (wpc_driver_sock < 0) {
        printf("socket errno=%d\n", wpc_driver_sock);
        return wpc_driver_sock;
    }
    memset(&src_addr, 0, sizeof(src_addr));
    src_addr.nl_family = PF_NETLINK;
    src_addr.nl_pid = getpid();  /* self pid */
    /* interested in group 1<<0 */
    src_addr.nl_groups = 0;


    return_status = setsockopt(wpc_driver_sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    if(return_status < 0) {
        printf("nl socket option failed\n");
        return return_status;
    }
    return_status = bind(wpc_driver_sock, (struct sockaddr*)&src_addr, sizeof(src_addr));
    if(return_status < 0) {
        printf("BIND errno=%d\n", return_status);
        return return_status;
    }

    return wpc_driver_sock;
}

void wpc_nlsock_initialise(struct sockaddr_nl *dst_addr, struct nlmsghdr *nlh, struct iovec *iov, struct msghdr *msg)
{
    memset(dst_addr, 0, sizeof(struct sockaddr_nl));
    dst_addr->nl_family = PF_NETLINK;
    dst_addr->nl_pid = 0;  /* self pid */

    /* interested in group 1<<0 */
    dst_addr->nl_groups = 0;
    nlh->nlmsg_len = NLMSG_SPACE(MAX_PAYLOAD);
    nlh->nlmsg_pid = getpid();
    nlh->nlmsg_flags = 0;

    iov->iov_base = (void *)nlh;
    iov->iov_len = nlh->nlmsg_len;

    memset(msg, 0, sizeof(struct msghdr));
    msg->msg_name = (void *)dst_addr;
    msg->msg_namelen = sizeof(struct sockaddr_nl);
    msg->msg_iov = iov;
    msg->msg_iovlen = 1;

}
