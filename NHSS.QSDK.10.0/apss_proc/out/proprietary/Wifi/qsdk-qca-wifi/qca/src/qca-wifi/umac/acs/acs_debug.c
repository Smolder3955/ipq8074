/*
 * Copyright (c) 2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * Description:
 * The ACS debug framework enabled the testing and validation of the ACS
 * algorithm by the use of custom beacon and channel events which are injected
 * from the userspace to the driver
 */

#include <ieee80211_defines.h>
#include <wlan_scan_tgt_api.h>

#include "acs_debug.h"

static uint8_t nbss            = 0;
static uint8_t nchan           = 0;
static uint8_t is_bcn_injected = 0;

/*
 * process_phymode:
 * Takes the phymode value and accordingly adds flags to manage IE additions
 * as well as populating the said IEs. This is done in accordance to the
 * wlan_phymode enums
 *
 * Parameters:
 * bcn        : The pointer to the beacon database
 * bcn_phymode: The phymode value to manipulate IEs
 * secch_1    : Value of the VHTop secondary channel (segment 1)
 * secch_2    : Value of the VHTop secondary channel (segment 2)
 *
 * Return:
 * -1: Error
 *  0: Success
 */
static int process_phymode(struct acs_debug_bcn_event *bcn, uint32_t phymode,
                           uint8_t secch_1, uint8_t secch_2)
{
    int8_t ret = 0;

    switch(phymode) {
        case WLAN_PHYMODE_11A:
            bcn->is_dot11abg |= IS_DOT_11A;
            break;
        case WLAN_PHYMODE_11B:
            bcn->is_dot11abg |= IS_DOT_11B;
            break;
        case WLAN_PHYMODE_11G:
            bcn->is_dot11abg |= IS_DOT_11G;
            break;
        case WLAN_PHYMODE_11NA_HT20:
        case WLAN_PHYMODE_11NG_HT20:
            /*
             * There is no need to populate HTcap specifically
             * For 11NA_HT20 and 11NG_HT20, just the presence of
             * the htcap and htinfo IE is enough which will be added
             * since the is_dot11abg flag is not set
             */
            break;
        case WLAN_PHYMODE_11NA_HT40PLUS:
        case WLAN_PHYMODE_11NG_HT40PLUS:
            bcn->htcap.hc_ie.hc_cap        |= WLAN_HTCAP_C_CHWIDTH40;
            bcn->htinfo.hi_ie.hi_extchoff   = WLAN_HTINFO_EXTOFFSET_ABOVE;
            break;
        case WLAN_PHYMODE_11NA_HT40MINUS:
        case WLAN_PHYMODE_11NG_HT40MINUS:
            bcn->htcap.hc_ie.hc_cap        |= WLAN_HTCAP_C_CHWIDTH40;
            bcn->htinfo.hi_ie.hi_extchoff   = WLAN_HTINFO_EXTOFFSET_BELOW;
            break;
        case WLAN_PHYMODE_11AC_VHT20:
            bcn->is_dot11acplus       = 1;
            bcn->vhtop.vht_op_chwidth = WLAN_VHTOP_CHWIDTH_2040;
            break;
        case WLAN_PHYMODE_11AC_VHT40PLUS:
            bcn->is_dot11acplus            = 1;
            bcn->vhtop.vht_op_chwidth      = WLAN_VHTOP_CHWIDTH_2040;
            bcn->htcap.hc_ie.hc_cap       |= WLAN_HTCAP_C_CHWIDTH40;
            bcn->htinfo.hi_ie.hi_extchoff  = WLAN_HTINFO_EXTOFFSET_ABOVE;
            break;
        case WLAN_PHYMODE_11AC_VHT40MINUS:
            bcn->is_dot11acplus            = 1;
            bcn->vhtop.vht_op_chwidth      = WLAN_VHTOP_CHWIDTH_2040;
            bcn->htcap.hc_ie.hc_cap       |= WLAN_HTCAP_C_CHWIDTH40;
            bcn->htinfo.hi_ie.hi_extchoff  = WLAN_HTINFO_EXTOFFSET_BELOW;
            break;
        case WLAN_PHYMODE_11AC_VHT80:
            bcn->is_dot11acplus            = 1;
            bcn->vhtop.vht_op_chwidth      = WLAN_VHTOP_CHWIDTH_80;
            bcn->vhtop.vht_op_ch_freq_seg1 = secch_1;
            break;
        case WLAN_PHYMODE_11AC_VHT160:
            bcn->is_dot11acplus            = 1;
            bcn->vhtop.vht_op_chwidth      = WLAN_VHTOP_CHWIDTH_160;
            bcn->vhtop.vht_op_ch_freq_seg1 = secch_1;
            bcn->vhtop.vht_op_ch_freq_seg2 = secch_2;
            break;
        case WLAN_PHYMODE_11AC_VHT80_80:
            bcn->is_dot11acplus            = 1;
            bcn->vhtop.vht_op_chwidth      = WLAN_VHTOP_CHWIDTH_80_80;
            bcn->vhtop.vht_op_ch_freq_seg1 = secch_1;
            bcn->vhtop.vht_op_ch_freq_seg2 = secch_2;
            break;
        default:
            ret = ACSDBG_ERROR;
            break;
    }

    return ret;
}

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
                           struct acs_debug_raw_bcn_event *bcn_raw)
{
    uint8_t ix;
    int8_t  ret = 0;
    struct acs_debug_bcn_event *bcn =
                    (struct acs_debug_bcn_event *)ic_acs->acs_debug_bcn_events;

