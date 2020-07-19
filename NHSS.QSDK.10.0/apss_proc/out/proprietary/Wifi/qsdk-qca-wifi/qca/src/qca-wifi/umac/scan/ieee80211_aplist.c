/*
 * Copyright (c) 2011, 2017-2018 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 * Copyright (c) 2008 Atheros Communications Inc.
 * All Rights Reserved.
 */

#include <osdep.h>

#include <ieee80211_var.h>

#if UMAC_SUPPORT_APLIST
#include "ieee80211_channel.h"
#include <ieee80211_rateset.h>
#include "if_upperproto.h"
#include "wlan_scan.h"


#include <wlan_cmn.h>
#include <wlan_objmgr_cmn.h>
#include <wlan_objmgr_global_obj.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_objmgr_peer_obj.h>

#ifdef WLAN_CONV_CRYPTO_SUPPORTED
#include "wlan_crypto_global_def.h"
#include "wlan_crypto_global_api.h"
#endif

/** Max number of desired BSSIDs we can handle */
#define IEEE80211_DES_BSSID_MAX_COUNT                  8
/** Maximum number of MAC addresses we support in the excluded list */
#define IEEE80211_EXCLUDED_MAC_ADDRESS_MAX_COUNT       4
/** Typical tx power delta (in dB) between AP and STA */
#define IEEE80211_DEFAULT_TX_POWER_DELTA               6


struct ieee80211_aplist_config {
    /** Desired BSSID list */
    u_int8_t                  des_bssid_list[IEEE80211_DES_BSSID_MAX_COUNT][IEEE80211_ADDR_LEN];
    u_int32_t                 des_nbssid;
    bool                      accept_any_bssid;

    /** Desired PHY list */
    u_int32_t                 active_phy_id;

    /** Desired BSS type */
    enum ieee80211_opmode     des_bss_type;

    /** MAC addresses to be excluded from list of candidate APs */
    u_int8_t                  exc_macaddress[IEEE80211_EXCLUDED_MAC_ADDRESS_MAX_COUNT][IEEE80211_ADDR_LEN];
    int                       exc_macaddress_count; /* # excluded mac addresses */
    bool                      ignore_all_mac_addresses;

    /** Miscelaneous parameters used to build list of candidate APs */
    bool                      strict_filtering;
    u_int32_t                 max_age;

    /** Parameters used to rank candidate APs */
    int                       tx_power_delta;

    /* custom security check function */
    ieee80211_aplist_match_security_func match_security_func;
    void                                 *match_security_func_arg;

    /* Bad AP Timeout value in milli seconds 
     * This value is used to clear scan entry's BAD_AP status flag
     * from the moment its marked BAD_AP until
     * expiration of bad ap timeout specified in this field
     */
    u_int32_t                 bad_ap_timeout;
};


/*
 * Internal UMAC API
 */

void ieee80211_aplist_config_init(struct ieee80211_aplist_config *pconfig)
{
    pconfig->strict_filtering     = false;
    pconfig->max_age              = IEEE80211_SCAN_ENTRY_EXPIRE_TIME;

    /** PHY list */
    pconfig->active_phy_id        = IEEE80211_MODE_AUTO;

    pconfig->des_bss_type         = IEEE80211_M_STA;

    pconfig->tx_power_delta       = IEEE80211_DEFAULT_TX_POWER_DELTA;

    pconfig->des_bssid_list[0][0] = 0xFF;
    pconfig->des_bssid_list[0][1] = 0xFF;
    pconfig->des_bssid_list[0][2] = 0xFF;
    pconfig->des_bssid_list[0][3] = 0xFF;
    pconfig->des_bssid_list[0][4] = 0xFF;
    pconfig->des_bssid_list[0][5] = 0xFF;
    pconfig->des_nbssid           = 1;
    pconfig->accept_any_bssid     = true;
    pconfig->bad_ap_timeout       = BAD_AP_TIMEOUT;
}

