/*
 * Copyright (c) 2011 Qualcomm Atheros, Inc..
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/tcp.h>
#include<ap.h>
#include<stdio.h>
#include<ieee80211_hlsmgr.h>
#include<bdaddr.h>
#include<err.h>
#include<stdlib.h>
#include<string.h>
#include<common.h>
#include<endian.h>

int  inc_station_response_count(bdaddr_t *staaddr)
{
    int i = 0 ;
    for ( i = 0; i < no_of_stations ; i++)
    {
        if( bacmp(&station[i].station_mac, staaddr) == 0)
        {
            station[i].response_count++;
            return i;
        }
    }
    return -1;
}

struct ap* find_ap_by_sockfd(int sockfd)
{
    int i = 0;

    if(sockfd<=0)
    {
        printf("%s Invalid sockfd\n",__FUNCTION__);
        return NULL;
    }

    for( i=0; i< (no_of_aps+new_ap); i++ )
    {
        if(aps[i].sockfd == sockfd)
            return &aps[i];
    }

    return NULL;
}

int  find_maxfd()
{
    int bigfd = aps[0].sockfd;
    int i = 0;

    for( i=0; i < (no_of_aps + new_ap); i++ )
    {
        if(bigfd < aps[i].sockfd )
            bigfd = aps[i].sockfd;
    }
    return bigfd;
}
void print_error_msg(struct nsp_error_msg *errmsg)
{
    printf("\n----------------NBP SERVER ERROR_MESSAGE-----------------");
    printf("\nRequest ID            : %x",errmsg->request_id);
    switch(ntohs(errmsg->errorid))
    {
        case NSP_UNSUPPORTED_PROTOCOL_VERSION:
            printf("\nError ID              : Unsupported  Protocol Version");
            break;
        case NSP_INCORRECT_MESSAGE_LENGTH:
            printf("\nError ID              : Invalid Message Length");
            break;
        default:
            printf("\nError ID              : Unknown Error ID");
            break;
    }
}

void print_nsp_header(struct nsp_header *nsp_hdr)
{

    if ( respoutputfmt == 0 ) {
        printf("\n----------------NBP_SERVER NSP_HEADER-----------------");
        printf("\nStart of frame Delimiter :%x",ntohs(nsp_hdr->SFD));
        printf("\nVersion                  :%x",nsp_hdr->version);
        printf("\nFrame Type               :%x",nsp_hdr->frame_type);
        printf("\nFrame Length             :%x",ntohs(nsp_hdr->frame_length));
    }
}

#define PKT_TYPE_MASK 0x001C
#define TX_CHAIN_MASK 0x0060
#define RX_CHAIN_MASK 0x0180

void print_rtt_mreq(struct nsp_mrqst *mreq)
{
    unsigned char *s = (char *)mreq->sta_mac_addr;
    if ( respoutputfmt == 0 ) {
        printf("\n----------------NBP SERVER RTT_MEASUREMENT_REQUEST-----------------");
        printf("\nRequest ID            : %x",mreq->request_id);
        printf("\nSTA MAC addr          : %x:%x:%x:%x:%x:%x", s[0],s[1],s[2],s[3],s[4],s[5]);
        s = (unsigned char *)mreq->spoof_mac_addr;
        printf("\nSPOOF MAC addr        : %x:%x:%x:%x:%x:%x", s[0],s[1],s[2],s[3],s[4],s[5]);
        printf("\nChannel               : %1x",mreq->channel);
        printf("\nMode                  : %x",ntohs(mreq->mode));
        printf("\nPacket Type           : %x",(ntohs(mreq->mode) & PKT_TYPE_MASK)>>2);
        printf("\nTxChainmask           : %x",(ntohs(mreq->mode) & TX_CHAIN_MASK)>>5);
        printf("\nRxChainmask           : %x",(ntohs(mreq->mode) & RX_CHAIN_MASK)>>7);
        printf("\nSta Info              : %x",ntohs(mreq->sta_info));
        printf("\nTransmit Rate         : %x",ntohl(mreq->transmit_rate));
        printf("\nNo. of Msrmts         : %x",mreq->no_of_measurements);
        printf("\nTimeout               : %x",mreq->timeout);
    } else {
        s = (char *)mreq->sta_mac_addr;
        printf("\n<<< Req ID : %x No. of Measurements : %x Station_mac : %x:%x:%x:%x:%x:%x  Channel : %d\n",mreq->request_id, ntohs(mreq->no_of_measurements), s[0],s[1],s[2],s[3],s[4],s[5], mreq->channel);
        printf("Request time");
        get_time_string(1);
    }
}


void print_rtt_mres(struct nsp_mresp *mres)
{
    unsigned char *s = (char *)mres->sta_mac_addr;

    if ( respoutputfmt == 0 ) {
        printf("\n-----------------NBP SERVER MEASUREMENT_RESPONSE-----------------");
        printf("\nRequest ID            : %x",mres->request_id);
        printf("\nMAC addr              : %x:%x:%x:%x:%x:%x", s[0],s[1],s[2],s[3],s[4],s[5]);
        printf("\nNo. of Rsps           : %d",ntohs(mres->no_of_responses));
        printf("\nRequest timestamp     : %llx",be64toh(mres->req_tstamp));
        printf("\nResponse timestamp    : %llx",be64toh(mres->resp_tstamp));
        printf("\nDiff                  :%llx",be64toh(mres->resp_tstamp) - be64toh(mres->req_tstamp));
        printf("\nResult                : %d",ntohs(mres->result));
    } else {
        printf("\n>>> REQTSMP : %08llX    RESTSMP : %08llX     DIFF : %08llX ", be64toh(mres->req_tstamp), be64toh(mres->resp_tstamp),be64toh(mres->resp_tstamp) - be64toh(mres->req_tstamp));
    }
}

void print_type1_res(struct nsp_type1_resp *type1esp)
{
    unsigned char *s=NULL;	
    static unsigned long resp=0;
    int i=0;
    u_int8_t numRxChains = ieee80211_mask2numchains[
                        (type1esp->no_of_chains & (RX_CHAINMASK_HASH >>8))];
    resp++;

    if ( respoutputfmt == 0 ) {
        printf("\nTOA                   : %llx",(type1esp->toa));
        printf("\nTOD                   : %llx",(type1esp->tod));
        printf("\nRTT                   : %llx",(type1esp->toa)-(type1esp->tod));
        for(i = 0; i < numRxChains; i++)
            printf("\nRSSI %d                : %x",i, type1esp->rssi[i]);
        printf("\nSend Rate             : %x",(type1esp->send_rate));
        printf("\nRecv Rate             : %x",(type1esp->receive_rate));
        printf("\nNo of retries         : %x",type1esp->no_of_retries);
        printf("\nNo of chains          : %x",type1esp->no_of_chains);
        printf("\nPayload info          : %x",type1esp->payload_info);
        s = (unsigned char *)type1esp->type1_payld;
        printf("\nChannel dump          :%x:%x:%x:%x:%x:%x\n", s[0],s[1],s[2],s[3],s[4],s[5]);
    } else {
        printf("\n>>> TOA : %llx    TOD : %llx     RTT : %llx resp : %lu\n",htobe64(type1esp->toa), htobe64(type1esp->tod), htobe64(type1esp->toa) - htobe64(type1esp->tod), resp);
    }
}

void print_type0_res(struct nsp_type0_resp *type0resp)
{
    static unsigned long resp=0;
    int i=0;
    resp++;
    unsigned char *s = (char *)type0resp->sta_mac_addr;

    if ( respoutputfmt == 0 ) {
        printf("\n----------------NBP SERVER MEASUREMENT_RESPONSE-----------------");
        printf("\nRequest ID            : %x",type0resp->request_id);
        printf("\nMAC addr              : %x:%x:%x:%x:%x:%x", s[0],s[1],s[2],s[3],s[4],s[5]);
        printf("\nResult                : %d",ntohl(type0resp->result));
        printf("\nRTT Average           : %d",ntohl(type0resp->rtt_mean));
        printf("\nRTT Stdev           : %d",ntohl(type0resp->rtt_stddev));
        printf("\nRTT Samples           : %d",ntohl(type0resp->rtt_samples));
        printf("\nRSSI Average          : %d",ntohl(type0resp->rssi_mean));
        printf("\nRSSI Stdev            : %d",ntohl(type0resp->rssi_stddev));
        printf("\nRSSI Samples          : %d",ntohl(type0resp->rssi_samples));
        printf("\n");
    } else {
        printf("\n>>> ReqId: %x  MAC:%x:%x:%x:%x:%x:%x  RTT Avg : %d    RTT Stddev : %x     RTT Samples: %x RSSI Avg : %x    RSSI Stddev : %x     RSSI Samples: %x \n" , type0resp->request_id,s[0],s[1],s[2],s[3],s[4],s[5],ntohl(type0resp->rtt_mean), ntohl(type0resp->rtt_stddev), ntohl(type0resp->rtt_samples), ntohl(type0resp->rssi_mean),ntohl(type0resp->rssi_stddev), ntohl(type0resp->rssi_samples));
    }
}


void print_sleep_req(struct nsp_sleep_req *sleep_req)
{
    unsigned char *s = (char *)&sleep_req->sta_mac_addr;
    printf("\n----------------NBP SERVER SLEEP_REQUEST-----------------");
    printf("\nRequest ID            : %x",sleep_req->request_id);
    printf("\nMAC addr              : %x:%x:%x:%x:%x:%x\n\n", s[0],s[1],s[2],s[3],s[4],s[5]);
}

void print_sleep_resp(struct nsp_sleep_resp *sleep_resp)
{
    unsigned char *s = (char *)&sleep_resp->sta_mac_addr;
    printf("\n----------------NBP SERVER SLEEP_RESPONSE-----------------");
    printf("\nRequest ID            : %x",sleep_resp->request_id);
    printf("\nMAC addr              : %x:%x:%x:%x:%x:%x", s[0],s[1],s[2],s[3],s[4],s[5]);
#if NSP_AP_SW_VERSION > 0x01
    printf("\n Num of KA            : %x", ntohl(sleep_resp->num_ka_frm)); 
#endif 
    printf("\nResponse              : %x\n\n",sleep_resp->result);
}

void print_wakeup_req(struct nsp_wakeup_req *wakeup_req)
{
    unsigned char *s = (char *)&wakeup_req->sta_mac_addr;
    printf("\n----------------NBP SERVER WAKEUP_REQUEST-----------------");
    printf("\nRequest ID            : %x",wakeup_req->request_id);
    printf("\nMAC addr              : %x:%x:%x:%x:%x:%x", s[0],s[1],s[2],s[3],s[4],s[5]);
    printf("\nWakeup interval       : %x",wakeup_req->wakeup_interval);
}

void print_wakeup_resp(struct nsp_wakeup_resp *wakeup_resp)
{
    unsigned char *s = (char *) (&wakeup_resp->sta_mac_addr);
    printf("\n----------------NBP SERVER WAKEUP_RESPONSE-----------------");
    printf("\nRequest ID            : %x",wakeup_resp->request_id);
    printf("\nMAC addr              : %x:%x:%x:%x:%x:%x", s[0],s[1],s[2],s[3],s[4],s[5]);
    printf("\nResponse              : %x",wakeup_resp->result);
}
void print_tsf_req(struct nsp_tsf_req *tsf_req)
{
    unsigned char *s = (char *)&tsf_req->ap_mac_addr;
    printf("\n----------------NBP SERVER TSF_REQUEST-----------------");
    printf("\nRequest ID            : %x",tsf_req->request_id);
    printf("\nMAC addr              : %x:%x:%x:%x:%x:%x", s[0],s[1],s[2],s[3],s[4],s[5]);
    printf("\nSSID                  : %s",tsf_req->ssid);
    printf("\nRequset Channel       : %x\n\n",tsf_req->channel);
}

void print_cap_req(struct nsp_cap_req *creq)
{
    printf("\n----------------NBP SERVER CAPABILITY_REQUEST-----------------");
    printf("\nRequest ID            : %x",creq->request_id);
}

void print_wpcdbgresp(struct nsp_wpc_dbg_resp *wpcdbgres)
{               
    printf("----------------WPC DEBUG_RESPONSE-----------------");
    printf("Request ID            :%x",wpcdbgres->request_id);
    printf("Result                :%x",wpcdbgres->result);
}  

void print_cap_resp(struct nsp_cap_resp *cresp)
{
    printf("\n----------------NBP SERVER CAPABILITY_RESPONSE-----------------");
    printf("\nRequest ID            :%x",cresp->request_id);
    printf("\nBand                  :%x",cresp->band);
    printf("\nPositioning           :%x",cresp->positioning);
    printf("\nSW Version            :%x",ntohs(cresp->sw_version));
    printf("\nHW Version            :%x",ntohs(cresp->hw_version));
    printf("\nClock Frequency       :%d",ntohl(cresp->clk_freq));
}
void print_sresp(struct nsp_sresp *sresp)
{
    unsigned char *s;
    int i=0;
    struct nsp_vap_info *vapinfo= (((char *)sresp) + SRES_LEN);
    struct nsp_station_info *stainfo = (((char *)sresp) + SRES_LEN + ntohs(sresp->no_of_vaps)* VAPINFO_LEN);
    printf("\n----------------NBP SERVER STATUS_RESPONSE-----------------");
    printf("\nRequest ID            :%x",sresp->request_id);
    printf("\nNo of VAPS        :%x",ntohs(sresp->no_of_vaps));
    printf("\nNo of stations        :%x",ntohs(sresp->no_of_stations));
    printf("\nRequest timestamp     :%llx",be64toh(sresp->req_tstamp));
    printf("\nResponse timestamp    :%llx",be64toh(sresp->resp_tstamp));


    for ( i = 0 ; i < ntohs(sresp->no_of_vaps) ; i++)
        print_vap_info(vapinfo+i);

    for ( i = 0 ; i < ntohs(sresp->no_of_stations) ; i++)
        print_sta_info(stainfo+i);

/*    s = (unsigned char *)sresp->ap1_mac;
    printf("\nAP1 Mac address       :%x %x %x %x %x %x",s[0],s[1],s[2],s[3],s[4],s[5]);
    printf("\nAP1 channel           :%x",sresp->ap1_channel);
    s = (unsigned char *)sresp->ap2_mac;
    printf("\nAP2 Mac address       :%x %x %x %x %x %x",s[0],s[1],s[2],s[3],s[4],s[5]);
    printf("\nAP2 channel           :%x",sresp->ap2_channel);
    printf("\nSSID                  :%s",sresp->ssid); */
}

