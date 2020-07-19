/*
 * Copyright (c) 2011,2019 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */


/*WRAP includes for MAT*/
#include <linux/udp.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <net/ndisc.h>
#include <net/arp.h>
#include "ol_if_mat.h"
#include "asf_print.h"    /* asf_print_setup */
#include "qdf_mem.h"   /* qdf_mem_malloc,free */
#include "qdf_lock.h"  /* qdf_spinlock_* */
#include "qdf_types.h" /* qdf_vprint */

#if ATH_SUPPORT_WRAP
extern void compute_udp_checksum(qdf_net_iphdr_t *p_iph, unsigned short  *ip_payload);

static uint16_t checksum(uint16_t protocol, uint16_t len, uint8_t src_addr[],
                         uint8_t dest_addr[], uint16_t addrleninbytes, uint8_t *buff)
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
	for (i = 0; i < len; i = i+2) {
        word16 = buff[i];
        word16 = (word16 << 8) + buff[i+1];
		sum = sum + (uint32_t)word16;
	}

    if (pad) {
        // Get the last byte
        word16 = buff[len];
        word16 <<= 8;
		sum = sum + (uint32_t)word16;
    }

	// add the UDP pseudo header which contains the IP source and destination addresses
	for (i = 0; i < addrleninbytes; i = i+2) {
        word16 = src_addr[i];
        word16 = (word16 << 8) + src_addr[i + 1];
		sum = sum + (uint32_t)word16;
	}


	for (i = 0; i < addrleninbytes; i = i+2) {
        word16 = dest_addr[i];
        word16 = (word16 << 8) + dest_addr[i + 1];
		sum = sum + (uint32_t)word16;
	}

	// the protocol number and the length of the s packet
	sum = sum + (uint32_t)protocol + (uint32_t)(len+pad);

	// keep only the last 16 bits of the 32 bit calculated sum and add back the carries
   	while (sum >> 16) {
	    sum = (sum & 0xFFFF) + (sum >> 16);
    }

	// Take the one's complement of sum
	sum = ~sum;

    return ((uint16_t) sum);
}

struct eth_arphdr {
	unsigned short	ar_hrd,	/* format of hardware address */
			ar_pro;	/* format of protocol address */
	unsigned char	ar_hln,	/* length of hardware address */
			ar_pln;	/* length of protocol address */
	unsigned short	ar_op;	/* ARP opcode (command) */
	unsigned char	ar_sha[ETH_ALEN],	/* sender hardware address */
			ar_sip[4],		/* sender IP address */
			ar_tha[ETH_ALEN],	/* target hardware address */
			ar_tip[4];		/* target IP address */
} __attribute__((__packed__));

struct dhcp_option {
    u_char type;
    u_char len;
    u_char value[0];	/* option value*/
} __attribute__((__packed__));

struct dhcp_packet {
    u_char              op;          /* packet opcode type */
    u_char              htype;       /* hardware addr type */
    u_char              hlen;        /* hardware addr length */
    u_char              hops;        /* gateway hops */
    u_int32_t           xid;         /* transaction ID */
    u_int16_t           secs;        /* seconds since boot began */
    u_int16_t           flags;       /* flags */
    struct in_addr      ciaddr;      /* client IP address */
    struct in_addr      yiaddr;      /* 'your' IP address */
    struct in_addr      siaddr;      /* server IP address */
    struct in_addr      giaddr;      /* gateway IP address */
    u_char              chaddr[16];  /* client hardware address */
    u_char              sname[64];   /* server host name */
    u_char              file[128];   /* boot file name */
    u_char              magic_cookie[4];   /* magic cookie */
    u_char              options[0];  /* variable-length options field */
} __attribute__((__packed__));

struct eth_icmp6_lladdr {
    unsigned char type;
    unsigned char len;
    unsigned char addr[6];	/* hardware address */
} __attribute__((__packed__));

typedef struct eth_icmp6_lladdr eth_icmp6_lladdr_t;

#define OPTION_DHCP_MSG_TYPE   0x35        /* Option: (53) DHCP Message Type  */
#define DHCP_DISOVER    1                  /* DHCP: Discover (1) */
#define OPTION_DHCP_REQUESTED_IP_ADDR   0x32 /*Option: (50) Requested IP Address */

/**
 * @brief WRAP MAT function for transmit.
 *
 * @param out_vap Output VAP.
 * @param buf
 *
 * @return 0 On success.
 * @return -ve On failure.
 */
