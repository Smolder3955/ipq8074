/*
 * @@-COPYRIGHT-START-@@
 *
 * Copyright (c) 2014 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * @@-COPYRIGHT-END-@@
 */

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/file.h>
#include <ctype.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <linux/types.h>
#include <linux/if.h>
#include <linux/netlink.h>
#include <stdlib.h>
#include <time.h>
#include <getopt.h>
#include <glob.h>  /* Including glob.h for glob() function used in find_pid() */
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <stdarg.h>
#include <linux/wireless.h>
#include <asm/byteorder.h>
#if defined(__LITTLE_ENDIAN)
#define _BYTE_ORDER _LITTLE_ENDIAN
#elif defined(__BIG_ENDIAN)
#define _BYTE_ORDER _BIG_ENDIAN
#else
#error "Please fix asm/byteorder.h"
#endif
#include "os/linux/include/ieee80211_external.h"

#ifndef ATH_SSID_STEERING
#define NETLINK_ATH_SSID_EVENT (NETLINK_GENERIC + 13)
#endif

#define MAX_PAYLOAD 1024  /* maximum payload size*/
struct sockaddr_nl src_addr, dest_addr;
struct nlmsghdr *nlh = NULL;
struct iovec iov;
int sock_fd;
#define MAX_NUM_VAPS 32
char public_iface[MAX_NUM_VAPS][IFNAMSIZ] = {{0,0}};
char private_iface[MAX_NUM_VAPS][IFNAMSIZ] = {{0,0}};
struct {
    int daemon;
    const char *conf_file;
    int private_num_vaps;
    int public_num_vaps;
}ssid_config_data;

static void
ieee80211_vap_send_mac_addr(const char *ifname, unsigned char macaddr[IEEE80211_ADDR_LEN])
{
    struct iwreq iwr = { 0 };
    struct sockaddr addr;
    memset(&addr, 0, sizeof(addr));
    memcpy(&addr.sa_data, macaddr, IEEE80211_ADDR_LEN);
    int s = 0;

    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0){
        perror(" Error in opening socket \n");
        return;
    }

    (void) memset(&iwr, 0, sizeof(iwr));
    (void) strlcpy(iwr.ifr_name, ifname, sizeof(iwr.ifr_name));
    memcpy(iwr.u.name, &addr, sizeof(addr));
    if (ioctl(s, IEEE80211_IOCTL_ADDMAC, &iwr) < 0) {
        perror("set IOCTL of send_mac_addr failed \n");
        close(s);
    }
    close(s);
    return;
}
static void
ieee80211_vap_send_ssid_config(char *ifname,  int vap_config)
{
    struct iwreq iwr = { 0 };
    int s = 0;
    
    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0){
        perror(" Error in opening socket \n");
        return;
    }

    (void) memset(&iwr, 0, sizeof(iwr));
    (void) strlcpy(iwr.ifr_name, ifname, sizeof(iwr.ifr_name));
    int params[2] = { IEEE80211_PARAM_VAP_SSID_CONFIG, vap_config };
    memcpy(iwr.u.name, &params, sizeof(params));

    if (ioctl(s, IEEE80211_IOCTL_SETPARAM, &iwr) < 0) {
        perror("set IOCTL of send_config failed \n");
        close(s);
        return;
    }
    if(!vap_config){
        int param[2] = { IEEE80211_PARAM_MACCMD, 2};
        memcpy(iwr.u.name, &param, sizeof(param));

        if (ioctl(s, IEEE80211_IOCTL_SETPARAM, &iwr) < 0) {
            perror("set IOCTL of ACL_DENY_LIST failed \n");
            close(s);
            return;
        }
    }

    close(s);
    return;
}

static int
flush_acl_list(char *ifname)
{
    struct iwreq iwr = { 0 };
    int s = 0;

    if (ifname == NULL){
        perror(" Interface name is NULL \n");
        return -1;
    }

    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0){
        perror(" Error in opening socket \n");
        return -1;
    }

    (void) memset(&iwr, 0, sizeof(iwr));
    (void) strlcpy(iwr.ifr_name, ifname, sizeof(iwr.ifr_name));
    int params[2] = { IEEE80211_PARAM_MACCMD, 3 };
    memcpy(iwr.u.name, &params, sizeof(params));

    if (ioctl(s, IEEE80211_IOCTL_SETPARAM, &iwr) < 0) {
        perror("set IOCTL of ACL_FLUSH_LIST failed \n");
        close(s);
        return -1;
    }

    close(s);
    return 0;
}

static void usage(void)
{
    printf( "Usage: ssidsteering [-d] [-C conf-file] \n");
    printf(" -d: Do NOT fork into the background: run in debug mode.\n");
    printf(" -C: Specify configuration file.\n");
    exit(1);
}

static void parse_arg(char **argv)
{
    char *arg;

    ssid_config_data.daemon = 1;

    argv++;     /* skip program name */

    while ((arg = *argv++) != NULL) {
        if (!strcmp(arg, "-d")) {
            ssid_config_data.daemon = 0;
        } else
            if (!strcmp(arg, "-C")) { /* configuration file */
                arg = *argv++;
                if (arg == NULL){
                    usage();
                }
                if (!access(arg, R_OK)){
                    ssid_config_data.conf_file = arg;
                }
                else{
                    ssid_config_data.conf_file = NULL;
                }
            } else {
                usage();
            }
    }
    return;
}

