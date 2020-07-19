/*
 * Copyright (c) 2014, 2017 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2014 Qualcomm Atheros, Inc.
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
 * Simple QCA specific tool to set thermal parameters for beeliner.
 */
#include <sys/types.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/if.h> 
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <err.h>
#include <ctype.h>
#include <stdint.h>

#include "if_athioctl.h"
#include <ol_if_thermal.h>
#if UMAC_SUPPORT_CFG80211
#include "cfg80211_nlwrapper_api.h"
#endif
#include <ext_ioctl_drv_if.h>
#include "cfg80211_nl_adapt.h"
#ifndef _LITTLE_ENDIAN
#define _LITTLE_ENDIAN 1234  /* LSB first: i386, vax */
#endif
#ifndef _BIG_ENDIAN
#define _BIG_ENDIAN 4321/* MSB first: 68000, ibm, net */
#endif
#if defined(__LITTLE_ENDIAN)
#define _BYTE_ORDER _LITTLE_ENDIAN
#elif defined(__BIG_ENDIAN)
#define _BYTE_ORDER _BIG_ENDIAN
#else
#error "Please fix asm/byteorder.h"
#endif

enum qca__nl80211_vendor_subcmds {
     QCA_NL80211_VENDOR_SUBCMD_EXTENDEDSTATS = 222,
};


static int cfg_flag = 0;
struct socket_context sock_ctx;

static int send_command (const char *ifname, void *buf, size_t buflen);

/**
 * send_command; function to send the cfg command or ioctl command.
 * @ifname :interface name
 * @buf: buffer
 * @buflen : buffer length
 * @cmd : command type
 * @ioctl_cmd: ioctl command type
 * Return 0 on success.
 */
static int send_command (const char *ifname, void *buf, size_t buflen)
{
#if UMAC_SUPPORT_WEXT
    struct ifreq ifr;
    int sock_fd, err;
#endif
#if UMAC_SUPPORT_CFG80211
    struct cfg80211_data buffer;
    int msg;
#endif    
    if (sock_ctx.cfg80211) {
#if UMAC_SUPPORT_CFG80211
        buffer.data = buf;
        buffer.length = buflen;
        buffer.callback = NULL;
        buffer.parse_data = 0;
        msg = wifi_cfg80211_send_generic_command(&(sock_ctx.ctx.cfg80211_ctxt),
                QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_CONFIGURATION,
                QCA_NL80211_VENDOR_SUBCMD_EXTENDEDSTATS, ifname, (char *)&buffer, buflen);
        if (msg < 0) {
            return msg;
        }
        return buffer.flags;
#endif        
    } else {
#if UMAC_SUPPORT_WEXT
        sock_fd = sock_ctx.ctx.sock_fd;

        if (ifname) {
            memset(ifr.ifr_name, '\0', IFNAMSIZ);
            strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
        } else {
            fprintf(stderr, "no such file or device");
            return -ENOENT;
        }

        ifr.ifr_data = buf;
        err = ioctl(sock_fd, SIOCGATHEXTENDED, &ifr);
        if (err < 0) {
            errx(1, "unable to send command");
            return err;
        }
#endif
    }
    return 0;
}

static int strnicmp (const char *str1, const char *str2, size_t len)
{
    int diff = -1;

    while (len && *str1)
    {
        --len;
        if ((diff = (toupper (*str1) - toupper (*str2))) != 0) {
            return diff;
        }
        str1++;
        str2++;
    }
    if (len) {
        return (*str2) ? -1 : 0;
    }
    return 0;
}


static int stricmp (const char *str1, const char *str2)
{
    int diff = -1;

    while (*str1 && *str2)
    {
        if ((diff = (toupper (*str1) - toupper (*str2))) != 0) {
            return diff;
        }
        str1++;
        str2++;
    }
    if (*str1 == *str2) {
        return 0;
    }
    return -1;
}

#define streq(a,b)  (stricmp(a,b) == 0)
#define strneq(a,b,c)  (strnicmp(a,b,c) == 0)

#define validate_and_set(var, val)  \
{   if (val >= 0) {                 \
        var = val;                  \
    } else {                        \
        parse_err = TH_TRUE;        \
        break;                      \
    }                               \
}

#define set_level_val(level, var, val)                  \
{                                                       \
    extended_cmd.ext_data.th_config.levelconf[level].var = val;               \
}

