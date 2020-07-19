/*
 * Copyright (c) 2011-2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011-2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * Copyright (c) 2010, Atheros Communications Inc. 
 * All Rights Reserved.
 */


#include "ieee80211_mlme_priv.h"    /* Private to MLME module */

#if UMAC_SUPPORT_IBSS

static int mlme_start_adhoc(struct ieee80211vap *vap);
static int mlme_adhoc_match_node(struct ieee80211_node *ni);
static void mlme_adhoc_node_list_clear_assoc_state(struct ieee80211vap *vap,
                                       int send_deauth);
static void mlme_adhoc_watchdog_timer_cancel(struct ieee80211vap *vap);
static void mlme_adhoc_watchdog_timer_start(struct ieee80211vap *vap);
static OS_TIMER_FUNC(mlme_adhoc_watchdog_timer);
static void wlan_mlme_join_adhoc_continue(struct ieee80211vap *vap);
static int mlme_create_adhoc_bss_continue(struct ieee80211vap *vap);


/*
 * Join an IBSS. The node must be passed in with reference held.
 */
int wlan_mlme_join_adhoc(wlan_if_t vaphandle, wlan_scan_entry_t bss_entry, u_int32_t timeout)
{
    struct ieee80211vap           *vap = vaphandle;
    struct ieee80211_mlme_priv    *mlme_priv = vap->iv_mlme_priv;
    struct ieee80211com           *ic = vap->iv_ic;
    struct ieee80211_node         *ni;
    int                           error = 0;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME, "%s\n", __func__);

    /* Initialize join state variables */
    ASSERT(mlme_priv->im_request_type == MLME_REQ_NONE);
    mlme_priv->im_request_type = MLME_REQ_JOIN_ADHOC;
    atomic_set(&(mlme_priv->im_join_wait_beacon_to_synchronize), 0);
    mlme_priv->im_connection_up = 0;

    if (IEEE80211_IS_CHAN_DISALLOW_ADHOC(ieee80211_scan_entry_channel(bss_entry))) {
        error = -EINVAL;
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME, 
            "%s Adhoc not allowed on chan %d flagExt %08X\n", __func__,
            ieee80211_chan2ieee(ic, ieee80211_scan_entry_channel(bss_entry)),
            ieee80211_chan_flagext(ieee80211_scan_entry_channel(bss_entry)));
        goto err;
    }
        
    error = ieee80211_join_ibss(vap, bss_entry);
    if (error) {
        goto err;
    }

    ni = vap->iv_bss;

    /* 
     * Issue VAP start request to resource manager.
     * If the return value is EOK, its okay to change the channel synchronously.
     * If the function returns EBUSY, then resource manager will switch channel asynchronously, and
     * post an event through event handler registered by VAP and VAP handler will in turn call
     * wlan_mlme_join_adhoc_continue().
     */
    mlme_priv->im_timeout = timeout;
    error = ieee80211_resmgr_vap_start(ic->ic_resmgr, vap, ni->ni_chan, MLME_REQ_ID, 0, 0);

    if (error == EOK) { /* no resource manager in place */

        /* XXX: Always switch channel even if we are already on that channel.
         * This is to make sure the ATH layer always syncs beacon when we set
         * our home channel. */
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME, "Setting channel number %d\n", ni->ni_chan->ic_ieee);
        ieee80211_set_channel(ic, ni->ni_chan);
        ieee80211_wme_initparams(vap);

        wlan_mlme_join_adhoc_continue(vap);

    } else if (error == EBUSY) {
        /* 
         * resource manager is handling the request asynchronously,
         *  this is transparent to  the caller , return EOK to the caller.
         */
        error = EOK;
    }  else {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME, "%s: Error %d (0x%08X) in ieee80211_resmgr_vap_start\n",
                           __func__, error, error);
    }

    return error;
err:
    mlme_priv->im_request_type = MLME_REQ_NONE;
    return error;
}

static void wlan_mlme_join_adhoc_continue(struct ieee80211vap *vap )
{
    struct ieee80211_mlme_priv    *mlme_priv = vap->iv_mlme_priv;
    struct ieee80211com           *ic = vap->iv_ic;

    /* Put underlying H/W to join state */
    ieee80211_vap_join(vap);

    ic->ic_enable_radar(ic, 0);

    /* Set the appropriate filtering function and wait for Join Beacon */
    MLME_WAIT_FOR_BSS_JOIN(mlme_priv);

    /* Set the timeout timer for Join Failure case. */
    OS_SET_TIMER(&mlme_priv->im_timeout_timer, mlme_priv->im_timeout);
}


void ieee80211_mlme_join_complete_adhoc(struct ieee80211_node *ni)
{
    struct ieee80211vap           *vap = ni->ni_vap;
    struct ieee80211_mlme_priv    *mlme_priv = vap->iv_mlme_priv;
    struct ieee80211com           *ic = vap->iv_ic;

    if (mlme_priv->im_request_type == MLME_REQ_JOIN_ADHOC &&
        MLME_STOP_WAITING_FOR_JOIN(mlme_priv) == 1) 
    {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME, "%s\n", __func__);

        /* Request complete */
        mlme_priv->im_request_type = MLME_REQ_NONE;

        OS_CANCEL_TIMER(&mlme_priv->im_timeout_timer);

        /* Start adhoc (i.e. start distributed beacons) */
        mlme_start_adhoc(vap);

        /*
         * Sync and start beacon generation in join complete context.
         * If we wait until next beacon to do to the sync, there is a chance that
         * peer node might go down and we will be left stranded without beaconing.
         */
        ic->ic_beacon_update(ni, ni->ni_rssi);

        IEEE80211_DELIVER_EVENT_MLME_JOIN_COMPLETE_ADHOC(vap, IEEE80211_STATUS_SUCCESS);
    }
}

