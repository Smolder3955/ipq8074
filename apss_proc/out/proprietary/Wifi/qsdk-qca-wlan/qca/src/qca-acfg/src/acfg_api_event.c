/*
 * Copyright (c) 2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
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
 * Qualcomm Atheros, Inc. chooses to take this file subject
 * only to the terms of the BSD license.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <stdint.h>

#include <linux/un.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <acfg_types.h>
#include <acfg_api.h>
#include <acfg_api_pvt.h>
#include <acfg_event.h>
#include <acfg_security.h>
#include <acfg_api_event.h>
#include <acfg_misc.h>

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
static uint32_t
notify_event(uint8_t *ifname, acfg_os_event_t *msg, \
        acfg_event_t *event, uint8_t *userif);

/**
 * @brief Indicates whether to wait for events or not
 */
static int loop_for_events ;
int wsupp_status_init = 0;

/* Extern Declarations */
extern int
hwaddr_aton(char  *txt, uint8_t *addr);
extern int
acfg_ctrl_iface_send(uint32_t sock, char *cmd, size_t cmd_len,
        char *replybuf, int32_t *reply_len);
extern int
acfg_ctrl_iface_open(uint8_t *ifname, char *ctrl_iface_dir,
        struct wsupp_ev_handle *wsupp_h);
extern void
acfg_close_ctrl_sock (struct wsupp_ev_handle wsupp_h);
extern uint32_t
acfg_app_open_socket(struct wsupp_ev_handle *app_sock_h);

/**
 * @brief signal handler
 *
 * @param int
 */
static void
sig_handler(int sig)
{
    if(sig == SIGINT) {
        loop_for_events = 0;
    }
    else
        acfg_log_errstr("%s:Unknown signal(%d) received\n\r", __func__, sig);

    return;
}


/*
 * Fields required to maintain netlink socket.
 */
struct acfg_rtnl_hdl
{
    int acfg_fd;
    struct sockaddr_nl acfg_local;
    struct sockaddr_nl      acfg_peer;
    __u32 acfg_seq;
    __u32 acfg_dump;
};


/*
 * Open a RtNetlink socket
 * Return: 0 on Success
 *         -ve on Error
 */
static inline
int acfg_rt_nl_open(struct acfg_rtnl_hdl *acfg_rth, unsigned sub)
{
    int acfg_addr_len;

    memset(acfg_rth, 0, sizeof(struct acfg_rtnl_hdl));

#ifdef ACFG_PARTIAL_OFFLOAD
    acfg_rth->acfg_fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_ACFG_EVENT);
#else
    acfg_rth->acfg_fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
#endif

    if (acfg_rth->acfg_fd < 0) {
        return -1;
    }

    memset(&acfg_rth->acfg_local, 0, sizeof(acfg_rth->acfg_local));
    acfg_rth->acfg_local.nl_family = AF_NETLINK;
    acfg_rth->acfg_local.nl_groups = sub;

    if (bind(acfg_rth->acfg_fd, \
                (struct sockaddr*)&acfg_rth->acfg_local, sizeof(acfg_rth->acfg_local)) < 0) {
        acfg_log_errstr("bind failed: %s\n", strerror(errno));
        goto error;
    }

    acfg_addr_len = sizeof(acfg_rth->acfg_local);
    if (getsockname(acfg_rth->acfg_fd, (struct sockaddr*)&acfg_rth->acfg_local,
                (socklen_t *) &acfg_addr_len) < 0) {
        goto error;
    }

    if (acfg_addr_len != sizeof(acfg_rth->acfg_local)) {
        goto error;
    }

    if (acfg_rth->acfg_local.nl_family != AF_NETLINK) {
        goto error;
    }
    acfg_rth->acfg_seq = time(NULL);
    return 0;

error:
      close(acfg_rth->acfg_fd);
      return -1;
}

static inline
void acfg_rt_nl_close(struct acfg_rtnl_hdl *acfg_rth)
{
    close(acfg_rth->acfg_fd);
}

/*
 * Respond to a single RTM_NEWLINK event from the rtnetlink socket.
 * Return: 0 On Success ;
 *
 */
