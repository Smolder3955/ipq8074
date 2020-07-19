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
/*
 * Copyright (c) 2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/wireless.h>
//#include <net/if.h>
#include <linux/un.h>

/*
 * Linux uses __BIG_ENDIAN and __LITTLE_ENDIAN while BSD uses _foo
 * and an explicit _BYTE_ORDER.  Sorry, BSD got there first--define
 * things in the BSD way...
 */
#ifndef _LITTLE_ENDIAN
#define _LITTLE_ENDIAN  1234    /* LSB first: i386, vax */
#endif
#ifndef _BIG_ENDIAN
#define _BIG_ENDIAN 4321    /* MSB first: 68000, ibm, net */
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

#include "os/linux/include/ieee80211_external.h"
/*
 * Prototypes
 */

/**
 * @brief
 *
 * @param ifname
 * @param msg
 * @param event
 * @param userif
 *
 * @return
 */

/**
 * @brief Indicates whether to wait for events or not
 */
static int loop_for_events ;


/**
 * @brief signal handler
 *
 * @param int
 */
static void
sig_handler(int sig)
{
    loop_for_events = 0;
    return;
}


/*
 *  The following code is extracted from : iwevent.c
 */
struct rtnl_handle
{
    int fd;
    struct sockaddr_nl local;
    struct sockaddr_nl      peer;
    __u32 seq;
    __u32 dump;
};


/*
 * Open a RtNetlink socket
 * Return: 0 on Success
 *         -ve on Error
 */
