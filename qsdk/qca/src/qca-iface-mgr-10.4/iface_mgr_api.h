/*
 * Copyright (c) 2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */
/*
 * Copyright (c) 2016 Qualcomm Atheros, Inc..
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#ifndef _IFACE_MGR_API__H
#define _IFACE_MGR_API__H

#if 1
#define ifacemgr_printf(fmt, args...) do { \
    printf("iface_mgr: %s: %d: " fmt "\n", __func__, __LINE__, ## args); \
} while (0)
#else
#define ifacemgr_printf(args...) do { } while (0)
#endif

#ifndef MAC2STR
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#endif
#define MAC2STR_ADDR(a) &(a)[0], &(a)[1], &(a)[2], &(a)[3], &(a)[4], &(a)[5]

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

enum driver_mode {
        WEXT_MODE = 0,
        CFG80211_MODE = 1,
};

typedef void ifacemgr_hdl_t;

ifacemgr_hdl_t * ifacemgr_load_conf_file(const char *conf_file, int *num_sta_vaps, int *num_plc_ifaces, enum driver_mode *drvmode);
int ifacemgr_conn_to_global_wpa_s(const char *global_supp, ifacemgr_hdl_t *ifacemgr_handle);

int ifacemgr_create_netlink_socket(ifacemgr_hdl_t *ifacemgr_handle);
void ifacemgr_wpa_s_ctrl_iface_receive(int sock, void *eloop_ctx, void *sock_ctx);
int ifacemgr_update_ifacemgr_handle_config(ifacemgr_hdl_t *ifacemgr_handle);
void ifacemgr_free_ifacemgr_handle(ifacemgr_hdl_t *ifacemgr_handle);
void ifacemgr_status_inform_to_driver(ifacemgr_hdl_t *ifacemgr_handle, u_int8_t iface_mgr_up);
void ifacemgr_display_updated_config(ifacemgr_hdl_t *ifacemgr_handle);
void son_main_function(void *conf_file,  enum driver_mode drv_mode);
int ifacemgr_create_hydsock(ifacemgr_hdl_t *ifacemgr_handle);
#endif //_IFACE_MGR_API__H