/* 
 * Indications 
 */
void ieee80211_mlme_adhoc_join_indication(struct ieee80211_node *ni, wbuf_t wbuf)
{
    struct ieee80211vap           *vap = ni->ni_vap;
    struct ieee80211com           *ic = vap->iv_ic;
    struct ieee80211_mlme_priv    *mlme_priv = vap->iv_mlme_priv;
    ieee80211_ssid                ssid_list[IEEE80211_SCAN_MAX_SSID];
    int                           n_ssid;
    int                           match;

    ASSERT(vap->iv_opmode == IEEE80211_M_IBSS);
    if (vap->iv_opmode != IEEE80211_M_IBSS) {
        return;
    }

    /* IBSS must be up and running */
    if (!mlme_priv->im_connection_up) {
        return;
    }

    /* Check BSSID */
    match = IEEE80211_ADDR_EQ(vap->iv_bss->ni_bssid, ni->ni_bssid);

    /* Check SSID */
    n_ssid = ieee80211_get_desired_ssidlist(vap, ssid_list, IEEE80211_SCAN_MAX_SSID);
    ASSERT(n_ssid == 1);

    if (ni->ni_esslen != 0) {
        match = match && ((ssid_list[0].len == ni->ni_esslen) &&
                          (OS_MEMCMP(ssid_list[0].ssid, ni->ni_essid, ni->ni_esslen) == 0));
    }

    /*
     * If the station was associated, but now its SSID or BSSID is changed, indicate disassociation.
     * Likewise, if the station was not associated, but its SSID and BSSID match with our current
     * BSSID and SSID, indicate association.
     */
    if ((ni->ni_assoc_state == IEEE80211_NODE_ADHOC_STATE_AUTH_ASSOC) && !match) {

        /* Indicate node left IBSS */
        ieee80211_mlme_adhoc_leave_indication(ni, IEEE80211_REASON_AUTH_LEAVE);

    } else if ((ni->ni_assoc_state == IEEE80211_NODE_ADHOC_STATE_UNAUTH_UNASSOC) && match 
               && mlme_adhoc_match_node(ni)) {

        IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME, "%s: node %s\n",
                           __func__, ether_sprintf(ni->ni_macaddr));

        ic->ic_newassoc(ni, 1);

        /* Mark the node associated */
        ni->ni_assoc_state = IEEE80211_NODE_ADHOC_STATE_AUTH_ASSOC;
        /* Indicate association */
        IEEE80211_DELIVER_EVENT_MLME_ASSOC_INDICATION(vap, ni->ni_macaddr, 0, wbuf, NULL);
    }
}

void ieee80211_mlme_adhoc_leave_indication(struct ieee80211_node *ni, u_int16_t reason_code)
{
    struct ieee80211vap           *vap = ni->ni_vap;
    struct ieee80211_mlme_priv    *mlme_priv = vap->iv_mlme_priv;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME, "%s\n", __func__);

    ASSERT(vap->iv_opmode == IEEE80211_M_IBSS);
    if (vap->iv_opmode != IEEE80211_M_IBSS) {
        return;
    }

    /* IBSS must be up and running */
    if (!mlme_priv->im_connection_up) {
        return;
    }

    /* This station is no longer associated. */
    ni->ni_assoc_state = IEEE80211_NODE_ADHOC_STATE_UNAUTH_UNASSOC;

    /* Call MLME indication handler */
    IEEE80211_DELIVER_EVENT_MLME_DISASSOC_INDICATION(vap, ni->ni_macaddr, 0, reason_code);
}

static void mlme_adhoc_merge_prepare_iter(void *arg, struct ieee80211_node *ni)
{
    struct ieee80211vap *vap = (struct ieee80211vap *) arg;
    struct ieee80211_rsnparms      *my_rsn = &vap->iv_rsn;

    if (vap != ni->ni_vap) return;
    if (ni->ni_assoc_state == IEEE80211_NODE_ADHOC_STATE_AUTH_ASSOC) {
        /*
         * If we are to send de-auth messages, send one to each associated station.
         *  
         * Do not send de-auth when authentication is open or shared key.
         * If we do, some WLAN drivers lose their keys. This can cause 
         * problem if we reconnect and they are not re-initialized.
         */
        if (!RSN_AUTH_IS_OPEN(my_rsn) && !RSN_AUTH_IS_SHARED_KEY(my_rsn)) {
            wlan_mlme_deauth_request(vap, ni->ni_macaddr, IEEE80211_REASON_AUTH_LEAVE);

            IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME, "%s: Sent deauth to adhoc STA: %s\n",
                              __func__, ether_sprintf(ni->ni_macaddr));
        }

        /* Indicate node left IBSS */
        ieee80211_mlme_adhoc_leave_indication(ni, IEEE80211_REASON_AUTH_LEAVE);
    }

}

