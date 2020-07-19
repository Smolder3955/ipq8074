// vim: set et sw=4 sts=4 cindent:
/*
 * @File: stadbEntry.c
 *
 * @Abstract: Implementation of accessors and mutators for stadbEntry
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

#include <stdlib.h>
#include <time.h>
#include <stdint.h>

#ifdef LBD_DBG_MENU
#include <cmd.h>
#endif

#include "stadbEntry.h"
#include "stadbEntryPrivate.h"
#include "stadbDiaglogDefs.h"
#include "stadb.h"

#include "lb_common.h"
#include "lb_assert.h"
#include "diaglog.h"

// Forward decls
static void stadbEntryMarkBandSupported(stadbEntry_handle_t handle,
                                        const lbd_bssInfo_t *bss);

static time_t stadbEntryGetTimestamp(void);
static void stadbEntryUpdateTimestamp(stadbEntry_handle_t entry);
static void stadbEntryBSSStatsUpdateTimestamp(stadbEntry_bssStatsHandle_t bssHandle);
static void stadbEntryResetBSSStatsEntry(stadbEntry_bssStatsHandle_t bssHandle,
                                         const lbd_bssInfo_t *newBSS);
static void stadbEntryFindBestPHYMode(stadbEntry_handle_t entry);
static LBD_BOOL stadbEntryIsValidAssociation(const struct timespec *ts,
                                             const lbd_bssInfo_t *bss,
                                             stadbEntry_handle_t entry,
                                             LBD_BOOL wasAssoc,
                                             LBD_BOOL expectAssoc,
                                             time_t assocAge);
static LBD_BOOL stadbEntryUpdateBandPHYCapInfo(stadbEntry_handle_t handle,
                                               wlanif_band_e band,
                                               const wlanif_phyCapInfo_t *newPHYCap);

static size_t stadbEntryDetermineEntrySizeAndType(
        LBD_BOOL inNetwork, LBD_BOOL trackRemoteAssoc,
        wlanif_capStateUpdate_e rrmStatus, size_t numRadiosServing,
        size_t numNonServingBSSStats, size_t *numBSSStats, stadbEntryType_e *type);
static void stadbEntryRealloc(stadbEntry_handle_t entry);

static LBD_BOOL stadbEntryIsSameServingAP(stadbEntry_handle_t entry,
                                          const lbd_bssInfo_t *bss);
static stadbEntry_bssStatsHandle_t stadbEntryFindSlotForBSSStats(
        stadbEntry_handle_t entry, const lbd_bssInfo_t *bss);
static LBD_BOOL stadbEntryIsBSSOlder(stadbEntry_bssStatsHandle_t bssStat1,
                                     stadbEntry_bssStatsHandle_t bssStat2);
static void stadbEntrySetSupportedBand(stadbEntry_handle_t entry,
                                       wlanif_band_e band);
static void stadbEntrySetLoadBalancingRejected(stadbEntry_handle_t entry,
                                               u_int8_t loadbalancingStaState);
static u_int8_t stadbEntryGetLoadBalancingRejected(stadbEntry_handle_t entry);


// Minimum time since association occurred when disassociation message
// is received.  If disassociation is received before this time, verify
// if the STA is really disassociated.  500 ms.
static const struct timespec STADB_ENTRY_MIN_TIME_ASSOCIATION = {0, 500000000};

const struct ether_addr *stadbEntry_getAddr(const stadbEntry_handle_t handle) {
    if (handle) {
        return &handle->addr;
    }

    return NULL;
}

LBD_BOOL stadbEntry_isMatchingAddr(const stadbEntry_handle_t entry,
                                   const struct ether_addr *addr) {
    if (!entry || !addr) {
        return LBD_FALSE;
    }

    return lbAreEqualMACAddrs(entry->addr.ether_addr_octet,
                              addr->ether_addr_octet);
}

LBD_BOOL stadbEntry_isBandSupported(const stadbEntry_handle_t entry,
                                    wlanif_band_e band) {
    if (!entry || band >= wlanif_band_invalid) {
        return LBD_FALSE;
    }

    return (entry->operatingBands & 1 << band) != 0;
}

LBD_BOOL stadbEntry_isSteeringDisallowed(const stadbEntry_handle_t entry){
    if (!entry) {
        return LBD_FALSE;
    }

    return entry->isSteeringDisallowed;
}

LBD_STATUS stadbEntry_setBandSupported(const stadbEntry_handle_t entry,
                                       wlanif_band_e band) {
    if (!entry || band >= wlanif_band_invalid) {
        return LBD_NOK;
    }

    if ((entry->operatingBands & (1 << band)) == 0) {
        stadbEntrySetDirtyIfInNetwork(entry);
    }

    entry->operatingBands |= 1 << band;
    return LBD_OK;
}

LBD_STATUS stadbEntry_setRemoteBandSupported(const stadbEntry_handle_t entry,
                                             wlanif_band_e band) {
    if (stadbEntry_setBandSupported(entry, band) == LBD_NOK) {
        return LBD_NOK;
    }

    stadbEntry_bssStatsHandle_t servingBSS = stadbEntry_getServingBSS(entry, NULL);
    const lbd_bssInfo_t *bssInfo = stadbEntry_resolveBSSInfo(servingBSS);

    stadbEntryPopulateBSSesFromSameESS(entry, bssInfo, band);

    return LBD_OK;
}

LBD_BOOL stadbEntry_isDualBand(const stadbEntry_handle_t entry) {
    if (!entry) {
        return LBD_FALSE;
    }

    u_int8_t mask = (1 << wlanif_band_24g | 1 << wlanif_band_5g);
    return (entry->operatingBands & mask) == mask;
}

wlanif_band_e stadbEntry_getAssociatedBand(const stadbEntry_handle_t entry,
                                           time_t *deltaSecs) {
    if (!stadbEntry_isInNetwork(entry)) {
        return wlanif_band_invalid;
    }

    if (!entry->inNetworkInfo->assoc.bssHandle) {
        return wlanif_band_invalid;
    }

    if (deltaSecs) {
        time_t curTime = stadbEntryGetTimestamp();
        *deltaSecs = curTime - entry->inNetworkInfo->assoc.lastAssoc.tv_sec;
    }

    return stadbEntry_resolveBandFromBSSStats(entry->inNetworkInfo->assoc.bssHandle);
}

LBD_BOOL stadbEntry_isInNetwork(const stadbEntry_handle_t entry) {
    if (!entry) {
        return LBD_FALSE;
    }

    return entry->entryType == stadbEntryType_inNetworkLocal ||
           entry->entryType == stadbEntryType_inNetworkLocalRemote;
}

LBD_STATUS stadbEntry_getAge(const stadbEntry_handle_t entry, time_t *ageSecs) {
    if (!entry || !ageSecs) {
        return LBD_NOK;
    }

    *ageSecs = stadbEntryGetTimestamp() - entry->lastUpdateSecs;
    return LBD_OK;
}

void *stadbEntry_getSteeringState(stadbEntry_handle_t entry) {
    if (stadbEntry_isInNetwork(entry)) {
        return entry->inNetworkInfo->steeringState;
    }

    return NULL;
}

LBD_STATUS stadbEntry_setSteeringState(
        stadbEntry_handle_t entry, void *state,
        stadbEntry_stateLifecycleCB_t callback) {
    if ((state && !callback) || !stadbEntry_isInNetwork(entry)) {
        return LBD_NOK;
    }

    entry->inNetworkInfo->steeringState = state;
    entry->inNetworkInfo->steeringStateLifecycleCB = callback;
    return LBD_OK;
}

void *stadbEntry_getEstimatorState(stadbEntry_handle_t entry) {
    if (stadbEntry_isInNetwork(entry)) {
        return entry->inNetworkInfo->estimatorState;
    }

    return NULL;
}

LBD_STATUS stadbEntry_setEstimatorState(
        stadbEntry_handle_t entry, void *state,
        stadbEntry_stateLifecycleCB_t callback) {
    if ((state && !callback) || !stadbEntry_isInNetwork(entry)) {
        return LBD_NOK;
    }

    entry->inNetworkInfo->estimatorState = state;
    entry->inNetworkInfo->estimatorStateLifecycleCB = callback;
    return LBD_OK;
}

void *stadbEntry_getSteerMsgState(stadbEntry_handle_t entry) {
    if (stadbEntry_isInNetwork(entry)) {
        return entry->inNetworkInfo->steermsgState;
    }

    return NULL;
}

void *stadbEntry_getMapServiceState(stadbEntry_handle_t entry) {
    if (stadbEntry_isInNetwork(entry)) {
        return entry->inNetworkInfo->mapServiceState;
    }

    return NULL;
}

void *stadbEntry_getMonitorState(stadbEntry_handle_t entry) {
    if (stadbEntry_isInNetwork(entry)) {
        return entry->inNetworkInfo->monitorState;
    }

    return NULL;
}

LBD_STATUS stadbEntry_setSteerMsgState(
        stadbEntry_handle_t entry, void *state,
        stadbEntry_stateLifecycleCB_t callback) {
    if ((state && !callback) || !stadbEntry_isInNetwork(entry)) {
        return LBD_NOK;
    }

    entry->inNetworkInfo->steermsgState = state;
    entry->inNetworkInfo->steermsgStateLifecycleCB = callback;
    return LBD_OK;
}

LBD_STATUS stadbEntry_setMapServiceState(
        stadbEntry_handle_t entry, void *state,
        stadbEntry_stateLifecycleCB_t callback) {
    if ((state && !callback) || !stadbEntry_isInNetwork(entry)) {
        return LBD_NOK;
    }

    entry->inNetworkInfo->mapServiceState = state;
    entry->inNetworkInfo->mapServiceStateLifecycleCB = callback;
    return LBD_OK;
}

LBD_STATUS stadbEntry_setMonitorState(
        stadbEntry_handle_t entry, void *state,
        stadbEntry_stateLifecycleCB_t callback) {
    if ((state && !callback) || !stadbEntry_isInNetwork(entry)) {
        return LBD_NOK;
    }

    entry->inNetworkInfo->monitorState = state;
    entry->inNetworkInfo->monitorStateLifecycleCB = callback;
    return LBD_OK;
}

LBD_STATUS stadbEntry_getActStatus(const stadbEntry_handle_t entry, LBD_BOOL *active, time_t *deltaSecs) {
    if (!entry || !active) {
        return LBD_NOK;
    }

    if (!stadbEntry_getServingBSS(entry, NULL)) {
        // If an entry is not associated, there is no activity status
        return LBD_NOK;
    }

    if (deltaSecs) {
        time_t curTime = stadbEntryGetTimestamp();
        *deltaSecs = curTime - entry->inNetworkInfo->lastActUpdate;
    }

    *active = entry->isAct;

    return LBD_OK;
}

LBD_STATUS stadbEntry_setActStatus(const stadbEntry_handle_t entry, LBD_BOOL active) {
    if (!entry) {
        return LBD_NOK;
    }

    stadbEntry_bssStatsHandle_t bssHandle = stadbEntry_getServingBSS(entry, NULL);
    const lbd_bssInfo_t *bss = stadbEntry_resolveBSSInfo(bssHandle);
    if (!bss) {
        dbgf(NULL, DBGERR,
             "%s: Failed to get BSS info from BSSID for STA " lbMACAddFmt(":"), __func__,
             lbMACAddData(entry->addr.ether_addr_octet));
        return LBD_NOK;
    }

    return stadbEntryMarkActive(entry, bss, active);
}

// ====================================================================
// "Package" and private helper functions
// ====================================================================

stadbEntry_handle_t stadbEntryCreate(const struct ether_addr *addr, LBD_BOOL inNetwork,
                                     wlanif_capStateUpdate_e rrmStatus,
                                     LBD_BOOL trackRemoteAssoc,
                                     size_t numRadiosServing,
                                     size_t numNonServingBSSStats) {
    if (!addr) {
        return NULL;
    }

    size_t numBSSStats = 0;
    stadbEntryType_e entryType = stadbEntryType_invalid;
    size_t entrySize =
        stadbEntryDetermineEntrySizeAndType(inNetwork, trackRemoteAssoc,
                                            rrmStatus, numRadiosServing,
                                            numNonServingBSSStats, &numBSSStats,
                                            &entryType);

    stadbEntry_handle_t entry = calloc(1, entrySize);
    if (entry) {
        lbCopyMACAddr(addr->ether_addr_octet, entry->addr.ether_addr_octet);
        entry->entryType = entryType;
        stadbEntryUpdateTimestamp(entry);
        if (entryType != stadbEntryType_outOfNetwork) {
            entry->inNetworkInfo->assoc.apId = LBD_APID_SELF;
            entry->inNetworkInfo->assoc.channel = LBD_CHANNEL_INVALID;
            entry->inNetworkInfo->assoc.lastServingESS = LBD_ESSID_INVALID;

            entry->inNetworkInfo->numBSSStats = numBSSStats;
            // All BSS stats entries should be invalid at this point. Before using a new
            // BSS stats entry, need to make sure it is reset so that all fields have invalid
            // values. (Currently done in stadbEntryFindBSSStats)
        }
    }

    return entry;
}

stadbEntry_handle_t stadbEntryChangeEntryType(
        stadbEntry_handle_t entry, wlanif_capStateUpdate_e rrmStatus,
        LBD_BOOL trackRemoteAssoc, size_t numRadiosServing,
        size_t numNonServingBSSStats) {
    lbDbgAssertExit(NULL, entry && numRadiosServing);

    size_t numBSSStats = 0;
    stadbEntryType_e newEntryType = stadbEntryType_invalid;
    size_t entrySize =
        stadbEntryDetermineEntrySizeAndType(
                LBD_TRUE /* inNetwork */, trackRemoteAssoc, rrmStatus,
                numRadiosServing, numNonServingBSSStats, &numBSSStats,
                &newEntryType);

    if (entry->entryType == newEntryType) {
        return entry;
    }

    stadbEntry_handle_t newEntry = realloc(entry, entrySize);
    if (newEntry) {
        if (newEntry->entryType == stadbEntryType_outOfNetwork) {
            memset(newEntry->inNetworkInfo, 0, sizeof(stadbEntryPriv_inNetworkInfo_t));
            memset(newEntry->inNetworkInfo->bssStats, 0,
                   sizeof(stadbEntryPriv_bssStats_t) * numBSSStats);
            newEntry->inNetworkInfo->assoc.apId = LBD_APID_SELF;
            newEntry->inNetworkInfo->assoc.channel = LBD_CHANNEL_INVALID;
            newEntry->inNetworkInfo->assoc.lastServingESS = LBD_ESSID_INVALID;
        } else {
            // From in-network local to in-network local and remote
            memset(&newEntry->inNetworkInfo->bssStats[numRadiosServing], 0,
                   sizeof(stadbEntryPriv_bssStats_t) * numNonServingBSSStats);
        }
        newEntry->inNetworkInfo->numBSSStats = numBSSStats;
        newEntry->entryType = newEntryType;

        // All new allocated BSS stats entries should be invalid at this point. If an entry
        // was out-of-network, then all BSS entries are invalid; if it was in-network local
        // only entries, then all remote BSS entries are invalid, and local ones will remain
        // unchanged. Before using a new BSS stats entry, need to make sure it is reset so that
        // all fields have invalid values. (Currently done in stadbEntryFindBSSStats)

        // Let the lifecycle callbacks react to the fact that the entry was reallocated.
        stadbEntryRealloc(newEntry);
        stadbEntrySetDirtyIfInNetwork(newEntry);
    } else {  // realloc failed
        // In this case, we want to clean up the old memory block, since the
        // caller will interpret a non-NULL value as success.
        stadbEntryDestroy(entry);
    }

    return newEntry;
}

