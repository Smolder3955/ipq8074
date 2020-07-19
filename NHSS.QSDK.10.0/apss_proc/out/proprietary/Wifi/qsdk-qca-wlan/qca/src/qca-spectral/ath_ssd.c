/*
 * =====================================================================================
 *
 *       Filename:  ath_ssd.c
 *
 *    Description:  Spectral daemon for Atheros UI
 *
 *        Version:  1.0
 *        Created:  12/13/2011 03:58:28 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  S.Karthikeyan ()
 *        Company:  Qualcomm Atheros
 *
 *        Copyright (c) 2012-2018 Qualcomm Technologies, Inc.
 *
 *        All Rights Reserved.
 *        Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 *        2012-2016 Qualcomm Atheros, Inc.
 *
 *        All Rights Reserved.
 *        Qualcomm Atheros Confidential and Proprietary.
 *
 * =====================================================================================
 */


#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/netlink.h>
#include <netdb.h>
#include <ctype.h>

#include "classifier.h"
#include "ath_ssd_defs.h"
#include "spectral_data.h"
#include "spec_msg_proto.h"
#include "ath_classifier.h"
#include "spectral_ioctl.h"

int dot11g_channels[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
int dot11a_channels[] = {36, 40, 44, 48, 149, 153, 157, 161, 165}; // Only Non-DFS channels

static ath_ssd_info_t   ath_ssdinfo;
static ath_ssd_info_t*  pinfo = &ath_ssdinfo;

/* Netlink timeout specification (second and microsecond components) */
#define QCA_ATHSSDTOOL_NL_TIMEOUT_SEC         (2)
#define QCA_ATHSSDTOOL_NL_TIMEOUT_USEC        (0)

/* if debug is enabled or not */
int debug   = FALSE;

#ifdef SPECTRAL_SUPPORT_CFG80211
static int init_cfg80211_socket(ath_ssd_info_t *pinfo);
static void destroy_cfg80211_socket(ath_ssd_info_t *pinfo);

/*
 * Function     : init_cfg80211_socket
 * Description  : initialize cfg80211 socket
 * Input params : pointer to ath_ssdinfo
 * Return       : SUCCESS or FAILURE
 *
 */
static int init_cfg80211_socket(ath_ssd_info_t *pinfo)
{
    wifi_cfg80211_context *pcfg80211_sock_ctx = GET_ADDR_OF_CFGSOCKINFO(pinfo);
    int status = SUCCESS;

    if (wifi_init_nl80211(pcfg80211_sock_ctx) != 0) {
        status = FAILURE;
    }
    return status;
}

/*
 * Function     : destroy_cfg80211_socket
 * Description  : destroy cfg80211 socket
 * Input params : pointer to ath_ssdinfo
 * Return       : void
 *
 */
static void destroy_cfg80211_socket(ath_ssd_info_t *pinfo)
{
    wifi_destroy_nl80211(GET_ADDR_OF_CFGSOCKINFO(pinfo));
}
#endif /* SPECTRAL_SUPPORT_CFG80211 */


/*
 * Function     : print_usage
 * Description  : print the athssd usage
 * Input params : void
 * Return       : void
 *
 */
void print_usage(void)
{
    printf("athssd - usage\n");
    line();
    printf("d : enable debug prints\n");
    printf("h : print this help message\n");
    printf("i : radio interface name <wifi0>\n");
    printf("j : interface name <ath0>\n");
    printf("p : play from file <filename>\n");
    printf("s : stand alone spectral scan on channel <channel>\n");
    printf("u : use udp socket\n");
    printf("c : capture None:0 MWO:1 CW:2 WiFi:3 FHSS:4 ALL:5\n");
    printf("x : enable(1)/disable(0) generation III linear bin\n"
           "    format scaling (default: %s). Will be ignored\n"
           "    for other generations.\n",
           (ATH_SSD_ENAB_GEN3_LINEAR_SCALING_DEFAULT) ? "enabled":"disabled");
    line();
    exit(0);
}

/*
 * Function     : issue_start_spectral_cmd
 * Description  : starts spectral scan command
 * Input params : void
 * Return       : void
 *
 */
void issue_start_spectral_cmd(void)
{
    enum ieee80211_cwm_width ch_width;
    ch_width = get_channel_width(pinfo);

    /* save configuration to restore it back after stop scan */
    ath_ssd_get_spectral_param(pinfo, &pinfo->prev_spectral_params);

    switch (ch_width) {
        case IEEE80211_CWM_WIDTH20:
          ath_ssd_set_spectral_param(pinfo, SPECTRAL_PARAM_FFT_SIZE, 7);
          break;
        case IEEE80211_CWM_WIDTH40:
          ath_ssd_set_spectral_param(pinfo, SPECTRAL_PARAM_FFT_SIZE, 8);
          break;
        case IEEE80211_CWM_WIDTH80:
          ath_ssd_set_spectral_param(pinfo, SPECTRAL_PARAM_FFT_SIZE, 9);
          break;
        case IEEE80211_CWM_WIDTH160:
          ath_ssd_set_spectral_param(pinfo, SPECTRAL_PARAM_FFT_SIZE, 9);
          break;
        case IEEE80211_CWM_WIDTHINVALID:
        default:
          printf("Invalid channel width\n");
          return;
          break;
    }

    ath_ssd_set_spectral_param(pinfo, SPECTRAL_PARAM_SPECT_PRI, 1);
    ath_ssd_start_spectral_scan(pinfo);
}

/*
 * Function     : restore_spectral_configuration
 * Description  : Restores the values of spectral parameters
 * Input params : void
 * Return       : void
 *
 */
void restore_spectral_configuration(void)
{
    /* Only the following parameters have been modifed by athssd, write them back */
    ath_ssd_set_spectral_param(pinfo, SPECTRAL_PARAM_SPECT_PRI , pinfo->prev_spectral_params.ss_spectral_pri);
    ath_ssd_set_spectral_param(pinfo, SPECTRAL_PARAM_FFT_SIZE, pinfo->prev_spectral_params.ss_fft_size);
}

/*
 * Function     : issue_stop_spectral_cmd
 * Description  : starts spectral scan command
 * Input params : void
 * Return       : void
 *
 */
void issue_stop_spectral_cmd(void)
{
    ath_ssd_stop_spectral_scan(pinfo);
    restore_spectral_configuration();
}

/*
 * Function     : init_inet_sockinfo
 * Description  : initializes inet socket info
 * Input params : pointer to ath_ssdinfo
 * Return       : SUCCESS or FAILURE
 *
 */
int init_inet_sockinfo(ath_ssd_info_t *pinfo)
{
    int status = SUCCESS;
    ath_ssd_inet_t      *pinet = GET_ADDR_OF_INETINFO(pinfo);

    /* init socket interface */
    pinet->listener = socket(PF_INET, SOCK_STREAM, 0);

    /* validate */
    if (pinet->listener < 0) {
        perror("unable to open socket\n");
        status = FAILURE;
    }

    /* set socket option : Reuse */
    if (setsockopt(pinet->listener, SOL_SOCKET, SO_REUSEADDR, &pinet->on, sizeof(pinet->on)) < 0) {
        perror("socket option failed\n");
        close(pinet->listener);
        status = FAILURE;
    }

    /* initialize ..... */
    memset(&pinet->server_addr, 0, sizeof(pinet->server_addr));
    pinet->server_addr.sin_family  = AF_INET;
    pinet->server_addr.sin_port    = htons(ATHPORT);
    pinet->server_addr.sin_addr.s_addr = INADDR_ANY;
    pinet->type = SOCK_TYPE_TCP;

    /* bind the listener socket */
    if (bind(pinet->listener, (struct sockaddr*)&pinet->server_addr, sizeof(pinet->server_addr)) < 0) {
        perror("bind error\n");
        close(pinet->listener);
        status = FAILURE;
    }

    /* start listening */
    if (listen(pinet->listener, BACKLOG) == -1) {
        perror("listen error\n");
        close(pinet->listener);
        status = FAILURE;
    }

    if (status) {
        info("socket init done");
    } else {
        info("socket init fail");
    }

    return status;
}

/*
 * Function     : init_inet_dgram_sockinfo
 * Description  : initializes netlink socket info
 * Input params : pointer to ath_ssdinfo
 * Return       : SUCCESS or FAILURE
 *
 */
int init_inet_dgram_sockinfo(ath_ssd_info_t *pinfo)
{
    int status = SUCCESS;
    ath_ssd_inet_t  *pinet = GET_ADDR_OF_INETINFO(pinfo);

    /* init socket interface */
    pinet->listener = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (pinet->listener < 0) {
        perror("unable to open socket\n");
        status = FAILURE;
    }

    /* initialize..... */
    memset(&pinet->server_addr, 0, sizeof(pinet->server_addr));
    pinet->server_addr.sin_family   = AF_INET;
    pinet->server_addr.sin_port     = htons(ATHPORT);
    pinet->server_addr.sin_addr.s_addr = INADDR_ANY;

    pinet->type = SOCK_TYPE_UDP;
    pinet->client_fd = INVALID_FD;

    /* bind the listener socket */
    if (bind(pinet->listener, (struct sockaddr*)&pinet->server_addr, sizeof(pinet->server_addr)) < 0) {
        perror("bind error\n");
        close(pinet->listener);
        status = FAILURE;
    }

    if (status) {
        info("udp socket init done");
    } else {
        info("udp socket init fail");
    }

    return status;
}


/*
 * Function     : init_nl_sockinfo
 * Description  : initializes netlink socket info
 * Input params : pointer to ath_ssdinfo
 * Return       : SUCCESS or FAILURE
 *
 */
int init_nl_sockinfo(ath_ssd_info_t *pinfo)
{
    int status = SUCCESS;
    ath_ssd_nlsock_t *pnl = GET_ADDR_OF_NLSOCKINFO(pinfo);

    /* init netlink connection to spectral driver */
    pnl->spectral_fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_ATHEROS);

    /* validate ..... */
    if (pnl->spectral_fd < 0) {
        perror("netlink error\n");
        status = FAILURE;
    }

    /* init netlink socket */
    memset(&pnl->src_addr, 0, sizeof(pnl->src_addr));
    pnl->src_addr.nl_family  = PF_NETLINK;
    pnl->src_addr.nl_pid     = getpid();
    pnl->src_addr.nl_groups  = 1;

    /* bind to the kernel sockets */
    if (bind(pnl->spectral_fd, (struct sockaddr*)&pnl->src_addr, sizeof(pnl->src_addr)) < 0) {
        perror("netlink bind error\n");
        close(pnl->spectral_fd);
        status = FAILURE;
    }

    if (status) {
        info("netlink socket init done");
    } else {
        info("netlink socket init fail");
    }

    return status;
}

