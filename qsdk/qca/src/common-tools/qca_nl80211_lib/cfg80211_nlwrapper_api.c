/*
 * Copyright (c) 2016, 2017 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include "../vendorcmdtool/nl80211_copy.h"
#include "cfg80211_nlwrapper_pvt.h"
#include "cfg80211_nlwrapper_api.h"
#include "qca-vendor.h"


static const unsigned NL80211_ATTR_MAX_INTERNAL = 256;
struct wifi_cfg80211_t *nl80211_ctx;

/* Other event handlers */
/**
 * valid_handler: valid handler
 * @msg: pointer to struct nl_msg
 * @arg: argument
 *
 * return NL state.
 */

int valid_handler(struct nl_msg *msg, void *arg)
{
	int *err = (int *)arg;
	*err = 0;
	return NL_SKIP;
}

/**
 * ack_handler: ack handler
 * @msg: pointer to struct nl_msg
 * @arg: argument
 *
 * return NL state.
 */

int ack_handler(struct nl_msg *msg, void *arg)
{
	int *err = (int *)arg;
	*err = 0;
	return NL_STOP;
}

/**
 * finish_handler: finish handler
 * @msg: pointer to struct nl_msg
 * @arg: argument
 *
 * return NL state.
 */

int finish_handler(struct nl_msg *msg, void *arg)
{
	int *ret = (int *)arg;
	*ret = 0;
	return NL_SKIP;
}

/**
 * error_handler: error handler
 * @msg: pointer to struct sockaddr
 * @err: pointer to nlmsgerr
 * @arg: argument
 *
 * return NL state.
 */

int error_handler(struct sockaddr_nl *nla, struct nlmsgerr *err, void *arg)
{
	int *ret = (int *)arg;
	*ret = err->error;
	printf("Error received: %d \n", err->error);
	if (err->error > 0) {
		*ret = -(err->error);
	}
	return NL_SKIP;
}

/* Event handlers */
/**
 * response_handler: response handler
 * @msg: pointer to struct nl_msg
 * @data: data
 *
 * return NL state.
 */

int response_handler(struct nl_msg *msg, void *data)
{
	int i, res;
	struct genlmsghdr *header = NULL;
	struct nlattr *attributes[NL80211_ATTR_MAX_INTERNAL + 1];
	struct nlattr *attr_vendor[NL80211_ATTR_MAX_INTERNAL];
	char *vendata = NULL;
	int datalen = 0;
	size_t response_len = 0;
	int result = 0;
	struct cfg80211_data *cfgdata = (struct cfg80211_data *)data;
	u_int32_t *temp = NULL;

	header = (struct genlmsghdr *)nlmsg_data(nlmsg_hdr(msg));
	result = nla_parse(attributes, NL80211_ATTR_MAX_INTERNAL, genlmsg_attrdata(header, 0),
			genlmsg_attrlen(header, 0), NULL);

	if (attributes[NL80211_ATTR_VENDOR_DATA]) {
		vendata = ((char *)nla_data(attributes[NL80211_ATTR_VENDOR_DATA]));
		datalen = nla_len(attributes[NL80211_ATTR_VENDOR_DATA]);
		if (!vendata) {
			fprintf(stderr, "Vendor data not found\n");
			return -EINVAL;
		}
	} else {
		fprintf(stderr, "NL80211_ATTR_VENDOR_DATA not found\n");
		return -EINVAL;
	}

	if (cfgdata->parse_data)  {
		cfgdata->nl_vendordata = vendata;
		cfgdata->nl_vendordata_len = datalen;
		if (cfgdata->callback) {
			cfgdata->callback(cfgdata);
			return NL_OK;
		}
	}

	/* extract data from NL80211_ATTR_VENDOR_DATA attributes */
	nla_parse(attr_vendor, QCA_WLAN_VENDOR_ATTR_PARAM_MAX,
			(struct nlattr *)vendata,
			datalen, NULL);

	if (attr_vendor[QCA_WLAN_VENDOR_ATTR_PARAM_DATA]) {
		/* memcpy tb_vendor to data */
		temp = nla_data(attr_vendor[QCA_WLAN_VENDOR_ATTR_PARAM_DATA]);
		response_len = nla_get_u32(attr_vendor[QCA_WLAN_VENDOR_ATTR_PARAM_LENGTH]);

		if (response_len <= cfgdata->length) {
			memcpy(cfgdata->data, temp, response_len);
		} else {
			cfgdata->data = temp;
		}

		cfgdata->length = response_len;

		if (cfgdata->callback) {
			cfgdata->callback(cfgdata);
		}
	}

	if (attr_vendor[QCA_WLAN_VENDOR_ATTR_PARAM_FLAGS]) {
		cfgdata->flags = nla_get_u32(attr_vendor[QCA_WLAN_VENDOR_ATTR_PARAM_FLAGS]);
	}

	return NL_OK;
}

