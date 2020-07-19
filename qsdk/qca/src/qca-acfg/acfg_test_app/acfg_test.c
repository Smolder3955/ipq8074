/*
 * Copyright (c) 2015-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * 2015-2016 Qualcomm Atheros, Inc.
 *
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary
*/

#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<ctype.h>
#include<string.h>
#include <libgen.h>
#include<stdint.h>
#include<sys/types.h>

#include <acfg_types.h>
#include<acfg_api.h>


#include<acfg_tool.h>
#include<acfg_event.h>

extern struct socket_context g_sock_ctx;
#define BUFF_INIT_SIZE      500
#define NULLCHAR            '\0'

#define FW_DUMP_TFTP_CMD_PREFIX "cd /dev && tftp -l q6mem -p "
#define FW_DUMP_TFTP_DEFAULT_SERVER_IP "192.168.1.100"
#define FW_DUMP_TFTP_CMD_SUFFIX " 2>&1"
#define FW_DUMP_HOTPLUG_DEFAULT_APP "/etc/hotplug.d/dump/00-q6dump"

#define MAX_NUM_CHAINS   8
#define MAX_RXG_CAL_CHANS 8

/*
 * Prototypes
 */
uint32_t doapitest(char *argv[]);
int get_tbl_idx(char *name) ;
int display_params(char *name) ;
void usage(void);
int recv_events(char *ifname, int nonblock) ;
void recv_wps_events(void);

/* External Declaration */
extern fntbl_t fntbl[] ;
extern char *type_desc [] ;

/* Globals */
int acfg_event_log = 1;
char tftp_server[IP_ADDR_LEN +1] = FW_DUMP_TFTP_DEFAULT_SERVER_IP;

/* Options acepted by this tool
 *
 * p - Print description of command line parameters for acfg api
 * e - Wait for events
 */
static char *option_args = "ne::p::w::st:" ;
char *appname;

int main(int argc , char *argv[])
{

    int c;
    int argvidx = 0 ;
    int ret = 0 ;
    int opt_events = 0 ;
    int opt_events_nonblock = 0 ;
    int opt_disp_param = 0 ;
    char *opt_disp_param_arg = NULL;
    char *opt_event_arg = NULL;
    int opt_wps_event = 0;
    acfg_dl_init();

    appname = basename(argv[0]);


    while( (c = getopt(argc , argv , option_args)) != -1 )
    {
        switch (c)
        {
            case 'e':
                opt_events = 1 ;
                opt_event_arg = optarg ;
                break;

            case 'n':
                opt_events_nonblock = 1 ;
                break;

            case 'p':
                opt_disp_param = 1 ;
                opt_disp_param_arg = optarg ;
                break;
            case 'w':
                opt_wps_event = 1 ;
                break;
            case 's':
                acfg_event_log = 0 ;
                break;
            case 't':
                strlcpy(tftp_server, optarg, sizeof(tftp_server));
                break;
            case '?':
                /* getopt returns error */
                usage();
                return 0;

            default:
                usage();
                return 0;
        } //end switch
    }//end while

    argvidx = optind ;

    g_sock_ctx.cfg80211 = get_config_mode_type();
    init_socket_context(&g_sock_ctx, DEFAULT_NL80211_CMD_SOCK_ID,
                                   DEFAULT_NL80211_EVENT_SOCK_ID);

    if(opt_disp_param)
    {
        ret = display_params(opt_disp_param_arg) ;
    }
    else if(opt_events)
    {
        ret = recv_events(opt_event_arg,opt_events_nonblock);
    }
    else if (opt_wps_event)
    {
        recv_wps_events();
    }
    else if (argv[argvidx] != NULL)
        ret = doapitest( &argv[argvidx] );

    if(ret != 0)
    {
        printf("\n<<<<<<<<<< Dumping LOG >>>>>>>>>>>>>\n");
        printf("Error %d , try again. \n", ret);
        printf("%s", acfg_get_errstr());
        printf("\n<<<<<<<<<<<<<< End >>>>>>>>>>>>>>>>>\n");
    }

    destroy_socket_context(&g_sock_ctx);
    return ret ;
}

void usage(void)
{
    printf("\n");
    printf("\t%s <acfg api name> <api arguments> \n",appname);
    printf("\t%s -p \n\t\tPrint help for "\
            "all acfg apis\n\n",appname);

    printf("\t%s -p<acfg api name> \n\t\tPrint help for "\
            "one acfg api\n\n",appname);

    printf("\t%s -e <interface name> [-n]"\
            "\n\t\tWait for events on interface. "
            " -n issues a nonblocking call to acfg library\n\n",appname);
}

/**
 * @brief Get the index into the table of function
 *        pointers for this acfg api
 *
 * @param name - Acfg api name
 *
 * @return integer representing an index
 */
int get_tbl_idx(char *name)
{
    int j ;

    j = 0 ;
    while( fntbl[j].apiname != NULL)
    {
        if(strcmp(name , fntbl[j].apiname) == 0)
        {
            return j ;
        }
        j++;
    }

    return -1 ;

}


/**
 * @brief Execute test for a particular acfg api.
 *
 * @param argv[] - Array of charater pointers to NULL terminated
 *                 strings. argv[0] is the acfg api name. Command
 *                 line arguments for testing this api begin from argv[1].
 *
 * @return
 */
