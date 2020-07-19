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
 *  WMM_AC common routines.
 */
#include "ieee80211_admctl_priv.h"

#if UMAC_SUPPORT_ADMCTL

/* 
 * Attach admission control module
 */
int ieee80211_admctl_attach(struct ieee80211com *ic)
{
	ieee80211_admctl_ap_attach(ic);
	return 0;
}

int ieee80211_admctl_detach(struct ieee80211com *ic)
{
	ieee80211_admctl_ap_detach(ic);
	return 0;
}

int ieee80211_admctl_init(struct ieee80211_node *ni)
{
    struct ieee80211com    *ic = ni->ni_ic;
    struct ieee80211_admctl_priv *admctl_priv;
    u_int8_t ac;

    admctl_priv = (ieee80211_admctl_priv_t) OS_MALLOC(ic->ic_osdev, 
                      (sizeof(struct ieee80211_admctl_priv)),0);
    ni->ni_admctl_priv = admctl_priv;
    if (admctl_priv == NULL) {
        return -ENOMEM;
    }
    else {
        OS_MEMZERO(admctl_priv, sizeof(struct ieee80211_admctl_priv));
        admctl_priv->ni = ni;
        admctl_priv->osdev = ic->ic_osdev;
    }

    for (ac = 0; ac < WME_NUM_AC; ac++) {
        ni->ni_uapsd_dyn_trigena[ac] = -1;
        ni->ni_uapsd_dyn_delivena[ac] = -1;
    }

    return 0;
}

/* 
 * Detach admission control module
 */
void ieee80211_admctl_deinit(struct ieee80211_node *ni)
{
    struct ieee80211_admctl_priv *admctl_priv = ni->ni_admctl_priv;
    if (admctl_priv) {
        OS_FREE(admctl_priv);
    }
}

/* 
 * Delete traffic stream for the passed macaddr and ac.
 */
int ieee80211_remove_tsentry(ieee80211_admctl_priv_t admctl_priv, u_int8_t tid)
{
    struct ieee80211_admctl_tsentry *tsentry;
    struct ieee80211com *ic = admctl_priv->ni->ni_ic;
    int status = 0;

    LIST_FOREACH(tsentry, &admctl_priv->ts_list, ts_entry) {
        if (tsentry->ts_info.tid == tid) {
            ieee80211_restore_psb(admctl_priv->ni, tsentry->ts_info.direction, tsentry->ts_ac, WME_UAPSD_AC_INVAL, WME_UAPSD_AC_INVAL);
            ic->ic_mediumtime_reserved -= tsentry->ts_info.medium_time;
            LIST_REMOVE(tsentry, ts_entry);
            OS_FREE(tsentry);
            status = 1;
            break;
        }
    }
    return status;
}

 /*
 **        Update UAPSD setting based on psb flag 
 */
void ieee80211_parse_psb(struct ieee80211_node *ni, u_int8_t direction,
                                u_int8_t psb, u_int8_t ac,int8_t ac_delivery,int8_t ac_trigger)
{
	struct ieee80211com *ic = ni->ni_ic;
       switch(direction) {
       case ADMCTL_TSPEC_UPLINK:
		   ac_trigger = psb;
           break;
       case ADMCTL_TSPEC_DOWNLINK:
		   ac_delivery = psb;
           break;
       case ADMCTL_TSPEC_BIDI:
		   ac_delivery = psb;
		   ac_trigger = psb;
           break;
       }
	   IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_ACTION, "%s  ac %d acd %d act %d\n",__func__,ac,ac_delivery,ac_trigger);
	   ic->ic_node_update_dyn_uapsd(ni,ac,ac_delivery,ac_trigger);
}

void ieee80211_restore_psb(struct ieee80211_node *ni, u_int8_t direction,  u_int8_t ac, int8_t ac_delivery, int8_t ac_trigger)
{
	struct ieee80211com *ic = ni->ni_ic;
    switch(direction) {
    case ADMCTL_TSPEC_UPLINK:
		ac_trigger = -1;
        break;
    case ADMCTL_TSPEC_DOWNLINK:
		ac_delivery = -1;
        break;
    case ADMCTL_TSPEC_BIDI:
		ac_trigger = -1;
		ac_delivery = -1;
        break;
    }
	ic->ic_node_update_dyn_uapsd(ni,ac,ac_delivery,ac_trigger);
}

/*
 * Add traffic stream for the passed macaddr and ac.
 */
struct ieee80211_admctl_tsentry* ieee80211_add_tsentry(ieee80211_admctl_priv_t admctl_priv, 
                                                       u_int8_t ac, u_int8_t tid)
{
    struct ieee80211_admctl_tsentry *tsentry = NULL;

    tsentry = (struct ieee80211_admctl_tsentry *)OS_MALLOC(admctl_priv->osdev,
                             sizeof(struct ieee80211_admctl_tsentry), GFP_KERNEL);
    if (!tsentry)
        return NULL;

    OS_MEMZERO(tsentry, sizeof(struct ieee80211_admctl_tsentry));
    tsentry->ts_ac = ac;
    LIST_INSERT_HEAD(&admctl_priv->ts_list, tsentry, ts_entry);
    return tsentry;
}

