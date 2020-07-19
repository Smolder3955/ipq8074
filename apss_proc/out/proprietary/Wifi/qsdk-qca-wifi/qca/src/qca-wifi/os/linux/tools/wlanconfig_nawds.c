/*
 * Copyright (c) 2018 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 */

#if UMAC_SUPPORT_NAWDS

#include <wlanconfig.h>

static int handle_nawds(struct socket_context *sock_ctx, const char *ifname,
        IEEE80211_WLANCONFIG_CMDTYPE cmdtype, char *addr, int value, char *psk)
{
    int i;
    struct ieee80211_wlanconfig config;
    char macaddr[MACSTR_LEN];
    uint8_t temp[MACSTR_LEN];

    memset(temp, '\0', sizeof(temp));
    if ((cmdtype == IEEE80211_WLANCONFIG_NAWDS_SET_ADDR ||
            cmdtype == IEEE80211_WLANCONFIG_NAWDS_CLR_ADDR ||
            cmdtype == IEEE80211_WLANCONFIG_NAWDS_KEY) && addr != NULL) {

        if(addr != NULL) {
            memset(macaddr, '\0', sizeof(macaddr));
            if (strlcpy(macaddr, addr, sizeof(macaddr)) >= sizeof(macaddr)) {
                printf("Invalid MAC address (format: xx:xx:xx:xx:xx:xx)\n");
                return -1;
            }

            if (ether_string2mac(temp, macaddr) != 0) {
                printf("Invalid MAC address\n");
                return -1;
            }
        }
    }

    /* fill up configuration */
    memset(&config, 0, sizeof(struct ieee80211_wlanconfig));
    config.cmdtype = cmdtype;
    switch (cmdtype) {
        case IEEE80211_WLANCONFIG_NAWDS_SET_MODE:
            config.data.nawds.mode = value;
            break;
        case IEEE80211_WLANCONFIG_NAWDS_SET_DEFCAPS:
            config.data.nawds.defcaps = value;
            break;
        case IEEE80211_WLANCONFIG_NAWDS_SET_OVERRIDE:
            config.data.nawds.override = value;
            break;
        case IEEE80211_WLANCONFIG_NAWDS_SET_ADDR:
            memcpy(config.data.nawds.mac, temp, IEEE80211_ADDR_LEN);
            config.data.nawds.caps = value;
            break;
        case IEEE80211_WLANCONFIG_NAWDS_CLR_ADDR:
            memcpy(config.data.nawds.mac, temp, IEEE80211_ADDR_LEN);
            break;
        case IEEE80211_WLANCONFIG_NAWDS_GET:
            config.data.nawds.num = 0;
            break;
        case IEEE80211_WLANCONFIG_NAWDS_KEY:
            if (addr != NULL)
                memcpy(config.data.nawds.mac, temp, IEEE80211_ADDR_LEN);
            if ((strlen(psk) == 16) || (strlen(psk) == 32)) {
                memset(config.data.nawds.psk, 0, sizeof(config.data.nawds.psk));
                memcpy(config.data.nawds.psk, psk, strlen(psk));
            } else {
                printf("invalid aes psk length\n"); /* valid string lengths: 16/32  */
                return -1;
            }
            break;
        default:
            errx(1, "invalid NAWDS command");
            break;
    }
    /* call NL80211 Vendor commnd frame work */
    send_command(sock_ctx, ifname, &config, sizeof(struct ieee80211_wlanconfig),
            NULL, QCA_NL80211_VENDOR_SUBCMD_NAWDS_PARAMS, IEEE80211_IOCTL_CONFIG_GENERIC);
    if (cmdtype == IEEE80211_WLANCONFIG_NAWDS_GET) {
        /* output the current configuration */
        printf("NAWDS configuration: \n");
        printf("Num     : %d\n", config.data.nawds.num);
        printf("Mode    : %d\n", config.data.nawds.mode);
        printf("Defcaps : %d\n", config.data.nawds.defcaps);
        printf("Override: %d\n", config.data.nawds.override);
        for (i = 0; i < config.data.nawds.num; i++) {
            config.data.nawds.num = i;
            send_command(sock_ctx, ifname, &config, sizeof(struct ieee80211_wlanconfig),
                    NULL, QCA_NL80211_VENDOR_SUBCMD_NAWDS_PARAMS, IEEE80211_IOCTL_CONFIG_GENERIC);
            printf("%d: %02x:%02x:%02x:%02x:%02x:%02x %x\n",
                    i,
                    config.data.nawds.mac[0], config.data.nawds.mac[1],
                    config.data.nawds.mac[2], config.data.nawds.mac[3],
                    config.data.nawds.mac[4], config.data.nawds.mac[5],
                    config.data.nawds.caps);
        }
    }

    return 0;
}

int handle_command_nawds (int argc, char *argv[], const char *ifname,
    struct socket_context *sock_ctx)
{
    if (argc == 5 && streq(argv[3], "mode")) {
        return handle_nawds(sock_ctx, ifname, IEEE80211_WLANCONFIG_NAWDS_SET_MODE,
                NULL, atoi(argv[4]), NULL);
    } else if (argc == 5 && streq(argv[3], "defcaps")) {
        return handle_nawds(sock_ctx, ifname, IEEE80211_WLANCONFIG_NAWDS_SET_DEFCAPS,
                NULL, strtoul(argv[4], NULL, 0), argv[5]);
    } else if (argc == 5 && streq(argv[3], "override")) {
        return handle_nawds(sock_ctx, ifname, IEEE80211_WLANCONFIG_NAWDS_SET_OVERRIDE,
                NULL, atoi(argv[4]), NULL);
    } else if (argc == 6 && streq(argv[3], "add-repeater")) {
        return handle_nawds(sock_ctx, ifname, IEEE80211_WLANCONFIG_NAWDS_SET_ADDR,
                argv[4], strtoul(argv[5], NULL, 0), NULL);
    } else if (argc == 5 && streq(argv[3], "del-repeater")) {
        return handle_nawds(sock_ctx, ifname, IEEE80211_WLANCONFIG_NAWDS_CLR_ADDR,
                argv[4], 0, NULL);
    } else if (argc == 4 && streq(argv[3], "list")) {
        return handle_nawds(sock_ctx, ifname, IEEE80211_WLANCONFIG_NAWDS_GET,
                argv[4], 0, NULL);
    } else if (argc == 6 && streq(argv[3], "addnawdskey")) {
	if(strcmp(argv[4], "-r") == 0)
	    return handle_nawds(sock_ctx, ifname, IEEE80211_WLANCONFIG_NAWDS_KEY,
                NULL, 0, argv[5]);
	else
            return handle_nawds(sock_ctx, ifname, IEEE80211_WLANCONFIG_NAWDS_KEY,
                argv[4], 0, argv[5]);
    } else {
        errx(1, "invalid NAWDS command");
    }
    return 0;
}
#endif
