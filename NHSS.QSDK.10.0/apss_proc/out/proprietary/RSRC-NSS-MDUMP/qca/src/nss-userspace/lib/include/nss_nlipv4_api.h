/*
 * Copyright (c) 2019 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#ifndef __NSS_NLIPV4_API_H__
#define __NSS_NLIPV4_API_H__

/**
 * @addtogroup libnss_nl
 * @{
 */

/**
 * @file nss_nlipv4_api.h
 * 	This file declares the NSS NL IPv4 API(s) for userspace. These
 * 	API(s) are wrapper functions for IPv4 family specific operation.
 */

/**
 * @brief response callback function
 *
 * @param user_ctx[IN] user context, provided at socket open
 * @param rule[IN] IPv4 rule associated with the response
 * @param cb_data[IN] data that the user wants per callback
 */
typedef void (*nss_nlipv4_resp_t)(void *user_ctx, struct nss_nlipv4_rule *rule, void *resp_ctx);

/**
 * @brief event callback function
 *
 * @param user_ctx[IN] user context, provided at socket open
 * @param rule[IN] IPv4 rule associated with the event
 */
typedef void (*nss_nlipv4_event_t)(void *user_ctx, struct nss_nlipv4_rule *rule);

/**
 * @brief NSS NL IPv4 response
 */
struct nss_nlipv4_resp {
	void *data;		/**< response context */
	nss_nlipv4_resp_t cb;	/**< response callback */
};

/**
 * @brief NSS IPv4 context
 */
struct nss_nlipv4_ctx {
	struct nss_nlsock_ctx sock;	/**< NSS socket context */
	nss_nlipv4_event_t event;	/**< NSS event callback function */
};

/**
 * @brief Open NSS IPv4 NL socket
 *
 * @param ctx[IN] NSS NL socket context, allocated by the caller
 * @param user_ctx[IN] user context stored per socket
 * @param event_cb[IN] event callback handler
 *
 * @return status of the open call
 */
int nss_nlipv4_sock_open(struct nss_nlipv4_ctx *ctx, void *user_ctx, nss_nlipv4_event_t event_cb);

/**
 * @brief Close NSS IPv4 NL socket
 *
 * @param ctx[IN] NSS NL context
 */
void nss_nlipv4_sock_close(struct nss_nlipv4_ctx *ctx);

/**
 * @brief send an IPv4 rule asynchronously to the NSS NETLINK
 *
 * @param ctx[IN] NSS IPv4 NL context
 * @param rule[IN] IPv4 rule to use
 *
 * @return status of send, where '0' is success and -ve means failure
 */
int nss_nlipv4_sock_send(struct nss_nlipv4_ctx *ctx, struct nss_nlipv4_rule *rule);

/**
 * @brief initialize the rule message
 *
 * @param ctx[IN] NSS IPv4 NL context
 * @param rule[IN] IPv4 rule
 * @param data[IN] response data per callback
 * @param cb[IN] response callback handler
 */
void nss_nlipv4_init_rule(struct nss_nlipv4_ctx *ctx, struct nss_nlipv4_rule *rule, void *data, nss_nlipv4_resp_t cb, enum nss_ipv4_message_types type);

/**
 * @brief initialize connection rule for create message
 *
 * @param create[IN] create message
 */
static inline void nss_nlipv4_init_conn_rule(struct nss_ipv4_rule_create_msg *create)
{
	create->valid_flags |= NSS_IPV4_RULE_CREATE_CONN_VALID;
}

/**
 * @brief initialize tcp protocol rule for create message
 *
 * @param create[IN] create message
 */
static inline void nss_nlipv4_init_tcp_rule(struct nss_ipv4_rule_create_msg *create)
{
	create->valid_flags |= NSS_IPV4_RULE_CREATE_TCP_VALID;
}

/**
 * @brief initialize pppoe rule for create message
 *
 * @param create[IN] create message
 */
static inline void nss_nlipv4_init_pppoe_rule(struct nss_ipv4_rule_create_msg *create)
{
	create->valid_flags |= NSS_IPV4_RULE_CREATE_PPPOE_VALID;
}

/**
 * @brief initialize qos rule for create message
 *
 * @param create[IN] create message
 */
static inline void nss_nlipv4_init_qos_rule(struct nss_ipv4_rule_create_msg *create)
{
	create->valid_flags |= NSS_IPV4_RULE_CREATE_QOS_VALID;
}

/**
 * @brief initialize dscp rule for create message
 *
 * @param create[IN] create message
 */
static inline void nss_nlipv4_init_dscp_rule(struct nss_ipv4_rule_create_msg *create)
{
	create->valid_flags |= NSS_IPV4_RULE_CREATE_DSCP_MARKING_VALID;
}

/**
 * @brief initialize vlan rule for create message
 *
 * @param create[IN] create message
 */
static inline void nss_nlipv4_init_vlan_rule(struct nss_ipv4_rule_create_msg *create)
{
	struct nss_ipv4_vlan_rule *primary;
	struct nss_ipv4_vlan_rule *secondary;

	primary = &create->vlan_primary_rule;
	secondary = &create->vlan_secondary_rule;

	create->valid_flags |= NSS_IPV4_RULE_CREATE_VLAN_VALID;

	/*
	 * set the tags to default values
	 */
	primary->ingress_vlan_tag = NSS_NLIPV4_VLAN_ID_NOT_CONFIGURED;
	primary->egress_vlan_tag = NSS_NLIPV4_VLAN_ID_NOT_CONFIGURED;

	secondary->ingress_vlan_tag = NSS_NLIPV4_VLAN_ID_NOT_CONFIGURED;
	secondary->egress_vlan_tag = NSS_NLIPV4_VLAN_ID_NOT_CONFIGURED;
}

/**}@*/
#endif /* !__NSS_NLIPV4_API_H__*/
