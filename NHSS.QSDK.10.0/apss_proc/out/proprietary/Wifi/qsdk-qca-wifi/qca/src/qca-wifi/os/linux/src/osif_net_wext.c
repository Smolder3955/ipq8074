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
/*
 * Copyright (c) 2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

//#define __ACFG_PHYMODE_STRINGS__

#include <linux/module.h>
#include <linux/version.h>
#include <linux/netdevice.h>
#include <linux/wireless.h>
#include <net/iw_handler.h>

#include <qdf_types.h>
#include <osif_net.h>
#include <qdf_trace.h>

#include <qdf_mem.h>
#include <acfg_api_types.h>

#include<qdf_trace.h>
#include<if_net_wext.h>


/**
 * @brief Linux Wireless Extention
 *        private ioctls.
 */
enum {
    LINUX_WPRIV_IOCTL_SETPARAM      = (SIOCIWFIRSTPRIV+0),
    LINUX_WPRIV_IOCTL_GETPARAM      = (SIOCIWFIRSTPRIV+1),
	LINUX_WPRIV_IOCTL_SET_KEY       = (SIOCIWFIRSTPRIV+2),
	LINUX_WPRIV_IOCTL_SETWMMPARAMS  = (SIOCIWFIRSTPRIV+3),
	LINUX_WPRIV_IOCTL_DEL_KEY       = (SIOCIWFIRSTPRIV+4),
	LINUX_WPRIV_IOCTL_GETWMMPARAMS  = (SIOCIWFIRSTPRIV+5),
	LINUX_WPRIV_IOCTL_SETMLME       = (SIOCIWFIRSTPRIV+6),
	LINUX_WPRIV_IOCTL_GETCHANINFO   = (SIOCIWFIRSTPRIV+7),
	LINUX_WPRIV_IOCTL_SETOPTIE      = (SIOCIWFIRSTPRIV+8),
	LINUX_WPRIV_IOCTL_ADDMAC        = (SIOCIWFIRSTPRIV+10),
	LINUX_WPRIV_IOCTL_DELMAC        = (SIOCIWFIRSTPRIV+12),
	LINUX_WPRIV_IOCTL_GETCHANLIST   = (SIOCIWFIRSTPRIV+13),
        LINUX_WPRIV_IOCTL_KICKMAC = (SIOCIWFIRSTPRIV+15),
    LINUX_WPRIV_IOCTL_DOTH_CHSWITCH = (SIOCIWFIRSTPRIV+16),
    LINUX_WPRIV_IOCTL_GETMODE       = (SIOCIWFIRSTPRIV+17),
    LINUX_WPRIV_IOCTL_SETMODE       = (SIOCIWFIRSTPRIV+18),
	LINUX_WPRIV_IOCTL_SET_APPIEBUF  = (SIOCIWFIRSTPRIV+20),
    LINUX_WPRIV_IOCTL_SET_ACPARAMS   = (SIOCIWFIRSTPRIV+21),
	LINUX_WPRIV_IOCTL_FILTERFRAME   = (SIOCIWFIRSTPRIV+22),
    LINUX_WPRIV_IOCTL_SEND_MGMT     = (SIOCIWFIRSTPRIV+26),
    LINUX_WPRIV_IOCTL_CHN_WIDTHSWITCH = (SIOCIWFIRSTPRIV+28),
	LINUX_WPRIV_IOCTL_GETMACADDR    = (SIOCIWFIRSTPRIV+29),
};

/**
 * wifiX ioctls
 */
enum {
     LINUX_WPRIV_IOCTL_SETCOUNTRY    = (SIOCIWFIRSTPRIV+2),
};

/* Prototypes */

#define PROTO_IW(name)   \
static int (name)(struct net_device *,  \
                  struct iw_request_info *,    \
			      union iwreq_data *, char *);


/********************* STANDARD HANDLERS *************************/
PROTO_IW(__wext_set_ssid);
PROTO_IW(__wext_get_ssid);
PROTO_IW(__wext_set_opmode);
PROTO_IW(__wext_get_opmode);
PROTO_IW(__wext_set_freq);
PROTO_IW(__wext_get_freq);
PROTO_IW(__wext_get_wireless_name);
PROTO_IW(__wext_set_rts);
PROTO_IW(__wext_get_rts);
PROTO_IW(__wext_set_frag);
PROTO_IW(__wext_get_frag);
PROTO_IW(__wext_set_txpow);
PROTO_IW(__wext_get_txpow);
PROTO_IW(__wext_set_ap);
PROTO_IW(__wext_get_ap);
PROTO_IW(__wext_get_range);
PROTO_IW(__wext_set_encode);
PROTO_IW(__wext_get_encode);
PROTO_IW(__wext_set_rate);
PROTO_IW(__wext_get_rate);
PROTO_IW(__wext_set_scan);
PROTO_IW(__wext_get_scanresults);
PROTO_IW(__wext_set_power);
PROTO_IW(__wext_get_power);



static
struct iw_statistics* __wext_getstats_vap(struct net_device *dev);


/********************* PRIVATE HANDLERS *************************/
PROTO_IW(__wpriv_set_param_radio);
PROTO_IW(__wpriv_get_param_radio);
PROTO_IW(__wpriv_set_country);

PROTO_IW(__wpriv_set_param_vap);
PROTO_IW(__wpriv_get_param_vap);
PROTO_IW(__wpriv_set_wmmparams_vap);
PROTO_IW(__wpriv_get_wmmparams_vap);
PROTO_IW(__wpriv_get_chmode);
PROTO_IW(__wpriv_set_chmode);
PROTO_IW(__wpriv_kickmac);
PROTO_IW(__wpriv_doth_chswitch);
PROTO_IW(__wpriv_addmac);
PROTO_IW(__wpriv_delmac);
PROTO_IW(__wpriv_set_mlme);
PROTO_IW(__wpriv_set_optie);
PROTO_IW(__wpriv_set_acparams);
PROTO_IW(__wpriv_set_filterframe);
PROTO_IW(__wpriv_set_appiebuf);
PROTO_IW(__wpriv_set_key);
PROTO_IW(__wpriv_del_key);
PROTO_IW(__wpriv_get_chan_info);
PROTO_IW(__wpriv_get_chan_list);
PROTO_IW(__wpriv_send_mgmt);
PROTO_IW(__wpriv_get_mac_address);
PROTO_IW(__wpriv_dbgreq);
PROTO_IW(__wpriv_set_chn_widthswitch);

#define WEXT_NUM(x)     [x - SIOCIWFIRST]
#define WPRIV_IDX(x)    [x]
#define WPRIV_NUM(x)    (x + SIOCIWFIRSTPRIV)


/**
 * @brief Standard Wireless Extension Set
 */
const iw_handler     __wext_handlers[] = {
    WEXT_NUM(SIOCSIWESSID)  = __wext_set_ssid,
    WEXT_NUM(SIOCGIWESSID)  = __wext_get_ssid,
    WEXT_NUM(SIOCSIWMODE)   = __wext_set_opmode,
    WEXT_NUM(SIOCGIWMODE)   = __wext_get_opmode,
    WEXT_NUM(SIOCSIWFREQ)   = __wext_set_freq,
    WEXT_NUM(SIOCGIWFREQ)   = __wext_get_freq,
    WEXT_NUM(SIOCGIWNAME)   = __wext_get_wireless_name,
    WEXT_NUM(SIOCSIWRTS)    = __wext_set_rts,
    WEXT_NUM(SIOCGIWRTS)    = __wext_get_rts,
    WEXT_NUM(SIOCSIWFRAG)   = __wext_set_frag,
    WEXT_NUM(SIOCGIWFRAG)   = __wext_get_frag,
    WEXT_NUM(SIOCSIWTXPOW)  = __wext_set_txpow,
    WEXT_NUM(SIOCGIWTXPOW)  = __wext_get_txpow,
    WEXT_NUM(SIOCSIWAP)     = __wext_set_ap,
    WEXT_NUM(SIOCGIWAP)     = __wext_get_ap,
    WEXT_NUM(SIOCGIWRANGE)  = __wext_get_range,
    WEXT_NUM(SIOCSIWENCODE) = __wext_set_encode,
    WEXT_NUM(SIOCGIWENCODE) = __wext_get_encode,
    WEXT_NUM(SIOCSIWRATE)   = __wext_set_rate,
    WEXT_NUM(SIOCGIWRATE)   = __wext_get_rate,
    WEXT_NUM(SIOCSIWSCAN)   = __wext_set_scan,
    WEXT_NUM(SIOCGIWSCAN)   = __wext_get_scanresults,
    WEXT_NUM(SIOCSIWPOWER)  = __wext_set_power,
    WEXT_NUM(SIOCGIWPOWER)  = __wext_get_power,
};

/**
 * @brief The PRIV numbers are ACFG_REQ_* + SIOCIWFIRSTPRIV
 */
#define WPRIV_PARAM_INT_SET(num, name)              \
    { num,                                          \
      (IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1),  \
      0 , name }

#define WPRIV_PARAM_INT_GET(num, name)              \
    { num, 0,                                       \
      (IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1),  \
      name }

#define WPRIV_PARAM_INT_SET_GEN(num, name, numarg)      \
    { num,                                              \
        (IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | numarg), \
        0 , name }

#define WPRIV_PARAM_INT_WMM_SET(num, name)              \
    { num, (IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 4), 0,  \
      name }
#define WPRIV_PARAM_INT_WMM_GET(num, name)              \
    { num, (IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 3),                                       \
      (IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1),  \
      name }
#define WPRIV_PARAM_INT_WMM_HACK_SET(num, name)              \
    { num, (IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 3), 0,  \
      name }
#define WPRIV_PARAM_INT_WMM_HACK_GET(num, name)              \
    { num, (IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2),                                       \
      (IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1),  \
      name }

#define WPRIV_PARAM_CHAR_SET(num, name)             \
    { num,                                          \
      (IW_PRIV_TYPE_CHAR | 16),                     \
      0 , name }

#define WPRIV_PARAM_CHAR_GET(num, name)             \
    { num, 0,                                       \
      (IW_PRIV_TYPE_CHAR | 16),                     \
      name }

/*
 * Optimize :MAX_CEILING_SIZE :
 * currently assumed to be of bigger size
 * to be optimized later
 */
#define MAX_CEILING_SIZE 65535
#define IW_PRIV_TYPE_MLME \
        IW_PRIV_TYPE_BYTE | MAX_CEILING_SIZE
#define IW_PRIV_TYPE_GETWPAIE \
        IW_PRIV_TYPE_BYTE | MAX_CEILING_SIZE

#define IW_PRIV_TYPE_FILTER \
        IW_PRIV_TYPE_BYTE | sizeof(uint32_t)

#define IEEE80211_APPIE_MAX                  1024 /* max appie buffer size */
#define IW_PRIV_TYPE_APPIEBUF  (IW_PRIV_TYPE_BYTE | IEEE80211_APPIE_MAX)

#define IW_PRIV_TYPE_KEY \
        IW_PRIV_TYPE_BYTE | MAX_CEILING_SIZE
#define IW_PRIV_TYPE_DELKEY \
        IW_PRIV_TYPE_BYTE | MAX_CEILING_SIZE

#define QDF_CHANINFO_MAX 2047 /* Don't increase beyond this */
#define IW_PRIV_TYPE_CHANINFO (IW_PRIV_TYPE_INT | QDF_CHANINFO_MAX)
#define IW_PRIV_TYPE_CHANLIST (IW_PRIV_TYPE_BYTE | 128)
#define IW_PRIV_TYPE_ACLMACLIST  (IW_PRIV_TYPE_ADDR | 256)

