/*
 * Copyright (c) 2011,2017 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 *  Copyright (c) 2009 Atheros Communications Inc.
 *  All Rights Reserved.
 *
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 *
 *  Header file for IQUE feature.
 */

#ifndef _IEEE80211_IQUE_H_
#define _IEEE80211_IQUE_H_

#include "osdep.h"
#include "wbuf.h"
#include "ieee80211.h"

struct ieee80211_node;
struct ieee80211vap;

/*
 * mcast enhancement ops
 */
struct ieee80211_ique_ops {
    /*
     * functions for mcast enhancement
     */
    int     (*me_inspect)(struct ieee80211vap *, struct ieee80211_node *, wbuf_t);
    void    (*me_detach)(struct ieee80211vap *);
#if ATH_SUPPORT_ME_SNOOP_TABLE
    int     (*me_convert)(struct ieee80211vap *, wbuf_t);
    void    (*me_dump)(struct ieee80211vap *);
    void    (*me_showdeny)(struct ieee80211vap *);
    int    (*me_adddeny)(struct ieee80211vap *, int *grp_addr);
    void    (*me_cleardeny)(struct ieee80211vap *);
    void    (*me_deletegrp)(struct ieee80211vap *, u_int8_t* grp_addr, u_int8_t *grp_ipaddr, u_int32_t type);
    void    (*me_clean)(struct ieee80211_node *);
#endif
    int     (*me_hmmc_find)(struct ieee80211vap *, u_int32_t);
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
    int     (*me_hifi_convert)(struct ieee80211vap *, wbuf_t);
    int     (*me_hifitbl_update)(struct ieee80211vap *, wbuf_t);
#endif

    /*
     * functions for headline block removal (HBR)
     */
    void    (*hbr_detach)(struct ieee80211vap *);
    void    (*hbr_nodejoin)(struct ieee80211vap *, struct ieee80211_node *);
    void    (*hbr_nodeleave)(struct ieee80211vap *, struct ieee80211_node *);
    int     (*hbr_dropblocked)(struct ieee80211vap *, struct ieee80211_node *, wbuf_t);
    void    (*hbr_set_probing)(struct ieee80211vap *, struct ieee80211_node *, wbuf_t, u_int8_t);
    void    (*hbr_sendevent)(struct ieee80211vap *, u_int8_t *, int);
    void    (*hbr_dump)(struct ieee80211vap *);
    void    (*hbr_settimer)(struct ieee80211vap *, u_int32_t);
};

#if ATH_SUPPORT_IQUE

#ifndef MAX_SNOOP_ENTRIES
#define MAX_SNOOP_ENTRIES    64    /* max number*/
#endif
#ifndef DEF_SNOOP_ENTRIES
#define DEF_SNOOP_ENTRIES    8    /* default list length */
#endif

#define DEF_ME_TIMEOUT       120000   /* 2 minutes for timeout  */

#define GRPADDR_FILTEROUT_N 8

/* value to disable the snoop feature, need to set in mcastenhence flag*/
#ifndef MC_SNOOP_DISABLE_CMD
#define MC_SNOOP_TUNNEL_CMD    (1)
#define MC_SNOOP_TRANSLATE_CMD (2)
#define MC_SNOOP_DISABLE_CMD   (4)
#define MC_HYFI_ENABLE         (5)
#define MC_AMSDU_ENABLE        (6)
#endif

/*
 * Data structures for mcast enhancement
 */

typedef rwlock_t ieee80211_snoop_lock_t;

/* TODO: Demo uses single combo list, not optimized */
/* list entry */

#define IGMP_SNOOP_CMD_OTHER 0
#define IGMP_SNOOP_CMD_ADD_EXC_LIST  1
#define IGMP_SNOOP_CMD_ADD_INC_LIST  2
#define IGMP_IP_ADDR_LENGTH  4
#define MLD_IP_ADDR_LENGTH   16
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
#define IEEE80211_ME_HIFI_SRCS_MAX 4
#define IEEE80211_ME_HIFI_IP6_SIZE 16
#endif

enum {
    IGMP_ACTION_ADD_MEMBER,
    IGMP_ACTION_DELETE_MEMBER
};

