/*
 * Copyright (c) 2012,2018 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * 2012 Qualcomm Atheros, Inc.
 *
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/un.h>
#include <linux/types.h>
#include <net/if_arp.h>
#include <sys/utsname.h>
#include "includes.h"
#include "common.h"
#include "linux_ioctl.h"
#include "wpa_ctrl.h"
#include "ieee802_11_defs.h"
#include "linux_wext.h"
#include "eloop.h"
#include "netlink.h"
#include "priv_netlink.h"

#include "wrapd_api.h"
#include "bridge.h"
#include "ext_ioctl_drv_if.h"
#include "qwrap_structure.h"

#define _BYTE_ORDER _BIG_ENDIAN
#include "ieee80211_external.h"

#define WRAP_MAX_PSTA_NUM   30
#define WRAP_MAX_CMD_LEN    128
#define WRAP_PSTA_START_OFF 2
#define IFNAME_LEN          8


#define BRCTL_FLAG_NORMAL_WIRED         4099
#define BRCTL_FLAG_NORMAL_WIRELESS      4100
#define BRCTL_FLAG_ISOLATION_WIRED      69635
#define BRCTL_FLAG_ISOLATION_WIRELESS   69636

#define HOSTAPD_MSG_ADDR_OFF    3
#define WPA_S_MSG_ADDR_OFF      3

#define WIFINAMSIZ    6

#define MAX(a, b) (((a) > (b)) ? a : b)
#define MIN(a, b) (((a) < (b)) ? a : b)

#define WRAPD_PSTA_FLAG_WIRED   (1 << 0)
#define WRAPD_PSTA_FLAG_MAT     (1 << 1)
#define WRAPD_PSTA_FLAG_OPEN    (1 << 2)

struct proxy_sta {
    u8 oma[IEEE80211_ADDR_LEN];
    u8 vma[IEEE80211_ADDR_LEN];
    char parent[IFNAMSIZ];
    char child[IFNAMSIZ];
    int vma_loaded;
    int connected;
    int added;
    u_int32_t flags;
    struct ctrl_conn *parent_ap;
};

struct wrapd_ctrl {
	int sock;
	struct sockaddr_un local;
	struct sockaddr_un dest;
};

typedef enum {
    OPEN = 0,
    SEC,
} sectype_t;

typedef enum {
    INIT = 0,
    RUNTIME,
} cmdtype_t;

struct ctrl_conn {
    char ifname[IFNAMSIZ];
    u8 ap_mac_addr[IEEE80211_ADDR_LEN];
    struct wrapd_ctrl *ctrl_iface;
    struct ctrl_conn *next;
    struct wrap_wifi_iface *aptr;
    struct wrap_wifi_iface *parent_wifi;
    int remove_from_user;
    sectype_t secmode;
};


struct wrap_wifi_iface {
    struct wpa_ctrl *ctrl;
    struct wpa_ctrl *global;
    struct wpa_ctrl *to_hostapd;
    struct wrapd_ctrl *wrapd;
    struct proxy_sta psta[WRAP_MAX_PSTA_NUM];
    struct ctrl_conn *hapd_list;
    struct ctrl_conn *wpa_s;
    struct wrapd_global *wptr;
    char *wpa_conf_file;
    char *wifi_ifname;
    int wifi_device_num;
    int mpsta_conn;
    int proxy_noack_war;
    u8 wifi_dev_addr[IEEE80211_ADDR_LEN];
    u8 mpsta_mac_addr[IEEE80211_ADDR_LEN];
    int we_version;
    int num_ap_vaps;
    int priority;
    struct wrap_wifi_iface *next;
    char *vma_conf_file;
    struct netlink_data *netlink;
};

static void disconnect_all_stas(struct ctrl_conn *hapd);
void wrapd_wpa_s_ctrl_iface_process(struct wrap_wifi_iface *aptr, char *msg, u_int8_t initial_stavap_status);

static int char2addr(char* addr)
{
    int i, j=2;

    for(i=2; i<17; i+=3) {
        addr[j++] = addr[i+1];
        addr[j++] = addr[i+2];
    }

    for(i=0; i<12; i++) {
        /* check 0~9, A~F */
        addr[i] = ((addr[i]-48) < 10) ? (addr[i] - 48) : (addr[i] - 55);
        /* check a~f */
        if ( addr[i] >= 42 )
            addr[i] -= 32;
        if ( addr[i] > 0xf )
            return -1;
    }

    for(i=0; i<6; i++)
        addr[i] = (addr[(i<<1)] << 4) + addr[(i<<1)+1];

    return 0;
}

static int
wrapd_ap_kick_all_stas(struct ctrl_conn *hapd)
{
    struct extended_ioctl_wrapper extended_cmd;
    int ret;
    struct wrap_wifi_iface *aptr = hapd->aptr;
    extended_cmd.cmd = EXTENDED_SUBIOCTL_DISASSOC_CLIENTS;
    extended_cmd.data_len = sizeof(extended_cmd.ext_data.qwrap_config);
    ret = driver->send_command(aptr->wifi_ifname, (void *)&extended_cmd, sizeof(extended_cmd), QCA_NL80211_VENDOR_SUBCMD_EXTENDEDSTATS, SIOCGATHEXTENDED, &(aptr->wptr->sock));
    if(ret != 0) {
        wrapd_printf("ret=%d ioctl SIOCGATHEXTENDED disassoc_clients err", ret);
        return -1;
    }
    else {
        wrapd_printf("Disassoc clients passed on %s \n",aptr->wifi_ifname);
        return 0;
    }
}
static int
wrapd_get_80211param(struct wrapd_global *wptr, char *ifname, int op, int *data)
{
    return driver->send_command_get(ifname, op, data,
        IEEE80211_IOCTL_GETPARAM, &(wptr->sock));
}

void
wrapd_ifname_to_parent_ifname(struct wrapd_global *wptr, char *child, char *parent)
{
    struct ifreq ifr;
    int parent_index=0;

    wrapd_get_80211param(wptr, child, IEEE80211_PARAM_PARENT_IFINDEX, &parent_index);

    os_memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_ifindex = parent_index;
    if (ioctl(wrapg->ioctl_sock, SIOCGIFNAME, &ifr) != 0) {
        wrapd_printf("ioctl SIOCGIFNAME err");
        return;
    }

    os_memcpy(parent, ifr.ifr_name, IFNAMSIZ);
}

struct ctrl_conn *global_ifname_to_ctrl_conn(struct wrapd_global *wptr, char *ifname)
{
    struct ctrl_conn *conn;
    struct wrap_wifi_iface *dev = NULL;

    for(dev = wptr->wrap_wifi_list; dev; dev=dev->next)
    {
        for(conn = dev->hapd_list ; conn ; conn = conn->next)
        {
            if (os_strcmp(conn->ifname, ifname) == 0) {
                return conn;
            }
        }
    }
    return NULL;
}

struct ctrl_conn *wifi_ifname_to_ctrl_conn(struct wrap_wifi_iface *aptr, char *ifname)
{
    struct ctrl_conn *conn;

    for(conn = aptr->hapd_list ; conn ; conn = conn->next)
    {
        if (os_strcmp(conn->ifname, ifname) == 0) {
            return conn;
        }
    }

    return NULL;
}
void cleanup_ifaces()
{
    struct wrap_wifi_iface *aptr, *nextaptr;
    struct ctrl_conn *next, *conn;

    wrapd_printf("Cleaning up wrapd sockets");

    aptr = wrapg->wrap_wifi_list;
    while(aptr)
    {
        conn = aptr->wpa_s;

        while(conn) {
            if(conn->ctrl_iface) {
                wrapd_psta_if_remove(aptr, conn->ifname);
                eloop_unregister_read_sock(conn->ctrl_iface->sock);
                wpa_ctrl_close((struct wpa_ctrl *)conn->ctrl_iface);
            }
            next = conn->next;
            os_free(conn);
            conn = next;
        }

        aptr->mpsta_conn = 0;
        wrapd_disconn_all(aptr);
        aptr->wpa_s = NULL;
        os_free(aptr->wpa_conf_file);

        conn = aptr->hapd_list;

        while(conn) {
            if(conn->ctrl_iface) {
                wrapd_remove_to_global(aptr, conn->ifname);
                eloop_unregister_read_sock(conn->ctrl_iface->sock);
                wpa_ctrl_close((struct wpa_ctrl *)conn->ctrl_iface);
            }
            next = conn->next;
            os_free(conn);
            conn = next;
        }
        aptr->hapd_list = NULL;

        if(aptr->wrapd)
        {
            eloop_unregister_read_sock(aptr->wrapd->sock);
            wpa_ctrl_close((struct wpa_ctrl *)aptr->wrapd);
        }
        nextaptr = aptr->next;
        os_free(aptr);
        aptr = nextaptr;
    }

    if(wrapg->ioctl_sock)
        close(wrapg->ioctl_sock);

    driver->destroy_context(&(wrapg->sock));

    if(wrapg->global_wpa_supp)
        wpa_ctrl_close(wrapg->global_wpa_supp);

    if(wrapg->global_hostapd)
        wpa_ctrl_close(wrapg->global_hostapd);

    pthread_cancel(wrapg->wrap_main_func);
    pthread_cancel(wrapg->wrap_cli_sock);
    os_free(wrapg);
    exit(0);
}

int
wrapd_get_we_version(struct wrap_wifi_iface *aptr, char *ifname)
{
    struct iw_range *range;
    struct iwreq iwr;
    size_t buflen;

    aptr->we_version = 0;

    /*
     * Use larger buffer than struct iw_range in order to allow the
     * structure to grow in the future.
     */
    buflen = sizeof(struct iw_range) + 500;
    range = os_zalloc(buflen);
    if (range == NULL)
        return -1;

    memset(&iwr, 0, sizeof(iwr));
    os_strlcpy(iwr.ifr_name, ifname, IFNAMSIZ);
    iwr.u.data.pointer = (caddr_t) range;
    iwr.u.data.length = buflen;

    if (ioctl(wrapg->ioctl_sock, SIOCGIWRANGE, &iwr) < 0) {
        wrapd_printf("ERROR : ioctl[SIOCGIWRANGE]\n");
        os_free(range);
        return -1;
    }

    wrapd_printf(" aptr:%p SIOCGIWRANGE: Wireless extension version=%d\n", aptr, range->we_version_compiled);
    aptr->we_version = range->we_version_compiled;
    os_free(range);
    return 0;
}

int
wrapd_conn_to_mpsta_wpa_s(struct ctrl_conn *conn)
{
	char *cfile;
	int flen, conn_cnt;

	flen = strlen(wpa_s_ctrl_iface_dir) + (2*strlen(conn->ifname)) + 3;
	cfile = os_malloc(flen);
	if (cfile == NULL)
    {
        wrapd_printf("ERROR:malloc failed\n");
		return -1;
    }

    if(wrapd_get_we_version(conn->aptr, conn->ifname) != 0) {
        wrapd_printf("Failed to get we version\n");
    }
	snprintf(cfile, flen, "%s-%s/%s", wpa_s_ctrl_iface_dir, conn->ifname, conn->ifname);
    for (conn_cnt = 0; conn_cnt < WPA_S_CONN_TIMES; conn_cnt ++) {
	    conn->ctrl_iface = (struct wrapd_ctrl*)wpa_ctrl_open(cfile);
        if(conn->ctrl_iface) {
            wrapd_printf("MPSTA wpa_s(%s) connected\n", conn->ifname);
            break;
        }
        os_sleep(3, 0);
    }
    os_free(cfile);

    if(!conn->ctrl_iface) {
        wrapd_printf("MPSTA wpa_s(%s) not connected\n", conn->ifname);
        return -1;
    }

    if (wpa_ctrl_attach((struct wpa_ctrl*)conn->ctrl_iface) != 0) {
        wrapd_printf("Failed to attach MPSTA wpa_s(%s)\n", conn->ifname);
        return -1;
    }
    eloop_register_read_sock(conn->ctrl_iface->sock, wrapd_wpa_s_ctrl_iface_receive, conn->aptr, (void *)conn->ifname);
    return 0;
}

int wrapd_conn_to_hostapd(struct ctrl_conn *conn)
{
	char *cfile;
	int flen, conn_cnt;
	char wifi_ifname[IFNAMSIZ] = {0};

	wrapd_ifname_to_parent_ifname(wrapg, conn->ifname, wifi_ifname);
	flen = os_strlen(hostapd_ctrl_iface_dir) + strlen(wifi_ifname) + strlen(conn->ifname) + 3;
	cfile = os_malloc(flen);
	if (cfile == NULL)
    {
        wrapd_printf("ERROR:malloc failed\n");
        return -1;
    }

	snprintf(cfile, flen, "%s-%s/%s", hostapd_ctrl_iface_dir, wifi_ifname, conn->ifname);
  for (conn_cnt = 0; conn_cnt < HOSTAPD_CONN_TIMES; conn_cnt ++) {
        conn->ctrl_iface = (struct wrapd_ctrl*)wpa_ctrl_open(cfile);
        if(conn->ctrl_iface) {
            wrapd_printf("WRAP hostapd(%s) connected\n", conn->ifname);
            break;
        }
        os_sleep(1, 0);
    }

    os_free(cfile);

    if(!conn->ctrl_iface) {
        wrapd_printf("WRAP hostapd(%s) not connected", conn->ifname);
        return -1;
    }

    if (wpa_ctrl_attach((struct wpa_ctrl*)conn->ctrl_iface) != 0) {
        wrapd_printf("Failed to attach to WRAP hostapd(%s)", conn->ifname);
        wpa_ctrl_close((struct wpa_ctrl *)conn->ctrl_iface);
        return -1;
    }
    eloop_register_read_sock(conn->ctrl_iface->sock, wrapd_hostapd_ctrl_iface_receive, conn, NULL);
    return 0;
}



static wrapd_status_t
wrapd_ctrl_iface_cmd(struct wpa_ctrl *ctrl, char *cmd)
{
    char buf[2048];
    size_t len = sizeof(buf);
    int ret;

    if (ctrl == NULL) {
        wrapd_printf("%s failed - not connected to ctrl iface\n", cmd);
        return -1;
    }

    ret = wpa_ctrl_request(ctrl, cmd, os_strlen(cmd), buf, &len, NULL);
    if (ret == -2) {
        wrapd_printf("'%s' command timed out", cmd);
        return WRAPD_STATUS_ETIMEDOUT;
    } else if (ret < 0) {
        wrapd_printf("'%s' command failed", cmd);
        return WRAPD_STATUS_FAILED;
    }
    buf[len] = '\0';
    wrapd_printf("%s:%s", cmd, buf);

    return WRAPD_STATUS_OK;
}

