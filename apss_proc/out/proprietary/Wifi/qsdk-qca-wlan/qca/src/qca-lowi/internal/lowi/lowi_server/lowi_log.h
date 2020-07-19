/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

                  QUIPC Debug Message Interface Header File

GENERAL DESCRIPTION
  This file contains the structure definitions and function prototypes for
  the debugging message interface for QUIPC software.

Copyright (c) 2013 Qualcomm Atheros, Inc.
  All Rights Reserved.
  Qualcomm Atheros Confidential and Proprietary.

Copyright (c) 2016 Qualcomm Technologies, Inc.
 All Rights Reserved.
 Confidential and Proprietary - Qualcomm Technologies, Inc.
=============================================================================*/
#ifndef __XTRAT_WIFI_LOG_H__
#ifndef _LOWI_LOG_H_
#define _LOWI_LOG_H_

#ifdef __cplusplus
extern "C"
{
#endif

/*=============================================================================================
                                  Debug message module
             This portion of the header file will need to be updated for each OS
 =============================================================================================*/
#ifndef LOG_NDDEBUG
#define LOG_NDDEBUG 0
#endif

extern int lowi_debug_level;
extern int lowi_debug_tag;

// The debug level is read from a config file, so that the log level can be changned
// without re-compiling the software
#define log_error(local_tag, format, ...) \
if (lowi_debug_level >= 1) { printf("[%s-%s] "format, lowi_debug_tag, local_tag, ##__VA_ARGS__); }

#define log_warning(local_tag, format, ...) \
if (lowi_debug_level >= 2) { printf("[%s-%s] "format, lowi_debug_tag, local_tag, ##__VA_ARGS__); }

#define log_info(local_tag, format, ...) \
if (lowi_debug_level >= 3) { printf("[%s-%s] "format, lowi_debug_tag, local_tag, ##__VA_ARGS__); }

#define log_debug(local_tag, format, ...) \
if (lowi_debug_level >= 4) { printf("[%s-%s] "format, lowi_debug_tag, local_tag, ##__VA_ARGS__); }

#define log_verbose(local_tag, format, ...) \
if (lowi_debug_level >= 5) { printf("[%s-%s] "format, lowi_debug_tag, local_tag, ##__VA_ARGS__); }

#define LOWI_LOG_ERROR(...) { log_error(LOG_TAG, __VA_ARGS__); }

#define LOWI_LOG_WARN(...) { log_warning(LOG_TAG, __VA_ARGS__); }

#define LOWI_LOG_INFO(...) { log_info(LOG_TAG, __VA_ARGS__); }

#define LOWI_LOG_DBG(...) { log_debug(LOG_TAG, __VA_ARGS__); }

#define LOWI_LOG_VERB(...) { log_verbose(LOG_TAG, __VA_ARGS__); }

#ifdef __cplusplus
}
#endif

#endif /* _LOWI_LOG_H_ */
#endif /* __XTRAT_WIFI_LOG_H__ */
