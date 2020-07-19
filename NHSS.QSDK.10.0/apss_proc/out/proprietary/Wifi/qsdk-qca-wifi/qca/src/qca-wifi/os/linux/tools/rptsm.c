/*
 * Copyright (c) 2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include "rpttool.h"

/* Master state machine states */ 
#define MASTER_INIT_ST    0
#define MASTER_SEL_SLAVE  1 
#define MASTER_SLAVE_CM1  2
#define MASTER_WAIT_ACK1  3
#define MASTER_CM_STA     4
#define MASTER_SLAVE_CM2  5
#define MASTER_WAIT_ACK2  6
#define MASTER_CM_AP      7
#define MASTER_CHECK_ST   8
#define MASTER_ROUTE_ST   9
#define MASTER_TXROUTE_ST 10 
#define MASTER_TXASSOC_ST 11
#define MASTER_WAIT_ACK3  12
#define MASTER_DONE_ST    13

/* Slave state machine states */
#define SLAVE_INIT_ST   0
#define SLAVE_SEND_ACK1 1
#define SLAVE_CM_AP     2
#define SLAVE_GPUT_EST  3
#define SLAVE_WAIT_ACK1 4
#define SLAVE_CM_STA    6
#define SLAVE_SEND_ACK2 7
#define SLAVE_WAIT_ACK2 8
#define SLAVE_DONE_ST   9


int  mode, master, interface;
int  num_stas_trained;
char self_macaddr[18];
char rootap_macaddr[18];
char rpt_macaddr[18];
u_int32_t min_gput;
int  sta_count;
char sta_macaddr[4][18];
u_int32_t goodput[4][2];
u_int8_t rpt_status;
u_int8_t status;
u_int8_t assoc;
int goodput_table[4];
int routing_table[4];
u_int32_t  sta_route;

#define minby2(a,b)  (a <= b) ? a/2 : b/2

/* This function is used to set the device addresses */
void init_params()
{

    get_set_addr(1); 
    if (mode == 0) { 
        get_set_addr(2);
    }
    get_set_addr(3);
} 

/* This function is used to set up training */
int init_training()
{
    FILE *fp;
    int  count;
    char buff[8];
    num_stas_trained = 0; 

    fp = fopen("StaAddr", "r");
    if (fp == NULL){
       printf("File StaAddr does not exist. Exiting....\n");
       exit(0); 
    }
    buff[0] = '\0';
    if(fgets(buff, sizeof(buff)/sizeof(buff[0]), fp) != NULL) {
        sta_count = atoi(buff);
    }
    else {
        printf("Error reading StaAddr file\n");
        goto err;
    }
    if (sta_count > 4) {
        printf("STA count cannot exceed 4. Exiting..\n");
        exit(0);
    }
    for (count = 0; count <  sta_count; count++) {
        if(fgets(sta_macaddr[count], sizeof(sta_macaddr)/sizeof(sta_macaddr[0]), fp) == NULL) {
            printf("Error reading file StaAddr\n");
            goto err;
        }
    } 
    fclose(fp);

    fp = fopen("RootAPAddr", "r");
    if (fp == NULL){
       printf("File RootAPAddr does not exist. Exiting....\n");
       exit(0); 
    }
    if(fgets(rootap_macaddr, sizeof(rootap_macaddr)/sizeof(rootap_macaddr[0]), fp) == NULL)
    {
        printf("Error reading file RootAPAddr\n");
        goto err;
    }
    fclose(fp); 

    if (mode == 0) {
        status = 0;
        assoc  = 0; 
        fp = fopen("RptAddr", "r");
        if (fp == NULL) {
            printf("File RptAddr does not exist. Exiting....\n");
            exit(0); 
        }
        if(fgets(rpt_macaddr, sizeof(rpt_macaddr)/sizeof(rpt_macaddr[0]), fp) == NULL) {
            printf("Error reading file RptAddr\n");
            goto err;
        }
        fclose(fp);
    }

    fp = fopen("MinGoodput", "r");
    if (fp == NULL){
       printf("File MinGoodput not found. Setting minimum goodput req to 20Mbps....\n");
       min_gput = 20000;
       return; 
    }
    buff[0] = '\0';
    if(fgets(buff, sizeof(buff)/sizeof(buff[0]), fp) != NULL) {
        min_gput = atoi(buff);
    }
    else {
        printf("Error reading MinGoodput\n");
        goto err;
    }
    fclose(fp);
    return 0;

err:
    fclose(fp);
    exit(0);
}