static void
wrapd_ifindex_to_ifname(int index, char *ifname)
{
    struct ifreq ifr;

    os_memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_ifindex = index;
    if (ioctl(wrapg->ioctl_sock, SIOCGIFNAME, &ifr) != 0) {
//        wrapd_printf("ioctl SIOCGIFNAME err");
        return;
    }
    os_memcpy(ifname, ifr.ifr_name, IFNAMSIZ);
}


#if PROXY_NOACK_WAR
void
wrapd_check_proxy_macaddr_ok(wrapd_hdl_t *handle, void *macaddr, int *status)
{
    struct extended_ioctl_wrapper extended_cmd;
    int ret=0;
    struct wrap_wifi_iface *aptr = (struct wrap_wifi_iface *)handle;

    memset(&extended_cmd.ext_data.qwrap_config, 0, sizeof(struct qwrap_config_t));

    extended_cmd.cmd = EXTENDED_SUBIOCTL_OL_RESERVE_PROXY_MACADDR;
    extended_cmd.data_len = sizeof(extended_cmd.ext_data.qwrap_config);
    os_memcpy(extended_cmd.ext_data.qwrap_config.addr, macaddr, IEEE80211_ADDR_LEN);
    extended_cmd.ext_data.qwrap_config.status = -1;
    ret = driver->send_command(aptr->wifi_ifname, (void *)&extended_cmd,
        sizeof(extended_cmd), QCA_NL80211_VENDOR_SUBCMD_EXTENDEDSTATS,
        SIOCGATHEXTENDED, &(aptr->wptr->sock));
    if(ret != 0) {
        wrapd_printf("ioctl SIOCGATHEXTENDED get_ast_info err");
    }
    *status = extended_cmd.ext_data.qwrap_config.status;
    wrapd_printf(" Macaddr :%02xx:%02x:%02x:%02x:%02x:%02x Status=%d",
        extended_cmd.ext_data.qwrap_config.addr[0],
        extended_cmd.ext_data.qwrap_config.addr[1],
        extended_cmd.ext_data.qwrap_config.addr[2],
        extended_cmd.ext_data.qwrap_config.addr[3],
        extended_cmd.ext_data.qwrap_config.addr[4],
        extended_cmd.ext_data.qwrap_config.addr[5],*status);
}
#endif


static wrapd_status_t
wrapd_wpa_s_cmd(wrapd_hdl_t *mctx, char *cmd)
{
    char buf[2048];
    size_t len = sizeof(buf);
    int ret;

    if (wrapg->global_wpa_supp == NULL) {
        wrapd_printf("Not connected to global wpa_supplicant");
        return -1;
    }

    ret = wpa_ctrl_request(wrapg->global_wpa_supp, cmd, os_strlen(cmd), buf, &len, NULL);
    if (ret == -2) {
        wrapd_printf("'%s' command timed out", cmd);
        return WRAPD_STATUS_ETIMEDOUT;
    } else if (ret < 0) {
        wrapd_printf("'%s' command failed", cmd);
        return WRAPD_STATUS_FAILED;
    }
    buf[len] = '\0';
    //wrapd_printf("%s", buf);

    return WRAPD_STATUS_OK;
}

static wrapd_status_t
wrapd_psta_if_add(wrapd_hdl_t *mctx, void *if_uctx, char *ifname_plus, char *ifname_dev)
{
    char cmd[WRAP_MAX_CMD_LEN] = {0};
    size_t res;
    int ret_val;
    int ret = 0;

    res = snprintf(cmd, sizeof(cmd), "INTERFACE_ADD %s", ifname_plus);

    if (res < 0 || res >= sizeof(cmd))
        return WRAPD_STATUS_BAD_ARG;

    cmd[sizeof(cmd) - 1] = '\0';

    ret_val =  wrapd_wpa_s_cmd(mctx, cmd);

    /*Create a .lock file for the interface*/
    if(ret_val == WRAPD_STATUS_OK) {
        snprintf(cmd, sizeof(cmd), "touch /var/run/wpa_supplicant-%s.lock", ifname_dev);
        cmd[sizeof(cmd) - 1] = '\0';
        ret = system(cmd);
    }
    if(ret<0)
	return ret;

    return ret_val;
}

wrapd_status_t
wrapd_psta_if_remove(wrapd_hdl_t *mctx, char *ifname)
{
    char cmd[WRAP_MAX_CMD_LEN] = {0};
    size_t res;
    int ret_val;
    int ret = 0;

    res = snprintf(cmd, sizeof(cmd), "INTERFACE_REMOVE %s", ifname);
    if (res < 0 || res >= sizeof(cmd))
        return WRAPD_STATUS_BAD_ARG;

    cmd[sizeof(cmd) - 1] = '\0';

    ret_val =  wrapd_wpa_s_cmd(mctx, cmd);

    /*Remove the corresponding .lock file for this interface*/
    if(ret_val == WRAPD_STATUS_OK) {
        snprintf(cmd, sizeof(cmd), "rm /var/run/wpa_supplicant-%s.lock", ifname);
        cmd[sizeof(cmd) - 1] = '\0';
        ret = system(cmd);
    }

    if(ret<0)
	return ret;

    return ret_val;
}

static void
wrapd_sta_list(struct wrap_wifi_iface *aptr)
{
    int i;
    u8 *oma, *vma;

    i = 0;
    printf("PSTA\tAP/wired\tstatus\t\tOMA\t\t\tVMA\n");
    while (i < WRAP_MAX_PSTA_NUM) {
        if(aptr->psta[i].added) {
            oma = aptr->psta[i].oma;
            vma = aptr->psta[i].vma;
            printf("ath%d%d\t%s\t\t%s\t%02x:%02x:%02x:%02x:%02x:%02x\t%02x:%02x:%02x:%02x:%02x:%02x\n",
                aptr->wifi_device_num,
                i + WRAP_PSTA_START_OFF,
                (aptr->psta[i].flags & WRAPD_PSTA_FLAG_WIRED)? "wired" : aptr->psta[i].child,
                (aptr->psta[i].connected)? "connected" : "disconnected",
                oma[0],oma[1],oma[2],oma[3],oma[4],oma[5],
                vma[0],vma[1],vma[2],vma[3],vma[4],vma[5]);
        }
        i ++;
    }
}

static void
wrapd_sta_list_all(struct wrapd_global *wptr)
{
    int i;
    u8 *oma, *vma;
    struct wrap_wifi_iface *aptr;

    printf("PSTA\tAP/wired\tstatus\t\tOMA\t\t\tVMA\t\t\tRadio\n");
    for(aptr = wptr->wrap_wifi_list ; aptr ; aptr = aptr->next)
    {
        i = 0;
        while (i < WRAP_MAX_PSTA_NUM) {
            if(aptr->psta[i].added) {
                oma = aptr->psta[i].oma;
                vma = aptr->psta[i].vma;
                printf("ath%d%d\t%s\t\t%s\t%02x:%02x:%02x:%02x:%02x:%02x\t%02x:%02x:%02x:%02x:%02x:%02x\t%s\n",
                    aptr->wifi_device_num,
                    i + WRAP_PSTA_START_OFF,
                    (aptr->psta[i].flags & WRAPD_PSTA_FLAG_WIRED)? "wired" : aptr->psta[i].child,
                    (aptr->psta[i].connected)? "connected" : "disconnected",
                    oma[0],oma[1],oma[2],oma[3],oma[4],oma[5],
                    vma[0],vma[1],vma[2],vma[3],vma[4],vma[5],
                    aptr->wifi_ifname);
            }
            i ++;
        }
    }
}
static void
wrapd_psta_conn(struct wrap_wifi_iface *aptr, int psta_off)
{
    struct proxy_sta *psta = NULL;
    char cmd[WRAP_MAX_CMD_LEN] = {0};
    int res, ifname_num;
    char ifname_dev[IFNAME_LEN] = {0};
    char *driver_type;

    psta = &aptr->psta[psta_off];

    psta->connected = 1;
    ifname_num = psta_off + WRAP_PSTA_START_OFF;
    driver_type = driver->get_driver_type();

    //add wpa_supplicant iface
    res = os_snprintf(cmd, sizeof(cmd),"ath%d%d\t%s\t%s\t%s\t%s\t%s",
                        aptr->wifi_device_num,
                        ifname_num,
                        aptr->wpa_conf_file,
                        driver_type,
                        "",
                        "",
                        "");
    if (res < 0 || res >= sizeof(cmd)){
        wrapd_printf("Fail to build wpa_s msg");
        return;
    }
    cmd[sizeof(cmd) - 1] = '\0';

    os_snprintf(ifname_dev, sizeof(ifname_dev),"ath%d%d", aptr->wifi_device_num,ifname_num);
    ifname_dev[sizeof(ifname_dev)-1] = '\0';

    wrapd_psta_if_add(aptr, NULL, cmd, ifname_dev);
    wrapd_printf("proxySTA %d is conn", psta_off);

}

static void
wrapd_psta_disconn(struct wrap_wifi_iface *aptr, int psta_off)
{
    struct proxy_sta *psta = NULL;
    char cmd[WRAP_MAX_CMD_LEN] = {0};
    int res, ifname_num;

    wrapd_printf("proxySTA %d is disconn", psta_off);

    psta = &aptr->psta[psta_off];

    psta->connected = 0;
    ifname_num = psta_off + WRAP_PSTA_START_OFF;
    //remove wpa_supplicant iface
    res = os_snprintf(cmd, sizeof(cmd), "ath%d%d", aptr->wifi_device_num, ifname_num);
    if (res < 0 || res >= sizeof(cmd)){
        wrapd_printf("Fail to build wpa_s del msg");
        return;
    }
    cmd[sizeof(cmd) - 1] = '\0';
    wrapd_psta_if_remove(aptr, cmd);

}

static int
wrapd_vap_create(struct wrap_wifi_iface *aptr, struct proxy_sta *psta, int ifname_num, const char *parent)
{
    struct ieee80211_clone_params cp;
    struct ifreq ifr;
    int res;
    int ret = 0;

    os_memset(&ifr, 0, sizeof(ifr));
    os_memset(&cp, 0, sizeof(cp));

    res = os_snprintf(cp.icp_name, sizeof(cp.icp_name), "ath%d%d", aptr->wifi_device_num, ifname_num);
    if (res < 0 || res >= sizeof(cp.icp_name)) {
        wrapd_printf("os_snprintf err");
        return -1;
    }
    cp.icp_name[IFNAMSIZ - 1] = '\0';
    cp.icp_opmode = IEEE80211_M_STA;
    cp.icp_flags = 0;


    if (psta->flags & WRAPD_PSTA_FLAG_MAT) {
        os_memcpy(cp.icp_bssid, psta->vma, IEEE80211_ADDR_LEN);
        os_memcpy(cp.icp_mataddr, psta->oma, IEEE80211_ADDR_LEN);
        cp.icp_flags |= IEEE80211_CLONE_MACADDR;
        cp.icp_flags |= IEEE80211_CLONE_MATADDR;
    } else {
        os_memcpy(cp.icp_bssid, psta->oma, IEEE80211_ADDR_LEN);
        cp.icp_flags |= IEEE80211_CLONE_MACADDR;
    }

    if(psta->flags & WRAPD_PSTA_FLAG_WIRED) {
        cp.icp_flags |= IEEE80211_WRAP_WIRED_STA;
    }

    ret = driver->create_vap(parent, (void *)&cp, sizeof(cp),
        QCA_NL80211_VENDOR_SUBCMD_CLONEPARAMS, SIOC80211IFCREATE,
        &(aptr->wptr->sock), aptr->wifi_device_num, ifname_num);

    if(ret != 0)
    {
        wrapd_printf("create_vap error\n");
        return -1;        
    }
    return 0;
}

static int
wrapd_vap_destroy(struct wrap_wifi_iface *aptr, int ifname_num)
{
    int res;
    res = driver->delete_vap(aptr->wifi_ifname, (void *)NULL, 0,
        QCA_NL80211_VENDOR_SUBCMD_CLONEPARAMS, SIOC80211IFDESTROY,
        &(aptr->wptr->sock), aptr->wifi_device_num, ifname_num);
    if(res != 0)
    {
        printf("Err:Could not delete Vap");
        return -1;
    }
    return 0;
}

static void
wrapd_conn_timer(void *eloop_ctx, void *timeout_ctx)
{
    struct wrap_wifi_iface *aptr = (struct wrap_wifi_iface *)eloop_ctx;
    struct proxy_sta *psta = NULL;
    int i, sec;
    static int cnt = 0;

    if (0 == aptr->mpsta_conn) {
        wrapd_printf("stop conn cause 0 == aptr->mpsta_conn");
        goto conn_done;
    }

    for (i = 0; i < WRAP_MAX_PSTA_NUM; i ++) {
        psta = &aptr->psta[i];
        if ((psta->added) && (0 == psta->connected)) {
            wrapd_psta_conn(aptr, i);
            if (cnt < 15)
                sec = (i/3) * 2 + 1;
            else
                sec = 6;
            wrapd_printf("<========delay %ds to connect PSTA %d========>", sec, i);
            eloop_register_timeout(sec, 0, wrapd_conn_timer, aptr, NULL);
            wrapg->in_timer = 1;
            cnt ++;
            return;
        }
    }

    wrapd_printf("%d connected",cnt);

conn_done:
    cnt = 0;
    wrapg->in_timer = 0;
    eloop_cancel_timeout(wrapd_conn_timer, aptr, NULL);
}

static void
wrapd_conn_all(struct wrap_wifi_iface *aptr)
{
    struct proxy_sta *psta = NULL;
    int i, cnt=0;

    if (0 == aptr->mpsta_conn) {
        wrapd_printf("stop conn cause 0 == aptr->mpsta_conn");
        return;
    }

    for (i = 0; i < WRAP_MAX_PSTA_NUM; i ++) {
        psta = &aptr->psta[i];
        if ((psta->added) && (0 == psta->connected)) {
            wrapd_psta_conn(aptr, i);
            cnt ++;
        }
    }

    wrapd_printf("%d connected",cnt);
}

void
wrapd_disconn_all(struct wrap_wifi_iface *aptr)
{
    struct proxy_sta *psta = NULL;
    int i;

    if (1 == aptr->mpsta_conn)
        return;

    for (i = 0; i < WRAP_MAX_PSTA_NUM; i ++) {
        psta = &aptr->psta[i];
        if ((psta->added) && (1 == psta->connected)) {
            wrapd_psta_disconn(aptr, i);
        }
    }
}

