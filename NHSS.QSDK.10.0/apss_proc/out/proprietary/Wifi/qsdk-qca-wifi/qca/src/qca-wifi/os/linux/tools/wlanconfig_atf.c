/*
 * Copyright (c) 2018 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 */

#if QCA_AIRTIME_FAIRNESS

#include <wlanconfig.h>
#define ATF_STA_NUM            50
#define ATF_VAP_NUM            16
#define ATF_CFG_BUF_SIZE       6000
#define ATF_VAP_NUM            16
#define AC_BE                  0    /* best effort */
#define AC_BK                  1    /* background */
#define AC_VI                  2    /* video */
#define AC_VO                  3    /* voice */

#define AC_STR_BE   "BE"
#define AC_STR_BK   "BK"
#define AC_STR_VI   "VI"
#define AC_STR_VO   "VO"

#define AC_STR_LEN  2

struct addssid_val{
    uint16_t    id_type;
    uint8_t     ssid[IEEE80211_NWID_LEN+1];
    uint32_t    value;
    uint8_t    ssid_exist;
};

struct addsta_val{
    uint16_t    id_type;
    uint8_t     sta_mac[IEEE80211_ADDR_LEN];
    uint8_t     ssid[IEEE80211_NWID_LEN + 1];
    uint32_t    value;
};

struct atfac_val{
    uint16_t  id_type;
    int8_t ac_id;
    int32_t ac_val;
    uint8_t ac_ssid[IEEE80211_NWID_LEN+1];
};

struct addgroup_val{
    uint16_t    id_type;
    u_int8_t    name[32];
    uint8_t     ssid[IEEE80211_NWID_LEN+1];
    uint32_t    value;
};

static u_int8_t atf_buf[ATF_CFG_BUF_SIZE];
u_int8_t atf_ssid[IEEE80211_NWID_LEN+1];
u_int32_t atf_ssid_length = 0;

static void print_atf_table(struct atftable *at)
{
#define OTHER_SSID "Others   \0"
    int i, ret = 0;
    uint8_t *sta_mac;
    int quotient_val = 0 ,remainder_val = 0;
    int quotient_cfg = 0 ,remainder_cfg = 0;
    char ntoa[MAC_STRING_LENGTH + 1] = {0};
    int8_t ac_configured = 0;

    if(atf_ssid_length < 0){
        errx(1, "Unable to get_ssid");
        return;
    }
    if(at->info_cnt) {
        if(at->atf_group) {
            fprintf(stderr,"\n   GROUP            SSID/Client(MAC Address)         Air time(Percentage)        Config ATF(Percentage)      Assoc_Status(1-Assoc,0-No-Assoc)    All-token-used\n");
        } else {
            fprintf(stderr,"\n   SSID             Client(MAC Address)         Air time(Percentage)        Config ATF(Percentage)      Peer_Assoc_Status(1--Assoc,0-No-Assoc)    All-token-used\n");
        }

        for (i =0; i < at->info_cnt; i++) {
            quotient_val = at->atf_info[i].value/10;
            remainder_val = at->atf_info[i].value%10;
            quotient_cfg = at->atf_info[i].cfg_value/10;
            remainder_cfg = at->atf_info[i].cfg_value%10;

            if( ((!strncmp((char *) atf_ssid,(char *) at->atf_info[i].ssid, atf_ssid_length ) && (strlen((char *) at->atf_info[i].ssid) == atf_ssid_length)) || !strncmp("Others   ",(char *) at->atf_info[i].ssid, strlen(OTHER_SSID))) || at->atf_group) {
                if(at->atf_info[i].info_mark == 0) {
                    if(at->atf_group) {
                        fprintf(stderr,"   %s",at->atf_info[i].grpname);
                    } else {
                        fprintf(stderr,"   %s",at->atf_info[i].ssid);
                    }
                    fprintf(stderr,"                                            %d.%d",quotient_val,remainder_val);
                    if( at->atf_info[i].cfg_value !=0)
                        fprintf(stderr,"                           %d.%d\n",quotient_cfg,remainder_cfg);
                    else
                        fprintf(stderr,"\n");
                    if (at->atf_info[i].atf_ac_cfg) {
                        ac_configured = 1;
                    }
                } else {
                    sta_mac = &(at->atf_info[i].sta_mac[0]);
                    ret = ether_mac2string(ntoa, sta_mac);
                    if(at->atf_group) {
                        fprintf(stderr,"                   %s / %s",at->atf_info[i].ssid, (ntoa != NULL) ? ntoa:"WRONG MAC");
                    } else {
                        fprintf(stderr,"                     %s",(ret != -1) ? ntoa:"WRONG MAC");
                    }
                    fprintf(stderr,"                   %d.%d",quotient_val,remainder_val);
                    fprintf(stderr,"                      %d.%d",quotient_cfg,remainder_cfg);
                    fprintf(stderr,"                                    %d\n",at->atf_info[i].assoc_status);
                    fprintf(stderr,"   %d\n",at->atf_info[i].all_tokens_used);
                }

                fprintf(stderr,"\n\n");
            }
        }
        if(at->atf_status == 0) {
            fprintf(stderr,"\n   ATF IS DISABLED!!! The above ATF configuration will not have any effect.\n\n");
        }
    } else {
        fprintf(stderr,"   Air time table is empty\n");
    }
    fprintf(stderr,"ctl busy %d ext busy %d rf %d tf %d \n",
            (at->busy & 0xff), (at->busy & 0xff00) >> 8,
            (at->busy & 0xff0000) >> 16, (at->busy & 0xff000000) >> 24);

    if (ac_configured) {
        fprintf(stderr, "ATF AC configuration exists for this VAP.Unconfigured Client airtime shown here may not be valid\n");
        fprintf(stderr, "Check 'wlanconfig <vap> showatfsubgroup' output for per-AC & unconfigured client Airtime distribution \n");
    }

#undef OTHER_SSID
}

