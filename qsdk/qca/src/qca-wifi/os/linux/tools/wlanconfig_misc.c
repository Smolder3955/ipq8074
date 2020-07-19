/*
 * Copyright (c) 2018 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 */

#include <wlanconfig.h>

#if ATH_SUPPORT_HYFI_ENHANCEMENTS

static int handle_ald(struct socket_context *sock_ctx, const char *ifname, IEEE80211_WLANCONFIG_CMDTYPE cmdtype,
        char *addr1, int enable)
{
    struct ieee80211_wlanconfig config;
    char ni_macaddr[MACSTR_LEN] = {0};
    uint8_t temp[MACSTR_LEN] = {0};

    if (addr1 && strlen(addr1) != 17) {
        printf("Invalid MAC address (format: xx:xx:xx:xx:xx:xx)\n");
        return -1;
    }

    if (addr1) {
        memset(ni_macaddr, '\0', sizeof(ni_macaddr));
        if(strlcpy(ni_macaddr, addr1, sizeof(ni_macaddr)) >= sizeof(ni_macaddr)) {
            printf("Invalid MAC address1\n");
            return -1;
        }
        if (ether_string2mac(temp, ni_macaddr) != 0) {
            printf("Invalid MAC address1\n");
            return -1;
        }
    }

    memset(&config, 0, sizeof config);
    /* fill up configuration */
    config.cmdtype = cmdtype;
    switch (cmdtype) {
        case IEEE80211_WLANCONFIG_ALD_STA_ENABLE:
            memcpy(config.data.ald.data.ald_sta.macaddr, temp, IEEE80211_ADDR_LEN);
            config.data.ald.data.ald_sta.enable = (u_int32_t)enable;
            break;
        default:
            printf("%d command not handled yet", cmdtype);
            break;
    }
    send_command(sock_ctx, ifname, &config, sizeof(struct ieee80211_wlanconfig),
            NULL, QCA_NL80211_VENDOR_SUBCMD_ALD_PARAMS, IEEE80211_IOCTL_CONFIG_GENERIC);

    return 0;
}

int handle_command_ald (int argc, char *argv[], const char *ifname,
    struct socket_context *sock_ctx)
{
    if (argc == 6 && streq(argv[3], "sta-enable")) {
        return handle_ald(sock_ctx, ifname, IEEE80211_WLANCONFIG_ALD_STA_ENABLE,
                argv[4], atoi(argv[5]));
    } else {
        errx(1, "invalid ALD command");
    }
    return 0;
}

static int handle_hmmc(struct socket_context *sock_ctx, const char *ifname,
    IEEE80211_WLANCONFIG_CMDTYPE cmdtype, char *ip_str, char *mask_str)
{
    int ip=0, mask=0;
    struct ieee80211_wlanconfig config;
    if (cmdtype == IEEE80211_WLANCONFIG_HMMC_ADD ||
            cmdtype == IEEE80211_WLANCONFIG_HMMC_DEL) {
        if ((ip = inet_addr(ip_str)) == -1 || !ip) {
            printf("Invalid ip string %s\n", ip_str);
            return -1;
        }

        if (!(mask = inet_addr(mask_str))) {
            printf("Invalid ip mask string %s\n", mask_str);
            return -1;
        }
    }

    config.cmdtype = cmdtype;
    switch (cmdtype) {
        case IEEE80211_WLANCONFIG_HMMC_ADD:
        case IEEE80211_WLANCONFIG_HMMC_DEL:
            config.data.hmmc.ip = ip;
            config.data.hmmc.mask = mask;
            break;
        case IEEE80211_WLANCONFIG_HMMC_DUMP:
            break;
        default:
            perror("invalid cmdtype");
            return -1;
    }
    send_command(sock_ctx, ifname, &config, sizeof(struct ieee80211_wlanconfig),
            NULL, QCA_NL80211_VENDOR_SUBCMD_HMMC, IEEE80211_IOCTL_CONFIG_GENERIC);

    return 0;
}

