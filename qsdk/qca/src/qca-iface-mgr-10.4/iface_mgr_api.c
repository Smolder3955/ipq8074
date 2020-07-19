/*
 * Copyright (c) 2016-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * 2016 Qualcomm Atheros, Inc.
 *
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * Copyright (c) 2004, Sam Leffler <sam@errno.com>
 * Copyright (c) 2004, Video54 Technologies
 * Copyright (c) 2005-2007, Jouni Malinen <j@w1.fi>
 * Copyright (c) 2009, Atheros Communications
 *
 * This software may be distributed under the terms of the BSD license.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name(s) of the above-listed copyright holder(s) nor the
 *    names of its contributors may be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/un.h>

#include "includes.h"
#include "common.h"
#include "linux_ioctl.h"
#include "wpa_ctrl.h"
#include "ieee802_11_defs.h"
#include "linux_wext.h"
#include "eloop.h"
#include "netlink.h"
#include "priv_netlink.h"
#include <fcntl.h>
#include <errno.h>

#ifdef IFACE_SUPPORT_CFG80211
#include "wlanif_cmn.h"
#include "qwrap_structure.h"
#endif

#include "iface_mgr_api.h"

#define _BYTE_ORDER _BIG_ENDIAN
#include "ieee80211_external.h"

#define MAX_RADIOS_CNT 3
#define MAX_RADIOS_CNT_FAST_LANE 2

#define CONN_FAIL_COUNT    10

#define WPA_S_MSG_ADDR_OFF      3

#define MAX_AP_VAPS_CNT 16

#define MAX_PLC_IFACE 1

#define IFACEMGR_MAX_CMD_LEN 128

#define HYD_BACK_HAUL_IFACE_PLC_UP        0x01
#define HYD_BACK_HAUL_IFACE_PLC_DOWN      0x02
#define IFACE_MGR_APVAP_UP_DELAY          1
#define IFACE_MGR_APVAP_RETRY_TIMER       8

#ifdef IFACE_SUPPORT_CFG80211
struct wlanif_config *wlanIfIfM = NULL;
#endif

struct ifacemgr_event_message {
    unsigned int cmd;
    unsigned int len;
    unsigned char data[1];
} __attribute__ ((packed));


/* Socket for HYD and IFACE MGR communication */
#define HYD_IFACE_MGR_SOCKET_CLIENT    "/var/run/hyd_iface_mgr_socket_client"

struct ifacemgr_ctrl {
    int sock;
    struct sockaddr_un local;
    struct sockaddr_un dest;
};

/*STAVAP priority*/
typedef enum {
    NO_PRIORITY = 0,
    HIGH_PRIORITY_STAVAP = 1,
    LOW_PRIORITY_STAVAP = 2,
} STAVAP_PRIORITY;

struct sta_vap {
    char ifname[IFNAMSIZ];
    char wifi_ifname[IFNAMSIZ];
    struct ifacemgr_ctrl *ifacemgr_stavap_conn;
    u_int8_t sta_vap_up;
    char *wpa_conf_file;
    u_int8_t conn_fail_count;
    struct group_data *gptr;
    STAVAP_PRIORITY priority;
    u8 stavap_mac_addr[IEEE80211_ADDR_LEN];
};

struct plc_iface {
    char ifname[IFNAMSIZ];
    u_int8_t plc_iface_up;
    struct group_data *gptr;
    int hyd_sock;
};

struct group_data {
    u_int8_t num_sta_vaps;
    u_int8_t num_sta_vaps_up;
    u_int8_t num_ap_vaps;
    u_int8_t is_primary_group;
    u_int8_t group_idx;
    struct sta_vap *stavap[MAX_RADIOS_CNT];
    char *ap_vap[MAX_AP_VAPS_CNT];
    struct plc_iface *plciface[MAX_PLC_IFACE];
    struct iface_daemon *iptr;
    u_int8_t num_plc_iface;
    u_int8_t num_plc_iface_up;

    /* variables for controlling the apvaps bringup process */
    u_int8_t retry_count;
    u_int8_t ap_vap_state_set_ret[MAX_AP_VAPS_CNT];
    u_int8_t ap_vap_cac_state[MAX_AP_VAPS_CNT];
};

struct iface_daemon {
    u_int8_t mode;
    int8_t primary_group_idx;
    u_int16_t timeout;
    int ioctl_sock;
    struct netlink_data *netlink;
    int we_version;
    struct wpa_ctrl *global;
    struct group_data group[MAX_RADIOS_CNT];
    char wifi_iface[IFNAMSIZ];
    u_int16_t stavap_scan_time;
    u_int16_t periodic_scan_time;
    enum driver_mode drv_mode;
    u_int8_t same_ssid_support;
};

#define IFACE_MODE_GLOBALWDS 1
#define IFACE_MODE_FASTLANE 2
#define HIGH_PRIORITY_STAVAP_SCAN_TIME 120
#define HIGH_PRIORITY_STAVAP_PERIODIC_SCAN_TIME 600
static int ping_interval = 5;

static void
ifacemgr_up_high_priority_stavap(void *eloop_ctx, void *timeout_ctx);

//No-op function defined to handle build/run-time link for SON libraries
void wlanifBSteerEventsMsgRx(void *state,
                             const ath_netlink_bsteering_event_t *event) {
    return;
}

void
ifacemgr_wpa_s_ctrl_iface_process(struct sta_vap *stavap, char *msg, int stavap_poll_status);

static inline int is_zero_mac_addr(const u8 *mac)
{
    return !(mac[0] | mac[1] | mac[2] | mac[3] | mac[4] | mac[5]);
}

static const char *wpa_s_ctrl_iface_dir = "/var/run/wpa_supplicant";
static void
ifacemgr_update_wpas_bssid_config(struct sta_vap *stavap, const u8 *addr);

ifacemgr_hdl_t *
ifacemgr_load_conf_file(const char *conf_file, int *num_sta_vaps, int *num_plc_ifaces, enum driver_mode *drvmode)
{
    FILE *f;
    char buf[256], *pos, *start, *iface, *group, *iface_type, *priority;
    int line = 0, i = 0;
    int group_id = 0;

    struct iface_daemon *iptr;
    struct sta_vap *stavap = NULL;
    struct plc_iface *plciface = NULL;
    char *ap_ifname = NULL;
    int stavap_configured = 0;
    int plciface_configured = 0;
    int hp_stavap_configured = 0;

    iptr = os_zalloc(sizeof(*iptr));
    if (!iptr)
	return NULL;

    f = fopen(conf_file, "r");
    if (f == NULL) {
	ifacemgr_printf("Cant open oma conf file(%s)", conf_file);
	return NULL;
    }
    iptr->stavap_scan_time = HIGH_PRIORITY_STAVAP_SCAN_TIME;
    iptr->periodic_scan_time = HIGH_PRIORITY_STAVAP_PERIODIC_SCAN_TIME;
    iptr->drv_mode = WEXT_MODE;
    *drvmode = WEXT_MODE;
    iptr->same_ssid_support = 0;

    while ((fgets(buf, sizeof(buf), f))) {
	line ++;
	if ((buf[0] == '#') || (buf[0] == ' '))
	    continue;
	pos = buf;
	while (*pos != '\0') {
	    if (*pos == '=') {
		pos ++;
		break;
	    }
	    pos ++;
	}
	while (*pos != '\0') {
	    if (*pos == ' ') {
		pos ++;
		continue;
	    } else {
		break;
	    }
	}
	start = pos;
	while (*pos != '\0') {
	    if (*pos == '\n') {
		*pos = '\0';
		break;
	    }
	    pos ++;
	}

	switch(buf[0])
	{
	    case 'M':
	    case 'm':
		iptr->mode = atoi(start);
		break;
	    case 'T':
	    case 't':
		iptr->timeout = atoi(start);
		break;
	    case 'R':
	    case 'r':
		strlcpy(iptr->wifi_iface, start, IFNAMSIZ);
                ifacemgr_printf("iptr->radio:%s",iptr->wifi_iface);
		break;
	    case 'S':
	    case 's':
		iptr->stavap_scan_time = atoi(start);
                ifacemgr_printf("iptr->stavap_scan_time:%d",iptr->stavap_scan_time);
		break;
	    case 'P':
	    case 'p':
		iptr->periodic_scan_time = atoi(start);
                ifacemgr_printf("iptr->periodic_scan_time:%d",iptr->periodic_scan_time);
		break;
	    case 'D':
	    case 'd':
                if(os_memcmp(start,"cfg80211",9) == 0) {
		    iptr->drv_mode = CFG80211_MODE;
		    *drvmode = CFG80211_MODE;
                }
		break;
	    case 'G':
	    case 'g':
		group=start;
		while (*start != '\0') {
		    if (*start == ' ') {
			*start = '\0';
			group_id=atoi(group);
			start ++;
			break;
		    }
		    start ++;
		}
		if (group_id >=3) {
		    ifacemgr_printf("group_id(%d) should not be greater than or equal to 3",group_id);
		    break;
		}
		iface_type=start;
		while (*start != '\0') {
		    if (*start == '=') {
			start ++;
			break;
		    }
		    start ++;
		}
		while (*start != '\0') {
		    if (*start == ' ') {
			start ++;
			continue;
		    } else {
			break;
		    }
		}

		iface=start;
		while (*start != '\0') {
		    if (*start == ' ') {
			*start = '\0';
			start ++;
			break;
		    }
		    start ++;
		}
		while (*start != '\0') {
		    if (*start == '=') {
			start ++;
			break;
		    }
		    start ++;
		}

                priority = start;
		ap_ifname = NULL;
		if ((iface_type[0] == 'A') || (iface_type[0] == 'a')) {
		    for (i = 0; i < MAX_AP_VAPS_CNT; i++) {
			if (iptr->group[group_id].ap_vap[i] == NULL) {
			    ap_ifname = os_strdup(iface);
			    iptr->group[group_id].ap_vap[i] = ap_ifname;
			    break;
			}
		    }
		    if (ap_ifname) {
			iptr->group[group_id].num_ap_vaps ++;
		    } else {
			ifacemgr_printf("line:%d Not able to add ap vap", line);
		    }

		    stavap = NULL;
		} else if ((iface_type[0] == 'S') || (iface_type[0] == 's')) {
		    for (i = 0; i < MAX_RADIOS_CNT; i++) {
			if (iptr->group[group_id].stavap[i] == NULL) {
			    stavap = (struct sta_vap *)os_malloc(sizeof(struct sta_vap));
			    iptr->group[group_id].stavap[i] = stavap;
			    break;
			}
		    }
		    if (stavap) {
			strlcpy(stavap->ifname, iface, IFNAMSIZ);
			iptr->group[group_id].num_sta_vaps ++;
			stavap_configured ++;
			    if ((priority[0] == '1') && !hp_stavap_configured) {
                                 stavap->priority = HIGH_PRIORITY_STAVAP;
                                 hp_stavap_configured = 1;
			    } else if (priority[0] == '2') {
                                 stavap->priority = LOW_PRIORITY_STAVAP;
                            } else {
			         stavap->priority = NO_PRIORITY;
			    }

		    } else {
			ifacemgr_printf("line:%d Not able to add sta vap", line);
		    }
		} else if ((iface_type[0] == 'P') || (iface_type[0] == 'p')) {
		    for (i = 0; i < MAX_PLC_IFACE; i++) {
			if (iptr->group[group_id].plciface[i] == NULL) {
			    plciface = (struct plc_iface *)os_malloc(sizeof(struct plc_iface));
			    iptr->group[group_id].plciface[i] = plciface;
			    break;
			}
		    }
		    if (plciface) {
			strlcpy(plciface->ifname, iface, IFNAMSIZ);
			iptr->group[group_id].num_plc_iface ++;
			plciface_configured ++;
		    } else {
			ifacemgr_printf("line:%d Not able to add plc iface", line);
		    }

		} else {
		    ifacemgr_printf("line:%d unknown interface", line);
		}
		break;
	    case 'E':
	    case 'e':
		iptr->same_ssid_support = atoi(start);
		break;
	    default:
		ifacemgr_printf("wrong input");
		break;
	}
    }
    *num_sta_vaps = stavap_configured;
    *num_plc_ifaces = plciface_configured;

    if (stavap_configured == 0) {
	ifacemgr_printf("No STA VAPs are configured");
    }
    if (plciface_configured == 0) {
	ifacemgr_printf("No PLC ifaces are configured");
    }
    if (hp_stavap_configured == 0) {
	iptr->stavap_scan_time = 0;
	iptr->periodic_scan_time = 0;
    }
    ifacemgr_printf("num_sta_vaps:%d num_plc_ifaces:%d drv_mode:%d same_ssid_support:%d Mode:%d",*num_sta_vaps,*num_plc_ifaces,*drvmode,iptr->same_ssid_support,iptr->mode);

    close((int)f);

    return (void *) iptr;
}

