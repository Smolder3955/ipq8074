/*
Copyright (c) 2013 Qualcomm Atheros, Inc.
All Rights Reserved.
Qualcomm Atheros Confidential and Proprietary.
*/

//q_os_if.c

#include <sys/types.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/types.h>
#include <linux/if.h>
#include <linux/wireless.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <ctype.h>
#include <getopt.h>
#include <err.h>
#include <errno.h>
#include <time.h>

#include <endian.h>
#if __BYTE_ORDER == __BIG_ENDIAN
#define SWAP16(_x) ( (A_UINT16)( (((const A_UINT8 *)(&_x))[0] ) |\
                         ( ( (const A_UINT8 *)( &_x ) )[1]<< 8) ) )
#define SWAP32(_x) ((A_UINT32)(                       \
                    (((const A_UINT8 *)(&_x))[0]) |        \
                    (((const A_UINT8 *)(&_x))[1]<< 8) |    \
                    (((const A_UINT8 *)(&_x))[2]<<16) |    \
                    (((const A_UINT8 *)(&_x))[3]<<24)))
#endif

#ifndef __packed
#define __packed    __attribute__((__packed__))
#endif

#define SIOCG80211STATS                 (SIOCDEVPRIVATE+2)
#define IEEE80211_IOCTL_GETKEY          (SIOCDEVPRIVATE+3)
#define IEEE80211_IOCTL_GETWPAIE        (SIOCDEVPRIVATE+4)
#define IEEE80211_IOCTL_STA_STATS       (SIOCDEVPRIVATE+5)
#define IEEE80211_IOCTL_STA_INFO        (SIOCDEVPRIVATE+6)
#define SIOC80211IFCREATE               (SIOCDEVPRIVATE+7)
#define SIOC80211IFDESTROY              (SIOCDEVPRIVATE+8)
#define IEEE80211_IOCTL_SCAN_RESULTS    (SIOCDEVPRIVATE+9)
#define IEEE80211_IOCTL_RES_REQ         (SIOCDEVPRIVATE+10)
#define IEEE80211_IOCTL_GETMAC          (SIOCDEVPRIVATE+11)
#define IEEE80211_IOCTL_CONFIG_GENERIC  (SIOCDEVPRIVATE+12)
#define SIOCIOCTLTX99                   (SIOCDEVPRIVATE+13)
#define IEEE80211_IOCTL_P2P_BIG_PARAM   (SIOCDEVPRIVATE+14)
#define SIOCDEVVENDOR                   (SIOCDEVPRIVATE+15)
#define IEEE80211_IOCTL_GET_SCAN_SPACE  (SIOCDEVPRIVATE+16)


#define ATH_XIOCTL_UNIFIED_UTF_CMD      0x1000
#define ATH_XIOCTL_UNIFIED_UTF_RSP      0x1001

#define CMD_TIMEOUT   10
#define QC98XX_CAL_SIZE 2116

#include "Qcmbr.h"


//struct ifreq {
//    char ifr_name[IFNAMSIZ]; /* Interface name */
//    union {
//        struct sockaddr ifr_addr;
//        struct sockaddr ifr_dstaddr;
//        struct sockaddr ifr_broadaddr;
//        struct sockaddr ifr_netmask;
//        struct sockaddr ifr_hwaddr;
//        short           ifr_flags;
//        int             ifr_ifindex;
//        int             ifr_metric;
//        int             ifr_mtu;
//        struct ifmap    ifr_map;
//        char            ifr_slave[IFNAMSIZ];
//        char            ifr_newname[IFNAMSIZ];
//        char           *ifr_data;
//    };
//};


extern void BoardDataCapture(unsigned char * respdata,unsigned int devid);
extern void DispHexString(A_UINT8 *pCmdStruct,A_UINT32 cmdSize);

typedef struct {
    int sock;
    struct ifreq ifr;
    char ifname[IFNAMSIZ];
    void (*rx_cb)(void *buf);
    unsigned char initialized;
    unsigned char timeout;
#ifdef SIGEV_NOTIFY_SIGNAL
    struct sigaction act;
#endif
    struct sigevent sev;
    timer_t timer;
} INIT_STRUCT;

static INIT_STRUCT initCfg;
static unsigned char responseBuf[2048+8];

int IsDeviceOpened()
{
  return(1);
}

int cmd_set_timer()
{
    struct itimerspec exp_time;
    int err;

    bzero(&exp_time, sizeof(exp_time));
    exp_time.it_value.tv_sec = CMD_TIMEOUT;
    err = timer_settime(initCfg.timer, 0, &exp_time, NULL);
    initCfg.timeout = 0;
    DbgPrintf("cmd_set_timer ret[%d]..\n",err);
    if (err < 0)
       return errno;

    return 0;
}

int cmd_stop_timer()
{
    struct itimerspec exp_time;
    int err;

    bzero(&exp_time, sizeof(exp_time));
    err = timer_settime(initCfg.timer, 0, &exp_time, NULL);
    DbgPrintf("cmd_stop_timer ret[%d]..\n",err);
    if (err < 0)
       return errno;

    return 0;
}


static void timer_expire(union sigval sig)
{
    DbgPrintf("Timer Expired..\n");
    initCfg.timeout = 1;
}

int cmd_init (char *ifname, void (*rx_cb)(void *buf))
{
    int ret = 0,s;

    DbgPrintf("cmd_init Beg..\n");
    if ( initCfg.initialized )
        return -1;

    memset(&initCfg.ifr, 0, sizeof(initCfg.ifr));
    memcpy(initCfg.ifr.ifr_name, ifname, ((IFNAMSIZ > strlen(ifname)) ? strlen(ifname) : IFNAMSIZ));

    initCfg.initialized = 1;

    initCfg.rx_cb = rx_cb;

    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) {
        err(1, "socket(SOCK_DRAGM)");
        ret = -1;
    }

    initCfg.sock = s;

