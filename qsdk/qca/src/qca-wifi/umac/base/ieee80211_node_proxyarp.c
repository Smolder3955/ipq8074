#include "ieee80211_node_priv.h"
#include "ieee80211_var.h"

#if UMAC_SUPPORT_PROXY_ARP
struct ieee80211_node *
ieee80211_find_node_by_ipv4(struct ieee80211_node_table *nt, const uint32_t addr)
{
    struct ieee80211_node *ni;
    int hash;
    rwlock_state_t lock_state;

    if (addr == 0)
        return NULL;

    OS_RWLOCK_READ_LOCK(&nt->nt_ipv4_hash_lock, &lock_state);
    hash = IEEE80211_IPV4_HASH(addr);
    LIST_FOREACH(ni, &nt->nt_ipv4_hash[hash], ni_ipv4_hash) {
        if (ni->ni_ipv4_addr == addr) {
            ieee80211_try_ref_node(ni);	/* mark referenced */
            OS_RWLOCK_READ_UNLOCK(&nt->nt_ipv4_hash_lock, &lock_state);
            return ni;
        }
    }
    OS_RWLOCK_READ_UNLOCK(&nt->nt_ipv4_hash_lock, &lock_state);
    return NULL;
}

void
ieee80211_node_add_ipv4(struct ieee80211_node_table *nt,
                        struct ieee80211_node *ni,
                        const uint32_t ipaddr)
{
    int hash;
    rwlock_state_t lock_state;

    KASSERT(ni, ("node is NULL\n"));

    if (ni->ni_ipv4_addr == ipaddr)
        return;

    OS_RWLOCK_WRITE_LOCK(&nt->nt_ipv4_hash_lock, &lock_state);
    if (ni->ni_ipv4_addr) {
        /* remove the old ip from the hash table */
        LIST_REMOVE(ni, ni_ipv4_hash);
    }

    /* insert the new ip to the hash table */
    hash = IEEE80211_IPV4_HASH(ipaddr);
    LIST_INSERT_HEAD(&nt->nt_ipv4_hash[hash], ni, ni_ipv4_hash);
    ni->ni_ipv4_addr = ipaddr;
    OS_RWLOCK_WRITE_UNLOCK(&nt->nt_ipv4_hash_lock, &lock_state);
}

void
ieee80211_node_remove_ipv4(struct ieee80211_node_table *nt, struct ieee80211_node *ni)
{
    rwlock_state_t lock_state;

    KASSERT(ni, ("node is NULL\n"));

    OS_RWLOCK_WRITE_LOCK(&nt->nt_ipv4_hash_lock, &lock_state);
    if (ni->ni_ipv4_addr != 0) {
        LIST_REMOVE(ni, ni_ipv4_hash);
        ni->ni_ipv4_addr = 0;
    }
    OS_RWLOCK_WRITE_UNLOCK(&nt->nt_ipv4_hash_lock, &lock_state);
}

struct ieee80211_node *
ieee80211_find_node_by_ipv6(struct ieee80211_node_table *nt, u8 *ip6addr)
{
    struct ieee80211_ipv6_node *n6;
    int hash;
    rwlock_state_t lock_state;

    OS_RWLOCK_READ_LOCK(&nt->nt_ipv6_hash_lock, &lock_state);
    hash = IEEE80211_IPV6_HASH(ip6addr);
    LIST_FOREACH(n6, &nt->nt_ipv6_hash[hash], ni_hash) {
        if (!memcmp(&n6->node->ni_ipv6_addr[n6->index], ip6addr, 16)) {
            ieee80211_try_ref_node(n6->node);	/* mark referenced */
            OS_RWLOCK_READ_UNLOCK(&nt->nt_ipv6_hash_lock, &lock_state);
            return n6->node;
        }
    }
    OS_RWLOCK_READ_UNLOCK(&nt->nt_ipv6_hash_lock, &lock_state);
    return NULL;
}

