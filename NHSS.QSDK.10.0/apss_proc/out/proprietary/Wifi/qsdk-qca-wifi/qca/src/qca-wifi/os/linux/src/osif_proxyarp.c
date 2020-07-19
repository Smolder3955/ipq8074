/*
 * Copyright (c) 2014 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#if UMAC_SUPPORT_PROXY_ARP

#include <net/arp.h>    /* arp_send */
#include <net/ip.h>   /* ipv6 */
#include <net/ipv6.h>   /* ipv6 */
#include <net/ndisc.h>  /* ipv6 ndp */

#include "osif_private.h"
#include <wlan_opts.h>
#include <ieee80211_var.h>
#include "if_athvar.h"

#define OSIF_TO_NETDEV(_osif) (((osif_dev *)(_osif))->netdev)
/*
 * IS_MYNODE includes every nodes in the BSS, while IS_MYSTA
 * doesn't include the BSS node itself.
 */
#define IS_MYNODE(_v, _n) (_n && (_n)->ni_vap == _v && (_n)->ni_associd)
#define IS_MYSTA(_v, _n) (IS_MYNODE(_v, _n) && _n != (_v)->iv_bss)

struct dhcp_packet {            /* BOOTP/DHCP packet format */
        struct iphdr iph;       /* IP header */
        struct udphdr udph;     /* UDP header */
        u8 op;                  /* 1=request, 2=reply */
        u8 htype;               /* HW address type */
        u8 hlen;                /* HW address length */
        u8 hops;                /* Used only by gateways */
        u32 xid;             /* Transaction ID */
        u16 secs;            /* Seconds since we started */
        u16 flags;           /* Just what it says */
        u32 client_ip;       /* Client's IP address if known */
        u32 your_ip;         /* Assigned IP address */
        u32 server_ip;       /* (Next, e.g. NFS) Server's IP address */
        u32 relay_ip;        /* IP address of BOOTP relay */
        u8 hw_addr[16];         /* Client's HW address */
        u8 serv_name[64];       /* Server host name */
        u8 boot_file[128];      /* Name of boot file */
        u8 exten[312];          /* DHCP options / BOOTP vendor extensions */
}; /* packed naturally */

static const u8 ic_bootp_cookie[4] = { 99, 130, 83, 99 };

#define DHCPOFFER   2
#define DHCPACK     5

#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,15,0)
static inline int ipv6_addr_is_multicast(const struct in6_addr *addr)
{
    return (addr->s6_addr32[0] & htonl(0xFF000000)) == htonl(0xFF000000);
}
#endif

static u8 *ipv6_ndisc_opt_lladdr(u8 *opt, int optlen, int src)
{
    struct nd_opt_hdr *ndopt = (struct nd_opt_hdr *)opt;
    int len;

    while (optlen) {
        if (optlen < sizeof(struct nd_opt_hdr))
            return NULL;

        len = ndopt->nd_opt_len << 3;
        if (optlen < len || len == 0)
            return NULL;

        switch (ndopt->nd_opt_type) {
        case ND_OPT_TARGET_LL_ADDR:
            if (!src) {
                return (u8 *)(ndopt + 1);
            }

            break;
        case ND_OPT_SOURCE_LL_ADDR:
            if (src) {
                return (u8 *)(ndopt + 1);
            }

            break;

        default:
            break;
        }
        optlen -= len;
        ndopt = (void *)ndopt + len;
    }

    return NULL;
}

/*
 * IEEE 802.11v Proxy ARP
 *
 * When enabled, AP is responsible for sending ARP responses
 * on behalf of its associated clients. Gratuitous ARP's are
 * dropped sliently wthin the BSS.
 */