LBD_STATUS stadbEntryRecordRSSI(stadbEntry_handle_t entry,
                                const lbd_bssInfo_t *bss,
                                lbd_rssi_t rssi) {
    if (!entry || !bss) { return LBD_NOK; }

    stadbEntryMarkBandSupported(entry, bss);

    if (!stadbEntry_isInNetwork(entry)) {
        // No RSSI update for out-of-network STA
        return LBD_NOK;
    }

    time_t curTime = stadbEntryGetTimestamp();

    stadbEntry_bssStatsHandle_t bssHandle =
        stadbEntryFindBSSStats(entry, bss, LBD_FALSE /* matchOnly */);
    lbDbgAssertExit(NULL, bssHandle);

    bssHandle->uplinkInfo.rssi = rssi;
    bssHandle->uplinkInfo.lastUpdateSecs = curTime;
    bssHandle->uplinkInfo.probeCount = 0;
    bssHandle->uplinkInfo.estimate = LBD_FALSE;

    bssHandle->lastUpdateSecs = curTime;

    if (diaglog_startEntry(mdModuleID_StaDB,
                           stadb_msgId_rssiUpdate,
                           diaglog_level_demo)) {
        diaglog_writeMAC(&entry->addr);
        diaglog_writeBSSInfo(bss);
        diaglog_write8(rssi);
        diaglog_finishEntry();
    }

    return LBD_OK;
}

LBD_STATUS stadbEntryRecordProbeRSSI(stadbEntry_handle_t entry,
                                     const lbd_bssInfo_t *bss,
                                     lbd_rssi_t rssi, time_t maxInterval) {
    if (!entry || !bss) { return LBD_NOK; }

    stadbEntryMarkBandSupported(entry, bss);

    if (!stadbEntry_isInNetwork(entry)) {
        // No RSSI update for out-of-network STA
        return LBD_NOK;
    }

    if (entry->inNetworkInfo->assoc.bssHandle &&
        lbAreBSSesSame(bss, &entry->inNetworkInfo->assoc.bssHandle->bss)) {
        // Ignore probes on the associated band since they present an
        // instantaneous measurement and may not be as accurate as our
        // average RSSI report or triggered RSSI measurement which are
        // both taken over a series of measurements.
        return LBD_NOK;
    }

    lbd_essId_t lastServingESS = stadbEntry_getLastServingESS(entry);
    if (lastServingESS != LBD_ESSID_INVALID &&
        bss->essId != lastServingESS) {
        // Only handle probes when the STA has never associated or if STA
        // probes on the same ESS as last association
        return LBD_NOK;
    }

    time_t curTime = stadbEntryGetTimestamp();

    stadbEntry_bssStatsHandle_t bssHandle =
        stadbEntryFindBSSStats(entry, bss, LBD_FALSE /* matchOnly */);
    lbDbgAssertExit(NULL, bssHandle);

    bssHandle->lastUpdateSecs = curTime;

    if (!bssHandle->uplinkInfo.probeCount ||
        (curTime - bssHandle->uplinkInfo.lastUpdateSecs) > maxInterval) {
        // Reset probe RSSI averaging
        bssHandle->uplinkInfo.rssi = rssi;
        bssHandle->uplinkInfo.lastUpdateSecs = curTime;
        bssHandle->uplinkInfo.probeCount = 1;
        bssHandle->uplinkInfo.estimate = LBD_FALSE;
    } else {
        // Average this one with previous measurements
        bssHandle->uplinkInfo.rssi =
            (bssHandle->uplinkInfo.rssi * bssHandle->uplinkInfo.probeCount + rssi) /
            (bssHandle->uplinkInfo.probeCount + 1);
        bssHandle->uplinkInfo.lastUpdateSecs = curTime;
        ++bssHandle->uplinkInfo.probeCount;
    }

    if (diaglog_startEntry(mdModuleID_StaDB,
                           stadb_msgId_rssiUpdate,
                           diaglog_level_demo)) {
        diaglog_writeMAC(&entry->addr);
        diaglog_writeBSSInfo(bss);
        // Report averaged probe RSSI
        diaglog_write8(bssHandle->uplinkInfo.rssi);
        diaglog_finishEntry();
    }

    return LBD_OK;
}

/**
 * @brief Check if a given STA is associated on a given BSS
 *
 * @param [in] entry  the handle to the STA entry
 * @param [in] bss  the given BSS
 *
 * @pre the STA must be in-network and BSS is valid
 *
 * @return LBD_TRUE if the STA is associated on the given BSS; otherwise
 *         return LBD_FALSE
 */
static inline LBD_BOOL stadbEntryIsAssociatedOnBSS(stadbEntry_handle_t entry,
                                                   const lbd_bssInfo_t *bss) {
    return entry->inNetworkInfo->assoc.bssHandle &&
           lbAreBSSesSame(&entry->inNetworkInfo->assoc.bssHandle->bss, bss);
}

LBD_STATUS stadbEntryMarkAssociated(stadbEntry_handle_t entry,
                                    const lbd_bssInfo_t *bss,
                                    LBD_BOOL isAssociated,
                                    LBD_BOOL updateActive,
                                    LBD_BOOL verifyAssociation,
                                    time_t assocAge,
                                    LBD_BOOL *assocChanged) {
    if (assocChanged) {
        *assocChanged = LBD_FALSE;
    }
    if (!bss || !stadbEntry_isInNetwork(entry)) {
        return LBD_NOK;
    }

    // Did the association status change?
    LBD_BOOL assocSame = entry->inNetworkInfo->assoc.bssHandle &&
                         lbAreBSSesSame(&entry->inNetworkInfo->assoc.bssHandle->bss, bss);
    lbd_apId_t oldAssocAP = entry->inNetworkInfo->assoc.apId;
    lbd_channelId_t oldAssocChannel = entry->inNetworkInfo->assoc.channel;
    lbd_essId_t oldServingESS = entry->inNetworkInfo->assoc.lastServingESS;

    LBD_BOOL wasAssoc = (oldAssocChannel != LBD_CHANNEL_INVALID);

    struct timespec ts = {0};
    lbGetTimestamp(&ts);
    stadbEntryMarkBandSupported(entry, bss);

    if (isAssociated) {
        if (verifyAssociation) {
            if (!assocSame &&
                    !stadbEntryIsValidAssociation(&ts, bss, entry,
                                                  wasAssoc,
                                                  LBD_TRUE /* expectAssoc */,
                                                  assocAge)) {
                // STA is not actually associated even though we got an
                // update, ignore.
                return LBD_OK;
            }
        }

        // Only update the last association time if the VAP on which we
        // previously thought the STA was associated is out of date/wrong.
        if (!assocSame) {
            entry->inNetworkInfo->assoc.lastAssoc = ts;

            if (entry->inNetworkInfo->assoc.lastAssoc.tv_sec > assocAge) {
                entry->inNetworkInfo->assoc.lastAssoc.tv_sec -= assocAge;
            } else {
                entry->inNetworkInfo->assoc.lastAssoc.tv_sec = 0;
            }
        }

        stadbEntry_bssStatsHandle_t bssHandle =
            stadbEntryFindBSSStats(entry, bss, LBD_FALSE /* matchOnly */);
        lbDbgAssertExit(NULL, bssHandle);

        entry->inNetworkInfo->assoc.bssHandle = bssHandle;
        entry->inNetworkInfo->assoc.apId = bss->apId;
        entry->inNetworkInfo->assoc.channel = bss->channelId;
        if (bss->essId != LBD_ESSID_INVALID) {
            // Update the lastServingESS only if the current ESSID is valid
            entry->inNetworkInfo->assoc.lastServingESS = bss->essId;
        }
        if (updateActive) {
            // Also mark entry as active
            entry->isAct = LBD_TRUE;
            entry->inNetworkInfo->lastActUpdate = ts.tv_sec;
        }
    } else if ((assocSame &&
                stadbEntryIsValidAssociation(&ts, bss, entry,
                                             wasAssoc,
                                             LBD_FALSE /* expectAssoc */,
                                             assocAge)) ||
               !entry->inNetworkInfo->assoc.bssHandle) {
        entry->inNetworkInfo->assoc.bssHandle = NULL;
        entry->inNetworkInfo->assoc.apId = LBD_APID_SELF;
        entry->inNetworkInfo->assoc.channel = LBD_CHANNEL_INVALID;

        // Also mark entry as inactive
        entry->isAct = LBD_FALSE;
        entry->inNetworkInfo->lastActUpdate = ts.tv_sec;
        stadbEntrySetDirtyIfInNetwork(entry);
    }

    if (oldAssocAP != entry->inNetworkInfo->assoc.apId ||
            oldAssocChannel != entry->inNetworkInfo->assoc.channel ||
            oldServingESS != entry->inNetworkInfo->assoc.lastServingESS) {
        // Association status changed, including ESS change
        if (assocChanged) {
            *assocChanged = LBD_TRUE;
        }

        stadbEntryAssocDiagLog(entry, bss);
    }

    return LBD_OK;
}

LBD_STATUS stadbEntry_updateIsBTMSupported(stadbEntry_handle_t entry,
                                           LBD_BOOL isBTMSupported,
                                           LBD_BOOL *changed) {
    if (!entry) {
        return LBD_NOK;
    }

    if (changed) {
        if (entry->isBTMSupported == isBTMSupported) {
            *changed = LBD_FALSE;
        } else {
            *changed = LBD_TRUE;
            stadbEntrySetDirtyIfInNetwork(entry);
        }
    }

    // update if BTM is supported
    entry->isBTMSupported = isBTMSupported;

    return LBD_OK;
}

LBD_STATUS stadbEntry_updateIsRRMSupported(stadbEntry_handle_t entry,
                                           LBD_BOOL isRRMSupported) {
    if (!entry) {
        return LBD_NOK;
    }

    if (entry->isRRMSupported != isRRMSupported) {
        stadbEntrySetDirtyIfInNetwork(entry);
    }

    // update if RRM is supported
    entry->isRRMSupported = isRRMSupported;

    return LBD_OK;
}

LBD_STATUS stadbEntry_setClientClassGroup(stadbEntry_handle_t entry,
                                        u_int8_t sta_class) {
    if (!entry) {
        return LBD_NOK;
    }

    // update sta entry with client classification group info
    entry->clientClassGroup = sta_class;

    return LBD_OK;
}

LBD_STATUS stadbEntry_updateIsMUMIMOSupported(stadbEntry_handle_t entry,
                                              LBD_BOOL isMUMIMOSupported) {
    if (!entry) {
        return LBD_NOK;
    }

    if (entry->isMUMIMOSupported != isMUMIMOSupported) {
        stadbEntrySetDirtyIfInNetwork(entry);
    }

    // update if MU-MIMO is supported
    entry->isMUMIMOSupported = isMUMIMOSupported;

    return LBD_OK;
}

/**
 * @brief React to an entry that has been reallocated by informing the
 *        registered state objects.
 *
 * @param [in] entry  the entry that was reallocated
 */
