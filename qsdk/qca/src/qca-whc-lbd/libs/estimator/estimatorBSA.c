// vim: set et sw=4 sts=4 cindent:
/*
 * @File: estimatorBSA.c
 *
 * @Abstract: Implementation of single AP estimator
 *
 * @Notes:
 *
 * @@-COPYRIGHT-START-@@
 *
 * Copyright (c) 2015-2016, 2018 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * 2015-2016 Qualcomm Atheros, Inc.
 *
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * @@-COPYRIGHT-END-@@
 *
 */

#include "lb_common.h"

#include "estimatorCmn.h"

// ====================================================================
// Private functions
// ====================================================================

// ====================================================================
// Package level functions
// ====================================================================

LBD_STATUS estimatorHandleValidBeaconReport(stadbEntry_handle_t entry,
                                            const wlanif_beaconReportEvent_t *bcnrptEvent,
                                            wlanif_band_e measuredBand,
                                            const lbd_bssInfo_t **reportedLocalBss) {
    estimatorNonServingRateAirtimeParams_t params;
    params.staAddr = &bcnrptEvent->sta_addr;
    params.measuredBand = measuredBand;
    params.result = LBD_OK;

    size_t i = 0;
    for (i = 0; i < bcnrptEvent->numBcnrpt; ++i) {
        // First find the reported local BSS
        if (bcnrptEvent->reportedBcnrptInfo[i].reportedBss.apId ==
                LBD_APID_SELF) {
            *reportedLocalBss = &bcnrptEvent->reportedBcnrptInfo[i].reportedBss;
            params.measuredBss = *reportedLocalBss;
            params.rcpi = bcnrptEvent->reportedBcnrptInfo[i].rcpi;
        }
        if (LBD_NOK == stadbEntry_setRCPIByBSSInfo(
                           entry,
                           &bcnrptEvent->reportedBcnrptInfo[i].reportedBss,
                           bcnrptEvent->reportedBcnrptInfo[i].rcpi)) {
            dbgf(estimatorState.dbgModule, DBGERR,
                 "%s: Failed to record downlink RSSI for " lbMACAddFmt(":")
                 " on " lbBSSInfoAddFmt(), __func__,
                 lbMACAddData(bcnrptEvent->sta_addr.ether_addr_octet),
                 lbBSSInfoAddData(&bcnrptEvent->reportedBcnrptInfo[i].reportedBss));
            // Still try to record on other BSSes if any
        }
    }

    if (!*reportedLocalBss) {
        dbgf(estimatorState.dbgModule, DBGERR,
             "%s: No local BSS reported in beacon report from " lbMACAddFmt(":"),
             __func__, lbMACAddData(bcnrptEvent->sta_addr.ether_addr_octet));
        return LBD_NOK;
    }

    estimatorCmnHandleLocalBeaconReport(entry, &bcnrptEvent->sta_addr,
                                        *reportedLocalBss, &params);

    return params.result;
}

LBD_STATUS estimator_handleSTATrafficStats(stadbEntry_handle_t entry, lbd_apId_t apId,
                                            const estimator_staTrafficStats_t *stats) {
    return LBD_NOK;
}

LBD_STATUS estimatorRequestDownlinkRSSI(stadbEntry_handle_t entry,
                                        stadbEntry_bssStatsHandle_t bssStats,
                                        const lbd_bssInfo_t *servingBSS,
                                        size_t numChannels,
                                        const lbd_channelId_t *channelList) {
    u_int8_t clientClassGroup = 0;
    const struct ether_addr *staAddr = stadbEntry_getAddr(entry);

    stadbEntry_getClientClassGroup(entry, &clientClassGroup);

    return wlanif_requestDownlinkRSSI(
        servingBSS, staAddr, stadbEntry_isRRMSupported(entry), numChannels,
        channelList, clientClassGroup);
}

LBD_BOOL estimatorIsRemoteEstimationAllowed(void) {
    // Does not allow remote estimation for single AP setup
    return LBD_FALSE;
}

void estimatorSubInit(void) {
   // No BSA specific initialization needs to be done.
}
