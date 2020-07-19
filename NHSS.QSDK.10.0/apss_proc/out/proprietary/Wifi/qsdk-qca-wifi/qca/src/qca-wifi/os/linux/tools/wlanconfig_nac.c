/*
 * Copyright (c) 2018 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 */

#include <wlanconfig.h>
#if ATH_SUPPORT_NAC

static int handle_nac(struct socket_context *sock_ctx, const char *ifname,
    IEEE80211_WLANCONFIG_CMDTYPE cmdtype, int argc, char *argv[])
{
    int i, j, max_addrlimit = 0;
    struct ieee80211_wlanconfig config = {0};
    char macaddr[MACSTR_LEN + 1] = {0};
    uint8_t temp[MACSTR_LEN + 1] = {0};
    char nullmac[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};


    /* zero out configuration */
    memset(&config, 0, sizeof(struct ieee80211_wlanconfig));

    if (streq(argv[4], "bssid")) {

        max_addrlimit = NAC_MAX_BSSID;
        config.data.nac.mac_type = IEEE80211_NAC_MACTYPE_BSSID;

    } else if (streq(argv[4], "client")) {

        max_addrlimit = NAC_MAX_CLIENT;
        config.data.nac.mac_type = IEEE80211_NAC_MACTYPE_CLIENT;
    }

    if (cmdtype == IEEE80211_WLANCONFIG_NAC_ADDR_ADD ||
            cmdtype == IEEE80211_WLANCONFIG_NAC_ADDR_DEL) {

        for(i=0, j=5; i < max_addrlimit && j < argc ; i++, j++) {

            memset(macaddr, '\0', sizeof(macaddr));
            if (strlcpy(macaddr, argv[j] , sizeof(macaddr)) >= sizeof(macaddr)) {
                printf("Invalid MAC address (format: xx:xx:xx:xx:xx:xx)\n");
                return -1;
            }

            if (ether_string2mac(temp, macaddr) != 0) {
                printf( "Invalid MAC address\n");
                return -1;
            }

            memcpy(config.data.nac.mac_list[i], temp, IEEE80211_ADDR_LEN);

        }
    }

    /* fill up cmd */
    config.cmdtype = cmdtype;
    send_command(sock_ctx, ifname, &config, sizeof(struct ieee80211_wlanconfig),
            NULL, QCA_NL80211_VENDOR_SUBCMD_NAC, IEEE80211_IOCTL_CONFIG_GENERIC);

    if (cmdtype == IEEE80211_WLANCONFIG_NAC_ADDR_LIST) {
        /* output the current configuration */
        if(config.data.nac.mac_type == IEEE80211_NAC_MACTYPE_BSSID) {
            printf("NAC BSSID configuration: \n");

        } else {
            printf("NAC CLIENT configuration: \n");
        }

        for (i = 0; i < max_addrlimit; i++) {
            if (memcmp(config.data.nac.mac_list[i], nullmac, IEEE80211_ADDR_LEN) != 0) {
                printf("%d-  %02x:%02x:%02x:%02x:%02x:%02x ",
                        i+1,
                        config.data.nac.mac_list[i][0], config.data.nac.mac_list[i][1],
                        config.data.nac.mac_list[i][2], config.data.nac.mac_list[i][3],
                        config.data.nac.mac_list[i][4], config.data.nac.mac_list[i][5]);
                if (config.data.nac.mac_type == IEEE80211_NAC_MACTYPE_CLIENT) {
                    printf("rssi %d \n", config.data.nac.rssi[i]);
                } else {
                    printf("\n");
                }
            }
        }
    }
    return 0;
}

int handle_command_nac (int argc, char *argv[], const char *ifname,
        struct socket_context *sock_ctx)
{
    if(streq(argv[3], "add") && streq(argv[4], "bssid") && (argc >= 6 && argc <=13)){
        return handle_nac (sock_ctx, ifname, IEEE80211_WLANCONFIG_NAC_ADDR_ADD,
                argc , argv);
    } else if(streq(argv[3], "add") && streq(argv[4], "client") && (argc >= 6 && argc <=29)){
        return handle_nac (sock_ctx, ifname, IEEE80211_WLANCONFIG_NAC_ADDR_ADD,
                argc , argv);
    } else if(streq(argv[3], "del") && streq(argv[4], "bssid") && (argc >= 6 && argc <=13)){
        return handle_nac (sock_ctx, ifname, IEEE80211_WLANCONFIG_NAC_ADDR_DEL,
                argc , argv);
    } else if(streq(argv[3], "del") && streq(argv[4], "client") && (argc >= 6 && argc <=29)){
        return handle_nac (sock_ctx, ifname, IEEE80211_WLANCONFIG_NAC_ADDR_DEL,
                argc , argv);
    } else if(streq(argv[3], "list") && (argc == 5 )){
        return handle_nac (sock_ctx, ifname, IEEE80211_WLANCONFIG_NAC_ADDR_LIST,
                argc , argv);
    } else {
        errx(1, "Invalid nac command , check wlanconfig help");
    }
    return 0;
}
#endif

#if ATH_SUPPORT_NAC_RSSI

