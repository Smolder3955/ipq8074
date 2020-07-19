/*
 * Copyright (c) 2019 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#ifndef __NLCFG_IPV6_H
#define __NLCFG_IPV6_H

/* NLCFG ipv6 commands */
enum nlcfg_ipv6_cmd {
	NLCFG_IPV6_CMD_FLOW_ADD,		/* flow add */
	NLCFG_IPV6_CMD_FLOW_DEL,		/* flow delete */

	NLCFG_IPV6_CMD_MAX
};

/* NLCFG IPv6 flow add */
enum nlcfg_ipv6_flow_add {
	/* nlipv6_sel */
	NLCFG_IPV6_FLOW_ADD_SEL_SIP,		/* source IP address */
	NLCFG_IPV6_FLOW_ADD_SEL_DIP,		/* destination IP address */
	NLCFG_IPV6_FLOW_ADD_SEL_PROTO,		/* IP protocol number */
	NLCFG_IPV6_FLOW_ADD_SEL_SPORT, 		/* source port {TCP, UDP, SCTP} */
	NLCFG_IPV6_FLOW_ADD_SEL_DPORT,		/* destination port {TCP, UDP, SCTP} */

	/* interface name */
	NLCFG_IPV6_FLOW_ADD_IFNAME_SRC,		/* ingress interface name */
	NLCFG_IPV6_FLOW_ADD_IFNAME_DST,		/* egress interface name */
	NLCFG_IPV6_FLOW_ADD_IFTYPE_SRC,		/* ingress interface type */
	NLCFG_IPV6_FLOW_ADD_IFTYPE_DST,		/* egress interface type */

	NLCFG_IPV6_FLOW_ADD_MAX
};

/* NLCFG IPv6 flow del */
enum nlcfg_ipv6_flow_del {
	/* nlipv6_sel */
	NLCFG_IPV6_FLOW_DEL_SEL_SIP,		/* source IP address */
	NLCFG_IPV6_FLOW_DEL_SEL_DIP,		/* destination IP address */
	NLCFG_IPV6_FLOW_DEL_SEL_PROTO,		/* IP protocol number */
	NLCFG_IPV6_FLOW_DEL_SEL_SPORT, 		/* source port {TCP, UDP, SCTP} */
	NLCFG_IPV6_FLOW_DEL_SEL_DPORT,		/* destination port {TCP, UDP, SCTP} */

	NLCFG_IPV6_FLOW_DEL_MAX
};

#endif /* __NLCFG_IPV6_H*/

