/*
 * Copyright (c) 2018 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

/* C and system library includes */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <sys/types.h>
#include <linux/if.h>

#include <ieee80211_band_steering_api.h>

#ifndef WME_NUM_AC
#define WME_NUM_AC  4
#endif

#ifndef IEEE80211_ADDR_LEN
#define IEEE80211_ADDR_LEN  6
#endif

#include  <ath_ald_external.h>

#define NLINK_MSG_LEN 4*1024

void printBSEventType(int32_t event);
void printALDEventType(int32_t event);

static int32_t createNLSock(int protocoltype)
{
    int32_t nl_sock;

    // create socket with specific protocol type
    if ((nl_sock = socket(PF_NETLINK, SOCK_RAW, protocoltype)) < 0)
    {
        fprintf(stderr, "%s: socket creation failed: %s\n", __func__, strerror(errno));
        goto out;
    }

    /*set socket non-blocking
    if (fcntl(nl_sock, F_SETFL, O_NONBLOCK | fcntl(nl_sock, F_GETFL)))
    {
        fprintf(stderr, "%s: fcntl failed: %s\n", __func__, strerror(errno));
        goto out;
    }
    */
    return nl_sock;

out:
    if (nl_sock > 0)
        close(nl_sock);
    return -1;
}

static int32_t bindNLSocket(int32_t nl_sock)
{
    struct sockaddr_nl src_addr;
    src_addr.nl_family = AF_NETLINK;
    src_addr.nl_pid = getpid();
    src_addr.nl_groups = 1;     //multicast group 1
    if(bind(nl_sock, (struct sockaddr *)&src_addr, sizeof(src_addr)) < 0)
    {
        fprintf(stderr, "%s: socket bind failed: %s\n", __func__, strerror(errno));
        goto err;
    }
    return nl_sock;

err:
    close(nl_sock);
    return -1;
}

static void closeNLSocket(int32_t nl_sock)
{
    close(nl_sock);
}

void *threadfn1(void *arg)
{
    struct nlmsghdr *nlh;
    struct msghdr msg;
    struct iovec iov;

    nlh = (struct nlmsghdr *) malloc(NLMSG_SPACE(NLINK_MSG_LEN));
    if (!nlh)
        return NULL;
    memset(nlh, 0, NLMSG_SPACE(NLINK_MSG_LEN));
    nlh->nlmsg_pid = getpid();
    nlh->nlmsg_len = NLMSG_SPACE(NLINK_MSG_LEN);
    nlh->nlmsg_flags = 0;

    iov.iov_base = (void *)nlh;
    iov.iov_len = nlh->nlmsg_len;

    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    int32_t nl_sock  = *((int32_t *)arg);
    int mylen = 0, k = 0;

    while (1)
    {
        mylen = 0;
        mylen = recvmsg(nl_sock, &msg, 0);

        int32_t *ptr = (int32_t *)NLMSG_DATA(nlh);
        fprintf(stderr, "Received BS Message, len: %d, ", mylen);

        int32_t eventtype = ptr[0];
        printBSEventType(eventtype);

        // print a few bytes of message
        for (k = 0; k < 24; k++)
            printf("%02x", ((char *)ptr)[k]);
        printf("\n");
    }

    free(nlh);
}

void *threadfn2(void *arg)
{
    struct nlmsghdr *nlh;
    struct msghdr msg;
    struct iovec iov;

    nlh = (struct nlmsghdr *) malloc(NLMSG_SPACE(NLINK_MSG_LEN));
    if (!nlh)
        return NULL;
    memset(nlh, 0, NLMSG_SPACE(NLINK_MSG_LEN));
    nlh->nlmsg_pid = getpid();
    nlh->nlmsg_len = NLMSG_SPACE(NLINK_MSG_LEN);
    nlh->nlmsg_flags = 0;

    iov.iov_base = (void *)nlh;
    iov.iov_len = nlh->nlmsg_len;

    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    int32_t nl_sock  = *((int32_t *)arg);
    int mylen = 0, k = 0;

    while (1)
    {
        mylen = 0;
        mylen = recvmsg(nl_sock, &msg, 0);

        int32_t *ptr = (int32_t *)NLMSG_DATA(nlh);
        fprintf(stderr, "Received ALD Message, len: %d, ", mylen);

        int32_t eventtype = ptr[0];
        printALDEventType(eventtype);

        // print a few bytes of message
        for (k = 0; k < 5; k++)
            printf("%02x", ((char *)ptr)[k]);

        if (IEEE80211_ALD_ALL == eventtype)
            printf(" %s", ((char *)ptr)+4);
            printf("\n");
    }

    free(nlh);
}