static void parse_ssid(struct cfg80211_data *buffer)
{
    memcpy(atf_ssid, buffer->data, buffer->length);
    atf_ssid_length = buffer->length;
}

static void get_atf_table(struct cfg80211_data *buffer)
{
    struct atftable *at;
    static uint32_t length = 0;

    memcpy(atf_buf + length, buffer->data, buffer->length);
    length += buffer->length;
    if (length >= sizeof(struct atftable)) {
        length = 0;
        at = (struct atftable *) atf_buf;
        print_atf_table(at);
        memset(atf_buf, 0, ATF_CFG_BUF_SIZE);
        atf_ssid_length = 0;
    }
}

static int get_ssid(struct socket_context *sock_ctx, const char *ifname)
{
    int len = 0;

    if(sock_ctx->cfg80211) {
        len = send_command(sock_ctx, ifname, atf_ssid, IEEE80211_NWID_LEN,
                parse_ssid, QCA_NL80211_VENDORSUBCMD_GET_SSID, 0);
    } else {
        len = send_command(sock_ctx, ifname, atf_ssid, IEEE80211_NWID_LEN,
                NULL, QCA_NL80211_VENDOR_SUBCMD_ATF, SIOCGIWESSID);
    }

    return len;
}

static void showatftable(struct socket_context *sock_ctx, const char *ifname, char *show_per_peer_table)
{
    struct atftable set_atp;
    struct atf_data atfdata;

    memset(atf_ssid, 0, IEEE80211_NWID_LEN+1);
    atf_ssid_length = 0;
    atf_ssid_length = get_ssid(sock_ctx, ifname); /*get current vap ssid */
    if(atf_ssid_length < 0){
        errx(1, "Unable to get_ssid");
        return;
    }
    atf_ssid[atf_ssid_length]='\0';

    memset(&set_atp, 0, sizeof(set_atp));
    set_atp.id_type = IEEE80211_IOCTL_ATF_SHOWATFTBL;
    if(show_per_peer_table){
        if(atoi(show_per_peer_table) != ATF_SHOW_PER_PEER_TABLE) {
            printf("Showatftable argument skipped to show per peer table\n");
        } else {
            set_atp.show_per_peer_table = atoi(show_per_peer_table);
        }
    }
    atfdata.id_type = IEEE80211_IOCTL_ATF_SHOWATFTBL;
    atfdata.buf = (uint8_t *)&set_atp;
    atfdata.len = sizeof(struct atftable);
    if(sock_ctx->cfg80211) {
        send_command(sock_ctx, ifname, &atfdata, sizeof(struct atf_data),
                &get_atf_table, QCA_NL80211_VENDOR_SUBCMD_ATF, 0);
    } else {
        send_command(sock_ctx, ifname, &set_atp, sizeof(set_atp),
                NULL, QCA_NL80211_VENDOR_SUBCMD_ATF, IEEE80211_IOCTL_CONFIG_GENERIC);
        print_atf_table(&set_atp);
        atf_ssid_length = 0;
    }
}

static void showairtime(struct socket_context *sock_ctx, const char *ifname)
{
    int i, ret = 0;
    uint8_t *sta_mac;
    struct atftable set_atp;
    struct atf_data atfdata;
    char ntoa[MAC_STRING_LENGTH + 1] = {0};

    (void) memset(&set_atp, 0, sizeof(set_atp));
    set_atp.id_type = IEEE80211_IOCTL_ATF_SHOWAIRTIME;
    atfdata.id_type = IEEE80211_IOCTL_ATF_SHOWAIRTIME;
    atfdata.buf = (uint8_t *)&set_atp;
    atfdata.len = sizeof(struct atftable);
    if(sock_ctx->cfg80211) {
        send_command(sock_ctx, ifname, &atfdata, sizeof(struct atf_data),
                NULL, QCA_NL80211_VENDOR_SUBCMD_ATF, IEEE80211_IOCTL_CONFIG_GENERIC);
    } else {
        send_command(sock_ctx, ifname, &set_atp, sizeof(struct atftable),
                NULL, QCA_NL80211_VENDOR_SUBCMD_ATF, IEEE80211_IOCTL_CONFIG_GENERIC);
    }

    if(set_atp.info_cnt)
    {
        fprintf(stderr,"\n         Client(MAC Address)         Air time(Percentage 1000) \n");
        for (i =0; i < set_atp.info_cnt; i++)
        {
            if(set_atp.atf_info[i].info_mark == 1)
            {
                sta_mac = &(set_atp.atf_info[i].sta_mac[0]);
                ret = ether_mac2string(ntoa, sta_mac);

                fprintf(stderr,"           %s",(ret != -1) ? ntoa:"WRONG MAC");
                fprintf(stderr,"                  %d \n",set_atp.atf_info[i].value);
            }
        }
        fprintf(stderr,"\n\n");
    }else{
        fprintf(stderr,"   Air time table is empty\n");
    }

}

