/*
 * Copyright (c) 2018 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 */

#include <wlanconfig.h>

#if UMAC_SUPPORT_WNM

#define FMS_REQUEST_STR "fms_request {"
#define FMS_ELEMENT_STR "fms_subelement {"
#define TFS_REQUEST_STR "tfs_request {"
#define TCLAS_ELEMENT_STR "tclaselement {"
#define SUBELEMENT_STR "subelement {"
#define ACTION_STR "action_code {"

static char * config_get_line(char *s, int size, FILE *stream, char **_pos)
{
    char *pos, *end, *sstart;

    while (fgets(s, size, stream)) {
        s[size - 1] = '\0';
        pos = s;

        /* Skip white space from the beginning of line. */
        while (*pos == ' ' || *pos == '\t' || *pos == '\r')
            pos++;

        /* Skip comment lines and empty lines */
        if (*pos == '#' || *pos == '\n' || *pos == '\0')
            continue;

        /*
         * Remove # comments unless they are within a double quoted
         * string.
         */
        sstart = strchr(pos, '"');
        if (sstart)
            sstart = strrchr(sstart + 1, '"');
        if (!sstart)
            sstart = pos;
        end = strchr(sstart, '#');
        if (end)
            *end-- = '\0';
        else
            end = pos + strlen(pos) - 1;

        /* Remove trailing white space. */
        while (end > pos &&
                (*end == '\n' || *end == ' ' || *end == '\t' ||
                 *end == '\r'))
            *end-- = '\0';
        if (*pos == '\0')
            continue;

        if (_pos)
            *_pos = pos;
        return pos;
    }
    if (_pos)
        *_pos = NULL;
    return NULL;
}

static int config_get_param_value(char *buf, char *pos,
        char *param, int param_len, char *value, int value_len)
{
    char *pos2, *pos3;

    pos2 = strchr(pos, '=');
    if (pos2 == NULL) {
        return -1;
    }
    pos3 = pos2 - 1;
    while (*pos3 && ((*pos3 == ' ') || (*pos3 == '\t'))) {
        pos3--;
    }
    if (*pos3) {
        pos3[1] = 0;
    }
    pos2 ++;
    while ((*pos2 == ' ') || (*pos2 == '\t')) {
        pos2++;
    }
    if (*pos2 == '"') {
        if (strchr(pos2 + 1, '"') == NULL) {
            return -1;
        }
    }
    /* param_len is the sizeof(param) passed by caller func*/
    if (strlcpy(param, pos, param_len) >= param_len) {
        printf("Invalid Parameter");
        exit(-1);
    }
    /* value_len is the sizeof(value) passed by caller func*/
    if (strlcpy(value, pos2, value_len) >= value_len) {
        printf("Invalid Parameter");
        exit(-1);
    }
    return 0;
}

