/*
 * Copyright (c) 2013-2019 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2013-2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.

 * Copyright (c) 2010, Atheros Communications Inc.
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

#include <osdep.h>
#include <wbuf.h>

#include "if_media.h"
#include "ieee80211_var.h"
//#include "ol_if_thermal.h"
#include "ext_ioctl_drv_if.h"
#include "if_athvar.h"
#include "ath_cwm.h"
#include "osif_private.h"
#include "if_athproto.h"

#ifdef ATH_PARAMETER_API
#include "ath_papi.h"
#endif

#ifdef ATH_TX99_DIAG
#include "ath_tx99.h"
#endif

#include "ath_netlink.h"

#include "ald_netlink.h"

#if ATH_SUPPORT_FLOWMAC_MODULE
#include <flowmac_api.h>
#endif

#if UMAC_SUPPORT_ACFG
#include <ieee80211_ioctl_acfg.h>
#include <acfg_api_types.h>
#include <acfg_drv_event.h>
#endif

#include <ieee80211_objmgr_priv.h>
#include <wlan_utility.h>
#include "ath_ucfg.h"
#include <wlan_lmac_if_def.h>
#include <wlan_lmac_if_api.h>
#include <dispatcher_init_deinit.h>
#include <wlan_global_lmac_if_api.h>
#include <wlan_osif_priv.h>
#include <wlan_mgmt_txrx_utils_api.h>
#include <wlan_reg_ucfg_api.h>
#ifndef REMOVE_PKT_LOG
#include <ah_pktlog.h>
#endif
#if WLAN_SPECTRAL_ENABLE
#include <wlan_spectral_utils_api.h>
#include <wlan_spectral_public_structs.h>
#endif
#include <wlan_mlme_dispatcher.h>

#ifdef QCA_SUPPORT_CP_STATS
#include <wlan_cfg80211_ic_cp_stats.h>
#include <wlan_cp_stats_ic_utils_api.h>
#endif

#include <da_aponly.h>
#include <da_dp_public.h>
#include <wlan_utility.h>

/*
 * Maximum acceptable MTU
 * MAXFRAMEBODY - WEP - QOS - RSN/WPA:
 * 2312 - 8 - 2 - 12 = 2290
 */
#define ATH_MAX_MTU     2290
#define ATH_MIN_MTU     32
#define WIFI_DEV_NAME_SOC   "soc%d"
#define WLAN_MAX_ACTIVE_SCANS_ALLOWED   1

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,1,0)
#define IRQF_DISABLED 0x00000020
#endif

unsigned int cfg80211_config = 0 ;

#if defined(ATH_TX_BUF_FLOW_CNTL) ||  defined(ATH_DEBUG)
extern int ACBKMinfree;
extern int ACBEMinfree;
extern int ACVIMinfree;
extern int ACVOMinfree;
extern int CABMinfree;
extern int UAPSDMinfree;
#endif

extern struct ath_softc_net80211 *global_scn[10];
extern int num_global_scn;
int osif_vap_hardstart(struct sk_buff *skb, struct net_device *dev);
extern void osif_hal_log_ani_callback_register(void *ani_func);

static struct ath_reg_parm ath_params = {
    .wModeSelect = MODE_SELECT_ALL,
    .NetBand = MODE_SELECT_ALL,
    .txAggrEnable = 1,
    .rxAggrEnable = 1,
    .txAmsduEnable = 1,
    .aggrLimit = IEEE80211_AMPDU_LIMIT_DEFAULT,
    .aggrSubframes = IEEE80211_AMPDU_SUBFRAME_DEFAULT,
    .aggrProtDuration = 8192,
    .aggrProtMax = 8192,
    .txRifsEnable = 0,
    .rifsAggrDiv = IEEE80211_RIFS_AGGR_DIV,
    .txChainMaskLegacy = 1,
    .rxChainMaskLegacy = 1,
    .rxChainDetectThreshA = 35,
    .rxChainDetectThreshG = 35,
    .rxChainDetectDeltaA = 30,
    .rxChainDetectDeltaG = 30,
    .calibrationTime = 30,
#if ATH_SUPPORT_LED
    .gpioPinFuncs = {GPIO_PIN_FUNC_0,GPIO_PIN_FUNC_1,GPIO_PIN_FUNC_2},
    .gpioLedCustom = ATH_LED_CUSTOMER,
#else
    .gpioPinFuncs = {1, 7, 7},
#endif
    .hwTxRetries = 4,
    .extendedChanMode = 1,
    .DmaStopWaitTime = 4,
    .swBeaconProcess = 1,
    .stbcEnable = 1,
    .ldpcEnable = 1,
    .cwmEnable = 1,
    .wpsButtonGpio = 0,
#ifdef ATH_SUPPORT_TxBF
    .TxBFSwCvTimeout = 1000 ,
#endif
#if WLAN_SPECTRAL_ENABLE
	.spectralEnable = 1,
#endif
#if ATH_SUPPORT_PAPRD
    .paprdEnable = 1,
#endif
#if ATH_TX_BUF_FLOW_CNTL
    .ACBKMinfree = 48,
    .ACBEMinfree = 32,
    .ACVIMinfree = 16,
    .ACVOMinfree = 0,
    .CABMinfree = 48,
    .UAPSDMinfree = 0,
#endif
#ifdef ATH_SWRETRY
    .numSwRetries = 2,
#endif
#if ATH_SUPPORT_FLOWMAC_MODULE
    .osnetif_flowcntrl = 0,
    .os_ethflowmac_enable = 0,
#endif
    .burstEnable = 0,
    .burstDur = 5100,
};

static struct ieee80211_reg_parameters wlan_reg_params = {
    .transmitRetrySet = 0x04040404,
    .sleepTimePwrSave = 100,         /* wake up every beacon */
    .sleepTimePwrSaveMax = 1000,     /* wake up every 10 th beacon */
    .sleepTimePerf=100,              /* station wakes after this many mS in max performance mode */
    .inactivityTimePwrSaveMax=400,   /* in max PS mode, how long (in mS) w/o Tx/Rx before going back to sleep */
    .inactivityTimePwrSave=200,      /* in normal PS mode, how long (in mS) w/o Tx/Rx before going back to sleep */
    .inactivityTimePerf=100,         /* in max perf mode, how long (in mS) w/o Tx/Rx before going back to sleep */
    .psPollEnabled=0,                /* Use PS-POLL to retrieve data frames after TIM is received */
    .wmeEnabled    = 1,
    .enable2GHzHt40Cap = 1,
    .cwmEnable = 1,
    .cwmExtBusyThreshold = IEEE80211_CWM_EXTCH_BUSY_THRESHOLD,
    .ignore11dBeacon = 1,
    .p2pGoUapsdEnable = 1,
    .extapUapsdEnable = 1,
    .shortPreamble = 1,               /*ShortPreamble is enabled default */
#ifdef ATH_SUPPORT_TxBF
    .autocvupdate = 0,
#define DEFAULT_PER_FOR_CVUPDATE 30
    .cvupdateper = DEFAULT_PER_FOR_CVUPDATE,
#endif
};

/* The code below is used to register a hw_caps file in sysfs */
static ssize_t wifi_hwcaps_show(struct device *dev,
                               struct device_attribute *attr, char *buf)
{
    struct net_device *net = to_net_dev(dev);
    struct ath_softc_net80211 *scn = ath_netdev_priv(net);
    struct ieee80211com *ic = &scn->sc_ic;

    u_int64_t hw_caps = ic->ic_modecaps;

    strlcpy(buf, "802.11", strlen("802.11") + 1);
    if(hw_caps &
        1 << IEEE80211_MODE_11A)
        strlcat(buf, "a", strlen("a") + strlen(buf) + 1);
    if(hw_caps &
        1 << IEEE80211_MODE_11B)
        strlcat(buf, "b", strlen("b") + strlen(buf) + 1);
    if(hw_caps &
        1 << IEEE80211_MODE_11G)
        strlcat(buf, "g", strlen("g") + strlen(buf) + 1);
    if(hw_caps &
        (1 << IEEE80211_MODE_11NA_HT20 |
         1 << IEEE80211_MODE_11NG_HT20 |
         1 << IEEE80211_MODE_11NA_HT40PLUS |
         1 << IEEE80211_MODE_11NA_HT40MINUS |
         1 << IEEE80211_MODE_11NG_HT40PLUS |
         1 << IEEE80211_MODE_11NG_HT40MINUS |
         1 << IEEE80211_MODE_11NG_HT40 |
         1 << IEEE80211_MODE_11NA_HT40))
        strlcat(buf, "n", strlen("n") + strlen(buf) + 1);
    if(hw_caps &
        (1 << IEEE80211_MODE_11AC_VHT20 |
         1 << IEEE80211_MODE_11AC_VHT40PLUS |
         1 << IEEE80211_MODE_11AC_VHT40MINUS |
         1 << IEEE80211_MODE_11AC_VHT40 |
         1 << IEEE80211_MODE_11AC_VHT80 |
         1 << IEEE80211_MODE_11AC_VHT160 |
         1 << IEEE80211_MODE_11AC_VHT80_80))
        strlcat(buf, "/ac", strlen("/ac") + strlen(buf) + 1);
    if(hw_caps &
        (1 << IEEE80211_MODE_11AXA_HE20 |
         1 << IEEE80211_MODE_11AXG_HE20 |
         1 << IEEE80211_MODE_11AXA_HE40PLUS |
         1 << IEEE80211_MODE_11AXA_HE40MINUS |
         1 << IEEE80211_MODE_11AXG_HE40PLUS |
         1 << IEEE80211_MODE_11AXG_HE40MINUS |
         1 << IEEE80211_MODE_11AXA_HE40 |
         1 << IEEE80211_MODE_11AXG_HE40 |
         1 << IEEE80211_MODE_11AXA_HE80 |
         1 << IEEE80211_MODE_11AXA_HE160 |
         1ULL << IEEE80211_MODE_11AXA_HE80_80))
        strlcat(buf, "/ax", strlen("/ax") + strlen(buf) + 1);
    return strlen(buf);
}
static DEVICE_ATTR(hwcaps, S_IRUGO, wifi_hwcaps_show, NULL);

/* Handler for sysfs entry hwmodes - returns all the hwmodes supported by the radio */
static ssize_t wifi_hwmodes_show(struct device *dev,
                               struct device_attribute *attr, char *buf)
{
    struct ath_softc_net80211 *scn = NULL;
    struct ieee80211com *ic = NULL;
    u_int64_t hw_caps = 0;
    struct net_device *net = to_net_dev(dev);

    scn = ath_netdev_priv(net);
    if(!scn){
       return 0;
    }

    ic = &scn->sc_ic;
    if(!ic){
        return 0;
    }

    hw_caps = ic->ic_modecaps;

    if(hw_caps &
        1 << IEEE80211_MODE_11A)
        strlcat(buf, "11A ", strlen("11A") + strlen(buf) + 1);
    if(hw_caps &
        1 << IEEE80211_MODE_11B)
        strlcat(buf, "11B ", strlen("11B") + strlen(buf) + 1);
    if(hw_caps &
        1 << IEEE80211_MODE_11G)
        strlcat(buf, "11G ", strlen("11G") + strlen(buf) + 1);
    if(hw_caps &
        1 << IEEE80211_MODE_FH)
        strlcat(buf, "FH ", strlen("FH") + strlen(buf) + 1);
    if(hw_caps &
        1 << IEEE80211_MODE_TURBO_A)
        strlcat(buf, "TURBO_A ", strlen("TURBO_A") + strlen(buf) + 1);
    if(hw_caps &
        1 << IEEE80211_MODE_TURBO_G)
        strlcat(buf, "TURBO_G ", strlen("TURBO_G") + strlen(buf) + 1);
    if(hw_caps &
        1 << IEEE80211_MODE_11NA_HT20)
        strlcat(buf, "11NA_HT20 ", strlen("11NA_HT20") + strlen(buf) + 1);
    if(hw_caps &
        1 << IEEE80211_MODE_11NG_HT20)
        strlcat(buf, "11NG_HT20 ", strlen("11NG_HT20") + strlen(buf) + 1);
    if(hw_caps &
        1 << IEEE80211_MODE_11NA_HT40PLUS)
        strlcat(buf, "11NA_HT40PLUS ", strlen("11NA_HT40PLUS") + strlen(buf) + 1);
    if(hw_caps &
        1 << IEEE80211_MODE_11NA_HT40MINUS)
        strlcat(buf, "11NA_HT40MINUS ", strlen("11NA_HT40MINUS") + strlen(buf) + 1);
    if(hw_caps &
        1 << IEEE80211_MODE_11NG_HT40PLUS)
        strlcat(buf, "11NG_HT40PLUS ", strlen("11NG_HT40PLUS") + strlen(buf) + 1);
    if(hw_caps &
        1 << IEEE80211_MODE_11NG_HT40MINUS)
        strlcat(buf, "11NG_HT40MINUS ", strlen("11NG_HT40MINUS") + strlen(buf) + 1);
    if(hw_caps &
        1 << IEEE80211_MODE_11NG_HT40)
        strlcat(buf, "11NG_HT40 ", strlen("11NG_HT40") + strlen(buf) + 1);
    if(hw_caps &
        1 << IEEE80211_MODE_11NA_HT40)
        strlcat(buf, "11NA_HT40 ", strlen("11NA_HT40") + strlen(buf) + 1);

