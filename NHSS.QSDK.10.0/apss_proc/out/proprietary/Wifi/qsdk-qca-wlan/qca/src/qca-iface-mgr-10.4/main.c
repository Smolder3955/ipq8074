/*
 * Copyright (c) 2016 Qualcomm Atheros, Inc..
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include <dirent.h>
#include <sys/un.h>

#include <pthread.h>
#include "includes.h"
#include "eloop.h"
#include "common.h"
#include "wpa_ctrl.h"
#include "iface_mgr_api.h"
#include "drv_nl80211.h"
#include <fcntl.h>

#ifdef IFACE_SUPPORT_CFG80211
#include "wlanif_cmn.h"

extern int wlanIfConfigInitIface(int);
extern struct wlanif_config *wlanIfIfM;
#endif

ifacemgr_hdl_t *ifacemgr_handle;
#define MAX_PID_LEN 6
int store_pid(const char *pid_file) {
    if (pid_file) {
	int f = open(pid_file, O_WRONLY | O_CREAT);
	int pid = getpid();
	char buf[MAX_PID_LEN] = {0};
	snprintf(buf, MAX_PID_LEN, "%d", pid);
	if (f) {
	    if (write(f, buf, strlen(buf)) != strlen(buf)) {
		ifacemgr_printf("Failed to write pid file");
		return -1;
	    }
	    close(f);
	    return 0;
	}
    }
    return -1;
}



int main(int argc, char *argv[])
{
    const char *conf_file = "/var/run/iface_mgr.conf";
    const char *global_wpa_s_ctrl_intf = "/var/run/wpa_supplicantglobal";
    const char *pid_file = "/var/run/iface_mgr.pid";
    void *son_conf = "/var/run/son.conf";
    int num_sta_vaps;
    int num_plc_ifaces;
    enum driver_mode drvmode;

    int ret = 0;

    if(store_pid(pid_file)) {
        ifacemgr_printf("Unable to store PID in file");
        goto out;
    }

    ifacemgr_handle = ifacemgr_load_conf_file(conf_file, &num_sta_vaps, &num_plc_ifaces, &drvmode);
    if (ifacemgr_handle == NULL) {
        ifacemgr_printf("Failed to load conf file");
        goto out;
    }

#ifdef IFACE_SUPPORT_CFG80211
    if (wlanIfConfigInitIface(drvmode)) {
        ifacemgr_printf("Iface Manager wlan config interface init failed!");
        goto out;
    }
#endif

    if (eloop_init()) {
	ifacemgr_printf("Failed to initialize event loop");
	goto out;
    }
    son_main_function(son_conf, drvmode);
    if ((num_sta_vaps + num_plc_ifaces) == 0) {
	goto out1;
    }
    if (num_plc_ifaces != 0) {
	ret = ifacemgr_create_hydsock(ifacemgr_handle);
	if (!ret) {
	    ifacemgr_printf("Failed to establish connection with Hyd sock\n");
	    goto out;
	}
    }
    if (num_sta_vaps != 0) {
	ret = ifacemgr_conn_to_global_wpa_s((const char*)global_wpa_s_ctrl_intf, ifacemgr_handle);
	if (!ret) {
	    ifacemgr_printf("Failed to establish connection with global wpa supplicant");
	    goto out;
	}

	ret = ifacemgr_update_ifacemgr_handle_config(ifacemgr_handle);
	if (!ret) {
	    ifacemgr_printf("Failed to update ifacemge handler config");
	    goto out;
	}
    }
    ret = ifacemgr_create_netlink_socket(ifacemgr_handle);
    if (!ret) {
            ifacemgr_printf("Failed to create netlink socket");
            goto out;
    }
    ifacemgr_status_inform_to_driver(ifacemgr_handle, 1);
    ifacemgr_display_updated_config(ifacemgr_handle);

out1:
    eloop_run();

    return 1;

out:
    if (ifacemgr_handle) {
        ifacemgr_status_inform_to_driver(ifacemgr_handle, 0);
        ifacemgr_free_ifacemgr_handle(ifacemgr_handle);
    }

#ifdef IFACE_SUPPORT_CFG80211
    wlanif_config_deinit(wlanIfIfM);
#endif
    return 0;

}

