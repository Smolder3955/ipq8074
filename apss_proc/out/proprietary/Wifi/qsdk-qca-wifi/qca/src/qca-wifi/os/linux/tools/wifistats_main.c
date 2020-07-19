/*
* Copyright (c) 2017-2018 Qualcomm Innovation Center, Inc.
*
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/
#include <wifistats.h>
#include <netlink/attr.h>

char interface[IFNAMSIZ];
uint32_t cookie;
uint8_t responses_done = LISTEN_CONTINUE;

struct wifistats_module *modulelist = NULL;
struct wifistats_module *current_module = NULL;

static void wifistats_event_callback(char *ifname, uint32_t cmdid, uint8_t *data,
                                    size_t len)
{
    int response_cookie = 0;

    struct nlattr *tb_array[QCA_WLAN_VENDOR_ATTR_CONFIG_MAX + 1];
    struct nlattr *tb;
    void *buffer = NULL;

    if (cmdid != QCA_NL80211_VENDOR_SUBCMD_HTT_STATS) {
       /* ignore anyother events*/
       return;
    }

    if (strncmp(interface, ifname, sizeof(interface)) != 0) {
       /* ignore events for other interfaces*/
       return;
    }

    if (nla_parse(tb_array, QCA_WLAN_VENDOR_ATTR_CONFIG_MAX,
                (struct nlattr *) data, len, NULL)) {
        printf("INVALID EVENT\n");
        return;
    }

    tb = tb_array[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_DATA];
    if (tb) {
        buffer = (void *)nla_data(tb);
    }

    if (buffer == NULL) {
        printf("ERROR!! wifistats received with NULL data\n");
        return;
    }

    response_cookie  = current_module->output_cookie_get(buffer, len); /*Cookie LSB*/
    if (response_cookie != cookie) {
       /* ignore events if there is cookie mismatch*/
       return;
    }

    responses_done = current_module->output_handler(buffer, len);
}

/**
 * wifi_send_command; function to send the cfg command or ioctl command.
 * @ifname :interface name
 * @buf: buffer
 * @buflen : buffer length
 * @cmd : command type
 * @ioctl_cmd: ioctl command type
 * return : 0 for success
 */
static int wifi_send_command (wifi_cfg80211_context *cfg80211_ctxt, const char *ifname, void *buf, size_t buflen, int cmd)
{
    int msg;
    struct cfg80211_data buffer;

        buffer.data = buf;
        buffer.length = buflen;
        buffer.callback = NULL;
        msg = wifi_cfg80211_send_generic_command(cfg80211_ctxt,
                QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_CONFIGURATION,
                cmd, ifname, (char *)&buffer, buflen);
        if (msg < 0) {
            fprintf(stderr, "Couldn't send NL command\n");
            return msg;
        }
        return buffer.flags;
}


static int chartohex(char c)
{
    int val = -1;
    if (c >= '0' && c <= '9')
        val = c - '0';
    else if (c >= 'a' && c <= 'f')
        val = c - 'a' + 10;
    else if (c >= 'A' && c <= 'F')
        val = c - 'A' + 10;

    return val;
}


int  extract_mac_addr(uint8_t *addr, const char *text)
{
    uint8_t i=0;
    int nibble;
    const char *temp = text;
    while (temp && i < IEEE80211_ADDR_LEN) {
        nibble = chartohex(*temp++);
        if (nibble == -1)
            return -1;
        addr[i] = nibble << 4;
        nibble = chartohex(*temp++);
        if (nibble == -1)
            return -1;
        addr[i++] += nibble;
        if (*temp == ':')
            temp++;
    }
    return 0;
}

void wifistats_usage (int argc, char *argv[]) {
    struct wifistats_module * temp = modulelist;

    printf("========= GENERIC USAGE =================\n");

    while (temp != NULL) {
        printf("\t%s %s <ifname> <private args>\n", argv[0], temp->name);
        temp = temp->next;
    }
    printf("=========================================\n");
}

struct wifistats_module *wifistats_module_get (char *modulename) {
    struct wifistats_module * temp = modulelist;

    while (temp != NULL) {
        if (strcmp(modulename, temp->name) == 0) {
            return temp;
        }
        temp = temp->next;
    }

    return NULL;
}

int wifistats_module_unregister (struct wifistats_module *module, int size)
{
    struct wifistats_module *prev = NULL, *temp = modulelist;

    if ((temp != NULL) && (strcmp(module->name, temp->name) == 0)) {
        modulelist = temp->next;
        return 0;
    }

    while ((temp != NULL) && (strcmp(module->name, temp->name) == 0)) {
        prev = temp;
        temp = temp->next;
    }

    if (temp == NULL)
         return -1;

    if (prev)
        prev->next = temp->next;

    return 0;
}

int wifistats_module_register (struct wifistats_module *module, int size)
{
    if(size != sizeof(struct wifistats_module)) {
        fprintf(stderr, "###### ERROR structure size mismatch######\n");
        exit(-1);
    }
    module->next = modulelist;
    modulelist = module;
    return 0;
}

int
main(int argc, char *argv[])
{
    int err = 0;
    wifi_cfg80211_context cfg80211_ctxt;
    char *ifname;
    int num_msecs = 0;
    void *req_buff = NULL;
    int req_buff_sz = 0;
    int status = 0;

    if(argc < 2) {
        fprintf(stderr, "Invalid commands args\n");
        wifistats_usage(argc, argv);
        return -EIO;
    }
    current_module = wifistats_module_get ("htt_fw_stats");
    if (current_module == NULL) {
        fprintf (stderr, "%s is not registered for wifistats\n", argv[1]);
        wifistats_usage(argc, argv);
        return -EIO;
    }

    /* Reset the cfg80211 context to 0 if the application does not pass
     * custom private event and command sockets. In this case, the default
     * port is used for netlink communication.
     */
    memset(&cfg80211_ctxt, 0, sizeof(wifi_cfg80211_context));

    req_buff = current_module->input_buff_alloc(&req_buff_sz);
    if (req_buff == NULL) {
        fprintf (stderr, "%s unable to alloc memory for IO\n", current_module->name);
        return -EIO;
    }

    if (current_module->input_parse(req_buff, argc, argv)) {
        fprintf(stderr, "Invalid commands args \n");
        current_module->help(argc, argv);
        return -EIO;
    }

    ifname = argv[1];
    memcpy(interface, ifname, sizeof(interface));
    cookie = current_module->input_cookie_generate();
    cfg80211_ctxt.event_callback = wifistats_event_callback;

    err = wifi_init_nl80211(&cfg80211_ctxt);
    if (err) {
        fprintf(stderr, "unable to create NL socket\n");
        return -EIO;
    }

    wifi_send_command(&cfg80211_ctxt, ifname, req_buff, req_buff_sz, QCA_NL80211_VENDOR_SUBCMD_HTTSTATS);

    /* Starting event thread to listen for responses*/
    if (wifi_nl80211_start_event_thread(&cfg80211_ctxt)) {
        fprintf(stderr, "Unable to setup nl80211 event thread\n");
        status = -EIO;
        goto cleanup;
    }

    while ((responses_done != LISTEN_DONE) && (num_msecs < current_module->timeout)) {
        /*sleep for 1 ms*/
        usleep (1000);
        num_msecs++;
    }

    if (num_msecs >= current_module->timeout) {
        status = -EBUSY;
        goto cleanup;
    }

    current_module->input_buff_free(req_buff);
    return 0;

cleanup:
    if (req_buff) {
        if (current_module) {
            current_module->input_buff_free(req_buff);
        }
    }
    wifi_destroy_nl80211(&cfg80211_ctxt);
    return status;
}
