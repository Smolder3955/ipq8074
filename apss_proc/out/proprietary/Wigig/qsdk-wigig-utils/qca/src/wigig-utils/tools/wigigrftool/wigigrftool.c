/*
 * Copyright (c) 2016 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <net/if.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netlink/netlink.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>
#include <poll.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <getopt.h>
#include "nl80211_copy.h"
#include "qca-vendor.h"

#define WIGIGRFTOOL_VERSION	"0.1"

#define ETH_ALEN	6

#ifndef SOL_NETLINK
#define SOL_NETLINK	270
#endif

#define WT_MAX_RF_MODULES	(8)
#define WT_MIN_ANTENNAS	(1)
#define WT_MAX_ANTENNAS	(32)

#define WT_UNLOCK_SECTOR_INDEX	(0xffff)

/* sector configuration for single RF module */
struct wt_sector_cfg_entry {
	uint8_t rf_module_index;
	uint32_t psh_hi, psh_lo;
	uint32_t etype0, etype1, etype2;
	uint32_t dtype_x16;
};

/* structure with global state, passed to callback handlers */
struct wt_state {
	/* callbacks handle for synchronous NL commands */
	struct nl_cb *cb;

	/* nl socket handle for synchronous NL commands */
	struct nl_sock *nl;

	/* nl socket handler for events */
	struct nl_sock *nl_event;

	/* family id for nl80211 events */
	int nl80211_id;

	/* interface index of wigig driver */
	int ifindex;

	/* capabilities ,set based on vendor commands supported by driver */
	bool capa_get_rf_sector_cfg, capa_set_rf_sector_cfg;
	bool capa_get_rf_sel_sector, capa_set_rf_sel_sector;

	/* tsf value from get sector/selected sector calls */
	uint64_t tsf;

	/* number of RF modules in sector configuration */
	int n_rf_modules;

	/* stores sector configuration */
	struct wt_sector_cfg_entry sectors[WT_MAX_RF_MODULES];

	/* stores selected sector */
	bool has_selected_sector;
	uint16_t selected_sector;
} g_state;

/* policy for parsing QCA vendor commands */
static struct
nla_policy nl80211_qca_policy[QCA_WLAN_VENDOR_ATTR_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_TSF] = { .type = NLA_U64 },
	[QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_INDEX] = { .type = NLA_U16 },
	[QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_TYPE] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_DMG_RF_MODULE_MASK] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_CFG] = { .type = NLA_NESTED },
	[QCA_WLAN_VENDOR_ATTR_BRP_ANT_NUM_LIMIT] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_BRP_ANT_LIMIT_MODE] = { .type = NLA_U8 },
};

/* policy for parsing RF sector cfg */
static struct
nla_policy nl80211_sector_cfg_policy[
	QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_CFG_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_CFG_MODULE_INDEX] = {
		.type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_CFG_ETYPE0] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_CFG_ETYPE1] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_CFG_ETYPE2] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_CFG_PSH_HI] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_CFG_PSH_LO] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_CFG_DTYPE_X16] = {
		.type = NLA_U32 },
};

__attribute__ ((format (printf, 1, 2)))
static int log_printf(const char *fmt, ...)
{
	int ret;
	va_list ap;

	va_start(ap, fmt);
	ret = vfprintf(stdout, fmt, ap);
	va_end(ap);
	fflush(stdout);
	return ret;
}

__attribute__ ((format (printf, 2, 3)))
static int log_error(int rc, const char *fmt, ...)
{
	int ret;
	va_list ap;

	va_start(ap, fmt);
	fprintf(stderr, "ERROR(%d): ", rc);
	ret = vfprintf(stderr, fmt, ap);
	va_end(ap);
	fflush(stderr);
	return ret;
}

/**
 * nl callback handler for disabling sequence number checking
 */
static int no_seq_check(struct nl_msg *msg, void *arg)
{
	return NL_OK;
}

/**
 * nl callback handler called on error
 */
static int
error_handler(struct sockaddr_nl *nla, struct nlmsgerr *err, void *arg)
{
	int *ret = (int *)arg;
	*ret = err->error;
	return NL_SKIP;
}

/**
 * nl callback handler called after all messages in
 * a multi-message reply are delivered. It is used
 * to detect end of synchronous command/reply
 */
