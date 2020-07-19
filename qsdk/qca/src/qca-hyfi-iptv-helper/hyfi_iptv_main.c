/*
 *  QCA HyFi Netfilter for IPTV implementations on Wireless interface
 *
 * Copyright (c) 2013 Qualcomm Atheros, Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
Manual build:
export CROSS_COMPILE=mips-linux-uclibc-
make KERNELPATH=`pwd`/../../linux/kernels/mips-linux-2.6.31/ CROSS_COMPILE=mips-linux-uclibc- MODULEPATH=`pwd`/../../rootfs-reh132_s27.build/lib/modules/2.6.31/net/
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/netfilter.h>
#include <linux/etherdevice.h>
#include <linux/netdevice.h>
#include <net/ip.h>
#include <net/udp.h>
#include <br_private.h>
#include <linux/udp.h>
#include <linux/if_arp.h>
#include <linux/version.h>

/* Macro definitions */
#define LKM_AUTHOR          "Hai Shalom"
#define LKM_DESCRIPTION     "QCA Hy-Fi IPTV Helper"

/* Default Linux bridge */
static char *hyfi_iptv_wireless_interface = "ath0";

/* This parameter can be set from the insmod command line */
MODULE_PARM_DESC(hyfi_iptv_wireless_interface, "Default wireless interface");
module_param(hyfi_iptv_wireless_interface, charp, 0000);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
unsigned int hyfi_iptv_netfilter_pre_routing_hook(void *priv,
                                                  struct sk_buff *skb,
                                                  const struct nf_hook_state *state);

unsigned int hyfi_iptv_netfilter_local_out_hook(void *priv,
                                                struct sk_buff *skb,
                                                const struct nf_hook_state *state);

unsigned int hyfi_iptv_netfilter_forward_hook(void *priv,
                                              struct sk_buff *skb,
                                              const struct nf_hook_state *state);
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0))
unsigned int hyfi_iptv_netfilter_pre_routing_hook(const struct nf_hook_ops *ops, struct sk_buff *skb,
        const struct net_device *in, const struct net_device *out,
        int (*okfn)(struct sk_buff *) );

unsigned int hyfi_iptv_netfilter_local_out_hook(const struct nf_hook_ops *ops, struct sk_buff *skb,
        const struct net_device *in, const struct net_device *out,
        int (*okfn)(struct sk_buff *) );

unsigned int hyfi_iptv_netfilter_forward_hook(const struct nf_hook_ops *ops, struct sk_buff *skb,
        const struct net_device *in, const struct net_device *out,
        int (*okfn)(struct sk_buff *) );
#else
unsigned int hyfi_iptv_netfilter_pre_routing_hook( unsigned int hooknum, struct sk_buff *skb,
        const struct net_device *in, const struct net_device *out,
        int (*okfn)(struct sk_buff *) );

unsigned int hyfi_iptv_netfilter_local_out_hook( unsigned int hooknum, struct sk_buff *skb,
        const struct net_device *in, const struct net_device *out,
        int (*okfn)(struct sk_buff *) );

unsigned int hyfi_iptv_netfilter_forward_hook( unsigned int hooknum, struct sk_buff *skb,
        const struct net_device *in, const struct net_device *out,
        int (*okfn)(struct sk_buff *) );
#endif

static struct nf_hook_ops hyfi_iptv_hook_ops[] __read_mostly =
{
    {
        .pf = NFPROTO_BRIDGE,
        .priority = 1,
        .hooknum = NF_INET_PRE_ROUTING,
        .hook = hyfi_iptv_netfilter_pre_routing_hook,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0))
        .owner = THIS_MODULE,
#endif
    },
    {
        .pf = NFPROTO_BRIDGE,
        .priority = 1,
        .hooknum = NF_INET_LOCAL_OUT,
        .hook = hyfi_iptv_netfilter_local_out_hook,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0))
        .owner = THIS_MODULE,
#endif
    },
    {
        .pf = NFPROTO_BRIDGE,
        .priority = 1,
        .hooknum = NF_INET_FORWARD,
        .hook = hyfi_iptv_netfilter_forward_hook,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0))
        .owner = THIS_MODULE,
#endif
    }

};

struct hyfi_iptv_entry
{
    u_int8_t mac_addr[6];       /* Real MAC address */
    u_int8_t alt_mac_addr[6];   /* Alternative MAC address */
    u_int32_t ip_addr;          /* IP Address attached to the alternative MAC address */
    u_int32_t egress;           /* Keep real MAC address on egree? */

    unsigned long timestamp;
};

#define LIST_SIZE 8

struct hyfi_iptv_module
{
    struct net_device *hyfi_iptv_dev;
    struct hyfi_iptv_entry list[LIST_SIZE];
    u_int32_t num_entries;

    spinlock_t lock;
};

