#ifndef _CONNECTIONS_H
#define _CONNECTIONS_H
int connect_with_aps();
int close_connecttions();
struct ap* find_ap_by_sockfd(int sockfd);
#endif

