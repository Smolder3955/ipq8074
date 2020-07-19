/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.

*/

/*-M- SonEven -- use for customer daemon to communication with HYD.
 * Provide five functions for customer daemon use.
 * wifison_event_init : init a socket and send a START event to HYD.
 *                     The socket will recevie any message send by HYD.
 * wifison_event_deinit : Destory the socket and send STOP event to HYD.
 * wifison_event_register : Register events which want HYD to supported.
 * wifison_event_deregister : Deregister events and HYD will stop to response it.
 * wifison_event_get : Get the event notification from HYD.
 *  */

/*===========================================================================*/
/*================= Includes and Configuration ==============================*/
/*===========================================================================*/


/* C and system library includes */
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <memory.h>
#include <signal.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "wifison_event.h"


struct soneventAppState {
    int socket;
    bool eventrigerflag[SON_MAX_EVENT];
}soneventAppS;

struct sonEventClient clients[] = {
    {
        SON_EVENT_CLIENT_HYD,
        SONE_EVENT_SOCKET_HYD
    },
    {
        SON_EVENT_CLIENT_LBD,
        SONE_EVENT_SOCKET_LBD
    },
};

static int SetFdNonBlocking(int iFd)
{
  int iIfFlags = 0;

  if((iIfFlags = fcntl(iFd, F_GETFL, 0)) < 0)
  {
    return -1;
  }
  iIfFlags |= O_NONBLOCK;
  if((fcntl(iFd, F_SETFL, iIfFlags)) < 0)
  {
    return -1;
  }
  return 0;
}

/* send the START EVENT to HYD
 * HYD will know the Daemon is started
 */
static void wifison_server_start(void)
{
    int i, sock_len;
    char buffer[MESSAGE_FRAME_LEN_MAX] = {};
    struct service_message *message = (struct service_message *)buffer;

    memset(buffer, 0, sizeof buffer);
    message->cmd = SERVER_START;
    message->len = 0;
    for (i = 0; i < SON_EVENT_MAX_CLIENTS; i++) {
        struct sockaddr_un client_addr;

        memset(&client_addr, 0, sizeof(client_addr));
        client_addr.sun_family = AF_UNIX;
        strlcpy(client_addr.sun_path, clients[i].path , sizeof(clients[i].path));
        sock_len = strlen(client_addr.sun_path);
        client_addr.sun_path[sock_len] = '\0';

        if (sendto (soneventAppS.socket, buffer, sizeof buffer, 0, (struct sockaddr *) (&client_addr), (socklen_t) (sizeof (client_addr))) < 0)
        {
            int temp = errno;
            if ((temp != ECONNREFUSED) && (temp != ENOENT) && (temp != EAGAIN))
                printf("%s: Device state response to client %d failed: %s\r\n", __func__, clients[i].clientID, strerror (temp));
        }
    }

}

/* send the data to HYD
 */
void wifison_data_send(struct vendorData *vData, int vDataLen)
{
    char buffer[MESSAGE_FRAME_LEN_MAX] = {};
    struct sockaddr_un soneventclientaddr = {
        AF_UNIX,
        SONE_EVENT_SOCKET_HYD
    };
    struct service_message *message = (struct service_message *)buffer;

    if (vDataLen >= MESSAGE_FRAME_LEN_MAX - 8)
    {
        printf("%s: Data length (%d) exceeds the max message data length %d.\r\n",
                __func__, vDataLen, MESSAGE_FRAME_LEN_MAX - 8);
        return;
    }

    memset(buffer, 0, sizeof buffer);
    message->cmd = VENDOR_DATA_SEND;
    memcpy(message->data, (unsigned char*)vData, vDataLen);
    message->len = vDataLen;
    if (sendto (soneventAppS.socket, buffer, sizeof buffer, 0, (struct sockaddr *) (&soneventclientaddr), (socklen_t) (sizeof (soneventclientaddr))) < 0)
    {
        int temp = errno;
        if ((temp != ECONNREFUSED) && (temp != ENOENT) && (temp != EAGAIN))
            printf("%s: Device state response to HYD failed: %s\r\n", __func__, strerror (temp));
    }
}

