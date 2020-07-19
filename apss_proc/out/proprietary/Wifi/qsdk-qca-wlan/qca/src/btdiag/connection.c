/* 
Copyright (c) 2013 Qualcomm Atheros, Inc.
All Rights Reserved. 
Qualcomm Atheros Confidential and Proprietary. 
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define _GNU_SOURCE
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <syslog.h>
#include <termios.h>
#include <time.h>
#include <sys/time.h>
#include <sys/poll.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include "global.h"

//#include <bluetooth/bluetooth.h>
//#include <bluetooth/hci.h>
//#include <bluetooth/hci_lib.h>

#include "connection.h"

#define FLOW_CTL	0x0001
#define ENABLE_PM	1
#define DISABLE_PM	0
#define GET_TYPE        "qca"

struct uart_t {
	char *type;
	int  init_speed;
	int  speed;
	int  flags;
	char *bdaddr;
	int  (*init) (int fildes, struct uart_t *u, struct termios *ti);
};
	
static void sig_alarm(int sig)
{
	fprintf(stderr, "Initialization timed out.\n");
	exit(1);
}

unsigned char change_to_tcio_baud(int cfg_baud, int *baud)
{
	if (cfg_baud == 115200)
		*baud = B115200;
	else if (cfg_baud == 4000000)
		*baud = B4000000;
	else if (cfg_baud == 3000000)
		*baud = B3000000;
	else if (cfg_baud == 2000000)
		*baud = B2000000;
	else if (cfg_baud == 1000000)
		*baud = B1000000;
	else if (cfg_baud == 921600)
		*baud = B921600;
	else if (cfg_baud == 460800)
		*baud = B460800;
	else if (cfg_baud == 230400)
		*baud = B230400;
	else if (cfg_baud == 57600)
		*baud = B57600;
	else if (cfg_baud == 19200)
		*baud = B19200;
	else if (cfg_baud == 9600)
		*baud = B9600;
	else if (cfg_baud == 1200)
		*baud = B1200;
	else if (cfg_baud == 600)
		*baud = B600;
	else
	{
		printf( "unsupported baud: %i, use default baud value - B115200\n", cfg_baud);
		*baud = B115200;
		return 1;
	}

	return 0;
}

int set_baud_rate(int fildes, struct termios *ti, int speed)
{
	int tcio_baud;
        printf("Setting Buad Rate to %d\n", speed);
	
	change_to_tcio_baud(speed, &tcio_baud);

	if (cfsetospeed(ti, tcio_baud) < 0)
		goto fail;

	if (cfsetispeed(ti, tcio_baud) < 0)
		goto fail;

	if (tcsetattr(fildes, TCSANOW, ti) < 0)
		goto fail;

	return 0;
	
fail:
    return -errno;
}

int read_hci_event(int fildes, unsigned char* databuf, int len)
{
	int received_bytes = 0;
	int remain, ret = 0;
	char tempbuf[50];

    printf("entering read_hci_event %s \n",__FUNCTION__);
	if (len <= 0)
		goto fail;

	while (1) {
	    ret = read(fildes, databuf, 1);
            printf("hci events1 %d bytes val = 0x%x :\n", ret, databuf[0]);
		if (ret <= 0)
			goto fail;
		else if (databuf[0] == 4)
			break;
	}
	received_bytes++;
	
	while (received_bytes < 3) {
		ret = read(fildes, databuf + received_bytes, 3 - received_bytes);
                printf("hci events2 %d :\n", ret);
		if (ret <= 0)
		    goto fail;
			
		received_bytes += ret;
	}

	/* Now we read the parameters. */
	if (databuf[2] < (len - 3))
	    remain = databuf[2];
	else
		remain = len - 3;

	while ((received_bytes - 3) < remain) {
		ret = read(fildes, databuf + received_bytes, remain - (received_bytes - 3));
                printf("hci events3 %d :\n", ret);
		
		if (ret <= 0)
			goto fail;
		received_bytes += ret;
	}

	
	//ret = read(fildes, tempbuf, sizeof (tempbuf));
	//printf("%s: %d\n", __func__, ret);
	return received_bytes;

fail:
    return -1;	
}

static int qca(int fildes, struct uart_t *u, struct termios *ti)
{
    fprintf(stderr,"qca\n");
    return qca_soc_init(fildes, u->bdaddr);
    //return 0;
}

