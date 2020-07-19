#ifndef _AP_H
#define _AP_H
#include<linux/socket.h>
#include<arpa/inet.h>
#include<ieee80211_hlsmgr.h>

#define DST_PORT 5000

enum {
	NOT_CONNECTED=0,
	CONNECTED,
	REQUEST_SENT,
	RESPONSE_RECEIVED	
} AP_STATE;

struct ap
{
    int sockfd;
    int state;
    int color;
    int id;
    int cpuspeed;
    int pixelradius;
    int channel;
    unsigned int response_count;
    unsigned int request_count;
    char apname[20];
    struct req req;
    struct resp *resp[100];
    struct sockaddr_in sockaddr;
} __attribute__((packed));

#endif

