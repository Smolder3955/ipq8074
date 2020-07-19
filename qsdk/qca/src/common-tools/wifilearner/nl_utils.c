/*
 * Copyright (c) 2018 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * Driver interaction with Linux nl80211/cfg80211
 * Copyright (c) 2002-2015, Jouni Malinen <j@w1.fi>
 * Copyright (c) 2003-2004, Instant802 Networks, Inc.
 * Copyright (c) 2005-2006, Devicescape Software, Inc.
 * Copyright (c) 2007, Johannes Berg <johannes@sipsolutions.net>
 * Copyright (c) 2009-2010, Atheros Communications
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */


#include <string.h>
#include "nl_utils.h"
#include "utils.h"

#define NL_SOCK_BUF_SIZE (32 * 1024)

/*
 * Function     : nl80211_cmd_msg
 * Description  : creates netlink message with given netlink command.
 * Input params : pointer to wifilearner_ctx, netlink flags, netlink command
 * Return       : pointer to netlink message/NULL
 */
static struct nl_msg *nl80211_cmd_msg(struct wifilearner_ctx *wlc,
				      int flags, uint8_t cmd)
{
	struct nl_msg *msg;
	struct nl80211_ctx *ctx = wlc->nl_ctx;

	msg = nlmsg_alloc();
	if (!msg) {
		wl_print(wlc, WL_MSG_ERROR, "Failed to allocate NL message");
		return NULL;
	}

	if (!genlmsg_put(msg, 0, 0, ctx->netlink_familyid, 0, flags, cmd, 0)) {
		wl_print(wlc, WL_MSG_ERROR, "Failed to put cmd in nl msg");
		nlmsg_free(msg);
		return NULL;
	}

	return msg;
}

static int ack_handler(struct nl_msg *msg, void *arg)
{
	int *err = arg;

	*err = 0;
	return NL_STOP;
}

static int finish_handler(struct nl_msg *msg, void *arg)
{
	int *ret = arg;

	*ret = 0;
	return NL_SKIP;
}

static int error_handler(struct sockaddr_nl *nla, struct nlmsgerr *err,
			 void *arg)
{
	int *ret = arg;

	*ret = err->error;
	return NL_SKIP;
}

/*
 * Function     : send_and_recv_msgs
 * Description  : sends given netlink message and receives response.
 * Input params : pointer to wifilearner_ctx, pointer to nl_msg,
 *                pointer to valid_handler for receiving netlink response.
 * Out params   : pointer to valid_data for extracting required information
 *                from netlink response.
 * Return       : SUCCESS/FAILURE
 */
int send_and_recv_msgs(struct wifilearner_ctx *wlc, struct nl_msg *nlmsg,
		       int (*valid_handler)(struct nl_msg *, void *),
		       void *valid_data)
{
	struct nl_cb *cb;
	struct nl80211_ctx *ctx = wlc->nl_ctx;
	int err;

	if (!nlmsg)
		return -EINVAL;

	cb = nl_cb_alloc(NL_CB_DEFAULT);
	if (!cb) {
		err = -ENOMEM;
		goto out;
	}

	err = nl_send_auto_complete(ctx->sock, nlmsg);
	if (err < 0) {
		wl_print(wlc, WL_MSG_ERROR, "nl80211: failed to send err=%d",
			 err);
		goto out;
	}

	err = 1;

	nl_cb_err(cb, NL_CB_CUSTOM, error_handler, &err);
	nl_cb_set(cb, NL_CB_FINISH, NL_CB_CUSTOM, finish_handler, &err);
	nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, ack_handler, &err);

	if (valid_handler)
		nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, valid_handler,
			  valid_data);

	while (err > 0) {
		int res = nl_recvmsgs(ctx->sock, cb);

		if (res < 0) {
			wl_print(wlc, WL_MSG_ERROR,
				 "nl80211: %s->nl_recvmsgs failed: res=%d, err=%d",
				  __func__, res, err);
		}
	}