/**
 * attr_start: start nest attr
 * @nlmsg: pointer to struct nl_msg
 * @attribute : attribute value
 * return pointer to struct nlattr
 */
struct nlattr *attr_start(struct nl_msg *nlmsg, int attribute)
{
	return nla_nest_start(nlmsg, attribute);
}

/**
 * attr_end: end nest attr
 * @nlmsg: pointer to struct nl_msg
 * @attribute : attribute value
 */
void attr_end(struct nl_msg *nlmsg, struct nlattr *attr)
{
	nla_nest_end(nlmsg, attr);
}

/**
 * start_vendor_data: start nest attr
 * @nlmsg: pointer to struct nl_msg
 * return pointer to struct nlattr
 */
struct nlattr *start_vendor_data(struct nl_msg *nlmsg)
{
	return attr_start(nlmsg, NL80211_ATTR_VENDOR_DATA);
}

/**
 * end_vendor_data: end nest attr
 * @nlmsg: pointer to struct nl_msg
 * @attr : pointer to struct nlattr
 */
void end_vendor_data(struct nl_msg *nlmsg, struct nlattr *attr)
{
	attr_end(nlmsg, attr);
}

/**
 * no_seq_check: no sequence check
 * @msg: pointer to struct nl_msg
 * @arg: argument
 *
 * return NL state.
 */
int no_seq_check(struct nl_msg *msg, void *arg)
{
	return NL_OK;
}

/**
 * put_u32: nla_put wrapper
 * @msg: pointer to struct nl_msg
 * @atrribute: atrribute
 * @value: value
 *
 * return NL state.
 */
int put_u32(struct nl_msg *nlmsg, int attribute, uint32_t value)
{
	return nla_put(nlmsg, attribute, sizeof(value), &value);
}

/**
 * set_iface_id: set interface id
 * @msg: pointer to struct nl_msg
 * @iface: pointer to iface
 *
 * return NL state.
 */
int set_iface_id(struct nl_msg *nlmsg, const char *iface)
{
	if (put_u32(nlmsg, NL80211_ATTR_IFINDEX, if_nametoindex(iface))) {
		return -EINVAL;
	}
	return 0;
}

/**
 * send_nlmsg: sends nl msg
 * @ctx: pointer to wifi_cfg80211_context
 * @nlmsg: pointer to nlmsg
 * @data: data
 *
 * return NL state.
 */
int send_nlmsg(wifi_cfg80211_context *ctx, struct nl_msg *nlmsg, void *data)
{
	int err = 0, res = 0;
	struct nl_cb *cb = nl_cb_alloc(NL_CB_DEFAULT);

	if (!cb) {
		err = -1;
		goto out;
	}

	/* send message */
	err = nl_send_auto_complete(ctx->cmd_sock, nlmsg);

	if (err < 0) {
		goto out;
	}
	err = 1;

	nl_cb_set(cb, NL_CB_SEQ_CHECK, NL_CB_CUSTOM, no_seq_check, NULL);
	nl_cb_err(cb, NL_CB_CUSTOM, error_handler, &err);
	nl_cb_set(cb, NL_CB_FINISH, NL_CB_CUSTOM, finish_handler, &err);
	nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, ack_handler, &err);
	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, response_handler, data);

	/*   wait for reply */
	while (err > 0) {  /* error will be set by callbacks */
		res = nl_recvmsgs(ctx->cmd_sock, cb);
		if (res) {
			fprintf(stderr, "nl80211: %s->nl_recvmsgs failed: %d\n", __func__, res);
		}
	}