int
ieee80211_node_add_ipv6(struct ieee80211_node_table *nt,
                        struct ieee80211_node *ni,
                        u8 *ip6addr)
{
    struct ieee80211com *ic = nt->nt_ic;
    struct ieee80211_ipv6_node *n6;
    int hash;
    const u8 ipv6_zero_addr[16] = { 0 };
    u8 *oaddr;
    rwlock_state_t lock_state;

    KASSERT(ni, ("node is NULL\n"));

    if (!memcmp(ip6addr, ipv6_zero_addr, 16)) {
        return 0;
    }

    OS_RWLOCK_WRITE_LOCK(&nt->nt_ipv6_hash_lock, &lock_state);
    /* Check if this ip6addr already exists */
    hash = IEEE80211_IPV6_HASH(ip6addr);
    LIST_FOREACH(n6, &nt->nt_ipv6_hash[hash], ni_hash) {
        if (!memcmp(&n6->node->ni_ipv6_addr[n6->index], ip6addr, 16)) {
            if (n6->node == ni) {
                OS_RWLOCK_WRITE_UNLOCK(&nt->nt_ipv6_hash_lock, &lock_state);
                /* The node has already been added */
                return 0;
            }
            /*
             * The ip6addr points to a different node. It might be caused
             * by the IPv6 tentative addr is rejected during DAD.
             */
            LIST_REMOVE(n6, ni_hash);
            goto modify;
        }
    }
    OS_RWLOCK_WRITE_UNLOCK(&nt->nt_ipv6_hash_lock, &lock_state);

    oaddr = ni->ni_ipv6_addr[ni->ni_ipv6_nidx];
    if (memcmp(oaddr, ipv6_zero_addr, 16)) {
        /* no buffer available for the new address */
        return -1;
    }

    /* Insert the new ip to the hash table */
    n6 = OS_MALLOC(ic->ic_osdev, sizeof(struct ieee80211_ipv6_node), GFP_ATOMIC);
    if (!n6) {
        printk("%s: no memory for to alloc ipv6 node!\n", __func__);
        return -1;
    }

    OS_RWLOCK_WRITE_LOCK(&nt->nt_ipv6_hash_lock, &lock_state);
    TAILQ_INSERT_TAIL(&nt->nt_ipv6_node, n6, ni_list);

modify:
    n6->node = ni;
    n6->index = ni->ni_ipv6_nidx;

    hash = IEEE80211_IPV6_HASH(ip6addr);
    LIST_INSERT_HEAD(&nt->nt_ipv6_hash[hash], n6, ni_hash);
    memcpy(ni->ni_ipv6_addr[ni->ni_ipv6_nidx], ip6addr, 16);
    if (++ni->ni_ipv6_nidx == IEEE80211_NODE_IPV6_MAX)
        ni->ni_ipv6_nidx = 0;
    OS_RWLOCK_WRITE_UNLOCK(&nt->nt_ipv6_hash_lock, &lock_state);

    return 0;
}

void
ieee80211_node_remove_ipv6_by_node(struct ieee80211_node_table *nt, struct ieee80211_node *ni)
{
    struct ieee80211_ipv6_node *curr, *next;
    rwlock_state_t lock_state;

    OS_RWLOCK_WRITE_LOCK(&nt->nt_ipv6_hash_lock, &lock_state);
    TAILQ_FOREACH_SAFE(curr, &nt->nt_ipv6_node, ni_list, next) {
        if (curr->node == ni) {
            LIST_REMOVE(curr, ni_hash);
            TAILQ_REMOVE(&nt->nt_ipv6_node, curr, ni_list);
            OS_FREE(curr);
        }
    }
    OS_RWLOCK_WRITE_UNLOCK(&nt->nt_ipv6_hash_lock, &lock_state);
}

void
ieee80211_node_remove_ipv6(struct ieee80211_node_table *nt, u8 *ip6addr)
{
    struct ieee80211_ipv6_node *n6;
    int hash;
    const u8 ipv6_zero_addr[16] = { 0 };
    rwlock_state_t lock_state;

    if (!memcmp(ip6addr, ipv6_zero_addr, 16)) {
        return;
    }

    OS_RWLOCK_WRITE_LOCK(&nt->nt_ipv6_hash_lock, &lock_state);
    hash = IEEE80211_IPV6_HASH(ip6addr);
    LIST_FOREACH(n6, &nt->nt_ipv6_hash[hash], ni_hash) {
        if (!memcmp(&n6->node->ni_ipv6_addr[n6->index], ip6addr, 16)) {
            LIST_REMOVE(n6, ni_hash);
            TAILQ_REMOVE(&nt->nt_ipv6_node, n6, ni_list);
            memset(&n6->node->ni_ipv6_addr[n6->index], 0, 16);
            OS_FREE(n6);
            OS_RWLOCK_WRITE_UNLOCK(&nt->nt_ipv6_hash_lock, &lock_state);
            return;
        }
    }
    OS_RWLOCK_WRITE_UNLOCK(&nt->nt_ipv6_hash_lock, &lock_state);
}
#endif /* UMAC_SUPPORT_PROXY_ARP */