int ol_if_wrap_mat_tx(struct ieee80211vap *out_vap, wbuf_t buf)
{
    struct ol_ath_vap_net80211 *avn;
    struct ol_ath_softc_net80211 *scn;
    struct ieee80211com *ic;
    struct ether_header *eh;
    uint16_t ether_type;
    int contig_len = sizeof(struct ether_header);
    int pktlen = wbuf_get_pktlen(buf);
    uint8_t *src_mac,*des_mac,*p,ismcast;
    uint8_t *arp_smac = NULL;
    uint8_t *arp_dmac = NULL;
    struct eth_arphdr *parp = NULL;

	if(!(OL_ATH_VAP_NET80211(out_vap)->av_use_mat))
		return 0;

    if ((pktlen < contig_len))
        return -EINVAL;

    eh = (struct ether_header *)(wbuf_header(buf));
    p = (uint8_t*)(eh+1);

    ether_type = eh->ether_type;
    src_mac = eh->ether_shost;
    des_mac = eh->ether_dhost;
    ismcast = IEEE80211_IS_MULTICAST(des_mac);

#ifdef ATH_MAT_TEST
	printk(KERN_ERR "%s: src %s type 0x%x",__func__,ether_sprintf(src_mac),ether_type);
	printk(KERN_ERR "des %s\n",ether_sprintf(des_mac));
#endif

	if (ether_type == htons(ETH_P_PAE))
			return 0;

	if (ether_type == htons(ETHERTYPE_ARP)) {
			parp = (struct eth_arphdr *)p ;
			contig_len += sizeof(struct eth_arphdr);

			if ((pktlen < contig_len))
				return -EINVAL;

		if(parp->ar_hln == ETH_ALEN && parp->ar_pro == htons(ETH_P_IP)) {
			arp_smac = parp->ar_sha;
			arp_dmac = parp->ar_tha;
		} else {
			parp = NULL;
		}
	}

	avn = OL_ATH_VAP_NET80211(out_vap);
	ic = out_vap->iv_ic;
	scn = OL_ATH_SOFTC_NET80211(ic);
	if(parp){
            if (parp->ar_op == htons(ARPOP_REQUEST) ||  parp->ar_op == htons(ARPOP_REPLY)) {
                IEEE80211_ADDR_COPY(arp_smac,out_vap->iv_myaddr);
            }
	}else if (ether_type == htons(ETHERTYPE_IP)) {
		struct iphdr *p_ip = (struct iphdr *)(p);
		int16_t ip_hlen;

		contig_len += sizeof(struct iphdr);
		if ((pktlen < contig_len))
    		return -EINVAL;

		ip_hlen = p_ip->ihl * 4;

		/* If Proto is UDP */
		if (p_ip->protocol == IPPROTO_UDP) {
			qdf_net_udphdr_t *p_udp = (qdf_net_udphdr_t *) (((uint8_t *)p_ip) + ip_hlen);
			uint16_t udplen;


			contig_len += sizeof(struct udphdr);
			if ((pktlen < contig_len))
    				return -EINVAL;

			udplen = p_ip->tot_len - (p_ip->ihl * 4);
#ifdef ATH_DEBUG_MAT
			printk(KERN_ERR "%s:%d sport %d dport %d\n",__func__,__LINE__,p_udp->source,p_udp->dest);
#endif
			/*
			* DHCP request UDP Client SP = 68 (bootpc), DP = 67 (bootps).
			*/
			if ((p_udp->dst_port == htons(67))) {
				struct dhcp_packet *p_dhcp = (struct dhcp_packet *)(((u_int8_t *)p_udp)+sizeof(struct udphdr));
				struct dhcp_option *option = (struct dhcp_option *)p_dhcp->options;
				u_int8_t *value;
                                uint8_t dhcpDicover = 0;

				contig_len += sizeof(struct dhcp_packet);
				if ((pktlen < contig_len))
    					return -EINVAL;

#ifdef ATH_DEBUG_MAT
				printk(KERN_ERR "%s:%d sport %d dport %d len %d chaddr %s\n",__func__,__LINE__,p_udp->source,p_udp->dest,udplen,ether_sprintf(p_dhcp->chaddr));
#endif
				if(p_dhcp->magic_cookie[0] == 0x63 && p_dhcp->magic_cookie[1] == 0x82 && p_dhcp->magic_cookie[2] == 0x53 && p_dhcp->magic_cookie[3] == 0x63) {
#ifdef ATH_DEBUG_MAT
					printk(" Found DHCP cookie .. \n");
#endif
					if (p_dhcp->op == 1) {
                                                /* dhcp REQ or DISCOVER*/

#ifdef BIG_ENDIAN_HOST
						if((p_dhcp->flags & 0x8000) == 0) {
							p_dhcp->flags |= 0x8000;
#else
						if((p_dhcp->flags & 0x0080) == 0) {
							p_dhcp->flags |= 0x0080;
#endif

#ifdef ATH_DEBUG_MAT
							printk(" Make the dhcp flag to broadcast ... \n");
#endif
							compute_udp_checksum((qdf_net_iphdr_t *)p_ip, (unsigned short *)p_udp);
						}
					}
				} /* Magic cookie for DHCP*/

				while (option->type != 0xFF) {
					/*Not an end option*/

					contig_len += (option->len + 2);
					if ((pktlen < contig_len))
						return -EINVAL;

                                        value = option->value;
                                        if(option->type == OPTION_DHCP_MSG_TYPE ) {
                                            if(value[0] == DHCP_DISOVER ) {
                                                dhcpDicover = 1;
                                            }
                                            option = (struct dhcp_option *)(value + option->len);

                                        } else if (option->type == OPTION_DHCP_REQUESTED_IP_ADDR && dhcpDicover == 1 ) {
                                            uint32_t    *addr = (uint32_t *)value;
                                            if(*addr != 0) {
                                                qdf_print("%s:%d:DHCP Disover with Non Zero IP Addr: %u.%u.%u.%u from caddr:%s ",
                                                        __func__,__LINE__,value[0],value[1],value[2],value[3],ether_sprintf(p_dhcp->chaddr));
                                                memset(value,0,option->len);
                                                compute_udp_checksum((qdf_net_iphdr_t *)p_ip, (unsigned short *)p_udp);
                                            }

                                            break;
                                        } else {
                                            option = (struct dhcp_option *)(value + option->len);
                                        }
				}
			}
		}
	}else if (ether_type == htons(ETHERTYPE_IPV6)) {
		struct ipv6hdr *p_ip6 = (struct ipv6hdr *)(p);
        eth_icmp6_lladdr_t  *phwaddr;
        int change_packet = 1;

        contig_len += sizeof(struct ipv6hdr);
        if (pktlen < contig_len)
    		return -EINVAL;

        if (p_ip6->nexthdr == IPPROTO_ICMPV6) {
            struct icmp6hdr *p_icmp6 = (struct icmp6hdr *)(p_ip6 + 1);
            uint16_t icmp6len = ntohs(p_ip6->payload_len);

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
                    contig_len += 16 + sizeof(eth_icmp6_lladdr_t);
                    if (pktlen < contig_len)
    					return -EINVAL;

                    phwaddr = (eth_icmp6_lladdr_t *)((uint8_t *)(p_icmp6+1)+16);
                    IEEE80211_ADDR_COPY(phwaddr->addr, out_vap->iv_myaddr);
                    p_icmp6->icmp6_cksum = 0;
                    p_icmp6->icmp6_cksum = htons(checksum((uint16_t)IPPROTO_ICMPV6, icmp6len,
					   p_ip6->saddr.s6_addr, p_ip6->daddr.s6_addr,

				           16 /* IPv6 has 32 byte addresses */, (uint8_t *)p_icmp6));
                    break;
                }
            	case NDISC_ROUTER_SOLICITATION:
                {
                    contig_len += sizeof(eth_icmp6_lladdr_t);
                    if (pktlen < contig_len)
    					return -EINVAL;

                    /* replace the HW address with the VMA */
                    phwaddr = (eth_icmp6_lladdr_t *)((uint8_t *)(p_icmp6 + 1));
                    break;
                }
            	default:
                	change_packet = 0;
                break;
            }

            if (change_packet) {
                IEEE80211_ADDR_COPY(phwaddr->addr, out_vap->iv_myaddr);
                p_icmp6->icmp6_cksum = 0;
                p_icmp6->icmp6_cksum = htons(checksum((uint16_t)IPPROTO_ICMPV6, icmp6len,
                            p_ip6->saddr.s6_addr, p_ip6->daddr.s6_addr,
                            16 /* IPv6 has 32 byte addresses */, (uint8_t *)p_icmp6));
            }
        }
	}
	IEEE80211_ADDR_COPY(src_mac,out_vap->iv_myaddr);
	return 0;
}
qdf_export_symbol(ol_if_wrap_mat_tx);