void print_vap_info (struct nsp_vap_info *vapinfo)
{
    unsigned char *s = (unsigned char *)vapinfo->vap_mac;
    printf("\n----------------NBP SERVER VAP_INFO-----------------");
    printf("\nVAP Mac address       :%x %x %x %x %x %x",s[0],s[1],s[2],s[3],s[4],s[5]);
    printf("\nVAP channel           :%x",vapinfo->vap_channel);
    printf("\nVAP SSID              :%s",vapinfo->vap_ssid); 

}
void print_sta_info(struct nsp_station_info *sta_info) 
{
    unsigned char *s;

    printf("\n----------------NBP SERVER STA_INFO-----------------");
    s = (unsigned char *)sta_info->sta_mac_addr;
    printf("\nSTA Mac address    :%x %x %x %x %x %x",s[0],s[1],s[2],s[3],s[4],s[5]);
    printf("\nVAP Index        :%x",sta_info->vap_index);
    printf("\nSTA last timestamp :%llx",be64toh(sta_info->tstamp));
    printf("\nStation info       :%x",sta_info->info);
    printf("\nStation RSSI       :%x",sta_info->rssi);

}
void print_wpcdbgreq(struct nsp_wpc_dbg_req *wpcdbgreq)
{
    printf("\n----------------NBP SERVER STATUS_REQUEST-----------------");
    printf("\nRequest ID            : %x",wpcdbgreq->request_id);
    printf("\nDebug Mode            : %x",wpcdbgreq->dbg_mode);
}
void print_sreq(struct nsp_sreq *sreq)
{
    printf("\n----------------NBP SERVER STATUS_REQUEST-----------------");
    printf("\nRequest ID            : %x",sreq->request_id);
}
void print_tsf_resp(struct nsp_tsf_resp *tsf_resp)
{
    unsigned char *s = &tsf_resp->ap_mac_addr;
    printf("\n----------------NBP SERVER TSF_RESPONSE-----------------");
    printf("\nRequest ID            : %x",tsf_resp->request_id);
    printf("\nMAC addr              : %x:%x:%x:%x:%x:%x", s[0],s[1],s[2],s[3],s[4],s[5]);
    printf("\nAssociated AP's TSF   : %llx",tsf_resp->assoc_tsf);
    printf("\nProbing AP's TSF      : %llx",tsf_resp->prob_tsf);
    printf("\nResult                : %x\n\n",tsf_resp->result);
}

