/*
 * =====================================================================================
 *
 *       Filename:  ath_ssd_defs.h
 *
 *    Description:  Atheros Spectral Daemon Definitions
 *
 *        Version:  1.0
 *        Created:  12/13/2011 04:00:15 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  S.Karthikeyan (),
 *        Company:  Qualcomm Atheros
 *
 *        Copyright (c) 2012, 2017-2018 Qualcomm Technologies, Inc.
 *
 *        All Rights Reserved.
 *        Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 *        2012 Qualcomm Atheros, Inc.
 *
 *        All Rights Reserved.
 *        Qualcomm Atheros Confidential and Proprietary.
 *
 * =====================================================================================
 */

#ifndef _ATH_SSD_DEFS_H_
#define _ATH_SSD_DEFS_H_

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <string.h>
#include <sys/socket.h>
#include <net/if.h>
#include <linux/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <linux/netlink.h>
#include <netinet/in.h>
#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <assert.h>
#include <netinet/if_ether.h>
#include "spectral_data.h"
#include "classifier.h"
#include "spec_msg_proto.h"

#include "if_athioctl.h"
#include "classifier.h"
#include "spectral_data.h"
#include "spec_msg_proto.h"
#include "spectral.h"
#include "nl80211_copy.h"
#include "cfg80211_nlwrapper_api.h"
#include "spectral_ioctl.h"

#define ATHSSD_ASSERT(expr)       assert((expr))

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif /* MIN */

#ifndef NETLINK_GENERIC
    #define NETLINK_GENERIC 16
#endif  /* NETLINK_GENERIC */

/* IFNAMSIZ definition */
#ifndef IFNAMSIZ
#define IFNAMSIZ    16
#endif

#define line()          printf("----------------------------------------------------\n")
#define not_yet()       printf("TODO : %s : %d\n", __func__, __LINE__)
#define here()          printf("%s : %d\n", __func__, __LINE__)
#define streq(a, b)     ((strcasecmp(a, b) == 0))
#define IS_DBG_ENABLED()    (debug?1:0)

#define info(fmt, args...) do {\
    printf("athssd: %s (%4d) : " fmt "\n", __func__, __LINE__, ## args); \
    } while (0)

#define HT40_MAX_BIN_COUNT      (128)

#define TRUE                    (1)
#define FALSE                   !(TRUE)
#define SUCCESS                 (1)
#define FAILURE                 !(SUCCESS)
#define NUM_MAXIMUM_CHANNELS    11
#define MAX_PAYLOAD             1024
#define ATHPORT                 8001
#define BACKLOG                 10
#define CMD_BUF_SIZE            256
#define ENABLE_CLASSIFIER_PRINT 1
#define MSG_PACE_THRESHOLD      1
#define INVALID_FD              (-1)

#define INVALID_CHANNEL         0
#define DEFAULT_RADIO_IFNAME    "wifi0"
#define DEFAULT_DEV_IFNAME      "ath0"

#define ARRAY_LEN(x) (sizeof(x)/sizeof(x[0]))

#ifndef NETLINK_GENERIC
    #define NETLINK_GENERIC 16
#endif  /* NETLINK_GENERIC */

typedef enum channels {
    CHANNEL_01 = 1,
    CHANNEL_02,
    CHANNEL_03,
    CHANNEL_04,
    CHANNEL_05,
    CHANNEL_06,
    CHANNEL_07,
    CHANNEL_08,
    CHANNEL_09,
    CHANNEL_10,
    CHANNEL_11,
    MAX_CHANNELS,
}channels_t;

typedef enum sock_type {
    SOCK_TYPE_TCP,
    SOCK_TYPE_UDP,
}sock_type_t;


/*
 * Spetral param data structure with sane names for data memembers
 * XXX : This is copy of what is present in ah.h
 * Can't avoid this dupication now as the app and kernel share
 * this data structure and I do not want to make the build infrastruture
 * as simple as possible
 */
