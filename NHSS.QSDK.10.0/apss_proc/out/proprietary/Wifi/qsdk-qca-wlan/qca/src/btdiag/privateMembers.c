/*
Copyright (c) 2013, 2018 Qualcomm Technologies, Inc.

All Rights Reserved.
Confidential and Proprietary - Qualcomm Technologies, Inc.

2013 Qualcomm Atheros, Inc.

All Rights Reserved.
Qualcomm Atheros Confidential and Proprietary.

*/

/* This function processes each DIAG packet and sends response based on its type
 *  If DIAG packet is for Bluetooth commands,
 *  it will strip the DIAG header and send only the BT command payload to the connected DUT
 */
#include <stdio.h> //printf
#include <string.h> //memset
#include <stdbool.h> //bool
#include "privateMembers.h"
//#include "global.h"

const BYTE DIAG_TERM_CHAR = 0x7E;

#define AHDLC_FLAG    0x7e      // Flag byte in Async HDLC
#define AHDLC_ESCAPE  0x7d      // Escape byte in Async HDLC
#define AHDLC_ESC_M   0x20      // Escape mask in Async HDLC

extern int sock;
extern int client_sock;
extern bool legacyMode;
extern int descriptor;
extern ConnectionType connectionTypeOptions;
extern char Device[];

unsigned short Calc_CRC_16_l(BYTE pBufPtr[], int bufLen)
{
    /*
	The CRC table size is based on how many bits at a time we are going
        to process through the table.  Given that we are processing the data
        8 bits at a time, this gives us 2^8 (256) entries.
    */
    //const int c_iCRC_TAB_SIZE = 256;	// 2^CRC_TAB_BITS

    /* CRC table for 16 bit CRC, with generator polynomial 0x8408,
    ** calculated 8 bits at a time, LSB first.
    */
    unsigned short _aCRC_16_L_Table[] =
    {
	0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
        0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
        0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
        0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
        0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
        0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
        0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
        0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
        0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
        0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
        0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
        0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
        0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
        0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
        0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
        0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
        0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
        0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
        0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
        0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
        0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
        0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
        0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
        0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
        0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
        0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
        0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
        0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
        0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
        0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
        0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
        0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78
    };

    /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

    /* Mask for CRC-16 polynomial:
    **
    **      x^16 + x^12 + x^5 + 1
    **
    ** This is more commonly referred to as CCITT-16.
    ** Note:  the x^16 tap is left off, it's implicit.
    */
    const unsigned short c_iCRC_16_L_POLYNOMIAL = 0x8408;

    /* Seed value for CRC register.  The all ones seed is part of CCITT-16, as
    ** well as allows detection of an entire data stream of zeroes.
    */
    const unsigned short c_iCRC_16_L_SEED = 0xFFFF;


    unsigned short data, crc_16;

    /*
	Generate a CRC-16 by looking up the transformation in a table and
        XOR-ing it into the CRC, one byte at a time.
    */
    int i = 0;
    int iLen = bufLen * 8;

    for (crc_16 = c_iCRC_16_L_SEED; iLen >= 8; iLen -= 8, i++)
    {
	//        temp1 = (crc_16 ^ pBufPtr[i]);
        //        temp1 = temp1 & 0x00ff;
        //        temp2 = (crc_16 >> 8) & 0xff;
        //        crc_16 = (short)(_aCRC_16_L_Table[temp1] ^ temp2);
        crc_16 = (unsigned short)(_aCRC_16_L_Table[(crc_16 ^ pBufPtr[i]) & 0x00ff] ^ (crc_16 >> 8) );
    }

    /*
	Finish calculating the CRC over the trailing data bits

        XOR the MS bit of data with the MS bit of the CRC.
        Shift the CRC and data left 1 bit.
        If the XOR result is 1, XOR the generating polynomial in with the CRC.
    */
    if (iLen != 0)
    {
	data = (unsigned short)(((unsigned short)(pBufPtr[i])) << (16 - 8));		// Align data MSB with CRC MSB

        while (iLen-- != 0)
        {
            if (((crc_16 ^ data) & 0x01) != 0)
            {										// Is LSB of XOR a 1

                crc_16 >>= 1;						// Right shift CRC
                crc_16 ^= c_iCRC_16_L_POLYNOMIAL;	// XOR polynomial into CRC

            }
            else
            {

                crc_16 >>= 1;						// Right shift CRC

            }	// if ( ((crc_16 ^ data) & 0x01) != 0 )

            data >>= 1;								// Right shift data

        }	// while (iLen-- != 0)

    }	// if (iLen != 0) {

    return ((unsigned short)~crc_16);            // return the 1's complement of the CRC

}

