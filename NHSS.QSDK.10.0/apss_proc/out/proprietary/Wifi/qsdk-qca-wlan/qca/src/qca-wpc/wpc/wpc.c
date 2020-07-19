/*
 * Copyright (c) 2011, 2018 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * 2011 Qualcomm Atheros, Inc.
 *
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include <sys/select.h>
#include <string.h>
#include <sys/socket.h>
#include <net/if.h>
#include <linux/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <linux/netlink.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <errno.h>
#include <math.h>
#include <time.h>
#include <ieee80211_wpc.h>
#include <wpc_mgr.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <rtt.h>
#define PRIN_LOG(format, args...) printf(format "\n", ## args)

#ifndef MAX_PAYLOAD
#define MAX_PAYLOAD 8192 //1024
#endif


//TBD: Reduce these globals

int wpc_server_sock=0;
fd_set read_fds, error_set;
int fdcount, wpc_driver_sock;
struct iovec iov;
struct nlmsghdr *nlh = NULL;
struct msghdr msg;
/* Added to remove Warnings */
extern void wpc_mgr_init();
extern int create_wpc_sock();
extern int wpc_create_nl_sock();
extern int wpc_mgr_handletimeout();
extern int wpc_accept(int sockfd,struct sockaddr *addr, int *addrlen );
extern void wpc_nlsock_initialise(struct sockaddr_nl *dst_addr, struct nlmsghdr *nlh, struct iovec *iov, struct msghdr *msg);
extern void wpc_nlsendmsg(int wpc_driver_sock, int nbytes, char *buf, struct nlmsghdr *nlh, struct iovec *iov, struct msghdr *msg);
int wpc_close(int sockfd);
int wpc_start()
{
    int wpc_listen_sock, newfd, addrlen;   // listen on sock_fd, new connection on new_fd
    struct sockaddr_in remoteaddr;         // connector's address information
    struct sockaddr_nl dst_addr;
    int fdmax;
    struct timeval tv;
    int tcp_nodelay = 1;

    memset(&remoteaddr, 0, sizeof(struct sockaddr_in));
    wpc_listen_sock = create_wpc_sock();
    if (wpc_listen_sock < 0) {
        printf("NBP socket errno=%d\n", wpc_listen_sock);
        return wpc_listen_sock;
    }
    wpc_driver_sock = wpc_create_nl_sock();
    if (wpc_driver_sock < 0) {
        printf("Netlink socket errno= %d\n", wpc_driver_sock);
        return wpc_driver_sock;
    }

    nlh = (struct nlmsghdr *)calloc(NLMSG_SPACE(MAX_PAYLOAD),1);
    wpc_nlsock_initialise(&dst_addr, nlh, &iov, &msg); 

    FD_ZERO(&read_fds);
    FD_ZERO(&error_set);
    if(wpc_driver_sock > 0)
        FD_SET(wpc_driver_sock, &read_fds);
    if(wpc_driver_sock > 0)
        FD_SET(wpc_driver_sock, &error_set);
    if(wpc_listen_sock> 0)
        FD_SET(wpc_listen_sock,&read_fds);

    fdmax = ((wpc_listen_sock > wpc_driver_sock) ? wpc_listen_sock : wpc_driver_sock);

    for( ; ; ) {

        tv.tv_sec =0;
        tv.tv_usec = WPC_TIMERRESOLUTION; 
        if (select(fdmax+1, &read_fds, NULL, &error_set, &tv) == -1) {
            perror("select");
            if(errno == EINTR)
                continue;
        }
        if (FD_ISSET(wpc_driver_sock, &error_set))
            printf("Error: Netlink socket error\n");

        for(fdcount = 0; fdcount <= fdmax+1; fdcount++) {
            if (FD_ISSET(fdcount, &read_fds)) {

                //New connection
                if (fdcount == wpc_listen_sock) {
                    addrlen = sizeof(remoteaddr);
                    newfd = wpc_accept(wpc_listen_sock, (struct sockaddr *)&remoteaddr,&addrlen);
                    if (newfd == -1) {  
                        printf("Error: WPC socket error \n");
                    }
                    else {
                        if (newfd > fdmax)    
                            fdmax = newfd;
                        wpc_server_sock = newfd;
                        if( setsockopt(newfd, IPPROTO_TCP, TCP_NODELAY, &tcp_nodelay, sizeof(tcp_nodelay)) < 0 )
                            printf("Unable to set TCP_NODELAY option at the socket\n");
                        PRIN_LOG("New connection from %s on socket %d\n", inet_ntoa(remoteaddr.sin_addr), newfd);
                    }
                } else if ( fdcount == wpc_driver_sock ) {
                    //Message from driver
                    wpc_mgr_procdrivermsg();
                } else if ( fdcount == wpc_server_sock ) {
                    //Message from server
                    wpc_mgr_procservermsg();
                }
            }
        }
        //Timeout
        //Check if a timeout has occured (irrespective of whether select has timed out or 
        //if there has been activity on the fd) 
        wpc_mgr_handletimeout();

        FD_ZERO(&read_fds);

        if(wpc_server_sock > 0)
            FD_SET(wpc_server_sock, &read_fds); 

        if(wpc_listen_sock > 0)
            FD_SET(wpc_listen_sock, &read_fds); 

        if(wpc_driver_sock > 0)
            FD_SET(wpc_driver_sock, &read_fds); 
    }

    wpc_close(wpc_listen_sock);
    wpc_close(wpc_server_sock);
    free(nlh);
    return 0;
}