static int parse_tclas_element(FILE *fp, struct tfsreq_tclas_element *tclas)
{
    char buf[256] = {0};
    char *pos = NULL;
    char param[50] = {0};
    char value[50] = {0};
    int end=0;
    struct sockaddr_in ipv4;
    struct sockaddr_in6 ipv6;

    while(config_get_line(buf, sizeof(buf), fp, &pos)) {
        if (strcmp(pos, "}") == 0) {
            end = 1;
            break;
        }
        config_get_param_value(buf, pos, param, sizeof(param), value, sizeof(value));
        if (strcmp(param, "classifier_type") == 0) {
            tclas->classifier_type = atoi(value);
        }
        if (strcmp(param, "classifier_mask") == 0) {
            tclas->classifier_mask = atoi(value);
        }
        if (strcmp(param, "priority") == 0) {
            tclas->priority = atoi(value);
        }
        if (strcmp(param, "filter_offset") == 0) {
            tclas->clas.clas3.filter_offset = atoi(value);
        }
        if (strcmp(param, "filter_value") == 0) {
            int i;
            int len;
            u_int8_t lbyte = 0, ubyte = 0;

            len = strlen(value);
            for (i = 0; i < len; i += 2) {
                if ((value[i] >= '0') && (value[i] <= '9'))  {
                    ubyte = value[i] - '0';
                } else if ((value[i] >= 'A') && (value[i] <= 'F')) {
                    ubyte = value[i] - 'A' + 10;
                } else if ((value[i] >= 'a') && (value[i] <= 'f')) {
                    ubyte = value[i] - 'a' + 10;
                }
                if ((value[i + 1] >= '0') && (value[i + 1] <= '9'))  {
                    lbyte = value[i + 1] - '0';
                } else if ((value[i + 1] >= 'A') && (value[i + 1] <= 'F')) {
                    lbyte = value[i + 1] - 'A' + 10;
                } else if ((value[i + 1] >= 'a') && (value[i + 1] <= 'f')) {
                    lbyte = value[i + 1] - 'a' + 10;
                }
                tclas->clas.clas3.filter_value[i / 2] = (ubyte << 4) | lbyte;
            }
            tclas->clas.clas3.filter_len = len / 2;
        }
        if (strcmp(param, "filter_mask") == 0) {
            int i;
            int len;
            u_int8_t lbyte = 0, ubyte = 0;

            len = strlen(value);
            for (i = 0; i < len; i += 2) {
                if ((value[i] >= '0') && (value[i] <= '9'))  {
                    ubyte = value[i] - '0';
                } else if ((value[i] >= 'A') && (value[i] <= 'F')) {
                    ubyte = value[i] - 'A' + 10;
                } else if ((value[i] >= 'a') && (value[i] <= 'f')) {
                    ubyte = value[i] - 'a' + 10;
                }
                if ((value[i + 1] >= '0') && (value[i + 1] <= '9'))  {
                    lbyte = value[i + 1] - '0';
                } else if ((value[i + 1] >= 'A') && (value[i + 1] <= 'F')) {
                    lbyte = value[i + 1] - 'A' + 10;
                } else if ((value[i + 1] >= 'a') && (value[i + 1] <= 'f')) {
                    lbyte = value[i + 1] - 'a' + 10;
                }
                tclas->clas.clas3.filter_mask[i / 2] = (ubyte << 4) | lbyte;
            }
            tclas->clas.clas3.filter_len = len / 2;
        }
        if (strcmp(param, "version") == 0) {
            tclas->clas.clas14.clas14_v4.version = atoi(value);
        }
        if (strcmp(param, "sourceport") == 0) {
            tclas->clas.clas14.clas14_v4.source_port = atoi(value);
        }
        if (strcmp(param, "destport") == 0) {
            tclas->clas.clas14.clas14_v4.dest_port = atoi(value);
        }
        if (strcmp(param, "dscp") == 0) {
            tclas->clas.clas14.clas14_v4.dscp = atoi(value);
        }
        if (strcmp(param, "protocol") == 0) {
            tclas->clas.clas14.clas14_v4.protocol = atoi(value);
        }
        if (strcmp(param, "flowlabel") == 0) {
            int32_t flow;
            flow = atoi(value);
            memcpy(&tclas->clas.clas14.clas14_v6.flow_label, &flow, 3);
        }
        if (strcmp(param, "nextheader") == 0) {
            tclas->clas.clas14.clas14_v6.clas4_next_header = atoi(value);
        }
        if (strcmp(param, "sourceip") == 0) {
            if(inet_pton(AF_INET, value, &ipv4.sin_addr) <= 0) {
                if(inet_pton(AF_INET6, value, &ipv6.sin6_addr) <= 0) {
                    break;
                } else {
                    tclas->clas.clas14.clas14_v6.version = 6;
                    memcpy(tclas->clas.clas14.clas14_v6.source_ip,
                            &ipv6.sin6_addr, 16);
                }
            } else {
                tclas->clas.clas14.clas14_v4.version = 4;
                memcpy(tclas->clas.clas14.clas14_v4.source_ip,
                        &ipv4.sin_addr, 4);
            }
        }
        if (strcmp(param, "destip") == 0) {
            if(inet_pton(AF_INET, value, &ipv4.sin_addr) <= 0) {
                if (inet_pton(AF_INET6, value, &ipv6.sin6_addr) <= 0) {
                    break;
                } else {
                    memcpy(tclas->clas.clas14.clas14_v6.dest_ip,
                            &ipv6.sin6_addr, 16);
                }
            } else {
                memcpy(tclas->clas.clas14.clas14_v4.dest_ip,
                        &ipv4.sin_addr, 4);
            }
        }
    }
    if (!end) {
        printf("Error in Tclas Element \n");
        return -1;
    }

    return 0;
}