static struct hyfi_iptv_module hyfi_iptv_module __read_mostly =
{
    .hyfi_iptv_dev = NULL,
    .num_entries = 0,
};

#define DHCP_OPTIONS_LEN    312

struct bootphdr
{
    u8 type;            /* DHCP message type 1=request, 2=reply */
    u8 hw_type;         /* Hardware address type */
    u8 hwaddr_len;      /* Hardware address length */
    u8 hops;            /* Number of hops */
    __be32 trans_id;    /* Transaction ID */
    __be16 secs;        /* Seconds elapsed */
    __be16 flags;       /* Flags */
    __be32 client_ip;   /* Client's IP address */
    __be32 your_ip;     /* Assigned (your) IP address */
    __be32 server_ip;   /* Next server's IP address */
    __be32 relay_ip;    /* Relay agent IP address */
    u8 hw_addr[16];     /* Client's Hardware address w/padding */
    u8 server_name[64]; /* Server host name */
    u8 boot_file[128];  /* Name of boot file */
    u8 options[DHCP_OPTIONS_LEN];    /* DHCP options */
};

struct arpopts
{
    unsigned char       ar_sha[ETH_ALEN];   /* sender hardware address  */
    unsigned char       ar_sip[4];      /* sender IP address        */
    unsigned char       ar_tha[ETH_ALEN];   /* target hardware address  */
    unsigned char       ar_tip[4];      /* target IP address        */

};

static const char MAGIC_COOKIE[] = { 0x63, 0x82, 0x53, 0x63 };  /* Required by RFC */

/* DHCP packet types */
#define BOOTP_REQUEST   1
#define BOOTP_REPLY     2

/* DHCP message types we care about */
#define DHCPDISCOVER    1
#define DHCPOFFER       2
#define DHCPREQUEST     3

#undef HYFI_IPTV_MSFT_DONT_UPDATE_ON_EGRESS

#undef HYFI_IPTV_DEBUG
#ifdef HYFI_IPTV_DEBUG
#define HYFI_IPTV_IP4_FMT(ip4)     (ip4)[0], (ip4)[1], (ip4)[2], (ip4)[3]
#define HYFI_IPTV_IP4_STR          "%d.%d.%d.%d"
#define HYFI_IPTV_MAC_FMT(addr)    (addr)[0], (addr)[1], (addr)[2], (addr)[3], (addr)[4], (addr)[5]
#define HYFI_IPTV_MAC_STR          "%02x:%02x:%02x:%02x:%02x:%02x"
#define DPRINTK(fmt, ...)  do {printk(fmt, ##__VA_ARGS__);} while(0)
#else
#define DPRINTK(fmt, ...)
#endif

#define HYFI_ENTRY_MAX_AGE  msecs_to_jiffies( 5 * 60 * 1000 ) /* 5 minutes */

static struct hyfi_iptv_entry *hyfi_iptv_find_entry_ingress( u_int8_t *mac_addr );
static unsigned int hyfi_iptv_netfilter_hook( struct sk_buff *skb, const struct net_device *dev, u_int32_t local, u_int32_t forward );

static inline int hyfi_iptv_entry_aged_out( u_int32_t idx )
{
    return time_after( jiffies, hyfi_iptv_module.list[ idx ].timestamp + HYFI_ENTRY_MAX_AGE );
}

static void hyfi_iptv_cleanup( void )
{
    u_int32_t i;

    spin_lock( &hyfi_iptv_module.lock );

    /* Go over the list and remove entries we didn't hear from or to for more than 5 minutes */
    for( i = 0; i < hyfi_iptv_module.num_entries; i++ )
    {
        if( hyfi_iptv_entry_aged_out( i ) )
        {
            DPRINTK( "%s: Aged out entry, index = %d, original MAC: "HYFI_IPTV_MAC_STR", new MAC: "HYFI_IPTV_MAC_STR", IP: "HYFI_IPTV_IP4_STR"\n",
                    __func__, i, HYFI_IPTV_MAC_FMT(hyfi_iptv_module.list[ i ].mac_addr), HYFI_IPTV_MAC_FMT(hyfi_iptv_module.list[ i ].alt_mac_addr), HYFI_IPTV_IP4_FMT((u8 *)(&hyfi_iptv_module.list[i].ip_addr)));

            hyfi_iptv_module.num_entries--;

            if( hyfi_iptv_module.num_entries != i )
            {
                memmove( &hyfi_iptv_module.list[ i ], &hyfi_iptv_module.list[ i + 1 ],
                    sizeof( hyfi_iptv_module.list[ 0 ] ) * ( hyfi_iptv_module.num_entries - i ));
            }
        }
    }

    spin_unlock( &hyfi_iptv_module.lock );
}