int ieee80211_aplist_set_desired_bssidlist(
    struct ieee80211_aplist_config    *pconfig, 
    u_int16_t                         nbssid, 
    u_int8_t                          (*bssidlist)[IEEE80211_ADDR_LEN]
    )
{
    int    i;
    u_int8_t zero_mac[IEEE80211_ADDR_LEN] = { 0, 0, 0, 0, 0, 0 };
    u_int8_t bcast_mac[IEEE80211_ADDR_LEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

    if (nbssid > IEEE80211_DES_BSSID_MAX_COUNT) {
        return EOVERFLOW;
    }

    /* We don't know if the wildcard BSSID will be in the list, so clear flag */
    pconfig->accept_any_bssid = false;

    for (i = 0; i < nbssid; i++) {
        /*
         * All zero MAC address is the indication to clear the pervious set
         * BSSID's. In this case, we convert it to a broadcast MAC address.
         */
        if (OS_MEMCMP(bssidlist[i], zero_mac, IEEE80211_ADDR_LEN) == 0) {
            OS_MEMCPY(bssidlist[i], bcast_mac, IEEE80211_ADDR_LEN);
        }
        IEEE80211_ADDR_COPY(pconfig->des_bssid_list[i], bssidlist[i]);

        /* Update flag based on whether the Wildcard BSSID appears on the list */
        if (IEEE80211_IS_BROADCAST(bssidlist[i]))
            pconfig->accept_any_bssid = true;
    }

    /* Save number of elements on the list */
    pconfig->des_nbssid = nbssid;

    return EOK; 
} 
                                        
int ieee80211_aplist_get_desired_bssidlist(
    struct ieee80211_aplist_config    *pconfig, 
    u_int8_t                          (*bssidlist)[IEEE80211_ADDR_LEN]
    )
{
    int    i;

    for (i = 0; i < pconfig->des_nbssid; i++) {
        IEEE80211_ADDR_COPY(bssidlist[i], pconfig->des_bssid_list[i]);
    }

    return (pconfig->des_nbssid);
}

int ieee80211_aplist_get_desired_bssid_count(
    struct ieee80211_aplist_config    *pconfig
    )
{
    return (pconfig->des_nbssid);
}

int ieee80211_aplist_get_desired_bssid(
    struct ieee80211_aplist_config    *pconfig,
    int                               index, 
    u_int8_t                          **bssid
    )
{
    if (index > pconfig->des_nbssid) {
        return EOVERFLOW;
    }

    *bssid = pconfig->des_bssid_list[index];

    return EOK;
}

bool ieee80211_aplist_get_accept_any_bssid(
    struct ieee80211_aplist_config    *pconfig
    )
{
    return (pconfig->accept_any_bssid);
}

int ieee80211_aplist_set_max_age(
    struct ieee80211_aplist_config    *pconfig, 
    u_int32_t                         max_age
    )
{
    pconfig->max_age = max_age;

    return EOK;
}

u_int32_t ieee80211_aplist_get_max_age(
    struct ieee80211_aplist_config    *pconfig
    )
{
    return (pconfig->max_age);
}

int ieee80211_aplist_set_ignore_all_mac_addresses(
    struct ieee80211_aplist_config    *pconfig, 
    bool                              flag
    )
{
    pconfig->ignore_all_mac_addresses = flag;

    return EOK;
}

bool ieee80211_aplist_get_ignore_all_mac_addresses(
    struct ieee80211_aplist_config    *pconfig
    )
{
    return (pconfig->ignore_all_mac_addresses);
}

int ieee80211_aplist_set_exc_macaddresslist(
    struct ieee80211_aplist_config    *pconfig, 
    u_int16_t                         n_entries, 
    u_int8_t                          (*macaddress)[IEEE80211_ADDR_LEN]
    )
{
    int    i;

    if (n_entries > IEEE80211_EXCLUDED_MAC_ADDRESS_MAX_COUNT) {
        return EOVERFLOW;
    }

    for (i = 0; i < n_entries; i++) {
        IEEE80211_ADDR_COPY(pconfig->exc_macaddress[i], macaddress[i]);
    }

    pconfig->exc_macaddress_count = n_entries;

    return EOK; 
} 
                                        
int ieee80211_aplist_get_exc_macaddresslist(
    struct ieee80211_aplist_config    *pconfig, 
    u_int8_t                          (*macaddress)[IEEE80211_ADDR_LEN]
    )
{
    int    i;

    for (i = 0; i < pconfig->exc_macaddress_count; i++) {
        IEEE80211_ADDR_COPY(macaddress[i], pconfig->exc_macaddress[i]);
    }

    return (pconfig->exc_macaddress_count);
}

int ieee80211_aplist_get_exc_macaddress_count(
    struct ieee80211_aplist_config    *pconfig
    )
{
    return (pconfig->exc_macaddress_count);
}

int ieee80211_aplist_get_exc_macaddress(
    struct ieee80211_aplist_config    *pconfig, 
    int                               index, 
    u_int8_t                          **pmacaddress
    )
{
    if (index >= pconfig->exc_macaddress_count) {
        return EOVERFLOW;
    }

    *pmacaddress = pconfig->exc_macaddress[index];

    return EOK;
}

int ieee80211_aplist_set_desired_bsstype(
    struct ieee80211_aplist_config    *pconfig, 
    enum ieee80211_opmode             bss_type
    )
{
    pconfig->des_bss_type = bss_type;

    return EOK;
}

enum ieee80211_opmode ieee80211_aplist_get_desired_bsstype(
    struct ieee80211_aplist_config    *pconfig
    )
{
    return (pconfig->des_bss_type);
}


int ieee80211_aplist_set_tx_power_delta(
    struct ieee80211_aplist_config    *pconfig, 
    int                               tx_power_delta
    )
{
    pconfig->tx_power_delta = tx_power_delta;

    return EOK;
}

int ieee80211_aplist_get_tx_power_delta(
    struct ieee80211_aplist_config    *pconfig
    )
{
    return (pconfig->tx_power_delta);
}
void ieee80211_aplist_register_match_security_func(
    struct ieee80211_aplist_config    *pconfig,
    ieee80211_aplist_match_security_func match_security_func,
    void *arg
    )
{
    pconfig->match_security_func = match_security_func;
    pconfig->match_security_func_arg = arg;

}

int ieee80211_aplist_set_bad_ap_timeout(
    struct ieee80211_aplist_config    *pconfig, 
    u_int32_t                         bad_ap_timeout
    )
{
    pconfig->bad_ap_timeout = bad_ap_timeout;

    return EOK;
}

int ieee80211_aplist_config_vattach(
    ieee80211_aplist_config_t    *pconfig, 
    osdev_t                      osdev
    )
{
    if ((*pconfig) != NULL) 
        return EINPROGRESS; /* already attached ? */

    *pconfig = (ieee80211_aplist_config_t) OS_MALLOC(osdev, (sizeof(struct ieee80211_aplist_config)),0);

    if (*pconfig != NULL) {
        OS_MEMZERO((*pconfig), sizeof(struct ieee80211_aplist_config));

        ieee80211_aplist_config_init(*pconfig);

        return EOK;
    }

    return ENOMEM;
}

int ieee80211_aplist_config_vdetach(
    ieee80211_aplist_config_t    *pconfig
    )
{
    if ((*pconfig) == NULL) 
        return EINPROGRESS; /* already detached ? */

    OS_FREE(*pconfig);

    *pconfig = NULL;

    return EOK;
}


/******************************* External API *******************************/

int wlan_aplist_set_desired_bssidlist(
    wlan_if_t    vaphandle, 
    u_int16_t    nbssid, 
    u_int8_t     (*bssidlist)[IEEE80211_ADDR_LEN]
    )
{
    return ieee80211_aplist_set_desired_bssidlist(ieee80211_vap_get_aplist_config(vaphandle), nbssid, bssidlist);
}

int wlan_aplist_get_desired_bssidlist(
    wlan_if_t    vaphandle, 
    u_int8_t     (*bssidlist)[IEEE80211_ADDR_LEN]
    )
{
    return ieee80211_aplist_get_desired_bssidlist(ieee80211_vap_get_aplist_config(vaphandle), bssidlist);
}

int wlan_aplist_get_desired_bssid_count(
    wlan_if_t    vaphandle
    )
{
    return ieee80211_aplist_get_desired_bssid_count(ieee80211_vap_get_aplist_config(vaphandle));
}

int wlan_aplist_get_desired_bssid(
    wlan_if_t    vaphandle,
    int          index, 
    u_int8_t     **bssid
    )
{
    return ieee80211_aplist_get_desired_bssid(ieee80211_vap_get_aplist_config(vaphandle), index, bssid);
}

bool wlan_aplist_get_accept_any_bssid(
    wlan_if_t vaphandle
    )
{
    return ieee80211_aplist_get_accept_any_bssid(ieee80211_vap_get_aplist_config(vaphandle));
}

int wlan_aplist_set_max_age(
    wlan_if_t    vaphandle, 
    u_int32_t    max_age
    )
{
    return ieee80211_aplist_set_max_age(ieee80211_vap_get_aplist_config(vaphandle), max_age);
}

u_int32_t wlan_aplist_get_max_age(
    wlan_if_t    vaphandle
    )
{
    return ieee80211_aplist_get_max_age(ieee80211_vap_get_aplist_config(vaphandle));
}

int wlan_aplist_set_ignore_all_mac_addresses(
    wlan_if_t    vaphandle, 
    bool         flag
    )
{
    return ieee80211_aplist_set_ignore_all_mac_addresses(ieee80211_vap_get_aplist_config(vaphandle), flag);
}

bool wlan_aplist_get_ignore_all_mac_addresses(
    wlan_if_t    vaphandle
    )
{
    return ieee80211_aplist_get_ignore_all_mac_addresses(ieee80211_vap_get_aplist_config(vaphandle));
}

int wlan_aplist_set_exc_macaddresslist(
    wlan_if_t    vaphandle, 
    u_int16_t    n_entries, 
    u_int8_t     (*macaddress)[IEEE80211_ADDR_LEN]
    )
{
    return ieee80211_aplist_set_exc_macaddresslist(ieee80211_vap_get_aplist_config(vaphandle), n_entries, macaddress);
} 
                                        
int wlan_aplist_get_exc_macaddresslist(
    wlan_if_t    vaphandle, 
    u_int8_t     (*macaddress)[IEEE80211_ADDR_LEN]
    )
{
    return ieee80211_aplist_get_exc_macaddresslist(ieee80211_vap_get_aplist_config(vaphandle), macaddress);
}

int wlan_aplist_get_exc_macaddress_count(
    wlan_if_t    vaphandle
    )
{
    return ieee80211_aplist_get_exc_macaddress_count(ieee80211_vap_get_aplist_config(vaphandle));
}

int wlan_aplist_get_exc_macaddress(
    wlan_if_t    vaphandle, 
    int          index, 
    u_int8_t     **pmacaddress
    )
{
    return ieee80211_aplist_get_exc_macaddress(ieee80211_vap_get_aplist_config(vaphandle), index, pmacaddress);
}

int wlan_aplist_set_desired_bsstype(
    wlan_if_t                vaphandle, 
    enum ieee80211_opmode    bss_type
    )
{
    return ieee80211_aplist_set_desired_bsstype(ieee80211_vap_get_aplist_config(vaphandle), bss_type);
}

enum ieee80211_opmode wlan_aplist_get_desired_bsstype(
    wlan_if_t    vaphandle
    )
{
    return ieee80211_aplist_get_desired_bsstype(ieee80211_vap_get_aplist_config(vaphandle));
}

int wlan_aplist_set_tx_power_delta(
    wlan_if_t    vaphandle, 
    int          tx_power_delta
    )
{
    return ieee80211_aplist_set_tx_power_delta(ieee80211_vap_get_aplist_config(vaphandle), tx_power_delta);
}

int wlan_aplist_set_bad_ap_timeout(
    wlan_if_t    vaphandle, 
    u_int32_t    bad_ap_timeout
    )
{
    return ieee80211_aplist_set_bad_ap_timeout(ieee80211_vap_get_aplist_config(vaphandle), bad_ap_timeout);
}

void wlan_aplist_init(
    wlan_if_t    vaphandle
    )
{
    ieee80211_aplist_config_init(ieee80211_vap_get_aplist_config(vaphandle));
}

void wlan_aplist_register_match_security_func(wlan_if_t vaphandle, ieee80211_aplist_match_security_func match_security_func, void *arg)
{
    ieee80211_aplist_register_match_security_func(ieee80211_vap_get_aplist_config(vaphandle), match_security_func,arg);
}

/*************************************************************************************/

#define IEEE80211_CANDIDATE_AP_MAX_COUNT              32
/** Max number of PMKID we can handle */
#define IEEE80211_PMKID_MAX_COUNT                      3
/** Max number of enabled multicast cipher algorithms */
#define IEEE80211_MULTICAST_CIPHER_MAX_COUNT           6

/** Cost at which we will reject an association */
#define IEEE80211_ASSOC_COST_MAX_DONT_CONNECT         10

struct candidate_ap_parameters {
    wlan_if_t            vaphandle;
    bool                 strict_filtering;
    bool                 calculate_rank;
    u_int32_t            maximum_age;
    wlan_scan_entry_t    *candidate_ap_list;
    int                  candidate_ap_entries;
    bool                 stop_on_max_count;
    int                  maximum_candidate_ap_length;
    wlan_scan_entry_t    active_ap;
};

/*
 * ieee80211_aplist_sta_mapping is used to specify an arbitrary mapping pair
 * from one value to another value.  This is used to map rssi
 * to hysteresis as well as rssi to utility using ieee80211_aplist_map_value
 * but could be used for other things.  A linear interpolation
 * is used between specified points
 */
typedef struct ieee80211_aplist_sta_mapping {
    int    inVal;
    int    outVal;
} ieee80211_aplist_sta_mapping;

/*
 * Hysteresis Table.  The RSSI of the current and new APs
 * are looked up and a relative hysteresis is determined
 * For the current AP, its lookup RSSI is reduced by this
 * factor, for the new AP, its lookup RSSI is increased by
 * this factor.   This RSSI is added to the current
 * AP and subtracted from the new one.  Algorithm assumes
 * thresholds are in descending order
 */
static const ieee80211_aplist_sta_mapping hysteresis_table[] = {
    { 35,    7 },
    { 25,    5 },
    { 15,    3 },
    { 10,    2 },
    {  5,    2 },
};

/*
 * These tables map RSSI to predicted throughput
 * They should be in decending order
 */

/*
 * 11a table
 */
static const ieee80211_aplist_sta_mapping StaUtility_11a[] = {
    { 100,     30600 },      /* Weak increasing benefit */
    {  21,     30500 },      /*  54 Mbps rate           */
    {  20,     28500 },      /*  48 Mbps rate           */
    {  15,     23700 },      /*  36 Mbps rate           */
    {  11,     17700 },      /*  24 Mbps rate           */
    {   8,     14100 },      /*  18 Mbps rate           */
    {   6,     10100 },      /*  12 Mbps rate           */
    {   4,      7800 },      /*   9 Mbps rate           */
    {   3,      5400 },      /*   6 Mbps rate           */
    {   1,         0 }       /* Failure                 */
};

/* 
 * 11b table
 */
static const ieee80211_aplist_sta_mapping StaUtility_11b[] = {
    { 100,      6500 },      /* Weak increasing benefit  */
    {   7,      6400 },      /*  11 Mbps rate            */
    {   4,      4300 },      /*   5 Mbps rate            */
    {   3,      1800 },      /*   2 Mbps rate            */
    {   2,       900 },      /*   1 Mbps rate            */
    {   0,         1 }       /* Failure - weakly hold on */
};

/*
 * 11g table - 11a rates are derated by 10%
 */
static const ieee80211_aplist_sta_mapping StaUtility_11g[] = {
    { 100,     28000 },      /* Weak increasing benefit  */
    {  21,     27450 },      /*  54 Mbps rate            */
    {  20,     25650 },      /*  48 Mbps rate            */
    {  15,     21330 },      /*  36 Mbps rate            */
    {  11,     15930 },      /*  24 Mbps rate            */
    {   8,     12690 },      /*  18 Mbps rate            */
    {   7,      9090 },      /*  12 Mbps rate            */
    {   6,      6400 },      /*  11 Mbps rate            */
    {   4,      4300 },      /*   5 Mbps rate            */
    {   3,      1800 },      /*   2 Mbps rate            */
    {   2,       900 },      /*   1 Mbps rate            */
    {   0,         1 }       /* Failure - weakly hold on */
};

/*  
 * 11a turbo table - same relative sensitivity as 11a since RSSI
 * drops by 3dB as does absolute sensitivity
 * THIS IS FOR STATIC TURBO - WE SHOULD HAVE A DYNAMIC TURBO TABLE
 */
static const ieee80211_aplist_sta_mapping StaUtility_11a_turbo[] = {
    { 100,     55000 },      /* Weak increasing benefit */
    {  21,     54800 },      /* 108 Mbps rate           */
    {  20,     51500 },      /*  96 Mbps rate           */
    {  15,     43600 },      /*  72 Mbps rate           */
    {  11,     33200 },      /*  48 Mbps rate           */
    {   8,     26800 },      /*  36 Mbps rate           */
    {   7,     19400 },      /*  24 Mbps rate           */
    {   6,     15100 },      /*  18 Mbps rate           */
    {   4,     10600 },      /*  12 Mbps rate           */
    {   2,         0 }       /* Failure                 */
};

/* 
 * 108g turbo table - derate 11a turbo by 0.9 for interference
 * THIS IS FOR STATIC TURBO - WE SHOULD HAVE A DYNAMIC TURBO TABLE
 */
static const ieee80211_aplist_sta_mapping StaUtility_108g[] = {
    { 100,     50000 },      /* Weak increasing benefit */
    {  21,     49320 },      /* 108 Mbps rate           */
    {  20,     46350 },      /*  96 Mbps rate           */
    {  15,     39240 },      /*  72 Mbps rate           */
    {  11,     29880 },      /*  48 Mbps rate           */
    {   8,     24120 },      /*  36 Mbps rate           */
    {   7,     17460 },      /*  24 Mbps rate           */
    {   6,     13590 },      /*  18 Mbps rate           */
    {   4,      9540 },      /*  12 Mbps rate           */
    {   2,         0 }       /* Failure                 */
};

/* 
 * XR table - reduce throughputs of 11a rates and cap at 12Mbps
 */
static const ieee80211_aplist_sta_mapping StaUtility_XR[] = {
    { 100,      6100 },      /* Weak increasing benefit */
    {   6,      6000 },      /*  12 Mbps rate           */
    {   4,      5000 },      /*   9 Mbps rate           */
    {   3,      4000 },      /*   6 Mbps rate           */
    {   0,      2770 },      /*   3 Mbps rate           */
    {  -3,      1820 },      /*   2 Mbps rate           */
    {  -5,       880 },      /*   1 Mbps rate           */
    {  -8,       390 },      /* 1/2 Mbps rate           */
    {  -9,       160 },      /* 1/4 Mbps rate           */
    { -12,         2 }       /* Will attempt to hold on */
};

/* 
 * 11ac table : TODO  find the right values here
 */
static const ieee80211_aplist_sta_mapping StaUtility_11ac[] = {
    { 100,     30600 },      /* Weak increasing benefit */
    {  21,     30500 },      /*  54 Mbps rate           */
    {  20,     28500 },      /*  48 Mbps rate           */
    {  15,     23700 },      /*  36 Mbps rate           */
    {  11,     17700 },      /*  24 Mbps rate           */
    {   8,     14100 },      /*  18 Mbps rate           */
    {   6,     10100 },      /*  12 Mbps rate           */
    {   4,      7800 },      /*   9 Mbps rate           */
    {   3,      5400 },      /*   6 Mbps rate           */
    {   1,         0 }       /* Failure                 */
};

/* 
 * 11na table : TODO  find the right values here
 */
static const ieee80211_aplist_sta_mapping StaUtility_11na[] = {
    { 100,     30600 },      /* Weak increasing benefit */
    {  21,     30500 },      /*  54 Mbps rate           */
    {  20,     28500 },      /*  48 Mbps rate           */
    {  15,     23700 },      /*  36 Mbps rate           */
    {  11,     17700 },      /*  24 Mbps rate           */
    {   8,     14100 },      /*  18 Mbps rate           */
    {   6,     10100 },      /*  12 Mbps rate           */
    {   4,      7800 },      /*   9 Mbps rate           */
    {   3,      5400 },      /*   6 Mbps rate           */
    {   1,         0 }       /* Failure                 */
};

/* 
 * 11ng table : TODO  find the right values here
 */
static const ieee80211_aplist_sta_mapping StaUtility_11ng[] = {
    { 100,     28000 },      /* Weak increasing benefit  */
    {  21,     27450 },      /*  54 Mbps rate            */
    {  20,     25650 },      /*  48 Mbps rate            */
    {  15,     21330 },      /*  36 Mbps rate            */
    {  11,     15930 },      /*  24 Mbps rate            */
    {   8,     12690 },      /*  18 Mbps rate            */
    {   7,      9090 },      /*  12 Mbps rate            */
    {   6,      6400 },      /*  11 Mbps rate            */
    {   4,      4300 },      /*   5 Mbps rate            */
    {   3,      1800 },      /*   2 Mbps rate            */
    {   2,       900 },      /*  1 Mbps rate             */
    {   0,         1 }       /* Failure - weakly hold on */
};

/*
 * 11axa table : TODO  find the right values here
 */
static const ieee80211_aplist_sta_mapping StaUtility_11axa[] = {
    { 100,     30600 },      /* Weak increasing benefit */
    {  21,     30500 },      /*  54 Mbps rate           */
    {  20,     28500 },      /*  48 Mbps rate           */
    {  15,     23700 },      /*  36 Mbps rate           */
    {  11,     17700 },      /*  24 Mbps rate           */
    {   8,     14100 },      /*  18 Mbps rate           */
    {   6,     10100 },      /*  12 Mbps rate           */
    {   4,      7800 },      /*   9 Mbps rate           */
    {   3,      5400 },      /*   6 Mbps rate           */
    {   1,         0 }       /* Failure                 */
};

/*
 * 11axg table : TODO  find the right values here
 */
static const ieee80211_aplist_sta_mapping StaUtility_11axg[] = {
    { 100,     30600 },      /* Weak increasing benefit */
    {  21,     30500 },      /*  54 Mbps rate           */
    {  20,     28500 },      /*  48 Mbps rate           */
    {  15,     23700 },      /*  36 Mbps rate           */
    {  11,     17700 },      /*  24 Mbps rate           */
    {   8,     14100 },      /*  18 Mbps rate           */
    {   6,     10100 },      /*  12 Mbps rate           */
    {   4,      7800 },      /*   9 Mbps rate           */
    {   3,      5400 },      /*   6 Mbps rate           */
    {   1,         0 }       /* Failure                 */
};

/* 
 * Collect the various tables and index by mode
 */
typedef struct ieee80211_utility_table {
    const ieee80211_aplist_sta_mapping    *data;              /* The data */
    const int                             numEntries;         /* The number of entries in the table */
} ieee80211_utility_table;

static const char*
ieee80211_get_phy_mode_name(
    enum ieee80211_phymode    phy_mode
    )
{
    static const char* mode_name[] = {
        /* IEEE80211_MODE_AUTO           */ "auto",
        /* IEEE80211_MODE_11A            */ "11a",
        /* IEEE80211_MODE_11B            */ "11b",
        /* IEEE80211_MODE_11G            */ "11g",
        /* IEEE80211_MODE_FH             */ "fh",
        /* IEEE80211_MODE_TURBO_A        */ "108a",
        /* IEEE80211_MODE_TURBO_G        */ "108g",
        /* IEEE80211_MODE_11NA_HT20      */ "11na_ht20",
        /* IEEE80211_MODE_11NG_HT20      */ "11ng_ht20",
        /* IEEE80211_MODE_11NA_HT40PLUS  */ "11na_ht40plus",
        /* IEEE80211_MODE_11NA_HT40MINUS */ "11na_ht40minus",
        /* IEEE80211_MODE_11NG_HT40PLUS  */ "11ng_ht40plus",
        /* IEEE80211_MODE_11NG_HT40MINUS */ "11ng_ht40minus",
        /* IEEE80211_MODE_11NG_HT40      */ "11ng_ht40",
        /* IEEE80211_MODE_11NA_HT40      */ "11na_ht40",
        /* IEEE80211_MODE_11AC_VHT20     */ "11ac_vht20",
        /* IEEE80211_MODE_11AC_VHT40PLUS */ "11ac_vht40plus",
        /* IEEE80211_MODE_11AC_VHT40MINUS*/ "11ac_vht40minus",
        /* IEEE80211_MODE_11AC_VHT40     */ "11ac_vht40",
        /* IEEE80211_MODE_11AC_VHT80     */ "11ac_vht80",
        /* IEEE80211_MODE_11AC_VHT160    */ "11ac_vht160",
        /* IEEE80211_MODE_11AC_VHT80_80  */ "11ac_vht80_80",
        /* IEEE80211_MODE_11AXA_HE20     */ "11axa_he20",
        /* IEEE80211_MODE_11AXG_HE20     */ "11axg_he20",
        /* IEEE80211_MODE_11AXA_HE40PLUS */ "11axa_he40plus",
        /* IEEE80211_MODE_11AXA_HE40MINUS*/ "11axa_he40minus",
        /* IEEE80211_MODE_11AXG_HE40PLUS */ "11axg_he40plus",
        /* IEEE80211_MODE_11AXG_HE40MINUS*/ "11axg_he40minus",
        /* IEEE80211_MODE_11AXA_HE40     */ "11axa_he40",
        /* IEEE80211_MODE_11AXG_HE40     */ "11axg_he40",
        /* IEEE80211_MODE_11AXA_HE80     */ "11axa_he80",
        /* IEEE80211_MODE_11AXA_HE160    */ "11axa_he160",
        /* IEEE80211_MODE_11AXA_HE80_80  */ "11axa_he80_80",
    };

    if (phy_mode < IEEE80211_N(mode_name)) {
        return mode_name[phy_mode];
    }

    return "unknown";
}

/* 
 * Returns a pointer to the utility table corresponding to the specified PHY mode.
 * Returns EOK if a table is returned successfully, and a negative error code otherwise.
 */ 
static int
ieee80211_get_utility_table(
    enum ieee80211_phymode     phy_mode,
    ieee80211_utility_table    **putility_table
    )
{
    static ieee80211_utility_table utility_tables[] = {
        /* IEEE80211_MODE_AUTO           */ { NULL,                   0                                 },
        /* IEEE80211_MODE_11A            */ { StaUtility_11a,         IEEE80211_N(StaUtility_11a)       },
        /* IEEE80211_MODE_11B            */ { StaUtility_11b,         IEEE80211_N(StaUtility_11b)       },
        /* IEEE80211_MODE_11G            */ { StaUtility_11g,         IEEE80211_N(StaUtility_11g)       },
        /* IEEE80211_MODE_FH             */ { NULL,                   0                                 },
        /* IEEE80211_MODE_TURBO_A        */ { StaUtility_11a_turbo,   IEEE80211_N(StaUtility_11a_turbo) },
        /* IEEE80211_MODE_TURBO_G        */ { StaUtility_108g,        IEEE80211_N(StaUtility_108g)      },
        /* IEEE80211_MODE_11NA_HT20      */ { StaUtility_11na,        IEEE80211_N(StaUtility_11na)      },
        /* IEEE80211_MODE_11NG_HT20      */ { StaUtility_11ng,        IEEE80211_N(StaUtility_11ng)      },
        /* IEEE80211_MODE_11NA_HT40PLUS  */ { StaUtility_11na,        IEEE80211_N(StaUtility_11na)      },
        /* IEEE80211_MODE_11NA_HT40MINUS */ { StaUtility_11na,        IEEE80211_N(StaUtility_11na)      },
        /* IEEE80211_MODE_11NG_HT40PLUS  */ { StaUtility_11ng,        IEEE80211_N(StaUtility_11ng)      },
        /* IEEE80211_MODE_11NG_HT40MINUS */ { StaUtility_11ng,        IEEE80211_N(StaUtility_11ng)      },
        /* IEEE80211_MODE_11NG_HT40      */ { StaUtility_11ng,        IEEE80211_N(StaUtility_11ng)      },
        /* IEEE80211_MODE_11NA_HT40      */ { StaUtility_11na,        IEEE80211_N(StaUtility_11na)      },
        /* IEEE80211_MODE_11AC_VHT20     */ { StaUtility_11ac,        IEEE80211_N(StaUtility_11ac)      },
        /* IEEE80211_MODE_11AC_VHT40PLUS */ { StaUtility_11ac,        IEEE80211_N(StaUtility_11ac)      },
        /* IEEE80211_MODE_11AC_VHT40MINUS*/ { StaUtility_11ac,        IEEE80211_N(StaUtility_11ac)      },
        /* IEEE80211_MODE_11AC_VHT40     */ { StaUtility_11ac,        IEEE80211_N(StaUtility_11ac)      },
        /* IEEE80211_MODE_11AC_VHT80     */ { StaUtility_11ac,        IEEE80211_N(StaUtility_11ac)      },
        /* IEEE80211_MODE_11AC_VHT160    */ { StaUtility_11ac,        IEEE80211_N(StaUtility_11ac)      },
        /* IEEE80211_MODE_11AC_VHT80_80  */ { StaUtility_11ac,        IEEE80211_N(StaUtility_11ac)      },
        /* IEEE80211_MODE_11AXA_HE20     */ { StaUtility_11axa,       IEEE80211_N(StaUtility_11axa)     },
        /* IEEE80211_MODE_11AXG_HE20     */ { StaUtility_11axg,       IEEE80211_N(StaUtility_11axg)     },
        /* IEEE80211_MODE_11AXA_HE40PLUS */ { StaUtility_11axa,       IEEE80211_N(StaUtility_11axa)     },
        /* IEEE80211_MODE_11AXA_HE40MINUS*/ { StaUtility_11axa,       IEEE80211_N(StaUtility_11axa)     },
        /* IEEE80211_MODE_11AXG_HE40PLUS */ { StaUtility_11axg,       IEEE80211_N(StaUtility_11axg)     },
        /* IEEE80211_MODE_11AXG_HE40MINUS*/ { StaUtility_11axg,       IEEE80211_N(StaUtility_11axg)     },
        /* IEEE80211_MODE_11AXA_HE40     */ { StaUtility_11axa,       IEEE80211_N(StaUtility_11axa)     },
        /* IEEE80211_MODE_11AXG_HE40     */ { StaUtility_11axg,       IEEE80211_N(StaUtility_11axg)     },
        /* IEEE80211_MODE_11AXA_HE80     */ { StaUtility_11axa,       IEEE80211_N(StaUtility_11axa)     },
        /* IEEE80211_MODE_11AXA_HE160    */ { StaUtility_11axa,       IEEE80211_N(StaUtility_11axa)     },
        /* IEEE80211_MODE_11AXA_HE80_80  */ { StaUtility_11axa,       IEEE80211_N(StaUtility_11axa)     },
    };

    /* validate received PHY mode */
    if (phy_mode > IEEE80211_N(utility_tables)) {
        QDF_ASSERT(0);
        return EINVAL;
    }

    *putility_table = utility_tables + phy_mode;
    return EOK;
}

/*
 * ieee80211_aplist_map_value maps an input value to an output value using
 * a specified mapping table.  Linear interpolation is used
 * between the specified points.  The input values of the
 * mapping table are assumed to be in decending order.
 */
static int
ieee80211_aplist_map_value(
    int                                   inVal, 
    const ieee80211_aplist_sta_mapping    *mappingTable,
    int                                   numMappingEntries
    )
{
    int    outVal, i, inStep, inPos;

    /*
     * If the input value is greater than or equal to the first
     * entry use the first output value.
     */
    if (inVal >= mappingTable[0].inVal) {
        outVal = mappingTable[0].outVal;
    } else {
        /*
         * Use last value if runs off end of table
         */
        outVal = mappingTable[numMappingEntries-1].outVal;

        /*
         * Step through the entries, stopping when we equal or
         * exceed the value
         */
        for (i = 1; i < numMappingEntries; i++) {
            if (inVal >= mappingTable[i].inVal) {
                inStep = mappingTable[i-1].inVal -
                         mappingTable[i].inVal;

                //
                // Linearly interpolate between this entry and the
                // previous one
                //
                inPos  = inVal - mappingTable[i].inVal;
                outVal = (mappingTable[i-1].outVal * inPos +
                          mappingTable[i].outVal * (inStep - inPos)) /
                          inStep;
                break;
            }
        }
    }

    return outVal;
}

static QDF_STATUS
ieee80211_aplist_clear_bad_ap_flags(
    void                 *arg, 
    wlan_scan_entry_t    scan_entry)
{
    systime_t    bad_ap_time = wlan_util_scan_entry_mlme_bad_ap_time(scan_entry);
    systime_t    current_time   = OS_GET_TIMESTAMP();
    struct candidate_ap_parameters    *pcandidate_ap_parameters = arg;
    struct ieee80211_aplist_config    *pconfig  = 
        ieee80211_vap_get_aplist_config(pcandidate_ap_parameters->vaphandle);

    if (bad_ap_time != 0) {
        if (CONVERT_SYSTEM_TIME_TO_MS(current_time - bad_ap_time) > pconfig->bad_ap_timeout){
            wlan_util_scan_entry_mlme_set_bad_ap_time(scan_entry, 0);
            wlan_util_scan_entry_mlme_set_status(scan_entry, AP_STATE_GOOD);
        }
    }
    else {
        wlan_util_scan_entry_mlme_set_bad_ap_time(scan_entry, 0);
        wlan_util_scan_entry_mlme_set_status(scan_entry, AP_STATE_GOOD);
    }

    return EOK;
}

/*
 * ieee80211_calculate_bss_rank
 * Routine to assign a preference index to each BSS entry. 
 * The BSS with the highest preference index is the one we should 
 * connect to.
 * This function is directly based on XP's 's cservSelectBSS,
 * except that the initial selection of candidate APs is made by
 * function wlan_candidate_list_build.
 */
static void
ieee80211_calculate_bss_rank(
    wlan_if_t            vaphandle,
    wlan_scan_entry_t    scan_entry,
    wlan_scan_entry_t    active_ap
    )
{
    u_int32_t                       hystAdj        = 0;
    ieee80211_utility_table         *pUtilityTable;
    int                             hystMode;
    int                             rssi;
    u_int32_t                       adjusted_utility;
    int32_t                         adjusted_channel_load;
    wlan_chan_t                     channel       = wlan_util_scan_entry_channel(scan_entry);
    enum ieee80211_phymode          phy_mode      = ieee80211_chan2mode(channel);
    struct ieee80211_ie_qbssload    *qbssload_ie  = (struct ieee80211_ie_qbssload *) util_scan_entry_qbssload(scan_entry);
    DEBUG_VAR_DECL_INIT(mac_address, u_int8_t *, util_scan_entry_macaddr(scan_entry));
    /*
     * Compute utility for this choice.  Use hysteresis as
     * follows - if we are connected or joined then the AP
     * that we are connected or joined to has preferece (hystMode = +1)
     * and others have negative preference (hystMode = -1).  If we
     * are not connected or joined then do not use hysteresis
     * (hystAdj = 0)
     */
    if (active_ap != NULL) {
        hystAdj = ieee80211_aplist_map_value(util_scan_entry_rssi(scan_entry),
                                             hysteresis_table,
                                             IEEE80211_N(hysteresis_table));
    }

    hystMode = active_ap ? ((scan_entry == active_ap) ? 1 : -1) : 0;
    
    rssi     = util_scan_entry_rssi(scan_entry) - 
               ieee80211_aplist_get_tx_power_delta(ieee80211_vap_get_aplist_config(vaphandle)) + 
               (hystAdj * hystMode);

    if (ieee80211_get_utility_table(phy_mode, &pUtilityTable) != EOK) {
        return;
    }

    adjusted_utility = ieee80211_aplist_map_value(rssi,
                                                  pUtilityTable->data,
                                                  pUtilityTable->numEntries);

    /* 
     * Give preference for AP with less load(Give a 50 % weightage for that
     * while adjusting BssUtility)
     * Give some more weightage for the current AP to avoid ping-pong effect
     * due to load balancing
     */
    if (qbssload_ie != NULL)
    {
        if (qbssload_ie->channel_utilization > 0) {
            adjusted_channel_load = 
                qbssload_ie->channel_utilization - (hystMode * QBSS_HYST_ADJ);
        
            if (adjusted_channel_load < 1) {
                adjusted_channel_load = 0;
            }
        } else {
            adjusted_channel_load = 0;
        }
        wlan_util_scan_entry_mlme_set_chanload(scan_entry, adjusted_channel_load);
    }

    /* Update rank of this BSS - this is heuristic scaling */
    wlan_util_scan_entry_mlme_set_rank(scan_entry, (wlan_util_scan_entry_mlme_rank(scan_entry) + (adjusted_utility / 500)));

    wlan_util_scan_entry_mlme_set_utility(scan_entry, adjusted_utility);

    IEEE80211_DPRINTF(vaphandle, IEEE80211_MSG_SCANENTRY, "BSSID=%02X:%02X:%02X:%02X:%02X:%02X Ut=%5d Rk=%3d (hyst=(%d * %d) RSSI:%2d => %d, mode:%s, load:%d)\n",
        mac_address[0], mac_address[1], mac_address[2],
        mac_address[3], mac_address[4], mac_address[5],
        adjusted_utility, wlan_util_scan_entry_mlme_rank(scan_entry),
        hystAdj, hystMode,
        util_scan_entry_rssi(scan_entry), rssi, 
        ieee80211_get_phy_mode_name(phy_mode),
        (qbssload_ie != NULL) ? qbssload_ie->channel_utilization : 0);
}

static void
ieee80211_process_selected_bss(
    wlan_scan_entry_t    scan_entry
    )
{
    u_int32_t    rank = wlan_util_scan_entry_mlme_rank(scan_entry);

    wlan_util_scan_entry_mlme_set_rank(scan_entry, rank += 256);

    /* Mark it as good AP */
    wlan_util_scan_entry_mlme_set_status(scan_entry, AP_STATE_GOOD);

}

/*
 * Compare 2 BSS entries based on their AdjustedUtility.
 * If AdjustedUtility is the same, use AdjustedChannelLoad as tie-breaker.
 * Returns: 
 *    <0 if pBSSEntry1 is LESS    than pBSSEntry2
 *     0 if pBSSEntry1 is EQUAL   to   pBSSEntry2
 *    >0 if pBSSEntry1 is GREATER than pBSSEntry2
 */
static int
ieee80211_aplist_compare_scan_entry_ranking(
    wlan_scan_entry_t    scan_entry_1,
    wlan_scan_entry_t    scan_entry_2
    )
{
    if (wlan_util_scan_entry_mlme_utility(scan_entry_1) == wlan_util_scan_entry_mlme_utility(scan_entry_2))
    {
        // if the utility values are the same, give preference to the AP 
        // with less channel load
        return (wlan_util_scan_entry_mlme_chanload(scan_entry_1) - wlan_util_scan_entry_mlme_chanload(scan_entry_2));
    }

    return (wlan_util_scan_entry_mlme_utility(scan_entry_1) - wlan_util_scan_entry_mlme_utility(scan_entry_2));
}

/*
 * Ranks the candidate access points list in the connection context.
 * It will rank and order the access points in the list such that the first
 * entry in the list is the most preferred access point for association. 
 *
 * \note APs which are not picked for use must have their refcounts decremented
 * 
 * \param pStation  Station structure contain the connection context
 * \sa wlan_candidate_list_build
 */
static void
ieee80211_sort_candidate_ap_list(
    wlan_if_t                             vaphandle,
    wlan_scan_entry_t                     *ap_list,
    u_int32_t                             n_entries,
    ieee80211_candidate_list_compare_func compare_func,
    void                                  *compare_arg
    )
{
    wlan_scan_entry_t    tmp_ap;
    int                  i, j;
    int                  compare_result;

    if (n_entries > 0)
    {
        /*
         * Order all the candidate APs according to their rank. 
         */

        for (i = 0; i < n_entries - 1; i++) 
        {
            for (j = i + 1; j < n_entries; j++)
            {
                if (compare_func) {
                    compare_result = compare_func(compare_arg,ap_list[i],ap_list[j]);
                } else {
                    compare_result = ieee80211_aplist_compare_scan_entry_ranking(ap_list[i], ap_list[j]);
                }
                if (compare_result < 0)
                {
                    tmp_ap     = ap_list[i];
                    ap_list[i] = ap_list[j];
                    ap_list[j] = tmp_ap;
                }
            }
        }

        ieee80211_process_selected_bss(ap_list[0]);

#if DBG
        /*
         * The block below is for tracing only
         */
        {
            u_int8_t       *bssid   = NULL;
            wlan_chan_t    channel  = NULL;
            u_int8_t       rssi     = 0;
            u_int32_t      rank     = 0;
            u_int32_t      utility  = 0;

            /*
             * Make these variables appear to be used apart from the
             * printf, so that if the printf is compiled out, the compiler
             * won't complain about these variables being unused.
             */
            bssid   = util_scan_entry_bssid(ap_list[0]);
            channel = wlan_util_scan_entry_channel(ap_list[0]);
            rssi    = util_scan_entry_rssi(ap_list[0]);
            rank    = wlan_util_scan_entry_mlme_rank(ap_list[0]);
            utility = wlan_util_scan_entry_mlme_utility(ap_list[0]);
            IEEE80211_DPRINTF(vaphandle, IEEE80211_MSG_ASSOC,
                "Selected BSSID=%02X:%02X:%02X:%02X:%02X:%02X "
                "PhyType=%08X Ch=%3d Rssi=%02d Rank=%05d Utility=%05d ChFlags=%08X\n",
                bssid[0], bssid[1], bssid[2],
                bssid[3], bssid[4], bssid[5],
                ieee80211_chan2mode(channel),
                ieee80211_channel_ieee(channel),
                rssi,
                rank,
                utility,
                wlan_channel_flags(channel));

            /* Print information about second-best entry if there is one */
            if (n_entries > 1) {
                bssid   = util_scan_entry_bssid(ap_list[1]);
                channel = wlan_util_scan_entry_channel(ap_list[1]);
                rssi    = util_scan_entry_rssi(ap_list[1]);
                rank    = wlan_util_scan_entry_mlme_rank(ap_list[1]);
                utility = wlan_util_scan_entry_mlme_utility(ap_list[1]);
                IEEE80211_DPRINTF(vaphandle, IEEE80211_MSG_ASSOC,
                    "Candidate #1 BSSID=%02X:%02X:%02X:%02X:%02X:%02X "
                    "PhyType=%08X Ch=%3d Rssi=%02d Rank=%05d Utility=%05d ChFlags=%08X\n",
                    bssid[0], bssid[1], bssid[2],
                    bssid[3], bssid[4], bssid[5],
                    ieee80211_chan2mode(channel),
                    ieee80211_channel_ieee(channel),
                    rssi,
                    rank,
                    utility,
                    wlan_channel_flags(channel));
            }
        }
#endif // DBG
    }
}
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
static bool
ieee80211_candidate_aplist_match_security(
    wlan_if_t    vap,
    u_int8_t     privacy,       /* AP is privacy enabled */
    u_int8_t     *rsn_ie,
    u_int8_t     *wpa_ie
    )
{
    bool                         bIsAcceptable = false;
    int                          status = IEEE80211_STATUS_SUCCESS;
    struct ieee80211_rsnparms    *my_rsn = &vap->iv_rsn;
    struct ieee80211_rsnparms    rsn_parms;
    
    if(vap->iv_wps_mode) {
        //qdf_print("================WPS on, skip securiy checking");
        return true;
    }
    
    /*
     * Privacy bit
     */
    if (!IEEE80211_VAP_IS_PRIVACY_ENABLED(vap)) {
        if (privacy) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCANENTRY, "%s", " - Reject (Privacy bit set)\n");
            return false;
        }
    } else if (RSN_CIPHER_IS_WEP(my_rsn)) {
        /*
         * If ExcludeUnencrypted is false, we associate with an AP even if 
         * it is not beaconing privacy bit
         */
        if (IEEE80211_VAP_IS_DROP_UNENC(vap)) {
            if (!privacy) {
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCANENTRY, "%s", " - Reject DROP_UNENC set (Privacy bit clear)\n");
                return false;
            }
        }
    } else {
        /*
         * We don't allow Secure/non-Secure mixed BSS
         */
        if (!privacy) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCANENTRY, "%s", " - Reject (Privacy bit clear)\n");
            return false;
        }
    }
    
    /*
     * Check RSNA (WPA2) or WPA 
     */