uint32_t doapitest(char *argv[])
{
    int param_num ;
    int idx ;
    char **pc ;
    uint32_t ret = 0;

    /* Get the index into the table of function pointers
     * which specifies the function to call to test this
     * acfg api.
     */
    idx = get_tbl_idx(argv[0]) ;

    if(idx < 0)
    {
        printf("Incorrect acfg api\n");
        return -1;
    }

    /* Need special care for these APIs
     * since they take variable numbers of parameters
     */
    if(strcmp("acfg_tx99_tool",fntbl[idx].apiname)==0 ||
         strcmp("acfg_offchan_rx",fntbl[idx].apiname)==0 ||
         strcmp("acfg_set_op_support_rates",fntbl[idx].apiname)==0){
        ret = fntbl[idx].wrapper(&argv[1]);
        goto exit;
    }

    /* Check for correct number of command line parameters
     * for this acfg api.
     */
    param_num = 0 ;
    pc = &argv[1] ;
    while(*pc != NULL)
    {
        param_num++ ;
        pc++ ;
    }

    if( param_num != fntbl[idx].num_param )
    {
        printf("Incorrect number of parameters\n");
        goto exit;
    }

    /* Call the wrapper function and return the
     * status.
     */
    ret = fntbl[idx].wrapper(&argv[1]) ;

exit:
    return ret;
}



/**
 * @brief Print the parameter info for one acfg api
 *
 * @param index
 *
 * @return
 */
int print_param_info(int index)
{
    param_info_t *pparam = NULL ;

    pparam = fntbl[index].param_info ;

    if(pparam)
    {
        int count = 0;
        printf("\n\n");
        printf("%s: \n",fntbl[index].apiname);
        printf("\tName\t\t\tType\t\t\tDescription\n");
        printf("\t----\t\t\t----\t\t\t-----------\n");
        while(count < fntbl[index].num_param && (pparam->name != NULL))
        {
            printf("\t%s\t\t\t%s\t\t\t%s\n",pparam->name,
                    type_desc[pparam->type], pparam->desc);
            count++; pparam++;
        }
    }
    else
    {
        dbglog("No param info specified for %s ",fntbl[index].apiname);
    }

    return 0;
}


/**
 * @brief Display parameter info
 *
 * @param name - If this is NULL, display param info
 *               for all acfg apis
 *
 * @return
 */
int display_params(char *name)
{
    int idx = -1 ;

    if(name)
    {
        idx =  get_tbl_idx(name) ;
        print_param_info(idx);
    }
    else
    {
        idx = 0 ;
        while( fntbl[idx].apiname != NULL )
        {
            print_param_info(idx);
            idx++;
        }
    }

    return 0;
}

uint32_t
acfg_logger(uint8_t *buf)
{
    FILE *ev_fp;

    if (!acfg_event_log)
        return QDF_STATUS_SUCCESS;

    ev_fp = fopen(ACFG_EVENT_LOG_FILE, "a+");
    if (ev_fp == NULL) {
        printf("unable to open event log file\n");
        return QDF_STATUS_E_FAILURE;
    }
    fprintf(ev_fp, "%s\n", buf);
    fclose(ev_fp);

    return QDF_STATUS_SUCCESS;
}


/*
 * Event Callbacks
 */
uint32_t
cb_assoc_sta(uint8_t *ifname, acfg_assoc_t *stadone)
{
    uint8_t buf[255];

    if (stadone->frame_send == 0) {
        snprintf((char *)buf, sizeof(buf),
                "%s:Event-assoc AP->STA:status %d  %02x:%02x:%02x:%02x:%02x:%02x\n",
                ifname,
                stadone->status,
                stadone->bssid[0],  stadone->bssid[1],
                stadone->bssid[2],  stadone->bssid[3],
                stadone->bssid[4],  stadone->bssid[5]);
    }
    acfg_logger(buf);
    return QDF_STATUS_SUCCESS ;
}

uint32_t
cb_disassoc_sta(uint8_t *ifname, acfg_disassoc_t *stadone)
{
    uint8_t buf[255];

    if (stadone->frame_send == 0) {
        snprintf((char *)buf, sizeof(buf),
                "%s:Event disssoc AP -> STA: reason %d %02x:%02x:%02x:%02x:%02x:%02x\n",
                ifname,
                stadone->reason,
                stadone->macaddr[0],  stadone->macaddr[1],
                stadone->macaddr[2],  stadone->macaddr[3],
                stadone->macaddr[4],  stadone->macaddr[5]);
    } else {
        snprintf((char *)buf, sizeof(buf),
                "%s:Event disassoc STA -> AP:status %d  reason %d %02x:%02x:%02x:%02x:%02x:%02x\n",
                ifname,
                stadone->status,
                stadone->reason,
                stadone->macaddr[0],  stadone->macaddr[1],
                stadone->macaddr[2],  stadone->macaddr[3],
                stadone->macaddr[4],  stadone->macaddr[5]);
    }
    acfg_logger(buf);
    return QDF_STATUS_SUCCESS ;
}

