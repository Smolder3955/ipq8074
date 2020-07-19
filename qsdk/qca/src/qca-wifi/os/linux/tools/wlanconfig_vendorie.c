/*
 * Copyright (c) 2018 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 */

#include <wlanconfig.h>

#if ATH_SUPPORT_DYNAMIC_VENDOR_IE

#define ARG_COUNT_VENDOR_IE_ADD_OR_UPDATE 12    /* check wlanconfig help */
#define ARG_COUNT_VENDOR_IE_REMOVE 10    /* check wlanconfig help */
#define ARG_COUNT_VENDOR_IE_LIST 8    /* check wlanconfig help */
#define ARG_COUNT_VENDOR_IE_LIST_ALL 4      /* check wlanconfig help */
#define ARG_COUNT_ADD_IE 9    /* check wlanconfig help */

/* convert char array to hex */
static int char2hex(char *charstr)
{
    int i ;
    int hex_len, len = strlen(charstr);

    for(i=0; i<len; i++) {
        /* check 0~9, A~F */
        charstr[i] = ((charstr[i]-48) < 10) ? (charstr[i] - 48) : (charstr[i] - 55);
        /* check a~f */
        if ( charstr[i] >= 42 )
            charstr[i] -= 32;
        if ( charstr[i] > 0xf )
            return -1;
    }

    /* hex len is half the string len */
    hex_len = len /2 ;
    if (hex_len *2 < len)
        hex_len ++;

    for(i=0; i<hex_len; i++)
        charstr[i] = (charstr[(i<<1)] << 4) + charstr[(i<<1)+1];

    return 0;
}