int connect_with_aps()
{
    int i = 0;
    int socket_count = 0;
    char buff[18];
    int flag = 1;
    int rcvlowat = WIFIPOS_TYPE1_RES_SIZE; //size of single mresponse is 426
    int rcvbuff = WIFIPOS_TYPE1_RES_SIZE * 80;

    FD_ZERO(&readset);

    for( i=0; i<(no_of_aps + new_ap); i++ )
    {
        aps[i].sockfd = socket(AF_INET, SOCK_STREAM, 0);
        aps[i].state = NOT_CONNECTED;
        aps[i].sockaddr.sin_family = AF_INET;
        aps[i].sockaddr.sin_port = htons(DST_PORT);

        if( setsockopt(aps[i].sockfd, SOL_SOCKET, SO_RCVBUF, &rcvbuff, sizeof(rcvlowat))  < 0 )	{
            printf("Couldnt increase the rcvbuf size\n");
        }

        if ( (connect(aps[i].sockfd,(struct sockaddr *) &aps[i].sockaddr, sizeof(struct sockaddr_in)) < 0))
        {
            printf("Couldnt connect with AP [%s]\n",inet_ntop(AF_INET, &aps[i].sockaddr.sin_addr,buff,18));
            aps[i].sockfd = -1 ;
            aps[i].state = NOT_CONNECTED ;
        }
        else {
            socket_count++;
            aps[i].state = CONNECTED;
            aps[i].response_count = 0;

            FD_SET(aps[i].sockfd , &readset);

            if(maxfd < aps[i].sockfd)
                maxfd = aps[i].sockfd;

            printf("Connected  with AP socketfd [%d] [%s]\n",aps[i].sockfd, inet_ntop(AF_INET, &aps[i].sockaddr.sin_addr,buff,18));
        }

    }
    return socket_count;
}