struct uart_t uart[] = {
    /* QCA ROME */
    { "qca", 115200, 115200, FLOW_CTL, NULL, qca},
	{ NULL, 0 }
};

static struct uart_t * get_type_from_table(char *type)
{
	int i = 0;

	// get type from table
	while(uart[i].type)
	{
	    if (!strcmp(uart[i].type, type))
			return &uart[i];
	    i++;
	}
	
	return NULL;
}

void set_rtscts_flag(struct termios *ti, bool enable)
{
    printf("%s: %d\n", __func__, enable);
	if (enable)
		ti->c_cflag |= CRTSCTS;
	else
		ti->c_cflag &= ~CRTSCTS;
}

static int initport(char *dev, struct uart_t *u, int sendbreak)
{
	int fildes;
	struct termios term_attr;

	if ((fildes = open(dev, O_RDWR | O_NOCTTY)) == -1)
	{
		printf("Uart: Open serial port failed\n");
		return -1;
	}

	//Flush Terminal Input or Output
	if (tcflush(fildes, TCIOFLUSH) != 0)
		perror("tcflush error");
		
	if (tcgetattr(fildes, &term_attr) != 0) {
		printf("Uart: Get port settings failed\n");
		return -1;
	}

	cfmakeraw(&term_attr);
	
	//term_attr.c_cflag |= (CLOCAL | PARENB);
	term_attr.c_cflag |= CLOCAL;
	if (u->flags & FLOW_CTL)
	{
		// flow control via CTS/RTS
		set_rtscts_flag(&term_attr, 1);
	}
	else
		set_rtscts_flag(&term_attr, 0);
	
	if (tcsetattr(fildes, TCSANOW, &term_attr) != 0) {
		printf("Uart: Set port settings failed\n");
		return -1;
	}

	if (set_baud_rate(fildes, &term_attr, u->init_speed) < 0) {
		printf("Uart: Set initial baud rate failed\n");
		return -1;
	}
	
	if (tcflush(fildes, TCIOFLUSH) != 0)
		perror("tcflush error");
	
	if (sendbreak) {
		tcsendbreak(fildes, 0);
		usleep(400000);
	}

	if (u->init && u->init(fildes, u, &term_attr) < 0)
		return -1;

	if (tcflush(fildes, TCIOFLUSH) != 0)
		perror("tcflush error");
	tcgetattr(fildes,&term_attr);
/*
    printf("0x%x\n", term_attr.c_iflag);
    printf("0x%x\n", term_attr.c_oflag);
    printf("0x%x\n", term_attr.c_cflag);
    printf("0x%x\n", term_attr.c_lflag);
    printf("0x%x\n", term_attr.c_line);
    printf("0x%x\n", term_attr.c_ispeed);
    printf("0x%x\n", term_attr.c_ospeed); */
	return fildes;
}

/*
 *  The value of below parameters are referred to hciattach.c and hciattach_rome.c(BlueZ):
 *    (1)Device type is "qca"
 *    (2)Flow control is turn "ON" 
 *    (3)Waiting time is "120 sec"
 */
int SerialConnection(char *dev_path, char *baudrate)
{
        printf("entering SerialConnection: %s",__FUNCTION__);
	struct uart_t *u = NULL;
	int opt, i, n, ld, err;
	int to = 10;
	int init_speed = 0;
	int sendbreak = 0;
	pid_t pid;
	struct sigaction sa;
	struct pollfd p;
	sigset_t sigs;
	char dev[PATH_MAX];
	
	/* Set the parameters */
	if(strlcpy(dev, dev_path,sizeof(dev)) >= sizeof(dev)){
            return -1; 
         }

        u = get_type_from_table(GET_TYPE);  	
    to = 120;

	if (!u) {
		fprintf(stderr, "Unknown device type or id\n");
		exit(-1);
	}
    
	u->speed = atoi(baudrate);    
	u->flags |=  FLOW_CTL;
        
	/* If user specified a initial speed, use that instead of
	   the hardware's default */
	if (init_speed)
            u->init_speed = init_speed;

	if (u->speed)
            u->init_speed = u->speed;

	memset(&sa, 0, sizeof(sa));
	sa.sa_flags   = SA_NOCLDSTOP;
        printf("before calling sig_alarm func \n");
	sa.sa_handler = sig_alarm;
	sigaction(SIGALRM, &sa, NULL);
        printf("after calling sigaction\n");

	/* 10 seconds should be enough for initialization */
	alarm(to);
	n = initport(dev, u, sendbreak);
        printf("return value of n %d ",n);
	if (n < 0) {
		perror("Can't initialize device");
		exit(-1);
	}
        printf("Serial port is initialized HOORAY---->\n");
	//printf("Download Patch firmware and NVM file completed\n");
	alarm(0);

	return n;
        printf("exiting the serial Connection function \n",__FUNCTION__);
}