#if ATH_SUPPORT_WAPI
    /*
     * Check WAPI first since we pass the wapi_ie via wpa_ie.
     */
    if (RSN_AUTH_IS_WAI(my_rsn)) {

        if (wpa_ie== NULL) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCANENTRY, "%s", " - Reject (AP not WAPI enabled)\n");
            return false;
        }
        
        status=ieee80211_parse_wapi(vap, wpa_ie, &rsn_parms);
        if (status != IEEE80211_STATUS_SUCCESS) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCANENTRY, "%s", " - Reject (AP contains invalid WAPI IE)\n");
            return false;
        }
        bIsAcceptable = ieee80211_match_rsn_info(vap, &rsn_parms);
    }
    else 
#endif
    if (RSN_AUTH_IS_RSNA(my_rsn)) {
        if (rsn_ie == NULL) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCANENTRY, "%s", " - Reject (AP not RSNA enabled)\n");
            return false;
        }

        status = ieee80211_parse_rsn(vap, rsn_ie, &rsn_parms);
        if (status != IEEE80211_STATUS_SUCCESS) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCANENTRY, "%s", " - Reject (AP contains invalid RSN IE)\n");
            return false;
        }
        bIsAcceptable = ieee80211_match_rsn_info(vap, &rsn_parms);
    } else if (RSN_AUTH_IS_WPA(my_rsn)) {
        if (wpa_ie == NULL) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCANENTRY, "%s", " - Reject (AP not WPA enabled)\n");
            return false;
        }

        status = ieee80211_parse_wpa(vap, wpa_ie, &rsn_parms);
        if (status != IEEE80211_STATUS_SUCCESS) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCANENTRY, "%s", " - Reject (AP contains invalid WPA IE)\n");
            return false;
        }
        bIsAcceptable = ieee80211_match_rsn_info(vap, &rsn_parms);
    } else if (RSN_AUTH_IS_CCKM(my_rsn)) {
        /* 
         * CCKM selector may be contained in either WPA or RSN IE.
         */
        if (rsn_ie != NULL) {
            status = ieee80211_parse_rsn(vap, rsn_ie, &rsn_parms);
            if (status == IEEE80211_STATUS_SUCCESS) {
                bIsAcceptable = ieee80211_match_rsn_info(vap, &rsn_parms);
            }
        }

        /* if a RSN IE was not there, or what was in it didn't match, check the WPA IE */
        if (rsn_ie == NULL || status != IEEE80211_STATUS_SUCCESS || bIsAcceptable == false) {
            if (wpa_ie != NULL) {
                status = ieee80211_parse_wpa(vap, wpa_ie, &rsn_parms);
                if (status == IEEE80211_STATUS_SUCCESS) {
                    bIsAcceptable = ieee80211_match_rsn_info(vap, &rsn_parms);
                }
            }
        }
    } else {
        bIsAcceptable = true;
    }

    return bIsAcceptable;
}
#else
static bool
ieee80211_candidate_aplist_match_security(
			wlan_if_t    vap,
			u_int8_t     privacy,       /* AP is privacy enabled */
			u_int8_t     *rsn_ie,
			u_int8_t     *wpa_ie){
	bool bIsAcceptable = false;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint32_t authmode;
	struct wlan_crypto_params crypto_params;

	if(vap->iv_wps_mode) {
		return true;
	}

	authmode = wlan_crypto_get_param(vap->vdev_obj, WLAN_CRYPTO_PARAM_AUTH_MODE);

	/*
	 * Privacy bit
	 */
	if (!IEEE80211_VAP_IS_PRIVACY_ENABLED(vap)) {
		if (privacy) {
			return false;
		}
	} else {
	/*
	 * We don't allow Secure/non-Secure mixed BSS
	 */
		if (!privacy) {
			return false;
		}
	}
        if (authmode & (1 << IEEE80211_AUTH_WAPI)) {
            if(!wpa_ie)
		return false;
            status = wlan_crypto_wapiie_check((struct wlan_crypto_params *)&crypto_params, wpa_ie);
                if (status != QDF_STATUS_SUCCESS) {
                        return false;
                }
                bIsAcceptable = wlan_crypto_rsn_info(vap->vdev_obj, (struct wlan_crypto_params *)&crypto_params);
	} else if (authmode & (1 << IEEE80211_AUTH_RSNA)) {
		if (rsn_ie == NULL) {
			return false;
		}

		status = wlan_crypto_rsnie_check((struct wlan_crypto_params *)&crypto_params,
							rsn_ie);
		if (status != QDF_STATUS_SUCCESS) {
			return false;
		}

		bIsAcceptable = wlan_crypto_rsn_info(vap->vdev_obj, &crypto_params);
	} else if (authmode & (1 << IEEE80211_AUTH_WPA)) {
		if (wpa_ie == NULL) {
			return false;
		}
		status = wlan_crypto_wpaie_check((struct wlan_crypto_params *)&crypto_params, wpa_ie);
		if (status != QDF_STATUS_SUCCESS) {
			return false;
		}
		bIsAcceptable = wlan_crypto_rsn_info(vap->vdev_obj, (struct wlan_crypto_params *)&crypto_params);
	} else {
            bIsAcceptable = true;
        }
	return bIsAcceptable;
}