int main()
{
    int32_t nl_sk1 = createNLSock(NETLINK_BAND_STEERING_EVENT);
    if (nl_sk1 < 0)
    {
        goto out;
    }

    nl_sk1 = bindNLSocket(nl_sk1);
    if (nl_sk1 < 0)
    {
        goto out;
    }

    int32_t *targs1 = (int32_t *)malloc(2*sizeof(int32_t));
    if (targs1 == NULL)
    {
        fprintf(stderr, "%s: malloc failed: %s\n", __func__, strerror(errno));
        closeNLSocket(nl_sk1);
        goto out;
    }
    targs1[0] = nl_sk1;

    pthread_t pid1 = 0;
    if (pthread_create(&pid1, NULL, &threadfn1, (void *)targs1) != 0)
    {
        free(targs1);
        closeNLSocket(nl_sk1);
        goto out;
    }

    int32_t nl_sk2 = createNLSock(NETLINK_ALD);
    if (nl_sk2 < 0)
    {
        goto out;
    }

    nl_sk2 = bindNLSocket(nl_sk2);
    if (nl_sk2 < 0)
    {
        goto out;
    }

    int32_t *targs2 = (int32_t *)malloc(2*sizeof(int32_t));
    if (targs2 == NULL)
    {
        fprintf(stderr, "%s: malloc failed: %s\n", __func__, strerror(errno));
        closeNLSocket(nl_sk2);
        goto out;
    }
    targs2[0] = nl_sk2;

    pthread_t pid2 = 0;
    if (pthread_create(&pid2, NULL, &threadfn2, (void *)targs2) != 0)
    {
        free(targs2);
        closeNLSocket(nl_sk2);
        goto out;
    }

    pthread_join(pid1, NULL);
    pthread_join(pid2, NULL);

    free(targs1);
    free(targs2);

    closeNLSocket(nl_sk1);
    closeNLSocket(nl_sk2);

    return 0;

out:
    return -1;

}

