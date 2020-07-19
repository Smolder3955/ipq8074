/*
 * Copyright (c) 2015,2018 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * 2015 Qualcomm Atheros, Inc.
 *
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#define SYSFS_PATH_MAX 256
#define SYS_CLASS_NET "/sys/class/net/"
#define ADDRESS "/tmp/wrapd_cli_socket"
#define MAX_PORTS       1024
#define CHUNK 128
#define DEFAULT_SLEEP_TIMER 1
#define DEFAULT_VAP_LIMIT 20
#define DEFAULT_DELETE_ENABLE 0
#define DEFAULT_BRIDGE_NAME "br-lan"
#define DEFAULT_INTERFACE "eth1"
#define MIN_SLEEP_TIMER 1
#define MAX_SLEEP_TIMER 10
#define MAX_VAP_LIMIT 28
#define MAX_PORT_LIMIT 8
#define DEFAULT_PORT 9999
#define MAX_READ_LENGTH 1024
#define IEEE80211_ADDR_LEN 6
#define SOCKET_ADDR_LEN 64

#ifndef INT_MAX
#define INT_MAX 0x7FFFFFFF
#endif

int global_vap_limit_flag;

struct input
{
	char brname[IFNAMSIZ];
	char ifname[MAX_PORT_LIMIT][IFNAMSIZ];
	int table_no;
	int new_table_no;
	int port[MAX_PORT_LIMIT];
	int no_of_interfaces;
	int delete_enable;
	char vap_interface[IFNAMSIZ];
	int vap_limit;
	int sleep_timer;
	int ioctl_sock;
	char wrapd_ctrl_intf[128];
};

struct new_fdb_table
{
	int flag;
	unsigned char mac_addr[6];
	unsigned int ageing_timer_value;
};

struct fdb_table
{
	int flag;
	unsigned char mac_addr[6];
	int success_flag;
};

struct fdb_table *table;
struct input *data;
int wrapd_send_msg(const char *msg, int len, const char *dest_path);
int wrap_read_forwarding_database(struct new_fdb_table *fdbs, unsigned long offset, int num);
int wrap_interface_to_port(char *ifname);
void *wrap_main_function();
void *wrap_check_socket();
void handle_signal_kill(int signum);

