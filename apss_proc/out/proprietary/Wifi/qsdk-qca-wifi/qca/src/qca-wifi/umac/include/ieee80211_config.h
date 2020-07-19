/*
* Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 *  Copyright (c) 2008 Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */

#ifndef _NET80211_IEEE80211_CONFIG_H
#define _NET80211_IEEE80211_CONFIG_H

#include <osdep.h>

/*
 * WMM Power Save (a.k.a U-APSD) Configuration Parameters
 */
typedef struct _ieee80211_wmmPowerSaveConfig {
    u_int8_t    vo;    /* true=U-APSD for AC_VO, false=legacy power save */
    u_int8_t    vi;    /* true=U-APSD for AC_VI, false=legacy power save */
    u_int8_t    bk;    /* true=U-APSD for AC_BK, false=legacy power save */
    u_int8_t    be;    /* true=U-APSD for AC_BE, false=legacy power save */
} IEEE80211_WMMPOWERSAVE_CFG;

/* Contents must be visible to other layers. */
typedef struct ieee80211_reg_parameters {
    u_int8_t        rateCtrlEnable;            /* enable rate control */
    u_int8_t        rc_txrate_fast_drop_en;    /* enable tx rate fast drop*/
    u_int32_t       transmitRateSet;           /* User specified rate set */
    u_int32_t       transmitRetrySet;          /* User specified retries */
    u_int16_t       userFragThreshold;         /* User-selected fragmentation threshold */
    u_int8_t        shortPreamble;             /* Allow short and long preamble in 11b mode */
    u_int16_t       adhocBand;                 /* User's choice of a, b, turbo, g, etc. to start adhoc */
    u_int8_t        capLinkSp;                 /* Cap link speed at 81 Mbps for reporting in non-turbo country */
    u_int32_t       scanTimePreSleep;          /* Time to continue scanning before sleeping (in s) */
    u_int32_t       sleepTimePostScan;         /* Time to sleep before starting to scan again (in s) */
    u_int8_t        indicateRxLinkSpeed;       /* Indicate Rx linkspeed to OS */	
    u_int16_t       sleepTimePwrSaveMax;       /* station wakes after this many mS in max power save mode */
    u_int16_t       sleepTimePwrSave;          /* station wakes after this many mS in normal power save mode */
    u_int16_t       sleepTimePerf;             /* station wakes after this many mS in max performance mode */
    u_int32_t       inactivityTimePwrSaveMax;  /* in max PS mode, how long (in mS) w/o Tx/Rx before going back to sleep */
    u_int32_t       inactivityTimePwrSave;     /* in normal PS mode, how long (in mS) w/o Tx/Rx before going back to sleep */
    u_int32_t       inactivityTimePerf;        /* in max perf mode, how long (in mS) w/o Tx/Rx before going back to sleep */
    u_int8_t        overrideACstatus;          /* ignore PnP AC event notification if TRUE */
    u_int8_t        translateNullSsid;         /* translate null ssid string to a string of nulls. */
    u_int32_t       regCapBits;                /* Regulatory capabilities mask */
    u_int8_t        defaultIbssChannel;        /* Default channel to create an IBSS network */
    u_int8_t        ignore11dBeacon;           /* Ignore 11d beacon flag */
    u_int8_t        disallowAutoCCchange;      /* If set, disallow country code change after assoc */
    u_int8_t        htDupIeEnable;             /* Enable duplicate HT IE (official IE) */
    u_int8_t        htVendorIeEnable;          /* Enable Vendor HT IE */
    u_int8_t        cwmMode;                   /* CWM mode */
    int8_t          cwmExtOffset;              /* CWM Extension Channel Offset */
    u_int8_t        cwmExtProtMode;            /* CWM Extension Channel Protection Mode */
    u_int8_t        cwmExtProtSpacing;         /* CWM Extension Channel Protection Spacing */
    u_int8_t        enable2GHzHt40Cap;         /* Enable Ht40 support in 2GHz channels */
    u_int8_t        cwmEnable;                 /* CWM enable state machine */
    u_int8_t        cwmExtBusyThreshold;       /* CWM Extension Channel Busy Threshold */
    u_int8_t        cwmIgnoreExtCCA;           /* CWM Ignore Ext channel CCA */
    u_int8_t        abolt;                     /* setting to control SuperG+XR features */
#ifdef ATH_COALESCING    
    u_int8_t        txCoalescingEnable;        /* Enable Tx Coalescing */
#endif    
    u_int8_t        wmeEnabled;                /* Enable WMM */
    u_int8_t        psPollEnabled;             /* Use PS-POLL to retrieve data frames after TIM is received */
    u_int8_t        extapUapsdEnable;          /* UAPSD enabled on SoftAP ports */
    u_int8_t        p2pGoUapsdEnable;          /* UAPSD enabled on P2P GO ports */

    IEEE80211_WMMPOWERSAVE_CFG    uapsd;       /* U-APSD Settings */
    u_int8_t        htEnableWepTkip;           /* Enable ht rate for WEP and TKIP */
    u_int8_t        smpsDynamic;               /* Enable SM Dynamic power save */
    u_int8_t        smpsRssiThreshold;         /* RSSI threshold above which SM power save is done */
    u_int8_t        smpsDataThreshold;         /* Data throughput (Mbps) below which SM power save is done */
    u_int8_t        ht20AdhocEnable;           /* Enable HT20 rates for Ad Hoc connections */
    u_int8_t        ht40AdhocEnable;           /* Enable HT40 rates for Ad Hoc connections */
    u_int8_t        htAdhocAggrEnable;         /* Enable aggregation with HT20 Ad Hoc connections */
    u_int8_t        disable2040Coexist;        /* Enable 20/40 coexistence */
    u_int8_t        ignoreDynamicHalt;         /* ignore dynamic halt */
    u_int8_t        ampTxopLimit;              /* Overload txop limit for AMP */
    u_int8_t        ampIgnoreNonERP;           /* Ignore ERP IE to disable CTS for BTAMP*/
    u_int8_t        resmgrSyncSm;              /* run resource manager state machine synchronously */
    u_int8_t        resmgrLowPriority;            /* run resource manager state machine at low priority */
    u_int8_t        noP2PDevMacPrealloc;       /* do not preallocate p2p device mac address */
#ifdef ATH_SUPPORT_TxBF
    u_int8_t        autocvupdate;              /* enable auto CV update*/
    u_int8_t        cvupdateper;               /* per threshould to initial cvupdate*/
#endif
    u_int32_t            regdmn;                          /* Regulatory domain code for private test */
    char                 countryName[4];                  /* country name */
    u_int32_t            wModeSelect;                     /* Software limiting of device capability for SKU reduction */
    u_int32_t            netBand;                         /* User's choice of a, b, turbo, g, etc. from registry */
    u_int32_t            extendedChanMode; 
} IEEE80211_REG_PARAMETERS, *PIEEE80211_REG_PARAMETERS;

int ieee80211_get_desired_ssidlist(struct ieee80211vap *vap, ieee80211_ssid *ssidlist, int nssid);
int ieee80211_get_desired_ssid(struct ieee80211vap *vap, int index, ieee80211_ssid **ssid);

#endif /* _NET80211_IEEE80211_CONFIG_H */
