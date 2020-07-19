/*
 * Copyright (c) 2011, 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * Copyright (c) 2010, Atheros Communications Inc.
 * All Rights Reserved.
 */

#ifndef __DP_EXTAP_MITBL_H_
#define __DP_EXTAP_MITBL_H_

#include "qdf_net_types.h"

#define ATH_MITBL_NONE 0x0000
#define ATH_MITBL_IPV4 0x0001
#define ATH_MITBL_IPV6 0x0002

#if MI_TABLE_AS_TREE
typedef struct mi_node { /* MAC - IP Node */
	struct mi_node *parent,
				   *left,
				   *right;
	u_int8_t h_dest[QDF_NET_ETH_LEN],
			 len,
			 ip_ver,
			 ip[16];	/* v4 or v6 ip addr */
} mi_node_t;
#else
#define mi_node_is_free(n)	((n)->ip_ver == ATH_MITBL_NONE)
#define mi_node_free(n)	do { (n)->ip_ver = ATH_MITBL_NONE; } while (0)

typedef struct mi_node { /* MAC - IP Node */
	u_int8_t h_dest[QDF_NET_ETH_LEN],
			 len,
			 ip_ver,
			 ip[16];	/* v4 or v6 ip addr */
} mi_node_t;
#endif /* MI_TABLE_AS_TREE */

#define minode_ip_len(n)	(((n)->ip_ver == ATH_MITBL_IPV4) ? 4 : 16)
#define mi_ip_len(n)		(((n) == ATH_MITBL_IPV4) ? 4 : 16)
#define mi_add		mi_tbl_add
#define mi_lkup		mi_tbl_lkup

mi_node_t *mi_tbl_add(mi_node_t **, u_int8_t *, u_int8_t *, int);
u_int8_t *mi_tbl_lkup(mi_node_t *, u_int8_t *, int);
void mi_tbl_del(mi_node_t **, u_int8_t *, int);
void mi_tbl_dump(void *);
void mi_tbl_purge(mi_node_t **);
#endif /* __DP_EXTAP_MITBL_H_ */
