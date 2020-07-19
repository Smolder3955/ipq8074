/*
* Copyright (c) 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

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
 */

#ifndef __ATD_CFG_H
#define __ATD_CFG_H

#include <acfg_drv_if.h>

/** 
 * @brief   This WLAN Config structure is registered to the 
 *          ADF during the device creation. Essentially the 
 *          ADF will make calls to the specific functions based
 *          on what it receives from ACFG.
 */
typedef struct atd_cfg_vap {

    /** 
     * @brief Delete a VAP 
     */
    a_status_t (*delete_vap)(adf_drv_handle_t  hdl);


    /** 
     * @brief Set the SSID for the VAP
     */
    a_status_t (*set_ssid)(adf_drv_handle_t hdl, acfg_ssid_t  *ssid);

    /** 
     * @brief Get the SSID for the given VAP
     */
    a_status_t (*get_ssid)(adf_drv_handle_t hdl, acfg_ssid_t *ssid);
    
    /** 
     * @brief Set the TESTMODE for the VAP
     */
    a_status_t (*set_testmode)(adf_drv_handle_t hdl, 
                               acfg_testmode_t  *testmode);

    /** 
     * @brief Get the TESTMODE for the given VAP
     */
    a_status_t (*get_testmode)(adf_drv_handle_t hdl, acfg_testmode_t *testmode);
    
    /** 
     * @brief Get the RSSI for the given VAP
     */
    a_status_t (*get_rssi)(adf_drv_handle_t hdl, acfg_rssi_t *rssi);

    /** 
     * @brief Get the CUSTDATA for the given VAP
     */
    a_status_t (*get_custdata)(adf_drv_handle_t hdl, acfg_custdata_t *custdata);

    /** 
     * @brief Set the Channel
     */
    a_status_t (*set_channel)(adf_drv_handle_t  hdl, a_uint8_t  ieee_chan);
    /** 
     * @brief Get the Channel
     */
    a_status_t (*get_channel)(adf_drv_handle_t  hdl, a_uint8_t  *ieee_chan);

    /** 
     * @brief Set the PHYMODE as a string (refer acfg_phymode_t)
     */
    a_status_t (*set_phymode)(adf_drv_handle_t  hdl, acfg_phymode_t  mode);
    /** 
     * @brief Get the PHYMODE
     */
    a_status_t (*get_phymode)(adf_drv_handle_t  hdl, acfg_phymode_t  *mode);
 
    /** 
     * @brief Set the OPMODE (refer acfg_opmode_t)
     */
    a_status_t (*set_opmode)(adf_drv_handle_t  hdl, acfg_opmode_t  mode);

    /** 
     * @brief Get the OPMODE
     */
    a_status_t (*get_opmode)(adf_drv_handle_t  hdl, acfg_opmode_t  *mode);

    /** 
     * @brief Set the channel mode
     */
    a_status_t (*set_chmode)(adf_drv_handle_t hdl, acfg_chmode_t  *mode);

    /** 
     * @brief Get the channel mode
     */
    a_status_t (*get_chmode)(adf_drv_handle_t hdl, acfg_chmode_t *mode);

    /** 
     * @brief Set the AMPDU status 
     */
    a_status_t (*set_ampdu)(adf_drv_handle_t  hdl, acfg_ampdu_t  ampdu);

    /** 
     * @brief Get the AMPDU status 
     */
    a_status_t (*get_ampdu)(adf_drv_handle_t  hdl, acfg_ampdu_t *ampdu);

    /** 
     * @brief Set the AMPDUFrames
     */
    a_status_t (*set_ampdu_frames)(adf_drv_handle_t  hdl, \
                                    acfg_ampdu_frames_t frames);

    /** 
     * @brief Get the AMPDUFrames
     */
    a_status_t (*get_ampdu_frames)(adf_drv_handle_t  hdl, \
                                    acfg_ampdu_frames_t *frames);

    /** 
     * @brief Set the AMPDU Limit
     */
    a_status_t (*set_ampdu_limit)(adf_drv_handle_t  hdl, \
                                        acfg_ampdu_limit_t lim);

    /** 
     * @brief Get the AMPDU Limit
     */
    a_status_t (*get_ampdu_limit)(adf_drv_handle_t  hdl, \
                                        acfg_ampdu_limit_t *lim);

    /** 
     * @brief Set the Shortgi setting
     */
    a_status_t (*set_shortgi)(adf_drv_handle_t  hdl, acfg_shortgi_t shortgi);

    /** 
     * @brief Set the Shortgi setting
     */
    a_status_t (*get_shortgi)(adf_drv_handle_t  hdl, acfg_shortgi_t *shortgi);


    /** 
     * @brief Set the frequency
     */
    a_status_t (*set_freq)(adf_drv_handle_t  hdl, acfg_freq_t *freq);
    
    /** 
     * @brief Get the frequency
     */
    a_status_t (*get_freq)(adf_drv_handle_t  hdl, acfg_freq_t *freq);
    
    /** 
     * @brief Get the wireless name (to verify the presence of 
     *        wireless extentions)
     */
    a_status_t (*get_wireless_name)(adf_drv_handle_t hdl, \
                            u_int8_t *name, u_int32_t maxlen);

    /** 
     * @brief Set RTS threshold
     */
    a_status_t (*set_rts)(adf_drv_handle_t hdl, acfg_rts_t *rts);

    /** 
     * @brief Get RTS threshold
     */
    a_status_t (*get_rts)(adf_drv_handle_t hdl, acfg_rts_t *rts);


    /** 
     * @brief Set Fragmentation threshold
     */
    a_status_t (*set_frag)(adf_drv_handle_t hdl, acfg_frag_t *frag);


    /** 
     * @brief Get Fragmentation threshold 
     */
    a_status_t (*get_frag)(adf_drv_handle_t hdl, acfg_frag_t *frag);


    /** 
     * @brief Set Tx Power
     */
    a_status_t (*set_txpow)(adf_drv_handle_t hdl, acfg_txpow_t *txpow);
    
    /** 
     * @brief Get Tx Power
     */
    a_status_t (*get_txpow)(adf_drv_handle_t hdl, acfg_txpow_t *txpow);
    
    
    /** 
     * @brief Set AP Mac Address
     */
    a_status_t (*set_ap)(adf_drv_handle_t hdl, acfg_macaddr_t *mac);
    

    /** 
     * @brief Get AP Mac Address
     */
    a_status_t (*get_ap)(adf_drv_handle_t hdl, acfg_macaddr_t *mac);
    

    /** 
     * @brief Get range of parameters
     */
    a_status_t (*get_range)(adf_drv_handle_t hdl, acfg_range_t *range);

    /** 
     * @brief Set encoding and token mode
     */
    a_status_t (*set_encode)(adf_drv_handle_t hdl, acfg_encode_t *encode);
	
    /** 
     * @brief Get encoding and token mode
     */
    a_status_t (*get_encode)(adf_drv_handle_t hdl, acfg_encode_t *range);
    /** 
     * @brief Set value of a param
     * 
     * @param 
     * @param param - param id
     * @param val   - value to set
     */
    a_status_t (*set_vap_param)(adf_drv_handle_t hdl, a_uint32_t param, 
                                a_uint32_t val);
    
    /** 
     * @brief Get the value of a param
     * 
     * @param 
     * @param param - param id
     * @param val   - pointer to location to return value
     */
    a_status_t (*get_vap_param)(adf_drv_handle_t hdl, a_uint32_t param, 
                                a_uint32_t *val);

    /**
     * @brief Set value of a vendor param
     *
     * @param
     * @param val   - pointer to vendor param req
     */
    a_status_t (*set_vap_vendor_param)(adf_drv_handle_t hdl, acfg_vendor_param_req_t *data);

    /**
     * @brief Get value of a vendor param
     *
     * @param
     * @param val   - pointer to vendor param req
     */
    a_status_t (*get_vap_vendor_param)(adf_drv_handle_t hdl, acfg_vendor_param_req_t *data);

    /** 
     * @brief Set default bit rate
     * 
     * @param 
     * @param rate
     */
    a_status_t (*set_rate)(adf_drv_handle_t hdl, acfg_rate_t *rate);
	
    /** 
     * @brief Get default bit rate
     * 
     * @param 
     * @param rate
     */
    a_status_t (*get_rate)(adf_drv_handle_t hdl, a_uint32_t *rate);
	
    /** 
     * @brief Set power management
     * 
     * @param 
     * @param powmgmt
     */
    a_status_t (*set_powmgmt)(adf_drv_handle_t hdl, acfg_powmgmt_t *pm);

    /** 
     * @brief Get power management
     * 
     * @param 
     * @param powmgmt
     */
    a_status_t (*get_powmgmt)(adf_drv_handle_t hdl, acfg_powmgmt_t *pm);

    /** 
     * @brief Set the Scan Request for the VAP
     */
    a_status_t (*set_scan)(adf_drv_handle_t hdl, acfg_set_scan_t  *scan);

    /** 
     * @brief Get the Scan Results for the given VAP
     */
    a_status_t (*get_scan_results)(adf_drv_handle_t hdl, acfg_scan_t *scan);

    /** 
     * @brief Get Wireless statistics
     * 
     * @param 
     * @param stat
     */
    a_status_t (*get_stats)(adf_drv_handle_t hdl, acfg_stats_t *stat);
    
    
    /** 
     * @brief Get info on associated stations
     * 
     * @param 
     * @param buff
     */
    a_status_t (*get_sta_info)(adf_drv_handle_t hdl, \
                            a_uint8_t *buff, a_uint32_t buflen);

    /** 
     * @brief Init security service
     */
    a_status_t (*wsupp_init)(adf_drv_handle_t hdl, acfg_wsupp_info_t *winfo);

    /** 
     * @brief Fini security service
     */
    a_status_t (*wsupp_fini)(adf_drv_handle_t hdl, acfg_wsupp_info_t *winfo);

    /** 
     * @brief Supplicant bridge add interface
     */
    a_status_t (*wsupp_if_add)(adf_drv_handle_t hdl, acfg_wsupp_info_t *winfo);

    /** 
     * @brief Supplicant bridge remove interface
     */
    a_status_t (*wsupp_if_remove)(adf_drv_handle_t hdl, 
                                  acfg_wsupp_info_t *winfo);

    /** 
     * @brief Supplicant bridge create netwrok
     */
    a_status_t (*wsupp_nw_create)(adf_drv_handle_t hdl, 
                                  acfg_wsupp_info_t *winfo);

    /** 
     * @brief Supplicant bridge delete netwrok
     */
    a_status_t (*wsupp_nw_delete)(adf_drv_handle_t hdl, 
                                  acfg_wsupp_info_t *winfo);

    /** 
     * @brief Supplicant bridge netwrok setting
     */
    a_status_t (*wsupp_nw_set)(adf_drv_handle_t hdl, acfg_wsupp_info_t *winfo);

    /** 
     * @brief Supplicant bridge netwrok get attribute setting
     */
    a_status_t (*wsupp_nw_get)(adf_drv_handle_t hdl, acfg_wsupp_info_t *winfo);

    /** 
     * @brief Supplicant bridge netwrok list
     */
    a_status_t (*wsupp_nw_list)(adf_drv_handle_t hdl, acfg_wsupp_info_t *winfo);

    /** 
     * @brief Supplicant bridge WPS request
     */
    a_status_t (*wsupp_wps_req)(adf_drv_handle_t hdl, acfg_wsupp_info_t *winfo);

    /** 
     * @brief Supplicant bridge setting at run time
     */
    a_status_t (*wsupp_set)(adf_drv_handle_t hdl, acfg_wsupp_info_t *winfo);

    /** 
     * @brief Set value of a wmm param
     * 
     * @param 
     * @param param - param array
     * @param val   - value to set
     */
    a_status_t (*set_wmmparams)(adf_drv_handle_t hdl, \
                        a_uint32_t *param, a_uint32_t val);	
						
    /** 
     * @brief Get the value of a wmm param
     * 
     * @param 
     * @param param - param array
     * @param val   - pointer to location to return value
     */
    a_status_t (*get_wmmparams)(adf_drv_handle_t hdl, 
                        a_uint32_t *param, a_uint32_t *val);
	
    /** 
     * @brief config_nawds
     * 
     * @param 
     * @param nawds_conf
     */
    a_status_t (*config_nawds)(adf_drv_handle_t hdl, 
            acfg_nawds_cfg_t *nawds_conf);

    /** 
     * @brief Channel switch
     */
    a_status_t (*doth_chsw)(adf_drv_handle_t hdl, acfg_doth_chsw_t *chsw);
    
    /** 
     * @brief Add Mac address to ACL list
     * 
     * @param 
     * @param chsw
     */
    a_status_t (*addmac)(adf_drv_handle_t hdl, acfg_macaddr_t *chsw);


    /** 
     * @brief Delete Mac address from ACL list
     * 
     * @param 
     * @param chsw
     */
    a_status_t (*delmac)(adf_drv_handle_t hdl, acfg_macaddr_t *chsw);
 
    /** 
     * @brief Disassociate station
     * 
     * @param 
     * @param chsw
     */   
    a_status_t (*kickmac)(adf_drv_handle_t hdl, acfg_macaddr_t *chsw);

    /** 
     * @brief Set MLME state machine
     * 
     * @param 
     * @param chsw
     */
    a_status_t (*set_mlme)(adf_drv_handle_t hdl, acfg_mlme_t *mlme);

 
    /** 
     * @brief Set Optional IE
     * 
     * @param hdl
     * @param ie
     */   
    a_status_t (*set_optie)(adf_drv_handle_t hdl, acfg_ie_t *ie);

    /** 
     * @brief Get info on associated stations
     * 
     * @param 
     * @param buff
     */
    a_status_t (*get_wpa_ie)(adf_drv_handle_t hdl, \
                            a_uint8_t *buff, a_uint32_t buflen);

    /** 
     * @brief Set AC params
     * 
     * @param 
     * @param chsw
     */
    a_status_t (*set_acparams)(adf_drv_handle_t hdl, a_uint32_t  *ac);


    /** 
     * @brief Set Filterframe state machine
     * 
     * @param 
     * @param chsw
     */
    a_status_t (*set_filterframe)(adf_drv_handle_t hdl, 
                                  acfg_filter_t  *fltrframe);

    /** 
     * @brief Set AppieBuf 
     * 
     * @param 
     * @param chsw
     */
    a_status_t (*set_appiebuf)(adf_drv_handle_t hdl, acfg_appie_t *appiebuf);

    /** 
     * @brief 
     * 
     * @param 
     * @param setkey
     */
    a_status_t (*set_key)(adf_drv_handle_t hdl, acfg_key_t *setkey);

    /** 
     * @brief Del Key
     * 
     * @param 
     * @param chsw
     */
    a_status_t (*del_key)(adf_drv_handle_t hdl, acfg_delkey_t *delkey);

    /** 
     * @brief Get info on key
     * 
     * @param 
     * @param buff
     */
    a_status_t (*get_key)(adf_drv_handle_t hdl, \
                            a_uint8_t *buff, a_uint32_t buflen);

    /** 
     * @brief Get STA Stats
     * 
     * @param 
     * @param buff
     */
    a_status_t (*get_sta_stats)(adf_drv_handle_t hdl, \
                            a_uint8_t *buff, a_uint32_t buflen);

    /** 
     * @brief Channel Info
     * 
     * @param 
     * @param channel info
     */
    a_status_t (*get_chan_info)(adf_drv_handle_t hdl, 
                                acfg_chan_info_t *chan_info);

    /** 
     * @brief Channel List
     * 
     * @param 
     * @param channel list
     */
    a_status_t (*get_chan_list)(adf_drv_handle_t hdl, acfg_opaque_t *chan_list);

    /** 
     * @brief ACL MAC Address List
     * 
     * @param 
     * @param mac address list
     */
    a_status_t (*get_mac_address)(adf_drv_handle_t hdl, 
                                  acfg_macacl_t *mac_addr_list);

    /** 
     * @brief Get P2P Param
     * 
     * @param hdl
     * @param p2p param 
     */   
    a_status_t (*get_p2p_param)(adf_drv_handle_t hdl, 
                                acfg_p2p_param_t *p2p_param);

    /** 
     * @brief Set P2P Param
     * 
     * @param hdl
     * @param p2p param
     */   
    a_status_t (*set_p2p_param)(adf_drv_handle_t hdl, 
                                acfg_p2p_param_t *p2p_param);

    /**
     * @brief Set mac address for primary ACL
     *
     * @param hdl
     * @param mac - mac address
     * @param add - '1' to addmac/ '0' to delmac
     */
    a_status_t (*acl_setmac)(adf_drv_handle_t hdl, acfg_macaddr_t *mac,
                                                                a_uint8_t add);

    /**
     * @brief Set mac address for secondary ACL
     *
     * @param hdl
     * @param mac - mac address
     * @param add - '1' to addmac/ '0' to delmac
     */
    a_status_t (*acl_setmac_sec)(adf_drv_handle_t hdl, acfg_macaddr_t *mac,
                                                                a_uint8_t add);

    /**
    * @brief Send dbgreq frame
    *
    * @param
    * @param dbgreq frame
    */
    a_status_t (*dbgreq)(adf_drv_handle_t hdl, acfg_athdbg_req_t  *dbgreq);

   /**
     * @brief Send Mgmt Frame
     *
     * @param
     * @param chsw
     */
    a_status_t (*send_mgmt)(adf_drv_handle_t hdl, 
                            acfg_mgmt_t  *sendmgmtframe);

    a_status_t (*set_chn_widthswitch)(adf_drv_handle_t hdl, acfg_set_chn_width_t* chnw);
    
    a_status_t (*mon_addmac)(adf_drv_handle_t hdl, acfg_macaddr_t *mac, 
                                a_uint8_t add);
    a_status_t (*mon_delmac)(adf_drv_handle_t hdl, acfg_macaddr_t *mac,
                                a_uint8_t add);                           
    a_status_t (*set_atf_ssid)(adf_drv_handle_t hdl, acfg_atf_ssid_val_t* atf_ssid);

    a_status_t (*set_atf_sta)(adf_drv_handle_t hdl, acfg_atf_sta_val_t* atf_sta);

} atd_cfg_vap_t;



