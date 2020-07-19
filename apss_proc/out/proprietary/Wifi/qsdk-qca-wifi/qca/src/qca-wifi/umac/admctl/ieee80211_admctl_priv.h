/*
* Copyright (c) 2014, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 *  2014 Qualcomm Atheros, Inc.  All rights reserved. 
 *
 *  Qualcomm is a trademark of Qualcomm Technologies Incorporated, registered in the United
 *  States and other countries.  All Qualcomm Technologies Incorporated trademarks are used with
 *  permission.  Atheros is a trademark of Qualcomm Atheros, Inc., registered in
 *  the United States and other countries.  Other products and brand names may be
 *  trademarks or registered trademarks of their respective owners. 
 */

/*
 * WMM-AC protocol definitions
 */

#include <osdep.h>
#include <sys/queue.h>
#include <ieee80211_var.h>

#if UMAC_SUPPORT_ADMCTL

#define IEEE80211_TS_HASHSIZE (32 * 4)
#define ADMCTL_ACKPOLICY_NORMAL 0
#define ADMCTL_ACKPOLICY_HT_IMM 3
#define ADMCTL_MPDU_DELIMITER_SZ 4
#define ADMCTL_ACK_SIZE  14
#define ADMCTL_BA_SIZE   32
#define ADMCTL_RT_INFO_SIZE 64

/* MAX MEDIUM_TIME is 1 seconds in 1/32 us unit */
#define ADMCTL_MEDIUM_TIME_MAX 31250  

#define ADMCTL_MAX_CHANNEL_UTIL 255

#define IEEE80211_ADMCTL_GET_PHY(rate_table,idx) rate_table->info[idx].phy 
#define IEEE80211_ADMCTL_GET_KBPS(rate_table,idx) rate_table->info[idx].rate_kbps
#define IEEE80211_ADMCTL_GET_RC(rate_table,idx) rate_table->info[idx].rate_code
#define IEEE80211_ADMCTL_GET_CTRLRATE(rate_table,idx) rate_table->info[idx].control_rate
#define IEEE80211_ADMCTL_GET_BITSPERSYMBOL(rate_table,idx) rate_table->info[idx].bits_per_symbol

#define IS_RATECODE_HT(_rc)  (((_rc) & 0xc0) == 0x80)
/* 
 * TSPEC direction 
 */
typedef enum {
  ADMCTL_TSPEC_UPLINK = 0x0,
  ADMCTL_TSPEC_DOWNLINK = 0x1,
  ADMCTL_TSPEC_RESERVED = 0x2,
  ADMCTL_TSPEC_BIDI = 0x3,
} ADMCTL_TSPEC_DIR;

/*
 * ADDTS response status
 */
typedef enum {
  ADMCTL_ADDTS_RESP_ACCEPTED      = 0,
  ADMCTL_ADDTS_RESP_INVALID_PARAM = 1,
  ADMCTL_ADDTS_RESP_REFUSED       = 3,
} ADMCTL_ADDTS_RESP;


/* simple hash for traffic stream identification */
#define IEEE80211_TS_HASH(addr, ac) \
   ((((const u_int8_t *)(addr))[IEEE80211_ADDR_LEN - 1] + ac) % IEEE80211_TS_HASHSIZE)

struct ieee80211_admctl_tsentry {
    LIST_ENTRY(ieee80211_admctl_tsentry) ts_entry;
    u_int8_t    ts_ac;
    ieee80211_tspec_info ts_info;
};

struct ieee80211_admctl_priv {
    struct ieee80211_node *ni;
    osdev_t osdev; /* OS opaque handle */
    ATH_LIST_HEAD  (, ieee80211_admctl_tsentry) ts_list;
};

struct ieee80211_admctl_rt {
   int     rateCount;      			
   struct {
        u_int8_t    phy;        			/* CCK/OFDM/XR */
        u_int32_t   rate_kbps;   			/* transfer rate in kbs */
        u_int8_t    rate_code;				/* rate that goes into hw descriptors */
        u_int8_t    control_rate;			/* Index of next lower basic rate, used for duration computation */
        u_int16_t   bits_per_symbol;		/* number of bits per rate symbol. used to calc tx time */
    } info[ADMCTL_RT_INFO_SIZE];
};

struct ieee80211_admctl_tsentry* ieee80211_add_tsentry(ieee80211_admctl_priv_t admctl_priv,
                      u_int8_t ac, u_int8_t tid);
struct ieee80211_admctl_tsentry* ieee80211_find_tsentry(ieee80211_admctl_priv_t admctl_priv, u_int8_t tid);
int ieee80211_remove_tsentry(ieee80211_admctl_priv_t admctl_priv,  u_int8_t tid);

void ieee80211_restore_psb(struct ieee80211_node *ni,u_int8_t direction, u_int8_t ac, int8_t ac_delivery, int8_t ac_trigger);
void ieee80211_parse_psb(struct ieee80211_node *ni, u_int8_t direction,
                                u_int8_t psb, u_int8_t ac, int8_t ac_delivery, int8_t ac_trigger);

int ieee80211_remove_ac_tsentries(ieee80211_admctl_priv_t admctl_priv,
                                             u_int8_t ac);
struct ieee80211_admctl_tsentry* ieee80211_find_ac_tsentry(ieee80211_admctl_priv_t
               admctl_priv, u_int8_t ac, u_int8_t direction);
struct ieee80211_admctl_tsentry* ieee80211_add_replace_tsentry(struct ieee80211_node *ni, 
                               u_int8_t ac,ieee80211_tspec_info *tsinfo);
int ieee80211_admctl_send_sta_delts(struct ieee80211vap *vap,
                                    u_int8_t *macaddr, u_int8_t tid);

int ieee80211_admctl_addts(struct ieee80211_node *ni, struct ieee80211_wme_tspec* tspec);
int ieee80211_admctl_send_delts(struct ieee80211vap *vap, u_int8_t *macaddr, u_int8_t tid);

int ieee80211_admctl_ap_attach(struct ieee80211com *ic);
int ieee80211_admctl_ap_detach(struct ieee80211com *ic);
#endif /* UMAC_SUPPORT_ADMCTL */
