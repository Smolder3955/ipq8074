/* Copyright (c) 2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * Description:
 * Contains API definitions for usage in the ACS debug framework to support
 * acs_debug.c
 */

#ifndef _ACS_DEBUG_H
#define _ACS_DEBUG_H

#include <ieee80211.h>
#include <ieee80211_var.h>
#include <ol_if_athvar.h>
#include <ieee80211_acs_internal.h>
#include <ieee80211_acs.h>

/* -- COMMON -- */
#define ACSDBG_ERROR     -EINVAL
#define ACSDBG_SUCCESS       EOK


/* -- CHANNEL EVENTS -- */
#define ACS_DEBUG_DEFAULT_CYCLE_CNT_VAL  100000
#define ACS_DEBUG_DEFAULT_FLAG                3

/*
 * acs_debug_raw_chan_event:
 * The format of the channel event as read from the user-space tool
 */
struct acs_debug_raw_chan_event {
    int8_t  nchan;
    uint8_t  channel_number;
    uint8_t  channel_load;
    int16_t  noise_floor;
    uint32_t txpower;
};

/*
 * acs_debug_chan_event:
 * The format of the channel event before sending to the ACS algorithm
 */
struct acs_debug_chan_event {
    uint8_t channel_number;
    struct  ieee80211_chan_stats chan;
    int16_t noise_floor;
};

/* -- BEACON EVENTS -- */
#define ACS_DEBUG_MAX_SSID_LEN  32
#define ACS_DEBUG_MAC_ADDR_SIZE  6

#define ACS_DEBUG_XRATES_NUM    20

#define IS_DOT_11A    0x001
#define IS_DOT_11B    0x010
#define IS_DOT_11G    0x100

#define ACS_DEBUG_POPULATE_IE(buf, ie_name, ie_len)                          \
        qdf_mem_copy(buf, &ie_name, sizeof(ie_name));                        \
        buf = (struct ie_header *)((uint8_t *)buf + sizeof(struct ie_header) \
              + ie_len);

/*
 * acs_debug_raw_bcn_event:
 * The format of the beacon events when read from the userspace
 */
struct acs_debug_raw_bcn_event {
    int8_t  nbss;
    uint32_t channel_number;
    int32_t  rssi;
    uint8_t  bssid[ACS_DEBUG_MAC_ADDR_SIZE];
    uint8_t  ssid[ACS_DEBUG_MAX_SSID_LEN];
    uint32_t phymode;
    uint8_t  sec_chan_seg1;
    uint8_t  sec_chan_seg2;
    uint8_t  srpen;
};

/*
 * acs_debug_bcn_event:
 * The format of the beacon events before sending to the ACS algorithm
 */
struct acs_debug_bcn_event {
    uint8_t is_dot11abg;
    uint8_t is_dot11acplus;
    uint8_t is_srp;
    int32_t rssi;
    uint8_t i_addr2[ACS_DEBUG_MAC_ADDR_SIZE];
    uint8_t i_addr3[ACS_DEBUG_MAC_ADDR_SIZE];
    /* Supported Beacon IEs in the ACS Debug Framework */
    struct ieee80211_ie_ssid      ssid;
    struct ieee80211_ie_rates     rates;
    struct ieee80211_ie_xrates    xrates;
    struct ieee80211_ds_ie        ds;
    struct ieee80211_ie_htinfo    htinfo;
    struct ieee80211_ie_htcap     htcap;
    struct ieee80211_ie_vhtcap    vhtcap;
    struct ieee80211_ie_vhtop     vhtop;
    struct ieee80211_ie_srp_extie srp;
};

/*
 * acs_debug_create_bcndb:
 * Takes the beacon information sent from the userspace and creates a database
 * of al lthe beacons which is kept ready to be injected into the ACS algorithm
 *
 * Parameters:
 * ic_acs : Pointer to the ACS structure
 * bcn_raw: Pointer to the raw beacon information sent from the userspace
 *
 * Return:
 * -1: Error
 *  0: Success
 */
int acs_debug_create_bcndb(ieee80211_acs_t ic_acs,
                           struct acs_debug_raw_bcn_event *bcn_raw);

/*
 * init_bcn:
 * Initializes all the IEs regardless of what is going to be added to the
 * particular beacon for IE ID and length which are static
 *
 * Parameters:
 * bcn_ev = Pointer to the beacon database
 *
 * Returns:
 * None
 */
void init_bcn(struct acs_debug_bcn_event *bcn);

/*
 * acs_debug_add_bcn:
 * Injects the custom beacons into the ACS algorithm by creating a scan_entry
 * for the custom-user-defined BSSIDs
 *
 * Parameters:
 * soc: Pointer to the SoC object
 * ic : Pointer to the radio_level ic structure
 *
 * Return:
 * -1: Error
 *  0: Success
 */
int acs_debug_add_bcn(ol_ath_soc_softc_t *soc,
                      struct ieee80211com *ic);

/*
 * acs_debug_create_chandb:
 * This API takes the channel information from the userspace and creates a
 * database of all the channel statistics which is kept ready to be injected
 * into the ACS algorithm
 *
 * Parameters:
 * ic_acs: Pointer to the ACS structure
 * chan_raw: Pointer to the raw channel information sent from the userspace
 *
 * Return:
 * -1: Error
 *  0: Success
 */
int acs_debug_create_chandb(ieee80211_acs_t ic_acs,
                            struct acs_debug_raw_chan_event *chan_raw);

/*
 * acs_debug_add_chan_event:
 * Injects the channel events status into the ACS algorithm by sending the
 * custom values during the invocation of the WMI event handler when receiving
 * genuine statistics from the firmware.
 *
 * Parameters:
 * chan_stats: Pointer to the channel stats which are to be sent to the ACS
 *             algorithm
 * chan_nf   : Pointer to the value of the noise floor of the particular channel
 * ieee_chan : Pointer containing the channel number of the particular channel
 *
 * Returns:
 * -1: Error
 *  0: Success
 */
int acs_debug_add_chan_event(struct ieee80211_chan_stats *chan_stats,
                             int16_t *chan_nf, uint32_t *ieee_chan,
                             ieee80211_acs_t ic_acs);

/*
 * acs_debug_reset_bcn_flag:
 * Resets the beacon flag after every tun of the ACs so the database can be sent
 * in again.
 *
 * Parameters:
 * None
 *
 * Returns:
 * None
 */
void acs_debug_reset_bcn_flag(void);

/*
 * acs_debug_cleanup:
 * Clears all the occupied memory during the unload of the Wi-Fi module
 *
 * Parameters:
 * None
 *
 * Returns:
 * None
 */
void acs_debug_cleanup(ieee80211_acs_t ic_acs);

#endif /* _ACS_DEBUG_H */