void DiagPrintToStream(BYTE response[], int Length, bool check)
{
	unsigned short crc;
	int responseLength = 0;
	int modifiedResponseMsg_Length = 0;
	int messageToSend_Length = 0;

	responseLength = Length;
	messageToSend_Length = responseLength + 3;

	printf("BA\n");
	crc = Calc_CRC_16_l(response, responseLength);

	modifiedResponseMsg_Length = responseLength + 2;
	BYTE modifiedResponseMsg[responseLength + 2];
	memset( modifiedResponseMsg, 0, responseLength + 2);

	int i;
	for (i = 0; i < responseLength; i++)
	 {
	  modifiedResponseMsg[i] = response[i];
	}

	modifiedResponseMsg[responseLength + 0] = (BYTE)(crc & 0xff);
	modifiedResponseMsg[responseLength + 1] = (BYTE)(crc >> 8);

	//List<byte> tempCheckList = new List<byte>();
	BYTE tempCheckList[512];
	memset(tempCheckList, 0 ,512);

	int responseCounter = 0;
	int tempCheckList_Length = 0;
	for (responseCounter = 0; responseCounter < modifiedResponseMsg_Length; responseCounter++)
	{
	//If the information to be sent contains either the ending flag (0x7E) or the escape character
        //(0x7D), the escape character is inserted into the data stream and the byte is XORed with the
        //escape complement value (0x20), then sent.
		if ((modifiedResponseMsg[responseCounter] == AHDLC_ESCAPE) || (modifiedResponseMsg[responseCounter] == AHDLC_FLAG))
		{
			if (check)
			{
			tempCheckList[tempCheckList_Length++] = AHDLC_ESCAPE;
			BYTE p = (BYTE)(modifiedResponseMsg[responseCounter] ^ AHDLC_ESC_M);
			tempCheckList[tempCheckList_Length++] = p;
			}
			else
			{
			tempCheckList[tempCheckList_Length++] = modifiedResponseMsg[responseCounter];
			}
		}
		else
		{
			tempCheckList[tempCheckList_Length++] = modifiedResponseMsg[responseCounter];
		}
	}
	//responseLength = responseCounter;
	//memcpy(modifiedResponseMsg, tempCheckList, sizeof(modifiedResponseMsg));

	//add termination character
	BYTE messageToSend[tempCheckList_Length + 1];   //modifiedResponseMsg.Length = responseLength +2;
	memset( messageToSend, 0, tempCheckList_Length + 1);
	for (i = 0; i < tempCheckList_Length; i++)
	{
		messageToSend[i] = tempCheckList[i];
	}

	messageToSend[tempCheckList_Length] = DIAG_TERM_CHAR;
	messageToSend_Length = tempCheckList_Length + 1;

	if (legacyMode == 1)
	{
        if (sock != -1)
        {
		if( send(sock , messageToSend , messageToSend_Length , 0) < 0)
		{
		puts("Send failed legacy mode");
                return;
		}
	}
	else
	{
		printf("Could not create socket");
		return;
	}
	}
	else
	{
	    if (client_sock != -1)
        {
            if( send(client_sock , messageToSend , messageToSend_Length , 0) < 0)
            {
                puts("client Sock Send failed");
                return;
            }
        }
	    else
	    {
	        printf("Could not create socket");
		    return;
	    }
	}
}

