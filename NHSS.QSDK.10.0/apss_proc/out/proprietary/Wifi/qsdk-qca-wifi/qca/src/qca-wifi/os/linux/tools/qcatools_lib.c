/*
 * Copyright (c) 2018 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 */

/*
 * Including common library headerfile
 */
#include <qcatools_lib.h>

/*
 * ether_mac2string: converts array containing mac address into a printable string format.
 * @mac: input mac address array
 * @mac_string: output mac string
 * returns -1 if passed mac array is NULL or copy fails else returns 0
 */
int ether_mac2string(char *mac_string, const uint8_t mac[IEEE80211_ADDR_LEN])
{
    int i;
    if (mac) {
        i = snprintf(mac_string, MAC_STRING_LENGTH+1, "%02x:%02x:%02x:%02x:%02x:%02x",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    } else {
        return -1;
    }
    return i;
}

/*
 * ether_string2mac: converts mac address string into integer array format.
 * @mac_addr: mac string to be converted
 * @mac converted mac array
 * returns 0 on succesful conversion and -1 otherwise
 */
int ether_string2mac( uint8_t mac[IEEE80211_ADDR_LEN], const char *mac_addr)
{
    int i, j = 2;
    char mac_string[MAC_STRING_LENGTH+1];

    if (strlcpy(mac_string, mac_addr, sizeof(mac_string)) >= sizeof(mac_string)) {
        printf("Invalid MAC address");
        return -1;
    }

    for (i = 2; i < MAC_STRING_LENGTH; i += 3) {
        mac_string[j++] = mac_string[i + 1];
        mac_string[j++] = mac_string[i + 2];
    }

    for(i = 0; i < 12; i++) {
        /* check 0~9, A~F */
        mac_string[i] = ((mac_string[i] - 48) < 10) ? (mac_string[i] - 48) : (mac_string[i] - 55);
        /* check a~f */
        if (mac_string[i] >= 42)
            mac_string[i] -= 32;
        if (mac_string[i] > 0xf)
            return -1;
    }

    for(i = 0; i < 6; i++) {
        mac[i] = (mac_string[(i<<1)] << 4) + mac_string[(i<<1)+1];
    }

    return 0;
}

/*
 * power: computes power using exponentiation by squaring method.
 * @index: index of which power has to be computed
 * @exponent: exponet to which power has to be raised
 * returns the resultant value of index raised to exponent
 */
long long int power (int index, int exponent)
{
    long long int temp;
    if (exponent == 0) return 1;
    if (exponent == 1) return index;

    temp = power(index, exponent / 2);

    if (exponent % 2 == 0) {
        return temp * temp;
    } else {
        if (exponent > 0){
            return index * temp * temp;
        } else {
            return (temp * temp) / index;
        }
    }
    return temp;
}

/*
 * print_hex_buffer: prints a buffer in hex format - used for dumping test events.
 * @buf: buffer to be printed
 * @len: length of buffer to be printed
 */
void print_hex_buffer(void *buf, int len)
{
    int cnt;
    for (cnt = 0; cnt < len; cnt++) {
        if (cnt % 8 == 0) {
            printf("\n");
        }
        printf("%02x ",((uint8_t *)buf) [cnt]);
    }
    fflush(stdout);
}

#if UMAC_SUPPORT_CFG80211
int start_event_thread (struct socket_context *sock_ctx)
{
    if (!sock_ctx->cfg80211) {
        return 0;
    }
    return wifi_nl80211_start_event_thread(&(sock_ctx->cfg80211_ctxt));
}
#endif

/*
 * get_config_mode_type: function that detects current config type
 *     and returns enum value corresponding to driver mode.
 *     returns CONFIG_CFG80211 if driver is in cfg mode,
 *     or CONFIG_IOCTL if driver is in wext mode.
 */
enum config_mode_type get_config_mode_type()
{
    int fd = -1;
    char filename[FILE_NAME_LENGTH];
    int radio;
    config_mode_type ret = CONFIG_IOCTL;

    for (radio = 0; radio < MAX_WIPHY; radio++) {
        snprintf(filename, sizeof(filename),"/sys/class/net/wifi%d/phy80211/",radio);
        fd = open(filename, O_RDONLY);
        if (fd > 0) {
            ret = CONFIG_CFG80211;
            close(fd);
            break;
        }
    }

    return ret;
}

/*
 * send_command; function to send the cfg command or ioctl command.
 * @sock_ctx: socket context
 * @ifname :interface name
 * @buf: buffer
 * @buflen : buffer length
 * @callback: callback that needs to be called when data recevied from driver
 * @cmd : command type
 * @ioctl_cmd: ioctl command type
 * returns 0 if success; otherwise negative value on failure
 */
int send_command (struct socket_context *sock_ctx, const char *ifname, void *buf,
        size_t buflen, void (*callback) (struct cfg80211_data *arg), int cmd, int ioctl_cmd)
{
#if UMAC_SUPPORT_WEXT
    struct iwreq iwr;
    int sock_fd, err;
#endif
#if UMAC_SUPPORT_CFG80211
    int msg;
    struct cfg80211_data buffer;
#endif
    if (sock_ctx->cfg80211) {
#if UMAC_SUPPORT_CFG80211
        buffer.data = buf;
        buffer.length = buflen;
        buffer.callback = callback;
        buffer.parse_data = 0;
        msg = wifi_cfg80211_send_generic_command(&(sock_ctx->cfg80211_ctxt),
                QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_CONFIGURATION,
                cmd, ifname, (char *)&buffer, buflen);
        if (msg < 0) {
            printf("Could not send NL command\n");
            return -1;
        }
        return buffer.length;
#endif
    } else {
#if UMAC_SUPPORT_WEXT
        sock_fd = sock_ctx->sock_fd;
        (void) memset(&iwr, 0, sizeof(iwr));

        if (strlcpy(iwr.ifr_name, ifname, sizeof(iwr.ifr_name)) >= sizeof(iwr.ifr_name)) {
            fprintf(stderr, "ifname too long: %s\n", ifname);
            return -1;
        }

        iwr.u.data.pointer = buf;
        iwr.u.data.length = buflen;
        err = ioctl(sock_fd, ioctl_cmd, &iwr);
        if (err < 0) {
            errx(1, "unable to send command");
            return -1;
        }

        return iwr.u.data.length;
#endif
    }

    return 0;
}

/*
 * init_socket_context: initialize the context
 * @sock_ctx: socket context
 * @cmd_sock_id, @event_sock_id: If application can run as background
 *                               process/daemon then use unique port numbers
 *                               otherwise default socket id for simple applications.
 * return 0 on success otherwise negative value on failure
 */
int init_socket_context (struct socket_context *sock_ctx,
        int cmd_sock_id, int event_sock_id)
{
    int err = 0;
#if UMAC_SUPPORT_CFG80211
    if (sock_ctx->cfg80211) {
        sock_ctx->cfg80211_ctxt.pvt_cmd_sock_id = cmd_sock_id;
        sock_ctx->cfg80211_ctxt.pvt_event_sock_id = event_sock_id;

        err = wifi_init_nl80211(&(sock_ctx->cfg80211_ctxt));
        if (err) {
            errx(1, "unable to create NL socket");
            return -EIO;
        }
    } else
#endif
    {
#if UMAC_SUPPORT_WEXT
        sock_ctx->cfg80211 = 0 /*false*/;
        sock_ctx->sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock_ctx->sock_fd < 0) {
            errx(1, "socket creation failed");
            return -EIO;
        }
#endif
    }
    return 0;
}

/**
 * destroy_socket_context: destroys the context
 * @sock_ctx: socket context
 * returns 0 if success; otherwise negative values on failures
 */
void destroy_socket_context (struct socket_context *sock_ctx)
{
#if UMAC_SUPPORT_CFG80211
    if (sock_ctx->cfg80211) {
        wifi_destroy_nl80211(&(sock_ctx->cfg80211_ctxt));
    } else
#endif
    {
#if UMAC_SUPPORT_WEXT
        close(sock_ctx->sock_fd);
#endif
    }
    return;
}