static const struct iw_priv_args __wpriv_args_vap[] = {

    { LINUX_WPRIV_IOCTL_ADDMAC,
      IW_PRIV_TYPE_ADDR | IW_PRIV_SIZE_FIXED | 1 , 0,"addmac" },

    { LINUX_WPRIV_IOCTL_DELMAC,
      IW_PRIV_TYPE_ADDR | IW_PRIV_SIZE_FIXED | 1 , 0,"delmac" },

    { LINUX_WPRIV_IOCTL_SETMLME,
      IW_PRIV_TYPE_MLME | IW_PRIV_SIZE_FIXED | 1 , 0,"setmlme" },

    { LINUX_WPRIV_IOCTL_SETOPTIE,
      IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | ACFG_MAX_IELEN, 0,"setoptie" },

    { LINUX_WPRIV_IOCTL_SET_ACPARAMS,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 4, 0, "acparams" },

    { LINUX_WPRIV_IOCTL_FILTERFRAME,
        IW_PRIV_TYPE_FILTER , 0,                   "setfilter" },

    { LINUX_WPRIV_IOCTL_SET_APPIEBUF,
        IW_PRIV_TYPE_APPIEBUF, 0,                   "setiebuf" },

    { LINUX_WPRIV_IOCTL_SET_KEY,
        IW_PRIV_TYPE_KEY | IW_PRIV_SIZE_FIXED, 0,    "setkey" },

    { LINUX_WPRIV_IOCTL_DEL_KEY,
        IW_PRIV_TYPE_DELKEY | IW_PRIV_SIZE_FIXED, 0, "delkey" },

    { LINUX_WPRIV_IOCTL_GETCHANINFO,
        0, IW_PRIV_TYPE_CHANINFO | IW_PRIV_SIZE_FIXED, "getchaninfo" },

    { LINUX_WPRIV_IOCTL_GETCHANLIST,
        0, IW_PRIV_TYPE_CHANLIST | IW_PRIV_SIZE_FIXED, "getchanlist" },

    { LINUX_WPRIV_IOCTL_GETMACADDR,
        0, IW_PRIV_TYPE_ACLMACLIST, "getmac" },

    { LINUX_WPRIV_IOCTL_KICKMAC,
      IW_PRIV_TYPE_ADDR | IW_PRIV_SIZE_FIXED | 1 , 0,"kickmac" },

    WPRIV_PARAM_INT_SET_GEN(LINUX_WPRIV_IOCTL_DOTH_CHSWITCH, "doth_chanswitch", 2) ,

    WPRIV_PARAM_INT_SET_GEN(LINUX_WPRIV_IOCTL_CHN_WIDTHSWITCH , "doth_ch_chwidth", 3),

    WPRIV_PARAM_CHAR_SET(LINUX_WPRIV_IOCTL_SETMODE, "mode"),
    WPRIV_PARAM_CHAR_GET(LINUX_WPRIV_IOCTL_GETMODE, "get_mode"),

    /*WPRIV_PARAM_CHAR_GET(IEEE80211_IOCTL_GETMODE, "get_mode"), */

    WPRIV_PARAM_INT_SET(LINUX_WPRIV_IOCTL_SETPARAM, "setparam"),
    WPRIV_PARAM_INT_GET(LINUX_WPRIV_IOCTL_GETPARAM, "getparam"),

    /* sub-ioctl handlers */
    WPRIV_PARAM_INT_SET(LINUX_WPRIV_IOCTL_SETPARAM, "" ),
    WPRIV_PARAM_INT_GET(LINUX_WPRIV_IOCTL_GETPARAM, "" ),

    /* sub-ioctl definitions */
    WPRIV_PARAM_INT_SET(ACFG_PARAM_VAP_SHORT_GI, "shortgi" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_VAP_SHORT_GI, "get_shortgi" ),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_VAP_AMPDU, "ampdu"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_VAP_AMPDU, "get_ampdu"),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_CHWIDTH, "chwidth"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_CHWIDTH, "get_chwidth"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_COUNTRYCODE, "get_countrycode"),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_COUNTRY_IE, "countryie"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_COUNTRY_IE, "get_countryie"),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_CHANBW, "chanbw"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_CHANBW, "get_chanbw"),

    /*WPRIV_PARAM_INT_SET(LINUX_WPRIV_IOCTL_SETMODE,"mode"),*/
    WPRIV_PARAM_INT_SET(ACFG_PARAM_AUTHMODE , "authmode" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_AUTHMODE , "get_authmode" ),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_MACCMD , "maccmd" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_MACCMD , "get_maccmd" ),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_HIDESSID , "hide_ssid" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_HIDESSID , "get_hide_ssid" ),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_APBRIDGE , "ap_bridge" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_APBRIDGE , "get_ap_bridge" ),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_DTIM_PERIOD, "dtim_period" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_DTIM_PERIOD, "get_dtim_period" ),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_BEACON_INTERVAL , "bintval" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_BEACON_INTERVAL , "get_bintval" ),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_BURST , "burst" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_BURST , "get_burst" ),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_AMSDU , "amsdu" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_AMSDU , "get_amsdu" ),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_PUREG , "pureg" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_PUREG , "get_pureg" ),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_SHORTPREAMBLE, "shpreamble" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_SHORTPREAMBLE, "get_shpreamble" ),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_SHORTPREAMBLE, "blockdfschan" ),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_WDS , "wds" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_WDS , "get_wds" ),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_11N_RATE , "set11NRates" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_11N_RATE , "get11NRates" ),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_11N_RETRIES , "set11NRetries" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_11N_RETRIES , "get11NRetries" ),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_DBG_LVL , "dbgLVL" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_DBG_LVL , "getdbgLVL" ),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_STA_FORWARD , "stafwd" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_STA_FORWARD , "get_stafwd" ),

    WPRIV_PARAM_INT_SET(ACFG_PARAM_SETADDBAOPER, "setaddbaoper"),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_OPMODE_NOTIFY, "opmode_notify"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_OPMODE_NOTIFY, "g_opmod_notify"),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_WEP_TKIP_HT, "htweptkip"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_WEP_TKIP_HT, "get_htweptkip"),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_CWM_ENABLE, "cwmenable"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_CWM_ENABLE,"get_cwmenable"),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_MAX_AMPDU, "maxampdu"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_MAX_AMPDU, "get_maxampdu"),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_VHT_MAX_AMPDU, "vhtmaxampdu"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_VHT_MAX_AMPDU, "get_vhtmaxampdu"),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_ENABLE_RTSCTS, "enablertscts"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_ENABLE_RTSCTS, "g_enablertscts"),

    /* IQUE */
    WPRIV_PARAM_INT_SET(ACFG_PARAM_ME, "mcastenhance" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_ME, "g_mcastenhance" ),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_MEDUMP, "medump_dummy" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_MEDUMP, "medump" ),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_MEDEBUG, "medebug" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_MEDEBUG, "get_medebug" ),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_ME_SNOOPLEN, "me_length" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_ME_SNOOPLEN, "get_me_length" ),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_ME_TIMEOUT, "metimeout" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_ME_TIMEOUT, "get_metimeout" ),

    WPRIV_PARAM_INT_SET(ACFG_PARAM_PUREN , "puren" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_PUREN , "get_puren" ),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_NO_EDGE_CH , "noedgech" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_NO_EDGE_CH , "get_noedgech" ),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_WPS , "wps" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_WPS , "get_wps" ),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_EXTAP , "extap" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_EXTAP , "get_extap" ),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_DISABLECOEXT , "disablecoext" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_DISABLECOEXT , "g_disablecoext" ),

    WPRIV_PARAM_INT_SET(ACFG_PARAM_MAXSTA , "maxsta" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_MAXSTA , "get_maxsta" ),

    WPRIV_PARAM_INT_SET(ACFG_PARAM_RRM_CAP, "rrm" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_RRM_CAP, "get_rrm" ),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_RRM_DEBUG, "rrmdbg" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_RRM_DEBUG, "get_rrmdbg" ),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_RRM_STATS, "rrmstats" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_RRM_STATS, "get_rrmstats" ),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_RRM_SLWINDOW, "rrmslwin" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_RRM_SLWINDOW, "get_rrmslwin" ),

    WPRIV_PARAM_INT_SET(ACFG_PARAM_GREEN_AP_PS_ENABLE , "ant_ps_on" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_GREEN_AP_PS_ENABLE , "get_ant_ps_on" ),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_GREEN_AP_PS_TIMEOUT , "ps_timeout" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_GREEN_AP_PS_TIMEOUT , "get_ps_timeout" ),
#if WEXT_SUPPORT_TDLS
    WPRIV_PARAM_INT_SET(ACFG_PARAM_TDLS_ENABLE , "tdls" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_TDLS_ENABLE , "get_tdls" ),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_SET_TDLS_RMAC , "set_tdls_rmac" ),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_CLR_TDLS_RMAC , "clr_tdls_rmac" ),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_TDLS_MACADDR1 , "tdlsmacaddr1" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_TDLS_MACADDR1 , "gettdlsmacaddr1" ),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_TDLS_MACADDR2 , "tdlsmacaddr2" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_TDLS_MACADDR2 , "gettdlsmacaddr2" ),
#if WEXT_TDLS_AUTO_CONNECT
    WPRIV_PARAM_INT_SET(ACFG_PARAM_TDLS_AUTO_ENABLE , "tdls_auto" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_TDLS_AUTO_ENABLE , "get_tdls_auto" ),
#endif
    WPRIV_PARAM_INT_SET(ACFG_PARAM_TDLS_DIALOG_TOKEN , "tdls_dtoken" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_TDLS_DIALOG_TOKEN , "get_tdls_dtoken" ),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_TDLS_DISCOVERY_REQ , "do_tdls_dc_req" ),
#endif
    WPRIV_PARAM_INT_SET(ACFG_PARAM_PERIODIC_SCAN , "periodicScan" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_PERIODIC_SCAN , "g_periodicScan" ),

    WPRIV_PARAM_INT_SET(ACFG_PARAM_SW_WOW , "sw_wow" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_SW_WOW , "get_sw_wow" ),

    WPRIV_PARAM_INT_SET(ACFG_PARAM_SCANVALID , "scanvalid" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_SCANVALID , "get_scanvalid" ),

    WPRIV_PARAM_INT_SET(ACFG_PARAM_SCANBAND , "scanband" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_SCANBAND , "get_scanband" ),

    /* wmm */
    WPRIV_PARAM_INT_WMM_SET(LINUX_WPRIV_IOCTL_SETWMMPARAMS, "setwmmparams"),

    WPRIV_PARAM_INT_WMM_GET(LINUX_WPRIV_IOCTL_GETWMMPARAMS, "getwmmparams"),

   /*
    * These depends on sub-ioctl support which added in version 12.
    */
    WPRIV_PARAM_INT_WMM_HACK_SET(LINUX_WPRIV_IOCTL_SETWMMPARAMS, "" ),
    WPRIV_PARAM_INT_WMM_HACK_GET(LINUX_WPRIV_IOCTL_GETWMMPARAMS, "" ),

    /* sub-ioctl handlers */
    WPRIV_PARAM_INT_WMM_HACK_SET(ACFG_PARAM_WMMPARAMS_CWMIN, "cwmin" ),
    WPRIV_PARAM_INT_WMM_HACK_GET(ACFG_PARAM_WMMPARAMS_CWMIN, "get_cwmin" ),
    WPRIV_PARAM_INT_WMM_HACK_SET(ACFG_PARAM_WMMPARAMS_CWMAX, "cwmax" ),
    WPRIV_PARAM_INT_WMM_HACK_GET(ACFG_PARAM_WMMPARAMS_CWMAX, "get_cwmax" ),
    WPRIV_PARAM_INT_WMM_HACK_SET(ACFG_PARAM_WMMPARAMS_AIFS, "aifs" ),
    WPRIV_PARAM_INT_WMM_HACK_GET(ACFG_PARAM_WMMPARAMS_AIFS,  "get_aifs" ),
    WPRIV_PARAM_INT_WMM_HACK_SET(ACFG_PARAM_WMMPARAMS_TXOPLIMIT, "txoplimit" ),
    WPRIV_PARAM_INT_WMM_HACK_GET(ACFG_PARAM_WMMPARAMS_TXOPLIMIT,  "get_txoplimit" ),
    WPRIV_PARAM_INT_WMM_HACK_SET(ACFG_PARAM_WMMPARAMS_ACM, "acm" ),
    WPRIV_PARAM_INT_WMM_HACK_GET(ACFG_PARAM_WMMPARAMS_ACM,  "get_acm" ),
    WPRIV_PARAM_INT_WMM_HACK_SET(ACFG_PARAM_WMMPARAMS_NOACKPOLICY, "get_noackpolicy" ),
    WPRIV_PARAM_INT_WMM_HACK_GET(ACFG_PARAM_WMMPARAMS_NOACKPOLICY,  "get_noackpolicy" ),

    WPRIV_PARAM_INT_SET(ACFG_PARAM_QBSS_LOAD , "qbssload" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_QBSS_LOAD , "get_qbssload" ),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_PROXYARP_CAP , "proxyarp" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_PROXYARP_CAP , "get_proxyarp" ),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_DGAF_DISABLE , "dgaf_disable" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_DGAF_DISABLE , "g_dgaf_disable" ),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_L2TIF_CAP , "l2tif" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_L2TIF_CAP , "get_l2tif" ),

    WPRIV_PARAM_INT_SET(ACFG_PARAM_VHT_MCS, "vhtmcs" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_VHT_MCS, "get_vhtmcs" ),

    WPRIV_PARAM_INT_SET(ACFG_PARAM_NSS, "nss" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_NSS, "get_nss" ),

    WPRIV_PARAM_INT_SET(ACFG_PARAM_LDPC, "ldpc" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_LDPC, "get_ldpc" ),

    WPRIV_PARAM_INT_SET(ACFG_PARAM_TX_STBC, "tx_stbc" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_TX_STBC, "get_tx_stbc" ),

    WPRIV_PARAM_INT_SET(ACFG_PARAM_WNM_ENABLE , "wnm" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_WNM_ENABLE , "get_wnm" ),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_WNM_BSSMAX , "wnm_bss" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_WNM_BSSMAX , "get_wnm_bss" ),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_WNM_TFS , "wnm_tfs" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_WNM_TFS , "get_wnm_tfs" ),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_WNM_TIM , "wnm_tim" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_WNM_TIM , "get_wnm_tim" ),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_WNM_SLEEP , "wnm_sleep" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_WNM_SLEEP , "get_wnm_sleep" ),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_MODIFY_BEACON_RATE , "set_bcn_rate" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_MODIFY_BEACON_RATE , "get_bcn_rate" ),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_IMPLICITBF , "implicitbf" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_IMPLICITBF , "get_implicitbf" ),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_SIFS_TRIGGER , "sifs_trigger" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_SIFS_TRIGGER , "g_sifs_trigger" ),
};

