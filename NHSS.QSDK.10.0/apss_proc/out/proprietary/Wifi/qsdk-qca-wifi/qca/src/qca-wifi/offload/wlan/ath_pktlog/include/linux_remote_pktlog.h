/*
 * Copyright (c) 2014, 2015, 2017 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary . Qualcomm Innovation Center, Inc.
 *
 * 2014, 2015 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#ifndef _REMOTE_PKTLOG_AC_H_
#define _REMOTE_PKTLOG_AC_H_

#if REMOTE_PKTLOG_SUPPORT

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/netdevice.h>
#include <linux/ip.h>
#include <linux/in.h>
#include <linux/delay.h>
#include <linux/un.h>
#include <linux/unistd.h>
#include <linux/wait.h>
#include <linux/ctype.h>
#include <asm/unistd.h>
#include <net/sock.h>
#include <net/inet_connection_sock.h>
#include <net/request_sock.h>
#include "ol_if_athvar.h"

#define MODULE_NAME "qca_remote_pktlog"
#define DEFAULT_REMOTE_PKTLOG_PORT 2335
#define MAX_SEND_SIZE 1024
#define MAX_LISTEN_CONECTIONS 5
#define PKTLOG_HEADER_SIZE 8
#define WAIT_EVENT_TIMEOUT_MAX 3
#define WAIT_EVENT_TIMEOUT_MIN 2
#define TRUE 1

/* remote packet log service */
struct pktlog_remote_service {
    qdf_work_t   connection_service; /* WORKQ for connection service */
    qdf_work_t   accept_service;  /* WORKQ for accept service */
    qdf_work_t   send_service;    /* WORKQ for data send serivce */
    struct socket *listen_socket;    /* listen socket */
    struct socket *send_socket;      /* send socket */
    uint16_t port;                   /* port used for sending data */
    int running;                     /* flag for remote packet log service */
    int connect_done;                /* flag for connection done  */
    uint32_t missed_records;         /* records that are not able to send */
    uint32_t fend_counts;            /* file end counts */
};

/*
 * Function     : pktlog_remote_enable
 * Description  : This function initializes remote packetlog
                  service structures and schedules connection service.
 * Input        : pointer to context (scn).
 * Return       : 0 on success, negative error code on failure.
 */
int pktlog_remote_enable(struct ol_ath_softc_net80211 *scn, int enable);

/*
 * Function     : pktlog_start_remote_service - WorkQueue
 * Description  : This function creates remote packetlog socket 
                  and schedules accept service.
 * Input        : pointer to context (scn).
 * Return       : None
 */
void pktlog_start_remote_service(void *context);

/*
 * Function     : pktlog_start_accept_service - WorkQueue
 * Description  : This function accepts incomming connections
                  to remote packetlog socket and schedule send
                  service.
 * Input        : pointer to context (scn).
 * Return       : None
 */
void pktlog_start_accept_service(void *context);

/*
 * Function     : pktlog_run_send_service - WorkQueue
 * Description  : This is main core function that manipulates packetlog read,
                  write buffers and take decession on how much data we need 
                  to send via sockets.
 * Input        : pointer to context (scn).
 * Return       : None
 */
void pktlog_run_send_service(void *context);

/*
 * Function     : pktlog_remote_service_send
 * Description  : This function sends packetlog buffer to sockets.
 * Input        : pointer to packetlog remote service , buffer and length.
 * Return       : size of data that sent through socket.
 */
int pktlog_remote_service_send(struct pktlog_remote_service *service, char *buf, int len); 

/*
 * Function     : stop_service
 * Description  : stop remote packet log service.
 *                release allocated resources like sockets etc.
 * Input        : pointer to scn
 * Return       : 0 on success, negative error code on failure.
 */
int stop_service(struct ol_ath_softc_net80211 * scn);

#endif
#endif
