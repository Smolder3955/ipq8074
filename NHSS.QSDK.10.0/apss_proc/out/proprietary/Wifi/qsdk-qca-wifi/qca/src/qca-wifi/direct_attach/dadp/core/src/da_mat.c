/*
 * Copyright (c) 2017-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */
/*WRAP includes for MAT*/
#include <linux/udp.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <net/ndisc.h>
#include <net/arp.h>
#include "da_mat.h"
#include "asf_print.h"		/* asf_print_setup */
#include "qdf_mem.h"		/* qdf_mem_malloc, free */
#include "qdf_lock.h"
#include <da_dp_public.h>

#if ATH_SUPPORT_WRAP
extern void compute_udp_checksum(qdf_net_iphdr_t * p_iph,
				 unsigned short *ip_payload);

static uint16_t checksum(uint16_t protocol, uint16_t len, uint8_t src_addr[],
			 uint8_t dest_addr[], uint16_t addrleninbytes,
			 uint8_t * buff)
{
	uint16_t pad = 0;
	uint16_t word16;
	uint32_t sum = 0;
	int i;

	// Find out if the length of data is even or odd number. If odd,
	// add a padding byte = 0 at the end of packet
	if (len & 1) {
		// Take care of the last byte by itself.
		len -= 1;
		pad = 1;
	}
	// make 16 bit words out of every two adjacent 8 bit words and
	// calculate the sum of all 16 bit words
	for (i = 0; i < len; i = i + 2) {
		word16 = buff[i];
		word16 = (word16 << 8) + buff[i + 1];
		sum = sum + (uint32_t) word16;
	}

	if (pad) {
		// Get the last byte
		word16 = buff[len];
		word16 <<= 8;
		sum = sum + (uint32_t) word16;
	}
	// add the UDP pseudo header which contains the IP source and destination addresses
	for (i = 0; i < addrleninbytes; i = i + 2) {
		word16 = src_addr[i];
		word16 = (word16 << 8) + src_addr[i + 1];
		sum = sum + (uint32_t) word16;
	}

	for (i = 0; i < addrleninbytes; i = i + 2) {
		word16 = dest_addr[i];
		word16 = (word16 << 8) + dest_addr[i + 1];
		sum = sum + (uint32_t) word16;
	}

	// the protocol number and the length of the s packet
	sum = sum + (uint32_t) protocol + (uint32_t) (len + pad);

	// keep only the last 16 bits of the 32 bit calculated sum and add back the carries
	while (sum >> 16) {
		sum = (sum & 0xFFFF) + (sum >> 16);
	}

	// Take the one's complement of sum
	sum = ~sum;

	return ((uint16_t) sum);
}

/**
 * @brief Helper to update udp checksum.
 *
 * @param osum Old checksum
 * @param omac Old MAC address
 * @param nmac New MAC address
 *
 * @return checksum
 */
static uint16_t update_checksum_addr(uint16_t osum, uint8_t omac[6],
				     uint8_t nmac[6])
{
	uint16_t nsum = osum;
	uint32_t sum;
	int i;

	if (osum == 0)
		return 0;

	for (i = 0; i < 3; i++) {
		sum = nsum;
		sum +=
		    *(uint16_t *) & omac[i * 2] +
		    (~(*(uint16_t *) & nmac[i * 2]) & 0XFFFF);
		sum = (sum >> 16) + (sum & 0XFFFF);
		nsum = (uint16_t) ((sum >> 16) + sum);
	}

	return nsum;
}

struct eth_arphdr {
	unsigned short ar_hrd,	/* format of hardware address */
	 ar_pro;		/* format of protocol address */
	unsigned char ar_hln,	/* length of hardware address */
	 ar_pln;		/* length of protocol address */
	unsigned short ar_op;	/* ARP opcode (command) */
	unsigned char ar_sha[ETH_ALEN],	/* sender hardware address */
	 ar_sip[4],		/* sender IP address */
	 ar_tha[ETH_ALEN],	/* target hardware address */
	 ar_tip[4];		/* target IP address */
} __attribute__ ((__packed__));