enum {
    IGMP_WILDCARD_SINGLE,
    IGMP_WILDCARD_ALL
};

/*
 * Struct LIST_UPDATE is use for passing parameters to ListUpdate function
 * which is used for updateing the snoop table.
 * */
struct MC_LIST_UPDATE{
    u_int32_t               src_ip_addr;                     /* source ip address */
    union {
        u_int32_t           grpaddr_ip4;
        u_int8_t            grpaddr_ip6[MLD_IP_ADDR_LENGTH];
    }u;
    u_int32_t               timestamp;                       /* timestamp */
    u_int8_t                grp_addr[IEEE80211_ADDR_LEN];    /* group address where this entry will added*/
    u_int8_t                grp_member[IEEE80211_ADDR_LEN];  /* group member mac address which is requesting update*/
    struct ieee80211vap     *vap;                             /* vap pointer*/
    struct ieee80211_node   *ni;                              /* node pointer*/
    u_int8_t                cmd;                             /* cmd to include, exclude, add or remove*/
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
    u_int8_t                nsrcs;
    u_int8_t                srcs[IEEE80211_ME_HIFI_IP6_SIZE * IEEE80211_ME_HIFI_SRCS_MAX];     /* source ip address */
#endif
};

/**
 * GRP_MEMBER_LIST is used to for storing one group member details
 * under a group list.
 * every entry has
 * src_ip_addr - source ip address
 * grp_member_address - mac address of report sender
 * mode - include / exclude src_ip_address.
 **/
struct MC_GRP_MEMBER_LIST{
    u_int32_t                       src_ip_addr;
    union {
        u_int32_t           grpaddr_ip4;
        u_int8_t            grpaddr_ip6[MLD_IP_ADDR_LENGTH];
    }u;
    u_int32_t                       timestamp;
    struct ieee80211vap             *vap;
    struct ieee80211_node           *niptr;
    u_int8_t                        grp_member_addr[IEEE80211_ADDR_LEN];
    u_int8_t                        mode;
    u_int32_t                       type;
    TAILQ_ENTRY(MC_GRP_MEMBER_LIST) member_list;
};

/**
 * GROUP_LIST_ENTRY is used to save the group members under each
 * groups. group list is based on the group address.
 **/
struct MC_GROUP_LIST_ENTRY {
    TAILQ_HEAD(GRP_MEMBER,MC_GRP_MEMBER_LIST)           src_list;                       /* list of members for this group*/
    u_int8_t                                            group_addr[IEEE80211_ADDR_LEN]; /* group address for this list*/
    union {
        u_int32_t           grpaddr_ip4;
        u_int8_t            grpaddr_ip6[MLD_IP_ADDR_LENGTH];
    }u;
    TAILQ_ENTRY(MC_GROUP_LIST_ENTRY)                    grp_list;
};

struct ipv6_mcast_deny{
    struct in6_addr dst;
    uint8_t mac[IEEE80211_ADDR_LEN];
};

/* global (for demo only) struct to manage snoop */
struct MC_SNOOP_LIST {
    atomic_t                                           msl_group_member_limit;
    atomic_t                                           msl_group_list_limit;
    u_int16_t                                          msl_group_list_count;
    u_int16_t                                          msl_misc;
    u_int16_t                                          msl_max_length;
    u_int16_t                                          msl_list_max_length;
    ieee80211_snoop_lock_t                             msl_lock;    /* lock snoop table */
    u_int8_t                                           msl_deny_count;
    u_int32_t                                          msl_deny_group[GRPADDR_FILTEROUT_N];
    u_int32_t                                          msl_deny_mask[GRPADDR_FILTEROUT_N];
    struct ipv6_mcast_deny                             msl_ipv6_deny_group[GRPADDR_FILTEROUT_N * 2];
    u_int8_t                                           msl_ipv6_deny_list_length;
    TAILQ_HEAD(MSL_HEAD_TYPE, MC_GROUP_LIST_ENTRY)     msl_node;    /* head of list of all snoop entries */
};

#define IEEE80211_REPORT_FROM_STA 1
#define IEEE80211_QUERY_FROM_STA  2

