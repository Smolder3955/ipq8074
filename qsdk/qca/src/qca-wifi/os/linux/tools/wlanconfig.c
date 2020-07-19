/*
 * Copyright (c) 2017-2018 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * Copyright (c) 2010, Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * wlanconfig athX create wlandev wifiX
 *	wlanmode station | adhoc | ibss | ap | monitor [bssid | -bssid]
 * wlanconfig athX destroy
 */

#include <wlanconfig.h>

static int (*handle_command) (int argc, char *argv[], const char *ifname, struct socket_context *sock_ctx);
static int set_p2p_noa(const char *ifname, char ** curargs);
static int get_noainfo(const char *ifname);
static int set_max_rate(struct socket_context *sock_ctx, const char *ifname,
            IEEE80211_WLANCONFIG_CMDTYPE cmdtype, char *macaddr, u_int8_t maxrate);
static void ieee80211_status(const char *ifname);

static inline int do_nothing (int argc, char *argv[], const char *ifname, struct socket_context *sock_ctx)
{
    /*Do nothing*/
    return 0;
}

int main(int argc, char *argv[])
{
    const char *ifname, *cmd;
    char *errorop;
    int status = 0;
    struct socket_context sock_ctx = {0};
    u_int8_t temp = 0, rate = 0;
    if (argc < 3)
        usage();

    ifname = argv[1];
    cmd = argv[2];
    handle_command = do_nothing;
    /*
     * Based on driver config mode (cfg80211/wext), application also runs
     * in same mode (wext/cfg80211)
     */
    sock_ctx.cfg80211 =  get_config_mode_type();

    if (streq(argv[argc-1],"-cfg80211")) {
        if (!sock_ctx.cfg80211) {
            fprintf(stderr, "Invalid tag '-cfg80211' for current mode.\n");
            return -EINVAL;
        }
        sock_ctx.cfg80211 = 1;
        argc--;
    }

    init_socket_context(&sock_ctx, WLANCONFIG_NL80211_CMD_SOCK_ID, WLANCONFIG_NL80211_EVENT_SOCK_ID);

    if (streq(cmd, "create")) {
        handle_command = handle_command_create;
    } else if (streq(cmd, "destroy")) {
        handle_command = handle_command_destroy;
    } else if (streq(cmd, "list")) {
        handle_command = handle_command_list;
#if UMAC_SUPPORT_NAWDS
    } else if (streq(cmd, "nawds")) {
        handle_command = handle_command_nawds;
#endif
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
    } else if (streq(cmd, "hmwds")) {
        handle_command = handle_command_hmwds;
    } else if (streq(cmd, "ald")) {
        handle_command = handle_command_ald;
#endif
#if UMAC_SUPPORT_WNM
    } else if(streq(cmd, "wnm")) {
        handle_command = handle_command_wnm;
#endif
    } else if (streq(cmd, "p2pgo_noa")) {
        return set_p2p_noa(ifname,&argv[3]);
    } else if (streq(cmd, "noainfo")) {
        return get_noainfo(ifname);
#ifdef ATH_BUS_PM
    } else if (streq(cmd, "suspend")) {
        return suspend(ifname, 1);
    } else if (streq(cmd, "resume")) {
        return suspend(ifname, 0);
#endif
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
    } else if (streq(cmd, "hmmc")) {
        handle_command = handle_command_hmmc;
#endif
    } else if (streq(cmd, "set_max_rate")) {
        if (argc < 5) {
            errx(1, "Insufficient Number of Arguments\n");
        } else {
            temp = strtoul(argv[4], &errorop, 16);
            rate = temp;
            return set_max_rate(&sock_ctx, ifname, IEEE80211_WLANCONFIG_SET_MAX_RATE,
                    argv[3], rate);
        }
    }
#if ATH_SUPPORT_DYNAMIC_VENDOR_IE
    else if (streq(cmd, "vendorie")) {
        handle_command = handle_command_vendorie;
    } else if (streq(cmd, "addie")) {
        handle_command = handle_command_addie;
    }
#endif
    else if (streq(cmd, "atfstat")) {
        handle_command = handle_command_atfstat;
    }
#if ATH_SUPPORT_NAC
    else if (streq(cmd, "nac")) {
        handle_command = handle_command_nac;
    }
#endif
#if ATH_SUPPORT_NAC_RSSI
    else if (streq(cmd, "rssi_nac")) {
        handle_command = handle_command_nac_rssi;
    }
#endif
#if WLAN_CFR_ENABLE
    else if (streq(cmd, "cfr")) {
        handle_command = handle_command_cfr;
    }
#endif
#if QCA_AIRTIME_FAIRNESS
    else if (strncmp(ifname,"ath",3) == 0) {
        handle_command = handle_command_atf;
    }
#endif
    else
        ieee80211_status(ifname);

    status = handle_command(argc, argv, ifname, &sock_ctx);
    destroy_socket_context(&sock_ctx);
    if (status != 0)
        return status;

    return 0;
}