static int handle_vendorie(struct socket_context *sock_ctx, const char *ifname,
    IEEE80211_WLANCONFIG_CMDTYPE cmdtype, int len, char *in_oui,
    char *in_cap_info, char *in_ftype_map)
{
    u_int8_t ie_buf[MAX_VENDOR_BUF_LEN + 12];
    struct ieee80211_wlanconfig_vendorie *vie = (struct ieee80211_wlanconfig_vendorie *) &ie_buf;
    /* From input every char occupies 1 byte and  +1 to include the null string in end */
    char oui[VENDORIE_OUI_LEN *2 + 1], *cap_info = NULL, ftype_map[3];
    int i, j = 0, k;
    u_int8_t *ie_buffer;
    u_int32_t ie_len;
    int block_length = 0;

    /* Param length check & conversion */

    if (cmdtype == IEEE80211_WLANCONFIG_VENDOR_IE_ADD || cmdtype == IEEE80211_WLANCONFIG_VENDOR_IE_UPDATE) {

        if (len >= MAX_VENDOR_IE_LEN || (len *2 != (strlen(in_oui) + strlen(in_cap_info))) || (len < 0)) {
            printf(" Len does not Match , either Invalid/exceeded max/min Vendor IE len \n");
            return -1;
        }

        if (strlen(in_oui) != (VENDORIE_OUI_LEN * 2)) {
            printf("Invalid OUI , OUI expected always 3 byte, format: xxxxxx)\n");
            return -1;
        }

        strlcpy(oui, in_oui, ((VENDORIE_OUI_LEN * 2) + 1));


        if (char2hex(oui) != 0) {
            printf("Invalid OUI Hex String , Correct Len & OUI field \n");
            return -1;
        }

        if (strlen(in_cap_info) != ((len - VENDORIE_OUI_LEN) * 2)) {
            printf("Invalid len , capability len =%d  not matching with total len=%d (bytes) , format: xxxxxxxx)\n", strlen(in_cap_info),len);
            return -1;
        }

        cap_info = malloc((len - VENDORIE_OUI_LEN) *2 + 1);

        if(!cap_info) {
            fprintf (stderr, "Unable to allocate memory for Vendor IE Cap_info \n");
            return -1;
        }
        strlcpy(cap_info, in_cap_info, ((len - VENDORIE_OUI_LEN) * 2 + 1));

        if (char2hex(cap_info) != 0) {
            printf("Invalid capability Hex String , Correct Len & Cap_Info field \n");
            return -1;
        }

        if (strlen(in_ftype_map) != 2) {
            printf("Invalid frame mapping , expected always 2 byte, format: xxxx)\n");
            return -1;
        }

        strlcpy(ftype_map, in_ftype_map, (2 + 1));

        if (char2hex(ftype_map) != 0) {
            printf("Invalid frame mapping Hex String \n");
            return -1;
        }
    }

    if (cmdtype == IEEE80211_WLANCONFIG_VENDOR_IE_LIST || cmdtype == IEEE80211_WLANCONFIG_VENDOR_IE_REMOVE) {
        if (len != 4 && len != 3 && len != 0){
            printf(" Invalid length ...Expected length is 3 or 0 for list command and 4 for remove command\n");
            return -1;
        }
        if(len != 0)
        {
            if (strlen(in_oui) != VENDORIE_OUI_LEN*2) {
                printf("Invalid OUI , OUI expected always 3 byte, format: xxxxxx)\n");
                return -1;
            }
            strlcpy(oui, in_oui, ((VENDORIE_OUI_LEN * 2) + 1));
            if (char2hex(oui) != 0) {
                printf("Invalid OUI Hex String , Correct Len & OUI field \n");
                return -1;
            }
        }
    }

    if(cmdtype == IEEE80211_WLANCONFIG_VENDOR_IE_REMOVE)
    {
        if (len != 4) {
            printf(" Invalid length ...Expected length is 4\n");
            return -1;
        }
        if (strlen(in_cap_info) != (len - VENDORIE_OUI_LEN) *2) {
            printf("Invalid len , capability len =%d  not matching with total len=%d (bytes) , format: xxxxxxxx)\n", strlen(in_cap_info),len);
            return -1;
        }

        cap_info = malloc((len - VENDORIE_OUI_LEN) *2 + 1);

        if(!cap_info) {
            fprintf (stderr, "Unable to allocate memory for Vendor IE Cap_info \n");
            return -1;
        }
        strlcpy(cap_info, in_cap_info, (((len - VENDORIE_OUI_LEN) * 2) + 1));

        if (char2hex(cap_info) != 0) {
            printf("Invalid capability Hex String , Correct Len & Cap_Info field \n");
            return -1;
        }
    }

    /* fill up configuration */
    memset(ie_buf, 0, MAX_VENDOR_BUF_LEN + 12);
    vie->cmdtype = cmdtype;

    switch (cmdtype) {
        case IEEE80211_WLANCONFIG_VENDOR_IE_ADD:
        case IEEE80211_WLANCONFIG_VENDOR_IE_UPDATE:
            vie->ie.id = IEEE80211_ELEMID_VENDOR; /*default vendor ie value */
            memcpy(vie->ie.oui, oui,VENDORIE_OUI_LEN);
            memcpy(vie->ie.cap_info, cap_info, (len - VENDORIE_OUI_LEN));
            memcpy(&vie->ftype_map, ftype_map, 1);
            vie->ie.len = len;
            vie->tot_len = len + 12;
            break;

        case IEEE80211_WLANCONFIG_VENDOR_IE_REMOVE:
            vie->ie.id = IEEE80211_ELEMID_VENDOR;
            memcpy(vie->ie.oui, oui,VENDORIE_OUI_LEN);
            memcpy(vie->ie.cap_info, cap_info, (len - VENDORIE_OUI_LEN));
            vie->ie.len = len;
            vie->tot_len = len + 12;
            break;
        case IEEE80211_WLANCONFIG_VENDOR_IE_LIST:
            if(len == 0)
            {
                vie->ie.id = 0x0;
                vie->ie.len = 0;
                vie->tot_len = MAX_VENDOR_BUF_LEN + 12;
                break;
            }
            vie->ie.id = IEEE80211_ELEMID_VENDOR;
            memcpy(vie->ie.oui, oui,VENDORIE_OUI_LEN);
            vie->ie.len = len;
            vie->tot_len = MAX_VENDOR_BUF_LEN + 12;
            break;
        default:
            printf("%d command not handled yet", cmdtype);
            break;
    }
    printf("\n sizeof vendorie: %d\n", sizeof(struct ieee80211_wlanconfig_vendorie));
    ie_len = send_command(sock_ctx, ifname, &ie_buf, vie->tot_len,
            NULL, QCA_NL80211_VENDOR_SUBCMD_VENDORIE, IEEE80211_IOCTL_CONFIG_GENERIC);

    if (cmdtype == IEEE80211_WLANCONFIG_VENDOR_IE_REMOVE) {
        printf(" Vendor IE Successfully Removed \n");
    }

    /*output configuration*/
    if(cmdtype == IEEE80211_WLANCONFIG_VENDOR_IE_ADD || cmdtype == IEEE80211_WLANCONFIG_VENDOR_IE_UPDATE)
    {
        printf("\n----------------------------------------\n");
        printf("Adding or updating following vendor ie\n");
        printf("Vendor IE info Ioctl CMD id     : %d \n",cmdtype);
        printf("ID                              : %2x\n", vie->ie.id);
        printf("Len (OUI+ Pcapdata) in Bytes    : %d\n", vie->ie.len);
        printf("OUI                             : %02x%02x%02x\n", vie->ie.oui[0],vie->ie.oui[1],vie->ie.oui[2]);
        printf("Private capibility_data         : ");
        for (i = 0; i < (vie->ie.len - VENDORIE_OUI_LEN); i++)
        {
            printf("%02x", vie->ie.cap_info[i]);
        }
        if (cmdtype == IEEE80211_WLANCONFIG_VENDOR_IE_ADD)
        {
            printf("\nFrame Include Mask              : %02x", vie->ftype_map);
        }
        printf("\n----------------------------------------\n");
    }
    if (cmdtype == IEEE80211_WLANCONFIG_VENDOR_IE_REMOVE)
    {
        printf("\n----------------------------------------\n");
        printf("Removing following vendor IEs matching OUI type and subtype\n");
        printf("Vendor IE info Ioctl CMD id     : %d \n",cmdtype);
        printf("ID                              : %02x\n", vie->ie.id);
        printf("OUI                             : %02x%02x%02x\n", vie->ie.oui[0],vie->ie.oui[1],vie->ie.oui[2]);
        if(vie->ie.cap_info[0] == 0xff)
        {
            printf("Subtype                         : all subtypes");
        }
        else
        {
            printf("Subtype                         : %02x",vie->ie.cap_info[0]);
        }
        printf("\n----------------------------------------\n");
    }
    /*
     * The list command returns a buffer containing multiple IEs.
     * The frame format of the buffer is as follows:
     * |ftype|type|length|OUI|pcap_data|
     * The buffer is traversed and the individual IEs are printed separately
     */
    if (cmdtype == IEEE80211_WLANCONFIG_VENDOR_IE_LIST)
    {
        printf("\n----------------------------------------\n");
        printf("Listing vendor IEs matching the following OUI type\n");
        printf("Vendor IE info Ioctl CMD id     : %d \n",cmdtype);
        ie_buffer = (u_int8_t *) &vie->ie;
        ie_len = ie_len - 12;
        printf("Total length = %d ",ie_len);
        if((ie_len <= 0) || (ie_len >= MAX_VENDOR_BUF_LEN))
        {
            goto exit;
        }
        j = 0;
        /* Buffer format:
         * +-----------------------------------------------+
         |F|T|L|  V of L bytes  |F|T|L| V of L bytes     |
         +-----------------------------------------------+
         */
        while((j+2 < ie_len) && (j+ie_buffer[j+2]+2 < ie_len))
        {
            block_length = ie_buffer[j+2] + 3;
            printf("\n\nFrame type                      : %2x\n", ie_buffer[j]);
            printf("ID                              : %2x\n", ie_buffer[j+1]);
            printf("Length                          : %d\n", ie_buffer[j+2]);
            printf("OUI                             : %02x%02x%02x\n", ie_buffer[j+3],ie_buffer[j+4],ie_buffer[j+5]);
            printf("Private capibility_data         : ");
            for(k = j+6; k < (j+block_length); k++)
            {
                printf("%02x ", ie_buffer[k]);
            }
            j += block_length;
        }
        printf("\n----------------------------------------\n");
    }
exit:
    free(cap_info);
    return 0;
}