#ifdef IFACE_SUPPORT_CFG80211
int wlanIfConfigInitIface(int cfg80211)
{
    ifacemgr_printf("%s: wlanIf %p\n", __func__, wlanIfIfM);

    /* Avoid multiple inits */
    if (wlanIfIfM) {
        ifacemgr_printf("%s: wlanIf Iface Manager %p already initialised\n", __func__, wlanIfIfM);
        return 0;
    }

    if (cfg80211) {
        wlanIfIfM = wlanif_config_init(WLANIF_CFG80211, IFACEMGR_NL80211_CMD_SOCK, IFACEMGR_NL80211_EVENT_SOCK);
    } else {
        wlanIfIfM = wlanif_config_init(WLANIF_WEXT, 0, 0);
    }

    if (!wlanIfIfM) {
        return -1;
    }

    ifacemgr_printf("%s: wlanIf Iface Manager %p INIT DONE\n", __func__, wlanIfIfM);
    return 0;
}
#endif

int
ifacemgr_conn_to_global_wpa_s(const char *global_supp, ifacemgr_hdl_t *ifacemgr_handle)
{

    struct iface_daemon *iptr = (struct iface_daemon *)ifacemgr_handle;
    char *realname = (void *)global_supp;
    if (!realname)
	return 0;

    iptr->ioctl_sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (iptr->ioctl_sock < 0) {
	ifacemgr_printf("socket[PF_INET,SOCK_DGRAM]");
	return 0;
    }

    iptr->global = wpa_ctrl_open(realname);
    if (!iptr->global) {
	close(iptr->ioctl_sock);
	ifacemgr_printf("Fail to connect global wpa_s");
	return 0;
    }

    ifacemgr_printf("connected to global wpa_s");
    return 1;

}


static void
ifacemgr_bringdown_apvaps(struct group_data *gptr)
{
    int i=0;
    char cmd[IFACEMGR_MAX_CMD_LEN] = {0};
    size_t res;

    for (i=0;i<gptr->num_ap_vaps;i++) {
        if (gptr->ap_vap[i]) {
            if ((gptr->ap_vap_cac_state[i] == 1) &&(IFACE_MODE_GLOBALWDS == (gptr->iptr->mode))) {
                continue;
            }
            ifacemgr_printf("Bringing down AP VAPs(%s)",gptr->ap_vap[i]);
            res = snprintf(cmd, sizeof(cmd), "ifconfig %s down", gptr->ap_vap[i]);
            if (res < 0 || res >= sizeof(cmd))
                return;

            cmd[sizeof(cmd) - 1] = '\0';
            system(cmd);
        } else {
            ifacemgr_printf("AP VAP iface not found");
        }
    }
}

static void
ifacemgr_bringup_apvaps(struct group_data *gptr)
{
    int i=0;
    char cmd[IFACEMGR_MAX_CMD_LEN] = {0};
    size_t res;

    for (i=0;i<gptr->num_ap_vaps;i++) {
	if (gptr->ap_vap[i]) {
            ifacemgr_printf("Bringing up AP VAPs(%s)",gptr->ap_vap[i]);
	    res = snprintf(cmd, sizeof(cmd), "ifconfig %s up", gptr->ap_vap[i]);
	    if (res < 0 || res >= sizeof(cmd))
		return;

	    cmd[sizeof(cmd) - 1] = '\0';
	    system(cmd);
	} else {
	    ifacemgr_printf("AP VAP iface not found");
	}
    }
}

static void ifacemgr_bringup_apvaps_stop (struct group_data *gptr);

/*
 * the actually timer handler for bringup apvaps and retry
 */
static void
ifacemgr_bringup_apvaps_timer(void *eloop_ctx, void *timeout_ctx)
{
    struct group_data *gptr = (struct group_data *)eloop_ctx;

    gptr->retry_count++;
    ifacemgr_printf("Timer ticks for bringup apvaps.");

    int i=0;
    char cmd[IFACEMGR_MAX_CMD_LEN] = {0};
    size_t res;
    u_int8_t allDone = 1;
    for (i=0;i<gptr->num_ap_vaps;i++) {
        if ((gptr->ap_vap[i]) && (gptr->ap_vap_state_set_ret[i] != 0)) {
            res = snprintf(cmd, sizeof(cmd), "ifconfig %s up; ifconfig %s | grep \"UP BROADCAST RUNNING\"",
                           gptr->ap_vap[i], gptr->ap_vap[i]);
            if (res < 0 || res >= sizeof(cmd))
                return;

            cmd[sizeof(cmd) - 1] = '\0';
            int ret = system(cmd);
            if (ret != 0)
                allDone = 0;
            gptr->ap_vap_state_set_ret[i] = (ret == 0) ? 0: 1;
            ifacemgr_printf("Bringing up AP VAPs(%s), result:%d",gptr->ap_vap[i], ret);
        }
    }

    if (gptr->retry_count >= 3 || allDone) {
        ifacemgr_printf("Bringup apvaps process Finished. retry count=%d, allDone=%d", gptr->retry_count, allDone);
        ifacemgr_bringup_apvaps_stop(gptr);
    } else {
        eloop_cancel_timeout(ifacemgr_bringup_apvaps_timer, gptr, NULL);
        eloop_register_timeout(IFACE_MGR_APVAP_RETRY_TIMER, 0, ifacemgr_bringup_apvaps_timer, gptr, NULL);
    }
}

/*
 * call this function to bringup apvaps. since the command may fail
 * so timer based retry may take place on failure.
 */
static void ifacemgr_bringup_apvaps_start(struct group_data *gptr)
{
    int i;
    ifacemgr_printf("Start bringup apvaps process");
    gptr->retry_count = 0;

    for (i=0;i<gptr->num_ap_vaps;i++) {
        gptr->ap_vap_state_set_ret[i] = 1;
    }

    eloop_register_timeout(IFACE_MGR_APVAP_UP_DELAY, 0, ifacemgr_bringup_apvaps_timer, gptr, NULL);
}

/*
 * call this function to stop the bringup process.
 * it may be called when the process finished or retry for 3 times
 */