/* Init function to create a socket
 * HYD can send any event to this socket.
 * When socket init ok, will send a SERVER START event
 * to HYD.
 */
int wifison_event_init(void)
{
    int eventsock_len;
    struct sockaddr_un sockaddr_un = {
        AF_UNIX,
        SONE_EVENT_SOCKET_SERVER
    };
    int fd = -1;

    memset(&sockaddr_un, 0, sizeof(sockaddr_un));
    sockaddr_un.sun_family = AF_UNIX;
    strlcpy(sockaddr_un.sun_path, SONE_EVENT_SOCKET_SERVER, sizeof(sockaddr_un.sun_path));
    eventsock_len = strlen(SONE_EVENT_SOCKET_SERVER);
    sockaddr_un.sun_path[eventsock_len] = '\0';

    if ((fd = socket (AF_UNIX, SOCK_DGRAM, 0)) == -1) {
        printf("%s: socket create(%s) failed:%s \r\n", __func__, sockaddr_un.sun_path, strerror (errno));
        return (-1);
    }

    if (unlink (sockaddr_un.sun_path)) {
        if (errno != ENOENT) {
            printf("%s: unlink(%s) failed: %s\r\n", __func__, sockaddr_un.sun_path, strerror (errno));
            return (-1);
        }
    }
    if (bind (fd, (struct sockaddr *)(&sockaddr_un), sizeof (sockaddr_un)) == -1) {
        printf( "%s: bind(%s) failed: %s\r\n", __func__, sockaddr_un.sun_path, strerror (errno));
        return (-1);
    }
    if (chmod (sockaddr_un.sun_path, 0666) == -1) {
        printf( "%s: chmod(%s) failed: %s\r\n", __func__, sockaddr_un.sun_path, strerror (errno));
        return (-1);
    }

    SetFdNonBlocking(fd);

    soneventAppS.socket=fd;

    wifison_server_start();

    return (fd);
}

/* De Init function to destroy the socket
 * Send a SERVER STOP event HYD before socket
 * destroy.
 */
void wifison_event_deinit(void)
{
    int i, sock_len;
    char buffer[MESSAGE_FRAME_LEN_MAX] = {};
    struct service_message *message = (struct service_message *)buffer;

    memset(buffer, 0, sizeof buffer);
    message->cmd = SERVER_STOP;
    message->len = 0;

    for (i = 0; i < SON_EVENT_MAX_CLIENTS; i++) {
        struct sockaddr_un client_addr;

        memset(&client_addr, 0, sizeof(client_addr));
        client_addr.sun_family = AF_UNIX;
        strlcpy(client_addr.sun_path, clients[i].path , sizeof(clients[i].path));
        sock_len = strlen(client_addr.sun_path);
        client_addr.sun_path[sock_len] = '\0';

        if (sendto (soneventAppS.socket, buffer, sizeof buffer, 0, (struct sockaddr *) (&client_addr), (socklen_t) (sizeof (client_addr))) < 0)
        {
            int temp = errno;
            if ((temp != ECONNREFUSED) && (temp != ENOENT) && (temp != EAGAIN))
                printf("%s: Device state response to client %d failed: %s\r\n", __func__, clients[i].clientID, strerror (temp));
        }
    }

    close (soneventAppS.socket);
}

/* register the event to HYD
 * HYD will send the notification for the event
 * that already register. If not, HYD will not send
 * the notification.
 */