static int finish_handler(struct nl_msg *msg, void *arg)
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
static int ack_handler(struct nl_msg *msg, void *arg)
{
	int *err = (int *)arg;
	*err = 0;
	return NL_STOP;
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
 * send NL command and receive reply synchronously
 */
static int nl_cmd_send_and_recv(struct nl_sock *nl,
				struct nl_cb *cb,
				struct nl_msg *msg,
				int (*valid_handler)(struct nl_msg *, void *),
				void *valid_data)
{
	struct nl_cb *cb_clone = nl_cb_clone(cb);
	int err;

	if (!cb_clone) {
		log_error(-ENOMEM, "failed to allocate cb_clone\n");
		return -ENOMEM;
	}

	err = nl_send_auto_complete(nl, msg);
	if (err < 0) {
		log_error(err, "failed to send message\n");
		nl_cb_put(cb_clone);
		return err;
	}

	err = 1;
	nl_cb_err(cb_clone, NL_CB_CUSTOM, error_handler, &err);
	nl_cb_set(cb_clone, NL_CB_FINISH, NL_CB_CUSTOM, finish_handler, &err);
	nl_cb_set(cb_clone, NL_CB_ACK, NL_CB_CUSTOM, ack_handler, &err);
	if (valid_handler)
		nl_cb_set(cb_clone, NL_CB_VALID, NL_CB_CUSTOM,
			  valid_handler, valid_data);

	while (err > 0) {
		int res = nl_recvmsgs(nl, cb_clone);

		if (res < 0) {
			log_error(res, "nl_recvmsgs failed: %s\n",
				  strerror(-res));
			/* TODO should we exit the loop here?
			 * similar code in supplicant does not
			 */
		}
	}

	nl_cb_put(cb_clone);
	return err;
}

/**
 * get a multicast group id for registering
 * (such as for vendor events)
 */
static int nl_get_multicast_id(struct wt_state *state,
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

	ret = nl_cmd_send_and_recv(state->nl, state->cb, msg,
				   family_handler, &res);
	if (ret == 0)
		ret = res.id;

	return ret;
}

/**
 * add a multicast group to an NL socket so it
 * can receive events for that group
 */
int nl_socket_add_membership(struct nl_sock *sk, int group_id)
{
	int err;
	int s_fd = nl_socket_get_fd(sk);

	if (s_fd == -1 || group_id < 0)
		return -EINVAL;

	err = setsockopt(s_fd, SOL_NETLINK, NETLINK_ADD_MEMBERSHIP,
			 &group_id, sizeof(group_id));

	if (err < 0)
		return err;

	return 0;
}

/**
 * handler for NL80211_CMD_GET_WIPHY results
 */
static int wiphy_info_handler(struct nl_msg *msg, void *arg)
{
	struct wt_state *state = (struct wt_state *)arg;
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct nlattr *attr;
	struct genlmsghdr *gnlh = (struct genlmsghdr *)
		nlmsg_data(nlmsg_hdr(msg));
	struct nl80211_vendor_cmd_info *cmd;
	int tmp;

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (tb[NL80211_ATTR_VENDOR_DATA]) {
		nla_for_each_nested(attr, tb[NL80211_ATTR_VENDOR_DATA], tmp) {
			if (nla_len(attr) != sizeof(*cmd))
				continue;

			cmd = nla_data(attr);
			if (cmd->vendor_id != OUI_QCA)
				continue;
			switch (cmd->subcmd) {
			case QCA_NL80211_VENDOR_SUBCMD_DMG_RF_GET_SECTOR_CFG:
				state->capa_get_rf_sector_cfg = true;
				break;
			case QCA_NL80211_VENDOR_SUBCMD_DMG_RF_SET_SECTOR_CFG:
				state->capa_set_rf_sector_cfg = true;
				break;
			case QCA_NL80211_VENDOR_SUBCMD_DMG_RF_GET_SELECTED_SECTOR:
				state->capa_get_rf_sel_sector = true;
				break;
			case QCA_NL80211_VENDOR_SUBCMD_DMG_RF_SET_SELECTED_SECTOR:
				state->capa_set_rf_sel_sector = true;
				break;
			}
		}
	}
	return NL_SKIP;
}

/**
 * handle for vendor events
 */
static int wt_event_handler(struct nl_msg *msg, void *arg)
{
	/* nothing needed for now */
	return NL_SKIP;
}

/**
 * destroy the structures for NL communication
 */
static void destroy_nl_globals(struct wt_state *state)
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
static bool init_nl_globals(struct wt_state *state)
{
	bool res = false;
	int rc;

	/* specify NL_CB_DEBUG instead of NL_CB_DEFAULT to
	 * get defailed traces of NL messages
	 */
	state->cb = nl_cb_alloc(NL_CB_DEFAULT);
	if (state->cb == NULL) {
		log_error(-1, "failed to allocate nl callback\n");
		goto out;
	}
	nl_cb_set(state->cb, NL_CB_SEQ_CHECK, NL_CB_CUSTOM, no_seq_check, NULL);

	state->nl = nl_socket_alloc_cb(state->cb);
	if (state->nl == NULL) {
		log_error(-1, "failed to allocate nl handle\n");
		goto out;
	}

	if (genl_connect(state->nl)) {
		log_error(-1, "failed to connect to nl handle\n");
		goto out;
	}

	state->nl80211_id = genl_ctrl_resolve(state->nl, "nl80211");
	if (state->nl80211_id < 0) {
		log_error(state->nl80211_id,
			  "failed to get nl80211 family\n");
		goto out;
	}

	state->nl_event = nl_socket_alloc_cb(state->cb);
	if (state->nl_event == NULL) {
		log_error(-1, "failed to allocate nl handle for events\n");
		goto out;
	}

	if (genl_connect(state->nl_event)) {
		log_error(-1, "failed to connect to nl_event handle\n");
		goto out;
	}

	/* register for receiving vendor events */
	rc = nl_get_multicast_id(state, "nl80211", "vendor");
	if (rc < 0) {
		log_error(rc, "could not get vendor multicast id: %s\n",
			  strerror(-rc));
		goto out;
	}

	rc = nl_socket_add_membership(state->nl_event, rc);
	if (rc < 0) {
		log_error(rc, "could not register for vendor events: %s\n",
			   strerror(-rc));
		goto out;
	}

	nl_socket_set_nonblocking(state->nl_event);

	nl_cb_set(state->cb, NL_CB_VALID, NL_CB_CUSTOM,
		  wt_event_handler, state);

	res = true;
out:
	if (!res)
		destroy_nl_globals(state);
	return res;
}

/**
 * allocate an nl_msg for sending a command
 */
static struct nl_msg *allocate_nl_cmd_msg(int family, int ifindex,
					  int flags, uint8_t cmd)
{
	struct nl_msg *msg = nlmsg_alloc();

	if (!msg) {
		log_error(-ENOMEM, "failed to allocate nl msg\n");
		return NULL;
	}

	if (!genlmsg_put(msg,
			 0, /* pid (automatic) */
			 0, /* sequence number (automatic) */
			 family, /* family */
			 0, /* user specific header length */
			 flags, /* flags */
			 cmd, /* command */
			 0) /* protocol version */
	    ) {
		log_error(-1, "failed to init msg\n");
		nlmsg_free(msg);
		return NULL;
	}

	if (nla_put_u32(msg, NL80211_ATTR_IFINDEX, (uint32_t)ifindex)) {
		log_error(-1, "failed to set interface index\n");
		nlmsg_free(msg);
		return NULL;
	}

	return msg;
}

/**
 * get basic capabilities from the driver
 * using the NL80211_CMD_GET_WIPHY command
 */
static bool get_basic_wt_capabilities(struct wt_state *state)
{
	bool res = false;
	int rc;
	/* get extended capabilities from driver to see
	 * which vendor commands it supports
	 */
	struct nl_msg *msg = allocate_nl_cmd_msg(state->nl80211_id,
						 state->ifindex,
						 NLM_F_DUMP,
						 NL80211_CMD_GET_WIPHY);
	if (msg == NULL) {
		log_error(-1, "failed to allocate msg for GET_WIPHY\n");
		goto out;
	}

	if (nla_put_flag(msg, NL80211_ATTR_SPLIT_WIPHY_DUMP)) {
		log_error(-1, "failed to set params for GET_WIPHY\n");
		goto out;
	}

	rc = nl_cmd_send_and_recv(state->nl, state->cb, msg,
				  wiphy_info_handler, state);
	if (rc) {
		log_error(rc, "failed to send GET_WIPHY command: %s\n",
			  strerror(-rc));
		goto out;
	}

	res = true;
out:
	nlmsg_free(msg);
	return res;
}

/**
 * handler for QCA_NL80211_VENDOR_SUBCMD_DMG_RF_GET_SECTOR_CFG results
 */
static int wt_get_sector_info_handler(struct nl_msg *msg, void *arg)
{
	struct wt_state *state = (struct wt_state *)arg;
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct nlattr *tb1[QCA_WLAN_VENDOR_ATTR_MAX + 1];
	struct nlattr *tb2[QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_CFG_MAX + 1];
	struct nlattr *cfg;
	struct genlmsghdr *gnlh =
		(struct genlmsghdr *)nlmsg_data(nlmsg_hdr(msg));
	int tmp, i;

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (!tb[NL80211_ATTR_VENDOR_DATA])
		return NL_SKIP;

	if (nla_parse_nested(tb1, QCA_WLAN_VENDOR_ATTR_MAX,
			     tb[NL80211_ATTR_VENDOR_DATA],
			     nl80211_qca_policy)) {
		log_error(-1, "failed to parse vendor data\n");
		return NL_SKIP;
	}

	if (!tb1[QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_CFG])
		return NL_SKIP;

	if (tb1[QCA_WLAN_VENDOR_ATTR_TSF])
		state->tsf = nla_get_u64(tb1[QCA_WLAN_VENDOR_ATTR_TSF]);

	i = 0;
	nla_for_each_nested(cfg, tb1[QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_CFG],
			    tmp) {
		if (nla_parse_nested(
			tb2, QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_CFG_MAX,
			cfg, nl80211_sector_cfg_policy)) {
			log_error(-1, "fail to parse sector config at %d\n", i);
			return NL_SKIP;
		}
		if (!tb2[QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_CFG_MODULE_INDEX] ||
		    !tb2[QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_CFG_ETYPE0] ||
		    !tb2[QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_CFG_ETYPE1] ||
		    !tb2[QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_CFG_ETYPE2] ||
		    !tb2[QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_CFG_PSH_HI] ||
		    !tb2[QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_CFG_PSH_LO] ||
		    !tb2[QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_CFG_DTYPE_X16]) {
			log_error(-1, "incomplete sector configuration at %d\n", i);
			return NL_SKIP;
		}
		state->sectors[i].rf_module_index = nla_get_u8(
			tb2[QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_CFG_MODULE_INDEX]);
		state->sectors[i].psh_hi = nla_get_u32(
			tb2[QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_CFG_PSH_HI]);
		state->sectors[i].psh_lo = nla_get_u32(
			tb2[QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_CFG_PSH_LO]);
		state->sectors[i].etype0 = nla_get_u32(
			tb2[QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_CFG_ETYPE0]);
		state->sectors[i].etype1 = nla_get_u32(
			tb2[QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_CFG_ETYPE1]);
		state->sectors[i].etype2 = nla_get_u32(
			tb2[QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_CFG_ETYPE2]);
		state->sectors[i].dtype_x16 = nla_get_u32(
			tb2[QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_CFG_DTYPE_X16]);
		i++;
	}

	state->n_rf_modules = i;
	return NL_SKIP;
}

/**
 * handler for QCA_NL80211_VENDOR_SUBCMD_DMG_RF_GET_SELECTED_SECTOR results
 */
static int wt_get_selected_sector_info_handler(struct nl_msg *msg, void *arg)
{
	struct wt_state *state = (struct wt_state *)arg;
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct nlattr *tb1[QCA_WLAN_VENDOR_ATTR_MAX + 1];
	struct genlmsghdr *gnlh =
		(struct genlmsghdr *)nlmsg_data(nlmsg_hdr(msg));

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (!tb[NL80211_ATTR_VENDOR_DATA])
		return NL_SKIP;

	if (nla_parse_nested(tb1, QCA_WLAN_VENDOR_ATTR_MAX,
			     tb[NL80211_ATTR_VENDOR_DATA],
			     nl80211_qca_policy)) {
		log_error(-1, "failed to parse vendor data\n");
		return NL_SKIP;
	}

	if (!tb1[QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_INDEX])
		return NL_SKIP;

	state->selected_sector = nla_get_u16(
		tb1[QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_INDEX]);
	state->has_selected_sector = true;

	if (tb1[QCA_WLAN_VENDOR_ATTR_TSF])
		state->tsf = nla_get_u64(tb1[QCA_WLAN_VENDOR_ATTR_TSF]);

	return NL_SKIP;
}

static void wt_cmd_get_help(void)
{
	log_printf("get sector <sector_index> <rx|tx> <rf_module_mask>\n");
	log_printf("get selected_sector <mac_addr|default> <rx|tx>\n");
}

static void wt_cmd_get_sector(struct wt_state *state, int argc, char *argv[])
{
	uint16_t sector_index;
	uint8_t sector_type;
	uint32_t rf_module_vec;
	struct nl_msg *msg;
	struct nlattr *vendor_data;
	int i, rc;

	if (argc < 3) {
		wt_cmd_get_help();
		return;
	}

	if (!state->capa_get_rf_sector_cfg) {
		log_error(-1, "Operation not supported by driver\n");
		return;
	}

	sector_index = (uint16_t)strtoul(argv[0], NULL, 0);
	if (!strcmp("rx", argv[1])) {
		sector_type = QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_TYPE_RX;
	} else if (!strcmp("tx", argv[1])) {
		sector_type = QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_TYPE_TX;
	} else {
		wt_cmd_get_help();
		return;
	}
	rf_module_vec = (uint32_t)strtoul(argv[2], NULL, 0);

	msg = allocate_nl_cmd_msg(state->nl80211_id, state->ifindex,
				  0, NL80211_CMD_VENDOR);
	if (msg == NULL) {
		log_error(-1, "failed to allocate msg for DMG_RF_GET_SECTOR_CFG\n");
		return;
	}

	if (nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
			QCA_NL80211_VENDOR_SUBCMD_DMG_RF_GET_SECTOR_CFG)) {
		log_error(-1, "unable to set vendor id/subcmd for DMG_RF_GET_SECTOR_CFG\n");
		goto out;
	}

	vendor_data = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA);
	if (!vendor_data) {
		log_error(-1, "fail nla_nest_start for vendor_data\n");
		goto out;
	}

	if (nla_put_u16(msg, QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_INDEX,
			sector_index) ||
	    nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_TYPE,
		       sector_type) ||
	    nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_DMG_RF_MODULE_MASK,
			rf_module_vec)) {
		log_error(-1, "unable to set args for DMG_RF_GET_SECTOR_CFG\n");
		goto out;
	}
	nla_nest_end(msg, vendor_data);

	rc = nl_cmd_send_and_recv(state->nl, state->cb, msg,
				  wt_get_sector_info_handler, state);
	if (rc) {
		log_error(rc, "%s\n", strerror(-rc));
		goto out;
	}

	/* print results */
	log_printf("index type rf_module_mask tsf\n");
	log_printf("rf_module_index psh_hi psh_lo etype0 etype1 etype2 dtype_x16\n");
	log_printf("%d %s %d 0x%" PRIx64 "\n", sector_index, argv[1], rf_module_vec,
		   state->tsf);
	for (i = 0; i < state->n_rf_modules; i++)
		log_printf("%d 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X\n",
			   state->sectors[i].rf_module_index,
			   state->sectors[i].psh_hi,
			   state->sectors[i].psh_lo,
			   state->sectors[i].etype0,
			   state->sectors[i].etype1,
			   state->sectors[i].etype2,
			   state->sectors[i].dtype_x16);