static int
parse_event(struct nlmsghdr *nlh, acfg_event_t *event, uint8_t *userif)
{
    struct ifinfomsg* ifi;

    ifi = NLMSG_DATA(nlh);

    if(nlh->nlmsg_type != RTM_NEWLINK)
        return 0;

    /* Check for attributes */
    if (nlh->nlmsg_len > NLMSG_ALIGN(sizeof(struct ifinfomsg)))
    {
        int attrlen = nlh->nlmsg_len - NLMSG_ALIGN(sizeof(struct ifinfomsg));
        struct rtattr *attr = (void *) ((char *) ifi +
                NLMSG_ALIGN(sizeof(struct ifinfomsg)));
        char *ifname = NULL;

        while (RTA_OK(attr, attrlen))
        {

            /* Check if the Wireless kind */
            if(attr->rta_type == IFLA_IFNAME)
            {
                ifname = (char *) attr + RTA_ALIGN(sizeof(struct rtattr)) ;
                /*
                   fprintf(stderr,"%s: attr type is IFLA_IFNAME - %s\n",\
                   __FUNCTION__,ifname);
                 */
            }

            if(attr->rta_type == IFLA_WIRELESS)
            {
                /*fprintf(stderr,"%s: wireless event \n",__FUNCTION__);*/

                acfg_os_event_t *msg = (acfg_os_event_t *) ((char *) attr + \
                        RTA_ALIGN(sizeof(struct rtattr))) ;

                if(ifname)
                    notify_event((uint8_t *)ifname, msg, event, userif);
            }

            attr = RTA_NEXT(attr, attrlen);
        }
    }//endif

    return 0;
}


/*
 * This routine handles events (i.e., call this when rth.acfg_fd
 * is ready to read).
 */
static inline void
handle_netlink_events(struct acfg_rtnl_hdl *rth, \
        acfg_event_t *event, uint8_t *ifname)
{

    /*fprintf(stderr, "acfg_lib: %s \n",__FUNCTION__);*/
    while(1)
    {
        struct sockaddr_nl sanl;
        socklen_t sanllen = sizeof(struct sockaddr_nl);
        struct nlmsghdr *h;
        int amt;
        char buf[2048];

        amt = recvfrom(rth->acfg_fd, buf, sizeof(buf), MSG_DONTWAIT, \
                (struct sockaddr*)&sanl, &sanllen);
        if(amt < 0)
        {
            /*fprintf(stderr, "acfg_lib: error reading netlink\n");*/
            return ;
        }

        if(amt == 0)
        {
            /*fprintf(stderr, "acfg_lib: EOF on netlink\n");*/
            return ;
        }

        h = (struct nlmsghdr*)buf;
        while(amt >= (int)sizeof(*h))
        {
            int len = h->nlmsg_len;
            int l = len - sizeof(*h);

            if(l < 0 || len > amt)
            {
                /*fprintf(stderr, "%s: malformed netlink message: len=%d\n",\
                  __FUNCTION__, len);*/
                break;
            }

            switch(h->nlmsg_type)
            {
                case RTM_NEWLINK:
                case RTM_DELLINK:
                    /*fprintf(stderr,"RTM_NEW/DEL LINK in %s\n",__FUNCTION__);*/
                    parse_event(h,event,ifname);
                    break;

                default:
                    /*fprintf(stderr,"default in %s \n",__FUNCTION__);*/
                    break;
            }

            len = NLMSG_ALIGN(len);
            amt -= len;
            h = (struct nlmsghdr*)((char*)h + len);
        }//end while

    }//end while
}

