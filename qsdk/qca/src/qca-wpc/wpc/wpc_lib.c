/*
 * Copyright (c) 2011 Qualcomm Atheros, Inc..
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include <sys/types.h>
#include<sys/socket.h>
#include<ieee80211_wpc.h>
#include<linux/netlink.h>
#include<netinet/in.h>
#include<stdlib.h>
#include<string.h>
#include<stdio.h>
#include<unistd.h>

#define PRIN_LOG(format, args...) printf(format "\n", ## args)

#define NUM_OFDM_TONES_ACK_FRAME 52
#define RESOLUTION_IN_BITS       10
#define CDUMP_SIZE(rxchain) (NUM_OFDM_TONES_ACK_FRAME * 2 * (rxchain) * RESOLUTION_IN_BITS) / 8

void print_vap_info (struct nsp_vap_info *vapinfo);
void print_sta_info(struct nsp_station_info *sta_info);
int wpc_sendmsg(int wpc_driver_sock, struct msghdr *msg, int flags);

int wpc_accept(int sockfd,struct sockaddr *addr, int *addrlen )
{
    return accept( sockfd, addr, (socklen_t *) addrlen);
}

int wpc_recv(int wpc_driver_sock, char *buffer, int recv_len, int flags)
{
    return recv(wpc_driver_sock, buffer, recv_len, flags);
}

/*
int wpc_recv(int wpc_serverfd,u_int8_t *buf, int len, int flags)
{
    return recv(wpc_serverfd,buf,len,0);
} 
*/

int wpc_write(int wpc_serverfd, char *buffer, int recv_len)
{
    /* Handle recv_len 0 condition */
    if (recv_len > 0)
    {
        return write(wpc_serverfd, buffer, recv_len);
    } else {
        return 0;
    }
}

void wpc_nlsendmsg(int wpc_driver_sock, int nbytes, char *buf, struct nlmsghdr *nlh, struct iovec *iov, struct msghdr *msg)
{
    int sendmsglen = 0;
    iov->iov_len = nlh->nlmsg_len;
    memcpy(NLMSG_DATA(nlh), buf, nbytes);
    sendmsglen = wpc_sendmsg(wpc_driver_sock, msg, 0);
    if ( sendmsglen < 0 )
        printf("Sendmsg Error\n");
}

int wpc_sendmsg(int wpc_driver_sock, struct msghdr *msg, int flags)
{
    return sendmsg(wpc_driver_sock, msg, flags);
}

int wpc_close(int sockfd)
{
    return close(sockfd);
}
void print_wpcdbgrqst(struct nsp_wpc_dbg_req *wpcdbgrqst)
{
    PRIN_LOG("----------------WPC DEBUG_REQUEST-----------------");
    PRIN_LOG("Request ID            :%x",wpcdbgrqst->request_id);
    PRIN_LOG("DEBUG MODE            :%x",wpcdbgrqst->dbg_mode);
}

void print_wpcdbgresp(struct nsp_wpc_dbg_resp *wpcdbgres)
{
    PRIN_LOG("----------------WPC DEBUG_RESPONSE-----------------");
    PRIN_LOG("Request ID            :%x",wpcdbgres->request_id);
    PRIN_LOG("Result                :%x",wpcdbgres->result);
}

void print_slrqst(struct nsp_sleep_req *slrqst)
{
    const char *s = (char *)slrqst->sta_mac_addr;
    PRIN_LOG("----------------WPC SLEEP_REQUEST-----------------");
    PRIN_LOG("Request ID            :%x",slrqst->request_id);
    PRIN_LOG("STA MAC addr          :%x:%x:%x:%x:%x:%x", s[0],s[1],s[2],s[3],s[4],s[5]);
}

void print_slresp(struct nsp_sleep_resp *slresp)
{
    
    const char *s = (char *)slresp->sta_mac_addr;
    PRIN_LOG("----------------WPC SLEEP_RESPONSE-----------------");
    PRIN_LOG("Request ID            :%x",slresp->request_id);
    PRIN_LOG("STA MAC addr          :%x:%x:%x:%x:%x:%x", s[0],s[1],s[2],s[3],s[4],s[5]);
    PRIN_LOG("KA Frames             :%x",slresp->num_ka_frm);
    PRIN_LOG("Result                :%x",slresp->result);
}

void print_wrqst(struct nsp_wakeup_req *wrqst)
{
    const char *s = (char *)wrqst->sta_mac_addr;
    PRIN_LOG("----------------WPC WAKEUP_REQUEST-----------------");
    PRIN_LOG("Request ID            :%x",wrqst->request_id);
    PRIN_LOG("STA MAC addr          :%x:%x:%x:%x:%x:%x", s[0],s[1],s[2],s[3],s[4],s[5]);
    PRIN_LOG("Wakeup interval       :%x",wrqst->wakeup_interval);
    PRIN_LOG("Wakeup mode           :%x",wrqst->mode);
}