int beacon_sent()
{
    char  devstr1[4], devstr2[18];
    int   devstatus = 0; 
    int   i;
    FILE *fp;
    /* Wait for 1st beacon */
    while (1) {
        fp = fopen("devup.txt", "r");
        if (fp != NULL) {
            fclose(fp); 
            system("\\rm devup.txt");
        } 
        system("iwpriv ath0 getrptdevup >> devup.txt");
        fp = fopen("devup.txt", "r");
        if(fgets(devstr1, sizeof(devstr1)/sizeof(devstr1[0]), fp) == NULL) {
            printf("Error reading devup\n");
            goto err;
        }
        if(fgets(devstr2, sizeof(devstr2)/sizeof(devstr2[0]), fp) == NULL) {
            printf("Error reading devup\n");
            goto err;
        }
        devstatus = (int) devstr2[12];
        devstatus = devstatus - 48; 
        fclose(fp);
        if (devstatus == 1) {
            system("\\rm devup.txt");
            break;
        }
        sleep(1);
        i++;
        if (i == 120) {
           printf("No beacons transmitted\n");
           return -1;
        }
    } 
    printf("Waiting 60s for STAs to assoc\n");
    sleep(60);
    return 1;

err:
    fclose(fp);
    exit(0);
}

/* This funcion is used to bring up the device in AP mode and waits for 
   the event where the 1st beacon is sent. After this event it waits for 
   60s allowing STAs to associate */
void start_ap()
{
    system("./startap");
    system("iwpriv ath0 rptgputmode 0");
    if (beacon_sent() == -1) {
        printf("Aborting training...\n");
        exit(0);
    }
    init_params();
    /* Enable custom protocol reception */
    system("iwpriv ath0 rptcustproto 1"); 
}

/* Get the device MAC address */
int get_self_mac_addr()
{
    FILE *fp;
    int i;
    system("ifconfig wifi0 > mac_addr.txt");
    fp = fopen("mac_addr.txt", "r");
    if (fp == NULL) {
        printf("Error in running ifconfig. exiting ...\n");
        exit(0);
    } else {
        if(fgets(self_macaddr, sizeof(self_macaddr)/sizeof(self_macaddr[0]), fp) == NULL) {
            printf("error reading mac_adr\n");
            fclose(fp);
            exit(0);
        }
        fclose(fp);
    }

    for (i = 0; i < 17; i++) {
        if (isupper(self_macaddr[i])) {
            self_macaddr[i] = tolower(self_macaddr[i]);
        } else if (self_macaddr[i] == '-') {
            self_macaddr[i] = ':';  
        } 
    }
    self_macaddr[17] = '\0'; 

    if (mode == 1) {
        strcpy(rpt_macaddr, self_macaddr);
    }  
}

/* This function is used to select the slave device that will start training to build its
   goodput table. In case of the repeater placement mode, the slave device will be the 
   repeater. */
int set_slave_dev()
{
    if (mode == 1) {
        if (num_stas_trained < sta_count) { 
            strcpy(rpt_macaddr, sta_macaddr[num_stas_trained]);
            num_stas_trained++;
            return 1;
        }
        return 0;
    } 
    return 1;
} 

/* This function is used to set the RootAP MAC address */
int set_rootap_addr()
{
    u_int8_t mac_addr[6];
    char tmp_rootap_macaddr[18]; 

    strcpy(tmp_rootap_macaddr, rootap_macaddr);
    mac_addr_from_string(tmp_rootap_macaddr, mac_addr);
    set_iwpriv_mac(mac_addr, 5);
    return 1;
} 

/* This function is used to set the repeater MAC address */
int set_rpt_addr()
{
    u_int8_t mac_addr[6];
    char tmp_rpt_macaddr[18];
    strcpy(tmp_rpt_macaddr, rpt_macaddr);
    mac_addr_from_string(tmp_rpt_macaddr, mac_addr);
    set_iwpriv_mac(mac_addr, 6); 
    return 1;
}

/* This function is used to switch the AP to STA mode during RPT training */
void master_cm_sta()
{
    system("./startsta");
    init_params(); 
    set_rpt_addr();
    system("iwpriv ath0 rptgputmode 0");
    system("iwpriv ath0 rptcustproto 1");  
}

/* Function to bring up a device in STA mode.*/
void start_sta()
{
    system("./startsta");
    init_params();
    get_self_mac_addr(); 
    set_rpt_addr();  
    system("iwpriv ath0 rptgputmode 1");
    /* Enable custom protocol reception */      
    system("iwpriv ath0 rptcustproto 1"); 
}

/* This function is used to switch the device mode from STA to AP. It waits 
   for the event where 1st beacon is sent. After this event it waits for 60s
   allowing STAs to associate */
