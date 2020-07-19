/* 
Copyright (c) 2013 Qualcomm Atheros, Inc.
All Rights Reserved. 
Qualcomm Atheros Confidential and Proprietary. 
*/  

/*
 *  This app makes a DIAG bridge for standalone BT SoC devices to communicate with QMSL/QRCT
 *  QPST is a requirement to make connection with QMSL/QRCT 
 *  Requires at least four command line arguments:
 *  1. Server IP address
 *  2. Server port number
 *  3. Connection type, SERIAL or USB
 *  4. Connection details
 *      a. COMPORT number for SERIAL
 *      b. USB connection string for USB mode
 *  5. Baudrate, optional and used only in SERIAL mode, default is 115200
 *  6. SERIAL port handshaking, optional and used only in SERIAL mode, default is NONE, valid options are NONE, XONXOFF, RequestToSend, RequestToSendXONXOFF
 */
#include <stdio.h>      //printf
#include <stdlib.h>      //printf
#include <string.h>     //strlen
#include <sys/socket.h> //socket
#include <arpa/inet.h>  //inet_addr

#include <stdbool.h>
#include <limits.h>     //PATH_MAX
#include <stdlib.h>     //atoi
#include <pthread.h>    //thread
#include "global.h"

//#include <bluetooth/bluetooth.h>
//#include <bluetooth/hci.h>
//#include <bluetooth/hci_lib.h>

/*
#define IPADDRESS         "IPADDRESS"
#define PORT              "PORT"
#define IOTYPE            "IOType"
#define CONNECTIONDETAILS "ConnectionDetails"
#define BAUDRATE          "BAUDRATE"
#define HANDSHAKE         "HANDSHAKE"     //NONE: 0; XOnXoff: 1; RequestToSend: 2; RequestToSendXOnXOff: 3
#define PATCH             "PATCH"
#define NVM               "NVM"
*/
int sock;
int client_sock;
bool legacyMode = 1;
ConnectionType connectionTypeOptions;
char Device[PATH_MAX] = "";
int descriptor = 0; 
void *NotifyObserver(void * arguments);

struct arg_struct {
    int desc;
    unsigned char* buf;
	int size;
};

enum{
    IPADDRESS  = 1,
	PORT       = 2,
	IOTYPE     = 3,
	DEVICE     = 4,
	BAUDRATE   = 5,
};



// DK_DEBUG
#define UCHAR unsigned char
#define UINT8 unsigned char
#define UINT16 unsigned short int

#define MAX_EVENT_SIZE	260
#define MAX_TAGS              50
#define PS_HDR_LEN            4
#define HCI_VENDOR_CMD_OGF    0x3F
#define HCI_PS_CMD_OCF        0x0B
#define PS_EVENT_LEN 100
#define HCI_EV_SUCCESS        0x00
#define HCI_COMMAND_HEADER_SIZE 3
#define HCI_OPCODE_PACK(ogf, ocf) (UINT16)((ocf & 0x03ff)|(ogf << 10))
#define htobs(d) (d)

