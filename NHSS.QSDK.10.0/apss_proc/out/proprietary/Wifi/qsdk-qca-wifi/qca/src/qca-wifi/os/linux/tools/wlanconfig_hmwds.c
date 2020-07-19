/*
 * Copyright (c) 2018 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 */

#if ATH_SUPPORT_HYFI_ENHANCEMENTS

#include <wlanconfig.h>

static int handle_hmwds(struct socket_context *sock_ctx, const char *ifname,
    IEEE80211_WLANCONFIG_CMDTYPE cmdtype, char *addr1, char *addr2)
{
    int i;
    struct ieee80211_wlanconfig *config;
    char wds_ni_macaddr[MACSTR_LEN] = {0}, wds_macaddr[MACSTR_LEN] = {0};
    char data[3 * 1024] = {0};
    uint8_t temp1[MACSTR_LEN] = {0}, temp2[MACSTR_LEN] = {0};

    if (addr1 && strlen(addr1) != 17) {
        printf("Invalid MAC address (format: xx:xx:xx:xx:xx:xx)\n");
        return -1;
    }

    if (addr2 && strlen(addr2) != 17) {
        printf("Invalid MAC address (format: xx:xx:xx:xx:xx:xx)\n");
        return -1;
    }

    if (cmdtype == IEEE80211_WLANCONFIG_HMWDS_REMOVE_ADDR ||
            cmdtype == IEEE80211_WLANCONFIG_HMWDS_SET_BRIDGE_ADDR) {
        if (addr1) {
            memset(wds_macaddr, '\0', sizeof(wds_macaddr));
            if (strlcpy(wds_macaddr, addr1, sizeof(wds_macaddr)) >= sizeof(wds_macaddr)) {
                printf("Invalid MAC address1\n");
                return -1;
            }
            if (ether_string2mac(temp1, wds_macaddr) != 0) {
                printf("Invalid MAC address1\n");
                return -1;
            }
        }

    } else {
        if (addr1) {
            memset(wds_ni_macaddr, '\0', sizeof(wds_ni_macaddr));
            if (strlcpy(wds_ni_macaddr, addr1, sizeof(wds_ni_macaddr)) >= sizeof(wds_ni_macaddr)) {
                printf("Invalid MAC address1\n");
                return -1;
            }
            if (ether_string2mac(temp2, wds_ni_macaddr) != 0) {
                printf("Invalid MAC address1\n");
                return -1;
            }
        }

        if (addr2) {
            memset(wds_macaddr, '\0', sizeof(wds_macaddr));
            if (strlcpy(wds_macaddr, addr2, sizeof(wds_macaddr)) >= sizeof(wds_macaddr)) {
                printf("Invalid MAC address1\n");
                return -1;
            }
            if (ether_string2mac(temp1, wds_macaddr) != 0) {
                printf("Invalid MAC address2\n");
                return -1;
            }
        }
    }

    config = (struct ieee80211_wlanconfig *)data;
    memset(config, 0, sizeof *config);
    /* fill up configuration */
    config->cmdtype = cmdtype;
    switch (cmdtype) {
        case IEEE80211_WLANCONFIG_HMWDS_ADD_ADDR:
            memcpy(config->data.hmwds.wds_ni_macaddr, temp2, IEEE80211_ADDR_LEN);
            config->data.hmwds.wds_macaddr_cnt = 1;
            memcpy(config->data.hmwds.wds_macaddr, temp1, IEEE80211_ADDR_LEN);
            break;
        case IEEE80211_WLANCONFIG_HMWDS_RESET_ADDR:
            memcpy(config->data.hmwds.wds_ni_macaddr, temp2, IEEE80211_ADDR_LEN);
            break;
        case IEEE80211_WLANCONFIG_HMWDS_RESET_TABLE:
            break;
        case IEEE80211_WLANCONFIG_HMWDS_READ_ADDR:
            memcpy(config->data.hmwds.wds_ni_macaddr, temp2, IEEE80211_ADDR_LEN);
            break;
        case IEEE80211_WLANCONFIG_HMWDS_READ_TABLE:
            break;
        case IEEE80211_WLANCONFIG_HMWDS_REMOVE_ADDR:
            memcpy(config->data.hmwds.wds_macaddr, temp1, IEEE80211_ADDR_LEN);
            break;
        case IEEE80211_WLANCONFIG_HMWDS_SET_BRIDGE_ADDR:
            memcpy(config->data.hmwds.wds_macaddr, temp1, IEEE80211_ADDR_LEN);
            break;
        case IEEE80211_WLANCONFIG_HMWDS_DUMP_WDS_ADDR:
            break;
        default:
            printf("%d command not handled yet", cmdtype);
            break;
    }

    /* fill up request */
    send_command(sock_ctx, ifname, config, sizeof(data),
            NULL, QCA_NL80211_VENDOR_SUBCMD_HMWDS_PARAMS, IEEE80211_IOCTL_CONFIG_GENERIC);
    switch (cmdtype) {
        case IEEE80211_WLANCONFIG_HMWDS_READ_ADDR:
            /* print MAC of host managed WDS node */
            printf("HOST MANAGED WDS nodes: %d\n", config->data.hmwds.wds_macaddr_cnt);
            for (i = 0; i < config->data.hmwds.wds_macaddr_cnt; i++) {
                printf("\t%02x:%02x:%02x:%02x:%02x:%02x\n",
                        *(config->data.hmwds.wds_macaddr + i * IEEE80211_ADDR_LEN),
                        *(config->data.hmwds.wds_macaddr + i * IEEE80211_ADDR_LEN + 1),
                        *(config->data.hmwds.wds_macaddr + i * IEEE80211_ADDR_LEN + 2),
                        *(config->data.hmwds.wds_macaddr + i * IEEE80211_ADDR_LEN + 3),
                        *(config->data.hmwds.wds_macaddr + i * IEEE80211_ADDR_LEN + 4),
                        *(config->data.hmwds.wds_macaddr + i * IEEE80211_ADDR_LEN + 5));
            }
            break;
        case IEEE80211_WLANCONFIG_HMWDS_READ_TABLE:
            printf("WDS nodes: \n");
            printf("DA\t\t\tNext Hop\t\tFlags: \n");
            struct ieee80211_wlanconfig_wds_table *wds_table = &config->data.wds_table;
            for (i = 0; i < wds_table->wds_entry_cnt; i++) {
                struct ieee80211_wlanconfig_wds *wds_entry =
                    &wds_table->wds_entries[i];
                printf("%02x:%02x:%02x:%02x:%02x:%02x\t",
                        wds_entry->destmac[0], wds_entry->destmac[1],
                        wds_entry->destmac[2], wds_entry->destmac[3],
                        wds_entry->destmac[4], wds_entry->destmac[5]);
                printf("%02x:%02x:%02x:%02x:%02x:%02x\t",
                        wds_entry->peermac[0], wds_entry->peermac[1],
                        wds_entry->peermac[2], wds_entry->peermac[3],
                        wds_entry->peermac[4], wds_entry->peermac[5]);
                printf("0x%x\n", wds_entry->flags);
            }
            break;
        default:
            printf("%d command not handled yet", cmdtype);
            break;
    }

    return 0;
}

