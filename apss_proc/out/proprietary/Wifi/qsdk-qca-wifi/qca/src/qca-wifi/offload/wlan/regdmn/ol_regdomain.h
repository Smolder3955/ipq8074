/*
 * Copyright (c) 2011-2013, 2017-2018 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary . Qualcomm Innovation Center, Inc.
 *
 * 2011-2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * Notifications and licenses are retained for attribution purposes only.
 */
/*
 * Copyright (c) 2002-2006 Sam Leffler, Errno Consulting
 * Copyright (c) 2005-2006 Atheros Communications, Inc.
 * Copyright (c) 2010, Atheros Communications Inc.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the following conditions are met:
 * 1. The materials contained herein are unmodified and are used
 *    unmodified.
 * 2. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following NO
 *    ''WARRANTY'' disclaimer below (''Disclaimer''), without
 *    modification.
 * 3. Redistributions in binary form must reproduce at minimum a
 *    disclaimer similar to the Disclaimer below and any redistribution
 *    must be conditioned upon including a substantially similar
 *    Disclaimer requirement for further binary redistribution.
 * 4. Neither the names of the above-listed copyright holders nor the
 *    names of any contributors may be used to endorse or promote
 *    product derived from this software without specific prior written
 *    permission.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ''AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT,
 * MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE
 * FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGES.
 *
 * This module contains the regulatory domain private structure definitions .
 *
 */

#include <osdep.h>
#include <wbuf.h>

#define REGDMN_SUPPORT_11D
#define PEREGRINE_RDEXT_DEFAULT  0x1F

#ifndef _OL_DUP_COUNTRY_CODE_F
typedef enum _HAL_BOOL
{
    AH_FALSE = 0,       /* NB: lots of code assumes false is zero */
    AH_TRUE  = 1,
} HAL_BOOL;
#endif /* _OL_DUP_COUNTRY_CODE_F */

/* WMI_REG_CHAN_LIST_CC_EVENTID receive timeout msec */
#define REG_INIT_CC_RX_TIMEOUT     8000

#define ATH_REGCLASSIDS_MAX     10

#define WORLDWIDE_ROAMING_FLAG  0x4000


/* New function added for regdmn devices */
struct ol_regdmn {
    struct   ol_ath_softc_net80211 *scn_handle; /* handle to device */
    osdev_t  osdev;      /* handle to use OS-independent services */
    uint16_t ol_regdmn_current_rd;       /* Current regulatory domain */
    uint16_t ol_regdmn_current_rd_ext;   /* Regulatory domain Extension reg from EEPROM*/
    int      ol_regdmn_xchanmode;
    A_UINT32 ol_regdmn_ch144_eppovrd;    /* Enable/disable (1/0) EPPROM for Channel 144*/
};

typedef struct ol_regdmn *ol_regdmn_t;

int ol_regdmn_attach(struct ol_ath_softc_net80211 *scn_handle);
void ol_regdmn_detach(struct ol_regdmn* ol_regdmn_handle);
bool ol_regdmn_set_regdomain(struct ol_regdmn* ol_regdmn_handle, uint16_t regdomain);
bool ol_regdmn_set_regdomain_ext(struct ol_regdmn* ol_regdmn_handle, uint16_t regdomain);
int ol_regdmn_getchannels(struct ol_regdmn *ol_regdmn_handle, u_int cc, bool outDoor, bool xchanMode,  IEEE80211_REG_PARAMETERS *reg_parm);
void ol_regdmn_start(struct ol_regdmn *ol_regdmn_handle, IEEE80211_REG_PARAMETERS *reg_parm );
void ol_80211_channel_setup(struct ieee80211com *ic, enum ieee80211_clist_cmd cmd,
        const u_int8_t *regclassids, u_int nregclass);
bool ol_regdmn_set_ch144_eppovrd(struct ol_regdmn *ol_regdmn_handle, u_int ch144);

/*
 * Return bit mask of wireless modes supported by the hardware.
 */
u_int ol_regdmn_getWirelessModes(struct ol_regdmn* ol_regdmn_handle);

/**
* @brief        Checks if 160 MHz flag is set in wireless_modes
*
* @param hal_cap:   pointer to HAL_REG_CAPABILITIES
*
* @return       true if 160 MHz flag is set, false otherwise.
*/
bool ol_regdmn_get_160mhz_support(struct wlan_psoc_hal_reg_capability *hal_cap);

/**
* @brief        Checks if 80+80 MHz flag is set in wireless_modes
*
* @param hal_cap:   pointer to HAL_REG_CAPABILITIES
*
* @return       true if 80+80 MHz flag is set, false otherwise.
*/

bool ol_regdmn_get_80p80mhz_support(struct wlan_psoc_hal_reg_capability *hal_cap);

/**
 * @brief Add 802.11ax modes to list of supported wireless modes
 *
 * @details
 *  This is temporary code to populate 11ax modes into list of supported
 *  wireless modes on the basis of the radio's MAC/PHY capabilities. As a
 *  precaution, it also cross-checks for presence of the equivalent 11ng/11ac
 *  mode before populating a given 11ax mode (else the current regulatory
 *  channel initialization code could get confused).
 *  This function should be called
 *      - only if the MAC/PHY capabilities include 11ax support, else it will
 *        assert.
 *      - only after legacy wireless modes are populated.
 *  11AX TODO - Remove this function once converged regulatory module is ready.
 *
 * @param ol_regdmn_handle - Handle for offload regulatory domain instance
 * @param mac_phy_cap - MAC/PHY capabilities of the radio
 *
 * @return 0 on success, -1 on failure.
 */
int ol_regdmn_add_11ax_modes(struct ol_regdmn* ol_regdmn_handle,
        struct wlan_psoc_host_mac_phy_caps mac_phy_cap);

#if QCA_11AX_STUB_SUPPORT
/**
 * @brief Add 802.11ax modes to list of supported wireless modes
 *
 * @details
 *  This is only for test related stubbing purposes on chipsets that do not
 *  support 11ax.
 *
 * @param ol_regdmn_handle - Handle for offload regulatory domain instance
 */
extern void ol_regdmn_stub_add_11ax_modes(struct ol_regdmn* ol_regdmn_handle);

int
ol_ath_set_regdomain(struct ieee80211com *ic, int regdomain, bool no_chanchange);
#endif /* QCA_11AX_STUB_SUPPORT */

