/*
 * Wigig AP Test Tool
 *
 * Copyright (c) 2016 Qualcomm Atheros, Inc.
 *
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary
 *
 * Copyright (c) 2002-2015, Jouni Malinen <j@w1.fi>
 * Copyright (c) 2003-2004, Instant802 Networks, Inc.
 * Copyright (c) 2005-2006, Devicescape Software, Inc.
 * Copyright (c) 2007, Johannes Berg <johannes@sipsolutions.net>
 * Copyright (c) 2009-2010, Atheros Communications
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 */

#include <stdio.h>
#include <stdarg.h>
#include <netlink/netlink.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <net/if.h>
#include <errno.h>
#include <sys/socket.h>
#include <linux/nl80211.h>
#include <linux/if_ether.h>
#include <getopt.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <time.h>

#ifndef SOL_NETLINK
#define SOL_NETLINK	270
#endif

#define IEEE80211_FCTL_FTYPE		0x000c
#define IEEE80211_FCTL_STYPE		0x00f0
#define WLAN_FC_GET_TYPE(fc)	(((fc) & IEEE80211_FCTL_FTYPE) >> 2)
#define WLAN_FC_GET_STYPE(fc)	(((fc) & IEEE80211_FCTL_STYPE) >> 4)

#define WLAN_FC_TYPE_MGMT		0
/* management */
#define WLAN_FC_STYPE_ASSOC_REQ		0
#define WLAN_FC_STYPE_ASSOC_RESP	1
#define WLAN_FC_STYPE_REASSOC_REQ	2
#define WLAN_FC_STYPE_REASSOC_RESP	3
#define WLAN_FC_STYPE_PROBE_REQ		4
#define WLAN_FC_STYPE_PROBE_RESP	5
#define WLAN_FC_STYPE_BEACON		8
#define WLAN_FC_STYPE_ATIM		9
#define WLAN_FC_STYPE_DISASSOC		10
#define WLAN_FC_STYPE_AUTH		11
#define WLAN_FC_STYPE_DEAUTH		12
#define WLAN_FC_STYPE_ACTION		13

#define WLAN_REASON_UNSPECIFIED 1
#define WLAN_REASON_DISASSOC_STA_HAS_LEFT 8
#define WLAN_STATUS_UNSPECIFIED_FAILURE 1

#define WLAN_EID_SSID 0

#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"

#define le_to_host16(n) ((u16) (le16) (n))
#define BIT(x) (1 << x)

#define STRUCT_PACKED __attribute__ ((packed))
#ifndef offsetof
#define offsetof(type, member) ((long) &((type *) 0)->member)
#endif

#define WIGIGAP_DEFAULT_SSID "11ad"
#define WIGIGAP_DEFAULT_FREQ 60480
#define WIGIGAP_MAX_STA 8

typedef unsigned char u8;
typedef unsigned short u16;
typedef int s32;
typedef u16 le16;

struct ieee80211_mgmt {
	le16 frame_control;
	le16 duration;
	u8 da[6];
	u8 sa[6];
	u8 bssid[6];
	le16 seq_ctrl;
	union {
		u8 variable[0];
		struct {
			u8 timestamp[8];
			le16 beacon_int;
			le16 capab_info;
			/* followed by some of SSID, Supported rates,
			* FH Params, DS Params, CF Params, IBSS Params, TIM */
			u8 variable[];
		} STRUCT_PACKED beacon;
		struct {
			u8 timestamp[8];
			le16 beacon_int;
			le16 capab_info;
			/* followed by some of SSID, Supported rates,
			* FH Params, DS Params, CF Params, IBSS Params */
			u8 variable[];
		} STRUCT_PACKED probe_resp;
		struct {
			le16 capab_info;
			le16 status_code;
			le16 aid;
			/* followed by Supported rates */
			u8 variable[0];
		} STRUCT_PACKED assoc_resp, reassoc_resp;
		struct {
			le16 reason_code;
		} STRUCT_PACKED disassoc;
	};
} STRUCT_PACKED;

struct wigigap_sta_info {
	bool valid;
	u8 mac[ETH_ALEN];
	int aid;
};

/* application state, passed to callback handlers */
struct wigigap_state {
	/* callbacks handle for synchronous NL commands */
	struct nl_cb *cb;

	/* nl socket handle for synchronous NL commands */
	struct nl_sock *nl;

	/* nl socket handler for events */
	struct nl_sock *nl_event;

	/* family id for nl80211 events */
	int nl80211_id;

	/* interface index being used */
	int ifindex;

	u8 own_mac[ETH_ALEN];
	bool device_ap_sme_disabled;
	int last_aid;
	struct wigigap_sta_info sta[WIGIGAP_MAX_STA];
	int assoc_iterations;
	int delay_assoc;
	bool local_disassoc;
	bool reject_assoc;
	int delay_sta;
} g_state;

int terminate = 0;

static int _log_printf(const char *fmt, ...)
{
	int ret;
	va_list ap;

	va_start(ap, fmt);
	ret = vfprintf(stdout, fmt, ap);
	va_end(ap);
	fflush(stdout);
	return (ret);
}