/**
 * @brief WRAP MAT on receive path.
 *
 * @param in_vap In VAP
 * @param buf
 *
 * @return 0 On success.
 * @return -ve On failure.
 */
int ol_if_wrap_mat_rx(struct ieee80211vap *in_vap, wbuf_t buf)
{
    struct ol_ath_softc_net80211 *scn;
    struct ieee80211com *ic;
    struct ether_header *eh;
    uint16_t ether_type;
    int contig_len = sizeof(struct ether_header);
    int pktlen = wbuf_get_pktlen(buf);
    uint8_t *src_mac,*des_mac,*p,ismcast;
    uint8_t *arp_smac = NULL;
    uint8_t *arp_dmac = NULL;
    struct eth_arphdr *parp = NULL;
    struct ol_ath_vap_net80211 *avn = OL_ATH_VAP_NET80211(in_vap);

    eh = (struct ether_header *)(wbuf_header(buf));
	p = (uint8_t*)(eh+1);

	if(!avn->av_is_wrap && !avn->av_is_psta)
		return 0;

    if ((pktlen < contig_len))
        return -EINVAL;

    ether_type = eh->ether_type;
	src_mac = eh->ether_shost;
	des_mac = eh->ether_dhost;
	ismcast = IEEE80211_IS_MULTICAST(des_mac);

   	if (ether_type == htons(ETH_P_PAE)){
		//mark the pkt to allow local delivery on the device
		buf->mark |= WRAP_MARK_ROUTE;
		return 0;
    }


#ifdef ATH_MAT_TEST
	printk(KERN_ERR "%s: src %s type 0x%x",__func__,ether_sprintf(src_mac),ether_type);
	printk(KERN_ERR "des %s\n",ether_sprintf(des_mac));
#endif



	if (ether_type == htons(ETHERTYPE_ARP)) {
		parp = (struct eth_arphdr *)p ;
		contig_len += sizeof(struct eth_arphdr);

			if ((pktlen < contig_len))
				return -EINVAL;

		if(parp->ar_hln == ETH_ALEN && parp->ar_pro == htons(ETH_P_IP)) {
			arp_smac = parp->ar_sha;
			arp_dmac = parp->ar_tha;
		} else {
			parp = NULL;
		}
	}

	ic = in_vap->iv_ic;
	scn = OL_ATH_SOFTC_NET80211(ic);

    if(ismcast && avn->av_is_mpsta){
		struct ieee80211vap *vap=NULL;
        TAILQ_FOREACH(vap,&ic->ic_vaps,iv_next){
			if (IEEE80211_ADDR_EQ(src_mac,vap->iv_myaddr)){
			    buf->mark |= WRAP_MARK_REFLECT;
				avn = OL_ATH_VAP_NET80211(vap);
				//translate src address else bridge view is consistent
				if(OL_ATH_VAP_NET80211(vap)->av_use_mat == 1)
				    IEEE80211_ADDR_COPY(src_mac,avn->av_mat_addr);
                return 0;
            }
        }
     }

	if(parp){
		if(!ismcast){
                    if (parp->ar_op == htons(ARPOP_REQUEST) ||  parp->ar_op == htons(ARPOP_REPLY)) {
                        if(avn->av_use_mat == 1) {
		            IEEE80211_ADDR_COPY(arp_dmac,avn->av_mat_addr);
                        }
                    }
		}else{
			struct ieee80211vap *t_vap=NULL;
			struct ieee80211vap *vap;
			if(!IEEE80211_ADDR_EQ(arp_dmac,avn->av_mat_addr)) {
				TAILQ_FOREACH(vap,&ic->ic_vaps,iv_next){
					if((OL_ATH_VAP_NET80211(vap)->av_use_mat == 1) \
						&& (IEEE80211_ADDR_EQ(arp_dmac,vap->iv_myaddr))){
						t_vap=vap;
						break;
					}
				}
				if(t_vap)
					avn = OL_ATH_VAP_NET80211(t_vap);
				else
					return 0;
			}

            if(avn->av_use_mat==0)
                return 0;

			IEEE80211_ADDR_COPY(arp_dmac,avn->av_mat_addr);
			return 0;
		}
	}

  else if (ether_type == htons(ETHERTYPE_IPV6)){
		//todo
	}

    if (!(ismcast) && !(avn->av_is_mpsta) && (avn->av_use_mat == 1))
	    IEEE80211_ADDR_COPY(des_mac,avn->av_mat_addr);
	return 0;
}
qdf_export_symbol(ol_if_wrap_mat_rx);
#endif
