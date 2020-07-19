/*
 * Copyright (c) 2019 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#ifndef __NLCFG_IPV4_H
#define __NLCFG_IPV4_H

/* NLCFG ipv4 commands */
enum nlcfg_ipv4_cmd {
	NLCFG_IPV4_CMD_FLOW_ADD,		/* flow add */
	NLCFG_IPV4_CMD_FLOW_DEL,		/* flow delete */

	NLCFG_IPV4_CMD_MAX
};

/* NLCFG IPv4 flow add */
enum nlcfg_ipv4_flow_add {
	/* nlipv4_sel */
	NLCFG_IPV4_FLOW_ADD_SEL_SIP,		/* source IP address */
	NLCFG_IPV4_FLOW_ADD_SEL_DIP,		/* destination IP address */
	NLCFG_IPV4_FLOW_ADD_SEL_PROTO,		/* IP protocol number */
	NLCFG_IPV4_FLOW_ADD_SEL_SPORT, 		/* source port {TCP, UDP, SCTP} */
	NLCFG_IPV4_FLOW_ADD_SEL_DPORT,		/* destination port {TCP, UDP, SCTP} */

	/* interface name */
	NLCFG_IPV4_FLOW_ADD_IFNAME_SRC,		/* ingress interface name */
	NLCFG_IPV4_FLOW_ADD_IFNAME_DST,		/* egress interface name */
	NLCFG_IPV4_FLOW_ADD_IFTYPE_SRC,		/* ingress interface type */
	NLCFG_IPV4_FLOW_ADD_IFTYPE_DST,		/* egress interface type */

	NLCFG_IPV4_FLOW_ADD_MAX
};

/* NLCFG IPv4 flow del */
enum nlcfg_ipv4_flow_del {
	/* nlipv4_sel */
	NLCFG_IPV4_FLOW_DEL_SEL_SIP,		/* source IP address */
	NLCFG_IPV4_FLOW_DEL_SEL_DIP,		/* destination IP address */
	NLCFG_IPV4_FLOW_DEL_SEL_PROTO,		/* IP protocol number */
	NLCFG_IPV4_FLOW_DEL_SEL_SPORT, 		/* source port {TCP, UDP, SCTP} */
	NLCFG_IPV4_FLOW_DEL_SEL_DPORT,		/* destination port {TCP, UDP, SCTP} */

	NLCFG_IPV4_FLOW_DEL_MAX
};

#endif /* __NLCFG_IPV4_H*/

