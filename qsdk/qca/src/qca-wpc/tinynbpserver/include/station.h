#ifndef _STATION_H
#define _STATION_H

#include<bdaddr.h>
struct station {
	char ssid[36];
	bdaddr_t station_mac;
	bdaddr_t ap_mac;
	int associated_ap;
	int result;
	int wakeup_interval;
	long int station_channel;
	int station_bandwidth;
	struct sockaddr_in ap_ipaddr;
	unsigned int request_count;
	unsigned int response_count;
	unsigned int response_count_from_ap[30];
	unsigned int request_count_to_ap[30];
};


#endif
