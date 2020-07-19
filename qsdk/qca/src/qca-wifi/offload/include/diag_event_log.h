/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */

/*
 * Copyright (c) 2011, 2014-2017 The Linux Foundation. All rights reserved.
 */

#ifndef _DIAG_EVENT_LOG_H_
#define _DIAG_EVENT_LOG_H_
int diag_fw_handler(ol_scn_t sc, uint8_t *data, uint32_t datalen);
int dbglog_set_log_lvl_tlv(wmi_unified_t wmi_handle, uint32_t log_lvl);
#endif /* _DIAG_EVENT_LOG_H_ */