#define BUF_SIZE 100
#define IEEE80211_ELEMID_MEASREQ 38
#define IEEE80211_ELEMID_NEIGHBOR_REPORT 52


void send_ftmrr(char *cfgFile)
{
    FILE *fp;
    char buf[BUF_SIZE];
    char *p;
    char word[BUF_SIZE];
    int i, j, ap_count=0;
    int curr_index=0, len_index;

    struct neighbor_report_arr neighbor_data;
    struct sockaddr_nl dst_addr;
    struct msghdr msg;
    int wpc_driver_sock;
    u_int8_t bufNL[MAX_PAYLOAD]={'\0'};
    struct nsp_header *nsp_header = (struct nsp_header *) bufNL;

    memset(&neighbor_data, 0, sizeof(struct neighbor_report_arr)); 
    
    neighbor_data.id = IEEE80211_ELEMID_MEASREQ;
    curr_index = 0;

    fp = fopen(cfgFile, "r");
    if(fp == NULL)
    {
       printf("Can't open input file.\n");
       exit (1);
    }
    while(fgets(buf, BUF_SIZE, fp) != NULL) {
        p = buf;
        while(isspace(*p)) p++;
        word[0] = '\0';
        sscanf(p, "%s", word);
        if(strcasecmp(word, "sta_mac") == 0) {
            p=p+sizeof("sta_mac");
            while(isspace(*p)) p++;
            if(*p=='=') {
                p++;
            } else  {
                printf("Incorrect format of mac address \n");
                exit (1);
            }
            while(isspace(*p)) p++;
            for(i=0; i<6; i++)
            {
              sscanf(p, "%2hhx", &neighbor_data.sta_mac[i]);
              p=p+3;
            }
        } else if(strcasecmp(word, "dialogtoken") == 0) {
            p=p+sizeof("dialogtoken");
            while(isspace(*p)) p++;
            if(*p=='=') {
                p++;
            } else  {
                printf("Incorrect format \n");
                exit (1);
            }
            while(isspace(*p)) p++;
            sscanf(p, "%hhx", &neighbor_data.dialogtoken);
        } else if(strcasecmp(word, "num_repetitions") == 0) {
            p=p+sizeof("num_repetitions");
            while(isspace(*p)) p++;
            if(*p=='=') {
                p++;
            } else  {
                printf("Incorrect format \n");
                exit (1);
            }
            while(isspace(*p)) p++;
            sscanf(p, "%hx", &neighbor_data.num_repetitions);
        } else if(strcasecmp(word, "meas_token") == 0) {
            p=p+sizeof("meas_token");
            while(isspace(*p)) p++;
            if(*p=='=') {
                p++;
            } else  {
                printf("Incorrect format \n");
                exit (1);
            }
            while(isspace(*p)) p++;
            sscanf(p, "%hhx", &neighbor_data.meas_token);
        } else if(strcasecmp(word, "meas_req_mode") == 0) {
            p=p+sizeof("meas_req_mode");
            while(isspace(*p)) p++;
            if(*p=='=') {
                p++;
            } else  {
                printf("Incorrect format \n");
                exit (1);
            }
            while(isspace(*p)) p++;
            sscanf(p, "%hhx", &neighbor_data.meas_req_mode);
        }  else if(strcasecmp(word, "meas_type") == 0) {
            p=p+sizeof("meas_type");
            while(isspace(*p)) p++;
            if(*p=='=') {
                p++;
            } else  {
                printf("Incorrect format \n");
                exit (1);
            }
            while(isspace(*p)) p++;
            sscanf(p, "%hhx", &neighbor_data.meas_type);
        } else if(strcasecmp(word, "rand_inter") == 0) {
            p=p+sizeof("rand_inter");
            while(isspace(*p)) p++;
            if(*p=='=') {
                p++;
            } else  {
                printf("Incorrect format \n");
                exit (1);
            }
            while(isspace(*p)) p++;
            sscanf(p, "%hx", (short *)&neighbor_data.rand_inter);
        } else if(strcasecmp(word, "min_ap_count") == 0) {
            p=p+sizeof("min_ap_count");
            while(isspace(*p)) p++;
            if(*p=='=') {
                p++;
            } else  {
                printf("Incorrect format \n");
                exit (1);
            }
            while(isspace(*p)) p++;
            sscanf(p, "%hhx", &neighbor_data.min_ap_count);
        } else if(strncasecmp(word, "ap", 2) == 0) {
            while(*p++ != '=');
            while(isspace(*p)) p++;
            neighbor_data.elem[curr_index++] = IEEE80211_ELEMID_NEIGHBOR_REPORT;  // Neighbor report subelement ID
            len_index = curr_index++;
            i=0;
            while((j=sscanf(p, "0x%2hhx", &neighbor_data.elem[curr_index++])) != EOF)
            {
                if (j==0 && (i==0)) { printf("Incorrect format\n"); exit(1); }
                if(j==0) break;
                p = p+5;
                i++;
            }
            curr_index--;
            neighbor_data.elem[len_index] = i;
            ap_count++;
        }

    }
    fclose(fp);

    neighbor_data.len = curr_index+6;
    wpc_driver_sock = wpc_create_nl_sock();
    if (wpc_driver_sock < 0) {
        printf("Netlink socket errno= %d\n", wpc_driver_sock);
        exit (1);
    }
    nlh = (struct nlmsghdr *)calloc(NLMSG_SPACE(MAX_PAYLOAD),1);
    if (nlh == NULL) {
        PRIN_LOG("%s, Failed to allocate memory for netlink message\n", __func__);
        exit(1);
    }
    wpc_nlsock_initialise(&dst_addr, nlh, &iov, &msg);
    nsp_header->frame_type = NSP_FTMRR;
    memcpy(bufNL+NSP_HDR_LEN, &neighbor_data, sizeof(neighbor_data));
    wpc_nlsendmsg(wpc_driver_sock,NSP_HDR_LEN+sizeof(neighbor_data),(char*)bufNL,nlh,&iov,&msg);

    wpc_close(wpc_driver_sock);

}

