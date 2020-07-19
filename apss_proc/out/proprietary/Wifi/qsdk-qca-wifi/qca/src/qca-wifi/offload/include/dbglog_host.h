/*
 * Copyright (c) 2017-2019 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary . Qualcomm Innovation Center, Inc.
 *
 * Copyright (c) 2011, Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _DBGLOG_HOST_H_
#define _DBGLOG_HOST_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <linux/debugfs.h>

#define MAX_DBG_MSGS 256

/** dbglog_int - Registers a WMI event handle for WMI_DBGMSG_EVENT
* @brief scn - handle
*/
void
dbglog_init(ol_scn_t scn);

/** set the size of the report size
* @brief wmi_handle - handle to Wmi module
* @brief size - Report size
*/
void
dbglog_set_report_size(ol_scn_t scn, A_UINT16 size);

/** Set the resolution for time stamp
* @brief wmi_handle - handle to Wmi module
* @ brief tsr - time stamp resolution
*/
void
dbglog_set_timestamp_resolution(ol_scn_t scn, A_UINT16 tsr);

/** Enable reporting. If it is set to false then Traget wont deliver
* any debug information
*/
void
dbglog_reporting_enable(ol_scn_t scn, bool isenable);

/** Set the log level
* @brief DBGLOG_INFO - Information lowest log level
* @brief DBGLOG_WARNING
* @brief DBGLOG_ERROR - default log level
* @brief DBGLOG_FATAL
*/
void
dbglog_set_log_lvl(ol_scn_t scn, uint32_t log_lvl);

/** Enable/Disable the logging for VAP */
void
dbglog_vap_log_enable(ol_scn_t scn, A_UINT16 vap_id,
			   bool isenable);
/** Enable/Disable logging for Module */
void
dbglog_module_log_enable(ol_scn_t scn, A_UINT32 mod_id,
			      bool isenable);
#ifdef unittest_dbglogs
void
test_dbg_config(ol_scn_t scn);
#endif

/** Custome debug_print handlers */
/* Args:
   module Id
   vap id
   debug msg id
   Time stamp
   no of arguments
   pointer to the buffer holding the args
*/
typedef A_BOOL (*module_dbg_print) (A_UINT32, A_UINT16, A_UINT32, A_UINT32,
                                   A_UINT16, A_UINT32 *);

/** Register module specific dbg print*/
void dbglog_reg_modprint(A_UINT32 mod_id, module_dbg_print printfn);

int dbglog_parse_debug_logs(ol_scn_t soc, u_int8_t *datap,
                            u_int16_t len, void *context);

/**
 * @brief dbglog print message path cookie
 */
typedef enum {
    DBGLOG_PRT_WMI = 0x00,
    DBGLOG_PRT_PKTLOG,
} dbglog_prt_path_t;

int
dbglog_message_handler(ol_scn_t scn, uint8_t *evt_buf, uint32_t len);

void
dbglog_free(void *scn);

void
dbglog_ratelimit_set(A_UINT32 burst_limit);

#ifdef OL_ATH_SMART_LOGGING
int
smart_log_init(void *icp);
void
smart_log_deinit(void *sc);
ssize_t smart_logs_dump(struct device *dev,
                               struct device_attribute *attr, char *buf);
/*Enum for representing type of log to store */
typedef enum smartlog_type {
    SMART_LOG_TYPE_FW,      /* To store the FW logs  */
    SMART_LOG_TYPE_CE,      /* To store CE data */
    SMART_LOG_TYPE_EVENT,   /* To store Specific events eg:Sta kickout*/
}SMARTLOG_TYPE;

#define SMARTLOG_DBGFS_NAME 20
/* Structure to maintain debug information */
struct smartlog_debugfs_info {
    char name[SMARTLOG_DBGFS_NAME];  /* debugfs name */
    struct debugfs_blob_wrapper blob; /*binary blob points to smartlogs*/
    struct dentry *dfs;
};

/* Indexes for smartlog memory */
typedef struct smart_log_mem {
    A_UINT8 *start;           /* Points to beggning of smartlogs mem*/
    A_UINT8 *cur;             /* Points to current pos filled */
    struct smartlog_debugfs_info dbgfs;
}smart_log_mem_t;

void  store_smart_debug_log(smart_log_mem_t *file, SMARTLOG_TYPE type, A_UINT8 *data, A_UINT32 len);
#endif /* OL_ATH_SMART_LOGGING */

#ifdef __cplusplus
}
#endif

#endif /* _DBGLOG_HOST_H_ */