static const iw_handler     __wpriv_handlers_vap[] = {
    WPRIV_IDX(0)  = __wpriv_set_param_vap,
    WPRIV_IDX(1)  = __wpriv_get_param_vap,
    WPRIV_IDX(2)  = __wpriv_set_key,
    WPRIV_IDX(3)  = __wpriv_set_wmmparams_vap,
    WPRIV_IDX(4)  = __wpriv_del_key,
    WPRIV_IDX(5)  = __wpriv_get_wmmparams_vap,
    WPRIV_IDX(6)  = __wpriv_set_mlme,
    WPRIV_IDX(7)  = __wpriv_get_chan_info,
    WPRIV_IDX(8)  = __wpriv_set_optie,
    WPRIV_IDX(13) = __wpriv_get_chan_list,
    WPRIV_IDX(15) = __wpriv_kickmac,
    WPRIV_IDX(16) = __wpriv_doth_chswitch,
    WPRIV_IDX(10) = __wpriv_addmac,
    WPRIV_IDX(12) = __wpriv_delmac,
    WPRIV_IDX(17) = __wpriv_get_chmode,
    WPRIV_IDX(18) = __wpriv_set_chmode,
    WPRIV_IDX(20) = __wpriv_set_appiebuf,
    WPRIV_IDX(21) = __wpriv_set_acparams,
    WPRIV_IDX(22) = __wpriv_set_filterframe,
    WPRIV_IDX(24) = __wpriv_dbgreq,
    WPRIV_IDX(26) = __wpriv_send_mgmt,
    WPRIV_IDX(28) = __wpriv_set_chn_widthswitch,
    WPRIV_IDX(29) = __wpriv_get_mac_address,
};

static const struct iw_priv_args __wpriv_args_radio[] = {
    /*
    WPRIV_PARAM_INT_SET(IEEE80211_IOCTL_SETPARAM, "setparam"),
    WPRIV_PARAM_INT_GET(IEEE80211_IOCTL_GETPARAM, "getparam"),
    */

    { LINUX_WPRIV_IOCTL_SETCOUNTRY, IW_PRIV_TYPE_CHAR | 3, 0,       "setCountry" },

    /* sub-ioctl handlers */
    WPRIV_PARAM_INT_SET(LINUX_WPRIV_IOCTL_SETPARAM, ""),
    WPRIV_PARAM_INT_GET(LINUX_WPRIV_IOCTL_GETPARAM, ""),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_RADIO_AMPDU, "AMPDU"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_RADIO_AMPDU, "getAMPDU"),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_RADIO_AMPDU_LIMIT, "AMPDULim"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_RADIO_AMPDU_LIMIT, "getAMPDULim"),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_RADIO_AMPDU_SUBFRAMES, "AMPDUFrames"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_RADIO_AMPDU_SUBFRAMES, "getAMPDUFrames"),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_RADIO_AMPDU_RX_BSIZE, "AMPDURxBsize"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_RADIO_AMPDU_RX_BSIZE, "getAMPDURxBsize"),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_RADIO_AGGR_BURST, "burst"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_RADIO_AGGR_BURST, "get_burst"),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_RADIO_AGGR_BURST_DUR, "burst_dur"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_RADIO_AGGR_BURST_DUR, "get_burst_dur"),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_RADIO_TXCHAINMASK, "txchainmask"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_RADIO_TXCHAINMASK, "get_txchainmask"),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_RADIO_RXCHAINMASK, "rxchainmask"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_RADIO_RXCHAINMASK, "get_rxchainmask"),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_RADIO_TXCHAINMASK_LEGACY, "tx_cm_legacy"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_RADIO_TXCHAINMASK_LEGACY, "g_tx_cm_legacy"),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_RADIO_RXCHAINMASK_LEGACY, "rx_cm_legacy"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_RADIO_RXCHAINMASK_LEGACY, "g_rx_cm_legacy"),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_RADIO_ATH_DEBUG, "ATHDebug"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_RADIO_ATH_DEBUG, "getATHDebug"),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_RADIO_GPIO_LED_CUSTOM, "set_ledcustom"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_RADIO_GPIO_LED_CUSTOM, "get_ledcustom"),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_RADIO_SWAP_DEFAULT_LED, "set_swapled"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_RADIO_SWAP_DEFAULT_LED, "get_swapled"),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_RADIO_VOWEXT, "setVowExt"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_RADIO_VOWEXT, "getVowExt"),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_RADIO_RCA, "set_rcaparam"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_RADIO_RCA, "get_rcaparam"),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_RADIO_VSP_ENABLE, "set_vsp_enable"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_RADIO_VSP_ENABLE, "get_vsp_enable"),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_RADIO_VSP_THRESHOLD, "set_vsp_thresh"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_RADIO_VSP_THRESHOLD, "get_vsp_thresh"),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_RADIO_VSP_EVALINTERVAL, "set_vsp_evaldur"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_RADIO_VSP_EVALINTERVAL, "get_vsp_evaldur"),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_RADIO_VOWEXT_STATS, "setVowExtStats"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_RADIO_VOWEXT_STATS, "getVowExtStats"),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_RADIO_DCS_ENABLE, "dcs_enable"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_RADIO_DCS_ENABLE, "get_dcs_enable"),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_RADIO_DCS_COCH, "set_dcs_intrth"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_RADIO_DCS_COCH, "get_dcs_intrth"),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_RADIO_DCS_TXERR, "set_dcs_errth"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_RADIO_DCS_TXERR, "get_dcs_errth"),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_RADIO_DCS_PHYERR, "s_dcs_phyerrth"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_RADIO_DCS_PHYERR, "g_dcs_phyerrth"),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_RADIO_PHYRESTART_WAR, "s_PhyRestartWar"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_RADIO_PHYRESTART_WAR, "g_PhyRestartWar"),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_RADIO_KEYSEARCH_ALWAYS_WAR, "s_KeySrchAlways"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_RADIO_KEYSEARCH_ALWAYS_WAR, "g_KeySrchAlways"),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_RADIO_COUNTRYID, "setCountryID"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_RADIO_COUNTRYID, "getCountryID"),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_RADIO_ENABLE_OL_STATS, "enable_ol_stats"),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_RADIO_FW_HANG_ID, "set_fw_hang"),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_RADIO_FW_RECOVERY_ID, "set_fw_recovery"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_RADIO_FW_RECOVERY_ID, "get_fw_recovery"),

    WPRIV_PARAM_INT_SET(ACFG_PARAM_RADIO_DYN_TX_CHAINMASK, "dyntxchain"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_RADIO_DYN_TX_CHAINMASK, "get_dyntxchain"),

    /* SmartAntenna */
    WPRIV_PARAM_INT_SET(ACFG_PARAM_RADIO_SMARTANTENNA, "setSmartAntenna"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_RADIO_SMARTANTENNA, "getSmartAntenna"),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_RADIO_SMARTANT_TRAIN_MODE, "ant_trainmode"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_RADIO_SMARTANT_TRAIN_MODE, "g_ant_trainmode"),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_RADIO_SMARTANT_TRAIN_TYPE, "ant_traintype" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_RADIO_SMARTANT_TRAIN_TYPE, "g_ant_traintype"),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_RADIO_SMARTANT_PKT_LEN, "ant_pktlen"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_RADIO_SMARTANT_PKT_LEN, "getant_pktlen"),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_RADIO_SMARTANT_NUM_PKTS, "ant_numpkts"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_RADIO_SMARTANT_NUM_PKTS, "getant_numpkts"),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_RADIO_SMARTANT_TRAIN_START, "ant_train"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_RADIO_SMARTANT_TRAIN_START, "getant_train"),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_RADIO_SMARTANT_NUM_ITR ,"ant_numitr" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_RADIO_SMARTANT_NUM_ITR ,"getant_numitr" ),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_RADIO_SMARTANT_RETRAIN_THRESH,"train_threshold" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_RADIO_SMARTANT_RETRAIN_THRESH,"get_tthreshold" ),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_RADIO_SMARTANT_RETRAIN_INTERVAL,"ret_interval" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_RADIO_SMARTANT_RETRAIN_INTERVAL,"get_retinterval" ),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_RADIO_SMARTANT_RETRAIN_DROP,"retrain_drop" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_RADIO_SMARTANT_RETRAIN_DROP,"get_retindrop" ),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_RADIO_SMARTANT_MIN_GP_THRESH ,"train_min_thrs" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_RADIO_SMARTANT_MIN_GP_THRESH ,"getrt_min_thrs" ),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_RADIO_SMARTANT_GP_AVG_INTERVAL ,"gp_avg_intrvl" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_RADIO_SMARTANT_GP_AVG_INTERVAL ,"g_gp_avg_intrvl" ),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_RADIO_ACS_ENABLE_BK_SCANTIMER,"acs_bkscanen" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_RADIO_ACS_ENABLE_BK_SCANTIMER,"g_acs_bkscanen" ),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_RADIO_ACS_SCANTIME,"acs_scanintvl" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_RADIO_ACS_SCANTIME,"g_acsscanintvl"),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_RADIO_ACS_RSSIVAR,"acs_rssivar"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_RADIO_ACS_RSSIVAR,"get_acs_rssivar"),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_RADIO_ACS_CHLOADVAR,"acs_chloadvar"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_RADIO_ACS_CHLOADVAR,"g_acschloadvar"),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_RADIO_ACS_LIMITEDOBSS,"acs_lmtobss"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_RADIO_ACS_LIMITEDOBSS,"get_acslmtobss"),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_RADIO_ACS_CTRLFLAG,"acs_ctrlflags"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_RADIO_ACS_CTRLFLAG,"g_acsctrlflags"),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_RADIO_ACS_DEBUGTRACE,"acs_dbgtrace"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_RADIO_ACS_DEBUGTRACE,"g_acs_dbgtrace"),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_RADIO_SMARTANT_CURRENT_ANT ,"current_ant" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_RADIO_SMARTANT_CURRENT_ANT ,"getcurrent_ant" ),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_RADIO_SMARTANT_DEFAULT_ANT ,"default_ant" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_RADIO_SMARTANT_DEFAULT_ANT ,"getdefault_ant" ),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_RADIO_SMARTANT_TRAFFIC_GEN_TIMER ,"traffic_timer"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_RADIO_SMARTANT_TRAFFIC_GEN_TIMER ,"gtraffic_timer" ),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_RADIO_SMARTANT_RETRAIN ,"ant_retrain" ),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_RADIO_SMARTANT_RETRAIN ,"getant_retrain" ),

    WPRIV_PARAM_INT_SET(ACFG_PARAM_HAL_CONFIG_FORCEBIASAUTO, "ForBiasAuto"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_HAL_CONFIG_FORCEBIASAUTO, "GetForBiasAuto"),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_HAL_CONFIG_DEBUG, "HALDbg"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_HAL_CONFIG_DEBUG, "GetHALDbg"),

    WPRIV_PARAM_INT_SET(ACFG_PARAM_HAL_CONFIG_CRDC_ENABLE, "crdcEnable"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_HAL_CONFIG_CRDC_ENABLE, "getcrdcEnable"),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_HAL_CONFIG_CRDC_WINDOW, "crdcWindow"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_HAL_CONFIG_CRDC_WINDOW, "getcrdcWindow"),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_HAL_CONFIG_CRDC_RSSITHRESH, "crdcRssith"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_HAL_CONFIG_CRDC_RSSITHRESH, "getcrdcRssith"),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_HAL_CONFIG_CRDC_DIFFTHRESH, "crdcDiffth"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_HAL_CONFIG_CRDC_DIFFTHRESH, "getcrdcDiffth"),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_HAL_CONFIG_CRDC_NUMERATOR, "crdcNum"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_HAL_CONFIG_CRDC_NUMERATOR, "getcrdcNum"),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_HAL_CONFIG_CRDC_DENOMINATOR, "crdcDenom"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_HAL_CONFIG_CRDC_DENOMINATOR, "getcrdcDenom"),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_HAL_CONFIG_PRINT_BBDEBUG, "printBBDebug"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_HAL_CONFIG_PRINT_BBDEBUG, "g_printBBDebug"),
    WPRIV_PARAM_INT_SET(ACFG_PARAM_HAL_CONFIG_ENABLEADAPTIVECCATHRES, "CCAThEna"),
    WPRIV_PARAM_INT_GET(ACFG_PARAM_HAL_CONFIG_ENABLEADAPTIVECCATHRES, "GetCCAThEna"),
};

static const iw_handler     __wpriv_handlers_radio[] = {
    WPRIV_IDX(0)     = __wpriv_set_param_radio,
    WPRIV_IDX(1)     = __wpriv_get_param_radio,
    WPRIV_IDX(2)     = __wpriv_set_country,
};



#define NUM(a)    (sizeof (a) / sizeof (a[0]))
struct iw_handler_def __w_handler_def_radio = {
    .standard           = NULL,
    .num_standard       = 0,
    .private            = __wpriv_handlers_radio,
    .num_private        = NUM(__wpriv_handlers_radio),
    .private_args       = __wpriv_args_radio,
    .num_private_args   = NUM(__wpriv_args_radio),
    /* .get_wireless_stats = __wext_getstats, */
};

struct iw_handler_def __w_handler_def_vap = {
    .standard           = __wext_handlers,
    .num_standard       = NUM(__wext_handlers),
    .private            = __wpriv_handlers_vap,
    .num_private        = NUM(__wpriv_handlers_vap),
    .private_args       = __wpriv_args_vap,
    .num_private_args   = NUM(__wpriv_args_vap),
    .get_wireless_stats = __wext_getstats_vap,
};
#undef N

typedef QDF_STATUS (*__wioctl_fn_t)(__qdf_softc_t  *sc, acfg_data_t *data);
extern const __wioctl_fn_t   __wioctl_sw[];

/**
 * @brief Get Wifi Wireless handlers
 */
struct iw_handler_def *
__qdf_net_iwget_wifi(void)
{
    return &__w_handler_def_radio;
}