out:
	if (cb) {
		nl_cb_put(cb);
	}
	if (nlmsg) {
		nlmsg_free(nlmsg);
	}
	return err;
}

struct nl_msg *wifi_cfg80211_prepare_command(wifi_cfg80211_context *ctx, int cmdid, const char *ifname)
{
	int res, ifindex = 0;
	struct nl_msg *nlmsg = nlmsg_alloc();
	if (nlmsg == NULL) {
		fprintf(stderr, "Out of memory\n");
		return NULL;
	}

	genlmsg_put(nlmsg, 0, 0, ctx->nl80211_family_id,
			0, 0, NL80211_CMD_VENDOR, 0);

	res = put_u32(nlmsg, NL80211_ATTR_VENDOR_ID, QCA_VENDOR_OUI);
	if (res < 0) {
		fprintf(stderr, "Failed to put vendor id\n");
		nlmsg_free(nlmsg);
		return NULL;
	}
	/* SET_WIFI_CONFIGURATION = 72 */
	res = put_u32(nlmsg, NL80211_ATTR_VENDOR_SUBCMD, cmdid);
	if (res < 0) {
		fprintf(stderr, "Failed to put vendor sub command\n");
		nlmsg_free(nlmsg);
		return NULL;
	}

	set_iface_id(nlmsg, ifname);

	return nlmsg;
}

int wifi_cfg80211_send_generic_command(wifi_cfg80211_context *ctx, int vendor_command, int cmdid, const char *ifname, char *buffer, int len)
{
	struct nl_msg *nlmsg = NULL;
	int res = -EIO;
	struct nlattr *nl_venData = NULL;
	struct cfg80211_data *cfgdata = (struct cfg80211_data *) buffer;
	nlmsg = wifi_cfg80211_prepare_command(ctx, vendor_command, ifname);

	/* Prepare Actual Payload
	   1. nla_put - command ID.
	   2. nla_put - data
	   3. nla_put length
	   QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_COMMAND,
	   QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_VALUE,
	   QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_DATA,
	   QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_LENGTH,
	   QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_FLAGS,
	 */
	if (nlmsg) {
		nl_venData = (struct nlattr *)start_vendor_data(nlmsg);
		if (!nl_venData) {
			fprintf(stderr, "failed to start vendor data\n");
			nlmsg_free(nlmsg);
			return -EIO;
		}

		if (nla_put_u32(nlmsg, QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_LENGTH, len)) {
			fprintf(stderr, "\n Failed nla_put_u32, \n");
			nlmsg_free(nlmsg);
			return -EIO;
		}
		if (nla_put_u32(nlmsg, QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_COMMAND, cmdid)) {
			nlmsg_free(nlmsg);
			return -EIO;
		}
		if (nla_put(nlmsg, QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_DATA,
			cfgdata->length, cfgdata->data)) {
			nlmsg_free(nlmsg);
			return -EIO;
		} else {
			if (nl_venData) {
				end_vendor_data(nlmsg, nl_venData);
			}
			res = send_nlmsg(ctx, nlmsg, buffer);

			if (res < 0) {
				return res;
			}
			return res;
		}

	} else {
		return -EIO;
	}
	return res;
}

int wifi_cfg80211_send_getparam_command(wifi_cfg80211_context *ctx, int cmdid,
		int param, const char *ifname, char *buffer, int len)
{
	struct nl_msg *nlmsg = NULL;
	int res = -EIO;
	struct nlattr *nl_venData = NULL;
	struct cfg80211_data *cfgdata = (struct cfg80211_data *) buffer;
	nlmsg = wifi_cfg80211_prepare_command(ctx,
			QCA_NL80211_VENDOR_SUBCMD_GET_WIFI_CONFIGURATION,
			ifname);

	/* Prepare Actual Payload
	   1. nla_put - command ID.
	   2. nla_put - data
	   3. nla_put length
	   QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_COMMAND,
	   QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_VALUE,
	 */
	if (nlmsg) {
		nl_venData = (struct nlattr *)start_vendor_data(nlmsg);
		if (!nl_venData) {
			fprintf(stderr, "failed to start vendor data\n");
			nlmsg_free(nlmsg);
			return -EIO;
		}

		if (nla_put_u32(nlmsg,
			QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_COMMAND, cmdid)) {
			nlmsg_free(nlmsg);
			return -EIO;
		}
		if (nla_put_u32(nlmsg,
			QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_VALUE, param)) {
			nlmsg_free(nlmsg);
			return -EIO;
		}

		if (nl_venData) {
			end_vendor_data(nlmsg, nl_venData);
		}
		res = send_nlmsg(ctx, nlmsg, buffer);

		if (res < 0) {
			return res;
		}
		return res;
	} else {
		return -EIO;
	}

	return res;
}