/*
 * Function     : accept_new_connection
 * Description  : accepts new client connections
 * Input params : pointer to ath_ssdinfo
 * Return       : SUCCESS or FAILURE
 *
 */
int accept_new_connection(ath_ssd_info_t *pinfo)
{
    int status = SUCCESS;
    ath_ssd_inet_t *pinet = GET_ADDR_OF_INETINFO(pinfo);

    pinet->addrlen = sizeof(pinet->client_addr);

    if ((pinet->client_fd = accept(pinet->listener, (struct sockaddr*)&pinet->client_addr, &pinet->addrlen)) == -1) {
            perror("unable to accept connection\n");
            status = FAILURE;
    }

    if (status) {
      info("new connection from %s on socket %d\n",\
            inet_ntoa(pinet->client_addr.sin_addr), pinet->client_fd);
    }

    return status;
}

/*
 * Function     : get_iface_macaddr
 * Description  : get MAC address of interface
 * Input params : interface name
 * Output params: MAC address filled into pointer to buffer
 *                passed, on success
 * Return       : SUCCESS/FAILURE
 *
 */
int get_iface_macaddr(char *ifname, u_int8_t *macaddr)
{
    struct ifreq  ifr;
    int fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);

    if (ifname == NULL || macaddr == NULL) {
        close(fd);
        return FAILURE;
    }

    if (strlcpy(ifr.ifr_name, ifname, IFNAMSIZ) >= IFNAMSIZ) {
        fprintf(stderr, "ifname too long: %s\n", ifname);
        close(fd);
        return FAILURE;
    }

    if(ioctl(fd, SIOCGIFHWADDR, &ifr) < 0)
    {
        perror("SIOCGIFHWADDR");
        close(fd);
        return FAILURE;
    }

    memcpy(macaddr, ifr.ifr_hwaddr.sa_data, 6);
    close(fd);

    return SUCCESS;
}

/*
 * Function     : new_process_spectral_msg
 * Description  : send data to client
 * Input params : pointer to ath_ssd_info_t, pointer to msg, msg length
 * Return       : SUCCESS or FAILURE
 *
 */