void send_message_to_brd(int success_flag, const u8 *addr, int operation)
{
    char buffer[1024];
    int s,nBytes;
    struct sockaddr_un dest;
    socklen_t addr_size;
    s=socket(AF_UNIX, SOCK_DGRAM, 0);
    if(s<0)
    {
        wrapd_printf("Socket creation error");
	return;
    }
    dest.sun_family = AF_UNIX;
    if(strlcpy(dest.sun_path, ADDRESS,SOCKET_ADDR_LEN) >= SOCKET_ADDR_LEN) {
        wrapd_printf("%s:ADDRESS too long.%s\n",__func__,ADDRESS);
        return;
    }
    addr_size= sizeof dest;
    buffer[0]= 0;
    if(operation==0)
    {
        buffer[1]=0;
    }
    else
    {
	buffer[1]=1;
    }
    if(success_flag==0)
    {
	buffer[2]=0;
    }
    else
    {
	buffer[2]=1;
    }
    os_memcpy(&buffer[3],addr,IEEE80211_ADDR_LEN);
    nBytes=10;
    sendto(s,buffer,nBytes,0,(struct sockaddr *)&dest, addr_size);
}

/* Return 1 if there is any collision in mac address.
 * Return 0 if there is no collision.
 */
int check_mac_address_collision(struct wrap_wifi_iface *aptr, struct proxy_sta *psta) {
    int i;
    for(i=0;i<WRAP_MAX_PSTA_NUM; i++) {
        if((aptr->psta[i].added) && !os_memcmp(psta->vma, aptr->psta[i].vma, IEEE80211_ADDR_LEN)) {
            return 1;
        }
    }
    return 0;
}

static psta_status_t
wrapd_psta_add(struct wrap_wifi_iface *aptr, const char *parent, char *child, const u8 *addr, u_int32_t flags)
{
    struct proxy_sta *psta = NULL;
    int res, ifname_num;
    int i;
    int ret = 0;
    int fst_unused = -1;
#if PROXY_NOACK_WAR
    int status = -1;
    int mac_reserve_try=0;
    u_int8_t temp=0x0, temp1=0x0;
#endif
    char cmd[WRAP_MAX_CMD_LEN] = {0};
    struct ctrl_conn *hapd = NULL;
    char *tool_type;

    if(child)
        hapd = global_ifname_to_ctrl_conn(aptr->wptr, child);

    if(addr == NULL) {
        wrapd_printf("IWEVREGISTERED with NULL addr");
        return PROXY_STA_CREATION_ERROR;
    }

    if((os_strcmp(parent, aptr->wifi_ifname)) != 0){
        wrapd_printf("QWRAP not enabled for this radio");
        return PROXY_STA_CREATION_ERROR;
    }

    wrapd_printf("addr(%02x:%02x:%02x:%02x:%02x:%02x)",
        addr[0],addr[1],addr[2],addr[3],addr[4],addr[5]);

    i = 0;
    while (i < WRAP_MAX_PSTA_NUM) {
	if((aptr->psta[i].added) && (os_memcmp(addr, aptr->psta[i].oma, IEEE80211_ADDR_LEN) == 0)) {
	    if (child) {
		os_strlcpy(aptr->psta[i].child, child, IFNAMSIZ);
            }
	    wrapd_printf("oma already exists");
            return PROXY_STA_CREATION_ERROR;
        } else {
            if ((fst_unused == -1) && (0 == aptr->psta[i].added)) {
                if ((flags & WRAPD_PSTA_FLAG_MAT) || (0 == aptr->psta[i].vma_loaded))
                    fst_unused = i;
            }
        }
        i ++;
    }

    if (fst_unused == -1) {
        if (flags & WRAPD_PSTA_FLAG_WIRED) {
	    send_message_to_brd(0,addr,1);
        }
	global_vap_limit_flag=1;
        wrapd_printf("proxySTA num is up to limit");
        return PROXY_STA_LIMIT_EXCEEDED;
    }

    psta = &aptr->psta[fst_unused];

    os_memcpy(psta->oma, addr, IEEE80211_ADDR_LEN);

    if (1 != psta->vma_loaded)
        os_memcpy(psta->vma, addr, IEEE80211_ADDR_LEN);

    if (flags & WRAPD_PSTA_FLAG_MAT) {
        psta->flags |= WRAPD_PSTA_FLAG_MAT;
        if (1 != psta->vma_loaded)
        {
            for(i=0; i<2; i++)
                psta->vma[i] = wrapg->wifiX_dev_addr[i+4];

            for(i=2; i<6; i++)
                psta->vma[i] = addr[i];

            psta->vma[0] &= ~(1);
            psta->vma[0] |= 0x02;
        }
    }

/* Check if there exists mac address collision in the vma table.
 * If there is collision, add 1 to the last byte of the mac address.
 * Do this recursively till there are no collisions.
 */
    while (check_mac_address_collision(aptr,psta)) {
        psta->flags |= WRAPD_PSTA_FLAG_MAT;
        psta->vma[5]++;
    }

    psta->added = 1;

    /* Parent AP interface of this client */
    psta->parent_ap = hapd;

#if PROXY_NOACK_WAR
    /* reserve proxy mac address from target */
    if (aptr->proxy_noack_war) {
        wrapd_check_proxy_macaddr_ok(aptr, psta->vma, &status);
        if(!(flags & WRAPD_PSTA_FLAG_MAT) && mac_reserve_try == 0 && status!=0) {
            psta->vma[0] |= 0x02;
            wrapd_check_proxy_macaddr_ok(aptr, psta->vma, &status);
            psta->flags |= WRAPD_PSTA_FLAG_MAT;
            mac_reserve_try++;
        }
        /* 3rd to 8th bit are used to derive incremental mac address */
        temp  = psta->vma[0] >> 2;
        temp1 = psta->vma[0] & 0x3;
        while (mac_reserve_try < 64 && status!=0) {
            temp = temp + 0x1;
            psta->vma[0] = (temp << 2 | temp1) & 0xff;
            wrapd_check_proxy_macaddr_ok(aptr, psta->vma, &status);
            mac_reserve_try++;
		if(status==0) {
                break;
            }
        }
    }
#endif

    if (flags & WRAPD_PSTA_FLAG_WIRED)
        psta->flags |= WRAPD_PSTA_FLAG_WIRED;

    if (flags & WRAPD_PSTA_FLAG_OPEN)
        psta->flags |= WRAPD_PSTA_FLAG_OPEN;

    ifname_num = fst_unused + WRAP_PSTA_START_OFF;

    //create ProxySTA VAP
    res = wrapd_vap_create(aptr, psta, ifname_num, parent);
    if (res != 0){
        wrapd_printf("Fail to create ProxySTA VAP");
        psta->added = 0;
        psta->flags = 0;
        if (flags & WRAPD_PSTA_FLAG_WIRED) {
            send_message_to_brd(0,addr,1);
        }
	if(res==5)
	{
		global_vap_limit_flag=1;
	}
        os_memset(psta->oma, 0, IEEE80211_ADDR_LEN);
        return PROXY_STA_CREATION_ERROR;
    }
    else{
        if (flags & WRAPD_PSTA_FLAG_WIRED) {
	    send_message_to_brd(1,addr,1);
        }
    }
    tool_type = driver->get_tool_type();
    os_snprintf(cmd, sizeof(cmd),"%s ath%d%d shortgi 1",tool_type, aptr->wifi_device_num, ifname_num);
    wrapd_printf("%s Setting command: %s \n", __func__, cmd);
    ret = system(cmd);

    os_snprintf(cmd, sizeof(cmd),"%s ath%d%d dbgLVL %s",tool_type, aptr->wifi_device_num, ifname_num, wrapg->dbglvl);
    ret = system(cmd);

    os_snprintf(cmd, sizeof(cmd),"%s ath%d%d dbgLVL_high %s",tool_type, aptr->wifi_device_num, ifname_num, wrapg->dbglvl_high);
    ret = system(cmd);

    if(ret<0)
	return ret;

    os_strlcpy(psta->parent, parent, IFNAMSIZ);
    if (child) {
        os_strlcpy(psta->child, child, IFNAMSIZ);
    } else {
        os_memset(psta->child, 0, IFNAMSIZ);
    }
    wrapd_printf("proxySTA %d is added to %s", fst_unused, aptr->wifi_ifname);

    if (1 == aptr->mpsta_conn) {
        if ((flags & WRAPD_PSTA_FLAG_OPEN) || (0 == wrapg->do_timer) ) {
            wrapd_psta_conn(aptr, fst_unused);

        } else {
            if((0 == wrapg->in_timer) ) {
                eloop_register_timeout(1, 0, wrapd_conn_timer, aptr, NULL);
                wrapg->in_timer = 1;
            }
        }
    }
    return PROXY_STA_SUCCESS;
}

static void
wrapd_psta_remove(struct wrap_wifi_iface *aptr, const u8 *addr, int msgtobrd, char *child)
{
    struct proxy_sta *psta = NULL;
    int res, i, ifname_num;

    if(addr == NULL) {
        wrapd_printf("IWEVEXPIRED with NULL addr");
        return;
    }

    wrapd_printf(" psta remove: addr(%02x:%02x:%02x:%02x:%02x:%02x)",
        addr[0],addr[1],addr[2],addr[3],addr[4],addr[5]);

    i = 0;
    while (i < WRAP_MAX_PSTA_NUM) {
        if((aptr->psta[i].added) && (os_memcmp(addr, aptr->psta[i].oma, IEEE80211_ADDR_LEN) == 0)) {
            psta = &aptr->psta[i];
            break;
        }
        i ++;
    }

    if(psta == NULL) {
        wrapd_printf("PSTA is null\n");
        return;
    }

    if(child && (os_strcmp(psta->child, child) != 0)) {
        wrapd_printf("psta->child:%s child:%s not matching\n",psta->child,child);
        return;
    }

    if (psta->flags & WRAPD_PSTA_FLAG_WIRED) {
	    wrapd_printf("Wired PSTA remove\n");
    } else {
	    wrapd_printf("Wireless PSTA remove\n");
    }


    wrapd_psta_disconn(aptr, i);

    ifname_num = i + WRAP_PSTA_START_OFF;

    //destory ProxySTA VAP
    res = wrapd_vap_destroy(aptr, ifname_num);

    if (res < 0) {
        if ((psta->flags & WRAPD_PSTA_FLAG_WIRED) && msgtobrd) {
	    send_message_to_brd(0,addr,0);
        }
        wrapd_printf("Fail to destroy ProxySTA VAP");
        return;
    }
    else{
        if ((psta->flags & WRAPD_PSTA_FLAG_WIRED) && msgtobrd) {
	    send_message_to_brd(1,addr,0);
        }
    }
    psta->added = 0;
    psta->flags = 0;

    os_memset(psta->oma, 0, IEEE80211_ADDR_LEN);
    os_memset(psta->parent, 0, IFNAMSIZ);
    os_memset(psta->child, 0, IFNAMSIZ);
    psta->parent_ap = NULL;

    wrapd_printf("proxySTA %d is removed from %s", i,aptr->wifi_ifname);

}

void link_ap_iface(struct wrap_wifi_iface *aptr, struct ctrl_conn *hapd)
{
    hapd->aptr = aptr;
    hapd->next = aptr->hapd_list;
    aptr->hapd_list = hapd;
}

void delink_ap_iface(struct wrap_wifi_iface *aptr, struct ctrl_conn *hapd)
{
    struct ctrl_conn *prev;
    /* Remove interface from the global list of interfaces */
    prev = aptr->hapd_list;
    if (prev == hapd) {
        aptr->hapd_list = hapd->next;
    } else {
        while (prev && prev->next != hapd)
            prev = prev->next;
        if (prev == NULL)
            return;
        prev->next = hapd->next;
    }
}
void remove_ap_iface(struct wrap_wifi_iface *aptr, struct ctrl_conn *hapd)
{
    delink_ap_iface(aptr, hapd);
    os_free(hapd);
}

void move_ap_ifaces(struct wrap_wifi_iface *src, struct wrap_wifi_iface *dest)
{
    struct ctrl_conn *conn, *next;

    conn = src->hapd_list;

    while(conn)
    {
        next = conn->next;
        delink_ap_iface(src, conn);
        link_ap_iface(dest, conn);
        conn = next;
    }
}

static void
wrapd_update_bridge_mac(struct wrap_wifi_iface *parent)
{
    struct ifreq ifr;

    wrapd_printf("wrapg->max_aptr = %s\n", wrapg->max_aptr->wifi_ifname);
    os_memset(&ifr, 0, sizeof(ifr));
    if(strlcpy(ifr.ifr_name, wrapg->brname, IFNAMSIZ) >= IFNAMSIZ) {
        wrapd_printf("%s:brname too long.%s\n",__func__,wrapg->brname);
        return;
    }
    ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;
    os_memcpy(ifr.ifr_hwaddr.sa_data, wrapg->max_aptr->mpsta_mac_addr, IEEE80211_ADDR_LEN);
    /* Get the MAC address */
    if(ioctl(wrapg->ioctl_sock, SIOCSIFHWADDR, &ifr) < 0)
    {
	wrapd_printf("SIOCSIFHWADDR - Unable to set bridge hw address\n");
	return;
    }

}

static void
wrapd_update_max_priority_radio(struct wrap_wifi_iface *parent)
{
    char max_priority_radio[IFNAMSIZ] = {0};
    int ret;
    struct extended_ioctl_wrapper extended_cmd;
    struct wrap_wifi_iface *other = NULL;

    memset(&extended_cmd.ext_data.qwrap_config, 0, sizeof(struct qwrap_config_t));
    extended_cmd.cmd = EXTENDED_SUBIOCTL_GET_MAX_PRIORITY_RADIO;
    extended_cmd.data_len = sizeof(extended_cmd.ext_data.qwrap_config);
    os_memcpy(extended_cmd.ext_data.qwrap_config.max_priority_radio,
        max_priority_radio, IFNAMSIZ);
    ret = driver->send_command(parent->wifi_ifname, (void *)&extended_cmd,
        sizeof(extended_cmd), QCA_NL80211_VENDOR_SUBCMD_EXTENDEDSTATS,
        SIOCGATHEXTENDED, &(parent->wptr->sock));

    if (ret != 0) {
        wrapd_printf("ret=%d ioctl SIOCGATHEXTENDED get_max_priority_active_radio err", ret);
        return;
    } else {
        os_memcpy(max_priority_radio,
            extended_cmd.ext_data.qwrap_config.max_priority_radio, IFNAMSIZ);
            wrapd_printf("Max priority active radio = %s\n", max_priority_radio);
    }

    if(max_priority_radio[0] == 0) {
        return;
    }
    for(other= wrapg->wrap_wifi_list; other ; other=other->next )
    {
        if (os_strcmp(max_priority_radio, other->wifi_ifname) == 0)
        {
            wrapg->max_aptr = other;
            break;
        }
    }


}