//CSR8811
//============================================================
// PSKEY
//============================================================
//UCHAR PSKEY_BDADDR[]					={0xc2, 0x02, 0x00, 0x0c, 0x00, 0x01, 0x00, 0x03, 0x70, 0x00, 0x00, 0x01, 0x00, 0x04, 0x00, 0x00, 0x00, 0x78, 0x00, 0x12, 0x90, 0x56, 0x00, 0x34, 0x12};
UCHAR PSKEY_BDADDR[]					={0xc2, 0x02, 0x00, 0x0c, 0x00, 0x01, 0x00, 0x03, 0x70, 0x00, 0x00, 0x01, 0x00, 0x04, 0x00, 0x00, 0x00, 0xc0, 0x00, 0xee, 0xff, 0x88, 0x00, 0x00, 0x00};
UCHAR PSKEY_ANA_FREQ[]					={0xc2, 0x02, 0x00, 0x09, 0x00, 0x02, 0x00, 0x03, 0x70, 0x00, 0x00, 0xfe, 0x01, 0x01, 0x00, 0x00, 0x00, 0x90, 0x65};
UCHAR PSKEY_ANA_FTRIM[]					={0xc2, 0x02, 0x00, 0x09, 0x00, 0x03, 0x00, 0x03, 0x70, 0x00, 0x00, 0xf6, 0x01, 0x01, 0x00, 0x00, 0x00, 0x1d, 0x00};
UCHAR PSKEY_ANA_FTRIM_READWRITE[]		={0xc2, 0x02, 0x00, 0x09, 0x00, 0x04, 0x00, 0x03, 0x70, 0x00, 0x00, 0x3f, 0x68, 0x01, 0x00, 0x00, 0x00, 0x1d, 0x00};
UCHAR PSKEY_BAUD_RATE[]					={0xc2, 0x02, 0x00, 0x0a, 0x00, 0x05, 0x00, 0x03, 0x70, 0x00, 0x00, 0xea, 0x01, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0xc2};
UCHAR PSKEY_INTERFACE_H4[]				={0xc2, 0x02, 0x00, 0x09, 0x00, 0x06, 0x00, 0x03, 0x70, 0x00, 0x00, 0xf9, 0x01, 0x01, 0x00, 0x00, 0x00, 0x03, 0x00};	
UCHAR PSKEY_COEX_SCHEME[]				={0xc2, 0x02, 0x00, 0x0a, 0x00, 0x07, 0x00, 0x03, 0x70, 0x00, 0x00, 0x80, 0x24, 0x01, 0x00, 0x00, 0x00, 0x03, 0x00};
UCHAR PSKEY_COEX_PIO_UNITY_3_BT_ACTIVE[]={0xc2, 0x02, 0x00, 0x0a, 0x00, 0x08, 0x00, 0x03, 0x70, 0x00, 0x00, 0x83, 0x24, 0x02, 0x00, 0x00, 0x00, 0x05, 0x00, 0x01, 0x00};
UCHAR PSKEY_COEX_PIO_UNITY_3_BT_STATUS[]={0xc2, 0x02, 0x00, 0x0a, 0x00, 0x09, 0x00, 0x03, 0x70, 0x00, 0x00, 0x84, 0x24, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00};
UCHAR PSKEY_COEX_PIO_UNITY_3_WLAN_DENY[]={0xc2, 0x02, 0x00, 0x0a, 0x00, 0x0a, 0x00, 0x03, 0x70, 0x00, 0x00, 0x85, 0x24, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00};
//============================================================
// BCCMD
//============================================================
UCHAR BCCMD_WARM_RESET[]		={0xc2, 0x02, 0x00, 0x09, 0x00, 0x20, 0x00, 0x02, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};	
UCHAR BCCMD_WARM_HALT[]			={0xc2, 0x02, 0x00, 0x09, 0x00, 0x21, 0x00, 0x04, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};	
UCHAR BCCMD_ENABLE_TX[]			={0xc2, 0x02, 0x00, 0x09, 0x00, 0x22, 0x00, 0x07, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};	
UCHAR BCCMD_DISABLE_TX[]		={0xc2, 0x02, 0x00, 0x09, 0x00, 0x23, 0x00, 0x08, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};	
UCHAR BCCMD_COEX_ENABLE[]		={0xc2, 0x02, 0x00, 0x09, 0x00, 0x24, 0x00, 0x78, 0x48, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};	
UCHAR BCCMD_PAUSE[]				={0xc2, 0x02, 0x00, 0x09, 0x00, 0x25, 0x00, 0x04, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};	
UCHAR BCCMD_CONFIG_PACKAGE_DH1[]={0xc2, 0x02, 0x00, 0x09, 0x00, 0x26, 0x00, 0x04, 0x50, 0x00, 0x00, 0x17, 0x00, 0x0f, 0x00, 0x53, 0x01, 0x00, 0x00};	
//BCCMD_CONFIG_XTAL_FTRIM(default = 27, 0 ~ 0x3f) 
UCHAR BCCMD_CONFIG_XTAL_FTRIM[]	={0xc2, 0x02, 0x00, 0x09, 0x00, 0x27, 0x00, 0x04, 0x50, 0x00, 0x00, 0x1d, 0x00, 0x1b, 0x00, 0x00, 0x00, 0x00, 0x00};
UCHAR BCCMD_TXDATA1_2402[]		={0xc2, 0x02, 0x00, 0x09, 0x00, 0x28, 0x00, 0x04, 0x50, 0x00, 0x00, 0x04, 0x00, 0x62, 0x09, 0x32, 0xff, 0x00, 0x00};	
//0x5004 = VarID = Radiotest (BCCMD)
//0x0013 = TestID=19 = BER1 (BCCMD)
//0x0962 = 2402
//0x0001 = highside
//0x0000 = attn
UCHAR BCCMD_BER1_2402[]			={0xc2, 0x02, 0x00, 0x09, 0x00, 0x29, 0x00, 0x04, 0x50, 0x00, 0x00, 0x13, 0x00, 0x62, 0x09, 0x01, 0x00, 0x00, 0x00};	
UCHAR BCCMD_RXDATA1_2402[]		={0xc2, 0x02, 0x00, 0x09, 0x00, 0x2a, 0x00, 0x04, 0x50, 0x00, 0x00, 0x08, 0x00, 0x62, 0x09, 0x01, 0x00, 0x00, 0x00};	