void print_wresp(struct nsp_wakeup_resp *wresp)
{
    
    const char *s = (char *)wresp->sta_mac_addr;
    PRIN_LOG("----------------WPC WAKEUP_RESPONSE-----------------");
    PRIN_LOG("Request ID            :%x",wresp->request_id);
    PRIN_LOG("STA MAC addr          :%x:%x:%x:%x:%x:%x", s[0],s[1],s[2],s[3],s[4],s[5]);
    PRIN_LOG("Result                :%x",wresp->result);
}

void print_crqst(struct nsp_cap_req *crqst)
{
    PRIN_LOG("----------------WPC CAPABILITY_REQUEST-----------------");
    PRIN_LOG("Request ID            :%x",crqst->request_id);
}

void print_tsfrqst(struct nsp_tsf_req *tsfrqst)
{
    const char *s = (char *)tsfrqst->ap_mac_addr;
    PRIN_LOG("----------------WPC TSF_REQUEST-----------------");
    PRIN_LOG("Request ID            :%x",tsfrqst->request_id);
    PRIN_LOG("AP MAC addr           :%x:%x:%x:%x:%x:%x", s[0],s[1],s[2],s[3],s[4],s[5]);
    PRIN_LOG("SSID                  :%s",tsfrqst->ssid);
    PRIN_LOG("Request channel       :%x",tsfrqst->channel);
    PRIN_LOG("Request timeout       :%x",tsfrqst->timeout);
}


void print_mrqst(struct nsp_mrqst *mrqst)
{
    const char *s = (char *)mrqst->sta_mac_addr;
    PRIN_LOG("----------------WPC MEASUREMENT_REQUEST-----------------");
    PRIN_LOG("Request ID            :%x",mrqst->request_id);
    PRIN_LOG("Mode                  :%x",mrqst->mode);
    PRIN_LOG("STA MAC addr          :%x:%x:%x:%x:%x:%x", s[0],s[1],s[2],s[3],s[4],s[5]);
    s = (const char *)mrqst->spoof_mac_addr;
    PRIN_LOG("Spoof MAC addr        :%x:%x:%x:%x:%x:%x", s[0],s[1],s[2],s[3],s[4],s[5]);
    PRIN_LOG("STA Info              :%x",mrqst->sta_info);
    PRIN_LOG("Channel               :%x",mrqst->channel);
    PRIN_LOG("Number of measurements:%x",mrqst->no_of_measurements);
    PRIN_LOG("Transmit Rate         :%x",mrqst->transmit_rate);
    PRIN_LOG("Transmit Retries      :%x",mrqst->transmit_retries);
    PRIN_LOG("Timeout               :%x\n",mrqst->timeout);
}

void print_type1_resp(struct nsp_type1_resp *type1_resp)
{
#ifdef IEEE80211_DEBUG
    unsigned int i, tmp;
#endif
    unsigned int type1_payld_size, chain_cnt;
     u_int8_t numRxChains = ieee80211_mask2numchains[
                    (type1_resp->no_of_chains & (RX_CHAINMASK_HASH >>8))];
    type1_payld_size  = CDUMP_SIZE(numRxChains);
    PRIN_LOG("TOA                   :%llx",type1_resp->toa);
    PRIN_LOG("TOD                   :%llx",type1_resp->tod);
    PRIN_LOG("RTT                   :%llx",(((type1_resp->toa - type1_resp->tod))));
    PRIN_LOG("Send rate             :%x",type1_resp->send_rate);
    PRIN_LOG("Receive rate          :%x",type1_resp->receive_rate);
    PRIN_LOG("No of retries         :%x",type1_resp->no_of_retries);
    PRIN_LOG("No of chains          :%x",type1_resp->no_of_chains);
    PRIN_LOG("Payload info          :%x",type1_resp->payload_info);
    for (chain_cnt = 0; chain_cnt < numRxChains; chain_cnt++)
        PRIN_LOG("RSSI for CHAIN %d      :%x",chain_cnt + 1, type1_resp->rssi[chain_cnt]);
    PRIN_LOG("Channel dump          :%d", type1_payld_size);
#ifdef IEEE80211_DEBUG
    for (i=0; i <= type1_payld_size/4; i++) {
        if (i%7 == 0 )
            printk("\n");
        tmp = *(((int *)type1_resp->type1_payld) + i);
        /* convert the channel dump to little endian format */
        tmp = ((tmp << 24) | ((tmp << 8) & 0x00ff0000) | ((tmp >> 8) & 0x0000ff00) | (tmp >> 24));
        printk("%x ", tmp);
    }
#endif
}
void print_sresp(struct nsp_sresp *sresp)
{
    int i=0;
    struct nsp_vap_info *vapinfo= (struct nsp_vap_info *)((char *)sresp + SRES_LEN);
    struct nsp_station_info *stainfo =(struct nsp_station_info *) ((char *)sresp + SRES_LEN + sresp->no_of_vaps* VAPINFO_LEN);

    PRIN_LOG("----------------WPC STATUS_RESPONSE-----------------");
    PRIN_LOG("Request ID            :%x",sresp->request_id);
    PRIN_LOG("No of stations        :%x",sresp->no_of_stations);
    PRIN_LOG("Request timestamp     :%llx",sresp->req_tstamp);
    PRIN_LOG("Response timestamp    :%llx",sresp->resp_tstamp);
    for ( i = 0 ; i < sresp->no_of_vaps ; i++)
        print_vap_info(vapinfo+i);

    for ( i = 0 ; i < sresp->no_of_stations ; i++)
        print_sta_info(stainfo+i);

    /*    s = (char *)sresp->ap1_mac;
    PRIN_LOG("AP1 Mac address    :%x %x %x %x %x %x",s[0],s[1],s[2],s[3],s[4],s[5]);
    PRIN_LOG("AP1 channel        :%x",sresp->ap1_channel);
    s = (char *)sresp->ap2_mac;
    PRIN_LOG("AP2 Mac address       :%x %x %x %x %x %x",s[0],s[1],s[2],s[3],s[4],s[5]);
    PRIN_LOG("AP2 channel           :%x",sresp->ap2_channel);
    PRIN_LOG("SSID                  :%s",sresp->ssid); */ 
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
    const char *s;
    s = (char *)sta_info->sta_mac_addr;
    PRIN_LOG("\nSTA Mac address    :%x %x %x %x %x %x",s[0],s[1],s[2],s[3],s[4],s[5]);
    PRIN_LOG("VAP Index          :%x",sta_info->vap_index);
    PRIN_LOG("STA last timestamp :%llx",sta_info->tstamp);
    PRIN_LOG("Station info       :%x",sta_info->info);
    PRIN_LOG("Station RSSI       :%x",sta_info->rssi);
}