    return strlen(buf);
}
static DEVICE_ATTR(hwmodes, S_IRUGO, wifi_hwmodes_show, NULL);

/*Handler for sysfs entry 2g_maxchwidth - returns the maximum channel width supported by the device in 2.4GHz*/
static ssize_t wifi_2g_maxchwidth_show(struct device *dev,
                               struct device_attribute *attr, char *buf)
{
    struct ath_softc_net80211 *scn = NULL;
    struct ieee80211com *ic = NULL;
    u_int64_t hw_caps = 0;
    struct net_device *net = to_net_dev(dev);

    scn = ath_netdev_priv(net);
    if(!scn){
       return 0;
    }

    ic = &scn->sc_ic;
    if(!ic){
        return 0;
    }

    hw_caps = ic->ic_modecaps;

    if (hw_caps &
        (1 << IEEE80211_MODE_11NG_HT40MINUS |
         1 << IEEE80211_MODE_11NG_HT40PLUS |
	 1 << IEEE80211_MODE_11NG_HT40)){
        strlcpy(buf, "40", strlen("40") + 1);
    }
    else if (hw_caps & (1 << IEEE80211_MODE_11NG_HT20 )){
        strlcpy(buf, "20", strlen("20") + 1);
    }

    /* NOTE: Only >=n chipsets are considered for now since productization will
     * involve only such chipsets. In the unlikely case where lower chipsets/crimped
     * phymodes are to be handled, it is a separate TODO and relevant enums need
     * to be considered.*/

    return strlen(buf);
}

static DEVICE_ATTR(2g_maxchwidth, S_IRUGO, wifi_2g_maxchwidth_show, NULL);

/*Handler for sysfs entry 5g_maxchwidth - returns the maximum channel width supported by the device in 5GHz*/
static ssize_t wifi_5g_maxchwidth_show(struct device *dev,
                               struct device_attribute *attr, char *buf)
{
    struct ath_softc_net80211 *scn = NULL;
    struct ieee80211com *ic = NULL;
    u_int64_t hw_caps = 0;
    struct net_device *net = to_net_dev(dev);

    scn = ath_netdev_priv(net);
    if(!scn){
       return 0;
    }

    ic = &scn->sc_ic;
    if(!ic){
        return 0;
    }

    hw_caps = ic->ic_modecaps;

    if (hw_caps &
        (1 <<  IEEE80211_MODE_11NA_HT40 |
         1 <<  IEEE80211_MODE_11NA_HT40MINUS |
         1 <<  IEEE80211_MODE_11NA_HT40PLUS)){
	strlcpy(buf, "40", strlen("40") + 1);
    }
    else if (hw_caps &
        (1 << IEEE80211_MODE_11NA_HT20)){
        strlcpy(buf, "20", strlen("20") + 1);
    }

    /* NOTE: Only >=n chipsets are considered for now since productization will
     * involve only such chipsets. In the unlikely case where lower chipsets/crimped
     * phymodes are to be handled, it is a separate TODO and relevant enums need
     * to be considered.*/

    return strlen(buf);
}

static DEVICE_ATTR(5g_maxchwidth, S_IRUGO, wifi_5g_maxchwidth_show, NULL);

/*Handler for sysfs entry cfg80211 xmlfile - returns the maximum channel width supported by the device in 5GHz*/
static ssize_t wifi_cfg80211_xmlfile_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    strlcpy(buf, "qcacommands_da_radio.xml", strlen("qcacommands_da_radio.xml") + 1);
    return strlen(buf);
}

static DEVICE_ATTR(cfg80211_xmlfile, S_IRUGO, wifi_cfg80211_xmlfile_show, NULL);

static struct attribute *wifi_device_attrs[] = {
    &dev_attr_hwcaps.attr,
    &dev_attr_hwmodes.attr,       /*sysfs entry for displaying all the hwmodes supported by the radio*/
    &dev_attr_5g_maxchwidth.attr, /*sysfs entry for displaying the max channel width supported in 5Ghz*/
    &dev_attr_2g_maxchwidth.attr, /*sysfs entry for displaying the max channel width supported in 2.4Ghz*/
    &dev_attr_cfg80211_xmlfile.attr,
    NULL
};

static struct attribute_group wifi_attr_group = {
       .attrs  = wifi_device_attrs,
};

/*
** Prototype for iw attach
*/

#ifdef ATH_SUPPORT_LINUX_STA
#ifdef CONFIG_SYSCTL
void ath_dynamic_sysctl_register(struct ath_softc *sc);
void ath_dynamic_sysctl_unregister(struct ath_softc *sc);
#endif
#endif
#if OS_SUPPORT_ASYNC_Q
static void os_async_mesg_handler( void  *ctx, u_int16_t  mesg_type, u_int16_t  mesg_len, void  *mesg );
#endif

#if UMAC_SUPPORT_WEXT
void ath_iw_attach(struct net_device *dev);
#endif
#if !NO_SIMPLE_CONFIG
extern int32_t unregister_simple_config_callback(char *name);
extern int32_t register_simple_config_callback (char *name, void *callback, void *arg1, void *arg2);
static irqreturn_t jumpstart_intr(int cpl, void *dev_id, struct pt_regs *regs, void *push_dur);
#endif

#ifdef QCA_PARTNER_PLATFORM
extern 	int osif_pltfrm_interupt_register(struct net_device *dev,int type, void * irs_handler, void * msi_ce_handler);
extern int osif_pltfrm_interupt_unregister(struct net_device *dev,int level);
extern void ath_pltfrm_init( struct net_device *dev );
#endif

#ifdef ATH_TX99_DIAG
extern u_int8_t tx99_ioctl(ath_dev_t dev, struct ath_softc *sc, int cmd, void *addr);
#endif


int
ath_get_netif_settings(ieee80211_handle_t ieee)
{
    struct ieee80211com *ic = NET80211_HANDLE(ieee);
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    osdev_t osdev = scn->sc_osdev;
    struct net_device *dev =  osdev->netdev;
    int flags = 0;

    if (dev->flags & IFF_RUNNING)
        flags |= ATH_NETIF_RUNNING;
    if (dev->flags & IFF_PROMISC)
        flags |= ATH_NETIF_PROMISCUOUS;
    if (dev->flags & IFF_ALLMULTI)
        flags |= ATH_NETIF_ALLMULTI;

    return flags;
}

static int
ath_vap_delete_on_rmmod(struct ath_softc_net80211 *scn)
{
    struct ieee80211vap *vap = NULL, *tvap = NULL;
    struct ieee80211com *ic = &scn->sc_ic;
    int delete_in_progress = 0;
#define  WAIT_VAP_IDLE_INTERVALL 10
    int gracetime = WAIT_VAP_IDLE_INTERVALL;

    do {
        IEEE80211_COMM_LOCK(ic);
        if (!TAILQ_EMPTY(&ic->ic_vaps)) {
            TAILQ_FOREACH_SAFE(vap, &ic->ic_vaps, iv_next, tvap) {
                if (((osif_dev *)((os_if_t)vap->iv_ifp))->is_delete_in_progress) {
                    delete_in_progress = 1;
                }
            }
        }
        IEEE80211_COMM_UNLOCK(ic);

        if (delete_in_progress) {
            msleep(1000);
            if (--gracetime == 0) {
                printk(KERN_ERR "timeout waiting for vap %s\n",
                       ((osif_dev *)((os_if_t)vap->iv_ifp))->netdev->name);
                return -1;
            }
        } else {
            break;
        }
    } while(1);
    /*
     *  If packets are heldup in the lmac queue, free the node ref count.
     */
    ath_draintxq(scn->sc_dev,0,0);

    /*
     * Don't add any more VAPs after this.
     * Else probably the detach should be done with rtnl_lock() held.
     */
    scn->sc_in_delete = 1;

    /* Bring down and delete vaps */
    if (!TAILQ_EMPTY(&ic->ic_vaps)) {
        TAILQ_FOREACH_SAFE(vap, &ic->ic_vaps, iv_next, tvap) {
            printk(KERN_ERR "%s: vap %s still registered, cleaning up\n",
                   __func__, ((osif_dev *)((os_if_t)vap->iv_ifp))->netdev->name);
            dev_close(((osif_dev *)((os_if_t)vap->iv_ifp))->netdev);
            osif_ioctl_delete_vap(((osif_dev *)((os_if_t)vap->iv_ifp))->netdev);
        }
    }
    return 0;
}

/*
 * Merge multicast addresses from all vap's to form the
 * hardware filter.  Ideally we should only inspect our
 * own list and the 802.11 layer would merge for us but
 * that's a bit difficult so for now we put the onus on
 * the driver.
 */
void
ath_mcast_merge(ieee80211_handle_t ieee, u_int32_t mfilt[2])
{
    struct ieee80211com *ic = NET80211_HANDLE(ieee);
    struct ieee80211vap *vap;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 34)
    struct netdev_hw_addr *ha;
#else
    struct dev_mc_list *mc;
#endif
    u_int32_t val;
    u_int8_t pos;

    mfilt[0] = mfilt[1] = 0;
    /* XXX locking */
    TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
        struct net_device *dev;
        os_if_t osif = vap->iv_ifp;
        if (osif == NULL)
            continue;

        dev = ((osif_dev *)osif)->netdev;
        if (dev == NULL)
            continue;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 34)
        netdev_for_each_mc_addr(ha, dev) {
            /* calculate XOR of eight 6-bit values */
            val = LE_READ_4(ha->addr + 0);
            pos = (val >> 18) ^ (val >> 12) ^ (val >> 6) ^ val;
            val = LE_READ_4(ha->addr + 3);
            pos ^= (val >> 18) ^ (val >> 12) ^ (val >> 6) ^ val;
            pos &= 0x3f;
            mfilt[pos / 32] |= (1 << (pos % 32));
        }
#else
        for (mc = dev->mc_list; mc; mc = mc->next) {
            /* calculate XOR of eight 6bit values */
            val = LE_READ_4(mc->dmi_addr + 0);
            pos = (val >> 18) ^ (val >> 12) ^ (val >> 6) ^ val;
            val = LE_READ_4(mc->dmi_addr + 3);
            pos ^= (val >> 18) ^ (val >> 12) ^ (val >> 6) ^ val;
            pos &= 0x3f;
            mfilt[pos / 32] |= (1 << (pos % 32));
        }
#endif
    }
}

#define VDEV_UP_STATE  1
#define VDEV_DOWN_STATE 0
#define MAX_ID_PER_WORD 32

static void ath_vdev_netdev_state_change(struct wlan_objmgr_pdev *pdev, void *obj, void *args)
{
    osif_dev  *osifp;
    struct net_device *netdev;
    u_int8_t myaddr[IEEE80211_ADDR_LEN];
    u_int8_t id = 0;
    u_int8_t vdev_id = 0;
    u_int8_t ix = 0;
    struct ieee80211com *ic;
    struct vdev_osif_priv *vdev_osifp = NULL;
    uint8_t flags = *((uint8_t *)args);
    struct wlan_objmgr_vdev *vdev = (struct wlan_objmgr_vdev *)obj;

    /* This API is invoked for all vdevs of the radio */
    if(vdev != NULL) {
        ic = wlan_vdev_get_ic(vdev);
        vdev_osifp = wlan_vdev_get_ospriv(vdev);
        if (!vdev_osifp) {
            return;
        }
        osifp = (osif_dev *)vdev_osifp->legacy_osif_priv;
        if (osifp == NULL) {
            return;
        }
        netdev = osifp->netdev;

        WLAN_ADDR_COPY(myaddr, wlan_vdev_mlme_get_macaddr(vdev));
        vdev_id = wlan_vdev_get_id(vdev);
        id = vdev_id;
        if(vdev_id >= MAX_ID_PER_WORD) {
            id -= 32;
            ix = 1;
        }
         /* open the vdev's netdev */
        if(flags == VDEV_UP_STATE) {
           /* Brings up the vdev */
           if( ic->id_mask_vap_downed[ix] & ( 1 << id ) ) {
               dev_change_flags(netdev, netdev->flags | ( IFF_UP ));
               ic->id_mask_vap_downed[ix] &= (~( 1 << id ));
           }
        } /* stop the vdev's netdev */
        else if(flags == VDEV_DOWN_STATE) {
           /* Brings down the vdev */
           if (IS_IFUP(netdev)) {
               dev_change_flags(netdev,netdev->flags & ( ~IFF_UP ));
               ic->id_mask_vap_downed[ix] |= ( 1 << id);
           }
        }
   }
}