/* Cleanup an entry, assumes caller locks database */
static void hyfi_iptv_entry_cleanup( u_int32_t idx )
{
    if( idx < hyfi_iptv_module.num_entries && hyfi_iptv_module.num_entries )
    {
        hyfi_iptv_module.num_entries--;

        if( hyfi_iptv_module.num_entries != idx )
        {
            memmove( &hyfi_iptv_module.list[ idx ], &hyfi_iptv_module.list[ idx + 1 ],
                sizeof( hyfi_iptv_module.list[ 0 ] ) * ( hyfi_iptv_module.num_entries - idx ));
        }
    }
}

static int hyfi_iptv_add_entry( u_int8_t *mac_addr, u_int8_t *alt_mac_addr, u_int32_t ip_addr, u_int32_t egress )
{
    struct hyfi_iptv_entry *entry;

    /* Clean up first */
    hyfi_iptv_cleanup();

    /* Find an existing entry */
    entry = hyfi_iptv_find_entry_ingress( alt_mac_addr );

    if( !entry )
    {
        if( hyfi_iptv_module.num_entries >= LIST_SIZE )
            return -1;

        spin_lock( &hyfi_iptv_module.lock );

        /* New */
        entry = &hyfi_iptv_module.list[ hyfi_iptv_module.num_entries ];
        hyfi_iptv_module.num_entries++;
    }
    else
    {
        spin_lock( &hyfi_iptv_module.lock );
    }

    /* Create/update entry */
    entry->ip_addr = htonl( ip_addr );
    memcpy( entry->mac_addr, mac_addr, ETH_ALEN );
    memcpy( entry->alt_mac_addr, alt_mac_addr, ETH_ALEN );
    entry->egress = egress;
    entry->timestamp = jiffies;

    spin_unlock( &hyfi_iptv_module.lock );

    DPRINTK( "%s: Added entry, original MAC: "HYFI_IPTV_MAC_STR", new MAC: "HYFI_IPTV_MAC_STR", IP: "HYFI_IPTV_IP4_STR"\n",
            __func__, HYFI_IPTV_MAC_FMT(entry->mac_addr), HYFI_IPTV_MAC_FMT(entry->alt_mac_addr), HYFI_IPTV_IP4_FMT((u8 *)(&ip_addr)));
    return 0;
}

/* From the Wireless Receiver to the world */
static struct hyfi_iptv_entry *hyfi_iptv_find_entry_egress( u_int32_t ip_addr )
{
    struct hyfi_iptv_entry *entry = NULL;
    u_int32_t i;

    spin_lock( &hyfi_iptv_module.lock );

    for( i = 0; i < hyfi_iptv_module.num_entries; i++ )
    {
        if( hyfi_iptv_module.list[i].ip_addr == htonl(ip_addr) )
        {
            entry = &hyfi_iptv_module.list[i];
            hyfi_iptv_module.list[ i ].timestamp = jiffies;

            DPRINTK( "%s: Found entry, index = %d, original MAC: "HYFI_IPTV_MAC_STR", new MAC: "HYFI_IPTV_MAC_STR", IP: "HYFI_IPTV_IP4_STR"\n",
                    __func__, i, HYFI_IPTV_MAC_FMT(entry->mac_addr), HYFI_IPTV_MAC_FMT(entry->alt_mac_addr), HYFI_IPTV_IP4_FMT((u8 *)(&hyfi_iptv_module.list[i].ip_addr)));
            break;
        }
    }

    spin_unlock( &hyfi_iptv_module.lock );
    return entry;
}

/* From the world to the Wireless receiver */
static struct hyfi_iptv_entry *hyfi_iptv_find_entry_ingress( u_int8_t *mac_addr )
{
    struct hyfi_iptv_entry *entry = NULL;
    u_int32_t i;

    spin_lock( &hyfi_iptv_module.lock );

    for( i = 0; i < hyfi_iptv_module.num_entries; i++ )
    {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0))
        if( ether_addr_equal( hyfi_iptv_module.list[i].alt_mac_addr, mac_addr ) )
#else
        if( !compare_ether_addr( hyfi_iptv_module.list[i].alt_mac_addr, mac_addr ) )
#endif
        {
            if( unlikely( hyfi_iptv_entry_aged_out( i ) ) )
            {
                /* Entry aged out, remove it from the database */
                hyfi_iptv_entry_cleanup( i );
                return NULL;
            }
            entry = &hyfi_iptv_module.list[i];

            DPRINTK( "%s: Found entry, index = %d, incoming MAC: "HYFI_IPTV_MAC_STR", converted MAC: "HYFI_IPTV_MAC_STR", IP: "HYFI_IPTV_IP4_STR"\n",
                    __func__, i, HYFI_IPTV_MAC_FMT(mac_addr), HYFI_IPTV_MAC_FMT(entry->alt_mac_addr), HYFI_IPTV_IP4_FMT((u8 *)(&hyfi_iptv_module.list[i].ip_addr)));
            break;
        }
    }

    spin_unlock( &hyfi_iptv_module.lock );
    return entry;
}