void new_process_spectral_msg(ath_ssd_info_t *pinfo, SPECTRAL_SAMP_MSG* msg)
{
    int count = 0;
    struct INTERF_SRC_RSP   *rsp    = NULL;
    struct INTERF_RSP*interf_rsp    = NULL ;
    CLASSIFER_DATA_STRUCT *pclas    = NULL;
    SPECTRAL_SAMP_DATA *ss_data     = NULL;


    /* validate */
    if (msg->signature != SPECTRAL_SIGNATURE) {
        return;
    }

    if (memcmp(pinfo->radio_macaddr, msg->macaddr,
                MIN(sizeof(pinfo->radio_macaddr), sizeof(msg->macaddr)))) {
        return;
    }

    ss_data     = &msg->samp_data;
    rsp         = &ss_data->interf_list;
    interf_rsp  = &rsp->interf[0];
    pclas       = get_classifier_data(msg->macaddr);

    classifier_process_spectral_msg(msg, pclas, pinfo->log_mode,
            pinfo->enable_gen3_linear_scaling);

    if (IS_MWO_DETECTED(pclas)) {
        interf_rsp->interf_type = INTERF_MW;
        interf_rsp++;
        count++;
    }


    if (IS_CW_DETECTED(pclas)) {
        interf_rsp->interf_type = INTERF_TONE;
        interf_rsp++;
        count++;
    }

    if (IS_WiFi_DETECTED(pclas)) {
        interf_rsp->interf_type = INTERF_WIFI;
        interf_rsp++;
        count++;
    }

    if (IS_CORDLESS_24_DETECTED(pclas)) {
        interf_rsp->interf_type = INTERF_CORDLESS_2GHZ;
        interf_rsp++;
        count++;
    }

    if (IS_CORDLESS_5_DETECTED(pclas)) {
        interf_rsp->interf_type = INTERF_CORDLESS_5GHZ;
        interf_rsp++;
        count++;
    }

    if (IS_BT_DETECTED(pclas)) {
        interf_rsp->interf_type = INTERF_BT;
        interf_rsp++;
        count++;
    }

    if (IS_FHSS_DETECTED(pclas)) {
        interf_rsp->interf_type = INTERF_FHSS;
        interf_rsp++;
        count++;
    }

    /* update the interference count */
    rsp->count = htons(count);

}



/*
 * Function     : send_to_client
 * Description  : send data to client
 * Input params : pointer to ath_ssd_info_t, pointer to msg, msg length
 * Return       : SUCCESS or FAILURE
 *
 */
int send_to_client(ath_ssd_info_t *pinfo, SPECTRAL_SAMP_MSG* ss_msg, int len)
{
    int err = -1;
    ath_ssd_inet_t *pinet = GET_ADDR_OF_INETINFO(pinfo);

    if (CONFIGURED_SOCK_TYPE(pinfo) == SOCK_TYPE_UDP) {

          err = sendto(pinet->listener, ss_msg, len, 0,
                     (struct sockaddr*)&pinet->peer_addr,
                     pinet->peer_addr_len );

    } else if (CONFIGURED_SOCK_TYPE(pinfo) == SOCK_TYPE_TCP) {

          err = send(pinet->client_fd, ss_msg, len, 0);

    }

    return err;
}
/*
 * Function     : handle_spectral_data
 * Description  : receive data from spectral driver
 * Input params : pointer to ath_ssdinfo
 * Return       : SUCCESS or FAILURE
 *
 */
int handle_spectral_data(ath_ssd_info_t *pinfo)
{

    ath_ssd_nlsock_t *pnl = GET_ADDR_OF_NLSOCKINFO(pinfo);
    ath_ssd_stats_t *pstats = GET_ADDR_OF_STATS(pinfo);
    struct nlmsghdr  *nlh = NULL;
    struct msghdr     msg;
    struct iovec      iov;
    int status = SUCCESS;
    int err = SUCCESS;
    int sockerr = 0;
    static int msg_pace_rate = 0;

    SPECTRAL_SAMP_MSG *ss_msg = NULL;
    SPECTRAL_SAMP_DATA  *ss_data = NULL;

    if (!(nlh = (struct nlmsghdr*)malloc(NLMSG_SPACE(sizeof(SPECTRAL_SAMP_MSG))))) {
        perror("no memory");
        status = FAILURE;
        return status;
    }

    memset(nlh, 0, NLMSG_SPACE(sizeof(SPECTRAL_SAMP_MSG)));
    nlh->nlmsg_len  = NLMSG_SPACE(sizeof(SPECTRAL_SAMP_MSG));
    nlh->nlmsg_pid  = getpid();
    nlh->nlmsg_flags = 0;

    iov.iov_base = (void *)nlh;
    iov.iov_len  = nlh->nlmsg_len;

    memset(&pnl->dst_addr, 0, sizeof(pnl->dst_addr));

    pnl->dst_addr.nl_family = PF_NETLINK;
    pnl->dst_addr.nl_pid    = 0;
    pnl->dst_addr.nl_groups = 1;

    memset(&msg, 0, sizeof(msg));
    msg.msg_name = (void*)&pnl->dst_addr;
    msg.msg_namelen = sizeof(pnl->dst_addr);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    /* receive spectral data from spectral driver */
    sockerr = recvmsg(pnl->spectral_fd, &msg, MSG_WAITALL);

    ss_msg = (SPECTRAL_SAMP_MSG*)NLMSG_DATA(nlh);
    ss_data = &ss_msg->samp_data;

    /* mute compiler number */
    ss_data = ss_data;


    if (sockerr >= 0) {
        if (nlh->nlmsg_len) {
            new_process_spectral_msg(pinfo, ss_msg);
            msg_pace_rate++;
            if (msg_pace_rate == MSG_PACE_THRESHOLD) {
                send_to_client(pinfo, ss_msg, sizeof(SPECTRAL_SAMP_MSG));
                pstats->ch[pinfo->current_channel].sent_msg++;
                msg_pace_rate = 0;
            }

        }

        if (err == -1) {
            perror("send err");
            status = FAILURE;
        }
    } else if ((sockerr < 0) && (sockerr != EINTR))  {
        //perror("recvmsg");
    }

    /* free the resource */
    free(nlh);

    return status;
}

/*
 * Function     : init_inet_sockinfo
 * Description  : initializes inet socket info
 * Input params : pointer to ath_ssdinfo
 * Return       : SUCCESS or FAILURE
 *
 */