/**
 * @brief Get VAP Wireless handlers
 */
struct iw_handler_def *
__qdf_net_iwget_vap(void)
{
    return &__w_handler_def_vap;
}

/**********************************************************************/
/******************   STANDARD HANDLERS *******************************/
/**********************************************************************/

/**
 * @brief Set the SSID
 *
 * @param dev
 * @param info
 * @param data
 * @param essid (Essid is already copied)
 *
 * @return
 */
static int
__wext_set_ssid(struct net_device *dev, struct iw_request_info *info,
                union iwreq_data *iwreq, char *extra)
{
    acfg_ssid_t         ssid   = {.len = 1};
    QDF_STATUS          status = QDF_STATUS_E_INVAL;

    if (iwreq->essid.flags)
        ssid.len = acfg_min(iwreq->essid.length, (ACFG_MAX_SSID_LEN ));

    /* Extra points to the SSID  */
    memcpy(ssid.name, extra, ssid.len);

    qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): Set SSID to %s \n",\
                                            __FUNCTION__,ssid.name);

    status = __wioctl_sw[ACFG_REQ_SET_SSID](netdev_to_softc(dev),
                                           (acfg_data_t *)&ssid);

    return __qdf_status_to_os(status);
}


/**
 * @brief Get the SSID
 *
 * @param dev
 * @param info
 * @param iwreq
 * @param extra
 *
 * @return
 */
static int
__wext_get_ssid(struct net_device *dev, struct iw_request_info *info,
                union iwreq_data *iwreq, char *extra)
{
    acfg_ssid_t         ssid   = {.len = 1};
    QDF_STATUS          status = QDF_STATUS_E_INVAL;

    if (iwreq->essid.flags)
        ssid.len = acfg_min(iwreq->essid.length, (ACFG_MAX_SSID_LEN));

    qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

    status = __wioctl_sw[ACFG_REQ_GET_SSID](netdev_to_softc(dev),
                                           (acfg_data_t *)&ssid);

    iwreq->essid.length = ssid.len ;
    qdf_str_lcopy(extra ,ssid.name ,ssid.len + 1) ;
    iwreq->essid.flags = 1 ; /* active */

    return __qdf_status_to_os(status);
}


/**
 * @brief Set Channel/Frequency
 *
 * @param dev
 * @param info
 * @param iwreq
 * @param extra
 *
 * @return
 */
static int
__wext_set_freq(struct net_device *dev, struct iw_request_info *info,
                union iwreq_data *iwreq, char *extra)
{
    QDF_STATUS          status = QDF_STATUS_E_INVAL;
    acfg_freq_t freq ;

    freq.m = iwreq->freq.m ;
    freq.e = iwreq->freq.e ;

    qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): mantissa = %d ; exponent = %d \n",\
            __FUNCTION__,freq.m , freq.e);

    status = __wioctl_sw[ACFG_REQ_SET_FREQUENCY](netdev_to_softc(dev), \
            (acfg_data_t *)&freq);

    return __qdf_status_to_os(status);
}


/**
 * @brief Get Channel/Frequency
 *
 * @param dev
 * @param info
 * @param iwreq
 * @param extra
 *
 * @return
 */
static int
__wext_get_freq(struct net_device *dev, struct iw_request_info *info,
                union iwreq_data *iwreq, char *extra)
{

    acfg_freq_t freq ;
    QDF_STATUS status = QDF_STATUS_E_INVAL;

    qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

    status = __wioctl_sw[ACFG_REQ_GET_FREQUENCY](netdev_to_softc(dev),
                                           (acfg_data_t *)&freq);
    iwreq->freq.e =  freq.e ;
    iwreq->freq.m = freq.m ;

    return __qdf_status_to_os(status);
}


/**
 * @brief Set the Opmode
 *
 * @param dev
 * @param info
 * @param iwreq
 * @param extra
 *
 * @return
 */
static int
__wext_set_opmode(struct net_device *dev, struct iw_request_info *info,
                union iwreq_data *iwreq, char *extra)
{
    acfg_opmode_t       opmode = 0 ;
    QDF_STATUS          status = QDF_STATUS_E_INVAL;

    opmode = (acfg_opmode_t)iwreq->mode ;

    switch(opmode)
    {
        case IW_MODE_AUTO: opmode = ACFG_OPMODE_STA ; break ;
        case IW_MODE_ADHOC: opmode = ACFG_OPMODE_IBSS ; break ;
        case IW_MODE_INFRA: opmode = ACFG_OPMODE_STA ; break ;
        case IW_MODE_MASTER: opmode = ACFG_OPMODE_HOSTAP ; break ;
        case IW_MODE_REPEAT: return -EINVAL ; break ;
        case IW_MODE_SECOND: return -EINVAL ; break ;
        case IW_MODE_MONITOR: opmode = ACFG_OPMODE_MONITOR ; break ;
        case IW_MODE_MESH: return -EINVAL ; break ;
    }

    qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): Set Opmode to %d \n",\
                                                __FUNCTION__,opmode);

    status = __wioctl_sw[ACFG_REQ_SET_OPMODE](netdev_to_softc(dev),
                                           (acfg_data_t *)&opmode);

    return __qdf_status_to_os(status);
}


/**
 * @brief Get the Opmode
 *
 * @param dev
 * @param info
 * @param iwreq
 * @param extra
 *
 * @return
 */
static int
__wext_get_opmode(struct net_device *dev, struct iw_request_info *info,
                union iwreq_data *iwreq, char *extra)
{
    acfg_opmode_t       opmode = 0 ;
    QDF_STATUS          status = QDF_STATUS_E_INVAL;

    qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

    status = __wioctl_sw[ACFG_REQ_GET_OPMODE](netdev_to_softc(dev),
                                           (acfg_data_t *)&opmode);

    iwreq->mode = opmode ;

    return __qdf_status_to_os(status);
}


/**
 * @brief Get wireless name (to verify presence of wireless extentions)
 *
 * @param dev
 * @param info
 * @param iwreq
 * @param extra
 *
 * @return
 */
static int
__wext_get_wireless_name(struct net_device *dev, struct iw_request_info *info,
                union iwreq_data *iwreq, char *extra)
{
    u_int8_t wname[ACFG_MAX_IFNAME] ;
    QDF_STATUS          status = QDF_STATUS_E_INVAL;

    qdf_trace(QDF_DEBUG_FUNCTRACE, "QDF_WEXT: %s(): \n",__FUNCTION__);

    status = __wioctl_sw[ACFG_REQ_GET_NAME](netdev_to_softc(dev),
                                    (acfg_data_t *)wname);

    qdf_str_lcopy(iwreq->name ,wname  ,ACFG_MAX_IFNAME + 1);

    return __qdf_status_to_os(status);
}



/**
 * @brief Set RTS threshold
 *
 * @param dev
 * @param info
 * @param iwreq
 * @param extra
 *
 * @return
 */
static int
__wext_set_rts(struct net_device *dev, struct iw_request_info *info,
                union iwreq_data *iwreq, char *extra)
{
    acfg_rts_t  rts ;
    QDF_STATUS  status = QDF_STATUS_E_INVAL;

    rts.val = iwreq->rts.value ;
    rts.flags = 0 ;

    if(iwreq->rts.disabled)
        rts.flags |= ACFG_RTS_DISABLED ;

    if(iwreq->rts.fixed)
        rts.flags |= ACFG_RTS_FIXED ;

    qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): rts= %d; disabled= %d; fixed= %d\n",
			__FUNCTION__, iwreq->rts.value,
			iwreq->rts.disabled, iwreq->rts.fixed);


    status = __wioctl_sw[ACFG_REQ_SET_RTS](netdev_to_softc(dev),
                                           (acfg_data_t *)&rts);

    return __qdf_status_to_os(status);
}



/**
 * @brief Get RTS threshold
 *
 * @param dev
 * @param info
 * @param iwreq
 * @param extra
 *
 * @return
 */
static int
__wext_get_rts(struct net_device *dev, struct iw_request_info *info,
                union iwreq_data *iwreq, char *extra)
{
    acfg_rts_t  rts ;
    QDF_STATUS          status = QDF_STATUS_E_INVAL;

    qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

    status = __wioctl_sw[ACFG_REQ_GET_RTS](netdev_to_softc(dev),
                                           (acfg_data_t *)&rts);

    iwreq->rts.value = rts.val ;

    if(rts.flags & ACFG_RTS_DISABLED)
        iwreq->rts.disabled = 1 ;
    else
        iwreq->rts.disabled = 0 ;

    if(rts.flags & ACFG_RTS_FIXED)
        iwreq->rts.fixed = 1 ;
    else
        iwreq->rts.fixed = 0 ;

    return __qdf_status_to_os(status);
}



/**
 * @brief Get default Tx Power (dBm)
 *
 * @param dev
 * @param info
 * @param iwreq
 * @param extra
 *
 * @return
 */
static int
__wext_get_txpow(struct net_device *dev, struct iw_request_info *info,
                union iwreq_data *iwreq, char *extra)
{
    acfg_txpow_t  txpow ;
    QDF_STATUS          status = QDF_STATUS_E_INVAL;

    qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

    qdf_mem_zero(&txpow, sizeof(acfg_txpow_t));
    status = __wioctl_sw[ACFG_REQ_GET_TXPOW](netdev_to_softc(dev),
                                           (acfg_data_t *)&txpow);

    iwreq->txpower.value = txpow.val ;
    iwreq->txpower.flags = IW_TXPOW_DBM ;

    if(txpow.flags & ACFG_TXPOW_DISABLED)
        iwreq->txpower.disabled = 1 ;
    else
        iwreq->txpower.disabled = 0 ;

    if(txpow.flags & ACFG_TXPOW_FIXED)
        iwreq->txpower.fixed = 1 ;
    else
        iwreq->txpower.fixed = 0 ;

    return __qdf_status_to_os(status);
}


/**
 * @brief Set default Tx Power (dBm)
 *
 * @param dev
 * @param info
 * @param iwreq
 * @param extra
 *
 * @return
 */
static int
__wext_set_txpow(struct net_device *dev, struct iw_request_info *info,
                union iwreq_data *iwreq, char *extra)
{
    acfg_txpow_t  txpow ;
    QDF_STATUS  status = QDF_STATUS_E_INVAL;

    qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

    txpow.val = iwreq->txpower.value ;
    txpow.flags = 0 ;

    if( (iwreq->txpower.flags & IW_TXPOW_TYPE) == IW_TXPOW_MWATT )
        return __qdf_status_to_os(QDF_STATUS_E_NOSUPPORT) ;

    if(iwreq->txpower.disabled)
        txpow.flags |= ACFG_TXPOW_DISABLED ;

    if(iwreq->txpower.fixed)
        txpow.flags |= ACFG_TXPOW_FIXED ;

    status = __wioctl_sw[ACFG_REQ_SET_TXPOW](netdev_to_softc(dev),
                                           (acfg_data_t *)&txpow);

    return __qdf_status_to_os(status);
}



/**
 * @brief Set fragmentation threshold
 *
 * @param dev
 * @param info
 * @param iwreq
 * @param extra
 *
 * @return
 */
static int
__wext_set_frag(struct net_device *dev, struct iw_request_info *info,
                union iwreq_data *iwreq, char *extra)
{
    acfg_frag_t  frag ;
    QDF_STATUS          status = QDF_STATUS_E_INVAL;

    frag.val = iwreq->frag.value ;
    frag.flags = 0 ;

    if(iwreq->frag.disabled)
        frag.flags |= ACFG_FRAG_DISABLED ;

    if(iwreq->frag.fixed)
        frag.flags |= ACFG_FRAG_FIXED ;

    qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): frag= %d; disabled= %d; fixed= %d\n",
			__FUNCTION__, iwreq->frag.value,
			iwreq->frag.disabled, iwreq->frag.fixed);


    status = __wioctl_sw[ACFG_REQ_SET_FRAG](netdev_to_softc(dev),
                                           (acfg_data_t *)&frag);

    return __qdf_status_to_os(status);
}



/**
 * @brief Get fragmentation threshold
 *
 * @param dev
 * @param info
 * @param iwreq
 * @param extra
 *
 * @return
 */
static int
__wext_get_frag(struct net_device *dev, struct iw_request_info *info,
                union iwreq_data *iwreq, char *extra)
{
    acfg_frag_t  frag ;
    QDF_STATUS          status = QDF_STATUS_E_INVAL;

    qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

    status = __wioctl_sw[ACFG_REQ_GET_FRAG](netdev_to_softc(dev),
                                           (acfg_data_t *)&frag);

    iwreq->frag.value = frag.val ;

    if(frag.flags & ACFG_FRAG_DISABLED)
        iwreq->frag.disabled = 1 ;
    else
        iwreq->frag.disabled = 0 ;

    if(frag.flags & ACFG_FRAG_FIXED)
        iwreq->frag.fixed = 1 ;
    else
        iwreq->frag.fixed = 0 ;

    qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): frag = %d\n",__FUNCTION__,iwreq->frag.value);
    return __qdf_status_to_os(status);
}


/**
 * @brief Set Access Point Mac Address
 *
 * @param dev
 * @param info
 * @param iwreq
 * @param extra
 *
 * @return
 */
static int
__wext_set_ap(struct net_device *dev, struct iw_request_info *info,
                union iwreq_data *iwreq, char *extra)
{
    acfg_macaddr_t      mac ;
    QDF_STATUS          status = QDF_STATUS_E_INVAL;

    qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

    memcpy(mac.addr, iwreq->ap_addr.sa_data, ACFG_MACADDR_LEN);

    status = __wioctl_sw[ACFG_REQ_SET_AP](netdev_to_softc(dev),
                                           (acfg_data_t *)&mac);

    return __qdf_status_to_os(status);
}


/**
 * @brief Get Access Point Mac Address
 *
 * @param dev
 * @param info
 * @param iwreq
 * @param extra
 *
 * @return
 */
static int
__wext_get_ap(struct net_device *dev, struct iw_request_info *info,
                union iwreq_data *iwreq, char *extra)
{
    acfg_macaddr_t      mac ;
    QDF_STATUS          status = QDF_STATUS_E_INVAL;

    qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

    status = __wioctl_sw[ACFG_REQ_GET_AP](netdev_to_softc(dev),
                                           (acfg_data_t *)&mac);

    iwreq->ap_addr.sa_family = AF_INET ;
    qdf_mem_copy(iwreq->ap_addr.sa_data,\
            (char *)mac.addr ,ACFG_MACADDR_LEN);

    return __qdf_status_to_os(status);
}



/**
 * @brief Get Range of parameters
 *
 * @param dev
 * @param info
 * @param iwreq
 * @param extra
 *
 * @return
 */
static int
__wext_get_range(struct net_device *dev, struct iw_request_info *info,
                union iwreq_data *iwreq, char *extra)
{
    acfg_range_t range ;
    QDF_STATUS status = QDF_STATUS_E_INVAL;


    qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): length %d \n",\
                            __FUNCTION__,iwreq->data.length);

    range.buff          = (uint8_t *)extra ;
    range.len           = sizeof(struct iw_range);

    status = __wioctl_sw[ACFG_REQ_GET_RANGE](netdev_to_softc(dev),
                                           (acfg_data_t *)&range);

    return __qdf_status_to_os(status);
}



/**
 * @brief Set encoding token and mode info
 *
 * @param dev
 * @param info
 * @param iwreq
 * @param extra
 *
 * @return
 */
static int
__wext_set_encode(struct net_device *dev, struct iw_request_info *info,
                union iwreq_data *iwreq, char *extra)
{
    QDF_STATUS          status = QDF_STATUS_E_INVAL;
    acfg_encode_t enc ;

    enc.len = iwreq->data.length;
    enc.flags = iwreq->data.flags;
    enc.buff = iwreq->data.pointer;

    qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): len = %d ; flags = 0x%04x \n",\
              __FUNCTION__, enc.len, enc.flags);

    status = __wioctl_sw[ACFG_REQ_SET_ENCODE](netdev_to_softc(dev), \
            (acfg_data_t *)&enc);

    return __qdf_status_to_os(status);
}



/**
 * @brief Get encoding token and mode info
 *
 * @param dev
 * @param info
 * @param iwreq
 * @param extra
 *
 * @return
 */
static int
__wext_get_encode(struct net_device *dev, struct iw_request_info *info,
                union iwreq_data *iwreq, char *extra)
{
    acfg_encode_t enc ;
    QDF_STATUS status = QDF_STATUS_E_INVAL;

    qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

    enc.buff = (uint8_t *)extra ;
    enc.len = iwreq->encoding.length ;
    status = __wioctl_sw[ACFG_REQ_GET_ENCODE](netdev_to_softc(dev),
                                           (acfg_data_t *)&enc);

    iwreq->encoding.length = enc.len ;
    iwreq->encoding.flags = enc.flags ;

    return __qdf_status_to_os(status);
}



/**
 * @brief Set default bitrate
 *
 * @param dev
 * @param info
 * @param iwreq
 * @param extra
 *
 * @return
*/
static int
__wext_set_rate(struct net_device *dev, struct iw_request_info *info,
                        union iwreq_data *iwreq, char *extra)
{
    acfg_rate_t bitrate ;
    QDF_STATUS status = QDF_STATUS_E_INVAL;

    qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

    bitrate.fixed = iwreq->bitrate.fixed;
    bitrate.value = iwreq->bitrate.value;

    status = __wioctl_sw[ACFG_REQ_SET_RATE](netdev_to_softc(dev),
                                     (acfg_data_t *)&bitrate);

    return __qdf_status_to_os(status);
}



/**
 * @brief Get default bitrate
 *
 * @param dev
 * @param info
 * @param iwreq
 * @param extra
 *
 * @return
*/
static int
__wext_get_rate(struct net_device *dev, struct iw_request_info *info,
                        union iwreq_data *iwreq, char *extra)
{
    uint32_t bitrate ;
    QDF_STATUS status = QDF_STATUS_E_INVAL;

    qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

    status = __wioctl_sw[ACFG_REQ_GET_RATE](netdev_to_softc(dev),
                                     (acfg_data_t *)&bitrate);
    iwreq->bitrate.value = bitrate ;

    return __qdf_status_to_os(status);
}

/**
 * @brief Set power management
 *
 * @param dev
 * @param info
 * @param iwreq
 * @param extra
 *
 * @return
 */
static int
__wext_set_power(struct net_device *dev, struct iw_request_info *info,
                union iwreq_data *iwreq, char *extra)
{
    QDF_STATUS status = QDF_STATUS_E_INVAL;
    acfg_powmgmt_t pm;

    pm.disabled = iwreq->power.disabled;
    pm.flags = iwreq->power.flags;
    pm.val = iwreq->power.value;

    qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): disabled = %d ; flags = 0x%04x ; val = %d\n",\
              __FUNCTION__, pm.disabled, pm.flags, pm.val);

    status = __wioctl_sw[ACFG_REQ_SET_POWMGMT](netdev_to_softc(dev), \
            (acfg_data_t *)&pm);

    return __qdf_status_to_os(status);
}

/**
 * @brief Get power management
 *
 * @param dev
 * @param info
 * @param iwreq
 * @param extra
 *
 * @return
 */
static int
__wext_get_power(struct net_device *dev, struct iw_request_info *info,
                union iwreq_data *iwreq, char *extra)
{
    QDF_STATUS status = QDF_STATUS_E_INVAL;
    acfg_powmgmt_t pm;

    qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

    status = __wioctl_sw[ACFG_REQ_GET_POWMGMT](netdev_to_softc(dev),
                                           (acfg_data_t *)&pm);

    iwreq->power.value = pm.val;
    iwreq->power.flags = pm.flags;
    iwreq->power.disabled = pm.disabled;

    return __qdf_status_to_os(status);
}



/**
 * @brief Translate Scan results to WE format
 *
 * @param arg
 * @param sr
 *
 * @return
 */
static int
__giwscan_cb(void *arg, struct acfg_scan_result * sr)
{
    struct iwscanreq *req         = (struct iwscanreq *)arg;
    uint8_t *current_ev         = (uint8_t *)(req->current_ev);
    uint8_t *end_buf            = (uint8_t *)(req->end_buf);
    struct iw_request_info *info  = req->info;
    uint8_t buf[MAX_IE_LENGTH];

    uint8_t *last_ev;
    struct iw_event iwe;
    uint8_t *current_val;
    int j;

    uint8_t *se_macaddr  = sr->isr_bssid;
    uint8_t *se_bssid    = sr->isr_bssid;
    uint8_t  se_ssid_len = sr->isr_ssid_len;
    uint8_t *se_ssid     = NULL;
    uint8_t  se_rssi     = sr->isr_rssi;
    uint8_t *se_rates    = sr->isr_rates;
    uint16_t se_intval   = sr->isr_intval;
    uint8_t *se_xrates   = NULL;
    uint8_t *vp          = NULL;


    struct acfg_ie_list ie;

    /* Sanity Check */
    if( sr->isr_len == 0 )
        return 0;

    qdf_mem_zero(&ie, sizeof(struct acfg_ie_list));

    vp = (uint8_t *)(sr+1);
    se_xrates = (sr->isr_rates +
                 acfg_min((int32_t)se_rates[1], IEEE80211_RATE_MAXSIZE));

    /* Populate IEs */
    __parseie(vp + sr->isr_ssid_len, sr->isr_ie_len, &ie );

    if (current_ev >= end_buf) {
        return QDF_STATUS_E_E2BIG;
    }

    /* WPA / !WPA Sort Criteria  */
    if((req->mode != 0) ^ ((ie.se_wpa_ie != NULL) || (ie.se_rsn_ie != NULL)))
    {
        return 0;
    }

    qdf_mem_zero(&iwe, sizeof(iwe));
    last_ev = current_ev;
    iwe.cmd = SIOCGIWAP;
    iwe.u.ap_addr.sa_family = ARPHRD_ETHER;
    /*    if (0 &&  opmode == IEEE80211_M_HOSTAP ) */
    if (0){
        qdf_mem_copy(iwe.u.ap_addr.sa_data, se_macaddr,IEEE80211_ADDR_LEN);
    } else {
        qdf_mem_copy(iwe.u.ap_addr.sa_data, se_bssid,IEEE80211_ADDR_LEN);
    }

    current_ev = iwe_stream_add_event(info,current_ev,
            end_buf, &iwe, IW_EV_ADDR_LEN);

    /* We ran out of space in the buffer. */
    if (last_ev == current_ev) {
        return QDF_STATUS_E_E2BIG;
    }

    qdf_mem_zero(&iwe, sizeof(iwe));
    se_ssid = vp;
    last_ev = current_ev;
    iwe.cmd = SIOCGIWESSID;
    iwe.u.data.flags = 1;
    {
        iwe.u.data.length = se_ssid_len;
        current_ev = iwe_stream_add_point(info,current_ev,
                end_buf, &iwe, (uint8_t *) se_ssid);
    }

    /* We ran out of space in the buffer. */
    if (last_ev == current_ev) {
        return QDF_STATUS_E_E2BIG;
    }

    qdf_mem_zero(&iwe, sizeof(iwe));
    last_ev = current_ev;
    iwe.cmd = SIOCGIWMODE;
    iwe.u.mode = IW_MODE_MASTER;

    current_ev = iwe_stream_add_event(info,current_ev,
            end_buf, &iwe, IW_EV_UINT_LEN);
    /* We ran out of space in the buffer. */
    if (last_ev == current_ev) {
        return QDF_STATUS_E_E2BIG;
    }


    qdf_mem_zero(&iwe, sizeof(iwe));
    last_ev = current_ev;
    iwe.cmd = SIOCGIWFREQ;
    iwe.u.freq.m = sr->isr_freq * 100000;
    iwe.u.freq.e = 1;

    current_ev = iwe_stream_add_event(info,current_ev,
            end_buf, &iwe, IW_EV_FREQ_LEN);

    /* We ran out of space in the buffer. */
    if (last_ev == current_ev) {
        return QDF_STATUS_E_E2BIG;
    }

    qdf_mem_zero(&iwe, sizeof(iwe));
    last_ev = current_ev;
    iwe.cmd = IWEVQUAL;
    __set_quality(&iwe.u.qual, se_rssi);

    current_ev = iwe_stream_add_event(info,current_ev,
            end_buf, &iwe, IW_EV_QUAL_LEN);

    /* We ran out of space in the buffer. */
    if (last_ev == current_ev) {
        return QDF_STATUS_E_E2BIG;
    }


    qdf_mem_zero(&iwe, sizeof(iwe));
    last_ev = current_ev;
    iwe.cmd = SIOCGIWENCODE;
    if (sr->isr_capinfo & IEEE80211_CAPINFO_PRIVACY) {
        iwe.u.data.flags = IW_ENCODE_ENABLED | IW_ENCODE_NOKEY;
    } else {
        iwe.u.data.flags = IW_ENCODE_DISABLED;
    }
    iwe.u.data.length = 0;
    current_ev = iwe_stream_add_point(info,current_ev, end_buf, &iwe, "");

    /* We ran out of space in the buffer. */
    if (last_ev == current_ev) {
        return QDF_STATUS_E_E2BIG;
    }

#define IEEE80211_RATE_VAL 0x7f
    qdf_mem_zero(&iwe, sizeof(iwe));
    last_ev = current_ev;
    iwe.cmd = SIOCGIWRATE;
    current_val = current_ev + IW_EV_LCP_LEN;
    /* NB: not sorted, does it matter? */
    if (se_rates != NULL) {
        for (j = 0; j < sr->isr_nrates; j++) {
            int r = se_rates[j] & IEEE80211_RATE_VAL;
            if (r != 0) {
                iwe.u.bitrate.value = r * (1000000 / 2);
                current_val = iwe_stream_add_value(info,current_ev,
                    current_val, end_buf, &iwe,
                    IW_EV_PARAM_LEN);
             }
        }
    }

    if ( ie.xrates_ie ) {
        for (j = 0; j < ie.xrates_ie[1]; j++) {
            int r = ie.xrates_ie[2+j] & IEEE80211_RATE_VAL;
            if (r != 0) {
                iwe.u.bitrate.value = r * (1000000 / 2);
                current_val = iwe_stream_add_value(info,current_ev,
                        current_val, end_buf, &iwe,
                        IW_EV_PARAM_LEN);
            }
        }
    }
    /* remove fixed header if no rates were added */
    if ((current_val - current_ev) > IW_EV_LCP_LEN) {
        current_ev = current_val;
    } else {
        /* We ran out of space in the buffer. */
        if (last_ev == current_ev)
            return QDF_STATUS_E_E2BIG;
    }


    qdf_mem_zero(&iwe, sizeof(iwe));
    last_ev = current_ev;
    iwe.cmd = IWEVCUSTOM;
    snprintf(buf, sizeof(buf), "bcn_int=%d", se_intval);
    iwe.u.data.length = strlen(buf);
    current_ev = iwe_stream_add_point(info,current_ev, end_buf, &iwe, buf);

    /* We ran out of space in the buffer. */
    if (last_ev == current_ev) {
        return QDF_STATUS_E_E2BIG;
    }

    if (ie.se_rsn_ie != NULL) {
        last_ev = current_ev;

        qdf_mem_zero(&iwe, sizeof(iwe));

        if ((ie.se_rsn_ie[1] + 2) > MAX_IE_LENGTH) {
            return QDF_STATUS_E_E2BIG;
        }

        qdf_mem_copy(buf, ie.se_rsn_ie, ie.se_rsn_ie[1] + 2);

        iwe.cmd = IWEVGENIE;
        iwe.u.data.length = ie.se_rsn_ie[1] + 2;


        if (iwe.u.data.length != 0) {
            current_ev = iwe_stream_add_point(info,current_ev, end_buf,
                                              &iwe, buf);

            /* We ran out of space in the buffer */
            if (last_ev == current_ev) return QDF_STATUS_E_E2BIG;
        }

    }


    if (ie.se_wpa_ie != NULL) {
        last_ev = current_ev;

        qdf_mem_zero(&iwe, sizeof(iwe));

        if ((ie.se_wpa_ie[1] + 2) > MAX_IE_LENGTH) return QDF_STATUS_E_E2BIG;

        qdf_mem_copy(buf, ie.se_wpa_ie, ie.se_wpa_ie[1] + 2);

        iwe.cmd = IWEVGENIE;
        iwe.u.data.length = ie.se_wpa_ie[1] + 2;

        if (iwe.u.data.length != 0) {
            current_ev = iwe_stream_add_point(info,current_ev, end_buf,
                                              &iwe, buf);

            /* We ran out of space in the buffer. */
            if (last_ev == current_ev) return QDF_STATUS_E_E2BIG;
        }

    }

    if (ie.se_wmeparam != NULL) {
        static const uint8_t wme_leader[] = "wme_ie=";

        qdf_mem_zero(&iwe, sizeof(iwe));
        last_ev = current_ev;
        iwe.cmd = IWEVCUSTOM;
        iwe.u.data.length = __encode_ie(buf, sizeof(buf),
                ie.se_wmeparam, ie.se_wmeparam[1]+2,
                wme_leader, sizeof(wme_leader)-1);
        if (iwe.u.data.length != 0) {
            current_ev = iwe_stream_add_point(info, current_ev, end_buf,
                                              &iwe, buf);
        }

        /* We ran out of space in the buffer. */
        if (last_ev == current_ev) return QDF_STATUS_E_E2BIG;

    } else if (ie.se_wmeinfo != NULL) {
        static const uint8_t wme_leader[] = "wme_ie=";

        qdf_mem_zero(&iwe, sizeof(iwe));
        last_ev = current_ev;
        iwe.cmd = IWEVCUSTOM;
        iwe.u.data.length = __encode_ie(buf, sizeof(buf),
                                        ie.se_wmeinfo,
                                        ie.se_wmeinfo[1]+2,
                                        wme_leader,
                                        sizeof(wme_leader)-1);
        if (iwe.u.data.length != 0)
            current_ev = iwe_stream_add_point(info,current_ev, end_buf,
                                              &iwe, buf);


        /* We ran out of space in the buffer. */
        if (last_ev == current_ev) return QDF_STATUS_E_E2BIG;

    }
    if (ie.se_ath_ie != NULL) {
        static const uint8_t ath_leader[] = "ath_ie=";

        qdf_mem_zero(&iwe, sizeof(iwe));

        last_ev = current_ev;
        iwe.cmd = IWEVCUSTOM;
        iwe.u.data.length = __encode_ie(buf, sizeof(buf),
                                        ie.se_ath_ie,
                                        ie.se_ath_ie[1]+2,
                                        ath_leader,
                                        sizeof(ath_leader)-1);
        if (iwe.u.data.length != 0)
            current_ev = iwe_stream_add_point(info,current_ev, end_buf,
                                              &iwe, buf);

        /* We ran out of space in the buffer. */
        if (last_ev == current_ev) return QDF_STATUS_E_E2BIG;
    }

    if (ie.se_wps_ie != NULL) {
        last_ev = current_ev;
        qdf_mem_zero(&iwe, sizeof(iwe));
        if ((ie.se_wps_ie[1] + 2) > MAX_IE_LENGTH) {
            return QDF_STATUS_E_E2BIG;
        }
        qdf_mem_copy(buf, ie.se_wps_ie,ie. se_wps_ie[1] + 2);
        iwe.cmd = IWEVGENIE;
        iwe.u.data.length = ie.se_wps_ie[1] + 2;

        if (iwe.u.data.length != 0) {
            current_ev = iwe_stream_add_point(info, current_ev, end_buf,
                                              &iwe, buf);

            /* We ran out of space in the buffer */
            if (last_ev == current_ev) return QDF_STATUS_E_E2BIG;
        }
    }

    req->current_ev = current_ev;

    return 0;
}