int hyfi_iptv_handle_dhcp( u_int8_t* sa, struct sk_buff *skb )
{
    u_int8_t *options_end;
    u_int8_t *option_ptr;
    u_int32_t requested_ip = 0;
    u_int8_t *client_id = NULL;
    u_int8_t msg_type = 0;
    char class_id[32];
    u_int8_t *option;
    struct iphdr *iph;
    struct udphdr *udph;
    struct bootphdr *bootph;
    int32_t len;

    iph = ip_hdr(skb);
    bootph = (struct bootphdr *) (skb_network_header(skb) + ip_hdrlen(skb) + sizeof(struct udphdr));

    len = ntohs(iph->tot_len) - (ip_hdrlen(skb) + sizeof(struct udphdr) );
    options_end = (u_int8_t *)bootph + len;

    /* We only care about requests */
    if( bootph->type != BOOTP_REQUEST )
        return 0;

    DPRINTK( "%s: DHCP request packet, SA: "HYFI_IPTV_MAC_STR"\n",
            __func__, HYFI_IPTV_MAC_FMT(sa) );

    /* Check cookie */
    if( memcmp( bootph->options, MAGIC_COOKIE, 4 ) )
        return 0;

    memset( class_id, 0, sizeof(class_id) );
    option_ptr = &bootph->options[4];

    while( option_ptr < options_end && *option_ptr != 0xFF )
    {
        option = option_ptr++;
        if( *option == 0 )
            continue; /* 0's are used as padding */

        option_ptr += *option_ptr + 1;
        if( option_ptr >= options_end )
            break;

        switch( *option )
        {
            case 50: /* Requested IP address */
                if( option[1] == sizeof(u_int32_t) )
                {
                    memcpy( &requested_ip, &option[2], sizeof(u_int32_t) );
                    DPRINTK( "%s: DHCP Option 50, requested IP: "HYFI_IPTV_IP4_STR"\n",
                            __func__, HYFI_IPTV_IP4_FMT((u8 *)(&option[2])));
                }
                break;

            case 61: /* Client ID */
                if( (option[1] == ETH_ALEN + 1) && option[2] == 1 )
                {
                    client_id = &option[2] + 1;
                    DPRINTK( "%s: DHCP Option 61, Cliend ID: "HYFI_IPTV_MAC_STR"\n",
                            __func__, HYFI_IPTV_MAC_FMT(client_id) );
                }
                break;

            case 53: /* Message type: REQUEST/DISCOVER */
                if( option[1] )
                {
                    msg_type = option[2];
                    DPRINTK( "%s: DHCP Option 53, message type: %d\n",
                            __func__, msg_type );
                }
                break;

            case 60: /* Class ID: udpchc/MSFT */
                if( option[1] )
                {
                    memcpy( class_id, &option[2], option[1] > sizeof(class_id)-1 ? sizeof(class_id)-1 : option[1] );

                    DPRINTK( "%s: DHCP Option 60, Class ID: %s\n",
                            __func__, class_id );
                }
                break;

            default:
                DPRINTK( "%s: Ignoring DHCP option %d\n", __func__, *option );
                break;
        }
    }

    if( msg_type == DHCPREQUEST || msg_type == DHCPDISCOVER )
    {
        u_int32_t update_sa = 0;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0))
        if( client_id &&
                !ether_addr_equal( client_id, bootph->hw_addr ) )
#else
        if( client_id &&
                compare_ether_addr( client_id, bootph->hw_addr ) )
#endif
        {
            u_int32_t egress;

            iph = ip_hdr(skb);
            udph = (struct udphdr *)(skb_network_header(skb) + ip_hdrlen(skb));

            /* Do the trick, update SA and Client HW address */
            memcpy( bootph->hw_addr, client_id, ETH_ALEN );

            /* Update the UDP checksum */
            udph->check = 0;
            skb->csum = csum_tcpudp_nofold(iph->saddr, iph->daddr,
                    htons(udph->len), IPPROTO_UDP, 0);
            udph->check = __skb_checksum_complete(skb);
            update_sa = 1;

#ifdef HYFI_IPTV_MSFT_DONT_UPDATE_ON_EGRESS
            if( class_id[0] &&
                    !strcmp( class_id, "MSFT_IPTV") )
            {
                egress = 0;
            }
            else
#endif
            {
                egress = 1;
            }

            if(!requested_ip && msg_type == DHCPREQUEST && bootph->client_ip)
            {
                /* MSFT sometimes sends requests without option 50, uses client IP field instead */
                requested_ip = bootph->client_ip;
            }

            /* Create/update an entry */
            hyfi_iptv_add_entry( sa, client_id, requested_ip, egress );

            if( update_sa )
            {
                memcpy( sa, client_id, ETH_ALEN );
                return 1;
            }
        }
    }

    return 0;
}

