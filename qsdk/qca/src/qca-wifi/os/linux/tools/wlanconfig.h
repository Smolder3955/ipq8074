/*
 * Copyright (c) 2017-2018 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 */

#ifndef _WLANCONFIG_H_
#define _WLANCONFIG_H_

#if UMAC_SUPPORT_CFG80211
#ifndef BUILD_X86
#include <netlink/genl/genl.h>
#endif
#endif
#include <netinet/in.h>
#include <arpa/inet.h>
#include <qcatools_lib.h>
#ifdef ANDROID
#include <compat.h>
#endif

#include <ieee80211_external.h>

#define PS_ACTIVITY  1<<0
#define HUMAN_FORMAT 1<<1
#define ALLCHANS     1<<2
#define LIST_STATION_CFG_ALLOC_SIZE 3*1024
#define LIST_STATION_IOC_ALLOC_SIZE 0xFFFF

#define WLANCONFIG_NL80211_CMD_SOCK_ID DEFAULT_NL80211_CMD_SOCK_ID
#define WLANCONFIG_NL80211_EVENT_SOCK_ID DEFAULT_NL80211_EVENT_SOCK_ID

#define	IEEE80211_C_BITS \
    "\020\1WEP\2TKIP\3AES\4AES_CCM\6CKIP\7FF\10TURBOP\11IBSS\12PMGT\13HOSTAP\14AHDEMO" \
"\15SWRETRY\16TXPMGT\17SHSLOT\20SHPREAMBLE\21MONITOR\22TKIPMIC\30WPA1" \
"\31WPA2\32BURST\33WME"

/*
 * These are taken from ieee80211_node.h
 */

#define IEEE80211_NODE_TURBOP   0x0001		/* Turbo prime enable */
#define IEEE80211_NODE_AR       0x0010		/* AR capable */
#define IEEE80211_NODE_BOOST    0x0080
#define MACSTR_LEN 18
#define IEEE80211_MAX_2G_CHANNEL 14

void usage(void);

int handle_command_create  (int argc, char *argv[], const char *ifname, struct socket_context *sock_ctx);
int handle_command_destroy (int argc, char *argv[], const char *ifname, struct socket_context *sock_ctx);
int handle_command_list    (int argc, char *argv[], const char *ifname, struct socket_context *sock_ctx);
int handle_command_nawds   (int argc, char *argv[], const char *ifname, struct socket_context *sock_ctx);
int handle_command_hmwds   (int argc, char *argv[], const char *ifname, struct socket_context *sock_ctx);
int handle_command_ald     (int argc, char *argv[], const char *ifname, struct socket_context *sock_ctx);
int handle_command_wnm     (int argc, char *argv[], const char *ifname, struct socket_context *sock_ctx);
int handle_command_hmmc    (int argc, char *argv[], const char *ifname, struct socket_context *sock_ctx);
int handle_command_vendorie(int argc, char *argv[], const char *ifname, struct socket_context *sock_ctx);
int handle_command_addie   (int argc, char *argv[], const char *ifname, struct socket_context *sock_ctx);
int handle_command_atfstat (int argc, char *argv[], const char *ifname, struct socket_context *sock_ctx);
int handle_command_nac     (int argc, char *argv[], const char *ifname, struct socket_context *sock_ctx);
int handle_command_nac_rssi(int argc, char *argv[], const char *ifname, struct socket_context *sock_ctx);
int handle_command_atf     (int argc, char *argv[], const char *ifname, struct socket_context *sock_ctx);
int handle_command_cfr     (int argc, char *argv[], const char *ifname, struct socket_context *sock_ctx);

#ifdef ATH_BUS_PM
static int suspend(const char *ifname, int suspend);
#endif

typedef void (*cfg80211_cb) (struct cfg80211_data *buffer);

#endif