static void ifacemgr_bringup_apvaps_stop(struct group_data *gptr)
{
    int i;
    ifacemgr_printf("Stop bringup apvaps process");
    eloop_cancel_timeout(ifacemgr_bringup_apvaps_timer, gptr, NULL);
    gptr->retry_count = 0;

    for (i=0;i<gptr->num_ap_vaps;i++) {
        gptr->ap_vap_state_set_ret[i] = 0;
    }

}
static void
ifacemgr_bring_updown_stavaps(struct group_data *gptr,int op, STAVAP_PRIORITY priority)
{
    int i=0;
    char cmd[IFACEMGR_MAX_CMD_LEN] = {0};
    size_t res;

    for (i=0;i<gptr->num_sta_vaps;i++) {
        if (gptr->stavap[i]) {
            if((priority == NO_PRIORITY) || (gptr->stavap[i]->priority == priority)) {
            ifacemgr_printf("Bringing %s STA VAPs(%s) priority:%d",op ? "up" : "down",gptr->stavap[i]->ifname, priority);
            res = snprintf(cmd, sizeof(cmd), "ifconfig %s %s", gptr->stavap[i]->ifname,op ? "up" : "down");
            if (res < 0 || res >= sizeof(cmd))
                return;

            cmd[sizeof(cmd) - 1] = '\0';
            system(cmd);
            }
        } else {
            ifacemgr_printf("STA VAP iface not found");
        }
    }
}
#define ifacemgr_bringdown_stavaps(gptr) ifacemgr_bring_updown_stavaps(gptr,0,NO_PRIORITY)

#define ifacemgr_bringup_stavaps(gptr) ifacemgr_bring_updown_stavaps(gptr,1,NO_PRIORITY)

static void
ifacemgr_disconn_timer(void *eloop_ctx, void *timeout_ctx)
{
    struct group_data *gptr = (struct group_data *)eloop_ctx;

    ifacemgr_bringdown_apvaps(gptr);
    eloop_cancel_timeout(ifacemgr_disconn_timer, gptr, NULL);
    return;
}

static void
ifacemgr_primarygrp_conn_timer(void *eloop_ctx, void *timeout_ctx)
{
    struct group_data *gptr = (struct group_data *)eloop_ctx;

    ifacemgr_bringdown_stavaps(gptr);
    return;
}

static void
ifacemgr_hp_stavap_disconnection(void *eloop_ctx, void *timeout_ctx)
{
    struct sta_vap *stavap = (struct sta_vap *)eloop_ctx;
    struct group_data *gptr = stavap->gptr;
    struct iface_daemon *iptr = gptr->iptr;
    ifacemgr_bring_updown_stavaps(gptr,0,HIGH_PRIORITY_STAVAP);
    ifacemgr_bring_updown_stavaps(gptr,1,LOW_PRIORITY_STAVAP);
    eloop_register_timeout(iptr->periodic_scan_time, 0, ifacemgr_up_high_priority_stavap, stavap, NULL);
    return;
}

static void
ifacemgr_down_high_priority_stavap(void *eloop_ctx, void *timeout_ctx)
{
    struct sta_vap *stavap = (struct sta_vap *)eloop_ctx;
    struct group_data *gptr = stavap->gptr;
    struct iface_daemon *iptr = gptr->iptr;
    ifacemgr_bring_updown_stavaps(gptr,0,HIGH_PRIORITY_STAVAP);
    eloop_register_timeout(iptr->periodic_scan_time, 0, ifacemgr_up_high_priority_stavap, stavap, NULL);
    return;
}

static void
ifacemgr_up_high_priority_stavap(void *eloop_ctx, void *timeout_ctx)
{
    struct sta_vap *stavap = (struct sta_vap *)eloop_ctx;
    struct group_data *gptr = stavap->gptr;
    struct iface_daemon *iptr = gptr->iptr;
    ifacemgr_bring_updown_stavaps(gptr,1,HIGH_PRIORITY_STAVAP);
    eloop_register_timeout(iptr->stavap_scan_time, 0, ifacemgr_down_high_priority_stavap, stavap, NULL);
    return;
}

static void
ifacemgr_backhaul_up_down_event_process(struct group_data *gptr, int event_flag)
{
    struct iface_daemon *iptr = gptr->iptr;
#ifndef IFACE_SUPPORT_CFG80211
    struct ifreq ifr;
#endif
    struct extended_ioctl_wrapper extended_cmd;
    int ret;

    if (((gptr->num_sta_vaps_up + gptr->num_plc_iface_up) ==0) && !event_flag) {
	/* Get Disconnection timeout*/
#ifndef IFACE_SUPPORT_CFG80211
	os_memset(&ifr, 0, sizeof(ifr));
	os_memcpy(ifr.ifr_name, iptr->wifi_iface, IFNAMSIZ);
	extended_cmd.cmd = EXTENDED_SUBIOCTL_GET_DISCONNECTION_TIMEOUT;
	ifr.ifr_data = (caddr_t) &extended_cmd;
	extended_cmd.data = (caddr_t)&iptr->timeout;
	if ((ret = ioctl(iptr->ioctl_sock, SIOCGATHEXTENDED, &ifr)) != 0) {
	    ifacemgr_printf("ret=%d ioctl SIOCGATHEXTENDED get timeout err", ret);
	}
#else
    memset(&extended_cmd.ext_data.iface_config, 0, sizeof(struct iface_config_t));
    extended_cmd.cmd = EXTENDED_SUBIOCTL_GET_DISCONNECTION_TIMEOUT;
    extended_cmd.data_len = sizeof(extended_cmd.ext_data.iface_config);
    extended_cmd.ext_data.iface_config.timeout = iptr->timeout;
    if((ret =(wlanIfIfM->ops->getExtended(wlanIfIfM->ctx,
                        iptr->wifi_iface, &extended_cmd, sizeof(extended_cmd)))) < 0) {
        ifacemgr_printf("%s ret=%d SIOCGATHEXTENDED get timeout err", __func__, ret);
    }
    iptr->timeout = extended_cmd.ext_data.iface_config.timeout;
#endif

	ifacemgr_printf("iptr->timeout: %d stavap_scan_time:%d", iptr->timeout,iptr->stavap_scan_time);

	eloop_register_timeout((iptr->stavap_scan_time + iptr->timeout), 0, ifacemgr_disconn_timer, gptr, NULL);
        ifacemgr_bringup_apvaps_stop(gptr);
    }

    if (((gptr->num_sta_vaps_up + gptr->num_plc_iface_up) ==1) && event_flag) {
        eloop_cancel_timeout(ifacemgr_disconn_timer, gptr, NULL);
        ifacemgr_bringup_apvaps_start(gptr);
    }
    return;
}


static int
ifacemgr_stavap_connection_status(struct iface_daemon *iptr, struct sta_vap *stavap)
{
    struct extended_ioctl_wrapper extended_cmd;
    int ret = 0;
    int stavap_status = 0;
    /* Get stavap connection status*/
#ifndef IFACE_SUPPORT_CFG80211
    struct ifreq ifr;
    os_memset(&ifr, 0, sizeof(ifr));
    os_memcpy(ifr.ifr_name, stavap->wifi_ifname, IFNAMSIZ);
    extended_cmd.cmd = EXTENDED_SUBIOCTL_GET_STAVAP_CONNECTION;
    ifr.ifr_data = (caddr_t) &extended_cmd;
    extended_cmd.data = (caddr_t)&stavap_status;
    if ((ret = ioctl(iptr->ioctl_sock, SIOCGATHEXTENDED, &ifr)) != 0) {
        ifacemgr_printf("ret=%d ioctl SIOCGATHEXTENDED getsta_vap_up err", ret);
    }
#else
    os_memset(&extended_cmd.ext_data.iface_config, 0, sizeof(struct iface_config_t));
    extended_cmd.cmd = EXTENDED_SUBIOCTL_GET_STAVAP_CONNECTION;
    extended_cmd.data_len = sizeof(extended_cmd.ext_data.iface_config);
    extended_cmd.ext_data.iface_config.stavap_up = stavap->sta_vap_up;
    if ((ret =(wlanIfIfM->ops->getExtended(wlanIfIfM->ctx,
                        stavap->wifi_ifname, &extended_cmd, sizeof(extended_cmd)))) < 0) {
        ifacemgr_printf("%s ret=%d ioctl SIOCGATHEXTENDED getsta_vap_up err stavap_up=%d", __func__,
                ret, extended_cmd.ext_data.iface_config.stavap_up);
    }
    stavap_status = extended_cmd.ext_data.iface_config.stavap_up;
#endif

    return stavap_status;
}

static int
ifacemgr_stavap_status_check(struct iface_daemon *iptr)
{
    struct group_data *gptr = NULL;
    int i = 0, j = 0;
    struct sta_vap *stavap = NULL;
    char msg[256];
    int ret =0;
    int stavap_status_mismatch = 0;

    for (i = 0; i < MAX_RADIOS_CNT; i++) {
        gptr =  &(iptr->group[i]);
        for (j = 0; j < gptr->num_sta_vaps; j++) {
            if(gptr->stavap[j] == NULL) {
                continue;
            }
            stavap = gptr->stavap[j];
#ifdef IFACE_SUPPORT_CFG80211 
            /* WAR for iface-mgr cfg support*/
            ret = ifacemgr_stavap_connection_status(iptr, stavap);
            if (ret != stavap->sta_vap_up) {
                os_memset(msg, 0, sizeof(msg));
                ifacemgr_printf("stavap(%s) status mismatch found, driver status:%d",stavap->ifname,ret);
                ifacemgr_wpa_s_ctrl_iface_process(stavap, msg, ret);
                stavap_status_mismatch = 1;
            }
#endif
        }
    }
    return stavap_status_mismatch;

}