out:
	nlmsg_free(msg);
}

static void
wt_cmd_get_selected_sector(struct wt_state *state, int argc, char *argv[])
{
	uint8_t sector_type;
	uint8_t mac_addr[ETH_ALEN];
	bool default_mac;
	struct nl_msg *msg;
	struct nlattr *vendor_data;
	int rc;

	if (argc < 2) {
		wt_cmd_get_help();
		return;
	}

	if (!state->capa_get_rf_sel_sector) {
		log_error(-1, "Operation not supported by driver\n");
		return;
	}

	if (!strcmp("default", argv[0])) {
		default_mac = true;
	} else {
		rc = sscanf(argv[0], "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
			    &mac_addr[0], &mac_addr[1], &mac_addr[2],
			    &mac_addr[3], &mac_addr[4], &mac_addr[5]);
		if (rc != 6) {
			log_error(-1, "invalid MAC address\n");
			return;
		}
		default_mac = false;
	}

	if (!strcmp("rx", argv[1])) {
		sector_type = QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_TYPE_RX;
	} else if (!strcmp("tx", argv[1])) {
		sector_type = QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_TYPE_TX;
	} else {
		wt_cmd_get_help();
		return;
	}

	msg = allocate_nl_cmd_msg(state->nl80211_id, state->ifindex,
				  0, NL80211_CMD_VENDOR);
	if (msg == NULL) {
		log_error(-1, "failed to allocate msg for DMG_RF_GET_SELECTED_SECTOR\n");
		return;
	}

	if (nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
			QCA_NL80211_VENDOR_SUBCMD_DMG_RF_GET_SELECTED_SECTOR)) {
		log_error(-1, "unable to set vendor id/subcmd for DMG_RF_GET_SELECTED_SECTOR\n");
		goto out;
	}

	vendor_data = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA);
	if (!vendor_data) {
		log_error(-1, "fail nla_nest_start for vendor_data\n");
		goto out;
	}

	if ((!default_mac && nla_put(msg, QCA_WLAN_VENDOR_ATTR_MAC_ADDR,
			ETH_ALEN, mac_addr)) ||
	    nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_TYPE,
		       sector_type)) {
		log_error(-1, "unable to set args for DMG_RF_GET_SELECTED_SECTOR\n");
		goto out;
	}
	nla_nest_end(msg, vendor_data);

	rc = nl_cmd_send_and_recv(state->nl, state->cb, msg,
				  wt_get_selected_sector_info_handler, state);
	if (rc) {
		log_error(rc, "%s\n", strerror(-rc));
		goto out;
	}

	/* print results */
	if (state->has_selected_sector)
		log_printf("selected sector: %u\n", state->selected_sector);
	else
		log_error(-1, "selected sector not available\n");