    is_bcn_injected = 0;

    if(bcn != NULL) {
        /*
         * On multiple invocations of the ACS tool, it will always reallocate
         * the existing space instead of allocating new memory
         */
        qdf_print("%s: Clearing old beacon data", __func__);
        qdf_mem_free(bcn);
        bcn = NULL;
        ic_acs->acs_debug_bcn_events = NULL;
    }

    if (bcn_raw == NULL) {
        qdf_print("%s: There are no beacon sent from the tool to inject",
                  __func__);
        return ACSDBG_ERROR;
    }

    nbss = bcn_raw[0].nbss;
    bcn = qdf_mem_malloc(nbss * sizeof(struct acs_debug_bcn_event));
    if (!bcn) {
        qdf_print("%s: Beacon allocation failed!", __func__);
        return ACSDBG_ERROR;
    }

    ic_acs->acs_debug_bcn_events = (void *)bcn;

    for (ix = 0; !ret && ix < nbss; ix++) {
        bcn[ix].is_dot11abg    = 0;
        bcn[ix].is_dot11acplus = 0;
        bcn[ix].is_srp         = 0;

        if (process_phymode(&bcn[ix], bcn_raw[ix].phymode,
                            bcn_raw[ix].sec_chan_seg1,
                            bcn_raw[ix].sec_chan_seg2)) {
            qdf_print("%s: Could not process phymode", __func__);
            ret = ACSDBG_ERROR;
            break;
        }

        qdf_mem_copy(bcn[ix].ssid.ssid, bcn_raw[ix].ssid,
                     strlen(bcn_raw[ix].ssid));
        bcn[ix].ds.current_channel = bcn_raw[ix].channel_number;
        bcn[ix].htinfo.hi_ie.hi_ctrlchannel = bcn_raw[ix].channel_number;
        qdf_mem_copy(bcn[ix].i_addr3, bcn_raw[ix].bssid, QDF_MAC_ADDR_SIZE);
        qdf_mem_copy(bcn[ix].i_addr2, bcn_raw[ix].bssid, QDF_MAC_ADDR_SIZE);
        bcn[ix].rssi = bcn_raw[ix].rssi;
        bcn[ix].is_srp = bcn_raw[ix].srpen;
    }

    if (!ret)
        qdf_print("%s: Populated ACS beacon events (debug framework)",
                  __func__);
    else {
        /*
         * In error cases, if there was memory allocated we are freeing them
         * here.
         */
        if (bcn != NULL) {
            qdf_mem_free(bcn);
            bcn = NULL;
        }

        qdf_print("%s: Could not populate ACS beacon events"
                  "(debug framework)", __func__);
    }

    /*
     * Memory will not be freed here because it is kept in a debug structure
     * within the ic-level ACS structure. Until the wifi is unloaded or
     * if there is another report that is sent in from the userspace
     * the database will remain.
     */
    return ret;
}

