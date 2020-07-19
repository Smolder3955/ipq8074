/*
 **************************************************************************
 * Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 **************************************************************************
 */
/*
 * nss_dp_api_if.h
 *	nss-dp exported structure/apis.
 */

#ifndef __DP_API_IF_H
#define __DP_API_IF_H

/*
 * NSS DP status
 */
#define NSS_DP_SUCCESS	0
#define NSS_DP_FAILURE	-1

/*
 * NSS PTP service code
 */
#define NSS_PTP_EVENT_SERVICE_CODE	0x9

/*
 * data plane context base class
 */
struct nss_dp_data_plane_ctx {
	struct net_device *dev;
};

/*
 * NSS data plane ops, default would be slowpath and can be overridden by
 * nss-drv
 */
struct nss_dp_data_plane_ops {
	int (*init)(struct nss_dp_data_plane_ctx *dpc);
	int (*open)(struct nss_dp_data_plane_ctx *dpc, uint32_t tx_desc_ring,
		    uint32_t rx_desc_ring, uint32_t mode);
	int (*close)(struct nss_dp_data_plane_ctx *dpc);
	int (*link_state)(struct nss_dp_data_plane_ctx *dpc,
			  uint32_t link_state);
	int (*mac_addr)(struct nss_dp_data_plane_ctx *dpc, uint8_t *addr);
	int (*change_mtu)(struct nss_dp_data_plane_ctx *dpc, uint32_t mtu);
	netdev_tx_t (*xmit)(struct nss_dp_data_plane_ctx *dpc, struct sk_buff *os_buf);
	void (*set_features)(struct nss_dp_data_plane_ctx *dpc);
	int (*pause_on_off)(struct nss_dp_data_plane_ctx *dpc,
			    uint32_t pause_on);
	int (*vsi_assign)(struct nss_dp_data_plane_ctx *dpc, uint32_t vsi);
	int (*vsi_unassign)(struct nss_dp_data_plane_ctx *dpc, uint32_t vsi);
	int (*rx_flow_steer)(struct nss_dp_data_plane_ctx *dpc, struct sk_buff *skb,
				uint32_t cpu, bool is_add);
};

/*
 * nss_dp_receive()
 */
void nss_dp_receive(struct net_device *netdev, struct sk_buff *skb,
						struct napi_struct *napi);

/*
 * nss_dp_is_in_open_state()
 */
bool nss_dp_is_in_open_state(struct net_device *netdev);

/*
 * nss_dp_override_data_palne()
 */
int nss_dp_override_data_plane(struct net_device *netdev,
			       struct nss_dp_data_plane_ops *dp_ops,
			       struct nss_dp_data_plane_ctx *dpc);

/*
 * nss_dp_start_data_plane()
 */
void nss_dp_start_data_plane(struct net_device *netdev,
			     struct nss_dp_data_plane_ctx *dpc);

/*
 * nss_dp_restore_data_plane()
 */
void nss_dp_restore_data_plane(struct net_device *netdev);

/*
 * nss_dp_get_netdev_by_macid()
 */
struct net_device *nss_dp_get_netdev_by_macid(int macid);

/*
 * nss_phy_tstamp_rx_buf()
 */
void nss_phy_tstamp_rx_buf(void *app_data, struct sk_buff *skb);

/*
 * nss_phy_tstamp_tx_buf()
 */
void nss_phy_tstamp_tx_buf(struct net_device *ndev, struct sk_buff *skb);
#endif	/* __DP_API_IF_H */