void usage(void)
{
    fprintf(stderr, "usage: wlanconfig athX create wlandev wifiX\n");
#if MESH_MODE_SUPPORT
    fprintf(stderr, "                  wlanmode [sta|adhoc|ap|monitor|wrap|p2pgo|p2pcli|p2pdev|specialvap|mesh|smart_monitor|lp_iot_mode]\n"
            "                  [wlanaddr <mac_addr>] [mataddr <mac_addr>] [bssid|-bssid] [vapid <0-15>] [nosbeacon]\n");
#else
    fprintf(stderr, "                  wlanmode [sta|adhoc|ap|monitor|wrap|p2pgo|p2pcli|p2pdev|specialvap|smart_monitor|lp_iot_mode]\n"
            "                  [wlanaddr <mac_addr>] [mataddr <mac_addr>] [bssid|-bssid] [vapid <0-15>] [nosbeacon]\n");
#endif
    fprintf(stderr, "usage: wlanconfig athX destroy\n");
#if UMAC_SUPPORT_NAWDS
    fprintf(stderr, "usage: wlanconfig athX nawds mode (0-4)\n");
    fprintf(stderr, "usage: wlanconfig athX nawds defcaps CAPS\n");
    fprintf(stderr, "usage: wlanconfig athX nawds override (0-1)\n");
    fprintf(stderr, "usage: wlanconfig athX nawds add-repeater MAC (0-1)\n");
    fprintf(stderr, "usage: wlanconfig athX nawds del-repeater MAC\n");
    fprintf(stderr, "usage: wlanconfig athX nawds list\n");
#endif
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
    fprintf(stderr, "usage: wlanconfig athX hmwds add-addr wds_ni_macaddr wds_macaddr\n");
    fprintf(stderr, "usage: wlanconfig athX hmwds reset-addr macaddr\n");
    fprintf(stderr, "usage: wlanconfig athX hmwds reset-table\n");
    fprintf(stderr, "usage: wlanconfig athX hmwds read-addr wds_ni_macaddr\n");
    fprintf(stderr, "usage: wlanconfig athX hmwds dump-wdstable\n");
    fprintf(stderr, "usage: wlanconfig athX hmwds read-table\n");
    fprintf(stderr, "usage: wlanconfig athX hmwds rem-addr <mac_addr>\n");
    fprintf(stderr, "usage: wlanconfig athX ald sta-enable <sta_mac_addr> <0/1>\n");

#endif
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
    fprintf(stderr, "usage: wlanconfig athX hmmc add ip mask\n");
    fprintf(stderr, "usage: wlanconfig athX hmmc del ip mask\n");
    fprintf(stderr, "usage: wlanconfig athX hmmc dump\n");
#endif
#if UMAC_SUPPORT_WNM
    fprintf(stderr, "usage: wlanconfig athX wnm setbssmax"
            " <idle period in seconds> [<idle option>]\n");
    fprintf(stderr, "usage: wlanconfig athX wnm getbssmax\n");
    fprintf(stderr, "usage: wlanconfig athX wnm tfsreq <filename>\n");
    fprintf(stderr, "usage: wlanconfig athX wnm deltfs\n");
    fprintf(stderr, "usage: wlanconfig athX wnm timintvl <Interval> \n");
    fprintf(stderr, "usage: wlanconfig athX wnm gettimparams\n");
    fprintf(stderr, "usage: wlanconfig athX wnm timrate "
            "<highrateEnable> <lowRateEnable> \n");
    fprintf(stderr, "usage: wlanconfig athX wnm bssterm "
            "<delay in TBTT> [<duration in minutes>]\n");
#endif
#ifdef ATH_BUS_PM
    fprintf(stderr, "usage: wlanconfig wifiX suspend|resume\n");
#endif
#if QCA_AIRTIME_FAIRNESS
    fprintf(stderr, "usage: wlanconfig athX addssid ssidname per_value(0--100)\n");
    fprintf(stderr, "usage: wlanconfig athX addsta  macaddr(example:112233445566) per_value(0--100)\n");
    fprintf(stderr, "usage: wlanconfig athX delssid ssidname\n");
    fprintf(stderr, "usage: wlanconfig athX delsta  macaddr\n");
    fprintf(stderr, "usage: wlanconfig athX showatftable [show_per_peer_table<1>]\n");
    fprintf(stderr, "usage: wlanconfig athX showairtime\n");
    fprintf(stderr, "usage: wlanconfig athX flushatftable\n");
    fprintf(stderr, "usage: wlanconfig athX addatfgroup groupname ssid\n");
    fprintf(stderr, "usage: wlanconfig athX configatfgroup groupname value (0 - 100)\n");
    fprintf(stderr, "usage: wlanconfig athX atfgroupsched <groupname> <group_scheduling_policy> \n"
            "       group_scheduling_policy : 0 - Fair, 1 - Strict, 2 - Fair with UB\n");
    fprintf(stderr, "usage: wlanconfig athX delatfgroup groupname\n");
    fprintf(stderr, "usage: wlanconfig athX showatfgroup\n");
    fprintf(stderr, "usage: wlanconfig athX addtputsta macaddr tput airtime(opt)\n");
    fprintf(stderr, "usage: wlanconfig athX deltputsta macaddr\n");
    fprintf(stderr, "usage: wlanconfig athX showtputtbl\n");
    fprintf(stderr, "usage: wlanconfig athX atfstat\n");
    fprintf(stderr, "usage: wlanconfig athX atfaddac <ssid/groupname> <ac_name>:<val> <ac_name>:<val> <ac_name>:<val> <ac_name>:<val>\n"
            "       ac_name: BE,BK,VI,VO\n"
            "       val: 0 - 100\n");
    fprintf(stderr, "usage: wlanconfig athX atfdelac <ssid/groupname> <ac_name> <ac_name> <ac_name> <ac_name>\n"
            "       ac_name: BE,BK,VI,VO\n");
    fprintf(stderr, "usage: wlanconfig athX showatfsubgroup\n");

#endif
#if ATH_SUPPORT_DYNAMIC_VENDOR_IE
    fprintf(stderr, "usage: wlanconfig athX vendorie add len <oui+pcap_data in bytes> oui <eg:xxxxxx> pcap_data <eg:xxxxxxxx> ftype_map <eg:xx>\n");
    fprintf(stderr, "usage: wlanconfig athX vendorie update len <oui+pcap_data in bytes> oui <eg:xxxxxx> pcap_data <eg:xxxxxxxx> ftype_map <eg:xx>\n");
    fprintf(stderr, "usage: wlanconfig athX vendorie remove len <oui+pcap_data in bytes> oui <eg:xxxxxx> pcap_data <eg:xx>\n");
    fprintf(stderr, "usage: wlanconfig athX vendorie list \n");
    fprintf(stderr, "usage: wlanconfig athX vendorie list len <oui in bytes> oui <eg:xxxxxx>\n");
    fprintf(stderr, "usage: wlanconfig athX addie ftype <frame type> len < data len> data <data>\n"
                    "       ftype: 0/2/4('0'-Beacon,'2'-Probe Resp,'4'-Assoc Resp)\n"
                    "       len: Data length(IE length + 2)\n"
                    "       data: Data(in hex) \n");
#endif
#if ATH_SUPPORT_NAC
    fprintf(stderr, "usage: wlanconfig athX nac add/del bssid <ad1 eg: xx:xx:xx:xx:xx:xx> <ad2> <ad3> ... <ad8>\n");
    fprintf(stderr, "usage: wlanconfig athX nac add/del client <ad1 eg: xx:xx:xx:xx:xx:xx> <ad2> <ad3> <ad4> <ad5>  <ad6> <ad7>  <ad8> ... <ad24>\n");
    fprintf(stderr, "usage: wlanconfig athX nac list bssid/client \n");
#endif
#if ATH_SUPPORT_NAC_RSSI
    fprintf(stderr, "usage: wlanconfig athX rssi_nac add bssid (eg: xx:xx:xx:xx:xx:xx)  client (eg: xx:xx:xx:xx:xx:xx)  channel (1 -- 255)\n");
    fprintf(stderr, "usage: wlanconfig athX rssi_nac del bssid (eg: xx:xx:xx:xx:xx:xx)  client (eg: xx:xx:xx:xx:xx:xx) \n");
    fprintf(stderr, "usage: wlanconfig athX rssi_nac show_rssi \n");
#endif
    fprintf(stderr, "                  wlanmode [sta|adhoc|ap|monitor|wrap|p2pgo|p2pcli|p2pdev|specialvap|mesh|smart_monitor|lp_iot_mode]\n"
                    "                  [wlanaddr <mac_addr>] [mataddr <mac_addr>] [bssid|-bssid] [vapid <0-15>] [nosbeacon]\n");
#if WLAN_CFR_ENABLE
    fprintf(stderr, "wlanconfig athX cfr start <peer MAC> <bw> <periodicity> <capture type> \n") ;
    fprintf(stderr, "wlanconfig athX cfr stop <peer MAC> \n") ;
#endif
    exit(-1);
}


