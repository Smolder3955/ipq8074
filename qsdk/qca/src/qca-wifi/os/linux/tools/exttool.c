/*
 * Copyright (c) 2016-2018 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

/*
 * QCA specific tool to trigger channel switch, scan and set thermal parameters for beeliner.
 */

#include <qcatools_lib.h>
#include <ol_if_thermal.h>
#include <ext_ioctl_drv_if.h>

#define FALSE   0
#define TRUE    1
#define MAX_INTERFACE_NAME_LEN    20
#define BASE10  10
#define INVALID_ARG  (-1)

#define THERMAL_NL80211_CMD_SOCK_ID DEFAULT_NL80211_CMD_SOCK_ID
#define THERMAL_NL80211_EVENT_SOCK_ID DEFAULT_NL80211_EVENT_SOCK_ID
#define EXT_NL80211_CMD_SOCK_ID DEFAULT_NL80211_CMD_SOCK_ID
#define EXT_NL80211_EVENT_SOCK_ID DEFAULT_NL80211_EVENT_SOCK_ID

struct socket_context sock_ctx;

#define streql(a,b)  (strncasecmp((a),(b),(sizeof(b)-1)))
#define strneq(a,b,c)  (strncasecmp(a,b,c) == 0)

#define validate_and_set(var, val)          \
{   if (val >= 0) {                         \
    var = val;                          \
                  } else {                                \
                      parse_err = TH_TRUE;                \
                      break;                              \
                  }                                       \
}

#define set_level_val(level, var, val)                                       \
{                                                                            \
    extended_cmd.ext_data.th_config.levelconf[level].var = val;              \
}

#define validate_and_get_level(src, ps, level)                   \
{                                                                \
    if ((src[strlen(ps)] >= '0') &&                              \
            (src[strlen(ps)] < '0' + THERMAL_LEVELS)) {              \
        level = atoi(&src[strlen(ps)]);                          \
    } else {                                                     \
        parse_err = TH_TRUE;                                     \
        break;                                                   \
    }                                                            \
}

const char *lower_th_str = "-lo";
const char *upper_th_str = "-hi";
const char *Tx_off_str = "-off";
const char *duty_cycle_str = "-dc";
const char *queue_prio_str = "-qp";
const char *policy_str = "-p";


/*
 * print_usage: prints usage message.
 */
static void print_usage() {
    fprintf(stdout, "Usage:\n");
    fprintf(stdout, "1: exttool --chanswitch"
            " --interface wifiX"
            " --chan <dest primary channel>"
            " --chwidth <channelwidth>"
            " --numcsa <channel switch announcement count>"
            " --secoffset <1: PLUS, 3: MINUS> "
            " --cfreq2 <secondary 80 centre channel freq,"
            " applicable only for 80+80 MHz>"
            "--cfg80211\n");

    fprintf(stdout, "2: exttool --scan"
            " --interface wifiX"
            " --wideband <0: 20 MHz scan, 1: wide band scan. [If set to 1, upper 16 bits are treated as scan chan width]>"
            " --mindwell <min dwell time> allowed range [50ms to less than <max dwell>]"
            " --maxdwell <max dwell time> allowed range [greater than min dwell to 10000ms]"
            " --resttime <rest time > allowed range [greater than 0]"
            " --maxscantime <max scan time > allowed range [greater than resttime]"
            " --idletime <idle time > allowed range [greater than 50ms]"
            " --scanmode <1: ACTIVE, 2: PASSIVE> "
            " --chcount <Channel count followed by list of channels [chancount(1 to 32) <16 bit scan channel width><16 bit channel number>]>\n");

    fprintf(stdout, "3: exttool --rptrmove"
            " --interface wifiX"
            " --chan <target root AP channel>"
            " --ssid <SSID of target Root AP> "
            " --bssid <BSSID of target Root AP> "
            " --numcsa <channel switch announcement count>"
            " --cfreq2 <secondary 80 centre channel freq,"
            " applicable only for 80+80 MHz>\n");

    fprintf(stdout, ""
            "Usage: thermaltool -i wifiX -get/-set -e [0/1: enable/disable] -et [event time in dutycycle units]"
            " -dc [duty cycle in ms] -dl [debug level] -pN [policy for lvl N] -loN [low th for lvl N] - hiN"
            " [high th for lvl N] -offN [Tx Off time for lvl N] -qpN [TxQ priority lvl N] -g\n");
}

static int ext_send_command (const char *ifname, void *buf, size_t buflen);

/*
 * ext_send_command; function to send the cfg command or ioctl command.
 * @ifname :interface name
 * @buf: buffer
 * @buflen : buffer length
 * Return 0 on success.
 */
