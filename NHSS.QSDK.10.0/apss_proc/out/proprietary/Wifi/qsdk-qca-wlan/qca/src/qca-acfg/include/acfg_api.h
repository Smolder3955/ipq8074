/*
 * Copyright (c) 2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * Copyright (c) 2008-2010, Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef __ACFG_API_H
#define __ACFG_API_H

#include <qcatools_lib.h>
#include <qdf_types.h>
#ifndef BUILD_X86
#include <ieee80211_external_config.h>
#else
#include <bsd/string.h>
#endif

char ctrl_hapd[ACFG_CTRL_IFACE_LEN];
char ctrl_wpasupp[ACFG_CTRL_IFACE_LEN];

/**
 * @brief  Initialize interface for device-less configurations
 *
 * @return
 */
uint32_t
acfg_dl_init(void);

/**
 * @brief Create a VAP on the specified WIFI
 *
 * @param wifi_name (Upto ACFG_MAX_IFNAME)
 * @param vap_name  (Upto ACFG_MAX_IFNAME)
 * @param mode
 * @param vapid
 * @param flags
 * @return
 */
uint32_t
acfg_create_vap(uint8_t            *wifi_name,
        uint8_t            *vap_name,
        acfg_opmode_t         mode,
        int32_t             vapid,
        uint32_t             flags);

/**
 * @brief Delete a VAP on the specified WIFI
 *
 * @param wifi_name (Upto ACFG_MAX_IFNAME)
 * @param vap_name  (Upto ACFG_MAX_IFNAME)
 *
 * @return
 */
uint32_t
acfg_delete_vap(uint8_t *wifi_name, uint8_t *vap_name);



/**
 * @brief Determine whether a VAP is local or remote
 *
 * @param vap_name  (Upto ACFG_MAX_IFNAME)
 *
 * @return On Success
 *             If vap is offload (remote): A_STATUS_OK
 *             Else: A_STATUS_FAILED
 *         On Error: A_STATUS_EFAULT
 */
uint32_t
acfg_is_offload_vap(uint8_t *vap_name);

/**
 * @brief Add client with overriding params
 *
 * @param vap_name  (Upto ACFG_MAX_IFNAME)
 *        mac
 *        aid
 *        qos
 *        lrates
 *        htrates
 *        vhtrates
 *
 * @return On Success
 *             If : A_STATUS_OK
 *             Else: A_STATUS_FAILED
 *         On Error: A_STATUS_EFAULT
 */
uint32_t
acfg_add_client(uint8_t *vap_name, uint8_t *mac, uint32_t aid, uint32_t qos,
                acfg_rateset_t lrates, acfg_rateset_t htrates, acfg_rateset_t vhtrates,
                acfg_rateset_t herates);

/**
 * @brief Authorize client
 *
 * @param vap_name  (Upto ACFG_MAX_IFNAME)
 *        mac
 *
 * @return On Success
 *             If : A_STATUS_OK
 *             Else: A_STATUS_FAILED
 *         On Error: A_STATUS_EFAULT
 */
uint32_t
acfg_forward_client(uint8_t *vap_name, uint8_t *mac);

/**
 * @brief Delete a client
 *
 * @param vap_name  (Upto ACFG_MAX_IFNAME)
 *        mac
 *
 * @return On Success
 *             If : A_STATUS_OK
 *             Else: A_STATUS_FAILED
 *         On Error: A_STATUS_EFAULT
 */
uint32_t
acfg_delete_client(uint8_t *vap_name, uint8_t *mac);

/**
 * @brief Delete the encryption key
 *
 * @param vap_name  (Upto ACFG_MAX_IFNAME)
 *        mac
 *        key index
 *
 * @return On Success
 *             If : A_STATUS_OK
 *             Else: A_STATUS_FAILED
 *         On Error: A_STATUS_EFAULT
 */
uint32_t
acfg_del_key(uint8_t *vap_name, uint8_t *mac, uint16_t keyidx);

