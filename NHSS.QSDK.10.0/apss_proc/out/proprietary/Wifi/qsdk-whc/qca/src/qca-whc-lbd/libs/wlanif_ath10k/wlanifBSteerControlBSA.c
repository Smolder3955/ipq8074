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
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * 2015-2016 Qualcomm Atheros, Inc.
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
