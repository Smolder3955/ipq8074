/*
 * Copyright (c) 2011 Qualcomm Atheros, Inc..
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include<stdio.h>
#include<stdlib.h>
#include<sys/select.h>
#include<string.h>
#include<getopt.h>
#include<pthread.h>
#include<signal.h>
#include<math.h>
#include<libconfig.h>

#include<ieee80211_hlsmgr.h>
#include<ap.h>
#include<connections.h>
#include<station.h>
#include<pthread.h>
#include<sys/time.h>
#include "version.h"

#define CLEAR_PKT_TYPE 0xFFE3
#define CLEAR_RQST_TYPE 0xFFFC
#define MIN_REQ_WAIT_PERIOD 200
#define DEFAULT_WAIT_BETWEEN_MRQSTS_IN_MS 10000    //10 seconds

int showdebug = 0;
int showchanneldump = 0;
int requestinterval = 10;
int respoutputfmt = 0;
int showrequestresp = 0;
short int rtt_count= 50;
int interactive = 0 ;
int wakeup_station = 0;
int synchronize_ap = 0;
int sta_req = 0;
int cap_req = 0;
int wpc_debug_req = 0;
long int no_of_stations = 0;
u_int16_t transmit_mode = 0;
u_int32_t transmit_rate = 0x0b0b0b0b;
long int no_of_aps = 1;
int new_ap=0;
int analyzeresult = 0;
uint16_t nreq = 1000;

int testduration_in_seconds = 0;
char  generatereport[50] ={0};

uint16_t burst_wait_ms = 0;
int maxfd=0;

u_int8_t lci_req = 0;
u_int8_t loc_civ_req = 0;
u_int8_t req_report_type = 0; //0: FAC, 1: CFR
u_int8_t req_preamble = 0; //0: Leagcy, 2: HT, 3: VHT
u_int8_t asap_mode = 0; // asap 0 or 1
u_int8_t burst_exp = 0;
u_int16_t burst_dur = 15;  //Default: 15
u_int16_t ftm_per_burst, burst_period = 0;


FILE *dumpfile=NULL;
#define MAX_NO_OF_APS 30
#define MAX_NO_OF_STATION 15
struct ap aps[MAX_NO_OF_APS];
struct station station[MAX_NO_OF_STATION];
pthread_mutex_t lock_response;
time_t starttime={0}, endtime={0};
fd_set readset;



int create_ap(void *addr1)
{
    int i=0;

    for(i=0 ; i < (no_of_aps + new_ap); i++) {
        if(memcmp(&aps[i].sockaddr.sin_addr, addr1, 4) == 0) {
            printf("Allocating new ap at index %d\n",new_ap);
            return i;
        }
    }
    new_ap+=1;
    memcpy(&aps[i].sockaddr.sin_addr, addr1, 4);
    printf("Allocating new ap at index %d\n",i);

    return i;


}



int app_init()
{
    const char *ptr = NULL;
    char str[18], str1[18], str2[18],ipaddr[4];
    int i=0, j=0, k=0;
    int result = 0;
    int sta_bandwidth = 0;

    config_t config;
    config_setting_t *array, *apsetting, *sta_setting;
    config_setting_t *station_details;

    config_init(&config);

    result  = config_read_file(&config, "nbpserver.conf");
    if( !result ) {
        printf("Error in reading configuration file");
        config_destroy(&config);
        return -1;
    }

    array = config_lookup(&config,"nbpserver.station");


    if( !array ) {
        printf("Error : Didnt find any station details in conf file\n");
    }


    config_lookup_int(&config, "nbpserver.no_of_aps", &no_of_aps);


    array = config_lookup(&config,"nbpserver.apdetails");

    if( !array ) {
        printf("Error : Didnt find any apdetails in conf file\n");
    }


    for( i=0; i<no_of_aps ; i++ ) {

        apsetting = config_setting_get_elem( array, i);

        if ( !apsetting ) {
            printf("No element at the specified index\n");
        }
        else {


            config_setting_lookup_string(apsetting, "ipaddr", &ptr);

            if(inet_pton(AF_INET, ptr, &aps[i].sockaddr.sin_addr) <=0) {
                printf("IPaddress is not in a valid format[%s]\n",ptr);
            }


            aps[i].id=i;
            aps[i].state = NOT_CONNECTED;
            aps[i].sockfd=-1;
            aps[i].request_count = 0;
            aps[i].response_count = 0;


            config_setting_lookup_string(apsetting, "name", &ptr);
            config_setting_lookup_int(apsetting, "channel", &aps[i].channel);
            memcpy(aps[i].apname, ptr, strlen(ptr));

        }

    }


    config_lookup_int(&config, "nbpserver.no_of_stations", &no_of_stations);
    station_details = config_lookup(&config, "nbpserver.station");
    printf("No of stations to be probed : %d\n",no_of_stations);

    for ( j=0; j<no_of_stations;j++, i++) {
        sta_setting = config_setting_get_elem(station_details, j);
        if(!sta_setting) {
            printf("No Sta setting at the specified index\n");

        } else {
            config_setting_lookup_string(sta_setting, "mac", &ptr);
            str2ba(ptr, &station[j].station_mac);
            config_setting_lookup_string(sta_setting, "apmac", &ptr);

            str2ba(ptr, &station[j].ap_mac);
            station[j].associated_ap = i;

            config_setting_lookup_string(sta_setting, "ssid", &ptr);
            memset(station[j].ssid, '\0', 36);
            memcpy(station[j].ssid, ptr, strlen(ptr));
            config_setting_lookup_int(sta_setting, "channel", &station[j].station_channel);
            config_setting_lookup_int(sta_setting, "wakeup_interval", &station[j].wakeup_interval);
            station[j].station_bandwidth = 0; //BW_20 default
            config_setting_lookup_int(sta_setting, "bandwidth", &sta_bandwidth);
            if (sta_bandwidth == 40)
		station[j].station_bandwidth = 1;  //1: BW_40
            if (sta_bandwidth == 80)
		station[j].station_bandwidth = 2;  //2: BW_80
            if (sta_bandwidth == 160)
		station[j].station_bandwidth = 3; //3: BW_160

            printf("Station bandwidth(0:20, 1:40, 2:80, 3:160): %d\n", station[j].station_bandwidth);

            config_setting_lookup_string(sta_setting, "apipaddr", &ptr);
            printf(" IPaddress : %s \n",ptr);

            if(inet_pton(AF_INET, ptr, &station[j].ap_ipaddr.sin_addr) <=0) {
                printf("IPaddress is not in a valid format[%s]\n",ptr);
            }

            if(wakeup_station) {
            station[j].associated_ap = create_ap(&station[j].ap_ipaddr.sin_addr);
            }
            for(k=0;k<MAX_NO_OF_APS; k++)
            {
                station[j].response_count_from_ap[k] = 0;
                station[j].request_count_to_ap[k] = 0;
            }

            ba2str(&station[j].station_mac,str1);
            ba2str(&station[j].ap_mac,str2);
            printf("Station details : mac : %s apmac : %s, apip %s channel %d ssid %s wakeup_interval %d\n", str1, str2,
                    inet_ntop(AF_INET,&aps[station[j].associated_ap].sockaddr.sin_addr,str,sizeof(str)), station[j].station_channel, station[j].ssid, station[j].wakeup_interval);
        }   
    }


    aps[0].color = 0x00FF0000;
    aps[1].color = 0x0000FF00;
    aps[2].color = 0x000000FF;

    config_destroy(&config);
    if(connect_with_aps()==0) {

        printf("Didnt Connect with any APs. Halt NBPServer\n");
        exit(0);
    }

    printf("Created connections with APs           [OK]\n");

}


static void cleanup(int sig)
{
    int i=0, j=0, k=0;
    static int cleanprocess=1;
    char str[120];
    int len =0;
    FILE *reportfile;
    time_t ti={0};

    if( strlen(generatereport)) {
        time(&endtime);
        sprintf(str, "%s",generatereport, ctime(&starttime));

       len = strlen(str);
        for ( i=0; i < len; i++)
            if(str[i] == ' ' || str[i] == '\n')
                str[i] = '_';
        str[len]=0;
       reportfile = fopen(str, "w");
       
       fprintf(reportfile,"\n*******************************STRESS TEST REPORT*******************************\n");
       fprintf(reportfile,"Start Time : %s",ctime(&starttime));
       fprintf(reportfile,"End Time   : %s",ctime(&endtime));
       fprintf(reportfile,"\n********************************************************************************\n");
       fprintf(reportfile,"AP Configuration\n\n");
       for( i = 0 ; i < no_of_aps ; i++)
           fprintf(reportfile,"%-18s\t%-18s\t%2d Ch \n",aps[i].apname,inet_ntop(AF_INET,&aps[i].sockaddr.sin_addr,str,sizeof(str)), aps[i].channel);

       fprintf(reportfile,"\n********************************************************************************\n");
       fprintf(reportfile,"STA Configuration\n\n");
       for( i = 0 ; i < no_of_stations ; i++) {
           ba2str(&station[i].station_mac, str);
           fprintf(reportfile,"%18s\t Associated to AP %s operating on channel %2d.  SSID : %s", str, aps[station[i].associated_ap].apname,aps[station[i].associated_ap].channel,station[i].ssid);
           ba2str(&station[i].ap_mac, str);
           fprintf(reportfile, " AP_MAC  : %s\n",str);
       }       
       fprintf(reportfile,"\n\n********************************************************************************\n\n");
       for(j=0; j< no_of_stations; j++)
       {
           ba2str(&station[j].station_mac,str);
           fprintf(reportfile,"Mac   : %-20s  Req : %-10u  Resp : %-10u Pass % : %3.2f \n",str, station[j].request_count, station[j].response_count,100.0 * ((float)station[j].response_count/(float)station[j].request_count));
          for( k = 0; k < no_of_aps ; k++)
          {
              fprintf(reportfile,"\t%-20s  Req : %-10u  Resp : %-10u Pass % : %3.2f \tForStaMac : %-20s\n",aps[k].apname, station[j].request_count_to_ap[k], station[j].response_count_from_ap[k],100.00 * ((float) station[j].response_count_from_ap[k] / (float) station[j].request_count_to_ap[k]), str) ; 
          }
           fprintf(reportfile,"\n\n");
       }
       fprintf(reportfile,"\n\n");

        for(j=0; j < no_of_aps ; j++)
        {
            fprintf(reportfile,"From %-20s -> Pass % : %3.2f   Req : %-10u Resp : %-10u   \n",aps[j].apname,100.0 * ((float)aps[j].response_count/(float)aps[j].request_count), aps[j].request_count, aps[j].response_count);
        }
        fprintf(reportfile,"\n********************************************************************************\n");

    }

    printf("\nCleanup routine reached\nClosing connections\n");


    if (dumpfile) {
        printf("Flushing buffered data to file\n");
        fclose(dumpfile);
        dumpfile=NULL;
    }

    printf("Stopping NBP Server\n");

    // Did we wakeup any station?
    if(wakeup_station) {
        for(i=0; i<no_of_stations; i++)
            send_sleep_req(&aps[station[i].associated_ap], &station[i].station_mac);

        sleep(5);
        wakeup_station = 0;
    }
    if (synchronize_ap) {
        for (i=0; i<no_of_aps; i++)
            send_sleep_req(&aps[i], 0x0);

        sleep(5);
        synchronize_ap = 0;
    }
    //TODO : Free allocated memory here 
    close_connections();

    if(strlen(generatereport))
        fclose(reportfile);

    if(sig == SIGALRM)
        printf("\nSUCCESSFULLRUN\n");

    exit(0);
}

int readn(int sockfd, char *buff, int resplen )
{
    int  readlen = 0, toberead = 0;
    static int readcount = 0;

    if (sockfd < 0 )
        return -1;
    readcount = 0;
    toberead = resplen;
    while( toberead!=0 && readcount != resplen ) {
        readlen = read(sockfd, buff+readcount, toberead);
        toberead = toberead - readlen;
        readcount = readcount + readlen;
    }
    return readcount;
}
void get_time_string(int mode24)
{
    struct timeval tv;
    time_t curtime;
    char time_buf[30];
    gettimeofday(&tv, NULL);
    curtime=tv.tv_sec;
    strftime(time_buf,30,"%m-%d-%Y  %T.",localtime(&curtime));
    printf("%s %ld\n",time_buf,tv.tv_usec);
}

void* recv_responses_and_update_gui(void *arg)
{

    char buff[WIFIPOS_TYPE1_RES_SIZE];
    char *sta_buff;
    struct ap *ap;
    int i = 0, j = 0, k = 0;
    int result = 0, readlen = 0, resplen = 0, sta_buff_len = 0;
    static int readcount = 0;
    unsigned int tmp = 0;
    struct nsp_wakeup_resp *wak_up;
    struct nsp_station_info *sta_info;
    struct nsp_tsf_resp *tsf_response;
    struct nsp_mresp *mres;
    int stationid = 0;
    char str[19];
    int sta_cnt, sta_i, vap_cnt = 0 ;

    maxfd=find_maxfd();

    while(1) {
        if( select(maxfd+1, &readset, NULL, NULL, NULL) == -1 ) {
            perror("select\n");
        }
        else {

            for(i=0 ; i<maxfd+1 ; i++)	{
                if(FD_ISSET(i, &readset)) {

                    memset(buff,0,sizeof(buff));
                    ap = find_ap_by_sockfd(i);
                    readlen = readn(i, buff, NSP_HDR_LEN);

                    if(readlen != NSP_HDR_LEN) { 
                        printf("ERROR: Didnt read the nsp hdr properly\n");
                    }

                    int frame_type = ((struct nsp_header*)buff)->frame_type;

                    if ( showrequestresp ) {
                        print_nsp_header( buff);
                    }

                    if( frame_type ==  NSP_TYPE1RESP ) {
                        resplen = MRES_LEN;
                    } else if ( frame_type == NSP_TYPE0RESP ) {
                        resplen = TYPE0RES_LEN ; 
                    } else if ( frame_type == NSP_SRESP ) {
                        resplen = SRES_LEN;
                    } else if ( frame_type == NSP_CRESP ) {
                        resplen = CAPRES_LEN;
                    } else if ( frame_type == NSP_WAKEUPRESP ) {
                        resplen = WAKEUPRES_LEN;
                    } else if ( frame_type == NSP_TSFRESP ) {
                        resplen = TSFRES_LEN;
                    } else if ( frame_type == NSP_STARETURNTOSLEEPRES ) {
                        resplen = SLEEPRES_LEN;
                    } else if ( frame_type == NSP_WPCDBGRESP ) {
                        resplen = WPCDBGRESP_LEN;
                    } else if ( frame_type == NSP_ERRORMSG ) {
                        resplen = ERRORMSG_LEN;
                    } else {
                        printf("\nERROR: Unknown response recieved from ap : 0x%2x \n",frame_type);
                    }

                    readcount = readn(i, buff, resplen);

                    if (readcount != resplen) {
                        printf("ERROR: Didnt read expected no of bytes from the socket\n");
                    } else {
                        if( frame_type ==  NSP_TYPE1RESP ) {
                            print_rtt_mres(buff);
                            mres = (struct nsp_mresp *)buff;
                            /* Removeing ntohs conversion as wpc is currently not converting these fields to network byte order before sending */
                            if((mres->result) == 0 || (mres->result) == 1 || ((mres->result) == 4 && (mres->no_of_responses) !=0)) {
                                resplen = TYPE1RES_LEN; 
                                readcount = readn(i, buff+MRES_LEN, resplen);

                                print_type1_res(((char*)buff+MRES_LEN));

                                if(!(ntohs(mres->result) == 4 && ntohs(mres->no_of_responses) !=0)) {
                                    stationid = inc_station_response_count(((struct nsp_mresp *)buff)->sta_mac_addr);
                                    station[stationid].response_count_from_ap[ap->id] += 1;
                                    ap->response_count++;
                                }
                                if(analyzeresult)
                                {
                                    printf("\n********************************************************************************\n\n");
                                    for(j=0; j< no_of_stations; j++)
                                    {
                                        ba2str(&station[j].station_mac,str);
                                        printf("Mac   : %-20s  Req : %-10u  Resp : %-10u Pass % : %3.2f \n",str, station[j].request_count, station[j].response_count,100.0 * ((float)station[j].response_count/(float)station[j].request_count));
                                        for( k = 0; k < no_of_aps ; k++)
                                        {
                                            printf("\t%-20s  Req : %-10u  Resp : %-10u Pass % : %3.2f \n",aps[k].apname, station[j].request_count_to_ap[k], station[j].response_count_from_ap[k],100.00 * ((float) station[j].response_count_from_ap[k] / (float) station[j].request_count_to_ap[k])) ; 
                                        }
                                        printf("\n");
                                    }
                                    printf("\n");

                                    for(j=0; j < no_of_aps ; j++)
                                    {
                                        printf("From %-20s -> Pass % : %3.2f   Req : %-10u Resp : %-10u   \n",aps[j].apname,100.0 * ((float)aps[j].response_count/(float)aps[j].request_count), aps[j].request_count, aps[j].response_count);
                                    }
                                    printf("\n********************************************************************************\n");
                                }
                                printf("\nResponse time");
                                get_time_string(1);
                            }
                        } else if ( frame_type == NSP_TYPE0RESP ) {
                            print_type0_res(((char*)buff));
                        } else if ( frame_type == NSP_SRESP ) {
                            sta_cnt = ntohs(((struct nsp_sresp *)buff)->no_of_stations);
                            vap_cnt = ntohs(((struct nsp_sresp *)buff)->no_of_vaps);
                            sta_buff_len = vap_cnt *VAPINFO_LEN + sta_cnt * SINFO_LEN;
                            readcount = readn(i, (char *)buff+SRES_LEN , sta_buff_len);
                            print_sresp(buff);

                            if (readcount != sta_buff_len) {
                                printf("ERROR: Didnt read expected no of bytes from the socket\n");
                            }
                            
                        } else if ( frame_type == NSP_CRESP ) {
                            print_cap_resp(buff);
                        } else if ( frame_type == NSP_WPCDBGRESP ) {
                            print_wpcdbgresp(buff);
                        } else if ( frame_type == NSP_WAKEUPRESP ) {
                            print_wakeup_resp(buff);
                            for(k=0; k < no_of_stations; k++) {
                                wak_up = (struct nsp_wakeup_resp *)buff;
                                if(memcmp(&station[k].station_mac,&wak_up->sta_mac_addr, 6) == 0) {
                                    station[k].result = wak_up->result;
                                }
                            }

                        } else if ( frame_type == NSP_TSFRESP ) {
                            print_tsf_resp(buff);
                            tsf_response = (struct nsp_tsf_resp *)buff;
                            for(k=0; k < no_of_stations; k++) {
                                    station[k].result = tsf_response->result;
                            }
                        } else if ( frame_type == NSP_STARETURNTOSLEEPRES ) {
                            print_sleep_resp(buff);
                        } else if ( frame_type == NSP_ERRORMSG ) {
                            print_error_msg(buff);
                        } else {
                            printf("ERROR: Unknown response recieved from ap : 0x%2x \n",frame_type);
                        }

                    }

                    if ( showchanneldump && frame_type ==  NSP_TYPE1RESP) {
                        struct nsp_type1_resp *type1resp = (struct nsp_type1_resp *) (((char *)buff) + MRES_LEN);
                        int no_cdump = ((CDUMP_SIZE_PER_CHAIN * type1resp->no_of_chains)/4) + (type1resp->no_of_chains%2) ; 

                        printf("Channel Dump :");
                        for(j=0;j< no_cdump ;j++) {
                            if(j%10==0)
                                printf("\n");
                            tmp = *(((unsigned int *)type1resp->type1_payld)+j);
                            printf("%08x ",tmp);
                        }

                        printf("\n\n");
                    }
                    
                    if(dumpfile && frame_type ==  NSP_TYPE1RESP) {
                        get_time_string(1);
                        fprintf(dumpfile,"%d\t",ap->request_count);
                        dump_response_to_file( dumpfile, buff, ap );
                    }
                    if(dumpfile && frame_type ==  NSP_TYPE0RESP) {
                        get_time_string(1);
                        dump_resp0_to_file(dumpfile, ((char*)buff));
                    }
                }
                else
                {
                    //					printf("No data available yet to read \n");
                }

            }

            FD_ZERO(&readset);

            for(j = 0; j < (no_of_aps + new_ap); j++)
                if(aps[j].sockfd > 0)
                    FD_SET(aps[j].sockfd, &readset);
        }
    }
    return NULL;
}
void usage()
{
    printf("\n\nnbpserver - NBP Server.\n\n");
    printf("\t-a, --analyzeresult     Analyzeresult\n");
    printf("\t-T, --testduration      Run test for specified seconds\n");
    printf("\t-g, --generatereport    Generate Report\n");
    printf("\t-d, --showdebug         Display debug messages\n");
    printf("\t-D, --dumpfile          Dump measurement responses to file\n");
    printf("\t-C, --showchanneldump   Display Channel dumps\n");
    printf("\t-i, --requestinterval   Sleep interval b/w requests in millisec. default 200 milli seconds\n");
    printf("\t-c, --rttcount          rttcount - Number rttcount to be used in measurement requests. default 50\n");
    printf("\t-r, --showrequestresp   Display Requests sent\n");
    printf("\t-o, --respoutputfmt     Display output in tabular format\n");
    printf("\t-R, --rate              Transmit rate. default 0x0b0b0b0b\n");
    printf("\t-p, --packettype        Packet Type. Null(0). Default QosNULL(1). TMR/RTT3 (2)\n");
    printf("\t-H, --hidestation       Hide station\n");
    printf("\t-m, --requesttype       Request type 0 - Type 0, 1 - Type 1 (Default)\n");
    printf("\t-t, --txchainmask       Tx chainmask. default(0). Whatever configured in AP will be the default.\n");
    printf("\t-k, --rxchainmask       Rx chainmask. default(0). Whatever configured in AP will be the default. \n");
    printf("\t-I, --interactive       Interactive\n");
    printf("\t-w, --wakeupstation     Wakeup Station\n");
    printf("\t-s, --synchronizeap     Synchronize AP\n");
    printf("\t-S, --Statusrequest     Status Request AP\n");
    printf("\t-P, --capabilityreq     Capability Request AP\n");
    printf("\t-W, --wpcdebugreq       WPC debug Request\n");
    printf("\t-v, --version           NBPServer version\n");
    printf("\t-n, --nreq              Number of mesaurement request to send, default is 1000 \n");
    printf("\t-f, --FAC               Need FAC values\n");
    printf("\t-h, --help              help\n\n\n");
    printf("\t-A, --asapmode          Asap mode (0 or 1)\n");
    printf("\t-l, --lcirequested      LCI Requested (0 or 1)\n");
    printf("\t-L, --lcrrequested      LCR Requested (0 or 1)\n");
    printf("\t-e, --reqpreamble       Request Preamble (0: Legacy, 2: HT, 3: VHT)\n");
    printf("\t-E, --reqreporttype     Request Report Type (0: FAC or 1: CFR)\n");
    printf("\t-b, --burstexponent     Burst exponent (default: 0)\n");
    printf("\t-B, --burstperiod       Burst period (default: 0)\n");
    printf("\t-u, --burstduration     Burst duration (default: 15)\n");
}