out:
	nlmsg_free(msg);
}

static void wt_cmd_get(struct wt_state *state, int argc, char *argv[])
{
	char *subcmd;

	if (argc < 1) {
		wt_cmd_get_help();
		return;
	}

	subcmd = argv[0];
	argc--;
	argv++;

	if (!strcmp(subcmd, "sector"))
		wt_cmd_get_sector(state, argc, argv);
	else if (!strcmp(subcmd, "selected_sector"))
		wt_cmd_get_selected_sector(state, argc, argv);
	else
		wt_cmd_get_help();
}

static void wt_cmd_set_help(void)
{
	log_printf("set sector <sector_index> <rx|tx> <rf_module_index> <psh_hi> <psh_lo> <etype0> <etype1> <etype2> <dtype_x16> [<rf_module_index> ... [....]]\n");
	log_printf("set selected_sector <mac_addr|all|default> <rx|tx> <sector_index|unlock>\n");
	log_printf("set antenna_limit <mac_addr> <disable|effective|force> <num antennas>\n");
}

static void wt_cmd_set_sector(struct wt_state *state, int argc, char *argv[])
{
	uint16_t sector_index;
	uint8_t sector_type, rf_module_index;
	uint32_t rf_module_vec = 0;
	struct nl_msg *msg;
	struct nlattr *vendor_data, *cfgs, *cfg;
	int i, rc;

	/* sector index, type + configuration for atleast one RF module (7 args) */
	if (argc < 9) {
		wt_cmd_set_help();
		return;
	}

	if (!state->capa_set_rf_sector_cfg) {
		log_error(-1, "Operation not supported by driver\n");
		return;
	}

	sector_index = (uint16_t)strtoul(argv[0], NULL, 0);
	if (!strcmp("rx", argv[1])) {
		sector_type = QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_TYPE_RX;
	} else if (!strcmp("tx", argv[1])) {
		sector_type = QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_TYPE_TX;
	} else {
		wt_cmd_get_help();
		return;
	}

	argc -= 2;
	argv += 2;

	i = 0;
	while (argc >= 7) {
		rf_module_index = (uint8_t)strtoul(argv[0], NULL, 0);
		if (rf_module_index >= WT_MAX_RF_MODULES) {
			log_error(-1, "invalid RF module index: %d\n",
				  rf_module_index);
			return;
		}
		rf_module_vec |= (1 << rf_module_index);
		state->sectors[i].psh_hi =
			(uint32_t)strtoul(argv[1], NULL, 0);
		state->sectors[i].psh_lo =
			(uint32_t)strtoul(argv[2], NULL, 0);
		state->sectors[i].etype0 =
			(uint32_t)strtoul(argv[3], NULL, 0);
		state->sectors[i].etype1 =
			(uint32_t)strtoul(argv[4], NULL, 0);
		state->sectors[i].etype2 =
			(uint32_t)strtoul(argv[5], NULL, 0);
		state->sectors[i].dtype_x16 =
			(uint32_t)strtoul(argv[6], NULL, 0);
		argc -= 7;
		argv += 7;
		i++;
	}

	if (argc > 0) {
		log_error(-1, "extra arguments found\n");
		wt_cmd_set_help();
		return;
	}

	state->n_rf_modules = i;
	msg = allocate_nl_cmd_msg(state->nl80211_id, state->ifindex,
				  0, NL80211_CMD_VENDOR);
	if (msg == NULL) {
		log_error(-1, "failed to allocate msg for DMG_RF_SET_SECTOR_CFG\n");
		return;
	}

	if (nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
			QCA_NL80211_VENDOR_SUBCMD_DMG_RF_SET_SECTOR_CFG)) {
		log_error(-1, "unable to set vendor id/subcmd for DMG_RF_SET_SECTOR_CFG\n");
		goto out;
	}

	vendor_data = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA);
	if (!vendor_data) {
		log_error(-1, "fail nla_nest_start for vendor_data\n");
		goto out;
	}

	if (nla_put_u16(msg, QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_INDEX,
			sector_index) ||
	    nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_TYPE,
		       sector_type)) {
		log_error(-1, "unable to set args for DMG_RF_SET_SECTOR_CFG\n");
		goto out;
	}

	cfgs = nla_nest_start(msg, QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_CFG);
	if (!cfgs) {
		log_error(-1, "fail nla_nest_start for sector configs\n");
		goto out;
	}
	for (i = 0; i < state->n_rf_modules; i++) {
		cfg = nla_nest_start(msg, i);
		if (!cfg) {
			log_error(-1, "fail nla_nest_start index %d\n", i);
			goto out;
		}
		if (nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_CFG_MODULE_INDEX,
			       state->sectors[i].rf_module_index) ||
		    nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_CFG_PSH_HI,
				state->sectors[i].psh_hi) ||
		    nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_CFG_PSH_LO,
				state->sectors[i].psh_lo) ||
		    nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_CFG_ETYPE0,
				state->sectors[i].etype0) ||
		    nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_CFG_ETYPE1,
				state->sectors[i].etype1) ||
		    nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_CFG_ETYPE2,
				state->sectors[i].etype2) ||
		    nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_CFG_DTYPE_X16,
				state->sectors[i].dtype_x16)) {
			log_error(-1, "fail to fill sector cfg, index %d\n", i);
			goto out;
		}
		nla_nest_end(msg, cfg);
	}
	nla_nest_end(msg, cfgs);
	nla_nest_end(msg, vendor_data);

	rc = nl_cmd_send_and_recv(state->nl, state->cb, msg, NULL, state);
	if (rc) {
		log_error(rc, "%s\n", strerror(-rc));
		goto out;
	}