static void flushatftable(struct socket_context *sock_ctx, const char *ifname)
{
    struct addssid_val set_atp;

    (void) memset(&set_atp, 0, sizeof(set_atp));
    set_atp.id_type = IEEE80211_IOCTL_ATF_FLUSHTABLE;
    send_command(sock_ctx, ifname, &set_atp, sizeof(set_atp),
            NULL, QCA_NL80211_VENDOR_SUBCMD_ATF, IEEE80211_IOCTL_CONFIG_GENERIC);
}

static void set_addssid_pval(struct socket_context *sock_ctx, const char *ifname, char *ssid, char *val)
{
    int cnt = 0;
    struct addssid_val  set_atp;
    cnt = strlen(val);

    (void) memset(&set_atp, 0, sizeof(set_atp));
    memcpy(&(set_atp.ssid[0]),ssid,strlen(ssid));
    set_atp.id_type = IEEE80211_IOCTL_ATF_ADDSSID;
    if(cnt >3 )
    {
        fprintf(stderr,"\n Input percentage value out of range between 0 and 100!!\n");
        return;
    }
    while(cnt-- != 0)
    {
        if((*val >= '0')&&(*val <= '9'))
        {
            set_atp.value = set_atp.value*10 + (*val - '0');
            val++;
        }
        else{
            fprintf(stderr, " Input wrong percentage value, its range is between 0 ~ 100\n");
            return;
        }
    }

    if(set_atp.value > 100)
    {
        fprintf(stderr,"Input percentage value is over 100!!");
        return;
    }

    set_atp.value = set_atp.value*10;
    send_command(sock_ctx, ifname, &set_atp, sizeof(set_atp),
            NULL, QCA_NL80211_VENDOR_SUBCMD_ATF, IEEE80211_IOCTL_CONFIG_GENERIC);
}

static void set_delssid(struct socket_context *sock_ctx, const char *ifname, char *ssid)
{
    struct addssid_val  set_atp;

    (void) memset(&set_atp, 0, sizeof(set_atp));
    memcpy(&(set_atp.ssid[0]),ssid,strlen(ssid));
    set_atp.id_type = IEEE80211_IOCTL_ATF_DELSSID;
    send_command(sock_ctx, ifname, &set_atp, sizeof(struct addssid_val),
            NULL, QCA_NL80211_VENDOR_SUBCMD_ATF, IEEE80211_IOCTL_CONFIG_GENERIC);
}

static void set_addsta_pval(struct socket_context *sock_ctx, const char *ifname,
        char *macaddr, char *val, char *ssid)
{
    int cnt = 0;
    struct addsta_val  set_sta;
    uint8_t i,len = 0;
    uint8_t lbyte = 0, ubyte = 0;
    cnt = strlen(val);
    (void) memset(&set_sta, 0, sizeof(set_sta));
    if (ssid)
        memcpy(&(set_sta.ssid[0]), ssid, strlen(ssid));
    len = strlen(macaddr);
    if((len != 2*IEEE80211_ADDR_LEN )||(cnt == 0))
    {
        err(1,"\n Unable to set ADD_STA success,failed on wrong MAC address length or format(example: 24aa450067fe)\n");
        return;
    }

    for (i = 0; i < len; i += 2) {
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

        set_sta.sta_mac[i/2] = (ubyte << 4) | lbyte;
    }

    if(cnt >3 )
    {
        err(1,"\n Input percentage value out of range between 0 and 100!!\n");
        return;
    }

    while(cnt-- != 0)
    {
        if((*val >= '0')&&(*val <= '9'))
        {
            set_sta.value = set_sta.value*10 + (*val - '0');
            val++;
        }
        else{
            err(1, "\n Input wrong percentage value, its range is between 0 ~ 100\n");
            return;
        }
    }

    if(set_sta.value > 100)
    {
        fprintf(stderr,"Input percentage value is over 100!!");
        return;
    }

    set_sta.value = set_sta.value * ATF_AIRTIME_CONVERSION_FACTOR;
    set_sta.id_type = IEEE80211_IOCTL_ATF_ADDSTA;
    send_command(sock_ctx, ifname, &set_sta, sizeof(set_sta),
            NULL, QCA_NL80211_VENDOR_SUBCMD_ATF, IEEE80211_IOCTL_CONFIG_GENERIC);
}