static int ext_send_command (const char *ifname, void *buf, size_t buflen)
{
#if UMAC_SUPPORT_WEXT
    struct ifreq ifr;
    int sock_fd, err;
#endif
#if UMAC_SUPPORT_CFG80211
    int msg;
    struct cfg80211_data buffer;
#endif
    if (sock_ctx.cfg80211) {
#if UMAC_SUPPORT_CFG80211
        buffer.data = buf;
        buffer.length = buflen;
        buffer.callback = NULL;
        buffer.parse_data = 0;
        msg = wifi_cfg80211_send_generic_command(&(sock_ctx.cfg80211_ctxt),
                QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_CONFIGURATION,
                QCA_NL80211_VENDOR_SUBCMD_EXTENDEDSTATS, ifname, (char *)&buffer, buflen);
        if (msg < 0) {
            fprintf(stderr,"Couldn't send NL command\n");
            return msg;
        }
        return buffer.flags;
#endif
    } else {
#if UMAC_SUPPORT_WEXT
        sock_fd = sock_ctx.sock_fd;
        if (ifname) {
            memset(ifr.ifr_name, '\0', IFNAMSIZ);
            if (strlcpy(ifr.ifr_name, ifname, strlen(ifname)+1) >=
                    sizeof(ifr.ifr_name)) {
                fprintf(stderr, "Interface Name Too Long\n");
                return -EINVAL;
            }
        } else {
            fprintf(stderr, "no such file or device");
            return -ENOENT;
        }

        ifr.ifr_data = (caddr_t)buf;
        err = ioctl(sock_fd, SIOCGATHEXTENDED, &ifr);
        if (err < 0) {
            errx(1, "unable to send command");
            return err;
        }
#endif
    }
    return 0;
}

#define MIN_DWELL_TIME        200  /* DEFAULT MIN DWELL Time for scanning 20ms */
#define MAX_DWELL_TIME        300  /* default MAX_DWELL time for scanning 300 ms */
#define CHAN_MIN                1  /* Lowest number channel */
#define CHAN_MAX              165  /* highest number channel */
#define PASSIVE_SCAN       0x0001  /* scan mode is passive*/
#define ACTIVE_SCAN        0x0002  /* scan mode is active*/

/*
 * validate_scan_param: validates scan parameters
 * @scan_req: pointer to scan_req
 * return 0 if valid param; otherwise -1
 */
int validate_scan_param( wifi_scan_custom_request_cmd_t *scan_req)
{
    if ((scan_req->min_dwell_time <= 0) || (scan_req->min_dwell_time < 50)) {
        fprintf(stdout, "Invalid min dwell time \n");
        return INVALID_ARG;
    }

    if ((scan_req->max_dwell_time < scan_req->min_dwell_time) || (scan_req->max_dwell_time > 10000)) {
        fprintf(stdout, "Invalid max dwell time \n");
        return INVALID_ARG;
    }

    if (scan_req->rest_time <= 0) {
        fprintf(stdout, "Invalid rest time \n");
        return INVALID_ARG;
    }

    if ((scan_req->scan_mode != PASSIVE_SCAN) &&
            (scan_req->scan_mode != ACTIVE_SCAN)) {
        fprintf(stdout, "Invalid scan mode: %d, it should be 1-Passive/2-Active\n", scan_req->scan_mode);
        return INVALID_ARG;
    }
    if ((scan_req->chanlist.n_chan <= 0) ||
            (scan_req->chanlist.n_chan > 32)) {
        fprintf(stdout, "Invalid number of channels requested to scan \n");
        return INVALID_ARG;
    }
    if (scan_req->max_scan_time < scan_req->rest_time) {
        fprintf(stdout, "Invalid max_scan_time \n");
        return INVALID_ARG;
    }
    if (scan_req->idle_time < 50) {
        fprintf(stdout, "Invalid idle \n");
        return INVALID_ARG;
    }
    return 0;
}

/*
 * handle_thermal_main: handles thermal main function
 * @argc: total number of command line arguments
 * @argv: values of command line arguments
 * returns 0 if success
 */