static struct option main_options[] = {
    { "analyzeresult",		0, 0, 'a' },
    { "showdebug",		0, 0, 'd' },
    { "showchanneldump",	0, 0, 'C' },
    { "requestinterval",	1, 0, 'i' },
    { "testduration",	1, 0, 'T' },
    { "showrequestresp",	0, 0, 'r' },
    { "respoutputfmt",	0, 0, 'o' },
    { "dumpfile",		1, 0, 'D' },
    { "rtt count",		1, 0, 'c' },
    { "interactive",	0, 0, 'I' },
    { "packettype",		1, 0, 'p' },
    { "requesttype",	1, 0, 'm' },
    { "rate",		1, 0, 'R' },
    { "txchainmask",	1, 0, 't' },
    { "rxchainmask",	1, 0, 'k' },
    { "generatedump",	1, 0, 'g' },
    { "wakeupstation",	0, 0, 'w' },
    { "synchronizeap",	0, 0, 's' },
    { "statusrequest",	0, 0, 'S' },
    { "capabilityreq",	0, 0, 'P' },
    { "wpcdebugreq",    0, 0, 'W' },
    { "version",        0, 0, 'v' },
    { "nreq",           1, 0, 'n' },
    { "help",     		0, 0, 'h' },
    { "FAC",            0, 0, 'f'},
    { "asap",            0, 0, 'A'},
    { "lci",            0, 0, 'l'},
    { "lcr",            0, 0, 'L'},
    { "reqpreamble",            0, 0, 'e'},
    { "reqreporttype",            0, 0, 'E'},
    { "burstexp",            0, 0, 'b'},
    { "burstper",            0, 0, 'B'},
    { "burstdur",            15, 0, 'u'},
    { 0, 0, 0, 0 }
};