static void stadbEntryRealloc(stadbEntry_handle_t entry) {
    lbDbgAssertExit(NULL, stadbEntry_isInNetwork(entry));
    if (entry->inNetworkInfo->steeringState) {
        lbDbgAssertExit(NULL, entry->inNetworkInfo->steeringStateLifecycleCB);
        entry->inNetworkInfo->steeringStateLifecycleCB(
                entry, entry->inNetworkInfo->steeringState);
    }
    if (entry->inNetworkInfo->estimatorState) {
        lbDbgAssertExit(NULL, entry->inNetworkInfo->estimatorStateLifecycleCB);
        entry->inNetworkInfo->estimatorStateLifecycleCB(
                entry, entry->inNetworkInfo->estimatorState);
    }
    if (entry->inNetworkInfo->steermsgState) {
        lbDbgAssertExit(NULL, entry->inNetworkInfo->steermsgStateLifecycleCB);
        entry->inNetworkInfo->steermsgStateLifecycleCB(
                entry, entry->inNetworkInfo->steermsgState);
    }
    if (entry->inNetworkInfo->mapServiceState) {
        lbDbgAssertExit(NULL, entry->inNetworkInfo->mapServiceStateLifecycleCB);
        entry->inNetworkInfo->mapServiceStateLifecycleCB(
                entry, entry->inNetworkInfo->mapServiceState);
    }
    if (entry->inNetworkInfo->monitorState) {
        lbDbgAssertExit(NULL, entry->inNetworkInfo->monitorStateLifecycleCB);
        entry->inNetworkInfo->monitorStateLifecycleCB(
                entry, entry->inNetworkInfo->monitorState);
    }
}

void stadbEntryDestroy(stadbEntry_handle_t entry) {
    if (stadbEntry_isInNetwork(entry)) {
        if (entry->inNetworkInfo->steeringState) {
            lbDbgAssertExit(NULL, entry->inNetworkInfo->steeringStateLifecycleCB);
            entry->inNetworkInfo->steeringStateLifecycleCB(
                    NULL, entry->inNetworkInfo->steeringState);
        }
        if (entry->inNetworkInfo->estimatorState) {
            lbDbgAssertExit(NULL, entry->inNetworkInfo->estimatorStateLifecycleCB);
            entry->inNetworkInfo->estimatorStateLifecycleCB(
                    NULL, entry->inNetworkInfo->estimatorState);
        }
        if (entry->inNetworkInfo->steermsgState) {
            lbDbgAssertExit(NULL, entry->inNetworkInfo->steermsgStateLifecycleCB);
            entry->inNetworkInfo->steermsgStateLifecycleCB(
                    NULL, entry->inNetworkInfo->steermsgState);
        }
        if (entry->inNetworkInfo->mapServiceState) {
            lbDbgAssertExit(NULL, entry->inNetworkInfo->mapServiceStateLifecycleCB);
            entry->inNetworkInfo->mapServiceStateLifecycleCB(
                    NULL, entry->inNetworkInfo->mapServiceState);
        }
        if (entry->inNetworkInfo->monitorState) {
            lbDbgAssertExit(NULL, entry->inNetworkInfo->monitorStateLifecycleCB);
            entry->inNetworkInfo->monitorStateLifecycleCB(
                    NULL, entry->inNetworkInfo->monitorState);
        }
    }

    if(entry && entry->steeringTimeHistory) {
        free(entry->steeringTimeHistory);
    }
    free(entry);
}

LBD_STATUS stadbEntryMarkActive(stadbEntry_handle_t entry,
                                const lbd_bssInfo_t *bss,
                                LBD_BOOL active) {
    if (!bss || !stadbEntry_isInNetwork(entry)) {
        return LBD_NOK;
    }

    // Only mark the entry as associated if it is being reported as active.
    // If we did it always, we might change the associated band due to the
    // STA having moved from one band to another without cleanly
    // disassociating.
    //
    // For example, if the STA is on 5 GHz and then moves to 2.4 GHz without
    // cleanly disassociating, the driver will still have an activity timer
    // running on 5 GHz. When that expires, if we mark it as associated, we
    // will clobber our current association state (of 2.4 GHz) with this 5 GHz
    // one. This will cause RSSI measurements and steering to be done with the
    // wrong band.
    //
    // Note that we do not update activity status, as it will be done
    // immediately below. We cannot let it mark the activity status as it
    // always sets the status to active for an associated device and inactive
    // for an unassociated device.
    if (active) {
        stadbEntryMarkAssociated(entry, bss, LBD_TRUE, /* isAssociated */
                                 LBD_FALSE /* updateActive */,
                                 LBD_TRUE /* verifyAssociation */,
                                 0 /* assocAge */,
                                 NULL /* assocChanged */);
    }

    // Only update the activity if the device is associated, as if it is not,
    // we do not know for sure that it is really a legitimate association
    // (see the note above for reasons).
    if (stadbEntryIsAssociatedOnBSS(entry, bss)) {
        entry->isAct = active;
        entry->inNetworkInfo->lastActUpdate = stadbEntryGetTimestamp();

        if (diaglog_startEntry(mdModuleID_StaDB, stadb_msgId_activityUpdate,
                               diaglog_level_demo)) {
            diaglog_writeMAC(&entry->addr);
            diaglog_writeBSSInfo(bss);
            diaglog_write8(entry->isAct);
            diaglog_finishEntry();
        }
    }

    return LBD_OK;
}

/**
 * @brief Mark the provided band as being supported by this entry.
 *
 * @param [in] handle  the handle to the entry to modify
 * @param [in] band  the band to enable for the entry
 *
 * @return LBD_OK on success; otherwise LBD_NOK
 */
static void stadbEntryMarkBandSupported(stadbEntry_handle_t entry,
                                        const lbd_bssInfo_t *bss) {
    lbDbgAssertExit(NULL, entry && bss);
    wlanif_band_e band = wlanif_resolveBandFromChannelNumber(bss->channelId);
    stadbEntrySetSupportedBand(entry, band);
    stadbEntryUpdateTimestamp(entry);
}

/**
 * @brief Set the provided band as being supported by this entry
 *
 * A dual band diaglog will be generated when the entry is marked as
 * dual band for the first time.
 *
 * @param [in] entry  the STA entry to set supported band
 * @param [in] band  the band to set
 */
static void stadbEntrySetSupportedBand(stadbEntry_handle_t entry,
                                       wlanif_band_e band) {
    LBD_BOOL wasDualBand = stadbEntry_isDualBand(entry);

    if ((entry->operatingBands & (1 << band)) == 0) {
        stadbEntrySetDirtyIfInNetwork(entry);
    }

    entry->operatingBands |= 1 << band;

    if (stadbEntry_isInNetwork(entry) &&
        !wasDualBand && stadbEntry_isDualBand(entry) &&
        diaglog_startEntry(mdModuleID_StaDB,
                           stadb_msgId_dualBandUpdate,
                           diaglog_level_demo)) {
        diaglog_writeMAC(&entry->addr);
        diaglog_write8(LBD_TRUE);
        diaglog_finishEntry();
    }
}

/**
 * @brief Set loadbalancingAttemptedButRejected field for this entry
 *
 * This private function will be used to mark as well as unmark the STA entries
 *
 * @param [in] entry the STA entry to set loadbalancingAttemptedButRejected
 * @param [in] LBD_BOOL value to set (TRUE or FALSE)
*/
static void stadbEntrySetLoadBalancingRejected(stadbEntry_handle_t entry,
                                               u_int8_t loadbalancingStaState) {
    if(stadbEntry_isInNetwork(entry))
    {
        entry->inNetworkInfo->loadbalancingAttemptedButRejected = loadbalancingStaState;
    }
}

/**
 * @brief Get loadbalancingAttemptedButRejected field for this entry
 *
 * This private function will be used to fetch the laodbalancingAttempteButRejected
 * Flag value.
 * @param [in] entry the StA entry to fetch the flag value
*/
static u_int8_t stadbEntryGetLoadBalancingRejected(stadbEntry_handle_t entry) {
    return entry->inNetworkInfo->loadbalancingAttemptedButRejected;
}

/**
 * @brief update the timestamp for this entry
 *
 * This function will be used to update the timestamp whenver a
 * steering has been initiated for this client. And check if the steering has reached the
 * threshold. If so then find  if the time difference is less than the Upgrade
 * allowed time interval. If so return true else false.
 *
 * @param [in] entry the STA entry for which the timestamp is to be updated
 */
LBD_BOOL stadbUpdateLegacyUpgradeSteeringTimestamp(stadbEntry_handle_t entry) {
    int legacyUpgradeAllowedCnt;
    int legacyUpgradeAllowedTime;
    struct timespec *first;
    struct timespec *last;
    struct timespec diff;
    u_int8_t clientClassGroup = 0;
    stadbEntry_getClientClassGroup(entry, &clientClassGroup);

    stadbGetLegacyUpgradeSteeringConfig(&legacyUpgradeAllowedCnt,
                                        &legacyUpgradeAllowedTime,
                                        clientClassGroup);
    struct timespec upgradeAllowedTime = {legacyUpgradeAllowedTime, 0};

    if (legacyUpgradeAllowedCnt != 0) {

        if (entry->steeringTimeHistory) {

            if (entry->steeringTimeId == legacyUpgradeAllowedCnt) {
                entry->steeringTimeId = 0;
            }

            lbGetTimestamp(&entry->steeringTimeHistory[entry->steeringTimeId]);
            entry->steeringTimeId++;

            if (entry->steeringTimeId == legacyUpgradeAllowedCnt) {
                entry->isLegacyUpgradeCntReached = LBD_TRUE;
            }

            if (entry->isLegacyUpgradeCntReached) {

                if (entry->steeringTimeId == legacyUpgradeAllowedCnt) {
                    last = &entry->steeringTimeHistory[entry->steeringTimeId -1];
                    first = &entry->steeringTimeHistory[0];
                } else {
                    last = &entry->steeringTimeHistory[entry->steeringTimeId -1];
                    first = &entry->steeringTimeHistory[entry->steeringTimeId];
                }

                lbTimeDiff(last, first, &diff);
                if (lbIsTimeBefore(&diff, &upgradeAllowedTime)) {
                    //For starting the legacy Upgrade steering unfriendly timer
                    return LBD_TRUE;
                }
            }
        }
    }
    return LBD_FALSE;
}

/**
 * @brief Determine if an association or disassociation is
 *        valid.
 *
 * On disassociation: Treated as valid if the current association is
 * older than STADB_ENTRY_MIN_TIME_ASSOCIATION or the STA is
 * verified as disassociated via wlanif
 *
 * On association: Treated as valid if the STA is verified as
 * associated on bss via wlanif or the association age is newer than
 * the current association age.
 *
 * @param [in] ts  current time
 * @param [in] bss BSS to check for association on
 * @param [in] entry  stadb entry to check for association
 * @param [in] wasAssoc  whether the STA was previously marked as associated
 * @param [in] expectAssoc  if LBD_TRUE check if STA is
 *                          associated on bss; if LBD_FALSE
 *                          check if STA is disassociated
 *                          on bss
 * @param [in] assocAge  the number of seconds elapsed since the STA
 *                       associated
 *
 * @return LBD_TRUE if the association is valid; LBD_FALSE
 *         otherwise
 */
static LBD_BOOL stadbEntryIsValidAssociation(const struct timespec *ts,
                                             const lbd_bssInfo_t *bss,
                                             stadbEntry_handle_t entry,
                                             LBD_BOOL wasAssoc,
                                             LBD_BOOL expectAssoc,
                                             time_t assocAge) {
    if (!expectAssoc) {
        // If this is disassociation, check the time relative to the last
        // association.
        struct timespec diff;
        lbTimeDiff(ts, &entry->inNetworkInfo->assoc.lastAssoc, &diff);
        if (lbIsTimeAfter(&diff, &STADB_ENTRY_MIN_TIME_ASSOCIATION)) {
            // The association happened more than the min time ago, treat it as valid
            return LBD_TRUE;
        }
    }

    if (lbIsBSSLocal(bss)) {
        // For a local BSS, we can at least validate the driver is providing
        // the same information as the event.
        LBD_BOOL isAssociated = wlanif_isSTAAssociated(bss, &entry->addr);

        if (expectAssoc != isAssociated) {
            return LBD_FALSE;
        } else if (!expectAssoc) {
            // Due to the above if check, when expectAssoc is false, it must
            // also be the case that isAssociated is false.
            return LBD_TRUE;
        }
        // else expectAssoc && isAssociated, fall through to check the age
    }

    if (expectAssoc && !wasAssoc) {
        // This is an association and previously was not associated, so it
        // must be valid.
        return LBD_TRUE;
    } else if (expectAssoc) {
        // We are handling an association. Check that the association age
        // reported is less than the age indicated by the last association.
        struct timespec newAssoc = *ts;
        newAssoc.tv_sec = (newAssoc.tv_sec > assocAge) ?
            newAssoc.tv_sec - assocAge : 0;

        struct timespec diff;
        lbTimeDiff(ts, &entry->inNetworkInfo->assoc.lastAssoc, &diff);
        return lbIsTimeAfter(&newAssoc,
                             &entry->inNetworkInfo->assoc.lastAssoc);
    }

    // If we reach this point, it must have been a remote disassociation that
    // occurred quickly after an association. Since we don't have a good way
    // to tell if this is right, we act conservatively and say that it is not.
    return LBD_FALSE;
}

LBD_BOOL stadbEntry_isBTMSupported(const stadbEntry_handle_t entry) {
    lbDbgAssertExit(NULL, entry);

    return entry->isBTMSupported;
}

LBD_BOOL stadbEntry_isRRMSupported(const stadbEntry_handle_t entry) {
    lbDbgAssertExit(NULL, entry);

    return entry->isRRMSupported;
}

LBD_BOOL stadbEntry_isMUMIMOSupported(const stadbEntry_handle_t entry) {
    lbDbgAssertExit(NULL, entry);

    return entry->isMUMIMOSupported;
}

LBD_STATUS stadbEntry_getClientClassGroup(const stadbEntry_handle_t entry, u_int8_t *sta_class) {
    if (!entry) {
        return LBD_NOK;
    }

    *sta_class = entry->clientClassGroup;

    return LBD_OK;
}

/**
 * @brief Get a timestamp in seconds for use in delta computations.
 *
 * @return the current time in seconds
 */