void wifison_event_register(int eventID)
{
    int i, sock_len;
    char buffer[MESSAGE_FRAME_LEN_MAX] = {};
    struct service_message *message = (struct service_message *)buffer;

    memset(buffer, 0, sizeof buffer);
    message->cmd = EVENT_REGISTER;
    message->len = 1;
    message->data[0] = eventID;

    for (i = 0; i < SON_EVENT_MAX_CLIENTS; i++) {
        struct sockaddr_un client_addr;

        memset(&client_addr, 0, sizeof(client_addr));
        client_addr.sun_family = AF_UNIX;
        strlcpy(client_addr.sun_path, clients[i].path , sizeof(clients[i].path));
        sock_len = strlen(client_addr.sun_path);
        client_addr.sun_path[sock_len] = '\0';

        if (sendto (soneventAppS.socket, buffer, sizeof buffer, 0, (struct sockaddr *) (&client_addr), (socklen_t) (sizeof (client_addr))) < 0)
        {
            int temp = errno;
            if ((temp != ECONNREFUSED) && (temp != ENOENT) && (temp != EAGAIN))
            {
                printf("%s: Device state response to client %d failed: %s\r\n", __func__, clients[i].clientID, strerror (temp));
            }
        } else {
            if (!soneventAppS.eventrigerflag[eventID])
                soneventAppS.eventrigerflag[eventID] = true;
        }
    }
}

/* De register the event to HYD
 * HYD will not send the notification for the event
 * that already de-register. If not, HYD will stil send
 * the notification.
 */
void wifison_event_deregister(int eventID)
{
    int i, sock_len;
    char buffer[MESSAGE_FRAME_LEN_MAX] = {};
    struct service_message *message = (struct service_message *)buffer;

    memset(buffer, 0, sizeof buffer);
    message->cmd = EVENT_DEREGISTER;
    message->len = 1;
    message->data[0] = eventID;

    for (i = 0; i < SON_EVENT_MAX_CLIENTS; i++) {
        struct sockaddr_un client_addr;

        memset(&client_addr, 0, sizeof(client_addr));
        client_addr.sun_family = AF_UNIX;
        strlcpy(client_addr.sun_path, clients[i].path , sizeof(clients[i].path));
        sock_len = strlen(client_addr.sun_path);
        client_addr.sun_path[sock_len] = '\0';

        if (sendto (soneventAppS.socket, buffer, sizeof buffer, 0, (struct sockaddr *) (&client_addr), (socklen_t) (sizeof (client_addr))) < 0)
        {
            int temp = errno;
            if ((temp != ECONNREFUSED) && (temp != ENOENT) && (temp != EAGAIN))
            {
                printf("%s: Device state response to client %d failed: %s\r\n", __func__, clients[i].clientID, strerror (temp));
            }
        } else {
            if (soneventAppS.eventrigerflag[eventID])
                soneventAppS.eventrigerflag[eventID] = false;
        }
    }
}

/* Parsing the notification EVENT from HYD
 * Customer daemon can base on the EVENT to
 * do the related response.
 */
int wifison_event_get(struct sonEventInfo *event)
{
    int i = 0, err = EVENT_NO_DATA;
    char frame [MESSAGE_FRAME_LEN_MAX] = {
    };

    struct service_message *message = (struct service_message *)frame;

    if (recvfrom (soneventAppS.socket, frame, sizeof frame, 0, (struct sockaddr *) (0), (socklen_t *)(0)) < 0) {
        if(errno == EAGAIN)
        {
            return EVENT_NO_DATA;
        }
        printf("%s: recvfrom() failed: %s\r\n", __func__, strerror (errno));
        return EVENT_SOCKET_ERROR;
    }

    switch (message->cmd) {
        case CLIENT_START:
            wifison_server_start();
            for (i = 0; i < SON_MAX_EVENT; i++)
            {
                if (soneventAppS.eventrigerflag[i]) //If client restart after daemon start, sent the evnt to re-register where already registered.
                {
                    wifison_event_register(i);
                    err = EVENT_OK;
                    event->eventMsg = SON_CLIENT_RESTART_EVENT;
                    event->length = message->len;
                    memcpy(&event->data, message->data, message->len);
                }
            }
            break;
        case EVENT_NOTIFICATION:
            err = EVENT_OK;
            memcpy(event, message->data, message->len);
            break;
        default:
            break;
    }

    return err;
}