#ifdef NOTREQUIRED
/* Get device descriptor */
int USBConnection(char *dev_name)
{
    int dd,dev_id;
	
    dev_id = hci_devid(dev_name);
	if (dev_id < 0) 
	{
		perror("Invalid device");
	    exit(-1);
	}
	
    if (dev_id < 0)
	 	dev_id = hci_get_route(NULL);	
	
	dd = hci_open_dev(dev_id);
	if (dd < 0) {
		perror("Device open failed");
	 	exit(-1);
	}
	
	return dd;
}
#endif

/*
 * Send an HCI command to (USB) device.
 *
 */
#ifdef NOTREQUIRED
int usb_send_hci_cmd(char *dev_name, unsigned char *rawbuf, int rawdatalen)
{
    unsigned char buf[HCI_MAX_EVENT_SIZE];
	unsigned char *ptr = buf;
	unsigned char *param_ptr = NULL;
	struct hci_filter flt;
	hci_event_hdr *hdr;
	int i, opt, len, dd, dev_id;
	uint16_t ocf, opcode;
	uint8_t ogf;	
	
	dev_id = hci_devid(dev_name);
	if (dev_id < 0) 
	{
		perror("Invalid device");
		exit(EXIT_FAILURE);
	}
	
    if (dev_id < 0)
		dev_id = hci_get_route(NULL);	
	
	dd = hci_open_dev(dev_id);
	if (dd < 0) {
		perror("Device open failed");
		exit(EXIT_FAILURE);
	}
		
	/* calculate OGF and OCF: ogf is bit 10~15 of opcode, ocf is bit 0~9 of opcode */
	errno = 0;	
	opcode = rawbuf[1] | (rawbuf[2] << 8);
	ogf = opcode >> 10;
	ocf = opcode & 0x3ff;
	
	if (errno == ERANGE || (ogf > 0x3f) || (ocf > 0x3ff)) {
		printf("wrong input format\n");
		exit(EXIT_FAILURE);
	}
	
	/* copy parameter to buffer and calculate parameter total length */	
	param_ptr = rawbuf + (1 + HCI_COMMAND_HDR_SIZE);
	//parameter length = HCI raw packet length - HCI_PACKET_TYPE length - HCI_COMMAND_HDR length
	len = rawdatalen - (1 + HCI_COMMAND_HDR_SIZE);
	memcpy(buf, param_ptr, len);	
	
	/* Setup filter */
	hci_filter_clear(&flt);
	hci_filter_set_ptype(HCI_EVENT_PKT, &flt);
	hci_filter_all_events(&flt);
	if (setsockopt(dd, SOL_HCI, HCI_FILTER, &flt, sizeof(flt)) < 0) {
		perror("HCI filter setup failed");
		exit(EXIT_FAILURE);
	}

	if (hci_send_cmd(dd, ogf, ocf, len, buf) < 0) {
		perror("HCI send failed in connection:  Send failed");
		exit(EXIT_FAILURE);
	}

	hci_close_dev(dd);
	return 0;
}

/*
 * Read an HCI raw data from the given device descriptor.
 *
 * return value: actual recived data length
 */
int read_raw_data(int dd, unsigned char *buf, int size)
{
    struct hci_filter flt;
	int ret;
	
    /* Setup filter */
	hci_filter_clear(&flt);
	hci_filter_set_ptype(HCI_EVENT_PKT, &flt);
	hci_filter_all_events(&flt);
	if (setsockopt(dd, SOL_HCI, HCI_FILTER, &flt, sizeof(flt)) < 0) {
		perror("HCI filter setup failed");
		exit(EXIT_FAILURE);
	}
	
	ret = read(dd, buf, size);
	return ret;
}
#endif