static uint32_t
parse_wsupp_event(char *event_buf, acfg_os_event_t *event)
{
    char *pos;

    if (ACFG_N_STR_MATCH(event_buf, WSUPP_AP_STA_CONNECTED))
    {
        acfg_wsupp_ap_sta_conn_t *ap_sta_conn;
        event->id = ACFG_EV_WSUPP_AP_STA_CONNECTED;
        ap_sta_conn = (void*) &event->data;
        pos = strchr(event_buf, ' ');
        pos++;
        hwaddr_aton(pos, ap_sta_conn->station_address);
        acfg_os_strcpy((uint8_t *)ap_sta_conn->raw_message, (uint8_t *)event_buf,sizeof(ap_sta_conn->raw_message));
    } else if (ACFG_N_STR_MATCH(event_buf, WSUPP_AP_STA_DISCONNECTED)) {
        acfg_wsupp_ap_sta_conn_t *ap_sta_conn;
        event->id = ACFG_EV_WSUPP_AP_STA_DISCONNECTED;
        ap_sta_conn = (void*) &event->data;
        pos = strchr(event_buf, ' ');
        pos++;
        hwaddr_aton(pos, ap_sta_conn->station_address);
        acfg_os_strcpy((uint8_t *)ap_sta_conn->raw_message, (uint8_t *)event_buf,sizeof(ap_sta_conn->raw_message));
    } else if (ACFG_N_STR_MATCH(event_buf, WSUPP_CTRL_EVENT_CONNECTED)) {
        acfg_wsupp_wpa_conn_t *wpa_conn;
        event->id = ACFG_EV_WSUPP_WPA_EVENT_CONNECTED;
        wpa_conn = (void*) &event->data;
        pos = strchr(event_buf + strlen(WSUPP_CTRL_EVENT_CONNECTED),
                ':');
        pos -= 2;
        hwaddr_aton(pos, wpa_conn->station_address);
        acfg_os_strcpy((uint8_t *)wpa_conn->raw_message, (uint8_t *)event_buf,sizeof(wpa_conn->raw_message));

    } else if (ACFG_N_STR_MATCH(event_buf, WSUPP_CTRL_EVENT_DISCONNECTED)) {
        acfg_wsupp_wpa_conn_t *wpa_conn;
        event->id = ACFG_EV_WSUPP_WPA_EVENT_DISCONNECTED;
        wpa_conn = (void*) &event->data;
        pos = strchr(event_buf + strlen(WSUPP_CTRL_EVENT_DISCONNECTED),
                ':');
        pos -= 2;
        hwaddr_aton(pos, wpa_conn->station_address);
        acfg_os_strcpy((uint8_t *)wpa_conn->raw_message, (uint8_t *)event_buf,sizeof(wpa_conn->raw_message));

    } else if (ACFG_N_STR_MATCH(event_buf, WSUPP_CTRL_EVENT_ASSOC_REJECT)) {
        acfg_wsupp_assoc_t *ap_sta_conn;
        event->id = ACFG_EV_WSUPP_ASSOC_REJECT;
        ap_sta_conn = (void*) &event->data;
        pos = strchr(event_buf + strlen(WSUPP_CTRL_EVENT_ASSOC_REJECT),
                ':');
        if (pos != NULL) {
            pos -= 2;
            hwaddr_aton(pos, ap_sta_conn->addr);
        }
        acfg_os_strcpy((uint8_t *)ap_sta_conn->raw_message, (uint8_t *)event_buf,sizeof(ap_sta_conn->raw_message));
    } else if (ACFG_N_STR_MATCH(event_buf, WSUPP_CTRL_EVENT_EAP_SUCCESS)) {
        acfg_wsupp_eap_t *eap_status;
        event->id = ACFG_EV_WSUPP_EAP_SUCCESS;
        eap_status = (void*) &event->data;
        pos = strchr(event_buf + strlen(WSUPP_CTRL_EVENT_EAP_SUCCESS),
                ':');
        if (pos != NULL) {
            pos -= 2;
            hwaddr_aton(pos, eap_status->addr);
        }
        acfg_os_strcpy((uint8_t *)eap_status->raw_message, (uint8_t *)event_buf,sizeof(eap_status->raw_message));
    } else if (ACFG_N_STR_MATCH(event_buf, WSUPP_CTRL_EVENT_EAP_FAILURE)) {
        acfg_wsupp_eap_t *eap_status;
        event->id = ACFG_EV_WSUPP_EAP_FAILURE;
        eap_status = (void*) &event->data;
        pos = strchr(event_buf + strlen(WSUPP_CTRL_EVENT_EAP_FAILURE),
                ':');
        if (pos != NULL) {
            pos -= 2;
            hwaddr_aton(pos, eap_status->addr);
        }
        acfg_os_strcpy((uint8_t *)eap_status->raw_message, (uint8_t *)event_buf,sizeof(eap_status->raw_message));
    } else if (ACFG_N_STR_MATCH(event_buf,
                WSUPP_CTRL_EVENT_WPS_NEW_AP_SETTINGS))
    {
        acfg_wsupp_wps_new_ap_settings_t *wps_new_ap_setting;
        event->id = ACFG_EV_WSUPP_WPS_NEW_AP_SETTINGS;
        wps_new_ap_setting = (void*) &event->data;
        acfg_os_strcpy((uint8_t *)wps_new_ap_setting->raw_message, (uint8_t *)event_buf,sizeof(wps_new_ap_setting->raw_message));

    } else if (ACFG_N_STR_MATCH(event_buf, WSUPP_CTRL_EVENT_WPS_SUCCESS)) {
        acfg_wsupp_wps_success_t   *wps_success;
        event->id = ACFG_EV_WSUPP_WPS_SUCCESS;
        wps_success = (void *)&event->data;
        acfg_os_strcpy((uint8_t *)wps_success->raw_message, (uint8_t *)event_buf,sizeof(wps_success->raw_message));
    } else if (ACFG_N_STR_MATCH(event_buf,
                WSUPP_CTRL_EVENT_WPS_ENROLLEE_SEEN))
    {
        acfg_wsupp_wps_enrollee_t   *wps_enr;
        event->id = ACFG_EV_WSUPP_WPS_EVENT_ENROLLEE_SEEN;
        wps_enr = (void *)&event->data;
        pos = strchr(event_buf + strlen(WSUPP_CTRL_EVENT_WPS_ENROLLEE_SEEN),
                ':');
        if (pos != NULL) {
            pos -= 2;
            hwaddr_aton(pos, wps_enr->station_address);
        }
        acfg_os_strcpy((uint8_t *)wps_enr->raw_message, (uint8_t *)event_buf,sizeof(wps_enr->raw_message));
    } else {
        acfg_wsupp_raw_message_t *raw;

        event->id = ACFG_EV_WSUPP_RAW_MESSAGE;
        raw = (void *)&event->data;
        acfg_os_strcpy((uint8_t *)raw->raw_message, (uint8_t *)event_buf,sizeof(raw->raw_message));
    }
    return QDF_STATUS_SUCCESS;
}