void sta_cm_ap()
{
    system("./startap");
    init_training();
    set_rootap_addr();
    set_rpt_addr();
    system("iwpriv ath0 rptgputmode 1"); 
    /* Wait for 1st beacon */
    if (beacon_sent() == -1) {
        printf("Aborting training...\n");
        exit(0);
    }
    /* Enable custom protocol reception */      
    system("iwpriv ath0 rptcustproto 1"); 
}


/* This function is used to initialize the goodput & routing table used in routing 
   table mode*/
void init_tables()
{
    int i, j, l;
    char mac_addr[18];
    u_int8_t mac_addr_tmp[6];
    char buffer[50];
    char mac[11];
 
    for (i = 0; i < 5; i++) {
        goodput_table[i] = 0;
        routing_table[i] = 0;
    }
    sprintf(buffer, "iwpriv ath0 rptnumstas %d", sta_count);
    system(buffer);
    get_set_addr(3);
    sta_route = 0;  
}

/* This function is used to determine the optimal routing to be used.
   It updates the goodput and routing tables. The goodput table 
   records goodputs from Master to each of the Slaves, either through
   direct link or 1 hop. The routing table keeps track of how a packet
   from the master can be routed to a slave. In case no connection can 
   be established bwn a Master & Slave (either direct or 1 -hop) that 
   meets the minimum goodput requirements, the corresponding entry will
   be set to 0. If a direct link can be set up bwn the Master & a slave
   the entry corresponding to this will be set to 5. 
   In case a 1 hop routing is to be deployed, the corresponding entry 
   in the routing table will be set to the number of the STA that will
   act as a repeater */
int create_tables()
{
    FILE *fp;
    int i, j, k, l, tmp_sta_count;
    char  macaddr[18], filename[20];
    u_int32_t  sta_gput, one_hop_gput;
    char  buffer[50];
    char buff[8];

    /* Update the Master's goodput & routing table */ 
    fp = fopen("RptGputCalc.txt", "rt");
    if (fp == NULL) {
        printf("Goodput not recorded at Master. Run rpttool -h. Exiting...\n");
        exit(0);
    } else {
        buff[0] = '\0';
        if(fgets(buff, sizeof(int), fp) != NULL) {
            tmp_sta_count = atoi(buff);
        }
        else {
                printf("Error reading tmp_sta_count\n");
                goto err;
            }
        for (i = 0; i < tmp_sta_count; i++)  {
            if(fgets(macaddr, sizeof(macaddr)/sizeof(macaddr[0]), fp) == NULL) {
                printf("Error reading macaddr\n");
                goto err;
            }
            buff[0] = '\0';
            if(fgets(buff, sizeof(buff)/sizeof(buff[0]), fp) != NULL) {
                sta_gput = atoi(buff);
            }
            else {
                printf("Error reading sta_gput\n");
                goto err;
            }
            for (j = 0; j < 4; j++) {
                if (strcmp(macaddr, sta_macaddr[j]) == 0) {
                    if (sta_gput > min_gput*5/4) { 
                        goodput_table[j] = sta_gput;
                        routing_table[j] = 5;
                    }
                }
            }
        } 
        fclose(fp); 
    }

    /* Read goodput tables from the STAs & update goodput, routing tables at AP */
    for (k = 1; k <= sta_count; k++) {
        if (goodput_table[k-1] < min_gput*5/4*2) {
            /* If Master-Slave BW is < 2x throughput req, this slave can't be
               used for packet forwarding. Move on to next slave in the list */ 
            continue;
        }
        sprintf(filename,"RxRptGput%d.txt", k);
        fp = fopen(filename, "rt");
        if (fp == NULL) {
            printf("Goodput not recorded at Slave %d. Moving to next Slave in list if any...\n", k);
            continue;
         } 
        buff[0] = '\0';
        if(fgets(buff, sizeof(buff)/sizeof(buff[0]), fp) != NULL) {
            tmp_sta_count = atoi(buff);
        }
        else {
            printf("Error reading tmp_sta_count\n");
            goto err;
        }
        /* Update cur row in goodput & routing tables */
        for (i = 0; i < tmp_sta_count; i++)  {
            if(fgets(macaddr, sizeof(macaddr)/sizeof(macaddr[0]), fp) == NULL) {
                printf("Error reading macaddr\n");
                goto err;
            }
            buff[0] = '\0';
            if(fgets(buff, sizeof(buff)/sizeof(buff[0]), fp) != NULL) {
                sta_gput = atoi(buff);
            }
            else {
                printf("Error reading sta_gput\n");
                goto err;
            }
            for (j = 0; j < 4; j++) {
                if (strcmp(macaddr, sta_macaddr[j]) == 0) {
                    if (sta_gput >= min_gput*5/4*2) {
                        one_hop_gput = minby2(goodput_table[k-1], sta_gput);
                        if (one_hop_gput >= goodput_table[j]) {
                            goodput_table[j] = one_hop_gput;
                            routing_table[j] = k - 1;
                        }
                    }
                }
            }
        }
        if (fp != NULL)
            fclose(fp); 
    }
    sta_route = 0;

    /* Create routing table info for each Slave */
    for (i = 0; i < sta_count; i++) { 
        sta_route += routing_table[i] << (4 * (3 - i));  
    }

    printf("\nGoodput Table\n");
    for (i = 0; i < sta_count; i++) {
        printf("%d ", goodput_table[i]);
    }
    printf("\nRouting Table\n");
    for (i = 0; i < sta_count; i++) {
        printf("%d ", routing_table[i]);
    } 
    printf("\n");
    /* Save routes for each STA. */
    sprintf(buffer, "iwpriv ath0 rptsta1route %d", sta_route);
    system(buffer);
    return 1;

err:
   fclose(fp);
   exit(0);
}