#define TRACE() printk("%s(%d)\n", __func__, __LINE__ )

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
unsigned int hyfi_iptv_netfilter_local_out_hook(void *priv,
                                                struct sk_buff *skb,
                                                const struct nf_hook_state *state)
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0))
unsigned int hyfi_iptv_netfilter_local_out_hook(const struct nf_hook_ops *ops, struct sk_buff *skb,
        const struct net_device *in, const struct net_device *out,
        int(*okfn)( struct sk_buff * ) )
#else
unsigned int hyfi_iptv_netfilter_local_out_hook( unsigned int hooknum, struct sk_buff *skb,
        const struct net_device *in, const struct net_device *out,
        int(*okfn)( struct sk_buff * ) )
#endif
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
    const struct net_device *out = state->out;
#endif
    return hyfi_iptv_netfilter_hook( skb, out, 1, 0 );
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
unsigned int hyfi_iptv_netfilter_pre_routing_hook(void *priv,
                                                  struct sk_buff *skb,
                                                  const struct nf_hook_state *state)
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0))
unsigned int hyfi_iptv_netfilter_pre_routing_hook(const struct nf_hook_ops *ops, struct sk_buff *skb,
        const struct net_device *in, const struct net_device *out,
        int(*okfn)( struct sk_buff * ) )
#else
unsigned int hyfi_iptv_netfilter_pre_routing_hook( unsigned int hooknum, struct sk_buff *skb,
        const struct net_device *in, const struct net_device *out,
        int(*okfn)( struct sk_buff * ) )
#endif
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
    const struct net_device *in = state->in;
#endif
    return hyfi_iptv_netfilter_hook( skb, in, 0, 0 );
}


#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
unsigned int hyfi_iptv_netfilter_forward_hook(void *priv,
                                              struct sk_buff *skb,
                                              const struct nf_hook_state *state)
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0))
unsigned int hyfi_iptv_netfilter_forward_hook(const struct nf_hook_ops *ops, struct sk_buff *skb,
        const struct net_device *in, const struct net_device *out,
        int(*okfn)( struct sk_buff * ) )
#else
unsigned int hyfi_iptv_netfilter_forward_hook( unsigned int hooknum, struct sk_buff *skb,
        const struct net_device *in, const struct net_device *out,
        int(*okfn)( struct sk_buff * ) )
#endif
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
    const struct net_device *in = state->in;
#endif
    return hyfi_iptv_netfilter_hook( skb, in, 0, 1 );
}

static inline struct net_bridge_port *hyfi_br_port_get(const struct net_device *dev)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 3, 0))
    struct net_bridge_port *br_port;
    rcu_read_lock();
    br_port = br_port_get_rcu(dev);
    rcu_read_unlock();
    return br_port;
#else
    return dev->br_port;
#endif
}