/* 
 * Find traffic stream associated with passed macaddr and ac 
 */
struct ieee80211_admctl_tsentry* ieee80211_find_tsentry(ieee80211_admctl_priv_t admctl_priv, 
                                                        u_int8_t tid)
{
    struct ieee80211_admctl_tsentry *tsentry;
    LIST_FOREACH(tsentry, &admctl_priv->ts_list, ts_entry) {
        if (tsentry->ts_info.tid == tid)
            return tsentry;
    }
    return NULL;
}

/* 
 * Node is leaving. Delete all the traffic stream associated with this node
 */
int ieee80211_admctl_node_leave(struct ieee80211vap *vap, struct ieee80211_node *ni)
{
    int ac;
    struct ieee80211_admctl_priv *admctl_priv = ni->ni_admctl_priv;
    /* Delete all TS for this node */
    if (admctl_priv) {
        for (ac = 0; ac < WME_NUM_AC; ac++) {
            ieee80211_remove_ac_tsentries(admctl_priv, ac);
        }
    }
    return 0;
}

/* 
 * Check if ACM is enabled for this AC and if enabled, 
 * verify TSPEC is active for this AC
 */
void ieee80211_admctl_classify(struct ieee80211vap *vap, struct ieee80211_node *ni, int *tid, int*ac)
{
    struct ieee80211_admctl_tsentry *tsentry;
    if (wlan_get_wmm_param(vap, WLAN_WME_ACM, 1, *ac)) {
        tsentry = ieee80211_find_ac_tsentry(ni->ni_admctl_priv, *ac, ADMCTL_TSPEC_DOWNLINK);
        /* TBD: Need to add available medium time check also */
        if (tsentry) {
            return;
        }
       /* ACM is set - but no traffic stream established 
        * force to send the frames at BE
        * with tid 0
        */
       *ac = WME_AC_BE;
       *tid = 0;
    }
}


/**
 * Set the admission contol filters and start admission control
 * process.
 * @param vaphandle         : handle to the VAP
 *
 * @return on success return 0.
 *         on failure returns -ve value.
 */
int ieee80211_remove_ac_tsentries(ieee80211_admctl_priv_t admctl_priv,
                                            u_int8_t ac)
{
    int status = 0;
    struct ieee80211_admctl_tsentry *tsentry;
    struct ieee80211_admctl_tsentry *temp_tsentry;
    struct ieee80211com *ic = NULL;

    if (admctl_priv && admctl_priv->ni) {
        ic = admctl_priv->ni->ni_ic;
	LIST_FOREACH_SAFE(tsentry, &admctl_priv->ts_list, ts_entry, temp_tsentry) {
	    if (tsentry->ts_ac == ac) {
		ieee80211_restore_psb(admctl_priv->ni, tsentry->ts_info.direction, ac, WME_UAPSD_AC_INVAL, WME_UAPSD_AC_INVAL);
		ic->ic_mediumtime_reserved -= tsentry->ts_info.medium_time;
		LIST_REMOVE(tsentry, ts_entry);
		OS_FREE(tsentry);
		status = 1;
	    }
	}
    }
    return status;
}


struct ieee80211_admctl_tsentry* ieee80211_find_ac_tsentry(ieee80211_admctl_priv_t
               admctl_priv, u_int8_t ac, u_int8_t direction)
{
    struct ieee80211_admctl_tsentry *tsentry;

    LIST_FOREACH(tsentry, &admctl_priv->ts_list, ts_entry) {
        if (tsentry->ts_ac == ac) {
            switch (direction) {
            case ADMCTL_TSPEC_BIDI:
                return tsentry;
            case ADMCTL_TSPEC_DOWNLINK:
            case ADMCTL_TSPEC_UPLINK:
                if ((direction == tsentry->ts_info.direction) || 
                    (tsentry->ts_info.direction == ADMCTL_TSPEC_BIDI)) {
                    return tsentry;
                }
                break;
            }
        }
    }
    return NULL;
}


