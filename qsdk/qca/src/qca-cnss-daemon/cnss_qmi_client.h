/*
 * Copyright (c) 2014,2017 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#ifndef __CNSS_QMI_CLIENT_H__
#define __CNSS_QMI_CLIENT_H__

#define TIMEOUT_MS 3000
#ifdef CONFIG_CNSS_LPASS_SCAN
int wlan_service_start(void);
void wlan_service_stop(void);
int wlan_service_process_msg(int type, void *data, int len);
int wlan_service_init(void);
void wlan_service_release(void);
void wlan_service_shutdown(void);
#else
int wlan_service_start(void) { return 0; }
void wlan_service_stop(void) { }
#endif

#endif /* __CNSS_QMI_CLIENT_H__ */
