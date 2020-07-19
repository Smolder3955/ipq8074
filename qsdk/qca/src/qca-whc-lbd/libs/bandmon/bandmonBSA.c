// vim: set et sw=4 sts=4 cindent:
/*
 * @File: bandmonBSA.c
 *
 * @Abstract: Implementation of single AP band monitor
 *
 * @Notes:
 *
 * @@-COPYRIGHT-START-@@
 *
 * Copyright (c) 2015, 2018 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * 2015 Qualcomm Atheros, Inc.
 *
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * @@-COPYRIGHT-END-@@
 *
 */

#include "module.h"

#include "bandmonCmn.h"

// BSA has no special field compared to common state
struct bandmonPriv_t bandmonBSAState;
struct bandmonPriv_t *bandmonCmnStateHandle = &bandmonBSAState;

// ====================================================================
// Private functions
// ====================================================================

/**
 * @brief React to an event providing updated channel utilization information
 *        for a single band.
 */
static void bandmonBSAHandleChanUtilEvent(struct mdEventNode *event) {
    const wlanif_chanUtilEvent_t *utilEvent =
        (const wlanif_chanUtilEvent_t *) event->Data;

#ifdef LBD_DBG_MENU
    if (!bandmonCmnStateHandle->debugModeEnabled)
#endif
    {
        bandmonCmnHandleChanUtil(utilEvent->bss.channelId,
                                 utilEvent->utilization);
    }
}

// ====================================================================
// Package level functions
// ====================================================================

inline void bandmonFinalizeChannelExtInfo(
        struct bandmonChannelUtilizationInfo_t *chanInfo) {
    // No extra info for BSA
    return;
}

void bandmonHandleActiveSteered(void) {
    if (bandmonCmnStateHandle->blackoutState == bandmon_blackoutState_loadBalancingAPAssigned) {
        bandmonCmnStateHandle->blackoutState = bandmon_blackoutState_pending;
    } else if (bandmonCmnStateHandle->blackoutState == bandmon_blackoutState_active) {
        bandmonCmnStateHandle->blackoutState = bandmon_blackoutState_activeWithPending;
    }
}

inline LBD_STATUS bandmonInitializeChannelExtInfo(
        struct bandmonChannelUtilizationInfo_t *chanInfo) {
    // No extra info for BSA
    return LBD_OK;
}

void bandmonSubInit(void) {
    // In BSA, all steerings are allowed by default
    bandmonCmnStateHandle->blackoutState = bandmon_blackoutState_loadBalancingAPAssigned;

    mdListenTableRegister(mdModuleID_WlanIF, wlanif_event_chan_util,
                          bandmonBSAHandleChanUtilEvent);
}

void bandmonHandleVAPRestart() {
    // Reset our state for steering blackout,
    // as the conditions that had dictated them are no longer relevant.
    bandmonCmnStateHandle->blackoutState = bandmon_blackoutState_loadBalancingAPAssigned;

    bandmonCmnResetUtilState();
}

LBD_BOOL bandmonIsLocalSteeringAllowed(const lbd_bssInfo_t *bss) {
    // In single AP mode, steering is always allowed locally.
    return LBD_TRUE;
}

LBD_BOOL bandmon_isInSteeringBlackout(lbd_apId_t apId) {
    return bandmonCmnStateHandle->blackoutState == bandmon_blackoutState_activeWithPending ||
           bandmonCmnStateHandle->blackoutState == bandmon_blackoutState_active;
}

lbd_apId_t bandmon_getCurrentOffloadingAP(void) {
    return LBD_APID_SELF;
}

LBD_BOOL bandmonCmnIsPreAssocSteeringPermitted(void) {
    // Pre-association steering is not permitted if all of the channels are
    // overloaded or none of the channels are overloaded.
    u_int8_t numOverloaded = bandmonCmnGetNumOverloadedChannels();

    return !(numOverloaded == 0 || numOverloaded == bandmonCmnStateHandle->numActiveChannels);
}