int processDiagPacket(BYTE inputDIAGMsg[])
{
	int i=0;
    printf("Entered process Diag packet after received packet\n");
	switch (inputDIAGMsg[0])
	{
        case 0X73:  //logmask respons
            printf("Log Mask\n");
            if (inputDIAGMsg[4] == 1)
            {
                DiagPrintToStream(logMaskResponseFirstMsg, sizeof(logMaskResponseFirstMsg), false);
            }
            else if (inputDIAGMsg[4] == 4)
            {
                DiagPrintToStream(logMaskResponseSecondMsg, sizeof(logMaskResponseSecondMsg), false);
            }
            return 1;
        case 0X0:  //verResponseMsg
            printf("echo for version \n");
            DiagPrintToStream(verResponseMsg, sizeof(verResponseMsg), false);
            return 1;
        case 0X1:  //esnResponseMsg;
            printf("echo for esn\n");
            DiagPrintToStream(esnResponseMsg, sizeof(esnResponseMsg), false);
            return 1;
        case 0XC:  //statusResponseMsg
            printf("echo for status\n");
            DiagPrintToStream(statusResponseMsg, sizeof(statusResponseMsg), false);
            return 1;
        case 0x3F: //phoneStateResponseMsg
            printf("echo for phoneState\n");
            DiagPrintToStream(phoneStateResponseMsg, sizeof(phoneStateResponseMsg), false);
            return 1;
        case 0x26: //nvItemReadResponseMsg;
            printf("echo for nvItemRead\n");
            DiagPrintToStream(nvItemReadResponseMsg, sizeof(nvItemReadResponseMsg), false);
            return 1;
        case 0x4B:  //(byte)CmdCode.subSystemDispatch:
            printf("\n>SubsystemDispatch Diag packet received of type 0x%02x\n", inputDIAGMsg[0]);
		printf("\n");

	DiagPrintToStream(inputDIAGMsg, TCP_BUF_LENGTH, false);
            //check for BT 4B packet and type Bluetooth
            if ((inputDIAGMsg[10] == 1) && (inputDIAGMsg[1] == 11) && (inputDIAGMsg[2] == 4) && (inputDIAGMsg[3] == 0))//check 4B packet
		{
                //check for length from 6th byte and copy payload from 10th byte
                int lengthOfPayload = inputDIAGMsg[6];
                BYTE msgToDUT[lengthOfPayload];

                //Re-calculate real length of the Diag packet, since it may contains 0x7D/0x7E.
		 //Assuming Diag Packet doesn't contain 0x7D/0x7E, lengthOfPayload is equal to inputDIAGMsg_Length in this case.
		int inputDIAGMsg_Length = 0;

		//Calculate inputDIAGMsg_Length which specifies the length of Diag packet except termination char 0x7E
		while(inputDIAGMsg[inputDIAGMsg_Length] != DIAG_TERM_CHAR)
			 inputDIAGMsg_Length++;

		//inputDIAGMsg_Length = inputDIAGMsg_Length calculate in above while loop - 10 (starting offset of payload) - 2 (CRC bytes)
		inputDIAGMsg_Length -= 12;

		//check if 7E was replaced
		int index = 0;
		int payloadIndex = 10;
                for (payloadIndex = 10; payloadIndex < (10 + inputDIAGMsg_Length); payloadIndex++)
                {
                    if ((inputDIAGMsg[payloadIndex] == AHDLC_ESCAPE) && ((inputDIAGMsg[payloadIndex + 1] == AHDLC_ESCAPE ^ AHDLC_ESC_M) || (inputDIAGMsg[payloadIndex + 1] == AHDLC_FLAG ^ AHDLC_ESC_M )))
                    {
                        msgToDUT[index] = inputDIAGMsg[payloadIndex + 1] ^ AHDLC_ESC_M ;
                        payloadIndex++;
                    }
                    else
                    {
                        msgToDUT[index] = inputDIAGMsg[payloadIndex];
                    }
                    index++;
                }

	printf("SD_--->Before writing Message to DUT data stream\n");
                for (i=0; i<lengthOfPayload; i++)
                    printf("%02x ", msgToDUT[i]);

	/* Send the HCI command packet to UART for transmission */
                unsigned char rsp[HCI_MAX_EVENT_SIZE];
                int ret = 0;
		switch (connectionTypeOptions)
		        {
			case SERIAL:
			printf("SD--->sending HCI command packet to UART for transmission\n");
			ret = write(descriptor, msgToDUT, lengthOfPayload);
                        printf("SD---->return value after writing to DUT %d and Length: %d",ret,lengthOfPayload);
			if (ret != lengthOfPayload) {
			fprintf(stderr, "%s: Send failed with ret value: %d\n", __FUNCTION__, ret);
			return 0;
			}
			break;
			case USB:
					//ret = usb_send_hci_cmd(Device, msgToDUT, lengthOfPayload);
				        //if (ret != 0) {
				         //   fprintf(stderr, "%s: Send failed with ret value: %d\n", __FUNCTION__, ret)
					return 0;
				       // }
					break;
		        }
			printf("Data Out(to DUT)(hex): ");
			for (i=0; i<lengthOfPayload; i++)
			printf("%02x ", msgToDUT[i]);
			printf("\n");
			}
			return 1;
			case 0x7B:  //diagProtLoopback:
			printf("echo for diagProtLoopback\n");
			//look for 7E to calculate length
			int indexOf7E = 0;
			for (i = 0; i < TCP_BUF_LENGTH; i++)
			{
				if (inputDIAGMsg[i] == AHDLC_FLAG)
			{
				indexOf7E = i;
				break;
			}
            }
		if (legacyMode == 1)
		{
			    if( send(sock , inputDIAGMsg , indexOf7E + 1 , 0) < 0)
                {
                    puts("legacy mode diag proto loop back Send failed");
                    return 0;
                }
			}
			else
			{
			if( send(client_sock , inputDIAGMsg , indexOf7E + 1 , 0) < 0)
                {
                    puts("Input Diag msg Send failed");
                    return 0;
                }
			}
            return 1;
		case 0x7C:  //extendedBuildIDRespMsg
            printf("echo for extendedBuildID\n");
            DiagPrintToStream(extendedBuildIDRespMsg, sizeof(extendedBuildIDRespMsg), false);
            return 1;
		default:
            printf("In default\n");
            return 1;
    }
}