static void set_delsta(struct socket_context *sock_ctx, const char *ifname, char *macaddr)
{
    struct addsta_val  set_sta;
    uint8_t i,len = 0;
    uint8_t lbyte = 0, ubyte = 0;

    (void) memset(&set_sta, 0, sizeof(set_sta));
    len = strlen(macaddr);
    if(len != 2*IEEE80211_ADDR_LEN )
    {
        errx(1, "Unable to set DEL_STA success,failed on wrong MAC address length");
        return;
    }

    for (i = 0; i < len; i += 2) {
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

        set_sta.sta_mac[i/2] = (ubyte << 4) | lbyte;
    }

    set_sta.id_type = IEEE80211_IOCTL_ATF_DELSTA;
    send_command(sock_ctx, ifname, &set_sta, sizeof(struct addsta_val),
            NULL, QCA_NL80211_VENDOR_SUBCMD_ATF, IEEE80211_IOCTL_CONFIG_GENERIC);
}

static void set_addatfgroup (struct socket_context *sock_ctx,
        const char *ifname, char *groupname, char *ssid)
{
    struct addgroup_val set_group;

    (void)memset(&set_group, 0, sizeof(set_group) );
    memcpy( &set_group.name[0], groupname, strlen(groupname) );
    memcpy( &set_group.ssid[0], ssid, strlen(ssid) );
    set_group.id_type = IEEE80211_IOCTL_ATF_ADDGROUP;

    send_command(sock_ctx, ifname, &set_group, sizeof(struct addgroup_val),
            NULL, QCA_NL80211_VENDOR_SUBCMD_ATF, IEEE80211_IOCTL_CONFIG_GENERIC);
    return;
}

static void set_configatfgroup (struct socket_context *sock_ctx,
        const char *ifname, char *groupname, char *val)
{
    struct addgroup_val config_group;

    if(atoi(val) <= 0 || atoi(val) > 100) {
        errx(1, "Invalid Airtime input.");
        return;
    }

    (void) memset(&config_group, 0, sizeof(config_group));
    memcpy(&config_group.name[0], groupname, strlen(groupname));

    config_group.id_type = IEEE80211_IOCTL_ATF_CONFIGGROUP;
    config_group.value = atoi(val);

    config_group.value = config_group.value * 10;
    send_command(sock_ctx, ifname, &config_group, sizeof(struct addgroup_val),
            NULL, QCA_NL80211_VENDOR_SUBCMD_ATF, IEEE80211_IOCTL_CONFIG_GENERIC);
    return;
}

static void set_atfgroupsched(struct socket_context *sock_ctx, const char *ifname,
    char *groupname, char *val)
{
    struct addgroup_val group_sched;

    if(atoi(val) < 0 || atoi(val) > 2) {
        errx(1, "Invalid Scheduling policy");
        return;
    }

    memset(&group_sched, 0, sizeof(group_sched));
    memcpy(&group_sched.name[0], groupname, strlen(groupname));
    group_sched.id_type = IEEE80211_IOCTL_ATF_GROUPSCHED;
    group_sched.value = atoi(val);

    send_command(sock_ctx, ifname, &group_sched, sizeof(struct addgroup_val),
                 NULL, QCA_NL80211_VENDOR_SUBCMD_ATF, IEEE80211_IOCTL_CONFIG_GENERIC);
    return;
}

static void set_delatfgroup(struct socket_context *sock_ctx,
        const char *ifname, char *groupname)
{
    struct addgroup_val del_group;

    (void) memset(&del_group, 0, sizeof(del_group));

    memcpy(&del_group.name[0], groupname, strlen(groupname));
    del_group.id_type = IEEE80211_IOCTL_ATF_DELGROUP;

    send_command(sock_ctx, ifname, &del_group, sizeof(struct addgroup_val),
            NULL, QCA_NL80211_VENDOR_SUBCMD_ATF, IEEE80211_IOCTL_CONFIG_GENERIC);
    return;
}

static void showatfgroup(struct socket_context *sock_ctx, const char *ifname)
{
    int32_t i = 0, j=0;
    struct atfgrouptable list_group;
    struct atf_data atfdata;
    (void) memset(&list_group, 0, sizeof(list_group));
    list_group.id_type = IEEE80211_IOCTL_ATF_SHOWGROUP;
    atfdata.id_type = IEEE80211_IOCTL_ATF_SHOWGROUP;
    atfdata.buf = (uint8_t *)&list_group;
    atfdata.len = sizeof(struct atfgrouptable);
    if(sock_ctx->cfg80211) {
        send_command(sock_ctx, ifname, &atfdata, sizeof(struct atf_data),
                NULL, QCA_NL80211_VENDOR_SUBCMD_ATF, IEEE80211_IOCTL_CONFIG_GENERIC);
    } else {
        send_command(sock_ctx, ifname, &list_group, sizeof(struct atfgrouptable),
                NULL, QCA_NL80211_VENDOR_SUBCMD_ATF, IEEE80211_IOCTL_CONFIG_GENERIC);
    }
    if(list_group.info_cnt)
    {
        fprintf(stderr,"\n          Group           Airtime         SSID List    \n");
        for (i =0; i < list_group.info_cnt; i++)
        {
            fprintf(stderr,"          %s", list_group.atf_groups[i].grpname);
            fprintf(stderr,"            %d", list_group.atf_groups[i].grp_cfg_value);
            fprintf(stderr,"              %d", list_group.atf_groups[i].grp_sched);
            fprintf(stderr,"           ");
            for(j=0; j<list_group.atf_groups[i].grp_num_ssid; j++)
            {
                fprintf(stderr,"%s ", list_group.atf_groups[i].grp_ssid[j]);
            }
            fprintf(stderr,"\n");
        }
        fprintf(stderr,"\n\n");
    } else {
        fprintf(stderr,"   Air time table is empty\n");
    }

}