struct dhcp_option {
	u_char type;
	u_char len;
	u_char value[0];	/* option value */
} __attribute__ ((__packed__));

struct dhcp_packet {
	u_char op;		/* packet opcode type */
	u_char htype;		/* hardware addr type */
	u_char hlen;		/* hardware addr length */
	u_char hops;		/* gateway hops */
	u_int32_t xid;		/* transaction ID */
	u_int16_t secs;		/* seconds since boot began */
	u_int16_t flags;	/* flags */
	struct in_addr ciaddr;	/* client IP address */
	struct in_addr yiaddr;	/* 'your' IP address */
	struct in_addr siaddr;	/* server IP address */
	struct in_addr giaddr;	/* gateway IP address */
	u_char chaddr[16];	/* client hardware address */
	u_char sname[64];	/* server host name */
	u_char file[128];	/* boot file name */
	u_char magic_cookie[4];	/* magic cookie */
	u_char options[0];	/* variable-length options field */
} __attribute__ ((__packed__));

struct eth_icmp6_lladdr {
	unsigned char type;
	unsigned char len;
	unsigned char addr[6];	/* hardware address */
} __attribute__ ((__packed__));

typedef struct eth_icmp6_lladdr eth_icmp6_lladdr_t;

/**
 * @brief WRAP MAT function for transmit.
 *
 * @param out_vap Output VAP.
 * @param buf
 *
 * @return 0 On success.
 * @return -ve On failure.
 */
