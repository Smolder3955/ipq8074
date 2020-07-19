// vim: set et sw=4 sts=4 cindent:
/*
 * @File: wlanifBSteerControlCmn.c
 *
 * @Abstract: Load balancing daemon band steering control interface
 *
 * @Notes:
 *
 * @@-COPYRIGHT-START-@@
 *
 * Copyright (c) 2014-2018 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * 2014-2016 Qualcomm Atheros, Inc.
 *
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * @@-COPYRIGHT-END-@@
 *
 */
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>

#include <net/if.h>
#include <sys/types.h>
#define _LINUX_IF_H /* Avoid redefinition of stuff */
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/types.h>
#include <linux/wireless.h>
#include <net/if_arp.h>   // for ARPHRD_ETHER
#include <errno.h>
#include <limits.h>
#include <sys/sem.h>
#include <sys/ipc.h>


#ifdef GMOCK_UNIT_TESTS
#include "strlcpy.h"
#endif

#include <split.h>

#include <diaglog.h>

#include "profile.h"
#include "module.h"
#include "lb_common.h"
#include "lb_assert.h"

#include "wlanifPrivate.h"
#include "wlanifBSteerControlCmn.h"
#include "wlanifBSteerEvents.h"

/* QCA header included only for non-SONLIB case */
#if !SONLIB_SUPPORT_ENABLED
#include "ieee80211_external.h"
#endif

#include "wlanif_cmn.h"
#include "sonlib_cmn.h"

struct wlanif_config *wlanIfLb = NULL;
static int isCfg80211;
static u_int8_t gnlSteerMulticast = 0;

// forward decls
static LBD_BOOL wlanifBSteerControlCmnIsBandValid(wlanifBSteerControlHandle_t state, wlanif_band_e band);

static struct wlanifBSteerControlRadioInfo *wlanifBSteerControlCmnLookupRadioByIfname(
        wlanifBSteerControlHandle_t state, const char *ifname);

static LBD_BOOL wlanifBSteerControlAllVAPIoctlsResolved(
        const struct wlanifBSteerControlRadioInfo *radio);
static LBD_STATUS wlanifBSteerControlCmnResolvePrivateIoctls(
        wlanifBSteerControlHandle_t state, const char *ifname,
        LBD_BOOL radioLevel, struct wlanifBSteerControlRadioInfo *radio);
static void wlanifBSteerControlCmnFindPrivateIoctls(
        wlanifBSteerControlHandle_t state,
        struct wlanifBSteerControlRadioInfo *radio,
        LBD_BOOL radioLevel, size_t numIoctls,
        struct iw_priv_args *privArgs);

static struct wlanifBSteerControlVapInfo *
wlanifBSteerControlCmnAllocateVap(wlanifBSteerControlHandle_t state, wlanif_band_e band);
static struct wlanifBSteerControlVapInfo * wlanifBSteerControlCmnInitVapFromIfname(
        wlanifBSteerControlHandle_t state, const char *ifname,
        struct wlanifBSteerControlRadioInfo *radio, LBD_BOOL includeFlag);
static struct wlanifBSteerControlVapInfo *wlanifBSteerControlCmnExtractVapHandle(
        const lbd_bssInfo_t *bss);
static LBD_STATUS wlanifBSteerControlCmnDisableNoDebug(wlanifBSteerControlHandle_t state,
                                                       const struct wlanifBSteerControlRadioInfo *radio);
static LBD_STATUS wlanifBSteerControlCmnEnableNoDebug(wlanifBSteerControlHandle_t state,
                                                      const struct wlanifBSteerControlRadioInfo *radio);

static LBD_STATUS wlanifBSteerControlCmnGetSSID(
    wlanifBSteerControlHandle_t state, const char *ifname,
    struct wlanifBSteerControlVapInfo *vap);

static LBD_STATUS wlanifBSteerControlCmnResolveWlanIfaces(
    wlanifBSteerControlHandle_t state, LBD_BOOL allowZeroAPIfaces, LBD_BOOL includedVaps);

static LBD_STATUS wlanifBSteerControlCmnInitializeACLs(
        wlanifBSteerControlHandle_t handle, wlanif_band_e band);
static void wlanifBSteerControlCmnFlushACLs(
        wlanifBSteerControlHandle_t handle, wlanif_band_e band);
static void wlanifBSteerControlCmnTeardownACLs(
        wlanifBSteerControlHandle_t handle, wlanif_band_e band);

static LBD_STATUS wlanifBSteerControlCmnReadConfig(
        wlanifBSteerControlHandle_t state, wlanif_band_e band);

static LBD_STATUS wlanifBSteerControlCmnSetSON(
        wlanifBSteerControlHandle_t handle, wlanif_band_e band,
        LBD_BOOL sonEnabled);

static LBD_STATUS wlanifBSteerControlCmnSendEnable(
        wlanifBSteerControlHandle_t state, wlanif_band_e band, LBD_BOOL enable,
        LBD_BOOL ignoreError);
static LBD_STATUS wlanifBSteerControlCmnSendSetParams(wlanifBSteerControlHandle_t state, wlanif_band_e band);
static LBD_STATUS wlanifBSteerControlCmnSendRequestRSSI(
        wlanifBSteerControlHandle_t state, struct wlanifBSteerControlVapInfo *vap,
        const struct ether_addr * staAddr, u_int8_t numSamples);
static inline LBD_STATUS wlanifBSteerControlCmnSendFirstVAP(
        wlanifBSteerControlHandle_t state, wlanif_band_e band, u_int8_t cmd,
        const struct ether_addr *destAddr, void *data, int data_len,
        LBD_BOOL ignoreError);

#if !SONLIB_SUPPORT_ENABLED
static LBD_STATUS wlanifBSteerControlCmnSendVAP(
        wlanifBSteerControlHandle_t state, const char *ifname, u_int8_t cmd,
        const struct ether_addr *destAddr, void *data, int data_len,
        void *output, int output_len, LBD_BOOL ignoreError);
static LBD_STATUS wlanifBSteerControlCmnGetSendVAP(
        wlanifBSteerControlHandle_t state, const char *ifname, u_int8_t cmd,
        const struct ether_addr *destAddr, void *output, int output_len);
#endif

static LBD_STATUS wlanifBSteerControlCmnPrivIoctlSetParam(
        wlanifBSteerControlHandle_t state, const char *ifname,
        int paramId, int val);

static LBD_STATUS wlanifBSteerControlCmnResolvePHYCapInfo(wlanifBSteerControlHandle_t state);
static struct wlanifBSteerControlVapInfo *wlanifBSteerControlCmnGetVAPFromSysIndex(
        wlanifBSteerControlHandle_t state, int sysIndex,
        wlanif_band_e indexBand);
static LBD_STATUS wlanifBSteerControlCmnDumpATFTableOneIface(
        wlanifBSteerControlHandle_t state,
        struct wlanifBSteerControlVapInfo *vap,
        wlanif_reservedAirtimeCB callback, void *cookie);
static void wlanifBSteerControlCmnNotifyChanChangeObserver(
        wlanifBSteerControlHandle_t state,
        struct wlanifBSteerControlVapInfo *vap);
static void wlanifBSteerControlCmnLogInterfaceInfo(
        wlanifBSteerControlHandle_t state, struct wlanifBSteerControlVapInfo *vap);

typedef LBD_STATUS (*wlanifBSteerControlCmnNonCandidateCB)(
        wlanifBSteerControlHandle_t state,
        struct wlanifBSteerControlVapInfo *vap,
        void *cookie);
static void wlanifBSteerControlCmnFindStrongestRadioOnBand(
        wlanifBSteerControlHandle_t state, wlanif_band_e band);
static LBD_STATUS wlanifBSteerControlCmnEnableSTAStatsBand(wlanifBSteerControlHandle_t state,
                                                           wlanif_band_e band,
                                                           LBD_BOOL enable);

// ====================================================================
// Internal types
// ====================================================================

/**
 * @brief internal structure for the STA whose RSSI is requested
 */
typedef struct {
    // Double-linked list for use in a given list
    list_head_t listChain;

    // The MAC address of the STA whose RSSI is requested
    struct ether_addr addr;

    // The VAP ths STA is associated with
    struct wlanifBSteerControlVapInfo *vap;

    // number of RSSI samples to average before reporting RSSI back
    u_int8_t numSamples;
} wlanifBSteerControlCmnRSSIRequestEntry_t;

typedef struct wlanifBSteerControlCmnNonCandidateSet_t {
    const struct ether_addr *staAddr;
    LBD_BOOL enable;
    LBD_BOOL probeOnly;
    u_int8_t clientClassGroup;
} wlanifBSteerControlCmnNonCandidateSet_t;

typedef struct wlanifBSteerControlCmnNonCandidateGet_t {
    u_int8_t maxCandidateCount;
    u_int8_t outCandidateCount;
    lbd_bssInfo_t *outCandidateList;
} wlanifBSteerControlCmnNonCandidateGet_t;


struct profileElement wlanifElementDefaultTable_24G[] = {
    {WLANIFBSTEERCONTROL_INACT_IDLE_THRESHOLD,        "10"},
    {WLANIFBSTEERCONTROL_INACT_OVERLOAD_THRESHOLD,    "10"},
    {WLANIFBSTEERCONTROL_INACT_RSSI_XING_HIGH_THRESHOLD, "45"},
    // Not expect RSSI crossing event from idle client when it
    // crosses this inact_low_threshold
    {WLANIFBSTEERCONTROL_INACT_RSSI_XING_LOW_THRESHOLD,  "0"},
    {WLANIFBSTEERCONTROL_LOW_RSSI_XING_THRESHOLD,     "10"},
    // Default value for medium utilization check interval is set to
    // invalid, to avoid ACS report in multi-AP setup. This value must
    // be set through config file in single-AP setup.
    {WLANIFBSTEERCONTROL_MU_CHECK_INTERVAL,           "0"},
    {WLANIFBSTEERCONTROL_MU_AVG_PERIOD,               "60"},
    {WLANIFBSTEERCONTROL_INACT_CHECK_INTERVAL,        "1"},
    {WLANIFBSTEERCONTROL_BCNRPT_ACTIVE_DURATION,      "50"},
    {WLANIFBSTEERCONTROL_BCNRPT_PASSIVE_DURATION,     "200"},
    {WLANIFBSTEERCONTROL_HIGH_TX_RATE_XING_THRESHOLD, "50000"},
    // Note: Low Tx rate / Low rate RSSI crossing thresholds
    // not used for 2.4GHz interface,
    // set to 0, so an event will never be generated.
    {WLANIFBSTEERCONTROL_LOW_TX_RATE_XING_THRESHOLD,  "0"},
    {WLANIFBSTEERCONTROL_LOW_RATE_RSSI_XING_THRESHOLD,"0"},
    {WLANIFBSTEERCONTROL_HIGH_RATE_RSSI_XING_THRESHOLD,"40"},
    // Default value for AP steering threshold is set to invalid, to
    // avoid crossing event in single-AP setup. These values must be
    // set through config file in multi-AP setup.
    {WLANIFBSTEERCONTROL_AP_STEER_LOW_XING_THRESHOLD,  "0"},
    {WLANIFBSTEERCONTROL_INTERFERENCE_DETECTION_ENABLE, "1"},
    {WLANIFBSTEERCONTROL_AUTH_ALLOW,                   "0"},
    // Default value for delaying probe responses in 2.4G band.
    {WLANIFBSTEERCONTROL_DELAY_24G_PROBE_RSSI_THRESHOLD,  "35"},
    {WLANIFBSTEERCONTROL_DELAY_24G_PROBE_TIME_WINDOW,     "0"},
    {WLANIFBSTEERCONTROL_DELAY_24G_PROBE_MIN_REQ_COUNT,   "0"},
    {WLANIFBSTEERCONTROL_BLACKLIST_OTHER_ESS,              "0"},
    {WLANIFBSTEERCONTROL_INACT_DETECTION_FROM_TX,          "0"},
    {WLANIFBSTEERCONTROL_NL_STEER_MULTICAST_ENABLE,        "0"},
    {WLANIFBSTEERCONTROL_ACK_RSSI_ENABLE,                  "0"},
    {WLANIFBSTEERCONTROL_LOG_MOD_COUNTER,                  "1"},
    {NULL,                              NULL},
};

struct profileElement wlanifElementDefaultTable_5G[] = {
    {WLANIFBSTEERCONTROL_INACT_IDLE_THRESHOLD,        "10"},
    {WLANIFBSTEERCONTROL_INACT_OVERLOAD_THRESHOLD,    "10"},
    {WLANIFBSTEERCONTROL_INACT_RSSI_XING_HIGH_THRESHOLD, "30"},
    {WLANIFBSTEERCONTROL_INACT_RSSI_XING_LOW_THRESHOLD,  "0"},
    {WLANIFBSTEERCONTROL_LOW_RSSI_XING_THRESHOLD,     "10"},
    // Default value for medium utilization check interval is set to
    // invalid, to avoid ACS report in multi-AP setup. This value must
    // be set through config file in single-AP setup.
    {WLANIFBSTEERCONTROL_MU_CHECK_INTERVAL,           "0"},
    {WLANIFBSTEERCONTROL_MU_AVG_PERIOD,               "60"},
    {WLANIFBSTEERCONTROL_INACT_CHECK_INTERVAL,        "1"},
    {WLANIFBSTEERCONTROL_BCNRPT_ACTIVE_DURATION,      "50"},
    {WLANIFBSTEERCONTROL_BCNRPT_PASSIVE_DURATION,     "200"},
    // Note: High Tx rate / High  crossing threshold not used for 5GHz interface,
    // set to maximum so an event will never be generated.
    {WLANIFBSTEERCONTROL_HIGH_TX_RATE_XING_THRESHOLD, "4294967295"},
    {WLANIFBSTEERCONTROL_LOW_TX_RATE_XING_THRESHOLD,  "6000"},
    {WLANIFBSTEERCONTROL_LOW_RATE_RSSI_XING_THRESHOLD,"0"},
    {WLANIFBSTEERCONTROL_HIGH_RATE_RSSI_XING_THRESHOLD,"255"},
    {WLANIFBSTEERCONTROL_AP_STEER_LOW_XING_THRESHOLD,  "0"},
    {WLANIFBSTEERCONTROL_INTERFERENCE_DETECTION_ENABLE, "1"},
    {WLANIFBSTEERCONTROL_AUTH_ALLOW,                   "0"},
    // Default value for delaying probe responses in 2.4G band,
    // Not applicable here.
    {WLANIFBSTEERCONTROL_DELAY_24G_PROBE_RSSI_THRESHOLD,  "35"},
    {WLANIFBSTEERCONTROL_DELAY_24G_PROBE_TIME_WINDOW,     "0"},
    {WLANIFBSTEERCONTROL_DELAY_24G_PROBE_MIN_REQ_COUNT,   "0"},
    {WLANIFBSTEERCONTROL_BLACKLIST_OTHER_ESS,              "0"},
    {WLANIFBSTEERCONTROL_INACT_DETECTION_FROM_TX,          "0"},
    {WLANIFBSTEERCONTROL_NL_STEER_MULTICAST_ENABLE,        "0"},
    {WLANIFBSTEERCONTROL_ACK_RSSI_ENABLE,                  "0"},
    {WLANIFBSTEERCONTROL_LOG_MOD_COUNTER,                  "1"},
    {NULL,                              NULL},
};

struct profileElement wlanifElementDefaultTable[] = {
    {WLANIFBSTEERCONTROL_ALLOW_ZERO_AP_INTERFACES,  "0"},
    {NULL,                              NULL},
};

/*========================================================================*/
/*============ Internal handling =========================================*/
/*========================================================================*/

/**
 * @brief Get sem set id if exists else create a new one.
 *
 * @param [in] handle  the handle returned from wlanifBSteerControlCmnCreate()
 *                     to use to disable band steering feature
 *
 * @return LBD_OK on successful get; otherwise LBD_NOK
 */
static LBD_STATUS wlanifBSteerControlCmnLockInit(
        wlanifBSteerControlHandle_t state) {
    int semid;
    key_t key;
    union semun{
        int val;
    } semarg;

    if (state->semid)
        return LBD_OK;

    key = ftok( "/proc/version", 64);
    if (key == -1)
        return LBD_NOK;

    semid = semget(key, 0, 0);
    if (semid == -1)
    {
        semid = semget(key, 1, IPC_CREAT|IPC_EXCL|0666);
        if (semid == -1)
            return LBD_NOK;

        semarg.val = 1;
        if (semctl(semid, 0, SETVAL, semarg) == -1)
        {
            semctl(semid, 0, IPC_RMID, semarg);
            return LBD_NOK;
        }
    }

    state->semid = semid;
    return LBD_OK;
 }

/**
 * @brief Get lock
 *
 * @param [in] handle  the handle returned from wlanifBSteerControlCmnCreate()
 *                     to use to disable band steering feature
 *
 * @return LBD_OK on successful lock; otherwise LBD_NOK
 */
static LBD_STATUS wlanifBSteerControlCmnLock(
        wlanifBSteerControlHandle_t state) {
    struct sembuf sem;

    if (wlanifBSteerControlCmnLockInit(state) == LBD_NOK)
        return LBD_NOK;

    sem.sem_num  = 0;
    sem.sem_op   = -1;
    sem.sem_flg  = SEM_UNDO;

    if (semop(state->semid, &sem, 1) == -1)
    {
        return LBD_NOK;
    }

    return LBD_OK;
}

/**
 * @brief Release lock
 *
 * @param [in] handle  the handle returned from wlanifBSteerControlCmnCreate()
 *                     to use to disable band steering feature
 *
 * @return LBD_OK on successful unlock; otherwise LBD_NOK
 */
static LBD_STATUS wlanifBSteerControlCmnUnlock(
        wlanifBSteerControlHandle_t state) {
    struct sembuf sem;

    if (wlanifBSteerControlCmnLockInit(state) == LBD_NOK)
        return LBD_NOK;

    sem.sem_num  = 0;
    sem.sem_op   = 1;
    sem.sem_flg  = SEM_UNDO;

    if (semop(state->semid, &sem, 1) == -1)
    {
        return LBD_NOK;
    }

    return LBD_OK;
}

/**
 * @brief Enable the band steering feature on a given band.
 *
 * @pre state and band are valid
 *
 * @param [in] handle  the handle returned from wlanifBSteerControlCmnCreate()
 *                     to use to enable band steering feature
 * @param [in] band  the band on which to enable/disable band
 *        steering feature
 *
 * @return LBD_OK on successful enable; otherwise LBD_NOK
 */
static LBD_STATUS wlanifBSteerControlCmnSetEnable(
        wlanifBSteerControlHandle_t state, wlanif_band_e band) {
    // TODO Dan: There is no check for whether band is resolved here, since only
    //           after both bands are resolved, a valid handle will be returned.
    //           But it may be necessary after we add periodically VAP
    //           monitoring logic, some band may become invalid.

    if (wlanifBSteerControlCmnSendSetParams(state, band) == LBD_NOK) {
        dbgf(state->dbgModule, DBGERR, "%s: Failed to set band steering parameters on band %u",
             __func__, band);
        return LBD_NOK;
    }

    if (wlanifBSteerControlCmnSendEnable(state, band, LBD_TRUE /* enable */,
                                         LBD_FALSE /* ignoreError */) == LBD_NOK) {
        dbgf(state->dbgModule, DBGERR, "%s: Failed to enable band steering on band %u",
             __func__, band);
        return LBD_NOK;
    }

    if (wlanifBSteerControlCmnEnableSTAStatsBand(state, band,
                                                 LBD_TRUE /* enable */) != LBD_OK) {
        dbgf(state->dbgModule, DBGERR, "%s: Failed to enable STA stats on band %u for IAS",
             __func__, band);
        return LBD_NOK;
    }

    state->bandInfo[band].enabled = LBD_TRUE;

    dbgf(state->dbgModule, DBGINFO, "%s: Successfully enabled band steering on band %u",
         __func__, band);

    return LBD_OK;
}

/**
 * @brief Disable the band steering feature on a given band.
 *
 * @pre state and band are valid
 *
 * @param [in] handle  the handle returned from wlanifBSteerControlCmnCreate()
 *                     to use to disable band steering feature
 * @param [in] band  the band on which to disable band steering
 *                   feature
 */
static void wlanifBSteerControlCmnSetDisable(
        wlanifBSteerControlHandle_t state, wlanif_band_e band) {

    // Since band-steering may already be disabled, ignore errors here.
    // Will just be reported at debug level that band steering is already
    // disabled.

    if (wlanifBSteerControlCmnEnableSTAStatsBand(state, band,
                                                 LBD_FALSE /* enable */) != LBD_OK) {
        dbgf(state->dbgModule, DBGDEBUG,
             "%s: STA stats on band %u for IAS already disabled",
             __func__, band);
    }

    if (wlanifBSteerControlCmnSendEnable(state, band, LBD_FALSE /* enable */,
                                         LBD_TRUE /* ignoreError */) == LBD_NOK) {
        dbgf(state->dbgModule, DBGDEBUG,
             "%s: Band steering on band %u already disabled",
             __func__, band);
    } else {
        dbgf(state->dbgModule, DBGINFO, "%s: Successfully disabled band steering on band %u",
             __func__, band);
    }

    state->bandInfo[band].enabled = LBD_FALSE;
}

/**
 * @brief Check if for a given band, there is at least one valid VAP
 *
 * @param [in] state  the "this" pointer
 * @param [in] band  the band to check
 *
 * @return LBD_TRUE if at least one VAP is valid; otherwise LBD_FALSE
 */
static LBD_BOOL
wlanifBSteerControlCmnIsBandValid(wlanifBSteerControlHandle_t state,
                               wlanif_band_e band) {
    int i;
    for (i = 0; i < MAX_VAP_PER_BAND; ++i) {
        if (state->bandInfo[band].vaps[i].valid) {
            return LBD_TRUE;
        }
    }
    return LBD_FALSE;
}

/**
 * @brief For a given band, get an empty entry for VAP information
 *
 * @param [in] state  the "this" pointer
 * @param [in] band  the band to get VAP entry
 *
 * @return the empty VAP entry if any; otherwise NULL
 */
static struct wlanifBSteerControlVapInfo *
wlanifBSteerControlCmnAllocateVap(wlanifBSteerControlHandle_t state,
                               wlanif_band_e band) {
    int i;
    for (i = 0; i < MAX_VAP_PER_BAND; ++i) {
        if (!state->bandInfo[band].vaps[i].valid) {
            memset(&state->bandInfo[band].vaps[i], 0, sizeof(state->bandInfo[band].vaps[i]));
            return &state->bandInfo[band].vaps[i];
        }
    }
    dbgf(state->dbgModule, DBGERR, "%s: No available VAP entry on band %u; "
                                   "maximum number of VAPs allowed on one band: %u",
        __func__, band, MAX_VAP_PER_BAND);
    return NULL;
}

/**
 * @brief Extract the VAP handle out of the BSS info and cast it to its proper
 *        type.
 *
 * @param [in] bss  the value from which to extract the VAP handle
 *
 * @return the VAP handle, or NULL if the BSS is invalid
 */
static struct wlanifBSteerControlVapInfo *wlanifBSteerControlCmnExtractVapHandle(
        const lbd_bssInfo_t *bss) {
    if (bss) {
        return bss->vap;
    }

    return NULL;
}

/**
 * @brief For a given interface name, find the internal radio entry.
 *
 * If one does not exist, allocate it from one of the free slots.
 *
 * @param [in] state  the "this" pointer
 * @param [in] ifname  the interface name of the radio
 *
 * @return the found (or newly created) radio entry, or NULL if there are
 *         no more free slots for radios
 */
static struct wlanifBSteerControlRadioInfo *
wlanifBSteerControlCmnLookupRadioByIfname(wlanifBSteerControlHandle_t state,
                                       const char *ifname) {
    struct wlanifBSteerControlRadioInfo *empty = NULL;

    size_t i;
    for (i = 0; i < sizeof(state->radioInfo) / sizeof(state->radioInfo[0]); ++i) {
        if (state->radioInfo[i].valid &&
            strcmp(ifname, state->radioInfo[i].ifname) == 0) {
            return &state->radioInfo[i];
        } else if (!state->radioInfo[i].valid && !empty) {
            empty = &state->radioInfo[i];
        }
    }

    if (empty) {
        strlcpy(empty->ifname, ifname, sizeof(empty->ifname));
        //NOTE: This ifname is radio name (i.e. wifi0, wifi1...)
#if SONLIB_SUPPORT_ENABLED
        // This is QTI driver specific call
        // Should be done only for when vendor is QTI
        if ( soncmnGetVendorFromRadioName (ifname) != SONCMN_VENDOR_QTI )
        {
            goto nonQti;
        }
#endif
        if(wlanIfLb->IsCfg80211 == 0)
        {
            if (wlanifBSteerControlCmnResolvePrivateIoctls(state, ifname,
                                                       LBD_TRUE,
                                                       empty) != LBD_OK) {
                // A log will already have been generated.
                return NULL;
            }
        }
        else
        {
#define OL_SPECIAL_PARAM_ENABLE_OL_STATS 8205
            /*
                Use the hardcode parameter for CFG80211
            */
            empty->enableOLStatsIoctl = OL_SPECIAL_PARAM_ENABLE_OL_STATS;

            /*Direct Attach does not have support for cfg80211 yet */
            empty->enableNoDebug = 0;/*ATH_PARAM_NODEBUG*/

            empty->sonIoctl = IEEE80211_PARAM_SON;
        }

        struct ifreq ifr;
        strlcpy(ifr.ifr_name, ifname, IFNAMSIZ);
        if (ioctl(state->controlSock, SIOCGIFHWADDR, &ifr) < 0) {
            dbgf(state->dbgModule, DBGERR, "%s: ioctl() SIOCGIFHWADDR failed, ifname: %s",
                 __func__, ifname);
            return NULL;
        }
        lbCopyMACAddr(ifr.ifr_hwaddr.sa_data, empty->radioAddr.ether_addr_octet);

#if SONLIB_SUPPORT_ENABLED
nonQti:
#endif
        empty->valid = LBD_TRUE;

        // Set the channel to invalid, it will be filled in while adding VAPs.
        empty->channel = LBD_CHANNEL_INVALID;

        // Initialize waiting list for RSSI measurement to be empty
        list_set_head(&empty->rssiWaitingList);
    }

    return empty;
}