static int
ath_netdev_open(struct net_device *dev)
{
    struct ath_softc_net80211 *scn = ath_netdev_priv(dev);
    int ath_ret;
    short flags = VDEV_UP_STATE; /* flag to indicate bring up */

#ifdef ATH_BUS_PM
    if (scn->sc_osdev->isDeviceAsleep)
        return -EPERM;
#endif /* ATH_BUS_PM */

    ath_ret = ath_resume(scn);
    if(ath_ret == 0){
        dev->flags |= IFF_UP | IFF_RUNNING;      /* we are ready to go */
        /*  If physical radio interface wifiX is shutdown,all virtual interfaces(athX) should gets shutdown and
            all these downed virtual interfaces should gets up when physical radio interface(wifiX) is up.Refer EV 116786.
         */
        if (wlan_objmgr_pdev_try_get_ref(scn->sc_pdev, WLAN_MLME_NB_ID) ==
                                         QDF_STATUS_SUCCESS) {
              wlan_objmgr_pdev_iterate_obj_list(scn->sc_pdev, WLAN_VDEV_OP,
                                       ath_vdev_netdev_state_change, &flags, 1,
                                       WLAN_MLME_NB_ID);
              wlan_objmgr_pdev_release_ref(scn->sc_pdev, WLAN_MLME_NB_ID);
        }
    }
    return ath_ret;
}

static int
ath_netdev_stop(struct net_device *dev)
{
    struct ath_softc_net80211 *scn = ath_netdev_priv(dev);
    struct ieee80211com *ic = &scn->sc_ic;

    short flags = VDEV_DOWN_STATE; /* flag to indicate bring up */

    /*  If physical radio interface wifiX is shutdown,all virtual interfaces(athX) should gets shutdown and
        all these downed virtual interfaces should gets up when physical radio interface(wifiX) is up.Refer EV 116786.
     */
    if (wlan_objmgr_pdev_try_get_ref(scn->sc_pdev, WLAN_MLME_NB_ID) ==
                                     QDF_STATUS_SUCCESS) {
         wlan_objmgr_pdev_iterate_obj_list(scn->sc_pdev, WLAN_VDEV_OP,
                                           ath_vdev_netdev_state_change, &flags,
                                           1, WLAN_MLME_NB_ID);
         wlan_objmgr_pdev_release_ref(scn->sc_pdev, WLAN_MLME_NB_ID);
    }
    dev->flags &= ~IFF_RUNNING;

    ic->ic_pwrsave_set_state(ic,IEEE80211_PWRSAVE_FULL_SLEEP);
    return 0;
}


static int
ath_netdev_hardstart(struct sk_buff *skb, struct net_device *dev)
{
    if(!qdf_nbuf_get_ext_cb(skb))
        return 0;
    do_ath_netdev_hardstart(skb,dev);
}
static void
ath_netdev_tx_timeout(struct net_device *dev)
{
    struct ath_softc_net80211 *scn = ath_netdev_priv(dev);

    DPRINTF(scn, ATH_DEBUG_WATCHDOG, "%s: %sRUNNING\n",
            __func__, (dev->flags & IFF_RUNNING) ? "" : "!");

	if (dev->flags & IFF_RUNNING) {
        scn->sc_ops->reset_start(scn->sc_dev, 0, 0, 0);
        scn->sc_ops->reset(scn->sc_dev);
        scn->sc_ops->reset_end(scn->sc_dev, 0);
	}
}

static int
ath_netdev_set_macaddr(struct net_device *dev, void *addr)
{
    struct ath_softc_net80211 *scn = ath_netdev_priv(dev);
    struct ieee80211com *ic = &scn->sc_ic;
    struct sockaddr *mac = addr;

    if (netif_running(dev)) {
        DPRINTF(scn, ATH_DEBUG_ANY,
            "%s: cannot set address; device running\n", __func__);
        return -EBUSY;
    }
    DPRINTF(scn, ATH_DEBUG_ANY, "%s: %.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n",
        __func__,
        mac->sa_data[0], mac->sa_data[1], mac->sa_data[2],
        mac->sa_data[3], mac->sa_data[4], mac->sa_data[5]);

    /* XXX not right for multiple vap's */
    IEEE80211_ADDR_COPY(ic->ic_myaddr, mac->sa_data);
    IEEE80211_ADDR_COPY(ic->ic_my_hwaddr, mac->sa_data);
    /* set hw mac address for pdev */
    wlan_pdev_set_hw_macaddr(scn->sc_pdev, mac->sa_data);
    IEEE80211_ADDR_COPY(dev->dev_addr, mac->sa_data);
    scn->sc_ops->set_macaddr(scn->sc_dev, dev->dev_addr);
    return 0;
}

static void
ath_netdev_set_mcast_list(struct net_device *dev)
{
    struct ath_softc_net80211 *scn = ath_netdev_priv(dev);

    scn->sc_ops->mc_upload(scn->sc_dev);
}

static int
ath_change_mtu(struct net_device *dev, int mtu)
{
    if (!(ATH_MIN_MTU < mtu && mtu <= ATH_MAX_MTU)) {
        DPRINTF((struct ath_softc_net80211 *) ath_netdev_priv(dev),
            ATH_DEBUG_ANY, "%s: invalid %d, min %u, max %u\n",
            __func__, mtu, ATH_MIN_MTU, ATH_MAX_MTU);
        return -EINVAL;
    }
    DPRINTF((struct ath_softc_net80211 *) ath_netdev_priv(dev), ATH_DEBUG_ANY,
        "%s: %d\n", __func__, mtu);

    dev->mtu = mtu;
    return 0;
}

#ifdef ATH_USB
#include "usb_eth.h"
#else
extern int ath_ioctl_ethtool(struct ath_softc_net80211 *scn, int cmd, void *addr);
#endif

#if ATH_SUPPORT_WRAP && DBDC_REPEATER_SUPPORT
int32_t ath_ioctl_get_primary_radio(struct ath_softc_net80211 *scn, void *param)
{
    int primary_radio;
    struct ieee80211com *ic = &scn->sc_ic;

    if(ic->ic_primary_radio) {
        primary_radio = 1;
    } else {
        primary_radio = 0;
    }

    return _copy_to_user(param , &primary_radio, sizeof(primary_radio));
}

int32_t ath_ioctl_get_mpsta_mac_addr(struct ath_softc_net80211 *scn, caddr_t param)
{
    struct ieee80211com *ic = &scn->sc_ic;
    struct ieee80211vap *mpsta_vap;

    if (ic->ic_mpsta_vap == NULL)
    {
        return -EFAULT;
    }

    mpsta_vap = ic->ic_mpsta_vap;

    return _copy_to_user((caddr_t)param , mpsta_vap->iv_myaddr, IEEE80211_ADDR_LEN);
}

void ath_ioctl_disassoc_clients(struct ath_softc_net80211 *scn)
{
        struct ieee80211com *ic = &scn->sc_ic;
        struct ieee80211vap           *tmp_vap;
        struct ieee80211com           *tmp_ic;
        int i=0;

        for (i=0; i < MAX_RADIO_CNT; i++) {
                tmp_ic = ic->ic_global_list->global_ic[i];
                if (tmp_ic) {
                        TAILQ_FOREACH(tmp_vap, &tmp_ic->ic_vaps, iv_next) {
                                if ((tmp_vap->iv_opmode == IEEE80211_M_HOSTAP) && 
                                    (wlan_vdev_is_up(tmp_vap->vdev_obj) == QDF_STATUS_SUCCESS)) {
                                        wlan_iterate_station_list(tmp_vap, sta_disassoc, NULL);
                                }
                        }
                }
        }
}

int32_t ath_ioctl_get_force_client_mcast(struct ath_softc_net80211 *scn, void *param)
{
    int force_client_mcast;
    struct ieee80211com *ic = &scn->sc_ic;

    if(ic->ic_global_list->force_client_mcast_traffic) {
        force_client_mcast = 1;
    } else {
        force_client_mcast = 0;
    }

    return _copy_to_user(param , &force_client_mcast, sizeof(force_client_mcast));
}

int32_t ath_ioctl_get_max_priority_radio(struct ath_softc_net80211 *scn, void *param)
{

    char wifi_iface[IFNAMSIZ];
    struct ieee80211com *ic = &scn->sc_ic;
    struct global_ic_list   *glist = ic->ic_global_list;
    struct ieee80211vap           *max_priority_stavap_up;
    struct ieee80211com *max_priority_ic;
    struct net_device *n_dev;

    memset(&wifi_iface[0], 0, sizeof(wifi_iface));
    GLOBAL_IC_LOCK_BH(glist);
    max_priority_stavap_up = glist->max_priority_stavap_up;
    GLOBAL_IC_UNLOCK_BH(glist);
    if (max_priority_stavap_up &&
        (wlan_vdev_is_up(max_priority_stavap_up->vdev_obj) == QDF_STATUS_SUCCESS)) {
        max_priority_ic = max_priority_stavap_up->iv_ic;
        n_dev = max_priority_ic->ic_osdev->netdev;
        OS_MEMCPY(wifi_iface, n_dev->name, sizeof(wifi_iface));
        qdf_print("max priority radio:%s",wifi_iface);
    }
    return _copy_to_user(param , wifi_iface, IFNAMSIZ);
}


void ath_ioctl_iface_mgr_status(struct ath_softc_net80211 *scn, void *param)
{
    struct ieee80211com *ic = &scn->sc_ic;
    struct iface_config_t *iface_config = (struct iface_config_t *)param;
    u_int8_t iface_mgr_up = iface_config->iface_mgr_up;

    GLOBAL_IC_LOCK_BH(ic->ic_global_list);
    if ((iface_mgr_up) == 1) {
        ic->ic_global_list->iface_mgr_up = 1;
        qdf_print("setting iface_mgr_up to 1");
    } else {
        ic->ic_global_list->iface_mgr_up = 0;
        qdf_print("setting iface_mgr_up to 0");
    }
    GLOBAL_IC_UNLOCK_BH(ic->ic_global_list);
}
#endif

u_int8_t ath_ioctl_get_stavap_connection(struct ath_softc_net80211 *scn, void *param)
{
    u_int8_t stavap_up;
    struct ieee80211com *ic = &scn->sc_ic;
    struct ieee80211vap           *stavap = ic->ic_sta_vap;
    struct iface_config_t *iface_config = (struct iface_config_t *)param;

    if(!stavap || !stavap->vdev_obj)
        return -EINVAL;

    if ((wlan_vdev_is_up(stavap->vdev_obj) == QDF_STATUS_SUCCESS)) {
        stavap_up = 1;
    } else {
        stavap_up = 0;
    }

    iface_config->stavap_up = stavap_up;
    return 0;
}

#if  DBDC_REPEATER_SUPPORT
u_int16_t ath_ioctl_get_disconnection_timeout(struct ath_softc_net80211 *scn, void *param)
{
    u_int16_t timeout;
    struct ieee80211com *ic = &scn->sc_ic;
    struct iface_config_t *iface_config = (struct iface_config_t *)param;
    u_int16_t disconnect_timeout = ic->ic_global_list->disconnect_timeout;
    u_int16_t reconfiguration_timeout = ic->ic_global_list->reconfiguration_timeout;

    if (disconnect_timeout >= reconfiguration_timeout) {
        timeout = disconnect_timeout;
        qdf_print("disconnect_timeout:%d",disconnect_timeout);
    } else {
        timeout = reconfiguration_timeout;
        qdf_print("reconfiguration_timeout:%d",reconfiguration_timeout);
    }

    iface_config->timeout = timeout;
    return 0;
}

int32_t ath_ioctl_get_preferred_uplink(struct ath_softc_net80211 *scn, void *param)
{
    int preferredUplink;
    struct ieee80211com *ic = &scn->sc_ic;
    struct iface_config_t *iface_config = (struct iface_config_t *)param;

    if(ic->ic_preferredUplink) {
        preferredUplink = 1;
    } else {
        preferredUplink = 0;
    }

    iface_config->is_preferredUplink = preferredUplink;
    return 0;
}
#endif

extern unsigned long ath_ioctl_debug;           /* defined in ah_osdep.c  */

struct ioctl_name_tbl {
    unsigned int ioctl;
    char *name;
};

static struct ioctl_name_tbl ioctl_names[] =
{
    {SIOCGATHSTATS, "SIOCGATHSTATS"},
    {SIOCGATHSTATSCLR,"SIOCGATHSTATSCLR"},
    {SIOCGATHPHYSTATS, "SIOCGATHPHYSTATS"},
    {SIOCGATHPHYSTATSCUR, "SIOCGATHPHYSTATSCUR"},
    {SIOCGATHDIAG, "SIOCGATHDIAG"},
#if defined(ATH_SUPPORT_DFS) || defined(WLAN_SPECTRAL_ENABLE)
    {SIOCGATHPHYERR,"SIOCGATHPHYERR"},
#endif
    {SIOCETHTOOL,"SIOCETHTOOL"},
    {SIOC80211IFCREATE, "SIOC80211IFCREATE"},
#ifdef ATH_TX99_DIAG
    {SIOCIOCTLTX99, "SIOCIOCTLTX99"},
#endif
    {0, NULL},
};