#define PERIODIC_POLL_TIME 60

/* Poll STAVAP staus, to know if iface-mgr missed disconnection event from
   wpa_supplicant*/

static void
ifacemgr_poll_stavap_status(void *eloop_ctx, void *timeout_ctx)
{
    struct iface_daemon *iptr = (struct iface_daemon *)eloop_ctx;
    int ret;
    ret = ifacemgr_stavap_status_check(iptr);
    if (ret) {
        eloop_cancel_timeout(ifacemgr_poll_stavap_status, iptr, NULL);
        eloop_register_timeout(PERIODIC_POLL_TIME, 0, ifacemgr_poll_stavap_status, iptr, NULL);
    }
    return;
}

/* When both STAVAPs disconnected, periodically poll driver for STAVAP status
   Till, CONNECTION status obtained from driver or from supplicant.
 */
static void
ifacemgr_driver_periodic_poll(void *eloop_ctx, void *timeout_ctx)
{
    struct iface_daemon *iptr = (struct iface_daemon *)eloop_ctx;
    ifacemgr_stavap_status_check(iptr);
    eloop_register_timeout(PERIODIC_POLL_TIME, 0, ifacemgr_driver_periodic_poll, iptr, NULL);
    return;
}


void
ifacemgr_wpa_s_ctrl_iface_process(struct sta_vap *stavap, char *msg, int stavap_poll_status)
{
    struct group_data *gptr = stavap->gptr;
    struct iface_daemon *iptr = gptr->iptr;

    if ((os_strncmp(msg + WPA_S_MSG_ADDR_OFF, "CTRL-EVENT-DISCONNECTED ", 24) == 0) || (stavap_poll_status == 0)) {
        if (!stavap->sta_vap_up) {
            return;
        }
        ifacemgr_printf("CTRL-EVENT-DISCONNECTED(%s)",stavap->ifname);
        if (iptr->same_ssid_support) {
            ifacemgr_update_wpas_bssid_config(stavap, stavap->stavap_mac_addr);
        }
        stavap->sta_vap_up = 0;
        gptr->num_sta_vaps_up --;

        if (gptr->num_sta_vaps_up == 0) {

            if(IFACE_MODE_FASTLANE == (iptr->mode)) {
                /*Bring Up the STA VAP of alternate group*/
                int tmp_grp_idx = (gptr->is_primary_group) ? (!iptr->primary_group_idx) : (iptr->primary_group_idx);
                struct group_data *tmp_gptr = &(iptr->group[tmp_grp_idx]);
                struct group_data *primary_gptr = &(iptr->group[iptr->primary_group_idx]);
                eloop_cancel_timeout(ifacemgr_primarygrp_conn_timer, primary_gptr, NULL);
                ifacemgr_bringup_stavaps(tmp_gptr);
                eloop_cancel_timeout(ifacemgr_poll_stavap_status, iptr, NULL);
                eloop_cancel_timeout(ifacemgr_driver_periodic_poll, iptr, NULL);
                eloop_register_timeout(PERIODIC_POLL_TIME, 0, ifacemgr_driver_periodic_poll, iptr, NULL);
	    } else {
                ifacemgr_backhaul_up_down_event_process(gptr, 0);
            }
        }
        if (IFACE_MODE_GLOBALWDS == (iptr->mode)) {
            if(stavap->priority == HIGH_PRIORITY_STAVAP){
                eloop_register_timeout(iptr->stavap_scan_time, 0, ifacemgr_hp_stavap_disconnection, stavap, NULL);
            }
        }
    } else if ((os_strncmp(msg + WPA_S_MSG_ADDR_OFF, "CTRL-EVENT-CONNECTED ", 21) == 0) || (stavap_poll_status == 1)) {
        if (stavap->sta_vap_up) {
            return;
        }
        stavap->sta_vap_up = 1;
        gptr->num_sta_vaps_up ++;
        ifacemgr_printf("CTRL-EVENT-CONNECTED(%s)",stavap->ifname);
        if (gptr->num_sta_vaps_up == 1) {
            if(IFACE_MODE_FASTLANE == (iptr->mode)) {
                if(gptr->is_primary_group) {
                    struct group_data *secondary_gptr = &(iptr->group[!iptr->primary_group_idx]);
                    eloop_cancel_timeout(ifacemgr_primarygrp_conn_timer, gptr, NULL);
                    ifacemgr_bringup_apvaps(gptr);
                    /*Bring down secondary group if up*/
                    ifacemgr_bringdown_apvaps(secondary_gptr);
                    ifacemgr_bringdown_stavaps(secondary_gptr);
                } else {
                    struct group_data *primary_gptr = &(iptr->group[iptr->primary_group_idx]);
                    ifacemgr_bringup_apvaps(gptr);
                    ifacemgr_bringdown_apvaps(primary_gptr);
                    eloop_register_timeout(iptr->timeout, 0, ifacemgr_primarygrp_conn_timer, primary_gptr, NULL);
                }
                eloop_cancel_timeout(ifacemgr_driver_periodic_poll, iptr, NULL);
            } else {
                ifacemgr_backhaul_up_down_event_process(gptr, 1);
            }
        }
        if ((gptr->num_sta_vaps_up == 2) && (IFACE_MODE_FASTLANE == (iptr->mode))) {
            if(gptr->is_primary_group) {
                struct group_data *secondary_gptr = &(iptr->group[!iptr->primary_group_idx]);
                eloop_cancel_timeout(ifacemgr_primarygrp_conn_timer, gptr, NULL);
                ifacemgr_bringup_apvaps(gptr);
                /*Bring down secondary group if up*/
                ifacemgr_bringdown_apvaps(secondary_gptr);
                ifacemgr_bringdown_stavaps(secondary_gptr);
            }
        }
        if ((IFACE_MODE_GLOBALWDS == (iptr->mode)) && (stavap->priority == HIGH_PRIORITY_STAVAP)){
                ifacemgr_bring_updown_stavaps(gptr,0,LOW_PRIORITY_STAVAP);
                eloop_cancel_timeout(ifacemgr_hp_stavap_disconnection, stavap, NULL);
                eloop_cancel_timeout(ifacemgr_down_high_priority_stavap, stavap, NULL);
                eloop_cancel_timeout(ifacemgr_up_high_priority_stavap, stavap, NULL);
        }
    } else {
       //             ifacemgr_printf("Unknown msg(%s)", msg + WPA_S_MSG_ADDR_OFF);
    }
}

void
ifacemgr_wpa_s_ctrl_iface_receive(int sock, void *eloop_ctx, void *sock_ctx)
{
    struct sta_vap *stavap = (struct sta_vap *)eloop_ctx;
    char msg[256];
    int res;
    struct sockaddr_un from;
    socklen_t fromlen = sizeof(from);

    res = recvfrom(sock, msg, sizeof(msg) - 1, 0, (struct sockaddr *) &from, &fromlen);
    if (res < 0) {
        ifacemgr_printf("recvfrom err");
        return;
    }
    msg[res] = '\0';
    ifacemgr_wpa_s_ctrl_iface_process(stavap, msg, 0xff);
}


struct ifacemgr_ctrl *
ifacemgr_conn_to_sta_wpa_s(char *ifname)
{
    struct wpa_ctrl *priv = NULL;
    char *cfile;
    int flen;

    if (ifname == NULL)  {
        ifacemgr_printf("ERROR:ifname is Null in %s\n", __func__);
        return NULL;
    }

    flen = strlen(wpa_s_ctrl_iface_dir) + (2*strlen(ifname)) + 3;
    cfile = malloc(flen);
    if (cfile == NULL)
        return NULL;

    snprintf(cfile, flen, "%s-%s/%s", wpa_s_ctrl_iface_dir, ifname, ifname);
    priv = wpa_ctrl_open(cfile);
    if (!priv) {
        ifacemgr_printf("cfile %s connection to sta vap failed\n", cfile);
    }
    free(cfile);
    return (struct ifacemgr_ctrl *)priv;
}

#ifndef IFACE_SUPPORT_CFG80211
static int
ifacemgr_get_80211param(struct iface_daemon *iptr, char *ifname, int op, int *data)
{
    struct iwreq iwr;

    os_memset(&iwr, 0, sizeof(iwr));
    os_strlcpy(iwr.ifr_name, ifname, IFNAMSIZ);

    iwr.u.mode = op;

    if (ioctl(iptr->ioctl_sock, IEEE80211_IOCTL_GETPARAM, &iwr) < 0) {
        ifacemgr_printf("ioctl IEEE80211_IOCTL_GETPARAM err, ioctl(%d) op(%d)",
                IEEE80211_IOCTL_GETPARAM, op);
        return -1;
    }

    *data = iwr.u.mode;
    return 0;
}
#endif