/**
 * @brief Set the encryption key
 *
 * @param vap_name  (Upto ACFG_MAX_IFNAME)
 *        mac
 *        cipher
 *        key index
 *        key length
 *        keydata - 64 bytes
 *
 * @return On Success
 *             If : A_STATUS_OK
 *             Else: A_STATUS_FAILED
 *         On Error: A_STATUS_EFAULT
 */
uint32_t
acfg_set_key(uint8_t *vap_name, uint8_t *mac, CIPHER_METH cipher, uint16_t keyidx,
             uint32_t keylen, uint8_t *keydata);

/**
 * @brief Set the SSID for the given VAP
 *
 * @param vap_name (Upto ACFG_MAX_IFNAME)
 * @param ssid
 *
 * @return
 */
uint32_t
acfg_set_ssid(uint8_t *vap_name, acfg_ssid_t *ssid);

/**
 * @brief Get the ssid for the given vap
 *
 * @param vap_name (Upto ACFG_MAX_IFNAME)
 * @param ssid
 */
uint32_t
acfg_get_ssid(uint8_t *vap_name, acfg_ssid_t *ssid);

/**
 * @brief Get the rssi for the given vap
 *
 * @param vap_name (Upto ACFG_MAX_IFNAME)
 * @param rssi
 */
uint32_t
acfg_get_rssi(uint8_t *vap_name, acfg_rssi_t *rssi);


/**
 * @brief Set the Channel (IEEE) number
 *
 * @param wifi_name (Radio)
 * @param chan_num (Upto ACFG_MAX_IEEE_CHAN)
 */
uint32_t
acfg_set_channel(uint8_t *wifi_name, uint8_t chan_num);

/**
 * @brief Get the Channel (IEEE) number
 *
 * @param wifi_name (Radio)
 * @param chan_num
 */
uint32_t
acfg_get_channel(uint8_t *wifi_name, uint8_t *chan_num);

/**
 * @brief Get scan results
 *
 * @param vap_name (Upto ACFG_MAX_IFNAME)
 * @param list (Pointer to the list of scanned results)
 * @param len (Pointer to store the number of scan entries)
 * @param offset (If there are more results, this should be
 *                used to retrive the next block)
 *
 * @return (A_STATUS_OK if no more scan results or else
 *          A_STATUS_EINPROGRESS for more results pending)
 */
//uint32_t
//acfg_get_scan_list(uint8_t         *vap_name,
//        acfg_scan_list_t  *list,
//        uint32_t        *len,
//        uint8_t          offset);

/**
 * @brief Set the opmode
 *
 * @param wifi_name (Radio)
 * @param opmode
 */
uint32_t
acfg_set_opmode(uint8_t *wifi_name, acfg_opmode_t opmode);

/**
 * @brief Get the opmode
 *
 * @param wifi_name (Radio)
 * @param opmode
 */
uint32_t
acfg_get_opmode(uint8_t *wifi_name, acfg_opmode_t *opmode);

/**
 * @brief Set the frequency
 *
 * @param wifi_name
 * @param freq - Frequency in MHz
 *
 * @return
 */
uint32_t
acfg_set_freq(uint8_t *wifi_name, uint32_t freq);

/**
 * @brief Get RTS threshold. The value returned is in iwparam->value.
 *        Other fields can be ignored.
 *
 * @param vap_name
 * @param rts
 *
 * @return
 */
uint32_t
acfg_get_rts(uint8_t *vap_name, acfg_rts_t *rts);

/**
 *   @brief Set RTS threshold. The value returned is in iwparam->value.
 *          Other fields can be ignored.
 *
 *     @param vap_name
 *     @rts value
 *     @rts flags
 *     @return
 */
uint32_t
acfg_set_rts(uint8_t *vapname, acfg_rts_t *rts);

/**
 *   @brief Set fragmentation threshold.
 *
 *     @param vap_name
 *     @param frag
 *     @return
 */

uint32_t
acfg_set_frag(uint8_t *vapname, acfg_frag_t *frag);