int ath_wrap_mat_tx(struct wlan_objmgr_vdev *out_vdev, wbuf_t buf)
{
	struct ether_header *eh;
	uint16_t ether_type;
	int contig_len = sizeof(struct ether_header);
	int pktlen = wbuf_get_pktlen(buf);
	uint8_t *src_mac, *des_mac, *p, ismcast;
	uint8_t *arp_smac = NULL;
	uint8_t *arp_dmac = NULL;
	struct eth_arphdr *parp = NULL;

	if (!(dadp_vdev_use_mat(out_vdev)))
		return 0;

	if ((pktlen < contig_len))
		return -EINVAL;

	eh = (struct ether_header *)(wbuf_header(buf));
	p = (uint8_t *) (eh + 1);

	ether_type = eh->ether_type;
	src_mac = eh->ether_shost;
	des_mac = eh->ether_dhost;
	ismcast = IEEE80211_IS_MULTICAST(des_mac);

#ifdef ATH_MAT_TEST
	printk(KERN_ERR "%s: src %s type 0x%x", __func__,
	       ether_sprintf(src_mac), ether_type);
	printk(KERN_ERR "des %s\n", ether_sprintf(des_mac));
#endif

	if (ether_type == htons(ETH_P_PAE))
		return 0;

	if (ether_type == htons(ETHERTYPE_ARP)) {
		parp = (struct eth_arphdr *)p;
		contig_len += sizeof(struct eth_arphdr);

		if ((pktlen < contig_len))
			return -EINVAL;

		if (parp->ar_hln == ETH_ALEN && parp->ar_pro == htons(ETH_P_IP)) {
			arp_smac = parp->ar_sha;
			arp_dmac = parp->ar_tha;
		} else {
			parp = NULL;
		}
	}

	if (parp) {
		if (parp->ar_op == htons(ARPOP_REQUEST)
		    || parp->ar_op == htons(ARPOP_REPLY)) {
			QDF_TRACE( QDF_MODULE_ID_WRAP,
				       QDF_TRACE_LEVEL_DEBUG,
				       "%s: ARP sha %s -> ", __func__,
				       ether_sprintf(arp_smac));
			IEEE80211_ADDR_COPY(arp_smac,
					    wlan_vdev_mlme_get_macaddr
					    (out_vdev));
			QDF_TRACE( QDF_MODULE_ID_WRAP,
				       QDF_TRACE_LEVEL_DEBUG,
				       "%s: ARP sha %s -> ", __func__,
				       ether_sprintf(arp_smac));
		}
	} else if (ether_type == htons(ETHERTYPE_IP)) {
		struct iphdr *p_ip = (struct iphdr *)(p);
		int16_t ip_hlen;

		contig_len += sizeof(struct iphdr);
		if ((pktlen < contig_len))
			return -EINVAL;

		ip_hlen = p_ip->ihl * 4;

		/* If Proto is UDP */
		if (p_ip->protocol == IPPROTO_UDP) {
			struct udphdr *p_udp =
			    (struct udphdr *)(((uint8_t *) p_ip) + ip_hlen);
			uint16_t udplen;

			contig_len += sizeof(struct udphdr);
			if ((pktlen < contig_len))
				return -EINVAL;

			udplen = p_ip->tot_len - (p_ip->ihl * 4);
#ifdef ATH_DEBUG_MAT
			printk(KERN_ERR "%s:%d sport %d dport %d\n", __func__,
			       __LINE__, p_udp->source, p_udp->dest);
#endif
			/*
			 * DHCP request UDP Client SP = 68 (bootpc), DP = 67 (bootps).
			 */
			if ((p_udp->dest == htons(67))) {
				struct dhcp_packet *p_dhcp =
				    (struct dhcp_packet *)(((u_int8_t *) p_udp)
							   +
							   sizeof(struct
								  udphdr));
#if 0
				struct dhcp_option *option =
				    (struct dhcp_option *)p_dhcp->options;
				u_int8_t *value;
#endif

				contig_len += sizeof(struct dhcp_packet);
				if ((pktlen < contig_len))
					return -EINVAL;

#ifdef ATH_DEBUG_MAT
				printk(KERN_ERR
				       "%s:%d sport %d dport %d len %d chaddr %s\n",
				       __func__, __LINE__, p_udp->source,
				       p_udp->dest, udplen,
				       ether_sprintf(p_dhcp->chaddr));
#endif
				if (p_dhcp->magic_cookie[0] == 0x63
				    && p_dhcp->magic_cookie[1] == 0x82
				    && p_dhcp->magic_cookie[2] == 0x53
				    && p_dhcp->magic_cookie[3] == 0x63) {
#ifdef ATH_DEBUG_MAT
					printk(" Found DHCP cookie .. \n");
#endif
					if (p_dhcp->op == 1) {
						/* dhcp REQ or DISCOVER */

#ifdef BIG_ENDIAN_HOST
						if ((p_dhcp->flags & 0x8000) ==
						    0) {
							p_dhcp->flags |= 0x8000;
#else
						if ((p_dhcp->flags & 0x0080) ==
						    0) {
							p_dhcp->flags |= 0x0080;
#endif
#ifdef ATH_DEBUG_MAT
							printk
							    (" Make the dhcp flag to broadcast ... \n");
#endif
							compute_udp_checksum((qdf_net_iphdr_t *) p_ip, (unsigned short *)p_udp);
						}
					}
				}
				/* Magic cookie for DHCP */
#if 0
				/* When MAC address is changed in DHCP payload, Apple devices are showing DHCP issue
				   So, not doing MAT translation on DHCP payload and only making DHCP flag as broadcast.
				   Refer IR-131533 */
				if (IEEE80211_ADDR_EQ(p_dhcp->chaddr, src_mac)) {
					/* replace the Client HW address with the VMA */
					DPRINTF(scn, ATH_DEBUG_MAT,
						"%s: DHCP chaddr %s -> ",
						__func__,
						ether_sprintf(p_dhcp->chaddr));
					IEEE80211_ADDR_COPY(p_dhcp->chaddr,
							    wlan_vdev_mlme_get_macaddr
							    (out_vdev));
					DPRINTF(scn, ATH_DEBUG_MAT, "%s\n",
						ether_sprintf(p_dhcp->chaddr));
					/*
					 * Since the packet was modified, do incremental checksum
					 * update for the UDP frame.
					 */
					p_udp->check =
					    update_checksum_addr(p_udp->check,
								 src_mac,
								 wlan_vdev_mlme_get_macaddr
								 (out_vdev));
				}
				/* replace the client HW address with the VMA in option field */
				while (option->type != 0xFF) {
					/*Not an end option */

					contig_len += (option->len + 2);
					if ((pktlen < contig_len))
						return -EINVAL;

					value = option->value;
					if (option->type == 0x3D) {
						/*client identifier option */
						if (value[0] == 1) {
							/*Hw type is ethernet */
							value++;
							if (IEEE80211_ADDR_EQ
							    (value, src_mac)) {
								IEEE80211_ADDR_COPY
								    (value,
								     wlan_vdev_mlme_get_macaddr
								     (out_vdev));
							}
							/*
							 * Since the packet was modified, do incremental checksum
							 * update for the UDP frame.
							 */
							p_udp->check =
							    update_checksum_addr
							    (p_udp->check,
							     src_mac,
							     wlan_vdev_mlme_get_macaddr
							     (out_vdev));
						}
						break;
					} else {
						option =
						    (struct dhcp_option *)(value
									   +
									   option->
									   len);
					}
				}
#endif
			}
		}
	} else if (ether_type == htons(ETHERTYPE_IPV6)) {
		struct ipv6hdr *p_ip6 = (struct ipv6hdr *)(p);
		eth_icmp6_lladdr_t *phwaddr;
		int change_packet = 1;

		contig_len += sizeof(struct ipv6hdr);
		if (pktlen < contig_len)
			return -EINVAL;

		if (p_ip6->nexthdr == IPPROTO_ICMPV6) {
			struct icmp6hdr *p_icmp6 =
			    (struct icmp6hdr *)(p_ip6 + 1);
			uint16_t icmp6len = p_ip6->payload_len;

			contig_len += sizeof(struct icmp6hdr);
			if (pktlen < contig_len)
				return -EINVAL;

			/*
			 * It seems that we only have to modify IPv6 packets being
			 * sent by a Proxy STA. Both the solicitation and advertisement
			 * packets have the STA's OMA. Flip that to the VMA.
			 */
			switch (p_icmp6->icmp6_type) {
			case NDISC_NEIGHBOUR_SOLICITATION:
			case NDISC_NEIGHBOUR_ADVERTISEMENT:
				{
					contig_len +=
					    16 + sizeof(eth_icmp6_lladdr_t);
					if (pktlen < contig_len)
						return -EINVAL;

					phwaddr =
					    (eth_icmp6_lladdr_t
					     *) ((uint8_t *) (p_icmp6 + 1) +
						 16);
					IEEE80211_ADDR_COPY(phwaddr->addr,
							    wlan_vdev_mlme_get_macaddr
							    (out_vdev));
					p_icmp6->icmp6_cksum = 0;
					p_icmp6->icmp6_cksum =
					    htons(checksum
						  ((uint16_t) IPPROTO_ICMPV6,
						   icmp6len,
						   p_ip6->saddr.s6_addr,
						   p_ip6->daddr.s6_addr,
						   16
						   /* IPv6 has 32 byte addresses */
						   , (uint8_t *) p_icmp6));
					break;
				}
			case NDISC_ROUTER_SOLICITATION:
				{
					contig_len +=
					    sizeof(eth_icmp6_lladdr_t);
					if (pktlen < contig_len)
						return -EINVAL;

					/* replace the HW address with the VMA */
					phwaddr =
					    (eth_icmp6_lladdr_t
					     *) ((uint8_t *) (p_icmp6 + 1));
					break;
				}
			default:
				change_packet = 0;
				break;
			}

			if (change_packet) {
				IEEE80211_ADDR_COPY(phwaddr->addr,
						    wlan_vdev_mlme_get_macaddr
						    (out_vdev));
				p_icmp6->icmp6_cksum = 0;
				p_icmp6->icmp6_cksum =
				    htons(checksum
					  ((uint16_t) IPPROTO_ICMPV6, icmp6len,
					   p_ip6->saddr.s6_addr,
					   p_ip6->daddr.s6_addr,
					   16 /* IPv6 has 32 byte addresses */ ,
					   (uint8_t *) p_icmp6));
			}
		}
	}
	IEEE80211_ADDR_COPY(src_mac, wlan_vdev_mlme_get_macaddr(out_vdev));
	return 0;
}

struct wlan_wrap_rx_args {
	struct wlan_objmgr_vdev *t_vdev;
	uint8_t *arp_dmac;
};

void wlan_check_dest_vdev(struct wlan_objmgr_pdev *pdev, void *obj, void *args)
{
	struct wlan_objmgr_vdev *vdev = (struct wlan_objmgr_vdev *)obj;
	struct wlan_wrap_rx_args *wrap_args = (struct wlan_wrap_rx_args *)args;

	if ((NULL == pdev) || (NULL == vdev)) {
		qdf_print("%s:pdev or vdev are NULL!", __func__);
		return;
	}

	if ((dadp_vdev_use_mat(vdev))
	    &&
	    (IEEE80211_ADDR_EQ
	     (wrap_args->arp_dmac, wlan_vdev_mlme_get_macaddr(vdev)))) {
		wrap_args->t_vdev = vdev;
		return;
	}
}

struct wlan_mcast_reflect_args {
	uint32_t status;
	wbuf_t buf;
	uint8_t *src_mac;

};

void wlan_check_mcast_reflect(struct wlan_objmgr_pdev *pdev, void *obj,
			      void *args)
{
	struct wlan_objmgr_vdev *vdev = (struct wlan_objmgr_vdev *)obj;
	struct wlan_mcast_reflect_args *reflect_args =
	    (struct wlan_mcast_reflect_args *)args;

	if ((NULL == pdev) || (NULL == vdev)) {
		qdf_print("%s:pdev or vdev are NULL!", __func__);
		return;
	}

	if (IEEE80211_ADDR_EQ
	    (reflect_args->src_mac, wlan_vdev_mlme_get_macaddr(vdev))) {
		reflect_args->buf->mark |= WRAP_MARK_REFLECT;
		//translate src address else bridge view is consistent
		if (dadp_vdev_use_mat(vdev))
			IEEE80211_ADDR_COPY(reflect_args->src_mac,
					    dadp_vdev_get_mataddr(vdev));
		reflect_args->status = QDF_STATUS_E_ABORTED;
	}
}

/**
 * @brief WRAP MAT on receive path.
 *
 * @param in_vap In VAP
 * @param buf
 *
 * @return 0 On success.
 * @return -ve On failure.
 */
int ath_wrap_mat_rx(struct wlan_objmgr_vdev *in_vdev, wbuf_t buf)
{
	uint16_t ether_type;
	struct ether_header *eh;
	int contig_len = sizeof(struct ether_header);
	int pktlen = wbuf_get_pktlen(buf);
	uint8_t *src_mac, *des_mac, *p, ismcast;
	uint8_t *arp_smac = NULL;
	uint8_t *arp_dmac = NULL;
	uint8_t *mat_addr = NULL;
	struct eth_arphdr *parp = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;
	struct wlan_objmgr_vdev *vdev = NULL;

	vdev = in_vdev;
	eh = (struct ether_header *)(wbuf_header(buf));
	p = (uint8_t *) (eh + 1);

	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev) {
		qdf_print("%s:pdev is NULL ", __func__);
		return -1;
	}

	if (!dadp_vdev_is_wrap(vdev) && !dadp_vdev_is_psta(vdev))
		return 0;

	if ((pktlen < contig_len))
		return -EINVAL;

	ether_type = eh->ether_type;
	src_mac = eh->ether_shost;
	des_mac = eh->ether_dhost;
	ismcast = IEEE80211_IS_MULTICAST(des_mac);

	if (ether_type == htons(ETH_P_PAE)) {
		//mark the pkt to allow local delivery on the device
		buf->mark |= WRAP_MARK_ROUTE;
		return 0;
	}

	if (!dadp_vdev_use_mat(vdev))
		return 0;

#ifdef ATH_MAT_TEST
	printk(KERN_ERR "%s: src %s type 0x%x", __func__,
	       ether_sprintf(src_mac), ether_type);
	printk(KERN_ERR "des %s\n", ether_sprintf(des_mac));
#endif

	if (ether_type == htons(ETHERTYPE_ARP)) {
		parp = (struct eth_arphdr *)p;
		contig_len += sizeof(struct eth_arphdr);

		if ((pktlen < contig_len))
			return -EINVAL;

		if (parp->ar_hln == ETH_ALEN && parp->ar_pro == htons(ETH_P_IP)) {
			arp_smac = parp->ar_sha;
			arp_dmac = parp->ar_tha;
		} else {
			parp = NULL;
		}
	}

	if (ismcast && dadp_vdev_is_mpsta(vdev)) {
		struct wlan_mcast_reflect_args reflect_args = {0};
		reflect_args.status = QDF_STATUS_SUCCESS;
		reflect_args.buf = buf;
		reflect_args.src_mac = src_mac;
		wlan_objmgr_pdev_iterate_obj_list(pdev, WLAN_VDEV_OP,
						  wlan_check_mcast_reflect,
						  (void *)&reflect_args, 1,
						  WLAN_MLME_SB_ID);

		if ((reflect_args.status) == QDF_STATUS_E_ABORTED)
			return 0;
	}

	mat_addr = dadp_vdev_get_mataddr(vdev);
	if (!mat_addr) {
		qdf_print("%s:mat_addr is NULL ", __func__);
		return -1;
	}


	if (parp) {
		if (!ismcast) {
			if (parp->ar_op == htons(ARPOP_REQUEST)
			    || parp->ar_op == htons(ARPOP_REPLY)) {
				if (dadp_vdev_use_mat(vdev)) {
					IEEE80211_ADDR_COPY(arp_dmac,
							    dadp_vdev_get_mataddr
							    (vdev));
				}
			}
		} else {
			if (!IEEE80211_ADDR_EQ
			    (arp_dmac, mat_addr)) {
				struct wlan_wrap_rx_args wrap_args;
				wrap_args.t_vdev = NULL;
				wrap_args.arp_dmac = arp_dmac;
				wlan_objmgr_pdev_iterate_obj_list(pdev,
								  WLAN_VDEV_OP,
								  wlan_check_dest_vdev,
								  (void *)
								  &wrap_args, 1,
								  WLAN_MLME_SB_ID);
				if (wrap_args.t_vdev)
					vdev = wrap_args.t_vdev;
				else
					return 0;
			}

			if (!dadp_vdev_use_mat(vdev))
				return 0;

			QDF_TRACE( QDF_MODULE_ID_WRAP,
				       QDF_TRACE_LEVEL_DEBUG,
				       "%s: ARP tha %s -> ", __func__,
				       ether_sprintf(arp_dmac));

			IEEE80211_ADDR_COPY(arp_dmac,
					    dadp_vdev_get_mataddr(vdev));

			QDF_TRACE( QDF_MODULE_ID_WRAP,
				       QDF_TRACE_LEVEL_DEBUG,
				       "%s: ARP tha %s -> ", __func__,
				       ether_sprintf(arp_dmac));
			return 0;
		}
	}
#if 0
	/* When MAC address is changed in DHCP payload, Apple devices are showing DHCP issue
	   So, not doing MAT translation on DHCP payload.
	   Refer IR-131533 */

	else if (ether_type == htons(ETHERTYPE_IP)) {
		struct iphdr *p_ip = (struct iphdr *)(p);
		int16_t ip_hlen;

		contig_len += sizeof(struct iphdr);
		if ((pktlen < contig_len))
			return -EINVAL;

		ip_hlen = p_ip->ihl * 4;

		/* If Proto is UDP */
		if (p_ip->protocol == IPPROTO_UDP) {
			struct udphdr *p_udp =
			    (struct udphdr *)(((uint8_t *) p_ip) + ip_hlen);

			contig_len += sizeof(struct udphdr);
			if ((pktlen < contig_len))
				return -EINVAL;

#ifdef ATH_DEBUG_MAT
			printk(KERN_ERR "%s:%d sport %d dport %d\n", __func__,
			       __LINE__, p_udp->source, p_udp->dest);
#endif
			//DHCP response pkt
			if ((p_udp->source == htons(67))) {
				struct dhcp_packet *p_dhcp =
				    (struct dhcp_packet *)(((u_int8_t *) p_udp)
							   +
							   sizeof(struct
								  udphdr));

				contig_len += sizeof(struct dhcp_packet);
				if ((pktlen < contig_len))
					return -EINVAL;

#ifdef ATH_DEBUG_MAT
				printk(KERN_ERR
				       "%s:%d sport %d dport %d chaddr %s\n",
				       __func__, __LINE__, p_udp->source,
				       p_udp->dest,
				       ether_sprintf(p_dhcp->chaddr));
#endif

				if (ismcast) {
					struct ieee80211vap *vap;
					struct ieee80211vap *t_vap = NULL;
					unsigned char orig_chaddr[ETH_ALEN];
					if (!IEEE80211_ADDR_EQ
					    (p_dhcp->chaddr,
					     in_vap->iv_myaddr)) {
						TAILQ_FOREACH(vap, &ic->ic_vaps,
							      iv_next) {
							if ((ATH_VAP_NET80211
							     (vap)->
							     av_use_mat == 1)
							    &&
							    (IEEE80211_ADDR_EQ
							     (p_dhcp->chaddr,
							      vap->
							      iv_myaddr))) {
								t_vap = vap;
								break;
							}
						}
						if (t_vap)
							avn =
							    ATH_VAP_NET80211
							    (t_vap);
						else
							return 0;
					}
					if (avn->av_use_mat == 0)
						return 0;
					DPRINTF(scn, ATH_DEBUG_MAT,
						"%s: DHCP chaddr %s -> ",
						__func__,
						ether_sprintf(p_dhcp->chaddr));
					IEEE80211_ADDR_COPY(orig_chaddr,
							    p_dhcp->chaddr);
					IEEE80211_ADDR_COPY(p_dhcp->chaddr,
							    avn->av_mat_addr);
					DPRINTF(scn, ATH_DEBUG_MAT, "%s\n",
						ether_sprintf(p_dhcp->chaddr));
					p_udp->check =
					    update_checksum_addr(p_udp->check,
								 orig_chaddr,
								 avn->
								 av_mat_addr);
					return 0;
				} else {
					unsigned char orig_chaddr[ETH_ALEN];
					//unicast
					if (IEEE80211_ADDR_EQ
					    (p_dhcp->chaddr,
					     in_vap->iv_myaddr)) {
						DPRINTF(scn, ATH_DEBUG_MAT,
							"%s: DHCP chaddr %s -> ",
							__func__,
							ether_sprintf(p_dhcp->
								      chaddr));
						IEEE80211_ADDR_COPY(orig_chaddr,
								    p_dhcp->
								    chaddr);
						IEEE80211_ADDR_COPY(p_dhcp->
								    chaddr,
								    avn->
								    av_mat_addr);
						DPRINTF(scn, ATH_DEBUG_MAT,
							"%s\n",
							ether_sprintf(p_dhcp->
								      chaddr));
						p_udp->check =
						    update_checksum_addr(p_udp->
									 check,
									 orig_chaddr,
									 avn->
									 av_mat_addr);
					} else {
						return 0;
					}
				}
			}
		}
	}
#endif
	else if (ether_type == htons(ETHERTYPE_IPV6)) {
		//todo
	}
	if (!(ismcast && dadp_vdev_is_mpsta(vdev)))
		IEEE80211_ADDR_COPY(des_mac, dadp_vdev_get_mataddr(vdev));
	return 0;
}
#endif