/* Parses through goodput table at the Master and goodput table received from Slave 
   and computes both AP-STA as well as AP-RPT-STA goodputs. It also ensure that the
   Master's goodput table has an entry for the Salve and the Slave's goodput table
   has an entry for the Master. It indicates this through the rpt_status flag. */
int update_ap_rpt_topology() 
{
    FILE *fp;
    char macaddr[18];
    u_int32_t ap_rpt_gput, ap_sta_gput, rpt_sta_gput;
    int i,j, sta_count; 
    char buff[8];
    init_training();

    fp = fopen("RptGputCalc.txt", "rt");
    if (fp == NULL) {
        printf("Goodput not recorded at Master device. Exiting...\n");
        exit(0);
    } else {
        buff[0] = '\0';
        if(fgets(buff, sizeof(buff)/sizeof(buff[0]), fp) != NULL) {
            sta_count = atoi(buff);
        }
        else {
            printf("Error reading sta_count\n");
            goto err;
        }
        for (i = 0; i < sta_count; i++)  {
            if(fgets(macaddr, sizeof(macaddr)/sizeof(macaddr[0]), fp) == NULL) {
                printf("Error reading macaddr \n"):
                goto err;
            }
            buff[0] = '\0';
            if(fgets(buff, sizeof(buff)/sizeof(buff[0]), fp) != NULL) {
            {
                ap_sta_gput = atoi(buff);
            }
            else {
                printf("error reading ap_sta_gput\n");
                goto err;
            }
            if (strcmp(macaddr, rpt_macaddr) == 0) {
                ap_rpt_gput  = ap_sta_gput;
                rpt_status   = 1;
            }
        }
        fclose(fp);
        fp = fopen("RptGputCalc.txt", "rt");
        buff[0] = '\0';
        if(fgets(buff, sizeof(buff)/sizeof(buff[0]), fp) != NULL) {
            sta_count = atoi(buff);
        }
        else {
            printf("error reading RptGputCalc\n");
            goto err;
        }
        for (i = 0; i < sta_count; i++)  {
            if(fgets(macaddr, sizeof(macaddr)/sizeof(macaddr[0]), fp) == NULL) {
                printf("error reading macaddr\n");
                goto err;
            }
            buff[0] = '\0';
            if(fgets(buff, sizeof(buff)/sizeof(buff[0]), fp) != NULL) {
                ap_sta_gput = atoi(buff);
            }
            else {
                printf("error reading ap_sta_gput\n");
                goto err;
            }
            for (j = 0; j < 4; j++) {
                if (strcmp(sta_macaddr[j], macaddr) == 0) {
                    goodput[j][0] = ap_sta_gput;
                    break;
                }
            }
        }
        fclose(fp);
    }

    fp = fopen("RxRptGput.txt", "rt");

    if (fp == NULL) {
        printf("Goodput table not received from slave. Exiting...\n");
        exit(0);
    } else {
        buff[0] = '\0';
        if(fgets(buff, sizeof(buff)/sizeof(buff[0]), fp) != NULL) {
            sta_count = atoi(buff);
        }
        else {
            printf("error reading sta_count\n");
            goto err;
        }
        for (i = 0; i < sta_count; i++)  {
            if(fgets(macaddr, sizeof(macaddr)/sizeof(macaddr[0]), fp) == NULL) {
                printf("error reading macaddr\n");
                goto err;
            }
            buff[0] = '\0';
            if(fgets(buff, sizeof(buff)/sizeof(buff[0]), fp) != NULL)
                rpt_sta_gput = atoi(buff);
            }
            else {
                printf("error reading rpt_sta_gput\n");
                goto err;
            }
            if (strcmp(macaddr, rootap_macaddr) == 0) {
                rpt_status = rpt_status | 0x02;
            }
        }
        fclose(fp);
        fp = fopen("RxRptGput.txt", "rt");
        buff[0] = '\0';
        if(fgets(buff, sizeof(buff)/sizeof(buff[0]), fp) != NULL) {
            sta_count = atoi(buff);
        }
        else {
            printf("error reading sta_count\n");
            goto err;
        }
        for (i = 0; i < sta_count; i++)  {
            if(fgets(macaddr, sizeof(macaddr)/sizeof(macaddr[0]), fp) == NULL) {
                printf("error reading macaddr\n");
                goto err;
            }
            buff[0] = '\0';
            if(fgets(buff, sizeof(buff)/sizeof(buff[0]), fp) !=NULL) {
                rpt_sta_gput = atoi(buff);
            }
            else {
                printf("error reading rpt_sta_gput\n");
                goto err;
            }
            for (j = 0; j < 4; j++) {
                if (strcmp(sta_macaddr[j], macaddr) == 0) {
                    goodput[j][1] = minby2(ap_rpt_gput, rpt_sta_gput);
                    break;
                }
            }
        }
    }
    printf("AP_RPT Topology Summary\n");
    for (i = 0; i < 4; i++) {
        printf("STA %d: %s %d %d\n", i+1, sta_macaddr[i], goodput[i][0], goodput[i][1]);
    }
    printf("Status: %d\n", rpt_status);
    return 1;

err:
    fclose(fp);
    exit(0);
}