#endif
static void
ieee80211_build_rate_set(
    struct ieee80211_ie_rates   *standard_rates,
    struct ieee80211_ie_xrates  *extended_rates,
    struct ieee80211_rateset    *merged_rate_set
    )
{
    /* Copy standard rates to resulting rate set */
    merged_rate_set->rs_nrates = standard_rates->rate_len;
    OS_MEMCPY(merged_rate_set->rs_rates, standard_rates->rate, standard_rates->rate_len);

    /* 
     * Copy extended rates to resulting rate set, paying attention to
     * array boundaries.
     */
    if (extended_rates != NULL) {
        merged_rate_set->rs_nrates += extended_rates->xrate_len;
        if (merged_rate_set->rs_nrates > IEEE80211_RATE_MAXSIZE) {
            merged_rate_set->rs_nrates = IEEE80211_RATE_MAXSIZE; 
        }
        OS_MEMCPY(&(merged_rate_set->rs_rates[standard_rates->rate_len]), 
                  extended_rates->xrate, 
                  merged_rate_set->rs_nrates - standard_rates->rate_len);
    }
}

static bool
ieee80211_candidate_aplist_match_ssid(
    wlan_if_t                         vaphandle,
    struct ieee80211_aplist_config    *pconfig,
    u_int8_t                          *candidate_ssid,
    int                               candidate_ssid_len,
    u_int8_t                          *mac_address,
    u_int8_t                          *bssid
    )
{
    int               status = EOK;
    bool              bIsAcceptable = true;  /* Default accept */
    int               i;
    ieee80211_ssid    *des_ssid;

    do {
        status = ieee80211_get_desired_ssid(vaphandle, 0, &des_ssid);
        if (status != EOK) {
            bIsAcceptable = false;
            break;
        }


        /*
         * Zero length desired SSID is the wildcard SSID. That would match any AP and
         * we don't need to compare.
         */
        if (des_ssid->len != 0) {
            /* 
             * Each different SSID has its own BSS entry, so the SSID
             * field in the BSS entry is the real SSID. No need to 
             * compare with ProbeSSID.
             */
            if (candidate_ssid_len <= 0 || candidate_ssid_len > IEEE80211_NWID_LEN ||
                    candidate_ssid == NULL) {
                /* SSID not available. Reject AP */
                IEEE80211_DPRINTF(vaphandle, IEEE80211_MSG_SCANENTRY, "%s", "- Reject (hidden SSID)"
                        "\n candidate_ssid_len = %d\n candidate_ssid = %pK\n",
                        candidate_ssid_len, candidate_ssid);
                bIsAcceptable = false;
                break;
            }

            /* Check that SSID matches (Note: case sensitive comparison) */
            if ((candidate_ssid_len != des_ssid->len) ||
                OS_MEMCMP(candidate_ssid, des_ssid->ssid, candidate_ssid_len)) {
                IEEE80211_DPRINTF(vaphandle, IEEE80211_MSG_SCANENTRY, "%s", " - Reject (mismatched SSID)\n");
                bIsAcceptable = false;
                break;
            }
        }
        
        /* Check the excluded MAC address list */
        if (ieee80211_aplist_get_ignore_all_mac_addresses(pconfig)) {
            IEEE80211_DPRINTF(vaphandle, IEEE80211_MSG_SCANENTRY, "%s", " - Reject (Ignore all MAC addresses)\n");
            bIsAcceptable = false;
            break;
        }
        else {
            bIsAcceptable = true;        

            /* Walk through the excluded MAC address list */
            for (i = 0; i < ieee80211_aplist_get_exc_macaddress_count(pconfig); i++) {
                u_int8_t    *macaddress;

                if (ieee80211_aplist_get_exc_macaddress(pconfig, i, &macaddress) != EOK) {
                    /* 
                     * Could't retrieve the excluded macaddress. Consider this
                     * a minor error and don't reject the entry.
                     */
                    break;
                }

                if (IEEE80211_ADDR_EQ(mac_address, macaddress) == true) {
                    bIsAcceptable = false;
                    break;
                }
            }

            if (bIsAcceptable == false) {
                IEEE80211_DPRINTF(vaphandle, IEEE80211_MSG_SCANENTRY, "%s", " - Reject (Excluded MAC addresses)\n");
                break;
            }
        }

        /* Check the desired BSSID list */
        if (! ieee80211_aplist_get_accept_any_bssid(pconfig)) {
            u_int8_t    *desired_bssid;

            bIsAcceptable = false;        

            /* Walk through the desired BSSID list */
            for (i = 0; i < ieee80211_aplist_get_desired_bssid_count(pconfig); i++) {
                if (ieee80211_aplist_get_desired_bssid(pconfig, i, &desired_bssid) != EOK) {
                    /* Failed to retried a desired SSID */
                    break;
                }

                if (IEEE80211_ADDR_EQ(bssid, desired_bssid) == true) {
                    /* This is an acceptable BSSID */
                    bIsAcceptable = true;
                    break;
                }
            }

            if (bIsAcceptable == false) {
                IEEE80211_DPRINTF(vaphandle, IEEE80211_MSG_SCANENTRY, "%s", " - Reject (Not in desired BSSID list)\n");
                break;
            }
        }
        
        /* The SSID matches */
        bIsAcceptable = true;
        
    } while (false);

    return bIsAcceptable;
}