static char *find_ath_std_ioctl_name(int param)
{
    int i = 0;
    for (i = 0; ioctl_names[i].ioctl; i++) {
        if (ioctl_names[i].ioctl == param)
            return(ioctl_names[i].name ? ioctl_names[i].name : "UNNAMED");
    }
    return("Unknown IOCTL");
}

int ath_ucfg_create_vap(struct ath_softc_net80211 *scn, struct ieee80211_clone_params *cp, char *dev_name)
{
    struct net_device *dev = scn->sc_osdev->netdev;
    struct ifreq ifr;
    int status;

    if (scn->sc_in_delete) {
        return -ENODEV;
    }

    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_data = (void *) cp;

    scn->sc_osdev->vap_hardstart = osif_vap_hardstart;
    status = osif_ioctl_create_vap(dev, &ifr, cp, scn->sc_osdev);

    /* return final device name */
    if(strlcpy(dev_name, ifr.ifr_name, IFNAMSIZ) >= IFNAMSIZ) {
        qdf_print("%s:Interface Name too long.%s",__func__,ifr.ifr_name);
        return -1;
    }

    return status;
}

int wlan_get_vap_info(struct ieee80211vap *vap,
        struct ieee80211vap_profile *vap_profile,
        void *handle);
int ath_ucfg_get_vap_info(struct ath_softc_net80211 *scn,
                                struct ieee80211_profile *profile)
{
    struct net_device *dev = scn->sc_osdev->netdev;
    struct ieee80211com *ic = &scn->sc_ic;
    struct ieee80211vap *vap = NULL;
    struct ieee80211vap_profile *vap_profile;
    wlan_chan_t chan;

    if(strlcpy(profile->radio_name, dev->name, IFNAMSIZ) >= IFNAMSIZ) {
        qdf_print("%s:Device Name too long.%s",__func__,dev->name);
        return -1;
    }
    wlan_get_device_mac_addr(ic, profile->radio_mac);
    profile->cc = (u_int16_t)wlan_get_device_param(ic,
            IEEE80211_DEVICE_COUNTRYCODE);
    chan = wlan_get_dev_current_channel(ic);
    if (chan != NULL) {
        profile->channel = chan->ic_ieee;
        profile->freq = chan->ic_freq;
    }
    TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
        vap_profile = &profile->vap_profile[profile->num_vaps];
        wlan_get_vap_info(vap, vap_profile, (void *)scn->sc_osdev);
        profile->num_vaps++;
    }
    return 0;
}

#if UMAC_SUPPORT_ACFG
extern int acfg_handle_ath_ioctl(struct net_device *dev, void *data);
#endif

int ath_extended_commands(struct net_device *dev, void *vextended_cmd)
{
    struct extended_ioctl_wrapper *extended_cmd = ( struct extended_ioctl_wrapper *)vextended_cmd;
    struct ath_softc_net80211 *scn = ath_netdev_priv(dev);
    struct ieee80211com *ic = &scn->sc_ic;
    int error = 0 ;

    if (scn->sc_in_delete) {
        qdf_mem_free(extended_cmd);
        return -ENODEV;
    }

    switch (extended_cmd->cmd) {

        case EXTENDED_SUBIOCTL_CHANNEL_SWITCH:
            error = ieee80211_extended_ioctl_chan_switch(dev, ic, (caddr_t)&(extended_cmd->ext_data.channel_switch_req));
            break;

        case EXTENDED_SUBIOCTL_CHANNEL_SCAN:
            error = ieee80211_extended_ioctl_chan_scan(dev, ic, (caddr_t)&(extended_cmd->ext_data.channel_scan_req));
            break;

        case EXTENDED_SUBIOCTL_REPEATER_MOVE:
            error = ieee80211_extended_ioctl_rep_move(dev, ic, (caddr_t)&(extended_cmd->ext_data.rep_move_req));
            break;

#if ATH_SUPPORT_WRAP && DBDC_REPEATER_SUPPORT
        case EXTENDED_SUBIOCTL_GET_PRIMARY_RADIO:
            error = ath_ioctl_get_primary_radio(scn, (caddr_t)&(extended_cmd->ext_data.qwrap_config));
            break;
        case EXTENDED_SUBIOCTL_GET_MPSTA_MAC_ADDR:
            error = ath_ioctl_get_mpsta_mac_addr(scn, (caddr_t)&(extended_cmd->ext_data.qwrap_config));
            break;
        case EXTENDED_SUBIOCTL_DISASSOC_CLIENTS:
            ath_ioctl_disassoc_clients(scn);
            break;
        case EXTENDED_SUBIOCTL_GET_FORCE_CLIENT_MCAST:
            error = ath_ioctl_get_force_client_mcast(scn, (caddr_t)&(extended_cmd->ext_data.qwrap_config));
            break;
        case EXTENDED_SUBIOCTL_GET_MAX_PRIORITY_RADIO:
            error = ath_ioctl_get_max_priority_radio(scn, (caddr_t)&(extended_cmd->ext_data.qwrap_config));
            break;
        case EXTENDED_SUBIOCTL_IFACE_MGR_STATUS:
            ath_ioctl_iface_mgr_status(scn, (caddr_t)&(extended_cmd->ext_data.iface_config));
            break;
#endif
        case EXTENDED_SUBIOCTL_GET_STAVAP_CONNECTION:
            error = ath_ioctl_get_stavap_connection(scn, (caddr_t)&(extended_cmd->ext_data.iface_config));
            break;
        case EXTENDED_SUBIOCTL_GET_DISCONNECTION_TIMEOUT:
#if DBDC_REPEATER_SUPPORT
            error = ath_ioctl_get_disconnection_timeout(scn, (caddr_t)&(extended_cmd->ext_data.iface_config));
#endif
            break;
        case EXTENDED_SUBIOCTL_GET_PREF_UPLINK:
#if DBDC_REPEATER_SUPPORT
            error = ath_ioctl_get_preferred_uplink(scn, (caddr_t)&(extended_cmd->ext_data.iface_config));
#endif
            break;
        default:
            qdf_print("%s: unsupported extended command %d", __func__, extended_cmd->cmd);
            break;
    }
    return error;

}

#ifdef QCA_SUPPORT_CP_STATS
void ath_copy_cp_stats(struct ath_stats *as, struct pdev_ic_cp_stats *pdev_cp_stats)
{
    as->ast_resetOnError = pdev_cp_stats->lmac_stats.cs_ast_reset_on_error;
    as->ast_hardware = pdev_cp_stats->lmac_stats.cs_ast_hardware;
    as->ast_halresets = pdev_cp_stats->lmac_stats.cs_ast_halresets;
    as->ast_bmiss = pdev_cp_stats->lmac_stats.cs_ast_bmiss;
    as->ast_mib = pdev_cp_stats->stats.cs_mib_int_count;
    as->ast_brssi = pdev_cp_stats->lmac_stats.cs_ast_brssi;
    as->ast_tx_mgmt = pdev_cp_stats->stats.cs_tx_mgmt;
    as->ast_tx_fifoerr = pdev_cp_stats->lmac_stats.cs_ast_tx_fifoerr;
    as->ast_tx_filtered = pdev_cp_stats->lmac_stats.cs_ast_tx_filtered;
    as->ast_tx_noack = pdev_cp_stats->lmac_stats.cs_ast_tx_noack;
    as->ast_tx_shortpre = pdev_cp_stats->lmac_stats.cs_ast_tx_shortpre;
    as->ast_tx_altrate = pdev_cp_stats->lmac_stats.cs_ast_tx_altrate;
    as->ast_tx_protect = pdev_cp_stats->lmac_stats.cs_ast_tx_protect;
    as->ast_rx_nobuf = pdev_cp_stats->lmac_stats.cs_ast_rx_nobuf;
    as->ast_rx_hal_in_progress = pdev_cp_stats->lmac_stats.cs_ast_rx_hal_in_progress;
    as->ast_rx_num_mgmt = pdev_cp_stats->stats.cs_rx_num_mgmt;
    as->ast_rx_num_ctl = pdev_cp_stats->stats.cs_rx_num_ctl;
    as->ast_rx_num_unknown = pdev_cp_stats->lmac_stats.cs_ast_rx_num_unknown;
    as->ast_be_xmit = pdev_cp_stats->stats.cs_tx_beacon;
    as->ast_be_nobuf = pdev_cp_stats->stats.cs_be_nobuf;
    as->ast_per_cal = pdev_cp_stats->lmac_stats.cs_ast_per_cal;
    as->ast_per_calfail  = pdev_cp_stats->lmac_stats.cs_ast_per_calfail;
    as->ast_per_rfgain = pdev_cp_stats->lmac_stats.cs_ast_per_rfgain;
    as->ast_ant_defswitch = pdev_cp_stats->lmac_stats.cs_ast_ant_defswitch;
    as->ast_bb_hang = pdev_cp_stats->lmac_stats.cs_ast_bb_hang;
    as->ast_mac_hang = pdev_cp_stats->lmac_stats.cs_ast_mac_hang;
#if ATH_WOW
    as->ast_wow_wakeups = pdev_cp_stats->lmac_stats.cs_ast_wow_wakeups;
    as->ast_wow_wakeupsok = pdev_cp_stats->lmac_stats.cs_ast_wow_wakeupsok;
#endif
#if ATH_SUPPORT_CFEND
    as->ast_cfend_sched = pdev_cp_stats->lmac_stats.cs_ast_cfend_sched;
    as->ast_cfend_sent = pdev_cp_stats->lmac_stats.cs_ast_cfend_sent;
#endif
#if ATH_RX_LOOPLIMIT_TIMER
    as->ast_rx_looplimit_start = pdev_cp_stats->stats.cs_rx_looplimit_start;
    as->ast_rx_looplimit_end = pdev_cp_stats->stats.cs_rx_looplimit_end;
#endif
    as->ast_chan_clr_cnt = pdev_cp_stats->stats.cs_rx_clear_count;
    as->ast_cycle_cnt = pdev_cp_stats->stats.cs_cycle_count;
    as->ast_noise_floor = pdev_cp_stats->lmac_stats.cs_ast_noise_floor;
}
#endif

static int
ath_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
    struct ath_softc_net80211 *scn = ath_netdev_priv(dev);
    struct ieee80211com *ic = &scn->sc_ic;
    enum ieee80211_phymode mode;
    struct ath_stats *as;
#ifdef QCA_SUPPORT_CP_STATS
    struct pdev_ic_cp_stats pdev_cp_stats;