/* Checks if the throughput requirements either directly bwn AP & STAs or AP-RPT-STAs meets
   the minimum goodput requirements. Also sets the status & association iwprivs */
void check_req_met()
{
    FILE *inFile, *fp;
    char buffer[50];
    int i;
 
    fp = fopen("MasterStatusAssoc.txt", "wt"); 
    if (rpt_status == 3) {
        for (i = 0; i < 4; i++) {
            /* 25% margin added in checking for goodput */ 
            if ((goodput[i][0] >= min_gput*5/4) || (goodput[i][1] >= min_gput*5/4)){
                status = status | (0x1 << i);
                if (goodput[i][0] >= goodput[i][1]) {
                   assoc = assoc | (0x1 << i);
                } else {
                   assoc = assoc | (0x1 << (i+4)); 
                }
            }  
        } 
        printf("Status = %x\n", status);
        printf("Assoc  = %x\n", assoc); 
        sprintf(buffer, "iwpriv ath0 rptstatus %d", (((sta_count & 0x0f) << 4) | status));
        system(buffer);
        sprintf(buffer, "iwpriv ath0 rptassoc %d", assoc);
        system(buffer);
        fprintf(fp, "%d\n", sta_count);
        fprintf(fp, "%d\n", status);
        fprintf(fp, "%d\n", assoc);
        fclose(fp);

    } else {
        printf("Status = 0x00\n");
    } 
}