/*
 * init_bcn:
 * Initializes all the IEs regardless of what is going to be added to the
 * particular beacon for IE ID and length which are static
 *
 * Parameters:
 * bcn: Pointer to the beacon database
 *
 * Returns:
 * None
 */
void init_bcn(struct acs_debug_bcn_event *bcn)
{
    bcn->ssid.ssid_id = WLAN_ELEMID_SSID;
    bcn->ssid.ssid_len = ACS_DEBUG_MAX_SSID_LEN;

    bcn->rates.rate_id = WLAN_ELEMID_RATES;
    bcn->rates.rate_len = sizeof(struct ieee80211_ie_rates) - sizeof(struct ie_header);

    bcn->xrates.xrate_id = WLAN_ELEMID_XRATES;
    /*
     * We are selecting an arbitrary number between 0-255 so as to fit it
     * in the 8-bit unsigned xrate_len since the regular size is 256 bytes.
     * The framework is only concerned with the presence of the IE and not it's
     * content.
     */
    bcn->xrates.xrate_len = ACS_DEBUG_XRATES_NUM;

    bcn->ds.ie = WLAN_ELEMID_DSPARMS;
    bcn->ds.len = sizeof(struct ieee80211_ds_ie) - sizeof(struct ie_header);

    bcn->htinfo.hi_id = WLAN_ELEMID_HTINFO_ANA;
    bcn->htinfo.hi_len = sizeof(struct ieee80211_ie_htinfo) - sizeof(struct ie_header);

    bcn->htcap.hc_id = WLAN_ELEMID_HTCAP_ANA;
    bcn->htcap.hc_len = sizeof(struct ieee80211_ie_htcap) - sizeof(struct ie_header);

    bcn->vhtcap.elem_id = WLAN_ELEMID_VHTCAP;
    bcn->vhtcap.elem_len = sizeof(struct ieee80211_ie_vhtcap) - sizeof(struct ie_header);

    bcn->vhtop.elem_id = WLAN_ELEMID_VHTOP;
    bcn->vhtop.elem_len = sizeof(struct ieee80211_ie_vhtop) - sizeof(struct ie_header);

    bcn->srp.srp_id = WLAN_ELEMID_EXTN_ELEM;
    bcn->srp.srp_id_extn = WLAN_EXTN_ELEMID_SRP;
    bcn->srp.srp_len =  sizeof(struct ieee80211_ie_srp_extie) - sizeof(struct ie_header);

}

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
                      struct ieee80211com *ic)
{
    struct wlan_objmgr_psoc *psoc = soc->psoc_obj;
    struct ie_header *ie;
    struct wlan_frame_hdr *hdr;
    struct mgmt_rx_event_params *rx_param;
    qdf_nbuf_t buf;

    uint32_t frame_len;
    uint8_t ix;
    int8_t  ret = 0;

    struct acs_debug_bcn_event *bcn =
                  (struct acs_debug_bcn_event *)ic->ic_acs->acs_debug_bcn_events;

    if (is_bcn_injected) {
        /* This API is run everytime the WMI handler receives a channel event
         * therefore, to prevent it form running N times (N is the number of
         * channels in the HW mode) we use this flag to exit this API if N > 1
         */
        return ACSDBG_SUCCESS;
    }

    if (bcn == NULL) {
        qdf_print("%s: There are no custom-beacons to inject", __func__);
        ret = ACSDBG_ERROR;
    }