static time_t stadbEntryGetTimestamp(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    return ts.tv_sec;
}

/**
 * @brief Update the timestamp in the entry that stores the last time it was
 *        updated.
 *
 * @param [in] entry   the handle to the entry to update
 */
static void stadbEntryUpdateTimestamp(stadbEntry_handle_t entry) {
    lbDbgAssertExit(NULL, entry);
    entry->lastUpdateSecs = stadbEntryGetTimestamp();
}

u_int8_t stadbEntryComputeHashCode(const struct ether_addr *addr) {
    return lbMACAddHash(addr->ether_addr_octet);
}

#ifdef LBD_DBG_MENU
static const char *stadbEntryChWidthString[] = {
    "20MHz",
    "40MHz",
    "80MHz",
    "160MHz",
    "NA"
};

static const char *stadbEntryPHYModeString[] = {
    "BASIC",
    "HT",
    "VHT",
    "NA"
};

// Definitions used to help format debug output,
#define MAC_ADDR_STR_LEN 25
#define BSS_INFO_STR_LEN 30
#define ASSOC_STR_LEN (BSS_INFO_STR_LEN + 10) // BSS (age)
#define ACTIVITY_STR_LEN 20
#define PHY_FIELD_LEN 15
#define RSSI_STR_LEN 25 // RSSI (age) (flag)
#define RATE_INFO_FIELD_LEN 15
#define RESERVED_AIRTIME_LEN 20
#define BAND_STR_LEN 10
#define POLLUTION_FLAG_LEN 25 // Polluted (expiry secs)

void stadbEntryPrintSummaryHeader(struct cmdContext *context, LBD_BOOL inNetwork) {
    cmdf(context, "%-*s%-10s%-10s", MAC_ADDR_STR_LEN, "MAC Address", "Age", "Bands");
    if (inNetwork) {
        cmdf(context, "%-*s%-*s%-10s\n",
             ASSOC_STR_LEN, "Assoc? (age)", ACTIVITY_STR_LEN, "Active? (age)", "Flags");
    } else {
        cmdf(context, "\n");
    }
}

void stadbEntryPrintSummary(const stadbEntry_handle_t entry,
                            struct cmdContext *context,
                            LBD_BOOL inNetwork) {
    if (!entry) {
        return;
    } else if (stadbEntry_isInNetwork(entry) ^ inNetwork) {
        return;
    }

    time_t curTime = stadbEntryGetTimestamp();
    cmdf(context, lbMACAddFmt(":") "        %-10u%c%c        ",
         lbMACAddData(entry->addr.ether_addr_octet),
         curTime - entry->lastUpdateSecs,
         stadbEntry_isBandSupported(entry, wlanif_band_24g) ? '2' : ' ',
         stadbEntry_isBandSupported(entry, wlanif_band_5g) ? '5' : ' ');

    if (!inNetwork) {
        cmdf(context, " %s  \n",entry->isSteeringDisallowed?"Steer Disallowed":"Steer Allowed");
        return;
    }

    char assocStr[ASSOC_STR_LEN + 1]; // Add one for null terminator
    if (!entry->inNetworkInfo->assoc.bssHandle) {
        snprintf(assocStr, sizeof(assocStr), "       (%u)",
                 (unsigned int)(curTime - entry->inNetworkInfo->assoc.lastAssoc.tv_sec));
    } else {
        snprintf(assocStr, sizeof(assocStr), lbBSSInfoAddFmt() " (%u)",
                 lbBSSInfoAddData(&entry->inNetworkInfo->assoc.bssHandle->bss),
                 (unsigned int)(curTime - entry->inNetworkInfo->assoc.lastAssoc.tv_sec));
    }
    cmdf(context, "%-*s", ASSOC_STR_LEN, assocStr);

    if (entry->inNetworkInfo->assoc.bssHandle) {
        char activityStr[ACTIVITY_STR_LEN + 1]; // Add one for null terminator
        snprintf(activityStr, sizeof(activityStr), "%-3s (%u)",
                 entry->isAct ? "yes" : "no",
                 (unsigned int)(curTime - entry->inNetworkInfo->lastActUpdate));
        cmdf(context, "%-*s", ACTIVITY_STR_LEN, activityStr);
    } else {
        cmdf(context, "%-*s", ACTIVITY_STR_LEN, " ");
    }

    cmdf(context, "%s%s%s%s%s%s",
         entry->isBTMSupported ? "BTM " : "",
         entry->isRRMSupported ? "RRM " : "",
         entry->hasReservedAirtime ? "RA " : "",
         entry->isMUMIMOSupported ? "MU " : "",
         entry->isStaticSMPS ? "PS" : "",
         entry->isSteeringDisallowed ? "  Steer Disallowed":"  Steer Allowed");

    u_int8_t clientClassGroup = 0;
    stadbEntry_getClientClassGroup(entry, &clientClassGroup);
    cmdf(context, " Class:%d", clientClassGroup);

    cmdf(context, "\n");
}

void stadbEntryPrintDetailHeader(struct cmdContext *context,
                                 stadbEntryDBGInfoType_e infoType,
                                 LBD_BOOL listAddr) {
    if (listAddr) {
        cmdf(context, "%-*s", MAC_ADDR_STR_LEN, "MAC Address");
    }
    switch (infoType) {
        case stadbEntryDBGInfoType_phy:
            cmdf(context, "%-*s%-*s%-*s%-*s%-*s%-*s",
                 BAND_STR_LEN, "Band",
                 PHY_FIELD_LEN, "MaxChWidth",
                 PHY_FIELD_LEN, "NumStreams",
                 PHY_FIELD_LEN, "PHYMode",
                 PHY_FIELD_LEN, "MaxMCS",
                 PHY_FIELD_LEN, "MaxTxPower");
            break;
        case stadbEntryDBGInfoType_bss:
            cmdf(context, "%-*s", BSS_INFO_STR_LEN, "BSS Info");
            cmdf(context, "%-*s%-*s%-*s%-*s",
                 RSSI_STR_LEN, "UL RSSI (age) (flags)",
                 RSSI_STR_LEN, "DL RCPI (age)",
                 RESERVED_AIRTIME_LEN, "Reserved Airtime",
                 POLLUTION_FLAG_LEN, "Polluted (expiry secs)");
            break;
        case stadbEntryDBGInfoType_rate_measured:
            cmdf(context, "%-*s", BSS_INFO_STR_LEN, "BSS Info");
            cmdf(context, "%-*s%-*s%-*s",
                 RATE_INFO_FIELD_LEN, "DLRate (Mbps)",
                 RATE_INFO_FIELD_LEN, "ULRate (Mbps)",
                 RATE_INFO_FIELD_LEN, "Age (seconds)");
            break;
        case stadbEntryDBGInfoType_rate_estimated:
            cmdf(context, "%-*s", BSS_INFO_STR_LEN, "BSS Info");
            cmdf(context, "%-*s%-*s%-*s",
                 RATE_INFO_FIELD_LEN, "fullCap (Mbps)",
                 RATE_INFO_FIELD_LEN, "airtime (%)",
                 RATE_INFO_FIELD_LEN, "Age (seconds)");
            break;
        default:
            break;
    }

    cmdf(context, "\n");
}

/**
 * @brief Parameters used when iterating BSS stats to print
 *        detailed information
 */
typedef struct stadbEntryPrintDetailCBParams_t {
    /// The context to print details
    struct cmdContext *context;
    /// The type of the detailed info to print
    stadbEntryDBGInfoType_e infoType;
    /// Whether to print MAC address
    LBD_BOOL listAddr;
} stadbEntryPrintDetailCBParams_t;

/**
 * @brief Print common information for each detailed info entry of a given STA
 *
 * @param [in] entry  the handle to the STA
 * @param [in] bssHandle  the handle to the BSS stats
 * @param [in] context  the output stream to print
 * @param [in] listAddr  whether to print MAC address
 */
static void stadbEntryPrintDetailCommonInfo(stadbEntry_handle_t entry,
                                            stadbEntry_bssStatsHandle_t bssHandle,
                                            struct cmdContext *context,
                                            LBD_BOOL listAddr) {
    if (listAddr) {
        char macStr[MAC_ADDR_STR_LEN + 1];
        snprintf(macStr, sizeof(macStr), lbMACAddFmt(":"), lbMACAddData(entry->addr.ether_addr_octet));
        cmdf(context, "%-*s", MAC_ADDR_STR_LEN, macStr);
    }

    char bssStr[BSS_INFO_STR_LEN + 1];
    snprintf(bssStr, sizeof(bssStr), lbBSSInfoAddFmt(), lbBSSInfoAddData(&bssHandle->bss));
    cmdf(context, "%-*s", BSS_INFO_STR_LEN, bssStr);
}

/**
 * @brief Callback function to print details of requested information on a BSS of a given STA
 *
 * @param [in] entry  the handle to the STA
 * @param [in] bssHandle  the handle to the BSS
 * @param [in] cookie  the parameters provided to the iteration
 *
 * @return LBD_FALSE (not used)
 */
static LBD_BOOL stadbEntryPrintDetailCB(stadbEntry_handle_t entry,
                                        stadbEntry_bssStatsHandle_t bssHandle,
                                        void *cookie) {
    stadbEntryPrintDetailCBParams_t *params = (stadbEntryPrintDetailCBParams_t *) cookie;

    switch (params->infoType) {
        case stadbEntryDBGInfoType_bss:
            // Always show BSS entry regardless of whether RSSI is valid or not
            stadbEntryPrintDetailCommonInfo(entry, bssHandle, params->context, params->listAddr);
            time_t curTime = stadbEntryGetTimestamp();
            if (bssHandle->uplinkInfo.rssi != LBD_INVALID_RSSI) {
                char rssiStr[RSSI_STR_LEN + 1];
                snprintf(rssiStr, sizeof(rssiStr), "%u (%lu) (%c%c)",
                         bssHandle->uplinkInfo.rssi,
                         curTime - bssHandle->uplinkInfo.lastUpdateSecs,
                         bssHandle->uplinkInfo.probeCount ? 'P' : ' ',
                         bssHandle->uplinkInfo.estimate ? 'E' : ' ');
                cmdf(params->context, "%-*s", RSSI_STR_LEN, rssiStr);
            } else {
                cmdf(params->context, "%-*s", RSSI_STR_LEN, " ");
            }

            if (bssHandle->downlinkInfo.rcpi != LBD_INVALID_RCPI) {
                char rssiStr[RSSI_STR_LEN + 1];
                snprintf(rssiStr, sizeof(rssiStr), "%d (%lu)",
                         bssHandle->downlinkInfo.rcpi,
                         curTime - bssHandle->downlinkInfo.lastUpdateSecs);
                cmdf(params->context, "%-*s", RSSI_STR_LEN, rssiStr);
            } else {
                cmdf(params->context, "%-*s", RSSI_STR_LEN, " ");
            }

            if (bssHandle->reservedAirtime != LBD_INVALID_AIRTIME) {
                char airtimeStr[RESERVED_AIRTIME_LEN + 1];
                snprintf(airtimeStr, sizeof(airtimeStr), "%u%%",
                         bssHandle->reservedAirtime);
                cmdf(params->context, "%-*s", RESERVED_AIRTIME_LEN, airtimeStr);
            } else {
                cmdf(params->context, "%-*s", RESERVED_AIRTIME_LEN, " ");
            }

            char pollutionFlagStr[POLLUTION_FLAG_LEN + 1];
            snprintf(pollutionFlagStr, sizeof(pollutionFlagStr),
                     "%s (%lu)", bssHandle->polluted ? "yes" : "no",
                     bssHandle->polluted && bssHandle->pollutionExpirySecs > curTime ?
                         bssHandle->pollutionExpirySecs - curTime : 0);
            cmdf(params->context, "%-*s", POLLUTION_FLAG_LEN, pollutionFlagStr);

            cmdf(params->context, "\n");
            break;
        case stadbEntryDBGInfoType_rate_estimated:
            if (bssHandle->downlinkInfo.fullCapacity != LBD_INVALID_LINK_CAP) {
                stadbEntryPrintDetailCommonInfo(entry, bssHandle, params->context, params->listAddr);
                time_t curTime = stadbEntryGetTimestamp();
                cmdf(params->context, "%-*u%-*u%-*lu\n",
                     RATE_INFO_FIELD_LEN, bssHandle->downlinkInfo.fullCapacity,
                     RATE_INFO_FIELD_LEN, bssHandle->downlinkInfo.airtime,
                     RATE_INFO_FIELD_LEN, curTime - bssHandle->downlinkInfo.lastUpdateSecs);
            }
            break;
        default:
            break;
    }

    return LBD_FALSE;
}

/**
 * @brief Print the measured rate info of a given STA
 *
 * @param [in] context  the output stream to print
 * @param [in] entry  the handle to the STA
 * @param [in] listAddr  whether to print MAC address
 */
static void stadbEntryPrintMeasuredRate(struct cmdContext *context,
                                        const stadbEntry_handle_t entry,
                                        LBD_BOOL listAddr) {
    if (!entry->inNetworkInfo->assoc.bssHandle || !entry->validDataRate) {
        // Ignore not associated STA or STA without measured rate
        return;
    }

    stadbEntryPrintDetailCommonInfo(entry, entry->inNetworkInfo->assoc.bssHandle, context, listAddr);
    time_t curTime = stadbEntryGetTimestamp();
    cmdf(context, "%-*u%-*u%-*lu\n",
         RATE_INFO_FIELD_LEN, entry->inNetworkInfo->dataRateInfo.downlinkRate,
         RATE_INFO_FIELD_LEN, entry->inNetworkInfo->dataRateInfo.uplinkRate,
         RATE_INFO_FIELD_LEN, curTime - entry->inNetworkInfo->dataRateInfo.lastUpdateSecs);

    cmdf(context, "\n");
}