#if ATH_SUPPORT_HYFI_ENHANCEMENTS
#define IEEE80211_ME_HIFI_NODE_MAX 8
#define IEEE80211_ME_HIFI_GROUP_MAX 16
#define IEEE80211_ME_HIFI_INCLUDE 1
#define IEEE80211_ME_HIFI_EXCLUDE 2
struct ieee80211_me_hifi_group {
    u_int32_t                       pro;
    union {
        u_int32_t                   ip4;
        u_int8_t                    ip6[IEEE80211_ME_HIFI_IP6_SIZE];
    } u;
};
struct ieee80211_me_hifi_node {
    u_int8_t                        mac[IEEE80211_ADDR_LEN];
    u_int8_t                        filter_mode;
    u_int8_t                        nsrcs;
    u_int8_t                        srcs[IEEE80211_ME_HIFI_SRCS_MAX * IEEE80211_ME_HIFI_IP6_SIZE];
};
struct ieee80211_me_hifi_entry {
    struct ieee80211_me_hifi_group  group;
    u_int32_t                       node_cnt;
    struct ieee80211_me_hifi_node   nodes[IEEE80211_ME_HIFI_NODE_MAX];
};
struct ieee80211_me_hifi_table {
    u_int32_t                       entry_cnt;
    struct ieee80211_me_hifi_entry  entry[IEEE80211_ME_HIFI_GROUP_MAX];
};
struct ieee80211_me_ra_entry {
    uint8_t                         mac[IEEE80211_ADDR_LEN];
    bool                            dup; /* Duplicate bit is set if the peer's next hop is the same as an existing entry */
};
#endif

struct ieee80211_ique_me {
    struct ieee80211vap *me_iv;	/* Backward pointer to vap instance */
    int             mc_snoop_enable;    /* option for enabling snoop feature */
    int             mc_mcast_enable;   /* option to define multicast enhancement when snoop is disabled this flag doesn't have any meaning*/
#if ATH_SUPPORT_ME_SNOOP_TABLE
    struct MC_SNOOP_LIST ieee80211_me_snooplist;
    u_int32_t       me_timeout;		/* Timeout to delete entry from the snoop list if no traffic */
    int             me_debug;
#endif
    int             mc_discard_mcast;   /*discard the mcast frames if empty table entry*/
    spinlock_t	    ieee80211_melock;
    int             me_hifi_enable;
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
    ieee80211_snoop_lock_t          me_hifi_lock;
    struct ieee80211_me_hifi_table  me_hifi_table;
    struct ieee80211_me_ra_entry    me_ra[IEEE80211_ME_HIFI_GROUP_MAX][IEEE80211_ME_HIFI_NODE_MAX];
#endif
};

/*
 * For HBR (headline block removal)
 */

typedef enum {
    IEEE80211_HBR_STATE_ACTIVE,
    IEEE80211_HBR_STATE_BLOCKING,
    IEEE80211_HBR_STATE_PROBING,
} ieee80211_hbr_state;

typedef enum {
    IEEE80211_HBR_EVENT_BACK,
    IEEE80211_HBR_EVENT_FORWARD,
    IEEE80211_HBR_EVENT_STALE,
}ieee80211_hbr_event;

struct ieee80211_hbr_list;

int ieee80211_me_attach(struct ieee80211vap * vap);
int ieee80211_hbr_attach(struct ieee80211vap * vap);

#if ATH_SUPPORT_HBR
#define ieee80211_ique_attach(ret, _vap) do {ret = ieee80211_me_attach(_vap);\
                                             if(ret == 0)\
                                                ret = ieee80211_hbr_attach(_vap);\
                                            } while(0)
#else
#define ieee80211_ique_attach(ret, _vap) do {\
                                                ret = ieee80211_me_attach(_vap);\
                                            } while(0)
#endif

#else

#define ieee80211_ique_attach(ret, _vap) do { OS_MEMZERO(&((_vap)->iv_ique_ops), sizeof(struct ieee80211_ique_ops)); ret = 0;} while(0)

#endif /*ATH_SUPPORT_IQUE*/

#endif /* _IEEE80211_IQUE_H_ */