int wifi_cfg80211_send_setparam_command(wifi_cfg80211_context *ctx, int cmdid,
		int param, const char *ifname, char *buffer, int len)
{
	struct nl_msg *nlmsg = NULL;
	int res = -EIO;
	struct nlattr *nl_venData = NULL;
	struct cfg80211_data *cfgdata = (struct cfg80211_data *) buffer;
	nlmsg = wifi_cfg80211_prepare_command(ctx,
			QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_CONFIGURATION,
			ifname);

	/* Prepare Actual Payload
	   1. nla_put - command ID.
	   2. nla_put - data
	   3. nla_put length
	   QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_COMMAND,
	   QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_VALUE,
	 */
	if (nlmsg) {
		nl_venData = (struct nlattr *)start_vendor_data(nlmsg);
		if (!nl_venData) {
			fprintf(stderr, "failed to start vendor data\n");
			nlmsg_free(nlmsg);
			return -EIO;
		}

		if (nla_put_u32(nlmsg,
			QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_COMMAND, cmdid)) {
			nlmsg_free(nlmsg);
			return -EIO;
		}
		if (nla_put_u32(nlmsg,
			QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_VALUE, param)) {
			nlmsg_free(nlmsg);
			return -EIO;
		}

		if (nla_put(nlmsg, QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_DATA,
			cfgdata->length, cfgdata->data)) {
			nlmsg_free(nlmsg);
			return -EIO;
		}

		if (nl_venData) {
			end_vendor_data(nlmsg, nl_venData);
		}
		res = send_nlmsg(ctx, nlmsg, buffer);

		if (res < 0) {
			return res;
		}
		return res;
	} else {
		return -EIO;
	}

	return res;
}

/**
 * wifi_cfg80211_sendcmd: sends cfg80211 sendcmd
 * @ctx: pointer to wifi_cfg80211_context
 * @cmdid: command id
 * @ifname: interface name
 * @buffer: buffer data
 * @len: length
 *
 * return NL state.
 */

int wifi_cfg80211_sendcmd(wifi_cfg80211_context *ctx, int cmdid, const char *ifname,
		char *buffer, int len)
{
	struct nl_msg *nlmsg = NULL;
	int res;
	struct cfg80211_data *cfgdata = (struct cfg80211_data *) buffer;
	nlmsg = wifi_cfg80211_prepare_command(ctx, cmdid, ifname);

	if (nlmsg) {
		if (nla_put(nlmsg, NL80211_ATTR_VENDOR_DATA, cfgdata->length, cfgdata->data)) {
			printf("\n nla_put failed for cmdid %d\n", cmdid);
			fprintf(stderr, "\n Failed nla_put, %d, cmdid %d\n", len, cmdid);
			nlmsg_free(nlmsg);
			return -EIO;
		} else {
			res = send_nlmsg(ctx, nlmsg, buffer);
			if (res < 0) {
				return res;
			}
			return res;
		}
	} else {
		return -ENOMEM;
	}
}

/**
 * wifi_socket_set_local_port: set local port
 * @sock: pointer to nl_sock
 * @port: port
 *
 */
void wifi_socket_set_local_port(struct nl_sock *sock, uint32_t port)
{
#define PID_MASK 0x3FFFFF
#define PORT_SHITF_VAL 22
	uint32_t pid;
	pid = getpid() & PID_MASK;

	if (port == 0) {
		sock->s_flags &= ~NL_OWN_PORT;
	} else {
		sock->s_flags |= NL_OWN_PORT;
	}

	sock->s_local.nl_pid = pid + (port << PORT_SHITF_VAL);
#undef PID_MASK
#undef PORT_SHITF_VAL
}