#endif
    struct ath_stats_container *asc = NULL;
    struct ath_phy_stats_container *apsc = NULL;
    struct ath_phy_stats *ps = NULL;
    struct ath_chan_stats chanstats;
    struct ath_mib_mac_stats mibstats;
    struct extended_ioctl_wrapper *extended_cmd;
    WIRELESS_MODE wmode;
    int error=0;
    struct ath_diag ad;

    if (ath_ioctl_debug) {
        printk("***%s:  dev=%s  ioctl=0%04X  name=%s\n", __func__, dev->name, cmd, find_ath_std_ioctl_name(cmd));
    }

    switch (cmd) {
    case SIOCGATHEACS:
#if WLAN_SPECTRAL_ENABLE
        error = osif_ioctl_eacs(dev, ifr, scn->sc_osdev);
#endif
        break;
    case SIOCGATHSTATS:
        if(((dev->flags & IFF_UP) == 0 ) || ((atomic_read( &ATH_DEV_TO_SC(scn->sc_dev)->sc_nap_vaps_up) <= 0) \
                    && (atomic_read( &ATH_DEV_TO_SC(scn->sc_dev)->sc_nsta_vaps_up) <= 0))){
            return -ENXIO;
        }

        asc = (struct ath_stats_container *)kmalloc(sizeof(struct ath_stats_container), GFP_KERNEL);
        if(asc == NULL) {
                error = -ENOMEM;
                break;
        }
        if(copy_from_user(asc, ifr->ifr_data, sizeof(struct ath_stats_container))) {
                kfree(asc);
                return -EFAULT;
        }
        if(asc->size == 0 || asc->address == NULL) {
           error = -EFAULT;
        }else {
            as = scn->sc_ops->get_ath_stats(scn->sc_dev);
            if (ATH_DEV_TO_SC(scn->sc_dev)->sc_nvaps > 0) {
                scn->sc_ops->ath_dev_get_chan_stats(scn->sc_dev, &chanstats);
#ifdef QCA_SUPPORT_CP_STATS
                pdev_cp_stats_rx_clear_count_update(scn->sc_pdev, chanstats.chan_clr_cnt);
                pdev_cp_stats_cycle_count_update(scn->sc_pdev, chanstats.cycle_cnt);
                pdev_lmac_cp_stats_ast_noise_floor_update(scn->sc_pdev,
                                         scn->sc_ops->ath_dev_get_noisefloor(scn->sc_dev));
#else
                as->ast_chan_clr_cnt = chanstats.chan_clr_cnt;
                as->ast_cycle_cnt = chanstats.cycle_cnt;
                as->ast_noise_floor = scn->sc_ops->ath_dev_get_noisefloor(scn->sc_dev);
#endif
            } else {
#ifdef QCA_SUPPORT_CP_STATS
                pdev_cp_stats_rx_clear_count_update(scn->sc_pdev, 0);
                pdev_cp_stats_cycle_count_update(scn->sc_pdev, 0);
                pdev_lmac_cp_stats_ast_noise_floor_update(scn->sc_pdev, 0);
#else
                as->ast_chan_clr_cnt = 0;
                as->ast_cycle_cnt = 0;
                as->ast_noise_floor = 0;
#endif
            }
            scn->sc_ops->ath_update_mibMacstats(scn->sc_dev);
            scn->sc_ops->ath_get_mibMacstats(scn->sc_dev, &mibstats);
            memcpy((void *)&(as->ast_mib_stats), (void *)&mibstats,
                                     sizeof(struct ath_mib_mac_stats));
#ifdef QCA_SUPPORT_CP_STATS
            if (wlan_cfg80211_get_pdev_cp_stats(scn->sc_pdev, &pdev_cp_stats))
                return -EFAULT;

            ath_copy_cp_stats(as, &pdev_cp_stats);
#endif
            if (_copy_to_user(asc->address, as,
                         sizeof(struct ath_stats)))
               error = -EFAULT;
            else
               error = 0;
        }
        asc->offload_if = 0;
        asc->size = sizeof(struct ath_stats);
        if(copy_to_user(ifr->ifr_data, asc, sizeof(struct ath_stats_container)))
                error = -EFAULT;
        else
                error = 0;

        kfree(asc);
        break;
    case SIOCGATHSTATSCLR:
        as = scn->sc_ops->get_ath_stats(scn->sc_dev);
        memset(as, 0, sizeof(struct ath_stats));
        error = 0;
        break;
    case SIOCGATHPHYSTATS:
        if((( dev->flags & IFF_UP) == 0 ) || ((atomic_read( &ATH_DEV_TO_SC(scn->sc_dev)->sc_nap_vaps_up) <= 0) \
                    && (atomic_read( &ATH_DEV_TO_SC(scn->sc_dev)->sc_nsta_vaps_up) <= 0))){
            return -ENXIO;
        }
        ps = scn->sc_ops->get_phy_stats_allmodes(scn->sc_dev);
        if (_copy_to_user(ifr->ifr_data, ps,
                          sizeof(struct ath_phy_stats) * WIRELESS_MODE_MAX)) {
            error = -EFAULT;
        }
        else {
            error = 0;
        }
        break;
    case SIOCGATHPHYSTATSCUR:
        if(((dev->flags & IFF_UP) == 0) || ((atomic_read( &ATH_DEV_TO_SC(scn->sc_dev)->sc_nap_vaps_up) <= 0) \
                    &&  (atomic_read( &ATH_DEV_TO_SC(scn->sc_dev)->sc_nsta_vaps_up) <= 0))){
            return -ENXIO;
        }
        mode = ieee80211_get_current_phymode(ic);
        wmode = ath_ieee2wmode(mode);

        if (wmode == WIRELESS_MODE_MAX) {
            ASSERT(0);
            return 0;
        }

        apsc = (struct ath_phy_stats_container *)kmalloc(sizeof(struct ath_phy_stats_container), GFP_KERNEL);
        if(apsc == NULL) {
                error = -ENOMEM;
                break;
        }

        if(copy_from_user(apsc, ifr->ifr_data, sizeof(struct ath_phy_stats_container))) {
                kfree(apsc);
                return -EFAULT;
        }
        if (apsc->size == 0 || apsc->address == NULL) {
           error = -EFAULT;
        } else {
            ps = scn->sc_ops->get_phy_stats(scn->sc_dev, wmode);
            if (_copy_to_user(apsc->address, ps,
                          sizeof(struct ath_phy_stats))) {
                error = -EFAULT;
            }
            else {
                error = 0;
            }
        }
        kfree(apsc);
        break;
    case SIOCGATHDIAG:
        if (!capable(CAP_NET_ADMIN))
            error = -EPERM;
        else {
            if(copy_from_user(&ad, ifr->ifr_data, sizeof(ad))) {
                    return -EFAULT;
            }
            error = ath_ucfg_diag(scn, &ad);
        }
        break;
#if defined(ATH_SUPPORT_DFS) || defined(WLAN_SPECTRAL_ENABLE)
    case SIOCGATHPHYERR:
        if (!capable(CAP_NET_ADMIN)) {
            error = -EPERM;
        } else {
            error = ath_ucfg_phyerr(scn, (struct ath_diag *)ifr->ifr_data);
        }
        break;
#endif
    case SIOCETHTOOL:
        if (__xcopy_from_user(&cmd, ifr->ifr_data, sizeof(cmd)))
            error = -EFAULT;
        else
            error = ath_ioctl_ethtool(scn, cmd, ifr->ifr_data);
        break;
    case SIOC80211IFCREATE:
	{
    	struct ieee80211_clone_params cp;
            if (scn->sc_in_delete) {
                return -ENODEV;
            }
            if (__xcopy_from_user(&cp, ifr->ifr_data, sizeof(cp))) {
       		return -EFAULT;
	}
            error = ath_ucfg_create_vap(scn, &cp, ifr->ifr_name);
	}
        break;
    case SIOCGATHEXTENDED:
    {
        extended_cmd = ( struct extended_ioctl_wrapper *)qdf_mem_malloc(sizeof(struct extended_ioctl_wrapper));
        if (!extended_cmd) {
            return -ENOMEM;
        }

        if (__xcopy_from_user(extended_cmd, ifr->ifr_data, sizeof(extended_cmd))) {
            qdf_mem_free(extended_cmd);
            return (-EFAULT);
        }

        qdf_mem_free(extended_cmd);
    }
        break;

#ifdef ATH_TX99_DIAG
    case SIOCIOCTLTX99:
        error = tx99_ioctl(dev, ATH_DEV_TO_SC(scn->sc_dev), cmd, ifr->ifr_data);
        break;
#endif
#ifdef ATH_SUPPORT_LINUX_VENDOR
    case SIOCDEVVENDOR:
        error = osif_ioctl_vendor(dev, ifr, 0);
        break;
#endif
#ifdef ATH_BUS_PM
    case SIOCSATHSUSPEND:
        {
            struct ieee80211com *ic = &scn->sc_ic;
            struct ieee80211vap *tmpvap;
            int val = 0;
            if (__xcopy_from_user(&val, ifr->ifr_data, sizeof(int)))
                return -EFAULT;

            if(val) {
              /* suspend only if all vaps are down */
                TAILQ_FOREACH(tmpvap, &ic->ic_vaps, iv_next) {
                    struct net_device *tmpdev = ((osif_dev *)tmpvap->iv_ifp)->netdev;
                    if (tmpdev->flags & IFF_RUNNING)
                        return -EBUSY;
                }
                error = bus_device_suspend(scn->sc_osdev);
                if (!error)
                    scn->sc_osdev->isDeviceAsleep = val;
            }
            else {
                int isDeviceAsleepSave = scn->sc_osdev->isDeviceAsleep;
                scn->sc_osdev->isDeviceAsleep = val;
                error = bus_device_resume(scn->sc_osdev);
                if (error)
                    scn->sc_osdev->isDeviceAsleep = isDeviceAsleepSave;
            }
        }
      break;
#endif /* ATH_BUS_PM */
	case SIOCG80211PROFILE:
        {
            struct ieee80211_profile *profile;

            profile = (struct ieee80211_profile *)qdf_mem_malloc(
                            sizeof (struct ieee80211_profile));
            if (profile == NULL) {
                error = -ENOMEM;
                break;
            }
            OS_MEMSET(profile, 0, sizeof (struct ieee80211_profile));

            error = ath_ucfg_get_vap_info(scn, profile);

            error = _copy_to_user(ifr->ifr_data, profile,
                            sizeof (struct ieee80211_profile));

            qdf_mem_free(profile);
            profile = NULL;
        }
        break;
    case SIOCGSETCTLPOW:
        {
            u_int32_t i = 0;
            u_int8_t *ctl  = (u_int8_t *)OS_MALLOC(scn->sc_osdev,1024, GFP_KERNEL);

            if (ctl == NULL) {
                qdf_print("Invalid ctl pointer");
                error = -EINVAL;
                break;
            }

            if(!__xcopy_from_user(ctl, ifr->ifr_data, sizeof(ctl))) {
                qdf_print("%s:%d Failed to copy from user", __func__, __LINE__);
                error = -EFAULT;
                OS_FREE(ctl);
                break;
            }
            printk("ctl ioctl :\n");
            for (i = 0; i < 1024; i++) {
                if ((i % 16) == 15) {
                    printk("\n");
                }
                printk(" %02X", ctl[i]);
            }
            printk("\n");

            scn->sc_ops->set_ctl_pwr(scn->sc_dev, ctl + 4, *(u_int32_t *)ctl, 0);
            OS_FREE(ctl);

        }
        break;

#if UMAC_SUPPORT_ACFG
	 case ACFG_PVT_IOCTL:
        error = acfg_handle_ath_ioctl(dev, ifr->ifr_data);
        break;
#endif

    default:
        error = -EINVAL;
        break;
    }
	return error;
}

/*
 * Return netdevice statistics.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
static struct rtnl_link_stats64 *
ath_getstats(struct net_device *dev, struct rtnl_link_stats64* stats64)
#else
static struct net_device_stats *
ath_getstats(struct net_device *dev)
#endif
{
    struct ath_softc_net80211 *scn = ath_netdev_priv(dev);
    struct ath_stats *as;
    struct ath_phy_stats *ps;
    struct ath_11n_stats *ans;
    struct ath_dfs_stats *dfss;
    WIRELESS_MODE wmode;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 36)
    struct net_device_stats *stats;
#else
    struct rtnl_link_stats64 * stats;
#endif

#ifdef ATH_SUPPORT_HTC
    if ((!scn) ||
        (scn && scn->sc_htc_delete_in_progress)){
        printk ("%s : #### delete is in progress, scn %pK \n", __func__, scn);

        /* Kernel expect return net_device_stats if *dev is not NULL. */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 36)
        if (scn)
            return (&scn->sc_osdev->devstats);
#else
        if (scn){
            memcpy(stats64, &scn->sc_osdev->devstats, sizefof(struct rtnl_link_stats64));
            return stats64;
        }
#endif
        else
            return NULL;
    }
#endif


    stats = &scn->sc_osdev->devstats;
    as = scn->sc_ops->get_ath_stats(scn->sc_dev);
    ans = scn->sc_ops->get_11n_stats(scn->sc_dev);
    dfss = scn->sc_ops->get_dfs_stats(scn->sc_dev);
    /* update according to private statistics */
    stats->tx_errors = as->ast_tx_xretries
#ifdef QCA_SUPPORT_CP_STATS
             + pdev_lmac_cp_stats_ast_tx_fifoerr_get(scn->sc_pdev)
             + pdev_lmac_cp_stats_ast_tx_filtered_get(scn->sc_pdev)
#else
             + as->ast_tx_fifoerr
             + as->ast_tx_filtered
#endif
             ;
    stats->tx_dropped = as->ast_tx_nobuf
            + as->ast_tx_encap
            + as->ast_tx_nonode
            + as->ast_tx_nobufmgt;
    /* Add tx mgmt(includes beacon), tx, 11n tx */
#ifdef QCA_SUPPORT_CP_STATS
    stats->tx_packets = pdev_cp_stats_tx_mgmt_get(scn->sc_pdev)
#else
    stats->tx_packets = as->ast_tx_mgmt
#endif
            + as->ast_tx_packets
            + ans->tx_pkts;
    /* 11n rx (rx mgmt is included) */
    stats->rx_packets = ans->rx_pkts;

    /* TX and RX bytes (includes mgmt.) */
    stats->tx_bytes = as->ast_tx_bytes;
    stats->rx_bytes = as->ast_rx_bytes;

    for (wmode = 0; wmode < WIRELESS_MODE_MAX; wmode++) {
        ps = scn->sc_ops->get_phy_stats(scn->sc_dev, wmode);

        stats->rx_errors = ps->ast_rx_fifoerr;
        stats->rx_dropped = ps->ast_rx_tooshort;
        stats->rx_crc_errors = ps->ast_rx_crcerr;
    }

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 36)
    return stats;
#else
    memcpy(stats64, stats, sizeof(struct rtnl_link_stats64));
    return stats64;
#endif
}