/*
 * AP entry must be locked against change
 */
static u_int8_t
ieee80211_candidate_aplist_match_entry(
    wlan_scan_entry_t    scan_entry,
    wlan_if_t            vaphandle,
    u_int32_t            maximum_age,
    bool                 strict_filtering,
    bool                 calculate_rank,
    wlan_scan_entry_t    active_ap
    )
{
    struct ieee80211_aplist_config    *pconfig  = ieee80211_vap_get_aplist_config(vaphandle);
    u_int8_t                          *ssid = NULL;
    u_int8_t                          ssid_len = 0;
    u_int8_t                          *mac_address = util_scan_entry_macaddr(scan_entry);
    wlan_chan_t                       channel      = wlan_util_scan_entry_channel(scan_entry);
    enum ieee80211_phymode            phy_mode     = ieee80211_chan2mode(channel);
    bool                              support_11g  = false;
    bool                              support_11a  = false;
    u_int32_t                         rank, age;
    int                               is_ht = 0;
    wlan_if_t                         tmpvap;

    IEEE80211_DPRINTF(vaphandle, IEEE80211_MSG_SCANENTRY, "%s: %02X:%02X:%02X:%02X:%02X:%02X PhyMode=%08X Ch=%3d RSSI=%2d",
        __func__,
        mac_address[0], mac_address[1], mac_address[2], 
        mac_address[3], mac_address[4], mac_address[5],
        phy_mode,
        wlan_channel_ieee(channel),
        util_scan_entry_rssi(scan_entry));

    /* reset this entry's rank */
    rank = 0;

    /*
     * Ignore stale entries. We do periodic scanning, so it
     * an AP is not reasonably fresh, we don't accept it
     */
    age = util_scan_entry_age(scan_entry);
    if (age >= maximum_age) {
        IEEE80211_DPRINTF(vaphandle, IEEE80211_MSG_SCANENTRY, "%s Age %d, MaxAge %d\n", 
                          " - Reject (Old Entry)", age, maximum_age);
        return false;
    }

    /* Skip the AP that is marked as a bad AP */
    if (wlan_util_scan_entry_mlme_status(scan_entry) & AP_STATE_BAD) {
#if AR900B_IP1_EMU_HACK || QCA_WIFI_QCA8074_VP
        if ((!pconfig->bad_ap_timeout) || (CONVERT_SYSTEM_TIME_TO_MS(OS_GET_TIMESTAMP() - wlan_util_scan_entry_mlme_bad_ap_time(scan_entry)) >
             pconfig->bad_ap_timeout))
#else
        if (CONVERT_SYSTEM_TIME_TO_MS(OS_GET_TIMESTAMP() - wlan_util_scan_entry_mlme_bad_ap_time(scan_entry)) >
             pconfig->bad_ap_timeout)

#endif
        {
            wlan_util_scan_entry_mlme_set_bad_ap_time(scan_entry, 0);
            wlan_util_scan_entry_mlme_set_status(scan_entry, AP_STATE_GOOD);
            IEEE80211_DPRINTF(vaphandle, IEEE80211_MSG_SCANENTRY, "%s", " - Bad AP status Expired\n");
        } else {
            IEEE80211_DPRINTF(vaphandle, IEEE80211_MSG_SCANENTRY, "%s", " - Reject Marked Bad AP\n");
            return false;
        }
    }

    if (IEEE80211_IS_CHAN_RADAR(channel)) {
        IEEE80211_DPRINTF(vaphandle, IEEE80211_MSG_SCANENTRY,
                          "%s", " - Reject Marked radar detected AP\n");
        return false;
    }

    /* Give an advantage to 11a */
    rank += ((phy_mode == IEEE80211_MODE_11A)            ||
             (phy_mode == IEEE80211_MODE_11NA_HT20)      ||
             (phy_mode == IEEE80211_MODE_11NA_HT40PLUS)  ||
             (phy_mode == IEEE80211_MODE_11NA_HT40MINUS) ||
             (phy_mode == IEEE80211_MODE_11AC_VHT20)     ||
             (phy_mode == IEEE80211_MODE_11AC_VHT40PLUS) ||
             (phy_mode == IEEE80211_MODE_11AC_VHT40MINUS)||             
             (phy_mode == IEEE80211_MODE_11AC_VHT80)) ? 10 : 0;

    /* BSS Type */
    {
        enum ieee80211_opmode    candidate_bss_type = wlan_util_scan_entry_bss_type(scan_entry);
        enum ieee80211_opmode    desired_bss_type   = ieee80211_aplist_get_desired_bsstype(pconfig);

        if (((desired_bss_type == IEEE80211_M_STA)  && (candidate_bss_type != IEEE80211_M_STA)) ||
            ((desired_bss_type == IEEE80211_M_IBSS) && (candidate_bss_type != IEEE80211_M_IBSS))) {
            IEEE80211_DPRINTF(vaphandle, IEEE80211_MSG_SCANENTRY, "%s", " - Reject (mismatched BSS)\n");
            return false;
        }
    }

    /* Increase the rank for each passed test */
    rank++;

    /* Check AP */
    ssid = util_scan_entry_ssid(scan_entry)->ssid;
    ssid_len = util_scan_entry_ssid(scan_entry)->length;

    if (!ssid_len || ieee80211_candidate_aplist_match_ssid(vaphandle,
                                              pconfig,
                                              ssid,
                                              ssid_len,
                                              mac_address,
                                              util_scan_entry_bssid(scan_entry)) == false) {
        return false;
    }

    /* Increase the rank for each passed test */
    rank++;

    if(phy_mode == IEEE80211_MODE_11NG_HT20 || \
       phy_mode == IEEE80211_MODE_11NG_HT40MINUS || \
       phy_mode == IEEE80211_MODE_11NG_HT40PLUS)
        support_11g = true;
    if(phy_mode == IEEE80211_MODE_11NA_HT20 || \
       phy_mode == IEEE80211_MODE_11NA_HT40MINUS || \
       phy_mode == IEEE80211_MODE_11NA_HT40PLUS || \
       phy_mode == IEEE80211_MODE_11AC_VHT20 || \
       phy_mode == IEEE80211_MODE_11AC_VHT40PLUS || \
       phy_mode == IEEE80211_MODE_11AC_VHT40MINUS || \
       phy_mode == IEEE80211_MODE_11AC_VHT80)
        support_11a = true;

    /* Check PHY type */
    ASSERT(phy_mode != IEEE80211_MODE_AUTO);
    if (! (IEEE80211_ACCEPT_ANY_PHY_MODE(vaphandle) ||
           IEEE80211_ACCEPT_PHY_MODE(vaphandle, phy_mode))) {
        /* Check if vap support 11a/g, when AP support 11a/g */
        if(! ((support_11g && IEEE80211_ACCEPT_PHY_MODE_11G(vaphandle)) || \
              (support_11a && IEEE80211_ACCEPT_PHY_MODE_11A(vaphandle)))){
            IEEE80211_DPRINTF(vaphandle, IEEE80211_MSG_SCANENTRY, "%s", " - Reject (Not in desired PHY list)\n");
            return false;
		}
    }

    /* Increase the rank for each passed test */
    rank++;

    /* Match data rates */
    {
        struct ieee80211_ie_rates    *standard_rates_ie;
        struct ieee80211_ie_xrates   *extended_rates_ie;
        struct ieee80211_rateset     merged_rate_set;

        /* Retrieve standard and extended supported rates */
        standard_rates_ie = (struct ieee80211_ie_rates *)  util_scan_entry_rates(scan_entry);
        extended_rates_ie = (struct ieee80211_ie_xrates *) util_scan_entry_xrates(scan_entry);

        ieee80211_build_rate_set(standard_rates_ie, 
                                 extended_rates_ie,
                                 &merged_rate_set);
        if(util_scan_entry_htcap(scan_entry))
        {
            is_ht = 1;
        }
        if (!ieee80211_check_rate(vaphandle, channel, &merged_rate_set,is_ht)) {
            IEEE80211_DPRINTF(vaphandle, IEEE80211_MSG_SCANENTRY, "%s", " - Reject (Mismatched data rate)\n");

            return false;
        }
    }

    /* Increase the rank for each passed test */
    rank++;

    /* check with custom security match function if registred */
    if (pconfig->match_security_func) {
        if (pconfig->match_security_func(pconfig->match_security_func_arg, scan_entry) == false) {
            IEEE80211_DPRINTF(vaphandle, IEEE80211_MSG_SCANENTRY, "%s", " - Reject (Mismatched security from custom security check)\n");
            return false;
        }
    }  else {

        /* Match WEP/WPA/WPA2 capabilities */
        if (ieee80211_candidate_aplist_match_security(
                vaphandle,
                util_scan_entry_privacy(scan_entry),
                util_scan_entry_rsn(scan_entry),
                util_scan_entry_wpa(scan_entry)) == false  
#if ATH_SUPPORT_WAPI
            &&
            ieee80211_candidate_aplist_match_security(
                vaphandle,
                util_scan_entry_privacy(scan_entry),
                util_scan_entry_rsn(scan_entry),
                util_scan_entry_wapi(scan_entry)) == false
#endif
           )
        {
            IEEE80211_DPRINTF(vaphandle, IEEE80211_MSG_SCANENTRY, "%s", " - Reject (Mismatched security)\n");
            return false;
        }
    }

    /*
     * Give chance to CCX to validate bss
     */
    if (vaphandle->iv_ccx_evtable && vaphandle->iv_ccx_evtable->wlan_ccx_validate_bss &&
       !vaphandle->iv_ccx_evtable->wlan_ccx_validate_bss(vaphandle->iv_ccx_arg,
                                                         scan_entry,
                                                         active_ap ?
                                                         util_scan_entry_rssi(active_ap) : 0)){
        IEEE80211_DPRINTF(vaphandle, IEEE80211_MSG_SCANENTRY, "%s", " - Reject (CCX Bss Verification Failed)\n");
        return false;
    }

    /* Skip scan entries whose mac address is same as some of our
     * vaps mac address.
     */
    TAILQ_FOREACH(tmpvap, &(vaphandle->iv_ic)->ic_vaps, iv_next) {
        if (IEEE80211_ADDR_EQ(mac_address, tmpvap->iv_myaddr)) {
            IEEE80211_DPRINTF(vaphandle, IEEE80211_MSG_SCANENTRY,
                "%s: - Reject BSSID matches with vap %d's mac: %s\n",
                __func__, tmpvap->iv_unit, ether_sprintf(mac_address));
            return false;
        }
    }

    if (calculate_rank) {
        /* Increase the rank for each passed test */
        rank++;

        wlan_util_scan_entry_mlme_set_rank(scan_entry, rank);

        IEEE80211_DPRINTF(vaphandle, IEEE80211_MSG_SCANENTRY, " - Accepted, RSSI %2d\n", util_scan_entry_rssi(scan_entry));
    
        /* Found a candidate; calculate rank and utility */
        ieee80211_calculate_bss_rank(vaphandle,
                                     scan_entry,
                                     active_ap);
    }

    /* We can use this AP */
    return true;
}