static unsigned int hyfi_iptv_netfilter_hook( struct sk_buff *skb, const struct net_device *dev, u_int32_t local, u_int32_t forward )
{
    struct iphdr *iph;
    struct ethhdr *eh;
    struct hyfi_iptv_entry *entry = NULL;
    struct net_bridge_port *br_port;

    if( !dev )
    	return NF_ACCEPT;

    br_port = hyfi_br_port_get(dev);

    if( !skb || !hyfi_iptv_module.hyfi_iptv_dev || !br_port )
        return NF_ACCEPT;

    eh = eth_hdr(skb);

    if( forward && dev != hyfi_iptv_module.hyfi_iptv_dev)
    {
        /* Forward packets which came from the world, already taken care of */
        return NF_ACCEPT;
    }

    /* Check for Multicast */
    if( is_multicast_ether_addr(eh->h_dest) &&
            !is_broadcast_ether_addr( eh->h_dest ) )
    {
        if( dev != hyfi_iptv_module.hyfi_iptv_dev )
        {
            /* Multicast, return immediately */
            return NF_ACCEPT;
        }
        else
        {
            if( !forward )
            {
                /* Let multicast packets from the wireless receiver be
                 * processed with the REAL SA MAC address - required for multicast module.
                 * If forward is 1, then its in it's post processing. Change the SA MAC
                 * address to the alternate.
                 */
                return NF_ACCEPT;
            }
        }
    }

    if( eh->h_proto == htons(ETH_P_IP))
    {
        struct udphdr *udph = NULL;

        if (unlikely(!pskb_may_pull(skb, sizeof(struct iphdr) )))
            return NF_ACCEPT;

        iph = ip_hdr(skb);

        if(!iph)
            return NF_DROP;

        /* Ignore non-IPv4, non-UDP */
        if (iph->ihl < 5 || iph->version != 4 )
            return NF_ACCEPT;

        /* Ignore fragments */
        if( iph->frag_off & htons(IP_MF | IP_OFFSET))
            return NF_ACCEPT;

        /* Sanity checks */
        if (unlikely(!pskb_may_pull(skb, iph->ihl*4)))
            return NF_DROP;

        if( skb->len < htons(iph->tot_len) )
            return NF_DROP;

        if( unlikely(ip_fast_csum((u8 *)iph, iph->ihl)))
            return NF_DROP;

        /* Did the packet arrive from the wireless interface? */
        if( !local && dev == hyfi_iptv_module.hyfi_iptv_dev )
        {
            /* Check if this packet is a DHCP packet */
            if( iph->protocol == IPPROTO_UDP )
            {
                if (unlikely(!pskb_may_pull(skb, sizeof(struct udphdr) )))
                    return NF_DROP;

                eh = eth_hdr(skb);
                iph = ip_hdr(skb);
                udph = (struct udphdr *)(skb_network_header(skb) + ip_hdrlen(skb));

                if( udph->source == htons(68) &&
                      udph->dest == htons(67) )
                {
                    if( !pskb_may_pull(skb, skb->len ) )
                        return NF_DROP;

                    /* DHCP packet */
                    if( hyfi_iptv_handle_dhcp( eh->h_source, skb ) )
                    {
                        return NF_ACCEPT;
                    }
                }
                else if( udph->source == htons(49300) && udph->dest == htons(49300) )
                {
                    /* FDB training packet, change DA to host and accept */
                    skb->pkt_type = PACKET_HOST;
                    memcpy( eh->h_dest, br_port->br->dev->dev_addr, ETH_ALEN );

                    return NF_ACCEPT;
                }
            }

            if( ( entry = hyfi_iptv_find_entry_egress( iph->saddr ) ) )
            {
                if( entry->egress )
                {
                    /* Replace the Wirelsss SA with the alternative SA */
                    memcpy( eh->h_source, entry->alt_mac_addr, ETH_ALEN );
                }

#ifdef HYFI_IPTV_DEBUG
                if( printk_ratelimit() )
                {
                    DPRINTK( "%s: IP Packet from interface %s, updating packet: original MAC: "HYFI_IPTV_MAC_STR", updated MAC: "HYFI_IPTV_MAC_STR", IP: "HYFI_IPTV_IP4_STR"\n",
                            __func__, dev->name, HYFI_IPTV_MAC_FMT(entry->mac_addr), HYFI_IPTV_MAC_FMT(entry->alt_mac_addr), HYFI_IPTV_IP4_FMT((u8 *)(&iph->saddr)));
                }
#endif
                return NF_ACCEPT;
            }
        }
        else
        {
            if( is_broadcast_ether_addr( eh->h_dest ) )
            {
                return NF_ACCEPT;
            }

            if( ( entry = hyfi_iptv_find_entry_ingress( eh->h_dest ) ) )
            {
                /* Restore the Wireless DA */
                memcpy( eh->h_dest, entry->mac_addr, ETH_ALEN );

#ifdef HYFI_IPTV_DEBUG
                if( printk_ratelimit() )
                {
                    DPRINTK( "%s: IP Packet from interface %s, updating packet: original MAC: "HYFI_IPTV_MAC_STR", updated MAC: "HYFI_IPTV_MAC_STR", IP: "HYFI_IPTV_IP4_STR"\n",
                            __func__, dev->name, HYFI_IPTV_MAC_FMT(entry->alt_mac_addr), HYFI_IPTV_MAC_FMT(entry->mac_addr), HYFI_IPTV_IP4_FMT((u8 *)(&iph->saddr)));
                }
#endif
                return NF_ACCEPT;
            }
        }
    }
    else if( eh->h_proto == htons( ETH_P_ARP ))
    {
        /* We must look at ARP packets because they are L2 packets that contain
         * L3 (IP) information. We would have to modify the packet accordingly.
         */
        struct arphdr *arph;

        if (!pskb_may_pull(skb, arp_hdr_len((struct net_device *)dev)))
            return NF_DROP;

        arph = arp_hdr(skb);
        eh = eth_hdr(skb);

        /* Look on ARPs that we are interested in */
        if( htons(arph->ar_hrd) == ARPHRD_ETHER &&
                htons(arph->ar_pro) == ETH_P_IP &&
                arph->ar_hln == ETH_ALEN &&
                arph->ar_pln == 4 )
        {
            struct hyfi_iptv_entry *entry = NULL;
            struct arpopts *arpopts = (struct arpopts *) (skb_network_header(skb) + sizeof(struct arphdr) );

            /* Did the packet arrive from the wireless interface? */
            if( !local && dev == hyfi_iptv_module.hyfi_iptv_dev )
            {
                u_int32_t ip_addr;

                if( arph->ar_op == htons(ARPOP_REQUEST) ||
                        arph->ar_op == htons(ARPOP_REPLY) )
                {
                    memcpy( &ip_addr, arpopts->ar_sip, sizeof(u_int32_t) );

                    entry = hyfi_iptv_find_entry_egress( ip_addr );

                    if( entry )
                    {
                        /* MSFT adds additional garbage bytes */
                        if( skb->len > 28 )
                            skb->len = 28;

                        /* Replace the Wirelsss SA with the alternative SA */
                        memcpy( eh->h_source, entry->alt_mac_addr, ETH_ALEN );
                        memcpy( arpopts->ar_sha, entry->alt_mac_addr, ETH_ALEN );

                        DPRINTK( "%s: ARP %s from wireless interface %s, updating packet: original MAC: "HYFI_IPTV_MAC_STR", updated MAC: "HYFI_IPTV_MAC_STR", IP: "HYFI_IPTV_IP4_STR"\n",
                                __func__, arph->ar_op == htons(ARPOP_REQUEST) ? "REQUEST" : "REPLY",
                                dev->name, HYFI_IPTV_MAC_FMT(entry->mac_addr), HYFI_IPTV_MAC_FMT(entry->alt_mac_addr), HYFI_IPTV_IP4_FMT(arpopts->ar_sip));

                        DPRINTK( "%s: DA MAC: "HYFI_IPTV_MAC_STR", ARP DA MAC: "HYFI_IPTV_MAC_STR", DIP: "HYFI_IPTV_IP4_STR", pkt_type=%d\n",
                                __func__,
                                HYFI_IPTV_MAC_FMT(eh->h_dest), HYFI_IPTV_MAC_FMT(arpopts->ar_tha), HYFI_IPTV_IP4_FMT(arpopts->ar_tip), skb->pkt_type);
                    }
                }

                return NF_ACCEPT;
            }
            else
            {
                if( arph->ar_op == htons(ARPOP_REPLY) )
                {
                    if( ( entry = hyfi_iptv_find_entry_ingress( eh->h_dest ) ) )
                    {
                        /* Replace the DA and ARP Target HW address */
                        memcpy( eh->h_dest, entry->mac_addr, ETH_ALEN );
                        memcpy( arpopts->ar_tha, entry->mac_addr, ETH_ALEN );

                        DPRINTK( "%s: ARP REPLY from interface %s, updating packet: original MAC: "HYFI_IPTV_MAC_STR", updated MAC: "HYFI_IPTV_MAC_STR", IP: "HYFI_IPTV_IP4_STR"\n",
                                __func__, dev->name, HYFI_IPTV_MAC_FMT(entry->alt_mac_addr), HYFI_IPTV_MAC_FMT(entry->mac_addr), HYFI_IPTV_IP4_FMT(arpopts->ar_tip));
                    }
                }
                else if( arph->ar_op == htons(ARPOP_REQUEST) )
                {
                    if( !is_broadcast_ether_addr( eh->h_dest ) )
                    {
                        entry = hyfi_iptv_find_entry_ingress( eh->h_dest );
                    }

                    if( entry )
                    {
                        /* Replace the DA - well, looks like the wireless receiver does not respond to unicast ARPs, so convert to broadcast */
                        //memcpy( eh->h_dest, entry->mac_addr, ETH_ALEN );
                        memset( eh->h_dest, 0xff, ETH_ALEN );

                        DPRINTK( "%s: ARP REQUEST from interface %s, updating packet: original MAC: "HYFI_IPTV_MAC_STR", updated MAC: "HYFI_IPTV_MAC_STR", IP: "HYFI_IPTV_IP4_STR"\n",
                                __func__, dev->name, HYFI_IPTV_MAC_FMT(entry->alt_mac_addr), HYFI_IPTV_MAC_FMT(eh->h_dest), HYFI_IPTV_IP4_FMT(arpopts->ar_tip));

                        DPRINTK( "%s: SA MAC: "HYFI_IPTV_MAC_STR", ARP SA MAC: "HYFI_IPTV_MAC_STR", SIP: "HYFI_IPTV_IP4_STR", pkt_type=%d\n",
                                __func__,
                                HYFI_IPTV_MAC_FMT(eh->h_source), HYFI_IPTV_MAC_FMT(arpopts->ar_sha), HYFI_IPTV_IP4_FMT(arpopts->ar_sip), skb->pkt_type);

                    }
                }
            }
        }

        return NF_ACCEPT;
    }
    else
    {
        eh = eth_hdr(skb);

        /* Some L2 traffic */
        if( is_broadcast_ether_addr( eh->h_dest ) )
        {
            return NF_ACCEPT;
        }

        /* Examine only local or outside traffic going to the Wireless receiver.
         * If the Wireless reciver sends L2 traffic, we have no way to determine the SA
         * because it requires IP layer. In this case, leave SA as is.
         */
        if( local || dev != hyfi_iptv_module.hyfi_iptv_dev )
        {
            if( ( entry = hyfi_iptv_find_entry_ingress( eh->h_dest ) ) )
            {
                /* Restore the Wireless DA */
                memcpy( eh->h_dest, entry->mac_addr, ETH_ALEN );

#ifdef HYFI_IPTV_DEBUG
                if( printk_ratelimit() )
                {
                    DPRINTK( "%s: L2 Packet from interface %s, updating packet: original MAC: "HYFI_IPTV_MAC_STR", updated MAC: "HYFI_IPTV_MAC_STR"\n",
                            __func__, dev->name, HYFI_IPTV_MAC_FMT(entry->alt_mac_addr), HYFI_IPTV_MAC_FMT(entry->mac_addr));
                }
#endif
                return NF_ACCEPT;
            }
        }
    }

    /* None of our business */
    return NF_ACCEPT;
}