/**
 * wifi_create_nl_socket: set nl socket
 * @port: pointer to port
 * @protocol: protocol
 *
 * return nl_sock sturct
 */

struct nl_sock *wifi_create_nl_socket(int port, int protocol)
{
	struct nl_sock *sock = nl_socket_alloc();
	struct sockaddr *addr = NULL;
#if CFG80211_DEBUG
	struct sockaddr_nl *addr_nl = &(sock->s_local);
#endif
	if (sock == NULL) {
		fprintf(stderr, "Failed to create NL socket\n");
		return NULL;
	}

	wifi_socket_set_local_port(sock, port);

#if CFG80211_DEBUG
	printf("socket address is %d:%d:%u:%d\n",
			addr_nl->nl_family, addr_nl->nl_pad, addr_nl->nl_pid,
			addr_nl->nl_groups);

	printf("sizeof(sockaddr) = %zu, sizeof(sockaddr_nl) = %zu\n", sizeof(*addr),
			sizeof(*addr_nl));
#endif
	if (nl_connect(sock, protocol)) {
		fprintf(stderr, "Could not connect handle\n");
		nl_socket_free(sock);
		return NULL;
	}

	return sock;
}

void *event_thread (void *arg)
{
	wifi_cfg80211_context *ctx = arg;
	ctx->event_thread_running = 1;
	while (ctx->event_thread_running) {
		nl_recvmsgs_default(ctx->event_sock);
	}
	return NULL;
}


static void nl80211_vendor_event(struct nlattr **tb, char *ifname)
{
	uint32_t vendor_id, subcmd, wiphy = 0;
	uint8_t *data = NULL;
	size_t len = 0;

	if (!tb[NL80211_ATTR_VENDOR_ID] ||
			!tb[NL80211_ATTR_VENDOR_SUBCMD])
		return;

	vendor_id = nla_get_u32(tb[NL80211_ATTR_VENDOR_ID]);
	subcmd = nla_get_u32(tb[NL80211_ATTR_VENDOR_SUBCMD]);

	if (tb[NL80211_ATTR_WIPHY])
		wiphy = nla_get_u32(tb[NL80211_ATTR_WIPHY]);

	if (tb[NL80211_ATTR_VENDOR_DATA]) {
		data = nla_data(tb[NL80211_ATTR_VENDOR_DATA]);
		len = nla_len(tb[NL80211_ATTR_VENDOR_DATA]);
	}


	switch (vendor_id) {
		case OUI_QCA:
			nl80211_ctx->event_callback(ifname, subcmd, data, len);
			break;
	}
}

#define MAX_INTERFACE_NAME_LEN 20
static int process_global_event(struct nl_msg *msg, void *arg)
{
	struct nl80211_global *global = arg;
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	int ifidx = -1;
	char ifname[MAX_INTERFACE_NAME_LEN] = {0};

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
			genlmsg_attrlen(gnlh, 0), NULL);

	if (tb[NL80211_ATTR_IFINDEX])
		ifidx = nla_get_u32(tb[NL80211_ATTR_IFINDEX]);

	if (ifidx != -1) {
		if_indextoname(ifidx, ifname);
	}

	switch (gnlh->cmd) {
		case NL80211_CMD_VENDOR:
			nl80211_vendor_event(tb, ifname);
			break;
	}

	return NL_SKIP;
}

