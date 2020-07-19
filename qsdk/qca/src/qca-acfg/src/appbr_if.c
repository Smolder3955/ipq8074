/*
 * Copyright (c) 2008-2010, Atheros Communications Inc.
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
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include <sys/un.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/mman.h>

#include <linux/netlink.h>
#include <linux/if.h>
#include <stdint.h>

#include <appbr_if.h>


int32_t sock_rm = -1;

appbr_status_t
appbr_if_open_dl_conn(uint32_t app_id)
{
    (void)app_id;
    return APPBR_STAT_OK;
}

void
appbr_if_close_dl_conn()
{
    return;
}

appbr_status_t
appbr_if_open_ul_conn(uint32_t app_id)
{
    struct sockaddr_nl src_addr;
    (void)app_id;

    sock_rm = socket(PF_NETLINK, SOCK_RAW, APPBR_NETLINK_NUM);

    if (sock_rm < 0)
        return APPBR_STAT_ENOSOCK;

    memset(&src_addr, 0, sizeof(src_addr));
    src_addr.nl_family = AF_NETLINK;
    src_addr.nl_pid = 0;  /* Destination = Kernel */
    src_addr.nl_groups = 0;  /* unicast */

    /*
     * Until  Discovery happens, bind won't succeed
     * Keep issuing bind() call indefinitely until success
     */
    while( 1 ) {
        if (bind(sock_rm, (struct sockaddr*)&src_addr,
                    sizeof(src_addr)) < 0) {
            sleep(1);
        }
        else
            break;
    }

    return APPBR_STAT_OK;
}

void
appbr_if_close_ul_conn()
{
    if (sock_rm > 0)
        close(sock_rm);

    return;
}

appbr_status_t
appbr_if_send_cmd_remote(uint32_t app_id, void *buf, uint32_t size)
{
    appbr_status_t ret = 0;
    struct nlmsghdr *nlh;

    nlh = (struct nlmsghdr *) malloc(NLMSG_SPACE(size));

    if (nlh == NULL)
        return APPBR_STAT_ENOMEM;

    memset(nlh, 0, NLMSG_SPACE(size));
    /* Fill the netlink message header */
    nlh->nlmsg_len = NLMSG_LENGTH(size);
    nlh->nlmsg_pid = app_id;  /* embed AppID as Source PID */
    nlh->nlmsg_flags = 0;

    /* Fill in the netlink message payload */
    memcpy(NLMSG_DATA(nlh), buf, size);

    if (send(sock_rm, nlh, nlh->nlmsg_len, 0) < 0)
        ret = APPBR_STAT_ESENDCMD;

    free(nlh);
    return ret;

}

appbr_status_t
appbr_if_wait_for_response(void *buf, uint32_t size, uint32_t timeout)
{
    struct timeval tv;
    int res;
    fd_set rfds;

    struct msghdr msg;
    struct sockaddr_nl src_addr;
    struct iovec iov;

    iov.iov_base = (void *)buf;
    iov.iov_len = size;
    msg.msg_name = (void *)&src_addr;
    msg.msg_namelen = sizeof(src_addr);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    for (;;) {
        tv.tv_sec = timeout;
        tv.tv_usec = 0;
        FD_ZERO(&rfds);
        FD_SET(sock_rm, &rfds);
        res = select(sock_rm + 1, &rfds, NULL, NULL, &tv);
        if (FD_ISSET(sock_rm, &rfds)) {
            res = recvmsg(sock_rm, &msg, 0);
            if (res < 0)
                return APPBR_STAT_ERECV;
            break;
        } else {
            return APPBR_STAT_ERECV;
        }
    }

    return APPBR_STAT_OK;
}