uint32_t
cb_assoc_ap(uint8_t *ifname, acfg_assoc_t *apdone)
{
    uint8_t buf[255];

    if (apdone->frame_send == 0) {
        snprintf((char *)buf, sizeof(buf),
                "%s:Event assoc STA -> AP status %d  %02x:%02x:%02x:%02x:%02x:%02x\n",
                ifname,
                apdone->status,
                apdone->bssid[0],  apdone->bssid[1],
                apdone->bssid[2],  apdone->bssid[3],
                apdone->bssid[4],  apdone->bssid[5]);
    } else {
        snprintf((char *)buf, sizeof(buf),
                "%s:Event assoc AP -> STA: status %d  \n",
                ifname,
                apdone->status);
    }
    acfg_logger(buf);
    return QDF_STATUS_SUCCESS ;
}

uint32_t
cb_disassoc_ap(uint8_t *ifname, acfg_disassoc_t *fail)
{
    uint8_t buf[255];

    if (fail->frame_send == 0) {
        snprintf((char *)buf, sizeof(buf),
                "%s:Event disassoc STA->AP: reason = %d %02x:%02x:%02x:%02x:%02x:%02x\n",
                ifname,
                fail->reason,
                fail->macaddr[0], fail->macaddr[1],
                fail->macaddr[2], fail->macaddr[3],
                fail->macaddr[4], fail->macaddr[5]
               );
    } else {
        snprintf((char *)buf, sizeof(buf),
                "%s:Event disassoc AP->STA: reason = %d status = %d %02x:%02x:%02x:%02x:%02x:%02x\n",
                ifname,
                fail->reason,
                fail->status,
                fail->macaddr[0], fail->macaddr[1],
                fail->macaddr[2], fail->macaddr[3],
                fail->macaddr[4], fail->macaddr[5]
               );
    }
    acfg_logger(buf);
    return QDF_STATUS_SUCCESS ;
}

uint32_t
cb_auth_sta(uint8_t *ifname, acfg_auth_t *stadone)
{
    uint8_t buf[255];

    if (stadone->frame_send == 0) {
        snprintf((char *)buf, sizeof(buf),
                "%s:Event auth AP->STA status %d :%02x:%02x:%02x:%02x:%02x:%02x\n",
                ifname,
                stadone->status,
                stadone->macaddr[0], stadone->macaddr[1],
                stadone->macaddr[2], stadone->macaddr[3],
                stadone->macaddr[4], stadone->macaddr[5]
               );
    } else {
        snprintf((char *)buf, sizeof(buf),
                "%s:Event auth STA->AP status %d\n",
                ifname,
                stadone->status);
    }
    acfg_logger(buf);
    return QDF_STATUS_SUCCESS ;
}


uint32_t
cb_deauth_sta(uint8_t *ifname, acfg_dauth_t *stadone)
{
    uint8_t buf[255];

    if (stadone->frame_send == 0) {
        snprintf((char *)buf, sizeof(buf),
                "%s:Event deauth AP->STA reason %d :%02x:%02x:%02x:%02x:%02x:%02x\n",
                ifname,
                stadone->reason,
                stadone->macaddr[0], stadone->macaddr[1],
                stadone->macaddr[2], stadone->macaddr[3],
                stadone->macaddr[4], stadone->macaddr[5]
               );
    } else {
      snprintf((char *)buf, sizeof(buf),
                "%s: Event deauth STA->AP status %d :%02x:%02x:%02x:%02x:%02x:%02x\n",
                ifname,
                stadone->status,
                stadone->macaddr[0], stadone->macaddr[1],
                stadone->macaddr[2], stadone->macaddr[3],
                stadone->macaddr[4], stadone->macaddr[5]
               );
    }
    acfg_logger(buf);
    return QDF_STATUS_SUCCESS ;
}

uint32_t
cb_auth_ap(uint8_t *ifname, acfg_auth_t *stadone)
{
    uint8_t buf[255];

    if (stadone->frame_send == 0) {
      snprintf((char *)buf, sizeof(buf),
                "%s: Event auth STA->AP status %d :%02x:%02x:%02x:%02x:%02x:%02x\n",
                ifname,
                stadone->status,
                stadone->macaddr[0], stadone->macaddr[1],
                stadone->macaddr[2], stadone->macaddr[3],
                stadone->macaddr[4], stadone->macaddr[5]
               );
    } else {
      snprintf((char *)buf, sizeof(buf), "AP -> STA auth status %d\n",
                stadone->status);
    }
    acfg_logger(buf);
    return QDF_STATUS_SUCCESS ;
}


uint32_t
cb_deauth_ap(uint8_t *ifname, acfg_dauth_t *stadone)
{
    uint8_t buf[255];

    if (stadone->frame_send == 0) {
        snprintf((char *)buf, sizeof(buf),
                "%s: Event deauth STA->AP reason %d :%02x:%02x:%02x:%02x:%02x:%02x\n",
                ifname,
                stadone->reason,
                stadone->macaddr[0], stadone->macaddr[1],
                stadone->macaddr[2], stadone->macaddr[3],
                stadone->macaddr[4], stadone->macaddr[5]
               );
    } else {
        snprintf((char *)buf, sizeof(buf),
                "%s: Event deauth AP->STA status %d :%02x:%02x:%02x:%02x:%02x:%02x\n",
                ifname,
                stadone->status,
                stadone->macaddr[0], stadone->macaddr[1],
                stadone->macaddr[2], stadone->macaddr[3],
                stadone->macaddr[4], stadone->macaddr[5]
               );
    }
    acfg_logger(buf);
    return QDF_STATUS_SUCCESS ;
}