typedef struct atd_cfg_wifi {

    /** 
     * @brief Create a VAP for the Wifi provided
     */
    a_status_t (*create_vap)(adf_drv_handle_t      hdl,
                             a_uint8_t       icp_name[ACFG_MAX_IFNAME],
                             acfg_opmode_t         icp_opmode, 
                             a_int32_t             icp_vapid, 
                             acfg_vapinfo_flags_t  icp_vapflags);
    /** 
     * @brief Set value of a param
     * 
     * @param 
     * @param param - param id
     * @param val   - value to set
     */
    a_status_t (*set_radio_param)(adf_drv_handle_t hdl, a_uint32_t param, 
                                  a_uint32_t val);
    
    /** 
     * @brief Get the value of a param
     * 
     * @param 
     * @param param - param id
     * @param val   - pointer to location to return value
     */
    a_status_t (*get_radio_param)(adf_drv_handle_t hdl, a_uint32_t param, 
                                  a_uint32_t *val);
    /**
     * @brief set value to the register indicated by offset
     */
    a_status_t (*set_reg)(adf_drv_handle_t hdl, a_uint32_t offset, 
                          a_uint32_t value);

    /**
     * @brief get value from the register indicated by offset
     */
    a_status_t (*get_reg)(adf_drv_handle_t hdl, a_uint32_t offset, 
                          a_uint32_t *value);

    /** 
     * @brief tx99tool
     * 
     * @param 
     * @param tx99_wcmd
     */
    a_status_t (*tx99tool)(adf_drv_handle_t hdl, acfg_tx99_t *tx99_wcmd);

    /**
     * @brief set hw(wifi0) address 
     */
    a_status_t (*set_hwaddr)(adf_drv_handle_t hdl, acfg_macaddr_t *mac);

    /** 
     * @brief Get wireless interface stats (ath_stats) for given interface
     */
    a_status_t (*get_ath_stats)(adf_drv_handle_t hdl, 
                                acfg_ath_stats_t *ath_stats);
    
    
    /** 
     * @brief Clear wireless interface stats (ath_stats) for given interface
     */
    a_status_t (*clr_ath_stats)(adf_drv_handle_t hdl, 
                                acfg_ath_stats_t *ath_stats);
    
    /** 
     * @brief Get radio profile (complete info) for the specified radio
     */
	a_status_t (*get_profile)(adf_drv_handle_t hdl, 
                              acfg_radio_vap_info_t *profile);

    /** 
     * @brief For radartool
     */
    a_status_t (*phyerr)(adf_drv_handle_t hdl, acfg_ath_diag_t *ath_diag);

    a_status_t (*set_country)(adf_drv_handle_t hdl, acfg_set_country_t* setcountry);

} atd_cfg_wifi_t;
#endif