int handle_client_data(ath_ssd_info_t *pinfo, int fd)
{
    int recvd_bytes = 0;
    int err = 0;

    ath_ssd_inet_t *pinet = GET_ADDR_OF_INETINFO(pinfo);

    struct TLV *tlv              = NULL;
    int buf[MAX_PAYLOAD] = {'\0'};
    int tlv_len = 0;

    if (CONFIGURED_SOCK_TYPE(pinfo) == SOCK_TYPE_UDP) {
        char host[NI_MAXHOST];
        char service[NI_MAXSERV];
        pinet->peer_addr_len = sizeof(pinet->peer_addr);
        recvd_bytes = recvfrom(fd, buf, sizeof(buf), 0,
            (struct sockaddr*)&pinet->peer_addr, &pinet->peer_addr_len);

        getnameinfo((struct sockaddr*)&pinet->peer_addr,
            pinet->peer_addr_len, host, NI_MAXHOST, service,
            NI_MAXSERV, NI_NUMERICSERV);

        if (recvd_bytes == -1) {
            /* ignore failed attempts */
            return 0;
        }
     } else if (CONFIGURED_SOCK_TYPE(pinfo) == SOCK_TYPE_TCP) {
        if ((recvd_bytes = recv(fd, buf, sizeof(buf), 0)) <= 0) {
            perror("recv error");
            err = -1;
        }
     }

    tlv = (struct TLV*)buf;
    tlv_len = htons(tlv->len);

    /* mute compiler warning */
    tlv_len = tlv_len;

    switch (tlv->tag) {
        case START_SCAN:
            start_spectral_scan(pinfo);
            break;
        case STOP_SCAN:
            stop_spectral_scan(pinfo);
            break;
        case START_CLASSIFY_SCAN:
            start_classifiy_spectral_scan(pinfo);
            break;
        default:
            printf("Tag (%d) Not supported\n", tlv->tag);
            break;
    }

    return err;
}

/*
 * Function     : update_next_channel
 * Description  : switch to next channel; while doing the scan
 * Input params : pointer to ath_ssd_info_t
 * Return       : void
 *
 */
void update_next_channel(ath_ssd_info_t *pinfo)
{
    if ((pinfo->channel_index >= pinfo->max_channels)) {
        pinfo->channel_index = 0;
    }
    pinfo->current_channel = pinfo->channel_list[pinfo->channel_index++];
}

/*
 * Function     : stop_spectral_scan
 * Description  : stop the ongoing spectral scan
 * Input params : pointer to ath_ssd_info_t
 * Return       : void
 *
 */
void stop_spectral_scan(ath_ssd_info_t *pinfo)
{
    pinfo->channel_index = 0;
    ath_ssd_stop_spectral_scan(pinfo);
    restore_spectral_configuration();
    alarm(0);
}

/*
 * Function     : switch_channel
 * Description  : swith channel; do necessary initialization
 * Input params : pointer to ath_ssd_info_t
 * Return       : void
 *
 */
void switch_channel(ath_ssd_info_t *pinfo)
{
    char cmd[CMD_BUF_SIZE] = {'\0'};

    /* disable timer */
    alarm(0);

    /* get the next channel to scan */
    update_next_channel(pinfo);


    /* change the channel */
    info("current channel = %d\n", pinfo->current_channel);
    snprintf(cmd, sizeof(cmd), "%s %s %s %1d", "iwconfig", pinfo->dev_ifname, "channel", pinfo->current_channel);
    system(cmd);

    pinfo->init_classifier = TRUE;
}

/*
 * Function     : start_spectral_scan
 * Description  : start the spectral scan
 * Input params : pointer to ath_ssd_info_t
 * Return       : void
 *
 */
void start_spectral_scan(ath_ssd_info_t *pinfo)
{
    pinfo->channel_index   = 0;
    pinfo->dwell_interval  = CHANNEL_NORMAL_DWELL_INTERVAL;
    switch_channel(pinfo);
    issue_start_spectral_cmd();
    alarm(pinfo->dwell_interval);
}

/*
 * Function     : start_classifiy_spectral_scan
 * Description  : start the classify spectral scan
 * Input params : pointer to ath_ssd_info_t
 * Return       : void
 *
 */
void start_classifiy_spectral_scan(ath_ssd_info_t *pinfo)
{
    pinfo->channel_index   = 0;
    pinfo->dwell_interval  = CHANNEL_CLASSIFY_DWELL_INTERVAL;
    switch_channel(pinfo);
    issue_start_spectral_cmd();
    alarm(pinfo->dwell_interval);
}

/*
 * Function     : print_interf_details
 * Description  : prints interference info for give type
 * Input params : pointer to ath_ssd_info_t
 * Return       : void
 *
 */
char interf_name[6][32] = {
    "None",
    "Microwave",
    "Bluetooth",
    "DECT phone",
    "Tone",
    "Other",
};
void print_interf_details(ath_ssd_info_t *pinfo, eINTERF_TYPE type)
{
    ath_ssd_interf_info_t *p = &pinfo->interf_info;
    struct INTERF_RSP *interf_rsp = NULL;

    if ((type < INTERF_NONE) || (type > INTERF_OTHER)) {
        return;
    }

    interf_rsp = &p->interf_rsp[type];

    line();
    printf("Interference Type       = %s\n", interf_name[type]);
    printf("Interference min freq   = %d\n", interf_rsp->interf_min_freq);
    printf("Interference max freq   = %d\n", interf_rsp->interf_max_freq);
    line();
}


/*
 * Function     : print_ssd_stats
 * Description  : prints stats info
 * Input params : pointer to ath_ssd_info_t
 * Return       : void
 *
 */
void print_ssd_stats(ath_ssd_info_t *pinfo)
{
    ath_ssd_stats_t *pstats = GET_ADDR_OF_STATS(pinfo);
    int i = 1;

    line();
    printf("Channel/Message Stats\n");
    line();
    for (i = 1; i < MAX_CHANNELS; i++) {
        printf("channel = %2d  sent msg = %llu\n", i, pstats->ch[i].sent_msg);
    }
    line();

}

/*
 * Function     : cleanup
 * Description  : release all socket, memory and others before exiting
 * Input params : pointer to ath_ssd_info_t
 * Return       : void
 *
 */
void cleanup(ath_ssd_info_t *pinfo)
{
    ath_ssd_inet_t *pinet = GET_ADDR_OF_INETINFO(pinfo);
    ath_ssd_nlsock_t *pnl = GET_ADDR_OF_NLSOCKINFO(pinfo);

    if (!pinfo->replay)
    	stop_spectral_scan(pinfo);

    close(pinet->listener);
    close(pinet->client_fd);
    close(pnl->spectral_fd);
#ifdef SPECTRAL_SUPPORT_CFG80211
    if (IS_CFG80211_ENABLED(pinfo)) {
        destroy_cfg80211_socket(pinfo);
    }
#endif /* SPECTRAL_SUPPORT_CFG80211 */
    print_spect_int_stats();
    exit(0);
}