int handle_thermal_main(int argc, char *argv[])
{
    struct th_config_t  *p_config;
    struct th_stats_t   *p_stats;
    struct extended_ioctl_wrapper extended_cmd;
    char ifname[IFNAMSIZ] = {'\0', };
    unsigned int subioctl_cmd = EXTENDED_SUBIOCTL_INVALID;
    int i, argi;
    int help_request = TH_FALSE;
    char *p = NULL;
    int parse_err = TH_FALSE;
    int val = 0;
    u_int32_t level = 0;

    /*
     * Based on driver config mode (cfg80211/wext), application also runs
     * in same mode (wext/cfg80211)
     */
    sock_ctx.cfg80211 =  get_config_mode_type();

    if(streql(argv[argc-1],"-g")) {
        if (!sock_ctx.cfg80211){
            fprintf(stderr, "Invalid tag 'g' for current mode.\n");
            return -EINVAL;
        }
        sock_ctx.cfg80211 = 1;
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
        } else if (streql(p, "-set")) {
            subioctl_cmd = EXTENDED_SUBIOCTL_THERMAL_SET_PARAM;
        } else if (streql(p, "-get")) {
            subioctl_cmd = EXTENDED_SUBIOCTL_THERMAL_GET_PARAM;
            break;
        } else if (streql(p, "-i")) {
            ++argi;
            if (strlcpy(ifname, argv[argi], IFNAMSIZ) >= IFNAMSIZ) {
                fprintf(stderr, "Interface name too long %s\n", argv[argi]);
                return -1;
            }
        } else if (streql(p, "-e")) {
            ++argi;
            val = atoi(argv[argi]);
            if ((val == THERMAL_MITIGATION_DISABLED) || (val == THERMAL_MITIGATION_ENABLED)) {
                validate_and_set(extended_cmd.ext_data.th_config.enable, val);
            } else {
                parse_err = TH_TRUE;
            }
        } else if (streql(p, "-et")) {
            ++argi;
            val = atoi(argv[argi]);
            if (val > 0) {
                validate_and_set(extended_cmd.ext_data.th_config.dc_per_event, val);
            } else {
                parse_err = TH_TRUE;
            }
        } else if (streql(p, "-dl")) {
            ++argi;
            val = atoi(argv[argi]);
            if (val > 0) {
                validate_and_set(extended_cmd.ext_data.th_config.log_lvl, val);
            } else {
                parse_err = TH_TRUE;
            }
        } else if (streql(p, duty_cycle_str)) {
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
        print_usage();
        return -1;
    }

    init_socket_context(&sock_ctx, THERMAL_NL80211_CMD_SOCK_ID, THERMAL_NL80211_EVENT_SOCK_ID);
    extended_cmd.cmd = subioctl_cmd;

    if (subioctl_cmd == EXTENDED_SUBIOCTL_THERMAL_SET_PARAM) {
        extended_cmd.data_len = sizeof(extended_cmd.ext_data.th_config);
    } else if (subioctl_cmd == EXTENDED_SUBIOCTL_THERMAL_GET_PARAM) {
        extended_cmd.data_len = sizeof(extended_cmd.ext_data.get_param);
    }

    ext_send_command(ifname, &extended_cmd, sizeof(extended_cmd));

    if (subioctl_cmd == EXTENDED_SUBIOCTL_THERMAL_GET_PARAM) {
        p_config = &(extended_cmd.ext_data.get_param.th_cfg);
        p_stats = &(extended_cmd.ext_data.get_param.th_stats);

        fprintf(stdout, "Thermal config for %s\n", ifname);
        fprintf(stdout, "  enable: %d, dc: %d, dc_per_event: %d\n",p_config->enable, p_config->dc, p_config->dc_per_event);
        for (i = 0; i < THERMAL_LEVELS; i++) {
            fprintf(stdout, "  level: %d, low threshold: %d, high threshold: %d, dcoffpercent: %d, queue priority %d, policy; %d\n",
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
    destroy_socket_context(&sock_ctx);
    return 0;
}

int main(int argc, char *argv[])
{
    wifi_channel_switch_request_t channel_switch_req = { 0 };
    wifi_scan_custom_request_cmd_t channel_scan_req = { 0 };
    wifi_repeater_move_request_t repeater_move_req = { 0 };
    struct extended_ioctl_wrapper extended_cmd;
    char ifname[IFNAMSIZ] = {'\0', };
    unsigned int subioctl_cmd = EXTENDED_SUBIOCTL_INVALID;
    unsigned int val = 0;
    int option = -1;
    int index = -1;
    int count = 0;
    int i = 0;
    int bssid[BSSID_LEN];

    if (strcmp (argv[0], "thermaltool") == 0) {
        handle_thermal_main (argc, argv);
    }

    static struct option exttool_long_options[] = {
        /* List of options supported by this tool */
        {"help",          no_argument,       NULL, 'h'},
        {"interface",     required_argument, NULL, 'i'},
        {"chanswitch",    no_argument,       NULL, 'c'},
        {"scan",          no_argument,       NULL, 's'},
        {"chan",          required_argument, NULL, 'a'},
        {"chwidth",       required_argument, NULL, 'w'},
        {"numcsa",        required_argument, NULL, 'n'},
        {"secoffset",     required_argument, NULL, 'o'},
        {"cfreq2",        required_argument, NULL, 'f'},
        {"wideband",      required_argument, NULL, 'u'},
        {"mindwell",      required_argument, NULL, 'd'},
        {"maxdwell",      required_argument, NULL, 'x'},
        {"resttime",      required_argument, NULL, 'r'},
        {"scanmode",      required_argument, NULL, 'm'},
        {"cfg80211",      no_argument      , NULL, 'g'},
        {"chcount",       required_argument, NULL, 'e'},
        {"rptrmove",      no_argument,       NULL, 'p'},
        {"ssid",          required_argument, NULL, 'q'},
        {"bssid",         required_argument, NULL, 'b'},
        {"maxscantime",   required_argument, NULL, 't'},
        {"idletime",      required_argument, NULL, 'l'},
        {0,               0,                 0,     0},
    };

    if (argc < 6) {
        print_usage();
        return -1;
    }

    memset(&extended_cmd.ext_data.channel_switch_req, 0, sizeof(wifi_channel_switch_request_t));
    memset(&extended_cmd.ext_data.channel_scan_req, 0, sizeof(wifi_scan_custom_request_cmd_t));

    /* Set channel width by default to MAX. Incase channel switch is triggered
     * with no channel width information provided, we use the channel width
     * of the current operating channel
     */
    extended_cmd.ext_data.channel_switch_req.target_chanwidth = WIFI_OPERATING_CHANWIDTH_MAX_ELEM;

    /*
     * Based on driver config mode (cfg80211/wext), application also runs
     * in same mode (wext/cfg80211)
     */
    sock_ctx.cfg80211 =  get_config_mode_type();

    while (TRUE)
    {
        option = getopt_long (argc, argv, "hi:csa:w:n:o:f:u:d:x:r:m:ge:pq:b:t:l",
                exttool_long_options, &index);

        if (option == -1) {
            break;
        }

        switch (option) {
            case 'h': /* Help */
                print_usage();
                break;
            case 'g': /*cfg80211*/
                if (!sock_ctx.cfg80211){
                    fprintf(stderr,"Invalid tag '-g' for current mode.\n");
                    return -EINVAL;
                }
                sock_ctx.cfg80211 = 1;
                break;
            case 'i': /* Interface name */
                if (strlcpy(ifname, optarg, IFNAMSIZ) >= IFNAMSIZ) {
                    printf("interface name too long");
                    return -1;
                }
                if (strncmp(ifname, "wifi", 4) != 0) {
                    fprintf(stdout, "Invalid interface name %s \n", ifname);
                    print_usage();
                    return -1;
                }
                break;
            case 'c': /* Channel switch request */
                subioctl_cmd = EXTENDED_SUBIOCTL_CHANNEL_SWITCH;
                break;
            case 's': /* Channel scan request */
                subioctl_cmd = EXTENDED_SUBIOCTL_CHANNEL_SCAN;
                break;
            case 'p': /* Repeater move request */
                subioctl_cmd = EXTENDED_SUBIOCTL_REPEATER_MOVE;
                break;
            case 'w': /* New channel width */
                extended_cmd.ext_data.channel_switch_req.target_chanwidth = atoi(optarg);
                break;
            case 'a': /* New primary 20 centre channel number */
                extended_cmd.ext_data.channel_switch_req.target_pchannel = atoi(optarg);
                break;
            case 'f': /* New secondary 80 MHz channel centre frequency for 80+80 */
                extended_cmd.ext_data.channel_switch_req.target_cfreq2 = atoi(optarg);
                break;
            case 'o': /* Secondary 20 is above or below primary 20 channel */
                extended_cmd.ext_data.channel_switch_req.sec_chan_offset = atoi(optarg);
                break;
            case 'n': /* Number of channel switch announcement frames */
                extended_cmd.ext_data.channel_switch_req.num_csa = atoi(optarg);
                break;
            case 'd': /* min dwell time  */
                extended_cmd.ext_data.channel_scan_req.min_dwell_time = atoi(optarg);
                break;
            case 'x': /* max dwell time  */
                extended_cmd.ext_data.channel_scan_req.max_dwell_time = atoi(optarg);
                break;
            case 'r': /* rest time  */
                extended_cmd.ext_data.channel_scan_req.rest_time = atoi(optarg);
                break;
            case 'm': /* scan active mode  */
                extended_cmd.ext_data.channel_scan_req.scan_mode = atoi(optarg);
                break;
            case 'u': /* wide band scan */
                extended_cmd.ext_data.channel_scan_req.wide_band = atoi(optarg);
                break;
            case 't': /* max scan time  */
                extended_cmd.ext_data.channel_scan_req.max_scan_time = atoi(optarg);
                break;
            case 'l': /* idle time  */
                extended_cmd.ext_data.channel_scan_req.idle_time = atoi(optarg);
                break;
            case 'e': /* channel to scan  */
                extended_cmd.ext_data.channel_scan_req.chanlist.n_chan = atoi(optarg);
                while (count < extended_cmd.ext_data.channel_scan_req.chanlist.n_chan) {
                    if (argv[optind] == NULL) {
                        fprintf(stdout, "Channel list invalid - Please provide %d channel(s) \n",
                                extended_cmd.ext_data.channel_scan_req.chanlist.n_chan);
                        return -1;
                    }
                    val = atoi(argv[optind]);
                    extended_cmd.ext_data.channel_scan_req.chanlist.chan[count] =
                        val & SCAN_CHANNEL_MASK;
                    extended_cmd.ext_data.channel_scan_req.chanlist.chan_width[count] =
                        ((val & SCAN_CHANNEL_WIDTH_MASK) >> SCAN_CHANNEL_WIDTH_SHIFT);
                    count++;
                    optind++;
                }
                break;
            case 'q': /* SSID information of new Root AP */
                if (!(strlen(optarg) < sizeof(repeater_move_req.target_ssid.ssid))) {
                    fprintf(stdout, "SSID length invalid %s \n", optarg);
                    return -1;
                }
		strlcpy((char *)extended_cmd.ext_data.rep_move_req.target_ssid.ssid, optarg,
                        sizeof(extended_cmd.ext_data.rep_move_req.target_ssid.ssid));
                extended_cmd.ext_data.rep_move_req.target_ssid.ssid_len = strlen(optarg);
                break;
            case 'b': /* BSSID information of new Root AP */
                if ((strlen(optarg) != 17)) {
                    fprintf(stdout, "BSSID length invalid %s \n", optarg);
                    return -1;
                }
                /*
                 * sscanf reads formatted input from a string and expects
                 * pointers to integers passed to it. Since target bssid information
                 * is stored as uint8 array, we first parse the BSSID with local
                 * variable of type int and then assign it to the target bssid
                 * of repeater move request structure. This is done to prevent warnings
                 * if reported by any compiler
                 */
                if (sscanf(optarg, "%x:%x:%x:%x:%x:%x",
                            &bssid[0],
                            &bssid[1],
                            &bssid[2],
                            &bssid[3],
                            &bssid[4],
                            &bssid[5]) < 6) {
                    fprintf(stdout, "Could not parse BSSID: %s\n", optarg);
                    return -1;
                }
                for (i = 0; i < BSSID_LEN; i++) {
                    extended_cmd.ext_data.rep_move_req.target_bssid[i] = (uint8_t) bssid[i];
                }
                break;
            default:
                print_usage();
                break;
        }
    }

    if (subioctl_cmd == EXTENDED_SUBIOCTL_INVALID) {
        print_usage();
        return -1;
    }

    init_socket_context(&sock_ctx, EXT_NL80211_CMD_SOCK_ID, EXT_NL80211_EVENT_SOCK_ID);
    extended_cmd.cmd = subioctl_cmd;

    if (subioctl_cmd == EXTENDED_SUBIOCTL_CHANNEL_SWITCH) {
        extended_cmd.data_len = sizeof(channel_switch_req);
    } else if (subioctl_cmd == EXTENDED_SUBIOCTL_CHANNEL_SCAN) {
        if (validate_scan_param(&extended_cmd.ext_data.channel_scan_req)) {
            print_usage();
            destroy_socket_context(&sock_ctx);
            return -1;
        }
        extended_cmd.data_len = sizeof(channel_scan_req);
    } else if (subioctl_cmd == EXTENDED_SUBIOCTL_REPEATER_MOVE) {
        /* Assign channel switch request parameters in repeater move request */
        extended_cmd.ext_data.rep_move_req.chan_switch_req = extended_cmd.ext_data.channel_switch_req;
        extended_cmd.data_len = sizeof(repeater_move_req);
    }

    ext_send_command(ifname, (void *)&extended_cmd, sizeof(struct extended_ioctl_wrapper));
    destroy_socket_context(&sock_ctx);
    return 0;
}