static void ssid_shutdown_signalhandler(int signal) {
    int i, status;
    for (i = 0; i < ssid_config_data.public_num_vaps ; i++){
        status = flush_acl_list(public_iface[i]);
        
        if (status == -1){
            printf(" Flushing Failed \n");
            close(sock_fd);
            exit(1);
        }
        }
        printf("Terminating SSID steering \n");
        close(sock_fd);
        exit(1); 
}
int
main(int argc, char *argv[])
{

#define DELIM_CONFIG "="
#define PUBLIC_VAP_CONFIG 0
#define PRIVATE_VAP_CONFIG 1
#define MAC_DENY_POLICY 2

    const char *fname;
    FILE *fp = NULL;
    int find_result = 0;
    char temp[512] = { 0 };
    char private_array[512] = { 0 };
    char public_array[512] = { 0 };
    char *private_vaps = NULL;
    char *public_vaps = NULL;
    char *substring_private = NULL;
    char *substring_public = NULL;
    int  i = 0;
    char private[10] = "private";
    char public[10] = "public";
    struct msghdr msg;
    struct ssidsteering_event event;
    char *tmp_private_array;
    char *tmp_public_array;
    char *tmp;

    sock_fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_ATH_SSID_EVENT);

    if (argc < 3){
        usage();
    }

    parse_arg(argv);

    if (ssid_config_data.daemon) {
        if (daemon(0,0)) {
            perror("daemon");
            exit(1);
        }
    }

    if (ssid_config_data.conf_file ==  NULL) {
        printf(" Configuration file not present \n");
        exit(1);
    }

    signal(SIGINT, ssid_shutdown_signalhandler);
    signal(SIGTERM,ssid_shutdown_signalhandler);

    memset(&src_addr, 0, sizeof(src_addr));
    src_addr.nl_family = AF_NETLINK;
    src_addr.nl_pid = getpid();  /* self pid */
    /* interested in group 1<<0 */
    src_addr.nl_groups = 1;
    bind(sock_fd, (struct sockaddr*)&src_addr, sizeof(src_addr));

    memset(&dest_addr, 0, sizeof(dest_addr));

    nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(MAX_PAYLOAD));
    if(nlh == NULL)
    {
        close(sock_fd);
        return 0;
    }

    memset(nlh, 0, NLMSG_SPACE(MAX_PAYLOAD));
    iov.iov_base = (void *)nlh;
    iov.iov_len = NLMSG_SPACE(MAX_PAYLOAD);
    msg.msg_name = (void *)&dest_addr;
    msg.msg_namelen = sizeof(dest_addr);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    fname = ssid_config_data.conf_file;

    if((fp = fopen(fname, "r")) == NULL) {
        printf(" FILE could not be opened \n");
        return(-1);
    }

    while(fgets(temp, 512, fp) != NULL) {

        if((strstr(temp,"#")) != NULL){
            printf("Contents not present \n");
            exit(1);
        }

        if((strstr(temp, private)) != NULL) {
            tmp = temp;
            substring_private = strtok_r(tmp, DELIM_CONFIG,&tmp);
            while(substring_private){
                if(strcmp(private, substring_private) != 0){
                    strlcpy(private_array, substring_private,sizeof(private_array));
                }
                substring_private = strtok_r(NULL, DELIM_CONFIG,&tmp);
            }
            tmp_private_array = private_array;
            private_vaps = strtok_r(tmp_private_array,",\n",&tmp_private_array);
            for(i = 0; private_vaps != NULL; i++){
                strlcpy(private_iface[i], private_vaps, sizeof(private_iface[i]));
                private_vaps = strtok_r(NULL,",\n",&tmp_private_array);
            }
            for(ssid_config_data.private_num_vaps = 0; ssid_config_data.private_num_vaps < i; ssid_config_data.private_num_vaps++){
                ieee80211_vap_send_ssid_config(private_iface[ssid_config_data.private_num_vaps], PRIVATE_VAP_CONFIG);
            }
        }

        else if((strstr(temp, public)) != NULL) {
            tmp = temp;
            substring_public = strtok_r(tmp, DELIM_CONFIG,&tmp);
            while(substring_public){
                if(strcmp(public, substring_public) != 0){
                    strlcpy(public_array, substring_public,sizeof(public_array));
                }
                substring_public = strtok_r(NULL, DELIM_CONFIG,&tmp);
            }
            tmp_public_array = public_array;
            public_vaps = strtok_r(tmp_public_array,",\n",&tmp_public_array);
            for(i = 0; public_vaps != NULL; i++){
                strlcpy(public_iface[i], public_vaps, sizeof(public_iface[i]));
                public_vaps = strtok_r(NULL,",\n",&tmp_public_array);
            }

            for(ssid_config_data.public_num_vaps = 0; ssid_config_data.public_num_vaps < i; ssid_config_data.public_num_vaps++){
                ieee80211_vap_send_ssid_config(public_iface[ssid_config_data.public_num_vaps],PUBLIC_VAP_CONFIG);
            }
        }
        find_result++;
    }
    if(find_result == 0) {
        printf("\nSorry, couldn't find a match.\n");
    }

    //Close the file if still open.
    if(fp) {
        fclose(fp);
    }

    printf("DAEMON is up and running\n");
    while(1)
    {
        /* Read event from kernel */
        recvmsg(sock_fd, &msg, 0);
        memcpy(&event, NLMSG_DATA(nlh), sizeof(event));
        if (event.type == ATH_EVENT_SSID_NODE_ASSOCIATED) {
            for(i = 0; i < ssid_config_data.public_num_vaps; i++){
                ieee80211_vap_send_mac_addr(public_iface[i], event.mac);
            }

        }
    }
    close(sock_fd);
    return 0;
}