/*
 * Function     : alarm_handler
 * Description  : alarm signal handler, used to switch channel
 * Input params : pointer to ath_ssd_info_t
 * Return       : void
 *
 */
void alarm_handler(ath_ssd_info_t *pinfo)
{
    /* disable the timer */
    alarm(0);
    /* stop any active spectral scan */
    issue_stop_spectral_cmd();

    /* print debug info */
    if (IS_DBG_ENABLED()) {
        print_ssd_stats(pinfo);
    }

    /* switch to new channel */
    switch_channel(pinfo);

    /* enable the timer handler */
    alarm(pinfo->dwell_interval);

    /* start spectral scan */
    issue_start_spectral_cmd();
}

/*
 * Function     : signal_handler
 * Description  : signal handler
 * Input params : pointer to ath_ssd_info_t
 * Return       : void
 *
 */
void signal_handler(int signal)
{
    switch (signal) {
        case SIGHUP:
        case SIGTERM:
        case SIGINT:
            cleanup(pinfo);
            break;
        case SIGALRM:
            alarm_handler(pinfo);
            break;
    }
}

/*
 * Function     : print_spectral_SAMP_msg
 * Description  : print spectral message info
 * Input params : pointer to ath_ssd_info_t
 * Return       : void
 *
 */
void print_spectral_SAMP_msg(SPECTRAL_SAMP_MSG* ss_msg)
{
    int i = 0;

    SPECTRAL_SAMP_DATA *p = &ss_msg->samp_data;
    SPECTRAL_CLASSIFIER_PARAMS *pc = &p->classifier_params;
    struct INTERF_SRC_RSP  *pi = &p->interf_list;

    line();
    printf("Spectral Message\n");
    line();
    printf("Signature   :   0x%x\n", ss_msg->signature);
    printf("Freq        :   %d\n", ss_msg->freq);
    printf("Freq load   :   %d\n", ss_msg->freq_loading);
    printf("Inter type  :   %d\n", ss_msg->int_type);
    line();
    printf("Spectral Data info\n");
    line();
    printf("data length     :   %d\n", p->spectral_data_len);
    printf("rssi            :   %d\n", p->spectral_rssi);
    printf("combined rssi   :   %d\n", p->spectral_combined_rssi);
    printf("upper rssi      :   %d\n", p->spectral_upper_rssi);
    printf("lower rssi      :   %d\n", p->spectral_lower_rssi);
    printf("bw info         :   %d\n", p->spectral_bwinfo);
    printf("timestamp       :   %d\n", p->spectral_tstamp);
    printf("max index       :   %d\n", p->spectral_max_index);
    printf("max exp         :   %d\n", p->spectral_max_exp);
    printf("max mag         :   %d\n", p->spectral_max_mag);
    printf("last timstamp   :   %d\n", p->spectral_last_tstamp);
    printf("upper max idx   :   %d\n", p->spectral_upper_max_index);
    printf("lower max idx   :   %d\n", p->spectral_lower_max_index);
    printf("bin power count :   %d\n", p->bin_pwr_count);
    line();
    printf("Classifier info\n");
    line();
    printf("20/40 Mode      :   %d\n", pc->spectral_20_40_mode);
    printf("dc index        :   %d\n", pc->spectral_dc_index);
    printf("dc in MHz       :   %d\n", pc->spectral_dc_in_mhz);
    printf("upper channel   :   %d\n", pc->upper_chan_in_mhz);
    printf("lower channel   :   %d\n", pc->lower_chan_in_mhz);
    line();
    printf("Interference info\n");
    line();
    printf("inter count     :   %d\n", pi->count);

    for (i = 0; i < pi->count; i++) {
        printf("inter type  :   %d\n", pi->interf[i].interf_type);
        printf("min freq    :   %d\n", pi->interf[i].interf_min_freq);
        printf("max freq    :   %d\n", pi->interf[i].interf_max_freq);
    }

}

/*
 * Function     : clear_interference_info
 * Description  : clear interference related info
 * Input params : pointer to ath_ssd_info_t
 * Return       : void
 *
 */
void clear_interference_info(ath_ssd_info_t *pinfo)
{
    ath_ssd_interf_info_t *p = &pinfo->interf_info;
    p->count = 0;
    memset(p, 0, sizeof(ath_ssd_interf_info_t));
}

/*
 * Function     : update_interf_info
 * Description  : update cumulative interference report
 * Input params : pointer to ath_ssd_info_t, pointer to classifier data
 * Return       : void
 *
 */
int update_interf_info(ath_ssd_info_t *pinfo, struct ss *bd)
{
    int interf_count = 0;
    int num_types_detected = 0;
    ath_ssd_interf_info_t *p = &pinfo->interf_info;
    struct INTERF_RSP *interf_rsp = NULL;

    if (bd->count_mwo) {
        interf_rsp = &p->interf_rsp[INTERF_MW];
        interf_rsp->interf_min_freq = (bd->mwo_min_freq/1000);
        interf_rsp->interf_max_freq = (bd->mwo_max_freq/1000);
        interf_rsp->interf_type = INTERF_MW;
        num_types_detected++;
    }

    if (bd->count_bts) {
        interf_rsp = &p->interf_rsp[INTERF_BT];
        interf_rsp->interf_min_freq = (bd->bts_min_freq/1000);
        interf_rsp->interf_max_freq = (bd->bts_max_freq/1000);
        interf_rsp->interf_type = INTERF_BT;
        num_types_detected++;
    }

    if(bd->count_cph) {
        interf_rsp = &p->interf_rsp[INTERF_DECT];
        interf_rsp->interf_min_freq = (bd->cph_min_freq/1000);
        interf_rsp->interf_max_freq = (bd->cph_max_freq/1000);
        interf_rsp->interf_type = INTERF_DECT;
        num_types_detected++;
    }

    if(bd->count_cwa){
        interf_rsp = &p->interf_rsp[INTERF_TONE];
        interf_rsp->interf_min_freq = (bd->cwa_min_freq/1000);
        interf_rsp->interf_max_freq = (bd->cwa_max_freq/1000);
        interf_rsp->interf_type = INTERF_TONE;
        num_types_detected++;
    }


    interf_count = bd->count_mwo + bd->count_bts + bd->count_bth + bd->count_cwa + bd->count_cph;

    if (interf_count)
        p->count = interf_count;

    return num_types_detected;

}