static int handle_nac_rssi(struct socket_context *sock_ctx, const char *ifname,
    IEEE80211_WLANCONFIG_CMDTYPE cmdtype, int argc, char *argv[])
{
    struct iwreq iwr;
    struct ieee80211_wlanconfig config;
    char macaddr[MACSTR_LEN + 1] = {0};
    uint8_t temp[MACSTR_LEN + 1] = {0};
    char nullmac[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    memset(&iwr, 0, sizeof(struct iwreq));

    if (strlcpy(iwr.ifr_name, ifname, sizeof(iwr.ifr_name)) >= sizeof(iwr.ifr_name)) {
        fprintf(stderr, "ifname too long: %s\n", ifname);
        return -1;
    }

    /* zero out configuration */
    memset(&config, 0, sizeof(struct ieee80211_wlanconfig));

    if (cmdtype == IEEE80211_WLANCONFIG_NAC_RSSI_ADDR_ADD ||
            cmdtype == IEEE80211_WLANCONFIG_NAC_RSSI_ADDR_DEL) {

        if (strlcpy(macaddr, argv[5] , sizeof(macaddr)) >= sizeof(macaddr)) {
            printf("Invalid MAC address (format: xx:xx:xx:xx:xx:xx), %s\n", argv[5]);
            return -1;
        }

        if (ether_string2mac(temp, macaddr) != 0) {
            printf( "Invalid MAC address\n");
            return -1;
        }

        memcpy(config.data.nac_rssi.mac_bssid, temp, IEEE80211_ADDR_LEN);

        if (strlcpy(macaddr, argv[7] , sizeof(macaddr)) >= sizeof(macaddr)) {
            printf("Invalid MAC address (format: xx:xx:xx:xx:xx:xx), %s\n", argv[7]);
            return -1;
        }

        if (ether_string2mac(temp, macaddr) != 0) {
            printf( "Invalid MAC address\n");
            return -1;
        }

        memcpy(config.data.nac_rssi.mac_client, temp, IEEE80211_ADDR_LEN);
    }
    if (cmdtype == IEEE80211_WLANCONFIG_NAC_RSSI_ADDR_ADD) {
        config.data.nac_rssi.chan_num = atoi(argv[9]);
    }
    /* fill up cmd */
    config.cmdtype = cmdtype;

    send_command(sock_ctx, ifname, &config, sizeof(struct ieee80211_wlanconfig),
            NULL, QCA_NL80211_VENDOR_SUBCMD_NAC_RSSI, IEEE80211_IOCTL_CONFIG_GENERIC);

    if (cmdtype == IEEE80211_WLANCONFIG_NAC_RSSI_ADDR_LIST) {
        /* output the current configuration */
        printf("RSSI_NAC: BSSID              Client             Channel  Avg_RSSI \n");
        if ((memcmp(config.data.nac_rssi.mac_bssid, nullmac, IEEE80211_ADDR_LEN) != 0)
                && (memcmp(config.data.nac_rssi.mac_client, nullmac, IEEE80211_ADDR_LEN) != 0)){
            printf("          %02x:%02x:%02x:%02x:%02x:%02x  %02x:%02x:%02x:%02x:%02x:%02x   %d        %d\n",
                    config.data.nac_rssi.mac_bssid[0], config.data.nac_rssi.mac_bssid[1],
                    config.data.nac_rssi.mac_bssid[2], config.data.nac_rssi.mac_bssid[3],
                    config.data.nac_rssi.mac_bssid[4], config.data.nac_rssi.mac_bssid[5],
                    config.data.nac_rssi.mac_client[0], config.data.nac_rssi.mac_client[1],
                    config.data.nac_rssi.mac_client[2], config.data.nac_rssi.mac_client[3],
                    config.data.nac_rssi.mac_client[4], config.data.nac_rssi.mac_client[5],
                    config.data.nac_rssi.chan_num,
                    (config.data.nac_rssi.client_rssi_valid != 0) ? config.data.nac_rssi.client_rssi : 0);
        }
    }

    return 0;
}

int handle_command_nac_rssi (int argc, char *argv[], const char *ifname,
        struct socket_context *sock_ctx)
{
    if(streq(argv[3], "add") && streq(argv[4], "bssid") &&
            streq(argv[6], "client") && streq(argv[8], "channel") && (argc == 10)) {
        return handle_nac_rssi (sock_ctx, ifname, IEEE80211_WLANCONFIG_NAC_RSSI_ADDR_ADD,
                argc , argv);

    } else if(streq(argv[3], "del") && streq(argv[4], "bssid") &&
            streq(argv[6], "client")&& ( argc ==8)){
        return handle_nac_rssi (sock_ctx, ifname, IEEE80211_WLANCONFIG_NAC_RSSI_ADDR_DEL,
                argc , argv);

    } else if(streq(argv[3], "show_rssi") && (argc == 4 )){
        return handle_nac_rssi (sock_ctx, ifname, IEEE80211_WLANCONFIG_NAC_RSSI_ADDR_LIST,
                argc , argv);
    } else {
        errx(1, "Invalid rssi_nac command , check wlanconfig help");
    }
    return 0;
}
#endif