static QDF_STATUS
ieee80211_candidate_aplist_add_entry(
   void                 *arg, 
   wlan_scan_entry_t    scan_entry
   )
{
    struct candidate_ap_parameters    *pcandidate_ap_parameters = arg;
    wlan_scan_entry_t scan_entry_copy = NULL;
#if ATH_SUPPORT_WRAP
    wlan_if_t vap = pcandidate_ap_parameters->vaphandle;
    wlan_if_t stavap = vap->iv_ic->ic_sta_vap;
#endif
    u_int8_t add_scanentry =0;

    if (ieee80211_candidate_aplist_match_entry(scan_entry,
                                               pcandidate_ap_parameters->vaphandle,
                                               pcandidate_ap_parameters->maximum_age,
                                               pcandidate_ap_parameters->strict_filtering,
                                               pcandidate_ap_parameters->calculate_rank,
                                               pcandidate_ap_parameters->active_ap)) {
        if (pcandidate_ap_parameters->candidate_ap_list) {
            if (pcandidate_ap_parameters->candidate_ap_entries < pcandidate_ap_parameters->maximum_candidate_ap_length) {
#if ATH_SUPPORT_WRAP
                if (vap->iv_psta && !vap->iv_mpsta) {
                    if (stavap && stavap->iv_bss) {
                        /* For PSTA , copy the scan entry, only which matches with MPSTA VAP BSSID*/
                        if (OS_MEMCMP(stavap->iv_bss->ni_macaddr, util_scan_entry_bssid(scan_entry),6) == 0) {
                            add_scanentry = 1;
                        }
                    }
                } else {
                    add_scanentry = 1;
                }
#else
                add_scanentry = 1;
#endif
                if (add_scanentry) {
                    /* Create a copy of this scan entry and store in candidate ap list */
                    scan_entry_copy = util_scan_copy_cache_entry(scan_entry);
                    if (!scan_entry_copy) {
                        qdf_print("%s: unable to copy scan entry", __func__);
                        return EOK;
                    }
                    pcandidate_ap_parameters->candidate_ap_list[pcandidate_ap_parameters->candidate_ap_entries++] = scan_entry_copy;
                }
            }
        }
        else {
            pcandidate_ap_parameters->candidate_ap_entries++;
        }

        /* Check whether we should stop search upon finding exact number of matches */
        if (pcandidate_ap_parameters->stop_on_max_count) {
            if (pcandidate_ap_parameters->candidate_ap_entries >= pcandidate_ap_parameters->maximum_candidate_ap_length) {
                return EOVERFLOW;
            }
        }
    }

    return EOK;
}