static bool nopatch = false;
bool QCA_DEBUG = true;
static char *soc_type = "csr8811";

int read_incoming_events(int fd, unsigned char* buf, int to){
	int r,size;
	int count = 0;

	do{
		//20160617_Austin
		//CSR8811
		//if((!strcasecmp(soc_type, "rome")) || (!strcasecmp(soc_type, "300x"))){
		if((!strcasecmp(soc_type, "rome")) || (!strcasecmp(soc_type, "300x")) || (!strcasecmp(soc_type, "csr8811"))){
			// for rome/ar3002/qca3003-uart, the 1st byte are packet type, should always be 4
			r = read(fd, buf, 1);
			if  (r<=0 || buf[0] != 4)  return -1;
		}
		/* The next two bytes are the event code and parameter total length. */
		while (count < 2) {
			r = read(fd, buf + count, 2 - count);
			if (r <= 0)
			{
				printf("read error \n");
				return -1;
			}
			count += r;
		}


		if (buf[1] == 0)
		{
			printf("Zero len , invalid \n");
			return -1;
		}
		size = buf[1];

		/* Now we read the parameters. */
		while (count  < size ) {
			r = read(fd, buf + count, size);
			if (r <= 0)
			{
				printf("read error :size = %d\n",size);
				return -1;
			}
			count += r;
		}

		switch (buf[0])
		{
			int j=0;
			case 0x0f:
			printf("Command Status Received\n");
			for (j=0 ; j< buf[1] + 2 ; j++)
				printf("[%x]", buf[j]);
			printf("\n");
			if(buf[2] == 0x02)
			{
				printf("\nUnknown connection identifier");
				return 0;
			}
			memset(buf , 0, MAX_EVENT_SIZE);
			count = 0; size =0;
			//20160622_Austin
			//CSR8811
			if(buf[buf[2] + 2] == 0x00)
			{
				printf("Command Status Event : success\n");
				return 0;
			}			
			break;

			case 0x02:
			printf("INQ RESULT EVENT RECEIVED \n");
			for (j=0 ; j< buf[1] + 2 ; j++)
				printf("[%x]", buf[j]);
			printf("\n");
			memset(buf , 0, MAX_EVENT_SIZE);
			count = 0; size =0;
			break;
			case 0x01:
			printf("INQ COMPLETE EVENT RECEIVED\n");
			printf("\n");
			memset(buf , 0, MAX_EVENT_SIZE);
			count = 0; size =0;
			return 0;
			case 0x03:
			if(buf[2] == 0x00)
				printf("CONNECTION COMPLETE EVENT RECEIVED WITH HANDLE: 0x%02x%02x \n",buf[4],buf[3]);
			else
				printf("CONNECTION COMPLETE EVENT RECEIVED WITH ERROR CODE 0x%x \n",buf[2]);
			for (j=0 ; j< buf[1] + 2 ; j++)
				printf("[%x]", buf[j]);
			printf("\n");
			memset(buf , 0, MAX_EVENT_SIZE);
			count = 0; size =0;
			return 0;
			case 0x05:
			printf("DISCONNECTION COMPLETE EVENT RECEIVED WITH REASON CODE: 0x%x \n",buf[5]);
			for (j=0 ; j< buf[1] + 2 ; j++)
				printf("[%x]", buf[j]);
			printf("\n");
			memset(buf , 0, MAX_EVENT_SIZE);
			count = 0; size =0;
			return 0;
			default:
			printf("Other event received, Breaking\n");

			for (j=0 ; j< buf[1] + 2 ; j++)
				printf("[%x]", buf[j]);
			printf("\n");

			memset(buf , 0, MAX_EVENT_SIZE);
			count = 0; size =0;
			return 0;
		}

		/* buf[1] should be the event opcode
		 * buf[2] shoudl be the parameter Len of the event opcode
		 */
	}while (1);

	return count;
}



/* HCI functions that require open device
 *  * dd - Device descriptor returned by hci_open_dev. */

int hci_send_cmd(int uart_fd, uint16_t ogf, uint16_t ocf, uint8_t plen, void *param)
{
    uint8_t type = 0x01; //HCI_COMMAND_PKT
    uint8_t hci_command_buf[256] = {0};
    uint8_t * p_buf = &hci_command_buf[0];
    uint8_t head_len = 3;
    hci_command_hdr *ch;

    if((!strcasecmp(soc_type, "rome")) || (!strcasecmp(soc_type, "300x")) || (!strcasecmp(soc_type, "csr8811")))
    {
          *p_buf++ = type;
           head_len ++;
    }
    
    ch = (void*)p_buf;
    ch->opcode = htobs(HCI_OPCODE_PACK(ogf,ocf));
    ch->plen = plen;
    p_buf += HCI_COMMAND_HEADER_SIZE;
 
    if(plen) {
               memcpy(p_buf, (uint8_t*) param,plen);
             }
    
    if(write(uart_fd, hci_command_buf, plen + head_len) < 0) {
              return -1;
            }
            return 0;
}