/*
 * Function     : add_interference_report
 * Description  : add interference related information to SAMP message
 * Input params : pointer to ath_ssd_info_t, pointer to interference response frame
 * Return       : void
 *
 */
void add_interference_report(ath_ssd_info_t *pinfo, struct INTERF_SRC_RSP *rsp)
{
    int i = 0;
    int count = 0;
    struct INTERF_RSP *interf_rsp = NULL;
    ath_ssd_interf_info_t *p = &pinfo->interf_info;

    interf_rsp = &rsp->interf[0];

    for (i = 0; i < MAX_INTERF_COUNT; i++) {
        if (p->interf_rsp[i].interf_type != INTERF_NONE) {
            count++;
            interf_rsp->interf_min_freq = htons(p->interf_rsp[i].interf_min_freq);
            interf_rsp->interf_max_freq = htons(p->interf_rsp[i].interf_max_freq);
            interf_rsp->interf_type     = p->interf_rsp[i].interf_type;
            interf_rsp++;
            /* debug print */
            //print_interf_details(pinfo, i);
        }
    }
    rsp->count = htons(count);
}

/*
 * Function     : start_standalone_spectral_scan
 * Description  : starts standalone spectral scan
 * Input params : channel
 * Return       : void
 *
 */
void start_standalone_spectral_scan(ath_ssd_info_t *pinfo, int channel)
{
    char cmd[CMD_BUF_SIZE] = {'\0'};

    if (channel != 0) {
        pinfo->current_channel = channel;
        /* change the channel */
        snprintf(cmd, sizeof(cmd), "%s %s %s %1d", "iwconfig", pinfo->dev_ifname, "channel", pinfo->current_channel);
        system(cmd);
    }

    pinfo->dwell_interval  = CHANNEL_CLASSIFY_DWELL_INTERVAL;

    /* disable alarm */
    alarm(0);

    /* start spectral scan */
    issue_start_spectral_cmd();

    /* debug print */
    printf("starting a stand alone spectral scan\n");

}
/*
 * Function     : get_next_word
 * Description  : Get next value from SAMP file
 *
 */
#define MAX_WORD_LEN 64


char word[MAX_WORD_LEN] = {'\0'};

char* get_next_word(FILE *file, int *is_eof)
{
    int c;
    int i = 0;

    /* Initialize the word area */
    memset(word, '\0', sizeof(word));

    /* Discard leading spaces or newlines */
    do {
        c = fgetc(file);
    } while (c == ' ' || c == '\n');

    /* Extract the word */
    while ((c != ' ') && (c != '\n') && (c != EOF)) {
        word[i++] = c;
        c = fgetc(file);
    }

    /* Mark EOF */
    if (c == EOF) {
        *is_eof = 1;
    }

    /* null terminate string */
    word[i] = '\0';
    return word;
}


/*
 * Function     : convert_addrstr_to_byte
 * Description  : Convert Address String to Macddr
 *
 */
char separator = ':';
char * convert_addrstr_to_byte(char* addr, char* dst)
{

    int i = 0;

    for (i = 0; i < 6; ++i)
    {
        unsigned int inum = 0;
        char ch;

        ch = tolower(*addr++);

        if ((ch < '0' || ch > '9') && (ch < 'a' || ch > 'f'))
            return NULL;

        inum = isdigit (ch)?(ch - '0'):(ch - 'a' + 10);
        ch = tolower(*addr);

        if ((i < 5 && ch != separator) ||
            (i == 5 && ch != '\0' && !isspace(ch)))
            {
                ++addr;
                if ((ch < '0' || ch > '9') &&
                    (ch < 'a' || ch > 'f'))
                        return NULL;

                inum <<= 4;
                inum += isdigit(ch) ? (ch - '0') : (ch - 'a' + 10);
                ch = *addr;

                if (i < 5 && ch != separator)
                    return NULL;
        }

        dst[i] = (unsigned char)inum;
        ++addr;
    }
    return dst;
}

/*
 * Function     : replay_from_file
 * Description  : Feed SAMP Messages from a file and Classifiy the signal
 * Input params : Pointer to Info and Filename
 * Return       : void
 *

 SAMP FIlE FORMAT
 ----------------

 Line 1 : Version number | MAC Address | Channel Width | Operating Frequency
 Line 2 : Sample 1 related info for primary 80Mhz segment
 Line 3 : Sample 1 related info for secondary 80Mhz segment
 Line 4 : Sample 2 related info for primary 80Mhz segment
 Line 5 : Sample 2 related info for secondary 80Mhz segment
 :
 Line n : Sample n related info

 */