/* typedef declaration already present in header file */
struct ieee80211_candidate_aplist {
    wlan_if_t                 vaphandle;
    wlan_dev_t                ic;
    wlan_scan_entry_t         candidate_ap_list[IEEE80211_CANDIDATE_AP_MAX_COUNT];
    u_int32_t                 candidate_ap_count;

    /* custom candidate compare function */
    ieee80211_candidate_list_compare_func   compare_func;
    void                                   *compare_arg; 
};

bool ieee80211_candidate_list_find(
    wlan_if_t            vaphandle,
    bool                 strict_filtering,
    wlan_scan_entry_t    *candidate_list,
    u_int32_t            maximum_age
    )
{
    struct candidate_ap_parameters    candidate_ap_params;

    OS_MEMZERO(&candidate_ap_params, sizeof(candidate_ap_params));
    candidate_ap_params.vaphandle                   = vaphandle;
    candidate_ap_params.strict_filtering            = strict_filtering;
    candidate_ap_params.candidate_ap_list           = candidate_list;
    candidate_ap_params.candidate_ap_entries        = 0;
    candidate_ap_params.maximum_candidate_ap_length = 1;
    candidate_ap_params.stop_on_max_count           = true;
    candidate_ap_params.calculate_rank              = false;
    candidate_ap_params.maximum_age                 = maximum_age;

    ucfg_scan_db_iterate(wlan_vap_get_pdev(vaphandle),
                                 ieee80211_candidate_aplist_add_entry,
                                 &candidate_ap_params);

    return (candidate_ap_params.candidate_ap_entries > 0);
}