/**
 * @brief Get Fragmentation threshold
 *
 * @param wifi_name
 * @param iwparam
 *
 * @return
 */
uint32_t
acfg_get_frag(uint8_t *vap_name, acfg_frag_t *frag);

uint32_t
acfg_set_txpow(uint8_t *vapname, acfg_txpow_t *txpow);


/**
 * @brief Get default Tx Power in dBm
 *
 * @param wifi_name
 * @param txpow
 */
uint32_t
acfg_get_txpow(uint8_t *wifi_name, acfg_txpow_t *txpow);

/**
 * @brief Get Access Point MAC Address
 *
 * @param vap_name
 * @param mac
 */
uint32_t
acfg_get_ap(uint8_t *vap_name, acfg_macaddr_t * mac);

/**
 * @brief Set Encode Information
 *
 * @param vap_name
 * @param flag
 * @param len
 * @param enc
 */
uint32_t
acfg_set_enc(uint8_t *vap_name, acfg_encode_flags_t flag, char *enc);

/**
 * @brief set Vap vendor param
 *
 * @param vap_name
 * @param param
 * @param data
 * @param len
 * @param type
 * @param reinit
 * @return
 */
uint32_t
acfg_set_vap_vendor_param(uint8_t *vap_name, \
        acfg_vendor_param_vap_t param, uint8_t *data,
        uint32_t len, uint32_t type, acfg_vendor_param_init_flag_t reinit);

/**
 * @brief get Vap vendor param
 *
 * @param vap_name
 * @param param
 * @param data
 * @param type
 * @return
 */
uint32_t
acfg_get_vap_vendor_param(uint8_t *vap_name, \
        acfg_vendor_param_vap_t param, uint8_t *data,
        uint32_t *type);

/**
 * @brief Set Vap param
 *
 * @param vap_name  -   vap name
 * @param param     -   param id
 * @param val       -   value
 *
 * @return
 */
uint32_t
acfg_set_vap_param(uint8_t *vap_name, \
        acfg_param_vap_t param, uint32_t val);


/**
 * @brief Get Vap param
 *
 * @param vap_name  -   vap name
 * @param param     -   param id
 * @param val       -   pointer to memory for return value
 *
 * @return
 */
uint32_t
acfg_get_vap_param(uint8_t *vap_name, \
        acfg_param_vap_t param, uint32_t *val);

/**
 * @brief Set Radio param
 *
 * @param radio_name    -   radio name
 * @param param         -   param id
 * @param val           -   value
 *
 * @return
 */
uint32_t
acfg_set_radio_param(uint8_t *radio_name, \
        acfg_param_radio_t param, uint32_t val);

/**
 * @brief Get Radio param
 *
 * @param radio_name    -   radio name
 * @param param         -   param id
 * @param val           -   pointer to memory for return value
 *
 * @return
 */
uint32_t
acfg_get_radio_param(uint8_t *radio_name, \
        acfg_param_radio_t param, uint32_t *val);

uint32_t
acfg_set_rate(uint8_t *vap_name, acfg_rate_t *rate);

/**
 * @brief Get default bit rate
 *
 * @param vap_name
 * @param mac
 *
 * @return
 */
uint32_t
acfg_get_rate(uint8_t *vap_name, uint32_t *rate);

/**
 * @brief Enable MU Grouping per tidmask
 *
 * @param vap_name  (Upto ACFG_MAX_IFNAME)
 *        mac
 *        tidmask
 *
 * @return On Success
 *             If : A_STATUS_OK
 *             Else: A_STATUS_FAILED
 *         On Error: A_STATUS_EFAULT
 */
uint32_t
acfg_set_mu_whtlist(uint8_t *vap_name, uint8_t *mac, uint16_t tidmask);

/**
 * @brief Set the phymode
 *
 * @param vap_name
 * @param mode
 *
 * @return
 */
uint32_t
acfg_set_phymode(uint8_t *vap_name, uint32_t mode);

/**
 * @brief Get the phymode
 *
 * @param vap_name
 * @param mode
 *
 * @return
 */