out:
	nlmsg_free(msg);
}

static void
wt_cmd_set_selected_sector(struct wt_state *state, int argc, char *argv[])
{
	uint16_t sector_index;
	uint8_t sector_type;
	uint8_t mac_addr[ETH_ALEN];
	bool has_mac_addr;
	struct nl_msg *msg;
	struct nlattr *vendor_data;
	int rc;

	if (argc < 3) {
		wt_cmd_set_help();
		return;
	}

	if (!state->capa_set_rf_sel_sector) {
		log_error(-1, "Operation not supported by driver\n");
		return;
	}

	if (!strcmp("default", argv[0])) {
		has_mac_addr = false;
	} else if (!strcmp("all", argv[0])) {
		/* broadcast MAC address */
		memset(mac_addr, 0xff, sizeof(mac_addr));
		has_mac_addr = true;
	} else {
		rc = sscanf(argv[0], "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
			    &mac_addr[0], &mac_addr[1], &mac_addr[2],
			    &mac_addr[3], &mac_addr[4], &mac_addr[5]);
		if (rc != 6) {
			log_error(-1, "invalid MAC address\n");
			return;
		}
		has_mac_addr = true;
	}

	if (!strcmp("rx", argv[1])) {
		sector_type = QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_TYPE_RX;
	} else if (!strcmp("tx", argv[1])) {
		sector_type = QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_TYPE_TX;
	} else {
		wt_cmd_get_help();
		return;
	}

	if (!strcmp("unlock", argv[2]))
		sector_index = WT_UNLOCK_SECTOR_INDEX;
	else
		sector_index = (uint16_t)strtoul(argv[2], NULL, 0);

	msg = allocate_nl_cmd_msg(state->nl80211_id, state->ifindex,
				  0, NL80211_CMD_VENDOR);
	if (msg == NULL) {
		log_error(-1, "failed to allocate msg for DMG_RF_SET_SELECTED_SECTOR\n");
		return;
	}

	if (nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
			QCA_NL80211_VENDOR_SUBCMD_DMG_RF_SET_SELECTED_SECTOR)) {
		log_error(-1, "unable to set vendor id/subcmd for DMG_RF_SET_SELECTED_SECTOR\n");
		goto out;
	}

	vendor_data = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA);
	if (!vendor_data) {
		log_error(-1, "fail nla_nest_start for vendor_data\n");
		goto out;
	}

	if ((has_mac_addr && nla_put(
		msg, QCA_WLAN_VENDOR_ATTR_MAC_ADDR, ETH_ALEN, mac_addr)) ||
	    nla_put_u16(msg, QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_INDEX,
			sector_index) ||
	    nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_DMG_RF_SECTOR_TYPE,
		       sector_type)) {
		log_error(-1, "unable to set args for DMG_RF_SET_SELECTED_SECTOR\n");
		goto out;
	}

	nla_nest_end(msg, vendor_data);

	rc = nl_cmd_send_and_recv(state->nl, state->cb, msg, NULL, state);
	if (rc) {
		log_error(rc, "%s\n", strerror(-rc));
		goto out;
	}

