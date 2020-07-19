/*
 * Copyright (c) 2015-2016, 2018-2019 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * 2015-2016 Qualcomm Atheros, Inc.
 *
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
*/

#include<stdio.h>
#include<string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/types.h>
#include <netinet/in.h>
#include <linux/if.h>
#include <linux/wireless.h>

#include <stdlib.h>
#include <ctype.h>
#include <getopt.h>

#include <acfg_types.h>
#include <acfg_api.h>
#include <acfg_misc.h>
#include <acfg_tool.h>
#include <acfg_config_file.h>
#include <acfg_api_pvt.h>
#ifndef BUILD_X86
#include <ieee80211_external_config.h>
#endif

/**
 * @brief Strings describing type of parameter
 */
char *type_desc [] = {
    [PARAM_UINT8]   = "uint8",
    [PARAM_UINT16]  = "uint16",
    [PARAM_UINT32]  = "uint32",
    [PARAM_SINT8]   = "sint8",
    [PARAM_SINT16]  = "sint16",
    [PARAM_SINT32]  = "sint32",
    [PARAM_STRING]  = "string",
};


/* -------------------------------
 *   Wrapper function prototypes
 * -------------------------------
 */
#define wrap_proto(name)    int wrap_##name(char *params[])

wrap_proto(create_vap);
wrap_proto(delete_vap);
wrap_proto(set_ssid);
wrap_proto(get_ssid);
wrap_proto(set_testmode);
wrap_proto(get_testmode);
wrap_proto(get_rssi);
wrap_proto(get_custdata);
wrap_proto(set_channel);
wrap_proto(get_channel);
wrap_proto(set_opmode);
wrap_proto(get_opmode);
wrap_proto(set_freq);
wrap_proto(get_freq);
wrap_proto(get_rts);
wrap_proto(get_frag);
wrap_proto(get_txpow);
wrap_proto(get_ap);
wrap_proto(set_enc);
wrap_proto(set_vap_vendor_param);
wrap_proto(set_vap_param);
wrap_proto(get_vap_param);
wrap_proto(set_radio_param);
wrap_proto(get_radio_param);
wrap_proto(get_rate);
wrap_proto(set_phymode);
wrap_proto(wlan_profile_create);
wrap_proto(wlan_profile_create_from_file);
wrap_proto(wlan_profile_get);
wrap_proto(is_offload_vap);

/*----------------------------------------------------
  Wrapper Functions
  -----------------------------------------------------
 */

param_info_t wrap_set_profile_params[] = {
    {"radio", PARAM_STRING, "radio name"},
    {"file", PARAM_STRING, "file name"},
};

int wrap_set_profile(char *params[])
{
    int status = QDF_STATUS_SUCCESS;
    acfg_wlan_profile_t *new_profile;
    int i = 0;

    /* Get New Profile */
    new_profile = acfg_get_profile(params[0]);
    if( NULL == new_profile )
        return QDF_STATUS_E_INVAL;
    /* Read New profile from user & populate new_profile */
    status = acfg_read_file(params[1], new_profile);
    if(status < 0 ) {
        printf("New profile could not be read \n\r");
        /* Free cur_profile & new_profile */
        acfg_free_profile(new_profile);
        return QDF_STATUS_E_FAILURE;
    }
    for (i = 0; i < new_profile->num_vaps; i++) {
        strlcpy((char *)new_profile->vap_params[i].radio_name,
                (char *)new_profile->radio_params.radio_name, sizeof(new_profile->vap_params[i].radio_name));
    }
    strlcpy(ctrl_hapd, new_profile->ctrl_hapd, sizeof(ctrl_hapd));
    strlcpy(ctrl_wpasupp, new_profile->ctrl_wpasupp, sizeof(ctrl_wpasupp));

    /* Apply the new profile */
    status = acfg_apply_profile(new_profile);

    if(status == QDF_STATUS_SUCCESS)
        printf("Configuration Completed \n\r");

    /* Free cur_profile & new_profile */
    acfg_free_profile(new_profile);

    return status;
}

param_info_t wrap_enable_radio_params[] = {
        {"wifi", PARAM_STRING, "radio name"},
};
int wrap_enable_radio(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE ;

    status = acfg_set_radio_enable((uint8_t *)params[0]);

    return acfg_to_os_status(status) ;
}

param_info_t wrap_disable_radio_params[] = {
        {"wifi", PARAM_STRING, "radio name"},
};
int wrap_disable_radio(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE ;

    status = acfg_set_radio_disable((uint8_t *)params[0]);

    return acfg_to_os_status(status) ;
}

param_info_t wrap_set_country_params[] = {
        {"wifi", PARAM_STRING, "radio name"},
        {"code", PARAM_UINT32 , "country"},
};

int wrap_set_country(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE ;
    uint32_t country_code;

    get_uint32(params[1], &country_code);

    status = acfg_set_country((uint8_t *)params[0], (uint16_t)country_code);

    return acfg_to_os_status(status) ;
}

param_info_t wrap_set_shpreamble_params[] = {
        {"wifi", PARAM_STRING, "radio name"},
        {"code", PARAM_UINT32 , "short preamble"},
};

int wrap_set_shpreamble(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE ;
    uint32_t shpreamble;

    get_uint32(params[1], &shpreamble);

    status = acfg_set_shpreamble((uint8_t *)params[0], (uint16_t)shpreamble);

    return acfg_to_os_status(status) ;
}

param_info_t wrap_set_shslot_params[] = {
        {"wifi", PARAM_STRING, "radio name"},
        {"code", PARAM_UINT32 , "short slot"},
};

int wrap_set_shslot(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE ;
    uint32_t shslot;

    get_uint32(params[1], &shslot);

    status = acfg_set_shslot((uint8_t *)params[0], (uint16_t)shslot);

    return acfg_to_os_status(status) ;
}

param_info_t wrap_set_antenna_params[] = {
        {"wifi", PARAM_STRING, "radio name"},
        {"mask", PARAM_UINT32 , "chainmask value"},
};

int wrap_set_tx_antenna(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE ;
    uint32_t mask;

    get_uint32(params[1], &mask);

    status = acfg_set_tx_antenna((uint8_t *)params[0], (uint16_t)mask);

    return acfg_to_os_status(status) ;
}

int wrap_set_rx_antenna(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE ;
    uint32_t mask;

    get_uint32(params[1], &mask);

    status = acfg_set_rx_antenna((uint8_t *)params[0], (uint16_t)mask);

    return acfg_to_os_status(status) ;
}

param_info_t wrap_get_antenna_params[] = {
        {"wifi", PARAM_STRING, "radio name"},
};

int wrap_get_tx_antenna(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE ;
    uint32_t mask;

    status = acfg_get_tx_antenna((uint8_t *)params[0], &mask);

    msg("Tx Antenna mask: %u", mask);

    return acfg_to_os_status(status) ;
}

int wrap_get_rx_antenna(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE ;
    uint32_t mask;

    status = acfg_get_rx_antenna((uint8_t *)params[0], &mask);

    msg("Rx Antenna mask: %u", mask);

    return acfg_to_os_status(status) ;
}

param_info_t wrap_set_txpower_limit_params[] = {
        {"wifi", PARAM_STRING, "radio name"},
        {"Band type", PARAM_UINT32 , "1 for 2.4Ghz, 2 for 5Ghz"},
        {"power", PARAM_UINT32 , "power limit"},
};

int wrap_set_txpower_limit(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE ;
    enum acfg_band_type band;
    uint32_t power;

    get_uint32(params[1], (uint32_t*)&band);
    get_uint32(params[2], &power);

    status = acfg_set_txpower_limit((uint8_t *)params[0], band, power);

    return acfg_to_os_status(status) ;
}

param_info_t wrap_reset_profile_params[] = {
    {"radio", PARAM_STRING, "radio name"},
};

/**
 * @ Force profile reset.
 * @ Deletes the current profile for the respective radio
 *
 * @param radio name
 *
 * @return
 */
int wrap_reset_profile(char *params[])
{
    int status = QDF_STATUS_SUCCESS;

    /* Takes radioname as input & delete the
       respective current profile file */
    status = acfg_reset_cur_profile(params[0]);
    if(status < 0 ) {
        printf("Reset profile failed \n\r");
        return QDF_STATUS_E_FAILURE;
    }
    return QDF_STATUS_SUCCESS;
}

param_info_t wrap_create_vap_params[] = {
    {"wifi", PARAM_STRING , "radio name"},
    {"vap", PARAM_STRING , "vap name"},
    {"opmode", PARAM_UINT32 , "opmode"},
    {"vapid", PARAM_SINT32 , "vapid"},
    {"flags", PARAM_UINT32 , "integer representing the bitwise"\
        "OR of vapinfo flags"},
};

/**
 * @brief Wrapper for acfg_create_vap
 *
 * @param params[]
 *
 * @return
 */
int wrap_create_vap(char *params[])
{
    uint8_t  *wifi , *vap ;
    int32_t   vapid;
    acfg_opmode_t mode ;
    uint32_t flags ;

    uint32_t status = QDF_STATUS_E_FAILURE ;

    /*printf("%s(): Param passed  - ",__FUNCTION__);*/

    wifi = (uint8_t *)params[0] ;
    vap = (uint8_t *)params[1] ;
    get_uint32(params[2], (uint32_t *)&mode);
    get_uint32(params[3], (uint32_t *)&vapid);
    get_uint32(params[4] , (uint32_t *)&flags);

    dbg_print_params("wifi - %s; vap - %s; mode - 0x%x; vapid - %d; flags - 0x%x",\
            wifi, vap, mode, vapid, flags);

    status = acfg_create_vap( wifi, vap, mode, vapid, flags);

    return acfg_to_os_status(status) ;
}


param_info_t wrap_delete_vap_params[] = {
    {"radio", PARAM_STRING , "radio name"},
    {"vap", PARAM_STRING , "vap name"},
};


/**
 * @brief Wrapper for acfg_delete_vap
 *
 * @param params[]
 *
 * @return
 */
int wrap_delete_vap(char *params[])
{

    uint32_t status = QDF_STATUS_E_FAILURE ;

    dbg_print_params("radio - %s; vap - %s ",params[0], params[1]);

    status = acfg_delete_vap((uint8_t *)params[0], (uint8_t *)params[1]);

    return acfg_to_os_status(status) ;
}


param_info_t wrap_set_ssid_params[] = {
    {"vap", PARAM_STRING , "vap name"},
    {"ssid", PARAM_STRING , "ssid to set"},
};


/**
 * @brief  Wrapper for acfg_set_ssid
 *
 * @param params[]
 *
 * @return
 */
int wrap_set_ssid(char *params[])
{
    acfg_ssid_t ssid ;
    uint32_t status = QDF_STATUS_E_FAILURE ;

    if (strlcpy((char *)ssid.ssid, params[1],
                sizeof(ssid.ssid)) >= sizeof(ssid.ssid)) {
        printf("%s: ssid too long to copy", __func__);
        return QDF_STATUS_E_FAILURE;
    }
    ssid.len = strlen((char *)ssid.ssid);

    dbg_print_params("vap - %s; ssid - %s; ssid len - %d",\
            params[0], ssid.ssid, ssid.len);

    status = acfg_set_ssid((uint8_t *)params[0], &ssid);
    if (status != QDF_STATUS_SUCCESS)
        printf("%s: setssid failed\n", __func__);

    return acfg_to_os_status(status) ;
}


param_info_t wrap_get_ssid_params[] = {
    {"vap", PARAM_STRING , "vap name"},
};


/**
 * @brief Wrapper for acfg_get_ssid
 *
 * @param params[]
 *
 * @return
 */
int wrap_get_ssid(char *params[])
{
    acfg_ssid_t ssid ;
    uint32_t status = QDF_STATUS_E_FAILURE ;

    dbg_print_params("vap - %s",params[0]);

    memset(&ssid, 0, sizeof(acfg_ssid_t));

    status = acfg_get_ssid((uint8_t *)params[0], &ssid);
    msg("SSID - %s, SSID len - %d",(char *)ssid.ssid,ssid.len);

    return acfg_to_os_status(status) ;
}

param_info_t wrap_add_client_params[] = {
        {"vap", PARAM_STRING , "vap name"},
        {"macadr", PARAM_STRING, "client mac address"},
        {"aid", PARAM_UINT16, " assoc id"},
        {"qos", PARAM_UINT8, "qos on/off"},
        {"lrates", PARAM_STRING, "legacy rates"},
        {"htrates", PARAM_STRING, "ht rates"},
        {"vhtrates", PARAM_STRING, "vht rates"},
        {"herates", PARAM_STRING, "HE rates"},
};

int wrap_add_client(char *params[])
{
    uint8_t   *vap;
    char *rate, *save_ptr;
    uint8_t   mac[6];
    uint32_t  aid, qos, tmp, count = 0;
    acfg_rateset_t   lrates, htrates, vhtrates, herates;
    uint32_t status = QDF_STATUS_E_FAILURE ;

    memset(&lrates, 0, sizeof(acfg_rateset_t));
    memset(&htrates, 0, sizeof(acfg_rateset_t));
    memset(&vhtrates, 0, sizeof(acfg_rateset_t));
    memset(&herates, 0, sizeof(acfg_rateset_t));

    vap = (uint8_t *)params[0] ;

	acfg_mac_str_to_octet((uint8_t*)params[1], mac);
    get_uint32(params[2], (uint32_t *)&aid);
    get_uint32(params[3], (uint32_t *)&qos);

    count = 0;
    rate = strtok_r(params[4], " ", &save_ptr);
    if( NULL == rate)
        return status;
    get_uint32(rate, &tmp);
    lrates.rs_rates[0] = (uint8_t)(tmp & 0xff);
    count ++;
    while((rate = strtok_r(NULL, " ", &save_ptr)) != NULL) {
        get_uint32(rate, &tmp);
        lrates.rs_rates[count] = (uint8_t)(tmp & 0xff);
        count ++;
    }
    lrates.rs_nrates = count;

    count = 0;
    rate = strtok_r(params[5], " ", &save_ptr);
    if( NULL == rate)
        return status;
    get_uint32(rate, &tmp);
    htrates.rs_rates[0] = (uint8_t)(tmp & 0xff);
    count ++;
    while((rate = strtok_r(NULL, " ", &save_ptr)) != NULL) {
        get_uint32(rate, &tmp);
        htrates.rs_rates[count] = (uint8_t)(tmp & 0xff);
        count ++;
    }
    htrates.rs_nrates = count;

    //    vhtrates.rs_rates[0] = 0xff;
    get_uint32(params[6], &tmp);
    vhtrates.rs_rates[0] = (uint8_t)(tmp& 0xff);

    get_hex(params[7], &tmp);
    herates.rs_rates[0] = (uint16_t)tmp & 0xff;
    herates.rs_rates[1] = ((uint16_t)tmp >> 8) & 0xff;

    status = acfg_add_client(vap, mac, aid, qos, lrates, htrates, vhtrates, herates);
    return status;
}

param_info_t wrap_del_client_params[] = {
        {"vap", PARAM_STRING , "vap name"},
        {"macadr", PARAM_STRING, "client mac address"},
};

int wrap_del_client(char *params[])
{
    uint8_t   *vap;
    uint8_t   mac[6];
    uint32_t status = QDF_STATUS_E_FAILURE ;

    vap = (uint8_t *)params[0] ;
	acfg_mac_str_to_octet((uint8_t*)params[1], mac);

    status = acfg_delete_client(vap, mac);
    return status;
}

param_info_t wrap_forward_client_params[] = {
        {"vap", PARAM_STRING , "vap name"},
        {"macadr", PARAM_STRING, "client mac address"},
};

int wrap_forward_client(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE ;
    uint8_t   *vap;
    uint8_t   mac[6];

    vap = (uint8_t *)params[0] ;
	acfg_mac_str_to_octet((uint8_t*)params[1], mac);

    status = acfg_forward_client(vap, mac);
    return status;
}

param_info_t wrap_del_key_params[] = {
        {"vap", PARAM_STRING , "vap name"},
        {"macadr", PARAM_STRING, "client mac address"},
        {"idx", PARAM_UINT32, "key index"},
};

int wrap_del_key(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE ;
    uint8_t   *vap;
    uint8_t   mac[6];
    uint32_t  idx;

    vap = (uint8_t *)params[0] ;
	acfg_mac_str_to_octet((uint8_t*)params[1], mac);
    get_uint32(params[2], &idx);

    idx &= 0xFFFF;
    status = acfg_del_key(vap, mac, (uint16_t)idx);
    return status;
}