/* Send where are you frame */
void send_whereareyou(char *cfgFile)
{
    FILE *fp;
    char buf[BUF_SIZE];
    char *p;
    char word[BUF_SIZE];
    int i;
    int curr_len=0;

    struct meas_req_lci_request wru;
    struct sockaddr_nl dst_addr;
    struct msghdr msg;
    int wpc_driver_sock;
    u_int8_t bufNL[MAX_PAYLOAD]={'\0'};
    struct nsp_header *nsp_header = (struct nsp_header *) bufNL;

    memset(&wru, 0, sizeof(struct meas_req_lci_request));
    wru.id = IEEE80211_ELEMID_MEASREQ;
    curr_len = 0;

    fp = fopen(cfgFile, "r");
    if(fp == NULL)
    {
       printf("Can't open input file.\n");
       exit (1);
    }
    while(fgets(buf, BUF_SIZE, fp) != NULL) {
        p = buf;
        while(isspace(*p)) p++;
        word[0] = '\0';
        sscanf(p, "%s", word);
        if(strcasecmp(word, "sta_mac") == 0) {
            p=p+sizeof("sta_mac");
            while(isspace(*p)) p++;
            if(*p=='=') {
                p++;
            } else  {
                printf("Incorrect format of mac address \n");
                exit (1);
            }
            while(isspace(*p)) p++;
            for(i=0; i<6; i++)
            {
              sscanf(p, "%2hhx", &wru.sta_mac[i]);
              p=p+3;
            }
        } else if(strcasecmp(word, "dialogtoken") == 0) {
            p=p+sizeof("dialogtoken");
            while(isspace(*p)) p++;
            if(*p=='=') {
                p++;
            } else  {
                printf("Incorrect format \n");
                exit (1);
            }
            while(isspace(*p)) p++;
            sscanf(p, "%hhx", &wru.dialogtoken);
        } else if(strcasecmp(word, "num_repetitions") == 0) {
            p=p+sizeof("num_repetitions");
            while(isspace(*p)) p++;
            if(*p=='=') {
                p++;
            } else  {
                printf("Incorrect format \n");
                exit (1);
            }
            while(isspace(*p)) p++;
            sscanf(p, "%hx", &wru.num_repetitions);
        } else if(strcasecmp(word, "meas_token") == 0) {
            curr_len++;
            p=p+sizeof("meas_token");
            while(isspace(*p)) p++;
            if(*p=='=') {
                p++;
            } else  {
                printf("Incorrect format \n");
                exit (1);
            }
            while(isspace(*p)) p++;
            sscanf(p, "%hhx", &wru.meas_token);
        } else if(strcasecmp(word, "meas_req_mode") == 0) {
            curr_len++;
            p=p+sizeof("meas_req_mode");
            while(isspace(*p)) p++;
            if(*p=='=') {
                p++;
            } else  {
                printf("Incorrect format \n");
                exit (1);
            }
            while(isspace(*p)) p++;
            sscanf(p, "%hhx", &wru.meas_req_mode);
        }  else if(strcasecmp(word, "meas_type") == 0) {
            curr_len++;
            p=p+sizeof("meas_type");
            while(isspace(*p)) p++;
            if(*p=='=') {
                p++;
            } else  {
                printf("Incorrect format \n");
                exit (1);
            }
            while(isspace(*p)) p++;
            sscanf(p, "%hhx", &wru.meas_type);
        }  else if(strcasecmp(word, "loc_subject") == 0) {
            curr_len++;
            p=p+sizeof("loc_subject");
            while(isspace(*p)) p++;
            if(*p=='=') {
                p++;
            } else  {
                printf("Incorrect format \n");
                exit (1);
            }
            while(isspace(*p)) p++;
            sscanf(p, "%hhx", &wru.loc_subject);
        }
    }
    fclose(fp);

    wru.len = curr_len;
    wpc_driver_sock = wpc_create_nl_sock();
    if (wpc_driver_sock < 0) {
        printf("Netlink socket errno= %d\n", wpc_driver_sock);
        exit (1);
    }
    nlh = (struct nlmsghdr *)calloc(NLMSG_SPACE(MAX_PAYLOAD),1);
    if (nlh == NULL) {
        PRIN_LOG("%s, Failed to allocate memory for netlink message\n", __func__);
        exit(1);
    }
    wpc_nlsock_initialise(&dst_addr, nlh, &iov, &msg);
    nsp_header->frame_type = NSP_WHEREAREYOU;
    memcpy(bufNL+NSP_HDR_LEN, &wru, sizeof(wru));
    wpc_nlsendmsg(wpc_driver_sock,NSP_HDR_LEN+sizeof(wru),(char*)bufNL,nlh,&iov,&msg);

    wpc_close(wpc_driver_sock);

}