uint32_t
cb_scan_done(uint8_t *ifname, acfg_scan_done_t *apdone)
{
    msg("Event-scan done: ifname - %s",(char *)ifname);
    return QDF_STATUS_SUCCESS ;
}

uint32_t
cb_wsupp_raw_message(uint8_t *ifname, acfg_wsupp_raw_message_t *raw)
{
    uint8_t buf[255];

    snprintf((char *)buf, sizeof(buf),
            "%s: Wsupp Raw: %s\n", ifname, raw->raw_message);
    acfg_logger(buf);
    return QDF_STATUS_SUCCESS ;
}

uint32_t
cb_wsupp_ap_sta_conn(uint8_t *ifname, acfg_wsupp_ap_sta_conn_t *conn)
{
    uint8_t buf[255];

    snprintf((char *)buf, sizeof(buf), "%s:%s\n", ifname, conn->raw_message);
    acfg_logger(buf);
    return QDF_STATUS_SUCCESS ;
}

uint32_t
cb_wsupp_ap_sta_disconn(uint8_t *ifname, acfg_wsupp_ap_sta_conn_t *conn)
{
    uint8_t buf[255];

    snprintf((char *)buf, sizeof(buf), "%s:%s\n", ifname, conn->raw_message);
    acfg_logger(buf);
    return QDF_STATUS_SUCCESS ;
}

uint32_t
cb_wsupp_wpa_conn(uint8_t *ifname, acfg_wsupp_wpa_conn_t *conn)
{
    uint8_t buf[255];

    snprintf((char *)buf, sizeof(buf), "%s:%s\n", ifname, conn->raw_message);
    acfg_logger(buf);
    return QDF_STATUS_SUCCESS ;
}

uint32_t
cb_wsupp_wpa_disconn(uint8_t *ifname, acfg_wsupp_wpa_conn_t *conn)
{
    uint8_t buf[255];

    snprintf((char *)buf, sizeof(buf), "%s:%s\n", ifname, conn->raw_message);
    acfg_logger(buf);
    return QDF_STATUS_SUCCESS ;
}

uint32_t
cb_wsupp_wpa_term(uint8_t *ifname, acfg_wsupp_wpa_conn_t *conn)
{
    msg("Event: %s: WPA TERMINATING: %s",(char *)ifname, conn->raw_message);
    return QDF_STATUS_SUCCESS ;
}

uint32_t
cb_wsupp_wpa_scan(uint8_t *ifname, acfg_wsupp_wpa_conn_t *conn)
{
    msg("Event: %s: WPA SCAN RESULT: %s",(char *)ifname, conn->raw_message);
    return QDF_STATUS_SUCCESS ;
}

uint32_t
cb_wsupp_assoc_reject(uint8_t *ifname, acfg_wsupp_assoc_t *assoc)
{
    uint8_t buf[255];

    snprintf((char *)buf, sizeof(buf), "%s:%s\n", ifname, assoc->raw_message);
    acfg_logger(buf);
    return QDF_STATUS_SUCCESS ;
}

uint32_t
cb_wsupp_eap_success(uint8_t *ifname, acfg_wsupp_eap_t *eap)
{
    uint8_t buf[255];

    snprintf((char *)buf, sizeof(buf), "%s:%s\n", ifname, eap->raw_message);
    acfg_logger(buf);
    return QDF_STATUS_SUCCESS ;
}

uint32_t
cb_wsupp_eap_failure(uint8_t *ifname, acfg_wsupp_eap_t *eap)
{
    uint8_t buf[255];

    snprintf((char *)buf, sizeof(buf), "%s:%s\n", ifname, eap->raw_message);
    acfg_logger(buf);
    return QDF_STATUS_SUCCESS ;
}

uint32_t
cb_wsupp_wps_enrollee(uint8_t *ifname, acfg_wsupp_wps_enrollee_t *enrollee)
{
    msg("Event: %s: WPS ENROLLEE SEEN: %s",
            (char *)ifname, enrollee->raw_message);
    return QDF_STATUS_SUCCESS ;
}

uint32_t
cb_push_button(uint8_t *ifname, acfg_pbc_ev_t *pbc)
{
    uint8_t buf[128];

    snprintf((char *)buf, sizeof(buf), "%s:Event Push button\n", ifname);
    acfg_logger(buf);
    return QDF_STATUS_SUCCESS;
}

uint32_t
cb_wsupp_wps_new_ap_setting(uint8_t * ifname,
        acfg_wsupp_wps_new_ap_settings_t *wps_new_ap)
{
    uint8_t buf[170];
    uint32_t status = QDF_STATUS_SUCCESS;

    status = acfg_handle_wps_event(ifname, ACFG_EVENT_WPS_NEW_AP_SETTINGS);
    snprintf((char *)buf,sizeof(buf),"%s:Wsupp wps recv new AP settings %s\n",
            ifname, wps_new_ap->raw_message);
    acfg_logger(buf);
    return status;
}