out:
	nlmsg_free(msg);
}

static void
wt_cmd_set_antenna_limit(struct wt_state *state, int argc, char *argv[])
{
	uint8_t limit_mode;
	uint8_t antenna_limit = 0;
	uint8_t mac_addr[ETH_ALEN];
	struct nl_msg *msg;
	struct nlattr *vendor_data;
	int rc;
	if (argc < 2) {
		wt_cmd_set_help();
		return;
	}

	rc = sscanf(argv[0], "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
		    &mac_addr[0], &mac_addr[1], &mac_addr[2],
		    &mac_addr[3], &mac_addr[4], &mac_addr[5]);
	if (rc != 6) {
		log_error(-1, "invalid MAC address\n");
		return;
	}

	if (!strcmp("disable", argv[1])) {
		limit_mode = QCA_WLAN_VENDOR_ATTR_BRP_ANT_LIMIT_MODE_DISABLE;
	} else if (!strcmp("effective", argv[1])) {
		limit_mode = QCA_WLAN_VENDOR_ATTR_BRP_ANT_LIMIT_MODE_EFFECTIVE;
	} else if (!strcmp("force", argv[1])) {
		limit_mode = QCA_WLAN_VENDOR_ATTR_BRP_ANT_LIMIT_MODE_FORCE;
	} else {
		wt_cmd_get_help();
		return;
	}

	if (limit_mode != QCA_WLAN_VENDOR_ATTR_BRP_ANT_LIMIT_MODE_DISABLE) {
		antenna_limit = (uint8_t)strtoul(argv[2], NULL, 0);
		if (antenna_limit > WT_MAX_ANTENNAS ||
		    antenna_limit < WT_MIN_ANTENNAS) {
			log_error(-1, "invalid antenna limit: %d\n",
				  antenna_limit);
			return;
		}
	}

	msg = allocate_nl_cmd_msg(state->nl80211_id, state->ifindex,
				  0, NL80211_CMD_VENDOR);
	if (msg == NULL) {
		log_error(-1, "failed to allocate msg for BRP_SET_ANT_LIMIT\n");
		return;
	}

	if (nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
			QCA_NL80211_VENDOR_SUBCMD_BRP_SET_ANT_LIMIT)) {
		log_error(-1, "unable to set vendor id/subcmd for BRP_SET_ANT_LIMIT\n");
		goto out;
	}

	vendor_data = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA);
	if (!vendor_data) {
		log_error(-1, "fail nla_nest_start for vendor_data\n");
		goto out;
	}
	if (nla_put(msg, QCA_WLAN_VENDOR_ATTR_MAC_ADDR, ETH_ALEN, mac_addr) ||
	    nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_BRP_ANT_LIMIT_MODE,
		       limit_mode) ||
	    nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_BRP_ANT_NUM_LIMIT,
		       antenna_limit)) {
		log_error(-1, "unable to set args for BRP_SET_ANT_LIMIT\n");
		goto out;
	}
	nla_nest_end(msg, vendor_data);

	rc = nl_cmd_send_and_recv(state->nl, state->cb, msg, NULL, state);
	if (rc) {
		log_error(rc, "%s\n", strerror(-rc));
		goto out;
	}