void
handle_wsupp_events (int32_t sock, acfg_event_t *event,
        uint8_t *ifname)
{
    char buf[255];
    int32_t len = sizeof (buf);
    int32_t res;
    acfg_os_event_t msg;

    memset(buf, '\0', sizeof (buf));
    do {
        res = recv(sock, buf, len, MSG_DONTWAIT);
        if (res > 0) {
            parse_wsupp_event(buf + 3, &msg);
            notify_event(ifname, &msg, event, ifname);
        } else {
            return;
        }
    } while (1);
}

int wait_for_event_all(struct wsupp_ev_handle *wsupp_h,
        int count,
        struct acfg_rtnl_hdl *rth,
        int app_sock,
        acfg_event_t *event)
{
    struct timeval tv, *tvp;
    char buf[255];
    int len = sizeof (buf), ret = 0, i;
    int last_fd;        /* Last fd */
    struct sockaddr_un from;
    socklen_t fromlen = sizeof(from);

    /* Register new signal handler */
    while (1)
    {
        fd_set rdfs;        /* File descriptors for select */
        tvp = &tv;

        last_fd = 0;
        tvp->tv_sec = 10;
        tvp->tv_usec = 0;

        FD_ZERO(&rdfs);
        if (app_sock > 0 && ( app_sock < __FD_SETSIZE)) {
            FD_SET(app_sock, &rdfs);
            if (app_sock > last_fd)
                last_fd = app_sock;
        }
        for (i = 0; i < count; i++) {
            if (wsupp_h[i].sock > 0) {
                if (wsupp_h[i].sock > last_fd)
                    last_fd = wsupp_h[i].sock;
                FD_SET(wsupp_h[i].sock, &rdfs);
            }
        }
        if (rth->acfg_fd > 0) {
            FD_SET(rth->acfg_fd, &rdfs);
            if (rth->acfg_fd > last_fd)
                last_fd = rth->acfg_fd;
        }
        ret = select(last_fd + 1, &rdfs, NULL, NULL, NULL);
        if (wsupp_status_init == 1)  {
            wsupp_status_init = 0;
            ret = 1;
            break;
        }
        if (ret < 0) {
            if (errno == EAGAIN) {
                continue;
            } else if (errno == EINTR) {
                if (loop_for_events == 0) {
                    ret = 2;
                    break;
                }
            }
        }
        if ((ret == 0) && tvp) {
            acfg_log_errstr("Timer expired\n");
            return -1;
        }
        if ((app_sock > 0 && ( app_sock < __FD_SETSIZE)) && FD_ISSET (app_sock, &rdfs)) {
            memset(buf , '\0', len);
            if (recvfrom (app_sock, buf, len, 0,
                        (struct sockaddr *) &from, &fromlen) < 0)
            {
                continue;
            }
            if (strncmp(buf, ACFG_APP_EVENT_INTERFACE_MOD,
                        strlen(ACFG_APP_EVENT_INTERFACE_MOD)) == 0)
            {
                ret = 1;
                break;
            }
        }
        if(FD_ISSET(rth->acfg_fd, &rdfs))
            handle_netlink_events(rth, event, NULL);

        for (i = 0; i < count; i++) {
            if (wsupp_h[i].sock > 0) {
                if (FD_ISSET (wsupp_h[i].sock, &rdfs)) {
                    handle_wsupp_events(wsupp_h[i].sock, event,
                            wsupp_h[i].ifname);
                }
            }
        }
    }
    return ret;
}