int close_connections()
{
    int i=0;
    for( i=0 ; i<no_of_aps ; i++ )
    {
        if(aps[i].sockfd>0)
        {
            close(aps[i].sockfd);
            aps[i].sockfd = -1;
        }
    }
    return 0;
}

int prepare_sleep_req(struct req *req, bdaddr_t *station_mac)
{
    req->nsphdr.SFD = htons(START_OF_FRAME);
    req->nsphdr.version = NSP_VERSION;
    req->nsphdr.frame_type = NSP_STARETURNTOSLEEPREQ;
    req->nsphdr.frame_length = SLEEPREQ_LEN;
    req->sleepreq.request_id = req->mreq.request_id+1;
    if(station_mac == 0)
        memset(req->sleepreq.sta_mac_addr, 0, ETH_ALEN);
    else
        memcpy(req->sleepreq.sta_mac_addr, station_mac, ETH_ALEN);
    return 0;
}

int prepare_wakeup_req(struct req *req, bdaddr_t *station_mac, int wakeup_interval)
{
    req->nsphdr.SFD = htons(START_OF_FRAME);
    req->nsphdr.version = NSP_VERSION;
    req->nsphdr.frame_type = NSP_WAKEUPRQST;
    req->nsphdr.frame_length = WAKEUPREQ_LEN;
    req->wakereq.request_id = req->mreq.request_id+1;
    memcpy(req->wakereq.sta_mac_addr, station_mac, ETH_ALEN);
    req->wakereq.wakeup_interval = wakeup_interval;
    req->wakereq.mode = 0x6;
    return 0;
}

