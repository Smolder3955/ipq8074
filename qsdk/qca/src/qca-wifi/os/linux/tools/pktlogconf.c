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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <getopt.h>
#include <net/if.h>
#include "pktlog_fmt.h"

int pktlog_enable(char *sysctl_name, unsigned filter)
{
    FILE *fp;

    fp = fopen(sysctl_name, "w");

    printf("Open status of the pktlog_enable:%pK %s\n", fp, sysctl_name);
    if (fp != NULL) {
        fprintf(fp, "%i", filter);
        fclose(fp);
        return 0;
    }
    return -1;
}

int pktlog_filter_mac(char *sysctl_name, char *macaddr)
{
    FILE *fp;

    fp = fopen(sysctl_name, "w");

    printf("Open status of the pktlog_mac:%pK %s\n", fp, sysctl_name);
    if (fp != NULL) {
        fprintf(fp, "%s", macaddr);
        fclose(fp);
        return 0;
    }
    return -1;
}

int
pktlog_options(char *sysctl_options, unsigned long options)
{
    FILE *fp;

    fp = fopen(sysctl_options, "w");
    if (fp == NULL)
        return -1;

    fprintf(fp, "%lu", options);
    fclose(fp);
    return 0;
}

int pktlog_size(char *sysctl_size, char *sysctl_enable, int size)
{
    FILE *fp;

    /* Make sure logging is disabled before changing size */
    fp = fopen(sysctl_enable, "w");

    if (fp == NULL) {
        fprintf(stderr, "Open failed on enable sysctl\n");
        return -1;
    }
    fprintf(fp, "%i", 0);
    fclose(fp);

    fp = fopen(sysctl_size, "w");

    if (fp != NULL) {
        fprintf(fp, "%i", size);
        fclose(fp);
        return 0;
    }
    return -1;
}

int pktlog_reset(char *sysctl_reset, char *sysctl_enable)
{
    FILE *fp;

    /* Make sure logging is disabled before resetting buffer */
    fp = fopen(sysctl_enable, "w");

    if (fp == NULL) {
        fprintf(stderr, "Open failed on enable sysctl\n");
        return -1;
    }
    fprintf(fp, "%i", 0);
    fclose(fp);

    fp = fopen(sysctl_reset, "w");

    if (fp == NULL)
        return -1;
    fprintf(fp, "%i", 1);
    fclose(fp);
    return 0;
}

void usage()
{
    fprintf(stderr,
            "Packet log configuration\n"
            "usage: pktlogconf [-a adapter] [-e[event-list]] [-d adapter] [-s log-size] [-t -k -l]\n"
            "                  [-b -p -i] \n"
            "    -h    show this usage\n"
            "    -a    configures packet logging for specific 'adapter';\n"
            "          configures system-wide logging if this option is\n"
            "          not specified\n"
            "    -d    disable packet logging\n"
            "    -e    enable logging events listed in the 'event-list'\n"
            "          event-list is an optional comma separated list of one or more\n"
            "          of the following: rx tx rcf rcu ani  (eg., pktlogconf -erx,rcu,tx,cbf,hinfo,steering)\n"
            "    -s    change the size of log-buffer to \"log-size\" bytes\n"
            "    -t    enable logging of TCP headers\n"
            "    -k    enable triggered stop by a threshold number of TCP SACK packets\n"
            "    -l    change the number of packets to log after triggered stop\n"
            "    -b    enable triggered stop by a throughput threshold\n"
            "    -p    enable triggered stop by a PER threshold\n"
// not implemented
//            "    -y    enable triggered stop by a threshold number of Phyerrs\n"
            "    -i    change the time period of counting throughput/PER\n"
            "    -r    reset the pktlog buffer\n"
            "    -R    to enable remote packet logging\n"
            "    -P    Remote packet log port\n"
            );

    exit(-1);
}