int csr8811_init(int uart_fd)
{
	int iRet;
	UCHAR resultBuf[MAX_EVENT_SIZE];

	printf("csr8811_init\n");

	/*
	memset(&resultBuf,0,MAX_EVENT_SIZE);	
	// OGF_HOST_CTL 0x03
	// OCF_RESET 0x0003
	printf("HCI Reset\n");
	iRet = hci_send_cmd(uart_fd, HCI_CMD_OGF_HOST_CTL, HCI_CMD_OCF_RESET, 0, resultBuf);
	if (!iRet)
		read_incoming_events(uart_fd, resultBuf, 0);
	*/
	//bcset warm_reset

	//PSKEY_ANA_FREQ (26MHz)
	printf("PSKEY_ANA_FREQ\n");
	memset(&resultBuf,0,MAX_EVENT_SIZE);
	iRet = hci_send_cmd( uart_fd, HCI_VENDOR_CMD_OGF, 0x00, 0x13, PSKEY_ANA_FREQ);
	if (!iRet)
		read_incoming_events(uart_fd, resultBuf, 0);
	printf("\n");

	//trimming cap value = 0x1d
//	iRet = hci_send_cmd( uart_fd, HCI_VENDOR_CMD_OGF, 0x00, 0x13, PSKEY_ANA_FTRIM);		
//	if (!iRet)
//		read_incoming_events(uart_fd, resultBuf, 0);
//	printf("\n");
		
	//Baud rate, 115200
	memset(&resultBuf,0,MAX_EVENT_SIZE);	
	iRet = hci_send_cmd( uart_fd, HCI_VENDOR_CMD_OGF, 0x00, 0x15, PSKEY_BAUD_RATE);
	if (!iRet)
		read_incoming_events(uart_fd, resultBuf, 0);
	printf("\n");
		
	//Interface, H4
	memset(&resultBuf,0,MAX_EVENT_SIZE);	
	iRet = hci_send_cmd( uart_fd, HCI_VENDOR_CMD_OGF, 0x00, 0x13, PSKEY_INTERFACE_H4);
	if (!iRet)
		read_incoming_events(uart_fd, resultBuf, 0);
	printf("\n");	

#ifdef NOTREQUIRED
	memset(&resultBuf,0,MAX_EVENT_SIZE);	
	iRet = hci_send_cmd( uart_fd, HCI_VENDOR_CMD_OGF, 0x00, 0x13, PSKEY_COEX_SCHEME);
	if (!iRet)
		read_incoming_events(uart_fd, resultBuf, 0);
	printf("\n");	
	
	memset(&resultBuf,0,MAX_EVENT_SIZE);	
	iRet = hci_send_cmd( uart_fd, HCI_VENDOR_CMD_OGF, 0x00, 0x13, PSKEY_COEX_PIO_UNITY_3_BT_ACTIVE);
	if (!iRet)
		read_incoming_events(uart_fd, resultBuf, 0);
	printf("\n");

	memset(&resultBuf,0,MAX_EVENT_SIZE);	
	//PSKEY_COEX_PIO_UNITY_3_BT_ACTIVE[12] = 0x24;
	//PSKEY_COEX_PIO_UNITY_3_BT_ACTIVE[11] = 0x84;
	//PSKEY_COEX_PIO_UNITY_3_BT_ACTIVE[15] = 0x01;		//pio_number
	//PSKEY_COEX_PIO_UNITY_3_BT_ACTIVE[17] = 0x01;		//polarity;	
	iRet = hci_send_cmd( uart_fd, HCI_VENDOR_CMD_OGF, 0x00, 0x13, PSKEY_COEX_PIO_UNITY_3_BT_STATUS);
	if (!iRet)
		read_incoming_events(uart_fd, resultBuf, 0);
	printf("\n");
#endif	
	//bcset warm_reset
	memset(&resultBuf,0,MAX_EVENT_SIZE);	
	iRet = hci_send_cmd( uart_fd, HCI_VENDOR_CMD_OGF, 0x00, 0x13, BCCMD_WARM_RESET);
	if (!iRet)
		read_incoming_events(uart_fd, resultBuf, 0);

	return true;		   
}