int handle_command_vendorie (int argc, char *argv[], const char *ifname,
    struct socket_context *sock_ctx)
{
    if (argc == ARG_COUNT_VENDOR_IE_ADD_OR_UPDATE && streq(argv[3], "add")) {
        if (streq(argv[4], "len") && streq(argv[6], "oui") && streq(argv[8], "pcap_data") && streq(argv[10], "ftype_map")) {
            return handle_vendorie (sock_ctx, ifname, IEEE80211_WLANCONFIG_VENDOR_IE_ADD,
                    atoi(argv[5]), argv[7], argv[9], argv[11]);
        } else {
            errx(1, "invalid vendorie command , check wlanconfig help");
        }
    } else if (argc == ARG_COUNT_VENDOR_IE_ADD_OR_UPDATE && streq(argv[3], "update")) {
        if (streq(argv[4], "len") && streq(argv[6], "oui") && streq(argv[8], "pcap_data") && streq(argv[10], "ftype_map")) {
            return handle_vendorie (sock_ctx, ifname, IEEE80211_WLANCONFIG_VENDOR_IE_UPDATE,
                    atoi(argv[5]), argv[7], argv[9], argv[11]);
        } else {
            errx(1, "invalid vendorie command , check wlanconfig help");
        }
    } else if (argc == ARG_COUNT_VENDOR_IE_REMOVE && streq(argv[3], "remove")) {
        if (streq(argv[4], "len") && streq(argv[6], "oui") && streq(argv[8], "pcap_data")){
            return handle_vendorie (sock_ctx, ifname, IEEE80211_WLANCONFIG_VENDOR_IE_REMOVE,
                    atoi(argv[5]), argv[7], argv[9], NULL);
        } else {
            errx(1, "invalid vendorie command , check wlanconfig help");
        }
    } else if (argc == ARG_COUNT_VENDOR_IE_LIST && streq(argv[3], "list")) {
        if (streq(argv[4], "len") && streq(argv[6], "oui")){
            return handle_vendorie (sock_ctx, ifname, IEEE80211_WLANCONFIG_VENDOR_IE_LIST,
                    atoi(argv[5]), argv[7], NULL, NULL);
        }
        else {
            errx(1, "invalid vendorie command , check wlanconfig help");
        }
    } else if (argc == ARG_COUNT_VENDOR_IE_LIST_ALL && streq(argv[3], "list")) {
        return handle_vendorie (sock_ctx, ifname, IEEE80211_WLANCONFIG_VENDOR_IE_LIST,
                0 , NULL, NULL, NULL);
    }
    else {
        errx(1, "Invalid vendorie command , check wlanconfig help");
    }
    return 0;
}

