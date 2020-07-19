
/*
 * Copyright (c) 2011 Qualcomm Atheros, Inc..
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include <sys/select.h>
#include <sys/types.h>
#include <string.h>
#include <sys/socket.h>
#include <net/if.h>
#include <linux/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <linux/netlink.h>
#include <netinet/in.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include <ieee80211_wpc.h>
#include <sys/mman.h>
#include "wpc_mgr.h"

#define PRIN_LOG(format, args...) printf(format "", ## args)
#define REQUEST_CANNOT_HANDLE 2
#define REQUEST_TIMEOUT 4
#define REQUEST_THROTTLE 2
#define WPC_DBG_SIZE 1024*1024
#define FILEPATH "/tmp/wpc_dbg_log"
#define wpc_rtte_calculate_ftoa(pd, rxchains) do {}while(0)
//TBD: Remove this
#ifndef MAX_PAYLOAD
#define MAX_PAYLOAD 8192 //1024
#endif

extern int wpc_server_sock;
extern fd_set read_fds, error_set;
extern int fdcount, wpc_driver_sock;
extern struct iovec iov;
extern struct nlmsghdr *nlh;
extern struct msghdr msg;
int request_id;
timer_t timerid, timerid1;
int wpc_debug;
u_int8_t *wpc_dbg_buf;
int wpc_dbg_log_len = 0;

/* State information for RTT requests. */
struct nsp_mrequest_state mrqst_state[WPC_MAX_CONCURRENTREQ];
unsigned int mrqst_index = 0;
/* Function defined extern to remove warnings */
extern int wpc_write(int wpc_serverfd, char *buffer, int recv_len);
extern int wpc_close(int sockfd);
extern int wpc_recv(int wpc_driver_sock, char *buffer, int recv_len, int flags);
extern void print_nsp_header(struct nsp_header *nsp_hdr);
extern void print_tsfrqst(struct nsp_tsf_req *tsfrqst);
extern void print_wpcdbgresp(struct nsp_wpc_dbg_resp *wpcdbgres);
extern void wpc_nlsendmsg(int wpc_driver_sock, int nbytes, char *buf, struct nlmsghdr *nlh, struct iovec *iov, struct msghdr *msg);
extern void print_wresp(struct nsp_wakeup_resp *wresp);
extern void print_slresp(struct nsp_sleep_resp *slresp);
extern void print_mrqst(struct nsp_mrqst *mrqst);
extern void print_srqst(struct nsp_sreq *sreq);
extern void print_crqst(struct nsp_cap_req *crqst);
extern void print_wrqst(struct nsp_wakeup_req *wrqst);
extern void print_slrqst(struct nsp_sleep_req *slrqst);
extern void print_wpcdbgrqst(struct nsp_wpc_dbg_req *wpcdbgrqst);
extern void print_mresp(struct nsp_mresp *mresp);
extern void print_tsfresp(struct nsp_tsf_resp *tsfresp);
extern void print_cap_resp(struct nsp_cap_resp *cresp);
extern void print_sresp(struct nsp_sresp *sresp);
extern void print_type1_resp(struct nsp_type1_resp *type1_resp);

int wpc_mgr_proctype0mreq(char *buffer);
int wpc_mgr_proctype1mreq(char * buffer);
int wpc_mgr_proctsfreq(char *buffer);
int wpc_mgr_checkresptype(char *buffer);
int wpc_mgr_proctype0mresp(char * buffer, int recv_len); 
int wpc_mgr_proctype1mresp(char *buffer); 
int wpc_mgr_calcstats(int index);
int wpc_mgr_sendtype0resp(int index); 


void wpc_mgr_init()
{
    memset(&mrqst_state, 0, sizeof(mrqst_state));
}

void wpc_mgr_initstate(int index) 
{
	//Initialize state at index
	mrqst_state[index].timeout =0;
	mrqst_state[index].mrqst.request_id  = -1;
	mrqst_state[index].starttime.tv_sec =0;
	mrqst_state[index].starttime.tv_usec =0;
	mrqst_state[index].valid =0;
	mrqst_state[index].type = NSP_TYPE1RESP;
	mrqst_state[index].state = WPC_MGRSTATE_INIT;
	mrqst_state[index].proberespcount =0;
	mrqst_state[index].rtt_mean =0;
	mrqst_state[index].rtt_stddev =0;
	mrqst_state[index].rtt_samples =0;
	mrqst_state[index].rssi_mean =0;
	mrqst_state[index].rssi_stddev =0;
	mrqst_state[index].rssi_samples =0;
    mrqst_state[index].mrqst.request_id = 0;
}