static void
ath_tasklet(TQUEUE_ARG data)
{
    struct net_device *dev = (struct net_device *)data;
    struct ath_softc_net80211 *scn = ath_netdev_priv(dev);

    do_ath_handle_intr(scn->sc_dev);
}



irqreturn_t
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,19)
ath_isr_generic(int irq, void *dev_id)
#else
ath_isr_generic(int irq, void *dev_id, struct pt_regs *regs)
#endif
{
    struct net_device *dev = dev_id;
    struct ath_softc_net80211 *scn = ath_netdev_priv(dev);
    int sched, needmark = 0;

#if PCI_INTERRUPT_WAR_ENABLE
    scn->int_scheduled_cnt++;
#endif

    /* always acknowledge the interrupt */
    sched = scn->sc_ops->isr(scn->sc_dev);

    switch(sched)
    {
    case ATH_ISR_NOSCHED:
        return  IRQ_HANDLED;

    case ATH_ISR_NOTMINE:
        return IRQ_NONE;

    default:
        if ((dev->flags & (IFF_RUNNING|IFF_UP)) != (IFF_RUNNING|IFF_UP))
        {
            DPRINTF_INTSAFE(scn, ATH_DEBUG_INTR, "%s: flags 0x%x\n", __func__, dev->flags);
            scn->sc_ops->disable_interrupt(scn->sc_dev);     /* disable further intr's */
            return IRQ_HANDLED;
        }
    }

    /*
    ** See if the transmit queue processing needs to be scheduled
    */

    ATH_SCHEDULE_TQUEUE(&scn->sc_osdev->intr_tq, &needmark);
    if (needmark)
        mark_bh(IMMEDIATE_BH);

    return IRQ_HANDLED;
}

/*
Interrupt could missing on some 3rd party platform,
and cause stop beaconning, or loading FW fail.
This WAR would detect interrupt count periodically,
and do a chip reset when interrupt missing detected.
*/
#if PCI_INTERRUPT_WAR_ENABLE
static
OS_TIMER_FUNC(ath_int_status_poll_timer)
{
    struct net_device *dev;
    struct ath_softc_net80211 *scn;
    struct ath_softc *sc;
    OS_GET_TIMER_ARG(dev, struct net_device *);
    scn = ath_netdev_priv(dev);
    sc = ATH_DEV_TO_SC(scn->sc_dev);

    if (!sc->sc_nvaps || sc->sc_invalid || sc->sc_scanning || sc->sc_dfs_wait) {
        scn->pre_int_scheduled_cnt = 0;
        OS_SET_TIMER(&scn->int_status_poll_timer, scn->int_status_poll_interval);
        return;
    }

    /* check whether internal reset is necessary to recover from wifi interrupt stop */
    if (scn->pre_int_scheduled_cnt !=0 && (scn->pre_int_scheduled_cnt == scn->int_scheduled_cnt)) {
        ath_bstuck_tasklet(sc);
                ATH_CLEAR_HANGS(sc);
        printk("%s: issue internal reset to recover wifi interrupt stop, count=%d\n",__func__,scn->int_scheduled_cnt);
    }

    scn->pre_int_scheduled_cnt = scn->int_scheduled_cnt;

    OS_SET_TIMER(&scn->int_status_poll_timer, scn->int_status_poll_interval);
    return;
}
#endif


irqreturn_t
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,19)
ath_isr(int irq, void *dev_id)
{
    do_ath_isr(irq,dev_id);
}
#else
ath_isr(int irq, void *dev_id, struct pt_regs *regs)
{
    do_ath_isr(irq,dev_id,regs);
}
#endif


#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,30)
static const struct net_device_ops athdev_net_ops = {
    .ndo_open    = ath_netdev_open,
    .ndo_stop    = ath_netdev_stop,
    .ndo_start_xmit = ath_netdev_hardstart,
    .ndo_set_mac_address = ath_netdev_set_macaddr,
    .ndo_tx_timeout = ath_netdev_tx_timeout,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
    .ndo_get_stats64 = ath_getstats,
#else
    .ndo_get_stats = ath_getstats,
#endif
    .ndo_change_mtu = ath_change_mtu,
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,2,0)
    .ndo_set_multicast_list = ath_netdev_set_mcast_list,
#else
    .ndo_set_rx_mode = ath_netdev_set_mcast_list,
#endif
    .ndo_do_ioctl = ath_ioctl,
};
#endif

static const struct net_device_ops dummy_soc_da_dev_ops;

#if ATH_SUPPORT_FLOWMAC_MODULE
int8_t
ath_flowmac_pause_wifi (struct net_device *dev, u_int8_t pause, u_int32_t duration)
{
    return 0;
}

int8_t
ath_flowmac_ratelimit_wifi(struct net_device *dev, flow_dir_t dir,
                void *rate)
{
    return 0;
}

int
ath_flowmac_notify_wifi(struct net_device *dev, flowmac_event_t event,
                void *event_data)
{
    struct ath_softc_net80211 *scn = ath_netdev_priv(dev);
    struct ath_softc *sc = ATH_DEV_TO_SC(scn->sc_dev);

    if (event == FLOWMAC_ENABLE) {
        printk("%s Flowmac is enabled\n",__func__);
        sc->sc_osnetif_flowcntrl = 1;
    } else if (event == FLOWMAC_DISABLE)  {
        printk("%s flowmac is disabled \n", __func__);
        sc->sc_osnetif_flowcntrl = 0;
    }
    sc->sc_ieee_ops->notify_flowmac_state(sc->sc_ieee, sc->sc_osnetif_flowcntrl);
    return 0;
}

int
ath_flowmac_attach(ath_dev_t dev)
{
    flow_ops_t ops;
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    if (!dev) {
        printk("%s dev pointer is wrong \n", __func__);
        return -EINVAL;
    }
    /* regiser with the flow mac driver */
    ops.pause = ath_flowmac_pause_wifi;
    ops.rate_limit  = ath_flowmac_ratelimit_wifi;
    ops.notify_event = ath_flowmac_notify_wifi;

    sc->sc_flowmac_dev = flowmac_register(sc->sc_osdev->netdev, &ops, 0, 0, 0, 0);
    if (sc->sc_flowmac_dev) {
        printk("%s flowmac registration is successful %pK\n",
                __func__, sc->sc_flowmac_dev);
        return 0;
    }
    return -EINVAL;

}
EXPORT_SYMBOL(ath_flowmac_attach);
int
ath_flowmac_detach(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    if (!dev) {
        printk("%s dev pointer is wrong \n", __func__);
        return -EINVAL;
    }
    if (sc->sc_flowmac_dev) {
        flowmac_unregister(sc->sc_flowmac_dev);
        sc->sc_flowmac_dev = NULL;
    }
    return 0;
}
#endif

int
osif_vap_hardstart(struct sk_buff *skb, struct net_device *dev)
{
    /* unshare skb before allocating memory
     * for ext_cb*/
    skb = qdf_nbuf_unshare(skb);
    if (unlikely(skb == NULL)) {
        return -ENOMEM;
    }

    if (wbuf_alloc_mgmt_ctrl_block(skb) == NULL)
        return -ENOMEM;

    do_osif_vap_hardstart(skb,dev);
    return 0;
}

static QDF_STATUS
ath_scan_set_max_active_scans(struct wlan_objmgr_psoc *psoc,
		uint32_t max_active_scans)
{
	struct wlan_lmac_if_scan_rx_ops *scan_rx_ops;
	QDF_STATUS status;

	scan_rx_ops =  &psoc->soc_cb.rx_ops.scan;
	if (scan_rx_ops->scan_set_max_active_scans) {
		status = scan_rx_ops->scan_set_max_active_scans(psoc,
				max_active_scans);
	} else {
		qdf_print("scan_set_max_active_scans uninitialized");
		status = QDF_STATUS_E_FAULT;
	}

	return status;
}

/*
 * ATH symbols exported to the HAL.
 * Make table static so that clients can keep a pointer to it
 * if they so choose.
 */
static u_int32_t
ath_read_pci_config_space(struct ath_softc *sc, u_int32_t offset, void *buffer, u_int32_t len)
{
    return OS_PCI_READ_CONFIG(sc->sc_osdev, offset, buffer, len);
}

static const struct ath_hal_callback halCallbackTable = {
    /* Callback Functions */
    /* read_pci_config_space */ (HAL_BUS_CONFIG_READER)ath_read_pci_config_space
};
extern int asf_adf_attach(void);
static void update_psoc_reg_caps(struct ath_hal *ah, struct wlan_objmgr_psoc *psoc)
{
	 struct wlan_psoc_host_hal_reg_capabilities_ext reg_cap_ptr;
	 uint32_t low_2g, high_2g, low_5g, high_5g;

	qdf_mem_zero(&reg_cap_ptr,
			sizeof(struct wlan_psoc_host_hal_reg_capabilities_ext));
	ath_hal_get_freq_range(ah, CHANNEL_2GHZ, &low_2g, &high_2g);
	ath_hal_get_freq_range(ah, CHANNEL_5GHZ, &low_5g, &high_5g);
	reg_cap_ptr.low_2ghz_chan = low_2g;
	reg_cap_ptr.high_2ghz_chan = high_2g;
	reg_cap_ptr.low_5ghz_chan = low_5g;
	reg_cap_ptr.high_5ghz_chan = high_5g;
	reg_cap_ptr.wireless_modes = ath_hal_get_chip_mode(ah);
	ucfg_reg_set_hal_reg_cap(psoc, &reg_cap_ptr, 1);
}

static void ath_soc_netdev_setup (struct net_device *ndev)
{
}
void __soc_detach(struct ath_softc_net80211 *scn)
{
    struct net_device *temp_dev;
    struct ath_soc_softc *soc;
    soc = scn->soc;
    temp_dev = soc->netdev;

    if (temp_dev->reg_state == NETREG_REGISTERED)
        unregister_netdev(temp_dev);
    free_netdev(temp_dev);
    soc->scn = NULL;
    scn->soc = NULL;
}
int
__ath_attach(u_int16_t devid, struct net_device *dev, HAL_BUS_CONTEXT *bus_context, osdev_t osdev)
{
    struct ath_softc_net80211 *scn = ath_netdev_priv(dev);
    struct ieee80211com *ic;
    int error = 0;
    struct hal_reg_parm hal_conf_parm;
    struct wlan_objmgr_pdev *pdev;
    struct wlan_objmgr_psoc *psoc;
    struct pdev_osif_priv *osdev_priv;
    struct mgmt_txrx_mgmt_frame_cb_info rx_cb_info;
    struct ath_hal *ah = NULL;
    HAL_STATUS status;
#if WLAN_SPECTRAL_ENABLE
    struct spectral_legacy_cbacks spectral_legacy_cbacks;
#endif
    struct net_device *temp_dev;
    ath_soc_softc_t *soc;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,19,0)
    temp_dev = alloc_netdev(sizeof(struct ath_soc_softc), WIFI_DEV_NAME_SOC, 0, ath_soc_netdev_setup);
#else
    temp_dev = alloc_netdev(sizeof(struct ath_soc_softc), WIFI_DEV_NAME_SOC, ath_soc_netdev_setup);
#endif
    if (temp_dev == NULL) {
        printk(KERN_ERR "ath: Cannot allocate soc object \n");
        error = -ENOMEM;
        goto bad0;
    }
    soc = ath_netdev_priv(temp_dev);
    OS_MEMZERO(soc, sizeof(*soc));

    soc->netdev = temp_dev;
    scn->soc = soc;
    soc->scn = scn;

    rtnl_lock();
    dev_alloc_name(temp_dev, temp_dev->name);
    rtnl_unlock();
    printk("In %s[%d]temp_dev->name: %s\n",__func__,__LINE__,temp_dev->name);

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,30)
    temp_dev->netdev_ops = &dummy_soc_da_dev_ops;
#endif

    if ((error = register_netdev(temp_dev)) != 0) {
        printk(KERN_ERR "%s: unable to register device\n", temp_dev->name);
        goto bad_soc_free;
    }

    hal_conf_parm.calInFlash = bus_context->bc_info.cal_in_flash;
    printk("%s: Set global_scn[%d]\n", __func__, num_global_scn);
    global_scn[num_global_scn++] = scn;