void
ifacemgr_ifname_to_parent_ifname(struct iface_daemon *iptr, char *child, char *parent)
{
    struct ifreq ifr;
    int parent_index=0;

#ifndef IFACE_SUPPORT_CFG80211
    ifacemgr_get_80211param(iptr, child, IEEE80211_PARAM_PARENT_IFINDEX, &parent_index);
#else
    if(wlanIfIfM->ops->getParentIfindex(wlanIfIfM->ctx, child, &parent_index) < 0) {
        ifacemgr_printf("ioctl get Parent Failed\n");
    }
#endif
    ifacemgr_printf("%s parent_index %d \n",__func__,parent_index);

    os_memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_ifindex = parent_index;
    if (ioctl(iptr->ioctl_sock, SIOCGIFNAME, &ifr) != 0) {
        ifacemgr_printf("ioctl SIOCGIFNAME err");
        return;
    }

    os_memcpy(parent, ifr.ifr_name, IFNAMSIZ);
}

void ifacemgr_free_ifacemgr_handle(ifacemgr_hdl_t *ifacemgr_handle)
{
    struct iface_daemon *iptr = (struct iface_daemon *)ifacemgr_handle;
    struct group_data *gptr = NULL;
    int i = 0, j = 0;
    struct sta_vap *stavap = NULL;
    struct plc_iface *plciface = NULL;
    for (i = 0; i < MAX_RADIOS_CNT; i++) {
        gptr =  &(iptr->group[i]);
        gptr->iptr = iptr;
        for (j = 0; j < gptr->num_sta_vaps; j++) {

            if(gptr->stavap[j] == NULL) {
                continue;
            }
            stavap = gptr->stavap[j];
            eloop_unregister_read_sock(stavap->ifacemgr_stavap_conn->sock);
            wpa_ctrl_detach((struct wpa_ctrl *)stavap->ifacemgr_stavap_conn);
            wpa_ctrl_close((struct wpa_ctrl *)stavap->ifacemgr_stavap_conn);
            os_free(stavap->wpa_conf_file);
            os_free(stavap);
        }
      for (j = 0; j < gptr->num_plc_iface; j++) {

            if(gptr->plciface[j] == NULL) {
                continue;
            }
            plciface = gptr->plciface[j];
            eloop_unregister_read_sock(plciface->hyd_sock);
            close(plciface->hyd_sock);
            os_free(plciface);
        }

        for (j = 0; j < gptr->num_ap_vaps; j++) {
            if(gptr->ap_vap[j]) {
                os_free(gptr->ap_vap[j]);
            }
        }

    }
    if (iptr->global) {
	wpa_ctrl_close((struct wpa_ctrl *)iptr->global);
        close(iptr->ioctl_sock);
    }
    os_free(iptr);
}

int ifacemgr_stavap_conn_to_wpa_supplicant(struct sta_vap *stavap)
{

    stavap->ifacemgr_stavap_conn = ifacemgr_conn_to_sta_wpa_s(stavap->ifname);
    if(stavap->ifacemgr_stavap_conn) {
        if (wpa_ctrl_attach((struct wpa_ctrl *)stavap->ifacemgr_stavap_conn) != 0) {
            if(!(stavap->conn_fail_count % CONN_FAIL_COUNT)){
                ifacemgr_printf("Failed to attach to MPSTA wpa_s(%s)", stavap->ifname);
            }
            stavap->conn_fail_count++;
            wpa_ctrl_close((struct wpa_ctrl *)stavap->ifacemgr_stavap_conn);
            stavap->ifacemgr_stavap_conn = NULL;
            return 0;
        }
        eloop_register_read_sock(stavap->ifacemgr_stavap_conn->sock, ifacemgr_wpa_s_ctrl_iface_receive, stavap, NULL);
    } else {
        if(!(stavap->conn_fail_count % CONN_FAIL_COUNT)){
            ifacemgr_printf("STA wpa_s(%s) not exists", stavap->ifname);
        }
        stavap->conn_fail_count++;
        return 0;
    }
    return 1;
}

void
ifacemgr_display_updated_config(ifacemgr_hdl_t *ifacemgr_handle)
{
    struct iface_daemon *iptr = (struct iface_daemon *)ifacemgr_handle;
    struct group_data *gptr = NULL;
    int i = 0, j = 0;
    struct sta_vap *stavap = NULL;
    struct plc_iface *plciface = NULL;

    ifacemgr_printf("Mode:%d timeout:%d",iptr->mode,iptr->timeout);
    for (i = 0; i < MAX_RADIOS_CNT; i++) {
        gptr =  &(iptr->group[i]);
        if (gptr->num_sta_vaps || gptr->num_ap_vaps) {
            printf("\nGroup ID: %d\n", i);
        }
        if (gptr->num_sta_vaps) {
            printf("STA VAP list: ");
        }
        for (j = 0; j < gptr->num_sta_vaps; j++) {

            if(gptr->stavap[j] == NULL) {
                continue;
            }
            stavap = gptr->stavap[j];
            printf("%s\t", stavap->ifname);
        }
        if (gptr->num_ap_vaps) {
            printf("\nAP VAP list: ");
        }
        for (j = 0; j < gptr->num_ap_vaps; j++) {
            if(gptr->ap_vap[j]) {
                printf("%s\t", gptr->ap_vap[j]);
            }
        }
        if (gptr->num_plc_iface) {
            printf("PLC iface list: ");
        }
        for (j = 0; j < gptr->num_plc_iface; j++) {

            if(gptr->plciface[j] == NULL) {
                continue;
            }
            plciface = gptr->plciface[j];
            printf("%s\t", plciface->ifname);
        }

    }
    printf("\n");
}

void
ifacemgr_get_stavap_mac(struct iface_daemon *iptr, struct sta_vap *stavap)
{
    struct ifreq ifr;

    if(strlcpy(ifr.ifr_name, stavap->ifname, IFNAMSIZ) >= IFNAMSIZ) {
        ifacemgr_printf("%s:Interface Name too long.%s\n",__func__,stavap->ifname);
        return;
    }
    /* Get the MAC address */
    if(ioctl(iptr->ioctl_sock, SIOCGIFHWADDR, &ifr) < 0)
    {
        ifacemgr_printf("SIOCGIFHWADDR - Unable to fetch hw address\n");
        return;
    }
    os_memcpy(stavap->stavap_mac_addr, ifr.ifr_hwaddr.sa_data, IEEE80211_ADDR_LEN);
}

void
ifacemgr_status_inform_to_driver(ifacemgr_hdl_t *ifacemgr_handle, u_int8_t iface_mgr_up)
{
    struct iface_daemon *iptr = (struct iface_daemon *)ifacemgr_handle;
    struct extended_ioctl_wrapper extended_cmd;
    int ret = 0;
#ifndef IFACE_SUPPORT_CFG80211
    struct ifreq ifr;
#endif

    /*
     * Inform driver about status of interface manager
     * only if globalwds option is set.
     */
    if (IFACE_MODE_GLOBALWDS == (iptr->mode)) {
#ifndef IFACE_SUPPORT_CFG80211
        os_memset(&ifr, 0, sizeof(ifr));
        os_memcpy(ifr.ifr_name, iptr->wifi_iface, IFNAMSIZ);
        extended_cmd.cmd = EXTENDED_SUBIOCTL_IFACE_MGR_STATUS;
        ifr.ifr_data = (caddr_t) &extended_cmd;
        extended_cmd.data = (caddr_t)&iface_mgr_up;
        if ((ret = ioctl(iptr->ioctl_sock, SIOCGATHEXTENDED, &ifr)) != 0) {
            ifacemgr_printf("ret=%d ioctl SIOCGATHEXTENDED iface_mgr up err", ret);
        }
#else
        memset(&extended_cmd.ext_data.iface_config, 0, sizeof(struct iface_config_t));
        extended_cmd.cmd = EXTENDED_SUBIOCTL_IFACE_MGR_STATUS;
        extended_cmd.data_len = sizeof(extended_cmd.ext_data.iface_config);
        extended_cmd.ext_data.iface_config.iface_mgr_up = iface_mgr_up;
        if ((ret =(wlanIfIfM->ops->getExtended(wlanIfIfM->ctx, iptr->wifi_iface,
                            &extended_cmd, sizeof(extended_cmd)))) < 0) {
            ifacemgr_printf("ret=%d ioctl SIOCGATHEXTENDED iface_mgr up err", ret);
        }
#endif
        else {
            ifacemgr_printf("iface mgr status:%d\n",iface_mgr_up);
        }
    }
}

static void
ifacemgr_reconnect_stavap(struct sta_vap *stavap)
{
    char buf[2048];
    size_t len = sizeof(buf);
    int ret;

    if (!stavap->ifacemgr_stavap_conn) {
        printf("stavap not connected to supplicant stavap:%p\n",stavap);
        return;
    }

    ret = wpa_ctrl_request((struct wpa_ctrl *)stavap->ifacemgr_stavap_conn, "RECONNECT", strlen("RECONNECT"), buf, &len, NULL);
    if (ret < 0) {
        ifacemgr_printf("ifacemgr_reconnect_stavap request failure\n");
        return;
    }
    return;
}

static void
ifacemgr_disconnect_stavap(struct sta_vap *stavap)
{
    char buf[2048];
    size_t len = sizeof(buf);
    int ret;

    if (!stavap->ifacemgr_stavap_conn) {
        return;
    }

    ret = wpa_ctrl_request((struct wpa_ctrl *)stavap->ifacemgr_stavap_conn, "DISCONNECT", strlen("DISCONNECT"), buf, &len, NULL);
    if (ret < 0) {
        ifacemgr_printf("ifacemgr_disconnect_stavap request failure\n");
        return;
    }
    return;
}