uint32_t
cb_wsupp_wps_success(uint8_t * ifname,
        acfg_wsupp_wps_success_t *wps_succ)
{
    uint8_t buf[170];
    uint32_t status = QDF_STATUS_SUCCESS;

    status = acfg_handle_wps_event(ifname, ACFG_EVENT_WPS_SUCCESS);
    snprintf((char *)buf,sizeof(buf),"%s:Wsupp wps success %s\n",
            ifname, wps_succ->raw_message);
    acfg_logger(buf);
    return status;
}

uint32_t
cb_chan_start(uint8_t * ifname,
        acfg_chan_start_t *chan)
{
    uint8_t buf[128];

    if (chan->reason == ACFG_CHAN_CHANGE_DFS) {
        snprintf((char *)buf, sizeof(buf), "%s: DFS channel change\n", ifname);
    } else{
        snprintf((char *)buf, sizeof(buf), "%s: Normal channel change\n", ifname);
    }
    acfg_logger(buf);
    snprintf((char *)buf, sizeof(buf), "%s: New Chan %d\n", ifname, chan->freq);
    acfg_logger(buf);

    return QDF_STATUS_SUCCESS ;
}

uint32_t
cb_radar(uint8_t * ifname,
        acfg_radar_t *radar)
{
    int i;
    uint8_t buf[128];

    /*
     * TODO: This is just a test code to set a new channel, which overrides
     * the channel choosen by the driver's dfs module.
     */
    acfg_set_chanswitch(ifname, 36);

    for (i = 0; i < radar->count; i++) {
        snprintf((char *)buf, sizeof(buf), "%s: RADAR. Chan %d unusable\n", ifname, radar->freqs[i]);
        acfg_logger(buf);
    }
    return QDF_STATUS_SUCCESS ;
}

uint32_t
cb_session_timeout(uint8_t * ifname,
                   acfg_session_t *session)
{
    uint8_t buf[128];
    uint32_t status = QDF_STATUS_SUCCESS;

    snprintf((char *)buf, sizeof(buf),
                "%s: Session timeout for STA %02x:%02x:%02x:%02x:%02x:%02x\n",
                ifname,
                session->mac[0], session->mac[1],
                session->mac[2], session->mac[3],
                session->mac[4], session->mac[5]);

    acfg_logger(buf);
    return status;
}

uint32_t
cb_tx_overflow(uint8_t * ifname)
{
    uint8_t buf[128];

    snprintf((char *)buf, sizeof(buf), "%s: TX overflow! \n", ifname);
    acfg_logger(buf);

    return QDF_STATUS_SUCCESS ;
}

uint32_t
cb_gpio_input(uint8_t * ifname,
                   acfg_gpio_t *gpio)
{
    uint8_t buf[128];
    uint32_t status = QDF_STATUS_SUCCESS;

    snprintf((char *)buf, sizeof(buf), "%s: GPIO pin %d changed state\n", ifname, gpio->num);
    acfg_logger(buf);

    return status;
}

uint32_t
cb_nf_dbr_dbm_info(uint8_t * ifname,
                   acfg_nf_dbr_dbm_t *nf_dbr_dbm)
{
    uint8_t i, j;
    uint8_t buf[128];
    uint32_t status = QDF_STATUS_SUCCESS;

    snprintf((char *)buf, sizeof(buf), "%s: nfdBr\tnfdBm", ifname);
    acfg_logger(buf);
    for (j = 0; j < MAX_RXG_CAL_CHANS; j++)
    {
        snprintf((char *)buf, sizeof(buf), "Freq = %d", nf_dbr_dbm->freqNum[j]);
        acfg_logger(buf);
        for (i = 0; i < MAX_NUM_CHAINS; i++)
        {
            snprintf((char *)buf, sizeof(buf), "%d\t%d",
                    nf_dbr_dbm->nfdbr[MAX_RXG_CAL_CHANS*j+i], nf_dbr_dbm->nfdbm[MAX_RXG_CAL_CHANS*j+i]);
            acfg_logger(buf);
        }
    }

    return status;
}

uint32_t
cb_packet_power_info(uint8_t * ifname,
                   acfg_packet_power_t *power_info)
{
    uint8_t buf[128];
    uint32_t status = QDF_STATUS_SUCCESS;
    snprintf((char *)buf, sizeof(buf), "%s: max packet power (dBm) = %f,"
             " min packet power (dBm) = %f", ifname,
            power_info->max_packet_power/2.0,
            power_info->min_packet_power/2.0);
    acfg_logger(buf);
    return status;
}

uint32_t
cb_mgmt_rx_info(uint8_t * ifname, acfg_mgmt_rx_info_t *mgmt_ri)
{
#define IEEE80211_ADDR_LEN 6
#define IEEE80211_FC0_TYPE_MASK             0x0c
#if MGMT_RX_INFO_DUMP
    struct ieee80211_frame {
        u_int8_t    i_fc[2];
        u_int8_t    i_dur[2];
        union {
            struct {
                u_int8_t    i_addr1[IEEE80211_ADDR_LEN];
                u_int8_t    i_addr2[IEEE80211_ADDR_LEN];
                u_int8_t    i_addr3[IEEE80211_ADDR_LEN];
            };
            u_int8_t    i_addr_all[3 * IEEE80211_ADDR_LEN];
        };
        u_int8_t    i_seq[2];
    } *wh;

    wh = (struct ieee80211_frame *)(mgmt_ri->raw_mgmt_frame);
    int type = -1;

    type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
    if(type == 0){
        printf("rx info: frame type=%d, management frame. \n", type);
    }else{
        printf("rx info: frame type=%d, other frame. \n", type);
    }

    printf("rx info: channel=%d\n", mgmt_ri->ri_channel);
    printf("rx info: rssi=%d\n", mgmt_ri->ri_rssi);
    printf("rx info: datarate=%d\n", mgmt_ri->ri_datarate);
    printf("rx info: flags=%d\n", mgmt_ri->ri_flags);
#endif

    return QDF_STATUS_SUCCESS ;
}