/**
 * @brief Determine if all ioctls have been resolved on the radio.
 *
 * @param [in] radio  the radio to check for its ioctl status
 *
 * @return LBD_TRUE if they have all been resolved; otherwise LBD_FALSE
 */
static LBD_BOOL wlanifBSteerControlAllVAPIoctlsResolved(
        const struct wlanifBSteerControlRadioInfo *radio) {
    return radio->sonIoctl;
}

/**
 * @brief Examine the private ioctls to find the ones relevant for the
 *        load balancing implementation.
 *
 * This will store the appropriate ioctl values in the radio object. If no
 * matching ioctl was found, a value of 0 is stored. This indicates the ioctl
 * is not supported (which in the case of the stats enable/disable ioctl means
 * nothing needs to be done to enable the stats).
 *
 * @param [in] state  the "this" pointer
 * @param [in] radio  the radio on which to resolve the ioctl
 *
 * @return LBD_OK on success; otherwise LBD_NOK
 */
static LBD_STATUS wlanifBSteerControlCmnResolvePrivateIoctls(
        wlanifBSteerControlHandle_t state, const char *ifname,
        LBD_BOOL radioLevel, struct wlanifBSteerControlRadioInfo *radio) {
    struct iw_priv_args *privArgs = NULL;

    // We do not know how many actual ioctls there are, so allocate a
    // reasonably large number to start with. If it is insufficient, the
    // kernel will tell us and we can re-allocate.
    size_t maxNumEntries = 256;
    privArgs = calloc(maxNumEntries, sizeof(struct iw_priv_args));
    if (!privArgs) {
        dbgf(state->dbgModule, DBGERR,
             "%s: Initial memory allocation failed for private ioctl buffer",
             __func__);
        return LBD_NOK;
    }

    struct iwreq wrq;
    memset(&wrq, 0, sizeof(wrq));
    strlcpy(wrq.ifr_name, ifname, sizeof(wrq.ifr_name));
    wrq.u.data.pointer = privArgs;
    wrq.u.data.length = maxNumEntries;

    LBD_STATUS result = LBD_OK;
    while (ioctl(state->controlSock, SIOCGIWPRIV, &wrq) < 0) {
        if (E2BIG == errno) {
            // Kernel can provide a hint of how many entries we need.
            if (wrq.u.data.length > maxNumEntries) {
                maxNumEntries = wrq.u.data.length;
            } else {
                maxNumEntries *= 2;
            }

            privArgs = realloc(privArgs, maxNumEntries * sizeof(struct iw_priv_args));
            if (!privArgs) {
                dbgf(state->dbgModule, DBGERR,
                     "%s: Failed to realloc for %zu entries",
                     __func__, maxNumEntries);
                result = LBD_NOK;
                break;
            }

            wrq.u.data.pointer = privArgs;
            wrq.u.data.length = maxNumEntries;
        } else {
            dbgf(state->dbgModule, DBGERR,
                 "%s: ioctl failed on %s for unknown reason: %d",
                 __func__, wrq.ifr_name, errno);
            result = LBD_NOK;
            break;
        }
    }

    if (LBD_OK == result) {
        wlanifBSteerControlCmnFindPrivateIoctls(state, radio, radioLevel,
                                                wrq.u.data.length, privArgs);

        dbgf(state->dbgModule, DBGINFO,
             "%s: Resolved private stats ioctl on %s: enable_ol_stats [%04x] "
             "disablestats [%04x] son [%04x] %p", __func__, radio->ifname,
             radio->enableOLStatsIoctl, radio->enableNoDebug,
             radio->sonIoctl, radio);
    }

    free(privArgs);
    return result;
}

/**
 * @brief Find the matching ioctls that can enable the offload stats and
 *        update the SON mode.
 *
 * The value of these ioctls will be stored in the radio. If any is not
 * found, a value of 0 will be stored for that ioctl.
 *
 * @param [in] state  the "this" pointer
 * @param [in] radio  the radio on which to resolve the ioctl
 * @param [in] radioLevel  whether these ioctls are on the radio or VAP
 * @param [in] numIoctls  the number of ioctls described in privArgs
 * @param [in] privArgs  the description of the private ioctls
 */
static void wlanifBSteerControlCmnFindPrivateIoctls(
        wlanifBSteerControlHandle_t state,
        struct wlanifBSteerControlRadioInfo *radio,
        LBD_BOOL radioLevel, size_t numIoctls,
        struct iw_priv_args *privArgs) {
    // Loop over all of the entries looking for the match.
    size_t i = 0;
    for (i = 0; i < numIoctls; ++i) {
        if (radioLevel && strcmp(privArgs[i].name, "enable_ol_stats") == 0) {
            radio->enableOLStatsIoctl = privArgs[i].cmd;
        } else if (radioLevel && strcmp(privArgs[i].name, "disablestats") == 0) {
            radio->enableNoDebug = privArgs[i].cmd;
        } else if (!radioLevel && strcmp(privArgs[i].name, "son") == 0) {
            radio->sonIoctl = privArgs[i].cmd;
        }


        // Terminate early if both are found
        if ((radio->enableOLStatsIoctl || radio->enableNoDebug) &&
            radio->sonIoctl) {
            break;
        }
    }
}

/**
 * @brief Log the interface info
 *
 * @param [in] state  the "this" pointer
 * @param [in] vap  VAP to log interface info for
 */
static void wlanifBSteerControlCmnLogInterfaceInfo(wlanifBSteerControlHandle_t state,
                                                struct wlanifBSteerControlVapInfo *vap) {
    if (diaglog_startEntry(mdModuleID_WlanIF, wlanif_msgId_interface,
                           diaglog_level_info)) {
        diaglog_writeMAC(&vap->macaddr);
        diaglog_write8(vap->radio->channel);
        diaglog_write8(vap->essId);
        diaglog_write8(state->essInfo[vap->essId].ssidLen);
        diaglog_write(state->essInfo[vap->essId].ssidStr,
                      state->essInfo[vap->essId].ssidLen);
        diaglog_write8(strlen(vap->ifname));
        diaglog_write(vap->ifname, strlen(vap->ifname));
        diaglog_finishEntry();
    }
}

LBD_STATUS
wlanifBSteerControlCmnStoreSSID(wlanifBSteerControlHandle_t state,
                             const char *ifname,
                             struct wlanifBSteerControlVapInfo *vap,
                             u_int8_t length,
                             u_int8_t *ssidStr) {
    int i;

    if ((!length) || (length > IEEE80211_NWID_LEN)) {
        dbgf(state->dbgModule, DBGERR, "%s: invalid ESSID length %d, ifName: %s",
             __func__, length, ifname);

        return LBD_NOK;
    }

    // Check if this ESSID is already known
    LBD_BOOL match = LBD_FALSE;
    for (i = 0; i < state->essCount; i++) {
        // Note memcmp is used here since the SSID string can theoretically have
        // embedded NULL characters
        if ((state->essInfo[i].ssidLen == length) &&
            (memcmp(&state->essInfo[i].ssidStr, ssidStr,
                    state->essInfo[i].ssidLen) == 0)) {
            dbgf(state->dbgModule, DBGDUMP,
                 "%s: ESS %s found at index %d for interface %s",
                 __func__, state->essInfo[i].ssidStr, i, ifname);
            vap->essId = i;
            match = LBD_TRUE;
            break;
        }
    }

    if (!match) {
        // New ESS found

        // Should not be possible to have more unique ESSes than there are VAPs.
        // However, this may need to be revisited if all ESSes in the network are
        // recorded here.
        lbDbgAssertExit(state->dbgModule, state->essCount < MAX_VAP_PER_BAND);

        vap->essId = state->essCount;
        state->essInfo[i].ssidLen = length;

        // Note memcpy is used here since the SSID string can theoretically have
        // embedded NULL characters.We also ensure there is a null terminator
        // (purely for debug logging).
        memcpy(&state->essInfo[state->essCount].ssidStr,
               ssidStr, length);
        state->essInfo[state->essCount].ssidStr[length] = '\0';

        dbgf(state->dbgModule, DBGINFO,
             "%s: Adding new ESS %s to index %d for interface %s",
             __func__, state->essInfo[i].ssidStr, i, ifname);
        state->essCount++;
    }

    return LBD_OK;
}

/**
 * @brief Fetch the SSID this VAP is operating on from the
 *        driver, and store in the local VAP structure.
 *
 * @param [in] state the "this" pointer
 * @param [in] ifname name of the interface this VAP is
 *                    operating on
 * @param [in] vap pointer to VAP structure
 *
 * @return LBD_STATUS returns LBD_OK if the SSID is valid,
 *                    LBD_NOK otherwise
 */
