/*
* Copyright (c) 2015 Qualcomm Atheros, Inc.
* All Rights Reserved.
* Qualcomm Atheros Confidential and Proprietary.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <libnetlink.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/if.h>
#include <linux/if_link.h>
#include <linux/wireless.h>

/*
* Linux uses __BIG_ENDIAN and __LITTLE_ENDIAN while BSD uses _foo
* and an explicit _BYTE_ORDER.  Sorry, BSD got there first--define
* things in the BSD way...
*/

#ifndef _LITTLE_ENDIAN
#define _LITTLE_ENDIAN  1234    /* LSB first: i386, vax */
#endif
#ifndef _BIG_ENDIAN
#define _BIG_ENDIAN 4321        /* MSB first: 68000, ibm, net */
#endif
#ifndef ATH_SUPPORT_LINUX_STA
#include <asm/byteorder.h>
#endif
#if defined(__LITTLE_ENDIAN)
#define _BYTE_ORDER _LITTLE_ENDIAN
#elif defined(__BIG_ENDIAN)
#define _BYTE_ORDER _BIG_ENDIAN
#else
#error "Please fix asm/byteorder.h"
#endif


#define QCA_ASSOCDENIAL_STR   "MLME-ASSOCDENIAL.indication"

/*
* Prototypes
*/
static int event_stream (int ifindex, char *data, int len);
static void process_nl_newlink (struct nlmsghdr *nlh);
static int wait_for_event (struct rtnl_handle *rth);
static int open_nl_socket (struct rtnl_handle *rth, unsigned subscriptions);
static void process_nl_events (struct rtnl_handle *rth);


/**
* @brief Indicates whether to wait for events or not
*/
static int loop_for_events ;


/**
* @brief signal handler
*
* @param int
*/
static void sig_handler(int sig) {
    loop_for_events = 0;
    return;
}

/**
* @brief:          main execution function
*
* @param argc      argument count (no additional count required)
* @param argv      argument vector (no additional string required)
*
* @return          (return 1 for success)
*/
int main(int argc, char **argv )
{
    struct rtnl_handle rth;
    int ret = 0 ;

    if(argc != 1) {
        printf(" Usage : %s\n",argv[0]);
        exit(0);
    }

    /* Open netlink channel */
    if(open_nl_socket(&rth, RTMGRP_LINK) < 0) {
        return -1;
    }
    ret = wait_for_event(&rth);
    close(rth.fd);

    return ret;
}

/**
 * @brief: Function responcible to accepts the netlink handle and process only
 *         RTM_NEWLINK messages. other messages shall be trated as default.
 *
 * @param rth  (pointer to rtnl_handle)
 */
void process_nl_events(struct rtnl_handle *rth)
{
    char buf[2048] = {0x00};

    while(1)
    {
        struct sockaddr_nl sanl;
        socklen_t sanllen = sizeof(struct sockaddr_nl);
        struct nlmsghdr *nl_hdr;
        int count = 0;

        count = recvfrom(rth->fd, buf, sizeof(buf), MSG_DONTWAIT, (struct sockaddr*)&sanl, &sanllen);

        if(count < 0) {
            if(errno != EINTR && errno != EAGAIN) {
                fprintf(stderr, "%s: error reading netlink: %s.\n", __PRETTY_FUNCTION__, strerror(errno));
            }
            return;
        }

        nl_hdr = (struct nlmsghdr*)buf;
        while(count >= (int)sizeof(*nl_hdr)){

            int len = nl_hdr->nlmsg_len;
            int l = len - sizeof(*nl_hdr);
            if(l < 0 || len > count){
                fprintf(stderr, "%s: malformed netlink message: len=%d\n", __PRETTY_FUNCTION__, len);
                break;
            }
            switch(nl_hdr->nlmsg_type){
            case RTM_NEWLINK:
                process_nl_newlink(nl_hdr);
                break;
            default:
                fprintf(stderr, "%s: Unhandled  nlmsg of type %#x.\n", __PRETTY_FUNCTION__, nl_hdr->nlmsg_type);
                break;
            }
            len = NLMSG_ALIGN(len);
            count -= len;
            nl_hdr = (struct nlmsghdr*)((char*)nl_hdr + len);
        }
    }
}


/**
 * @brief : Function responcible to open netlink socket
 *
 * @param rth
 * @param subscriptions
 *
 * @return return 1 for success
 */
