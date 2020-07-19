/* 
Copyright (c) 2013 Qualcomm Atheros, Inc.
All Rights Reserved. 
Qualcomm Atheros Confidential and Proprietary. 
*/
#include <termios.h>

#define HCI_UART_H4	            0

#define HCI_UART_RAW_DEVICE	    0

int read_hci_event(int fd, unsigned char* buf, int size);
int qca_soc_init(int fd, char *bdaddr);