out:
	nl_cb_put(cb);
	if (!valid_handler && valid_data == (void *) -1) {
		if (nlmsg) {
			struct nlmsghdr *hdr = nlmsg_hdr(nlmsg);
			void *data = nlmsg_data(hdr);
			int len = hdr->nlmsg_len - NLMSG_HDRLEN;

			memset(data, 0, len);
		}
	}

	nlmsg_free(nlmsg);
	return err;
}

/*
 * Function     : nl80211_init
 * Description  : initialization of netlink communication.
 * Input params : pointer to wifilearner_ctx
 * Out params   : pointer to wifilearner_ctx with netlink generic socket info
 * Return       : SUCCESS/FAILURE
 */
int nl80211_init(struct wifilearner_ctx *wlc)
{
	struct nl80211_ctx *ctx;

	wlc->nl_ctx = NULL;
	ctx = calloc(1, sizeof(struct nl80211_ctx));
	if (!ctx) {
		wl_print(wlc, WL_MSG_ERROR, "Failed to alloc nl80211_ctx");
		return -1;
	}

	ctx->sock = nl_socket_alloc();
	if (!ctx->sock) {
		wl_print(wlc, WL_MSG_ERROR, "Failed to create NL socket (%s)",
			 strerror(errno));
		goto cleanup;
	}

	if (nl_connect(ctx->sock, NETLINK_GENERIC)) {
		wl_print(wlc, WL_MSG_ERROR, "Could not connect socket (%s)",
			 strerror(errno));
		goto cleanup;
	}

	if (nl_socket_set_buffer_size(ctx->sock, NL_SOCK_BUF_SIZE, 0) < 0) {
		wl_print(wlc, WL_MSG_INFO,
			 "Could not set nl_socket RX buffer size for sock (%s)",
			 strerror(errno));
	}

	ctx->netlink_familyid = genl_ctrl_resolve(ctx->sock, "nl80211");
	if (ctx->netlink_familyid < 0) {
		wl_print(wlc, WL_MSG_ERROR,
			 "Could not resolve nl80211 family id");
		goto cleanup;
	}

	ctx->nlctrl_familyid = genl_ctrl_resolve(ctx->sock, "nlctrl");
	if (ctx->nlctrl_familyid < 0) {
		wl_print(wlc, WL_MSG_ERROR,
				 "net link family nlctrl is not present: %d err:%s",
				 ctx->nlctrl_familyid, strerror(errno));
		goto cleanup;
	}

	wlc->nl_ctx = ctx;
	return 0;

cleanup:
	if (ctx->sock)
		nl_socket_free(ctx->sock);
	free(ctx);
	return -1;
}

/*
 * Function     : nl80211_deinit
 * Description  : deinitialization netlink communication.
 * Input params : pointer to wifilearner_ctx
 * Return       : void
 */
void nl80211_deinit(struct wifilearner_ctx *wlc)
{
	struct nl80211_ctx *ctx = wlc->nl_ctx;

	if (!ctx) {
		wl_print(wlc, WL_MSG_ERROR, "%s: ctx is NULL", __func__);
		return;
	}

	if (ctx->sock)
		nl_socket_free(ctx->sock);
	free(ctx);
}

/*
 * Function     : get_iface_index_handler
 * Description  : handler for get_iface_index
 * Input params : netlink message
 * Out params   : pointer to iface_info data
 * Return       : netlink handler enum entry
 */
static int get_iface_index_handler(struct nl_msg *msg, void *arg)
{
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *tb_msg[NL80211_ATTR_MAX + 1];
	const char *ifname = "";
	unsigned int ifindex;
	struct iface_info *ifinfo = arg;

	nla_parse(tb_msg, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
			  genlmsg_attrlen(gnlh, 0), NULL);

	if (tb_msg[NL80211_ATTR_IFNAME]) {
		ifname = nla_get_string(tb_msg[NL80211_ATTR_IFNAME]);
	} else {
		return NL_SKIP;
	}

	if (tb_msg[NL80211_ATTR_IFINDEX]) {
		ifindex = nla_get_u32(tb_msg[NL80211_ATTR_IFINDEX]);
	} else {
		return NL_SKIP;
	}

	if (strncmp(ifname, ifinfo->name, IFNAMSIZ-1) == 0) {
		ifinfo->index = ifindex;
		ifinfo->found = 1;
	}

	return NL_SKIP;
}

