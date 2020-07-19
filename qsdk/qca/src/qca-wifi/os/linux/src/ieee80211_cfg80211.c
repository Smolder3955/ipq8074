/*
 * Copyright (c) 2016-2019 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#if UMAC_SUPPORT_CFG80211
#include <ieee80211_cfg80211.h>
#include <osif_private.h>
#include <ieee80211_defines.h>
#include <ieee80211_var.h>
#include <if_athvar.h>
#include <i_qdf_mem.h>
#include <if_athioctl.h>
#include <ol_if_athvar.h>
#include <ol_ath_ucfg.h>
#include "cdp_txrx_cmn.h"
#include "cdp_txrx_cmn_reg.h"
#include "cdp_txrx_ctrl.h"
#include <ieee80211_ucfg.h>
#include "ieee80211_crypto_nlshim_api.h"
#include <ext_ioctl_drv_if.h>
#include <ol_if_thermal.h>
#include <qdf_types.h>
#include <ieee80211_var.h>
#include <wlan_tgt_def_config.h>
#include "qwrap_structure.h"
#if QCA_AIRTIME_FAIRNESS
#include <wlan_atf_ucfg_api.h>
#endif
#include <wlan_osif_priv.h>
#include <wlan_cfg80211_scan.h>
#include <wlan_cfg80211_spectral.h>
#include <wlan_dfs_ucfg_api.h>
#include <wlan_reg_services_api.h>
#include "ieee80211_mlme_priv.h"
#ifdef WLAN_SUPPORT_FILS
#include <wlan_fd_utils_api.h>
#endif /* WLAN_SUPPORT_FILS */
#include "qal_devcfg.h"
#include <dp_rate_stats_pub.h>

#if ATH_ACS_DEBUG_SUPPORT
#include <acs_debug.h>
#endif
#include <ieee80211_ioctl_acfg.h>

#define MAX_BUFFER_LEN 1180
#define RADIO_CMD 1
#define VAP_CMD 0
#define REPLY_SKB_SIZE ((2*sizeof(u_int32_t)) + NLMSG_HDRLEN)
#define LIST_ALLOC_SIZE 3*1024
#define MAX_CFG80211_BUF_LEN 4000
#define MAX_NSS_INTERFACE_NUM 256

#define print_eacs_chanlist(list, len)                    \
    do {                                                  \
    int j;                                                \
    qdf_print("channel count:%d \nchannel list: ",(len)); \
    for (j = 0;j < (len);j++) {                           \
        qdf_print("%d  ", (list)[j]);                     \
    }                                                     \
    qdf_print("\n");                                      \
} while(0)

struct ieee80211_regdomain wlan_cfg80211_world_regdom_60_61_62 = {
    .n_reg_rules = 7,
    .alpha2 =  "00",
    .reg_rules = {
        REG_RULE_2412_2462,
        REG_RULE_2467_2472,
        REG_RULE_2484,
        REG_RULE_5180_5320,
        REG_RULE_5500_5640,
        REG_RULE_5660_5720,
        REG_RULE_5745_5925,
    }
};

/* enums to parse netlink messages sent by FTM daemon */
enum wlan_tm_attr {
    WLAN_TM_ATTR_INVALID = 0,
    WLAN_TM_ATTR_CMD = 1,
    WLAN_TM_ATTR_DATA = 2,
    /* keep last */
    WLAN_TM_ATTR_MAX,
};

enum wlan_tm_cmd {
    WLAN_TM_CMD_WLAN_FTM = 0,
    WLAN_TM_CMD_WLAN_HB = 1,
};

#define WLAN_TM_DATA_MAX_LEN    5000

static const struct nla_policy wlan_tm_policy[WLAN_TM_ATTR_MAX] = {
    [WLAN_TM_ATTR_CMD] = {.type = NLA_U32},
    [WLAN_TM_ATTR_DATA] = {.type = NLA_BINARY,
                           .len = WLAN_TM_DATA_MAX_LEN},
};

/* funtion prototype */
static void clear_roam_params(struct net_device *dev);

/**
 *ieee80211_cfg80211_schedule_channel_notify: WorkQ for cfg80211_ch_switch_notify
 * Return: None
 */
void ieee80211_cfg80211_schedule_channel_notify(void *context)
{
    wlan_if_t vap = (wlan_if_t) context;
    osif_dev *osifp = (osif_dev *)vap->iv_ifp;
    struct net_device *dev = osifp->netdev;

    if (!(dev->flags & IFF_UP)) {
        return;
    }
    ieee80211_cfg80211_dump_chandef(&vap->chandef_notify);
    cfg80211_ch_switch_notify(dev, &vap->chandef_notify);
    return;
}


/**
 * extract_command : extract if it is radio or vap command
 * @ic; pointer to ic
 * @wdev :pointer to wdev
 * @cmd_type : command type
 * return pointer to data
 */
void * extract_command(struct ieee80211com *ic, struct wireless_dev *wdev, int *cmd_type) {

    struct net_device *dev = NULL;
    osif_dev *osifp = NULL;
    void *data = NULL;

    if (ic->ic_wdev.netdev == wdev->netdev) {
        dev = wdev->netdev;
        data = (void *) ath_netdev_priv(dev);
        *cmd_type = RADIO_CMD;
    } else {
        dev = wdev->netdev;
        osifp = ath_netdev_priv(dev);
        data = (void *)osifp->os_if;
        *cmd_type = VAP_CMD;
    }

    return data;
}

/**
 * cfg80211_reply_command : reply skb to the user space
 * @wiphy: pointer to wiphy object
 * @length : data length
 * @data : point to data
 * @flag : flag value
 * return 0 on success and -1 on failure
 */
int
cfg80211_reply_command(struct wiphy *wiphy, int length, void *data, u_int32_t flag)
{
    struct sk_buff *reply_skb = NULL;
    QDF_STATUS status;

    reply_skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy,
                                                    length + REPLY_SKB_SIZE);
    if (reply_skb) {
        if ((nla_put(reply_skb, QCA_WLAN_VENDOR_ATTR_PARAM_DATA, length, data)) ||
                (nla_put_u32(reply_skb, QCA_WLAN_VENDOR_ATTR_PARAM_LENGTH, length))
                || (nla_put_u32(reply_skb, QCA_WLAN_VENDOR_ATTR_PARAM_FLAGS, flag))){
            kfree_skb(reply_skb);
            return -EINVAL;
        }
        status = qal_devcfg_send_response((qdf_nbuf_t)reply_skb);
        return qdf_status_to_os_return(status);
    } else {
        return -ENOMEM;
    }
    return -EIO;
}

/**
 * wlan_cfg80211_getopmode : Given the nl opmode type , the function return the corresponding
 * ieee80211 opmode type.
 * @type: nl80211_iftype
 * @flags: point to flag
 *
 * returns ieee80211 mode  and return 0 on invalid input.
 */
int
wlan_cfg80211_getopmode(enum nl80211_iftype type, u_int32_t *flags)
{
    if (type == NL80211_IFTYPE_UNSPECIFIED) {
        return IEEE80211_M_HOSTAP;
    }
    if (type == NL80211_IFTYPE_ADHOC) {
        return IEEE80211_M_IBSS;
    }
    if (type == NL80211_IFTYPE_MONITOR) {
        return IEEE80211_M_MONITOR;
    }
    if (type == NL80211_IFTYPE_AP) {
        return IEEE80211_M_HOSTAP;
    }
    if(type == NL80211_IFTYPE_STATION) {
        return IEEE80211_M_STA;
    }
    if(type == NL80211_IFTYPE_WDS) {
        return IEEE80211_M_HOSTAP;
    }
    if (type == NL80211_IFTYPE_P2P_GO) {
        return IEEE80211_M_P2P_GO;
    }
    if (type == NL80211_IFTYPE_P2P_CLIENT) {
        return IEEE80211_M_P2P_CLIENT;
    }
    if (type == NL80211_IFTYPE_P2P_DEVICE) {
        return IEEE80211_M_P2P_DEVICE;
    }
#if MESH_MODE_SUPPORT
    if (type == NL80211_IFTYPE_MESH_POINT) {
        *flags |= IEEE80211_MESH_VAP;
        return IEEE80211_M_HOSTAP;
    }
#endif
    return IEEE80211_M_ANY;
}

/**
 *   wlan_cfg80211_add_virtual_intf: This function create a virtual interface.
 *   @wiphy: pointer to wiphy structure.
 *   @name: name of the interface
 *   @type: nl80211_type
 *   @flags: pointer to flags
 *   @vif_params: pointer to vif_params that contains macaddress
 *
 *   returns wirelessdev.
 */

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0))
struct wireless_dev *wlan_cfg80211_add_virtual_intf(struct wiphy *wiphy,
        const char *name,
        unsigned char name_assign_type,
        enum nl80211_iftype type,
        u_int32_t *flags,
        struct vif_params *params)
{
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 7, 0))
struct wireless_dev *wlan_cfg80211_add_virtual_intf(struct wiphy *wiphy,
        const char *name,
        enum nl80211_iftype type,
        u_int32_t *flags,
        struct vif_params *params)
{
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0))
struct wireless_dev *wlan_cfg80211_add_virtual_intf(struct wiphy *wiphy, char *name,
        enum nl80211_iftype type,
        u_int32_t *flags,
        struct vif_params *params)
{
#else
struct net_device *wlan_cfg80211_add_virtual_intf(struct wiphy *wiphy, char *name,
        enum nl80211_iftype type,
        u_int32_t *flags,
        struct vif_params *params)
{
#endif
    struct cfg80211_context *cfg_ctx = NULL;
    struct net_device *ndev= NULL;
    struct ieee80211_clone_params cp;
    struct ieee80211com *ic;
    osif_dev *osifp;
    wlan_if_t vap;
    int retv;
    int found_cp_entry = 0;
    struct ieee80211_clone_params_list *cp_entry, *next_entry;
    struct ol_ath_softc_net80211 *scn = NULL;

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;
    scn = (struct ol_ath_softc_net80211 *) ic;

    if (scn->soc->sc_in_delete) {
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_CFG80211, "Delete in Progress, Vaps can't be created !");
        return ERR_PTR(-EBUSY);
    }


    /* Adding support for creating vaps in proprietary mode
       This is needed to create vaps in modes that are not
       supported by cfg80211
    */
    LIST_FOREACH_SAFE(cp_entry, &ic->ic_cp, link_entry, next_entry)
    {
        if(!(strcmp(name, cp_entry->cp.icp_name)))
        {
            found_cp_entry = 1;
            qdf_mem_copy(&cp, &cp_entry->cp, sizeof(struct ieee80211_clone_params));
            LIST_REMOVE(cp_entry,link_entry);
            qdf_mem_free(cp_entry);
            qdf_info("proprietary mode %d for interface: %s : clone params: 0x%08X ",
                   cp.icp_opmode, name, cp.icp_flags);
        }
    }

    if(found_cp_entry == 0)
    {
        /*
         * Device Name
         */
        strlcpy(cp.icp_name, name, IFNAMSIZ - 1);
        /*
         * NL80211_IFTYPE_UNSPECIFIED,
         * NL80211_IFTYPE_ADHOC,
         * NL80211_IFTYPE_STATION,
         * NL80211_IFTYPE_AP,
         * NL80211_IFTYPE_AP_VLAN,
         * NL80211_IFTYPE_WDS,
         * NL80211_IFTYPE_MONITOR,
         * NL80211_IFTYPE_MESH_POINT,
         * NL80211_IFTYPE_P2P_CLIENT,
         * NL80211_IFTYPE_P2P_GO,
         * NL80211_IFTYPE_P2P_DEVICE,
         * NL80211_IFTYPE_OCB,
         * NUM_NL80211_IFTYPES,
         * NL80211_IFTYPE_MAX = NUM_NL80211_IFTYPES - 1
         * For other mode like NAWDS need to follow wlanconfig to populate cp flags.
         */

        cp.icp_opmode = (u_int16_t)wlan_cfg80211_getopmode(type, flags);
        if(cp.icp_opmode == IEEE80211_M_ANY) {
            qdf_print("%s: Unsupported wireless mode : %d ", __func__, type) ;
            return ERR_PTR(-EINVAL);
        }

        /*
         * Based on opmode we have differnt icp_flags
         * using wlanconfig we can populate flags
         * for cfg80211 if hostpad gives mac addres in vif_params
         * we need to clear IEEE80211_CLONE_BSSID and populate mac
         * address in ieee80211_clone_params
         *
         */
        /* TODO: do experments with hostpad bssid options and confim */

        cp.icp_flags = IEEE80211_CLONE_BSSID;
    }

#define CFG80211_VAP_CREATE 1
    /* Third parameter represents if the VAP is created using cfg80211 (1) or WEXT (0) */
    ndev = osif_create_vap(ic, ic->ic_wdev.netdev, &cp, CFG80211_VAP_CREATE);

    if (ndev == NULL) {
        qdf_print("Failed to create VAP. osif_create_vap returned NULL!");
        return ERR_PTR(-EINVAL);
    }
    else {
        /* Adding support for WDS*/
        if(params->use_4addr) {
            osifp = ath_netdev_priv(ndev);
            vap = osifp->os_if;
            retv = wlan_set_param(vap, IEEE80211_FEATURE_WDS, params->use_4addr);
            if (retv == 0) {
                /* WAR: set the auto assoc feature also for WDS */
                if (params->use_4addr) {
                    wlan_set_param(vap, IEEE80211_AUTO_ASSOC, 1);
                    /* disable STA powersave for WDS */
                    if (wlan_vap_get_opmode(vap) == IEEE80211_M_STA) {
                        (void) wlan_set_powersave(vap,IEEE80211_PWRSAVE_NONE);
                        (void) wlan_pwrsave_force_sleep(vap,0);
                    }
                }
            }
        }

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0))
        return ndev->ieee80211_ptr;
#else
        return ndev;
#endif
    }
}

/**
*  wlan_cfg80211_probe_client: Probe client
*  @wiphy: pointer to wiphy structure
*  @dev: pointer to dev
*  @peer: pointer to peer
*  @cookie : cookie information
*/

int wlan_cfg80211_probe_client(struct wiphy *wiphy, struct net_device *dev,const u8 *peer, u64 *cookie)
{
    return 0;
}

/**
 *   wlan_cfg80211_change_virtual_intf: This function changes a virtual interface.
 *   @wiphy: pointer to wiphy structure.
 *   @name: name of the interface
 *   @type: nl80211_type
 *   @flags: pointer to flags
 *   @vif_params: pointer to vif_params that contains macaddress
 *
 *   returns 1/0 and -1 on invalid input
 */

static int wlan_cfg80211_change_virtual_intf(struct wiphy *wiphy, struct net_device *ndev,
        enum nl80211_iftype type,
        u32 *flags,
        struct vif_params *params)
{


    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic;

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    if (ndev->ieee80211_ptr->iftype != type) {
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_CFG80211,"%s: type : %d Rejecting ...\n", __func__, type);
        return -1;
    }
    /* resolve vAP from ndev*/
    ndev->ieee80211_ptr->iftype = type;
    return 0;
}


/**
 *  wlan_cfg80211_add_station: Set Station parameters
 *  @wiphy: pointer to wiphy structure
 *  @dev: pointer to dev
 *  @mac: Pointer to mac
 *  @station_parameters: Pointer to station parameters
 */

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0))
static int wlan_cfg80211_add_station(struct wiphy *wiphy,
                                         struct net_device *dev,
                                         const uint8_t *mac,
                                         struct station_parameters *params)
#else
static int wlan_cfg80211_add_station(struct wiphy *wiphy,
                                         struct net_device *dev, uint8_t *mac,
                                         struct station_parameters *params)
#endif
{
     return 0;
}

/**
 *  wlan_cfg80211_del_station: Del client
 *  @wiphy: pointer to wiphy structure
 *  @dev: pointer to dev
 *  @mac: Pointer to mac
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0))
int wlan_cfg80211_del_station(struct wiphy *wiphy,
                                  struct net_device *dev,
                                  struct station_del_parameters *params)
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0))
int wlan_cfg80211_del_station(struct wiphy *wiphy,
                                  struct net_device *dev,
                                  const uint8_t *mac)
#else
int wlan_cfg80211_del_station(struct wiphy *wiphy,
                                  struct net_device *dev,
                                  uint8_t *mac)
#endif
{
    return 0 ;
}

/**
 * wlan_cfg80211_set_rekey_dataata() - set rekey data
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to network device
 * @data: Pointer to rekey data
 *
 * This function is used to offload GTK rekeying job to the firmware.
 *
 * Return: 0 for success, non-zero for failure
 */
int wlan_cfg80211_set_rekey_dataata(struct wiphy *wiphy,
        struct net_device *dev,
        struct cfg80211_gtk_rekey_data *data)
{
    return 0;
}

/**
 * wlan_cfg80211_sched_scan_start() - cfg80211 scheduled scan(pno) start
 * @wiphy: Pointer to wiphy
 * @dev: Pointer network device
 * @request: Pointer to cfg80211 scheduled scan start request
 *
 * Return: 0 for success, non zero for failure
 */
int wlan_cfg80211_sched_scan_start(struct wiphy *wiphy,
        struct net_device *dev,
        struct cfg80211_sched_scan_request
        *request)
{
    return 0;
}

/**
 * wlan_cfg80211_sched_scan_stop() - stop cfg80211 scheduled scan(pno)
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to network device
 *
 * Return: 0 for success, non zero for failure
 */
int wlan_cfg80211_sched_scan_stop(struct wiphy *wiphy,
        struct net_device *dev)
{
    return 0;
}

/**
 * wlan_cfg80211_set_ap_chanwidth- call back for AP channel width change
 * This is called when we need to change channel width (dynamic HT2040)
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to network device
 * @chandef: pointer to cfg80211_chan_def
 *
 * Return: 0 for success, non zero for failure
 */
int wlan_cfg80211_set_ap_chanwidth(struct wiphy *wiphy, struct net_device *dev,
        struct cfg80211_chan_def *chandef)
{

    /* As of now we no need todo any thing */
    qdf_print("%s: freq:%d hw_value:%d ch_width:%d ",
            __func__, chandef->chan->center_freq, chandef->chan->hw_value, chandef->width);

    return 0;
}

/**
 * ieee80211_cfg80211_dump_chandef:  dump channel def
 * @chandef: pointer to cfg80211_chan_def
 *
 * Return: 0 for success, non zero for failure
 */
int ieee80211_cfg80211_dump_chandef(struct cfg80211_chan_def *chan_def)
{
    QDF_TRACE(QDF_MODULE_ID_DEBUG, QDF_TRACE_LEVEL_DEBUG,
              "band:%d cfreq:%d channel:%d flags:0x%x",
              chan_def->chan->band, chan_def->chan->center_freq,
              chan_def->chan->hw_value, chan_def->chan->flags);
    QDF_TRACE(QDF_MODULE_ID_DEBUG, QDF_TRACE_LEVEL_DEBUG,
              "antenna_gain:%d max_power:%d max_reg_power:%d date_state:%d cac_ms:%d",
              chan_def->chan->max_antenna_gain, chan_def->chan->max_power,
              chan_def->chan->max_reg_power, chan_def->chan->dfs_state,
              chan_def->chan->dfs_cac_ms);
    QDF_TRACE(QDF_MODULE_ID_DEBUG, QDF_TRACE_LEVEL_DEBUG,
              "width:%d cfreq1:%d cfreq2:%d", chan_def->width,
              chan_def->center_freq1, chan_def->center_freq2);

    return 0;
}

/**
 *  ieee80211_get_nl80211_chwidth: get nl80211 channel width for a given internal width
 *  and phymode.
 *  @ch_width: internal channel width.
 *  @phymode: wireless mode / phymode
 *  return nl80211_chan_width.
 */
enum nl80211_chan_width ieee80211_get_nl80211_chwidth(enum ieee80211_cwm_width ch_width ,
        enum ieee80211_phymode phymode)
{
    enum nl80211_chan_width nl_ch_width = NL80211_CHAN_WIDTH_20_NOHT;

    switch (phymode) {
        case IEEE80211_MODE_11B:
        case IEEE80211_MODE_11G:
        case IEEE80211_MODE_11A:
        case IEEE80211_MODE_FH:
        case IEEE80211_MODE_TURBO_G:
        case IEEE80211_MODE_TURBO_A:
            return NL80211_CHAN_WIDTH_20_NOHT;
            break;
        default:
            break;
    }

    switch (ch_width) {
        case IEEE80211_CWM_WIDTH20:
            nl_ch_width = NL80211_CHAN_WIDTH_20;
            break;
        case IEEE80211_CWM_WIDTH40:
            nl_ch_width = NL80211_CHAN_WIDTH_40;
            break;
        case IEEE80211_CWM_WIDTH80:
            nl_ch_width = NL80211_CHAN_WIDTH_80;
            break;
        case IEEE80211_CWM_WIDTH160:
            nl_ch_width = NL80211_CHAN_WIDTH_160;
            break;
        case IEEE80211_CWM_WIDTH80_80:
            nl_ch_width = NL80211_CHAN_WIDTH_80P80;
            break;
        default:
            nl_ch_width = NL80211_CHAN_WIDTH_20_NOHT;
            break;
    }
    return nl_ch_width;
}

/**
 *  ieee80211_get_ath_chwidth: get internal channel width from nl80211 channel width.
 *  @nl_ch_width: nl80211 channel width.
 *  return ch_width. (Channel width used in Wide Bandwidth Channel Switch Element)
 */
int ieee80211_get_ath_chwidth(enum nl80211_chan_width nl_ch_width)
{
    int ch_width = CHWIDTH_VHT20;

    switch (nl_ch_width) {
        case NL80211_CHAN_WIDTH_20_NOHT:
            ch_width = CHWIDTH_VHT20;
            break;
        case NL80211_CHAN_WIDTH_20:
            ch_width = CHWIDTH_VHT20;
            break;
        case NL80211_CHAN_WIDTH_40:
            ch_width = CHWIDTH_VHT40;
            break;
        case NL80211_CHAN_WIDTH_80:
            ch_width = CHWIDTH_VHT80;
            break;
        case NL80211_CHAN_WIDTH_160:
            ch_width = CHWIDTH_VHT160;
            break;
        case NL80211_CHAN_WIDTH_80P80:
            ch_width = CHWIDTH_VHT80;
            break;
        default:
            ch_width = CHWIDTH_VHT20;
            break;
    }
    return ch_width;
}

/**
 *  ieee80211_cfg80211_construct_chandef: construct channel def for a
 *              given VAP based on current phymode and channel width.
 *  @ic: pointer to ic
 *  @chan_def: pointer to channel def that we need to construct.
 *  @nl_chan: Pointer to ieee80211_channel(nl channel) for which we need to
 *            construct channel def.
 *  return 0/-1 on success/fail
 */
int ieee80211_cfg80211_construct_chandef(struct ieee80211com *ic, struct cfg80211_chan_def *chan_def,
        struct ieee80211_channel *nl_chan)
{
    struct ieee80211_ath_channel *channel = ic->ic_curchan;
    enum ieee80211_phymode  phymode;
    enum ieee80211_cwm_width ch_width;
    u_int8_t vht_seg1 = 0, vht_seg0 = 0;

    if (channel == NULL) {
        qdf_print("%s: channel NULL ", __func__);
        return -1;
    }

    /* Create Channel def with NL80211_CHAN_NO_HT, later update
     width, cfreq1 and cfreq2 */
    cfg80211_chandef_create(chan_def, nl_chan, NL80211_CHAN_NO_HT);

    phymode = wlan_channel_phymode(channel);
    ch_width = get_chwidth_phymode(phymode);
    chan_def->width = ieee80211_get_nl80211_chwidth(ch_width, phymode);

    if (is_phymode_5G(phymode)) {
        /*
         * Note:
         * Driver adheres to 802.11-2016 wherein seg2 should contain center of entire 160 MHz span
         * and seg1 contains center of primary 80 MHz.
         * However hostapd currently adheres to the older 802.11ac-2013 wherein seg1 contains center
         * of entire 160MHz span and seg2 contains zero.
         */
        switch (phymode) {
            case IEEE80211_MODE_11AC_VHT160:
            case IEEE80211_MODE_11AXA_HE160:
                vht_seg0 = channel->ic_vhtop_ch_freq_seg2;
                vht_seg1 = 0;
                break;
            case IEEE80211_MODE_11AC_VHT80_80:
            case IEEE80211_MODE_11AXA_HE80_80:
                vht_seg0 = channel->ic_vhtop_ch_freq_seg1;
                vht_seg1 = channel->ic_vhtop_ch_freq_seg2;
                break;
            default:
                vht_seg0 = channel->ic_vhtop_ch_freq_seg1;
                vht_seg1 = 0;
                break;
        }

        if (vht_seg0) {
            chan_def->center_freq1 =
                ieee80211_ieee2mhz(ic, vht_seg0, channel->ic_flags);
        } else {
            chan_def->center_freq1 = 0;
        }

        if (vht_seg1) {
            chan_def->center_freq2 =
                ieee80211_ieee2mhz(ic, vht_seg1, channel->ic_flags);
        } else {
            chan_def->center_freq2 = 0;
        }
    } else {
        /* compute center_freq1 & center_freq1 for 2G
           as these are not populated by driver */
        chan_def->center_freq2 = 0;
        switch(phymode)  {
            case IEEE80211_MODE_11B:
            case IEEE80211_MODE_11G:
            case IEEE80211_MODE_TURBO_G:
            case IEEE80211_MODE_FH:
            case IEEE80211_MODE_11NG_HT20:
            case IEEE80211_MODE_11AXG_HE20:
                chan_def->center_freq1 = nl_chan->center_freq;
                break;
            case IEEE80211_MODE_11NG_HT40PLUS:
            case IEEE80211_MODE_11AXG_HE40PLUS:
            case IEEE80211_MODE_11AXG_HE40:
            case IEEE80211_MODE_11NG_HT40:
                chan_def->center_freq1 = (nl_chan->center_freq+10);
                break;
            case IEEE80211_MODE_11NG_HT40MINUS:
            case IEEE80211_MODE_11AXG_HE40MINUS:
                chan_def->center_freq1 = (nl_chan->center_freq-10);
                break;
            default:
                chan_def->center_freq1 = nl_chan->center_freq;
                break;
        }
    }

    return 0;
}

/**
 * wlan_cfg80211_channel_switch - do channel switch
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to network device
 * @csa_params - pointer to CSA params
 * Return: 0 for success, non zero for failure
 */
static int wlan_cfg80211_channel_switch(struct wiphy *wiphy,
        struct net_device *dev,
        struct cfg80211_csa_settings *csa_params)
{
    osif_dev *osnetdev = ath_netdev_priv(dev);
    wlan_if_t vap = osnetdev->os_if;
    int channel = 0;
    int tbtt = 0;
    enum ieee80211_cwm_width ch_width;
    struct ieee80211_ath_channel *ath_channel = vap->iv_ic->ic_curchan;

    channel = (u_int8_t)wlan_mhz2ieee(osnetdev->os_devhandle,
            csa_params->chandef.chan->center_freq, 0);

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_CFG80211, "%s: freq: %d tbtt: %d channel: %d ch_width: %d \n",
            __func__, csa_params->chandef.chan->center_freq, csa_params->count,
            channel, csa_params->chandef.width);

    ieee80211_cfg80211_dump_chandef(&csa_params->chandef);
    /*
     * If current channel is same as existing channel retrun
     */

    if (channel == ath_channel->ic_ieee) {
        qdf_print("%s: Channel:%d Configured already. ", __func__, channel);
        return 0;
    }

    ch_width = ieee80211_get_ath_chwidth(csa_params->chandef.width);
    tbtt = csa_params->count;
    if (tbtt > 0) {
        ieee80211_ucfg_set_chanswitch(vap, channel, tbtt, ch_width);
    } else {
        /* if tbtt is ZERO then do channel change immediately */
        ieee80211_ucfg_set_freq(vap, channel);
    }
    return 0;
}

/**
 *  wlan_cfg80211_change_station: Set Station parameters
 *  @wiphy: pointer to wiphy structure
 *  @dev: pointer to dev
 *  @mac: Pointer to mac
 *  @station_parameters: Pointer to station parameters
 *  return 0/-1 on success/fail
 */

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0)) || defined(WITH_BACKPORTS)
static int wlan_cfg80211_change_station(struct wiphy *wiphy,
        struct net_device *dev,
        const u8 *mac,
        struct station_parameters *params)
#else
static int wlan_cfg80211_change_station(struct wiphy *wiphy,
        struct net_device *dev,
        u8 *mac,
        struct station_parameters *params)
#endif
{
    struct ieee80211req_mlme mlme;
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;

    if( params->sta_flags_set & BIT(NL80211_STA_FLAG_AUTHORIZED)) {
        mlme.im_op = IEEE80211_MLME_AUTHORIZE;
    } else {
        mlme.im_op = IEEE80211_MLME_UNAUTHORIZE;
    }

    mlme.im_reason = 0;
    memcpy(mlme.im_macaddr, mac, IEEE80211_ADDR_LEN);

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_AUTH, "%s: %s macaddr :%s \n",
        __func__, ether_sprintf(mlme.im_macaddr),(mlme.im_op == IEEE80211_MLME_AUTHORIZE)? "authorize" : "unauthorize");
    if (mlme.im_op == IEEE80211_MLME_AUTHORIZE) {
        wlan_node_authorize(vap, 1, mlme.im_macaddr);
    } else {
        wlan_node_authorize(vap, 0, mlme.im_macaddr);
    }
    return 0;
}

/**
 *   wlan_cfg80211_del_virtual_intf: This function deletes a virtual interface.
 *   @wiphy: pointer to wiphy structure.
 *   @wdev: pointer to wireless_dev
 *
 *   returns 0/-1 on success/fail
 */

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0))
int wlan_cfg80211_del_virtual_intf(struct wiphy *wiphy, struct wireless_dev *wdev)
#else
int wlan_cfg80211_del_virtual_intf(struct wiphy *wiphy, struct net_device *dev)
#endif
{
    int retval = 0;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0))
    struct net_device *dev = wdev->netdev;
#endif
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic;
    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    /*
     * can not delete wifiX interface
     * when recovery_in_progress is set VAP destroy is already done.
     */
    if ((ic->ic_wdev.netdev == dev) || ic->recovery_in_progress) {
        return -1;
    }

    retval = osif_ioctl_delete_vap(dev);
    return retval;
}

/**
 * wlan_cfg80211_start_ap - start AP vap
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to netdev
 * @params: Pointer to start ap configuration parameters
 * struct cfg80211_ap_settings - AP configuration
 * Return: zero for success non-zero for failure
 */


int wlan_cfg80211_start_ap(struct wiphy *wiphy,
        struct net_device *dev,
        struct cfg80211_ap_settings *params)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    int retval = 0, retv;
    int mode = 0;
    enum ieee80211_opmode opmode;
    ieee80211_ssid   tmpssid;
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    int ic_flags = 0;

    struct ol_ath_softc_net80211 *scn = NULL;
    ol_ath_soc_softc_t *soc = NULL;

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    scn = OL_ATH_SOFTC_NET80211(ic);
    soc = scn->soc;

    if ((osifp->is_delete_in_progress) || (osifp->is_deleted) || !(soc->soc_attached)) {
        return -EINVAL;
    }

    opmode = wlan_vap_get_opmode(vap);

    if (opmode == IEEE80211_M_WDS) {
        return -EOPNOTSUPP;
    }

    OS_MEMZERO(&tmpssid, sizeof(ieee80211_ssid));

    ic_flags = ic->ic_flags;

    /*
     * wlan_set_desired_ssidlist
     */

    if (params->ssid != NULL) {

        if (params->ssid_len > IEEE80211_NWID_LEN) {
            params->ssid_len = IEEE80211_NWID_LEN;
        }

        tmpssid.len = params->ssid_len;
        qdf_mem_copy(tmpssid.ssid, params->ssid, params->ssid_len);
        tmpssid.ssid[tmpssid.len] = '\0';
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_CFG80211," \n DES SSID SET=%s length: %d\n", tmpssid.ssid, tmpssid.len);
        ieee80211_ucfg_set_essid(vap, &tmpssid, 0);
    } else {
        /* ANY */
        tmpssid.ssid[0] = '\0';
        tmpssid.len = 0;
    }

    /*
     * cfg80211_vap_set_phymode - @chandef
     *
     */
    mode = wlan_cfg80211_set_phymode(dev, params->chandef );
    if(mode < 0) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_CFG80211, "%s : Invalid phymode\n",
                __func__);
        return -EOPNOTSUPP;
    } else {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_CFG80211, "%s : Phymode set to %d \n",
                __func__, wlan_get_desired_phymode(vap));
    }
    /*
     * TODO: iv_des_cfreq2 -  vendorcmd application need to set this
     */

    /*
     * Setting dtim period
     */
    if ((osifp->os_opmode == IEEE80211_M_HOSTAP ||
                osifp->os_opmode == IEEE80211_M_IBSS) &&
            (params->dtim_period > IEEE80211_DTIM_MAX ||
             params->dtim_period < IEEE80211_DTIM_MIN)) {

        IEEE80211_DPRINTF(vap, IEEE80211_MSG_CFG80211,
                "DTIM_PERIOD should be within %d to %d\n",
                IEEE80211_DTIM_MIN,
                IEEE80211_DTIM_MAX);
        return -EINVAL;
    } else {
        retv = wlan_set_param(vap, IEEE80211_DTIM_INTVAL, params->dtim_period);
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_CFG80211,
                "%s: DTIM_PERIOD:%d \n",
                __func__,
                params->dtim_period);

        if (retv != EOK) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_CFG80211, "%s : DTIM period not set \n",
                    __func__);
        }
    }

    /*
     * Set beacon interval
     */
    if (vap->iv_create_flags & IEEE80211_LP_IOT_VAP) {
        params->beacon_interval = IEEE80211_BINTVAL_LP_IOT_IWMIN;
    }

    retv = ieee80211_ucfg_set_beacon_interval(vap, ic, params->beacon_interval, 0);
    if (retv != EOK) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_CFG80211, "%s : Beacon Interval not set \n",
                __func__);
    }

    /*
     * Hidden SSID
     */
    if ((params->hidden_ssid == NL80211_HIDDEN_SSID_ZERO_LEN) || (params->hidden_ssid == NL80211_HIDDEN_SSID_ZERO_CONTENTS)) {
        IEEE80211_VAP_HIDESSID_ENABLE(vap);
    } else {
        IEEE80211_VAP_HIDESSID_DISABLE(vap);
    }

    /*
     * inactivity_timeout: time in seconds to determine station's inactivity.
     */
    wlan_set_param(vap, IEEE80211_RUN_INACT_TIMEOUT, params->inactivity_timeout);
    /* Set IEEE80211_TRIGGER_MLME_RESP to TRUE */
    wlan_set_param(vap, IEEE80211_TRIGGER_MLME_RESP, 1);

    /* add restart flag */
    ieee80211_ucfg_set_freq(vap, vap->iv_cfg80211_channel);
    /* restore ic flags*/
    ic->ic_flags |= ic_flags;
    wlan_cfg80211_security_init(params,dev);

    /*
     * Beacon interval consistency check on VAPs in case of MBSSID
     */
    wlan_vap_up_check_beacon_interval(vap,opmode);
    if (ic->ic_need_vap_reinit) {
        wlan_if_t tmpvap;
        TAILQ_FOREACH(tmpvap, &ic->ic_vaps, iv_next) {
            struct net_device *tmpvap_netdev = ((osif_dev *)tmpvap->iv_ifp)->netdev;
            osifp = (osif_dev *)wlan_vap_get_registered_handle(tmpvap);
            if ((tmpvap != vap) && IS_UP(tmpvap_netdev) &&
                (wlan_vdev_chan_config_valid(tmpvap->vdev_obj) == QDF_STATUS_SUCCESS)) {
                OS_DELAY(10000);
                osif_vap_init(tmpvap_netdev, RESCAN);
            }
        }
        ic->ic_need_vap_reinit = 0;
    }

    retval = osif_vap_init(dev, 0);
    return retval;
}

int wlan_cfg80211_phyerr(struct wiphy *wiphy,
                struct wireless_dev *wdev,
                const void *data,
                int data_len)

{
        struct cfg80211_context *cfg_ctx = NULL;
        struct ieee80211com *ic = NULL;
        u_int8_t error;
        int cmd_type;
        void *cmd;
        struct ol_ath_softc_net80211 *scn = NULL;
        struct ath_diag *req = (struct ath_diag *)data;
        cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
        ic = cfg_ctx->ic;
        cmd = extract_command(ic, wdev, &cmd_type);

        if (cmd_type) {
            scn =  (struct ol_ath_softc_net80211 *)cmd;
        } else {
            qdf_print("Invalid vap Command !");
            return -EINVAL;
        }
        error = ic->ic_cfg80211_radio_handler.ucfg_phyerr(scn, req);

        error = cfg80211_reply_command(wiphy, sizeof (struct ath_diag), req, 0);
        return error;
}

/*
 * wlan_cfg80211_set_freq: This function sets the channel
 * @dev: pointer to net_device
 * @chandef: cfg80211_chan_def struct
 *
 * return 1/0.
 */

int
wlan_cfg80211_set_freq(struct net_device *dev, struct ieee80211_channel *cfgchan)
{
    osif_dev *osnetdev = ath_netdev_priv(dev);
    wlan_if_t vap = osnetdev->os_if;
    int channel  = 0;
    int retval;
    if (osnetdev->is_delete_in_progress) {
        return -EINVAL;
    } else {
        channel = (u_int8_t)wlan_mhz2ieee(osnetdev->os_devhandle, cfgchan->center_freq, 0);
        retval = ieee80211_ucfg_set_freq(vap, channel);
    }
    return retval;
}

static int wlan_cfg80211_get_channel(struct wiphy *wiphy, struct wireless_dev *wdev, struct cfg80211_chan_def *chandef)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    struct ieee80211_ath_channel *ath_chan;
    struct ieee80211_channel *nl_chan;
    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;
    if(!ic) {
        qdf_print("%s: Invalid ic struct pointer .....", __func__);
        return -1;
    }

    ath_chan = ic->ic_curchan;
    if ((ath_chan) && (ath_chan != IEEE80211_CHAN_ANYC)) {
        nl_chan = ieee80211_get_channel(ic->ic_wiphy, wlan_channel_frequency(ath_chan));
        if (!nl_chan) {
            qdf_print("%s: ieee80211_get_channel Failed .....", __func__);
            return -1;
        }
        ieee80211_cfg80211_construct_chandef(ic, chandef, nl_chan);
    } else {
        qdf_print("%s: invalid channel ....",__func__);
        return -1;
    }

    return 0;
}

/*
 * wlan_cfg80211_set_phymode: This function sets the phymode.
 * @dev: pointer to struct netdev
 * @chandef : cfg80211_chan_def structure
 *
 * return 1/0
 */
int wlan_cfg80211_set_phymode(struct net_device *dev, struct cfg80211_chan_def chandef)
{
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    int mode = 0;
    int retval = 0;

    if (!vap)
        return -EINVAL;

    /*
     * support for wireless modes that are not supported by cfg80211
     * b mode, 11ACVHT20, 11ACVHT40
     */
    mode = vap->iv_des_hw_mode;
    if (mode) {
        qdf_info("cfg80211 mode:%d ", mode);
        retval = ieee80211_ucfg_set_wirelessmode(vap, mode);
        return retval;
    }

    /*
     * Covert the channel width to phymode. Need to take care
     * of phymode HT40+ and HT40-
     */
    mode = wlan_cfg80211_chan_to_phymode(chandef);
    if (mode < 0) {
        return -EINVAL;
    } else {

        retval = ieee80211_ucfg_set_wirelessmode(vap, mode);
    }
    return retval;

}

/*
 * wlan_cfg80211_chan_to_phymode ; Convert the channel to phymode.
 * @chandef: cfg80211_chan_def
 * VHT20 / VHT40 - cfg80211 kernel support required.
 * 80P80 - we need to set nlvendor commands
 * return 1/0
 */

int
wlan_cfg80211_chan_to_phymode(struct cfg80211_chan_def chandef)
{
    int phymode = IEEE80211_MODE_AUTO;

    qdf_print("%s: band:%d width: %d channel_cfreq: %d center_freq1: %d chandef.center_freq2: %d flags: %d ", __func__,
		    chandef.chan->band, chandef.width, chandef.chan->center_freq, chandef.center_freq1,
		    chandef.center_freq2, chandef.chan->flags);

    switch (chandef.chan->band) {
        case IEEE80211_BAND_2GHZ:
            switch (chandef.width) {
                case NL80211_CHAN_WIDTH_20_NOHT:
                    phymode = IEEE80211_MODE_11G;
                    break;
                case NL80211_CHAN_WIDTH_20:
                    phymode = IEEE80211_MODE_11NG_HT20;
                    break;
                case NL80211_CHAN_WIDTH_40:
                    if (chandef.chan->center_freq < chandef.center_freq1) {
                        phymode = IEEE80211_MODE_11NG_HT40PLUS;
                    } else {
                        phymode = IEEE80211_MODE_11NG_HT40MINUS;
                    }
                    break;
                default:
                    phymode = IEEE80211_MODE_AUTO;
                    break;
            }
            break;

        case IEEE80211_BAND_5GHZ:
            switch (chandef.width) {
                case NL80211_CHAN_WIDTH_20_NOHT:
                    phymode = IEEE80211_MODE_11A;
                    break;
                case NL80211_CHAN_WIDTH_20:
                    phymode = IEEE80211_MODE_11NA_HT20;
                    break;
                case NL80211_CHAN_WIDTH_40:
                    if (chandef.chan->center_freq < chandef.center_freq1) {
                        phymode = IEEE80211_MODE_11NA_HT40PLUS;
                    } else {
                        phymode = IEEE80211_MODE_11NA_HT40MINUS;
                    }

                    break;
                case NL80211_CHAN_WIDTH_80:
                    phymode = IEEE80211_MODE_11AC_VHT80;
                    break;
                case NL80211_CHAN_WIDTH_80P80:
                    phymode = IEEE80211_MODE_11AC_VHT80_80;
                    break;
                case NL80211_CHAN_WIDTH_160:
                    phymode = IEEE80211_MODE_11AC_VHT160;
                    break;
                default:
                    phymode = IEEE80211_MODE_AUTO;
                    break;

            }
            break;
        default:
            qdf_print("Setting phymode to AUTO");
            phymode = IEEE80211_MODE_AUTO;
            break;
    }

    return phymode;
}

#if ATH_SUPPORT_HS20
extern int ieee80211dbg_setqosmapconf(struct net_device *dev,
                                      struct ieee80211req_athdbg *req);

int wlan_cfg80211_set_qos_map(struct wiphy *wiphy,
        struct net_device *dev,
        struct cfg80211_qos_map *qos_map)
{
   struct ieee80211req_athdbg *req;

   if (qos_map) {
      int status = 0;

      req = qdf_mem_malloc(sizeof(struct ieee80211req_athdbg));
      if ( req == NULL )
         return -ENOMEM;
      OS_MEMCPY(req->data.qos_map.up, qos_map->up, IEEE80211_MAX_QOS_UP_RANGE * (sizeof(struct ieee80211_dscp_range)));
      OS_MEMCPY(req->data.qos_map.dscp_exception, qos_map->dscp_exception, IEEE80211_QOS_MAP_MAX_EX * (sizeof (struct ieee80211_dscp_exception)));
      req->data.qos_map.num_dscp_except = qos_map->num_des;
      req->data.qos_map.valid = 1;

      status = ieee80211dbg_setqosmapconf(dev, req);
      qdf_mem_free(req);
      return status;
   }
   else
      return 0;
}
#endif

int wlan_cfg80211_change_bss(struct wiphy *wiphy,
        struct net_device *dev,
        struct bss_parameters *params)
{
    return 0;
}


/**
 * wlan_cfg80211_change_beacon() - change beacon content in sap mode
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to netdev
 * @params: Pointer to change beacon parameters
 *
 * Return: zero for success non-zero for failure
 */

int wlan_cfg80211_change_beacon(struct wiphy *wiphy,
        struct net_device *dev,
        struct cfg80211_beacon_data *params)
{
    wlan_set_beacon_ies(dev,params);
    return 0;
}


/**
 * wlan_cfg80211_stop_ap - stop ap vap
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to netdev
 *
 * Return: zero for success non-zero for failure
 */

int wlan_cfg80211_stop_ap(struct wiphy *wiphy,
        struct net_device *dev)
{
    int retval = 0;
    retval = osif_vap_stop(dev);
    return retval;
}

/**
 * wlan_cfg80211_scan_abort - abort ongoing scan
 * @wiphy: Pointer to wiphy
 * @wdev: Pointer to wireless_dev
 *
 * Return: void
 */
void wlan_cfg80211_scan_abort(struct wiphy *wiphy, struct wireless_dev *wdev)
{
    struct net_device *dev;
    struct wlan_objmgr_vdev *vdev;
    struct wlan_objmgr_pdev *pdev;
    struct ieee80211com *ic;
    osif_dev *osifp = NULL;
    uint32_t pdev_id;
    uint32_t vdev_id;
    uint32_t scan_id = 0;
    QDF_STATUS status;
    struct cfg80211_context *cfg_ctx = NULL;

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;
    pdev = ic->ic_pdev_obj;
    dev = wdev->netdev;

     /* scan is not applicable for wifiX interface */
    if (ic->ic_wdev.netdev == dev) {
        return;
    } else {
        osifp = ath_netdev_priv(dev);
    }

    vdev = osifp->ctrl_vdev;

    vdev_id = wlan_vdev_get_id(vdev);
    pdev_id = wlan_objmgr_pdev_get_pdev_id(pdev);

    status = wlan_abort_scan(pdev, pdev_id, vdev_id, scan_id, true);
    if (QDF_IS_STATUS_ERROR(status)) {
        qdf_print("%s: scan sbort failed. status: %d", __func__, status);
    }
}

/**
 * wlan_cfg80211_scan_start- API to process cfg80211 scan request
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to net device
 * @request: Pointer to scan request
 *
 * This API responds to scan trigger and update cfg80211 scan database
 * later, scan dump command can be used to recieve scan results
 *
 * Return: 0 for success, non zero for failure
 */
int wlan_cfg80211_scan_start(struct wiphy *wiphy,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 6, 0))
        struct net_device *dev,
#endif
        struct cfg80211_scan_request *request)
{
    /*
     * TODO: scan can trigger internally , we need add a    916
     * flag in scan object to know that trigger happend from internal/wext/cfg80211    917
     * if possible stroe request in scan object itsef 918
     */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0))
    struct net_device *dev = request->wdev->netdev;
#endif
    struct wlan_objmgr_pdev *pdev;
    struct wlan_objmgr_vdev *vdev = NULL;
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    struct scan_params params;
    bool is_connecting = false;
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    int is_ap_cac_timer_running = 0;
    ieee80211_frame_type ftype = IEEE80211_FRAME_TYPE_PROBEREQ;
    u_int32_t ielen = 0;
    int error;
    QDF_STATUS status;

    /* If connection is in progress, do not start scan */
    is_connecting = ieee80211_vap_is_connecting(vap);
    if (is_connecting) {
        scan_info("%s: vap: 0x%pK is connecting, return BUSY", __func__, vap);
        return -EBUSY;
    }

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    /* scan is not applicable for wifiX interface */
    if (ic->ic_wdev.netdev == dev) {
        return -1;
    }

#if ATH_SUPPORT_WRAP
    /* Indicate SCAN_DONE directly for VAP's not able to scan */
    if (vap->iv_no_event_handler) {
        qdf_print("%s:Bypass scan for proxysta", __func__);
        cfg80211_scan_done(request, false);
        return QDF_STATUS_SUCCESS;
    }
#endif

#if DBDC_REPEATER_SUPPORT
    /*Flush the scan entries to remove old beacons*/
    if (ic->ic_global_list->same_ssid_support) {
        request->flags |= NL80211_SCAN_FLAG_FLUSH;
    }
#endif

    if (ic->no_chans_available
#if ATH_SUPPORT_DFS && QCA_DFS_NOL_VAP_RESTART
            || ic->ic_pause_stavap_scan
#endif
       ) {
        return -EBUSY;
    }


    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_CFG80211, "%s \n",
            __func__);

    pdev = ic->ic_pdev_obj;
    ucfg_dfs_is_ap_cac_timer_running(pdev, &is_ap_cac_timer_running);
    if (is_ap_cac_timer_running) {
        return -EBUSY;
    }

    if (ic->ic_flags & IEEE80211_F_CHANSWITCH) {
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_CFG80211,
                "%s CSA in progress. Discard scan\n", __func__);
        return -EBUSY;
    }

    vdev = vap->vdev_obj;
    if (!vdev) {
        return -EINVAL;
    }

    status = wlan_objmgr_vdev_try_get_ref(vdev, WLAN_OSIF_ID);
    if (QDF_IS_STATUS_ERROR(status)) {
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_CFG80211,
                "%s Couldnt get vdev ref, fail new scan\n", __func__);
        return -EINVAL;
    }

    /* Initialize driver specific scan config param */
    params.source = NL_SCAN;
    params.default_ie.len = 0;
    params.default_ie.ptr = NULL;

    if (vap->iv_app_ie_list[ftype].total_ie_len > 0) {
        params.vendor_ie.ptr = qdf_mem_malloc(vap->iv_app_ie_list[ftype].total_ie_len);
        if(!params.vendor_ie.ptr) {
            wlan_objmgr_vdev_release_ref(vdev, WLAN_OSIF_ID);
            return -ENOMEM;
        }

        error = wlan_mlme_get_appie(vap, ftype, params.vendor_ie.ptr, &ielen,
                vap->iv_app_ie_list[ftype].total_ie_len, DEFAULT_IDENTIFIER);
        if (error) {
            wlan_objmgr_vdev_release_ref(vdev, WLAN_OSIF_ID);
            return -EINVAL;
        }

        params.vendor_ie.len = ielen;
    } else {
        params.vendor_ie.len = 0;
        params.vendor_ie.ptr = NULL;
    }

    params.half_rate = false;
    params.quarter_rate= false;
    params.strict_pscan = false;

    if (ieee80211_ic_enh_ind_rpt_is_set(ic)) {
        params.priority = SCAN_PRIORITY_HIGH;
    } else {
        params.priority = SCAN_PRIORITY_COUNT;
    }

    if (request->n_channels) {
        if (ic->ic_chanbwflag & IEEE80211_CHAN_HALF)
            params.half_rate = true;
        else if (ic->ic_chanbwflag & IEEE80211_CHAN_QUARTER)
            params.quarter_rate= true;
    }
    if (ic->ic_strict_pscan_enable) {
        params.strict_pscan = true;
    }

    status = wlan_cfg80211_scan(vdev, request, &params);
    wlan_objmgr_vdev_release_ref(vdev, WLAN_OSIF_ID);

    if (params.vendor_ie.ptr)
        qdf_mem_free(params.vendor_ie.ptr);

    return status;
}
#if UMAC_SUPPORT_WPA3_STA
int wlan_cfg80211_external_auth(struct wiphy *wiphy, struct net_device *dev,
                                 struct cfg80211_external_auth_params *params)
{
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;

    qdf_timer_sync_cancel(&vap->iv_sta_external_auth_timer);
    qdf_info("external auth status %d", params->status);

    vap->iv_mlme_priv->im_request_type = 0;
    IEEE80211_DELIVER_EVENT_MLME_AUTH_COMPLETE(vap, NULL, params->status);
    return 0;
}
#endif
/**
 * wlan_cfg80211_connect() - cfg80211 connect api
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to network device
 * @req: Pointer to cfg80211 connect request
 *
 * Return: 0 for success, non-zero for failure
 */
int wlan_cfg80211_connect(struct wiphy *wiphy,
        struct net_device *ndev,
        struct cfg80211_connect_params *req)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    ieee80211_ssid   tmpssid;
    enum ieee80211_opmode opmode;
    osif_dev *osifp = ath_netdev_priv(ndev);
    wlan_if_t vap = osifp->os_if, tempvap = NULL;
    u_int8_t des_bssid[IEEE80211_ADDR_LEN] = {0x0,0x0,0x0,0x0,0x0,0x0};
    u_int8_t skip_bssid_set = 0;
    u_int8_t ret_val = 0;
#if DBDC_REPEATER_SUPPORT && ATH_SUPPORT_WRAP
    wlan_if_t stavap;
#endif
    opmode = wlan_vap_get_opmode(vap);

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    if (osifp->is_delete_in_progress)
        return -EINVAL;

    /*Set essid, channel , bssid*/
    if (opmode == IEEE80211_M_WDS)
        return -EOPNOTSUPP;

#if ATH_SUPPORT_WRAP
    if (vap->iv_no_event_handler && ic->ic_sta_vap) {
        if (!wlan_is_connected(ic->ic_sta_vap)) {
            return -EBUSY;
        }
    }
#endif

    OS_MEMZERO(&tmpssid, sizeof(ieee80211_ssid));

#if UMAC_SUPPORT_WPA3_STA
    if (req->flags & CONNECT_REQ_EXTERNAL_AUTH_SUPPORT)
        vap->iv_sta_external_auth_enabled = true;
    else
        vap->iv_sta_external_auth_enabled = false;
#endif
    tmpssid.len = req->ssid_len;
    OS_MEMCPY(tmpssid.ssid, req->ssid, req->ssid_len);
    /*
     * Deduct a trailing \0 since iwconfig passes a string
     * length that includes this.  Unfortunately this means
     * that specifying a string with multiple trailing \0's
     * won't be handled correctly.  Not sure there's a good
     * solution; the API is botched (the length should be
     * exactly those bytes that are meaningful and not include
     * extraneous stuff).
     */
    if (req->ssid_len > 0 &&
            tmpssid.ssid[req->ssid_len-1] == '\0'){
        tmpssid.len--;
    }
    wlan_set_desired_ssidlist(vap,1,&tmpssid);

    qdf_print("DES SSID SET=%s  ",tmpssid.ssid);

#if DBDC_REPEATER_SUPPORT
    if (ic->ic_global_list->same_ssid_support) {
        if (IS_NULL_ADDR(ic->ic_preferred_bssid)) {
            return -EBUSY;
        }
    }
#endif
    if (req->bssid && !IS_NULL_ADDR(req->bssid)) {
        /*If AP mac to which our sta vap is trying to connect has
          same mac as one of our ap vaps ,dont set that as sta bssid */
        TAILQ_FOREACH(tempvap, &ic->ic_vaps, iv_next) {
            if (tempvap->iv_opmode == IEEE80211_M_HOSTAP && IEEE80211_ADDR_EQ(tempvap->iv_myaddr,req->bssid)) {
                qdf_print("[%s] Mac collision for [%s]",__func__,ether_sprintf(req->bssid));
                skip_bssid_set = 1;
                break;
            }
        }
        if (skip_bssid_set == 0) {
            IEEE80211_ADDR_COPY(des_bssid, req->bssid);
        }
    }
#if DBDC_REPEATER_SUPPORT
    if (ic->ic_global_list->same_ssid_support) {
           if (vap == ic->ic_sta_vap) {
               if(!IEEE80211_ADDR_EQ(ic->ic_preferred_bssid, des_bssid)) {
                   return -EBUSY;
               }
           } else {
#if ATH_SUPPORT_WRAP
                /*For PSTA desired BSSID should match with MPSTA bssid mac address*/
               if (vap->iv_psta && !vap->iv_mpsta) {
                   stavap = ic->ic_sta_vap;
                   if (stavap)
                       if(!IEEE80211_ADDR_EQ(stavap->iv_bss->ni_macaddr, des_bssid))
                           IEEE80211_ADDR_COPY(des_bssid, stavap->iv_bss->ni_macaddr);
               }
#endif
           }
    }
#endif
    if (!IS_NULL_ADDR(des_bssid)) {
        wlan_aplist_set_desired_bssidlist(vap, 1, &des_bssid);
        qdf_print("DES BSSID SET=%s  ",ether_sprintf(des_bssid));
    }

    if (req->ie_len) {
        wlan_mlme_set_optie(vap, (u_int8_t *)req->ie, req->ie_len);
    } else {
        wlan_set_clr_appopt_ie(vap);
    }

    /*set privacy */
    wlan_set_param(vap,IEEE80211_FEATURE_PRIVACY,req->privacy);
    ret_val = wlan_cfg80211_crypto_setting(ndev,&(req->crypto),req->auth_type);

    if (((u_int8_t)osifp->os_opmode == IEEE80211_M_STA) && !ret_val && IS_UP(ndev))
        return 0;

    if ((ndev->flags & IFF_UP) && (osifp->os_opmode == IEEE80211_M_STA ||
                (u_int8_t)osifp->os_opmode == IEEE80211_M_P2P_GO ||
                (u_int8_t)osifp->os_opmode == IEEE80211_M_P2P_CLIENT ||
                osifp->os_opmode == IEEE80211_M_IBSS)) {
        if (vap->iv_roam.iv_roaming)
            osif_vap_roam(ndev);
        else
            osif_vap_init(ndev, 0);
    }
    return 0 ;
}

/**
 * wlan_cfg80211_disconnect() - cfg80211 disconnect api
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to network device
 * @reason: Disconnect reason code
 *
 * Return: 0 for success, non-zero for failure
 */
int wlan_cfg80211_disconnect(struct wiphy *wiphy,
        struct net_device *dev, u16 reason)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    int retval = 0;
    enum ieee80211_opmode opmode;
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    opmode = wlan_vap_get_opmode(vap);
    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;
    vap->iv_locally_generated_disconnect = 1;
    if(opmode == IEEE80211_M_STA || opmode == IEEE80211_M_P2P_CLIENT) {
#if ATH_SUPPORT_DFS && ATH_SUPPORT_STA_DFS
        if(wlan_mlme_is_stacac_running(vap)) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME, "Do not stop the BSS STA CAC is on\n");
            return -EINVAL;
        } else {
            retval = osif_vap_stop(dev);
        }
#else
        retval = osif_vap_stop(dev);
#endif
    }

    return retval;
}

/**
 * wlan_cfg80211_set_wiphy_params - set wiphy parameters
 * @wiphy: Pointer to wiphy
 * @changed: Parameters changed
 * Return: 0 for success, non-zero for failure
 */
int wlan_cfg80211_set_wiphy_params(struct wiphy *wiphy, u32 changed)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    int value = 0, curval = 0, retval = 0;
    wlan_if_t vap = NULL , tvap = NULL;
    osif_dev *osifp = NULL;

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    if (changed & WIPHY_PARAM_RTS_THRESHOLD) {
        value = (wiphy->rts_threshold == CFG80211_RTS_THRESHOLD_DISABLE) ?
            IEEE80211_RTS_MAX : wiphy->rts_threshold;

        if ( (value < IEEE80211_RTS_MIN) ||
                (value > IEEE80211_RTS_MAX)) {
            return -EINVAL;
        }

        /* RTS values are validated, apply parameter for all vaps */
        if (!TAILQ_EMPTY(&ic->ic_vaps)) {
            TAILQ_FOREACH_SAFE(vap, &ic->ic_vaps, iv_next, tvap) {
                curval = wlan_get_param(vap, IEEE80211_RTS_THRESHOLD);
                if (value != curval) {
                    osifp = (osif_dev *)vap->iv_ifp;
                    wlan_set_param(vap, IEEE80211_RTS_THRESHOLD, value);
                    if (IS_UP(osifp->netdev) && (vap->iv_novap_reset == 0)) {
                        osif_vap_init(osifp->netdev, RESCAN);
                    }
                }
            }
        }
    }

    if(changed & WIPHY_PARAM_FRAG_THRESHOLD) {
        value = (wiphy->frag_threshold == CFG80211_FRAG_THRESHOLD_DISABLE) ?
            IEEE80211_FRAGMT_THRESHOLD_MAX : wiphy->frag_threshold;

        if ( (value < IEEE80211_FRAGMT_THRESHOLD_MIN) ||
                (value > IEEE80211_FRAGMT_THRESHOLD_MAX)) {
            return -EINVAL;
        }

        if (!TAILQ_EMPTY(&ic->ic_vaps)) {

            TAILQ_FOREACH_SAFE(vap, &ic->ic_vaps, iv_next, tvap) {
                if((wlan_get_desired_phymode(vap) != IEEE80211_MODE_AUTO) &&
                        (wlan_get_desired_phymode(vap) < IEEE80211_MODE_11NA_HT20))
                {
                    curval = wlan_get_param(vap, IEEE80211_FRAG_THRESHOLD);
                    if (value != curval) {
                        osifp = (osif_dev *)vap->iv_ifp;
                        wlan_set_param(vap, IEEE80211_FRAG_THRESHOLD, value);
                        if (IS_UP(osifp->netdev)) {
                            osif_vap_init(osifp->netdev, RESCAN);
                        }
                    }
                } else {
                    qdf_print("WARNING: Fragmentation with HT mode NOT ALLOWED!!");
                    retval = -EINVAL;
                }
            }
        }
    }

    return retval;
}

/**
 * wlan_cfg80211_set_txpower - set TX power
 * @wiphy: Pointer to wiphy
 * @wdev: Pointer to network device
 * @type: TX power setting type
 * @dbm: TX power in dbm
 *
 * Return: 0 for success, non-zero for failure
 */
int wlan_cfg80211_set_txpower(struct wiphy *wiphy,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 8, 0)
        struct wireless_dev *wdev,
#endif
        enum nl80211_tx_power_setting type,
        int dbm)
{
    return 0;
}

/**
 * wlan_cfg80211_get_txpower - cfg80211 get power handler function
 * @wiphy: Pointer to wiphy structure.
 * @wdev: Pointer to wireless_dev structure.
 * @dbm: dbm
 *
 * Return: 0 for success, error number on failure.
 */
int wlan_cfg80211_get_txpower(struct wiphy *wiphy,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 8, 0) || defined(WITH_BACKPORTS)
        struct wireless_dev *wdev,
#endif
        int *dbm)
{
    return 0;
}

/**
 * wlan_cfg80211_set_default_mgmt_key - set_default_mgmt_key
 * @wiphy: pointer to wiphy
 * @netdev: pointer to net_device structure
 * @key_index: key index
 *
 * Return: 0 on success, error number on failure
 */
int wlan_cfg80211_set_default_mgmt_key(struct wiphy *wiphy,
        struct net_device *netdev,
        u8 key_index)
{
    osif_dev *osifp = ath_netdev_priv(netdev);
    wlan_if_t vap = osifp->os_if;

    wlan_set_default_keyid(vap,key_index);

    return 0;
}

/**
 * wlan_cfg80211_get_station() - get station statistics
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to network device
 * @mac: Pointer to mac
 * @sinfo: Pointer to station info
 *
 * Return: 0 for success, non-zero for failure
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0))
int wlan_cfg80211_get_station(struct wiphy *wiphy,
        struct net_device *dev, const uint8_t *mac,
        struct station_info *sinfo)
#else
int wlan_cfg80211_get_station(struct wiphy *wiphy,
        struct net_device *dev, uint8_t *mac,
        struct station_info *sinfo)
#endif
{


    return 0;
}

/**
 * wlan_cfg80211_set_power_mgmt() - set cfg80211 power management config
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to network device
 * @mode: Driver mode
 * @timeout: Timeout value
 *
 * Return: 0 for success, non-zero for failure
 */
int wlan_cfg80211_set_power_mgmt(struct wiphy *wiphy,
        struct net_device *dev, bool mode,
        int timeout)
{
    return 0;
}


/**
 * wlan_regulatory_notifier: Regulatory callback
 * @wiphy: Pointer to wiphy
 * @request: Pointer to regulatory_request.
 *
 */
void wlan_regulatory_notifier(struct wiphy *wiphy, struct regulatory_request *request)
{

    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_CFG80211,"country: %c%c, initiator %d, dfs_region: %d",
            request->alpha2[0],
            request->alpha2[1],
            request->initiator,
            request->dfs_region);
}

/**
 * wlan_cfg80211_configure_acl: Configure ACL policy and acl table.
 * @ic: pointer to ieee80211com.
 * @vap: pointer to vap.
 * @params: pointer to cfg80211_acl_data
 *
 * Return; 0 on success, error number otherwise
 */

int wlan_cfg80211_configure_acl(struct ieee80211com *ic, wlan_if_t vap,
        const struct cfg80211_acl_data *params)
{
    int i = 0;

    /* clear acl */
    if (params->n_acl_entries == 0) {
        /* disable acl and flush acl table */
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_CFG80211, "%s: clearing acl ... \n",
                __func__);
        wlan_set_acl_policy(vap, IEEE80211_MACCMD_POLICY_OPEN, IEEE80211_ACL_FLAG_ACL_LIST_1);
        wlan_set_acl_policy(vap, IEEE80211_MACCMD_FLUSH, IEEE80211_ACL_FLAG_ACL_LIST_1);
        return 0;
    }

    /* Replace existing list with new list */
    wlan_set_acl_policy(vap, IEEE80211_MACCMD_FLUSH, IEEE80211_ACL_FLAG_ACL_LIST_1);
    for (i = 0; i < params->n_acl_entries; i++) {
        wlan_set_acl_add(vap, params->mac_addrs[i].addr, IEEE80211_ACL_FLAG_ACL_LIST_1);
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_CFG80211, "%s: adding mac:%s \n",
                __func__, ether_sprintf(params->mac_addrs[i].addr));
    }
    /*
     * @NL80211_ACL_POLICY_ACCEPT_UNLESS_LISTED: Deny stations which are
     *      listed in ACL, i.e. allow all the stations which are not listed
     *      in ACL to authenticate.
     * @NL80211_ACL_POLICY_DENY_UNLESS_LISTED: Allow the stations which are listed
     *      in ACL, i.e. deny all the stations which are not listed in ACL.
     */

    /* set acl policy */
    if (params->acl_policy == NL80211_ACL_POLICY_ACCEPT_UNLESS_LISTED){
        wlan_set_acl_policy(vap, IEEE80211_MACCMD_POLICY_DENY, IEEE80211_ACL_FLAG_ACL_LIST_1);
    } else if (params->acl_policy == NL80211_ACL_POLICY_DENY_UNLESS_LISTED){
        wlan_set_acl_policy(vap, IEEE80211_MACCMD_POLICY_ALLOW, IEEE80211_ACL_FLAG_ACL_LIST_1);
    }
    return 0;
}

/**
 * wlan_cfg80211_set_mac_acl: apply ACL configuration.
 * @wiphy: pointer to wiphy structure
 * @dev: pointer to net_device
 * @params: pointer to cfg80211_acl_data
 *
 * Return; 0 on success, error number otherwise
 */
int wlan_cfg80211_set_mac_acl(struct wiphy *wiphy,
        struct net_device *dev,
        const struct cfg80211_acl_data *params)
{
#if CONFIGURE_ACL_HOSTAPD
    /* Enable only when hostapd needs to manage ACL.
     * Currently this is in disabled state as ACL is
     * managed in the Driver.
     */
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    osif_dev *osifp = NULL;
    wlan_if_t vap = NULL;

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    osifp = ath_netdev_priv(dev);
    vap = osifp->os_if;

    wlan_cfg80211_configure_acl(ic, vap, params);
#endif
    return 0;
}

/* FTM */
/**
 * wlan_cfg80211_testmode()
 * Receives UTF commands from FTM daemon
 * @wiphy: Pointer to wiphy
 * @wdev: Pointer to wireless device
 * @data: Data pointer
 * @len: Data length
 *
 * Return: 0 for success, non-zero for failure
 */
int wlan_cfg80211_testmode(struct wiphy *wiphy,
                                  struct wireless_dev *wdev,
                                  void *data, int len)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    struct ol_ath_softc_net80211 *scn = NULL;
    int cmd_type;
    void *cmd;

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    cmd = extract_command(ic, wdev, &cmd_type);
    if (cmd_type) {
        scn =  (struct ol_ath_softc_net80211 *)cmd;
    } else {
        QDF_TRACE(QDF_MODULE_ID_CONFIG, QDF_TRACE_LEVEL_ERROR, "Invalid vap Command !");
        return -EINVAL;
    }

    return ic->ic_ucfg_testmode_cmd(scn, ATH_FTM_UTF_CMD, (char*) data, len);
}

#ifdef WLAN_SUPPORT_FILS
/*
 * wlan_cfg80211_set_fils_aad: Set FILS AAD data from Hostapd
 * @wiphy: pointer to wiphy structure
 * @dev: pointer to net_device
 * @fils_aad: pointer to cfg80211_fils_aad structure
 *
 * Return: 0 on Success, error number otherwise
 */
int wlan_cfg80211_set_fils_aad(struct wiphy *wiphy, struct net_device *dev,
                               struct cfg80211_fils_aad *fils_aad)
{
    osif_dev  *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    struct ieee80211req_fils_aad filsaad;
    uint8_t peer_mac[QDF_MAC_ADDR_SIZE] = {};

    if (!vap || !fils_aad)
        return -EINVAL;

    if (!wlan_fils_is_enable(vap->vdev_obj)) {
        qdf_print("%s: FILS is Disable; Set FILS AAD failed", __func__);
        return -EINVAL;
    }
    qdf_mem_zero(&filsaad, sizeof(struct ieee80211req_fils_aad));
    qdf_mem_copy(filsaad.SNonce, fils_aad->snonce, IEEE80211_FILS_NONCE_LEN);
    qdf_mem_copy(filsaad.ANonce, fils_aad->anonce, IEEE80211_FILS_NONCE_LEN);
    qdf_mem_copy(filsaad.kek, fils_aad->kek, fils_aad->kek_len);
    filsaad.kek_len = fils_aad->kek_len;
    qdf_mem_copy(peer_mac, fils_aad->macaddr, QDF_MAC_ADDR_SIZE);

    if (QDF_STATUS_SUCCESS !=
        wlan_mlme_auth_fils(vap->vdev_obj, &filsaad, peer_mac)) {
        qdf_print("%s: FILS crypto registration failed", __func__);
    } else {
        qdf_print("%s: FILS crypto registered successfully", __func__);
    }

    return 0;
}
#endif /* WLAN_SUPPORT_FILS */

int wlan_cfg80211_remain_on_channel(struct wiphy *wiphy,
                                     struct wireless_dev *wdev,
                                     struct ieee80211_channel *chan,
                                     unsigned int duration,
                                     u64 *cookie)
{
    osif_dev *osifp = ath_netdev_priv(wdev->netdev);
    struct ieee80211vap *vap = osifp->os_if;
    struct ieee80211com *ic = vap->iv_ic;
    int ret = 0;
    struct ieee80211_offchan_req offchan_req;

    if (chan->center_freq != 0 && chan->center_freq !=  ieee80211_chan2freq(ic,ic->ic_curchan)) {
        offchan_req.freq = chan->center_freq;
        offchan_req.dwell_time = duration;
        offchan_req.cookie = *cookie;
        offchan_req.request_type = IEEE80211_OFFCHAN_RX;
        ret = wlan_remain_on_channel(vap, &offchan_req);
    }

    return ret;
}

int wlan_cfg80211_cancel_remain_on_channel(struct wiphy *wiphy,
                                            struct wireless_dev *wdev,
                                            u64 cookie)
{
    osif_dev *osifp = ath_netdev_priv(wdev->netdev);
    struct ieee80211vap *vap = osifp->os_if;

    if(vap->iv_mgmt_offchan_current_req.cookie &&
        vap->iv_mgmt_offchan_current_req.freq)
        wlan_cancel_remain_on_channel(vap);

    return 0;
}

/*
 * wlan_cfg80211_mgmt_tx: Transmit mgmt packet from hostapd.
 * @wiphy: pointer to wiphy structure
 * @dev: pointer to net_device
 * @params: pointer to cfg80211_mgmt_tx_params
 * @cookie : cookie information
 *
 * Return; 0 on success, error number otherwise
 */
int wlan_cfg80211_mgmt_tx(struct wiphy *wiphy, struct wireless_dev *wdev,
        struct cfg80211_mgmt_tx_params *params, u64 *cookie)
{

    osif_dev  *osifp = ath_netdev_priv(wdev->netdev);
    wlan_if_t vap = osifp->os_if;
    u8 *buf;
    int rc = 0;
    const struct ieee80211_mgmt *mgmt;
    struct ieee80211req_mgmtbuf *mgmt_frm;
    struct ieee80211_node *ni;
    int data_len=params->len;
    int type = -1, subtype;
    bool send_tx_status = true;

    buf = (u8 *)qdf_mem_malloc(MAX_TX_RX_PACKET_SIZE - sizeof(struct ieee80211_qosframe));
    if(!buf) {
        return -ENOMEM;
    }
    mgmt = (const struct ieee80211_mgmt *) params->buf;
    mgmt_frm = (struct ieee80211req_mgmtbuf *) buf;
    memcpy(mgmt_frm->macaddr, (u8 *)mgmt->da, IEEE80211_ADDR_LEN);
    mgmt_frm->buflen = params->len;

    if (&mgmt_frm->buf[0] + data_len > (buf + (MAX_TX_RX_PACKET_SIZE - sizeof(struct ieee80211_qosframe)))) {
        qdf_print("wlan_send_mgmt: Too long frame for "
                "wlan_send_mgmt (%u)", (unsigned int) params->len);
        qdf_mem_free(buf);
        return -1;
    }


    type = mgmt->frame_control & IEEE80211_FC0_TYPE_MASK;
    subtype = mgmt->frame_control & IEEE80211_FC0_SUBTYPE_MASK;

    if ((type == IEEE80211_FC0_TYPE_MGT) &&
        ((subtype == IEEE80211_FC0_SUBTYPE_DISASSOC) ||
         (subtype == IEEE80211_FC0_SUBTYPE_DEAUTH))) {
        send_tx_status = false;
    }

    if ((type == IEEE80211_FC0_TYPE_MGT) && ((subtype == IEEE80211_FC0_SUBTYPE_ASSOC_RESP) ||(subtype == IEEE80211_FC0_SUBTYPE_REASSOC_RESP))) {
        uint8_t elem_len;
        uint8_t *ptr = (uint8_t *) mgmt->u.assoc_resp.variable ;
        uint16_t pos = 0 ;
        struct ieee80211_app_ie_t optie;

        send_tx_status = false;
        data_len -= (char *) mgmt->u.assoc_resp.variable - (char *) mgmt;

        while (data_len >= 2) {
            elem_len = ptr[1];
            data_len -= 2;
            if (elem_len > data_len) {
                qdf_print("Invalid IEs elem_len=%d left=%d",
                    elem_len, data_len);
                break;
            }
            if (!is_hostie(ptr, IE_LEN(ptr))) {
#ifdef WLAN_SUPPORT_FILS
                /* After FILS Indication element all IEs has to be encrypted. */
                if ((ptr[0] == WLAN_ELEMID_EXTN_ELEM) && (ptr[2] == WLAN_ELEMID_EXT_FILS_SESSION)) {
                    qdf_mem_copy(&mgmt_frm->buf[pos], ptr, data_len + 2);
                    pos += data_len + 2;
                    break;
                }
#endif /* WLAN_SUPPORT_FILS */
                qdf_mem_copy(&mgmt_frm->buf[pos], ptr, IE_LEN(ptr));
                pos += (elem_len + 2);
            }
            data_len -= elem_len;
            ptr += (elem_len + 2);
        }
        optie.ie = &mgmt_frm->buf[0];
        optie.length = pos;

        ni = ieee80211_vap_find_node(vap, mgmt_frm->macaddr);

        if (ni != NULL) {
            if (ieee80211_is_pmf_enabled(vap, ni)
                    && vap->iv_opmode == IEEE80211_M_HOSTAP
                    && (vap->iv_pmf_enable > 0)
                    && (ni->ni_flags & IEEE80211_NODE_AUTH)) {
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_ASSOC,
                   "[%s] drop assoc resp for pmf client from hostapd\n",
                   __func__);
                cfg80211_mgmt_tx_status(wdev, *cookie, params->buf, params->len, true, GFP_ATOMIC);
            } else {
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_ASSOC,
                    "[%s] (Re)Assoc Resp for macaddr:%s \n",
                    __func__, ether_sprintf(mgmt_frm->macaddr));
                rc = wlan_mlme_assoc_resp(vap, mgmt_frm->macaddr,
                    mgmt->u.assoc_resp.status_code,
                    ((subtype == IEEE80211_FC0_SUBTYPE_ASSOC_RESP) ? 0:1),
                    &optie);
            }
            ieee80211_free_node(ni);
        } else {
            qdf_print("%s (Re)Assoc Resp failure, ni is NULL for macaddr:%s ",__func__,ether_sprintf(mgmt_frm->macaddr));
            rc = -EINVAL;
        }
    } else if (params->offchan &&
              (type == IEEE80211_FC0_TYPE_MGT) &&
              (subtype == IEEE80211_FC0_SUBTYPE_ACTION)) {

        struct ieee80211_offchan_req offchan_req;

        offchan_req.freq       = params->chan->center_freq;
        offchan_req.dwell_time = params->wait;
        /* when large number of channels are provided, the completions are
           issued at end of the scan, there is a remote chance that the scan
           requester would timeout. Fixing the max dwell time 1.5 seconds */
        if (offchan_req.dwell_time >= vap->iv_ic->ic_offchan_dwell_time)
            offchan_req.dwell_time = vap->iv_ic->ic_offchan_dwell_time;
        qdf_mem_copy(&offchan_req.cookie, cookie, sizeof(u64));
        offchan_req.request_type = IEEE80211_OFFCHAN_TX;

        /* if frame is added in offchan queue sucessfully, then don't send tx status in this function */
        if(wlan_offchan_mgmt_tx_start(vap, mgmt->da, mgmt->sa,
                                      mgmt->bssid, (params->buf+24),
                                      (params->len- 24), &offchan_req) == 0) {
            send_tx_status = false;
        }
    } else {
        memcpy(&mgmt_frm->buf[0],params->buf, params->len);
        rc = wlan_send_mgmt(vap, mgmt_frm->macaddr, mgmt_frm->buf, mgmt_frm->buflen);
    }

    if(send_tx_status) {
        cfg80211_mgmt_tx_status(wdev, *cookie, params->buf, params->len, ((rc == QDF_STATUS_SUCCESS) ? true : false), GFP_ATOMIC);
    } else if(rc != QDF_STATUS_SUCCESS) {
        cfg80211_mgmt_tx_status(wdev, *cookie, params->buf, params->len, false, GFP_ATOMIC);
    }
    qdf_mem_free(buf);
    return rc;
}

/**
 * wlan_cfg80211_mgmt_tx_cancel_wait: Transmit mgmt packet from hostapd.
 * @wiphy: pointer to wiphy structure
 * @dev: pointer to net_device
 * @params: pointer to cfg80211_mgmt_tx_params
 * @cookie : cookie information
 *
 * Return; 0 on success, error number otherwise
 */
int wlan_cfg80211_mgmt_tx_cancel_wait(struct wiphy *wiphy,
        struct wireless_dev *wdev, u64 cookie)
{
    return 0;
}

/**
 * cfg80211_get_ac_internal: convert nl80211 AC to internal AC
 * @ac: Access category
 *
 * Return; 0 on success, error number otherwise
 */
int cfg80211_get_ac_internal(enum nl80211_ac ac)
{
    int ac_internal = WME_AC_BE;
    switch(ac)
    {
        case NL80211_AC_BK:
            ac_internal = WME_AC_BK;
            break;
        case NL80211_AC_BE:
            ac_internal = WME_AC_BE;
            break;
        case NL80211_AC_VI:
            ac_internal = WME_AC_VI;
            break;
        case NL80211_AC_VO:
            ac_internal = WME_AC_VO;
            break;
        default:
            ac_internal = WME_AC_BE;
            break;
    }
    return ac_internal;
}

/**
 * wlan_configure_wmm_params: configure WMM params
 * @vaphandle: pointer to VAP.
 * @mode: Phymode.
 * @param: WME param type.
 * @ac: access category.
 * @val: WME param value.
 * Return; 0 on success, error number otherwise
 */
int wlan_configure_wmm_params(wlan_if_t vaphandle, enum ieee80211_phymode mode,
        wlan_wme_param param, u_int8_t ac, u_int32_t val)
{
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211_wme_state *wme = &ic->ic_wme;

    switch (param)
    {
        case WLAN_WME_CWMIN:
            wme->wme_wmeChanParams.cap_wmeParams[ac].wmep_logcwmin = val;
            wme->wme_chanParams.cap_wmeParams[ac].wmep_logcwmin = val;
            ic->phyParamForAC[ac][mode].logcwmin = val;
            break;
        case WLAN_WME_CWMAX:
            wme->wme_wmeChanParams.cap_wmeParams[ac].wmep_logcwmax = val;
            wme->wme_chanParams.cap_wmeParams[ac].wmep_logcwmax = val;
            ic->phyParamForAC[ac][mode].logcwmax = val;
            break;
        case WLAN_WME_AIFS:
            wme->wme_wmeChanParams.cap_wmeParams[ac].wmep_aifsn = val;
            wme->wme_chanParams.cap_wmeParams[ac].wmep_aifsn = val;
            ic->phyParamForAC[ac][mode].aifsn = val;
            break;
        case WLAN_WME_TXOPLIMIT:
            wme->wme_wmeChanParams.cap_wmeParams[ac].wmep_txopLimit = val;
            wme->wme_chanParams.cap_wmeParams[ac].wmep_txopLimit = val;
            ic->phyParamForAC[ac][mode].txopLimit = val;
            break;
        default:
            break;
    }

    return 0;
}

/**
 * wlan_cfg80211_set_txq_params: set txq params.
 * @wiphy: pointer to wiphy structure
 * @dev: pointer to net_device
 * @params: pointer to ieee80211_txq_params
 *
 * Return; 0 on success, error number otherwise
 */
int wlan_cfg80211_set_txq_params(struct wiphy *wiphy,
        struct net_device *dev,
        struct ieee80211_txq_params *params)
{
    /*
     * WMM is handled internally by driver (Custom WMM)
     * user vendor commands to configure WMM parameters
     */
#if CONFIGURE_MANAGE_WMM_HOSTAPD
    /* Enable only when hostapd needs to manage WMM.
     * Currently this is in disabled state as WMM parameters are
     * managed in the Driver.
     */
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    osif_dev *osifp = NULL;
    wlan_if_t vap = NULL;
    int ac_internal = 0;
    enum ieee80211_phymode mode;

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    osifp = ath_netdev_priv(dev);
    vap = osifp->os_if;

    if (vap->iv_bsschan != IEEE80211_CHAN_ANYC) {
        mode = ieee80211_chan2mode(vap->iv_bsschan);
    } else {
        mode = IEEE80211_MODE_AUTO;
    }
    ac_internal = cfg80211_get_ac_internal(params->ac);

    wlan_configure_wmm_params(vap, mode, WLAN_WME_CWMIN, ac_internal, params->cwmin);
    wlan_configure_wmm_params(vap, mode, WLAN_WME_CWMAX, ac_internal, params->cwmax);
    wlan_configure_wmm_params(vap, mode, WLAN_WME_AIFS, ac_internal, params->aifs);
    wlan_configure_wmm_params(vap, mode, WLAN_WME_TXOPLIMIT, ac_internal, params->txop);
#endif
    return 0;
}

/**
 *   ieee80211_cfg80211_send_connect_result: send connect result to cfg80211
 *   This function send connection result to cfg80211 kernel module.
 *   @osdev : pointer to osdev.
 *   @bssid : bssid of connected AP.
 *
 *   returns NONE.
 */
void ieee80211_cfg80211_send_connect_result(osif_dev  *osdev, u_int8_t bssid[IEEE80211_ADDR_LEN], uint8_t status)
{
    wlan_if_t vap = osdev->os_if;
    struct net_device *dev = osdev->netdev;
    struct wireless_dev *wdev = dev->ieee80211_ptr;

    if (!wdev->ssid_len) {
        wdev->ssid_len =  vap->iv_des_ssid[0].len;
        memcpy(wdev->ssid, vap->iv_des_ssid[0].ssid, wdev->ssid_len);
    }
    qdf_nofl_info("ssid: %s len: %d bssid:%s ",wdev->ssid, wdev->ssid_len,ether_sprintf(bssid));

    if(!vap->iv_roam.iv_roaming) {
        cfg80211_connect_result(dev,
                bssid,
                NULL, 0, vap->iv_sta_assoc_resp_ie, vap->iv_sta_assoc_resp_len,
                status,
                GFP_ATOMIC);
    } else {
        cfg80211_roamed(dev,
                NULL,
                bssid,
                NULL, 0, vap->iv_sta_assoc_resp_ie, vap->iv_sta_assoc_resp_len,
                GFP_ATOMIC);
    }
    qdf_mem_free(vap->iv_sta_assoc_resp_ie);
    vap->iv_sta_assoc_resp_ie = NULL;
    vap->iv_sta_assoc_resp_len = 0;
}

/**
 * ieee80211_cfg80211_disconnected: send disconnect event to cfg80211.
 *   This function sends disconnet event to cfg80211 kernel module.
 *   @dev : pointer to netdevice.
 *   @resoncode : disconnect reasoncode.
 *
 *   returns NONE.
 */
void ieee80211_cfg80211_disconnected(struct net_device *dev, u_int8_t *locally_generated_disconnect, u_int8_t reasoncode)
{
    clear_roam_params(dev);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 2, 0))
    cfg80211_disconnected(dev, reasoncode, NULL,
        0, *locally_generated_disconnect, GFP_ATOMIC);
    *locally_generated_disconnect = 0;
#else
    cfg80211_disconnected(dev, reasoncode, NULL,
        0, GFP_ATOMIC);
#endif
}

/**
 * struct cfg80211_ops - cfg80211_ops
 *
 * @add_virtual_intf: Add virtual interface
 * @del_virtual_intf: Delete virtual interface
 * @change_virtual_intf: Change virtual interface
 * @change_station: Change station
 * @add_beacon: Add beacon in sap mode
 * @del_beacon: Delete beacon in sap mode
 * @set_beacon: Set beacon in sap mode
 * @start_ap: Start ap
 * @change_beacon: Change beacon
 * @stop_ap: Stop ap
 * @change_bss: Change bss
 * @add_key: Add key
 * @get_key: Get key
 * @del_key: Delete key
 * @set_default_key: Set default key
 * @set_channel: Set channel
 * @scan: Scan
 * @connect: Connect
 * @disconnect: Disconnect
 * @join_ibss = Join ibss
 * @leave_ibss = Leave ibss
 * @set_wiphy_params = Set wiphy params
 * @set_tx_power = Set tx power
 * @get_tx_power = get tx power
 * @remain_on_channel = Remain on channel
 * @cancel_remain_on_channel = Cancel remain on channel
 * @mgmt_tx = Tx management frame
 * @mgmt_tx_cancel_wait = Cancel management tx wait
 * @set_default_mgmt_key = Set default management key
 * @set_txq_params = Set tx queue parameters
 * @get_station = Get station
 * @set_power_mgmt = Set power management
 * @del_station = Delete station
 * @add_station = Add station
 * @set_pmksa = Set pmksa
 * @del_pmksa = Delete pmksa
 * @flush_pmksa = Flush pmksa
 * @update_ft_ies = Update FT IEs
 * @tdls_mgmt = Tdls management
 * @tdls_oper = Tdls operation
 * @set_rekey_data = Set rekey data
 * @sched_scan_start = Scheduled scan start
 * @sched_scan_stop = Scheduled scan stop
 * @resume = Resume wlan
 * @suspend = Suspend wlan
 * @set_mac_acl = Set mac acl
 * @testmode_cmd = Test mode command
 * @set_ap_chanwidth = Set AP channel bandwidth
 * @dump_survey = Dump survey
 * @key_mgmt_set_pmk = Set pmk key management
 * @get_channel = get Channel info
 */
static struct cfg80211_ops wlan_cfg80211_ops = {
    .add_virtual_intf = wlan_cfg80211_add_virtual_intf,
    .del_virtual_intf = wlan_cfg80211_del_virtual_intf,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 4, 0))
    .set_beacon = NULL,
#else
    .start_ap = wlan_cfg80211_start_ap,
    .change_beacon = wlan_cfg80211_change_beacon,
    .stop_ap = wlan_cfg80211_stop_ap,
#endif
    .change_bss = wlan_cfg80211_change_bss,
    .add_key = wlan_cfg80211_add_key,
    .get_key = wlan_cfg80211_get_key,
    .del_key = wlan_cfg80211_del_key,
    .set_default_key = wlan_cfg80211_set_default_key,
    .scan = wlan_cfg80211_scan_start,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0))
    .abort_scan = wlan_cfg80211_scan_abort,
#endif
    .connect = wlan_cfg80211_connect,
    .disconnect = wlan_cfg80211_disconnect,
    .join_ibss = NULL,
    .leave_ibss = NULL,
    .set_wiphy_params = wlan_cfg80211_set_wiphy_params,
    .set_tx_power = wlan_cfg80211_set_txpower,
    .get_tx_power = wlan_cfg80211_get_txpower,
    .set_default_mgmt_key = wlan_cfg80211_set_default_mgmt_key,
    .get_station = wlan_cfg80211_get_station,
    .set_power_mgmt = wlan_cfg80211_set_power_mgmt,
    .set_pmksa = NULL,
    .del_pmksa = NULL,
    .flush_pmksa = NULL,
    .update_ft_ies = wlan_cfg80211_update_ft_ies,
    .change_virtual_intf = wlan_cfg80211_change_virtual_intf,
    .probe_client = wlan_cfg80211_probe_client,
    .add_station = wlan_cfg80211_add_station,
    .del_station = wlan_cfg80211_del_station,
    .change_station = wlan_cfg80211_change_station,
    .set_rekey_data = wlan_cfg80211_set_rekey_dataata,
    .sched_scan_start = wlan_cfg80211_sched_scan_start,
    .sched_scan_stop = wlan_cfg80211_sched_scan_stop,
    .set_mac_acl = wlan_cfg80211_set_mac_acl,
#ifdef CONFIG_NL80211_TESTMODE
    /* Callback funtion to receive UTF commands from FTM daemon */
    .testmode_cmd = wlan_cfg80211_testmode,
#endif
    .mgmt_tx_cancel_wait = wlan_cfg80211_mgmt_tx_cancel_wait,
    .mgmt_tx = wlan_cfg80211_mgmt_tx,
    .set_txq_params = wlan_cfg80211_set_txq_params,
    .tdls_mgmt = NULL,
    .remain_on_channel = wlan_cfg80211_remain_on_channel,
    .tdls_oper = NULL,
    .cancel_remain_on_channel = wlan_cfg80211_cancel_remain_on_channel,
    .dump_survey = wlan_hdd_cfg80211_dump_survey,
    .channel_switch = wlan_cfg80211_channel_switch,
    .set_ap_chanwidth = wlan_cfg80211_set_ap_chanwidth,
    .get_channel = wlan_cfg80211_get_channel,
#if ATH_SUPPORT_HS20
    .set_qos_map = wlan_cfg80211_set_qos_map,
#endif
#if UMAC_SUPPORT_WPA3_STA
    .external_auth = wlan_cfg80211_external_auth,
#endif
#ifdef WLAN_SUPPORT_FILS
    .set_fils_aad = wlan_cfg80211_set_fils_aad,
#endif /* WLAN_SUPPORT_FILS */
};

/* This structure contain information what kind of frame are expected in
   TX/RX direction for each kind of interface */
static const struct ieee80211_txrx_stypes
wlan_cfg80211_txrx_stypes[NUM_NL80211_IFTYPES] = {
    [NL80211_IFTYPE_STATION] = {
        .tx = 0xffff,
        .rx = 0xffff,
    },
    [NL80211_IFTYPE_AP] = {
        .tx = 0xffff,
        .rx = 0xffff,
    },
    [NL80211_IFTYPE_MONITOR] = {
        .tx = 0xffff,
        .rx = 0xffff,
    },
};

static const struct ieee80211_iface_limit wlan_cfg80211_if_limits[] = {
    {
        .max    = 17,
        .types  = BIT(NL80211_IFTYPE_AP)
    },
    {
        .max    = 30, /*  30 sta vaps is needed in max client setup
                          in qwrap mode */
        .types  = BIT(NL80211_IFTYPE_STATION)
    },
    {
        .max    = 1,
        .types  = BIT(NL80211_IFTYPE_MONITOR)
    },
};

static const struct ieee80211_iface_combination wlan_cfg80211_iface_combination[] = {
    {
        .limits = wlan_cfg80211_if_limits,
        .n_limits = ARRAY_SIZE(wlan_cfg80211_if_limits),
        .max_interfaces = 17,
        .num_different_channels = 1,
        .beacon_int_infra_match = true,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 9, 0))
        .radar_detect_widths =  BIT(NL80211_CHAN_WIDTH_20_NOHT) |
            BIT(NL80211_CHAN_WIDTH_20) |
            BIT(NL80211_CHAN_WIDTH_40) |
            BIT(NL80211_CHAN_WIDTH_80),
#endif
    },
};

/*
 * cipher suite selectors
 * include/linux/ieee80211.h
 */
static const u32 wlan_cfg80211_cipher_suites[] = {
    WLAN_CIPHER_SUITE_USE_GROUP,
    WLAN_CIPHER_SUITE_WEP40,
    WLAN_CIPHER_SUITE_TKIP,
    WLAN_CIPHER_SUITE_CCMP,
    WLAN_CIPHER_SUITE_WEP104,
    WLAN_CIPHER_SUITE_AES_CMAC,
    WLAN_CIPHER_SUITE_GCMP,
    WLAN_CIPHER_SUITE_GCMP_256,
    WLAN_CIPHER_SUITE_CCMP_256,
    WLAN_CIPHER_SUITE_BIP_GMAC_128,
    WLAN_CIPHER_SUITE_BIP_GMAC_256,
    WLAN_CIPHER_SUITE_BIP_CMAC_256,
};

/* wlan_cfg80211_dbgreq - cfg80211 dbgreq handler
 * @wiphy:   pointer to wireless wiphy structure.
 * @wdev:    pointer to wireless_dev structure.
 * @data:    Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed 1895
 *
 * Return: 0 on success, negative errno on failure
 */

static int
wlan_cfg80211_dbgreq(struct wiphy *wiphy, struct wireless_dev *wdev, const void *data, int data_len)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    struct net_device *dev = NULL;
    osif_dev *osifp = NULL;
    wlan_if_t vap = NULL;
    int err = 0;

    struct ieee80211req_athdbg *req = (struct ieee80211req_athdbg *)data;
    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    if (sizeof(struct ieee80211req_athdbg) > data_len) {
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_IOCTL,
                             "%s: Insufficient buffer size\n", __func__);
        return -EINVAL;
    }

    if (ic->ic_wdev.netdev == wdev->netdev) {
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_IOCTL,
                "%s: Radio Command\n", __func__);
        return -EINVAL;
    }

    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_IOCTL,
                         "%s: VAP Command\n", __func__);
    dev = wdev->netdev;
    osifp = ath_netdev_priv(dev);
    vap = osifp->os_if;

    if ((dev != NULL) && (req != NULL)){
        err = ieee80211_ucfg_handle_dbgreq(dev, req, NULL, data_len);
        if (!err && req->needs_reply) {
            err = cfg80211_reply_command(wiphy, data_len, req, 0);
        }
    }

    return err;
}

static const struct nla_policy
wlan_cfg80211_set_wificonfiguration_policy[QCA_WLAN_VENDOR_ATTR_CONFIG_MAX + 1] = {

    [QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_COMMAND] = {.type = NLA_U32 },
    [QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_VALUE] = {.type = NLA_U32 },
    [QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_DATA] = {.type = NLA_BINARY,
                                                  .len = 5000 },
    [QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_LENGTH] = {.type = NLA_U32 },
    [QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_FLAGS] = {.type = NLA_U32 },
};

/**
 * wlan_cfg80211_acs_report_channel - send selected channel to hostapd
 *                                      through vendor event.
 * @vap                 : pointer to vap
 * @channel             : pointer to channel
 *
 */
void wlan_cfg80211_generic_event(struct ieee80211com *ic, int cmdid,
        void *buffer, uint32_t buffer_len,
        struct net_device *vap_dev, gfp_t gfp)
{
    struct sk_buff *vendor_event;
    struct net_device *dev;
    int ret_val;
    struct nlattr *nla;
    uint32_t tlv_buffer_length;

    if (ic  == NULL) {
        printk("%s: Invalid param: ic is null.\n",__func__);
        return;
    }

    dev = ic->ic_netdev;

    tlv_buffer_length =
        NLA_HDRLEN + sizeof(uint32_t) + /* IF-Index */
        NLA_HDRLEN + /* NEST-Start TLV */
            NLA_HDRLEN + sizeof(uint32_t) + /* sub-Command-ID */
            NLA_HDRLEN + buffer_len + 4; /* Command data + 4 byte align */

#define VENDOR_CMD_CB_ASSIGN(a, b) ((void **)a->cb)[2] = b

    vendor_event =
        cfg80211_vendor_event_alloc(ic->ic_wiphy, dev->ieee80211_ptr,
                NLMSG_HDRLEN + tlv_buffer_length,
                QCA_NL80211_VENDOR_SUBCMD_GET_WIFI_CONFIGURATION_INDEX,
                GFP_KERNEL);

    if (!vendor_event) {
        QDF_TRACE(QDF_MODULE_ID_DEBUG, QDF_TRACE_LEVEL_ERROR,
                "cfg80211_vendor_event_alloc failed\n");
        return;
    }

    nla_nest_cancel(vendor_event, ((void **)vendor_event->cb)[2]);
    ret_val = nla_put_u32(vendor_event,
            NL80211_ATTR_IFINDEX,
            vap_dev->ifindex);
    if (ret_val) {
        QDF_TRACE(QDF_MODULE_ID_DEBUG, QDF_TRACE_LEVEL_ERROR,
                "NL80211_ATTR_IFINDEX put fail\n");
        goto error_cleanup;
    }

    nla = nla_nest_start(vendor_event, NL80211_ATTR_VENDOR_DATA);

    if(nla == NULL){
        QDF_TRACE(QDF_MODULE_ID_DEBUG, QDF_TRACE_LEVEL_ERROR,
                "nla_nest_start fail nla is NULL \n");
        goto error_cleanup;
    }

    VENDOR_CMD_CB_ASSIGN(vendor_event, nla);

    if (nla_put_u32(vendor_event, QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_COMMAND,
                cmdid)) {
        QDF_TRACE(QDF_MODULE_ID_DEBUG, QDF_TRACE_LEVEL_ERROR,
                "%s:%d nla put fail", __func__, __LINE__);
        goto error_cleanup;
    }

    if (nla_put(vendor_event, QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_DATA,
                buffer_len, buffer)) {
        QDF_TRACE(QDF_MODULE_ID_DEBUG, QDF_TRACE_LEVEL_ERROR,
                "%s:%d nla put fail", __func__, __LINE__);
        goto error_cleanup;
    }
    nla_nest_end(vendor_event, nla);

    cfg80211_vendor_event(vendor_event, gfp);
    return;
error_cleanup:
    kfree_skb(vendor_event);
}
EXPORT_SYMBOL(wlan_cfg80211_generic_event);


void wlan_cfg80211_acs_report_channel(wlan_if_t vap,
                                  struct ieee80211_ath_channel *channel)
{
    struct sk_buff *vendor_event;
    int ret_val;
    u_int8_t hw_mode = 0;
    u_int16_t chwidth = 0;
    struct ieee80211com *ic = vap->iv_ic;
    osif_dev *osif = (osif_dev *)wlan_vap_get_registered_handle(vap);
    struct net_device *dev = osif->netdev;
    u_int8_t channel_ieee_se = ieee80211_chan2ieee(ic, channel);
    u_int8_t sec_channel = 0;
    u_int8_t primary_channel = 0;
    int8_t pri_center_ch_diff = 0;
    struct nlattr *nla;
    int mode = 0;
    u_int8_t vht_seg1 = 0, vht_seg0 = 0;

#define VENDOR_CMD_CB_ASSIGN(a, b) ((void **)a->cb)[2] = b

    vendor_event =
        cfg80211_vendor_event_alloc(ic->ic_wiphy, dev->ieee80211_ptr,
                4 * sizeof(u8) + 1 * sizeof(u16) + 4 + NLMSG_HDRLEN,
                QCA_NL80211_VENDOR_SUBCMD_DO_ACS_INDEX,
                GFP_KERNEL);
    if (!vendor_event) {
        qdf_print( "cfg80211_vendor_event_alloc failed");
        return;
    }

    nla_nest_cancel(vendor_event, ((void **)vendor_event->cb)[2]);
    /* Send the IF INDEX to differentiate the ACS event for each interface
     * TODO: To be update once cfg80211 APIs are updated to accept if_index
     */
    ret_val = nla_put_u32(vendor_event,
            NL80211_ATTR_IFINDEX,
            dev->ifindex);
    if (ret_val) {
        qdf_print("NL80211_ATTR_IFINDEX put fail");
        kfree_skb(vendor_event);
        return;
    }
    nla = nla_nest_start(vendor_event, NL80211_ATTR_VENDOR_DATA);
    if(nla == NULL){
        qdf_print("nla_nest_start fail nla is NULL ");
        kfree_skb(vendor_event);
        return;
    }
    VENDOR_CMD_CB_ASSIGN(vendor_event, nla);

    if((channel != NULL) && (channel != IEEE80211_CHAN_ANYC)){
        primary_channel = channel->ic_ieee;
    }
    ret_val = nla_put_u8(vendor_event,
            QCA_WLAN_VENDOR_ATTR_ACS_PRIMARY_CHANNEL,
            primary_channel);

    if (ret_val) {
        qdf_print("QCA_WLAN_VENDOR_ATTR_ACS_PRIMARY_CHANNEL put fail");
        kfree_skb(vendor_event);
        return;
    }

    if(primary_channel){
        /* Parameters to derive secondary channels */
#define SEC_40_LOWER -6
#define SEC_40_UPPER -2
#define SEC_80_1 2
#define SEC_80_4 14

        switch(ieee80211_chan2mode(channel)) {
            /*secondary channel for VHT40MINUS and NAHT40MINUS is same */
            case IEEE80211_MODE_11NA_HT40PLUS:
            case IEEE80211_MODE_11AC_VHT40PLUS:
            case IEEE80211_MODE_11NG_HT40PLUS:
                sec_channel = channel_ieee_se + 4;
                break;
            case IEEE80211_MODE_11NG_HT40MINUS:
            case IEEE80211_MODE_11NA_HT40MINUS:
            case IEEE80211_MODE_11AC_VHT40MINUS:
                sec_channel = channel_ieee_se - 4;
                break;
            case IEEE80211_MODE_11AC_VHT80:
            case IEEE80211_MODE_11AC_VHT80_80:
                pri_center_ch_diff = channel_ieee_se - channel->ic_vhtop_ch_freq_seg1;

                if(pri_center_ch_diff > 0) {
                    sec_channel = channel->ic_vhtop_ch_freq_seg1 + SEC_40_LOWER;
                } else {
                    sec_channel = channel->ic_vhtop_ch_freq_seg1 - SEC_40_UPPER;
                }
                break;
            case IEEE80211_MODE_11AC_VHT160:
                pri_center_ch_diff = channel_ieee_se - channel->ic_vhtop_ch_freq_seg1;

                if(pri_center_ch_diff > 0) {
                    sec_channel = channel->ic_vhtop_ch_freq_seg1 - SEC_80_4;
                } else {
                    sec_channel = channel->ic_vhtop_ch_freq_seg1 + SEC_80_1;
                }
                break;
            default:
                break;
        }
    }

    /* must report secondary channel always, 0 is harmless */
    ret_val = nla_put_u8(vendor_event,
            QCA_WLAN_VENDOR_ATTR_ACS_SECONDARY_CHANNEL, sec_channel);
    if (ret_val) {
        qdf_print("QCA_WLAN_VENDOR_ATTR_ACS_SECONDARY_CHANNEL put fail");
        kfree_skb(vendor_event);
        return;
    }

    if(primary_channel){
        mode = wlan_get_desired_phymode(vap);
        if(IEEE80211_IS_CHAN_5GHZ(channel))
            hw_mode = QCA_ACS_MODE_IEEE80211A;
        else if(IEEE80211_IS_CHAN_2GHZ(channel))
            hw_mode = QCA_ACS_MODE_IEEE80211G;
        else if(IEEE80211_IS_CHAN_B(channel))
            hw_mode = QCA_ACS_MODE_IEEE80211B;

        ret_val = nla_put_u8(vendor_event,
                QCA_WLAN_VENDOR_ATTR_ACS_HW_MODE, hw_mode);
        if (ret_val) {
            qdf_print("QCA_WLAN_VENDOR_ATTR_ACS_HW_MODE put fail");
            kfree_skb(vendor_event);
            return;
        }
        chwidth = ieee80211_get_chan_width(channel);

        ret_val = nla_put_u8(vendor_event,
                QCA_WLAN_VENDOR_ATTR_ACS_CHWIDTH, chwidth);
        if (ret_val) {
            qdf_print("QCA_WLAN_VENDOR_ATTR_ACS_CHWIDTH put fail");
            kfree_skb(vendor_event);
            return;
        }
        /*
         * Note:
         * Driver adheres to 802.11-2016 wherein seg2 should contain center of entire 160 Mhz span
         * and seg1 contains center of primary 80 Mhz.
         * However hostapd currently adheres to the older 802.11ac-2013 wherein seg1 contains center
         * of entire 160Mhz span and seg2 contains zero.
         */

        if (mode == IEEE80211_MODE_11AC_VHT160) {
            vht_seg0 = channel->ic_vhtop_ch_freq_seg2;
        } else {
            vht_seg0 = channel->ic_vhtop_ch_freq_seg1;
        }
        ret_val = nla_put_u8(vendor_event,
                QCA_WLAN_VENDOR_ATTR_ACS_VHT_SEG0_CENTER_CHANNEL, vht_seg0);

        if (ret_val) {
            qdf_print("QCA_WLAN_VENDOR_ATTR_ACS_VHT_SEG0_CENTER_CHANNEL put fail");
            kfree_skb(vendor_event);
            return;
        }

        if (mode == IEEE80211_MODE_11AC_VHT80_80) {
            vht_seg1 = channel->ic_vhtop_ch_freq_seg2;
        } else {
            vht_seg1 = 0;
        }
        ret_val = nla_put_u8(vendor_event,
                QCA_WLAN_VENDOR_ATTR_ACS_VHT_SEG1_CENTER_CHANNEL, vht_seg1);
        if (ret_val) {
            qdf_print("QCA_WLAN_VENDOR_ATTR_ACS_VHT_SEG1_CENTER_CHANNEL put fail");
            kfree_skb(vendor_event);
            return;
        }
    }

    if (!channel){
       qdf_print("%s: Returning due to null channel.", __func__);
       return;
    }

    qdf_nofl_info("vap-%d(%s): ACS result PCH %d, SCH %d, hw_mode %d chwidth %d, vht_seg0 %d, vht_seg1 %d ",
                  vap->iv_unit, vap->iv_netdev_name,
                  channel->ic_ieee, sec_channel, hw_mode, chwidth,
                  vht_seg0, vht_seg1);

    vap->iv_des_cfreq2 = vht_seg1;
    vap->iv_cfg80211_channel = channel->ic_ieee;
    cfg80211_vendor_event(vendor_event, GFP_KERNEL);
}

/* Short name for QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_CHAN_INFO event */
#define CHAN_INFO_ATTR_FLAGS \
    QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_FLAGS
#define CHAN_INFO_ATTR_FLAG_EXT \
    QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_FLAG_EXT
#define CHAN_INFO_ATTR_FREQ \
    QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_FREQ
#define CHAN_INFO_ATTR_MAX_REG_POWER \
    QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_MAX_REG_POWER
#define CHAN_INFO_ATTR_MAX_POWER \
    QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_MAX_POWER
#define CHAN_INFO_ATTR_MIN_POWER \
    QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_MIN_POWER
#define CHAN_INFO_ATTR_REG_CLASS_ID \
    QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_REG_CLASS_ID
#define CHAN_INFO_ATTR_ANTENNA_GAIN \
    QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_ANTENNA_GAIN
#define CHAN_INFO_ATTR_VHT_SEG_0 \
    QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_VHT_SEG_0
#define CHAN_INFO_ATTR_VHT_SEG_1 \
    QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_VHT_SEG_1
#define CHAN_INFO_ATTR_FLAGS_2 \
    QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_FLAGS_2

/**
 * cfg80211_update_channel_info() - add channel info attributes
 * @skb: pointer to sk buff
 * @vap: pointer to vap structure
 * @idx: attribute index
 * @freq_list: pointer to the channel frequency list array
 * @ccount: number of channels in the freq_list array
 *
 * Return: Success(0) or reason code for failure
 */
int32_t
cfg80211_update_channel_info(struct sk_buff *skb,
                             wlan_if_t vap, int idx, uint32_t *freq_list, uint32_t *ccount)
{
    struct nlattr *nla_attr;
    struct nlattr *channel;
    struct ieee80211_ath_channel *icv = NULL;
    int i;
    int status = 0;
    struct ieee80211req_chaninfo *chanlist;
    int nchans_max = ((IEEE80211_CHANINFO_MAX - 1) * sizeof(__u32))/
        sizeof(struct ieee80211_ath_channel);

    chanlist = (struct ieee80211req_chaninfo *)qdf_mem_malloc(sizeof(*chanlist));
    if (NULL == chanlist) {
        qdf_print("Not enough memmory to form channel list %s",__func__);
        status = -ENOMEM;
        goto fail;
    }

    wlan_get_chaninfo(vap, 0, chanlist->ic_chans, &chanlist->ic_nchans);

    if (chanlist->ic_nchans > nchans_max) {
        chanlist->ic_nchans = nchans_max;
    }

    *ccount = chanlist->ic_nchans;

    nla_attr = nla_nest_start(skb, idx);

    if (!nla_attr) {
        status = -EINVAL;
        goto fail;
    }

    for (i = 0; i < chanlist->ic_nchans; i++) {
        channel = nla_nest_start(skb, i);

        if (!channel) {
            status = -EINVAL;
            goto fail;
        }

        icv = &chanlist->ic_chans[i];
        if (!icv) {
            qdf_print("channel info not found %s",__func__);
            status = -EINVAL;
            goto fail;
        }

        freq_list[i] = icv->ic_freq;

        if (nla_put_u16(skb, CHAN_INFO_ATTR_FREQ,
                icv->ic_freq) ||
            nla_put_u32(skb, CHAN_INFO_ATTR_FLAGS,
                (icv->ic_flags & 0xFFFFFFFF)) ||
            nla_put_u32(skb, CHAN_INFO_ATTR_FLAG_EXT,
                icv->ic_flagext) ||
            nla_put_u8(skb, CHAN_INFO_ATTR_MAX_REG_POWER,
                icv->ic_maxregpower) ||
            nla_put_u8(skb, CHAN_INFO_ATTR_MAX_POWER,
                icv->ic_maxpower) ||
            nla_put_u8(skb, CHAN_INFO_ATTR_MIN_POWER,
                icv->ic_minpower) ||
            nla_put_u8(skb, CHAN_INFO_ATTR_REG_CLASS_ID,
                icv->ic_regClassId) ||
            nla_put_u8(skb, CHAN_INFO_ATTR_ANTENNA_GAIN,
                icv->ic_antennamax) ||
            nla_put_u8(skb, CHAN_INFO_ATTR_VHT_SEG_0,
                icv->ic_vhtop_ch_freq_seg1) ||
            nla_put_u8(skb, CHAN_INFO_ATTR_VHT_SEG_1,
                icv->ic_vhtop_ch_freq_seg2) ||
            nla_put_u32(skb, CHAN_INFO_ATTR_FLAGS_2,
                ((icv->ic_flags & 0xFFFFFFFF00000000) >> 32))) {
            qdf_print("put fail");
            status = -EINVAL;
            goto fail;
                }
        nla_nest_end(skb, channel);
    }
    nla_nest_end(skb, nla_attr);
fail:
    if (chanlist) {
        qdf_mem_free(chanlist);
    }
    return status;
}

#undef CHAN_INFO_ATTR_FLAGS
#undef CHAN_INFO_ATTR_FLAG_EXT
#undef CHAN_INFO_ATTR_FREQ
#undef CHAN_INFO_ATTR_MAX_REG_POWER
#undef CHAN_INFO_ATTR_MAX_POWER
#undef CHAN_INFO_ATTR_MIN_POWER
#undef CHAN_INFO_ATTR_REG_CLASS_ID
#undef CHAN_INFO_ATTR_ANTENNA_GAIN
#undef CHAN_INFO_ATTR_VHT_SEG_0
#undef CHAN_INFO_ATTR_VHT_SEG_1
#undef CHAN_INFO_ATTR_FLAGS_2

int send_acs_event(wlan_if_t vap, cfg80211_hostapd_acs_params *cfg_acs_params,
                    uint8_t reason)
{
    struct sk_buff *skb;
    uint32_t channel_count = 0, status;
    uint32_t freq_list[QDF_MAX_NUM_CHAN] = {0};
    struct ieee80211com *ic = vap->iv_ic;
    osif_dev *osif = (osif_dev *)wlan_vap_get_registered_handle(vap);
    struct net_device *dev = osif->netdev;
    int band = 0;
    int phy_mode = 0;
    int cwidth = 0;

    skb = cfg80211_vendor_event_alloc(ic->ic_wiphy,
                                      dev->ieee80211_ptr,
                                      EXTSCAN_EVENT_BUF_SIZE + NLMSG_HDRLEN,
                                      QCA_NL80211_VENDOR_SUBCMD_UPDATE_EXTERNAL_ACS_CONFIG,
                                      GFP_KERNEL);
    if (!skb) {
        qdf_print("cfg80211_vendor_event_alloc failed");
        return -ENOMEM;
    }

    band = vap->iv_scan_band;
    if (cfg_acs_params != NULL) {
        phy_mode = cfg_acs_params->hw_mode;
        cwidth = cfg_acs_params->ch_width;
    }

    status = cfg80211_update_channel_info(skb, vap,
            QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_CHAN_INFO, freq_list, &channel_count);
    if (status != 0)
        goto fail;

    qdf_print("external acs event params reason:%d spectral:1 offload:1"
              " chan stats:1 channel width:%d", reason, cwidth);
    qdf_print("vapaddr:%x-%x-%x-%x-%x-%x",vap->iv_myaddr[0], vap->iv_myaddr[1], vap->iv_myaddr[2],
              vap->iv_myaddr[3], vap->iv_myaddr[4], vap->iv_myaddr[5]);
    qdf_print("band:%d phymode:%d", band, phy_mode);
    print_eacs_chanlist(freq_list, channel_count);

    /* Update values in NL buffer */
    if (nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_REASON,
               reason) ||
        nla_put_u8(skb,
        QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_IS_SPECTRAL_SUPPORTED,
               true) ||
        nla_put_u8(skb,
        QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_IS_OFFLOAD_ENABLED,
               true) ||
        nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_ADD_CHAN_STATS_SUPPORT,
               true) ||
        nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_CHAN_WIDTH,
               cwidth) ||
        nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_BAND,
               band) ||
        nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_PHY_MODE,
               phy_mode) ||
        nla_put(skb, QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_FREQ_LIST,
            channel_count * sizeof(uint32_t), freq_list) ||
        nla_put_u16(skb, QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_RROPAVAIL_INFO,
            QCA_WLAN_VENDOR_ATTR_RROPAVAIL_INFO_VSCAN_END)) {
        qdf_print("nla put fail");
        goto fail;
    }
    cfg80211_vendor_event(skb, GFP_KERNEL);
    return 0;
fail:
    qdf_print("sending acs event failed %s",__func__);
    if (skb)
        kfree_skb(skb);
    return -EPERM;
}

/**
 * wlan_cfg80211_do_acs - ACS trigger command from hostapd.
 * @wiphy:   pointer to wireless wiphy structure.
 * @wdev:    pointer to wireless_dev structure.
 * @data:    Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed
 *
 * Return: 0 on success, negative errno on failure
 */
static int wlan_cfg80211_do_acs(struct wiphy *wiphy,
                                struct wireless_dev *wdev,
                                const void *data,
                                int data_len)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    osif_dev *osifp = ath_netdev_priv(wdev->netdev);
    wlan_if_t vap = osifp->os_if;
    cfg80211_hostapd_acs_params cfg_acs_params = {0};
    u_int8_t hw_mode = 0;
    struct nlattr *attr[QCA_WLAN_VENDOR_ATTR_ACS_MAX + 1];
    struct nlattr *nla;
    struct ieee80211_ath_channel *channel = NULL;

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    if( nla_parse(attr, QCA_WLAN_VENDOR_ATTR_ACS_MAX,
                 data, data_len,
                 NULL)){
        qdf_print("%s[%d] nla_parse failed  ", __func__,__LINE__);
        return -EINVAL;
    }

    /* Get channel list and copy to acs structure */
    nla = nla_find(data, data_len, QCA_WLAN_VENDOR_ATTR_ACS_CH_LIST);
    if(nla){
        cfg_acs_params.ch_list_len = nla_len(nla);
        if (cfg_acs_params.ch_list_len){
            cfg_acs_params.ch_list = qdf_mem_malloc(cfg_acs_params.ch_list_len);
            if (cfg_acs_params.ch_list){
                nla_memcpy((void*)cfg_acs_params.ch_list,attr[QCA_WLAN_VENDOR_ATTR_ACS_CH_LIST], cfg_acs_params.ch_list_len);
            }
        }
    }
    if (attr[QCA_WLAN_VENDOR_ATTR_ACS_HW_MODE]) {
        hw_mode = nla_get_u8(
                     attr[QCA_WLAN_VENDOR_ATTR_ACS_HW_MODE]);
    }

    if (attr[QCA_WLAN_VENDOR_ATTR_ACS_HT_ENABLED]) {
        cfg_acs_params.ht_enabled = nla_get_flag(
                    attr[QCA_WLAN_VENDOR_ATTR_ACS_HT_ENABLED]);
    }

    if (attr[QCA_WLAN_VENDOR_ATTR_ACS_HT40_ENABLED]) {
        cfg_acs_params.ht40_enabled = nla_get_flag(
                    attr[QCA_WLAN_VENDOR_ATTR_ACS_HT40_ENABLED]);
    }

    if (attr[QCA_WLAN_VENDOR_ATTR_ACS_VHT_ENABLED]) {
        cfg_acs_params.vht_enabled = nla_get_flag(
                    attr[QCA_WLAN_VENDOR_ATTR_ACS_VHT_ENABLED]);
    }

    if (attr[QCA_WLAN_VENDOR_ATTR_ACS_CHWIDTH]) {
        cfg_acs_params.ch_width = nla_get_u16(
                    attr[QCA_WLAN_VENDOR_ATTR_ACS_CHWIDTH]);
    }

    /* find the hw mode through parameters recevied */
    if(cfg_acs_params.vht_enabled){
       if(cfg_acs_params.ch_width == 160)
            cfg_acs_params.hw_mode = IEEE80211_MODE_11AC_VHT160;
       else if(cfg_acs_params.ch_width == 80)
            cfg_acs_params.hw_mode = IEEE80211_MODE_11AC_VHT80;
       else if(cfg_acs_params.ch_width == 20)
         cfg_acs_params.hw_mode = IEEE80211_MODE_11AC_VHT80;
    } else if(hw_mode == QCA_ACS_MODE_IEEE80211A){
            if(cfg_acs_params.ht40_enabled )
                cfg_acs_params.hw_mode = IEEE80211_MODE_11NA_HT40;
            else if(cfg_acs_params.ht_enabled)
                cfg_acs_params.hw_mode = IEEE80211_MODE_11NA_HT20;
            else
                cfg_acs_params.hw_mode = IEEE80211_MODE_11A;
    } else if (hw_mode == QCA_ACS_MODE_IEEE80211G){
            if(cfg_acs_params.ht40_enabled )
                cfg_acs_params.hw_mode = IEEE80211_MODE_11NG_HT40;
            else if(cfg_acs_params.ht_enabled)
                cfg_acs_params.hw_mode = IEEE80211_MODE_11NG_HT20;
            else
                cfg_acs_params.hw_mode = IEEE80211_MODE_11G;
    } else if (hw_mode == QCA_ACS_MODE_IEEE80211B){
        cfg_acs_params.hw_mode = IEEE80211_MODE_11B;
    } else if (hw_mode == QCA_ACS_MODE_IEEE80211ANY){
        cfg_acs_params.hw_mode = IEEE80211_MODE_AUTO;
    } else {
        qdf_print("%s[%d] Invalid ACS hw mode ", __func__,__LINE__);
        if (cfg_acs_params.ch_list){
            qdf_mem_free((void *)cfg_acs_params.ch_list);
        }
        return -EINVAL;
    }

    cfg_acs_params.hw_mode = wlan_get_desired_phymode(vap);

    qdf_info("vap-%d(%s):ACS Params", vap->iv_unit,vap->iv_netdev_name);
    qdf_info("ht_enabled:%d|ht40_enabled:%d|vht_enabled:%d|hw_mode:%d|chwidth:%d|",
                    cfg_acs_params.ht_enabled, cfg_acs_params.ht40_enabled,
                    cfg_acs_params.vht_enabled, cfg_acs_params.hw_mode, cfg_acs_params.ch_width);

    if (ieee80211_vap_ext_ifu_acs_is_set(vap)) {
        /* set the deired mode for vap */
        vap->iv_des_mode = cfg_acs_params.hw_mode;
        /* If any VAP is active, the channel is already selected
         * so go ahead and init the VAP, the same channel will be used */
        if (ieee80211_vaps_active(vap->iv_ic)) {
            /* Set the curent channel to the VAP */
            wlan_chan_t sel_chan;
            int sel_ieee_chan = 0;
            int error;
            sel_chan = wlan_get_current_channel(vap, true);
            sel_ieee_chan = wlan_channel_ieee(sel_chan);
            error = wlan_set_channel(vap, sel_ieee_chan, sel_chan->ic_vhtop_ch_freq_seg2);
            qdf_print("wlan_set_channel error code: %d", error);
            if (error !=0) {
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_ACS,
                                  "%s : failed to set channel with error code %d\n",
                                  __func__, error);
                qdf_print("wlan_set_channel failed. error code : %d", error);
                if (cfg_acs_params.ch_list){
                    qdf_mem_free((void *)cfg_acs_params.ch_list);
                }
                 return -EINVAL;
            } else {
                qdf_print("Inheriting the channel from the active vap");
            }
        } else {
            int ret = 0;
            /* No VAPs active */
            /* An external entity is responsible for
               Auto Channel Selection at VAP init
               time */
            ret = send_acs_event(vap, &cfg_acs_params, QCA_WLAN_VENDOR_ACS_SELECT_REASON_INIT);
            if (ret == 0) {
                qdf_print("External channel selection triggered");
            } else {
                qdf_print("sending acs event failed %s",__func__);
                if (cfg_acs_params.ch_list){
                    qdf_mem_free((void *)cfg_acs_params.ch_list);
                }
                 return -EINVAL;
            }
        }
    } else {
        channel = vap->iv_des_chan[vap->iv_des_mode];
        if ((!channel) || (channel == IEEE80211_CHAN_ANYC)) {
            wlan_cfg80211_start_acs_scan(vap, &cfg_acs_params);
        } else {
            wlan_cfg80211_acs_report_channel(vap, channel);
        }
    }

    if (cfg_acs_params.ch_list){
        qdf_mem_free((void *)cfg_acs_params.ch_list);
    }

    return 0;
}

/* extract generic command params & send back them to caller */
static int extract_generic_command_params(struct wiphy *wiphy,
        const void *data, int data_len,
        struct wlan_cfg8011_genric_params *params)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    struct nlattr *attr[QCA_WLAN_VENDOR_ATTR_CONFIG_MAX + 1];

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    if ((data == NULL) || (!data_len)) {
        qdf_print("%s: invalid data length data ptr: %pK ", __func__, data);
        return -1;
    }

    if (nla_parse(attr, QCA_WLAN_VENDOR_ATTR_CONFIG_MAX,
                data, data_len,
                wlan_cfg80211_set_wificonfiguration_policy)) {
        return -EINVAL;
    }

    if (attr[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_COMMAND]) {
        params->command = nla_get_u32(
                attr[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_COMMAND]);
    }

    if (attr[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_VALUE]) {
        params->value = nla_get_u32(
                attr[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_VALUE]);
    }

    if (attr[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_DATA]) {
        params->data = nla_data(
                attr[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_DATA]);
        params->data_len = nla_len(attr[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_DATA]);
    }

    if (attr[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_LENGTH]) {
        params->length = nla_get_u32(
                attr[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_LENGTH]);
    }

    if (attr[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_FLAGS]) {
        params->flags = nla_get_u32(
                attr[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_FLAGS]);
    }

    return 0;
}

void wlan_cfg80211_dfs_cac_start(wlan_if_t vap, uint32_t timeout)
{
    struct sk_buff *vendor_event;
    int ret_val;
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211_ath_channel *channel = ic->ic_curchan;
    osif_dev *osif = (osif_dev *)wlan_vap_get_registered_handle(vap);
    struct net_device *dev = osif->netdev;
    struct nlattr *nla;
    uint32_t ch_width;

#define VENDOR_CMD_CB_ASSIGN(a, b) ((void **)a->cb)[2] = b

    vendor_event =
        cfg80211_vendor_event_alloc(ic->ic_wiphy, NULL,
                6 * sizeof(uint32_t) + NLMSG_HDRLEN,
                QCA_NL80211_VENDOR_SUBCMD_DFS_OFFLOAD_CAC_STARTED_INDEX,
                GFP_KERNEL);
    if (!vendor_event) {
        QDF_TRACE(QDF_MODULE_ID_DEBUG, QDF_TRACE_LEVEL_DEBUG,
                "cfg80211_vendor_event_alloc failed");
        return;
    }

    nla_nest_cancel(vendor_event, ((void **)vendor_event->cb)[2]);

    ret_val = nla_put_u32(vendor_event,
            NL80211_ATTR_IFINDEX,
            dev->ifindex);
    if (ret_val) {
        QDF_TRACE(QDF_MODULE_ID_DEBUG, QDF_TRACE_LEVEL_DEBUG,
                "NL80211_ATTR_IFINDEX put fail");
        kfree_skb(vendor_event);
        return;
    }
    nla = nla_nest_start(vendor_event, NL80211_ATTR_VENDOR_DATA);
    if(nla == NULL){
        QDF_TRACE(QDF_MODULE_ID_DEBUG, QDF_TRACE_LEVEL_DEBUG,
                "nla_nest_start fail nla is NULL");
        kfree_skb(vendor_event);
        return;
    }
    VENDOR_CMD_CB_ASSIGN(vendor_event, nla);

    ret_val = nla_put_u32(vendor_event, NL80211_ATTR_WIPHY_FREQ,
                channel->ic_freq);
    if (ret_val) {
        QDF_TRACE(QDF_MODULE_ID_DEBUG, QDF_TRACE_LEVEL_DEBUG,
                "%s:%d NL80211_ATTR_WIPHY_FREQ put fail", __func__, __LINE__);
        return;
    }

    ret_val = nla_put_u32(vendor_event, QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_DATA,
                timeout);
    if (ret_val) {
        QDF_TRACE(QDF_MODULE_ID_DEBUG, QDF_TRACE_LEVEL_DEBUG,
                "%s:%d QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_DATA put fail", __func__, __LINE__);
        return;
    }

    if (IEEE80211_IS_CHAN_HT20(channel))
            ret_val = nla_put_u32(vendor_event, NL80211_ATTR_WIPHY_CHANNEL_TYPE,
                        NL80211_CHAN_HT20);
    else if (IEEE80211_IS_CHAN_HT40PLUS(channel))
            ret_val = nla_put_u32(vendor_event, NL80211_ATTR_WIPHY_CHANNEL_TYPE,
                        NL80211_CHAN_HT40PLUS);
    else if (IEEE80211_IS_CHAN_HT40MINUS(channel))
            ret_val = nla_put_u32(vendor_event, NL80211_ATTR_WIPHY_CHANNEL_TYPE,
                        NL80211_CHAN_HT40MINUS);
    else
            ret_val = nla_put_u32(vendor_event, NL80211_ATTR_WIPHY_CHANNEL_TYPE,
                        NL80211_CHAN_NO_HT);

    if (ret_val) {
        QDF_TRACE(QDF_MODULE_ID_DEBUG, QDF_TRACE_LEVEL_DEBUG,
                "%s:%d NL80211_ATTR_WIPHY_CHANNEL_TYPE put fail", __func__, __LINE__);
        return;
    }
    ch_width = ieee80211_get_chan_width(channel);
    ret_val = nla_put_u32(vendor_event, NL80211_ATTR_CHANNEL_WIDTH,
                ch_width);
    if (ret_val) {
        QDF_TRACE(QDF_MODULE_ID_DEBUG, QDF_TRACE_LEVEL_DEBUG,
                "%s:%d NL80211_ATTR_WIPHY_CHANNEL_TYPE put fail", __func__, __LINE__);
        return;
    }

    ret_val = nla_put_u32(vendor_event, NL80211_ATTR_CENTER_FREQ1,
                channel->ic_vhtop_ch_freq_seg1);
    if (ret_val) {
        QDF_TRACE(QDF_MODULE_ID_DEBUG, QDF_TRACE_LEVEL_DEBUG,
                "%s:%d NL80211_ATTR_CENTER_FREQ1 put fail", __func__, __LINE__);
        return;
    }

    ret_val = nla_put_u32(vendor_event, NL80211_ATTR_CENTER_FREQ2,
                channel->ic_vhtop_ch_freq_seg2);
    if (ret_val) {
        QDF_TRACE(QDF_MODULE_ID_DEBUG, QDF_TRACE_LEVEL_DEBUG,
                "%s:%d NL80211_ATTR_CENTER_FREQ2 put fail", __func__, __LINE__);
        return;
    }

    nla_nest_end(vendor_event, nla);
    cfg80211_vendor_event(vendor_event, GFP_KERNEL);
}
EXPORT_SYMBOL(wlan_cfg80211_dfs_cac_start);

static int wlan_cfg80211_addmac(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        struct wlan_cfg8011_genric_params *params)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    wlan_if_t vap = NULL;
    int cmd_type ,ret = 0;
    void *cmd;
    u_int8_t mac[IEEE80211_ADDR_LEN];

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    cmd = extract_command(ic, wdev, &cmd_type);
    if (cmd_type) {
        qdf_print(" Command on invalid interface");
        return -EINVAL;
    } else {
        vap = (wlan_if_t)cmd;
    }

    if (params->data_len == IEEE80211_ADDR_LEN) {
        qdf_mem_copy(mac, params->data, IEEE80211_ADDR_LEN);
    } else {
        qdf_print("Invalid mac address ");
        return -EINVAL;
    }

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_CFG80211, "%s :%s \n", __func__, ether_sprintf(mac));

    ret = wlan_set_acl_add(vap, mac, IEEE80211_ACL_FLAG_ACL_LIST_1);
    return ret;
}

static int wlan_cfg80211_delmac(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        struct wlan_cfg8011_genric_params *params)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    wlan_if_t vap = NULL;
    int cmd_type ,ret = 0;
    void *cmd;
    u_int8_t mac[IEEE80211_ADDR_LEN];

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    cmd = extract_command(ic, wdev, &cmd_type);
    if (cmd_type) {
        qdf_print(" Command on invalid interface");
        return -EINVAL;
    } else {
        vap = (wlan_if_t)cmd;
    }

    if (params->data_len == IEEE80211_ADDR_LEN) {
        qdf_mem_copy(mac, params->data, IEEE80211_ADDR_LEN);
    } else {
        qdf_print("Invalid mac address ");
        return -EINVAL;
    }

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_CFG80211, "%s :%s \n", __func__, ether_sprintf(mac));
    ret = wlan_set_acl_remove(vap, mac, IEEE80211_ACL_FLAG_ACL_LIST_1);
    return ret;
}

#if WLAN_DFS_CHAN_HIDDEN_SSID
static int wlan_cfg80211_conf_bssid(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        struct wlan_cfg8011_genric_params *params)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    wlan_if_t vap = NULL;
    int cmd_type ,ret = 0;
    void *cmd;
    u_int8_t mac[IEEE80211_ADDR_LEN];

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    cmd = extract_command(ic, wdev, &cmd_type);
    if (cmd_type) {
        qdf_print(" Command on invalid interface");
        return -EINVAL;
    } else {
        vap = (wlan_if_t)cmd;
    }

    if (params->data_len == IEEE80211_ADDR_LEN) {
        qdf_mem_copy(mac, params->data, IEEE80211_ADDR_LEN);
    } else {
        qdf_print("Invalid mac address ");
        return -EINVAL;
    }

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_CFG80211, "%s :%s \n", __func__, ether_sprintf(mac));

    ret = ieee80211_set_conf_bssid(vap,mac);

    return ret;
}
static int wlan_cfg80211_get_conf_bssid(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        struct wlan_cfg8011_genric_params *params)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    wlan_if_t vap = NULL;
    osif_dev *osifp = NULL;
    int cmd_type;
    void *cmd;
    u_int8_t mac[IEEE80211_ADDR_LEN];

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    cmd = extract_command(ic, wdev, &cmd_type);
    if (cmd_type) {
        qdf_print(" Command on invalid interface");
        return -EINVAL;
    } else {
        osifp = ath_netdev_priv(wdev->netdev);
        vap = (wlan_if_t)cmd;
    }

    qdf_mem_copy(&mac, vap->iv_conf_des_bssid, IEEE80211_ADDR_LEN);
    cfg80211_reply_command(wiphy, IEEE80211_ADDR_LEN, &mac, 0);

    return 0;
}
#endif /* WLAN_DFS_CHAN_HIDDEN_SSID */

#if ATH_ACL_SOFTBLOCKING
static int wlan_cfg80211_addmac_sec(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        struct wlan_cfg8011_genric_params *params)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    wlan_if_t vap = NULL;
    int cmd_type ,ret = 0;
    void *cmd;
    u_int8_t mac[IEEE80211_ADDR_LEN];

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    cmd = extract_command(ic, wdev, &cmd_type);
    if (cmd_type) {
        qdf_print(" Command on invalid interface");
        return -EINVAL;
    } else {
        vap = (wlan_if_t)cmd;
    }

    if (params->data_len == IEEE80211_ADDR_LEN) {
        qdf_mem_copy(mac, params->data, IEEE80211_ADDR_LEN);
    } else {
        qdf_print("Invalid mac address ");
        return -EINVAL;
    }

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_CFG80211, "%s :%s \n", __func__, ether_sprintf(mac));

    ret = wlan_set_acl_add(vap, mac, IEEE80211_ACL_FLAG_ACL_LIST_2);
    return ret;
}

static int wlan_cfg80211_delmac_sec(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        struct wlan_cfg8011_genric_params *params)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    wlan_if_t vap = NULL;
    int cmd_type ,ret = 0;
    void *cmd;
    u_int8_t mac[IEEE80211_ADDR_LEN];

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    cmd = extract_command(ic, wdev, &cmd_type);
    if (cmd_type) {
        qdf_print(" Command on invalid interface");
        return -EINVAL;
    } else {
        vap = (wlan_if_t)cmd;
    }

    if (params->data_len == IEEE80211_ADDR_LEN) {
        qdf_mem_copy(mac, params->data, IEEE80211_ADDR_LEN);
    } else {
        qdf_print("Invalid mac address ");
        return -EINVAL;
    }

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_CFG80211, "%s :%s \n", __func__, ether_sprintf(mac));
    ret = wlan_set_acl_remove(vap, mac, IEEE80211_ACL_FLAG_ACL_LIST_2);
    return ret;
}

static int wlan_cfg80211_get_aclmac_sec(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        struct wlan_cfg8011_genric_params *params)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    wlan_if_t vap = NULL;
    osif_dev *osifp = NULL;
    int cmd_type;
    void *cmd;
    u_int8_t mac[IEEE80211_ADDR_LEN];
    u_int8_t *mac_list;
    int i, rc, num_mac;

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    cmd = extract_command(ic, wdev, &cmd_type);
    if (cmd_type) {
        qdf_print(" Command on invalid interface");
        return -EINVAL;
    } else {
        osifp = ath_netdev_priv(wdev->netdev);
        vap = (wlan_if_t)cmd;
    }

    /* read and send mac list to application */
    mac_list = (u_int8_t *)OS_MALLOC(osifp->os_handle,
            (IEEE80211_ADDR_LEN * 256), GFP_KERNEL);
    if (!mac_list) {
        return -EFAULT;
    }
    rc = wlan_get_acl_list(vap, mac_list, (IEEE80211_ADDR_LEN * 256),
            &num_mac, IEEE80211_ACL_FLAG_ACL_LIST_2);
    if(rc) {
        OS_FREE(mac_list);
        return -EFAULT;
    }

    for (i = 0; i < num_mac && i < 256; i++) {
        memcpy(&mac, &mac_list[i * IEEE80211_ADDR_LEN], IEEE80211_ADDR_LEN);
        cfg80211_reply_command(wiphy, IEEE80211_ADDR_LEN, &mac, 0);
    }
    OS_FREE(mac_list);
    return 0;
}
#endif

/*
 * wlan_cfg80211_check_11hcap: Checks a connected station for 802.11h
 * spectrum management capability using the concerned station's
 * MAC address as the input value.
 *
 * @wiphy: Pointer to wiphy structure
 * @wdev: Pointer to wireless_dev structure
 * @params: Pointer to the parameters sent from the cfg80211tool command
 *
 * Return: 0 is successful
 */

static int wlan_cfg80211_check_11hcap(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        struct wlan_cfg8011_genric_params *params)
{
        struct cfg80211_context *cfg_ctx = NULL;
        struct ieee80211com *ic = NULL;
        osif_dev *osifp = NULL;
        wlan_if_t vap = NULL;
        int cmd_type;
        void *cmd;
        u_int8_t mac[IEEE80211_ADDR_LEN];
        struct ieee80211_node *ni;
        bool spectrum_management_en;

        cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
        if (cfg_ctx == NULL) {
                qdf_print("Invalid Context");
                return -EINVAL;
        }

        ic = cfg_ctx->ic;
        if (ic == NULL) {
                qdf_print("Invalid Interface");
                return -EINVAL;
        }

        cmd = extract_command(ic, wdev, &cmd_type);
        if (cmd_type) {
                qdf_print("Command on Invalid Interface");
                return -EINVAL;
        }
        vap = (wlan_if_t)cmd;
        osifp = ath_netdev_priv(wdev->netdev);

        if (params->data_len != IEEE80211_ADDR_LEN) {
                qdf_print("Invalid MAC Address");
                return -EINVAL;
        }
        qdf_mem_copy(mac, params->data, IEEE80211_ADDR_LEN);

        ni = ieee80211_vap_find_node(vap, mac);
        if (ni == NULL) {
                qdf_print("MAC Address not found");
                return  -EINVAL;
        }

        spectrum_management_en = (ni->ni_capinfo & IEEE80211_CAPINFO_SPECTRUM_MGMT) ? true : false;
        cfg80211_reply_command(wiphy, sizeof(spectrum_management_en), &spectrum_management_en, 0);
        ieee80211_free_node(ni);

        return 0;
}

/**
 * wlan_cfg80211_set_qdepth_thresh_policy: Sets the nla_policy for the parsing
 * attributes for the command to set MSDUQ depth threshold params
 */
static const struct nla_policy
wlan_cfg80211_set_qdepth_thresh_policy[QCA_WLAN_VENDOR_ATTR_QDEPTH_THRESH_MAX + 1] = {
    [QCA_WLAN_VENDOR_ATTR_QDEPTH_THRESH_MAC_ADDR] = {.type = NLA_BINARY },
    [QCA_WLAN_VENDOR_ATTR_QDEPTH_THRESH_TID] = {.type = NLA_U32 },
    [QCA_WLAN_VENDOR_ATTR_QDEPTH_THRESH_UPDATE_MASK] = {.type = NLA_U32 },
    [QCA_WLAN_VENDOR_ATTR_QDEPTH_THRESH_VALUE] = {.type = NLA_U32 },
};

/**
 * wlan_cfg80211_set_qdepth_thresh_cb: cfg80211 API to set the MSDU
 * queue depth threshold per TID per peer
 * @wiphy: wiphy pointer
 * @wdev: wireless_dev pointer
 * @params: parameters sent from the cfg80211tool command
 */

static int wlan_cfg80211_set_qdepth_thresh_cb(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        const void *data,
        int data_len)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    osif_dev *osifp = NULL;
    wlan_if_t vap = NULL;
    int cmd_type = 0;
    void *cmd = NULL;
    struct nlattr *attr[QCA_WLAN_VENDOR_ATTR_CONFIG_MAX + 1];
    uint8_t *mac_addr = NULL;
    uint32_t tid = 0, update_mask = 0, thresh_val = 0;

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    if (cfg_ctx == NULL) {
        qdf_print("Invalid Context");
        return -EINVAL;
    }

    ic = cfg_ctx->ic;
    if (ic == NULL) {
        qdf_print("Invalid Interface");
        return -EINVAL;
    }

    cmd = extract_command(ic, wdev, &cmd_type);
    if (cmd_type) {
        qdf_print("Command on Invalid Interface");
        return -EINVAL;
    }
    vap = (wlan_if_t)cmd;
    osifp = ath_netdev_priv(wdev->netdev);

    if ((data == NULL) || (!data_len)) {
        qdf_print("%s: Invalid data length data ptr: %pK ", __func__, data);
        return - EINVAL;
    }

    if (nla_parse(attr, QCA_WLAN_VENDOR_ATTR_QDEPTH_THRESH_MAX,
                  data, data_len,
                  wlan_cfg80211_set_qdepth_thresh_policy)) {
        return -EINVAL;
    }

    /* Extracting MAC Address */
    if(attr[QCA_WLAN_VENDOR_ATTR_QDEPTH_THRESH_MAC_ADDR]) {
        mac_addr = nla_data(
                       attr[QCA_WLAN_VENDOR_ATTR_QDEPTH_THRESH_MAC_ADDR]);
    }

    /* Extracting TID */
    if(attr[QCA_WLAN_VENDOR_ATTR_QDEPTH_THRESH_TID]) {
        tid = nla_get_u32(
                  attr[QCA_WLAN_VENDOR_ATTR_QDEPTH_THRESH_TID]);
    }

    /* Extracting Update Mask */
    if(attr[QCA_WLAN_VENDOR_ATTR_QDEPTH_THRESH_UPDATE_MASK]) {
      update_mask = nla_get_u32(
                        attr[QCA_WLAN_VENDOR_ATTR_QDEPTH_THRESH_UPDATE_MASK]);
    }

    /* Extracting Threshold Value */
    if(attr[QCA_WLAN_VENDOR_ATTR_QDEPTH_THRESH_VALUE]) {
        thresh_val = nla_get_u32(
                         attr[QCA_WLAN_VENDOR_ATTR_QDEPTH_THRESH_VALUE]);
    }

    ic->ic_vap_set_qdepth_thresh(ic, vap, mac_addr, tid, update_mask, thresh_val);
    return 0;
}

/*
 * wlan_cfg80211_enable_sounding_int:
 * Enables/Disables TXBF sounding interval
 */
static int wlan_cfg80211_enable_sounding_int(struct wiphy *wiphy,
                struct wireless_dev *wdev,
                struct wlan_cfg8011_genric_params *params)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;

    osif_dev *osifp = NULL;
    wlan_if_t vap = NULL;
    int cmd_type, retval = 0;
    void *cmd;
    uint8_t mac[IEEE80211_ADDR_LEN];
    struct ieee80211_node *ni;
    uint32_t enable;

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    if (cfg_ctx == NULL) {
        qdf_print("Invalid Context");
        return -EINVAL;
    }

    ic = cfg_ctx->ic;
    if (ic == NULL) {
        qdf_print("Invalid Interface");
        return -EINVAL;
    }

    cmd = extract_command(ic, wdev, &cmd_type);
    if (cmd_type) {
        qdf_print("Command on Invalid Interface");
        return -EINVAL;
    }
    vap = (wlan_if_t)cmd;
    osifp = ath_netdev_priv(wdev->netdev);

    if(params->data_len != IEEE80211_ADDR_LEN) {
        qdf_print("Invalid MAC Address!");
        return -EINVAL;
    }
    qdf_mem_copy(mac, params->data, IEEE80211_ADDR_LEN);

    ni = ieee80211_vap_find_node(vap, mac);
    if (ni == NULL) {
        qdf_print("MAC Address Not Found!");
        return -EINVAL;
    }

    enable = !!params->value;

    retval = ic->ic_node_enable_sounding_int(ni, enable);
    ieee80211_free_node(ni);

    return retval;

}

/*
 * wlan_cfg80211_set_su_sounding_int:
 * Sets the single-user txbf sounding interval
 */
static int wlan_cfg80211_set_su_sounding_int(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        struct wlan_cfg8011_genric_params *params)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;

    osif_dev *osifp = NULL;
    wlan_if_t vap = NULL;
    int cmd_type, retval = 0;
    void *cmd;
    uint8_t mac[IEEE80211_ADDR_LEN];
    struct ieee80211_node *ni;

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    if (cfg_ctx == NULL) {
        qdf_print("Invalid Context");
        return -EINVAL;
    }

    ic = cfg_ctx->ic;
    if (ic == NULL) {
        qdf_print("Invalid Interface");
        return -EINVAL;
    }

    cmd = extract_command(ic, wdev, &cmd_type);
    if (cmd_type) {
        qdf_print("Command on Invalid Interface");
        return -EINVAL;
    }
    vap = (wlan_if_t)cmd;
    osifp = ath_netdev_priv(wdev->netdev);

    if(params->data_len != IEEE80211_ADDR_LEN) {
        qdf_print("Invalid MAC Address!");
        return -EINVAL;
    }
    qdf_mem_copy(mac, params->data, IEEE80211_ADDR_LEN);

    ni = ieee80211_vap_find_node(vap, mac);
    if (ni == NULL) {
        qdf_print("MAC Address Not Found!");
        return -EINVAL;
    }

    retval = ic->ic_node_set_su_sounding_int(ni, params->value);
    ieee80211_free_node(ni);

    return retval;

}

/*
 * wlan_cfg80211_set_mu_sounding_int:
 * Sets the multi-user TXBF sounding interval
 */
static int wlan_cfg80211_set_mu_sounding_int(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        struct wlan_cfg8011_genric_params *params)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;

    osif_dev *osifp = NULL;
    wlan_if_t vap = NULL;
    int cmd_type, retval = 0;
    void *cmd;
    uint8_t mac[IEEE80211_ADDR_LEN];
    struct ieee80211_node *ni;

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    if (cfg_ctx == NULL) {
        qdf_print("Invalid Context");
        return -EINVAL;
    }

    ic = cfg_ctx->ic;
    if (ic == NULL) {
        qdf_print("Invalid Interface");
        return -EINVAL;
    }

    cmd = extract_command(ic, wdev, &cmd_type);
    if (cmd_type) {
        qdf_print("Command on Invalid Interface");
        return -EINVAL;
    }
    vap = (wlan_if_t)cmd;
    osifp = ath_netdev_priv(wdev->netdev);

    if(params->data_len != IEEE80211_ADDR_LEN) {
        qdf_print("Invalid MAC Address!");
        return -EINVAL;
    }
    qdf_mem_copy(mac, params->data, IEEE80211_ADDR_LEN);

    ni = ieee80211_vap_find_node(vap, mac);
    if (ni == NULL) {
        qdf_print("MAC Address Not Found!");
        return -EINVAL;
    }

    retval = ic->ic_node_set_mu_sounding_int(ni, params->value);
    ieee80211_free_node(ni);

    return retval;

}

/*
 * wlan_cfg80211_sched_mu_enable:
 * Enables/Disables the scheduler API and stats for MU
 */
static int wlan_cfg80211_sched_mu_enable(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        struct wlan_cfg8011_genric_params *params)

{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;

    osif_dev *osifp = NULL;
    wlan_if_t vap = NULL;
    int cmd_type, retval = 0;
    void *cmd;
    u_int8_t mac[IEEE80211_ADDR_LEN];
    struct ieee80211_node *ni;
    uint32_t enable;

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    if (cfg_ctx == NULL) {
        qdf_print("Invalid Context");
        return -EINVAL;
    }

    ic = cfg_ctx->ic;
    if (ic == NULL) {
        qdf_print("Invalid Interface");
        return -EINVAL;
    }

    cmd = extract_command(ic, wdev, &cmd_type);
    if (cmd_type) {
        qdf_print("Command on Invalid Interface");
        return -EINVAL;
    }
    vap = (wlan_if_t)cmd;
    osifp = ath_netdev_priv(wdev->netdev);

    if (params->data_len != IEEE80211_ADDR_LEN) {
        qdf_print("Invalid MAC Address");
        return -EINVAL;
    }
    qdf_mem_copy(mac, params->data, IEEE80211_ADDR_LEN);

    ni = ieee80211_vap_find_node(vap, mac);
    if (ni == NULL) {
        qdf_print("MAC Address not found");
        return  -EINVAL;
    }

    enable = !!params->value;

    retval = ic->ic_node_sched_mu_enable(ni, enable);
    ieee80211_free_node(ni);

    return retval;
}

/*
 * wlan_cfg80211_sched_ofdma_enable:
 * Enables/Disables the scheduler API and stats for ofdma
 */
static int wlan_cfg80211_sched_ofdma_enable(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        struct wlan_cfg8011_genric_params *params)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;

    osif_dev *osifp = NULL;
    wlan_if_t vap = NULL;
    int cmd_type, retval = 0;
    void *cmd;
    u_int8_t mac[IEEE80211_ADDR_LEN];
    struct ieee80211_node *ni;
    uint32_t enable;

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    if (cfg_ctx == NULL) {
        qdf_print("Invalid Context");
        return -EINVAL;
    }

    ic = cfg_ctx->ic;
    if (ic == NULL) {
        qdf_print("Invalid Interface");
        return -EINVAL;
    }

    cmd = extract_command(ic, wdev, &cmd_type);
    if (cmd_type) {
        qdf_print("Command on Invalid Interface");
        return -EINVAL;
    }
    vap = (wlan_if_t)cmd;
    osifp = ath_netdev_priv(wdev->netdev);

    if (params->data_len != IEEE80211_ADDR_LEN) {
        qdf_print("Invalid MAC Address");
        return -EINVAL;
    }
    qdf_mem_copy(mac, params->data, IEEE80211_ADDR_LEN);

    ni = ieee80211_vap_find_node(vap, mac);
    if (ni == NULL) {
        qdf_print("MAC Address not found");
        return  -EINVAL;
    }

    enable = !!params->value;

    retval = ic->ic_node_sched_ofdma_enable(ni, enable);
    ieee80211_free_node(ni);

    return retval;
}

static int wlan_cfg80211_get_he_bss_color(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        struct wlan_cfg8011_genric_params *params)
{
#define HE_BSS_COLOR_STRING 20
#define HE_BSSCOLOR_ARGS 2
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    struct ol_ath_softc_net80211 *scn = NULL;
    int cmd_type ,ret = 0;
    void *cmd;
    int val[HE_BSSCOLOR_ARGS] = {0};
    char string[HE_BSS_COLOR_STRING];

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    cmd = extract_command(ic, wdev, &cmd_type);

    if (!cmd_type) {
        qdf_print(" Command on Invalid interface");
        return -EINVAL;
    } else {
        scn = (struct ol_ath_softc_net80211 *)cmd;
    }

    if ( ic->ic_cfg80211_radio_handler.get_he_bss_color ) {
        ret = ic->ic_cfg80211_radio_handler.get_he_bss_color((void *)scn, val);
    }
    else {
        ret = -EOPNOTSUPP;
    }

    snprintf(string, HE_BSS_COLOR_STRING, "%d Override: %d", val[0], val[1]);

    cfg80211_reply_command(wiphy, sizeof(string), string, 0);

    return ret;
}

static int wlan_cfg80211_he_bss_color(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        struct wlan_cfg8011_genric_params *params)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    struct ol_ath_softc_net80211 *scn = NULL;
    int cmd_type ,ret = 0;
    void *cmd;
    uint8_t value, ovrride;
    uint8_t *data = (uint8_t *)params->data;

    if(data == NULL) {
        qdf_print("Invalid Arguments ");
        return -EINVAL;
    }

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    cmd = extract_command(ic, wdev, &cmd_type);

    if (!cmd_type) {
        qdf_print(" Command on Invalid interface");
        return -EINVAL;
    } else {
        scn = (struct ol_ath_softc_net80211 *)cmd;
    }

    value = (uint8_t)params->value;
    ovrride = data[0];

    if (ic->ic_cfg80211_radio_handler.set_he_bss_color) {
        ret = ic->ic_cfg80211_radio_handler.set_he_bss_color((void *)scn,
                                                             value, ovrride);
    }
    else {
        ret = -EOPNOTSUPP;
    }
    qdf_print("%s: value: %d override: %d ret: %d", __func__,
            value, ovrride, ret);

    return ret;
}

static int wlan_cfg80211_he_sr_config(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        struct wlan_cfg8011_genric_params *params)
{
    struct cfg80211_context *cfg_ctx  = NULL;
    struct ieee80211com *ic           = NULL;
    struct ol_ath_softc_net80211 *scn = NULL;
    int cmd_type = 0, ret = 0;
    void *cmd = NULL, *data = NULL;
    uint8_t param = params->value;
    uint8_t value;

    if(params->data == NULL) {
        qdf_info("Invalid Arguments");
        return -EINVAL;
    }

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    if (cfg_ctx == NULL) {
        qdf_err("Invalid Context");
        return -EINVAL;
    }

    ic = cfg_ctx->ic;
    if (ic == NULL) {
        qdf_err("Invalid Interface");
        return -EINVAL;
    }

    cmd = extract_command(ic, wdev, &cmd_type);

    if (!cmd_type) {
        qdf_info("Command on Invalid interface");
        return -EINVAL;
    } else {
        scn = (struct ol_ath_softc_net80211 *)cmd;
    }

    value = *((uint8_t *)params->data);
    data = &params->length;

    if (ic->ic_cfg80211_radio_handler.set_he_sr_config) {
        ret = ic->ic_cfg80211_radio_handler.set_he_sr_config((void *)scn,
                                                      param, value, data);
        qdf_info("%s: param: %d value: %d data: %pK ret: %d", __func__,
                param, value, data, ret);
    }
    else {
        ret = -EOPNOTSUPP;
    }

    return ret;
}

static int wlan_cfg80211_get_he_sr_config(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        struct wlan_cfg8011_genric_params *params)
{
#define HE_SR_CONFIG_NON_SRG_OBSSPD_STRING 32
#define HE_SR_CONFIG_NON_SRG_OBSSPD_ARGS 3
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    struct ol_ath_softc_net80211 *scn = NULL;
    int cmd_type = 0, ret = 0;
    void *cmd = NULL;
    uint32_t val[HE_SR_CONFIG_NON_SRG_OBSSPD_ARGS]  = {0};
    char string[HE_SR_CONFIG_NON_SRG_OBSSPD_STRING] = {0};

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    if (cfg_ctx == NULL) {
        qdf_err("Invalid Context");
        return -EINVAL;
    }

    ic = cfg_ctx->ic;
    if (ic == NULL) {
        qdf_err("Invalid Interface");
        return -EINVAL;
    }

    cmd = extract_command(ic, wdev, &cmd_type);

    if (!cmd_type) {
        qdf_info(" Command on Invalid interface");
        return -EINVAL;
    } else {
        scn = (struct ol_ath_softc_net80211 *)cmd;
    }

    if (ic->ic_cfg80211_radio_handler.get_he_sr_config) {
        ret = ic->ic_cfg80211_radio_handler.get_he_sr_config((void *)scn, params->value, val);

        switch (params->value) {

            case HE_SR_SRP_ENABLE:
            case HE_SR_SR15_ENABLE:
                return cfg80211_reply_command(wiphy, sizeof(uint32_t), val, 0);
            break;
            /* Non-SRG OBSS_PD ENABLE */
            case HE_SR_NON_SRG_OBSSPD_ENABLE:
                snprintf(string, HE_SR_CONFIG_NON_SRG_OBSSPD_STRING,
                        "%u obss_pd_threshold: %u", val[0], val[1]);

                cfg80211_reply_command(wiphy, sizeof(string), string, 0);
            break;

            default:
                qdf_info("Unhandled SR config command");
                return -EINVAL;
        }
    }
    else {
        ret = -EOPNOTSUPP;
    }


    return ret;
}

static int wlan_cfg80211_set_muedca_params(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        struct wlan_cfg8011_genric_params *params)

{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    osif_dev *osifp = NULL;
    wlan_if_t vap = NULL;
    int cmd_type;
    uint8_t muedcaparam, ac, value;

    void *cmd;
    uint8_t *data = (uint8_t *) params->data;

    if(data == NULL) {
        qdf_print("Invalid Arguments ");
        return -EINVAL;
    }

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    cmd = extract_command(ic, wdev, &cmd_type);
    if (cmd_type) {
        qdf_print(" Command on invalid interface");
        return -EINVAL;
    } else {
        vap = (wlan_if_t)cmd;
        osifp = ath_netdev_priv(wdev->netdev);
    }

    muedcaparam = params->value;
    ac = data[0];
    value = params->length;
    IEEE80211_DPRINTF
        (vap, IEEE80211_MSG_CFG80211,
         "%s: muedcaparam: %d ac: %d value: %d \n",
         __func__, muedcaparam, ac, value);
    return ieee80211_ucfg_set_muedcaparams((void *)osifp, muedcaparam,
            ac, value);

}

static int wlan_cfg80211_get_muedca_params(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        struct wlan_cfg8011_genric_params *params)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    osif_dev *osifp = NULL;
    wlan_if_t vap = NULL;
    int cmd_type;
    uint8_t muedcaparam, ac, value;

    void *cmd;
    uint8_t *data = (uint8_t *) params->data;

    if(data == NULL) {
        qdf_print("Invalid Arguments ");
        return -EINVAL;
    }

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    cmd = extract_command(ic, wdev, &cmd_type);
    if (cmd_type) {
        qdf_print(" Command on invalid interface");
        return -EINVAL;
    } else {
        vap = (wlan_if_t)cmd;
        osifp = ath_netdev_priv(wdev->netdev);
    }

    muedcaparam = params->value;
    ac = (data[0] <= MUEDCA_AC_VO) ? data[0] : MUEDCA_AC_BE;
    value = ieee80211_ucfg_get_muedcaparams((void *)osifp, muedcaparam, ac);
    IEEE80211_DPRINTF
        (vap, IEEE80211_MSG_CFG80211,
         "%s: muedcaparam: %d ac: %d value: %d \n",
         __func__, muedcaparam, ac, value);
    return cfg80211_reply_command(wiphy, sizeof(uint8_t), &value, 0);

}

#if ATH_ACS_DEBUG_SUPPORT

/* wlan_cfg80211_acsdbgtool_add_bcn:
 * Sends the custom beacon from the user-space ACS debug tool and passes it
 * to the ACS debug framework
 *
 * Parameters:
 * wiphy: Pointer to the wiphy device structure
 * wdev: Pointer to the wireless_dev structure
 * params: Pointer containing all the custom beacons defined in the user-space
 *         ACS debug tool
 *
 * Return:
 * -1: Error
 *  0: Success
 */
static int wlan_cfg80211_acsdbgtool_add_bcn(struct wiphy *wiphy,
                                            struct wireless_dev *wdev,
                                            struct wlan_cfg8011_genric_params *params)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    osif_dev *osifp = NULL;
    wlan_if_t vap = NULL;
    int cmd_type = 0;
    void *cmd = NULL;
    void *bcn_data = NULL;

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);

    if (cfg_ctx == NULL) {
        qdf_print("Invalid cfg content");
        return -EINVAL;
    }

    ic = cfg_ctx->ic;

    if (ic == NULL) {
        qdf_print("Invalid ic pointer");
        return -EINVAL;
    }

    cmd = extract_command(ic, wdev, &cmd_type);

    if (cmd_type) {
        qdf_print("Invalid cfg80211 command");
    }

    vap = (wlan_if_t)cmd;
    osifp = ath_netdev_priv(wdev->netdev);

    if (!params->data_len) {
        bcn_data = NULL;
    } else {
        bcn_data = params->data;
    }

    if (acs_debug_create_bcndb(ic->ic_acs, bcn_data)) {
        if (params->data_len) {
            qdf_print("Could not create the ACS debug beacon database");
            return -EINVAL;
        } else {
            qdf_print("Continuing without beacon injections");
        }
    }

    return EOK;
}


/* wlan_cfg80211_acsdbgtool_add_chanev:
 * Sends the custom channel information from the user-space ACS debug tool and
 * passes it to the ACS debug framework
 *
 * Parameters:
 * wiphy: Pointer to the wiphy device structure
 * wdev: Pointer to the wireless_dev structure
 * params: Pointer containing all the custom beacons defined in the user-space
 *         ACS debug tool
 *
 * Return:
 * -1: Error
 *  0: Success
 */
static int wlan_cfg80211_acsdbgtool_add_chanev(struct wiphy *wiphy,
                                             struct wireless_dev *wdev,
                                             struct wlan_cfg8011_genric_params *params)

{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    osif_dev *osifp = NULL;
    wlan_if_t vap = NULL;
    int cmd_type = 0;
    void *cmd = NULL;
    void *chan_data = NULL;

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);

    if (cfg_ctx == NULL) {
        qdf_print("Invalid cfg context");
        return -EINVAL;
    }

    ic = cfg_ctx->ic;

    if (ic == NULL) {
        qdf_print("Invalid ic pointer");
        return -EINVAL;
    }

    cmd = extract_command(ic, wdev, &cmd_type);

    if (cmd_type) {
        qdf_print("Invalid cfg80211 command");
    }

    vap = (wlan_if_t)cmd;
    osifp = ath_netdev_priv(wdev->netdev);

    if (!params->data_len) {
        chan_data = NULL;
    } else {
        chan_data = params->data;
    }

    if (acs_debug_create_chandb(ic->ic_acs, chan_data)) {
        if (params->data_len) {
            qdf_print("Could not create ACS debug channel event database");
            return -EINVAL;
        } else {
            qdf_print("Continuing without channel injections");
        }
    }

    return EOK;
}
#endif

static int wlan_cfg80211_kickmac(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        struct wlan_cfg8011_genric_params *params)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    osif_dev *osifp = NULL;
    wlan_if_t vap = NULL;
    int cmd_type ,retval = 0;
    void *cmd;
    u_int8_t mac[IEEE80211_ADDR_LEN];
    struct ieee80211req_mlme mlme;

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    cmd = extract_command(ic, wdev, &cmd_type);
    if (cmd_type) {
        qdf_print(" Command on invalid interface");
        return -EINVAL;
    } else {
        vap = (wlan_if_t)cmd;
        osifp = ath_netdev_priv(wdev->netdev);
    }

    if (params->data_len == IEEE80211_ADDR_LEN) {
        qdf_mem_copy(mac, params->data, IEEE80211_ADDR_LEN);
    } else {
        qdf_print("Invalid mac address ");
        return -EINVAL;
    }

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_CFG80211, "%s :%s \n", __func__, ether_sprintf(mac));
    /* Setup a MLME request for disassociation of the given MAC */
    mlme.im_op = IEEE80211_MLME_DISASSOC;
    mlme.im_reason = IEEE80211_REASON_UNSPECIFIED;
    qdf_mem_copy(&(mlme.im_macaddr), mac, IEEE80211_ADDR_LEN);
    /* Send the MLME request and return the result. */
    retval = ieee80211_ucfg_setmlme(ic, (void *)osifp, &mlme);
    return retval;
}

static int wlan_cfg80211_chan_widthswitch(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        struct wlan_cfg8011_genric_params *params)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    wlan_if_t vap = NULL;
    int cmd_type;
    void *cmd;
    u_int8_t chan, tbtt;
    u_int16_t ch_width;

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    cmd = extract_command(ic, wdev, &cmd_type);
    if (cmd_type) {
        qdf_print(" Command on invalid interface");
        return -EINVAL;
    } else {
        vap = (wlan_if_t)cmd;
    }

    chan = (u_int8_t)params->value;
    tbtt = (u_int8_t)params->length;
    ch_width = (u_int16_t)params->flags;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_CFG80211, "%s chan; %d tbtt:%d ch_width: %d \n", __func__, chan, tbtt, ch_width);
    qdf_print("Enabling Channel Switch Announcement on current channel");
    return (ieee80211_ucfg_set_chanswitch(vap, chan, tbtt, ch_width));
}

static int wlan_cfg80211_set_acparams(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        struct wlan_cfg8011_genric_params *params)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    wlan_if_t vap = NULL;
    int cmd_type;
    u_int8_t ac, use_rts, aggrsize_scaling;
    u_int32_t min_kbps;

    void *cmd;
    u_int8_t *data = (u_int8_t *) params->data;

    if(data == NULL) {
        qdf_print("Invalid Arguments ");
        return -EINVAL;
    }

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    cmd = extract_command(ic, wdev, &cmd_type);
    if (cmd_type) {
        qdf_print(" Command on invalid interface");
        return -EINVAL;
    } else {
        vap = (wlan_if_t)cmd;
    }

    ac  = (u_int8_t) params->value;
    use_rts = (u_int8_t) data[0];
    aggrsize_scaling = (u_int8_t) params->length;
    min_kbps = ((u_int8_t)params->flags) * 1000;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_CFG80211, "%s: ac: %d rts: %d aggrscale: %d kbps: %d \n", __func__, ac, use_rts, aggrsize_scaling, min_kbps);

    if (ac > 3 || (use_rts != 0 && use_rts != 1) ||
        aggrsize_scaling > 4 || min_kbps > 250000)
    {
        goto error;
    }

    wlan_set_acparams(vap, ac, use_rts, aggrsize_scaling, min_kbps);
    return 0;

error:
    qdf_print("usage: acparams ac <0|3> RTS <0|1> aggr scaling <0..4> min mbps <0..250>");
    return -EINVAL;

}

static int wlan_cfg80211_setrtparams(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        struct wlan_cfg8011_genric_params *params)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    wlan_if_t vap = NULL;
    int cmd_type , retv = -1;
    void *cmd;
    uint32_t param[4];
    uint32_t *data = (u_int32_t *) params->data;

    if(data == NULL) {
        qdf_print("Invalid Arguments ");
        return -EINVAL;
    }

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    cmd = extract_command(ic, wdev, &cmd_type);
    if (cmd_type == VAP_CMD) {
        vap = (wlan_if_t)cmd;
    } else {
        qdf_print(" Command on invalid interface");
        return -EINVAL;
    }

    param[0] = params->value;
    param[1] = data[0];
    param[2] = params->length;
    param[3] = params->flags;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_CFG80211,
            "%s: param0: %d param1: %d param2: %d param3: %d\n",
            __func__, param[0], param[1], param[2], param[3]);

    switch(param[0])
    {
        case IEEE80211_RCPARAMS_RTPARAM:
            retv = ieee80211_ucfg_rcparams_setrtparams(vap, param[1], param[2], param[3]);
            break;
        case IEEE80211_RCPARAMS_RTMASK:
            /* TODO: Last argument passed is 0 because support to extend command usage
             * for an extra argument is pending in WEXT. Will be updated once
             * the support is added for WEXT command as well. */
            retv = ieee80211_ucfg_rcparams_setratemask(vap, param[1], param[2], param[3], 0);
            break;
        default:
            break;
    }

    return retv;
}

static int wlan_cfg80211_setratemask(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        struct wlan_cfg8011_genric_params *params)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    wlan_if_t vap = NULL;
    int cmd_type , retv = -1;
    void *cmd;
    uint32_t param[4];
    uint32_t *data = (u_int32_t *) params->data;

    if(data == NULL) {
        qdf_print("Invalid Arguments ");
        return -EINVAL;
    }

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    cmd = extract_command(ic, wdev, &cmd_type);
    if (cmd_type == VAP_CMD) {
        vap = (wlan_if_t)cmd;
    } else {
        qdf_print(" Command on invalid interface");
        return -EINVAL;
    }

    param[0] = params->value;
    param[1] = data[0];
    param[2] = params->length;
    param[3] = params->flags;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_CFG80211,
            "%s: param0: %d param1: %d param2: %d param3: %d \n",
            __func__, param[0], param[1], param[2], param[3]);

    retv = ieee80211_ucfg_rcparams_setratemask(vap, param[0], param[1],
                                                param[2], param[3]);

    return retv;
}

static int wlan_cfg80211_set_wmm_params(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        struct wlan_cfg8011_genric_params *params)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    osif_dev *osifp = NULL;
    wlan_if_t vap = NULL;
    int cmd_type;
    int wmmparam, ac, bss, value;

    void *cmd;
    u_int32_t *data = (u_int32_t *) params->data;

    if(data == NULL) {
        qdf_print("Invalid Arguments ");
        return -EINVAL;
    }

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    cmd = extract_command(ic, wdev, &cmd_type);
    if (cmd_type) {
        qdf_print(" Command on invalid interface");
        return -EINVAL;
    } else {
        vap = (wlan_if_t)cmd;
        osifp = ath_netdev_priv(wdev->netdev);
    }

    wmmparam = params->value;
    ac = data[0];
    bss = params->length;
    value = params->flags;
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_CFG80211, "%s: wmmparam: %d ac: %d bss: %d value: %d \n", __func__, wmmparam, ac, bss, value);
    return ieee80211_ucfg_setwmmparams((void *)osifp, wmmparam, ac, bss, value);
}

static int wlan_cfg80211_set_filter(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        struct wlan_cfg8011_genric_params *params)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    osif_dev  *osifp = NULL;
    wlan_if_t vap = NULL;
    int cmd_type;
    void *cmd;
    uint32_t app_filterype = 0;

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    cmd = extract_command(ic, wdev, &cmd_type);
    if (cmd_type) {
        qdf_print(" Command on invalid interface");
        return -EINVAL;
    } else {
        osifp = ath_netdev_priv(wdev->netdev);
        vap = (wlan_if_t)cmd;

    }

    app_filterype = params->value;

    if (app_filterype & ~IEEE80211_FILTER_TYPE_ALL) {
        return -EINVAL;
    }

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_CFG80211, "%s: app_filterype: %d \n", __func__, app_filterype);
    osifp->app_filter = app_filterype;
    if (osifp->app_filter &
            (IEEE80211_FILTER_TYPE_ASSOC_REQ | IEEE80211_FILTER_TYPE_AUTH)) {
        wlan_set_param(vap, IEEE80211_TRIGGER_MLME_RESP, 1);
    }

    if (osifp->app_filter == 0) {
        wlan_set_param(vap, IEEE80211_TRIGGER_MLME_RESP, 0);
    }
    return 0;
}

void static
wlan_cfg80211_sta_send_mgmt(void *arg, struct ieee80211_node *ni)
{
    struct ieee80211vap    *vap = ni->ni_vap;
    struct ieee80211req_mgmtbuf *mgmt_frm = (struct ieee80211req_mgmtbuf *)arg;
    int rc =0;

    if (ni != vap->iv_bss)
    {
        rc = wlan_send_mgmt(vap, ni->ni_macaddr, mgmt_frm->buf, mgmt_frm->buflen);
        if (rc < 0) {
            qdf_err("%s: send mgmt failed with err %d \n", __func__, rc);
        }
    }
}

static int wlan_cfg80211_send_mgmt(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        struct wlan_cfg8011_genric_params *params)
{
    int ret = 0;
    struct ieee80211req_mgmtbuf *mgmt_frm;
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    wlan_if_t vap = NULL;
    int cmd_type;
    void *cmd;
    uint32_t *data = (u_int32_t *) params->data;

    if(data == NULL) {
        qdf_err("%s: Invalid Arguments \n", __func__);
        return -EINVAL;
    }

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    cmd = extract_command(ic, wdev, &cmd_type);
    if (cmd_type == VAP_CMD) {
        vap = (wlan_if_t)cmd;
    } else {
        qdf_err(" %s Command on invalid interface \n", __func__);
        return -EINVAL;
    }

    mgmt_frm = (struct ieee80211req_mgmtbuf *) params->data;

    if(mgmt_frm->buflen >= 37 && mgmt_frm->buf[24] == 0xff) {
        qdf_err(" %s unknown action frame\n", __func__);
        return -EINVAL;
    }

    /* if the macaddr requested is for broadcast then search for
       all connected sta and send the mgmt packet */
    if(vap) {
        if(IEEE80211_IS_BROADCAST(mgmt_frm->macaddr)) {
            ret = wlan_iterate_station_list(vap, wlan_cfg80211_sta_send_mgmt, mgmt_frm);
        }
        else {
            ret = wlan_send_mgmt(vap, mgmt_frm->macaddr, mgmt_frm->buf, mgmt_frm->buflen);
        }
    } else {
        qdf_err("%s: Invalid vap \n", __func__);
        return -EINVAL;
    }

    /* wlan_iterate_station_list returns the number of sta connected,
       so return error only if the return value is less than zero */
    if (ret < 0) {
        return ret;
    } else {
        return 0;
    }
}

int wlan_cfg80211_set_channel(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        struct wlan_cfg8011_genric_params *params)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    osif_dev  *osifp = NULL;
    wlan_if_t vap = NULL;
    int cmd_type;
    void *cmd;
    int channel = 0;

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    cmd = extract_command(ic, wdev, &cmd_type);
    if (cmd_type) {
        qdf_print(" Command on invalid interface");
        return -EINVAL;
    } else {
        osifp = ath_netdev_priv(wdev->netdev);
        vap = (wlan_if_t)cmd;

    }
    channel = params->value;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_CFG80211, "%s: channel: %d \n", __func__, channel);
    if ((wlan_vdev_chan_config_valid(vap->vdev_obj) == QDF_STATUS_SUCCESS)) {
        return ieee80211_ucfg_set_freq(vap, channel);
    } else {
        /* store channel value in VAP, which will be set in start_ap */
        vap->iv_cfg80211_channel = channel;
        if (vap->iv_opmode == IEEE80211_M_MONITOR || vap->iv_special_vap_mode) {
            if (ieee80211_ucfg_set_freq(vap, vap->iv_cfg80211_channel) < 0) {
                qdf_err("%s: Could not set channel for monitor vap\n", __func__);
                return -EINVAL;
            }
        }
        return 0;
    }
}

static int wlan_cfg80211_get_aclmac(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        struct wlan_cfg8011_genric_params *params)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    wlan_if_t vap = NULL;
    osif_dev *osifp = NULL;
    int cmd_type;
    void *cmd;
    u_int8_t mac[IEEE80211_ADDR_LEN];
    u_int8_t *mac_list;
    int i, rc, num_mac;

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    cmd = extract_command(ic, wdev, &cmd_type);
    if (cmd_type) {
        qdf_print(" Command on invalid interface");
        return -EINVAL;
    } else {
        osifp = ath_netdev_priv(wdev->netdev);
        vap = (wlan_if_t)cmd;
    }

    /* read and send mac list to application */
    mac_list = (u_int8_t *)OS_MALLOC(osifp->os_handle,
            (IEEE80211_ADDR_LEN * 256), GFP_KERNEL);
    if (!mac_list) {
        return -EFAULT;
    }
    rc = wlan_get_acl_list(vap, mac_list, (IEEE80211_ADDR_LEN * 256),
            &num_mac, IEEE80211_ACL_FLAG_ACL_LIST_1);
    if(rc) {
        OS_FREE(mac_list);
        return -EFAULT;
    }

    for (i = 0; i < num_mac && i < 256; i++) {
        memcpy(&mac, &mac_list[i * IEEE80211_ADDR_LEN], IEEE80211_ADDR_LEN);
        cfg80211_reply_command(wiphy, IEEE80211_ADDR_LEN, &mac, 0);
    }
    OS_FREE(mac_list);
    return 0;
}

static int wlan_cfg80211_mac_commands(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        struct wlan_cfg8011_genric_params *params)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    wlan_if_t vap = NULL;
    int cmd_type ,ret = 0, mac_command = 0;
    void *cmd;
    u_int8_t mac[IEEE80211_ADDR_LEN];

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    cmd = extract_command(ic, wdev, &cmd_type);
    if (cmd_type) {
        qdf_print(" Command on invalid interface");
        return -EINVAL;
    } else {
        vap = (wlan_if_t)cmd;
    }

    mac_command = params->value;
    if (params->data_len == IEEE80211_ADDR_LEN) {
        qdf_mem_copy(mac, params->data, IEEE80211_ADDR_LEN);
    } else {
        qdf_print("Invalid mac address ");
        return -EINVAL;
    }

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_CFG80211, " %s :%s  maccmd: %d \n", __func__, ether_sprintf(mac), mac_command);

    switch (mac_command)
    {
        case MAC_COMMND_ADD_WDS_ADDR:
            ret = ieee80211_add_wdsaddr(vap, (union iwreq_data *)mac);
            if (ret) {
                return -EFAULT;
            }
            break;
        case MAC_COMMND_SEND_WOWPKT:
            return ieee80211_sendwowpkt(wdev->netdev, mac);
            break;
        default:
            break;
    }

    return ret;
}

static int wlan_cfg80211_set_wirelessmode(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        struct wlan_cfg8011_genric_params *params)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    wlan_if_t vap = NULL;
    int cmd_type;
    void *cmd;
    char mode[30];

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    qdf_mem_zero(mode, sizeof(mode));
    cmd = extract_command(ic, wdev, &cmd_type);
    if (cmd_type) {
        qdf_print(" Command on invalid interface");
        return -EINVAL;
    } else {
        vap = (wlan_if_t)cmd;
    }

    if ((params->data_len) && (params->data_len < sizeof(mode))) {
        qdf_mem_copy(mode, params->data, params->data_len);
    } else {
        qdf_print("%s: Invalid mode ", __func__);
        return -EINVAL;
    }

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_CFG80211, "%s: mode %s \n", __func__, mode);
    return ieee80211_ucfg_set_phymode(vap, mode, params->data_len);

}

static int wlan_cfg80211_get_wirelessmode(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        struct wlan_cfg8011_genric_params *params)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    wlan_if_t vap = NULL;
    int cmd_type;
    u_int16_t length = 0;
    void *cmd;
    char mode[30];

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    cmd = extract_command(ic, wdev, &cmd_type);
    if (cmd_type) {
        qdf_print(" Command on invalid interface");
        return -EINVAL;
    } else {
        vap = (wlan_if_t)cmd;
    }

    qdf_mem_zero(mode, sizeof(mode));
    ieee80211_ucfg_get_phymode(vap, mode, &length, CURR_MODE);

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_CFG80211, "%s: mode %s legth %d\n", __func__, mode, length);
    cfg80211_reply_command(wiphy, length+1, mode, 0);
    return 0;
}

static int wlan_cfg80211_get_ssid(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        struct wlan_cfg8011_genric_params *params)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    wlan_if_t vap = NULL;
    int cmd_type;
    void *cmd;
    ieee80211_ssid tmpssid;
    int des_nssid = 0, error = 0;

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    cmd = extract_command(ic, wdev, &cmd_type);
    if (cmd_type) {
        qdf_print(" Command on invalid interface");
        return -EINVAL;
    } else {
        vap = (wlan_if_t)cmd;
    }
    qdf_mem_zero(&tmpssid, sizeof(ieee80211_ssid));
    error = ieee80211_ucfg_get_essid(vap, &tmpssid, &des_nssid);
    if (error)
        return error;

    if ((des_nssid > 0) || (tmpssid.len > 0)) {
        cfg80211_reply_command(wiphy, tmpssid.len, tmpssid.ssid, 0);
    }

    return error;
}

static int wlan_cfg80211_set_ssid(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        struct wlan_cfg8011_genric_params *params)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    wlan_if_t vap = NULL;
    int cmd_type;
    void *cmd;
    ieee80211_ssid   tmpssid;

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    cmd = extract_command(ic, wdev, &cmd_type);
    if (cmd_type) {
        qdf_print(" Command on invalid interface");
        return -EINVAL;
    } else {
        vap = (wlan_if_t)cmd;
    }

    if (params->data_len) {

        if (params->data_len > IEEE80211_NWID_LEN) {
            params->data_len = IEEE80211_NWID_LEN;
        }

        tmpssid.len = params->data_len;
        qdf_mem_copy(tmpssid.ssid, params->data, params->data_len);
        tmpssid.ssid[tmpssid.len] = '\0';
    } else {
        /* ANY */
        tmpssid.ssid[0] = '\0';
        tmpssid.len = 0;
    }
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_CFG80211, "%s: SSID %s \n", __func__, tmpssid.ssid);

    /*
     * Special VAP mode is AP+Monitor mode, for this mode VAP is up in dev_open itself.
     * As we are not do VAP bringup using start_ap. poulate SSID values so that
     * other applications (SON) can read SSID values.
     */

    if (vap->iv_special_vap_mode) {
        wdev->ssid_len =  tmpssid.len;
        memcpy(wdev->ssid, tmpssid.ssid, wdev->ssid_len);
    }

    return ieee80211_ucfg_set_essid(vap, &tmpssid, 1);
}

/* Radio Commands */
#define ISO_NAME_SIZE 3

static int wlan_cfg80211_set_country(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        struct wlan_cfg8011_genric_params *params)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    struct ol_ath_softc_net80211 *scn = NULL;
    int cmd_type ,ret = 0;
    void *cmd;
    char isoname[ISO_NAME_SIZE];

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    cmd = extract_command(ic, wdev, &cmd_type);

    if (!cmd_type) {
        qdf_print(" Command on Invalid interface");
        return -EINVAL;
    } else {
        scn = (struct ol_ath_softc_net80211 *)cmd;
    }

    if (params->data_len <= ISO_NAME_SIZE) {
        qdf_mem_copy(&isoname, params->data, ISO_NAME_SIZE);
        if (params->data_len <= ISO_NAME_SIZE-1)
            isoname[params->data_len] = '\0';
    } else {
        qdf_print("Invalid country name/length ");
        return -EINVAL;
    }
    if(ic->ic_cfg80211_radio_handler.setcountry)
        ret = ic->ic_cfg80211_radio_handler.setcountry((void*)scn, (char *)&isoname);
    else
        ret = -EOPNOTSUPP;
    if (params->data_len < ISO_NAME_SIZE)
        qdf_print("%s: Country: %s ret: %d ", __func__, isoname, ret);

    return ret;
}

static int wlan_cfg80211_get_country(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        struct wlan_cfg8011_genric_params *params)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    struct ol_ath_softc_net80211 *scn = NULL;
    int cmd_type ,ret = 0;
    void *cmd;
    char isoname[ISO_NAME_SIZE+1];

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    cmd = extract_command(ic, wdev, &cmd_type);

    if (!cmd_type) {
        qdf_print(" Command on Invalid interface");
        return -EINVAL;
    } else {
        scn = (struct ol_ath_softc_net80211 *)cmd;
    }

    if ( ic->ic_cfg80211_radio_handler.getcountry )
        ret =  ic->ic_cfg80211_radio_handler.getcountry((void*)scn, isoname);
    else
        ret = -EOPNOTSUPP;

    isoname[ISO_NAME_SIZE] = '\0';
    cfg80211_reply_command(wiphy, ISO_NAME_SIZE+1, isoname, 0);
    qdf_print("%s: Country: %s ret: %d", __func__, isoname,ret);

    return ret;
}
static int wlan_cfg80211_set_hwaddr(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        struct wlan_cfg8011_genric_params *params)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    struct ol_ath_softc_net80211 *scn = NULL;
    int cmd_type ,ret = 0;
    void *cmd;
    u_int8_t mac[IEEE80211_ADDR_LEN];

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    cmd = extract_command(ic, wdev, &cmd_type);

    if (!cmd_type) {
        qdf_print(" Command on Invalid interface");
        return -EINVAL;
    } else {
        scn = (struct ol_ath_softc_net80211 *)cmd;
    }

    if (params->data_len == IEEE80211_ADDR_LEN) {
        qdf_mem_copy(mac, params->data, IEEE80211_ADDR_LEN);
    } else {
        qdf_print("Invalid mac address ");
        return -EINVAL;
    }

    if (  ic->ic_cfg80211_radio_handler.sethwaddr )
        ret = ic->ic_cfg80211_radio_handler.sethwaddr((void*)scn, mac);
    else
        ret = -EOPNOTSUPP;
    qdf_print("%s :%s ret: %d ", __func__, ether_sprintf(mac),ret);
    return ret;
}

static int wlan_cfg80211_set_ba_timeout(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        struct wlan_cfg8011_genric_params *params)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    struct ol_ath_softc_net80211 *scn = NULL;
    int cmd_type ,ret = 0;
    void *cmd;
    uint32_t duration;
    uint8_t ac;

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    cmd = extract_command(ic, wdev, &cmd_type);

    if (!cmd_type) {
        qdf_print(" Command on Invalid interface");
        return -EINVAL;
    } else {
        scn = (struct ol_ath_softc_net80211 *)cmd;
    }
    ac = params->value;
    duration = params->length;
   /* Valid access category - 0,1,2,3
    * 0 - Background
    * 1 - Best effort
    * 2 - Video
    * 3 - Voice
    */
    if ( ac > 3 ) {
        ret = -EOPNOTSUPP;
        qdf_print("Invalid AC value - %d\n", ac);
        return ret;
    }
    if ( ic->ic_cfg80211_radio_handler.set_ba_timeout ) {
        ic->ic_cfg80211_radio_handler.set_ba_timeout(scn, ac, duration);
    }
    else
        ret = -EOPNOTSUPP;
    qdf_print("%s: ac: %d timeout: %d ret: %d\n", __func__, ac, duration, ret);
    return ret;

}

static int wlan_cfg80211_get_ba_timeout(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        struct wlan_cfg8011_genric_params *params)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    struct ol_ath_softc_net80211 *scn = NULL;
    int cmd_type ,ret = 0;
    void *cmd;
    uint32_t duration;
    uint8_t ac;

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    cmd = extract_command(ic, wdev, &cmd_type);

    if (!cmd_type) {
        qdf_print(" Command on Invalid interface");
        return -EINVAL;
    } else {
        scn = (struct ol_ath_softc_net80211 *)cmd;
    }
    ac = params->value;
    /* Valid access category - 0,1,2,3
    * 0 - Background
    * 1 - Best effort
    * 2 - Video
    * 3 - Voice
    */
    if ( ac > 3 ) {
        ret = -EOPNOTSUPP;
        qdf_print("Invalid AC value - %d\n", ac);
        return ret;
    }
    if ( ic->ic_cfg80211_radio_handler.get_ba_timeout ) {
        ic->ic_cfg80211_radio_handler.get_ba_timeout(scn, ac, &duration);
        qdf_print("AC: %d BA_timeout: %d\n", ac, duration);
        return ret;
    }
    else
        ret = -EOPNOTSUPP;
    return ret;

}

static int wlan_cfg80211_set_aggr_burst(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        struct wlan_cfg8011_genric_params *params)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    struct ol_ath_softc_net80211 *scn = NULL;
    int cmd_type ,ret = 0;
    void *cmd;
    uint32_t ac, duration;

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    cmd = extract_command(ic, wdev, &cmd_type);

    if (!cmd_type) {
        qdf_print(" Command on Invalid interface");
        return -EINVAL;
    } else {
        scn = (struct ol_ath_softc_net80211 *)cmd;
    }

    ac = params->value;
    duration = params->length;

    if ( ic->ic_cfg80211_radio_handler.set_aggr_burst )
        ret = ic->ic_cfg80211_radio_handler.set_aggr_burst(scn, ac, duration);
    else
        ret = -EOPNOTSUPP;
    qdf_print("%s: ac: %d duration: %d ret: %d", __func__, ac, duration, ret);
    return ret;
}

static int wlan_cfg80211_set_atf_sched_dur(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        struct wlan_cfg8011_genric_params *params)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    struct ol_ath_softc_net80211 *scn = NULL;
    int cmd_type ,ret = 0;
    void *cmd;
    uint32_t ac, duration;

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    cmd = extract_command(ic, wdev, &cmd_type);

    if (!cmd_type) {
        qdf_print(" Command on Invalid interface");
        return -EINVAL;
    } else {
        scn = (struct ol_ath_softc_net80211 *)cmd;
    }

    ac = params->value;
    duration = params->length;
#if QCA_AIRTIME_FAIRNESS
    if ( ic->ic_cfg80211_radio_handler.set_atf_sched_dur )
        ret = ic->ic_cfg80211_radio_handler.set_atf_sched_dur(scn, ac, duration);
    else
#endif
        ret = -EOPNOTSUPP;
    return ret;
}

static int wlan_cfg80211_txrx_peer_stats(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        struct wlan_cfg8011_genric_params *params)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    struct ol_ath_softc_net80211 *scn = NULL;
    int cmd_type;
    void *cmd;
    u_int8_t mac[IEEE80211_ADDR_LEN];

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    cmd = extract_command(ic, wdev, &cmd_type);

    if (!cmd_type) {
        qdf_print(" Command on Invalid interface");
        return -EINVAL;
    } else {
        scn = (struct ol_ath_softc_net80211 *)cmd;
    }

    if (params->data_len == IEEE80211_ADDR_LEN) {
        qdf_mem_copy(mac, params->data, IEEE80211_ADDR_LEN);
    } else {
        qdf_print("Invalid mac address ");
        return -EINVAL;
    }
    if( ic->ic_cfg80211_radio_handler.txrx_peer_stats )
        ic->ic_cfg80211_radio_handler.txrx_peer_stats((void*)scn, mac);
    else
        return -EOPNOTSUPP;
    qdf_print("%s :%s  ", __func__, ether_sprintf(mac));

    return 0;
}

static int wlan_cfg80211_txrx_dp_fw_peer_stats(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        struct wlan_cfg8011_genric_params *params)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    struct ol_ath_softc_net80211 *scn = NULL;
    int cmd_type;
    void *cmd;
    u_int8_t mac[IEEE80211_ADDR_LEN];
    u_int8_t caps;

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    cmd = extract_command(ic, wdev, &cmd_type);

    if (!cmd_type) {
        qdf_print(" Command on Invalid interface");
        return -EINVAL;
    } else {
        scn = (struct ol_ath_softc_net80211 *)cmd;
    }

    if (params->data_len == IEEE80211_ADDR_LEN) {
        qdf_mem_copy(mac, params->data, IEEE80211_ADDR_LEN);
    } else {
        qdf_print("Invalid mac address ");
        return -EINVAL;
    }
    caps = params->value; /* attribute 18 holds caps value */
    qdf_print("%s :%s caps:%d ", __func__, ether_sprintf(mac), caps);

    if(ic->ic_cfg80211_radio_handler.get_dp_fw_peer_stats) {
        ic->ic_cfg80211_radio_handler.get_dp_fw_peer_stats((void*)scn, mac, caps);
    }
    else  {
        return -EOPNOTSUPP;
    }

    return 0;
}

static int wlan_cfg80211_set_peer_nexthop(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        struct wlan_cfg8011_genric_params *params)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    osif_dev *osifp = NULL;
    wlan_if_t vap = NULL;
    int cmd_type;
    void *cmd;
    u_int8_t mac[IEEE80211_ADDR_LEN];
    u_int32_t if_num;


    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    cmd = extract_command(ic, wdev, &cmd_type);

    if (cmd_type) {
        qdf_print(" Command on Invalid interface");
        return -EINVAL;
    } else {
        vap = (wlan_if_t)cmd;
        osifp = ath_netdev_priv(wdev->netdev);
    }

    if (params->data_len == IEEE80211_ADDR_LEN) {
        qdf_mem_copy(mac, params->data, IEEE80211_ADDR_LEN);
    } else {
        qdf_print("Invalid mac address ");
        return -EINVAL;
    }
    if_num = params->value; /* attribute 18 holds next hop interface number */

    if (if_num > MAX_NSS_INTERFACE_NUM) {
        qdf_print("Next hop interface number should be less than %d", MAX_NSS_INTERFACE_NUM);
        return -EINVAL;
    }

    return ieee80211_ucfg_set_peer_nexthop((void *)osifp, mac, if_num);
}

#if SM_ENG_HIST_ENABLE
extern void wlan_mlme_print_all_sm_history(void);

static int wlan_cfg80211_sm_history(struct wiphy *wiphy,
                                    struct wireless_dev *wdev,
                                    struct wlan_cfg8011_genric_params *params)
{
    wlan_mlme_print_all_sm_history();
    return 0;
}
#endif

static int wlan_cfg80211_set_vlan_type(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        struct wlan_cfg8011_genric_params *params)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    osif_dev *osifp = NULL;
    wlan_if_t vap = NULL;
    int cmd_type;
    void *cmd;
    uint8_t default_vlan, port_vlan;
    uint8_t param[2];
    uint8_t *data = (u_int8_t *) params->data;


    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    cmd = extract_command(ic, wdev, &cmd_type);

    if (cmd_type) {
        return -EINVAL;
    } else {
        vap = (wlan_if_t)cmd;
        osifp = ath_netdev_priv(wdev->netdev);
    }

    param[0] = params->value;
    param[1] = data[0];

    default_vlan = param[0];
    port_vlan = param[1];
    ieee80211_ucfg_set_vlan_type((void *)osifp, default_vlan, port_vlan);

    return 0;
}

static struct ol_ath_softc_net80211 *
wlan_cfg80211_extract_radio_intf(struct wiphy *wiphy, struct wireless_dev *wdev)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    struct ol_ath_softc_net80211 *scn = NULL;
    int cmd_type;
    void *cmd;

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    if (cfg_ctx == NULL) {
        qdf_err("Invalid Context");
        return NULL;
    }

    ic = cfg_ctx->ic;
    if (ic == NULL) {
        qdf_err("Invalid Interface");
        return NULL;
    }

    ic = cfg_ctx->ic;

    cmd = extract_command(ic, wdev, &cmd_type);

    if (!cmd_type)
        qdf_err(" Command on Invalid interface");
    else
        scn = (struct ol_ath_softc_net80211 *)cmd;
    return scn;
}

static wlan_if_t
wlan_cfg80211_extract_vap_intf(struct wiphy *wiphy, struct wireless_dev *wdev)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;

    wlan_if_t vap = NULL;
    int cmd_type;
    void *cmd;

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    if (cfg_ctx == NULL) {
        qdf_err("Invalid Context");
        return NULL;
    }

    ic = cfg_ctx->ic;
    if (ic == NULL) {
        qdf_err("Invalid Interface");
        return NULL;
    }

    cmd = extract_command(ic, wdev, &cmd_type);
    if (cmd_type) {
        qdf_err("Command on Invalid Interface");
        return NULL;
    }

    vap = (wlan_if_t)cmd;
    return vap;
}

#if UNIFIED_SMARTANTENNA
#define MAX_SA_PARAM_LEN 16     /* 4 DWORDS */
/*
 * wlan_cfg80211_set_sa_params:
 * Sets smart antenna params for a particular radio
 */
static int
wlan_cfg80211_set_sa_params(struct wiphy *wiphy, struct wireless_dev *wdev,
                            struct wlan_cfg8011_genric_params *params)
{
    struct ieee80211com *ic = NULL;
    struct ol_ath_softc_net80211 *scn = NULL;
    struct cfg80211_context *cfg_ctx = NULL;
    int cmd_type;
    void *cmd;
    int ret = 0;
    char sa_param_buf[MAX_SA_PARAM_LEN] = {0};
    uint32_t *buff = (uint32_t *)sa_param_buf;

    if (params->data == NULL) {
        qdf_err("Invalid Arguments");
        return -EINVAL;
    }

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    if (cfg_ctx == NULL) {
        qdf_err("Invalid Context");
        return -EINVAL;
    }

    ic = cfg_ctx->ic;
    if (ic == NULL) {
        qdf_err("Invalid Interface");
        return -EINVAL;
    }

    cmd = extract_command(ic, wdev, &cmd_type);

    if ((!cmd_type) || (!cmd)) {
        qdf_err(" Command on Invalid interface");
        return -EINVAL;
    } else
        scn = (struct ol_ath_softc_net80211 *)cmd;

    buff[0] = params->value;
    buff[1] = *((uint32_t *)params->data);
    buff[2] = params->length;
    buff[3] = params->flags;

    if (ic->ic_cfg80211_radio_handler.set_saparam) {
        ic->ic_cfg80211_radio_handler.set_saparam((void *)scn, sa_param_buf);
    } else
        ret = -EOPNOTSUPP;

    return ret;
}

/*
 * wlan_cfg80211_get_sa_params:
 * Gets smart antenna params for a particular radio
 */
static int
wlan_cfg80211_get_sa_params(struct wiphy *wiphy, struct wireless_dev *wdev,
                            struct wlan_cfg8011_genric_params *params)
{
    struct ieee80211com *ic = NULL;
    struct ol_ath_softc_net80211 *scn = NULL;
    struct cfg80211_context *cfg_ctx = NULL;
    int cmd_type;
    void *cmd;
    int ret = 0;
    char sa_param_buf[MAX_SA_PARAM_LEN] = {0};
    uint32_t *buff = (uint32_t *)sa_param_buf;

    if (params->data == NULL) {
        qdf_err("Invalid Arguments");
        return -EINVAL;
    }

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    if (cfg_ctx == NULL) {
        qdf_err("Invalid Context");
        return -EINVAL;
    }

    ic = cfg_ctx->ic;
    if (ic == NULL) {
        qdf_err("Invalid Interface");
        return -EINVAL;
    }

    cmd = extract_command(ic, wdev, &cmd_type);

    if ((!cmd_type) || (!cmd)) {
        qdf_err(" Command on Invalid interface");
        return -EINVAL;
    } else
        scn = (struct ol_ath_softc_net80211 *)cmd;

    buff[0] = params->value;
    buff[1] = *((uint32_t *)params->data);
    buff[2] = params->length;

    if (ic->ic_cfg80211_radio_handler.get_saparam) {
        ret = ic->ic_cfg80211_radio_handler.get_saparam((void *)scn, sa_param_buf);
    } else
        ret = -EOPNOTSUPP;

    if (ret == 0)
        cfg80211_reply_command(wiphy, sizeof(u_int32_t), sa_param_buf, 0);

    return ret;
}
#endif

/*
 * wlan_updt_vap_pcp_tid_map:
 * Updates pcp to tid mapping for all vap's of a particular radio
 */
static INLINE void wlan_updt_vap_pcp_tid_map(void *arg, wlan_if_t vap)
{
    uint32_t pcp = *(uint32_t *)arg;
    if (!vap->iv_tidmap_tbl_id)
        wlan_set_vap_pcp_tid_map(vap, pcp,
                                 (uint32_t)vap->iv_ic->ic_pcp_tid_map[pcp]);
}

/*
 * wlan_cfg80211_set_def_pcp_tid_map:
 * Sets pcp to tid mapping for a particular radio
 */
static int
wlan_cfg80211_set_def_pcp_tid_map(struct wiphy *wiphy,
                                  struct wireless_dev *wdev,
                                  struct wlan_cfg8011_genric_params *params)
{
    struct ieee80211com *ic = NULL;
    struct ol_ath_softc_net80211 *scn = NULL;
    int ret = 0;
    uint32_t pcp, tid;

    scn = wlan_cfg80211_extract_radio_intf(wiphy, wdev);
    if (!scn) {
        qdf_err(" Command on Invalid interface");
        return -EINVAL;
    } else
        ic = &scn->sc_ic;

    pcp = params->value;
    tid = params->length;

    if ( ic->ic_cfg80211_radio_handler.set_pcp_tid_map ) {
        ic->ic_cfg80211_radio_handler.set_pcp_tid_map(scn, pcp, tid);
        wlan_iterate_vap_list(ic, wlan_updt_vap_pcp_tid_map, (void *)&pcp);
    } else
        ret = -EOPNOTSUPP;

    return ret;
}

/*
 * wlan_cfg80211_get_def_pcp_tid_map:
 * Gets pcp to tid mapping for a particular radio
 */
static int
wlan_cfg80211_get_def_pcp_tid_map(struct wiphy *wiphy,
                                  struct wireless_dev *wdev,
                                  struct wlan_cfg8011_genric_params *params)
{
    struct ieee80211com *ic = NULL;
    struct ol_ath_softc_net80211 *scn = NULL;
    int retval = 0;
    uint32_t pcp, value;

    scn = wlan_cfg80211_extract_radio_intf(wiphy, wdev);
    if (!scn) {
        qdf_err(" Command on Invalid interface");
        return -EINVAL;
    } else
        ic = &scn->sc_ic;
    pcp = params->value;
    if ( ic->ic_cfg80211_radio_handler.get_pcp_tid_map ) {
        ic->ic_cfg80211_radio_handler.get_pcp_tid_map(ic, pcp, &value);
        if (value < 0)
            retval = -EOPNOTSUPP;
    } else
        retval = -EOPNOTSUPP;

    cfg80211_reply_command(wiphy, sizeof(u_int32_t), &value, 0);
    return retval;
}

/*
 * wlan_updt_vap_tidmap_prty:
 * Updates tidmap priority for all vap's of a particular radio
 */
static INLINE void wlan_updt_vap_tidmap_prty(void *arg, wlan_if_t vap)
{
    if (!vap->iv_tidmap_tbl_id)
        wlan_set_vap_tidmap_prty(vap, (uint32_t)vap->iv_ic->ic_tidmap_prty);
}
/*
 * wlan_cfg80211_set_def_tidmap_prty:
 * Sets tidmap priority for a particular radio
 */
static int wlan_cfg80211_set_def_tidmap_prty(struct wiphy *wiphy,
                struct wireless_dev *wdev,
                struct wlan_cfg8011_genric_params *params)
{
    struct ieee80211com *ic = NULL;
    struct ol_ath_softc_net80211 *scn = NULL;
    int retval = 0;
    uint32_t prio;

    scn = wlan_cfg80211_extract_radio_intf(wiphy, wdev);
    if (!scn) {
        qdf_err(" Command on Invalid interface");
        return -EINVAL;
    } else
        ic = &scn->sc_ic;
    prio = params->value;
    if (ic->ic_cfg80211_radio_handler.set_tidmap_prty) {
        ic->ic_cfg80211_radio_handler.set_tidmap_prty(scn, prio);
        wlan_iterate_vap_list(ic, wlan_updt_vap_tidmap_prty, NULL);
    } else
        retval = -EOPNOTSUPP;

    return retval;
}

/*
 * wlan_cfg80211_get_def_tidmap_prty:
 * Gets tidmap priority for a particular radio
 */
static int wlan_cfg80211_get_def_tidmap_prty(struct wiphy *wiphy,
                struct wireless_dev *wdev,
                struct wlan_cfg8011_genric_params *params)
{
    struct ieee80211com *ic = NULL;
    struct ol_ath_softc_net80211 *scn = NULL;
    int retval = 0;
    uint32_t value;

    scn = wlan_cfg80211_extract_radio_intf(wiphy, wdev);
    if (!scn) {
        qdf_err("Command on Invalid interface");
        return -EINVAL;
    } else
        ic = &scn->sc_ic;
    if (ic->ic_cfg80211_radio_handler.get_tidmap_prty) {
        ic->ic_cfg80211_radio_handler.get_tidmap_prty(scn, &value);
        if (value < 0)
            retval = -EOPNOTSUPP;
    } else
        retval = -EOPNOTSUPP;

    cfg80211_reply_command(wiphy, sizeof(u_int32_t), &value, 0);
    return retval;
}

/*
 * wlan_cfg80211_set_pcp_tid_map:
 * Sets pcp to tid mapping for a particular vap
 */
static int wlan_cfg80211_set_pcp_tid_map(struct wiphy *wiphy,
                struct wireless_dev *wdev,
                struct wlan_cfg8011_genric_params *params)
{
    wlan_if_t vap = NULL;
    int retval = 0;
    uint32_t pcp, tid;

    vap = wlan_cfg80211_extract_vap_intf(wiphy, wdev);
    if (!vap) {
        qdf_err("Command on Invalid Interface");
        return -EINVAL;
    }
    if (!vap->iv_tidmap_tbl_id) {
        qdf_info("Vap is configured to use radio's mapping table");
        return -1;
    }

    pcp = params->value;
    tid = params->length;
    retval = wlan_set_vap_pcp_tid_map(vap, pcp, tid);

    return retval;
}

/*
 * wlan_cfg80211_get_pcp_tid_map:
 * Gets pcp to tid mapping for a particular vap
 */
static int wlan_cfg80211_get_pcp_tid_map(struct wiphy *wiphy,
                struct wireless_dev *wdev,
                struct wlan_cfg8011_genric_params *params)
{
    wlan_if_t vap = NULL;
    int retval = 0, value;
    uint32_t pcp;

    vap = wlan_cfg80211_extract_vap_intf(wiphy, wdev);
    if (!vap) {
        qdf_err("Command on Invalid Interface");
        return -EINVAL;
    }

    pcp = params->value;
    value = wlan_get_vap_pcp_tid_map(vap, pcp);

    if (value < 0)
        return -EOPNOTSUPP;

    cfg80211_reply_command(wiphy, sizeof(u_int32_t), &value, 0);
    return retval;
}

/*
 * wlan_cfg80211_set_tidmap_prty:
 * Sets tid mapping priority for a particular vap
 */
static int wlan_cfg80211_set_tidmap_prty(struct wiphy *wiphy,
                struct wireless_dev *wdev,
                struct wlan_cfg8011_genric_params *params)
{
    wlan_if_t vap = NULL;
    int retval = 0;
    uint32_t prio;

    vap = wlan_cfg80211_extract_vap_intf(wiphy, wdev);
    if (!vap) {
        qdf_err("Command on Invalid Interface");
        return -EINVAL;
    }
    if (!vap->iv_tidmap_tbl_id) {
        qdf_info("Vap is configured to use radio's mapping table");
        return -1;
    }

    prio = params->value;
    retval = wlan_set_vap_tidmap_prty(vap, prio);

    return retval;
}

/*
 * wlan_cfg80211_get_tidmap_prty:
 * Gets tid mapping priority for a particular vap
 */
static int wlan_cfg80211_get_tidmap_prty(struct wiphy *wiphy,
                struct wireless_dev *wdev,
                struct wlan_cfg8011_genric_params *params)
{
    wlan_if_t vap = NULL;
    int retval = 0, value;

    vap = wlan_cfg80211_extract_vap_intf(wiphy, wdev);
    if (!vap) {
        qdf_err("Command on Invalid Interface");
        return -EINVAL;
    }

    value = wlan_get_vap_tidmap_prty(vap);
    if (value < 0)
        retval = -EOPNOTSUPP;

    cfg80211_reply_command(wiphy, sizeof(u_int32_t), &value, 0);
    return retval;
}

/*
 * wlan_cfg80211_set_tidmap_tbl_id:
 * Sets tidmap table_id for a particular vap
 */
static int wlan_cfg80211_set_tidmap_tbl_id(struct wiphy *wiphy,
                struct wireless_dev *wdev,
                struct wlan_cfg8011_genric_params *params)
{
    wlan_if_t vap = NULL;
    int retval = 0;
    uint32_t mapid;

    vap = wlan_cfg80211_extract_vap_intf(wiphy, wdev);
    if (!vap) {
        qdf_err("Command on Invalid Interface");
        return -EINVAL;
    }
    mapid = params->value;
    retval = wlan_set_vap_tidmap_tbl_id(vap, mapid);

    return retval;
}

/*
 * wlan_cfg80211_get_tidmap_tbl_id:
 * Gets tidmap table_id for a particular vap
 */
static int wlan_cfg80211_get_tidmap_tbl_id(struct wiphy *wiphy,
                struct wireless_dev *wdev,
                struct wlan_cfg8011_genric_params *params)
{
    wlan_if_t vap = NULL;
    int retval = 0, value;

    vap = wlan_cfg80211_extract_vap_intf(wiphy, wdev);
    if (!vap) {
        qdf_err("Command on Invalid Interface");
        return -EINVAL;
    }
    value = wlan_get_vap_tidmap_tbl_id(vap);
    if (value < 0)
        retval = -EOPNOTSUPP;

    cfg80211_reply_command(wiphy, sizeof(u_int32_t), &value, 0);
    return retval;
}

static int wlan_cfg80211_set_btcoex_duty_cycle(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        struct wlan_cfg8011_genric_params *params)
{
    struct ieee80211com *ic = NULL;
    struct ol_ath_softc_net80211 *scn = NULL;
    int ret = 0;
    uint32_t period, duration;

    scn = wlan_cfg80211_extract_radio_intf(wiphy, wdev);
    if (!scn) {
        qdf_err(" Command on Invalid interface");
        return -EINVAL;
    }
    ic = &scn->sc_ic;

    period = params->value;
    duration = params->length;
    if ( ic->ic_cfg80211_radio_handler.set_btcoex_duty_cycle ) {
        ret = ic->ic_cfg80211_radio_handler.set_btcoex_duty_cycle(scn, period, duration);
    }
    else
        ret = -EOPNOTSUPP;

    return ret;
}

static const struct nla_policy
wlan_cfg80211_set_gpioconfig_policy[QCA_WLAN_VENDOR_ATTR_GPIO_PARAM_MAX + 1] = {
    [QCA_WLAN_VENDOR_ATTR_GPIO_PARAM_COMMAND] = {.type = NLA_U32 },
    [QCA_WLAN_VENDOR_ATTR_GPIO_PARAM_PINNUM] = {.type = NLA_U32 },
    [QCA_WLAN_VENDOR_ATTR_GPIO_PARAM_VALUE] = {.type = NLA_U32 },
    [QCA_WLAN_VENDOR_ATTR_GPIO_PARAM_PULL_TYPE] = {.type = NLA_U32 },
    [QCA_WLAN_VENDOR_ATTR_GPIO_PARAM_INTR_MODE] = {.type = NLA_U32 },
};

/**
 * wlan_cfg80211_get_pri20_blockchanlist : Get list of channels blocked for
 * usage as primary 20 MHz (applicable only for channel selection algorithms)
 * @wiphy: pointer to wiphy
 * @wdev: pointer to wireless_wdev
 * @params: pointer to wlan_cfg8011_genric_params
 *
 * Return: 0 for success or error code
 */
static int wlan_cfg80211_get_pri20_blockchanlist(struct wiphy *wiphy,
            struct wireless_dev *wdev,
            struct wlan_cfg8011_genric_params *params)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    int cmd_type = -1;
    void *radioctx = NULL;
    uint32_t source_array_size = 0;

    /*
     * QCA_NL80211_VENDOR_SUBCMD_GET_PRI20_BLOCKCHANLIST is currently expected
     * to return IEEE channel numbers sized one octet each if the block channel
     * list is non-empty. Enforce the one octet sizing.
     */
    qdf_assert_always(sizeof(ic->ic_pri20_cfg_blockchanlist.chan[0]) == 1);

    if (wiphy == NULL) {
        qdf_err("NULL wiphy argument.");
        return -EINVAL;
    }

    if (wdev == NULL) {
        qdf_err("NULL wdev argument.");
        return -EINVAL;
    }

    if (params == NULL) {
        qdf_err("NULL params argument.");
        return -EINVAL;
    }

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    if (cfg_ctx == NULL) {
        qdf_err("NULL cfg80211 context obtained from wiphy. Investigate.");
        return -EINVAL;
    }

    ic = cfg_ctx->ic;
    if (ic == NULL) {
        qdf_err("NULL ic obtained from cfg80211 context. Investigate.");
        return -EINVAL;
    }

    radioctx = extract_command(ic, wdev, &cmd_type);

    /*
     * Additional sanity checks for the command. Note that though we don't need
     * the radio context obtained above, we sanity check it as a precaution.
     */
    if (radioctx == NULL) {
        qdf_err("NULL radio context obtained from command. Sanity check "
                "failed. Investigate.");
        return -EINVAL;
    }

    if (cmd_type != RADIO_CMD) {
        qdf_err("Command is not on radio interface");
        return -EINVAL;
    }

    source_array_size = QDF_ARRAY_SIZE(ic->ic_pri20_cfg_blockchanlist.chan);
    if (ic->ic_pri20_cfg_blockchanlist.n_chan > source_array_size) {
        qdf_err("Number of channels indicated in source blocked chan list %hhu "
                "exceeds source blocked chan list array size %u.",
                ic->ic_pri20_cfg_blockchanlist.n_chan, source_array_size);
        return -EINVAL;
    }

    if (ic->ic_pri20_cfg_blockchanlist.n_chan) {
        return cfg80211_reply_command(wiphy,
                    ic->ic_pri20_cfg_blockchanlist.n_chan *
                        sizeof(ic->ic_pri20_cfg_blockchanlist.chan[0]),
                    (void*)(&(ic->ic_pri20_cfg_blockchanlist.chan[0])), 0);
    } else {
        return cfg80211_reply_command(wiphy, 0, NULL, 0);
    }
}

/**
 * wlan_cfg80211_set_gpio_config :set the gpio configuration
 * @wiphy: pointer to wiphy
 * @wdev: pointer to wireless_wdev
 * @data: pointer to data
 * @data_len: data length
 */

#define IEEE80211_GPIO_CONFIG 0
#define IEEE80211_GPIO_OUTPUT 1

static int wlan_cfg80211_set_gpio_config(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        const void *data,
        int data_len)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    int command = 0,gpionum = 0, input = 0, pulltype = 0, intrmode = 0;
    struct ol_ath_softc_net80211 *scn = NULL;
    struct nlattr *attr[QCA_WLAN_VENDOR_ATTR_CONFIG_MAX + 1];
    void *cmd;
    int cmd_type = 0;
    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    if ((data == NULL) || (!data_len)) {
        qdf_print("%s: invalid data length data ptr: %pK ", __func__, data);
        return -1;
    }
    cmd = extract_command(ic, wdev, &cmd_type);
    if (!cmd_type) {
        qdf_print("Invalid VAP Command !");
        return -EINVAL;
    } else {
        scn = (struct ol_ath_softc_net80211 *)cmd;
    }
    if (nla_parse(attr, QCA_WLAN_VENDOR_ATTR_GPIO_PARAM_MAX,
                data, data_len,
                wlan_cfg80211_set_gpioconfig_policy)) {
        return -EINVAL;
    }

    if (attr[QCA_WLAN_VENDOR_ATTR_GPIO_PARAM_COMMAND]) {
        command = nla_get_u32(
                attr[QCA_WLAN_VENDOR_ATTR_GPIO_PARAM_COMMAND]);
    }
    if(command == IEEE80211_GPIO_CONFIG) {
        if (attr[QCA_WLAN_VENDOR_ATTR_GPIO_PARAM_PINNUM]) {
            gpionum = nla_get_u32(
                    attr[QCA_WLAN_VENDOR_ATTR_GPIO_PARAM_PINNUM]);
        }
        if (attr[QCA_WLAN_VENDOR_ATTR_GPIO_PARAM_VALUE]) {
            input = nla_get_u32(
                    attr[QCA_WLAN_VENDOR_ATTR_GPIO_PARAM_VALUE]);
        }
        if (attr[QCA_WLAN_VENDOR_ATTR_GPIO_PARAM_PULL_TYPE]) {
            pulltype = nla_get_u32(
                    attr[QCA_WLAN_VENDOR_ATTR_GPIO_PARAM_PULL_TYPE]);
        }
        if (attr[QCA_WLAN_VENDOR_ATTR_GPIO_PARAM_INTR_MODE]) {
            intrmode = nla_get_u32(
                    attr[QCA_WLAN_VENDOR_ATTR_GPIO_PARAM_INTR_MODE]);
        }
	if ( ic->ic_cfg80211_radio_handler.gpio_config )
	    ic->ic_cfg80211_radio_handler.gpio_config((void*)scn, gpionum, input, pulltype, intrmode);
        else
            return -EOPNOTSUPP;

    } else if(command == IEEE80211_GPIO_OUTPUT) {
        if (attr[QCA_WLAN_VENDOR_ATTR_GPIO_PARAM_PINNUM]) {
            gpionum = nla_get_u32(
                    attr[QCA_WLAN_VENDOR_ATTR_GPIO_PARAM_PINNUM]);
        }
        if (attr[QCA_WLAN_VENDOR_ATTR_GPIO_PARAM_VALUE]) {
            input = nla_get_u32(
                    attr[QCA_WLAN_VENDOR_ATTR_GPIO_PARAM_VALUE]);
        }
        if ( ic->ic_cfg80211_radio_handler.gpio_output )
	    ic->ic_cfg80211_radio_handler.gpio_output((void*)scn, gpionum, input);
        else
            return -EOPNOTSUPP;

    } else {
        qdf_print("\n Invalid command");
        return -EINVAL;
    }
    return 0;
}

static int wlan_cfg80211_set_mgmt_rssi_thr(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        struct wlan_cfg8011_genric_params *params)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    int cmd_type ,ret = 0;
    void *cmd;
    int value = 0;

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    cmd = extract_command(ic, wdev, &cmd_type);

    if (!cmd_type) {
        qdf_print(" Command on Invalid interface");
        return -EINVAL;
    }

    value = *(int *)(params->data);

    if (value <= RSSI_MIN || value > RSSI_MAX) {
        qdf_print("invalid value: %d, RSSI is between 1-127 ", value);
        return -EINVAL;
    }
    ic->mgmt_rx_rssi = value;

    qdf_print("%s: value=%d", __func__, value);

    return ret;
}

static int wlan_cfg80211_get_bssid(struct wiphy *wiphy,
            struct wireless_dev *wdev,
            struct wlan_cfg8011_genric_params *params)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    wlan_if_t vap = NULL;
    int cmd_type ,ret = 0;
    void *cmd;
    u_int8_t bssid[IEEE80211_ADDR_LEN];
    struct ether_addr *data = (struct ether_addr *) &params->data;
    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;
    cmd = extract_command(ic, wdev, &cmd_type);
    if (cmd_type) {
        qdf_print(" Command on invalid interface");
        return -EINVAL;
    } else {
        vap = (wlan_if_t)cmd;
    }
    wlan_vap_get_bssid(vap, bssid);
    qdf_mem_copy(&data->octet,bssid,IEEE80211_ADDR_LEN);
    cfg80211_reply_command(wiphy, IEEE80211_ADDR_LEN, &bssid, 0);

    return ret;
}

static int wlan_cfg80211_get_mgmt_rssi_thr(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        struct wlan_cfg8011_genric_params *params)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    int cmd_type ,ret = 0;
    void *cmd;

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    cmd = extract_command(ic, wdev, &cmd_type);
    if (!cmd_type) {
        qdf_print(" Command on Invalid interface");
        return -EINVAL;
    }

    cfg80211_reply_command(wiphy, 4, &(ic->mgmt_rx_rssi), 0);

    return ret;
}

static int wlan_cfg80211_get_wmm_params(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        struct wlan_cfg8011_genric_params *params)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    osif_dev *osifp = NULL;
    wlan_if_t vap = NULL;
    int cmd_type;
    int wmmparam, ac, bss, value;

    void *cmd;
    u_int32_t *data = (u_int32_t *) params->data;

    if(data == NULL) {
        qdf_print("Invalid Arguments ");
        return -EINVAL;
    }

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    cmd = extract_command(ic, wdev, &cmd_type);
    if (cmd_type) {
        qdf_print(" Command on invalid interface");
        return -EINVAL;
    } else {
        vap = (wlan_if_t)cmd;
        osifp = ath_netdev_priv(wdev->netdev);
    }

    wmmparam = params->value;
    ac = (data[0] < WME_NUM_AC) ? data[0] : WME_AC_BE;
    bss = params->length;
    value = ieee80211_ucfg_getwmmparams((void *)osifp, wmmparam, ac, bss);
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_CFG80211, "%s: wmmparam: %d ac: %d bss: %d value: %d \n", __func__, wmmparam, ac, bss, value);
    return cfg80211_reply_command(wiphy, sizeof(u_int32_t), &value, 0);

}

/*
 * wlan_cfg80211_set_block_mgt:
 * Enables blocking mgmt frames from a particular mac
 */
static int wlan_cfg80211_set_block_mgt(struct wiphy *wiphy,
                struct wireless_dev *wdev,
                struct wlan_cfg8011_genric_params *params)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;

    wlan_if_t vap = NULL;
    int cmd_type, retval = 0;
    void *cmd;
    uint8_t mac[IEEE80211_ADDR_LEN];

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    if (cfg_ctx == NULL) {
        qdf_print("Invalid Context\n");
        return -EINVAL;
    }

    ic = cfg_ctx->ic;
    if (ic == NULL) {
        qdf_print("Invalid Interface\n");
        return -EINVAL;
    }

    cmd = extract_command(ic, wdev, &cmd_type);
    if (cmd_type) {
        qdf_print("Command on Invalid Interface\n");
        return -EINVAL;
    }
    vap = (wlan_if_t)cmd;

    if(params->data_len != IEEE80211_ADDR_LEN) {
        qdf_print("Invalid MAC Address!\n");
        return -EINVAL;
    }
    qdf_mem_copy(mac, params->data, IEEE80211_ADDR_LEN);

    retval = wlan_set_acl_block_mgmt(vap, mac, TRUE);

    return retval;
}

/*
 * wlan_cfg80211_clr_block_mgt:
 * Disables blocking mgmt frames from a particular mac
 */
static int wlan_cfg80211_clr_block_mgt(struct wiphy *wiphy,
                struct wireless_dev *wdev,
                struct wlan_cfg8011_genric_params *params)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;

    wlan_if_t vap = NULL;
    int cmd_type, retval = 0;
    void *cmd;
    uint8_t mac[IEEE80211_ADDR_LEN];

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    if (cfg_ctx == NULL) {
        qdf_print("Invalid Context\n");
        return -EINVAL;
    }

    ic = cfg_ctx->ic;
    if (ic == NULL) {
        qdf_print("Invalid Interface\n");
        return -EINVAL;
    }

    cmd = extract_command(ic, wdev, &cmd_type);
    if (cmd_type) {
        qdf_print("Command on Invalid Interface\n");
        return -EINVAL;
    }
    vap = (wlan_if_t)cmd;

    if(params->data_len != IEEE80211_ADDR_LEN) {
        qdf_print("Invalid MAC Address!\n");
        return -EINVAL;
    }
    qdf_mem_copy(mac, params->data, IEEE80211_ADDR_LEN);

    retval = wlan_set_acl_block_mgmt(vap, mac, FALSE);

    return retval;
}

/*
 * wlan_cfg80211_get_block_mgt:
 * Get the list of addresses for which blocking mgmt frames is set
 */
static int wlan_cfg80211_get_block_mgt(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        struct wlan_cfg8011_genric_params *params)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    osif_dev *osifp = NULL;
    wlan_if_t vap = NULL;
    int cmd_type;
    void *cmd;
    u_int8_t mac[IEEE80211_ADDR_LEN];
    u_int8_t *mac_list;
    int i, rc, num_mac;

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    cmd = extract_command(ic, wdev, &cmd_type);
    if (cmd_type) {
        qdf_print(" Command on invalid interface\n");
        return -EINVAL;
    } else {
        osifp = ath_netdev_priv(wdev->netdev);
        vap = (wlan_if_t)cmd;
    }

    /* read and send mac list to application */
    mac_list = (u_int8_t *)OS_MALLOC(osifp->os_handle,
                                     (IEEE80211_ADDR_LEN * 256), GFP_KERNEL);
    if (!mac_list)
        return -EFAULT;

    rc = wlan_get_acl_list(vap, mac_list, (IEEE80211_ADDR_LEN * 256),
                           &num_mac, IEEE80211_ACL_FLAG_ACL_LIST_1);
    if (rc) {
        OS_FREE(mac_list);
        return -EFAULT;
    }

    for (i = 0; i < num_mac; i++) {
        if (wlan_acl_is_block_mgmt_set(vap,
                                       &mac_list[i * IEEE80211_ADDR_LEN])) {
            memcpy(&mac, &mac_list[i * IEEE80211_ADDR_LEN], IEEE80211_ADDR_LEN);
            cfg80211_reply_command(wiphy, IEEE80211_ADDR_LEN, &mac, 0);
        }
    }
    OS_FREE(mac_list);
    return 0;
}

static int wlan_cfg80211_addie(struct wiphy *wiphy, struct wireless_dev *wdev,
        struct wlan_cfg8011_genric_params *params)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    wlan_if_t vap = NULL;
    int cmd_type;
    void *cmd;
    struct ieee80211_wlanconfig_ie *ie_buffer;

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    cmd = extract_command(ic, wdev, &cmd_type);
    if (cmd_type) {
        QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY,
                       QDF_TRACE_LEVEL_ERROR, "Command on invalid interface\n");
        return -EINVAL;
    } else {
        vap = (wlan_if_t)cmd;
    }

    ie_buffer = (struct ieee80211_wlanconfig_ie *)params->data;

    return ieee80211_ucfg_addie(vap, ie_buffer);
}

/*
 * @wiphy:   pointer to wireless wiphy structure.
 * @wdev:    pointer to wireless_dev structure.
 * @data:    Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed 1895
 *
 * Return: 0 on success, negative errno on failure
 */
int wlan_cfg80211_set_lat_capture(struct wiphy *wiphy,
        struct wireless_dev *wdev, const void *data, int data_len)
{
    void *cmd;
    int cmd_type, k, ret;
    struct ieee80211com *ic = NULL;
    ol_txrx_soc_handle soc_txrx_handle;
    ol_txrx_pdev_handle pdev_txrx_handle;
    struct cfg80211_context *cfg_ctx = NULL;
    struct wlan_objmgr_psoc *psoc;
    wlan_if_t vap = NULL;

    ret = sscanf((char *)data, "%d", &k);
    if ((ret != 1) || ((k != 0) && (k != 1))) {
        qdf_err("Wrong input");
        return -EINVAL;
    }
    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;
    cmd = extract_command(ic, wdev, &cmd_type);
    if (cmd_type) {
        qdf_err("Command on invalid interface !");
        return -EINVAL;
    } else {
        vap = (wlan_if_t)cmd;
        psoc = wlan_pdev_get_psoc(vap->iv_ic->ic_pdev_obj);
        soc_txrx_handle = wlan_psoc_get_dp_handle(psoc);
        pdev_txrx_handle = wlan_pdev_get_dp_handle(vap->iv_ic->ic_pdev_obj);
        cdp_txrx_set_pdev_param(soc_txrx_handle,
                (struct cdp_pdev *)pdev_txrx_handle,
                CDP_CONFIG_CAPTURE_LATENCY, k);
    }
    return 0;
}


#if UMAC_SUPPORT_ACFG
/*
 * @wiphy:   pointer to wireless wiphy structure.
 * @wdev:    pointer to wireless_dev structure.
 * @data:    Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed 1895
 *
 * Return: 0 on success, negative errno on failure
 */
static int
wlan_cfg80211_acfg(struct wiphy *wiphy, struct wireless_dev *wdev, const void *data, int data_len)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    struct net_device *dev = NULL;
    int err = 0;

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    if (ic->ic_wdev.netdev == wdev->netdev) {
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_IOCTL,
                "%s: Radio Command\n", __func__);
        dev = wdev->netdev;
        if ((dev != NULL) && (data != NULL)){
            err = ic->ic_acfg_cmd(dev, (void *)data);
            if (!err) {
                err = cfg80211_reply_command(wiphy, sizeof(acfg_os_req_t), (void *)data, 0);
            }
        }
    } else {
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_IOCTL,
                "%s: VAP Command\n", __func__);
        dev = wdev->netdev;
        if ((dev != NULL) && (data != NULL)) {
            err = acfg_handle_vap_config(dev, (void *)data);
            if (!err) {
                err = cfg80211_reply_command(wiphy, sizeof(acfg_os_req_t), (void *)data, 0);
            }
        }
    }

    return err;
}
#endif

#ifdef WLAN_SUPPORT_RX_PROTOCOL_TYPE_TAG
#ifdef WLAN_SUPPORT_RX_TAG_STATISTICS
/*
 * wlan_cfg80211_dump_rx_pkt_protocol_tag_stats:
 * Get the protocol tag statistics for the given protocol type. If protocol
 * type is 255, dump tag statistics for all the supported protocol types.
 */
static int wlan_cfg80211_dump_rx_pkt_protocol_tag_stats(struct wiphy *wiphy, struct wireless_dev *wdev,
        struct wlan_cfg8011_genric_params *params)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    struct net_device *dev = NULL;
    struct ol_ath_softc_net80211 *scn = NULL;
    int return_val = -EOPNOTSUPP;
    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    qdf_info("Protocol Tag Stats: PKT_TYPE = %u\n", params->value);

    if (ic->ic_wdev.netdev == wdev->netdev) {
        dev = wdev->netdev;
        scn =  ath_netdev_priv(dev);
        if (ic->ic_cfg80211_radio_handler.ic_dump_rx_pkt_protocol_tag_stats)
            return_val = ic->ic_cfg80211_radio_handler.ic_dump_rx_pkt_protocol_tag_stats((void*)scn, params->value);
    }
    else
        return_val = -EOPNOTSUPP;

    return return_val;

}
#endif //WLAN_SUPPORT_RX_TAG_STATISTICS

/*
 * wlan_cfg80211_rx_pkt_protocol_tag:
 * Add/delete given tag (or metadata) for the given protocol type. The
 * programmed metadata is tagged on the QDF packet if the received packet
 * type matches the programmed packet type.
 */
static int wlan_cfg80211_rx_pkt_protocol_tag(struct wiphy *wiphy, struct wireless_dev *wdev,
        struct wlan_cfg8011_genric_params *params)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    struct net_device *dev = NULL;
    struct ol_ath_softc_net80211 *scn = NULL;
    struct ieee80211_rx_pkt_protocol_tag protocol_tag_info;
    int return_val = -EOPNOTSUPP;
    uint32_t *data_ptr = (uint32_t *)params->data;
    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    protocol_tag_info.op_code = params->value;
    protocol_tag_info.pkt_type = data_ptr[0];
    protocol_tag_info.pkt_type_metadata = params->length;

    qdf_info("Protocol Tag Info: OPCODE = %d, PKT_TYPE = %u, METADATA = %u\n",
            protocol_tag_info.op_code, protocol_tag_info.pkt_type, protocol_tag_info.pkt_type_metadata);

    if (ic->ic_wdev.netdev == wdev->netdev) {
        dev = wdev->netdev;
        scn =  ath_netdev_priv(dev);
        if (ic->ic_cfg80211_radio_handler.ic_set_rx_pkt_protocol_tagging)
            return_val = ic->ic_cfg80211_radio_handler.ic_set_rx_pkt_protocol_tagging((void*)scn, &protocol_tag_info);
    }
    else
        return_val = -EOPNOTSUPP;

    return return_val;
}
#endif //WLAN_SUPPORT_RX_PROTOCOL_TYPE_TAG

static int wlan_cfg80211_get_wificonfiguration(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        const void *data,
        int data_len)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    struct wlan_cfg8011_genric_params generic_params;
    int ret = 0;

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;
    if (ic->recovery_in_progress) {
        return -1;
    }

    qdf_mem_zero(&generic_params, sizeof(generic_params));
    extract_generic_command_params(wiphy, data, data_len, &generic_params);

    /* call extract_generic_command_params */
    switch(generic_params.command) {
        case QCA_NL80211_VENDOR_SUBCMD_WIFI_PARAMS:
            wlan_cfg80211_get_params(wiphy, wdev, &generic_params);
            break;
        case QCA_NL80211_VENDORSUBCMD_WIRELESS_MODE:
            wlan_cfg80211_get_wirelessmode(wiphy, wdev, &generic_params);
            break;
        case QCA_NL80211_VENDORSUBCMD_WMM_PARAMS:
            wlan_cfg80211_get_wmm_params(wiphy, wdev, &generic_params);
            break;
        case QCA_NL80211_VENDORSUBCMD_BSSID:
            wlan_cfg80211_get_bssid(wiphy, wdev, &generic_params);
            break;
        case QCA_NL80211_VENDORSUBCMD_COUNTRY_CONFIG:
            wlan_cfg80211_get_country(wiphy, wdev, &generic_params);
            break;
        case QCA_NL80211_VENDORSUBCMD_MGMT_RSSI_THR:
            wlan_cfg80211_get_mgmt_rssi_thr(wiphy, wdev, &generic_params);
            break;
        case QCA_NL80211_VENDOR_SUBCMD_HE_BSS_COLOR:
            wlan_cfg80211_get_he_bss_color(wiphy, wdev, &generic_params);
            break;
        case QCA_NL80211_VENDOR_SUBCMD_HE_SR_CONFIG:
            wlan_cfg80211_get_he_sr_config(wiphy, wdev, &generic_params);
            break;
        case QCA_NL80211_VENDOR_SUBCMD_MU_EDCA_PARAMS:
            wlan_cfg80211_get_muedca_params(wiphy, wdev, &generic_params);
            break;
        case QCA_NL80211_VENDORSUBCMD_GET_SSID:
            wlan_cfg80211_get_ssid(wiphy, wdev, &generic_params);
            break;
#if UNIFIED_SMARTANTENNA
        case QCA_NL80211_VENDOR_SUBCMD_SA_PARAMS:
            ret = wlan_cfg80211_get_sa_params(wiphy, wdev, &generic_params);
            break;
#endif
        case QCA_NL80211_VENDORSUBCMD_DEFAULT_PCP_TID_MAP:
            wlan_cfg80211_get_def_pcp_tid_map(wiphy, wdev, &generic_params);
            break;
        case QCA_NL80211_VENDORSUBCMD_DEFAULT_TIDMAP_PRTY:
           wlan_cfg80211_get_def_tidmap_prty(wiphy, wdev, &generic_params);
            break;
        case QCA_NL80211_VENDORSUBCMD_PCP_TID_MAP:
            wlan_cfg80211_get_pcp_tid_map(wiphy, wdev, &generic_params);
            break;
	case QCA_NL80211_VENDORSUBCMD_TIDMAP_PRTY:
            wlan_cfg80211_get_tidmap_prty(wiphy, wdev, &generic_params);
            break;
        case QCA_NL80211_VENDORSUBCMD_TIDMAP_TBL_ID:
            wlan_cfg80211_get_tidmap_tbl_id(wiphy, wdev, &generic_params);
            break;
        case QCA_NL80211_VENDOR_SUBCMD_GET_PRI20_BLOCKCHANLIST:
            wlan_cfg80211_get_pri20_blockchanlist(wiphy, wdev, &generic_params);
            break;
        default:
            qdf_print("%s: Unsuported Genric command: %d ", __func__, generic_params.command);
    }

    return ret;
}


static int wlan_cfg80211_set_wificonfiguration(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        const void *data,
        int data_len)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    struct wlan_cfg8011_genric_params generic_params;
    int return_value = 0;

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;
    if (ic->recovery_in_progress) {
        return -1;
    }

    qdf_mem_zero(&generic_params, sizeof(generic_params));
    extract_generic_command_params(wiphy, data, data_len, &generic_params);

    /* call extract_generic_command_params */
    switch(generic_params.command) {
        case QCA_NL80211_VENDOR_SUBCMD_WIFI_PARAMS:
            if (generic_params.data == NULL) {
                qdf_print("%s: Unsupported Arguments ", __func__);
                return -EINVAL;
            }
            return_value = wlan_cfg80211_set_params(wiphy, wdev, &generic_params);
            break;
        case QCA_NL80211_VENDOR_SUBCMD_NAWDS_PARAMS:
            return_value = wlan_cfg80211_nawds_params(wiphy, wdev, generic_params.data, generic_params.data_len);
            break;
        case QCA_NL80211_VENDOR_SUBCMD_HMWDS_PARAMS:
            return_value = wlan_cfg80211_hmwds_params(wiphy, wdev, generic_params.data, generic_params.data_len);
            break;
        case QCA_NL80211_VENDOR_SUBCMD_ALD_PARAMS:
            return_value = wlan_cfg80211_ald_params(wiphy, wdev, generic_params.data, generic_params.data_len);
            break;
#if QCA_AIRTIME_FAIRNESS
        case QCA_NL80211_VENDOR_SUBCMD_ATF:
            return_value = wlan_cfg80211_atf(wiphy, wdev, generic_params.data, generic_params.data_len);
            break;
#endif
        case QCA_NL80211_VENDOR_SUBCMD_WNM:
            return_value = wlan_cfg80211_wnm_params(wiphy, wdev, generic_params.data, generic_params.data_len);
            break;
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
        case QCA_NL80211_VENDOR_SUBCMD_HMMC:
            return_value = wlan_cfg80211_hmmc_params(wiphy, wdev, generic_params.data, generic_params.data_len);
            break;
#endif
        case QCA_NL80211_VENDOR_SUBCMD_SET_MAXRATE:
            return_value = wlan_cfg80211_set_maxrate(wiphy, wdev, generic_params.data, generic_params.data_len);
            break;
#if ATH_SUPPORT_DYNAMIC_VENDOR_IE
        case QCA_NL80211_VENDOR_SUBCMD_VENDORIE:
            return_value = wlan_cfg80211_vendorie_params(wiphy, wdev, generic_params.data, generic_params.data_len);
            break;
#endif
#if ATH_SUPPORT_NAC
        case QCA_NL80211_VENDOR_SUBCMD_NAC:
            return_value = wlan_cfg80211_nac_params(wiphy, wdev, generic_params.data, generic_params.data_len);
            break;
#endif
        case QCA_NL80211_VENDOR_SUBCMD_LIST_SCAN:
            return_value = wlan_cfg80211_list_scan(wiphy, wdev, generic_params.data, generic_params.data_len);
            break;
        case QCA_NL80211_VENDOR_SUBCMD_LIST_CAP:
            return_value = wlan_cfg80211_list_cap(wiphy, wdev, generic_params.data, generic_params.data_len);
            break;
        case QCA_NL80211_VENDOR_SUBCMD_LIST_STA:
            return_value = wlan_cfg80211_list_sta(wiphy, wdev, generic_params.data, generic_params.data_len);
            break;
        case QCA_NL80211_VENDOR_SUBCMD_ACTIVECHANLIST:
            return_value = wlan_cfg80211_active_chan_list(wiphy, wdev, generic_params.data, generic_params.data_len);
            break;
        case QCA_NL80211_VENDOR_SUBCMD_LIST_CHAN:
            return_value = wlan_cfg80211_list_chan(wiphy, wdev, generic_params.data, generic_params.data_len);
            break;
        case QCA_NL80211_VENDOR_SUBCMD_LIST_CHAN160:
            return_value = wlan_cfg80211_list_chan160(wiphy, wdev, generic_params.data, generic_params.data_len);
            break;
        case QCA_NL80211_VENDOR_SUBCMD_DBGREQ:
            return_value = wlan_cfg80211_dbgreq(wiphy, wdev, generic_params.data, generic_params.data_len);
            break;
        case QCA_NL80211_VENDOR_SUBCMD_PHYSTATS:
            return_value = wlan_cfg80211_phystats(wiphy, wdev, generic_params.data, generic_params.data_len);
            break;
        case QCA_NL80211_VENDOR_SUBCMD_ATHSTATS:
            return_value = wlan_cfg80211_athstats(wiphy, wdev, generic_params.data, generic_params.data_len);
            break;
        case QCA_NL80211_VENDOR_SUBCMD_EXTENDEDSTATS:
            return_value = wlan_cfg80211_extendedstats(wiphy, wdev, generic_params.data, generic_params.data_len);
            break;
        case QCA_NL80211_VENDOR_SUBCMD_80211STATS:
            return_value = wlan_cfg80211_80211stats(wiphy, wdev, generic_params.data, generic_params.data_len);
            break;
        case QCA_NL80211_VENDOR_SUBCMD_STA_STATS:
            return_value = wlan_cfg80211_sta_stats(wiphy, wdev, generic_params.data, generic_params.data_len);
            break;
        case QCA_NL80211_VENDOR_SUBCMD_CLONEPARAMS:
            return_value = wlan_cfg80211_cloneparams(wiphy, wdev, generic_params.data, generic_params.data_len);
            break;
        case QCA_NL80211_VENDORSUBCMD_ADDMAC:
            return_value = wlan_cfg80211_addmac(wiphy, wdev, &generic_params);
            break;
        case QCA_NL80211_VENDORSUBCMD_DELMAC:
            return_value = wlan_cfg80211_delmac(wiphy, wdev, &generic_params);
            break;
        case QCA_NL80211_VENDORSUBCMD_KICKMAC:
            return_value = wlan_cfg80211_kickmac(wiphy, wdev, &generic_params);
            break;
#if WLAN_DFS_CHAN_HIDDEN_SSID
        case QCA_NL80211_VENDORSUBCMD_CONF_BSSID:
            return_value = wlan_cfg80211_conf_bssid(wiphy, wdev, &generic_params);
            break;
        case QCA_NL80211_VENDORSUBCMD_GET_CONF_BSSID:
            return_value = wlan_cfg80211_get_conf_bssid(wiphy, wdev, &generic_params);
            break;
#endif /* WLAN_DFS_CHAN_HIDDEN_SSID */
        case QCA_NL80211_VENDORSUBCMD_CHAN_WIDTHSWITCH:
            return_value = wlan_cfg80211_chan_widthswitch(wiphy, wdev, &generic_params);
            break;
        case QCA_NL80211_VENDORSUBCMD_WIRELESS_MODE:
            return_value = wlan_cfg80211_set_wirelessmode(wiphy, wdev, &generic_params);
            break;
        case QCA_NL80211_VENDORSUBCMD_AC_PARAMS:
            return_value = wlan_cfg80211_set_acparams(wiphy, wdev, &generic_params);
            break;
        case QCA_NL80211_VENDORSUBCMD_RC_PARAMS_SETRTPARAMS:
            return_value = wlan_cfg80211_setrtparams(wiphy, wdev, &generic_params);
            break;
        case QCA_NL80211_VENDORSUBCMD_SETFILTER:
            return_value = wlan_cfg80211_set_filter(wiphy, wdev, &generic_params);
            break;
        case QCA_NL80211_VENDORSUBCMD_SEND_MGMT:
            return_value = wlan_cfg80211_send_mgmt(wiphy, wdev, &generic_params);
            break;
        case QCA_NL80211_VENDORSUBCMD_MAC_COMMANDS:
            return_value = wlan_cfg80211_mac_commands(wiphy, wdev, &generic_params);
            break;
        case QCA_NL80211_VENDORSUBCMD_WMM_PARAMS:
            return_value = wlan_cfg80211_set_wmm_params(wiphy, wdev, &generic_params);
            break;
        case QCA_NL80211_VENDORSUBCMD_COUNTRY_CONFIG:
            return_value = wlan_cfg80211_set_country(wiphy, wdev, &generic_params);
            break;
        case QCA_NL80211_VENDORSUBCMD_HWADDR_CONFIG:
            return_value = wlan_cfg80211_set_hwaddr(wiphy, wdev, &generic_params);
            break;
        case QCA_NL80211_VENDORSUBCMD_AGGR_BURST_CONFIG:
            return_value = wlan_cfg80211_set_aggr_burst(wiphy, wdev, &generic_params);
            break;
        case QCA_NL80211_VENDORSUBCMD_ATF_SCHED_DURATION_CONFIG:
            return_value = wlan_cfg80211_set_atf_sched_dur(wiphy, wdev, &generic_params);
            break;
        case QCA_NL80211_VENDORSUBCMD_TXRX_PEER_STATS:
            return_value = wlan_cfg80211_txrx_peer_stats(wiphy, wdev, &generic_params);
            break;
            /* channel configuration for propritery modes */
        case QCA_NL80211_VENDORSUBCMD_CHANNEL_CONFIG:
            return_value = wlan_cfg80211_set_channel(wiphy, wdev, &generic_params);
            break;
            /* ssid configuration for propritery modes */
        case QCA_NL80211_VENDORSUBCMD_SSID_CONFIG:
            return_value = wlan_cfg80211_set_ssid(wiphy, wdev, &generic_params);
            break;
        case QCA_NL80211_VENDORSUBCMD_MGMT_RSSI_THR:
            return_value = wlan_cfg80211_set_mgmt_rssi_thr(wiphy, wdev, &generic_params);
            break;
        case QCA_NL80211_VENDORSUBCMD_GET_SSID:
            return_value= wlan_cfg80211_get_ssid(wiphy, wdev, &generic_params);
            break;
        case QCA_NL80211_VENDOR_SUBCMD_GET_ACLMAC:
            return_value = wlan_cfg80211_get_aclmac(wiphy, wdev, &generic_params);
            break;
        case QCA_NL80211_VENDORSUBCMD_DP_FW_PEER_STATS:
            return_value = wlan_cfg80211_txrx_dp_fw_peer_stats(wiphy, wdev, &generic_params);
            break;
        case QCA_NL80211_VENDORSUBCMD_SET_PEER_NEXTHOP:
            return_value = wlan_cfg80211_set_peer_nexthop(wiphy, wdev, &generic_params);
            break;
        case QCA_NL80211_VENDORSUBCMD_CHECK_11H:
            return_value = wlan_cfg80211_check_11hcap(wiphy, wdev, &generic_params);
			break;
#if ATH_SUPPORT_NAC_RSSI
        case QCA_NL80211_VENDOR_SUBCMD_NAC_RSSI:
            return_value = wlan_cfg80211_nac_rssi_params(wiphy, wdev, generic_params.data, generic_params.data_len);
            break;
#endif
        case QCA_NL80211_VENDOR_SUBCMD_HTTSTATS:
            return_value = wlan_cfg80211_httstats(wiphy, wdev, generic_params.data, generic_params.data_len);
            break;
#if ATH_ACL_SOFTBLOCKING
        case QCA_NL80211_VENDORSUBCMD_ADDMAC_SEC:
            return_value= wlan_cfg80211_addmac_sec(wiphy, wdev, &generic_params);
            break;
        case QCA_NL80211_VENDORSUBCMD_DELMAC_SEC:
            return_value = wlan_cfg80211_delmac_sec(wiphy, wdev, &generic_params);
            break;
        case QCA_NL80211_VENDORSUBCMD_GET_ACLMAC_SEC:
           return_value =  wlan_cfg80211_get_aclmac_sec(wiphy, wdev, &generic_params);
            break;
#endif
        case QCA_NL80211_VENDOR_SUBCMD_SET_SU_SOUNDING_INT:
            return_value = wlan_cfg80211_set_su_sounding_int(wiphy, wdev, &generic_params);
            break;
        case QCA_NL80211_VENDOR_SUBCMD_SET_MU_SOUNDING_INT:
            return_value = wlan_cfg80211_set_mu_sounding_int(wiphy, wdev, &generic_params);
            break;
        case QCA_NL80211_VENDOR_SUBCMD_ENABLE_SOUNDING_INT:
            return_value = wlan_cfg80211_enable_sounding_int(wiphy, wdev, &generic_params);
            break;
        case QCA_NL80211_VENDOR_SUBCMD_SCHED_MU_ENABLE:
            return_value = wlan_cfg80211_sched_mu_enable(wiphy, wdev, &generic_params);
            break;
        case QCA_NL80211_VENDOR_SUBCMD_SCHED_OFDMA_ENABLE:
            return_value = wlan_cfg80211_sched_ofdma_enable(wiphy, wdev, &generic_params);
            break;
        case QCA_NL80211_VENDOR_SUBCMD_HE_BSS_COLOR:
            return_value = wlan_cfg80211_he_bss_color(wiphy, wdev, &generic_params);
            break;
        case QCA_NL80211_VENDOR_SUBCMD_HE_SR_CONFIG:
            return_value = wlan_cfg80211_he_sr_config(wiphy, wdev, &generic_params);
            break;
        case QCA_NL80211_VENDOR_SUBCMD_SET_BA_TIMEOUT:
            return_value = wlan_cfg80211_set_ba_timeout(wiphy, wdev, &generic_params);
            break;
        case QCA_NL80211_VENDOR_SUBCMD_GET_BA_TIMEOUT:
            return_value = wlan_cfg80211_get_ba_timeout(wiphy, wdev, &generic_params);
            break;
        case QCA_NL80211_VENDOR_SUBCMD_MU_EDCA_PARAMS:
            return_value = wlan_cfg80211_set_muedca_params(wiphy, wdev, &generic_params);
            break;
#if ATH_ACS_DEBUG_SUPPORT
        case QCA_NL80211_VENDOR_SUBCMD_ACSDBGTOOL_ADD_BCN:
            return_value = wlan_cfg80211_acsdbgtool_add_bcn(wiphy, wdev, &generic_params);
            break;
        case QCA_NL80211_VENDOR_SUBCMD_ACSDBGTOOL_ADD_CHAN:
            return_value = wlan_cfg80211_acsdbgtool_add_chanev(wiphy, wdev, &generic_params);
            break;
#endif
        case QCA_NL80211_VENDORSUBCMD_RC_PARAMS_SETRATEMASK:
            return_value = wlan_cfg80211_setratemask(wiphy, wdev, &generic_params);
            break;
        case QCA_NL80211_VENDORSUBCMD_SET_BLOCK_MGT:
            return_value = wlan_cfg80211_set_block_mgt(wiphy, wdev, &generic_params);
            break;
        case QCA_NL80211_VENDORSUBCMD_CLR_BLOCK_MGT:
            return_value = wlan_cfg80211_clr_block_mgt(wiphy, wdev, &generic_params);
            break;
        case QCA_NL80211_VENDORSUBCMD_GET_BLOCK_MGT:
            return_value = wlan_cfg80211_get_block_mgt(wiphy, wdev, &generic_params);
            break;
        case QCA_NL80211_VENDOR_SUBCMD_ADDIE:
            return_value = wlan_cfg80211_addie(wiphy, wdev, &generic_params);
            break;
        case QCA_NL80211_VENDORSUBCMD_SET_VLAN_TYPE:
            return_value = wlan_cfg80211_set_vlan_type(wiphy, wdev, &generic_params);
            break;
#if WLAN_CFR_ENABLE
        case QCA_NL80211_VENDOR_SUBCMD_CFR_CONFIG:
            return_value = wlan_cfg80211_cfr_params(wiphy, wdev, generic_params.data, generic_params.data_len);
            break;
#endif
#if UNIFIED_SMARTANTENNA
        case QCA_NL80211_VENDOR_SUBCMD_SA_PARAMS:
            return_value = wlan_cfg80211_set_sa_params(wiphy, wdev, &generic_params);
            break;
#endif
        case QCA_NL80211_VENDORSUBCMD_DEFAULT_PCP_TID_MAP:
            return_value = wlan_cfg80211_set_def_pcp_tid_map(wiphy, wdev, &generic_params);
            break;
        case QCA_NL80211_VENDORSUBCMD_DEFAULT_TIDMAP_PRTY:
            return_value = wlan_cfg80211_set_def_tidmap_prty(wiphy, wdev, &generic_params);
            break;
        case QCA_NL80211_VENDORSUBCMD_PCP_TID_MAP:
            return_value = wlan_cfg80211_set_pcp_tid_map(wiphy, wdev, &generic_params);
            break;
        case QCA_NL80211_VENDORSUBCMD_TIDMAP_PRTY:
            return_value = wlan_cfg80211_set_tidmap_prty(wiphy, wdev, &generic_params);
            break;
        case QCA_NL80211_VENDORSUBCMD_TIDMAP_TBL_ID:
            return_value = wlan_cfg80211_set_tidmap_tbl_id(wiphy, wdev, &generic_params);
            break;
#if UMAC_SUPPORT_ACFG
        case QCA_NL80211_VENDOR_SUBCMD_ACFG:
            return_value = wlan_cfg80211_acfg(wiphy, wdev, generic_params.data, generic_params.data_len);
            break;
#endif
        case QCA_NL80211_VENDOR_SUBCMD_CAPTURE_LATENCY_ENABLE:
            wlan_cfg80211_set_lat_capture(wiphy, wdev, generic_params.data, generic_params.data_len);
            break;
#ifdef WLAN_SUPPORT_RX_PROTOCOL_TYPE_TAG
        case QCA_NL80211_VENDOR_SUBCMD_RX_PKT_PROTOCOL_TAG:
            return_value = wlan_cfg80211_rx_pkt_protocol_tag(wiphy, wdev, &generic_params);
            break;
#ifdef WLAN_SUPPORT_RX_TAG_STATISTICS
        case QCA_NL80211_VENDOR_SUBCMD_RX_PKT_PROTOCOL_TAG_STATS:
            wlan_cfg80211_dump_rx_pkt_protocol_tag_stats(wiphy, wdev, &generic_params);
            break;
#endif //WLAN_SUPPORT_RX_TAG_STATISTICS
#endif //WLAN_SUPPORT_RX_PROTOCOL_TYPE_TAG
        case QCA_NL80211_VENDOR_SUBCMD_SET_BTCOEX_DUTY_CYCLE:
            wlan_cfg80211_set_btcoex_duty_cycle(wiphy, wdev, &generic_params);
            break;
        default:
            qdf_print( "%s: Unsuported Genric command: %d ", __func__, generic_params.command);
            return_value = -EOPNOTSUPP;
    }

    return return_value;
}

static const struct nla_policy
wlan_cfg80211_set_params_policy[QCA_WLAN_VENDOR_ATTR_SETPARAM_MAX + 1] = {

    [QCA_WLAN_VENDOR_ATTR_SETPARAM_COMMAND] = {.type = NLA_U32 },
    [QCA_WLAN_VENDOR_ATTR_SETPARAM_VALUE] = {.type = NLA_U32 },
};

/**
 * wlan_cfg80211_set_params - get driver DFS capability
 * @wiphy:   pointer to wireless wiphy structure.
 * @wdev:    pointer to wireless_dev structure.
 * @data:    Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed
 *
 * Return: 0 on success, negative errno on failure
 */
int wlan_cfg80211_set_params(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        struct wlan_cfg8011_genric_params *params)
{
#define IWPRIV_PARAM_INDEX 0
#define IWPRIV_VALUE_INDEX 1
#define IWPRIV_MAX_INDEX 4
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    struct net_device *dev = NULL;
    osif_dev *osifp = NULL;
    wlan_if_t vap = NULL;
    struct ol_ath_softc_net80211 *scn = NULL;

    int param = params->value;
    u_int32_t *data = (u_int32_t *) params->data;
    int value = *data;
    int extra[IWPRIV_MAX_INDEX];

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    extra[IWPRIV_PARAM_INDEX] = param;
    extra[IWPRIV_VALUE_INDEX] = value;

    switch (param) {
#if ATH_SUPPORT_DSCP_OVERRIDE
        case  IEEE80211_PARAM_DSCP_TID_MAP:
#endif
        case IEEE80211_PARAM_ADD_LOCAL_PEER:
        case IEEE80211_PARAM_ALLOW_DATA:
        case IEEE80211_PARAM_TXPOW_MGMT:
        case IEEE80211_PARAM_WHC_APINFO_OTHERBAND_BSSID:
        case IEEE80211_PARAM_ENABLE_FILS:
        case IEEE80211_PARAM_OCE_WAN_METRICS:
        case IEEE80211_PARAM_TXPOW:
        case IEEE80211_PARAM_HE_SUBFEE_STS_SUPRT:
            extra[IWPRIV_VALUE_INDEX+1] = params->length;
            break;
        default:
            break;
    }

    if (ic->ic_wdev.netdev == wdev->netdev) {
        dev = wdev->netdev;
        scn =  ath_netdev_priv(dev);
        if (ic->ic_cfg80211_radio_handler.setparam)
            return ic->ic_cfg80211_radio_handler.setparam((void*)scn, param, value);
        else
            return -EOPNOTSUPP;
    } else {
        dev = wdev->netdev;
        osifp = ath_netdev_priv(dev);
        vap = osifp->os_if;
        if (vap == NULL) {
            qdf_print("%s: VAP is null ", __func__);
            return -1;
        }
        return ieee80211_ucfg_setparam(vap, param, value ,(char *) &extra);
    }

    return 0;
}

static const struct nla_policy
wlan_cfg80211_get_params_policy[QCA_WLAN_VENDOR_ATTR_SETPARAM_MAX + 1] = {
    [QCA_WLAN_VENDOR_ATTR_SETPARAM_COMMAND] = {.type = NLA_U32 },
};

#if WLAN_CFR_ENABLE
/**
 * wlan_cfg80211_cfr_params - get cfr params
 * @wiphy:   pointer to wireless wiphy structure.
 * @wdev:    pointer to wireless_dev structure.
 * @data:    Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed
 *
 * Return: 0 on success, negative errno on failure
 */
int wlan_cfg80211_cfr_params(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        const void *data,
        int data_len)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    wlan_if_t vap = NULL;
    int cmd_type;
    void *cmd;
    int ret = 0;

    struct ieee80211_wlanconfig *config = (struct ieee80211_wlanconfig *)data;
    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    cmd = extract_command(ic, wdev, &cmd_type);
    if (cmd_type) {
        qdf_info("Invalid Radio Command !");
        return -EINVAL;
    } else {
        vap = (wlan_if_t)cmd;
    }

    ieee80211_ucfg_cfr_params(ic, vap, config);

    return ret;
}
#endif

/**
 * wlan_cfg80211_nawds_params - get nawds params
 * @wiphy:   pointer to wireless wiphy structure.
 * @wdev:    pointer to wireless_dev structure.
 * @data:    Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed
 *
 * Return: 0 on success, negative errno on failure
 */
int wlan_cfg80211_nawds_params(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        const void *data,
        int data_len)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    wlan_if_t vap = NULL;
    int length, cmd_type;
    void *cmd;
    int ret = 0;

    struct ieee80211_wlanconfig *config = (struct ieee80211_wlanconfig *)data;
    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    cmd = extract_command(ic, wdev, &cmd_type);
    if (cmd_type) {
        qdf_print("Invalid Radio Command !");
        return -EINVAL;
    } else {
        vap = (wlan_if_t)cmd;
    }

    ret = ieee80211_ucfg_nawds(vap, config);
    if (config->cmdtype == IEEE80211_WLANCONFIG_NAWDS_GET) {
        length = sizeof(struct ieee80211_wlanconfig);
        /*
         * Allocate memory size of the data being sent + skb header length + u_32 for FLA + u_32
         * for the length of the data being sent
         */
        ret = cfg80211_reply_command(wiphy, length, config, 0);
    }
    return ret;
}

/* wlan_cfg80211_get_params - hmwds param
 * @wiphy:   pointer to wireless wiphy structure.
 * @wdev:    pointer to wireless_dev structure.
 * @data:    Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed
 * Return: 0 on success, negative errno on failure
 */
int wlan_cfg80211_get_params(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        struct wlan_cfg8011_genric_params *params)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    struct net_device *dev = NULL;
    osif_dev *osifp = NULL;
    wlan_if_t vap = NULL;
    struct ol_ath_softc_net80211 *scn = NULL;
    int param = 0, value = 0, return_val = 0;
    int char_value[16] = {0};
    int *data = (int*) params->data;
    int  get_param[2] = {0};
    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;
    param = params->value;
    if(data)
    {
        get_param[0] = param;
        get_param[1] = *data;
    }
    if (ic->ic_wdev.netdev == wdev->netdev) {
        dev = wdev->netdev;
        scn =  ath_netdev_priv(dev);
        if (ic->ic_cfg80211_radio_handler.getparam)
            return_val = ic->ic_cfg80211_radio_handler.getparam((void*)scn, param, &value);
        else
            return_val = -EOPNOTSUPP;
        if (return_val == 0) {
            cfg80211_reply_command(wiphy, sizeof(u_int32_t), &value, 0);
        }
    } else {
        dev = wdev->netdev;
        osifp = ath_netdev_priv(dev);
        vap = osifp->os_if;

        /* special case to handle commands that are defined as char types
           which require different reply length based on return value */
        switch(param)
        {
            case IEEE80211_PARAM_WHC_APINFO_BSSID:
            case IEEE80211_PARAM_WHC_APINFO_CAP_BSSID:
            case IEEE80211_PARAM_WHC_APINFO_BEST_UPLINK_OTHERBAND_BSSID:
            case IEEE80211_PARAM_WHC_APINFO_OTHERBAND_UPLINK_BSSID:
                return_val = ieee80211_ucfg_getparam(vap, param, char_value);
                if (return_val == 0)
                    cfg80211_reply_command(wiphy, 13, &char_value, 0);
                break;
            case IEEE80211_PARAM_DBG_LVL:
            case IEEE80211_PARAM_HE_SUBFEE_STS_SUPRT:
                return_val = ieee80211_ucfg_getparam(vap, param, char_value);
                if (return_val == 0)
                    cfg80211_reply_command(wiphy, 17, &char_value, 0);
                break;
            default:
                /* Use get_param[0] for return value and param[1] for sending the
                   set value for commands that send value while get call.
                */
                return_val = ieee80211_ucfg_getparam(vap, param, get_param);
                value=get_param[0];
                if (return_val == 0)
                    cfg80211_reply_command(wiphy, sizeof(u_int32_t), &value, 0);
        }
    }
    return return_val;
}

/* wlan_cfg80211_hmwds_params - hmwds param
 * @wiphy:   pointer to wireless wiphy structure.
 * @wdev:    pointer to wireless_dev structure.
 * @data:    Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed
 * Return: 0 on success, negative errno on failure
 */

int wlan_cfg80211_hmwds_params(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        const void *data,
        int data_len)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    wlan_if_t vap = NULL;
    u_int8_t ret = 0;
    int cmd_type;
    struct ieee80211_wlanconfig *config = (struct ieee80211_wlanconfig *)data;
    void *cmd;
    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    cmd = extract_command(ic, wdev, &cmd_type);
    if (cmd_type) {
        qdf_print("Invalid Radio Command !");
        return -EINVAL;
    } else {
        vap = (wlan_if_t)cmd;
    }

    ret = ieee80211_ucfg_hmwds(vap, config, data_len);
    if ((config->cmdtype == IEEE80211_WLANCONFIG_HMWDS_READ_TABLE) || (config->cmdtype == IEEE80211_WLANCONFIG_HMWDS_READ_ADDR)) {
        ret = cfg80211_reply_command(wiphy, data_len, config, 0);
    }

    return ret;
}


/* TODO: Vikram - remove this and use IEEE80211_DRIVER_CAPS get param in application */
/* wlan_cfg80211_list_cap - get driver  capability
 * @wiphy:   pointer to wireless wiphy structure.
 * @wdev:    pointer to wireless_dev structure.
 * @data:    Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed
 *
 * Return: 0 on success, negative errno on failure
 */
int wlan_cfg80211_list_cap(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        const void *data,
        int data_len)

{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    wlan_if_t vap = NULL;
    u_int8_t ret;
    int cmd_type;
    void *cmd;
    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;
    cmd = extract_command(ic, wdev, &cmd_type);
    if (cmd_type) {
        qdf_print("Invalid Radio Command !");
        return -EINVAL;
    } else {
        vap = (wlan_if_t)cmd;
    }

    ret = wlan_get_param(vap, IEEE80211_DRIVER_CAPS);
    ret = cfg80211_reply_command(wiphy, sizeof(u_int8_t), &ret, 0);
    return ret;
}

/* wlan_cfg80211_list_chan - list channel
 * @wiphy:   pointer to wireless wiphy structure.
 * @wdev:    pointer to wireless_dev structure.
 * @data:    Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed
 *
 * Return: 0 on success, negative errno on failure
 */

int wlan_cfg80211_list_chan(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        const void *data,
        int data_len)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    wlan_if_t vap = NULL;
    u_int8_t ret;
    int cmd_type;
    void *cmd;
    struct ieee80211req_chaninfo *channel = NULL;
    int nchans_max = ((IEEE80211_CHANINFO_MAX - 1) * sizeof(__u32))/
        sizeof(struct ieee80211_ath_channel);

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;
    channel = (struct ieee80211req_chaninfo *)qdf_mem_malloc(sizeof(*channel));
    if (channel == NULL) {
        return -ENOMEM;
    }

    cmd = extract_command(ic, wdev, &cmd_type);
    if (cmd_type) {
        qdf_print("Invalid Radio Command !");
        qdf_mem_free(channel);
        return -EINVAL;
    } 	 else {
        vap = (wlan_if_t)cmd;
    }


    wlan_get_chaninfo(vap, 0, channel->ic_chans, &channel->ic_nchans);

    if (channel->ic_nchans > nchans_max) {
        channel->ic_nchans = nchans_max;
    }
    ret = cfg80211_reply_command(wiphy, (channel->ic_nchans *
                sizeof(struct ieee80211_ath_channel) + sizeof(__u32)), channel, 0);
    qdf_mem_free(channel);
    return ret;
}

/* wlan_cfg80211_active_chan_list - get active chan list
 * @wiphy:   pointer to wireless wiphy structure.
 * @wdev:    pointer to wireless_dev structure.
 * @data:    Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed
 *
 * Return: 0 on success, negative errno on failure
 */
int wlan_cfg80211_active_chan_list(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        const void *data,
        int data_len)

{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    wlan_if_t vap = NULL;
    u_int8_t ret;
    int cmd_type;
    void *cmd;
    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    cmd = extract_command(ic, wdev, &cmd_type);
    if (cmd_type) {
        qdf_print("Invalid Radio Command !");
        return -EINVAL;
    } 	 	 else {
        vap = (wlan_if_t)cmd;
    }
    ret = cfg80211_reply_command(wiphy, sizeof(ic->ic_chan_active), ic->ic_chan_active, 0);

    return ret;
}


/* wlan_cfg80211_list_chan160 - get chan 160 list
 * @wiphy:   pointer to wireless wiphy structure.
 * @wdev:    pointer to wireless_dev structure.
 * @data:    Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed
 *
 * Return: 0 on success, negative errno on failure
 */
int wlan_cfg80211_list_chan160(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        const void *data,
        int data_len)

{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    wlan_if_t vap = NULL;
    struct net_device *dev = NULL;
    osif_dev *osifp = NULL;
    u_int8_t ret;
    void *cmd;
    int cmd_type;
    struct ieee80211req_chaninfo *channel = NULL;
    int nchans_max = ((IEEE80211_CHANINFO_MAX - 1) * sizeof(__u32))/
        sizeof(struct ieee80211_ath_channel);
    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;
    dev = wdev->netdev;
    osifp = ath_netdev_priv(dev);
    channel = (struct ieee80211req_chaninfo *)qdf_mem_malloc(sizeof(*channel));
    if (channel == NULL) {
        return -ENOMEM;
    }
    cmd = extract_command(ic, wdev, &cmd_type);
    if (cmd_type) {
        qdf_print("Invalid Radio Command !");
        qdf_mem_free(channel);
        return -EINVAL;
    } else {
        vap = (wlan_if_t)cmd;
    }

    wlan_get_chaninfo(vap, 1, channel->ic_chans, &channel->ic_nchans);

    if (channel->ic_nchans > nchans_max) {
        channel->ic_nchans = nchans_max;
    }

    ret = cfg80211_reply_command(wiphy, channel->ic_nchans *
                        sizeof(struct ieee80211_ath_channel) + sizeof(__u32), channel, 0);

    qdf_mem_free(channel);
    return ret;
}


/* wlan_cfg80211_wnm_params - wnm param
 * @wiphy:   pointer to wireless wiphy structure.
 * @wdev:    pointer to wireless_dev structure.
 * @data:    Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed
 *
 * Return: 0 on success, negative errno on failure
 */
int wlan_cfg80211_wnm_params(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        const void *data,
        int data_len)

{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    wlan_if_t vap = NULL;
    int ret;
    int cmd_type;
    void *cmd;
    struct ieee80211_wlanconfig *config = (struct ieee80211_wlanconfig *)data;

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;
    cmd = extract_command(ic, wdev, &cmd_type);
    if (cmd_type) {
        qdf_print("Invalid Radio Command !");
        return -EINVAL;
    } else {
        vap = (wlan_if_t)cmd;
    }

    ret = ieee80211_ucfg_wnm(vap, config);
    return ret;
}


#if ATH_SUPPORT_DYNAMIC_VENDOR_IE
/* wlan_cfg80211_vendorie_params - vendorie params
 * @wiphy:   pointer to wireless wiphy structure.
 * @wdev:    pointer to wireless_dev structure.
 * @data:    Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed
 *
 * Return: 0 on success, negative errno on failure
 */
int wlan_cfg80211_vendorie_params(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        const void *data,
        int data_len)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    wlan_if_t vap = NULL;
    u_int8_t ret;
    u_int8_t *ie_buf;
    u_int32_t ie_len = 0, ftype = 0;
    u_int8_t temp_buf[IEEE80211_ADDR_LEN];
    int cmd_type;
    void *cmd;
    struct ieee80211_wlanconfig_vendorie *vie = (struct ieee80211_wlanconfig_vendorie *)data;
    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;
    cmd = extract_command(ic, wdev, &cmd_type);
    if (cmd_type) {
        qdf_print("Invalid Radio Command !");
        return -EINVAL;
    } else {
        vap = (wlan_if_t)cmd;
    }

    ret = ieee80211_ucfg_vendorie(vap, vie);
    if(vie->cmdtype == IEEE80211_WLANCONFIG_VENDOR_IE_LIST) {
        /*
         * List command searches all the frames and returns the IEs matching
         * the given OUI type. A simple list command will return all the IEs
         * whereas a list command followed by the OUI type will return only those
         * IEs matching the OUI. A temp_buf is used to store the OUI type recieved
         * as the iwr.u.data.pointer will be written with IE data to be returned.
         */
        ie_buf = (u_int8_t *)&vie->ie;
        memcpy(temp_buf, ie_buf, 5);
#if ATH_SUPPORT_DYNAMIC_VENDOR_IE
        for (ftype = 0; ftype < IEEE80211_FRAME_TYPE_MAX; ftype++)
        {
            ret = wlan_get_vendorie(vap, vie, ftype, &ie_len, temp_buf);
        }
#endif
        ret = cfg80211_reply_command(wiphy,ie_len + 12, vie, 0);

    }
    return ret;
}
#endif

/* wlan_cfg80211_set_max_rate - set max rate
 * @wiphy:   pointer to wireless wiphy structure.
 * @wdev:    pointer to wireless_dev structure.
 * @data:    Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed
 *
 * Return: 0 on success, negative errno on failure
 */
int wlan_cfg80211_set_maxrate(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        const void *data,
        int data_len)

{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    wlan_if_t vap = NULL;
    int cmd_type;
    void *cmd;
    struct ieee80211_wlanconfig *config = (struct ieee80211_wlanconfig *)data;
    struct ieee80211_wlanconfig_setmaxrate *smr = &(config->smr);
    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;
    cmd = extract_command(ic, wdev, &cmd_type);
    if (cmd_type) {
        qdf_print("Invalid Radio Command !");
        return -EINVAL;
    } else {
        vap = (wlan_if_t)cmd;
    }
    /* Now iterate through the node table */
    return wlan_iterate_station_list(vap, ieee80211_ucfg_setmaxrate_per_client, (void *)smr);
}

#if ATH_SUPPORT_NAC_RSSI
/* wlan_cfg80211_nac_rssi_params - set nac param
 * @wiphy:   pointer to wireless wiphy structure.
 * @wdev:    pointer to wireless_dev structure.
 * @data:    Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed
 *
 * Return: 0 on success, negative errno on failure
 */
int wlan_cfg80211_nac_rssi_params(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        const void *data,
        int data_len)

{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    wlan_if_t vap = NULL;
    u_int8_t ret;
    int cmd_type;
    void *cmd;
    struct ieee80211_wlanconfig *config = (struct ieee80211_wlanconfig *)data;
    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;
    cmd = extract_command(ic, wdev, &cmd_type);
    if (cmd_type) {
        qdf_print("Invalid Radio Command !");
        return -EINVAL;
    } else {
        vap = (wlan_if_t)cmd;
    }

    ret = ieee80211_ucfg_nac_rssi(vap, config);
    if (config->cmdtype == IEEE80211_WLANCONFIG_NAC_RSSI_ADDR_LIST) {
        ret = cfg80211_reply_command(wiphy, sizeof(struct ieee80211_wlanconfig), config, 0);
    }
    return ret;
}
#endif

#if ATH_SUPPORT_NAC
/* wlan_cfg80211_nac_params - set nac param
 * @wiphy:   pointer to wireless wiphy structure.
 * @wdev:    pointer to wireless_dev structure.
 * @data:    Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed
 *
 * Return: 0 on success, negative errno on failure
 */
int wlan_cfg80211_nac_params(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        const void *data,
        int data_len)

{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    wlan_if_t vap = NULL;
    u_int8_t ret;
    int cmd_type;
    void *cmd;
    struct ieee80211_wlanconfig *config = (struct ieee80211_wlanconfig *)data;
    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;
    cmd = extract_command(ic, wdev, &cmd_type);
    if (cmd_type) {
        qdf_print("Invalid Radio Command !");
        return -EINVAL;
    } else {
        vap = (wlan_if_t)cmd;
    }

    if(!vap->iv_smart_monitor_vap) {
        qdf_print("Nac commands not supported in non smart monitor vap!");
        return -EPERM;
    }

    ret = ieee80211_ucfg_nac(vap, config);
    if(config->cmdtype == IEEE80211_WLANCONFIG_NAC_ADDR_LIST) {
        ret = cfg80211_reply_command(wiphy, sizeof(struct ieee80211_wlanconfig), config, 0);
    }
    return ret;
}
#endif

#if QCA_AIRTIME_FAIRNESS
/* wlan_cfg80211_atf_setssid - set atf ssid
 * @wiphy:   pointer to wireless wiphy structure.
 * @wdev:    pointer to wireless_dev structure.
 * @data:    Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed
 *
 * Return: 0 on success, negative errno on failure
 */
int wlan_cfg80211_atf(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        const void *data,
        int data_len)

{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    wlan_if_t vap = NULL;
    struct atf_subtype *buf = (struct atf_subtype *)data;
    struct ssid_val ssidval;
    struct atf_data *atfdata;
    int ret = 0, total_len = 0, len = 0, index = 0;
    int cmd_type;
    void *cmd;
    struct atftable  *at = 0;

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;
    memcpy(&ssidval, data, sizeof(struct ssid_val));
    cmd = extract_command(ic, wdev, &cmd_type);
    if (cmd_type) {
        qdf_print("Invalid Radio Command !");
        return -EINVAL;
    } else {
        vap = (wlan_if_t)cmd;
    }
    if (vap == NULL) {
        qdf_print("Vap is NULL !");
        return -EINVAL;
    }

    switch (buf->id_type) {
        case IEEE80211_IOCTL_ATF_ADDSSID:
            ret = ucfg_atf_set_ssid(vap->vdev_obj, &ssidval);
            break;
        case IEEE80211_IOCTL_ATF_DELSSID:
            ret = ucfg_atf_delete_ssid(vap->vdev_obj, (struct ssid_val *)data);
            break;
        case IEEE80211_IOCTL_ATF_ADDSTA:
            ret = ucfg_atf_set_sta(vap->vdev_obj, (struct sta_val *)data);
            break;
        case IEEE80211_IOCTL_ATF_DELSTA:
            ret = ucfg_atf_delete_sta(vap->vdev_obj, (struct sta_val *)data);
            break;
        case IEEE80211_IOCTL_ATF_SHOWATFTBL:
            atfdata = (struct atf_data  *) data;
            total_len = sizeof(struct atftable);
            at = (struct atftable *)atfdata->buf;
            ret = ucfg_atf_show_table(vap->vdev_obj, at);

            while (index < total_len) {
                if ((total_len - index) / MAX_CFG80211_BUF_LEN)
                    len = MAX_CFG80211_BUF_LEN;
                else
                    len = total_len - index;
                if (cfg80211_reply_command(wiphy, len, ((u_int8_t *)at) + index, 0)) {
                    qdf_print("Reply failed");
                    return -EINVAL;
                }
                index += len;
            }
            break;
        case IEEE80211_IOCTL_ATF_SHOWAIRTIME:
            atfdata = (struct atf_data  *) data;
            ret = ucfg_atf_show_airtime(vap->vdev_obj, (struct atftable  *)atfdata->buf);
            break;
        case IEEE80211_IOCTL_ATF_FLUSHTABLE:
            ret = ucfg_atf_flush_table(vap->vdev_obj);
            break;
        case IEEE80211_IOCTL_ATF_ADDGROUP:
            ret = ucfg_atf_add_group(vap->vdev_obj, (struct atf_group *)data);
            break;
        case IEEE80211_IOCTL_ATF_CONFIGGROUP:
            ret = ucfg_atf_config_group(vap->vdev_obj, (struct atf_group *)data);
            break;
        case IEEE80211_IOCTL_ATF_GROUPSCHED:
            ret = ucfg_atf_group_sched(vap->vdev_obj, (struct atf_group *)data);
            break;
        case IEEE80211_IOCTL_ATF_DELGROUP:
            ret = ucfg_atf_delete_group(vap->vdev_obj, (struct atf_group *)data);
            break;
        case IEEE80211_IOCTL_ATF_SHOWGROUP:
            atfdata = (struct atf_data  *) data;
            ret = ucfg_atf_show_group(vap->vdev_obj, (struct atfgrouptable *)atfdata->buf);
            break;
        case IEEE80211_IOCTL_ATF_SHOWSUBGROUP:
            atfdata = (struct atf_data  *) data;
            ret = ucfg_atf_show_subgroup(vap->vdev_obj, (struct atfgrouplist_table *)atfdata->buf);
            break;
        case IEEE80211_IOCTL_ATF_ADDAC:
            ret = ucfg_atf_add_ac(vap->vdev_obj, (struct atf_ac *)data);
            break;
        case IEEE80211_IOCTL_ATF_DELAC:
            ret = ucfg_atf_del_ac(vap->vdev_obj, (struct atf_ac *)data);
            break;
        case IEEE80211_IOCTL_ATF_ADDSTA_TPUT:
            ret = ucfg_atf_add_sta_tput(vap->vdev_obj, (struct sta_val*)data);
            break;
        case IEEE80211_IOCTL_ATF_DELSTA_TPUT:
            ret = ucfg_atf_delete_sta_tput(vap->vdev_obj, (struct sta_val*)data);
            break;
        case IEEE80211_IOCTL_ATF_SHOW_TPUT:
            ret = ucfg_atf_show_tput(vap->vdev_obj);
            break;
        default:
            break;
    }

    return ret;
}
#endif

/* wlan_cfg80211_list_sta - list sta
 * @wiphy:   pointer to wireless wiphy structure.
 * @wdev:    pointer to wireless_dev structure.
 * @data:    Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed
 *
 * Return: 0 on success, negative errno on failure
 */
int wlan_cfg80211_list_sta(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        const void *data,
        int data_len)

{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    wlan_if_t vap = NULL;
    u_int8_t ret = 0;
    int cmd_type;
    struct stainforeq req;
    u_int32_t length;
    void *cmd;
    struct net_device *dev = NULL;
    osif_dev *osifp = NULL;
    dev = wdev->netdev;
    osifp = ath_netdev_priv(dev);

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    cmd = extract_command(ic, wdev, &cmd_type);
    if (cmd_type) {
        qdf_print("Invalid Radio Command !");
        return -EINVAL;
    } else {
        vap = (wlan_if_t)cmd;
    }

    if(osifp->os_opmode == IEEE80211_M_STA) {
        return -EPERM;
    }

    req.space = 0;
    req.vap = vap;
    wlan_iterate_station_list(vap, get_sta_space, &req);

    if(req.space == 0) {
        ret = cfg80211_reply_command(wiphy, 0 , NULL, 0);
        return ret;
    }

    if (req.space > 0) {
        void *si_data;
        int space;
        void *send_msg, *msg;
        int send_length = LIST_ALLOC_SIZE;
        int len, total_len;
        space = req.space;
        si_data = (void *)qdf_mem_malloc(space);
        if (si_data == NULL) {
            return -ENOMEM;
        }

        req.si = si_data;
        wlan_iterate_station_list(vap, get_sta_info, &req);
        length = space - req.space;
        send_msg = si_data;
        len = 0;
        total_len = 0;
        msg = send_msg;

        while (total_len < length) {
            struct ieee80211req_sta_info *si;
            si = (struct ieee80211req_sta_info *) send_msg;

            if(((len + si->isi_len) < send_length)) {

                len +=  si->isi_len;
                total_len += si->isi_len;
                send_msg += si->isi_len;
            } else {
                ret = cfg80211_reply_command(wiphy, len, msg, 0);
                msg = send_msg;
                len = 0;
            }
        }

        /* Send Remaining data!*/

        if(len != 0) {
            ret = cfg80211_reply_command(wiphy, len, msg, 0);
        }

        qdf_mem_free(si_data);
    }
    return ret;
}


/* wlan_cfg80211_list_scan - list scan
 * @wiphy:   pointer to wireless wiphy structure.
 * @wdev:    pointer to wireless_dev structure.
 * @data:    Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed
 *
 * Return: 0 on success, negative errno on failure
 */
int wlan_cfg80211_list_scan(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        const void *data,
        int data_len)

{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    wlan_if_t vap = NULL;
    u_int8_t ret = 0;
    struct scanreq req;
    int time_elapsed = OS_SIWSCAN_TIMEOUT;
    u_int32_t length, cmd_type;
    struct net_device *dev = NULL;
    osif_dev *osifp = NULL;
    void *cmd;
    struct wlan_objmgr_pdev *pdev = NULL;

    dev = wdev->netdev;
    osifp = ath_netdev_priv(dev);
    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;
    cmd = extract_command(ic, wdev, &cmd_type);
    if (cmd_type) {
        qdf_print("Invalid Radio Command !");
        return -EINVAL;
    } 	 else {
        vap = (wlan_if_t)cmd;
    }

    if (!(dev->flags & IFF_UP)) {
        return -EINVAL;
    }

    /* Increase timeout value for EIR since a rpt scan itself takes 12 seconds */
    if (ieee80211_ic_enh_ind_rpt_is_set(vap->iv_ic)) {
        time_elapsed = OS_SIWSCAN_TIMEOUT * SIWSCAN_TIME_ENH_IND_RPT;
    }

    if (osifp->os_opmode != IEEE80211_M_STA ||
            osifp->os_opmode != IEEE80211_M_P2P_DEVICE ||
            osifp->os_opmode != IEEE80211_M_P2P_CLIENT) {
        /* For station mode - umac connection sm always runs SCAN */
        if (wlan_scan_in_progress(vap) &&
                (time_after(osifp->os_last_siwscan + time_elapsed, OS_GET_TICKS())))
        {
            osifp->os_giwscan_count++;
            return -EAGAIN;
        }
    }
    req.space = 0;
    req.vap = vap;

    pdev = wlan_vap_get_pdev(vap);
    ucfg_scan_db_iterate(pdev, get_scan_space, (void *) &req);
    if (req.space > 0) {
        size_t space;
        void *sc_data;
        void *send_msg, *msg;
        int send_length = 3*1024;
        int len, total_len;
        space = req.space;
        sc_data = (void *)qdf_mem_malloc(space);
        if (sc_data == NULL) {
            return -ENOMEM;
        }
        req.sr = sc_data;
        ucfg_scan_db_iterate(pdev, get_scan_result,(void *) &req);
        length = space - req.space;
        send_msg = sc_data;
        len = 0;
        total_len = 0;
        msg = send_msg;

        while (total_len < length) {
            struct ieee80211req_scan_result *sr;
            sr = (struct ieee80211req_scan_result *) send_msg;
            if(((len + sr->isr_len) < send_length)) {
                len +=  sr->isr_len;
                total_len += sr->isr_len;
                send_msg += sr->isr_len;
            } else {
                ret = cfg80211_reply_command(wiphy, len, msg, 0);
                msg = send_msg;
                len = 0;
            }

        }
        if(len != 0) {
            ret = cfg80211_reply_command(wiphy, len, msg, 0);
        }

        ret = ieee80211_ucfg_scanlist(vap);

        qdf_mem_free(sc_data);
    }

    return ret;
}

#if ATH_SUPPORT_HYFI_ENHANCEMENTS
/* wlan_cfg80211_hmmc_params - list hmmc param
 * @wiphy:   pointer to wireless wiphy structure.
 * @wdev:    pointer to wireless_dev structure.
 * @data:    Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed
 *
 * Return: 0 on success, negative errno on failure
 */
int wlan_cfg80211_hmmc_params(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        const void *data,
        int data_len)

{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    u_int8_t ret;
    wlan_if_t vap = NULL;
    int cmd_type;
    void *cmd;
    struct ieee80211_wlanconfig *config = (struct ieee80211_wlanconfig *)data;

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;
    cmd = extract_command(ic, wdev, &cmd_type);
    if (cmd_type) {
        qdf_print("Invalid Radio Command !");
        return -EINVAL;
    } else {
        vap = (wlan_if_t)cmd;
    }

    ret = ieee80211_ucfg_hmmc(vap, config);

    return ret;
}
#endif

extern int wlan_update_peer_cp_stats(struct ieee80211_node *ni,
                              struct ieee80211_nodestats *ni_stats_user);

extern int wlan_update_vdev_cp_stats(struct ieee80211vap *vap,
                              struct ieee80211_stats *vap_stats_usr,
                              struct ieee80211_mac_stats *ucast_stats_usr,
                              struct ieee80211_mac_stats *mcast_stats_usr);

extern int wlan_update_pdev_cp_stats(struct ieee80211com *ic,
                              struct ol_ath_radiostats *scn_stats_user);
extern int
wlan_get_peer_dp_stats(struct ieee80211com *ic,
                       struct wlan_objmgr_peer *peer,
                       struct ieee80211_nodestats *ni_stats_user);
extern int
wlan_get_vdev_dp_stats(struct ieee80211vap *vap,
                       struct ieee80211_stats *vap_stats_usr,
                       struct ieee80211_mac_stats *ucast_stats_usr,
                       struct ieee80211_mac_stats *mcast_stats_usr);
extern int
wlan_get_pdev_dp_stats(struct ieee80211com *ic,
                       struct ol_ath_radiostats *scn_stats_user);

#if UMAC_SUPPORT_STA_STATS
/* wlan_cfg80211_sta_stats - list sta stats
 * @wiphy:   pointer to wireless wiphy structure.
 * @wdev:    pointer to wireless_dev structure.
 * @data:    Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed
 *
 * Return: 0 on success, negative errno on failure
 */
int wlan_cfg80211_sta_stats(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        const void *data,
        int data_len)

{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    u_int8_t error;
    wlan_if_t vap = NULL;
    struct ieee80211_node *ni;
    u_int8_t macaddr[IEEE80211_ADDR_LEN];
    int cmd_type;
    void *cmd;
    struct ieee80211req_sta_stats *stats = (struct ieee80211req_sta_stats *)data;
    struct ieee80211req_sta_stats stats_user;
    bool is_offload = false;
    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;
    cmd = extract_command(ic, wdev, &cmd_type);
    if (cmd_type) {
        qdf_print("Invalid Radio Command !");
        return -EINVAL;
    } else {
        vap = (wlan_if_t)cmd;
    }
    memcpy(macaddr, stats->is_u.macaddr,
            IEEE80211_ADDR_LEN);
    qdf_mem_zero(&stats_user, sizeof(stats_user));
    memcpy(&stats_user.is_u.macaddr, stats->is_u.macaddr,
            IEEE80211_ADDR_LEN);
    ni = ieee80211_vap_find_node(vap, macaddr);

    if (ni == NULL) {
        return -EINVAL;
    }

#ifdef QCA_SUPPORT_CP_STATS
    if(wlan_update_peer_cp_stats(ni, &stats_user.is_stats) != 0) {
       ieee80211_free_node(ni);
       return -EFAULT;
    }
#endif
    is_offload = ic->ic_is_mode_offload(ic);
    if (is_offload) {
        if (wlan_get_peer_dp_stats(ic,
                                   ni->peer_obj,
                                   &stats_user.is_stats) != 0) {
            ieee80211_free_node(ni);
            return -EFAULT;
        }
        stats_user.is_stats.ns_rssi = ni->ni_rssi;
    }
    error = cfg80211_reply_command(wiphy, sizeof(struct ieee80211req_sta_stats),
                                   &stats_user, 0);
    ieee80211_free_node(ni);
    return error;
}
#endif

/* wlan_cfg80211_80211stats - list 80211 stats
 * @wiphy:   pointer to wireless wiphy structure.
 * @wdev:    pointer to wireless_dev structure.
 * @data:    Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed
 *
 * Return: 0 on success, negative errno on failure
 */
int wlan_cfg80211_80211stats(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        const void *data,
        int data_len)

{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    int error;
    wlan_if_t vap = NULL;
    int cmd_type;
    void *cmd;
    struct ieee80211_stats *stats = NULL;
    struct ieee80211_mac_stats *ucaststats = NULL;
    struct ieee80211_mac_stats *mcaststats = NULL;
    bool is_offload;
    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    if (ic == NULL) {
        qdf_print("%s: ic is NULL",__func__);
        return -EINVAL;
    }

    cmd = extract_command(ic, wdev, &cmd_type);

    if (cmd_type) {
        qdf_print("Invalid Radio Command !");
        return -EINVAL;
    } else {
        vap = (wlan_if_t)cmd;
    }

    if(vap->get_vdev_bcn_stats) {
        qdf_atomic_init(&(vap->vap_bcn_event));
        vap->get_vdev_bcn_stats(vap);
#define BCN_STATS_TIMEOUT ((CONVERT_SEC_TO_SYSTEM_TIME(5)/10) + 1) /* 50 ms*/
                /* give enough delay to get the result */
        schedule_timeout_interruptible(BCN_STATS_TIMEOUT);

        if (qdf_atomic_read(&(vap->vap_bcn_event)) != 1) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,
                              "%s: VAP BCN STATS FAILED\n", __func__);
        }
    }

    stats = (struct ieee80211_stats *)qdf_mem_malloc(sizeof(struct ieee80211_stats) +
                               (2*sizeof(struct ieee80211_mac_stats)));
    if (!stats) {
        qdf_print("no memory for stats_user!");
        return -ENOMEM;
    }

    ucaststats = (struct ieee80211_mac_stats*)((unsigned char *)stats +
                                               sizeof(struct ieee80211_stats));
    mcaststats = (struct ieee80211_mac_stats*)((unsigned char *)ucaststats +
                                            sizeof(struct ieee80211_mac_stats));
#ifdef QCA_SUPPORT_CP_STATS
    if (wlan_update_vdev_cp_stats(vap, stats, ucaststats,
                                  mcaststats) != 0) {
        qdf_mem_free(stats);
        return -EFAULT;
    }
#endif
    is_offload = ic->ic_is_mode_offload(ic);
    if (is_offload) {
        if (wlan_get_vdev_dp_stats(vap, stats, ucaststats,
                                   mcaststats) != 0) {
            qdf_mem_free(stats);
            return -EFAULT;
        }
    }

    error = cfg80211_reply_command(wiphy, sizeof (struct ieee80211_stats) +
            sizeof(struct ieee80211_mac_stats) +
            sizeof(struct ieee80211_mac_stats), stats, 0);

    qdf_mem_free(stats);

    return error;
}

/*
    Receiving clone params before vap creation and
    storing it in a lined list in ic structure.
    During vap creation, this values will be used.
*/
int wlan_cfg80211_cloneparams(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        const void *data,
        int data_len)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    int cmd_type = 0;
    void *cmd = NULL;
    struct ol_ath_softc_net80211 *scn = NULL;
    struct net_device *dev = NULL;
    struct ieee80211_clone_params *cp = (struct ieee80211_clone_params *)data;
    struct ieee80211_clone_params_list *clone_list;
    struct ieee80211_clone_params_list *cp_entry;
    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;
    cmd = extract_command(ic, wdev, &cmd_type);

    if (cmd_type) {
        dev = wdev->netdev;
        scn =  (struct ol_ath_softc_net80211 *)cmd;
    } else {
        qdf_print("Invalid VAP Command !");
        return -EINVAL;
    }

    if (scn->soc->sc_in_delete) {
        qdf_print("Delete in Progress ! Command cannot be applied!");
        return -EBUSY;
    }
    LIST_FOREACH(cp_entry, &ic->ic_cp, link_entry)
    {
        if(!(OS_MEMCMP(cp->icp_name, cp_entry->cp.icp_name, IFNAMSIZ-1)))
        {
            return -1;
        }
    }
    clone_list = (struct ieee80211_clone_params_list *)
    qdf_mem_malloc(sizeof(struct ieee80211_clone_params_list));
    if (!clone_list){
        qdf_print("%s: Returning as mem malloc failed.",__func__);
        return -EINVAL;
    }
    qdf_mem_copy(&clone_list->cp, cp, sizeof(struct ieee80211_clone_params));
    LIST_INSERT_HEAD(&ic->ic_cp, clone_list, link_entry);
    return 0;
}

/* wlan_cfg80211_extendedstats - list extended stats
 * @wiphy:   pointer to wireless wiphy structure.
 * @wdev:    pointer to wireless_dev structure.
 * @data:    Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed
 *
 * Return: 0 on success, negative errno on failure
 */
int wlan_cfg80211_extendedstats(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        const void *data,
        int data_len)

{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    u_int8_t error;
    int cmd_type;
    void *cmd;
    int reply = 0;
    struct ol_ath_softc_net80211 *scn = NULL;
    struct net_device *dev = NULL;
    struct extended_ioctl_wrapper *extended_cmd = ( struct extended_ioctl_wrapper *)data;
    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;
    cmd = extract_command(ic, wdev, &cmd_type);

    if (cmd_type) {
        dev = wdev->netdev;
        scn =  (struct ol_ath_softc_net80211 *)cmd;
    } else {
        qdf_print("Invalid VAP Command !");
        return -EINVAL;
    }

    reply = ic->ic_cfg80211_radio_handler.extended_commands(dev, (void*) extended_cmd);

    if(reply == 1) {
        error = cfg80211_reply_command(wiphy, sizeof(struct extended_ioctl_wrapper), extended_cmd, 0);
    }
    return 0;
}

/* wlan_cfg80211_athstats - athstats
 * @wiphy:   pointer to wireless wiphy structure.
 * @wdev:    pointer to wireless_dev structure.
 * @data:    Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed
 *
 * Return: 0 on success, negative errno on failure
 */
int wlan_cfg80211_athstats(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        const void *data,
        int data_len)

{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    u_int8_t ret = 0;
    struct ol_ath_softc_net80211 *scn = NULL;
    int cmd_type;
    void *cmd;
    struct sk_buff *reply_skb = NULL;
    struct ath_stats_container *asc = (struct ath_stats_container *)data;
    QDF_STATUS status;

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;
    cmd = extract_command(ic, wdev, &cmd_type);

    if (!cmd_type) {
        qdf_print("Invalid VAP Command !");
        return -EINVAL;
    } else {
        scn = (struct ol_ath_softc_net80211 *)cmd;
    }

    ic->ic_cfg80211_radio_handler.get_ath_stats(scn, asc);

    /* Allocate memory size of the data being sent + skb header length + u_32 for FLA + u_32
     * for the length of the data being sent
     */
    reply_skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, (2*sizeof(u_int32_t)) + NLMSG_HDRLEN);

    if (reply_skb) {
        if((nla_put_u32(reply_skb, QCA_WLAN_VENDOR_ATTR_PARAM_FLAGS, asc->offload_if))){
            kfree_skb(reply_skb);
            return -EINVAL;
        }
        status = qal_devcfg_send_response((qdf_nbuf_t)reply_skb);
        return qdf_status_to_os_return(status);
    } else {
        return -ENOMEM;
    }

    return ret;
}

/* wlan_cfg80211_phystats - phy stats
 * @wiphy:   pointer to wireless wiphy structure.
 * @wdev:    pointer to wireless_dev structure.
 * @data:    Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed
 *
 * Return: 0 on success, negative errno on failure
 */
int wlan_cfg80211_phystats(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        const void *data,
        int data_len)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    int cmd_type;
    void *cmd;
    struct ol_ath_softc_net80211 *scn = NULL;
    struct ol_ath_radiostats scn_stats;
    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;
    cmd = extract_command(ic, wdev, &cmd_type);
    if (!cmd_type) {
        qdf_print("Invalid VAP Command !");
        return -EINVAL;
    } else {
        scn = (struct ol_ath_softc_net80211 *)cmd;
        qdf_mem_zero(&scn_stats, sizeof(struct ol_ath_radiostats));

#ifdef QCA_SUPPORT_CP_STATS
        if (wlan_update_pdev_cp_stats(ic, &scn_stats) != 0) {
            qdf_print("Invalid VAP Command !");
            return -EFAULT;
        }
#endif
        if (wlan_get_pdev_dp_stats(ic, &scn_stats) != 0) {
            qdf_print("Invalid VAP Command !");
            return -EFAULT;
        }
    }
    return cfg80211_reply_command(wiphy, sizeof(struct ol_ath_radiostats), &scn_stats, 0);
}

/* wlan_cfg80211_httstats - htt stats
 * @wiphy:   pointer to wireless wiphy structure.
 * @wdev:    pointer to wireless_dev structure.
 * @data:    Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed
 *
 * Return: 0 on success, negative errno on failure
 */
int wlan_cfg80211_httstats(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        const void *data,
        uint32_t data_len)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    struct ol_ath_softc_net80211 *scn = NULL;
    int cmd_type;
    void *cmd;
    void * req = (void*) data;

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;
    cmd = extract_command(ic, wdev, &cmd_type);

    if (cmd_type != RADIO_CMD) {
        qdf_print("Invalid VAP Command !");
        return -EINVAL;
    } else {
        scn = (struct ol_ath_softc_net80211 *)cmd;

	if(ic->ic_cfg80211_radio_handler.get_dp_htt_stats) {
            ic->ic_cfg80211_radio_handler.get_dp_htt_stats((void*)scn, req, data_len);
        } else  {
            return -EOPNOTSUPP;
        }
    }

    return 0;
}
/* wlan_cfg80211_ald_params - ald stats
 * @wiphy:   pointer to wireless wiphy structure.
 * @wdev:    pointer to wireless_dev structure.
 * @data:    Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed
 *
 * Return: 0 on success, negative errno on failure
 */
int wlan_cfg80211_ald_params(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        const void *data,
        int data_len)

{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    int cmd_type;
    void *cmd;
    int ret = 0;
    wlan_if_t vap = NULL;
    struct ieee80211_wlanconfig *config = (struct ieee80211_wlanconfig *)data;
    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;
    cmd = extract_command(ic, wdev, &cmd_type);
    if (cmd_type) {
        qdf_print("Invalid Radio Command !");
        return -EINVAL;
    } else {
        vap = (wlan_if_t)cmd;
    }
    ret = ieee80211_ucfg_ald(vap, config);
    return ret;
}

/* free the clone arams list on radio detach */
void wlan_cfg80211_free_clone_params_list(struct ieee80211com *ic)
{
    struct ieee80211_clone_params_list *cp_entry, *next_entry;
    LIST_FOREACH_SAFE(cp_entry, &ic->ic_cp, link_entry, next_entry)
    {
        LIST_REMOVE(cp_entry,link_entry);
        qdf_mem_free(cp_entry);
    }
}

/**
 * wlan_cfg80211_register_spectral_cmd_handler() - Registration api
 * for spectral to register vendor command handlers
 * @pdev:    Pointer to pdev
 * @idx:     Index of function table
 * @handler: Reference to handler
 *
 * Return: nothing
 */
void wlan_cfg80211_register_spectral_cmd_handler(struct wlan_objmgr_pdev *pdev,
                                                 int idx, void *handler) {
    struct ol_ath_softc_net80211 *scn = NULL;
    struct pdev_osif_priv *osif_priv = NULL;
    struct ieee80211com * ic = NULL;

    osif_priv = wlan_pdev_get_ospriv(pdev);
    scn = (struct ol_ath_softc_net80211 *)osif_priv->legacy_osif_priv;
    ic = &scn->sc_ic;

    ic->ic_cfg80211_spectral_handler.wlan_cfg80211_spectral_handlers[idx] = handler;
}
EXPORT_SYMBOL(wlan_cfg80211_register_spectral_cmd_handler);

/**
 * wlan_cfg80211_spectral_scan_config_and_start_cb() - Handles spectral scan start command
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Length of @data
 *
 * Return: 0 on success, negative errno on failure
 */
static int wlan_cfg80211_spectral_scan_config_and_start_cb(struct wiphy *wiphy,
                        struct wireless_dev *wdev,
                        const void *data,
                        int data_len)
{
    struct wlan_objmgr_pdev *pdev;
    struct ieee80211com *ic;
    int ret;
    struct cfg80211_context *cfg_ctx = NULL;

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;
    pdev = ic->ic_pdev_obj;

    ret = SPECTRAL_CMD_HANDLER(SPECTRAL_SCAN_START_HANDLER_IDX,
                               wiphy,
                               pdev,
                               data,
                               data_len);

    return ret;
}

/**
 * wlan_cfg80211_spectral_scan_stop_cb() - Handles spectral scan stop command
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Length of @data
 *
 * Return: 0 on success, negative errno on failure
 */
static int wlan_cfg80211_spectral_scan_stop_cb(struct wiphy *wiphy,
                        struct wireless_dev *wdev,
                        const void *data,
                        int data_len)

{
    struct wlan_objmgr_pdev *pdev;
    struct ieee80211com *ic;
    int ret;
    struct cfg80211_context *cfg_ctx = NULL;

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;
    pdev = ic->ic_pdev_obj;

    ret = SPECTRAL_CMD_HANDLER(SPECTRAL_SCAN_STOP_HANDLER_IDX,
                               wiphy,
                               pdev,
                               data,
                               data_len);

    return ret;
}

/**
 * wlan_cfg80211_spectral_scan_stop_cb() - Handles spectral get config command
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Length of @data
 *
 * Return: 0 on success, negative errno on failure
 */
static int wlan_cfg80211_spectral_scan_get_config_cb(struct wiphy *wiphy,
                        struct wireless_dev *wdev,
                        const void *data, int data_len)
{
    struct wlan_objmgr_pdev *pdev;
    struct ieee80211com *ic;
    int ret;
    struct cfg80211_context *cfg_ctx = NULL;

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;
    pdev = ic->ic_pdev_obj;

    ret = SPECTRAL_CMD_HANDLER(SPECTRAL_SCAN_GET_CONFIG_HANDLER_IDX,
                               wiphy,
                               pdev,
                               data,
                               data_len);

    return ret;
}

/**
 * wlan_cfg80211_spectral_scan_get_diag_stats_cb() - Handles spectral get diag stats command
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Length of @data
 *
 * Return: 0 on success, negative errno on failure
 */
static int wlan_cfg80211_spectral_scan_get_diag_stats_cb(struct wiphy *wiphy,
                        struct wireless_dev *wdev,
                        const void *data,
                        int data_len)
{
    struct wlan_objmgr_pdev *pdev;
    struct ieee80211com *ic;
    int ret;
    struct cfg80211_context *cfg_ctx = NULL;

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;
    pdev = ic->ic_pdev_obj;

    ret = SPECTRAL_CMD_HANDLER(SPECTRAL_SCAN_GET_DIAG_STATS_HANDLER_IDX,
                               wiphy,
                               pdev,
                               data,
                               data_len);

    return ret;
}

/**
 * wlan_cfg80211_spectral_scan_get_cap_cb() - Handles spectral get cap command
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Length of @data
 *
 * Return: 0 on success, negative errno on failure
 */
static int wlan_cfg80211_spectral_scan_get_cap_cb(struct wiphy *wiphy,
                        struct wireless_dev *wdev,
                        const void *data,
                        int data_len)
{
    struct wlan_objmgr_pdev *pdev;
    struct ieee80211com *ic;
    int ret;
    struct cfg80211_context *cfg_ctx = NULL;

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;
    pdev = ic->ic_pdev_obj;

    ret = SPECTRAL_CMD_HANDLER(SPECTRAL_SCAN_GET_CAP_HANDLER_IDX,
                               wiphy,
                               pdev,
                               data,
                               data_len);

    return ret;
}

/**
 * wlan_cfg80211_spectral_scan_get_status_cb() - Handles spectral get status command
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Length of @data
 *
 * Return: 0 on success, negative errno on failure
 */
static int wlan_cfg80211_spectral_scan_get_status_cb(struct wiphy *wiphy,
                        struct wireless_dev *wdev,
                        const void *data,
                        int data_len)
{
    struct wlan_objmgr_pdev *pdev;
    struct ieee80211com *ic;
    int ret;
    struct cfg80211_context *cfg_ctx = NULL;

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;
    pdev = ic->ic_pdev_obj;

    ret = SPECTRAL_CMD_HANDLER(SPECTRAL_SCAN_GET_STATUS_HANDLER_IDX,
                               wiphy,
                               pdev,
                               data,
                               data_len);

    return ret;
}
void wlan_cfg80211_httstats_event(struct ieee80211com *ic,
        void *buffer, uint32_t buffer_len)
{
    struct sk_buff *vendor_event;
    struct nlattr *nla;
    const uint32_t NUM_TLVS = 10;
    struct net_device *dev = ic->ic_netdev;
    int ret_val;

#define VENDOR_CMD_CB_ASSIGN(a, b) ((void **)a->cb)[2] = b


    vendor_event = cfg80211_vendor_event_alloc(ic->ic_wiphy, NULL,
                buffer_len + NLMSG_HDRLEN + NUM_TLVS*sizeof(uint32_t),
                QCA_NL80211_VENDOR_SUBCMD_HTT_STATS_INDEX,
                GFP_KERNEL);


    if (!vendor_event) {
        QDF_TRACE(QDF_MODULE_ID_DEBUG, QDF_TRACE_LEVEL_ERROR,
                "cfg80211_vendor_event_alloc failed\n");
        return;
    }

    nla_nest_cancel(vendor_event, ((void **)vendor_event->cb)[2]);

    /* Send the IF INDEX to differentiate the ACS event for each interface
     * TODO: To be update once cfg80211 APIs are updated to accept if_index
     */
    ret_val = nla_put_u32(vendor_event,
            NL80211_ATTR_IFINDEX,
            dev->ifindex);
    if (ret_val) {
        qdf_print("NL80211_ATTR_IFINDEX put fail");
        kfree_skb(vendor_event);
        return;
    }

    nla = nla_nest_start(vendor_event, NL80211_ATTR_VENDOR_DATA);

    if(nla == NULL){
        qdf_print("nla_nest_start fail nla is NULL ");
        kfree_skb(vendor_event);
        return;
    }

    VENDOR_CMD_CB_ASSIGN(vendor_event, nla);

    if (nla_put(vendor_event, QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_DATA,
                buffer_len, buffer)) {
        QDF_TRACE(QDF_MODULE_ID_DEBUG, QDF_TRACE_LEVEL_DEBUG,
                "%s:%d nla put fail", __func__, __LINE__);
        return;
    }
    nla_nest_end(vendor_event, nla);

    cfg80211_vendor_event(vendor_event, GFP_KERNEL);
}
EXPORT_SYMBOL(wlan_cfg80211_httstats_event);


/* Vendor id to be used in vendor specific command and events
 *
 * to user space.
 */

#define QCA_NL80211_VENDOR_ID           0x001374
#define MAX_REQUEST_ID                  0xFFFFFFFF

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0))
const struct wiphy_vendor_command wlan_cfg80211_vendor_commands[] = {
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_CONFIGURATION,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
            WIPHY_VENDOR_CMD_NEED_NETDEV,
        .doit = wlan_cfg80211_set_wificonfiguration
    },
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_GET_WIFI_CONFIGURATION,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
            WIPHY_VENDOR_CMD_NEED_NETDEV,
        .doit = wlan_cfg80211_get_wificonfiguration
    },

    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_GPIO_CONFIG_COMMAND,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
            WIPHY_VENDOR_CMD_NEED_NETDEV,
        .doit = wlan_cfg80211_set_gpio_config
    },
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_DO_ACS,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
            WIPHY_VENDOR_CMD_NEED_NETDEV | WIPHY_VENDOR_CMD_NEED_RUNNING,
        .doit = wlan_cfg80211_do_acs
    },
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_PHYERR,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
            WIPHY_VENDOR_CMD_NEED_NETDEV | WIPHY_VENDOR_CMD_NEED_RUNNING,
        .doit = wlan_cfg80211_phyerr
    },
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_TRIGGER_SCAN,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
            WIPHY_VENDOR_CMD_NEED_NETDEV | WIPHY_VENDOR_CMD_NEED_RUNNING,
        .doit = wlan_cfg80211_vendor_scan
    },
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTERNAL_ACS,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
             WIPHY_VENDOR_CMD_NEED_NETDEV |
             WIPHY_VENDOR_CMD_NEED_RUNNING,
        .doit = wlan_cfg80211_update_vendor_channel
    },
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_START,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
             WIPHY_VENDOR_CMD_NEED_NETDEV |
             WIPHY_VENDOR_CMD_NEED_RUNNING,
        .doit = wlan_cfg80211_spectral_scan_config_and_start_cb
    },
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_STOP,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
             WIPHY_VENDOR_CMD_NEED_NETDEV |
             WIPHY_VENDOR_CMD_NEED_RUNNING,
        .doit = wlan_cfg80211_spectral_scan_stop_cb
    },
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_GET_CONFIG,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
             WIPHY_VENDOR_CMD_NEED_NETDEV |
             WIPHY_VENDOR_CMD_NEED_RUNNING,
        .doit = wlan_cfg80211_spectral_scan_get_config_cb
    },
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_GET_DIAG_STATS,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
             WIPHY_VENDOR_CMD_NEED_NETDEV |
             WIPHY_VENDOR_CMD_NEED_RUNNING,
        .doit = wlan_cfg80211_spectral_scan_get_diag_stats_cb
    },
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_GET_CAP_INFO,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
             WIPHY_VENDOR_CMD_NEED_NETDEV |
             WIPHY_VENDOR_CMD_NEED_RUNNING,
        .doit = wlan_cfg80211_spectral_scan_get_cap_cb
    },
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_GET_STATUS,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
             WIPHY_VENDOR_CMD_NEED_NETDEV |
             WIPHY_VENDOR_CMD_NEED_RUNNING,
        .doit = wlan_cfg80211_spectral_scan_get_status_cb
    },
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_GET_RROP_INFO,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
            WIPHY_VENDOR_CMD_NEED_NETDEV | WIPHY_VENDOR_CMD_NEED_RUNNING,
        .doit = wlan_cfg80211_vendor_get_rropinfo
    },
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_SET_QDEPTH_THRESH,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
            WIPHY_VENDOR_CMD_NEED_NETDEV |
            WIPHY_VENDOR_CMD_NEED_RUNNING,
        .doit = wlan_cfg80211_set_qdepth_thresh_cb
    },
};

/* vendor specific events */
static const struct nl80211_vendor_cmd_info wlan_cfg80211_vendor_events[] = {
    [QCA_NL80211_VENDOR_SUBCMD_DO_ACS_INDEX] = {
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_DO_ACS
    },
    [QCA_NL80211_VENDOR_SUBCMD_DFS_OFFLOAD_CAC_STARTED_INDEX] = {
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_DFS_OFFLOAD_CAC_STARTED
    },
    [QCA_NL80211_VENDOR_SUBCMD_UPDATE_EXTERNAL_ACS_CONFIG] = {
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTERNAL_ACS
    },
    [QCA_NL80211_VENDOR_SUBCMD_SCAN_INDEX] = {
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_TRIGGER_SCAN
    },
    [QCA_NL80211_VENDOR_SUBCMD_SCAN_DONE_INDEX] = {
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_SCAN_DONE
    },
    [QCA_NL80211_VENDOR_SUBCMD_GET_WIFI_CONFIGURATION_INDEX] = {
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_GET_WIFI_CONFIGURATION
    },
    [QCA_NL80211_VENDOR_SUBCMD_HTT_STATS_INDEX] = {
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_HTT_STATS
    },
    [QCA_NL80211_VENDOR_SUBCMD_PEER_STATS_CACHE_FLUSH_INDEX] = {
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_PEER_STATS_CACHE_FLUSH
    },
};
#endif

static struct ieee80211_rate wlan_cfg80211_rates_2g[] = {
    /* CCK */
    RATETAB(10,  0x82, 0),
    RATETAB(20,  0x84, 0),
    RATETAB(55,  0x8b, 0),
    RATETAB(110, 0x96, 0),
    /* OFDM */
    RATETAB(60,  0x0c, 0),
    RATETAB(90,  0x12, 0),
    RATETAB(120, 0x18, 0),
    RATETAB(180, 0x24, 0),
    RATETAB(240, 0x30, 0),
    RATETAB(360, 0x48, 0),
    RATETAB(480, 0x60, 0),
    RATETAB(540, 0x6c, 0),
};
#define wlan_cfg80211_rates_2g_size (12)

static struct ieee80211_rate wlan_cfg80211_rates_5g[] = {
    /* OFDM */
    RATETAB(60,  0x0c, 0),
    RATETAB(90,  0x12, 0),
    RATETAB(120, 0x18, 0),
    RATETAB(180, 0x24, 0),
    RATETAB(240, 0x30, 0),
    RATETAB(360, 0x48, 0),
    RATETAB(480, 0x60, 0),
    RATETAB(540, 0x6c, 0),
};
#define wlan_cfg80211_rates_5g_size (8)

static struct ieee80211_rate wlan_cfg80211_rates_49g_half[] = {
    /* OFDM */
    RATETAB(30,  0x06, 0),
    RATETAB(45,  0x09, 0),
    RATETAB(60, 0x0c, 0),
    RATETAB(90, 0x12, 0),
    RATETAB(120, 0x18, 0),
    RATETAB(180, 0x24, 0),
    RATETAB(240, 0x30, 0),
    RATETAB(270, 0x6c, 0),
};
#define wlan_cfg80211_rates_49g_half_size (8)

static struct ieee80211_rate wlan_cfg80211_rates_49g_quarter[] = {
    /* OFDM */
    RATETAB(15,  0x03, 0),
    RATETAB(20,  0x04, 0),
    RATETAB(30, 0x06, 0),
    RATETAB(45, 0x09, 0),
    RATETAB(60, 0x0c, 0),
    RATETAB(90, 0x12, 0),
    RATETAB(120, 0x18, 0),
    RATETAB(135, 0x1b, 0),
};
#define wlan_cfg80211_rates_49g_quarter_size (8)

/**
 *   wlan_cfg80211_get_num_channels: returns number of channels available for
 *   given mode and ic.
 *   @ic: pointer to ieee80211com.
 *   @phymode: phymode (2G/5G) for internal driver usage.
 *   Returns number of channels.
 */
int
wlan_cfg80211_get_num_channels(struct ieee80211com *ic, enum ieee80211_phymode phymode)

{
    struct ieee80211_ath_channel *chan;
    int i, nchan = 0;
    for (i = 0; i < ic->ic_nchans; i++) {
        chan = &ic->ic_channels[i];

        if (phymode == IEEE80211_MODE_AUTO ||
                phymode == ieee80211_chan2mode(chan)) {
            nchan++;
        }
    }
    return nchan;
}

/**
 *   wlan_cfg80211_populate_channels: This function populates channels list for
 *   given phymode and phyband.
 *   @ic: pointer to ieee80211com.
 *   @phymodie: phymode (2G/5G) for internal driver usage.
 *   @phy_band: band 2G/5G for cfg80211.
 *   @channel_list: pointer to channel list wihch needs to be filled.
 */
int wlan_cfg80211_populate_channels(struct ieee80211com *ic, enum ieee80211_phymode phymode,
        enum ieee80211_band phy_band,
        struct ieee80211_channel *channel_list)
{
    struct ieee80211_ath_channel *chan;
    int i, nchan = 0;

    for (i = 0; i < ic->ic_nchans; i++) {
        chan = &ic->ic_channels[i];
        if (phymode == ieee80211_chan2mode(chan)) {
            channel_list->band =  phy_band;
            channel_list->center_freq = chan->ic_freq;
            channel_list->hw_value = chan->ic_ieee;
            channel_list->flags = 0;
            channel_list->max_antenna_gain = chan->ic_antennamax;
            channel_list->max_power = chan->ic_maxregpower;
            channel_list++;
        }
    }
    return nchan;
}

/**
 *   wlan_cfg80211_populate_band_2g: This function populates ieee80211_supported_band
 *   data struccture, ic contains list of channel that are supported for that radio.
 *   populate channels, bitrates, HTcap and VHTcap for this radio.
 *   @ic: pointer to ieee80211com.
 *   @band: pointer to ieee80211_supported_band.
 *   @channel_list: pointer to channel list wihch needs to be filled.
 */
void wlan_cfg80211_populate_band_2g(struct ieee80211com *ic, struct ieee80211_supported_band *band, struct ieee80211_channel *channel_list,
        int num_channels)
{
    /* Fill band data struture */
    u16 mcs_map = 0, i = 0;
    wlan_cfg80211_populate_channels(ic, IEEE80211_MODE_11G, IEEE80211_BAND_2GHZ, channel_list);

    band->channels = channel_list;
    band->n_channels = num_channels;
    band->band = IEEE80211_BAND_2GHZ;
    band->bitrates = wlan_cfg80211_rates_2g;
    band->n_bitrates = wlan_cfg80211_rates_2g_size;
    band->ht_cap.ht_supported = (ic->ic_caps & IEEE80211_C_HT) ? 1 : 0;
    band->ht_cap.cap = ic->ic_htcap;
    band->ht_cap.ampdu_factor = ic->ic_maxampdu;
    band->ht_cap.ampdu_density = ic->ic_mpdudensity;
    for (i = 0; i < MIN(ic->ic_num_rx_chain, IEEE80211_MAX_11N_CHAINS); i++) {
        band->ht_cap.mcs.rx_mask[i] = 0xFF;
    }
    band->ht_cap.mcs.tx_params = IEEE80211_HT_MCS_TX_DEFINED;
    /* vht not supported in 2G ???*/
    band->vht_cap.vht_supported = (ic->ic_caps_ext & IEEE80211_CEXT_11AC) ? 1 : 0;
    band->vht_cap.cap = ic->ic_vhtcap;

    for (i = 0; i < 8; i++) {
        if (i < ic->ic_num_rx_chain)
            mcs_map |= IEEE80211_VHT_MCS_SUPPORT_0_9 << (i*2);
        else
            mcs_map |= IEEE80211_VHT_MCS_NOT_SUPPORTED << (i*2);
    }

    band->vht_cap.vht_mcs.rx_mcs_map = cpu_to_le16(mcs_map);
    band->vht_cap.vht_mcs.tx_mcs_map = cpu_to_le16(mcs_map);
}

/**
 *   wlan_cfg80211_populate_band_5g: This function populates ieee80211_supported_band
 *   data struccture, ic contains list of channel that are supported for that radio.
 *   populate channels, bitrates, HTcap and VHTcap for this radio.
 *   @ic: pointer to ieee80211com.
 *   @band: pointer to ieee80211_supported_band.
 *   @channel_list: pointer to channel list wihch needs to be filled.
 */
void wlan_cfg80211_populate_band_5g(struct ieee80211com *ic, struct ieee80211_supported_band *band, struct ieee80211_channel *channel_list,
        int num_channels)
{
    /* Fill band data struture */
    u16 mcs_map = 0, i = 0;
    wlan_cfg80211_populate_channels(ic, IEEE80211_MODE_11A, IEEE80211_BAND_5GHZ, channel_list);

    band->channels = channel_list;
    band->n_channels = wlan_cfg80211_get_num_channels(ic, IEEE80211_MODE_11A);
    band->band = IEEE80211_BAND_5GHZ;

    if (WLAN_REG_IS_49GHZ_FREQ(band->channels[i].center_freq)) {
        if (ic->ic_chanbwflag ==  IEEE80211_CHAN_HALF) {
            band->bitrates = wlan_cfg80211_rates_49g_half;
            band->n_bitrates = wlan_cfg80211_rates_49g_half_size;
        } else if (ic->ic_chanbwflag ==  IEEE80211_CHAN_QUARTER) {
            band->bitrates = wlan_cfg80211_rates_49g_quarter;
            band->n_bitrates = wlan_cfg80211_rates_49g_quarter_size;
        } else {
            band->bitrates = wlan_cfg80211_rates_5g;
            band->n_bitrates = wlan_cfg80211_rates_5g_size;
        }
    } else {
        band->bitrates = wlan_cfg80211_rates_5g;
        band->n_bitrates = wlan_cfg80211_rates_5g_size;
    }

    band->ht_cap.ht_supported = (ic->ic_caps & IEEE80211_C_HT) ? 1 : 0;
    band->ht_cap.cap = ic->ic_htcap;
    band->ht_cap.ampdu_factor = ic->ic_maxampdu;
    band->ht_cap.ampdu_density = ic->ic_mpdudensity;
    for (i = 0; i < MIN(ic->ic_num_rx_chain, IEEE80211_MAX_11N_CHAINS); i++) {
        band->ht_cap.mcs.rx_mask[i] = 0xFF;
    }
    band->ht_cap.mcs.tx_params = IEEE80211_HT_MCS_TX_DEFINED;
    band->vht_cap.vht_supported = (ic->ic_caps_ext & IEEE80211_CEXT_11AC) ? 1 : 0;
    band->vht_cap.cap = ic->ic_vhtcap;

    for (i = 0; i < 8; i++) {
        if (i < ic->ic_num_rx_chain)
            mcs_map |= IEEE80211_VHT_MCS_SUPPORT_0_9 << (i*2);
        else
            mcs_map |= IEEE80211_VHT_MCS_NOT_SUPPORTED << (i*2);
    }

    band->vht_cap.vht_mcs.rx_mcs_map = cpu_to_le16(mcs_map);
    band->vht_cap.vht_mcs.tx_mcs_map = cpu_to_le16(mcs_map);
}

/**
 * wlan_cfg80211_update_channel_list: populate update channelist in wiphy
 * @ic: pointer to ieee80211come structure
 * returns 0 on success and -1 on failure
 */

int wlan_cfg80211_update_channel_list(struct ieee80211com *ic)
{
    struct ieee80211_supported_band *wlan_band_2g;
    struct ieee80211_supported_band *wlan_band_5g;
    unsigned int num_channels_2g = 0, num_channels_5g = 0;
    struct wiphy *wiphy = NULL;

    if (!ic->ic_cfg80211_config) {
        return -1;
    }
    wiphy = ic->ic_wiphy;
    wlan_band_2g = wiphy->bands[IEEE80211_BAND_2GHZ];
    wlan_band_5g = wiphy->bands[IEEE80211_BAND_5GHZ];

    /* Band and Channel information */
    num_channels_2g = wlan_cfg80211_get_num_channels(ic, IEEE80211_MODE_11G);
    if (num_channels_2g) {
        wlan_cfg80211_populate_band_2g(ic, wlan_band_2g, ic->channel_list_g, num_channels_2g);
    }

    num_channels_5g = wlan_cfg80211_get_num_channels(ic, IEEE80211_MODE_11A);
    if (num_channels_5g) {
        wlan_cfg80211_populate_band_5g(ic, wlan_band_5g, ic->channel_list_a, num_channels_5g);
    }

    QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO,
              "number of channels: 2G = %d 5G = %d", num_channels_2g, num_channels_5g);
    return 0;
}
EXPORT_SYMBOL(wlan_cfg80211_update_channel_list);

/**
 *   wlan_cfg80211_init: This function initilizes below wiphy params
 *   regulatory_flags, supported features, supported operating modes,
 *   channels supported and vendor commands/events.
 *   @dev: pointer to bus device structure.
 *   @net_dev: pointer to allocated netdevice for radio.
 *   @ic: pointer to ieee80211com.
 *
 *   returns 0/Error.
 */

int wlan_cfg80211_init(struct net_device *dev, struct wiphy *wiphy, struct ieee80211com *ic)
{
    int num_channels = 0;
    struct ieee80211_regdomain *reg_domain = &wlan_cfg80211_world_regdom_60_61_62;

    wiphy->mgmt_stypes = wlan_cfg80211_txrx_stypes;

    /* This will disable updating of NL channels from passive to
     * active if a beacon is received on passive channel.*/
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0))
    wiphy->regulatory_flags |= REGULATORY_DISABLE_BEACON_HINTS;
    wiphy->regulatory_flags |= REGULATORY_CUSTOM_REG;
#else
    wiphy->flags |= WIPHY_FLAG_DISABLE_BEACON_HINTS;
#endif

    /*
     * cfg80211 enables ps by default, clear WIPHY_FLAG_PS_ON_BY_DEFAULT
     * flag.
     */
    wiphy->flags &= ~WIPHY_FLAG_PS_ON_BY_DEFAULT;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0))
    wiphy->flags |=  WIPHY_FLAG_4ADDR_AP
        | WIPHY_FLAG_AP_UAPSD
        | WIPHY_FLAG_REPORTS_OBSS
        | WIPHY_FLAG_4ADDR_STATION
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 15, 0))
        | WIPHY_FLAG_HAS_CHANNEL_SWITCH
#endif
        | WIPHY_FLAG_OFFCHAN_TX
        | WIPHY_FLAG_HAS_REMAIN_ON_CHANNEL;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0))
    wiphy->regulatory_flags = REGULATORY_COUNTRY_IE_IGNORE;
#else
    wiphy->country_ie_pref = NL80211_COUNTRY_IE_IGNORE_CORE;
#endif
#endif

    wiphy->features = 0;    /* enum nl80211_feature_flags */
    /* scan params */
    wiphy->max_scan_ssids = IEEE80211_SCAN_MAX_SSID;
    wiphy->max_scan_ie_len = IEEE80211_MAX_IE_LEN;

    /* Regulatory callback */
    wiphy->reg_notifier = wlan_regulatory_notifier; /* not handling regulatory notifcation */
    wiphy->features |= NL80211_FEATURE_SK_TX_STATUS;    /* enum nl80211_feature_flags */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0))
    wiphy->features |= NL80211_FEATURE_DYNAMIC_SMPS;
#endif

    wiphy->features |= NL80211_FEATURE_FULL_AP_CLIENT_STATE;
    wiphy->features |= NL80211_FEATURE_SAE;

    /*
     * Supports AP and WDS-STA modes right now
     * enum nl80211_iftype
     */
    wiphy->interface_modes = BIT(NL80211_IFTYPE_AP)
        | BIT(NL80211_IFTYPE_STATION)
        | BIT(NL80211_IFTYPE_AP_VLAN)
        | BIT(NL80211_IFTYPE_WDS)
        | BIT(NL80211_IFTYPE_MONITOR);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0))
    wiphy->n_iface_combinations =
        ARRAY_SIZE(wlan_cfg80211_iface_combination);
    wiphy->iface_combinations = wlan_cfg80211_iface_combination;
#endif

   wiphy->max_remain_on_channel_duration = IEEE80211_MAX_REMAIN_ON_CHANNEL;

    /* Band and Channel information */
    num_channels = wlan_cfg80211_get_num_channels(ic, IEEE80211_MODE_11G);
    if (num_channels) {
        qdf_info("Number of 2G channels: %d ", num_channels);
        ic->channel_list_g = (struct ieee80211_channel *)qdf_mem_malloc((sizeof(struct ieee80211_channel) * CFG80211_MAX_CHANNELS_2G));
        if (ic->channel_list_g == NULL) {
            qdf_print("%s: Memory allocation failed ", __func__);
            return -1;
        }
        wlan_cfg80211_populate_band_2g(ic, &ic->wlan_band_2g, ic->channel_list_g, num_channels);
        wiphy->bands[IEEE80211_BAND_2GHZ] = &ic->wlan_band_2g;
    }

    num_channels = wlan_cfg80211_get_num_channels(ic, IEEE80211_MODE_11A);
    if (num_channels) {
        qdf_info("Number of 5G channels: %d ", num_channels);
        ic->channel_list_a = (struct ieee80211_channel *)qdf_mem_malloc((sizeof(struct ieee80211_channel) * CFG80211_MAX_CHANNELS_5G));
        if (ic->channel_list_a == NULL) {
            if (ic->channel_list_g) {
                qdf_mem_free(ic->channel_list_g);
            }
            qdf_print("%s: Memory allocation failed ", __func__);
            return -1;
        }
        wlan_cfg80211_populate_band_5g(ic, &ic->wlan_band_5g, ic->channel_list_a, num_channels);
        wiphy->bands[IEEE80211_BAND_5GHZ] = &ic->wlan_band_5g;
    }

    /* Initialise the supported cipher suite details */
    wiphy->cipher_suites = wlan_cfg80211_cipher_suites;
    wiphy->n_cipher_suites = ARRAY_SIZE(wlan_cfg80211_cipher_suites);

    /*signal strength in mBm (100*dBm) */
    wiphy->signal_type = CFG80211_SIGNAL_TYPE_MBM;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0))
    wiphy->n_vendor_commands =
        ARRAY_SIZE(wlan_cfg80211_vendor_commands);
    wiphy->vendor_commands = wlan_cfg80211_vendor_commands;

    wiphy->vendor_events = wlan_cfg80211_vendor_events;
    wiphy->n_vendor_events =
        ARRAY_SIZE(wlan_cfg80211_vendor_events);
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 15, 0))
    /*TODO: lets populate max peers in ic for both DC and OL */;
    wiphy->max_ap_assoc_sta = CFG_TGT_NUM_PEERS_MAX;
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0))
    wiphy->regulatory_flags |= REGULATORY_DISABLE_BEACON_HINTS;
    wiphy->regulatory_flags |= REGULATORY_CUSTOM_REG;
#else
    wiphy->flags |= WIPHY_FLAG_DISABLE_BEACON_HINTS;
    wiphy->flags |= WIPHY_FLAG_CUSTOM_REGULATORY;
#endif
#ifdef WLAN_SUPPORT_FILS
    wiphy_ext_feature_set(wiphy, NL80211_EXT_FEATURE_FILS_CRYPTO_OFFLOAD);
#endif /* WLAN_SUPPORT_FILS */

    wiphy_apply_custom_regulatory(wiphy, reg_domain);
    /* Register to cfg80211 */
    wlan_cfg80211_register(wiphy);

    return 0;
}

/**
 *   ieee80211_cfg80211_radio_attach: Attach wiphy/cfg80211 dev to kernel.
 *   This function initilizes wiphy data structures, callback etc .. and registers
 *   with kernel.
 *   @dev: pointer to bus device structure.
 *   @net_dev: pointer to allocated netdevice for radio.
 *   @ic: pointer to ieee80211com.
 *
 *   returns 0/Error.
 */
int ieee80211_cfg80211_radio_attach(struct device *dev, struct net_device *net_dev, struct ieee80211com *ic)
{

    struct cfg80211_context *cfg_ctx = NULL;
    struct wiphy *wiphy = NULL;
    struct wlan_objmgr_pdev *pdev = NULL;
    struct wlan_objmgr_psoc *psoc = NULL;
    struct pdev_osif_priv *pdev_ospriv = NULL;
    QDF_STATUS status;

    wiphy = wlan_cfg80211_wiphy_alloc(&wlan_cfg80211_ops, sizeof(struct cfg80211_context));
    if (wiphy == NULL){
        return -1;
    }

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    cfg_ctx->ic = ic;

    /* Store cfg80211 data struct in ic */
    ic->ic_wiphy = wiphy;
    /* bind the underlying wlan device with wiphy */
    wlan_cfg8011_set_wiphy_dev(wiphy, dev);
    wlan_cfg80211_init(net_dev, wiphy, ic);
    LIST_INIT(&ic->ic_cp);

    /* Link allocated netdev with wiphy */
    ic->ic_wdev.wiphy = wiphy;
    ic->ic_wdev.netdev = net_dev;
    ic->ic_wdev.iftype = NL80211_IFTYPE_AP;
    net_dev->ieee80211_ptr = &ic->ic_wdev;
    qdf_info("ic: 0x%pK, wdev: 0x%pK, wiphy: 0x%pK, netdev: 0x%pk ",
             ic, &ic->ic_wdev, wiphy, net_dev);

    pdev = ic->ic_pdev_obj;
    pdev_ospriv = wlan_pdev_get_ospriv(pdev);
    pdev_ospriv->wiphy = wiphy;
    status = wlan_cfg80211_scan_priv_init(ic->ic_pdev_obj);
    if (QDF_IS_STATUS_ERROR(status)) {
        qdf_print("%s: cfg80211_scan_priv_init failed (status: %d), ic: 0x%pK, pdev: 0x%pK",
                __func__, status, ic, pdev);
        return -1;
    }

    psoc = wlan_pdev_get_psoc(pdev);

    /* Register cfg80211 inform bss callback */
    ucfg_scan_register_bcn_cb(psoc, wlan_cfg80211_inform_bss_frame,
            SCAN_CB_TYPE_INFORM_BCN);

    return 0;
}
EXPORT_SYMBOL(ieee80211_cfg80211_radio_attach);

/**
 *   ieee80211_cfg80211_radio_detach: detach wiphy/cfg80211 device
 *   This function unregisters and frees datastructures.
 *   @ic: pointer to ieee80211com.
 *
 *   returns 0/Error.
 */
int ieee80211_cfg80211_radio_detach(struct ieee80211com *ic)
{
    struct net_device *netdev;
    qdf_print("%s: ic: 0x%pK, wdev: 0x%pK, netdev: 0x%pk ",
            __func__, ic, &ic->ic_wdev, ic->ic_wdev.netdev);

    netdev = ic->ic_wdev.netdev;
    wlan_cfg80211_scan_priv_deinit(ic->ic_pdev_obj);
    if (netdev && netdev->reg_state == NETREG_REGISTERED)
        unregister_netdev(netdev);
    wlan_cfg80211_unregister(ic->ic_wiphy);
    wlan_cfg80211_free(ic->ic_wiphy);
    wlan_cfg80211_free_clone_params_list(ic);
    if (ic->channel_list_g) {
        qdf_mem_free(ic->channel_list_g);
    }
    if (ic->channel_list_a) {
        qdf_mem_free(ic->channel_list_a);
    }
    return 0;
}
EXPORT_SYMBOL(ieee80211_cfg80211_radio_detach);


/**
 * wlan_copy_bssid() - API to copy the bssid to vendor Scan request
 * @request: Pointer to vendor scan request
 * @bssid: Pointer to BSSID
 *
 * This API copies the specific BSSID received from Supplicant and copies it to
 * the vendor Scan request
 *
 * Return: None
 */
#if defined(CFG80211_SCAN_BSSID) || \
    (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 7, 0))
static inline void wlan_copy_bssid(struct cfg80211_scan_request *request,
                    uint8_t *bssid)
{
    qdf_mem_copy(request->bssid, bssid, QDF_MAC_ADDR_SIZE);
}
#else
static inline void wlan_copy_bssid(struct cfg80211_scan_request *request,
                    uint8_t *bssid)
{
}
#endif

/**
 * wlan_get_rates() -API to get the rates from scan request
 * @wiphy: Pointer to wiphy
 * @band: Band
 * @rates: array of rates
 * @rate_count: number of rates
 *
 * Return: o for failure, rate bitmap for success
 */
static uint32_t wlan_get_rates(struct wiphy *wiphy,
    enum nl80211_band band,
    const u8 *rates, unsigned int rate_count)
{
    uint32_t j, count, rate_bitmap = 0;
    uint32_t rate;
    bool found;

    for (count = 0; count < rate_count; count++) {
        rate = ((rates[count]) & RATE_MASK) * 5;
        found = false;
        for (j = 0; j < wiphy->bands[band]->n_bitrates; j++) {
            if (wiphy->bands[band]->bitrates[j].bitrate == rate) {
                found = true;
                rate_bitmap |= (1 << j);
                break;
            }
        }
        if (!found)
            return 0;
    }
    return rate_bitmap;
}


/**
 * wlan_send_scan_start_event() -API to send the scan start event
 * @wiphy: Pointer to wiphy
 * @wdev: Pointer to net device
 * @cookie: scan identifier
 *
 * Return: return 0 on success and negative error code on failure
 */
static int wlan_send_scan_start_event(struct wiphy *wiphy,
        struct wireless_dev *wdev, uint64_t cookie)
{
    struct sk_buff *skb;
    int ret;
    QDF_STATUS status;

    skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, sizeof(u64) +
            NLA_HDRLEN + NLMSG_HDRLEN);
    if (!skb) {
        qdf_print(" reply skb alloc failed");
        return -ENOMEM;
    }

    if (wlan_nla_put_u64(skb, QCA_WLAN_VENDOR_ATTR_SCAN_COOKIE,
                 cookie)) {
        qdf_print("nla put fail");
        kfree_skb(skb);
        return -EINVAL;
    }

    status = qal_devcfg_send_response((qdf_nbuf_t)skb);
    ret = qdf_status_to_os_return(status);

    /* Send a scan started event to supplicant */
    skb = cfg80211_vendor_event_alloc(wiphy, wdev,
        sizeof(u64) + 4 + NLMSG_HDRLEN,
        QCA_NL80211_VENDOR_SUBCMD_SCAN_INDEX, GFP_KERNEL);
    if (!skb) {
        qdf_print("skb alloc failed");
        return -ENOMEM;
    }

    if (wlan_nla_put_u64(skb, QCA_WLAN_VENDOR_ATTR_SCAN_COOKIE,
                 cookie)) {
        kfree_skb(skb);
        return -EINVAL;
    }
    cfg80211_vendor_event(skb, GFP_KERNEL);
    if (0 == ret) {
        qdf_print("Sending scan done vendor event %s",__func__);
    } else {
        qdf_print("Failed to send scan done vendor event %s",__func__);
    }

    return ret;
}

/**
 * __wlan_cfg80211_vendor_scan() - API to process venor scan request
 * @wiphy: Pointer to wiphy
 * @wdev: Pointer to net device
 * @data : Pointer to the data
 * @data_len : length of the data
 *
 * API to process venor scan request.
 *
 * Return: return 0 on success and negative error code on failure
 */
static int __wlan_cfg80211_vendor_scan(struct wiphy *wiphy,
        struct wireless_dev *wdev, const void *data,
        int data_len)
{
    struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_SCAN_MAX + 1];
    struct cfg80211_scan_request *request = NULL;
    struct nlattr *attr;
    enum nl80211_band band;
    uint8_t n_channels = 0, n_ssid = 0, ie_len = 0;
    uint32_t tmp, count, j;
    unsigned int len;
    struct ieee80211_channel *chan;
    int ret;
    struct wlan_objmgr_pdev *pdev;
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;
    struct net_device *dev = wdev->netdev;
    osif_dev *osifp = ath_netdev_priv(dev);
    struct wlan_objmgr_vdev *vdev = osifp->ctrl_vdev;
    struct scan_params params;
#if ATH_SUPPORT_WRAP
    wlan_if_t vap;
#endif

    qdf_mem_zero(&params, sizeof(params));

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;
    pdev = ic->ic_pdev_obj;

#if ATH_SUPPORT_WRAP
    /* Donot initiate scan for Proxy stations */
    vap = osifp->os_if;
    if (vap->iv_no_event_handler) {
        qdf_info("%s:Bypass scan for proxysta", __func__);
        return QDF_STATUS_SUCCESS;
    }
#endif

    if (ic->no_chans_available
#if ATH_SUPPORT_DFS && QCA_DFS_NOL_VAP_RESTART
            || ic->ic_pause_stavap_scan
#endif
       ) {
        return -EINVAL;
    }

    if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_SCAN_MAX, data,
        data_len, NULL)) {
        qdf_print("Invalid ATTR");
        return -EINVAL;
    }

    if (tb[QCA_WLAN_VENDOR_ATTR_SCAN_FREQUENCIES]) {
        nla_for_each_nested(attr,
            tb[QCA_WLAN_VENDOR_ATTR_SCAN_FREQUENCIES], tmp)
            n_channels++;
    } else {
        for (band = 0; band < NUM_NL80211_BANDS; band++)
            if (wiphy->bands[band])
                n_channels += wiphy->bands[band]->n_channels;
    }

    if (MAX_CHANNEL < n_channels) {
        qdf_print("Exceed max number of channels: %d", n_channels);
        return -EINVAL;
    }
    if (tb[QCA_WLAN_VENDOR_ATTR_SCAN_SSIDS])
        nla_for_each_nested(attr,
            tb[QCA_WLAN_VENDOR_ATTR_SCAN_SSIDS], tmp)
            n_ssid++;

    if (MAX_SCAN_SSID < n_ssid) {
        qdf_print("Exceed max number of SSID: %d", n_ssid);
        return -EINVAL;
    }

    if (tb[QCA_WLAN_VENDOR_ATTR_SCAN_IE])
        ie_len = nla_len(tb[QCA_WLAN_VENDOR_ATTR_SCAN_IE]);
    else
        ie_len = 0;

    len = sizeof(*request) + (sizeof(*request->ssids) * n_ssid) +
            (sizeof(*request->channels) * n_channels) + ie_len;

    request = qdf_mem_malloc(len);
    if (!request)
        goto error;
    if (n_ssid)
        request->ssids = (void *)&request->channels[n_channels];
    request->n_ssids = n_ssid;
    if (ie_len) {
        if (request->ssids)
            request->ie = (void *)(request->ssids + n_ssid);
        else
            request->ie = (void *)(request->channels + n_channels);
    }

    count = 0;
    if (tb[QCA_WLAN_VENDOR_ATTR_SCAN_FREQUENCIES]) {
        nla_for_each_nested(attr,
                tb[QCA_WLAN_VENDOR_ATTR_SCAN_FREQUENCIES],
                tmp) {
            chan = __ieee80211_get_channel(wiphy,
                            nla_get_u32(attr));
            if (!chan)
                goto error;
            if (chan->flags & IEEE80211_CHAN_DISABLED)
                continue;
            request->channels[count] = chan;
            count++;
        }
    } else {
        for (band = 0; band < NUM_NL80211_BANDS; band++) {
            if (!wiphy->bands[band])
                continue;
            for (j = 0; j < wiphy->bands[band]->n_channels;
                j++) {
                chan = &wiphy->bands[band]->channels[j];
                if (chan->flags & IEEE80211_CHAN_DISABLED)
                    continue;
                request->channels[count] = chan;
                count++;
            }
        }
    }

    if (!count)
        goto error;

    request->n_channels = count;
    count = 0;
    if (tb[QCA_WLAN_VENDOR_ATTR_SCAN_SSIDS]) {
        nla_for_each_nested(attr, tb[QCA_WLAN_VENDOR_ATTR_SCAN_SSIDS],
                tmp) {

            if (!request->ssids){
                qdf_print("%s:Returning as ssid is NULL", __func__);
                return -EINVAL;
            }

            request->ssids[count].ssid_len = nla_len(attr);
            if (request->ssids[count].ssid_len >
                SIR_MAC_MAX_SSID_LENGTH) {
                qdf_print("SSID Len %d is not correct for network %d",
                     request->ssids[count].ssid_len, count);
                goto error;
            }
            memcpy(request->ssids[count].ssid, nla_data(attr),
                    nla_len(attr));
            count++;
        }
    }

    if (tb[QCA_WLAN_VENDOR_ATTR_SCAN_IE]) {
        request->ie_len = nla_len(tb[QCA_WLAN_VENDOR_ATTR_SCAN_IE]);
        memcpy((void *)request->ie,
                nla_data(tb[QCA_WLAN_VENDOR_ATTR_SCAN_IE]),
                request->ie_len);
    }

    for (count = 0; count < NUM_NL80211_BANDS; count++)
        if (wiphy->bands[count])
            request->rates[count] =
                (1 << wiphy->bands[count]->n_bitrates) - 1;

    if (tb[QCA_WLAN_VENDOR_ATTR_SCAN_SUPP_RATES]) {
        nla_for_each_nested(attr,
                    tb[QCA_WLAN_VENDOR_ATTR_SCAN_SUPP_RATES],
                    tmp) {
            band = nla_type(attr);
            if (band >= NUM_NL80211_BANDS)
                continue;
            if (!wiphy->bands[band])
                continue;
            request->rates[band] =
                wlan_get_rates(wiphy,
                               band, nla_data(attr),
                               nla_len(attr));
        }
    }

    if (tb[QCA_WLAN_VENDOR_ATTR_SCAN_FLAGS]) {
        request->flags =
            nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SCAN_FLAGS]);
        if ((request->flags & NL80211_SCAN_FLAG_LOW_PRIORITY) &&
            !(wiphy->features & NL80211_FEATURE_LOW_PRIORITY_SCAN)) {
            qdf_print("LOW PRIORITY SCAN not supported");
            goto error;
        }
    }

    if (tb[QCA_WLAN_VENDOR_ATTR_SCAN_BSSID]) {
        if (nla_len(tb[QCA_WLAN_VENDOR_ATTR_SCAN_BSSID]) <
            QDF_MAC_ADDR_SIZE) {
            qdf_print("invalid bssid length");
            goto error;
        }
        wlan_copy_bssid(request,
            nla_data(tb[QCA_WLAN_VENDOR_ATTR_SCAN_BSSID]));
    }

    request->no_cck =
        nla_get_flag(tb[QCA_WLAN_VENDOR_ATTR_SCAN_TX_NO_CCK_RATE]);
    request->wdev = wdev;
    request->wiphy = wiphy;
    request->scan_start = jiffies;

    /* Initialize driver specific scan config param */
    params.source = VENDOR_SCAN;
    params.default_ie.len = 0;
    params.default_ie.ptr = NULL;
    params.half_rate = false;
    params.quarter_rate= false;
    params.strict_pscan = false;

    if (request->n_channels) {
        if (ic->ic_chanbwflag & IEEE80211_CHAN_HALF)
            params.half_rate = true;
        else if (ic->ic_chanbwflag & IEEE80211_CHAN_QUARTER)
            params.quarter_rate= true;
    }
    if (ic->ic_strict_pscan_enable) {
        params.strict_pscan = true;
    }

    ret = wlan_objmgr_vdev_try_get_ref(vdev, WLAN_OSIF_ID);
    if (QDF_IS_STATUS_ERROR(ret)) {
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_CFG80211,
                "%s Couldnt get vdev ref, fail new scan\n", __func__);
        goto error;
    }

    ret = wlan_cfg80211_scan(vdev, request, &params);
    wlan_objmgr_vdev_release_ref(vdev, WLAN_OSIF_ID);

    if (0 != ret) {
        qdf_print("Scan Failed. Ret = %d", ret);
        qdf_mem_free(request);
        return ret;
    }
    ret = wlan_send_scan_start_event(wiphy, wdev, (uintptr_t)request);

    return ret;
error:
    qdf_print("Scan Request Failed");
    qdf_mem_free(request);
    return -EINVAL;
}

/**
 * wlan_cfg80211_vendor_scan() -API to process venor scan request
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to net device
 * @data : Pointer to the data
 * @data_len : length of the data
 *
 * This is called from userspace to request scan.
 *
 * Return: Return the Success or Failure code.
 */
int wlan_cfg80211_vendor_scan(struct wiphy *wiphy,
        struct wireless_dev *wdev, const void *data,
        int data_len)
{
    int ret;

    ret = __wlan_cfg80211_vendor_scan(wiphy, wdev,
                                      data, data_len);

    return ret;
}

static int update_acs_channel(wlan_if_t vap, uint8_t reason,
                  uint8_t channel_cnt,
                  struct vendor_chan_info *channel_list)
{
    QDF_STATUS status = QDF_STATUS_SUCCESS;
    struct ieee80211_ath_channel *best_chan;

    switch (reason) {
        /* VAP init case */
        case QCA_WLAN_VENDOR_ACS_SELECT_REASON_INIT:
        {
            int chan;
            enum ieee80211_phymode act_phymode;
            uint8_t secchanoffset = IEEE80211_SEC_CHAN_OFFSET_SCN;
            enum ieee80211_mode mode;

            if (channel_list->chan_width == IEEE80211_CWM_WIDTH40) {
                    if (channel_list->ht_sec_ch > channel_list->pri_ch)
                       secchanoffset = IEEE80211_SEC_CHAN_OFFSET_SCA;
                    else if (channel_list->ht_sec_ch < channel_list->pri_ch)
                       secchanoffset = IEEE80211_SEC_CHAN_OFFSET_SCB;
            }

            mode = get_mode_from_phymode(vap->iv_des_mode);
            act_phymode = ieee80211_get_composite_phymode(mode,
                                                          channel_list->chan_width,
                                                          secchanoffset);

            ieee80211_ucfg_set_wirelessmode(vap, act_phymode);
            best_chan = ieee80211_find_dot11_channel(vap->iv_ic, channel_list->pri_ch,
                                             0, act_phymode);
            if (!best_chan){
                qdf_print("%s:Finding channel failed.",__func__);
                qdf_mem_free(channel_list);
                return -EINVAL;
            }
            chan = wlan_channel_ieee(best_chan);

            /* Update Hostapd */
            wlan_cfg80211_acs_report_channel(vap, best_chan);
            break;
        }
        default:
            qdf_print("invalid reason for channel selection");
    }
    qdf_mem_free(channel_list);
    return status;
}

/**
 * Define short name for vendor channel set config
 */
#define SET_CHAN_REASON QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_CHANNEL_REASON
#define SET_CHAN_CHAN_LIST QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_CHANNEL_LIST
#define SET_CHAN_PRIMARY_CHANNEL \
    QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_CHANNEL_PRIMARY
#define SET_CHAN_SECONDARY_CHANNEL \
    QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_CHANNEL_SECONDARY
#define SET_CHAN_SEG0_CENTER_CHANNEL \
    QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_CHANNEL_CENTER_SEG0
#define    SET_CHAN_SEG1_CENTER_CHANNEL \
    QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_CHANNEL_CENTER_SEG1
#define    SET_CHAN_CHANNEL_WIDTH \
    QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_CHANNEL_WIDTH
#define    SET_CHAN_MAX QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_CHANNEL_MAX

/**
 * parse_vendor_acs_chan_config() - API to parse vendor acs channel config
 * @channel_list: pointer to vendor_chan_info
 * @reason: channel change reason
 * @channel_cnt: channel count
 * @data: data
 * @data_len: data len
 *
 * Return: 0 on success, negative errno on failure
 */
static int parse_vendor_acs_chan_config(struct vendor_chan_info
        **chan_list_ptr, uint8_t *reason, uint8_t *channel_cnt,
        const void *data, int data_len)
{
    int rem, i = 0;
    struct nlattr *tb[SET_CHAN_MAX + 1];
    struct nlattr *tb2[SET_CHAN_MAX + 1];
    struct nlattr *curr_attr;
    struct vendor_chan_info *channel_list;

    if (nla_parse(tb, SET_CHAN_MAX, data, data_len, NULL)) {
        qdf_print("Invalid ATTR");
        return -EINVAL;
    }

    if (tb[SET_CHAN_REASON])
        *reason = nla_get_u8(tb[SET_CHAN_REASON]);

    /* Hard coding the channel count to 1 for now */
    *channel_cnt = 1;
    qdf_print("channel count %d", *channel_cnt);

    if (!(*channel_cnt)) {
        qdf_print("channel count is %d", *channel_cnt);
        return -EINVAL;
    }

    channel_list = qdf_mem_malloc(sizeof(struct vendor_chan_info) *
                    (*channel_cnt));

    if (!channel_list){
        qdf_print("%s: qdf_mem_malloc failed.", __func__);
        return -EINVAL;
    }

    nla_for_each_nested(curr_attr, tb[SET_CHAN_CHAN_LIST], rem) {
        if (nla_parse(tb2,
            SET_CHAN_MAX,
            nla_data(curr_attr), nla_len(curr_attr),
            NULL)) {
            qdf_print("nla_parse failed");
            qdf_mem_free(channel_list);
            return -EINVAL;
        }
        /* Parse and Fetch allowed SSID list*/
        if (tb2[SET_CHAN_PRIMARY_CHANNEL]) {
            channel_list[i].pri_ch =
                nla_get_u8(
                    tb2[SET_CHAN_PRIMARY_CHANNEL]);
        }
        if (tb2[SET_CHAN_SECONDARY_CHANNEL]) {
            channel_list[i].ht_sec_ch =
                nla_get_u8(tb2[SET_CHAN_SECONDARY_CHANNEL]);
        }
        if (tb2[SET_CHAN_SEG0_CENTER_CHANNEL]) {
            channel_list[i].vht_seg0_center_ch =
                nla_get_u8(tb2[SET_CHAN_SEG0_CENTER_CHANNEL]);
        }
        if (tb2[SET_CHAN_SEG1_CENTER_CHANNEL]) {
            channel_list[i].vht_seg1_center_ch =
                nla_get_u8(tb2[SET_CHAN_SEG1_CENTER_CHANNEL]);
        }
        if (tb2[SET_CHAN_CHANNEL_WIDTH]) {
            channel_list[i].chan_width =
                nla_get_u8(tb2[SET_CHAN_CHANNEL_WIDTH]);
        }
        qdf_print("index %d pri %d sec %d seg0 %d seg1 %d width %d",
            i, channel_list[i].pri_ch,
            channel_list[i].ht_sec_ch,
            channel_list[i].vht_seg0_center_ch,
            channel_list[i].vht_seg1_center_ch,
            channel_list[i].chan_width);
        i++;
        if (i > *channel_cnt)
            break;
    }
    *chan_list_ptr = channel_list;

    return 0;
}

/**
 * Undef short names for vendor set channel configuration
 */
#undef SET_CHAN_REASON
#undef SET_CHAN_CHANNEL_COUNT
#undef SET_CHAN_CHAN_LIST
#undef SET_CHAN_PRIMARY_CHANNEL
#undef SET_CHAN_SECONDARY_CHANNEL
#undef SET_CHAN_SEG0_CENTER_CHANNEL
#undef SET_CHAN_SEG1_CENTER_CHANNEL
#undef SET_CHAN_CHANNEL_WIDTH
#undef SET_CHAN_MAX

/**
 * __wlan_cfg80211_update_vendor_channel() - update vendor channel
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Length of @data
 *
 * Return: 0 on success, negative errno on failure
 */
static int __wlan_cfg80211_update_vendor_channel(struct wiphy *wiphy,
        struct wireless_dev *wdev,
        const void *data, int data_len)
{
    int ret_val;
    QDF_STATUS qdf_status;
    uint8_t channel_cnt = 0, reason = -1;
    struct vendor_chan_info *channel_list = NULL;
    osif_dev *osifp = ath_netdev_priv(wdev->netdev);
    wlan_if_t vap = osifp->os_if;

    ret_val = parse_vendor_acs_chan_config(&channel_list, &reason,
                    &channel_cnt, data, data_len);
    if (ret_val)
        return ret_val;

    if ((channel_cnt <= 0) || !channel_list) {
        qdf_print("no available channel/chanlist %pK", channel_list);
        return -EINVAL;
    }

    qdf_status = update_acs_channel(vap, reason,
                      channel_cnt, channel_list);
    return qdf_status_to_os_return(qdf_status);
}

/**
 * wlan_cfg80211_update_vendor_channel() - update vendor channel
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Length of @data
 *
 * Return: 0 on success, negative errno on failure
 */
int wlan_cfg80211_update_vendor_channel(struct wiphy *wiphy,
                        struct wireless_dev *wdev,
                        const void *data, int data_len)
{
    int ret;

    ret = __wlan_cfg80211_update_vendor_channel(wiphy, wdev, data,
                                data_len);

    return ret;
}

/*
 * Fill the survey results (total survey time, total time channel is busy, tx time)
 * Steps to calculate time in ms from cycle count is given below
 * time = cycle_count * cycle
 * cycle = 1 / clock_freq
 * Since the unit of clock_freq reported from
 * FW is MHZ, and we want to calculate time in
 * ms level, the result is
 * time = cycle / (clock_freq * 1000)
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 0, 0))
static bool wlan_fill_survey_result(struct survey_info *survey,
                                    int opfreq,
                                    struct scan_chan_info *chan_info,
                                    struct ieee80211_channel *channel)
{
    uint32_t clock_freq = chan_info->clock_freq * 1000;

    if (channel->center_freq != (uint16_t)chan_info->freq)
        return false;

    survey->channel = channel;
    survey->noise = chan_info->noise_floor;
    survey->filled = SURVEY_INFO_NOISE_DBM;

    if (opfreq == chan_info->freq)
        survey->filled |= SURVEY_INFO_IN_USE;

    if (clock_freq == 0)
        return true;

    survey->time = chan_info->cycle_count / clock_freq;
    survey->time_busy = chan_info->rx_clear_count / clock_freq;
    survey->time_tx = chan_info->tx_frame_count / clock_freq;

    survey->filled |= SURVEY_INFO_TIME |
            SURVEY_INFO_TIME_BUSY |
            SURVEY_INFO_TIME_TX;
    return true;
}
#else
static bool wlan_fill_survey_result(struct survey_info *survey,
                                    int opfreq,
                                    struct scan_chan_info *chan_info,
                                    struct ieee80211_channel *channel)
{
    uint64_t clock_freq = chan_info->clock_freq * 1000;

    if (channel->center_freq != (uint16_t)chan_info->freq)
        return false;

    survey->channel = channel;
    survey->noise = chan_info->noise_floor;
    survey->filled = SURVEY_INFO_NOISE_DBM;

    if (opfreq == chan_info->freq)
        survey->filled |= SURVEY_INFO_IN_USE;

    if (clock_freq == 0)
        return true;

    survey->channel_time = chan_info->cycle_count / clock_freq;
    survey->channel_time_busy = chan_info->rx_clear_count / clock_freq;
    survey->channel_time_tx = chan_info->tx_frame_count / clock_freq;

    survey->filled |= SURVEY_INFO_CHANNEL_TIME |
            SURVEY_INFO_CHANNEL_TIME_BUSY |
            SURVEY_INFO_CHANNEL_TIME_TX;
    return true;
}
#endif

static bool wlan_hdd_update_survey_info(struct wiphy *wiphy,
                                        struct external_acs_obj *extacs_handle,
                                        struct survey_info *survey,
                                        int idx)
{
    bool filled = false;
    int i = 0;
    int j = 0;
    int k = 0;
    uint32_t opfreq = 0;
    int valid_idx = 0;
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic = NULL;

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    if (ic->ic_curchan)
        opfreq = ic->ic_curchan->ic_freq;

    /* find the idxth non zero element from the start of array */
    while (k < (NUM_MAX_CHANNELS -1)) {
        if (extacs_handle->chan_info[k].freq != 0 && valid_idx++ == idx)
            break;
        k++;
    }
    if (k  != (NUM_MAX_CHANNELS -1))
        idx = k;
    else
        return false;

    qdf_spin_lock(&extacs_handle->chan_info_lock);
    for (i = 0; i < IEEE80211_NUM_BANDS && !filled; i++) {
        if (wiphy->bands[i] == NULL)
            continue;

        for (j = 0; j < wiphy->bands[i]->n_channels && !filled; j++) {
            struct ieee80211_supported_band *band = wiphy->bands[i];
            filled = wlan_fill_survey_result(survey, opfreq,
            &extacs_handle->chan_info[idx],
            &band->channels[j]);
        }
    }
    qdf_spin_unlock(&extacs_handle->chan_info_lock);

    return filled;
}

/**
 * __wlan_hdd_cfg80211_dump_survey() - get survey related info
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to network device
 * @idx: Index
 * @survey: Pointer to survey info
 *
 * Return: 0 for success, non-zero for failure
 */
static int __wlan_hdd_cfg80211_dump_survey(struct wiphy *wiphy,
                                           struct net_device *dev,
                                           int idx,
                                           struct survey_info *survey)
{
    bool filled = false;
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com *ic;

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    if (idx > NUM_MAX_CHANNELS - 1)
        return -EINVAL;

    filled = wlan_hdd_update_survey_info(wiphy, &ic->ic_extacs_obj, survey, idx);

    if (!filled) {
        return -ENONET;
    }

   return 0;
}

/**
 * wlan_hdd_cfg80211_dump_survey() - get survey related info
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to network device
 * @idx: Index
 * @survey: Pointer to survey info
 *
 * Return: 0 for success, non-zero for failure
 */
int wlan_hdd_cfg80211_dump_survey(struct wiphy *wiphy,
                                  struct net_device *dev,
                                  int idx,
                                  struct survey_info *survey)
{
    return  __wlan_hdd_cfg80211_dump_survey(wiphy, dev, idx, survey);
}

/**
 * _wlan_cfg80211_vendor_get_rropinfo() - Internal API to get RROP info
 * @wiphy: Pointer to wiphy
 * @wdev: Pointer to net device
 * @data: Pointer to the data
 * @data_len: length of the data
 *
 * Internal API to process vendor specific request to get Representative RF
 * Operating Parameter (RROP) information.
 *
 * Return: return 0 on success and negative error code on failure
 */
static int __wlan_cfg80211_vendor_get_rropinfo(struct wiphy *wiphy,
        struct wireless_dev *wdev, const void *data,
        int data_len)
{
    struct ieee80211com *ic = NULL;
    struct external_acs_obj *extacs_handle = NULL;
    struct sk_buff *skb = NULL;
    struct nlattr *nla_attr = NULL;
    struct nlattr *rtplinst_attr = NULL;
    struct scan_chan_info *chan_info = NULL;
    bool info_available = false;
    int ret = 0, i = 0;
    QDF_STATUS status;

    ic = ((struct cfg80211_context *)wiphy_priv(wiphy))->ic;
    qdf_assert_always(ic != NULL);

    extacs_handle = &ic->ic_extacs_obj;

    skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, RROP_REPLY_BUF_SIZE);

    if (!skb) {
        qdf_print("Reply skb alloc failed");
        return -ENOMEM;
    }

    nla_attr = nla_nest_start(skb, QCA_WLAN_VENDOR_ATTR_RROP_INFO_RTPL);

    if (!nla_attr) {
        qdf_print("nla nest start failed for RTPL info");
        ret = -EINVAL;
        goto preskbsend_fail;
    }

    qdf_spin_lock(&extacs_handle->chan_info_lock);

    for (i = 0; i < QDF_ARRAY_SIZE(ic->ic_extacs_obj.chan_info); i++) {
        chan_info = &ic->ic_extacs_obj.chan_info[i];

        /*
         * An entry is considered unpopulated if the freq and tx power fields
         * are all zero.
         * TODO: If the channel info in ic->ic_extacs_obj is later optimized to
         * explicitly indicate which channel entries have valid information, we
         * can optionally change the below check.
         */
        if ((chan_info->freq == 0) && (chan_info->tx_power_tput == 0) &&
                (chan_info->tx_power_range == 0))
            continue;

        info_available = true;

        rtplinst_attr = nla_nest_start(skb, i);

        if (!rtplinst_attr) {
            qdf_print("nla nest start failed for RTPL instance");
            ret = -EINVAL;
            qdf_spin_unlock(&extacs_handle->chan_info_lock);
            goto preskbsend_fail;
        }

        /* ic->ic_extacs_obj.chan_info is indexed by primary channel number */
        if (nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_RTPLINST_PRIMARY,
                i) ||
            nla_put_s32(skb, QCA_WLAN_VENDOR_ATTR_RTPLINST_TXPOWER_THROUGHPUT,
                chan_info->tx_power_tput) ||
            nla_put_s32(skb, QCA_WLAN_VENDOR_ATTR_RTPLINST_TXPOWER_RANGE,
                chan_info->tx_power_range)) {
            qdf_print("put failed for RTPL instance");
            ret = -EINVAL;
            qdf_spin_unlock(&extacs_handle->chan_info_lock);
            goto preskbsend_fail;
        }

        nla_nest_end(skb, rtplinst_attr);
    }

    qdf_spin_unlock(&extacs_handle->chan_info_lock);

    if (!info_available) {
        /* The application might have made the API call in an incorrect
         * sequence.
         */
        ret = -EINVAL;
        goto preskbsend_fail;
    }

    nla_nest_end(skb, nla_attr);

    status = qal_devcfg_send_response((qdf_nbuf_t)skb);
    ret = qdf_status_to_os_return(status);

    if (ret == 0) {
        qdf_print("%s: Sending RROP info",__func__);
        return 0;
    } else {
        qdf_print("%s: Failed to send RROP info",__func__);
        return -EINVAL;
    }

preskbsend_fail:
    kfree_skb(skb);
    return ret;
}

/**
 * wlan_cfg80211_vendor_get_rropinfo() - API to get RROP info
 * @wiphy: Pointer to wiphy
 * @wdev: Pointer to net device
 * @data: Pointer to the data
 * @data_len: length of the data
 *
 * API to process vendor specific request to get Representative RF Operating
 * Parameter (RROP) information. This is called from userspace.
 *
 * Return: 0 for success, non-zero for failure
 */
int wlan_cfg80211_vendor_get_rropinfo(struct wiphy *wiphy,
        struct wireless_dev *wdev, const void *data,
        int data_len)
{
    return  __wlan_cfg80211_vendor_get_rropinfo(wiphy, wdev, data, data_len);
}

#define GET_BE16(a) ((u16) (((a)[0] << 8) | (a)[1]))
#define ELEMENT_TYPE 0x1012
#define FIRST_ELEMENT_TYPE_FIELD 0x6
#define ELEMENT_LENGTH_FIELD 0x3
#define ELEMENT_LENGTH 0x2
#define PASSWORD_ID_FIELD 0x4
#define PASSWORD_ID 0x0004

int push_button_method(const u_int8_t *frm)
{
     int len = FIRST_ELEMENT_TYPE_FIELD;
     int rc = 0;

     while ((len+4) < frm[1] ) {
        if((GET_BE16(frm+len) == ELEMENT_TYPE) && (frm[len+ELEMENT_LENGTH_FIELD] == ELEMENT_LENGTH)&& (GET_BE16(frm+len+PASSWORD_ID_FIELD)== PASSWORD_ID))
        {
            rc = 1;
            qdf_print("wps pb detected");
            return rc;
        } else {
          len += frm[len+ELEMENT_LENGTH_FIELD]+4;
        }
     }
     return rc;
}

/**
 * wlan_vendor_fwd_mgmt_frame() - Forward management frame event
 *
 * @vap: Pointer to vap
 * @skb: Pointer to mgmt frame
 * @subtype: frame subtype
 *
 * This function forwards management frame to NL.
 *
 * Return: none
 */
void wlan_vendor_fwd_mgmt_frame(wlan_if_t vap, struct sk_buff *skb, u_int16_t subtype)
{
    osif_dev *osif = (osif_dev *)wlan_vap_get_registered_handle(vap);
    struct net_device *dev = osif->netdev;
    struct ieee80211_frame *wh;
    u_int8_t *frm, *efrm;
    bool send_to_app = false;

    switch (subtype)
    {
        case IEEE80211_FC0_SUBTYPE_PROBE_REQ:
            if (!wlan_get_param(vap, IEEE80211_WPS_MODE)) {
                break;
            }

            if(IEEE80211_VAP_IS_BACKHAUL_ENABLED(vap)) {
                wh = (struct ieee80211_frame *) wbuf_header(skb);
                frm = (u_int8_t *)&wh[1];
                efrm = wbuf_header(skb) + wbuf_get_pktlen(skb);

                while (((frm+1) < efrm) && (frm + frm[1] + 1 < efrm)) {
                    if (iswpsoui(frm) && push_button_method(frm))
                    {
                        send_to_app = true;
                        break;
                    }
                    else
                        frm += frm[1] + 2;
                }
            }
            break;
        case IEEE80211_FC0_SUBTYPE_ACTION:
            send_to_app = true;
            break;
        default:
            break;
    }
    if (send_to_app == false) {
        return;
    }
    wlan_cfg80211_generic_event(vap->iv_ic,
            QCA_NL80211_VENDOR_SUBCMD_FWD_MGMT_FRAME, skb->data,
            skb->len, dev, GFP_ATOMIC);
}

int wlan_cfg80211_set_pmksa(struct wiphy *wiphy, struct net_device *dev, struct cfg80211_pmksa *pmksa)
{
	return 0;
}
int wlan_cfg80211_del_pmksa(struct wiphy *wiphy, struct net_device *dev, struct cfg80211_pmksa *pmksa)
{
	return 0;
}
int wlan_cfg80211_flush_pmksa(struct wiphy *wiphy, struct net_device *dev)
{
	return 0;
}
/**
 * wlan_cfg80211_update_ft_ies() - Updates ft ies provided by supplicant
 *
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to network device
 * @req: Pointer to cfg80211 update ft ies params
 *
 * This function stores the ft ies provided in vap context later
 * used for roaming
 *
 * Return: 0 for success, non-zero for failure
 */
int wlan_cfg80211_update_ft_ies(struct wiphy *wiphy,
        struct net_device *ndev,
        struct cfg80211_update_ft_ies_params *ftie)
{
    struct cfg80211_context *cfg_ctx = NULL;
    struct ieee80211com     *ic = NULL;
    osif_dev                *osifp = ath_netdev_priv(ndev);
    wlan_if_t               vap = osifp->os_if;
    int                     retval = 0;

    cfg_ctx = (struct cfg80211_context *)wiphy_priv(wiphy);
    ic = cfg_ctx->ic;

    if (!vap->iv_roam.iv_ft_enable) {
        retval = -EINVAL;
        goto error;
    }

    if (!ieee80211_vap_is_connected(vap) &&
            !ieee80211_vap_is_connecting(vap)) {
        retval = -EINVAL;
        goto error;
    }

    if (!ftie) {
        retval = -EINVAL;
        goto error;
    }

    if (osifp->is_delete_in_progress) {
        retval = -EINVAL;
        goto error;
    }

    retval = wlan_mlme_set_ftie(vap, ftie->md, (u_int8_t *)ftie->ie, ftie->ie_len);

error:
    if (retval == 0) {
        vap->iv_roam.iv_roaming = 1;
        vap->iv_roam.iv_ft_roam = 1;
    }
    else {
        vap->iv_roam.iv_roaming = 0;
        vap->iv_roam.iv_ft_roam = 0;
    }
    vap->iv_roam.iv_wait_for_ftie_update = 0;

    return retval;
}

/**
 * ieee80211_cfg80211_post_ft_event() - Posts ft event to supplicant
 *
 * @osdev: Pointer to osdev
 *
 * This function posts ft event to supplicant used for ft roaming
 *
 * Return: None
 */
void ieee80211_cfg80211_post_ft_event(osif_dev *osdev)
{
    struct cfg80211_ft_event_params ft_event;
    struct net_device *dev = osdev->netdev;
    wlan_if_t vap = osdev->os_if;

    if (vap->iv_roam.iv_ft_roam && vap->iv_roam.iv_ft_params.fties) {
        qdf_mem_zero(&ft_event, sizeof(ft_event));
        ft_event.ies = vap->iv_roam.iv_ft_params.fties;
        ft_event.ies_len = vap->iv_roam.iv_ft_params.fties_len;
        ft_event.target_ap = vap->iv_roam.iv_ft_params.target_ap;

        cfg80211_ft_event(dev, &ft_event);

        qdf_mem_free(vap->iv_roam.iv_ft_params.fties);
        vap->iv_roam.iv_ft_params.fties = NULL;
        vap->iv_roam.iv_ft_params.fties_len = 0;
        qdf_mem_zero(vap->iv_roam.iv_ft_params.target_ap, IEEE80211_ADDR_LEN);
    }
}

/**
 * clear_roam_params() - Clears roam related fields
 *
 * @dev: Pointer to netdev
 *
 * This function clears roam related fields
 *
 * Return: None
 */
static void clear_roam_params(struct net_device *dev)
{
    osif_dev *osnetdev = ath_netdev_priv(dev);
    wlan_if_t vap = osnetdev->os_if;

    if (vap->iv_roam.iv_roaming) {
        vap->iv_roam.iv_roaming = 0;
        if (vap->iv_roam.iv_ft_roam) {
            vap->iv_roam.iv_ft_roam = 0;
            vap->iv_roam.iv_wait_for_ftie_update = 0;
            if (vap->iv_roam.iv_ft_ie.ie) {
                qdf_mem_free(vap->iv_roam.iv_ft_ie.ie);
                vap->iv_roam.iv_ft_ie.ie = NULL;
                vap->iv_roam.iv_ft_ie.length = 0;
            }
            if (vap->iv_roam.iv_ft_params.fties) {
                qdf_mem_free(vap->iv_roam.iv_ft_params.fties);
                vap->iv_roam.iv_ft_params.fties = NULL;
                vap->iv_roam.iv_ft_params.fties_len = 0;
                qdf_mem_zero(vap->iv_roam.iv_ft_params.target_ap,
                    IEEE80211_ADDR_LEN);
            }
        }
    }
}

int wlan_cfg80211_reset_channel_survey_stats(struct ieee80211com *ic)
{

    qdf_spin_lock_bh(&ic->ic_channel_stats.lock);
    qdf_mem_zero(&ic->ic_channel_stats.home_chan_stats,
            sizeof(ic->ic_channel_stats.home_chan_stats));
    qdf_mem_zero(&ic->ic_channel_stats.scan_chan_stats[0],
            sizeof(ic->ic_channel_stats.scan_chan_stats));
    qdf_spin_unlock_bh(&ic->ic_channel_stats.lock);

    return 0;
}

int wlan_cfg80211_get_channel_survey_stats(struct ieee80211com *ic)
{
    struct sk_buff *reply_skb = NULL;
    QDF_STATUS status;
    size_t msglen = 0;
    struct channel_stats *chan_stats = NULL;

    msglen = sizeof(ic->ic_channel_stats.home_chan_stats) +
             sizeof(ic->ic_channel_stats.scan_chan_stats);
    reply_skb = cfg80211_vendor_cmd_alloc_reply_skb(ic->ic_wiphy,
            (msglen + (2 * sizeof(uint32_t)) + NLMSG_HDRLEN));

    if (!reply_skb) {
        return -ENOMEM;
    }

    /* Put payload len in bytes */
    nla_put_u32(reply_skb, QCA_WLAN_VENDOR_ATTR_PARAM_LENGTH, msglen);
    /* Put number of stats structs in flags */
    nla_put_u32(reply_skb, QCA_WLAN_VENDOR_ATTR_PARAM_FLAGS, msglen);

    qdf_spin_lock_bh(&ic->ic_channel_stats.lock);
    /* Add home channel stats */
    if (nla_put(reply_skb, QCA_WLAN_VENDOR_ATTR_PARAM_DATA,
                sizeof(ic->ic_channel_stats.home_chan_stats),
                &ic->ic_channel_stats.home_chan_stats)) {
        goto err;
    }

    /* Add scan channel stats */
    chan_stats = &ic->ic_channel_stats.scan_chan_stats[0];
    if (nla_put_nohdr(reply_skb, sizeof(ic->ic_channel_stats.scan_chan_stats),
                &ic->ic_channel_stats.scan_chan_stats[0])) {
        goto err;
    }
    qdf_spin_unlock_bh(&ic->ic_channel_stats.lock);

    status = qal_devcfg_send_response((qdf_nbuf_t)reply_skb);
    return qdf_status_to_os_return(status);

err:
    qdf_spin_unlock_bh(&ic->ic_channel_stats.lock);
    kfree_skb(reply_skb);
    return -EIO;
}

void wlan_cfg80211_flush_rate_stats(void *ctrl_pdev, enum WDI_EVENT event,
                                    void *ptr, uint16_t len,
                                    uint32_t type)
{
    struct wlan_objmgr_pdev *pdev;
    struct wlan_peer_rate_stats_intf *buf;
    uint32_t max_buf_len;
    struct ol_ath_softc_net80211 *scn = NULL;
    struct pdev_osif_priv *osif_priv = NULL;
    struct ieee80211com * ic = NULL;
    struct sk_buff *vendor_event;
    struct nlattr *nla;
    const uint32_t NUM_TLVS = 10;
    struct net_device *dev;
    int ret_val;

   pdev = (struct wlan_objmgr_pdev *)ctrl_pdev;
   buf = (struct wlan_peer_rate_stats_intf *)ptr;
   max_buf_len = max(WLANSTATS_CACHE_SIZE * sizeof(struct wlan_tx_rate_stats),
                     WLANSTATS_CACHE_SIZE * sizeof(struct wlan_rx_rate_stats));

    if (buf->buf_len > max_buf_len) {
        qdf_err("recevied buf len excceds max buffer len, return");
        return;
    }

    osif_priv = wlan_pdev_get_ospriv(pdev);
    scn = (struct ol_ath_softc_net80211 *)osif_priv->legacy_osif_priv;
    ic = &scn->sc_ic;
    dev = ic->ic_netdev;
#define VENDOR_CMD_CB_ASSIGN(a, b) ((void **)a->cb)[2] = b

    vendor_event = cfg80211_vendor_event_alloc(ic->ic_wiphy, NULL,
                buf->buf_len + NLMSG_HDRLEN + NUM_TLVS*sizeof(uint32_t),
                QCA_NL80211_VENDOR_SUBCMD_PEER_STATS_CACHE_FLUSH_INDEX,
                GFP_KERNEL);


    if (!vendor_event) {
        QDF_TRACE(QDF_MODULE_ID_DEBUG, QDF_TRACE_LEVEL_ERROR,
                "cfg80211_vendor_event_alloc failed\n");
        return;
    }

    nla_nest_cancel(vendor_event, ((void **)vendor_event->cb)[2]);

    /* Send the IF INDEX to differentiate the ACS event for each interface
     * Needs to be updated once cfg80211 APIs are updated to accept if_index
     */
    ret_val = nla_put_u32(vendor_event,
            NL80211_ATTR_IFINDEX,
            dev->ifindex);
    if (ret_val) {
        qdf_warn("NL80211_ATTR_IFINDEX put fail");
        goto error_cleanup;
    }

    nla = nla_nest_start(vendor_event, NL80211_ATTR_VENDOR_DATA);

    if(nla == NULL){
        qdf_warn("nla_nest_start fail nla is NULL ");
        goto error_cleanup;
    }

    VENDOR_CMD_CB_ASSIGN(vendor_event, nla);

    if (nla_put_u32(vendor_event, QCA_WLAN_VENDOR_ATTR_PEER_STATS_CACHE_TYPE,
                buf->stats_type)) {
        QDF_TRACE(QDF_MODULE_ID_DEBUG, QDF_TRACE_LEVEL_ERROR,
                "%s:%d nla put fail", __func__, __LINE__);
        goto error_cleanup;
    }

    if (nla_put(vendor_event, QCA_WLAN_VENDOR_ATTR_PEER_STATS_CACHE_PEER_MAC,
                6, (void *)buf->peer_mac)) {
        QDF_TRACE(QDF_MODULE_ID_DEBUG, QDF_TRACE_LEVEL_DEBUG,
                "%s:%d nla put fail", __func__, __LINE__);
        goto error_cleanup;
    }

    if (nla_put(vendor_event, QCA_WLAN_VENDOR_ATTR_PEER_STATS_CACHE_DATA,
                buf->buf_len, buf->stats)) {
        QDF_TRACE(QDF_MODULE_ID_DEBUG, QDF_TRACE_LEVEL_DEBUG,
                "%s:%d nla put fail", __func__, __LINE__);
        goto error_cleanup;
    }

    if (nla_put_u64(vendor_event, QCA_WLAN_VENDOR_ATTR_PEER_STATS_CACHE_PEER_COOKIE,
                buf->cookie)) {
        QDF_TRACE(QDF_MODULE_ID_DEBUG, QDF_TRACE_LEVEL_ERROR,
                "%s:%d nla put fail", __func__, __LINE__);
        goto error_cleanup;
    }

    nla_nest_end(vendor_event, nla);

    cfg80211_vendor_event(vendor_event, GFP_KERNEL);
    return;

error_cleanup:
    kfree_skb(vendor_event);
    return;
}
qdf_export_symbol(wlan_cfg80211_flush_rate_stats);
#endif