/**
 * @brief Get the Scan Results
 *
 * @param dev
 * @param info
 * @param iwreq
 * @param extra
 *
 * @return status
 */
static int
__wext_get_scanresults(struct net_device *dev, struct iw_request_info *info,
                union iwreq_data *iwreq, char *extra)
{
    acfg_scan_t   *scan       = NULL;
    acfg_data_t   *data       = NULL;
    uint8_t     *cp         = NULL;
    int8_t      *extra_ptr  = NULL;
    QDF_STATUS     status     = QDF_STATUS_E_INVAL;
    uint16_t     iterate    = 0;

    struct acfg_scan_result *sr;
    struct iwscanreq req;

    qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

    /* XXX: Optimize  */
    scan = vmalloc(sizeof(struct acfg_scan));
    data = vmalloc(sizeof(union acfg_data));

    if (scan == NULL || data == NULL) {
        printk("%s : Mem Allocation Fail\n",__FUNCTION__);
        if (scan)   vfree(scan);
        if (data)   vfree(data);
        status = QDF_STATUS_E_NOMEM;
        return __qdf_status_to_os(status);
    }

    data->scan = (acfg_scan_t *)scan;

    if (iwreq->essid.flags)
        scan->len = acfg_min(iwreq->data.length, 1500);
    else
        scan->len = iwreq->data.length;

    scan->results = qdf_mem_malloc(scan->len);

    if (scan->results == NULL) {
        printk("%s : Mem Allocation Fail\n",__FUNCTION__);
        vfree(scan);
        vfree(data);
        status = QDF_STATUS_E_NOMEM;
        return __qdf_status_to_os(status);
    }

    status = __wioctl_sw[ACFG_REQ_GET_SCANRESULTS](netdev_to_softc(dev), data);

    if (status != 0)
        goto fail;

    extra_ptr      = extra;
    req.current_ev = extra;
    req.info       = info;
    if (iwreq->data.length == 0) {
        req.end_buf = extra + IW_SCAN_MAX_DATA;
    } else {
        req.end_buf = extra + iwreq->data.length;
    }

    /*
     * Do two passes to insure WPA/non-WPA scan candidates
     * are sorted to the front.  This is a hack to deal with
     * the wireless extensions capping scan results at
     * IW_SCAN_MAX_DATA bytes.  In densely populated environments
     * it's easy to overflow this buffer (especially with WPA/RSN
     * information elements).  Note this sorting hack does not
     * guarantee we won't overflow anyway.
     * Reference : Newma
     */
#define IEEE80211_AUTH_WPA  0x1
#define IEEE80211_AUTH_RSNA 0x2
    req.mode = IEEE80211_AUTH_WPA | IEEE80211_AUTH_RSNA;
    iterate = 0;
    do {
        cp = scan->results;
        sr = (struct acfg_scan_result *) cp;
        do {
            status = __giwscan_cb(&req, sr);
            if( status != 0 ) {
                iwreq->data.length = (req.current_ev - extra_ptr);
                goto fail;
            }
            cp += sr->isr_len;
            sr = (struct acfg_scan_result *) cp;
        } while(sr->isr_len != 0);
        iterate++;
        req.mode = req.mode ? 0 : 0x3;
    } while(iterate < 2);

    iwreq->data.length = (req.current_ev - extra_ptr);

fail:
    qdf_mem_free(scan->results);
    vfree(scan);
    vfree(data);

    return __qdf_status_to_os(status);
}


/**
 * @brief Set the Scan Request
 *
 * @param dev
 * @param info
 * @param iwreq
 * @param extra
 *
 * @return
 */
static int
__wext_set_scan(struct net_device *dev, struct iw_request_info *info,
                union iwreq_data *iwreq, char *extra)
{
    acfg_set_scan_t         *scan ;
    QDF_STATUS          status = QDF_STATUS_E_INVAL;

    qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

    scan = vmalloc(sizeof(struct acfg_set_scan));

    memset(scan, 0, sizeof(acfg_set_scan_t));

    if(!copy_from_user(scan, iwreq->data.pointer, iwreq->data.length)) {
        qdf_print("%s:%d copy from user failed", __func__, __LINE__);
        status = QDF_STATUS_E_FAULT;
        goto err;
    }
    scan->len = iwreq->data.length;
    scan->point_flags = iwreq->data.flags;

    status = __wioctl_sw[ACFG_REQ_SET_SCAN](netdev_to_softc(dev),
                                           (acfg_data_t *)scan);
err:
    vfree(scan);

    return __qdf_status_to_os(status);
}


static struct iw_statistics iw_stats_vap ;

static
struct iw_statistics* __wext_getstats_vap(struct net_device *dev)
{
    acfg_stats_t stats ;
    QDF_STATUS status = QDF_STATUS_E_INVAL;

    qdf_trace(QDF_DEBUG_FUNCTRACE, "QDF_WEXT: %s(): \n",__FUNCTION__);

    status = __wioctl_sw[ACFG_REQ_GET_STATS](netdev_to_softc(dev),
                                           (acfg_data_t *)&stats);

    if(status != QDF_STATUS_SUCCESS)
        return NULL ;

    iw_stats_vap.status = stats.status ;
    iw_stats_vap.qual.qual = stats.link_quality ;
    iw_stats_vap.qual.level = stats.signal_level ;
    iw_stats_vap.qual.noise = stats.noise_level ;
    iw_stats_vap.qual.updated = stats.updated ;
    iw_stats_vap.discard.nwid = stats.discard_nwid ;
    iw_stats_vap.miss.beacon = stats.missed_beacon ;

    return &iw_stats_vap ;
}

/**********************************************************************/
/******************   PRIVATE HANDLERS ********************************/
/**********************************************************************/


/**
 * @brief Main handler to set VAP parameters
 *
 * @param dev
 * @param info
 * @param iwreq
 * @param extra
 *
 * @return
 */
static int
__wpriv_set_param_vap(struct net_device *dev, struct iw_request_info *info,
                    union iwreq_data *iwreq, char *extra)
{
    QDF_STATUS status = QDF_STATUS_E_INVAL;
    int *i = (int *)extra ;
    acfg_data_t *data = NULL;

    qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): param - %d; val - %d\n",\
                                                __FUNCTION__,i[0],i[1]);
    data = vmalloc(sizeof(acfg_param_req_t));
    if (!data)
        return __qdf_status_to_os(status);

    data->param_req.param = i[0] ;
    data->param_req.val = i[1] ;

    status = __wioctl_sw[ACFG_REQ_SET_VAP_PARAM](netdev_to_softc(dev), data);
    vfree(data);

    return __qdf_status_to_os(status);
}


/**
 * @brief Main handler to get VAP parameters
 *
 * @param dev
 * @param info
 * @param iwreq
 * @param extra
 *
 * @return
 */
static int
__wpriv_get_param_vap(struct net_device *dev, struct iw_request_info *info,
                    union iwreq_data *iwreq, char *extra)
{
    QDF_STATUS status = QDF_STATUS_E_INVAL;
    int param = *(int *)extra ;
    acfg_data_t *data = NULL;

    data = vmalloc(sizeof(acfg_param_req_t));
    if (!data)
        return __qdf_status_to_os(status);

    qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): param - %d \n",__FUNCTION__,param);

    data->param_req.param = param ;
    data->param_req.val = 0 ;

    status = __wioctl_sw[ACFG_REQ_GET_VAP_PARAM](netdev_to_softc(dev), data);

    *(uint32_t *)extra = data->param_req.val ;
    vfree(data);

    return __qdf_status_to_os(status);
}

/**
 * @brief Main handler to set VAP wmm parameters
 *
 * @param dev
 * @param info
 * @param iwreq
 * @param extra
 *
 * @return
 */
static int
__wpriv_set_wmmparams_vap(struct net_device *dev, struct iw_request_info *info,
                    union iwreq_data *iwreq, char *extra)
{
    QDF_STATUS status = QDF_STATUS_E_INVAL;
    int *i = (int *)extra ;
    acfg_data_t *data = NULL;

    qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): param - %d; val - %d\n",\
                                                __FUNCTION__,i[0],i[1]);
    data = vmalloc(sizeof(acfg_wmmparams_req_t));
    if (!data)
        return __qdf_status_to_os(status);
    data->wmmparams_req.param[0] = i[0] ;//wmm param
	data->wmmparams_req.param[1] = i[1] ;//ac
    data->wmmparams_req.param[2] = i[2] ;//bss
    data->wmmparams_req.val = i[3] ;

    status = __wioctl_sw[ACFG_REQ_SET_VAP_WMMPARAMS](netdev_to_softc(dev), data);

    vfree(data);
    return __qdf_status_to_os(status);
}