uint32_t
acfg_get_phymode(uint8_t *vap_name, uint32_t *mode);

uint32_t
acfg_assoc_sta_info(uint8_t *vap_name, acfg_sta_info_req_t *sinfo);

/**
 * @brief Set Reg value
 *
 * @param radio_name    -   radio name
 * @param offset        -   offset
 * @param val           -   value
 *
 * @return
 */
uint32_t
acfg_set_reg(uint8_t *radio_name, \
        uint32_t offset, uint32_t val);

/**
 * @brief Get Reg Value
 *
 * @param radio_name    -   radio name
 * @param offset        -   offset
 * @param val           -   pointer to memory for return value
 *
 * @return
 */
uint32_t
acfg_get_reg(uint8_t *radio_name, \
        uint32_t param, uint32_t *val);


uint32_t
acfg_hostapd_getconfig(uint8_t *vap_name, char *reply_buf);

uint32_t
acfg_hostapd_set_wpa(uint8_t *argv[]);


/*
 * Functions pertaining to adding/removing/getting
 * mac addresses from the first ACL list.
 */
uint32_t
acfg_acl_addmac(uint8_t *vap_name, uint8_t *addr);

uint32_t
acfg_acl_getmac(uint8_t *vap_name, acfg_macacl_t *maclist);

uint32_t
acfg_acl_delmac(uint8_t *vap_name, uint8_t *addr);

/*
 * Functions pertaining to adding/removing/getting
 * mac addresses from the secondary ACL list.
 */
uint32_t
acfg_acl_addmac_secondary(uint8_t *vap_name, uint8_t *addr);

uint32_t
acfg_acl_getmac_secondary(uint8_t *vap_name, acfg_macacl_t *maclist);

uint32_t
acfg_acl_delmac_secondary(uint8_t *vap_name, uint8_t *addr);

uint32_t
acfg_set_ap(uint8_t *vap_name, uint8_t *addr);

uint32_t
acfg_wlan_profile_get (acfg_wlan_profile_t *profile);

uint32_t
acfg_wlan_vap_profile_get (acfg_wlan_profile_vap_params_t *vap_params);

uint32_t
acfg_mon_listmac(uint8_t *vap_name);

uint32_t
acfg_mon_enable_filter(uint8_t *vap_name, u_int32_t val);

uint32_t
acfg_mon_addmac(uint8_t *vap_name, uint8_t *addr);

uint32_t
acfg_mon_delmac(uint8_t *vap_name, uint8_t *addr);

void
acfg_wlan_vap_profile_print (acfg_wlan_profile_vap_params_t *vap_params);

void
acfg_wlan_profile_print (acfg_wlan_profile_t *profile);

uint32_t
acfg_set_profile(acfg_wlan_profile_t *new_profile,
        acfg_wlan_profile_t *cur_profile);

uint32_t
acfg_write_file(acfg_wlan_profile_t *new_profile);

uint32_t
acfg_reset_cur_profile(char *radio_name);

uint32_t
acfg_apply_profile(acfg_wlan_profile_t *new_profile);

acfg_wlan_profile_t * acfg_get_profile(char *radio_name);

uint32_t acfg_recover_profile(char *radio_name);

void
acfg_free_profile(acfg_wlan_profile_t *profile);

uint32_t
acfg_get_current_profile (acfg_wlan_profile_t *profile);

void
acfg_init_profile(acfg_wlan_profile_t *profile);

void
acfg_init_radio_params (acfg_wlan_profile_radio_params_t *unspec_radio_params);

void
acfg_init_vap_params (acfg_wlan_profile_vap_params_t *unspec_vap_params);

void
acfg_mac_str_to_octet(uint8_t *mac_str, uint8_t *mac);

uint32_t
acfg_set_wps_pbc(char *ifname);

uint32_t
acfg_set_wps_pin(char *ifname, int action, char *pin, char *pin_txt,
        char *bssid);

uint32_t
acfg_wps_config(uint8_t *ifname, char *ssid,
        char *auth, char *encr, char *key);