int convert_ac_str2int(char *ac_name)
{
    if (strlen(ac_name) != AC_STR_LEN) {
        printf(" %s Invalid Length : %d \n",__func__, strlen(ac_name));
        return -1;
    }
    if (strncmp(ac_name, AC_STR_BE, AC_STR_LEN) == 0) {
        return AC_BE;
    } else if (strncmp(ac_name, AC_STR_BK, AC_STR_LEN) == 0) {
        return AC_BK;
    } else if (strncmp(ac_name, AC_STR_VI, AC_STR_LEN) == 0) {
        return AC_VI;
    } else if (strncmp(ac_name, AC_STR_VO, AC_STR_LEN) == 0) {
        return AC_VO;
    }

    return -1;
}

int convert_ac_int2str(int8_t ac_id, char *ac_str)
{
    char ac[5][8] = {"BE", "BK", "VI", "VO", "UNKNOWN"};

    strlcpy(ac_str, ac[ac_id], sizeof(ac_str));

    return 0;
}

void get_quotient_remainder(u_int32_t val, u_int32_t *quotient, u_int32_t *remainder)
{
    *quotient = 0;
    *remainder = 0;

    *quotient = val / 10;
    *remainder = val % 10;
}

static void showatfsubgroup(struct socket_context *sock_ctx, const char *ifname)
{
    int32_t i = 0, j = 0, ret = 0;
    struct atfgrouplist_table list_group;
    uint8_t subgroup_type_str[3][5] = {"SSID", "PEER", "AC"};
    char ac_str[8];
    uint32_t quo_cfg = 0, rem_cfg = 0;
    uint32_t quo_allot = 0, rem_allot = 0;
    char ntoa[MAC_STRING_LENGTH + 1] = {0};
    struct atf_data atfdata;

    (void) memset(&list_group, 0, sizeof(list_group));
    list_group.id_type = IEEE80211_IOCTL_ATF_SHOWSUBGROUP;
    atfdata.id_type = IEEE80211_IOCTL_ATF_SHOWSUBGROUP;
    atfdata.buf = (uint8_t *)&list_group;
    atfdata.len = sizeof(struct atfgrouplist_table);

    if (sock_ctx->cfg80211) {
        send_command(sock_ctx, ifname, &atfdata, sizeof(struct atf_data),
                NULL, QCA_NL80211_VENDOR_SUBCMD_ATF, IEEE80211_IOCTL_CONFIG_GENERIC);
    } else {
        send_command(sock_ctx, ifname, &list_group, sizeof(struct atfgrouplist_table),
                NULL, QCA_NL80211_VENDOR_SUBCMD_ATF, IEEE80211_IOCTL_CONFIG_GENERIC);
    }
    if(list_group.info_cnt)
    {
        fprintf(stderr,"\n    Group                      Group Airtime        Subgroup                           Configured Airtime         Alloted Airtime        Subgroup Type  \n");
        for (i = 0; i < list_group.info_cnt; i++)
        {
           get_quotient_remainder(list_group.atf_list[i].grp_value, &quo_cfg, &rem_cfg);
           fprintf(stderr, "    %s      %d.%d\n",list_group.atf_list[i].grpname, quo_cfg, rem_cfg);
            for (j = 0; j < list_group.atf_list[i].num_subgroup; j++)
            {
                get_quotient_remainder(list_group.atf_list[i].sg_table[j].subgrp_cfg_value, &quo_cfg, &rem_cfg);
                get_quotient_remainder(list_group.atf_list[i].sg_table[j].subgrp_value, &quo_allot, &rem_allot);
                fprintf(stderr,"                                                    %s                            %d.%d                      %d.%d                  %s",
                        list_group.atf_list[i].sg_table[j].subgrpname, quo_cfg, rem_cfg, quo_allot, rem_allot, subgroup_type_str[list_group.atf_list[i].sg_table[j].subgrp_type]);
                    if (list_group.atf_list[i].sg_table[j].subgrp_type == 0x1) {
                        ret = ether_mac2string(ntoa, list_group.atf_list[i].sg_table[j].peermac);
                        fprintf(stderr,"           %s",(ret != -1) ? ntoa:"WRONG MAC");
                    } else if (list_group.atf_list[i].sg_table[j].subgrp_type == 0x2) {
                        convert_ac_int2str(list_group.atf_list[i].sg_table[j].subgrp_ac_id, ac_str);
                        fprintf(stderr," (%s)", ac_str);
                    }
                    fprintf(stderr,"\n");
            }
            fprintf(stderr,"\n");
        }
        fprintf(stderr,"\n\n");
    } else {
        fprintf(stderr,"   Air time table is empty\n");
    }
}

char *get_atf_ac_name_val(char *ac, char *ac_name)
{
    char *str;
    char *tmp = ac;

    str = strtok_r(tmp, ":", &tmp);
    while (str != NULL) {
        if (strlen(str) != AC_STR_LEN) {
            return NULL;
        }
        strlcpy(ac_name, str, 3);
        ac_name[AC_STR_LEN] = '\0';
        str = strtok_r(NULL, ":", &tmp);
        if (str != NULL) {
            str[strlen(str)] = '\0';
        } else {
            str = NULL;
        }
        return str;
    }

    return NULL;
}

