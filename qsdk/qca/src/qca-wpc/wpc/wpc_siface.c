/*
 * Copyright (c) 2011 Qualcomm Atheros, Inc..
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>

#define SOCK_SNDBUF_SIZE 42600
#define BACKLOG 10

int create_wpc_sock()
{
    int wpc_listen_fd, return_status;
    int on = 1,sndbuf = SOCK_SNDBUF_SIZE;
    struct sockaddr_in my_addr;             // my address information
    wpc_listen_fd = socket(PF_INET, SOCK_STREAM, 0); // do some error checking!
    if (wpc_listen_fd < 0) {
        printf("Listening socket errno=%d Please restart\n", wpc_listen_fd);
        return wpc_listen_fd;
    }

    return_status = setsockopt(wpc_listen_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)); //Port reuse option 
    if(return_status < 0) {
        printf("socket option failed\n");
        return return_status;
    }

    return_status = setsockopt(wpc_listen_fd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));
    if(return_status < 0) {
        printf("setsocket sndbuf option failed\n");
        return return_status;
    }

    memset(&my_addr, 0, sizeof(my_addr));
    /* interested in group 1<<0 */
    my_addr.sin_family = AF_INET;           // host byte order
    my_addr.sin_port = htons(5000);      // short, network byte order
    my_addr.sin_addr.s_addr = INADDR_ANY;   // auto-fill with my IP
    return_status = bind(wpc_listen_fd, (struct sockaddr *)&my_addr, sizeof(my_addr));
    if(return_status < 0 ) {
        printf("Bind Error\n");
        return return_status;
    }
    return_status = listen(wpc_listen_fd, BACKLOG);
    if( return_status < 0 ) {
        printf("Listen error\n");
        return return_status;
    }

    return wpc_listen_fd;
}