void replay_from_file(ath_ssd_info_t *pinfo)
{
    FILE* pfile = NULL;
    int is_eof = FALSE;
    SPECTRAL_SAMP_MSG msg;
    SPECTRAL_SAMP_DATA *pdata = NULL;
    int i = 0;
    int file_version = 0;
    char macaddr[64] = {'\0'};
    int seg_info = 0; /* Indicates if samples are for primary or secondary 80Mhz segment */

    pfile = fopen(pinfo->filename, "rt");

    if (!pfile) {
        printf("Unable to open %s\n", pinfo->filename);
        return;
    }

    file_version = atoi(get_next_word(pfile, &is_eof));

    if ((file_version != SPECTRAL_LOG_VERSION_ID1) && (file_version != SPECTRAL_LOG_VERSION_ID2)) {
        printf("SAMP File version (%d) not supported\n", file_version);
        fclose(pfile);
        return;
    }

    /* Get the Mac address from file */
    memset(macaddr, 0, sizeof(macaddr));
    memcpy(macaddr, get_next_word(pfile, &is_eof), sizeof(macaddr));

    /* Copy the Macaddress */
    convert_addrstr_to_byte((char*)macaddr, (char*)&msg.macaddr);
    pdata = &msg.samp_data;

    /* Add Signature */
    msg.signature = SPECTRAL_SIGNATURE;

    /* Get the channel width */
    msg.samp_data.ch_width = atoi(get_next_word(pfile, &is_eof));

    /* Get the operating frequency */
    msg.freq = (u_int16_t)atoi(get_next_word(pfile, &is_eof));

    while(TRUE) {
        /* ignore the line number */
        (void)atoi(get_next_word(pfile, &is_eof));
        if (!is_eof) {

          /* Populating primary 80Mhz segment related info */
          if((seg_info % 2 == 0) || (!(msg.samp_data.ch_width == IEEE80211_CWM_WIDTH160))) {
                pdata->bin_pwr_count = (u_int16_t)atoi(get_next_word(pfile, &is_eof));

                for (i = 0; i < pdata->bin_pwr_count; i++) {
                    pdata->bin_pwr[i] = (u_int8_t)atoi(get_next_word(pfile, &is_eof));
                }

                /* populate TS,RSSI and NF */
                pdata->spectral_tstamp = (int)atoi(get_next_word(pfile, &is_eof));
                pdata->spectral_rssi = abs((int16_t)atoi(get_next_word(pfile, &is_eof)));
                pdata->noise_floor = (int16_t)atoi(get_next_word(pfile, &is_eof));
                if (file_version == SPECTRAL_LOG_VERSION_ID2) {
                        pdata->spectral_agc_total_gain =
                                (u_int8_t)atoi(get_next_word(pfile, &is_eof));
                        pdata->spectral_gainchange =
                                (u_int8_t)atoi(get_next_word(pfile, &is_eof));
                }
            }
          /* Populating secondary 80Mhz segment related info only if it is a
             odd numbered sample and the channel width is 160Mhz */
            else {
                pdata->bin_pwr_count_sec80 = (u_int16_t)atoi(get_next_word(pfile, &is_eof));

                for (i = 0; i < pdata->bin_pwr_count_sec80; i++) {
                    pdata->bin_pwr_sec80[i] = (u_int8_t)atoi(get_next_word(pfile, &is_eof));
                }

                /* populate TS,RSSI and NF */
                pdata->spectral_tstamp = (int)atoi(get_next_word(pfile, &is_eof));
                pdata->spectral_rssi_sec80 = abs((int16_t)atoi(get_next_word(pfile, &is_eof)));
                pdata->noise_floor_sec80 = (int16_t)atoi(get_next_word(pfile, &is_eof));
                if (file_version == SPECTRAL_LOG_VERSION_ID2) {
                    pdata->spectral_agc_total_gain_sec80 =
                        (u_int8_t)atoi(get_next_word(pfile, &is_eof));
                    pdata->spectral_gainchange_sec80 =
                        (u_int8_t)atoi(get_next_word(pfile, &is_eof));
                }
            }
            /* Process incoming SAMP Message */
            new_process_spectral_msg(pinfo, &msg);

            seg_info++;
        } else {
            break;
        }
    }
    fclose(pfile);
    cleanup(pinfo);
    return ;
}

#define FILE_NAME_LENGTH 64
#define MAX_WIPHY 3

#ifdef SPECTRAL_SUPPORT_CFG80211
/*
* get_config_mode_type: Function that detects current config type
* and returns corresponding enum value.
*/
static config_mode_type get_config_mode_type()
{
    FILE *fp;
    char filename[FILE_NAME_LENGTH];
    int radio;
    config_mode_type ret = CONFIG_IOCTL;

    for (radio = 0; radio < MAX_WIPHY; radio++) {
        snprintf(filename, sizeof(filename),"/sys/class/net/wifi%d/phy80211/",radio);
        fp = fopen(filename, "r");
        if (fp != NULL){
            fclose(fp);
            ret = CONFIG_CFG80211;
            break;
        }
    }

    return ret;
}
#endif /* SPECTRAL_SUPPORT_CFG80211 */

/*
 * Function     : main
 * Description  : entry point
 * Input params : argc, argv
 * Return       : void
 *
 */