/**
 * @brief Print PHY capability information for a given STA
 *
 * @param [in] context  the output stream to print
 * @param [in] entry  the handle to the STA
 * @param [in] listAddr  whether to print MAC address
 */
static void stadbEntryPrintPHYCapInfo(struct cmdContext *context,
                                      const stadbEntry_handle_t entry,
                                      LBD_BOOL listAddr) {
    wlanif_band_e band;
    for (band = wlanif_band_24g; band < wlanif_band_invalid; ++band) {
        const wlanif_phyCapInfo_t *phyCapInfo = &entry->inNetworkInfo->phyCapInfo[band];
        if (!phyCapInfo->valid) {
            continue;
        }
        if (listAddr) {
            char macStr[MAC_ADDR_STR_LEN + 1];
            snprintf(macStr, sizeof(macStr), lbMACAddFmt(":"),
                     lbMACAddData(entry->addr.ether_addr_octet));
            cmdf(context, "%-*s", MAC_ADDR_STR_LEN, macStr);
        }
        cmdf(context, "%-*c%-*s%-*u%-*s%-*u%-*u\n",
             BAND_STR_LEN, band == wlanif_band_24g ? '2' : '5',
             PHY_FIELD_LEN, stadbEntryChWidthString[phyCapInfo->maxChWidth],
             PHY_FIELD_LEN, phyCapInfo->numStreams,
             PHY_FIELD_LEN, stadbEntryPHYModeString[phyCapInfo->phyMode],
             PHY_FIELD_LEN, phyCapInfo->maxMCS,
             PHY_FIELD_LEN, phyCapInfo->maxTxPower);
    }
}

void stadbEntryPrintDetail(struct cmdContext *context,
                           const stadbEntry_handle_t entry,
                           stadbEntryDBGInfoType_e infoType,
                           LBD_BOOL listAddr) {
    if (infoType == stadbEntryDBGInfoType_rate_measured) {
        // Only have one measured rate info per STA
        stadbEntryPrintMeasuredRate(context, entry, listAddr);
    } else if (infoType == stadbEntryDBGInfoType_phy) {
        // One PHY capability info per band
        stadbEntryPrintPHYCapInfo(context, entry, listAddr);
    } else {
        // Other info will be one per BSS entry
        stadbEntryPrintDetailCBParams_t params = {
            context, infoType, listAddr
        };
        stadbEntry_iterateBSSStats(entry, stadbEntryPrintDetailCB, &params, NULL, NULL);
    }
}

#undef POLLUTION_FLAG_LEN
#undef BAND_STR_LEN
#undef RESERVED_AIRTIME_LEN
#undef MAC_ADDR_STR_LEN
#undef BSS_INFO_STR_LEN
#undef ASSOC_STR_LEN
#undef ACTIVITY_STR_LEN
#undef PHY_FIELD_LEN
#undef RSSI_STR_LEN
#undef RATE_INFO_FIELD_LEN

#endif /* LBD_DBG_MENU */

/*****************************************************
 *  New APIs
 ****************************************************/
/**
 * @brief Add BSS that meets requirement to the selected list
 *
 * The list is sorted, and this BSS will be inserted before the entries that
 * have a lower metric than it, or to the end if none. If there are already
 * enough better BSSes selected, do nothing
 *
 * @pre sortedMetrics must be initialized to all 0
 *
 * @param [inout] selectedBSSList  the list to insert BSS
 * @param [inout] sortedMetrics  the list to insert metric, the order must be
 *                               the same as selectedBSSList
 * @param [in] bssStats  the handle to the BSS
 * @param [in] metric  the metric returned from callback function for this BSS
 * @param [in] maxNumBSS  maximum number of BSS requested
 * @param [inout] numBSSSelected  number of BSS being added to the list
 */
static void stadbEntryAddBSSToSelectedList(lbd_bssInfo_t *selectedBSSList,
                                           u_int32_t *sortedMetrics,
                                           stadbEntryPriv_bssStats_t *bssStats,
                                           u_int32_t metric, size_t maxNumBSS,
                                           size_t *numBSSSelected) {
    size_t i;
    for (i = 0; i < maxNumBSS; ++i) {
        if (metric > sortedMetrics[i]) {
            // Need to move all entries from i to right by 1, last one will be discarded
            size_t numEntriesToMove = maxNumBSS - i - 1;
            if (numEntriesToMove) {
                memmove(&selectedBSSList[i + 1], &selectedBSSList[i],
                        sizeof(lbd_bssInfo_t) * numEntriesToMove);
                memmove(&sortedMetrics[i + 1], &sortedMetrics[i],
                        sizeof(u_int32_t) * numEntriesToMove);
            }
            lbCopyBSSInfo(&bssStats->bss, &selectedBSSList[i]);
            sortedMetrics[i] = metric;
            if (*numBSSSelected < maxNumBSS) {
                ++*numBSSSelected;
            }
            return;
        }
    }
}

LBD_STATUS stadbEntry_iterateBSSStats(stadbEntry_handle_t entry, stadbEntry_iterBSSFunc_t callback,
                                      void *cookie, size_t *maxNumBSS, lbd_bssInfo_t *bss) {
    // Sanity check
    if (!callback || (maxNumBSS && !bss) || ((!maxNumBSS || !(*maxNumBSS)) && bss) ||
        !stadbEntry_isInNetwork(entry)) {
        //No BSS iteration should be done for out-of-network
        return LBD_NOK;
    }

    size_t i, numBSSSelected = 0;
    stadbEntryPriv_bssStats_t *bssStats = NULL;
    u_int32_t metric = 0;
    u_int32_t *sortedMetrics = calloc(entry->inNetworkInfo->numBSSStats,
                                      sizeof(u_int32_t));
    if (!sortedMetrics) { return LBD_NOK; }

    for (i = 0; i < entry->inNetworkInfo->numBSSStats; ++i) {
        bssStats = &entry->inNetworkInfo->bssStats[i];
        if (bssStats->valid &&
            (entry->inNetworkInfo->assoc.lastServingESS == LBD_ESSID_INVALID ||
             bssStats->bss.essId == entry->inNetworkInfo->assoc.lastServingESS)) {
            metric = callback(entry, bssStats, cookie);
            if (bss && metric) {
                stadbEntryAddBSSToSelectedList(bss, sortedMetrics, bssStats,
                                               metric, *maxNumBSS, &numBSSSelected);
            }
        }
    }

    if (maxNumBSS) {
        *maxNumBSS = numBSSSelected;
    }

    free(sortedMetrics);
    return LBD_OK;
}

LBD_STATUS stadbEntry_getPHYCapInfo(const stadbEntry_handle_t entry,
                                    const stadbEntry_bssStatsHandle_t bssHandle,
                                    wlanif_phyCapInfo_t *phyCapInfo) {
    if (!entry || !bssHandle || !phyCapInfo) {
        return LBD_NOK;
    }
    lbDbgAssertExit(NULL, bssHandle->valid);

    wlanif_band_e band = stadbEntry_resolveBandFromBSSStats(bssHandle);
    lbDbgAssertExit(NULL, band != wlanif_band_invalid);

    return stadbEntry_getPHYCapInfoByBand(entry, band, phyCapInfo);
}

LBD_STATUS stadbEntry_getPHYCapInfoByBand(const stadbEntry_handle_t entry,
                                          wlanif_band_e band,
                                          wlanif_phyCapInfo_t *phyCapInfo) {
    if (!stadbEntry_isInNetwork(entry) || band >= wlanif_band_invalid ||
            !phyCapInfo) {
        return LBD_NOK;
    }

    if (!entry->inNetworkInfo->phyCapInfo[band].valid) {
        return LBD_NOK;
    }

    *phyCapInfo = entry->inNetworkInfo->phyCapInfo[band];
    if (entry->isStaticSMPS) {
        // When STA is operating in Static SMPS mode, NSS is forced to 1
        phyCapInfo->numStreams = 1;
    }

    return LBD_OK;
}

LBD_STATUS stadbEntrySetPHYCapInfoByBand(
        stadbEntry_handle_t entry, wlanif_band_e band,
        const wlanif_phyCapInfo_t *phyCapInfo) {
    if (!entry || band >= wlanif_band_invalid ||
            !phyCapInfo || !phyCapInfo->valid ||
            !stadbEntry_isInNetwork(entry)) {
        return LBD_NOK;
    }

    if (stadbEntryUpdateBandPHYCapInfo(entry, band, phyCapInfo)) {
        stadbEntryFindBestPHYMode(entry);
    }

    return LBD_OK;
}

void stadbEntry_getFullCapacities(const stadbEntry_handle_t entry,
                                        const stadbEntry_bssStatsHandle_t bssHandle,
                                        lbd_linkCapacity_t *ulCapacity,
                                        lbd_linkCapacity_t *dlCapacity,
                                        time_t *deltaUlSecs,
                                        time_t *deltaDlSecs) {
    if (!entry || !bssHandle) {
        if (ulCapacity) {
            *ulCapacity = LBD_INVALID_LINK_CAP;
        }
        if (dlCapacity) {
            *dlCapacity = LBD_INVALID_LINK_CAP;
        }
        return;
    }

    lbDbgAssertExit(NULL, bssHandle->valid);
    time_t curTime = stadbEntryGetTimestamp();
    if (deltaUlSecs) {
        *deltaUlSecs = curTime - bssHandle->uplinkInfo.lastUpdateSecs;
    }
    if (deltaDlSecs) {
        *deltaDlSecs = curTime - bssHandle->downlinkInfo.lastUpdateSecs;
    }

    if (ulCapacity) {
        *ulCapacity = bssHandle->uplinkInfo.fullCapacity;
    }
    if (dlCapacity) {
        *dlCapacity = bssHandle->downlinkInfo.fullCapacity;
    }
}

LBD_STATUS stadbEntry_setFullCapacities(stadbEntry_handle_t entry,
                                      stadbEntry_bssStatsHandle_t bssHandle,
                                      lbd_linkCapacity_t ulCapacity,
                                      lbd_linkCapacity_t dlCapacity) {
    if (!entry || !bssHandle) {
        return LBD_NOK;
    }
    lbDbgAssertExit(NULL, bssHandle->valid);

    if (dlCapacity != LBD_INVALID_LINK_CAP || ulCapacity != LBD_INVALID_LINK_CAP) {
        time_t curTime = stadbEntryGetTimestamp();
        if (dlCapacity != LBD_INVALID_LINK_CAP) {
            bssHandle->downlinkInfo.fullCapacity = dlCapacity;
            bssHandle->downlinkInfo.lastUpdateSecs = curTime;
        }
        if (ulCapacity != LBD_INVALID_LINK_CAP) {
            bssHandle->uplinkInfo.fullCapacity = ulCapacity;
            bssHandle->uplinkInfo.lastUpdateSecs = curTime;
        }
    }
    return LBD_OK;
}

LBD_STATUS stadbEntry_setFullCapacitiesByBSSInfo(stadbEntry_handle_t entry,
                                               const lbd_bssInfo_t *bss,
                                               lbd_linkCapacity_t ulCapacity,
                                               lbd_linkCapacity_t dlCapacity) {
    if (!bss || !stadbEntry_isInNetwork(entry)) {
        return LBD_NOK;
    }

    stadbEntry_bssStatsHandle_t bssHandle =
        stadbEntryFindBSSStats(entry, bss, LBD_FALSE /* matchOnly */);
    lbDbgAssertExit(NULL, bssHandle);

    if (dlCapacity != LBD_INVALID_LINK_CAP || ulCapacity != LBD_INVALID_LINK_CAP) {
        time_t curTime = stadbEntryGetTimestamp();
        bssHandle->lastUpdateSecs = curTime;
        if (dlCapacity != LBD_INVALID_LINK_CAP) {
            bssHandle->downlinkInfo.fullCapacity = dlCapacity;
            bssHandle->downlinkInfo.lastUpdateSecs = curTime;
        }
        if (ulCapacity != LBD_INVALID_LINK_CAP) {
            bssHandle->uplinkInfo.fullCapacity = ulCapacity;
            bssHandle->uplinkInfo.lastUpdateSecs = curTime;
        }
    }
    return LBD_OK;
}

lbd_rcpi_t stadbEntry_getRCPI(const stadbEntry_handle_t entry,
                              const stadbEntry_bssStatsHandle_t bssHandle,
                              time_t *deltaSecs) {
    if (!entry || !bssHandle) {
        return LBD_INVALID_RCPI;
    }
    lbDbgAssertExit(NULL, bssHandle->valid);

    if (deltaSecs) {
        time_t curTime = stadbEntryGetTimestamp();
        *deltaSecs = curTime - bssHandle->downlinkInfo.lastUpdateSecs;
    }

    return bssHandle->downlinkInfo.rcpi;
}

LBD_STATUS stadbEntry_setRCPI(stadbEntry_handle_t entry,
                              stadbEntry_bssStatsHandle_t bssHandle,
                              lbd_rcpi_t rcpi) {
    if (!entry || !bssHandle) {
        return LBD_NOK;
    }
    lbDbgAssertExit(NULL, bssHandle->valid);

    bssHandle->downlinkInfo.rcpi = rcpi;
    time_t curTime = stadbEntryGetTimestamp();
    bssHandle->downlinkInfo.lastUpdateSecs = curTime;

    return LBD_OK;
}