void update_wired_client_parent(char *prim)
{
    char *cfile = NULL;
    int flen;

    if(wrapg == NULL)
    {
        wrapd_printf("Error: wrapg is NULL\n");
        return;
    }

    os_strlcpy(wrapg->ethernet_extender, prim, sizeof(wrapg->ethernet_extender));
    flen = os_strlen(wrapd_ctrl_iface_dir) + strlen(wrapg->ethernet_extender) + 2;
    cfile = os_malloc(flen);
    if (cfile == NULL)
    {
        wrapd_printf("ERROR:malloc failed\n");
        return;
    }

    snprintf(cfile, flen, "%s-%s", wrapd_ctrl_iface_dir, wrapg->ethernet_extender);
    if(strlcpy(wrapg->wrapd_ctrl_intf, cfile, 128) >= 128) {
        wrapd_printf("%s:cfile too long.%s\n",__func__,cfile);
    }
    os_free(cfile);
}

static void wrapd_max_aptr_mpsta_changed(struct wrap_wifi_iface *parent)
{
    u_int32_t flags;
    u8 addr[IEEE80211_ADDR_LEN];
    psta_status_t retv;
    int i;
    struct extended_ioctl_wrapper extended_cmd;
    int ret;
    int force_client_mcast = 0;
    struct wrap_wifi_iface *other;

    memset(&extended_cmd.ext_data.qwrap_config, 0, sizeof(struct qwrap_config_t));
    extended_cmd.cmd = EXTENDED_SUBIOCTL_GET_FORCE_CLIENT_MCAST;
    extended_cmd.data_len = sizeof(extended_cmd.ext_data.qwrap_config);
    extended_cmd.ext_data.qwrap_config.force_client_mcast = force_client_mcast;
    ret = driver->send_command(parent->wifi_ifname, (void *)&extended_cmd,
        sizeof(extended_cmd), QCA_NL80211_VENDOR_SUBCMD_EXTENDEDSTATS,
        SIOCGATHEXTENDED, &(parent->wptr->sock));
    if (ret != 0) {
        wrapd_printf("ret=%d ioctl SIOCGATHEXTENDED get_force_client_mcast err", ret);
    } else {
        force_client_mcast = extended_cmd.ext_data.qwrap_config.force_client_mcast;
        wrapd_printf("force_client_mcast: %d \n", force_client_mcast);
    }

    if((os_strcmp(wrapg->max_aptr->wifi_ifname, wrapg->ethernet_extender)) != 0)
    {
        update_wired_client_parent(wrapg->max_aptr->wifi_ifname);
    }

    for(other= wrapg->wrap_wifi_list; other ; other=other->next)
    {
        if (other != parent)
        {
            i = 0;
            while (i < WRAP_MAX_PSTA_NUM) {
                if(other->psta[i].added)
		{
		    flags = other->psta[i].flags;
		    os_memcpy(addr, other->psta[i].oma, IEEE80211_ADDR_LEN);
		    if (force_client_mcast && !(IS_PSTA_WIRED(other->psta[i]))) {
			/* During max radio stavap connection, if force_client_mcast is set,
                           then wireless clients will get disassociated and reconnect again.
                           So, remove all wireless PSTAs here, to avoid creation of
                           multiple PSTAs for single client due to missing of client
                           disconnection event on wrapd.*/
                        wrapd_psta_remove(other, addr, 0, other->psta[i].child);
                        i ++;
                        continue;
		    }
		    wrapd_printf("Creating PSTA on %s flags:%x\n", parent->wifi_ifname,flags);
		    retv = wrapd_psta_add(parent, parent->wifi_ifname, other->psta[i].child, addr, flags);

		    if (retv == PROXY_STA_SUCCESS)
		    {
			wrapd_printf("Removing PSTA on %s \n", other->wifi_ifname);
			wrapd_psta_remove(other, addr, 0, other->psta[i].child);
		    }
		    else if (retv == PROXY_STA_LIMIT_EXCEEDED)
		    {
			wrapd_printf("PSTA vap limit exceeded, Failsafe move to %s failed\n", parent->wifi_ifname);
			/*
			 * Remove clients that cannot be shifted to parent
			 * - no priority for clients based on order of connection
			 * - psta's are added based on availablity of space at that instant
			 */
			wrapd_psta_remove(other, addr, 1, other->psta[i].child);
			break;
		    }
		}
                i ++;
            }
        }
    }
    if(force_client_mcast) {
        memset(&extended_cmd.ext_data.qwrap_config, 0, sizeof(struct qwrap_config_t));
        extended_cmd.cmd = EXTENDED_SUBIOCTL_DISASSOC_CLIENTS;
        extended_cmd.data_len = sizeof(extended_cmd.ext_data.qwrap_config);
        ret = driver->send_command(parent->wifi_ifname, (void *)&extended_cmd,
             sizeof(extended_cmd), QCA_NL80211_VENDOR_SUBCMD_EXTENDEDSTATS,
             SIOCGATHEXTENDED, &(parent->wptr->sock));
        if (ret != 0) {
            wrapd_printf("ret=%d ioctl SIOCGATHEXTENDED disassoc clients err", ret);
        } else {
            wrapd_printf("DISASSOC_CLIENTS cmds passsed to driver\n");
        }

    }
}

static void
wrapd_move_wired_pstas_to_max_aptr(struct wrap_wifi_iface *parent)
{
    u_int32_t flags;
    u8 addr[IEEE80211_ADDR_LEN];
    psta_status_t retv;
    int i;
    struct wrap_wifi_iface *other =NULL;

    if((os_strcmp(wrapg->max_aptr->wifi_ifname, wrapg->ethernet_extender)) != 0)
    {
        update_wired_client_parent(wrapg->max_aptr->wifi_ifname);
    }

    for(other= wrapg->wrap_wifi_list; other ; other=other->next)
    {
	if (other != wrapg->max_aptr)
	{
	    i = 0;
	    while (i < WRAP_MAX_PSTA_NUM) {
		if((other->psta[i].added) && (IS_PSTA_WIRED(other->psta[i]))) {
		    flags = other->psta[i].flags;
		    os_memcpy(addr, other->psta[i].oma, IEEE80211_ADDR_LEN);

		    wrapd_printf("Creating PSTA on %s flags:%x\n", wrapg->max_aptr->wifi_ifname,flags);
		    retv = wrapd_psta_add(wrapg->max_aptr, wrapg->max_aptr->wifi_ifname, NULL, addr, flags);

		    if (retv == PROXY_STA_SUCCESS)
		    {
			wrapd_printf("Removing PSTA on %s \n", other->wifi_ifname);
			wrapd_psta_remove(other, addr, 0, NULL);
		    }
		    else if (retv == PROXY_STA_LIMIT_EXCEEDED)
		    {
			wrapd_printf("PSTA vap limit exceeded, Failsafe move to %s failed\n", wrapg->max_aptr->wifi_ifname);
			wrapd_psta_remove(other, addr, 1, NULL);
			break;
		    }
		}
		i ++;
	    }
	}
    }
}

static void
wrapd_wireless_event_wireless(struct wrap_wifi_iface *aptr, struct ifinfomsg *ifi,
				                        char *data, int len)
{
    struct iw_event iwe_buf, *iwe = &iwe_buf;
    char *pos, *end, *custom;
    char child[IFNAMSIZ] = {0};
    u_int32_t flags = WRAPD_PSTA_FLAG_OPEN;
    struct ctrl_conn *hapd = NULL;
    struct wrap_wifi_iface *dev = NULL;

    wrapd_ifindex_to_ifname(ifi->ifi_index, child);

    hapd = wifi_ifname_to_ctrl_conn(aptr, child);

    pos = data;
    end = data + len;

    while (pos + IW_EV_LCP_LEN <= end) { //+4
        os_memcpy(&iwe_buf, pos, IW_EV_LCP_LEN);
        if (iwe->len <= IW_EV_LCP_LEN)
            return;

        custom = pos + IW_EV_POINT_LEN;
        if(aptr->we_version > 18 && iwe->cmd == IWEVCUSTOM)
        {
            /* WE-19 removed the pointer from struct iw_point */
            char *dpos = (char *) &iwe_buf.u.data.length;
            int dlen = dpos - (char *) &iwe_buf;
            memcpy(dpos, pos + IW_EV_LCP_LEN,
                   sizeof(struct iw_event) - dlen);

        }
        else {
            os_memcpy(&iwe_buf, pos, sizeof(struct iw_event));
            custom += IW_EV_POINT_OFF;
        }

	if((!hapd || hapd->secmode == SEC) && (iwe->cmd != IWEVCUSTOM)) {
	    /* wrapd not listening to this interface */
	    return;
	}
        switch (iwe->cmd) {


        case IWEVEXPIRED:
            if(wrapg->wrapd_psta_config == 2)
            {
                for(dev = wrapg->wrap_wifi_list; dev; dev=dev->next)
                {
                    wrapd_psta_remove(dev,(u8 *) iwe->u.addr.sa_data, 1, child);
                }
            }
            else if(wrapg->wrapd_psta_config == 1)
            {
                for(dev = wrapg->wrap_wifi_list; dev; dev=dev->next)
                {
                    wrapd_psta_remove(dev,(u8 *) iwe->u.addr.sa_data, 1, child);
                }
            }
            else
            {
                wrapd_psta_remove(aptr,(u8 *) iwe->u.addr.sa_data, 1, child);
            }
            break;

        case IWEVREGISTERED:
            /*
             * If AP-VAP(in 2G/5G radio of QWRAP) is in a channel different from that of Root AP
             * MAT is not required as there will not be any ACK cloberring - packets
             * from Root will not be heard by clients behind AP-VAP
             *
             * Still including MAT for following reason -
             * When STA roams from 2G radio to 5G radio of our platform, 2 AST entries might need
             * to be created
             *  - 1 for peer that has joined the 5G offload radio
             *  - another entry already exists corresp to Proxy vap created for sta when it was
             *  connected to 2G radio
             *
             * 2 AST entries with same macaddr is not allowed per FW impl.
             */
            flags |= WRAPD_PSTA_FLAG_MAT;
            if(wrapg->wrapd_psta_config == 2)
            {
                wrapd_psta_add(wrapg->max_aptr, wrapg->max_aptr->wifi_ifname, child, (u8 *)(iwe->u.addr.sa_data), flags);
            }
            else if (wrapg->wrapd_psta_config == 1)
            {
                for(dev = wrapg->wrap_wifi_list; dev; dev=dev->next)
                {
                    wrapd_psta_add(dev, dev->wifi_ifname, child, (u8 *)(iwe->u.addr.sa_data), flags);
                }
            }
            else
            {
                wrapd_psta_add(aptr, aptr->wifi_ifname, child, (u8 *)(iwe->u.addr.sa_data), flags);
            }
            break;
    	case IWEVASSOCREQIE:
            break;
        case IWEVCUSTOM:
	    {
		u_int16_t opcode = iwe->u.data.flags;
		struct wrap_wifi_iface *last_max_aptr;

		if ((opcode == IEEE80211_EV_IF_NOT_RUNNING) && (hapd != NULL) && (hapd->remove_from_user)) {
		    wrapd_printf("Removing ap vap");
		    remove_ap_iface(aptr, hapd);
		    hapd = NULL;
		}
		if (opcode == IEEE80211_EV_PRIMARY_RADIO_CHANGED) {
		    wrapd_printf("IWEVCUSTOM event recvd opcode :%d IEEE80211_EV_PRIMARY_RADIO_CHANGED:%d\n",opcode,IEEE80211_EV_PRIMARY_RADIO_CHANGED);

		    last_max_aptr = wrapg->max_aptr;
		    wrapd_update_max_priority_radio(aptr);

		    if (last_max_aptr != wrapg->max_aptr) {
			wrapd_update_bridge_mac(aptr);
			if (wrapg->wrapd_psta_config == 2) {
		            wrapd_max_aptr_mpsta_changed(wrapg->max_aptr);
			} else {
			    if (wrapg->wrapd_psta_config == 0) {
				wrapd_move_wired_pstas_to_max_aptr(aptr);
			    }
			}
		    }
		}
	    }
	    break;
	}

        pos += iwe->len;
    }
}

static void
wrapd_event_rtm_newlink(void *ctx, struct ifinfomsg *ifi, u8 *buf, size_t len)
{
    struct wrap_wifi_iface *aptr = ctx;
    int attrlen, rta_len;
    struct rtattr *attr;

    attrlen = len;
    attr = (struct rtattr *) buf;

    rta_len = RTA_ALIGN(sizeof(struct rtattr));
    while (RTA_OK(attr, attrlen)) {
        if (attr->rta_type == IFLA_WIRELESS) {
            wrapd_wireless_event_wireless(aptr, ifi, ((char *) attr) + rta_len,
                attr->rta_len - rta_len);
        }
        attr = RTA_NEXT(attr, attrlen);
    }

}

struct wifi_cmd_args {
    const char *wifi_ifname;
    const char *vma_confname;
};

int process_wifi_add_cmd(char *cmd, struct wifi_cmd_args *args)
{
   /*
    * <ifname>TAB<vma_confname>
    */
    char *pos, *extra;

    do {
        args->wifi_ifname = pos = cmd;
        pos = os_strchr(pos, '\t');
        if (pos)
            *pos++ = '\0';
        else
            return -1;

        if (args->wifi_ifname[0] == '\0')
            return -1;

        if(pos == NULL)
            return -1;

        args->vma_confname = pos;
        pos = os_strchr(pos, '\t');
        if (pos)
            *pos++ = '\0';
        else
            return -1;

        if (args->vma_confname[0] == '\0')
            return -1;

        extra = pos;
        pos = os_strchr(pos, '\t');
        if (pos)
            *pos++ = '\0';
        if (!extra[0])
            break;
        else {
            wrapd_printf("unsupported extra parameter %s\n", extra);
            return -1;
        }
    } while(0);

    return 0;
}

int process_wifi_remove_cmd(char *cmd, struct wifi_cmd_args *args)
{
   /*
    * <ifname>
    */
    char *pos, *extra;

    do {
        args->wifi_ifname = pos = cmd;
        pos = os_strchr(pos, '\t');
        if (pos)
            *pos++ = '\0';
        else
            return -1;

        if (args->wifi_ifname[0] == '\0')
            return -1;

        extra = pos;
        pos = os_strchr(pos, '\t');
        if (pos)
            *pos++ = '\0';
        if (!extra[0])
            break;
        else {
            wrapd_printf("unsupported extra parameter %s\n", extra);
            return -1;
        }
    } while(0);

    return 0;
}

struct cmd_args {
    const char *wifiname;
    const char *ifname;
    const char *confname;
    const char *cmd_type;
    sectype_t secmode;
    cmdtype_t cmdmode;
};