int open_nl_socket(struct rtnl_handle *rth, unsigned subscriptions)
{
    int addr_len;

    memset(rth, 0, sizeof(*rth));

    rth->fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (rth->fd < 0) {
        perror("Cannot open netlink socket");
        return -1;
    }

    memset(&rth->local, 0, sizeof(rth->local));
    rth->local.nl_family = AF_NETLINK;
    rth->local.nl_groups = subscriptions; /*subscriptions;*/

    if (bind(rth->fd, (struct sockaddr*)&rth->local, sizeof(struct sockaddr_nl)) < 0) {
        perror("Cannot bind netlink socket");
        return -1;
    }
    addr_len = sizeof(rth->local);

    if (getsockname(rth->fd, (struct sockaddr*)&rth->local,
                (socklen_t *) &addr_len) < 0) {
        perror("Cannot getsockname");
        return -1;
    }

    if (addr_len != sizeof(struct sockaddr_nl)) {
        /*fprintf(stderr, "Wrong address length %d\n", addr_len);*/
        return -1;
    }

    if (rth->local.nl_family != AF_NETLINK) {
        /*fprintf(stderr,"Wrong address family %d\n", rth->local.nl_family);*/
        return -1;
    }
    rth->seq = time(NULL);
    return 0;
}


/**
 * @brief : Function responcible to wait for all netlink messages and send to appropiate
 *          function for processing.
 *
 * @param rth
 *
 * @return return 1 for success
 */
int wait_for_event(struct rtnl_handle * rth)
{
    void (*old_handler)(int sig) ;
    int status = 0;

    /* Register new signal handler */
    old_handler = signal(SIGINT, sig_handler) ;
    if(old_handler == SIG_ERR)
    {
        printf("%s(): unable to register signal handler \n",__FUNCTION__);
        return -1;
    }

    loop_for_events = 1;
    while(1)
    {
        fd_set rfds;        /* File descriptors for select */
        int last_fd;        /* Last fd */
        int ret;

        /* Guess what ? We must re-generate_id rfds each time */
        FD_ZERO(&rfds);
        FD_SET(rth->fd, &rfds);
        last_fd = rth->fd;

        /* Wait until something happens */
        ret = select(last_fd + 1, &rfds, NULL, NULL, NULL);

        /* Check if there was an error */
        if(ret < 0)
        {
            if(errno == EINTR)
            {
                if(loop_for_events == 0)
                {
                    status = 1;
                    break ;
                }
                else
                continue ;
            }
            else if(errno == EAGAIN)
            {
                continue;
            }
            else
            {
                /* Unhandled signal */
                status = -1;
                break;
            }
        }

        /* Check for interface discovery events. */
        if(FD_ISSET(rth->fd, &rfds)) {
            process_nl_events(rth);
        }
    }//end while

    /* Restore original signal handler for SIGINT */
    signal(SIGINT, old_handler) ;
    return status ;
}


/**
 * @brief: Function responcible to process all RTM_NEWLINK messages and does sanity check before
 *         sending for further processing.
 *
 * @param nlh
 */
void process_nl_newlink(struct nlmsghdr *nlh) {
    struct ifinfomsg    *nl_ifi = NLMSG_DATA(nlh);

    if(nlh->nlmsg_type != RTM_NEWLINK){
        fprintf(stderr, "Unexpected Message nlmsg_type = %d\n", nlh->nlmsg_type);
        return;
    }
    /* Check for attributes */
    if (nlh->nlmsg_len > NLMSG_ALIGN(sizeof(struct ifinfomsg))){
        int attrlen = nlh->nlmsg_len - NLMSG_ALIGN(sizeof(struct ifinfomsg));
        struct rtattr *attr = (void *) ((char *) nl_ifi + NLMSG_ALIGN(sizeof(struct ifinfomsg)));
        while (RTA_OK(attr, attrlen)){
            event_stream(nl_ifi->ifi_index,(char *) attr + RTA_ALIGN(sizeof(struct rtattr)),
            attr->rta_len - RTA_ALIGN(sizeof(struct rtattr)));
            attr = RTA_NEXT(attr, attrlen);
        }
    }
}


/**
 * @brief:  Function responcible to process IWEVCUSTOM netlink data buffer
 *
 * @param ifindex
 * @param data
 * @param len
 *
 * @return return 1 for success
 */
int event_stream(int ifindex, char *data, int len)
{
    struct iw_event iw_buf, *iw = &iw_buf;
    char *pos, *end, *custom, *buf;

    pos = data;
    end = data + len;

    while (pos + IW_EV_LCP_LEN <= end) {
        memcpy(&iw_buf, pos, IW_EV_LCP_LEN);
        if (iw->len <= IW_EV_LCP_LEN)
        return -1;
        custom = pos + IW_EV_POINT_LEN;
        char *dpos = (char *) &iw_buf.u.data.length;
        int dlen = dpos - (char *) &iw_buf;
        memcpy(dpos, pos + IW_EV_LCP_LEN, sizeof(struct iw_event) - dlen);

        if(iw->cmd == IWEVCUSTOM) {
            if (custom + iw->u.data.length > end)
            return -1;
            buf = malloc(iw->u.data.length + 1);
            if ((buf == NULL) || (iw->len < sizeof(QCA_ASSOCDENIAL_STR))) return -1;
            memcpy(buf, custom, iw->u.data.length);
            buf[iw->u.data.length] = '\0';
            fprintf(stderr, "Custom event from QCA Driver: %s\n",buf);
        }
        pos += iw->len;
    }
    return(0);
}