static int hyfi_iptv_dev_event(struct notifier_block *unused, unsigned long event, void *ptr);

struct notifier_block hyfi_device_notifier = {
    .notifier_call = hyfi_iptv_dev_event
};

static int hyfi_iptv_dev_event(struct notifier_block *unused, unsigned long event, void *ptr)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0))
    struct net_device *dev = netdev_notifier_info_to_dev(ptr);
#else
    struct net_device *dev = ptr;
#endif

    if( hyfi_iptv_module.hyfi_iptv_dev && dev != hyfi_iptv_module.hyfi_iptv_dev )
        return NOTIFY_DONE;

    switch (event)
    {
    case NETDEV_DOWN:
        if( !hyfi_iptv_module.hyfi_iptv_dev )
            break;

        DPRINTK( "Interface %s is down, ptr = %p\n", dev->name, dev );

        /* Free the hold of the device */
        dev_put( hyfi_iptv_module.hyfi_iptv_dev );
        hyfi_iptv_module.hyfi_iptv_dev = NULL;
        break;

    case NETDEV_UP:
        if( dev == hyfi_iptv_module.hyfi_iptv_dev )
            break;

        if( !strcmp( dev->name, hyfi_iptv_wireless_interface ) )
        {
            DPRINTK( "Interface %s is up, ptr = %p\n", dev->name, dev );

            /* Get the wireless device */
            hyfi_iptv_module.hyfi_iptv_dev = dev_get_by_name(&init_net, hyfi_iptv_wireless_interface);
        }
        break;

    default:
        break;
    }

    return NOTIFY_DONE;
}