static int ifacemgr_stavap_config(struct sta_vap *stavap){
    struct group_data *gptr = stavap->gptr;
    struct iface_daemon *iptr = gptr->iptr;
    struct extended_ioctl_wrapper extended_cmd;
    int  ret;
    char buf[2048];
#ifndef IFACE_SUPPORT_CFG80211
    struct ifreq ifr;
#endif

    /*find parent interface*/
    ifacemgr_ifname_to_parent_ifname(iptr, stavap->ifname, stavap->wifi_ifname);

    if ((IFACE_MODE_FASTLANE == (iptr->mode)) && (iptr->primary_group_idx < 0)) {
        int  is_preferredUplink = 0;
        /*
         * If interface manager is operating in Mode 2, then get the preferredUplink information
         */
#ifndef IFACE_SUPPORT_CFG80211
        os_memset(&ifr, 0, sizeof(ifr));
        os_memcpy(ifr.ifr_name, stavap->wifi_ifname, IFNAMSIZ);
        extended_cmd.cmd = EXTENDED_SUBIOCTL_GET_PREF_UPLINK;
        ifr.ifr_data = (caddr_t) &extended_cmd;
        extended_cmd.data = (caddr_t)&is_preferredUplink;

        if ((ret = ioctl(iptr->ioctl_sock, SIOCGATHEXTENDED, &ifr)) != 0) {
            ifacemgr_printf("ret=%d ioctl SIOCGATHEXTENDED get_pref_Uplink err", ret);
        } else {
            ifacemgr_printf("Preferred Uplink status:%d",is_preferredUplink);
        }
#else
        memset(&extended_cmd.ext_data.iface_config, 0, sizeof(struct iface_config_t));
        extended_cmd.cmd = EXTENDED_SUBIOCTL_GET_PREF_UPLINK;
        extended_cmd.data_len = sizeof(extended_cmd.ext_data.iface_config);
        extended_cmd.ext_data.iface_config.is_preferredUplink = is_preferredUplink;
        if((ret =(wlanIfIfM->ops->getExtended(wlanIfIfM->ctx, stavap->wifi_ifname,
                            &extended_cmd, sizeof(extended_cmd)))) < 0) {
            ifacemgr_printf("ret=%d ioctl SIOCGATHEXTENDED get_pref_Uplink err", ret);
        } else {
            is_preferredUplink = extended_cmd.ext_data.iface_config.is_preferredUplink;
            ifacemgr_printf("%s SIOCGATHEXTENDED Preferred Uplink status:%d", __func__, is_preferredUplink);
        }
#endif

        if (is_preferredUplink) {
            /*Preferred Uplink found ;so no need to loop through all the radios*/
            iptr->primary_group_idx = gptr->group_idx;
            gptr->is_primary_group = 1;
        }
    }
    ifacemgr_get_stavap_mac(iptr, stavap);
    /* Get stavap connection status*/
#ifndef IFACE_SUPPORT_CFG80211
    os_memset(&ifr, 0, sizeof(ifr));
    os_memcpy(ifr.ifr_name, stavap->wifi_ifname, IFNAMSIZ);
    extended_cmd.cmd = EXTENDED_SUBIOCTL_GET_STAVAP_CONNECTION;
    ifr.ifr_data = (caddr_t) &extended_cmd;
    extended_cmd.data = (caddr_t)&stavap->sta_vap_up;
    if ((ret = ioctl(iptr->ioctl_sock, SIOCGATHEXTENDED, &ifr)) != 0) {
        ifacemgr_printf("ret=%d ioctl SIOCGATHEXTENDED getsta_vap_up err", ret);
    } else {
        ifacemgr_printf("initial stavap(%s) connection: %d", stavap->ifname,stavap->sta_vap_up);
    }
#else
    memset(&extended_cmd.ext_data.iface_config, 0, sizeof(struct iface_config_t));
    extended_cmd.cmd = EXTENDED_SUBIOCTL_GET_STAVAP_CONNECTION;
    extended_cmd.data_len = sizeof(extended_cmd.ext_data.iface_config);
    extended_cmd.ext_data.iface_config.stavap_up = stavap->sta_vap_up;
    if ((ret =(wlanIfIfM->ops->getExtended(wlanIfIfM->ctx,
                        stavap->wifi_ifname, &extended_cmd, sizeof(extended_cmd)))) < 0) {
        ifacemgr_printf("ret=%d ioctl SIOCGATHEXTENDED getsta_vap_up err", ret);
    } else {
        stavap->sta_vap_up = extended_cmd.ext_data.iface_config.stavap_up;
        ifacemgr_printf("%s SIOCGATHEXTENDED initial stavap(%s) connection: %d", __func__, stavap->ifname,stavap->sta_vap_up);
    }
#endif

    if (stavap->sta_vap_up) {
        gptr->num_sta_vaps_up ++;
    }
    if (iptr->same_ssid_support) {
        if (stavap->sta_vap_up) {
            ifacemgr_printf("initial stavap(%s) connection: %d, so bring down and up for same ssid support", stavap->ifname,stavap->sta_vap_up);
            ifacemgr_disconnect_stavap(stavap);
            ifacemgr_reconnect_stavap(stavap);
            stavap->sta_vap_up = 0;
            gptr->num_sta_vaps_up --;
        }
    }
    if (IFACE_MODE_GLOBALWDS == (iptr->mode)) {
	if (gptr->num_sta_vaps_up) {
	    if ((gptr->num_sta_vaps_up == 1) && stavap->sta_vap_up) {
		ifacemgr_bringup_apvaps(gptr);
	    }
	}
    }
    if ((IFACE_MODE_FASTLANE == (iptr->mode)) && (iptr->primary_group_idx >= 0)) {
        struct group_data *primary_gptr = NULL;
        struct group_data *secondary_gptr = NULL;

        primary_gptr = &(iptr->group[iptr->primary_group_idx]);
        secondary_gptr = &(iptr->group[!iptr->primary_group_idx]);
        /*
         * Check if the primary group STA VAP is connected.
         * If connected, then bring down the secondary group.
         */
        if(primary_gptr->num_sta_vaps_up) {
            ifacemgr_bringup_apvaps(primary_gptr);
            ifacemgr_bringdown_apvaps(secondary_gptr);
            ifacemgr_bringdown_stavaps(secondary_gptr);
        } else if(secondary_gptr->num_sta_vaps_up) {
            /*
             * Since a secondary group STA VAP is connected,
             * bring up the AP VAPs of secondary group and
             * register a timer call back handler for time out.
             */
            ifacemgr_bringup_apvaps(secondary_gptr);
            ifacemgr_bringdown_apvaps(primary_gptr);
            eloop_register_timeout(iptr->timeout, 0, ifacemgr_primarygrp_conn_timer, primary_gptr, NULL);
        }
    }
    if (IFACE_MODE_GLOBALWDS == (iptr->mode)) {
	if (stavap->priority == LOW_PRIORITY_STAVAP){
            /*Bring down LOW_PRIORITY_STAVAP initially*/
            ifacemgr_bring_updown_stavaps(gptr,0,LOW_PRIORITY_STAVAP);
	}
        if ((stavap->priority == HIGH_PRIORITY_STAVAP) && !stavap->sta_vap_up) {
	    eloop_register_timeout(iptr->stavap_scan_time, 0, ifacemgr_hp_stavap_disconnection, stavap, NULL);
	}
    }
    return 0;
}

static void ifacemgr_wpa_cli_ping(void *eloop_ctx, void *timeout_ctx)
{
    struct sta_vap *stavap = (struct sta_vap *)eloop_ctx;
    struct group_data *gptr = NULL;

    if(NULL == stavap){
        ifacemgr_printf("failed to make sta vap conn with wpa supplicant\n");
        return;
    }

    gptr = stavap->gptr;

    if (stavap->ifacemgr_stavap_conn) {
        int res;
        char buf[4096];
        size_t len;
        len = sizeof(buf) - 1;
        res = wpa_ctrl_request((struct wpa_ctrl *)stavap->ifacemgr_stavap_conn,
                                "PING", strlen("PING"), buf, &len,NULL);

        if (res < 0) {
            ifacemgr_printf("Connection to wpa_supplicant lost - trying to reconnect\n");
            eloop_unregister_read_sock(stavap->ifacemgr_stavap_conn->sock);
            wpa_ctrl_detach((struct wpa_ctrl *)stavap->ifacemgr_stavap_conn);
            wpa_ctrl_close((struct wpa_ctrl *)stavap->ifacemgr_stavap_conn);
            stavap->ifacemgr_stavap_conn = NULL;
        }
    }

    if(!stavap->ifacemgr_stavap_conn) {
        if (stavap->sta_vap_up)
            gptr->num_sta_vaps_up --;
        stavap->sta_vap_up = 0;
        if(ifacemgr_stavap_conn_to_wpa_supplicant(stavap)){
            ifacemgr_stavap_config(stavap);
        }
    }
    eloop_register_timeout(ping_interval, 0, ifacemgr_wpa_cli_ping, stavap, NULL);
}

static void ifacemgr_try_connection(void *eloop_ctx, void *timeout_ctx)
{
    struct sta_vap *stavap = (struct sta_vap *)eloop_ctx;
    int ret;

    if (NULL == stavap){
        ifacemgr_printf("failed to make sta vap conn with wpa supplicant\n");
        return;
    }

    ret = ifacemgr_stavap_conn_to_wpa_supplicant(stavap);
    if (!ret) {
        ifacemgr_printf("failed to make sta vap conn with wpa supplicant\n");
        eloop_register_timeout(1, 0, ifacemgr_try_connection, stavap, NULL);
    } else {
        ifacemgr_stavap_config(stavap);
        eloop_register_timeout(ping_interval, 0, ifacemgr_wpa_cli_ping, stavap, NULL);
    }
}