/**
 * Wait until we get an event
 *
 * Return: 0 - Success
 *         1 - Exit due to received SIGINT
 *         -ve - Error
 */
static inline int
wait_for_event(struct acfg_rtnl_hdl * rth, \
        acfg_event_t *event, uint8_t *ifname)
{
    void (*old_handler)(int sig) ;
    int status = 0;

    /*printf("%s() \n",__FUNCTION__);*/

    /* Register new signal handler */
    old_handler = signal(SIGINT, sig_handler) ;
    if(old_handler == SIG_ERR)
    {
        acfg_log_errstr("%s(): unable to register signal handler \n",__FUNCTION__);
        return -1;
    }

    loop_for_events = 1;
    while(1)
    {
        fd_set rfds;        /* File descriptors for select */
        int last_fd;        /* Last fd */
        int ret;

        /* Guess what ? We must re-generate rfds each time */
        FD_ZERO(&rfds);
        FD_SET(rth->acfg_fd, &rfds);
        last_fd = rth->acfg_fd;

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
                continue;
            else
            {
                /* Unhandled signal */
                status = -1;
                break;
            }
        }

        /* Check for interface discovery events. */
        if(FD_ISSET(rth->acfg_fd, &rfds))
            handle_netlink_events(rth, event,ifname);
    }//end while

    /* Restore original signal handler for SIGINT */
    signal(SIGINT, old_handler) ;
    return status ;
}



static uint32_t
notify_event(uint8_t *ifname, acfg_os_event_t *msg, \
        acfg_event_t *event, uint8_t *userif)

