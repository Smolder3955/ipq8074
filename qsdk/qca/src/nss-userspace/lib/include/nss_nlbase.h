/*
 * Copyright (c) 2019 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#ifndef __NSS_NLBASE_H__
#define __NSS_NLBASE_H__

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <net/if.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <getopt.h>
#include <stdint.h>
#include <limits.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <linux/socket.h>

/* Generic Netlink header */
#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>

#if !defined (likely) || !defined (unlikely)
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif

/* NSS headers */
#include <nss_def.h>
#include <nss_arch.h>
#include <nss_cmn.h>
#include <nss_ipv4.h>
#include <nss_ipv6.h>
#include <nss_nl_if.h>

/* NSS NL socket headers */
#include <nss_nlist_api.h>
#include <nss_nlcmn_if.h>
#include <nss_nlipv4_if.h>
#include <nss_nlipv6_if.h>
#include <nss_nlsock_api.h>
#include <nss_nlipv4_api.h>
#include <nss_nlipv6_api.h>


#endif /* !__NSS_NLBASE_H__*/