int main(int argc, char* argv[])
{
    int fdmax;
    int fd = 0;
    int ret = 0;
    int channel = INVALID_CHANNEL;
    int do_standalone_scan = FALSE;
    pinfo->replay = FALSE;
    pinfo->enable_gen3_linear_scaling =
        ATH_SSD_ENAB_GEN3_LINEAR_SCALING_DEFAULT;

    int optc;
    char *radio_ifname  = NULL;
    char *dev_ifname    = NULL;
    char *filename      = NULL;
    struct timeval tv_timeout;
    u_int8_t radio_mac_addr[6];
    struct spectral_caps caps;

    fd_set  master;
    fd_set  read_fds;

    ath_ssd_inet_t *pinet = GET_ADDR_OF_INETINFO(pinfo);
    ath_ssd_nlsock_t *pnl = GET_ADDR_OF_NLSOCKINFO(pinfo);

    /* use TCP by default */
    sock_type_t sock_type = SOCK_TYPE_UDP;

#ifdef SPECTRAL_SUPPORT_CFG80211
    /*
     * Based on the driver config mode (cfg80211/wext), application also runs
     * in same mode (wext/cfg80211)
     */
    pinfo->cfg_flag = get_config_mode_type();
#endif /* SPECTRAL_SUPPORT_CFG80211 */

    while ((optc = getopt(argc, argv, "adc:gHhi:j:p:Tts:nx:")) != -1) {
        switch (optc) {
            case 'a':
		info("Option -%c is deprecated as the band information is auto-detected", optc);
		break;
            case 'c':
                pinfo->log_mode = atoi(optarg);
                break;
            case 'd':
                debug = TRUE;
                break;
            case 'g':
		info("Option -%c is deprecated as the band information is auto-detected", optc);
		break;
            case 'T':
            case 't':
                sock_type = SOCK_TYPE_TCP;
                break;
            case 'h':
            case 'H':
                print_usage();
                break;
            case 's':
                channel = atoi(optarg);
                do_standalone_scan = TRUE;
                break;
            case 'i':
                radio_ifname = optarg;
                break;
            case 'j':
                dev_ifname = optarg;
                break;
            case 'p':
                filename = optarg;
                pinfo->replay = TRUE;
                break;
#ifdef SPECTRAL_SUPPORT_CFG80211
            case 'n':
                if (!pinfo->cfg_flag)
                {
                    fprintf(stderr, "Invalid tag '-n' for wext mode.\n");
                    exit(EXIT_FAILURE);
                }
                break;
#endif /* SPECTRAL_SUPPORT_CFG80211 */
            case 'x':
                pinfo->enable_gen3_linear_scaling = !!atoi(optarg);
                break;
            case '?':
                if ((optopt == 's') || (optopt == 'i') || (optopt == 'j') ||
                        (optopt == 'c') || (optopt == 'p') || (optopt == 'x')) {
                    fprintf(stderr, "Option -%c requries an argument.\n", optopt);
                } else {
                    fprintf(stderr, "Unknown option '-%c'.\n", optopt);
                }
                exit(EXIT_FAILURE);
            default:
                break;
        }
    }

    /* init the socket type */
    pinfo->sock_type = sock_type;

    /* init the dwell interval */
    pinfo->dwell_interval = CHANNEL_NORMAL_DWELL_INTERVAL;

    /* init the current channel list */
    if (IS_BAND_5GHZ(pinfo)) {
        pinfo->channel_list = dot11a_channels;
        pinfo->max_channels = ARRAY_LEN(dot11a_channels);
    } else {
        pinfo->channel_list = dot11g_channels;
        pinfo->max_channels = ARRAY_LEN(dot11g_channels);
    }

    /* save the interface name */
    if (radio_ifname) {
        pinfo->radio_ifname = radio_ifname;
    } else {
        pinfo->radio_ifname = DEFAULT_RADIO_IFNAME;
    }

    if (dev_ifname) {
        pinfo->dev_ifname = dev_ifname;
    } else {
        pinfo->dev_ifname = DEFAULT_DEV_IFNAME;
    }

    if (ath_ssd_init_spectral(pinfo) != SUCCESS) {
        exit(EXIT_FAILURE);
    }

#ifdef SPECTRAL_SUPPORT_CFG80211
    if (IS_CFG80211_ENABLED(pinfo)) {
        /* init cfg80211 socket */
        if (init_cfg80211_socket(pinfo) == FAILURE) {
            info("unable to create cfg80211 socket");
            exit(EXIT_FAILURE);
        }
    }

#endif /* SPECTRAL_SUPPORT_CFG80211 */

    if (CONFIGURED_SOCK_TYPE(pinfo) == SOCK_TYPE_TCP) {
        /* init TCP socket interface */
        if (init_inet_sockinfo(pinfo) == FAILURE) {
            exit(EXIT_FAILURE);
        }
    }
    else if (CONFIGURED_SOCK_TYPE(pinfo) == SOCK_TYPE_UDP) {
        /* init UDP socket interface */
        if (init_inet_dgram_sockinfo(pinfo) == FAILURE) {
            exit(EXIT_FAILURE);
        }
    } else {
        info("invalid socket type");
        exit(EXIT_FAILURE);
    }

    memset(radio_mac_addr, 0, sizeof(radio_mac_addr));

    memset(&caps, 0, sizeof(caps));

    /* No need of socket intialization in replay mode */
    if (pinfo->replay) {
        caps.advncd_spectral_cap = true;
        /* TODO: Record and play other capability information. */
    } else {
        if (init_nl_sockinfo(pinfo) == FAILURE) {
            exit(EXIT_FAILURE);
        }

        if (get_iface_macaddr(pinfo->radio_ifname, radio_mac_addr)
                != SUCCESS) {
            exit(EXIT_FAILURE);
        }

        memcpy(pinfo->radio_macaddr, radio_mac_addr,
                MIN(sizeof(pinfo->radio_macaddr), sizeof(radio_mac_addr)));

        if (ath_ssd_get_spectral_capabilities(pinfo, &caps) != SUCCESS) {
            exit(EXIT_FAILURE);
        }
    }

    pinfo->current_band = get_vap_priv_int_param(pinfo,pinfo->dev_ifname,
				IEEE80211_PARAM_FREQ_BAND);
    init_classifier_lookup_tables();
    init_classifier_data(radio_mac_addr, &caps, sizeof(caps));

    FD_ZERO(&master);
    FD_ZERO(&read_fds);

    FD_SET(pnl->spectral_fd, &master);
    FD_SET(pinet->listener, &master);

    fdmax = (pinet->listener > pnl->spectral_fd)?pinet->listener:pnl->spectral_fd;

    signal(SIGINT, signal_handler);
    signal(SIGALRM, signal_handler);
    signal(SIGCHLD, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGHUP, signal_handler);

    /* Replay SAMP messages from a File */
    if (pinfo->replay) {
        info("Replay Mode   : %s", (pinfo->replay == TRUE)?"YES":"NO");
        pinfo->filename = filename;
        replay_from_file(pinfo);
        return 0;
    }

    info("server (built at %s %s )", __DATE__, __TIME__);
    info("interface          : %s", pinfo->radio_ifname);
    info("current band       : %s", (pinfo->current_band == BAND_5GHZ)?"5GHz ":"2.4GHz ");
    info("logging            : %s (%d)", (pinfo->log_mode)?"Enabled":"Disabled", pinfo->log_mode);
    if (caps.hw_gen == SPECTRAL_CAP_HW_GEN_3) {
        info("gen3 linear scaling: %s (%d)",
                (pinfo->enable_gen3_linear_scaling) ? "Enabled" : "Disabled",
                pinfo->enable_gen3_linear_scaling);
    }

    if (do_standalone_scan) {
        start_standalone_spectral_scan(pinfo, channel);
    }

    for (;;) {

        read_fds = master;
        tv_timeout.tv_sec = QCA_ATHSSDTOOL_NL_TIMEOUT_SEC;
        tv_timeout.tv_usec = QCA_ATHSSDTOOL_NL_TIMEOUT_USEC;

        ret = select(fdmax + 1, &read_fds, NULL, NULL, &tv_timeout);

        if (ret == -1) {
            continue;
        }
        else if(ret == 0){
            printf("Timed out waiting for spectral message.\n");
            cleanup(pinfo);
            break;
        }

        for (fd = 0; fd <= fdmax; fd++) {
            if (FD_ISSET(fd, &read_fds)) {
                if (fd ==  pinet->listener ) {

                    if (CONFIGURED_SOCK_TYPE(pinfo) == SOCK_TYPE_UDP) {
                        if (handle_client_data(pinfo, fd) == -1) {
                            cleanup(pinfo);
                            exit(EXIT_FAILURE);
                        }
                    } else if (CONFIGURED_SOCK_TYPE(pinfo) == SOCK_TYPE_TCP) {
                        if (accept_new_connection(pinfo)) {
                            FD_SET(pinet->client_fd, &master);
                            fdmax = (pinet->client_fd > fdmax)?pinet->client_fd:fdmax;
                        }
                    }
                }
                else if (fd ==  pnl->spectral_fd) {
                        if (handle_spectral_data(pinfo) == FAILURE) {
                            cleanup(pinfo);
                            exit(EXIT_FAILURE);
                        }
                }
                else if ((fd == pinet->client_fd) &&
                         (CONFIGURED_SOCK_TYPE(pinfo) == SOCK_TYPE_TCP)) {
                        if (handle_client_data(pinfo, fd ) == -1) {
                            cleanup(pinfo);
                            exit(EXIT_FAILURE);
                        }
                }
            }
        }
    }
    return 0;
}