int process_add_cmd(char *cmd, struct cmd_args *args)
{
   /*
    * <wifiname>TAB<ifname>TAB<confname>TAB<cmd_type>TAB
    */
    char *pos, *extra;
    args->cmdmode = RUNTIME;

    do {
        args->wifiname = pos = cmd;
        pos = os_strchr(pos, '\t');
        if (pos)
            *pos++ = '\0';
        else
            return -1;

        if (args->wifiname[0] == '\0')
            return -1;

        if(pos == NULL)
            return -1;

        args->ifname = pos;
        pos = os_strchr(pos, '\t');
        if (pos)
            *pos++ = '\0';
        else
            return -1;

        if (args->ifname[0] == '\0')
            return -1;

        if(pos == NULL)
            return -1;

        args->confname = pos;
        pos = os_strchr(pos, '\t');
        if (pos)
            *pos++ = '\0';
        else
            return -1;

        if (args->confname[0] == '\0')
            return -1;

        if(os_strcmp(args->confname, "OPEN") == 0) {
            args->secmode = OPEN;
        } else {
            args->secmode = SEC;
        }

        args->cmd_type = pos;
        extra = pos;
        if (!extra[0]) {
            break;
        }
        pos = os_strchr(pos, '\t');
        if (pos)
            *pos++ = '\0';
        else
            return -1;

        if(os_strcmp(args->cmd_type, "INIT") == 0) {
            args->cmdmode = INIT;
        } else if(os_strcmp(args->cmd_type, "RUNTIME") == 0) {
            args->cmdmode = RUNTIME;
        } else {
            goto extra;
        }

        extra = pos;
        pos = os_strchr(pos, '\t');
        if (pos)
            *pos++ = '\0';
extra:
        if (!extra[0]) {
            break;
        }
        else {
            wrapd_printf("unsupported extra parameter %s\n", extra);
            return -1;
        }
    } while(0);

    return 0;
}

int process_remove_cmd(char *cmd, struct cmd_args *args)
{
   /*
    * <wifiname>TAB<ifname>TAB
    */
    char *pos, *extra;

    do {
        args->wifiname = pos = cmd;
        pos = os_strchr(pos, '\t');
        if (pos)
            *pos++ = '\0';
        else
            return -1;

        if (args->wifiname[0] == '\0')
            return -1;

        if(pos == NULL)
            return -1;

        args->ifname = pos;
        pos = os_strchr(pos, '\t');
        if (pos)
            *pos++ = '\0';
        else
            return -1;

        if (args->ifname[0] == '\0')
            return -1;

        extra = pos;
        pos = os_strchr(pos, '\t');
        if (pos)
            *pos++ = '\0';
        if (!extra[0])
            break;
        else {
            wrapd_printf("unsupported extra parameter %s\n", extra);
            return -1;
        }
    } while(0);

    return 0;
}

struct ctrl_conn*  wrapd_get_ap_iface(struct wrap_wifi_iface *aptr, const char *ifname)
{
    struct ctrl_conn *conn;

    for(conn = aptr->hapd_list ; conn ; conn = conn->next)
    {
        if (os_strcmp(conn->ifname, ifname) == 0) {
            return conn;
        }
    }
    return NULL;

}
struct ctrl_conn*  wrapd_get_sta_iface(struct wrap_wifi_iface *aptr, const char *ifname)
{
    struct ctrl_conn *conn = aptr->wpa_s;

    if (conn && os_strcmp(conn->ifname, ifname) == 0) {
        return conn;
    }
    return NULL;

}


static int
wrapd_ap_add_to_global(struct wrap_wifi_iface *aptr, struct cmd_args *args)
{
    char cmd[WRAP_MAX_CMD_LEN] = {0};
    int res;

    //add iface to hostapd global
    res = os_snprintf(cmd, sizeof(cmd),"ADD bss_config=%s:%s",
                       args->ifname,
                       args->confname);
    if (res < 0 || res >= sizeof(cmd)){
        wrapd_printf("Fail to build add hostapd iface msg");
        return -1;
    }
    cmd[sizeof(cmd) - 1] = '\0';

    res = wrapd_ctrl_iface_cmd(wrapg->global_hostapd, cmd);
    wrapd_printf("ap vap %s is added to global hostapd", args->ifname);
    return res;

}


static int
wrapd_ap_disassociate_stas(struct wpa_ctrl *ctrl)
{
    char cmd[WRAP_MAX_CMD_LEN] = {0};
    int res;

    /* enable / disable interface */
    res = os_snprintf(cmd, sizeof(cmd),"DISASSOCIATE ff:ff:ff:ff:ff:ff");

    if (res < 0 || res >= sizeof(cmd)){
        wrapd_printf("Fail to build enable/disable hostapd iface msg");
        return -1;
    }
    cmd[sizeof(cmd) - 1] = '\0';

    res = wrapd_ctrl_iface_cmd(ctrl, cmd);
    return res;

}

int
wrapd_remove_to_global(struct wrap_wifi_iface *aptr, const char *ifname)
{
    char cmd[WRAP_MAX_CMD_LEN] = {0};
    int res;

    //add iface to hostapd global
    res = os_snprintf(cmd, sizeof(cmd),"REMOVE %s",
                       ifname);

    if (res < 0 || res >= sizeof(cmd)){
        wrapd_printf("Fail to build remove hostapd iface msg");
        return -1;
    }
    cmd[sizeof(cmd) - 1] = '\0';

    res = wrapd_ctrl_iface_cmd(wrapg->global_hostapd, cmd);
    wrapd_printf("ap vap %s is removed from global hostapd", ifname);
    return res;

}
static int ifconfig_helper(const char *if_name, int up)
{
    int fd;
    struct ifreq ifr;

    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        wrapd_printf("%s: socket(AF_INET,SOCK_STREAM)", if_name);
        return -1;
    }

    os_memset(&ifr, 0, sizeof(ifr));
    os_strlcpy(ifr.ifr_name, if_name, IFNAMSIZ);

    if (ioctl(fd, SIOCGIFFLAGS, &ifr) != 0) {
        wrapd_printf("%s: ioctl(SIOCGIFFLAGS) failed", if_name);
        close(fd);
        return -1;
    }

    if (up)
        ifr.ifr_flags |= IFF_UP;
    else
        ifr.ifr_flags &= ~IFF_UP;

    if (ioctl(fd, SIOCSIFFLAGS, &ifr) != 0) {
        wrapd_printf("%s: ioctl(SIOCGIFFLAGS) failed up=%d", if_name, up);
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}


int ifconfig_up(const char *if_name)
{
    wpa_printf(MSG_DEBUG, "VLAN: Set interface %s up", if_name);
    return ifconfig_helper(if_name, 1);
}


int ifconfig_down(const char *if_name)
{
    wpa_printf(MSG_DEBUG, "VLAN: Set interface %s down", if_name);
    return ifconfig_helper(if_name, 0);
}

struct wrap_wifi_iface*  wrapd_get_wifi_iface(struct wrapd_global *wptr, const char *ifname)
{
    struct wrap_wifi_iface *dev;

    for(dev = wptr->wrap_wifi_list ; dev ; dev = dev->next)
    {
        if (os_strcmp(dev->wifi_ifname, ifname) == 0) {
            return dev;
        }
    }
    return NULL;

}

struct wrap_wifi_iface* wrapd_alloc_wifi_iface(struct wrapd_global *wptr, struct wifi_cmd_args *args)
{
    struct wrap_wifi_iface *dev;
    struct ifreq ifr;
    char *wrapd_path;
    int path_len;
    const char *wrapd_ctrl_iface_path = "/var/run/wrapd";
    struct netlink_config *cfg;
#if PROXY_NOACK_WAR
    struct extended_ioctl_wrapper extended_cmd;
    int ret;
#endif

    dev = os_zalloc(sizeof(struct wrap_wifi_iface));
    if(dev == NULL) {
        wrapd_printf("ERROR: dev alloc failed\n");
        return NULL;
    }
    printf("\nAllocated wrap_wifi_iface %s:%d\n",__func__,__LINE__);
    dev->wptr = wptr;
    dev->next = wptr->wrap_wifi_list;
    wptr->wrap_wifi_list = dev;
    if (wptr->max_aptr == NULL) {
       wptr->max_aptr = dev;
    }
    if(args->wifi_ifname) {
        dev->wifi_ifname = os_strdup(args->wifi_ifname);
        if (dev->wifi_ifname == NULL) {
            goto error;
        }

        if((dev->wifi_device_num = wifi_ifname_to_num(dev->wifi_ifname)) == -1) {
            wrapd_printf("Wifi intf undefined\n");
            goto error1;
        }

        if(strlcpy(ifr.ifr_name, dev->wifi_ifname, IFNAMSIZ) >= IFNAMSIZ) {
            wrapd_printf("%s:Interface Name too long.%s\n",__func__,dev->wifi_ifname);
            goto error1;
        }

        /* Get the MAC address */
        if(ioctl(wrapg->ioctl_sock, SIOCGIFHWADDR, &ifr) < 0)
        {
            wrapd_printf("SIOCGIFHWADDR - Unable to fetch hw address\n");
            goto error1;
        }
        os_memcpy(dev->wifi_dev_addr, ifr.ifr_hwaddr.sa_data, IEEE80211_ADDR_LEN);

        if(os_strcmp(args->vma_confname, "NONE") == 0)
        {
            dev->vma_conf_file = NULL;
        }
        else
        {
            dev->vma_conf_file = os_strdup(args->vma_confname);
            if (dev->vma_conf_file == NULL)
            {
                wrapd_printf("malloc failed for vma_conf_file\n");
                goto error1;
            }
        }
    }
    else {
        goto error;
    }
#if PROXY_NOACK_WAR
    /* Check if proxy noack WAR is required */
    extended_cmd.cmd = EXTENDED_SUBIOCTL_OL_GET_PROXY_NOACK_WAR;
    extended_cmd.data_len = sizeof(extended_cmd.ext_data.qwrap_config);
    extended_cmd.ext_data.qwrap_config.proxy_noack_war = dev->proxy_noack_war;
    ret = driver->send_command(dev->wifi_ifname,(void *) &extended_cmd,
        sizeof(extended_cmd), QCA_NL80211_VENDOR_SUBCMD_EXTENDEDSTATS,
        SIOCGATHEXTENDED, &(wptr->sock));
    dev->proxy_noack_war = extended_cmd.ext_data.qwrap_config.proxy_noack_war;
    if (ret != 0) { 
        wrapd_printf("ioctl SIOCGATHEXTENDED get_proxy_noack_war err");
        goto error2;
    } else {
        wrapd_printf("Proxy noack WAR status: %d\n", dev->proxy_noack_war);
    }
#endif
    extended_cmd.cmd = EXTENDED_SUBIOCTL_GET_MPSTA_MAC_ADDR;
    extended_cmd.data_len = sizeof(extended_cmd.ext_data.qwrap_config);
    os_memcpy(extended_cmd.ext_data.qwrap_config.addr, dev->mpsta_mac_addr, 6);
    ret = driver->send_command(dev->wifi_ifname,(void *) &extended_cmd,
        sizeof(extended_cmd), QCA_NL80211_VENDOR_SUBCMD_EXTENDEDSTATS,
        SIOCGATHEXTENDED, &(wptr->sock));
    os_memcpy(dev->mpsta_mac_addr, extended_cmd.ext_data.qwrap_config.addr, 6);
    if (ret != 0) {
        wrapd_printf("ret=%d ioctlSIOCGATHEXTENDED get_mpsta macaddr err", ret);
        goto error2;
    } else {
        os_memcpy(dev->mpsta_mac_addr, extended_cmd.ext_data.qwrap_config.addr, 6);
        wrapd_printf("mpsta_mac_addr:%x:%x:%x:%x:%x:%x",
            dev->mpsta_mac_addr[0], dev->mpsta_mac_addr[1],
            dev->mpsta_mac_addr[2], dev->mpsta_mac_addr[3],
            dev->mpsta_mac_addr[4], dev->mpsta_mac_addr[5]);
    }

    path_len = strlen(wrapd_ctrl_iface_path) + strlen(dev->wifi_ifname) + 2;
	wrapd_path = os_malloc(path_len);
	if (wrapd_path == NULL)
    {
        wrapd_printf("ERROR:malloc failed\n");
		goto error2;
    }
    snprintf(wrapd_path, path_len, "%s-%s", wrapd_ctrl_iface_path, dev->wifi_ifname);

    dev->wrapd = wrapd_ctrl_open(wrapd_path, dev);
    if(dev->wrapd)
        eloop_register_read_sock(dev->wrapd->sock, wrapd_ctrl_iface_receive, (void *)dev, NULL);
    else
        goto error3;

    cfg = os_zalloc(sizeof(*cfg));
    if (cfg == NULL) {
        goto error4;
    }

    cfg->ctx = dev;
    cfg->newlink_cb = wrapd_event_rtm_newlink;

    dev->netlink = netlink_init(cfg);
    if (dev->netlink == NULL) {
        goto error5;
    }
    return dev;

error5:
    os_free(cfg);
error4:
    close(dev->wrapd->sock);
error3:
    os_free(wrapd_path);
error2:
    if(dev->vma_conf_file)
        os_free(dev->vma_conf_file);
error1:
    os_free(dev->wifi_ifname);
error:
    os_free(dev);
    return NULL;
}


void remove_wifi_iface(struct wrapd_global *wptr, struct wrap_wifi_iface *dev) {

    struct wrap_wifi_iface *prev;
    /* Remove interface from the global list of interfaces */
    prev = wptr->wrap_wifi_list;
    if (prev == dev) {
        wptr->wrap_wifi_list = dev->next;
    } else {
        while (prev && prev->next != dev)
            prev = prev->next;
        if (prev == NULL)
            return;
        prev->next = dev->next;
    }

}

int wrapd_add_wifi(struct wrapd_global *wptr, char *cmd)
{
    int status;
    struct wifi_cmd_args args;
    struct wrap_wifi_iface *dev;


    status = process_wifi_add_cmd(cmd, &args);
    if(status)
    {
        wrapd_printf("parsing command failed\n");
        return -1;
    }

    wrapd_printf("Add wifi command invoked!! - (%s)\n", args.wifi_ifname);

    if(wrapd_get_wifi_iface(wptr, args.wifi_ifname))
    {
        wrapd_printf("Interface (%s) already exits!\n", args.wifi_ifname);
        return -1;
    }

    dev = wrapd_alloc_wifi_iface(wptr, &args);
    if(!dev)
    {
        wrapd_printf("(%s) - wrapd ap iface linking failed\n", args.wifi_ifname);
        return -1;
    }

    if(dev->vma_conf_file)
        wrapd_load_vma_list(dev->vma_conf_file, (void*)dev);

    ++wrapg->wifi_count;
    return 0;
}