void ieee80211_mlme_adhoc_merge_prepare(struct ieee80211vap *vap)
{
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME, "%s\n", __func__);

    ieee80211_iterate_node(vap->iv_ic,mlme_adhoc_merge_prepare_iter,(void *)vap);
}

void ieee80211_mlme_adhoc_merge_start(struct ieee80211_node *ni)
{
    struct ieee80211vap    *vap = ni->ni_vap;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME, "%s\n", __func__);

    /* Call MLME indication handler */
    IEEE80211_DELIVER_EVENT_MLME_IBSS_MERGE_START_INDICATION(vap, vap->iv_bss->ni_bssid);
}

void ieee80211_mlme_adhoc_merge_completion(struct ieee80211_node *ni)
{
    struct ieee80211vap    *vap = ni->ni_vap;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME, "%s\n", __func__);

    /* Call MLME indication handler */
    IEEE80211_DELIVER_EVENT_MLME_IBSS_MERGE_COMPLETE_INDICATION(vap, vap->iv_bss->ni_bssid);
}
/* 
 * Adhoc 
 */
static void
mlme_generate_random_bssid(u_int8_t *macaddr, u_int8_t *bssid)
{
    systime_t    _time;

    /* initialize the random BSSID to be the same as MAC address. */
    IEEE80211_ADDR_COPY(bssid, macaddr);

    /* get the system time in ms. */
    _time = CONVERT_SYSTEM_TIME_TO_MS(OS_GET_TIMESTAMP());

    /* randomize the first 4 bytes of BSSID. */
    bssid[0] ^= (u_int8_t)(_time & 0xff); 
    bssid[0] &= ~0x01;              /* Turn off multicast bit */
    bssid[0] |= 0x02;               /* Turn on local bit */
    
    _time >>= 8;
    bssid[1] ^= (u_int8_t)(_time & 0xff); 
    
    _time >>= 8;
    bssid[2] ^= (u_int8_t)(_time & 0xff); 
    
    _time >>= 8;
    bssid[3] ^= (u_int8_t)(_time & 0xff);
}

int 
mlme_create_adhoc_bss(struct ieee80211vap *vap)    
{
    struct ieee80211com               *ic = vap->iv_ic;
    struct ieee80211_aplist_config    *pconfig = ieee80211_vap_get_aplist_config(vap);
    struct ieee80211_ath_channel          *chan = NULL;
    u_int8_t                          bssid[IEEE80211_ADDR_LEN];
    ieee80211_ssid                    ssid_list[IEEE80211_SCAN_MAX_SSID];
    int                               n_ssid;
    int                               error = 0;


    n_ssid = ieee80211_get_desired_ssidlist(vap, ssid_list, IEEE80211_SCAN_MAX_SSID);
    ASSERT(n_ssid == 1);

    if (n_ssid == 0) {
        error = -EINVAL;
        goto err;
    }

    /*
     * If we have desired BSSID list, use the first BSSID from the list as the
     * BSSID of the new Ad Hoc network. Otherwise, generate a random BSSID. 
     */
    if (!ieee80211_aplist_get_accept_any_bssid(pconfig) &&
        ieee80211_aplist_get_desired_bssid_count(pconfig) > 0) {
        u_int8_t *bssid0;
        
        ieee80211_aplist_get_desired_bssid(pconfig,
                                           0,
                                           &bssid0);
        IEEE80211_ADDR_COPY(bssid, bssid0);
    } else {
        mlme_generate_random_bssid(vap->iv_myaddr, bssid);
    }
    if(ic->ic_softap_enable){
        IEEE80211_ADDR_COPY(bssid, vap->iv_myaddr);
    }
    /* create BSS node for ad-hoc network */
    error = ieee80211_create_ibss(vap, bssid, ssid_list[0].ssid, ssid_list[0].len);
    if (error) {
        goto err;
    }

    /* 
     * Use the desired chan and mode.
     */
    if(vap->iv_des_ibss_chan) {
        chan = ieee80211_find_dot11_channel(ic, vap->iv_des_ibss_chan, vap->iv_des_cfreq2,  vap->iv_des_mode);

        /* If desired chan and mode is not supported, use desired chan.*/
        /*
         * In Windows NDIS would set iv_des_mode to HT40PLUS. However, HT40 is not supported in Windows.
         * Therefore, we need to fall back to HT20, thus the following code is crucial.
         */
        if (chan == NULL) {
            chan = ieee80211_find_dot11_channel(ic, vap->iv_des_ibss_chan, 0, IEEE80211_MODE_AUTO);
        }

        /* don't allow to change to channel with radar and possibly in NOL */
        if (chan && IEEE80211_IS_CHAN_RADAR(chan)) {
            /*
             *  channel has radar, so look for another channel
             */
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME, "%s: channel at freq=%d has radar\n", __func__, chan->ic_freq);
            chan = ieee80211_find_dot11_channel(ic, 0, 0, vap->iv_des_mode | ic->ic_chanbwflag);
            if (chan) {
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME, "%s: picking channel at freq=%d\n", __func__, chan->ic_freq);
                vap->iv_des_chan[vap->iv_des_mode] = chan;
            }
        }
    } else {
        /*
         * In Windows, a preferece list of ibss channel is defined in function
         * ieee80211_autoselect_adhoc_channel(). A default ibss channel is picked
         * in the function when iv_des_ibss_chan = 0.
         */
        chan = ieee80211_autoselect_adhoc_channel(ic);
    }

    if (chan == NULL || IEEE80211_IS_CHAN_DISALLOW_ADHOC(chan)) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME,
                           "%s: Can not find IBSS channel (default = %d)\n", __func__, vap->iv_des_ibss_chan);
        /* Pointing infomation to user ,by defualt we are in FCC domain 
           and user may end up setting dfs channel without changing regualtory. 
           Combination of FCC + adhoc+ dfs channel is not supported ,
           so giving information to user is essential  */

        printk("%s: Invalid channel[%d] \n",__func__, vap->iv_des_ibss_chan);
        error = -EINVAL;
        goto err;
    }

    /*
     * Issue VAP start request to resource manager.
     * If the function returns EOK (0), then its ok to change the channel synchronously.
     * If the function returns EBUSY, then resource manager will switch channel asynchronously
     * and post an event through event handler registred by VAP and VAP handler will
     * in turn call mlme_create_adhoc_bss_continue().
    */
    error = ieee80211_resmgr_vap_start(ic->ic_resmgr, vap, chan, MLME_REQ_ID, 0, 0);
    if (error == EOK) { /* no resource manager in place */

        /* switch to default IBSS channel */
        vap->iv_bsschan = chan;

        /* By default, set the IEEE80211_NODE_HT flag when the channel is HT */
        if (ieee80211_ic_ht20Adhoc_is_set(ic) || ieee80211_ic_ht40Adhoc_is_set(ic)) 
        {
            struct ieee80211_node *ni = vap->iv_bss;
            ieee80211node_set_flag(ni, IEEE80211_NODE_HT);
        }
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME, "%s: Setting channel number %d\n", __func__, chan->ic_ieee);
        ieee80211_vap_join(vap);
        ieee80211_set_channel(ic, chan);

        error = mlme_create_adhoc_bss_continue(vap);
    }

    return error;