void ieee80211_candidate_list_build(
    wlan_if_t            vaphandle,
    bool                 strict_filtering,
    wlan_scan_entry_t    active_ap,
    u_int32_t            maximum_age
    )
{
    ieee80211_candidate_aplist_t      aplist;
    struct candidate_ap_parameters    candidate_ap_params;

    aplist = ieee80211_vap_get_aplist(vaphandle);
    OS_MEMZERO(&candidate_ap_params, sizeof(candidate_ap_params));
    candidate_ap_params.vaphandle                   = vaphandle;
    candidate_ap_params.strict_filtering            = strict_filtering;
    candidate_ap_params.candidate_ap_list           = aplist->candidate_ap_list;
    candidate_ap_params.candidate_ap_entries        = 0;
    candidate_ap_params.maximum_candidate_ap_length = IEEE80211_CANDIDATE_AP_MAX_COUNT;
    candidate_ap_params.stop_on_max_count           = true;
    candidate_ap_params.calculate_rank              = true;
    candidate_ap_params.maximum_age                 = maximum_age;
    candidate_ap_params.active_ap                   = active_ap;

    ucfg_scan_db_iterate(wlan_vap_get_pdev(vaphandle), 
                                 ieee80211_candidate_aplist_add_entry, 
                                 &candidate_ap_params);

    aplist->candidate_ap_count = candidate_ap_params.candidate_ap_entries;

    /*
     * If we are building a list of candidate APs, sort it according
     * to the preference index.
     */
    if (aplist->candidate_ap_count > 0) {
        /*
         * Now reorder APs in our candidate list to have the most
         * preferred AP first
         */
        ieee80211_sort_candidate_ap_list(aplist->vaphandle,
                                         aplist->candidate_ap_list,
                                         aplist->candidate_ap_count,
                                         aplist->compare_func,
                                         aplist->compare_arg);
    }
    else {
        ucfg_scan_db_iterate(wlan_vap_get_pdev(vaphandle),
                                     ieee80211_aplist_clear_bad_ap_flags, 
                                     &candidate_ap_params);
    }
}

/* Opportunistic Roam */
void ieee80211_candidate_list_prioritize_bssid(
    ieee80211_candidate_aplist_t aplist,
    struct ether_addr *bssid
    )
{
    int                  i, j;
    int                  ap_count;
    u_int8_t             *tstBSSID;
    wlan_scan_entry_t    tmp_ap;
    wlan_scan_entry_t    *ap_list;

    ap_list  = aplist->candidate_ap_list;
    ap_count = aplist->candidate_ap_count;
    
    if( ap_count <= 0 ) {
        return;
    }

    for (i = 0; i < ap_count; i++) 
    {
        tstBSSID = util_scan_entry_bssid(ap_list[i]);

        if (memcmp(bssid->octet, tstBSSID, sizeof(bssid->octet)) == 0)
        {
            tmp_ap = ap_list[i];

            for (j = i; j > 0; j--) 
            {
                ap_list[j] = ap_list[j-1];
            }
            ap_list[0] = tmp_ap;
            break;
        }
    }

    ieee80211_process_selected_bss(ap_list[0]);
}

void ieee80211_candidate_list_free(
    ieee80211_candidate_aplist_t    aplist
    )
{
    int    i;

    for (i = 0; i < aplist->candidate_ap_count; i++) {
        /* store current mlme info back to scan module */
        wlan_util_scan_entry_update_mlme_info(aplist->vaphandle,
            aplist->candidate_ap_list[i]);
        /* free scan entry from ap list */
        util_scan_free_cache_entry(aplist->candidate_ap_list[i]);
        aplist->candidate_ap_list[i] = NULL;
    }
    aplist->candidate_ap_count = 0;
}

int ieee80211_candidate_list_count(
    ieee80211_candidate_aplist_t    aplist
    )
{
    return aplist->candidate_ap_count;
}

wlan_scan_entry_t ieee80211_candidate_list_copy_candidate(
    wlan_scan_entry_t    candidate
    )
{
   return util_scan_copy_cache_entry(candidate);
}

void ieee80211_candidate_list_free_copy_candidate(
    wlan_scan_entry_t    copy_candidate
    )
{
    util_scan_free_cache_entry(copy_candidate);
}

wlan_scan_entry_t ieee80211_candidate_list_get(
    ieee80211_candidate_aplist_t    aplist,
    int                             index
    )
{
    if ( (index < aplist->candidate_ap_count) && (index < IEEE80211_CANDIDATE_AP_MAX_COUNT) ) {
        return aplist->candidate_ap_list[index];
    }
    else {
        return NULL;
    }
}

void ieee80211_candidate_list_register_compare_func (
    ieee80211_candidate_aplist_t    aplist,
    ieee80211_candidate_list_compare_func compare_func,
    void *arg
    )
{
        aplist->compare_func = compare_func;
        aplist->compare_arg =arg;
}

int ieee80211_aplist_vattach(
    ieee80211_candidate_aplist_t    *aplist, 
    wlan_if_t                       vaphandle, 
    osdev_t                         osdev
    )
{
    if ((*aplist) != NULL) 
        return EINPROGRESS; /* already attached ? */

    *aplist = (ieee80211_candidate_aplist_t) OS_MALLOC(osdev, (sizeof(struct ieee80211_candidate_aplist)),0);

    if (*aplist != NULL) {
        OS_MEMZERO((*aplist), sizeof(struct ieee80211_candidate_aplist));

        (*aplist)->vaphandle  = vaphandle;

        return EOK;
    }

    return ENOMEM;
}

int ieee80211_aplist_vdetach(
    ieee80211_candidate_aplist_t    *aplist
    )
{
    if ((*aplist) == NULL) 
        return EINPROGRESS; /* already detached ? */

    if ((*aplist)->candidate_ap_count != 0) {
        ieee80211_candidate_list_free(*aplist);
    }

    OS_FREE(*aplist);

    *aplist = NULL;

    return EOK;
}

bool wlan_candidate_list_find(
    wlan_if_t            vaphandle,
    bool                 strict_filtering,
    wlan_scan_entry_t    *candidate_list,
    u_int32_t            maximum_age
    )
{
    /*
     * Use current scan start time when searching for new entry.
     */
    return (ieee80211_candidate_list_find(vaphandle, 
                                          strict_filtering, 
                                          candidate_list,
                                          maximum_age));
}

void wlan_candidate_list_build(
    wlan_if_t            vaphandle,
    bool                 strict_filtering,
    wlan_scan_entry_t    active_ap,
    u_int32_t            maximum_age
    )
{
    ieee80211_candidate_list_build(vaphandle, 
                                   strict_filtering, 
                                   active_ap,
                                   maximum_age);
}

wlan_scan_entry_t wlan_candidate_list_copy_candidate(
    wlan_scan_entry_t    candidate
    )
{
    return ieee80211_candidate_list_copy_candidate(candidate);
}

void wlan_candidate_list_free_copy_candidate(
    wlan_scan_entry_t    copy_candidate
    )
{
    ieee80211_candidate_list_free_copy_candidate(copy_candidate);
}

/* Opportunistic Roam */
void wlan_candidate_list_prioritize_bssid(
    wlan_if_t         vaphandle, 
    struct ether_addr *bssid
    )
{
    ieee80211_candidate_list_prioritize_bssid(ieee80211_vap_get_aplist(vaphandle), bssid);
}

void wlan_candidate_list_free(
    wlan_if_t    vaphandle
    )
{
   ieee80211_candidate_list_free(ieee80211_vap_get_aplist(vaphandle));
}

int wlan_candidate_list_count(
    wlan_if_t    vaphandle
    )
{
    return ieee80211_candidate_list_count(ieee80211_vap_get_aplist(vaphandle));
}

wlan_scan_entry_t wlan_candidate_list_get(
    wlan_if_t    vaphandle, 
    int          index
    )
{
    return ieee80211_candidate_list_get(ieee80211_vap_get_aplist(vaphandle), index);
}

void wlan_candidate_list_register_compare_func(wlan_if_t vaphandle, ieee80211_candidate_list_compare_func compare_func, void *arg)
{
    ieee80211_candidate_list_register_compare_func(ieee80211_vap_get_aplist(vaphandle), compare_func,arg);
}

#endif