#define validate_and_get_level(src, ps, level)          \
{                                                       \
    if ((src[strlen(ps)] >= '0') &&                     \
        (src[strlen(ps)] < '0' + THERMAL_LEVELS)) {     \
        level = atoi(&src[strlen(ps)]);                 \
    } else {                                            \
        parse_err = TH_TRUE;                            \
        break;                                          \
    }                                                   \
}


const char *lower_th_str = "-lo";
const char *upper_th_str = "-hi";
const char *Tx_off_str = "-off";
const char *duty_cycle_str = "-dc";
const char *queue_prio_str = "-qp";
const char *policy_str = "-p";


int
main(int argc, char *argv[])
{
    struct th_config_t  *p_config;
    struct th_stats_t   *p_stats;
    struct ifreq ifr;
    struct extended_ioctl_wrapper extended_cmd;
    char ifname[20] = {'\0', };
    int i, argi;
    unsigned int subioctl_cmd = EXTENDED_SUBIOCTL_INVALID;
    int help_request = TH_FALSE;
    char *p = NULL;
    int parse_err = TH_FALSE;
    int val = 0;
    u_int32_t level = 0;

    /*
     * Based on driver config mode (cfg80211/wext), application also runs
     * in same mode (wext/cfg80211)
     */
    cfg_flag =  get_config_mode_type();

    if(streq(argv[argc-1],"-g")) {
        if (!cfg_flag){
            fprintf(stderr, "Invalid tag 'g' for current mode.\n");
            return -EINVAL;
        }
        cfg_flag = 1;
        argc--;
    }

    if (argc < 4) {
        parse_err = TH_TRUE;
    }

    memset(&extended_cmd.ext_data.th_config, INVALID_BYTE, sizeof(struct th_config_t));
                    

    for (argi = 1; argi < argc; argi++) {
        p = &argv[argi][0];
        if (strneq(p, "-help", 5) || strneq(p, "--help", 6) ||
            strneq(p, "-use", 4) || strneq(p, "--use", 5)) {
            help_request = TH_TRUE;
        } else if (streq(p, "-set")) {
            subioctl_cmd = EXTENDED_SUBIOCTL_THERMAL_SET_PARAM;
        } else if (streq(p, "-get")) {
            subioctl_cmd = EXTENDED_SUBIOCTL_THERMAL_GET_PARAM;
            break;
        } else if (streq(p, "-i")) {
            ++argi;
            if (strlcpy(ifname, argv[argi], sizeof(ifr.ifr_name)) >= sizeof(ifr.ifr_name)) {
                fprintf(stderr, "Interface name too long %s\n", argv[argi]);
                return -1;
            }
        } else if (streq(p, "-e")) {
            ++argi;
            val = atoi(argv[argi]);
            if ((val == THERMAL_MITIGATION_DISABLED) || (val == THERMAL_MITIGATION_ENABLED)) {
                validate_and_set(extended_cmd.ext_data.th_config.enable, val);
            } else {
                parse_err = TH_TRUE;
            }
        } else if (streq(p, "-et")) {
            ++argi;
            val = atoi(argv[argi]);
            if (val > 0) {
                validate_and_set(extended_cmd.ext_data.th_config.dc_per_event, val);
            } else {
                parse_err = TH_TRUE;
            }
        } else if (streq(p, "-dl")) {
            ++argi;
            val = atoi(argv[argi]);
            if (val > 0) {
                validate_and_set(extended_cmd.ext_data.th_config.log_lvl, val);
            } else {
                parse_err = TH_TRUE;
            }
        } else if (streq(p, duty_cycle_str)) {
            ++argi;
            val = atoi(argv[argi]);
            if (val > 0) {
                validate_and_set(extended_cmd.ext_data.th_config.dc, val);
            } else {
                parse_err = TH_TRUE;
            }
        } else if (strneq(p, policy_str, strlen(policy_str))) {
            validate_and_get_level(p, policy_str, level);
            ++argi;
            val = atoi(argv[argi]);
            /* As of now only queue pass TT scheme is supported */
            if (val == THERMAL_POLICY_QUEUE_PAUSE) {
                set_level_val(level, policy, val);
            } else {
                fprintf(stdout, "Only THERMAL_POLICY_QUEUE_PAUSE:(%d) is supported\n", THERMAL_POLICY_QUEUE_PAUSE);
                return -1;
            }
        } else if (strneq(p, lower_th_str, strlen(lower_th_str))) {
            validate_and_get_level(p, lower_th_str, level);
            ++argi;
            val = atoi(argv[argi]);
            set_level_val(level, tmplwm, val);
        } else if (strneq(p, upper_th_str, strlen(upper_th_str))) {
            validate_and_get_level(p, upper_th_str, level);
            ++argi;
            val = atoi(argv[argi]);
            set_level_val(level, tmphwm, val);
        } else if (strneq(p, Tx_off_str, strlen(Tx_off_str))) {
            validate_and_get_level(p, Tx_off_str, level);
            ++argi;
            val = atoi(argv[argi]);
            set_level_val(level, dcoffpercent, val);
        } else if (strneq(p, queue_prio_str, strlen(queue_prio_str))) {
            validate_and_get_level(p, queue_prio_str, level);
            ++argi;
            val = atoi(argv[argi]);
            set_level_val(level, priority, val);
        } else {
            parse_err = TH_TRUE;
        }
        if (parse_err == TH_TRUE) {
            break;
        }
    }

    if (((subioctl_cmd == EXTENDED_SUBIOCTL_THERMAL_SET_PARAM) && (argc < 6))     ||
            ((subioctl_cmd == EXTENDED_SUBIOCTL_THERMAL_GET_PARAM) && (argc < 4)) ||
            (subioctl_cmd == EXTENDED_SUBIOCTL_INVALID)) {
        parse_err = TH_TRUE;
    }

    if (strncmp(ifname, "wifi", 4) != 0) {
        fprintf(stdout, "Invalid interface name %s \n", ifname);
        parse_err = TH_TRUE;
    }


    if (parse_err == TH_TRUE || help_request == TH_TRUE) {
        fprintf(stdout, "Uses: thermaltool -i wifiX -get/-set -e [0/1: enable/disable] -et [event time in dutycycle units]"
                        " -dc [duty cycle in ms] -dl [debug level] -pN [policy for lvl N] -loN [low th for lvl N] - hiN"
                        " [high th for lvl N] -offN [Tx Off time for lvl N] -qpN [TxQ priority lvl N] -g\n");
        return -1;
    }
     
    init_context(cfg_flag, &sock_ctx);
    extended_cmd.cmd = subioctl_cmd;

    if (subioctl_cmd == EXTENDED_SUBIOCTL_THERMAL_SET_PARAM) {
        extended_cmd.data_len = sizeof(extended_cmd.ext_data.th_config);
    } else if (subioctl_cmd == EXTENDED_SUBIOCTL_THERMAL_GET_PARAM) {
        extended_cmd.data_len = sizeof(extended_cmd.ext_data.get_param);
    }
    
    send_command(ifname, &extended_cmd, sizeof(extended_cmd));

    if (subioctl_cmd == EXTENDED_SUBIOCTL_THERMAL_GET_PARAM) {
        p_config = &(extended_cmd.ext_data.get_param.th_cfg);
        p_stats = &(extended_cmd.ext_data.get_param.th_stats);
    
        fprintf(stdout, "Thermal config for %s\n", ifname);
        fprintf(stdout, "  enable: %d, dc: %d, dc_per_event: %d\n",p_config->enable, p_config->dc, p_config->dc_per_event);
        for (i = 0; i < THERMAL_LEVELS; i++) {
            fprintf(stdout, "  level: %d, low thresold: %d, high thresold: %d, dcoffpercent: %d, queue priority %d, policy; %d\n",
                            i, p_config->levelconf[i].tmplwm, p_config->levelconf[i].tmphwm, p_config->levelconf[i].dcoffpercent,
                            p_config->levelconf[i].priority, p_config->levelconf[i].policy);
        }
        fprintf(stdout, "Thermal stats for %s\n", ifname);
        fprintf(stdout, "  sensor temperature: %d, current level: %d\n",p_stats->temp, p_stats->level);
        for (i = 0; i < THERMAL_LEVELS; i++) {
            fprintf(stdout, "  level: %d, entry count: %d, duty cycle spent: %d\n",
                            i, p_stats->levstats[i].levelcount, p_stats->levstats[i].dccount); 
        }
    } 
    destroy_context(&sock_ctx);
    return 0;
}