struct ieee80211_admctl_tsentry* ieee80211_add_replace_tsentry(struct ieee80211_node *ni,
                                u_int8_t ac,ieee80211_tspec_info *tsinfo)
{
    int status;
    struct ieee80211_admctl_tsentry *tsentry;
    u_int16_t old_mediumtime;
	int8_t ac_delivery, ac_trigger;
    ieee80211_admctl_priv_t admctl_priv;
  
    ac_delivery = ac_trigger = WME_UAPSD_AC_INVAL;
        admctl_priv = ni->ni_admctl_priv;
    /* search for the each tsentry in the list */ 
    tsentry = ieee80211_find_ac_tsentry(admctl_priv, ac,
                                        tsinfo->direction);
    if (tsentry) {
        switch (tsinfo->direction) {
            case ADMCTL_TSPEC_BIDI:
                status = ieee80211_remove_ac_tsentries(admctl_priv, ac);
                if(status) {
                    tsentry = ieee80211_add_tsentry(admctl_priv, ac, tsinfo->tid);
                    if (tsentry) {
                        OS_MEMCPY(&tsentry->ts_info, tsinfo, 
                                    sizeof(ieee80211_tspec_info));
                        tsentry->ts_info.medium_time = 0; 
                    }
                }
            break;
            case ADMCTL_TSPEC_UPLINK:
                /* replace tspec, but restore previous medium time */
                old_mediumtime = tsentry->ts_info.medium_time;
                if(tsentry->ts_info.direction == ADMCTL_TSPEC_UPLINK) {
                    OS_MEMCPY(&tsentry->ts_info, tsinfo,
                              sizeof(ieee80211_tspec_info));
					ac_trigger = -1;
		
                } else if (tsentry->ts_info.direction == ADMCTL_TSPEC_BIDI) {
                    OS_MEMCPY(&tsentry->ts_info, tsinfo,
                              sizeof(ieee80211_tspec_info));
					ac_trigger = -1;
					ac_delivery = -1;
                }
                tsentry->ts_info.medium_time = old_mediumtime; 
            break;
            case ADMCTL_TSPEC_DOWNLINK:
                /* replace tspec, but restore previous medium time */
                old_mediumtime = tsentry->ts_info.medium_time;
                if(tsentry->ts_info.direction == ADMCTL_TSPEC_DOWNLINK) {
                    OS_MEMCPY(&tsentry->ts_info, tsinfo,
                              sizeof(ieee80211_tspec_info));
					ac_delivery = -1;
                } else if (tsentry->ts_info.direction == ADMCTL_TSPEC_BIDI) {
                    OS_MEMCPY(&tsentry->ts_info, tsinfo,
                              sizeof(ieee80211_tspec_info));
					ac_trigger = -1;
					ac_delivery = -1;
                }
                tsentry->ts_info.medium_time = old_mediumtime; 
            break;
        }
    } 
    else {
        tsentry = ieee80211_add_tsentry(admctl_priv, ac, tsinfo->tid);
        if (tsentry) {
            OS_MEMCPY(&tsentry->ts_info, tsinfo,
                      sizeof(ieee80211_tspec_info));
            tsentry->ts_info.medium_time = 0; 
        }
    }

    ieee80211_parse_psb(ni, tsinfo->direction, tsinfo->psb, ac, ac_delivery, ac_trigger);

    return tsentry;
}


/**
 * Process the add tspec request and fill in the tspec response.  
 * @param vaphandle         : handle to the VAP
 * @param macaddr           : mac addr which requested traffic stream 
 * @param tspecie           : tspec ie for addts request 
 *
 * @return on success return 0.
 *         on failure returns -ve value.
 */
int wlan_admctl_addts(struct ieee80211vap *vap, u_int8_t *macaddr, u_int8_t *tspecie)
{
    int error = 0;
    struct ieee80211_node    *ni;
    ni = ieee80211_vap_find_node(vap, macaddr);
    IEEE80211_NOTE_MAC(vap, IEEE80211_MSG_ACTION, macaddr, "%s : \n", __func__);
    if (ni == NULL) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ACTION,"%s : \n", "ieee80211_vap_find_node failed");
        return -EINVAL;
    }
    error = ieee80211_admctl_addts(ni, (struct ieee80211_wme_tspec* )tspecie);
    /* claim node immediately */
    ieee80211_free_node(ni);

    return error;
};

/**
 * Send the DELTS to the specified tid and macaddr.  
 * @param vaphandle         : handle to the VAP
 * @param macaddr           : mac addr which requested traffic stream 
 * @param tid               : send DELTS for this specified tid 
 *
 * @return on success return 0.
 *         on failure returns -ve value.
 */
int wlan_admctl_send_delts(struct ieee80211vap *vap, u_int8_t *macaddr, u_int8_t tid)
{
    int error = 0;
    IEEE80211_NOTE_MAC(vap, IEEE80211_MSG_ACTION, macaddr, "%s : \n", __func__);
    if (vap->iv_opmode == IEEE80211_M_STA) {
        error = ieee80211_admctl_send_sta_delts(vap, macaddr, tid);
    }
    else {
        error = ieee80211_admctl_send_delts(vap, macaddr, tid);
    }
    return error;
};

int ieee80211_admctl_send_sta_delts(struct ieee80211vap *vap, u_int8_t *macaddr, u_int8_t tid)
{
    int error = 0;
    struct ieee80211_admctl_tsentry temp_tsentry;

    OS_MEMZERO(&temp_tsentry.ts_info, sizeof(ieee80211_tspec_info));

    temp_tsentry.ts_info.tid = tid;
    temp_tsentry.ts_info.acc_policy_edca = 1;

    error =  wlan_send_delts(vap, macaddr, &temp_tsentry.ts_info);
    if (error < 0) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ACTION, "%s : wlan_send_delts is failed \n", __func__);
        return -EINVAL;
    }
    error = 1;
    return error;
}


#endif /* UMAC_SUPPORT_ADMCTL */