void printBSEventType(int32_t event)
{
    switch (event)
    {
        case ATH_EVENT_BSTEERING_CHAN_UTIL:
            fprintf(stderr, "ATH_EVENT_BSTEERING_CHAN_UTIL ");
            break;

        case ATH_EVENT_BSTEERING_PROBE_REQ:
            fprintf(stderr, "ATH_EVENT_BSTEERING_PROBE_REQ ");
            break;

        case ATH_EVENT_BSTEERING_NODE_ASSOCIATED:
            fprintf(stderr, "ATH_EVENT_BSTEERING_NODE_ASSOCIATED ");
            break;

        case ATH_EVENT_BSTEERING_CLIENT_ACTIVITY_CHANGE:
            fprintf(stderr, "ATH_EVENT_BSTEERING_CLIENT_ACTIVITY_CHANGE ");
            break;

        case ATH_EVENT_BSTEERING_TX_AUTH_FAIL:
            fprintf(stderr, "ATH_EVENT_BSTEERING_TX_AUTH_FAIL ");
            break;

        case ATH_EVENT_BSTEERING_DBG_CHAN_UTIL:
            fprintf(stderr, "ATH_EVENT_BSTEERING_DBG_CHAN_UTIL ");
            break;

        case ATH_EVENT_BSTEERING_CLIENT_RSSI_CROSSING:
            fprintf(stderr, "ATH_EVENT_BSTEERING_CLIENT_RSSI_CROSSING ");
            break;

        case ATH_EVENT_BSTEERING_CLIENT_RSSI_MEASUREMENT:
            fprintf(stderr, "ATH_EVENT_BSTEERING_CLIENT_RSSI_MEASUREMENT ");
            break;

        case ATH_EVENT_BSTEERING_DBG_RSSI:
            fprintf(stderr, "ATH_EVENT_BSTEERING_DBG_RSSI ");
            break;

        case ATH_EVENT_BSTEERING_WNM_EVENT:
            fprintf(stderr, "ATH_EVENT_BSTEERING_WNM_EVENT ");
            break;

        case ATH_EVENT_BSTEERING_RRM_REPORT:
            fprintf(stderr, "ATH_EVENT_BSTEERING_RRM_REPORT ");
            break;

        case ATH_EVENT_BSTEERING_RRM_FRAME_REPORT:
            fprintf(stderr, "ATH_EVENT_BSTEERING_RRM_FRAME_REPORT ");
            break;

        case ATH_EVENT_BSTEERING_CLIENT_TX_RATE_CROSSING:
            fprintf(stderr, "ATH_EVENT_BSTEERING_CLIENT_TX_RATE_CROSSING ");
            break;

        case ATH_EVENT_BSTEERING_VAP_STOP:
            fprintf(stderr, "ATH_EVENT_BSTEERING_VAP_STOP ");
            break;

        case ATH_EVENT_BSTEERING_DBG_TX_RATE:
            fprintf(stderr, "ATH_EVENT_BSTEERING_DBG_TX_RATE ");
            break;

        case ATH_EVENT_BSTEERING_TX_POWER_CHANGE:
            fprintf(stderr, "ATH_EVENT_BSTEERING_TX_POWER_CHANGE ");
            break;

        case ATH_EVENT_BSTEERING_STA_STATS:
            fprintf(stderr, "ATH_EVENT_BSTEERING_STA_STATS ");
            break;

        case ATH_EVENT_BSTEERING_SMPS_UPDATE:
            fprintf(stderr, "ATH_EVENT_BSTEERING_SMPS_UPDATE ");
            break;

        case ATH_EVENT_BSTEERING_OPMODE_UPDATE:
            fprintf(stderr, "ATH_EVENT_BSTEERING_OPMODE_UPDATE ");
            break;

        case ATH_EVENT_BSTEERING_DBG_TX_AUTH_ALLOW:
            fprintf(stderr, "ATH_EVENT_BSTEERING_DBG_TX_AUTH_ALLOW ");
            break;

        case ATH_EVENT_BSTEERING_MAP_CLIENT_RSSI_CROSSING:
            fprintf(stderr, "ATH_EVENT_BSTEERING_MAP_CLIENT_RSSI_CROSSING ");
            break;

        default:
            fprintf(stderr, "%s: Unhandled msg: type %u", __func__, event);
            break;
    }
}

void printALDEventType (int32_t event)
{

    switch (event)
    {
        case IEEE80211_ALD_UTILITY:
            fprintf(stderr, "IEEE80211_ALD_UTILITY ");
            break;

        case IEEE80211_ALD_CAPACITY:
            fprintf(stderr, "IEEE80211_ALD_CAPACITY ");
            break;

        case IEEE80211_ALD_LOAD:
            fprintf(stderr, "IEEE80211_ALD_LOAD ");
            break;

         case IEEE80211_ALD_ALL:
            fprintf(stderr, "IEEE80211_ALD_ALL ");
            break;

        case IEEE80211_ALD_MAXCU:
            fprintf(stderr, "IEEE80211_ALD_MAXCU ");
            break;

        case IEEE80211_ALD_ASSOCIATE:
            fprintf(stderr, "IEEE80211_ALD_ASSOCIATE ");
            break;

        case IEEE80211_ALD_BUFFULL_WRN:
            fprintf(stderr, "IEEE80211_ALD_BUFFULL_WRN ");
            break;

        case IEEE80211_ALD_MCTBL_UPDATE:
            fprintf(stderr, "IEEE80211_ALD_MCTBL_UPDATE ");
            break;

        case IEEE80211_ALD_ERROR:
            fprintf(stderr, "IEEE80211_ALD_ERROR ");
            break;
    }
}