/*
 * Function     : get_iface_index
 * Description  : function to get interface index from interface name.
 *                ifinfo.name should be the interface name for which
 *                interface index needs to be fetched.
 * Input params : pointer to wifilearner_ctx,
 *                pointer iface_info with ifname filled
 * Out params   : pointer to iface_info with interface index info.
 * Return       : SUCCESS/FAILURE
 *
 */
static int get_iface_index(struct wifilearner_ctx *wlc, const char *ifname)
{
	int ret;
	struct nl_msg *msg;
	struct iface_info ifinfo;

	ifinfo.name = ifname;
	ifinfo.found = 0;

	msg = nl80211_cmd_msg(wlc, NLM_F_DUMP, NL80211_CMD_GET_INTERFACE);
	if (msg == NULL) {
		wl_print(wlc, WL_MSG_ERROR, "%s: err in adding commnd",
			 __func__);
		nlmsg_free(msg);
		return -1;
	}

	ret = send_and_recv_msgs(wlc, msg, get_iface_index_handler, &ifinfo);
	if (ret) {
		wl_print(wlc, WL_MSG_ERROR,
			 "%s: err in send_and_recv_msgs, ret=%d",
			 __func__, ret);
		return -1;
	}

	if (ifinfo.found == 0) {
		wl_print(wlc, WL_MSG_ERROR, "%s interface not found.",
			 ifinfo.name);
		return -1;
	}

	return ifinfo.index;
}

/*
 * Function     : get_sta_info_handler
 * Description  : handler for get_iface_stats
 * Input params : netlink message
 * Out params   : pointer to station_info data
 * Return       : netlink handler enum entry
 */
static int get_sta_info_handler(struct nl_msg *msg, void *arg)
{
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct station_info *data = arg;
	struct nlattr *stats[NL80211_STA_INFO_MAX + 1];
	static struct nla_policy stats_policy[NL80211_STA_INFO_MAX + 1] = {
		[NL80211_STA_INFO_TX_BITRATE] = { .type = NLA_NESTED },
		[NL80211_STA_INFO_RX_BITRATE] = { .type = NLA_NESTED },
	};
	struct nlattr *rate[NL80211_RATE_INFO_MAX + 1];
	static struct nla_policy rate_policy[NL80211_RATE_INFO_MAX + 1] = {
		[NL80211_RATE_INFO_BITRATE] = { .type = NLA_U16 },
		[NL80211_RATE_INFO_BITRATE32] = { .type = NLA_U32 },
	};

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
			genlmsg_attrlen(gnlh, 0), NULL);

	if (!tb[NL80211_ATTR_STA_INFO]) {
		wl_print(NULL, WL_MSG_ERROR, "sta stats missing!");
		return NL_SKIP;
	}
	if (nla_parse_nested(stats, NL80211_STA_INFO_MAX,
				tb[NL80211_ATTR_STA_INFO],
				stats_policy)) {
		wl_print(NULL, WL_MSG_ERROR,
			 "failed to parse nested attributes!");
		return NL_SKIP;
	}

	if (stats[NL80211_STA_INFO_TX_BITRATE] &&
	    nla_parse_nested(rate, NL80211_RATE_INFO_MAX,
			     stats[NL80211_STA_INFO_TX_BITRATE],
			     rate_policy) == 0) {
		if (rate[NL80211_RATE_INFO_BITRATE32]) {
			data->current_tx_rate =
				nla_get_u32(rate[NL80211_RATE_INFO_BITRATE32]);
			data->filled |= NL_STATS_HAS_TX_RATE;
		} else if (rate[NL80211_RATE_INFO_BITRATE]) {
			data->current_tx_rate =
				nla_get_u16(rate[NL80211_RATE_INFO_BITRATE]);
			data->filled |= NL_STATS_HAS_TX_RATE;
		}
	}

	if (stats[NL80211_STA_INFO_RX_BITRATE] &&
	    nla_parse_nested(rate, NL80211_RATE_INFO_MAX,
			     stats[NL80211_STA_INFO_RX_BITRATE],
			     rate_policy) == 0) {
		if (rate[NL80211_RATE_INFO_BITRATE32]) {
			data->current_rx_rate =
				nla_get_u32(rate[NL80211_RATE_INFO_BITRATE32]);
			data->filled |= NL_STATS_HAS_RX_RATE;
		} else if (rate[NL80211_RATE_INFO_BITRATE]) {
			data->current_rx_rate =
				nla_get_u16(rate[NL80211_RATE_INFO_BITRATE]);
			data->filled |= NL_STATS_HAS_RX_RATE;
		}
	}

	return NL_SKIP;
}