LBD_STATUS stadbEntry_setRCPIByBSSInfo(stadbEntry_handle_t entry,
                                       const lbd_bssInfo_t *bss,
                                       lbd_rcpi_t rcpi) {
    if (!bss || !stadbEntry_isInNetwork(entry)) {
        return LBD_NOK;
    }

    stadbEntry_bssStatsHandle_t bssHandle =
        stadbEntryFindBSSStats(entry, bss, LBD_FALSE /* matchOnly */);
    lbDbgAssertExit(NULL, bssHandle);

    bssHandle->downlinkInfo.rcpi = rcpi;
    time_t curTime = stadbEntryGetTimestamp();
    bssHandle->downlinkInfo.lastUpdateSecs = curTime;

    bssHandle->lastUpdateSecs = curTime;

    return LBD_OK;
}

lbd_rssi_t stadbEntry_getUplinkRSSI(const stadbEntry_handle_t entry,
                                    const stadbEntry_bssStatsHandle_t bssHandle,
                                    time_t *ageSecs, u_int8_t *probeCount) {
    if (!entry || !bssHandle) {
        return LBD_INVALID_RSSI;
    }
    lbDbgAssertExit(NULL, bssHandle->valid);

    if (ageSecs) {
        time_t curTime = stadbEntryGetTimestamp();
        *ageSecs = curTime - bssHandle->uplinkInfo.lastUpdateSecs;
    }

    if (probeCount) {
        *probeCount = bssHandle->uplinkInfo.probeCount;
    }

    return bssHandle->uplinkInfo.rssi;
}

LBD_STATUS stadbEntry_setUplinkRSSI(stadbEntry_handle_t entry,
                                    stadbEntry_bssStatsHandle_t bssHandle,
                                    lbd_rssi_t rssi, LBD_BOOL estimated) {
    if (!entry || !bssHandle) {
        return LBD_NOK;
    }
    lbDbgAssertExit(NULL, bssHandle->valid);

    bssHandle->uplinkInfo.rssi = rssi;
    bssHandle->uplinkInfo.estimate = estimated;
    bssHandle->uplinkInfo.probeCount = 0;

    time_t curTime = stadbEntryGetTimestamp();
    bssHandle->uplinkInfo.lastUpdateSecs = curTime;

    if (!estimated) {
        bssHandle->lastUpdateSecs = curTime;
    }
    return LBD_OK;
}

lbd_airtime_t stadbEntry_getAirtime(const stadbEntry_handle_t entry,
                                    const stadbEntry_bssStatsHandle_t bssHandle,
                                    time_t *deltaSecs) {
    if (!entry || !bssHandle) {
        return LBD_INVALID_AIRTIME;
    }
    lbDbgAssertExit(NULL, bssHandle->valid);

    if (deltaSecs) {
        time_t curTime = stadbEntryGetTimestamp();
        *deltaSecs = curTime - bssHandle->downlinkInfo.lastUpdateSecs;
    }

    return bssHandle->downlinkInfo.airtime;
}

LBD_STATUS stadbEntry_setAirtime(stadbEntry_handle_t entry,
                                 stadbEntry_bssStatsHandle_t bssHandle,
                                 lbd_airtime_t airtime) {
    if (!entry || !bssHandle) {
        return LBD_NOK;
    }
    lbDbgAssertExit(NULL, bssHandle->valid);

    bssHandle->downlinkInfo.airtime = airtime;

    return LBD_OK;
}

LBD_STATUS stadbEntry_setAirtimeByBSSInfo(stadbEntry_handle_t entry,
                                          const lbd_bssInfo_t *bss,
                                          lbd_airtime_t airtime) {
    if (!bss || !stadbEntry_isInNetwork(entry)) {
        return LBD_NOK;
    }

    stadbEntry_bssStatsHandle_t bssHandle =
        stadbEntryFindBSSStats(entry, bss, LBD_FALSE /* matchOnly */);
    lbDbgAssertExit(NULL, bssHandle);

    bssHandle->downlinkInfo.airtime = airtime;

    stadbEntryBSSStatsUpdateTimestamp(bssHandle);
    return LBD_OK;
}

LBD_STATUS stadbEntry_getLastDataRate(const stadbEntry_handle_t entry,
                                      lbd_linkCapacity_t *dlRate,
                                      lbd_linkCapacity_t *ulRate,
                                      time_t *deltaSecs) {
    if (!stadbEntry_isInNetwork(entry) || !entry->validDataRate) {
        return LBD_NOK;
    }

    if (dlRate) {
        *dlRate = entry->inNetworkInfo->dataRateInfo.downlinkRate;
    }

    if (ulRate) {
        *ulRate = entry->inNetworkInfo->dataRateInfo.uplinkRate;
    }

    if (deltaSecs) {
        time_t curTime = stadbEntryGetTimestamp();
        *deltaSecs = curTime - entry->inNetworkInfo->dataRateInfo.lastUpdateSecs;
    }

    return LBD_OK;
}

LBD_STATUS stadbEntry_setLastDataRate(stadbEntry_handle_t entry,
                                      lbd_linkCapacity_t dlRate,
                                      lbd_linkCapacity_t ulRate) {
    if (!stadbEntry_isInNetwork(entry)) {
        return LBD_NOK;
    }

    entry->validDataRate = LBD_TRUE;
    entry->inNetworkInfo->dataRateInfo.downlinkRate = dlRate;
    entry->inNetworkInfo->dataRateInfo.uplinkRate = ulRate;
    entry->inNetworkInfo->dataRateInfo.lastUpdateSecs = stadbEntryGetTimestamp();

    stadbEntryUpdateTimestamp(entry);
    return LBD_OK;
}

LBD_STATUS stadbEntry_incSteerAttemptCount(stadbEntry_handle_t entry) {
    if (!stadbEntry_isInNetwork(entry)) {
        return LBD_NOK;
    }

    // Unlikely to hit. Perhaps we can skip the check, roll over, and allow steer.
    if (entry->inNetworkInfo->numSteerAttempts == UINT8_MAX) {
        return LBD_OK;
    }

    entry->inNetworkInfo->numSteerAttempts++;
    return LBD_OK;
}

LBD_STATUS stadbEntry_getSteerAttemptCount(stadbEntry_handle_t entry, u_int8_t *cnt) {
    if (!stadbEntry_isInNetwork(entry) || !cnt) {
        return LBD_NOK;
    }

    *cnt = entry->inNetworkInfo->numSteerAttempts;
    return LBD_OK;
}

LBD_STATUS stadbEntry_resetSteerAttemptCount(stadbEntry_handle_t entry) {
    if (!stadbEntry_isInNetwork(entry)) {
        return LBD_NOK;
    }

    entry->inNetworkInfo->numSteerAttempts = 0;
    return LBD_OK;
}

LBD_BOOL stadbEntry_isChannelSupported(const stadbEntry_handle_t entry,
                                       lbd_channelId_t channel) {
    if (!stadbEntry_isInNetwork(entry)) {
        return LBD_FALSE;
    }

    size_t i = 0;
    for (i = 0; i < entry->inNetworkInfo->numBSSStats; ++i) {
        if (entry->inNetworkInfo->bssStats[i].valid &&
            entry->inNetworkInfo->bssStats[i].bss.channelId == channel) {
            return LBD_TRUE;
        }
    }

    return LBD_FALSE;
}

stadbEntry_bssStatsHandle_t stadbEntry_getServingBSS(
        const stadbEntry_handle_t entry, time_t *deltaSecs) {
    if (!stadbEntry_isInNetwork(entry) ||
        !entry->inNetworkInfo->assoc.bssHandle) {
        return NULL;
    }

    if (deltaSecs) {
        time_t curTime = stadbEntryGetTimestamp();
        *deltaSecs = curTime - entry->inNetworkInfo->assoc.lastAssoc.tv_sec;
    }
    return entry->inNetworkInfo->assoc.bssHandle;
}

const lbd_bssInfo_t *stadbEntry_resolveBSSInfo(const stadbEntry_bssStatsHandle_t bssHandle) {
    if (!bssHandle) {
        return NULL;
    }

    return &bssHandle->bss;
}

stadbEntry_bssStatsHandle_t stadbEntry_findMatchBSSStats(stadbEntry_handle_t entry,
                                                         const lbd_bssInfo_t *bss) {
    return stadbEntryFindBSSStats(entry, bss, LBD_TRUE /* matchOnly */);
}

stadbEntry_bssStatsHandle_t stadbEntry_findMatchRemoteBSSStats(stadbEntry_handle_t entry,
                                                               const lbd_bssInfo_t *bss) {
    if (!bss || bss->apId == LBD_APID_SELF) {
        return NULL;
    }

    return stadbEntryFindBSSStats(entry, bss, LBD_FALSE /* matchOnly */);
}

stadbEntry_bssStatsHandle_t stadbEntryFindBSSStats(stadbEntry_handle_t entry,
                                                   const lbd_bssInfo_t *bss,
                                                   LBD_BOOL matchOnly) {
    if (!bss || !stadbEntry_isInNetwork(entry)) {
        return NULL;
    }

    size_t i;
    for (i = 0; i < entry->inNetworkInfo->numBSSStats; ++i) {
        if (entry->inNetworkInfo->bssStats[i].valid) {
            if (lbAreBSSesSame(bss, &entry->inNetworkInfo->bssStats[i].bss)) {
                // When there is a match, return
                return &entry->inNetworkInfo->bssStats[i];
            } else if (!matchOnly &&
                       lbAreBSSesOnSameRadio(bss, &entry->inNetworkInfo->bssStats[i].bss)) {
                // This will happen when ESS changes. Update BSS info and keep other stats
                lbCopyBSSInfo(bss, &entry->inNetworkInfo->bssStats[i].bss);
                return &entry->inNetworkInfo->bssStats[i];
            }
        }
    }

    if (matchOnly) { return NULL; }

    // Find a slot for the new BSS.
    stadbEntry_bssStatsHandle_t newBSSStats =
        stadbEntryFindSlotForBSSStats(entry, bss);
    if (newBSSStats) {
        stadbEntryResetBSSStatsEntry(newBSSStats, bss);
        newBSSStats->valid = LBD_TRUE;
    }

    return newBSSStats;
}

LBD_STATUS stadbEntrySetPHYCapInfo(stadbEntry_handle_t entry,
                                   stadbEntry_bssStatsHandle_t bssHandle,
                                   const wlanif_phyCapInfo_t *phyCapInfo) {
    if (!entry || !bssHandle || !phyCapInfo || !phyCapInfo->valid) {
        return LBD_NOK;
    }

    stadbEntryBSSStatsUpdateTimestamp(bssHandle);

    wlanif_band_e band = stadbEntry_resolveBandFromBSSStats(bssHandle);
    lbDbgAssertExit(NULL, band != wlanif_band_invalid);

    if (stadbEntryUpdateBandPHYCapInfo(entry, band, phyCapInfo)) {
        stadbEntryFindBestPHYMode(entry);
    }

    return LBD_OK;
}

/**
 * @brief Compare a new PHY capability to an existing one and update the
 *        existing one if the new one is better.
 *
 * @pre newPHYCap is valid
 *
 * @param [inout] oldPHYCap  the current set of PHY capabilities
 * @param [in] newPHYCap  the new PHY capabilities to use to perform any
 *                        necessary updates
 *
 * @return LBD_TRUE if the capabilties were updated based on the new one
 */
static LBD_BOOL stadbEntryUpdateBetterPHYCapInfo(wlanif_phyCapInfo_t *oldPHYCap,
                                                 const wlanif_phyCapInfo_t *newPHYCap) {
    LBD_BOOL changed = LBD_FALSE;
    if (!oldPHYCap->valid) {
        *oldPHYCap = *newPHYCap;
        changed = LBD_TRUE;
    } else if (newPHYCap->phyMode != wlanif_phymode_invalid) {
        if (oldPHYCap->phyMode == wlanif_phymode_invalid ||
            newPHYCap->phyMode > oldPHYCap->phyMode) {
            if (oldPHYCap->phyMode == wlanif_phymode_basic) {
                // Need to reset this since anything beyond basic uses this
                // field with MCS indices rather than rates.
                oldPHYCap->maxMCS = 0;
            }

            oldPHYCap->phyMode = newPHYCap->phyMode;
            changed = LBD_TRUE;
        }

        if (newPHYCap->maxChWidth > oldPHYCap->maxChWidth) {
            oldPHYCap->maxChWidth = newPHYCap->maxChWidth;
            changed = LBD_TRUE;
        }

        if (newPHYCap->numStreams > oldPHYCap->numStreams) {
            oldPHYCap->numStreams = newPHYCap->numStreams;
            changed = LBD_TRUE;
        }

        // Note the PHY mode check here as the MCS values have different
        // meanings for basic versus non-basic.
        if (newPHYCap->phyMode >= oldPHYCap->phyMode &&
            newPHYCap->maxMCS > oldPHYCap->maxMCS) {
            oldPHYCap->maxMCS = newPHYCap->maxMCS;
            changed = LBD_TRUE;
        }

        if (newPHYCap->maxTxPower > oldPHYCap->maxTxPower) {
            oldPHYCap->maxTxPower = newPHYCap->maxTxPower;
            changed = LBD_TRUE;
        }
    }

    return changed;
}

/**
 * @brief Update PHY capability info on a given band with the new PHY info
 *
 * Only record the best PHY capability on a given band.
 *
 * @pre new PHY capability information is valid
 *
 * @param [in] entry  the handle to the STA
 * @param [in] band  the band on which PHY capability may update
 * @param [in] newPHYCap  new PHY capability information
 *
 * @return LBD_TRUE if the PHY capability on the band is updated; otherwise return
 *         LBD_FALSE if the new capability is not better than the original one
 */