int main(int argc , char *argv[])
{
    struct sockaddr_in server, client;
	int clientaddr_size, read_size;
    char message[1000] , server_reply[2000];
    char ipAddress[255] = "localhost";
    int portNumber = 2390;            //default: 2390
    char IOTypeString[10] = "";      
    char baudrate[10] = "115200";    //default: 115200
    //int handShakeMode = 0;            //default: NONE
    int i, ret;	
	bool isPatchandNvmRequired = 0;		
	
    //getting all the input arguments and splitting based on "="
    char tempStr[20] = "";
    char* pPosition = NULL;

	//get argument values and check if minimum agruments are provided
    if (argc < 5)
    {
        printf("Require at least 5 arguments.\n\nExample of client mode, Btdiag usage:\n");
		printf("- BT FTM mode for serial:\n./Btdiag UDT=yes PORT=2390 IOType=SERIAL DEVICE=/dev/ttyUSB0 BAUDRATE=115200\n\n");
		printf("- BT FTM mode for USB:\n./Btdiag UDT=yes PORT=2390 IOType=USB DEVICE=hci0\n\n");
		return 1;
    }
	
	//Check UDT
	if (strstr(argv[1], "UDT"))
    {
        pPosition = strchr(argv[1], '=');
        if (0 == strcmp(pPosition+1, "yes"))
		{
		    printf("Run in UDT mode\n");
			legacyMode = 0;
		}
        else
            printf("Run in legacy mode\n");		
    }
	
    if (legacyMode)
    {	    
        //IPADDRESS
        if (NULL == strstr(argv[1], "IPADDRESS"))
        {
            printf("\nMissing QPST server ipaddress, Example: Btdiag.exe IPADDRESS=192.168.1.1\n");
            return 1;
        }
        else
        {
            pPosition = strchr(argv[1], '=');
            memcpy(ipAddress, pPosition+1, strlen(pPosition+1));
        }
        
        //PORT
        if (NULL == strstr(argv[2], "PORT"))
        {
            printf("\nMissing QPST server port number\n");
            return 1;
        }
        else
        {
            pPosition = strchr(argv[2], '=');
            memcpy(tempStr, pPosition+1, strlen(pPosition+1));
            portNumber = atoi(tempStr);
        }
		
        //IOType
        if (NULL == strstr(argv[3], "IOType"))
        {
            printf("\nMissing IOType option, valid options are SERIAL, USB\n");
            return 1;
        }
        else
        {
            pPosition = strchr(argv[3], '=');
            memcpy(IOTypeString, pPosition+1, strlen(pPosition+1));
        }
		
        //get connection type
        if(!strcmp(IOTypeString, "SERIAL"))
        {            
            connectionTypeOptions = SERIAL;
        }
        else if (!strcmp(IOTypeString, "USB"))
        {
            connectionTypeOptions = USB;
        }
        else
        {
            printf("Invalid entry, valid options are : SERIAL or USB\n");
            return;
        }
        
        //DEVICE
        if (NULL == strstr(argv[4], "DEVICE"))
        {
            printf("\nSpecifies the serial device to attach is not valid\n");
            return 1;
        }
        else
        {
            pPosition = strchr(argv[4], '=');
            memcpy(Device, pPosition+1, strlen(pPosition+1));
        }
        
        if (connectionTypeOptions == SERIAL)
        {
            //BAUDRATE
            if (argc > 5)
            {
                if (NULL == strstr(argv[5], "BAUDRATE"))
                {
                    printf("\nMissing baudrate option\n");
                    return 1;
                }
                else
                {
                    pPosition = strchr(argv[5], '=');
                    memcpy(baudrate, pPosition+1, strlen(pPosition+1));
                }
            }
            else 
            {
                printf("\nMissing baudrate option\n");
                return 1;
            }            
	        //require to download patch and NVM files
	        isPatchandNvmRequired = 1;
        }
		
        /* start TCP client */		
        //Create socket
        sock = socket(AF_INET , SOCK_STREAM , 0);
        if (sock == -1)
        {
            printf("Could not create socket");
        }
        puts("Socket created");
         
        server.sin_addr.s_addr = inet_addr(ipAddress);
        server.sin_family = AF_INET;
        server.sin_port = htons( portNumber );
        
        //Connect to remote server
        printf("Connecting.....\n");
        if (connect(sock , (struct sockaddr *)&server , sizeof(server)) < 0)
        {
            printf("\n\nFailed to connect to QPST, please make sure QPST is open and configured with above listed IP address and port number\n");
            perror("connect failed. Error");
            return 1;
        }
        else
        {
            puts("Connected\n");
        }
        
        //Buffer for data from stream
        BYTE rxbuf[TCP_BUF_LENGTH];
        memset(rxbuf, 0, TCP_BUF_LENGTH);			
        
        //Listening loop
        while(1)
        {
            printf("Waiting for QPST connection... \n");
        
            //starting and making connection to SoC DUT
            switch (connectionTypeOptions)
            {
                case SERIAL:		    	    
	    	    descriptor = SerialConnection(Device, baudrate);
                    printf("descriptor value %d from device",descriptor);
                break;
                case USB:		    
                    return 0;
                    //descriptor = USBConnection(Device);					
                break;
            }
		    
            if (descriptor <= 0)
            {
                printf("\nFailed to connect to external device on\n");
                return 1;
            }
            else		  
                printf("Connected to SoC DUT!\n");
			            
            /* Create thread to recive HCI event */
            {
                //Check: move it to subfunction.
                //Create a thread for receiving HCI event from DUT.
                int err;
                pthread_t thread;
                struct arg_struct args;
                unsigned char databuf[HCI_MAX_EVENT_SIZE];
			    
                if (connectionTypeOptions == SERIAL)
                    args.desc = descriptor;
                else if (connectionTypeOptions == USB)
                    args.desc = descriptor;
				
                args.buf = databuf;
                args.size = HCI_MAX_EVENT_SIZE;
			    
                err = pthread_create(&thread, NULL, &NotifyObserver, (void *)&args);
                if (err != 0)
                {
                    printf("\nCan't create thread :[%s]", strerror(err));
                    break;
                }
                else
                    printf("\nThread created successfully\n");
            }

            // Loop to receive all the data sent by the QMSL/QRCT(via QPST), as long as it is greater than 0
            int lengthOfStream;
            while ((lengthOfStream = recv(sock, rxbuf, TCP_BUF_LENGTH, 0)) != 0)
            {
                processDiagPacket(rxbuf);
				
                //initiate buf
                memset(rxbuf, 0, TCP_BUF_LENGTH);							
            }
			
            //checking if server is still on, if not close the application
            if (lengthOfStream == 0)
            {
                printf("\n>>>QPST/Server is closed, closing application\n");			
                close(sock);
                return;
            }
        }
    }
	else  //UDT mode: User Defined Transport mode
    {
        //PORT
        if (NULL == strstr(argv[2], "PORT"))
        {
            printf("\nMissing QPST server port number\n");
            return 1;
        }
        else
        {
            pPosition = strchr(argv[2], '=');
            memcpy(tempStr, pPosition+1, strlen(pPosition+1));
            portNumber = atoi(tempStr);
        }
		
        //IOType
        if (NULL == strstr(argv[3], "IOType"))
        {
            printf("\nMissing IOType option, valid options are SERIAL, USB\n");
            return 1;
        }
        else
        {
            pPosition = strchr(argv[3], '=');
            memcpy(IOTypeString, pPosition+1, strlen(pPosition+1));
        }
		
        //get connection type
        if(!strcmp(IOTypeString, "SERIAL"))
        {            
            connectionTypeOptions = SERIAL;
        }
        else if (!strcmp(IOTypeString, "USB"))
        {
            connectionTypeOptions = USB;
        }
        else
        {
            printf("Invalid entry, valid options are : SERIAL or USB\n");
            return;
        }
        
        //DEVICE
        if (NULL == strstr(argv[4], "DEVICE"))
        {
            printf("\nSpecifies the serial device to attach is not valid\n");
            return 1;
        }
        else
        {
            pPosition = strchr(argv[4], '=');
            memcpy(Device, pPosition+1, strlen(pPosition+1));
        }
        
        if (connectionTypeOptions == SERIAL)
        {
            //BAUDRATE
            if (argc > 5)
            {
                if (NULL == strstr(argv[5], "BAUDRATE"))
                {
                    printf("\nMissing baudrate option\n");
                    return 1;
                }
                else
                {
                    pPosition = strchr(argv[5], '=');
                    memcpy(baudrate, pPosition+1, strlen(pPosition+1));
                }
            }
            else 
            {
                printf("\nMissing baudrate option\n");
                return 1;
            }            
	        //require to download patch and NVM files
	        isPatchandNvmRequired = 0;
        }

        //starting and making connection to SoC DUT

        switch (connectionTypeOptions)
        {
            case SERIAL:		    	    
            printf("Entering serial connection: \n");
            descriptor = SerialConnection(Device, baudrate);
            printf("descriptor ret value %d\n" , descriptor);
            break;
            case USB:						
                return 0;
               // descriptor = USBConnection(Device);					
            break;
        }
		    
        if (descriptor <= 0)
        {
            printf("\nFailed to connect to external device on\n");
            return 1;
        }
        else		  
            printf("Connected to SoC DUT!\n");
		
		// DK_DEBUG
		csr8811_init (descriptor);

        /* Create thread to recive HCI event */
        {
            //Check: move it to subfunction.
            //Create a thread for receiving HCI event from DUT.
            int err;
            pthread_t thread;
            struct arg_struct args;
            unsigned char databuf[HCI_MAX_EVENT_SIZE];
			    
            if (connectionTypeOptions == SERIAL)
                args.desc = descriptor;
            else if (connectionTypeOptions == USB)
                args.desc = descriptor;
				
            args.buf = databuf;
            args.size = HCI_MAX_EVENT_SIZE;
			    
            err = pthread_create(&thread, NULL, &NotifyObserver, (void *)&args);
            if (err != 0)
            {
                printf("\nCan't create thread :[%s]", strerror(err));
                //break;
            }
            else
                printf("\nThread created successfully\n");
        }
 		
        /* start TCP server */		
        //Create socket
        sock = socket(AF_INET , SOCK_STREAM , 0);
        printf("Sock created : %d",sock);

        if (sock == -1)
        {
            printf("Could not create socket");
        }
        puts("Socket created");
         
        server.sin_addr.s_addr = INADDR_ANY; //inet_addr(ipAddress);
        server.sin_family = AF_INET;
        server.sin_port = htons( portNumber );
		
		//For fixing "Address already in use" issue. 
		//(When client keeps connection to server, terminate server and reopen it will make "Address already in use" issue occur.)
		int opt = 1;
		setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));		
		
        //Bind
        if( bind(sock,(struct sockaddr *)&server , sizeof(server)) < 0)
        {
            //print the error message
            perror("bind failed. Error");
            //TODO:check
            close(sock);
            return 1;
        }
        puts("bind done");
         
        //Listen
        printf("Listneing to sock at : %d",sock);
        listen(sock , 3);
         		 
        //Accept and incoming connection
        puts("Waiting for incoming connections...");
        clientaddr_size  = sizeof(struct sockaddr_in);
		
		//accept connection from an incoming client
        client_sock = accept(sock, (struct sockaddr *)&client, (socklen_t*)&clientaddr_size);
        if (client_sock < 0)
        {
            perror("accept failed");
            return 1;
        }
        puts("Connected!");
		
		//Buffer for data from stream
        BYTE incomingDataBytes[TCP_BUF_LENGTH];
        memset(incomingDataBytes, 0, TCP_BUF_LENGTH); 


        // Loop to receive all the data sent by the QMSL/QRCT(via QPST), as long as it is greater than 0
        int lengthOfStream;
        while ((lengthOfStream = recv(client_sock, incomingDataBytes, TCP_BUF_LENGTH, 0)) != 0)
        {
            printf("calling processDiagPacket in Btdiag \n");
            printf("length of stream: %d\n",lengthOfStream);
            processDiagPacket(incomingDataBytes);
            printf("SD------->returned from Process DIag packet to caller in BTdiag:\n");
			
            //initiate incomingDataBytes buf
            memset(incomingDataBytes, 0, TCP_BUF_LENGTH);
        }
		
        if(lengthOfStream == 0)
        {
            puts("Client disconnected");
            fflush(stdout);
        }
        else if(lengthOfStream == -1)
        {
            perror("recv failed");
        }
    }
    return 0;
}