/* Slave state m/c */
void slave_sm()
{
    u_int8_t cur_state = SLAVE_INIT_ST, nxt_state;
    FILE *fp;
    char  devstr1[4], devstr2[18], c;
    int   devstatus = 0, protmsg = 0, i, j, msg_rcvd = 0, retry = 0; 

    while(1) { 
        switch (cur_state) {

            case SLAVE_INIT_ST:
                printf("SLAVE INIT STATE\n");
                start_sta(); 
                for (i = 0; ; i++) {
                    /* Wait for reception of protocol msg #1 */ 
                    protmsg = rx_proto_msg(); 
                    if (protmsg == 1) {
                       printf("Received protocol message 1\n");
                       break;
                    }
                    sleep(1);
                }
                nxt_state = SLAVE_SEND_ACK1;
                retry     = 0;
                break;

            case SLAVE_SEND_ACK1:
                printf("SLAVE SEND ACK1\n");
                /* Send protocol message 2 as an ack to protocol message #1 
                   Message is sent 9 times with a gap of 1s. This is done to
                   increase the probability of successful reception at the 
                   Master. If the master does not receive protocol msg #2
                   within 3s, it re-transmits meesage # 1. It repeats this
                   process 3 times. Hence we attempt to send out the ack 
                   at least 9 times (3 retires * 3 secs per retry) */
                printf("Sending protocol message #2\n");
                for (i = 0; i < 9; i++) {
                    system("iwpriv ath0 rpttxprotomsg 2"); 
                    sleep(1);
                } 
                nxt_state = SLAVE_CM_AP;
                sleep(4); /* Delay disabling STA in case some msg 2 pkts not sent */ 
                break;

            case SLAVE_CM_AP:
                printf("SLAVE CM AP\n");
                /* Switch to AP mode */
                sta_cm_ap();
                nxt_state = SLAVE_GPUT_EST;
                retry     = 0;
                break;

            case SLAVE_GPUT_EST:
                printf("SLAVE GPUT EST\n");
                /* Send protocol message # 3 to (goodput table) to Master
                   If 3 retries have exceeded, assume that the Master is
                   beyond the STA's range & abort */
                if (retry < 3) { 
                    rpttoolListen(0,0, interface);
                    nxt_state = SLAVE_WAIT_ACK1;
                } else {
                    nxt_state = SLAVE_INIT_ST;
                    retry     = 0;
                }  
                break;

            case SLAVE_WAIT_ACK1:
                printf("SLAVE WAIT ACK1\n"); 
                for (i = 0; ; i++) {
                    /* Wait for reception of protocol msg #4 */ 
                    protmsg = rx_proto_msg(); 
                    if (protmsg == 4) {
                        printf("Received protocol message 4\n");
                        /* Send protocol message #5 as an ack to protocol message #4 
                           Message is sent 9 times with a gap of 1s. This is done to
                           increase the probability of successful reception at the 
                           Root AP. If the root AP does not receive protocol msg #5
                           within 3s, it re-transmits meesage # 1. It repeats this
                           process 3 times. Hence we attempt to send out the ack 
                           at least 9 times (3 retires * 3 secs per retry) */
                        printf("Sending protocol message 5\n");
                        for (j = 0; j < 9; j++) {
                            system("iwpriv ath0 rpttxprotomsg 5"); 
                            sleep(1);
                        } 
                        sleep(1); 
                        nxt_state = SLAVE_CM_STA;
                        break;
                    }
                    sleep(1);

                    /* If frame not received in 3 secs, assume that the tx
                       of goodput table had a problem & retry */
                    if (i == 3) {
                        nxt_state = SLAVE_GPUT_EST;
                        retry++;
                        break; 
                    } 
                }
                break;

            case SLAVE_CM_STA:
                printf("SLAVE CM STA\n");
                start_sta();
                if (mode == 0) {
                    rpttoolListen(2, 0, interface);
                    fp = fopen("SlaveStatusAssoc.txt", "r");
                    if (fp == NULL) {
                        printf("Did not receive training status & association from AP. Aborting training...\n");
                        nxt_state = SLAVE_INIT_ST;
                        retry     = 0; 
                    } else {
                        fclose(fp);  
                        nxt_state = SLAVE_SEND_ACK2;
                        retry     = 0;
                    }
                } else {
                    system("\\rm RoutingTable.txt");
                    rpttoolListen(3, 0, interface); 
                    fp = fopen("RoutingTable.txt", "r");
                    if (fp == NULL) {
                        printf("Did not receive routing table from AP. Aborting training...\n");
                        nxt_state = SLAVE_INIT_ST;
                        retry     = 0; 
                    } else {
                        fclose(fp);  
                        nxt_state = SLAVE_SEND_ACK2;
                        retry     = 0;
                    }
                }
                break;

            case SLAVE_SEND_ACK2:
                printf("SLAVE SEND ACK2\n");
                /* Send protocol message 8 as an ack to protocol message #6/7 
                   Message is sent 9 times with a gap of 1s. This is done to
                   increase the probability of successful reception at the 
                   Master. If the Master  does not receive protocol msg #8
                   within 3s, it re-transmits message # 6/7. It repeats this
                   process 3 times. Hence we attempt to send out the ack 
                   at least 9 times (3 retires * 3 secs per retry) */
                printf("Sending protocol message 8\n");
                for (i = 0; i < 9; i++) {
                    system("iwpriv ath0 rpttxprotomsg 8"); 
                    sleep(1);
                } 
                /* Wait for reception of protocol msg #9 */ 
                for (i = 0; ; i++) { 
                    protmsg = rx_proto_msg(); 
                    if (protmsg == 9) {
                        printf("Received protocol message 9\n");
                        nxt_state = SLAVE_DONE_ST;
                        sleep(3);
                        break;
                    }
                    sleep(1);
                    /* If frame not received in 500 secs, assume that the tx
                       of gtraining abort frame had a problem & abort */
                    if (i == 500) {
                        printf("Training end frame timeout. Aborting training....\n");
                        exit(0); 
                        break; 
                    }
                }
                break;

            case SLAVE_DONE_ST:
                printf("SLAVE DONE STATE\n");  
                printf("Training completed.\n");
                exit(0);
                break; 

             default: 
                break;
        }
        cur_state = nxt_state; 
    }
}