struct handler_args {
	const char *group;
	int id;
};
static int family_handler(struct nl_msg *msg, void *arg)
{
	struct handler_args *grp = arg;
	struct nlattr *tb[CTRL_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *mcgrp;
	int rem_mcgrp;

	nla_parse(tb, CTRL_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
			genlmsg_attrlen(gnlh, 0), NULL);

	if (!tb[CTRL_ATTR_MCAST_GROUPS])
		return NL_SKIP;

	nla_for_each_nested(mcgrp, tb[CTRL_ATTR_MCAST_GROUPS], rem_mcgrp) {
		struct nlattr *tb_mcgrp[CTRL_ATTR_MCAST_GRP_MAX + 1];

		nla_parse(tb_mcgrp, CTRL_ATTR_MCAST_GRP_MAX,
				nla_data(mcgrp), nla_len(mcgrp), NULL);

		if (!tb_mcgrp[CTRL_ATTR_MCAST_GRP_NAME] ||
				!tb_mcgrp[CTRL_ATTR_MCAST_GRP_ID])
			continue;
		else
			grp->id = nla_get_u32(tb_mcgrp[CTRL_ATTR_MCAST_GRP_ID]);

		if (strncmp(nla_data(tb_mcgrp[CTRL_ATTR_MCAST_GRP_NAME]),
				grp->group,
				nla_len(tb_mcgrp[CTRL_ATTR_MCAST_GRP_NAME])))
			continue;

		grp->id = nla_get_u32(tb_mcgrp[CTRL_ATTR_MCAST_GRP_ID]);
		break;
	}

	return NL_SKIP;
}

static int nl_get_multicast_id(struct nl_sock *sock, const char *family,
		const char *group)
{
	struct nl_msg *msg;
	struct nl_cb *cb;
	int ret, ctrlid;
	struct handler_args grp = {
		.group = group,
		.id = -ENOENT,
	};

	msg = nlmsg_alloc();
	if (!msg)
		return -ENOMEM;

	cb = nl_cb_alloc(NL_CB_DEFAULT);
	if (!cb) {
		ret = -ENOMEM;
		goto out_fail_cb;
	}

	ctrlid = genl_ctrl_resolve(sock, "nlctrl");

#ifdef ANDROID
	genlmsg_put(msg, NL_AUTO_PID, NL_AUTO_SEQ, GENL_ID_CTRL, 0, 0,
			CTRL_CMD_GETFAMILY, 1);
#else
	genlmsg_put(msg, 0, 0, ctrlid, 0, 0, CTRL_CMD_GETFAMILY, 0);
#endif

	ret = -ENOBUFS;
	NLA_PUT_STRING(msg, CTRL_ATTR_FAMILY_NAME, family);

	ret = nl_send_auto_complete(sock, msg);
	if (ret < 0)
		goto out;

	ret = 1;

	nl_cb_err(cb, NL_CB_CUSTOM, error_handler, &ret);
	nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, ack_handler, &ret);
	nl_cb_set(cb, NL_CB_FINISH, NL_CB_CUSTOM, finish_handler, &ret);
	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, family_handler, &grp);

	while (ret > 0)
		nl_recvmsgs(sock, cb);

	if (ret == 0)
		ret = grp.id;
nla_put_failure:
out:
	nl_cb_put(cb);
out_fail_cb:
	nlmsg_free(msg);
	return ret;
}

static int setup_event_mechanism(wifi_cfg80211_context *ctx);

/**
 * wifi_init_nl80211: initiliaze nlsocket
 * @ctx: wifi cfg80211 context
 *
 * return 1/0
 */
int wifi_init_nl80211(wifi_cfg80211_context *ctx)
{
	struct nl_sock *cmd_sock = NULL;
	nl80211_ctx = ctx;

	/**
	* Use private command socket if the application asks to create using
	* a custom socket id. This option is set by the application daemons.
	* Use the default command socket if private socket is not specified.
	*/
	if (ctx->pvt_cmd_sock_id > 0) {
		cmd_sock = wifi_create_nl_socket(ctx->pvt_cmd_sock_id, NETLINK_GENERIC);
	} else {
		cmd_sock = wifi_create_nl_socket(NCT_CMD_SOCK_PORT, NETLINK_GENERIC);
	}

	if (cmd_sock == NULL) {
		fprintf(stderr, "Failed to create command socket port\n");
	        return -EIO;
	}

	/* Size of the nl socket buffer*/
#define ALLOC_SIZE (256*1024)
	/* Set the socket buffer size */
	if (nl_socket_set_buffer_size(cmd_sock, (ALLOC_SIZE), 0) < 0) {
		fprintf(stderr, "Could not set nl_socket RX buffer size for cmd_sock: %s\n",
				strerror(errno));
		goto cleanup;
	}
	ctx->cmd_sock = cmd_sock;

	ctx->nl80211_family_id = genl_ctrl_resolve(cmd_sock, "nl80211");
	if (ctx->nl80211_family_id < 0) {
		fprintf(stderr, "Could not resolve nl80211 familty id\n");
		goto cleanup;
	}
	if (setup_event_mechanism(ctx)) {
		goto cleanup;
	}
	return 0;
cleanup:
	ctx->cmd_sock = NULL;
	nl_socket_free(cmd_sock);
	return -EIO;
}