{
    uint32_t status = QDF_STATUS_SUCCESS;
    acfg_ev_data_t *pdata = &msg->data ;

    if (userif != NULL) {
        if(acfg_os_cmp_str(ifname, userif, ACFG_MAX_IFNAME) != 0)
            return QDF_STATUS_SUCCESS ;
    }

    switch(msg->id)
    {
        case ACFG_EV_SCAN_DONE:
            if(event->scan_done)
                status = event->scan_done(ifname, &pdata->scan);
            break;

        case ACFG_EV_ASSOC_AP:
            if(event->assoc_ap)
                status = event->assoc_ap(ifname, &pdata->assoc_ap);
            break;
        case ACFG_EV_DISASSOC_AP:
            if(event->disassoc_ap)
                status = event->disassoc_ap(ifname, &pdata->disassoc_ap);
            break;
        case ACFG_EV_AUTH_AP:
            if(event->auth_ap)
                status = event->auth_ap(ifname, &pdata->auth);
            break;
        case ACFG_EV_DEAUTH_AP:
            if(event->deauth_ap)
                status = event->deauth_ap(ifname, &pdata->dauth);
            break;

        case ACFG_EV_ASSOC_STA:
            if(event->assoc_sta)
                status = event->assoc_sta(ifname, &pdata->assoc_sta);
            break;
        case ACFG_EV_DISASSOC_STA:
            if(event->disassoc_sta)
                status = event->disassoc_sta(ifname, &pdata->disassoc_sta);
            break;
        case ACFG_EV_AUTH_STA:
            if(event->auth_sta)
                status = event->auth_sta(ifname, &pdata->auth);
            break;
        case ACFG_EV_DEAUTH_STA:
            if(event->deauth_sta)
                status = event->deauth_sta(ifname, &pdata->dauth);
            break;

        case ACFG_EV_CHAN_START:
            if(event->chan_start)
                status = event->chan_start(ifname, &pdata->chan_start);
            break;

        case ACFG_EV_CHAN_END:
            if(event->chan_end)
                status = event->chan_end(ifname, &pdata->chan_end);
            break;

        case ACFG_EV_RX_MGMT:
            if(event->rx_mgmt)
                status = event->rx_mgmt(ifname, NULL);
            break;

        case ACFG_EV_SENT_ACTION:
            if(event->sent_action)
                status = event->sent_action(ifname, NULL);
            break;

        case ACFG_EV_LEAVE_AP:
            if(event->leave_ap)
                status = event->leave_ap(ifname, &pdata->leave_ap);
            break;

        case ACFG_EV_GEN_IE:
            if(event->gen_ie)
                status = event->gen_ie(ifname, &pdata->gen_ie);
            break;

        case ACFG_EV_ASSOC_REQ_IE:
            if(event->assoc_req_ie)
                status = event->assoc_req_ie(ifname, &pdata->gen_ie);
            break;

        case ACFG_EV_RADAR:
            if(event->radar)
                status = event->radar(ifname, &pdata->radar);
            break;

        case ACFG_EV_WSUPP_RAW_MESSAGE:
            if (event->wsupp_raw_message)
                status = event->wsupp_raw_message(ifname, &pdata->wsupp_raw_message);
            break;
        case ACFG_EV_WSUPP_AP_STA_CONNECTED:
            if (event->wsupp_ap_sta_conn)
                status = event->wsupp_ap_sta_conn(ifname, &pdata->wsupp_ap_sta_conn);
            break;
        case ACFG_EV_WSUPP_WPA_EVENT_CONNECTED:
            if (event->wsupp_wpa_conn)
                status = event->wsupp_wpa_conn(ifname, &pdata->wsupp_wpa_conn);
            break;
        case ACFG_EV_WSUPP_WPA_EVENT_DISCONNECTED:
            if (event->wsupp_wpa_disconn)
                status = event->wsupp_wpa_disconn(ifname, &pdata->wsupp_wpa_conn);
            break;
        case ACFG_EV_WSUPP_WPA_EVENT_TERMINATING:
            if (event->wsupp_wpa_term)
                status = event->wsupp_wpa_term(ifname, &pdata->wsupp_wpa_conn);
            break;
        case ACFG_EV_WSUPP_WPA_EVENT_SCAN_RESULTS:
            if (event->wsupp_wpa_scan)
                status = event->wsupp_wpa_scan(ifname, &pdata->wsupp_wpa_conn);
            break;
        case ACFG_EV_WSUPP_WPS_EVENT_ENROLLEE_SEEN:
            if (event->wsupp_wps_enrollee)
                status = event->wsupp_wps_enrollee(ifname, &pdata->wsupp_wps_enrollee);
            break;
        case ACFG_EV_PUSH_BUTTON:
            if (event->push_button) {
                event->push_button(ifname, &pdata->pbc);
            }
            break;
        case ACFG_EV_WSUPP_ASSOC_REJECT:
            if (event->wsupp_assoc_reject) {
                event->wsupp_assoc_reject(ifname, &pdata->wsupp_assoc);
            }
            break;
        case ACFG_EV_WSUPP_EAP_SUCCESS:
            if (event->wsupp_eap_success) {
                event->wsupp_eap_success(ifname, &pdata->wsupp_eap_status);
            }
            break;
        case ACFG_EV_WSUPP_EAP_FAILURE:
            if (event->wsupp_eap_failure) {
                event->wsupp_eap_failure(ifname, &pdata->wsupp_eap_status);
            }
            break;
        case ACFG_EV_WSUPP_WPS_NEW_AP_SETTINGS:
            {
                if (event->wsupp_wps_new_ap_setting) {
                    event->wsupp_wps_new_ap_setting(ifname,
                            &pdata->wsupp_wps_new_ap_setting);
                }
            }
            break;
        case ACFG_EV_WSUPP_WPS_SUCCESS:
            {
                if (event->wsupp_wps_success) {
                    event->wsupp_wps_success(ifname, &pdata->wsupp_wps_success);
                }
            }
            break;
        case ACFG_EV_STA_SESSION:
            if(event->session_timeout)
                status = event->session_timeout(ifname, &pdata->session);
            break;
        case ACFG_EV_TX_OVERFLOW:
            if(event->tx_overflow)
                status = event->tx_overflow(ifname);
            break;
        case ACFG_EV_MGMT_RX_INFO:
            if(event->mgmt_rx_info)
                status = event->mgmt_rx_info(ifname, &(pdata->mgmt_ri));
            break;
        case ACFG_EV_WDT_EVENT:
            if(event->wdt_event)
                status = event->wdt_event(ifname, &pdata->wdt);
            break;
        case ACFG_EV_NF_DBR_DBM_INFO:
            if(event->nf_dbr_dbm_info)
                status = event->nf_dbr_dbm_info(ifname, &pdata->nf_dbr_dbm);
            break;
        case ACFG_EV_PACKET_POWER_INFO:
            if(event->packet_power_info)
                status = event->packet_power_info(ifname, &pdata->packet_power);
            break;
        case ACFG_EV_CAC_START:
            if(event->cac_start)
                status = event->cac_start(ifname);
            break;
        case ACFG_EV_UP_AFTER_CAC:
            if(event->up_after_cac)
                status = event->up_after_cac(ifname);
            break;
        case ACFG_EV_QUICK_KICKOUT:
            if(event->kickout)
                status = event->kickout(ifname, &pdata->kickout);
            break;
        case ACFG_EV_ASSOC_FAILURE:
            if(event->assoc_failure)
                status = event->assoc_failure(ifname, &pdata->assoc_fail);
            break;
        case ACFG_EV_DIAGNOSTICS:
            if(event->diagnostics)
                status = event->diagnostics(ifname, &pdata->diagnostics);
            break;
        case ACFG_EV_CHAN_STATS:
            if(event->chan_stats)
                status = event->chan_stats(ifname, &pdata->chan_stats);
            break;
        case ACFG_EV_EXCEED_MAX_CLIENT:
            if(event->exceed_max_client)
                status = event->exceed_max_client(ifname);
            break;
#if defined(OL_ATH_SMART_LOGGING) && !defined(REMOVE_PKT_LOG)
        case ACFG_EV_SMART_LOG_FW_PKTLOG_STOP:
            if(event->smart_log_fw_pktlog_stop)
                status = event->smart_log_fw_pktlog_stop(ifname,
                            &pdata->slfwpktlog_stop);
            break;
#endif /* defined(OL_ATH_SMART_LOGGING) && !defined(REMOVE_PKT_LOG) */
    }

    return status ;
}