int main(int argc, char *argv[])
{

    int mreqlen=0;
    int i = 0, x = 0, y = 0, j = 0;
    int distance[3] = { 0 };
    int result = 1;
    int wpc_dbg_mode;
    int opt = 0;
    struct nsp_mrqst mreq = { 0 };
    unsigned long wait_req=0;
    uint16_t mreqcnt = 0;
    int pkt_type = 1; //Default is 1 (QoS NULL)

    struct sigaction sa;

    struct itimerval timer= { { 60, 0}, {600,0} }, old_timer;

    pthread_t gui_thread, recv_thread;

    transmit_mode = 5;
    while((opt=getopt_long(argc, argv, "avc:g:T:drowhsSPD:WCi:t:k:IR:p:m:n:f:A:l:L:e:E:b:B:u", main_options, NULL)) != -1)
    {
        switch (opt) {

            case 'a':
                analyzeresult = 1;
                break;

            case 'd':
                showdebug = 1;
                break;

            case 'c':
                rtt_count = atoi(optarg);
                ftm_per_burst = rtt_count;
                break;

            case 'C':
                showchanneldump = 1;
                break;

            case 'i':
                requestinterval = atoi(optarg);
                break;

            case 'o':
                respoutputfmt = 1;
                break;
            case 'r':
                showrequestresp = 1;
                break;

            case 'D':
                dumpfile = fopen(optarg, "w");
                if ( !dumpfile )
                    printf("Couldnt open dumpfile %s\n",optarg);
                else
                    printf("\nDumping measurement responses to file %s\n",optarg);
                break;

            case 'I':
                interactive = 1;
                break;

            case 'p':
                pkt_type = atoi(optarg);
                transmit_mode = (transmit_mode & CLEAR_PKT_TYPE) | ((atoi(optarg) & 0x0007)<<2);
                break;

            case 't':
                transmit_mode = transmit_mode | ((atoi(optarg) & 0x0007)<<5);
                break;

            case 'k':
                transmit_mode = transmit_mode | ((atoi(optarg) & 0x0007)<<8);
                break;
            
            case 'm':
                transmit_mode = (transmit_mode & CLEAR_RQST_TYPE) | ((atoi(optarg) & 0x0003));
                break;

            case 'R':
                transmit_rate = strtoll(optarg, NULL, 16);
                break;
            case 'w':
                wakeup_station = 1;
                break;
            
            case 's':
                synchronize_ap = 1;
                break;
            case 'S':
                sta_req = 1;
                break;
            case 'P':
                cap_req = 1;
                break;

            case 'T':
                testduration_in_seconds = atoi(optarg);
                break;
            case 'W':
                wpc_debug_req = 1;
                break;

            case 'g':
                if (strlen(optarg) > 50) {
                    printf("File name text length must be below 50\n");
                    exit(1);
                }
                memcpy(generatereport, optarg, strlen(optarg));
                break;
            case 'v':
                printf("nbpserver version : %s\n",NBPSERVER_VERSION);
                exit(0);
            case 'n':
                nreq = atoi(optarg);
                printf("Nreq: %d \n", nreq);
                break;

            case 'f':
                transmit_mode = transmit_mode | 0x1000;
                break;

            case 'h':
                usage();
                exit(0);

            case 'l':
                lci_req = atoi(optarg);
                break;

            case 'L':
                loc_civ_req = atoi(optarg);
                break;

            case 'e':
                req_preamble = atoi(optarg);
                break;

            case 'E':
                req_report_type = atoi(optarg);
                break;

            case 'A':
                asap_mode = atoi(optarg);
                break;

            case 'b':
                burst_exp = atoi(optarg);
                break;

            case 'B':
                burst_period = atoi(optarg);
                break;

            case 'u':
                burst_dur = atoi(optarg);
                break;

            default:
                usage();
                exit(0);
        }
    }

    //rtt_count represents number of measurement responses to be recieved by initiator (RTT3)
    /* If RTT3 and burst_exp is present, update rtt_count = (ftm per burst*2^burst_exp) */
    if ((pkt_type == 2) && (burst_exp > 0)) {
         rtt_count = (rtt_count * (1<<burst_exp));
    }
    //FOR RTT3 i.e. pkt_type == 2, for asap 0 case, there's an extra ftm per burst
    if ((asap_mode == 0) && (pkt_type == 2)){
         rtt_count++;
    }

    printf("Measurement Request: Pkt_type%d, rtt_count:%d, total no of measurements:%d\n", pkt_type, rtt_count, nreq);
    printf("Measurement Request: req_preamble:%d, req_report_type:%d, asap_mode:%d, burst_exp:%d, burst_period:%d, burst_dur:%d\n", req_preamble, req_report_type, asap_mode, burst_exp, burst_period, burst_dur);
    if (testduration_in_seconds)
        timer.it_value.tv_sec = testduration_in_seconds ;


    memset(&sa, 0, sizeof(sa));

    sa.sa_flags = 0;
    sa.sa_handler = cleanup ;

    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGKILL, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGALRM, &sa, NULL);

    app_init();

    sleep(3);

    result = pthread_create ( &recv_thread, NULL, recv_responses_and_update_gui, (void *)NULL);
    if ( result )
        printf("Thread creation failed\n");

    time(&starttime);
    if(testduration_in_seconds)
        setitimer(ITIMER_REAL , &timer, &old_timer);
    if(cap_req) {
        printf("Sending Capability Request to AP \n");
        for(i=0; i<no_of_aps; i++) {
            send_cap_req(&aps[i]);
        }
    } 
    if(sta_req) {
        printf("Sending Status Request to AP \n");
        for(i=0; i<no_of_aps; i++) {
            send_sreq(&aps[i]);
        }
    }
    if(wpc_debug_req) {
        printf("Sending WPC debug Request to AP START/STOP(1-START/0-STOP)\n");
        scanf("%d",&wpc_dbg_mode);
        for(i=0; i<no_of_aps; i++) {
            send_wpcdbgreq(&aps[i], wpc_dbg_mode);
        }
    }

    if(synchronize_ap) {
        printf("Sending TSF Request to Associated AP to get TSF\n");
        for (j = 0;j < no_of_aps ; j++)
            for(i=0; i<no_of_stations; i++) {
		if( memcmp(&aps[j].sockaddr.sin_addr, &station[i].ap_ipaddr.sin_addr, 4) != 0 )
		{
	                send_tsf_req(&aps[j], &station[i].ap_mac, station[i].station_channel, &station[i].ssid);
                    // Wait to keep the probe request going in different time. 
                    // This becomes significant when we have 2 APs at different Channel 
                    // and both try to do TSF measurment at the same time. 
                    usleep(1000*1000);
                    
		}
            }
        transmit_mode = transmit_mode | SYNCHRONIZE;
        // Wait time to recieve the response from the APs. 
        usleep(500*1000);
    } 

    if(wakeup_station) {
        printf("Sending Request to Associated AP to keep the station awake\n");
        for(i=0; i<no_of_stations; i++) { 
            if (synchronize_ap) {
                if (station[i].result != 0) {
                    station[i].result = 1;
                    printf("\n AP's are not synchronized properly");
                    continue;
                } 
            } 
            station[i].result = 1;
            send_wakeup_req(&aps[station[i].associated_ap], &station[i].station_mac, station[i].wakeup_interval);
            // We need sleep is wait for getting sleep response back from AP
            usleep(MIN_REQ_WAIT_PERIOD * 10000);
        }
    } 

        if(interactive)  {
            printf("\nEnter the rtt_count to be used to send req : ");
            scanf("%hd",&rtt_count);
        }
        //Wait time after each burst in ms, used only for RTT3 where burst_exp > 0
        //Asuumption: Each ftm takes around 10ms + total number of burst * burst period
        burst_wait_ms = (ftm_per_burst *100) + ((1 << burst_exp) * burst_period * 100);

        /* Sending nreq number of measurement requests, with DEFAULT_WAIT_BETWEEN_MRQSTS_IN_MS pause between each request */
        while(mreqcnt < nreq) {
        for( j=0; j<no_of_stations; j++) {

            for (i=0; i<no_of_aps; i++) {
                if(wakeup_station) {
                    if(!station[j].result) {
                        send_req(&aps[i], &station[j].station_mac,&station[j].ap_mac,station[j].station_channel,station[j].station_bandwidth);
                        station[j].request_count += rtt_count;
                        station[j].request_count_to_ap[i] += rtt_count;
                        aps[i].request_count += rtt_count;
                    }
                }
                else {
                    send_req(&aps[i], &station[j].station_mac,&station[j].ap_mac,station[j].station_channel,station[j].station_bandwidth);
                    station[j].request_count += rtt_count;
                    station[j].request_count_to_ap[i] += rtt_count;
                    aps[i].request_count += rtt_count;
                }
            }
        }
        mreqcnt++;
        /* For RTT3, where burst exponent > 0, wait for all bursts to finish before sending new request */
        if ((pkt_type == 2) && (burst_exp > 0)) {
           burst_wait_ms = burst_wait_ms + DEFAULT_WAIT_BETWEEN_MRQSTS_IN_MS;
           usleep (burst_wait_ms * 1000);
        }
        else {
           usleep(DEFAULT_WAIT_BETWEEN_MRQSTS_IN_MS*1000);
        }
    }
    printf("Measurement completed. No of Measurements: %d, Each Measurement had (ftms: %d, burst_exp: %d}\n", mreqcnt, ftm_per_burst, burst_exp);
}



