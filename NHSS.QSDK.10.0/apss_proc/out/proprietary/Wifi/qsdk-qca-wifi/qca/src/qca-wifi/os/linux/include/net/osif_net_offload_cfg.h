/*
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
 *
 * Copyright (c) 2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 */

#ifndef __ATD_CFG_H
#define __ATD_CFG_H

/**
 * @brief   This WLAN Config structure is registered to the
 *          QDF during the device creation. Essentially the
 *          QDF will make calls to the specific functions based
 *          on what it receives from ACFG.
 */
typedef struct atd_cfg_vap {

    /**
     * @brief Delete a VAP
     */
    QDF_STATUS (*delete_vap)(qdf_drv_handle_t  hdl);


    /**
     * @brief Set the SSID for the VAP
     */
    QDF_STATUS (*set_ssid)(qdf_drv_handle_t hdl, acfg_ssid_t  *ssid);

    /**
     * @brief Get the SSID for the given VAP
     */
    QDF_STATUS (*get_ssid)(qdf_drv_handle_t hdl, acfg_ssid_t *ssid);

    /**
     * @brief Set the TESTMODE for the VAP
     */
    QDF_STATUS (*set_testmode)(qdf_drv_handle_t hdl,
                               acfg_testmode_t  *testmode);

    /**
     * @brief Get the TESTMODE for the given VAP
     */
    QDF_STATUS (*get_testmode)(qdf_drv_handle_t hdl, acfg_testmode_t *testmode);

    /**
     * @brief Get the RSSI for the given VAP
     */
    QDF_STATUS (*get_rssi)(qdf_drv_handle_t hdl, acfg_rssi_t *rssi);

    /**
     * @brief Get the CUSTDATA for the given VAP
     */
    QDF_STATUS (*get_custdata)(qdf_drv_handle_t hdl, acfg_custdata_t *custdata);

    /**
     * @brief Set the Channel
     */
    QDF_STATUS (*set_channel)(qdf_drv_handle_t  hdl, uint8_t  ieee_chan);
    /**
     * @brief Get the Channel
     */
    QDF_STATUS (*get_channel)(qdf_drv_handle_t  hdl, uint8_t  *ieee_chan);

    /**
     * @brief Set the PHYMODE as a string (refer acfg_phymode_t)
     */
    QDF_STATUS (*set_phymode)(qdf_drv_handle_t  hdl, acfg_phymode_t  mode);
    /**
     * @brief Get the PHYMODE
     */
    QDF_STATUS (*get_phymode)(qdf_drv_handle_t  hdl, acfg_phymode_t  *mode);

    /**
     * @brief Set the OPMODE (refer acfg_opmode_t)
     */
    QDF_STATUS (*set_opmode)(qdf_drv_handle_t  hdl, acfg_opmode_t  mode);

    /**
     * @brief Get the OPMODE
     */
    QDF_STATUS (*get_opmode)(qdf_drv_handle_t  hdl, acfg_opmode_t  *mode);

    /**
     * @brief Set the channel mode
     */
    QDF_STATUS (*set_chmode)(qdf_drv_handle_t hdl, acfg_chmode_t  *mode);

    /**
     * @brief Get the channel mode
     */
    QDF_STATUS (*get_chmode)(qdf_drv_handle_t hdl, acfg_chmode_t *mode);

    /**
     * @brief Set the AMPDU status
     */
    QDF_STATUS (*set_ampdu)(qdf_drv_handle_t  hdl, acfg_ampdu_t  ampdu);

    /**
     * @brief Get the AMPDU status
     */
    QDF_STATUS (*get_ampdu)(qdf_drv_handle_t  hdl, acfg_ampdu_t *ampdu);

    /**
     * @brief Set the AMPDUFrames
     */
    QDF_STATUS (*set_ampdu_frames)(qdf_drv_handle_t  hdl, \
                                    acfg_ampdu_frames_t frames);

    /**
     * @brief Get the AMPDUFrames
     */
    QDF_STATUS (*get_ampdu_frames)(qdf_drv_handle_t  hdl, \
                                    acfg_ampdu_frames_t *frames);

    /**
     * @brief Set the AMPDU Limit
     */
    QDF_STATUS (*set_ampdu_limit)(qdf_drv_handle_t  hdl, \
                                        acfg_ampdu_limit_t lim);

    /**
     * @brief Get the AMPDU Limit
     */
    QDF_STATUS (*get_ampdu_limit)(qdf_drv_handle_t  hdl, \
                                        acfg_ampdu_limit_t *lim);

    /**
     * @brief Set the Shortgi setting
     */
    QDF_STATUS (*set_shortgi)(qdf_drv_handle_t  hdl, acfg_shortgi_t shortgi);

    /**
     * @brief Set the Shortgi setting
     */
    QDF_STATUS (*get_shortgi)(qdf_drv_handle_t  hdl, acfg_shortgi_t *shortgi);


    /**
     * @brief Set the frequency
     */
    QDF_STATUS (*set_freq)(qdf_drv_handle_t  hdl, acfg_freq_t *freq);

    /**
     * @brief Get the frequency
     */
    QDF_STATUS (*get_freq)(qdf_drv_handle_t  hdl, acfg_freq_t *freq);

    /**
     * @brief Get the wireless name (to verify the presence of
     *        wireless extentions)
     */
    QDF_STATUS (*get_wireless_name)(qdf_drv_handle_t hdl, \
                            u_int8_t *name, u_int32_t maxlen);

    /**
     * @brief Set RTS threshold
     */
    QDF_STATUS (*set_rts)(qdf_drv_handle_t hdl, acfg_rts_t *rts);

    /**
     * @brief Get RTS threshold
     */
    QDF_STATUS (*get_rts)(qdf_drv_handle_t hdl, acfg_rts_t *rts);


    /**
     * @brief Set Fragmentation threshold
     */
    QDF_STATUS (*set_frag)(qdf_drv_handle_t hdl, acfg_frag_t *frag);


    /**
     * @brief Get Fragmentation threshold
     */
    QDF_STATUS (*get_frag)(qdf_drv_handle_t hdl, acfg_frag_t *frag);


    /**
     * @brief Set Tx Power
     */
    QDF_STATUS (*set_txpow)(qdf_drv_handle_t hdl, acfg_txpow_t *txpow);

    /**
     * @brief Get Tx Power
     */
    QDF_STATUS (*get_txpow)(qdf_drv_handle_t hdl, acfg_txpow_t *txpow);


    /**
     * @brief Set AP Mac Address
     */
    QDF_STATUS (*set_ap)(qdf_drv_handle_t hdl, acfg_macaddr_t *mac);


    /**
     * @brief Get AP Mac Address
     */
    QDF_STATUS (*get_ap)(qdf_drv_handle_t hdl, acfg_macaddr_t *mac);


    /**
     * @brief Get range of parameters
     */
    QDF_STATUS (*get_range)(qdf_drv_handle_t hdl, acfg_range_t *range);

    /**
     * @brief Set encoding and token mode
     */
    QDF_STATUS (*set_encode)(qdf_drv_handle_t hdl, acfg_encode_t *encode);

    /**
     * @brief Get encoding and token mode
     */
    QDF_STATUS (*get_encode)(qdf_drv_handle_t hdl, acfg_encode_t *range);
    /**
     * @brief Set value of a param
     *
     * @param
     * @param param - param id
     * @param val   - value to set
     */
    QDF_STATUS (*set_vap_param)(qdf_drv_handle_t hdl, uint32_t param,
                                uint32_t val);

    /**
     * @brief Get the value of a param
     *
     * @param
     * @param param - param id
     * @param val   - pointer to location to return value
     */
    QDF_STATUS (*get_vap_param)(qdf_drv_handle_t hdl, uint32_t param,
                                uint32_t *val);

    /**
     * @brief Set value of a vendor param
     *
     * @param
     * @param val   - pointer to vendor param req
     */
    QDF_STATUS (*set_vap_vendor_param)(qdf_drv_handle_t hdl, acfg_vendor_param_req_t *data);

    /**
     * @brief Get value of a vendor param
     *
     * @param
     * @param val   - pointer to vendor param req
     */
    QDF_STATUS (*get_vap_vendor_param)(qdf_drv_handle_t hdl, acfg_vendor_param_req_t *data);

    /**
     * @brief Set default bit rate
     *
     * @param
     * @param rate
     */
    QDF_STATUS (*set_rate)(qdf_drv_handle_t hdl, acfg_rate_t *rate);

    /**
     * @brief Get default bit rate
     *
     * @param
     * @param rate
     */
    QDF_STATUS (*get_rate)(qdf_drv_handle_t hdl, uint32_t *rate);

    /**
     * @brief Set power management
     *
     * @param
     * @param powmgmt
     */
    QDF_STATUS (*set_powmgmt)(qdf_drv_handle_t hdl, acfg_powmgmt_t *pm);

    /**
     * @brief Get power management
     *
     * @param
     * @param powmgmt
     */
    QDF_STATUS (*get_powmgmt)(qdf_drv_handle_t hdl, acfg_powmgmt_t *pm);

    /**
     * @brief Set the Scan Request for the VAP
     */
    QDF_STATUS (*set_scan)(qdf_drv_handle_t hdl, acfg_set_scan_t  *scan);

    /**
     * @brief Get the Scan Results for the given VAP
     */
    QDF_STATUS (*get_scan_results)(qdf_drv_handle_t hdl, acfg_scan_t *scan);

    /**
     * @brief Get Wireless statistics
     *
     * @param
     * @param stat
     */
    QDF_STATUS (*get_stats)(qdf_drv_handle_t hdl, acfg_stats_t *stat);


    /**
     * @brief Get info on associated stations
     *
     * @param
     * @param buff
     */
    QDF_STATUS (*get_sta_info)(qdf_drv_handle_t hdl, \
                            uint8_t *buff, uint32_t buflen);

    /**
     * @brief Init security service
     */
    QDF_STATUS (*wsupp_init)(qdf_drv_handle_t hdl, acfg_wsupp_info_t *winfo);

    /**
     * @brief Fini security service
     */
    QDF_STATUS (*wsupp_fini)(qdf_drv_handle_t hdl, acfg_wsupp_info_t *winfo);

    /**
     * @brief Supplicant bridge add interface
     */
    QDF_STATUS (*wsupp_if_add)(qdf_drv_handle_t hdl, acfg_wsupp_info_t *winfo);

    /**
     * @brief Supplicant bridge remove interface
     */
    QDF_STATUS (*wsupp_if_remove)(qdf_drv_handle_t hdl,
                                  acfg_wsupp_info_t *winfo);

    /**
     * @brief Supplicant bridge create netwrok
     */
    QDF_STATUS (*wsupp_nw_create)(qdf_drv_handle_t hdl,
                                  acfg_wsupp_info_t *winfo);

    /**
     * @brief Supplicant bridge delete netwrok
     */
    QDF_STATUS (*wsupp_nw_delete)(qdf_drv_handle_t hdl,
                                  acfg_wsupp_info_t *winfo);

    /**
     * @brief Supplicant bridge netwrok setting
     */
    QDF_STATUS (*wsupp_nw_set)(qdf_drv_handle_t hdl, acfg_wsupp_info_t *winfo);

    /**
     * @brief Supplicant bridge netwrok get attribute setting
     */
    QDF_STATUS (*wsupp_nw_get)(qdf_drv_handle_t hdl, acfg_wsupp_info_t *winfo);

    /**
     * @brief Supplicant bridge netwrok list
     */
    QDF_STATUS (*wsupp_nw_list)(qdf_drv_handle_t hdl, acfg_wsupp_info_t *winfo);

    /**
     * @brief Supplicant bridge WPS request
     */
    QDF_STATUS (*wsupp_wps_req)(qdf_drv_handle_t hdl, acfg_wsupp_info_t *winfo);

    /**
     * @brief Supplicant bridge setting at run time
     */
    QDF_STATUS (*wsupp_set)(qdf_drv_handle_t hdl, acfg_wsupp_info_t *winfo);

    /**
     * @brief Set value of a wmm param
     *
     * @param
     * @param param - param array
     * @param val   - value to set
     */
    QDF_STATUS (*set_wmmparams)(qdf_drv_handle_t hdl, \
                        uint32_t *param, uint32_t val);

    /**
     * @brief Get the value of a wmm param
     *
     * @param
     * @param param - param array
     * @param val   - pointer to location to return value
     */
    QDF_STATUS (*get_wmmparams)(qdf_drv_handle_t hdl,
                        uint32_t *param, uint32_t *val);

    /**
     * @brief config_nawds
     *
     * @param
     * @param nawds_conf
     */
    QDF_STATUS (*config_nawds)(qdf_drv_handle_t hdl,
            acfg_nawds_cfg_t *nawds_conf);

    /**
     * @brief Channel switch
     */
    QDF_STATUS (*doth_chsw)(qdf_drv_handle_t hdl, acfg_doth_chsw_t *chsw);

    /**
     * @brief Add Mac address to ACL list
     *
     * @param
     * @param chsw
     */
    QDF_STATUS (*addmac)(qdf_drv_handle_t hdl, acfg_macaddr_t *chsw);


    /**
     * @brief Delete Mac address from ACL list
     *
     * @param
     * @param chsw
     */
    QDF_STATUS (*delmac)(qdf_drv_handle_t hdl, acfg_macaddr_t *chsw);

    /**
     * @brief Disassociate station
     *
     * @param
     * @param chsw
     */
    QDF_STATUS (*kickmac)(qdf_drv_handle_t hdl, acfg_macaddr_t *chsw);

    /**
     * @brief Set MLME state machine
     *
     * @param
     * @param chsw
     */
    QDF_STATUS (*set_mlme)(qdf_drv_handle_t hdl, acfg_mlme_t *mlme);


    /**
     * @brief Set Optional IE
     *
     * @param hdl
     * @param ie
     */
    QDF_STATUS (*set_optie)(qdf_drv_handle_t hdl, acfg_ie_t *ie);

    /**
     * @brief Get info on associated stations
     *
     * @param
     * @param buff
     */
    QDF_STATUS (*get_wpa_ie)(qdf_drv_handle_t hdl, \
                            uint8_t *buff, uint32_t buflen);

    /**
     * @brief Set AC params
     *
     * @param
     * @param chsw
     */
    QDF_STATUS (*set_acparams)(qdf_drv_handle_t hdl, uint32_t  *ac);


    /**
     * @brief Set Filterframe state machine
     *
     * @param
     * @param chsw
     */
    QDF_STATUS (*set_filterframe)(qdf_drv_handle_t hdl,
                                  acfg_filter_t  *fltrframe);

    /**
     * @brief Set AppieBuf
     *
     * @param
     * @param chsw
     */
    QDF_STATUS (*set_appiebuf)(qdf_drv_handle_t hdl, acfg_appie_t *appiebuf);

    /**
     * @brief
     *
     * @param
     * @param setkey
     */
    QDF_STATUS (*set_key)(qdf_drv_handle_t hdl, acfg_key_t *setkey);

    /**
     * @brief Del Key
     *
     * @param
     * @param chsw
     */
    QDF_STATUS (*del_key)(qdf_drv_handle_t hdl, acfg_delkey_t *delkey);

    /**
     * @brief Get info on key
     *
     * @param
     * @param buff
     */
    QDF_STATUS (*get_key)(qdf_drv_handle_t hdl, \
                            uint8_t *buff, uint32_t buflen);

    /**
     * @brief Get STA Stats
     *
     * @param
     * @param buff
     */
    QDF_STATUS (*get_sta_stats)(qdf_drv_handle_t hdl, \
                            uint8_t *buff, uint32_t buflen);

    /**
     * @brief Channel Info
     *
     * @param
     * @param channel info
     */
    QDF_STATUS (*get_chan_info)(qdf_drv_handle_t hdl,
                                acfg_chan_info_t *chan_info);

    /**
     * @brief Channel List
     *
     * @param
     * @param channel list
     */
    QDF_STATUS (*get_chan_list)(qdf_drv_handle_t hdl, acfg_opaque_t *chan_list);

    /**
     * @brief ACL MAC Address List
     *
     * @param
     * @param mac address list
     */
    QDF_STATUS (*get_mac_address)(qdf_drv_handle_t hdl,
                                  acfg_macacl_t *mac_addr_list);

    /**
     * @brief Get P2P Param
     *
     * @param hdl
     * @param p2p param
     */
    QDF_STATUS (*get_p2p_param)(qdf_drv_handle_t hdl,
                                acfg_p2p_param_t *p2p_param);

    /**
     * @brief Set P2P Param
     *
     * @param hdl
     * @param p2p param
     */
    QDF_STATUS (*set_p2p_param)(qdf_drv_handle_t hdl,
                                acfg_p2p_param_t *p2p_param);
	QDF_STATUS (*acl_setmac)(qdf_drv_handle_t hdl, acfg_macaddr_t *mac,
								uint8_t add);

    /**
    * @brief Send dbgreq frame
    *
    * @param
    * @param dbgreq frame
    */
    QDF_STATUS (*dbgreq)(qdf_drv_handle_t hdl, acfg_athdbg_req_t  *dbgreq);

   /**
     * @brief Send Mgmt Frame
     *
     * @param
     * @param chsw
     */
    QDF_STATUS (*send_mgmt)(qdf_drv_handle_t hdl,
                            acfg_mgmt_t  *sendmgmtframe);

    QDF_STATUS (*set_chn_widthswitch)(qdf_drv_handle_t hdl, acfg_set_chn_width_t* chnw);

    QDF_STATUS (*mon_addmac)(qdf_drv_handle_t hdl, acfg_macaddr_t *mac,
                                uint8_t add);
    QDF_STATUS (*mon_delmac)(qdf_drv_handle_t hdl, acfg_macaddr_t *mac,
                                uint8_t add);
    QDF_STATUS (*set_atf_ssid)(qdf_drv_handle_t hdl, acfg_atf_ssid_val_t* atf_ssid);

    QDF_STATUS (*set_atf_sta)(qdf_drv_handle_t hdl, acfg_atf_sta_val_t* atf_sta);

} atd_cfg_vap_t;