#if ATH_DEBUG
    ath_params.ACBKMinfree = ath_params.ACBKMinfree == ACBKMinfree ? ath_params.ACBKMinfree : ACBKMinfree;
    ath_params.ACBEMinfree = ath_params.ACBEMinfree == ACBEMinfree ? ath_params.ACBEMinfree : ACBEMinfree;
    ath_params.ACVIMinfree = ath_params.ACVIMinfree == ACVIMinfree ? ath_params.ACVIMinfree : ACVIMinfree;
    ath_params.ACVOMinfree = ath_params.ACVOMinfree == ACVOMinfree ? ath_params.ACVOMinfree : ACVOMinfree;
    ath_params.CABMinfree = ath_params.CABMinfree == CABMinfree ? ath_params.CABMinfree : CABMinfree;
    ath_params.UAPSDMinfree = ath_params.UAPSDMinfree == UAPSDMinfree ? ath_params.UAPSDMinfree : UAPSDMinfree;
    printk("*** All the minfree values should be <= ATH_TXBUF-32, otherwise default value will be used instead ***\n");
    printk("ACBKMinfree = %d\n", ath_params.ACBKMinfree);
    printk("ACBEMinfree = %d\n", ath_params.ACBEMinfree);
    printk("ACVIMinfree = %d\n", ath_params.ACVIMinfree);
    printk("ACVOMinfree = %d\n", ath_params.ACVOMinfree);
    printk("CABMinfree = %d\n", ath_params.CABMinfree);
    printk("UAPSDMinfree = %d\n", ath_params.UAPSDMinfree);
    printk("ATH_TXBUF=%d\n", ATH_TXBUF);
#elif QCA_AIRTIME_FAIRNESS
    printk("ATH_TXBUF=%d\n", ATH_TXBUF);
#endif

    /*
     * create and initialize ath layer
     */
#if ATH_SUPPORT_FLOWMAC_MODULE
    ath_params.osnetif_flowcntrl = 0; /* keep it disabled by default */
    ath_params.os_ethflowmac_enable = 0; /* keep it disabled by default */
#endif

#ifdef QCA_PARTNER_PLATFORM
    ath_pltfrm_init( dev );
#endif
    /* initialize the qdf_dev handle */
    scn->qdf_dev = qdf_mem_malloc(sizeof(*(osdev->qdf_dev)));

    if (scn->qdf_dev == NULL) {
        goto bad;
    }
    osdev->qdf_dev = scn->qdf_dev;

    memset(scn->qdf_dev, 0, sizeof(*(scn->qdf_dev)));
    scn->qdf_dev->drv = osdev;
    scn->qdf_dev->drv_hdl = osdev->bdev; /* bus handle */
    scn->qdf_dev->dev = osdev->device; /* device */


    /*
     * 1. Resolving name to avoid a crash in request_irq() on new kernels
     * 2. Getting device name early to save it to bus context for Caldata in file solution
     */
    dev_alloc_name(dev, dev->name);

    bus_context->bc_info.bc_tag = (char *)(dev->name);
	/*
	 * Set bus context to osdev
	 */
	osdev->bc = *bus_context;

    /* UMAC Dispatcher PSOC creater attach*/
    psoc = wlan_objmgr_psoc_obj_create(devid, WLAN_DEV_DA);
    if (psoc == NULL) {
         qdf_print(KERN_ERR "ath: psoc alloc failed");
         error = -ENOMEM;
         goto bad1;
    }
    wlan_psoc_set_qdf_dev(psoc, scn->qdf_dev);

    /* lmac_if function pointer registration */
    if (wlan_global_lmac_if_open(psoc)) {
        qdf_print("LMAC_IF open failed ");
        goto bad_objmgr1;
    }
    scn->rx_ops = &psoc->soc_cb.rx_ops;
	if(ath_scan_set_max_active_scans(psoc, WLAN_MAX_ACTIVE_SCANS_ALLOWED))
	{
        qdf_print("Serialization: set max active scans failed ");
        goto bad_objmgr1;
	}
    /*UMAC Disatcher Open for all components under PSOC */
    if (dispatcher_psoc_open(psoc)) {
        qdf_print("Dispatcher open failed ");
        goto bad_objmgr2;
    }

    osdev_priv = qdf_mem_malloc(sizeof(struct pdev_osif_priv));
    if (osdev_priv == NULL) {
        qdf_print("OSIF private member allocation failed ");
        goto bad_objmgr3;
    }

    osdev_priv->legacy_osif_priv = (void *)scn;

    asf_adf_attach();

#if  WLAN_SPECTRAL_ENABLE
    spectral_legacy_cbacks.vdev_get_chan_freq = wlan_vdev_get_chan_freq;
    spectral_legacy_cbacks.vdev_get_ch_width = wlan_vdev_get_ch_width;
    spectral_legacy_cbacks.vdev_get_sec20chan_freq_mhz =
        wlan_vdev_get_sec20chan_freq_mhz;
    spectral_register_legacy_cb(psoc, &spectral_legacy_cbacks);
#endif

    ah = _ath_hal_attach(devid, osdev, NULL/*sc == NULL*/, bus_context, &hal_conf_parm,
                         &halCallbackTable, &status);
    if (ah == NULL) {
        printk("%s: unable to attach hardware; HAL status %u\n",
               __func__, status);
        error = -ENXIO;
        goto bad_objmgr3;
    }
	update_psoc_reg_caps(ah, psoc);
    pdev =  wlan_objmgr_pdev_obj_create(psoc, osdev_priv);
    if (pdev == NULL) {
        qdf_print("PDEV (creation) failed ");
        goto bad_objmgr4;
    }

    if (dispatcher_pdev_open(pdev)) {
        qdf_print("%s() : Dispatcher pdev open failed ", __func__);
        goto bad_objmgr4;
    }

    scn->sc_pdev = pdev;
    ic = wlan_pdev_get_mlme_ext_obj(pdev);

    if (ic == NULL) {
        qdf_print("%s:ic is NULL",__func__);
        goto bad_objmgr4;
    }
#if UMAC_SUPPORT_CFG80211
    ic->ic_cfg80211_config = cfg80211_config;
#endif
    ic->ic_netdev = dev;

    dadp_pdev_attach(pdev, scn, osdev);

    error = ath_attach(devid, bus_context, scn, osdev, &ath_params, &hal_conf_parm, &wlan_reg_params, ah);

    if (error != 0)
        goto bad_objmgr4;

#if ATH_SUPPORT_WRAP
    wlan_pdev_set_max_vdev_count(pdev, ATH_VAPSIZE);
#else
    wlan_pdev_set_max_vdev_count(pdev, ATH_BCBUF);
#endif

#ifndef REMOVE_PKT_LOG
    if(enable_pktlog_support) {
        osif_hal_log_ani_callback_register((void*)ath_hal_log_ani_callback_register);
        scn->pl_dev->name = dev->name;
    }
#endif

#if ATH_PARAMETER_API
    if(EOK != ath_papi_netlink_init()) {
        QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO, "Parameter API socket init failed __investigate__\n");
    }
#endif

    ath_adhoc_netlink_init();

#if ATH_RXBUF_RECYCLE
    ath_rxbuf_recycle_init(osdev);
#endif

    ald_init_netlink();

#if UMAC_SUPPORT_ACFG
    acfg_event_netlink_init();
#endif

    osif_attach(dev);
    /* For STA Mode default CWM mode is Auto */
    if ( ic->ic_opmode == IEEE80211_M_STA)
        ic->ic_cwm_set_mode(ic, IEEE80211_CWM_MODE2040);

#if 0 /* TBD: is it taken care in UMAC ? */
    /*
     * This will be used in linux client while processing
     * country ie in 11d beacons
     */
    ic->ic_ignore_11dbeacon = 0;

    /*
     * commommode is used while determining the channel power
     * in standard client mode
     */
    ic->ic_commonmode = ic->ic_country.isMultidomain;

#endif

    /*
     * initialize tx/rx engine
     */
    error = scn->sc_ops->tx_init(scn->sc_dev, ATH_TXBUF);
    if (error != 0)
        goto bad2;

    error = scn->sc_ops->rx_init(scn->sc_dev, ATH_RXBUF);
    if (error != 0)
        goto bad3;

    /*
     * setup net device
     */
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,30)
    dev->netdev_ops = &athdev_net_ops;
#else
    dev->open = ath_netdev_open;
    dev->stop = ath_netdev_stop;
    dev->hard_start_xmit = ath_netdev_hardstart;
    dev->set_mac_address = ath_netdev_set_macaddr;
    dev->tx_timeout = ath_netdev_tx_timeout;
    dev->set_multicast_list = ath_netdev_set_mcast_list;
    dev->do_ioctl = ath_ioctl;
    dev->get_stats = ath_getstats;
    dev->change_mtu = ath_change_mtu;
#endif
    dev->watchdog_timeo = 5 * HZ;           /* XXX */
    dev->tx_queue_len = ATH_TXBUF-1;        /* 1 for mgmt frame */
    dev->hard_header_len += sizeof (struct ieee80211_qosframe) + sizeof(struct llc) + IEEE80211_ADDR_LEN + IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN;

#if UMAC_SUPPORT_WEXT
    /*
    ** Attach the iwpriv handlers
    */

    ath_iw_attach(dev);
#endif

    /*
     * setup interrupt serivce routine
     */

    ATH_INIT_TQUEUE(&osdev->intr_tq, (qdf_defer_fn_t)ath_tasklet, (void*)dev);

#if OS_SUPPORT_ASYNC_Q
   OS_MESGQ_INIT(osdev, &osdev->async_q, sizeof(os_async_q_mesg),
        OS_ASYNC_Q_MAX_MESGS,os_async_mesg_handler,  osdev,MESGQ_PRIORITY_NORMAL,MESGQ_ASYNCHRONOUS_EVENT_DELIVERY);
#endif


#ifdef QCA_PARTNER_PLATFORM
    error = osif_pltfrm_interupt_register(dev, DIRECT_TYPE, ath_isr, NULL);
#else
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
    error = request_irq(dev->irq, ath_isr, SA_SHIRQ | SA_SAMPLE_RANDOM, dev->name, dev);
#else
    if (bus_context->bc_bustype == HAL_BUS_TYPE_AHB) {
        error = request_irq(dev->irq, ath_isr, IRQF_DISABLED, dev->name, dev);
    } else {
        error = request_irq(dev->irq, ath_isr, IRQF_SHARED, dev->name, dev);
    }
#endif

#endif
    if (error)
    {
        printk(KERN_WARNING "%s: request_irq failed\n", dev->name);
        error = -EIO;
        goto bad4;
    }

#if PCI_INTERRUPT_WAR_ENABLE
    {
        printk(KERN_INFO "%s : Interrupt WA timer is enabled\n", __func__);
        scn->int_status_poll_interval = 200; //ms
        OS_INIT_TIMER(scn->sc_osdev, &scn->int_status_poll_timer, ath_int_status_poll_timer, dev);
        OS_SET_TIMER(&scn->int_status_poll_timer, 1000); /* start polling after 1 sec */
    }
#endif
    /* Kernel 2.6.25 needs valid dev_addr before  register_netdev */
    IEEE80211_ADDR_COPY(dev->dev_addr,ic->ic_myaddr);

    /* do cfg80211 attach */

#if UMAC_SUPPORT_CFG80211
    if ( ic->ic_cfg80211_config) {
        ic->ic_cfg80211_radio_attach(osdev->device, dev, ic);
    }
#endif


    /*
     * finally register netdev and ready to go
     */
    if ((error = register_netdev(dev)) != 0) {
        printk(KERN_ERR "%s: unable to register device\n", dev->name);
        goto bad5;
    }
#if ATH_SSID_STEERING
    if(EOK != ath_ssid_steering_netlink_init()) {
        printk("SSID steering socket init failed __investigate__\n");
        goto bad4;
    }
#endif


#if !NO_SIMPLE_CONFIG
    /* Request Simple Config intr handler */
    register_simple_config_callback (dev->name, (void *) jumpstart_intr,
                                     (void *) dev,
                                     (void *)&osdev->sc_push_button_dur);
#endif
    if (qdf_vfs_set_file_attributes((struct qdf_dev_obj *)&dev->dev.kobj,
                                    (struct qdf_vfs_attr *)&wifi_attr_group)) {
        qdf_warn("Failed to create sysfs entry");
    }

#ifdef ATH_SUPPORT_LINUX_STA
#ifdef CONFIG_SYSCTL
    ath_dynamic_sysctl_register(ATH_DEV_TO_SC(scn->sc_dev));
#endif
#endif

#if ATH_SUPPORT_FLOWMAC_MODULE
    ath_flowmac_attach(scn->sc_dev);
#endif

    /*
     * Register Rx callback with mgmt_txrx layer for receiving
     * mgmt frames.
     */
    rx_cb_info.frm_type = MGMT_FRAME_TYPE_ALL;
    rx_cb_info.mgmt_rx_cb = ieee80211_mgmt_input;
    wlan_mgmt_txrx_register_rx_cb(psoc, WLAN_UMAC_COMP_MLME,
                                       &rx_cb_info, 1);

    return 0;

bad5:
#ifdef QCA_PARTNER_PLATFORM
    osif_pltfrm_interupt_unregister(dev,0);
#else
    free_irq(dev->irq, dev);
#endif
bad4:
    scn->sc_ops->rx_cleanup(scn->sc_dev);
bad3:
    scn->sc_ops->tx_cleanup(scn->sc_dev);
bad2:

    osif_detach(dev);

#if UMAC_SUPPORT_ACFG
    acfg_event_netlink_delete();