err:
    return error;
}

static int mlme_create_adhoc_bss_continue(struct ieee80211vap *vap)
{
    int                    error = 0;

    /* XXX Update WMM params before or after changing channel ??? */
    ieee80211_wme_initparams(vap);

    /* Update channel and rates of the node */
    ieee80211_node_set_chan(vap->iv_bss);
    vap->iv_cur_mode = ieee80211_chan2mode(vap->iv_bss->ni_chan);

    /* Start adhoc */
    error = mlme_start_adhoc(vap);

    return error;
}

void ieee80211_mlme_create_join_ibss_continue(struct ieee80211vap *vap, int32_t status)
{
    struct ieee80211_mlme_priv  *mlme_priv = vap->iv_mlme_priv;
    int                         error = 0;

    if (status == EOK) {
        if (mlme_priv->im_request_type == MLME_REQ_JOIN_ADHOC) {
            /* IBSS Join continuation */
            wlan_mlme_join_adhoc_continue(vap);

        } else {
            /* IBSS Create continuation */
            error = mlme_create_adhoc_bss_continue(vap);
            IEEE80211_DELIVER_EVENT_MLME_JOIN_COMPLETE_ADHOC(vap, error);
        }
    } else {
        IEEE80211_DELIVER_EVENT_MLME_JOIN_COMPLETE_ADHOC(vap, IEEE80211_STATUS_REFUSED);
    }
}

static int 
mlme_start_adhoc(struct ieee80211vap *vap)    
{
    int          error = 0;
    u_int32_t    linkspeed;
    bool         thread_started = false;
    
    /* Update the DotH flags */
    ieee80211_update_spectrumrequirement(vap, &thread_started);

    if(!IEEE80211_IS_CHAN_DFS(vap->iv_bsschan) || vap->iv_state_info.iv_state == IEEE80211_S_JOIN)
    {
        /* NON DFS channel --> Put underlying H/W to RUN state */
        ieee80211_vap_start(vap);
    }

    /* Start adhoc watchdog timer */
    mlme_adhoc_watchdog_timer_start(vap);

    /* indicate linkspeed */
     linkspeed = mlme_dot11rate_to_bps(vap->iv_bss->ni_rates.rs_rates[vap->iv_bss->ni_rates.rs_nrates - 1] & IEEE80211_RATE_VAL);
     IEEE80211_DELIVER_EVENT_LINK_SPEED(vap,linkspeed, linkspeed);

    return error;
}

/*
 * This function determines if we could accept a station on the same
 * ad hoc network as ours based on its attributes other than SSID/BSSID.
 */