#ifndef MAX_CHAINS
#define MAX_CHAINS  4
#endif
#define HAL_PHYERR_PARAM_NOVAL  65355
#define HAL_PHYERR_PARAM_ENABLE 0x8000

#define CHANNEL_NORMAL_DWELL_INTERVAL   1
#define CHANNEL_CLASSIFY_DWELL_INTERVAL 10

typedef enum band_type {
    BAND_2GHZ,
    BAND_5GHZ,
}band_type_t;

typedef enum config_mode_type {
    CONFIG_IOCTL = 0,
    CONFIG_CFG80211 = 1,
    CONFIG_INVALID = 2,
} config_mode_type;

typedef struct ath_ssd_inet {
    int listener;
    int on;
    int client_fd;
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    struct sockaddr_storage peer_addr;
    socklen_t peer_addr_len;
    socklen_t addrlen;
    sock_type_t type;
}ath_ssd_inet_t;

typedef struct ath_ssd_nlsock {
    struct sockaddr_nl  src_addr;
    struct sockaddr_nl  dst_addr;
    int spectral_fd;
}ath_ssd_nlsock_t;

typedef struct ath_ssd_config {
    int ch_dwell_time;
    int max_hold_interval;
}ath_ssd_config_t;

#define MAX_INTERF_COUNT 10
typedef struct ath_ssd_interf_info {
    int count;
    struct INTERF_RSP interf_rsp[MAX_INTERF_COUNT];
}ath_ssd_interf_info_t;

typedef struct chan_stats {
    unsigned long long sent_msg;
    int interf_count;
}chan_stats_t;

typedef struct ath_ssd_stats {
    chan_stats_t ch[MAX_CHANNELS];
}ath_ssd_stats_t;

typedef struct ath_ssd_spectral_info {
    struct ath_diag atd;
    struct ifreq ifr;
}ath_ssd_spectral_info_t;

enum qca_nl80211_vendor_subcmds {
        QCA_NL80211_VENDOR_SUBCMD_WIFI_PARAMS = 200,
	QCA_NL80211_VENDOR_SUBCMD_PHYERR = 243,
};

typedef struct ath_ssd_info {
    int current_channel;                            /* current homing channel */
    int dwell_interval;                             /* channel dwell interval */
    int init_classifier;                            /* classifier needs initialization */
    int current_band;                               /* current operating band */
    int *channel_list;                              /* points to the current channel list */
    int max_channels;                               /* current max channels */
    int channel_index;                              /* points to current channel index */
    u_int16_t log_mode;                             /* Log mode to be used */

    sock_type_t sock_type;                          /* use tcp or udp */

    ath_ssd_inet_t inet_sock_info;                  /* inet socket information */
    ath_ssd_nlsock_t nl_sock_info;                  /* netlink socket information */
    ath_ssd_config_t config;                        /* config parameters */
    ath_ssd_interf_info_t  interf_info;             /* interference info */
    ath_ssd_stats_t stats;                          /* stats info */

    ath_ssd_spectral_info_t sinfo;                  /* will hold info related spectral */

    struct ss lwrband;                              /* lower band classifier information */
    struct ss uprband;                              /* upper band classifier information */

    char *radio_ifname;                             /* pointer to interface name */
    u_int8_t radio_macaddr[ETH_ALEN];               /* radio MAC address */
    char *dev_ifname;                               /* device ifname */
    char *filename;                                 /* Play SAMP data from this file */
    int replay;                                     /* Whether it is the `replay` mode */
#ifdef SPECTRAL_SUPPORT_CFG80211
    wifi_cfg80211_context cfg80211_sock_ctx;        /* cfg80211 socket context */
    config_mode_type cfg_flag;                      /* cfg flag */
#endif /* SPECTRAL_SUPPORT_CFG80211 */
    SPECTRAL_PARAMS_T prev_spectral_params;         /* previous spectral config */
    bool enable_gen3_linear_scaling;                /* enable gen3 linear
                                                       scaling */
}ath_ssd_info_t;