static int handle_addie(struct socket_context *sock_ctx, const char *ifname,
    IEEE80211_WLANCONFIG_CMDTYPE cmdtype, char *ftype, int tot_len, char *data)
{
    u_int8_t ie_buf[MAX_VENDOR_BUF_LEN];
    struct ieee80211_wlanconfig_ie *ie_buffer = (struct ieee80211_wlanconfig_ie *) &ie_buf;
    /* From input every char occupies 1 byte and  +1 to include the null string in end */
    char *ie_data = NULL;

    memset(ie_buf, 0, MAX_VENDOR_BUF_LEN);
    ie_data = malloc(tot_len * 2 + 1);
    if (!ie_data) {
        fprintf(stderr, "Unable to allocate memory for App IE \n");
        return -1;
    }
    strlcpy(ie_data, data, (tot_len * 2) + 1);

    if (char2hex(ie_data) != 0) {
        printf("Invalid capability Hex String , Corret Len & Cap_Info field \n");
        return -1;
    }
    ie_buffer->ie.elem_id = ie_data[0];
    ie_buffer->ie.len = ie_data[1];
    ie_buffer->ftype = atoi(ftype);
    ie_buffer->cmdtype = cmdtype;

    if (!tot_len || (ie_buffer->ie.len != (tot_len - 2))) {
        printf("Input Validation Failed. Length mismatch : tot_len : %d ie->len : %d \n", tot_len, ie_buffer->ie.len);
        return -1;
    }
    memcpy(ie_buffer->ie.app_buf, &ie_data[2], ie_buffer->ie.len);

    /* fill up request */
    if (send_command(sock_ctx, ifname, &ie_buf, (sizeof(struct ieee80211_wlanconfig_ie) + strlen(ie_data)),
            NULL, QCA_NL80211_VENDOR_SUBCMD_ADDIE, IEEE80211_IOCTL_CONFIG_GENERIC) < 0) {
        perror("config_generic failed in handle_addie()");
        return -1;
    }

    free(ie_data);
    return 0;
}

int handle_command_addie(int argc, char *argv[], const char *ifname,
    struct socket_context *sock_ctx)
{
    if (argc == ARG_COUNT_ADD_IE) {
        if (streq(argv[3], "ftype") && streq(argv[5], "len") && streq(argv[7], "data")) {
            return handle_addie(sock_ctx, ifname, IEEE80211_WLANCONFIG_ADD_IE, argv[4], atoi(argv[6]), argv[8]);
        } else {
            errx(1, "invalid addie command , check wlanconfig help");
        }
    } else {
        errx(1, "invalid addie command , check wlanconfig help");
    }

    return 0;
}
#endif /* ATH_SUPPORT_DYNAMIC_VENDOR_IE */