static int
mlme_adhoc_match_node(struct ieee80211_node *ni)
{
    struct ieee80211vap       *vap = ni->ni_vap;
    struct ieee80211_aplist_config    *pconfig = ieee80211_vap_get_aplist_config(vap);
    enum ieee80211_phymode    mode;
    u_int8_t                  *macaddr;
    int i,is_ht=0;

    /* Check if the node is on the excluded MAC Addr list */
    for (i = 0; i < ieee80211_aplist_get_exc_macaddress_count(pconfig); i++)  {
        ieee80211_aplist_get_exc_macaddress(pconfig, i, &macaddr);
        if (IEEE80211_ADDR_EQ(ni->ni_macaddr, macaddr)) {
            return 0;
        }
    }

    /* Check if the StaEntry matches the desired PHY list */
    mode = ieee80211_chan2mode(ni->ni_chan);
    if (mode == IEEE80211_MODE_AUTO     ||
        mode == IEEE80211_MODE_FH       ||
        mode == IEEE80211_MODE_TURBO_A  ||
        mode == IEEE80211_MODE_TURBO_G
        ) {
        /* XXX: do not join IBSS in turbo mode */
        return 0;
    }

    if (!(IEEE80211_ACCEPT_ANY_PHY_MODE(vap) ||
          IEEE80211_ACCEPT_PHY_MODE(vap, mode))) {
        return 0;
    }
    if(ni->ni_htcap)
    {
        is_ht = 1;
    }
    /* Check if all basic data rates are supported */
    if (!ieee80211_check_rate(vap, ni->ni_chan, &ni->ni_rates,is_ht)) {
        return 0;
    }

    /* Check security settings */
    if (ni->ni_capinfo & IEEE80211_CAPINFO_PRIVACY)
    {
        if (!IEEE80211_VAP_IS_PRIVACY_ENABLED(vap)) {
            return 0;
        }

        if (!ieee80211_match_rsn_info(vap, &ni->ni_rsn)) {
            return 0;
        }
    } else if (IEEE80211_VAP_IS_PRIVACY_ENABLED(vap)) {
        return 0;
    }

    return 1;
}

typedef struct {
    bool send_deauth;
    struct ieee80211vap *vap;
} mlme_node_list_clear_iter_arg;

/* Mark all nodes as unassoc */
static void mlme_node_list_clear_iter(void *arg, struct ieee80211_node *ni)
{
    mlme_node_list_clear_iter_arg *param = (mlme_node_list_clear_iter_arg *) arg;
    struct ieee80211vap *vap = ni->ni_vap;

    if (vap != param->vap || ni == vap->iv_bss) {
        return; 
    }

    /* If we are to send de-auth messages, send one to each associated station. */
    if (param->send_deauth && (ni->ni_assoc_state == IEEE80211_NODE_ADHOC_STATE_AUTH_ASSOC)) {
        wlan_mlme_deauth_request(vap, ni->ni_macaddr, IEEE80211_REASON_AUTH_LEAVE);

        IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME, "%s: Sent deauth to STA: %s\n",
                          __func__, ether_sprintf(ni->ni_macaddr));

        ieee80211_mlme_adhoc_leave_indication(ni, IEEE80211_REASON_AUTH_LEAVE);
    }
    ni->ni_assoc_state = IEEE80211_NODE_ADHOC_STATE_UNAUTH_UNASSOC;
    ieee80211_sta_leave(ni);
}

static void
mlme_adhoc_node_list_clear_assoc_state(struct ieee80211vap *vap,
                                       int send_deauth)
{
   mlme_node_list_clear_iter_arg param;
   param.send_deauth = send_deauth;
   param.vap = vap;
   ieee80211_iterate_node(vap->iv_ic,mlme_node_list_clear_iter,(void *)&param);
}

static void mlme_adhoc_watchdog_timer_init(struct ieee80211vap *vap)
{
    struct ieee80211com           *ic = vap->iv_ic;
    struct ieee80211_mlme_priv    *mlme_priv = vap->iv_mlme_priv;

    mlme_priv->im_ibss_timer_cancel_flag = 0;
    atomic_set(&(mlme_priv->im_ibss_timer_lock), 0);
    mlme_priv->im_ibss_timer_interval = MLME_IBSS_WATCHDOG_INTERVAL;
    mlme_priv->im_ibss_beacon_miss_alert = MLME_IBSS_BEACON_MISS_ALERT;

    OS_INIT_TIMER(ic->ic_osdev, &mlme_priv->im_ibss_timer, 
                  mlme_adhoc_watchdog_timer, vap, QDF_TIMER_TYPE_WAKE_APPS);
    ASSERT(mlme_priv->im_disassoc_timeout >= mlme_priv->im_ibss_beacon_miss_alert);
}

static void mlme_adhoc_watchdog_timer_cancel(struct ieee80211vap *vap)
{
    struct ieee80211_mlme_priv    *mlme_priv = vap->iv_mlme_priv;
    int                           tick_counter = 0;

    /* indicate timer is being cancelled */
    mlme_priv->im_ibss_timer_cancel_flag = 1;

    /* try to acquire timer synchronization object */
    while (OS_ATOMIC_CMPXCHG(&(mlme_priv->im_ibss_timer_lock), 0, 1) == 1) {
        if (tick_counter++ > 1000) {    // no more than 10ms
            break;
        }

        /* busy wait; can be executed at IRQL <= DISPATCH_LEVEL */
        OS_DELAY(10);   
    }

    OS_CANCEL_TIMER(&mlme_priv->im_ibss_timer);

    /* release synchronization object */
    (void) OS_ATOMIC_CMPXCHG(&(mlme_priv->im_ibss_timer_lock), 1, 0); 
}