static inline
int rtnl_open(struct rtnl_handle *rth, unsigned subscriptions)
{
    int addr_len;

    /*printf("%s() \n",__FUNCTION__);*/
    memset(rth, 0, sizeof(*rth));

    rth->fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (rth->fd < 0) {
        /*perror("Cannot open netlink socket");*/
        return -1;
    }

    memset(&rth->local, 0, sizeof(rth->local));
    rth->local.nl_family = AF_NETLINK;
    rth->local.nl_groups = subscriptions;

    if (bind(rth->fd, (struct sockaddr*)&rth->local, \
                sizeof(rth->local)) < 0) {
        perror("Cannot bind netlink socket");
        return -1;
    }
    addr_len = sizeof(rth->local);

    if (getsockname(rth->fd, (struct sockaddr*)&rth->local,
                (socklen_t *) &addr_len) < 0) {
        perror("Cannot getsockname");
        return -1;
    }

    if (addr_len != sizeof(rth->local)) {
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

static inline
void rtnl_close(struct rtnl_handle *rth)
{
    /*printf("%s() \n",__FUNCTION__);*/
    close(rth->fd);
}

int tfs_notify(char *notify, int len)
{
    int i;
    printf("TFS NOTIFY \n");
    for(i = 0 ; i < len; i++) {
        printf("TFSID %d", notify[i]);
    }
    return 0;
}


int tfs_response(char *response, int len)
{
    int i;
    printf(" TFS RESPONSE \n");
    for(i = 0 ; i < (len - 1); i += 2) {
        printf("TFSID %d Status %d \n",response[i],response[i+1]);
    }
    return 0;
}

static void tclass_element_print(struct tfsreq_tclas_element *req_tclas)
{
    int i;

    printf("                   classifier_type=%d\n",
            req_tclas->classifier_type);
    printf("                   classifier_mask=%d\n",
            req_tclas->classifier_mask);

    printf("                   priority=%d\n",
            req_tclas->priority);


    if (req_tclas->classifier_type == 4) {
        if (req_tclas->clas.clas14.clas14_v4.version ==  4) {
            printf("                   version=%d\n",
                    req_tclas->clas.clas14.clas14_v4.version);
            printf("                   src_port=%d\n",
                    req_tclas->clas.clas14.clas14_v4.source_port);
            printf("                   dest_port=%d\n",
                    req_tclas->clas.clas14.clas14_v4.dest_port);
            printf("                   descp=%d\n",
                    req_tclas->clas.clas14.clas14_v4.dscp);
            printf("                   protocol =%d\n",
                    req_tclas->clas.clas14.clas14_v4.protocol);
            printf("                   sourceip=%d.%d.%d.%d\n",
                    req_tclas->clas.clas14.clas14_v4.source_ip[0],
                    req_tclas->clas.clas14.clas14_v4.source_ip[1],
                    req_tclas->clas.clas14.clas14_v4.source_ip[2],
                    req_tclas->clas.clas14.clas14_v4.source_ip[3]);
            printf("                   destip=%d.%d.%d.%d\n",
                    req_tclas->clas.clas14.clas14_v4.dest_ip[0],
                    req_tclas->clas.clas14.clas14_v4.dest_ip[1],
                    req_tclas->clas.clas14.clas14_v4.dest_ip[2],
                    req_tclas->clas.clas14.clas14_v4.dest_ip[3]);
        } else if(req_tclas->clas.clas14.clas14_v6.version == 6) {
            printf("                   version=%d\n",
                    req_tclas->clas.clas14.clas14_v6.version);
            printf("                   src_port=%d\n",
                    req_tclas->clas.clas14.clas14_v6.source_port);
            printf("                   dest_port=%d\n",
                    req_tclas->clas.clas14.clas14_v6.dest_port);
            printf("                   dscp=%d\n",
                    req_tclas->clas.clas14.clas14_v6.clas4_dscp);
            printf("                   next_header =%d\n",
                    req_tclas->clas.clas14.clas14_v6.clas4_next_header);
            printf("                   flow_label =%d%d%d\n",
                    req_tclas->clas.clas14.clas14_v6.flow_label[0],
                    req_tclas->clas.clas14.clas14_v6.flow_label[1],
                    req_tclas->clas.clas14.clas14_v6.flow_label[1]);
            printf("                   sourceip=");
            for(i = 0; i < 16; i++)
                printf("%d",
                        req_tclas->clas.clas14.clas14_v6.source_ip[i]);
            printf("\n");
            printf("                   destip=");
            for(i = 0; i < 16; i++)
                printf("%d",
                        req_tclas->clas.clas14.clas14_v6.dest_ip[i]);
            printf("\n");
        }
    } else if (req_tclas->classifier_type == 3) {

        int filter_len;

        printf("                       filter_offset=%d\n",
                    req_tclas->clas.clas3.filter_offset);
        filter_len = req_tclas->clas.clas3.filter_len;
        printf("                       filter_value=");
        for(i = 0 ; i < filter_len; i++)
            printf("%x",req_tclas->clas.clas3.filter_value[i]);

        printf("\n                     filter_mask=");
        for(i = 0 ; i < filter_len; i++)
            printf("%x",req_tclas->clas.clas3.filter_mask[i]);

        printf("\n");

    }
}
int fms_response(struct ieee80211_wnm_fmsresp *fmsrsp)
{
    struct fmsresp_element *fmsrsp_ie;
    struct fmsresp_fms_subele_status *fms_subele_status;
    struct fmsresp_tclas_subele_status *tclas_subele_status;
    struct tfsreq_tclas_element *req_tclas;
    int subelem_count = 0;
    int tclas_elem_count = 0;
    int i;

    for (i = 0; i < fmsrsp->num_fmsresp; i++)
    {
        fmsrsp_ie = &fmsrsp->fms_resp[i];
        printf(" fms response {\n");
        printf("    fms_token = %d \n",fmsrsp_ie->fms_token);

        while (subelem_count < fmsrsp_ie->num_subelements)
        {
            if(fmsrsp_ie->subelement_type == IEEE80211_FMS_STATUS_SUBELE)
            {
                printf("     fms_status_element { \n");
                fms_subele_status = &fmsrsp_ie->status.fms_subele_status[subelem_count];
                printf("        status = %d \n", fms_subele_status->status);
                printf("        delivery_interval = %d \n", fms_subele_status->del_itvl);
                printf("        max_delivery_interval = %d \n", fms_subele_status->max_del_itvl);
                printf("        fmsid = %d \n", fms_subele_status->fmsid);
                printf("        fms_counter = %d \n", fms_subele_status->fms_counter);
                printf("        rate_id(mcsidx.mask.rate_id) =0x%d.%d.%d \n",
                       fms_subele_status->rate_id.mcs_idx, fms_subele_status->rate_id.mask, fms_subele_status->rate_id.rate);
                printf("        mcast_addr = %02x:%02x:%02x:%02x:%02x:%02x \n",
                       fms_subele_status->mcast_addr[0], fms_subele_status->mcast_addr[1], fms_subele_status->mcast_addr[2],
                       fms_subele_status->mcast_addr[3], fms_subele_status->mcast_addr[4], fms_subele_status->mcast_addr[5]);
            }
            else if (fmsrsp_ie->subelement_type == IEEE80211_TCLASS_STATUS_SUBELE)
            {
                tclas_elem_count = 0;
                tclas_subele_status = &fmsrsp_ie->status.tclas_subele_status[subelem_count];
                printf("       tclas_status_element { \n");
                printf("         fmsid = %d \n", tclas_subele_status->fmsid);
                while(tclas_elem_count < tclas_subele_status->num_tclas_elements) {
                    printf("     tclas_elements {\n");
                    req_tclas = &tclas_subele_status->tclas[tclas_elem_count];
                    tclass_element_print(req_tclas);
                    tclas_elem_count++;
                    printf("     }\n");
                }
                printf("       } \n");

                if (tclas_subele_status->tclasprocess.elem_id == IEEE80211_ELEMID_TCLAS_PROCESS) {
                    printf("tclass_processing = %d\n", tclas_subele_status->tclasprocess.tclas_process);
                }
            }

            subelem_count++;
            printf(" }\n");
        }

    }
    return 0;
}
int fms_request(struct ieee80211_wlanconfig_wnm_fms *fms)
{
    struct fmsreq_subelement *req_subelement;
    struct ieee80211_wlanconfig_wnm_fms_req *req_fmsreq;
    struct tfsreq_tclas_element *req_tclas;
    int fmsreqelem_count = 0;
    int subelem_count = 0;
    int tclas_elem_count = 0;


    while(fmsreqelem_count < fms->num_fmsreq) {
        req_fmsreq = (struct ieee80211_wlanconfig_wnm_fms_req *)
                                &fms->fms_req[fmsreqelem_count];

        printf("\n fms request {\n");
        printf("    fms_token = %d \n",req_fmsreq->fms_token);

        subelem_count = 0;

        while(subelem_count < req_fmsreq->num_subelements) {
            req_subelement = (struct fmsreq_subelement *)
                                &req_fmsreq->subelement[subelem_count];


            printf("        sub_elements {\n");
            printf("           delivery_interval = %d \n",req_subelement->del_itvl);
            printf("           max_delivery_interval = %d \n",req_subelement->max_del_itvl);
            printf("           rate_id(mcsidx.mask.rate_id) =0x%d.%d.%d \n",
                    req_subelement->rate_id.mcs_idx, req_subelement->rate_id.mask, req_subelement->rate_id.rate);
            tclas_elem_count = 0;
            while(tclas_elem_count < req_subelement->num_tclas_elements) {

            printf("           tclas_elements {\n");
                req_tclas = &req_subelement->tclas[tclas_elem_count];
                tclass_element_print(req_tclas);
                tclas_elem_count++;
                    printf("            }\n");
            }

            subelem_count++;
                    printf("   }\n");
        }
        fmsreqelem_count++;
                    printf("}\n");
    }
    return 0;
}
int tfs_request(struct ieee80211_wlanconfig_wnm_tfs *tfs)
{

    struct tfsreq_subelement *req_subelement;
    struct ieee80211_wlanconfig_wnm_tfs_req *req_tfsreq;
    struct tfsreq_tclas_element *req_tclas;
    int tfsreqelem_count = 0;
    int subelem_count = 0;
    int tclas_elem_count = 0;


    while(tfsreqelem_count < tfs->num_tfsreq) {
        req_tfsreq = (struct ieee80211_wlanconfig_wnm_tfs_req *)
                                &tfs->tfs_req[tfsreqelem_count];

        printf(" tfs request {\n");
        printf("    tfsid = %d \n",req_tfsreq->tfsid);
        printf("    action_code  = %d \n",req_tfsreq->actioncode);


        subelem_count = 0;

        while(subelem_count < req_tfsreq->num_subelements) {
            req_subelement = (struct tfsreq_subelement *)
                                &req_tfsreq->subelement[subelem_count];


            printf("        sub_elements {\n");
            tclas_elem_count = 0;
            while(tclas_elem_count < req_subelement->num_tclas_elements) {

            printf("            tclas_elements {\n");
                req_tclas = &req_subelement->tclas[tclas_elem_count];
                tclass_element_print(req_tclas);
                tclas_elem_count++;
                    printf("            }\n");
            }

            subelem_count++;
                    printf("   }\n");
        }
        tfsreqelem_count++;
                    printf("}\n");
    }
    return 0;
}

int wnm_event(wnm_netlink_event_t *event)
{
    struct ieee80211_wlanconfig_wnm_tfs tfs;
    struct ieee80211_wlanconfig_wnm_fms fms;
    struct ieee80211_wnm_fmsresp fmsrsp;

    if(event->type == 0x11) {
       memcpy(&tfs, event->event_data, sizeof(tfs));
       tfs_request(&tfs);
    } else  if(event->type == 0x12) {
        tfs_response((char *) (event->event_data + 1), event->datalen);
    } else  if(event->type == 0x13) {
        tfs_notify((char *) event->event_data, event->datalen);
    } else if (event->type == 0x14) {
        memcpy(&fms, event->event_data, sizeof(fms));
        fms_request(&fms);
    } else if(event->type == 0x15) {
        memcpy(&fmsrsp, event->event_data, sizeof(fmsrsp));
        fms_response(&fmsrsp);
    }
    return 0;
}


/*
 * This routine handles events (i.e., call this when rth.fd
 * is ready to read).
*/
#define IV_HEADER_LEN 48
static inline void
handle_netlink_events(struct rtnl_handle *rth, \
        unsigned char *ifname)
{
    //char buf[8192];
    char buf[2500];
    char *pos;
    struct ieee80211_wlanconfig_wnm_tfs tfs;

    while(1)
    {
        struct sockaddr_nl sanl;
        socklen_t sanllen = sizeof(struct sockaddr_nl);
        struct wnm_netlink_event *event;

        recvfrom(rth->fd, buf, sizeof(buf), MSG_DONTWAIT, \
                                (struct sockaddr*)&sanl, &sanllen);
        memcpy(&tfs, buf + 62, sizeof(tfs));
        sleep(1);
        pos = buf + IV_HEADER_LEN;
        event = (struct wnm_netlink_event *) pos;
        wnm_event(event);
        return;
    }//end while
}


static inline int
wait_for_event(struct rtnl_handle * rth, \
               unsigned char *ifname)
{
    void (*old_handler)(int sig) ;
    int status = 0;

    /*printf("%s() \n",__FUNCTION__);*/

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
                    /*printf("%s(): Exit due to signal \n",__FUNCTION__);*/
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
            handle_netlink_events(rth, ifname);
        }
    }//end while

    /* Restore original signal handler for SIGINT */
    signal(SIGINT, old_handler) ;
    return status ;
}



/**
 * @brief
 *
 * @param ifname    (Interface name to listen)
 * @param event     (Event Structure)
 * @param mode      (Listening mode)
 *
 * @return
*/
int
main(int argc, char **argv )
{
    struct rtnl_handle    rth;
    int status = 0;
    unsigned char ifname[10];

    if(argc < 2) {
        printf(" Usage : %s <interface name>\n",argv[0]);
        exit(0);
    }
    /* Open netlink channel */
    //if(rtnl_open(&rth, RTMGRP_LINK) < 0) {
    //strlcpy(ifname, argv[1],sizeof(ifname));
    if (strlcpy((char *) ifname, argv[1], sizeof(ifname) >= sizeof(ifname))) {
        printf("%s: Interface too long %s\n",argv[0], argv[1]);
        return -1;
    }

    if(rtnl_open(&rth, RTMGRP_NOTIFY) < 0) {
        return -1;
    }
    wait_for_event(&rth ,ifname);
    rtnl_close(&rth);
//ret
    return status;
}