static int setup_event_mechanism(wifi_cfg80211_context *ctx)
{
	struct nl_cb *cb = NULL;
	int err = 0;

	if (!ctx->event_callback) {
		/*
		 * If event_callback is not specified
		 * no need to start event thread and other things
		 */
		return 0;
	}

	/*
	* Use private event socket if the application asks to create using
	* a custom socket id. This option is set by the application daemons.
	* Use the default event socket if private socket is not specified.
	*/
	if (ctx->pvt_event_sock_id > 0) {
		fprintf(stderr, "Create event socket port %d\n", ctx->pvt_event_sock_id);
		ctx->event_sock = wifi_create_nl_socket(ctx->pvt_event_sock_id,
			NETLINK_GENERIC);
	} else {
		ctx->event_sock = wifi_create_nl_socket(NCT_EVENT_SOCK_PORT,
			NETLINK_GENERIC);
	}

	if (ctx->event_sock == NULL) {
		fprintf(stderr, "Failed to create event socket port\n");
		return -EIO;
	}

	/* replace this with genl_ctrl_resolve_grp() once we move to libnl3 */
	err = nl_get_multicast_id(ctx->event_sock, "nl80211", "vendor");
	if (err >= 0) {
		err = nl_socket_add_membership(ctx->event_sock, err);
		if (err) {
			printf("failed to join testmode group!\n");
			goto cleanup;
		}
	} else
		goto cleanup;

	/* Set the socket buffer size */
	if (nl_socket_set_buffer_size(ctx->event_sock, (ALLOC_SIZE), 0) < 0) {
		fprintf(stderr,
		"Could not set nl_socket RX buffer size for event_sock: %s\n",
		strerror(errno));
		goto cleanup;
	}

	/*
	 * Enable peek mode so drivers can send large amounts
	 * of data in blobs without problems.
	 */
	nl_socket_enable_msg_peek(ctx->event_sock);

	/*
	 * disable sequence checking to handle events.
	 */
	nl_socket_disable_seq_check(ctx->event_sock);

	cb = nl_socket_get_cb(ctx->event_sock);
	if (cb == NULL) {
		fprintf(stderr,
		"Failed to get NL control block for event socket port\n");
		goto cleanup;
	}

	err = 1;
	nl_cb_set(cb, NL_CB_SEQ_CHECK, NL_CB_CUSTOM, no_seq_check, NULL);
	nl_cb_err(cb, NL_CB_CUSTOM, error_handler, &err);
	nl_cb_set(cb, NL_CB_FINISH, NL_CB_CUSTOM, finish_handler, &err);
	nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, ack_handler, &err);

	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, process_global_event,
			NULL);
	nl_cb_put(cb);
	return 0; /* success */
cleanup:
	nl_socket_free(ctx->event_sock);
	return -EIO;

}

/**
 * wifi_nl80211_start_event_thread: Start the thread which processes
 *                                  the async netlink events
 * @ctx: wifi cfg80211 context
 *
 * return 1/0
 */
int wifi_nl80211_start_event_thread(wifi_cfg80211_context *ctx)
{
	if (pthread_create(&ctx->event_thread_handle, NULL,
				event_thread, ctx)) {
		return -EFAULT;
	}
	while(!ctx->event_thread_running);
	return 0;
}

/**
 * wifi_destroy_nl80211: destriy nl80211 socket
 * @ctx: wifi cfg80211 context
 *
 * return 1/0
 */
void wifi_destroy_nl80211(wifi_cfg80211_context *ctx)
{
	nl_socket_free(ctx->cmd_sock);
	if (ctx->event_thread_running) {
		/*
		 * Make the thread exit
		 */
		ctx->event_thread_running = 0;
		pthread_cancel(ctx->event_thread_handle);
		pthread_join(ctx->event_thread_handle, NULL);
	}
	if (ctx->event_sock) {
		nl_socket_free(ctx->event_sock);
	}
}