uint32_t
cb_wdt_event(uint8_t * ifname, acfg_wdt_event_t * wdt_event)
{
    uint8_t buf[128];
    uint8_t *bufp;
    uint32_t size;

    if(wdt_event->reason==ACFG_WDT_TARGET_ASSERT){
        snprintf((char *)buf, sizeof(buf), "%s: Watchdog event: target assert!\n", ifname);
        acfg_logger(buf);
    }else if(wdt_event->reason==ACFG_WDT_FWDUMP_READY){
        FILE *file;
        file = fopen(FW_DUMP_HOTPLUG_DEFAULT_APP, "r");
        if(!file) {
            /* If hotplug file doesn't exists, collect the dump here */
            bufp = buf;
            size = sizeof(buf);
            snprintf((char *)bufp, size, FW_DUMP_TFTP_CMD_PREFIX);
            bufp += (sizeof(FW_DUMP_TFTP_CMD_PREFIX) -1);
            size -= (sizeof(FW_DUMP_TFTP_CMD_PREFIX) -1);
            snprintf((char *)bufp, size, tftp_server);
            bufp += strlen(tftp_server);
            size -= strlen(tftp_server);
            snprintf((char *)bufp, size, FW_DUMP_TFTP_CMD_SUFFIX);
            system((char *)buf);
        }
        else {
            /* File exists, the hotplug would have read the FW dump */
            fclose(file);
        }
        snprintf((char *)buf, sizeof(buf), "%s: Watchdog event: FW Dump Ready!\n", ifname);
        acfg_logger(buf);
    }else if(wdt_event->reason==ACFG_WDT_REINIT_DONE){
        snprintf((char *)buf, sizeof(buf), "%s: Watchdog event: re-init done!\n", ifname);
        acfg_logger(buf);
        if (acfg_recover_profile((char *)ifname) == QDF_STATUS_SUCCESS) {
            snprintf((char *)buf, sizeof(buf), "%s: Profile recovered.\n", ifname);
            acfg_logger(buf);
        }
    }

    return QDF_STATUS_SUCCESS ;
}

uint32_t
cb_cac_start_event(uint8_t * ifname)
{
    uint8_t buf[128];

    snprintf((char *)buf, sizeof(buf), "%s: DFS CAC timeout \n", ifname);
    acfg_logger(buf);

    return QDF_STATUS_SUCCESS ;
}

uint32_t
cb_up_after_cac_event(uint8_t * ifname)
{
    uint8_t buf[128];

    snprintf((char *)buf, sizeof(buf), "%s: VAP up after CAC\n", ifname);
    acfg_logger(buf);

    return QDF_STATUS_SUCCESS ;
}

uint32_t
cb_ev_exceed_max_client(uint8_t * ifname)
{
    uint8_t buf[128];

    snprintf((char *)buf, sizeof(buf), "%s: ACFG Event exceed Max client\n", ifname);
    acfg_logger(buf);

    return QDF_STATUS_SUCCESS ;
}


#if defined(OL_ATH_SMART_LOGGING) && !defined(REMOVE_PKT_LOG)
uint32_t
cb_ev_smart_log_fw_pktlog_stop(uint8_t *ifname,
        acfg_smart_log_fw_pktlog_stop_t *slfwpktlog_stop)
{
    uint8_t buf[256];

    if ((ifname == NULL) || (slfwpktlog_stop == NULL)) {
        return QDF_STATUS_E_INVAL;
    }

    switch(slfwpktlog_stop->reason) {
        case ACFG_SMART_LOG_FW_PKTLOG_STOP_NORMAL:
            snprintf((char *)buf, sizeof(buf),
                    "%s: ACFG Event Smart Log FW initiated packetlog stop "
                    "event: Normal stop by FW.\n",
                    ifname);
            break;
        case ACFG_SMART_LOG_FW_PKTLOG_STOP_HOSTREQ:
            snprintf((char *)buf, sizeof(buf),
                    "%s: ACFG Event Smart Log FW initiated packetlog stop "
                    "event: Stop requested by host.\n",
                    ifname);
            break;
        case ACFG_SMART_LOG_FW_PKTLOG_STOP_DISABLE:
            snprintf((char *)buf, sizeof(buf),
                    "%s: ACFG Event Smart Log FW initiated packetlog stop "
                    "event: Stop since the feature is being disabled.\n",
                    ifname);
            break;
        default:
            snprintf((char *)buf, sizeof(buf),
                    "%s: ACFG Event Smart Log FW initiated packetlog stop "
                    "event: Unknown reason %u.\n",
                    ifname, slfwpktlog_stop->reason);
            break;
    }

    acfg_logger(buf);

    return QDF_STATUS_SUCCESS ;
}
#endif /* defined(OL_ATH_SMART_LOGGING) && !defined(REMOVE_PKT_LOG) */