#define log_printf(fmt, arg...) \
do { \
	struct timeval tv = {0}; \
	gettimeofday(&tv, NULL); \
	_log_printf("%ld.%06u: %s: " fmt "\n", (long)tv.tv_sec, (unsigned int)tv.tv_usec, __func__, ##arg); \
} while (0)

static void log_hexdump(const char *title, const u8 *buf, size_t len)
{
	size_t i;
	struct timeval tv = {0};

	gettimeofday(&tv, NULL);
	printf("%ld.%06u: %s - hexdump(len=%lu):", (long)tv.tv_sec,
	       (unsigned int)tv.tv_usec, title, (unsigned long)len);
	if (buf == NULL)
		printf(" [NULL]");
	else
		for (i = 0; i < len; i++)
			printf(" %02x", buf[i]);

	_log_printf("\n");
}

/**
 * nl callback handler for disabling sequence number checking
 */
static int nl_no_seq_check(struct nl_msg *msg, void *arg)
{
	return NL_OK;
}

/**
 * nl callback handler called on error
 */
static int nl_error_handler(struct sockaddr_nl *nla, struct nlmsgerr *err, void *arg)
{
	int *ret = (int *)arg;
	*ret = err->error;
	log_printf("error handler with error: %d", *ret);
	return NL_SKIP;
}

/**
 * nl callback handler called after all messages in
 * a multi-message reply are delivered. It is used
 * to detect end of synchronous command/reply
 */
static int nl_finish_handler(struct nl_msg *msg, void *arg)
{
	int *ret = (int *)arg;
	*ret = 0;
	return NL_SKIP;
}

/**
 * nl callback handler called when ACK is received
 * for a command. It is also used to detect end of
 * synchronous command/reply
 */
static int nl_ack_handler(struct nl_msg *msg, void *arg)
{
	int *err = (int *)arg;
	*err = 0;
	return NL_STOP;
}

/**
 * allocate an nl_msg for sending a command
 */
static struct nl_msg *allocate_nl_cmd_msg(int family, int ifindex, int flags,
					  uint8_t cmd)
{
	struct nl_msg *msg = nlmsg_alloc();

	if (!msg) {
		log_printf("failed to allocate nl msg");
		return NULL;
	}

	if (!genlmsg_put(msg,
			 0, // pid (automatic)
			 0, // sequence number (automatic)
			 family, // family
			 0, // user specific header length
			 flags, // flags
			 cmd, // command
			 0) // protocol version
		) {
		log_printf("failed to init msg");
		nlmsg_free(msg);
		return NULL;
	}

	if (nla_put_u32(msg, NL80211_ATTR_IFINDEX, (uint32_t)ifindex)) {
		log_printf("failed to set interface index");
		nlmsg_free(msg);
		return NULL;
	}

	return msg;
}

/**
 * send NL command and receive reply synchronously
 */
static int nl_cmd_send_and_recv(struct nl_sock *nl, struct nl_cb *cb,
				struct nl_msg *msg,
				int(*valid_handler)(struct nl_msg *, void *),
				void *valid_data)
{
	struct nl_cb *cb_clone = nl_cb_clone(cb);
	int err;

	if (!cb_clone) {
		log_printf("failed to allocate cb_clone");
		return -ENOMEM;
	}

	err = nl_send_auto_complete(nl, msg);
	if (err < 0) {
		log_printf("failed to send message, err %d", err);
		nl_cb_put(cb_clone);
		return -ENOMEM;
	}

	err = 1;
	nl_cb_err(cb_clone, NL_CB_CUSTOM, nl_error_handler, &err);
	nl_cb_set(cb_clone, NL_CB_FINISH, NL_CB_CUSTOM, nl_finish_handler, &err);
	nl_cb_set(cb_clone, NL_CB_ACK, NL_CB_CUSTOM, nl_ack_handler, &err);
	if (valid_handler)
		nl_cb_set(cb_clone, NL_CB_VALID, NL_CB_CUSTOM, valid_handler,
			  valid_data);

	while (err > 0) {
		int res = nl_recvmsgs(nl, cb_clone);

		if (res < 0) {
			log_printf("nl_recvmsgs failed: %d", res);
			/* TODO should we exit the loop here?
			   similar code in supplicant does not */
		}
	}

	nl_cb_put(cb_clone);
	return err;
}

static const char *nl80211_command_to_string(enum nl80211_commands cmd)
{
#define C2S(x) case x: return #x;
	switch (cmd) {
	C2S(NL80211_CMD_UNSPEC)
	C2S(NL80211_CMD_GET_INTERFACE)
	C2S(NL80211_CMD_SET_INTERFACE)
	C2S(NL80211_CMD_START_AP)
	C2S(NL80211_CMD_STOP_AP)
	C2S(NL80211_CMD_GET_STATION)
	C2S(NL80211_CMD_SET_STATION)
	C2S(NL80211_CMD_NEW_STATION)
	C2S(NL80211_CMD_DEL_STATION)
	C2S(NL80211_CMD_ASSOCIATE)
	C2S(NL80211_CMD_DEAUTHENTICATE)
	C2S(NL80211_CMD_DISASSOCIATE)
	C2S(NL80211_CMD_DISCONNECT)
	C2S(NL80211_CMD_REGISTER_FRAME)
	C2S(NL80211_CMD_FRAME)
	C2S(NL80211_CMD_FRAME_TX_STATUS)
	default:
		return "NL80211_CMD_UNKNOWN";
	}
#undef C2S
}

static const char *fc2str(u16 fc)
{
	u16 stype = WLAN_FC_GET_STYPE(fc);
#define C2S(x) case x: return #x;

	switch (WLAN_FC_GET_TYPE(fc)) {
	case WLAN_FC_TYPE_MGMT:
		switch (stype) {
		C2S(WLAN_FC_STYPE_ASSOC_REQ)
		C2S(WLAN_FC_STYPE_ASSOC_RESP)
		C2S(WLAN_FC_STYPE_REASSOC_REQ)
		C2S(WLAN_FC_STYPE_REASSOC_RESP)
		C2S(WLAN_FC_STYPE_PROBE_REQ)
		C2S(WLAN_FC_STYPE_PROBE_RESP)
		C2S(WLAN_FC_STYPE_BEACON)
		C2S(WLAN_FC_STYPE_ATIM)
		C2S(WLAN_FC_STYPE_DISASSOC)
		C2S(WLAN_FC_STYPE_AUTH)
		C2S(WLAN_FC_STYPE_DEAUTH)
		C2S(WLAN_FC_STYPE_ACTION)
		}
		break;
	}
	return "WLAN_FC_TYPE_UNKNOWN";
#undef C2S
}

static int wigigap_find_sta(struct wigigap_state *state, const u8 *mac)
{
	int i;

	for (i = 0; i < WIGIGAP_MAX_STA; ++i)
		if (state->sta[i].valid &&
		    !memcmp(state->sta[i].mac, mac, ETH_ALEN))
			return i;

	return -1;
}

static void wigigap_free_sta(struct wigigap_state *state, const u8 *mac)
{
	int i;

	for (i = 0; i < WIGIGAP_MAX_STA; ++i)
		if (state->sta[i].valid &&
		    !memcmp(state->sta[i].mac, mac, ETH_ALEN)) {
			state->sta[i].valid = false;
			return;
		}
}

static int wigigap_alloc_sta(struct wigigap_state *state, const u8 *mac)
{
	int i;

	wigigap_free_sta(state, mac);

	for (i = 0; i < WIGIGAP_MAX_STA; ++i)
		if (!state->sta[i].valid) {
			state->sta[i].valid = true;
			memcpy(state->sta[i].mac, mac, ETH_ALEN);
			return i;
		}

	return -1;
}

static int wigigap_send_frame_cmd(struct wigigap_state *state, const u8 *buf,
				  size_t buf_len)
{
	struct nl_msg *msg;
	int rc = 0;

	log_printf("sending");
	log_hexdump("CMD_FRAME", buf, buf_len);
	msg = allocate_nl_cmd_msg(state->nl80211_id, state->ifindex, 0,
				  NL80211_CMD_FRAME);
	if (msg == NULL) {
		log_printf("failed to allocate msg");
		return -ENOMEM;
	}

	if (nla_put(msg, NL80211_ATTR_FRAME, buf_len, buf)) {
		log_printf("failed to set params");
		rc = -ENOBUFS;
		goto out;
	}

	rc = nl_cmd_send_and_recv(state->nl, state->cb, msg, NULL, NULL);
	if (rc) {
		log_printf("failed to send command, %s (%d)", strerror(-rc), rc);
		goto out;
	}

	log_printf("succeeded");

out:
	nlmsg_free(msg);
	return rc;
}

static void wigigap_delay_ms(int delay)
{
	struct timespec ts;
	ts.tv_sec = delay / 1000;
	ts.tv_nsec = (delay % 1000) * 1000000;
	nanosleep(&ts, NULL);
}

static int wigigap_send_new_sta(struct wigigap_state *state, const u8 *mac,
				u16 aid)
{
	int rc = 0;
	struct nl_msg *msg;

	if (state->delay_sta == -1)
		return 0;
	if (state->delay_sta)
		wigigap_delay_ms(state->delay_sta);

	log_printf(MACSTR ", aid=%d", MAC2STR(mac), aid);

	msg = allocate_nl_cmd_msg(state->nl80211_id, state->ifindex, 0,
				  NL80211_CMD_NEW_STATION);
	if (msg == NULL) {
		log_printf("failed to allocate msg");
		rc = -ENOMEM;
		goto out;
	}

	if (nla_put(msg, NL80211_ATTR_MAC, ETH_ALEN, mac) ||
	    nla_put_u16(msg, NL80211_ATTR_STA_AID, aid) ||
	    nla_put_u16(msg, NL80211_ATTR_STA_LISTEN_INTERVAL, 0) ||
	    nla_put(msg, NL80211_ATTR_STA_SUPPORTED_RATES, 0, NULL)) {
		rc = -ENOBUFS;
		goto out;
	}

	rc = nl_cmd_send_and_recv(state->nl, state->cb, msg, NULL, NULL);
	if (rc) {
		log_printf("failed to send command, %s (%d)",
			   strerror(-rc), rc);
		goto out;
	}

	log_printf("succeeded");

out:
	nlmsg_free(msg);
	return rc;
}

static int wigigap_send_set_sta(struct wigigap_state *state, const u8 *mac,
				bool allowed)
{
	int rc = 0;
	struct nl_msg *msg;
	struct nl80211_sta_flag_update upd = {0};

	log_printf(MACSTR ", data %sallowed",
		   MAC2STR(mac), allowed ? "" : "not ");

	msg = allocate_nl_cmd_msg(state->nl80211_id, state->ifindex, 0,
				  NL80211_CMD_SET_STATION);
	if (msg == NULL) {
		log_printf("failed to allocate msg");
		rc = -ENOMEM;
		goto out;
	}

	upd.mask = BIT(NL80211_STA_FLAG_AUTHORIZED);
	upd.set = allowed ? BIT(NL80211_STA_FLAG_AUTHORIZED) : 0;

	if (nla_put(msg, NL80211_ATTR_MAC, ETH_ALEN, mac) ||
	    nla_put(msg, NL80211_ATTR_STA_FLAGS2, sizeof(upd), &upd)) {
		rc = -ENOBUFS;
		goto out;
	}

	rc = nl_cmd_send_and_recv(state->nl, state->cb, msg, NULL, NULL);
	if (rc) {
		log_printf("failed to send command, %s (%d)",
			   strerror(-rc), rc);
		goto out;
	}

	log_printf("succeeded");

out:
	nlmsg_free(msg);
	return rc;
}

static int wigigap_send_del_sta(struct wigigap_state *state, const u8 *mac,
				u16 reason_code)
{
	int rc = 0;
	struct nl_msg *msg;

	if (state->delay_sta == -1)
		return 0;
	if (state->delay_sta)
		wigigap_delay_ms(state->delay_sta);

	log_printf(MACSTR ", reason %d", MAC2STR(mac), reason_code);

	msg = allocate_nl_cmd_msg(state->nl80211_id, state->ifindex, 0,
				  NL80211_CMD_DEL_STATION);
	if (msg == NULL) {
		log_printf("failed to allocate msg");
		rc = -ENOMEM;
		goto out;
	}

	if (nla_put(msg, NL80211_ATTR_MAC, ETH_ALEN, mac) ||
	    nla_put_u16(msg, NL80211_ATTR_REASON_CODE, reason_code)) {
		rc = -ENOBUFS;
		goto out;
	}

	rc = nl_cmd_send_and_recv(state->nl, state->cb, msg, NULL, NULL);
	if (rc) {
		log_printf("failed to send command, %s (%d)",
			   strerror(-rc), rc);
		goto out;
	}

	log_printf("succeeded");

out:
	nlmsg_free(msg);
	return rc;
}

static void wigigap_handle_assoc_req(struct wigigap_state *state,
				     const struct ieee80211_mgmt *assoc_req)
{
	int rc, i;

	const u8 assoc_resp_template[] =
		{ 0x10, 0x00, 0xcf, 0x04,
		  /* RA */
		  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		  /* TA */
		  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		  /* BSSID */
		  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		  /* seq number */
		  0xb0, 0x01,
		  /* capabilities */
		  0x06, 0x00,
		  /* status code */
		  0x00, 0x00,
		  /* aid */
		  0x01, 0x00 };
	struct ieee80211_mgmt *assoc_resp = (struct ieee80211_mgmt *)assoc_resp_template;

	log_printf("assoc req received from " MACSTR, MAC2STR(assoc_req->sa));

	if (!state->device_ap_sme_disabled)
		return;

	if (state->delay_assoc == -1)
		return;

	/* send assoc response */
	if (state->delay_assoc)
		wigigap_delay_ms(state->delay_assoc);
	memcpy(assoc_resp->da, assoc_req->sa, ETH_ALEN);
	memcpy(assoc_resp->sa, assoc_req->da, ETH_ALEN);
	memcpy(assoc_resp->bssid, assoc_req->bssid, ETH_ALEN);
	state->last_aid++;
	assoc_resp->assoc_resp.aid = state->last_aid;
	if (state->reject_assoc) {
		assoc_resp->assoc_resp.status_code =
			WLAN_STATUS_UNSPECIFIED_FAILURE;
	} else {
		i = wigigap_alloc_sta(state, assoc_req->sa);
		if (i == -1) {
			log_printf("too many STAs connected");
			return;
		}
	}

	rc = wigigap_send_frame_cmd(state, (const u8 *)assoc_resp,
				    sizeof(assoc_resp_template));
	if (rc) {
		wigigap_free_sta(state, assoc_req->sa);
		return;
	}

	if (state->reject_assoc) {
		log_printf("assoc rejected");
		return;
	}

	state->sta[i].aid = state->last_aid;
	log_printf("assoc accepted, aid %d", state->last_aid);
}

static void wigigap_handle_disassoc(struct wigigap_state *state,
				    const struct ieee80211_mgmt *disassoc)
{
	int rc, i;

	log_printf("disassoc received from " MACSTR ", reason %d",
		   MAC2STR(disassoc->sa), disassoc->disassoc.reason_code);

	if (!state->device_ap_sme_disabled)
		return;

	i = wigigap_find_sta(state, disassoc->sa);
	if (i == -1) {
		log_printf("fail to find such STA");
		return;
	}

	/* disallow data transmit */
	rc = wigigap_send_set_sta(state, disassoc->sa, false);
	if (rc)
		log_printf("fail to disallow data transmit");

	/* send DEL_STA command */
	rc = wigigap_send_del_sta(state, disassoc->sa,
				  WLAN_REASON_DISASSOC_STA_HAS_LEFT);
	if (rc)
		return;

	log_printf("DEL_STA sent");
}

static void wigigap_handle_mgmt(struct wigigap_state *state,
				struct nlattr *freq, const u8 *frame,
				size_t len)
{
	const struct ieee80211_mgmt *mgmt;
	u16 fc, stype;
	int rx_freq = 0;

	mgmt = (const struct ieee80211_mgmt *)frame;
	if (len < offsetof(struct ieee80211_mgmt, variable)) {
		log_printf("Too short management frame (%d)", len);
		return;
	}

	fc = le_to_host16(mgmt->frame_control);
	stype = WLAN_FC_GET_STYPE(fc);

	if (freq)
		rx_freq = nla_get_u32(freq);

	log_printf("RX frame sa=" MACSTR " freq=%d fc=0x%x seq_ctrl=0x%x stype=%u (%s) len=%u",
		   MAC2STR(mgmt->sa), rx_freq, fc, le_to_host16(mgmt->seq_ctrl),
		   stype, fc2str(fc), (unsigned int)len);

	switch (stype) {
	case WLAN_FC_STYPE_ASSOC_REQ:
		wigigap_handle_assoc_req(state, mgmt);
		break;
	case WLAN_FC_STYPE_DISASSOC:
		wigigap_handle_disassoc(state, mgmt);
		break;
	default:
		break;
	}
}

static void mlme_event_mgmt_tx_status(struct wigigap_state *state,
				      const u8 *frame, size_t len,
				      struct nlattr *ack)
{
	const struct ieee80211_mgmt *mgmt;
	u16 fc, stype;
	int i;

	mgmt = (const struct ieee80211_mgmt *)frame;
	if (len < offsetof(struct ieee80211_mgmt, variable)) {
		log_hexdump("MLME event frame", frame, len);
		log_printf("Too short management frame (%d)", len);
		return;
	}

	fc = le_to_host16(mgmt->frame_control);
	stype = WLAN_FC_GET_STYPE(fc);

	if (ack == NULL) {
		log_hexdump("MLME event frame", frame, len);
		log_printf("fail");
		return;
	}

	log_printf("%s sent successfuly to " MACSTR,
		   fc2str(fc), MAC2STR(mgmt->da));

	switch (stype) {
	case WLAN_FC_STYPE_ASSOC_RESP:
		/* send NEW_STA command */
		i = wigigap_find_sta(state, mgmt->da);
		if (i == -1) {
			log_printf("fail to find such STA");
			break;
		}

		wigigap_send_new_sta(state, mgmt->da, state->sta[i].aid);
		break;
	case WLAN_FC_STYPE_DISASSOC:
		/* disconnect triggered locally, send DEL_STA command */
		wigigap_send_del_sta(state, mgmt->da,
				     WLAN_REASON_UNSPECIFIED);
		break;
	default:
		break;
	}
}

static void wigigap_mlme_event(struct wigigap_state *state,
			       enum nl80211_commands cmd,
			       struct nlattr *frame,
			       struct nlattr *addr, struct nlattr *timed_out,
			       struct nlattr *freq, struct nlattr *ack)
{
	const u8 *data;
	size_t len;

	if (timed_out && addr) {
		log_printf("mlme_timeout_event");
		return;
	}

	if (frame == NULL) {
		log_printf("MLME event %d (%s) without frame data",
			   cmd, nl80211_command_to_string(cmd));
		return;
	}

	data = nla_data(frame);
	len = nla_len(frame);
	if (len < 4 + 2 * ETH_ALEN) {
		log_printf("MLME event %d (%s) too short (%d)",
			   cmd, nl80211_command_to_string(cmd), len);
		return;
	}
	log_printf("MLME event %d (%s) A1=" MACSTR " A2=" MACSTR, cmd,
		   nl80211_command_to_string(cmd), MAC2STR(data + 4),
		   MAC2STR(data + 4 + ETH_ALEN));

	/* TODO: ignore frame if not addressed to us */

	switch (cmd) {
	case NL80211_CMD_FRAME:
		log_hexdump("MLME event frame", data, len);
		wigigap_handle_mgmt(state, freq, data, len);
		break;
	case NL80211_CMD_FRAME_TX_STATUS:
		mlme_event_mgmt_tx_status(state, data, len, ack);
		break;
	default:
		break;
	}
}

static void wigigap_local_disassoc(struct wigigap_state *state, const u8 *mac)
{
	int rc;
	const u8 disassoc_template[] =
		{ 0xa0, 0x00, 0xcf, 0x04,
		  /* RA */
		  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		  /* TA */
		  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		  /* BSSID */
		  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		  /* seq number */
		  0xb0, 0x01,
		  /* capabilities */
		  0x06, 0x00,
		  /* reason code */
		  0x01, 0x00 };
	struct ieee80211_mgmt *disassoc = (struct ieee80211_mgmt *)disassoc_template;

	sleep(3);
	log_printf("sta " MACSTR, MAC2STR(mac));

	/* send disassoc frame */
	memcpy(disassoc->da, mac, ETH_ALEN);
	memcpy(disassoc->sa, state->own_mac, ETH_ALEN);
	memcpy(disassoc->bssid, state->own_mac, ETH_ALEN);
	disassoc->disassoc.reason_code = WLAN_REASON_UNSPECIFIED;

	rc = wigigap_send_frame_cmd(state, (const u8 *)disassoc,
				    sizeof(disassoc_template));
	if (rc)
		return;
}

static void nl80211_new_station_event(struct wigigap_state *state,
				      struct nlattr **tb)
{
	unsigned char *addr;
	unsigned char *ies = NULL;
	size_t ies_len = 0;

	if (tb[NL80211_ATTR_MAC] == NULL)
		return;
	addr = nla_data(tb[NL80211_ATTR_MAC]);
	log_printf("New station " MACSTR, MAC2STR(addr));

	if (tb[NL80211_ATTR_IE]) {
		ies = nla_data(tb[NL80211_ATTR_IE]);
		ies_len = nla_len(tb[NL80211_ATTR_IE]);
	}
	log_printf("Assoc Req IEs %p, len %d", ies, ies_len);

	if (!state->device_ap_sme_disabled)
		return;

	/* allow data transmit */
	wigigap_send_set_sta(state, addr, true);

	if (state->local_disassoc)
		wigigap_local_disassoc(state, addr);

	if (state->assoc_iterations)
		if (--state->assoc_iterations == 0)
			terminate = 2;
}

static void nl80211_del_station_event(struct wigigap_state *state,
				      struct nlattr **tb)
{
	unsigned char *addr;

	if (tb[NL80211_ATTR_MAC] == NULL)
		return;
	addr = nla_data(tb[NL80211_ATTR_MAC]);

	wigigap_free_sta(state, addr);

	log_printf("Delete station " MACSTR, MAC2STR(addr));
}

/**
 * handler for nl events, both unicast and broadcast
 */
static int wigigap_nl_event_handler(struct nl_msg *msg, void *arg)
{
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	int ifidx = -1;
	struct wigigap_state *state = (struct wigigap_state *)arg;
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	enum nl80211_commands cmd = gnlh->cmd;

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (tb[NL80211_ATTR_IFINDEX])
		ifidx = nla_get_u32(tb[NL80211_ATTR_IFINDEX]);

	log_printf("cmd %d (%s) received for ifidx %d", cmd,
		   nl80211_command_to_string(cmd), ifidx);

	if (ifidx != state->ifindex)
		return NL_SKIP;

	switch (cmd) {
	case NL80211_CMD_NEW_STATION:
		nl80211_new_station_event(state, tb);
		break;
	case NL80211_CMD_DEL_STATION:
		nl80211_del_station_event(state, tb);
		break;
	case NL80211_CMD_FRAME:
	case NL80211_CMD_FRAME_TX_STATUS:
		wigigap_mlme_event(state, gnlh->cmd, tb[NL80211_ATTR_FRAME],
				   tb[NL80211_ATTR_MAC],
				   tb[NL80211_ATTR_TIMED_OUT],
				   tb[NL80211_ATTR_WIPHY_FREQ],
				   tb[NL80211_ATTR_ACK]);
		break;
	default:
		log_printf("unknown nl event");
		break;
	}

	return NL_SKIP;
}

/**
 * handler for resolving multicast group (family) id
 * used in nl_get_multicast_id below
 */
struct family_data {
	const char *group;
	int id;
};

static int family_handler(struct nl_msg *msg, void *arg)
{
	struct family_data *res = (struct family_data *)arg;
	struct nlattr *tb[CTRL_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *mcgrp;
	int tmp;

	nla_parse(tb, CTRL_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);
	if (!tb[CTRL_ATTR_MCAST_GROUPS])
		return NL_SKIP;

	nla_for_each_nested(mcgrp, tb[CTRL_ATTR_MCAST_GROUPS], tmp) {
		struct nlattr *tb2[CTRL_ATTR_MCAST_GRP_MAX + 1];

		nla_parse(tb2, CTRL_ATTR_MCAST_GRP_MAX, nla_data(mcgrp),
			  nla_len(mcgrp), NULL);
		if (!tb2[CTRL_ATTR_MCAST_GRP_NAME] ||
		    !tb2[CTRL_ATTR_MCAST_GRP_ID] ||
		    strncmp(nla_data(tb2[CTRL_ATTR_MCAST_GRP_NAME]),
			    res->group,
			    nla_len(tb2[CTRL_ATTR_MCAST_GRP_NAME])) != 0)
			continue;
		res->id = nla_get_u32(tb2[CTRL_ATTR_MCAST_GRP_ID]);
		break;
	};

	return NL_SKIP;
}

/**
 * get a multicast group id for registering
 * (such as for "vendor" or "mlme" events)
 */
static int nl_get_multicast_id(struct wigigap_state *state,
			       const char *family, const char *group)
{
	struct nl_msg *msg;
	int ret;
	struct family_data res = { group, -ENOENT };

	msg = nlmsg_alloc();
	if (!msg)
		return -ENOMEM;
	if (!genlmsg_put(msg, 0, 0, genl_ctrl_resolve(state->nl, "nlctrl"),
			 0, 0, CTRL_CMD_GETFAMILY, 0) ||
	    nla_put_string(msg, CTRL_ATTR_FAMILY_NAME, family)) {
		nlmsg_free(msg);
		return -ENOBUFS;
	}

	ret = nl_cmd_send_and_recv(state->nl, state->cb, msg, family_handler,
				   &res);
	if (ret == 0)
		ret = res.id;

	return ret;
}

/**
 * add a multicast group to an NL socket so it
 * can receive events for that group
 */
static int wigigap_nl_socket_add_membership(struct nl_sock *sk, int group_id)
{
	int s_fd = nl_socket_get_fd(sk);

	if (s_fd == -1 || group_id < 0)
		return -EINVAL;

	return setsockopt(s_fd, SOL_NETLINK, NETLINK_ADD_MEMBERSHIP,
			  &group_id, sizeof(group_id));
}

/**
 * destroy the structures for NL communication
 */
static void wigigap_destroy_nl_globals(struct wigigap_state *state)
{
	if (state->nl) {
		nl_socket_free(state->nl);
		state->nl = NULL;
	}
	if (state->nl_event) {
		nl_socket_free(state->nl_event);
		state->nl_event = NULL;
	}
	if (state->cb) {
		nl_cb_put(state->cb);
		state->cb = NULL;
	}
	state->nl80211_id = 0;
}

/**
 * initialize structures for NL communication
 */
static bool wigigap_init_nl_globals(struct wigigap_state *state)
{
	bool res = false;
	int rc;

	/* specify NL_CB_DEBUG instead of NL_CB_DEFAULT to
	   get defailed traces of NL messages */
	state->cb = nl_cb_alloc(NL_CB_DEFAULT);
	if (state->cb == NULL) {
		log_printf("failed to allocate nl callback");
		goto out;
	}
	nl_cb_set(state->cb, NL_CB_SEQ_CHECK, NL_CB_CUSTOM, nl_no_seq_check, NULL);

	state->nl = nl_socket_alloc_cb(state->cb);
	if (state->nl == NULL) {
		log_printf("failed to allocate nl handle");
		goto out;
	}

	if (genl_connect(state->nl)) {
		log_printf("failed to connect to nl handle");
		goto out;
	}

	state->nl80211_id = genl_ctrl_resolve(state->nl, "nl80211");
	if (state->nl80211_id < 0) {
		log_printf("failed to get nl80211 family, error %d",
			   state->nl80211_id);
		goto out;
	}

	state->nl_event = nl_socket_alloc_cb(state->cb);
	if (state->nl_event == NULL) {
		log_printf("failed to allocate nl handle for events");
		goto out;
	}

	if (genl_connect(state->nl_event)) {
		log_printf("failed to connect to nl_event handle");
		goto out;
	}

	/* register for NEW_STA, DEL_STA */
	rc = nl_get_multicast_id(state, "nl80211", "mlme");
	if (rc < 0) {
		log_printf("could not get mlme multicast id, err %d", rc);
		goto out;
	}
	rc = wigigap_nl_socket_add_membership(state->nl_event, rc);
	if (rc < 0) {
		log_printf("could not register for mlme events, err %d", rc);
		goto out;
	}

	nl_socket_set_nonblocking(state->nl_event);

	nl_cb_set(state->cb, NL_CB_VALID, NL_CB_CUSTOM, wigigap_nl_event_handler,
		  state);

	res = true;
out:
	if (!res)
		wigigap_destroy_nl_globals(state);
	return res;
}

static int wigigap_set_interface(struct wigigap_state *state,
				 enum nl80211_iftype mode)
{
	int rc = 0;

	log_printf("mode %d", mode);

	struct nl_msg *msg = allocate_nl_cmd_msg(state->nl80211_id,
						 state->ifindex,
						 0,
						 NL80211_CMD_SET_INTERFACE);
	if (msg == NULL) {
		log_printf("failed to allocate msg");
		rc = -ENOMEM;
		goto out;
	}

	if (nla_put_u32(msg, NL80211_ATTR_IFTYPE, mode)) {
		log_printf("failed to set params");
		rc = -ENOBUFS;
		goto out;
	}

	rc = nl_cmd_send_and_recv(state->nl, state->cb, msg, NULL, NULL);
	if (rc) {
		log_printf("failed to send command, %s (%d)", strerror(-rc), rc);
		goto out;
	}

	log_printf("succeeded");

out:
	nlmsg_free(msg);
	return rc;
}

static int wigigap_register_frame(struct wigigap_state *state, int type)
{
	int rc = 0;
	struct nl_msg *msg;

	log_printf("frame type %d, subtype %d",
		   (type & IEEE80211_FCTL_FTYPE) >> 2,
		   (type & IEEE80211_FCTL_STYPE) >> 4);

	msg = allocate_nl_cmd_msg(state->nl80211_id, state->ifindex, 0,
				  NL80211_CMD_REGISTER_ACTION);
	if (msg == NULL) {
		log_printf("failed to allocate msg");
		rc = -ENOMEM;
		goto out;
	}

	if (nla_put_u32(msg, NL80211_ATTR_FRAME_TYPE, type) ||
	    nla_put(msg, NL80211_ATTR_FRAME_MATCH, 0, NULL)) {
		log_printf("failed to set params");
		rc = -ENOBUFS;
		goto out;
	}

	rc = nl_cmd_send_and_recv(state->nl, state->cb, msg, NULL, NULL);
	if (rc) {
		log_printf("failed to send command, %s (%d)", strerror(-rc), rc);
		goto out;
	}

	log_printf("succeeded");

out:
	nlmsg_free(msg);
	return rc;
}

static int wigigap_register_frames(struct wigigap_state *state)
{
	int fc, rc;

	fc = (WLAN_FC_TYPE_MGMT << 2 | WLAN_FC_STYPE_PROBE_REQ << 4);
	rc = wigigap_register_frame(state, fc);
	if (rc)
		return rc;
	fc = (WLAN_FC_TYPE_MGMT << 2 | WLAN_FC_STYPE_ASSOC_REQ << 4);
	rc = wigigap_register_frame(state, fc);
	if (rc)
		return rc;
	fc = (WLAN_FC_TYPE_MGMT << 2 | WLAN_FC_STYPE_DISASSOC << 4);
	rc = wigigap_register_frame(state, fc);

	return rc;
}

/**
 * handler for NL80211_CMD_GET_WIPHY results
 */
static int wiphy_info_handler(struct nl_msg *msg, void *arg)
{
	struct wigigap_state *state = (struct wigigap_state *)arg;
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = (struct genlmsghdr *)
		nlmsg_data(nlmsg_hdr(msg));

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (!tb[NL80211_ATTR_DEVICE_AP_SME])
		state->device_ap_sme_disabled = 1;

	return NL_SKIP;
}

static int wigigap_get_device_ap_sme(struct wigigap_state *state)
{
	int rc = 0;
	struct nl_msg *msg;

	msg = allocate_nl_cmd_msg(state->nl80211_id, state->ifindex, 0,
				  NL80211_CMD_GET_WIPHY);
	if (msg == NULL || nla_put_flag(msg, NL80211_ATTR_SPLIT_WIPHY_DUMP)) {
		log_printf("failed to allocate msg");
		rc = -ENOBUFS;
		goto out;
	}

	state->device_ap_sme_disabled = 0;
	rc = nl_cmd_send_and_recv(state->nl, state->cb, msg,
				  wiphy_info_handler, state);
	if (rc) {
		log_printf("failed to send command, %s (%d)", strerror(-rc), rc);
		goto out;
	}

	log_printf("succeeded");

out:
	nlmsg_free(msg);
	return rc;
}

static int wigigap_start_ap(struct wigigap_state *state, const char *ssid,
			    size_t ssid_len)
{
	int rc = 0;
	struct nl_msg *msg;
	struct ieee80211_mgmt *frame;
	size_t frame_len = sizeof(*frame) + ssid_len + 2;

	log_printf("ssid %s", ssid);

	frame = (struct ieee80211_mgmt*)malloc(frame_len);
	if (frame == NULL) {
		log_printf("failed to allocate frame");
		return -ENOMEM;
	}
	memset(frame, 0, sizeof(*frame));
	frame->beacon.beacon_int = 100;
	frame->beacon.capab_info = 1;
	((u8*)(frame + 1))[0] = WLAN_EID_SSID;
	((u8*)(frame + 1))[1] = ssid_len;
	memcpy((u8*)(frame + 1) + 2, ssid, ssid_len);

	msg = allocate_nl_cmd_msg(state->nl80211_id, state->ifindex, 0,
				  NL80211_CMD_START_AP);
	if (msg == NULL) {
		log_printf("failed to allocate msg");
		rc = -ENOMEM;
		goto out;
	}

	if (nla_put(msg, NL80211_ATTR_SSID, ssid_len, ssid) ||
	    nla_put_u32(msg, NL80211_ATTR_BEACON_INTERVAL, 100) ||
	    nla_put_u32(msg, NL80211_ATTR_DTIM_PERIOD, 0)) {
		rc = -ENOBUFS;
		goto out;
	}

	frame->frame_control = WLAN_FC_STYPE_BEACON << 4;
	rc = nla_put(msg, NL80211_ATTR_BEACON_HEAD, frame_len, frame);
	if (rc)
		goto out;

	frame->frame_control = WLAN_FC_STYPE_PROBE_RESP << 4;
	rc = nla_put(msg, NL80211_ATTR_PROBE_RESP, frame_len, frame);
	if (rc)
		goto out;

	if (nla_put_u32(msg, NL80211_ATTR_WIPHY_FREQ, WIGIGAP_DEFAULT_FREQ)) {
		rc = -ENOBUFS;
		goto out;
	}

	rc = nl_cmd_send_and_recv(state->nl, state->cb, msg, NULL, NULL);
	if (rc) {
		log_printf("failed to send command, %s (%d)", strerror(-rc), rc);
		goto out;
	}

	log_printf("succeeded");

out:
	free(frame);
	nlmsg_free(msg);
	return rc;
}

static int wigigap_stop_ap(struct wigigap_state *state)
{
	int rc = 0;
	struct nl_msg *msg;

	log_printf("");

	msg = allocate_nl_cmd_msg(state->nl80211_id, state->ifindex, 0,
				  NL80211_CMD_STOP_AP);
	if (msg == NULL) {
		log_printf("failed to allocate msg");
		rc = -ENOMEM;
		goto out;
	}

	rc = nl_cmd_send_and_recv(state->nl, state->cb, msg, NULL, NULL);
	if (rc) {
		log_printf("failed to send command, %s (%d)", strerror(-rc), rc);
		goto out;
	}

	log_printf("succeeded");

out:
	nlmsg_free(msg);
	return rc;
}

void wigigap_terminate(int sig)
{
	terminate = 1;
}

int wigigap_get_ifhwaddr(const char *ifname, u8 *addr)
{
	struct ifreq ifr;
	int s;

	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, ifname, IFNAMSIZ);
	s = socket(PF_INET, SOCK_DGRAM, 0);
	if (ioctl(s, SIOCGIFHWADDR, &ifr)) {
		log_printf("Could not get interface %s hwaddr: %s",
			   ifname, strerror(errno));
		close(s);
		return -1;
	}
	close(s);

	memcpy(addr, ifr.ifr_hwaddr.sa_data, ETH_ALEN);

	return 0;
}

int main(int argc, char *argv[])
{
	const char *ifname;
	struct pollfd fds[2];
	int f, optidx = 0;
	int rc;
	bool print_help = false;
	static const struct option options[] = {
		{ "assoc-iterations", required_argument, 0, 'i' },
		{ "delay-assoc", required_argument, 0, 'd' },
		{ "local-disassoc", no_argument, 0, 'l' },
		{ "reject-assoc", no_argument, 0, 'r' },
		{ "delay-sta", required_argument, 0, 's' },
		{ "help", no_argument, 0, 'h' },
		{ 0, 0, 0, 0 }
	};

	memset(&g_state, 0, sizeof(g_state));
	while ((f = getopt_long(argc, argv, "i:d:lrs:h", options, &optidx)) != EOF)
		switch (f) {
		case 'i':
			g_state.assoc_iterations = atoi(optarg);
			break;
		case 'd':
			g_state.delay_assoc = atoi(optarg);
			break;
		case 'l':
			g_state.local_disassoc = true;
			break;
		case 'r':
			g_state.reject_assoc = true;
			break;
		case 's':
			g_state.delay_sta = atoi(optarg);
			break;
		case 'h':
			print_help = true;
			break;
		default:
			goto help;
	}

	argc -= optind;
	argv = &argv[optind];

	if (argc < 1 || print_help) {
help:
		_log_printf("Usage: wigigaptest [options] interface_name\n");
		_log_printf("Options:\n");
		_log_printf(" -i <num> | --assoc-iterations : exit after num sta associations\n");
		_log_printf(" -d <ms> | --delay-assoc : delay sending of assoc response by ms milli-seconds\n");
		_log_printf(" -l | --local-disassoc : trigger local disassoc following association\n");
		_log_printf(" -r | --reject-assoc : reject assoc (send assoc response with reject code)\n");
		_log_printf(" -s <ms> | --delay-sta : delay sending of NEW_STA/DEL_STA by ms milli-seconds\n");
		_log_printf(" -h | --help : print this message\n");
		return -1;
	}

	ifname = argv[0];
	g_state.ifindex = if_nametoindex(ifname);
	if (g_state.ifindex == 0) {
		_log_printf("unknown interface %s\n", ifname);
		return -1;
	}
	rc = wigigap_get_ifhwaddr(ifname, g_state.own_mac);
	if (rc) {
		_log_printf("fail to get own mac\n");
		return -1;
	}
	_log_printf("interface index of %s is %d, mac " MACSTR "\n", ifname,
		    g_state.ifindex, MAC2STR(g_state.own_mac));

	if (!wigigap_init_nl_globals(&g_state))
		return -1;

	rc = wigigap_set_interface(&g_state, NL80211_IFTYPE_AP);
	if (rc)
		goto out;

	rc = wigigap_register_frames(&g_state);
	if (rc)
		goto out;

	rc = wigigap_get_device_ap_sme(&g_state);
	if (rc)
		goto out;
	_log_printf("device AP SME %s\n",
		    g_state.device_ap_sme_disabled ? "OFF" : "ON");

	rc = wigigap_start_ap(&g_state, WIGIGAP_DEFAULT_SSID,
			      strlen(WIGIGAP_DEFAULT_SSID));
	if (rc)
		goto out;

	/* wait for events on unicast and multicast nl sockets */
	memset(fds, 0, sizeof(fds));
	fds[0].fd = nl_socket_get_fd(g_state.nl);
	fds[1].fd = nl_socket_get_fd(g_state.nl_event);
	fds[0].events = fds[1].events = POLLIN;

	signal(SIGINT, wigigap_terminate);
	_log_printf("waiting for events...\n");
	while (1) {
		int res = poll(fds, 2, -1);
		if (res < 0) {
			_log_printf("poll failed: %s\n", strerror(errno));
			break;
		}
		if (fds[0].revents & POLLIN) {
			res = nl_recvmsgs(g_state.nl, g_state.cb);
			if (res < 0) {
				_log_printf("nl_recvmsgs(nl) failed: %s\n",
					    strerror(errno));
				/* something is wrong with the events socket */
				break;
			}
		}
		if (fds[1].revents & POLLIN) {
			res = nl_recvmsgs(g_state.nl_event, g_state.cb);
			if (res < 0) {
				_log_printf("nl_recvmsgs(nl_event) failed: %s\n",
					    strerror(errno));
				/* something is wrong with the events socket */
				break;
			}
		}

		if (terminate) {
			if (terminate > 1)
				sleep(5);
			break;
		}
	}

out:
	_log_printf("\ncleanup...\n");
	wigigap_stop_ap(&g_state);
	wigigap_destroy_nl_globals(&g_state);

	return rc;
}