static LBD_BOOL stadbEntryUpdateBandPHYCapInfo(stadbEntry_handle_t entry,
                                               wlanif_band_e band,
                                               const wlanif_phyCapInfo_t *newPHYCap) {
    LBD_BOOL updated = LBD_FALSE;
    wlanif_phyCapInfo_t *oldPHYCap = &entry->inNetworkInfo->phyCapInfo[band];
    // Currently we only record the best PHY capabilities on the band,
    // and do not handle any downgrade
    if (stadbEntryUpdateBetterPHYCapInfo(oldPHYCap, newPHYCap)) {
        updated = LBD_TRUE;
    }

    return updated;
}

/**
 * @brief Find the best PHY mode supported by a STA across all bands
 *
 * @param [in] entry  the handle to the STA entry
 */
static void stadbEntryFindBestPHYMode(stadbEntry_handle_t entry) {
    entry->bestPHYMode = wlanif_phymode_basic;

    wlanif_band_e band;
    for (band = wlanif_band_24g; band < wlanif_band_invalid; ++band) {
        const wlanif_phyCapInfo_t *phyCapInfo = &entry->inNetworkInfo->phyCapInfo[band];
        if (phyCapInfo->valid && phyCapInfo->phyMode != wlanif_phymode_invalid &&
            phyCapInfo->phyMode > entry->bestPHYMode) {
                entry->bestPHYMode = phyCapInfo->phyMode;
        }
    }
}

/**
 * @brief Update the timestamp of a BSS stats entry
 *
 * The caller should confirm the entry is valid
 *
 * @param [in] bssHandle  the handle to the BSS entry to be updated
 */
static void stadbEntryBSSStatsUpdateTimestamp(stadbEntry_bssStatsHandle_t bssHandle) {
    lbDbgAssertExit(NULL, bssHandle && bssHandle->valid);
    bssHandle->lastUpdateSecs = stadbEntryGetTimestamp();
}

LBD_STATUS stadbEntryAddReservedAirtime(stadbEntry_handle_t entry,
                                        const lbd_bssInfo_t *bss,
                                        lbd_airtime_t airtime) {
    if (!entry || !bss || airtime == LBD_INVALID_AIRTIME) {
        return LBD_NOK;
    }

    if (entry->entryType == stadbEntryType_outOfNetwork) {
        // This should not happen on target, add a check to be conservative
        return LBD_NOK;
    }

    stadbEntry_bssStatsHandle_t bssHandle =
        stadbEntryFindBSSStats(entry, bss, LBD_FALSE /* matchOnly */);
    lbDbgAssertExit(NULL, bssHandle);

    bssHandle->reservedAirtime = airtime;
    stadbEntryBSSStatsUpdateTimestamp(bssHandle);

    entry->hasReservedAirtime = LBD_TRUE;
    // Mark band as supported
    stadbEntryMarkBandSupported(entry, bss);

    return LBD_OK;
}

LBD_BOOL stadbEntry_hasReservedAirtime(stadbEntry_handle_t handle) {
    if (!handle) { return LBD_FALSE; }

    return handle->hasReservedAirtime;
}

lbd_airtime_t stadbEntry_getReservedAirtime(stadbEntry_handle_t handle,
                                            stadbEntry_bssStatsHandle_t bssHandle) {
    if (!handle || !bssHandle) {
        return LBD_INVALID_AIRTIME;
    }
    lbDbgAssertExit(NULL, bssHandle->valid);

    return bssHandle->reservedAirtime;
}


wlanif_phymode_e stadbEntry_getBestPHYMode(stadbEntry_handle_t entry) {
    if (!entry) { return wlanif_phymode_invalid; }

    // If there is no PHY cap info for this client yet, it will return
    // wlanif_phymode_basic since this field is 0 initialized at entry
    // creation time.
    return entry->bestPHYMode;
}

void stadbEntryHandleChannelChange(stadbEntry_handle_t entry,
                                   lbd_vapHandle_t vap,
                                   lbd_channelId_t channelId) {
    if (!stadbEntry_isInNetwork(entry)) { return; }

    size_t i = 0;
    for (i = 0; i < entry->inNetworkInfo->numBSSStats; ++i) {
        stadbEntryPriv_bssStats_t *bssStats = &entry->inNetworkInfo->bssStats[i];
        if (bssStats->valid && bssStats->bss.vap == vap) {
            bssStats->bss.channelId = channelId;
            if (stadbEntry_clearPolluted(entry, bssStats) != LBD_OK) {
                dbgf(NULL, DBGERR,
                        "%s: Failed to clear polluted state ",__func__);
            }
            // The new channel may have a different TX power, so nuke RSSI
            // here to allow a new one filled in.
            memset(&bssStats->uplinkInfo, 0, sizeof(bssStats->uplinkInfo));
            bssStats->uplinkInfo.rssi = LBD_INVALID_RSSI;

            // Assume for now that the STA followed the AP in the channel
            // change
            if (bssStats == entry->inNetworkInfo->assoc.bssHandle) {
                entry->inNetworkInfo->assoc.channel = channelId;
            }
            break;
        }
    }
}

void stadbEntryAssocDiagLog(stadbEntry_handle_t entry,
                            const lbd_bssInfo_t *bss) {
    if (diaglog_startEntry(mdModuleID_StaDB,
                           stadb_msgId_associationUpdate,
                           diaglog_level_demo)) {
        diaglog_writeMAC(&entry->addr);
        diaglog_writeBSSInfo(bss);
        diaglog_write8(entry->inNetworkInfo->assoc.bssHandle != NULL);
        diaglog_write8(entry->isAct);
        diaglog_write8(stadbEntry_isDualBand(entry));
        diaglog_write8(entry->isBTMSupported);
        diaglog_write8(entry->isRRMSupported);
        diaglog_write8(entry->clientClassGroup);
        diaglog_finishEntry();
    }
}

void stadbEntryPopulateBSSesFromSameESS(stadbEntry_handle_t entry,
                                        const lbd_bssInfo_t *servingBSS,
                                        wlanif_band_e band) {
    if (!stadbEntry_isInNetwork(entry)) {
        return;
    }

    lbd_essId_t lastServingESS = stadbEntry_getLastServingESS(entry);
    if (lastServingESS == LBD_ESSID_INVALID) {
        // To handle a case when associating for the first time
        // and don't have a valid last serving ESS
        if (servingBSS && servingBSS->essId == LBD_ESSID_INVALID) {
            return;
        }
        // The client must have never associated with us before, pick ESS ID 0.
        // We will re-populate this on real association
        lbDbgAssertExit(NULL, !servingBSS);
        lastServingESS = 0;
    }

    size_t maxNumBSSes = entry->inNetworkInfo->numBSSStats - 1; // exclude serving BSS
    lbd_bssInfo_t bss[maxNumBSSes];

    if (LBD_NOK == wlanif_getBSSesSameESS(servingBSS, lastServingESS,
                                          band, &maxNumBSSes, bss) ||
        !maxNumBSSes) {
        // No other BSSes on the serving ESS
        return;
    }

    size_t i;
    for (i = 0; i < maxNumBSSes; ++i) {
        // Create a BSS entry for all same ESS BSSes if they do not exist
        stadbEntry_bssStatsHandle_t bssHandle =
            stadbEntryFindBSSStats(entry, &bss[i], LBD_FALSE /* matchOnly */);
        if (!bssHandle) {
            dbgf(NULL, DBGERR,
                 "%s: Failed to create BSS stats entry for " lbBSSInfoAddFmt(),
                 __func__, lbBSSInfoAddData(&bss[i]));
        }
    }
}

lbd_essId_t stadbEntry_getLastServingESS(stadbEntry_handle_t entry) {
    if (!stadbEntry_isInNetwork(entry)) {
        return LBD_ESSID_INVALID;
    }

    return entry->inNetworkInfo->assoc.lastServingESS;
}

wlanif_band_e stadbEntry_resolveBandFromBSSStats(
        const stadbEntry_bssStatsHandle_t bssHandle) {
    if (!bssHandle) {
        return wlanif_band_invalid;
    }

    return wlanif_resolveBandFromChannelNumber(bssHandle->bss.channelId);
}

/**
 * @brief Decide the entry size and type for a given STA entry
 *
 * @param [in] inNetwork  whether the STA is in-network or not
 * @param [in] trackRemoteAssoc  whether the database is tracking remote
 *                               associations
 * @param [in] rrmStatus  whether 802.11 Radio Resource Management is supported,
 *                        disabled, or unchanged from the current state.
 * @param [in] numRadiosServing  number of BSSes to allocate for the serving AP
 * @param [in] numNonServingBSSStats  number of BSSes reserved for non-serving AP
 * @param [out] numBSSStats  number of BSSes entries should be allocated
 * @param [out] type  the stadb entry type determined based on given information
 *
 * @return the memory size should be allocated for ths STA entry
 */
static size_t stadbEntryDetermineEntrySizeAndType(
        LBD_BOOL inNetwork, LBD_BOOL trackRemoteAssoc,
        wlanif_capStateUpdate_e rrmStatus, size_t numRadiosServing,
        size_t numNonServingBSSStats, size_t *numBSSStats,
        stadbEntryType_e *type) {
    size_t entrySize = sizeof(stadbEntryPriv_t);
    *numBSSStats = 0;
    *type = stadbEntryType_outOfNetwork;
    if (inNetwork) {
        entrySize += (sizeof(stadbEntryPriv_inNetworkInfo_t) +
                      sizeof(stadbEntryPriv_bssStats_t) * numRadiosServing);
        *numBSSStats += numRadiosServing;
        *type = stadbEntryType_inNetworkLocal;

        // When tracking remote associations, we need to allow for more BSS
        // entries to be stored. This enables the complete view for steering
        // on the serving AP as well as some metrics for the non-serving.
        if ((rrmStatus == wlanif_cap_enabled || trackRemoteAssoc) &&
                numNonServingBSSStats) {
           entrySize += numNonServingBSSStats * sizeof(stadbEntryPriv_bssStats_t);
           *numBSSStats += numNonServingBSSStats;
           *type = stadbEntryType_inNetworkLocalRemote;
        }
    }
    return entrySize;
}

/**
 * @brief Reset a BSS stats entry with new BSS info
 *
 * All other fields should be set to invalid values.
 *
 * @param [in] bssStats  the entry to be reset
 * @param [in] newBSS  the new BSS info to be assigned to the entry
 */
static void stadbEntryResetBSSStatsEntry(stadbEntry_bssStatsHandle_t bssStats,
                                         const lbd_bssInfo_t *newBSS) {
    memset(bssStats, 0, sizeof(*bssStats));
    bssStats->reservedAirtime = LBD_INVALID_AIRTIME;
    lbCopyBSSInfo(newBSS, &bssStats->bss);
}

/**
 * @brief Compare two BSS entries to determine if BSS1 is older than BSS2
 *
 * BSS1 is older than BSS2 if the last update time of BSS1 is before that of BSS2
 *
 * @param [in] bssStat1  the given BSS stat entry to compare
 * @param [in] bssStat2  the given BSS stat entry to compare with
 *
 * @return LBD_TRUE if BSS1 is older, otherwise return LBD_FALSE
 */
static LBD_BOOL stadbEntryIsBSSOlder(stadbEntry_bssStatsHandle_t bssStat1,
                                     stadbEntry_bssStatsHandle_t bssStat2) {
    lbDbgAssertExit(NULL, bssStat1);
    if (!bssStat2) { return LBD_TRUE; }

    return bssStat1->lastUpdateSecs < bssStat2->lastUpdateSecs;
}

/**
 * @brief Determine if the BSS provides corresponds to the same AP as the
 *        one that is serving.
 *
 * If no AP is serving, then check whether the BSS is for the local AP
 * (since we prefer to track stats on the local one in the absence of any
 * other info).
 *
 * @param [in] entry  the STA to check
 * @param [in] bss  the entry to check against the serving AP
 *
 * @return LBD_TRUE if the serving AP matches or there is no AP serving and
 *         the BSS corresponds to the local AP; otherwise LBD_FALSE
 */
static LBD_BOOL stadbEntryIsSameServingAP(stadbEntry_handle_t entry,
                                          const lbd_bssInfo_t *bss) {
    return entry->inNetworkInfo->assoc.apId == bss->apId;
}

/**
 * @brief Find a slot to hold BSS stats for the given BSS.
 *
 * The following rules are used to pick a slot:
 * 1. Try to find an empty slot;
 * 2. If no empty slot, look for a Least Recently Used (LRU) BSS stats slot.
 *    When looking for LRU BSS slot, need to consider whether to look for slot for
 *    serving BSS or non-serving BSS.
 *    # For local LRU, try to find a LRU same band entry first, if none, pick
 *      the LRU one among all local slots
 *    # For remote LRU, always pick LRU one among all remote slots
 *
 * @param [in] entry  the STA entry to look for BSS stats slot
 * @param [in] bss  the new BSS to be added
 *
 * @return the handle to the BSS stats slot found
 */