int
ifacemgr_get_we_version(struct iface_daemon *iptr, char *ifname)
{
#ifndef IFACE_SUPPORT_CFG80211
    struct iw_range *range;
    struct iwreq iwr;
    size_t buflen;

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

    if (ioctl(iptr->ioctl_sock, SIOCGIWRANGE, &iwr) < 0) {
        ifacemgr_printf("ERROR : ioctl[SIOCGIWRANGE]\n");
        os_free(range);
        return -1;
    }
    iptr->we_version = range->we_version_compiled;
    os_free(range);
#else
    int we_version = 0;
    iptr->we_version = 0;
    if (wlanIfIfM->ops->getRange(wlanIfIfM->ctx, ifname, &we_version) < 0) {
        ifacemgr_printf("ERROR : ioctl[SIOCGIWRANGE]\n");
        return -1;
    }
    ifacemgr_printf(" %s range %d \n",__func__,we_version);
    iptr->we_version = we_version;
#endif
    return 0;
}

static void
ifacemgr_check_initial_stavaps_conn(void *eloop_ctx, void *timeout_ctx)
{
    struct group_data *gptr = (struct group_data *)eloop_ctx;

    if (gptr->num_sta_vaps_up) {
        ifacemgr_printf("STAVAPs on up state, don't bring down APVAPs. Num_stavaps_up:%d",gptr->num_sta_vaps_up);
        return;
    }
    ifacemgr_bringdown_apvaps(gptr);
    return;
}

static void
ifacemgr_check_apvap_cac_state(struct group_data *gptr)
{
#ifdef IFACE_SUPPORT_CFG80211
    struct iface_daemon *iptr = gptr->iptr;
    int cac_state  = 0;
    char parent[IFNAMSIZ];
    struct extended_ioctl_wrapper extended_cmd;
    int i, ret;

    for (i=0;i<gptr->num_ap_vaps;i++) {
        /* Get CAC state*/
        cac_state  = 0;
        /*find parent interface*/
        ifacemgr_ifname_to_parent_ifname(iptr, gptr->ap_vap[i], parent);

        memset(&extended_cmd.ext_data.iface_config, 0, sizeof(struct iface_config_t));
        extended_cmd.cmd = EXTENDED_SUBIOCTL_GET_CAC_STATE;
        extended_cmd.data_len = sizeof(extended_cmd.ext_data.iface_config);
        extended_cmd.ext_data.iface_config.cac_state = cac_state;
        if((ret =(wlanIfIfM->ops->getExtended(wlanIfIfM->ctx,
                            parent, &extended_cmd, sizeof(extended_cmd)))) < 0) {
            ifacemgr_printf("%s ret=%d SIOCGATHEXTENDED get cac_state err", __func__, ret);
        }
        cac_state = extended_cmd.ext_data.iface_config.cac_state;
        gptr->ap_vap_cac_state[i] = cac_state;
        ifacemgr_printf("Checking APVAP cac state vap:%s cac_state:%d",gptr->ap_vap[i],gptr->ap_vap_cac_state[i]);
    }
#endif
}

#define ACS_SCAN_DELAY 15

int
ifacemgr_update_ifacemgr_handle_config(ifacemgr_hdl_t *ifacemgr_handle)
{
    struct iface_daemon *iptr = (struct iface_daemon *)ifacemgr_handle;
    struct group_data *gptr = NULL;
    int i = 0, j = 0;
    struct sta_vap *stavap = NULL;
    char *wpa_s_conf_file = "/var/run/wpa_supplicant";
    char *cfile;
    int flen;
    int  max_radio_cnt = (IFACE_MODE_FASTLANE != (iptr->mode)) ? MAX_RADIOS_CNT : MAX_RADIOS_CNT_FAST_LANE;

    //set primary to -1 for error checking
    iptr->primary_group_idx = -1;

    for (i = 0; i < max_radio_cnt; i++) {
        gptr =  &(iptr->group[i]);
        gptr->iptr = iptr;
        gptr->group_idx = i;
        for (j = 0; j < gptr->num_sta_vaps; j++) {

            if(gptr->stavap[j] == NULL) {
                ifacemgr_printf("Failed to connect to MPSTA wpa_s - iptr->group[0].sta_vap[0] == NULL");
                continue;
            }
            stavap = gptr->stavap[j];
            stavap->gptr = gptr;
            flen = strlen(wpa_s_conf_file) + (strlen(stavap->ifname)) + 7;
            cfile = os_malloc(flen);
            if (cfile == NULL)
            {
                ifacemgr_printf("ERROR:malloc failed\n");
                return 0;
            }

            snprintf(cfile, flen, "%s-%s.conf", wpa_s_conf_file, stavap->ifname);

            stavap->wpa_conf_file = os_strdup(cfile);
            os_free(cfile);
            eloop_register_timeout(0, 0, ifacemgr_try_connection, stavap, NULL);
            if (iptr->we_version == 0) {
                if(ifacemgr_get_we_version(iptr, stavap->ifname) != 0) {
                    ifacemgr_printf("Failed to get we version\n");
                }
            }
        }
        if((IFACE_MODE_GLOBALWDS == (iptr->mode)) && gptr->num_sta_vaps) {
            /*
             * In case of AUTO channel, ACS scan takes additional time to bringup AP VAP
             * So, bringdown APVAPs after some delay in order to set correct channel for AP VAP
             */
            eloop_register_timeout(ACS_SCAN_DELAY, 0, ifacemgr_check_initial_stavaps_conn, gptr, NULL);
            ifacemgr_check_apvap_cac_state(gptr);
        }
    }
    return 1;
}

static
void ifacemgr_hyd_event_receive(int sock, void *eloop_ctx, void *sock_ctx)
{
    char msg[256];
    int res;
    struct sockaddr_un from;
    socklen_t fromlen = sizeof(from);
    struct ifacemgr_event_message *message;
    struct plc_iface *plciface = (struct plc_iface *)eloop_ctx;
    struct group_data *gptr = plciface->gptr;

    res = recvfrom(sock, msg, sizeof(msg) - 1, MSG_DONTWAIT, (struct sockaddr *) &from, &fromlen);
    if (res < 0) {
	ifacemgr_printf("recvfrom Err no=%d", errno);
	return;
    }
    msg[res] = '\0';

    message = (struct ifacemgr_event_message *)msg;

    switch (message->cmd) {
	case HYD_BACK_HAUL_IFACE_PLC_UP:
            if (plciface->plc_iface_up == 1) {
                return;
            }
	    plciface->plc_iface_up = 1;
	    gptr->num_plc_iface_up++;
	    ifacemgr_printf("HYD_BACK_HAUL_IFACE_PLC_UP event received, gptr->num_plc_iface_up:%d", gptr->num_plc_iface_up);
	    ifacemgr_backhaul_up_down_event_process(gptr, 1);
	    break;
	case HYD_BACK_HAUL_IFACE_PLC_DOWN:
            if (plciface->plc_iface_up == 0) {
                return;
            }
	    plciface->plc_iface_up = 0;
	    gptr->num_plc_iface_up--;
	    ifacemgr_printf("HYD_BACK_HAUL_IFACE_PLC_DOWN event received, gptr->num_plc_iface_up:%d", gptr->num_plc_iface_up);
	    ifacemgr_backhaul_up_down_event_process(gptr, 0);
	    break;
	default:
	    break;
    }
    return;
}

int
ifacemgr_create_hydsock(ifacemgr_hdl_t *ifacemgr_handle)
{
    struct iface_daemon *iptr = (struct iface_daemon *)ifacemgr_handle;
    struct group_data *gptr = NULL;
    int i = 0, j = 0;
    struct plc_iface *plciface = NULL;
    int  max_radio_cnt = MAX_RADIOS_CNT;
    int32_t sock;
    int32_t plcsock_len;
    struct sockaddr_un clientAddr = {
                AF_UNIX,
                HYD_IFACE_MGR_SOCKET_CLIENT
    };


    for (i = 0; i < max_radio_cnt; i++) {
	gptr =  &(iptr->group[i]);
	gptr->iptr = iptr;
	gptr->group_idx = i;
        gptr->num_plc_iface_up = 0;
	for (j = 0; j < gptr->num_plc_iface; j++) {

	    if(gptr->plciface[j] == NULL) {
		ifacemgr_printf("Failed to connect to hyd - iptr->group[0].plciface[0] == NULL");
		continue;
	    }
	    plciface = gptr->plciface[j];
	    plciface->gptr = gptr;
            plciface->plc_iface_up = 0;
	    if ((sock = socket (AF_UNIX, SOCK_DGRAM, 0)) == -1)
	    {
		ifacemgr_printf("%s:Socket() failed. Err no=%d", __func__, errno);
		goto err;
	    }
	    memset(&clientAddr, 0, sizeof(clientAddr));
	    clientAddr.sun_family = AF_UNIX;
	    strlcpy(clientAddr.sun_path, HYD_IFACE_MGR_SOCKET_CLIENT, sizeof(HYD_IFACE_MGR_SOCKET_CLIENT));
	    plcsock_len = strlen(HYD_IFACE_MGR_SOCKET_CLIENT);

	    clientAddr.sun_path[plcsock_len] = '\0';
	    unlink(clientAddr.sun_path);
	    if (bind (sock, (struct sockaddr *)(&clientAddr), sizeof (clientAddr)) == -1)
	    {
		ifacemgr_printf("%s:Bind() failed. Err no=%d", __func__, errno);
		close(sock);
		goto err;
	    }

	    /* Set nonblock. */
	    if (fcntl(sock, F_SETFL, fcntl(sock, F_GETFL) | O_NONBLOCK))
	    {
		ifacemgr_printf("%s failed to set fd NONBLOCK", __func__);
		goto err;
	    }

	    plciface->hyd_sock = sock;

	    if (eloop_register_read_sock(sock, ifacemgr_hyd_event_receive, plciface, NULL)) {
		ifacemgr_printf("%s failed to register callback func", __func__);
		close(sock);
		goto err;
	    }

	}
    }
    return 1;
err:
    return 0;
}