int handle_command_hmmc (int argc, char *argv[], const char *ifname,
    struct socket_context *sock_ctx)
{
    if (argc == 4 && streq(argv[3], "dump")) {
        return handle_hmmc(sock_ctx, ifname, IEEE80211_WLANCONFIG_HMMC_DUMP, NULL, NULL);
    } else if (argc == 6 && streq(argv[3], "add")) {
        return handle_hmmc(sock_ctx, ifname, IEEE80211_WLANCONFIG_HMMC_ADD, argv[4], argv[5]);
    } else if (argc == 6 && streq(argv[3], "del")) {
        return handle_hmmc(sock_ctx, ifname, IEEE80211_WLANCONFIG_HMMC_DEL, argv[4], argv[5]);
        errx(1, "invalid HMMC command");
    }
    return 0;
}
#endif

static int atf_show_stat(const char *ifname, char *macaddr)
{
    int s, i;
    struct iwreq iwr;
    struct ieee80211_wlanconfig config;
    uint8_t len, lbyte, ubyte;

    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) {
        printf("socket error\n");
        return 1;
    }

    memset(&config, 0, sizeof(config));
    config.cmdtype = IEEE80211_PARAM_STA_ATF_STAT;
    len = strlen(macaddr);
    for (i = 0; i < len; i += 2) {
        lbyte = ubyte = 0;

        if ((macaddr[i] >= '0') && (macaddr[i] <= '9'))  {
            ubyte = macaddr[i] - '0';
        } else if ((macaddr[i] >= 'A') && (macaddr[i] <= 'F')) {
            ubyte = macaddr[i] - 'A' + 10;
        } else if ((macaddr[i] >= 'a') && (macaddr[i] <= 'f')) {
            ubyte = macaddr[i] - 'a' + 10;
        }

        if ((macaddr[i + 1] >= '0') && (macaddr[i + 1] <= '9'))  {
            lbyte = macaddr[i + 1] - '0';
        } else if ((macaddr[i + 1] >= 'A') && (macaddr[i + 1] <= 'F')) {
            lbyte = macaddr[i + 1] - 'A' + 10;
        } else if ((macaddr[i + 1] >= 'a') && (macaddr[i + 1] <= 'f')) {
            lbyte = macaddr[i + 1] - 'a' + 10;
        }

        config.data.atf.macaddr[i / 2] = (ubyte << 4) | lbyte;
    }
    memset(&iwr, 0, sizeof(iwr));

    if (strlcpy(iwr.ifr_name, ifname, sizeof(iwr.ifr_name)) >= sizeof(iwr.ifr_name)) {
        fprintf(stderr, "ifname too long: %s\n", ifname);
        close(s);
        return -1;
    }

    iwr.u.data.pointer = (void *)&config;
    iwr.u.data.length = sizeof(config);

    if (ioctl(s, IEEE80211_IOCTL_CONFIG_GENERIC, &iwr) < 0) {
        printf("unable to get stats\n");
        close(s);
        return 1;
    }

    fprintf(stderr, "Short Average %d, Total Used Tokens %llu\n",
            config.data.atf.short_avg, config.data.atf.total_used_tokens);

    close(s);
    return 0;
}

int handle_command_atfstat (int argc, char *argv[], const char *ifname,
    struct socket_context *sock_ctx)
{
        if (argc > 3)
            atf_show_stat(ifname, argv[3]);
        else
            fprintf(stderr,"\n\n     Missing parameters!!  \n\n");
        return 0;
}