static stadbEntry_bssStatsHandle_t stadbEntryFindSlotForBSSStats(
        stadbEntry_handle_t entry, const lbd_bssInfo_t *bss) {
    stadbEntry_bssStatsHandle_t oldestServingEntry = NULL;
    stadbEntry_bssStatsHandle_t oldestNonServingEntry = NULL;
    stadbEntry_bssStatsHandle_t oldestServingSameBandEntry = NULL;
    size_t numServing = 0;

    wlanif_band_e band = wlanif_resolveBandFromChannelNumber(bss->channelId);
    LBD_BOOL isServingAP = stadbEntryIsSameServingAP(entry, bss);

    size_t i;
    for (i = 0; i < entry->inNetworkInfo->numBSSStats; ++i) {
        stadbEntry_bssStatsHandle_t bssEntry =
            &entry->inNetworkInfo->bssStats[i];
        if (!bssEntry->valid) {
            // Found an empty slot, use it
            return &entry->inNetworkInfo->bssStats[i];
        }

        if (stadbEntryIsSameServingAP(entry, &bssEntry->bss)) {
            numServing++;
            if (wlanif_resolveBandFromChannelNumber(
                    bssEntry->bss.channelId) == band &&
                    stadbEntryIsBSSOlder(bssEntry, oldestServingSameBandEntry)) {
                oldestServingSameBandEntry = bssEntry;
            }

            if (stadbEntryIsBSSOlder(bssEntry, oldestServingEntry)) {
                oldestServingEntry = bssEntry;
            }
        } else if (stadbEntryIsBSSOlder(bssEntry, oldestNonServingEntry)) {
            oldestNonServingEntry = bssEntry;
        }
    }

    if (isServingAP) {
        if (numServing < WLANIF_MAX_RADIOS) {
            // If we have not reached our limit on the number of serving BSSes
            // being tracked or the BSS being stored is not a serving one, just
            // use the oldest entry available.
            lbDbgAssertExit(NULL, oldestNonServingEntry);
            return oldestNonServingEntry;
        } else if (oldestServingSameBandEntry) {
            // Reached limit but have another same band BSS, so use it.
            lbDbgAssertExit(NULL, oldestServingSameBandEntry);
            return oldestServingSameBandEntry;
        } else {
            // Reached limit but no same band BSS. Just use the oldest serving
            // entry.
            lbDbgAssertExit(NULL, oldestServingEntry);
            return oldestServingEntry;
        }
    } else {
        return oldestNonServingEntry;
    }
}

LBD_STATUS stadbEntry_setPolluted(stadbEntry_handle_t entry,
                                  stadbEntry_bssStatsHandle_t bssStats,
                                  time_t expirySecs) {
    if (!stadbEntry_isInNetwork(entry) || !bssStats) {
        return LBD_NOK;
    }

    if (!bssStats->polluted) {
        bssStats->polluted = LBD_TRUE;
        entry->inNetworkInfo->numPollutedBSS++;
    }

    bssStats->pollutionExpirySecs = stadbEntryGetTimestamp() + expirySecs;

    return LBD_OK;
}

LBD_STATUS stadbEntry_clearPolluted(stadbEntry_handle_t entry,
                                    stadbEntry_bssStatsHandle_t bssStats) {
    if (!stadbEntry_isInNetwork(entry) || !bssStats) {
        return LBD_NOK;
    }

    if (bssStats->polluted) {
        lbDbgAssertExit(NULL, entry->inNetworkInfo->numPollutedBSS);
        bssStats->polluted = LBD_FALSE;
        entry->inNetworkInfo->numPollutedBSS--;
    }

    bssStats->pollutionExpirySecs = 0;

    return LBD_OK;
}

LBD_STATUS stadbEntry_getPolluted(stadbEntry_handle_t entry,
                                  stadbEntry_bssStatsHandle_t bssStats,
                                  LBD_BOOL *polluted, time_t *expirySecs) {
    if (!stadbEntry_isInNetwork(entry) || !bssStats || !polluted) {
        return LBD_NOK;
    }

    *polluted = bssStats->polluted;

    if (expirySecs) {
        time_t curTime = stadbEntryGetTimestamp();
        *expirySecs = *polluted && bssStats->pollutionExpirySecs > curTime ?
                          bssStats->pollutionExpirySecs - curTime : 0;
    }

    return LBD_OK;
}

LBD_STATUS stadbEntry_isChannelPollutionFree(stadbEntry_handle_t entry,
                                             lbd_channelId_t channel,
                                             LBD_BOOL *pollutionFree) {
    if (!stadbEntry_isInNetwork(entry) || !pollutionFree) {
        return LBD_NOK;
    }

    // If no polluted BSS, return directly
    if (!entry->inNetworkInfo->numPollutedBSS) {
        *pollutionFree = LBD_TRUE;
        return LBD_OK;
    }

    LBD_BOOL channelMatch = LBD_FALSE;
    size_t i = 0;
    for (i = 0; i < entry->inNetworkInfo->numBSSStats; ++i) {
        if (entry->inNetworkInfo->bssStats[i].valid &&
            entry->inNetworkInfo->bssStats[i].bss.channelId == channel) {
            channelMatch = LBD_TRUE;
            if (entry->inNetworkInfo->bssStats[i].polluted) {
                *pollutionFree = LBD_FALSE;
                return LBD_OK;
            }
        }
    }

    if (channelMatch) {
        *pollutionFree = LBD_TRUE;
        return LBD_OK;
    }

    return LBD_NOK;
}

LBD_STATUS stadbEntryUpdateSMPSMode(stadbEntry_handle_t entry,
                                    const lbd_bssInfo_t *bss,
                                    LBD_BOOL isStatic) {
    if (!entry || !bss) { return LBD_NOK; }

    entry->isStaticSMPS = isStatic;
    stadbEntryMarkBandSupported(entry, bss);
    return LBD_OK;
}

LBD_STATUS stadbEntryUpdateOpMode(stadbEntry_handle_t entry,
                                  const lbd_bssInfo_t *bss,
                                  wlanif_chwidth_e maxChWidth,
                                  u_int8_t numStreams) {
    if (!stadbEntry_isInNetwork(entry) || !bss ||
        maxChWidth >= wlanif_chwidth_invalid || !numStreams) {
        return LBD_NOK;
    }

    // Operating Mode notification should only happen on the serving BSS
    stadbEntry_bssStatsHandle_t servingBSS = stadbEntry_getServingBSS(entry, NULL);
    const lbd_bssInfo_t *servingBSSInfo = stadbEntry_resolveBSSInfo(servingBSS);

    if (!lbAreBSSesSame(bss, servingBSSInfo)) {
        return LBD_NOK;
    }

    wlanif_band_e band = wlanif_resolveBandFromChannelNumber(servingBSSInfo->channelId);
    lbDbgAssertExit(NULL, band < wlanif_band_invalid);

    entry->inNetworkInfo->phyCapInfo[band].maxChWidth = maxChWidth;
    entry->inNetworkInfo->phyCapInfo[band].numStreams = numStreams;

    stadbEntryUpdateTimestamp(entry);

    return LBD_OK;
}

LBD_STATUS stadbEntryMarkDualBandSupported(stadbEntry_handle_t entry) {
    if (!entry) { return LBD_NOK; }

    stadbEntrySetSupportedBand(entry, wlanif_band_24g);
    stadbEntrySetSupportedBand(entry, wlanif_band_5g);
    stadbEntryUpdateTimestamp(entry);

    return LBD_OK;
}

LBD_STATUS stadbEntryMarkGivenBandSupported(stadbEntry_handle_t entry, u_int8_t bandCap) {
    if (!entry) { return LBD_NOK; }

    if (bandCap & (1 << wlanif_band_24g)) {
        stadbEntrySetSupportedBand(entry, wlanif_band_24g);
    }
    if (bandCap & (1 << wlanif_band_5g)) {
        stadbEntrySetSupportedBand(entry, wlanif_band_5g);
    }
    stadbEntryUpdateTimestamp(entry);
    return LBD_OK;
}

LBD_STATUS stadbEntryMarkActiveLoadBalancingRejected(stadbEntry_handle_t entry, u_int8_t loadbalancingStaState) {
    if(!entry) { return LBD_NOK;}

    stadbEntrySetLoadBalancingRejected(entry, loadbalancingStaState);
    stadbEntryUpdateTimestamp(entry);

    return LBD_OK;
}

LBD_STATUS stadbEntryUnmarkActiveLoadBalancingRejected(stadbEntry_handle_t entry) {
    if(!entry) { return LBD_NOK;}

    stadbEntrySetLoadBalancingRejected(entry, stadbEntryLoadBalancingStaState_initial);
    stadbEntryUpdateTimestamp(entry);

    return LBD_OK;
}

u_int8_t stadbEntryReadActiveLoadBalancingRejected(stadbEntry_handle_t entry) {
    if(!entry) { return LBD_NOK;}

    return stadbEntryGetLoadBalancingRejected(entry);
}

LBD_STATUS stadbEntryPopulateNonServingPHYInfo(stadbEntry_handle_t entry,
                                               const lbd_bssInfo_t *servingBSS,
                                               const wlanif_phyCapInfo_t *servingPHY) {
    wlanif_band_e servingBand, nonServingBand;
    wlanif_phyCapInfo_t phyInfo = { LBD_FALSE };

    if (!stadbEntry_isInNetwork(entry) ||
        !servingBSS || !servingPHY || !servingPHY->valid) {
        return LBD_NOK;
    }

    servingBand = wlanif_resolveBandFromChannelNumber(servingBSS->channelId);
    if (servingBand == wlanif_band_invalid) {
        return LBD_NOK;
    }

    nonServingBand = (servingBand == wlanif_band_24g) ? wlanif_band_5g :
                                                        wlanif_band_24g;

    if (LBD_OK == stadbEntry_getPHYCapInfoByBand(entry, nonServingBand,
                                                 &phyInfo) &&
        phyInfo.valid) {
        // Already have valid PHY info on non-serving band, do nothing
        return LBD_OK;
    }

    if (nonServingBand == wlanif_band_5g) {
        // Assume what STA is capable of on 2.4 GHz will be capable on 5 GHz,
        // but do not assume anything about the power.
        phyInfo = *servingPHY;
        phyInfo.maxTxPower = 0;  // unknown for now
        return stadbEntrySetPHYCapInfoByBand(entry, nonServingBand, &phyInfo);
    } else { // non-serving band is 2.4 GHz
        // Currently assume max capability on 2.4 GHz is 11n/40MHz/MCS7,
        // and number of streams will be the same as ones on 5 GHz.
        // Make no assumptions about the TX power.
        const wlanif_phyCapInfo_t BEST_24G_PHY = {
            LBD_TRUE /* valid */, wlanif_chwidth_40, WLANIF_MAX_NUM_STREAMS,
            wlanif_phymode_ht, WLANIF_MAX_MCS_HT, 0 // maxTxPower
        };
        wlanif_resolveMinPhyCap(servingPHY, &BEST_24G_PHY, &phyInfo);
        if (phyInfo.phyMode == wlanif_phymode_basic) {
            // Correct MCS if running in 11a mode
            phyInfo.maxMCS = servingPHY->maxMCS;
        }

        return stadbEntrySetPHYCapInfoByBand(entry, nonServingBand, &phyInfo);
    }
}

static json_t *stadbEntryPhyCapInfoJsonize(const stadbEntry_handle_t entry) {
    json_t *phyCapInfo_j, *pci_j;
    wlanif_phyCapInfo_t *pci;
    int i;

    phyCapInfo_j = json_array();
    for (i = 0; i < wlanif_band_invalid; i++) {
        pci = &(entry->inNetworkInfo->phyCapInfo[i]);
        pci_j = json_pack("{s:b, s:i, s:i, s:i, s:i, s:i}",
                "valid", pci->valid,
                "maxChWidth", pci->maxChWidth,
                "numStreams", pci->numStreams,
                "phyMode", pci->phyMode,
                "maxMCS", pci->maxMCS,
                "maxTxPower", pci->maxTxPower);

        if (pci_j == NULL) {
            dbgf(NULL, DBGERR, "%s: Failed to jsonize a phyCapInfo", __func__);
            json_decref(phyCapInfo_j);
            phyCapInfo_j = NULL;
            break;
        }

        if (json_array_append_new(phyCapInfo_j, pci_j) != 0) {
            dbgf(NULL, DBGERR, "%s: Failed to append a phyCapInfo", __func__);
            json_decref(pci_j);
            json_decref(phyCapInfo_j);
            phyCapInfo_j = NULL;
            break;
        }
    }

    return phyCapInfo_j;
}

json_t *stadbEntryJsonize(const stadbEntry_handle_t entry,
                          stadbEntry_jsonizeSteerExecCB_t jseCB) {
    json_t *ret, *phyCapInfo_j, *steerExec_j;
    char *ether_a;

    /* stringify addr */
    ether_a = ether_ntoa(&entry->addr);
    if (ether_a == NULL) {
        dbgf(NULL, DBGERR, "%s: Failed to convert ether addr to string.",
             __func__);
        return NULL;
    }

    phyCapInfo_j = stadbEntryPhyCapInfoJsonize(entry);
    if (phyCapInfo_j == NULL) {
        dbgf(NULL, DBGERR, "%s: Failed to jsonize phyCapInfo.", __func__);
        return NULL;
    }

    ret = json_pack("{s:s, s:i, s:i, s:b, s:b, s:b, s:b, s:{s:o}}, s:i",
            "addr", ether_a,
            "entryType", entry->entryType,
            "operatingBands", entry->operatingBands,
            "isBTMSupported", entry->isBTMSupported,
            "isRRMSupported", entry->isRRMSupported,
            "isMUMIMOSupported", entry->isMUMIMOSupported,
            "isSteeringDisallowed", entry->isSteeringDisallowed,
            "inNetworkInfo",
                "phyCapInfo", phyCapInfo_j,
            "clientClassGroup", entry->clientClassGroup
    );

    if (ret == NULL) {
        dbgf(NULL, DBGERR, "%s: Failed to jsonize stadb entry", __func__);
        return NULL;
    }

    /* steerExec (optional) */
    steerExec_j = jseCB(entry);
    if (steerExec_j != NULL) {
        if (json_object_set_new(ret, "steerExec", steerExec_j) != 0) {
            dbgf(NULL, DBGERR, "%s: Failed to set steerExec", __func__);
            json_decref(ret);
            json_decref(steerExec_j);
            ret = NULL;
        }
    }

    return ret;
}

void stadbEntrySetDirtyIfInNetwork(stadbEntry_handle_t entry) {
    if (stadbEntry_isInNetwork(entry)) {
        stadb_setDirty();
    }
}