static void mlme_adhoc_watchdog_timer_start(struct ieee80211vap *vap)
{
    struct ieee80211_mlme_priv    *mlme_priv = vap->iv_mlme_priv;

    /* mark timer as active */
    mlme_priv->im_ibss_timer_cancel_flag = 0;

    OS_SET_TIMER(&mlme_priv->im_ibss_timer, mlme_priv->im_ibss_timer_interval);
}

static void mlme_adhoc_watchdog_timer_delete(struct ieee80211vap *vap)
{
    mlme_adhoc_watchdog_timer_cancel(vap);
    OS_FREE_TIMER(&((struct ieee80211_mlme_priv *)vap->iv_mlme_priv)->im_ibss_timer);

}

#if ATH_SUPPORT_IBSS_NETLINK_NOTIFICATION
static void mlme_adhoc_rssi_monitor_check_indicate(struct ieee80211_node *ni)
{
    struct ieee80211vap            *vap = ni->ni_vap;
    int i, new_rssi_class;
    int send_event = 0, hyst = 0;

    for (i = 0; i < IBSS_RSSI_CLASS_MAX; i++) {
        if (ni->ni_rssi >= vap->iv_ibss_rssi_class[i])
	    break;
    }

    new_rssi_class = i;

    if (vap->iv_ibss_rssi_monitor == 1) {
	/* First time always send event */
	send_event = 1;
	vap->iv_ibss_rssi_monitor++;
    } else if (ni->ni_rssi_class < new_rssi_class) {
	/* STA is moving away */
	hyst = vap->iv_ibss_rssi_class[ni->ni_rssi_class] - ni->ni_rssi;
    } else if (ni->ni_rssi_class > new_rssi_class) {
	/* STA is moving closer */
	hyst = ni->ni_rssi - (vap->iv_ibss_rssi_class[ni->ni_rssi_class - 1] -1);
    }
    /* "hyst" gives how much is the fluctuation beyond the band of current
     * class on higher(moving closer) or lower(moving away) end. Send an
     * event, only if this exceeds the configured hysteresis.
     */
    if ( hyst > vap->iv_ibss_rssi_hysteresis )
	send_event = 1;

    //printk("new class %d old class %d rssi %d hyst %d event %d\n", new_rssi_class, ni->ni_rssi_class, ni->ni_rssi, hyst, send_event);
    if (send_event) {
	IEEE80211_DELIVER_EVENT_IBSS_RSSI_MONITOR(vap, ni->ni_macaddr, new_rssi_class);
	/* store RSSI class in the node */
	ni->ni_rssi_class = new_rssi_class;
    }

}
#endif