#define MAX_LINE_SIZE             1000
#define NAME_SIZE                 20
#define COUNTRY_CODE_SIZE         2
#define LOC_CIVIC_TYPE_SIZE       1
#define LOC_CIVIC_LEN_SIZE        1
#define LOC_CIVIC_ADDR_SIZE       250

#define LCI_SIZE                  23
#define LCI_LATITUDE_SIZE         5
#define LCI_LONGITUDE_SIZE        5
#define LCI_LAT_UNCERTAIN_SIZE    1
#define LCI_LONG_UNCERTAIN_SIZE   1
#define LCI_ALTITUDE_SIZE         4
#define LCI_ALTI_UNCERTAIN_SIZE   1
#define LCI_ALTITUDE_TYPE_SIZE    1
#define LCI_FLOOR_INFO_Z_SIZE     4
#define LCI_HEIGHT_UNCERTAIN_SIZE 2
#define LCI_HEIGHT_Z_SIZE         2


int record_loc(char * cfgFile)
{
    FILE *fp;
    wmi_rtt_lcr_cfg_head lcr_data;
    wmi_rtt_lci_cfg_head lci_data;
    unsigned short packedData=0;
    char buf[MAX_LINE_SIZE];
    char word[MAX_LINE_SIZE];
    char *p;
    char name[NAME_SIZE+1];
    char countryCode[COUNTRY_CODE_SIZE];
    unsigned char locCivAddr[LOC_CIVIC_ADDR_SIZE];
    unsigned char lci[LCI_SIZE];
    int loc_civic_addr_type;
    int loc_civic_addr_len;
    int floor_info_z;
    int height_above_floor_z;
    int height_above_floor_unc_z;
    int i;

    struct sockaddr_nl dst_addr;
    int wpc_driver_sock;
    u_int8_t bufNL[MAX_PAYLOAD]={'\0'};
    struct nsp_header *nsp_header = (struct nsp_header *) bufNL;

    memset(&lcr_data, 0, sizeof(lcr_data));
    memset(&lci_data, 0, sizeof(lci_data));

    printf("Recording location configuration data in driver\n");
    fp = fopen(cfgFile, "r");
    if(fp == NULL)
    {
       printf("Can't open input file.\n");
       exit (1);
    }
    while(fgets(buf, MAX_LINE_SIZE, fp) != NULL) {
        p = buf;
        while(isspace(*p)) p++;
        word[0] = '\0';
        sscanf(p, "%s", word);
        if(strcasecmp(word, "name") == 0) {
            p=p+sizeof("name");
            while(isspace(*p)) p++;
            if(*p=='=') {
                p++;
            } else  {
                printf("Incorrect format of mac address \n");
                exit (1);
            }
            while(isspace(*p)) p++;
            strncpy(name, p, NAME_SIZE);
        } else if(strcasecmp(word, "CountryCode") == 0) {
            p=p+sizeof("CountryCode");
            while(isspace(*p)) p++;
            if(*p=='=') {
                p++;
            } else  {
                printf("Incorrect format of mac address \n");
                exit (1);
            }
            while(isspace(*p)) p++;
            if(sscanf(p,"%2s",countryCode) == 0) {
                printf("Country code error\n");
                exit(1);
            }
        } else if(strcasecmp(word, "LocCivicAddr") == 0) {
            p=p+sizeof("LocCivicAddr");
            while(isspace(*p)) p++;
            if(*p=='=') {
                p++;
            } else  {
                printf("Incorrect format of mac address \n");
                exit (1);
            }
            while(isspace(*p)) p++;
            for(i=0; i<LOC_CIVIC_ADDR_SIZE; i++)
            {
                sscanf(p, "%2hhx", &locCivAddr[i]);
                p = p+2;
            }
        } else if(strcasecmp(word, "LocCivicAddrType") == 0) {
            p=p+sizeof("LocCivicAddrType");
            while(isspace(*p)) p++;
            if(*p=='=') {
                p++;
            } else  {
                printf("Incorrect format of mac address \n");
                exit (1);
            }
            while(isspace(*p)) p++;
            sscanf(p, "%d", &loc_civic_addr_type);
        } else if(strcasecmp(word, "LocCivicAddrLength") == 0) {
            p=p+sizeof("LocCivicAddrLength");
            while(isspace(*p)) p++;
            if(*p=='=') {
                p++;
            } else  {
                printf("Incorrect format of mac address \n");
                exit (1);
            }
            while(isspace(*p)) p++;
            sscanf(p, "%d", &loc_civic_addr_len);

        } else if(strcasecmp(word, "lci") == 0) {
            p=p+sizeof("lci");
            while(isspace(*p)) p++;
            if(*p=='=') {
                p++;
            } else  {
                printf("Incorrect format of mac address \n");
                exit (1);
            }
            while(isspace(*p)) p++;
            for(i=0; i<LCI_SIZE; i++)
            {
                sscanf(p, "%2hhx", &lci[i]);
                p = p+2;
            }
        } else if(strcasecmp(word, "FloorInfoZ") == 0) {
            p=p+sizeof("FloorInfoZ");
            while(isspace(*p)) p++;
            if(*p=='=') {
                p++;
            } else  {
                printf("Incorrect format of mac address \n");
                exit (1);
            }
            while(isspace(*p)) p++;
            sscanf(p, "%x", &floor_info_z);
        } else if(strcasecmp(word, "HeightAboveFloorZ") == 0) {
            p=p+sizeof("HeightAboveFloorZ");
            while(isspace(*p)) p++;
            if(*p=='=') {
                p++;
            } else  {
                printf("Incorrect format of mac address \n");
                exit (1);
            }
            while(isspace(*p)) p++;
            sscanf(p, "%x", &height_above_floor_z);
        } else if(strcasecmp(word, "HeightAboveFloorUncZ") == 0) {
            p=p+sizeof("HeightAboveFloorUncZ");
            while(isspace(*p)) p++;
            if(*p=='=') {
                p++;
            } else  {
                printf("Incorrect format of mac address \n");
                exit (1);
            }
            while(isspace(*p)) p++;
            sscanf(p, "%x", &height_above_floor_unc_z);
        }
    }

    fclose(fp);

    /* Acutal civic info has 2 bytes countrycode, 1 byte CAtype, 1 byte Length of Civic addr (from here onwards), rest is civic loc addr */
    p=(char *)&lcr_data.civic_info[0];

    // Country code 2 bytes.
    memcpy(p, &countryCode, sizeof(countryCode));
    p = p + COUNTRY_CODE_SIZE;

    //CAtype 1 byte
    memcpy(p, &loc_civic_addr_type, LOC_CIVIC_TYPE_SIZE);
    p = p + LOC_CIVIC_TYPE_SIZE;

    // CaLength 1 byte
    memcpy(p, &loc_civic_addr_len, LOC_CIVIC_LEN_SIZE);
    p = p + LOC_CIVIC_LEN_SIZE;

    // LocCivicAddr array 250 bytes
    memcpy(p, &locCivAddr[0], loc_civic_addr_len);

    // 2 bytes for country code, 1 byte for CAtype, 1 byte for length
    lcr_data.loc_civic_params = ((loc_civic_addr_len + (COUNTRY_CODE_SIZE + LOC_CIVIC_TYPE_SIZE + LOC_CIVIC_LEN_SIZE))& 0xFF);

    memcpy(&lci_data.latitude, &lci[1], LCI_LATITUDE_SIZE);
    memcpy(&lci_data.longitude, &lci[7], LCI_LONGITUDE_SIZE);

    // Pack lci_cfg_param_info
    packedData = 0;

    p = (char *)&lci_data.lci_cfg_param_info;
    memcpy(p, &lci[0], LCI_LAT_UNCERTAIN_SIZE); //bits 7:0       Latitude_uncertainity
    p++;
    memcpy(p, &lci[6], LCI_LONG_UNCERTAIN_SIZE); // bits 15:8      Longitude_uncertainity
    p++;
    packedData = (lci[18] & 0x7);        // bits 18:16     Datum
    packedData |= ((lci[19] & 0x1)<<3);  // bits 19:19     RegLocAgreement
    packedData |= ((lci[20] & 0x1)<<4);  // bits 20:20     RegLocDSE
    packedData |= ((lci[21] & 0x1)<<5);  // bits 21:21     Dependent State
    packedData |= ((lci[22] & 0x3)<<6);  // bits 23:22     Version
    /*bits 27:24     motion_pattern for use with z subelement cfg as per
                     wmi_rtt_z_subelem_motion_pattern
     *bits 30:28     Reserved
     *bits 31:31     Clear LCI - Force LCI to "Unknown"
    */

    memcpy(p, &packedData, sizeof(packedData));
    memcpy(&lci_data.altitude, &lci[14], LCI_ALTITUDE_SIZE);

    p = (char *)&lci_data.altitude_info;
    memcpy(p, &lci[12], LCI_ALTI_UNCERTAIN_SIZE);            // bits 7:0       Altitude_uncertainity
    p++;
    memcpy(p, &lci[11], LCI_ALTITUDE_TYPE_SIZE);            // bits 15:8      Altitude type
    memcpy(&lci_data.floor, &floor_info_z, LCI_FLOOR_INFO_Z_SIZE);

    p = (char *) &lci_data.floor_param_info;
    /*bits 15:0      Height above floor in units of 1/64 m
     *bits 23:16     Height uncertainity as defined in 802.11REVmc D4.0 Z subelem format
   value 0 means unknown, values 1-18 are valid and 19 and above are reserved
    */

    packedData = (unsigned short)height_above_floor_unc_z;
    memcpy(p, &packedData, LCI_HEIGHT_UNCERTAIN_SIZE);
    p = p + LCI_HEIGHT_UNCERTAIN_SIZE;
    packedData = (unsigned short)height_above_floor_z;
    memcpy(p, &packedData, LCI_HEIGHT_Z_SIZE);
    lci_data.usage_rules = 1;

    wpc_driver_sock = wpc_create_nl_sock();
    if (wpc_driver_sock < 0) {
        printf("Netlink socket errno= %d\n", wpc_driver_sock);
        exit (1);
    }

    nlh = (struct nlmsghdr *)calloc(NLMSG_SPACE(MAX_PAYLOAD),1);
    wpc_nlsock_initialise(&dst_addr, nlh, &iov, &msg);

    nsp_header->frame_type = NSP_WPC_LCI;

    memcpy(bufNL+NSP_HDR_LEN, &lci_data, sizeof(lci_data));
    printf("%s[%d]\n", __func__,__LINE__);
    wpc_nlsendmsg(wpc_driver_sock,NSP_HDR_LEN+sizeof(lci_data),(char*)bufNL,nlh,&iov,&msg);

    nsp_header->frame_type = NSP_WPC_LCR;
    memcpy(bufNL+NSP_HDR_LEN, &lcr_data, sizeof(lcr_data));
    wpc_nlsendmsg(wpc_driver_sock,NSP_HDR_LEN+sizeof(lcr_data),(char*)bufNL,nlh,&iov,&msg);

    printf("Closing netlink socket.\n");
    wpc_close(wpc_driver_sock);
    free(nlh);
    return 0;
}


int main(int argc, char *argv[])
{
    if(argc < 2) {
        printf("usage:wpc -d or -n or -f <ftmrr_config_file> or -l <lci_config_file> -w <wru_file>\n");
        return -1 ;
    }
    if(strcmp(argv[1],"-n") == 0){
        printf("Running WPC...\n");
    } else if(strcmp(argv[1], "-f") == 0) {
        printf("Send FTMRR frame with config data from %s\n", argv[2]);
        send_ftmrr(argv[2]);
        return 0;
    } else if(strcmp(argv[1], "-w") == 0) {
        printf("Send Where Are You frame with config data from %s\n", argv[2]);
        send_whereareyou(argv[2]);
        return 0;
    } else if(strcmp(argv[1], "-l") == 0) {
        printf("Record LCI/LCR config data from %s\n", argv[2]);
        record_loc(argv[2]);
        return 0;
    } else if(strcmp(argv[1], "-d") != 0) {
        printf("usage:wpc -d or -n\n");
        return -1;
    }
    else
        daemon(0,0);

    wpc_mgr_init();
    wpc_start();
    return 0;
}
