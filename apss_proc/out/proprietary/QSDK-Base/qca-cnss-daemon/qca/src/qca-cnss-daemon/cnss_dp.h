/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#ifdef CONFIG_CNSS_DP
int wlan_dp_service_start(void);
void wlan_dp_service_stop(void);
#else
static int wlan_dp_service_start(void) { return 0; }
static void wlan_dp_service_stop(void) { return; }
#ifdef ANDROID
static int cnssd_perfd_client_start(void) { return 0; }
static int wlan_service_run_netd_cmd(const char *cmd) { return 0; }
static int wlan_service_set_tuning_parameter(const char *param,
                                             const char *value)
{
	return 0;
}
#endif
#endif