static void mlme_adhoc_watchdog_timer_iter(void *arg, struct ieee80211_node *ni)
{
    struct ieee80211vap            *vap = ni->ni_vap;
    systime_t                      beacon_time;
    u_int32_t                      elapsed_time;
    int                            i;
    u_int8_t                       *macaddr;
    struct ieee80211_mlme_priv     *mlme_priv;
    struct ieee80211_aplist_config *pconfig = ieee80211_vap_get_aplist_config(vap);

        
    mlme_priv = vap->iv_mlme_priv;

    /* Disassociate the station if it appears on the exclusion list. */
    if (ni->ni_assoc_state == IEEE80211_NODE_ADHOC_STATE_AUTH_ASSOC) {

        /* Check if the node is on the excluded MAC Addr list */
        for (i = 0; i < ieee80211_aplist_get_exc_macaddress_count(pconfig); i++)  {
            ieee80211_aplist_get_exc_macaddress(pconfig, i, &macaddr);
            if (IEEE80211_ADDR_EQ(ni->ni_macaddr, macaddr)) {
                break;
            }
        }

        if (i < ieee80211_aplist_get_exc_macaddress_count(pconfig)) {
            /* Indicate DISASSOCIATION */
            ieee80211_mlme_adhoc_leave_indication(ni, IEEE80211_REASON_UNSPECIFIED);
        }
    }
        
    /*
     * We must obtain the beacon timestamp BEFORE obtaining the current
     * time to avoid negative values caused by preemption.
     */
    beacon_time  = ni->ni_beacon_rstamp;
    elapsed_time = (u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(OS_GET_TIMESTAMP() - beacon_time);

    /*
     * Disassociate the station if we have not received a beacon or probe response from
     * from it for more than disassocTime.
     */
    if ((ni->ni_assoc_state == IEEE80211_NODE_ADHOC_STATE_AUTH_ASSOC) &&
        (elapsed_time > mlme_priv->im_ibss_beacon_miss_alert)         &&
        !vap->iv_ic->ic_softap_enable)
    {
        /*
         * If reached im_disassoc_timeout without hearing from a peer then
         * disassociate it, otherwise send probe request.
         */
        if (elapsed_time < mlme_priv->im_disassoc_timeout) {
            ni->ni_probe_ticks++;
            if (ni->ni_probe_ticks <= MLME_PROBE_REQUEST_LIMIT) {
                /*
                 * Don't do anything if we are scanning a foreign channel.
                 * Trying to transmit a frame (Probe Request) during a channel change 
                 * (which includes a channel reset) can cause a NMI due to invalid HW 
                 * addresses. 
                 * Trying to transmit a Probe Request while in a foreign channel 
                 * wouldn't do us any good either.
                 */
                if (wlan_scan_can_transmit(ni->ni_vap)) {
                    /*
                     * XXX: Should it be a PROBE REQUEST or NULL frame???
                     */
                    ieee80211_send_probereq(ni, vap->iv_myaddr, ni->ni_macaddr,
                                            ni->ni_bssid, ni->ni_essid, ni->ni_esslen,
                                            vap->iv_opt_ie.ie, vap->iv_opt_ie.length);

                    IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME,
                                      "%s: Sent direct probe request to inactive Ad Hoc station: %s\n",
                                      __func__, ether_sprintf(ni->ni_macaddr));
                }
            }else if (ni->ni_probe_ticks <= (MLME_PROBE_REQUEST_LIMIT+1)) {
                /*
                 * If probe request does not work, chip could be deaf/mute. Reset the chip.
                 */
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME, "%s: Beacon miss, do internal reset!!\n", __func__);
                IEEE80211_DELIVER_EVENT_DEVICE_ERROR(vap);
            }

            return;
        }
#if ATH_SUPPORT_IBSS_DFS
        if (vap->iv_ibssdfs_state == IEEE80211_IBSSDFS_JOINER && 
                vap->iv_ic->ic_curchan->ic_flagext & IEEE80211_CHAN_DFS) {
            vap->iv_ibssdfs_state = IEEE80211_IBSSDFS_WAIT_RECOVERY;
            vap->iv_ibss_dfs_no_channel_switch = true;
            ieee80211_ibss_beacon_update_start(vap->iv_ic);
        }
#endif 
        /*
         * Disassociate a peer if we have not received a beacon or probe
         * response from it for more than disassocTime.
         */
        ieee80211_mlme_adhoc_leave_indication(ni, IEEE80211_REASON_AUTH_LEAVE);
    }

    /*
     * If we just received de-auth from the station, it's in a special state. Change it to 
     * dot11_assoc_state_unauth_unassoc after it reaches its waiting period. This prevents 
     * us from re-associating a station in the situation where the station sends a de-auth 
     * frame, then sends a few beacon before finally quits.
     */
    if (ni->ni_assoc_state == IEEE80211_NODE_ADHOC_STATE_ZERO) {
        if (ni->ni_wait0_ticks++ > MLME_DEAUTH_WAITING_THRESHOLD && !vap->iv_ic->ic_softap_enable)
            ni->ni_assoc_state = IEEE80211_NODE_ADHOC_STATE_UNAUTH_UNASSOC;
    }

    /*
     * Remove the station from the list if we have not received a beacon or
     * probe response from it for more than MLME_NODE_EXPIRE_TIME.
     */
    if (ni->ni_assoc_state == IEEE80211_NODE_ADHOC_STATE_UNAUTH_UNASSOC && !vap->iv_ic->ic_softap_enable) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME,
                          "%s: Ad Hoc station removed: %s\n",
                          __func__, ether_sprintf(ni->ni_macaddr));

        ieee80211_sta_leave(ni);
    }

#if ATH_SUPPORT_IBSS_NETLINK_NOTIFICATION
    /*
     * If STA is still connected, indicate if RSSI class changes.
     */
    if (vap->iv_ibss_rssi_monitor && 
        (ni->ni_assoc_state == IEEE80211_NODE_ADHOC_STATE_AUTH_ASSOC))
      {
        mlme_adhoc_rssi_monitor_check_indicate(ni);
      }
#endif
}

static OS_TIMER_FUNC(mlme_adhoc_watchdog_timer)
{
    struct ieee80211vap            *vap;
    struct ieee80211_mlme_priv     *mlme_priv;
    OS_GET_TIMER_ARG(vap, struct ieee80211vap *);

    mlme_priv = vap->iv_mlme_priv;

    /* 
     * Acquire the synchronization object. 
     * This is used to synchronize with routine mlme_adhoc_watchdog_timer_cancel.
     */
    if (OS_ATOMIC_CMPXCHG(&(mlme_priv->im_ibss_timer_lock), 0, 1) == 1) {
        return;
    }

    /* if timer is being cancelled, do not run handler, and do not rearm timer. */
    if (mlme_priv->im_ibss_timer_cancel_flag) {
        goto exit;
    }


    /* If we are not in the connected state, exit */
    if (!mlme_priv->im_connection_up) {
        goto exit;
    }


    /*
     * Go through the Ad Hoc station list. If we have not received a beacon or
     * probe response from an associated station for more than disassocTime, 
     * we disassociate the station. If we have not received a beacon or probe 
     * response from any station for more than STA_BSS_ENTRY_EXPIRE_TIME, we 
     * remove the station from the list.
     */
    wlan_iterate_all_sta_list(vap,mlme_adhoc_watchdog_timer_iter,NULL);
    
    /* Set the timer again. */
    OS_SET_TIMER(&mlme_priv->im_ibss_timer, mlme_priv->im_ibss_timer_interval);

exit:
    /* release the synchronization object */
    (void) OS_ATOMIC_CMPXCHG(&(mlme_priv->im_ibss_timer_lock), 1, 0);
}

