// vim: set et sw=4 sts=4 cindent:
/*
 * @File: stadbCmn.h
 *
 * @Abstract: Common APIs for use by BSA and MBSA modules
 *
 * @Notes:
 *
 * @@-COPYRIGHT-START-@@
 *
 * Copyright (c) 2018 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * @@-COPYRIGHT-END-@@
 */

#ifndef stadbCmn__h
#define stadbCmn__h

#include "lbd_types.h"
#include "wlanif.h"

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief Record in the database that the STA associated or disassociated on
 *        the specified band.
 *
 * @param [in] addr  the MAC address of the STA
 * @param [in] bss  the BSS on which the STA associated/disassociated
 * @param [in] btmStatus whether BTM support is enabled,
 *                       disabled, or unchanged from the current
 *                       state
 * @param [in] classification group the STA belongs to
 * @param [in] rrmStatus whether 802.11 Radio Resource Management is supported,
 *                       disabled, or unchanged from the current state
 * @param [in] isMUMIMOSupported  set to LBD_TRUE if MU-MIMO is
 *                                supported by the STA,
 *                                LBD_FALSE otherwise
 * @param [in] isStaticSMPS  whether the STA is operating in static SMPS mode
 * @param [in] phyCapInfo  PHY capability supported by the STA
 * @param [in] isAssoc  LBD_TRUE if it was an association; otherwise LBD_FALSE
 * @param [in] assocAge  the number of seconds since the STA associated
 * @param [in] verifyAssociation  whether to verify that the association
 *                                information is up-to-date
 */
void stadbUpdateAssoc(const struct ether_addr *addr,
                      const lbd_bssInfo_t *bss,
                      wlanif_capStateUpdate_e btmStatus,
                      wlanif_capStateUpdate_e rrmStatus,
                      u_int8_t sta_class,
                      wlanif_capStateUpdate_e isMUMIMOSupported,
                      wlanif_capStateUpdate_e isStaticSMPS,
                      u_int8_t bandCap, const wlanif_phyCapInfo_t *phyCapInfo,
                      LBD_BOOL isAssoc, time_t assocAge,
                      LBD_BOOL verifyAssociation);

#if defined(__cplusplus)
}
#endif

#endif  // stadbCmn__h