int atf_fill_ac_val(char *ac, int *ac_id, int *ac_val)
{
    char ac_name[AC_STR_LEN + 1];
    char *ac_val_str;
    int ret = 0;

    ac_val_str = get_atf_ac_name_val(ac, ac_name);
    if (!ac_val_str) {
        printf("Error in fetching AC ID. Check command usage\n");
        ret = -1;
        goto end;
    }

    *ac_val = atoi(ac_val_str);
    *ac_id = convert_ac_str2int(ac_name);
    if (*ac_id < 0) {
        printf("Error in fetching AC ID. Check command usage\n");
        ret = -1;
    }

end:
    if (ret) {
        printf("usage: wlanconfig athX atfaddac <ssid/groupname> <ac_name>:<val> <ac_name>:<val> <ac_name>:<val> <ac_name>:<val>\n"
                        "                   ac_name: BE,BK,VI,VO\n"
                        "                   val: 0 - 100\n");
    }

    return ret;
}

int del_atf_ac(struct socket_context *sock_ctx, const char *ifname,
    char *ssid, int ac_id)
{
    struct atfac_val atfac_set;

    memset(&atfac_set, 0, sizeof(atfac_set));
    atfac_set.ac_id = ac_id;
    atfac_set.id_type = IEEE80211_IOCTL_ATF_DELAC;
    strlcpy((char *) atfac_set.ac_ssid, ssid, IEEE80211_NWID_LEN);

    send_command(sock_ctx, ifname, &atfac_set, sizeof(struct atfac_val),
            NULL, QCA_NL80211_VENDOR_SUBCMD_ATF, IEEE80211_IOCTL_CONFIG_GENERIC);

    return 0;
}

int configure_atf_ac(struct socket_context *sock_ctx, const char *ifname, char *ssid, int ac_id, int val)
{
    struct atfac_val atfac_set;

    memset(&atfac_set, 0, sizeof(atfac_set));
    atfac_set.ac_id = ac_id;
    atfac_set.ac_val = val * ATF_AIRTIME_CONVERSION_FACTOR;
    atfac_set.id_type = IEEE80211_IOCTL_ATF_ADDAC;
    strlcpy((char *) atfac_set.ac_ssid, ssid, IEEE80211_NWID_LEN);

    send_command(sock_ctx, ifname, &atfac_set, sizeof(struct atfac_val),
            NULL, QCA_NL80211_VENDOR_SUBCMD_ATF, IEEE80211_IOCTL_CONFIG_GENERIC);
        printf("AC %d on SSID : %s configured with %d airtime \n",
                ac_id, ssid, val);

    return 0;
}

static int atf_delac(struct socket_context *sock_ctx, const char *ifname,
    char *ssid, char *ac1, char *ac2, char *ac3, char *ac4)
{
    int ac_id = 0, ret = 0;

    if(!ac1 || !strcmp(ac1, "--cfg80211")) {
        ret = -1;
        goto fail;
    } else {
        ac_id = convert_ac_str2int(ac1);
        if (ac_id < 0) {
            ret = -1;
            goto fail;
        }
        if (del_atf_ac(sock_ctx, ifname, ssid, ac_id) < 0) {
            ret = -1;
            goto fail;
        }
    }

    if (!ac2 || !strcmp(ac2, "--cfg80211")) {
        ret = 0;
        goto fail;
    } else {
        ac_id = 0;
        ac_id = convert_ac_str2int(ac2);
        if (ac_id < 0) {
            ret = -1;
            goto fail;
        }
        if (del_atf_ac(sock_ctx, ifname, ssid, ac_id) < 0) {
            ret = -1;
            goto fail;
        }
    }

    if (!ac3 || !strcmp(ac3, "--cfg80211")) {
        ret = 0;
        goto fail;
    } else {
        ac_id = 0;
        ac_id = convert_ac_str2int(ac3);
        if (ac_id < 0) {
            ret = -1;
            goto fail;
        }
        if (del_atf_ac(sock_ctx, ifname, ssid, ac_id) < 0) {
            ret = -1;
            goto fail;
        }
    }

    if (!ac4 || !strcmp(ac4, "--cfg80211")) {
        ret = 0;
        goto fail;
    } else {
        ac_id = 0;
        ac_id = convert_ac_str2int(ac4);
        if (ac_id < 0) {
            return -1;
        }
        if (del_atf_ac(sock_ctx, ifname, ssid, ac_id) < 0) {
            ret = -1;
        }
    }

fail:
    return ret;
}