int wrapd_remove_wifi(struct wrapd_global *wptr, char *cmd)
{
    int status;
    struct wifi_cmd_args args;
    struct wrap_wifi_iface *dev;

    wrapd_printf("Remove wifi command invoked!!");

    status = process_wifi_remove_cmd(cmd, &args);
    if(status)
    {
        wrapd_printf("parsing command failed\n");
        return -1;
    }

    dev = wrapd_get_wifi_iface(wptr, args.wifi_ifname);
    if(!dev)
    {
        wrapd_printf("Interface (%s) not present!\n", args.wifi_ifname);
        return -1;
    }

    remove_wifi_iface(wptr, dev);

    --wrapg->wifi_count;
    return 0;
}
struct ctrl_conn* wrapd_alloc_ap_iface(struct wrap_wifi_iface *aptr, struct cmd_args *args)
{
    struct ctrl_conn *hapd;
    int status;
    struct ifreq ifr;
    struct utsname u;
    memset(&u, 0, sizeof(struct utsname));
    uname(&u);

    hapd = os_zalloc(sizeof(struct ctrl_conn));
    if(hapd == NULL) {
        wrapd_printf("ERROR: hapd alloc failed\n");
        return NULL;
    }
    os_strlcpy(hapd->ifname, args->ifname, IFNAMSIZ);
    hapd->aptr = aptr;
    hapd->next = aptr->hapd_list;
    hapd->secmode = args->secmode;
    hapd->parent_wifi = aptr;

    if (args->cmdmode == RUNTIME) {
    	wrapd_remove_to_global(aptr, args->ifname);
    	/*Adding delays for x86 emulation*/
	 if(!os_strcmp(u.machine, "x86_64"))
	 {
	    os_sleep(5, 0);
         }

        if(args->secmode == SEC) {
	    status = wrapd_ap_add_to_global(aptr, args);
            if(status)
                goto out;

            status = wrapd_conn_to_hostapd(hapd);
            if(status)
                goto out;

	}
        if(!os_strcmp(u.machine, "x86_64"))
        {
            os_sleep(5, 0);
        }
        aptr->hapd_list = hapd;
	disconnect_all_stas(hapd);
    } else {
	if(args->secmode == SEC) {
		status = wrapd_conn_to_hostapd(hapd);
		if(status)
			goto out;
	}
        if(!os_strcmp(u.machine, "x86_64"))
       	{
            os_sleep(5, 0);
       	}
        aptr->hapd_list = hapd;
	disconnect_all_stas(hapd);
    }

    if(strlcpy(ifr.ifr_name, hapd->ifname, IFNAMSIZ) >= IFNAMSIZ) {
        wrapd_printf("%s:Interface Name too long.%s\n",__func__,hapd->ifname);
        goto out;
    }

    /* Get the MAC address */
    if(ioctl(wrapg->ioctl_sock, SIOCGIFHWADDR, &ifr) < 0)
    {
        wrapd_printf("SIOCGIFHWADDR - Unable to fetch hw address\n");
        goto out;
    }
    os_memcpy(hapd->ap_mac_addr, ifr.ifr_hwaddr.sa_data, IEEE80211_ADDR_LEN);

    return hapd;

out:
   remove_ap_iface(aptr, hapd);
   return NULL;
}


int wrapd_add_ap(struct wrapd_global *wptr, char *cmd)
{
    int status;
    struct cmd_args args;
    struct ctrl_conn *hapd;
    struct wrap_wifi_iface *aptr = NULL;


    status = process_add_cmd(cmd, &args);
    if(status)
    {
        wrapd_printf("parsing command failed\n");
        return -1;
    }

    aptr = wrapd_get_wifi_iface(wptr, args.wifiname);
    if(!aptr)
    {
        wrapd_printf("Interface (%s) not present!\n", args.wifiname);
        return -1;
    }

    wrapd_printf("Add ap command invoked!! - (%s)\n", args.ifname);

    if(wrapd_get_ap_iface(aptr, args.ifname))
    {
        wrapd_printf("Interface (%s) already exits!\n", args.ifname);
        return -1;
    }

    hapd = wrapd_alloc_ap_iface(aptr, &args);
    if(!hapd)
    {
        wrapd_printf("(%s) - wrapd ap iface linking failed\n", args.ifname);
        return -1;
    }

    wrapd_printf("(%s) created in secmode = (%s)\n", args.ifname, ((hapd->secmode) ? "SEC":"OPEN"));
    return 0;
}

int wrapd_add_sta(struct wrapd_global *wptr, char *cmd)
{
    int status;
    struct cmd_args args;
    struct ctrl_conn *wpas;
    struct wrap_wifi_iface *aptr = NULL;
    char add_cmd[WRAP_MAX_CMD_LEN] = {0};
    int res;
    char *driver_type;
    struct extended_ioctl_wrapper extended_cmd;
    u_int8_t sta_vap_up = 0;
    char msg[256];
    int ret = 0;

    status = process_add_cmd(cmd, &args);
    if(status)
    {
        wrapd_printf("parsing command failed\n");
        return -1;
    }

    aptr = wrapd_get_wifi_iface(wptr, args.wifiname);
    if(!aptr)
    {
        wrapd_printf("Interface (%s) not present!\n", args.wifiname);
        return -1;
    }

    wrapd_printf("Add sta command invoked!! - (%s)\n", args.ifname);

    if(wrapd_get_sta_iface(aptr, args.ifname))
    {
        wrapd_printf("Interface (%s) already exits!\n", args.ifname);
        return -1;
    }

    aptr->wpa_conf_file = os_strdup(args.confname);
    if (aptr->wpa_conf_file == NULL)
    {
        wrapd_printf("malloc failed for wpa_conf_file\n");
        return -1;
    }

    wpas = os_zalloc(sizeof(struct ctrl_conn));
    if(wpas == NULL) {
        wrapd_printf("ERROR: wpas alloc failed\n");
        os_free(aptr->wpa_conf_file);
        return -1;
    }
    os_strlcpy(wpas->ifname, args.ifname, IFNAMSIZ);
    wpas->aptr = aptr;
    wpas->next = NULL;
    aptr->wpa_s = wpas;
    driver_type = driver->get_driver_type();

    if (args.cmdmode == RUNTIME) {
        //add wpa_supplicant iface
        res = os_snprintf(add_cmd, sizeof(add_cmd),"%s\t%s\t%s\t%s\t%s\t%s",
                        wpas->ifname,
                        aptr->wpa_conf_file,
                        driver_type,
                        "",
                        "",
                        wrapg->brname);
        if (res < 0 || res >= sizeof(add_cmd)){
            wrapd_printf("Fail to build wpa_s msg");
            goto out;
        }
        add_cmd[sizeof(add_cmd) - 1] = '\0';

        /* Remove iface from global wpa supp list if it is already added */
        wrapd_psta_if_remove(aptr, wpas->ifname);

        printf("cmd to global supp:add_cmd = %s\n", add_cmd);

        status = wrapd_psta_if_add(aptr, NULL, add_cmd, wpas->ifname);
        if(status)
        {
            wrapd_printf("MPSTA (%s) add add_cmd to global wpa supp failed\n", wpas->ifname);
            goto out;
       }
   }


    status = wrapd_conn_to_mpsta_wpa_s(wpas);
    if(status)
    {
        wrapd_printf("MPSTA (%s) conn failed\n", wpas->ifname);
        wrapd_psta_if_remove(aptr, wpas->ifname);
        goto out;
    }

    /*
     * Between issual of INTERFACE_ADD cmd to global wpa supplicant and socket creation in wrapd,
     * connection event from MPSTA ctrl iface might be missed, so get MPSTA VAP connection stats from driver
     * wrapd will never miss MPSTA connection event
     */
    if (aptr->mpsta_conn == 0) {
    memset(&extended_cmd.ext_data.iface_config, 0, sizeof(struct iface_config_t));
    extended_cmd.cmd = EXTENDED_SUBIOCTL_GET_STAVAP_CONNECTION;
    extended_cmd.data_len = sizeof(extended_cmd.ext_data.iface_config);
    extended_cmd.ext_data.iface_config.stavap_up = sta_vap_up;
    ret = driver->send_command(aptr->wifi_ifname, (void *)&extended_cmd,
        sizeof(extended_cmd), QCA_NL80211_VENDOR_SUBCMD_EXTENDEDSTATS,
        SIOCGATHEXTENDED, &(aptr->wptr->sock));
    if (ret != 0) {
        wrapd_printf("ret=%d ioctl SIOCGATHEXTENDED get_stavap_conn err", ret);
    } else {
        sta_vap_up = extended_cmd.ext_data.iface_config.stavap_up;
        wrapd_printf("Initial stavap connection: %d \n",sta_vap_up);
    }
	if (sta_vap_up == 1) {
	    os_memset(&msg, 0, sizeof(msg));
	    wrapd_wpa_s_ctrl_iface_process(aptr, msg, sta_vap_up);
	}
    }
    update_wired_client_parent(aptr->wifi_ifname);

    if(wrapg->enable_proxysta_addition && !wrapg->auto_wired_started)
    {
        start_wired_client_addition();
        wrapg->auto_wired_started = 1;
    }

    return 0;
out:
    aptr->wpa_s = NULL;
    os_free(wpas);
    os_free(aptr->wpa_conf_file);
    return -1;
}

int wrapd_remove_ap(struct wrapd_global *wptr, char *cmd)
{
    int status;
    struct cmd_args args;
    struct ctrl_conn *hapd;
    struct wrap_wifi_iface *aptr = NULL;

    status = process_remove_cmd(cmd, &args);
    if(status)
    {
        wrapd_printf("parsing command failed\n");
        return -1;
    }

    aptr = wrapd_get_wifi_iface(wptr, args.wifiname);
    if(!aptr)
    {
        wrapd_printf("Interface (%s) not present!\n", args.wifiname);
        return -1;
    }

    hapd = wrapd_get_ap_iface(aptr, args.ifname);
    if(!hapd)
    {
        wrapd_printf("Interface (%s) does not exist\n", args.ifname);
        return -1;
    }

    hapd->remove_from_user = 1;

    if(hapd->secmode == SEC)
    {
        wrapd_remove_to_global(aptr, hapd->ifname);
    }
    else {
        ifconfig_down(hapd->ifname);
    }
    return 0;
}

int wrapd_remove_sta(struct wrapd_global *wptr, char *cmd)
{
    int status;
    struct cmd_args args;
    struct ctrl_conn *wpas;
    struct wrap_wifi_iface *aptr = NULL;

    status = process_remove_cmd(cmd, &args);
    if(status)
    {
        wrapd_printf("parsing command failed\n");
        return -1;
    }

    aptr = wrapd_get_wifi_iface(wptr, args.wifiname);
    if(!aptr)
    {
        wrapd_printf("Interface (%s) not present!\n", args.wifiname);
        return -1;
    }

    wpas = wrapd_get_sta_iface(aptr, args.ifname);
    if(!wpas)
    {
        wrapd_printf("Interface (%s) does not exist\n", args.ifname);
        return -1;
    }

    /* Stop listening MPSTA iface */
    eloop_unregister_read_sock(wpas->ctrl_iface->sock);
    wpa_ctrl_close((struct wpa_ctrl *)wpas->ctrl_iface);
    aptr->mpsta_conn = 0;
    wrapd_disconn_all(aptr);

    /* Remove MPSTA iface via global wpa_supplicant ctrl interface */
    wrapd_psta_if_remove(aptr, wpas->ifname);

    os_free(wpas);
    aptr->wpa_s = NULL;
    os_free(aptr->wpa_conf_file);

    return 0;
}
char *
wrapd_ctrl_iface_process(struct wrap_wifi_iface *aptr, char *buf, size_t *resp_len)
{
	char *reply;
	const int reply_size = 2048;
	int reply_len, addr_off;
    u_int32_t flags = 0;
    struct wrap_wifi_iface *dev;

	reply = os_malloc(reply_size);
	if (reply == NULL) {
		*resp_len = 1;
		return NULL;
	}

	os_memcpy(reply, "OK\n", 3);
	reply_len = 3;

	if (os_strcmp(buf, "PING") == 0) {
		os_memcpy(reply, "PONG\n", 5);
		reply_len = 5;

	} else if (os_strncmp(buf, "ETH_PSTA_ADD ", 13) == 0) {
        if (os_strncmp(buf + 13, "MAT ", 4) == 0) {
            flags |= (WRAPD_PSTA_FLAG_MAT | WRAPD_PSTA_FLAG_WIRED);
            addr_off = 13 + 4;

        } else {
            flags |= WRAPD_PSTA_FLAG_WIRED;
            addr_off = 13;
        }
        if (char2addr(buf + addr_off) != 0) {
            wrapd_printf("Invalid MAC addr");
            return NULL;
        }
	if(wrapg->wrapd_psta_config == 2)
	{
	    wrapd_psta_add(wrapg->max_aptr, wrapg->max_aptr->wifi_ifname, NULL, (u8 *)(buf + addr_off), flags);
	}
	else if (wrapg->wrapd_psta_config == 1)
	{
            for(dev = wrapg->wrap_wifi_list; dev; dev=dev->next)
            {
                wrapd_psta_add(dev, dev->wifi_ifname, NULL, (u8 *)(buf + addr_off), flags);
            }
        }
        else
        {
            /* Find primary radio and send add command to the same */
	    	wrapd_psta_add(aptr, aptr->wifi_ifname, NULL, (u8 *)(buf + addr_off), flags);
        }

	} else if (os_strncmp(buf, "ETH_PSTA_REMOVE ", 16) == 0) {
        if (char2addr(buf + 16) != 0) {
            wrapd_printf("Invalid MAC addr");
            return NULL;
        }
        if(wrapg->wrapd_psta_config == 2)
        {
	    for(dev = wrapg->wrap_wifi_list; dev; dev=dev->next)
	    {
		wrapd_psta_remove(dev, (u8 *)(buf + 16), 1, NULL);
	    }
        }
        else if (wrapg->wrapd_psta_config == 1)
        {
            for(dev = wrapg->wrap_wifi_list; dev; dev=dev->next)
            {
		        wrapd_psta_remove(dev, (u8 *)(buf + 16), 1, NULL);
            }
        }
        else
        {
		    wrapd_psta_remove(aptr, (u8 *)(buf + 16), 1, NULL);
        }

	} else if (os_strcmp(buf, "PSTA_LIST") == 0) {
		wrapd_sta_list(aptr);
    } else {
		os_memcpy(reply, "UNKNOWN COMMAND\n", 16);
		reply_len = 16;
	}

	if (reply_len < 0) {
		os_memcpy(reply, "FAIL\n", 5);
		reply_len = 5;
	}

	*resp_len = reply_len;
	return reply;
}