#define GET_ADDR_OF_INETINFO(p)     (&(p)->inet_sock_info)
#define GET_ADDR_OF_NLSOCKINFO(p)   (&(p)->nl_sock_info)
#define GET_ADDR_OF_CFGSOCKINFO(p)  (&(p)->cfg80211_sock_ctx)
#define GET_ADDR_OF_STATS(p)        (&(p)->stats)
#define CONFIGURED_SOCK_TYPE(p)     ((p)->sock_type)
#define IS_BAND_2GHZ(p)              (((p)->current_band == BAND_2GHZ)?1:0)
#define IS_BAND_5GHZ(p)              (((p)->current_band == BAND_5GHZ)?1:0)
#define IS_CFG80211_ENABLED(p)       (((p)->cfg_flag == CONFIG_CFG80211)?1:0)

/* Default value for whether gen3 linear format bin scaling is enabled */
#define ATH_SSD_ENAB_GEN3_LINEAR_SCALING_DEFAULT   (TRUE)

extern int init_inet_sockinfo(ath_ssd_info_t *pinfo);
extern int init_nl_sockinfo(ath_ssd_info_t *pinfo);
extern int accept_new_connection(ath_ssd_info_t *pinfo);
extern int accept_new_connection(ath_ssd_info_t *pinfo);
extern int handle_spectral_data(ath_ssd_info_t *pinfo);
extern int handle_client_data(ath_ssd_info_t *pinfo, int fd);

extern void process_spectral_msg(ath_ssd_info_t *pinfo, SPECTRAL_SAMP_MSG* msg);
extern void update_next_channel(ath_ssd_info_t *pinfo);
extern void stop_spectral_scan(ath_ssd_info_t *pinfo);
extern void switch_channel(ath_ssd_info_t *pinfo);
extern void start_spectral_scan(ath_ssd_info_t *pinfo);
extern void run_state(ath_ssd_info_t *pinfo);
extern void cleanup(ath_ssd_info_t *pinfo);
extern void alarm_handler(ath_ssd_info_t *pinfo);
extern void signal_handler(int signal);
extern void print_usage(void);
extern void init_bandinfo(struct ss *plwrband, struct ss *puprband, int print_enable);
extern void ms_init_classifier(struct ss *lwrband, struct ss *uprband, SPECTRAL_CLASSIFIER_PARAMS *cp);
extern void classifier(struct ss *bd, int timestamp, int last_capture_time, int rssi, int narrowband, int peak_index);
extern void print_spectral_SAMP_msg(SPECTRAL_SAMP_MSG* ss_msg);
extern void add_interference_report(ath_ssd_info_t *pinfo, struct INTERF_SRC_RSP *rsp);
extern int update_interf_info(ath_ssd_info_t *pinfo, struct ss *bd);
extern void clear_interference_info(ath_ssd_info_t *pinfo);
extern void print_ssd_stats(ath_ssd_info_t *pinfo);
extern void print_interf_details(ath_ssd_info_t *pinfo, eINTERF_TYPE type);
extern void new_process_spectral_msg(ath_ssd_info_t *pinfo, SPECTRAL_SAMP_MSG* msg);
extern void start_classifiy_spectral_scan(ath_ssd_info_t *pinfo);
extern const char* ether_sprintf(const u_int8_t *mac);
extern int ath_ssd_init_spectral(ath_ssd_info_t* pinfo);
extern int ath_ssd_start_spectral_scan(ath_ssd_info_t* pinfo);
extern int ath_ssd_stop_spectral_scan(ath_ssd_info_t* pinfo);
extern int ath_ssd_set_spectral_param(ath_ssd_info_t* pinfo, int op, int param);
extern int get_channel_width(ath_ssd_info_t* pinfo);
extern void ath_ssd_get_spectral_param(ath_ssd_info_t* pinfo, SPECTRAL_PARAMS_T* sp);
extern int ath_ssd_get_spectral_capabilities(ath_ssd_info_t* pinfo,
                struct spectral_caps *caps);
extern int get_vap_priv_int_param(ath_ssd_info_t* pinfo, const char *ifname, int param);

#endif /* _ATH_SSD_DEFS_H_ */