static int parse_actioncode(FILE *fp, u_int8_t *tfs_actioncode)
{
#define DELETEBIT 0
#define NOTIFYBIT 1
    char param[50] = {0};
    char value[50] = {0};
    char buf[50] = {0};
    int end = 0;
    u_int8_t delete = 0, notify = 0;
    char *pos;

    while(config_get_line(buf, sizeof(buf), fp, &pos)) {
        if (strcmp(pos, "}") == 0) {
            end = 1;
            break;
        }
        config_get_param_value(buf, pos, param,
                sizeof(param), value, sizeof(value));
        if(strcmp(param, "delete") == 0) {
            delete = atoi(value);
        }
        if(strcmp(param, "notify") == 0) {
            notify = atoi(value);
        }
    }
    if (!end) {
        printf("Subelement Configuration is not correct\n");
        return -1;
    }
    if (delete == 1)
        *tfs_actioncode |= 1 << DELETEBIT;
    else
        *tfs_actioncode &= ~(1 << DELETEBIT);

    if(notify == 1)
        *tfs_actioncode |= 1 << NOTIFYBIT;
    else
        *tfs_actioncode &= ~(1 << NOTIFYBIT);

    return 0;
}


static int parse_subelement(FILE *fp, int req_type, void *sub)
{
    int tclas_count = 0;
    int end = 0;
    int rate = 0;
    char *pos = NULL;
    char param[50] = {0};
    char value[50] = {0};
    char buf[50] = {0};
    struct tfsreq_subelement *tfs_subelem = (struct tfsreq_subelement *)sub;
    struct fmsreq_subelement *fms_subelem = (struct fmsreq_subelement *)sub;

    while (config_get_line(buf, sizeof(buf), fp, &pos)) {
        if (strcmp(pos, "}") == 0) {
            end = 1;
            break;
        }

        if (IEEE80211_WLANCONFIG_WNM_FMS_ADD_MODIFY == req_type) {
            config_get_param_value(buf, pos, param, sizeof(param), value, sizeof(value));
            if (strcmp(param, "delivery_interval") == 0) {
                fms_subelem->del_itvl = atoi(value);
            }

            config_get_param_value(buf, pos, param, sizeof(param), value, sizeof(value));
            if (strcmp(param, "maximum_delivery_interval") == 0) {
                fms_subelem->max_del_itvl = atoi(value);
            }

            config_get_param_value(buf, pos, param, sizeof(param), value, sizeof(value));
            if (strcmp(param, "multicast_rate") == 0) {
                rate = atoi(value);
                fms_subelem->rate_id.mask = rate & 0xff;
                fms_subelem->rate_id.mcs_idx = (rate >> 8) & 0xff;
                fms_subelem->rate_id.rate = (rate >> 16) & 0xffff;
            }
            if (strcmp(TCLAS_ELEMENT_STR, pos) == 0) {
                parse_tclas_element(fp, &fms_subelem->tclas[tclas_count++]);
            }

            config_get_param_value(buf, pos, param, sizeof(param), value, sizeof(value));
            if (strcmp(param, "tclas_processing") == 0) {
                fms_subelem->tclas_processing = atoi(value);
            }
        } else {

            if (strcmp(TCLAS_ELEMENT_STR, pos) == 0) {
                parse_tclas_element(fp, &tfs_subelem->tclas[tclas_count++]);
            }

            config_get_param_value(buf, pos, param, sizeof(param), value, sizeof(value));
            if (strcmp(param, "tclas_processing") == 0) {
                tfs_subelem->tclas_processing = atoi(value);
            }
        }

    }
    if (!end) {
        printf("Subelement Configuration is not correct\n");
        return -1;
    }
    if (IEEE80211_WLANCONFIG_WNM_FMS_ADD_MODIFY == req_type) {
        fms_subelem->num_tclas_elements = tclas_count;
    } else {
        tfs_subelem->num_tclas_elements = tclas_count;
    }

    return 0;
}

static int parse_fmsrequest(FILE *fp, struct ieee80211_wlanconfig_wnm_fms_req *fms)
{
    char param[50] = {0};
    char value[50] = {0};
    int end = 0;
    char *pos = NULL;
    char buf[512] = {0};
    int subelement_count = 0;
    int status;

    while(config_get_line(buf, sizeof(buf), fp, &pos)) {
        if (strcmp(pos, "}") == 0) {
            end = 1;
            break;
        }
        config_get_param_value(buf, pos, param, sizeof(param), value, sizeof(value));
        if(strcmp(param, "fms_token") == 0) {
            fms->fms_token = atoi(value);
        }

        if (strcmp(FMS_ELEMENT_STR, pos) == 0) {
            status = parse_subelement(fp, IEEE80211_WLANCONFIG_WNM_FMS_ADD_MODIFY,
                    (void *)&fms->subelement[subelement_count++]);
            if (status < 0) {
                break;
            }
        }
        fms->num_subelements = subelement_count;
    }

    if (!end) {
        printf("Subelement Configuration is not correct\n");
        return -1;
    }
    return 0;
}