char *acfg_get_errstr(void);

uint32_t
acfg_set_radio_enable(uint8_t *ifname);

uint32_t
acfg_set_radio_disable(uint8_t *ifname);

uint32_t
acfg_set_country(uint8_t *radio_name, uint16_t country_code);

uint32_t
acfg_get_country(uint8_t *radio_name, uint32_t *country_code);

uint32_t
acfg_set_regdomain(uint8_t *radio_name, uint32_t regdomain);

uint32_t
acfg_get_regdomain(uint8_t *radio_name, uint32_t *regdomain);

uint32_t
acfg_set_tx_antenna(uint8_t *radio_name, uint16_t mask);

uint32_t
acfg_set_rx_antenna(uint8_t *radio_name, uint16_t mask);

uint32_t
acfg_set_shpreamble(uint8_t *radio_name, uint16_t shpreamble);

uint32_t
acfg_get_shpreamble(uint8_t *radio_name, uint32_t *shpreamble);

uint32_t
acfg_set_txpower_limit(uint8_t *radio_name, uint32_t band, uint32_t power);

uint32_t
acfg_set_chainmask(uint8_t *radio_name, uint32_t type, uint16_t mask);

uint32_t
acfg_config_radio(uint8_t *radio_name);

uint32_t
acfg_set_preamble(uint8_t  *vap_name, uint32_t preamble);

uint32_t
acfg_set_slot_time(uint8_t  *vap_name, uint32_t slot);

uint32_t
acfg_set_erp(uint8_t  *vap_name, uint32_t erp);

uint32_t
acfg_add_app_ie(uint8_t  *vap_name, const uint8_t *ie, uint32_t ie_len);

uint32_t
acfg_set_chanswitch(uint8_t  *wifi_name, uint8_t  chan_num);

uint32_t
acfg_set_ratemask(uint8_t  *vap_name, uint32_t preamble, uint32_t mask_lower32,
                  uint32_t mask_higher32, uint32_t mask_lower32_2);

uint32_t
acfg_set_shslot(uint8_t *radio_name, uint16_t shslot);

uint32_t
acfg_get_shslot(uint8_t *radio_name, uint32_t *shslot);

uint32_t
acfg_get_rx_antenna(uint8_t *radio_name,  uint32_t *mask);

uint32_t
acfg_get_tx_antenna(uint8_t *radio_name,  uint32_t *mask);

uint32_t
acfg_tx99_tool(uint8_t  *wifi_name, acfg_tx99_data_t* tx99_data);

uint32_t
acfg_set_tx_power(uint8_t  *wifi_name, uint32_t mode, float power);

uint32_t
acfg_set_sens_level(uint8_t  *wifi_name, float sens_level);

uint32_t
acfg_gpio_config(uint8_t  *wifi_name, uint32_t gpio_num, uint32_t input, uint32_t pull_type, uint32_t intr_mode);

uint32_t
acfg_gpio_set(uint8_t  *wifi_name, uint32_t gpio_num, uint32_t set);

uint32_t
acfg_send_raw_pkt(uint8_t  *vap_name, uint8_t *pkt_buf, uint32_t len, uint8_t type, uint16_t chan, uint8_t nss, uint8_t preamble, uint8_t mcs, uint8_t retry, uint8_t power, uint16_t scan_dur);

uint32_t
acfg_send_raw_multi(uint8_t  *vap_name, uint8_t *pkt_buf, uint32_t len, uint8_t type, uint16_t chan, uint8_t nss, uint8_t preamble, uint8_t mcs, uint8_t retry, uint8_t power, uint16_t scan_dur);

#if QCA_SUPPORT_GPR
uint32_t
acfg_start_gpr(uint8_t  *vap_name, uint8_t *pkt_buf, uint32_t len, uint32_t period, uint8_t nss, uint8_t preamble, uint8_t mcs);

uint32_t
acfg_send_gpr_cmd(uint8_t  *vap_name, uint8_t command);
#endif