void
wrapd_ctrl_iface_receive(int sock, void *eloop_ctx, void *sock_ctx)
{
    struct wrap_wifi_iface *aptr = (struct wrap_wifi_iface *)eloop_ctx;
    char buf[256];
    int res;
    struct sockaddr_un from;
    socklen_t fromlen = sizeof(from);
    char *reply;
    size_t reply_len;
    res = recvfrom(sock, buf, sizeof(buf) - 1, 0, (struct sockaddr *) &from, &fromlen);
    if (res < 0) {
        wrapd_printf("recvfrom err");
        return;
    }
    buf[res] = '\0';

    reply = wrapd_ctrl_iface_process(aptr, buf, &reply_len);

    if (reply) {
        sendto(sock, reply, reply_len, 0, (struct sockaddr *) &from, fromlen);
        os_free(reply);
    } else if (reply_len) {
        sendto(sock, "FAIL\n", 5, 0, (struct sockaddr *) &from, fromlen);
    }
}

char *
wrapd_global_ctrl_iface_process(struct wrapd_global *wptr, char *buf, size_t *resp_len)
{
	char *reply;
	const int reply_size = 2048;
	int reply_len, addr_off;
    u_int32_t flags = 0;
    struct wrap_wifi_iface *aptr = NULL;
	reply = os_malloc(reply_size);
	if (reply == NULL) {
		*resp_len = 1;
		return NULL;
	}

	os_memcpy(reply, "OK\n", 3);
	reply_len = 3;

	if (os_strcmp(buf, "PING") == 0) {
		os_memcpy(reply, "PONG\n", 5);
		reply_len = 5;
	} else if (os_strncmp(buf, "ETH_PSTA_ADD ", 13) == 0) {
        if (os_strncmp(buf + 13, "MAT ", 4) == 0) {
            flags |= (WRAPD_PSTA_FLAG_MAT | WRAPD_PSTA_FLAG_WIRED);
            addr_off = 13 + 4;

        } else {
            flags |= WRAPD_PSTA_FLAG_WIRED;
            addr_off = 13;
        }
        if (char2addr(buf + addr_off) != 0) {
            wrapd_printf("Invalid MAC addr");
            return NULL;
        }
        if(wrapg->wrapd_psta_config == 2)
        {
            wrapd_psta_add(wrapg->max_aptr, wrapg->max_aptr->wifi_ifname, NULL, (u8 *)(buf + addr_off), flags);
        }
        else if (wrapg->wrapd_psta_config == 1)
        {
            for(aptr = wrapg->wrap_wifi_list; aptr; aptr=aptr->next)
            {
                wrapd_psta_add(aptr, aptr->wifi_ifname, NULL, (u8 *)(buf + addr_off), flags);
            }
        }
        else
        {
            /* Find primary radio and send add command to the same */
            aptr = wrapd_get_wifi_iface(wptr, wptr->ethernet_extender);
            if(!aptr)
            {
                wrapd_printf("Interface (%s) not present!\n", wptr->ethernet_extender);
                return NULL;
            }
	    	wrapd_psta_add(aptr, aptr->wifi_ifname, NULL, (u8 *)(buf + addr_off), flags);
        }

	} else if (os_strncmp(buf, "ETH_PSTA_REMOVE ", 16) == 0) {
        if (char2addr(buf + 16) != 0) {
            wrapd_printf("Invalid MAC addr");
            return NULL;
        }
        if(wrapg->wrapd_psta_config == 2)
	{
	    for(aptr = wrapg->wrap_wifi_list; aptr; aptr=aptr->next)
	    {
		wrapd_psta_remove(aptr, (u8 *)(buf + 16), 1, NULL);
	    }
	}
        else if (wrapg->wrapd_psta_config == 1)
        {
            for(aptr = wrapg->wrap_wifi_list; aptr; aptr=aptr->next)
            {
		        wrapd_psta_remove(aptr, (u8 *)(buf + 16), 1, NULL);
            }
        }
        else
        {
            aptr = wrapd_get_wifi_iface(wptr, wptr->ethernet_extender);
            if(!aptr)
            {
                wrapd_printf("Interface (%s) not present!\n", wptr->ethernet_extender);
                return NULL;
            }
		    wrapd_psta_remove(aptr, (u8 *)(buf + 16), 1, NULL);
        }

	} else if(os_strncmp(buf, "ap_add\t", 7) == 0) {
        wrapd_add_ap(wptr, buf + 7);
	} else if(os_strncmp(buf, "ap_remove\t", 10) == 0) {
        wrapd_remove_ap(wptr, buf + 10);
	} else if(os_strncmp(buf, "sta_add\t", 8) == 0) {
        wrapd_add_sta(wptr, buf + 8);
	} else if(os_strncmp(buf, "sta_remove\t", 11) == 0) {
        wrapd_remove_sta(wptr, buf + 11);
	} else if(os_strncmp(buf, "wifi_add\t", 9) == 0) {
        wrapd_add_wifi(wptr, buf + 9);
	} else if(os_strncmp(buf, "wifi_remove\t", 12) == 0) {
        wrapd_remove_wifi(wptr, buf + 12);
	} else if (os_strcmp(buf, "PSTA_LIST") == 0) {
		wrapd_sta_list_all(wptr);
    } else {
		os_memcpy(reply, "UNKNOWN COMMAND\n", 16);
		reply_len = 16;
	}

	if (reply_len < 0) {
		os_memcpy(reply, "FAIL\n", 5);
		reply_len = 5;
	}

	*resp_len = reply_len;
	return reply;
}

void
wrapd_global_ctrl_iface_receive(int sock, void *eloop_ctx, void *sock_ctx)
{
    struct wrapd_global *wptr = (struct wrapd_global *)eloop_ctx;
    char buf[256];
    int res;
    struct sockaddr_un from;
    socklen_t fromlen = sizeof(from);
    char *reply;
    size_t reply_len;
    res = recvfrom(sock, buf, sizeof(buf) - 1, 0, (struct sockaddr *) &from, &fromlen);
    if (res < 0) {
        wrapd_printf("recvfrom err");
        return;
    }
    buf[res] = '\0';

    reply = wrapd_global_ctrl_iface_process(wptr, buf, &reply_len);

    if (reply) {
        sendto(sock, reply, reply_len, 0, (struct sockaddr *) &from, fromlen);
        os_free(reply);
    } else if (reply_len) {
        sendto(sock, "FAIL\n", 5, 0, (struct sockaddr *) &from, fromlen);
    }
}
void
wrapd_hostapd_ctrl_iface_process(struct ctrl_conn *hapd, char *msg)
{
	int addr_off;
    u_int32_t flags = 0;
    struct wrap_wifi_iface *dev;

    if(!hapd)
        return;

	if (os_strncmp(msg + HOSTAPD_MSG_ADDR_OFF, "AP-STA-CONNECTED ", 17) == 0) {
        flags |= WRAPD_PSTA_FLAG_MAT ;
        addr_off = HOSTAPD_MSG_ADDR_OFF + 17;

        if (char2addr(msg + addr_off) != 0) {
            wrapd_printf("Invalid MAC addr");
            return;
        }

        if(wrapg->wrapd_psta_config == 2)
        {
	    wrapd_psta_add(wrapg->max_aptr, wrapg->max_aptr->wifi_ifname, hapd->ifname, (u8 *)(msg + addr_off), flags);
        }
        else if (wrapg->wrapd_psta_config == 1)
        {
            for(dev = wrapg->wrap_wifi_list; dev; dev=dev->next)
            {
                wrapd_psta_add(dev, dev->wifi_ifname, hapd->ifname, (u8 *)(msg + addr_off), flags);
            }
        }
        else
        {
	        wrapd_psta_add(hapd->aptr, hapd->aptr->wifi_ifname, hapd->ifname, (u8 *)(msg + addr_off), flags);
        }

	} else if (os_strncmp(msg + HOSTAPD_MSG_ADDR_OFF, "AP-STA-DISCONNECTED ", 20) == 0) {
	    addr_off = HOSTAPD_MSG_ADDR_OFF + 20;
        if (char2addr(msg + addr_off) != 0) {
            wrapd_printf("Invalid MAC addr");
            return;
        }
        if(wrapg->wrapd_psta_config == 2)
        {
            for(dev = wrapg->wrap_wifi_list; dev; dev=dev->next)
            {
		        wrapd_psta_remove(dev, (u8 *)(msg + addr_off), 1, hapd->ifname);
            }
        }
        else if (wrapg->wrapd_psta_config == 1)
        {
            for(dev = wrapg->wrap_wifi_list; dev; dev=dev->next)
            {
		        wrapd_psta_remove(dev, (u8 *)(msg + addr_off), 1, hapd->ifname);
            }
        }
        else
        {
		    wrapd_psta_remove(hapd->aptr, (u8 *)(msg + addr_off), 1, hapd->ifname);
        }

	} else if (os_strncmp(msg + HOSTAPD_MSG_ADDR_OFF, "AP-DISABLED ", 12) == 0) {
        if(hapd->remove_from_user) {
            wrapd_printf("AP-DISABLED %s\n", hapd->ifname);
            eloop_unregister_read_sock(hapd->ctrl_iface->sock);
            wpa_ctrl_close((struct wpa_ctrl *)hapd->ctrl_iface);
            remove_ap_iface(hapd->aptr, hapd);
        }
    } else {
		//wrapd_printf("Unknow msg(%s)", msg);
	}

}

void
wrapd_hostapd_ctrl_iface_receive(int sock, void *eloop_ctx, void *sock_ctx)
{
    struct ctrl_conn *vap = (struct ctrl_conn*)eloop_ctx;
    char msg[256];
    int res;
    struct sockaddr_un from;
    socklen_t fromlen = sizeof(from);

    res = recvfrom(sock, msg, sizeof(msg) - 1, 0, (struct sockaddr *) &from, &fromlen);
    if (res < 0) {
        wrapd_printf("recvfrom err");
        return;
    }
    msg[res] = '\0';

    wrapd_hostapd_ctrl_iface_process(vap, msg);
}

static void disconnect_all_stas(struct ctrl_conn *hapd)
{
    if(hapd->secmode == SEC)
        wrapd_ap_disassociate_stas((struct wpa_ctrl*)hapd->ctrl_iface);
    else
    {
        wrapd_ap_kick_all_stas(hapd);

    }
}
static void wrapd_mpsta_connect(struct wrap_wifi_iface *parent)
{
    u_int32_t flags;
    u8 addr[IEEE80211_ADDR_LEN];
    psta_status_t retv;
    int i;
    struct extended_ioctl_wrapper extended_cmd;
    int ret;
    int force_client_mcast = 0;
    struct wrap_wifi_iface *other;
    int is_max_priority_radio = 0;
    struct ctrl_conn *conn, *next;

    memset(&extended_cmd.ext_data.qwrap_config, 0, sizeof(struct qwrap_config_t));
    extended_cmd.cmd = EXTENDED_SUBIOCTL_GET_FORCE_CLIENT_MCAST;
    extended_cmd.data_len = sizeof(extended_cmd.ext_data.qwrap_config);
    extended_cmd.ext_data.qwrap_config.force_client_mcast = force_client_mcast;
    ret = driver->send_command(parent->wifi_ifname, (void *)&extended_cmd,
        sizeof(extended_cmd), QCA_NL80211_VENDOR_SUBCMD_EXTENDEDSTATS,
        SIOCGATHEXTENDED, &(parent->wptr->sock));
    if (ret != 0) {
        wrapd_printf("ret=%d ioctl SIOCGATHEXTENDED get_force_client_mcast err", ret);
    } else {
        force_client_mcast = extended_cmd.ext_data.qwrap_config.force_client_mcast;
        wrapd_printf("force_client_mcast: %d \n", force_client_mcast);
    }

    is_max_priority_radio = ((os_strcmp(parent->wifi_ifname, wrapg->max_aptr->wifi_ifname) == 0) ? 1 : 0);

    if( is_max_priority_radio && ((os_strcmp(wrapg->max_aptr->wifi_ifname, wrapg->ethernet_extender)) != 0))
    {
        update_wired_client_parent(wrapg->max_aptr->wifi_ifname);
    }

    for(other= wrapg->wrap_wifi_list; other ; other=other->next)
    {
        if (other != parent)
        {
            i = 0;
            while (i < WRAP_MAX_PSTA_NUM) {
                if(other->psta[i].added &&
                        (IS_PSTA_MY_CHILD(parent, other->psta[i]) ||
                        (is_max_priority_radio && IS_PSTA_MOVABLE(other, psta))))
                {
                    flags = other->psta[i].flags;
                    os_memcpy(addr, other->psta[i].oma, IEEE80211_ADDR_LEN);

                    if(force_client_mcast)
                    {
                        wrapd_psta_remove(other, addr, 1, other->psta[i].child);
                    }
                    else
                    {
                        wrapd_printf("Creating PSTA on %s flags:%x\n", parent->wifi_ifname,flags);
                        retv = wrapd_psta_add(parent, parent->wifi_ifname, other->psta[i].child, addr, flags);

                        if (retv == PROXY_STA_SUCCESS)
                        {
                            wrapd_printf("Removing PSTA on %s \n", other->wifi_ifname);
                            wrapd_psta_remove(other, addr, 0, other->psta[i].child);
                     }
                        else if (retv == PROXY_STA_LIMIT_EXCEEDED)
                        {
                            wrapd_printf("PSTA vap limit exceeded, Failsafe move to %s failed\n", parent->wifi_ifname);
                            /*
                             * Remove clients that cannot be shifted to parent
                             * - no priority for clients based on order of connection
                             * - psta's are added based on availablity of space at that instant
                             */
                            wrapd_psta_remove(other, addr, 1, other->psta[i].child);
                            break;
                        }
                    }
                }
                i ++;
            }

            conn = other->hapd_list;
            while(conn)
            {
                next = conn->next;
                if(IS_AP_MY_CHILD(parent, conn) || (is_max_priority_radio && IS_AP_FOSTERED(conn)))
                {
                    if(force_client_mcast)
                        disconnect_all_stas(conn);

                    delink_ap_iface(other, conn);
                    link_ap_iface(parent, conn);
                }
                conn = next;
            }
        }
    }
}