int prepare_cap_req(struct req *req)
{
    req->nsphdr.SFD = htons(START_OF_FRAME);
    req->nsphdr.version = NSP_VERSION;
    req->nsphdr.frame_type = NSP_CRQST;
    req->nsphdr.frame_length = CAPREQ_LEN;
    req->tsfreq.request_id = req->mreq.request_id+1;
    return 0;
}

int prepare_wpcdbgreq(struct req *req, int dbg_mode)
{
    req->nsphdr.SFD = htons(START_OF_FRAME);
    req->nsphdr.version = NSP_VERSION;
    req->nsphdr.frame_type = NSP_WPCDBGRGST;
    req->nsphdr.frame_length = WPCDBGREQ_LEN;
    req->wpcdbgreq.request_id = req->mreq.request_id+1;
    req->wpcdbgreq.dbg_mode = dbg_mode;
    return 0;
}

int prepare_sreq(struct req *req)
{
    req->nsphdr.SFD = htons(START_OF_FRAME);
    req->nsphdr.version = NSP_VERSION;
    req->nsphdr.frame_type = NSP_SRQST;
    req->nsphdr.frame_length = SREQ_LEN;
    req->tsfreq.request_id = req->mreq.request_id+1;
    return 0;
}

int prepare_tsf_req(struct req *req, bdaddr_t *ap_mac, unsigned int channel, char *ssid)
{
    req->nsphdr.SFD = htons(START_OF_FRAME);
    req->nsphdr.version = NSP_VERSION;
    req->nsphdr.frame_type = NSP_TSFRQST;
    req->nsphdr.frame_length = TSFREQ_LEN;
    req->tsfreq.request_id = req->mreq.request_id+1;
    memcpy(req->tsfreq.ap_mac_addr, ap_mac, ETH_ALEN);
    memcpy(req->tsfreq.ssid, ssid, strlen(ssid));
    req->tsfreq.channel = channel;
    req->tsfreq.timeout = 250;
    return 0;
}