param_info_t wrap_set_key_params[] = {
        {"vap", PARAM_STRING , "vap name"},
        {"macadr", PARAM_STRING, "client mac address"},
        {"cipher", PARAM_UINT32, "cipher type"},
        {"idx", PARAM_UINT32, "key index"},
        {"keylen", PARAM_UINT32, "key length"},
        {"idx", PARAM_STRING, "key data of 64bytes"},
};

int wrap_set_key(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE ;
    uint8_t   *vap;
    uint8_t   mac[6];
    uint32_t  idx;
    uint32_t cipher;
    uint32_t keylen;
    uint8_t keydata[ACFG_KEYBUF_SIZE+ACFG_MICBUF_SIZE];
    char tmpstr[3];
    uint32_t tmpint, i;

    vap = (uint8_t *)params[0] ;
	acfg_mac_str_to_octet((uint8_t*)params[1], mac);
    get_uint32(params[2], &cipher);
    get_uint32(params[3], &idx);
    get_uint32(params[4], &keylen);
    if( keylen > (ACFG_KEYBUF_SIZE+ACFG_MICBUF_SIZE)){
        dbg_print_params("Invalid key len- %d",keylen);
        return QDF_STATUS_E_FAILURE;
    }
    memset(keydata, 0, sizeof(keydata));
    for (i = 0; i < keylen; i ++) {
        tmpstr[0] = params[5][2 *i];
        tmpstr[1] = params[5][2 *i +1];
        tmpstr[2] = '\0';
        get_hex(tmpstr, &tmpint);
        keydata[i] = (uint8_t)(tmpint & 0xFF);
    }
    idx &= 0xFFFF;
    status = acfg_set_key(vap, mac, (CIPHER_METH)cipher, (uint16_t)idx,
                          keylen, keydata);
    return status;
}

param_info_t wrap_set_channel_params[] = {
    {"vap", PARAM_STRING, "vap name"},
    {"channel", PARAM_UINT8, "channel number"},
};


/**
 * @brief Wrapper for acfg_set_channel
 *
 * @param params[]
 *
 * @return
 */
int wrap_set_channel(char *params[])
{
    uint32_t chan ;
    uint32_t status = QDF_STATUS_E_FAILURE ;

    get_uint32(params[1], (uint32_t *) &chan);
    dbg_print_params("vap - %s; channel - %d",params[0],chan);

    status = acfg_set_channel((uint8_t *)params[0], chan);

    return acfg_to_os_status(status) ;
}


param_info_t wrap_get_channel_params[] = {
    {"vap", PARAM_STRING, "vap name"},
};


/**
 * @brief Wrapper for acfg_get_channel
 *
 * @param params[]
 *
 * @return
 */
int wrap_get_channel(char *params[])
{
    uint8_t chan ;
    uint32_t status = QDF_STATUS_E_FAILURE ;

    dbg_print_params("vap - %s; channel - %d",params[0],chan);

    status = acfg_get_channel((uint8_t *)params[0], &chan);
    msg("Channel - %d",chan);

    return acfg_to_os_status(status) ;
}



param_info_t wrap_set_opmode_params[] = {
    {"vap", PARAM_STRING, "vap name"},
    {"opmode", PARAM_UINT32, "operation mode to set"},
};


/**
 * @brief Wrapper for acfg_set_opmode
 *
 * @param params[]
 *
 * @return
 */
int wrap_set_opmode(char *params[])
{
    acfg_opmode_t opmode ;
    uint32_t status = QDF_STATUS_E_FAILURE ;

    get_uint32(params[1], (uint32_t *)&opmode);

    dbg_print_params("vap - %s; opmode - %d",params[0],opmode);

    status = acfg_set_opmode((uint8_t *)params[0], opmode);

    return acfg_to_os_status(status) ;
}


param_info_t wrap_get_opmode_params[] = {
    {"vap", PARAM_STRING, "vap name"},
};


/**
 * @brief Wrapper for acfg_get_opmode
 *
 * @param params[]
 *
 * @return
 */
int wrap_get_opmode(char *params[])
{
    acfg_opmode_t opmode ;
    uint32_t status = QDF_STATUS_E_FAILURE ;

    dbg_print_params("vap - %s;",params[0]);

    status = acfg_get_opmode((uint8_t *)params[0], &opmode);
    msg("Opmode - %d",opmode);

    return acfg_to_os_status(status) ;
}



param_info_t wrap_set_freq_params[] = {
    {"vap", PARAM_STRING, "vap name"},
    {"freq", PARAM_STRING, "frequency in MHz"},
};


/**
 * @brief Wrapper for acfg_set_freq
 *
 * @param params[]
 *
 * @return
 */
int wrap_set_freq(char *params[])
{
    uint32_t freq ;
    uint32_t status = QDF_STATUS_E_FAILURE ;

    get_uint32(params[1], &freq);
    dbg_print_params("vap - %s; frequency - %dMHz",params[0],freq);

    status = acfg_set_freq((uint8_t *)params[0], freq);

    return acfg_to_os_status(status) ;
}

param_info_t wrap_set_rts_params[] = {
    {"vap", PARAM_STRING, "vap name"},
    {"rts", PARAM_STRING, "rts value"},
    {"fixed", PARAM_STRING, "fixed?"},
};


/**
 * @brief Wrapper for acfg_set_rts
 *
 * @param params[]
 *
 * @return
 */
int wrap_set_rts(char *params[])
{
    acfg_rts_t rts = 0;
    uint32_t status = QDF_STATUS_E_FAILURE ;
    uint32_t temp = 0;

    if(!strncmp(params[1], "off", 3))
        rts = IEEE80211_RTS_MAX;
    else
        get_uint32(params[1], &rts);
    if(rts <= 0 || rts > IEEE80211_RTS_MAX)
        rts = IEEE80211_RTS_MAX;
    get_uint32(params[2], &temp);

    dbg_print_params("vap - %s; rts - %d",params[0], rts);

    status = acfg_set_rts((uint8_t *)params[0], &rts);

    return acfg_to_os_status(status) ;
}



param_info_t wrap_get_rts_params[] = {
    {"vap", PARAM_STRING, "vap name"},
};


/**
 * @brief Wrapper for acfg_get_rts
 *
 * @param params[]
 *
 * @return
 */
int wrap_get_rts(char *params[])
{
    acfg_rts_t rts ;
    uint32_t status = QDF_STATUS_E_FAILURE ;

    dbg_print_params("vap - %s;",params[0]);

    status = acfg_get_rts((uint8_t *)params[0], &rts);

    if(rts == 0 || rts == IEEE80211_RTS_MAX)
        msg("RTS Threshold - Disabled");
    else
        msg("RTS Threshold - %d",rts);

    return acfg_to_os_status(status) ;
}


param_info_t wrap_set_frag_params[] = {
    {"vap", PARAM_STRING, "vap name"},
    {"frag", PARAM_STRING, "frag value"},
    {"fixed", PARAM_STRING, "fixed?"},
};


/**
 * @brief Wrapper for acfg_set_frag
 *
 * @param params[]
 *
 * @return
 */
int wrap_set_frag(char *params[])
{
    acfg_frag_t frag = 0;
    uint32_t status = QDF_STATUS_E_FAILURE ;

    if(!strncmp(params[1], "off", 3))
        frag = IEEE80211_FRAGMT_THRESHOLD_MAX;
    else
        get_uint32(params[1], &frag);
    if( frag <= 0 || frag > IEEE80211_FRAGMT_THRESHOLD_MAX)
        frag = IEEE80211_FRAGMT_THRESHOLD_MAX;
    dbg_print_params("vap - %s; frag - %d",params[0], frag);

    status = acfg_set_frag((uint8_t *)params[0], &frag);

    return acfg_to_os_status(status) ;
}

param_info_t wrap_get_frag_params[] = {
    {"vap", PARAM_STRING, "vap name"},
};


/**
 * @brief Wrapper for acfg_get_frag
 *
 * @param params[]
 *
 * @return
 */
int wrap_get_frag(char *params[])
{
    acfg_frag_t frag ;
    uint32_t status = QDF_STATUS_E_FAILURE ;

    dbg_print_params("vap - %s;",params[0]);

    status = acfg_get_frag((uint8_t *)params[0], &frag);

    if(frag < 256 || frag == IEEE80211_FRAGMT_THRESHOLD_MAX)
        msg("Frag Threshold - Disabled");
    else
        msg("Frag Threshold - %d",frag);

    return acfg_to_os_status(status) ;
}


param_info_t wrap_set_txpow_params[] = {
    {"vap", PARAM_STRING, "vap name"},
    {"txpow", PARAM_STRING, "txpow value"},
    {"fixed", PARAM_STRING, "fixed?"},
};


/**
 * @brief Wrapper for acfg_set_frag
 *
 * @param params[]
 *
 * @return
 */
int wrap_set_txpow(char *params[])
{
    acfg_txpow_t txpow ;
    uint32_t status = QDF_STATUS_E_FAILURE ;

    if(!strncmp(params[1], "off", 3))
        txpow = 0;
    else
        get_uint32(params[1], &txpow);

    dbg_print_params("vap - %s; txpow - %d",params[0],
            txpow);

    status = acfg_set_txpow((uint8_t *)params[0], &txpow);

    return acfg_to_os_status(status) ;
}


param_info_t wrap_get_txpow_params[] = {
    {"vap", PARAM_STRING, "vap name"},
};


/**
 * @brief Wrapper for acfg_get_txpow
 *
 * @param params[]
 *
 * @return
 */
int wrap_get_txpow(char *params[])
{
    acfg_txpow_t txp ;
    uint32_t status = QDF_STATUS_E_FAILURE ;

    dbg_print_params("vap - %s;",params[0]);

    status = acfg_get_txpow((uint8_t *)params[0], &txp);

    if(txp == 0)
        msg("TxPower - Disabled");
    else
        msg("TxPower Threshold - %d",txp);

    return acfg_to_os_status(status) ;
}


param_info_t wrap_get_ap_params[] = {
    {"vap", PARAM_STRING, "vap name"},
};


/**
 * @brief Wrapper for acfg_get_ap
 *
 * @param params[]
 *
 * @return
 */
int wrap_get_ap(char *params[])
{
    acfg_macaddr_t macaddr ;
    uint32_t status = QDF_STATUS_E_FAILURE ;

    dbg_print_params("vap - %s;",params[0]);

    status = acfg_get_ap((uint8_t *)params[0], &macaddr);
    msg("AP Macaddr - %x:%x:%x:%x:%x:%x",\
            macaddr.addr[0], macaddr.addr[1], macaddr.addr[2], \
            macaddr.addr[3], macaddr.addr[4], macaddr.addr[5]);

    return acfg_to_os_status(status) ;
}


param_info_t wrap_set_enc_params[] = {
    {"vap", PARAM_STRING, "vap name"},
    {"flag",PARAM_STRING, "flags"},
    {"enc", PARAM_STRING, "encode str"},
};


/**
 * @brief Wrapper for acfg_set_enc
 *
 * @param params[]
 *
 * @return
 */
int wrap_set_enc(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE;
    uint32_t flag = 0, len;
    uint32_t temp = 0;

    dbg_print_params("vap - %s; flag - %s, encode - %s",
            params[0], params[1], params[2]);

    if(strchr(params[1], '[') == NULL) {
        get_hex(params[1], &flag);
    }
    if(!strncmp(params[2], "off", 3))
        flag |= ACFG_ENCODE_DISABLED;
    if(strchr(params[1], '[') != NULL) {
        if( (sscanf(params[1], "[%i]",(int32_t*) &temp) == 1) &&
                (temp > 0) && (temp < ACFG_ENCODE_INDEX)) {
            flag |= temp;
        }
    }
    /* in wep open string case, for test purpose; */
    len = strnlen(params[2], ACFG_ENCODING_TOKEN_MAX);
    if(!len) {
        status = acfg_set_enc((uint8_t *)params[0],
                (acfg_encode_flags_t)flag, NULL);
    }
    else {
        status = acfg_set_enc((uint8_t *)params[0],
                (acfg_encode_flags_t)flag, params[2]);
    }
    return acfg_to_os_status(status);
}


param_info_t wrap_set_vendor_params[] = {
    {"vap", PARAM_STRING, "vap name"},
    {"param", PARAM_STRING, "parameter id"},
    {"val", PARAM_STRING, "parameter value"},
    {"param_type", PARAM_STRING, "parameter type str/int/mac"},
    {"reinit", PARAM_STRING, "reinit flag"},
};

/**
 * @brief Wrapper for acfg_set_vap_vendor_param
 *
 * @param params[]
 *
 * @return
 */
int wrap_set_vap_vendor_param(char *params[])
{
    acfg_param_vap_t paramid ;
    acfg_vendor_param_data_t val ;
    uint32_t status = QDF_STATUS_E_FAILURE ;
    uint32_t len = 0, type = ACFG_TYPE_INT, reinit = 0;

    get_uint32(params[1], (uint32_t *)&paramid);
    get_uint32(params[4], (uint32_t *)&reinit);
    /* convert val to required type */
    if(strcmp(params[3], "str") == 0)
    {
        memset((char *)&val,0,sizeof(val));
        if (strlcpy((char *)&val, params[2], sizeof(val)) >= sizeof(val)) {
            printf("%s: param too long to copy\n", __func__);
            return QDF_STATUS_E_FAILURE;
        }
        len = strlen((char *)&val) + 1;
        type = ACFG_TYPE_STR;
    }
    else if(strcmp(params[3], "int") == 0)
    {
        *(uint32_t *)(&val) = atol(params[2]);
        len = sizeof(uint32_t);
        type = ACFG_TYPE_INT;
    }
    else if(strcmp(params[3], "mac") == 0)
    {
        acfg_mac_str_to_octet((uint8_t *)params[2], (uint8_t *)&val);
        len = ACFG_MACADDR_LEN;
        type = ACFG_TYPE_MACADDR;
    }
    else
    {
        dbg_print_params("Invalid type");
        acfg_to_os_status(status);
    }

    dbg_print_params("vap - %s; paramid - %d; value - %s; paramtype - %s; reinit - %d",\
            params[0], paramid, params[2], params[3], reinit);

    status = acfg_set_vap_vendor_param((uint8_t *)params[0], paramid, (uint8_t *)&val, len, type, reinit);

    return acfg_to_os_status(status) ;
}

param_info_t wrap_get_vendor_params[] = {
    {"vap", PARAM_STRING, "vap name"},
    {"param", PARAM_STRING, "parameter id"},
};


/**
 * @brief Wrapper for acfg_get_vap_vendor_param
 *
 * @param params[]
 *
 * @return
 */
int wrap_get_vap_vendor_param(char *params[])
{
    acfg_param_vap_t paramid ;
    acfg_vendor_param_data_t val ;
    uint32_t status = QDF_STATUS_E_FAILURE ;
    uint32_t type;

    get_uint32(params[1], (uint32_t *)&paramid);

    dbg_print_params("vap - %s; paramid - %d",\
            params[0], paramid);

    status = acfg_get_vap_vendor_param((uint8_t *)params[0], paramid,
            (uint8_t *)&val, (uint32_t *)&type);

    if(type == ACFG_TYPE_INT)
        msg("value - %d", *(uint32_t *)&val);
    else if(type == ACFG_TYPE_STR)
        msg("str - %s", (char *)&val);
    else if(type == ACFG_TYPE_MACADDR)
    {
        msg("mac - %02x:%02x:%02x:%02x:%02x:%02x", val.data[0],
                val.data[1],
                val.data[2],
                val.data[3],
                val.data[4],
                val.data[5]);
    }
    else
        msg("Driver returned invalid type");

    return acfg_to_os_status(status) ;
}


param_info_t wrap_set_vapprm_params[] = {
    {"vap", PARAM_STRING, "vap name"},
    {"param", PARAM_STRING, "parameter id"},
    {"val", PARAM_STRING, "parameter value"},
};


/**
 * @brief Wrapper for acfg_set_vap_param
 *
 * @param params[]
 *
 * @return
 */
int wrap_set_vap_param(char *params[])
{
    acfg_param_vap_t paramid ;
    uint32_t val ;
    uint32_t status = QDF_STATUS_E_FAILURE ;

    get_uint32(params[1], (uint32_t *)&paramid);
    get_uint32(params[2], (uint32_t *)&val);

    dbg_print_params("vap - %s; paramid - %d; value - %d",\
            params[0], paramid, val);

    status = acfg_set_vap_param((uint8_t *)params[0], paramid, val);

    return acfg_to_os_status(status) ;
}


