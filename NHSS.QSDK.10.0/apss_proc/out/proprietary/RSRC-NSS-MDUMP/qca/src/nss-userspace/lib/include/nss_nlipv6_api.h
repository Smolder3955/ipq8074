/*
 * Copyright (c) 2019 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#ifndef __NSS_NLIPV6_API_H__
#define __NSS_NLIPV6_API_H__

/**
 * @addtogroup libnss_nl
 * @{
 */

/**
 * @file nss_nlipv6_api.h
 * 	This file declares the NSS NL IPv6 API(s) for userspace. These
 * 	API(s) are wrapper functions for IPv6 family specific operation.
 */

/**
 * @brief response callback function
 *
 * @param user_ctx[IN] user context, provided at socket open
 * @param rule[IN] IPv6 rule associated with the response
 * @param cb_data[IN] data that the user wants per callback
 */
typedef void (*nss_nlipv6_resp_t)(void *user_ctx, struct nss_nlipv6_rule *rule, void *resp_ctx);

/**
 * @brief event callback function
 *
 * @param user_ctx[IN] user context, provided at socket open
 * @param rule[IN] IPv6 rule associated with the event
 */
typedef void (*nss_nlipv6_event_t)(void *user_ctx, struct nss_nlipv6_rule *rule);

/**
 * @brief NSS NL IPv6 response
 */
struct nss_nlipv6_resp {
	void *data;		/**< response context */
	nss_nlipv6_resp_t cb;	/**< response callback */
};

/**
 * @brief NSS IPv6 context
 */
struct nss_nlipv6_ctx {
	struct nss_nlsock_ctx sock;	/**< NSS socket context */
	nss_nlipv6_event_t event;	/**< NSS event callback function */
};

/**
 * @brief Open NSS IPv6 NL socket
 *
 * @param ctx[IN] NSS NL socket context, allocated by the caller
 * @param user_ctx[IN] user context stored per socket
 * @param event_cb[IN] event callback handler
 *
 * @return status of the open call
 */
int nss_nlipv6_sock_open(struct nss_nlipv6_ctx *ctx, void *user_ctx, nss_nlipv6_event_t event_cb);

/**
 * @brief Close NSS IPv6 NL socket
 *
 * @param ctx[IN] NSS NL context
 */
void nss_nlipv6_sock_close(struct nss_nlipv6_ctx *ctx);

/**
 * @brief send an IPv6 rule asynchronously to the NSS NETLINK
 *
 * @param ctx[IN] NSS IPv6 NL context
 * @param rule[IN] IPv6 rule to use
 *
 * @return status of send, where '0' is success and -ve means failure
 */
int nss_nlipv6_sock_send(struct nss_nlipv6_ctx *ctx, struct nss_nlipv6_rule *rule);

/**
 * @brief initialize the rule message
 *
 * @param ctx[IN] NSS IPv6 NL context
 * @param rule[IN] IPv6 rule
 * @param data[IN] response data per callback
 * @param cb[IN] response callback handler
 */
void nss_nlipv6_init_rule(struct nss_nlipv6_ctx *ctx, struct nss_nlipv6_rule *rule, void *data, nss_nlipv6_resp_t cb, enum nss_ipv6_message_types type);

/**
 * @brief initialize connection rule for create message
 *
 * @param create[IN] create message
 */
static inline void nss_nlipv6_init_conn_rule(struct nss_ipv6_rule_create_msg *create)
{
	create->valid_flags |= NSS_IPV6_RULE_CREATE_CONN_VALID;
}

/**
 * @brief initialize tcp protocol rule for create message
 *
 * @param create[IN] create message
 */
static inline void nss_nlipv6_init_tcp_rule(struct nss_ipv6_rule_create_msg *create)
{
	create->valid_flags |= NSS_IPV6_RULE_CREATE_TCP_VALID;
}

/**
 * @brief initialize pppoe rule for create message
 *
 * @param create[IN] create message
 */
static inline void nss_nlipv6_init_pppoe_rule(struct nss_ipv6_rule_create_msg *create)
{
	create->valid_flags |= NSS_IPV6_RULE_CREATE_PPPOE_VALID;
}

/**
 * @brief initialize qos rule for create message
 *
 * @param create[IN] create message
 */
static inline void nss_nlipv6_init_qos_rule(struct nss_ipv6_rule_create_msg *create)
{
	create->valid_flags |= NSS_IPV6_RULE_CREATE_QOS_VALID;
}

/**
 * @brief initialize dscp rule for create message
 *
 * @param create[IN] create message
 */
static inline void nss_nlipv6_init_dscp_rule(struct nss_ipv6_rule_create_msg *create)
{
	create->valid_flags |= NSS_IPV6_RULE_CREATE_DSCP_MARKING_VALID;
}

/**
 * @brief initialize vlan rule for create message
 *
 * @param create[IN] create message
 */
static inline void nss_nlipv6_init_vlan_rule(struct nss_ipv6_rule_create_msg *create)
{
	struct nss_ipv6_vlan_rule *primary;
	struct nss_ipv6_vlan_rule *secondary;

	primary = &create->vlan_primary_rule;
	secondary = &create->vlan_secondary_rule;

	create->valid_flags |= NSS_IPV6_RULE_CREATE_VLAN_VALID;

	/*
	 * set the tags to default values
	 */
	primary->ingress_vlan_tag = NSS_NLIPV6_VLAN_ID_NOT_CONFIGURED;
	primary->egress_vlan_tag = NSS_NLIPV6_VLAN_ID_NOT_CONFIGURED;

	secondary->ingress_vlan_tag = NSS_NLIPV6_VLAN_ID_NOT_CONFIGURED;
	secondary->egress_vlan_tag = NSS_NLIPV6_VLAN_ID_NOT_CONFIGURED;
}

/**}@*/
#endif /* !__NSS_NLIPV6_API_H__*/