int main(int argc, char *argv[])
{
    int c;
    int size = -1, tail_length = -1, sack_thr = -1;
    int thruput_thresh = -1, per_thresh = -1, phyerr_thresh = -1, trigger_interval = -1, remote_port = -1;
    unsigned long filter = 0, options = 0;
    char fstr[24];
    char ad_name[24];
    char sysctl_size[128];
    char sysctl_enable[128];
    char sysctl_mac[128];
    char sysctl_options[128];
    char sysctl_sack_thr[128];
    char sysctl_tail_length[128];
    char sysctl_thruput_thresh[128];
    char sysctl_phyerr_thresh[128];
    char sysctl_per_thresh[128];
    char sysctl_trigger_interval[128];
    char sysctl_remote_port[128];
    char sysctl_reset_buffer[128];
    char macaddr[128];
    int opt_a = 0, opt_d = 0, opt_e = 0, opt_m = 0, opt_r = 0, fflag = 0, opt_remote = 0;

    ad_name[0] = '\0';

    for (;;) {
        c = getopt(argc, argv, "s:m:e::a:d:tk:l:b:p:i:r:RP:h");

        if (c < 0)
            break;

        switch (c) {
            case 't':
                options |= ATH_PKTLOG_PROTO;
                break;
        case 'k': /* triggered stop after # of TCP SACK packets are seen */
            options |= ATH_PKTLOG_TRIGGER_SACK;
            if (optarg) {
                sack_thr = atoi(optarg);
            }
            break;
        case 'l': /* # of tail packets to log after triggered stop */
            if (optarg) {
                tail_length = atoi(optarg);
            }
            break;
        case 's':
            if (optarg) {
                size = atoi(optarg);
            }
            break;
        case 'e':
            if (opt_d) {
                usage();
                exit(-1);
            }
            opt_e = 1;
            if (optarg) {
                fflag = 1;
                if (strlcpy(fstr, optarg, sizeof(fstr)) >= sizeof(fstr)) {
                    printf("%s: Arg too long %s\n",__func__,optarg);
                    exit(-1);
                }
            }
            break;
        case 'a':
            opt_a = 1;
            if (optarg) {
                if (strlcpy(ad_name, optarg, sizeof(ad_name)) >= sizeof(ad_name)) {
                    printf("%s: Arg too long %s\n",__func__,optarg);
                    exit(-1);
                }
                printf("Option a:%s\n", ad_name);
            }
            break;
        case 'm':
            opt_m = 1;
            if (optarg) {
                if (strlcpy(macaddr, optarg, sizeof(macaddr)) >= sizeof(macaddr)) {
                    printf("%s: Arg too long %s\n",__func__,optarg);
                    exit(-1);
                }
                printf("Option a:%s\n", macaddr);
            }
            break;

        case 'd':
            if (optarg) {
                if (strlcpy(ad_name, optarg, sizeof(ad_name)) >= sizeof(ad_name)) {
                    printf("%s: Arg too long %s\n",__func__,optarg);
                    exit(-1);
                }
                printf("adname:%s\n", ad_name);
            }
            if (opt_e) {
                usage();
                exit(-1);
            }
            opt_d = 1;
            break;
        case 'b': /* triggered stop after throughput drop below this threshold */
            options |= ATH_PKTLOG_TRIGGER_THRUPUT;
            if (optarg) {
                thruput_thresh = atoi(optarg);
            }
            break;
        case 'p': /* triggered stop after PER increase over this threshold */
            options |= ATH_PKTLOG_TRIGGER_PER;
            if (optarg) {
                per_thresh = atoi(optarg);
            }
            break;
        case 'y': /* triggered stop after # of phyerrs are seen */
            options |= ATH_PKTLOG_TRIGGER_PHYERR;
            if (optarg) {
                phyerr_thresh = atoi(optarg);
            }
            break;
        case 'i': /* time period of counting trigger statistics */
            if (optarg) {
                trigger_interval = atoi(optarg);
            }
            break;
        case 'h':
            usage();
            exit(0);
        case 'r': /* reset the pktlog buffer*/
            if (optarg) {
            if (strlcpy(ad_name, optarg, sizeof(ad_name)) >= sizeof(ad_name)) {
                    printf("%s: Arg too long %s\n",__func__,optarg);
                    exit(-1);
                }
                printf("adname:%s\n", ad_name);
            }
            if (opt_e) {
                usage();
                exit(-1);
            }
            opt_r = 1;
            break;
        case 'R': /* to enable  remote packetlog */
            opt_remote = 1;
            break;
        case 'P':
            if (optarg) {
                if (opt_remote) {
                    remote_port = atoi(optarg);
                } else {
                    printf("Please enable -R option \n");
                    usage();
                    exit(-1);
                }
            }
            break;
        default:
            usage();
        }
    }

    /*
     * This protection is needed since system wide logging is not supported yet
     */
    if (opt_e) {
        if (!opt_a) {
            printf("Please enter the adapter\n");
            usage();
            exit(-1);
        }
    }

    if (if_nametoindex(ad_name) == 0) {
        printf("adname: %s is invalid\n", ad_name);
        usage();
        exit(-1);
    }

    if (opt_a) {
        snprintf(sysctl_enable,sizeof(sysctl_enable), "/proc/sys/" PKTLOG_PROC_DIR "/%s/enable", ad_name);
        snprintf(sysctl_size,sizeof(sysctl_size), "/proc/sys/" PKTLOG_PROC_DIR "/%s/size", ad_name);
        snprintf(sysctl_options,sizeof(sysctl_options), "/proc/sys/" PKTLOG_PROC_DIR "/%s/options",
                ad_name);
        snprintf(sysctl_sack_thr,sizeof(sysctl_sack_thr), "/proc/sys/" PKTLOG_PROC_DIR "/%s/sack_thr", ad_name);
        snprintf(sysctl_tail_length, sizeof(sysctl_tail_length), "/proc/sys/" PKTLOG_PROC_DIR "/%s/tail_length",
                ad_name);
        snprintf(sysctl_thruput_thresh,sizeof(sysctl_thruput_thresh), "/proc/sys/" PKTLOG_PROC_DIR "/%s/thruput_thresh", ad_name);
        snprintf(sysctl_per_thresh,sizeof(sysctl_per_thresh), "/proc/sys/" PKTLOG_PROC_DIR "/%s/per_thresh", ad_name);
        snprintf(sysctl_phyerr_thresh,sizeof(sysctl_phyerr_thresh), "/proc/sys/" PKTLOG_PROC_DIR "/%s/phyerr_thresh", ad_name);
        snprintf(sysctl_trigger_interval,sizeof(sysctl_trigger_interval), "/proc/sys/" PKTLOG_PROC_DIR "/%s/trigger_interval", ad_name);
        snprintf(sysctl_remote_port,sizeof(sysctl_remote_port), "/proc/sys/" PKTLOG_PROC_DIR "/%s/remote_port", ad_name);
    } else {
        snprintf(sysctl_enable, sizeof(sysctl_enable),"/proc/sys/" PKTLOG_PROC_DIR "/" PKTLOG_PROC_SYSTEM "/enable");
        snprintf(sysctl_size,sizeof(sysctl_size), "/proc/sys/" PKTLOG_PROC_DIR "/" PKTLOG_PROC_SYSTEM "/size");
        snprintf(sysctl_options,sizeof(sysctl_options), "/proc/sys/" PKTLOG_PROC_DIR "/" PKTLOG_PROC_SYSTEM "/options");
        snprintf(sysctl_sack_thr,sizeof(sysctl_sack_thr), "/proc/sys/" PKTLOG_PROC_DIR "/" PKTLOG_PROC_SYSTEM "/sack_thr");
        snprintf(sysctl_tail_length,sizeof(sysctl_tail_length), "/proc/sys/" PKTLOG_PROC_DIR "/" PKTLOG_PROC_SYSTEM "/tail_length");
        snprintf(sysctl_thruput_thresh,sizeof(sysctl_thruput_thresh), "/proc/sys/" PKTLOG_PROC_DIR "/" PKTLOG_PROC_SYSTEM "/thruput_thresh");
        snprintf(sysctl_per_thresh,sizeof(sysctl_per_thresh), "/proc/sys/" PKTLOG_PROC_DIR "/" PKTLOG_PROC_SYSTEM "/per_thresh");
        snprintf(sysctl_phyerr_thresh,sizeof(sysctl_phyerr_thresh), "/proc/sys/" PKTLOG_PROC_DIR "/" PKTLOG_PROC_SYSTEM "/phyerr_thresh");
        snprintf(sysctl_trigger_interval,sizeof(sysctl_trigger_interval), "/proc/sys/" PKTLOG_PROC_DIR "/"
        PKTLOG_PROC_SYSTEM "/trigger_interval");
    }

    if (opt_m) {
        snprintf(sysctl_mac,sizeof(sysctl_mac), "/proc/sys/" PKTLOG_PROC_DIR "/%s/mac", ad_name);
    } else {
        snprintf(sysctl_mac, sizeof(sysctl_mac),"/proc/sys/" PKTLOG_PROC_DIR "/" PKTLOG_PROC_SYSTEM "/mac");
    }
    if (opt_d) {
        /*
         * Need to be removed
         * Must disbale the entire system
         * However doing the above does not work for individual adapter logging
         * Needs fix
         */
        snprintf(sysctl_enable,sizeof(sysctl_enable), "/proc/sys/" PKTLOG_PROC_DIR "/%s/enable", ad_name);
        printf("sysctl_enable: %s\n", sysctl_options);
        pktlog_options(sysctl_options, 0);
        pktlog_enable(sysctl_enable, 0);
        printf("Called _pktlog_enable with parameter %d\n", (int) 0);
        return 0;
    }

    if (opt_r) {
        snprintf(sysctl_reset_buffer,sizeof(sysctl_reset_buffer), "/proc/sys/" PKTLOG_PROC_DIR "/%s/reset_buffer", ad_name);
        snprintf(sysctl_enable,sizeof(sysctl_enable), "/proc/sys/" PKTLOG_PROC_DIR "/%s/enable", ad_name);
        printf("Reset pktlog buffer for adapter: %s\n", ad_name);
        pktlog_reset(sysctl_reset_buffer, sysctl_enable);
        return 0;
    }

    if (sack_thr > 0) {
        if (options & ATH_PKTLOG_PROTO) {
            if (pktlog_size(sysctl_sack_thr, sysctl_enable, sack_thr) != 0) {
                fprintf(stderr, "pktlogconf: log sack_thr setting failed\n");
                exit(-1);
            }
            if (pktlog_size(sysctl_tail_length, sysctl_enable, tail_length) != 0) {
                fprintf(stderr, "pktlogconf: log tail_length setting failed\n");
                exit(-1);
            }
        } else {
            usage();
            exit(-1);
        }
    }

    if (thruput_thresh > 0) {
        if (pktlog_size(sysctl_thruput_thresh, sysctl_enable, thruput_thresh) != 0) {
            fprintf(stderr, "pktlogconf: log thruput_thresh setting failed\n");
            exit(-1);
        }
    }
    if (per_thresh > 0) {
        if (pktlog_size(sysctl_per_thresh, sysctl_enable, per_thresh) != 0) {
            fprintf(stderr, "pktlogconf: log per_thresh setting failed\n");
            exit(-1);
        }
    }
    if (phyerr_thresh > 0) {
        if (pktlog_size(sysctl_phyerr_thresh, sysctl_enable, phyerr_thresh) != 0) {
            fprintf(stderr, "pktlogconf: log phyerr_thresh setting failed\n");
            exit(-1);
        }
    }
    if (trigger_interval > 0) {
        if (pktlog_size(sysctl_trigger_interval, sysctl_enable, trigger_interval) != 0) {
            fprintf(stderr, "pktlogconf: log trigger_interval setting failed\n");
            exit(-1);
        }
    }

    if (remote_port > 0) {
        printf("Setting Port: %d \n", remote_port);
        if (pktlog_size(sysctl_remote_port, sysctl_enable, remote_port) != 0) {
            fprintf(stderr, "pktlogconf: log remote port setting failed\n");
            exit(-1);
        }
    }

    if (fflag) {
        if (strstr(fstr, "rx"))
            filter |= ATH_PKTLOG_RX;
        if (strstr(fstr, "tx"))
            filter |= ATH_PKTLOG_TX;
        if (strstr(fstr, "rcf"))
            filter |= ATH_PKTLOG_RCFIND;
        if (strstr(fstr, "rcu"))
            filter |= ATH_PKTLOG_RCUPDATE;
        if (strstr(fstr, "ani"))
            filter |= ATH_PKTLOG_ANI;
        if (strstr(fstr, "text"))
            filter |= ATH_PKTLOG_TEXT;
        if (strstr(fstr, "cbf"))
        {
            filter |= ATH_PKTLOG_RX | ATH_PKTLOG_CBF;
            printf("cbf enabled: _pktlog_filter= 0x%lx\n", filter);
        }
        if (strstr(fstr, "hinfo"))
            filter |= ATH_PKTLOG_RX | ATH_PKTLOG_H_INFO;
        if (strstr(fstr, "steering"))
            filter |= ATH_PKTLOG_RX | ATH_PKTLOG_STEERING;
        if (strstr(fstr, "lite")) {
            filter |= ATH_PKTLOG_LITE_RX;
            filter |= ATH_PKTLOG_LITE_T2H;
	}

        printf("_pktlog_filter = 0x%lx \n", filter);
        if (filter == 0)
            usage();
    } else {
        filter = ATH_PKTLOG_ANI | ATH_PKTLOG_RCUPDATE | ATH_PKTLOG_RCFIND |
            ATH_PKTLOG_RX | ATH_PKTLOG_TX | ATH_PKTLOG_TEXT | ATH_PKTLOG_CBF | ATH_PKTLOG_H_INFO | ATH_PKTLOG_STEERING;
        printf("_pktlog_filter else path : = 0x%lx \n", filter);
    }

    if (opt_remote) {
        filter |= ATH_PKTLOG_REMOTE_LOGGING_ENABLE;
    }

    if (size >= 0)
        if (pktlog_size(sysctl_size, sysctl_enable, size) != 0) {
            fprintf(stderr, "pktlogconf: log size setting failed\n");
            exit(-1);
        }

    if (opt_m) {
        if (pktlog_filter_mac(sysctl_mac, macaddr) != 0) {
            fprintf(stderr, "pktlogconf: mac address cannot be passed\n");
            exit(-1);
        }
    }
    if (opt_e) {
        if (pktlog_enable(sysctl_enable, filter) != 0) {
            fprintf(stderr, "pktlogconf: log filter setting failed\n");
            exit(-1);
        }
        if (pktlog_options(sysctl_options, options) != 0) {
            fprintf(stderr, "pktlogconf: options setting failed\n");
            exit(-1);
        }
    }
    return 0;
}