param_info_t wrap_get_vapprm_params[] = {
    {"vap", PARAM_STRING, "vap name"},
    {"param", PARAM_STRING, "parameter id"},
};


/**
 * @brief Wrapper for acfg_get_vap_param
 *
 * @param params[]
 *
 * @return
 */
int wrap_get_vap_param(char *params[])
{
    acfg_param_vap_t paramid ;
    uint32_t val ;
    uint32_t status = QDF_STATUS_E_FAILURE ;

    get_uint32(params[1], (uint32_t *)&paramid);
    dbg_print_params("vap - %s; paramid - %d; ",params[0], paramid);

    status = acfg_get_vap_param((uint8_t *)params[0], paramid, &val);
    msg("Param value - %d",val);

    return acfg_to_os_status(status) ;
}



param_info_t wrap_set_radioprm_params[] = {
    {"radio", PARAM_STRING, "radio name"},
    {"param", PARAM_STRING, "parameter id"},
    {"val", PARAM_STRING, "parameter value"},
};


/**
 * @brief Wrapper for acfg_set_radio_param
 *
 * @param params[]
 *
 * @return
 */
int wrap_set_radio_param(char *params[])
{
    acfg_param_radio_t paramid ;
    uint32_t val ;
    uint32_t status = QDF_STATUS_E_FAILURE ;

    get_uint32(params[1], (uint32_t *)&paramid);
    get_uint32(params[2], (uint32_t *)&val);
    dbg_print_params("vap - %s; paramid - %d; value - %d",\
            params[0], paramid, val);

    status = acfg_set_radio_param((uint8_t *)params[0], paramid, val);

    return acfg_to_os_status(status) ;
}


param_info_t wrap_get_radioprm_params[] = {
    {"radio", PARAM_STRING, "radio name"},
    {"param", PARAM_STRING, "parameter id"},
};


/**
 * @brief Wrapper for acfg_get_radio_param
 *
 * @param params[]
 *
 * @return
 */
int wrap_get_radio_param(char *params[])
{
    acfg_param_radio_t paramid ;
    uint32_t val ;
    uint32_t status = QDF_STATUS_E_FAILURE ;

    get_uint32(params[1], (uint32_t *)&paramid);
    dbg_print_params("vap - %s; paramid - %d",params[0], paramid);

    status = acfg_get_radio_param((uint8_t *)params[0], paramid, &val);
    msg("Param value - %d",val);

    return acfg_to_os_status(status) ;
}

param_info_t wrap_get_country_params[] = {
    {"radio", PARAM_STRING, "radio name"},
};

int wrap_get_country(char *params[])
{
    uint32_t val ;
    uint32_t status = QDF_STATUS_E_FAILURE ;

    dbg_print_params("radio - %s",params[0]);

    status = acfg_get_country((uint8_t *)params[0], &val);

    msg("Country %u", val);

    return acfg_to_os_status(status) ;
}

param_info_t wrap_get_regdomain_params[] = {
    {"radio", PARAM_STRING, "radio name"},
};

int wrap_get_regdomain(char *params[])
{
    uint32_t val ;
    uint32_t status = QDF_STATUS_E_FAILURE ;

    dbg_print_params("radio - %s",params[0]);

    status = acfg_get_regdomain((uint8_t *)params[0], &val);

    msg("Regdomain %u", val);

    return acfg_to_os_status(status) ;
}

param_info_t wrap_get_shpreamble_params[] = {
    {"radio", PARAM_STRING, "radio name"},
};

int wrap_get_shpreamble(char *params[])
{
    uint32_t val ;
    uint32_t status = QDF_STATUS_E_FAILURE ;

    dbg_print_params("radio - %s",params[0]);

    status = acfg_get_shpreamble((uint8_t *)params[0], &val);

    msg("Short Preamble %u", val);

    return acfg_to_os_status(status) ;
}

param_info_t wrap_get_shslot_params[] = {
    {"radio", PARAM_STRING, "radio name"},
};

int wrap_get_shslot(char *params[])
{
    uint32_t val ;
    uint32_t status = QDF_STATUS_E_FAILURE ;

    dbg_print_params("radio - %s",params[0]);

    status = acfg_get_shslot((uint8_t *)params[0], &val);

    msg("Short Slot %u", val);

    return acfg_to_os_status(status) ;
}

param_info_t wrap_set_rate_params[] = {
    {"vap", PARAM_STRING, "vap name"},
    {"rate", PARAM_STRING, "rate in Mbps"},
};


/**
 * @brief Wrapper for acfg_get_rate
 *
 * @param params[]
 *
 * @return
 */
int wrap_set_rate(char *params[])
{
    acfg_rate_t rate ;
    uint32_t status = QDF_STATUS_E_FAILURE ;

    get_uint32(params[1], (uint32_t *)&rate);
    dbg_print_params("vap - %s; rate - %dMBps",params[0],rate);

    if(strchr(params[1], 'M') != NULL)
        rate *= 1000000;

    status = acfg_set_rate((uint8_t *)params[0], &rate);

    return acfg_to_os_status(status) ;
}

param_info_t wrap_get_rate_params[] = {
    {"vap", PARAM_STRING, "vap name"},
};

/**
 * @brief Wrapper for acfg_get_rate
 *
 * @param params[]
 *
 * @return
 */
int wrap_get_rate(char *params[])
{
    uint32_t rate ;
    uint32_t status = QDF_STATUS_E_FAILURE ;

    dbg_print_params("vap - %s",params[0]);

    status = acfg_get_rate((uint8_t *)params[0], &rate);
    msg("Default bit rate - %d",rate);

    return acfg_to_os_status(status) ;
}

param_info_t wrap_set_phymode_params[] = {
    {"vap", PARAM_STRING, "vap name"},
    {"phymode", PARAM_UINT32, "phymode to set"},
};

/**
 * @brief Wrapper for acfg_set_phymode
 *
 * @param params[]
 *
 * @return
 */
int wrap_set_phymode(char *params[])
{
    acfg_phymode_t mode ;
    uint32_t status = QDF_STATUS_E_FAILURE ;

    get_uint32(params[1], (uint32_t *)&mode);
    dbg_print_params("vap - %s; phymode - %d",params[0],mode);

    status = acfg_set_phymode((uint8_t *)params[0], mode);

    return acfg_to_os_status(status) ;
}

param_info_t wrap_get_phymode_params[] = {
    {"vap", PARAM_STRING, "vap name"},
};

/**
 * @brief Wrapper for acfg_set_phymode
 *
 * @param params[]
 *
 * @return
 */
int wrap_get_phymode(char *params[])
{
    acfg_phymode_t mode ;
    uint32_t status = QDF_STATUS_E_FAILURE ;

    dbg_print_params("vap - %s",params[0]);

    status = acfg_get_phymode((uint8_t *)params[0], &mode);

    msg("Phymode %u", mode);

    return acfg_to_os_status(status) ;
}