/**
 * @brief Main handler to get VAP wmm parameters
 *
 * @param dev
 * @param info
 * @param iwreq
 * @param extra
 *
 * @return
 */
static int
__wpriv_get_wmmparams_vap(struct net_device *dev, struct iw_request_info *info,
                    union iwreq_data *iwreq, char *extra)
{
    QDF_STATUS status = QDF_STATUS_E_INVAL;
    int param = *(int *)extra ;
    acfg_data_t *data = NULL;

    data = vmalloc(sizeof(acfg_wmmparams_req_t));
    if (!data)
        return __qdf_status_to_os(status);
    qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): param - %d \n",__FUNCTION__,param);
    data->wmmparams_req.param[0] = *(int *)extra ;//wmm param
	data->wmmparams_req.param[1] = *((int *)extra+1) ;//ac
	data->wmmparams_req.param[2] = *((int *)extra+2) ;//bss
    data->wmmparams_req.val = 0 ;

    status = __wioctl_sw[ACFG_REQ_GET_VAP_WMMPARAMS](netdev_to_softc(dev), data);

    *(uint32_t *)extra = data->wmmparams_req.val ;
    vfree(data);
    return __qdf_status_to_os(status);
}

/**
 * @brief Main handler to set Radio parameters
 *
 * @param dev
 * @param info
 * @param iwreq
 * @param extra
 *
 * @return
 */
static int
__wpriv_set_param_radio(struct net_device *dev, struct iw_request_info *info,
                    union iwreq_data *iwreq, char *extra)
{
    QDF_STATUS status = QDF_STATUS_E_INVAL;
    int *i = (int *)extra ;
    acfg_data_t *data = NULL;

    qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): param - %d; val - %d\n",\
                                                __FUNCTION__,i[0],i[1]);

    data = vmalloc(sizeof(acfg_param_req_t));
    if (!data)
        return __qdf_status_to_os(status);
    data->param_req.param = i[0] ;
    data->param_req.val = i[1] ;

    status = __wioctl_sw[ACFG_REQ_SET_RADIO_PARAM](netdev_to_softc(dev), data);

    vfree(data);
    return __qdf_status_to_os(status);
}


/**
 * @brief Main handler to get Radio parameters
 *
 * @param dev
 * @param info
 * @param iwreq
 * @param extra
 *
 * @return
 */
static int
__wpriv_get_param_radio(struct net_device *dev, struct iw_request_info *info,
                    union iwreq_data *iwreq, char *extra)
{
    QDF_STATUS status = QDF_STATUS_E_INVAL;
    int param = *(int *)extra ;
    acfg_data_t *data = NULL;

    qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): param - %d \n",__FUNCTION__,param);
    data = vmalloc(sizeof(acfg_param_req_t));
    if (!data)
        return __qdf_status_to_os(status);

    data->param_req.param = param ;
    data->param_req.val = 0 ;

    status = __wioctl_sw[ACFG_REQ_GET_RADIO_PARAM](netdev_to_softc(dev), data);

    *(uint32_t *)extra = data->param_req.val ;
    vfree(data);

    return __qdf_status_to_os(status);
}


/**********************************************************************/
/******************   PRIVATE HANDLERS ********************************/
/**********************************************************************/


/**
 * @brief Main handler to set channel mode
 *
 * @param dev
 * @param info
 * @param iwreq
 * @param extra
 *
 * @return
 */
static int
__wpriv_set_chmode(struct net_device *dev, struct iw_request_info *info,
                    union iwreq_data *iwreq, char *extra)
{
    QDF_STATUS status = QDF_STATUS_E_INVAL;
    acfg_data_t *data = NULL;

    data = vmalloc(sizeof(acfg_chmode_t));
    if (!data)
        return __qdf_status_to_os(status);
    data->chmode.len = iwreq->data.length;
    memcpy(data->chmode.mode, iwreq->data.pointer, iwreq->data.length);
    data->chmode.mode[data->chmode.len-1] = '\0';

    qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): param - %s\n",\
                                                __FUNCTION__,data->chmode.mode);

    status = __wioctl_sw[ACFG_REQ_SET_CHMODE](netdev_to_softc(dev), data);

    vfree(data);

    return __qdf_status_to_os(status);
}

static int
__wpriv_doth_chswitch(struct net_device *dev, struct iw_request_info *info,
                    union iwreq_data *iwreq, char *extra)
{
    QDF_STATUS status = QDF_STATUS_E_INVAL;
    acfg_data_t *data = NULL;
    int *i = (int *)extra ;

    data = vmalloc(sizeof(acfg_doth_chsw_t));
    if (!data)
        return __qdf_status_to_os(status);
    data->doth_chsw.channel = i[0] ;
    data->doth_chsw.time = i[1] ;

    qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): param - %u %u\n",\
     __FUNCTION__,data->doth_chsw.channel,data->doth_chsw.time);

    status = __wioctl_sw[ACFG_REQ_DOTH_CHSWITCH](netdev_to_softc(dev), data);
    vfree(data);

    return __qdf_status_to_os(status);
}

/**
 * @brief
 *
 * @param dev
 * @param info
 * @param iwreq
 * @param extra
 *
 * @return
 */
static int
__wpriv_addmac(struct net_device *dev, struct iw_request_info *info,
                    union iwreq_data *iwreq, char *extra)
{
    QDF_STATUS status = QDF_STATUS_E_INVAL;
    acfg_data_t *data = NULL;
    uint32_t count ;
    struct sockaddr *sa ;

    data = vmalloc(sizeof(acfg_macaddr_t));
    if (!data)
        return __qdf_status_to_os(status);
    sa = (struct sockaddr *)extra ;
    for(count = 0 ; count < ACFG_MACADDR_LEN; count++)
    {
        data->macaddr.addr[count] = sa->sa_data[count] ;
    }

    qdf_trace(QDF_DEBUG_FUNCTRACE,"%s(): sending addr - ",__FUNCTION__);
    for(count = 0 ; count < ACFG_MACADDR_LEN; count++)
    {
        qdf_trace(QDF_DEBUG_FUNCTRACE,"%x:",data->macaddr.addr[count]  );
    }
    qdf_trace(QDF_DEBUG_FUNCTRACE,  "\n");

    status = __wioctl_sw[ACFG_REQ_ADDMAC](netdev_to_softc(dev), data);

    vfree(data);
    return __qdf_status_to_os(status);

}


/**
 * @brief Delete Mac address from ACL list
 *
 * @param dev
 * @param info
 * @param iwreq
 * @param extra
 *
 * @return
 */
static int
__wpriv_delmac(struct net_device *dev, struct iw_request_info *info,
                    union iwreq_data *iwreq, char *extra)
{
    QDF_STATUS status = QDF_STATUS_E_INVAL;
    acfg_data_t *data = NULL;
    uint32_t count ;
    struct sockaddr *sa ;

    data = vmalloc(sizeof(acfg_macaddr_t));
    if (!data)
        return __qdf_status_to_os(status);
    sa = (struct sockaddr *)extra ;
    for(count = 0 ; count < ACFG_MACADDR_LEN; count++)
    {
        data->macaddr.addr[count] = sa->sa_data[count] ;
    }

    qdf_trace(QDF_DEBUG_FUNCTRACE,"%s(): sending addr - ",__FUNCTION__);
    for(count = 0 ; count < ACFG_MACADDR_LEN; count++)
    {
        qdf_trace(QDF_DEBUG_FUNCTRACE,"%x:",data->macaddr.addr[count]  );
    }
    qdf_trace(QDF_DEBUG_FUNCTRACE,  "\n");

    status = __wioctl_sw[ACFG_REQ_DELMAC](netdev_to_softc(dev), data);

    vfree(data);
    return __qdf_status_to_os(status);

}



/**
 * @brief Disassociate station from AP
 *
 * @param dev
 * @param info
 * @param iwreq
 * @param extra
 *
 * @return
 */
static int
__wpriv_kickmac(struct net_device *dev, struct iw_request_info *info,
                    union iwreq_data *iwreq, char *extra)
{
    QDF_STATUS status = QDF_STATUS_E_INVAL;
    acfg_data_t *data = NULL ;
    uint32_t count ;
    struct sockaddr *sa ;

    data = vmalloc(sizeof(acfg_macaddr_t));
    if (!data)
        return __qdf_status_to_os(status);
    sa = (struct sockaddr *)extra ;
    for(count = 0 ; count < ACFG_MACADDR_LEN; count++)
    {
        data->macaddr.addr[count] = sa->sa_data[count] ;
    }

    qdf_trace(QDF_DEBUG_FUNCTRACE,"%s(): sending addr - ",__FUNCTION__);
    for(count = 0 ; count < ACFG_MACADDR_LEN; count++)
    {
        qdf_trace(QDF_DEBUG_FUNCTRACE,"%x:",data->macaddr.addr[count]  );
    }
    qdf_trace(QDF_DEBUG_FUNCTRACE,  "\n");

    status = __wioctl_sw[ACFG_REQ_KICKMAC](netdev_to_softc(dev), data);
    vfree(data);

    return __qdf_status_to_os(status);

}



/**
 * @brief Set MLME
 *
 * @param dev
 * @param info
 * @param iwreq
 * @param extra
 *
 * @return
 */
static int
__wpriv_set_mlme(struct net_device *dev, struct iw_request_info *info,
                    union iwreq_data *iwreq, char *extra)
{
    QDF_STATUS status = QDF_STATUS_E_NOMEM;
    acfg_mlme_t *mlme = NULL;
    acfg_data_t *data = NULL;

    mlme = vmalloc(iwreq->data.length);
    if (!mlme)
        return __qdf_status_to_os(status);

    data = vmalloc(sizeof(acfg_mlme_t));
    if(!data) {
	    vfree(mlme);
	    return __qdf_status_to_os(status);
    }

    data->mlme = mlme;

    if(!copy_from_user(mlme, iwreq->data.pointer, iwreq->data.length)) {
        qdf_print("%s:%d copy from user failed", __func__, __LINE__);
        status = QDF_STATUS_E_FAULT;
        goto err;
    }

    qdf_trace(QDF_DEBUG_FUNCTRACE,  "\n");

    status = __wioctl_sw[ACFG_REQ_SET_MLME](netdev_to_softc(dev), data);

err:
    vfree(mlme);
    vfree(data);

    return __qdf_status_to_os(status);
}



/**
 * @brief Set Optional IE
 *
 * @param dev
 * @param info
 * @param iwreq
 * @param extra
 *
 * @return
 */
static int
__wpriv_set_optie(struct net_device *dev, struct iw_request_info *info,
                    union iwreq_data *iwreq, char *extra)
{
    QDF_STATUS status = QDF_STATUS_E_INVAL;
    acfg_ie_t ie ;

    qdf_trace(QDF_DEBUG_FUNCTRACE,  "%s(): \n",__FUNCTION__);

    qdf_mem_set(ie.data, ACFG_MAX_IELEN, 0) ;

    qdf_mem_copy(ie.data, extra, iwreq->data.length);
    ie.len = iwreq->data.length;

    status = __wioctl_sw[ACFG_REQ_SET_OPTIE](netdev_to_softc(dev), (acfg_data_t *)&ie);

    return __qdf_status_to_os(status);
}


/**
 * @brief Set acparams
 *
 * @param dev
 * @param info
 * @param iwreq
 * @param extra
 *
 * @return
 */
static int
__wpriv_set_acparams(struct net_device *dev, struct iw_request_info *info,
                    union iwreq_data *iwreq, char *extra)
{
    QDF_STATUS status = QDF_STATUS_E_INVAL;
    uint32_t *ac;

    ac = (uint32_t *)extra ;

    qdf_trace(QDF_DEBUG_FUNCTRACE, "ACFG Params - %d %d %d %d\n",
                                           ac[0],ac[1],ac[2],ac[3]);

    status = __wioctl_sw[ACFG_REQ_SET_ACPARAMS](netdev_to_softc(dev),
                                           (acfg_data_t *)ac);

    return __qdf_status_to_os(status);
}


/**
 * @brief Set Filterframe
 *
 * @param dev
 * @param info
 * @param iwreq
 * @param extra
 *
 * @return
 */
static int
__wpriv_set_filterframe(struct net_device *dev, struct iw_request_info *info,
                    union iwreq_data *iwreq, char *extra)
{
    QDF_STATUS status = QDF_STATUS_E_INVAL;
    acfg_filter_t fltrframe ;

    if(!copy_from_user(&fltrframe, iwreq->data.pointer, iwreq->data.length)) {
        qdf_print("%s:%d copy from user failed", __func__, __LINE__);
        status = QDF_STATUS_E_FAULT;
        return __qdf_status_to_os(status);
    }

    qdf_trace(QDF_DEBUG_FUNCTRACE,  "\n");

    status = __wioctl_sw[ACFG_REQ_SET_FILTERFRAME](netdev_to_softc(dev), (acfg_data_t *)&fltrframe);

    return __qdf_status_to_os(status);
}

/**
 * @brief Send Mgmt frame
 *
 * @param dev
 * @param info
 * @param iwreq
 * @param extra
 *
 * @return
 */

static int
__wpriv_send_mgmt(struct net_device *dev, struct iw_request_info *info,
                    union iwreq_data *iwreq, char *extra)
{
    QDF_STATUS status = QDF_STATUS_E_INVAL;
    acfg_mgmt_t *mgmt_frame ;

