/*
 * Copyright (c) 2019 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include <nss_nlbase.h>
#include <nss_nlsock_api.h>
#include <nss_nlipv6_api.h>

/*
 * nss_nlipv6_sock_cb()
 * 	NSS NL IPv6 callback
 */
int nss_nlipv6_sock_cb(struct nl_msg *msg, void *arg)
{
	pid_t pid = getpid();

	struct nss_nlipv6_ctx *ctx = (struct nss_nlipv6_ctx *)arg;
	struct nss_nlsock_ctx *sock = &ctx->sock;

	struct nss_nlipv6_rule *rule = nss_nlsock_get_data(msg);
	if (!rule) {
		nss_nlsock_log_error("%d:failed to get NSS NL IPv6 header\n", pid);
		return NL_SKIP;
	}

	uint8_t cmd = nss_nlcmn_get_cmd(&rule->cm);

	switch (cmd) {
	case NSS_IPV6_TX_CREATE_RULE_MSG:
	case NSS_IPV6_TX_DESTROY_RULE_MSG:
	{
		void *cb_data = nss_nlcmn_get_cb_data(&rule->cm, sock->family_id);
		if (!cb_data) {
			return NL_SKIP;
		}

		/*
		 * Note: The callback user can modify the CB content so it
		 * needs to locally save the response data for further use
		 * after the callback is completed
		 */
		struct nss_nlipv6_resp resp;
		memcpy(&resp, cb_data, sizeof(struct nss_nlipv6_resp));

		/*
		 * clear the ownership of the CB so that callback user can
		 * use it if needed
		 */
		nss_nlcmn_clr_cb_owner(&rule->cm);

		if (!resp.cb) {
			nss_nlsock_log_error("%d:no IPv6 response callback for cmd(%d)\n", pid, cmd);
			return NL_SKIP;
		}

		resp.cb(sock->user_ctx, rule, resp.data);

		return NL_OK;
	}

	case NSS_IPV6_RX_CONN_STATS_SYNC_MSG:
	{
		nss_nlipv6_event_t event = ctx->event;

		assert(event);
		event(sock->user_ctx, rule);

		return NL_OK;
	}

	default:
		nss_nlsock_log_error("%d:unsupported message cmd type(%d)\n", pid, cmd);
		return NL_SKIP;
	}
}

/*
 * nss_nlipv6_sock_open()
 * 	this opens the NSS IPv6 NL socket for usage
 */
int nss_nlipv6_sock_open(struct nss_nlipv6_ctx *ctx, void *user_ctx, nss_nlipv6_event_t event_cb)
{
	pid_t pid = getpid();
	int error;

	if (!ctx) {
		nss_nlsock_log_error("%d: invalid parameters passed\n", pid);
		return -1;
	}

	memset(ctx, 0, sizeof(struct nss_nlipv6_ctx));

	nss_nlsock_set_family(&ctx->sock, NSS_NLIPV6_FAMILY);
	nss_nlsock_set_user_ctx(&ctx->sock, user_ctx);

	/*
	 * try opening the socket with Linux
	 */
	error = nss_nlsock_open(&ctx->sock, nss_nlipv6_sock_cb);
	if (error) {
		nss_nlsock_log_error("%d:unable to open NSS IPv6 socket, error(%d)\n", pid, error);
		goto fail;
	}

	/*
	 * check to see if caller wants to listen for events
	 */
	if (!event_cb) {
		nss_nlsock_log_error("%d:NSS IPv6 event listening is disabled\n", pid);
		return 0;
	}

	/*
	 * subscribe to the NSS NL Multicast group
	 */
	error = nss_nlsock_listen(&ctx->sock, NSS_NLIPV6_MCAST_GRP);
	if (!error) {
		nss_nlsock_log_error("%d:unable to subscribe NSS IPv6 group, error(%d)\n", pid, error);
		goto fail;
	}

	ctx->event = event_cb;
	return 0;
fail:
	memset(ctx, 0, sizeof(struct nss_nlipv6_ctx));
	return error;
}

/*
 * nss_nlipv6_sock_close()
 * 	close the NSS IPv6 NL socket
 */
void nss_nlipv6_sock_close(struct nss_nlipv6_ctx *ctx)
{
	nss_nlsock_close(&ctx->sock);
}

/*
 * nss_nlipv6_sock_send()
 *	send the IPv6 message asynchronously through the socket
 */
int nss_nlipv6_sock_send(struct nss_nlipv6_ctx *ctx, struct nss_nlipv6_rule *rule)
{
	pid_t pid = getpid();
	int error;

	if (!rule) {
		nss_nlsock_log_error("%d:invalid NSS IPv6 rule\n", pid);
		return -ENOMEM;
	}

	error = nss_nlsock_send(&ctx->sock, &rule->cm, rule);
	if (error) {
		nss_nlsock_log_error("%d:failed to send NSS IPv6 rule, error(%d)\n", pid, error);
		return error;
	}

	return 0;
}

/*
 * nss_nlipv6_init_rule()
 * 	init the rule message
 */
void nss_nlipv6_init_rule(struct nss_nlipv6_ctx *ctx, struct nss_nlipv6_rule *rule, void *data, nss_nlipv6_resp_t cb, enum nss_ipv6_message_types type)
{
	int32_t family_id = ctx->sock.family_id;

	nss_nlipv6_rule_init(rule, type);

	nss_nlcmn_set_cb_owner(&rule->cm, family_id);

	struct nss_nlipv6_resp *resp = nss_nlcmn_get_cb_data(&rule->cm, family_id);
	assert(resp);

	resp->data = data;
	resp->cb = cb;
}
