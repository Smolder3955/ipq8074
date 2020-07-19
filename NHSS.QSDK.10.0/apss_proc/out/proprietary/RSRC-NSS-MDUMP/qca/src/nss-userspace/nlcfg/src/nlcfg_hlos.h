/*
 * Copyright (c) 2019 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#ifndef __NLCFG_HLOS_H
#define __NLCFG_HLOS_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <net/if.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <getopt.h>
#include <stdint.h>
#include <limits.h>
#include <arpa/inet.h>

#define  NLCFG_ALIGN(x, b) (((x) + (b) - 1) & ~((b) - 1))

#define NLCFG_COLOR_RST "\x1b[0m"
#define NLCFG_COLOR_GRN "\x1b[32m"
#define NLCFG_COLOR_RED "\x1b[31m"
#define NLCFG_COLOR_MGT "\x1b[35m"

#define nlcfg_log_error(fmt, arg...) printf(NLCFG_COLOR_RED"[ERR]"NLCFG_COLOR_RST fmt, ## arg)
#define nlcfg_log_info(fmt, arg...) printf(NLCFG_COLOR_GRN"[INF]"NLCFG_COLOR_RST fmt, ## arg)
#define nlcfg_log_trace(fmt, arg...) printf(NLCFG_COLOR_MGT"[TRC(<%s>)]"NLCFG_COLOR_RST fmt, __func__, ## arg)
#define nlcfg_log_options(fmt, arg...) printf(NLCFG_COLOR_MGT"[OPT_%d]"NLCFG_COLOR_RST fmt, ## arg)
#define nlcfg_log_warn(fmt, arg...) printf(NLCFG_COLOR_RED"[WARN]"NLCFG_COLOR_RST fmt, ##arg)

#define nlcfg_log_arg_options(num, param) nlcfg_log_options("'%s'\n", num, (param)->name)
#define nlcfg_log_arg_error(param) nlcfg_log_error("invalid args for '%s'\n", (param)->name)
#define nlcfg_log_data_error(param) nlcfg_log_error("invalid data for '%s'\n", (param)->name)

#endif /* __NLCFG_HLOS_H*/