static void
ifacemgr_ifindex_to_ifname(struct iface_daemon *iptr, int index, char *ifname)
{
    struct ifreq ifr;

    os_memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_ifindex = index;
    if (ioctl(iptr->ioctl_sock, SIOCGIFNAME, &ifr) != 0) {
        return;
    }
    os_memcpy(ifname, ifr.ifr_name, IFNAMSIZ);
}

int
ifacemgr_validate_sender_ifname(struct iface_daemon *iptr, char *ifname)
{
    struct group_data *gptr = NULL;
    int i = 0, j = 0;
    struct sta_vap *stavap = NULL;
    for (i = 0; i < MAX_RADIOS_CNT; i++) {
        gptr =  &(iptr->group[i]);
        for (j = 0; j < gptr->num_sta_vaps; j++) {
            if(gptr->stavap[j] == NULL) {
                continue;
            }
            stavap = gptr->stavap[j];
            if(os_memcmp(stavap->ifname, ifname, os_strlen(ifname)) == 0){
                return 1;
            }
        }
    }
    return 0;
}

#define IFACEMGR_MAX_CMD_LEN    128

static void
ifacemgr_update_wpas_bssid_config(struct sta_vap *stavap, const u8 *addr)
{
    char buf[2048];
    size_t len = sizeof(buf);
    int ret;
    char cmd[IFACEMGR_MAX_CMD_LEN] = {0};

    if (!stavap->ifacemgr_stavap_conn) {
        printf("stavap not connected to supplicant stavap:%p\n",stavap);
        return;
    }

    ret = snprintf(cmd, sizeof(cmd), "BSSID 0 %02x:%02x:%02x:%02x:%02x:%02x", addr[0],addr[1],addr[2],addr[3],addr[4],addr[5]);

    if (ret < 0 || ret >= sizeof(cmd)) {
        printf("ifacemgr_update_wpas_bssid_config cmd failure\n");
        return;
    }
    cmd[sizeof(cmd) - 1] = '\0';

    ret = wpa_ctrl_request((struct wpa_ctrl *)stavap->ifacemgr_stavap_conn, cmd, os_strlen(cmd), buf, &len, NULL);
    if (ret < 0) {
        printf("ifacemgr_update_wpas_bssid_config request failure\n");
        return;
    }
    buf[len] = '\0';
    return;
}

struct sta_vap *wifi_ifname_to_vap_struct(struct iface_daemon *iptr, char *ifname)
{
    struct group_data *gptr = NULL;
    int i = 0, j = 0;
    struct sta_vap *stavap = NULL;

    for (i = 0; i < MAX_RADIOS_CNT; i++) {
        gptr =  &(iptr->group[i]);
        for (j = 0; j < gptr->num_sta_vaps; j++) {
            if(gptr->stavap[j] == NULL) {
                continue;
            }
            stavap = gptr->stavap[j];
            if(os_memcmp(stavap->ifname, ifname, os_strlen(ifname)) == 0){
                return stavap;
            }
        }
    }
    return NULL;
}

#ifdef IFACE_SUPPORT_CFG80211
struct group_data *apvap_to_gptr(struct iface_daemon *iptr, char *apvap, uint8_t *cnt)
{
    struct group_data *gptr = NULL;
    uint8_t i,j;

    for (i = 0; i < MAX_RADIOS_CNT; i++) {
        gptr =  &(iptr->group[i]);
        for (j = 0; j < gptr->num_ap_vaps; j++) {
            if(gptr->ap_vap[j] == NULL) {
                continue;
            }
            if(os_memcmp(gptr->ap_vap[j], apvap, os_strlen(apvap)) == 0){
                *cnt = j;
                return gptr;
            }
        }
    }
    return NULL;
}
#endif

static void
ifacemgr_wireless_event_wireless(struct iface_daemon *iptr, struct ifinfomsg *ifi,
                                                       char *data, int len)
{
    struct iw_event iwe_buf, *iwe = &iwe_buf;
    char *pos, *end, *custom;
    char child[IFNAMSIZ] = {0};
    struct ev_msg *msg;
    int ret = 0;
    struct sta_vap *stavap = NULL;
#ifdef IFACE_SUPPORT_CFG80211
    struct group_data *gptr = NULL;
    uint8_t cnt=0;
    char cmd[IFACEMGR_MAX_CMD_LEN] = {0};
    size_t res;
#endif

    ifacemgr_ifindex_to_ifname(iptr, ifi->ifi_index, child);


    pos = data;
    end = data + len;

    while (pos + IW_EV_LCP_LEN <= end) { //+4
        os_memcpy(&iwe_buf, pos, IW_EV_LCP_LEN);
        if (iwe->len <= IW_EV_LCP_LEN)
            return;

        custom = pos + IW_EV_POINT_LEN;
        if(iptr->we_version > 18 && iwe->cmd == IWEVCUSTOM)
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
        switch (iwe->cmd) {
        case SIOCGIWAP:
            {
                ret = ifacemgr_validate_sender_ifname(iptr, child);
                if (ret) {
                    if (is_zero_mac_addr((const u8 *) iwe->u.ap_addr.sa_data) ||
                        os_memcmp(iwe->u.ap_addr.sa_data, "\x44\x44\x44\x44\x44\x44", ETH_ALEN) == 0) {
                        eloop_cancel_timeout(ifacemgr_poll_stavap_status, iptr, NULL);
                        eloop_register_timeout(PERIODIC_POLL_TIME, 0, ifacemgr_poll_stavap_status, iptr, NULL);
                    }
                }
            }
            break;
        case IWEVCUSTOM:
            {
                u_int16_t opcode = iwe->u.data.flags;
                if (iptr->same_ssid_support) {
                    stavap = wifi_ifname_to_vap_struct(iptr, child);
                    if ((stavap) && (opcode == IEEE80211_EV_PREFERRED_BSSID)) {
                        msg = (struct ev_msg *)custom;
                        ifacemgr_printf("vap:%s Recvd PREFERRED_BSSID event MAC:0x%x.%x.%x.%x.%x.%x\n",stavap->ifname,msg->addr[0],msg->addr[1],msg->addr[2],msg->addr[3],msg->addr[4],msg->addr[5]);
                        ifacemgr_update_wpas_bssid_config(stavap, msg->addr);
                    }
                }
#ifdef IFACE_SUPPORT_CFG80211
                if (IFACE_MODE_GLOBALWDS == (iptr->mode) && (opcode == IEEE80211_EV_CAC_EXPIRED)) {
                    ifacemgr_printf("cac expired for vap:%s\n",(char *)custom);
                    gptr = apvap_to_gptr(iptr, (char *) custom ,&cnt);
                    if (gptr && (gptr->ap_vap_cac_state[cnt] == 1)) {
                         gptr->ap_vap_cac_state[cnt] = 0;
                         if (gptr->num_sta_vaps_up == 0)
                         {
                             /*Bring down this AP VAP*/
                             ifacemgr_printf("Bringing down AP VAPs(%s)",gptr->ap_vap[cnt]);
                             res = snprintf(cmd, sizeof(cmd), "ifconfig %s down", gptr->ap_vap[cnt]);
                             if (res < 0 || res >= sizeof(cmd))
                                 return;

                             cmd[sizeof(cmd) - 1] = '\0';
                             system(cmd);

                         }
                    }
                }
#endif
            }
            break;
        }

        pos += iwe->len;
    }
}

static void
ifacemgr_event_rtm_newlink(void *ctx, struct ifinfomsg *ifi, u8 *buf, size_t len)
{
    struct iface_daemon *iptr = ctx;
    int attrlen, rta_len;
    struct rtattr *attr;

    attrlen = len;
    attr = (struct rtattr *) buf;

    rta_len = RTA_ALIGN(sizeof(struct rtattr));
    while (RTA_OK(attr, attrlen)) {
        if (attr->rta_type == IFLA_WIRELESS) {
            ifacemgr_wireless_event_wireless(iptr, ifi, ((char *) attr) + rta_len,
                attr->rta_len - rta_len);
        }
        attr = RTA_NEXT(attr, attrlen);
    }

}

int
ifacemgr_create_netlink_socket(ifacemgr_hdl_t *ifacemgr_handle)
{
    struct iface_daemon *iptr = (struct iface_daemon *)ifacemgr_handle;
    struct netlink_config *cfg;

    cfg = os_zalloc(sizeof(*cfg));
    if (cfg == NULL) {
        goto err;
    }

    cfg->ctx = iptr;
    cfg->newlink_cb = ifacemgr_event_rtm_newlink;

    iptr->netlink = netlink_init(cfg);
    if (iptr->netlink == NULL) {
        goto err1;
    }
    return 1;
err1:
    os_free(cfg);
err:
    return 0;
}