int handle_command_hmwds (int argc, char *argv[], const char *ifname,
    struct socket_context *sock_ctx)
{
    if (argc == 6 && streq(argv[3], "add-addr")) {
        return handle_hmwds(sock_ctx, ifname, IEEE80211_WLANCONFIG_HMWDS_ADD_ADDR,
                argv[4], argv[5]);
    } else if (argc == 5 && streq(argv[3], "reset-addr")) {
        return handle_hmwds(sock_ctx, ifname, IEEE80211_WLANCONFIG_HMWDS_RESET_ADDR,
                argv[4], NULL);
    } else if (argc == 4 && streq(argv[3], "reset-table")) {
        return handle_hmwds(sock_ctx, ifname, IEEE80211_WLANCONFIG_HMWDS_RESET_TABLE,
                NULL, NULL);
    } else if (argc == 5 && streq(argv[3], "read-addr")) {
        return handle_hmwds(sock_ctx, ifname, IEEE80211_WLANCONFIG_HMWDS_READ_ADDR,
                argv[4], NULL);
    } else if (argc == 4 && streq(argv[3], "read-table")) {
        return handle_hmwds(sock_ctx, ifname, IEEE80211_WLANCONFIG_HMWDS_READ_TABLE,
                NULL, NULL);
    } else if (argc == 5 && streq(argv[3], "rem-addr")) {
        return handle_hmwds(sock_ctx, ifname, IEEE80211_WLANCONFIG_HMWDS_REMOVE_ADDR,
                argv[4], NULL);
    } else if (argc == 5 && streq(argv[3], "set-braddr")) {
        return handle_hmwds(sock_ctx, ifname, IEEE80211_WLANCONFIG_HMWDS_SET_BRIDGE_ADDR,
                argv[4], NULL);
    } else if (argc == 4 && streq(argv[3], "dump-wdstable")) {
        return handle_hmwds(sock_ctx, ifname, IEEE80211_WLANCONFIG_HMWDS_DUMP_WDS_ADDR,
                argv[4], NULL);
    } else {
        errx(1, "invalid HMWDS command");
    }
    return 0;
}
#endif