/* Master device state m/c */
void master_sm()
{
    u_int8_t cur_state = MASTER_INIT_ST, nxt_state;
    FILE *fp;
    char  devstr1[4], devstr2[18];
    char *tmpstr, *protmsgstr;
    int   devstatus = 0, protmsg, i, led_status = 0, assoc_status = 0;
    int   retry = 0, status = 0; 
    char  buffer[50], filename[20];

    while(1) { 
        switch (cur_state) {

            case MASTER_INIT_ST:
                printf("MASTER INIT STATE\n");
                /* Start AP - set RootAP Address, wait for STA assoc */  
                start_ap();
                /* Estimate goodput on all AP-STA links */
                rpttoolListen(0,0, interface); 
                nxt_state = MASTER_SEL_SLAVE;
                break;

            case MASTER_SEL_SLAVE:
                printf("MASTER SELECT SLAVE STATE\n");
                /* Select the slave device that has to estimate goodput on all its links */
                status = set_slave_dev();
                if (status == 1) {
                    nxt_state = MASTER_SLAVE_CM1;
                    retry     = 0;
                } else {
                    printf("Training on all STAs done...Processing tables\n");
                    nxt_state = MASTER_CHECK_ST;
                }  
                set_rpt_addr();
                break;

            case MASTER_SLAVE_CM1:
                printf("MASTER SLAVE CM1\n");

                /* Send protocol message # 1 to selected slave. If 3 retries have 
                   exceeded, assume that the chosen slave is beyond the master's 
                   range & move on to the next slave in the list */
                printf("Sending protocol message 1\n");
                if (retry < MAX_RETRIES) { 
                    nxt_state = MASTER_WAIT_ACK1;
                    system("iwpriv ath0 rpttxprotomsg 1");
                } else {
                    if (mode == 1) {
                        nxt_state = MASTER_SEL_SLAVE;
                        printf("Moving to next Slave device\n");
                        retry     = 0;
                    } else {
                        printf("Repeater device not responding. Aborting training ...\n");
                        exit(0);  
                    }
                }
                break;
 
            case MASTER_WAIT_ACK1:
                printf("MASTER WAIT ACK1\n");

                for (i = 0; ; i++) {
                    /* Wait for reception of protocol msg #2 */ 
                    protmsg = rx_proto_msg(); 
                    if (protmsg == 2) {
                       printf("Received protocol message 2\n");
                       nxt_state = MASTER_CM_STA;
                       sleep(8);
                       retry     = 0; 
                       break;
                    }
                    sleep(1);
                    /* If frame not received in 3 secs, assume that the tx
                      of protocol message #1 had a problem & retry */
                    if (i == 3) {
                        nxt_state = MASTER_SLAVE_CM1;
                        retry++;
                        break; 
                    } 
                } 
                break;

             case MASTER_CM_STA:
                printf("MASTER CM STA\n");
                /* Master switches to STA mode, while the chosen slave switches to
                   AP mode and estimates goodput on links to all STAs */
                master_cm_sta();
                system("\\rm RxRptGput.txt"); 
                /* Wait for goodput table from Slave (protocol msg #3) */
                rpttoolListen(1, 0, interface);
                fp = fopen("RxRptGput.txt", "r");

                if (fp == NULL) {
                    retry     = 0; 
                    if (mode == 1) {
                        printf("Did not receive goodput table from Slave. Moving to nxt Slave in list...\n");
                        nxt_state = MASTER_SEL_SLAVE;
                    } else {
                        printf("Did not receive goodput table from Slave. Aborting...\n");
                        exit(0);
                    }
                } else {
                    fclose(fp);
                    if (mode == 1) { 
                        /* Save received goodput table with Slave number */
                        snprintf(devstr1,sizeof(devstr1),"%d",num_stas_trained);
                        strcpy(filename, "RxRptGput\0");
                        strcat(filename, devstr1);
                        strcat(filename, ".txt\0");
                        sprintf(buffer, "mv RxRptGput.txt %s", filename);
                        system(buffer);
                    } 
                    nxt_state = MASTER_SLAVE_CM2;
                    retry     = 0;
                }
                break;

            case MASTER_SLAVE_CM2:
                printf("MASTER SLAVE CM2\n");

                /* Send msg to Slave asking it to switch back to STA mode */
                system("iwpriv ath0 rpttxprotomsg 4");
                printf("Sent protocol message 4\n");   

                /* If 3 retries have exceeded, assume that the chosen Slave is
                   beyond the AP's range & move on to the next Slave in the list */
                if (retry < MAX_RETRIES) { 
                    nxt_state = MASTER_WAIT_ACK2;
                } else {
                    if (mode == 1) { 
                        nxt_state = MASTER_SEL_SLAVE;
                    }
                    else {
                        printf("Did not receive ACK for message #4 from Slave. Aborting..\n");
                        exit(0);
                    }
                    retry = 0;
                } 
                break;

            case MASTER_WAIT_ACK2:
                printf("MASTER WAIT ACK2\n");
                for (i = 0; ; i++) {
                    /* Wait for reception of protocol msg #5 */ 
                    protmsg = rx_proto_msg();
                    if (protmsg == 5) {
                       printf("Received protocol message 5\n");
                       nxt_state = MASTER_CM_AP;
                       retry = 0;
                       sleep(8); 
                       break;
                    }
                    sleep(1);

                    if (i == 3) {
                        nxt_state = MASTER_SLAVE_CM2;
                        retry++;
                        break; 
                    }
                } 
                break;

            case MASTER_CM_AP:
                printf("MASTER CM AP STATE\n");
                start_ap(); 
                if (mode == 1) {
                    nxt_state = MASTER_SEL_SLAVE;
                } else {
                    nxt_state = MASTER_CHECK_ST;
                }
                break; 

            case MASTER_CHECK_ST:
                printf("MASTER CHECK STATE\n");
                if (mode == 1) {
                    init_tables();
                    create_tables();
                    nxt_state        = MASTER_ROUTE_ST;
                    num_stas_trained = 0; 
                    retry            = 0;
                    init_training(); 
                } else {
                    update_ap_rpt_topology(); 
                    check_req_met();
                    nxt_state        = MASTER_TXASSOC_ST;
                    num_stas_trained = 0; 
                    retry            = 0;
                }
                break; 

            case MASTER_ROUTE_ST:
                printf("AP ROUTES AVAILABLE STATE\n");
                status = set_slave_dev();
                if (status == 1) {
                    nxt_state = MASTER_TXROUTE_ST;
                    retry     = 0;
                } else {
                    printf("Routing table sent to all Slaves ...\n");
                    nxt_state = MASTER_DONE_ST;
                }  
                set_rpt_addr();
                set_rootap_addr();
                break;

            case MASTER_TXROUTE_ST:
                printf("MASTER SEND ROUTING TABLE STATE\n");
                /* Send protocol message # 7 to selected Slave.
                   If 3 retries have exceeded, assume that the chosen STA is
                   beyond the AP's range & move on to the next STA in the list */
                if (retry < MAX_RETRIES) { 
                    nxt_state = MASTER_WAIT_ACK3;
                    system("iwpriv ath0 rpttxprotomsg 7");
                    printf("Sent protocol message 7\n");
                } else {
                    printf("Did not receive ACK for routing message from slave. Moving to next node ... \n"); 
                    nxt_state = MASTER_ROUTE_ST; 
                    retry = 0;
                }
                break;

            case MASTER_TXASSOC_ST:
                printf("MASTER TX ASSOC STATUS\n");
                /* Send protocol message # 6 to selected Slave.
                   If 3 retries have exceeded, assume that the chosen STA is
                   beyond the AP's range & move on to the next STA in the list */
                if (retry < MAX_RETRIES) { 
                    nxt_state = MASTER_WAIT_ACK3;
                    system("iwpriv ath0 rpttxprotomsg 6");
                    printf("Sent protocol message 6\n");
                } else {
                    printf("Did not receive ACK for status & assoc from slave. Aborting ... \n"); 
                    retry = 0;
                    exit(0);
                } 
                break;

            case MASTER_WAIT_ACK3:
                printf("MASTER WAIT ACK3\n");
                for (i = 0; ; i++) {
                    /* Wait for reception of protocol msg #8 */ 
                    protmsg = rx_proto_msg(); 
                    if (mode == 1 && protmsg == 8) {
                       printf("Received protocol message 8\n");
                       nxt_state = MASTER_ROUTE_ST;
                       retry     = 0; 
                       sleep(8);
                       break;
                    } else if (mode == 0 && protmsg == 8) {
                       printf("Received protocol message 8\n");
                       nxt_state = MASTER_DONE_ST;
                       retry     = 0;
                       sleep(8); 
                       break;
                    }
                    sleep(1);
                    /* If frame not received in 3 secs, assume that the tx
                       of protocol message #6 or #7 had a problem & retry */
                    if (i == 3) {
                        if (mode == 0) {
                            nxt_state = MASTER_TXASSOC_ST;
                        } else {    
                            nxt_state = MASTER_TXROUTE_ST;
                        }
                        retry++;
                        break; 
                    } 
                } 
                break;  
               
            case MASTER_DONE_ST:
                printf("MASTER DONE STATE\n");
                for (i = 0; i < 3 ; i++) {
                    system("iwpriv ath0 rpttxprotomsg 9");
                }
                printf("Sent protocol message 9\n");
                printf("Training completed.\n");
                exit(0);
                break; 
 
            default: 
                break;

        }
        cur_state = nxt_state; 
    }
}