uint32_t
acfg_ctrl_iface_detach (int32_t sock)
{
    char cmd[255], replybuf[255] = {0};
    int32_t len  = 0;
    uint32_t status = QDF_STATUS_SUCCESS;

    snprintf(cmd, sizeof(cmd), "%s", WPA_CTRL_IFACE_DETACH_CMD);

    len = sizeof (replybuf);

    if (acfg_ctrl_iface_send(sock, cmd, strlen(cmd), replybuf, &len) < 0) {
        return QDF_STATUS_E_FAILURE;
    }
    if (strncmp(replybuf, "OK", 2)) {
        return QDF_STATUS_E_FAILURE;
    }
    return status;
}

uint32_t
acfg_ctrl_iface_attach (int32_t sock)
{
    char cmd[255], replybuf[255] = {0};
    int32_t len  = 0;
    uint32_t status = QDF_STATUS_SUCCESS;

    snprintf(cmd, sizeof(cmd), "%s", WPA_CTRL_IFACE_ATTACH_CMD);
    len = sizeof (replybuf);
    if (acfg_ctrl_iface_send(sock, cmd, strlen(cmd), replybuf, &len) < 0) {
        acfg_log_errstr("Send failed\n");
        return QDF_STATUS_E_FAILURE;
    }
    if (strncmp(replybuf, "OK", 2)) {
        return QDF_STATUS_E_FAILURE;
    }
    return status;
}