void 
mlme_stop_adhoc_bss(struct ieee80211vap *vap,int flags)
{
    struct ieee80211_rsnparms    *my_rsn = &vap->iv_rsn;
    int                          send_deauth = (flags & WLAN_MLME_STOP_BSS_F_SEND_DEAUTH) != 0;
    int                          clear_assoc_state = (flags & WLAN_MLME_STOP_BSS_F_CLEAR_ASSOC_STATE) != 0;

    /* Cancel watchdog timer */
    mlme_adhoc_watchdog_timer_cancel(vap);
    
    /*
     * Send every station on the ad hoc network a de-auth message and clear their
     * association states.
     *
     * Do not send de-auth when authentication is open or shared key. If we do,
     * some WLAN drivers lose their keys. This can cause problem if we reconnect
     * and they are not re-initialized.
     */
    if (RSN_AUTH_IS_OPEN(my_rsn) ||  RSN_AUTH_IS_SHARED_KEY(my_rsn)) {
        send_deauth = 0;
    }

    if (clear_assoc_state) {
        mlme_adhoc_node_list_clear_assoc_state(vap, send_deauth);
    }
}

void mlme_pause_adhoc_bss(struct ieee80211vap *vap)
{
    struct ieee80211_node *ni;

    wlan_mlme_stop_bss(vap,
                       WLAN_MLME_STOP_BSS_F_SEND_DEAUTH         |
                       WLAN_MLME_STOP_BSS_F_CLEAR_ASSOC_STATE   |
                       WLAN_MLME_STOP_BSS_F_WAIT_RX_DONE        |
                       WLAN_MLME_STOP_BSS_F_NO_RESET);

    ni = ieee80211_try_ref_bss_node(vap);
	if(ni != NULL) {
		ieee80211_dup_ibss(vap, ni);
		ieee80211_free_node(ni);
	}
}

int mlme_resume_adhoc_bss(struct ieee80211vap *vap)
{
    int                    error = 0;
    
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME, "%s\n", __func__);

    /* XXX: Just put H/W into active state */
    vap->iv_bss->ni_tstamp.tsf = 0;
    ieee80211_vap_start(vap);

    /* Start watchdog timer watching for disconnected stations. */
    mlme_adhoc_watchdog_timer_start(vap);

    return error;
}

void mlme_adhoc_vattach(struct ieee80211vap *vap)
{
    mlme_adhoc_watchdog_timer_init(vap);
}

void mlme_adhoc_vdetach(struct ieee80211vap *vap)
{
    mlme_adhoc_watchdog_timer_delete(vap);
}

#define WLAN_GET_RSSI_ITER_VALID 0x8000

typedef struct {
    bool rssi_valid;
    struct ieee80211vap *vap;
    u_int8_t   rssi;
} mlme_get_rssi_iter_arg;

static void mlme_get_rssi_iter(void *arg, struct ieee80211_node *ni)
{
    mlme_get_rssi_iter_arg *param = ( mlme_get_rssi_iter_arg *) arg;
    /* Adhoc should avoid to get the rssi from itself */
    if ( !param->rssi_valid && !(ni == ni->ni_bss_node)) {
        param->rssi = ieee80211node_get_rssi(ni);
        param->rssi_valid=true;
    }

}


/* Get RSSI (adhoc) */
int32_t wlan_get_rssi_adhoc(wlan_if_t vaphandle)
{
    struct ieee80211vap            *vap = vaphandle;
    struct ieee80211com            *ic = vap->iv_ic;
    mlme_get_rssi_iter_arg         param;

    ASSERT(vap->iv_opmode == IEEE80211_M_IBSS);

    /*
     *  Cannot indicate RSSI for every peer,
     *  so just get from the first node.
     */
    param.rssi=0;
    param.rssi_valid=false;
    /*
     * iterate through the table and get the first nodes rssi.
     */
    ieee80211_iterate_node(ic, mlme_get_rssi_iter,(void *) &param);
    
    return (param.rssi);
}

#undef WLAN_GET_RSSI_ITER_VALID


int mlme_recv_auth_ibss(struct ieee80211_node *ni,
                         u_int16_t algo, u_int16_t seq, u_int16_t status_code,
                         u_int8_t *challenge, u_int8_t challenge_length, wbuf_t wbuf)
{
    struct ieee80211vap           *vap = ni->ni_vap;
    struct ieee80211_mlme_priv    *mlme_priv = vap->iv_mlme_priv;

    /* IBSS must be up and running */
    if (!mlme_priv->im_connection_up) {
        return -1;
    }

    /* If the sender is not in our node table, drop the frame. */
    if (ni == vap->iv_bss)
        return -1;

    /*
     * We only process open system authentication request. When we receive such a request,
     * if the sender's association state is dot11_assoc_state_auth_assoc, we respond with
     * a success authentication packet.
     */

    if ((algo != IEEE80211_AUTH_ALG_OPEN) || (seq != 1)) {
        return -1;
    }

    if (ni->ni_assoc_state != IEEE80211_NODE_ADHOC_STATE_AUTH_ASSOC) {
        return -1;
    }

    ieee80211_send_auth(ni, seq + 1, 0, NULL, 0, NULL);

    IEEE80211_DELIVER_EVENT_MLME_AUTH_INDICATION(vap, ni->ni_macaddr, IEEE80211_STATUS_SUCCESS);
    return 0;
}

#endif