#ifdef ATH_BUS_PM
static int suspend(const char *ifname, int suspend)
{
    struct ifreq ifr;
    int s, val = suspend;

    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0)
        err(1, "socket(SOCK_DRAGM)");
    memset(&ifr, 0, sizeof(ifr));

    if (strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name)) >= sizeof(ifr.ifr_name)) {
        fprintf(stderr, "ifname too long: %s\n", ifname);
        close (s);
        return -1;
    }

    ifr.ifr_data = (void *) &val;
    if (ioctl(s, SIOCSATHSUSPEND, &ifr) < 0)
        err(1, "ioctl");

    close(s);
}
#endif


static void ieee80211_status(const char *ifname)
{
    /* XXX fill in */
}

static int getsocket(void)
{
    static int s = -1;

    if (s < 0) {
        s = socket(AF_INET, SOCK_DGRAM, 0);
        if (s < 0)
            err(1, "socket(SOCK_DRAGM)");
    }
    return s;
}

#define MAX_NUM_SET_NOA     2   /* Number of set of NOA schedule to set */
static int set_p2p_noa(const char *ifname, char ** curargs)
{
    char ** curptr = curargs;
    struct ieee80211_p2p_go_noa go_noa[MAX_NUM_SET_NOA];
    int num_noa_set = 0;
    int i;
    struct iwreq iwr;

    while (num_noa_set < MAX_NUM_SET_NOA) {
        if (*curptr) {
            int input_param = atoi(*curptr);

            if (input_param > 255) {
                printf("Invalid Number of iterations. Equal 1 for one-shot.\n\tPeriodic is 2-254. 255 is continuous. 0 is removed\n");
                goto setcmd_p2pnoa_err;
            }

            go_noa[num_noa_set].num_iterations = (u_int8_t)input_param;

        } else{
            goto setcmd_p2pnoa_err;
        }
        curptr++;

        if (*curptr) {
            go_noa[num_noa_set].offset_next_tbtt = (u_int16_t)atoi(*curptr);
        } else{
            goto setcmd_p2pnoa_err;
        }
        curptr++;

        if (*curptr) {
            go_noa[num_noa_set].duration = (u_int16_t)atoi(*curptr);
        } else{
            goto setcmd_p2pnoa_err;
        }

        if ((go_noa[num_noa_set].num_iterations == 0) && (go_noa[num_noa_set].duration != 0)) {
            printf("Error: Invalid Number of iterations. To remove NOA, the duration must also be 0.\n");
            goto setcmd_p2pnoa_err;
        }

        num_noa_set++;

        /* Check if there is another set */
        curptr++;

        if (*curptr == NULL) {
            /* we are done*/
            break;
        }
    }


    memset(&iwr, 0, sizeof(iwr));

    if (strlcpy(iwr.ifr_name, ifname, sizeof(iwr.ifr_name)) >= sizeof(iwr.ifr_name)) {
        fprintf(stderr, "ifname too long: %s\n", ifname);
        return -1;
    }

    iwr.u.data.pointer = (void *) &(go_noa[0]);
    iwr.u.data.length = sizeof(struct ieee80211_p2p_go_noa) * num_noa_set;
    iwr.u.data.flags = IEEE80211_IOC_P2P_GO_NOA;
    if (ioctl(getsocket(), IEEE80211_IOCTL_P2P_BIG_PARAM, &iwr) < 0)
        err(1, "ioctl: failed to set p2pgo_noa\n");
    printf("%s  p2pgo_noa for", iwr.ifr_name);
    for (i = 0; i < num_noa_set; i++) {
        printf(", [%d] %d %d %d", i, go_noa[i].num_iterations,
                go_noa[i].offset_next_tbtt, go_noa[i].duration);
        if (go_noa[i].num_iterations == 1) {
            printf(" (one-shot NOA)");
        }
        if (go_noa[i].num_iterations == 255) {
            printf(" (continuous NOA)");
        }
        else {
            printf(" (%d iterations NOA)", (unsigned int)go_noa[i].num_iterations);
        }
    }
    printf("\n");

    return 1;

setcmd_p2pnoa_err:
    printf("Usage: wlanconfig wlanX p2pgonoa <num_iteration:1-255> <offset from tbtt in msec> < duration in msec> {2nd set} \n");
    return 0;
}