/*
 * Function     : get_iface_stats
 * Description  : get peer station stats on given interface.
 * Input params : pointer to wifilearner_ctx, interface name, peer mac address
 * Out params   : pointer to station_info data
 * Return       : SUCCESS/FAILURE
 *
 */
int get_iface_stats(struct wifilearner_ctx *wlc, const char *ifname,
		    const unsigned char *peer_mac,
		    struct station_info *sta_data)
{
	int ret;
	struct nl_msg *msg;
	int ifindex;

	ifindex = get_iface_index(wlc, ifname);
	if (ifindex < 0)
		return -1;

	memset(sta_data, 0, sizeof(struct station_info));

	msg = nl80211_cmd_msg(wlc, peer_mac ? NLM_F_DUMP : 0,
			      NL80211_CMD_GET_STATION);
	if (msg == NULL) {
		wl_print(wlc, WL_MSG_ERROR, "%s: Failed to add nl command",
			 __func__);
		return -1;
	}

	if (nla_put_u32(msg, NL80211_ATTR_IFINDEX, ifindex) ||
	    (peer_mac && nla_put(msg, NL80211_ATTR_MAC, ETH_ALEN, peer_mac))) {
		wl_print(wlc, WL_MSG_ERROR, "%s: Failed to put attrs",
			 __func__);
		nlmsg_free(msg);
		return -1;
	}

	ret = send_and_recv_msgs(wlc, msg, get_sta_info_handler, sta_data);
	if (ret) {
		wl_print(wlc, WL_MSG_ERROR,
			 "%s: err in send_and_recv_msgs, ret=%d",
			 __func__, ret);
		return -1;
	}

	return 0;
}

/*
 * Function     : get_survey_dump_handler
 * Description  : handler for get_survey_dump_results
 * Input params : netlink message
 * Out params   : pointer to survey_info data
 * Return       : netlink handler enum entry
 */