void print_srqst(struct nsp_sreq *sreq)
{
    PRIN_LOG("----------------WPC STATUS_REQUEST-----------------");
    PRIN_LOG("Request ID            :%x",sreq->request_id);
}
void print_tsfresp(struct nsp_tsf_resp *tsfresp)
{
    unsigned char *s;
    s = (unsigned char *)tsfresp->ap_mac_addr;
    PRIN_LOG("----------------WPC TSF_RESPONSE-----------------");
    PRIN_LOG("Request ID            :%x",tsfresp->request_id);
    PRIN_LOG("MAC addr              :%x:%x:%x:%x:%x:%x", s[0],s[1],s[2],s[3],s[4],s[5]);
    PRIN_LOG("Associated AP's tsf   :%llx\n",tsfresp->assoc_tsf);
    PRIN_LOG("Probing AP's tsf      :%llx\n",tsfresp->prob_tsf);
    PRIN_LOG("Result                :%x\n",tsfresp->result);
}

void print_mresp(struct nsp_mresp *mresp)
{
    const char *s;
    s = (char *)mresp->sta_mac_addr;
    PRIN_LOG("----------------WPC TYPE1 MEASUREMENT_RESPONSE-----------------");
    PRIN_LOG("Request ID            :%x",mresp->request_id);
    PRIN_LOG("MAC addr              :%x:%x:%x:%x:%x:%x", s[0],s[1],s[2],s[3],s[4],s[5]);
    PRIN_LOG("No of responses       :%x",mresp->no_of_responses);
    PRIN_LOG("Request timestamp     :%llx",mresp->req_tstamp);
    PRIN_LOG("Response timestamp    :%llx",mresp->resp_tstamp);
    PRIN_LOG("Result                :%x",mresp->result);
}
void print_cap_resp(struct nsp_cap_resp *cresp)
{
    PRIN_LOG("----------------WPC CAPABILITY_RESPONSE-----------------");
    PRIN_LOG("Request ID            :%x",cresp->request_id);
    PRIN_LOG("Band                  :%x",cresp->band);
    PRIN_LOG("Positioning           :%x",cresp->positioning);
    PRIN_LOG("SW Version            :%x",cresp->sw_version);
    PRIN_LOG("HW Version            :%x",cresp->hw_version);
    PRIN_LOG("Clock Frequency       :%d",cresp->clk_freq);
}
void print_nsp_header(struct nsp_header *nsp_hdr)
{
    PRIN_LOG("----------------WPC NSP_HEADER-----------------");
    PRIN_LOG("Start of Frame Delimiter :%x",nsp_hdr->SFD);
    PRIN_LOG("Version                  :%x",nsp_hdr->version);
    PRIN_LOG("Frame Type               :%x",nsp_hdr->frame_type);
    PRIN_LOG("Frame Length             :%x",nsp_hdr->frame_length);
}