void *NotifyObserver(void * arguments)
{
    struct arg_struct *args = arguments;
    int i, ret = 256, recv_byte_len = 256, data_len, DataToSendasLogPacketFor1366_Length = 0;
    //struct hci_filter flt;
    unsigned char* tempbuf;
    BYTE uartbuf[500]  = { 0x00 };
    BYTE DataToSendasLogPacketFor1366[500];
    BYTE DataToSendasLogPacketHeader[] = { 0x10, 00, 0x15, 00, 0x15, 00, 0x7C, 0x11, 00, 00, 0xC7, 0xE5, 0x62, 0x2A, 0xC5, 0xFF, 01, 00 };
    BYTE DataToSendasLogPacketHeaderFor1366[] = { 0x10, 00, 0x12, 00, 0x12, 00, 0x66, 0x13, 0xE5, 0x62, 0x2A, 0xC5, 0xFF, 01, 00 };

    //printf("Entering function %s\n",__FUNCTION__);

    while (1)
    {
	switch (connectionTypeOptions)
        {

            case SERIAL:
              // printf("SD----> reading args Descriptor %d\n",args->desc);
               //printf("SD--------->>>>>>>>>>>>>Reading HCI Event\n");
	       //ret = read_hci_event(args->desc, args->buf, args->size);
	       ret = read_hci_event(descriptor, uartbuf, HCI_MAX_EVENT_SIZE);
               //printf("--------->>>>>>>>>returned val from HCI read event  %d\n",ret);
               recv_byte_len = ret;
               for (i=0; i<recv_byte_len; i++)
              	printf("%02x ", uartbuf[i]);
 	       printf("\n");
	       break;
#ifdef NOTREQUIRED
              printf("SD---->>>>>>>>>received Byte length: %d\n",recv_byte_len);
              printf("SD1---->>>>>>>>>received Byte length: %d\n",recv_byte_len);
              printf("SD2---->>>>>>>>>received Byte length: %d\n",recv_byte_len);
              printf("SD---Data In from DUT ---->\n");
              printf("SD---Data In(from DUT)(hex) - Event: \n");
              printf("\n\n");
             break;
#endif
#ifdef NOTREQUIRED
            case USB:				
				ret = read_raw_data(args->desc, args->buf, args->size);
				
				//USB get returned data is raw data, the first byte of raw data is "HCI_PACKET_TYPE" header. (its value should be "HCI_EVENT_PKT" here).
				//The HCI data follow it, so we shift one byte to get HCI data.
				tempbuf = args->buf;
				args->buf = (args->buf + 1);
			    recv_byte_len = ret - 1;
			    break;				
#endif
	    default:
		printf("%s: Invalid option %d\n", __func__, connectionTypeOptions);
		break;
        }
			
#ifdef NOTREQUIRED
	if (ret < 0) {
            fprintf(stderr, "%s: Failed to send hci command to Controller, ret = %d\n", __FUNCTION__, ret);
            return 0;
        }
        else
        {
	    switch (connectionTypeOptions)
            {
                case SERIAL:			
                printf("Data In from DUT ---->");
                printf("Data In(from DUT)(hex) - Event: ");
                for (i=0; i<recv_byte_len; i++)
                    printf("%02x ", args->buf[i]);
        	    printf("\n\n");
				break;
				case USB:
				printf("Data In(from DUT)(hex) - Event: ");
                for (i=0; i<ret; i++)
                    printf("%02x ", tempbuf[i]);
        	    printf("\n\n");
                break;				
			}
        }
#endif
        
        //if (packetData.type == PacketDataType.Data)
        {
            
         switch (connectionTypeOptions)
         {
                case SERIAL:
                    break;
                case USB:
                    break;
         }
        	
	 if (connectionTypeOptions == USB)
		    data_len = recv_byte_len + 1;
		else
		    data_len = recv_byte_len;
			
#ifdef NOTREQUIRED
	BYTE DataToSendasLogPacketFor1366[15 + data_len];
#endif
	DataToSendasLogPacketFor1366_Length = 15 + data_len;
        printf("AB \n");
	memset(DataToSendasLogPacketFor1366, 0, DataToSendasLogPacketFor1366_Length);         
			        
                                      
        printf("p: d_l = %d \n", data_len);
            //for 0x1366
            for (i = 0; i < 15; i++)
            {
                DataToSendasLogPacketFor1366[i] = DataToSendasLogPacketHeaderFor1366[i];
            }
                	
#ifdef NOTEREQUIED
            if (connectionTypeOptions == USB)
            {                       
                DataToSendasLogPacketFor1366[15] = 4;
            }        
#endif
        	
            if (connectionTypeOptions == USB)
            {
                //adding BT HCI event to the DIAG log packet
                for (i = 0; i < recv_byte_len; i++)
                {                           
                    //DataToSendasLogPacketFor1366[16 + i] = args->buf[i];
                    DataToSendasLogPacketFor1366[16 + i] = uartbuf[i];
                }
            }
            else
            {
                //adding BT HCI event to the DIAG log packet
                for (i = 0; i < recv_byte_len; i++)
                {                            
                   // DataToSendasLogPacketFor1366[15 + i] = args->buf[i];
                    DataToSendasLogPacketFor1366[15 + i] = uartbuf[i];
                }
            }
        	
            //fixing the exact length in the DIAG log packet for 0x1366
            int lengthOfDiagPacketFor1366 = DataToSendasLogPacketFor1366_Length - 4;
            DataToSendasLogPacketFor1366[2] = (BYTE)lengthOfDiagPacketFor1366;
            DataToSendasLogPacketFor1366[4] = (BYTE)lengthOfDiagPacketFor1366;

        printf("Sending TCP \n");
            DiagPrintToStream(DataToSendasLogPacketFor1366, DataToSendasLogPacketFor1366_Length, true);
            //debug message commented out
            //Console.WriteLine("1366 (from DUT): " + Conversions.ByteArrayTo0xHexString(DataToSendasLogPacketFor1366) + "\r\n");
        }
    }
}