#ifdef SIGEV_NOTIFY_SIGNAL

    memset(&(initCfg.act), 0, sizeof(initCfg.act));

    initCfg.act.sa_handler = timer_expire;
    initCfg.act.sa_flags = 0;

    sigemptyset(&(initCfg.act.sa_mask));

    if (sigaction(SIGUSR1, &(initCfg.act), NULL) == -1)
    {
        perror("fail to sigaction");
        exit(-1);
    }

    memset(&(initCfg.sev), 0, sizeof(initCfg.sev));

    initCfg.sev.sigev_signo = SIGUSR1;
    initCfg.sev.sigev_notify = SIGEV_SIGNAL;
#else

    initCfg.sev.sigev_notify = SIGEV_THREAD;
    initCfg.sev.sigev_notify_function = timer_expire;
#endif

    if ( -1 == timer_create(CLOCK_REALTIME,&initCfg.sev,&initCfg.timer))
    {
        DbgPrintf("%s[%d] failed to create timer\n", __func__, __LINE__);
        ret = -1;
    }

    DbgPrintf("cmd_init Socket[%d] ret[%d]..\n",s,ret);
    return ret;
}


int cmd_end()
{
    initCfg.initialized = 0;
    return 0;
}

void cmd_send2 (void *buf, int len, unsigned char *respdata, unsigned int *responseNeeded, unsigned int devid)
{
    int error = 0, rLen;
    //unsigned int *testData;
#if __BYTE_ORDER == __BIG_ENDIAN    
    unsigned int *ptr;
    unsigned int i;
#endif

    if (!initCfg.initialized)
       return;

    *(unsigned int *)buf = ATH_XIOCTL_UNIFIED_UTF_CMD;
    *((unsigned int *)buf + 1) = len;

#if __BYTE_ORDER == __BIG_ENDIAN
    DbgPrintf("SWAP qcmbr -> ioctrl payload, len=%d bytes\n", len);
    ptr = (unsigned int *)((unsigned int)buf + 8);  // skip cmd and len.
    for (i=0; i<(len/sizeof(int)); i++) // change to dword.
    {
        ptr[i] = SWAP32(ptr[i]);
    }   
#endif

    initCfg.ifr.ifr_data = (void *)buf;

    DbgPrintf("cmd_send2 bufLen[%d] rspLen[%u]..Beg\n",len, *responseNeeded);
    if (ioctl(initCfg.sock, SIOCIOCTLTX99, &initCfg.ifr) < 0) {
       err(1, "ioctl");
       return;
    }
    DbgPrintf("cmd_send2 RspNeeded[%d]..Mid\n",*responseNeeded);

    if (*responseNeeded)
    {
        cmd_set_timer();

        memset(responseBuf, 0xff, sizeof(responseBuf));
        while (1)
        {
            *(unsigned int *)responseBuf = ATH_XIOCTL_UNIFIED_UTF_RSP;

            initCfg.ifr.ifr_data = (void *)responseBuf;

            error = ioctl(initCfg.sock, SIOCIOCTLTX99, &initCfg.ifr);
            if ( initCfg.timeout )
            {
                DbgPrintf("cmd_send2 timeout ioctl[%d]..\n",ATH_XIOCTL_UNIFIED_UTF_RSP);
                //memset(&responseBuf[0], 0, sizeof(responseBuf));
                initCfg.ifr.ifr_data = responseBuf;
                break;
            }

            if ( error < 0 )
            {
                if ( errno == EAGAIN )
                    continue;
                else
                {
                    DbgPrintf("------errno %d\n",errno);
                    //memset(&responseBuf[0], 0, sizeof(responseBuf));
                    initCfg.ifr.ifr_data = responseBuf;
                    break;
                }
            }
            DbgPrintf("cmd_send2 OK RspIoctl ret[%d]..\n",error);

         break;
        }

        if(!initCfg.timeout)
        {
            cmd_stop_timer();
            initCfg.timeout = 0;
        }

        if ( initCfg.rx_cb != NULL ) {
            rLen = *(int *)initCfg.ifr.ifr_data;
            memcpy(respdata,(unsigned char *)(initCfg.ifr.ifr_data),rLen+4);

#if __BYTE_ORDER == __BIG_ENDIAN
            DbgPrintf("SWAP ioctrl -> qcmbr, rLen=%d bytes\n", rLen);
            ptr = (unsigned int *)((unsigned int)respdata + sizeof(int));   // skip len.
            for (i=0; i<(rLen/sizeof(int)); i++) // change to dword.
            {
                ptr[i] = SWAP32(ptr[i]);
            }
#endif

            rLen+=4;  // Includes length itself 4 bytes
            DbgPrintf("cmd_send2 CallBack() rLen[%d]\n",rLen);
            initCfg.rx_cb(initCfg.ifr.ifr_data);
            DispHexString((unsigned char *)respdata, rLen);
            // check if caldata needs to be logged and write into Flash
            if (rLen>107){
                if ((devid == 0x50)||(devid == 0x3c)) {
                    if((respdata[52]==0xC8) && (respdata[92]==7)) {

                        BoardDataCapture(respdata,devid);
                    }
                }else{
                    if((respdata[52]==0xE9) && (respdata[92]==7) &&(respdata[107]<0x30) ){

                        BoardDataCapture(respdata,devid);
                    }
                }
            }
            error = rLen;
        }
        *responseNeeded = (error>0)?error:0;
    }
    DbgPrintf("cmd_send2 ..End [%d]\n", error);
}