   for (ix = 0; !ret && ix < nbss; ix++) {
       init_bcn(&bcn[ix]);

       /* Setting the standard frame length for the custom beacon */
       frame_len = sizeof(struct wlan_frame_hdr) + sizeof(struct wlan_bcn_frame)
                   - sizeof(struct ie_header) + sizeof(struct ieee80211_ie_ssid)
                   + sizeof(struct ieee80211_ie_rates) + sizeof(struct ieee80211_ds_ie);

       if (!bcn[ix].is_dot11abg) {
           frame_len += sizeof(struct ieee80211_ie_htcap)
                        + sizeof(struct ieee80211_ie_htinfo);
       }

       if (bcn[ix].is_dot11abg & IS_DOT_11G) {
           /*
            * Can't take the size of the IE directly since the size of the rates
            * array is 256 which is larger than what the 8-bit value of xrate_len
            * can hold.
            */
           frame_len += sizeof(struct ie_header) + bcn[ix].xrates.xrate_len;
       }

       if (bcn[ix].is_dot11acplus) {
           frame_len += sizeof(struct ieee80211_ie_vhtcap)
                        + sizeof(struct ieee80211_ie_vhtop);
       }

       if (bcn[ix].is_srp) {
           frame_len += sizeof(struct ieee80211_ie_srp_extie);
       }

       buf = qdf_nbuf_alloc(soc->qdf_dev, frame_len, 0, 0, FALSE);
       if (!buf) {
           qdf_print("%s: Buffer allocation failed", __func__);
           ret = ACSDBG_ERROR;
           continue;
       }

       qdf_nbuf_set_pktlen(buf, frame_len);
       qdf_mem_zero((uint8_t *)qdf_nbuf_data(buf), frame_len);

       hdr = (struct wlan_frame_hdr *)qdf_nbuf_data(buf);
       qdf_mem_copy(hdr->i_addr3, bcn[ix].i_addr3, QDF_MAC_ADDR_SIZE);
       qdf_mem_copy(hdr->i_addr2, bcn[ix].i_addr2, QDF_MAC_ADDR_SIZE);

       rx_param = qdf_mem_malloc(sizeof(struct mgmt_rx_event_params));
       if (!rx_param) {
           qdf_print("%s: Rx params could not be allocated", __func__);
           ret = ACSDBG_ERROR;
           continue;
       }

       rx_param->rssi    = bcn[ix].rssi;
       rx_param->channel = bcn[ix].ds.current_channel;
       rx_param->pdev_id = ic->ic_pdev_obj->pdev_objmgr.wlan_pdev_id;

       ie = (struct ie_header *)(((uint8_t *)qdf_nbuf_data(buf))
                                  + sizeof(struct wlan_frame_hdr)
                                  + offsetof(struct wlan_bcn_frame, ie));

       ACS_DEBUG_POPULATE_IE(ie, bcn[ix].ssid,  bcn[ix].ssid.ssid_len);
       ACS_DEBUG_POPULATE_IE(ie, bcn[ix].rates, bcn[ix].rates.rate_len);
       ACS_DEBUG_POPULATE_IE(ie, bcn[ix].ds,    bcn[ix].ds.len);

       if (!bcn[ix].is_dot11abg) {
           ACS_DEBUG_POPULATE_IE(ie, bcn[ix].htcap,  bcn[ix].htcap.hc_len);
           ACS_DEBUG_POPULATE_IE(ie, bcn[ix].htinfo, bcn[ix].htinfo.hi_len);
       }

       if (bcn[ix].is_dot11abg & IS_DOT_11G) {
           ACS_DEBUG_POPULATE_IE(ie, bcn[ix].xrates, bcn[ix].xrates.xrate_len);
       }

       if (bcn[ix].is_dot11acplus) {
           ACS_DEBUG_POPULATE_IE(ie, bcn[ix].vhtcap, bcn[ix].vhtcap.elem_len);
           ACS_DEBUG_POPULATE_IE(ie, bcn[ix].vhtop,  bcn[ix].vhtop.elem_len);
       }

       if (bcn[ix].is_srp) {
           ACS_DEBUG_POPULATE_IE(ie, bcn[ix].srp, bcn[ix].srp.srp_len);
       }

       if (tgt_scan_bcn_probe_rx_callback(psoc, NULL, buf, rx_param,
                                          MGMT_BEACON)) {
           qdf_print("%s: Could not send beacon: %s", __func__,
                                                        bcn[ix].ssid.ssid);
           ret = ACSDBG_ERROR;
           continue;
       }

   }
   /* Preventing the API to run more than once because of the WMI handler */
   is_bcn_injected = 1;

   /*
    * Memory will not be freed here because it is kept in a debug structure
    * within the ic-level ACS structure. Until the wifi is unloaded or
    * if there is another report that is sent in from the userspace
    * the database will remain.
    */
   return ret;
}

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
                            struct acs_debug_raw_chan_event *chan_raw)
{
    uint8_t ix;

