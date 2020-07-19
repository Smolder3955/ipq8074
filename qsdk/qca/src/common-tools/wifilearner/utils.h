/*
 * Copyright (c) 2018 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */


#ifndef UTILS_H
#define UTILS_H

#include "wifilearner.h"

#ifdef __GNUC__
#define PRINTF_FORMAT(a, b) __attribute__ ((format (printf, (a), (b))))
#else
#define PRINTF_FORMAT(a, b)
#endif


enum wl_log_print_level {
	WL_MSG_DEBUG, WL_MSG_INFO, WL_MSG_ERROR
};

void wl_print(struct wifilearner_ctx *wlc, int level, const char *fmt, ...)
PRINTF_FORMAT(3, 4);

#endif  //UTILS_H
