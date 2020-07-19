/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

/* C and system library includes */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <net/if.h>
#include <fcntl.h>
#include <netdb.h>

#define BUFSIZE 1024
#define SERVICE_PORT 5555

/*
 * LISTEN_STATUS - ListenPort API return values:
 *
 * LISTEN_OK: Function succeeded
 * LISTEN_NOK: Function failed
 *
 */
typedef enum
{
    LISTEN_OK = 0,
    LISTEN_NOK = -1

} LISTEN_STATUS;

struct listenConfig {
    unsigned int logServerPort;
};

struct listenState {
    struct listenConfig config;
    int socketFd;
    int lastErrno;
    struct sockaddr_in serverAddr;
    unsigned int seqNum;
} listenState;

static LISTEN_STATUS listenToPort() {
    struct sockaddr_in remaddr; /* remote address */
    socklen_t addrlen = sizeof(remaddr);        /* length of addresses */
    int recvlen;            /* # bytes received */
    unsigned char buf[BUFSIZE]; /* receive buffer */

    if (bind(listenState.socketFd, (struct sockaddr *)&listenState.serverAddr, sizeof(listenState.serverAddr)) < 0) {
        printf("bind failed");
        return 0;
    }

    /* now loop, receiving data and printing what we received */
    for (;;) {
        recvlen = recvfrom(listenState.socketFd, buf, BUFSIZE, 0, (struct sockaddr *)&remaddr, &addrlen);
        if (recvlen > 0) {
            buf[recvlen] = 0;
            printf("%s", buf);
            fflush(stdout);
        }
    }
}

/**
 * @brief Initialize the address to use as the destination for listening to
 *        logging messages.
 *
 * @pre the logServerPort member of the configuration have already been populated
 *
 * @return LISTEN_OK if the address was resolved successfully; otherwise LISTEN_NOK
 */
static LISTEN_STATUS listenInitServerAddr(struct sockaddr_in *serverAddr) {
    memset(serverAddr, 0, sizeof(*serverAddr));
    serverAddr->sin_family = AF_INET;
    serverAddr->sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr->sin_port = htons(listenState.config.logServerPort);

    return LISTEN_OK;
}

/**
 * @brief Open the socket that will be used to receive diagnostic logging
 *        records.
 */
static LISTEN_STATUS listenOpenSocket(void) {
    struct sockaddr_in serverAddr;
    if (listenInitServerAddr(&serverAddr) == LISTEN_NOK) {
        return LISTEN_NOK;
    }
    listenState.serverAddr = serverAddr;

    listenState.socketFd = socket(AF_INET, SOCK_DGRAM, 0);
    if (listenState.socketFd == -1) {
        printf("socket(...) failed");
        return LISTEN_NOK;
    }

    return LISTEN_OK;
}

int main(int argc, char **argv)
{
    char *cmdstr, *portstr;

    listenState.config.logServerPort = SERVICE_PORT;
    cmdstr = argv[1];

    if (!cmdstr)
    {
        printf("Input commands like:\r\n");
        printf("qca_listen_port -P 5555\r\n");
        return 0;
    } else if (!strcmp(cmdstr, "-P")) {
        portstr = argv[2];

        if(!portstr)
        {
            printf("port (%p) is NULL\n", portstr);
            return 0;
        } else {
            listenState.config.logServerPort = atoi(portstr);
        }
    } else {
        printf("Unsupport argv:%s\n", cmdstr);
        return 0;
    }


    if (LISTEN_OK == listenOpenSocket()) {
        listenToPort();
    }
    return 0;
}
