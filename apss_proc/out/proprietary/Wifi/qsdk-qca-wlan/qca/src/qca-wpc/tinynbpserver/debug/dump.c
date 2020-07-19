/*
 * Copyright (c) 2011 Qualcomm Atheros, Inc..
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include<stdio.h>
#include<arpa/inet.h>
#include<ap.h>
// <Request_ID>  <MAC addr> <Result> <RTT Avg.> <RTT Stdev> <RTT Samples> <RSSI Avg.> <RSSI stdev> <RSSI Samples> 
int dump_resp0_to_file(FILE *fp, struct nsp_type0_resp *type0resp)
{
    int i=0;
    unsigned char *s = (char *)type0resp->sta_mac_addr;

//        fprintf(fp,"\n----------------NBP SERVER MEASUREMENT_RESPONSE-----------------");
        fprintf(fp,"\n<ReqID>\t<MAC>\t<Result><RTTAvg.>\t<RTTStdev>\t<RTTSamples>\t<RSSIAvg.>\t<RSSIstdev>\t<RSSISamples>\n");
        fprintf(fp,"\t%x",type0resp->request_id);
        fprintf(fp,"\t%x:%x:%x:%x:%x:%x", s[0],s[1],s[2],s[3],s[4],s[5]);
        fprintf(fp,"\t%x",ntohs(type0resp->result));
        fprintf(fp,"\t%x",ntohl(type0resp->rtt_mean));
        fprintf(fp,"\t%x",ntohl(type0resp->rtt_stddev));
        fprintf(fp,"\t%x",(type0resp->rtt_samples));
        fprintf(fp,"\t%x",(type0resp->rssi_mean));
        fprintf(fp,"\t%x",(type0resp->rssi_stddev));
        fprintf(fp,"\t%x \n",(type0resp->rssi_samples));
}

// <RESPT - RQST T> < RQST Timestamp> <RESP TIMESTAMP> <Send Timestamp> < Receive timestamp> <IP address of AP> < MAC address of STA> <Request ID> <Send Rate> <Recv Rate> <RSSI0> <RSSI1> <Channel dump>


int dump_response_to_file(FILE *fp, struct type1_resp *resp, struct ap *ap)
{

//	struct ap *ap=find_ap_by_sockfd( sockfd );
	char dst[18]={0};
	int i = 0;
	char macaddr[20]={0};
    u_int8_t numRxChains = 0;

	ba2str(&resp->mresp.sta_mac_addr, macaddr);
	

	fprintf(fp,"%llx\t%llx\t%llx\t%llx\t%llx\t%s\t%s\t%d\t%d\t%d\t",htobe64(resp->mresp.resp_tstamp) - htobe64(resp->mresp.req_tstamp) ,
                                                        htobe64(resp->mresp.req_tstamp),
                                                        htobe64(resp->mresp.resp_tstamp),
                                                        htobe64(resp->type1resp.tod), 
                                                        htobe64(resp->type1resp.toa), 
			                                            inet_ntop(AF_INET, &ap->sockaddr.sin_addr.s_addr, dst, sizeof(dst)),  
                                                        macaddr, resp->mresp.request_id, 
			                                            htonl(resp->type1resp.send_rate),
                                                        htonl(resp->type1resp.receive_rate));
    numRxChains = ieee80211_mask2numchains[
                    (resp->type1resp.no_of_chains & (RX_CHAINMASK_HASH >>8))];

	for ( i=0; i< numRxChains ; i++ )
		fprintf(fp,"%02x\t",resp->type1resp.rssi[i]);


	for ( i=0; i<CDUMP_SIZE_PER_CHAIN * numRxChains; i++ ) {
		
		fprintf(fp, "%02x ",(unsigned char) resp->type1resp.type1_payld[i]);
	}

	fprintf(fp, "\n");
	
	return 0;

}