int wlan_proxy_arp(wlan_if_t vap, wbuf_t wbuf)
{
    struct net_device *dev = OSIF_TO_NETDEV(vap->iv_ifp);
    struct ether_header *eh = (struct ether_header *) wbuf_header(wbuf);
    struct ieee80211_node_table *nt = &vap->iv_ic->ic_sta;
    struct ieee80211_node *ni;
    uint16_t ether_type;
    int linear_len;
#if (defined(CONFIG_IPV6) && CONFIG_IPV6 !=0) || (defined(CONFIG_IPV6_MODULE) && CONFIG_IPV6_MODULE !=0)
    uint8_t eth_ipv6_mcast_addr[] = { 0x33, 0x33, 0, 0, 0, 1 };
#endif
    uint8_t eth_zero_addr[] = { 0, 0, 0, 0, 0, 0 };

    KASSERT(vap->iv_opmode == IEEE80211_M_HOSTAP, ("Proxy ARP in !AP mode"));

    if (IEEE80211_VAP_IS_DELIVER_80211_ENABLED(vap)) {
        printk("%s: IEEE80211_FEXT_DELIVER_80211 is not supported\n", __func__);
        goto pass;
    }
    if (eh == NULL)
        goto drop;

    ether_type = eh->ether_type;
    linear_len = sizeof(*eh);

    if (ether_type == htons(ETHERTYPE_ARP)) {
        struct arphdr *arp = (struct arphdr *)(eh + 1);
        unsigned char *arp_ptr;
        unsigned char *sha, *tha;
        __be32 sip, tip;

        linear_len += sizeof(struct arphdr);
        if (!pskb_may_pull(wbuf, linear_len))
            goto pass;

        arp_ptr = (unsigned char *)(arp + 1);
        sha = arp_ptr;
        arp_ptr += dev->addr_len;
        memcpy(&sip, arp_ptr, 4);
        arp_ptr += 4;
        tha = arp_ptr;
        arp_ptr += dev->addr_len;
        memcpy(&tip, arp_ptr, 4);

        if (arp->ar_op == htons(ARPOP_REQUEST)) {
            int bridge = 1;

            ni = ieee80211_find_node(nt, sha);
            if (IS_MYSTA(vap, ni))
                bridge = 0;
            if (ni)
                ieee80211_free_node(ni);

            /* Proxy ARP request for the clients within the BSS */
            ni = ieee80211_find_node_by_ipv4(nt, tip);
            if (IS_MYSTA(vap, ni)) {
                if (ni->ni_ipv4_lease_timeout != 0 &&
                    ni->ni_ipv4_lease_timeout < jiffies_to_msecs(jiffies) / 1000)
                {
                    /* remove the node from the ipv4 hash table */
                    IEEE80211_DPRINTF(vap, IEEE80211_MSG_PROXYARP, "Remove "
                            "ARP entry: mac %s -> ip %pI4 due to timeout. "
                            "expire %d, current %d\n",
                            ether_sprintf(ni->ni_macaddr), &tip,
                            ni->ni_ipv4_lease_timeout,
                            jiffies_to_msecs(jiffies) / 1000);
                    ieee80211_node_remove_ipv4(nt, ni);
                } else if (memcmp(ni->ni_macaddr, sha, IEEE80211_ADDR_LEN)) {
                    int defense = 0;
                    int arpop = ARPOP_REPLY;

                    /*
                     * Is this a defense ARP Announcement frame from the bridge?
                     */
                    if (bridge && tip == sip &&
                        !memcmp(tha, eth_zero_addr, IEEE80211_ADDR_LEN))
                    {
                        /*
                         * ARP Announcement from the bridge for a previous
                         * ARP Probe Request sent by our associated STA's.
                         * We need to remove the IPv4 address we learned
                         * previously (from the ARP Probe frame).
                         */
                        IEEE80211_DPRINTF(vap, IEEE80211_MSG_PROXYARP, "Remove "
                            "ARP entry: mac %s -> ip %pI4 due to defense ARP "
                            "Announcement Request frame received from bridge.\n",
                            ether_sprintf(ni->ni_macaddr), &tip);
                        ieee80211_node_remove_ipv4(nt, ni);

                        /* allow the Defense ARP Announcement from bridge go through */
                        goto pass_node;
                    }

                    /* Suppress Gratuitous ARP request from both BSS and bridge */
                    if (tip == sip) {
                        goto drop_node;
                    }

                    /*
                     * IPv4 Address Conflict Detection (ACD), RFC5227
                     *
                     * When ACD ARP Probe is received, the AP needs to send
                     * an Defense ARP Announcement on behalf the STA which
                     * contains the IPv4 address. This is the IPv4 version
                     * of the Proxy DAD.
                     *
                     * ACD ARP Probe frame format:
                     *      ARP Type:           ARP Request
                     *      Sender Mac Address: Sender's Mac (either wireless or wired)
                     *      Sender IP Address:  all zeros
                     *      Target Mac Address: doesn't matter, usually broadcast addr
                     *      Target IP Address:  the IPv4 address to be probed
                     *
                     * ACD ARP Announcement frame format:
                     *      ARP Type:           ARP Request
                     *      Sender Mac Address: STA's Mac address
                     *      Sender IP Address:  STA's IPv4 address
                     *      Target Mac Address: all zeros
                     *      Target IP Address:  STA's IPv4 address
                     */
                    if (sip == 0) {
                        /* ARP Probe is received, form an ARP Announcement */
                        tha = eth_zero_addr;
                        sip = tip;
                        defense = 1;
                        arpop = ARPOP_REQUEST;
                    } else {
                        /* Regular ARP Request is received, form an ARP Reply */
                        tha = sha;
                    }

                    /*
                     * Send the proxy ARP reply: if the ARP request sender is
                     * within the BSS, we can send the ARP reply directly.
                     * Otherwise it must be coming from the bridge. If this
                     * is the case, we assemble an ARP reply packet and give
                     * it to the local stack so that the bridge code can
                     * forward it to the ARP request sender.
                     */
                    if (bridge) {
                        struct sk_buff *skb;

                        skb = arp_create(ARPOP_REPLY, ETH_P_ARP, sip, dev, tip,
                                         sha, ni->ni_macaddr, tha);
                        if (skb)
                            __osif_deliver_data(vap->iv_ifp, skb);
                    } else {
                        struct ieee80211_node *sni;
#ifdef HOST_OFFLOAD
                        struct sk_buff *skb;

                        skb = arp_create(ARPOP_REPLY, ETH_P_ARP, sip, dev, tip,
                                         sha, ni->ni_macaddr, tha);
                        if (skb)
                            atd_proxy_arp_send(skb);
#else
                        arp_send(ARPOP_REPLY, ETH_P_ARP, sip, dev, tip, sha,
                                 ni->ni_macaddr, tha);
#endif
                        sni = ieee80211_find_node_by_ipv4(nt, sip);
                        if (!sni) {
                            sni = ieee80211_find_node(nt, sha);
                            if (IS_MYSTA(vap, sni) &&
                                !memcmp(sha, sni->ni_macaddr, IEEE80211_ADDR_LEN)) {
                                /* Learn from the ARP requesting STA in our BSS */
                                IEEE80211_DPRINTF(vap, IEEE80211_MSG_PROXYARP,
                                                  "ARP Request: learn "
                                                  "src mac %s -> ip %pI4\n",
                                                  ether_sprintf(sha), &sip);
                                ieee80211_node_add_ipv4(nt, sni, sip);
                            }
                        }
                        if (sni)
                            ieee80211_free_node(sni);
                    }
                    IEEE80211_DPRINTF(vap, IEEE80211_MSG_PROXYARP, "send %s "
                                      "(arpop %d) for ip %pI4 -> mac %s to %pM %s\n",
                                      defense ? "Defense ARP Announcement "
                                      : " ARP reply", arpop, &tip,
                                      ether_sprintf(ni->ni_macaddr), sha,
                                      bridge ? "via local stack" : "over the air");
                    /* Note: the original frame will be dropped */
                } else if (!memcmp(ni->ni_macaddr, sha, IEEE80211_ADDR_LEN)) {
                    /*
                     * The AP initiated defense ARP Announcement frame will
                     * finally come here before going out.
                     */
                     if (tip == sip && !memcmp(tha, eth_zero_addr, IEEE80211_ADDR_LEN) &&
                         !IEEE80211_IS_BROADCAST(eh->ether_dhost))
                     {
                         goto pass_node;
                     }
                }

                goto drop_node;
            }
            /*
             * The ARP Request Target IP Address is not in our neighbor cache.
             *
             * We can try to learn some from the Gratuitous ARP request
             * or RFC 5227 ARP Probe.
             */
            if (tip == sip || sip == 0) {
                /* release the previous ni */
                if (ni)
                    ieee80211_free_node(ni);

                 ni = ieee80211_find_node(nt, sha);

                 /*
                  * Learn ARP mapping from Gratuitous ARP request. Note we
                  * allow the IPv4 address update from a later Gratuitous
                  * ARP to overwrite a previous one.
                  */
                 if (IS_MYSTA(vap, ni)) {
                     IEEE80211_DPRINTF(vap, IEEE80211_MSG_PROXYARP, "%s Request: "
                            "mac %s -> ip %pI4\n",
                            sip ? "Gratuitous ARP" : "ARP Probe",
                            ether_sprintf(sha), &tip);
                     ieee80211_node_add_ipv4(nt, ni, tip);

                     /*
                      * If we receive a gratuitous ARP for a previously
                      * expired DHCP lease, change the lease type to
                      * ulimited.
                      */
                     if (ni->ni_ipv4_lease_timeout &&
                         ni->ni_ipv4_lease_timeout < jiffies_to_msecs(jiffies) / 1000)
                     {
                         ni->ni_ipv4_lease_timeout = 0;
                     }
                }
            }

            /* Suppress ARP Request within the BSS */
            goto drop_node;

        } else if (arp->ar_op == htons(ARPOP_REPLY)) {
            /* Suppress Gratuitous ARP reply within the BSS */
            if (IEEE80211_IS_BROADCAST(eh->ether_dhost))
                goto drop;
            else
                goto pass;
        }
    } else if (ether_type == htons(ETHERTYPE_IP)) {
        struct dhcp_packet *dhcp = (struct dhcp_packet *)(eh + 1);
        int len, ext_len;

        /* Do not pull too deep ATM */
        linear_len += sizeof(struct iphdr) + sizeof(struct udphdr) + 1;
        if (!pskb_may_pull(wbuf, linear_len))
            goto pass;


        /*
         * We only care about downstream DHCP packets i.e.
         * DHCPOFFER and DHCPACK
         */
        if (dhcp->iph.protocol != IPPROTO_UDP ||
            ntohs(dhcp->udph.source) != 67 ||
            ntohs(dhcp->udph.dest) != 68 ||
            dhcp->op != 2) /* BOOTP Reply */
        {
            goto pass;
        }

        /* If it is a unicast packet, make sure it is within our BSS */
        if (!IEEE80211_IS_BROADCAST(eh->ether_dhost)) {
            ni = ieee80211_find_node(nt, eh->ether_dhost);
            if (!IS_MYNODE(vap, ni))
                goto pass_node;
            if (ni)
                ieee80211_free_node(ni);
        }

        if (wbuf->len < ntohs(dhcp->iph.tot_len))
            goto pass;

        if (ntohs(dhcp->iph.tot_len) < ntohs(dhcp->udph.len) + sizeof(struct iphdr))
            goto pass;

        len = ntohs(dhcp->udph.len) - sizeof(struct udphdr);
        ext_len = len - (sizeof(*dhcp) -
                         sizeof(struct iphdr) -
                         sizeof(struct udphdr) -
                         sizeof(dhcp->exten));
        if (ext_len < 0)
            goto pass;

        /* linearize the skb */
        if (skb_linearize(wbuf))
            goto pass;

        /* reload pointer after skb_linearize */
        dhcp = (struct dhcp_packet *)(eh + 1);

        if(!dhcp) {
            goto pass;
        }

        /* Parse extensions */
        if (ext_len >= 4 && !memcmp(dhcp->exten, ic_bootp_cookie, 4)) {
            u8 *end = (u8 *)dhcp + ntohs(dhcp->iph.tot_len);
            u8 *ext = &dhcp->exten[4];
            int mt = 0;
            uint32_t lease_time = 0;

            while (ext < end && *ext != 0xff) {
                u8 *opt = ext++;
                if (*opt == 0) /* padding */
                    continue;
                ext += *ext + 1;
                if (ext >= end)
                    break;
                switch (*opt) {
                case 53: /* message type */
                    if (opt[1])
                        mt = opt[2];
                    break;
                case 51: /* lease time */
                    if (opt[1] == 4)
                        lease_time = ntohl(*(uint32_t *)&opt[2]);
                    break;
                }
            }

            if (mt == DHCPACK) {
                ni = ieee80211_find_node(nt, dhcp->hw_addr);
                if (IS_MYSTA(vap, ni)) {
                    if (dhcp->your_ip == 0) {
                        /* DHCPACK for DHCPINFORM */
                        IEEE80211_DPRINTF(vap, IEEE80211_MSG_PROXYARP,
                            "DHCPACK: respond to DHCPINFORM for mac %s "
                            "lease %u seconds\n",
                            ether_sprintf(dhcp->hw_addr), lease_time);
                        goto pass_node;
                    }

                    /* DHCPACK for DHCPREQUEST */
                    ieee80211_node_add_ipv4(nt, ni, dhcp->your_ip);

                    if (lease_time)
                        ni->ni_ipv4_lease_timeout = jiffies_to_msecs(jiffies) / 1000 + lease_time;
                    else
                        ni->ni_ipv4_lease_timeout = 0;

                    IEEE80211_DPRINTF(vap, IEEE80211_MSG_PROXYARP, "DHCPACK: "
                            "mac %s -> ip %pI4, lease %u seconds\n",
                            ether_sprintf(dhcp->hw_addr), &dhcp->your_ip,
                            lease_time);
                }
                if (ni)
                    ieee80211_free_node(ni);
            }
#if UMAC_SUPPORT_DGAF_DISABLE
            if ((mt == DHCPOFFER || mt == DHCPACK) &&
                IEEE80211_IS_BROADCAST(eh->ether_dhost))
            {
                ni = ieee80211_find_node(nt, dhcp->hw_addr);
                if (IS_MYSTA(vap, ni)) {
                    /* Convert broadcast DHCP reply to unicast */
                    IEEE80211_DPRINTF(vap, IEEE80211_MSG_PROXYARP, "convert "
                        "broadcast %s packet to unicast %s\n",
                        mt == DHCPOFFER ? "DHCPOFFER" : "DHCPACK",
                        ether_sprintf(dhcp->hw_addr));
                    memcpy(eh->ether_dhost, dhcp->hw_addr, ETH_ALEN);
                    ieee80211_free_node(ni);
                } else {
                    IEEE80211_DPRINTF(vap, IEEE80211_MSG_PROXYARP, "dropping "
                        "broadcast %s packet for %s\n",
                        mt == DHCPOFFER ? "DHCPOFFER" : "DHCPACK",
                        ether_sprintf(dhcp->hw_addr));
                    goto drop_node;
                }
            }
#endif
        }
    }
#if (defined(CONFIG_IPV6) && CONFIG_IPV6 !=0) || (defined(CONFIG_IPV6_MODULE) && CONFIG_IPV6_MODULE !=0)
     else if (ether_type == htons(ETHERTYPE_IPV6)) {
        struct ipv6hdr *ip6 = (struct ipv6hdr *)(eh + 1);

        linear_len += sizeof(struct ipv6hdr);
        if (!pskb_may_pull(wbuf, linear_len))
            goto pass;

        if (ip6->nexthdr == IPPROTO_ICMPV6) {
            struct icmp6hdr *icmp6 = (struct icmp6hdr *)(ip6 + 1);
            struct nd_msg *msg = (struct nd_msg *)icmp6;
            int optlen = wbuf->len - linear_len - sizeof(struct nd_msg);
            u8 *lladdr;
            int src_type, dst_type;
            int unsolicited;
            int is_dad_transmit = 0;

            linear_len += sizeof(struct icmp6hdr);
            if (!pskb_may_pull(wbuf, linear_len))
                goto pass;

            switch (icmp6->icmp6_type) {
            case NDISC_NEIGHBOUR_SOLICITATION:
                /*
                 * Learn neighbor mapping from DupAddrDetectTransmits
                 * Neighbor Solicitations:
                 *
                 *     ND Target:       address being checked
                 *     IP source:       unspecified address (::)
                 *     IP destination:  solicited-node multicast address
                 *
                 */
                src_type = ipv6_addr_type(&ip6->saddr);
                dst_type = ipv6_addr_type(&ip6->daddr);
                if ((src_type == IPV6_ADDR_ANY) && (dst_type & IPV6_ADDR_MULTICAST)) {
                    struct ieee80211_node *ni1;

                    /* See if the addr is already in our neighbor cache */
                    ni = ieee80211_find_node(nt, eh->ether_shost);
                    ni1 = ieee80211_find_node_by_ipv6(nt, (u8 *)&msg->target);
                    if (ni1) {
                        /* No interest in STA's own DAD transmit */
                        if (ni1 == ni) {
                            ieee80211_free_node(ni1);
                            goto drop_node;
                        }

                        IEEE80211_DPRINTF(vap, IEEE80211_MSG_PROXYARP, "NDP SC: "
                                  "ipv6 %pI6 -> mac %pM ignored (DAD w/ %pM)\n",
                                  &msg->target, eh->ether_shost, ni1->ni_macaddr);

                        ieee80211_free_node(ni1);
                        if (ni)
                            ieee80211_free_node(ni);

                        /*
                         * Reply DAD NA on behalf of the STA. lladdr points
                         * to the ethernet IPv6 multicast address (33:33:0:0:0:1)
                         * which corresponds to the IPv6 all nodes multicast
                         * address (FF02::1).
                         */
                        is_dad_transmit = 1;
                        lladdr = eth_ipv6_mcast_addr;
                        goto send_na;
                    }

                    if (IS_MYNODE(vap, ni)) {
                        /* Learn from this SC if it is from our associated STA's */
                        if (ieee80211_node_add_ipv6(nt, ni, (u8 *)&msg->target)) {
                            printk("Maximum multiple IPv6 addresses exceeded "
                                    "[%d]. Proxy ARP disabled. Consider to "
                                    "increase IEEE80211_NODE_IPV6_MAX!\n",
                                    IEEE80211_NODE_IPV6_MAX);
                            ieee80211_vap_proxyarp_clear(vap);
                            goto pass_node;
                        }

                        IEEE80211_DPRINTF(vap, IEEE80211_MSG_PROXYARP, "NDP "
                                "SC: mac %s -> ipv6 %pI6\n",
                                ether_sprintf(eh->ether_shost), &msg->target);
                    }
            	    /* always suppress DupAddrDetectTransmits */
                    goto drop_node;
                }

                /*
            	 * Normal Neighbor Solicitation (vs DupAddrDetectTransmits)
            	 * handling begins here. It must have the ICMPv6 Option.
            	 */
                lladdr = ipv6_ndisc_opt_lladdr(msg->opt, optlen, 1);
                if (!lladdr)
                        goto drop;

send_na:
                ni = ieee80211_find_node_by_ipv6(nt, (u8 *)&msg->target);
                if (IS_MYSTA(vap, ni)) {
                    struct sk_buff *skb;
                    struct ipv6hdr *nip6;
                    struct nd_msg *nmsg;
                    struct ieee80211_node *ni1;

                    skb = alloc_skb(
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0)
                                    LL_RESERVED_SPACE(dev) +
                                    dev->needed_tailroom +
#else
                                    LL_ALLOCATED_SPACE(dev) +
#endif
                                    sizeof(struct ipv6hdr) +
                                    sizeof(struct nd_msg) +
                                    8, /* ICMPv6 option */
                                    GFP_ATOMIC);
                    if (!skb) {
                        IEEE80211_DPRINTF(vap, IEEE80211_MSG_PROXYARP,
                                "%s: couldn't alloc skb\n");
                        goto drop_node;
                    }
                    skb_reserve(skb, LL_RESERVED_SPACE(dev));
                    nip6 = (struct ipv6hdr *) skb_put(skb, sizeof(struct ipv6hdr));
                    skb->dev = dev;
                    skb->protocol = htons(ETHERTYPE_IPV6);
                    skb_put(skb, sizeof(struct nd_msg) + 8);
                    nmsg = (struct nd_msg *)(nip6 + 1);

                    /* Response with NDP NA */

                    /* fill the Ethernet header */
                    if (dev_hard_header(skb, dev, ETHERTYPE_IPV6, lladdr,
                                        ni->ni_macaddr, skb->len) < 0)
                    {
                        kfree_skb(skb);
                        goto drop_node;
                    }

                    /* build IPv6 header */
                    *(__be32 *)nip6 = htonl(0x60000000);
                    nip6->payload_len = htons(sizeof(struct nd_msg) + 8);
                    nip6->nexthdr = IPPROTO_ICMPV6;
                    nip6->hop_limit = 0xff;
		    if (is_dad_transmit) {
			    /* IPv6 dest addr is all nodes multicast addr for DAD NA */
			    ipv6_addr_set(&nip6->daddr, htonl(0xFF020000), 0, 0,
					    htonl(0x00000001));
		    } else {
			    nip6->daddr = ip6->saddr;
		    }
		    nip6->saddr = msg->target;

                    /* build ICMPv6 NDP NA packet */
                    memset(&nmsg->icmph, 0, sizeof(struct icmp6hdr));
                    nmsg->icmph.icmp6_type = NDISC_NEIGHBOUR_ADVERTISEMENT;
		    if (is_dad_transmit) {
                        nmsg->icmph.icmp6_override = 1;
                    } else {
                        nmsg->icmph.icmp6_solicited = 1;
                    }

                    nmsg->target = msg->target;
                    /* ICMPv6 Option */
                    nmsg->opt[0] = ND_OPT_TARGET_LL_ADDR;
                    nmsg->opt[1] = 1;
                    memcpy(&nmsg->opt[2], ni->ni_macaddr, IEEE80211_ADDR_LEN);

                    nmsg->icmph.icmp6_cksum = csum_ipv6_magic(&nip6->saddr,
                                &nip6->daddr, sizeof(*nmsg) + 8, IPPROTO_ICMPV6,
                                csum_partial(&nmsg->icmph, sizeof(*nmsg) + 8, 0));

                    if (is_dad_transmit) {
                        /*
                         * DAD reply is required to send multicast NA to
                         * both STA's and the bridge. As we don't know
                         * where it is originated at this point.
                         */
                        struct sk_buff *skb1 = qdf_nbuf_clone(skb);

			nbuf_debug_del_record(skb);
                        dev_queue_xmit(skb);

                        if (!skb1) {
                            IEEE80211_DPRINTF(vap, IEEE80211_MSG_PROXYARP,
                                "%s: skb_clone failed\n");
                            goto drop_node;
                        }
                        __osif_deliver_data(vap->iv_ifp, skb1);
                        IEEE80211_DPRINTF(vap, IEEE80211_MSG_PROXYARP, "send DAD NDP "
                                          "NA for ip %pI6 -> mac %pM to %pM both "
                                          "over the air and via local stack\n",
                                          &msg->target, ni->ni_macaddr, lladdr);
                    } else {
                        /* Send unicast NA for normal SC */
                        ni1 = ieee80211_find_node(nt, lladdr);
                        if (IS_MYSTA(vap, ni1)) {
                            ieee80211_free_node(ni1);
                            /* Send it to the STA */
#ifdef HOST_OFFLOAD
                            atd_proxy_arp_send(skb);
#else
			    nbuf_debug_del_record(skb);
                            dev_queue_xmit(skb);
#endif
                        } else {
                            if (ni1) {
                                ieee80211_free_node(ni1);
                                ni1 = NULL;
                            }
                            /* Deliver it to the bridge */
                            __osif_deliver_data(vap->iv_ifp, skb);
                        }

                        IEEE80211_DPRINTF(vap, IEEE80211_MSG_PROXYARP, "send NDP "
                                          "NA for ip %pI6 -> mac %pM to %pM %s\n",
                                          &msg->target, ni->ni_macaddr,
                                          &eh->ether_shost,
                                          ni1 ? "over the air" : "via local stack");
                    }

                    /* Suppress NDP NS within the BSS */
                    goto drop_node;
                }
                /*
                 * We drop all the ND SC packets here since we can detect the
                 * STA's IPv6 address w/ DAD reliably even for DHCPv6.
                 */
                goto drop_node;

            case NDISC_NEIGHBOUR_ADVERTISEMENT:
                if (msg->icmph.icmp6_solicited && ipv6_addr_is_multicast(&msg->target))
                    printf("ERROR: both solicited and multicast is set!\n");

                unsolicited = !msg->icmph.icmp6_solicited ||
                              ipv6_addr_is_multicast(&msg->target);

                /* Ignore NA packets w/o option */
                lladdr = ipv6_ndisc_opt_lladdr(msg->opt, optlen, 0);
                if (!lladdr) {
                    if (unsolicited)
                        goto drop;
                    else
                        goto pass;
                }

                ni = ieee80211_find_node_by_ipv6(nt, (u8 *)&msg->target);
                if (ni && memcmp(ni->ni_macaddr, lladdr, IEEE80211_ADDR_LEN)) {
                    /*
                     * Received a NA with a different link addr. It could
                     * be a NA indicating duplication of the tentative addr
                     * from a previous SC.
                     */
                    IEEE80211_DPRINTF(vap, IEEE80211_MSG_PROXYARP, "NDP NA: "
                                      "ipv6 %pI6 8< mac %pM removed (DAD w/ %pM)\n",
                                      &msg->target, ni->ni_macaddr, lladdr);
                    ieee80211_node_remove_ipv6(nt, (u8 *)&msg->target);
                    goto pass_node;
                }
                if (ni)
                    ieee80211_free_node(ni);

                /* Suppress non-solicited and non-override NA within the BSS */
                if (unsolicited && !msg->icmph.icmp6_override)
                    goto drop;
#if 0
                ni = ieee80211_find_node(nt, lladdr);
                if (!IS_MYNODE(vap, ni)) {
                    /*
                     * In this case we are not interested in this NA packet.
                     * Forward it within the BSS if it is solicited or
                     * suppress it if it is non-solicited.
                     */
                    if (unsolicited)
                        goto drop_node;

                    goto pass_node;
                }

                /* Learn from this NA if it is from our associated STA's */
                ieee80211_node_add_ipv6(nt, ni, (u8 *)&msg->target);

                IEEE80211_DPRINTF(vap, IEEE80211_MSG_PROXYARP,
                        "NDP NA: mac %s -> ipv6 %pI6, multicast %d, solicited %d\n",
                        ether_sprintf(lladdr), &msg->target,
                        ipv6_addr_is_multicast(&msg->target),
                        msg->icmph.icmp6_solicited);
                 if (ni)
                     ieee80211_free_node(ni);

                /* Suppress non-solicited NA within the BSS */
                if (unsolicited)
                    goto drop;
#endif
                break;

            default:
                break;
            }
        }
    }
#endif /*end of (defined(CONFIG_IPV6) && CONFIG_IPV6 !=0) || (defined(CONFIG_IPV6_MODULE) && CONFIG_IPV6_MODULE !=0) */

pass:
    /* pass by default */
    return 0;
pass_node:
    if (ni)
        ieee80211_free_node(ni);
    return 0;
drop_node:
    if (ni)
        ieee80211_free_node(ni);
drop:
    return 1;
}
#endif