int prepare_mreq(struct req *req, bdaddr_t *station_mac, bdaddr_t *spoof_mac, int channel, int bandwidth)
{
    uint16_t burst_timeout = burst_wait_ms/10;
    req->nsphdr.SFD = htons(START_OF_FRAME);
    req->nsphdr.version = NSP_VERSION;
    req->nsphdr.frame_type = NSP_MRQST;
    req->nsphdr.frame_length = MREQ_LEN;
    req->mreq.request_id = req->mreq.request_id+1;
    memcpy(&req->mreq.sta_mac_addr, station_mac, ETH_ALEN);
    memcpy(&req->mreq.spoof_mac_addr,  spoof_mac,  ETH_ALEN);
    req->mreq.channel = channel;
    req->mreq.mode = htons(transmit_mode);
    req->mreq.sta_info = htonl(0x0000);
    req->mreq.transmit_rate = htonl(transmit_rate);
    req->mreq.no_of_measurements = rtt_count;
    req->mreq.timeout = htons(150);
    if(burst_exp > 0){
        req->mreq.timeout = htons(burst_timeout);
    }
    req->mreq.bandwidth = (u_int8_t) bandwidth;
    req->mreq.lci_req = lci_req;
    req->mreq.loc_civ_req = loc_civ_req;
    req->mreq.req_report_type = req_report_type;
    req->mreq.req_preamble = req_preamble;
    req->mreq.asap_mode = asap_mode;
    req->mreq.burst_exp = burst_exp;
    req->mreq.burst_period = htons(burst_period);
    req->mreq.burst_dur = htons(burst_dur);
    req->mreq.ftm_per_burst = htons(ftm_per_burst);
    return 0;
}