out:
	nlmsg_free(msg);
}

static void wt_cmd_set(struct wt_state *state, int argc, char *argv[])
{
	char *subcmd;

	if (argc < 1) {
		wt_cmd_set_help();
		return;
	}

	subcmd = argv[0];
	argc--;
	argv++;

	if (!strcmp(subcmd, "sector"))
		wt_cmd_set_sector(state, argc, argv);
	else if (!strcmp(subcmd, "selected_sector"))
		wt_cmd_set_selected_sector(state, argc, argv);
	else if (!strcmp(subcmd, "antenna_limit"))
		wt_cmd_set_antenna_limit(state, argc, argv);
	else
		wt_cmd_set_help();
}

/**
 * available commands
 */
static struct wt_cmd {
	const char *name;
	void (*handler)(struct wt_state *state, int argc, char *argv[]);
	void (*print_help)(void);
} wt_cmds[] = {
	{ "get", wt_cmd_get, wt_cmd_get_help },
	{ "set", wt_cmd_set, wt_cmd_set_help},
	{ NULL, NULL, NULL } /* keep last */
};

int main(int argc, char *argv[])
{
	const char *ifname;
	const char *name;
	struct wt_cmd *cmd;
	bool handled = false;
	bool print_help = false;
	int f, optidx = 0;
	static const struct option options[] = {
		{ "help", no_argument, 0, 'h' },
		{ 0, 0, 0, 0 }
	};

	memset(&g_state, 0, sizeof(g_state));
	while ((f = getopt_long(argc, argv, "h", options, &optidx)) != EOF)
		switch (f) {
		case 'h':
			print_help = true;
			break;
		default:
			fprintf(stderr, "Unknown option '%c'\n", f);
			goto help;
		}

	argc -= optind;
	argv = &argv[optind];

	if (argc < 3 || print_help) {
help:
		log_printf("wigigrftool version %s\n", WIGIGRFTOOL_VERSION);
		log_printf("Usage: wigigrftool [options] interface_name cmd [args]\n");
		log_printf("Options:\n");
		log_printf(" -h | --help : print this message\n");
		log_printf("Commands:\n");
		for (cmd = wt_cmds; cmd->print_help; cmd++)
			cmd->print_help();
		return -1;
	}

	ifname = argv[0];
	name = argv[1];

	argc -= 2;
	argv += 2;

	g_state.ifindex = if_nametoindex(ifname);
	if (g_state.ifindex == 0) {
		log_error(-1, "unknown interface %s\n", ifname);
		return -1;
	}

	if (!init_nl_globals(&g_state))
		return -1;

	if (!get_basic_wt_capabilities(&g_state))
		return -1;

	for (cmd = wt_cmds; cmd->name; cmd++) {
		if (!strcmp(name, cmd->name)) {
			cmd->handler(&g_state, argc, argv);
			handled = true;
			break;
		}
	}

	if (!handled)
		log_error(-1, "unknown command: %s\n", name);

	destroy_nl_globals(&g_state);
	return 0;
}