static const char *
addr_ntoa(const uint8_t mac[ACFG_MACADDR_LEN])
{
    static char a[18];
    int i;

    i = snprintf(a, sizeof(a), "%02x:%02x:%02x:%02x:%02x:%02x",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return (i < 17 ? NULL : a);
}
extern const char * getcaps(int capinfo);
extern const char * getathcaps(int capinfo);
extern const char * gethtcaps(int capinfo);


param_info_t wrap_assoc_sta_info_params[] = {
    {"vap", PARAM_STRING, "vap name"},
};

/**
 * @brief Wrapper for acfg_assoc_sta_info
 *
 * @param params[]
 *
 * @return
 */
int wrap_assoc_sta_info(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE ;
    u_int32_t txrate, rxrate = 0;
    acfg_sta_info_req_t sireq ;
    acfg_sta_info_t sibuf[128] ;
    uint8_t *cp ;
    static const char *pMac = NULL;
    static const char *athcap = NULL;

    dbg_print_params("vap - %s ",params[0]);

    sireq.len = sizeof(sibuf) ;
    sireq.info = &sibuf[0] ;

    dbglog("Sending buffer of length %d",sireq.len);

    status = acfg_assoc_sta_info((uint8_t *)params[0], &sireq);
    if(status != QDF_STATUS_SUCCESS)
        return -1;

    dbglog("Received buffer of length %d",sireq.len);

    if(sireq.len == 0){
        msg("Received buffer of length 0, no STA associated? \n");
        return 0;
    }
    msg("%-17.17s %4s %4s %4s %4s %4s %4s %6s %6s %4s %5s %3s %8s %6s\n"
            , "ADDR", "AID", "CHAN", "TXRATE", "RXRATE", "RSSI", "IDLE", "TXSEQ", "RXSEQ"
            , "CAPS", "ACAPS", "ERP", "STATE", "HTCAPS" );

    cp = (uint8_t *)&sibuf[0] ;
    do {
        acfg_sta_info_t *psi = (acfg_sta_info_t *)cp;
        if(psi->isi_txratekbps == 0)
            txrate = (psi->isi_rates[psi->isi_txrate] & IEEE80211_RATE_VAL)/2;
        else
            txrate = psi->isi_txratekbps / 1000;
        if(psi->isi_rxratekbps >= 0) {
            rxrate = psi->isi_rxratekbps / 1000;
        }
        athcap = getathcaps(psi->isi_athflags);
        msg("%s %4u %3u %5dM %5dM %4d %4d %6d %6d %4s %5s %3x %8x %6s", \
                (((pMac = addr_ntoa(psi->isi_macaddr)) == NULL)? "00:00:00:00:00:00": pMac), \
                IEEE80211_AID(psi->isi_associd), psi->isi_ieee, txrate, rxrate, psi->isi_rssi, \
                psi->isi_inact, (int32_t)psi->isi_txseqs[0], (int32_t)psi->isi_rxseqs[0], \
                getcaps(psi->isi_capinfo), (*athcap == '\0' ? "NULL": athcap), psi->isi_erp, \
                psi->isi_state, gethtcaps(psi->isi_htcap) );
        cp += psi->isi_len;
    }while(cp < (uint8_t *)&sibuf[0] + sireq.len);

    return acfg_to_os_status(status) ;
}


param_info_t wrap_mon_enable_filter_params[] = {
    {"vap", PARAM_STRING, "vap name"},
    {"val", PARAM_STRING , "int val"},

};
/**
 * @brief Wrapper for acfg_mon_enable_filter
 *
 * @param params[]
 *
 * @return
 */

int wrap_mon_enable_filter(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE;
    uint32_t val;
    get_hex(params[1], (uint32_t *) &val);

    printf("setting the val as 0x%x\n", val);
    status = acfg_mon_enable_filter((uint8_t *)params[0], val);

    return acfg_to_os_status(status) ;
}


param_info_t wrap_mon_listmac_params[] = {
    {"vap", PARAM_STRING, "vap name"},
};

/**
 ++ * @brief Wrapper for acfg_mon_listmac
 ++ *
 ++ * @param params[]
 ++ *
 ++ * @return
 ++ */

int wrap_mon_listmac(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE;
    status = acfg_mon_listmac((uint8_t *)params[0]);

    return acfg_to_os_status(status) ;
}


param_info_t wrap_mon_addmac_params[] = {
    {"vap", PARAM_STRING, "vap name"},
    {"macadr", PARAM_STRING, "mac address"},
};

/**
 * @brief Wrapper for acfg_mon_addmac
 *
 * @param params[]
 *
 * @return
 */
int
acfg_mac_str_to_octet_mon(char *mac_str, uint8_t *mac)
{
    int hex_count = 0;
    int char_count = 0;

    char *temp = mac_str;
    while (*mac_str) {
       if (isxdigit(*mac_str)) {
          hex_count++;
       }
       else if (*mac_str == ':' || *mac_str == '-') {

          if (hex_count == 0 || hex_count / 2 - 1 != char_count || (hex_count % 2) != 0)
            break;

          ++char_count;
       }
       else {
           char_count = -1;
       }


       ++mac_str;
    }

     if ((hex_count == 12 && char_count == 5)) {
        sscanf (temp,"%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",&mac[0],&mac[1],&mac[2],&mac[3],&mac[4],&mac[5]);
        return 1;
    }
    else
        return -1;
}

int wrap_mon_addmac(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE ;
    uint8_t mac[ACFG_MACSTR_LEN];
    uint8_t mac_len = strlen(params[1]);
    int retv;
    if (mac_len < 17 || mac_len > 17)
        return acfg_to_os_status(status);
    printf("adding mac on mon\n");
    dbg_print_params("vap - %s; macaddr - %s",params[0], params[1]);
    printf("vap - %s: macaddr - %s",(char *)params[0], (char *)params[1]);

    retv = acfg_mac_str_to_octet_mon(params[1], mac);
    if (retv == 1) {
        status = acfg_mon_addmac((uint8_t *)params[0], mac);
    }
    return acfg_to_os_status(status) ;
}

param_info_t wrap_mon_delmac_params[] = {
    {"vap", PARAM_STRING, "vap name"},
    {"macadr", PARAM_STRING, "mac address"},
};

/**
 * @brief Wrapper for acfg_mon_delmac
 *
 * @param params[]
 *
 * @return
 */

int wrap_mon_delmac(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE ;
    uint8_t mac[ACFG_MACSTR_LEN];
    dbg_print_params("vap - %s; macaddr - %s",params[0], params[1]);
    printf("vap - %s: macaddr - %s",(char *)params[0], (char *)params[1]);

    memcpy(mac, params[1], ACFG_MACSTR_LEN);

    acfg_mac_str_to_octet((uint8_t *)params[1], mac);
    status = acfg_mon_delmac((uint8_t *)params[0], mac);

    return acfg_to_os_status(status) ;
}

/* Wrapper functions for the first ACL list*/
param_info_t wrap_acl_addmac_params[] = {
    {"vap", PARAM_STRING, "vap name"},
    {"macadr", PARAM_STRING, "mac address"},
};

/**
 * @brief Wrapper for acfg_acl_addmac
 *
 * @param params[]
 *
 * @return
 */
int wrap_acl_addmac(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE ;
    uint8_t mac[ACFG_MACSTR_LEN];

    dbg_print_params("vap - %s; macaddr - %s",params[0], params[1]);
    printf("vap - %s: macaddr - %s",(char *)params[0], (char *)params[1]);

    memcpy(mac, params[1], ACFG_MACSTR_LEN);

    acfg_mac_str_to_octet((uint8_t *)params[0], mac);
    status = acfg_acl_addmac((uint8_t *)params[0], mac);

    return acfg_to_os_status(status) ;
}

param_info_t wrap_acl_getmac_params[] = {
    {"vap", PARAM_STRING, "vap name"},
};

/**
 * @brief Wrapper for acfg_acl_getmac
 *
 * @param params[]
 *
 * @return
 */
int wrap_acl_getmac(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE;
    acfg_macacl_t macacl;
    int i, j;

    dbg_print_params("vap - %s: ",params[0]);

    status = acfg_acl_getmac((uint8_t *)params[0], &macacl);

    msg("MACADDR's in ACL - %d", macacl.num);

    for(i = 0; i < macacl.num; i++) {
        printf("MAC address %d = ", (i + 1));
        for(j = 0; j < 6; j++) {
            printf("%x", macacl.macaddr[i][j]);
            printf("%c", (j != 5)? ':' : '\0');
        }
        printf("\n");
    }
    return acfg_to_os_status(status) ;
}


param_info_t wrap_acl_delmac_params[] = {
    {"vap", PARAM_STRING, "vap name"},
    {"macadr", PARAM_STRING, "mac address"},
};

/**
 * @brief Wrapper for acfg_acl_delmac
 *
 * @param params[]
 *
 * @return
 */
int wrap_acl_delmac(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE ;
    char mac[20];

    dbg_print_params("vap - %s; macaddr - %s",params[0], params[1]);
    printf("vap - %s: macaddr - %s",(char *)params[0], (char *)params[1]);

    memcpy(mac, params[1], ACFG_MACSTR_LEN);

    status = acfg_acl_delmac((uint8_t *)params[0], (uint8_t *)params[1]);

    return acfg_to_os_status(status) ;
}

/* Wrapper functions for the secondary ACL list*/
param_info_t wrap_acl_addmac_secondary_params[] = {
    {"vap", PARAM_STRING, "vap name"},
    {"macadr", PARAM_STRING, "mac address"},
};

/**
 * @brief Wrapper for acfg_acl_addmac_secondary
 *
 * @param params[]
 *
 * @return
 */
int wrap_acl_addmac_secondary(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE ;
    uint8_t mac[ACFG_MACSTR_LEN];

    dbg_print_params("vap - %s; macaddr - %s",params[0], params[1]);
    printf("vap - %s: macaddr - %s",(char *)params[0], (char *)params[1]);

    memcpy(mac, params[1], ACFG_MACSTR_LEN);

    acfg_mac_str_to_octet((uint8_t *)params[0], mac);
    status = acfg_acl_addmac_secondary((uint8_t *)params[0], mac);

    return acfg_to_os_status(status) ;
}


param_info_t wrap_acl_getmac_secondary_params[] = {
    {"vap", PARAM_STRING, "vap name"},
};

/**
 * @brief Wrapper for acfg_acl_getmac_secondary
 *
 * @param params[]
 *
 * @return
 */
int wrap_acl_getmac_secondary(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE ;
    acfg_macacl_t macacl;
    int i, j;

    dbg_print_params("vap - %s: ",params[0]);

    status = acfg_acl_getmac_secondary((uint8_t *)params[0], &macacl);

    msg("MACADDR's in ACL - %d", macacl.num);

    for(i = 0; i < macacl.num; i++) {
        printf("MAC address %d = ", (i + 1));
        for(j = 0; j < 6; j++) {
            printf("%x", macacl.macaddr[i][j]);
            printf("%c", (j != 5)? ':' : '\0');
        }
        printf("\n");
    }
    return acfg_to_os_status(status) ;
}


param_info_t wrap_acl_delmac_secondary_params[] = {
    {"vap", PARAM_STRING, "vap name"},
    {"macadr", PARAM_STRING, "mac address"},
};

/**
 * @brief Wrapper for acfg_acl_delmac_secondary
 *
 * @param params[]
 *
 * @return
 */
int wrap_acl_delmac_secondary(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE ;
    char mac[20];

    dbg_print_params("vap - %s; macaddr - %s",params[0], params[1]);
    printf("vap - %s: macaddr - %s",(char *)params[0], (char *)params[1]);

    memcpy(mac, params[1], ACFG_MACSTR_LEN);

    status = acfg_acl_delmac_secondary((uint8_t *)params[0], (uint8_t *)params[1]);

    return acfg_to_os_status(status) ;
}

param_info_t wrap_wlan_profile_create_from_file_params[] = {
    {"file", PARAM_STRING, "file name"},
};

param_info_t wrap_wlan_profile_create_params[] = {
    {"radio", PARAM_STRING, "radio name"},
    {"chan", PARAM_STRING, "channel"},
    {"freq", PARAM_STRING, "frequency"},
    {"txpow", PARAM_STRING, "txpower"},
    {"ctrycode", PARAM_STRING, "countrycode"},
    {"radiomac", PARAM_STRING, "mac address: radio"},
    {"vap", PARAM_STRING, "vap name"},
    {"opmode", PARAM_STRING, "operating mode: 1 - STA 6 - AP"},
    {"phymode", PARAM_STRING, "phy mode: 0-AUTO 1-11A 2-11B \
        3-11G 5-11NA 6-11NG"},
    {"ssid", PARAM_STRING, "essid"},
    {"bitrate", PARAM_STRING, "data rate 1M-54M"},
    {"bintval", PARAM_STRING, "for AP only 100, 200, 300"},
    {"rts", PARAM_STRING, "rts threshold 0-2346"},
    {"frag", PARAM_STRING, "frag threshold 0-2346"},
    {"vapmac", PARAM_STRING, "mac address: vap"},
    {"nodemac", PARAM_STRING, "mac address: station"},
    {"nodeacl", PARAM_STRING, "acl policy:0-none 1-accept 2-deny"},
    {"sec_method", PARAM_STRING, "security method"},
    {"ciph_method", PARAM_STRING, "cipher method"},
    {"psk/pasphrase", PARAM_STRING, "wpa-psk/wpa-passphrase"},
    {"WEP-KEY0", PARAM_STRING, "WEP-KEY-0"},
    {"WEP-KEY1", PARAM_STRING, "WEP-KEY-1"},
    {"WEP-KEY2", PARAM_STRING, "WEP-KEY-2"},
    {"WEP-KEY3", PARAM_STRING, "WEP-KEY-3"},
};

param_info_t wrap_wlan_profile_modify_params[] = {
    {"radio", PARAM_STRING, "radio name"},
    {"chan", PARAM_STRING, "channel"},
    {"freq", PARAM_STRING, "frequency"},
    {"txpow", PARAM_STRING, "txpower"},
    {"ctrycode", PARAM_STRING, "countrycode"},
    {"radiomac", PARAM_STRING, "mac address: radio"},
    {"vap", PARAM_STRING, "vap name"},
    {"opmode", PARAM_STRING, "operating mode: 1 - STA 6 - AP"},
    {"phymode", PARAM_STRING, "phy mode: 0-AUTO 1-11A 2-11B \
        3-11G 5-11NA 6-11NG"},
    {"ssid", PARAM_STRING, "essid"},
    {"bitrate", PARAM_STRING, "data rate 1M-54M"},
    {"bintval", PARAM_STRING, "for AP only 100, 200, 300"},
    {"rts", PARAM_STRING, "rts threshold 0-2346"},
    {"frag", PARAM_STRING, "frag threshold 0-2346"},
    {"vapmac", PARAM_STRING, "mac address: vap"},
    {"nodemac", PARAM_STRING, "mac address: station"},
    {"nodeacl", PARAM_STRING, "acl policy:0-none 1-accept 2-deny"},
    {"sec_method", PARAM_STRING, "security method"},
    {"ciph_method", PARAM_STRING, "cipher method"},
    {"psk/pasphrase", PARAM_STRING, "wpa-psk/wpa-passphrase"},
    {"WEP-KEY0", PARAM_STRING, "WEP-KEY-0"},
    {"WEP-KEY1", PARAM_STRING, "WEP-KEY-1"},
    {"WEP-KEY2", PARAM_STRING, "WEP-KEY-2"},
    {"WEP-KEY3", PARAM_STRING, "WEP-KEY-3"},
};

param_info_t wrap_hostapd_getconfig_params[] = {
    {"vap", PARAM_STRING, "vap name"},
};


int wrap_hostapd_getconfig(char *params[])
{
    uint32_t status = 0;
    char buffer[4096] = {0};

    status = acfg_hostapd_getconfig((uint8_t *)params[0], buffer);

    printf("%s: \nReceived Buffer: \n%s\n", __func__, buffer);

    return acfg_to_os_status(status) ;
}

param_info_t wrap_hostapd_set_wpa_params[] = {
    {"vap", PARAM_STRING, "vap name"},
    {"wpa", PARAM_STRING, "wpa"},
    {"psk/passphrase", PARAM_STRING, "wpa_psk=/wpa_passphrase=(full)"},
    {"wpa_key_mgmt", PARAM_STRING, "wpa_key_mgmt"},
    {"wpa_pairwise", PARAM_STRING, "wpa_pairwise"},
    {"wpa_group_rekey", PARAM_STRING, "wpa_group_rekey"},
    {"wpa_passphrase", PARAM_STRING, "wpa_passphrase"},
};

int wrap_hostapd_set_wpa(char *params[])
{
    uint32_t status = 0;

    //    status = acfg_hostapd_set_wpa((uint8_t **)params);

    return acfg_to_os_status(status) ;
}

int wrap_wps_pbc(char *params[])
{
    uint32_t status = 0;

    status = acfg_set_wps_pbc(params[0]);
    return acfg_to_os_status(status);
}

int wrap_wps_pin(char *params[])
{
    uint32_t status = 0;
    int pin_action = 0;
    char pin[10], pin_txt[10];

    if (strncmp(params[1], "set", 3) == 0) {
        pin_action = ACFG_WPS_PIN_SET;
        if (strlcpy(pin, params[2], sizeof(pin)) >= sizeof(pin)) {
            printf("%s: pin too lengthy to copy.\n", __func__);
            return QDF_STATUS_E_FAILURE;
        }
    } else if (strncmp(params[1], "random", 6) == 0) {
        pin_action = ACFG_WPS_PIN_RANDOM;
        pin[0] = '\0';
    }
    status = acfg_set_wps_pin(params[0], pin_action, pin, pin_txt, params[3]);
    return acfg_to_os_status(status);
}

int wrap_wps_config(char *params[])
{
    uint32_t status = 0;

    status = acfg_wps_config((uint8_t *)params[0], params[1],
            params[2], params[3], params[4]);

    return acfg_to_os_status(status);
}



param_info_t wrap_is_offload_vap_params[] = {
    {"vap", PARAM_STRING, "vap name"},
};


int wrap_is_offload_vap(char *params[])
{
    uint32_t status = 0;

    status = acfg_is_offload_vap((uint8_t *)params[0]);

    return acfg_to_os_status(status) ;
}


param_info_t wrap_set_preamble_params[] = {
    {"vap", PARAM_STRING , "vap name"},
    {"preamble", PARAM_STRING , "long/short preamble"},
};

/**
 * @brief  Wrapper for acfg_set_preamble
 *
 * @param params[]
 *
 * @return
 */
int wrap_set_preamble(char *params[])
{
    uint32_t preamble ;
    uint32_t status = QDF_STATUS_E_FAILURE ;

    get_uint32(params[1], (uint32_t *) &preamble);
    status = acfg_set_preamble((uint8_t *)params[0], preamble);
    if (status != QDF_STATUS_SUCCESS)
        printf("%s: set preamble failed\n", __func__);

    return acfg_to_os_status(status) ;
}


param_info_t wrap_set_slot_time_params[] = {
    {"vap", PARAM_STRING , "vap name"},
    {"slot", PARAM_STRING , "long/short slot time"},
};

/**
 * @brief  Wrapper for acfg_set_slot_time
 *
 * @param params[]
 *
 * @return
 */
int wrap_set_slot_time(char *params[])
{
    uint32_t slot ;
    uint32_t status = QDF_STATUS_E_FAILURE ;

    get_uint32(params[1], (uint32_t *) &slot);
    status = acfg_set_slot_time((uint8_t *)params[0], slot);
    if (status != QDF_STATUS_SUCCESS)
        printf("%s: set slot time failed\n", __func__);

    return acfg_to_os_status(status) ;
}

param_info_t wrap_set_erp_params[] = {
    {"vap", PARAM_STRING , "vap name"},
    {"erp", PARAM_STRING , "ERP protection mode"},
};

/**
 * @brief  Wrapper for acfg_set_erp
 *
 * @param params[]
 *
 * @return
 */
int wrap_set_erp(char *params[])
{
    uint32_t erp ;
    uint32_t status = QDF_STATUS_E_FAILURE ;

    get_uint32(params[1], (uint32_t *) &erp);
    status = acfg_set_erp((uint8_t *)params[0], erp);
    if (status != QDF_STATUS_SUCCESS)
        printf("%s: set ERP failed\n", __func__);

    return acfg_to_os_status(status) ;
}

param_info_t wrap_set_regdomain_params[] = {
    {"radio", PARAM_STRING , "radio name"},
    {"regdomain", PARAM_STRING , "regdomain number in decimal"},
};

/**
 * @brief  Wrapper for acfg_set_regdomain
 *
 * @param params[]
 *
 * @return
 */
int wrap_set_regdomain(char *params[])
{
    uint32_t regdomain;
    uint32_t status = QDF_STATUS_E_FAILURE ;

    get_uint32(params[1], (uint32_t *) &regdomain);
    status = acfg_set_regdomain((uint8_t *)params[0], regdomain);
    if (status != QDF_STATUS_SUCCESS)
        printf("%s: set ERP failed\n", __func__);

    return acfg_to_os_status(status) ;
}

/* sample beacon app ie */
static void
acfg_app_ie_init(u_int8_t *frm, uint32_t *ie_len)
{
    u_int8_t vendor_ath[3] = {00,0x03,0x7f};

    *frm++ = 221;   //ID: vendor specific
    *frm++ = 8;     //len
    memcpy(frm, vendor_ath, 3);  //OUI
    frm += 3;
    *frm++ = 1;     //OUI type
    *frm++ = 1;     //Type: advanced capability
    *frm++ = 1;     //subtype
    *frm++ = 1;     //version
    *frm = 0;       //capability

    *ie_len += 10;
}

param_info_t wrap_add_app_ie_params[] = {
    {"vap", PARAM_STRING , "vap name"},
};

int wrap_add_app_ie(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE ;
    uint8_t *ie = NULL, *frm = NULL;
    uint32_t ie_len = 0;

    frm = malloc(1500);
    if(frm==NULL) return QDF_STATUS_E_FAILURE;
    memset(frm, 0, 1500);
    ie = frm;

    /* example of custom beacon app ie */
    acfg_app_ie_init(frm, &ie_len);

    status = acfg_add_app_ie((uint8_t *)params[0], ie, ie_len);
    if (status != QDF_STATUS_SUCCESS){
        printf("%s: set beacon failed\n", __func__);
    }
    free(frm);

    return acfg_to_os_status(status) ;
}

param_info_t wrap_set_ratemask_params[] = {
    {"vap", PARAM_STRING , "vap name"},
    {"preamble", PARAM_STRING , "0 - legacy, 1 - HT, 2 - VHT"},
    {"lower ratemask", PARAM_STRING , "lower 32-bit ratemask"},
    {"higher ratemask", PARAM_STRING , "higher 32-bit ratemask"},
    {"lower_2 ratemask", PARAM_STRING , "lower_2 32-bit ratemask"},
};

int wrap_set_ratemask(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE;
    uint32_t preamble;
    uint32_t mask_lower32, mask_higher32;
    uint32_t mask_lower32_2;

    get_uint32(params[1], (uint32_t *) &preamble);
    get_hex(params[2], &mask_lower32);
    get_hex(params[3], &mask_higher32);
    get_hex(params[4], &mask_lower32_2);
    status = acfg_set_ratemask((uint8_t *)params[0], preamble, mask_lower32, mask_higher32, mask_lower32_2);
    if (status != QDF_STATUS_SUCCESS)
        printf("%s: set ratemask failed\n", __func__);

    return acfg_to_os_status(status) ;
}

/*tx99 example
 *TX:
 *acfg_tool acfg_tx99_tool wifi1 tx tx99 freq 149 chain 1 rate 12 mode vht80_0
 *acfg_tool acfg_tx99_tool wifi1 tx off
 *
 *RX:
 *acfg_tool acfg_tx99_tool wifi1 rx promis freq 149 chain 1 mode vht80_0
 *acfg_tool acfg_tx99_tool wifi1 rx report
*/
param_info_t wrap_tx99_tool_params[] = {
    {"wifi", PARAM_STRING , "wifi radio name"},
};

static int acfg_tx99_data_init(acfg_tx99_data_t* tx99_data, char *params[])
{
    u_int8_t i, param_num = 0 ;
    char **pc = NULL;

    //find out number of parameters
    pc = &params[0];
    while(*pc != NULL)
    {
        param_num++ ;
        pc++ ;
    }

    for(i=0;i<param_num;i++){
        if(strcmp(params[i],"tx")==0){
            //is TX
            SET_TX99_TX(tx99_data->flags);
            ENABLE_TX99_TX(tx99_data->flags);
            if(params[i+1] && strcmp(params[i+1],"off")==0){
                DISABLE_TX99_TX(tx99_data->flags);
            }
        }else if(strcmp(params[i],"rx")==0){
            SET_TX99_RX(tx99_data->flags);
            if(params[i+1] && strcmp(params[i+1],"report")==0){
                SET_TX99_RX_REPORT(tx99_data->flags);
            }
        }else if(strcmp(params[i],"freq")==0){
            if(!params[i+1])
                return -1;
            get_uint32(params[i+1], (uint32_t *) &(tx99_data->freq));
        }else if(strcmp(params[i],"chain")==0){
            if(!params[i+1])
                return -1;
            get_uint32(params[i+1], (uint32_t *) &(tx99_data->chain));
        }else if(strcmp(params[i],"rate")==0){
            if(!params[i+1])
                return -1;
            get_uint32(params[i+1], (uint32_t *) &(tx99_data->rate));
        }else if(strcmp(params[i],"power")==0){
            if(!params[i+1])
                return -1;
            get_uint32(params[i+1], (uint32_t *) &(tx99_data->power));
        }else if(strcmp(params[i],"pattern")==0){
            if(!params[i+1])
                return -1;
            get_uint32(params[i+1], (uint32_t *) &(tx99_data->pattern));
        }else if(strcmp(params[i],"shortguard")==0){
            tx99_data->shortguard = 1;
        }else if(strcmp(params[i],"mode")==0){
            if(!params[i+1])
                return -1;
            memcpy(tx99_data->mode, params[i+1], 16);
        }
    }
    return 0;
}


int wrap_tx99_tool(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE ;
    acfg_tx99_data_t tx99_data = {0};
    if(!params[0]) {
        return -1;
    }
    if(acfg_tx99_data_init(&tx99_data, &params[1])) {
        return -1;
    }

    status = acfg_tx99_tool((uint8_t *)params[0], &tx99_data);
    if (status != QDF_STATUS_SUCCESS){
        printf("%s: tx99 tool failed\n", __func__);
    }

    return acfg_to_os_status(status) ;
}
param_info_t wrap_enable_amsdu_params[] = {
        {"wifi", PARAM_STRING, "radio name"},
        {"mask", PARAM_UINT32 , "amsdu tid mask"},
};

int wrap_enable_amsdu(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE ;
    uint32_t mask;

    get_uint32(params[1], &mask);

    status = acfg_enable_amsdu((uint8_t *)params[0], (uint16_t)mask);

    return acfg_to_os_status(status) ;
}

param_info_t wrap_enable_ampdu_params[] = {
        {"wifi", PARAM_STRING, "radio name"},
        {"mask", PARAM_UINT32 , "ampdu tid mask"},
};

int wrap_enable_ampdu(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE ;
    uint32_t mask;

    get_uint32(params[1], &mask);

    status = acfg_enable_ampdu((uint8_t *)params[0], (uint16_t)mask);

    return acfg_to_os_status(status) ;
}

param_info_t wrap_set_mgmt_retry_limit_params[] = {
    {"radio", PARAM_STRING , "radio name"},
    {"limit", PARAM_UINT32 , "Management retry limit"},
};

int wrap_set_mgmt_retry_limit(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE ;
    uint32_t limit = 0;

    get_uint32(params[1], (uint32_t *) &limit);
    status = acfg_set_mgmt_retry_limit((uint8_t *)params[0], limit);

    return acfg_to_os_status(status);
}


param_info_t wrap_get_mgmt_retry_limit_params[] = {
    {"radio", PARAM_STRING , "radio name"},
};

int wrap_get_mgmt_retry_limit(char *params[])
{
    uint8_t limit = 0;
    uint32_t status = QDF_STATUS_E_FAILURE ;

    status = acfg_get_mgmt_retry_limit((uint8_t *)params[0], &limit);
    if (status != QDF_STATUS_SUCCESS){
        printf("%s get mgmt retry limit failed \n", __func__);
        return acfg_to_os_status(status);
    }
    printf("Current mgmt retry limit is %u \n", limit);

    return acfg_to_os_status(status);
}

param_info_t wrap_tx_power_set_params[] = {
    {"wifi", PARAM_STRING , "wifi radio name"},
    {"mode", PARAM_STRING , "0-5G, 1-2G"},
    {"power", PARAM_STRING , "Power in dBm"},
};

int wrap_set_tx_power(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE ;
    float power;
    uint32_t mode;

    printf("%s[%d] params: %s %s \n", __func__,__LINE__, params[1],params[2]);
    get_uint32(params[1], &mode);
    if(params[2][0] == '/')
        power = atof(params[2]+sizeof(char));
    else
        power = atof(params[2]);

    printf("%s[%d] mode = %d, power = %f\n", __func__,__LINE__, mode, power);
    status = acfg_set_tx_power((uint8_t *)params[0], mode, power);
    return acfg_to_os_status(status) ;
}

param_info_t wrap_cca_threshold_params[] = {
    {"wifi", PARAM_STRING , "wifi radio name"},
    {"cca_threshold", PARAM_STRING , "CCA threshold in dBm"},
};

int wrap_cca_threshold(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE ;
    float threshold;

    printf("%s[%d] params: %s\n", __func__,__LINE__, params[1]);
    if(params[1][0] == '/')
        threshold = atof(params[1]+sizeof(char));
    else
        threshold = atof(params[1]);

    status = acfg_set_cca_threshold((uint8_t *)params[0], threshold);
    return acfg_to_os_status(status) ;
}

param_info_t wrap_get_nf_dbr_dbm_info_params[] = {
    {"wifi", PARAM_STRING , "wifi radio name"},
};

int wrap_get_nf_dbr_dbm_info(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE ;
    status = acfg_get_nf_dbr_dbm_info((uint8_t *)params[0]);
    return acfg_to_os_status(status) ;
}

param_info_t wrap_get_packet_power_info_params[] = {
    {"wifi", PARAM_STRING , "wifi radio name"},
    {"chainmask", PARAM_STRING , "chainmask"},
    {"channel_width", PARAM_STRING , "channel_width"},
    {"rate_flags", PARAM_STRING , "rate mask flags"},
    {"su_mu_ofdma", PARAM_STRING , "su mu ofdma flags"},
    {"nss", PARAM_STRING , "Num of Spacial Stream"},
    {"preamble", PARAM_STRING , "Preamble Mode"},
    {"hw_rate", PARAM_STRING , "Rate Code in MCS"},
};

int wrap_get_packet_power_info(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE ;
    acfg_packet_power_param_t param;

    printf("%s[%d] params: %s %s %s %s %s %s %s\n",
            __func__,__LINE__, params[1],params[2],params[3],
            params[4], params[5], params[6], params[7]);

    get_hex(params[1], (uint32_t *) &param.chainmask);
    get_hex(params[2], (uint32_t *) &param.chan_width);
    get_hex(params[3], (uint32_t *) &param.rate_flags);
    get_hex(params[4], (uint32_t *) &param.su_mu_ofdma);
    get_hex(params[5], (uint32_t *) &param.nss);
    get_hex(params[6], (uint32_t *) &param.preamble);
    get_hex(params[7], (uint32_t *) &param.hw_rate);

    status = acfg_get_packet_power_info((uint8_t *)params[0], &param);
    return acfg_to_os_status(status) ;
}

param_info_t wrap_sens_set_params[] = {
    {"wifi", PARAM_STRING , "wifi radio name"},
    {"sens_level", PARAM_STRING , "Sensitivity level in dBm"},
};

int wrap_set_sens_level(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE ;
    float sens_level;

    printf("%s[%d] params: %s\n", __func__,__LINE__, params[1]);
    if(params[1][0] == '/')
        sens_level = atof(params[1]+sizeof(char));
    else
        sens_level = atof(params[1]);

    status = acfg_set_sens_level((uint8_t *)params[0], sens_level);
    return acfg_to_os_status(status) ;
}


param_info_t wrap_gpio_config_params[] = {
    {"wifi", PARAM_STRING , "wifi radio name"},
    {"gpio_num", PARAM_STRING , "GPIO pin number"},
    {"input", PARAM_STRING , "0-output, 1-input"},
    {"pull_type", PARAM_STRING , "Pull type"},
    {"intr_mode", PARAM_STRING , "Interrrupt mode"},
};

int wrap_gpio_config(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE ;
    uint32_t gpio_num, input, pull_type, intr_mode;
    printf("%s[%d] params: %s %s %s %s\n", __func__,__LINE__, params[1],params[2],params[3],params[4]);

    get_uint32(params[1], &gpio_num);
    get_uint32(params[2], &input);
    get_uint32(params[3], &pull_type);
    get_uint32(params[4], &intr_mode);

    status = acfg_gpio_config((uint8_t *)params[0], gpio_num, input, pull_type, intr_mode);
    return acfg_to_os_status(status) ;
}


param_info_t wrap_gpio_set_params[] = {
    {"wifi", PARAM_STRING , "wifi radio name"},
    {"gpio_num", PARAM_STRING , "GPIO pin number"},
    {"set", PARAM_STRING , "0-unset, 1-set"},
};

int wrap_gpio_set(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE ;
    uint32_t gpio_num, set;
    printf("%s[%d] params: %s %s\n", __func__,__LINE__, params[1],params[2]);

    get_uint32(params[1], &gpio_num);
    get_uint32(params[2], &set);

    status = acfg_gpio_set((uint8_t *)params[0], gpio_num, set);
    return acfg_to_os_status(status) ;
}

param_info_t wrap_set_ctl_table_params[] = {
    {"wifi", PARAM_STRING , "wifi radio name"},
    {"mode", PARAM_STRING , "0-5G, 1-2G"},
    {"ctl_file", PARAM_STRING , "CTL table binary file name"},
};

#define CTL_INFO_SIZE (3 * sizeof(uint32_t))

/**
 * enum acfg_wmi_target_type - type of supported wmi command
 * @ACFG_WMI_TLV_TARGET: tlv based target
 * @ACFG_WMI_NON_TLV_TARGET: non-tlv based target
 *
 */
enum wmi_target_type {
    ACFG_WMI_TGT_TYPE_INVAILD = -1,
    ACFG_WMI_TGT_TYPE_TLV,
    ACFG_WMI_TGT_TYPE_NON_TLV
};

int wrap_set_ctl_table(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE ;
    uint32_t mode, len;
    uint32_t seq, total_len;
    uint32_t offset = 0;
    uint32_t *ptr_ctl_buf = NULL;
    enum wmi_target_type wmi_tgt_type = ACFG_WMI_TGT_TYPE_INVAILD;
    unsigned char clt_buf[MAX_CTL_SIZE];
    acfg_os_req_t req = {.cmd = ACFG_REQ_GET_TLV_TYPE};
    FILE *fp;

    printf("%s[%d] params: %s %s\n", __func__,__LINE__, params[1],params[2]);

    get_uint32(params[1], &mode);

    if (QDF_STATUS_SUCCESS != acfg_os_send_req((uint8_t *)params[0], &req)) {
        printf("%s:Get for ACFG_REQ_GET_TLV_TYPE failed\n", __func__);
        return -1;
    }

    wmi_tgt_type = *(enum wmi_target_type *)req.data;
    if (wmi_tgt_type == ACFG_WMI_TGT_TYPE_INVAILD) {
        printf("%s:Invalid target type\n", __func__);
        return -1;
    }

    // open file and read the table
    if((fp = fopen(params[2], "rb"))== NULL)
    {
        printf("Can't open input file %s\n", params[2]);
        return QDF_STATUS_E_FAILURE;
    }

    if (wmi_tgt_type == ACFG_WMI_TGT_TYPE_NON_TLV) {
        len = fread(clt_buf, 1, MAX_CTL_SIZE, fp);
        status = acfg_set_ctl_table((uint8_t *)params[0], mode, len, clt_buf);
    } else if (wmi_tgt_type == ACFG_WMI_TGT_TYPE_TLV) {
        fseek(fp, 0, SEEK_END);
        total_len = ftell(fp);
        fseek(fp , 0, SEEK_SET);
        seq = 0;

        for (; offset < total_len; seq++) {
            ptr_ctl_buf = (uint32_t *)clt_buf;
            len = fread(&clt_buf[12], 1, MAX_CTL_SIZE - CTL_INFO_SIZE, fp);
            offset = ftell(fp);
            if (len != (MAX_CTL_SIZE - CTL_INFO_SIZE)) {
                len = total_len - ((MAX_CTL_SIZE - CTL_INFO_SIZE) * seq);
                *ptr_ctl_buf = seq;
                ptr_ctl_buf++;
                *ptr_ctl_buf = 1;
                ptr_ctl_buf++;
                *ptr_ctl_buf = total_len;
                status = acfg_set_ctl_table((uint8_t *)params[0], mode, len + CTL_INFO_SIZE, clt_buf);
            } else {
                *ptr_ctl_buf = seq;
                ptr_ctl_buf++;
                *ptr_ctl_buf = 0;
                ptr_ctl_buf++;
                *ptr_ctl_buf = total_len;
                status = acfg_set_ctl_table((uint8_t *)params[0], mode, len + CTL_INFO_SIZE, clt_buf);
            }
            if (status != QDF_STATUS_SUCCESS)
                break;
        }
    } else {
        printf("%s: set CTL is not supported for this[%d] wmi target type",
               __func__, wmi_tgt_type);
    }

    fclose(fp);
    return acfg_to_os_status(status) ;
}

param_info_t wrap_send_raw_pkt_params[] = {
    {"vap", PARAM_STRING , "vap name"},
    {"type", PARAM_STRING , "mgmt/data"},
    {"channel", PARAM_STRING , "channel to tx the pkt"},
    {"nss", PARAM_STRING , "nss of the pkt"},
    {"preamble", PARAM_STRING , "preamble of the pkt"},
    {"mcs", PARAM_STRING , "mcs of the pkt"},
    {"retry", PARAM_STRING , "no of retries of the pkt"},
    {"power", PARAM_STRING , "tx power of the pkt"},
    {"mac1", PARAM_STRING , "mac address 1"},
    {"mac2", PARAM_STRING , "mac address 2"},
    {"mac3", PARAM_STRING , "mac address 3"},
    {"scan dur", PARAM_STRING , "scan duration"},
    {"encrypt", PARAM_STRING , "encrypt"},
};

#if QCA_SUPPORT_GPR
param_info_t wrap_start_gpr_params[] = {
    {"vap", PARAM_STRING , "vap name"},
    {"period", PARAM_STRING , "periodicity of sending gpr pkt"},
    {"nss", PARAM_STRING , "nss of the pkt"},
    {"preamble", PARAM_STRING , "preamble of the pkt"},
    {"mcs", PARAM_STRING , "mcs of the pkt"},
};

param_info_t wrap_send_gpr_cmd_params[] = {
    {"vap", PARAM_STRING , "vap name"},
    {"command", PARAM_STRING , "command"},
};
#endif

#define IEEE80211_ADDR_COPY(dst,src)    memcpy(dst, src, 6)
int wrap_send_raw_pkt(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE ;
    uint8_t *pkt = NULL;
    uint16_t *pi_dur = NULL;
    struct ieee80211_qosframe_addr4 *qwh;
    struct ieee80211_frame *wh;
    uint32_t tmp[ACFG_MACADDR_LEN] = {0};
    uint8_t mac[ACFG_MACADDR_LEN];
    uint8_t mac2[ACFG_MACADDR_LEN];
    uint8_t mac3[ACFG_MACADDR_LEN];
    uint32_t type, chan, frmlen;
    uint32_t retry, power;
    uint32_t nss, preamble, mcs, encrypt;
    int32_t scan_dur;
    char *frm;
    int i;

    get_uint32(params[1], (uint32_t *) &type);
    get_uint32(params[2], (uint32_t *) &chan);
    get_uint32(params[3], (uint32_t *) &nss);
    get_uint32(params[4], (uint32_t *) &preamble);
    get_uint32(params[5], (uint32_t *) &mcs);
    get_uint32(params[6], (uint32_t *) &retry);
    get_uint32(params[7], (uint32_t *) &power);
    get_int32(params[11], (int32_t *) &scan_dur);
    get_uint32(params[12], (uint32_t *) &encrypt);

    if (scan_dur < 0) {
        printf("%s: Dwell time cannot be negative. Please input valid Dwell time\n", __func__);
        return QDF_STATUS_E_INVAL;
    }

    if((strlen(params[8]) == ACFG_MAC_STR_LEN)) {
            sscanf(params[8],"%x:%x:%x:%x:%x:%x",(unsigned int *)&tmp[0],\
                    (unsigned int *)&tmp[1],\
                    (unsigned int *)&tmp[2],\
                    (unsigned int *)&tmp[3],\
                    (unsigned int *)&tmp[4],\
                    (unsigned int *)&tmp[5] );
    } else {
         return QDF_STATUS_E_INVAL;
    }

    for (i = 0; i < ACFG_MACADDR_LEN; i++)
        mac[i] = tmp[i];

    if((strlen(params[9]) == ACFG_MAC_STR_LEN)) {
            sscanf(params[9],"%x:%x:%x:%x:%x:%x",(unsigned int *)&tmp[0],\
                    (unsigned int *)&tmp[1],\
                    (unsigned int *)&tmp[2],\
                    (unsigned int *)&tmp[3],\
                    (unsigned int *)&tmp[4],\
                    (unsigned int *)&tmp[5] );
    }

    for (i = 0; i < ACFG_MACADDR_LEN; i++)
        mac2[i] = tmp[i];

    if((strlen(params[10]) == ACFG_MAC_STR_LEN)) {
            sscanf(params[10],"%x:%x:%x:%x:%x:%x",(unsigned int *)&tmp[0],\
                    (unsigned int *)&tmp[1],\
                    (unsigned int *)&tmp[2],\
                    (unsigned int *)&tmp[3],\
                    (unsigned int *)&tmp[4],\
                    (unsigned int *)&tmp[5] );
    }

    for (i = 0; i < ACFG_MACADDR_LEN; i++)
        mac3[i] = tmp[i];

    pkt =  malloc(1500);
    if(pkt==NULL) return QDF_STATUS_E_FAILURE;
    memset(pkt, 0, 1500);

    if (type != 1 ) {
        qwh = (struct ieee80211_qosframe_addr4 *)pkt;

        qwh->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_DATA;
        if (encrypt) {
            qwh->i_fc[1] = IEEE80211_FC1_DIR_DSTODS | IEEE80211_FC1_WEP;
        }
        else {
            qwh->i_fc[1] = IEEE80211_FC1_DIR_DSTODS;
        }

        IEEE80211_ADDR_COPY(qwh->i_addr1, mac3);
        IEEE80211_ADDR_COPY(qwh->i_addr2, mac);
        IEEE80211_ADDR_COPY(qwh->i_addr3, mac3);
        IEEE80211_ADDR_COPY(qwh->i_addr4, mac2);
        pi_dur = (u_int16_t *)&qwh->i_dur[0];
	*(pi_dur) = 0x0;
        pi_dur = (u_int16_t *)&qwh->i_seq[0];
	*(pi_dur) = 0x0;

        qwh->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_DATA |
            IEEE80211_FC0_SUBTYPE_QOS;

        qwh->i_qos[0] = IEEE80211_QOS_TID;
        frm = (char*)(qwh +1);
        *frm++ = 0xaa;        *frm++ = 0xaa;        *frm++ = 0x03;        *frm++ = 0x00;
        *frm++ = 0x0b;        *frm++ = 0x85;        *frm++ = 0xcc;        *frm++ = 0xcd;
        *frm++ = 0x01;        *frm++ = 0x1b;        *frm++ = 0xc0;        *frm++ = 0xa8;
        *frm++ = 0x01;        *frm++ = 0x0a;        *frm++ = 0x01;        *frm++ = 0xf4;
        *frm++ = 0x00;        *frm++ = 0x27;        *frm++ = 0xc0;        *frm++ = 0xa8;
        *frm++ = 0x01;        *frm++ = 0x0a;        *frm++ = 0x24;        *frm++ = 0x17;
        *frm++ = 0x01;        *frm++ = 0xa1;        *frm++ = 0x07;        *frm++ = 0x17;
        *frm++ = 0x17;        *frm++ = 0x00;        *frm++ = 0xb1;        *frm++ = 0x98;
        *frm++ = 0x83;        *frm++ = 0xfa;        *frm++ = 0xa8;        *frm++ = 0x40;
        *frm++ = 0x86;        *frm++ = 0x74;        *frm++ = 0x9b;        *frm++ = 0x84;
        *frm++ = 0x59;        *frm++ = 0x79;        *frm++ = 0x07;        *frm++ = 0x01;
        *frm++ = 0x58;        *frm++ = 0xa4;

        frmlen = 78;
    } else {
        wh = (struct ieee80211_frame *)pkt;
        wh->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_MGT | IEEE80211_FC0_SUBTYPE_DEAUTH;
        wh->i_fc[1] = IEEE80211_FC1_DIR_NODS;
        IEEE80211_ADDR_COPY(wh->i_addr1, mac);
        IEEE80211_ADDR_COPY(wh->i_addr2, mac2);
        IEEE80211_ADDR_COPY(wh->i_addr3, mac3);

        pi_dur = (u_int16_t *)&wh->i_dur[0];
	*(pi_dur) = 0x0;
        frm = (char *)&wh[1];
        *(u_int16_t *)frm = 0;
        frm += 2;
        frmlen = sizeof(struct ieee80211_frame) + 2;

    }
    status = acfg_send_raw_pkt((uint8_t *)params[0], pkt, frmlen, type, chan, nss, preamble, mcs, retry, power, scan_dur);
    if (status != QDF_STATUS_SUCCESS){
        printf("%s: send raw pkt failed\n", __func__);
    }
    free(pkt);

    return acfg_to_os_status(status) ;
}

#if QCA_SUPPORT_GPR
int wrap_send_gpr_cmd(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE;
    uint8_t command;

    get_uint32(params[1], (uint32_t *) &command);
    if (command < 0 || command > 3) {
        printf("%s: Supported command are:-\n 0-Disable\n 1-Enable\n 2-Print stats\n 3-Clear Stats\n", __func__);
        return QDF_STATUS_E_INVAL;
    }

    status = acfg_send_gpr_cmd((uint8_t *)params[0], command);
    if (status != QDF_STATUS_SUCCESS) {
        printf("%s: send_gpr_cmd failed\n", __func__);
    }
    return acfg_to_os_status(status) ;
}

int wrap_start_gpr(char *params[])
{
    struct ieee80211_frame *wh;
    uint32_t status = QDF_STATUS_E_FAILURE ;
    uint32_t frmlen, period;
    uint32_t nss, preamble, mcs;
    uint8_t *pkt = NULL;
    char *frm;

    get_uint32(params[1], (uint32_t *) &period);
    get_uint32(params[2], (uint32_t *) &nss);
    get_uint32(params[3], (uint32_t *) &preamble);
    get_uint32(params[4], (uint32_t *) &mcs);

    if ((int)period > DEFAULT_MAX_GRATITOUS_PROBE_RESP_PERIOD || (int)period < DEFAULT_MIN_GRATITOUS_PROBE_RESP_PERIOD) {
        printf("%s: Set value in range of 10msec - 3600000msec \n", __func__);
        return status;
    }
    pkt =  malloc(1500);
    if(pkt==NULL)
        return QDF_STATUS_E_FAILURE;
    memset(pkt, 0, 1500);

    wh = (struct ieee80211_frame *)pkt;
    /* 2 Bytes FC */
    wh->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_MGT | IEEE80211_FC0_SUBTYPE_PROBE_RESP;
    wh->i_fc[1] = IEEE80211_FC1_DIR_NODS;

    /* Creating a test payload for GPR */
    frm = (char*)(wh +1);

    /* 12 Bytes Fixed (8 Timestamp + 2 Beacon Interval + 2 Capabilities) */
    *frm++ = 0xb5;*frm++ = 0xb1;*frm++ = 0x56;*frm++ = 0x1b;*frm++ = 0x01;*frm++ = 0x00;*frm++ = 0x00;*frm++ = 0x00;
    *frm++ = 0x64;*frm++ = 0x00;*frm++ = 0x01;*frm++ = 0x05;

    /* 305 Bytes Tagged Parameters */
    *frm++ = 0x00;*frm++ = 0x10;*frm++ = 0x41;*frm++ = 0x43;*frm++ = 0x46;*frm++ = 0x47;*frm++ = 0x5f;*frm++ = 0x31;
    *frm++ = 0x30;*frm++ = 0x2e;*frm++ = 0x34;*frm++ = 0x2e;*frm++ = 0x37;*frm++ = 0x5f;*frm++ = 0x39;*frm++ = 0x34;
    *frm++ = 0x39;*frm++ = 0x36;*frm++ = 0x01;*frm++ = 0x08;*frm++ = 0x8c;*frm++ = 0x12;*frm++ = 0x98;*frm++ = 0x24;
    *frm++ = 0xb0;*frm++ = 0x48;*frm++ = 0x60;*frm++ = 0x6c;*frm++ = 0x03;*frm++ = 0x01;*frm++ = 0x24;*frm++ = 0x07;
    *frm++ = 0x4e;*frm++ = 0x55;*frm++ = 0x53;*frm++ = 0x20;*frm++ = 0x24;*frm++ = 0x01;*frm++ = 0x1e;*frm++ = 0x28;
    *frm++ = 0x01;*frm++ = 0x1e;*frm++ = 0x2c;*frm++ = 0x01;*frm++ = 0x1e;*frm++ = 0x30;*frm++ = 0x01;*frm++ = 0x1e;
    *frm++ = 0x34;*frm++ = 0x01;*frm++ = 0x18;*frm++ = 0x38;*frm++ = 0x01;*frm++ = 0x18;*frm++ = 0x3c;*frm++ = 0x01;
    *frm++ = 0x18;*frm++ = 0x40;*frm++ = 0x01;*frm++ = 0x18;*frm++ = 0x64;*frm++ = 0x01;*frm++ = 0x18;*frm++ = 0x68;
    *frm++ = 0x01;*frm++ = 0x18;*frm++ = 0x6c;*frm++ = 0x01;*frm++ = 0x18;*frm++ = 0x70;*frm++ = 0x01;*frm++ = 0x18;
    *frm++ = 0x74;*frm++ = 0x01;*frm++ = 0x18;*frm++ = 0x78;*frm++ = 0x01;*frm++ = 0x18;*frm++ = 0x7c;*frm++ = 0x01;
    *frm++ = 0x18;*frm++ = 0x80;*frm++ = 0x01;*frm++ = 0x18;*frm++ = 0x84;*frm++ = 0x01;*frm++ = 0x18;*frm++ = 0x88;
    *frm++ = 0x01;*frm++ = 0x18;*frm++ = 0x8c;*frm++ = 0x01;*frm++ = 0x18;*frm++ = 0x90;*frm++ = 0x01;*frm++ = 0x18;
    *frm++ = 0x95;*frm++ = 0x01;*frm++ = 0x1e;*frm++ = 0x99;*frm++ = 0x01;*frm++ = 0x1e;*frm++ = 0x9d;*frm++ = 0x01;
    *frm++ = 0x1e;*frm++ = 0xa1;*frm++ = 0x01;*frm++ = 0x1e;*frm++ = 0xa5;*frm++ = 0x01;*frm++ = 0x1e;*frm++ = 0x2d;
    *frm++ = 0x1a;*frm++ = 0x8f;*frm++ = 0x09;*frm++ = 0x03;*frm++ = 0xff;*frm++ = 0xff;*frm++ = 0xff;*frm++ = 0xff;
    *frm++ = 0x00;*frm++ = 0x00;*frm++ = 0x00;*frm++ = 0x00;*frm++ = 0x00;*frm++ = 0x00;*frm++ = 0x00;*frm++ = 0x00;
    *frm++ = 0x01;*frm++ = 0x00;*frm++ = 0x00;*frm++ = 0x00;*frm++ = 0x00;*frm++ = 0x00;*frm++ = 0x00;*frm++ = 0x00;
    *frm++ = 0x00;*frm++ = 0x00;*frm++ = 0x00;*frm++ = 0x3d;*frm++ = 0x16;*frm++ = 0x24;*frm++ = 0x05;*frm++ = 0x00;
    *frm++ = 0x00;*frm++ = 0x00;*frm++ = 0x00;*frm++ = 0x00;*frm++ = 0x00;*frm++ = 0x00;*frm++ = 0x00;*frm++ = 0x00;
    *frm++ = 0x00;*frm++ = 0x00;*frm++ = 0x00;*frm++ = 0x00;*frm++ = 0x00;*frm++ = 0x00;*frm++ = 0x00;*frm++ = 0x00;
    *frm++ = 0x00;*frm++ = 0x00;*frm++ = 0x00;*frm++ = 0x7f;*frm++ = 0x08;*frm++ = 0x04;*frm++ = 0x00;*frm++ = 0x0f;
    *frm++ = 0x00;*frm++ = 0x00;*frm++ = 0x00;*frm++ = 0x00;*frm++ = 0x40;*frm++ = 0xbf;*frm++ = 0x0c;*frm++ = 0xf2;
    *frm++ = 0x79;*frm++ = 0x8b;*frm++ = 0x33;*frm++ = 0xaa;*frm++ = 0xff;*frm++ = 0x00;*frm++ = 0x00;*frm++ = 0xaa;
    *frm++ = 0xff;*frm++ = 0x00;*frm++ = 0x20;*frm++ = 0xc0;*frm++ = 0x05;*frm++ = 0x01;*frm++ = 0x2a;*frm++ = 0x00;
    *frm++ = 0xfc;*frm++ = 0xff;*frm++ = 0xc3;*frm++ = 0x05;*frm++ = 0x03;*frm++ = 0x3c;*frm++ = 0x3c;*frm++ = 0x3c;
    *frm++ = 0x3c;*frm++ = 0xdd;*frm++ = 0x18;*frm++ = 0x00;*frm++ = 0x50;*frm++ = 0xf2;*frm++ = 0x02;*frm++ = 0x01;
    *frm++ = 0x01;*frm++ = 0x80;*frm++ = 0x00;*frm++ = 0x03;*frm++ = 0xa4;*frm++ = 0x00;*frm++ = 0x00;*frm++ = 0x27;
    *frm++ = 0xa4;*frm++ = 0x00;*frm++ = 0x00;*frm++ = 0x42;*frm++ = 0x43;*frm++ = 0x5e;*frm++ = 0x00;*frm++ = 0x62;
    *frm++ = 0x32;*frm++ = 0x2f;*frm++ = 0x00;*frm++ = 0xdd;*frm++ = 0x09;*frm++ = 0x00;*frm++ = 0x03;*frm++ = 0x7f;
    *frm++ = 0x01;*frm++ = 0x01;*frm++ = 0x00;*frm++ = 0x00;*frm++ = 0xff;*frm++ = 0x7f;*frm++ = 0xdd;*frm++ = 0x16;
    *frm++ = 0x8c;*frm++ = 0xfd;*frm++ = 0xf0;*frm++ = 0x04;*frm++ = 0x00;*frm++ = 0x00;*frm++ = 0x49;*frm++ = 0x4c;
    *frm++ = 0x51;*frm++ = 0x03;*frm++ = 0x02;*frm++ = 0x09;*frm++ = 0x72;*frm++ = 0x01;*frm++ = 0x8c;*frm++ = 0x16;
    *frm++ = 0x00;*frm++ = 0x00;*frm++ = 0x46;*frm++ = 0x00;*frm++ = 0x00;*frm++ = 0x00;*frm++ = 0xdd;*frm++ = 0x1f;
    *frm++ = 0x8c;*frm++ = 0xfd;*frm++ = 0xf0;*frm++ = 0x00;*frm++ = 0x00;*frm++ = 0x01;*frm++ = 0x01;*frm++ = 0x00;
    *frm++ = 0x00;*frm++ = 0x01;*frm++ = 0x00;*frm++ = 0x00;*frm++ = 0x00;*frm++ = 0x00;*frm++ = 0x00;*frm++ = 0x00;
    *frm++ = 0x00;*frm++ = 0xff;*frm++ = 0xff;*frm++ = 0x8c;*frm++ = 0xfd;*frm++ = 0xf0;*frm++ = 0x02;*frm++ = 0x10;
    *frm++ = 0x71;*frm++ = 0x00;*frm++ = 0x00;*frm++ = 0x00;*frm++ = 0x00;*frm++ = 0x00;*frm++ = 0x00;*frm++ = 0xdd;
    *frm++ = 0x08;*frm++ = 0x8c;*frm++ = 0xfd;*frm++ = 0xf0;*frm++ = 0x01;*frm++ = 0x01;*frm++ = 0x02;*frm++ = 0x01;
    *frm++ = 0x00;

    frmlen = 345; // 2FC+2Duratoin+18(ADD*3)+2(seq_ctrl)+ 12 Fixed payload + 305 tagged payload + 4(FCS)

    status = acfg_start_gpr((uint8_t *)params[0], pkt, frmlen, period, nss, preamble, mcs);
    if (status != QDF_STATUS_SUCCESS){
        printf("%s: send gpr pkt failed\n", __func__);
    }
    free(pkt);

    return acfg_to_os_status(status) ;

}
#endif

int wrap_send_raw_multi(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE ;
    uint8_t *pkt = NULL;
    uint16_t *pi_dur = NULL;
    struct ieee80211_qosframe_addr4 *qwh;
    struct ieee80211_frame *wh;
    uint32_t tmp[ACFG_MACADDR_LEN] = {0};
    uint8_t mac[ACFG_MACADDR_LEN];
    uint8_t mac2[ACFG_MACADDR_LEN];
    uint8_t mac3[ACFG_MACADDR_LEN];
    uint32_t type, chan, frmlen;
    uint32_t retry, power;
    uint32_t nss, preamble, mcs, scan_dur, encrypt;
    char *frm;
    int i;

    get_uint32(params[1], (uint32_t *) &type);
    get_uint32(params[2], (uint32_t *) &chan);
    get_uint32(params[3], (uint32_t *) &nss);
    get_uint32(params[4], (uint32_t *) &preamble);
    get_uint32(params[5], (uint32_t *) &mcs);
    get_uint32(params[6], (uint32_t *) &retry);
    get_uint32(params[7], (uint32_t *) &power);
    get_uint32(params[11], (uint32_t *) &scan_dur);
    get_uint32(params[12], (uint32_t *) &encrypt);

    if((strlen(params[8]) == ACFG_MAC_STR_LEN)) {
            sscanf(params[8],"%x:%x:%x:%x:%x:%x",(unsigned int *)&tmp[0],\
                    (unsigned int *)&tmp[1],\
                    (unsigned int *)&tmp[2],\
                    (unsigned int *)&tmp[3],\
                    (unsigned int *)&tmp[4],\
                    (unsigned int *)&tmp[5] );
    } else {
        return QDF_STATUS_E_INVAL;
    }

    for (i = 0; i < ACFG_MACADDR_LEN; i++)
        mac[i] = tmp[i];

    if((strlen(params[9]) == ACFG_MAC_STR_LEN)) {
            sscanf(params[9],"%x:%x:%x:%x:%x:%x",(unsigned int *)&tmp[0],\
                    (unsigned int *)&tmp[1],\
                    (unsigned int *)&tmp[2],\
                    (unsigned int *)&tmp[3],\
                    (unsigned int *)&tmp[4],\
                    (unsigned int *)&tmp[5] );
    }

    for (i = 0; i < ACFG_MACADDR_LEN; i++)
        mac2[i] = tmp[i];

    if((strlen(params[10]) == ACFG_MAC_STR_LEN)) {
            sscanf(params[10],"%x:%x:%x:%x:%x:%x",(unsigned int *)&tmp[0],\
                    (unsigned int *)&tmp[1],\
                    (unsigned int *)&tmp[2],\
                    (unsigned int *)&tmp[3],\
                    (unsigned int *)&tmp[4],\
                    (unsigned int *)&tmp[5] );
    }

    for (i = 0; i < ACFG_MACADDR_LEN; i++)
        mac3[i] = tmp[i];

    pkt =  malloc(1500);
    if(pkt==NULL) return QDF_STATUS_E_FAILURE;
    memset(pkt, 0, 1500);

    if (type != 1 ) {
        qwh = (struct ieee80211_qosframe_addr4 *)pkt;

        qwh->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_DATA;
        if (encrypt) {
            qwh->i_fc[1] = IEEE80211_FC1_DIR_DSTODS | IEEE80211_FC1_WEP;
        }
        else {
            qwh->i_fc[1] = IEEE80211_FC1_DIR_DSTODS;
        }

        IEEE80211_ADDR_COPY(qwh->i_addr1, mac3);
        IEEE80211_ADDR_COPY(qwh->i_addr2, mac);
        IEEE80211_ADDR_COPY(qwh->i_addr3, mac3);
        IEEE80211_ADDR_COPY(qwh->i_addr4, mac2);
        pi_dur = (u_int16_t *)&qwh->i_dur[0];
	*(pi_dur) = 0x0;
        pi_dur = (u_int16_t *)&qwh->i_seq[0];
	*(pi_dur) = 0x0;

        qwh->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_DATA |
            IEEE80211_FC0_SUBTYPE_QOS;

        qwh->i_qos[0] = IEEE80211_QOS_TID;
        frm = (char*)(qwh +1);
        *frm++ = 0xaa;        *frm++ = 0xaa;        *frm++ = 0x03;        *frm++ = 0x00;
        *frm++ = 0x0b;        *frm++ = 0x85;        *frm++ = 0xcc;        *frm++ = 0xcd;
        *frm++ = 0x01;        *frm++ = 0x1b;        *frm++ = 0xc0;        *frm++ = 0xa8;
        *frm++ = 0x01;        *frm++ = 0x0a;        *frm++ = 0x01;        *frm++ = 0xf4;
        *frm++ = 0x00;        *frm++ = 0x27;        *frm++ = 0xc0;        *frm++ = 0xa8;
        *frm++ = 0x01;        *frm++ = 0x0a;        *frm++ = 0x24;        *frm++ = 0x17;
        *frm++ = 0x01;        *frm++ = 0xa1;        *frm++ = 0x07;        *frm++ = 0x17;
        *frm++ = 0x17;        *frm++ = 0x00;        *frm++ = 0xb1;        *frm++ = 0x98;
        *frm++ = 0x83;        *frm++ = 0xfa;        *frm++ = 0xa8;        *frm++ = 0x40;
        *frm++ = 0x86;        *frm++ = 0x74;        *frm++ = 0x9b;        *frm++ = 0x84;
        *frm++ = 0x59;        *frm++ = 0x79;        *frm++ = 0x07;        *frm++ = 0x01;
        *frm++ = 0x58;        *frm++ = 0xa4;

        frmlen = 78;
    } else {
        wh = (struct ieee80211_frame *)pkt;
        wh->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_MGT | IEEE80211_FC0_SUBTYPE_DEAUTH;
        wh->i_fc[1] = IEEE80211_FC1_DIR_NODS;
        IEEE80211_ADDR_COPY(wh->i_addr1, mac);
        IEEE80211_ADDR_COPY(wh->i_addr2, mac2);
        IEEE80211_ADDR_COPY(wh->i_addr3, mac3);

        pi_dur = (u_int16_t *)&wh->i_dur[0];
	*(pi_dur) = 0x0;
        frm = (char *)&wh[1];
        *(u_int16_t *)frm = 0;
        frm += 2;
        frmlen = sizeof(struct ieee80211_frame) + 2;

    }
    status = acfg_send_raw_multi((uint8_t *)params[0], pkt, frmlen, type, chan, nss, preamble, mcs, retry, power, scan_dur);
    if (status != QDF_STATUS_SUCCESS){
        printf("%s: send raw pkt failed\n", __func__);
    }
    free(pkt);

    return acfg_to_os_status(status) ;
}

param_info_t wrap_set_op_support_rates_params[] = {
    {"radio", PARAM_STRING , "radio name"},
};

static void acfg_rateset_init(acfg_rateset_t  *rs, char *params[])
{
    u_int8_t i, param_num = 0 ;
    char **pc = NULL;
    uint32_t tmp = 0;

    //find out number of parameters
    pc = &params[0];
    while(*pc != NULL)
    {
        param_num++ ;
        pc++ ;
    }

    if(param_num>=ACFG_MAX_RATE_SIZE){
        printf("Too many rates passed!\n");
        return;
    }

    for(i=0;i<param_num;i++){
        get_uint32(params[i], &tmp);
        rs->rs_rates[i] = (uint8_t)tmp;
    }
    rs->rs_nrates = param_num;
#if DEBUG_RATES
    printf("Rates init done, rates are: \n");
    for(i=0;i<rs->rs_nrates;i++){
        printf("%d ",rs->rs_rates[i]);
    }
    printf("\n");
#endif
}

int wrap_set_op_support_rates(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE ;
    acfg_rateset_t  rs;

    acfg_rateset_init(&rs, &params[1]);
    status = acfg_set_op_support_rates((uint8_t *)params[0], rs.rs_rates, rs.rs_nrates);
    if (status != QDF_STATUS_SUCCESS){
        printf("%s: one of the rates not valid for %s ?\n", __func__,(uint8_t *)params[0]);
        return acfg_to_os_status(status);
    }

    return acfg_to_os_status(status);
}

void print_phymodes(void)
{
#define PHYMODE_NUM  16
    char *ieee80211_phymode[PHYMODE_NUM]= {
        "IEEE80211_MODE_11A              = 1,     5GHz, OFDM",
        "IEEE80211_MODE_11B              = 2,     2GHz, CCK",
        "IEEE80211_MODE_11G              = 3,     2GHz, OFDM",
        "IEEE80211_MODE_11NA_HT20        = 7,     5Ghz, HT20",
        "IEEE80211_MODE_11NG_HT20        = 8,     2Ghz, HT20",
        "IEEE80211_MODE_11NA_HT40PLUS    = 9,     5Ghz, HT40 (ext ch +1)",
        "IEEE80211_MODE_11NA_HT40MINUS   = 10,    5Ghz, HT40 (ext ch -1)",
        "IEEE80211_MODE_11NG_HT40PLUS    = 11,    2Ghz, HT40 (ext ch +1)",
        "IEEE80211_MODE_11NG_HT40MINUS   = 12,    2Ghz, HT40 (ext ch -1)",
        "IEEE80211_MODE_11NG_HT40        = 13,    2Ghz, Auto HT40",
        "IEEE80211_MODE_11NA_HT40        = 14,    5Ghz, Auto HT40",
        "IEEE80211_MODE_11AC_VHT20       = 15,    5Ghz, VHT20 ",
        "IEEE80211_MODE_11AC_VHT40PLUS   = 16,    5Ghz, VHT40 (Ext ch +1) ",
        "IEEE80211_MODE_11AC_VHT40MINUS  = 17,    5Ghz  VHT40 (Ext ch -1) ",
        "IEEE80211_MODE_11AC_VHT40       = 18,    5Ghz, VHT40 ",
        "IEEE80211_MODE_11AC_VHT80       = 19,    5Ghz, VHT80 "};
    uint8_t i;

    printf("All supported phymode codes are: \n");
    for(i=0; i<PHYMODE_NUM; i++){
        printf("%s\n",ieee80211_phymode[i]);
    }
}

param_info_t wrap_get_radio_supported_rates_params[] = {
    {"radio", PARAM_STRING , "radio name"},
    {"mode", PARAM_UINT32 , "phymode"},
};

int wrap_get_radio_supported_rates(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE ;
    acfg_rateset_t  rs;
    acfg_phymode_t  phymode;
    uint8_t  i, rate_code;
    char print_str[8];

    get_uint32(params[1], &phymode);
    if(phymode<1 || phymode>19){
        print_phymodes();
        return acfg_to_os_status(status);
    }

    memset(&rs, 0, sizeof(acfg_rateset_t));
    rs.rs_nrates = sizeof(rs.rs_rates)/sizeof(uint8_t);

    status = acfg_get_radio_supported_rates((uint8_t *)params[0], &rs, phymode);
    if (status != QDF_STATUS_SUCCESS || rs.rs_nrates==0){
        print_phymodes();
        return acfg_to_os_status(status);
    }

    printf("For phymode %s, all legacy rate codes supported by %s are:\n",params[1],params[0]);
    for(i=0; i<rs.rs_nrates; i++){
        rate_code = rs.rs_rates[i];
        printf("%d ",rate_code);
    }
    printf("\n");


    printf("The actual rates are:\n");
    for(i=0; i<rs.rs_nrates; i++){
        rate_code = rs.rs_rates[i];
        if(rate_code&IEEE80211_RATE_BASIC){
            if((rate_code&ACFG_RATE_VAL)==11){
                snprintf(print_str,sizeof(print_str),"%s%s","5.5","(B)");
            }else{
                snprintf(print_str,sizeof(print_str),"%d%s",((rate_code&ACFG_RATE_VAL)/2),"(B)");
            }
        }else{
            snprintf(print_str,sizeof(print_str),"%d",((rate_code&ACFG_RATE_VAL)/2));
        }
        printf("%s ",print_str);
    }
    printf("\n");

    return acfg_to_os_status(status);
}

param_info_t wrap_get_beacon_supported_rates_params[] = {
    {"VAP", PARAM_STRING , "VAP name"},
};

int wrap_get_beacon_supported_rates(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE ;
    acfg_rateset_t  rs;
    uint8_t  i, rate_code;
    char print_str[8];

    memset(&rs, 0, sizeof(acfg_rateset_t));
    rs.rs_nrates = sizeof(rs.rs_rates)/sizeof(uint8_t);

    status = acfg_get_beacon_supported_rates((uint8_t *)params[0], &rs);
    if (status != QDF_STATUS_SUCCESS || rs.rs_nrates==0){
        return acfg_to_os_status(status);
    }

    printf("The supported rates in beacon are:\n");
    for(i=0; i<rs.rs_nrates; i++){
        rate_code = rs.rs_rates[i];
        printf("%d ",rate_code);
    }
    printf("\n");


    printf("The actual rates are:\n");
    for(i=0; i<rs.rs_nrates; i++){
        rate_code = rs.rs_rates[i];
        if(rate_code&IEEE80211_RATE_BASIC){
            if((rate_code&ACFG_RATE_VAL)==11){
                snprintf(print_str,sizeof(print_str),"%s%s","5.5","(B)");
            }else{
                snprintf(print_str,sizeof(print_str),"%d%s",((rate_code&ACFG_RATE_VAL)/2),"(B)");
            }
        }else{
            snprintf(print_str,sizeof(print_str),"%d",((rate_code&ACFG_RATE_VAL)/2));
        }
        printf("%s ",print_str);
    }
    printf("\n");

    return acfg_to_os_status(status);
}


param_info_t wrap_set_mu_whtlist_params[] = {
        {"vap", PARAM_STRING , "vap name"},
        {"macadr", PARAM_STRING, "client mac address"},
        {"tidmask", PARAM_STRING, "tid mask"},
};

int wrap_set_mu_whtlist(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE ;
    uint8_t   *vap;
    uint8_t   mac[6];
    uint32_t  tidmask;

    vap = (uint8_t *)params[0] ;
    acfg_mac_str_to_octet((uint8_t*)params[1], mac);
    get_hex(params[2], &tidmask);

    status = acfg_set_mu_whtlist(vap, mac, (uint16_t)tidmask);

    return status;
}

param_info_t wrap_send_raw_cancel_params[] = {
    {"vap", PARAM_STRING , "vap name"},
};
int wrap_send_raw_cancel(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE ;
    uint8_t *pkt = NULL;

    pkt =  malloc(1500);
    if(pkt==NULL) return QDF_STATUS_E_FAILURE;
    memset(pkt, 0, 1500);

    status = acfg_send_raw_cancel((uint8_t *)params[0]);
    if (status != QDF_STATUS_SUCCESS){
        printf("%s: send raw cancel failed\n", __func__);
    }
    free(pkt);

    return acfg_to_os_status(status) ;
}

param_info_t wrap_offchan_rx_params[] = {
    {"vap", PARAM_STRING , "vap name"},
    {"channel", PARAM_STRING , "channel"},
    {"scan dur", PARAM_STRING , "scan duration"},
};
int wrap_offchan_rx(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE ;
    uint8_t *pkt = NULL;
    uint32_t chan;
    uint32_t scan_dur;
    int param_num = 0;
    char **pc;

    pc = params;
    while(*pc != NULL)
    {
        param_num++ ;
        pc++ ;
    }

    if (!(param_num  > 2 && param_num < 5)) {
        return QDF_STATUS_E_FAILURE;
    }

    get_uint32(params[1], (uint32_t *) &chan);
    get_uint32(params[2], (uint32_t *) &scan_dur);

    pkt =  malloc(1500);
    if(pkt==NULL) return QDF_STATUS_E_FAILURE;
    memset(pkt, 0, 1500);

    status = acfg_offchan_rx((uint8_t *)params[0], chan, scan_dur, params[3]);
    if (status != QDF_STATUS_SUCCESS){
        printf("%s: send offchan rx failed\n", __func__);
    }
    free(pkt);

    return acfg_to_os_status(status) ;
}

param_info_t wrap_set_bss_color_params[] = {
    {"wifi", PARAM_STRING , "radio name"},
    {"bsscolor", PARAM_STRING , "bsscolor"},
    {"override", PARAM_STRING , "override"},
};

int wrap_set_bss_color(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE ;
    uint32_t bsscolor;
    uint32_t override;

    get_uint32(params[1], (uint32_t *) &bsscolor);
    get_uint32(params[2], (uint32_t *) &override);

    status = acfg_set_bss_color((uint8_t *)params[0], bsscolor, override);
    if (status != QDF_STATUS_SUCCESS){
        printf("%s failed\n", __func__);
    }

    return acfg_to_os_status(status) ;
}

param_info_t wrap_get_bss_color_params[] = {
    {"wifi", PARAM_STRING , "radio name"},
};

int wrap_get_bss_color(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE ;
    uint32_t bsscolor;

    status = acfg_get_bss_color((uint8_t *)params[0], &bsscolor);
    if (status != QDF_STATUS_SUCCESS){
        printf("%s failed\n", __func__);
    }
    printf("%s: bsscolor=%d \n", __func__, bsscolor);

    return acfg_to_os_status(status) ;
}

param_info_t wrap_set_muedca_params[] = {
    {"vap", PARAM_STRING , "vap name"},
    {"ac", PARAM_STRING , "AC"},
    {"value", PARAM_STRING , "value"},
};

param_info_t wrap_get_muedca_params[] = {
    {"vap", PARAM_STRING , "vap name"},
    {"ac", PARAM_STRING , "AC"},
};

int wrap_set_muedca_ecwmin(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE ;
    uint32_t ac;
    uint32_t val;

    get_uint32(params[1], (uint32_t *) &ac);
    get_uint32(params[2], (uint32_t *) &val);
    status = acfg_set_muedca_ecwmin((uint8_t *)params[0], ac, val);
    if (status != QDF_STATUS_SUCCESS){
        printf("%s failed\n", __func__);
    }
    return acfg_to_os_status(status) ;
}

int wrap_get_muedca_ecwmin(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE ;
    uint32_t ac;
    uint32_t val;

    get_uint32(params[1], (uint32_t *) &ac);
    status = acfg_get_muedca_ecwmin((uint8_t *)params[0], ac, &val);
    if (status != QDF_STATUS_SUCCESS){
        printf("%s failed\n", __func__);
    }
    printf("%s: muedca_ecwmin=%d \n", __func__, val);

    return acfg_to_os_status(status) ;
}

int wrap_set_muedca_ecwmax(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE ;
    uint32_t ac;
    uint32_t val;

    get_uint32(params[1], (uint32_t *) &ac);
    get_uint32(params[2], (uint32_t *) &val);
    status = acfg_set_muedca_ecwmax((uint8_t *)params[0], ac, val);
    if (status != QDF_STATUS_SUCCESS){
        printf("%s failed\n", __func__);
    }
    return acfg_to_os_status(status) ;
}

int wrap_get_muedca_ecwmax(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE ;
    uint32_t ac;
    uint32_t val;

    get_uint32(params[1], (uint32_t *) &ac);
    status = acfg_get_muedca_ecwmax((uint8_t *)params[0], ac, &val);
    if (status != QDF_STATUS_SUCCESS){
        printf("%s failed\n", __func__);
    }
    printf("%s: muedca_ecwmax=%d \n", __func__, val);
    return acfg_to_os_status(status) ;
}

int wrap_set_muedca_aifsn(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE ;
    uint32_t ac;
    uint32_t val;

    get_uint32(params[1], (uint32_t *) &ac);
    get_uint32(params[2], (uint32_t *) &val);
    status = acfg_set_muedca_aifsn((uint8_t *)params[0], ac, val);
    if (status != QDF_STATUS_SUCCESS){
        printf("%s failed\n", __func__);
    }
    return acfg_to_os_status(status) ;
}

int wrap_get_muedca_aifsn(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE ;
    uint32_t ac;
    uint32_t val;

    get_uint32(params[1], (uint32_t *) &ac);
    status = acfg_get_muedca_aifsn((uint8_t *)params[0], ac, &val);
    if (status != QDF_STATUS_SUCCESS){
        printf("%s failed\n", __func__);
    }
    printf("%s: muedca_aifsn=%d \n", __func__, val);
    return acfg_to_os_status(status) ;
}

int wrap_set_muedca_acm(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE ;
    uint32_t ac;
    uint32_t val;

    get_uint32(params[1], (uint32_t *) &ac);
    get_uint32(params[2], (uint32_t *) &val);
    status = acfg_set_muedca_acm((uint8_t *)params[0], ac, val);
    if (status != QDF_STATUS_SUCCESS){
        printf("%s failed\n", __func__);
    }
    return acfg_to_os_status(status) ;
}

int wrap_get_muedca_acm(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE ;
    uint32_t ac;
    uint32_t val;

    get_uint32(params[1], (uint32_t *) &ac);
    status = acfg_get_muedca_acm((uint8_t *)params[0], ac, &val);
    if (status != QDF_STATUS_SUCCESS){
        printf("%s failed\n", __func__);
    }
    printf("%s: muedca_acm=%d \n", __func__, val);
    return acfg_to_os_status(status) ;
}

int wrap_set_muedca_timer(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE ;
    uint32_t ac;
    uint32_t val;

    get_uint32(params[1], (uint32_t *) &ac);
    get_uint32(params[2], (uint32_t *) &val);
    status = acfg_set_muedca_timer((uint8_t *)params[0], ac, val);
    if (status != QDF_STATUS_SUCCESS){
        printf("%s failed\n", __func__);
    }
    return acfg_to_os_status(status) ;
}

int wrap_get_muedca_timer(char *params[])
{
    uint32_t status = QDF_STATUS_E_FAILURE ;
    uint32_t ac;
    uint32_t val;

    get_uint32(params[1], (uint32_t *) &ac);
    status = acfg_get_muedca_timer((uint8_t *)params[0], ac, &val);
    if (status != QDF_STATUS_SUCCESS){
        printf("%s failed\n", __func__);
    }
    printf("%s: muedca_timer=%d \n", __func__, val);
    return acfg_to_os_status(status) ;
}

/*
 * Wrapper function table
 */
fntbl_t fntbl[] = {
    {"acfg_set_profile", wrap_set_profile, 2, wrap_set_profile_params },
    {"acfg_create_vap", wrap_create_vap, 5, wrap_create_vap_params },
    {"acfg_delete_vap", wrap_delete_vap, 2,  wrap_delete_vap_params },
    {"acfg_set_ssid", wrap_set_ssid, 2, wrap_set_ssid_params },
    {"acfg_get_ssid", wrap_get_ssid, 1, wrap_get_ssid_params },
    {"acfg_set_channel", wrap_set_channel, 2, wrap_set_channel_params },
    {"acfg_get_channel", wrap_get_channel, 1, wrap_get_channel_params },
    {"acfg_set_opmode", wrap_set_opmode, 2, wrap_set_opmode_params },
    {"acfg_get_opmode", wrap_get_opmode, 1, wrap_get_opmode_params },
    {"acfg_set_freq", wrap_set_freq, 2, wrap_set_freq_params },
    {"acfg_set_rts", wrap_set_rts, 3, wrap_set_rts_params },
    {"acfg_get_rts", wrap_get_rts, 1, wrap_get_rts_params },
    {"acfg_set_frag", wrap_set_frag, 3, wrap_set_frag_params },
    {"acfg_get_frag", wrap_get_frag, 1, wrap_get_frag_params },
    {"acfg_set_txpow", wrap_set_txpow, 3, wrap_set_txpow_params },
    {"acfg_get_txpow", wrap_get_txpow, 1, wrap_get_txpow_params },
    {"acfg_get_ap", wrap_get_ap, 1, wrap_get_ap_params },
    {"acfg_set_enc", wrap_set_enc, 3, wrap_set_enc_params },
    {"acfg_set_vap_vendor_param", wrap_set_vap_vendor_param, 5, wrap_set_vendor_params },
    {"acfg_get_vap_vendor_param", wrap_get_vap_vendor_param, 2, wrap_get_vendor_params },
    {"acfg_set_vap_param", wrap_set_vap_param, 3, wrap_set_vapprm_params },
    {"acfg_get_vap_param", wrap_get_vap_param, 2, wrap_get_vapprm_params },
    {"acfg_set_radio_param",wrap_set_radio_param, 3, wrap_set_radioprm_params},
    {"acfg_get_radio_param",wrap_get_radio_param, 2, wrap_get_radioprm_params},
    {"acfg_get_rate",wrap_get_rate, 1, wrap_get_rate_params},
    {"acfg_set_rate",wrap_set_rate, 2, wrap_set_rate_params},
    {"acfg_set_phymode", wrap_set_phymode, 2, wrap_set_phymode_params},
    {"acfg_get_phymode", wrap_get_phymode, 1, wrap_get_phymode_params},
    {"acfg_assoc_sta_info",wrap_assoc_sta_info, 1, wrap_assoc_sta_info_params},
    {"acfg_acl_addmac", wrap_acl_addmac, 2, wrap_acl_addmac_params},
    {"acfg_acl_getmac", wrap_acl_getmac, 1, wrap_acl_getmac_params},
    {"acfg_acl_delmac", wrap_acl_delmac, 2, wrap_acl_delmac_params},
    {"acfg_acl_addmac_secondary", wrap_acl_addmac_secondary, 2, wrap_acl_addmac_secondary_params},
    {"acfg_acl_getmac_secondary", wrap_acl_getmac_secondary, 1, wrap_acl_getmac_secondary_params},
    {"acfg_acl_delmac_secondary", wrap_acl_delmac_secondary, 2, wrap_acl_delmac_secondary_params},
    {"acfg_hostapd_getconfig", wrap_hostapd_getconfig,
        1, wrap_hostapd_getconfig_params},
    {"acfg_hostapd_set_wpa", wrap_hostapd_set_wpa,
        6, wrap_hostapd_set_wpa_params},
    {"acfg_wps_pbc", wrap_wps_pbc, 1, NULL},
    {"acfg_wps_pin", wrap_wps_pin, 4, NULL},
    {"acfg_wps_config", wrap_wps_config, 5, NULL},
    {"acfg_is_offload_vap", wrap_is_offload_vap, 1, wrap_is_offload_vap_params},
    {"acfg_enable_radio", wrap_enable_radio, 1, wrap_enable_radio_params },
    {"acfg_disable_radio", wrap_disable_radio, 1, wrap_disable_radio_params },
    {"acfg_set_country", wrap_set_country, 2, wrap_set_country_params },
    {"acfg_get_country", wrap_get_country, 1, wrap_get_country_params },
    {"acfg_get_regdomain", wrap_get_regdomain, 1, wrap_get_regdomain_params },
    {"acfg_set_shpreamble", wrap_set_shpreamble, 2, wrap_set_shpreamble_params },
    {"acfg_get_shpreamble", wrap_get_shpreamble, 1, wrap_get_shpreamble_params },
    {"acfg_set_shslot", wrap_set_shslot, 2, wrap_set_shslot_params },
    {"acfg_get_shslot", wrap_get_shslot, 1, wrap_get_shslot_params },
    {"acfg_set_tx_antenna", wrap_set_tx_antenna, 2, wrap_set_antenna_params },
    {"acfg_set_rx_antenna", wrap_set_rx_antenna, 2, wrap_set_antenna_params },
    {"acfg_get_tx_antenna", wrap_get_tx_antenna, 1, wrap_get_antenna_params },
    {"acfg_get_rx_antenna", wrap_get_rx_antenna, 1, wrap_get_antenna_params },
    {"acfg_set_txpower_limit", wrap_set_txpower_limit, 3, wrap_set_txpower_limit_params },
    {"acfg_add_client", wrap_add_client, 8, wrap_add_client_params},
    {"acfg_del_client", wrap_del_client, 2, wrap_del_client_params},
    {"acfg_forward_client", wrap_forward_client, 2, wrap_forward_client_params},
    {"acfg_set_key", wrap_set_key, 6, wrap_set_key_params},
    {"acfg_del_key", wrap_del_key, 3, wrap_del_key_params},
    {"acfg_set_preamble", wrap_set_preamble, 2, wrap_set_preamble_params},
    {"acfg_set_slot_time", wrap_set_slot_time, 2, wrap_set_slot_time_params},
    {"acfg_set_erp", wrap_set_erp, 2, wrap_set_erp_params},
    {"acfg_set_regdomain", wrap_set_regdomain, 2, wrap_set_regdomain_params},
    {"acfg_add_app_ie", wrap_add_app_ie, 1, wrap_add_app_ie_params},
    {"acfg_reset_profile", wrap_reset_profile, 1, wrap_reset_profile_params },
    {"acfg_set_ratemask", wrap_set_ratemask, 5, wrap_set_ratemask_params},
    {"acfg_tx99_tool", wrap_tx99_tool, TX99_MAX_PARAM_NUM, wrap_tx99_tool_params},
    {"acfg_set_mgmt_retry_limit", wrap_set_mgmt_retry_limit, 2, wrap_set_mgmt_retry_limit_params},
    {"acfg_get_mgmt_retry_limit", wrap_get_mgmt_retry_limit, 1, wrap_get_mgmt_retry_limit_params},
    {"acfg_set_tx_power", wrap_set_tx_power, 3, wrap_tx_power_set_params},
    {"acfg_set_sens_level", wrap_set_sens_level, 2, wrap_sens_set_params},
    {"acfg_gpio_config", wrap_gpio_config, 5, wrap_gpio_config_params},
    {"acfg_gpio_set", wrap_gpio_set, 3, wrap_gpio_set_params},
    {"acfg_send_raw_pkt", wrap_send_raw_pkt, 13, wrap_send_raw_pkt_params},
#if QCA_SUPPORT_GPR
    {"acfg_start_gpr", wrap_start_gpr, 5, wrap_start_gpr_params},
    {"acfg_send_gpr_cmd", wrap_send_gpr_cmd, 2, wrap_send_gpr_cmd_params},
#endif
    {"acfg_enable_amsdu", wrap_enable_amsdu, 2, wrap_enable_amsdu_params },
    {"acfg_enable_ampdu", wrap_enable_ampdu, 2, wrap_enable_ampdu_params },
    {"acfg_set_ctl_table", wrap_set_ctl_table, 3, wrap_set_ctl_table_params},
    {"acfg_set_op_support_rates", wrap_set_op_support_rates, ACFG_MAX_RATE_SIZE, wrap_set_op_support_rates_params},
    {"acfg_get_radio_supported_rates", wrap_get_radio_supported_rates, 2, wrap_get_radio_supported_rates_params},
    {"acfg_get_beacon_supported_rates", wrap_get_beacon_supported_rates, 1, wrap_get_beacon_supported_rates_params},
    {"acfg_set_mu_whtlist", wrap_set_mu_whtlist, 3, wrap_set_mu_whtlist_params},
    {"acfg_mon_addmac", wrap_mon_addmac, 2, wrap_mon_addmac_params},
    {"acfg_mon_delmac", wrap_mon_delmac, 2, wrap_mon_delmac_params},
    {"acfg_send_raw_cancel", wrap_send_raw_cancel, 1, wrap_send_raw_cancel_params},
    {"acfg_set_cca_threshold", wrap_cca_threshold, 2, wrap_cca_threshold_params},
    {"acfg_get_nf_dbr_dbm_info", wrap_get_nf_dbr_dbm_info, 1, wrap_get_nf_dbr_dbm_info_params},
    {"acfg_get_packet_power_info", wrap_get_packet_power_info, 8, wrap_get_packet_power_info_params},
    {"acfg_offchan_rx", wrap_offchan_rx, 3, wrap_offchan_rx_params},
    {"acfg_send_raw_multi", wrap_send_raw_multi, 13, wrap_send_raw_pkt_params},
    {"acfg_mon_listmac", wrap_mon_listmac, 1, wrap_mon_listmac_params},
    {"acfg_mon_enable_filter", wrap_mon_enable_filter, 2, wrap_mon_enable_filter_params},
    {"acfg_set_bss_color", wrap_set_bss_color, 3, wrap_set_bss_color_params},
    {"acfg_get_bss_color", wrap_get_bss_color, 1, wrap_get_bss_color_params},
    {"acfg_set_muedca_ecwmin", wrap_set_muedca_ecwmin, 3, wrap_set_muedca_params},
    {"acfg_get_muedca_ecwmin", wrap_get_muedca_ecwmin, 2, wrap_get_muedca_params},
    {"acfg_set_muedca_ecwmax", wrap_set_muedca_ecwmax, 3, wrap_set_muedca_params},
    {"acfg_get_muedca_ecwmax", wrap_get_muedca_ecwmax, 2, wrap_get_muedca_params},
    {"acfg_set_muedca_aifsn", wrap_set_muedca_aifsn, 3, wrap_set_muedca_params},
    {"acfg_get_muedca_aifsn", wrap_get_muedca_aifsn, 2, wrap_get_muedca_params},
    {"acfg_set_muedca_acm", wrap_set_muedca_acm, 3, wrap_set_muedca_params},
    {"acfg_get_muedca_acm", wrap_get_muedca_acm, 2, wrap_get_muedca_params},
    {"acfg_set_muedca_timer", wrap_set_muedca_timer, 3, wrap_set_muedca_params},
    {"acfg_get_muedca_timer", wrap_get_muedca_timer, 2, wrap_get_muedca_params},
    {NULL,NULL},
};
