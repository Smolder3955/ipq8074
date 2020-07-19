/*
 * Copyright (c) 2018 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include <stdio.h>
#include <stdint.h>
#include <sys/time.h>
#include "utils.h"

#ifdef ANDROID
#include <android/log.h>

static enum android_LogPriority level_to_android_priority(int level)
{
	switch (level) {
	case WL_MSG_ERROR:
		return ANDROID_LOG_ERROR;
	case WL_MSG_INFO:
		return ANDROID_LOG_INFO;
	case WL_MSG_DEBUG:
		return ANDROID_LOG_DEBUG;
	default:
		return ANDROID_LOG_VERBOSE;
	}
}
#endif /* ANDROID */

/*
 * Function     : wl_print
 * Description  : prints given formatted string to logcat and/or system
 * Input params : pointer to wifilearner_ctx, log message level, log message
 * Return       : void
 *
 */
void wl_print(struct wifilearner_ctx *wlc, int level, const char *fmt, ...)
{
	va_list ap;
	struct timeval tv;

#ifdef ANDROID
	va_start(ap, fmt);
	__android_log_vprint(level_to_android_priority(level),
			     "wifilearner", fmt, ap);
	va_end(ap);
#endif /* ANDROID */

	if (wlc && !wlc->stdout_debug)
		return;
	va_start(ap, fmt);
	gettimeofday(&tv, NULL);
	printf("%ld.%06u: ", (long) tv.tv_sec,
	       (unsigned int) tv.tv_usec);
	vprintf(fmt, ap);
	printf("\n");
	va_end(ap);
}