static int get_survey_dump_handler(struct nl_msg *msg, void *arg)
{
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *sinfo[NL80211_SURVEY_INFO_MAX + 1];
	static struct nla_policy survey_policy[NL80211_SURVEY_INFO_MAX + 1] = {
		[NL80211_SURVEY_INFO_FREQUENCY] = { .type = NLA_U32 },
		[NL80211_SURVEY_INFO_NOISE] = { .type = NLA_U8 },
	};
	struct survey_info *info = arg;

	if (info->count >= MAX_SURVEY_CHANNELS)
		return NL_STOP;

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (!tb[NL80211_ATTR_SURVEY_INFO]) {
		wl_print(NULL, WL_MSG_ERROR, "nl80211: Survey data missing");
		return NL_SKIP;
	}

	if (nla_parse_nested(sinfo, NL80211_SURVEY_INFO_MAX,
			     tb[NL80211_ATTR_SURVEY_INFO], survey_policy)) {
		wl_print(NULL, WL_MSG_ERROR,
			 "nl80211: Failed to parse nested attributes");
		return NL_SKIP;
	}

	if (!sinfo[NL80211_SURVEY_INFO_FREQUENCY])
		return NL_SKIP;

	info->freq[info->count] =
		nla_get_u32(sinfo[NL80211_SURVEY_INFO_FREQUENCY]);

	if (sinfo[NL80211_SURVEY_INFO_NOISE]) {
		info->noise[info->count] =
			(int8_t) nla_get_u8(sinfo[NL80211_SURVEY_INFO_NOISE]);
		info->filled[info->count] |= NL_SURVEY_HAS_NOISE;
	}

	if (sinfo[NL80211_SURVEY_INFO_IN_USE])
		info->in_use[info->count] = 1;

	if (sinfo[NL80211_SURVEY_INFO_TIME]) {
		info->time[info->count] =
				nla_get_u64(sinfo[NL80211_SURVEY_INFO_TIME]);
		info->filled[info->count] |= NL_SURVEY_HAS_CHAN_TIME;
	}
	if (sinfo[NL80211_SURVEY_INFO_TIME_BUSY]) {
		info->time_busy[info->count] =
			nla_get_u64(sinfo[NL80211_SURVEY_INFO_TIME_BUSY]);
		info->filled[info->count] |= NL_SURVEY_HAS_CHAN_TIME_BUSY;
	}
	if (sinfo[NL80211_SURVEY_INFO_TIME_EXT_BUSY]) {
		info->time_ext_busy[info->count] =
			nla_get_u64(sinfo[NL80211_SURVEY_INFO_TIME_EXT_BUSY]);
		info->filled[info->count] |= NL_SURVEY_HAS_CHAN_TIME_EXT_BUSY;
	}
	if (sinfo[NL80211_SURVEY_INFO_TIME_RX]) {
		info->time_rx[info->count] =
			nla_get_u64(sinfo[NL80211_SURVEY_INFO_TIME_RX]);
		info->filled[info->count] |= NL_SURVEY_HAS_CHAN_TIME_RX;
	}
	if (sinfo[NL80211_SURVEY_INFO_TIME_TX]) {
		info->time_tx[info->count] =
			nla_get_u64(sinfo[NL80211_SURVEY_INFO_TIME_TX]);
		info->filled[info->count] |= NL_SURVEY_HAS_CHAN_TIME_TX;
	}
	if (sinfo[NL80211_SURVEY_INFO_TIME_SCAN]) {
		info->time_scan[info->count] =
			nla_get_u64(sinfo[NL80211_SURVEY_INFO_TIME_SCAN]);
		info->filled[info->count] |= NL_SURVEY_HAS_CHAN_TIME_SCAN;
	}

	info->count++;

	return NL_SKIP;
}

/*
 * Function     : get_survey_dump_results
 * Description  : get sutvey dump results/CCA.
 * Input params : pointer to wifilearner_ctx, interface name
 * Out params   : pointer to survey_info data
 * Return       : SUCCESS/FAILURE
 *
 */
int get_survey_dump_results(struct wifilearner_ctx *wlc, const char *ifname,
			    struct survey_info *survey_data)
{
	int ret;
	struct nl_msg *msg;
	int ifindex;

	ifindex = get_iface_index(wlc, ifname);
	if (ifindex < 0)
		return -1;

	memset(survey_data, 0, sizeof(struct survey_info));

	msg = nl80211_cmd_msg(wlc, NLM_F_DUMP, NL80211_CMD_GET_SURVEY);
	if (msg == NULL) {
		wl_print(wlc, WL_MSG_ERROR, "%s: Failed to add nl command",
			 __func__);
		return -1;
	}

	if (nla_put_u32(msg, NL80211_ATTR_IFINDEX, ifindex)) {
		wl_print(wlc, WL_MSG_ERROR, "%s: Failed to put ifindex",
			 __func__);
		nlmsg_free(msg);
		return -1;
	}

	ret = send_and_recv_msgs(wlc, msg, get_survey_dump_handler,
				 survey_data);
	if (ret) {
		wl_print(wlc, WL_MSG_ERROR,
				"%s: err in send_and_recv_msgs, ret=%d",
				__func__, ret);
		return -1;
	}

	return 0;
}