static int parse_tfsrequest(FILE *fp, struct ieee80211_wlanconfig_wnm_tfs_req *tfs)
{
    char param[50] = {0};
    char value[50] = {0};
    int end = 0;
    char *pos = NULL;
    char buf[512] = {0};
    int subelement_count = 0;
    int status;

    while (config_get_line(buf, sizeof(buf), fp, &pos)) {
        if (strcmp(pos, "}") == 0) {
            end = 1;
            break;
        }
        config_get_param_value(buf, pos, param, sizeof(param), value, sizeof(value));
        if (strcmp(param, "tfsid") == 0) {
            tfs->tfsid = atoi(value);
        }
        if (strcmp(ACTION_STR, pos) == 0) {
            status = parse_actioncode(fp, &tfs->actioncode);
            if (status < 0) {
                break;
            }
        }
        if (strcmp(SUBELEMENT_STR, pos) == 0) {
            status = parse_subelement(fp, IEEE80211_WLANCONFIG_WNM_TFS_ADD,
                    (void *)&tfs->subelement[subelement_count++]);
            if (status < 0) {
                break;
            }
        }
        tfs->num_subelements = subelement_count;
    }

    if (!end) {
        printf("Subelement Configuration is not correct\n");
        return -1;
    }
    return 0;
}

static int handle_wnm(struct socket_context *sock_ctx, const char *ifname,
    int cmdtype, const char *arg1, const char *arg2)
{
    FILE *fp = NULL;
    char buf[512] = {0};
    char *pos = NULL;
    int end = 0;
    struct ieee80211_wlanconfig config;
    struct ieee80211_wlanconfig_wnm_bssmax *bssmax;
    struct ieee80211_wlanconfig_wnm_tfs *tfs;
    struct ieee80211_wlanconfig_wnm_fms *fms;
    struct ieee80211_wlanconfig_wnm_tim *tim;
    struct ieee80211_wlanconfig_wnm_bssterm *bssterm;
    int req_count = 0;
    int status = 0;
    u_int32_t timrate;

    memset(&config, 0, sizeof(config));

    config.cmdtype = cmdtype;
    switch(cmdtype) {
        case IEEE80211_WLANCONFIG_WNM_SET_BSSMAX:
            if (atoi(arg1) <= 0 || atoi(arg1) > 65534) {
                perror(" Value must be within 1 to 65534 \n");
                return -1;
            }
            bssmax = &config.data.wnm.data.bssmax;
            bssmax->idleperiod = atoi(arg1);
            if (arg2) {
                bssmax->idleoption = atoi(arg2);
            }
            break;
        case IEEE80211_WLANCONFIG_WNM_GET_BSSMAX:
            break;
        case IEEE80211_WLANCONFIG_WNM_FMS_ADD_MODIFY:
        case IEEE80211_WLANCONFIG_WNM_TFS_ADD:
            fp = fopen(arg1, "r");
            if (fp == NULL) {
                perror("Unable to open config file");
                return -1;
            }
            while(config_get_line(buf, sizeof(buf), fp, &pos)) {
                if (strcmp(pos, "}") == 0) {
                    end = 1;
                    break;
                }
                if (cmdtype == IEEE80211_WLANCONFIG_WNM_TFS_ADD) {

                    tfs = &config.data.wnm.data.tfs;
                    if (strcmp(TFS_REQUEST_STR, pos) == 0) {

                        status = parse_tfsrequest(fp,
                                &tfs->tfs_req[req_count++]);

                        if (status < 0) {
                            break;
                        }
                    }
                    tfs->num_tfsreq = req_count;
                }
                else {
                    fms = &config.data.wnm.data.fms;

                    if (strcmp(FMS_REQUEST_STR, pos) == 0) {
                        status = parse_fmsrequest(fp,
                                &fms->fms_req[req_count++]);

                        if (status < 0) {
                            break;
                        }
                    }
                    fms->num_fmsreq = req_count;
                }
            }
            if (feof(fp)) {
                if (status == 0) {
                    end = 1;
                }
            }
            fclose(fp);
            if (!end) {
                printf("Bad Configuration file....\n");
                exit(0);
            }
            break;

        case IEEE80211_WLANCONFIG_WNM_TFS_DELETE:
            tfs = &config.data.wnm.data.tfs;
            tfs->num_tfsreq = 0;
            break;

        case IEEE80211_WLANCONFIG_WNM_SET_TIMBCAST:

            tim = &config.data.wnm.data.tim;
            if (arg1) {
                tim->interval = atoi(arg1);
            }
            if (arg2) {
                timrate = atoi(arg2);
                tim->enable_highrate = timrate & IEEE80211_WNM_TIM_HIGHRATE_ENABLE;
                tim->enable_lowrate = timrate & IEEE80211_WNM_TIM_LOWRATE_ENABLE;
            }

        case IEEE80211_WLANCONFIG_WNM_GET_TIMBCAST:
            break;
        case IEEE80211_WLANCONFIG_WNM_BSS_TERMINATION:
            bssterm = &config.data.wnm.data.bssterm;
            bssterm->delay = atoi(arg1);
            if (arg2)
                bssterm->duration = atoi(arg2);
            break;
        default:
            printf("Unknown option: %d\n", cmdtype);
            break;
    }
    send_command(sock_ctx, ifname, &config, sizeof(struct ieee80211_wlanconfig),
            NULL, QCA_NL80211_VENDOR_SUBCMD_WNM, IEEE80211_IOCTL_CONFIG_GENERIC);
    if (cmdtype == IEEE80211_WLANCONFIG_WNM_GET_BSSMAX) {
        printf("IdlePeriod    : %d\n", config.data.wnm.data.bssmax.idleperiod);
        printf("IdleOption    : %d\n", config.data.wnm.data.bssmax.idleoption);
    }
    if (cmdtype == IEEE80211_WLANCONFIG_WNM_GET_TIMBCAST) {
        printf("TIM Interval     : %d\n", config.data.wnm.data.tim.interval);
        printf("High DataRateTim : %s\n",
                config.data.wnm.data.tim.enable_highrate ? "Enable" : "Disable");
        printf("Low DataRateTim : %s\n",
                config.data.wnm.data.tim.enable_lowrate ? "Enable" : "Disable");
    }
    return 0;
}