#endif

    ald_destroy_netlink();

#if ATH_RXBUF_RECYCLE
    ath_rxbuf_recycle_destroy(osdev);
#endif /* ATH_RXBUF_RECYCLE */

    ath_adhoc_netlink_delete();

#if ATH_SSID_STEERING
    ath_ssid_steering_netlink_delete();
#endif

#if ATH_PARAMETER_API
    ath_papi_netlink_delete();
#endif

    ath_detach(scn);

bad_objmgr4:
    dadp_pdev_detach(scn->sc_pdev);

    if (osdev_priv) {
        qdf_mem_free(osdev_priv);
    }
    if (pdev) {
        dispatcher_pdev_close(pdev);
        wlan_objmgr_pdev_obj_delete(pdev);
    }
bad_objmgr3:
    dispatcher_psoc_close(psoc);
bad_objmgr2:
    wlan_global_lmac_if_close(psoc);
bad_objmgr1:
    wlan_objmgr_psoc_obj_delete(psoc);
bad1:
    if(scn->qdf_dev) {
        OS_FREE(scn->qdf_dev);
    }
bad:
    global_scn[--num_global_scn] = NULL;
bad_soc_free:
    __soc_detach(scn);
bad0:
    return error;
}

int
__ath_detach(struct net_device *dev)
{
    struct ath_softc_net80211 *scn = ath_netdev_priv(dev);
    int ret, i;
    struct wlan_objmgr_psoc *psoc = NULL;
#if UMAC_SUPPORT_CFG80211
    struct ieee80211com *ic = &scn->sc_ic;
#endif
    struct mgmt_txrx_mgmt_frame_cb_info rx_cb_info;
    struct wlan_lmac_if_dfs_rx_ops *dfs_rx_ops;

    psoc = wlan_pdev_get_psoc(scn->sc_pdev);

    /* == Fixed crash removing umac.ko while downloading target firmware */
    if (!scn->sc_dev) {
        return -EINVAL;
    }
    __soc_detach(scn);

#if UMAC_SUPPORT_CFG80211
	if(ic->ic_cfg80211_config) {
		ic->ic_cfg80211_radio_detach(ic);
	}
#endif
    rtnl_lock();
#ifdef CONFIG_COMCERTO_CUSTOM_SKB_LAYOUT
    device_remove_file(&dev->dev, &dev_attr_custom_skb_enable);
#endif
    ath_vap_delete_on_rmmod(scn);

    osif_detach(dev);

    if (dev->irq)
    {
#ifdef QCA_PARTNER_PLATFORM
        osif_pltfrm_interupt_unregister(dev,0);
#else
        free_irq(dev->irq, dev);
#endif
    }

#ifdef ATH_SUPPORT_LINUX_STA
#ifdef CONFIG_SYSCTL
    ath_dynamic_sysctl_unregister(ATH_DEV_TO_SC(scn->sc_dev));
#endif
#endif

#ifndef NO_SIMPLE_CONFIG
    unregister_simple_config_callback(dev->name);
#endif
    sysfs_remove_group(&dev->dev.kobj, &wifi_attr_group);
    unregister_netdevice(dev);

#if OS_SUPPORT_ASYNC_Q
   OS_MESGQ_DRAIN(&scn->sc_osdev->async_q,NULL);
   OS_MESGQ_DESTROY(&scn->sc_osdev->async_q);
#endif
    scn->sc_ops->rx_cleanup(scn->sc_dev);
    scn->sc_ops->tx_cleanup(scn->sc_dev);
#if PCI_INTERRUPT_WAR_ENABLE
    OS_CANCEL_TIMER(&scn->int_status_poll_timer);
    OS_FREE_TIMER(&scn->int_status_poll_timer);
#endif


    ath_adhoc_netlink_delete();

#if ATH_SSID_STEERING
    ath_ssid_steering_netlink_delete();
#endif

#if ATH_PARAMETER_API
    ath_papi_netlink_delete();
#endif

#if UMAC_SUPPORT_ACFG
    acfg_event_netlink_delete();
#endif

#if ATH_RXBUF_RECYCLE
    ath_rxbuf_recycle_destroy(scn->sc_osdev);
#endif /* ATH_RXBUF_RECYCLE */

    ald_destroy_netlink();

#if ATH_SUPPORT_FLOWMAC_MODULE
    ath_flowmac_detach(scn->sc_dev);
#endif

    /* Dereigster MgMt TxRx handler */
    rx_cb_info.frm_type = MGMT_FRAME_TYPE_ALL;
    rx_cb_info.mgmt_rx_cb = ieee80211_mgmt_input;
    wlan_mgmt_txrx_deregister_rx_cb(psoc, WLAN_UMAC_COMP_MLME,
                                       &rx_cb_info, 1);
#ifndef REMOVE_PKT_LOG
    if(enable_pktlog_support) {
        osif_hal_log_ani_callback_register(NULL);
    }
#endif

    ret = ath_detach(scn);

    if (scn->sc_pdev->pdev_nif.pdev_ospriv) {
        qdf_mem_free(scn->sc_pdev->pdev_nif.pdev_ospriv);
        scn->sc_pdev->pdev_nif.pdev_ospriv = NULL;
    }

    dfs_rx_ops = wlan_lmac_if_get_dfs_rx_ops(psoc);
    if (dfs_rx_ops && dfs_rx_ops->dfs_stop) {
        if (wlan_objmgr_pdev_try_get_ref(scn->sc_pdev, WLAN_DFS_ID) !=
                QDF_STATUS_SUCCESS) {
            return -EINVAL;
        }
        dfs_rx_ops->dfs_stop(scn->sc_pdev);
        wlan_objmgr_pdev_release_ref(scn->sc_pdev, WLAN_DFS_ID);
    }

    dispatcher_pdev_close(scn->sc_pdev);
    dadp_pdev_detach(scn->sc_pdev);

    /* Delete PDEV object */
    wlan_objmgr_pdev_obj_delete(scn->sc_pdev);

    dispatcher_psoc_close(psoc);
    wlan_global_lmac_if_close(psoc);

    wlan_objmgr_print_ref_all_objects_per_psoc(psoc);
   /* UMAC Dispatcher PSOC delete */
    wlan_objmgr_psoc_obj_delete(psoc);

    if(scn->qdf_dev) {
        OS_FREE(scn->qdf_dev);
        scn->qdf_dev = NULL;
    }

    for (i = 0; i < num_global_scn; i++) {
        if(global_scn[i] == scn){
            global_scn[i] = NULL;
            break;
        }
    }
    num_global_scn--;

    rtnl_unlock();
    return ret;
}

int
__ath_remove_interface(struct net_device *dev)
{
    struct ath_softc_net80211 *scn = ath_netdev_priv(dev);
    struct ieee80211com *ic = &scn->sc_ic;
    struct ieee80211vap *vap;
    struct ieee80211vap *vapnext;
    osif_dev  *osifp;
    struct net_device *netdev;

    vap = TAILQ_FIRST(&ic->ic_vaps);
    while (vap != NULL) {
        /* osif_ioctl_delete_vap() destroy vap->iv_next information,
        so need to store next VAP address in vapnext */
        vapnext = TAILQ_NEXT(vap, iv_next);
        osifp = (osif_dev *)vap->iv_ifp;
        netdev = osifp->netdev;
        printk("Remove interface on %s\n",netdev->name);
        rtnl_lock();
        dev_close(netdev);
        osif_ioctl_delete_vap(netdev);
        rtnl_unlock();
        vap = vapnext;
    }
    return 0;
}

void
__ath_set_delete(struct net_device *dev)
{
}

int
__ath_suspend(struct net_device *dev)
{
    return ath_netdev_stop(dev);
}

int
__ath_resume(struct net_device *dev)
{
    return ath_netdev_open(dev);
}
#if !NO_SIMPLE_CONFIG
/*
 * Handler for front panel SW jumpstart switch
 */
void *ath_vdev_jumpstart_intr(struct wlan_objmgr_pdev *pdev, void *obj, void *args)
{
    osif_dev  *osifp;
    struct wlan_objmgr_vdev *vdev = (struct wlan_objmgr_vdev *)obj;
    uint32_t push_time = *((uint32_t *)args);
    u_int32_t push_duration;
    wlan_if_t vap;

    if(vdev != NULL) {
        vdev_osifp = wlan_vdev_get_ospriv(vdev);
        if (!vdev_osifp) {
            return;
        }
        osifp = (osif_dev *)vdev_osifp->legacy_osif_priv
        if(osifp == NULL) {
            return;
        }
        if (push_time) {
            push_duration = push_time;
        } else {
            push_duration = 0;
        }
        vap = wlan_vdev_get_mlme_ext_obj(vdev);
        if (vap == NULL) {
           return;
        }
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
        if (!ieee80211_vap_nopbn_is_set(vap))
        {
#endif
           /* Since we are having single physical push button on device,
              and for push button + muti bss combination mode we will be
              have limitations, we designed that,physical push button
              notification will be sent to first AP vap(main BSS) and all
              sta vaps.
            */
           if ((wlan_vdev_get_opmode(vdev) != WLAN_OPMODE_AP) ||
                     pdev->pdev_nif.notified_ap_vdev == 0 ) {
               qdf_print("SC Pushbutton Notify on %s for %d sec(s)"
                      "and the vap %pK dev %pK:",dev->name, push_duration, vap,
                      (struct net_device *)(osifp)->netdev);
               osif_notify_push_button (osifp, push_duration);
               if (wlan_vdev_get_opmode(vdev) == WLAN_OPMODE_AP )
                   pdev->pdev_nif.notified_ap_vdev = 1;
               }
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
        }
#endif
    }
}

static irqreturn_t
jumpstart_intr (int cpl, void *dev_id, struct pt_regs *regs, void *push_time)
{
    struct net_device *dev = dev_id;
    struct ath_softc_net80211 *scn = ath_netdev_priv(dev);
    struct ieee80211com *ic = &scn->sc_ic;
    struct ieee80211vap *vap;
    u_int32_t           push_duration;
    int is_ap_vap_notified = 0;
    /*
    ** Iterate through all VAPs, since any of them may have WPS enabled
    */
    scn->sc_pdev->pdev_nif.notified_ap_vdev = 0;
    if (wlan_objmgr_pdev_try_get_ref(scn->sc_pdev, WLAN_MLME_NB_ID) ==
                                      QDF_STATUS_SUCCESS) {
         wlan_objmgr_pdev_iterate_obj_list(scn->sc_pdev, WLAN_VDEV_OP,
                                           ath_vdev_jumpstart_intr, push_time,
                                           1, WLAN_MLME_NB_ID);
         wlan_objmgr_pdev_release_ref(scn->sc_pdev, WLAN_MLME_NB_ID);
    }
    return IRQ_HANDLED;
}
#endif

#if OS_SUPPORT_ASYNC_Q
static void os_async_mesg_handler( void  *ctx, u_int16_t  mesg_type, u_int16_t  mesg_len, void  *mesg )
{
    if (mesg_type == OS_SCHEDULE_ROUTING_MESG_TYPE) {
        os_schedule_routing_mesg  *s_mesg = (os_schedule_routing_mesg *) mesg;
        s_mesg->routine(s_mesg->context, NULL);
    }
}
#endif

void
init_wlan(void)
{

}

#ifdef ATH_USB

#define IS_UP(_dev) \
    (((_dev)->flags & (IFF_RUNNING|IFF_UP)) == (IFF_RUNNING|IFF_UP))

static int ath_reinit_interface(struct net_device *dev)
{
    struct ath_softc_net80211 *scn = ath_netdev_priv(dev);
    struct ieee80211com *ic = &scn->sc_ic;
    struct ieee80211vap *vap;
    struct ieee80211vap *vapnext;
    osif_dev  *osifp;
    struct net_device *netdev;

    vap = TAILQ_FIRST(&ic->ic_vaps);
    while (vap != NULL) {
        vapnext = TAILQ_NEXT(vap, iv_next);
        osifp = (osif_dev *)vap->iv_ifp;
        netdev = osifp->netdev;
        rtnl_lock();
        if (IS_UP(netdev)) {
            dev_close(netdev);
            msleep(50);
            dev_open(netdev);
        }
        rtnl_unlock();
        vap = vapnext;
    }

    return 0;
}

void ath_usb_vap_restart(struct net_device *dev)
{
    struct ath_softc_net80211 *scn = ath_netdev_priv(dev);

    cmpxchg((int*)(&(scn->sc_dev_enabled)), 1, 0);
    ath_netdev_open(dev);
    ath_reinit_interface(dev);
}

EXPORT_SYMBOL(__ath_detach);
EXPORT_SYMBOL(init_wlan);
EXPORT_SYMBOL(__ath_remove_interface);
EXPORT_SYMBOL(__ath_attach);
EXPORT_SYMBOL(ath_usb_vap_restart);
EXPORT_SYMBOL(__ath_set_delete);
#endif

