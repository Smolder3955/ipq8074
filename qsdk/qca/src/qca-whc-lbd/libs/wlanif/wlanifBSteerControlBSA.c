// vim: set et sw=4 sts=4 cindent:
/*
 * @File: wlanifBSteerControlBSA.c
 *
 * @Abstract: Load balancing daemon band steering control interface
 *
 * @Notes:
 *
 * @@-COPYRIGHT-START-@@
 *
 * Copyright (c) 2015-2018 Qualcomm Technologies, Inc.
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
#include "wlanifBSteerControl.h"
#include "wlanifBSteerControlCmn.h"

// ====================================================================
// Package level functions
// ====================================================================

LBD_STATUS wlanifBSteerControlGenerateRadioOperChanChangeEvents(
    wlanifBSteerControlHandle_t handle) {
    // Not supported in single AP daemon
    return LBD_NOK;
}

LBD_STATUS wlanifBSteerControlSetAPSteeringRSSIThreshold(
        wlanifBSteerControlHandle_t handle, const struct ether_addr *radioAddr,
        lbd_rssi_t rssi, wlanif_band_e *band) {
    // Not supported in single AP daemon
    return LBD_NOK;
}

LBD_STATUS wlanifBSteerControlSetRSSIMetricsReportingParams(
        wlanifBSteerControlHandle_t handle, const struct ether_addr *radioAddr,
        lbd_rssi_t rssi, u_int8_t hysteresis, wlanif_band_e *band) {
    // Not supported in single AP daemon
    return LBD_NOK;
}

LBD_STATUS wlanifBSteerControlSendBTMRequestMapParams(
    wlanifBSteerControlHandle_t handle, const lbd_bssInfo_t *assocBSS,
    const struct ether_addr *staAddr, u_int8_t dialogToken,
    u_int16_t disassocTimer, LBD_BOOL disassocImminent, LBD_BOOL abridgedBit,
    const wlanif_bssTargetInfo_t *candidate) {
    // Not supported in single AP daemon
    return LBD_NOK;
}

LBD_STATUS wlanifBSteerControlGetBSSInfoFromBSSID(
        wlanifBSteerControlHandle_t state,
        const u_int8_t *bssid, lbd_bssInfo_t *bss) {

    return wlanifBSteerControlCmnGetLocalBSSInfoFromBSSID(state, bssid, bss);
}

const struct ether_addr *wlanifBSteerControlGetBSSIDForBSSInfo(
        wlanifBSteerControlHandle_t state, const lbd_bssInfo_t *bssInfo) {
    return wlanifBSteerControlCmnGetBSSIDForLocalBSSInfo(bssInfo);
}

LBD_STATUS wlanifBSteerControlGetBSSesSameESS(
        wlanifBSteerControlHandle_t state, const lbd_bssInfo_t *bss,
        lbd_essId_t lastServingESS, wlanif_band_e requestedBand,
        size_t* maxNumBSSes, lbd_bssInfo_t *bssList) {
    return wlanifBSteerControlGetBSSesSameESSLocal(state, bss, lastServingESS,
                                                   requestedBand, maxNumBSSes,
                                                   bssList);
}

void wlanifBSteerControlNotifyRadioOperChanChange(wlanifBSteerControlHandle_t state) {
    // No module is expecting in single AP setup
    return;
}

LBD_BOOL wlanifBSteerControlGetSONInitVal(wlanifBSteerControlHandle_t state) {
    // SON is always disabled in single AP or uncoordinated steering scenarios.
    return LBD_FALSE;
}

LBD_STATUS wlanifBSteerControlGetBSSPHYCapInfo(wlanifBSteerControlHandle_t state,
                                               const lbd_bssInfo_t *bss,
                                               wlanif_phyCapInfo_t *phyCap) {
    return wlanifBSteerControlCmnGetLocalBSSPHYCapInfo(state, bss, phyCap);
}

LBD_STATUS wlanifBSteerControlCmnSendBcnRequestExtented(wlanifBSteerControlHandle_t handle,
                                                        const struct ether_addr *staAddr,
                                                        const wlanif_bcnMetricsQuery_t *query,
                                                        const lbd_bssInfo_t *bss,
                                                        u_int8_t clientClassGroup) {
    // Not supported in single AP daemon
    return LBD_NOK;
}

LBD_STATUS wlanifBSteerControlGetNLSteerMulticast(
        wlanifBSteerControlHandle_t state,
        u_int8_t *nlSteerMulticast) {
    return wlanifBSteerControlGetLocalNLSteerMulticast(state,
                                                       nlSteerMulticast);
}

LBD_STATUS wlanifBSteerControlGetAckRssiEnable(
        wlanifBSteerControlHandle_t state,
        u_int8_t *ack_rssi_enable) {
    return wlanifBSteerControlGetLocalAckRssiEnable(state,
                                                       ack_rssi_enable);
}

LBD_STATUS wlanifBSteerControlGetModCounter(
        wlanifBSteerControlHandle_t state,
        u_int32_t *mod_counter) {
    return wlanifBSteerControlGetLocalModCounter(state, mod_counter);
}