int send_sleep_req(struct ap *ap, bdaddr_t *station_mac)
{
    if( ap->state == NOT_CONNECTED )
        return -1;
    prepare_sleep_req(&ap->req, station_mac);

    if ( showrequestresp )
        print_sleep_req((struct nsp_sleep_req *)&ap->req.sleepreq);
    if(ap->sockfd > 0)
        write(ap->sockfd,&ap->req, NSP_HDR_LEN + SLEEPREQ_LEN);

    return 0;
}

int send_wakeup_req(struct ap *ap, bdaddr_t *station_mac, int wakeup_interval)
{
    if( ap->state == NOT_CONNECTED )
        return -1;
    prepare_wakeup_req(&ap->req, station_mac, wakeup_interval);
    if ( showrequestresp )
        print_wakeup_req((struct nsp_wakeup_req *)&ap->req.wakereq);
    if(ap->sockfd > 0)
        write(ap->sockfd,&ap->req, NSP_HDR_LEN + WAKEUPREQ_LEN);
    return 0;
}
     
int send_tsf_req(struct ap *ap, bdaddr_t *ap_mac, unsigned int channel, char *ssid)
{
    if( ap->state == NOT_CONNECTED )
        return -1;
    prepare_tsf_req(&ap->req, ap_mac, channel, ssid);
    if ( showrequestresp )
        print_tsf_req((struct nsp_mrqst *)&ap->req.tsfreq);

    if(ap->sockfd > 0)
        write(ap->sockfd,&ap->req, NSP_HDR_LEN + TSFREQ_LEN);

    return 0;
}

int send_cap_req(struct ap *ap)
{
    if( ap->state == NOT_CONNECTED )
        return -1;
    prepare_cap_req(&ap->req);
    if ( showrequestresp )
        print_cap_req((struct nsp_mrqst *)&ap->req.capreq);

    if(ap->sockfd > 0)
        write(ap->sockfd,&ap->req, NSP_HDR_LEN + CAPREQ_LEN);

    return 0;
}

int send_wpcdbgreq(struct ap *ap, int dbg_mode)
{
    if( ap->state == NOT_CONNECTED )
        return -1;
    prepare_wpcdbgreq(&ap->req, dbg_mode);
    if ( showrequestresp )
        print_wpcdbgreq((struct nsp_mrqst *)&ap->req.wpcdbgreq);

    if(ap->sockfd > 0)
        write(ap->sockfd,&ap->req, NSP_HDR_LEN + WPCDBGREQ_LEN);

    return 0;
}

int send_sreq(struct ap *ap)
{
    if( ap->state == NOT_CONNECTED )
        return -1;
    prepare_sreq(&ap->req);
    if ( showrequestresp )
        print_sreq((struct nsp_mrqst *)&ap->req.sreq);

    if(ap->sockfd > 0)
        write(ap->sockfd,&ap->req, NSP_HDR_LEN + SREQ_LEN);

    return 0;
}

int send_req(struct ap *ap, bdaddr_t *station_mac,bdaddr_t *ap_mac, int channel, int bandwidth)
{
    int i = 0, readlen = 0;

    if( ap->state == NOT_CONNECTED )
        return -1;
    //	if(ap->state == REQUEST_SENT )
    //		return -1;

    prepare_mreq(&ap->req, station_mac, ap_mac, channel, bandwidth);

    if ( showrequestresp )
        print_rtt_mreq((struct nsp_mrqst *)&ap->req.mreq);

    if(ap->sockfd > 0)
        write(ap->sockfd,&ap->req, NSP_HDR_LEN + MREQ_LEN);

    ap->state = REQUEST_SENT;

    return 0;	
}