#if WLAN_CFR_ENABLE
static int handle_cfr_start_command(struct socket_context *sock_ctx, const char *ifname,
        char *peer_mac, int band_width, int periodicity, int capture_type)
{
    struct ieee80211_wlanconfig config;
    char macaddr[MACSTR_LEN];
    uint8_t temp[MACSTR_LEN];

    memset(temp, '\0', sizeof(temp));
    config.cmdtype = IEEE80211_WLANCONFIG_CFR_START;
    if(peer_mac != NULL) {
        memset(macaddr, '\0', sizeof(macaddr));
        if (strlcpy(macaddr, peer_mac, sizeof(macaddr)) >= sizeof(macaddr)) {
            printf("Invalid MAC address (format: xx:xx:xx:xx:xx:xx)\n");
            return -1;
        }

        if (ether_string2mac(temp, macaddr) != 0) {
            printf("Invalid MAC address\n");
            return -1;
        }
        memcpy(config.data.cfr_config.mac, temp, IEEE80211_ADDR_LEN);
    }

    config.data.cfr_config.bandwidth = band_width;
    config.data.cfr_config.periodicity = periodicity;
    config.data.cfr_config.capture_method = capture_type;

    send_command(sock_ctx, ifname, &config, sizeof(struct ieee80211_wlanconfig),
            NULL, QCA_NL80211_VENDOR_SUBCMD_CFR_CONFIG, IEEE80211_IOCTL_CONFIG_GENERIC);

    return 0;
}

static int handle_cfr_stop_command(struct socket_context *sock_ctx, const char *ifname,
        char *peer_mac)
{
    struct ieee80211_wlanconfig config;
    char macaddr[MACSTR_LEN];
    uint8_t temp[MACSTR_LEN];

    memset(temp, '\0', sizeof(temp));
    config.cmdtype = IEEE80211_WLANCONFIG_CFR_STOP;
    if(peer_mac != NULL) {
        memset(macaddr, '\0', sizeof(macaddr));
        if (strlcpy(macaddr, peer_mac, sizeof(macaddr)) >= sizeof(macaddr)) {
            printf("Invalid MAC address (format: xx:xx:xx:xx:xx:xx)\n");
            return -1;
        }

        if (ether_string2mac(temp, macaddr) != 0) {
            printf("Invalid MAC address\n");
            return -1;
        }
        memcpy(config.data.cfr_config.mac, temp, IEEE80211_ADDR_LEN);
    }

    send_command(sock_ctx, ifname, &config, sizeof(struct ieee80211_wlanconfig),
            NULL, QCA_NL80211_VENDOR_SUBCMD_CFR_CONFIG, IEEE80211_IOCTL_CONFIG_GENERIC);

    return 0;
}

static int handle_cfr_list_command(struct socket_context *sock_ctx, const char *ifname)
{
    /*
     * List all peers that are CFR enabled & Display all CFR capture properties
     */
    struct ieee80211_wlanconfig config;
    config.cmdtype = IEEE80211_WLANCONFIG_CFR_LIST_PEERS;
    send_command(sock_ctx, ifname, &config, sizeof(struct ieee80211_wlanconfig),
            NULL, QCA_NL80211_VENDOR_SUBCMD_CFR_CONFIG, IEEE80211_IOCTL_CONFIG_GENERIC);
    return 0;
}

int handle_command_cfr(int argc, char *argv[], const char *ifname,
        struct socket_context *sock_ctx)
{
    if (argc == 8 && streq(argv[3], "start")) {
        return handle_cfr_start_command(sock_ctx, ifname,
                argv[4], atoi(argv[5]), atoi(argv[6]), atoi(argv[7]));
    } else if (argc == 5 && streq(argv[3], "stop")) {
        return handle_cfr_stop_command(sock_ctx, ifname,
                argv[4]);
    } else if (argc == 4 && streq(argv[3], "list")) {
        return handle_cfr_list_command(sock_ctx, ifname);
    } else {
        printf("Invalid CFR command format: Usage \n");
        printf("\t wlanconfig athX cfr start <peer MAC> <bw> <periodicity> <capture type> \n") ;
        printf("\t wlanconfig athX cfr stop <peer MAC>\n") ;
    }

    return 0;
}
#endif