static int atf_addac(struct socket_context *sock_ctx, const char *ifname,
    char *ssid, char *ac1, char *ac2, char *ac3, char *ac4)
{
    int ac_id = 0;
    int val = 0;
    int ret = 0;

    if(!ac1 || !strcmp(ac1, "--cfg80211")) {
        ret = -1;
        goto fail;
    } else {
        if (atf_fill_ac_val(ac1, &ac_id, &val) < 0) {
            ret = -1;
            goto fail;
        }
        if (configure_atf_ac(sock_ctx, ifname, ssid, ac_id, val) < 0) {
            ret = -1;
            goto fail;
        }
    }

    if (!ac2 || !strcmp(ac2, "--cfg80211")) {
        ret = 0;
        goto fail;
    } else {
        ac_id = val = 0;
        if (atf_fill_ac_val(ac2, &ac_id, &val) < 0) {
            ret = -1;
            goto fail;
        }
        if (configure_atf_ac(sock_ctx, ifname, ssid, ac_id, val) < 0) {
            ret = -1;
            goto fail;
        }
    }

    if (!ac3 || !strcmp(ac3, "--cfg80211")) {
        ret = 0;
        goto fail;
    } else {
        ac_id = val = 0;
        if (atf_fill_ac_val(ac3, &ac_id, &val) < 0) {
            ret = -1;
            goto fail;
        }
        if (configure_atf_ac(sock_ctx, ifname, ssid, ac_id, val) < 0) {
            ret = -1;
            goto fail;
        }
    }

    if (!ac4 || !strcmp(ac4, "--cfg80211")) {
        ret = 0;
        goto fail;
    } else {
        ac_id = val = 0;
        if (atf_fill_ac_val(ac4, &ac_id, &val) < 0) {
            return -1;
        }
        if (configure_atf_ac(sock_ctx, ifname, ssid, ac_id, val) < 0) {
            ret = -1;
        }
    }

fail:
    return ret;
}

static int atf_addsta_tput(struct socket_context *sock_ctx,
        const char *ifname, char *macaddr, char *val, char *val2)
{
    int i;
    struct addsta_val set_sta;
    uint8_t len, cnt, cnt2;
    uint8_t lbyte = 0, ubyte = 0, non_zero, wild_card, value;

    cnt = strlen(val);
    cnt2 = val2 ? strlen(val2) : 0;
    len = strlen(macaddr);
    memset(&set_sta, 0, sizeof(set_sta));

    if ((len != 2 * IEEE80211_ADDR_LEN) || (cnt == 0)) {
        printf("Invalid mac address (eg:aabbcc112233) or invalid throughput\n");
        return 1;
    }

    non_zero = 0;
    for (i = 0; i < len; i += 2) {
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

        set_sta.sta_mac[i / 2] = (ubyte << 4) | lbyte;

        if (set_sta.sta_mac[i / 2])
            non_zero = 1;
    }

    if (!non_zero) {
        printf("Invalid mac address\n");
        return 1;
    }

    while (cnt--) {
        if ((*val >= '0') && (*val <= '9')) {
            set_sta.value = set_sta.value * 10 + (*val - '0');
            val++;
        } else {
            printf("Invalid char in throughput\n");
            return 1;
        }
    }

    value = 0;
    if (cnt2) {
        while (cnt2--) {
            if ((*val2 >= '0') && (*val2 <= '9')) {
                value = value * 10 + (*val2 - '0');
                val2++;
            } else {
                printf("Invalid char in airtime\n");
                return 1;
            }
        }
    }

    wild_card = 1;
    for (i = 0; i < IEEE80211_ADDR_LEN; i++) {
        if (set_sta.sta_mac[i] != 0xFF) {
            wild_card = 0;
            break;
        }
    }
    if (wild_card)
        set_sta.value = 1300000;

    if (!set_sta.value || set_sta.value > 1300000) {
        printf("Invalid throughput\n");
        return 1;
    }

    if (!value || value > 100) {
        value = 100;
    }

    set_sta.value &= ATF_TPUT_MASK;
    set_sta.value |= (value << ATF_AIRTIME_SHIFT) & ATF_AIRTIME_MASK;

    set_sta.id_type = IEEE80211_IOCTL_ATF_ADDSTA_TPUT;

    send_command(sock_ctx, ifname, &set_sta, sizeof(struct addsta_val),
            NULL, QCA_NL80211_VENDOR_SUBCMD_ATF, IEEE80211_IOCTL_CONFIG_GENERIC);
    return 0;
}

static int atf_delsta_tput(struct socket_context *sock_ctx,
        const char *ifname, char *macaddr)
{
    int32_t i;
    struct addsta_val set_sta;
    uint8_t len;
    uint8_t lbyte = 0, ubyte = 0, non_zero;

    len = strlen(macaddr);
    memset(&set_sta, 0, sizeof(set_sta));

    if (len != 2 * IEEE80211_ADDR_LEN) {
        printf("Invalid mac address (eg:aabbcc112233)\n");
        return 1;
    }

    non_zero = 0;
    for (i = 0; i < len; i += 2) {
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

        set_sta.sta_mac[i / 2] = (ubyte << 4) | lbyte;

        if (set_sta.sta_mac[i / 2])
            non_zero = 1;
    }

    if (!non_zero) {
        printf("Invalid mac address\n");
        return 1;
    }

    set_sta.id_type = IEEE80211_IOCTL_ATF_DELSTA_TPUT;
    send_command(sock_ctx, ifname, &set_sta, sizeof(struct addsta_val),
            NULL, QCA_NL80211_VENDOR_SUBCMD_ATF, IEEE80211_IOCTL_CONFIG_GENERIC);
    return 0;
}