typedef struct atd_cfg_wifi {

    /**
     * @brief Create a VAP for the Wifi provided
     */
    QDF_STATUS (*create_vap)(qdf_drv_handle_t      hdl,
                             uint8_t       icp_name[ACFG_MAX_IFNAME],
                             acfg_opmode_t         icp_opmode,
                             int32_t             icp_vapid,
                             acfg_vapinfo_flags_t  icp_vapflags);
    /**
     * @brief Set value of a param
     *
     * @param
     * @param param - param id
     * @param val   - value to set
     */
    QDF_STATUS (*set_radio_param)(qdf_drv_handle_t hdl, uint32_t param,
                                  uint32_t val);

    /**
     * @brief Get the value of a param
     *
     * @param
     * @param param - param id
     * @param val   - pointer to location to return value
     */
    QDF_STATUS (*get_radio_param)(qdf_drv_handle_t hdl, uint32_t param,
                                  uint32_t *val);
    /**
     * @brief set value to the register indicated by offset
     */
    QDF_STATUS (*set_reg)(qdf_drv_handle_t hdl, uint32_t offset,
                          uint32_t value);

    /**
     * @brief get value from the register indicated by offset
     */
    QDF_STATUS (*get_reg)(qdf_drv_handle_t hdl, uint32_t offset,
                          uint32_t *value);

    /**
     * @brief tx99tool
     *
     * @param
     * @param tx99_wcmd
     */
    QDF_STATUS (*tx99tool)(qdf_drv_handle_t hdl, acfg_tx99_t *tx99_wcmd);

    /**
     * @brief set hw(wifi0) address
     */
    QDF_STATUS (*set_hwaddr)(qdf_drv_handle_t hdl, acfg_macaddr_t *mac);

    /**
     * @brief Get wireless interface stats (ath_stats) for given interface
     */
    QDF_STATUS (*get_ath_stats)(qdf_drv_handle_t hdl,
                                acfg_ath_stats_t *ath_stats);


    /**
     * @brief Clear wireless interface stats (ath_stats) for given interface
     */
    QDF_STATUS (*clr_ath_stats)(qdf_drv_handle_t hdl,
                                acfg_ath_stats_t *ath_stats);

    /**
     * @brief Get radio profile (complete info) for the specified radio
     */
	QDF_STATUS (*get_profile)(qdf_drv_handle_t hdl,
                              acfg_radio_vap_info_t *profile);

    /**
     * @brief For radartool
     */
    QDF_STATUS (*phyerr)(qdf_drv_handle_t hdl, acfg_ath_diag_t *ath_diag);

    QDF_STATUS (*set_country)(qdf_drv_handle_t hdl, acfg_set_country_t* setcountry);

} atd_cfg_wifi_t;
#endif

