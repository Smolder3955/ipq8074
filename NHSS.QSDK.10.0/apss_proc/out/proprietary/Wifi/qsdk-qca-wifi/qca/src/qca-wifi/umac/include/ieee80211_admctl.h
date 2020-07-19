/*
* Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 *  Copyright (c) 2009 Atheros Communications Inc.  All rights reserved.
 */
/*
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */


#ifndef _IEEE80211_ADMCTL_H
#define _IEEE80211_ADMCTL_H

struct ieee80211_admctl_priv;
typedef struct ieee80211_admctl_priv *ieee80211_admctl_priv_t;

#if UMAC_SUPPORT_ADMCTL
/**
 * Admission control module attach 
 * @param ic         : ieee80211com
 *
 * @return on success return 0.
 *         on failure returns -ve value.
 */
int ieee80211_admctl_attach(struct ieee80211com *ic);

/**
 * Admission control module detach 
 * @param ic         : ieee80211com
 *
 * @return on success return 0.
 *         on failure returns -ve value.
 */
int ieee80211_admctl_detach(struct ieee80211com *ic);

/**
 * Admission control module init 
 * @param ni         : node pointer
 *
 * @return on success return 0.
 *         on failure returns -ve value.
 */
int ieee80211_admctl_init(struct ieee80211_node *ni);

/**
 * Admission control module deinit
 * @param ni         : node pointer
 *
 * @return on success return 0.
 *         on failure returns -ve value.
 */
void ieee80211_admctl_deinit(struct ieee80211_node *ni);

/**
 * Addts request handler
 * @param ni         : node pointer 
 * @param tsinfo     : traffic stream request info 
 * @param dialog_token: dialog_token in the ADDTS request frame
 *
 * @return on success return 0.
 *         on failure returns -ve value.
 */
int ieee80211_admctl_process_addts_req(struct ieee80211_node *ni, ieee80211_tspec_info* tsinfo, int dialog_token);

/**
 * Admctl classifier 
 * @param vap        : vap pointer 
 * @param ni         : node pointer 
 * @param tid        : tid pointer 
 * @param ac         : access category pointer 
 *
 */
void ieee80211_admctl_classify(struct ieee80211vap *vap, struct ieee80211_node *ni, int *tid, int*ac);

/**
 * Admctl delete ts for node 
 * @param vap        : vap pointer 
 * @param ni         : node pointer 
 *
 */
int ieee80211_admctl_node_leave(struct ieee80211vap *vap, struct ieee80211_node *ni);

/**
 * Delts request handler
 * @param ni         : node pointer 
 * @param tsinfo     : traffic stream request info 
 * 
 * @return on success return 0.
 * on failure returns -ve value.
 */
int ieee80211_admctl_process_delts_req(struct ieee80211_node *ni, ieee80211_tspec_info* tsinfo);

#else /* UMAC_SUPPORT_ADMCTL */
static INLINE int ieee80211_admctl_init(struct ieee80211_node *ni) { return 0; }
static INLINE int ieee80211_admctl_deinit(struct ieee80211_node *ni) { return 0; }
static INLINE int ieee80211_admctl_node_leave(struct ieee80211vap *vap, struct ieee80211_node *ni) { return 0; }
#define ieee80211_admctl_process_addts_req(ni, tsinfo, dialogtoken) (0)
#define ieee80211_admctl_process_delts_req(ni, tsinfo) (0)
#define ieee80211_admctl_classify(vap, ni, tid, ac) /* */
#define ieee80211_admctl_attach(ic) /**/
#define ieee80211_admctl_detach(ic) /**/
#endif /* UMAC_SUPPORT_ADMCTL */

#endif