uint32_t
cb_kickout(uint8_t * ifname, acfg_kickout_t *kickout)
{
    uint8_t buf[255];

    snprintf((char *)buf, sizeof(buf),
            "%s:Event kickout STA -> %02x:%02x:%02x:%02x:%02x:%02x\n",
            ifname,
            kickout->macaddr[0],  kickout->macaddr[1],
            kickout->macaddr[2],  kickout->macaddr[3],
            kickout->macaddr[4],  kickout->macaddr[5]);
    acfg_logger(buf);
    return QDF_STATUS_SUCCESS ;
}

uint32_t
cb_assoc_fail(uint8_t * ifname, acfg_assoc_failure_t *assoc_fail)
{
    uint8_t buf[255];

    snprintf((char *)buf, sizeof(buf),
            "%s: Event association failed for STA: %02x:%02x:%02x:%02x:%02x:%02x, reason code %d\n",
            ifname,
            assoc_fail->macaddr[0],  assoc_fail->macaddr[1],
            assoc_fail->macaddr[2],  assoc_fail->macaddr[3],
            assoc_fail->macaddr[4],  assoc_fail->macaddr[5],
            assoc_fail->reason);
    acfg_logger(buf);
    return QDF_STATUS_SUCCESS ;
}

uint32_t
cb_diagnostics(uint8_t * ifname,
        acfg_diag_event_t *diag)
{
    uint8_t buf[255];

    snprintf((char *)buf, sizeof(buf), "\n==================================================");
    acfg_logger(buf);
    snprintf((char *)buf, sizeof(buf), "%s:Diagnostics", ifname);
    acfg_logger(buf);

    if(diag->type & ACFG_DIAGNOSTICS_WARNING) {
        snprintf((char *)buf, sizeof(buf), "Type: WARNING");
        acfg_logger(buf);
    } else if(diag->type & ACFG_DIAGNOSTICS_ERROR) {
        snprintf((char *)buf, sizeof(buf), "Type: ERROR");
        acfg_logger(buf);
    } else if(diag->type & ACFG_DIAGNOSTICS_NORMAL) {
        snprintf((char *)buf, sizeof(buf), "Type: NORMAL");
        acfg_logger(buf);
    }

    if(diag->status & ACFG_DIAGNOSTICS_DOWN) {
        snprintf((char *)buf, sizeof(buf), "Status: Below threshold");
	    acfg_logger(buf);
    } else if(diag->status & ACFG_DIAGNOSTICS_DOWN_15_SEC) {
        snprintf((char *)buf, sizeof(buf), "Status: Below threshold for more than 15secs");
	    acfg_logger(buf);
    } else if(diag->status & ACFG_DIAGNOSTICS_DOWN_1_HR) {
        snprintf((char *)buf, sizeof(buf), "Status: Below threshold for more than 1hr");
	    acfg_logger(buf);
    } else {
        snprintf((char *)buf, sizeof(buf), "Status: Above threshold");
        acfg_logger(buf);
    }

    if (diag->data_rate_threshold) {
        snprintf((char *)buf, sizeof(buf), "Configured datarate threshold: %d", diag->data_rate_threshold);
        acfg_logger(buf);
    }

    snprintf((char *)buf, sizeof(buf), "Timestamp: %s", diag->tstamp);
    acfg_logger(buf);

    snprintf((char *)buf, sizeof(buf), "Client mode: %s", diag->mode);
    acfg_logger(buf);

    snprintf((char *)buf, sizeof(buf), "Client MAC: %02x:%02x:%02x:%02x:%02x:%02x",
            diag->macaddr[0],  diag->macaddr[1],
            diag->macaddr[2],  diag->macaddr[3],
            diag->macaddr[4],  diag->macaddr[5]);
    acfg_logger(buf);

    snprintf((char *)buf, sizeof(buf), "Power level: %d", diag->power_level);
    acfg_logger(buf);

    snprintf((char *)buf, sizeof(buf), "Rssi: %d", diag->rssi);
    acfg_logger(buf);

    snprintf((char *)buf, sizeof(buf), "Channel index: %d", diag->channel_index);
    acfg_logger(buf);

    snprintf((char *)buf, sizeof(buf), "Current Tx data rate: %d", diag->tx_data_rate);
    acfg_logger(buf);

    if (diag->mcs_index != 0xFF) {
        snprintf((char *)buf, sizeof(buf), "Last Tx pkt MCS index: %d", diag->mcs_index);
        acfg_logger(buf);
    } else {
        snprintf((char *)buf, sizeof(buf), "Last Tx pkt MCS index: NA(Legacy datarate)");
        acfg_logger(buf);
    }

    snprintf((char *)buf, sizeof(buf), "Last Tx Channel width: %d", diag->chan_width);
    acfg_logger(buf);

    if(diag->sgi) {
        snprintf((char *)buf, sizeof(buf), "Last Tx Guard Interval: Short");
        acfg_logger(buf);
    } else {
        snprintf((char *)buf, sizeof(buf), "Last Tx Guard Interval: Long");
        acfg_logger(buf);
    }

    snprintf((char *)buf, sizeof(buf), "Last Tx pkt no of spatial streams: %d", diag->nss);
    acfg_logger(buf);

    if (diag->airtime != 0xFF) {
        snprintf((char *)buf, sizeof(buf), "Airtime: %d percentage", diag->airtime);
        acfg_logger(buf);
    }

    snprintf((char *)buf, sizeof(buf), "==================================================\n");
    acfg_logger(buf);

    return QDF_STATUS_SUCCESS ;
}