static void wrapd_mpsta_disconnect(struct wrap_wifi_iface *parent)
{
    u_int32_t flags;
    u8 addr[IEEE80211_ADDR_LEN];
    psta_status_t retv;
    int i;
    struct extended_ioctl_wrapper extended_cmd;
    int force_client_mcast = 0, ret;
    struct ctrl_conn *hapd;

    if((os_strcmp(wrapg->max_aptr->wifi_ifname, wrapg->ethernet_extender)) != 0)
    {
        update_wired_client_parent(wrapg->max_aptr->wifi_ifname);
    }

    memset(&extended_cmd.ext_data.qwrap_config, 0, sizeof(struct qwrap_config_t));
    extended_cmd.cmd = EXTENDED_SUBIOCTL_GET_FORCE_CLIENT_MCAST;
    extended_cmd.data_len = sizeof(extended_cmd.ext_data.qwrap_config);
    extended_cmd.ext_data.qwrap_config.force_client_mcast = force_client_mcast;
    ret = driver->send_command(parent->wifi_ifname, (void *)&extended_cmd,
        sizeof(extended_cmd), QCA_NL80211_VENDOR_SUBCMD_EXTENDEDSTATS,
        SIOCGATHEXTENDED, &(parent->wptr->sock));
    if (ret != 0) {
        wrapd_printf("ret=%d ioctl SIOCGATHEXTENDED get_force_client_mcast err", ret);
    } else {
        force_client_mcast = extended_cmd.ext_data.qwrap_config.force_client_mcast;
        wrapd_printf("force_client_mcast: %d \n", force_client_mcast);
    }

    if (force_client_mcast) {

        i = 0;
        while (i < WRAP_MAX_PSTA_NUM) {
            if(parent->psta[i].added) {
                os_memcpy(addr, parent->psta[i].oma, IEEE80211_ADDR_LEN);
                wrapd_psta_remove(parent, addr, 1, parent->psta[i].child);
            }
            i ++;
        }

        for(hapd = parent->hapd_list ; hapd ; hapd = hapd->next)
            disconnect_all_stas(hapd);

        move_ap_ifaces(parent, wrapg->max_aptr);
    }
    else
    {
        i = 0;

        move_ap_ifaces(parent, wrapg->max_aptr);

        while (i < WRAP_MAX_PSTA_NUM) {
            if((parent->psta[i].added)) {
                flags = parent->psta[i].flags;
                os_memcpy(addr, parent->psta[i].oma, IEEE80211_ADDR_LEN);

                wrapd_printf("Creating PSTA on %s flags:%x\n", wrapg->max_aptr->wifi_ifname,flags);
                retv = wrapd_psta_add(wrapg->max_aptr, wrapg->max_aptr->wifi_ifname, parent->psta[i].child, addr, flags);

                if (retv == PROXY_STA_SUCCESS)
                {
                    wrapd_printf("Removing PSTA on %s \n", parent->wifi_ifname);
                    wrapd_psta_remove(parent, addr, 0, parent->psta[i].child);
                }
                else if (retv == PROXY_STA_LIMIT_EXCEEDED)
                {
                    wrapd_printf("PSTA vap limit exceeded, Failsafe move to %s failed\n", wrapg->max_aptr->wifi_ifname);
                    wrapd_psta_remove(parent, addr, 1, parent->psta[i].child);
                    break;
                }
            }
            i ++;
        }
    }

}

static void wrapd_max_aptr_mpsta_disconnect(struct wrap_wifi_iface *parent)
{
    u_int32_t flags;
    u8 addr[IEEE80211_ADDR_LEN];
    psta_status_t retv;
    int i = 0;
    struct extended_ioctl_wrapper extended_cmd;
    int force_client_mcast = 0, ret;

    if((os_strcmp(wrapg->max_aptr->wifi_ifname, wrapg->ethernet_extender)) != 0)
    {
        update_wired_client_parent(wrapg->max_aptr->wifi_ifname);
    }

    memset(&extended_cmd.ext_data.qwrap_config, 0, sizeof(struct qwrap_config_t));
    extended_cmd.cmd = EXTENDED_SUBIOCTL_GET_FORCE_CLIENT_MCAST;
    extended_cmd.data_len = sizeof(extended_cmd.ext_data.qwrap_config);
    extended_cmd.ext_data.qwrap_config.force_client_mcast = force_client_mcast;
    ret = driver->send_command(parent->wifi_ifname, (void *)&extended_cmd,
        sizeof(extended_cmd), QCA_NL80211_VENDOR_SUBCMD_EXTENDEDSTATS,
        SIOCGATHEXTENDED, &(parent->wptr->sock));
    if (ret != 0) {
        wrapd_printf("ret=%d ioctl SIOCGATHEXTENDED get_force_client_mcast err", ret);
    } else {
        force_client_mcast = extended_cmd.ext_data.qwrap_config.force_client_mcast;
        wrapd_printf("force_client_mcast: %d \n", force_client_mcast);
    }

    while (i < WRAP_MAX_PSTA_NUM) {
	if((parent->psta[i].added)) {
	    flags = parent->psta[i].flags;
	    os_memcpy(addr, parent->psta[i].oma, IEEE80211_ADDR_LEN);
	    if (force_client_mcast && !(IS_PSTA_WIRED(parent->psta[i]))) {
                i ++;
		continue;
	    }
	    wrapd_printf("Creating PSTA on %s flags:%x\n", wrapg->max_aptr->wifi_ifname,flags);
	    retv = wrapd_psta_add(wrapg->max_aptr, wrapg->max_aptr->wifi_ifname, parent->psta[i].child, addr, flags);

	    if (retv == PROXY_STA_SUCCESS)
	    {
		wrapd_printf("Removing PSTA on %s \n", parent->wifi_ifname);
		wrapd_psta_remove(parent, addr, 0, parent->psta[i].child);
	    }
	    else if (retv == PROXY_STA_LIMIT_EXCEEDED)
	    {
		wrapd_printf("PSTA vap limit exceeded, Failsafe move to %s failed\n", wrapg->max_aptr->wifi_ifname);
		wrapd_psta_remove(parent, addr, 1, parent->psta[i].child);
		break;
	    }
	}
	i ++;
    }
    if(force_client_mcast) {
        memset(&extended_cmd.ext_data.qwrap_config, 0, sizeof(struct qwrap_config_t));
        extended_cmd.cmd = EXTENDED_SUBIOCTL_DISASSOC_CLIENTS;
        extended_cmd.data_len = sizeof(extended_cmd.ext_data.qwrap_config);
        ret = driver->send_command(parent->wifi_ifname, (void *)&extended_cmd,
             sizeof(extended_cmd), QCA_NL80211_VENDOR_SUBCMD_EXTENDEDSTATS,
             SIOCGATHEXTENDED, &(parent->wptr->sock));
        if (ret != 0) {
            wrapd_printf("ret=%d ioctl SIOCGATHEXTENDED disassoc clients err", ret);
        } else {
            wrapd_printf("DISASSOC_CLIENTS cmds passsed to driver\n");
        }
    }
}


void
wrapd_wpa_s_ctrl_iface_process(struct wrap_wifi_iface *aptr, char *msg, u_int8_t initial_stavap_status)
{
    struct wrap_wifi_iface *lat_max_aptr;

    if (os_strncmp(msg + WPA_S_MSG_ADDR_OFF, "CTRL-EVENT-DISCONNECTED ", 24) == 0) {
	aptr->mpsta_conn = 0;
	wrapd_printf("disconnected event recvd, updating bridge mac\n");
        lat_max_aptr = wrapg->max_aptr;
	wrapd_update_max_priority_radio(aptr);
	wrapd_update_bridge_mac(aptr);
	wrapd_disconn_all(aptr);
	if(wrapg->wrapd_psta_config == 0) {
	    wrapd_mpsta_disconnect(aptr);
	} else if (wrapg->wrapd_psta_config == 2) {
	    if (lat_max_aptr == aptr) {
		wrapd_max_aptr_mpsta_disconnect(aptr);
	    }
	} else {
            /* Do Nothing*/
	}
    } else if ((os_strncmp(msg + WPA_S_MSG_ADDR_OFF, "CTRL-EVENT-CONNECTED ", 21) == 0) || (initial_stavap_status == 1)) {
	aptr->mpsta_conn = 1;
	wrapd_printf("connected event recvd, updating bridge mac\n");
	wrapd_update_max_priority_radio(aptr);
	wrapd_update_bridge_mac(aptr);
	if(wrapg->wrapd_psta_config ==0) {
	    wrapd_mpsta_connect(aptr);
	} else if (wrapg->wrapd_psta_config == 2) {
	    if (wrapg->max_aptr == aptr) {
		wrapd_max_aptr_mpsta_changed(aptr);
	    }
	}
	if (0 == wrapg->do_timer) {
	    wrapd_conn_all(aptr);
	} else {
	    if (0 == wrapg->in_timer) {
		eloop_register_timeout(1, 0, wrapd_conn_timer, aptr, NULL);
		wrapg->in_timer = 1;
	    }
	}
    } else {
	//wrapd_printf("Unknow msg(%s)", msg);
    }

}

void
wrapd_wpa_s_ctrl_iface_receive(int sock, void *eloop_ctx, void *sock_ctx)
{
    struct wrap_wifi_iface *aptr = (struct wrap_wifi_iface *)eloop_ctx;
    char msg[256];
    int res;
    struct sockaddr_un from;
    u_int8_t initial_stavap_status = 0xff;
    socklen_t fromlen = sizeof(from);

    res = recvfrom(sock, msg, sizeof(msg) - 1, 0, (struct sockaddr *) &from, &fromlen);
    if (res < 0) {
        wrapd_printf("recvfrom err");
        return;
    }
    msg[res] = '\0';
    wrapd_wpa_s_ctrl_iface_process(aptr, msg, initial_stavap_status);
}

//addr format: xx:xx:xx:xx:xx:xx
void
wrapd_load_vma_list(const char *conf_file, wrapd_hdl_t *handle)
{
    struct wrap_wifi_iface *aptr = (struct wrap_wifi_iface *)handle;
	FILE *f;
	char buf[256], *pos, *start;
	int line = 0, off =0;
    char vma_conf_device[WIFINAMSIZ];
    int vma_conf_wifi_off = 0;

    vma_conf_wifi_off = os_strlen(conf_file) - 10;
    os_strlcpy(vma_conf_device, conf_file + vma_conf_wifi_off, WIFINAMSIZ);  //copy wifi interface alone from conf_file

    if (os_strcmp(vma_conf_device, aptr->wifi_ifname) != 0) {
       return;
    }

    wrapd_printf("oma conf file(%s)", conf_file);

    f = fopen(conf_file, "r");
    if (f == NULL) {
        wrapd_printf("Cant open oma conf file(%s)", conf_file);
        return;
    }

	while ((fgets(buf, sizeof(buf), f)) && (off < WRAP_MAX_PSTA_NUM)) {
        line ++;
		if (buf[0] == '#')
			continue;

        pos = buf;
		while (*pos != '\0') {
			if (*pos == '\n') {
				*pos = '\0';
				break;
			}
			pos ++;
		}

        pos = os_strchr(buf, ':');
        if ((pos == NULL) || (pos - buf < 2) || (os_strlen(pos) < 15)) {
            wrapd_printf("Invalid addr in line %d", line);
            continue;
        }

        start = pos - 2;
        start[17] = '\0';
        if((start[5] != ':') ||(start[8] != ':') ||(start[11] != ':')|| (start[14] != ':')){
            wrapd_printf("Invalid addr in line %d", line);
            continue;
        }

        if (char2addr(start) != 0) {
            wrapd_printf("Invalid addr in line %d", line);
            continue;
        }

        os_memcpy(aptr->psta[off].vma, start, IEEE80211_ADDR_LEN);
        aptr->psta[off].vma_loaded = 1;
        aptr->psta[off].added = 0;
        aptr->psta[off].connected = 0;

        wrapd_printf("Load VMA(%02x:%02x:%02x:%02x:%02x:%02x) to off(%d) in line %d",
            aptr->psta[off].vma[0], aptr->psta[off].vma[1], aptr->psta[off].vma[2],
            aptr->psta[off].vma[3], aptr->psta[off].vma[4], aptr->psta[off].vma[5],
            off, line);
        off ++;
    }

    fclose(f);
}

int
wifi_ifname_to_num(const char *wifi_ifname)
{
    char *pos = (char *)wifi_ifname;
    int dec=-1;
    while (*pos != '\0') {
        dec = *pos - 48; /* Decimal value of ASCII */
        /*
         * Works only for wifi0 to wifi9
         */
        if (dec < 10) {
            wrapd_printf("WRAPD started on RADIO:%d\n", dec);
            break;
        }
        pos ++;
    }
    return dec;
}


struct wrapd_global*
wrapd_init_global(const char *global_supp, const char *global_hostapd, const char *global_wrapd, char *brname, int timer)
{
    struct wrapd_global *wrapg;
    char *realname = (void *)global_supp;
    struct ifreq ifr;

    if (!realname)
        return NULL;

    wrapg = os_zalloc(sizeof(*wrapg));
    if (!wrapg)
        return NULL;

    wrapg->ioctl_sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (wrapg->ioctl_sock < 0) {
        wrapd_printf("socket[PF_INET,SOCK_DGRAM]");
        return NULL;
    }
    driver->init_context(&(wrapg->sock));

    wrapg->global_wpa_supp = wpa_ctrl_open(realname);
    if (!wrapg->global_wpa_supp) {
        close(wrapg->ioctl_sock);
        driver->destroy_context(&(wrapg->sock));
        os_free(wrapg);
        wrapd_printf("Fail to connect global_wpa_supp");
        return NULL;
    }

    realname = (void *)global_hostapd;

    if(!realname) {
        wpa_ctrl_close(wrapg->global_wpa_supp);
        close(wrapg->ioctl_sock);
        driver->destroy_context(&(wrapg->sock));
        os_free(wrapg);
        wrapd_printf("global_hostapd is NULL");
        return NULL;
    }

    wrapg->global_hostapd = wpa_ctrl_open(realname);
    if (!wrapg->global_hostapd) {
        wrapd_printf("Failed to connect global_hostapd");
    }

    wrapd_printf("Connected to global_hostapd");

    if (brname) {
        wrapg->brname = os_strdup(brname);
        if (wrapg->brname == NULL) {
            return NULL;
        }
    }

    if(strlcpy(ifr.ifr_name, "wifi0", IFNAMSIZ) >= IFNAMSIZ) {
        wrapd_printf("%s:Source too long\n",__func__);
        if (wrapg->global_hostapd)
            wpa_ctrl_close(wrapg->global_hostapd);
        wpa_ctrl_close(wrapg->global_wpa_supp);
        close(wrapg->ioctl_sock);
        os_free(wrapg);
        return NULL;
    }

    /* Get the MAC address */
    if(ioctl(wrapg->ioctl_sock, SIOCGIFHWADDR, &ifr) < 0)
    {
        wrapd_printf("SIOCGIFHWADDR - Unable to fetch hw address\n");
        if (wrapg->global_hostapd)
            wpa_ctrl_close(wrapg->global_hostapd);
        wpa_ctrl_close(wrapg->global_wpa_supp);
        close(wrapg->ioctl_sock);
        os_free(wrapg);
        return NULL;
    }
    os_memcpy(wrapg->wifiX_dev_addr, ifr.ifr_hwaddr.sa_data, IEEE80211_ADDR_LEN);

    if (timer == 1)
        wrapg->do_timer = 1;
    else
        wrapg->do_timer = 0;

    return (void *) wrapg;
}