    mgmt_frame = vmalloc(iwreq->data.length);

    if(!copy_from_user(mgmt_frame, iwreq->data.pointer, iwreq->data.length)) {
        qdf_print("%s:%d copy from user failed", __func__, __LINE__);
        status = QDF_STATUS_E_FAULT;
        goto err;
    }

    qdf_trace(QDF_DEBUG_FUNCTRACE,  "\n");
    status = __wioctl_sw[ACFG_REQ_SEND_MGMT](netdev_to_softc(dev), (acfg_data_t *)&mgmt_frame);

err:
    vfree(mgmt_frame);

    return __qdf_status_to_os(status);
}

/*
* @brief Send dbgreq frame
*
* @param dev
* @param info
* @param iwreq
* @param extra
*
* @return
*/

static int
__wpriv_dbgreq(struct net_device *dev, struct iw_request_info *info,
        union iwreq_data *iwreq, char *extra)
{
    QDF_STATUS status = QDF_STATUS_E_NOMEM;
    acfg_athdbg_req_t *dbgreq;
    uint32_t data_size = 0;
    void *data_addr = NULL;

    dbgreq = vmalloc(iwreq->data.length);
    if (!dbgreq) {
        return __qdf_status_to_os(status);
    }

    qdf_trace(QDF_DEBUG_FUNCTRACE,  "\n");

    if(!copy_from_user(dbgreq, iwreq->data.pointer, iwreq->data.length)) {
        qdf_print("%s:%d copy from user failed", __func__, __LINE__);
        status = QDF_STATUS_E_FAULT;
        goto err;
    }
    if (dbgreq->cmd == ACFG_DBGREQ_GETRRMSTATS ||
        dbgreq->cmd == ACFG_DBGREQ_GETBCNRPT) {
        data_size = dbgreq->data.rrmstats_req.data_size;
        data_addr = dbgreq->data.rrmstats_req.data_addr;
        dbgreq->data.rrmstats_req.data_addr = vmalloc(data_size);
        if (!dbgreq->data.rrmstats_req.data_addr) {
            vfree(dbgreq);
            return __qdf_status_to_os(status);
        }
        if(!copy_from_user(dbgreq->data.rrmstats_req.data_addr, data_addr, data_size)) {
            qdf_print("%s:%d copy from user failed", __func__, __LINE__);
            status = QDF_STATUS_E_FAULT;
            goto err;
        }
    } else if (dbgreq->cmd == ACFG_DBGREQ_GETACSREPORT) {
        data_size = dbgreq->data.acs_rep.data_size;
        data_addr = dbgreq->data.acs_rep.data_addr;
        dbgreq->data.acs_rep.data_addr = vmalloc(data_size);
        if (!dbgreq->data.acs_rep.data_addr) {
            vfree(dbgreq);
            return __qdf_status_to_os(status);
        }
        if(!copy_from_user(dbgreq->data.acs_rep.data_addr, data_addr, data_size)) {
            qdf_print("%s:%d copy from user failed", __func__, __LINE__);
            status = QDF_STATUS_E_FAULT;
            goto err;
        }
    }

    status = __wioctl_sw[ACFG_REQ_DBGREQ](netdev_to_softc(dev), (acfg_data_t *)dbgreq);

    if (dbgreq->cmd == ACFG_DBGREQ_GETRRMSTATS ||
        dbgreq->cmd == ACFG_DBGREQ_GETBCNRPT) {
        if(!copy_to_user(data_addr, dbgreq->data.rrmstats_req.data_addr, data_size)) {
            qdf_print("%s:%d copy to user failed", __func__, __LINE__);
            status = QDF_STATUS_E_FAULT;
            vfree(dbgreq->data.rrmstats_req.data_addr);
            goto err;
        }
        vfree(dbgreq->data.rrmstats_req.data_addr);
        dbgreq->data.rrmstats_req.data_addr = data_addr;
    } else if (dbgreq->cmd == ACFG_DBGREQ_GETACSREPORT) {
        if(!copy_to_user(data_addr, dbgreq->data.acs_rep.data_addr, data_size)) {
            qdf_print("%s:%d copy to user failed", __func__, __LINE__);
            status = QDF_STATUS_E_FAULT;
            vfree(dbgreq->data.acs_rep.data_addr);
            goto err;
        }
        vfree(dbgreq->data.acs_rep.data_addr);
        dbgreq->data.acs_rep.data_addr = data_addr;
    }

    if(!copy_to_user(iwreq->data.pointer, dbgreq, iwreq->data.length)) {
        qdf_print("%s:%d copy to user failed", __func__, __LINE__);
        status = QDF_STATUS_E_FAULT;
    }

err:
    vfree(dbgreq);

    return __qdf_status_to_os(status);
}

/**
 * @brief Set Appiebuffer
 *
 * @param dev
 * @param info
 * @param iwreq
 * @param extra
 *
 * @return
 */
static int
__wpriv_set_appiebuf(struct net_device *dev, struct iw_request_info *info,
                    union iwreq_data *iwreq, char *extra)
{
    QDF_STATUS status = QDF_STATUS_E_NOMEM;
    acfg_appie_t  *appiebuf = NULL;

    appiebuf = (acfg_appie_t *)vmalloc(iwreq->data.length);
    if (!appiebuf)
        return __qdf_status_to_os(status);

    if(!copy_from_user(appiebuf, iwreq->data.pointer, iwreq->data.length)) {
        qdf_print("%s:%d copy from user failed", __func__, __LINE__);
        status = QDF_STATUS_E_FAULT;
        goto err;
    }

    qdf_trace(QDF_DEBUG_FUNCTRACE,  "\n");

    status = __wioctl_sw[ACFG_REQ_SET_APPIEBUF](netdev_to_softc(dev), (acfg_data_t *)appiebuf);

err:
    vfree(appiebuf);

    return __qdf_status_to_os(status);
}


/**
 * @brief  Set Key
 *
 * @param dev
 * @param info
 * @param iwreq
 * @param extra
 *
 * @return
 */
static int
__wpriv_set_key(struct net_device *dev, struct iw_request_info *info,
                    union iwreq_data *iwreq, char *extra)
{
    QDF_STATUS status = QDF_STATUS_E_INVAL;
    acfg_key_t setkey ;

    if(!copy_from_user(&setkey, iwreq->data.pointer, iwreq->data.length)) {
        qdf_print("%s:%d copy from user failed", __func__, __LINE__);
        status = QDF_STATUS_E_FAULT;
        return __qdf_status_to_os(status);
    }

    qdf_trace(QDF_DEBUG_FUNCTRACE,  "\n");

    status = __wioctl_sw[ACFG_REQ_SET_KEY](netdev_to_softc(dev), (acfg_data_t *)&setkey);

    return __qdf_status_to_os(status);
}


/**
 * @brief Del Key
 *
 * @param dev
 * @param info
 * @param iwreq
 * @param extra
 *
 * @return
 */
static int
__wpriv_del_key(struct net_device *dev, struct iw_request_info *info,
                    union iwreq_data *iwreq, char *extra)
{
    QDF_STATUS status = QDF_STATUS_E_INVAL;

    qdf_trace(QDF_DEBUG_FUNCTRACE,  "\n");

    /* Data is inlined into iwreq->name (*extra) since acfg_delkey_t is
     * smaller than IFNAMSIZ. No copy_from_user() is needed because iwreq was
     * already copied into kernel space by dev_ioctl(). */
    status = __wioctl_sw[ACFG_REQ_DEL_KEY](netdev_to_softc(dev), (acfg_data_t *)extra);

    return __qdf_status_to_os(status);
}


/**
 * @brief Get Channel Info
 *
 * @param dev
 * @param info
 * @param iwreq
 * @param extra
 *
 * @return
 */
static int
__wpriv_get_chan_info(struct net_device *dev, struct iw_request_info *info,
                    union iwreq_data *iwreq, char *extra)
{
    QDF_STATUS status = QDF_STATUS_E_NOMEM;
    acfg_chan_info_t *chan_info;

    chan_info = vmalloc(sizeof(acfg_chan_info_t));
    if (!chan_info)
        return __qdf_status_to_os(status);

    qdf_trace(QDF_DEBUG_FUNCTRACE,  "\n");

    status = __wioctl_sw[ACFG_REQ_GET_CHAN_INFO](netdev_to_softc(dev), (acfg_data_t *)chan_info);

    if (status == QDF_STATUS_SUCCESS) {
        uint32_t len;
        len = (chan_info->num_channels * sizeof(acfg_channel_t)) +
            sizeof(unsigned int);

        if(len <= (QDF_CHANINFO_MAX * sizeof(unsigned int)))
            qdf_mem_copy(extra, chan_info, len);
        else
            printk("%s : Not enough memory\n",__FUNCTION__);
    }

    vfree(chan_info);
    return __qdf_status_to_os(status);
}


/**
 * @brief Get ACL MAC Address List
 *
 * @param dev
 * @param info
 * @param iwreq
 * @param extra
 *
 * @return
 */
static int
__wpriv_get_mac_address(struct net_device *dev, struct iw_request_info *info,
                    union iwreq_data *iwreq, char *extra)
{
    QDF_STATUS status = QDF_STATUS_E_NOMEM;
    acfg_opaque_t mac_addr_list;

    mac_addr_list.pointer = vmalloc(iwreq->data.length);
    mac_addr_list.len     = iwreq->data.length;
    if (!mac_addr_list.pointer)
        return __qdf_status_to_os(status);

    qdf_trace(QDF_DEBUG_FUNCTRACE,  "\n");

    status = __wioctl_sw[ACFG_REQ_GET_MAC_ADDR](netdev_to_softc(dev), (acfg_data_t *)&mac_addr_list);

    if (status == QDF_STATUS_SUCCESS) {
        qdf_mem_copy(extra, mac_addr_list.pointer, iwreq->data.length);
        iwreq->data.length = mac_addr_list.len;
    }

    vfree(mac_addr_list.pointer);
    return __qdf_status_to_os(status);
}


/**
 * @brief Get Channel List
 *
 * @param dev
 * @param info
 * @param iwreq
 * @param extra
 *
 * @return
 */
static int
__wpriv_get_chan_list(struct net_device *dev, struct iw_request_info *info,
                    union iwreq_data *iwreq, char *extra)
{
    QDF_STATUS status = QDF_STATUS_E_NOMEM;
    acfg_opaque_t chan_list;

    chan_list.pointer = vmalloc(iwreq->data.length);
    chan_list.len     = iwreq->data.length;
    if (!chan_list.pointer)
        return __qdf_status_to_os(status);

    qdf_trace(QDF_DEBUG_FUNCTRACE,  "\n");

    status = __wioctl_sw[ACFG_REQ_GET_CHAN_LIST](netdev_to_softc(dev), (acfg_data_t *)&chan_list);

    if (status == QDF_STATUS_SUCCESS)
        qdf_mem_copy(extra, chan_list.pointer, iwreq->data.length);

    vfree(chan_list.pointer);
    return __qdf_status_to_os(status);
}

/**
 * @brief Get Channel Mode
 *
 * @param dev
 * @param info
 * @param iwreq
 * @param extra
 *
 * @return
 */
static int
__wpriv_get_chmode(struct net_device *dev, struct iw_request_info *info,
                    union iwreq_data *iwreq, char *extra)
{
    QDF_STATUS status = QDF_STATUS_E_NOMEM;

    acfg_chmode_t mode;

    memset(&mode, 0, sizeof(acfg_chmode_t));

    status = __wioctl_sw[ACFG_REQ_GET_CHMODE](netdev_to_softc(dev), (acfg_data_t *)&mode);

    if (status == QDF_STATUS_SUCCESS) {
        qdf_mem_copy(extra, mode.mode, mode.len);
        iwreq->data.length = mode.len;
    }

    return __qdf_status_to_os(status);
}


static int
__wpriv_set_chn_widthswitch(struct net_device *dev, struct iw_request_info *info,
                union iwreq_data *iwreq, char *extra)
{
    QDF_STATUS status = QDF_STATUS_E_NOMEM;
    acfg_data_t *data = NULL;

    qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): [0]:%d, [1]:%d, [2]:%d",
                                       __FUNCTION__, *((uint32_t *)extra) ,
                                       *((uint32_t *)extra + 1), *((uint32_t *)extra + 2) );
    data = vmalloc(sizeof(acfg_set_chn_width_t));
    if (!data)
        return __qdf_status_to_os(status);

    memcpy(data->chnw.setchnwidth, (uint32_t *)extra, 3*sizeof(uint32_t));

    status = __wioctl_sw[ACFG_REQ_SET_CHNWIDTHSWITCH](netdev_to_softc(dev), data);
    vfree(data);

    return __qdf_status_to_os(status);
}

static int
__wpriv_set_country(struct net_device *dev, struct iw_request_info *info,
                union iwreq_data *iwreq, char *extra)
{
    QDF_STATUS status = QDF_STATUS_E_NOMEM;
    acfg_data_t *data = NULL;

    data = vmalloc(sizeof(acfg_set_country_t));
    if (!data)
        return __qdf_status_to_os(status);

    qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): parameter: %s", __FUNCTION__, extra);

    memcpy(data->setcntry.setcountry,(uint8_t *)extra, 3) ;

    status = __wioctl_sw[ACFG_REQ_SET_COUNTRY](netdev_to_softc(dev), data);
    vfree(data);

    return __qdf_status_to_os(status);
}