    struct acs_debug_chan_event *chan =
                   (struct acs_debug_chan_event *)ic_acs->acs_debug_chan_events;

    if (chan != NULL) {
        /*
         * If we are running the tool multiple times, it will delete the old
         * database before populating the new one, preventing excess memory
         * usage
         */
        qdf_print("%s: Clearing old channel data", __func__);
        qdf_mem_free(chan);
        chan = NULL;
        ic_acs->acs_debug_chan_events = NULL;
    }

    if (chan_raw == NULL) {
        qdf_print("%s: There are no channel events to inject", __func__);
        return ACSDBG_ERROR;
    }

    nchan = chan_raw[0].nchan;
    chan = qdf_mem_malloc(nchan * sizeof(struct acs_debug_chan_event));
    if (!chan) {
        qdf_print("%s: Channel allocation failed", __func__);
        return ACSDBG_ERROR;
    }

    ic_acs->acs_debug_chan_events = (void *)chan;

    for (ix = 0; ix < nchan; ix++) {
        chan[ix].channel_number = chan_raw[ix].channel_number;
        chan[ix].chan.cycle_cnt = ACS_DEBUG_DEFAULT_CYCLE_CNT_VAL;
        chan[ix].chan.chan_clr_cnt = (uint32_t)(chan_raw[ix].channel_load *
                                               chan[ix].chan.cycle_cnt) / 100;
        chan[ix].chan.chan_tx_power_tput = chan_raw[ix].txpower;
        chan[ix].chan.chan_tx_power_range = chan_raw[ix].txpower;
        chan[ix].noise_floor = chan_raw[ix].noise_floor;
    }

    qdf_print("%s: Populated ACS channel events (debug framework)",
                                                                 __func__);

    return ACSDBG_SUCCESS;
}

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
                             ieee80211_acs_t ic_acs)
{
    uint8_t ix;
    int8_t ret = 0;
    struct acs_debug_chan_event *chan;

    chan = (struct acs_debug_chan_event *)ic_acs->acs_debug_chan_events;

    if (chan == NULL) {
        /* If there are no channel events to inject, then it will go ahead and
         * inject the genuine beacons
         */
        return ACSDBG_SUCCESS;
    }

    switch(*ieee_chan) {
        case 36:    ix =  0; break;
        case 40:    ix =  1; break;
        case 44:    ix =  2; break;
        case 48:    ix =  3; break;
        case 52:    ix =  4; break;
        case 56:    ix =  5; break;
        case 60:    ix =  6; break;
        case 64:    ix =  7; break;
        case 100:   ix =  8; break;
        case 104:   ix =  9; break;
        case 108:   ix = 10; break;
        case 112:   ix = 11; break;
        case 116:   ix = 12; break;
        case 120:   ix = 13; break;
        case 124:   ix = 14; break;
        case 128:   ix = 15; break;
        case 132:   ix = 16; break;
        case 136:   ix = 17; break;
        case 140:   ix = 18; break;
        case 144:   ix = 19; break;
        case 149:   ix = 20; break;
        case 153:   ix = 21; break;
        case 157:   ix = 22; break;
        case 161:   ix = 23; break;
        case 165:   ix = 24; break;
        default :   ix = *ieee_chan - 1; break;
    }

    *chan_nf = chan[ix].noise_floor;
    qdf_mem_copy((void *)chan_stats, (void *)&(chan[ix].chan),
                  sizeof(struct ieee80211_chan_stats));

    return ret;
}

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
void acs_debug_reset_bcn_flag(void)
{
    qdf_print("%s: Resetting flag", __func__);
    is_bcn_injected = 0;
}

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
void acs_debug_cleanup(ieee80211_acs_t ic_acs)
{
    struct acs_debug_bcn_event *bcn =
                     (struct acs_debug_bcn_event *)ic_acs->acs_debug_bcn_events;
    struct acs_debug_chan_event *chan =
                   (struct acs_debug_chan_event *)ic_acs->acs_debug_chan_events;

    qdf_print("%s: Freeing all memory from the ACS debug framework",
                                                            __func__);

    if (bcn != NULL) {
        qdf_mem_free(bcn);
        bcn = NULL;
    }

    if (chan != NULL) {
        qdf_mem_free(chan);
        chan = NULL;
    }
}