uint32_t
acfg_enable_amsdu(uint8_t *radio_name, uint16_t amsdutidmask);

uint32_t
acfg_enable_ampdu(uint8_t *radio_name, uint16_t ampdutidmask);

uint32_t
acfg_set_ctl_table(uint8_t  *wifi_name, uint32_t band, uint32_t len, uint8_t  *ctl_table);

/**
 * @brief set basic & supported rates in beacon,
 * and use lowest basic rate as mgmt mgmt/bcast/mcast rates by default.
 * target_rates: an array of supported rates with bit7 set for basic rates.
 * num_of_rates: number of rates in the array
*/
uint32_t
acfg_set_op_support_rates(uint8_t  *radio_name, uint8_t *target_rates, uint32_t num_of_rates);

/**
 * @brief Set management/action frame retry limit
 *
 * @param
 * @radio_name: Physical radio interface
 * @limit:      Management/action frame retry limit
 *
 * @return
 */
uint32_t
acfg_set_mgmt_retry_limit(uint8_t *radio_name, uint8_t limit);

/**
 * @brief Get management/action frame retry limit
 *
 * @param
 * @radio_name: Physical radio interface
 * @limit:      Management/action frame retry limit
 *
 * @return
 */
uint32_t
acfg_get_mgmt_retry_limit(uint8_t *radio_name, uint8_t *limit);

/**
 * @brief get supported rates of the specified phymode for the radio
 * target_rates: return an array of supported rates with bit7 set for basic rates.
 * phymode : phymode
*/
uint32_t
acfg_get_radio_supported_rates(uint8_t  *radio_name, acfg_rateset_t *target_rates, uint32_t phymode);

/**
 * @brief get supported legacy rates from BEACON IE
 * target_rates: return an array of supported rates with bit7 set for basic rates.
*/
uint32_t
acfg_get_beacon_supported_rates(uint8_t  *vap_name, acfg_rateset_t *target_rates);

uint32_t
acfg_send_raw_cancel(uint8_t  *vap_name);

uint32_t
acfg_set_cca_threshold(uint8_t  *wifi_name, float cca_threshold);

uint32_t
acfg_get_nf_dbr_dbm_info(uint8_t  *wifi_name);

uint32_t
acfg_get_packet_power_info(uint8_t  *wifi_name, acfg_packet_power_param_t *param);

uint32_t
acfg_offchan_rx(uint8_t  *vap_name, uint16_t chan, uint16_t scan_dur, char *bw_mode_params);

uint32_t
acfg_handle_wps_event(uint8_t *ifname, enum acfg_event_handler_type event);

uint32_t
acfg_set_bss_color(uint8_t *radio_name, uint32_t bsscolor, uint32_t override);

uint32_t
acfg_get_bss_color(uint8_t *radio_name, uint32_t *bsscolor);

uint32_t
acfg_set_muedca_ecwmin(uint8_t *vap, uint32_t ac, uint32_t value);
uint32_t
acfg_get_muedca_ecwmin(uint8_t *vap, uint32_t ac, uint32_t *value);
uint32_t
acfg_set_muedca_ecwmax(uint8_t *vap, uint32_t ac, uint32_t value);
uint32_t
acfg_get_muedca_ecwmax(uint8_t *vap, uint32_t ac, uint32_t *value);
uint32_t
acfg_set_muedca_aifsn(uint8_t *vap, uint32_t ac, uint32_t value);
uint32_t
acfg_get_muedca_aifsn(uint8_t *vap, uint32_t ac, uint32_t *value);
uint32_t
acfg_set_muedca_acm(uint8_t *vap, uint32_t ac, uint32_t value);
uint32_t
acfg_get_muedca_acm(uint8_t *vap, uint32_t ac, uint32_t *value);
uint32_t
acfg_set_muedca_timer(uint8_t *vap, uint32_t ac, uint32_t value);
uint32_t
acfg_get_muedca_timer(uint8_t *vap, uint32_t ac, uint32_t *value);

#endif
