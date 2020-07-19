/*
* Copyright (c) 2017 Qualcomm Technologies, Inc.
*
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define INVALID_SOCKET    -1
#define SOCKET_ERROR   -1
#define Sleep(x) usleep(x*1000)

#define DbgPrintf(_fmt, arg...) \
    do{ if (console > 0) { \
          printf("%s, Line %d: "_fmt, __FILE__, __LINE__, ##arg);} \
    }while(0)

#define IP_SIZE 32
#define BUF_SIZE 256

typedef struct {
    int fd;             // socket file descriptor;
    int port;
    struct sockaddr_in addr;
    char ip[IP_SIZE];
    char buf[BUF_SIZE];
} QMSCT_SOCKET_STRUCT;

typedef struct {
    QMSCT_SOCKET_STRUCT broadcast;
    QMSCT_SOCKET_STRUCT response;
    QMSCT_SOCKET_STRUCT keepalive;
} QMSCT_CLIENT_STRUCT;

static QMSCT_CLIENT_STRUCT gQmsctClient;
static int console = 0;

void signal_handler(int s)
{
    QMSCT_CLIENT_STRUCT *pClient = &gQmsctClient;

    printf("\nCaught signal %d, killing process %d\n", s, getpid());

    if (pClient->broadcast.fd != INVALID_SOCKET)
        close(pClient->broadcast.fd);
    if (pClient->response.fd != INVALID_SOCKET)
        close(pClient->response.fd);
    if (pClient->keepalive.fd != INVALID_SOCKET)
        close(pClient->keepalive.fd);

    exit(1);
}

int main(int narg, char *arg[])
{
    QMSCT_CLIENT_STRUCT *pClient = &gQmsctClient;

    memset(pClient, 0, sizeof(QMSCT_CLIENT_STRUCT));

    // disable printf line buffer.
    setbuf(stdout, NULL);

    // handle "ctrl+c" to stop program.
    signal (SIGINT, signal_handler);

    int iarg;
    for(iarg=1; iarg<narg; iarg++)
    {
        if ( strncmp(arg[iarg], "-ip", sizeof( "-ip" ) ) == 0)
        {
            if((iarg+1) < narg)
            {
                memcpy(pClient->response.ip, arg[iarg+1], IP_SIZE);
                iarg++;
            }
        }
        else if ( strncmp(arg[iarg], "-console", sizeof( "-console" ) ) == 0)
        {
            console = 1;
            printf( "Turn on debug message now.\n" );
        }
        else if ( strncmp(arg[iarg], "-broadcastport",
             sizeof( "-broadcastport" ) ) == 0)
        {
            if((iarg+1) < narg)
            {
                sscanf( arg[iarg+1]," %d ", &pClient->broadcast.port);
                iarg++;
            }
        }
        else if ( strncmp(arg[iarg], "-responseport",
             sizeof( "-responseport" ) ) == 0)
        {
            if((iarg+1) < narg)
            {
                sscanf( arg[iarg+1]," %d ", &pClient->response.port);
                iarg++;
            }
        }
        else if ( strncmp(arg[iarg], "-keepaliveport",
             sizeof( "-keepaliveport" ) ) == 0)
        {
            if((iarg+1) < narg)
            {
                sscanf( arg[iarg+1]," %d ", &pClient->keepalive.port);
                iarg++;
            }
        }
        else if ( strncmp(arg[iarg],"-help", sizeof( "-help" ) ) == 0)
        {
            printf( "-console : turn on message\n" );
            printf( "-ip 192.168.1.1: set user dedicated ip.\n" );
            printf( "-broadcast.port port: default is 2389\n" );
            printf( "-response.port port: default is 2388\n" );
            printf( "-keepalive.port port: default is 2387\n" );
            exit(0);
        }
        else
        {
            printf( "Error - Unknown parameter" );
        }
    }

    char *ret;
    // If ip is not set, get 'lan' ipaddr from /etc/config/network.
    if (strlen(pClient->response.ip) == 0)
    {
        struct hostent *hostInfo;
        char hostName[64];
        char *ipStr;

        if (gethostname (hostName, sizeof(hostName)) == 0)
        {
           if ((hostInfo = gethostbyname(hostName)) != NULL)
           {
             ipStr = (char *)inet_ntoa (*(struct in_addr *)*hostInfo->h_addr_list);
             if (ipStr)
             {
                DbgPrintf("Local ip=%s\n", ipStr);
                if (memcpy(pClient->response.ip, ipStr, IP_SIZE) == 0)
                {
                    printf("response.ip not init !\n");
                    exit(0);
                }
             }
           }
        }
    }

    DbgPrintf("response.ip=%s\n", pClient->response.ip);
    memcpy(pClient->broadcast.ip, pClient->response.ip, IP_SIZE);
    ret = strrchr(pClient->broadcast.ip,'.');
    if (!ret) {
        DbgPrintf("broadcast.ip format error = %s !!\n",
                  pClient->broadcast.ip);
        exit(0);
    }
    memcpy(ret, ".255", strlen(".255"));
    memcpy(pClient->broadcast.buf, pClient->response.ip, BUF_SIZE);

    DbgPrintf("broadcast.ip=%s\n", pClient->broadcast.ip);
    DbgPrintf("broadcast.buf=%s\n", pClient->broadcast.buf);

    if (pClient->broadcast.port == 0)
        pClient->broadcast.port = 2389;
    DbgPrintf("broadcast.port=%d\n", pClient->broadcast.port);
    if (pClient->response.port == 0)
        pClient->response.port = 2388;
    DbgPrintf("response.port=%d\n", pClient->response.port);
    if (pClient->keepalive.port == 0)
        pClient->keepalive.port = 2387;
    DbgPrintf("keepalive.port=%d\n", pClient->keepalive.port);

    pClient->broadcast.fd = socket(AF_INET,SOCK_DGRAM,0);
    if(pClient->broadcast.fd == INVALID_SOCKET)
    {
        perror("pClient->broadcast.fd socket create error!\n");
        goto EXIT;
    }

    int bcPermission = 1;
    if (0 != setsockopt(pClient->broadcast.fd, SOL_SOCKET, SO_BROADCAST,
       (char *) &bcPermission, sizeof(bcPermission)))
    {
        perror("setsockopt pClient->broadcast.fd error!\n");
        goto EXIT;
    }

    pClient->response.fd = socket(AF_INET,SOCK_DGRAM,0);
    if (pClient->response.fd == INVALID_SOCKET) {
        perror("pClient->response.fd socket create error!\n");
        goto EXIT;
    }

    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    if (setsockopt(pClient->response.fd, SOL_SOCKET,
        SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("setsockopt pClient->response.fd error !");
        goto EXIT;
    }

    pClient->broadcast.addr.sin_family      = AF_INET;
    pClient->broadcast.addr.sin_addr.s_addr = inet_addr(pClient->broadcast.ip);
    pClient->broadcast.addr.sin_port        = htons(pClient->broadcast.port);

    pClient->response.addr.sin_family      = AF_INET;
    pClient->response.addr.sin_addr.s_addr = inet_addr(pClient->response.ip);
    pClient->response.addr.sin_port        = htons(pClient->response.port);

    if (bind(pClient->response.fd, (struct sockaddr *)&pClient->response.addr,
       sizeof(struct sockaddr_in)) == -1)
    {
        perror("bind pClient->response.fd error!\n");
        goto EXIT;
    }

    int i = 0;
    while(++i)  // UDP wait response timeout is 1 sec in every loop.
    {
       if (0 > sendto(pClient->broadcast.fd, pClient->broadcast.buf,
           strlen(pClient->broadcast.buf), 0,
           (struct sockaddr *)&pClient->broadcast.addr,
            sizeof(struct sockaddr_in)))
       {
           printf("\nFailed to BC\n");
       }
       else
       {
          DbgPrintf("(%04d) BroadCast on %s:%d.\n", i,
          pClient->broadcast.ip, pClient->broadcast.port);
          DbgPrintf("(%04d) BroadCast Msg: %s.\n", i, pClient->broadcast.buf);
       }
       memset(pClient->response.buf, 0, BUF_SIZE);
       DbgPrintf("(%04d) Wait Response... from %s:%d\n", i,
       pClient->response.ip, pClient->response.port);
       if (recv(pClient->response.fd, pClient->response.buf, BUF_SIZE, 0) > 0)
       {
         //DbgPrintf("Response Msg: %s\n", pClient->response.buf);
         if (strcmp(pClient->response.buf, pClient->broadcast.buf) != 0)
         {
             // Connect TCP
             char *pStr;
             int aLiveCount = 0;

             DbgPrintf("Response Msg: %s\n", pClient->response.buf);
             memcpy(pClient->keepalive.ip, pClient->response.buf, IP_SIZE);
             pStr = strchr(pClient->keepalive.ip,':');
             if (!pStr) {
                 DbgPrintf("Keepalive.ip format error = %s !!\n", pClient->keepalive.ip);
                 exit(0);
             }
             *pStr = '\0';

             // use port 2387 to send "keep alive" message every 0.5 sec.
             pClient->keepalive.fd = socket(PF_INET, SOCK_STREAM, 0);
             pClient->keepalive.addr.sin_family = AF_INET;
             pClient->keepalive.addr.sin_addr.s_addr = inet_addr(pClient->keepalive.ip);
             pClient->keepalive.addr.sin_port = htons(pClient->keepalive.port);
             DbgPrintf("Connecting to TCP server %s:%d\n",
                      pClient->keepalive.ip, pClient->keepalive.port);
             connect(pClient->keepalive.fd,
                    (struct sockaddr *) &pClient->keepalive.addr,
                     sizeof(struct sockaddr_in));

             memcpy (pClient->keepalive.buf, "Alive", strlen("Alive"));
             while (pClient->keepalive.fd != INVALID_SOCKET)
             {
                 if ((aLiveCount%4) == 0) DbgPrintf("\r[|]");
                 if ((aLiveCount%4) == 1) DbgPrintf("\r[/]");
                 if ((aLiveCount%4) == 2) DbgPrintf("\r[-]");
                 if ((aLiveCount%4) == 3) DbgPrintf("\r[\\]");
                 aLiveCount ++;

                 // Send an initial buffer
                 if (SOCKET_ERROR == send( pClient->keepalive.fd,
                     pClient->keepalive.buf,
                     (int)strlen(pClient->keepalive.buf), 0 ))
                 {
                     DbgPrintf("\nSend keep alive message with error !\n");
                     break;
                 }
                 Sleep(500);
             }
         }
         else
             DbgPrintf("Skip own message =%s\n", pClient->response.buf);
       }
    }

EXIT:
    if (pClient->broadcast.fd != INVALID_SOCKET)
        close(pClient->broadcast.fd);
    if (pClient->response.fd != INVALID_SOCKET)
        close(pClient->response.fd);
    if (pClient->keepalive.fd != INVALID_SOCKET)
        close(pClient->keepalive.fd);

    DbgPrintf("Program end\n");
    return 0;
}