uint32_t
cb_chan_stats(uint8_t * ifname, acfg_chan_stats_t *chan_stats)
{
    uint8_t buf[255];

    snprintf((char *)buf, sizeof(buf),
            "%s: Chan stats: frequency: %u, noise_floor: %d, obss_utilization: %u, self_bss_utilization: %u\n",
            ifname,
            chan_stats->frequency,
            chan_stats->noise_floor,
            chan_stats->obss_util,
            chan_stats->self_bss_util);

    acfg_logger(buf);
    return QDF_STATUS_SUCCESS ;
}

acfg_event_t ev ;

/**
 * @brief Receive events
 *
 * @param ifname
 * @param nonblock - 1 for nonblocking call
 *                   0 for blocking call
 * @return
 */
int recv_events(char *ifname, int nonblock)
{
    uint32_t status ;
    acfg_event_mode_t evmode ;

    if(nonblock == 1)
        evmode = ACFG_EVENT_NOBLOCK ;
    else
        evmode = ACFG_EVENT_BLOCK ;

    msg("Issuing %s call to wait for events",\
            evmode==ACFG_EVENT_NOBLOCK ? "nonblocking " : "blocking");

    ev.assoc_sta = cb_assoc_sta ;
    ev.disassoc_sta = cb_disassoc_sta ;
    ev.assoc_ap = cb_assoc_ap ;
    ev.disassoc_ap = cb_disassoc_ap ;
    ev.auth_sta = cb_auth_sta ;
    ev.deauth_sta = cb_deauth_sta ;
    ev.auth_ap = cb_auth_ap ;
    ev.deauth_ap = cb_deauth_ap ;
    ev.scan_done = cb_scan_done ;
    ev.wsupp_raw_message = cb_wsupp_raw_message;
    ev.wsupp_ap_sta_conn = cb_wsupp_ap_sta_conn;
    ev.wsupp_ap_sta_disconn = cb_wsupp_ap_sta_disconn;
    ev.wsupp_wpa_conn = cb_wsupp_wpa_conn;
    ev.wsupp_wpa_disconn = cb_wsupp_wpa_disconn;
    ev.wsupp_wpa_term = cb_wsupp_wpa_term;
    ev.wsupp_wpa_scan = cb_wsupp_wpa_scan;
    ev.wsupp_wps_enrollee = cb_wsupp_wps_enrollee;
    ev.wsupp_assoc_reject = cb_wsupp_assoc_reject;
    ev.wsupp_eap_success = cb_wsupp_eap_success;
    ev.wsupp_eap_failure = cb_wsupp_eap_failure;
    ev.push_button = cb_push_button;
    ev.wsupp_wps_new_ap_setting = cb_wsupp_wps_new_ap_setting;
    ev.wsupp_wps_success = cb_wsupp_wps_success;
    ev.radar = cb_radar;
    ev.chan_start = cb_chan_start;
    ev.session_timeout = cb_session_timeout;
    ev.tx_overflow = cb_tx_overflow;
    ev.mgmt_rx_info = cb_mgmt_rx_info;
    ev.wdt_event = cb_wdt_event;
    ev.nf_dbr_dbm_info = cb_nf_dbr_dbm_info;
    ev.packet_power_info = cb_packet_power_info;
    ev.cac_start = cb_cac_start_event;
    ev.up_after_cac = cb_up_after_cac_event;
    ev.kickout = cb_kickout;
    ev.assoc_failure = cb_assoc_fail;
    ev.diagnostics = cb_diagnostics;
    ev.chan_stats = cb_chan_stats;
    ev.exceed_max_client = cb_ev_exceed_max_client;
#if defined(OL_ATH_SMART_LOGGING) && !defined(REMOVE_PKT_LOG)
    ev.smart_log_fw_pktlog_stop = cb_ev_smart_log_fw_pktlog_stop;
#endif /* defined(OL_ATH_SMART_LOGGING) && !defined(REMOVE_PKT_LOG) */

    status = acfg_recv_events(&ev, evmode);
    if(status != QDF_STATUS_SUCCESS && status != QDF_STATUS_E_SIG)
    {
        msg("Acfg lib returned error...");
        goto errout;
    }

    if(evmode == ACFG_EVENT_NOBLOCK)
    {
        msg("Returned from acfg lib call. Going to sleep...");
        while(1)
            sleep(1000);
    }

errout: ;
        return acfg_to_os_status(status) ;
}

void recv_wps_events(void)
{
    uint32_t status = QDF_STATUS_SUCCESS;

    ev.wsupp_wps_new_ap_setting = cb_wsupp_wps_new_ap_setting;
    ev.wsupp_wps_success = cb_wsupp_wps_success;
    status = acfg_recv_events(&ev, ACFG_EVENT_BLOCK);
    if(status != QDF_STATUS_SUCCESS && status != QDF_STATUS_E_SIG)
    {
        msg("Acfg lib returned error...");
    }
}