int handle_command_wnm(int argc, char *argv[], const char *ifname,
        struct socket_context *sock_ctx)
{
    if (argc < 4) {
        errx(1, "err : Insufficient arguments \n");
    }
    if (streq(argv[3], "setbssmax")) {
        if (argc < 5) {
            errx(1, "usage: wlanconfig athX wnm setbssmax");
        } else if (argc == 5) {
            argv[5] = 0;
        }
        handle_wnm(sock_ctx, ifname, IEEE80211_WLANCONFIG_WNM_SET_BSSMAX,
                argv[4], argv[5]);
    }
    if (streq(argv[3], "getbssmax")) {
        handle_wnm(sock_ctx, ifname, IEEE80211_WLANCONFIG_WNM_GET_BSSMAX, 0, 0);
    }
    if (streq(argv[3], "tfsreq")) {
        if (argc < 4) {
            errx(1, "no input file specified");
        }
        handle_wnm(sock_ctx, ifname, IEEE80211_WLANCONFIG_WNM_TFS_ADD, argv[4], 0);
    }
    if (streq(argv[3], "deltfs")) {
        handle_wnm(sock_ctx, ifname, IEEE80211_WLANCONFIG_WNM_TFS_DELETE, 0, 0);
    }
    if (streq(argv[3], "fmsreq")) {
        handle_wnm(sock_ctx, ifname, IEEE80211_WLANCONFIG_WNM_FMS_ADD_MODIFY,  argv[4], 0);
    }
    if (streq(argv[3], "gettimparams")) {
        handle_wnm(sock_ctx, ifname, IEEE80211_WLANCONFIG_WNM_GET_TIMBCAST,
                0, 0);
    }
    if (streq(argv[3], "timintvl")) {
        if (argc < 4) {
            errx(1, "err : Enter TimInterval in number of Beacons");
        } else {
            handle_wnm(sock_ctx, ifname, IEEE80211_WLANCONFIG_WNM_SET_TIMBCAST,
                    argv[4], 0);
        }
    }
    if (streq(argv[3], "timrate")) {
        char temp[10];
        if (argc < 6) {
            errx(1, "invalid args");
        } else {
            snprintf(temp, sizeof(temp), "%d", ((!!atoi(argv[4])) | (!!atoi(argv[5])) << 1));

            handle_wnm(sock_ctx, ifname, IEEE80211_WLANCONFIG_WNM_SET_TIMBCAST, 0, temp);
        }
    }
    /* BSS Termination */
    if (streq(argv[3], "bssterm")) {
        if (argc < 5) {
            errx(1, "usage: wlanconfig athX wnm bssterm <delay in TBTT> <duration in minutes>");
        } else {
            if (argc == 5)
                argv[5] = 0;
            handle_wnm(sock_ctx, ifname, IEEE80211_WLANCONFIG_WNM_BSS_TERMINATION, argv[4], argv[5]);
        }
    }
    return 0;
}
#endif