static int __init hyfi_iptv_init(void)
{
	int ret;

	hyfi_iptv_module.hyfi_iptv_dev = NULL;

    /* Register netfilter hooks */
    if( ( ret = nf_register_hooks(hyfi_iptv_hook_ops, ARRAY_SIZE(hyfi_iptv_hook_ops))) )
	{
        printk( KERN_ERR "hyfi-iptv-helper: Failed to register to netfilter hooks\n" );
	    goto out;
	}

    if( ( ret = register_netdevice_notifier(&hyfi_device_notifier) ) )
    {
        printk( KERN_ERR "hyfi-iptv-helper: Failed to register to netdevice notifier\n" );
        goto out1;
    }

    goto out;

out1:
    nf_unregister_hooks( hyfi_iptv_hook_ops, ARRAY_SIZE(hyfi_iptv_hook_ops) );

out:
	printk( "QCA Hy-Fi IPTV helper netfilter installation: %s\n", ret ? "FAILED" : "OK" );
	return ret;
}

static void __exit hyfi_iptv_exit(void)
{
    nf_unregister_hooks( hyfi_iptv_hook_ops, ARRAY_SIZE(hyfi_iptv_hook_ops) );
    unregister_netdevice_notifier(&hyfi_device_notifier);

	if( hyfi_iptv_module.hyfi_iptv_dev )
	{
        DPRINTK( "calling dev_put, device: %s, ptr=%p\n", hyfi_iptv_module.hyfi_iptv_dev->name, hyfi_iptv_module.hyfi_iptv_dev);

        /* Free the hold of the device */
        dev_put( hyfi_iptv_module.hyfi_iptv_dev );
	}

	printk( "QCA Hy-Fi IPTV helper netfilter uninstalled\n" );
}

module_init(hyfi_iptv_init);
module_exit(hyfi_iptv_exit);

/*
 * Define the module’s license. Important!
 */

MODULE_LICENSE("Dual BSD/GPL");

/*
 * Optional definitions
 */

MODULE_AUTHOR(LKM_AUTHOR);  /* The author’s name */
MODULE_DESCRIPTION(LKM_DESCRIPTION);    /* The module description */

/*
 * API the module exports to other modules
 */