static LBD_STATUS
wlanifBSteerControlCmnGetSSID(wlanifBSteerControlHandle_t state, const char *ifname,
                           struct wlanifBSteerControlVapInfo *vap) {
    u_int8_t buf[IEEE80211_NWID_LEN + 1];
    u_int32_t length;

    length = IEEE80211_NWID_LEN;
#if SONLIB_SUPPORT_ENABLED
    if (vap->drvHandle->ops->getSSID(vap->drvHandle->ctx, ifname, &buf, &length)) {
#else
    if(wlanIfLb->ops->getESSID(wlanIfLb->ctx, ifname, (void *)&buf, &length) < 0) {
#endif
        dbgf(state->dbgModule, DBGERR, "%s: getSSID failed, ifName: %s",
             __func__, ifname);

        return LBD_NOK;
    }

    return wlanifBSteerControlCmnStoreSSID(state, ifname, vap, length, buf);
}

/**
 * @brief If no channel is stored for the radio associated with
 *        this VAP, resolve the channel and regulatory class
 *        from the frequency.
 *
 * @param [in] state the "this" pointer
 * @param [in] frequency frequency the radio is operating on
 * @param [in] vap the VAP pointer
 *
 * @return LBD_STATUS LBD_OK if the channel / regulatory class
 *                    could be resolved, LBD_NOK otherwise
 */
static LBD_STATUS
wlanifBSteerControlCmnUpdateRadioForFrequency(
        wlanifBSteerControlHandle_t state, int frequency,
        struct wlanifBSteerControlVapInfo *vap) {
    if (vap->radio->channel != LBD_CHANNEL_INVALID) {
        // Radio channel is already resolved, return
        return LBD_OK;
    }

    if (wlanifResolveRegclassAndChannum(frequency,
                                        &vap->radio->channel,
                                        &vap->radio->regClass) != LBD_OK) {
        dbgf(state->dbgModule, DBGERR,
             "%s: Invalid channel / regulatory class for radio %s, frequency is %d",
             __func__, vap->radio->ifname, frequency);
        return LBD_NOK;
    }

    return LBD_OK;
}

/**
 * @brief For a given interface name, resolve a VAP entry on the corresponding band
 *
 * Once successfully resolved, the VAP entry will be marked
 * valid, with channel and system index information
 *
 * @param [in] state  the "this" pointer
 * @param [in] ifname  the interface name
 * @param [in] radio  the radio instance that owns this VAP
 *
 * @return the VAP entry containing interface name and channel
 *         on success; otherwise NULL
 */
static struct wlanifBSteerControlVapInfo *
wlanifBSteerControlCmnInitVapFromIfname(wlanifBSteerControlHandle_t state,
                                     const char *ifname,
                                     struct wlanifBSteerControlRadioInfo *radio,
                                     LBD_BOOL includeFlag) {
    struct wlanifBSteerControlVapInfo *vap = NULL;
    struct ifreq buffer;
    wlanif_band_e band;
    int32_t freq = 0;

#if SONLIB_SUPPORT_ENABLED
    struct soncmnDrvHandle *drvHandle = soncmnGetDrvHandleFromIfname (ifname);
    if (!drvHandle) {
        dbgf(state->dbgModule, DBGERR, "%s: Failed to obtain drvHandle for ifName: %s\n",
                    __func__, ifname);
        return NULL;
    }
    if (drvHandle->ops->getFreq(drvHandle->ctx, ifname, &freq) < 0) {
#else
    if (wlanIfLb->ops->getFreq(wlanIfLb->ctx ,ifname , &freq) < 0) {
#endif
        dbgf(state->dbgModule, DBGERR, "%s: getFreq failed, ifName: %s",
             __func__, ifname);
        return NULL;
    }

    band = wlanifMapFreqToBand(freq);
    if (band >= wlanif_band_invalid) {
        dbgf(state->dbgModule, DBGERR, "%s: ioctl() SIOCGIWFREQ returned invalid frequency, ifName: %s",
             __func__, ifname);
        return NULL;
    }

    vap = wlanifBSteerControlCmnAllocateVap(state, band);
    if (vap == NULL) {
        // Maximum number of VAPs reached on the given band
        return NULL;
    }

    strlcpy(vap->ifname, ifname, IFNAMSIZ + 1);
    vap->radio = radio;
#if SONLIB_SUPPORT_ENABLED
    //Store DrvHandle in VAP structure
    vap->drvHandle = soncmnGetDrvHandleFromIfname (ifname);
    if (!vap->drvHandle){
        dbgf(state->dbgModule, DBGERR, "%s: Failed to obtain drvHandle for ifName: %s\n",
                    __func__, ifname);
        return NULL;
    }
#endif
    // Get the channel and store in the radio (if not already done)
    if (wlanifBSteerControlCmnUpdateRadioForFrequency(state,
                                                   freq, vap) != LBD_OK) {
        return NULL;
    }

    if (!(vap->sysIndex = if_nametoindex(ifname))) {
        dbgf(state->dbgModule, DBGERR, "%s: Resolve index failed, ifname: %s",
             __func__, ifname);
        return NULL;
    }

    strlcpy(buffer.ifr_name, ifname, IFNAMSIZ);
    if (ioctl(state->controlSock, SIOCGIFHWADDR, &buffer) < 0) {
        dbgf(state->dbgModule, DBGERR, "%s: ioctl() SIOCGIFHWADDR failed, ifName: %s",
             __func__, ifname);
        return NULL;
    }
    lbCopyMACAddr(buffer.ifr_hwaddr.sa_data, vap->macaddr.ether_addr_octet);

    // Get the SSID
    if (wlanifBSteerControlCmnGetSSID(state, ifname, vap) != LBD_OK) {
        return NULL;
    }

    if (!wlanifBSteerControlAllVAPIoctlsResolved(radio)) {
        // Look for private ioctls at the VAP level as well.

#if SONLIB_SUPPORT_ENABLED
        // This is QTI vendor specific, only to be called for QTI
        if (soncmnGetVendorFromIfname (ifname) != SONCMN_VENDOR_QTI)
        {
            goto nonQti;
        }
#endif
        if(wlanIfLb->IsCfg80211 == 0) {
            if (wlanifBSteerControlCmnResolvePrivateIoctls(state, ifname,
                                                       LBD_FALSE,
                                                       radio) != LBD_OK) {
                return NULL;
            }
        }
        else
        {
            radio->enableOLStatsIoctl = IEEE80211_PARAM_ENABLE_OL_STATS;
            /*Direct Attach does not have support for cfg80211 yet */
            radio->enableNoDebug = 0;/*ATH_PARAM_NODEBUG*/
            radio->sonIoctl = IEEE80211_PARAM_SON;
        }
    }
#if SONLIB_SUPPORT_ENABLED
nonQti:
#endif

    vap->valid = LBD_TRUE;
    vap->includedIface = includeFlag;
    // Log the newly constructed interface
    wlanifBSteerControlCmnLogInterfaceInfo(state, vap);

    return vap;
}

void isCfg80211_lbd (int cfg80211)
{
    isCfg80211=cfg80211;
}
#ifdef LBD_SUPPORT_QSDK
int wlanIfConfigInit(int isCfg80211)
{
    fprintf(stderr, "%s: wlanIfLb %p\n",__func__,wlanIfLb);

    if(wlanIfLb) {
        fprintf(stderr, "%s: wlanIfLb %p is already init\n",__func__,wlanIfLb);
        return 0;
    }

    if(isCfg80211)
    {
        wlanIfLb = wlanif_config_init(WLANIF_CFG80211, LBD_NL80211_CMD_SOCK,
                                      LBD_NL80211_EVENT_SOCK);
    }
    else {
        wlanIfLb = wlanif_config_init(WLANIF_WEXT, 0, 0);
    }

    if ( !wlanIfLb )
    {
        return -1;
    }
    fprintf(stderr, "%s: wlanIfLb INIT %p\n",__func__,wlanIfLb);
    return 0;
}
#endif
/**
 * @brief Resolve Wlan interfaces from configuration file and system.
 *
 * It will parse interface names from config file, then resolve
 * band and system index using ioctl() and if_nametoindex().
 *
 * @param [in] state  the "this" pointer
 * @param [in] allowZeroAPIfaces  whether the number of AP interfaces can be
 *                                0 or not
 * @param [in] includedVaps  variable to indicate whether to resolve
 *                           included or excluded wlan interfaces
 *
 * @return LBD_OK if both bands are resolved; otherwise LBD_NOK
 */
static LBD_STATUS wlanifBSteerControlCmnResolveWlanIfaces(
    wlanifBSteerControlHandle_t state, LBD_BOOL allowZeroAPIfaces,
    LBD_BOOL includedVaps) {
    // The size here considers we have two interface names, separated by a
    // colon.
    char ifnamePair[MAX_VAP_PER_BAND * WLANIF_MAX_RADIOS][1 + 2 * (IFNAMSIZ + 1)];
    u_int8_t i = 0;
    const char *wlanInterfaces;
    int numInterfaces;
    struct wlanifBSteerControlVapInfo *vap = NULL;

    if (includedVaps) {
        wlanInterfaces = profileGetOpts(mdModuleID_WlanIF,
                                        WLANIFBSTEERCONTROL_WLAN_INTERFACES,
                                        NULL);
    } else {
        wlanInterfaces = profileGetOpts(mdModuleID_WlanIF,
                                        WLANIFBSTEERCONTROL_WLAN_INTERFACES_EXCLUDED,
                                        NULL);
    }

    if (!wlanInterfaces) {
        if (!allowZeroAPIfaces) {
            dbgf(state->dbgModule, DBGERR,
                 "%s: No WLAN interface listed in config file (allowZero=%d, incluedVAPs=%d)",
                 __func__, allowZeroAPIfaces, includedVaps);
            return LBD_NOK;
        } else {
            // No interfaces is allowed when specially configured to accept it.
            return LBD_OK;
        }
    }

    do {
        numInterfaces = splitByToken(wlanInterfaces,
                                     sizeof(ifnamePair) / sizeof(ifnamePair[0]),
                                     sizeof(ifnamePair[0]),
                                     (char *)ifnamePair, ',');
        if (numInterfaces < wlanif_band_invalid) {
            // An empty string is allowed if we are specially configured for
            // it.
            if (allowZeroAPIfaces &&
                (numInterfaces == 0 ||
                 (numInterfaces == 1 && strlen(ifnamePair[0]) == 0))) {
                free((char *)wlanInterfaces);
                return LBD_OK;
            }

            dbgf(state->dbgModule, DBGERR,
                 "%s: Failed to resolve WLAN interfaces from '%s':"
                 " at least one interface per band is required (got %u)",
                 __func__, wlanInterfaces, numInterfaces);

            if (!includedVaps) {
                free((char *)wlanInterfaces);
                return LBD_OK;
            }
            break;
        }
#ifdef LBD_SUPPORT_QSDK
        if(wlanIfConfigInit(isCfg80211))
        {
            dbgf(state->dbgModule, DBGERR, "%s: wlanif init failed\n",__func__);
            break;
        }
#endif
        if ((state->controlSock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
            dbgf(state->dbgModule, DBGERR, "%s: Create ioctl socket failed", __func__);
            break;
        }

        if (fcntl(state->controlSock, F_SETFL, fcntl(state->controlSock, F_GETFL) | O_NONBLOCK)) {
            dbgf(state->dbgModule, DBGERR, "%s: fcntl() failed", __func__);
            break;
        }

        for (i = 0; i < numInterfaces; i++) {
            char ifnames[2][IFNAMSIZ + 1];
            if (splitByToken(ifnamePair[i], sizeof(ifnames) / sizeof(ifnames[0]),
                             sizeof(ifnames[0]), (char *) ifnames,
                             ':') != 2) {
                dbgf(state->dbgModule, DBGERR,
                     "%s: Failed to resolve radio and VAP names from %s",
                     __func__, ifnamePair[i]);
                vap = NULL;
                break;
            }

            struct wlanifBSteerControlRadioInfo *radio =
                wlanifBSteerControlCmnLookupRadioByIfname(state, ifnames[0]);
            if (!radio) {
                vap = NULL;  // signal a failiure
                break;
            }

            if (includedVaps) {
                vap = wlanifBSteerControlCmnInitVapFromIfname(state, ifnames[1], radio, LBD_TRUE);
            } else {
                vap = wlanifBSteerControlCmnInitVapFromIfname(state, ifnames[1], radio, LBD_FALSE);
            }

            if (!vap) {
                break;
            }
        }

        if (!vap) {
            break;
        }

        if (wlanifBSteerControlCmnIsBandValid(state, wlanif_band_24g) &&
            wlanifBSteerControlCmnIsBandValid(state, wlanif_band_5g)) {
            free((char *)wlanInterfaces);
            return LBD_OK;
        }
    } while(0);

    if (state->controlSock > 0) {
        close(state->controlSock);
    }
    free((char *)wlanInterfaces);
    return LBD_NOK;
}

/**
 * @brief Read configuration parameters from config file and do sanity check
 *
 * If inactivity check period or MU check period from config file is invalid,
 * use the default one.
 *
 * @param [in] state  the "this" pointer
 * @param [in] band  the band to resolve configuration parameters
 *
 * @return LBD_NOK if the MU average period is shorter then MU sample period;
 *                 otherwise return LBD_OK
 */
static LBD_STATUS
wlanifBSteerControlCmnReadConfig(wlanifBSteerControlHandle_t state,
                                 wlanif_band_e band) {
    enum mdModuleID_e moduleID;
    struct profileElement *defaultProfiles;
    const char *inactivityCheckPeriod;
    u_int8_t index = 0;
    int value[BSTEERING_MAX_CLIENT_CLASS_GROUP];

    if (band == wlanif_band_24g) {
        moduleID = mdModuleID_WlanIF_Config_24G;
        defaultProfiles = wlanifElementDefaultTable_24G;
    } else {
        moduleID = mdModuleID_WlanIF_Config_5G;
        defaultProfiles = wlanifElementDefaultTable_5G;
    }

    if (profileGetOptsIntArray(moduleID,
        WLANIFBSTEERCONTROL_INACT_IDLE_THRESHOLD,
        defaultProfiles, value) == LBD_NOK) {
        dbgf(state->dbgModule, DBGERR,
             "[Band %u] Unable to parse %s",
             band, WLANIFBSTEERCONTROL_INACT_IDLE_THRESHOLD);
        return LBD_NOK;
    }
    for (index = 0; index < BSTEERING_MAX_CLIENT_CLASS_GROUP; index++) {
        state->bandInfo[band].configParams.inactivity_timeout_normal[index] = value[index];
    }

    state->bandInfo[band].configParams.inactivity_timeout_overload =
        profileGetOptsInt(moduleID,
                          WLANIFBSTEERCONTROL_INACT_OVERLOAD_THRESHOLD,
                          defaultProfiles);

    state->bandInfo[band].configParams.inactivity_check_period =
        profileGetOptsInt(moduleID,
                          WLANIFBSTEERCONTROL_INACT_CHECK_INTERVAL,
                          defaultProfiles);
    if (state->bandInfo[band].configParams.inactivity_check_period <= 0) {
        dbgf(state->dbgModule, DBGINFO, "[Band %u] Inactivity check period value is invalid (%d), use default one",
                band, state->bandInfo[band].configParams.inactivity_check_period);
        inactivityCheckPeriod = profileElementDefault(WLANIFBSTEERCONTROL_INACT_CHECK_INTERVAL,
                                                       defaultProfiles);
        if (inactivityCheckPeriod == NULL) {
            dbgf(state->dbgModule, DBGERR, "[Band %u] Inactivity check period value is NULL",
                 band);
            return LBD_NOK;
        } else {
            state->bandInfo[band].configParams.inactivity_check_period =
                atoi(inactivityCheckPeriod);
        }
    }

    if (profileGetOptsIntArray(moduleID,
        WLANIFBSTEERCONTROL_INACT_RSSI_XING_HIGH_THRESHOLD,
        defaultProfiles, value) == LBD_NOK) {
        dbgf(state->dbgModule, DBGERR,
             "[Band %u] Unable to parse %s",
             band, WLANIFBSTEERCONTROL_INACT_RSSI_XING_HIGH_THRESHOLD);
        return LBD_NOK;
    }
    for (index = 0; index < BSTEERING_MAX_CLIENT_CLASS_GROUP; index++) {
        state->bandInfo[band].configParams.inactive_rssi_xing_high_threshold[index] = value[index];
    }

    if (profileGetOptsIntArray(moduleID,
        WLANIFBSTEERCONTROL_INACT_RSSI_XING_LOW_THRESHOLD,
        defaultProfiles, value) == LBD_NOK) {
        dbgf(state->dbgModule, DBGERR,
             "[Band %u] Unable to parse %s",
             band, WLANIFBSTEERCONTROL_INACT_RSSI_XING_LOW_THRESHOLD);
        return LBD_NOK;
    }
    for (index = 0; index < BSTEERING_MAX_CLIENT_CLASS_GROUP; index++) {
        state->bandInfo[band].configParams.inactive_rssi_xing_low_threshold[index] = value[index];
    }

    state->bandInfo[band].configParams.low_rssi_crossing_threshold =
        profileGetOptsInt(moduleID,
                          WLANIFBSTEERCONTROL_LOW_RSSI_XING_THRESHOLD,
                          defaultProfiles);

    state->bandInfo[band].configParams.utilization_sample_period =
        profileGetOptsInt(moduleID,
                          WLANIFBSTEERCONTROL_MU_CHECK_INTERVAL,
                          defaultProfiles);

    // If utilization check interval is zero, disable ACS report
    if (!state->bandInfo[band].configParams.utilization_sample_period) {
        dbgf(state->dbgModule, DBGINFO, "[Band %u] Utilization sample period value is invalid (%d), will not perform ACS report",
                band, state->bandInfo[band].configParams.utilization_sample_period);
        state->bandInfo[band].configParams.utilization_average_num_samples = 0;
    } else {
        int muAvgPeriod = profileGetOptsInt(moduleID,
                                            WLANIFBSTEERCONTROL_MU_AVG_PERIOD,
                                            defaultProfiles);
        // Sanity check to make sure utilization average period is larger than sample interval
        if (muAvgPeriod <= state->bandInfo[band].configParams.utilization_sample_period) {
            dbgf(state->dbgModule, DBGINFO, "[Band %u] Utilization average period (%d seconds) is shorter than"
                                            " Utilization sample period (%d seconds).",
                    band, muAvgPeriod, state->bandInfo[band].configParams.utilization_sample_period);
            return LBD_NOK;
        }
        state->bandInfo[band].configParams.utilization_average_num_samples = muAvgPeriod /
            state->bandInfo[band].configParams.utilization_sample_period;
    }

    if (profileGetOptsIntArray(moduleID,
        WLANIFBSTEERCONTROL_BCNRPT_ACTIVE_DURATION,
        defaultProfiles, value) == LBD_NOK) {
        dbgf(state->dbgModule, DBGERR,
             "[Band %u] Unable to parse %s",
             band, WLANIFBSTEERCONTROL_BCNRPT_ACTIVE_DURATION);
        return LBD_NOK;
    }
    for (index = 0; index < BSTEERING_MAX_CLIENT_CLASS_GROUP; index++) {
        state->bandInfo[band].bcnrptDurations[IEEE80211_RRM_BCNRPT_MEASMODE_ACTIVE][index] = value[index];
    }

    if (profileGetOptsIntArray(moduleID,
        WLANIFBSTEERCONTROL_BCNRPT_PASSIVE_DURATION,
        defaultProfiles, value) == LBD_NOK) {
        dbgf(state->dbgModule, DBGERR,
             "[Band %u] Unable to parse %s",
             band, WLANIFBSTEERCONTROL_BCNRPT_PASSIVE_DURATION);
        return LBD_NOK;
    }
    for (index = 0; index < BSTEERING_MAX_CLIENT_CLASS_GROUP; index++) {
        state->bandInfo[band].bcnrptDurations[IEEE80211_RRM_BCNRPT_MEASMODE_PASSIVE][index] = value[index];
    }

    state->bandInfo[band].configParams.low_tx_rate_crossing_threshold =
        profileGetOptsInt(moduleID,
                          WLANIFBSTEERCONTROL_LOW_TX_RATE_XING_THRESHOLD,
                          defaultProfiles);

    if (profileGetOptsIntArray(moduleID,
        WLANIFBSTEERCONTROL_HIGH_TX_RATE_XING_THRESHOLD,
        defaultProfiles, value) == LBD_NOK) {
        dbgf(state->dbgModule, DBGERR,
             "[Band %u] Unable to parse %s",
             band, WLANIFBSTEERCONTROL_HIGH_TX_RATE_XING_THRESHOLD);
        return LBD_NOK;
    }
    for (index = 0; index < BSTEERING_MAX_CLIENT_CLASS_GROUP; index++) {
        state->bandInfo[band].configParams.high_tx_rate_crossing_threshold[index] = value[index];
    }

    // Sanity check that the high Tx rate threshold is greater than the low Tx rate
    // threshold.
    for (index = 0; index < BSTEERING_MAX_CLIENT_CLASS_GROUP; index++) {
        if (state->bandInfo[band].configParams.low_tx_rate_crossing_threshold >=
            state->bandInfo[band].configParams.high_tx_rate_crossing_threshold[index]) {
            dbgf(state->dbgModule, DBGERR,
                 "[Band %u] Low Tx rate crossing threshold (%u) is greater or equal to"
                 " high Tx rate crossing threshold (%u).",
                 band, state->bandInfo[band].configParams.low_tx_rate_crossing_threshold,
                 state->bandInfo[band].configParams.high_tx_rate_crossing_threshold[index]);
            return LBD_NOK;
        }
    }

    state->bandInfo[band].configParams.low_rate_rssi_crossing_threshold =
        profileGetOptsInt(moduleID,
                          WLANIFBSTEERCONTROL_LOW_RATE_RSSI_XING_THRESHOLD,
                          defaultProfiles);

    if (profileGetOptsIntArray(moduleID,
        WLANIFBSTEERCONTROL_HIGH_RATE_RSSI_XING_THRESHOLD,
        defaultProfiles, value ) == LBD_NOK) {
        dbgf(state->dbgModule, DBGERR,
             "[Band %u] Unable to parse %s",
             band, WLANIFBSTEERCONTROL_HIGH_RATE_RSSI_XING_THRESHOLD);
        return LBD_NOK;
    }
    for (index = 0; index < BSTEERING_MAX_CLIENT_CLASS_GROUP; index++) {
        state->bandInfo[band].configParams.high_rate_rssi_crossing_threshold[index] = value[index];
    }

    // Sanity check that the high rate RSSI threshold is greater than the low rate RSSI
    // threshold.
    for (index = 0; index < BSTEERING_MAX_CLIENT_CLASS_GROUP; index++) {
        if (state->bandInfo[band].configParams.low_rate_rssi_crossing_threshold >=
            state->bandInfo[band].configParams.high_rate_rssi_crossing_threshold[index]) {
            dbgf(state->dbgModule, DBGERR,
                 "[Band %u] Low rate RSSI crossing threshold (%u) is greater or equal to"
                 " high rate RSSI crossing threshold (%u).",
                 band, state->bandInfo[band].configParams.low_rate_rssi_crossing_threshold,
                 state->bandInfo[band].configParams.high_rate_rssi_crossing_threshold[index]);
            return LBD_NOK;
        }
    }

    if (profileGetOptsIntArray(moduleID,
        WLANIFBSTEERCONTROL_AP_STEER_LOW_XING_THRESHOLD,
        defaultProfiles, value) == LBD_NOK) {
        dbgf(state->dbgModule, DBGERR,
             "[Band %u] Unable to parse %s",
             band, WLANIFBSTEERCONTROL_AP_STEER_LOW_XING_THRESHOLD);
        return LBD_NOK;
    }
    for (index = 0; index < BSTEERING_MAX_CLIENT_CLASS_GROUP; index++) {
        state->bandInfo[band].configParams.ap_steer_rssi_xing_low_threshold[index] = value[index];
    }

    state->bandInfo[band].configParams.interference_detection_enable =
        profileGetOptsInt(moduleID,
                          WLANIFBSTEERCONTROL_INTERFERENCE_DETECTION_ENABLE,
                          defaultProfiles);

    state->bandInfo[band].configParams.client_classification_enable =
        profileGetOptsInt(moduleID,
                          WLANIFBSTEERCONTROL_CLIENT_CLASSIFICATION_ENABLE,
                          defaultProfiles);

    if (profileGetOptsIntArray(moduleID,
        WLANIFBSTEERCONTROL_AUTH_ALLOW,
        defaultProfiles, value) == LBD_NOK) {
        dbgf(state->dbgModule, DBGERR,
             "[Band %u] Unable to parse %s",
             band, WLANIFBSTEERCONTROL_AUTH_ALLOW);
        return LBD_NOK;
    }
    for (index = 0; index < BSTEERING_MAX_CLIENT_CLASS_GROUP; index++) {
        state->auth[index] = value[index];
    }

    state->bandInfo[band].configParams.delay_24g_probe_rssi_threshold =
        profileGetOptsInt(moduleID,
                          WLANIFBSTEERCONTROL_DELAY_24G_PROBE_RSSI_THRESHOLD,
                          defaultProfiles);

    state->bandInfo[band].configParams.delay_24g_probe_time_window =
        profileGetOptsInt(moduleID,
                          WLANIFBSTEERCONTROL_DELAY_24G_PROBE_TIME_WINDOW,
                          defaultProfiles);

    state->bandInfo[band].configParams.delay_24g_probe_min_req_count =
        profileGetOptsInt(moduleID,
                          WLANIFBSTEERCONTROL_DELAY_24G_PROBE_MIN_REQ_COUNT,
                          defaultProfiles);

    state->blacklistOtherESS =
        profileGetOptsInt(moduleID,
                WLANIFBSTEERCONTROL_BLACKLIST_OTHER_ESS,
                defaultProfiles);

    state->inactDetectionFromTx =
        profileGetOptsInt(moduleID,
                WLANIFBSTEERCONTROL_INACT_DETECTION_FROM_TX,
                defaultProfiles);

    state->nlSteerMulticast =
        profileGetOptsInt(moduleID,
                WLANIFBSTEERCONTROL_NL_STEER_MULTICAST_ENABLE,
                defaultProfiles);
    gnlSteerMulticast = state->nlSteerMulticast;

    state->ackRssiEnable =
        profileGetOptsInt(moduleID,
                WLANIFBSTEERCONTROL_ACK_RSSI_ENABLE,
                defaultProfiles);

    state->logModCounter =
        profileGetOptsInt(moduleID,
                WLANIFBSTEERCONTROL_LOG_MOD_COUNTER,
                defaultProfiles);
    return LBD_OK;
}

/**
 * @brief Function to set mod counter which is used to control
 *        print messages when ack_rssi is enabled
 *
 * @param [in] state  the "this" pointer
 * @param [in] modCounter (1 ... INT_MAX)
 *             1 - prints all
 *             INT_MAX - prints less
 *
 */
LBD_STATUS wlanifBSteerControlGetLocalModCounter(
        wlanifBSteerControlHandle_t state,
        u_int32_t *modCounter) {
    if (state->logModCounter < 1 || state->logModCounter > INT_MAX) {
        state->logModCounter = 1;
    }
    *modCounter = state->logModCounter;
    dbgf(state->dbgModule, DBGINFO, "state->logModCounter=%d\n", state->logModCounter);
    return LBD_OK;
}

/**
 * @brief Function to get whether ack_rssi option
 *        is enabled or not for band steering.
 *
 * @param [in] state  the "this" pointer
 * @param [in] ack_rssi enable option
 *
 */
LBD_STATUS wlanifBSteerControlGetLocalAckRssiEnable(
        wlanifBSteerControlHandle_t state,
        u_int8_t *ack_rssi_enable) {
    *ack_rssi_enable = state->ackRssiEnable;

    return LBD_OK;
}

/**
 * @brief Function to get whether multicast
 *        option is enabled on NL socket
 *
 * @param [in] state  the "this" pointer
 * @param [out]  multicast  enable option
 *
 */
LBD_STATUS wlanifBSteerControlGetLocalNLSteerMulticast(
        wlanifBSteerControlHandle_t state,
        u_int8_t *nlSteerMulticast) {
    *nlSteerMulticast = state->nlSteerMulticast;

    return LBD_OK;
}

/**
 * @brief Function to check whether multicast
 *        is enabled or not on NL socket
 *
 * @param [out] flag  read value
 *
 */
void isMulticastEnabled(u_int8_t *nlMulticast) {
    *nlMulticast = gnlSteerMulticast;
}

/**
 * @brief Set the SON value on all interfaces on the given band to the desired
 *        value.
 *
 * Note that this functionally does nothing other than cause a bit to be
 * set in the Whole Home Coverage AP Info IE included at association time.
 *
 * @param [in] state  the "this" pointer
 * @param [in] band  the band on which to send this request
 * @param [in] sonEnabled  the value to set it to
 *
 * @return LBD_OK if the value was set successfully on all interfaces;
 *         otherwise LBD_NOK
 */
static LBD_STATUS wlanifBSteerControlCmnSetSON(
        wlanifBSteerControlHandle_t state, wlanif_band_e band,
        LBD_BOOL sonEnabled) {
    size_t vapIndex;
    for (vapIndex = 0; vapIndex < MAX_VAP_PER_BAND; ++vapIndex) {
        struct wlanifBSteerControlVapInfo *vap =
            &state->bandInfo[band].vaps[vapIndex];
        if (!vap->valid) {
            // No more valid VAPs, can exit the loop
            break;
        }

        if (vap->radio->sonIoctl) {
            if (wlanifBSteerControlCmnPrivIoctlSetParam(
                        state, vap->ifname, vap->radio->sonIoctl,
                        sonEnabled) != LBD_OK) {
                // Error will already hvae been printed.
                return LBD_NOK;
            }
        }
    }

    return LBD_OK;
}

/**
 * @brief Send IEEE80211_DBGREQ_BSTEERING_ENABLE_ACK_RSSI on each
 *        included VAP on the radio.
 *
 * @param [in] state  the "this" pointer
 * @param [in] band  The band on which to send this request
 * @param [in] ignoreError  set to LBD_TRUE if ignoring errors
 *
 * @return LBD_OK if the request is sent sucessfully; otherwise LBD_NOK
 */
LBD_STATUS
wlanifBSteerControlCmnSendAckRSSIEnable(wlanifBSteerControlHandle_t state,
                                 wlanif_band_e band, LBD_BOOL ignoreError) {

    u_int8_t ackrssi_enable;
    LBD_STATUS returnCode = LBD_OK;

    ackrssi_enable = state->ackRssiEnable ? 1 : 0;

    size_t vap;
    for (vap = 0; vap < MAX_VAP_PER_BAND; ++vap) {
        if (!state->bandInfo[band].vaps[vap].valid) {
            // No more valid VAPs, can exit the loop
            break;
        }

        if (state->bandInfo[band].vaps[vap].includedIface) {
#if !SONLIB_SUPPORT_ENABLED
            if (wlanifBSteerControlCmnSendVAP(
                state, state->bandInfo[band].vaps[vap].ifname,
                IEEE80211_DBGREQ_BSTEERING_ENABLE_ACK_RSSI, NULL,
                (void *) &ackrssi_enable,
                sizeof(ackrssi_enable), NULL, 0, ignoreError) != LBD_OK) {

                if (!ignoreError) {
                    return LBD_NOK;
                } else {
                    returnCode = LBD_NOK;
                }
            }
#endif
            dbgf(state->dbgModule, DBGINFO, "%s: ifname=%s,ackrssi_enable=%d",
                __func__, state->bandInfo[band].vaps[vap].ifname,
                          ackrssi_enable);
        }
    }
    return returnCode;
}

/**
 * @brief Send IEEE80211_DBGREQ_BSTEERING_ENABLE request on the
 *        first VAP (to enable on the radio level), and
 *        IEEE80211_DBGREQ_BSTEERING_ENABLE_EVENTS on each VAP
 *        on the radio.
 *
 * @param [in] state  the "this" pointer
 * @param [in] band  The band on which to send this request
 * @param [in] enable  LBD_TRUE for enable, LBD_FALSE for disable
 * @param [in] ignoreError  set to LBD_TRUE if ignoring errors
 *
 * @return LBD_OK if the request is sent sucessfully; otherwise LBD_NOK
 */
static LBD_STATUS
wlanifBSteerControlCmnSendEnable(wlanifBSteerControlHandle_t state,
                                 wlanif_band_e band, LBD_BOOL enable,
                                 LBD_BOOL ignoreError) {
    u_int8_t bsteering_enable;
    LBD_STATUS returnCode = LBD_OK;

    bsteering_enable = enable ? 1 : 0;

    // On enable: do the radio level enable first
    if (enable) {
        if (wlanifBSteerControlCmnSendFirstVAP(state, band,
                                           IEEE80211_DBGREQ_BSTEERING_ENABLE, NULL,
                                           (void *) &bsteering_enable,
                                           sizeof(bsteering_enable),
                                           ignoreError) != LBD_OK) {
            return LBD_NOK;
        }
    }

    // Now do the individual enable / disable per VAP
    size_t vap;
    for (vap = 0; vap < MAX_VAP_PER_BAND; ++vap) {
        if (!state->bandInfo[band].vaps[vap].valid) {
            // No more valid VAPs, can exit the loop
            break;
        }

        if (state->bandInfo[band].vaps[vap].includedIface) {
#if SONLIB_SUPPORT_ENABLED
            struct soncmnDrvHandle *drvHandle = state->bandInfo[band].vaps[vap].drvHandle;
            if(drvHandle->ops->setBSteerEvents(drvHandle->ctx,
                  state->bandInfo[band].vaps[vap].ifname, bsteering_enable) != LBD_OK) {
#else
            if (wlanifBSteerControlCmnSendVAP(
                state, state->bandInfo[band].vaps[vap].ifname,
                IEEE80211_DBGREQ_BSTEERING_ENABLE_EVENTS, NULL,
                (void *) &bsteering_enable,
                sizeof(bsteering_enable), NULL, 0, ignoreError) != LBD_OK) {
#endif
                if (!ignoreError) {
                    return LBD_NOK;
                } else {
                    returnCode = LBD_NOK;
                }
            }
        }
    }

    // On disable: do the radio level disable last
    if (!enable) {
        if (wlanifBSteerControlCmnSendFirstVAP(state, band,
                                           IEEE80211_DBGREQ_BSTEERING_ENABLE, NULL,
                                           (void *) &bsteering_enable,
                                           sizeof(bsteering_enable),
                                           ignoreError) != LBD_OK) {
            return LBD_NOK;
        }
    }

    return returnCode;
}

/**
 * @brief Send IEEE80211_DBGREQ_BSTEERING_SET_PARAMS request
 *
 * @param [in] state  the "this" pointer
 * @param [in] band  The band on which to send this request
 *
 * @return LBD_OK if the request is sent sucessfully; otherwise LBD_NOK
 */
static LBD_STATUS
wlanifBSteerControlCmnSendSetParams(wlanifBSteerControlHandle_t state,
                                 wlanif_band_e band) {
    return wlanifBSteerControlCmnSendFirstVAP(state, band, IEEE80211_DBGREQ_BSTEERING_SET_PARAMS, NULL,
                                           (void *) &state->bandInfo[band].configParams,
                                           sizeof(state->bandInfo[band].configParams),
                                           LBD_FALSE /* ignoreError */);
}

/**
 * @brief Send IEEE80211_DBGREQ_BSTEERING_GET_RSSI request
 *
 * @param [in] state  the "this" pointer
 * @param [in] band  The band on which to send this request
 * @param [in] staAddr  the MAC address of the station to request RSSI
 * @param [in] numSamples  number of RSSI measurements to average before reporting back
 *
 * @return LBD_OK if the request is sent sucessfully; otherwise LBD_NOK
 */
static LBD_STATUS
wlanifBSteerControlCmnSendRequestRSSI(wlanifBSteerControlHandle_t state,
                                   struct wlanifBSteerControlVapInfo *vap,
                                   const struct ether_addr * staAddr, u_int8_t numSamples) {
#if SONLIB_SUPPORT_ENABLED
    return (vap->drvHandle->ops->getBSteerRSSI(vap->drvHandle->ctx,
                 vap->ifname, staAddr->ether_addr_octet, numSamples));
#else
    return wlanifBSteerControlCmnSetSendVAP(
        state, vap->ifname,
        IEEE80211_DBGREQ_BSTEERING_GET_RSSI,
        staAddr, (void *) &numSamples, sizeof(numSamples),
        LBD_FALSE /* ignoreError */);
#endif
}

/**
 * @brief Send 802.11k beacon report request
 *
 * If multiple channels are provided, current implementation will try to
 * send separate request with each channel until success.
 *
 * @param [in] state  the "this" pointer
 * @param [in] vap  the VAP to send the request
 * @param [in] staAddr  the MAC address of the specific station
 * @param [in] rrmCapable  flag indicating if the STA implements 802.11k feature
 * @param [in] numChannels  number of channels in channelList
 * @param [in] channelList  set of channels to measure downlink RSSI
 * @param [in] clientClassGroup client classification group the client belongs to
 *
 * @return  LBD_OK if the request is sent successfully; otherwise LBD_NOK
 */
static LBD_STATUS wlanifBSteerControlCmnSendRRMBcnrptRequest(
        wlanifBSteerControlHandle_t state,
        struct wlanifBSteerControlVapInfo *vap,
        const struct ether_addr *staAddr, size_t numChannels,
        const lbd_channelId_t *channels, u_int8_t clientClassGroup) {
#if SONLIB_SUPPORT_ENABLED
    struct soncmn_ieee80211_rrm_beaconreq_info_t bcnrpt = {0};
#else
    ieee80211_rrm_beaconreq_info_t bcnrpt = {0};
#endif
    LBD_STATUS status = LBD_NOK;
    size_t i;

    // For multiple channels, based on current testing, it's more reliable
    // to send a request for each channel, rather than relying on set 255
    // as channel number and append extra AP info element.
    for (i = 0; i < numChannels; ++i) {
        // Only handle single channel per band for now
        bcnrpt.channum = channels[i];
        if (LBD_OK != wlanif_resolveRegClass(bcnrpt.channum, &bcnrpt.regclass)) {
            dbgf(state->dbgModule, DBGERR, "%s: Failed to resolve regulatory class from channel %d",
                 __func__, bcnrpt.channum);
            return LBD_NOK;
        }
        bcnrpt.req_ssid = 1;
        memset(bcnrpt.bssid, 0xff, ETH_ALEN);
        switch (bcnrpt.regclass) {
            case IEEE80211_RRM_REGCLASS_112:
            case IEEE80211_RRM_REGCLASS_115:
            case IEEE80211_RRM_REGCLASS_125:
                bcnrpt.mode = IEEE80211_RRM_BCNRPT_MEASMODE_ACTIVE;
                bcnrpt.duration = state->bandInfo[wlanif_band_5g].bcnrptDurations[bcnrpt.mode][clientClassGroup];
                break;
            case IEEE80211_RRM_REGCLASS_81:
            case IEEE80211_RRM_REGCLASS_82:
                bcnrpt.mode = IEEE80211_RRM_BCNRPT_MEASMODE_ACTIVE;
                bcnrpt.duration = state->bandInfo[wlanif_band_24g].bcnrptDurations[bcnrpt.mode][clientClassGroup];
                break;
            case IEEE80211_RRM_REGCLASS_118:
            case IEEE80211_RRM_REGCLASS_121:
                bcnrpt.mode = IEEE80211_RRM_BCNRPT_MEASMODE_PASSIVE;
                bcnrpt.duration = state->bandInfo[wlanif_band_5g].bcnrptDurations[bcnrpt.mode][clientClassGroup];
                break;
            default:
                dbgf(state->dbgModule, DBGERR, "%s: Invalid regulatory class %d",
                     __func__, bcnrpt.regclass);
                return LBD_NOK;
        }

#if SONLIB_SUPPORT_ENABLED
        if (LBD_OK == vap->drvHandle->ops->sendBcnRptReq(vap->drvHandle->ctx,
                              vap->ifname, staAddr->ether_addr_octet, &bcnrpt)) {
#else
        if (LBD_OK == wlanifBSteerControlCmnSetSendVAP(
                          state, vap->ifname,
                          IEEE80211_DBGREQ_SENDBCNRPT, staAddr,
                          (void *) &bcnrpt, sizeof(bcnrpt),
                          LBD_FALSE /* ignoreError */)) {
#endif
            // Based on our testing, most devices will reject multiple requests sent in a short
            // interval, so for now we only ensure one request is sent.
            status = LBD_OK;
            break;
        }
    }
    return status;
}

/**
 * @brief Send request down to driver using ioctl() on the first
 *        VAP operating on the specified band for each radio operating
 *        on that band.
 *
 * Note that this function will attempt the operation on all radios on the
 * band even if it fails on one of them.
 *
 * @param [in] band  the band on which this request should be sent
 * @param [in] cmd  the command contained in the request
 * @param [in] destAddr  optional parameters to specify the dest client of this
 *                       request
 * @param [in] data  the data contained in the request
 * @param [in] data_len  the length of data contained in the request
 * @param [in] ignoreError  set to LBD_TRUE if ignoring errors
 *
 * @return LBD_OK if the request is sent successfully on all radios on the
 *         band; otherwise LBD_NOK
 */
static inline LBD_STATUS
wlanifBSteerControlCmnSendFirstVAP(wlanifBSteerControlHandle_t state,
                                wlanif_band_e band, u_int8_t cmd,
                                const struct ether_addr *destAddr,
                                void *data, int data_len, LBD_BOOL ignoreError) {
    LBD_STATUS result = LBD_OK;
    size_t i;
    for (i = 0; i < WLANIF_MAX_RADIOS; ++i) {
        if (!state->radioInfo[i].valid) {
            break;
        } else if (wlanif_resolveBandFromChannelNumber(
                    state->radioInfo[i].channel) == band) {
            struct wlanifBSteerControlVapInfo *vap =
                wlanifBSteerControlCmnGetFirstVAPByRadio(
                        state, &state->radioInfo[i]);
            if (!vap) {
                dbgf(state->dbgModule, DBGERR,
                     "%s: Failed to resolve VAP for radio [%s]",
                     __func__, state->radioInfo[i].ifname);
                result = LBD_NOK;
#if SONLIB_SUPPORT_ENABLED
            } else {
                struct soncmnDrvHandle *drvHandle = vap->drvHandle;
                if (!drvHandle) {
                    dbgf(state->dbgModule, DBGERR,"%s:, drvHandle is NULL\n", __func__);
                    result = LBD_NOK;
                    break;
                }
                if (cmd == IEEE80211_DBGREQ_BSTEERING_ENABLE) {
                    if(drvHandle->ops->setBSteering(drvHandle->ctx, vap->ifname, *(uint8_t *)data) != LBD_OK)
                        result = LBD_NOK;
                }
                if (cmd == IEEE80211_DBGREQ_BSTEERING_SET_PARAMS) {
                    if(drvHandle->ops->setBSteerParams(drvHandle->ctx, vap->ifname, data, data_len) != LBD_OK)
                        result = LBD_NOK;
                }
                if (result == LBD_NOK) {
                    if (!ignoreError) {
                        dbgf(state->dbgModule, DBGERR,
                         "%s: Failed to execute command [%u] on %s (errno=%d)\n",
                         __func__, cmd, vap->ifname, errno);
                    }
                } else {
                    dbgf(state->dbgModule, DBGDEBUG,
                         "%s: Successfully executed command [%u] on %s\n",
                         __func__, cmd, vap->ifname);
                }
            }
#else
            } else if (wlanifBSteerControlCmnSetSendVAP(state, vap->ifname, cmd,
                                                        destAddr, data,
                                                        data_len,
                                                        ignoreError) == LBD_OK) {
                dbgf(state->dbgModule, DBGDEBUG,
                     "%s: Successfully executed command [%u] on %s\n",
                     __func__, cmd, vap->ifname);

            } else {
                if (!ignoreError) {
                    dbgf(state->dbgModule, DBGERR,
                         "%s: Failed to execute command [%u] on %s (errno=%d)\n",
                         __func__, cmd, vap->ifname, errno);
                }
                result = LBD_NOK;
            }
#endif
        } /* if radio valid */
    } /* for */

    return result;
}

#if !SONLIB_SUPPORT_ENABLED
//These functions are not used when SONLIB is enabled
/**
 * @brief Send request down to driver using ioctl() for GET operations.
 *
 * This function will record the ioctl() output on success.
 *
 * @see wlanifBSteerControlCmnSendVAP for parameters and return value explanation
 */
static LBD_STATUS
wlanifBSteerControlCmnGetSendVAP(wlanifBSteerControlHandle_t state,
                              const char *ifname, u_int8_t cmd,
                              const struct ether_addr *destAddr,
                              void *output, int output_len) {
    return wlanifBSteerControlCmnSendVAP(
        state, ifname, cmd, destAddr, NULL /* data */, 0 /* data_len */,
        output, output_len, LBD_FALSE /* ignoreError */);
}

/**
 * @brief Send request down to driver using ioctl() for SET operations.
 *
 * @see wlanifBSteerControlCmnSendVAP for parameters and return value explanation
 */
LBD_STATUS
wlanifBSteerControlCmnSetSendVAP(wlanifBSteerControlHandle_t state,
                                 const char *ifname, u_int8_t cmd,
                                 const struct ether_addr *destAddr,
                                 void *data, int data_len,
                                 LBD_BOOL ignoreError) {
    return wlanifBSteerControlCmnSendVAP(
        state, ifname, cmd, destAddr, data,
        data_len, NULL /* output */, 0 /* output_len */, ignoreError);
}

/**
 * @brief Send request down to driver using ioctl()
 *
 * @param [in] ifname  the name of the interface on which this request should
 *                     be done
 * @param [in] cmd  the command contained in the request
 * @param [in] destAddr  optional parameters to specify the dest client of this request
 * @param [in] data  the data contained in the request
 * @param [in] data_len  the length of data contained in the request
 * @param [out] output  if not NULL, fill in the response data from the request
 * @param [in] output_len  expected number of bytes of output
 * @param [in] ignoreError  set to LBD_FALSE to print an error
 *                          if the send is not successful, set
 *                          to LBD_TRUE if no error message
 *                          should be printed
 *
 * @return LBD_OK if the request is sent successfully; otherwise LBD_NOK
 */
static LBD_STATUS
wlanifBSteerControlCmnSendVAP(wlanifBSteerControlHandle_t state,
                              const char *ifname, u_int8_t cmd,
                              const struct ether_addr *destAddr,
                              void *data, int data_len,
                              void *output, int output_len,
                              LBD_BOOL ignoreError) {
    struct ieee80211req_athdbg req;
    memset(&req, 0, sizeof(struct ieee80211req_athdbg));

    if (destAddr) {
        lbCopyMACAddr(destAddr->ether_addr_octet, req.dstmac);
    }

    req.cmd = cmd;
    if (data) {
        memcpy(&req.data, data, data_len);
    }
    dbgf(state->dbgModule, DBGDUMP, "%s: getDbgreq cmd %d ifname %s\n",__func__,cmd,ifname);
    if( wlanIfLb->ops->getDbgreq(wlanIfLb->ctx, ifname,(void *) &req, (sizeof(struct ieee80211req_athdbg))) < 0) {
        if (!ignoreError) {
            dbgf(state->dbgModule, DBGERR,
                 "%s: Send %d request failed (errno=%d) on %s",
                 __func__, cmd, errno, ifname);

        }

        /* if the state is already set, don't treat it as error */
        if(cmd == IEEE80211_DBGREQ_BSTEERING_ENABLE_EVENTS && errno == EALREADY) {
            return LBD_OK;
        } else {
            return LBD_NOK;
        }
    }

    if (output) {
        lbDbgAssertExit(state->dbgModule, output_len <= sizeof(req.data));
        memcpy(output, &req.data, output_len);
    }
    return LBD_OK;
}
#endif
/**
 * @brief Dump the associated STAs for a single interface
 *
 * @param [in] state the 'this' pointer
 * @param [in] vap  the vap to dump associated STAs for
 * @param [in] band  the current band on which the interface is operating
 * @param [in] callback  the callback function to invoke for each associated
 *                       STA
 * @param [in] cookie  the parameter to provide back in the callback
 *
 * @return LBD_OK if the dump succeeded on this interface; otherwise LBD_NOK
 */
#if !SONLIB_SUPPORT_ENABLED
static LBD_STATUS wlanifBSteerControlCmnDumpAssociatedSTAsOneIface(
        wlanifBSteerControlHandle_t state,
        struct wlanifBSteerControlVapInfo *vap, wlanif_band_e band,
        wlanif_associatedSTAsCB callback, void *cookie) {
#define LIST_STATION_IOC_ALLOC_SIZE 24*1024
#define LIST_STATION_CFG_ALLOC_SIZE (3*1024)
    u_int8_t *buf = NULL;
    int length = 0;

    if(wlanIfLb->IsCfg80211) {
        length = LIST_STATION_CFG_ALLOC_SIZE;
    } else {
        length = LIST_STATION_IOC_ALLOC_SIZE;
    }

    buf = malloc(length);
    if (!buf) {
        dbgf(state->dbgModule, DBGERR,
             "%s: Failed to allocate buffer for iface %s", __func__, vap->ifname);
        return LBD_NOK;
    }

    LBD_STATUS result = LBD_OK;
    do {
        if(wlanIfLb->ops->getStationInfo(wlanIfLb->ctx, vap->ifname, buf, &length) < 0) {
            dbgf(state->dbgModule, DBGERR,
                 "%s: Failed to perform ioctl for iface %s", __func__, vap->ifname);
            result = LBD_NOK;
            break;
        }
        struct timespec curTime;
        lbGetTimestamp(&curTime);

        // Loop over all of the STAs, providing a callback for each one.
        u_int8_t *currentPtr = buf;
        u_int8_t *endPtr = buf + length;

        dbgf(state->dbgModule, DBGINFO,"%s: station info length %d\n", __func__, length);
        length = 0;
        while (currentPtr + sizeof(struct ieee80211req_sta_info) <= endPtr) {
            const struct ieee80211req_sta_info *staInfo =
                (const struct ieee80211req_sta_info *) currentPtr;
            struct ether_addr addr;
            u_int8_t client_class_group = 0;
            struct ieee80211req_athdbg req = {0};
            lbCopyMACAddr(staInfo->isi_macaddr, addr.ether_addr_octet);
            ieee80211_bsteering_datarate_info_t datarateInfo;
            wlanif_phyCapInfo_t phyCapInfo = {
                LBD_FALSE /* valid */, wlanif_chwidth_invalid, 0 /* numStreams */,
                wlanif_phymode_invalid, 0 /* maxMCS */, 0 /* maxTxPower */
            };
            LBD_BOOL isStaticSMPS = LBD_FALSE;
            LBD_BOOL isMUMIMOSupported = LBD_FALSE;
            if (LBD_OK ==
                    wlanifBSteerControlCmnGetSendVAP(state, vap->ifname,
                                                  IEEE80211_DBGREQ_BSTEERING_GET_DATARATE_INFO,
                                                  &addr, (void *)&datarateInfo,
                                                  sizeof(ieee80211_bsteering_datarate_info_t))) {
                phyCapInfo.valid = LBD_TRUE;
                phyCapInfo.maxChWidth =
                        wlanifMapToBandwidth(state->dbgModule,
                                             (enum ieee80211_cwm_width)(datarateInfo.max_chwidth)),
                phyCapInfo.numStreams = datarateInfo.num_streams,
                phyCapInfo.phyMode =
                        wlanifMapToPhyMode(state->dbgModule,
                                           (enum ieee80211_phymode)datarateInfo.phymode),
                phyCapInfo.maxMCS =
                        wlanifConvertToSingleStreamMCSIndex(state->dbgModule,
                                (enum ieee80211_phymode)datarateInfo.phymode,
                                datarateInfo.max_MCS);
                phyCapInfo.maxTxPower = datarateInfo.max_txpower;
                isMUMIMOSupported = (LBD_BOOL)(datarateInfo.is_mu_mimo_supported);
                isStaticSMPS = (LBD_BOOL)(datarateInfo.is_static_smps);
            }
            lbd_bssInfo_t bss;

            bss.apId = LBD_APID_SELF;
            bss.channelId = vap->radio->channel;
            bss.essId = vap->essId;
            bss.vap = vap;

            req.cmd = IEEE80211_DBGREQ_BSTEERING_GET_PEER_CLASS_GROUP;
            req.needs_reply = DBGREQ_REPLY_IS_REQUIRED;
            lbCopyMACAddr(staInfo->isi_macaddr, req.dstmac);

            if (wlanIfLb->ops->getDbgreq(wlanIfLb->ctx, vap->ifname, (void *)&req,
                                       (sizeof(struct ieee80211req_athdbg))) < 0) {
                dbgf(state->dbgModule, DBGERR,
                     "%s: Failed to perform ioctl(%d) for iface %s",
                     __func__, IEEE80211_DBGREQ_BSTEERING_GET_PEER_CLASS_GROUP, vap->ifname);
            } else {
                client_class_group = req.data.bsteering_peer_class_group;
            }

            // When failed to get PHY capability info, still report the associated STA,
            // so we cannot estimate PHY capability for this STA but can still perform
            // other operations on it.
            callback(&addr, &bss, (LBD_BOOL)(staInfo->isi_ext_cap & IEEE80211_EXTCAPIE_BSSTRANSITION),
                     staInfo->isi_beacon_measurement_support, client_class_group,
                     isMUMIMOSupported,
                     isStaticSMPS, staInfo->isi_operating_bands, &phyCapInfo,
                     staInfo->isi_tr069_assoc_time.tv_sec, cookie);

            currentPtr += staInfo->isi_len;
        }
    } while (0);

    free(buf);
    return result;
}
#else
static LBD_STATUS wlanifBSteerControlCmnDumpAssociatedSTAsOneIface(
        wlanifBSteerControlHandle_t state,
        struct wlanifBSteerControlVapInfo *vap, wlanif_band_e band,
        wlanif_associatedSTAsCB callback, void *cookie)
{
#define CLIENT_CLASS 0
    struct soncmn_req_stainfo_t getStaInfo;
    int i;

    if (vap->drvHandle->ops->getStaInfo(vap->drvHandle->ctx, vap->ifname,
                                        &getStaInfo) != LBD_OK) {
      dbgf(state->dbgModule, DBGERR, "%s: getStaInfo() API call failed",
           __func__);
      return LBD_NOK;
    }

    struct timespec curTime;
    lbGetTimestamp(&curTime);

    /* Get each STA info */
    for (i=0; i < getStaInfo.numSta; i++)
    {
        lbd_bssInfo_t bss;
        struct ether_addr sta_addr;
        wlanif_phyCapInfo_t phyCapInfo = {
                LBD_FALSE /* valid */, wlanif_chwidth_invalid, 0 /* numStreams */,
                wlanif_phymode_invalid, 0 /* maxMCS */, 0 /* maxTxPower */ };
        LBD_BOOL isStaticSMPS = LBD_FALSE;
        LBD_BOOL isMUMIMOSupported = LBD_FALSE;

        /* Fill phyCapInfo */
        if ( getStaInfo.stainfo[i].phy_cap_valid )
        {
            phyCapInfo.valid = LBD_TRUE;
            phyCapInfo.maxChWidth =
                    wlanifMapToBandwidth(state->dbgModule,
                      (enum ieee80211_cwm_width)
                       (getStaInfo.stainfo[i].datarateInfo.max_chwidth));
            phyCapInfo.numStreams = getStaInfo.stainfo[i].datarateInfo.num_streams;
            phyCapInfo.phyMode =
                    wlanifMapToPhyMode(state->dbgModule,
                         (enum ieee80211_phymode)
                                 getStaInfo.stainfo[i].datarateInfo.phymode),
            phyCapInfo.maxMCS =
                wlanifConvertToSingleStreamMCSIndex(state->dbgModule,
                         (enum ieee80211_phymode)
                             getStaInfo.stainfo[i].datarateInfo.phymode,
                             getStaInfo.stainfo[i].datarateInfo.max_MCS);
            phyCapInfo.maxTxPower = getStaInfo.stainfo[i].datarateInfo.max_txpower;
            isMUMIMOSupported = (LBD_BOOL)(getStaInfo.stainfo[i].datarateInfo.is_mu_mimo_supported);
            isStaticSMPS = (LBD_BOOL)(getStaInfo.stainfo[i].datarateInfo.is_static_smps);
        } //valid phy cap

        bss.apId = LBD_APID_SELF;
        bss.channelId = vap->radio->channel;
        bss.essId = vap->essId;
        bss.vap = vap;
        lbCopyMACAddr(&getStaInfo.stainfo[i].stamac, sta_addr.ether_addr_octet);

        time_t assocAge = 0;

        if (curTime.tv_sec >
            getStaInfo.stainfo[i].isi_tr069_assoc_time.tv_sec) {
          assocAge = curTime.tv_sec -
                     getStaInfo.stainfo[i].isi_tr069_assoc_time.tv_sec;
        }

        // When failed to get PHY capability info, still report the associated STA,
        // so we cannot estimate PHY capability for this STA but can still perform
        // other operations on it.
        callback(&sta_addr, &bss, (LBD_BOOL)(getStaInfo.stainfo[i].isbtm),
                 (LBD_BOOL)(getStaInfo.stainfo[i].isrrm), CLIENT_CLASS, isMUMIMOSupported,
                 isStaticSMPS, getStaInfo.stainfo[i].operating_band,
                 &phyCapInfo, assocAge, cookie);
    }

    return LBD_OK;
#undef CLIENT_CLASS
}
#endif

/**
 * @brief Run an ioctl operation that takes a single MAC address on all
 *        interfaces that operate on the provided band.
 *
 * @param [in] state  the handle returned from wlanifBSteerControlCmnCreate()
 *                     to use for this operation
 * @param [in] ioctlReq  the operation to run
 * @param [in] band  the band on which to perform the operation
 * @param [in] staAddr  the MAC address of the STA
 *
 * @return LBD_OK if the operation was successful on all VAPs for that band;
 *         otherwise LBD_NOK
 */
static LBD_STATUS wlanifBSteerControlCmnPerformIoctlWithMAC(
        wlanifBSteerControlHandle_t state, int ioctlReq,
        struct wlanifBSteerControlVapInfo *vap,
        const struct ether_addr *staAddr) {

#if !SONLIB_SUPPORT_ENABLED
    struct sockaddr addr;
    int ret;

    memset(&addr, 0, sizeof(addr));
    addr.sa_family = ARPHRD_ETHER;
    lbCopyMACAddr(staAddr->ether_addr_octet, addr.sa_data);

    ret = wlanIfLb->ops->addDelKickMAC(wlanIfLb->ctx, vap->ifname, ioctlReq, (void *)&addr, sizeof(addr));
    if ((ret < 0) && (ret != -EEXIST) && (ret != -ENOENT)) {
        dbgf(state->dbgModule, DBGERR,
             "%s: addDelKickMAC IO 0x%04x failed with errno %u",
             __func__, ioctlReq, errno);
        return LBD_NOK;
    }

    return LBD_OK;
#else
    int result;
    switch (ioctlReq)
    {
        case IO_OPERATION_ADDMAC:
            result = vap->drvHandle->ops->setMacFiltering(vap->drvHandle->ctx,
                                vap->ifname, staAddr->ether_addr_octet, LBD_TRUE);
            break;
        case IO_OPERATION_DELMAC:
            result = vap->drvHandle->ops->setMacFiltering(vap->drvHandle->ctx,
                                vap->ifname, staAddr->ether_addr_octet, LBD_FALSE);
            break;
        case IO_OPERATION_KICKMAC:
            result = vap->drvHandle->ops->performKickMacDisassoc(vap->drvHandle->ctx,
                                             vap->ifname, staAddr->ether_addr_octet);
            break;
        default:
            /*Unsupported operation*/
            result = LBD_NOK;
            break;
    }

    if (result != LBD_OK ) {
        dbgf(state->dbgModule, DBGERR,
             "%s: Add MAC, Del MAC or Kick MAC IO 0x%04x failed with errno %u",
             __func__, ioctlReq, errno);
    }
    return result;
#endif
}

/**
 * @brief Run the maccmd ioctl with the provided value.
 *
 * @param [in] state  the handle returned from wlanifBSteerControlCmnCreate()
 *                     to use for this operation
 * @param [in] cmd  the command to inject
 * @param [in] band  the band on which to perform the operation
 *
 * @return LBD_OK if the operation was successful on all VAPs for that band;
 *         otherwise LBD_NOK
 */
static LBD_STATUS wlanifBSteerControlCmnPerformMacCmdOnBand(
        wlanifBSteerControlHandle_t state, int cmd, wlanif_band_e band) {
#if !SONLIB_SUPPORT_ENABLED
    int params[2] = { IEEE80211_PARAM_MACCMD, cmd };
#endif
    size_t i;
    for (i = 0; i < MAX_VAP_PER_BAND; ++i) {
        if (state->bandInfo[band].vaps[i].valid) {
#if SONLIB_SUPPORT_ENABLED
            struct soncmnDrvHandle *drvHandle = state->bandInfo[band].vaps[i].drvHandle;
            if (drvHandle->ops->setAclPolicy(drvHandle->ctx,
                    state->bandInfo[band].vaps[i].ifname, cmd) < 0) {
#else
            if(wlanIfLb->ops->setParamMaccmd(wlanIfLb->ctx,
                   state->bandInfo[band].vaps[i].ifname, (void*)&params[0], (sizeof(int) * 2)) < 0) {
#endif
                dbgf(state->dbgModule, DBGERR,
                     "%s: ioctl (cmd=%u) failed with errno %u",
                     __func__, cmd, errno);
                return LBD_NOK;
            }
        }
    }

    return LBD_OK;
}

/**
 * @brief Timeout handler that checks if the VAPs are ready and re-enables
 *        band steering if they are.
 *
 * @param [in] cookie  the state object for wlanifBSteerControlCmn
 */
static void wlanifBSteerControlCmnAreVAPsReadyTimeoutHandler(void *cookie) {
    wlanifBSteerControlHandle_t state =
        (wlanifBSteerControlHandle_t) cookie;

    LBD_BOOL enabled = LBD_FALSE;
    LBD_STATUS ret;

    dbgf(state->dbgModule, DBGERR,"%s: TIMEOUT\n",__func__);

    /* lock the call to avoid race condition between hyd daemons. */
    int fd_lock = open(HYD_MY_LOCK, O_CREAT);
    if(fd_lock >= 0) {
        flock(fd_lock, LOCK_EX);
    }

    ret = wlanifBSteerControlEnableWhenReady(state, &enabled);

    if(fd_lock >= 0) {
        flock(fd_lock, LOCK_UN);
        close(fd_lock);
    }

    if (ret == LBD_NOK)  {
        dbgf(state->dbgModule, DBGERR,
             "%s: Re-enabling on both bands failed", __func__);
        fprintf(stderr,"%s: LBD Exiting\n",__func__);
        fflush(stderr);
        fprintf(stderr,"%s: LBD Exiting\n",__func__);
        exit(1);
    }
}

/**
 * @brief Determine if the interface state indicates that it is up.
 *
 * @param [in] state  the handle returned from wlanifBSteerControlCmnCreate()
 *                     to use for this operation
 * @param [in] ifname  the name of the interface to check
 *
 * @return LBD_TRUE if the VAPs are ready; otherwise LBD_FALSE
 */
#if SONLIB_SUPPORT_ENABLED
static LBD_BOOL wlanifBSteerControlCmnIsLinkUp(
        wlanifBSteerControlHandle_t state, const char *ifname,
                            struct soncmnDrvHandle *drvHandle) {
#else
static LBD_BOOL wlanifBSteerControlCmnIsLinkUp(
        wlanifBSteerControlHandle_t state, const char *ifname) {
#endif
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));

    strlcpy(ifr.ifr_name, ifname, IFNAMSIZ);
#if SONLIB_SUPPORT_ENABLED
    if (drvHandle->ops->getIfFlags(drvHandle->ctx, ifname, &ifr) < 0) {
#else
    if (ioctl(state->controlSock, SIOCGIFFLAGS, &ifr) < 0) {
#endif
        dbgf(state->dbgModule, DBGERR,
             "%s: Failed to get interface flags for %s",
             __func__, ifname);
        return LBD_FALSE;
    }

    return (ifr.ifr_flags & IFF_RUNNING) != 0;
}

/**
 * @brief Determine if a WiFI interface has a valid BSSID.
 *
 * @param [in] state  the handle returned from wlanifBSteerControlCmnCreate()
 *                     to use for this operation
 * @param [in] ifname  the name of the interface to check
 *
 * @return LBD_TRUE if the VAP has a valid BSSID; otherwise
 *         LBD_FALSE
 */
#if SONLIB_SUPPORT_ENABLED
static LBD_BOOL wlanifBSteerControlCmnIsVAPAssociated(
        wlanifBSteerControlHandle_t state, const char *ifname,
                            struct soncmnDrvHandle *drvHandle) {
#else
static LBD_BOOL wlanifBSteerControlCmnIsVAPAssociated(
        wlanifBSteerControlHandle_t state, const char *ifname) {
#endif

    struct ether_addr zeroAddr = {{0,0,0,0,0,0}};
    static const struct ether_addr tempAddr = {{0,0,0,0,0,0}};

#if SONLIB_SUPPORT_ENABLED
    if (drvHandle->ops->getBSSID(drvHandle->ctx , ifname, &zeroAddr ) < 0) {
#else
    if (wlanIfLb->ops->getBSSID(wlanIfLb->ctx , ifname, &zeroAddr ) < 0) {
#endif
        dbgf(state->dbgModule, DBGERR,
             "%s: Failed to get WAP status for %s",
             __func__, ifname);
        return LBD_FALSE;
    }

    // An all-zeros address is returned if the interface does not have a valid BSSID
    if (lbAreEqualMACAddrs(&tempAddr.ether_addr_octet, &zeroAddr.ether_addr_octet)) {
        dbgf(state->dbgModule, DBGDEBUG,
             "%s: Interface %s does not have a valid BSSID",
             __func__, ifname);
        return LBD_FALSE;
    } else {
        dbgf(state->dbgModule, DBGDEBUG,
             "%s: Interface %s has BSSID " lbMACAddFmt(":"),
             __func__, ifname, lbMACAddData(&zeroAddr.ether_addr_octet));
    }

    return LBD_TRUE;
}

/**
 * @brief Determine whether all VAPs are ready for band steering to be
 *        enabled.
 *
 * @pre state has already been checked for validity
 *
 * @param [in] state  the handle returned from wlanifBSteerControlCmnCreate()
 *                     to use for this operation
 *
 * @return LBD_TRUE if the VAPs are ready; otherwise LBD_FALSE
 */
static LBD_BOOL wlanifBSteerControlCmnAreAllVAPsReady(
        wlanifBSteerControlHandle_t state) {

    size_t i, j;

    for (i = 0; i < wlanif_band_invalid; ++i) {
        // if there are multiple vaps configured for a band, it is considered
        // to be ready as long as one vap is ready.
        LBD_BOOL bandReady = LBD_FALSE;

        size_t numValidVAPs = 0;
        for (j = 0; j < MAX_VAP_PER_BAND; ++j) {
            if (!state->bandInfo[i].vaps[j].valid) {
                break;
            }

            numValidVAPs++;

            // First check that the link is up. It may not be if the
            // operator made the interface administratively down.
#if SONLIB_SUPPORT_ENABLED
            //Get drvHandle for this VAP
            struct soncmnDrvHandle *drvHandle = state->bandInfo[i].vaps[j].drvHandle;
            if (!wlanifBSteerControlCmnIsLinkUp(state,state->bandInfo[i].vaps[j].ifname, drvHandle)){
#else
            if (!wlanifBSteerControlCmnIsLinkUp(state,state->bandInfo[i].vaps[j].ifname)){
#endif
                dbgf(state->dbgModule, DBGDEBUG, "%s: vap:%s link up check failed.",
                     __func__, state->bandInfo[i].vaps[j].ifname);
                continue;
            }

            // Check the VAP has a valid BSSID.  This will always be immediately
            // true for an interface on a radio that only has AP VAPs.
            // For a VAP on a radio that has a STA VAP (range extender mode)
            // it will not be true until the STA associates with the CAP.
#if SONLIB_SUPPORT_ENABLED
            if (!wlanifBSteerControlCmnIsVAPAssociated(state,
                                                    state->bandInfo[i].vaps[j].ifname,
                                                    drvHandle)) {
#else
            if (!wlanifBSteerControlCmnIsVAPAssociated(state,state->bandInfo[i].vaps[j].ifname)){
#endif
                dbgf(state->dbgModule, DBGDEBUG, "%s: vap:%s associated check failed",
                     __func__,  state->bandInfo[i].vaps[j].ifname);
                continue;
            }

            // This may not be necessary, but it should be more efficient
            // than checking it first.
            state->bandInfo[i].vaps[j].ifaceUp = LBD_TRUE;

            // This parameter is small enough that it can fit in the name union
            // member.

            int acsState = 0;
#if SONLIB_SUPPORT_ENABLED
            if(drvHandle->ops->getAcsState(drvHandle->ctx, state->bandInfo[i].vaps[j].ifname, &acsState) < 0) {
#else
            if(wlanIfLb->ops->getAcsState(wlanIfLb->ctx, state->bandInfo[i].vaps[j].ifname, &acsState) < 0) {
#endif
                dbgf(state->dbgModule, DBGERR,
                     "%s: GET_ACS failed on %s with errno %u",
                     __func__, state->bandInfo[i].vaps[j].ifname, errno);
                continue;
            }

            if (acsState) {
                dbgf(state->dbgModule, DBGINFO,
                     "%s: ACS scan in progress on %s",
                     __func__, state->bandInfo[i].vaps[j].ifname);
                continue;
            }

            int cacState = 0;
#if SONLIB_SUPPORT_ENABLED
            if(drvHandle->ops->getCacState(drvHandle->ctx, state->bandInfo[i].vaps[j].ifname, &cacState) < 0) {
#else
            if(wlanIfLb->ops->getCacState(wlanIfLb->ctx, state->bandInfo[i].vaps[j].ifname, &cacState) < 0) {
#endif
                dbgf(state->dbgModule, DBGERR,
                     "%s: GET_CAC failed on %s with errno %u",
                     __func__, state->bandInfo[i].vaps[j].ifname, errno);
                continue;
            }

            if (cacState) {
                dbgf(state->dbgModule, DBGINFO,
                     "%s: CAC in progress on %s",
                     __func__, state->bandInfo[i].vaps[j].ifname);
                continue;
            }

            // at least one vap passed the checking for this band
            bandReady = LBD_TRUE;
        }

        // return false if all the valid vaps are not ready for this band.
        if(!bandReady) {
            // To avoid filling the log with useless messages when no VAPs
            // are being managed, only generate an error when there is at
            // least one VAP and it is not ready.
            if (numValidVAPs) {
                dbgf(state->dbgModule, DBGDEBUG, "%s: no vap ready for band [%zd].",
                     __func__, i);
            }

            return LBD_FALSE;
        }

    }

    // Got this far, so all of them were ok.
    return LBD_TRUE;
}

/**
 * @brief Set a single private ioctl parameter at the radio level.
 *
 * The parameter is assumed to be an integer.
 *
 * @param [in] state  the handle returned from wlanifBSteerControlCmnCreate()
 *                     to use for this operation
 * @param [in] radio  the radio on which to set it
 * @param [in] paramId  the identifier for the parameter
 * @param [in] val  the value to set
 *
 * @return LBD_OK on success; otherwise LBD_NOK
 */
static LBD_STATUS wlanifBSteerControlCmnPrivIoctlSetParam(
        wlanifBSteerControlHandle_t state,
        const char *ifname, int paramId, int val) {
    if(wlanIfLb->ops->setParam(wlanIfLb->ctx, ifname, paramId, &val , sizeof(val)) < 0) {
        dbgf(state->dbgModule, DBGERR,
             "%s: Failed to set %04x parameter on %s",
             __func__, paramId, ifname);
        return LBD_NOK;
    }

    return LBD_OK;
}

/**
 * @brief Initialize the ACLs on the provided band (as being empty).
 *
 * @pre state and band are valid
 *
 * @param [in] state  the handle returned from wlanifBSteerControlCmnCreate()
 *                    to use for the initilization
 * @param [in] band  the band on which to initialize
 *
 * @return LBD_OK if the ACLs were initialized; otherwise LBD_NOK
 */
static LBD_STATUS wlanifBSteerControlCmnInitializeACLs(
        wlanifBSteerControlHandle_t state, wlanif_band_e band) {
    if (wlanifBSteerControlCmnPerformMacCmdOnBand(
                state, IEEE80211_MACCMD_FLUSH, band) == LBD_OK &&
        wlanifBSteerControlCmnPerformMacCmdOnBand(
            state, IEEE80211_MACCMD_POLICY_DENY, band) == LBD_OK) {
        return LBD_OK;
    }

    return LBD_NOK;
}

/**
 * @brief Clear the ACLs on the provided band.
 *
 * @pre state and band are valid
 *
 * @param [in] handle  the handle returned from wlanifBSteerControlCmnCreate()
 *                     to use for the teardown
 * @param [in] band  the band on which to teardown
 *
 * @return LBD_OK if the ACLs were flushed; otherwise LBD_NOK
 */
static void wlanifBSteerControlCmnFlushACLs(
        wlanifBSteerControlHandle_t state, wlanif_band_e band) {
    // Note that errors are ignored here, as we want to clean up all the
    // way regardless.
    wlanifBSteerControlCmnPerformMacCmdOnBand(
        state, IEEE80211_MACCMD_FLUSH, band);
}

/**
 * @brief Clear and disable the ACLs on the provided band.
 *
 * @pre state and band are valid
 *
 * @param [in] handle  the handle returned from wlanifBSteerControlCmnCreate()
 *                     to use for the teardown
 * @param [in] band  the band on which to teardown
 *
 * @return LBD_OK if the ACLs were torn down; otherwise LBD_NOK
 */
static void wlanifBSteerControlCmnTeardownACLs(
        wlanifBSteerControlHandle_t state, wlanif_band_e band) {
    // Note that errors are ignored here, as we want to clean up all the
    // way regardless.
    wlanifBSteerControlCmnPerformMacCmdOnBand(
        state, IEEE80211_MACCMD_FLUSH, band);
    wlanifBSteerControlCmnPerformMacCmdOnBand(
        state, IEEE80211_MACCMD_POLICY_OPEN, band);
}

/**
 * @brief Resolve PHY capability information for all VAPs
 *
 * @pre state is valid
 *
 * @param [in] state  the handle returned from wlanifBSteerControlCmnCreate()
 *                    to use for this operation
 *
 * @return LBD_OK if all VAPs' PHY capability info are resolved successfully;
 *         otherwise return LBD_NOK
 */
static LBD_STATUS wlanifBSteerControlCmnResolvePHYCapInfo(wlanifBSteerControlHandle_t state) {
#if SONLIB_SUPPORT_ENABLED
    struct soncmn_ieee80211_bsteering_datarate_info_t datarateInfo={0};
#else
    ieee80211_bsteering_datarate_info_t datarateInfo={0};
#endif
    size_t band, i;
    struct wlanifBSteerControlVapInfo *vap = NULL;
    for (band = wlanif_band_24g; band < wlanif_band_invalid; ++band) {
        for (i = 0; i < MAX_VAP_PER_BAND; ++i) {
            vap = &state->bandInfo[band].vaps[i];
            if (!vap->valid) {
                break;
            }
#if SONLIB_SUPPORT_ENABLED
            if (LBD_NOK == vap->drvHandle->ops->getDataRateInfo(vap->drvHandle->ctx,
                        vap->ifname, vap->macaddr.ether_addr_octet, &datarateInfo)) {
#else
            if (LBD_NOK == wlanifBSteerControlCmnGetSendVAP(
                    state, vap->ifname, IEEE80211_DBGREQ_BSTEERING_GET_DATARATE_INFO,
                    &vap->macaddr, (void *)&datarateInfo,
                    sizeof(ieee80211_bsteering_datarate_info_t))) {
#endif
                // Error has already been printed in wlanifBSteerControlCmnGetSendVAP function
                return LBD_NOK;
            }
            vap->phyCapInfo.valid = LBD_TRUE;
            vap->phyCapInfo.maxChWidth = wlanifMapToBandwidth(
                    state->dbgModule, (enum ieee80211_cwm_width)datarateInfo.max_chwidth),
            vap->phyCapInfo.numStreams = datarateInfo.num_streams;
            vap->phyCapInfo.phyMode = wlanifMapToPhyMode(
                    state->dbgModule, (enum ieee80211_phymode)datarateInfo.phymode),
            vap->phyCapInfo.maxMCS = wlanifConvertToSingleStreamMCSIndex(
                    state->dbgModule,
                    (enum ieee80211_phymode)datarateInfo.phymode,
                    datarateInfo.max_MCS);
            vap->phyCapInfo.maxTxPower = datarateInfo.max_txpower;
            dbgf(state->dbgModule, DBGDEBUG, "%s: Resolved PHY capability on %s: "
                 "maxChWidth %u, numStreams %u, phyMode %u maxMCS %u, maxTxPower %u",
                 __func__, vap->ifname, vap->phyCapInfo.maxChWidth,
                 vap->phyCapInfo.numStreams, vap->phyCapInfo.phyMode,
                 vap->phyCapInfo.maxMCS, vap->phyCapInfo.maxTxPower);
            if (vap->radio->maxTxPower && vap->phyCapInfo.maxTxPower != vap->radio->maxTxPower) {
                // Currently, on the same radio, Tx power is always the same for all VAPs.
                // Add a warning log here if it is no longer the case in the future.
                dbgf(state->dbgModule, DBGERR,
                     "%s: VAPs report different Tx power on %s",
                     __func__, vap->radio->ifname);
            }
            if (vap->phyCapInfo.maxTxPower > vap->radio->maxTxPower) {
                // If there is Tx power difference on the same radio, which should
                // not happen for now, the highest value will be used.
                vap->radio->maxTxPower = vap->phyCapInfo.maxTxPower;
            }
            if (vap->phyCapInfo.maxChWidth != wlanif_chwidth_invalid &&
                vap->phyCapInfo.maxChWidth > vap->radio->maxChWidth) {
                // If there is channel width different, the highest value will be used
                vap->radio->maxChWidth = vap->phyCapInfo.maxChWidth;
            }
        }
    }

    wlanifBSteerControlCmnFindStrongestRadioOnBand(state, wlanif_band_24g);
    wlanifBSteerControlCmnFindStrongestRadioOnBand(state, wlanif_band_5g);

    wlanifBSteerControlNotifyRadioOperChanChange(state);

    return LBD_OK;
}

/**
 * @brief Enable or disable association or just probe responses
 *        on a VAP
 *
 * @param [in] state the handle returned from
 *                   wlanifBSteerControlCmnCreate()
 * @param [in] vap  VAP to change the state on
 * @param [inout] cookie contains
 *                       wlanifBSteerControlCmnNonCandidateSet_t
 *
 * @return LBD_OK on success; LBD_NOK otherwise
 */
static LBD_STATUS wlanifBSteerControlCmnNonCandidateSetCB(
    wlanifBSteerControlHandle_t state,
    struct wlanifBSteerControlVapInfo *vap,
    void *cookie) {
    wlanifBSteerControlCmnNonCandidateSet_t *setParams =
        (wlanifBSteerControlCmnNonCandidateSet_t *)cookie;
    if (!setParams->probeOnly) {
        // Set association state
        u_int32_t operation = IO_OPERATION_DELMAC;
        if (!setParams->enable) {
            operation = IO_OPERATION_ADDMAC;
        }

        dbgf(state->dbgModule, DBGDEBUG,
            "%s: %s Blacklist for " lbMACAddFmt(":")
            " on vap %s", __func__,
            (setParams->enable?"Removing":"Installing"),
            lbMACAddData(setParams->staAddr->ether_addr_octet),
            vap->ifname);

        if (wlanifBSteerControlCmnPerformIoctlWithMAC(
            state, operation, vap, setParams->staAddr) != LBD_OK) {
            dbgf(state->dbgModule, DBGERR,
                 "%s: ioctl to change state to %d for " lbMACAddFmt(":")
                 "on interface %s failed with errno %u",
                 __func__, setParams->enable,
                 lbMACAddData(setParams->staAddr->ether_addr_octet),
                 vap->ifname,
                 errno);
            return LBD_NOK;
        }
    }
    // Only need to disable probe response witholding,
    // or enable if in probe only mode - it is enabled automatically when
    // enabling the VAP
    if ((!setParams->enable) || setParams->probeOnly) {
        u_int8_t bsteering_withhold, bsteering_auth;
        bsteering_withhold = setParams->enable ? 0 : 1;
        bsteering_auth = setParams->enable ? 0 : 1;

#if SONLIB_SUPPORT_ENABLED
        if(vap->drvHandle->ops->setBSteerProbeRespWH(vap->drvHandle->ctx,
                  vap->ifname, setParams->staAddr->ether_addr_octet, bsteering_withhold) != LBD_OK) {
#else
        if (wlanifBSteerControlCmnSetSendVAP(
            state, vap->ifname,
            IEEE80211_DBGREQ_BSTEERING_SET_PROBE_RESP_WH,
            setParams->staAddr, (void *) &bsteering_withhold,
            sizeof(bsteering_withhold),
            LBD_FALSE /* ignoreError */) != LBD_OK) {
#endif
            dbgf(state->dbgModule, DBGERR,
                 "%s: ioctl to set probe response status to %d for candidate "
                 " failed with errno %u",
                 __func__, setParams->enable, errno);
            return LBD_NOK;
        }

        if(state->auth[setParams->clientClassGroup]){
#if SONLIB_SUPPORT_ENABLED
            if(vap->drvHandle->ops->setBSteerAuthAllow(vap->drvHandle->ctx,
                vap->ifname, setParams->staAddr->ether_addr_octet, bsteering_auth) != LBD_OK) {
#else
            if (wlanifBSteerControlCmnSetSendVAP(
                        state, vap->ifname,
                        IEEE80211_DBGREQ_BSTEERING_SET_AUTH_ALLOW,
                        setParams->staAddr, (void *) &bsteering_auth,
                        sizeof(bsteering_auth),
                        LBD_FALSE /* ignoreError */) != LBD_OK) {
#endif
                dbgf(state->dbgModule, DBGERR,
                        "%s: ioctl to set authentication allow status to %d for candidate "
                        " failed with errno %u",
                        __func__, setParams->enable, errno);
                return LBD_NOK;
            }
        }
    }
    return LBD_OK;
}

/**
 * @brief Callback function to use when finding VAPs that match
 *        the ESS but aren't on the candidate list.
 *
 * @param [in] state  BSteerControl state
 * @param [in] vap  VAP found
 * @param [inout] cookie contains
 *                       wlanifBSteerControlCmnNonCandidateGet_t
 *
 * @return Always returns LBD_OK
 */
static LBD_STATUS wlanifBSteerControlCmnNonCandidateGetCB(
    wlanifBSteerControlHandle_t state,
    struct wlanifBSteerControlVapInfo *vap,
    void *cookie) {

    wlanifBSteerControlCmnNonCandidateGet_t *getParams =
        (wlanifBSteerControlCmnNonCandidateGet_t *)cookie;

    if (getParams->outCandidateCount >= getParams->maxCandidateCount) {
        return LBD_OK;
    }

    getParams->outCandidateList[getParams->outCandidateCount].apId =
        LBD_APID_SELF;
    getParams->outCandidateList[getParams->outCandidateCount].channelId =
        vap->radio->channel;
    getParams->outCandidateList[getParams->outCandidateCount].essId =
        vap->essId;
    getParams->outCandidateList[getParams->outCandidateCount].vap =
        vap;

    getParams->outCandidateCount++;

    return LBD_OK;
}

/**
 * @brief Find the set of VAPs on the same ESS as the candidate
 *        list but not matching the candidate list.
 *
 *        Will take action dependant on the callback function
 *        provided.
 *
 * @param [in] handle  the handle returned from
 *                     wlanifBSteerControlCmnCreate()
 * @param [in] candidateCount number of candidates in
 *                            candidateList
 * @param [in] candidateList set of candidate BSSes
 * @param [in] callback  callback function to call when an
 *                       appropriate VAP is found
 * @param [in] cookie cookie provided to callback function
 *
 * @return LBD_OK on success; LBD_NOK otherwise
 */
static LBD_STATUS wlanifBSteerControlCmnNonCandidateMatch(
    wlanifBSteerControlHandle_t state,
    u_int8_t candidateCount,
    const lbd_bssInfo_t *candidateList,
    wlanifBSteerControlCmnNonCandidateCB callback,
    void *cookie,
    const lbd_bssInfo_t *servingBSSInfo) {

    // Note can have an empty candidate list, but only if the candidateCount is
    // also 0
    if (!state  || (candidateCount && !candidateList)) {
        return LBD_NOK;
    }

    wlanif_band_e band;
    // If there are no candidates (which can occur if all candidates are remote
    // and could not be resolved), use the default ESSID
    lbd_essId_t essIdToMatch = LBD_ESSID_REMOTE_DEFAULT;

    if (candidateCount) {
        essIdToMatch = candidateList[0].essId;
    }

    // Disable or enable for all VAPs on the same ESS but not on the candidate list
    for (band = wlanif_band_24g; band < wlanif_band_invalid; band++) {
        int vap;
        for (vap = 0; vap < MAX_VAP_PER_BAND; ++vap) {
            if (!state->bandInfo[band].vaps[vap].valid) {
                // No more valid VAPs on this band
                break;
            }

            // Check if this VAP is on the same ESS as the candidates
            if (state->bandInfo[band].vaps[vap].essId == essIdToMatch)
            {
                // Is this a candidate VAP?
                LBD_BOOL match = LBD_FALSE;
                int i;
                for (i = 0; i < candidateCount; i++) {
                    if (!lbIsBSSLocal(&candidateList[i])) {
                        // Not a local BSS - will not match any VAP
                        continue;
                    }

                    if (!candidateList[i].vap) {
                        return LBD_NOK;
                    }
                    if (candidateList[i].vap == &state->bandInfo[band].vaps[vap]) {
                        // Candidate VAP
                        match = LBD_TRUE;
                        break;
                    }
                }

                if (!match) {
                    if (callback(state, &state->bandInfo[band].vaps[vap],
                                 cookie) != LBD_OK) {
                        return LBD_NOK;
                    }
                }
            }
        }
    }

    return LBD_OK;
}

#if !SONLIB_SUPPORT_ENABLED
//Function not used when SONLIB is enabled
/**
 * @brief Get STA stats from the driver
 *
 * @param [in] state  the handle returned from
 *                    wlanifBSteerControlCmnCreate()
 * @param [in] vap  VAP to fetch STA stats on
 * @param [in] staAddr  STA to collect stats for
 * @param [out] stats  stats returned from driver
 *
 * @return LBD_OK if the STA stats could be fetched; LBD_NOK
 *         otherwise
 */
static LBD_STATUS wlanifBSteerControlCmnGetSTAStats(wlanifBSteerControlHandle_t state,
                                                 const struct wlanifBSteerControlVapInfo *vap,
                                                 const struct ether_addr *staAddr,
                                                 struct ieee80211req_sta_stats *stats) {
    lbCopyMACAddr(staAddr->ether_addr_octet, stats->is_u.macaddr);
    if(wlanIfLb->ops->getStaStats(wlanIfLb->ctx, vap->ifname, (void *)stats, sizeof(*stats)) < 0) {
        return LBD_NOK;
    }

    return LBD_OK;
}
#endif

/**
 * @brief Get the VAP corresponding to sysIndex
 *
 * @param [in] state the 'this' pointer
 * @param [in] sysIndex sysIndex for the VAP to search for
 * @param [in] indexBand band on which sysIndex is found (if
 *                       known).  If set to wlanif_band_invalid,
 *                       will search all bands.
 *
 * @return struct wlanifBSteerControlVapInfo*
 *     pointer to the VAP corresponding to sysIndex if found,
 *     otherwise NULL
 */
static
struct wlanifBSteerControlVapInfo *wlanifBSteerControlCmnGetVAPFromSysIndex(
        wlanifBSteerControlHandle_t state, int sysIndex,
        wlanif_band_e indexBand) {
    lbDbgAssertExit(state->dbgModule, state);

    wlanif_band_e band;
    wlanif_band_e startingBand = wlanif_band_24g;
    wlanif_band_e endingBand = wlanif_band_5g;
    int i;

    if (indexBand != wlanif_band_invalid) {
        startingBand = indexBand;
        endingBand = indexBand;
    }

    for (band = startingBand; band <= endingBand; ++band) {
        for (i = 0; i < MAX_VAP_PER_BAND; ++i) {
            if (!state->bandInfo[band].vaps[i].valid) {
                break;
            }
            if (state->bandInfo[band].vaps[i].sysIndex == sysIndex) {
                return &state->bandInfo[band].vaps[i];
            }
        }
    }

    // Not found
    return NULL;
}

/**
 * @brief Get the VAP with a matching channel
 *
 * @param [in] state the 'this' pointer
 * @param [in] channelId  the channel to search for
 *
 * @pre state must be valid
 * @pre channelId must be valid
 *
 * @return pointer to the first VAP with a matching channel; otherwise NULL
 */
struct wlanifBSteerControlVapInfo *
wlanifBSteerControlCmnGetFirstVAPByRadio(
        wlanifBSteerControlHandle_t state,
        const struct wlanifBSteerControlRadioInfo *radio) {
    wlanif_band_e band = wlanif_resolveBandFromChannelNumber(radio->channel);
    lbDbgAssertExit(state->dbgModule, band != wlanif_band_invalid);

    size_t i;
    for (i = 0; i < MAX_VAP_PER_BAND; ++i) {
        // This never happens in practice assuming that this function is
        // always called with a valid radio.
        if (!state->bandInfo[band].vaps[i].valid) {
            break;
        }

        if (radio == state->bandInfo[band].vaps[i].radio) {
            return &state->bandInfo[band].vaps[i];
        }
    }

    // Not found
    return NULL;
}

/**
 * @brief Dump the ATF table for a single interface
 *
 * @param [in] state the 'this' pointer
 * @param [in] vap  the vap to dump associated STAs for
 * @param [in] callback  the callback function to invoke for each reserved
 *                       airtime STA entry
 * @param [in] cookie  the parameter to provide back in the callback
 *
 * @return LBD_OK if the dump succeeded on this interface; otherwise LBD_NOK
 */
static LBD_STATUS wlanifBSteerControlCmnDumpATFTableOneIface(
        wlanifBSteerControlHandle_t state,
        struct wlanifBSteerControlVapInfo *vap,
        wlanif_reservedAirtimeCB callback, void *cookie) {
    struct atftable atfInfo;
    memset(&atfInfo, 0, sizeof(atfInfo));
    atfInfo.id_type = IEEE80211_IOCTL_ATF_SHOWATFTBL;
#if !SONLIB_SUPPORT_ENABLED
    if(wlanIfLb->ops->getGenericInfoAtf(wlanIfLb->ctx, vap->ifname,
        IEEE80211_IOCTL_ATF_SHOWATFTBL, &atfInfo, sizeof(struct atftable)) < 0) {
#else
    if(vap->drvHandle->ops->dumpAtfTable(vap->drvHandle->ctx,
           vap->ifname, &atfInfo, sizeof(struct atftable)) < 0) {
#endif
        if (errno == EOPNOTSUPP) {
            dbgf(state->dbgModule, DBGINFO,
                 "%s: WARNING: ATF is not enabled by driver for iface %s",
                 __func__, vap->ifname);
            return LBD_OK;
        } else {
            dbgf(state->dbgModule, DBGERR,
                 "%s: Failed to perform ioctl for iface %s [errno %d]",
                 __func__, vap->ifname, errno);
            return LBD_NOK;
        }
    }

    lbd_bssInfo_t bss = {
        LBD_APID_SELF, vap->radio->channel,
        vap->essId, vap
    };

    size_t i;
    for (i = 0; i < atfInfo.info_cnt; ++i) {
        struct atfcntbl *entry = &atfInfo.atf_info[i];
        if (entry->info_mark) {
            lbd_airtime_t airtime = wlanifMapToAirtime(state->dbgModule, entry->cfg_value);
            if (airtime != LBD_INVALID_AIRTIME) {
                struct ether_addr staAddr;
                lbCopyMACAddr(entry->sta_mac, staAddr.ether_addr_octet);
                callback(&staAddr, &bss, airtime, cookie);
            }
        }
    }
    return LBD_OK;
}

/**
 * @brief Compare all radios on a given band to determine which
 *        radio(s) has the strongest Tx power
 *
 * @param [in] state  the 'this' pointer
 * @param [in] band  the given band
 */
static void wlanifBSteerControlCmnFindStrongestRadioOnBand(
        wlanifBSteerControlHandle_t state, wlanif_band_e band) {
    size_t i;
    u_int8_t strongestTxPower = 0;
    for (i = 0; i < WLANIF_MAX_RADIOS; ++i) {
        if (!state->radioInfo[i].valid ||
            wlanif_resolveBandFromChannelNumber(state->radioInfo[i].channel) != band) {
            // Only compare same band radios
            continue;
        }
        if(state->radioInfo[i].maxTxPower > strongestTxPower) {
            strongestTxPower = state->radioInfo[i].maxTxPower;
        }
    }
    // Mark all radios with highest Tx power as strongest radio, since
    // we want to keep 11ac clients on any of them.
    for (i = 0; i < WLANIF_MAX_RADIOS; ++i) {
        if (!state->radioInfo[i].valid ||
            wlanif_resolveBandFromChannelNumber(state->radioInfo[i].channel) != band) {
            // Only compare same band radios
            continue;
        }
        if (state->radioInfo[i].maxTxPower == strongestTxPower) {
            state->radioInfo[i].strongestRadio = LBD_TRUE;
        } else {
            state->radioInfo[i].strongestRadio = LBD_FALSE;
        }
    }
}

/**
 * @brief Notify all observers about a channel change
 *
 * @param [in] state the 'this' pointer
 * @param [in] vap  the VAP on which channel change happens
 */
static void wlanifBSteerControlCmnNotifyChanChangeObserver(
        wlanifBSteerControlHandle_t state,
        struct wlanifBSteerControlVapInfo *vap) {
    size_t i;
    for (i = 0; i < MAX_CHAN_CHANGE_OBSERVERS; ++i) {
        if (state->chanChangeObserver[i].isValid) {
            state->chanChangeObserver[i].callback(
                    vap, vap->radio->channel, state->chanChangeObserver[i].cookie);
        }
    }
}

// ====================================================================
// Package level functions
// ====================================================================

#if SONLIB_SUPPORT_ENABLED
/**
 * @brief  Function to call SON Library Init to generate
 *          Ifname, Vendor and Driver Handle mapping
 *          using info from config file
 *
 * @param [in] state  the handle returned from
 *                    wlanifBSteerControlCmnCreate()
 */
static void wlanifBSteerInitIfnameVendorMap(wlanifBSteerControlHandle_t state)
{
    const char *wlanInterfaces = NULL;
    const char *wlanInterfacesEx = NULL;
    const char *radioVendorMap= NULL;
    char wlanInterfaceList [1024]; //Max size for wlan interface list
    uint8_t isHYD = 0;
    uint8_t isLBD = 1;

    dbgf(state->dbgModule, DBGINFO, "%s: Initializing ifname-drvhandle mapping", __func__);
    memset(wlanInterfaceList, 0, sizeof(wlanInterfaceList));
    wlanInterfaces = profileGetOpts(mdModuleID_WlanIF,
                                        WLANIFBSTEERCONTROL_WLAN_INTERFACES,
                                        NULL);
    if (wlanInterfaces != NULL && strlen(wlanInterfaces) > 0 ){
        strlcpy(wlanInterfaceList, wlanInterfaces, sizeof(wlanInterfaceList));
    }

    /* Excluded WLAN interfaces */
    wlanInterfacesEx = profileGetOpts(mdModuleID_WlanIF,
                                        WLANIFBSTEERCONTROL_WLAN_INTERFACES_EXCLUDED,
                                        NULL);
    if (wlanInterfacesEx != NULL && strlen(wlanInterfacesEx) > 0 ){
        /* Add Excluded Wlan interfaces to previous wlan interface list */
        if (strlen(wlanInterfaceList) > 0) {
            /* Add comma separator if list was not empty */
            strlcat(wlanInterfaceList, ",", sizeof(wlanInterfaceList));
        }
        strlcat(wlanInterfaceList, wlanInterfacesEx, sizeof(wlanInterfaceList));
    }
    dbgf(state->dbgModule, DBGINFO, "%s: wlanInterfaceList: %s", __func__, wlanInterfaceList);

    radioVendorMap = profileGetOpts(mdModuleID_WlanIF,
                                        WLANIFBSTEERCONTROL_WLAN_RADIO_VENDORMAP,
                                        NULL);

    /* Call SON Library function to generate iface-vendor mapping */
    /* This function is defined in sonlib_cmn.h/.c                */
    soncmnIfnameVendorMapResolve(wlanInterfaceList, radioVendorMap, isHYD, isLBD);
    free((char *)wlanInterfaces);
    free((char *)wlanInterfacesEx);
    free((char *)radioVendorMap);
}
#endif

wlanifBSteerControlHandle_t wlanifBSteerControlCreate(struct dbgModule *dbgModule) {
    struct wlanifBSteerControlPriv_t *state =
        calloc(1, sizeof(struct wlanifBSteerControlPriv_t));
    if (!state) {
        dbgf(dbgModule, DBGERR, "%s: Failed to allocate state structure",
             __func__);
        return NULL;
    }

    state->dbgModule = dbgModule;
    state->controlSock = -1;

#if SONLIB_SUPPORT_ENABLED
    //Initialize ifname-vendor mapping, for both
    //included and excluded wlan interfaces
    wlanifBSteerInitIfnameVendorMap (state);
#endif

    LBD_BOOL allowZeroAPIfaces =
        profileGetOptsInt(mdModuleID_WlanIF, WLANIFBSTEERCONTROL_ALLOW_ZERO_AP_INTERFACES,
                          wlanifElementDefaultTable) ? LBD_TRUE : LBD_FALSE;

    /* Resovlve WLAN interfaces */
    if (wlanifBSteerControlCmnResolveWlanIfaces(state, allowZeroAPIfaces, LBD_TRUE) ==
        LBD_NOK) {
        free(state);
        return NULL;
    }

    /* Resolve the excluded WLAN interfaces */
    if (wlanifBSteerControlCmnResolveWlanIfaces(state, allowZeroAPIfaces, LBD_FALSE) ==
        LBD_NOK) {
        dbgf(dbgModule, DBGINFO, "%s: No Excluded Interfaces is  present", __func__);
        free(state);
        return NULL;
    }

    if (allowZeroAPIfaces &&
            !wlanifBSteerControlCmnIsBandValid(state, wlanif_band_24g) &&
            !wlanifBSteerControlCmnIsBandValid(state, wlanif_band_5g)) {
        // Nothing more to do if we don't have any AP interfaces. We must
        // be operating in a STA only configuration (temporarily) or be acting
        // as a standalone controller in Multi-AP SIG mode.
        evloopTimeoutCreate(&state->vapReadyTimeout, "vapReadyTimeout",
                            wlanifBSteerControlCmnAreVAPsReadyTimeoutHandler,
                            state);
        return state;
    }

    // Socket is open at this point, so if an error is encountered, we need
    // to make sure to close it so as not to leak it.
    do {
        if (wlanifBSteerControlCmnInitializeACLs(state, wlanif_band_24g) == LBD_NOK ||
            wlanifBSteerControlCmnInitializeACLs(state, wlanif_band_5g) == LBD_NOK) {
            break;
        }

        /* Get configuration parameters from configuration file. */
        if (wlanifBSteerControlCmnReadConfig(state, wlanif_band_24g) == LBD_NOK ||
            wlanifBSteerControlCmnReadConfig(state, wlanif_band_5g) == LBD_NOK) {
            break;
        }

        LBD_BOOL sonEnabled = wlanifBSteerControlGetSONInitVal(state);
        if (wlanifBSteerControlCmnSetSON(state, wlanif_band_24g,
                                         sonEnabled) == LBD_NOK ||
            wlanifBSteerControlCmnSetSON(state, wlanif_band_5g,
                                         sonEnabled) == LBD_NOK) {
            break;
        }

        // Make sure band steering is disabled in the driver, in case
        // it wasn't shut down cleanly last time it ran
        wlanifBSteerControlDisable(state);

        evloopTimeoutCreate(&state->vapReadyTimeout, "vapReadyTimeout",
                            wlanifBSteerControlCmnAreVAPsReadyTimeoutHandler,
                            state);

        return state;
    } while (0);

    // This will tear down the ACLs, close the socket, and then deallocate
    // the state object.
    wlanifBSteerControlDestroy(state);
    return NULL;
}

LBD_STATUS wlanifBSteerControlEnableWhenReady(
        wlanifBSteerControlHandle_t state, LBD_BOOL *enabled) {
    // Sanity check
    if (!state || !enabled) {
        return LBD_NOK;
    }

    *enabled = LBD_FALSE;

    // Check whether all VAPs on both bands are ready, and only
    // then perform the enable.
    if (wlanifBSteerControlCmnAreAllVAPsReady(state)) {
        if (LBD_NOK == wlanifBSteerControlCmnResolvePHYCapInfo(state)) {
            return LBD_NOK;
        }
        if (wlanifBSteerControlCmnSetEnable(state, wlanif_band_24g) == LBD_NOK ||
            wlanifBSteerControlCmnSetEnable(state, wlanif_band_5g) == LBD_NOK) {
            dbgf(state->dbgModule, DBGERR,
                 "%s: Enabling on both bands failed", __func__);
            return LBD_NOK;
        }

        *enabled = LBD_TRUE;
        state->bandSteeringEnabled = LBD_TRUE;

        wlanif_bandSteeringStateEvent_t bandSteeringStateEvent;
        bandSteeringStateEvent.enabled = LBD_TRUE;

        mdCreateEvent(mdModuleID_WlanIF, mdEventPriority_High,
                      wlanif_event_band_steering_state,
                      &bandSteeringStateEvent, sizeof(bandSteeringStateEvent));

        return LBD_OK;
    } else {
        evloopTimeoutRegister(&state->vapReadyTimeout,
                              VAP_READY_CHECK_PERIOD,
                              0 /* us */);

        return LBD_OK;
    }
}

/**
 * @brief Enable the ack rssi for steering feature.
 *
 * @param [in] handle  the handle returned from wlanifBSteerControlCmnCreate()
 *                     to enable the feature
 *
 * @return LBD_OK on successful enable; otherwise LBD_NOK
 */
LBD_STATUS wlanifBSteerControlCmnSetAckRSSI(
                                 wlanifBSteerControlHandle_t state) {

    if( wlanifBSteerControlCmnSetAckRSSIEachBand(state, wlanif_band_24g)== LBD_NOK ||
        wlanifBSteerControlCmnSetAckRSSIEachBand(state, wlanif_band_5g)== LBD_NOK) {
        dbgf(state->dbgModule, DBGERR,
                 "%s: Enabling AckRSSI on both bands failed", __func__);

        return  LBD_NOK;
    }

    return LBD_OK;
}

/**
 * @brief Enable the ack rssi on each band for band steering
 *
 * @param [in] handle  the handle returned from wlanifBSteerControlCmnCreate()
 *                     to enable the feature
 * @param [in] band  the band on which to enable ack rssi feature
 *
 * @return LBD_OK on successful enable; otherwise LBD_NOK
 */
LBD_STATUS wlanifBSteerControlCmnSetAckRSSIEachBand(
                               wlanifBSteerControlHandle_t state,
                               wlanif_band_e band) {

    if (wlanifBSteerControlCmnSendAckRSSIEnable(state, band,
                                LBD_FALSE /* ignoreError */) == LBD_NOK) {
        dbgf(state->dbgModule, DBGERR,
                     "%s: Failed to enable ACK RSSI on band %u",
                                                __func__, band);
        return LBD_NOK;
    }

    state->bandInfo[band].ackrssi = LBD_TRUE;

    dbgf(state->dbgModule, DBGINFO,
           "%s: Successfully enabled ACK RSSI on band %u", __func__, band);
    return LBD_OK;
}

LBD_STATUS wlanifBSteerControlEventsEnable(wlanifBSteerControlHandle_t state,
                                           wlanifBSteerEventsHandle_t handle) {
    wlanif_band_e band;
    int i;
    // Sanity check
    if (!state || !handle)
        return LBD_NOK;

    for (band = wlanif_band_24g ; band < wlanif_band_invalid; ++band) {
        for (i = 0; i < MAX_VAP_PER_BAND; ++i) {
            if (state->bandInfo[band].vaps[i].valid) {
                if (wlanifBSteerEventsEnable(handle,
                                state->bandInfo[band].vaps[i].sysIndex) == LBD_NOK)
                    return LBD_NOK;
            }
        }
    }

    return LBD_OK;
}

LBD_STATUS wlanifBSteerControlDisable(wlanifBSteerControlHandle_t state) {
    // Sanity check
    if (!state) {
        return LBD_NOK;
    }

    wlanifBSteerControlCmnSetDisable(state, wlanif_band_24g);
    wlanifBSteerControlCmnSetDisable(state, wlanif_band_5g);

    state->bandSteeringEnabled = LBD_FALSE;
    return LBD_OK;
}

LBD_STATUS wlanifBSteerControlDestroy(wlanifBSteerControlHandle_t state) {
    if (state) {
        // Ignore the errors since there is nothing really that can be done
        // at this point.
        (void) wlanifBSteerControlCmnSetSON(state, wlanif_band_24g, LBD_FALSE);
        (void) wlanifBSteerControlCmnSetSON(state, wlanif_band_5g, LBD_FALSE);

        wlanifBSteerControlCmnTeardownACLs(state, wlanif_band_24g);
        wlanifBSteerControlCmnTeardownACLs(state, wlanif_band_5g);

        size_t i;
        struct wlanifBSteerControlRadioInfo *radio;
        for (i = 0; i < WLANIF_MAX_RADIOS; ++i) {
            radio = &state->radioInfo[i];
            if (radio->valid) {
                // Clear RSSI waiting list if any
                list_head_t *iter = radio->rssiWaitingList.next;
                while (iter != &radio->rssiWaitingList) {
                    wlanifBSteerControlCmnRSSIRequestEntry_t *curEntry =
                        list_entry(iter, wlanifBSteerControlCmnRSSIRequestEntry_t, listChain);

                    iter = iter->next;
                    free(curEntry);
                }
                if (radio->enableOLStatsIoctl && radio->numEnableStats) {
                    // Disable the stats on all radios where they are still active.
                    wlanifBSteerControlCmnPrivIoctlSetParam(state, radio->ifname,
                                                            radio->enableOLStatsIoctl,
                                                            0);
                }
                else if (radio->enableNoDebug && radio->numEnableStats) {
                    // Disable the stats on all radios where they are still active.
                    wlanifBSteerControlCmnPrivIoctlSetParam(state, radio->ifname,
                                                            radio->enableNoDebug,
                                                            1);
                }
            }
        }

        close(state->controlSock);
        evloopTimeoutUnregister(&state->vapReadyTimeout);

        free(state);
    }

    return LBD_OK;
}

LBD_STATUS wlanifBSteerControlSetOverload(wlanifBSteerControlHandle_t state,
                                          lbd_channelId_t channelId,
                                          LBD_BOOL overload) {
    size_t i;

    wlanif_band_e band = wlanif_resolveBandFromChannelNumber(channelId);

    // Sanity check
    if (!state || band == wlanif_band_invalid) {
        return LBD_NOK;
    }

    for (i = 0; i < WLANIF_MAX_RADIOS; ++i) {
        if (!state->radioInfo[i].valid ||
            state->radioInfo[i].channel != channelId) {
            continue;
        }

        struct wlanifBSteerControlVapInfo *vap =
            wlanifBSteerControlCmnGetFirstVAPByRadio(state, &state->radioInfo[i]);
        if (!vap || !state->bandInfo[band].enabled) {
            dbgf(state->dbgModule, DBGERR,
                 "%s: Band Steering is not enabled on band %u",
                 __func__, band);
            return LBD_NOK;
        }

        u_int8_t bsteering_overload;
        bsteering_overload = overload ? 1 : 0;

#if SONLIB_SUPPORT_ENABLED
        if (vap->drvHandle->ops->setBSteerOverload(vap->drvHandle->ctx,
                vap->ifname, bsteering_overload) != LBD_OK) {
#else
        if (wlanifBSteerControlCmnSetSendVAP(
                state, vap->ifname, IEEE80211_DBGREQ_BSTEERING_SET_OVERLOAD, NULL,
                (void *) &bsteering_overload, sizeof(bsteering_overload),
                LBD_FALSE /* ignoreError */) != LBD_OK) {
#endif
            return LBD_NOK;
        }
    }

    return LBD_OK;
}

wlanif_band_e wlanifBSteerControlResolveBandFromSystemIndex(wlanifBSteerControlHandle_t state,
                                                            int index) {
    wlanif_band_e band;
    int i;

    if (!state) {
         // Invalid control handle
        return wlanif_band_invalid;
    }

    for (band = wlanif_band_24g ; band < wlanif_band_invalid; ++band) {
        for (i = 0; i < MAX_VAP_PER_BAND; ++i) {
            if (state->bandInfo[band].vaps[i].valid &&
                state->bandInfo[band].vaps[i].sysIndex == index) {
                return band;
            }
        }
    }
    return wlanif_band_invalid;
}

void wlanifBSteerControlUpdateLinkState(wlanifBSteerControlHandle_t state,
                                        int sysIndex, LBD_BOOL ifaceUp,
                                        LBD_BOOL *changed) {
    if (state && changed) {
        *changed = LBD_FALSE;
        struct wlanifBSteerControlVapInfo *vap =
            wlanifBSteerControlCmnGetVAPFromSysIndex(state, sysIndex, wlanif_band_invalid);

        if (vap) {
            if (vap->ifaceUp != ifaceUp) {
                *changed = LBD_TRUE;
                vap->ifaceUp = ifaceUp;
            }
        }
    }

    // Not found, invalid control handle, or invalid changed param. Do nothing.
}

LBD_STATUS wlanifBSteerControlDumpAssociatedSTAs(wlanifBSteerControlHandle_t state,
                                                 wlanif_associatedSTAsCB callback,
                                                 void *cookie) {
    size_t i;
    for (i = 0; i < wlanif_band_invalid; ++i) {
        size_t j;
        for (j = 0; j < MAX_VAP_PER_BAND; ++j) {
            if ((state->bandInfo[i].vaps[j].valid) &&(state->bandInfo[i].vaps[j].includedIface))  {
                if (wlanifBSteerControlCmnDumpAssociatedSTAsOneIface(state,
                            &state->bandInfo[i].vaps[j],
                            (wlanif_band_e) i, callback, cookie) != LBD_OK) {
                    return LBD_NOK;
                }
            }
        }
    }

    return LBD_OK;
}

LBD_STATUS wlanifBSteerControlRequestStaRSSI(wlanifBSteerControlHandle_t state,
                                             const lbd_bssInfo_t *bss,
                                             const struct ether_addr * staAddr,
                                             u_int8_t numSamples) {
    LBD_STATUS status = LBD_NOK;

    struct wlanifBSteerControlVapInfo *vap =
        wlanifBSteerControlCmnExtractVapHandle(bss);
    if (!state || !vap || !staAddr || !numSamples) {
        return status;
    }

    wlanif_band_e band = wlanif_resolveBandFromChannelNumber(bss->channelId);
    lbDbgAssertExit(state->dbgModule, band <= wlanif_band_invalid);

    struct wlanifBSteerControlRadioInfo *radio = vap->radio;
    lbDbgAssertExit(state->dbgModule, radio);
    LBD_BOOL measurementBusy = !list_is_empty(&radio->rssiWaitingList);

    if (measurementBusy) {
        status = LBD_OK;
    } else {
        status = wlanifBSteerControlCmnSendRequestRSSI(state, vap, staAddr, numSamples);
    }

    if (status == LBD_OK) {
        list_head_t *iter = radio->rssiWaitingList.next;
        while (iter != &radio->rssiWaitingList) {
            wlanifBSteerControlCmnRSSIRequestEntry_t *curEntry =
                list_entry(iter, wlanifBSteerControlCmnRSSIRequestEntry_t, listChain);

            if (lbAreEqualMACAddrs(&curEntry->addr, staAddr)) {
                // RSSI measuremenet has been queued before, do nothing
                return LBD_OK;
            }
            iter = iter->next;
        }

        // Wait for other RSSI measurement done
        wlanifBSteerControlCmnRSSIRequestEntry_t *entry = calloc(1,
                sizeof(wlanifBSteerControlCmnRSSIRequestEntry_t));
        if (!entry) {
            dbgf(state->dbgModule, DBGERR, "%s: Failed to allocate entry for "
                                           "STA "lbMACAddFmt(":")".",
                 __func__, lbMACAddData(staAddr->ether_addr_octet));
            return LBD_NOK;
        }

        lbCopyMACAddr(staAddr, &entry->addr);
        entry->numSamples = numSamples;
        entry->vap = vap;

        if (measurementBusy) {
            dbgf(state->dbgModule, DBGDEBUG,
                 "%s: RSSI measurement request for STA " lbMACAddFmt(":")
                 " is queued on BSS " lbBSSInfoAddFmt(),
                 __func__, lbMACAddData(staAddr->ether_addr_octet),
                 lbBSSInfoAddData(bss));
        } // else the request has been sent and waiting for measurement back
        list_insert_entry(&entry->listChain, &radio->rssiWaitingList);
    }

    return status;
}

LBD_STATUS wlanifBSteerControlRequestDownlinkRSSI(
        wlanifBSteerControlHandle_t state, const lbd_bssInfo_t *bss,
        const struct ether_addr *staAddr, LBD_BOOL rrmCapable,
        size_t numChannels, const lbd_channelId_t *channelList,
        u_int8_t clientClassGroup) {
    struct wlanifBSteerControlVapInfo *vap =
        wlanifBSteerControlCmnExtractVapHandle(bss);
    if (!state || !vap || !staAddr || !rrmCapable ||
        !numChannels || !channelList) {
        // Currently only support 11k capable device
        return LBD_NOK;
    }

    return wlanifBSteerControlCmnSendRRMBcnrptRequest(
               state, vap, staAddr, numChannels, channelList, clientClassGroup);
}

LBD_STATUS wlanifBSteerControlSetChannelStateForSTA(
    wlanifBSteerControlHandle_t state,
    u_int8_t channelCount,
    const lbd_channelId_t *channelList,
    lbd_essId_t lastServingESS,
    const struct ether_addr *staAddr,
    LBD_BOOL enable) {
    int i, vap;

    if (!state || !channelCount || !channelList || !staAddr) {
        return LBD_NOK;
    }

    LBD_STATUS status = LBD_OK;

    u_int32_t operation = IO_OPERATION_DELMAC;
    if (!enable) {
        operation = IO_OPERATION_ADDMAC;
    }

    for (i = 0; i < channelCount; i++) {
        u_int8_t changedCount = 0;

        // Get the band
        wlanif_band_e band = wlanif_resolveBandFromChannelNumber(channelList[i]);
        if (band == wlanif_band_invalid) {
            dbgf(state->dbgModule, DBGERR,
                 "%s: Channel %u is not valid", __func__, channelList[i]);
            return LBD_NOK;
        }
        if (wlanifBSteerControlPerformIoctlExcludedVaps(
                state, staAddr, enable) == LBD_NOK) {
            dbgf(state->dbgModule, DBGERR,
                 "%s: Error in blacklisting on the Excluded VAPs", __func__);
            return LBD_NOK;
        }
        LBD_BOOL actionFlag;
        for (band = wlanif_band_24g; band < wlanif_band_invalid; band++) {
            for (vap = 0; vap < MAX_VAP_PER_BAND; ++vap) {
                actionFlag = LBD_FALSE;
                if (!state->bandInfo[band].vaps[vap].valid) {
                    // No more valid VAPs, can exit the loop
                    break;
                }
                if (!state->blacklistOtherESS) {
                    if (state->bandInfo[band].vaps[vap].radio->channel == channelList[i]) {
                        // Found match
                        actionFlag = LBD_TRUE;
                    }
                } else {
                        if ((state->bandInfo[band].vaps[vap].radio->channel ==
                             channelList[i]) ||
                            (state->bandInfo[band].vaps[vap].essId != lastServingESS)) {
                            actionFlag = LBD_TRUE;
                        }
                    }

                if(actionFlag) {
                    dbgf(state->dbgModule, DBGDEBUG,
                        "%s: %s Blacklist for " lbMACAddFmt(":")
                        " on vap : %s channel : %d",
                        __func__, (enable?"Removing":"Installing"),
                        lbMACAddData(staAddr->ether_addr_octet),
                        state->bandInfo[band].vaps[vap].ifname,
                        state->bandInfo[band].vaps[vap].radio->channel);
                    if (wlanifBSteerControlCmnPerformIoctlWithMAC(
                                state, operation, &state->bandInfo[band].vaps[vap],
                                staAddr) != LBD_OK) {
                        dbgf(state->dbgModule, DBGERR,
                             "%s: ioctl to change state to %d for " lbMACAddFmt(":")
                             "on interface %s failed with errno %u",
                             __func__, enable, lbMACAddData(staAddr->ether_addr_octet),
                             state->bandInfo[band].vaps[vap].ifname,
                             errno);
                        return LBD_NOK;
                    }
                    changedCount++;
                }
            }
        }

        if (!changedCount) {
            dbgf(state->dbgModule, DBGERR,
                 "%s: Requested change state to %d on channel %d for STA " lbMACAddFmt(":")
                 ", but no VAPs operating on that channel",
                 __func__, enable, channelList[i], lbMACAddData(staAddr));
            status = LBD_NOK;
        }
    }

    return status;
}

LBD_STATUS wlanifBSteerControlSetChannelProbeStateForSTA(
    wlanifBSteerControlHandle_t state,
    u_int8_t channelCount,
    const lbd_channelId_t *channelList,
    const struct ether_addr *staAddr,
    LBD_BOOL enable) {
    int i, vap;

    if (!state || !channelCount || !channelList || !staAddr) {
        return LBD_NOK;
    }

    LBD_STATUS status = LBD_OK;

    for (i = 0; i < channelCount; i++) {
        u_int8_t changedCount = 0;

        // Get the band
        wlanif_band_e band = wlanif_resolveBandFromChannelNumber(channelList[i]);
        if (band == wlanif_band_invalid) {
            dbgf(state->dbgModule, DBGERR,
                 "%s: Channel %u is not valid", __func__, channelList[i]);
            return LBD_NOK;
        }

        if (band == wlanif_band_5g) {
            continue;
        }

        // Find all VAPs on this band that match channel
        for (vap = 0; vap < MAX_VAP_PER_BAND; ++vap) {
            if (!state->bandInfo[band].vaps[vap].valid) {
                // No more valid VAPs, can exit the loop
                break;
            }
            if (state->bandInfo[band].vaps[vap].radio->channel == channelList[i]) {

                u_int8_t bsteering_allow = enable ? 1 : 0;

                // Found match
#if SONLIB_SUPPORT_ENABLED
                struct soncmnDrvHandle *drvHandle = state->bandInfo[band].vaps[vap].drvHandle;
                if (drvHandle->ops->setBSteerProbeRespAllow2G(drvHandle->ctx,
                          state->bandInfo[band].vaps[vap].ifname,
                          staAddr->ether_addr_octet, bsteering_allow) != LBD_OK) {
#else
                if (wlanifBSteerControlCmnSetSendVAP(
                            state, state->bandInfo[band].vaps[vap].ifname,
                            IEEE80211_DBGREQ_BSTEERING_SET_PROBE_RESP_ALLOW_24G,
                            staAddr, (void *) &bsteering_allow,
                            sizeof(bsteering_allow),
                            LBD_FALSE /* ignoreError */) != LBD_OK) {
#endif
                    dbgf(state->dbgModule, DBGERR,
                         "%s: ioctl to change state to %d for " lbMACAddFmt(":")
                         "on interface %s failed with errno %u",
                         __func__, enable, lbMACAddData(staAddr->ether_addr_octet),
                         state->bandInfo[band].vaps[vap].ifname,
                         errno);
                    return LBD_NOK;
                }

                changedCount++;
            }
        }

        if (!changedCount) {
            dbgf(state->dbgModule, DBGERR,
                 "%s: Requested change probe state to %d on channel %d for STA " lbMACAddFmt(":")
                 ", but no VAPs operating on that channel",
                 __func__, enable, channelList[i], lbMACAddData(staAddr));
            status = LBD_NOK;
        }
    }

    return status;
}

LBD_STATUS wlanifBSteerControlPerformIoctlExcludedVaps(
        wlanifBSteerControlHandle_t state,
        const struct ether_addr *staAddr,
        LBD_BOOL enable) {
        wlanif_band_e band;
        u_int32_t operation = IO_OPERATION_DELMAC;
        if (!enable) {
            operation = IO_OPERATION_ADDMAC;
        }

        for (band = wlanif_band_24g; band < wlanif_band_invalid; band++) {
            int vap;
            for (vap = 0; vap < MAX_VAP_PER_BAND; ++vap) {
                if (!state->bandInfo[band].vaps[vap].valid) {
                    // No more valid VAPs on this band
                    break;
                }
                if (!state->bandInfo[band].vaps[vap].includedIface) {
                    dbgf(state->dbgModule, DBGDEBUG,
                        "%s: %s Blacklist for " lbMACAddFmt(":")
                        " on vap %s", __func__,
                        (enable?"Removing":"Installing"),
                        lbMACAddData(staAddr->ether_addr_octet),
                        state->bandInfo[band].vaps[vap].ifname);
                    if (wlanifBSteerControlCmnPerformIoctlWithMAC(
                            state, operation, &state->bandInfo[band].vaps[vap],
                            staAddr) != LBD_OK) {
                        dbgf(state->dbgModule, DBGERR,
                            "%s: ioctl to change state to %d for " lbMACAddFmt(":")
                            "on interface %s failed with errno %u",
                            __func__, enable,
                            lbMACAddData(staAddr->ether_addr_octet),
                            state->bandInfo[band].vaps[vap].ifname,
                            errno);
                            return LBD_NOK;
                    }
                }
            }
        }

        return LBD_OK;
}

LBD_STATUS wlanifBSteerControlPerformIoctlOtherEss(
    wlanifBSteerControlHandle_t state,
    lbd_essId_t lastServingESS,
    const struct ether_addr *staAddr,
    LBD_BOOL enable) {

    wlanif_band_e band;
    u_int32_t operation = IO_OPERATION_DELMAC;

    if (!enable) {
        operation = IO_OPERATION_ADDMAC;
    }

    for (band = wlanif_band_24g; band < wlanif_band_invalid; band++) {
        int vap;
        for (vap = 0; vap < MAX_VAP_PER_BAND; ++vap) {
            if (!state->bandInfo[band].vaps[vap].valid) {
                // No more valid VAPs on this band
                break;
            }
            if (state->bandInfo[band].vaps[vap].essId != lastServingESS){
                dbgf(state->dbgModule, DBGDEBUG,
                    "%s: %s Blacklist for " lbMACAddFmt(":")
                    " on vap %s", __func__,
                    (enable?"Removing":"Installing"),
                    lbMACAddData(staAddr->ether_addr_octet),
                    state->bandInfo[band].vaps[vap].ifname);
                if (wlanifBSteerControlCmnPerformIoctlWithMAC(
                            state, operation, &state->bandInfo[band].vaps[vap],
                            staAddr) != LBD_OK) {
                    dbgf(state->dbgModule, DBGERR,
                            "%s: ioctl to change state to %d for " lbMACAddFmt(":")
                            "on interface %s failed with errno %u",
                            __func__, enable,
                            lbMACAddData(staAddr->ether_addr_octet),
                            state->bandInfo[band].vaps[vap].ifname,
                            errno);
                    return LBD_NOK;
                }
            }
        }
    }

    return LBD_OK;
}

LBD_STATUS wlanifBSteerControlSetNonCandidateStateForSTA(
    wlanifBSteerControlHandle_t state,
    u_int8_t candidateCount,
    const lbd_bssInfo_t *candidateList,
    const struct ether_addr *staAddr,
    lbd_essId_t lastServingESS,
    LBD_BOOL enable,
    LBD_BOOL probeOnly,
    const lbd_bssInfo_t *servingBSSInfo,
    u_int8_t clientClassGroup) {

    // Other parameters checked inside the function call
    if (!state || !staAddr) {
        return LBD_NOK;
    }

    wlanifBSteerControlCmnNonCandidateSet_t setParams = {
        staAddr,
        enable,
        probeOnly,
        clientClassGroup
    };

    if (!probeOnly) {
        if (wlanifBSteerControlPerformIoctlExcludedVaps(
                state, staAddr, enable) == LBD_NOK) {
            dbgf(state->dbgModule, DBGERR,
                 "%s: Error in blacklisting on the Excluded VAPs", __func__);
            return LBD_NOK;
        }
    }

    if(state->blacklistOtherESS){
        if (!probeOnly) {
            if (wlanifBSteerControlPerformIoctlOtherEss(state, lastServingESS, staAddr, enable) == LBD_NOK) {
                dbgf(state->dbgModule, DBGERR,
                        "%s: Error in blacklisting on the VAPs on other ESS", __func__);
                return LBD_NOK;
            }
        }
    }

    return wlanifBSteerControlCmnNonCandidateMatch(
        state, candidateCount, candidateList,
        wlanifBSteerControlCmnNonCandidateSetCB,
        (void *)&setParams, servingBSSInfo);
}

u_int8_t wlanifBSteerControlGetNonCandidateStateForSTA(
    wlanifBSteerControlHandle_t state,
    u_int8_t candidateCount,
    const lbd_bssInfo_t *candidateList,
    u_int8_t maxCandidateCount,
    lbd_bssInfo_t *outCandidateList,
    const lbd_bssInfo_t *servingBSSInfo) {

    // Other parameters checked inside the function call
    if (!outCandidateList || !maxCandidateCount) {
        return 0;
    }

    wlanifBSteerControlCmnNonCandidateGet_t getParams = {
        maxCandidateCount,
        0,
        outCandidateList
    };

    if (wlanifBSteerControlCmnNonCandidateMatch(
            state, candidateCount, candidateList,
            wlanifBSteerControlCmnNonCandidateGetCB,
            (void *)&getParams,
            servingBSSInfo) != LBD_OK) {
        return 0;
    }

    return getParams.outCandidateCount;
}

LBD_STATUS wlanifBSteerControlSetCandidateStateForSTA(
    wlanifBSteerControlHandle_t state,
    u_int8_t candidateCount,
    const lbd_bssInfo_t *candidateList,
    const struct ether_addr *staAddr,
    LBD_BOOL enable,
    const lbd_bssInfo_t *servingBSSInfo,
    u_int16_t validitySecs,
    u_int8_t clientClassGroup) {

    if (!state || !candidateCount || !candidateList || !staAddr) {
        return LBD_NOK;
    }

    struct sockaddr addr;

    memset(&addr, 0, sizeof(addr));
    addr.sa_family = ARPHRD_ETHER;
    lbCopyMACAddr(staAddr->ether_addr_octet, addr.sa_data);

    // Set association state
    u_int32_t operation = IO_OPERATION_DELMAC;

#if SONLIB_SUPPORT_ENABLED
    struct soncmn_client_assoc_req_acl_t client_assoc_req;
#else
    client_assoc_req_acl_t client_assoc_req;
#endif

    if (!enable) {
        if (!validitySecs) {
            operation = IO_OPERATION_ADDMAC;
        } else {
            operation = IO_OPERATION_ADDMAC_VALIDITY_TIMER;
            client_assoc_req.validity_period = validitySecs;
            lbCopyMACAddr(staAddr->ether_addr_octet, client_assoc_req.stamac);
        }
    }

    size_t i;

    for (i = 0; i < candidateCount; i++) {
        struct wlanifBSteerControlVapInfo *vap =
            (struct wlanifBSteerControlVapInfo *)candidateList[i].vap;
        if (!vap) {
            return LBD_NOK;
        }

#if SONLIB_SUPPORT_ENABLED
        if (operation != IO_OPERATION_ADDMAC_VALIDITY_TIMER) {
            if ((vap->drvHandle->ops->setMacFiltering(
                    vap->drvHandle->ctx, vap->ifname, staAddr->ether_addr_octet,
                    ((operation == IO_OPERATION_ADDMAC) ? LBD_TRUE : LBD_FALSE))) < 0) {
                dbgf(state->dbgModule, DBGERR,
                 "%s: ioctl to set VAP state to %d for candidate " lbBSSInfoAddFmt() " failed with errno %u",
                 __func__, enable, lbBSSInfoAddData(&candidateList[i]), errno);
                return LBD_NOK;
            }
        } else {
            if ((vap->drvHandle->ops->setMacBlockTimer(vap->drvHandle->ctx, vap->ifname,
                                                       staAddr->ether_addr_octet,
                                                       client_assoc_req)) < 0) {
                dbgf(state->dbgModule, DBGERR,
                 "%s: ioctl to set VAP state to %d for candidate " lbBSSInfoAddFmt() " failed with errno %u",
                 __func__, enable, lbBSSInfoAddData(&candidateList[i]), errno);
                return LBD_NOK;
            }
        }
#else

        int ret;
        if (operation != IO_OPERATION_ADDMAC_VALIDITY_TIMER) {
            ret = wlanIfLb->ops->addDelKickMAC(wlanIfLb->ctx, vap->ifname, operation, (char *)&addr,
                    sizeof(addr));
            if ((ret < 0) && (ret != -EEXIST) && (ret != -ENOENT)) {
                dbgf(state->dbgModule, DBGERR,
                 "%s: ioctl to set VAP state to %d for candidate " lbBSSInfoAddFmt() " failed with errno %u",
                 __func__, enable, lbBSSInfoAddData(&candidateList[i]), errno);
                return LBD_NOK;
            }
        } else {
            if (wlanifBSteerControlCmnSetSendVAP(
                    state, vap->ifname, IEEE80211_DBGREQ_ADD_MAC_VALIDITY_TIMER_ACL,
                    staAddr, (void *)&client_assoc_req, sizeof(client_assoc_req),
                    LBD_FALSE /* ignoreError */) != LBD_OK) {
                dbgf(state->dbgModule, DBGERR,
                     "%s: ioctl to Add MAC to ACL with Validity Timer for "
                     "BSS " lbBSSInfoAddFmt() " failed with errno %u",
                     __func__, lbBSSInfoAddData(&candidateList[i]), errno);
                return LBD_NOK;
             }
        }
#endif

        // Only need to disable probe response witholding - it is enabled automatically when
        // enabling the VAP
        if (!enable) {
            u_int8_t bsteering_withhold = 1;
            u_int8_t bsteering_auth = 1;

#if SONLIB_SUPPORT_ENABLED
            if(vap->drvHandle->ops->setBSteerProbeRespWH(vap->drvHandle->ctx,
                  vap->ifname, staAddr->ether_addr_octet, bsteering_withhold) != LBD_OK) {
#else
            if (wlanifBSteerControlCmnSetSendVAP(
                        state, vap->ifname,
                        IEEE80211_DBGREQ_BSTEERING_SET_PROBE_RESP_WH,
                        staAddr, (void *) &bsteering_withhold,
                        sizeof(bsteering_withhold),
                        LBD_FALSE /* ignoreError */) != LBD_OK) {
#endif
                dbgf(state->dbgModule, DBGERR,
                     "%s: ioctl to start probe response witholding for candidate "
                     lbBSSInfoAddFmt() " failed with errno %u",
                     __func__, lbBSSInfoAddData(&candidateList[i]), errno);
                return LBD_NOK;
            }

            if(state->auth[clientClassGroup]){
#if SONLIB_SUPPORT_ENABLED
                if(vap->drvHandle->ops->setBSteerAuthAllow(vap->drvHandle->ctx,
                      vap->ifname, staAddr->ether_addr_octet, bsteering_auth) != LBD_OK) {
#else
                if (wlanifBSteerControlCmnSetSendVAP(
                            state, vap->ifname,
                            IEEE80211_DBGREQ_BSTEERING_SET_AUTH_ALLOW,
                            staAddr, (void *) &bsteering_auth,
                            sizeof(bsteering_auth),
                            LBD_FALSE /* ignoreError */) != LBD_OK) {
#endif
                    dbgf(state->dbgModule, DBGERR,
                            "%s: ioctl to set authentication allow status for candidate "
                            " failed with errno %u",
                            __func__, errno);
                    return LBD_NOK;
                }
            }
        }
    }

    return LBD_OK;
}

LBD_STATUS wlanifBSteerControlSetCandidateProbeStateForSTA(
    wlanifBSteerControlHandle_t state,
    u_int8_t candidateCount,
    const lbd_bssInfo_t *candidateList,
    const struct ether_addr *staAddr,
    LBD_BOOL enable) {

    size_t i;

    if (!state || !candidateCount || !candidateList || !staAddr) {
        return LBD_NOK;
    }

    for (i = 0; i < candidateCount; i++) {
        struct wlanifBSteerControlVapInfo *vap;
        wlanif_band_e band;

        /* We need only the local BSSes */
        if (!lbIsBSSLocal(&candidateList[i]))
            continue;

        vap = (struct wlanifBSteerControlVapInfo *)candidateList[i].vap;
        if (!vap) {
            return LBD_NOK;
        }

        band = wlanif_resolveBandFromChannelNumber(candidateList[i].channelId);
        if(band == wlanif_band_5g)
            continue;

        u_int8_t bsteering_allow = enable ? 1 : 0;

#if SONLIB_SUPPORT_ENABLED
        if (vap->drvHandle->ops->setBSteerProbeRespAllow2G(vap->drvHandle->ctx,
                vap->ifname, staAddr->ether_addr_octet, bsteering_allow) != LBD_OK) {
#else
        if (wlanifBSteerControlCmnSetSendVAP(
                    state, vap->ifname,
                    IEEE80211_DBGREQ_BSTEERING_SET_PROBE_RESP_ALLOW_24G,
                    staAddr, (void *) &bsteering_allow,
                    sizeof(bsteering_allow),
                    LBD_FALSE /* ignoreError */) != LBD_OK) {
#endif
            dbgf(state->dbgModule, DBGERR,
                    "%s: ioctl to start/stop probe response for candidate "
                    lbBSSInfoAddFmt() " failed with errno %u",
                    __func__, lbBSSInfoAddData(&candidateList[i]), errno);
            return LBD_NOK;
        }
    }

    return LBD_OK;
}

LBD_BOOL wlanifBSteerControlIsBSSIDInList(
    wlanifBSteerControlHandle_t state,
    u_int8_t candidateCount,
    const lbd_bssInfo_t *candidateList,
    const struct ether_addr *bssid) {

    if (!state || !candidateCount || !candidateList || !bssid) {
        return LBD_FALSE;
    }

    // Get the BSS corresponding to this BSSID
    lbd_bssInfo_t bss;
    if (wlanifBSteerControlGetBSSInfoFromBSSID(
            state,
            &bssid->ether_addr_octet[0], &bss) == LBD_NOK) {
        // Couldn't resolve, so must be unknown
        return LBD_FALSE;
    }

    size_t i;
    for (i = 0; i < candidateCount; i++) {
        if (lbAreBSSesSame(&bss, &candidateList[i])) {
            return LBD_TRUE;
        }
    }

    // No match found
    return LBD_FALSE;
}

u_int8_t wlanifBSteerControlGetChannelList(wlanifBSteerControlHandle_t state,
                                           lbd_channelId_t *channelList,
                                           wlanif_chwidth_e *chwidthList,
                                           u_int8_t maxSize) {
    if (!state || !channelList) {
        return 0;
    }

    u_int8_t channelCount = 0;
    size_t i;

    for (i = 0; i < WLANIF_MAX_RADIOS; ++i) {
        if (state->radioInfo[i].valid) {
            channelList[channelCount] = state->radioInfo[i].channel;
            if (chwidthList) {
                chwidthList[channelCount] = state->radioInfo[i].maxChWidth;
            }
            channelCount++;

            if (channelCount >= maxSize) {
                // Have reached the maximum number of channels
                break;
            }
        }
    }

    return channelCount;
}

LBD_STATUS wlanifBSteerControlDisassociateSTA(
        wlanifBSteerControlHandle_t state, const lbd_bssInfo_t *assocBSS,
        const struct ether_addr *staAddr, LBD_BOOL local) {
    if (!state || !assocBSS || !assocBSS->vap || !staAddr) {
        return LBD_NOK;
    }

    if (local) {
        struct wlanifBSteerControlVapInfo *vap =
            (struct wlanifBSteerControlVapInfo *)assocBSS->vap;
#if SONLIB_SUPPORT_ENABLED
        return vap->drvHandle->ops->performLocalDisassoc(vap->drvHandle->ctx, vap->ifname, staAddr->ether_addr_octet);
#else
        return wlanifBSteerControlCmnSetSendVAP(
            state, vap->ifname, IEEE80211_DBGREQ_BSTEERING_LOCAL_DISASSOCIATION, staAddr,
            NULL, 0, LBD_TRUE /* ignoreError */);
#endif
    } else {
        return wlanifBSteerControlCmnPerformIoctlWithMAC(
                state, IO_OPERATION_KICKMAC, assocBSS->vap, staAddr);
    }
}

LBD_STATUS wlanifBSteerControlSendBTMRequest(wlanifBSteerControlHandle_t state,
                                             const lbd_bssInfo_t *assocBSS,
                                             const struct ether_addr *staAddr,
                                             u_int8_t dialogToken,
                                             u_int8_t candidateCount,
                                             u_int8_t steerReason,
                                             u_int8_t forceSteer,
                                             const lbd_bssInfo_t *candidateList) {
    int i;
#if SONLIB_SUPPORT_ENABLED
    struct soncmn_ieee80211_bstm_reqinfo_target reqinfo;
#else
    struct ieee80211_bstm_reqinfo_target reqinfo;
#endif

    // Sanity check
    if (!state || !assocBSS || !assocBSS->vap ||
        !staAddr || !candidateCount || !candidateList ||
        candidateCount > ieee80211_bstm_req_max_candidates) {
        return LBD_NOK;
    }

    reqinfo.dialogtoken = dialogToken;
    reqinfo.num_candidates = candidateCount;
    reqinfo.trans_reason = steerReason;

    if(forceSteer)
    {
        reqinfo.disassoc = 1;
        reqinfo.disassoc_timer = 1;
    }
    else
    {
        reqinfo.disassoc = 0;
        reqinfo.disassoc_timer = 0;
    }

    // Copy the candidates
    // Candidates are in preference order - first candidate is most preferred
    for (i = 0; i < candidateCount; i++) {
        const struct ether_addr *bssid =
            wlanifBSteerControlGetBSSIDForBSSInfo(state, &candidateList[i]);

        if (!bssid) {
            return LBD_NOK;
        }

        lbCopyMACAddr(bssid, &reqinfo.candidates[i].bssid);
        reqinfo.candidates[i].channel_number = candidateList[i].channelId;
        reqinfo.candidates[i].preference = UCHAR_MAX - i;
        if (LBD_NOK == wlanif_resolveRegClass(candidateList[i].channelId,
                                             &reqinfo.candidates[i].op_class)) {
            dbgf(state->dbgModule, DBGERR, "%s: Failed to resolve regulatory class from channel %d",
                 __func__, candidateList[i].channelId);
            return LBD_NOK;
        }
        wlanif_phyCapInfo_t phyCap = { LBD_FALSE /* valid */};
        if (LBD_NOK == wlanifBSteerControlGetBSSPHYCapInfo(state, &candidateList[i], &phyCap) ||
            !phyCap.valid) {
            dbgf(state->dbgModule, DBGERR, "%s: Failed to resolve PHY capability for candidate "
                lbBSSInfoAddFmt(), __func__, lbBSSInfoAddData(&candidateList[i]));
            return LBD_NOK;
        }
        reqinfo.candidates[i].phy_type = wlanifMapToPhyType(phyCap.phyMode);
    }

    dbgf(state->dbgModule, DBGINFO, "%s: Sending BTM Request TO Driver:%d %d %d %d",
         __func__, reqinfo.dialogtoken,
        reqinfo.num_candidates, reqinfo.trans_reason, reqinfo.disassoc);

    // Send on the VAP this STA is associated on
    struct wlanifBSteerControlVapInfo *vap =
        (struct wlanifBSteerControlVapInfo *)assocBSS->vap;
#if SONLIB_SUPPORT_ENABLED
    return vap->drvHandle->ops->sendBTMReq(vap->drvHandle->ctx, vap->ifname,
                 staAddr->ether_addr_octet, &reqinfo);
#else
    return wlanifBSteerControlCmnSetSendVAP(
        state, vap->ifname, IEEE80211_DBGREQ_SENDBSTMREQ_TARGET, staAddr,
        (void *)&reqinfo, sizeof(reqinfo), LBD_FALSE /* ignoreError */);
#endif
}

LBD_STATUS wlanifBSteerControlRestartChannelUtilizationMonitoring(
        wlanifBSteerControlHandle_t state) {
    // Sanity check
    if (!state) {
        return LBD_NOK;
    }

    if (wlanifBSteerControlCmnLock(state))
        dbgf(state->dbgModule, DBGERR, "%s: Band steer control lock failed",__func__);

    if (wlanifBSteerControlDisable(state) == LBD_NOK) {
        dbgf(state->dbgModule, DBGERR,
             "%s: Temporarily disabling on both bands failed",
             __func__);
        if (wlanifBSteerControlCmnUnlock(state))
            dbgf(state->dbgModule, DBGERR, "%s: Band steer control unlock failed",__func__);
        return LBD_NOK;
    }

    // Flush the ACLs as we could be disabled for a while if the new channel
    // is a DFS one.
    wlanifBSteerControlCmnFlushACLs(state, wlanif_band_24g);
    wlanifBSteerControlCmnFlushACLs(state, wlanif_band_5g);

    LBD_BOOL enabled = LBD_FALSE;
    if (wlanifBSteerControlEnableWhenReady(state, &enabled) == LBD_NOK) {
        dbgf(state->dbgModule, DBGERR,
             "%s: Re-enabling on both bands failed", __func__);
        if (wlanifBSteerControlCmnUnlock(state))
            dbgf(state->dbgModule, DBGERR, "%s: Band steer control unlock failed",__func__);
        return LBD_NOK;
    }

    if (enabled) {
        dbgf(state->dbgModule, DBGINFO, "%s: Restart complete", __func__);
        evloopTimeoutUnregister(&state->vapReadyTimeout);
    }

    if (wlanifBSteerControlCmnUnlock(state))
        dbgf(state->dbgModule, DBGERR, "%s: Band steer control unlock failed",__func__);

    return LBD_OK;
}

void wlanifBSteerControlHandleRSSIMeasurement(
        wlanifBSteerControlHandle_t state,
        const lbd_bssInfo_t *bss,
        const struct ether_addr *staAddr) {
    struct wlanifBSteerControlVapInfo *vap =
        wlanifBSteerControlCmnExtractVapHandle(bss);
    if (!state || !vap || !staAddr) {
        return;
    }

    struct wlanifBSteerControlRadioInfo *radio = vap->radio;
    lbDbgAssertExit(state->dbgModule, radio);
    if (list_is_empty(&radio->rssiWaitingList)) {
        dbgf(state->dbgModule, DBGERR, "%s: No RSSI measurement is pending (received one from "
                                       lbMACAddFmt(":")").",
             __func__, lbMACAddData(staAddr->ether_addr_octet));
        return;
    }

    wlanifBSteerControlCmnRSSIRequestEntry_t *head =
        list_first_entry(&radio->rssiWaitingList,
                         wlanifBSteerControlCmnRSSIRequestEntry_t,
                         listChain);

    if (!lbAreEqualMACAddrs(head->addr.ether_addr_octet,
                            staAddr->ether_addr_octet)) {
        dbgf(state->dbgModule, DBGERR, "%s: Expecting RSSI measurement from "
                                       lbMACAddFmt(":")", received one from "
                                       lbMACAddFmt(":")".",
             __func__, lbMACAddData(head->addr.ether_addr_octet),
             lbMACAddData(staAddr->ether_addr_octet));
        return;
    }

    list_remove_entry(&head->listChain);
    free(head);

    if (list_is_empty(&radio->rssiWaitingList)) {
        return;
    }

    list_head_t *iter = radio->rssiWaitingList.next;
    while (iter != &radio->rssiWaitingList) {
        wlanifBSteerControlCmnRSSIRequestEntry_t *curEntry =
            list_entry(iter, wlanifBSteerControlCmnRSSIRequestEntry_t, listChain);

        iter = iter->next;

        if (LBD_NOK == wlanifBSteerControlCmnSendRequestRSSI(state, curEntry->vap, &curEntry->addr,
                                                          curEntry->numSamples)) {
            // If request RSSI fails, do not retry, rely on RSSI xing event to update RSSI
            dbgf(state->dbgModule, DBGERR, "%s: Failed to request RSSI measurement for "
                                           lbMACAddFmt(":")".",
                 __func__, lbMACAddData(curEntry->addr.ether_addr_octet));
            list_remove_entry(&curEntry->listChain);
            free(curEntry);
        } else {
            dbgf(state->dbgModule, DBGDEBUG, "%s: RSSI measurement request for STA "
                                         lbMACAddFmt(":")" is dequeued and sent.",
                 __func__, lbMACAddData(curEntry->addr.ether_addr_octet));
            break;
        }
    }
}

LBD_STATUS wlanifBSteerControlGetBSSInfo(wlanifBSteerControlHandle_t state,
                                         u_int32_t sysIndex, lbd_bssInfo_t *bss) {
    if (!state || !bss) {
        return LBD_NOK;
    }

    struct wlanifBSteerControlVapInfo *vap =
            wlanifBSteerControlCmnGetVAPFromSysIndex(state, sysIndex, wlanif_band_invalid);

    if (vap) {
        bss->apId = LBD_APID_SELF;
        bss->essId = vap->essId;
        bss->channelId = vap->radio->channel;
        bss->vap = vap;

        return LBD_OK;
    }

    // No match found
    return LBD_NOK;
}

LBD_STATUS wlanifBSteerControlIsIncludedIface(wlanifBSteerControlHandle_t state,
                                               u_int32_t sysIndex) {
    struct wlanifBSteerControlVapInfo *vap;

    if (!state) {
        return LBD_NOK;
    }

    vap = wlanifBSteerControlCmnGetVAPFromSysIndex(state, sysIndex, wlanif_band_invalid);

    if ((vap) && (vap->includedIface))
        return LBD_OK;
    else
        return LBD_NOK;
}

/**
 * @brief Enable stats collection for direct attach
 *
 * @param [in] state the handle returned from
 *                   wlanifBSteerControlCreate()
 * @param [in] radio  radio for which stats collection needs to
 *                    be enabled
 */
static LBD_STATUS wlanifBSteerControlCmnEnableNoDebug(
    wlanifBSteerControlHandle_t state, const struct wlanifBSteerControlRadioInfo *radio) {
    LBD_STATUS result = LBD_OK;
    if (0 == radio->numEnableStats) {
        result =
            wlanifBSteerControlCmnPrivIoctlSetParam(state, radio->ifname,
                                                    radio->enableNoDebug,
                                                    0); /*its a -ve ioctl so 0 means enable */
    }
    return result;
}

/**
 * @brief Enable the collection of byte and packet count
 *        statistics on the provided radio.
 *
 * @param [in] handle  the handle returned from wlanifBSteerControlCreate()
 *                     to use for enabling the stats
 * @param [in] radio  radio for which stats collection needs to
 *                    be enabled
 *
 * @return LBD_OK on success; otherwise LBD_NOK
 */
static LBD_STATUS wlanifBSteerControlEnableSTAStatsRadio(
    wlanifBSteerControlHandle_t state,
    struct wlanifBSteerControlRadioInfo *radio) {

    LBD_STATUS result = LBD_OK;
    if (radio->enableNoDebug) {
        result = wlanifBSteerControlCmnEnableNoDebug(state, radio);
    }
    else if (radio->enableOLStatsIoctl) {
        if (0 == radio->numEnableStats) {
            result = wlanifBSteerControlCmnPrivIoctlSetParam(
                         state, radio->ifname,
                         radio->enableOLStatsIoctl, 1);
        }
    }
    // Otherwise, must be a radio that does not require stats to be
    // enabled. Succeed without doing anything.

    // Keep track of the number of enables so that when an equivalent number
    // of disables is done, we can set the driver back into no stats mode.
    //
    // Note that this bookkeeping is done even when the ioctl was not resolved
    // so that we can enforce that an enable has to be done prior to sampling
    // the STA stats.
    if (LBD_OK == result) {
        radio->numEnableStats++;
    }
    return result;
}


LBD_STATUS wlanifBSteerControlEnableSTAStats(wlanifBSteerControlHandle_t state,
                                             const lbd_bssInfo_t *bss) {
    struct wlanifBSteerControlVapInfo *vap =
        wlanifBSteerControlCmnExtractVapHandle(bss);
    if (!state || !vap) {
        return LBD_NOK;
    }

    return wlanifBSteerControlEnableSTAStatsRadio(state, vap->radio);
}

LBD_STATUS wlanifBSteerControlSampleSTAStats(wlanifBSteerControlHandle_t state,
                                             const lbd_bssInfo_t *bss,
                                             const struct ether_addr *staAddr,
                                             LBD_BOOL rateOnly,
                                             wlanif_staStatsSnapshot_t *staStats) {
    struct wlanifBSteerControlVapInfo *vap =
        wlanifBSteerControlCmnExtractVapHandle(bss);
    if (!state || !vap || !staAddr || !staStats) {
        return LBD_NOK;
    }

    if (vap->radio->numEnableStats || rateOnly) {
#if SONLIB_SUPPORT_ENABLED
        struct soncmn_staStatsSnapshot_t stats;
        if (vap->drvHandle->ops->getStaStats(vap->drvHandle->ctx, vap->ifname,
                                       staAddr->ether_addr_octet, &stats) != LBD_OK) {
#else
        struct ieee80211req_sta_stats stats;
        if (wlanifBSteerControlCmnGetSTAStats(state, vap, staAddr, &stats) != LBD_OK) {
#endif

            dbgf(state->dbgModule, DBGERR,
                 "%s: Failed to retrieve STA stats for " lbMACAddFmt(":") " on %s",
                 __func__, lbMACAddData(staAddr->ether_addr_octet),
                 vap->ifname);
            return LBD_NOK;
        }

        // Success, so fill in the out parameter.
        if (!rateOnly) {
            // Tx and Rx byte counts will only be valid if called with
            // stats enabled.
#if SONLIB_SUPPORT_ENABLED
            staStats->txBytes = stats.txBytes;
            staStats->rxBytes = stats.rxBytes;
#else
            staStats->txBytes = stats.is_stats.ns_tx_bytes_success;
            staStats->rxBytes = stats.is_stats.ns_rx_bytes;
#endif
        } else {
            staStats->txBytes = 0;
            staStats->rxBytes = 0;
        }

        // Rates are reported in Kbps, so convert them to Mbps.
#if SONLIB_SUPPORT_ENABLED
        staStats->lastTxRate = stats.lastTxRate / 1000;
        staStats->lastRxRate = stats.lastRxRate / 1000;
#ifdef WIFISON_SUPPORT_QSDK
        // Raw counter of the number of packets successfully sent
        staStats->packetsSent = stats.ns_tx_data_success;

        // Raw counter of the number of packets which could not be transmitted
        staStats->txPacketsErrors = stats.ns_tx_data
                                    - stats.ns_tx_data_success;

        // Raw counter of the number of packets received from the associated
        staStats->packetsReceived = stats.ns_rx_data;

        // time at which the earliest measurement that contributed to the data rate estimates
        // were made
        staStats->timeDelta = stats.ns_rssi_time_delta;

        // Raw counter of the number of packets which were received in error
        staStats->rxPacketsErrors =  stats.ns_rx_tkipmic + /* rx TKIP MIC failure */
                                     stats.ns_rx_ccmpmic + /* rx CCMP MIC failure */
                                     stats.ns_rx_wpimic +  /* rx WAPI MIC failure */
                                     stats.ns_rx_tkipicv + /* rx ICV check failed (TKIP) */
                                     stats.ns_rx_decap +   /* rx decapsulation failed */
                                     stats.ns_rx_defrag;   /* rx defragmentation failed */

        // most recent received packet RSSI from connected sta
        staStats->updatedRSSI = stats.ns_rssi;

        // Raw counter of the number of packets sent with the retry flag set
        staStats->retransmissionCount = stats.ns_is_tx_not_ok;
#endif
#else
        staStats->lastTxRate = stats.is_stats.ns_last_tx_rate / 1000;
        staStats->lastRxRate = stats.is_stats.ns_last_rx_rate / 1000;

        // Raw counter of the number of packets successfully sent
        staStats->packetsSent = stats.is_stats.ns_tx_data_success;

        // Raw counter of the number of packets which could not be transmitted
        staStats->txPacketsErrors = stats.is_stats.ns_tx_data
                                    - stats.is_stats.ns_tx_data_success;

        // Raw counter of the number of packets received from the associated
        staStats->packetsReceived = stats.is_stats.ns_rx_data;

        // time at which the earliest measurement that contributed to the data rate estimates
        // were made
        staStats->timeDelta = stats.is_stats.ns_rssi_time_delta;

        // Raw counter of the number of packets which were received in error
        staStats->rxPacketsErrors =  stats.is_stats.ns_rx_tkipmic + /* rx TKIP MIC failure */
                                     stats.is_stats.ns_rx_ccmpmic + /* rx CCMP MIC failure */
                                     stats.is_stats.ns_rx_wpimic +  /* rx WAPI MIC failure */
                                     stats.is_stats.ns_rx_tkipicv + /* rx ICV check failed (TKIP) */
                                     stats.is_stats.ns_rx_decap +   /* rx decapsulation failed */
                                     stats.is_stats.ns_rx_defrag;   /* rx defragmentation failed */

        // most recent received packet RSSI from connected sta
        staStats->updatedRSSI = stats.is_stats.ns_rssi;

        // Raw counter of the number of packets sent with the retry flag set
        staStats->retransmissionCount = stats.is_stats.ns_is_tx_not_ok;
#endif
        return LBD_OK;
    } else {   // stats are not enabled
        dbgf(state->dbgModule, DBGERR,
             "%s: Cannot sample STA stats for " lbMACAddFmt(":") " on %s "
             "as stats collection is not enabled",
             __func__, lbMACAddData(staAddr->ether_addr_octet),
             vap->ifname);
        return LBD_NOK;
    }
}

/**
 * @brief Disable the collection of byte and packet count
 *        statistics on the provided radio.
 *
 * @param [in] handle  the handle returned from wlanifBSteerControlCreate()
 *                     to use for enabling the stats
 * @param [in] radio  radio for which stats collection needs to
 *                    be enabled
 *
 * @return LBD_OK on success; otherwise LBD_NOK
 */
static LBD_STATUS wlanifBSteerControlDisableSTAStatsRadio(
    wlanifBSteerControlHandle_t state,
    struct wlanifBSteerControlRadioInfo *radio) {

    LBD_STATUS result = LBD_OK;
    if (radio->enableNoDebug) {
        result = wlanifBSteerControlCmnDisableNoDebug(state, radio);
    }
    else if (radio->enableOLStatsIoctl) {
        if (1 == radio->numEnableStats) {
            result =
                wlanifBSteerControlCmnPrivIoctlSetParam(
                        state, radio->ifname,
                        radio->enableOLStatsIoctl, 0);
        }
    }
    // Otherwise, must be a radio that does not require stats to be
    // disabled. Succeed without doing anything.
    if (LBD_OK == result && radio->numEnableStats) {
        radio->numEnableStats--;
    }

    return result;
}

LBD_STATUS wlanifBSteerControlDisableSTAStats(wlanifBSteerControlHandle_t state,
                                              const lbd_bssInfo_t *bss) {
    struct wlanifBSteerControlVapInfo *vap =
        wlanifBSteerControlCmnExtractVapHandle(bss);
    if (!state || !vap) {
        return LBD_NOK;
    }

    return wlanifBSteerControlDisableSTAStatsRadio(state, vap->radio);
}

/**
 * @brief Disable stats collection on direct attach
 *
 * @param [in] state the handle returned from
 *                    wlanifBSteerControlCreate()
 * @param [in] radio  radio for which stats collection needs to
 *                    be disabled
 */
static LBD_STATUS wlanifBSteerControlCmnDisableNoDebug(
        wlanifBSteerControlHandle_t state, const struct wlanifBSteerControlRadioInfo *radio) {

    LBD_STATUS result = LBD_OK;
    if (1 == radio->numEnableStats) {
        result = wlanifBSteerControlCmnPrivIoctlSetParam(state, radio->ifname,
                                                         radio->enableNoDebug,
                                                         1); /*-ve ioctl 1 mean disable here */
    }
    return result;
}

/**
 * @brief Enable or disable STA stats on all radios on a band
 *
 * @param [in] state the handle returned from
 *                    wlanifBSteerControlCreate()
 * @param [in] band  band to enable or disable on
 * @param [in] enable  LBD_TRUE if enabling; LBD_FALSE otherwise
 *
 * @return LBD_OK on success; LBD_NOK otherwise
 */
static LBD_STATUS wlanifBSteerControlCmnEnableSTAStatsBand(wlanifBSteerControlHandle_t state,
                                                           wlanif_band_e band,
                                                           LBD_BOOL enable) {
    size_t i;
    LBD_STATUS ret = LBD_OK;

    if (!state->inactDetectionFromTx &&
        !state->bandInfo[band].configParams.interference_detection_enable) {
        // No inactivity detection from tx and no interference detection - nothing to do
        return LBD_OK;
    }

    for (i = 0; i < WLANIF_MAX_RADIOS; ++i) {
        if (!state->radioInfo[i].valid) {
            break;
        } else if (wlanif_resolveBandFromChannelNumber(
            state->radioInfo[i].channel) != band) {
            continue;
        }

        if (enable) {
            if (wlanifBSteerControlEnableSTAStatsRadio(state, &state->radioInfo[i]) != LBD_OK) {
                ret = LBD_NOK;
            }
        } else {
            if (wlanifBSteerControlDisableSTAStatsRadio(state, &state->radioInfo[i]) != LBD_OK) {
                ret = LBD_NOK;
            }
        }
    }

    return ret;
}

LBD_STATUS wlanifBSteerControlUpdateChannel(wlanifBSteerControlHandle_t state,
                                            wlanif_band_e band,
                                            u_int32_t sysIndex,
                                            u_int32_t frequency) {
    if (!state || band >= wlanif_band_invalid) {
        return LBD_NOK;
    }

    // Find the VAP which has changed channel.
    struct wlanifBSteerControlVapInfo *vap =
            wlanifBSteerControlCmnGetVAPFromSysIndex(state, sysIndex, band);
    if (vap) {
        // Found match - get the frequency for the VAP
        if (frequency <= 1000) {
            // This is a channel - need to resolve the regclass
            vap->radio->channel = frequency;

            if (wlanif_resolveRegClass(
                frequency,
                &vap->radio->regClass) != LBD_OK) {
                dbgf(state->dbgModule, DBGERR,
                     "%s: Invalid regulatory class for radio %s, channel is %d",
                     __func__, vap->radio->ifname, frequency);
                return LBD_NOK;
            }
        } else {
            // This is a frequency - resolve channel and regclass
            if (wlanifResolveRegclassAndChannum(
                frequency, &vap->radio->channel, &vap->radio->regClass) != LBD_OK) {
                dbgf(state->dbgModule, DBGERR,
                     "%s: Invalid channel / regulatory class for radio %s, frequency is %d",
                     __func__, vap->radio->ifname, frequency);
                return LBD_NOK;
            }
        }

        vap->radio->maxTxPower = 0;
        wlanifBSteerControlCmnNotifyChanChangeObserver(state, vap);

        // Log the updated interface
        wlanifBSteerControlCmnLogInterfaceInfo(state, vap);

        return LBD_OK;
    }

    // No match found
    return LBD_NOK;
}

LBD_STATUS wlanifBSteerControlDumpATFTable(wlanifBSteerControlHandle_t state,
                                           wlanif_reservedAirtimeCB callback,
                                           void *cookie) {
    size_t i;
    for (i = 0; i < wlanif_band_invalid; ++i) {
        size_t j;
        for (j = 0; j < MAX_VAP_PER_BAND; ++j) {
            if (state->bandInfo[i].vaps[j].valid) {
                if (wlanifBSteerControlCmnDumpATFTableOneIface(state,
                            &state->bandInfo[i].vaps[j],
                            callback, cookie) != LBD_OK) {
                    return LBD_NOK;
                }
            } else {
                // No more valid VAPs on this band
                break;
            }
        }
    }

    return LBD_OK;
}

LBD_BOOL wlanifBSteerControlIsSTAAssociated(wlanifBSteerControlHandle_t state,
                                            const lbd_bssInfo_t *bss,
                                            const struct ether_addr *staAddr) {
    struct wlanifBSteerControlVapInfo *vap =
        wlanifBSteerControlCmnExtractVapHandle(bss);
    if (!state || !staAddr || !vap) {
        return LBD_FALSE;
    }

#if SONLIB_SUPPORT_ENABLED
    struct soncmn_staStatsSnapshot_t stats;
    if (vap->drvHandle->ops->getStaStats(vap->drvHandle->ctx, vap->ifname,
                            staAddr->ether_addr_octet, &stats) == LBD_OK) {
#else
    struct ieee80211req_sta_stats stats;
    if (wlanifBSteerControlCmnGetSTAStats(state, vap,
                                       staAddr, &stats) == LBD_OK) {
#endif
        // STA is associated on bss
        return LBD_TRUE;
    }

    // STA is not associated on bss
    return LBD_FALSE;
}

LBD_STATUS wlanifBSteerControlRegisterChanChangeObserver(
        wlanifBSteerControlHandle_t state, wlanif_chanChangeObserverCB callback,
        void *cookie) {
    if (!callback) {
        return LBD_NOK;
    }

    struct wlanifBSteerControlChanChangeObserver *freeSlot = NULL;
    size_t i;
    for (i = 0; i < MAX_CHAN_CHANGE_OBSERVERS; ++i) {
        struct wlanifBSteerControlChanChangeObserver *curSlot = &state->chanChangeObserver[i];
        if (curSlot->isValid && curSlot->callback == callback &&
            curSlot->cookie == cookie) {
            dbgf(state->dbgModule, DBGERR, "%s: Duplicate registration "
                                               "(func %p, cookie %p)",
                 __func__, callback, cookie);
           return LBD_NOK;
        }

        if (!freeSlot && !curSlot->isValid) {
            freeSlot = curSlot;
        }

    }

    if (freeSlot) {
        freeSlot->isValid = LBD_TRUE;
        freeSlot->callback = callback;
        freeSlot->cookie = cookie;
        return LBD_OK;
    }

    // No free entry found
    return LBD_NOK;
}

LBD_STATUS wlanifBSteerControlUnregisterChanChangeObserver(
        wlanifBSteerControlHandle_t state, wlanif_chanChangeObserverCB callback,
        void *cookie) {
    if (!callback) {
        return LBD_NOK;
    }

    size_t i;
    for (i = 0; i < MAX_CHAN_CHANGE_OBSERVERS; ++i) {
        struct wlanifBSteerControlChanChangeObserver *curSlot = &state->chanChangeObserver[i];
        if (curSlot->isValid && curSlot->callback == callback &&
            curSlot->cookie == cookie) {
            curSlot->isValid = LBD_FALSE;
            curSlot->callback = NULL;
            curSlot->cookie = NULL;
            return LBD_OK;
        }
    }

    // No match found
    return LBD_NOK;
}

void wlanifBSteerControlUpdateMaxTxPower(wlanifBSteerControlHandle_t state,
                                         const lbd_bssInfo_t *bss,
                                         u_int16_t maxTxPower) {
    struct wlanifBSteerControlVapInfo *vap = wlanifBSteerControlCmnExtractVapHandle(bss);
    if (!state || !vap || !maxTxPower) { return; }

    vap->phyCapInfo.maxTxPower = maxTxPower;
    dbgf(state->dbgModule, DBGINFO,
         "%s: Max Tx power changed to %d dBm on " lbBSSInfoAddFmt(),
         __func__, maxTxPower, lbBSSInfoAddData(bss));
    // When there is Tx power change on one VAP, we assume it also changes
    // on all other VAPs on the same radio.
    if (maxTxPower != vap->radio->maxTxPower) {
        vap->radio->maxTxPower = maxTxPower;
        wlanifBSteerControlCmnFindStrongestRadioOnBand(
            state, wlanif_resolveBandFromChannelNumber(vap->radio->channel));

        wlanifBSteerControlNotifyRadioOperChanChange(state);
    }
}

LBD_STATUS wlanifBSteerControlIsStrongestChannel(
        wlanifBSteerControlHandle_t state, lbd_channelId_t channelId,
        LBD_BOOL *isStrongest) {
    if (!state || channelId == LBD_CHANNEL_INVALID || !isStrongest) {
        return LBD_NOK;
    }

    size_t i;
    for (i = 0; i < WLANIF_MAX_RADIOS; ++i) {
        if (state->radioInfo[i].valid &&
            state->radioInfo[i].channel == channelId) {
            *isStrongest = state->radioInfo[i].strongestRadio;
            return LBD_OK;
        }
    }

    return LBD_NOK;
}

LBD_STATUS wlanifBSteerControlIsBSSOnStrongestChannel(
        wlanifBSteerControlHandle_t state, const lbd_bssInfo_t *bss,
        LBD_BOOL *isStrongest) {
    struct wlanifBSteerControlVapInfo *vap = wlanifBSteerControlCmnExtractVapHandle(bss);
    if (!state || !vap || !isStrongest) { return LBD_NOK; }

    *isStrongest = vap->radio->strongestRadio;

    return LBD_OK;
}

LBD_STATUS wlanifBSteerControlGetIfaceStatus(const lbd_bssInfo_t *bss) {
    struct wlanifBSteerControlVapInfo *vap = wlanifBSteerControlCmnExtractVapHandle(bss);

    if (!vap) {
        return LBD_NOK;
    }
    if (vap->ifaceUp == LBD_FALSE){
        return LBD_NOK;
    } else {
        return LBD_OK;
    }
}

LBD_STATUS wlanifBSteerControlGetBSSesSameESSLocal(
        wlanifBSteerControlHandle_t state, const lbd_bssInfo_t *bss,
        lbd_essId_t lastServingESS, wlanif_band_e requestedBand,
        size_t* maxNumBSSes, lbd_bssInfo_t *bssList) {
    struct wlanifBSteerControlVapInfo *vap = wlanifBSteerControlCmnExtractVapHandle(bss);
    if (!state || (!vap && lastServingESS == LBD_ESSID_INVALID) ||
        !bssList || !maxNumBSSes || !(*maxNumBSSes)) {
        return LBD_NOK;
    }

    size_t i, numBSSes = 0, numBands = wlanif_band_invalid;
    struct wlanifBSteerControlVapInfo *vapEntry = NULL;

    wlanif_band_e band = requestedBand;
    lbd_essId_t essId = lastServingESS;
    if (band == wlanif_band_invalid) {
        // If not specify band, start with BSSes on the same band as serving
        // BSS, or 2.4 GHz if not associated
        if (bss) {
            band = wlanif_resolveBandFromChannelNumber(bss->channelId);
        } else {
            band = wlanif_band_24g;
        }
    }
    if (bss)  {
        // If specify BSS, find entries within same ESS as the given BSS
        essId = bss->essId;
    }
    lbDbgAssertExit(state->dbgModule, band != wlanif_band_invalid);
    lbDbgAssertExit(state->dbgModule, essId != LBD_ESSID_INVALID);

    while (numBands--) {
        for (i = 0; i < MAX_VAP_PER_BAND; ++i) {
            vapEntry = &state->bandInfo[band].vaps[i];
            if (!vapEntry->valid) {
                break;
            } else if (vapEntry == vap) {
                // Ignore current BSS
                continue;
            } else if (vapEntry->essId != essId) {
                // Ignore BSS on other ESS
                continue;
            }

            if (numBSSes < *maxNumBSSes) {
                bssList[numBSSes].apId = LBD_APID_SELF;
                bssList[numBSSes].essId = vapEntry->essId;
                bssList[numBSSes].channelId = vapEntry->radio->channel;
                bssList[numBSSes].vap = vapEntry;
                ++numBSSes;
            } else {
                // Reach maximum BSSes requested, return here
                return LBD_OK;
            }
        }

        if (requestedBand != wlanif_band_invalid) {
            // Only request single band BSSes, return
            *maxNumBSSes = numBSSes;
            return LBD_OK;
        }

        // Find BSSes on the other band
        band = band == wlanif_band_24g ? wlanif_band_5g : wlanif_band_24g;
    }

    *maxNumBSSes = numBSSes;
    return LBD_OK;
}

LBD_STATUS wlanifBSteerControlUpdateSteeringStatus(
        wlanifBSteerControlHandle_t state, const struct ether_addr *addr,
        const lbd_bssInfo_t *bss, LBD_BOOL steeringInProgress) {
    struct wlanifBSteerControlVapInfo *vap =
        wlanifBSteerControlCmnExtractVapHandle(bss);
    if (!state || !addr || !vap) { return LBD_NOK; }

#if SONLIB_SUPPORT_ENABLED
    return vap->drvHandle->ops->setBSteerInProgress(vap->drvHandle->ctx,
                            vap->ifname, addr->ether_addr_octet, steeringInProgress);
#else
    return wlanifBSteerControlCmnSetSendVAP(
        state, vap->ifname,
        IEEE80211_DBGREQ_BSTEERING_SET_STEERING,
        addr, (void *) &steeringInProgress, sizeof(steeringInProgress),
        LBD_FALSE /* ignoreError */);
#endif
}

// ====================================================================
// Functions shared internally by BSA and MBSA
// ====================================================================

LBD_STATUS wlanifBSteerControlCmnGetLocalBSSInfoFromBSSID(
    wlanifBSteerControlHandle_t state, const u_int8_t *bssid,
    lbd_bssInfo_t *bss) {

    wlanif_band_e band;
    int i;

    if (!state || !bss || !bssid) {
        return LBD_NOK;
    }

    for (band = wlanif_band_24g ; band < wlanif_band_invalid; ++band) {
        for (i = 0; i < MAX_VAP_PER_BAND; ++i) {
            if (!state->bandInfo[band].vaps[i].valid) {
                break;
            }
            if (lbAreEqualMACAddrs(bssid,
                                   state->bandInfo[band].vaps[i].macaddr.ether_addr_octet)) {
                // Found match
                bss->apId = LBD_APID_SELF;
                bss->essId = state->bandInfo[band].vaps[i].essId;
                bss->channelId = state->bandInfo[band].vaps[i].radio->channel;
                bss->vap = &state->bandInfo[band].vaps[i];

                return LBD_OK;
            }
        }
    }

    // No match found
    return LBD_NOK;
}

const struct ether_addr *wlanifBSteerControlCmnGetBSSIDForLocalBSSInfo(
        const lbd_bssInfo_t *bssInfo) {
    if (lbIsBSSLocal(bssInfo)) {
        const struct wlanifBSteerControlVapInfo *vap =
            wlanifBSteerControlCmnExtractVapHandle(bssInfo);
        return (vap) ? &vap->macaddr : NULL;
    };

    return NULL;
}

LBD_STATUS wlanifBSteerControlCmnGetLocalBSSPHYCapInfo(
        wlanifBSteerControlHandle_t state, const lbd_bssInfo_t *bss,
        wlanif_phyCapInfo_t *phyCap) {
    if (state && lbIsBSSLocal(bss) && phyCap) {
        struct wlanifBSteerControlVapInfo *vap =
            wlanifBSteerControlCmnExtractVapHandle(bss);
        if (vap) {
            memcpy(phyCap, &vap->phyCapInfo, sizeof(wlanif_phyCapInfo_t));
            return LBD_OK;
        }
    }

    return LBD_NOK;
}

LBD_STATUS wlanifBSteerControlCmnResolveSSID(wlanifBSteerControlHandle_t state,
                                             lbd_essId_t essId, char *ssid,
                                             u_int8_t *len) {
    if (!ssid || !len || (essId >= state->essCount) ||
        ((*len) < state->essInfo[essId].ssidLen)) {
        return LBD_NOK;
    }

    memcpy(ssid, state->essInfo[essId].ssidStr, state->essInfo[essId].ssidLen);
    *len = state->essInfo[essId].ssidLen;
    return LBD_OK;
}

LBD_STATUS wlanifBSteerControlCmnGetRadioAddrFromBSS(wlanifBSteerControlHandle_t handle,
                                                     const lbd_bssInfo_t *bss,
                                                     struct ether_addr *addr) {
    if (!bss || !handle || !addr) return LBD_NOK;

    struct wlanifBSteerControlVapInfo *vap = wlanifBSteerControlCmnExtractVapHandle(bss);
    if (vap) {
        struct wlanifBSteerControlRadioInfo *radio = vap->radio;
        if (radio) {
            lbCopyMACAddr(&radio->radioAddr.ether_addr_octet, addr->ether_addr_octet);
            return LBD_OK;
        }
    }

    return LBD_NOK;
}