static int atf_show_tput(struct socket_context *sock_ctx, const char *ifname)
{
    struct addsta_val set_sta;

    set_sta.id_type = IEEE80211_IOCTL_ATF_SHOW_TPUT;
    send_command(sock_ctx, ifname, &set_sta, sizeof(struct addsta_val),
            NULL, QCA_NL80211_VENDOR_SUBCMD_ATF, IEEE80211_IOCTL_CONFIG_GENERIC);
    return 0;
}

int handle_command_atf (int argc, char *argv[], const char *ifname,
        struct socket_context *sock_ctx)
{
    if (argc >= 2) {
        if (streq(argv[2], "addssid")) {
            if (argc >= 5){
                set_addssid_pval(sock_ctx, ifname, argv[3], argv[4]);
            } else {
                fprintf(stderr,"\n\n     Missing parameters!!  \n\n");
            }
        } else if (streq(argv[2], "delssid")) {
            if (argc >= 4) {
                set_delssid(sock_ctx, ifname, argv[3]);
            } else {
                fprintf(stderr,"\n\n     Missing parameters!!  \n\n");
            }
        } else if (streq(argv[2], "addsta")) {
            if (argc >= 6)
                set_addsta_pval(sock_ctx, ifname, argv[3], argv[4], argv[5]);
            else if (argc >= 5)
                set_addsta_pval(sock_ctx, ifname, argv[3], argv[4], NULL);
            else
                fprintf(stderr, "Missing Parameters %d\n", argc);
        } else if (streq(argv[2], "delsta")) {
            if (argc >= 4) {
                set_delsta(sock_ctx, ifname, argv[3]);
            } else {
                fprintf(stderr,"\n\n     Missing parameters!!  \n\n");
            }
        } else if (streq(argv[2], "showatftable")) {
            fprintf(stderr,"\n\n                      SHOW   ATF    TABLE  \n");
            if (argc >= 4)
                showatftable(sock_ctx, ifname, argv[3]);
            else
                showatftable(sock_ctx, ifname, NULL);
        } else if (streq(argv[2], "showairtime")) {
            fprintf(stderr,"\n\n                      SHOW   AIRTIME    TABLE  \n");
            showairtime(sock_ctx, ifname);
        } else if (streq(argv[2], "flushatftable")) {
            flushatftable(sock_ctx, ifname);
        } else if (streq(argv[2], "addatfgroup")) {
            if (argc >= 5) {
                set_addatfgroup(sock_ctx, ifname, argv[3], argv[4]);
            } else {
                fprintf(stderr,"\n\n     Missing parameters!!  \n\n");
            }
        } else if (streq(argv[2], "configatfgroup")){
            if (argc >= 5) {
                set_configatfgroup(sock_ctx, ifname, argv[3], argv[4]);
            } else {
                fprintf(stderr,"\n\n     Missing parameters!!  \n\n");
            }
        } else if (streq(argv[2], "atfgroupsched")) {
            if (argc >= 5) {
                set_atfgroupsched(sock_ctx, ifname, argv[3], argv[4]);
            } else {
                fprintf(stderr,"\n\n     Missing parameters!!  \n\n");
            }
        } else if (streq(argv[2], "delatfgroup")){
            if (argc >= 4) {
                set_delatfgroup(sock_ctx, ifname, argv[3]);
            } else {
                fprintf(stderr,"\n\n     Missing parameters!!  \n\n");
            }
        } else if (streq(argv[2], "showatfgroup")){
            fprintf(stderr,"\n\n                      SHOW   ATF    GROUP  \n");
            showatfgroup(sock_ctx, ifname);
        } else if (streq(argv[2], "addtputsta")) {
            if (argc >= 6)
                atf_addsta_tput(sock_ctx, ifname, argv[3], argv[4], argv[5]);
            else if (argc >= 5)
                atf_addsta_tput(sock_ctx, ifname, argv[3], argv[4], NULL);
            else
                fprintf(stderr, "Missing Parameters %d\n", argc);
        } else if (streq(argv[2], "deltputsta")) {
            if (argc >= 4)
                atf_delsta_tput(sock_ctx, ifname, argv[3]);
            else
                fprintf(stderr, "Missing Parameters %d\n", argc);
        } else if (streq(argv[2], "showtputtbl")) {
            atf_show_tput(sock_ctx, ifname);
        } else if (streq(argv[2], "atfaddac")) {
            if (argc >= 5) {
                atf_addac(sock_ctx, ifname, argv[3], argv[4], argv[5], argv[6], argv[7]);
            } else {
                fprintf(stderr,"Missing parameters!!  \n\n");
                usage();
            }
        } else if (streq(argv[2], "atfdelac")) {
            if (argc >= 5) {
                atf_delac(sock_ctx, ifname, argv[3], argv[4], argv[5], argv[6], argv[7]);
            } else {
                fprintf(stderr,"Missing parameters!!  \n\n");
                usage();
            }
        } else if (streq(argv[2], "showatfsubgroup")) {
            fprintf(stderr,"\n\n                      SHOW   ATF    GROUP  \n");
            showatfsubgroup(sock_ctx, ifname);
        }
    }
    return 0;
}
#endif