int32_t
acfg_open_ev_sock (struct wsupp_ev_handle *wsupp_h)
{
    acfg_opmode_t opmode;

    if (acfg_get_opmode(wsupp_h->ifname, &opmode) != QDF_STATUS_SUCCESS) {
        acfg_log_errstr("Opmode get failed\n");
        return -1;
    }
    if (opmode == ACFG_OPMODE_STA) {
        wsupp_h->sock = acfg_ctrl_iface_open(wsupp_h->ifname,
                ctrl_wpasupp, wsupp_h);
    }
    else if (opmode == ACFG_OPMODE_HOSTAP) {
        wsupp_h->sock = acfg_ctrl_iface_open(wsupp_h->ifname,
                ctrl_hapd, wsupp_h);
    }
    if (acfg_ctrl_iface_attach(wsupp_h->sock) != QDF_STATUS_SUCCESS) {
        return -1;
    }
    return wsupp_h->sock;
}

uint32_t
acfg_close_ev_sock (struct wsupp_ev_handle wsupp_h)
{
    uint32_t status = QDF_STATUS_SUCCESS;

    if (acfg_ctrl_iface_detach(wsupp_h.sock) != QDF_STATUS_SUCCESS) {
        status = QDF_STATUS_E_FAILURE;
    }
    acfg_close_ctrl_sock(wsupp_h);

    return status;
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
uint32_t
acfg_recv_events(acfg_event_t *event, \
        acfg_event_mode_t  mode)
{
    struct acfg_rtnl_hdl    rth;
    struct wsupp_ev_handle wsupp_h[ACFG_MAX_VAPS];
    uint32_t status = QDF_STATUS_SUCCESS;
    int ret = 0 ;
    int32_t wsupp_sock, app_sock = -1, i;
    void (*old_handler)(int sig) ;
    acfg_vap_list_t list;
    struct wsupp_ev_handle app_sock_h;

    if(mode == ACFG_EVENT_NOBLOCK)
        return QDF_STATUS_E_NOSUPPORT ;
    /* Open netlink channel */
    if(acfg_rt_nl_open(&rth, RTMGRP_NOTIFY) < 0) {
        return QDF_STATUS_E_FAILURE ;
    }
    app_sock = acfg_app_open_socket(&app_sock_h);
    if (app_sock < 0) {
        acfg_rt_nl_close(&rth);
        return QDF_STATUS_E_FAILURE;
    }

    acfg_get_ctrl_iface_path(ACFG_CONF_FILE, ctrl_hapd,
            ctrl_wpasupp);
    old_handler = signal(SIGINT, sig_handler) ;
    if(old_handler == SIG_ERR)
    {
        acfg_log_errstr("%s(): unable to register signal handler \n",__FUNCTION__);
        acfg_rt_nl_close(&rth);
        acfg_close_ctrl_sock(app_sock_h);
        return -1;
    }

    loop_for_events = 1;
    while (1) {
        acfg_get_iface_list(&list, &list.num_iface);
        for (i = 0; i < list.num_iface; i++) {
            snprintf((char *)wsupp_h[i].ifname, sizeof(wsupp_h[i].ifname),
                     "%s", list.iface[i]);
            wsupp_sock = acfg_open_ev_sock(&wsupp_h[i]);
            if (wsupp_sock < 0) {
                wsupp_h[i].sock = -1;
                continue;
            }
            wsupp_h[i].sock = wsupp_sock;
            if (acfg_get_opmode(wsupp_h[i].ifname,
                        &wsupp_h[i].opmode) != QDF_STATUS_SUCCESS)
            {
                continue;
            }
        }
        ret = wait_for_event_all(wsupp_h, list.num_iface, &rth,
                app_sock, event);
        for (i = 0; i < list.num_iface; i++) {
            if (wsupp_h[i].sock > 0) {
                acfg_close_ev_sock(wsupp_h[i]);
            }
        }
        if (ret == 2) {
            break;
        }
    }
    signal(SIGINT, old_handler) ;
    acfg_rt_nl_close(&rth);
    acfg_close_ctrl_sock(app_sock_h);
    if(ret == 2)
        status = QDF_STATUS_E_SIG ;
    return status;
}