int write_dbg_log(int fd)
{
    int result;
    
    fd = open(FILEPATH, O_RDWR);
    if (fd == -1) {
        perror("Error opening file for writing");
        exit(EXIT_FAILURE);
    }
    wpc_dbg_buf = malloc(WPC_DBG_SIZE);
    memset(wpc_dbg_buf, 0, WPC_DBG_SIZE); 
    result = lseek(fd, WPC_DBG_SIZE-1, SEEK_SET);
    if (result == -1) {
        close(fd);
        perror("Error calling lseek() to 'stretch' the file");
        exit(EXIT_FAILURE);
    }
    result = write(fd, "", 1);
    if (result != 1) {
        close(fd);
        perror("Error writing last byte of the file");
        exit(EXIT_FAILURE);
    }

    wpc_dbg_buf = mmap(0, WPC_DBG_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (wpc_dbg_buf == MAP_FAILED) {
        close(fd);
        perror("Error mmapping the file");
        exit(EXIT_FAILURE);
    }
    return fd;
}

void stop_dbg_log ( int fd)
{
    if (munmap(wpc_dbg_buf, WPC_DBG_SIZE) == -1) {
        perror("Error un-mmapping the file");
    }
    free(wpc_dbg_buf);
    close(fd);
}

void dump_wpc_log (int line_no, u_int8_t frame_type)
{
    struct wpc_dbg_header wpc_dbg_hdr;
    memcpy(wpc_dbg_hdr.filename, __FILE__, sizeof(__FILE__));
    wpc_dbg_hdr.line_no = line_no;
    wpc_dbg_hdr.frame_type = frame_type;
    memcpy(wpc_dbg_buf + wpc_dbg_log_len, &wpc_dbg_hdr, WPCDBGHDR_LEN);
    wpc_dbg_log_len += WPCDBGHDR_LEN;
}

int send_empty_tsfresponse(int request_id, int result) {
    struct nsp_header nsp_hdr;
    struct nsp_tsf_resp tsfresp={0};
    char buf[NSP_HDR_LEN + TSFRES_LEN];
    int recv_len;
    int written;
    
    memset(&buf, 0, sizeof(buf));
    nsp_hdr.SFD = START_OF_FRAME;
    nsp_hdr.version = 1;
    nsp_hdr.frame_type = NSP_TSFRESP;
    nsp_hdr.frame_length = TSFRES_LEN;
    memcpy(buf, &nsp_hdr, NSP_HDR_LEN);

    tsfresp.request_id = request_id;
    memset(tsfresp.ap_mac_addr, 0, ETH_ALEN);
    tsfresp.result = -1;
    tsfresp.assoc_tsf = 0;
    tsfresp.prob_tsf = 0;
    memcpy((buf + NSP_HDR_LEN), &tsfresp, TSFRES_LEN);
    recv_len = NSP_HDR_LEN + TSFRES_LEN;
    if (wpc_debug) {
        if (wpc_dbg_log_len + WPCDBGHDR_LEN <= WPC_DBG_SIZE) {
            dump_wpc_log(__LINE__, nsp_hdr.frame_type);
        }
        else {
            printf("wpc log file is full:cannot written\n");
            return -1;
        }
    }

    written = wpc_write(wpc_server_sock, ((char *)buf), recv_len);
    if ( written < recv_len) {
        // TBD : Need to write pending data if there is partial write 
        printf("Partial write closing connection Size: %d Written: %d\n", sizeof(struct nsp_header)+sizeof(struct nsp_mresp), written);
        wpc_close(wpc_server_sock);

        if(wpc_server_sock > 0)
            FD_CLR(wpc_server_sock, &read_fds);

        wpc_server_sock = -1;
    }
    return 0;	
}

int send_error_msg(int request_id, int error_id)
{
    struct nsp_header *nsp_hdr;
    struct nsp_error_msg *error_msg;
    char buf[NSP_HDR_LEN + ERRORMSG_LEN];
    int written = 0;
    int recv_len = NSP_HDR_LEN + ERRORMSG_LEN;
    memset(&buf, 0, sizeof(buf));
    nsp_hdr = (struct nsp_header*)(buf);
    error_msg = (struct nsp_error_msg*)(((char *)buf)+ NSP_HDR_LEN);
    nsp_hdr->SFD = START_OF_FRAME;
    nsp_hdr->version = NSP_VERSION;
    nsp_hdr->frame_type = NSP_ERRORMSG;
    nsp_hdr->frame_length = ERRORMSG_LEN;
    error_msg->request_id = request_id;
    error_msg->errorid = error_id;

    written = wpc_write(wpc_server_sock, ((char *)buf), recv_len);
    if ( written < recv_len) {
        // TBD : Need to write pending data if there is partial write 
        printf("Partial write closing connection Size: %d Written: %d\n", sizeof(struct nsp_header)+sizeof(struct nsp_mresp), written);

        if(wpc_server_sock > 0 ) {
                wpc_close(wpc_server_sock);
                FD_CLR(wpc_server_sock, &read_fds);
                wpc_server_sock = -1;
        }
    }
	return 0;	
}

int send_empty_mresponse(int request_id, int result) {
    struct nsp_header nsp_hdr;
    struct nsp_mresp mresp={0};
//    struct nsp_type1_resp chanest_resp={0};
    char buf[NSP_HDR_LEN + MRES_LEN + TYPE1RES_LEN];
//    unsigned int type1_payld_size;
    int recv_len;
    int written;
	int count = 0,index = -1;
        for(count =0; count < WPC_MAX_CONCURRENTREQ; count++ ) {
            //Look up request 
            if(request_id == mrqst_state[count].mrqst.request_id && mrqst_state[count].valid == 1) {
                index = count;
                break;
            }
        }
    if(index < 0) {
        printf("Couldn't find Request : %x, result: %x\n", request_id, result);
        return -1; 
    }

    memset(&buf, 0, sizeof(buf));
    nsp_hdr.SFD = START_OF_FRAME;
    nsp_hdr.version = NSP_VERSION;
    nsp_hdr.frame_type = NSP_TYPE1RESP;
    nsp_hdr.frame_length = MRES_LEN;
    memcpy(buf, &nsp_hdr, NSP_HDR_LEN);

    mresp.request_id = request_id;
    mresp.no_of_responses = 0;
    mresp.req_tstamp = 0;
    mresp.resp_tstamp = 0; 
    mresp.result = result;
    if(!(result & WPC_INPUT_THROTTLE_MASK)){
        PRIN_LOG("Sending empty response RI: %d, %d\n", result,  mresp.request_id);
        memcpy(mresp.sta_mac_addr, &mrqst_state[index].mrqst.sta_mac_addr, ETH_ALEN);
    } else {
        PRIN_LOG("Sending empty response RI: %d, %d\n", result,  mresp.request_id);
        memset(mresp.sta_mac_addr,0, sizeof(mresp.sta_mac_addr) );
        
    }
    memcpy(((char *)buf + NSP_HDR_LEN), &mresp, MRES_LEN);
    recv_len = NSP_HDR_LEN + MRES_LEN;

    if (wpc_debug) {
        if (wpc_dbg_log_len + WPCDBGHDR_LEN <= WPC_DBG_SIZE) {
            dump_wpc_log(__LINE__, nsp_hdr.frame_type);
        }
        else {
            printf("wpc log file is full:cannot written\n");
            return -1 ;
        }
    }

    written = wpc_write(wpc_server_sock, ((char *)buf), recv_len);
    if ( written < recv_len) {
        // TBD : Need to write pending data if there is partial write 
        printf("Partial write closing connection Size: %d Written: %d\n", sizeof(struct nsp_header)+sizeof(struct nsp_mresp), written);

        if(wpc_server_sock > 0 ) {
                wpc_close(wpc_server_sock);
                FD_CLR(wpc_server_sock, &read_fds);
                wpc_server_sock = -1;
        }
    }
    return 1;
}

#define CLOCKID CLOCK_REALTIME

int wpc_mgr_procservermsg() 
{
	int nbytes;
	int written;
	int write_len;
	int fd = 0;
	u_int8_t buf[MAX_PAYLOAD]={'\0'};
	nbytes = wpc_recv(wpc_server_sock, (char*)buf, NSP_HDR_LEN, 0);
	struct nsp_header *nsp_header = (struct nsp_header *) buf;

	struct nsp_header nsp_hdr;
	int len = 0;
	//TBD: read length from frame header
	switch (nsp_header->frame_type)
	{
		case NSP_MRQST:
			len = MREQ_LEN;
			break;
		case NSP_SRQST:
			len = SREQ_LEN;
			break;
		case NSP_CRQST:
			len = CAPREQ_LEN;
			break;
		case NSP_WAKEUPRQST:
			len = WAKEUPREQ_LEN;
			break;
		case NSP_TSFRQST:
			len = TSFREQ_LEN;
			break;
		case NSP_STARETURNTOSLEEPREQ:
			len = SLEEPREQ_LEN;
			break;
		case NSP_WPCDBGRGST:
			len = WPCDBGREQ_LEN;
			break;
	}
	print_nsp_header(nsp_header);
	nbytes += wpc_recv(wpc_server_sock, (char *)(buf+NSP_HDR_LEN), len, 0);
	if (nbytes <= 0) {
		//error or connection closed by client
		if (nbytes == 0)
			printf("selectserver: socket %d hung up\n", fdcount);
		else 
			printf("recv failed\n");
		wpc_close(wpc_server_sock);


        if(wpc_server_sock > 0)
    		FD_CLR(wpc_server_sock, &read_fds);

		wpc_server_sock = -1;
	} else {
		buf[nbytes] = '\0';

		struct nsp_mrqst *mrqst;
		struct nsp_sreq *srqst;
		struct nsp_cap_req *crqst;
		struct nsp_wakeup_req *wrqst;
		struct nsp_tsf_req *tsfrqst;
		struct nsp_sleep_req *slrqst;
		struct nsp_wpc_dbg_req *wpcdbgrqst;
		struct nsp_wpc_dbg_resp wpcdbgresp;
		struct request_no *reqno;
		struct timeval tv;
		time_t req_curtime;
        int do_not_send = 0;
		char time_buf[30];
		reqno = (struct request_no *)(((char *)buf) + NSP_HDR_LEN);
		if( nsp_header->version > NSP_VERSION ) {
			printf("ERROR: Request protocol version no (%02x) >  Current Version (%02x)\n",nsp_header->version, NSP_VERSION);
			send_error_msg(reqno->request_id, NSP_UNSUPPORTED_PROTOCOL_VERSION);
			return -1;
		}
		switch (nsp_header->frame_type)
		{
		case NSP_MRQST:
			mrqst = (struct nsp_mrqst *)(((char *)buf) + NSP_HDR_LEN);
                request_id = mrqst->request_id;
                if (wpc_debug) {
                    if (wpc_dbg_log_len + WPCDBGHDR_LEN <= WPC_DBG_SIZE) {
                        dump_wpc_log(__LINE__, nsp_header->frame_type);
                    }
                    else {
                        printf("wpc log file is full:cannot written\n");
                        return -1;
                    }

                }
                else {
                    print_mrqst(mrqst);
                    mrqst->mode = htons(mrqst->mode);
                    gettimeofday(&tv, NULL);
                    req_curtime=tv.tv_sec;
                    strftime(time_buf,30,"%m-%d-%Y  %T.",localtime(&req_curtime));
                    PRIN_LOG("Req Time%s %ld\n",time_buf,tv.tv_usec);
                    if((mrqst->mode & NSP_MRQSTTYPE_MASK) ==  NSP_MRQSTTYPE_TYPE0) {
                        printf("%s: %d: Received a Type 0 Measurement Request\n", __FUNCTION__, __LINE__);
                        wpc_mgr_proctype0mreq((char*)buf);
                    }else if((mrqst->mode & NSP_MRQSTTYPE_MASK) ==  NSP_MRQSTTYPE_TYPE1) {
                        printf("%s: %d: Received a Type 1 Measurement Request\n", __FUNCTION__, __LINE__);
                        wpc_mgr_proctype1mreq((char*)buf);
                    }
                }
			break;
		case NSP_SRQST: 
			srqst = (struct nsp_sreq *)(((char *)buf) + NSP_HDR_LEN);
            if (wpc_debug) {
                if (wpc_dbg_log_len + WPCDBGHDR_LEN <= WPC_DBG_SIZE) {
                    dump_wpc_log(__LINE__, nsp_header->frame_type);
                }
                else {
                    printf("wpc log file is full:cannot written\n");
                    return -1;
                }
            }
            else {
                print_srqst(srqst);
            }
			break;
		case NSP_CRQST: 
			crqst = (struct nsp_cap_req *)(((char *)buf) + NSP_HDR_LEN);
            if (wpc_debug) {
                if (wpc_dbg_log_len + WPCDBGHDR_LEN <= WPC_DBG_SIZE) {
                    dump_wpc_log(__LINE__, nsp_header->frame_type);
                }
                else {
                    printf("wpc log file is full:cannot written\n");
                    return -1;
                }
            }
            else {
                print_crqst(crqst);
            }
			break;
		case NSP_WAKEUPRQST:
			wrqst = (struct nsp_wakeup_req *) (((char *)buf) + NSP_HDR_LEN);
            if (wpc_debug) {
                if (wpc_dbg_log_len + WPCDBGHDR_LEN <= WPC_DBG_SIZE) {
                    dump_wpc_log(__LINE__, nsp_header->frame_type);
                }
                else {
                    printf("wpc log file is full:cannot written\n");
                    return -1;
                }
            }
            else {
                print_wrqst(wrqst);
            }
			break;
		case NSP_TSFRQST:
			tsfrqst = (struct nsp_tsf_req *) (((char *)buf) + NSP_HDR_LEN);
			wpc_mgr_proctsfreq((char*)buf);
            if (wpc_debug) {
                if (wpc_dbg_log_len + WPCDBGHDR_LEN <= WPC_DBG_SIZE) {
                    dump_wpc_log(__LINE__, nsp_header->frame_type);
                }
                else {
                    printf("wpc log file is full:cannot written\n");
                    return -1;
                }

            }

			print_tsfrqst(tsfrqst);
			break;
		case NSP_STARETURNTOSLEEPREQ:
			slrqst = (struct nsp_sleep_req *) (((char *)buf) + NSP_HDR_LEN);
            if (wpc_debug) {
                if (wpc_dbg_log_len + WPCDBGHDR_LEN <= WPC_DBG_SIZE) {
                    dump_wpc_log(__LINE__, nsp_header->frame_type);
                }
                else {
                    printf("wpc log file is full:cannot written\n");
                    return -1;
                }
            }
            else {
                print_slrqst(slrqst);
            }
			break;
         default:
            do_not_send = 1;
       	}
		if(nsp_header->frame_type == NSP_WPCDBGRGST) {
            wpcdbgrqst = (struct nsp_wpc_dbg_req*) (((char *)buf) + NSP_HDR_LEN);
            print_wpcdbgrqst(wpcdbgrqst);
            if (wpcdbgrqst->dbg_mode) {
                if( !wpc_debug ) { 
                wpc_debug = 1;
                fd = write_dbg_log(fd);
                }
            }
            else {
                if (wpc_debug) {
                wpc_debug = 0;
                stop_dbg_log(fd);
                }
            }
            nsp_hdr.SFD = START_OF_FRAME;
            nsp_hdr.version = NSP_VERSION;
            nsp_hdr.frame_type = NSP_WPCDBGRESP;
            nsp_hdr.frame_length = WPCDBGRESP_LEN;
            memcpy(buf, &nsp_hdr, NSP_HDR_LEN);
	        print_nsp_header(&nsp_hdr);
            wpcdbgresp.request_id = wpcdbgrqst->request_id;
            wpcdbgresp.result = 0;
            memcpy((buf + NSP_HDR_LEN), &wpcdbgresp, WPCDBGRESP_LEN);
            print_wpcdbgresp(&wpcdbgresp);
            write_len = NSP_HDR_LEN + WPCDBGRESP_LEN;
            written = wpc_write(wpc_server_sock, ((char *)buf), write_len);
            if ( written < write_len) {
                // TBD : Need to write pending data if there is partial write 
                printf("Partial write closing connection Size: %d Written: %d\n", sizeof(struct nsp_header)+sizeof(struct nsp_mresp), written);
                wpc_close(wpc_server_sock);
                FD_CLR(wpc_server_sock, &read_fds);
                wpc_server_sock = -1;
            }
        }
        else {
            if (do_not_send == 0)
                wpc_nlsendmsg(wpc_driver_sock,nbytes,(char*)buf,nlh,&iov,&msg);
            else
                do_not_send = 1;
        }
	}
	return 0;	
}


int wpc_mgr_proctsfresp(char *buffer)
{
    struct nsp_mresp *mresp = (struct nsp_mresp *)((char *)buffer + sizeof(struct nlmsghdr) + NSP_HDR_LEN);
    int index = -1;
    int count =0;
        
    for(count =0; count < WPC_MAX_CONCURRENTREQ; count++ ) {
        //Look up request 
        if(mresp->request_id == mrqst_state[count].mrqst.request_id && mrqst_state[count].valid == 1) {
            index = count;
            break;
        }
    }
    if(index == -1) {
        PRIN_LOG("%s: %d Could not find entry for response in state table.\n", __FUNCTION__, __LINE__);
        printf("%s: %d Could not find entry for response[%x] in state table.\n", __FUNCTION__, __LINE__, 
                                mresp->request_id);
        return -1;
    }

    if((mrqst_state[index].type != NSP_TSFRESP ) || (mrqst_state[index].valid != 1)) {
        PRIN_LOG("%s: %d: Error: Should be a Type 0 Resp and valid: %d %d  \n", __FUNCTION__, __LINE__, mrqst_state[index].type, mrqst_state[index].valid );
        return -1;
    }
    mrqst_state[index].valid = 0;
    wpc_mgr_initstate(index);
    printf("Recieved TSF Responses. Cancelling timeout \n");
    return 1;
}


int wpc_mgr_procdrivermsg() 
{
	int recvlen;
	int written;
	char buffer[MAX_PAYLOAD];
	int type;
	// handle data sent up by kernel on the netlink socket
	recvlen = wpc_recv(wpc_driver_sock, buffer, MAX_PAYLOAD, 0);
	if( recvlen <= 0 ) {
		wpc_close(wpc_driver_sock);
        if(wpc_driver_sock > 0)
    		FD_CLR(wpc_driver_sock, &read_fds);
	}
	else {
		struct nsp_mresp *res;
		struct nsp_sresp *sres;
		struct nsp_header *nsp_header;
		struct nsp_type1_resp *type1_resp;
		struct nsp_cap_resp *cap_resp;
//		struct nsp_station_info *sta_info;
        struct nsp_wakeup_resp *wresp;
        struct nsp_sleep_resp *sleep_resp;
		struct timeval tv;
		time_t curtime;
		char time_buf[30];
		unsigned int recv_len = 0;
		//int i;
		int send = 1;
        printf("\n msg from the driver****************\n");
		if (wpc_server_sock > 0) {
			nsp_header = (struct nsp_header *) ((char *)buffer + sizeof(struct nlmsghdr));
			print_nsp_header(nsp_header);
			switch( nsp_header->frame_type ) {
                //TBD: read length from frame header
				case NSP_WAKEUPRESP:
					recv_len = NSP_HDR_LEN + WAKEUPRES_LEN;
                    wresp = ((struct nsp_wakeup_resp *)(((char *)buffer)+sizeof(struct nlmsghdr)
                                    + NSP_HDR_LEN));
                    print_wresp(wresp);
					break;
				case NSP_STARETURNTOSLEEPRES:
					recv_len = NSP_HDR_LEN + SLEEPRES_LEN;
                    sleep_resp = ((struct nsp_sleep_resp *)(((char *)buffer)+sizeof(struct nlmsghdr)
                                    + NSP_HDR_LEN));
                    print_slresp(sleep_resp);
					break;
				case NSP_TSFRESP:
					recv_len = NSP_HDR_LEN + TSFRES_LEN;
                    wpc_mgr_proctsfresp(buffer);
                    if (wpc_debug) {
                        if (wpc_dbg_log_len + WPCDBGHDR_LEN <= WPC_DBG_SIZE) {
                            dump_wpc_log(__LINE__, nsp_header->frame_type);
                        }
                        else {
                            printf("wpc log file is full:cannot written\n");
                            return -1;
                        }
                    }
                    else {
                        print_tsfresp((struct nsp_tsf_resp *)(((char *)buffer)+sizeof(struct nlmsghdr) 
                                    + NSP_HDR_LEN));
                    }
					break;
				case NSP_CRESP:
					recv_len = NSP_HDR_LEN + CAPRES_LEN;
					cap_resp = (struct nsp_cap_resp *)((char *)buffer + sizeof(struct nlmsghdr) 
							+ sizeof(struct nsp_header));
                    if (wpc_debug) {
                        if (wpc_dbg_log_len + WPCDBGHDR_LEN <= WPC_DBG_SIZE) {
                            dump_wpc_log(__LINE__, nsp_header->frame_type);
                        }
                        else {
                            printf("wpc log file is full:cannot written\n");
                            return -1;
                        }
                    }
                    else {
                        print_cap_resp(cap_resp);
                    }
					break;
				case NSP_SRESP:
					sres = (struct nsp_sresp *)((char *)buffer + sizeof(struct nlmsghdr) 
							+ sizeof(struct nsp_header));
					recv_len = NSP_HDR_LEN + SRES_LEN + sres->no_of_vaps * VAPINFO_LEN + sres->no_of_stations*SINFO_LEN;
                    if (wpc_debug) {
                        if (wpc_dbg_log_len + WPCDBGHDR_LEN <= WPC_DBG_SIZE) {
                            dump_wpc_log(__LINE__, nsp_header->frame_type);
                        }
                        else {
                            printf("wpc log file is full:cannot written\n");
                            return -1;
                        }
                    }
                    else {
                        //print_nsp_header(nsp_header);
                        print_sresp(sres);
                    }
					break;
				case NSP_TYPE1RESP:
					res = (struct nsp_mresp *)((char *)buffer + sizeof(struct nlmsghdr) 
							+ sizeof(struct nsp_header));
					gettimeofday(&tv, NULL);
					curtime=tv.tv_sec;
					strftime(time_buf,30,"%m-%d-%Y  %T.",localtime(&curtime));
					PRIN_LOG("Resp Time%s %ld\n",time_buf,tv.tv_usec);
					type1_resp = (struct nsp_type1_resp *)((char *)buffer 
							+ sizeof(struct nlmsghdr) + sizeof(struct nsp_header) + MRES_LEN);
					recv_len = NSP_HDR_LEN + MRES_LEN + (res->no_of_responses * TYPE1RES_LEN);
                    if (wpc_debug) {
                        if (wpc_dbg_log_len + WPCDBGHDR_LEN <= WPC_DBG_SIZE) {
                            dump_wpc_log(__LINE__, nsp_header->frame_type);
                        }
                        else {
                            printf("wpc log file is full:cannot written\n");
                            return -1;
                        }
                    }
                    else {
                        //Response from the driver will be Type 1, 
                        //check what response has been requested by server
                        type = wpc_mgr_checkresptype(buffer);
                        printf("\n the type is %d\n",type);
                        if (type==1)
                            send = wpc_mgr_proctype1mresp(buffer); 
                        else if(type==0)
                            send = wpc_mgr_proctype0mresp(buffer, recv_len); 
                        else {
                            printf("Drop packet as typeis %d\n",type);
                            send = -1;
                        }
//                      print_nsp_header(nsp_header);
                        print_mresp(res);
                        print_type1_resp(type1_resp);
                    }
					break;
				case NSP_TYPE0RESP:
					printf("Driver can't send Type0 responses\n");
					wpc_mgr_proctype0mresp(buffer, 0); 
					break;
				default:
					printf("Unknown frame type\n");
					break;
			}

            if ( send >= 0 ) {
                written = wpc_write(wpc_server_sock, ((char *)buffer)+sizeof(struct nlmsghdr), recv_len);
                if ( written < recv_len) {
                    // TBD : Need to write pending data if there is partial write 
                    printf("Partial write closing connection Size: %d Written: %d\n", sizeof(struct nsp_header)+sizeof(struct nsp_mresp), written);
                    if(wpc_server_sock > 0)
                    {
                        wpc_close(wpc_server_sock);
                        FD_CLR(wpc_server_sock, &read_fds);
                        wpc_server_sock = -1;
                    }
                }
            }
            else if(send == -1){
                PRIN_LOG("Dropping frame as send = %d, this request has been timeout\n", send);
            } else if(send == -2){
                PRIN_LOG("Dropping frame as send = %d, this request One of TX has failed \n", send);
            }

        }
    }

    return 0;	
}

int wpc_mgr_proctsfreq(char *buffer)
{
    struct nsp_tsf_req * tsfrqst = (struct nsp_tsf_req *)(((char *)buffer) + NSP_HDR_LEN);

    if(mrqst_state[mrqst_index].valid == 0) {
        PRIN_LOG("%s: %d: Adding entry to request state: %d \n", __FUNCTION__, __LINE__, mrqst_index);

        memcpy (&(mrqst_state[mrqst_index].tsfrqst), tsfrqst , sizeof(struct nsp_tsf_req));
        mrqst_state[mrqst_index].type = NSP_TSFRESP;
        gettimeofday(&(mrqst_state[mrqst_index].starttime), NULL);
        mrqst_state[mrqst_index].timeout = tsfrqst->timeout * 1000;
        mrqst_state[mrqst_index].valid = 1;
        mrqst_state[mrqst_index].state = WPC_MGRSTATE_REQUESTINPROGRESS;

        mrqst_index++;
        if(mrqst_index >= WPC_MAX_CONCURRENTREQ)
            mrqst_index = 0;

    } else {
        PRIN_LOG("%s: %d: Error: Insufficient buffering for requests %d \n", __FUNCTION__, __LINE__, mrqst_index);
        send_empty_mresponse(tsfrqst->request_id, REQUEST_THROTTLE);
    }

    return 1;
}


int wpc_mgr_proctype0mreq(char *buffer) 
{


    //Check if state entry exists for this STA
	//struct nsp_header * nsp_header = (struct nsp_header *) ((char *)buffer);
        struct nsp_mrqst * mrqst = (struct nsp_mrqst *)(((char *)buffer) + NSP_HDR_LEN);

    PRIN_LOG("Processing Type0 resp\n");
	if(mrqst_state[mrqst_index].valid == 0) {
		PRIN_LOG("%s: %d: Adding entry to request state: %d \n", __FUNCTION__, __LINE__, mrqst_index);
		

		//Setup state 
	        memcpy (&(mrqst_state[mrqst_index].mrqst), mrqst , sizeof(struct nsp_mrqst));
	        mrqst_state[mrqst_index].type = NSP_TYPE0RESP;
		gettimeofday(&(mrqst_state[mrqst_index].starttime), NULL);
	        mrqst_state[mrqst_index].timeout = mrqst->timeout * 1000;
	        mrqst_state[mrqst_index].valid = 1;
	        mrqst_state[mrqst_index].state = WPC_MGRSTATE_REQUESTINPROGRESS;
            mrqst_state[mrqst_index].proberespcount = 0;
/*
            if((mrqst->mode & RX_CHAINMASK_HASH) == RX_CHAINMASK_1)
	            mrqst_state[mrqst_index].pkt_corr_info.nrx_chains = 1;
            if((mrqst->mode & RX_CHAINMASK_HASH) == RX_CHAINMASK_2)
	            mrqst_state[mrqst_index].pkt_corr_info.nrx_chains = 2;
            if((mrqst->mode & RX_CHAINMASK_HASH) == RX_CHAINMASK_3)
	            mrqst_state[mrqst_index].pkt_corr_info.nrx_chains = 3;
*/
       		mrqst_index++;
		if(mrqst_index >= WPC_MAX_CONCURRENTREQ)
			mrqst_index = 0;

	} else {
		PRIN_LOG("%s: %d: Error: Insufficient buffering for requests %d \n", __FUNCTION__, __LINE__, mrqst_index);
        send_empty_mresponse(mrqst->request_id, REQUEST_THROTTLE);
	}


    //Pass Measurement Request to the driver: The request is changed from Type 0 to Type 1
	mrqst->mode &= NSP_MRQSTTYPE_TYPE1;
	PRIN_LOG("%s: %d: Received a Type 0 Measurement Request from Server\n", __FUNCTION__, __LINE__);
	PRIN_LOG("%s: %d: Will send Type 1 Request to Driver\n", __FUNCTION__, __LINE__);

	return 0;


}


int wpc_mgr_proctype1mreq(char * buffer) 
{
	//Check if state entry exists for this STA
    //struct nsp_header * nsp_header = (struct nsp_header *) ((char *)buffer);
    struct nsp_mrqst * mrqst = (struct nsp_mrqst *)(((char *)buffer) + NSP_HDR_LEN);

    if(mrqst_state[mrqst_index].valid == 0) {
        PRIN_LOG("%s: %d: Adding entry to request state: %d \n with timeout as %d\n", __FUNCTION__, __LINE__, mrqst_index, mrqst->timeout);


        //Setup state 
        memcpy (&(mrqst_state[mrqst_index].mrqst), mrqst , sizeof(struct nsp_mrqst));
        mrqst_state[mrqst_index].type = NSP_TYPE1RESP;
        gettimeofday(&(mrqst_state[mrqst_index].starttime), NULL);
        mrqst_state[mrqst_index].timeout = ntohs(mrqst->timeout) * 100000;
        mrqst_state[mrqst_index].valid = 1;
        mrqst_state[mrqst_index].state = WPC_MGRSTATE_REQUESTINPROGRESS;
        mrqst_state[mrqst_index].proberespcount = 0;
        /*
           if((mrqst->mode & RX_CHAINMASK_HASH) == RX_CHAINMASK_1)
           mrqst_state[mrqst_index].pkt_corr_info.nrx_chains = 1;
           if((mrqst->mode & RX_CHAINMASK_HASH) == RX_CHAINMASK_2)
           mrqst_state[mrqst_index].pkt_corr_info.nrx_chains = 2;
           if((mrqst->mode & RX_CHAINMASK_HASH) == RX_CHAINMASK_3)
           mrqst_state[mrqst_index].pkt_corr_info.nrx_chains = 3;
         */
        mrqst_index++;
        if(mrqst_index >= WPC_MAX_CONCURRENTREQ)
            mrqst_index = 0;

    } else {
        PRIN_LOG("%s: %d: Error: Insufficient buffering for requests %d \n", __FUNCTION__, __LINE__, mrqst_index);
        send_empty_mresponse(mrqst->request_id, REQUEST_THROTTLE);
    }

    //Pass Measurement Request to the driver: The request is changed from Type 0 to Type 1
    PRIN_LOG("%s: %d: Received a Type 1 Measurement Request from Server\n", __FUNCTION__, __LINE__);
    PRIN_LOG("%s: %d: Will send Type 1 Request to Driver\n", __FUNCTION__, __LINE__);

    return 0;
}

int wpc_mgr_proctype0mresp(char * buffer, int recv_len) 
{
    int chainCnt = 0;
    wpc_probe_data *pd;

	//The server request is for a message of Type 0. However, the request is always sent to the driver as Type 1.
	//Hence extract the Type 1 fields from the response.
    struct nsp_mresp *mresp = (struct nsp_mresp *)((char *)buffer + sizeof(struct nlmsghdr) + NSP_HDR_LEN);
    struct nsp_type1_resp *resp = (struct nsp_type1_resp *)((char *)buffer + sizeof(struct nlmsghdr) + NSP_HDR_LEN + MRES_LEN);
    int index = -1;
    int count =0;
    int probeindex =0;
	u_int8_t rxchains =0;
    int written = 0;


	for(count =0; count < WPC_MAX_CONCURRENTREQ; count++ ) {
		//Look up request 
		if(mresp->request_id == mrqst_state[count].mrqst.request_id && mrqst_state[count].valid == 1) {
			index = count;
			break;
		}
	}
	if(index == -1) {
		PRIN_LOG("%s: %d Could not find entry for response in state table.\n", __FUNCTION__, __LINE__);
		return -1;
	}

	if((mrqst_state[index].type != NSP_TYPE0RESP ) || (mrqst_state[index].valid != 1)) {
		PRIN_LOG("%s: %d: Error: Should be a Type 0 Resp and valid: %d %d  \n", __FUNCTION__, __LINE__, mrqst_state[index].type, mrqst_state[index].valid );
		return -1;
	}

	
	//Store data
    probeindex = mrqst_state[index].proberespcount;
    pd =  &(mrqst_state[index].probedata[probeindex]);
    mrqst_state[index].proberespcount++;
		
	pd->tod = resp->tod;
	pd->toa = resp->toa;
    /* We keep track of RTT from all the chains, and in the end we use it for outliers */
    for (chainCnt = 0; chainCnt < WPC_MAX_CHAINS; chainCnt++)
        pd->rtt[chainCnt] = resp->toa - resp->tod;
	pd->txrate = resp->send_rate;

	memcpy(pd->rssi, resp->rssi, sizeof(u_int8_t)*WPC_MAX_CHAINS);
	memcpy(pd->cd, resp->type1_payld, sizeof(u_int8_t)*TYPE1PAYLDLEN);
//	rxchains = (u_int8_t) ((mrqst_state[index].mrqst.mode & RX_CHAINMASK_3) >> 7);
    rxchains =  ieee80211_mask2numchains[(resp->no_of_chains & (RX_CHAINMASK_HASH >>8))];
	PRIN_LOG("%s: %d: Received a measurement response from driver. RTT: %lf RSSI: %d %d %d\n", __FUNCTION__, __LINE__, pd->frtt, pd->rssi[0], pd->rssi[1], pd->rssi[2]);
	PRIN_LOG("%s rxchains: %d, mrqst.ode: %x \n", __func__, rxchains, mrqst_state[index].mrqst.mode);
	//Calculate Type 0 correction
    wpc_rtte_calculate_ftoa(pd, rxchains);

    printf("mrqst_state[index].mrqst.no_of_measurements %d == mrqst_state[index].proberespcount %d\n", mrqst_state[index].mrqst.no_of_measurements , mrqst_state[index].proberespcount);

	//If all probes have been received, then send response
	if(mrqst_state[index].mrqst.no_of_measurements == mrqst_state[index].proberespcount) {
       	
		PRIN_LOG("%s: %d: Received all measurement responses from driver. Sending Measurement Response to server Index: %d\n", __FUNCTION__, __LINE__, index);
	
		//Calculate statistics
		wpc_mgr_calcstats(index);

        if ( recv_len ) {
            written = wpc_write(wpc_server_sock, ((char *)buffer)+sizeof(struct nlmsghdr), recv_len);
            if ( written < recv_len) {
                // TBD : Need to write pending data if there is partial write 
                printf("Partial write closing connection Size: %d Written: %d\n", sizeof(struct nsp_header)+sizeof(struct nsp_mresp), written);
                if(wpc_server_sock > 0)
                {
                    wpc_close(wpc_server_sock);
                    FD_CLR(wpc_server_sock, &read_fds);
                    wpc_server_sock = -1;
                }
            }
        }

		//Send response
		wpc_mgr_sendtype0resp(index);
	
		//Delete state entry	
		wpc_mgr_initstate(index);
        return -1;

	} else {
		PRIN_LOG("%s: %d: Waiting for more measurement responses from driver \n", __FUNCTION__, __LINE__);
		//Extend the timeout period
		gettimeofday(&(mrqst_state[index].starttime), NULL);
	}


	return 1;
}

int wpc_mgr_proctype1mresp(char *buffer) 
{
    struct nsp_mresp *mresp = (struct nsp_mresp *)((char *)buffer + sizeof(struct nlmsghdr) + NSP_HDR_LEN);
    int index = -1;
    int count =0;
    PRIN_LOG("Processing Type1 resp\n");
        
    for(count =0; count < WPC_MAX_CONCURRENTREQ; count++ ) {
        //Look up request 
        if(mresp->request_id == mrqst_state[count].mrqst.request_id && mrqst_state[count].valid == 1) {
            index = count;
            break;
        }
    }
    if(index == -1) {
        PRIN_LOG("%s: %d Could not find entry for response in state table.\n", __FUNCTION__, __LINE__);
        return -1;
    }

    mrqst_state[index].proberespcount++;
    /* QUIPS - 342 */
    if(mresp->result == REQUEST_THROTTLE){
        PRIN_LOG("%s[%d]: rx empty response from Driver [%d]: C: %d\n", __func__, __LINE__,
                    mresp->request_id,  mrqst_state[index].proberespcount);
            if( mrqst_state[index].proberespcount == mrqst_state[index].mrqst.no_of_measurements ) {
                mresp->result = 0;
                mrqst_state[index].valid = 0;
                PRIN_LOG("%s[%d]: Recieved all responses. Cancelling timeout \n", __func__, __LINE__);
                send_empty_mresponse(mrqst_state[index].mrqst.request_id, REQUEST_TIMEOUT);
                 wpc_mgr_initstate(index);
        }   
        return -2;
    }

    if( mrqst_state[index].proberespcount == mrqst_state[index].mrqst.no_of_measurements ) {
                mresp->result = 0;
                mrqst_state[index].valid = 0;
                PRIN_LOG("Recieved all responses. Cancelling timeout \n");
				wpc_mgr_initstate(index);
    }
    else
        mresp->result = 1;

    return 1;
}

//Check to see if server expect a response of Type 0 or Type 1
int wpc_mgr_checkresptype(char *buffer) 
{
	int index = -1;
	int count =0;
    struct nsp_mresp *mresp = (struct nsp_mresp *)((char *)buffer + sizeof(struct nlmsghdr) + NSP_HDR_LEN);

	//Check to see what type of resposne shoudl be sent to the server.
	//The WPC <-> Driver exchange only  Type 1 messages. However the Server 
        //may have requested a Type 0 response. If so, the WPC needs to process and convert the message
	for(count =0; count < WPC_MAX_CONCURRENTREQ; count++ ) {
		//Look up request 
		if((mresp->request_id == mrqst_state[count].mrqst.request_id) && mrqst_state[count].valid == 1) {
			index = count;
			break;
		}
	}
	PRIN_LOG("%s: %d: Found state index: %d\n", __FUNCTION__, __LINE__, index);
	if (index == -1) {
		//Request not logged in state array. WPC does not log Type1 requests.
		PRIN_LOG("%s: %d: Could find any index to locate this response \n", __FUNCTION__, __LINE__);
		return -1;	
	}

    if(mrqst_state[index].type == NSP_TYPE1RESP ) {
        //Server has requested a Type 1 message. Forward this response to the Server
        PRIN_LOG("%s: %d: Received a Type 1 Measurement Response\n", __FUNCTION__, __LINE__);
        PRIN_LOG("%s: %d: Will send Type 1 Response to Server\n", __FUNCTION__, __LINE__);
		return 1;
	} else if(mrqst_state[index].type == NSP_TYPE0RESP ) {
		//Server has requested a Type 0 message. Perform processing and Forward this response to the Server
		PRIN_LOG("%s: %d: Received a Type 1 Measurement Response\n", __FUNCTION__, __LINE__);
		PRIN_LOG("%s: %d: Will send Type 0 Response to Server\n", __FUNCTION__, __LINE__);
		return 0;	
	}
    return 0;	
}

char * wpc_mgr_createtype0resp(int index)
{
	struct nsp_header nsp_hdr;
	struct nsp_type0_resp resp={0};
	char * buf = malloc(NSP_HDR_LEN + TYPE0RES_LEN);
	PRIN_LOG("%s: %d: Creating Type 0 response for index: %d \n", __FUNCTION__, __LINE__, index);

	memset(buf, 0, (NSP_HDR_LEN + TYPE0RES_LEN));
	nsp_hdr.SFD = START_OF_FRAME;
	nsp_hdr.version = NSP_VERSION;
	nsp_hdr.frame_type = NSP_TYPE0RESP;
	nsp_hdr.frame_length = TYPE0RES_LEN;

	resp.request_id = mrqst_state[index].mrqst.request_id;
	memcpy(resp.sta_mac_addr, mrqst_state[index].mrqst.sta_mac_addr, ETH_ALEN); 
	if ( mrqst_state[index].proberespcount > 0) 
		resp.result = 0;
	else
		resp.result = 1;
	resp.rtt_mean = mrqst_state[index].rtt_mean;
	resp.rtt_stddev =  mrqst_state[index].rtt_stddev;
	resp.rtt_samples = mrqst_state[index].proberespcount;
	resp.rssi_mean = mrqst_state[index].rssi_mean;
	resp.rssi_stddev = mrqst_state[index].rssi_stddev;
	resp.rssi_samples = mrqst_state[index].proberespcount *WPC_MAX_CHAINS;

	memcpy(buf, &nsp_hdr, NSP_HDR_LEN);
	memcpy((buf + NSP_HDR_LEN), &resp, TYPE0RES_LEN);

	return buf;

}
	
int wpc_mgr_sendtype0resp(int index) 
{


	int recv_len, written;
	int count;
	char * buffer = wpc_mgr_createtype0resp(index);

	//Send Type0 message to Server
	if(mrqst_state[index].proberespcount > 0) {
		PRIN_LOG("%s: %d: Sending Type 0 response for index: %d Result : Success\n", __FUNCTION__, __LINE__, index);
		recv_len = NSP_HDR_LEN + TYPE0RES_LEN;
		written = wpc_write(wpc_server_sock, ((char *)buffer), recv_len);
		if ( written < recv_len) {
       		 	// TODO : Need to write pending date if there is partial write 
		        printf("Partial write Closing connection!!!!! Size %d written %d\n",
                                        sizeof(struct nsp_header)+sizeof(struct nsp_mresp), written);

                if(wpc_server_sock > 0) {
                    FD_CLR(wpc_server_sock, &read_fds);
                    wpc_close(wpc_server_sock);
                    wpc_server_sock = -1;
                }
        }


		//Delete all other responses (Free memory)
		for (count =0; count < mrqst_state[index].proberespcount; count++) {
		//TBD
		}
	} else {
		PRIN_LOG("%s: %d: Sending Type 0 response for index: %d Result : Fail\n", __FUNCTION__, __LINE__, index);

		//If no responses, result = fail
		//TBD
	}

	//TBD
	free(buffer);
	return 1;

}

int wpc_mgr_calcstats(int index) 
{

	PRIN_LOG("%s: %d: Calculating stats for index: %d \n", __FUNCTION__, __LINE__, index);
	//RTT and RSSI mean
	int count, i;
    double rtt_dev = 0.0, rssi_dev = 0.0, rssi_dev_t = 0.0;
	wpc_probe_data *pd;
	for (count =0; count < mrqst_state[index].proberespcount; count++) {
    		pd =  &(mrqst_state[index].probedata[count]);
		mrqst_state[index].rtt_mean+= pd->frtt;
		for(i =0; i < WPC_MAX_CHAINS; i++)
			mrqst_state[index].rssi_mean+=pd->rssi[i];
	}
	mrqst_state[index].rtt_mean=  mrqst_state[index].rtt_mean/mrqst_state[index].proberespcount; 
	mrqst_state[index].rssi_mean= mrqst_state[index].rssi_mean/(mrqst_state[index].proberespcount *WPC_MAX_CHAINS); 
	PRIN_LOG("%s: %d: RTT Mean: %f RSSI mean %f\n", __FUNCTION__, __LINE__, mrqst_state[index].rtt_mean, mrqst_state[index].rssi_mean);
	//RTT and RSSI Stddev
    if(mrqst_state[index].proberespcount >1) {
        for (count = 0; count < mrqst_state[index].proberespcount; count++) {
            pd =  &(mrqst_state[index].probedata[count]);
            rtt_dev += pow(pd->frtt - mrqst_state[index].rtt_mean, 2);
            for(i =0; i < WPC_MAX_CHAINS; i++)
                rssi_dev_t +=pd->rssi[i];
            rssi_dev_t = rssi_dev_t/WPC_MAX_CHAINS;
            rssi_dev += pow(rssi_dev_t - mrqst_state[index].rssi_mean, 2);
        }
        mrqst_state[index].rtt_stddev = sqrt(rtt_dev/mrqst_state[index].proberespcount);
        mrqst_state[index].rssi_stddev = sqrt(rssi_dev/mrqst_state[index].proberespcount);
    	PRIN_LOG("%s: %d: RTT STDDEV: %f RSSI STDDEV %f\n", __FUNCTION__, __LINE__, mrqst_state[index].rtt_stddev, mrqst_state[index].rssi_stddev);
    }
	//TBD

	return 1;
}



//WPC Timeout handler: Will check all pednign requests and see if any repsonses need to be sent

int wpc_mgr_handletimeout()
{
	int count;
	struct timeval tv;
	int elapsed;
    int timers = 0;
	gettimeofday(&tv, NULL);
	//Iterate through pending requests. Check if any of them have timed out.
//    printf("\n in wpc_mgr_timeout\n");
	for(count =0; count < WPC_MAX_CONCURRENTREQ; count++ ) {

		if(mrqst_state[count].valid == 1) {
            timers++;
			elapsed = (tv.tv_sec - mrqst_state[count].starttime.tv_sec) * 1000;
			elapsed += (tv.tv_usec - mrqst_state[count].starttime.tv_usec) / 1000;
/*
			PRIN_LOG("%s: %d: Checking index: %d Elapsed time: %d and type is %d\n the probe_count is %d and mrqst is %d\n and timeout is set as %d\n", __FUNCTION__, __LINE__, count, elapsed, mrqst_state[count].type, mrqst_state[count].proberespcount, mrqst_state[count].mrqst.no_of_measurements, mrqst_state[count].timeout/1000);
			PRIN_LOG("%s: %d: Checking index: %d Starttime : %d %d\n", __FUNCTION__, __LINE__, count, mrqst_state[count].starttime.tv_sec, mrqst_state[count].starttime.tv_usec);
			PRIN_LOG("%s: %d: Checking index: %d Current time : %d %d\n", __FUNCTION__, __LINE__, count, tv.tv_sec, tv.tv_usec);
*/
			if(elapsed > mrqst_state[count].timeout/1000) {
				PRIN_LOG("%s: %d: Request timed out. Will send Type 0 Response to Server\n", __FUNCTION__, __LINE__);
//Send response
                if (mrqst_state[count].type == NSP_TYPE0RESP)
                {
                    //Calculate statistics
                    PRIN_LOG("%s: %d: Request timed out. Will send Type 0 Response to Server\n", __FUNCTION__, __LINE__);
                    wpc_mgr_calcstats(count);
                    wpc_mgr_sendtype0resp(count);
                    mrqst_state[count].valid = 0;
                }
                else if (mrqst_state[count].proberespcount <  mrqst_state[count].mrqst.no_of_measurements &&  mrqst_state[count].type == NSP_TYPE1RESP)
                {
                    send_empty_mresponse(mrqst_state[count].mrqst.request_id, REQUEST_TIMEOUT);
                    mrqst_state[count].valid = 0;
                    PRIN_LOG("Sending Empty Mresponse\n");
                }
                else if (mrqst_state[count].type == NSP_TSFRESP)
                {
                    send_empty_tsfresponse(mrqst_state[count].mrqst.request_id, REQUEST_TIMEOUT);
                    mrqst_state[count].valid = 0;
                    PRIN_LOG("Sending Empty TSF Reponse\n");
                }
                else {
                    PRIN_LOG("Doesn't meet any condition mrqst_state[mrqst_index].type %d\n",mrqst_state[count].type);
                }
                //Delete state entry	
                wpc_mgr_initstate(count);
            }
        }
    }
    return 0;
} 


