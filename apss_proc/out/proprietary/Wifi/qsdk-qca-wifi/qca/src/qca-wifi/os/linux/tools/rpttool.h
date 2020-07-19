/*
 * Copyright (c) 2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#ifndef _RPTTOOL_H_
#define _RPTTOOL_H_

#include <sys/ioctl.h>
#include <string.h>
#include <sys/socket.h>
#include <net/if.h>
#include <linux/types.h>
#include <stdio.h>
#include <linux/netlink.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include "if_athioctl.h"
#define _LINUX_TYPES_H

#include <getopt.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/file.h>
#include <endian.h>
#include <fcntl.h>
#include <linux/if_bridge.h>
#include <asm/param.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include "iwlib.h"
#include "wireless.h"

#define MAXNUMSTAS         10
#define	IEEE80211_ADDR_LEN  6
#define	streq(a,b)	   (strcasecmp(a,b) == 0)
#define MAX_RETRIES         3

#ifndef NETLINK_RPTGPUT
    #define NETLINK_RPTGPUT 18
#endif

struct sta_topology {
    u_int8_t  mac_address[IEEE80211_ADDR_LEN];
    u_int32_t goodput;
};

typedef struct rptgput_msg {
    u_int32_t  sta_count;
    struct sta_topology sta_top_map[MAXNUMSTAS];
} RPTGPUT_MSG;

extern struct rptgput_msg *rptgput_data_buf;

#endif