#define _ATH_LINUX_OSDEP_H
#define _WBUF_H
#define _IEEE80211_API_H_
typedef int ieee80211_scan_params;
typedef int wlan_action_frame_complete_handler;
typedef int wbuf_t;
#include "include/ieee80211P2P_api.h"
static int get_noainfo(const char *ifname)
{
    /*
     * Handle the ssid get cmd
     */
    struct iwreq iwr;
    wlan_p2p_noa_info noa_info;
    int i;
    memset(&iwr, 0, sizeof(iwr));

    if (strlcpy(iwr.ifr_name, ifname, sizeof(iwr.ifr_name)) >= sizeof(iwr.ifr_name)) {
        fprintf(stderr, "ifname too long: %s\n", ifname);
        return -1;
    }

    iwr.u.data.pointer = (void *) &(noa_info);
    iwr.u.data.length = sizeof(noa_info);
    iwr.u.data.flags = IEEE80211_IOC_P2P_NOA_INFO;
    if (ioctl(getsocket(), IEEE80211_IOCTL_P2P_BIG_PARAM, &iwr) < 0)
        err(1, "ioctl: failed to get noa info\n");
    printf("%s  noainfo : \n", iwr.ifr_name);
    printf("tsf %d index %d oppPS %d ctwindow %d  \n",
            noa_info.cur_tsf32, noa_info.index, noa_info.oppPS, noa_info.ctwindow );
    printf("num NOA descriptors %d  \n",noa_info.num_descriptors);
    for (i=0;i<noa_info.num_descriptors;++i) {
        printf("descriptor %d : type_count %d duration %d interval %d start_time %d  \n",i,
                noa_info.noa_descriptors[i].type_count,
                noa_info.noa_descriptors[i].duration,
                noa_info.noa_descriptors[i].interval,
                noa_info.noa_descriptors[i].start_time );
    }
    return 1;
}

static int set_max_rate(struct socket_context *sock_ctx,
        const char *ifname, IEEE80211_WLANCONFIG_CMDTYPE cmdtype,
        char *addr, u_int8_t maxrate)
{
    struct ieee80211_wlanconfig config;
    uint8_t temp[MACSTR_LEN] = {0};
    char macaddr[MACSTR_LEN] = {0};

    if (cmdtype == IEEE80211_WLANCONFIG_SET_MAX_RATE) {
        if (strlcpy(macaddr, addr, sizeof(macaddr)) >= sizeof(macaddr)) {
            printf("Invalid MAC address (format: xx:xx:xx:xx:xx:xx)\n");
            return -1;
        }

        if (ether_string2mac(temp, macaddr) != 0) {
            printf("Invalid MAC address\n");
            return -1;
        }
    }

    memset(&config, 0, sizeof(struct ieee80211_wlanconfig));
    config.cmdtype = cmdtype;
    memcpy(config.smr.mac, temp, IEEE80211_ADDR_LEN);
    config.smr.maxrate = maxrate;
    send_command(sock_ctx, ifname, &config, sizeof(struct ieee80211_wlanconfig),
            NULL, 255, IEEE80211_IOCTL_CONFIG_GENERIC);

    return 0;
}
